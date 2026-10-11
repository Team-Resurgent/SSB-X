/* mvopeningyoshi.c -- see mvopeningyoshi.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningyoshi.c; the substitutions are
 * explained once, in mvopeningyoshi.h's own header comment (which also
 * names mvopeningmario.h's substitutions 1-5 and mvopeningdonkey.h's
 * substitution 6, all reused here unchanged) -- not repeated at every
 * site.
 */
#include "mvopeningyoshi.h"
#include "overlay.h"

#include "camanim.h"    /* the Yoshi cam-anim script */
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
#include <macros.h>                /* I_CONTROLLER_RANGE_MAX */
#include <mp/mpdef.h>              /* nMPMapObjKindMoviePlayer1 */
#include <sc/scsubsys/scsubsys.h>  /* scSubsysFighterSetStatus */
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

/* relocData file 37 (IFCommonAnnounceCommon), the five letters "YOSHI"
 * spells (no repeated letter this time). Offsets are
 * src/dc/decomp/reloc_data.us.h's own ll<Name>Sprite link labels -- S is
 * the exact literal mvopeningsamus.c already uses for its own "S"; Y/O/
 * H/I are new to this scene. */
#define llIFCommonAnnounceCommonLetterYSprite 0x07608
#define llIFCommonAnnounceCommonLetterOSprite 0x044B0
#define llIFCommonAnnounceCommonLetterSSprite 0x057F0
#define llIFCommonAnnounceCommonLetterHSprite 0x02408
#define llIFCommonAnnounceCommonLetterISprite 0x026B8

/* ---- the pools --------------------------------------------------------
 *
 * Reused verbatim from mvopeningmario.h's own note: one real VS stage
 * (Yoster) and up to two real Yoshi fighters (posed + walking), the same
 * budget as every other scene in this chain. */
#define MVOPENINGYOSHI_GOBJS       48
#define MVOPENINGYOSHI_GOBJPROCS   48
#define MVOPENINGYOSHI_XOBJS      288
#define MVOPENINGYOSHI_AOBJS      576
#define MVOPENINGYOSHI_MOBJS       32
#define MVOPENINGYOSHI_DOBJS       96
#define MVOPENINGYOSHI_SOBJS        8
#define MVOPENINGYOSHI_COBJS        8

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mvopeningyoshi.c:8018E0C0/E0DC, verbatim. */
CObjDesc dMVOpeningYoshiCObjDescStart = { { 1200.0F, 150.0F, 1000.0F }, { 100.0F, 200.0F, 0.0F }, 0.0F };
CObjDesc dMVOpeningYoshiCObjDescEnd = { { 2000.0F, 100.0F, 600.0F }, { 1300.0F, 100.0F, -100.0F }, 0.0F };

/* mvopeningyoshi.c:8018E0F8 dMVOpeningYoshiKeyEvents, verbatim -- one
 * more entry than every sibling scene's own script: a leading
 * FTKEY_EVENT_STICK, ahead of the same Z_TRIG/A_BUTTON tap pair. */
FTKeyEvent dMVOpeningYoshiKeyEvents[] =
{
    FTKEY_EVENT_STICK(I_CONTROLLER_RANGE_MAX, 0, 20),
    FTKEY_EVENT_BUTTON(Z_TRIG, 1),
    FTKEY_EVENT_BUTTON(A_BUTTON, 1),
    FTKEY_EVENT_END()
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static s32 sMVOpeningYoshiTotalTimeTics;
static GObj *sMVOpeningYoshiNameGObj;
static GObj *sMVOpeningYoshiFighterGObj;
static GObj *sMVOpeningYoshiStageCameraGObj;

/* mvopeningmario.h's own note (Substitution 4): this stays NULL. */
static void *sMVOpeningYoshiFigatreeHeap;

static f32 sMVOpeningYoshiPosedFighterSpeed;
static CObjDesc sMVOpeningYoshiAdjustedStartCObjDesc;
static CObjDesc sMVOpeningYoshiAdjustedEndCObjDesc;
static SCBattleState sMVOpeningYoshiBattleState;

/* The port's stand-in for sMVOpeningYoshiFiles[0]: file 37's sprite
 * bank (romdisk/ifannounce.spr), the same shape
 * sMVOpeningMarioNamesBank/.../sMVOpeningSamusNamesBank already take.
 * sMVOpeningYoshiNamesFileHead is not static so hosttest_ft.c's own
 * openingyoshi_find can read it. */
static SpriteBank sMVOpeningYoshiNamesBank;
void *sMVOpeningYoshiNamesFileHead;

/* The port's stand-in for sMVOpeningYoshiFiles[1]: the
 * whole of what it wanted out of that file is
 * romdisk/mvopeningcommon.cam's "Yoshi" entry (src/dc/camanim.h). */
static CamAnimBank sMVOpeningYoshiCamAnimBank;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningyoshi.c:8018D0C0 mvOpeningYoshiSetupFiles. DIVERGES: the two
 * loads mvopeningmario.h's header names, in place of
 * lbRelocInitSetup/lbRelocLoadFilesListed. */
void mvOpeningYoshiSetupFiles(void)
{
    if (sprite_bank_load(&sMVOpeningYoshiNamesBank, "ifannounce.spr") < 0)
    {
        sMVOpeningYoshiNamesFileHead = NULL;
    }
    else
    {
        sMVOpeningYoshiNamesFileHead = &sMVOpeningYoshiNamesBank;
    }
    camanim_bank_load(&sMVOpeningYoshiCamAnimBank, "mvopeningcommon.cam");
}

/* mvopeningyoshi.c:8018D160 mvOpeningYoshiInitName, verbatim. */
void mvOpeningYoshiInitName(SObj *sobj)
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

/* mvopeningyoshi.c:8018D194 mvOpeningYoshiMakeName, verbatim but for
 * lbRelocGetFileData's own redefinition above. */
void mvOpeningYoshiMakeName(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 i;

    intptr_t offsets[] =
    {
        llIFCommonAnnounceCommonLetterYSprite,
        llIFCommonAnnounceCommonLetterOSprite,
        llIFCommonAnnounceCommonLetterSSprite,
        llIFCommonAnnounceCommonLetterHSprite,
        llIFCommonAnnounceCommonLetterISprite,
        0x0
    };

    f32 pos_x[] =
    {
        0.0F, 30.0F, 65.0F, 95.0F, 128.0F
    };

    sMVOpeningYoshiNameGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; offsets[i] != 0x0; i++)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningYoshiNamesFileHead, offsets[i]));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = pos_x[i] + 80.0F;
        sobj->pos.y = 100.0F;

        mvOpeningYoshiInitName(sobj);
    }
}

/* mvopeningyoshi.c:8018D314 mvOpeningYoshiMotionCameraProcUpdate,
 * verbatim. */
void mvOpeningYoshiMotionCameraProcUpdate(GObj *camera_gobj)
{
    CObj *cobj = CObjGetStruct(camera_gobj);

    if (sMVOpeningYoshiTotalTimeTics >= 15)
    {
        cobj->vec.eye.x += (((sMVOpeningYoshiAdjustedEndCObjDesc.eye.x - sMVOpeningYoshiAdjustedStartCObjDesc.eye.x) / 45.0F));
        cobj->vec.eye.y += (((sMVOpeningYoshiAdjustedEndCObjDesc.eye.y - sMVOpeningYoshiAdjustedStartCObjDesc.eye.y) / 45.0F));
        cobj->vec.eye.z += (((sMVOpeningYoshiAdjustedEndCObjDesc.eye.z - sMVOpeningYoshiAdjustedStartCObjDesc.eye.z) / 45.0F));
        cobj->vec.at.x += (((sMVOpeningYoshiAdjustedEndCObjDesc.at.x - sMVOpeningYoshiAdjustedStartCObjDesc.at.x) / 45.0F));
        cobj->vec.at.y += (((sMVOpeningYoshiAdjustedEndCObjDesc.at.y - sMVOpeningYoshiAdjustedStartCObjDesc.at.y) / 45.0F));
        cobj->vec.at.z += (((sMVOpeningYoshiAdjustedEndCObjDesc.at.z - sMVOpeningYoshiAdjustedStartCObjDesc.at.z) / 45.0F));
        cobj->vec.up.x += (((sMVOpeningYoshiAdjustedEndCObjDesc.upx - sMVOpeningYoshiAdjustedStartCObjDesc.upx) / 45.0F));
    }
}

/* mvopeningyoshi.c:8018D40C mvOpeningYoshiMakeMotionCamera. DIVERGES:
 * gmCameraMakeMovieCamera(NULL) is the decomp's own call (its kind 8 is
 * the roll look-at, objdisplay.c gcCameraLookAtF), and camera_mask is
 * set right after it, the
 * same line scAutoDemoFuncStart makes for its own real camera. */
void mvOpeningYoshiMakeMotionCamera(Vec3f move)
{
    CObj *cobj;

    sMVOpeningYoshiAdjustedStartCObjDesc = dMVOpeningYoshiCObjDescStart;
    sMVOpeningYoshiAdjustedEndCObjDesc = dMVOpeningYoshiCObjDescEnd;

    sMVOpeningYoshiStageCameraGObj = gmCameraMakeMovieCamera(NULL);
    gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK;

    cobj = CObjGetStruct(sMVOpeningYoshiStageCameraGObj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 150.0F);

    cobj->projection.persp.aspect = 15.0F / 7.0F;

    gcEndProcessAll(sMVOpeningYoshiStageCameraGObj);
    gcAddGObjProcess(sMVOpeningYoshiStageCameraGObj, mvOpeningYoshiMotionCameraProcUpdate, nGCProcessKindFunc, 1);

    sMVOpeningYoshiAdjustedStartCObjDesc.eye.x += move.x;
    sMVOpeningYoshiAdjustedStartCObjDesc.eye.y += move.y;
    sMVOpeningYoshiAdjustedStartCObjDesc.eye.z += move.z;
    sMVOpeningYoshiAdjustedStartCObjDesc.at.x += move.x;
    sMVOpeningYoshiAdjustedStartCObjDesc.at.y += move.y;
    sMVOpeningYoshiAdjustedStartCObjDesc.at.z += move.z;

    sMVOpeningYoshiAdjustedEndCObjDesc.eye.x += move.x;
    sMVOpeningYoshiAdjustedEndCObjDesc.eye.y += move.y;
    sMVOpeningYoshiAdjustedEndCObjDesc.eye.z += move.z;
    sMVOpeningYoshiAdjustedEndCObjDesc.at.x += move.x;
    sMVOpeningYoshiAdjustedEndCObjDesc.at.y += move.y;
    sMVOpeningYoshiAdjustedEndCObjDesc.at.z += move.z;

    cobj->vec.eye.x = sMVOpeningYoshiAdjustedStartCObjDesc.eye.x;
    cobj->vec.eye.y = sMVOpeningYoshiAdjustedStartCObjDesc.eye.y;
    cobj->vec.eye.z = sMVOpeningYoshiAdjustedStartCObjDesc.eye.z;
    cobj->vec.at.x = sMVOpeningYoshiAdjustedStartCObjDesc.at.x;
    cobj->vec.at.y = sMVOpeningYoshiAdjustedStartCObjDesc.at.y;
    cobj->vec.at.z = sMVOpeningYoshiAdjustedStartCObjDesc.at.z;
    cobj->vec.up.x = sMVOpeningYoshiAdjustedStartCObjDesc.upx;
}

/* mvopeningyoshi.c:8018D61C mvOpeningYoshiMakeMotionWindow. DIVERGES:
 * grWallpaperMakeDecideKind()/grCommonSetupInitAll() move from before
 * mvOpeningYoshiMakeMotionCamera to after it (the ordinary Substitution
 * 3 reordering); everything else (the mpCollision calls,
 * gmRumbleMakeActor/ftPublicMakeActor, the fighter loop) is verbatim.
 * Unlike Samus, this scene's own decomp body carries no stage-specific
 * fixup here (no Substitution 7) -- checked directly, not assumed. */
void mvOpeningYoshiMakeMotionWindow(void)
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

    pos.x += -1000.0F;
    pos.y += 70.0F;

    mvOpeningYoshiMakeMotionCamera(pos);

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

        sMVOpeningYoshiFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

        ftParamInitPlayerBattleStats(i, fighter_gobj);
        ftParamSetKey(fighter_gobj, dMVOpeningYoshiKeyEvents);
    }
}

/* mvopeningyoshi.c:8018D874 mvOpeningYoshiPosedWallpaperProcDisplay,
 * verbatim. */
void mvOpeningYoshiPosedWallpaperProcDisplay(GObj *gobj)
{
    (void)gobj;

    /* DIVERGES: the fill quad in place of the raw gDPFillRectangle, as
     * mvopeningmario.c's panel says why. */
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    lbCommonSpriteFillRect(10, 150, 310, 230, 0xFF, 0xBE, 0x5A, 0xFF);
}

/* mvopeningyoshi.c:8018D974 mvOpeningYoshiMakePosedWallpaper, verbatim. */
void mvOpeningYoshiMakePosedWallpaper(void)
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
        mvOpeningYoshiPosedWallpaperProcDisplay,
        28,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
}

/* mvopeningyoshi.c:8018D9C0 mvOpeningYoshiPosedFighterProcUpdate,
 * verbatim -- moves translate.vec.f.x (+=), not .y like Mario's/
 * Samus's own (mvopeningyoshi.h's own note). */
void mvOpeningYoshiPosedFighterProcUpdate(GObj *fighter_gobj)
{
    switch (sMVOpeningYoshiTotalTimeTics)
    {
    default:
        break;

    case 15:
        sMVOpeningYoshiPosedFighterSpeed = 17.0F;
        break;

    case 45:
        sMVOpeningYoshiPosedFighterSpeed = 15.0F;
        break;

    case 60:
        sMVOpeningYoshiPosedFighterSpeed = 0.0F;
        break;
    }
    if ((sMVOpeningYoshiTotalTimeTics > 15) && (sMVOpeningYoshiTotalTimeTics < 45))
    {
        sMVOpeningYoshiPosedFighterSpeed += -1.0F / 15.0F;
    }
    if ((sMVOpeningYoshiTotalTimeTics > 45) && (sMVOpeningYoshiTotalTimeTics < 60))
    {
        sMVOpeningYoshiPosedFighterSpeed += -1.0F;
    }
    DObjGetStruct(fighter_gobj)->translate.vec.f.x += sMVOpeningYoshiPosedFighterSpeed;
}

/* mvopeningyoshi.c:8018DA90 mvOpeningYoshiMakePosedFighter, verbatim:
 * desc.figatree_heap reads sMVOpeningYoshiFigatreeHeap, always NULL
 * (mvopeningmario.h's Substitution 4). No desc.lr assignment -- the
 * decomp's own default from dFTManagerDefaultFighterDesc is used
 * unmodified here, unlike every sibling scene so far. */
void mvOpeningYoshiMakePosedFighter(void)
{
    GObj *fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = nFTKindYoshi;
    desc.costume = ftParamGetCostumeCommonID(nFTKindYoshi, 0);
    desc.figatree_heap = sMVOpeningYoshiFigatreeHeap;

    desc.pos.x = -600.0F;
    desc.pos.y = 0.0F;
    desc.pos.z = 0.0F;

    fighter_gobj = ftManagerMakeFighter(&desc);
    scSubsysFighterSetStatus(fighter_gobj, nFTDemoStatusStance);
    gcMoveGObjDL(fighter_gobj, 26, -1);
    gcAddGObjProcess(fighter_gobj, mvOpeningYoshiPosedFighterProcUpdate, nGCProcessKindFunc, 1);

    DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;
}

/* mvopeningyoshi.c:8018DB90 mvOpeningYoshiMakeNameCamera, verbatim. */
void mvOpeningYoshiMakeNameCamera(void)
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

/* mvopeningyoshi.c:8018DC30 mvOpeningYoshiMakePosedFighterCamera.
 * DIVERGES: the AObjEvent32 script's own reach, mvopeningmario.h's
 * Substitution 1 -- camanim_get(&sMVOpeningYoshiCamAnimBank, "Yoshi")
 * in place of lbRelocGetFileData(AObjEvent32*, sMVOpeningYoshiFiles[1],
 * &llMVOpeningCommonYoshiCamAnimJoint). The attach-and-play pair is
 * `#ifdef FT_HOSTTEST`-guarded, the same shape every scene in this
 * chain already uses (mvopeningmario.c's own note says why). */
void mvOpeningYoshiMakePosedFighterCamera(void)
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

    syRdpSetViewport(&cobj->viewport, 10.0F, 150.0F, 310.0F, 230.0F);

    cobj->projection.persp.aspect = 26.25F / 7.0F;

#ifdef FT_HOSTTEST
    /* the host cannot walk a script; see mvopeningmario.c's own note */
#else
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningYoshiCamAnimBank, "Yoshi"), 0.0F);
    gcAddGObjProcess(camera_gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
#endif
}

/* mvopeningyoshi.c:8018DD18 mvOpeningYoshiMakePosedWallpaperCamera,
 * verbatim. */
void mvOpeningYoshiMakePosedWallpaperCamera(void)
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
    syRdpSetViewport(&cobj->viewport, 10.0F, 150.0F, 310.0F, 230.0F);

    cobj->flags = COBJ_FLAG_DLBUFFERS | COBJ_FLAG_ZBUFFER;
}

/* mvopeningyoshi.c:8018DDC0 mvOpeningYoshiFuncRun, verbatim except the
 * eject: gcEjectGObjIfMade in place of a raw gcEjectGObj, the port-wide
 * standing rule (mvopeningportraits.h). */
void mvOpeningYoshiFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningYoshiTotalTimeTics++;

    if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindTitle;

        syTaskmanSetLoadScene();
    }
    if (sMVOpeningYoshiTotalTimeTics == 15)
    {
        gcEjectGObjIfMade(sMVOpeningYoshiNameGObj);
        mvOpeningYoshiMakeMotionWindow();
        mvOpeningYoshiMakePosedWallpaper();
        mvOpeningYoshiMakePosedFighter();
    }
    if (sMVOpeningYoshiTotalTimeTics == 60)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindOpeningKirby;

        syTaskmanSetLoadScene();
    }
}

/* mvopeningyoshi.c:8018DE7C mvOpeningYoshiInitVars, verbatim. */
void mvOpeningYoshiInitVars(void)
{
    sMVOpeningYoshiTotalTimeTics = 0;
}

/* mvopeningyoshi.c:8018DE88 mvOpeningYoshiFuncStart. DIVERGES:
 * mvopeningmario.h's Substitutions 1-4, all at their decomp call sites
 * except the stage acquire (grStageAcquire/stage_bind), which is new --
 * gkind is fixed for this whole scene (nGRKindYoster). Unlike Samus,
 * this scene's own decomp source adds no extra call beyond Mario's
 * shape (no scSubsysFighterSetLightParams here). */
void mvOpeningYoshiFuncStart(void)
{
    Stage *stage;

    sMVOpeningYoshiBattleState = dSCManagerDefaultBattleState;
    gSCManagerBattleState = &sMVOpeningYoshiBattleState;

    gSCManagerBattleState->game_type = nSCBattleGameTypeMovie;

    gSCManagerBattleState->gkind = nGRKindYoster;
    gSCManagerBattleState->pl_count = 1;

    gSCManagerBattleState->players[0].fkind = nFTKindYoshi;
    gSCManagerBattleState->players[0].pkind = nFTPlayerKindKey;

    mvOpeningYoshiSetupFiles();

    gcMakeGObjSPAfter(nGCCommonKindMovie, mvOpeningYoshiFuncRun, nGCCommonLinkIDMovie, GOBJ_PRIORITY_DEFAULT);

    mvOpeningYoshiInitVars();
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

    ftManagerSetupFilesAllKind(nFTKindYoshi);

    mvOpeningYoshiMakeNameCamera();
    mvOpeningYoshiMakePosedWallpaperCamera();
    mvOpeningYoshiMakePosedFighterCamera();
    mvOpeningYoshiMakeName();

    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 1875 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(1875);
}

/* mvopeningyoshi.c:8018E010 mvOpeningYoshiFuncLights. Not ported -- the
 * standing rule (a Lights1/FuncLights pair never survives test-host,
 * mvopeningroom.h/mvopeningportraits.h). func_lights is NULL below. */

/* mvopeningyoshi.c:8018E158-shaped dMVOpeningYoshiTaskmanSetup. See the
 * pools note above for the counts, and mvopeningportraits.h for
 * func_draw (scManagerFuncDraw), the zeroed RSP/RDP fields and the
 * pre-render/controller/matrix-list substitutions -- the same blanket
 * rules apply here. */
SYTaskmanSetup dMVOpeningYoshiTaskmanSetup =
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
    MVOPENINGYOSHI_GOBJPROCS,
    MVOPENINGYOSHI_GOBJS,
    sizeof(GObj),
    MVOPENINGYOSHI_XOBJS,
    NULL,
    NULL,
    MVOPENINGYOSHI_AOBJS,
    MVOPENINGYOSHI_MOBJS,
    MVOPENINGYOSHI_DOBJS,
    sizeof(DObj),
    MVOPENINGYOSHI_SOBJS,
    sizeof(SObj),
    MVOPENINGYOSHI_COBJS,
    sizeof(CObj),

    mvOpeningYoshiFuncStart
};

/* mvopeningyoshi.c:8018E05C mvOpeningYoshiStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeningyoshi.h), and the stage release
 * below -- mvopeningmario.c's own StartScene says why it is there and
 * why the kind is a constant. */
void mvOpeningYoshiStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningYoshiTaskmanSetup);

    grStageRelease(nGRKindYoster);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[41]. */
void mvOpeningYoshiOverlayLoad(void)
{
    OVERLAY_CLEAR(sMVOpeningYoshiTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningYoshiNameGObj);
    OVERLAY_CLEAR(sMVOpeningYoshiFighterGObj);
    OVERLAY_CLEAR(sMVOpeningYoshiStageCameraGObj);
    OVERLAY_CLEAR(sMVOpeningYoshiFigatreeHeap);
    OVERLAY_CLEAR(sMVOpeningYoshiPosedFighterSpeed);
    OVERLAY_CLEAR(sMVOpeningYoshiAdjustedStartCObjDesc);
    OVERLAY_CLEAR(sMVOpeningYoshiAdjustedEndCObjDesc);
    OVERLAY_CLEAR(sMVOpeningYoshiBattleState);
    OVERLAY_CLEAR(sMVOpeningYoshiNamesBank);
    OVERLAY_CLEAR(sMVOpeningYoshiNamesFileHead);
    OVERLAY_CLEAR(sMVOpeningYoshiCamAnimBank);
}
