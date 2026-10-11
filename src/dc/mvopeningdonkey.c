/* mvopeningdonkey.c -- see mvopeningdonkey.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningdonkey.c; every function names
 * its line range. This scene reuses mvopeningmario.c's whole recipe
 * (mvopeningdonkey.h's own header explains what actually differs) --
 * not repeated at every site.
 */
#include "mvopeningdonkey.h"
#include "overlay.h"

#include "camanim.h"    /* the Donkey cam-anim script */
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
#include <sys/rdp.h>
#include <wp/wpmanager.h>          /* wpManagerAllocWeapons */
#include <lb/lbdef.h>

/* The game reaches the name letters as file 37's base plus the offset
 * its link label took; the port's file is a bank and the label's value
 * is the offset (src/dc/mvopeningmario.c does the same for its own
 * copy of this exact file). */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* relocData file 37 (IFCommonAnnounceCommon), the two letters "DK"
 * (REGION_US) spells with. Offsets are src/dc/mvopeningmario.c's own
 * copies of the decomp's ll<Name>Sprite link labels
 * (src/dc/decomp/reloc_data.us.h agrees). */
#define llIFCommonAnnounceCommonLetterDSprite 0x01268
#define llIFCommonAnnounceCommonLetterKSprite 0x02F98

/* ---- the pools --------------------------------------------------------
 *
 * Reuses mvopeningmario.c's own rationale verbatim (mvopeningdonkey.h's
 * header): one real VS stage and up to two real fighters, both Donkey,
 * the same shape as Mario's own scene -- a two-letter name card instead
 * of five costs nothing extra to size for. */
#define MVOPENINGDONKEY_GOBJS       48
#define MVOPENINGDONKEY_GOBJPROCS   48
#define MVOPENINGDONKEY_XOBJS      288
#define MVOPENINGDONKEY_AOBJS      576
#define MVOPENINGDONKEY_MOBJS       32
#define MVOPENINGDONKEY_DOBJS       96
#define MVOPENINGDONKEY_SOBJS        8
#define MVOPENINGDONKEY_COBJS        8

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mvopeningdonkey.c:8018E070/E08C, verbatim. */
CObjDesc dMVOpeningDonkeyStartCObjDesc = { { -1100.0F, 150.0F, 400.0F }, { 0.0F, 150.0F, 0.0F }, 0.0F };
CObjDesc dMVOpeningDonkeyEndCObjDesc = { { -900.0F, 500.0F, 1800.0F }, { 0.0F, 500.0F, 0.0F }, 0.0F };

/* mvopeningdonkey.c:8018E0A8 dMVOpeningDonkeyKeyEvents, verbatim: the
 * walking fighter's canned input. */
FTKeyEvent dMVOpeningDonkeyKeyEvents[] =
{
    FTKEY_EVENT_STICK(0, -I_CONTROLLER_RANGE_MAX, 0),
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

static s32 sMVOpeningDonkeyTotalTimeTics;
static GObj *sMVOpeningDonkeyNameGObj;
static GObj *sMVOpeningDonkeyFighterGObj;
static GObj *sMVOpeningDonkeyMotionCameraGObj;

/* mvopeningdonkey.h's own note: this stays NULL, the same
 * ftManagerAllocFigatreeHeapKind()-always-NULL state mvopeningmario.c's
 * own heap already carries. */
static void *sMVOpeningDonkeyFigatreeHeap;

static f32 sMVOpeningDonkeyPosedFighterSpeed;
static CObjDesc sMVOpeningDonkeyAdjustedStartCObjDesc;
static CObjDesc sMVOpeningDonkeyAdjustedEndCObjDesc;
static SCBattleState sMVOpeningDonkeyBattleState;

/* The port's stand-in for sMVOpeningDonkeyFiles[0]: file 37's sprite
 * bank (romdisk/ifannounce.spr), the same shape
 * src/dc/mvopeningmario.c's own sMVOpeningMarioNamesBank already takes.
 * sMVOpeningDonkeyNamesFileHead is not static, so hosttest_ft.c's own
 * openingdonkey_find can read it. */
static SpriteBank sMVOpeningDonkeyNamesBank;
void *sMVOpeningDonkeyNamesFileHead;

/* The port's stand-in for sMVOpeningDonkeyFiles[1]: file
 * 65 held nothing this scene read but one camera-anim script, and the
 * whole of what it wanted out of that file is romdisk/
 * mvopeningcommon.cam's "Donkey" entry (src/dc/camanim.h). */
static CamAnimBank sMVOpeningDonkeyCamAnimBank;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningdonkey.c:8018D0C0 mvOpeningDonkeySetupFiles. DIVERGES: the
 * two loads mvopeningdonkey.h's header names, in place of
 * lbRelocInitSetup/lbRelocLoadFilesListed -- the same substitution
 * mvOpeningMarioSetupFiles already makes. */
void mvOpeningDonkeySetupFiles(void)
{
    if (sprite_bank_load(&sMVOpeningDonkeyNamesBank, "ifannounce.spr") < 0)
    {
        sMVOpeningDonkeyNamesFileHead = NULL;
    }
    else
    {
        sMVOpeningDonkeyNamesFileHead = &sMVOpeningDonkeyNamesBank;
    }
    camanim_bank_load(&sMVOpeningDonkeyCamAnimBank, "mvopeningcommon.cam");
}

/* mvopeningdonkey.c:8018D160 mvOpeningDonkeyInitName, verbatim. */
void mvOpeningDonkeyInitName(SObj *sobj)
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

/* mvopeningdonkey.c:8018D194 mvOpeningDonkeyMakeName. DIVERGES: only
 * the decomp's own `#if defined (REGION_US)` arm is ported (two
 * letters, "D" then "K") -- mvopeningdonkey.h's header says why. */
void mvOpeningDonkeyMakeName(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 i;

    intptr_t offsets[] =
    {
        llIFCommonAnnounceCommonLetterDSprite,
        llIFCommonAnnounceCommonLetterKSprite,
        0x0
    };

    f32 pos_x[] =
    {
        0.0F, 40.0F
    };

    sMVOpeningDonkeyNameGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; offsets[i] != 0x0; i++)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningDonkeyNamesFileHead, offsets[i]));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = pos_x[i] + 120.0F;
        sobj->pos.y = 100.0F;

        mvOpeningDonkeyInitName(sobj);
    }
}

/* mvopeningdonkey.c:8018D2FC mvOpeningDonkeyMotionCameraProcUpdate,
 * verbatim. */
void mvOpeningDonkeyMotionCameraProcUpdate(GObj *camera_gobj)
{
    CObj *cobj = CObjGetStruct(camera_gobj);

    if (sMVOpeningDonkeyTotalTimeTics >= 15)
    {
        cobj->vec.eye.x += (((sMVOpeningDonkeyAdjustedEndCObjDesc.eye.x - sMVOpeningDonkeyAdjustedStartCObjDesc.eye.x) / 45.0F));
        cobj->vec.eye.y += (((sMVOpeningDonkeyAdjustedEndCObjDesc.eye.y - sMVOpeningDonkeyAdjustedStartCObjDesc.eye.y) / 45.0F));
        cobj->vec.eye.z += (((sMVOpeningDonkeyAdjustedEndCObjDesc.eye.z - sMVOpeningDonkeyAdjustedStartCObjDesc.eye.z) / 45.0F));
        cobj->vec.at.x += (((sMVOpeningDonkeyAdjustedEndCObjDesc.at.x - sMVOpeningDonkeyAdjustedStartCObjDesc.at.x) / 45.0F));
        cobj->vec.at.y += (((sMVOpeningDonkeyAdjustedEndCObjDesc.at.y - sMVOpeningDonkeyAdjustedStartCObjDesc.at.y) / 45.0F));
        cobj->vec.at.z += (((sMVOpeningDonkeyAdjustedEndCObjDesc.at.z - sMVOpeningDonkeyAdjustedStartCObjDesc.at.z) / 45.0F));
        cobj->vec.up.x += (((sMVOpeningDonkeyAdjustedEndCObjDesc.upx - sMVOpeningDonkeyAdjustedStartCObjDesc.upx) / 45.0F));
    }
}

/* mvopeningdonkey.c:8018D3F4 mvOpeningDonkeyMakeMotionCamera. DIVERGES:
 * gmCameraMakeMovieCamera(NULL) is the decomp's own call (its kind 8 is
 * the roll look-at, objdisplay.c gcCameraLookAtF), and camera_mask is
 * set right after it. The viewport is mirrored to the
 * LEFT two-thirds (10-210), not Mario's RIGHT (110-310) --
 * mvopeningdonkey.h's own header note. */
void mvOpeningDonkeyMakeMotionCamera(Vec3f move)
{
    CObj *cobj;

    sMVOpeningDonkeyAdjustedStartCObjDesc = dMVOpeningDonkeyStartCObjDesc;
    sMVOpeningDonkeyAdjustedEndCObjDesc = dMVOpeningDonkeyEndCObjDesc;

    sMVOpeningDonkeyMotionCameraGObj = gmCameraMakeMovieCamera(NULL);
    gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK;

    cobj = CObjGetStruct(sMVOpeningDonkeyMotionCameraGObj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 210.0F, 230.0F);

    cobj->projection.persp.aspect = 10.0F / 11.0F;

    gcEndProcessAll(sMVOpeningDonkeyMotionCameraGObj);
    gcAddGObjProcess(sMVOpeningDonkeyMotionCameraGObj, mvOpeningDonkeyMotionCameraProcUpdate, nGCProcessKindFunc, 1);

    sMVOpeningDonkeyAdjustedStartCObjDesc.eye.x += move.x;
    sMVOpeningDonkeyAdjustedStartCObjDesc.eye.y += move.y;
    sMVOpeningDonkeyAdjustedStartCObjDesc.eye.z += move.z;
    sMVOpeningDonkeyAdjustedStartCObjDesc.at.x += move.x;
    sMVOpeningDonkeyAdjustedStartCObjDesc.at.y += move.y;
    sMVOpeningDonkeyAdjustedStartCObjDesc.at.z += move.z;

    sMVOpeningDonkeyAdjustedEndCObjDesc.eye.x += move.x;
    sMVOpeningDonkeyAdjustedEndCObjDesc.eye.y += move.y;
    sMVOpeningDonkeyAdjustedEndCObjDesc.eye.z += move.z;
    sMVOpeningDonkeyAdjustedEndCObjDesc.at.x += move.x;
    sMVOpeningDonkeyAdjustedEndCObjDesc.at.y += move.y;
    sMVOpeningDonkeyAdjustedEndCObjDesc.at.z += move.z;

    cobj->vec.eye.x = sMVOpeningDonkeyAdjustedStartCObjDesc.eye.x;
    cobj->vec.eye.y = sMVOpeningDonkeyAdjustedStartCObjDesc.eye.y;
    cobj->vec.eye.z = sMVOpeningDonkeyAdjustedStartCObjDesc.eye.z;
    cobj->vec.at.x = sMVOpeningDonkeyAdjustedStartCObjDesc.at.x;
    cobj->vec.at.y = sMVOpeningDonkeyAdjustedStartCObjDesc.at.y;
    cobj->vec.at.z = sMVOpeningDonkeyAdjustedStartCObjDesc.at.z;
    cobj->vec.up.x = sMVOpeningDonkeyAdjustedStartCObjDesc.upx;
}

/* mvopeningdonkey.c:8018D604 mvOpeningDonkeyMakeMotionWindow. DIVERGES:
 * mpCollisionGetMapObjCountKind/IDsKind/PositionID stay verbatim;
 * grWallpaperMakeDecideKind()/grCommonSetupInitAll() move to after
 * mvOpeningDonkeyMakeMotionCamera() rather than before -- the same
 * reorder mvOpeningMarioMakeMotionWindow already takes
 * (mvopeningmario.h's Substitution 3). */
void mvOpeningDonkeyMakeMotionWindow(void)
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

    mvOpeningDonkeyMakeMotionCamera(pos);

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
        desc.lr = -1;
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

        sMVOpeningDonkeyFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

        ftParamInitPlayerBattleStats(i, fighter_gobj);
        ftParamSetKey(fighter_gobj, dMVOpeningDonkeyKeyEvents);
    }
}

/* mvopeningdonkey.c:8018D834 mvOpeningDonkeyPosedWallpaperProcDisplay,
 * verbatim. The fill rectangle is the RIGHT third (210-310), not
 * Mario's LEFT (10-110) -- mvopeningdonkey.h's own header note. */
void mvOpeningDonkeyPosedWallpaperProcDisplay(GObj *gobj)
{
    (void)gobj;

    /* DIVERGES: the fill quad in place of the raw gDPFillRectangle, as
     * mvopeningmario.c's panel says why. */
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    lbCommonSpriteFillRect(210, 10, 310, 230, 0x46, 0x5A, 0x00, 0xFF);
}

/* mvopeningdonkey.c:8018D934 mvOpeningDonkeyMakePosedWallpaper,
 * verbatim. */
void mvOpeningDonkeyMakePosedWallpaper(void)
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
        mvOpeningDonkeyPosedWallpaperProcDisplay,
        28,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
}

/* mvopeningdonkey.c:8018D980 mvOpeningDonkeyPosedFighterProcUpdate,
 * verbatim (note the `+=`, not Mario's `-=` -- each scene's own
 * decomp sign, mvopeningdonkey.h's header note). */
void mvOpeningDonkeyPosedFighterProcUpdate(GObj *fighter_gobj)
{
    switch (sMVOpeningDonkeyTotalTimeTics)
    {
    default:
        break;

    case 15:
        sMVOpeningDonkeyPosedFighterSpeed = 17.0F;
        break;

    case 45:
        sMVOpeningDonkeyPosedFighterSpeed = 15.0F;
        break;

    case 60:
        sMVOpeningDonkeyPosedFighterSpeed = 0.0F;
        break;
    }
    if ((sMVOpeningDonkeyTotalTimeTics > 15) && (sMVOpeningDonkeyTotalTimeTics < 45))
    {
        sMVOpeningDonkeyPosedFighterSpeed += -1.0F / 15.0F;
    }
    if ((sMVOpeningDonkeyTotalTimeTics > 45) && (sMVOpeningDonkeyTotalTimeTics < 60))
    {
        sMVOpeningDonkeyPosedFighterSpeed += -1.0F;
    }
    DObjGetStruct(fighter_gobj)->translate.vec.f.y += sMVOpeningDonkeyPosedFighterSpeed;
}

/* mvopeningdonkey.c:8018DA50 mvOpeningDonkeyMakePosedFighter, verbatim:
 * desc.figatree_heap reads sMVOpeningDonkeyFigatreeHeap, always NULL
 * (mvopeningmario.h's Substitution 4). */
void mvOpeningDonkeyMakePosedFighter(void)
{
    GObj *fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = nFTKindDonkey;
    desc.costume = ftParamGetCostumeCommonID(nFTKindDonkey, 0);
    desc.figatree_heap = sMVOpeningDonkeyFigatreeHeap;

    desc.pos.x = 0.0F;
    desc.pos.y = -600.0F;
    desc.pos.z = 0.0F;

    fighter_gobj = ftManagerMakeFighter(&desc);
    scSubsysFighterSetStatus(fighter_gobj, nFTDemoStatusStance);
    gcMoveGObjDL(fighter_gobj, 26, -1);
    gcAddGObjProcess(fighter_gobj, mvOpeningDonkeyPosedFighterProcUpdate, nGCProcessKindFunc, 1);

    DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;
}

/* mvopeningdonkey.c:8018DB50 mvOpeningDonkeyMakeNameCamera, verbatim.
 * Full-width viewport, same as Mario's own name camera. */
void mvOpeningDonkeyMakeNameCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
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

/* mvopeningdonkey.c:8018DBF0 mvOpeningDonkeyMakePosedFighterCamera.
 * DIVERGES: camanim_get(&sMVOpeningDonkeyCamAnimBank, "Donkey") in
 * place of lbRelocGetFileData, and the attach-and-play pair is
 * `#ifdef FT_HOSTTEST`-guarded, the same shape
 * mvOpeningMarioMakePosedFighterCamera already uses and for the same
 * reason (mvopeningmario.c's own note on that function: `union
 * AObjEvent32`'s `void *p` makes the host's stride wrong). Viewport is
 * mirrored to the RIGHT third (210-310), not Mario's LEFT (10-110). */
void mvOpeningDonkeyMakePosedFighterCamera(void)
{
    GObj *camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
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

    syRdpSetViewport(&cobj->viewport, 210.0F, 10.0F, 310.0F, 230.0F);

    cobj->projection.persp.aspect = 5.0F / 11.0F;

#ifdef FT_HOSTTEST
    /* the host cannot walk a script; see mvOpeningMarioMakePosedFighterCamera's own note */
#else
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningDonkeyCamAnimBank, "Donkey"), 0.0F);
    gcAddGObjProcess(camera_gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
#endif
}

/* mvopeningdonkey.c:8018DCD8 mvOpeningDonkeyMakePosedWallpaperCamera,
 * verbatim. Viewport is mirrored to the RIGHT third (210-310), not
 * Mario's LEFT (10-110). */
void mvOpeningDonkeyMakePosedWallpaperCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
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
        )
    );
    syRdpSetViewport(&cobj->viewport, 210.0F, 10.0F, 310.0F, 230.0F);

    cobj->flags = COBJ_FLAG_DLBUFFERS | COBJ_FLAG_ZBUFFER;
}

/* mvopeningdonkey.c:8018DD80 mvOpeningDonkeyFuncRun, verbatim except the
 * eject: gcEjectGObjIfMade in place of a raw gcEjectGObj, the port-wide
 * standing rule (mvopeningportraits.h). Hands off to
 * nSCKindOpeningLink, not nSCKindOpeningDonkey -- checked directly
 * against the decomp, mvopeningdonkey.h's own header note. */
void mvOpeningDonkeyFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningDonkeyTotalTimeTics++;

    if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindTitle;

        syTaskmanSetLoadScene();
    }
    if (sMVOpeningDonkeyTotalTimeTics == 15)
    {
        gcEjectGObjIfMade(sMVOpeningDonkeyNameGObj);
        mvOpeningDonkeyMakeMotionWindow();
        mvOpeningDonkeyMakePosedWallpaper();
        mvOpeningDonkeyMakePosedFighter();
    }
    if (sMVOpeningDonkeyTotalTimeTics == 60)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindOpeningLink;

        syTaskmanSetLoadScene();
    }
}

/* mvopeningdonkey.c:8018DE3C mvOpeningDonkeyInitVars, verbatim. */
void mvOpeningDonkeyInitVars(void)
{
    sMVOpeningDonkeyTotalTimeTics = 0;
}

/* mvopeningdonkey.c:8018DE48 mvOpeningDonkeyFuncStart. DIVERGES: the
 * same substitutions mvOpeningMarioFuncStart already makes (the stage
 * acquire in place of mpCollisionInitGroundData/
 * gmCameraMakeWallpaperCamera, efManagerLoadEffectBank/
 * efManagerPreloadModels in place of efManagerInitEffects, the cut
 * figatree malloc, the cut trailing busy-wait), at the same call sites,
 * for the same reasons -- mvopeningmario.h's Substitutions 3-5. */
void mvOpeningDonkeyFuncStart(void)
{
    Stage *stage;

    sMVOpeningDonkeyBattleState = dSCManagerDefaultBattleState;
    gSCManagerBattleState = &sMVOpeningDonkeyBattleState;

    gSCManagerBattleState->game_type = nSCBattleGameTypeMovie;

    gSCManagerBattleState->gkind = nGRKindJungle;
    gSCManagerBattleState->pl_count = 1;

    gSCManagerBattleState->players[0].fkind = nFTKindDonkey;
    gSCManagerBattleState->players[0].pkind = nFTPlayerKindKey;

    mvOpeningDonkeySetupFiles();

    gcMakeGObjSPAfter(nGCCommonKindMovie, mvOpeningDonkeyFuncRun, 13, GOBJ_PRIORITY_DEFAULT);

    mvOpeningDonkeyInitVars();
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

    /* efManagerInitEffects() is NOT called here -- see
     * mvOpeningMarioFuncStart's own note (mvopeningmario.h's
     * Substitution 3b) for why this scene, carrying a real fighter
     * pack, takes efManagerLoadEffectBank/efManagerPreloadModels
     * instead. */
    efManagerLoadEffectBank();
    efManagerPreloadModels();

    ftManagerSetupFilesAllKind(nFTKindDonkey);

    mvOpeningDonkeyMakeNameCamera();
    mvOpeningDonkeyMakePosedWallpaperCamera();
    mvOpeningDonkeyMakePosedFighterCamera();
    mvOpeningDonkeyMakeName();

    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 1605 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(1605);
}

/* mvopeningdonkey.c:8018DFCC mvOpeningDonkeyFuncLights. Not ported --
 * the standing rule (a Lights1/FuncLights pair never survives
 * test-host, mvopeningroom.h/mvopeningportraits.h): the whole body is
 * ftDisplayLightsDrawReflect, the dropped reflection lighting every
 * other scene's func_lights drops. func_lights is NULL below. */

/* mvopeningdonkey.c:8018E0FC-shaped dMVOpeningDonkeyTaskmanSetup. See
 * the pools note above for the counts, and mvopeningportraits.h for
 * func_draw (scManagerFuncDraw), the zeroed RSP/RDP fields and the
 * pre-render/controller/matrix-list substitutions -- the same blanket
 * rules apply here. */
SYTaskmanSetup dMVOpeningDonkeyTaskmanSetup =
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
    MVOPENINGDONKEY_GOBJPROCS,
    MVOPENINGDONKEY_GOBJS,
    sizeof(GObj),
    MVOPENINGDONKEY_XOBJS,
    NULL,
    NULL,
    MVOPENINGDONKEY_AOBJS,
    MVOPENINGDONKEY_MOBJS,
    MVOPENINGDONKEY_DOBJS,
    sizeof(DObj),
    MVOPENINGDONKEY_SOBJS,
    sizeof(SObj),
    MVOPENINGDONKEY_COBJS,
    sizeof(CObj),

    mvOpeningDonkeyFuncStart
};

/* mvopeningdonkey.c:8018E018 mvOpeningDonkeyStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeningdonkey.h), and the stage release
 * below -- mvopeningmario.c's own StartScene says why it is there and
 * why the kind is a constant. */
void mvOpeningDonkeyStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningDonkeyTaskmanSetup);

    grStageRelease(nGRKindJungle);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[37]. */
void mvOpeningDonkeyOverlayLoad(void)
{
    OVERLAY_CLEAR(sMVOpeningDonkeyTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningDonkeyNameGObj);
    OVERLAY_CLEAR(sMVOpeningDonkeyFighterGObj);
    OVERLAY_CLEAR(sMVOpeningDonkeyMotionCameraGObj);
    OVERLAY_CLEAR(sMVOpeningDonkeyFigatreeHeap);
    OVERLAY_CLEAR(sMVOpeningDonkeyPosedFighterSpeed);
    OVERLAY_CLEAR(sMVOpeningDonkeyAdjustedStartCObjDesc);
    OVERLAY_CLEAR(sMVOpeningDonkeyAdjustedEndCObjDesc);
    OVERLAY_CLEAR(sMVOpeningDonkeyBattleState);
    OVERLAY_CLEAR(sMVOpeningDonkeyNamesBank);
    OVERLAY_CLEAR(sMVOpeningDonkeyNamesFileHead);
    OVERLAY_CLEAR(sMVOpeningDonkeyCamAnimBank);
}
