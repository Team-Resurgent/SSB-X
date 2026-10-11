/* lbpdraw.c -- see lbpdraw.h. The model behind the eighteen macros, and
 * the one of them that draws.
 *
 * Nothing here is the game's. It is the divergence lbpdraw.h describes,
 * kept out of src/dc/lbparticle.c so that that file can be the decomp's
 * to the character.
 */
#include <math.h>
#include <string.h>

#include <ssb_types.h>
#include <sys/obj.h>
#include <lb/library.h>
#include <sys/taskman.h>

#include "lbcommon.h"           /* LB_SPRITE_SCREEN_SCALE */
#include "lbpartex.h"
#include "lbpdraw.h"

#ifndef FT_HOSTTEST
#include <dc/pvr.h>
#include "objpvr.h"
#endif
#include "perf.h"

/* Where a particle sits in the translucent list when the render mode
 * does not compare Z -- which is three of ef/efdisplay.c's four procs,
 * and is the RDP saying "over whatever is already there". The PVR sorts
 * that list by depth, so "already there" has to become a number: one
 * above every 1/w the scene's geometry writes (every camera's near
 * plane is 100 units, sys/objman.c dGCPerspDefault, so no fighter's
 * invw exceeds 0.01) and below the HUD's sprites, which start at
 * LB_SPRITE_Z_BASE = 1.0 (lbcommon.h). The step is the rectangle
 * counter, so the order within a frame is the order the renderer
 * emitted them -- the RDP's own. */
#define LBPDRAW_Z_BASE  0.5F
#define LBPDRAW_Z_STEP  (1.0F / 4096.0F)

LBPDrawRect gLBPDrawLog[LBPDRAW_LOG_MAX];
s32 gLBPDrawLogCount;
sb32 gLBPDrawLogging;

/* The modelled RDP. Everything the eighteen macros set, standing until
 * the next one sets it -- which is what the RDP does and what the
 * renderer counts on: it emits a texture load or a TLUT only when the
 * one it wants is not the one already there. */
static LBPDrawRect sLBPDraw;
static s32 sLBPDrawCount;
static s32 sLBPDrawQuads;
static u32 sLBPDrawFrame = (u32)-1;

/* The counter runs for the frame, not the pass -- lb/lbcommon.c's sprite
 * depth counter is the same idea and src/dc/lbcommon.c says why. Four
 * display procs draw particles and the frame calls each of them once per
 * PVR list, so nothing here can know it is the first. */
void lbpDrawResetFrame(void)
{
    sLBPDrawCount = 0;
    sLBPDrawQuads = 0;
    sLBPDrawFrame = dSYTaskmanFrameCount;
}

s32 lbpDrawRectCount(void)
{
    return sLBPDrawCount;
}

s32 lbpDrawQuadCount(void)
{
    return sLBPDrawQuads;
}

void lbpDrawSetRenderMode(u32 c0)
{
    sLBPDraw.zcmp = (c0 & Z_CMP) ? TRUE : FALSE;
}

void lbpDrawSetColorDither(u32 mode)
{
    sLBPDraw.dither_c = (u8)mode;
}

void lbpDrawSetAlphaDither(u32 mode)
{
    sLBPDraw.dither_a = (u8)mode;
}

void lbpDrawSetTextureLUT(u32 type)
{
    sLBPDraw.tlut = (s32)type;
}

void lbpDrawLoadTLUT(const void *palette)
{
    sLBPDraw.palette = palette;
}

void lbpDrawLoadTexture(const void *image, s32 fmt, s32 siz, s32 width,
                        s32 height, s32 cms, s32 cmt, s32 masks, s32 maskt)
{
    sLBPDraw.image = image;
    sLBPDraw.fmt = fmt;
    sLBPDraw.siz = siz;
    sLBPDraw.width = width;
    sLBPDraw.height = height;
    sLBPDraw.cms = cms;
    sLBPDraw.cmt = cmt;
    sLBPDraw.masks = masks;
    sLBPDraw.maskt = maskt;
}

void lbpDrawSetPrimColor(u32 r, u32 g, u32 b, u32 a)
{
    sLBPDraw.prim[0] = (u8)r;
    sLBPDraw.prim[1] = (u8)g;
    sLBPDraw.prim[2] = (u8)b;
    sLBPDraw.prim[3] = (u8)a;
}

void lbpDrawSetEnvColor(u32 r, u32 g, u32 b, u32 a)
{
    sLBPDraw.env[0] = (u8)r;
    sLBPDraw.env[1] = (u8)g;
    sLBPDraw.env[2] = (u8)b;
    sLBPDraw.env[3] = (u8)a;
}

void lbpDrawSetCombine(s32 which)
{
    sLBPDraw.combine = which;
}

/* The two combiners the renderer writes out in full, told apart by the
 * RGB cycle's four sources. Anything else would be a fifth combiner in a
 * function that has three, and is worth knowing about. */
void lbpDrawSetCombineLERP(s32 a, s32 b, s32 c, s32 d)
{
    if (a == G_CCMUX_NOISE)
    {
        sLBPDraw.combine = nLBPDrawCombineNoise;
    }
    else if (a == G_CCMUX_PRIMITIVE && b == G_CCMUX_ENVIRONMENT &&
             c == G_CCMUX_TEXEL0 && d == G_CCMUX_ENVIRONMENT)
    {
        sLBPDraw.combine = nLBPDrawCombineEnvLerp;
    }
    else
    {
        sLBPDraw.combine = nLBPDrawCombineModulateIAPrim;
    }
}

void lbpDrawSetBlendColor(u32 r, u32 g, u32 b, u32 a)
{
    (void)r;
    (void)g;
    (void)b;
    sLBPDraw.blend_alpha = (u8)a;
}

void lbpDrawSetAlphaCompare(u32 type)
{
    sLBPDraw.ac = (s32)type;
}

void lbpDrawSetPrimDepth(s32 z)
{
    sLBPDraw.z = z;
}

/* ---- the depth ------------------------------------------------------- */

/* gDPSetPrimDepth's argument back into the number the PVR sorts by.
 *
 * The RDP is handed a screen-space Z: the particle's clip z over its w,
 * put through the viewport's vscale[2]/vtrans[2] and scaled by 32. The
 * PVR is handed 1/w. Those are two encodings of one depth and the
 * projection is what converts between them, so this undoes the viewport
 * and then the perspective:
 *
 *   ndc = z / 32 / vscale2 - vtrans2 / vscale2
 *   1/w = (ndc * (near - far) + (near + far)) / (2 * near * far)
 *
 * -- which is syMatrixPerspF's third row solved for 1/w, and which the
 * `scale` argument drops out of, as it drops out of everything but the
 * RSP's fixed point. The camera is the one the renderer just read.
 *
 * A camera with no perspective XObj has no such row and no w to speak
 * of: an ortho projection puts every particle at the same 1/w, which
 * is the painter order below and is what the caller falls back to. */
static sb32 lbpDrawDepthOf(s32 z, f32 *out)
{
    CObj *cobj;
    f32 vscale2, vtrans2, ndc, near, far;
    s32 i;

    if (gGCCurrentCamera == NULL)
    {
        return FALSE;
    }
    cobj = CObjGetStruct(gGCCurrentCamera);

    for (i = 0; i < cobj->xobjs_num; i++)
    {
        if (cobj->xobjs[i] == NULL)
        {
            continue;
        }
        if (cobj->xobjs[i]->kind == nGCMatrixKindPerspFastF ||
            cobj->xobjs[i]->kind == nGCMatrixKindPerspF)
        {
            break;
        }
    }
    if (i == cobj->xobjs_num)
    {
        return FALSE;
    }
    vscale2 = (f32)cobj->viewport.vp.vscale[2];
    vtrans2 = (f32)cobj->viewport.vp.vtrans[2];

    if (vscale2 == 0.0F)
    {
        return FALSE;
    }
    near = cobj->projection.persp.near;
    far = cobj->projection.persp.far;

    if (near <= 0.0F || far <= near)
    {
        return FALSE;
    }
    ndc = ((f32)z * (1.0F / 32.0F) - vtrans2) / vscale2;

    *out = (ndc * (near - far) + (near + far)) / (2.0F * near * far);

    return TRUE;
}

/* ---- the rectangle --------------------------------------------------- */

#ifndef FT_HOSTTEST
/* The combiner as a pair of PVR vertex colours.
 *
 *   MODULATEIA_PRIM  rgb = TEXEL0 * PRIM, a = TEXEL0.a * PRIM.a, which
 *                    is PVR_TXRENV_MODULATEALPHA with PRIM as the base.
 *   EnvLerp          rgb = ENV + (PRIM - ENV) * TEXEL0, which is the
 *                    same modulate with ENV as the offset colour and
 *                    PRIM - ENV as the base -- src/dc/lbcommon.c's
 *                    nLBCommonCombineIAPrimEnv, the same identity for
 *                    the same reason, and the same clamp: the PVR's base
 *                    cannot go negative, so an ENV brighter than PRIM in
 *                    a channel flattens to ENV.
 *                    DIVERGES in alpha: the RDP lerps that too, and the
 *                    PVR has one alpha multiply. PRIM's is kept, which
 *                    is the RDP's answer where the texel is opaque.
 *   Noise            rgb = NOISE * TEXEL0. There is no noise source on
 *                    the PVR and there is no per-pixel random to build
 *                    one from without touching the game's own seed --
 *                    which would move every particle in the scene. So
 *                    the noise is its mean, 128, and the sparkle
 *                    becomes a steady half-bright texel.
 */
static void lbpDrawColorsOf(const LBPDrawRect *r, u32 *argb, u32 *oargb)
{
    const u8 *p = r->prim;
    const u8 *e = r->env;

    switch (r->combine)
    {
    case nLBPDrawCombineEnvLerp:
        {
            u32 cr = (p[0] > e[0]) ? p[0] - e[0] : 0;
            u32 cg = (p[1] > e[1]) ? p[1] - e[1] : 0;
            u32 cb = (p[2] > e[2]) ? p[2] - e[2] : 0;

            *argb = ((u32)p[3] << 24) | (cr << 16) | (cg << 8) | cb;
            *oargb = ((u32)e[0] << 16) | ((u32)e[1] << 8) | e[2];
        }
        break;

    case nLBPDrawCombineNoise:
        *argb = ((u32)p[3] << 24) | 0x808080;
        *oargb = 0;
        break;

    default:
        *argb = ((u32)p[3] << 24) | ((u32)p[0] << 16) | ((u32)p[1] << 8) | p[2];
        *oargb = 0;
        break;
    }
}

static s32 lbpDrawSubmit(const LBPDrawRect *r, f32 z)
{
    const LBPTex *tex = lbpTexForImage(r->image);
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t v;
    f32 u0, v0, u1, v1;
    u32 argb, oargb;
    s32 i;
    /* G_TX_MIRROR on an axis whose mask names the tile's own power of
     * two is the PVR's uv_flip: the second repeat is the image
     * backwards. The mask is zero when the width is not a power of two
     * (lb/lbparticle.c:1840's switch has no case for 48), and the RDP
     * ignores the wrap bits when it is -- so that is a clamp on both
     * machines. It is also the one case where the PVR texture is wider
     * than the image it holds (tools/export/ssb_particletexexport.py pads to
     * the next power of two), and so the one case where a mirror would
     * turn at the wrong texel. */
    sb32 mirror_s = (r->cms == G_TX_MIRROR) && (r->masks != 0);
    sb32 mirror_t = (r->cmt == G_TX_MIRROR) && (r->maskt != 0);

    if (tex == NULL || tex->txr == NULL)
    {
        return 0;
    }
    if (tex->w != tex->src_w)
    {
        mirror_s = FALSE;
    }
    if (tex->h != tex->src_h)
    {
        mirror_t = FALSE;
    }
    u0 = (f32)r->s * (1.0F / 32.0F);
    v0 = (f32)r->t * (1.0F / 32.0F);
    u1 = u0 + (r->xh - r->xl) * (f32)r->dsdx * (1.0F / 4096.0F);
    v1 = v0 + (r->yh - r->yl) * (f32)r->dtdy * (1.0F / 4096.0F);

    u0 /= (f32)tex->w;
    u1 /= (f32)tex->w;
    v0 /= (f32)tex->h;
    v1 /= (f32)tex->h;

    lbpDrawColorsOf(r, &argb, &oargb);

    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, tex->fmt, tex->w, tex->h,
                     tex->txr, PVR_FILTER_BILINEAR);
    cxt.gen.specular = PVR_SPECULAR_ENABLE;
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.txr.env = PVR_TXRENV_MODULATEALPHA;
    cxt.txr.uv_flip = mirror_s ? (mirror_t ? PVR_UVFLIP_UV : PVR_UVFLIP_U)
                               : (mirror_t ? PVR_UVFLIP_V : PVR_UVFLIP_NONE);
    cxt.txr.uv_clamp = mirror_s ? (mirror_t ? PVR_UVCLAMP_NONE : PVR_UVCLAMP_V)
                                : (mirror_t ? PVR_UVCLAMP_U : PVR_UVCLAMP_UV);
    cxt.blend.src = PVR_BLEND_SRCALPHA;
    cxt.blend.dst = PVR_BLEND_INVSRCALPHA;
    cxt.depth.comparison = r->zcmp ? PVR_DEPTHCMP_GEQUAL : PVR_DEPTHCMP_ALWAYS;
    cxt.depth.write = PVR_DEPTHWRITE_DISABLE;
    DBPERF_COMPILE();
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

    v.argb = argb;
    v.oargb = oargb;
    v.z = z;

    for (i = 0; i < 4; i++)
    {
        v.flags = (i == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        /* The RDP's 10.2 in the game's 320x240 onto the port's 640x480. */
        v.x = ((i & 2) ? r->xh : r->xl) * (0.25F * LB_SPRITE_SCREEN_SCALE);
        v.y = ((i & 1) ? r->yl : r->yh) * (0.25F * LB_SPRITE_SCREEN_SCALE);
        v.u = (i & 2) ? u1 : u0;
        v.v = (i & 1) ? v0 : v1;
        pvr_prim(&v, sizeof(v));
    }
    return 1;
}
#endif /* !FT_HOSTTEST */

void lbpDrawRect(f32 xl, f32 yl, f32 xh, f32 yh, s32 s, s32 t,
                 s32 dsdx, s32 dtdy)
{
    f32 z;

    sLBPDraw.xl = xl;
    sLBPDraw.yl = yl;
    sLBPDraw.xh = xh;
    sLBPDraw.yh = yh;
    sLBPDraw.s = s;
    sLBPDraw.t = t;
    sLBPDraw.dsdx = dsdx;
    sLBPDraw.dtdy = dtdy;

    if (sLBPDrawFrame != dSYTaskmanFrameCount)
    {
        lbpDrawResetFrame();
    }
    if (!sLBPDraw.zcmp || !lbpDrawDepthOf(sLBPDraw.z, &z))
    {
        z = LBPDRAW_Z_BASE + (f32)sLBPDrawCount * LBPDRAW_Z_STEP;
    }
    sLBPDrawCount++;
    sLBPDraw.depth = z;

    if (gLBPDrawLogging && gLBPDrawLogCount < LBPDRAW_LOG_MAX)
    {
        gLBPDrawLog[gLBPDrawLogCount++] = sLBPDraw;
    }
#ifndef FT_HOSTTEST
    sLBPDrawQuads += lbpDrawSubmit(&sLBPDraw, z);
#endif
}
