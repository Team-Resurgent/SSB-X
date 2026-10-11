/* mvopeningkirby.c -- see mvopeningkirby.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningkirby.c; the substitutions are
 * explained once, in mvopeningkirby.h's own header comment (which also
 * names mvopeningmario.h's substitutions 1-5 and mvopeningdonkey.h's
 * substitution 6, all reused here unchanged) -- not repeated at every
 * site.
 */
#include "mvopeningkirby.h"
#include "overlay.h"

#include "camanim.h"    /* the Kirby cam-anim script */
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

/* relocData file 37 (IFCommonAnnounceCommon), the five letters "KIRBY"
 * spells (no repeated letter). Offsets are src/dc/decomp/reloc_data.us.h's
 * own ll<Name>Sprite link labels. */
#define llIFCommonAnnounceCommonLetterKSprite 0x02F98
#define llIFCommonAnnounceCommonLetterISprite 0x026B8
#define llIFCommonAnnounceCommonLetterRSprite 0x05418
#define llIFCommonAnnounceCommonLetterBSprite 0x009A8
#define llIFCommonAnnounceCommonLetterYSprite 0x07608

/* ---- the pools --------------------------------------------------------
 *
 * Reused verbatim from mvopeningmario.h's own note: one real VS stage
 * (Pupupu) and up to two real Kirby fighters (posed + walking), the same
 * budget as every other scene in this chain. */
#define MVOPENINGKIRBY_GOBJS       48
#define MVOPENINGKIRBY_GOBJPROCS   48
#define MVOPENINGKIRBY_XOBJS      288
#define MVOPENINGKIRBY_AOBJS      576
#define MVOPENINGKIRBY_MOBJS       32
#define MVOPENINGKIRBY_DOBJS       96
#define MVOPENINGKIRBY_SOBJS        8
#define MVOPENINGKIRBY_COBJS        8

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mvopeningkirby.c:8018E0B0/E0CC, verbatim. */
CObjDesc dMVOpeningKirbyCObjDescStart = { { 0.0F, 400.0F, 2000.0F }, { 0.0F, 400.0F, 0.0F }, 0.0F };
CObjDesc dMVOpeningKirbyCObjDescEnd = { { 1100.0F, 400.0F, 1800.0F }, { 1100.0F, 400.0F, 0.0F }, 0.0F };

/* mvopeningkirby.c:8018E0E8 dMVOpeningKirbyKeyEvents, verbatim -- a
 * STICK then a single A_BUTTON tap (no Z_TRIG, unlike the siblings). */
FTKeyEvent dMVOpeningKirbyKeyEvents[] =
{
    FTKEY_EVENT_STICK(45, I_CONTROLLER_RANGE_MAX, 1),
    FTKEY_EVENT_BUTTON(A_BUTTON, 1),
    FTKEY_EVENT_END()
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static s32 sMVOpeningKirbyTotalTimeTics;
static GObj *sMVOpeningKirbyNameGObj;
static GObj *sMVOpeningKirbyFighterGObj;
static GObj *sMVOpeningKirbyMotionCameraGObj;

/* mvopeningmario.h's own note (Substitution 4): this stays NULL. */
static void *sMVOpeningKirbyFigatreeHeap;

static f32 sMVOpeningKirbyPosedFighterSpeed;
static CObjDesc sMVOpeningKirbyAdjustedStartCObjDesc;
static CObjDesc sMVOpeningKirbyAdjustedEndCObjDesc;
static SCBattleState sMVOpeningKirbyBattleState;

/* The port's stand-in for sMVOpeningKirbyFiles[0]: file 37's sprite
 * bank (romdisk/ifannounce.spr), the same shape
 * sMVOpeningMarioNamesBank/.../sMVOpeningSamusNamesBank already take.
 * sMVOpeningKirbyNamesFileHead is not static so hosttest_ft.c's own
 * openingyoshi_find can read it. */
static SpriteBank sMVOpeningKirbyNamesBank;
void *sMVOpeningKirbyNamesFileHead;

/* The port's stand-in for sMVOpeningKirbyFiles[1]: the
 * whole of what it wanted out of that file is
 * romdisk/mvopeningcommon.cam's "Kirby" entry (src/dc/camanim.h). */
static CamAnimBank sMVOpeningKirbyCamAnimBank;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningkirby.c:8018D0C0 mvOpeningKirbySetupFiles. DIVERGES: the two
 * loads mvopeningmario.h's header names, in place of
 * lbRelocInitSetup/lbRelocLoadFilesListed. */
void mvOpeningKirbySetupFiles(void)
{
    if (sprite_bank_load(&sMVOpeningKirbyNamesBank, "ifannounce.spr") < 0)
    {
        sMVOpeningKirbyNamesFileHead = NULL;
    }
    else
    {
        sMVOpeningKirbyNamesFileHead = &sMVOpeningKirbyNamesBank;
    }
    camanim_bank_load(&sMVOpeningKirbyCamAnimBank, "mvopeningcommon.cam");
}

/* mvopeningkirby.c:8018D160 mvOpeningKirbyInitName, verbatim. */
void mvOpeningKirbyInitName(SObj *sobj)
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

/* mvopeningkirby.c:8018D194 mvOpeningKirbyMakeName, verbatim but for
 * lbRelocGetFileData's own redefinition above. */
void mvOpeningKirbyMakeName(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 i;

    intptr_t offsets[] =
    {
        llIFCommonAnnounceCommonLetterKSprite,
        llIFCommonAnnounceCommonLetterISprite,
        llIFCommonAnnounceCommonLetterRSprite,
        llIFCommonAnnounceCommonLetterBSprite,
        llIFCommonAnnounceCommonLetterYSprite,
        0x0
    };

    Vec2f pos[] =
    {
        {   0.0F, 0.0F },
        {  35.0F, 0.0F },
        {  50.0F, 0.0F },
        {  80.0F, 0.0F },
        { 110.0F, 0.0F }
    };

    sMVOpeningKirbyNameGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; offsets[i] != 0x0; i++)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningKirbyNamesFileHead, offsets[i]));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = pos[i].x + 90.0F;
        sobj->pos.y = pos[i].y + 100.0F;

        mvOpeningKirbyInitName(sobj);
    }
}

/* mvopeningkirby.c:8018D314 mvOpeningKirbyMotionCameraProcUpdate,
 * verbatim. */
void mvOpeningKirbyMotionCameraProcUpdate(GObj *camera_gobj)
{
    CObj *cobj = CObjGetStruct(camera_gobj);

    if (sMVOpeningKirbyTotalTimeTics >= 15)
    {
        cobj->vec.eye.x += (((sMVOpeningKirbyAdjustedEndCObjDesc.eye.x - sMVOpeningKirbyAdjustedStartCObjDesc.eye.x) / 45.0F));
        cobj->vec.eye.y += (((sMVOpeningKirbyAdjustedEndCObjDesc.eye.y - sMVOpeningKirbyAdjustedStartCObjDesc.eye.y) / 45.0F));
        cobj->vec.eye.z += (((sMVOpeningKirbyAdjustedEndCObjDesc.eye.z - sMVOpeningKirbyAdjustedStartCObjDesc.eye.z) / 45.0F));
        cobj->vec.at.x += (((sMVOpeningKirbyAdjustedEndCObjDesc.at.x - sMVOpeningKirbyAdjustedStartCObjDesc.at.x) / 45.0F));
        cobj->vec.at.y += (((sMVOpeningKirbyAdjustedEndCObjDesc.at.y - sMVOpeningKirbyAdjustedStartCObjDesc.at.y) / 45.0F));
        cobj->vec.at.z += (((sMVOpeningKirbyAdjustedEndCObjDesc.at.z - sMVOpeningKirbyAdjustedStartCObjDesc.at.z) / 45.0F));
        cobj->vec.up.x += (((sMVOpeningKirbyAdjustedEndCObjDesc.upx - sMVOpeningKirbyAdjustedStartCObjDesc.upx) / 45.0F));
    }
}

/* mvopeningkirby.c:8018D40C mvOpeningKirbyMakeMotionCamera. DIVERGES:
 * gmCameraMakeMovieCamera(NULL) is the decomp's own call (its kind 8 is
 * the roll look-at, objdisplay.c gcCameraLookAtF), and camera_mask is
 * set right after it, the
 * same line scAutoDemoFuncStart makes for its own real camera. */
void mvOpeningKirbyMakeMotionCamera(Vec3f move)
{
    CObj *cobj;

    sMVOpeningKirbyAdjustedStartCObjDesc = dMVOpeningKirbyCObjDescStart;
    sMVOpeningKirbyAdjustedEndCObjDesc = dMVOpeningKirbyCObjDescEnd;

    sMVOpeningKirbyMotionCameraGObj = gmCameraMakeMovieCamera(NULL);
    gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK;

    cobj = CObjGetStruct(sMVOpeningKirbyMotionCameraGObj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 210.0F, 230.0F);

    cobj->projection.persp.aspect = 10.0F / 11.0F;

    gcEndProcessAll(sMVOpeningKirbyMotionCameraGObj);
    gcAddGObjProcess(sMVOpeningKirbyMotionCameraGObj, mvOpeningKirbyMotionCameraProcUpdate, nGCProcessKindFunc, 1);

    sMVOpeningKirbyAdjustedStartCObjDesc.eye.x += move.x;
    sMVOpeningKirbyAdjustedStartCObjDesc.eye.y += move.y;
    sMVOpeningKirbyAdjustedStartCObjDesc.eye.z += move.z;
    sMVOpeningKirbyAdjustedStartCObjDesc.at.x += move.x;
    sMVOpeningKirbyAdjustedStartCObjDesc.at.y += move.y;
    sMVOpeningKirbyAdjustedStartCObjDesc.at.z += move.z;

    sMVOpeningKirbyAdjustedEndCObjDesc.eye.x += move.x;
    sMVOpeningKirbyAdjustedEndCObjDesc.eye.y += move.y;
    sMVOpeningKirbyAdjustedEndCObjDesc.eye.z += move.z;
    sMVOpeningKirbyAdjustedEndCObjDesc.at.x += move.x;
    sMVOpeningKirbyAdjustedEndCObjDesc.at.y += move.y;
    sMVOpeningKirbyAdjustedEndCObjDesc.at.z += move.z;

    cobj->vec.eye.x = sMVOpeningKirbyAdjustedStartCObjDesc.eye.x;
    cobj->vec.eye.y = sMVOpeningKirbyAdjustedStartCObjDesc.eye.y;
    cobj->vec.eye.z = sMVOpeningKirbyAdjustedStartCObjDesc.eye.z;
    cobj->vec.at.x = sMVOpeningKirbyAdjustedStartCObjDesc.at.x;
    cobj->vec.at.y = sMVOpeningKirbyAdjustedStartCObjDesc.at.y;
    cobj->vec.at.z = sMVOpeningKirbyAdjustedStartCObjDesc.at.z;
    cobj->vec.up.x = sMVOpeningKirbyAdjustedStartCObjDesc.upx;
}

/* mvopeningkirby.c:8018D61C mvOpeningKirbyMakeMotionWindow. DIVERGES:
 * grWallpaperMakeDecideKind()/grCommonSetupInitAll() move from before
 * mvOpeningKirbyMakeMotionCamera to after it (the ordinary Substitution
 * 3 reordering); everything else (the mpCollision calls,
 * gmRumbleMakeActor/ftPublicMakeActor, the fighter loop) is verbatim.
 * Unlike Samus, this scene's own decomp body carries no stage-specific
 * fixup here (no Substitution 7) -- checked directly, not assumed. */
void mvOpeningKirbyMakeMotionWindow(void)
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

    pos.y += 30.0F;

    mvOpeningKirbyMakeMotionCamera(pos);

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

        sMVOpeningKirbyFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

        ftParamInitPlayerBattleStats(i, fighter_gobj);
        ftParamSetKey(fighter_gobj, dMVOpeningKirbyKeyEvents);
    }
}

/* mvopeningkirby.c:8018D874 mvOpeningKirbyPosedWallpaperProcDisplay,
 * verbatim. */
void mvOpeningKirbyPosedWallpaperProcDisplay(GObj *gobj)
{
    (void)gobj;

    /* DIVERGES: the fill quad in place of the raw gDPFillRectangle, as
     * mvopeningmario.c's panel says why. */
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    lbCommonSpriteFillRect(210, 10, 310, 230, 0x50, 0xAA, 0xFF, 0xFF);
}

/* mvopeningkirby.c:8018D974 mvOpeningKirbyMakePosedWallpaper, verbatim. */
void mvOpeningKirbyMakePosedWallpaper(void)
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
        mvOpeningKirbyPosedWallpaperProcDisplay,
        28,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
}

/* mvopeningkirby.c:8018D9C0 mvOpeningKirbyPosedFighterProcUpdate,
 * verbatim -- moves translate.vec.f.y (-=), like Mario's own
 * (mvopeningkirby.h's own note). */
void mvOpeningKirbyPosedFighterProcUpdate(GObj *fighter_gobj)
{
    switch (sMVOpeningKirbyTotalTimeTics)
    {
    default:
        break;

    case 15:
        sMVOpeningKirbyPosedFighterSpeed = 17.0F;
        break;

    case 45:
        sMVOpeningKirbyPosedFighterSpeed = 15.0F;
        break;

    case 60:
        sMVOpeningKirbyPosedFighterSpeed = 0.0F;
        break;
    }
    if ((sMVOpeningKirbyTotalTimeTics > 15) && (sMVOpeningKirbyTotalTimeTics < 45))
    {
        sMVOpeningKirbyPosedFighterSpeed += -1.0F / 15.0F;
    }
    if ((sMVOpeningKirbyTotalTimeTics > 45) && (sMVOpeningKirbyTotalTimeTics < 60))
    {
        sMVOpeningKirbyPosedFighterSpeed += -1.0F;
    }
    DObjGetStruct(fighter_gobj)->translate.vec.f.y -= sMVOpeningKirbyPosedFighterSpeed;
}

/* mvopeningkirby.c:8018DA90 mvOpeningKirbyMakePosedFighter, verbatim:
 * desc.figatree_heap reads sMVOpeningKirbyFigatreeHeap, always NULL
 * (mvopeningmario.h's Substitution 4). No desc.lr assignment -- the
 * decomp's own default from dFTManagerDefaultFighterDesc is used
 * unmodified here, as Yoshi's own scene does. */
void mvOpeningKirbyMakePosedFighter(void)
{
    GObj *fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = nFTKindKirby;
    desc.costume = ftParamGetCostumeCommonID(nFTKindKirby, 0);
    desc.figatree_heap = sMVOpeningKirbyFigatreeHeap;

    desc.pos.x = 0.0F;
    desc.pos.y = 600.0F;
    desc.pos.z = 0.0F;

    fighter_gobj = ftManagerMakeFighter(&desc);
    scSubsysFighterSetStatus(fighter_gobj, nFTDemoStatusStance);
    gcMoveGObjDL(fighter_gobj, 26, -1);
    gcAddGObjProcess(fighter_gobj, mvOpeningKirbyPosedFighterProcUpdate, nGCProcessKindFunc, 1);

    DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;
}

/* mvopeningkirby.c:8018DB90 mvOpeningKirbyMakeNameCamera, verbatim. */
void mvOpeningKirbyMakeNameCamera(void)
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

/* mvopeningkirby.c:8018DC30 mvOpeningKirbyMakePosedFighterCamera.
 * DIVERGES: the AObjEvent32 script's own reach, mvopeningmario.h's
 * Substitution 1 -- camanim_get(&sMVOpeningKirbyCamAnimBank, "Kirby")
 * in place of lbRelocGetFileData(AObjEvent32*, sMVOpeningKirbyFiles[1],
 * &llMVOpeningCommonKirbyCamAnimJoint). The attach-and-play pair is
 * `#ifdef FT_HOSTTEST`-guarded, the same shape every scene in this
 * chain already uses (mvopeningmario.c's own note says why). */
void mvOpeningKirbyMakePosedFighterCamera(void)
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

    syRdpSetViewport(&cobj->viewport, 210.0F, 10.0F, 310.0F, 230.0F);

    cobj->projection.persp.aspect = 5.0F / 11.0F;

#ifdef FT_HOSTTEST
    /* the host cannot walk a script; see mvopeningmario.c's own note */
#else
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningKirbyCamAnimBank, "Kirby"), 0.0F);
    gcAddGObjProcess(camera_gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
#endif
}

/* mvopeningkirby.c:8018DD18 mvOpeningKirbyMakePosedWallpaperCamera,
 * verbatim. */
void mvOpeningKirbyMakePosedWallpaperCamera(void)
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
    syRdpSetViewport(&cobj->viewport, 210.0F, 10.0F, 310.0F, 230.0F);

    cobj->flags = COBJ_FLAG_DLBUFFERS | COBJ_FLAG_ZBUFFER;
}

/* mvopeningkirby.c:8018DDC0 mvOpeningKirbyFuncRun, verbatim except the
 * eject: gcEjectGObjIfMade in place of a raw gcEjectGObj, the port-wide
 * standing rule (mvopeningportraits.h). */
void mvOpeningKirbyFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningKirbyTotalTimeTics++;

    if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindTitle;

        syTaskmanSetLoadScene();
    }
    if (sMVOpeningKirbyTotalTimeTics == 15)
    {
        gcEjectGObjIfMade(sMVOpeningKirbyNameGObj);
        mvOpeningKirbyMakeMotionWindow();
        mvOpeningKirbyMakePosedWallpaper();
        mvOpeningKirbyMakePosedFighter();
    }
    if (sMVOpeningKirbyTotalTimeTics == 60)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindOpeningFox;

        syTaskmanSetLoadScene();
    }
}

/* mvopeningkirby.c:8018DE7C mvOpeningKirbyInitVars, verbatim. */
void mvOpeningKirbyInitVars(void)
{
    sMVOpeningKirbyTotalTimeTics = 0;
}

/* mvopeningkirby.c:8018DE88 mvOpeningKirbyFuncStart. DIVERGES:
 * mvopeningmario.h's Substitutions 1-4, all at their decomp call sites
 * except the stage acquire (grStageAcquire/stage_bind), which is new --
 * gkind is fixed for this whole scene (nGRKindPupupu). Unlike Samus,
 * this scene's own decomp source adds no extra call beyond Mario's
 * shape (no scSubsysFighterSetLightParams here). */
void mvOpeningKirbyFuncStart(void)
{
    Stage *stage;

    sMVOpeningKirbyBattleState = dSCManagerDefaultBattleState;
    gSCManagerBattleState = &sMVOpeningKirbyBattleState;

    gSCManagerBattleState->game_type = nSCBattleGameTypeMovie;

    gSCManagerBattleState->gkind = nGRKindPupupu;
    gSCManagerBattleState->pl_count = 1;

    gSCManagerBattleState->players[0].fkind = nFTKindKirby;
    gSCManagerBattleState->players[0].pkind = nFTPlayerKindKey;

    mvOpeningKirbySetupFiles();

    gcMakeGObjSPAfter(nGCCommonKindMovie, mvOpeningKirbyFuncRun, nGCCommonLinkIDMovie, GOBJ_PRIORITY_DEFAULT);

    mvOpeningKirbyInitVars();
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

    ftManagerSetupFilesAllKind(nFTKindKirby);

    mvOpeningKirbyMakeNameCamera();
    mvOpeningKirbyMakePosedWallpaperCamera();
    mvOpeningKirbyMakePosedFighterCamera();
    mvOpeningKirbyMakeName();

    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 1965 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(1965);
}

/* mvopeningkirby.c:8018E010 mvOpeningKirbyFuncLights. Not ported -- the
 * standing rule (a Lights1/FuncLights pair never survives test-host,
 * mvopeningroom.h/mvopeningportraits.h). func_lights is NULL below. */

/* mvopeningkirby.c:8018E158-shaped dMVOpeningKirbyTaskmanSetup. See the
 * pools note above for the counts, and mvopeningportraits.h for
 * func_draw (scManagerFuncDraw), the zeroed RSP/RDP fields and the
 * pre-render/controller/matrix-list substitutions -- the same blanket
 * rules apply here. */
SYTaskmanSetup dMVOpeningKirbyTaskmanSetup =
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
    MVOPENINGKIRBY_GOBJPROCS,
    MVOPENINGKIRBY_GOBJS,
    sizeof(GObj),
    MVOPENINGKIRBY_XOBJS,
    NULL,
    NULL,
    MVOPENINGKIRBY_AOBJS,
    MVOPENINGKIRBY_MOBJS,
    MVOPENINGKIRBY_DOBJS,
    sizeof(DObj),
    MVOPENINGKIRBY_SOBJS,
    sizeof(SObj),
    MVOPENINGKIRBY_COBJS,
    sizeof(CObj),

    mvOpeningKirbyFuncStart
};

/* mvopeningkirby.c:8018E05C mvOpeningKirbyStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeningkirby.h), and the stage release
 * below -- mvopeningmario.c's own StartScene says why it is there and
 * why the kind is a constant. */
void mvOpeningKirbyStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningKirbyTaskmanSetup);

    grStageRelease(nGRKindPupupu);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[43]. */
void mvOpeningKirbyOverlayLoad(void)
{
    OVERLAY_CLEAR(sMVOpeningKirbyTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningKirbyNameGObj);
    OVERLAY_CLEAR(sMVOpeningKirbyFighterGObj);
    OVERLAY_CLEAR(sMVOpeningKirbyMotionCameraGObj);
    OVERLAY_CLEAR(sMVOpeningKirbyFigatreeHeap);
    OVERLAY_CLEAR(sMVOpeningKirbyPosedFighterSpeed);
    OVERLAY_CLEAR(sMVOpeningKirbyAdjustedStartCObjDesc);
    OVERLAY_CLEAR(sMVOpeningKirbyAdjustedEndCObjDesc);
    OVERLAY_CLEAR(sMVOpeningKirbyBattleState);
    OVERLAY_CLEAR(sMVOpeningKirbyNamesBank);
    OVERLAY_CLEAR(sMVOpeningKirbyNamesFileHead);
    OVERLAY_CLEAR(sMVOpeningKirbyCamAnimBank);
}
