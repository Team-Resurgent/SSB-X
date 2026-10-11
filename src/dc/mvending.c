/* mvending.c -- see mvending.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvending/mvending.c; every function names its
 * line range. mvEndingInitVars is verbatim now (the run's own fighter);
 * the remaining DIVERGES are the standard ones, explained once, in
 * mvending.h's own header comment -- not repeated at every site. The
 * cameras and room props are verbatim now but for their substitutions.
 */
#include "mvending.h"
#include "overlay.h"

#include "camanim.h"   /* the operator camera's AObjEvent32 script */
#include "ftcommon.h"
#include "gmcamera.h"
#include "gmcommon.h"
#include "ifcommon.h"
#include "input.h"
#include "scmanager.h"
#include "sprite.h"
#include "lbpdraw.h"
#include "objpvr.h"   /* gcEjectGObjIfMade -- see mvEndingEjectRoomGObjs; gcGetDrawList */
#include "lbcommon.h" /* lbCommonSpriteFillRect -- the two fades */
#include "efmanager.h"
#include "taskman.h"
#include "fighter.h"    /* the room props' packs */
#include "objmodel.h"   /* dc_model_add_dobjs, dc_model_proc_display */

#include <ef/efdisplay.h>
#include <ef/efparticle.h>
#include <ef/effect.h>
#include <ft/fighter.h>
#include <ft/ftdef.h>
#include <ft/ftparam.h>
#include <ft/ftpublic.h>
#include <gm/gmrumble.h>
#include <gm/gmsound.h>
#include <if/ifcommon.h>
#include <sc/scsubsys/scsubsys.h> /* scSubsysFighterSetStatus/SetLightParams */
#include <sys/controller.h>
#include <sys/objanim.h>          /* gcAddCObjCamAnimJoint, gcPlayCamAnim */
#include <sys/rdp.h>
#include <lb/lbdef.h>
#include <lb/lbfade.h>
#include <sys/debug.h>  /* syDebugPrintf, as src/dc/mvopeningroom.c */

/* ---- the pools ---------------------------------------------------------
 *
 * mvending.c:99-141 dMVEndingTaskmanSetup carries zero for every pool
 * count, the same placeholder scvsbattle.c:24-64/scautodemo.c:56-73/
 * scexplain.c:62-72 document. This scene poses one fighter in the
 * opening Room's own room -- the same background (52 DObjs over its two
 * packs, seven MObjs), desk, books, pencils, lamp and tissues -- so it
 * takes src/dc/mvopeningroom.c's figures, which cover that room plus
 * Master Hand, two trophies and ten more props. */
#define MVENDING_GOBJS       48
#define MVENDING_GOBJPROCS   48
#define MVENDING_XOBJS      288
#define MVENDING_AOBJS      576
#define MVENDING_MOBJS       32
#define MVENDING_DOBJS       96
#define MVENDING_SOBJS       16
#define MVENDING_COBJS        6

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static void *sMVEndingFigatreeHeap;
static s32 sMVEndingTotalTimeTics;
static GObj *sMVEndingRoomCameraGObj;
static GObj *sMVEndingFighterCameraGObj;
static GObj *sMVEndingFighterGObj;
static GObj *sMVEndingRoomBackgroundGObj;
static GObj *sMVEndingRoomBackground1GObj; /* the second pack, see mvEndingMakeRoomBackground */
static GObj *sMVEndingRoomDeskGObj;
static GObj *sMVEndingRoomBooksGObj;
static GObj *sMVEndingRoomPencilsGObj;
static GObj *sMVEndingRoomLampGObj;
static GObj *sMVEndingRoomTissuesGObj;
static s32 sMVEndingRoomFadeInAlpha;
static f32 sMVEndingRoomLightAlpha;
static GObj *sMVEndingRoomFadeInGObj;
static GObj *sMVEndingRoomLightGObj;
static FTDemoDesc sMVEndingFighterDemoDesc;
static s32 sMVEndingUnused0x80132C14;

/* The port's stand-in for sMVEndingFiles[1]: file 76's own
 * camera-anim script, romdisk/mvending.cam's one entry
 * (src/dc/camanim.h). */
static CamAnimBank sMVEndingCamAnimBank;

/* The port's stand-in for sMVEndingFiles[0], the mvcommon room file:
 * the opening Room's own packs (src/dc/mvopeningroom.c), because the
 * ending builds the same six props out of the same
 * llMVCommonRoom* symbols. tools/export/ssb_scenemodelexport.py baked them;
 * nothing new is exported for this scene. The background is two packs
 * for the reason mvOpeningRoomMakeBackground gives (51 joints against
 * FIGHTER_MAX_JOINTS's 40). */
enum
{
    nMVEndingPropBackground0,
    nMVEndingPropBackground1,
    nMVEndingPropDesk,
    nMVEndingPropBooks,
    nMVEndingPropPencils,
    nMVEndingPropLamp,
    nMVEndingPropTissues,
    nMVEndingPropCount
};

static const char *const dMVEndingPropModels[nMVEndingPropCount] =
{
    "mvopeningroombackground0.mdl",
    "mvopeningroombackground1.mdl",
    "mvopeningroomdesk.mdl",
    "mvopeningroombooks.mdl",
    "mvopeningroompencils.mdl",
    "mvopeningroomlamp.mdl",
    "mvopeningroomtissues.mdl",
};

static Fighter sMVEndingPropPacks[nMVEndingPropCount];
static sb32 sMVEndingPropIsLoaded[nMVEndingPropCount];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvending.c:150-155 mvEndingFuncLights 0x80131B00. Not ported --
 * ftDisplayLightsDrawReflect is the same dropped reflection lighting
 * every other scene's func_lights drops (src/dc/scautodemo.c's own
 * scAutoDemoFuncLights comment). func_lights is NULL in
 * dMVEndingTaskmanSetup below, so this is never called; kept unported
 * rather than stubbed, the same as that file's own precedent. */

/* gcSetupCommonDObjs, gcAddAnimJointAll and gcAddMObjAll +
 * gcAddMatAnimJointAll over one of the room file's prop trees, out of a
 * pack instead: mvOpeningRoomSetupPropTree (src/dc/mvopeningroom.c)
 * line for line, and its comment says the rest. The one difference is
 * the start frame: the ending binds the books', pencils', lamp's and
 * tissues' scripts at 300 where the opening binds them at 0. With no
 * update proc, the props hold whatever pose the first gcPlayAnimAll
 * gives them. The host build builds the DObjs and adds no animation,
 * for the reason that function gives. */
static void mvEndingSetupPropTree(GObj *gobj, s32 prop, f32 anim_frame)
{
    DObj *joints[FIGHTER_MAX_JOINTS];
    Fighter *model = &sMVEndingPropPacks[prop];
    s32 njoints;

    if (sMVEndingPropIsLoaded[prop] == FALSE)
    {
        int pal_bank = 0;

        if (fighter_load_scene(model, dMVEndingPropModels[prop],
                               &pal_bank) != 0)
        {
            syDebugPrintf("mvEnding: no %s; that prop is not in the "
                          "room\n", dMVEndingPropModels[prop]);
            return;
        }
        sMVEndingPropIsLoaded[prop] = TRUE;
    }
    njoints = dc_model_add_dobjs(gobj, NULL, model, joints);

#ifndef FT_HOSTTEST
    if ((njoints > 0) && (model->hd->anim_count != 0))
    {
        AObjEvent32 *anim_joints[FIGHTER_MAX_JOINTS];
        const FPackAnim *anim = &model->anims[0];
        const s32 *entries =
            (const s32 *)((const u8 *)model->blob + anim->off_entries);
        s32 i;

        for (i = 0; i < njoints; i++)
        {
            anim_joints[i] = ((entries[i] >= 0) &&
                              ((u32)entries[i] < anim->nwords))
                           ? (AObjEvent32 *)((u8 *)model->blob +
                                             anim->off_words) + entries[i]
                           : NULL;
        }
        gcAddAnimJointAll(gobj, anim_joints, anim_frame);
    }
    if (njoints > 0)
    {
        dc_model_add_mobjs(gobj, model, 0.0F);
    }
#else
    (void)njoints;
    (void)anim_frame;
#endif
}

/* mvending.c:158-169 mvEndingMakeRoomBackground 0x80131B58, verbatim but
 * for the tree, its materials and its display proc (the substitutions
 * mvEndingSetupPropTree makes), and the split into two GObjs that
 * mvOpeningRoomMakeBackground argues. The second half holds every
 * MObjSub. sMVEndingRoomBackgroundGObj holds the first half; the second
 * is ejected with it at tic 540 through its own static. */
void mvEndingMakeRoomBackground(void)
{
    GObj *gobj;

    sMVEndingRoomBackgroundGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvEndingSetupPropTree(gobj, nMVEndingPropBackground0, 0.0F);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 29, GOBJ_PRIORITY_DEFAULT, ~0);
    gcPlayAnimAll(gobj);

    sMVEndingRoomBackground1GObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvEndingSetupPropTree(gobj, nMVEndingPropBackground1, 0.0F);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 29, GOBJ_PRIORITY_DEFAULT, ~0);
    gcPlayAnimAll(gobj);
}

/* mvending.c:172-180 mvEndingMakeRoomDesk 0x80131C1C, verbatim but for
 * the tree and its display proc: dc_model_proc_display walks the tree
 * gcDrawDObjTreeForGObj walked (mvOpeningRoomMakeDesk). */
void mvEndingMakeRoomDesk(void)
{
    GObj *gobj;

    sMVEndingRoomDeskGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvEndingSetupPropTree(gobj, nMVEndingPropDesk, 0.0F);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 29, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mvending.c:183-193 mvEndingMakeRoomBooks 0x80131C94, 196-206
 * mvEndingMakeRoomPencils 0x80131D34 and 209-219 mvEndingMakeRoomLamp
 * 0x80131DD4, verbatim but for the desk's two substitutions. The tree
 * and its animation arrive together out of the pack, so
 * gcAddAnimJointAll runs before the display proc is added rather than
 * after it; nothing reads either in between. */
void mvEndingMakeRoomBooks(void)
{
    GObj *gobj;

    sMVEndingRoomBooksGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvEndingSetupPropTree(gobj, nMVEndingPropBooks, 300.0F);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 29, GOBJ_PRIORITY_DEFAULT, ~0);
    gcPlayAnimAll(gobj);
}

void mvEndingMakeRoomPencils(void)
{
    GObj *gobj;

    sMVEndingRoomPencilsGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvEndingSetupPropTree(gobj, nMVEndingPropPencils, 300.0F);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 29, GOBJ_PRIORITY_DEFAULT, ~0);
    gcPlayAnimAll(gobj);
}

void mvEndingMakeRoomLamp(void)
{
    GObj *gobj;

    sMVEndingRoomLampGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvEndingSetupPropTree(gobj, nMVEndingPropLamp, 300.0F);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 29, GOBJ_PRIORITY_DEFAULT, ~0);
    gcPlayAnimAll(gobj);
}

/* mvending.c:222-235 mvEndingMakeRoomTissues 0x80131E74, verbatim but
 * for the substitutions above. The decomp binds one display list to one
 * DObj and its script with gcAddDObjAnimJoint(dobj, script, 300.0F); the
 * pack is that one joint, and gcAddAnimJointAll over a one-entry table
 * is the same call on it (mvOpeningRoomMakeTissues says why). The
 * Tra/RotRpyR/Sca XObj the decomp adds is the triple dc_model_add_dobjs
 * already gives every joint, so it is not added twice. */
void mvEndingMakeRoomTissues(void)
{
    GObj *gobj;

    sMVEndingRoomTissuesGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvEndingSetupPropTree(gobj, nMVEndingPropTissues, 300.0F);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 29, GOBJ_PRIORITY_DEFAULT, ~0);
    gcPlayAnimAll(gobj);
}

/* mvending.c:238-258 mvEndingMakeFighter, verbatim: the 1P clear
 * character, dropped onto the room's desk, lit and posed via nFTDemoStatusFigureDropped
 * (ft/ftdef.h -- "fighter dropped in opening movie / ending movie /
 * game over"), the same status the port's other demo poses use. */
void mvEndingMakeFighter(s32 fkind)
{
    GObj *fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = fkind;

    desc.pos.x = -1077.804F;
    desc.pos.y = 4038.864F;

    desc.costume = sMVEndingFighterDemoDesc.costume;
    desc.shade = sMVEndingFighterDemoDesc.shade;

    desc.figatree_heap = sMVEndingFigatreeHeap;

    desc.pos.z = -3688.5298F;

    sMVEndingFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

    scSubsysFighterSetStatus(fighter_gobj, nFTDemoStatusFigureDropped);
}

/* mvending.c:261-287 mvEndingRoomFadeInProcDisplay: the black that
 * opens the scene, lifting from tic 70 and back at 540 for the teardown.
 * DIVERGES: it is a gDPFillRectangle under G_CC_PRIMITIVE, and the raw
 * GBI lands in the write-only scratch heads (src/dc/wpmanager.c), so it
 * is the fill quad src/dc/ifscreenflash.c draws instead, in the
 * translucent pass only. The alpha steps there too, so it steps once a
 * frame, as the N64's one-pass draw did. The same substitution
 * mvOpeningRoomLogoWallpaperProcDisplay makes. */
void mvEndingRoomFadeInProcDisplay(GObj *gobj)
{
    (void)gobj;

    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    if (sMVEndingTotalTimeTics >= 540)
    {
        sMVEndingRoomFadeInAlpha = 0xFF;
    }
    if ((sMVEndingTotalTimeTics >= 70) && (sMVEndingTotalTimeTics < 540))
    {
        if (sMVEndingRoomFadeInAlpha > 0x00)
        {
            sMVEndingRoomFadeInAlpha -= 0x07;

            if (sMVEndingRoomFadeInAlpha < 0x00)
            {
                sMVEndingRoomFadeInAlpha = 0x00;
            }
        }
    }
    lbCommonSpriteFillRect(10, 10, 310, 230, 0x00, 0x00, 0x00, sMVEndingRoomFadeInAlpha);
}

/* mvending.c:290-299 mvEndingMakeRoomFadeIn, verbatim. */
void mvEndingMakeRoomFadeIn(void)
{
    GObj *gobj;

    sMVEndingRoomFadeInAlpha = 0xFF;

    sMVEndingRoomFadeInGObj = gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, mvEndingRoomFadeInProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mvending.c:302-324 mvEndingMakeRoomFadeInCamera, verbatim: a fixed
 * viewport/sprite camera, no relocData dependency. */
void mvEndingMakeRoomFadeInCamera(void)
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
            60,
            COBJ_MASK_DLLINK(26),
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

/* mvending.c:327-346 mvEndingRoomLightProcDisplay: the white that
 * rises from tic 340 to alpha 220. DIVERGES the way
 * mvEndingRoomFadeInProcDisplay above does, for the same reason. */
void mvEndingRoomLightProcDisplay(GObj *gobj)
{
    (void)gobj;

    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    if ((sMVEndingTotalTimeTics >= 340) && (sMVEndingRoomLightAlpha < 220.0F))
    {
        sMVEndingRoomLightAlpha += 1.1F;

        if (sMVEndingRoomLightAlpha > 220.0F)
        {
            sMVEndingRoomLightAlpha = 220.0F;
        }
    }
    lbCommonSpriteFillRect(10, 10, 310, 230, 0xFF, 0xFF, 0xFF, (u8)sMVEndingRoomLightAlpha);
}

/* mvending.c:349-358 mvEndingMakeRoomLight, verbatim. */
void mvEndingMakeRoomLight(void)
{
    GObj *gobj;

    sMVEndingRoomLightAlpha = 0.0F;

    sMVEndingRoomLightGObj = gobj = gcMakeGObjSPAfter(0, NULL, 21, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, mvEndingRoomLightProcDisplay, 30, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mvending.c:361-383 mvEndingMakeRoomLightCamera, verbatim. */
void mvEndingMakeRoomLightCamera(void)
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
            30,
            COBJ_MASK_DLLINK(30),
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

/* mvending.c:386-393 mvEndingEjectRoomGObjs, verbatim, plus the
 * background's second half (mvEndingMakeRoomBackground's split). The
 * decomp leaves the desk standing, and so does this. A prop whose pack
 * failed to load is still a made GObj with no DObjs, so every static
 * here is non-NULL once FuncStart has run.
 *
 * DIVERGES: this comment used to say ejecting a NULL GObj
 * was a safe no-op. It is not. objman.c:1780-1786 answers NULL by
 * setting sGCRunStatus = nGCRunStatusEject, and objman.c:2132-2146
 * gcRunGObj acts on it the instant the running GObj's func_run returns,
 * by ejecting that GObj. mvEndingFuncRun calls this at tic 540, from
 * inside its own driver GObj, so on the port it ejected that driver and
 * the tic-660 hand-off to nSCKindStaffroll could never fire -- a real,
 * silent bug, found by src/dc/mvopeningroom.c hitting the identical trap
 * at its own tic 280 (src/dc/objpvr.h's gcEjectGObjIfMade carries the
 * full note). */
void mvEndingEjectRoomGObjs(void)
{
    gcEjectGObjIfMade(sMVEndingRoomBackgroundGObj);
    gcEjectGObjIfMade(sMVEndingRoomBackground1GObj);
    gcEjectGObjIfMade(sMVEndingRoomBooksGObj);
    gcEjectGObjIfMade(sMVEndingRoomPencilsGObj);
    gcEjectGObjIfMade(sMVEndingRoomLampGObj);
    gcEjectGObjIfMade(sMVEndingRoomTissuesGObj);
}

/* mvending.c:396-407 mvEndingSetupOperatorCamera, verbatim:
 * `lbRelocGetFileData(AObjEvent32*, sMVEndingFiles[1],
 * &llMVEndingOperatorCamAnimJoint)` is camanim_get(bank, "Operator"),
 * the same 316-word script out of the same relocData file 76, reached by
 * name instead of by linker offset (src/dc/camanim.h 4b). Both
 * main cameras take it, which is the decomp's own arrangement. */
void mvEndingSetupOperatorCamera(GObj *gobj)
{
    CObj *cobj = CObjGetStruct(gobj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->projection.persp.near = 128.0F;
    cobj->projection.persp.far = 16384.0F;

    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVEndingCamAnimBank, "Operator"), 0.0F);
    gcAddGObjProcess(gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
}

/* mvending.c:410-455 mvEndingMakeMainCameras, verbatim. */
void mvEndingMakeMainCameras(void)
{
    GObj *gobj;

    sMVEndingRoomCameraGObj = gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        80,
        COBJ_MASK_DLLINK(29),
        -1,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    gcAddXObjForCamera(CObjGetStruct(gobj), nGCMatrixKindPerspFastF, 0);
    gcAddXObjForCamera(CObjGetStruct(gobj), 8, 0);
    mvEndingSetupOperatorCamera(gobj);

    CObjGetStruct(gobj)->flags |= 4;

    sMVEndingFighterCameraGObj = gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        40,
        COBJ_MASK_DLLINK(9),
        -1,
        TRUE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    mvEndingSetupOperatorCamera(gobj);

    CObjGetStruct(gobj)->flags |= 4;
}

/* mvending.c:458-465 mvEndingInitVars, verbatim: the run's own fighter,
 * in the costume and shade it played in. */
void mvEndingInitVars(void)
{
    sMVEndingTotalTimeTics = 0;

    sMVEndingFighterDemoDesc.fkind = gSCManager1PGameBattleState.players[gSCManagerSceneData.player].fkind;
    sMVEndingFighterDemoDesc.costume = gSCManager1PGameBattleState.players[gSCManagerSceneData.player].costume;
    sMVEndingFighterDemoDesc.shade   = gSCManager1PGameBattleState.players[gSCManagerSceneData.player].shade;
}

/* mvending.c:468-505 mvEndingFuncRun, verbatim: the whole scene is this
 * tic counter -- light at 340, fighter/room teardown and the door-close
 * cue at 540, the hand-off to nSCKindStaffroll at 660. */
void mvEndingFuncRun(GObj *gobj)
{
    sMVEndingTotalTimeTics++;

    if (sMVEndingTotalTimeTics >= 10)
    {
        if (sMVEndingUnused0x80132C14 != 0)
        {
            sMVEndingUnused0x80132C14--;
        }
        if
        (
            (scSubsysControllerGetPlayerStickInRangeLR(-15, 15) != FALSE) &&
            (scSubsysControllerGetPlayerStickInRangeUD(-15, 15) != FALSE)
        )
        {
            sMVEndingUnused0x80132C14 = 0;
        }
        if (sMVEndingTotalTimeTics == 340)
        {
            mvEndingMakeRoomLight();
        }
        if (sMVEndingTotalTimeTics == 540)
        {
            mvEndingEjectRoomGObjs();
            ftManagerDestroyFighter(sMVEndingFighterGObj);
            gcEjectGObjIfMade(sMVEndingRoomLightGObj);
            func_800269C0_275C0(nSYAudioFGMDoorClose);
        }
        if (sMVEndingTotalTimeTics == 660)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindStaffroll;

            syTaskmanSetLoadScene();
        }
    }
}

/* mvending.c:508-548 mvEndingFuncStart. DIVERGES: the reloc loader
 * (lbRelocInitSetup/lbRelocLoadFilesListed) is gone -- the same
 * scAutoDemoSetupFiles-shaped cut. What stands in for its two files is
 * the camera bank below and the room props' packs, which each prop's
 * maker loads for itself (mvEndingSetupPropTree). The figatree heap allocation
 * (syTaskmanMalloc(gFTManagerFigatreeHeapSize, 0x10)) is gone too, the
 * same cut mnCharactersFuncStart/mnPlayersVSFuncStart already document:
 * ftManagerAllocFigatreeHeapKind always returns NULL port-side (the
 * pack's animations play in place, src/dc/ftmanager.c's own note), so
 * nothing ever reads gFTManagerFigatreeHeapSize -- which the port's own
 * ftmanager.c does not even define, only decomp's does -- and
 * sMVEndingFigatreeHeap stays NULL, exactly the value
 * mvEndingMakeFighter already hands FTDesc.figatree_heap unconditionally
 * (mncharacters.c's own header note on the same gap). */
void mvEndingFuncStart(void)
{
    /* The one thing the cut lbRelocLoadFilesListed did that this scene
     * can have back: file 76's camera-anim script, which
     * mvEndingMakeMainCameras below hands both main cameras. A failure
     * here is not fatal -- camanim_get then answers NULL and
     * gcAddCObjCamAnimJoint(NULL) leaves the cameras static, which is
     * what this scene shipped with at  3. */
    camanim_bank_load(&sMVEndingCamAnimBank, "mvending.cam");
    gcMakeGObjSPAfter(0, mvEndingFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
    /* DIVERGES: COBJ_FLAG_FILLCOLOR dropped, GObj and colour kept -- the
     * port's fill is a sprite-depth quad that would cover every 3D camera
     * after it (objdisplay.c gcPrepCameraViewport), which is exactly what
     * this scene showed: a white screen from start to teardown. The
     * opening Room's clear camera takes the same cut. Its colour is
     * still what shows after the tic-540 teardown, so it goes to the
     * PVR's background plane instead, as Newcomers' white does, and
     * StartScene puts it back to black on the way out. */
    gcMakeDefaultCameraGObj(0, GOBJ_PRIORITY_DEFAULT, 100, COBJ_FLAG_ZBUFFER, GPACK_RGBA8888(0xFF, 0xFF, 0xFF, 0xFF));
#ifndef FT_HOSTTEST
    pvr_set_bg_color(1.0F, 1.0F, 1.0F);
#endif
    efParticleInitAll();
    mvEndingInitVars();
    efManagerInitEffects();
    ftManagerAllocFighter(FTDATA_FLAG_SUBMOTION, 1);
    ftManagerSetupFilesAllKind(sMVEndingFighterDemoDesc.fkind);

    mvEndingMakeMainCameras();
    mvEndingMakeRoomFadeInCamera();
    mvEndingMakeRoomLightCamera();
    mvEndingMakeRoomBackground();
    mvEndingMakeRoomDesk();
    mvEndingMakeRoomBooks();
    mvEndingMakeRoomLamp();
    mvEndingMakeRoomPencils();
    mvEndingMakeRoomTissues();
    mvEndingMakeFighter(sMVEndingFighterDemoDesc.fkind);
    mvEndingMakeRoomFadeIn();

    scSubsysFighterSetLightParams(45.0F, 45.0F, 0xFF, 0xFF, 0xFF, 0xFF);
    /* DIVERGES: the fighter's camera shares the room's projection and
     * does not clear Z, so the Demo fighter takes the Z buffer rather
     * than the layered band -- the opening Room's call, for the same
     * reason (scmanager.c resets it at the next scene change). */
    dc_model_set_demo_opaque(TRUE);
    syAudioPlayBGM(0, nSYAudioBGMEnding);
}

/* mvending.c:551-560 mvEndingStartScene. DIVERGES: syVideoInit and the
 * zbuffer allocation are cut the same way every other scene's own
 * StartScene documents -- one video mode, set once at boot
 * (src/dc/sysshim.c); arena_size (the link-map address difference) has
 * no port meaning either, so scManagerFuncUpdate runs the task directly
 * instead of syTaskmanStartTask, the same substitution scexplain.c/
 * scautodemo.c already use. This scene sets scene_curr itself (tic 660
 * above), so this does not set it again either. */
void mvEndingStartScene(void)
{
    dMVEndingTaskmanSetup.func_start = mvEndingFuncStart;

    scManagerFuncUpdate(&dMVEndingTaskmanSetup);

#ifndef FT_HOSTTEST
    /* the clear colour FuncStart put on the background plane */
    pvr_set_bg_color(0.0F, 0.0F, 0.0F);
#endif
    gmRumbleInitPlayers();
}

/* mvending.c:99-141 dMVEndingTaskmanSetup -- see mvending.h's header
 * note and this file's own "the pools" comment above. */
SYTaskmanSetup dMVEndingTaskmanSetup =
{
    {
        0,                              /* flags */
        gcRunAll,                       /* update function */
        scManagerFuncDraw,              /* frame draw function */
        NULL,                           /* allocatable memory pool start */
        0,                              /* allocatable memory pool size */
        1,                              /* ??? */
        2,                              /* number of contexts? */
        0, 0, 0, 0,                     /* the four DL buffer sizes */
        0,                              /* graphics heap size */
        2,                              /* ??? */
        0,                              /* RDP output buffer size */
        NULL,                           /* pre-render function */
        syControllerFuncRead,           /* controller I/O function */
    },

    0,                                  /* number of GObjThreads */
    sizeof(u64) * 192,                  /* thread stack size */
    0,                                  /* number of thread stacks */
    0,                                  /* ??? */
    MVENDING_GOBJPROCS,
    MVENDING_GOBJS,     sizeof(GObj),
    MVENDING_XOBJS,
    NULL,                               /* matrix function list */
    NULL,                               /* DObjVec eject function */
    MVENDING_AOBJS,
    MVENDING_MOBJS,
    MVENDING_DOBJS,     sizeof(DObj),
    MVENDING_SOBJS,     sizeof(SObj),
    MVENDING_COBJS,     sizeof(CObj),

    mvEndingFuncStart                   /* task start function */
};

/* The port's own bzero arm for dSCManagerOverlays[54] (src/dc/overlay.c),
 * the same role scAutoDemoOverlayLoad/scExplainOverlayLoad play for
 * overlays 64/63. */
void mvEndingOverlayLoad(void)
{
    OVERLAY_CLEAR(sMVEndingFigatreeHeap);
    OVERLAY_CLEAR(sMVEndingTotalTimeTics);
    OVERLAY_CLEAR(sMVEndingRoomCameraGObj);
    OVERLAY_CLEAR(sMVEndingFighterCameraGObj);
    OVERLAY_CLEAR(sMVEndingFighterGObj);
    OVERLAY_CLEAR(sMVEndingRoomBackgroundGObj);
    OVERLAY_CLEAR(sMVEndingRoomBackground1GObj);
    OVERLAY_CLEAR(sMVEndingRoomDeskGObj);
    OVERLAY_CLEAR(sMVEndingRoomBooksGObj);
    OVERLAY_CLEAR(sMVEndingRoomPencilsGObj);
    OVERLAY_CLEAR(sMVEndingRoomLampGObj);
    OVERLAY_CLEAR(sMVEndingRoomTissuesGObj);
    OVERLAY_CLEAR(sMVEndingRoomFadeInAlpha);
    OVERLAY_CLEAR(sMVEndingRoomLightAlpha);
    OVERLAY_CLEAR(sMVEndingRoomFadeInGObj);
    OVERLAY_CLEAR(sMVEndingRoomLightGObj);
    OVERLAY_CLEAR(sMVEndingFighterDemoDesc);
    OVERLAY_CLEAR(sMVEndingUnused0x80132C14);
    OVERLAY_CLEAR(sMVEndingCamAnimBank);
    /* the props' packs go back with the scene (fighter_load_scene), so a
     * second visit has to load them again rather than trust the flag */
    OVERLAY_CLEAR(sMVEndingPropPacks);
    OVERLAY_CLEAR(sMVEndingPropIsLoaded);
}
