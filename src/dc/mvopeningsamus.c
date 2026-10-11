/* mvopeningsamus.c -- see mvopeningsamus.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningsamus.c; the substitutions
 * are explained once, in mvopeningsamus.h's own header comment (which
 * also names mvopeningmario.h's substitutions 1-5 and
 * mvopeningdonkey.h's substitution 6, all reused here unchanged) -- not
 * repeated at every site.
 */
#include "mvopeningsamus.h"
#include "overlay.h"

#include "camanim.h"    /* the Samus cam-anim script */
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
#include <sc/scsubsys/scsubsys.h>  /* scSubsysFighterSetStatus/SetLightParams */
#include <sys/controller.h>
#include <sys/objanim.h>           /* gcAddCObjCamAnimJoint, gcPlayCamAnim */
#include <sys/objdef.h>            /* nGCCommonLinkIDSceneCamera/Movie */
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

/* relocData file 37 (IFCommonAnnounceCommon), the five letters "SAMUS"
 * spells with (S read twice). Offsets are
 * src/dc/decomp/reloc_data.us.h's own ll<Name>Sprite link labels -- A
 * and M are the exact literals mvopeningmario.c already uses for its
 * own "A"/"M"; S and U are new to this scene. */
#define llIFCommonAnnounceCommonLetterSSprite 0x057F0
#define llIFCommonAnnounceCommonLetterASprite 0x005E0
#define llIFCommonAnnounceCommonLetterMSprite 0x03980
#define llIFCommonAnnounceCommonLetterUSprite 0x060D8

/* ---- the pools --------------------------------------------------------
 *
 * Reused verbatim from mvopeningmario.h's own note: one real VS stage
 * (Zebes) and up to two real Samus fighters (posed + walking), the same
 * budget as every other scene in this chain. */
#define MVOPENINGSAMUS_GOBJS       48
#define MVOPENINGSAMUS_GOBJPROCS   48
#define MVOPENINGSAMUS_XOBJS      288
#define MVOPENINGSAMUS_AOBJS      576
#define MVOPENINGSAMUS_MOBJS       32
#define MVOPENINGSAMUS_DOBJS       96
#define MVOPENINGSAMUS_SOBJS        8
#define MVOPENINGSAMUS_COBJS        8

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mvopeningsamus.c:8018E120/E13C, verbatim. */
CObjDesc dMVOpeningSamusCObjDescStart = { { 400.0F, 1100.0F, 0.0F }, { 0.0F, 200.0F, 0.0F }, 0.6F };
CObjDesc dMVOpeningSamusCObjDescEnd = { { 1600.0F, 230.0F, 200.0F }, { 0.0F, 200.0F, 0.0F }, 0.6F };

/* mvopeningsamus.c:8018E158 dMVOpeningSamusKeyEvents, verbatim. */
FTKeyEvent dMVOpeningSamusKeyEvents[] =
{
    FTKEY_EVENT_BUTTON(Z_TRIG, 1),
    FTKEY_EVENT_BUTTON(A_BUTTON, 1),
    FTKEY_EVENT_END()
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static s32 sMVOpeningSamusTotalTimeTics;
static GObj *sMVOpeningSamusNameGObj;
static GObj *sMVOpeningSamusFighterGObj;
static GObj *sMVOpeningSamusStageCameraGObj;

/* mvopeningmario.h's own note (Substitution 4): this stays NULL. */
static void *sMVOpeningSamusFigatreeHeap;

static f32 sMVOpeningSamusPosedFighterSpeed;
static CObjDesc sMVOpeningSamusAdjustedStartCObjDesc;
static CObjDesc sMVOpeningSamusAdjustedEndCObjDesc;
static SCBattleState sMVOpeningSamusBattleState;

/* The port's stand-in for sMVOpeningSamusFiles[0]: file 37's sprite
 * bank (romdisk/ifannounce.spr), the same shape
 * sMVOpeningMarioNamesBank/sMVOpeningDonkeyNamesBank/sMVOpeningLinkNamesBank
 * already take. sMVOpeningSamusNamesFileHead is not static so
 * hosttest_ft.c's own openingsamus_find can read it. */
static SpriteBank sMVOpeningSamusNamesBank;
void *sMVOpeningSamusNamesFileHead;

/* The port's stand-in for sMVOpeningSamusFiles[1]: the
 * whole of what it wanted out of that file is
 * romdisk/mvopeningcommon.cam's "Samus" entry (src/dc/camanim.h). */
static CamAnimBank sMVOpeningSamusCamAnimBank;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningsamus.c:8018D0C0 mvOpeningSamusSetupFiles. DIVERGES: the two
 * loads mvopeningmario.h's header names, in place of
 * lbRelocInitSetup/lbRelocLoadFilesListed. */
void mvOpeningSamusSetupFiles(void)
{
    if (sprite_bank_load(&sMVOpeningSamusNamesBank, "ifannounce.spr") < 0)
    {
        sMVOpeningSamusNamesFileHead = NULL;
    }
    else
    {
        sMVOpeningSamusNamesFileHead = &sMVOpeningSamusNamesBank;
    }
    camanim_bank_load(&sMVOpeningSamusCamAnimBank, "mvopeningcommon.cam");
}

/* mvopeningsamus.c:8018D160 mvOpeningSamusInitName, verbatim. */
void mvOpeningSamusInitName(SObj *sobj)
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

/* mvopeningsamus.c:8018D194 mvOpeningSamusMakeName, verbatim but for
 * lbRelocGetFileData's own redefinition above. */
void mvOpeningSamusMakeName(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 i;

    intptr_t offsets[] =
    {
        llIFCommonAnnounceCommonLetterSSprite,
        llIFCommonAnnounceCommonLetterASprite,
        llIFCommonAnnounceCommonLetterMSprite,
        llIFCommonAnnounceCommonLetterUSprite,
        llIFCommonAnnounceCommonLetterSSprite,
        0x0
    };

    f32 pos_x[] =
    {
        0.0F, 30.0F, 70.0F, 110.0F, 140.0F
    };

    sMVOpeningSamusNameGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; offsets[i] != 0x0; i++)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningSamusNamesFileHead, offsets[i]));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = pos_x[i] + 80.0F;
        sobj->pos.y = 100.0F;

        mvOpeningSamusInitName(sobj);
    }
}

/* mvopeningsamus.c:8018D314 mvOpeningSamusMotionCameraProcUpdate,
 * verbatim. */
void mvOpeningSamusMotionCameraProcUpdate(GObj *camera_gobj)
{
    CObj *cobj = CObjGetStruct(camera_gobj);

    if (sMVOpeningSamusTotalTimeTics >= 15)
    {
        cobj->vec.eye.x += (((sMVOpeningSamusAdjustedEndCObjDesc.eye.x - sMVOpeningSamusAdjustedStartCObjDesc.eye.x) / 45.0F));
        cobj->vec.eye.y += (((sMVOpeningSamusAdjustedEndCObjDesc.eye.y - sMVOpeningSamusAdjustedStartCObjDesc.eye.y) / 45.0F));
        cobj->vec.eye.z += (((sMVOpeningSamusAdjustedEndCObjDesc.eye.z - sMVOpeningSamusAdjustedStartCObjDesc.eye.z) / 45.0F));
        cobj->vec.at.x += (((sMVOpeningSamusAdjustedEndCObjDesc.at.x - sMVOpeningSamusAdjustedStartCObjDesc.at.x) / 45.0F));
        cobj->vec.at.y += (((sMVOpeningSamusAdjustedEndCObjDesc.at.y - sMVOpeningSamusAdjustedStartCObjDesc.at.y) / 45.0F));
        cobj->vec.at.z += (((sMVOpeningSamusAdjustedEndCObjDesc.at.z - sMVOpeningSamusAdjustedStartCObjDesc.at.z) / 45.0F));
        cobj->vec.up.x += (((sMVOpeningSamusAdjustedEndCObjDesc.upx - sMVOpeningSamusAdjustedStartCObjDesc.upx) / 45.0F));
    }
}

/* mvopeningsamus.c:8018D40C mvOpeningSamusMakeMotionCamera. DIVERGES:
 * gmCameraMakeMovieCamera(NULL) is the decomp's own call (its kind 8 is
 * the roll look-at, objdisplay.c gcCameraLookAtF), and camera_mask is
 * set right after it, the
 * same line scAutoDemoFuncStart makes for its own real camera. */
void mvOpeningSamusMakeMotionCamera(Vec3f move)
{
    CObj *cobj;

    sMVOpeningSamusAdjustedStartCObjDesc = dMVOpeningSamusCObjDescStart;
    sMVOpeningSamusAdjustedEndCObjDesc = dMVOpeningSamusCObjDescEnd;

    sMVOpeningSamusStageCameraGObj = gmCameraMakeMovieCamera(NULL);
    gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK;

    cobj = CObjGetStruct(sMVOpeningSamusStageCameraGObj);

    syRdpSetViewport(&cobj->viewport, 110.0F, 10.0F, 310.0F, 230.0F);

    cobj->projection.persp.aspect = 10.0F / 11.0F;

    gcEndProcessAll(sMVOpeningSamusStageCameraGObj);
    gcAddGObjProcess(sMVOpeningSamusStageCameraGObj, mvOpeningSamusMotionCameraProcUpdate, nGCProcessKindFunc, 1);

    sMVOpeningSamusAdjustedStartCObjDesc.eye.x += move.x;
    sMVOpeningSamusAdjustedStartCObjDesc.eye.y += move.y;
    sMVOpeningSamusAdjustedStartCObjDesc.eye.z += move.z;
    sMVOpeningSamusAdjustedStartCObjDesc.at.x += move.x;
    sMVOpeningSamusAdjustedStartCObjDesc.at.y += move.y;
    sMVOpeningSamusAdjustedStartCObjDesc.at.z += move.z;

    sMVOpeningSamusAdjustedEndCObjDesc.eye.x += move.x;
    sMVOpeningSamusAdjustedEndCObjDesc.eye.y += move.y;
    sMVOpeningSamusAdjustedEndCObjDesc.eye.z += move.z;
    sMVOpeningSamusAdjustedEndCObjDesc.at.x += move.x;
    sMVOpeningSamusAdjustedEndCObjDesc.at.y += move.y;
    sMVOpeningSamusAdjustedEndCObjDesc.at.z += move.z;

    cobj->vec.eye.x = sMVOpeningSamusAdjustedStartCObjDesc.eye.x;
    cobj->vec.eye.y = sMVOpeningSamusAdjustedStartCObjDesc.eye.y;
    cobj->vec.eye.z = sMVOpeningSamusAdjustedStartCObjDesc.eye.z;
    cobj->vec.at.x = sMVOpeningSamusAdjustedStartCObjDesc.at.x;
    cobj->vec.at.y = sMVOpeningSamusAdjustedStartCObjDesc.at.y;
    cobj->vec.at.z = sMVOpeningSamusAdjustedStartCObjDesc.at.z;
    cobj->vec.up.x = sMVOpeningSamusAdjustedStartCObjDesc.upx;
}

/* mvopeningsamus.c:8018D61C mvOpeningSamusMakeMotionWindow. DIVERGES:
 * the decomp's own Zebes spotlight DObj-tree fixup is cut outright
 * (mvopeningsamus.h's Substitution 7); grWallpaperMakeDecideKind()/
 * grCommonSetupInitAll() move from before mvOpeningSamusMakeMotionCamera
 * to after it (the ordinary Substitution 3 reordering); everything else
 * (the mpCollision calls, gmRumbleMakeActor/ftPublicMakeActor, the
 * fighter loop) is verbatim. */
void mvOpeningSamusMakeMotionWindow(void)
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

    mvOpeningSamusMakeMotionCamera(pos);

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

        sMVOpeningSamusFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

        ftParamInitPlayerBattleStats(i, fighter_gobj);
        ftParamSetKey(fighter_gobj, dMVOpeningSamusKeyEvents);
    }
}

/* mvopeningsamus.c:8018D8B0 mvOpeningSamusPosedWallpaperProcDisplay,
 * verbatim. */
void mvOpeningSamusPosedWallpaperProcDisplay(GObj *gobj)
{
    (void)gobj;

    /* DIVERGES: the fill quad in place of the raw gDPFillRectangle, as
     * mvopeningmario.c's panel says why. */
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    lbCommonSpriteFillRect(10, 10, 110, 230, 0x00, 0x00, 0x50, 0xFF);
}

/* mvopeningsamus.c:8018D9AC mvOpeningSamusMakePosedWallpaper, verbatim. */
void mvOpeningSamusMakePosedWallpaper(void)
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
        mvOpeningSamusPosedWallpaperProcDisplay,
        28,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
}

/* mvopeningsamus.c:8018D9F8 mvOpeningSamusPosedFighterProcUpdate,
 * verbatim. */
void mvOpeningSamusPosedFighterProcUpdate(GObj *fighter_gobj)
{
    switch (sMVOpeningSamusTotalTimeTics)
    {
    default:
        break;

    case 15:
        sMVOpeningSamusPosedFighterSpeed = 17.0F;
        break;

    case 45:
        sMVOpeningSamusPosedFighterSpeed = 15.0F;
        break;

    case 60:
        sMVOpeningSamusPosedFighterSpeed = 0.0F;
        break;
    }
    if ((sMVOpeningSamusTotalTimeTics > 15) && (sMVOpeningSamusTotalTimeTics < 45))
    {
        sMVOpeningSamusPosedFighterSpeed += -1.0F / 15.0F;
    }
    if ((sMVOpeningSamusTotalTimeTics > 45) && (sMVOpeningSamusTotalTimeTics < 60))
    {
        sMVOpeningSamusPosedFighterSpeed += -1.0F;
    }
    DObjGetStruct(fighter_gobj)->translate.vec.f.y -= sMVOpeningSamusPosedFighterSpeed;
}

/* mvopeningsamus.c:8018DAC8 mvOpeningSamusMakePosedFighter, verbatim:
 * desc.figatree_heap reads sMVOpeningSamusFigatreeHeap, always NULL
 * (mvopeningmario.h's Substitution 4). */
void mvOpeningSamusMakePosedFighter(void)
{
    GObj *fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = nFTKindSamus;
    desc.costume = ftParamGetCostumeCommonID(nFTKindSamus, 0);
    desc.figatree_heap = sMVOpeningSamusFigatreeHeap;

    desc.pos.x = 0.0F;
    desc.pos.y = 600.0F;
    desc.pos.z = 0.0F;

    fighter_gobj = ftManagerMakeFighter(&desc);
    scSubsysFighterSetStatus(fighter_gobj, nFTDemoStatusStance);
    gcMoveGObjDL(fighter_gobj, 26, -1);
    gcAddGObjProcess(fighter_gobj, mvOpeningSamusPosedFighterProcUpdate, nGCProcessKindFunc, 1);

    DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;
}

/* mvopeningsamus.c:8018DBC8 mvOpeningSamusMakeNameCamera, verbatim. */
void mvOpeningSamusMakeNameCamera(void)
{
    GObj *camera_gobj = gcMakeCameraGObj
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
    );
    CObj *cobj = CObjGetStruct(camera_gobj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mvopeningsamus.c:8018DC68 mvOpeningSamusMakePosedFighterCamera.
 * DIVERGES: the AObjEvent32 script's own reach, mvopeningmario.h's
 * Substitution 1 -- camanim_get(&sMVOpeningSamusCamAnimBank, "Samus")
 * in place of lbRelocGetFileData(AObjEvent32*, sMVOpeningSamusFiles[1],
 * &llMVOpeningCommonSamusCamAnimJoint). The attach-and-play pair is
 * `#ifdef FT_HOSTTEST`-guarded, the same shape every scene in this
 * chain already uses (mvopeningmario.c's own note says why). */
void mvOpeningSamusMakePosedFighterCamera(void)
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

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 110.0F, 230.0F);

    cobj->projection.persp.aspect = 5.0F / 11.0F;

#ifdef FT_HOSTTEST
    /* the host cannot walk a script; see mvopeningmario.c's own note */
#else
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningSamusCamAnimBank, "Samus"), 0.0F);
    gcAddGObjProcess(camera_gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
#endif
}

/* mvopeningsamus.c:8018DD58 mvOpeningSamusMakePosedWallpaperCamera,
 * verbatim. */
void mvOpeningSamusMakePosedWallpaperCamera(void)
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
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 110.0F, 230.0F);

    cobj->flags = COBJ_FLAG_DLBUFFERS | COBJ_FLAG_ZBUFFER;
}

/* mvopeningsamus.c:8018DE08 mvOpeningSamusFuncRun, verbatim except the
 * eject: gcEjectGObjIfMade in place of a raw gcEjectGObj, the port-wide
 * standing rule (mvopeningportraits.h). */
void mvOpeningSamusFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningSamusTotalTimeTics++;

    if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindTitle;

        syTaskmanSetLoadScene();
    }
    if (sMVOpeningSamusTotalTimeTics == 15)
    {
        gcEjectGObjIfMade(sMVOpeningSamusNameGObj);
        mvOpeningSamusMakeMotionWindow();
        mvOpeningSamusMakePosedWallpaper();
        mvOpeningSamusMakePosedFighter();
    }
    if (sMVOpeningSamusTotalTimeTics == 60)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindOpeningYoshi;

        syTaskmanSetLoadScene();
    }
}

/* mvopeningsamus.c:8018DEC4 mvOpeningSamusInitVars, verbatim. */
void mvOpeningSamusInitVars(void)
{
    sMVOpeningSamusTotalTimeTics = 0;
}

/* mvopeningsamus.c:8018DED0 mvOpeningSamusFuncStart. DIVERGES:
 * mvopeningmario.h's Substitutions 1-4, all at their decomp call sites
 * except the stage acquire (grStageAcquire/stage_bind), which is new --
 * gkind is fixed for this whole scene (nGRKindZebes). The one call this
 * scene's own decomp adds beyond Mario's shape,
 * scSubsysFighterSetLightParams, is kept verbatim at its own decomp
 * position (mvopeningsamus.h's own note -- already a ported function,
 * no substitution needed). */
void mvOpeningSamusFuncStart(void)
{
    Stage *stage;

    sMVOpeningSamusBattleState = dSCManagerDefaultBattleState;
    gSCManagerBattleState = &sMVOpeningSamusBattleState;

    gSCManagerBattleState->game_type = nSCBattleGameTypeMovie;

    gSCManagerBattleState->gkind = nGRKindZebes;
    gSCManagerBattleState->pl_count = 1;

    gSCManagerBattleState->players[0].fkind = nFTKindSamus;
    gSCManagerBattleState->players[0].pkind = nFTPlayerKindKey;

    mvOpeningSamusSetupFiles();

    gcMakeGObjSPAfter(nGCCommonKindMovie, mvOpeningSamusFuncRun, nGCCommonLinkIDMovie, GOBJ_PRIORITY_DEFAULT);

    mvOpeningSamusInitVars();
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

    /* mvopeningmario.h's own Substitution 3b: a real fighter pack is
     * loaded alongside a real stage here too, so the raw
     * efManagerInitEffects() call is replaced the same way. */
    efManagerLoadEffectBank();
    efManagerPreloadModels();

    scSubsysFighterSetLightParams(45.0F, 45.0F, 0xFF, 0xFF, 0xFF, 0xFF);

    ftManagerSetupFilesAllKind(nFTKindSamus);

    mvOpeningSamusMakeNameCamera();
    mvOpeningSamusMakePosedWallpaperCamera();
    mvOpeningSamusMakePosedFighterCamera();
    mvOpeningSamusMakeName();

    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 1785 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(1785);
}

/* mvopeningsamus.c:8018E07C mvOpeningSamusFuncLights. Not ported -- the
 * standing rule (a Lights1/FuncLights pair never survives test-host,
 * mvopeningroom.h/mvopeningportraits.h). func_lights is NULL below. */

/* mvopeningsamus.c:8018E1B4-shaped dMVOpeningSamusTaskmanSetup. See the
 * pools note above for the counts, and mvopeningportraits.h for
 * func_draw (scManagerFuncDraw), the zeroed RSP/RDP fields and the
 * pre-render/controller/matrix-list substitutions -- the same blanket
 * rules apply here. */
SYTaskmanSetup dMVOpeningSamusTaskmanSetup =
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
    MVOPENINGSAMUS_GOBJPROCS,
    MVOPENINGSAMUS_GOBJS,
    sizeof(GObj),
    MVOPENINGSAMUS_XOBJS,
    NULL,
    NULL,
    MVOPENINGSAMUS_AOBJS,
    MVOPENINGSAMUS_MOBJS,
    MVOPENINGSAMUS_DOBJS,
    sizeof(DObj),
    MVOPENINGSAMUS_SOBJS,
    sizeof(SObj),
    MVOPENINGSAMUS_COBJS,
    sizeof(CObj),

    mvOpeningSamusFuncStart
};

/* mvopeningsamus.c:8018E0C8 mvOpeningSamusStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeningsamus.h), and the stage release
 * below -- mvopeningmario.c's own StartScene says why it is there and
 * why the kind is a constant. */
void mvOpeningSamusStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningSamusTaskmanSetup);

    grStageRelease(nGRKindZebes);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[38]. */
void mvOpeningSamusOverlayLoad(void)
{
    OVERLAY_CLEAR(sMVOpeningSamusTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningSamusNameGObj);
    OVERLAY_CLEAR(sMVOpeningSamusFighterGObj);
    OVERLAY_CLEAR(sMVOpeningSamusStageCameraGObj);
    OVERLAY_CLEAR(sMVOpeningSamusFigatreeHeap);
    OVERLAY_CLEAR(sMVOpeningSamusPosedFighterSpeed);
    OVERLAY_CLEAR(sMVOpeningSamusAdjustedStartCObjDesc);
    OVERLAY_CLEAR(sMVOpeningSamusAdjustedEndCObjDesc);
    OVERLAY_CLEAR(sMVOpeningSamusBattleState);
    OVERLAY_CLEAR(sMVOpeningSamusNamesBank);
    OVERLAY_CLEAR(sMVOpeningSamusNamesFileHead);
    OVERLAY_CLEAR(sMVOpeningSamusCamAnimBank);
}
