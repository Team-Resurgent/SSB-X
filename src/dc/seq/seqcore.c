/* seqcore.c -- see seqcore.h. */
#include "seqcore.h"

#include <stddef.h>

/* ---- pitch: 2^(c/1200) without a float ------------------------------- */

/* 2^(k/12) and 2^(k/1200), Q2.30 */
static const uint32_t kSemi[12] = {
    0x40000000u, 0x43CE3E4Bu, 0x47D66B0Fu, 0x4C1BF829u, 0x50A28BE6u,
    0x556E0424u, 0x5A82799Au, 0x5FE4435Eu, 0x6597FA95u, 0x6BA27E65u,
    0x7208F81Du, 0x78D0DF9Cu,
};
static const uint32_t kCent[100] = {
    0x40000000u, 0x4009776Du, 0x4012F040u, 0x401C6A7Au, 0x4025E61Bu,
    0x402F6322u, 0x4038E192u, 0x40426168u, 0x404BE2A7u, 0x4055654Du,
    0x405EE95Bu, 0x40686ED2u, 0x4071F5B1u, 0x407B7DF9u, 0x408507AAu,
    0x408E92C4u, 0x40981F47u, 0x40A1AD34u, 0x40AB3C8Bu, 0x40B4CD4Cu,
    0x40BE5F77u, 0x40C7F30Cu, 0x40D1880Cu, 0x40DB1E77u, 0x40E4B64Du,
    0x40EE4F8Eu, 0x40F7EA3Bu, 0x41018653u, 0x410B23D8u, 0x4114C2C8u,
    0x411E6324u, 0x412804EEu, 0x4131A823u, 0x413B4CC6u, 0x4144F2D6u,
    0x414E9A53u, 0x4158433Eu, 0x4161ED97u, 0x416B995Du, 0x41754692u,
    0x417EF535u, 0x4188A547u, 0x419256C8u, 0x419C09B8u, 0x41A5BE17u,
    0x41AF73E5u, 0x41B92B23u, 0x41C2E3D1u, 0x41CC9DF0u, 0x41D6597Eu,
    0x41E0167Du, 0x41E9D4EDu, 0x41F394CEu, 0x41FD561Fu, 0x420718E3u,
    0x4210DD18u, 0x421AA2BEu, 0x422469D7u, 0x422E3262u, 0x4237FC60u,
    0x4241C7D0u, 0x424B94B3u, 0x42556309u, 0x425F32D2u, 0x4269040Fu,
    0x4272D6C0u, 0x427CAAE4u, 0x4286807Du, 0x4290578Au, 0x429A300Cu,
    0x42A40A03u, 0x42ADE56Fu, 0x42B7C250u, 0x42C1A0A6u, 0x42CB8072u,
    0x42D561B4u, 0x42DF446Cu, 0x42E9289Au, 0x42F30E3Fu, 0x42FCF55Bu,
    0x4306DDEEu, 0x4310C7F8u, 0x431AB379u, 0x4324A072u, 0x432E8EE3u,
    0x43387ECBu, 0x4342702Cu, 0x434C6306u, 0x43565758u, 0x43604D24u,
    0x436A4468u, 0x43743D26u, 0x437E375Du, 0x4388330Eu, 0x4392303Au,
    0x439C2EDFu, 0x43A62EFFu, 0x43B03099u, 0x43BA33AFu, 0x43C4383Fu,
};

/* a * b for two Q8.24 ratios */
static uint32_t rmul(uint32_t a, uint32_t b)
{
    return (uint32_t)(((uint64_t)a * b) >> 24);
}

/* 2^(r/1200) for 0 <= r < 1200, Q8.24 */
static uint32_t ratio_in_octave(uint32_t r)
{
    uint64_t q30 = ((uint64_t)kSemi[r / 100] * kCent[r % 100]) >> 30;

    return (uint32_t)((q30 + 32) >> 6);
}

uint32_t seq_cents_ratio(int32_t cents)
{
    uint32_t a = (uint32_t)(cents < 0 ? -cents : cents);
    uint32_t oct = a / 1200, r = a % 1200;

    if (cents >= 0)
        return ratio_in_octave(r) << oct;
    /* 2^-(oct + r/1200) = 2^-(oct+1) * 2^((1200-r)/1200) */
    if (r == 0)
        return SEQ_RATIO_ONE >> oct;
    return ratio_in_octave(1200 - r) >> (oct + 1);
}

/* ---- the compressed-MIDI reader, spec 1 ------------------------------ */

enum
{
    SEV_NONE, SEV_MIDI, SEV_TEMPO, SEV_END, SEV_TRACKEND,
    SEV_LOOPSTART, SEV_LOOPEND,
};

typedef struct
{
    uint8_t kind, status, b1, b2;
    uint32_t value;             /* note duration in ticks, or the tempo */
} SeqMsg;

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/* spec 1.3: one byte of track t, through the back-reference expander */
static uint8_t fetch(SeqReader *r, int t)
{
    uint8_t b, hi;

    if (r->bref_left[t])
    {
        r->bref_left[t]--;
        return r->base[r->bref[t]++];
    }
    b = r->base[r->cursor[t]++];
    if (b != 0xFE)
        return b;
    hi = r->base[r->cursor[t]++];
    if (hi == 0xFE)
        return 0xFE;            /* an escaped literal */
    {
        uint32_t code_at = r->cursor[t] - 2;    /* the 0xFE itself */
        uint32_t dist = ((uint32_t)hi << 8) | r->base[r->cursor[t]];
        uint8_t len = r->base[r->cursor[t] + 1];

        r->cursor[t] += 2;
        r->bref[t] = code_at - dist;
        r->bref_left[t] = (uint8_t)(len - 1);
        return r->base[r->bref[t]++];
    }
}

static uint32_t varlen(SeqReader *r, int t)
{
    uint32_t v = fetch(r, t), c;

    if (v & 0x80)
    {
        v &= 0x7F;
        do
        {
            c = fetch(r, t);
            v = (v << 7) + (c & 0x7F);
        } while (c & 0x80);
    }
    return v;
}

static void reader_open(SeqReader *r, uint8_t *data)
{
    int t;

    r->base = data;
    r->valid = 0;
    r->tick = 0;
    r->division = be32(data + 0x40);
    for (t = 0; t < SEQ_CHANNELS; t++)
    {
        uint32_t off = be32(data + 4 * t);

        r->status[t] = 0;
        r->bref_left[t] = 0;
        r->next_tick[t] = 0;
        if (!off)
            continue;
        r->valid |= 1u << t;
        r->cursor[t] = off;
        r->next_tick[t] = varlen(r, t);
    }
}

/* spec 1.6: ticks from now to the earliest next event, 0 if none */
static int reader_next_delta(const SeqReader *r, uint32_t *ticks)
{
    uint32_t best = 0xFFFFFFFFu;
    int t, any = 0;

    for (t = 0; t < SEQ_CHANNELS; t++)
        if ((r->valid & (1u << t)) && r->next_tick[t] - r->tick < best)
        {
            best = r->next_tick[t] - r->tick;
            any = 1;
        }
    *ticks = best;
    return any;
}

/* spec 1.5: the six bytes come off the cursor, not through fetch() */
static void loop_end(SeqReader *r, int t)
{
    uint8_t *p = r->base + r->cursor[t];
    uint32_t after = r->cursor[t] + 6;

    if (p[1] == 0)
    {
        p[1] = p[0];            /* finished; re-arm */
        r->cursor[t] = after;
        return;
    }
    if (p[1] != 0xFF)
        p[1]--;
    r->cursor[t] = after - be32(p + 2);
}

static void reader_next(SeqReader *r, SeqMsg *m)
{
    int t, pick = -1;
    uint8_t s;

    for (t = 0; t < SEQ_CHANNELS; t++)
        if ((r->valid & (1u << t)) &&
            (pick < 0 || r->next_tick[t] - r->tick <
                         r->next_tick[pick] - r->tick))
            pick = t;
    m->kind = SEV_NONE;
    m->status = m->b1 = m->b2 = 0;
    m->value = 0;
    if (pick < 0)
        return;
    t = pick;
    r->tick = r->next_tick[t];

    s = fetch(r, t);
    if (s == 0xFF)
    {
        uint8_t type = fetch(r, t);

        if (type == 0x51)
        {
            uint32_t a = fetch(r, t), b = fetch(r, t), c = fetch(r, t);

            m->kind = SEV_TEMPO;
            m->value = (a << 16) | (b << 8) | c;
            r->status[t] = 0;
        }
        else if (type == 0x2F)
        {
            r->valid &= ~(1u << t);
            m->kind = r->valid ? SEV_TRACKEND : SEV_END;
            return;             /* an ended track reads nothing more */
        }
        else if (type == 0x2E)
        {
            fetch(r, t);
            fetch(r, t);
            m->kind = SEV_LOOPSTART;
            r->status[t] = 0;
        }
        else if (type == 0x2D)
        {
            loop_end(r, t);
            m->kind = SEV_LOOPEND;
            r->status[t] = 0;
        }
        /* any other meta: nothing (SSB has none) */
    }
    else
    {
        uint8_t cls;

        if (s & 0x80)
        {
            r->status[t] = s;
            m->b1 = fetch(r, t);
        }
        else
        {
            m->b1 = s;
        }
        m->status = r->status[t];
        cls = m->status & 0xF0;
        m->b2 = (cls == 0xC0 || cls == 0xD0) ? 0 : fetch(r, t);
        m->value = (cls == 0x90) ? varlen(r, t) : 0;
        m->kind = SEV_MIDI;
    }
    r->next_tick[t] += varlen(r, t);
}

/* ---- the event queue, spec 2.4 --------------------------------------- */

enum
{
    EV_NONE, EV_SEQREF, EV_NOTEOFF, EV_ENV, EV_NOTEEND, EV_OSC, EV_API,
    EV_VOLUME, EV_PRIORITY, EV_FXMIX, EV_PLAY, EV_STOPREQ, EV_STOP,
    EV_SETSEQ,
};

#define EVQ_END 0x7FFFFFFF
#define FRAME_US 16000          /* the housekeeping tick */
#define KILL_US 50000           /* spec 3.12 */

static void post(SeqPlayer *p, const SeqEvent *e, int32_t delay)
{
    int16_t it = p->ev_head, prev = -1, n = p->ev_free;

    if (n < 0)
        return;                 /* full: dropped, as on the N64 */
    p->ev_free = p->ev[n].next;
    p->ev[n] = *e;
    if (delay == EVQ_END)
    {
        /* after the last item, at its due time */
        while (it >= 0)
        {
            prev = it;
            it = p->ev[it].next;
        }
        delay = 0;
    }
    else
    {
        while (it >= 0 && p->ev[it].delta <= delay)
        {
            delay -= p->ev[it].delta;
            prev = it;
            it = p->ev[it].next;
        }
    }
    p->ev[n].delta = delay;
    p->ev[n].next = it;
    if (it >= 0)
        p->ev[it].delta -= delay;
    if (prev >= 0)
        p->ev[prev].next = n;
    else
        p->ev_head = n;
}

static void unlink_at(SeqPlayer *p, int16_t prev, int16_t it)
{
    int16_t next = p->ev[it].next;

    if (next >= 0)
        p->ev[next].delta += p->ev[it].delta;
    if (prev >= 0)
        p->ev[prev].next = next;
    else
        p->ev_head = next;
    p->ev[it].next = p->ev_free;
    p->ev_free = it;
}

/* Remove every queued event of `type` (and, if voice >= 0, of that voice;
 * if which >= 0, of that oscillator). */
static void flush(SeqPlayer *p, int type, int voice, int which)
{
    int16_t it = p->ev_head, prev = -1;

    while (it >= 0)
    {
        int16_t next = p->ev[it].next;

        if (p->ev[it].type == type &&
            (voice < 0 || p->ev[it].voice == voice) &&
            (which < 0 || p->ev[it].value == which))
            unlink_at(p, prev, it);
        else
            prev = it;
        it = next;
    }
}

static void post_simple(SeqPlayer *p, int type, int voice, int chan,
                        int key, int32_t value, int32_t delay)
{
    SeqEvent e;

    e.type = (uint8_t)type;
    e.chan = (uint8_t)chan;
    e.key = (uint8_t)key;
    e.pad = 0;
    e.value = value;
    e.voice = (int16_t)voice;
    e.next = -1;
    e.delta = 0;
    post(p, &e, delay);
}

/* ---- voices ---------------------------------------------------------- */

enum { PH_ATTACK, PH_DECAY, PH_RELEASE };
enum { SU_NOTEON, SU_SUSTAIN, SU_SUSTREL, SU_RELEASE };

static const BGMSound *sound_of(const SeqPlayer *p, const SeqVoice *v)
{
    return &p->bank->sounds[v->sound];
}

/* spec 5.1 */
static int voice_vol(const SeqPlayer *p, const SeqVoice *v)
{
    const SeqChannel *c = &p->chan[v->chan];
    uint32_t seqvol = ((uint32_t)(uint16_t)p->vol * p->master_vol) >> 7;
    uint32_t chanvol = ((uint32_t)c->vol * c->vol2) / 127;
    uint32_t t1 = ((uint32_t)v->trem * v->vel * v->env_gain) >> 6;
    uint32_t t2 = ((uint32_t)sound_of(p, v)->sampleVolume * seqvol *
                   chanvol) >> 14;

    return (int)((t1 * t2) >> 15);
}

/* spec 5.3, the remaining-segment ramp */
static int32_t remaining(const SeqPlayer *p, const SeqVoice *v)
{
    int32_t d = v->env_end - p->cur_time;

    return d >= 0 ? d : 1000;
}

/* spec 6 */
static int voice_pan(const SeqPlayer *p, const SeqVoice *v)
{
    int pan = (int)p->chan[v->chan].pan - 64 + sound_of(p, v)->samplePan;

    return pan < 0 ? 0 : pan > 127 ? 127 : pan;
}

/* spec 3.5 / 8.3: base x (1 + (vib - 1) x vibScale / 127) x bend */
static uint32_t voice_ratio(const SeqPlayer *p, const SeqVoice *v)
{
    const SeqChannel *c = &p->chan[v->chan];
    int64_t d = ((int64_t)v->vib - (int64_t)SEQ_RATIO_ONE) * c->vib_scale;
    uint32_t term = (uint32_t)((int64_t)SEQ_RATIO_ONE + d / 127);

    return rmul(rmul(v->base, term), c->bend);
}

static void osc_return(SeqPlayer *p, SeqVoice *v, int vi)
{
    int w;

    for (w = 0; w < 2; w++)
        if (v->osc[w] >= 0)
        {
            p->osc[(int)v->osc[w]].next = p->osc_free;
            p->osc_free = v->osc[w];
            v->osc[w] = -1;
            flush(p, EV_OSC, vi, w);
        }
}

static void voice_free(SeqPlayer *p, int vi)
{
    int8_t *link = &p->v_alloc;

    osc_return(p, &p->voice[vi], vi);
    while (*link >= 0 && *link != vi)
        link = &p->voice[(int)*link].next;
    if (*link == vi)
        *link = p->voice[vi].next;
    p->voice[vi].next = p->v_free;
    p->v_free = (int8_t)vi;
}

/* spec 5.4 */
static void voice_release(SeqPlayer *p, int vi, int32_t r)
{
    SeqVoice *v = &p->voice[vi];

    if (v->phase == PH_ATTACK)
        flush(p, EV_ENV, vi, -1);
    v->vel = 0;
    v->phase = PH_RELEASE;
    v->env_gain = 0;
    v->env_end = p->cur_time + r;
    p->syn->vol(p->ctx, vi, voice_vol(p, v), r);
    post_simple(p, EV_NOTEEND, vi, 0, 0, 0, r);
}

/* spec 8.1: take a state if there is one; a zero delay leaks it */
static void osc_start(SeqPlayer *p, SeqVoice *v, int vi, int which)
{
    const SeqOsc *shape =
        &p->bank->osc[(int)(p->chan[v->chan].inst - p->bank->insts) * 2 +
                      which];
    int8_t s = p->osc_free;

    if (!shape->type || s < 0)
        return;
    p->osc_free = p->osc[(int)s].next;
    p->osc[(int)s].count = shape->count0;
    p->osc[(int)s].len = shape->len;
    p->osc[(int)s].which = (uint8_t)which;
    p->osc[(int)s].shape = shape;
    if (which)
        v->vib = (uint32_t)shape->start;
    else
        v->trem = (uint8_t)shape->start;
    if (shape->delay_us == 0)
        return;                 /* never returned: the N64's leak */
    v->osc[which] = s;
    post_simple(p, EV_OSC, vi, 0, 0, which, shape->delay_us);
}

/* spec 4.2 */
static int find_sound(const SeqPlayer *p, const BGMInstrument *inst,
                      uint8_t key, uint8_t vel)
{
    const BGMSound *base = &p->bank->sounds[inst->soundFirst];
    int lo = 0, hi = inst->soundCount;

    while (lo < hi)
    {
        int mid = lo + (hi - lo - 1) / 2;
        const BGMSound *s = &base[mid];
        int key_in = key >= s->keyMin && key <= s->keyMax;

        if (key_in && vel >= s->velocityMin && vel <= s->velocityMax)
            return inst->soundFirst + mid;
        if (key < s->keyMin || (key_in && vel < s->velocityMin))
            hi = mid;
        else
            lo = mid + 1;
    }
    return -1;
}

/* spec 3.2 */
static void note_on(SeqPlayer *p, int ch, uint8_t key, uint8_t vel,
                    uint32_t duration)
{
    SeqChannel *c = &p->chan[ch];
    const BGMSound *snd;
    SeqVoice *v;
    int8_t *tail;
    int vi, si;

    if (p->state != SEQ_PLAYING || !c->inst)
        return;
    si = find_sound(p, c->inst, key, vel);
    if (si < 0 || p->v_free < 0)
        return;
    vi = p->v_free;
    v = &p->voice[vi];
    p->v_free = v->next;
    v->next = -1;
    for (tail = &p->v_alloc; *tail >= 0; tail = &p->voice[(int)*tail].next)
        ;
    *tail = (int8_t)vi;

    snd = &p->bank->sounds[si];
    v->chan = (uint8_t)ch;
    v->key = key;
    v->vel = vel;
    v->sound = (int16_t)si;
    v->phase = PH_ATTACK;
    v->sustain = c->sustain > 63 ? SU_SUSTAIN : SU_NOTEON;
    v->base = seq_cents_ratio((int16_t)(((int)key - snd->keyBase) * 100 +
                                        snd->detune));
    v->env_gain = snd->attackVolume;
    v->env_end = p->cur_time + snd->attackTime;
    v->trem = 127;
    v->vib = SEQ_RATIO_ONE;
    v->osc[0] = v->osc[1] = -1;
    osc_start(p, v, vi, 0);
    osc_start(p, v, vi, 1);

    /* the start value goes in unscaled (spec 8.3) */
    p->syn->start(p->ctx, vi, snd->wave,
                  rmul(rmul(v->base, c->bend), v->vib), voice_vol(p, v),
                  voice_pan(p, v), c->fxmix, c->wet, c->dry,
                  snd->attackTime);
    post_simple(p, EV_ENV, vi, 0, 0, 0, snd->attackTime);
    if (duration)
        post_simple(p, EV_NOTEOFF, -1, ch, key, 0,
                    (int32_t)((uint32_t)p->uspt * duration));
}

/* spec 3.3: the oldest voice on (ch, key) not already released */
static int find_held(const SeqPlayer *p, int ch, uint8_t key)
{
    int8_t vi;

    for (vi = p->v_alloc; vi >= 0; vi = p->voice[(int)vi].next)
    {
        const SeqVoice *v = &p->voice[(int)vi];

        if (v->chan == ch && v->key == key &&
            v->sustain != SU_RELEASE && v->sustain != SU_SUSTREL)
            return vi;
    }
    return -1;
}

static void note_off(SeqPlayer *p, int ch, uint8_t key)
{
    int vi = find_held(p, ch, key);
    SeqVoice *v;

    if (vi < 0)
        return;
    v = &p->voice[vi];
    if (v->sustain == SU_SUSTAIN)
    {
        v->sustain = SU_SUSTREL;
        return;
    }
    v->sustain = SU_RELEASE;
    voice_release(p, vi, sound_of(p, v)->releaseTime);
}

/* ---- channels -------------------------------------------------------- */

static void chan_perf_reset(SeqChannel *c)
{
    c->vol = 127;
    c->fxmix = 0;
    c->sustain = 0;
    c->bend = SEQ_RATIO_ONE;
    c->wet = 0;
    c->dry = 95;
}

static void chan_take_inst(SeqChannel *c, const BGMInstrument *inst)
{
    c->inst = inst;
    c->priority = inst->priority;
    c->bend_range = inst->bendRange;
    c->vol2 = inst->volume;
}

/* spec 3.11 */
static void chans_from_bank(SeqPlayer *p)
{
    const SeqBank *b = p->bank;
    const BGMInstrument *first = NULL;
    uint32_t i;
    int ch;

    for (i = 0; i < b->n_insts && !first; i++)
        if (b->insts[i].valid)
            first = &b->insts[i];
    for (ch = 0; ch < SEQ_CHANNELS; ch++)
    {
        SeqChannel *c = &p->chan[ch];

        chan_perf_reset(c);
        if (!first)
            continue;
        chan_take_inst(c, first);
        c->pan = first->pan;
    }
    if (b->percussion >= 0)
    {
        SeqChannel *c = &p->chan[9];
        const BGMInstrument *perc = &b->insts[b->percussion];

        chan_perf_reset(c);
        chan_take_inst(c, perc);
        c->pan = perc->pan;
    }
}

static void each_on_chan(SeqPlayer *p, int ch, int what, int skip_released)
{
    int8_t vi;

    for (vi = p->v_alloc; vi >= 0; vi = p->voice[(int)vi].next)
    {
        SeqVoice *v = &p->voice[(int)vi];

        if (v->chan != ch || (skip_released && v->phase == PH_RELEASE))
            continue;
        switch (what)
        {
        case 0:
            p->syn->vol(p->ctx, vi, voice_vol(p, v), remaining(p, v));
            break;
        case 1:
            p->syn->pan(p->ctx, vi, voice_pan(p, v));
            break;
        case 2:
            p->syn->fxmix(p->ctx, vi, p->chan[ch].fxmix);
            break;
        case 3:
            p->syn->wetdry(p->ctx, vi, p->chan[ch].wet, p->chan[ch].dry);
            break;
        case 4:
            p->syn->pitch(p->ctx, vi, voice_ratio(p, v));
            break;
        }
    }
}

/* spec 3.7 */
static void control(SeqPlayer *p, int ch, uint8_t cc, uint8_t val)
{
    SeqChannel *c = &p->chan[ch];
    int8_t vi;

    switch (cc)
    {
    case 7:
        c->vol = val;
        each_on_chan(p, ch, 0, 1);
        break;
    case 10:
        c->pan = val;
        each_on_chan(p, ch, 1, 0);
        break;
    case 16:
    case 25:
        c->priority = val;
        break;
    case 20:
        c->bend_range = (int16_t)(val >= 121 ? 1200 : val * 10);
        break;
    case 21:
        p->master_vol = val;
        break;
    case 22:
        c->wet = val;
        each_on_chan(p, ch, 3, 0);
        break;
    case 23:
        c->dry = val;
        each_on_chan(p, ch, 3, 0);
        break;
    case 64:
        c->sustain = val;
        for (vi = p->v_alloc; vi >= 0; )
        {
            SeqVoice *v = &p->voice[(int)vi];
            int8_t next = v->next;

            if (v->chan == ch && v->sustain != SU_RELEASE)
            {
                if (val > 63)
                {
                    if (v->sustain == SU_NOTEON)
                        v->sustain = SU_SUSTAIN;
                }
                else if (v->sustain == SU_SUSTAIN)
                {
                    v->sustain = SU_NOTEON;
                }
                else if (v->sustain == SU_SUSTREL)
                {
                    v->sustain = SU_RELEASE;
                    voice_release(p, vi, sound_of(p, v)->releaseTime);
                }
            }
            vi = next;
        }
        break;
    case 91:
        c->fxmix = val;
        each_on_chan(p, ch, 2, 0);
        break;
    default:
        break;                  /* 24 and the rest: nothing */
    }
}

static void midi(SeqPlayer *p, const SeqMsg *m)
{
    int ch = m->status & 0x0F;
    SeqChannel *c = &p->chan[ch];

    switch (m->status & 0xF0)
    {
    case 0x90:
        if (m->b2)
        {
            note_on(p, ch, m->b1, m->b2, m->value);
            break;
        }
        note_off(p, ch, m->b1);     /* velocity 0 is a note off */
        break;
    case 0x80:
        note_off(p, ch, m->b1);
        break;
    case 0xA0:
    {
        int vi = find_held(p, ch, m->b1);

        if (vi >= 0)
        {
            p->voice[vi].vel = m->b2;
            p->syn->vol(p->ctx, vi, voice_vol(p, &p->voice[vi]),
                        remaining(p, &p->voice[vi]));
        }
        break;
    }
    case 0xB0:
        control(p, ch, m->b1, m->b2);
        break;
    case 0xC0:
        if (m->b1 < p->bank->n_insts)
        {
            const BGMInstrument *inst = &p->bank->insts[m->b1];

            if (inst->valid)
                chan_take_inst(c, inst);
            else
                c->inst = NULL;
        }
        break;
    case 0xD0:
        c->vib_scale = m->b1;
        break;
    case 0xE0:
    {
        int32_t bend = ((int32_t)m->b2 << 7) + m->b1 - 8192;

        c->bend = seq_cents_ratio((c->bend_range * bend) / 8192);
        each_on_chan(p, ch, 4, 0);
        break;
    }
    default:
        break;
    }
}

/* ---- the sequence, spec 2.2-2.3 -------------------------------------- */

static void post_next_seq(SeqPlayer *p)
{
    uint32_t ticks;

    if (p->state != SEQ_PLAYING || !p->have_seq)
        return;
    if (!reader_next_delta(&p->seq, &ticks))
        return;
    post_simple(p, EV_SEQREF, -1, 0, 0, 0,
                (int32_t)(ticks * (uint32_t)p->uspt));
}

/* spec 2.3: a new tempo re-times the queued note-offs */
static void set_tempo(SeqPlayer *p, uint32_t tempo)
{
    int32_t old = p->uspt;
    struct { uint8_t chan, key; int32_t at; } found[SEQ_EVENTS];
    int n = 0, i;
    int16_t it = p->ev_head, prev = -1;
    int32_t t = 0;

    p->uspt = (int32_t)(tempo / p->seq.division);
    while (it >= 0)
    {
        int16_t next = p->ev[it].next;

        t += p->ev[it].delta;
        if (p->ev[it].type == EV_NOTEOFF)
        {
            found[n].chan = p->ev[it].chan;
            found[n].key = p->ev[it].key;
            found[n].at = t;
            n++;
            /* its delta moves to the next item, so it leaves the sum */
            t -= p->ev[it].delta;
            unlink_at(p, prev, it);
        }
        else
        {
            prev = it;
        }
        it = next;
    }
    /* the first found, then the rest last to first */
    for (i = 0; i < n; i++)
    {
        int k = i == 0 ? 0 : n - i;

        post_simple(p, EV_NOTEOFF, -1, found[k].chan, found[k].key, 0,
                    (found[k].at / old) * p->uspt);
    }
}

static void next_seq_event(SeqPlayer *p)
{
    SeqMsg m;

    if (!p->have_seq)
        return;
    reader_next(&p->seq, &m);
    switch (m.kind)
    {
    case SEV_MIDI:
        midi(p, &m);
        post_next_seq(p);
        break;
    case SEV_TEMPO:
        set_tempo(p, m.value);
        post_next_seq(p);
        break;
    case SEV_END:
        p->state = SEQ_STOPPING;
        post_simple(p, EV_STOP, -1, 0, 0, 0, EVQ_END);
        break;
    default:
        post_next_seq(p);
        break;
    }
}

/* ---- the handler, spec 2.4 / 3.10 / 3.12 ----------------------------- */

/* spec 3.12: does the voice have a note end due within KILL_US? A later
 * one is taken out of the queue. */
static int needs_kill(SeqPlayer *p, int vi)
{
    int16_t it = p->ev_head, prev = -1;
    int32_t t = 0;

    while (it >= 0)
    {
        t += p->ev[it].delta;
        if (p->ev[it].type == EV_NOTEEND && p->ev[it].voice == vi)
        {
            if (t > KILL_US)
            {
                unlink_at(p, prev, it);
                return 1;
            }
            return 0;
        }
        prev = it;
        it = p->ev[it].next;
    }
    return 1;
}

static void handle(SeqPlayer *p, const SeqEvent *e)
{
    int8_t vi;
    int ch;

    switch (e->type)
    {
    case EV_SEQREF:
        next_seq_event(p);
        break;
    case EV_API:
        post_simple(p, EV_API, -1, 0, 0, 0, FRAME_US);
        break;
    case EV_NOTEOFF:
        note_off(p, e->chan, e->key);
        break;
    case EV_NOTEEND:
        p->syn->stop(p->ctx, e->voice);
        voice_free(p, e->voice);
        break;
    case EV_ENV:
    {
        SeqVoice *v = &p->voice[e->voice];
        const BGMSound *s = sound_of(p, v);

        if (v->phase == PH_ATTACK)
            v->phase = PH_DECAY;
        v->env_gain = s->decayVolume;
        v->env_end = p->cur_time + s->decayTime;
        p->syn->vol(p->ctx, e->voice, voice_vol(p, v), s->decayTime);
        break;
    }
    case EV_OSC:
    {
        SeqVoice *v = &p->voice[e->voice];
        SeqOscState *o;

        if (v->osc[e->value] < 0)
            break;
        o = &p->osc[(int)v->osc[e->value]];
        if (++o->count >= o->len)
            o->count = 0;
        if (e->value)
        {
            v->vib = (uint32_t)o->shape->values[o->count];
            p->syn->pitch(p->ctx, e->voice, voice_ratio(p, v));
        }
        else
        {
            v->trem = (uint8_t)o->shape->values[o->count];
            p->syn->vol(p->ctx, e->voice, voice_vol(p, v), remaining(p, v));
        }
        post_simple(p, EV_OSC, e->voice, 0, 0, e->value,
                    o->shape->interval_us);
        break;
    }
    case EV_VOLUME:
        p->vol = (int16_t)e->value;
        for (vi = p->v_alloc; vi >= 0; vi = p->voice[(int)vi].next)
            p->syn->vol(p->ctx, vi, voice_vol(p, &p->voice[(int)vi]),
                        remaining(p, &p->voice[(int)vi]));
        break;
    case EV_PRIORITY:
        p->chan[e->chan].priority = (uint8_t)e->value;
        break;
    case EV_FXMIX:
        control(p, e->chan, 91, (uint8_t)e->value);
        break;
    case EV_PLAY:
        p->master_vol = 100;
        if (p->state != SEQ_PLAYING)
        {
            p->state = SEQ_PLAYING;
            post_next_seq(p);
        }
        break;
    case EV_STOPREQ:
        if (p->state != SEQ_PLAYING)
            break;
        flush(p, EV_SEQREF, -1, -1);
        flush(p, EV_NOTEOFF, -1, -1);
        flush(p, EV_FXMIX, -1, -1);
        for (vi = p->v_alloc; vi >= 0; vi = p->voice[(int)vi].next)
            if (needs_kill(p, vi))
                voice_release(p, vi, KILL_US);
        p->state = SEQ_STOPPING;
        post_simple(p, EV_STOP, -1, 0, 0, 0, EVQ_END);
        break;
    case EV_STOP:
        if (p->state != SEQ_STOPPING)
            break;
        while ((vi = p->v_alloc) >= 0)
        {
            p->syn->stop(p->ctx, vi);
            flush(p, EV_ENV, vi, -1);
            flush(p, EV_NOTEEND, vi, -1);
            voice_free(p, vi);
        }
        for (ch = 0; ch < SEQ_CHANNELS; ch++)
        {
            p->chan[ch].inst = NULL;
            p->chan[ch].vib_scale = 0;
            chan_perf_reset(&p->chan[ch]);
        }
        p->state = SEQ_STOPPED;
        break;
    case EV_SETSEQ:
        if (!p->next_data)
            break;
        reader_open(&p->seq, p->next_data);
        p->next_data = NULL;
        p->have_seq = 1;
        p->uspt = (int32_t)(500000u / p->seq.division);
        chans_from_bank(p);
        break;
    default:
        break;
    }
}

/* One call of the player: handle the pending event and everything due at
 * the same time, then return the microseconds to the next. */
static int32_t handler(SeqPlayer *p)
{
    SeqEvent e = p->pending;

    handle(p, &e);
    for (;;)
    {
        int16_t it = p->ev_head;
        int32_t d;

        if (it < 0)
        {
            /* cannot happen while the API tick runs; keep it so */
            post_simple(p, EV_API, -1, 0, 0, 0, FRAME_US);
            continue;
        }
        e = p->ev[it];
        d = e.delta;
        p->ev_head = e.next;
        p->ev[it].next = p->ev_free;
        p->ev_free = it;
        if (d == 0)
        {
            handle(p, &e);
            continue;
        }
        p->pending = e;
        p->cur_time += d;
        return d;
    }
}

/* ---- the public calls ------------------------------------------------ */

void seq_init(SeqPlayer *p, const SeqBank *bank, const SeqSynth *syn,
              void *ctx)
{
    int i;

    for (i = 0; i < (int)sizeof(*p); i++)
        ((uint8_t *)p)[i] = 0;
    p->bank = bank;
    p->syn = syn;
    p->ctx = ctx;
    for (i = 0; i < SEQ_EVENTS; i++)
        p->ev[i].next = (int16_t)(i + 1 < SEQ_EVENTS ? i + 1 : -1);
    p->ev_head = -1;
    p->ev_free = 0;
    /* voice states are handed out from the top, as the N64's list does */
    p->v_alloc = -1;
    for (i = 0; i < SEQ_VOICES; i++)
        p->voice[i].next = (int8_t)(i - 1);
    p->v_free = SEQ_VOICES - 1;
    for (i = 0; i < SEQ_OSCS; i++)
        p->osc[i].next = (int8_t)(i + 1 < SEQ_OSCS ? i + 1 : -1);
    p->osc_free = 0;
    p->pending.type = EV_API;
    p->uspt = 488;
    p->vol = 0x7FFF;
    p->master_vol = 100;
    p->state = SEQ_STOPPED;
    p->cur_time = p->call_at = p->now = 0;
}

void seq_set_sequence(SeqPlayer *p, uint8_t *data)
{
    p->next_data = data;
    post_simple(p, EV_SETSEQ, -1, 0, 0, 0, 0);
}

void seq_play(SeqPlayer *p)
{
    post_simple(p, EV_PLAY, -1, 0, 0, 0, 0);
}

void seq_stop(SeqPlayer *p)
{
    post_simple(p, EV_STOPREQ, -1, 0, 0, 0, 0);
}

void seq_set_volume(SeqPlayer *p, int16_t vol)
{
    post_simple(p, EV_VOLUME, -1, 0, 0, vol, 0);
}

void seq_set_chan_priority(SeqPlayer *p, int chan, int priority)
{
    post_simple(p, EV_PRIORITY, -1, chan, 0, priority, 0);
}

void seq_set_chan_fxmix(SeqPlayer *p, int chan, int fxmix)
{
    post_simple(p, EV_FXMIX, -1, chan, 0, fxmix, 0);
}

void seq_advance(SeqPlayer *p, int32_t us)
{
    p->now += us;
    while (p->call_at <= p->now)
        p->call_at += handler(p);
}
