/* lbpdraw.h -- the RDP that lbParticleDrawTextures draws through.
 *
 * lb/lbparticle.c:1450-2118 is the game's particle renderer, and
 * src/dc/lbparticle.c now holds it character for character: the camera
 * matrix it builds for itself, the perspective divide, the frustum
 * reject, the screen rectangle it makes out of a particle's size and the
 * projection's two column magnitudes, the texture and palette it picks
 * out of the bank, the mirror switch tables, the combiner, the alpha
 * compare, the primitive depth. All of that is arithmetic and all of it
 * is the decomp's, checked line by line by tools/check/lbparticle_check.py.
 *
 * What is not arithmetic is the eighteen GBI macros the arithmetic ends
 * in. This file is those, and it is the whole divergence: the port
 * redefines each one to drive a small model of the RDP state the
 * renderer touches, and turns the one that draws --
 * gSPScisTextureRectangle -- into a PVR quad. src/dc/lbcommon.c does the
 * same thing for lb/lbcommon.c's sprite rectangles and the idea is that
 * one; a macro whose argument is a display-list head discards it, so
 * gSYTaskmanDLHeads never has to exist here (src/dc/ifcommon.c:53 on
 * why it does not).
 *
 * Define LBPDRAW_MODEL before including this to get the macro block.
 * Two files do: src/dc/lbparticle.c, for the renderer, and
 * src/dc/efdisplay.c, for the four display procs around it.
 *
 * What the model keeps and what it drops
 * --------------------------------------
 *   kept   the primitive and environment colours, which combiner of the
 *          three the particle asked for, the tile's cms/cmt/masks/maskt,
 *          the texture's fmt/siz/width/height and the address the RDP
 *          was pointed at, the TLUT, the primitive depth, and whether
 *          the render mode the display proc set compares Z.
 *   drops  the pipe syncs (the PVR has no pipeline to stall), the
 *          texture-persp and depth-source switches (both are what the
 *          renderer is: screen-space rectangles at a depth of their
 *          own), the colour and alpha dither modes, and the alpha
 *          compare with its blend colour. The last two are the RDP's
 *          answer to a 16-bit framebuffer and an 8-bit alpha it could
 *          not blend smoothly; the PVR blends the alpha it is given, so
 *          G_AC_DITHER and a G_AC_THRESHOLD at 8/255 are both a
 *          rounding of what the port already does. They are modelled --
 *          the fields are set and tools/check/lbparticle_check.py compares
 *          them against the display list the decomp's own build emits --
 *          and then not used.
 */
#ifndef SSB_DC_LBPDRAW_H
#define SSB_DC_LBPDRAW_H

#include <ssb_types.h>
#include <sys/obj.h>

/* The DL links ef/efdisplay.c:82-96 puts its four display GObjs on, less
 * link 25: that one is the effect camera's (gm/gmcamera.c:1502
 * gmCameraMakeEffectCamera) and there is no effect camera in the port
 * yet, so its GObj exists and nothing asks it to draw. The other three
 * are the battle camera's, which captures them in the fourth, fifth and
 * third of gmCameraDefaultProcDisplay's six passes -- and so after the
 * fighters and before the HUD, which is the order the port's single
 * ascending walk gives them too (src/dc/scvsbattle.c). */
#define EFDISPLAY_DLLINK_MASK \
    (COBJ_MASK_DLLINK(10) | COBJ_MASK_DLLINK(15) | COBJ_MASK_DLLINK(18))

/* Which of the three combiners lb/lbparticle.c:2043-2069 selects. */
enum
{
    nLBPDrawCombineModulateIAPrim,  /* G_CC_MODULATEIA_PRIM */
    nLBPDrawCombineEnvLerp,         /* lerp(ENV, PRIM, TEXEL0), both channels */
    nLBPDrawCombineNoise            /* NOISE * TEXEL0, alpha TEXEL0 * PRIM */
};

/* One rectangle, as the renderer asked for it: every argument of
 * gSPScisTextureRectangle together with the modelled state standing
 * behind it. This is what the PVR draws from and what the host build
 * logs, so that tools/check/lbparticle_check.py can hold each field to the
 * display list the decomp's own copy of the same function emits.
 *
 * The screen coordinates are the RDP's 10.2 fixed point in the game's
 * 320x240 and are NOT clamped at zero here, where libultra's macro
 * clamps them and walks s and t forward to match. The PVR clips the
 * quad against the framebuffer itself, so the port hands it the
 * rectangle the arithmetic produced and the picture is the same one;
 * the check applies the macro to these numbers before comparing. */
typedef struct LBPDrawRect
{
    f32 xl, yl, xh, yh;
    s32 s, t, dsdx, dtdy;
    s32 z;                      /* gDPSetPrimDepth's, (s32)(tz * 32) */
    const void *image;          /* the bank's own frame, LBTexture.data[] */
    const void *palette;        /* its TLUT, or NULL when fmt is not CI */
    s32 fmt, siz;
    s32 width, height;
    s32 cms, cmt;
    s32 masks, maskt;
    s32 combine;                /* nLBPDrawCombine* */
    s32 ac;                     /* G_AC_DITHER or G_AC_THRESHOLD */
    s32 tlut;                   /* G_TT_RGBA16 or G_TT_NONE */
    u8 prim[4], env[4];
    u8 blend_alpha;             /* gDPSetBlendColor's alpha */
    u8 dither_c, dither_a;
    u8 zcmp;                    /* the render mode compares Z */
    f32 depth;                  /* what the PVR sorted this quad by --
                                 * not the RDP's and not compared against
                                 * it, see lbpDrawRect */
} LBPDrawRect;

/* ---- the modelled state, one setter per macro ------------------------ */

void lbpDrawSetRenderMode(u32 c0);
void lbpDrawSetColorDither(u32 mode);
void lbpDrawSetAlphaDither(u32 mode);
void lbpDrawSetTextureLUT(u32 type);
void lbpDrawLoadTLUT(const void *palette);
void lbpDrawLoadTexture(const void *image, s32 fmt, s32 siz, s32 width,
                        s32 height, s32 cms, s32 cmt, s32 masks, s32 maskt);
void lbpDrawSetPrimColor(u32 r, u32 g, u32 b, u32 a);
void lbpDrawSetEnvColor(u32 r, u32 g, u32 b, u32 a);
void lbpDrawSetCombine(s32 which);
void lbpDrawSetCombineLERP(s32 a, s32 b, s32 c, s32 d);
void lbpDrawSetBlendColor(u32 r, u32 g, u32 b, u32 a);
void lbpDrawSetAlphaCompare(u32 type);
void lbpDrawSetPrimDepth(s32 z);
void lbpDrawRect(f32 xl, f32 yl, f32 xh, f32 yh, s32 s, s32 t,
                 s32 dsdx, s32 dtdy);

/* The frame's rectangle counter, which is also the painter order the
 * PVR's translucent list is given when the render mode does not compare
 * Z -- as it does not for three of ef/efdisplay.c's four procs. */
void lbpDrawResetFrame(void);
s32 lbpDrawRectCount(void);

/* How many of those found a texture in VRAM and became a quad. The two
 * differ only when a bank's .txp is not loaded, which is the one thing
 * about this path a target can tell you and a host cannot. */
s32 lbpDrawQuadCount(void);

/* The host build's log: every rectangle the last call to
 * lbParticleDrawTextures asked for, in order. Empty on the Dreamcast. */
#define LBPDRAW_LOG_MAX 4096
extern LBPDrawRect gLBPDrawLog[LBPDRAW_LOG_MAX];
extern s32 gLBPDrawLogCount;
extern sb32 gLBPDrawLogging;

/* ---- the macros ------------------------------------------------------ */
#ifdef LBPDRAW_MODEL

#undef gDPPipeSync
#undef gDPSetTexturePersp
#undef gDPSetDepthSource
#undef gDPSetColorDither
#undef gDPSetAlphaDither
#undef gDPSetTextureLUT
#undef gDPLoadTLUT_pal256
#undef gDPLoadTextureBlock
#undef gDPLoadTextureBlock_4b
#undef gDPSetPrimColor
#undef gDPSetEnvColor
#undef gDPSetCombineMode
#undef gDPSetCombineLERP
#undef gDPSetBlendColor
#undef gDPSetAlphaCompare
#undef gDPSetPrimDepth
#undef gDPSetRenderMode
#undef gSPScisTextureRectangle
#undef gSPClearGeometryMode
#undef gSPSetGeometryMode

/* The pipeline, the texture-persp switch and the depth source: the three
 * the PVR has no state for. */
#define gDPPipeSync(pkt)                        ((void)0)
#define gDPSetTexturePersp(pkt, type)           ((void)0)
#define gDPSetDepthSource(pkt, type)            ((void)0)
/* The RSP geometry mode, which ef/efdisplay.c's two non-particle procs
 * set and clear around the effect models the port does not draw yet. */
#define gSPClearGeometryMode(pkt, mode)         ((void)0)
#define gSPSetGeometryMode(pkt, mode)           ((void)0)

#define gDPSetRenderMode(pkt, c0, c1)           lbpDrawSetRenderMode((u32)(c0))
#define gDPSetColorDither(pkt, mode)            lbpDrawSetColorDither((u32)(mode))
#define gDPSetAlphaDither(pkt, mode)            lbpDrawSetAlphaDither((u32)(mode))
#define gDPSetTextureLUT(pkt, type)             lbpDrawSetTextureLUT((u32)(type))
#define gDPLoadTLUT_pal256(pkt, dram)           lbpDrawLoadTLUT((const void *)(dram))
#define gDPSetPrimColor(pkt, m, l, r, g, b, a)  lbpDrawSetPrimColor((r), (g), (b), (a))
#define gDPSetEnvColor(pkt, r, g, b, a)         lbpDrawSetEnvColor((r), (g), (b), (a))
#define gDPSetBlendColor(pkt, r, g, b, a)       lbpDrawSetBlendColor((r), (g), (b), (a))
#define gDPSetAlphaCompare(pkt, type)           lbpDrawSetAlphaCompare((u32)(type))
#define gDPSetPrimDepth(pkt, z, dz)             lbpDrawSetPrimDepth((s32)(z))

#define gDPLoadTextureBlock(pkt, image, fmt, siz, width, height, pal, \
                            cms, cmt, masks, maskt, shifts, shiftt)  \
    lbpDrawLoadTexture((image), (fmt), (siz), (width), (height),     \
                       (cms), (cmt), (masks), (maskt))

#define gDPLoadTextureBlock_4b(pkt, image, fmt, width, height, pal, \
                               cms, cmt, masks, maskt, shifts, shiftt) \
    lbpDrawLoadTexture((image), (fmt), G_IM_SIZ_4b, (width), (height), \
                       (cms), (cmt), (masks), (maskt))

/* Only one combine mode reaches this file and it is the default one. */
#define gDPSetCombineMode(pkt, a, b) \
    lbpDrawSetCombine(nLBPDrawCombineModulateIAPrim)

/* The two the renderer spells out. Only the RGB cycle-1 sources are
 * needed to tell them apart, and taking the token pastes is what makes
 * the call sites the decomp's own text. */
#define gDPSetCombineLERP(pkt, a0, b0, c0, d0, Aa0, Ab0, Ac0, Ad0, \
                               a1, b1, c1, d1, Aa1, Ab1, Ac1, Ad1) \
    lbpDrawSetCombineLERP(G_CCMUX_##a0, G_CCMUX_##b0, G_CCMUX_##c0, G_CCMUX_##d0)

#define gSPScisTextureRectangle(pkt, xl, yl, xh, yh, tile, s, t, dsdx, dtdy) \
    lbpDrawRect((xl), (yl), (xh), (yh), (s32)(s), (s32)(t), \
                (s32)(dsdx), (s32)(dtdy))

#endif /* LBPDRAW_MODEL */

#endif /* SSB_DC_LBPDRAW_H */
