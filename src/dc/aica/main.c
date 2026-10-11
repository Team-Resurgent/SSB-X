/* The port's AICA firmware: the ARM7's main loop.
 *
 A drop-in for KallistiOS's stream.drv that also plays the music. It
 * serves the same SH-4 -> AICA command queue (dc/sound/aica_comm.h, at the
 * addresses aica_cmd_iface.h fixes), so src/dc/fgm.c drives its channels
 * exactly as it does on KOS's driver; the channel register work is KOS's
 * own aica.c and the vectors its crt0.s, both compiled from the image's
 * KallistiOS tree at build time (src/game/ssb64/Makefile). And it
 * runs the port's sequencer (src/dc/seq/seqcore.c) on channels 0..23,
 * driven by DC_AICA_CMD_SEQ packets from src/dc/bgmarm.c (shared.h).
 *
 * Memory: code, data and the stack (crt0.s puts sp at 0xB000) live below
 * AICA_MEM_CMD_QUEUE; the SH-4 owns everything from AICA_RAM_START up.
 */
#include <stddef.h>

#include "aica_cmd_iface.h"
#include "aica.h"

#include "shared.h"
#include "seqsyn.h"
#include "../seq/seqcore.h"

/* crt0.s's timer interrupt bumps this 4410 times a second. */
#define CLOCK (*(volatile uint32 *)AICA_MEM_CLOCK)

/* What the SH-4 can see of the ARM, which has no serial (shared.h). */
static volatile DCAicaStatus *const status =
    (volatile DCAicaStatus *)DC_AICA_STATUS_AT;

static SeqPlayer player;
static int have_bank;

/* The AICA's DSP runs whatever program the boot ROM left in it (its
 * startup sound's reverb), and its ring buffer starts at the bottom of
 * sound RAM -- right over this program's data. Clearing the program
 * makes every step a no-op that writes no memory.
 * (The game's reverb, its ring placed by the SH-4, would replace it.) */
static void dsp_off(void)
{
    volatile uint32 *mpro = (volatile uint32 *)0x00803400;
    int i;

    for (i = 0; i < 128 * 4; i++)
        mpro[i] = 0;
}

static volatile aica_queue_t *const q_cmd =
    (volatile aica_queue_t *)AICA_MEM_CMD_QUEUE;
static volatile aica_queue_t *const q_resp =
    (volatile aica_queue_t *)AICA_MEM_RESP_QUEUE;
/* The channel mirror aica.c reads its parameters from (it declares this
 * extern, by this name); the SH-4 reads it back for positions. */
volatile aica_channel_t *chans = (volatile aica_channel_t *)AICA_MEM_CHANNELS;

/* GCC emits calls to these for struct copies even with -ffreestanding. */
void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    while (n--)
        *d++ = *s++;
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = dst;

    while (n--)
        *d++ = (unsigned char)c;
    return dst;
}

static void queue_init(volatile aica_queue_t *q, uint32 base, uint32 end)
{
    q->head = 0;
    q->tail = 0;
    q->data = base + sizeof(aica_queue_t);
    q->size = end - q->data;
    q->process_ok = 1;
    q->valid = 1;
}

static void channel_cmd(uint32 ch, const aica_channel_t *c)
{
    uint32 op = c->cmd & AICA_CH_CMD_MASK;

    if (op == AICA_CH_CMD_START && (c->cmd & AICA_CH_START_SYNC))
    {
        /* cmd_id is a channel bitmap here, not a channel */
        aica_sync_play(ch);
        return;
    }
    if (ch >= 64)
        return;
    if (op == AICA_CH_CMD_START)
    {
        memcpy((void *)&chans[ch], c, sizeof(*c));
        chans[ch].pos = 0;
        aica_play(ch, c->cmd & AICA_CH_START_DELAY);
    }
    else if (op == AICA_CH_CMD_STOP)
    {
        aica_stop(ch);
    }
    else if (op == AICA_CH_CMD_UPDATE)
    {
        if (c->cmd & AICA_CH_UPDATE_SET_FREQ)
        {
            chans[ch].freq = c->freq;
            aica_freq(ch);
        }
        if (c->cmd & AICA_CH_UPDATE_SET_VOL)
        {
            chans[ch].vol = c->vol;
            aica_vol(ch);
        }
        if (c->cmd & AICA_CH_UPDATE_SET_PAN)
        {
            chans[ch].pan = c->pan;
            aica_pan(ch);
        }
    }
}

/* A DC_AICA_CMD_SEQ packet. Each but KILL is posted to the player and
 * runs in its next handler call, as the N64's API calls do. */
static void seq_cmd(uint32 what, const uint32 *arg)
{
    switch (what)
    {
    case DC_SEQ_BANK:
    {
        const DCSeqImage *img = (const DCSeqImage *)arg[0];

        seqsyn_bank(img);
        seq_init(&player, (const SeqBank *)img->bank, &gSeqSynAica, 0);
        have_bank = 1;
        break;
    }
    case DC_SEQ_SETSEQ:
        if (have_bank)
            seq_set_sequence(&player, (uint8_t *)arg[0]);
        break;
    case DC_SEQ_PLAY:
        if (have_bank)
            seq_play(&player);
        break;
    case DC_SEQ_STOP:
        if (have_bank)
            seq_stop(&player);
        break;
    case DC_SEQ_VOLUME:
        if (have_bank)
            seq_set_volume(&player, (int16_t)arg[0]);
        break;
    case DC_SEQ_PRIORITY:
        if (have_bank)
            seq_set_chan_priority(&player, (int)arg[0], (int)arg[1]);
        break;
    case DC_SEQ_FXSEND:
        /* the sound effects' (fgm.c), not the music's: bgmarm.c counts
         * only its own commands against seq_cmds */
        seqsyn_fx_send((int)arg[0], (int)(arg[1] & 0xFF),
                       (int)((arg[1] >> 8) & 0xFF));
        return;
    case DC_SEQ_KILL:
        seqsyn_silence();
        if (have_bank)
            seq_init(&player, player.bank, &gSeqSynAica, 0);
        break;
    default:
        break;
    }
    status->seq_cmds++;
}

/* One packet starting `tail` bytes into the queue, which may wrap. Returns
 * its length in words. */
static uint32 run_packet(uint32 tail)
{
    uint32 buf[AICA_CMD_MAX_SIZE];
    const aica_cmd_t *pkt = (const aica_cmd_t *)buf;
    uint32 base = q_cmd->data, size = q_cmd->size;
    uint32 n = *(volatile uint32 *)(base + tail), i;

    if (n == 0)
        n = 1;                  /* never spin on a zero-length packet */
    if (n > AICA_CMD_MAX_SIZE)
        n = AICA_CMD_MAX_SIZE;
    for (i = 0; i < n; i++)
    {
        buf[i] = *(volatile uint32 *)(base + tail);
        tail += 4;
        if (tail >= size)
            tail = 0;
    }

    status->packets++;
    if (pkt->cmd == AICA_CMD_CHAN)
        channel_cmd(pkt->cmd_id, (const aica_channel_t *)pkt->cmd_data);
    else if (pkt->cmd == AICA_CMD_SYNC_CLOCK)
        CLOCK = 0;
    else if (pkt->cmd == DC_AICA_CMD_SEQ)
        seq_cmd(pkt->cmd_id, pkt->misc);
    return n;
}

static void drain_queue(void)
{
    uint32 head = q_cmd->head, tail = q_cmd->tail, size = q_cmd->size;

    while (tail != head)
    {
        uint32 at = tail + offsetof(aica_cmd_t, timestamp);
        uint32 when;

        if (at >= size)
            at -= size;
        when = *(volatile uint32 *)(q_cmd->data + at);
        if (when && when >= CLOCK)
            return;             /* scheduled for later */
        tail += run_packet(tail) * 4;
        if (tail >= size)
            tail -= size;
        q_cmd->tail = tail;
    }
}

/* The 4410 Hz clock in microseconds, the remainder carried. */
static uint32 last_clock, us_rem;

static int32_t elapsed_us(void)
{
    uint32 now = CLOCK, ticks;

    if (now < last_clock)       /* AICA_CMD_SYNC_CLOCK reset it */
        last_clock = now;
    ticks = now - last_clock;
    last_clock = now;
    if (ticks > status->clk_max_step)
        status->clk_max_step = ticks;
    if (ticks > 1000)
    {
        status->clk_big_steps++;
        status->big_now = now;
        status->big_last = now - ticks;
    }
    us_rem += ticks * 1000000u;
    ticks = us_rem / 4410u;
    us_rem -= ticks * 4410u;
    return (int32_t)ticks;
}

int arm_main(void)
{
    uint32 ch, next;

    dsp_off();
    queue_init(q_cmd, AICA_MEM_CMD_QUEUE, AICA_MEM_RESP_QUEUE);
    queue_init(q_resp, AICA_MEM_RESP_QUEUE, AICA_MEM_CHANNELS);
    aica_init();
    status->passes = 0;
    status->packets = 0;
    status->seq_cmds = 0;
    status->seq_state = SEQ_STOPPED;
    status->seq_time = 0;
    status->seq_missed = 0;
    status->seq_missed_wave = 0;
    status->trace_n = 0;
    status->min_free_voices = 99;
    status->max_queue = 0;
    status->fx_sends = 0;
    status->clk_max_step = 0;
    status->clk_big_steps = 0;
    seqsyn_init(status);
    last_clock = CLOCK;
    status->magic = DC_AICA_MAGIC;

    for (;;)
    {
        status->passes++;
        for (ch = 0; ch < 64; ch++)
            aica_get_pos(ch);
        if (q_cmd->process_ok)
            drain_queue();
        {
            int32_t us = elapsed_us();

            if (have_bank && us > 0)
            {
                extern int32_t gSeqTraceTime;

                gSeqTraceTime = player.cur_time;
                seq_advance(&player, us);
                seqsyn_advance(us);
                {
                    uint32 fv = 0, ql = 0;
                    int k;

                    for (k = player.v_free; k >= 0;
                         k = player.voice[k].next)
                        fv++;
                    for (k = player.ev_head; k >= 0; k = player.ev[k].next)
                        ql++;
                    if (fv < status->min_free_voices)
                        status->min_free_voices = fv;
                    if (ql > status->max_queue)
                        status->max_queue = ql;
                }
            }
            status->seq_state = player.state;
            status->seq_time = player.cur_time;
            status->seq_now = player.now;
            status->seq_call_at = player.call_at;
        }
        /* Let the SH-4 at the bus between passes: ~2.3 ms. */
        next = CLOCK + 10;
        while (CLOCK <= next)
            ;
    }
}
