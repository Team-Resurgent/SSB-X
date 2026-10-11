/* bgmarm.c -- see bgmarm.h. */
#include "bgmarm.h"

#include <kos.h>
#include <dc/spu.h>
#include <dc/sound/sound.h>
#include <dc/g2bus.h>
#include <stdlib.h>
#include <string.h>

#include "dcaica.h"
#include "aica_reverb.h"           /* tools/export/ssb_reverbexport.py */
#include "seq/seqcore.h"
#include "seq/seqprep.h"

static const BGMBank *sBank;
static uint32_t sImage, sImageSize;     /* AICA addresses */
static uint32_t sWavesAt;               /* the DCSeqWave table in it */
static uint32_t sSeq, sSeqSize;
static uint32_t sSent;                  /* DC_AICA_CMD_SEQ packets sent */
static int sHeading = SEQ_STOPPED;      /* what the last command aims at */
static uint32_t sMissedSeen;

static void seq_send(uint32_t what, uint32_t a0, uint32_t a1)
{
#ifdef DB_AICA_STATUS
    if (what != DC_SEQ_PRIORITY)
        dbglog(DBG_INFO, "bgm: to the ARM: command %lu (%lu, %lu) at %lu ms\n",
               (unsigned long)what, (unsigned long)a0, (unsigned long)a1,
               (unsigned long)timer_ms_gettime64());
#endif
    dc_aica_seq_cmd(what, a0, a1);
    sSent++;
}

static uint32_t align4(uint32_t n)
{
    return (n + 3) & ~3u;
}

static void fill_waves(DCSeqWave *out)
{
    uint32_t i;

    for (i = 0; i < sBank->n_waves; i++)
    {
        const BGMWaveTable *w = &sBank->waves[i];

        out[i].sram = sBank->wave_sram ? sBank->wave_sram[i] : 0;
        out[i].nsamples = w->nsamples;
        out[i].loop_start = w->loopStart;
        out[i].loop_end = w->loopEnd;
        out[i].loop = w->loopCount != 0;
        out[i].rate_shift = w->rateShift;
        out[i].pad[0] = out[i].pad[1] = 0;
    }
}

#ifdef DB_SEQ_SH4_SELFTEST
/* -DDB_SEQ_SH4_SELFTEST: the same player run on the SH-4 over one song,
 * its first voice starts logged as "shtrace" lines -- to tell a fault in
 * src/dc/seq from one in the ARM build of it (compare with the firmware's
 * "seqtrace" and tools/check/seqcore_oracle.c). */
static int sSelfN;
static SeqPlayer sSelf;
static void st_start(void *ctx, int vi, int wave, uint32_t r, int vol,
                     int pan, int fx, int wet, int dry, int32_t ramp)
{
    (void)ctx; (void)vi; (void)fx; (void)wet; (void)dry; (void)ramp;
    if (sSelfN++ < 80)
        dbglog(DBG_INFO, "shtrace %ld %d %lu %d %d\n", (long)sSelf.cur_time,
               wave, (unsigned long)r, vol, pan);
}
static void st_vol(void *c, int v, int vol, int32_t t) { (void)c; (void)v; (void)vol; (void)t; }
static void st_pitch(void *c, int v, uint32_t r) { (void)c; (void)v; (void)r; }
static void st_pan(void *c, int v, int p) { (void)c; (void)v; (void)p; }
static void st_fx(void *c, int v, int f) { (void)c; (void)v; (void)f; }
static void st_wd(void *c, int v, int w, int d) { (void)c; (void)v; (void)w; (void)d; }
static void st_stop(void *c, int v) { (void)c; (void)v; }
static const SeqSynth kSelf = { st_start, st_vol, st_pitch, st_pan, st_fx,
                                st_wd, st_stop };

static void selftest(const BGMBank *bank, const SeqOsc *osc, int song)
{
    static SeqBank sb;
    const BGMSeqEntry *e = &bank->seqs[song];
    uint8_t *buf = malloc(e->size);
    int i;

    if (!buf)
        return;
    memcpy(buf, bank->seqdata + e->off, e->size);
    sb.insts = bank->insts;
    sb.sounds = bank->sounds;
    sb.osc = osc;
    sb.n_insts = bank->n_insts;
    sb.percussion = bank->percussion;
    seq_init(&sSelf, &sb, &kSelf, NULL);
    seq_set_sequence(&sSelf, buf);
    seq_play(&sSelf);
    for (i = 0; i < SEQ_CHANNELS; i++)
        seq_set_chan_priority(&sSelf, i, 20);
    while (sSelf.cur_time < 6000000)
        seq_advance(&sSelf, 16000);
    free(buf);
}
#endif

int bgm_arm_setup(const BGMBank *bank, uint32_t max_seq)
{
    static int32_t values[8192];
    uint32_t n_insts = bank->n_insts, at;
    uint32_t o_insts, o_sounds, o_osc, o_values, o_waves;
    SeqOsc *osc;
    DCSeqImage *img;
    uint8_t *buf;
    int nv, i;

    osc = calloc(2 * n_insts, sizeof(*osc));
    if (!osc)
        return -1;
    nv = seq_prep_osc(bank->insts, n_insts, osc, values,
                      sizeof(values) / sizeof(values[0]));
    if (nv < 0)
    {
        dbglog(DBG_ERROR, "bgm: oscillator tables overflow\n");
        free(osc);
        return -1;
    }

    o_insts = align4(sizeof(DCSeqImage));
    o_sounds = align4(o_insts + n_insts * sizeof(BGMInstrument));
    o_osc = align4(o_sounds + bank->n_sounds * sizeof(BGMSound));
    o_values = align4(o_osc + 2 * n_insts * sizeof(SeqOsc));
    o_waves = align4(o_values + (uint32_t)nv * 4);
    sImageSize = (o_waves + bank->n_waves * sizeof(DCSeqWave) + 31) & ~31u;

    buf = calloc(1, sImageSize);
    sImage = snd_mem_malloc(sImageSize);
    sSeqSize = (max_seq + 31) & ~31u;
    sSeq = snd_mem_malloc(sSeqSize);
    if (!buf || !sImage || !sSeq)
    {
        dbglog(DBG_ERROR, "bgm: no room for the sequencer's %u + %u bytes "
               "of sound RAM\n", (unsigned)sImageSize, (unsigned)sSeqSize);
        free(buf);
        free(osc);
        if (sImage)
            snd_mem_free(sImage);
        if (sSeq)
            snd_mem_free(sSeq);
        sImage = sSeq = 0;
        return -1;
    }

#ifdef DB_SEQ_SH4_SELFTEST
    selftest(bank, osc, 9);
#endif
    /* Every pointer the ARM follows is an AICA address in this image. */
    at = sImage;
    for (i = 0; i < 2 * (int)n_insts; i++)
        if (osc[i].type)
            osc[i].values = (const int32_t *)(uintptr_t)
                (at + o_values + 4u * (uint32_t)(osc[i].values - values));
        else
            osc[i].values = NULL;
    img = (DCSeqImage *)buf;
    img->bank[0] = at + o_insts;
    img->bank[1] = at + o_sounds;
    img->bank[2] = at + o_osc;
    img->bank[3] = n_insts;
    img->bank[4] = (uint32_t)bank->percussion;
    img->rate = (uint32_t)bank->rate;
    img->n_waves = bank->n_waves;
    img->waves = at + o_waves;
    memcpy(buf + o_insts, bank->insts, n_insts * sizeof(BGMInstrument));
    memcpy(buf + o_sounds, bank->sounds, bank->n_sounds * sizeof(BGMSound));
    memcpy(buf + o_osc, osc, 2 * n_insts * sizeof(SeqOsc));
    memcpy(buf + o_values, values, (size_t)nv * 4);
    sBank = bank;
    sWavesAt = at + o_waves;
    fill_waves((DCSeqWave *)(buf + o_waves));

    spu_memload(sImage, buf, sImageSize);
    free(buf);
    free(osc);
#ifndef DB_REVERB_OFF
    /* the N64's reverb on the DSP; the voices' sends feed it (seqsyn.c) */
    dc_aica_dsp_load(kAicaReverbMpro, kAicaReverbCoef, kAicaReverbMadrs,
                     AICA_REVERB_RBL, AICA_REVERB_SILENCE);
#endif
    seq_send(DC_SEQ_BANK, sImage, 0);
    dbglog(DBG_INFO, "bgm: sequencer on the ARM; bank image %u bytes at "
           "%06lx (%d oscillator values), sequence buffer %u bytes\n",
           (unsigned)sImageSize, (unsigned long)sImage, nv,
           (unsigned)sSeqSize);
    return 0;
}

void bgm_arm_load_seq(const uint8_t *data, uint32_t size)
{
    uint8_t tail[4] = { 0, 0, 0, 0 };
    uint32_t body = size & ~3u;

    if (size > sSeqSize)
        return;
    /* spu_memload moves whole words */
    if (body)
        spu_memload(sSeq, (void *)data, body);
    if (size > body)
    {
        memcpy(tail, data + body, size - body);
        spu_memload(sSeq + body, tail, 4);
    }
}

void bgm_arm_play(int priority)
{
    int ch;

    seq_send(DC_SEQ_SETSEQ, sSeq, 0);
    seq_send(DC_SEQ_PLAY, 0, 0);
    for (ch = 0; ch < SEQ_CHANNELS; ch++)
        seq_send(DC_SEQ_PRIORITY, (uint32_t)ch, (uint32_t)priority);
    sHeading = SEQ_PLAYING;
}

void bgm_arm_stop(void)
{
    seq_send(DC_SEQ_STOP, 0, 0);
    if (sHeading == SEQ_PLAYING)
        sHeading = SEQ_STOPPING;
}

void bgm_arm_volume(int16_t vol)
{
    seq_send(DC_SEQ_VOLUME, (uint32_t)(uint16_t)vol, 0);
}

int bgm_arm_state(void)
{
    DCAicaStatus st;

    dc_aica_status(&st);
    if (st.seq_cmds != sSent)
        return sHeading;
    return (int)st.seq_state;
}

void bgm_arm_kill(void)
{
    DCAicaStatus st;
    int ms;

    seq_send(DC_SEQ_KILL, 0, 0);
    sHeading = SEQ_STOPPED;
    for (ms = 0; ms < 200; ms++)
    {
        dc_aica_status(&st);
        if (st.seq_cmds == sSent)
            return;
        thd_sleep(1);
    }
    dbglog(DBG_WARNING, "bgm: the AICA firmware did not take the kill "
           "(%lu of %lu commands)\n", (unsigned long)st.seq_cmds,
           (unsigned long)sSent);
}

void bgm_arm_waves_changed(void)
{
    DCSeqWave *w;
    size_t n;

    if (!sBank)
        return;
    n = sBank->n_waves * sizeof(DCSeqWave);
    w = malloc(n);
    if (!w)
        return;
    fill_waves(w);
    spu_memload(sWavesAt, w, (n + 3) & ~(size_t)3);
#ifdef DB_AICA_STATUS
    {
        uint32_t i, res = 0;

        for (i = 0; i < sBank->n_waves; i++)
            res += w[i].sram != 0;
        dbglog(DBG_INFO, "bgm: wave table to %06lx: %lu of %lu resident; "
               "wave 30 here %06lx, there %06lx\n", (unsigned long)sWavesAt,
               (unsigned long)res, (unsigned long)sBank->n_waves,
               (unsigned long)w[30].sram,
               (unsigned long)g2_read_32(SPU_RAM_UNCACHED_BASE + sWavesAt +
                                         30 * sizeof(DCSeqWave)));
    }
#endif
    free(w);
}

#ifdef DB_AICA_STATUS
/* the firmware's first voice starts (shared.h DC_AICA_TRACE_AT), once */
static void dump_trace(void)
{
    static int done;
    DCAicaStatus st;
    uint32_t i;

    dc_aica_status(&st);
    if (done || st.trace_n < DC_AICA_TRACE_N)
        return;
    done = 1;
    for (i = 0; i < DC_AICA_TRACE_N; i++)
    {
        uint32_t a = SPU_RAM_UNCACHED_BASE + DC_AICA_TRACE_AT + 16 * i;

        dbglog(DBG_INFO, "seqtrace %lu %lu %lu %lu %lu\n",
               (unsigned long)g2_read_32(a), (unsigned long)g2_read_32(a + 4),
               (unsigned long)g2_read_32(a + 8),
               (unsigned long)(g2_read_32(a + 12) >> 8),
               (unsigned long)(g2_read_32(a + 12) & 0xFF));
    }
}
#endif

#ifdef DB_AICA_STATUS
/* the DSP's 16-bit packed float -> 24-bit: sign, a count of redundant
 * sign bits (12 = none left), 11 mantissa bits */
static int32_t unpack(uint16_t w)
{
    int s = (w >> 15) & 1, e = (w >> 11) & 0xF;
    uint32_t u = ((uint32_t)(w & 0x7FF) << 11) | ((uint32_t)s << 22);

    if (e > 11)
        e = 11;
    else
        u ^= 1u << 22;
    u |= (uint32_t)s << 23;
    return ((int32_t)(u << 8) >> 8) >> e;
}

/* every 5 s: how much of the reverb's line (shared.h DC_AICA_RING_AT) is
 * not silence, sampled every 64th word -- the DSP is running and the
 * voices' sends reach it */
static void log_ring(void)
{
    static uint64_t next;
    uint64_t now = timer_ms_gettime64();
    uint32_t i, busy = 0, words = 0;
    int32_t peak = 0;
    int64_t sum = 0;

    if (now < next)
        return;
    next = now + 5000;
    for (i = 0; i < DC_AICA_RING_BYTES; i += 4 * 64, words += 2)
    {
        uint32_t w = g2_read_32(SPU_RAM_UNCACHED_BASE + DC_AICA_RING_AT + i);

        uint32_t k;

        for (k = 0; k < 2; k++)
        {
            uint16_t h = (uint16_t)(w >> (16 * k));
            int32_t v = unpack(h);

            busy += h != AICA_REVERB_SILENCE;
            sum += v;
            if (v < 0)
                v = -v;
            if (v > peak)
                peak = v;
        }
    }
    dbglog(DBG_INFO, "bgm: reverb line %lu of %lu sampled words not "
           "silent, peak %ld, mean %ld (24-bit)\n", (unsigned long)busy,
           (unsigned long)words, (long)peak, (long)(sum / (int64_t)words));
}
#endif

void bgm_arm_log_misses(void)
{
#ifdef DB_AICA_STATUS
    dump_trace();
    log_ring();
#endif
    DCAicaStatus st;

    dc_aica_status(&st);
    if (st.seq_missed != sMissedSeen)
    {
        dbglog(DBG_WARNING, "bgm: %lu note(s) on waves not resident (last "
               "wave %lu)\n", (unsigned long)(st.seq_missed - sMissedSeen),
               (unsigned long)st.seq_missed_wave);
        sMissedSeen = st.seq_missed;
    }
}
