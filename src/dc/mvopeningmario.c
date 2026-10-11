/* mvopeningmario.c -- see mvopeningmario.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningmario.c; every function names
 * its line range. The five substitutions (the two relocData files, the
 * dropped FILLCOLOR camera, the real-ground/real-fighter recipe, the
 * figatree heap and the trailing busy-wait) are explained once, in
 * mvopeningmario.h's own header comment -- not repeated at every site.
 */
#include "mvopeningmario.h"
#include "overlay.h"

#include "camanim.h"    /* the Mario cam-anim script */
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
 * is the offset (src/dc/scvsresults.c does the same for its own copy of
 * this exact file). */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* relocData file 37 (IFCommonAnnounceCommon), the five letters "MARIO"
 * spells with. Offsets are src/dc/scvsresults.c's own copies of the
 * decomp's ll<Name>Sprite link labels (src/dc/decomp/reloc_data.us.h
 * agrees). */
#define llIFCommonAnnounceCommonLetterMSprite 0x03980
#define llIFCommonAnnounceCommonLetterASprite 0x005E0
#define llIFCommonAnnounceCommonLetterRSprite 0x05418
#define llIFCommonAnnounceCommonLetterISprite 0x026B8
#define llIFCommonAnnounceCommonLetterOSprite 0x044B0

/* ---- the pools --------------------------------------------------------
 *
 * mvopeningmario.c:1379-1421-shaped dMVOpeningMarioTaskmanSetup carries
 * zero for every pool count, the same placeholder scvsbattle.c:24-64
 * documents. This scene runs one real VS stage (Castle) and up to two
 * real Mario fighters (posed + walking) -- smaller than
 * scAutoDemoFuncStart's own four-fighter budget, which its own header
 * already reuses from scvsbattle.c rather than compute fresh; this file
 * does the same rather than guess narrower figures for one fewer
 * fighter. SObjs and CObjs are this scene's own: five name letters is
 * the whole SObj bill (the wallpaper and posed-wallpaper procs are raw
 * gDPFillRectangle, no SObj), and four cameras (name, posed wallpaper,
 * posed fighter, motion) never all change at once, so a small multiple
 * covers it. */
#define MVOPENINGMARIO_GOBJS       48
#define MVOPENINGMARIO_GOBJPROCS   48
#define MVOPENINGMARIO_XOBJS      288
#define MVOPENINGMARIO_AOBJS      576
#define MVOPENINGMARIO_MOBJS       32
#define MVOPENINGMARIO_DOBJS       96
#define MVOPENINGMARIO_SOBJS        8
#define MVOPENINGMARIO_COBJS        8

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mvopeningmario.c:8018E090/E0AC, verbatim. */
CObjDesc dMVOpeningMarioStartCObjDesc = { { 300.0F, 500.0F, 1700.0F }, { 0.0F, 100.0F, 0.0F }, 0.15F };
CObjDesc dMVOpeningMarioEndCObjDesc = { { 800.0F, 500.0F, 1300.0F }, { 100.0F, 100.0F, 0.0F }, 0.15F };

/* mvopeningmario.c:8018E0C8 dMVOpeningMarioKeyEvents, verbatim: the
 * walking fighter's canned input, ftParamSetKey/ftKeyProcessKeyEvents
 * (already ported -- src/dc/scexplain.h's own header names the same
 * mechanism). */
FTKeyEvent dMVOpeningMarioKeyEvents[] =
{
    FTKEY_EVENT_STICK(0, 0, 0),
    FTKEY_EVENT_BUTTON(A_BUTTON, 1),
    FTKEY_EVENT_BUTTON(0, 11),
    FTKEY_EVENT_BUTTON(A_BUTTON, 1),
    FTKEY_EVENT_BUTTON(0, 20),
    FTKEY_EVENT_STICK(0, -I_CONTROLLER_RANGE_MAX, 0),
    FTKEY_EVENT_BUTTON(A_BUTTON, 1),
    FTKEY_EVENT_END()
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static s32 sMVOpeningMarioTotalTimeTics;
static GObj *sMVOpeningMarioNameGObj;
static GObj *sMVOpeningMarioFighterGObj;
static GObj *sMVOpeningMarioMotionCameraGObj;

/* mvopeningmario.h's own note: this stays NULL, the same
 * ftManagerAllocFigatreeHeapKind()-always-NULL state mvopeningroom.c's
 * three heaps already carry. */
static void *sMVOpeningMarioFigatreeHeap;

static f32 sMVOpeningMarioPosedFighterSpeed;
static CObjDesc sMVOpeningMarioAdjustedStartCObjDesc;
static CObjDesc sMVOpeningMarioAdjustedEndCObjDesc;
static SCBattleState sMVOpeningMarioBattleState;

/* The port's stand-in for sMVOpeningMarioFiles[0]: file 37's sprite
 * bank (romdisk/ifannounce.spr), its own copy the same shape
 * src/dc/scvsresults.c's own sMNVSResultsFiles[6] already takes.
 * sMVOpeningMarioNamesFileHead is not static, the same choice
 * mvopeningportraits.c's own sMVOpeningPortraitsFiles already makes, so
 * hosttest_ft.c's own openingmario_find can read it. */
static SpriteBank sMVOpeningMarioNamesBank;
void *sMVOpeningMarioNamesFileHead;

/* The port's stand-in for sMVOpeningMarioFiles[1]: file
 * 65 held nothing this scene read but one camera-anim script, and the
 * whole of what it wanted out of that file is
 * romdisk/mvopeningcommon.cam's "Mario" entry (src/dc/camanim.h). */
static CamAnimBank sMVOpeningMarioCamAnimBank;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningmario.c:8018D0C0 mvOpeningMarioSetupFiles. DIVERGES: the two
 * loads mvopeningmario.h's header names, in place of
 * lbRelocInitSetup/lbRelocLoadFilesListed. A bank that fails to load
 * leaves its file-head NULL / camanim_get answering NULL for every
 * name, the same fallback camanim_bank_load's own header documents --
 * not fatal, just a held shot or missing letters. */
void mvOpeningMarioSetupFiles(void)
{
    if (sprite_bank_load(&sMVOpeningMarioNamesBank, "ifannounce.spr") < 0)
    {
        sMVOpeningMarioNamesFileHead = NULL;
    }
    else
    {
        sMVOpeningMarioNamesFileHead = &sMVOpeningMarioNamesBank;
    }
    camanim_bank_load(&sMVOpeningMarioCamAnimBank, "mvopeningcommon.cam");
}

/* mvopeningmario.c:8018D160 mvOpeningMarioInitName, verbatim. */
void mvOpeningMarioInitName(SObj *sobj)
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

/* mvopeningmario.c:8018D194 mvOpeningMarioMakeName, verbatim but for
 * lbRelocGetFileData's own redefinition above. */
void mvOpeningMarioMakeName(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 i;

    intptr_t offsets[] =
    {
        llIFCommonAnnounceCommonLetterMSprite,
        llIFCommonAnnounceCommonLetterASprite,
        llIFCommonAnnounceCommonLetterRSprite,
        llIFCommonAnnounceCommonLetterISprite,
        llIFCommonAnnounceCommonLetterOSprite,
        0x0
    };

    f32 pos_x[] =
    {
        0.0F, 40.0F, 80.0F, 110.0F, 125.0F
    };

    sMVOpeningMarioNameGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; offsets[i] != 0x0; i++)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningMarioNamesFileHead, offsets[i]));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = pos_x[i] + 80.0F;
        sobj->pos.y = 100.0F;

        mvOpeningMarioInitName(sobj);
    }
}

/* mvopeningmario.c:8018D314 mvOpeningMarioMotionCameraProcUpdate,
 * verbatim. */
void mvOpeningMarioMotionCameraProcUpdate(GObj *camera_gobj)
{
    CObj *cobj = CObjGetStruct(camera_gobj);

    if (sMVOpeningMarioTotalTimeTics >= 15)
    {
        cobj->vec.eye.x += (((sMVOpeningMarioAdjustedEndCObjDesc.eye.x - sMVOpeningMarioAdjustedStartCObjDesc.eye.x) / 45.0F));
        cobj->vec.eye.y += (((sMVOpeningMarioAdjustedEndCObjDesc.eye.y - sMVOpeningMarioAdjustedStartCObjDesc.eye.y) / 45.0F));
        cobj->vec.eye.z += (((sMVOpeningMarioAdjustedEndCObjDesc.eye.z - sMVOpeningMarioAdjustedStartCObjDesc.eye.z) / 45.0F));
        cobj->vec.at.x += (((sMVOpeningMarioAdjustedEndCObjDesc.at.x - sMVOpeningMarioAdjustedStartCObjDesc.at.x) / 45.0F));
        cobj->vec.at.y += (((sMVOpeningMarioAdjustedEndCObjDesc.at.y - sMVOpeningMarioAdjustedStartCObjDesc.at.y) / 45.0F));
        cobj->vec.at.z += (((sMVOpeningMarioAdjustedEndCObjDesc.at.z - sMVOpeningMarioAdjustedStartCObjDesc.at.z) / 45.0F));
        cobj->vec.up.x += (((sMVOpeningMarioAdjustedEndCObjDesc.upx - sMVOpeningMarioAdjustedStartCObjDesc.upx) / 45.0F));
    }
}

/* mvopeningmario.c:8018D40C mvOpeningMarioMakeMotionCamera. DIVERGES:
 * gmCameraMakeMovieCamera(NULL) is the decomp's own call (its kind 8 is
 * the roll look-at, objdisplay.c gcCameraLookAtF), and camera_mask is
 * set right after it, the same line
 * scAutoDemoFuncStart makes for its own real camera. */
void mvOpeningMarioMakeMotionCamera(Vec3f move)
{
    CObj *cobj;

    sMVOpeningMarioAdjustedStartCObjDesc = dMVOpeningMarioStartCObjDesc;
    sMVOpeningMarioAdjustedEndCObjDesc = dMVOpeningMarioEndCObjDesc;

    sMVOpeningMarioMotionCameraGObj = gmCameraMakeMovieCamera(NULL);
    gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK;

    cobj = CObjGetStruct(sMVOpeningMarioMotionCameraGObj);

    syRdpSetViewport(&cobj->viewport, 110.0F, 10.0F, 310.0F, 230.0F);

    cobj->projection.persp.aspect = 10.0F / 11.0F;

    gcEndProcessAll(sMVOpeningMarioMotionCameraGObj);
    gcAddGObjProcess(sMVOpeningMarioMotionCameraGObj, mvOpeningMarioMotionCameraProcUpdate, nGCProcessKindFunc, 1);

    sMVOpeningMarioAdjustedStartCObjDesc.eye.x += move.x;
    sMVOpeningMarioAdjustedStartCObjDesc.eye.y += move.y;
    sMVOpeningMarioAdjustedStartCObjDesc.eye.z += move.z;
    sMVOpeningMarioAdjustedStartCObjDesc.at.x += move.x;
    sMVOpeningMarioAdjustedStartCObjDesc.at.y += move.y;
    sMVOpeningMarioAdjustedStartCObjDesc.at.z += move.z;

    sMVOpeningMarioAdjustedEndCObjDesc.eye.x += move.x;
    sMVOpeningMarioAdjustedEndCObjDesc.eye.y += move.y;
    sMVOpeningMarioAdjustedEndCObjDesc.eye.z += move.z;
    sMVOpeningMarioAdjustedEndCObjDesc.at.x += move.x;
    sMVOpeningMarioAdjustedEndCObjDesc.at.y += move.y;
    sMVOpeningMarioAdjustedEndCObjDesc.at.z += move.z;

    cobj->vec.eye.x = sMVOpeningMarioAdjustedStartCObjDesc.eye.x;
    cobj->vec.eye.y = sMVOpeningMarioAdjustedStartCObjDesc.eye.y;
    cobj->vec.eye.z = sMVOpeningMarioAdjustedStartCObjDesc.eye.z;
    cobj->vec.at.x = sMVOpeningMarioAdjustedStartCObjDesc.at.x;
    cobj->vec.at.y = sMVOpeningMarioAdjustedStartCObjDesc.at.y;
    cobj->vec.at.z = sMVOpeningMarioAdjustedStartCObjDesc.at.z;
    cobj->vec.up.x = sMVOpeningMarioAdjustedStartCObjDesc.upx;
}

/* mvopeningmario.c:8018D614 mvOpeningMarioMakeMotionWindow. DIVERGES:
 * mpCollisionGetMapObjCountKind/IDsKind/PositionID stay verbatim
 * (mp/mpcollision.c, compiled unmodified, scautodemo.c:531-533's own
 * note); grWallpaperMakeDecideKind()/grCommonSetupInitAll() move to
 * after mvOpeningMarioMakeMotionCamera() rather than before --
 * mvopeningmario.h's Substitution 3. */
void mvOpeningMarioMakeMotionWindow(void)
{
    GObj *fighter_gobj;
    s32 i;
    s32 pos_ids[2];
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

    mvOpeningMarioMakeMotionCamera(pos);

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

        sMVOpeningMarioFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

        ftParamInitPlayerBattleStats(i, fighter_gobj);
        ftParamSetKey(fighter_gobj, dMVOpeningMarioKeyEvents);
    }
}

/* mvopeningmario.c:8018D844 mvOpeningMarioPosedWallpaperProcDisplay.
 * DIVERGES: the colour panel under the posed fighter is a
 * gDPFillRectangle under G_CC_PRIMITIVE, and the raw GBI lands in the
 * write-only scratch heads (src/dc/wpmanager.c), so it is the fill quad
 * src/dc/ifscreenflash.c draws instead, translucent pass only. Its
 * camera (priority 20) is walked before the posed fighter's (10), so the
 * quad takes the earlier sprite depth and the layered Demo fighter draws
 * over it; the stage wallpaper and the motion window are opaque and sit
 * under both. */
void mvOpeningMarioPosedWallpaperProcDisplay(GObj *gobj)
{
    (void)gobj;

    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    lbCommonSpriteFillRect(10, 10, 110, 230, 0xA0, 0xAA, 0xFF, 0xFF);
}

/* mvopeningmario.c:8018D944 mvOpeningMarioMakePosedWallpaper, verbatim. */
void mvOpeningMarioMakePosedWallpaper(void)
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
        mvOpeningMarioPosedWallpaperProcDisplay,
        28,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
}

/* mvopeningmario.c:8018D990 mvOpeningMarioPosedFighterProcUpdate,
 * verbatim. */
void mvOpeningMarioPosedFighterProcUpdate(GObj *fighter_gobj)
{
    switch (sMVOpeningMarioTotalTimeTics)
    {
    default:
        break;

    case 15:
        sMVOpeningMarioPosedFighterSpeed = 17.0F;
        break;

    case 45:
        sMVOpeningMarioPosedFighterSpeed = 15.0F;
        break;

    case 60:
        sMVOpeningMarioPosedFighterSpeed = 0.0F;
        break;
    }
    if ((sMVOpeningMarioTotalTimeTics > 15) && (sMVOpeningMarioTotalTimeTics < 45))
    {
        sMVOpeningMarioPosedFighterSpeed += -1.0F / 15.0F;
    }
    if ((sMVOpeningMarioTotalTimeTics > 45) && (sMVOpeningMarioTotalTimeTics < 60))
    {
        sMVOpeningMarioPosedFighterSpeed += -1.0F;
    }
    DObjGetStruct(fighter_gobj)->translate.vec.f.y -= sMVOpeningMarioPosedFighterSpeed;
}

/* mvopeningmario.c:8018DA60 mvOpeningMarioMakePosedFighter, verbatim:
 * desc.figatree_heap reads sMVOpeningMarioFigatreeHeap, always NULL
 * (mvopeningmario.h's Substitution 4). */
void mvOpeningMarioMakePosedFighter(void)
{
    GObj *fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = nFTKindMario;
    desc.costume = ftParamGetCostumeCommonID(nFTKindMario, 0);
    desc.figatree_heap = sMVOpeningMarioFigatreeHeap;

    desc.pos.x = 0.0F;
    desc.pos.y = 600.0F;
    desc.pos.z = 0.0F;

    fighter_gobj = ftManagerMakeFighter(&desc);
    scSubsysFighterSetStatus(fighter_gobj, nFTDemoStatusStance);
    gcMoveGObjDL(fighter_gobj, 26, -1);
    gcAddGObjProcess(fighter_gobj, mvOpeningMarioPosedFighterProcUpdate, nGCProcessKindFunc, 1);

    DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;
}

/* mvopeningmario.c:8018DB5C mvOpeningMarioMakeNameCamera, verbatim. */
void mvOpeningMarioMakeNameCamera(void)
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

/* mvopeningmario.c:8018DBFC mvOpeningMarioMakePosedFighterCamera.
 * DIVERGES: the AObjEvent32 script's own reach, mvopeningmario.h's
 * Substitution 1 -- camanim_get(&sMVOpeningMarioCamAnimBank, "Mario")
 * in place of lbRelocGetFileData(AObjEvent32*, sMVOpeningMarioFiles[1],
 * &llMVOpeningCommonMarioCamAnimJoint). func_80017EC0 is the decomp's
 * own function, already ported (src/dc/objdisplay.c).
 *
 * The attach-and-play pair itself is `#ifdef FT_HOSTTEST`-guarded, the
 * same shape src/dc/grcastle.c and src/dc/gryamabuki.c already use for
 * this exact reason (their own headers say so): `union AObjEvent32`
 * carries a `void *p`, so it is eight bytes on x86-64 and four on the
 * target, and the decomp's `AObjAnimAdvance(script)` (`script++`) steps
 * two words at a time on the host and never reaches an End -- confirmed
 * here (not assumed) the hard way, via a host gdb backtrace landing
 * inside gcParseCObjCamAnimJoint's own do-while spinning at tic 1 the
 * first time this scene's own host test called gcRunAll with the
 * process registered. src/game/ssb64/hosttest_ft.c's own
 * test_openingroom's header names the same limitation and works around
 * it by never calling gcRunAll while a camanim process is registered;
 * this scene's test does call gcRunAll (it has to, to reach the tic-15
 * hand-off), so it needs the guard room's test could do without. */
void mvOpeningMarioMakePosedFighterCamera(void)
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
        -1,
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
    /* the host cannot walk a script; see this function's own note */
#else
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningMarioCamAnimBank, "Mario"), 0.0F);
    gcAddGObjProcess(camera_gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
#endif
}

/* mvopeningmario.c:8018DCEC mvOpeningMarioMakePosedWallpaperCamera,
 * verbatim. */
void mvOpeningMarioMakePosedWallpaperCamera(void)
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
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 110.0F, 230.0F);

    cobj->flags = COBJ_FLAG_DLBUFFERS | COBJ_FLAG_ZBUFFER;
}

/* mvopeningmario.c:8018DD9C mvOpeningMarioFuncRun, verbatim except the
 * eject: gcEjectGObjIfMade in place of a raw gcEjectGObj, the
 * port-wide standing rule (mvopeningportraits.h) every eject follows
 * regardless of whether NULL is reachable -- it is not, here, since
 * mvOpeningMarioMakeName always makes the name GObj in FuncStart before
 * FuncRun's first tic can read it. */
void mvOpeningMarioFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningMarioTotalTimeTics++;

    if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindTitle;

        syTaskmanSetLoadScene();
    }
    if (sMVOpeningMarioTotalTimeTics == 15)
    {
        gcEjectGObjIfMade(sMVOpeningMarioNameGObj);
        mvOpeningMarioMakeMotionWindow();
        mvOpeningMarioMakePosedWallpaper();
        mvOpeningMarioMakePosedFighter();
    }
    if (sMVOpeningMarioTotalTimeTics == 60)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindOpeningDonkey;

        syTaskmanSetLoadScene();
    }
}

/* mvopeningmario.c:8018DE58 mvOpeningMarioInitVars, verbatim. */
void mvOpeningMarioInitVars(void)
{
    sMVOpeningMarioTotalTimeTics = 0;
}

/* mvopeningmario.c:8018DE64 mvOpeningMarioFuncStart. DIVERGES:
 * mvopeningmario.h's Substitutions 1-4, all at their decomp call
 * sites except the stage acquire (grStageAcquire/stage_bind), which is
 * new -- gkind is fixed for this whole scene (nGRKindCastle), so it is
 * acquired here rather than threaded through as a return value the way
 * scAutoDemoFuncStart's own runtime-picked gkind has to be. */
void mvOpeningMarioFuncStart(void)
{
    Stage *stage;

    sMVOpeningMarioBattleState = dSCManagerDefaultBattleState;
    gSCManagerBattleState = &sMVOpeningMarioBattleState;

    gSCManagerBattleState->game_type = nSCBattleGameTypeMovie;

    gSCManagerBattleState->gkind = nGRKindCastle;
    gSCManagerBattleState->pl_count = 1;

    gSCManagerBattleState->players[0].fkind = nFTKindMario;
    gSCManagerBattleState->players[0].pkind = nFTPlayerKindKey;

    mvOpeningMarioSetupFiles();

    gcMakeGObjSPAfter(nGCCommonKindMovie, mvOpeningMarioFuncRun, 13, GOBJ_PRIORITY_DEFAULT);

    mvOpeningMarioInitVars();
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

    /* mvOpeningMarioFuncStart's own efManagerInitEffects() (verbatim,
     * the same name mvopeningroom.c calls with no real stage present)
     * is NOT the substitution here: this scene loads a real fighter
     * pack (Mario's) with real particle-effect data, and
     * efManagerInitEffects's own efDisplayInitAll -> lbParticleSetupBankID
     * chain walks an LBScript-pointer/LBTexture-pointer array that is only safe on a
     * 32-bit target -- efManagerLoadEffectBank's own FT_HOSTTEST guard
     * (src/dc/efmanager.c) says exactly why and skips it on this build.
     * Every scene that already carries a real fighter pack alongside a
     * real stage (scVSBattleStartBattle, scAutoDemoFuncStart) already
     * takes this substitution -- efManagerInitEffects itself is on
     * scVSBattleStartBattle's own DIVERGES cut list, replaced by
     * efParticleInitAll (already called above) plus
     * efManagerLoadEffectBank/efManagerPreloadModels, the same pair
     * used here. Room's own use of the raw call merely never happened
     * to load a pack whose particle range trips the bug. */
    efManagerLoadEffectBank();
    efManagerPreloadModels();

    ftManagerSetupFilesAllKind(nFTKindMario);

    mvOpeningMarioMakeNameCamera();
    mvOpeningMarioMakePosedWallpaperCamera();
    mvOpeningMarioMakePosedFighterCamera();
    mvOpeningMarioMakeName();

    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 1515 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(1515);
}

/* mvopeningmario.c:8018DFE4 mvOpeningMarioFuncLights. Not ported --
 * the standing rule (a Lights1/FuncLights pair never survives
 * test-host, mvopeningroom.h/mvopeningportraits.h): the whole body is
 * ftDisplayLightsDrawReflect, the dropped reflection lighting every
 * other scene's func_lights drops. func_lights is NULL below. */

/* mvopeningmario.c:8018E138-shaped dMVOpeningMarioTaskmanSetup. See the
 * pools note above for the counts, and mvopeningportraits.h for
 * func_draw (scManagerFuncDraw), the zeroed RSP/RDP fields and the
 * pre-render/controller/matrix-list substitutions -- the same blanket
 * rules apply here. */
SYTaskmanSetup dMVOpeningMarioTaskmanSetup =
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
    MVOPENINGMARIO_GOBJPROCS,
    MVOPENINGMARIO_GOBJS,
    sizeof(GObj),
    MVOPENINGMARIO_XOBJS,
    NULL,                               /* matrix function list -- the port's
                                          * matrix kinds are a switch in
                                          * objdisplay.c, not a function list
                                          * (mvopeningportraits.c's own note) */
    NULL,
    MVOPENINGMARIO_AOBJS,
    MVOPENINGMARIO_MOBJS,
    MVOPENINGMARIO_DOBJS,
    sizeof(DObj),
    MVOPENINGMARIO_SOBJS,
    sizeof(SObj),
    MVOPENINGMARIO_COBJS,
    sizeof(CObj),

    mvOpeningMarioFuncStart
};

/* mvopeningmario.c:8018E030 mvOpeningMarioStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeningmario.h), and the stage release
 * below.
 *
 * The port's own, and the pair to mvOpeningMarioFuncStart's
 * grStageAcquire. The game's ground is a file in the scene's status
 * buffer and goes when the scene heap does; this port's is a .pack whose
 * blob is malloc'd and whose textures are pvr_mem_malloc'd, so the heap
 * reset at the scene change does not reach it and the stage stays
 * resident for the rest of the run. src/dc/scvsbattle.c's own
 * grStageRelease at the end of scVSBattleStartScene is the precedent and
 * gives the same reason. The nine opening movies that acquire a stage
 * had no such line, and that is what drained the attract chain's texture
 * RAM -- nine stages held at once, with nothing to give them back.
 *
 * gkind is the constant FuncStart named two lines above its acquire, so
 * there is nothing to remember across the task; and grStageRelease is a
 * guarded no-op for a kind whose pack the acquire did not find, which is
 * the same arm that let the scene run without a stage at all. */
void mvOpeningMarioStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningMarioTaskmanSetup);

    grStageRelease(nGRKindCastle);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[36]. */
void mvOpeningMarioOverlayLoad(void)
{
    OVERLAY_CLEAR(sMVOpeningMarioTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningMarioNameGObj);
    OVERLAY_CLEAR(sMVOpeningMarioFighterGObj);
    OVERLAY_CLEAR(sMVOpeningMarioMotionCameraGObj);
    OVERLAY_CLEAR(sMVOpeningMarioFigatreeHeap);
    OVERLAY_CLEAR(sMVOpeningMarioPosedFighterSpeed);
    OVERLAY_CLEAR(sMVOpeningMarioAdjustedStartCObjDesc);
    OVERLAY_CLEAR(sMVOpeningMarioAdjustedEndCObjDesc);
    OVERLAY_CLEAR(sMVOpeningMarioBattleState);
    OVERLAY_CLEAR(sMVOpeningMarioNamesBank);
    OVERLAY_CLEAR(sMVOpeningMarioNamesFileHead);
    OVERLAY_CLEAR(sMVOpeningMarioCamAnimBank);
}
