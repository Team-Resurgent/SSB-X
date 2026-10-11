/* aica_fw_xbox.c -- the port's AICA firmware, on the x86.
 *
 * Two pieces of ARM7 code meet here, both re-expressed for a machine on
 * which sound RAM is not at address 0:
 *
 *   src/dc/aica/main.c -- the port's firmware loop: it serves KOS's SH-4 ->
 *     AICA command queue (channel commands from src/dc/fgm.c, sequencer
 *     commands from src/dc/bgmarm.c), clocks the music sequencer
 *     (src/dc/seq/seqcore.c, compiled once for both sides here) and its
 *     voices (src/dc/aica/seqsyn.c, compiled unchanged through
 *     aica_fw_seqsyn.c), and keeps the status block the SH-4 reads.
 *     Everything below follows it statement for statement; what differs is
 *     only that its fixed AICA addresses (the queues, the channel mirror,
 *     the clock word, the status block) go through aica_host(), and that
 *     the loop is not a loop: the mixer calls one pass every 100 output
 *     samples, which is the 10 ticks of the 4410 Hz clock the firmware's
 *     own busy-wait spaces its passes by.
 *
 *   KallistiOS's ARM aica.c (kernel/arch/dreamcast/sound/arm/aica.c,
 *     (c)2000-2002 Megan Potter, (c)2024 Stefanos Kornilios Mitsis Poiitidis,
 *     (c)2026 Ruslan Rostovtsev, KOS licence) -- the channel routines, which
 *     write the channel's registers exactly as KOS does. Key on and key off,
 *     which on the chip take effect when a register write sets KYONEX, are
 *     handed to the mixer at that write. The one register value that cannot
 *     be kept as KOS writes it is the sample address: 23 bits of it are
 *     packed into two registers, and a host address does not fit, so the
 *     whole address is kept beside them (xbox_aica_chan_base). */
#include <stddef.h>
#include <string.h>

#include "../aica_cmd_iface.h"
#include "../aica.h"
#include "aica_xbox.h"

#include "../../../src/dc/aica/shared.h"
#include "../../../src/dc/aica/seqsyn.h"
#include "../../../src/dc/seq/seqcore.h"

int aica_fw_running;
uint32_t xbox_aica_chan_base[64];

#define CLOCK (*(volatile uint32 *)aica_host(AICA_MEM_CLOCK))

static volatile DCAicaStatus *status;
static SeqPlayer player;
static int have_bank;
static volatile aica_queue_t *q_cmd;
static volatile aica_queue_t *q_resp;
volatile aica_channel_t *chans;     /* aica.c and seqsyn.c read it by this name */

/* ===================================================== KOS's aica.c */

void aica_init(void)
{
    int i, j;

    /* the interrupt registers the ARM's FIQ timer uses: there is no FIQ,
     * the mixer is the clock, but the values are kept */
    SNDREG32(0x289C) = 0;
    SNDREG32(0x28B4) = 0;
    SNDREG32(0x28A4) = 0x7FF;
    SNDREG32(0x28BC) = 0x7FF;
    SNDREG32(0x28A8) = 0x18;
    SNDREG32(0x28AC) = 0x50;
    SNDREG32(0x28B0) = 0x08;
    SNDREG32(0x2800) = 0x0000;
    for (i = 0; i < 64; i++)
    {
        aica_mixer_key_off(i);
        CHNREG32(i, 0) = 0x8000;
        for (j = 4; j < 0x80; j += 4)
            CHNREG32(i, j) = 0;
        CHNREG32(i, 20) = 0x1f;
        xbox_aica_chan_base[i] = 0;
    }
    SNDREG32(0x2800) = 0x000f;
    SNDREG32(0x2890) = 256 - (44100 / 4410);
    SNDREG32(0x289C) = 0x40;
}

/* 16 * log2(255 / i), as KOS tabulates it */
static const uint8 logs[] = {
    255, 127, 111, 102, 95, 90, 86, 82, 79, 77, 74, 72, 70, 68, 66, 65,
    63, 62, 61, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 50, 49, 48,
    47, 47, 46, 45, 45, 44, 43, 43, 42, 42, 41, 41, 40, 40, 39, 39,
    38, 38, 37, 37, 36, 36, 35, 35, 34, 34, 34, 33, 33, 33, 32, 32,
    31, 31, 31, 30, 30, 30, 29, 29, 29, 28, 28, 28, 27, 27, 27, 27,
    26, 26, 26, 25, 25, 25, 25, 24, 24, 24, 24, 23, 23, 23, 23, 22,
    22, 22, 22, 21, 21, 21, 21, 20, 20, 20, 20, 20, 19, 19, 19, 19,
    18, 18, 18, 18, 18, 17, 17, 17, 17, 17, 17, 16, 16, 16, 16, 16,
    15, 15, 15, 15, 15, 15, 14, 14, 14, 14, 14, 14, 13, 13, 13, 13,
    13, 13, 12, 12, 12, 12, 12, 12, 11, 11, 11, 11, 11, 11, 11, 10,
    10, 10, 10, 10, 10, 10, 9, 9, 9, 9, 9, 9, 9, 8, 8, 8,
    8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6,
    6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static inline uint8 calc_aica_vol(uint8 x)
{
    return logs[x];
}

static inline int calc_aica_pan(int x)
{
    if (x == 0x80)
        return 0;
    else if (x < 0x80)
        return 0x10 | ((0x7f - x) >> 3);
    else
        return (x - 0x80) >> 3;
}

static void freq_regs(int ch, uint32 freq)
{
    uint32 freq_lo, freq_base = 5644800;
    int freq_hi = 7;

    while (freq < freq_base && freq_hi > -8)
    {
        freq_base >>= 1;
        --freq_hi;
    }
    freq_lo = (freq << 10) / freq_base;
    CHNREG32(ch, 24) = (freq_hi << 11) | (freq_lo & 1023);
}

/* A KYONEX write: every channel whose KYONB is set and is not sounding
 * starts, every one whose KYONB is clear and is sounding stops -- the
 * chip's rule, which aica_play, aica_stop and aica_sync_play rely on. */
static void key_execute(void)
{
    int i;

    for (i = 0; i < 64; i++)
    {
        uint32 r = CHNREG32(i, 0);

        if (r & AICA_CHANNEL_KEYONB)
            aica_mixer_key_on(i);
        else
            aica_mixer_key_off(i);
    }
}

void aica_play(int ch, int delay)
{
    uint32 smpptr = chans[ch].base;
    uint32 mode = chans[ch].type;
    uint32 loopst = chans[ch].loopstart;
    uint32 loopend = chans[ch].loopend;
    uint32 freq = chans[ch].freq;
    uint32 vol = chans[ch].vol;
    uint32 pan = chans[ch].pan;
    uint32 loopflag = chans[ch].loop;
    uint32 playCont;

    aica_stop(ch);

    CHNREG32(ch, 8) = loopst & 0xffff;
    CHNREG32(ch, 12) = loopend & 0xffff;
    freq_regs(ch, freq);
    CHNREG8(ch, 36) = calc_aica_pan(pan);
    CHNREG8(ch, 37) = 0xf;
    CHNREG8(ch, 40) = 0x24;
    CHNREG8(ch, 41) = calc_aica_vol(vol);
    CHNREG32(ch, 16) = 0x1f;
    CHNREG32(ch, 4) = smpptr & 0xffff;
    playCont = (mode << 7) | ((smpptr >> 16) & 0x7f);
    if (loopflag)
        playCont |= 0x0200;
    xbox_aica_chan_base[ch] = smpptr;
    if (delay)
    {
        CHNREG32(ch, 0) = playCont;
    }
    else
    {
        CHNREG32(ch, 0) = 0xc000 | playCont;
        key_execute();
    }
}

void aica_sync_play(uint32 chmap)
{
    int i = 0;

    while (chmap)
    {
        if (chmap & 0x1)
            CHNREG32(i, 0) = CHNREG32(i, 0) | 0xc000;
        i++;
        chmap >>= 1;
    }
    key_execute();
}

void aica_stop(int ch)
{
    CHNREG32(ch, 0) = (CHNREG32(ch, 0) & ~0x4000) | 0x8000;
    key_execute();
}

void aica_vol(int ch)
{
    CHNREG8(ch, 41) = calc_aica_vol((uint8)chans[ch].vol);
}

void aica_pan(int ch)
{
    CHNREG8(ch, 36) = calc_aica_pan(chans[ch].pan);
}

void aica_freq(int ch)
{
    freq_regs(ch, chans[ch].freq);
}

int aica_get_pos(int ch)
{
    chans[ch].pos = aica_mixer_pos(ch) & 0xffff;
    return chans[ch].pos;
}

/* ================================================ the port's firmware */

static void dsp_off(void)
{
    volatile uint32 *mpro = (volatile uint32 *)(xbox_aica_regs + 0x3400);
    int i;

    for (i = 0; i < 128 * 4; i++)
        mpro[i] = 0;
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

static void seq_cmd(uint32 what, const uint32 *arg)
{
    switch (what)
    {
    case DC_SEQ_BANK:
    {
        const DCSeqImage *img = (const DCSeqImage *)aica_host(arg[0]);

        seqsyn_bank(img);
        seq_init(&player, (const SeqBank *)img->bank, &gSeqSynAica, 0);
        have_bank = 1;
        break;
    }
    case DC_SEQ_SETSEQ:
        if (have_bank)
            seq_set_sequence(&player, (uint8_t *)aica_host(arg[0]));
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
        seqsyn_fx_send((int)arg[0], (int)(arg[1] & 0xFF), (int)((arg[1] >> 8) & 0xFF));
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

static uint32 run_packet(uint32 tail)
{
    uint32 buf[AICA_CMD_MAX_SIZE];
    const aica_cmd_t *pkt = (const aica_cmd_t *)buf;
    uint32 base = q_cmd->data, size = q_cmd->size;
    uint32 n = *(volatile uint32 *)aica_host(base + tail), i;

    if (n == 0)
        n = 1;
    if (n > AICA_CMD_MAX_SIZE)
        n = AICA_CMD_MAX_SIZE;
    for (i = 0; i < n; i++)
    {
        buf[i] = *(volatile uint32 *)aica_host(base + tail);
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
        when = *(volatile uint32 *)aica_host(q_cmd->data + at);
        if (when && when >= CLOCK)
            return;
        tail += run_packet(tail) * 4;
        if (tail >= size)
            tail -= size;
        q_cmd->tail = tail;
    }
}

static uint32 last_clock, us_rem;

static int32_t elapsed_us(void)
{
    uint32 now = CLOCK, ticks;

    if (now < last_clock)
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

/* arm_main up to its loop. Run by spu_enable, before the mixer starts. */
void aica_fw_boot(void)
{
    status = (volatile DCAicaStatus *)aica_host(DC_AICA_STATUS_AT);
    q_cmd = (volatile aica_queue_t *)aica_host(AICA_MEM_CMD_QUEUE);
    q_resp = (volatile aica_queue_t *)aica_host(AICA_MEM_RESP_QUEUE);
    chans = (volatile aica_channel_t *)aica_host(AICA_MEM_CHANNELS);
    have_bank = 0;

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
    CLOCK = 0;
    last_clock = CLOCK;
    us_rem = 0;
    status->magic = DC_AICA_MAGIC;
}

/* One pass of arm_main's loop. The mixer advances CLOCK by 10 (100 samples
 * at 4410 Hz) before each call, which is what the firmware's busy-wait
 * between passes measures out on the Dreamcast. */
void aica_fw_pass(void)
{
    uint32 ch;

    if (!aica_fw_running)
        return;
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

                for (k = player.v_free; k >= 0; k = player.voice[k].next)
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
}

/* src/dc/dcaica.c copies "the firmware" into sound RAM before it lets the
 * ARM run. The firmware is this file, linked into the XBE; what goes into
 * the ARM's corner of sound RAM is a marker, so dc_aica_init's copy has
 * something real to copy and the corner says what runs there. */
const uint8_t dc_aica_fw[64] = "SSB64X AICA firmware: x86, Platform/xbox/aica/aica_fw_xbox.c";
__asm__(".globl _dc_aica_fw_end\n\t.set _dc_aica_fw_end, _dc_aica_fw + 64\n");
