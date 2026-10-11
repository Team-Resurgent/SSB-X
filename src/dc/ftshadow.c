/* ftshadow.c -- ft/ftshadow.c, all three of its functions, verbatim but
 * for what is marked DIVERGES.
 *
 * The blob under every fighter. It is not a model and not a sprite: the
 * display proc walks the floor line the fighter is standing over,
 * projects his x +- attr->shadow_size onto it, and builds four, six or
 * eight vertices by hand -- six when the shadow crosses one bend in the
 * floor, eight when it crosses two -- then issues its own display list
 * over them. So it is a renderer, and this file is the port's renderer
 * for it.
 *
 * The source: ftShadowGetAltitude 0x8013AE10, ftShadowProcDisplay
 * 0x8013AE60, ftShadowMakeShadow 0x8013BB88, and the two display lists
 * at 0x80188410 and 0x80188458.
 *
 * The arithmetic is the decomp's line for line -- the vertex positions,
 * the texture coordinates, the two 0..2048 clamps, the strip's index
 * order and which of the two orders a left-to-right floor takes. What
 * DIVERGES is the GBI it ends in, and it diverges the way src/dc/lbpdraw.h
 * does for lb/lbparticle.c: the macros are redefined below to drive a
 * small model of the RDP state this file touches, and the two that draw
 * -- gSPVertex and gSP2Triangles -- become PVR triangles. A macro whose
 * argument is a display-list head discards it, so gSYTaskmanDLHeads
 * never has to exist here.
 *
 * DIVERGES, and the list is the whole of it:
 *
 *   dFTShadowNoPrevLinkDL and dFTShadowNoNextLinkDL are gone. They are
 *   the state the first and last shadow on the DL link set and put back:
 *   the LUT off, G_RM_AA_XLU_SURF, G_CC_MODULATEIA_PRIM, the texture on
 *   at half scale, an alpha threshold of 0x0F, Z and lighting and smooth
 *   shading off; then Z, lighting, G_RM_AA_ZB_OPA_SURF and no alpha
 *   compare back. Every one of those is per-polygon on the PVR and lives
 *   in the poly header ftShadowSubmitHeader compiles, so there is no
 *   state to put back and the second display list has nothing to do. The
 *   alpha threshold is the one thing dropped rather than moved: it is the
 *   RDP's answer to a framebuffer it could not blend smoothly, and the
 *   PVR blends what it is given (src/dc/lbpdraw.h says the same of the
 *   particle renderer's).
 *
 *   The texture. gDPLoadTextureBlock_4b points the RDP at 128 bytes
 *   inside relocData file 84 every frame; the port uploads the same
 *   image to VRAM once, as romdisk/ftshadow.mdl
 *   (tools/export/ssb_shadowexport.py), and the header names it. Both axes were
 *   G_TX_MIRROR with mask 4 and gsSPTexture halved the coordinates, so the
 *   vertices' 0..2048 covers one mirrored 32-texel period either way; the PVR
 *   mirrors on its own second repeat (PVR_UVFLIP_UV), which makes the texture
 *   coordinate tc / 1024 over the 16x16 tile.
 *
 *   Z. The game draws the shadow with the Z buffer off, on DL link 7 --
 *   before the fighters on link 9 -- so the floor paints, then the
 *   shadow over it, then the fighter over that. A PVR translucent
 *   polygon is blended after the opaque lists are resolved, so painter
 *   order alone would put the shadow over the fighter's legs. The header
 *   therefore compares depth (and does not write it), which is the same
 *   picture by a different route: the fighter is nearer and wins. The
 *   shadow lies on the floor it is drawn over, so its 1/w is scaled by
 *   FTSHADOW_ZBIAS to keep it off the surface it is coplanar with.
 *
 *   gSYTaskmanTaskID is the graphics task being built, 0 or 1, and it
 *   picks which of FTShadow's two vertex arrays this frame writes -- the
 *   other is still being read by the RSP. The port hands its triangles
 *   to the PVR inside this call and keeps no display list, so there is
 *   one buffer in flight; the variable is defined here and stays 0, and
 *   vtx2 is kept because FTShadow is the game's struct and
 *   ftShadowMakeShadow initialises both halves.
 *
 *   ftShadowMakeShadow loads the pack the first time it is called and
 *   ftShadowOverlayLoad releases it, the pattern src/dc/ifcommon.c's
 *   magnifying glass uses. The game's texture comes with the effect
 *   file the scene already loaded.
 *
 * The model keeps a log of every draw -- the vertices as the arithmetic
 * built them, the triangles it indexed them with, and the primitive
 * colour -- so that hosttest_ft.c can hold them against the floor line
 * read back independently, and src/dc/db.c can print them on the target.
 */
#include "ftcommon.h"

#include <ft/ftcommondata.h>
#include <mp/mpcollision.h>
#include "overlay.h"
#include "ftshadow.h"
#include "objpvr.h"
#include "perf.h"
#include "clip.h"

#ifndef SSB_NO_DRAW
#include "dcpvr.h"
#include "fighter.h"
#endif

/* ---- the model ------------------------------------------------------- */

FTShadowDraw gFTShadowLog[FTSHADOW_LOG_MAX];
s32 gFTShadowLogCount;

/* The graphics task this frame is building. See DIVERGES above. */
s32 gSYTaskmanTaskID;

/* The pack, which is one 16x16 texture and no geometry. */
#ifndef SSB_NO_DRAW
#define FTSHADOW_MODEL "ftshadow.mdl"
#define FTSHADOW_TEX_W 16
#define FTSHADOW_TEX_H 16

/* 1/w is larger nearer, so a factor above one lifts the shadow off the
 * floor it lies on without moving it: one part in two thousand, which at
 * the battle camera's distance is under a millimetre of world depth and
 * still clears the floor mesh's own quantisation. */
#define FTSHADOW_ZBIAS 1.0005f

static Fighter sFTShadowPack;
static sb32 sFTShadowIsLoaded;
static sb32 sFTShadowHdrSent;
#endif

static FTShadowDraw *sFTShadowDraw;
static u32 sFTShadowFrame = (u32)-1;

/* gDPSetPrimColor: the colour the combiner multiplies the texel by, and
 * the start of one shadow's draw. */
static void ftShadowSetPrimColor(u32 r, u32 g, u32 b, u32 a)
{
    sFTShadowDraw = (gFTShadowLogCount < FTSHADOW_LOG_MAX)
                    ? &gFTShadowLog[gFTShadowLogCount++] : NULL;

    if (sFTShadowDraw != NULL)
    {
        sFTShadowDraw->prim[0] = r;
        sFTShadowDraw->prim[1] = g;
        sFTShadowDraw->prim[2] = b;
        sFTShadowDraw->prim[3] = a;
        sFTShadowDraw->vtx_num = 0;
        sFTShadowDraw->tri_num = 0;
    }
}

/* gSPVertex: the RSP's vertex cache, which here is the log's own copy --
 * the eight the arithmetic just wrote, at the addresses it wrote them. */
static void ftShadowVertex(const Vtx *v, s32 n)
{
    s32 i;

    if (sFTShadowDraw == NULL)
    {
        return;
    }
    if (n > (s32)ARRAY_COUNT(sFTShadowDraw->vtx))
    {
        n = ARRAY_COUNT(sFTShadowDraw->vtx);
    }
    for (i = 0; i < n; i++)
    {
        sFTShadowDraw->vtx[i] = v[i];
    }
    sFTShadowDraw->vtx_num = n;
}

#ifndef SSB_NO_DRAW
/* The poly header every shadow of the frame draws through: the render
 * mode, the combiner and the tile of dFTShadowNoPrevLinkDL, in the one
 * place the PVR keeps them. */
static void ftShadowSubmitHeader(void)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;

    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444,
                     FTSHADOW_TEX_W, FTSHADOW_TEX_H, sFTShadowPack.txr[0],
                     PVR_FILTER_BILINEAR);
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.txr.env = PVR_TXRENV_MODULATEALPHA;
    cxt.txr.uv_flip = PVR_UVFLIP_UV;
    cxt.txr.uv_clamp = PVR_UVCLAMP_NONE;
    cxt.blend.src = PVR_BLEND_SRCALPHA;
    cxt.blend.dst = PVR_BLEND_INVSRCALPHA;
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    cxt.depth.write = PVR_DEPTHWRITE_DISABLE;
    DBPERF_COMPILE();
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));
}

/* One Vtx through the camera the display walk installed, into clip
 * space. The vertices are in world coordinates: the shadow GObj has no
 * DObj and issues no matrix, so the RSP transformed them by whatever the
 * camera left on the stack, which is the view alone. */
static void ftShadowProject(const float *view, const float *proj,
                            const Vtx *v, ClipVtx *out, u32 argb)
{
    f32 x = v->n.ob[0], y = v->n.ob[1], z = v->n.ob[2];
    f32 vx = view[0] * x + view[1] * y + view[2] * z + view[3];
    f32 vy = view[4] * x + view[5] * y + view[6] * z + view[7];
    f32 vz = view[8] * x + view[9] * y + view[10] * z + view[11];

    out->cx = proj[0] * vx + proj[1] * vy + proj[2] * vz + proj[3];
    out->cy = proj[4] * vx + proj[5] * vy + proj[6] * vz + proj[7];
    out->cz = proj[8] * vx + proj[9] * vy + proj[10] * vz + proj[11];
    out->cw = proj[12] * vx + proj[13] * vy + proj[14] * vz + proj[15];
    out->u = (f32)v->n.tc[0] / 1024.0F;
    out->v = (f32)v->n.tc[1] / 1024.0F;
    out->argb = argb;
}

static void ftShadowEmit(const ClipVtx *tri)
{
    const DCViewport *vp = gcGetViewport();
    ClipVtx poly[4];
    pvr_vertex_t pv[3];
    s32 n, k, i;

    n = clip_near(tri, poly);

    for (k = 1; k + 1 < n; k++)
    {
        const ClipVtx *p[3];

        p[0] = &poly[0];
        p[1] = &poly[k];
        p[2] = &poly[k + 1];

        for (i = 0; i < 3; i++)
        {
            f32 invw = 1.0F / p[i]->cw;

            pv[i].flags = (i == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            pv[i].x = p[i]->cx * invw * vp->hw + vp->cx;
            pv[i].y = vp->cy - p[i]->cy * invw * vp->hh;
            pv[i].z = invw * FTSHADOW_ZBIAS;
            pv[i].u = p[i]->u;
            pv[i].v = p[i]->v;
            pv[i].argb = p[i]->argb;
            pv[i].oargb = 0;
        }
        pvr_prim(pv, sizeof(pv));
    }
}
#endif

/* gSP2Triangles: two triangles over the vertex cache, which is where
 * every shadow the game draws ends. */
static void ftShadow2Triangles(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f)
{
    const s32 idx[2][3] = { { a, b, c }, { d, e, f } };
    s32 t;

    if (sFTShadowDraw == NULL)
    {
        return;
    }
    for (t = 0; t < 2; t++)
    {
        if (sFTShadowDraw->tri_num < (s32)ARRAY_COUNT(sFTShadowDraw->tri))
        {
            u8 *row = sFTShadowDraw->tri[sFTShadowDraw->tri_num++];

            row[0] = idx[t][0];
            row[1] = idx[t][1];
            row[2] = idx[t][2];
        }
    }
#ifndef SSB_NO_DRAW
    {
        const float *view = gcGetViewF();
        const float *proj = gcGetProjF();
        u32 argb = ((u32)sFTShadowDraw->prim[3] << 24) |
                   ((u32)sFTShadowDraw->prim[0] << 16) |
                   ((u32)sFTShadowDraw->prim[1] << 8) |
                   (u32)sFTShadowDraw->prim[2];

        if (sFTShadowIsLoaded == FALSE)
        {
            return;
        }
        if (sFTShadowHdrSent == FALSE)
        {
            ftShadowSubmitHeader();
            sFTShadowHdrSent = TRUE;
        }
        for (t = 0; t < 2; t++)
        {
            ClipVtx tri[3];
            s32 k;

            for (k = 0; k < 3; k++)
            {
                ftShadowProject(view, proj,
                                &sFTShadowDraw->vtx[idx[t][k]], &tri[k], argb);
            }
            ftShadowEmit(tri);
        }
    }
#endif
}

/* The frame boundary the model needs: one poly header a frame, and a
 * fresh log. Nothing calls it -- the proc notices the frame changed, as
 * src/dc/lbpdraw.c's counter does and for the same reason: the frame
 * runs each camera once per PVR list and no display proc can know it is
 * the first. */
void ftShadowResetFrame(void)
{
    gFTShadowLogCount = 0;
    sFTShadowDraw = NULL;
    sFTShadowFrame = dSYTaskmanFrameCount;
#ifndef SSB_NO_DRAW
    sFTShadowHdrSent = FALSE;
#endif
}

/* ---- the macros, over ssb-decomp-re/include/PR/gbi.h ----------------- */

#undef gDPPipeSync
#undef gSPDisplayList
#undef gDPLoadTextureBlock_4b
#undef gDPSetPrimColor
#undef gSPVertex
#undef gSP2Triangles

/* The two display lists and the pipeline: see DIVERGES above. */
#define gDPPipeSync(pkt)                        ((void)0)
#define gSPDisplayList(pkt, dl)                 ((void)0)
#define gDPLoadTextureBlock_4b(pkt, image, fmt, width, height, pal, \
                               cms, cmt, masks, maskt, shifts, shiftt) \
    ((void)0)

#define gDPSetPrimColor(pkt, m, l, r, g, b, a) \
    ftShadowSetPrimColor((r), (g), (b), (a))
#define gSPVertex(pkt, v, n, v0)                ftShadowVertex((v), (n))
#define gSP2Triangles(pkt, a, b, c, f0, d, e, f, f1) \
    ftShadow2Triangles((a), (b), (c), (d), (e), (f))

/* ---- the game's own text --------------------------------------------- */

// 0x8013AE10
f32 ftShadowGetAltitude(Vec3f *a, Vec3f *b, f32 f)
{
    if (b->x == a->x)
    {
        return a->y;
    }
    else return (((f - a->x) * (b->y - a->y)) / (b->x - a->x)) + a->y;
}

/* the port's DB_PERF timing around it, attached in its place (perf.h) */
DBPERF_DEFINE_TIMED(DBP_SHADOW, ftShadowProcDisplay, GObj *)

// 0x8013AE60
void ftShadowProcDisplay(GObj *shadow_gobj)
{
    /* DIVERGES: the frame runs the battle camera once per PVR list and a
     * shadow is a translucent quad, so the other two passes have nothing
     * to do -- the guard ef/efdisplay.c's three procs carry, for the
     * same reason (src/dc/efdisplay.c:84). The frame's first shadow
     * clears the log behind it. */
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    if (sFTShadowFrame != dSYTaskmanFrameCount)
    {
        ftShadowResetFrame();
    }
    {
    FTShadow *fs;
    FTStruct *fp;
    f32 shadow_center;
    Vtx *shadow_vertex;
    f32 shadow_size;
    Vtx *sv;
    GObj *fighter_gobj;
    s32 i;
    s32 floor_line_id;
    Vec3f vertex_pos0;
    Vec3f vertex_pos1;
    f32 shadow_calc_left;
    f32 shadow_calc_right;
    s32 unused;
    f32 shadow_edge_left;
    f32 shadow_alt_left;
    f32 shadow_alt_right;
    f32 spF0;
    f32 spEC;
    f32 spE8;
    f32 spE4;
    f32 spE0;
    f32 spDC;
    s32 gfx_vertex_num;
    s32 coll_vertex_num;
    f32 shadow_edge_right;
    sb32 edge_left_or_right;

    if (shadow_gobj->dl_link_prev == NULL)
    {
        gSPDisplayList(gSYTaskmanDLHeads[0]++, dFTShadowNoPrevLinkDL);

       
        gDPLoadTextureBlock_4b(gSYTaskmanDLHeads[0]++, ((uintptr_t)gEFManagerFiles[1] + (intptr_t)&llEFCommonEffects2ShadowTextureImage), G_IM_FMT_I, 16, 16, 0, G_TX_MIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP, 4, 4, G_TX_NOLOD, G_TX_NOLOD);
    }
    fs = (FTShadow*)shadow_gobj->user_data.p;

    fighter_gobj = gSCManagerBattleState->players[fs->player].fighter_gobj;

    fp = ftGetStruct(fighter_gobj);

    if (!(fp->is_invisible) && !(fp->is_shadow_hide))
    {
        Vec3f ga_last;
        Vec3f pos_project;
        f32 ga_dist;

        if (fp->ga == nMPKineticsGround)
        {
            floor_line_id = fp->coll_data.floor_line_id;

            ga_dist = 0;
        }
        else if (mpCollisionCheckProjectFloor(&DObjGetStruct(fighter_gobj)->translate.vec.f, &floor_line_id, &ga_dist, NULL, NULL) == FALSE)
        {
            floor_line_id = -1;
        }
        if ((floor_line_id != -1) && (fp->coll_data.floor_line_id != -2))
        {
            pos_project.x = DObjGetStruct(fighter_gobj)->translate.vec.f.x;
            pos_project.y = DObjGetStruct(fighter_gobj)->translate.vec.f.y - 65536;

            if 
            (
                (
                    ((mpCollisionCheckLWallLineCollisionSame(&DObjGetStruct(fighter_gobj)->translate.vec.f, &pos_project, &ga_last, NULL, NULL, NULL) == FALSE)) ||
                    !((DObjGetStruct(fighter_gobj)->translate.vec.f.y - ga_last.y) < -ga_dist)
                ) 
                &&
                (
                    (mpCollisionCheckRWallLineCollisionSame(&DObjGetStruct(fighter_gobj)->translate.vec.f, &pos_project, &ga_last, NULL, NULL, NULL) == FALSE) ||
                    !((DObjGetStruct(fighter_gobj)->translate.vec.f.y - ga_last.y) < -ga_dist)
                )
            )
            {
                shadow_center = DObjGetStruct(fighter_gobj)->translate.vec.f.x;
                shadow_size = fp->attr->shadow_size;
                shadow_calc_left = 0.0F, shadow_calc_right = 1984.0F;

                mpCollisionGetFloorEdgeL(floor_line_id, &vertex_pos0);
                mpCollisionGetFloorEdgeR(floor_line_id, &vertex_pos1);

                shadow_edge_left = shadow_center - shadow_size;
                shadow_edge_right = shadow_center + shadow_size;

                if (shadow_edge_left < vertex_pos0.x)
                {
                    shadow_edge_left = vertex_pos0.x;

                    shadow_calc_left = (((shadow_edge_left - shadow_center) + shadow_size) * 992.0F) / shadow_size;

                    if (shadow_calc_left < 0.0F)
                    {
                        shadow_calc_left = 0.0F;
                    }
                    if (shadow_calc_left > 2048.0F)
                    {
                        shadow_calc_left = 2048.0F;
                    }
                }
                if (vertex_pos1.x < shadow_edge_right)
                {
                    shadow_edge_right = vertex_pos1.x;

                    shadow_calc_right = (((shadow_edge_right - shadow_center) * 992.0F) / shadow_size) + 992.0F;

                    if (shadow_calc_right < 0.0F)
                    {
                        shadow_calc_right = 0.0F;
                    }
                    if (shadow_calc_right > 2048.0F)
                    {
                        shadow_calc_right = 2048.0F;
                    }
                }
                gfx_vertex_num = 4;
                coll_vertex_num = mpCollisionGetVertexCountLineID(floor_line_id);

                if (coll_vertex_num >= 3)
                {
                    mpCollisionGetVertexPositionID(floor_line_id, 0, &vertex_pos0);
                    mpCollisionGetVertexPositionID(floor_line_id, 1, &vertex_pos1);

                    if (vertex_pos0.x < vertex_pos1.x)
                    {
                        edge_left_or_right = 0;

                        for (i = 1; i < coll_vertex_num; i++)
                        {
                            mpCollisionGetVertexPositionID(floor_line_id, i, &vertex_pos1);

                            if ((vertex_pos0.x <= shadow_edge_left) && (shadow_edge_left <= vertex_pos1.x))
                            {
                                shadow_alt_left = ftShadowGetAltitude(&vertex_pos0, &vertex_pos1, shadow_edge_left);

                                if ((vertex_pos0.x <= shadow_edge_right) && (shadow_edge_right <= vertex_pos1.x))
                                {
                                    shadow_alt_right = ftShadowGetAltitude(&vertex_pos0, &vertex_pos1, shadow_edge_right);
                                }
                                else
                                {
                                    gfx_vertex_num = 6;
                                    vertex_pos0 = vertex_pos1;

                                    mpCollisionGetVertexPositionID(floor_line_id, i + 1, &vertex_pos1);

                                    spE8 = vertex_pos0.x;
                                    spE4 = vertex_pos0.y;
                                    spF0 = (((shadow_calc_right - shadow_calc_left) * (vertex_pos0.x - shadow_edge_left)) / (shadow_edge_right - shadow_edge_left)) + shadow_calc_left;

                                    if (shadow_edge_right <= vertex_pos1.x)
                                    {
                                        shadow_alt_right = ftShadowGetAltitude(&vertex_pos0, &vertex_pos1, shadow_edge_right);
                                    }
                                    else
                                    {
                                        gfx_vertex_num = 8;
                                        vertex_pos0 = vertex_pos1;

                                        mpCollisionGetVertexPositionID(floor_line_id, i + 2, &vertex_pos1);
                                        shadow_alt_right = ftShadowGetAltitude(&vertex_pos0, &vertex_pos1, shadow_edge_right);
                                        spE0 = vertex_pos0.x;
                                        spDC = vertex_pos0.y;
                                        spEC = (((shadow_calc_right - shadow_calc_left) * (vertex_pos0.x - shadow_edge_left)) / (shadow_edge_right - shadow_edge_left)) + shadow_calc_left;
                                    }
                                }
                                break;
                            }
                            else vertex_pos0 = vertex_pos1;
                        }
                    }
                    else
                    {
                        edge_left_or_right = 1;
                        vertex_pos1 = vertex_pos0;

                        for (i = 1; i < coll_vertex_num; i++)
                        {
                            mpCollisionGetVertexPositionID(floor_line_id, i, &vertex_pos0);

                            if ((vertex_pos0.x <= shadow_edge_right) && (shadow_edge_right <= vertex_pos1.x))
                            {
                                shadow_alt_right = ftShadowGetAltitude(&vertex_pos0, &vertex_pos1, shadow_edge_right);

                                if ((vertex_pos0.x <= shadow_edge_left) && (vertex_pos1.x >= shadow_edge_left))
                                {
                                    shadow_alt_left = ftShadowGetAltitude(&vertex_pos0, &vertex_pos1, shadow_edge_left);
                                }
                                else
                                {
                                    gfx_vertex_num = 6;
                                    vertex_pos1 = vertex_pos0;

                                    mpCollisionGetVertexPositionID(floor_line_id, i + 1, &vertex_pos0);

                                    spE8 = vertex_pos1.x;
                                    spE4 = vertex_pos1.y;
                                    spF0 = (((shadow_calc_right - shadow_calc_left) * (vertex_pos1.x - shadow_edge_left)) / (shadow_edge_right - shadow_edge_left)) + shadow_calc_left;

                                    if (vertex_pos0.x <= shadow_edge_left)
                                    {
                                        shadow_alt_left = ftShadowGetAltitude(&vertex_pos0, &vertex_pos1, shadow_edge_left);
                                    }
                                    else
                                    {
                                        gfx_vertex_num = 8;
                                        vertex_pos1 = vertex_pos0;

                                        mpCollisionGetVertexPositionID(floor_line_id, i + 2, &vertex_pos0);

                                        shadow_alt_left = ftShadowGetAltitude(&vertex_pos0, &vertex_pos1, shadow_edge_left);
                                        spE0 = vertex_pos1.x;
                                        spDC = vertex_pos1.y;

                                        spEC = (((shadow_calc_right - shadow_calc_left) * (vertex_pos1.x - shadow_edge_left)) / (shadow_edge_right - shadow_edge_left)) + shadow_calc_left;
                                    }
                                }
                                break;
                            }
                            else vertex_pos1 = vertex_pos0;
                        }
                    }
                }
                else
                {
                    shadow_alt_left = ftShadowGetAltitude(&vertex_pos0, &vertex_pos1, shadow_edge_left);
                    shadow_alt_right = ftShadowGetAltitude(&vertex_pos0, &vertex_pos1, shadow_edge_right);
                }
                if (gfx_vertex_num != 0)
                {
                    if (gSYTaskmanTaskID != 0)
                    {
                        shadow_vertex = &fs->vtx2[0];
                    }
                    else shadow_vertex = &fs->vtx1[0];

                    gDPPipeSync(gSYTaskmanDLHeads[0]++);

                    if ((gSCManagerBattleState->is_team_battle == TRUE) && !(gSCManagerBattleState->is_not_teamshadows))
                    {
                        gDPSetPrimColor
                        (
                            gSYTaskmanDLHeads[0]++,
                            0,
                            0,
                            dFTCommonDataShadowColorTeams[fp->team].r,
                            dFTCommonDataShadowColorTeams[fp->team].g,
                            dFTCommonDataShadowColorTeams[fp->team].b,
                            dFTCommonDataShadowColorTeams[fp->team].a
                        );
                    }
                    else gDPSetPrimColor
                    (
                        gSYTaskmanDLHeads[0]++,
                        0,
                        0,
                        dFTCommonDataShadowColorDefault.r,
                        dFTCommonDataShadowColorDefault.g,
                        dFTCommonDataShadowColorDefault.b,
                        dFTCommonDataShadowColorDefault.a
                    );
                    sv = shadow_vertex;

                    sv->n.ob[0] = shadow_edge_left;
                    sv->n.ob[1] = shadow_alt_left;
                    sv->n.ob[2] = 200;
                    sv->n.tc[0] = 0;
                    sv->n.tc[1] = shadow_calc_left;

                    sv++;

                    sv->n.ob[0] = shadow_edge_left;
                    sv->n.ob[1] = shadow_alt_left;
                    sv->n.ob[2] = -200;
                    sv->n.tc[0] = 1984;
                    sv->n.tc[1] = shadow_calc_left;

                    sv++;

                    sv->n.ob[0] = shadow_edge_right;
                    sv->n.ob[1] = shadow_alt_right;
                    sv->n.ob[2] = 200;
                    sv->n.tc[0] = 0;
                    sv->n.tc[1] = shadow_calc_right;

                    sv++;

                    sv->n.ob[0] = shadow_edge_right;
                    sv->n.ob[1] = shadow_alt_right;
                    sv->n.ob[2] = -200;
                    sv->n.tc[0] = 1984;
                    sv->n.tc[1] = shadow_calc_right;

                    if (gfx_vertex_num >= 6)
                    {
                        sv++;

                        sv->n.ob[0] = spE8;
                        sv->n.ob[1] = spE4;
                        sv->n.ob[2] = 200;
                        sv->n.tc[0] = 0;

                        if (spF0 < 0.0F)
                        {
                            spF0 = 0.0F;
                        }
                        if (spF0 > 2048.0F)
                        {
                            spF0 = 2048.0F;
                        }
                        sv->n.tc[1] = spF0;

                        sv++;

                        sv->n.ob[0] = spE8;
                        sv->n.ob[1] = spE4;
                        sv->n.ob[2] = -200;
                        sv->n.tc[0] = 1984;
                        sv->n.tc[1] = spF0;

                        if (gfx_vertex_num == 8)
                        {
                            sv++;

                            sv->n.ob[0] = spE0;
                            sv->n.ob[1] = spDC;
                            sv->n.ob[2] = 200;
                            sv->n.tc[0] = 0;

                            if (spEC < 0.0F)
                            {
                                spEC = 0.0F;
                            }
                            if (spEC > 2048.0F)
                            {
                                spEC = 2048.0F;
                            }
                            sv->n.tc[1] = spEC;

                            sv++;

                            sv->n.ob[0] = spE0;
                            sv->n.ob[1] = spDC;
                            sv->n.ob[2] = -200;
                            sv->n.tc[0] = 1984;
                            sv->n.tc[1] = spEC;

                            gSPVertex(gSYTaskmanDLHeads[0]++, shadow_vertex, 8, 0);

                            if (edge_left_or_right != 0)
                            {
                                gSP2Triangles(gSYTaskmanDLHeads[0]++, 1, 0, 7, 0, 0, 6, 7, 0);
                                gSP2Triangles(gSYTaskmanDLHeads[0]++, 7, 6, 5, 0, 6, 4, 5, 0);
                                gSP2Triangles(gSYTaskmanDLHeads[0]++, 5, 4, 3, 0, 4, 2, 3, 0);
                            }
                            else
                            {
                                gSP2Triangles(gSYTaskmanDLHeads[0]++, 1, 0, 5, 0, 0, 4, 5, 0);
                                gSP2Triangles(gSYTaskmanDLHeads[0]++, 5, 4, 7, 0, 4, 6, 7, 0);
                                gSP2Triangles(gSYTaskmanDLHeads[0]++, 7, 6, 3, 0, 6, 2, 3, 0);
                            }
                        }
                        else
                        {
                            gSPVertex(gSYTaskmanDLHeads[0]++, shadow_vertex, 6, 0);
                            gSP2Triangles(gSYTaskmanDLHeads[0]++, 1, 0, 5, 0, 0, 4, 5, 0);
                            gSP2Triangles(gSYTaskmanDLHeads[0]++, 5, 4, 3, 0, 4, 2, 3, 0);
                        }
                    }
                    else
                    {
                        gSPVertex(gSYTaskmanDLHeads[0]++, shadow_vertex, 4, 0);
                        gSP2Triangles(gSYTaskmanDLHeads[0]++, 1, 0, 3, 0, 0, 2, 3, 0);
                    }
                }
            }
        }
    }
    if (shadow_gobj->dl_link_next == NULL)
    {
        gSPDisplayList(gSYTaskmanDLHeads[0]++, dFTShadowNoNextLinkDL);
    }
    }
}

/* DIVERGES: syTaskmanMalloc(sizeof(FTShadow), 0x8).
 *
 * The game allocates one FTShadow per fighter from the scene heap, and
 * never gives it back -- nothing ejects a shadow, and the memory goes
 * when the scene's heap does. That is sound for a game that makes four
 * fighters a match and tears the heap down between them, and it is not
 * sound here: hosttest_ft spawns hundreds of fighters inside one heap
 * and makes and destroys them freely.
 *
 * So the struct is a slot per player instead, which is the same lifetime
 * -- a shadow reads gSCManagerBattleState->players[fs->player] and there
 * is one fighter behind that at a time -- with no allocation to lose.
 * ftShadowEjectShadow below is the other half: it takes the GObj back,
 * which the game also never does. */
static FTShadow sFTShadowStructs[GMCOMMON_PLAYERS_MAX];

static FTShadow *ftShadowGetStruct(GObj *fighter_gobj)
{
    s32 player = ftGetStruct(fighter_gobj)->player;

    if ((player < 0) || (player >= (s32)ARRAY_COUNT(sFTShadowStructs)))
    {
        return NULL;
    }
    return &sFTShadowStructs[player];
}

/* DIVERGES, and it has no counterpart in the game: the shadow a fighter
 * was given back, so that ftManagerDestroyFighter leaves nothing behind.
 * The game's ftManagerDestroyFighter does not do this and does not need
 * to (see above). */
void ftShadowEjectShadow(GObj *fighter_gobj)
{
    const FTShadow *fs = ftShadowGetStruct(fighter_gobj);
    GObj *g = gGCCommonLinks[nGCCommonLinkIDShadow];

    while (g != NULL)
    {
        GObj *next = g->link_next;

        /* link 13 is shared -- the wallpaper, the transition and the
         * fighter's parts are all on it (sys/objdef.h:91-97) -- so the
         * GObj's own kind is what says this one is a shadow. */
        if ((g->id == nGCCommonKindShadow) && (g->user_data.p == fs))
        {
            gcEjectGObj(g);
        }
        g = next;
    }
}

// 0x8013BB88
GObj* ftShadowMakeShadow(GObj *fighter_gobj)
{
    GObj *shadow_gobj = gcMakeGObjSPAfter(nGCCommonKindShadow, NULL, nGCCommonLinkIDShadow, GOBJ_PRIORITY_DEFAULT);
    FTStruct *fp;
    FTShadow *fs = ftShadowGetStruct(fighter_gobj);   /* DIVERGES */
    s32 i;

    if (fs == NULL) 
    {
        return NULL;
    }
#ifndef SSB_NO_DRAW
    /* DIVERGES: the game's texture arrives with the effect file the
     * scene loaded; the port's is a pack of its own, read once. */
    {
        int pal_bank = 0;

        if (sFTShadowIsLoaded == FALSE &&
            fighter_load(&sFTShadowPack, FTSHADOW_MODEL, &pal_bank) == 0)
        {
            sFTShadowIsLoaded = TRUE;
        }
    }
#endif
    fp = ftGetStruct(fighter_gobj);

    fs->player = fp->player;

    for (i = 0; i < (ARRAY_COUNT(fs->vtx1) + ARRAY_COUNT(fs->vtx2)) / 2; i++)
    {
        fs->vtx1[i].n.flag = 0;
        fs->vtx1[i].n.n[0] = 0;
        fs->vtx1[i].n.n[1] = 0;
        fs->vtx1[i].n.n[2] = 0;
        fs->vtx1[i].n.a = 128;
        fs->vtx2[i].n.flag = 0;
        fs->vtx2[i].n.n[0] = 0;
        fs->vtx2[i].n.n[1] = 0;
        fs->vtx2[i].n.n[2] = 0;
        fs->vtx2[i].n.a = 128;
    }
    shadow_gobj->user_data.p = fs;

    gcAddGObjDisplay(shadow_gobj, DBPERF_TIMED(ftShadowProcDisplay), 7, GOBJ_PRIORITY_DEFAULT, ~0);

    return shadow_gobj;
}

/* The overlay's own clear: ft/ftshadow.c is in dSCManagerOverlays[3]
 * (src/dc/overlay.c), whose .bss the game bzeroes when the overlay is
 * loaded again. The pack is released before the pointers go, as the
 * magnifying glass's is. */
void ftShadowOverlayLoad(void)
{
#ifndef SSB_NO_DRAW
    fighter_release(&sFTShadowPack);
    OVERLAY_CLEAR(sFTShadowPack);
    OVERLAY_CLEAR(sFTShadowIsLoaded);
    OVERLAY_CLEAR(sFTShadowHdrSent);
#endif
    OVERLAY_CLEAR(sFTShadowStructs);
    OVERLAY_CLEAR(gFTShadowLog);
    OVERLAY_CLEAR(gFTShadowLogCount);
    OVERLAY_CLEAR(gSYTaskmanTaskID);
    OVERLAY_CLEAR(sFTShadowDraw);
    /* not a clear: zero is a frame number, and the reset has to fire on
     * the first frame of the new scene rather than be skipped by it */
    sFTShadowFrame = (u32)-1;
}
