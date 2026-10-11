/* ftdisplaymain.c -- see ftdisplaymain.h. */
#include "ftdisplaymain.h"

#include "ftcommon.h"
#include "gmcamera.h"
#include "objmodel.h"
#include "fighter.h"            /* fighter_set_fog_color */

#include <ft/fighter.h>
#include <if/ifcommon.h>
#include <if/interface.h>
#include <sc/scene.h>
#include <sys/develop.h>       /* nDBDisplayModeMaster */
#include <sys/objman.h>
#include <sys/vector.h>
#include <sc/scsubsys/scsubsys.h> /* scSubsysFighterDrawLightColorGetAlpha */

#include "objpvr.h"             /* gcGetDrawList, gcGetViewF */
#include "perf.h"
#include "clip.h"
#ifndef SSB_NO_DRAW
#include "dcpvr.h"

/* lb/lbcommon.c:312, defined in src/dc/lbcommon.c */
extern Vec3f gLBCommonScale;
#endif

/* ftdisplaymain.c:38-55 dFTDisplayMainShufflePositions 0x8012B940,
 * verbatim: the four offsets a fighter in hitlag is jogged by, cycled one
 * per tic (ftcommon.c:12553 counts shuffle_frame_index through
 * shuffle_index_max). Two rows, ordinary damage and electric -- the
 * electric one is smaller and shakes sideways as well.
 *
 * The decomp uses it in ftDisplayMainProcDisplay (:1205-1216), which
 * pushes the translation onto the modelview stack before drawing the
 * fighter; the port folds it into the view the fighter's models are
 * transformed by (src/dc/objmodel.c dc_model_view_for).
 * Its other reader is the held item's matrix kind 0x52
 * (src/dc/objdisplay.c), which adds the same offset to the hand's world
 * matrix so an item stays in a hand that is being shaken. */
Vec2f dFTDisplayMainShufflePositions[/* */][4] =
{
    /* Non-electric */
    {
        {   0.0F, -100.0F },
        {   0.0F,  -50.0F },
        {   0.0F,  100.0F },
        {   0.0F,   50.0F }
    },

    /* Electric */
    {
        {   0.0F,  -20.0F },
        {  15.0F,    5.0F },
        { -15.0F,    5.0F },
        {   0.0F,    0.0F }
    }
};

/* ftdisplaymain.c:29 */
SYColorRGBA sFTDisplayMainFogColor;

/* ftdisplaymain.c:23, the alpha the fighter being drawn is drawn at:
 * below 0xFF the game draws it under G_RM_AA_ZB_XLU_SURF2 (:715-719).
 * In VS it is 0xFF but for a menu's Demo fighter, whose alpha is the
 * scene's light (scSubsysFighterSetLightParams) -- the results screen's
 * podium fade-in. */
u8 sFTDisplayMainSkyFogAlpha;

#ifdef SSB_NO_DRAW
/* The fog the last draw handed its models, packed RGBA: what the host
 * tests read where the target reads Fighter.fog_live. */
u32 gFTDisplayMainLastFog;
#endif

/* ftdisplaymain.c:599-663 ftDisplayMainCalcFogColor 0x800F17E8, verbatim:
 * the colour animation's tint (colanim.color1) folded over the team
 * shade, its alpha scaled by the fighter's own fog_color.r. */
void ftDisplayMainCalcFogColor(FTStruct *fp)
{
    s32 shade;
    s32 temp_color;
    s32 red;
    s32 green;
    s32 blue;
    s32 alpha;
    s32 shade_base;

    if (fp->shade == 0)
    {
        red = fp->colanim.color1.r;
        green = fp->colanim.color1.g;
        blue = fp->colanim.color1.b;
        alpha = fp->colanim.color1.a;
    }
    else
    {
        SYColorRGBA *attr_shade_color = &fp->attr->shade_color[fp->shade - 1];
        GMColKeys *ck = &fp->colanim.color1;

        shade_base = (((0xFF - attr_shade_color->a) * (0xFF - ck->a)) / 0xFF);

        if (shade_base == 0xFF)
        {
            red = fp->colanim.color1.r;
            green = fp->colanim.color1.g;
            blue = fp->colanim.color1.b;
            alpha = fp->colanim.color1.a;
        }
        else
        {
            shade_base = (0xFF - shade_base);

            temp_color = (((ck->r - fp->shade_color.r) * ck->a) / 0xFF) + fp->shade_color.r;

            red = (temp_color * 0xFF) / shade_base;

            if (red != 0)
            {
                alpha = ((temp_color * 0xFF) / red);
            }
            else alpha = ((shade_base - temp_color) * 0xFF) / 0xFF;

            temp_color = (((ck->g - fp->shade_color.g) * ck->a) / 0xFF) + fp->shade_color.g;

            green = (temp_color * 0xFF) / shade_base;

            temp_color = (((ck->b - fp->shade_color.b) * ck->a) / 0xFF) + fp->shade_color.b;

            blue = (temp_color * 0xFF) / shade_base;
        }
    }

    if (fp->attr->fog_color.r != 0xFF)
    {
        alpha = (fp->attr->fog_color.r * alpha) / 0xFF;
    }
    sFTDisplayMainFogColor.r = red;
    sFTDisplayMainFogColor.g = green;
    sFTDisplayMainFogColor.b = blue;
    sFTDisplayMainFogColor.a = alpha;
}

/* The port's gDPSetFogColor: the colour every model the fighter draws
 * blends toward (Fighter.fog_live, src/dc/fighter.c material_of). */
static void ftDisplayMainSetModelFog(FTStruct *fp, u8 r, u8 g, u8 b, u8 a)
{
    u32 rgba = ((u32)r << 24) | ((u32)g << 16) | ((u32)b << 8) | a;

#ifndef SSB_NO_DRAW
    Fighter *models[DC_MODEL_TREE_MODELS];
    int n = dc_model_models_of(fp->fighter_gobj, models, DC_MODEL_TREE_MODELS);
    int i;

    for (i = 0; i < n; i++)
    {
        fighter_set_fog_color(models[i], rgba);
    }
#else
    gFTDisplayMainLastFog = rgba;
#endif
}

/* ftdisplaymain.c:665-669 ftDisplayMainSetFogColor 0x800F1B24 */
void ftDisplayMainSetFogColor(FTStruct *fp)
{
    ftDisplayMainSetModelFog(fp, sFTDisplayMainFogColor.r, sFTDisplayMainFogColor.g, sFTDisplayMainFogColor.b, sFTDisplayMainFogColor.a);
}

/* ftdisplaymain.c:671-684 ftDisplayMainDecideFogColor 0x800F1B7C: no
 * tint, so only the team shade (or nothing) */
void ftDisplayMainDecideFogColor(FTStruct *fp)
{
    if (fp->shade == 0)
    {
        ftDisplayMainSetModelFog(fp, 0x00, 0x00, 0x00, 0x00);
    }
    else
    {
        SYColorRGBA *fog_color = &fp->attr->shade_color[fp->shade - 1];

        ftDisplayMainSetModelFog(fp, fog_color->r, fog_color->g, fog_color->b, fog_color->a);
    }
}

/* ---- the swing trail ---------------------------------- */

/* ft/ftdisplaymain.c:380-389, verbatim: the trail's inner and outer edge
 * colours, Link's sword and the Beam Sword's. */
// 0x8012C4C8
SYColorRGBA dFTDisplayMainDefaultAfterImageColor1 = { 0x00, 0xFF, 0xFF, 0x00 };

// 0x8012C4CC
SYColorRGBA dFTDisplayMainDefaultAfterImageColor2 = { 0xFF, 0xFF, 0xFF, 0x00 };

// 0x8012C4D0
SYColorRGBA dFTDisplayMainItemAfterImageColor1 = { 0xFF, 0x40, 0xC0, 0x00 };

// 0x8012C4D4
SYColorRGBA dFTDisplayMainItemAfterImageColor2 = { 0xFF, 0xFF, 0xFF, 0x00 };

/* What the last trail drew: its vertices and triangles, as the RSP would
 * have been handed them. The host tests read it; the Dreamcast draws from
 * the same record. */
Vtx gFTDisplayMainAfterImageVtx[FTDISPLAYMAIN_AFTERIMAGE_VTX_MAX];
s32 gFTDisplayMainAfterImageVtxCount;
s32 gFTDisplayMainAfterImageTriCount;

/* The vertices' storage. The game takes them off gSYTaskmanGraphicsHeap,
 * which the port has no use for. Three samples are six vertices, and the
 * fill between two neighbours is at most six more pairs (a half turn in
 * thirty-degree steps), so a trail is thirty at most. */
static struct
{
    void *ptr;
} sFTDisplayMainAfterImageHeap;

#ifndef SSB_NO_DRAW
/* dFTDisplayMainAfterImageVertexDL and TriangleDL (ftdisplaymain.c:363-377):
 * shade-only combiner, no lighting, no culling. The trail goes on DL head
 * 1, the translucent one, drawn with the vertices' own alpha over what is
 * behind it and without writing depth. */
static void ftDisplayMainAfterImageHeader(void)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;

    pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.gen.shading = PVR_SHADE_GOURAUD;
    cxt.blend.src = PVR_BLEND_SRCALPHA;
    cxt.blend.dst = PVR_BLEND_INVSRCALPHA;
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    cxt.depth.write = PVR_DEPTHWRITE_DISABLE;
    DBPERF_COMPILE();
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));
}

/* One triangle of world-space vertices through the current camera. */
static void ftDisplayMainAfterImageTriangle(const Vtx *v, s32 a, s32 b, s32 c)
{
    const float *view = gcGetViewF();
    const float *proj = gcGetProjF();
    const DCViewport *vp = gcGetViewport();
    const s32 idx[3] = { a, b, c };
    ClipVtx tri[3], poly[4];
    pvr_vertex_t pv[3];
    s32 n, k, i;

    for (k = 0; k < 3; k++)
    {
        const Vtx *p = &v[idx[k]];
        f32 x = p->v.ob[0], y = p->v.ob[1], z = p->v.ob[2];
        f32 vx = view[0] * x + view[1] * y + view[2] * z + view[3];
        f32 vy = view[4] * x + view[5] * y + view[6] * z + view[7];
        f32 vz = view[8] * x + view[9] * y + view[10] * z + view[11];

        tri[k].cx = proj[0] * vx + proj[1] * vy + proj[2] * vz + proj[3];
        tri[k].cy = proj[4] * vx + proj[5] * vy + proj[6] * vz + proj[7];
        tri[k].cz = proj[8] * vx + proj[9] * vy + proj[10] * vz + proj[11];
        tri[k].cw = proj[12] * vx + proj[13] * vy + proj[14] * vz + proj[15];
        tri[k].u = tri[k].v = 0.0F;
        tri[k].argb = ((u32)p->v.cn[3] << 24) | ((u32)p->v.cn[0] << 16) |
                      ((u32)p->v.cn[1] << 8) | (u32)p->v.cn[2];
    }
    n = clip_near(tri, poly);

    for (k = 1; k + 1 < n; k++)
    {
        const ClipVtx *q[3];

        q[0] = &poly[0];
        q[1] = &poly[k];
        q[2] = &poly[k + 1];

        for (i = 0; i < 3; i++)
        {
            f32 invw = 1.0F / q[i]->cw;

            pv[i].flags = (i == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            pv[i].x = q[i]->cx * invw * vp->hw + vp->cx;
            pv[i].y = vp->cy - q[i]->cy * invw * vp->hh;
            pv[i].z = invw;
            pv[i].u = pv[i].v = 0.0F;
            pv[i].argb = q[i]->argb;
            pv[i].oargb = 0;
        }
        pvr_prim(pv, sizeof(pv));
    }
}
#endif /* !SSB_NO_DRAW */

/* gSPVertex and gSP2Triangles, over the record above. */
static void ftDisplayMainAfterImageVertex(const Vtx *v, s32 n)
{
    s32 i;

    if (n > FTDISPLAYMAIN_AFTERIMAGE_VTX_MAX)
    {
        n = FTDISPLAYMAIN_AFTERIMAGE_VTX_MAX;
    }
    for (i = 0; i < n; i++)
    {
        gFTDisplayMainAfterImageVtx[i] = v[i];
    }
    gFTDisplayMainAfterImageVtxCount = n;
    gFTDisplayMainAfterImageTriCount = 0;
#ifndef SSB_NO_DRAW
    if (n >= 3)
    {
        ftDisplayMainAfterImageHeader();
    }
#endif
}

static void ftDisplayMainAfterImage2Triangles(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f)
{
    if ((a >= gFTDisplayMainAfterImageVtxCount) || (b >= gFTDisplayMainAfterImageVtxCount) ||
        (c >= gFTDisplayMainAfterImageVtxCount) || (d >= gFTDisplayMainAfterImageVtxCount) ||
        (e >= gFTDisplayMainAfterImageVtxCount) || (f >= gFTDisplayMainAfterImageVtxCount))
    {
        return;
    }
    gFTDisplayMainAfterImageTriCount += 2;
#ifndef SSB_NO_DRAW
    ftDisplayMainAfterImageTriangle(gFTDisplayMainAfterImageVtx, a, b, c);
    ftDisplayMainAfterImageTriangle(gFTDisplayMainAfterImageVtx, d, e, f);
#endif
}

#undef gSPDisplayList
#undef gSPVertex
#undef gSP2Triangles
#define gSPDisplayList(pkt, dl)                 ((void)(dl))
#define gSPVertex(pkt, v, n, v0)                ftDisplayMainAfterImageVertex((v), (n))
#define gSP2Triangles(pkt, a, b, c, f0, d, e, f, f1) \
    ftDisplayMainAfterImage2Triangles((a), (b), (c), (d), (e), (f))
#define gSYTaskmanGraphicsHeap                  sFTDisplayMainAfterImageHeap

/* ft/ftdisplaymain.c:397-596 ftDisplayMainDrawAfterImage 0x800F1020: the
 * swing trail, a strip between the three samples ftMainProcParams keeps,
 * filled in every thirty degrees they turn by. Verbatim but for what the
 * macros above make of its display list: the vertices come from a static
 * buffer rather than the graphics heap, and the two display lists it
 * brackets them with are the PVR header (ftDisplayMainAfterImageHeader).
 * Drawn in the translucent pass only (ftDisplayMainProcDisplay). */
/* the port's DB_PERF timing around it, called in its place (perf.h) */
DBPERF_DEFINE_TIMED(DBP_AFTER, ftDisplayMainDrawAfterImage, FTStruct *)

// 0x800F1020
void ftDisplayMainDrawAfterImage(FTStruct *fp)
{
    static Vtx vtx_buf[FTDISPLAYMAIN_AFTERIMAGE_VTX_MAX + 16];
    s32 i, j;
    s32 next_index;
    s32 index;
    Vtx *base_p_vtx;
    Vtx *p_vtx;
    s32 vtx_count;
    s32 add_alpha;
    s32 base_alpha;
    f32 var_f20;
    f32 var_f22;
    f32 rotate;
    SYColorRGBA *color1, *color2;
    Gfx *vtx_dl, *tri_dl;
    FTAfterImage *afterimage;
    Vec3f spC8;
    FTAfterImage *next_afterimage;
    s32 alpha;
    f32 scale;
    s32 alphainc;
    Vec3f spAC;

    gSYTaskmanGraphicsHeap.ptr = vtx_buf;

    index = fp->afterimage.desc_id;

    switch (fp->afterimage.is_itemswing)
    {
    case FALSE:
        var_f20 = 50.0F;
        var_f22 = 250.0F;

        add_alpha = 0;
        base_alpha = 0xFF;

        rotate = F_CLC_DTOR32(30.0F); // 0.5235988F

        color1 = &dFTDisplayMainDefaultAfterImageColor1;
        color2 = &dFTDisplayMainDefaultAfterImageColor2;

        vtx_dl = NULL;
        tri_dl = NULL;
        break;

    case TRUE:
        var_f20 = 80.0F;
        var_f22 = 580.0F;

        add_alpha = 0;
        base_alpha = 0xFF;

        rotate = F_CLC_DTOR32(30.0F); // 0.5235988F

        color1 = &dFTDisplayMainItemAfterImageColor1;
        color2 = &dFTDisplayMainItemAfterImageColor2;

        vtx_dl = NULL;
        tri_dl = NULL;
        break;
    }
    base_p_vtx = p_vtx = (Vtx*)gSYTaskmanGraphicsHeap.ptr;

    if (index != 0)
    {
        index = index - 1;
    }
    else index = ARRAY_COUNT(fp->afterimage.desc) - 1;

    for (i = fp->afterimage.drawstatus - 1; i >= 0; index = next_index, i--)
    {
        afterimage = &fp->afterimage.desc[index];

        alpha = (((base_alpha - add_alpha) / (fp->afterimage.drawstatus - 1)) * i) + add_alpha;

        p_vtx->v.ob[0] = (afterimage->translate_x + (afterimage->vec.x * var_f20));
        p_vtx->v.ob[1] = (afterimage->translate_y + (afterimage->vec.y * var_f20));
        p_vtx->v.ob[2] = (afterimage->translate_z + (afterimage->vec.z * var_f20));

        p_vtx->v.flag = 0;

        p_vtx->v.tc[0] = p_vtx->v.tc[1] = 0;

        p_vtx->v.cn[0] = color1->r;
        p_vtx->v.cn[1] = color1->g;
        p_vtx->v.cn[2] = color1->b;
        p_vtx->v.cn[3] = alpha;

        p_vtx = (gSYTaskmanGraphicsHeap.ptr = (Vtx*)gSYTaskmanGraphicsHeap.ptr + 1);

        p_vtx->v.ob[0] = (afterimage->translate_x + (afterimage->vec.x * var_f22));
        p_vtx->v.ob[1] = (afterimage->translate_y + (afterimage->vec.y * var_f22));
        p_vtx->v.ob[2] = (afterimage->translate_z + (afterimage->vec.z * var_f22));

        p_vtx->v.flag = 0;

        p_vtx->v.tc[0] = p_vtx->v.tc[1] = 0;

        p_vtx->v.cn[0] = color2->r;
        p_vtx->v.cn[1] = color2->g;
        p_vtx->v.cn[2] = color2->b;
        p_vtx->v.cn[3] = alpha;

        p_vtx = (gSYTaskmanGraphicsHeap.ptr = (Vtx*)gSYTaskmanGraphicsHeap.ptr + 1);

        if (i != 0)
        {
            if (index != 0)
            {
                next_index = index - 1;
            }
            else next_index = ARRAY_COUNT(fp->afterimage.desc) - 1;

            next_afterimage = &fp->afterimage.desc[next_index];

            if (syVectorNormCross3D(&afterimage->vec, &next_afterimage->vec, &spC8) != NULL)
            {
                f32 f_angle_diff = syVectorAngleDiff3D(&afterimage->vec, &next_afterimage->vec);
                s32 target_angle = f_angle_diff / rotate;

                if (target_angle != 0)
                {
                    s16 n_ai_x;
                    s16 vtx_x;
                    s16 vtx_y;
                    s16 vtx_z;
                    s16 n_ai_y;
                    s16 n_ai_z;

                    target_angle++;

                    scale = 1.0F / (target_angle);

                    n_ai_x = afterimage->translate_x;
                    n_ai_y = afterimage->translate_y;
                    n_ai_z = afterimage->translate_z;

                    f_angle_diff *= scale;

                    vtx_x = ((next_afterimage->translate_x - n_ai_x) * scale);
                    vtx_y = ((next_afterimage->translate_y - n_ai_y) * scale);
                    vtx_z = ((next_afterimage->translate_z - n_ai_z) * scale);

                    spAC = afterimage->vec;

                    alphainc = (((((base_alpha - add_alpha) / (fp->afterimage.drawstatus - 1)) * (i - 1)) + add_alpha) - alpha) * scale;

                    for (j = 0; j < target_angle - 1; j++)
                    {
                        n_ai_x += vtx_x;
                        n_ai_y += vtx_y;
                        n_ai_z += vtx_z;

                        syVectorRotateAbout3D(&spAC, &spC8, f_angle_diff);

                        alpha += alphainc;

                        p_vtx->v.ob[0] = (n_ai_x + (spAC.x * var_f20));
                        p_vtx->v.ob[1] = (n_ai_y + (spAC.y * var_f20));
                        p_vtx->v.ob[2] = (n_ai_z + (spAC.z * var_f20));

                        p_vtx->v.flag = 0;

                        p_vtx->v.tc[0] = p_vtx->v.tc[1] = 0;

                        p_vtx->v.cn[0] = color1->r;
                        p_vtx->v.cn[1] = color1->g;
                        p_vtx->v.cn[2] = color1->b;
                        p_vtx->v.cn[3] = alpha;

                        p_vtx = (gSYTaskmanGraphicsHeap.ptr = (Vtx*)gSYTaskmanGraphicsHeap.ptr + 1);

                        p_vtx->v.ob[0] = (n_ai_x + (spAC.x * var_f22));
                        p_vtx->v.ob[1] = (n_ai_y + (spAC.y * var_f22));
                        p_vtx->v.ob[2] = (n_ai_z + (spAC.z * var_f22));

                        p_vtx->v.flag = 0;

                        p_vtx->v.tc[0] = p_vtx->v.tc[1] = 0;

                        p_vtx->v.cn[0] = color2->r;
                        p_vtx->v.cn[1] = color2->g;
                        p_vtx->v.cn[2] = color2->b;
                        p_vtx->v.cn[3] = alpha;

                        p_vtx = (gSYTaskmanGraphicsHeap.ptr = (Vtx*)gSYTaskmanGraphicsHeap.ptr + 1);
                    }
                }
            }
        }
    }
    vtx_count = ((uintptr_t)p_vtx - (uintptr_t)base_p_vtx) / (sizeof(*p_vtx) | sizeof(*base_p_vtx));

    gSPDisplayList(gSYTaskmanDLHeads[1]++, vtx_dl);
    gSPVertex(gSYTaskmanDLHeads[1]++, base_p_vtx, vtx_count, 0);

    for (i = 0; i < (vtx_count - 2); i += 2)
    {
        gSP2Triangles(gSYTaskmanDLHeads[1]++, i, i + 1, i + 2, FALSE, i + 1, i + 3, i + 2, FALSE);
    }
    gSPDisplayList(gSYTaskmanDLHeads[1]++, tri_dl);
}

#undef gSYTaskmanGraphicsHeap

/* ft/ftdisplaymain.c:947-958, the tail of ftDisplayMainDrawAll: the trail
 * once it has two samples. DIVERGES: in the translucent pass only, the one
 * the PVR draws it in; the other two leave its record alone. */
static void ftDisplayMainDrawAllAfterImage(FTStruct *fp)
{
#ifndef SSB_NO_DRAW
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
#endif
    if (fp->afterimage.drawstatus >= 2)
    {
        switch (fp->afterimage.is_itemswing)
        {
        case FALSE:
            DBPERF_TIMED(ftDisplayMainDrawAfterImage)(fp);
            break;

        case TRUE:
            DBPERF_TIMED(ftDisplayMainDrawAfterImage)(fp);
            break;
        }
    }
}

/* ft/ftdisplaymain.c:687-721 ftDisplayMainDecideFogDraw 0x800F1C08, as
 * the port draws: the game calls it with a joint's FTParts.flags before
 * that joint's display list, and sets the render mode the list is drawn
 * under. DIVERGES: the pack's batches are drawn after the walk
 * (dc_model_proc_display), so the walk records each joint's flags on the
 * model instead (Fighter.part_flags), and fighter.c material_of draws a
 * FTPARTS_FLAG_NOFOG joint out of the fog -- the G_RM_PASS arm. The
 * FTPARTS_FLAG_TOGGLEFOG arm, and sFTDisplayMainIsShadeFog with it, is
 * not carried: no fighter's FTModelPart or FTCommonPart sets 0x80
 * (relocData <F>Main.c), so it never runs. The render mode's opaque and
 * XLU arms are Fighter.alpha_cut, which ftDisplayMainProcDisplay sets
 * from sFTDisplayMainSkyFogAlpha. */
_Static_assert(FIGHTER_PART_NOFOG == FTPARTS_FLAG_NOFOG, "fighter.h FIGHTER_PART_NOFOG is ft/ftdef.h's");

static void ftDisplayMainDecideFogDrawAll(FTStruct *fp)
{
    Fighter *model = dc_model_of(fp->fighter_gobj);
    FTParts *parts;
    DObj *joint;
    s32 i;

    if (model == NULL)
    {
        return;
    }
    for (i = 0; i < ARRAY_COUNT(fp->joints) - nFTPartsJointCommonStart; i++)
    {
        joint = fp->joints[i + nFTPartsJointCommonStart];
        parts = (joint != NULL) ? ftGetParts(joint) : NULL;

        model->part_flags[i] = (parts != NULL) ? parts->flags : 0;
    }
}

/* ft/ftdisplaymain.c:723-750 ftDisplayMainDrawAccessory 0x800F1D44:
 * Pikachu's hat and Jigglypuff's bow, drawn with the joint that carries
 * the accessory GObj under that joint's own tests. DIVERGES: the
 * accessory is a part of the fighter's pack (src/dc/fighter.h
 * FPackParts.accessory_tag) that dc_model_proc_display draws with the
 * joint, so drawing it is turning it on; its MObjs are baked into the
 * pack, and the fog it is drawn under is the whole fighter's (see
 * ftDisplayMainProcDisplay). */
void ftDisplayMainDrawAccessory(FTStruct *fp, DObj *dobj, FTParts *parts)
{
    Fighter *model = dc_model_of(fp->fighter_gobj);

    if (model == NULL)
    {
        return;
    }
    switch (parts->flags & 0xF)
    {
    case 0:
        if ((dobj->dv != NULL) && !(dobj->flags & DOBJ_FLAG_NOTEXTURE))
        {
            model->accessory_on = TRUE;
        }
        break;

    case 1:
        if ((dobj->dls != NULL) && (dobj->dls[1] != NULL) && !(dobj->flags & DOBJ_FLAG_NOTEXTURE))
        {
            model->accessory_on = TRUE;
        }
        break;
    }
}

/* ft/ftdisplaymain.c:776-822, the three ftDisplayMainDrawAccessory calls
 * out of ftDisplayMainDrawDefault's walk, for the one joint they can
 * reach: the one whose parts hold a GObj, which only the accessory's
 * joint does (ftManagerMakeFighter, ftParamInitAllParts). DIVERGES: the
 * walk itself is dc_model_proc_display's, which skips a hidden joint and
 * everything under it; Jigglypuff's call sits in the arm for parts flags
 * 0 and Pikachu's is after the switch, which the flags test inside
 * ftDisplayMainDrawAccessory makes the same test. */
static void ftDisplayMainDecideAccessory(FTStruct *fp)
{
    Fighter *model = dc_model_of(fp->fighter_gobj);
    FTAccessPart *accesspart = fp->attr->accesspart;
    DObj *dobj;
    FTParts *parts;

    if (model == NULL)
    {
        return;
    }
    model->accessory_on = FALSE;

    if ((accesspart == NULL) || ((fp->fkind != nFTKindPurin) && (fp->fkind != nFTKindPikachu)))
    {
        return;
    }
    dobj = fp->joints[accesspart->joint_id];

    if ((dobj == NULL) || (dobj->flags & DOBJ_FLAG_HIDDEN))
    {
        return;
    }
    parts = ftGetParts(dobj);

    if ((parts != NULL) && (parts->gobj != NULL))
    {
        if ((fp->fkind == nFTKindPikachu) || ((parts->flags & 0xF) == 0))
        {
            ftDisplayMainDrawAccessory(fp, dobj, parts);
        }
    }
}

/* ftdisplaymain.c:1068-1160 0x800F293C, the head of
 * ftDisplayMainProcDisplay, verbatim -- and then the port's own draw
 * where the decomp's 250 lines of GBI would be.
 *
 * What the block does, once a frame for each fighter, under the battle
 * camera: take the fighter's TopN joint, raise it by the character's
 * cam_offset_y (and pull that point back toward the camera's target if
 * it is further away than the offset, so that a fighter at the edge of a
 * wide shot is measured from where it looks rather than where it is),
 * project it, and ask gmCameraCheckTargetInBounds whether it landed
 * inside the viewport. If it did not, project a second point 300 units
 * over the fighter's head -- that is the one the magnifying glass and the
 * arrow point at -- into fp->magnify_pos, raise is_magnify_show, and,
 * unless this fighter is exempt or respawning, ask the arrows which edge
 * it went out of.
 *
 * The two dying statuses are the exception: a fighter on its way up the
 * star or falling off the bottom is off screen on purpose and gets no
 * glass.
 *
 * Under the magnify camera the same proc runs again for a
 * fighter that is off screen: the else-if arm sets up the glass's own
 * viewport and projection (ifCommonPlayerMagnifyUpdateViewport), the
 * fighter is drawn with its TopN's translation switched off so it stands
 * at the origin of that view (lines 1216-1222), and the tail draws the
 * handle of the glass over it (ifCommonPlayerMagnifyProcDisplay).
 *
 * DIVERGES:
 *   - the TopN kind switch is Tra to Null and back rather than 0x4B to
 *     RotRpyR and back: the port's TopN carries gcAddDObjMatrixSetsRpyR's
 *     three XObjs where the game's carries one of its own kind, and
 *     dropping the first of the three is dropping the translation
 *     (src/dc/objdisplay.c gcDObjLocalMatrix).
 *   - the AVOID_UB arm of the distance clamp is the one taken. The
 *     decomp's default arm writes through `&ft_pos - 2` to reproduce a
 *     stack layout, and says so in its own comment; the two compute the
 *     same thing. Chosen by hand here; the build
 *     says the same thing everywhere (-DAVOID_UB in DECOMP_DEFS).
 *   - everything else below the block but the fog -- the reflection
 *     lights, ftDisplayMainDrawParts over the DObj tree, the shadow
 *     and the collision debug shapes -- is the port's
 *     dc_model_proc_display (src/dc/objmodel.c), which draws the baked
 *     pack, and under the magnify camera draws it clipped to the glass. */
/* ft/ftdisplaymain.c:929-947 ftDisplayMainDrawAll 0x800F24A0, the
 * choice: the electric skeleton when the colour animation
 * names one (GMColEventDefault's skeleton_id), the fighter has that
 * table, and the joint word 0 names is built with a display list --
 * ftDisplayMainDrawSkeleton -- and the fighter's own model otherwise.
 * Returns the skeleton id, 0 for the model. DIVERGES: the two draws are
 * the pack's, so the port returns the choice and the caller hands it to
 * the models (Fighter.skeleton_id), which draw only the skeleton's tags
 * (tools/lib/ssb_meshexport.py read_skeleton); the after-image half is
 * ftDisplayMainDrawAllAfterImage. */
s32 ftDisplayMainGetSkeletonID(FTStruct *fp)
{
    FTAttributes *attr = fp->attr;

    if
    (
        (fp->colanim.skeleton_id)                                   &&
        (attr->skeleton != NULL)                                    &&
        (attr->skeleton[fp->colanim.skeleton_id] != NULL)           &&
        (fp->joints[(s32)(intptr_t)(attr->skeleton[0])] != NULL)    &&
        (fp->joints[(s32)(intptr_t)(attr->skeleton[0])]->dl != NULL)
    )
    {
        return fp->colanim.skeleton_id;
    }
    return 0;
}

void ftDisplayMainProcDisplay(GObj *fighter_gobj)
{
    FTStruct *fp;
    Vec3f ft_pos;
    f32 cam_pos_x;
    f32 cam_pos_y;
    Vec3f dist;

    fp = ftGetStruct(fighter_gobj);

#if !defined(SSB_NO_DRAW) && !defined(DB_NO_LISTMASK)
    /* The punch-through pass: nothing below sets state a later pass reads
     * (the magnify and arrow flags, the fog and the alpha are all set again
     * by each pass that draws), and the afterimage is translucent-only, so
     * a fighter with no punch-through batch has nothing to do here. */
    if (gcGetDrawList() == PVR_LIST_PT_POLY)
    {
        Fighter *models[DC_MODEL_TREE_MODELS];
        int n = dc_model_models_of(fighter_gobj, models, DC_MODEL_TREE_MODELS);
        int any = (n == 0), i;

        for (i = 0; i < n; i++)
            any |= fighter_has_list(models[i], FPACK_LIST_PT);
        if (!any)
            return;
    }
#endif
    if ((fp->is_invisible) && (fp->display_mode == nDBDisplayModeMaster))
    {
        fp->is_magnify_show = FALSE;

        return;
    }
    if ((fp->pkind == nFTPlayerKindMan) || (fp->pkind == nFTPlayerKindCom) || (fp->pkind == nFTPlayerKindGameKey))
    {
        if (gGCCurrentCamera->id == nGCCommonKindMainCamera)
        {
            switch (fp->status_id)
            {
            case nFTCommonStatusDeadUpStar:
            case nFTCommonStatusDeadUpFall:
                fp->is_magnify_show = FALSE;
                break;

            default:
                ft_pos = fp->joints[nFTPartsJointTopN]->translate.vec.f;
                ft_pos.y += fp->attr->cam_offset_y;

                syVectorDiff3D(&dist, &CObjGetStruct(gGMCameraGObj)->vec.at, &ft_pos);

                if (fp->attr->cam_offset_y < syVectorMag3D(&dist))
                {
                    syVectorNorm3D(&dist);
                    syVectorScale3D(&dist, fp->attr->cam_offset_y);
                    syVectorAdd3D(&ft_pos, &dist);
                }
                func_ovl2_800EB924(CObjGetStruct(gGMCameraGObj), gGMCameraMatrix, &ft_pos, &cam_pos_x, &cam_pos_y);

                if (gmCameraCheckTargetInBounds(cam_pos_x, cam_pos_y) == FALSE)
                {
                    ft_pos = fp->joints[nFTPartsJointTopN]->translate.vec.f;
                    ft_pos.y += 300.0F;

                    func_ovl2_800EB924(CObjGetStruct(gGMCameraGObj), gGMCameraMatrix, &ft_pos, &fp->magnify_pos.x, &fp->magnify_pos.y);

                    fp->is_magnify_show = TRUE;

                    if (gIFCommonPlayerInterface.is_magnify_display != FALSE)
                    {
                        if (!(fp->is_magnify_ignore) && !(fp->is_rebirth))
                        {
                            gIFCommonPlayerInterface.magnify_mode = 1;

                            ifCommonPlayerArrowsUpdateFlags(cam_pos_x, cam_pos_y);
                        }
                    }
                    return;
                }
                else fp->is_magnify_show = FALSE;

                break;
            }
        }
        else if (!(fp->is_magnify_ignore) && !(fp->is_rebirth) && (fp->is_magnify_show))
        {
            /* gSYTaskmanDLHeads: there are no display-list heads here
             * (src/dc/taskman.c), as for gcPrepCameraMatrix */
            ifCommonPlayerMagnifyUpdateViewport(NULL, fp);
        }
        else return;
    }
    /* ftdisplaymain.c:1180-1196, the alpha arm: which
     * alpha the fighter is drawn at, handed to its models
     * (Fighter.alpha_cut) where the game picks the XLU render mode.
     * DIVERGES: the env colours these arms write have no display list
     * to go in, and a Man or COM fighter's arm reads 0xFF where the game
     * calls mpCollisionSetLightColorGetAlpha, whose gMPCollisionLightColor
     * is 0xFF on every stage (mp/mpcollision.c:4003-4006). */
    if (fp->colanim.is_use_color2)
    {
        sFTDisplayMainSkyFogAlpha = fp->colanim.color2.a;
    }
    else if (fp->is_use_fogcolor)
    {
        sFTDisplayMainSkyFogAlpha = fp->fog_color.a;
    }
    else if (fp->pkind != nFTPlayerKindDemo)
    {
        sFTDisplayMainSkyFogAlpha = 0xFF;
    }
    else sFTDisplayMainSkyFogAlpha = scSubsysFighterDrawLightColorGetAlpha(NULL);

#ifndef SSB_NO_DRAW
    {
        Fighter *models[DC_MODEL_TREE_MODELS];
        int n = dc_model_models_of(fighter_gobj, models, DC_MODEL_TREE_MODELS);
        int i;

        for (i = 0; i < n; i++)
        {
            fighter_set_alpha(models[i], sFTDisplayMainSkyFogAlpha);
        }
    }
#endif
    /* ftdisplaymain.c:1198-1203, the fog the whole fighter is drawn
     * under, and each joint's part flags for the fog
     * draw (ftDisplayMainDecideFogDrawAll, V06). */
    if (fp->colanim.is_use_color1)
    {
        ftDisplayMainCalcFogColor(fp);
        ftDisplayMainSetFogColor(fp);
    }
    else ftDisplayMainDecideFogColor(fp);

    ftDisplayMainDecideFogDrawAll(fp);
    ftDisplayMainDecideAccessory(fp);

    /* ftdisplaymain.c:1162: the running scale of matrix kind 0x4B starts
     * at one for each fighter's walk */
    gLBCommonScale.x = gLBCommonScale.y = gLBCommonScale.z = 1.0F;

#ifndef SSB_NO_DRAW
    {
        Fighter *models[DC_MODEL_TREE_MODELS];
        int n = dc_model_models_of(fighter_gobj, models, DC_MODEL_TREE_MODELS);
        u8 skeleton_id = (u8)ftDisplayMainGetSkeletonID(fp);
        int i;

        for (i = 0; i < n; i++)
        {
            models[i]->skeleton_id = skeleton_id;
        }
    }
#endif

    /* ftdisplaymain.c:1216-1222 */
    if ((fp->pkind == nFTPlayerKindDemo) || (fp->pkind == nFTPlayerKindKey) || (gGCCurrentCamera->id == nGCCommonKindMainCamera))
    {
        dc_model_proc_display(fighter_gobj);
        ftDisplayMainDrawAllAfterImage(fp);
    }
    else
    {
        fp->joints[nFTPartsJointTopN]->xobjs[0]->kind = nGCMatrixKindNull;

        dc_model_proc_display(fighter_gobj);
        ftDisplayMainDrawAllAfterImage(fp);

        fp->joints[nFTPartsJointTopN]->xobjs[0]->kind = nGCMatrixKindTra;
    }
    /* ftdisplaymain.c:1384-1392 */
    if ((fp->pkind == nFTPlayerKindMan) || (fp->pkind == nFTPlayerKindCom) || (fp->pkind == nFTPlayerKindGameKey))
    {
        if (gGCCurrentCamera->id != nGCCommonKindMainCamera)
        {
            if (!(fp->is_magnify_ignore) && !(fp->is_rebirth) && (fp->is_magnify_show))
            {
                ifCommonPlayerMagnifyProcDisplay(fp);
            }
        }
    }
}
