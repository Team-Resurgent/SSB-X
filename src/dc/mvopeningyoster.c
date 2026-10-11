/* mvopeningyoster.c -- see mvopeningyoster.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningyoster.c; the substitutions are
 * explained once, in mvopeningyoster.h's own header comment -- not repeated
 * at every site.
 */
#include "mvopeningyoster.h"
#include "overlay.h"

#include "camanim.h"    /* the camera script */
#include "fighter.h"
#include "objmodel.h"    /* dc_model_add_dobjs, dc_model_proc_display */
#include "ftcommon.h"
#include "lbcommon.h"   /* lbCommonDrawSprite, lbCommonDrawSObjAttr */
#include "scmanager.h"
#include "sprite.h"
#include "efmanager.h"
#include "taskman.h"

#include <ef/efparticle.h>
#include <ft/fighter.h>
#include <ft/ftdef.h>
#include <ft/ftparam.h>
#include <gm/gmsound.h>
#include <sc/scsubsys/scsubsys.h>  /* scSubsysFighterSetStatus/SetLightParams */
#include <sys/controller.h>
#include <sys/objanim.h>           /* gcAddDObjAnimJoint, gcAddCObjCamAnimJoint */
#include <sys/objdef.h>
#include <sys/rdp.h>
#include <lb/lbdef.h>

/* The game reaches the wallpaper as file 93's base plus its link label's
 * offset; the port's file is a sprite bank and the label's value is the
 * offset (mvopeningyamabuki.c does the same). */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* relocData file 93's (StageYoshi) sprite the game names
 * llStageYoshiSprite: the Yoshi's Island backdrop
 * (src/dc/decomp/reloc_data.us.h). */
#define llStageYoshiSprite 0x26C88

/* ---- the pools --------------------------------------------------------
 *
 * Four Yoshis at once plus the nest and ground trees: the decomp's own
 * counts (512 XObjs/DObjs) are short of the port's one-set-per-joint model
 * (mvopeningrun.c), so these are sized as Run's are. */
#define MVOPENINGYOSTER_GOBJS       128
#define MVOPENINGYOSTER_GOBJPROCS   128
#define MVOPENINGYOSTER_XOBJS       768
#define MVOPENINGYOSTER_AOBJS      1200
#define MVOPENINGYOSTER_MOBJS       240
#define MVOPENINGYOSTER_DOBJS       512
#define MVOPENINGYOSTER_SOBJS        16
#define MVOPENINGYOSTER_COBJS         8

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static s32 sMVOpeningYosterTotalTimeTics;
static s32 sMVOpeningYosterUnused0x8013243C;

/* The port's stand-in for sMVOpeningYosterFiles[1] (file 93): the
 * wallpaper sprite, romdisk/mvopeningyoster.spr. Not static so
 * hosttest_ft.c can read it. */
static SpriteBank sMVOpeningYosterWallpaperBank;
void *sMVOpeningYosterWallpaperFileHead;

/* The port's stand-in for the camera script in file 67. */
static CamAnimBank sMVOpeningYosterCamAnimBank;

/* The file's two models as packs (the nest and the ground), their instances, and each one's table of per-joint scripts rebuilt from
 * its pack (ifcommon.c's arrows do the same: gcAddAnimJointAll wants the
 * pointers the file held). */
enum
{
    MVOPENINGYOSTER_MODEL_NEST,
    MVOPENINGYOSTER_MODEL_NESTREST,
    MVOPENINGYOSTER_MODEL_GROUND,
    MVOPENINGYOSTER_MODEL_COUNT
};

#ifndef FT_HOSTTEST
static const char *const sMVOpeningYosterModelNames[MVOPENINGYOSTER_MODEL_COUNT] =
{
    "mvopeningyosternest.mdl",
    "mvopeningyosternestrest.mdl",
    "mvopeningyosterground.mdl"
};
#endif

static Fighter sMVOpeningYosterPacks[MVOPENINGYOSTER_MODEL_COUNT];
static Fighter sMVOpeningYosterModels[MVOPENINGYOSTER_MODEL_COUNT];
static sb32 sMVOpeningYosterModelsLoaded;
static AObjEvent32 *sMVOpeningYosterAnimJoints[MVOPENINGYOSTER_MODEL_COUNT][FIGHTER_MAX_JOINTS];

#ifndef FT_HOSTTEST
static void mvOpeningYosterSetAnimJoint(s32 id)
{
    const Fighter *f = &sMVOpeningYosterPacks[id];
    AObjEvent32 **table = sMVOpeningYosterAnimJoints[id];
    const FPackAnim *anim;
    const s32 *entries;
    u32 i;

    for (i = 0; i < FIGHTER_MAX_JOINTS; i++)
    {
        table[i] = NULL;
    }
    if (f->hd->anim_count == 0)
    {
        return;
    }
    anim = &f->anims[0];
    entries = (const s32 *)((const u8 *)f->blob + anim->off_entries);

    for (i = 0; i < f->hd->joint_count && i < FIGHTER_MAX_JOINTS; i++)
    {
        if (entries[i] >= 0 && (u32) entries[i] < anim->nwords)
        {
            table[i] = (AObjEvent32 *)((u8 *)f->blob + anim->off_words) + entries[i];
        }
    }
}
#endif

/* The two Make* functions below share this: the game's
 * gcSetupCommonDObjs (+ gcAddAnimJointAll + gcPlayAnimAll for the ground),
 * over the pack.
 * DIVERGES: the tree comes from the pack, the table from its rebuilt
 * pointers, the display proc is dc_model_proc_display (both of the game's,
 * gcDrawDObjTreeForGObj and ...DLLinks..., are the walk it does), and the
 * pack build and the attach are FT_HOSTTEST-guarded (the host links no
 * renderer and cannot walk a script). */
static void mvOpeningYosterSetupModel(GObj *gobj, s32 id, sb32 is_anim)
{
#ifndef FT_HOSTTEST
    if (sMVOpeningYosterModelsLoaded != FALSE)
    {
        fighter_clone(&sMVOpeningYosterModels[id], &sMVOpeningYosterPacks[id],
                      syTaskmanMalloc(sizeof(float[4]) *
                                      sMVOpeningYosterPacks[id].hd->vert_count, 0x8));
        dc_model_add_dobjs(gobj, NULL, &sMVOpeningYosterModels[id], NULL);
    }
#else
    gcAddDObjRpyR(gobj, NULL);
#endif
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);

    DObjGetStruct(gobj)->translate.vec.f.x = 0.0F;
    DObjGetStruct(gobj)->translate.vec.f.y = 0.0F;
    DObjGetStruct(gobj)->translate.vec.f.z = 0.0F;

#ifndef FT_HOSTTEST
    if ((is_anim != FALSE) && (sMVOpeningYosterModelsLoaded != FALSE))
    {
        gcAddAnimJointAll(gobj, sMVOpeningYosterAnimJoints[id], 0.0F);
        gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
    }
#endif
}

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningyoster.c:80131B00 mvOpeningYosterFuncLights. Not ported --
 * the standing rule. func_lights is NULL below. */

/* mvopeningyoster.c:80131D38 mvOpeningYosterMakeWallpaper, verbatim but
 * for lbRelocGetFileData's own redefinition above. */
void mvOpeningYosterMakeWallpaper(void)
{
    GObj *wallpaper_gobj;
    SObj *wallpaper_sobj;

    wallpaper_gobj = gcMakeGObjSPAfter(0, NULL, 20, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(wallpaper_gobj, lbCommonDrawSObjAttr, 28, GOBJ_PRIORITY_DEFAULT, ~0);

    wallpaper_sobj = lbCommonMakeSObjForGObj(wallpaper_gobj, lbRelocGetFileData(Sprite*, sMVOpeningYosterWallpaperFileHead, llStageYoshiSprite));
    wallpaper_sobj->pos.x = 10.0F;
    wallpaper_sobj->pos.y = 10.0F;
}

/* mvopeningyoster.c:80131BEC mvOpeningYosterMakeFighters, verbatim but for
 * the figatree heaps (mvopeningmario.h's Substitution 4, NULL). Four
 * Yoshis in his four costumes, each in its own status. */
void mvOpeningYosterMakeFighters(void)
{
    GObj *fighter_gobj;
    s32 status_ids[/* */] =
    {
        0x1000F,
        0x10010,
        0x10011,
        0x10012
    };
    s32 i;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    for (i = 0; i < (s32)ARRAY_COUNT(status_ids); i++)
    {
        desc.fkind = nFTKindYoshi;
        desc.costume = ftParamGetCostumeCommonID(nFTKindYoshi, i);

        desc.pos.x = 0.0F;
        desc.pos.y = 0.0F;
        desc.pos.z = 0.0F;

        desc.figatree_heap = NULL;
        fighter_gobj = ftManagerMakeFighter(&desc);

        scSubsysFighterSetStatus(fighter_gobj, status_ids[i]);

        DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
        DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
        DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;
    }
}

/* mvopeningyoster.c:80131B58 mvOpeningYosterMakeNest. DIVERGES: the nest
 * tree is 73 joints and a pack holds FIGHTER_MAX_JOINTS (40), so it is
 * two packs -- entries 0-38 and the root, entry 4 and 39-72
 * (tools/export/ssb_scenemodelexport.py --entries) -- on two GObjs at the same
 * origin, which draw exactly what the one did. */
void mvOpeningYosterMakeNest(void)
{
    mvOpeningYosterSetupModel(gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT),
                              MVOPENINGYOSTER_MODEL_NEST, FALSE);
    mvOpeningYosterSetupModel(gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT),
                              MVOPENINGYOSTER_MODEL_NESTREST, FALSE);
}

/* mvopeningyoster.c:80131DB8 mvOpeningYosterMakeGround. */
void mvOpeningYosterMakeGround(void)
{
    mvOpeningYosterSetupModel(gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT),
                              MVOPENINGYOSTER_MODEL_GROUND, TRUE);
}

/* mvopeningyoster.c:80131E84 mvOpeningYosterMakeMainCamera. DIVERGES:
 * the script is camanim_get(..., "Cam") and the attach-and-play pair is
 * FT_HOSTTEST-guarded. */
void mvOpeningYosterMakeMainCamera(void)
{
    GObj *camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        80,
        COBJ_MASK_DLLINK(18) | COBJ_MASK_DLLINK(15) |
        COBJ_MASK_DLLINK(10) | COBJ_MASK_DLLINK(9)  |
        COBJ_MASK_DLLINK(6),
        -1,
        TRUE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(camera_gobj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->projection.persp.near = 128.0F;
    cobj->projection.persp.far = 16384.0F;

#ifndef FT_HOSTTEST
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningYosterCamAnimBank, "Cam"), 0.0F);
    gcAddGObjProcess(camera_gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
    gcPlayCamAnim(camera_gobj);
#endif
}

/* mvopeningyoster.c:80131F90 mvOpeningYosterMakeWallpaperCamera.
 * DIVERGES: lbCommonDrawSpriteBackdrop for lbCommonDrawSprite -- this
 * camera's 90 puts the wallpaper under the nest's camera (80) on the
 * N64, and the backdrop band is what puts it under the nest here
 * (src/dc/lbcommon.h). */
void mvOpeningYosterMakeWallpaperCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSpriteBackdrop,
            90,
            COBJ_MASK_DLLINK(28),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mvopeningyoster.c:80132030 mvOpeningYosterInitTotalTimeTics,
 * verbatim. */
void mvOpeningYosterInitTotalTimeTics(void)
{
    sMVOpeningYosterTotalTimeTics = 0;
}

/* mvopeningyoster.c:8013203C mvOpeningYosterMainProc, verbatim. */
void mvOpeningYosterMainProc(GObj *gobj)
{
    (void)gobj;

    sMVOpeningYosterTotalTimeTics++;

    if (sMVOpeningYosterTotalTimeTics >= 10)
    {
        if (sMVOpeningYosterUnused0x8013243C != 0)
        {
            sMVOpeningYosterUnused0x8013243C--;
        }
        if ((scSubsysControllerGetPlayerStickInRangeLR(-15, 15) != FALSE) && (scSubsysControllerGetPlayerStickInRangeUD(-15, 15) != FALSE))
        {
            sMVOpeningYosterUnused0x8013243C = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            syTaskmanSetLoadScene();
        }
        if (sMVOpeningYosterTotalTimeTics == 160)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindOpeningSector;

            syTaskmanSetLoadScene();
        }
    }
}

/* mvopeningyoster.c:80132108 mvOpeningYosterFuncStart. DIVERGES: the
 * reloc loader is the bank/pack loads (mvopeningyoster.h),
 * efManagerInitEffects is Substitution 3b, the figatree-heap malloc is
 * cut (4), the default camera's FILLCOLOR is dropped (fillcolor-camera
 * note: it paints over every 3D list here; the black clear is the same)
 * and so is the trailing tic-sync busy-wait (5). */
void mvOpeningYosterFuncStart(void)
{
    if (sprite_bank_load(&sMVOpeningYosterWallpaperBank, "mvopeningyoster.spr") < 0)
    {
        sMVOpeningYosterWallpaperFileHead = NULL;
    }
    else
    {
        sMVOpeningYosterWallpaperFileHead = &sMVOpeningYosterWallpaperBank;
    }
    camanim_bank_load(&sMVOpeningYosterCamAnimBank, "mvopeningyoster.cam");
#ifndef FT_HOSTTEST
    {
        int pal_bank = 0;
        s32 i;

        sMVOpeningYosterModelsLoaded = TRUE;
        for (i = 0; i < MVOPENINGYOSTER_MODEL_COUNT; i++)
        {
            if (fighter_load_scene(&sMVOpeningYosterPacks[i], sMVOpeningYosterModelNames[i], &pal_bank) != 0)
            {
                sMVOpeningYosterModelsLoaded = FALSE;
                break;
            }
            mvOpeningYosterSetAnimJoint(i);
        }
    }
#endif

    gcMakeGObjSPAfter(0, mvOpeningYosterMainProc, 0, GOBJ_PRIORITY_DEFAULT);
    gcMakeDefaultCameraGObj(0, GOBJ_PRIORITY_DEFAULT, 100, COBJ_FLAG_ZBUFFER, GPACK_RGBA8888(0x00, 0x00, 0x00, 0x00));

    efParticleInitAll();
    mvOpeningYosterInitTotalTimeTics();

    efManagerLoadEffectBank();
    efManagerPreloadModels();
    ftManagerAllocFighter(FTDATA_FLAG_SUBMOTION, 4);
    ftManagerSetupFilesAllKind(nFTKindYoshi);

    mvOpeningYosterMakeMainCamera();
    mvOpeningYosterMakeWallpaperCamera();
    mvOpeningYosterMakeWallpaper();
    mvOpeningYosterMakeNest();
    mvOpeningYosterMakeGround();
    mvOpeningYosterMakeFighters();

    scSubsysFighterSetLightParams(45.0F, 45.0F, 0xFF, 0xFF, 0xFF, 0xFF);

    /* DIVERGES (port only): dc_model_set_demo_opaque(TRUE) -- one camera
     * (80) covers the nest and Yoshi, so Yoshi takes the opaque draw at
     * his real depth against it, as the N64's shared Z buffer does,
     * instead of the layered band the per-fighter panel scenes need.
     * See [[room-logo-layering]] / mvopeningroom.c. */
    dc_model_set_demo_opaque(TRUE);
    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 3230 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(3230);
}

/* mvopeningyoster.c:80132394-shaped dMVOpeningYosterTaskmanSetup. See
 * mvopeningportraits.h for func_draw (scManagerFuncDraw), the zeroed
 * RSP/RDP fields and the pre-render/controller/matrix-list
 * substitutions. */
SYTaskmanSetup dMVOpeningYosterTaskmanSetup =
{
    {
        0,
        gcRunAll,
        scManagerFuncDraw,
        NULL,
        0,
        1,
        2,
        0,
        0,
        0,
        0,
        0,
        2,
        0,
        NULL,
        syControllerFuncRead,
    },

    0,
    sizeof(u64) * 192,
    0,
    0,
    MVOPENINGYOSTER_GOBJPROCS,
    MVOPENINGYOSTER_GOBJS,
    sizeof(GObj),
    MVOPENINGYOSTER_XOBJS,
    NULL,
    NULL,
    MVOPENINGYOSTER_AOBJS,
    MVOPENINGYOSTER_MOBJS,
    MVOPENINGYOSTER_DOBJS,
    sizeof(DObj),
    MVOPENINGYOSTER_SOBJS,
    sizeof(SObj),
    MVOPENINGYOSTER_COBJS,
    sizeof(CObj),

    mvOpeningYosterFuncStart
};

/* mvopeningyoster.c:801322CC mvOpeningYosterStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeningyoster.h). */
void mvOpeningYosterStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningYosterTaskmanSetup);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[45].
 * The packs are given back first (mvopeningcliff.c does the same). */
void mvOpeningYosterOverlayLoad(void)
{
#ifndef FT_HOSTTEST
    s32 i;

    for (i = 0; i < MVOPENINGYOSTER_MODEL_COUNT; i++)
    {
        fighter_release(&sMVOpeningYosterPacks[i]);
    }
#endif
    OVERLAY_CLEAR(sMVOpeningYosterTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningYosterUnused0x8013243C);
    OVERLAY_CLEAR(sMVOpeningYosterWallpaperBank);
    OVERLAY_CLEAR(sMVOpeningYosterWallpaperFileHead);
    OVERLAY_CLEAR(sMVOpeningYosterCamAnimBank);
    OVERLAY_CLEAR(sMVOpeningYosterPacks);
    OVERLAY_CLEAR(sMVOpeningYosterModels);
    OVERLAY_CLEAR(sMVOpeningYosterModelsLoaded);
    OVERLAY_CLEAR(sMVOpeningYosterAnimJoints);
}
