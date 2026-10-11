/* seqsyn.c -- the music sequencer's voices, on the ARM.
 *
 * The voices run next to the channels, with no command queue: the wave lookup
 * and "not resident" rule, the volume law (spec 5.1's value, >> 7 + the mix headroom, into KOS's 0..255),
 * the linear ramp walked in steps of at least RAMP_MIN_STEP_US, and the
 * frequency as the wave's rate times the ratio -- in integers, through
 * KOS's aica.c, which reads each channel's parameters from `chans`.
 *
 * The effect send: each voice's dry and wet levels come
 * from the N64's equal-power split (spec 9.1), and its wet signal reaches
 * the reverb (tools/export/ssb_reverbexport.py's DSP program) through the
 * channel's DSP send. The send is mono and taken before pan, where the
 * N64's is stereo and after it, so the wet level is scaled by how much of
 * the voice a pan leaves in L + R (1 at the centre, 0.71 hard over; the
 * program's input gain makes the centre match). The channel's TL carries
 * the volume times the larger of the two levels, and DISDL and IMXL, 3 dB
 * a step, what each level is of that: the dry path stays exact, the send
 * is within 1.5 dB.
 */
#include "aica_cmd_iface.h"
#include "aica.h"

#include "shared.h"
#include "seqsyn.h"
#include "../aicamix.h"
#include "aica_sends.h"            /* tools/export/ssb_reverbexport.py */

#define RAMP_MIN_STEP_US 4000

extern volatile aica_channel_t *chans;

typedef struct
{
    uint8_t sent_vol, live, pan, pad;
    int32_t vol_from, vol_to;
    int32_t elapsed, len, since;
    uint32_t base_freq;
    int32_t dry, wet;           /* the N64's levels, Q15 (spec 9.1) */
    int32_t hi;                 /* the larger of dry and the panned wet */
} SynVoice;

static SynVoice sv[SEQ_VOICES];
int32_t gSeqTraceTime;
static const DCSeqImage *img;
static volatile DCAicaStatus *stat;

/* every entry point checks its voice; one out of range does nothing
 * rather than write outside sv[] */
static int bad(int i)
{
    return i < 0 || i >= SEQ_VOICES;
}

static uint8_t to_aica_vol(int32_t v)
{
    if (v < 0)
        v = 0;
    v >>= 7 + AICA_MIX_HEADROOM_SHIFT;
    return (uint8_t)(v > 255 ? 255 : v);
}

static int32_t eq(int i)
{
    /* CC 23 = 0 on a sounding voice indexes one past the table (spec
     * 9.1): the N64 reads -32759 there and flips the dry signal's phase,
     * which a channel cannot do; its level is kept. SSB never sends it. */
    if (i >= AICA_EQPOWER_LENGTH)
        return 32759;
    return kAicaEqPower[i < 0 ? 0 : i];
}

/* the AICA's 3 dB send step nearest a Q15 level */
static int send_step(int32_t level)
{
    int i;

    for (i = 15; i > 0; i--)
        if (level >= kAicaSendStep[i])
            return i;
    return 0;
}

/* Split a voice: `dry` and `wet` Q15 levels at N64 pan `pan` into the
 * larger level (TL's share) and the two send steps, and write the steps
 * to channel `ch`. */
static int32_t split(int ch, int32_t dry, int32_t wet, int pan)
{
    int32_t lr = eq(pan) + eq(127 - pan);
    int32_t hi;
    int disdl = 0, imxl = 0;

    wet = (int32_t)(((int64_t)wet * lr) / (eq(64) + eq(63)));
    hi = dry > wet ? dry : wet;
    if (hi > 0)
    {
        disdl = send_step((int32_t)(((int64_t)dry << 15) / hi));
        imxl = send_step((int32_t)(((int64_t)wet << 15) / hi));
    }
    CHNREG8(ch, 37) = (uint8_t)disdl;
    /* ISEL 0: every voice into the DSP's MIXS0, the reverb's input */
    CHNREG32(ch, 32) = (uint32_t)imxl << 4;
    return hi;
}

/* the voice's send steps now; push_vol then writes TL with its share */
static void sends(int i)
{
    SynVoice *s = &sv[i];

    s->hi = split(DC_SEQ_CHN_BASE + i, s->dry, s->wet, s->pan);
}

static uint8_t to_aica_pan(int pan)
{
    return (uint8_t)(pan > 127 ? 255 : pan * 2);
}

static int32_t ramp_value(const SynVoice *s)
{
    int64_t span;

    if (s->len <= 0 || s->elapsed >= s->len)
        return s->vol_to;
    span = (int64_t)(s->vol_to - s->vol_from) * s->elapsed;
    return s->vol_from + (int32_t)(span / s->len);
}

static uint32_t freq_of(const SynVoice *s, uint32_t ratio)
{
    return (uint32_t)(((uint64_t)s->base_freq * ratio) >> 24);
}

static void push_vol(int i)
{
    SynVoice *s = &sv[i];
    uint8_t want = to_aica_vol((int32_t)(((int64_t)ramp_value(s) * s->hi)
                                         >> 15));

    if (want == s->sent_vol)
        return;
    s->sent_vol = want;
    chans[DC_SEQ_CHN_BASE + i].vol = want;
    aica_vol(DC_SEQ_CHN_BASE + i);
}

static void syn_start(void *ctx, int i, int wave, uint32_t ratio, int vol,
                      int pan, int fxmix, int wet, int dry, int32_t ramp)
{
    if (bad(i))
        return;
    SynVoice *s = &sv[i];
    const DCSeqWave *w;
    volatile aica_channel_t *c = &chans[DC_SEQ_CHN_BASE + i];

    (void)ctx;
    if (stat->trace_n < DC_AICA_TRACE_N)
    {
        volatile uint32_t *t = (volatile uint32_t *)DC_AICA_TRACE_AT +
                               4 * stat->trace_n;

        t[0] = (uint32_t)gSeqTraceTime;
        t[1] = (uint32_t)wave;
        t[2] = ratio;
        t[3] = ((uint32_t)vol << 8) | (uint32_t)pan;
        stat->trace_n++;
    }
    s->live = 0;
    if (!img || (uint32_t)wave >= img->n_waves)
        return;
    w = &((const DCSeqWave *)img->waves)[wave];
    if (!w->sram)
    {
        /* silent, and counted: the SH-4 logs it (bgmarm.c) */
        stat->seq_missed++;
        stat->seq_missed_wave = (uint32_t)wave;
        return;
    }
    s->base_freq = img->rate >> w->rate_shift;
    s->pan = (uint8_t)pan;
    s->vol_from = 0;
    s->vol_to = vol;
    s->elapsed = 0;
    s->len = ramp;
    s->since = 0;
    s->live = 1;
    /* n_env.c:912-919: the channel's default wet and dry (0, 95) mean
     * "follow the FX mix" */
    if (wet == 0 && dry == 95)
    {
        s->dry = eq(fxmix);
        s->wet = eq(127 - fxmix);
    }
    else
    {
        s->dry = eq(127 - dry);
        s->wet = eq(127 - wet);
    }
    s->hi = s->dry;
    s->sent_vol = 0;

    c->cmd = AICA_CH_CMD_START;
    c->base = w->sram;
    c->type = AICA_SM_ADPCM;
    c->length = w->nsamples;
    c->loop = w->loop;
    c->loopstart = w->loop_start;
    c->loopend = w->loop_end ? w->loop_end : w->nsamples;
    c->freq = freq_of(s, ratio);
    c->vol = 0;
    c->pan = to_aica_pan(pan);
    c->pos = 0;
    aica_play(DC_SEQ_CHN_BASE + i, 0);
    /* aica_play sets DISDL to full; the voice's own split replaces it */
    sends(i);
    push_vol(i);
}

static void syn_vol(void *ctx, int i, int vol, int32_t ramp)
{
    if (bad(i))
        return;
    SynVoice *s = &sv[i];

    (void)ctx;
    s->vol_from = ramp_value(s);
    s->vol_to = vol;
    s->elapsed = 0;
    s->len = ramp > 0 ? ramp : 0;
    s->since = RAMP_MIN_STEP_US;
    if (s->live && s->len == 0)
        push_vol(i);
}

static void syn_pitch(void *ctx, int i, uint32_t ratio)
{
    if (bad(i))
        return;
    (void)ctx;
    if (!sv[i].live)
        return;
    chans[DC_SEQ_CHN_BASE + i].freq = freq_of(&sv[i], ratio);
    aica_freq(DC_SEQ_CHN_BASE + i);
}

static void syn_pan(void *ctx, int i, int pan)
{
    if (bad(i))
        return;
    (void)ctx;
    if (!sv[i].live || sv[i].pan == pan)
        return;
    sv[i].pan = (uint8_t)pan;
    chans[DC_SEQ_CHN_BASE + i].pan = to_aica_pan(pan);
    aica_pan(DC_SEQ_CHN_BASE + i);
    sends(i);
    push_vol(i);
}

static void syn_fxmix(void *ctx, int i, int fxmix)
{
    /* n_env.c:1005-1008 */
    if (bad(i))
        return;
    (void)ctx;
    sv[i].dry = eq(fxmix);
    sv[i].wet = eq(127 - fxmix);
    if (!sv[i].live)
        return;
    sends(i);
    push_vol(i);
}

static void syn_wetdry(void *ctx, int i, int wet, int dry)
{
    /* n_env.c:1010-1013: 128 - dry here, one more than at a start */
    if (bad(i))
        return;
    (void)ctx;
    sv[i].dry = eq(128 - dry);
    sv[i].wet = eq(127 - wet);
    if (!sv[i].live)
        return;
    sends(i);
    push_vol(i);
}

static void syn_stop(void *ctx, int i)
{
    if (bad(i))
        return;
    (void)ctx;
    sv[i].live = 0;
    sv[i].len = 0;
    sv[i].vol_to = 0;
    sv[i].sent_vol = 0;
    aica_stop(DC_SEQ_CHN_BASE + i);
}

const SeqSynth gSeqSynAica = {
    syn_start, syn_vol, syn_pitch, syn_pan, syn_fxmix, syn_wetdry, syn_stop,
};

void seqsyn_init(volatile DCAicaStatus *status)
{
    int i;

    stat = status;
    img = 0;
    for (i = 0; i < SEQ_VOICES; i++)
        syn_stop(0, i);
}

void seqsyn_bank(const DCSeqImage *image)
{
    img = image;
}

void seqsyn_silence(void)
{
    int i;

    for (i = 0; i < SEQ_VOICES; i++)
        syn_stop(0, i);
}

void seqsyn_fx_send(int ch, int fxmix, int pan)
{
    /* n_env.c:4362-4374: dry = eqpower[s], wet = eqpower[127 - s]. The
     * effect's TL is fgm.c's and stays as it is: an effect's send is a
     * quarter of its script's fx at the default base, so its wet level is
     * well under its dry and the dry level's own fall (0.6 dB at most
     * then) is not carried. */
    if (ch < 0 || ch >= 64 || (ch >= DC_SEQ_CHN_BASE &&
                               ch < DC_SEQ_CHN_BASE + SEQ_VOICES))
        return;
    if (fxmix > 127)
        fxmix = 127;
    if (pan > 127)
        pan = 127;
    (void)split(ch, eq(fxmix), eq(127 - fxmix), pan);
    stat->fx_sends++;
}

void seqsyn_advance(int32_t us)
{
    int i;

    for (i = 0; i < SEQ_VOICES; i++)
    {
        SynVoice *s = &sv[i];

        if (!s->live || s->len <= 0 || s->elapsed >= s->len)
            continue;
        s->elapsed += us;
        s->since += us;
        if (s->since < RAMP_MIN_STEP_US && s->elapsed < s->len)
            continue;
        s->since = 0;
        push_vol(i);
    }
}
