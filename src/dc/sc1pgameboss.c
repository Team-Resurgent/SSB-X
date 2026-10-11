/* See sc1pgameboss.h. sc/sc1pmode/sc1pgameboss.c, overlay 65.
 *
 * The file is the decomp's, line for line, except where a note below
 * says otherwise. Four things diverge. The first three have the same
 * reason: the decomp reads its trees out of a live ROM overlay and this
 * port reads them out of the stage's baked BWP1 packs.
 *
 *   1. SC1PGameBossEffect's o_dobjdesc/o_mobjsub and SC1PGameBossAnim's
 *      o_anim_joint/o_matanim_joint are offsets into
 *      sSC1PGameBossMain.file_head. There is no file_head here -- the
 *      trees are Stage.boss_models[] and their scripts are
 *      Stage.boss_anim_tables[] -- so the four offsets become a pack
 *      NAME, a "has an AnimJoint" flag and a material ALT index. Hence
 *      DCBossEffect/DCBossAnim below rather than the decomp's two
 *      structs; every other field of every table is transcribed
 *      unchanged.
 *
 *      The alt is not cosmetic. Two wallpaper rows share ONE tree with
 *      DIFFERENT materials: dSC1PGameBossEffects2[1] and
 *      dSC1PGameBossEffects3[0] are both llGRLastMapEffects2_1, and
 *      dSC1PGameBossAnims2[1] plays its Anims2_1 MatAnimJoint while
 *      dSC1PGameBossAnims3[0] plays Anims3_0. The exporter lays the two
 *      down as that pack's two alts, in that order.
 *
 *   2. sc1PGameBossSetupBackgroundDObjs walks a raw DObjDesc[] with the
 *      XOR hack and hands each entry's `dl` to gcAddChildForDObj. That
 *      union is what gcSubmitDObj reads back as a DCDisplay pointer
 *      here, so a raw display-list pointer in it is a wrong-typed read,
 *      not a shortcut -- src/dc/efground.c's dcGroundSetupEffectDObjs
 *      spells this out at length and is the precedent this follows.
 *      dcBossSetupBackgroundDObjs replaces it.
 *
 *   3. The display procs' gDP lines. gSYTaskmanDLHeads is a scratch
 *      buffer nothing renders from (src/dc/taskman.h), so a render mode
 *      written there does nothing: what the RDP would have been left in
 *      is instead baked into each pack's batch buckets, out of the seed
 *      render mode tools/export/ssb_stageexport.py gives these five trees
 *      (G_RM_AA_XLU_SURF, which is exactly what these procs set). The
 *      two colours a proc sets ahead of the draw are real and are kept,
 *      through fighter_set_prim_color/fighter_set_env_color -- the
 *      shield's and the impact wave's own path (src/dc/efmanager.c
 *      efManagerImpactWaveModelColors), correct per instance because a
 *      display proc runs immediately before its own GObj's draw. That
 *      includes Wallpaper1/2/3's gDPSetPrimColor(.., alpha): it is the
 *      ONLY way their fade reaches the vortex (MObj flags 0x20), the
 *      tunnel picture (0x04) and the glow (no MObj); for the fog (0x6b)
 *      the MObj's own emit (objdisplay.c:1218 sends PRIM for flag 0x8
 *      as well as 0x200) carries the same alpha the proc just wrote into
 *      it. The MObj-colour lines stay verbatim beside it.
 *

 *   4. The layering. The N64 layers these effects over and under the
 *      stage by camera ORDER, Z-less; the PVR has one Z buffer, so each
 *      tree is drawn in a depth band chosen by its plan's camera tag
 *      instead (boss_draw_tree).
 *
 * The two fade procs' gDPFillRectangle(10, 10, 310, 230) is
 * lbCommonSpriteFillRect, the port's own equivalent (src/dc/lbfade.c:90
 * fills the same rectangle from the same corners). Like the decomp's,
 * they draw the rectangle and not the tree.
 */

#ifdef FT_HOSTTEST
#include <stdio.h>
#define dbglog(level, ...) printf(__VA_ARGS__)
#else
#include <kos.h>                /* dbglog */
#endif

#include <gr/ground.h>
#include <if/interface.h>
#include <if/ifcommon.h>        /* ifCommonBattleEndSetBossDefeat */
#include <sc/scene.h>
#include <sys/rdp.h>

#ifdef DB_BOSS_TOUR
#include <ft/fighter.h>
#endif

#include "sc1pgameboss.h"
#include "stage.h"
#include "objmodel.h"
#include "objpvr.h"             /* gcGetDrawList */
#include "fighter.h"
#include "lbcommon.h"

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* DIVERGES (1): SC1PGameBossEffect with the two offsets resolved to the
 * name of the BWP1 pack they addressed. */
typedef struct
{
    void (*proc_update)(GObj *);
    void (*proc_display)(GObj *);
    const char *pack;
} DCBossEffect;

/* DIVERGES (1): SC1PGameBossAnim with the two offsets resolved. A NULL
 * o_anim_joint becomes has_anim FALSE; an o_matanim_joint becomes which
 * of the pack's MatAnimJoint alts it is, or -1 for a row with none. */
typedef struct
{
    sb32 has_anim;
    s32 matanim_alt;
    f32 anim_speed;
} DCBossAnim;

/* The decomp's SC1PGameBossWallpaper with those two pointers retyped;
 * every scalar is its own. */
typedef struct
{
    s32 loop_count;
    s32 effect_count;
    s32 anim_count;
    s32 plan_count;
    s32 dobj_color_id;
    s32 color_id;
    s32 change_wait_base;
    s32 change_damage_min;
    sb32 is_random_wallpaper;
    const DCBossEffect *bosseffect;
    const DCBossAnim *bossanim;
    const SC1PGameBossPlan *bossplan;
} DCBossWallpaper;

/* The decomp's SC1PGameBossMain, `file_head` dropped: the trees do not
 * live at an offset from a loaded overlay here. */
typedef struct
{
    sb32 is_skip_wallpaper_change;
    s32 wallpaper_id;
    s32 change_wait;
    const DCBossWallpaper *bosswallpaper;
    s32 bossplayer;
} DCBossMain;

// 0x80192BC0 - Red color values of shooting stars on Final Destination
u8 dSC1PGameBossCometEnvColorR[/* */] = { 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x78 };

// 0x80192BC8 - Green color values of shooting stars on Final Destination
u8 dSC1PGameBossCometEnvColorG[/* */] = { 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x78 };

// 0x80192BD0 - Blue color values of shooting stars on Final Destination
u8 dSC1PGameBossCometEnvColorB[/* */] = { 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x78 };

// 0x80192BD8
static const DCBossEffect dSC1PGameBossEffects0[/* */] =
{
    // Effect 0
    {
        SC1PGameBossWallpaper0ProcUpdate,       // Proc Update
        SC1PGameBossWallpaper0ProcDisplay,      // Proc Render
        "Effects0"                              // llGRLastMapEffects0 DObjDesc + MObjSub
    }
};

// 0x80192BE8
static const DCBossAnim dSC1PGameBossAnims0[/* */] =
{
    { TRUE, 0, 0.5F },                          // Anim 0: Anims0 AnimJoint + MatAnimJoint
    { TRUE, 0, 1.0F },                          // Anim 1
    { TRUE, 0, 0.7F },                          // Anim 2
    { TRUE, 0, 0.25F },                         // Anim 3
};

// 0x80192C18
static const SC1PGameBossPlan dSC1PGameBossPlans0[/* */] =
{
    { 10, 5, 1, { 0.0F, 0.0F, -2000.0F } },     // Plan 0
    { 14, 5, 2, { 0.0F, 0.0F, -3000.0F } },     // Plan 1
};

// 0x80192C48
static const DCBossEffect dSC1PGameBossEffects1[/* */] =
{
    {
        SC1PGameBossWallpaper1ProcUpdate,
        SC1PGameBossWallpaper1ProcDisplay,
        "Effects1"
    }
};

// 0x80192C58
static const DCBossAnim dSC1PGameBossAnims1[/* */] =
{
    { TRUE, 0, 0.5F },                          // Anim 0: Anims1 AnimJoint + MatAnimJoint
};

// 0x80192C64
static const SC1PGameBossPlan dSC1PGameBossPlans1[/* */] =
{
    { 1, 5, 2, { 0.0F, 4800.0F, -10000.0F } },  // Plan 0
};

// 0x80192C7C
static const DCBossEffect dSC1PGameBossEffects2[/* */] =
{
    {
        SC1PGameBossWallpaper2ProcUpdate0,
        SC1PGameBossWallpaper2ProcDisplay,
        "Effects2_0"
    },
    {
        SC1PGameBossWallpaper2ProcUpdate1,
        SC1PGameBossWallpaper3ProcDisplay0,
        "Effects2_1"
    }
};

// 0x80192C9C
static const DCBossAnim dSC1PGameBossAnims2[/* */] =
{
    { TRUE, 0, 1.0F },                          // Anim 0: Anims2_0 AnimJoint + MatAnimJoint
    { FALSE, 0, 1.0F },                         // Anim 1: no AnimJoint, Anims2_1 MatAnimJoint
};

// 0x801292CB4
static const SC1PGameBossPlan dSC1PGameBossPlans2[/* */] =
{
    { 1, 5, 2, { 0.0F, 0.0F, 2000.0F } },       // Plan 0
    { 1, 5, 2, { 0.0F, 0.0F, 1000.0F } },       // Plan 1
};

// 0x80192CE4
static const DCBossEffect dSC1PGameBossEffects3[/* */] =
{
    {
        SC1PGameBossWallpaper3ProcUpdate0,
        SC1PGameBossWallpaper3ProcDisplay0,
        /* llGRLastMapEffects3_0 is the SAME DObjDesc and MObjSub as
         * Effects2_1 -- five trees, six rows. What tells the two rows
         * apart is the material alt below. */
        "Effects2_1"
    },
    {
        SC1PGameBossWallpaper3ProcUpdate1,
        SC1PGameBossWallpaper2ProcDisplay,
        "Effects3_1"
    }
};

// 0x80192D04
static const DCBossAnim dSC1PGameBossAnims3[/* */] =
{
    { FALSE, 1, 0.5F },                         // Anim 0: no AnimJoint, Anims3_0 MatAnimJoint (alt 1)
    { TRUE, -1, 1.0F },                         // Anim 1: Anims3_1 AnimJoint, no MatAnimJoint
};

// 0x80192D1C
static const SC1PGameBossPlan dSC1PGameBossPlans3[/* */] =
{
    { 1, 5, 2, { 0.0F, 0.0F, 1000.0F } },       // Plan 0
    { 1, 5, 1, { 0.0F, 0.0F, -2000.0F } },      // Plan 1
};

// 0x80192D4C
static const DCBossWallpaper dSC1PGameBossWallpapers[/* */] =
{
    // Space background?
    {
        24,                                     // Loop count?
        ARRAY_COUNT(dSC1PGameBossEffects0),     // Total effect count?
        ARRAY_COUNT(dSC1PGameBossAnims0),       // Total animation count?
        ARRAY_COUNT(dSC1PGameBossPlans0),       // Total plan count?
        1,                                      // ???
        1,                                      // Color ID
        -1,                                     // Background change wait default
        90,                                     // Minimum damage for background change
        TRUE,                                   // Use random effects and animations?
        dSC1PGameBossEffects0,
        dSC1PGameBossAnims0,
        dSC1PGameBossPlans0
    },

    // ???
    {
        1,
        ARRAY_COUNT(dSC1PGameBossEffects1),
        ARRAY_COUNT(dSC1PGameBossAnims1),
        ARRAY_COUNT(dSC1PGameBossPlans1),
        1,
        1,
        -1,
        180,
        FALSE,
        dSC1PGameBossEffects1,
        dSC1PGameBossAnims1,
        dSC1PGameBossPlans1
    },

    // ???
    {
        2,
        ARRAY_COUNT(dSC1PGameBossEffects2),
        ARRAY_COUNT(dSC1PGameBossAnims2),
        ARRAY_COUNT(dSC1PGameBossPlans2),
        1,
        3,
        -1,
        -1,
        FALSE,
        dSC1PGameBossEffects2,
        dSC1PGameBossAnims2,
        dSC1PGameBossPlans2
    },

    // ???
    {
        2,
        ARRAY_COUNT(dSC1PGameBossEffects3),
        ARRAY_COUNT(dSC1PGameBossAnims3),
        ARRAY_COUNT(dSC1PGameBossPlans3),
        5,
        1,
        -1,
        -1,
        FALSE,
        dSC1PGameBossEffects3,
        dSC1PGameBossAnims3,
        dSC1PGameBossPlans3
    }
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x801938D0
GObj *sSC1PGameBossWallpaperGObj;

// 0x801938D8
static DCBossMain sSC1PGameBossMain;

// 0x801938F0
static f32 sSC1PGameBossWallpaperStepRGBA;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

// 0x801910B0
void func_ovl65_801910B0(void)
{
    GObj *gobj;

    gcResumeGObjProcessAll(sSC1PGameBossWallpaperGObj);

    gobj = gGCCommonLinks[nGCCommonLinkIDWallpaper];

    while (gobj != NULL)
    {
        if (gobj->id == nGCCommonKindBossWallpaper)
        {
            gcResumeGObjProcessAll(gobj);
        }
        gobj = gobj->link_next;
    }
}

// 0x80191114
void sc1PGameBossSetChangeWallpaper(void)
{
    sSC1PGameBossMain.is_skip_wallpaper_change = FALSE;
}

// 0x80191120
void sc1PGameBossMakeCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindUnkCamera3,
            NULL,
            nGCCommonLinkIDCamera,
            GOBJ_PRIORITY_DEFAULT,
            func_80017EC0,
            40,
            COBJ_MASK_DLLINK(5),
            1,
            TRUE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, gGMCameraStruct.viewport_ulx, gGMCameraStruct.viewport_uly, gGMCameraStruct.viewport_lrx, gGMCameraStruct.viewport_lry);

    cobj->projection.persp.aspect = (f32)(gGMCameraStruct.viewport_lrx - gGMCameraStruct.viewport_ulx) / (f32)(gGMCameraStruct.viewport_lry - gGMCameraStruct.viewport_uly);

    cobj->flags |= COBJ_FLAG_DLBUFFERS;

    cobj->vec.at.x = cobj->vec.at.y = cobj->vec.at.z = 0.0F;
    cobj->vec.eye.x = cobj->vec.eye.y = 0.0F;
    cobj->vec.eye.z = 2000.0F;

    cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindUnkCamera3,
            NULL,
            nGCCommonLinkIDCamera,
            GOBJ_PRIORITY_DEFAULT,
            func_80017EC0,
            60,
            COBJ_MASK_DLLINK(5),
            2,
            TRUE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, gGMCameraStruct.viewport_ulx, gGMCameraStruct.viewport_uly, gGMCameraStruct.viewport_lrx, gGMCameraStruct.viewport_lry);

    cobj->projection.persp.aspect = (f32)(gGMCameraStruct.viewport_lrx - gGMCameraStruct.viewport_ulx) / (f32)(gGMCameraStruct.viewport_lry - gGMCameraStruct.viewport_uly);

    cobj->flags |= COBJ_FLAG_DLBUFFERS;

    cobj->vec.at.x = cobj->vec.at.y = cobj->vec.at.z = 0.0F;
    cobj->vec.eye.x = cobj->vec.eye.y = 0.0F;
    cobj->vec.eye.z = 2000.0F;
}

/* DIVERGES (3): the render modes these four procs bracket their draw
 * with are baked into the packs' buckets; what is live is the colour,
 * and the draw. See this file's own header.
 *
 * DIVERGES (4): the draw's DEPTH. sc1PGameBossMakeCamera's two cameras
 * straddle the battle camera (priority 50) in the camera link, which
 * draws highest priority first (sys/objman.c gcDLLinkGObjTail): tag 2's
 * at 60 draws before the stage and the fighters, tag 1's at 40 after
 * them, and neither the procs' G_RM_AA_XLU_SURF nor the heads they
 * write read or write Z. So on the N64 the draw ORDER is the layering.
 * The PVR has one Z buffer for every camera, and these cameras' eye
 * stands 2000 from the effects -- far nearer than the battle camera
 * stands from the stage -- so at their own 1/w the vortex (tag 2) drew
 * over the stage. A plan's camera tag says which camera captures it,
 * so it picks the band: tag 2 the backdrop band, under every 3D pixel
 * and over the stage wallpaper; tag 1 the frame's next sprite depth,
 * over the stage and the fighters and under the HUD, which draws later
 * still. */
/*
 * Every instance of one plan shares the stage's one Fighter for that
 * pack (st->boss_models): the 24 comets are 24 GObjs over one pack. The
 * layered draw transforms in the opaque pass and submits in the
 * translucent one, so every comet would submit the LAST comet's pose --
 * one blob, 24 deep. The Z-less draw walks and transforms in the
 * translucent pass, immediately ahead of each GObj's own submit (the
 * same class as 65ced5a's items and weapons), each tree over the ones
 * drawn before it and its batches in display-list order, as the Z-less
 * procs drew them.
 *
 * `head` is the display-list head the proc wrote: inside one camera the
 * heads splice 0, 2, 1, 3 (sys/taskman.c syTaskmanUpdateDLBuffers), so
 * Wallpaper3ProcDisplay0's head-0 picture at the end of the vortex
 * draws under the head-1 vortex, though it is made after it. Every
 * head-0 tree is a tag-2 one. */
static void boss_draw_tree(GObj *gobj, s32 head)
{
    s32 band;

    if (!(gobj->camera_tag & 2))
    {
        band = DC_MODEL_ZLESS_FRONT;
    }
    else band = (head == 0) ? DC_MODEL_ZLESS_BACKDROP_UNDER
                            : DC_MODEL_ZLESS_BACKDROP;

    dc_model_proc_display_zless(gobj, band);
}

/* The proc's gDPSetPrimColor(0, 0, r, g, b, alpha) ahead of the draw,
 * RGBA as the game packs it; prim_halves (fighter.c) keeps only the
 * halves each batch's combiner reads. */
static void boss_prim(GObj *gobj, u32 rgb, s32 alpha)
{
    Fighter *f = dc_model_of(gobj);

    if (f != NULL)
    {
        fighter_set_prim_color(f, (rgb << 8) | (u32)(alpha & 0xFF));
    }
}

// 0x80191364
void SC1PGameBossWallpaper0ProcDisplay(GObj *gobj)
{
    s32 color_id = DObjGetStruct(gobj)->child->user_data.s;
    s32 alpha = gobj->user_data.s;
    Fighter *f = dc_model_of(gobj);

    if (f != NULL)
    {
        fighter_set_prim_color(f, 0xFFFFFF00u | (u32)(alpha & 0xFF));
        fighter_set_env_color(f,
            ((u32)dSC1PGameBossCometEnvColorR[color_id] << 24) |
            ((u32)dSC1PGameBossCometEnvColorG[color_id] << 16) |
            ((u32)dSC1PGameBossCometEnvColorB[color_id] << 8) |
            (u32)(alpha & 0xFF));
    }
    boss_draw_tree(gobj, 1);
}

// 0x80191498
void SC1PGameBossWallpaper1ProcDisplay(GObj *gobj)
{
    s32 alpha = gobj->user_data.s;
    DObj *dobj = DObjGetStruct(gobj);

    dobj = dobj->child;

    while (dobj != NULL)
    {
        dobj->mobj->sub.primcolor.s.a = alpha;
        dobj = dobj->child;
    }
    boss_prim(gobj, 0x000000, alpha);
    boss_draw_tree(gobj, 1);
}

// 0x801915B8
void SC1PGameBossWallpaper2ProcDisplay(GObj *gobj)
{
    s32 alpha = gobj->user_data.s;
    DObj *dobj = DObjGetStruct(gobj);

    while (dobj != NULL)
    {
        if (dobj->mobj != NULL) // NULL check here but not the function above? WTF?
        {
            dobj->mobj->sub.primcolor.s.a = alpha;
        }
        dobj = dobj->child;
    }
    boss_prim(gobj, 0xFFFFFF, alpha);
    boss_draw_tree(gobj, 1);
}

// 0x801916A8
void SC1PGameBossWallpaper3ProcDisplay0(GObj *gobj)
{
    s32 alpha = gobj->user_data.s;
    DObj *dobj = DObjGetStruct(gobj);

    while (dobj != NULL)
    {
        if (dobj->mobj != NULL)
        {
            dobj->mobj->sub.primcolor.s.a = alpha;
        }
        dobj = dobj->child;
    }
    boss_prim(gobj, 0xFFFFFF, alpha);
    boss_draw_tree(gobj, 0);
}

// 0x80191798
void sc1PGameBossProcDisplayFadeAlpha(GObj *gobj)
{
    s32 alpha;

    /* The proc runs once per PVR list (src/dc/taskman.c) and steps the
     * fade as it draws; the rectangle is translucent-only anyway
     * (lbCommonSpriteFillRect), so the step goes with it -- once a
     * frame, the N64's rate. scStaffrollJobProcDisplay gates the same
     * way. */
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    sSC1PGameBossWallpaperStepRGBA++;

    if (sSC1PGameBossWallpaperStepRGBA > 255.0F)
    {
        sSC1PGameBossWallpaperStepRGBA = 255.0F;
    }
    alpha = sSC1PGameBossWallpaperStepRGBA;

    /* DIVERGES (3): gDPFillRectangle(10, 10, 310, 230) under
     * G_CC_PRIMITIVE / G_RM_AA_XLU_SURF with the primitive colour, which
     * is lbCommonSpriteFillRect's whole job (src/dc/lbfade.c:90). */
    lbCommonSpriteFillRect(10, 10, 310, 230, 0xFF, 0xFF, 0xFF, alpha);
}

// 0x80191908
void sc1PGameBossProcDisplayFadeColor(GObj *gobj)
{
    f32 sub = 2.55F;
    s32 color;

    /* once a frame, as sc1PGameBossProcDisplayFadeAlpha */
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    sSC1PGameBossWallpaperStepRGBA -= sub; // Maybe this is what they did? Doing this only because there's unused stack otherwise.

    if (sSC1PGameBossWallpaperStepRGBA < 0.0F)
    {
        sSC1PGameBossWallpaperStepRGBA = 0.0F;
    }
    color = sSC1PGameBossWallpaperStepRGBA;

    lbCommonSpriteFillRect(10, 10, 310, 230, color, color, color, 0xFF);
}

// 0x80191A94
void sc1PGameBossUpdateWallpaperColorID(void)
{
    GObj *gobj = gGCCommonLinks[nGCCommonLinkIDWallpaper];

    while (gobj != NULL)
    {
        if (gobj->id == nGCCommonKindBossWallpaper)
        {
            s32 color = sSC1PGameBossMain.bosswallpaper->color_id;

            DObjGetStruct(gobj)->user_data.s = color * -1;
        }
        gobj = gobj->link_next;
    }
}

// 0x80191AEC
void SC1PGameBossWallpaper3ProcUpdate0(GObj *gobj)
{
    DObj *dobj = DObjGetStruct(gobj);

    gobj->user_data.s += dobj->user_data.s;

    if (gobj->user_data.s < 0)
    {
        gcEjectGObj(gobj);
    }
    else
    {
        if (gobj->user_data.s > 0xFF)
        {
            gobj->user_data.s = 0xFF;
        }
        gcPlayAnimAll(gobj);
    }
}

// 0x80191B44
void func_ovl65_80191B44(GObj *gobj)
{
    DObj *dobj = DObjGetStruct(gobj);
    f32 lr;
    f32 bt;
    s32 angle;
    s32 sw;

    sw = 0;
    angle = (syUtilsRandIntRange(2) * 30) + 30;

    lr = ABS(gMPCollisionBounds.current.left - 2000.0F) + ABS(gMPCollisionBounds.current.right + 2000.0F);
    bt = ABS(gMPCollisionGroundData->map_bound_top - 2000.0F) + ABS(gMPCollisionGroundData->map_bound_bottom + 2000.0F);

    dobj->translate.vec.f.x = (syUtilsRandFloat() * lr) + (gMPCollisionBounds.current.left - 2000.0F);
    dobj->translate.vec.f.y = (syUtilsRandFloat() * bt) + (gMPCollisionGroundData->map_bound_bottom + 2000.0F);

    if (dobj->translate.vec.f.x < 0.0F)
    {
        sw = 2;
    }
    if (dobj->translate.vec.f.y < 0.0F)
    {
        sw++;
    }
    switch (sw)
    {
    case 1:
        angle += 180;
        break;

    case 3:
        angle += 90;
        break;

    case 0:
        angle += 270;
        break;
    }
    dobj->rotate.vec.f.z = F_CLC_DTOR32(angle);
}

// 0x80191D60
void SC1PGameBossWallpaper0ProcUpdate(GObj *gobj)
{
    SC1PGameBossWallpaper3ProcUpdate0(gobj);

    if (gobj->anim_frame <= 0.0F)
    {
        func_ovl65_80191B44(gobj);
    }
}

// 0x80191DA4
void SC1PGameBossWallpaper1ProcUpdate(GObj *gobj)
{
    DObj *dobj = DObjGetStruct(gobj);

    if (dobj->user_data.s == -1)
    {
        dobj->anim_speed += (-0.0012);

        gcSetAllAnimSpeed(gobj, dobj->anim_speed);
    }
    else if (gobj->user_data.s < 0xFF)
    {
        dobj->translate.vec.f.y += (-18.8F);
    }
    SC1PGameBossWallpaper3ProcUpdate0(gobj);
}

// 0x80191E28
void SC1PGameBossWallpaper2ProcUpdate0(GObj *gobj)
{
    DObj *dobj = DObjGetStruct(gobj);

    if (gobj->user_data.s == 0)
    {
        dobj->scale.vec.f.x = dobj->scale.vec.f.y = dobj->scale.vec.f.z = 0.0F;
    }
    if (gSCManagerBattleState->players[sSC1PGameBossMain.bossplayer].stock_damage_all > 270)
    {
        dobj->anim_speed += 0.02;

        gcSetAllAnimSpeed(gobj, dobj->anim_speed);
    }
    else if (gobj->user_data.s < 0xFF)
    {
        dobj->scale.vec.f.x += 0.004;
        dobj->scale.vec.f.y += 0.004;
        dobj->scale.vec.f.z += 0.004;
    }
    SC1PGameBossWallpaper3ProcUpdate0(gobj);
}

// 0x80191F28
void SC1PGameBossWallpaper2ProcUpdate1(GObj *gobj)
{
    if (gSCManagerBattleState->players[sSC1PGameBossMain.bossplayer].stock_damage_all > 270)
    {
        gobj->flags = GOBJ_FLAG_NONE;

        SC1PGameBossWallpaper3ProcUpdate0(gobj);
    }
    else gobj->flags = GOBJ_FLAG_HIDDEN;
}

// 0x80191F90
void SC1PGameBossWallpaper3ProcUpdate1(GObj *gobj)
{
    DObj *dobj = DObjGetStruct(gobj)->child;

    if (dobj->anim_wait == AOBJ_ANIM_NULL)
    {
        if ((gobj->proc_display != sc1PGameBossProcDisplayFadeAlpha) && (gobj->proc_display != sc1PGameBossProcDisplayFadeColor))
        {
            sSC1PGameBossWallpaperStepRGBA = 230.0F;
            dobj->user_data.s = 0x64;
            gobj->proc_display = sc1PGameBossProcDisplayFadeAlpha;
        }
        else
        {
            dobj->user_data.s--;

            if (dobj->user_data.s == 0)
            {
#ifdef DB_BOSS_WALLPAPER
                dbglog(DBG_INFO, "db: boss wallpaper: fade %s ran out\n",
                       (gobj->proc_display == sc1PGameBossProcDisplayFadeAlpha) ? "alpha" : "color");
#endif
                if (gobj->proc_display == sc1PGameBossProcDisplayFadeAlpha)
                {
                    sSC1PGameBossWallpaperStepRGBA = 255.0F;
                    dobj->user_data.s = 0x64;
                    gobj->proc_display = sc1PGameBossProcDisplayFadeColor;
                }
                else if (gobj->proc_display == sc1PGameBossProcDisplayFadeColor)
                {
#ifndef DB_BOSS_TOUR    /* the tour loops past the flash instead */
                    ifCommonBattleEndSetBossDefeat();
                    gcFuncGObjAll(ifCommonBattleInterfacePauseGObj, 0);
#endif
                }
            }
        }
    }
    else SC1PGameBossWallpaper3ProcUpdate0(gobj);
}

/* DIVERGES (2): PORT-OWN, replacing sc1PGameBossSetupBackgroundDObjs
 * (sc1pgameboss.c:782). The decomp walks a DObjDesc[] with the XOR hack,
 * hands each `dl` to gcAddChildForDObj and appends one fused
 * nGCMatrixKindTraRotRpyRSca per joint; the tree it builds IS the baked
 * pack's tree, so this is dc_model_add_dobjs over the pack, exactly as
 * src/dc/efground.c's dcGroundSetupEffectDObjs does for a ground actor.
 *
 * `parent` is NULL, so the pack's root joint hangs on the GObj itself:
 * the decomp's own entry 0 is `gcAddDObjForGObj(gobj, ...)`, and every
 * DObjGetStruct(gobj) in this file means pack joint 0 (the plan's
 * translate, the colour id, the alpha step). A bare wrapper DObj like
 * dcGroundMakeEffect's would shift all of them by one.
 *
 * The transform set is dc_model_add_dobjs' own Tra/RotRpyR/Sca triple
 * rather than an appended TraRotRpyRSca: the two compose the same
 * transform, and appending would leave the triple in the array with
 * nothing to drive (src/dc/objdisplay.c gcXObjDrivesDObj). The billboard
 * arm IS kept -- it is a different matrix, not a restatement -- and is
 * the decomp's two appends, 0x2E and 0x46. No tree in the block
 * actually carries a billboard joint (all five masks export as 0), so
 * nothing exercises it today. */
static void dcBossSetupBackgroundDObjs(GObj *gobj, Fighter *pack,
                                       u32 billboard, s32 matanim_alt)
{
    DObj *joints[STAGE_BOSS_JOINTS_MAX];
    int n, j;

    n = dc_model_add_dobjs(gobj, NULL, pack, joints);

    if (n < 0)
    {
        return;
    }
    for (j = 0; j < n; j++)
    {
        const FPackJoint *pj = &pack->joints[j];
        DObj *dobj = joints[j];

        if (dobj == NULL)
        {
            continue;
        }
        if (billboard & (1U << j))
        {
            gcAddXObjForDObjFixed(dobj, nGCMatrixKindTraRotRpyR, 0);
            gcAddXObjForDObjFixed(dobj, 0x46, 0);
        }
        /* The bind pose goes back on after the append, which claims the
         * vectors and resets them (sys/objman.c) -- the decomp writes
         * the three vectors after its own gcAddXObjForDObjFixed too. */
        dobj->translate.vec.f.x = pj->t[0];
        dobj->translate.vec.f.y = pj->t[1];
        dobj->translate.vec.f.z = pj->t[2];
        dobj->rotate.vec.f.x = pj->r[0];
        dobj->rotate.vec.f.y = pj->r[1];
        dobj->rotate.vec.f.z = pj->r[2];
        dobj->scale.vec.f.x = pj->s[0];
        dobj->scale.vec.f.y = pj->s[1];
        dobj->scale.vec.f.z = pj->s[2];
    }
    if ((pack->mobjs == NULL) || (matanim_alt < 0))
    {
        return;
    }
    /* The decomp's own p_mobjsubs walk is per DObjDesc entry, so every
     * joint, not just the first -- and `matanim_alt` is which of the
     * pack's MatAnimJoints this row plays (the whole reason the alt is
     * in the table at all: Effects2_1 is shared by two rows). */
    for (j = 0; j < n; j++)
    {
        if (joints[j] != NULL)
        {
            dc_model_add_mobjs_dobj(joints[j], pack, 0.0F, matanim_alt, j);
        }
    }
}

// 0x8019223C
void sc1PGameBossSetWallpaperTranslate(GObj *gobj, s32 plan_id)
{
    DObj *dobj = DObjGetStruct(gobj);

    if (sSC1PGameBossMain.bosswallpaper->is_random_wallpaper == TRUE)
    {
        func_ovl65_80191B44(gobj);

        dobj->translate.vec.f.z = sSC1PGameBossMain.bosswallpaper->bossplan[plan_id].pos.z;
    }
    else dobj->translate.vec.f = sSC1PGameBossMain.bosswallpaper->bossplan[plan_id].pos;
}

// 0x801922D4
GObj *sc1PGameBossMakeWallpaperEffect(s32 effect_id, s32 anim_id, s32 plan_id)
{
    GObj *effect_gobj;
    DObj *dobj;
    const DCBossEffect *effect;
    const DCBossAnim *anim;
    const SC1PGameBossPlan *plan;
    s32 dobj_color_id = sSC1PGameBossMain.bosswallpaper->dobj_color_id;
    Stage *st = stage_bound();
    int index;

    /* DIVERGES (1): `addr = sSC1PGameBossMain.file_head` and the three
     * offsets added to it become this one lookup -- which pack of the
     * stage's BWP1 block the row names. */
    if (st == NULL)
    {
        return NULL;
    }
    effect = &sSC1PGameBossMain.bosswallpaper->bosseffect[effect_id];
    anim = &sSC1PGameBossMain.bosswallpaper->bossanim[anim_id];
    plan = &sSC1PGameBossMain.bosswallpaper->bossplan[plan_id];

    index = stage_boss_pack(st, effect->pack);

    if (index < 0)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindBossWallpaper, NULL, nGCCommonLinkIDWallpaperEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    gcAddGObjDisplay
    (
        effect_gobj,
        effect->proc_display,
        plan->dl_link,
        GOBJ_PRIORITY_DEFAULT,
        plan->camera_tag
    );
    dcBossSetupBackgroundDObjs(effect_gobj, &st->boss_models[index],
                               st->boss_billboard[index],
                               anim->matanim_alt);

    if (DObjGetStruct(effect_gobj) == NULL)
    {
        gcEjectGObjIfMade(effect_gobj);
        return NULL;
    }
    gcSetAllAnimSpeed(effect_gobj, anim->anim_speed);
    gcAddGObjProcess(effect_gobj, effect->proc_update, nGCProcessKindFunc, 1);

    dobj = DObjGetStruct(effect_gobj);

    /* The decomp hangs both tables here; on this port the MatAnimJoint
     * rides in the pack and went on with the tree above (its alt), so
     * only the AnimJoint half is left -- the same split
     * src/dc/efground.c's dcGroundMakeEffect makes, and the reason
     * Stage.ground_assets[].matanim_joint is NULL there too. */
    if (anim->has_anim)
    {
        lbCommonAddTreeDObjsAnimAll(dobj, st->boss_anim_tables[index], NULL,
                                    0.0F);
        gcPlayAnimAll(effect_gobj);
    }
    if (dobj->child != NULL)
    {
        dobj->child->user_data.s = syUtilsRandIntRange
        (
            (
                ARRAY_COUNT(dSC1PGameBossCometEnvColorR) +
                ARRAY_COUNT(dSC1PGameBossCometEnvColorG) +
                ARRAY_COUNT(dSC1PGameBossCometEnvColorR)
            ) / 3
        );
    }
    dobj->user_data.s = dobj_color_id;
    effect_gobj->user_data.s = 0;

    return effect_gobj;
}

// 0x801924E0
void sc1PGameBossAdvanceWallpaper(void)
{
    GObj *gobj;
    s32 anim_id;
    s32 effect_id;
    s32 plan_id;
    s32 i, j, k;

    for (i = j = k = plan_id = 0; i < sSC1PGameBossMain.bosswallpaper->loop_count; i++, j++)
    {
        if (sSC1PGameBossMain.bosswallpaper->is_random_wallpaper == TRUE)
        {
            effect_id = syUtilsRandIntRange(sSC1PGameBossMain.bosswallpaper->effect_count);
            anim_id   = syUtilsRandIntRange(sSC1PGameBossMain.bosswallpaper->anim_count);

            if (j == sSC1PGameBossMain.bosswallpaper->bossplan[k].unk_sc1pbossplan_0x0)
            {
                plan_id++, k++;
                j = 0;
            }
        }
        else
        {
            effect_id = anim_id = i;

            if (j == sSC1PGameBossMain.bosswallpaper->bossplan[k].unk_sc1pbossplan_0x0)
            {
                plan_id++, k++;
                j = 0;
            }
        }
        gobj = sc1PGameBossMakeWallpaperEffect(effect_id, anim_id, plan_id);

        if (gobj != NULL)
        {
            sc1PGameBossSetWallpaperTranslate(gobj, plan_id);
        }
    }
#ifdef DB_BOSS_WALLPAPER
    {
        s32 boss = 0, tree = 0, joints = 0, other = 0;
        GObj *g = gGCCommonLinks[nGCCommonLinkIDWallpaperEffect];

        for (; g != NULL; g = g->link_next)
        {
            if (g->id != nGCCommonKindBossWallpaper)
            {
                other++;
                continue;
            }
            boss++;
            if (DObjGetStruct(g) != NULL)
            {
                DObj *d = DObjGetStruct(g);
                tree++;
                for (; d != NULL; d = d->child) joints++;
            }
        }
        dbglog(DBG_INFO, "db: boss row %ld: loop %ld -> %ld boss gobj(s), "
                         "%ld with a tree, %ld joint(s) on the chain, "
                         "%ld other\n",
               (long)sSC1PGameBossMain.wallpaper_id,
               (long)sSC1PGameBossMain.bosswallpaper->loop_count,
               (long)boss, (long)tree, (long)joints, (long)other);
    }
#endif
    sSC1PGameBossMain.is_skip_wallpaper_change = TRUE;
    sSC1PGameBossMain.wallpaper_id++;
    sSC1PGameBossMain.change_wait = sSC1PGameBossMain.bosswallpaper->change_wait_base;
}

#ifdef DB_BOSS_TOUR
/* -DDB_BOSS_TOUR (with -DDB_BOOT_SCENE=nSCKind1PGame
 * -DDB_BOOT_1P_STAGE=nSC1PGameStageBoss): the stage by itself, looped
 * through every background phase. A debugging aid, not the game.
 *
 * The rows advance on the boss's stock_damage_all (past 90, then past
 * 180; row 2's own effects change past 270) and the last on the defeat
 * (sc1PGameBossSetChangeWallpaper), so this plays what a fight would:
 * 10 s on each of damage 0, 91, 181 and 271, 10 s on the defeat row,
 * then it clears the effects and starts the rows over. The fighters are
 * hidden and made untouchable so nothing but the background is on
 * screen and the match cannot end under it. */
#define DC_BOSS_TOUR_PHASE_TICS 600

static void dcBossTourTick(void)
{
    static s32 tour_tic;
    static const s32 damage[] = { 0, 91, 181, 271, 271 };
    s32 phase = (tour_tic / DC_BOSS_TOUR_PHASE_TICS) % 5;
    GObj *g;

    if (gSCManagerBattleState->game_status != nSCBattleGameStatusGo)
    {
        return;         /* the clock starts at GO, so a capture can follow it */
    }
    for (g = gGCCommonLinks[nGCCommonLinkIDFighter]; g != NULL; g = g->link_next)
    {
        g->flags = GOBJ_FLAG_HIDDEN;
        ftGetStruct(g)->special_hitstatus = nGMHitStatusInvincible;
    }
    if ((tour_tic % DC_BOSS_TOUR_PHASE_TICS) == 0)
    {
        if (phase == 0 && tour_tic != 0)
        {
            GObj *next;

            for (g = gGCCommonLinks[nGCCommonLinkIDWallpaperEffect]; g != NULL; g = next)
            {
                next = g->link_next;
                if (g->id == nGCCommonKindBossWallpaper)
                {
                    gcEjectGObj(g);
                }
            }
            sSC1PGameBossMain.wallpaper_id = 0;
            sSC1PGameBossMain.is_skip_wallpaper_change = FALSE;
        }
        gSCManagerBattleState->players[sSC1PGameBossMain.bossplayer].stock_damage_all = damage[phase];
        if (phase == 4)
        {
            sc1PGameBossSetChangeWallpaper();
        }
        dbglog(DBG_INFO, "db: boss tour: tic %ld phase %ld damage %ld row %ld\n",
               (long)tour_tic, (long)phase, (long)damage[phase],
               (long)sSC1PGameBossMain.wallpaper_id);
    }
    tour_tic++;
}
#endif

// 0x80192620
void sc1PGameBossWallpaperProcUpdate(GObj *gobj)
{
#ifdef DB_BOSS_TOUR
    dcBossTourTick();
#endif
    if (sSC1PGameBossMain.is_skip_wallpaper_change == FALSE)
    {
        sc1PGameBossUpdateWallpaperColorID();
        sSC1PGameBossMain.bosswallpaper = &dSC1PGameBossWallpapers[sSC1PGameBossMain.wallpaper_id];
        sc1PGameBossAdvanceWallpaper();
    }
    if (sSC1PGameBossMain.change_wait != -1)
    {
        sSC1PGameBossMain.change_wait--;
    }
    if (sSC1PGameBossMain.bosswallpaper->change_damage_min != -1)
    {
        if (sSC1PGameBossMain.bosswallpaper->change_damage_min < gSCManagerBattleState->players[sSC1PGameBossMain.bossplayer].stock_damage_all)
        {
            sSC1PGameBossMain.is_skip_wallpaper_change = FALSE;
        }
    }
    else if (sSC1PGameBossMain.change_wait == 0)
    {
        sSC1PGameBossMain.is_skip_wallpaper_change = FALSE;
    }
}

// 0x801926F8
void sc1PGameBossSetBossPlayer(void)
{
    s32 player;

    for (player = 0; player < ARRAY_COUNT(gSCManagerBattleState->players); player++)
    {
        if (gSCManagerBattleState->players[player].fkind == nFTKindBoss)
        {
            sSC1PGameBossMain.bossplayer = player;
        }
    }
#ifdef DB_BOSS_WALLPAPER
    dbglog(DBG_INFO, "db: boss wallpaper: boss player %ld\n", (long)sSC1PGameBossMain.bossplayer);
#endif
}

// 0x80192764
void sc1PGameBossInitWallpaper(void)
{
    GObj *gobj;
    Stage *st = stage_bound();

    /* DIVERGES: the decomp has no such guard because the assets are in
     * the overlay it was loaded with. Here they are in the stage pack,
     * and a stage whose .stg predates the BWP1 block simply has no
     * background rather than four rows of NULL trees. */
    if ((st == NULL) || (st->boss_model_count == 0))
    {
        dbglog(DBG_WARNING, "sc1pgameboss: no BWP1 block; no background\n");
        return;
    }
    sSC1PGameBossWallpaperGObj = gobj = gcMakeGObjSPAfter(nGCCommonKindWallpaper, NULL, nGCCommonLinkIDWallpaper, GOBJ_PRIORITY_DEFAULT);

    if (gobj != NULL)
    {
        gcAddGObjProcess(gobj, sc1PGameBossWallpaperProcUpdate, nGCProcessKindFunc, 3);

        sc1PGameBossMakeCamera();
        sc1PGameBossSetBossPlayer();

        sSC1PGameBossMain.is_skip_wallpaper_change = FALSE;
        sSC1PGameBossMain.wallpaper_id = 0;
        /* file_head dropped -- see this file's header, DIVERGES (1). */
        sSC1PGameBossMain.bosswallpaper = &dSC1PGameBossWallpapers[0];
        sSC1PGameBossMain.change_wait = 0;
        sSC1PGameBossWallpaperStepRGBA = 0.0F;
    }
}
