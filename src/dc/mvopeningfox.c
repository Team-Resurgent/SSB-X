/* mvopeningfox.c -- see mvopeningfox.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningfox.c; the substitutions are
 * explained once, in mvopeningfox.h's own header comment (which also
 * names mvopeningmario.h's substitutions 1-5 and mvopeningdonkey.h's
 * substitution 6, all reused here unchanged) -- not repeated at every
 * site.
 */
#include "mvopeningfox.h"
#include "overlay.h"

#include "camanim.h"    /* the Fox cam-anim script */
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

/* relocData file 37 (IFCommonAnnounceCommon), the three letters "FOX"
 * spells. Offsets are src/dc/decomp/reloc_data.us.h's own link labels;
 * O is the literal mvopeningyoshi.c already uses. */
#define llIFCommonAnnounceCommonLetterFSprite 0x01A00
#define llIFCommonAnnounceCommonLetterOSprite 0x044B0
#define llIFCommonAnnounceCommonLetterXSprite 0x07108

/* ---- the pools --------------------------------------------------------
 *
 * Reused verbatim from mvopeningmario.h's own note: one real VS stage
 * (Sector) and up to two real Fox fighters (posed + walking), the same
 * budget as every other scene in this chain. */
#define MVOPENINGFOX_GOBJS       48
#define MVOPENINGFOX_GOBJPROCS   48
#define MVOPENINGFOX_XOBJS      288
#define MVOPENINGFOX_AOBJS      576
#define MVOPENINGFOX_MOBJS       32
#define MVOPENINGFOX_DOBJS       96
#define MVOPENINGFOX_SOBJS        8
#define MVOPENINGFOX_COBJS        8

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mvopeningfox.c:8018E090/E0AC, verbatim. */
CObjDesc dMVOpeningFoxCObjDescStart = { { -400.0F, 320.0F, 100.0F }, { 0.0F, 320.0F, 0.0F }, 0.0F };
CObjDesc dMVOpeningFoxCObjDescEnd = { { -3000.0F, 300.0F, 250.0F }, { 0.0F, 300.0F, -200.0F }, 0.7F };

/* mvopeningfox.c:8018E0C8 dMVOpeningFoxKeyEvents, verbatim -- a STICK,
 * then two B taps each followed by a 12-tic release. */
FTKeyEvent dMVOpeningFoxKeyEvents[] =
{
    FTKEY_EVENT_STICK(-50, 0, 1),
    FTKEY_EVENT_BUTTON(B_BUTTON, 1),
    FTKEY_EVENT_BUTTON(0, 12),
    FTKEY_EVENT_BUTTON(B_BUTTON, 1),
    FTKEY_EVENT_BUTTON(0, 12),
    FTKEY_EVENT_END()
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static s32 sMVOpeningFoxTotalTimeTics;
static GObj *sMVOpeningFoxNameGObj;
static GObj *sMVOpeningFoxFighterGObj;
static GObj *sMVOpeningFoxMotionCameraGObj;

/* mvopeningmario.h's own note (Substitution 4): this stays NULL. */
static void *sMVOpeningFoxFigatreeHeap;

static f32 sMVOpeningFoxPosedFighterSpeed;
static CObjDesc sMVOpeningFoxAdjustedStartCObjDesc;
static CObjDesc sMVOpeningFoxAdjustedEndCObjDesc;
static SCBattleState sMVOpeningFoxBattleState;

/* The port's stand-in for sMVOpeningFoxFiles[0]: file 37's sprite
 * bank (romdisk/ifannounce.spr), the same shape
 * sMVOpeningMarioNamesBank/.../sMVOpeningSamusNamesBank already take.
 * sMVOpeningFoxNamesFileHead is not static so hosttest_ft.c's own
 * openingyoshi_find can read it. */
static SpriteBank sMVOpeningFoxNamesBank;
void *sMVOpeningFoxNamesFileHead;

/* The port's stand-in for sMVOpeningFoxFiles[1]: the
 * whole of what it wanted out of that file is
 * romdisk/mvopeningcommon.cam's "Fox" entry (src/dc/camanim.h). */
static CamAnimBank sMVOpeningFoxCamAnimBank;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningfox.c:8018D0C0 mvOpeningFoxSetupFiles. DIVERGES: the two
 * loads mvopeningmario.h's header names, in place of
 * lbRelocInitSetup/lbRelocLoadFilesListed. */
void mvOpeningFoxSetupFiles(void)
{
    if (sprite_bank_load(&sMVOpeningFoxNamesBank, "ifannounce.spr") < 0)
    {
        sMVOpeningFoxNamesFileHead = NULL;
    }
    else
    {
        sMVOpeningFoxNamesFileHead = &sMVOpeningFoxNamesBank;
    }
    camanim_bank_load(&sMVOpeningFoxCamAnimBank, "mvopeningcommon.cam");
}

/* mvopeningfox.c:8018D160 mvOpeningFoxInitName, verbatim. */
void mvOpeningFoxInitName(SObj *sobj)
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

/* mvopeningfox.c:8018D194 mvOpeningFoxMakeName, verbatim but for
 * lbRelocGetFileData's own redefinition above. */
void mvOpeningFoxMakeName(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 i;

    intptr_t offsets[] =
    {
        llIFCommonAnnounceCommonLetterFSprite,
        llIFCommonAnnounceCommonLetterOSprite,
        llIFCommonAnnounceCommonLetterXSprite,
        0x0
    };

    Vec2f pos[] =
    {
        {  0.0F, 0.0F },
        { 30.0F, 0.0F },
        { 75.0F, 0.0F }
    };

    sMVOpeningFoxNameGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; offsets[i] != 0x0; i++)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningFoxNamesFileHead, offsets[i]));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = pos[i].x + 110.0F;
        sobj->pos.y = pos[i].y + 100.0F;

        mvOpeningFoxInitName(sobj);
    }
}

/* mvopeningfox.c:8018D314 mvOpeningFoxMotionCameraProcUpdate,
 * verbatim. */
void mvOpeningFoxMotionCameraProcUpdate(GObj *camera_gobj)
{
    CObj *cobj = CObjGetStruct(camera_gobj);

    if (sMVOpeningFoxTotalTimeTics >= 15)
    {
        cobj->vec.eye.x += (((sMVOpeningFoxAdjustedEndCObjDesc.eye.x - sMVOpeningFoxAdjustedStartCObjDesc.eye.x) / 45.0F));
        cobj->vec.eye.y += (((sMVOpeningFoxAdjustedEndCObjDesc.eye.y - sMVOpeningFoxAdjustedStartCObjDesc.eye.y) / 45.0F));
        cobj->vec.eye.z += (((sMVOpeningFoxAdjustedEndCObjDesc.eye.z - sMVOpeningFoxAdjustedStartCObjDesc.eye.z) / 45.0F));
        cobj->vec.at.x += (((sMVOpeningFoxAdjustedEndCObjDesc.at.x - sMVOpeningFoxAdjustedStartCObjDesc.at.x) / 45.0F));
        cobj->vec.at.y += (((sMVOpeningFoxAdjustedEndCObjDesc.at.y - sMVOpeningFoxAdjustedStartCObjDesc.at.y) / 45.0F));
        cobj->vec.at.z += (((sMVOpeningFoxAdjustedEndCObjDesc.at.z - sMVOpeningFoxAdjustedStartCObjDesc.at.z) / 45.0F));
        cobj->vec.up.x += (((sMVOpeningFoxAdjustedEndCObjDesc.upx - sMVOpeningFoxAdjustedStartCObjDesc.upx) / 45.0F));
    }
}

/* mvopeningfox.c:8018D40C mvOpeningFoxMakeMotionCamera. DIVERGES:
 * gmCameraMakeMovieCamera(NULL) is the decomp's own call (its kind 8 is
 * the roll look-at, objdisplay.c gcCameraLookAtF), and camera_mask is
 * set right after it, the
 * same line scAutoDemoFuncStart makes for its own real camera. */
void mvOpeningFoxMakeMotionCamera(Vec3f move)
{
    CObj *cobj;

    sMVOpeningFoxAdjustedStartCObjDesc = dMVOpeningFoxCObjDescStart;
    sMVOpeningFoxAdjustedEndCObjDesc = dMVOpeningFoxCObjDescEnd;

    sMVOpeningFoxMotionCameraGObj = gmCameraMakeMovieCamera(NULL);
    gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK;

    cobj = CObjGetStruct(sMVOpeningFoxMotionCameraGObj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 210.0F, 230.0F);

    cobj->projection.persp.aspect = 10.0F / 11.0F;

    gcEndProcessAll(sMVOpeningFoxMotionCameraGObj);
    gcAddGObjProcess(sMVOpeningFoxMotionCameraGObj, mvOpeningFoxMotionCameraProcUpdate, nGCProcessKindFunc, 1);

    sMVOpeningFoxAdjustedStartCObjDesc.eye.x += move.x;
    sMVOpeningFoxAdjustedStartCObjDesc.eye.y += move.y;
    sMVOpeningFoxAdjustedStartCObjDesc.eye.z += move.z;
    sMVOpeningFoxAdjustedStartCObjDesc.at.x += move.x;
    sMVOpeningFoxAdjustedStartCObjDesc.at.y += move.y;
    sMVOpeningFoxAdjustedStartCObjDesc.at.z += move.z;

    sMVOpeningFoxAdjustedEndCObjDesc.eye.x += move.x;
    sMVOpeningFoxAdjustedEndCObjDesc.eye.y += move.y;
    sMVOpeningFoxAdjustedEndCObjDesc.eye.z += move.z;
    sMVOpeningFoxAdjustedEndCObjDesc.at.x += move.x;
    sMVOpeningFoxAdjustedEndCObjDesc.at.y += move.y;
    sMVOpeningFoxAdjustedEndCObjDesc.at.z += move.z;

    cobj->vec.eye.x = sMVOpeningFoxAdjustedStartCObjDesc.eye.x;
    cobj->vec.eye.y = sMVOpeningFoxAdjustedStartCObjDesc.eye.y;
    cobj->vec.eye.z = sMVOpeningFoxAdjustedStartCObjDesc.eye.z;
    cobj->vec.at.x = sMVOpeningFoxAdjustedStartCObjDesc.at.x;
    cobj->vec.at.y = sMVOpeningFoxAdjustedStartCObjDesc.at.y;
    cobj->vec.at.z = sMVOpeningFoxAdjustedStartCObjDesc.at.z;
    cobj->vec.up.x = sMVOpeningFoxAdjustedStartCObjDesc.upx;
}

/* mvopeningfox.c:8018D61C mvOpeningFoxMakeMotionWindow. DIVERGES:
 * grWallpaperMakeDecideKind()/grCommonSetupInitAll() move from before
 * mvOpeningFoxMakeMotionCamera to after it (the ordinary Substitution
 * 3 reordering); everything else (the mpCollision calls,
 * gmRumbleMakeActor/ftPublicMakeActor, the fighter loop) is verbatim.
 * Unlike Samus, this scene's own decomp body carries no stage-specific
 * fixup here (no Substitution 7) -- checked directly, not assumed. */
void mvOpeningFoxMakeMotionWindow(void)
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

    mvOpeningFoxMakeMotionCamera(pos);

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

        sMVOpeningFoxFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

        ftParamInitPlayerBattleStats(i, fighter_gobj);
        ftParamSetKey(fighter_gobj, dMVOpeningFoxKeyEvents);
    }
}

/* mvopeningfox.c:8018D874 mvOpeningFoxPosedWallpaperProcDisplay,
 * verbatim. */
void mvOpeningFoxPosedWallpaperProcDisplay(GObj *gobj)
{
    (void)gobj;

    /* DIVERGES: the fill quad in place of the raw gDPFillRectangle, as
     * mvopeningmario.c's panel says why. */
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    lbCommonSpriteFillRect(210, 10, 310, 230, 0x00, 0x3C, 0x28, 0xFF);
}

/* mvopeningfox.c:8018D974 mvOpeningFoxMakePosedWallpaper, verbatim. */
void mvOpeningFoxMakePosedWallpaper(void)
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
        mvOpeningFoxPosedWallpaperProcDisplay,
        28,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
}

/* mvopeningfox.c:8018D9C0 mvOpeningFoxPosedFighterProcUpdate,
 * verbatim -- moves translate.vec.f.y (-=), like Mario's own
 * (mvopeningfox.h's own note). */
void mvOpeningFoxPosedFighterProcUpdate(GObj *fighter_gobj)
{
    switch (sMVOpeningFoxTotalTimeTics)
    {
    default:
        break;

    case 15:
        sMVOpeningFoxPosedFighterSpeed = 17.0F;
        break;

    case 45:
        sMVOpeningFoxPosedFighterSpeed = 15.0F;
        break;

    case 60:
        sMVOpeningFoxPosedFighterSpeed = 0.0F;
        break;
    }
    if ((sMVOpeningFoxTotalTimeTics > 15) && (sMVOpeningFoxTotalTimeTics < 45))
    {
        sMVOpeningFoxPosedFighterSpeed += -1.0F / 15.0F;
    }
    if ((sMVOpeningFoxTotalTimeTics > 45) && (sMVOpeningFoxTotalTimeTics < 60))
    {
        sMVOpeningFoxPosedFighterSpeed += -1.0F;
    }
    DObjGetStruct(fighter_gobj)->translate.vec.f.y -= sMVOpeningFoxPosedFighterSpeed;
}

/* mvopeningfox.c:8018DA90 mvOpeningFoxMakePosedFighter, verbatim:
 * desc.figatree_heap reads sMVOpeningFoxFigatreeHeap, always NULL
 * (mvopeningmario.h's Substitution 4). No desc.lr assignment -- the
 * decomp's own default from dFTManagerDefaultFighterDesc is used
 * unmodified here, as Yoshi's own scene does. */
void mvOpeningFoxMakePosedFighter(void)
{
    GObj *fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = nFTKindFox;
    desc.costume = ftParamGetCostumeCommonID(nFTKindFox, 0);
    desc.figatree_heap = sMVOpeningFoxFigatreeHeap;

    desc.pos.x = 0.0F;
    desc.pos.y = 600.0F;
    desc.pos.z = 0.0F;

    fighter_gobj = ftManagerMakeFighter(&desc);
    scSubsysFighterSetStatus(fighter_gobj, nFTDemoStatusStance);
    gcMoveGObjDL(fighter_gobj, 26, -1);
    gcAddGObjProcess(fighter_gobj, mvOpeningFoxPosedFighterProcUpdate, nGCProcessKindFunc, 1);

    DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;
}

/* mvopeningfox.c:8018DB90 mvOpeningFoxMakeNameCamera, verbatim. */
void mvOpeningFoxMakeNameCamera(void)
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

/* mvopeningfox.c:8018DC30 mvOpeningFoxMakePosedFighterCamera.
 * DIVERGES: the AObjEvent32 script's own reach, mvopeningmario.h's
 * Substitution 1 -- camanim_get(&sMVOpeningFoxCamAnimBank, "Fox")
 * in place of lbRelocGetFileData(AObjEvent32*, sMVOpeningFoxFiles[1],
 * &llMVOpeningCommonFoxCamAnimJoint). The attach-and-play pair is
 * `#ifdef FT_HOSTTEST`-guarded, the same shape every scene in this
 * chain already uses (mvopeningmario.c's own note says why). */
void mvOpeningFoxMakePosedFighterCamera(void)
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
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningFoxCamAnimBank, "Fox"), 0.0F);
    gcAddGObjProcess(camera_gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
#endif
}

/* mvopeningfox.c:8018DD18 mvOpeningFoxMakePosedWallpaperCamera,
 * verbatim. */
void mvOpeningFoxMakePosedWallpaperCamera(void)
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

/* mvopeningfox.c:8018DDC0 mvOpeningFoxFuncRun, verbatim except the
 * eject: gcEjectGObjIfMade in place of a raw gcEjectGObj, the port-wide
 * standing rule (mvopeningportraits.h). */
void mvOpeningFoxFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningFoxTotalTimeTics++;

    if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindTitle;

        syTaskmanSetLoadScene();
    }
    if (sMVOpeningFoxTotalTimeTics == 15)
    {
        gcEjectGObjIfMade(sMVOpeningFoxNameGObj);
        mvOpeningFoxMakeMotionWindow();
        mvOpeningFoxMakePosedWallpaper();
        mvOpeningFoxMakePosedFighter();
    }
    if (sMVOpeningFoxTotalTimeTics == 60)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindOpeningPikachu;

        syTaskmanSetLoadScene();
    }
}

/* mvopeningfox.c:8018DE7C mvOpeningFoxInitVars, verbatim. */
void mvOpeningFoxInitVars(void)
{
    sMVOpeningFoxTotalTimeTics = 0;
}

/* mvopeningfox.c:8018DE88 mvOpeningFoxFuncStart. DIVERGES:
 * mvopeningmario.h's Substitutions 1-4, all at their decomp call sites
 * except the stage acquire (grStageAcquire/stage_bind), which is new --
 * gkind is fixed for this whole scene (nGRKindSector). Unlike Samus,
 * this scene's own decomp source adds no extra call beyond Mario's
 * shape (no scSubsysFighterSetLightParams here). */
void mvOpeningFoxFuncStart(void)
{
    Stage *stage;

    sMVOpeningFoxBattleState = dSCManagerDefaultBattleState;
    gSCManagerBattleState = &sMVOpeningFoxBattleState;

    gSCManagerBattleState->game_type = nSCBattleGameTypeMovie;

    gSCManagerBattleState->gkind = nGRKindSector;
    gSCManagerBattleState->pl_count = 1;

    gSCManagerBattleState->players[0].fkind = nFTKindFox;
    gSCManagerBattleState->players[0].pkind = nFTPlayerKindKey;

    mvOpeningFoxSetupFiles();

    gcMakeGObjSPAfter(nGCCommonKindMovie, mvOpeningFoxFuncRun, nGCCommonLinkIDMovie, GOBJ_PRIORITY_DEFAULT);

    mvOpeningFoxInitVars();
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

    ftManagerSetupFilesAllKind(nFTKindFox);

    mvOpeningFoxMakeNameCamera();
    mvOpeningFoxMakePosedWallpaperCamera();
    mvOpeningFoxMakePosedFighterCamera();
    mvOpeningFoxMakeName();

    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 2055 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(2055);
}

/* mvopeningfox.c:8018E010 mvOpeningFoxFuncLights. Not ported -- the
 * standing rule (a Lights1/FuncLights pair never survives test-host,
 * mvopeningroom.h/mvopeningportraits.h). func_lights is NULL below. */

/* mvopeningfox.c:8018E158-shaped dMVOpeningFoxTaskmanSetup. See the
 * pools note above for the counts, and mvopeningportraits.h for
 * func_draw (scManagerFuncDraw), the zeroed RSP/RDP fields and the
 * pre-render/controller/matrix-list substitutions -- the same blanket
 * rules apply here. */
SYTaskmanSetup dMVOpeningFoxTaskmanSetup =
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
    MVOPENINGFOX_GOBJPROCS,
    MVOPENINGFOX_GOBJS,
    sizeof(GObj),
    MVOPENINGFOX_XOBJS,
    NULL,
    NULL,
    MVOPENINGFOX_AOBJS,
    MVOPENINGFOX_MOBJS,
    MVOPENINGFOX_DOBJS,
    sizeof(DObj),
    MVOPENINGFOX_SOBJS,
    sizeof(SObj),
    MVOPENINGFOX_COBJS,
    sizeof(CObj),

    mvOpeningFoxFuncStart
};

/* mvopeningfox.c:8018E05C mvOpeningFoxStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeningfox.h), and the stage release
 * below -- mvopeningmario.c's own StartScene says why it is there and
 * why the kind is a constant. */
void mvOpeningFoxStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningFoxTaskmanSetup);

    grStageRelease(nGRKindSector);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[43]. */
void mvOpeningFoxOverlayLoad(void)
{
    OVERLAY_CLEAR(sMVOpeningFoxTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningFoxNameGObj);
    OVERLAY_CLEAR(sMVOpeningFoxFighterGObj);
    OVERLAY_CLEAR(sMVOpeningFoxMotionCameraGObj);
    OVERLAY_CLEAR(sMVOpeningFoxFigatreeHeap);
    OVERLAY_CLEAR(sMVOpeningFoxPosedFighterSpeed);
    OVERLAY_CLEAR(sMVOpeningFoxAdjustedStartCObjDesc);
    OVERLAY_CLEAR(sMVOpeningFoxAdjustedEndCObjDesc);
    OVERLAY_CLEAR(sMVOpeningFoxBattleState);
    OVERLAY_CLEAR(sMVOpeningFoxNamesBank);
    OVERLAY_CLEAR(sMVOpeningFoxNamesFileHead);
    OVERLAY_CLEAR(sMVOpeningFoxCamAnimBank);
}
