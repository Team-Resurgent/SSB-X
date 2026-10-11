/* mvopeningjungle.c -- see mvopeningjungle.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningjungle.c; every function names
 * its line range. This scene reuses mvopeningmario.c's whole recipe
 * (mvopeningjungle.h's own header explains what actually differs) --
 * not repeated at every site.
 */
#include "mvopeningjungle.h"
#include "overlay.h"

#include "camanim.h"    /* the camera script */
#include "ftcommon.h"
#include "gmcamera.h"   /* gmCameraMakeDefaultCamera, GMCAMERA_BATTLE_DLLINK_MASK */
#include "lbcommon.h"   /* lbCommonDrawSprite, lbCommonDrawSObjAttr */
#include "scmanager.h"
#include "sprite.h"
#include "stage.h"      /* grStageAcquire/grStageRelease, stage_bind, stage_bind_collision */
#include "objpvr.h"     /* gcEjectGObjIfMade */
#include "efmanager.h"
#include "taskman.h"

#include <ef/efparticle.h>
#include <if/interface.h>          /* ifScreenFlashMakeInterface */
#include <ft/fighter.h>
#include <ft/ftdef.h>
#include <ft/ftparam.h>
#include <ft/ftpublic.h>
#include <gm/gmrumble.h>
#include <gm/gmsound.h>
#include <gr/ground.h>
#include <it/itmanager.h>          /* itManagerInitItems */
#include <mp/mpdef.h>              /* nMPMapObjKindMoviePlayer1 */
#include <sc/scsubsys/scsubsys.h>  /* scSubsysFighterSetStatus */
#include <sys/controller.h>
#include <sys/objanim.h>           /* gcAddCObjCamAnimJoint, gcPlayCamAnim */
#include <sys/rdp.h>
#include <wp/wpmanager.h>          /* wpManagerAllocWeapons */
#include <lb/lbdef.h>


/* ---- the pools --------------------------------------------------------
 *
 * One real VS stage and two real fighters (Donkey and Samus), the same
 * budget as every tier-2 scene in this chain (mvopeningmario.h). */
#define MVOPENINGJUNGLE_GOBJS       64
#define MVOPENINGJUNGLE_GOBJPROCS   64
#define MVOPENINGJUNGLE_XOBJS      384
#define MVOPENINGJUNGLE_AOBJS      576
#define MVOPENINGJUNGLE_MOBJS       48
#define MVOPENINGJUNGLE_DOBJS      128
#define MVOPENINGJUNGLE_SOBJS        8
#define MVOPENINGJUNGLE_COBJS        8

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mvopeningjungle.c:8018D8A8, verbatim: the Donkey Kong fighter's canned
 * input. */
FTKeyEvent dMVOpeningJungleDonkeyKeyEvents[] =
{
    FTKEY_EVENT_STICK(0, -I_CONTROLLER_RANGE_MAX, 0),
    FTKEY_EVENT_BUTTON(B_BUTTON, 1),
    FTKEY_EVENT_BUTTON(0, 1),
    FTKEY_EVENT_STICK(0, 0, 50),
    FTKEY_EVENT_STICK(15, I_CONTROLLER_RANGE_MAX, 30),
    FTKEY_EVENT_STICK(I_CONTROLLER_RANGE_MAX, 0, 10),
    FTKEY_EVENT_BUTTON(A_BUTTON, 1),
    FTKEY_EVENT_BUTTON(0, 1),
    FTKEY_EVENT_STICK(I_CONTROLLER_RANGE_MAX, 0, 60),
    FTKEY_EVENT_STICK(-30, 0, 3),
    FTKEY_EVENT_STICK(0, 0, 10),
    FTKEY_EVENT_STICK(-I_CONTROLLER_RANGE_MAX, 0, 1),
    FTKEY_EVENT_BUTTON(B_BUTTON, 1),
    FTKEY_EVENT_BUTTON(0, 1),
    FTKEY_EVENT_BUTTON(B_BUTTON, 1),
    FTKEY_EVENT_BUTTON(0, 1),
    FTKEY_EVENT_STICK(0, 0, 1),
    FTKEY_EVENT_END()
};

/* mvopeningjungle.c:8018D8F0, verbatim: Samus's. */
FTKeyEvent dMVOpeningJungleSamusKeyEvents[] =
{
    FTKEY_EVENT_STICK(-I_CONTROLLER_RANGE_MAX, 0, 20),
    FTKEY_EVENT_STICK(0, 0, 75),
    FTKEY_EVENT_BUTTON(Z_TRIG, 1),
    FTKEY_EVENT_STICK(-I_CONTROLLER_RANGE_MAX, 0, 60),
    FTKEY_EVENT_STICK(0, 0, 1),
    FTKEY_EVENT_STICK(-I_CONTROLLER_RANGE_MAX, 0, 5),
    FTKEY_EVENT_STICK(0, 0, 1),
    FTKEY_EVENT_STICK(0, 0, 23),
    FTKEY_EVENT_STICK(-I_CONTROLLER_RANGE_MAX, 0, 5),
    FTKEY_EVENT_BUTTON(0, 1),
    FTKEY_EVENT_STICK(0, 0, 40),
    FTKEY_EVENT_STICK(30, 0, 3),
    FTKEY_EVENT_STICK(0, 0, 1),
    FTKEY_EVENT_BUTTON(B_BUTTON, 1),
    FTKEY_EVENT_BUTTON(0, 1),
    FTKEY_EVENT_BUTTON(B_BUTTON, 1),
    FTKEY_EVENT_END()
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static s32 sMVOpeningJungleTotalTimeTics;
static GObj *sMVOpeningJungleFighterGObj;
static GObj *sMVOpeningJungleStageCameraGObj;
static SCBattleState sMVOpeningJungleBattleState;

/* The port's stand-in for sMVOpeningJungleFiles[1] (file 64): the whole of
 * what the scene wanted out of it is the camera's script,
 * romdisk/mvopeningjungle.cam (bank OpeningJungle). The other file,
 * IFCommonAnnounceCommon, was only the name letters this scene does not
 * draw. */
static CamAnimBank sMVOpeningJungleCamAnimBank;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningjungle.c:8018D0C0 mvOpeningJungleSetupFiles. DIVERGES: the
 * bank load in place of lbRelocInitSetup/lbRelocLoadFilesListed. */
void mvOpeningJungleSetupFiles(void)
{
    camanim_bank_load(&sMVOpeningJungleCamAnimBank, "mvopeningjungle.cam");
}

/* mvopeningjungle.c:8018D168 mvOpeningJungleMakeGroundViewport. DIVERGES:
 * gmCameraMakeMovieCamera(NULL) is the decomp's own call (its kind 8 is
 * the roll look-at, objdisplay.c gcCameraLookAtF), and camera_mask is
 * set right after it; the two unused CObjDesc copies are cut with their statics; the
 * script is camanim_get(..., "Cam"), and the attach and the priming
 * gcPlayCamAnim are FT_HOSTTEST-guarded (the host cannot walk a script). */
void mvOpeningJungleMakeGroundViewport(Vec3f unused)
{
    CObj *cobj;

    (void)unused;

    sMVOpeningJungleStageCameraGObj = gmCameraMakeMovieCamera(NULL);
    gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK;

    cobj = CObjGetStruct(sMVOpeningJungleStageCameraGObj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->projection.persp.aspect = 15.0F / 11.0F;

    gcEndProcessAll(sMVOpeningJungleStageCameraGObj);

    cobj->projection.persp.near = 50.0F;
    cobj->projection.persp.far = 15000.0F;

#ifdef FT_HOSTTEST
    /* the host cannot walk a script; see mvopeningmario.c's own note */
#else
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningJungleCamAnimBank, "Cam"), 0.0F);
    gcAddGObjProcess(sMVOpeningJungleStageCameraGObj, gcPlayCamAnim, nGCProcessKindFunc, 1);
    gcPlayCamAnim(sMVOpeningJungleStageCameraGObj);
#endif
}

/* mvopeningjungle.c:8018D2DC mvOpeningJungleMakeFighters. DIVERGES:
 * grWallpaperMakeDecideKind()/grCommonSetupInitAll() move to after the
 * camera (the ordinary Substitution 3 reordering); the rest is verbatim. */
void mvOpeningJungleMakeFighters(void)
{
    GObj *fighter_gobj;
    s32 i;
    s32 pos_ids[2];
    Vec3f spawn_position[2];
    FTStruct *fp;

    if (mpCollisionGetMapObjCountKind(nMPMapObjKindMoviePlayer2) != 1)
    {
        while (TRUE)
        {
            syDebugPrintf("wrong number of mapobject\n");
            scManagerRunPrintGObjStatus();
        }
    }
    mpCollisionGetMapObjIDsKind(nMPMapObjKindMoviePlayer2, &pos_ids[1]);
    mpCollisionGetMapObjPositionID(pos_ids[1], &spawn_position[1]);

    if (mpCollisionGetMapObjCountKind(nMPMapObjKindMoviePlayer3) != 1)
    {
        while (TRUE)
        {
            syDebugPrintf("wrong number of mapobject\n");
            scManagerRunPrintGObjStatus();
        }
    }
    mpCollisionGetMapObjIDsKind(nMPMapObjKindMoviePlayer3, &pos_ids[0]);
    mpCollisionGetMapObjPositionID(pos_ids[0], &spawn_position[0]);

    spawn_position[0].x += 1100.0F;

    mvOpeningJungleMakeGroundViewport(spawn_position[1]);

    grWallpaperMakeDecideKind();
    grCommonSetupInitAll();

    gmRumbleMakeActor();
    ftPublicMakeActor();

    for (i = 0; i < (s32)ARRAY_COUNT(gSCManagerBattleState->players); i++)
    {
        FTDesc desc = dFTManagerDefaultFighterDesc;

        if (gSCManagerBattleState->players[i].pkind == nFTPlayerKindNot)
        {
            continue;
        }
        ftManagerSetupFilesAllKind(gSCManagerBattleState->players[i].fkind);

        desc.fkind = gSCManagerBattleState->players[i].fkind;

        if (gSCManagerBattleState->players[i].fkind == nFTKindDonkey)
        {
            desc.pos.x = spawn_position[1].x;
            desc.pos.y = spawn_position[1].y;
            desc.pos.z = spawn_position[1].z;
            desc.lr = +1;
            desc.damage = 200;
        }
        else
        {
            desc.pos.x = spawn_position[0].x;
            desc.pos.y = spawn_position[0].y;
            desc.pos.z = spawn_position[0].z;
            desc.lr = -1;
            desc.damage = 40;
        }
        desc.team = gSCManagerBattleState->players[i].team;
        desc.player = i;
        desc.detail = nFTPartsDetailHigh;
        desc.costume = gSCManagerBattleState->players[i].costume;
        desc.handicap = gSCManagerBattleState->players[i].handicap;
        desc.level = gSCManagerBattleState->players[i].level;
        desc.stock_count = gSCManagerBattleState->stocks;
        desc.pkind = gSCManagerBattleState->players[i].pkind;
        desc.controller = &gSYControllerDevices[i];
        desc.figatree_heap = ftManagerAllocFigatreeHeapKind(gSCManagerBattleState->players[i].fkind);

        sMVOpeningJungleFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

        if (gSCManagerBattleState->players[i].fkind == nFTKindDonkey)
        {
            fp = ftGetStruct(fighter_gobj);
            fp->passive_vars.donkey.charge_level = 9;
        }
        else
        {
            fp = ftGetStruct(fighter_gobj);
            fp->passive_vars.samus.charge_level = 6;
        }
        ftParamInitPlayerBattleStats(i, fighter_gobj);

        if (gSCManagerBattleState->players[i].fkind == nFTKindDonkey)
        {
            ftParamSetKey(fighter_gobj, dMVOpeningJungleDonkeyKeyEvents);
        }
#ifndef FT_HOSTTEST
        else ftParamSetKey(fighter_gobj, dMVOpeningJungleSamusKeyEvents);
#else
        /* Samus's script presses Z at tic 95 (a guard); the host's mock
         * pack has no shield motion to run and the status spins, so the
         * host leaves her without input. Donkey's runs in full. */
#endif
    }
}

/* mvopeningjungle.c:8018D5E4 mvOpeningJungleFuncRun, verbatim. */
void mvOpeningJungleFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningJungleTotalTimeTics++;

    if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindTitle;

        syTaskmanSetLoadScene();
    }
    if (sMVOpeningJungleTotalTimeTics == 320)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindOpeningYoster;

        syTaskmanSetLoadScene();
    }
}

/* mvopeningjungle.c:8018D670 mvOpeningJungleFuncStart. DIVERGES:
 * mvopeningmario.h's Substitutions 1-4 at their decomp call sites: the
 * stage acquire in place of mpCollisionInitGroundData /
 * gmCameraMakeWallpaperCamera, the default (FILLCOLOR) camera cut,
 * efManagerLoadEffectBank/PreloadModels in place of efManagerInitEffects,
 * and the trailing busy-wait cut. */
void mvOpeningJungleFuncStart(void)
{
    Stage *stage;

    sMVOpeningJungleBattleState = dSCManagerDefaultBattleState;
    gSCManagerBattleState = &sMVOpeningJungleBattleState;

    gSCManagerBattleState->game_type = nSCBattleGameTypeMovie;

    gSCManagerBattleState->gkind = nGRKindJungle;
    gSCManagerBattleState->pl_count = 1;

    gSCManagerBattleState->players[0].fkind = nFTKindDonkey;
    gSCManagerBattleState->players[0].pkind = nFTPlayerKindKey;

    gSCManagerBattleState->players[1].fkind = nFTKindSamus;
    gSCManagerBattleState->players[1].pkind = nFTPlayerKindKey;

    mvOpeningJungleSetupFiles();

    gcMakeGObjSPAfter(nGCCommonKindMovie, mvOpeningJungleFuncRun, 13, GOBJ_PRIORITY_DEFAULT);

    efParticleInitAll();
    ftParamInitGame();

    stage = grStageAcquire(gSCManagerBattleState->gkind);
    if (stage != NULL)
    {
        stage_bind(stage);
    }
    stage_bind_collision();

    gmCameraSetViewportDimensions(10, 10, 310, 230);

    ftManagerAllocFighter(FTDATA_FLAG_MAINMOTION, 2);
    wpManagerAllocWeapons();
    itManagerInitItems();

    efManagerLoadEffectBank();
    efManagerPreloadModels();

    ifScreenFlashMakeInterface(0xFF);

    mvOpeningJungleMakeFighters();

    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 2880 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(2880);
}

/* mvopeningjungle.c:8018D7CC mvOpeningJungleFuncLights. Not ported -- the
 * standing rule. func_lights is NULL below. */

/* mvopeningjungle.c:8018D958-shaped dMVOpeningJungleTaskmanSetup. See the
 * pools note above for the counts, and mvopeningportraits.h for
 * func_draw (scManagerFuncDraw), the zeroed RSP/RDP fields and the
 * pre-render/controller/matrix-list substitutions. */
SYTaskmanSetup dMVOpeningJungleTaskmanSetup =
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
    MVOPENINGJUNGLE_GOBJPROCS,
    MVOPENINGJUNGLE_GOBJS,
    sizeof(GObj),
    MVOPENINGJUNGLE_XOBJS,
    NULL,
    NULL,
    MVOPENINGJUNGLE_AOBJS,
    MVOPENINGJUNGLE_MOBJS,
    MVOPENINGJUNGLE_DOBJS,
    sizeof(DObj),
    MVOPENINGJUNGLE_SOBJS,
    sizeof(SObj),
    MVOPENINGJUNGLE_COBJS,
    sizeof(CObj),

    mvOpeningJungleFuncStart
};

/* mvopeningjungle.c:8018D818 mvOpeningJungleStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeningjungle.h), and the stage release
 * below -- mvopeningmario.c's own StartScene says why it is there and
 * why the kind is a constant. */
void mvOpeningJungleStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningJungleTaskmanSetup);

    grStageRelease(nGRKindJungle);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[51]. */
void mvOpeningJungleOverlayLoad(void)
{
    OVERLAY_CLEAR(sMVOpeningJungleTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningJungleFighterGObj);
    OVERLAY_CLEAR(sMVOpeningJungleStageCameraGObj);
    OVERLAY_CLEAR(sMVOpeningJungleBattleState);
    OVERLAY_CLEAR(sMVOpeningJungleCamAnimBank);
}
