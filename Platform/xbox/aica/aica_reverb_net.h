/* aica_reverb_net.h -- the N64's reverb, run as a network (one sample).
 *
 * Pure C with no Xbox headers, so the mixer (aica_mixer_xbox.c) and the host
 * test (tools/xbox/hosttest/test_decode.c, which holds it to
 * tools/check/reverb_check.py's run_net44, the port's own float model of the
 * same network) compile the same code.
 *
 * The chains are tools/export/ssb_reverbexport.py's, generated into
 * Platform/xbox/gen/xbox_reverb.h. The delay line is addressed as the AICA
 * DSP addresses its ring -- tap + a counter that falls by one a sample -- so a
 * value written at tap a is read at tap b, b - a samples later; every read
 * in a sample sees the line as it was before that sample's writes. */
#ifndef XBOX_AICA_REVERB_NET_H
#define XBOX_AICA_REVERB_NET_H

#include <stdint.h>
#include "../gen/xbox_reverb.h"

typedef struct XbReverb
{
    float ring[XB_RV_RING];
    uint32_t ct;
    float state;                /* the low-pass's last output */
} XbReverb;

static inline float xb_reverb_sample(XbReverb *r, float in)
{
    float temp[XB_RV_TEMPS + 1];
    float out = 0.0f;
    struct { uint32_t at; float v; } writes[XB_RV_CHAINS];
    int nw = 0, c, t;

    for (c = 0; c < XB_RV_CHAINS; c++)
    {
        const XbRvChain *ch = &kXbRvChains[c];
        float acc = 0.0f;

        for (t = 0; t < ch->count; t++)
        {
            const XbRvTerm *tm = &kXbRvTerms[ch->first + t];
            float x;

            switch (tm->kind)
            {
                case XB_RV_MIX:  x = in; break;
                case XB_RV_MEM:  x = r->ring[(tm->arg + r->ct) & (XB_RV_RING - 1)]; break;
                case XB_RV_TEMP: x = temp[tm->arg]; break;
                default:         x = r->state; break;
            }
            acc += tm->coef * x;
        }
        if (ch->temp_dest >= 0)
            temp[ch->temp_dest] = acc;
        if (ch->mem_dest >= 0)
        {
            writes[nw].at = (ch->mem_dest + r->ct) & (XB_RV_RING - 1);
            writes[nw].v = acc;
            nw++;
        }
        if (ch->state_dest)
            r->state = acc;
        if (ch->ef)
            out = acc;
    }
    /* the line as it was before this sample's writes was what every read
     * saw; now the writes land, and the line moves on */
    for (c = 0; c < nw; c++)
        r->ring[writes[c].at] = writes[c].v;
    r->ct = (r->ct - 1) & (XB_RV_RING - 1);
    /* denormals would cost far more than they are worth as the tail dies */
    if (out > -1e-12f && out < 1e-12f)
        out = 0.0f;
    return out;
}

#endif /* XBOX_AICA_REVERB_NET_H */
