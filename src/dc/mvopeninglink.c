/* mvopeninglink.c -- see mvopeninglink.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeninglink.c; the substitutions are
 * explained once, in mvopeninglink.h's own header comment (which also
 * names mvopeningmario.h's substitutions 1-5 and mvopeningdonkey.h's
 * substitution 6, all reused here unchanged) -- not repeated at every
 * site.
 */
#include "mvopeninglink.h"
#include "overlay.h"

#include "camanim.h"    /* the Link cam-anim script */
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
#include <sys/objdef.h>            /* nGCCommonLinkIDSceneCamera/Movie/Camera */
#include <sys/rdp.h>
#include <wp/wpmanager.h>          /* wpManagerAllocWeapons */
#include <lb/lbdef.h>

/* The game reaches the name letters as file 37's base plus the offset
 * its link label took; the port's file is a bank and the label's value
 * is the offset (src/dc/scvsresults.c does the same for its own copy of
 * this exact file). */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* relocData file 37 (IFCommonAnnounceCommon), the four letters "LINK"
 * spells with. Offsets are src/dc/decomp/reloc_data.us.h's own ll<Name>
 * Sprite link labels -- L and N are new to this scene, I is the exact
 * literal mvopeningmario.c already uses for its own "I", and K is the
 * exact literal mvopeningdonkey.c already uses for its own "K" (same
 * relocData file, same sprite bank, different offset per letter). */
#define llIFCommonAnnounceCommonLetterLSprite 0x03358
#define llIFCommonAnnounceCommonLetterISprite 0x026B8
#define llIFCommonAnnounceCommonLetterNSprite 0x03E88
#define llIFCommonAnnounceCommonLetterKSprite 0x02F98

/* ---- the pools --------------------------------------------------------
 *
 * Reused verbatim from mvopeningmario.h/mvopeningdonkey.h's own note:
 * one real VS stage (Hyrule) and up to two real Link fighters (posed +
 * walking), the same budget as every other scene in this chain. */
#define MVOPENINGLINK_GOBJS       48
#define MVOPENINGLINK_GOBJPROCS   48
#define MVOPENINGLINK_XOBJS      288
#define MVOPENINGLINK_AOBJS      576
#define MVOPENINGLINK_MOBJS       32
#define MVOPENINGLINK_DOBJS       96
#define MVOPENINGLINK_SOBJS        8
#define MVOPENINGLINK_COBJS        8

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mvopeninglink.c:8018E070/E08C, verbatim. */
CObjDesc dMVOpeningLinkStartCObjDesc = { { -800.0F, 180.0F, 800.0F }, { 0.0F, 180.0F, 0.0F }, 0.0F };
CObjDesc dMVOpeningLinkEndCObjDesc = { { 200.0F, 0.0F, 400.0F }, { 0.0F, 240.0F, 0.0F }, 0.4F };

/* mvopeninglink.c:8018E0A8 dMVOpeningLinkKeyEvents, verbatim. */
FTKeyEvent dMVOpeningLinkKeyEvents[] =
{
    FTKEY_EVENT_BUTTON(L_TRIG, 1),
    FTKEY_EVENT_END()
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static s32 sMVOpeningLinkTotalTimeTics;
static GObj *sMVOpeningLinkNameGObj;
static GObj *sMVOpeningLinkFighterGObj;
static GObj *sMVOpeningLinkStageCameraGObj;

/* mvopeningmario.h's own note (Substitution 4): this stays NULL. */
static void *sMVOpeningLinkFigatreeHeap;

static f32 sMVOpeningLinkPosedFighterSpeed;
static CObjDesc sMVOpeningLinkAdjustedStartCObjDesc;
static CObjDesc sMVOpeningLinkAdjustedEndCObjDesc;
static SCBattleState sMVOpeningLinkBattleState;

/* The port's stand-in for sMVOpeningLinkFiles[0]: file 37's sprite
 * bank (romdisk/ifannounce.spr), the same shape
 * sMVOpeningMarioNamesBank/sMVOpeningDonkeyNamesBank already take.
 * sMVOpeningLinkNamesFileHead is not static, so hosttest_ft.c's own
 * openinglink_find can read it. */
static SpriteBank sMVOpeningLinkNamesBank;
void *sMVOpeningLinkNamesFileHead;

/* The port's stand-in for sMVOpeningLinkFiles[1]: the
 * "Link" entry in the shared OpeningCommon camera-anim bank. */
static CamAnimBank sMVOpeningLinkCamAnimBank;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeninglink.c:8018D0C0 mvOpeningLinkSetupFiles. DIVERGES: the same
 * two loads mvopeningmario.c's own Substitution 1 makes, in place of
 * lbRelocInitSetup/lbRelocLoadFilesListed. */
void mvOpeningLinkSetupFiles(void)
{
    if (sprite_bank_load(&sMVOpeningLinkNamesBank, "ifannounce.spr") < 0)
    {
        sMVOpeningLinkNamesFileHead = NULL;
    }
    else
    {
        sMVOpeningLinkNamesFileHead = &sMVOpeningLinkNamesBank;
    }
    camanim_bank_load(&sMVOpeningLinkCamAnimBank, "mvopeningcommon.cam");
}

/* mvopeninglink.c:8018D160 mvOpeningLinkInitName, verbatim. */
void mvOpeningLinkInitName(SObj *sobj)
{
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->envcolor.r = 0xFF;
    sobj->envcolor.g = 0xFF;
    sobj->envcolor.b = 0xFF;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0xFF;
    sobj->sprite.blue = 0xFF;
}

/* mvopeninglink.c:8018D194 mvOpeningLinkMakeName, verbatim but for
 * lbRelocGetFileData's own redefinition above. */
void mvOpeningLinkMakeName(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 i;

    intptr_t offsets[] =
    {
        llIFCommonAnnounceCommonLetterLSprite,
        llIFCommonAnnounceCommonLetterISprite,
        llIFCommonAnnounceCommonLetterNSprite,
        llIFCommonAnnounceCommonLetterKSprite,
        0x0
    };

    f32 pos_x[] =
    {
        0.0F, 30.0F, 45.0F, 80.0F
    };

    sMVOpeningLinkNameGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; offsets[i] != 0x0; i++)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningLinkNamesFileHead, offsets[i]));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = pos_x[i] + 100.0F;
        sobj->pos.y = 100.0F;

        mvOpeningLinkInitName(sobj);
    }
}

/* mvopeninglink.c:8018D2F4 mvOpeningLinkMotionCameraProcUpdate,
 * verbatim. */
void mvOpeningLinkMotionCameraProcUpdate(GObj *camera_gobj)
{
    CObj *cobj = CObjGetStruct(camera_gobj);

    if (sMVOpeningLinkTotalTimeTics >= 15)
    {
        cobj->vec.eye.x += (((sMVOpeningLinkAdjustedEndCObjDesc.eye.x - sMVOpeningLinkAdjustedStartCObjDesc.eye.x) / 45.0F));
        cobj->vec.eye.y += (((sMVOpeningLinkAdjustedEndCObjDesc.eye.y - sMVOpeningLinkAdjustedStartCObjDesc.eye.y) / 45.0F));
        cobj->vec.eye.z += (((sMVOpeningLinkAdjustedEndCObjDesc.eye.z - sMVOpeningLinkAdjustedStartCObjDesc.eye.z) / 45.0F));
        cobj->vec.at.x += (((sMVOpeningLinkAdjustedEndCObjDesc.at.x - sMVOpeningLinkAdjustedStartCObjDesc.at.x) / 45.0F));
        cobj->vec.at.y += (((sMVOpeningLinkAdjustedEndCObjDesc.at.y - sMVOpeningLinkAdjustedStartCObjDesc.at.y) / 45.0F));
        cobj->vec.at.z += (((sMVOpeningLinkAdjustedEndCObjDesc.at.z - sMVOpeningLinkAdjustedStartCObjDesc.at.z) / 45.0F));
        cobj->vec.up.x += (((sMVOpeningLinkAdjustedEndCObjDesc.upx - sMVOpeningLinkAdjustedStartCObjDesc.upx) / 45.0F));
    }
}

/* mvopeninglink.c:8018D3EC mvOpeningLinkMakeMotionCamera. DIVERGES:
 * gmCameraMakeMovieCamera(NULL) is the decomp's own call (its kind 8 is
 * the roll look-at, objdisplay.c gcCameraLookAtF), and camera_mask is
 * set right after it. */
void mvOpeningLinkMakeMotionCamera(Vec3f move)
{
    CObj *cobj;

    sMVOpeningLinkAdjustedStartCObjDesc = dMVOpeningLinkStartCObjDesc;
    sMVOpeningLinkAdjustedEndCObjDesc = dMVOpeningLinkEndCObjDesc;

    sMVOpeningLinkStageCameraGObj = gmCameraMakeMovieCamera(NULL);
    gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK;

    cobj = CObjGetStruct(sMVOpeningLinkStageCameraGObj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 90.0F, 310.0F, 230.0F);

    cobj->projection.persp.aspect = 15.0F / 7.0F;

    gcEndProcessAll(sMVOpeningLinkStageCameraGObj);
    gcAddGObjProcess(sMVOpeningLinkStageCameraGObj, mvOpeningLinkMotionCameraProcUpdate, nGCProcessKindFunc, 1);

    sMVOpeningLinkAdjustedStartCObjDesc.eye.x += move.x;
    sMVOpeningLinkAdjustedStartCObjDesc.eye.y += move.y;
    sMVOpeningLinkAdjustedStartCObjDesc.eye.z += move.z;
    sMVOpeningLinkAdjustedStartCObjDesc.at.x += move.x;
    sMVOpeningLinkAdjustedStartCObjDesc.at.y += move.y;
    sMVOpeningLinkAdjustedStartCObjDesc.at.z += move.z;

    sMVOpeningLinkAdjustedEndCObjDesc.eye.x += move.x;
    sMVOpeningLinkAdjustedEndCObjDesc.eye.y += move.y;
    sMVOpeningLinkAdjustedEndCObjDesc.eye.z += move.z;
    sMVOpeningLinkAdjustedEndCObjDesc.at.x += move.x;
    sMVOpeningLinkAdjustedEndCObjDesc.at.y += move.y;
    sMVOpeningLinkAdjustedEndCObjDesc.at.z += move.z;

    cobj->vec.eye.x = sMVOpeningLinkAdjustedStartCObjDesc.eye.x;
    cobj->vec.eye.y = sMVOpeningLinkAdjustedStartCObjDesc.eye.y;
    cobj->vec.eye.z = sMVOpeningLinkAdjustedStartCObjDesc.eye.z;
    cobj->vec.at.x = sMVOpeningLinkAdjustedStartCObjDesc.at.x;
    cobj->vec.at.y = sMVOpeningLinkAdjustedStartCObjDesc.at.y;
    cobj->vec.at.z = sMVOpeningLinkAdjustedStartCObjDesc.at.z;
    cobj->vec.up.x = sMVOpeningLinkAdjustedStartCObjDesc.upx;
}

/* mvopeninglink.c:8018D5FC mvOpeningLinkMakeMotionWindow. DIVERGES:
 * mpCollisionGetMapObjCountKind/IDsKind/PositionID stay verbatim;
 * grWallpaperMakeDecideKind()/grCommonSetupInitAll() move to after
 * mvOpeningLinkMakeMotionCamera() rather than before --
 * mvopeningmario.h's Substitution 3, reused unchanged. */
void mvOpeningLinkMakeMotionWindow(void)
{
    GObj *fighter_gobj;
    s32 i;
    s32 pos_ids[3];
    Vec3f pos;

    if (mpCollisionGetMapObjCountKind(nMPMapObjKindMoviePlayer1) != 1)
    {
        while (TRUE)
        {
            syDebugPrintf("wrong number of mapobject\n");
            scManagerRunPrintGObjStatus();
        }
    }
    mpCollisionGetMapObjIDsKind(nMPMapObjKindMoviePlayer1, pos_ids);
    mpCollisionGetMapObjPositionID(pos_ids[0], &pos);

    mvOpeningLinkMakeMotionCamera(pos);

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
        desc.pos.x = pos.x;
        desc.pos.y = pos.y;
        desc.pos.z = pos.z;
        desc.lr = +1;
        desc.team = gSCManagerBattleState->players[i].team;
        desc.player = i;
        desc.detail = nFTPartsDetailHigh;
        desc.costume = gSCManagerBattleState->players[i].costume;
        desc.handicap = gSCManagerBattleState->players[i].handicap;
        desc.level = gSCManagerBattleState->players[i].level;
        desc.stock_count = gSCManagerBattleState->stocks;
        desc.damage = 0;
        desc.pkind = gSCManagerBattleState->players[i].pkind;
        desc.controller = &gSYControllerDevices[i];
        desc.figatree_heap = ftManagerAllocFigatreeHeapKind(gSCManagerBattleState->players[i].fkind);

        sMVOpeningLinkFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

        ftParamInitPlayerBattleStats(i, fighter_gobj);
        ftParamSetKey(fighter_gobj, dMVOpeningLinkKeyEvents);
    }
}

/* mvopeninglink.c:8018D824 mvOpeningLinkPosedWallpaperProcDisplay,
 * verbatim. */
void mvOpeningLinkPosedWallpaperProcDisplay(GObj *gobj)
{
    (void)gobj;

    /* DIVERGES: the fill quad in place of the raw gDPFillRectangle, as
     * mvopeningmario.c's panel says why. */
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    lbCommonSpriteFillRect(10, 10, 310, 90, 0x96, 0x78, 0xB4, 0xFF);
}

/* mvopeninglink.c:8018D924 mvOpeningLinkMakePosedWallpaper, verbatim. */
void mvOpeningLinkMakePosedWallpaper(void)
{
    gcAddGObjDisplay
    (
        gcMakeGObjSPAfter
        (
            0,
            NULL,
            19,
            GOBJ_PRIORITY_DEFAULT
        ),
        mvOpeningLinkPosedWallpaperProcDisplay,
        28,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
}

/* mvopeninglink.c:8018D970 mvOpeningLinkPosedFighterProcUpdate,
 * verbatim: the horizontal box moves the posed fighter along X, not Y
 * (mvopeninglink.h's own note). */
void mvOpeningLinkPosedFighterProcUpdate(GObj *fighter_gobj)
{
    switch (sMVOpeningLinkTotalTimeTics)
    {
    default:
        break;

    case 15:
        sMVOpeningLinkPosedFighterSpeed = 17.0F;
        break;

    case 45:
        sMVOpeningLinkPosedFighterSpeed = 15.0F;
        break;

    case 60:
        sMVOpeningLinkPosedFighterSpeed = 0.0F;
        break;
    }
    if ((sMVOpeningLinkTotalTimeTics > 15) && (sMVOpeningLinkTotalTimeTics < 45))
    {
        sMVOpeningLinkPosedFighterSpeed += -1.0F / 15.0F;
    }
    if ((sMVOpeningLinkTotalTimeTics > 45) && (sMVOpeningLinkTotalTimeTics < 60))
    {
        sMVOpeningLinkPosedFighterSpeed += -1.0F;
    }
    DObjGetStruct(fighter_gobj)->translate.vec.f.x -= sMVOpeningLinkPosedFighterSpeed;
}

/* mvopeninglink.c:8018DA40 mvOpeningLinkMakePosedFighter, verbatim:
 * desc.figatree_heap reads sMVOpeningLinkFigatreeHeap, always NULL
 * (mvopeningmario.h's Substitution 4). */
void mvOpeningLinkMakePosedFighter(void)
{
    GObj *fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = nFTKindLink;
    desc.costume = ftParamGetCostumeCommonID(nFTKindLink, 0);
    desc.figatree_heap = sMVOpeningLinkFigatreeHeap;

    desc.pos.x = 600.0F;
    desc.pos.y = 0.0F;
    desc.pos.z = 0.0F;

    fighter_gobj = ftManagerMakeFighter(&desc);
    scSubsysFighterSetStatus(fighter_gobj, nFTDemoStatusStance);
    gcMoveGObjDL(fighter_gobj, 26, -1);
    gcAddGObjProcess(fighter_gobj, mvOpeningLinkPosedFighterProcUpdate, nGCProcessKindFunc, 1);

    DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;
}

/* mvopeninglink.c:8018DB40 mvOpeningLinkMakeNameCamera, verbatim
 * (mvopeninglink.h's own note on the named nGCCommonLinkIDSceneCamera
 * constant in place of Mario's/Donkey's literal 16). */
void mvOpeningLinkMakeNameCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            nGCCommonLinkIDSceneCamera,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            80,
            COBJ_MASK_DLLINK(27),
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

/* mvopeninglink.c:8018DBE0 mvOpeningLinkMakePosedFighterCamera.
 * DIVERGES: camanim_get(&sMVOpeningLinkCamAnimBank, "Link") in place of
 * lbRelocGetFileData(AObjEvent32*, sMVOpeningLinkFiles[1],
 * &llMVOpeningCommonLinkCamAnimJoint) -- mvopeningmario.h's
 * Substitution 1. func_80017EC0 is the decomp's own function, already
 * ported (src/dc/objdisplay.c).
 *
 * The attach-and-play pair is `#ifdef FT_HOSTTEST`-guarded, the same
 * shape mvOpeningMarioMakePosedFighterCamera/
 * mvOpeningDonkeyMakePosedFighterCamera already use for the 64-bit-host
 * pointer-stride bug in `union AObjEvent32`'s `void *p` member -- see
 * mvopeningmario.c's own note by its matching function. */
void mvOpeningLinkMakePosedFighterCamera(void)
{
    GObj *camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        nGCCommonLinkIDSceneCamera,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        10,
        COBJ_MASK_DLLINK(26),
        ~0,
        TRUE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(camera_gobj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 90.0F);

    cobj->projection.persp.aspect = 26.25F / 7.0F;

#ifdef FT_HOSTTEST
    /* the host cannot walk a script; see mvOpeningMarioMakePosedFighterCamera's own note */
#else
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningLinkCamAnimBank, "Link"), 0.0F);
    gcAddGObjProcess(camera_gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
#endif
}

/* mvopeninglink.c:8018DCD0 mvOpeningLinkMakePosedWallpaperCamera,
 * verbatim. */
void mvOpeningLinkMakePosedWallpaperCamera(void)
{
    CObj *cobj;
    GObj *camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        nGCCommonLinkIDSceneCamera,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        20,
        COBJ_MASK_DLLINK(28),
        ~0,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    cobj = CObjGetStruct(camera_gobj);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 90.0F);

    cobj->flags = COBJ_FLAG_DLBUFFERS | COBJ_FLAG_ZBUFFER;
}

/* mvopeninglink.c:8018DD80 mvOpeningLinkFuncRun, verbatim except the
 * eject: gcEjectGObjIfMade in place of a raw gcEjectGObj, the port-wide
 * standing rule every eject follows. At tic 60, hands off to
 * nSCKindOpeningSamus -- checked directly against the decomp source
 * (mvopeninglink.h's own header note), not Donkey (the scene that
 * handed off TO this one) and not Fox. */
void mvOpeningLinkFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningLinkTotalTimeTics++;

    if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindTitle;

        syTaskmanSetLoadScene();
    }
    if (sMVOpeningLinkTotalTimeTics == 15)
    {
        gcEjectGObjIfMade(sMVOpeningLinkNameGObj);
        mvOpeningLinkMakeMotionWindow();
        mvOpeningLinkMakePosedWallpaper();
        mvOpeningLinkMakePosedFighter();
    }
    if (sMVOpeningLinkTotalTimeTics == 60)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindOpeningSamus;

        syTaskmanSetLoadScene();
    }
}

/* mvopeninglink.c:8018DE3C mvOpeningLinkInitVars, verbatim. */
void mvOpeningLinkInitVars(void)
{
    sMVOpeningLinkTotalTimeTics = 0;
}

/* mvopeninglink.c:8018DE48 mvOpeningLinkFuncStart. DIVERGES:
 * mvopeningmario.h's Substitutions 1-4, all at their decomp call sites,
 * plus mvopeningmario.h's own note that the stage acquire
 * (grStageAcquire/stage_bind) is new relative to the decomp's
 * mpCollisionInitGroundData()/gmCameraMakeWallpaperCamera() pair --
 * gkind is fixed for this whole scene (nGRKindHyrule). */
void mvOpeningLinkFuncStart(void)
{
    Stage *stage;

    sMVOpeningLinkBattleState = dSCManagerDefaultBattleState;
    gSCManagerBattleState = &sMVOpeningLinkBattleState;

    gSCManagerBattleState->game_type = nSCBattleGameTypeMovie;

    gSCManagerBattleState->gkind = nGRKindHyrule;
    gSCManagerBattleState->pl_count = 1;

    gSCManagerBattleState->players[0].fkind = nFTKindLink;
    gSCManagerBattleState->players[0].pkind = nFTPlayerKindKey;

    mvOpeningLinkSetupFiles();

    gcMakeGObjSPAfter(nGCCommonKindMovie, mvOpeningLinkFuncRun, nGCCommonLinkIDMovie, GOBJ_PRIORITY_DEFAULT);

    mvOpeningLinkInitVars();
    efParticleInitAll();
    ftParamInitGame();

    stage = grStageAcquire(gSCManagerBattleState->gkind);
    if (stage != NULL)
    {
        stage_bind(stage);
    }
    stage_bind_collision();

    gmCameraSetViewportDimensions(10, 10, 310, 230);

    ftManagerAllocFighter(FTDATA_FLAG_MAINMOTION | FTDATA_FLAG_SUBMOTION, 2);
    wpManagerAllocWeapons();
    itManagerInitItems();

    /* mvOpeningLinkFuncStart's own efManagerInitEffects() (verbatim in
     * the decomp) is NOT the substitution here -- mvopeningmario.h's
     * Substitution 3b, reused unchanged: this scene loads a real
     * fighter pack (Link's) alongside a real stage, so
     * efManagerLoadEffectBank()/efManagerPreloadModels() stand in for
     * it, same as Mario's and Donkey's own scenes. */
    efManagerLoadEffectBank();
    efManagerPreloadModels();

    ftManagerSetupFilesAllKind(nFTKindLink);

    mvOpeningLinkMakeNameCamera();
    mvOpeningLinkMakePosedWallpaperCamera();
    mvOpeningLinkMakePosedFighterCamera();
    mvOpeningLinkMakeName();

    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 1695 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(1695);
}

/* mvopeninglink.c:8018DFCC mvOpeningLinkFuncLights. Not ported -- the
 * standing rule (a Lights1/FuncLights pair never survives test-host,
 * mvopeningmario.c/mvopeningdonkey.c's own note): the whole body is
 * ftDisplayLightsDrawReflect, the dropped reflection lighting every
 * other scene's func_lights drops. func_lights is NULL below. */

/* mvopeninglink.c:8018E0F8-shaped dMVOpeningLinkTaskmanSetup. See the
 * pools note above for the counts, and mvopeningmario.h/
 * mvopeningportraits.h for func_draw (scManagerFuncDraw), the zeroed
 * RSP/RDP fields and the pre-render/controller/matrix-list
 * substitutions -- the same blanket rules apply here. */
SYTaskmanSetup dMVOpeningLinkTaskmanSetup =
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
    MVOPENINGLINK_GOBJPROCS,
    MVOPENINGLINK_GOBJS,
    sizeof(GObj),
    MVOPENINGLINK_XOBJS,
    NULL,
    NULL,
    MVOPENINGLINK_AOBJS,
    MVOPENINGLINK_MOBJS,
    MVOPENINGLINK_DOBJS,
    sizeof(DObj),
    MVOPENINGLINK_SOBJS,
    sizeof(SObj),
    MVOPENINGLINK_COBJS,
    sizeof(CObj),

    mvOpeningLinkFuncStart
};

/* mvopeninglink.c:8018E018 mvOpeningLinkStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeninglink.h), and the stage release
 * below -- mvopeningmario.c's own StartScene says why it is there and
 * why the kind is a constant. */
void mvOpeningLinkStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningLinkTaskmanSetup);

    grStageRelease(nGRKindHyrule);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[40]. */
void mvOpeningLinkOverlayLoad(void)
{
    OVERLAY_CLEAR(sMVOpeningLinkTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningLinkNameGObj);
    OVERLAY_CLEAR(sMVOpeningLinkFighterGObj);
    OVERLAY_CLEAR(sMVOpeningLinkStageCameraGObj);
    OVERLAY_CLEAR(sMVOpeningLinkFigatreeHeap);
    OVERLAY_CLEAR(sMVOpeningLinkPosedFighterSpeed);
    OVERLAY_CLEAR(sMVOpeningLinkAdjustedStartCObjDesc);
    OVERLAY_CLEAR(sMVOpeningLinkAdjustedEndCObjDesc);
    OVERLAY_CLEAR(sMVOpeningLinkBattleState);
    OVERLAY_CLEAR(sMVOpeningLinkNamesBank);
    OVERLAY_CLEAR(sMVOpeningLinkNamesFileHead);
    OVERLAY_CLEAR(sMVOpeningLinkCamAnimBank);
}
