/* mvopeningyamabuki.c -- see mvopeningyamabuki.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningyamabuki.c; the substitutions are
 * explained once, in mvopeningyamabuki.h's own header comment -- not repeated
 * at every site.
 */
#include "mvopeningyamabuki.h"
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

/* The game reaches the wallpaper as file 71's base plus its link label's
 * offset; the port's file is a sprite bank and the label's value is the
 * offset (mvopeningcliff.c does the same). */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* relocData file 71's one Sprite: llMVOpeningYamabukiWallpaperSprite
 * (src/dc/decomp/reloc_data.us.h). */
#define llMVOpeningYamabukiWallpaperSprite 0x3EE58

/* ---- the pools -------------------------------------------------------- */
#define MVOPENINGYAMABUKI_GOBJS       128
#define MVOPENINGYAMABUKI_GOBJPROCS   128
#define MVOPENINGYAMABUKI_XOBJS       512
#define MVOPENINGYAMABUKI_AOBJS       800
#define MVOPENINGYAMABUKI_MOBJS       128
#define MVOPENINGYAMABUKI_DOBJS       256
#define MVOPENINGYAMABUKI_SOBJS        16
#define MVOPENINGYAMABUKI_COBJS         8

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static s32 sMVOpeningYamabukiTotalTimeTics;
static GObj *sMVOpeningYamabukiFighterGObj;
static s32 sMVOpeningYamabukiUnused0x8013249C;

/* The port's stand-in for sMVOpeningYamabukiFiles[0] (file 71): the
 * wallpaper sprite, romdisk/mvopeningyamabuki.spr. Not static so
 * hosttest_ft.c can read it. */
static SpriteBank sMVOpeningYamabukiWallpaperBank;
void *sMVOpeningYamabukiWallpaperFileHead;

/* The port's stand-in for the camera script in file 71. */
static CamAnimBank sMVOpeningYamabukiCamAnimBank;

/* The file's three models as packs (the legs, their shadow, the Poke Ball),
 * their instances, and each one's table of per-joint scripts rebuilt from
 * its pack (ifcommon.c's arrows do the same: gcAddAnimJointAll wants the
 * pointers the file held). */
enum
{
    MVOPENINGYAMABUKI_MODEL_LEGS,
    MVOPENINGYAMABUKI_MODEL_LEGSSHADOW,
    MVOPENINGYAMABUKI_MODEL_MBALL,
    MVOPENINGYAMABUKI_MODEL_COUNT
};

#ifndef FT_HOSTTEST
static const char *const sMVOpeningYamabukiModelNames[MVOPENINGYAMABUKI_MODEL_COUNT] =
{
    "mvopeningyamabukilegs.mdl",
    "mvopeningyamabukilegsshadow.mdl",
    "mvopeningyamabukimball.mdl"
};
#endif

static Fighter sMVOpeningYamabukiPacks[MVOPENINGYAMABUKI_MODEL_COUNT];
static Fighter sMVOpeningYamabukiModels[MVOPENINGYAMABUKI_MODEL_COUNT];
static sb32 sMVOpeningYamabukiModelsLoaded;
static AObjEvent32 *sMVOpeningYamabukiAnimJoints[MVOPENINGYAMABUKI_MODEL_COUNT][FIGHTER_MAX_JOINTS];

#ifndef FT_HOSTTEST
static void mvOpeningYamabukiSetAnimJoint(s32 id)
{
    const Fighter *f = &sMVOpeningYamabukiPacks[id];
    AObjEvent32 **table = sMVOpeningYamabukiAnimJoints[id];
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

/* The three Make* functions below share this: the game's
 * gcSetupCustomDObjs + gcAddAnimJointAll + gcPlayAnimAll, over the pack.
 * DIVERGES: the tree comes from the pack, the table from its rebuilt
 * pointers, the display proc is dc_model_proc_display (both of the game's,
 * gcDrawDObjTreeForGObj and ...DLLinks..., are the walk it does), and the
 * pack build and the attach are FT_HOSTTEST-guarded (the host links no
 * renderer and cannot walk a script). */
static void mvOpeningYamabukiSetupModel(GObj *gobj, s32 id)
{
#ifndef FT_HOSTTEST
    if (sMVOpeningYamabukiModelsLoaded != FALSE)
    {
        fighter_clone(&sMVOpeningYamabukiModels[id], &sMVOpeningYamabukiPacks[id],
                      syTaskmanMalloc(sizeof(float[4]) *
                                      sMVOpeningYamabukiPacks[id].hd->vert_count, 0x8));
        dc_model_add_dobjs(gobj, NULL, &sMVOpeningYamabukiModels[id], NULL);
    }
#else
    gcAddDObjRpyR(gobj, NULL);
#endif
    gcAddGObjDisplay(gobj, dc_model_proc_display, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    DObjGetStruct(gobj)->translate.vec.f.x = 0.0F;
    DObjGetStruct(gobj)->translate.vec.f.y = 0.0F;
    DObjGetStruct(gobj)->translate.vec.f.z = 0.0F;

#ifndef FT_HOSTTEST
    if (sMVOpeningYamabukiModelsLoaded != FALSE)
    {
        gcAddAnimJointAll(gobj, sMVOpeningYamabukiAnimJoints[id], 0.0F);
        gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
    }
#endif
}

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningyamabuki.c:80131B00 mvOpeningYamabukiFuncLights. Not ported --
 * the standing rule. func_lights is NULL below. */

/* mvopeningyamabuki.c:80131B58 mvOpeningYamabukiMakeWallpaper, verbatim
 * but for lbRelocGetFileData's own redefinition above. */
void mvOpeningYamabukiMakeWallpaper(void)
{
    GObj* gobj;
    SObj* sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 20, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, nGCMatrixKindTraRotRpyRSca, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningYamabukiWallpaperFileHead, llMVOpeningYamabukiWallpaperSprite));
    sobj->pos.x = 0.0F;
    sobj->pos.y = 0.0F;
}

/* mvopeningyamabuki.c:80131BD4 mvOpeningYamabukiMakeFighter, verbatim but
 * for the figatree heap (Substitution 4). */
void mvOpeningYamabukiMakeFighter(void)
{
    GObj* fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = nFTKindPikachu;
    desc.costume = ftParamGetCostumeCommonID(nFTKindPikachu, 0);

    desc.pos.x = 0.0F;
    desc.pos.y = 0.0F;
    desc.pos.z = 0.0F;

    desc.figatree_heap = NULL;
    sMVOpeningYamabukiFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

    scSubsysFighterSetStatus(fighter_gobj, 0x1000F);

    DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;
}

/* mvopeningyamabuki.c:80131CA4 mvOpeningYamabukiMakeLegs. */
void mvOpeningYamabukiMakeLegs(void)
{
    mvOpeningYamabukiSetupModel(gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT),
                                MVOPENINGYAMABUKI_MODEL_LEGS);
}

/* mvopeningyamabuki.c:80131D7C mvOpeningYamabukiMakeLegsShadow. */
void mvOpeningYamabukiMakeLegsShadow(void)
{
    mvOpeningYamabukiSetupModel(gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT),
                                MVOPENINGYAMABUKI_MODEL_LEGSSHADOW);
}

/* mvopeningyamabuki.c:80131E54 mvOpeningYamabukiMakeMBall. */
void mvOpeningYamabukiMakeMBall(void)
{
    mvOpeningYamabukiSetupModel(gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT),
                                MVOPENINGYAMABUKI_MODEL_MBALL);
}

/* mvopeningyamabuki.c:80131F2C mvOpeningYamabukiMakeMainCamera. DIVERGES:
 * the script is camanim_get(..., "Cam") and the attach-and-play pair is
 * FT_HOSTTEST-guarded. */
void mvOpeningYamabukiMakeMainCamera(void)
{
    GObj *camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        80,
        COBJ_MASK_DLLINK(27) |
        COBJ_MASK_DLLINK(9),
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
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningYamabukiCamAnimBank, "Cam"), 0.0F);
    gcAddGObjProcess(camera_gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
#endif
}

/* mvopeningyamabuki.c:80132030 mvOpeningYamabukiMakeWallpaperCamera.
 * DIVERGES: lbCommonDrawSpriteBackdrop for lbCommonDrawSprite -- this
 * camera's 90 puts the wallpaper under the models' camera (80) on the
 * N64, and the backdrop band is what puts it under them here
 * (src/dc/lbcommon.h). */
void mvOpeningYamabukiMakeWallpaperCamera(void)
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

/* mvopeningyamabuki.c:801320D0 mvOpeningYamabukiInitTotalTimeTics,
 * verbatim. */
void mvOpeningYamabukiInitTotalTimeTics(void)
{
    sMVOpeningYamabukiTotalTimeTics = 0;
}

/* mvopeningyamabuki.c:801320DC mvOpeningYamabukiFuncRun, verbatim. */
void mvOpeningYamabukiFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningYamabukiTotalTimeTics++;

    if (sMVOpeningYamabukiTotalTimeTics >= 10)
    {
        if (sMVOpeningYamabukiUnused0x8013249C != 0)
        {
            sMVOpeningYamabukiUnused0x8013249C--;
        }
        if ((scSubsysControllerGetPlayerStickInRangeLR(-15, 15) != FALSE) && (scSubsysControllerGetPlayerStickInRangeUD(-15, 15) != FALSE))
        {
            sMVOpeningYamabukiUnused0x8013249C = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            syTaskmanSetLoadScene();
        }
        if (sMVOpeningYamabukiTotalTimeTics == 160)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindOpeningJungle;

            syTaskmanSetLoadScene();
        }
    }
}

/* mvopeningyamabuki.c:801321A8 mvOpeningYamabukiFuncStart. DIVERGES: the
 * reloc loader is the bank/pack loads (mvopeningyamabuki.h),
 * efManagerInitEffects is Substitution 3b, the figatree-heap malloc is
 * cut (4), the default camera's FILLCOLOR is dropped (fillcolor-camera
 * note: it paints over every 3D list here; the black clear is the same)
 * and so is the trailing tic-sync busy-wait (5). */
void mvOpeningYamabukiFuncStart(void)
{
    if (sprite_bank_load(&sMVOpeningYamabukiWallpaperBank, "mvopeningyamabuki.spr") < 0)
    {
        sMVOpeningYamabukiWallpaperFileHead = NULL;
    }
    else
    {
        sMVOpeningYamabukiWallpaperFileHead = &sMVOpeningYamabukiWallpaperBank;
    }
    camanim_bank_load(&sMVOpeningYamabukiCamAnimBank, "mvopeningyamabuki.cam");
#ifndef FT_HOSTTEST
    {
        int pal_bank = 0;
        s32 i;

        sMVOpeningYamabukiModelsLoaded = TRUE;
        for (i = 0; i < MVOPENINGYAMABUKI_MODEL_COUNT; i++)
        {
            if (fighter_load_scene(&sMVOpeningYamabukiPacks[i], sMVOpeningYamabukiModelNames[i], &pal_bank) != 0)
            {
                sMVOpeningYamabukiModelsLoaded = FALSE;
                break;
            }
            mvOpeningYamabukiSetAnimJoint(i);
        }
    }
#endif

    gcMakeGObjSPAfter(0, mvOpeningYamabukiFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
    gcMakeDefaultCameraGObj(0, GOBJ_PRIORITY_DEFAULT, 100, COBJ_FLAG_ZBUFFER, GPACK_RGBA8888(0x00, 0x00, 0x00, 0xFF));

    efParticleInitAll();
    mvOpeningYamabukiInitTotalTimeTics();

    efManagerLoadEffectBank();
    efManagerPreloadModels();
    ftManagerAllocFighter(FTDATA_FLAG_SUBMOTION, 1);
    ftManagerSetupFilesAllKind(nFTKindPikachu);

    mvOpeningYamabukiMakeMainCamera();
    mvOpeningYamabukiMakeWallpaperCamera();
    mvOpeningYamabukiMakeWallpaper();
    mvOpeningYamabukiMakeFighter();
    mvOpeningYamabukiMakeLegs();
    mvOpeningYamabukiMakeLegsShadow();
    mvOpeningYamabukiMakeMBall();

    scSubsysFighterSetLightParams(45.0F, 45.0F, 0xFF, 0xFF, 0xFF, 0xFF);

    /* DIVERGES (port only): dc_model_set_demo_opaque(TRUE) -- one camera
     * (80) covers the models and Pikachu, so Pikachu takes the opaque
     * draw at his real depth against them, as the N64's shared Z buffer
     * does, instead of the layered band the per-fighter panel scenes
     * need. See [[room-logo-layering]] / mvopeningroom.c. */
    dc_model_set_demo_opaque(TRUE);
    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 2690 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(2690);
}

/* mvopeningyamabuki.c:801323F4-shaped dMVOpeningYamabukiTaskmanSetup. See
 * mvopeningportraits.h for func_draw (scManagerFuncDraw), the zeroed
 * RSP/RDP fields and the pre-render/controller/matrix-list
 * substitutions. */
SYTaskmanSetup dMVOpeningYamabukiTaskmanSetup =
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
    MVOPENINGYAMABUKI_GOBJPROCS,
    MVOPENINGYAMABUKI_GOBJS,
    sizeof(GObj),
    MVOPENINGYAMABUKI_XOBJS,
    NULL,
    NULL,
    MVOPENINGYAMABUKI_AOBJS,
    MVOPENINGYAMABUKI_MOBJS,
    MVOPENINGYAMABUKI_DOBJS,
    sizeof(DObj),
    MVOPENINGYAMABUKI_SOBJS,
    sizeof(SObj),
    MVOPENINGYAMABUKI_COBJS,
    sizeof(CObj),

    mvOpeningYamabukiFuncStart
};

/* mvopeningyamabuki.c:80132344 mvOpeningYamabukiStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeningyamabuki.h). */
void mvOpeningYamabukiStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningYamabukiTaskmanSetup);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[48].
 * The packs are given back first (mvopeningcliff.c does the same). */
void mvOpeningYamabukiOverlayLoad(void)
{
#ifndef FT_HOSTTEST
    s32 i;

    for (i = 0; i < MVOPENINGYAMABUKI_MODEL_COUNT; i++)
    {
        fighter_release(&sMVOpeningYamabukiPacks[i]);
    }
#endif
    OVERLAY_CLEAR(sMVOpeningYamabukiTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningYamabukiFighterGObj);
    OVERLAY_CLEAR(sMVOpeningYamabukiUnused0x8013249C);
    OVERLAY_CLEAR(sMVOpeningYamabukiWallpaperBank);
    OVERLAY_CLEAR(sMVOpeningYamabukiWallpaperFileHead);
    OVERLAY_CLEAR(sMVOpeningYamabukiCamAnimBank);
    OVERLAY_CLEAR(sMVOpeningYamabukiPacks);
    OVERLAY_CLEAR(sMVOpeningYamabukiModels);
    OVERLAY_CLEAR(sMVOpeningYamabukiModelsLoaded);
    OVERLAY_CLEAR(sMVOpeningYamabukiAnimJoints);
}
