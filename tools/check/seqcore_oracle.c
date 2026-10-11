/* Drives the port's own sequencer, src/dc/seq/seqcore.c, over
 * one sequence out of a real bank and prints what it asks of the
 * synthesizer -- "on" lines on stdout and, with BGM_ORACLE_TRACE=<file>,
 * a trace of every call -- so tools/check/bgm_check.py can hold it to the
 * decomp's tick timeline.
 *
 *   seqcore_oracle <bgm.pak> <sequence index> <microseconds>
 *
 * SEQ_ORACLE_JITTER=1 advances in uneven 1-7 ms steps (the firmware's
 * loop) instead of 16 ms; SEQ_ORACLE_CLOCK=1 prints the player's clocks
 * at the end, to stderr.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bgmbank.h"
#include "seq/seqcore.h"
#include "seq/seqprep.h"

static FILE *gTrace;
static SeqPlayer sPlayer;

static double ratio(uint32_t r)
{
    return (double)r / (double)SEQ_RATIO_ONE;
}

static void t_start(void *ctx, int vi, int wave, uint32_t r, int vol,
                    int pan, int fxmix, int wet, int dry, int32_t ramp)
{
    SeqPlayer *p = ctx;
    const SeqVoice *v = &p->voice[vi];

    (void)wet; (void)dry;
    printf("on %d %u %u %u\n", (int)p->cur_time, v->chan, v->key, v->vel);
    if (gTrace)
        fprintf(gTrace, "start %d %d %d %.9g %d %d %d %d\n",
                (int)p->cur_time, vi, wave, ratio(r), vol, pan, fxmix,
                (int)ramp);
}

static void t_vol(void *ctx, int vi, int vol, int32_t ramp)
{
    SeqPlayer *p = ctx;

    /* the release sets the phase before it asks for silence */
    if (vol == 0 && p->voice[vi].phase == 2 /* PH_RELEASE */)
        printf("off %d %u %u\n", (int)p->cur_time, p->voice[vi].chan,
               p->voice[vi].key);
    if (gTrace)
        fprintf(gTrace, "vol %d %d %d %d\n", (int)p->cur_time, vi, vol,
                (int)ramp);
}

static void t_pitch(void *ctx, int vi, uint32_t r)
{
    if (gTrace)
        fprintf(gTrace, "pitch %d %d %.9g\n",
                (int)((SeqPlayer *)ctx)->cur_time, vi, ratio(r));
}

static void t_pan(void *ctx, int vi, int pan)
{
    if (gTrace)
        fprintf(gTrace, "pan %d %d %d\n", (int)((SeqPlayer *)ctx)->cur_time,
                vi, pan);
}

static void t_fxmix(void *ctx, int vi, int fx)
{
    if (gTrace)
        fprintf(gTrace, "fx %d %d %d 0\n",
                (int)((SeqPlayer *)ctx)->cur_time, vi, fx);
}

static void t_wetdry(void *ctx, int vi, int wet, int dry)
{
    if (gTrace)
        fprintf(gTrace, "fx %d %d %d %d\n",
                (int)((SeqPlayer *)ctx)->cur_time, vi, wet, dry);
}

static void t_stop(void *ctx, int vi)
{
    if (gTrace)
        fprintf(gTrace, "stop %d %d\n", (int)((SeqPlayer *)ctx)->cur_time,
                vi);
}

static const SeqSynth kTrace = {
    t_start, t_vol, t_pitch, t_pan, t_fxmix, t_wetdry, t_stop,
};

int main(int argc, char **argv)
{
    static int32_t values[65536];
    const BGMSeqEntry *e;
    SeqBank bank;
    SeqOsc *osc;
    uint8_t *buf;
    int32_t idx, limit;
    int i;

    if (argc != 4)
        return fprintf(stderr, "usage: %s <bgm.pak> <seq> <us>\n", argv[0]), 2;
    idx = (int32_t)strtol(argv[2], NULL, 10);
    limit = (int32_t)strtol(argv[3], NULL, 10);
    if (bgm_bank_load(&gBGMBank, argv[1]) < 0)
        return fprintf(stderr, "%s: not a bank\n", argv[1]), 1;
    if (idx < 0 || (uint32_t)idx >= gBGMBank.n_seqs)
        return fprintf(stderr, "no sequence %d\n", (int)idx), 1;
    if (getenv("BGM_ORACLE_TRACE"))
    {
        gTrace = fopen(getenv("BGM_ORACLE_TRACE"), "w");
        if (!gTrace)
            return perror(getenv("BGM_ORACLE_TRACE")), 1;
    }

    osc = calloc(2 * gBGMBank.n_insts, sizeof(*osc));
    if (!osc || seq_prep_osc(gBGMBank.insts, gBGMBank.n_insts, osc, values,
                             sizeof(values) / sizeof(values[0])) < 0)
        return fprintf(stderr, "oscillator tables do not fit\n"), 1;
    bank.insts = gBGMBank.insts;
    bank.sounds = gBGMBank.sounds;
    bank.osc = osc;
    bank.n_insts = gBGMBank.n_insts;
    bank.percussion = gBGMBank.percussion;

    e = &gBGMBank.seqs[idx];
    buf = malloc(e->size);
    if (!buf)
        return 1;
    memcpy(buf, gBGMBank.seqdata + e->off, e->size);

    /* what src/dc/bgm.c queues before the first tick */
    seq_init(&sPlayer, &bank, &kTrace, &sPlayer);
    seq_set_sequence(&sPlayer, buf);
    seq_play(&sPlayer);
    for (i = 0; i < SEQ_CHANNELS; i++)
        seq_set_chan_priority(&sPlayer, i, 20);

    /* SEQ_ORACLE_JITTER=1: advance in uneven 1-7 ms steps, the way the
     * AICA firmware's main loop does, instead of the N64 frame */
    while (sPlayer.cur_time < limit)
    {
        static uint32_t lcg = 12345;

        if (getenv("SEQ_ORACLE_JITTER"))
        {
            lcg = lcg * 1103515245u + 12345u;
            seq_advance(&sPlayer, 1000 + (int32_t)((lcg >> 16) % 6000));
        }
        else
            seq_advance(&sPlayer, 16000);
        if (sPlayer.state == SEQ_STOPPED && sPlayer.have_seq)
            break;
    }
    if (getenv("SEQ_ORACLE_CLOCK"))
        fprintf(stderr, "now %ld cur %ld call_at %ld\n", (long)sPlayer.now,
                (long)sPlayer.cur_time, (long)sPlayer.call_at);
    if (gTrace)
        fclose(gTrace);
    free(buf);
    free(osc);
    bgm_bank_free(&gBGMBank);
    return 0;
}
