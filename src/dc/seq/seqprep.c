/* seqprep.c -- see seqprep.h.
 * the float expressions are sys/audio.c's (syAudioDepth2Cents,
 * syAudioInitOsc, syAudioUpdateOsc), evaluated in single precision as the
 * N64 does, through libultra's own __sinf (tools/export/ssb_trigexport.py),
 * so a value that truncates to whole cents truncates where it does there. */
#include "seqprep.h"

/* libultra's sine, the one the N64's sinf is an alias of */
extern float __sinf(float x);

#define DTOR32 ((float)(3.14159265358979323846 / 180))

/* spec 8.2: 1.03099303^depth by repeated squaring, in f32 */
static float depth_cents(uint8_t depth)
{
    float x = 1.03099303f, cents = 1.0f;

    while (depth)
    {
        if (depth & 1)
            cents *= x;
        x *= x;
        depth >>= 1;
    }
    return cents;
}

static int prep_one(uint8_t type, uint8_t rate, uint8_t depth, uint8_t delay,
                    SeqOsc *o, int32_t *v, size_t room)
{
    int max, k, len;

    o->type = type;
    o->count0 = 0;
    o->pad = 0;
    o->delay_us = (int32_t)delay * 0x4000;
    o->interval_us = 16000;
    o->values = v;
    switch (type)
    {
    case 1:                     /* tremolo sine */
    {
        int half = depth >> 1, base = 127 - half;

        max = 259 - rate;
        len = max;
        if ((size_t)len > room)
            return -1;
        o->start = base;
        for (k = 0; k < len; k++)
        {
            float f = (float)k / (float)max;

            f = __sinf(f * (float)(360.0f * DTOR32)) * (float)half;
            v[k] = (uint8_t)(int32_t)((float)base + f);
        }
        break;
    }
    case 2:                     /* tremolo square: high first, then low */
        max = 256 - rate;
        len = 2;
        if ((size_t)len > room)
            return -1;
        o->start = 127;
        v[0] = 127;
        v[1] = 127 - depth;
        o->interval_us = 16000 * max;
        break;
    case 3:                     /* tremolo descending saw */
    case 4:                     /* tremolo ascending saw */
    {
        int base = type == 3 ? 127 : 127 - depth;

        max = 256 - rate;
        len = max + 1;
        if ((size_t)len > room)
            return -1;
        o->start = base;
        for (k = 0; k < len; k++)
        {
            float f = (float)k / (float)max * (float)depth;

            v[k] = (uint8_t)(int32_t)(type == 3 ? (float)base - f
                                                : (float)base + f);
        }
        break;
    }
    case 128:                   /* vibrato sine */
    {
        float dc = depth_cents(depth);

        max = 259 - rate;
        len = max;
        if ((size_t)len > room)
            return -1;
        o->start = (int32_t)SEQ_RATIO_ONE;
        for (k = 0; k < len; k++)
        {
            float f = (float)k / (float)max;

            f = __sinf(f * (float)(360.0f * DTOR32)) * dc;
            v[k] = (int32_t)seq_cents_ratio((int32_t)f);
        }
        break;
    }
    case 129:                   /* vibrato square: starts high, low first */
    {
        int32_t c = (int32_t)depth_cents(depth);

        max = 256 - rate;
        len = 2;
        if ((size_t)len > room)
            return -1;
        v[0] = (int32_t)seq_cents_ratio(c);
        v[1] = (int32_t)seq_cents_ratio(-c);
        o->start = v[0];
        o->interval_us = 16000 * max;
        break;
    }
    case 130:                   /* vibrato descending saw */
    case 131:                   /* vibrato ascending saw */
    {
        int32_t c = (int32_t)depth_cents(depth);

        max = 256 - rate;
        len = max + 1;
        if ((size_t)len > room)
            return -1;
        /* the vibrato saws start their counter at the top */
        o->count0 = (uint8_t)max;
        o->start = (int32_t)seq_cents_ratio(type == 130 ? c : -c);
        for (k = 0; k < len; k++)
        {
            float f = (float)k / (float)max * (float)(2 * c);

            f = type == 130 ? (float)c - f : f + (float)-c;
            v[k] = (int32_t)seq_cents_ratio((int32_t)f);
        }
        break;
    }
    default:
        o->type = 0;
        o->len = 0;
        return 0;
    }
    o->len = (uint8_t)len;
    return len;
}

int seq_prep_osc(const BGMInstrument *insts, uint32_t n_insts, SeqOsc *osc,
                 int32_t *values, size_t cap)
{
    size_t used = 0;
    uint32_t i;

    for (i = 0; i < n_insts; i++)
    {
        const BGMInstrument *in = &insts[i];
        int n;

        n = in->valid ? prep_one(in->tremType, in->tremRate, in->tremDepth,
                                 in->tremDelay, &osc[2 * i], values + used,
                                 cap - used)
                      : prep_one(0, 0, 0, 0, &osc[2 * i], values, 0);
        if (n < 0)
            return -1;
        used += (size_t)n;
        n = in->valid ? prep_one(in->vibType, in->vibRate, in->vibDepth,
                                 in->vibDelay, &osc[2 * i + 1], values + used,
                                 cap - used)
                      : prep_one(0, 0, 0, 0, &osc[2 * i + 1], values, 0);
        if (n < 0)
            return -1;
        used += (size_t)n;
    }
    return (int)used;
}
