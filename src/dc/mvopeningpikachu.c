/* mvopeningpikachu.c -- see mvopeningpikachu.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningpikachu.c; the substitutions are
 * explained once, in mvopeningpikachu.h's own header comment (which also
 * names mvopeningmario.h's substitutions 1-5 and mvopeningdonkey.h's
 * substitution 6, all reused here unchanged) -- not repeated at every
 * site.
 */
#include "mvopeningpikachu.h"
#include "overlay.h"

#include "camanim.h"    /* the Pikachu cam-anim script */
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

/* relocData file 37 (IFCommonAnnounceCommon), the seven letters
 * "PIKACHU" spells. Offsets are src/dc/decomp/reloc_data.us.h's own link
 * labels. */
#define llIFCommonAnnounceCommonLetterPSprite 0x04890
#define llIFCommonAnnounceCommonLetterISprite 0x026B8
#define llIFCommonAnnounceCommonLetterKSprite 0x02F98
#define llIFCommonAnnounceCommonLetterASprite 0x005E0
#define llIFCommonAnnounceCommonLetterCSprite 0x00D80
#define llIFCommonAnnounceCommonLetterHSprite 0x02408
#define llIFCommonAnnounceCommonLetterUSprite 0x060D8

/* ---- the pools --------------------------------------------------------
 *
 * Reused verbatim from mvopeningmario.h's own note: one real VS stage
 * (Yamabuki) and up to two real Pikachu fighters (posed + walking), the same
 * budget as every other scene in this chain. */
#define MVOPENINGPIKACHU_GOBJS       48
#define MVOPENINGPIKACHU_GOBJPROCS   48
#define MVOPENINGPIKACHU_XOBJS      288
#define MVOPENINGPIKACHU_AOBJS      576
#define MVOPENINGPIKACHU_MOBJS       32
#define MVOPENINGPIKACHU_DOBJS       96
#define MVOPENINGPIKACHU_SOBJS        8
#define MVOPENINGPIKACHU_COBJS        8

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mvopeningpikachu.c:8018E0B0/E0CC (start/end), verbatim. */
CObjDesc dMVOpeningPikachuCObjDescStart = { { 0.0F, 0.0F, 20000.0F }, { 0.0F, 0.0F, 0.0F }, 0.0F };
CObjDesc dMVOpeningPikachuCObjDescEnd = { { 50.0F, -1640.0F, 1000.0F }, { 50.0F, -1640.0F, 0.0F }, 0.0F };

/* mvopeningpikachu.c dMVOpeningPikachuKeyEvents, verbatim -- empty:
 * just the terminator, the fighter is given no input. */
FTKeyEvent dMVOpeningPikachuKeyEvents[] =
{
    FTKEY_EVENT_END()
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static s32 sMVOpeningPikachuTotalTimeTics;
static GObj *sMVOpeningPikachuNameGObj;
static GObj *sMVOpeningPikachuFighterGObj;
static GObj *sMVOpeningPikachuMotionCameraGObj;

/* mvopeningmario.h's own note (Substitution 4): this stays NULL. */
static void *sMVOpeningPikachuFigatreeHeap;

static f32 sMVOpeningPikachuPosedFighterSpeed;
static CObjDesc sMVOpeningPikachuAdjustedStartCObjDesc;
static CObjDesc sMVOpeningPikachuAdjustedEndCObjDesc;
static SCBattleState sMVOpeningPikachuBattleState;

/* The port's stand-in for sMVOpeningPikachuFiles[0]: file 37's sprite
 * bank (romdisk/ifannounce.spr), the same shape
 * sMVOpeningMarioNamesBank/.../sMVOpeningSamusNamesBank already take.
 * sMVOpeningPikachuNamesFileHead is not static so hosttest_ft.c's own
 * openingyoshi_find can read it. */
static SpriteBank sMVOpeningPikachuNamesBank;
void *sMVOpeningPikachuNamesFileHead;

/* The port's stand-in for sMVOpeningPikachuFiles[1]: the
 * whole of what it wanted out of that file is
 * romdisk/mvopeningcommon.cam's "Pikachu" entry (src/dc/camanim.h). */
static CamAnimBank sMVOpeningPikachuCamAnimBank;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningpikachu.c:8018D0C0 mvOpeningPikachuSetupFiles. DIVERGES: the two
 * loads mvopeningmario.h's header names, in place of
 * lbRelocInitSetup/lbRelocLoadFilesListed. */
void mvOpeningPikachuSetupFiles(void)
{
    if (sprite_bank_load(&sMVOpeningPikachuNamesBank, "ifannounce.spr") < 0)
    {
        sMVOpeningPikachuNamesFileHead = NULL;
    }
    else
    {
        sMVOpeningPikachuNamesFileHead = &sMVOpeningPikachuNamesBank;
    }
    camanim_bank_load(&sMVOpeningPikachuCamAnimBank, "mvopeningcommon.cam");
}

/* mvopeningpikachu.c:8018D160 mvOpeningPikachuInitName, verbatim. */
void mvOpeningPikachuInitName(SObj *sobj)
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

/* mvopeningpikachu.c:8018D194 mvOpeningPikachuMakeName, verbatim but for
 * lbRelocGetFileData's own redefinition above. */
void mvOpeningPikachuMakeName(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 i;

    intptr_t offsets[] =
    {
        llIFCommonAnnounceCommonLetterPSprite,
        llIFCommonAnnounceCommonLetterISprite,
        llIFCommonAnnounceCommonLetterKSprite,
        llIFCommonAnnounceCommonLetterASprite,
        llIFCommonAnnounceCommonLetterCSprite,
        llIFCommonAnnounceCommonLetterHSprite,
        llIFCommonAnnounceCommonLetterUSprite,
        0x0
    };

    f32 pos_x[] =
    {
        0.0F, 30.0F, 45.0F, 75.0F, 110.0F, 140.0F, 170.0F
    };

    sMVOpeningPikachuNameGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; offsets[i] != 0x0; i++)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningPikachuNamesFileHead, offsets[i]));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = pos_x[i] + 65.0F;
        sobj->pos.y = 100.0F;

        mvOpeningPikachuInitName(sobj);
    }
}

/* mvopeningpikachu.c:8018D314 mvOpeningPikachuMotionCameraProcUpdate,
 * verbatim. */
void mvOpeningPikachuMotionCameraProcUpdate(GObj *camera_gobj)
{
    CObj *cobj = CObjGetStruct(camera_gobj);

    if (sMVOpeningPikachuTotalTimeTics >= 15)
    {
        cobj->vec.eye.x += (((sMVOpeningPikachuAdjustedEndCObjDesc.eye.x - sMVOpeningPikachuAdjustedStartCObjDesc.eye.x) / 45.0F));
        cobj->vec.eye.y += (((sMVOpeningPikachuAdjustedEndCObjDesc.eye.y - sMVOpeningPikachuAdjustedStartCObjDesc.eye.y) / 45.0F));
        cobj->vec.eye.z += (((sMVOpeningPikachuAdjustedEndCObjDesc.eye.z - sMVOpeningPikachuAdjustedStartCObjDesc.eye.z) / 45.0F));
        cobj->vec.at.x += (((sMVOpeningPikachuAdjustedEndCObjDesc.at.x - sMVOpeningPikachuAdjustedStartCObjDesc.at.x) / 45.0F));
        cobj->vec.at.y += (((sMVOpeningPikachuAdjustedEndCObjDesc.at.y - sMVOpeningPikachuAdjustedStartCObjDesc.at.y) / 45.0F));
        cobj->vec.at.z += (((sMVOpeningPikachuAdjustedEndCObjDesc.at.z - sMVOpeningPikachuAdjustedStartCObjDesc.at.z) / 45.0F));
        cobj->vec.up.x += (((sMVOpeningPikachuAdjustedEndCObjDesc.upx - sMVOpeningPikachuAdjustedStartCObjDesc.upx) / 45.0F));
    }
}

/* mvopeningpikachu.c:8018D40C mvOpeningPikachuMakeMotionCamera. DIVERGES:
 * gmCameraMakeMovieCamera(NULL) is the decomp's own call (its kind 8 is
 * the roll look-at, objdisplay.c gcCameraLookAtF), and camera_mask is
 * set right after it, the
 * same line scAutoDemoFuncStart makes for its own real camera. */
void mvOpeningPikachuMakeMotionCamera(Vec3f move)
{
    CObj *cobj;

    sMVOpeningPikachuAdjustedStartCObjDesc = dMVOpeningPikachuCObjDescStart;
    sMVOpeningPikachuAdjustedEndCObjDesc = dMVOpeningPikachuCObjDescEnd;

    sMVOpeningPikachuMotionCameraGObj = gmCameraMakeMovieCamera(NULL);
    gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK;

    cobj = CObjGetStruct(sMVOpeningPikachuMotionCameraGObj);

    syRdpSetViewport(&cobj->viewport, 110.0F, 10.0F, 310.0F, 230.0F);

    cobj->projection.persp.aspect = 10.0F / 11.0F;

    gcEndProcessAll(sMVOpeningPikachuMotionCameraGObj);
    gcAddGObjProcess(sMVOpeningPikachuMotionCameraGObj, mvOpeningPikachuMotionCameraProcUpdate, nGCProcessKindFunc, 1);

    sMVOpeningPikachuAdjustedStartCObjDesc.eye.x += move.x;
    sMVOpeningPikachuAdjustedStartCObjDesc.eye.y += move.y;
    sMVOpeningPikachuAdjustedStartCObjDesc.eye.z += move.z;
    sMVOpeningPikachuAdjustedStartCObjDesc.at.x += move.x;
    sMVOpeningPikachuAdjustedStartCObjDesc.at.y += move.y;
    sMVOpeningPikachuAdjustedStartCObjDesc.at.z += move.z;

    sMVOpeningPikachuAdjustedEndCObjDesc.eye.x += move.x;
    sMVOpeningPikachuAdjustedEndCObjDesc.eye.y += move.y;
    sMVOpeningPikachuAdjustedEndCObjDesc.eye.z += move.z;
    sMVOpeningPikachuAdjustedEndCObjDesc.at.x += move.x;
    sMVOpeningPikachuAdjustedEndCObjDesc.at.y += move.y;
    sMVOpeningPikachuAdjustedEndCObjDesc.at.z += move.z;

    cobj->vec.eye.x = sMVOpeningPikachuAdjustedStartCObjDesc.eye.x;
    cobj->vec.eye.y = sMVOpeningPikachuAdjustedStartCObjDesc.eye.y;
    cobj->vec.eye.z = sMVOpeningPikachuAdjustedStartCObjDesc.eye.z;
    cobj->vec.at.x = sMVOpeningPikachuAdjustedStartCObjDesc.at.x;
    cobj->vec.at.y = sMVOpeningPikachuAdjustedStartCObjDesc.at.y;
    cobj->vec.at.z = sMVOpeningPikachuAdjustedStartCObjDesc.at.z;
    cobj->vec.up.x = sMVOpeningPikachuAdjustedStartCObjDesc.upx;
}

/* mvopeningpikachu.c:8018D61C mvOpeningPikachuMakeMotionWindow. DIVERGES:
 * grWallpaperMakeDecideKind()/grCommonSetupInitAll() move from before
 * mvOpeningPikachuMakeMotionCamera to after it (the ordinary Substitution
 * 3 reordering); everything else (the mpCollision calls,
 * gmRumbleMakeActor/ftPublicMakeActor, the fighter loop) is verbatim.
 * Unlike Samus, this scene's own decomp body carries no stage-specific
 * fixup here (no Substitution 7) -- checked directly, not assumed. */
void mvOpeningPikachuMakeMotionWindow(void)
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

    mvOpeningPikachuMakeMotionCamera(pos);

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

        sMVOpeningPikachuFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

        ftParamInitPlayerBattleStats(i, fighter_gobj);
        ftParamSetKey(fighter_gobj, dMVOpeningPikachuKeyEvents);
    }
}

/* mvopeningpikachu.c:8018D874 mvOpeningPikachuPosedWallpaperProcDisplay,
 * verbatim. */
void mvOpeningPikachuPosedWallpaperProcDisplay(GObj *gobj)
{
    (void)gobj;

    /* DIVERGES: the fill quad in place of the raw gDPFillRectangle, as
     * mvopeningmario.c's panel says why. */
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    lbCommonSpriteFillRect(10, 10, 110, 230, 0x6E, 0xAA, 0x6E, 0xFF);
}

/* mvopeningpikachu.c:8018D974 mvOpeningPikachuMakePosedWallpaper, verbatim. */
void mvOpeningPikachuMakePosedWallpaper(void)
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
        mvOpeningPikachuPosedWallpaperProcDisplay,
        28,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
}

/* mvopeningpikachu.c:8018D9C0 mvOpeningPikachuPosedFighterProcUpdate,
 * verbatim -- moves translate.vec.f.y (+=), Donkey's direction
 * (mvopeningpikachu.h's own note). */
void mvOpeningPikachuPosedFighterProcUpdate(GObj *fighter_gobj)
{
    switch (sMVOpeningPikachuTotalTimeTics)
    {
    default:
        break;

    case 15:
        sMVOpeningPikachuPosedFighterSpeed = 17.0F;
        break;

    case 45:
        sMVOpeningPikachuPosedFighterSpeed = 15.0F;
        break;

    case 60:
        sMVOpeningPikachuPosedFighterSpeed = 0.0F;
        break;
    }
    if ((sMVOpeningPikachuTotalTimeTics > 15) && (sMVOpeningPikachuTotalTimeTics < 45))
    {
        sMVOpeningPikachuPosedFighterSpeed += -1.0F / 15.0F;
    }
    if ((sMVOpeningPikachuTotalTimeTics > 45) && (sMVOpeningPikachuTotalTimeTics < 60))
    {
        sMVOpeningPikachuPosedFighterSpeed += -1.0F;
    }
    DObjGetStruct(fighter_gobj)->translate.vec.f.y += sMVOpeningPikachuPosedFighterSpeed;
}

/* mvopeningpikachu.c:8018DA90 mvOpeningPikachuMakePosedFighter, verbatim:
 * desc.figatree_heap reads sMVOpeningPikachuFigatreeHeap, always NULL
 * (mvopeningmario.h's Substitution 4). No desc.lr assignment -- the
 * decomp's own default from dFTManagerDefaultFighterDesc is used
 * unmodified here, as Yoshi's own scene does. */
void mvOpeningPikachuMakePosedFighter(void)
{
    GObj *fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = nFTKindPikachu;
    desc.costume = ftParamGetCostumeCommonID(nFTKindPikachu, 0);
    desc.figatree_heap = sMVOpeningPikachuFigatreeHeap;

    desc.pos.x = 0.0F;
    desc.pos.y = -600.0F;
    desc.pos.z = 0.0F;

    fighter_gobj = ftManagerMakeFighter(&desc);
    scSubsysFighterSetStatus(fighter_gobj, nFTDemoStatusStance);
    gcMoveGObjDL(fighter_gobj, 26, -1);
    gcAddGObjProcess(fighter_gobj, mvOpeningPikachuPosedFighterProcUpdate, nGCProcessKindFunc, 1);

    DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;
}

/* mvopeningpikachu.c:8018DB90 mvOpeningPikachuMakeNameCamera, verbatim. */
void mvOpeningPikachuMakeNameCamera(void)
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

/* mvopeningpikachu.c:8018DC30 mvOpeningPikachuMakePosedFighterCamera.
 * DIVERGES: the AObjEvent32 script's own reach, mvopeningmario.h's
 * Substitution 1 -- camanim_get(&sMVOpeningPikachuCamAnimBank, "Pikachu")
 * in place of lbRelocGetFileData(AObjEvent32*, sMVOpeningPikachuFiles[1],
 * &llMVOpeningCommonPikachuCamAnimJoint). The attach-and-play pair is
 * `#ifdef FT_HOSTTEST`-guarded, the same shape every scene in this
 * chain already uses (mvopeningmario.c's own note says why). */
void mvOpeningPikachuMakePosedFighterCamera(void)
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
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningPikachuCamAnimBank, "Pikachu"), 0.0F);
    gcAddGObjProcess(camera_gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
#endif
}

/* mvopeningpikachu.c:8018DD18 mvOpeningPikachuMakePosedWallpaperCamera,
 * verbatim. */
void mvOpeningPikachuMakePosedWallpaperCamera(void)
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

/* mvopeningpikachu.c:8018DDC0 mvOpeningPikachuFuncRun, verbatim except the
 * eject: gcEjectGObjIfMade in place of a raw gcEjectGObj, the port-wide
 * standing rule (mvopeningportraits.h). */
void mvOpeningPikachuFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningPikachuTotalTimeTics++;

    if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindTitle;

        syTaskmanSetLoadScene();
    }
    if (sMVOpeningPikachuTotalTimeTics == 15)
    {
        gcEjectGObjIfMade(sMVOpeningPikachuNameGObj);
        mvOpeningPikachuMakeMotionWindow();
        mvOpeningPikachuMakePosedWallpaper();
        mvOpeningPikachuMakePosedFighter();
    }
    if (sMVOpeningPikachuTotalTimeTics == 60)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindOpeningRun;

        syTaskmanSetLoadScene();
    }
}

/* mvopeningpikachu.c:8018DE7C mvOpeningPikachuInitVars, verbatim. */
void mvOpeningPikachuInitVars(void)
{
    sMVOpeningPikachuTotalTimeTics = 0;
}

/* mvopeningpikachu.c:8018DE88 mvOpeningPikachuFuncStart. DIVERGES:
 * mvopeningmario.h's Substitutions 1-4, all at their decomp call sites
 * except the stage acquire (grStageAcquire/stage_bind), which is new --
 * gkind is fixed for this whole scene (nGRKindYamabuki). Unlike Samus,
 * this scene's own decomp source adds no extra call beyond Mario's
 * shape (no scSubsysFighterSetLightParams here). */
void mvOpeningPikachuFuncStart(void)
{
    Stage *stage;

    sMVOpeningPikachuBattleState = dSCManagerDefaultBattleState;
    gSCManagerBattleState = &sMVOpeningPikachuBattleState;

    gSCManagerBattleState->game_type = nSCBattleGameTypeMovie;

    gSCManagerBattleState->gkind = nGRKindYamabuki;
    gSCManagerBattleState->pl_count = 1;

    gSCManagerBattleState->players[0].fkind = nFTKindPikachu;
    gSCManagerBattleState->players[0].pkind = nFTPlayerKindKey;

    mvOpeningPikachuSetupFiles();

    gcMakeGObjSPAfter(nGCCommonKindMovie, mvOpeningPikachuFuncRun, nGCCommonLinkIDMovie, GOBJ_PRIORITY_DEFAULT);

    mvOpeningPikachuInitVars();
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

    ftManagerSetupFilesAllKind(nFTKindPikachu);

    mvOpeningPikachuMakeNameCamera();
    mvOpeningPikachuMakePosedWallpaperCamera();
    mvOpeningPikachuMakePosedFighterCamera();
    mvOpeningPikachuMakeName();

    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 2145 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(2145);
}

/* mvopeningpikachu.c:8018E010 mvOpeningPikachuFuncLights. Not ported -- the
 * standing rule (a Lights1/FuncLights pair never survives test-host,
 * mvopeningroom.h/mvopeningportraits.h). func_lights is NULL below. */

/* mvopeningpikachu.c:8018E158-shaped dMVOpeningPikachuTaskmanSetup. See the
 * pools note above for the counts, and mvopeningportraits.h for
 * func_draw (scManagerFuncDraw), the zeroed RSP/RDP fields and the
 * pre-render/controller/matrix-list substitutions -- the same blanket
 * rules apply here. */
SYTaskmanSetup dMVOpeningPikachuTaskmanSetup =
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
    MVOPENINGPIKACHU_GOBJPROCS,
    MVOPENINGPIKACHU_GOBJS,
    sizeof(GObj),
    MVOPENINGPIKACHU_XOBJS,
    NULL,
    NULL,
    MVOPENINGPIKACHU_AOBJS,
    MVOPENINGPIKACHU_MOBJS,
    MVOPENINGPIKACHU_DOBJS,
    sizeof(DObj),
    MVOPENINGPIKACHU_SOBJS,
    sizeof(SObj),
    MVOPENINGPIKACHU_COBJS,
    sizeof(CObj),

    mvOpeningPikachuFuncStart
};

/* mvopeningpikachu.c:8018E05C mvOpeningPikachuStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeningpikachu.h), and the stage release
 * below -- mvopeningmario.c's own StartScene says why it is there and
 * why the kind is a constant. */
void mvOpeningPikachuStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningPikachuTaskmanSetup);

    grStageRelease(nGRKindYamabuki);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[43]. */
void mvOpeningPikachuOverlayLoad(void)
{
    /* the empty key script is one all-zero word, so the compiler puts
     * it in .bss; it stays zero either way */
    OVERLAY_CLEAR(dMVOpeningPikachuKeyEvents);
    OVERLAY_CLEAR(sMVOpeningPikachuTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningPikachuNameGObj);
    OVERLAY_CLEAR(sMVOpeningPikachuFighterGObj);
    OVERLAY_CLEAR(sMVOpeningPikachuMotionCameraGObj);
    OVERLAY_CLEAR(sMVOpeningPikachuFigatreeHeap);
    OVERLAY_CLEAR(sMVOpeningPikachuPosedFighterSpeed);
    OVERLAY_CLEAR(sMVOpeningPikachuAdjustedStartCObjDesc);
    OVERLAY_CLEAR(sMVOpeningPikachuAdjustedEndCObjDesc);
    OVERLAY_CLEAR(sMVOpeningPikachuBattleState);
    OVERLAY_CLEAR(sMVOpeningPikachuNamesBank);
    OVERLAY_CLEAR(sMVOpeningPikachuNamesFileHead);
    OVERLAY_CLEAR(sMVOpeningPikachuCamAnimBank);
}
