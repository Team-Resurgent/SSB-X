/* ftshade.h -- the arithmetic the fighter core lights a vertex by, and
 * the direction a scene's two light angles become. Header-only so that
 * tools/check/shade_oracle.c runs the same lines src/dc/fighter.c and
 * src/dc/objmodel.c do, without either file's PVR and DObj dependencies. */
#ifndef SSB_DC_FTSHADE_H
#define SSB_DC_FTSHADE_H

#include <stdint.h>

/* lb/lbcommon.c:321,340 -- the game's 4096-step table trigonometry,
 * declared here rather than through lbcommon.h so this header needs no
 * decomp include path. src/dc/lbcommon.c defines both. */
float lbCommonSin(float angle);
float lbCommonCos(float angle);

/* An RGBA8888 word's three colour bytes as 0..1 floats: the form
 * material_of (fighter.c) holds PRIM and the two light colours in. */
static inline void ft_shade_unpack(uint32_t rgba, float *out)
{
    out[0] = (float)((rgba >> 24) & 0xFF) / 255.0f;
    out[1] = (float)((rgba >> 16) & 0xFF) / 255.0f;
    out[2] = (float)((rgba >> 8) & 0xFF) / 255.0f;
}

static inline uint32_t ft_shade_chan(float f)
{
    int c = (int)(f * 255.0f + 0.5f);
    if (c < 0)
        c = 0;
    if (c > 255)
        c = 255;
    return (uint32_t)c;
}

/* One lit vertex: PRIM x SHADE, as ARGB8888.
 *
 * The RSP computes SHADE as ambient + diffuse * N.L and clamps it to 8
 * bits per channel *before* the RDP's combiner multiplies PRIM by it, so
 * the clamp comes first here too. Multiplying first and clamping the
 * product would let a channel keep growing past the point the N64 stops
 * it, and since each channel stops at a different N.L the hue slides:
 * Mario's skin (prim FFE199, light2 8C6666) came out yellow instead of
 * peach that way. base, amb and dif are the three colours 0..1 per
 * channel, d is N.L, alpha the byte to carry. */
static inline uint32_t ft_shade_lit(const float *base, const float *amb,
                                    const float *dif, float d,
                                    uint32_t alpha)
{
    float s[3];
    int c;

    if (d < 0.0f)
        d = 0.0f;
    for (c = 0; c < 3; c++)
    {
        s[c] = amb[c] + dif[c] * d;
        if (s[c] > 1.0f)
            s[c] = 1.0f;
    }
    return (alpha << 24) | (ft_shade_chan(base[0] * s[0]) << 16) |
           (ft_shade_chan(base[1] * s[1]) << 8) |
           ft_shade_chan(base[2] * s[2]);
}

/* The same lighting in integers, for the per-vertex path. ft_shade_lit turns
 * three floats into three bytes through three float-to-int conversions and
 * six clamps a vertex; here the colours are 16-bit fractions built once a
 * batch (ft_shade_fx_prep) and a vertex costs one float-to-int, for N.L, and
 * integer multiplies. The order is unchanged: SHADE is clamped to 1.0
 * before PRIM multiplies it.
 *
 * Every shift is 16, because the SH-4 shifts by 1, 2, 8 and 16 only and any
 * other count is a chain of them: 1.0 is 65535, not 65536, so that
 * 65535 * 65535 still fits an unsigned word.
 *
 * Accuracy, measured over 60,000 random materials and N.L from -0.3 to 1:
 * a channel differs from ft_shade_lit's byte by at most 1, in 0.36% of
 * channels, and from the unrounded value by at most 0.52 of a byte (the
 * float code's own rounding is 0.50). shade_check.py holds both to the same
 * one-byte bound against the N64 model. N.L saturates at 1.0, which is
 * above any unit normal against the port's 0.787-length light. */
typedef struct
{
    uint32_t base[3], amb[3], dif[3];
} FtShadeFx;

static inline void ft_shade_fx_prep(FtShadeFx *o, const float *base,
                                    const float *amb, const float *dif)
{
    int c;

    for (c = 0; c < 3; c++)
    {
        o->base[c] = (uint32_t)(base[c] * 65535.0f + 0.5f);
        o->amb[c] = (uint32_t)(amb[c] * 65535.0f + 0.5f);
        o->dif[c] = (uint32_t)(dif[c] * 65535.0f + 0.5f);
    }
}

static inline uint32_t ft_shade_lit_fx(const FtShadeFx *m, float d,
                                       uint32_t alpha)
{
    uint32_t dq, s, ch[3];
    int c;

    if (d < 0.0f)
        d = 0.0f;
    dq = (d >= 1.0f) ? 65535u : (uint32_t)(d * 65535.0f);
    for (c = 0; c < 3; c++)
    {
        s = m->amb[c] + ((m->dif[c] * dq) >> 16);
        if (s > 65535u)
            s = 65535u;
        ch[c] = ((((m->base[c] * s) >> 16) * 255u) + 0x8000u) >> 16;
    }
    return (alpha << 24) | (ch[0] << 16) | (ch[1] << 8) | ch[2];
}

/* G_RM_FOG_PRIM_A, the blend a fighter's colanim fog (the respawn and
 * invincibility flashes, team shades) is drawn with: the pixel the
 * combiner made, lerped toward the fog colour by the fog's alpha. The PVR
 * pixel is texel * base + offset, so base and offset both take (1 - a)
 * and the offset gains fog * a. `fog` is RGBA8888, `offset` 0RGB. */
static inline void ft_shade_fog(float *base, uint32_t *offset, uint32_t fog)
{
    uint32_t fa = fog & 0xFF;
    uint32_t keep = 0xFF - fa;
    uint32_t oc[3];
    int c;

    for (c = 0; c < 3; c++)
    {
        base[c] *= (float)keep / 255.0f;
        oc[c] = ((((*offset >> (16 - 8 * c)) & 0xFF) * keep) +
                 (((fog >> (24 - 8 * c)) & 0xFF) * fa)) / 0xFF;
    }
    *offset = (oc[0] << 16) | (oc[1] << 8) | oc[2];
}

/* The PVR's `+ offset` for a polygon with no texture, where the hardware
 * applies none (the offset colour is part of the textured shading
 * instruction only): the vertex colour gains it here instead, each
 * channel saturating as the PVR's sum does. */
static inline uint32_t ft_shade_add_offset(uint32_t argb, uint32_t offset)
{
    uint32_t out = argb & 0xFF000000u;
    int c;

    for (c = 0; c < 3; c++)
    {
        uint32_t v = ((argb >> (16 - 8 * c)) & 0xFF) +
                     ((offset >> (16 - 8 * c)) & 0xFF);

        out |= ((v > 0xFF) ? 0xFF : v) << (16 - 8 * c);
    }
    return out;
}

/* ft/ftdisplaylights.c:10-27 ftDisplayLightsDrawReflect: the two angles
 * (degrees) a scene lights its fighters by, as the world-space direction
 * the RSP would have got. The game writes vec * 100 into the Light's
 * three s8 bytes and the RSP reads those on a 0x7F == 1.0 scale, so the
 * light the N64 actually shades by is vec * 100 / 127 -- a non-unit
 * vector, and the 0.787 it loses is part of every fighter's look.
 * F_CLC_DTOR32 (macros.h:42) is spelled out, in its order. */
static inline void ft_light_dir(float angle_x, float angle_y, float *out)
{
    float rx = (float)((angle_x * 3.1415927f) / 180.0f);
    float ry = (float)((angle_y * 3.1415927f) / 180.0f);
    float x, y, z;

    y = -lbCommonSin(-ry);
    z = lbCommonCos(-ry);
    x = lbCommonSin(rx) * z;
    z *= lbCommonCos(rx);

    out[0] = x * 100.0f / 127.0f;
    out[1] = y * 100.0f / 127.0f;
    out[2] = z * 100.0f / 127.0f;
}

#endif /* SSB_DC_FTSHADE_H */
