/* mvopeningroom.c -- see mvopeningroom.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningroom.c; every function names
 * its line range. The two DIVERGES that remain (the N64 swap-buffer
 * hook and the standard video/reloc/figatree cuts) are explained once,
 * in mvopeningroom.h's own header comment -- not repeated at every
 * site. The relocData scene-graph gap that once took the whole room set
 * and Master Hand's pack is closed; the header keeps
 * the record. The five camera-anim calls are verbatim (src/dc/camanim.h).
 */
#include "mvopeningroom.h"
#include "overlay.h"

#include "camanim.h"    /* the cameras' AObjEvent32 scripts */
#include "fighter.h"    /* the room props' packs */
#include "objmodel.h"   /* dc_model_add_dobjs, dc_model_proc_display */
#include "ftcommon.h"
#include "gmcamera.h"
#include "lbcommon.h"   /* lbCommonDrawSprite, lbCommonDrawSObjNoAttr */
#include "scmanager.h"
#include "sprite.h"
#include "lbpdraw.h"
#include "objpvr.h"   /* gcEjectGObjIfMade -- see mvOpeningRoomEjectRoomGObjs */
#include "efmanager.h"
#include "taskman.h"

#include <sys/debug.h>  /* syDebugPrintf: an implicit `int f()`
                         * declaration of a VARIADIC function is the
                         * trap src/dc/fighter.h's hosttest note
                         * names */
#include <ef/efparticle.h>
#include <ft/fighter.h>
#include <ft/ftdef.h>
#include <ft/ftparam.h>
#include <ft/ftpublic.h>
#include <gm/gmrumble.h>
#include <gm/gmsound.h>
#include <sc/scsubsys/scsubsys.h> /* scSubsysFighterSetStatus/SetLightParams */
#include <sys/controller.h>
#include <sys/objanim.h>          /* gcAddCObjCamAnimJoint, gcPlayCamAnim */
#include <sys/rdp.h>
#include <lb/lbdef.h>
#include <macros.h>               /* I_SEC_TO_TICS */
#include <sys/objdef.h>           /* aobjEvent32*, the wipe's AnimJoints */
#include "assetroot.h"            /* asset_read_whole, the wipe's stars */
#include "dcpvr.h"
#include "taskman.h"               /* syTaskmanGetPhoto */
#include "mtx.h"                  /* mtx_apply */
#include <stdlib.h>
#include <string.h>

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/scexplain.c and src/dc/mnbackupclear.c do the same).
 * Only the wallpaper uses this: it is the one entry of
 * dMVOpeningRoomFileIDs that is a sprite bank rather than a scene-graph
 * tree (mvopeningroom.h). */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* relocData file 90 (MVOpeningRoomWallpaper), tools/export/ssb_spriteexport.py
 * --file 90 --list against the ROM: one Sprite, and this is its offset
 * -- the value the decomp's own &llMVOpeningRoomWallpaperSprite link
 * label carries (src/dc/decomp/reloc_data.us.h agrees, // 0x26c88). */
#define llMVOpeningRoomWallpaperSprite 0x26c88

/* ---- the pools ---------------------------------------------------------
 *
 * mvopeningroom.c:1367-1409 dMVOpeningRoomTaskmanSetup carries zero for
 * every pool count, the same placeholder scvsbattle.c:24-64/
 * scautodemo.c:56-73/scexplain.c:62-72/mvending.c:99-141 document. With
 * the room set cut this scene holds two posed fighters, eight cameras
 * (four of them replaced mid-scene) and three screen-space display
 * GObjs -- two fighters is scexplain.c's own load, and the camera count
 * is higher than any of them, so this takes scexplain.c's figures with
 * the cameras doubled and the SObj pool cut to what one wallpaper
 * sprite needs. */
#define MVOPENINGROOM_GOBJS       48
#define MVOPENINGROOM_GOBJPROCS   48
#define MVOPENINGROOM_XOBJS      288
#define MVOPENINGROOM_AOBJS      576
#define MVOPENINGROOM_MOBJS       32
#define MVOPENINGROOM_DOBJS       96
#define MVOPENINGROOM_SOBJS       16
#define MVOPENINGROOM_COBJS       12

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* mvopeningroom.c:42-147. The three figatree heaps are kept as fields
 * (mvopeningroom.h says why they stay NULL); the two LBFileNode status
 * buffers and the eight-entry sMVOpeningRoomFiles array are gone with
 * the reloc loader, replaced by the one sprite bank below.
 *
 * sMVOpeningRoomDroppedFighterKind is declared GObj* in the decomp and
 * used as an s32 fkind at both its write (mvOpeningRoomInitVars) and
 * its read (mvOpeningRoomFuncRun tic 695) -- a decomp typing slip, not
 * a real pointer. It is an s32 here. */
static void *sMVOpeningRoomBossFigatreeHeap;
static void *sMVOpeningRoomPluckedFigatreeHeap;
static void *sMVOpeningRoomDroppedFigatreeHeap;
static s32 sMVOpeningRoomTotalTimeTics;
static GObj *sMVOpeningRoomMainCameraGObj;
static GObj *sMVOpeningRoomFighterCameraGObj;
static GObj *sMVOpeningRoomLogoCameraGObj;
static GObj *sMVOpeningRoomBossGObj;
static s32 sMVOpeningRoomPulledFighterKind;
static s32 sMVOpeningRoomDroppedFighterKind;
static GObj *sMVOpeningRoomLogoGObj;
static GObj *sMVOpeningRoomPulledFighterGObj;
static GObj *sMVOpeningRoomDroppedFighterGObj;
static GObj *sMVOpeningRoomGObj;
static GObj *sMVOpeningRoomSunlightGObj;
static GObj *sMVOpeningRoomDeskGObj;
static GObj *sMVOpeningRoomOutsideGObj;
static GObj *sMVOpeningRoomOutsideHazeGObj;
static GObj *sMVOpeningRoomBooksGObj;
static GObj *sMVOpeningRoomPencilsGObj;
static GObj *sMVOpeningRoomLampGObj;
static GObj *sMVOpeningRoomTissuesGObj;
static GObj *sMVOpeningRoomBossShadowGObj;
static s32 sMVOpeningRoomOverlayAlpha;
static GObj *sMVOpeningRoomOverlayGObj;
/* TRUE while the close-up dimmer is up, tic 450 to the wipe: the room's
 * camera draws at sMVOpeningRoomOverlayAlpha's darkness instead of the
 * dimmer drawing over it (mvOpeningRoomMainCameraProcDisplay) */
static sb32 sMVOpeningRoomCloseUpIsDim;
static GObj *sMVOpeningRoomSpotlightGObj;
static GObj *sMVOpeningRoomBackgroundGObj;
static GObj *sMVOpeningTransitionOutlineGObj;
static GObj *sMVOpeningTransitionOverlayGObj;
static GObj *sMVOpeningRoomCameraGObj;
static s32 sMVOpeningRoomUnused0x80134D54;

/* The port's stand-in for the four plain props of sMVOpeningRoomFiles[0]:
 * the desk, the books, the pencils and the lamp, each
 * baked out of relocData 52 by tools/export/ssb_scenemodelexport.py -- the
 * general exporter the openings' own models already use -- into a pack
 * of the shape src/dc/fighter.h reads: a DObjDesc tree of plain display
 * lists, plus the file's own AnimJoint table as the pack's one
 * animation. This is the general exporter's `[dobj]` shape.
 *
 * fighter_load_scene, not fighter_load: these live as long as the scene
 * and go back at the scene change with the rest of it (src/dc/fighter.c
 * sFighterScenePacks), so there is no release here to forget and no
 * pack held across the attract chain. */
/* and its five LONE display lists: the same file, the same
 * exporter and the same pack, but the scene reaches these through
 * gcAddDObjForGObj rather than gcSetupCommonDObjs -- one block of
 * display-list commands (or of DObjDLLinks) bound straight to one DObj,
 * with no DObjDesc array to read, which is the exporter's `[dl]` shape,
 * --dl. A pack of one joint at the identity is
 * what comes out, and the scene's own transform is what moves it, which
 * is exactly the N64's arrangement. */
#define MVOPENINGROOM_DESK_MODEL     "mvopeningroomdesk.mdl"
#define MVOPENINGROOM_BOOKS_MODEL    "mvopeningroombooks.mdl"
#define MVOPENINGROOM_PENCILS_MODEL  "mvopeningroompencils.mdl"
#define MVOPENINGROOM_LAMP_MODEL     "mvopeningroomlamp.mdl"
#define MVOPENINGROOM_SUNLIGHT_MODEL "mvopeningroomsunlight.mdl"
#define MVOPENINGROOM_OUTSIDE_MODEL  "mvopeningroomoutside.mdl"
#define MVOPENINGROOM_HAZE_MODEL     "mvopeningroomhaze.mdl"
#define MVOPENINGROOM_TISSUES_MODEL  "mvopeningroomtissues.mdl"
#define MVOPENINGROOM_SHADOW_MODEL   "mvopeningroombossshadow.mdl"

/* and the two that carry MATERIALS: the same file and the
 * same exporter again, with --mobjsub and --matanim, because their
 * makers call gcAddMObjAll and gcAddMatAnimJointAll beside the tree.
 * The pack keeps the MObjSub records, the batch-to-MObj link and the
 * MatAnimJoint scripts (fighter.h FPackMObjs, the block
 * tools/export/ssb_emblemexport.py first baked), and src/dc/objmodel.c's
 * dc_model_add_mobjs builds the two pointer-array shapes back out of
 * them and hands them to those same two decomp functions -- so the
 * fade each script drives is played by the game's parser, not by
 * anything here. */
#define MVOPENINGROOM_DESKGND_MODEL  "mvopeningroomdeskground.mdl"
#define MVOPENINGROOM_LOGO_MODEL     "mvopeningroomlogo.mdl"

/* and the room itself, which is the same shape again --
 * a tree, an MObjSub table and a MatAnimJoint table -- but 51 joints
 * against FIGHTER_MAX_JOINTS's 40, so it is TWO packs off one
 * DObjDesc array (--entries 0-17 and 0,18-50: entry 0 is the tree's
 * one root, at the identity with no display list, and goes in both).
 * scstaffroll.h and mvopeningyoster.c both argue out why splitting is
 * the fix rather than raising the cap -- it is a fighter-rig figure
 * the port's matrix arrays are sized to, and a room is not a rig. */
#define MVOPENINGROOM_BG0_MODEL      "mvopeningroombackground0.mdl"
#define MVOPENINGROOM_BG1_MODEL      "mvopeningroombackground1.mdl"

/* and the last four, which took no new exporter work at
 * all -- they are the three shapes above composed. The camera snap is
 * a tree with an AnimJoint (exported with --dllinks);
 * the close-up effect's two halves are a tree with an AnimJoint AND a
 * MatAnimJoint, which needs both sets of flags on one command line; the
 * trophy spotlight is a LONE display list with materials, exported with
 * --dl and --dlhead 1 beside --mobjsub and --matanim. */
#define MVOPENINGROOM_SNAP_MODEL     "mvopeningroomsnap.mdl"
#define MVOPENINGROOM_CUAIR_MODEL    "mvopeningroomcloseupair.mdl"
#define MVOPENINGROOM_CUGND_MODEL    "mvopeningroomcloseupground.mdl"
#define MVOPENINGROOM_SPOT_MODEL     "mvopeningroomspotlight.mdl"

enum
{
    nMVOpeningRoomPropDesk,
    nMVOpeningRoomPropBooks,
    nMVOpeningRoomPropPencils,
    nMVOpeningRoomPropLamp,
    nMVOpeningRoomPropSunlight,
    nMVOpeningRoomPropOutside,
    nMVOpeningRoomPropHaze,
    nMVOpeningRoomPropTissues,
    nMVOpeningRoomPropBossShadow,
    nMVOpeningRoomPropDeskGround,
    nMVOpeningRoomPropLogo,
    nMVOpeningRoomPropBackground0,
    nMVOpeningRoomPropBackground1,
    nMVOpeningRoomPropSnap,
    nMVOpeningRoomPropCloseUpAir,
    nMVOpeningRoomPropCloseUpGround,
    nMVOpeningRoomPropSpotlight,
    nMVOpeningRoomPropCount
};

static const char *const dMVOpeningRoomPropModels[nMVOpeningRoomPropCount] =
{
    MVOPENINGROOM_DESK_MODEL,
    MVOPENINGROOM_BOOKS_MODEL,
    MVOPENINGROOM_PENCILS_MODEL,
    MVOPENINGROOM_LAMP_MODEL,
    MVOPENINGROOM_SUNLIGHT_MODEL,
    MVOPENINGROOM_OUTSIDE_MODEL,
    MVOPENINGROOM_HAZE_MODEL,
    MVOPENINGROOM_TISSUES_MODEL,
    MVOPENINGROOM_SHADOW_MODEL,
    MVOPENINGROOM_DESKGND_MODEL,
    MVOPENINGROOM_LOGO_MODEL,
    MVOPENINGROOM_BG0_MODEL,
    MVOPENINGROOM_BG1_MODEL,
    MVOPENINGROOM_SNAP_MODEL,
    MVOPENINGROOM_CUAIR_MODEL,
    MVOPENINGROOM_CUGND_MODEL,
    MVOPENINGROOM_SPOT_MODEL,
};

static Fighter sMVOpeningRoomPropPacks[nMVOpeningRoomPropCount];
static sb32 sMVOpeningRoomPropIsLoaded[nMVOpeningRoomPropCount];

/* The port's stand-in for sMVOpeningRoomFiles[7]: file 90's sprite bank
 * (romdisk/mvopeningroomwallpaper.spr), the same shape
 * sSCExplainGraphicsBank/sSCStaffrollGraphicsBank already take. */
static SpriteBank sMVOpeningRoomWallpaperBank;
static void *sMVOpeningRoomWallpaperFileHead;

/* The port's stand-in for sMVOpeningRoomFiles[2..5]: the
 * four scene files 56-59 hold nothing but one camera-anim script each,
 * so the whole of what this scene wanted out of them is
 * romdisk/mvopeningroom.cam's four scripts (src/dc/camanim.h). */
static CamAnimBank sMVOpeningRoomCamAnimBank;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningroom.c:30-33 dMVOpeningRoomLights11/12 and 156-160
 * mvOpeningRoomFuncLights. Not ported: the two Lights1 tables are
 * unreferenced even in the decomp, and the proc's whole body is
 * ftDisplayLightsDrawReflect, the dropped reflection lighting every
 * other scene's func_lights drops. func_lights is NULL in
 * dMVOpeningRoomTaskmanSetup below. */

/* mvopeningroom.c:163-173 mvOpeningRoomBackgroundProcUpdate 0x80131AF4,
 * verbatim. The room fades OUT over the 60 tics after 18 s -- its
 * MatAnimJoint script is 0xFFFFFFFF at frame 0 and 0xFFFFFF00 at frame
 * 60, the mirror of the desk ground's below -- and then takes itself
 * away at 19 s. Running on BOTH halves of the split tree is what the
 * port wants here rather than a special case: each half ejects itself
 * on the same tic, which is what the one GObj did. */
void mvOpeningRoomBackgroundProcUpdate(GObj *gobj)
{
    if (sMVOpeningRoomTotalTimeTics > I_SEC_TO_TICS(18))
    {
        gcPlayAnimAll(gobj);
    }
    if (sMVOpeningRoomTotalTimeTics == I_SEC_TO_TICS(19))
    {
        gcEjectGObj(gobj);
    }
}

/* mvopeningroom.c:505-511 mvOpeningRoomDeskGroundProcUpdate 0x80132AB0,
 * verbatim. The desk's ground fades IN over the 60 tics after 1060 --
 * its MatAnimJoint script is 0xFFFFFF00 at frame 0 and 0xFFFFFFFF at
 * frame 60 -- and this is what starts it. */
void mvOpeningRoomDeskGroundProcUpdate(GObj *gobj)
{
    if (sMVOpeningRoomTotalTimeTics > 1060)
    {
        gcPlayAnimAll(gobj);
    }
}

/* mvopeningroom.c:278-284 mvOpeningRoomTissuesProcUpdate 0x801320FC,
 * verbatim. Byte for byte mvOpeningRoomCommonProcUpdate below, and a
 * separate function in the game all the same. */
void mvOpeningRoomTissuesProcUpdate(GObj *gobj)
{
    if (sMVOpeningRoomTotalTimeTics >= 560)
    {
        gcPlayAnimAll(gobj);
    }
}

/* mvopeningroom.c:230-236 mvOpeningRoomCommonProcUpdate 0x80131E88,
 * verbatim. The books, the pencils and the lamp all start their
 * animation at tic 560 rather than at the scene's start, and this is
 * how: the proc runs every tic and plays nothing until then. */
void mvOpeningRoomCommonProcUpdate(GObj *gobj)
{
    if (sMVOpeningRoomTotalTimeTics >= 560)
    {
        gcPlayAnimAll(gobj);
    }
}

/* gcSetupCommonDObjs and gcAddAnimJointAll over one of file 52's prop
 * trees, which the port reads out of a pack instead.
 * mnTitleSetupAnimTree (src/dc/mntitle.c) is the same substitution over
 * the same two calls and says the rest: the DObjs come from
 * dc_model_add_dobjs, and gcAddAnimJointAll is handed the table the
 * pack's animation carries, one pointer per joint in tree order, which
 * is the table the game's file holds. The host build builds the DObjs
 * and adds no animation -- `union AObjEvent32` is eight bytes on x86-64
 * and its interpreter cannot read the words (src/dc/grcastle.c).
 *
 * And gcAddMObjAll + gcAddMatAnimJointAll for a
 * prop whose pack carries materials: dc_model_add_mobjs rebuilds the
 * `MObjSub ***` and `AObjEvent32 ***` the file held and calls those two
 * itself (src/dc/objmodel.c), so the call here is one rather than two.
 * A pack without materials returns 0 and adds nothing, which is every
 * prop above. */
static void mvOpeningRoomSetupPropTree(GObj *gobj, s32 prop)
{
    DObj *joints[FIGHTER_MAX_JOINTS];
    Fighter *model = &sMVOpeningRoomPropPacks[prop];
    s32 njoints;

    if (sMVOpeningRoomPropIsLoaded[prop] == FALSE)
    {
        int pal_bank = 0;

        if (fighter_load_scene(model, dMVOpeningRoomPropModels[prop],
                               &pal_bank) != 0)
        {
            syDebugPrintf("mvOpeningRoom: no %s; that prop is not in the "
                          "room\n", dMVOpeningRoomPropModels[prop]);
            return;
        }
        sMVOpeningRoomPropIsLoaded[prop] = TRUE;
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
        gcAddAnimJointAll(gobj, anim_joints, 0.0F);
    }
    if (njoints > 0)
    {
        dc_model_add_mobjs(gobj, model, 0.0F);
    }
#else
    (void)njoints;
#endif
}

/* mvopeningroom.c:176-187 mvOpeningRoomMakeBackground 0x80131B7C, the
 * room Master Hand's desk stands in: the walls, the window, the floor
 * and the shelves. Verbatim but for the tree, its display proc and its
 * materials -- the substitutions mvOpeningRoomSetupPropTree makes --
 * and one DIVERGES of its own.
 *
 * DIVERGES: the tree is 51 joints and FIGHTER_MAX_JOINTS is 40, so it
 * is TWO GObjs at the same origin carrying two packs, the same fix
 * mvOpeningYosterMakeNest makes for the nest's 73. The split is entry
 * 0 with 1-17, and entry 0 again with 18-50: entry 0 is the array's
 * one root, at the identity with no display list of its own, so
 * duplicating it costs a DObj and draws nothing twice. Every one of
 * the seven MObjSubs is on the second half, which is why only it
 * carries --mobjsub/--matanim.
 *
 * The update proc goes on both, and that is not a special case: it
 * plays each half's own MatAnimJoint fade at 18 s and ejects its own
 * GObj at 19 s, which between them is exactly what the one GObj did.
 * sMVOpeningRoomGObj holds the first half; nothing in this file or the
 * decomp's ever reads it (only OVERLAY_CLEAR does), so the second half
 * needs no static of its own. */
void mvOpeningRoomMakeBackground(void)
{
    GObj *gobj;

    sMVOpeningRoomGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropBackground0);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mvOpeningRoomBackgroundProcUpdate, nGCProcessKindFunc, 1);
    gcPlayAnimAll(gobj);

    gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropBackground1);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mvOpeningRoomBackgroundProcUpdate, nGCProcessKindFunc, 1);
    gcPlayAnimAll(gobj);
}

/* mvopeningroom.c:190-197 mvOpeningRoomMakeSunlight 0x80131C84,
 * 210-217 mvOpeningRoomMakeOutside 0x80131D80 and 220-227
 * mvOpeningRoomMakeHaze 0x80131E04, all three verbatim but for the two
 * substitutions the desk's maker already takes (the tree and the
 * display proc), plus one the desk did not need.
 *
 * The game writes each of these as
 *
 *     gcAddXObjForDObjFixed(gcAddDObjForGObj(gobj, <DisplayList>),
 *                           nGCMatrixKindTraRotRpyRSca, 0);
 *
 * and the port's dc_model_add_dobjs gives every joint it builds the
 * Tra/RotRpyR/Sca triple instead (gcAddDObjMatrixSetsRpyR,
 * objhelper.c:275-280). Those are the same matrix, not two policies:
 * src/dc/objdisplay.c's gcDObjLocalMatrix builds
 * nGCMatrixKindTraRotRpyRSca as exactly RotRpyR, then the scale down
 * the three axes, then the translate row -- the product the triple's
 * three XObjs multiply out to, off the same three DObj vectors. So the
 * XObj is not added again here; adding it would apply the transform
 * twice.
 *
 * The display proc differs the other way round from the desk's. These
 * three call gcDrawDObjDLLinksForGObj rather than
 * gcDrawDObjTreeForGObj, because their block is a DObjDLLink ARRAY --
 * a list of (head, display list) pairs -- rather than commands; the
 * exporter reads it with --dllinks and bakes each entry into its own
 * batch with that head's PVR list, so dc_model_proc_display submits
 * the same lists into the same places without the dispatch. */
void mvOpeningRoomMakeSunlight(void)
{
    GObj *gobj;

    sMVOpeningRoomSunlightGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropSunlight);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mvopeningroom.c:200-207 mvOpeningRoomMakeDesk 0x80131D08, verbatim
 * but for the tree (mvOpeningRoomSetupPropTree) and its display proc:
 * the game's gcDrawDObjTreeForGObj walks the tree and runs each DObj's
 * display list as it goes, and the port's dc_model_proc_display walks
 * the same tree and submits the pack's batches after it
 * (src/dc/objmodel.h). Master Hand's desk. */
void mvOpeningRoomMakeDesk(void)
{
    GObj *gobj;

    sMVOpeningRoomDeskGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropDesk);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);
}

void mvOpeningRoomMakeOutside(void)
{
    GObj *gobj;

    sMVOpeningRoomOutsideGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropOutside);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);
}

void mvOpeningRoomMakeHaze(void)
{
    GObj *gobj;

    sMVOpeningRoomOutsideHazeGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropHaze);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mvopeningroom.c:239-249 mvOpeningRoomMakeBooks 0x80131EBC, 252-262
 * mvOpeningRoomMakePencils 0x80131F7C and 265-275 mvOpeningRoomMakeLamp
 * 0x8013203C, all three verbatim but for the same two substitutions the
 * desk takes, plus one ordering note: the game adds the animation after
 * the display proc (and the pencils add it before their update proc),
 * and here the tree and its animation arrive together out of the pack,
 * so gcAddAnimJointAll runs first. Nothing reads either between the two
 * calls; gcPlayAnimAll below is still what starts them, and
 * mvOpeningRoomCommonProcUpdate is still what restarts them at tic 560. */
void mvOpeningRoomMakeBooks(void)
{
    GObj *gobj;

    sMVOpeningRoomBooksGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropBooks);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mvOpeningRoomCommonProcUpdate, nGCProcessKindFunc, 1);
    gcPlayAnimAll(gobj);
}

void mvOpeningRoomMakePencils(void)
{
    GObj *gobj;

    sMVOpeningRoomPencilsGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropPencils);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mvOpeningRoomCommonProcUpdate, nGCProcessKindFunc, 1);
    gcPlayAnimAll(gobj);
}

void mvOpeningRoomMakeLamp(void)
{
    GObj *gobj;

    sMVOpeningRoomLampGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropLamp);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mvOpeningRoomCommonProcUpdate, nGCProcessKindFunc, 1);
    gcPlayAnimAll(gobj);
}

/* mvopeningroom.c:287-299 mvOpeningRoomMakeTissues 0x80132130,
 * verbatim but for the three substitutions above and one more that
 * belongs to the animation.
 *
 * This one binds its AnimJoint with gcAddDObjAnimJoint(dobj, script,
 * 0.0F) -- the object system's other way in, which hands over one
 * AObjEvent32 script rather than gcAddAnimJointAll's per-joint table of
 * pointers to them. The port reads it with the exporter's --animdl and
 * carries it as the pack's one animation all the same, and
 * mvOpeningRoomSetupPropTree then feeds it back through
 * gcAddAnimJointAll over a one-entry array, which for a tree of one
 * joint is the same call: objanim.c:165-185 walks the tree and does
 * gcAddDObjAnimJoint(dobj, *anim_joints, anim_frame) on the only DObj
 * there is. It also sets that DObj's `is_anim_root` and the GObj's
 * anim_frame, neither of which anything in this scene reads -- the
 * flag is only consulted where parent_gobj->func_anim is non-NULL
 * (objanim.c:517/527/581) and these GObjs have none, the same
 * reasoning src/dc/grsector.c:589-612 already spells out for Sector Z's
 * Arwing. */
void mvOpeningRoomMakeTissues(void)
{
    GObj *gobj;

    sMVOpeningRoomTissuesGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropTissues);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mvOpeningRoomTissuesProcUpdate, nGCProcessKindFunc, 1);
    gcPlayAnimAll(gobj);
}

/* mvopeningroom.c:302-316 mvOpeningRoomMakeBoss, verbatim.
 * Master Hand has a fighter pack, a status table and an entrance.
 * 0x1000F is FTSTAT_OPENING1_START, row 15 of his SubMotion
 * table -- the first of the three in his own opening band.
 *
 * sMVOpeningRoomBossFigatreeHeap is NULL, as the two makers below pass
 * NULL heaps: the three syTaskmanMalloc calls the decomp's FuncStart
 * makes are a DIVERGES of this port's own scene memory (mvopeningroom.h,
 * and [[scene-memory-strategy]]), not of this function. */
void mvOpeningRoomMakeBoss(void)
{
    GObj *fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = nFTKindBoss;
    desc.costume = ftParamGetCostumeCommonID(nFTKindBoss, 0);
    desc.pos.x = 0.0F;
    desc.pos.y = 0.0F;
    desc.pos.z = 0.0F;
    desc.figatree_heap = sMVOpeningRoomBossFigatreeHeap;
    sMVOpeningRoomBossGObj = fighter_gobj = ftManagerMakeFighter(&desc);

    scSubsysFighterSetStatus(fighter_gobj, FTSTAT_OPENING1_START);
}

/* mvopeningroom.c:319-322 mvOpeningFighterProcUpdate, verbatim.
 * It is the proc row 8 of D_ovl1_80390BE8 carries -- the
 * plucked trophy's, nFTDemoStatusFigurePulled -- and its only job is to
 * name the hand this scene made, since the table's rows take one GObj
 * and this needs two.
 *
 * mvopeningroom.c:325-349 func_ovl34_801322C8/80132320/80132328. Not
 * ported: marked "Unused?" in the decomp and genuinely unreferenced
 * anywhere in the tree. */
void mvOpeningFighterProcUpdate(GObj *fighter_gobj)
{
    scSubsysFighterOpeningProcUpdate(sMVOpeningRoomBossGObj, fighter_gobj);
}

/* mvopeningroom.c:352-372 mvOpeningRoomMakePulledFighter, verbatim: the
 * trophy Master Hand plucks off the desk, one of the eight originals
 * drawn at random, scaled to 1 and moved onto DL link 6. Real -- all
 * eight kinds have packs. Its nFTDemoStatusFigurePulled pose is the
 * port's standing demo-status gap (mvopeningroom.h). */
void mvOpeningRoomMakePulledFighter(s32 fkind)
{
    GObj *fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = fkind;
    desc.costume = ftParamGetCostumeCommonID(fkind, 0);
    desc.pos.x = 0.0F;
    desc.pos.y = 0.0F;
    desc.pos.z = 0.0F;
    desc.figatree_heap = sMVOpeningRoomPluckedFigatreeHeap;
    sMVOpeningRoomPulledFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

    DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;

    scSubsysFighterSetStatus(fighter_gobj, nFTDemoStatusFigurePulled);

    gcMoveGObjDL(fighter_gobj, 6, -1);
}

/* mvopeningroom.c:375-397 mvOpeningRoomLogoWallpaperProcDisplay: the
 * opening's black curtain, fading off over ~20 tics from tic 60.
 * DIVERGES: it is a gDPFillRectangle under G_CC_PRIMITIVE, and the raw
 * GBI lands in the write-only scratch heads (src/dc/wpmanager.c; that
 * it wrote head 1 only ever chose a scratch row), so it is the fill
 * quad src/dc/ifscreenflash.c draws instead, translucent pass only --
 * the fade runs once a frame with it, as the N64's one-pass draw had
 * it. */
void mvOpeningRoomLogoWallpaperProcDisplay(GObj *gobj)
{
    (void)gobj;

    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    if (sMVOpeningRoomTotalTimeTics >= 60)
    {
        if (sMVOpeningRoomOverlayAlpha > 0x00)
        {
            sMVOpeningRoomOverlayAlpha -= 0x0D;

            if (sMVOpeningRoomOverlayAlpha < 0x00)
            {
                sMVOpeningRoomOverlayAlpha = 0x00;
            }
        }
    }
    lbCommonSpriteFillRect(10, 10, 310, 230, 0x00, 0x00, 0x00, sMVOpeningRoomOverlayAlpha);
}

/* mvopeningroom.c:400-407 mvOpeningRoomMakeLogoWallpaper, verbatim. */
void mvOpeningRoomMakeLogoWallpaper(void)
{
    GObj *gobj;

    sMVOpeningRoomOverlayAlpha = 0xFF;
    sMVOpeningRoomOverlayGObj = gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, mvOpeningRoomLogoWallpaperProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mvopeningroom.c:410-421 mvOpeningRoomMakeLogo 0x801325C8, verbatim
 * but for the same four-call substitution the desk's ground takes. The
 * game's logo over the wallpaper, on GObj link 21 and display link 29,
 * which is the only link mvOpeningRoomMakeLogoCamera's mask names --
 * that camera frames this. */
void mvOpeningRoomMakeLogo(void)
{
    GObj *gobj;

    sMVOpeningRoomLogoGObj = gobj = gcMakeGObjSPAfter(0, NULL, 21, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropLogo);
    /* DIVERGES: dc_model_proc_display_layered for dc_model_proc_display.
     * The logo camera (50) runs after the curtain's (60), so on the N64
     * the logo shows over the black from tic 0 and the room fades in
     * behind it. The curtain is a translucent-list quad at the front
     * sprite depth, which beats every opaque-list pixel, so an opaque
     * logo sat under it until the curtain was gone. The layered draw
     * puts the model in the translucent list at the next sprite depth,
     * past the curtain's, as the menus' fighters stand over their
     * panels (src/dc/objmodel.c). */
    gcAddGObjDisplay(gobj, dc_model_proc_display_layered, 29, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
    gcPlayAnimAll(gobj);
}

/* mvopeningroom.c:424-434 mvOpeningRoomMakeSnap 0x80132680, verbatim
 * but for the tree and its display proc -- the two substitutions the
 * desk's maker takes, nothing more. The camera-flash quad the scene
 * makes at tic 860 on GObj link 19 and display link 27, which is the
 * fighter camera's. Its GObj is never stored and never ejected by
 * name; the scene change takes it. */
void mvOpeningRoomMakeSnap(void)
{
    GObj *gobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 19, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropSnap);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 27, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
    gcPlayAnimAll(gobj);
}

/* mvopeningroom.c:437-470 mvOpeningRoomMakeCloseUpEffect 0x80132738,
 * both halves, verbatim but for the same substitutions. These two are
 * the first models in this file to carry an AnimJoint AND a
 * MatAnimJoint at once -- the tree moves and its colour moves -- which
 * the pack can hold: mvOpeningRoomSetupPropTree hands the one to
 * gcAddAnimJointAll and the other to dc_model_add_mobjs, in that
 * order, which is the decomp's own order here too.
 *
 * Each half's root translate is zeroed after the tree, exactly as the
 * game does it. That is not redundant on this port either: the root
 * comes out of the pack carrying the DObjDesc's own translate, and
 * this is the game overriding it. */
void mvOpeningRoomMakeCloseUpEffect(void)
{
    GObj *gobj;

    /* Close-up effect air */
    gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropCloseUpAir);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);

    DObjGetStruct(gobj)->translate.vec.f.x = 0.0F;
    DObjGetStruct(gobj)->translate.vec.f.y = 0.0F;
    DObjGetStruct(gobj)->translate.vec.f.z = 0.0F;

    gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
    gcPlayAnimAll(gobj);

    /* Close-up effect ground */
    gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropCloseUpGround);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);

    DObjGetStruct(gobj)->translate.vec.f.x = 0.0F;
    DObjGetStruct(gobj)->translate.vec.f.y = 0.0F;
    DObjGetStruct(gobj)->translate.vec.f.z = 0.0F;

    gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
    gcPlayAnimAll(gobj);
}

/* mvopeningroom.c:473-488 mvOpeningRoomMakeDroppedFighter, verbatim:
 * the second trophy, dropped onto the desk at tic 695 at the room's own
 * floor position. Real, same as the plucked one above. */
void mvOpeningRoomMakeDroppedFighter(s32 fkind)
{
    GObj *fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = fkind;
    desc.costume = ftParamGetCostumeCommonID(fkind, 0);
    desc.figatree_heap = sMVOpeningRoomDroppedFigatreeHeap;
    desc.pos.x = 872.3249512F;
    desc.pos.y = 4038.864014F;
    desc.pos.z = -4734.600098F;
    sMVOpeningRoomDroppedFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

    scSubsysFighterSetStatus(fighter_gobj, nFTDemoStatusFigureDropped);
    gcMoveGObjDL(fighter_gobj, 6, -1);
}

/* mvopeningroom.c:491-502 mvOpeningRoomMakeBossShadow 0x801329F0,
 * verbatim but for the same substitutions the tissue box takes. Master
 * Hand's shadow on the desk: a 60-triangle fan on display link 9, the
 * fighter camera's (with 27), not link 6's.
 *
 * Its display proc is gcDrawDObjDLHead1, not gcDrawDObjDLHead0, and
 * that is the whole of what decides which PVR list it lands in. The
 * shadow's display list is untextured and sets no render mode of its
 * own, so MeshBaker.bucket_of has nothing to read and falls back to the
 * head id -- objdisplay.c queues opaque into heads 0 and 2 and
 * translucent into 1 and 3, so head 1 is translucent and head 0 would
 * have been a black polygon over the desk. The exporter's --dlhead 1
 * is how the maker's choice reaches the bake; nothing in the file says
 * it. */
void mvOpeningRoomMakeBossShadow(void)
{
    GObj *gobj;

    sMVOpeningRoomBossShadowGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropBossShadow);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 9, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
}

/* mvopeningroom.c:514-525 mvOpeningRoomMakeDeskGround 0x80132B40,
 * verbatim but for the tree, its display proc and its materials -- the
 * three substitutions mvOpeningRoomSetupPropTree above makes, which for
 * this maker stand in for FOUR decomp calls rather than two:
 * gcSetupCommonDObjs, gcAddMObjAll and gcAddMatAnimJointAll all come
 * out of the pack, and gcDrawDObjTreeDLLinksForGObj becomes
 * dc_model_proc_display because the tree's DObjDLLink payloads are
 * baked into the batches' buckets (--dllinks). The desktop the trophies
 * land on after the wipe. */
void mvOpeningRoomMakeDeskGround(void)
{
    GObj *gobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropDeskGround);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mvOpeningRoomDeskGroundProcUpdate, nGCProcessKindFunc, 1);
    gcPlayAnimAll(gobj);
}

/* mvopeningroom.c:528-547 mvOpeningRoomCloseUpOverlayProcDisplay: the
 * close-up's dimming veil, fading up to 0xA0 from tic 450. The same
 * substitution the curtain above takes (G_RM_CLD_SURF blends by alpha
 * as G_RM_AA_XLU_SURF does, which is the fill quad's one blend). */
void mvOpeningRoomCloseUpOverlayProcDisplay(GObj *gobj)
{
    (void)gobj;

    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    if (sMVOpeningRoomOverlayAlpha < 0xA0)
    {
        sMVOpeningRoomOverlayAlpha += 0x09;

        if (sMVOpeningRoomOverlayAlpha > 0xA0)
        {
            sMVOpeningRoomOverlayAlpha = 0xA0;
        }
    }
    /* DIVERGES: no quad. The N64 fills the viewport black at this alpha
     * with a render mode that writes no Z, between the room's camera
     * (80) and the fighter camera (40), which then draws the trophy,
     * its spotlight and Master Hand undimmed yet still Z-tested against
     * the room. A port sprite is in front of every model, so the quad
     * dimmed those too and swallowed the spotlight's beam; the room's
     * camera draws itself at this darkness instead
     * (mvOpeningRoomMainCameraProcDisplay), which is the same picture. */
}

/* mvopeningroom.c:550-558 mvOpeningRoomMakeCloseUpOverlay, verbatim but
 * for the flag that turns the room's dimming on. */
void mvOpeningRoomMakeCloseUpOverlay(void)
{
    GObj *gobj;

    sMVOpeningRoomOverlayAlpha = 0x00;
    sMVOpeningRoomCloseUpIsDim = TRUE;
    sMVOpeningRoomOverlayGObj = gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, mvOpeningRoomCloseUpOverlayProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mvopeningroom.c:561-583 mvOpeningRoomMakeCloseUpOverlayCamera,
 * verbatim: a fixed-viewport sprite camera over DL link 26, no
 * relocData dependency. */
void mvOpeningRoomMakeCloseUpOverlayCamera(void)
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

/* mvopeningroom.c:586-608 mvOpeningRoomWallpaperProcDisplay, verbatim:
 * the wallpaper sprite drawn at a fixed primitive depth behind
 * everything else. Real -- its sprite is the one relocData file of this
 * scene's eight that the port does load (mvopeningroom.h). Ten gDP
 * commands a pass plus lbCommonDrawSObjNoAttr's own draw, which on the
 * PVR writes no display list at all. */
void mvOpeningRoomWallpaperProcDisplay(GObj *gobj)
{
    gDPPipeSync(gSYTaskmanDLHeads[0]++);
    gDPSetCycleType(gSYTaskmanDLHeads[0]++, G_CYC_1CYCLE);
    gDPSetPrimDepth(gSYTaskmanDLHeads[0]++, 36863.0F, 1);
    gDPSetDepthSource(gSYTaskmanDLHeads[0]++, G_ZS_PRIM);

    gDPSetRenderMode
    (
        gSYTaskmanDLHeads[0]++,
        AA_EN | Z_CMP | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | ALPHA_CVG_SEL | GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
        AA_EN | Z_CMP | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | ALPHA_CVG_SEL | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)
    );
    gDPSetCombineLERP(gSYTaskmanDLHeads[0]++, 0, 0, 0, TEXEL0,  0, 0, 0, TEXEL0,  0, 0, 0, TEXEL0,  0, 0, 0, TEXEL0);

    lbCommonDrawSObjNoAttr(gobj);

    gDPPipeSync(gSYTaskmanDLHeads[0]++);
    gDPSetCycleType(gSYTaskmanDLHeads[0]++, G_CYC_1CYCLE);
    gDPSetDepthSource(gSYTaskmanDLHeads[0]++, G_ZS_PIXEL);
    gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
}

/* mvopeningroom.c:611-623 mvOpeningRoomMakeWallpaper. Verbatim but for
 * where the Sprite comes from: sMVOpeningRoomFiles[7] is the port's own
 * file-90 sprite bank (the lbRelocGetFileData redefinition at the top of
 * this file). Guarded on the bank having loaded, the same guard
 * scExplainMakeInterfaceSObj's own NULL file head takes. */
void mvOpeningRoomMakeWallpaper(void)
{
    GObj *gobj;
    SObj *sobj;

    if (sMVOpeningRoomWallpaperFileHead == NULL)
    {
        return;
    }
    sMVOpeningRoomBackgroundGObj = gobj = gcMakeGObjSPAfter(0, NULL, 20, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, mvOpeningRoomWallpaperProcDisplay, 28, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningRoomWallpaperFileHead, llMVOpeningRoomWallpaperSprite));

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;
}

/* mvopeningroom.c:626-665 mvOpeningRoomSetSpotlightPosition, verbatim:
 * where the trophy spotlight sits and how wide it spreads, per fighter
 * kind (the four 1P-only kinds' rows are zero). Its one
 * caller is mvOpeningRoomMakeSpotlight below. */
void mvOpeningRoomSetSpotlightPosition(GObj *gobj, s32 fkind)
{
    Vec3f translates[] =
    {
        { -38.310F,   74.904F, -122.733F },
        { -38.870F,   74.904F, -121.776F },
        { -38.870F,   74.904F, -119.480F },
        { -38.870F,   74.904F, -119.480F },
        {   0.000F,    0.000F,    0.000F },
        { -37.040F,   74.904F, -119.600F },
        { -39.390F,   74.904F, -118.380F },
        {   0.000F,    0.000F,    0.000F },
        { -38.310F,   74.904F, -122.733F },
        { -39.390F,   74.904F, -118.380F },
        {   0.000F,    0.000F,    0.000F },
        {   0.000F,    0.000F,    0.000F }
    };
    Vec3f scales[] =
    {
        { 1.00F, 1.00F, 1.00F },
        { 1.10F, 1.00F, 1.10F },
        { 1.30F, 1.00F, 1.30F },
        { 1.30F, 1.00F, 1.30F },
        { 0.00F, 0.00F, 0.00F },
        { 1.00F, 1.00F, 1.00F },
        { 1.20F, 1.00F, 1.20F },
        { 0.00F, 0.00F, 0.00F },
        { 1.17F, 1.00F, 1.17F },
        { 1.20F, 1.00F, 1.20F },
        { 0.00F, 0.00F, 0.00F },
        { 0.00F, 0.00F, 0.00F }
    };

    DObjGetStruct(gobj)->translate.vec.f.x = translates[fkind].x * 30.0F;
    DObjGetStruct(gobj)->translate.vec.f.y = translates[fkind].y * 30.0F;
    DObjGetStruct(gobj)->translate.vec.f.z = translates[fkind].z * 30.0F;
    DObjGetStruct(gobj)->scale.vec.f.x = scales[fkind].x;
    DObjGetStruct(gobj)->scale.vec.f.y = scales[fkind].y;
    DObjGetStruct(gobj)->scale.vec.f.z = scales[fkind].z;
}

/* mvopeningroom.c:668-682 mvOpeningRoomMakeSpotlight 0x801330B8,
 * verbatim but for the same substitutions, and the last of this
 * file's cut makers. The pool of light the pulled trophy stands in
 * from tic 810: a LONE display list -- gcAddDObjForGObj, no DObjDesc
 * -- with two MObjSubs and two MatAnimJoint scripts on it, so its
 * pack is the exporter's --dl and --dlhead 1 (gcDrawDObjDLHead1, the
 * translucent head, which a pool of light wants) beside --mobjsub and
 * --matanim. The gcAddXObjForDObjFixed the game writes here is the
 * same equivalence mvOpeningRoomMakeSunlight's note spells out, so it
 * is not repeated; mvOpeningRoomSetSpotlightPosition below then has a
 * real DObj to place, after four steps of having none. */
void mvOpeningRoomMakeSpotlight(void)
{
    GObj *gobj;

    sMVOpeningRoomSpotlightGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    mvOpeningRoomSetupPropTree(gobj, nMVOpeningRoomPropSpotlight);
    gcAddGObjDisplay(gobj, dc_model_proc_display, 27, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
    gcPlayAnimAll(gobj);

    mvOpeningRoomSetSpotlightPosition(gobj, sMVOpeningRoomPulledFighterKind);
}

/* mvopeningroom.c:685-693 mvOpeningRoomEjectRoomGObjs. Verbatim in
 * shape. The room props it ejects are all made now, so a NULL here is a
 * prop whose pack did not load.
 *
 * DIVERGES, defensively, and it is load-bearing when it fires: ejecting a NULL
 * GObj is NOT a no-op. objman.c:1780-1786 answers NULL by setting sGCRunStatus
 * = nGCRunStatusEject, and objman.c:2132-2146 gcRunGObj acts on that the
 * instant the running GObj's func_run returns, by ejecting that GObj. NULL is
 * the object system's "eject me" signal. Since this function and the FuncRun
 * arms below run from inside mvOpeningRoomFuncRun's own GObj, a verbatim eject
 * on a prop that was never made kills the scene's driver -- the tic counter
 * stops dead at 280 (the first arm with such a call) while the fighters kept
 * animating. Every eject in this file that can see a NULL goes through
 * gcEjectGObjIfMade (src/dc/objpvr.h) instead. */
void mvOpeningRoomEjectRoomGObjs(void)
{
    gcEjectGObjIfMade(sMVOpeningRoomOutsideGObj);
    gcEjectGObjIfMade(sMVOpeningRoomOutsideHazeGObj);
    gcEjectGObjIfMade(sMVOpeningRoomBooksGObj);
    gcEjectGObjIfMade(sMVOpeningRoomPencilsGObj);
    gcEjectGObjIfMade(sMVOpeningRoomLampGObj);
    gcEjectGObjIfMade(sMVOpeningRoomTissuesGObj);
}

/* mvopeningroom.c:696-709 mvOpeningRoomInitScene1Cameras, verbatim:
 * `lbRelocGetFileData(AObjEvent32*, sMVOpeningRoomFiles[2],
 * &llMVOpeningRoomScene1CamAnimJoint)` is camanim_get(bank, "Scene1"),
 * which is the same script out of the same relocData file 56 reached by
 * name instead of by linker offset (src/dc/camanim.h). This shot sets no
 * eye/at/up in the C source at all -- the script is the only thing that
 * ever frames it. */
void mvOpeningRoomInitScene1Cameras(GObj *gobj)
{
    CObj *cobj = CObjGetStruct(gobj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->projection.persp.near = 80.0F;
    cobj->projection.persp.far = 15000.0F;

    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningRoomCamAnimBank, "Scene1"), 0.0F);
    gcAddGObjProcess(gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);

    cobj->flags |= COBJ_FLAG_DLBUFFERS;
}

/* DIVERGES (port only): the room's camera of scenes 1 to 3. While the
 * close-up dimmer is up it draws the room as the dimmer would leave it,
 * every colour scaled by 1 - alpha (fighter.h fighter_set_dim), since
 * the dimmer itself draws nothing here -- see
 * mvOpeningRoomCloseUpOverlayProcDisplay. */
static void mvOpeningRoomMainCameraProcDisplay(GObj *camera_gobj)
{
#ifndef FT_HOSTTEST
    if (sMVOpeningRoomCloseUpIsDim)
    {
        fighter_set_dim((u32)(0xFF - sMVOpeningRoomOverlayAlpha) * 256 / 0xFF);
    }
#endif
    func_80017EC0(camera_gobj);
#ifndef FT_HOSTTEST
    fighter_set_dim(256);
#endif
}

/* mvopeningroom.c:712-753 mvOpeningRoomMakeScene1Cameras, verbatim: the
 * opening shot's two cameras, the wide one over DL link 6 and the
 * fighter one over links 27 and 9. The wide camera's second XObj is
 * matrix kind 14 -- one of objman.c:1164's own 6-17 look-at family, the
 * same family kind 8 belongs to (src/dc/objdisplay.c carries both
 * cases). */
void mvOpeningRoomMakeScene1Cameras(void)
{
    GObj *gobj;

    sMVOpeningRoomMainCameraGObj = gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        mvOpeningRoomMainCameraProcDisplay,
        80,
        COBJ_MASK_DLLINK(6),
        ~0,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    gcAddXObjForCamera(CObjGetStruct(gobj), nGCMatrixKindPerspFastF, 0);
    gcAddXObjForCamera(CObjGetStruct(gobj), 14, 0);
    mvOpeningRoomInitScene1Cameras(gobj);

    sMVOpeningRoomFighterCameraGObj = gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        40,
        COBJ_MASK_DLLINK(27) | COBJ_MASK_DLLINK(9),
        ~0,
        TRUE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    mvOpeningRoomInitScene1Cameras(gobj);
}

/* mvopeningroom.c:756-765 mvOpeningRoomInitScene2Cameras, verbatim, the
 * same substitution scene 1's takes, out of file 57. This one has no
 * near/far of its own either, so the script is the whole shot. */
void mvOpeningRoomInitScene2Cameras(GObj *gobj)
{
    CObj *cobj = CObjGetStruct(gobj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningRoomCamAnimBank, "Scene2"), 0.0F);
    gcAddGObjProcess(gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);

    cobj->flags |= COBJ_FLAG_DLBUFFERS;
}

/* mvopeningroom.c:768-809 mvOpeningRoomMakeScene2Cameras, verbatim. */
void mvOpeningRoomMakeScene2Cameras(void)
{
    GObj *gobj;

    sMVOpeningRoomMainCameraGObj = gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        mvOpeningRoomMainCameraProcDisplay,
        80,
        COBJ_MASK_DLLINK(6),
        ~0,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    gcAddXObjForCamera(CObjGetStruct(gobj), nGCMatrixKindPerspFastF, 0);
    gcAddXObjForCamera(CObjGetStruct(gobj), 8, 0);
    mvOpeningRoomInitScene2Cameras(gobj);

    sMVOpeningRoomFighterCameraGObj = gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        40,
        COBJ_MASK_DLLINK(27) | COBJ_MASK_DLLINK(9),
        ~0,
        TRUE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    mvOpeningRoomInitScene2Cameras(gobj);
}

/* mvopeningroom.c:812-834 mvOpeningRoomInitScene3Cameras, verbatim.
 * Every eye/at/up and the whole projection are literals in the C source
 * -- the script (file 58) drifts the camera away from that opening
 * framing rather than supplying it. */
void mvOpeningRoomInitScene3Cameras(GObj *gobj)
{
    CObj *cobj = CObjGetStruct(gobj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->vec.eye.x = 9.2993F;
    cobj->vec.eye.y = 3880.389404F;
    cobj->vec.eye.z = 4077.981689F;
    cobj->vec.at.x = 0.9915789962F;
    cobj->vec.at.y = 2995.681396F;
    cobj->vec.at.z = -388.9534302F;
    cobj->vec.up.x = 0.0F;
    cobj->vec.up.y = 1.0F;
    cobj->vec.up.z = 0.0F;
    cobj->projection.persp.fovy = 18.60718727F;
    cobj->projection.persp.near = 128.0F;
    cobj->projection.persp.far = 16384.0F;

    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningRoomCamAnimBank, "Scene3"), 0.0F);
    gcAddGObjProcess(gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);

    cobj->flags |= COBJ_FLAG_DLBUFFERS;
}

/* mvopeningroom.c:837-879 mvOpeningRoomMakeScene3Cameras, verbatim. */
void mvOpeningRoomMakeScene3Cameras(void)
{
    GObj *main_camera_gobj;
    GObj *fighter_camera_gobj;

    sMVOpeningRoomMainCameraGObj = main_camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        mvOpeningRoomMainCameraProcDisplay,
        80,
        COBJ_MASK_DLLINK(6),
        ~0,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    gcAddXObjForCamera(CObjGetStruct(main_camera_gobj), nGCMatrixKindPerspFastF, 0);
    gcAddXObjForCamera(CObjGetStruct(main_camera_gobj), 8, 0);
    mvOpeningRoomInitScene3Cameras(main_camera_gobj);

    sMVOpeningRoomFighterCameraGObj = fighter_camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        40,
        COBJ_MASK_DLLINK(27) | COBJ_MASK_DLLINK(9),
        ~0,
        TRUE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    mvOpeningRoomInitScene3Cameras(fighter_camera_gobj);
}

/* mvopeningroom.c:882-904 mvOpeningRoomInitScene4Cameras, verbatim, and
 * framed by literals the way scene 3's is. Note this one never sets
 * COBJ_FLAG_DLBUFFERS -- that is the decomp's, not an omission here. */
void mvOpeningRoomInitScene4Cameras(GObj *gobj)
{
    CObj *cobj = CObjGetStruct(gobj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->vec.eye.x = -1039.880615F;
    cobj->vec.eye.y = 3199.215576F;
    cobj->vec.eye.z = -1235.168823F;
    cobj->vec.at.x = -1162.40979F;
    cobj->vec.at.y = 2127.824463F;
    cobj->vec.at.z = -3853.073242F;
    cobj->vec.up.x = 0.0F;
    cobj->vec.up.y = 1.0F;
    cobj->vec.up.z = 0.0F;

    cobj->projection.persp.fovy = 11.98226547F;
    cobj->projection.persp.near = 128.0F;
    cobj->projection.persp.far = 16384.0F;

    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningRoomCamAnimBank, "Scene4"), 0.0F);
    gcAddGObjProcess(gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
}

/* mvopeningroom.c:907-947 mvOpeningRoomMakeScene4Cameras, verbatim:
 * the final framing, whose wide camera takes no matrix XObj at all. */
void mvOpeningRoomMakeScene4Cameras(void)
{
    GObj *main_camera_gobj;
    GObj *fighter_camera_gobj;

    sMVOpeningRoomMainCameraGObj = main_camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        80,
        COBJ_MASK_DLLINK(6),
        ~0,
        TRUE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    mvOpeningRoomInitScene4Cameras(main_camera_gobj);

    sMVOpeningRoomFighterCameraGObj = fighter_camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        40,
        COBJ_MASK_DLLINK(9),
        ~0,
        TRUE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    mvOpeningRoomInitScene4Cameras(fighter_camera_gobj);
}

/* mvopeningroom.c:950-972 mvOpeningRoomMakeWallpaperCamera: the sprite
 * camera the wallpaper draws under, DL link 28. DIVERGES:
 * lbCommonDrawSpriteBackdrop for lbCommonDrawSprite -- the wallpaper
 * proc above pins its sprite to the far end of the Z buffer
 * (gDPSetPrimDepth, G_ZS_PRIM) so the logo camera's model (95) shows
 * over it; the backdrop band is the port's far end
 * (src/dc/lbcommon.h). */
void mvOpeningRoomMakeWallpaperCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSpriteBackdrop,
            90,
            COBJ_MASK_DLLINK(28),
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

/* mvopeningroom.c:975-1001 mvOpeningRoomMakeLogoCamera, verbatim: the
 * camera-anim script is file 56's, the same one scene 1 plays, so it
 * takes the same "Scene1" entry of the bank. This is the FIFTH
 * camera-anim call in this file, the one that does not live in an
 * Init*Cameras. It moves over the logo it frames on DL link 29. */
void mvOpeningRoomMakeLogoCamera(void)
{
    GObj *gobj;
    CObj *cobj;

    sMVOpeningRoomLogoCameraGObj = gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        50,
        COBJ_MASK_DLLINK(29),
        ~0,
        TRUE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    cobj = CObjGetStruct(gobj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningRoomCamAnimBank, "Scene1"), 0.0F);
    gcAddGObjProcess(gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
}

/* ---- the tic-1040 star wipe (mvopeningroom.c:1004-1064)
 *
 * What the N64 does. The transition camera (priority 95, so walked
 * before every other camera the scene has) draws two GObjs, both made
 * out of relocData 63 and both scaled 0.05 -> 1.0 over 40 tics by their
 * AnimJoints:
 *
 *   Outline: points the RDP's colour image at the Z BUFFER and fills it
 *     with 0 -- nearest, so every later Z-tested pixel of the frame
 *     fails -- then points it back at the frame and draws the outer
 *     star's triangles, red by vertex colour, with no Z test.
 *   Overlay: points the colour image at the Z buffer again and draws the
 *     inner star in white, 0xFFFF -- farthest -- so inside it, and only
 *     inside it, the new scene's geometry passes.
 *
 * Nothing clears the colour buffer (tic 1037 drops the room camera's
 * FILLCOLOR, and CheckSetFramebuffer keeps the three buffers turning),
 * so outside the red star the screen holds the room's last frame, the
 * red star grows over it, and the desk and wallpaper show through the
 * inner star as it opens.
 *
 * DIVERGES, the PVR way. There is no colour-image redirect and no
 * mid-frame Z write without a polygon, but the opaque list is taken in
 * submission order per tile with each polygon's own depth compare, and
 * this camera's pass submits first. So:
 *
 *   Outline: the room's last frame -- the frame loop's photo of tic
 *     1039 (taskman.h), as the post-battle wipe takes -- as one viewport quad,
 *     then the outer star, both depth ALWAYS at the frame border's own
 *     10000 (taskman.c syTaskmanDrawBorder), nearer than any sprite:
 *     the "Z = 0" fill and the red draw in one.
 *   Overlay: the inner star, depth ALWAYS at just over the background
 *     plane (main.c pvr_set_zclip) -- the "Z = 0xFFFF" draw -- black,
 *     where nothing of the new scene lands.
 *
 * Every later camera tests GEQUAL against those, as the N64's tests
 * against its Z, so the result is the same frame. The stars come from
 * mvroomwipe.bin (tools/export/ssb_roomwipeexport.py: file 63's two flat
 * display lists as bare triangles, all at z 0), projected here through
 * the running camera with the DObj's animated scale -- the one matrix
 * gcDrawDObjDLHead0 would have loaded. */
#define MVOPENINGROOM_WIPE_FILE     "mvroomwipe.bin"
#define MVOPENINGROOM_WIPE_Z_NEAR   10000.0F
#define MVOPENINGROOM_WIPE_Z_FAR    1.5e-6F

typedef struct MVOpeningRoomWipeCorner
{
    f32 x, y;
    u32 argb;

} MVOpeningRoomWipeCorner;

static MVOpeningRoomWipeCorner *sMVOpeningRoomWipeOverlayTris;
static MVOpeningRoomWipeCorner *sMVOpeningRoomWipeOutlineTris;
static s32 sMVOpeningRoomWipeOverlayCount;
static s32 sMVOpeningRoomWipeOutlineCount;
/* the frame loop's photo (taskman.h) of tic 1039, and its window */
static void *sMVOpeningRoomWipePhoto;
static sb32 sMVOpeningRoomWipeHasPhoto;
static f32 sMVOpeningRoomWipePhotoWin[4];

/* relocData 63's two AnimJoints, @ 0x714 and 0x11C4: SetVal0RateBlock
 * SCAXYZ 0.05 at frame 0, 1.0 at frame 40, End -- the decomp's own
 * words (src/relocData/63_MVOpeningRoomTransition.c), the scstaffroll.c
 * way. The two are identical. */
#ifndef FT_HOSTTEST
static u32 dMVOpeningRoomTransitionOverlayAnimJoint[9] =
{
    aobjEvent32SetVal0RateBlock(AOBJ_FLAG_SCAXYZ, 0),
        0x3D4CCCCD, 0x3D4CCCCD, 0x3D4CCCCD,     /* 0.05F */
    aobjEvent32SetVal0RateBlock(AOBJ_FLAG_SCAXYZ, 40),
        0x3F800000, 0x3F800000, 0x3F800000,     /* 1.0F */
    aobjEvent32End()
};
static u32 dMVOpeningRoomTransitionOutlineAnimJoint[9] =
{
    aobjEvent32SetVal0RateBlock(AOBJ_FLAG_SCAXYZ, 0),
        0x3D4CCCCD, 0x3D4CCCCD, 0x3D4CCCCD,     /* 0.05F */
    aobjEvent32SetVal0RateBlock(AOBJ_FLAG_SCAXYZ, 40),
        0x3F800000, 0x3F800000, 0x3F800000,     /* 1.0F */
    aobjEvent32End()
};
#endif

/* mvroomwipe.bin into the scene heap, where it goes back with the rest
 * of the scene. FALSE and a log line if it is not on the disc: the
 * GObjs are still made and animate, and simply draw nothing. */
sb32 mvOpeningRoomLoadWipe(void)
{
    long size;
    u8 *blob = asset_read_whole(MVOPENINGROOM_WIPE_FILE, &size);
    u32 hd[4];
    long need;

    if (blob == NULL || size < (long)sizeof(hd))
    {
        syDebugPrintf("mvOpeningRoom: %s is not in the romdisk\n", MVOPENINGROOM_WIPE_FILE);
        free(blob);
        return FALSE;
    }
    memcpy(hd, blob, sizeof(hd));
    need = (long)sizeof(hd) + (long)(hd[2] + hd[3]) * 3 * (long)sizeof(MVOpeningRoomWipeCorner);

    if (memcmp(blob, "RWIP", 4) != 0 || hd[1] != 1 || size != need)
    {
        syDebugPrintf("mvOpeningRoom: %s is not an RWIP v1 file\n", MVOPENINGROOM_WIPE_FILE);
        free(blob);
        return FALSE;
    }
    sMVOpeningRoomWipeOverlayCount = (s32)hd[2];
    sMVOpeningRoomWipeOutlineCount = (s32)hd[3];
    sMVOpeningRoomWipeOverlayTris = syTaskmanMalloc(need - sizeof(hd), 0x4);
    memcpy(sMVOpeningRoomWipeOverlayTris, blob + sizeof(hd), need - sizeof(hd));
    sMVOpeningRoomWipeOutlineTris = sMVOpeningRoomWipeOverlayTris + hd[2] * 3;
    free(blob);

    return TRUE;
}

/* See mvopeningroom.h: the host test's view of what the loader read. */
const f32 *mvOpeningRoomGetWipeTris(s32 outline, s32 *count, u32 *argb0)
{
    const MVOpeningRoomWipeCorner *tris = (outline) ? sMVOpeningRoomWipeOutlineTris : sMVOpeningRoomWipeOverlayTris;

    *count = (outline) ? sMVOpeningRoomWipeOutlineCount : sMVOpeningRoomWipeOverlayCount;
    *argb0 = (tris != NULL) ? tris[0].argb : 0;

    return (const f32*)tris;
}

#ifndef FT_HOSTTEST
/* The running camera's matrix and viewport, the DObj's scale, and each
 * triangle as one strip of three, into whatever header is current. */
static void mvOpeningRoomDrawWipeTris(GObj *gobj, const MVOpeningRoomWipeCorner *tris, s32 count, f32 z, u32 argb_mask)
{
    DObj *dobj = DObjGetStruct(gobj);
    f32 sx = dobj->scale.vec.f.x;
    f32 sy = dobj->scale.vec.f.y;
    float m[16];
    pvr_vertex_t v;
    s32 i;

    gcCameraMatrixF(CObjGetStruct(gGCCurrentCamera), m);

    v.oargb = 0;
    v.u = v.v = 0.0F;
    v.z = z;

    for (i = 0; i < count * 3; i++)
    {
        float cx, cy, cz, cw;

        mtx_apply(m, tris[i].x * sx, tris[i].y * sy, 0.0F, &cx, &cy, &cz, &cw);

        v.flags = ((i % 3) == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = gDCViewport.cx + (cx / cw) * gDCViewport.hw;
        v.y = gDCViewport.cy - (cy / cw) * gDCViewport.hh;
        v.argb = tris[i].argb & argb_mask;
        pvr_prim(&v, sizeof(v));
    }
}

static void mvOpeningRoomWipeColHeader(void)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;

    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));
}
#endif

/* mvopeningroom.c:1004-1041 mvOpeningRoomTransitionOverlayProcDisplay:
 * the inner star, written far -- see the wipe comment above. */
void mvOpeningRoomTransitionOverlayProcDisplay(GObj *gobj)
{
#ifndef FT_HOSTTEST
    if ((gcGetDrawList() != PVR_LIST_OP_POLY) || (sMVOpeningRoomWipeOverlayTris == NULL))
    {
        return;
    }
    mvOpeningRoomWipeColHeader();
    mvOpeningRoomDrawWipeTris(gobj, sMVOpeningRoomWipeOverlayTris, sMVOpeningRoomWipeOverlayCount, MVOPENINGROOM_WIPE_Z_FAR, 0xFF000000);
#else
    (void)gobj;
#endif
}

/* mvopeningroom.c:1015-1041 mvOpeningRoomTransitionOutlineProcDisplay:
 * the room's frozen last frame and the red outer star, written near. */
void mvOpeningRoomTransitionOutlineProcDisplay(GObj *gobj)
{
#ifndef FT_HOSTTEST
    if ((gcGetDrawList() != PVR_LIST_OP_POLY) || (sMVOpeningRoomWipeOutlineTris == NULL))
    {
        return;
    }
    if (sMVOpeningRoomWipeHasPhoto)
    {
        pvr_poly_cxt_t cxt;
        pvr_poly_hdr_t hdr;
        pvr_vertex_t v;
        s32 i;

        const f32 *win = sMVOpeningRoomWipePhotoWin;

        pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY,
                         PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_X32_STRIDE,
                         SY_TASKMAN_PHOTO_TEXW, SY_TASKMAN_PHOTO_TEXH, sMVOpeningRoomWipePhoto, PVR_FILTER_BILINEAR);
        cxt.gen.culling = PVR_CULLING_NONE;
        cxt.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
        pvr_poly_compile(&hdr, &cxt);
        pvr_prim(&hdr, sizeof(hdr));

        /* the photo's middle 300x220 is the viewport's, the camera's own */
        v.oargb = 0;
        v.argb = 0xFFFFFFFF;
        v.z = MVOPENINGROOM_WIPE_Z_NEAR;

        for (i = 0; i < 4; i++)
        {
            v.flags = (i == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v.x = (i & 2) ? (gDCViewport.cx + gDCViewport.hw) : (gDCViewport.cx - gDCViewport.hw);
            v.y = (i & 1) ? (gDCViewport.cy - gDCViewport.hh) : (gDCViewport.cy + gDCViewport.hh);
            v.u = win[0] + ((i & 2) ? win[2] : 0.0F);
            v.v = win[1] + ((i & 1) ? 0.0F : win[3]);
            pvr_prim(&v, sizeof(v));
        }
    }
    mvOpeningRoomWipeColHeader();
    mvOpeningRoomDrawWipeTris(gobj, sMVOpeningRoomWipeOutlineTris, sMVOpeningRoomWipeOutlineCount, MVOPENINGROOM_WIPE_Z_NEAR, 0xFFFFFFFF);
#else
    (void)gobj;
#endif
}

/* mvopeningroom.c:1044-1064 mvOpeningRoomMakeTransition. The decomp's
 * two GObjs, DObjs, XObjs, display procs and AnimJoints, line for line;
 * the DObj carries no display list (the procs draw the baked stars),
 * the scripts are the embedded words above, and the port's two
 * additions are the stars, read off the disc at the scene's start, and
 * the photo of the frame the room last drew. */
void mvOpeningRoomMakeTransition(void)
{
    GObj *gobj;
    DObj *dobj;

    /* the stars were read at the scene's start (mvOpeningRoomFuncStart) */

    /* tic 1039's frame, which the frame loop drew into the photo at the
     * scene's request (mvOpeningRoomFuncRun) */
    {
        s32 w, h;

        sMVOpeningRoomWipePhoto = syTaskmanGetPhoto(&w, &h);
        sMVOpeningRoomWipeHasPhoto = (sMVOpeningRoomWipePhoto != NULL);

        if (sMVOpeningRoomWipeHasPhoto)
        {
            syTaskmanPhotoWindow(w, h, sMVOpeningRoomWipePhotoWin);
        }
        else syDebugPrintf("mvOpeningRoom: no photo for the star wipe\n");
    }

    sMVOpeningTransitionOutlineGObj = gobj = gcMakeGObjSPAfter(0, NULL, 22, GOBJ_PRIORITY_DEFAULT);
    dobj = gcAddDObjForGObj(gobj, NULL);
    gcAddXObjForDObjFixed(dobj, nGCMatrixKindTraRotRpyRSca, 0);
    gcAddGObjDisplay(gobj, mvOpeningRoomTransitionOutlineProcDisplay, 30, GOBJ_PRIORITY_DEFAULT, ~0);
#ifndef FT_HOSTTEST
    /* not on the host: AObjEvent32 is eight bytes there (camanim.h) */
    gcAddDObjAnimJoint(dobj, (AObjEvent32*)dMVOpeningRoomTransitionOutlineAnimJoint, 0.0F);
#endif
    gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
    gcPlayAnimAll(gobj);

    sMVOpeningTransitionOverlayGObj = gobj = gcMakeGObjSPAfter(0, NULL, 22, GOBJ_PRIORITY_DEFAULT);
    dobj = gcAddDObjForGObj(gobj, NULL);
    gcAddXObjForDObjFixed(dobj, nGCMatrixKindTraRotRpyRSca, 0);
    gcAddGObjDisplay(gobj, mvOpeningRoomTransitionOverlayProcDisplay, 30, GOBJ_PRIORITY_DEFAULT, ~0);
#ifndef FT_HOSTTEST
    /* not on the host: AObjEvent32 is eight bytes there (camanim.h) */
    gcAddDObjAnimJoint(dobj, (AObjEvent32*)dMVOpeningRoomTransitionOverlayAnimJoint, 0.0F);
#endif
    gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
    gcPlayAnimAll(gobj);
}

/* The photo back to the frame loop, once nothing can draw it: at the 18 s
 * eject and, if the scene is left before then, at the scene's end. */
static void mvOpeningRoomFreeWipePhoto(void)
{
    if (sMVOpeningRoomWipePhoto != NULL)
    {
        syTaskmanReleasePhoto();
        sMVOpeningRoomWipePhoto = NULL;
        sMVOpeningRoomWipeHasPhoto = FALSE;
    }
}

/* mvopeningroom.c:1067-1102 mvOpeningRoomMakeTransitionCamera,
 * verbatim: every value it sets is a literal, so this camera is whole
 * even though what it framed is not. */
void mvOpeningRoomMakeTransitionCamera(void)
{
    GObj *gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        95,
        COBJ_MASK_DLLINK(30),
        ~0,
        TRUE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(gobj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->vec.eye.x = 0.0F;
    cobj->vec.eye.y = 0.0F;
    cobj->vec.eye.z = 1000.0F;
    cobj->vec.at.x = 0.0F;
    cobj->vec.at.y = 0.0F;
    cobj->vec.at.z = 0.0F;
    cobj->vec.up.x = 0.0F;
    cobj->vec.up.y = 1.0F;
    cobj->vec.up.z = 0.0F;

    cobj->projection.persp.fovy = 39.56115341F;
    cobj->projection.persp.near = 128.0F;
    cobj->projection.persp.far = 16384.0F;
}

/* mvopeningroom.c:1105-1109 mvOpeningRoomEjectCameraGObjs, verbatim. */
void mvOpeningRoomEjectCameraGObjs(void)
{
    /* both are always real -- the cameras are ported -- but they go
     * through the guard too, so no eject in this file can ever be the
     * "eject me" signal by accident (see mvOpeningRoomEjectRoomGObjs) */
    gcEjectGObjIfMade(sMVOpeningRoomMainCameraGObj);
    gcEjectGObjIfMade(sMVOpeningRoomFighterCameraGObj);
}

/* mvopeningroom.c:1112-1131 mvOpeningRoomGetDroppedFighterKind,
 * verbatim: one of the eight originals, re-drawn until it differs from
 * the plucked one. syUtilsRandTimeUCharRange is genuinely wall-clock
 * seeded here, as it is everywhere else the port uses it (the
 * mncharacters.c flake is the same function). */
s32 mvOpeningRoomGetDroppedFighterKind(void)
{
    s32 fkinds[] =
    {
        nFTKindMario,
        nFTKindFox,
        nFTKindDonkey,
        nFTKindSamus,
        nFTKindLink,
        nFTKindYoshi,
        nFTKindKirby,
        nFTKindPikachu
    };

    s32 fkind;

    while (fkind = fkinds[syUtilsRandTimeUCharRange(ARRAY_COUNT(fkinds))], fkind == sMVOpeningRoomPulledFighterKind);

    return fkind;
}

/* mvopeningroom.c:1134-1149 mvOpeningRoomGetPulledFighterKind, verbatim. */
s32 mvOpeningRoomGetPulledFighterKind(void)
{
    s32 fkinds[] =
    {
        nFTKindMario,
        nFTKindFox,
        nFTKindDonkey,
        nFTKindSamus,
        nFTKindLink,
        nFTKindYoshi,
        nFTKindKirby,
        nFTKindPikachu
    };

#ifdef DB_ROOM_FKIND
    /* src/dc/db.h: the fighter to pull, not a random one */
    (void)fkinds;
    return DB_ROOM_FKIND;
#endif
    return fkinds[syUtilsRandTimeUCharRange(ARRAY_COUNT(fkinds))];
}

/* mvopeningroom.c:1152-1157 mvOpeningRoomInitVars, verbatim. */
void mvOpeningRoomInitVars(void)
{
    sMVOpeningRoomTotalTimeTics = 0;
    sMVOpeningRoomPulledFighterKind = mvOpeningRoomGetPulledFighterKind();
    sMVOpeningRoomDroppedFighterKind = mvOpeningRoomGetDroppedFighterKind();
}

/* mvopeningroom.c:1160-1195 mvOpeningRoomCheckSetFramebuffer. Not
 * ported -- see mvopeningroom.h's DIVERGES note: it is N64 triple-buffer
 * rotation over gSYFramebufferSets[], and syTaskmanSetFuncSwapBuffer
 * does not exist port-side. Both of its install/teardown calls
 * (FuncStart, StartScene) are cut with it. */

/* mvopeningroom.c:1198-1304 mvOpeningRoomFuncRun. Verbatim, including
 * every tic constant and the A/B/START skip to the title, but for two
 * guards: the tic-560 and tic-860 scSubsysFighterSetStatus calls are
 * the only reads of sMVOpeningRoomBossGObj that would dereference it.
 * The guards are
 * needed because ftManagerMakeFighter still answers NULL for a
 * pack that fails to load, which on this port is a disc read and not a
 * certainty. The
 * gcEjectGObj calls on it and on the cut props' GObjs all go through
 * gcEjectGObjIfMade, because gcEjectGObj(NULL) is NOT a no-op: a NULL
 * or self argument sets sGCRunStatusEject and the runner then ejects
 * the GObj that is running (objman.c:1780-1786, 2132-2141), so a
 * verbatim eject on a cut prop would kill this driver from inside
 * itself -- silently, with no crash and no log line.
 *
 * The scene's arc: tic 280 plucks a trophy, 380 drops it, 450 veils the
 * close-up, 500 moves it onto the spotlight's DL link, 560 and 860 cut
 * to the scene-2 and scene-3 framings, 695 drops the second trophy,
 * 1040 tears the room down for the wipe, and 19s/18s/22s finish the
 * close-up and hand off. */
void mvOpeningRoomFuncRun(GObj *gobj)
{
    sMVOpeningRoomTotalTimeTics++;

    if (sMVOpeningRoomTotalTimeTics >= 10)
    {
        if (sMVOpeningRoomUnused0x80134D54 != 0)
        {
            sMVOpeningRoomUnused0x80134D54--;
        }
        if ((scSubsysControllerGetPlayerStickInRangeLR(-15, 15)) && (scSubsysControllerGetPlayerStickInRangeUD(-15, 15)))
        {
            sMVOpeningRoomUnused0x80134D54 = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON))
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            syTaskmanSetLoadScene();
        }
        if (sMVOpeningRoomTotalTimeTics == 280)
        {
            mvOpeningRoomMakePulledFighter(sMVOpeningRoomPulledFighterKind);
            mvOpeningRoomMakePencils();

            gcEjectGObjIfMade(sMVOpeningRoomLogoGObj);
            gcEjectGObjIfMade(sMVOpeningRoomOverlayGObj);
            gcEjectGObjIfMade(sMVOpeningRoomBossShadowGObj);
        }
        if (sMVOpeningRoomTotalTimeTics == 695)
        {
            mvOpeningRoomMakeDroppedFighter(sMVOpeningRoomDroppedFighterKind);
        }
        if (sMVOpeningRoomTotalTimeTics == 380)
        {
            scSubsysFighterSetStatus(sMVOpeningRoomPulledFighterGObj, nFTDemoStatusFigureDropped);

            DObjGetStruct(sMVOpeningRoomPulledFighterGObj)->rotate.vec.f.x = 0.0F;
            DObjGetStruct(sMVOpeningRoomPulledFighterGObj)->rotate.vec.f.y = 0.0F;
            DObjGetStruct(sMVOpeningRoomPulledFighterGObj)->rotate.vec.f.z = 0.0F;
        }
        if (sMVOpeningRoomTotalTimeTics == 450)
        {
            mvOpeningRoomMakeCloseUpOverlay();
            gcEjectGObjIfMade(sMVOpeningRoomSunlightGObj);
        }
        if (sMVOpeningRoomTotalTimeTics == 560)
        {
            mvOpeningRoomEjectCameraGObjs();
            mvOpeningRoomMakeScene2Cameras();

            if (sMVOpeningRoomBossGObj != NULL)
            {
                scSubsysFighterSetStatus(sMVOpeningRoomBossGObj, 0x10010);
            }
        }
        if (sMVOpeningRoomTotalTimeTics == 500)
        {
            gcMoveGObjDL(sMVOpeningRoomPulledFighterGObj, 9, -1);
            mvOpeningRoomMakeSpotlight();
        }
        if (sMVOpeningRoomTotalTimeTics == 860)
        {
            mvOpeningRoomEjectCameraGObjs();
            mvOpeningRoomMakeScene3Cameras();

            if (sMVOpeningRoomBossGObj != NULL)
            {
                scSubsysFighterSetStatus(sMVOpeningRoomBossGObj, 0x10011);
            }
            mvOpeningRoomMakeSnap();
        }
        if (sMVOpeningRoomTotalTimeTics == 1037)
        {
            /* Verbatim, and on this port a no-op: mvOpeningRoomFuncStart
             * never sets the bit, and says why. */
            CObjGetStruct(sMVOpeningRoomCameraGObj)->flags &= ~COBJ_FLAG_FILLCOLOR;
        }
        if (sMVOpeningRoomTotalTimeTics == 1039)
        {
            /* DIVERGES (port only): the star wipe's picture is this
             * tic's frame, which the N64 finds in its framebuffer at
             * 1040. Here the frame loop draws it into a texture instead
             * of onto the screen (taskman.h syTaskmanWantPhoto). */
            syTaskmanWantPhoto();
        }
        if (sMVOpeningRoomTotalTimeTics == 1040)
        {
            gcEjectGObjIfMade(sMVOpeningRoomOverlayGObj);
            sMVOpeningRoomCloseUpIsDim = FALSE;
            gcEjectGObjIfMade(sMVOpeningRoomSpotlightGObj);
            gcEjectGObjIfMade(sMVOpeningRoomBossGObj);

            mvOpeningRoomMakeTransitionCamera();
            mvOpeningRoomMakeTransition();
            mvOpeningRoomEjectRoomGObjs();
            mvOpeningRoomMakeDeskGround();
            mvOpeningRoomMakeWallpaper();
        }
        if (sMVOpeningRoomTotalTimeTics == I_SEC_TO_TICS(19))
        {
            mvOpeningRoomEjectCameraGObjs();
            mvOpeningRoomMakeCloseUpEffect();

            DObjGetStruct(sMVOpeningRoomPulledFighterGObj)->rotate.vec.f.x = 0.0F;
            DObjGetStruct(sMVOpeningRoomPulledFighterGObj)->rotate.vec.f.y = 0.0F;
            DObjGetStruct(sMVOpeningRoomPulledFighterGObj)->rotate.vec.f.z = 0.0F;

            scSubsysFighterSetStatus(sMVOpeningRoomPulledFighterGObj, nFTDemoStatusFigureStand);
            mvOpeningRoomMakeScene4Cameras();
        }
        if (sMVOpeningRoomTotalTimeTics == I_SEC_TO_TICS(18))
        {
            gcEjectGObjIfMade(sMVOpeningTransitionOverlayGObj);
            gcEjectGObjIfMade(sMVOpeningTransitionOutlineGObj);

            mvOpeningRoomFreeWipePhoto();
        }
        if (sMVOpeningRoomTotalTimeTics == I_SEC_TO_TICS(22))
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindOpeningPortraits;

            syTaskmanSetLoadScene();
        }
    }
}

/* mvopeningroom.c:1307-1361 mvOpeningRoomFuncStart. DIVERGES, all named
 * in mvopeningroom.h: the swap-buffer hook, the reloc loader (replaced
 * by the one file-90 sprite bank), the three figatree-heap mallocs and
 * gSCManagerUnkown0x800A50F0 are cut; everything else runs in the
 * decomp's own order, including the verbatim
 * ftManagerSetupFilesAllKind(nFTKindBoss) (a self-guarding no-op for a
 * kind with no pack) and the verbatim ftManagerAllocFighter for three
 * fighters, of which two are ever made.
 *
 * Plus one word: the clear camera below keeps its GObj and loses
 * COBJ_FLAG_FILLCOLOR, which is src/dc/scstaffroll.c's divergence and
 * the same fault. src/dc/objdisplay.c's gcPrepCameraViewport says what
 * the flag does here -- a quad at the frame's next sprite depth in the
 * translucent list, and every sprite depth is in front of every opaque
 * depth -- so this camera, which runs before the scene's own, painted an
 * opaque black rectangle over the whole viewport and over everything the
 * opaque and punch-through lists had drawn. The fighters survived it
 * because their camera draws later still, in the same list; the desk,
 * the books, the pencils and the lamp did not, so props submitted real,
 * on-screen, correctly-lit triangles into a list that rendered a black
 * screen.
 *
 * The GObj stays because mvOpeningRoomFuncRun reaches it at tic 1037 to
 * clear the flag -- which is now a clear of a bit that was never set,
 * left verbatim rather than cut so the tic keeps its line in the cut
 * list. The colour is the PVR's own clear. */
void mvOpeningRoomFuncStart(void)
{
    if (sprite_bank_load(&sMVOpeningRoomWallpaperBank, "mvopeningroomwallpaper.spr") < 0)
    {
        sMVOpeningRoomWallpaperFileHead = NULL;
    }
    else
    {
        sMVOpeningRoomWallpaperFileHead = &sMVOpeningRoomWallpaperBank;
    }
    /* The other half of what the cut lbRelocLoadFilesListed would have
     * loaded: files 56-59's camera scripts. A failure here
     * is not fatal -- camanim_get then answers NULL for every name, and
     * gcAddCObjCamAnimJoint(NULL) leaves the camera exactly where the
     * scene's own literals put it. */
    camanim_bank_load(&sMVOpeningRoomCamAnimBank, "mvopeningroom.cam");
    /* The star wipe's triangles, for tic 1040: read here with the rest of
     * the scene rather than mid-movie. They
     * live in the scene heap, which lasts until the scene ends. */
    mvOpeningRoomLoadWipe();
    gcMakeGObjSPAfter(0, mvOpeningRoomFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
    sMVOpeningRoomCameraGObj = gcMakeDefaultCameraGObj(0, GOBJ_PRIORITY_DEFAULT, 100, COBJ_FLAG_ZBUFFER, GPACK_RGBA8888(0x00, 0x00, 0x00, 0xFF));
    efParticleInitAll();
    mvOpeningRoomInitVars();
    efManagerInitEffects();
    ftManagerAllocFighter(FTDATA_FLAG_SUBMOTION, 3);
    ftManagerSetupFilesAllKind(sMVOpeningRoomPulledFighterKind);
    ftManagerSetupFilesAllKind(sMVOpeningRoomDroppedFighterKind);
    ftManagerSetupFilesAllKind(nFTKindBoss);

    mvOpeningRoomMakeScene1Cameras();
    mvOpeningRoomMakeCloseUpOverlayCamera();
    mvOpeningRoomMakeWallpaperCamera();
    mvOpeningRoomMakeLogoCamera();
    mvOpeningRoomMakeOutside();
    mvOpeningRoomMakeHaze();
    mvOpeningRoomMakeBackground();
    mvOpeningRoomMakeSunlight();
    mvOpeningRoomMakeDesk();
    mvOpeningRoomMakeLogoWallpaper();
    mvOpeningRoomMakeLogo();
    mvOpeningRoomMakeBooks();
    mvOpeningRoomMakeLamp();
    mvOpeningRoomMakeTissues();
    mvOpeningRoomMakeBoss();
    mvOpeningRoomMakeBossShadow();
    scSubsysFighterSetLightParams(45.0F, 45.0F, 0xFF, 0xFF, 0xFF, 0xFF);
    func_800266A0_272A0();
    /* DIVERGES: the fighters (Master Hand, the trophies) share the props'
     * projection, so they take the Z buffer, not the layered band. */
    dc_model_set_demo_opaque(TRUE);
    syAudioPlayBGM(0, nSYAudioBGMOpening);
    sySchedulerSetTicCount(0);
}

/* mvopeningroom.c:1412-1421 mvOpeningRoomStartScene. DIVERGES: the
 * SYVideoSetup, its zbuffer allocation and arena_size are cut the same
 * way every other scene's own StartScene documents -- one video mode,
 * set once at boot (src/dc/sysshim.c) -- so scManagerFuncUpdate runs
 * the task directly, the same substitution mvending.c/scexplain.c/
 * scautodemo.c already use, and syTaskmanSetFuncSwapBuffer(NULL) goes
 * with the hook it tears down. This scene sets scene_curr itself (the
 * skip to the title, and the tic-22s hand-off), so this does not set it
 * again either. gmRumbleInitPlayers is this port's own addition, the
 * same one every ported scene's StartScene makes. */
void mvOpeningRoomStartScene(void)
{
    dMVOpeningRoomTaskmanSetup.func_start = mvOpeningRoomFuncStart;

    scManagerFuncUpdate(&dMVOpeningRoomTaskmanSetup);

    /* the wipe's texture, if the scene was skipped mid-wipe */
    mvOpeningRoomFreeWipePhoto();

    gmRumbleInitPlayers();
}

/* mvopeningroom.c:1367-1409 dMVOpeningRoomTaskmanSetup -- see
 * mvopeningroom.h's header note and this file's own "the pools"
 * comment above. */
SYTaskmanSetup dMVOpeningRoomTaskmanSetup =
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
    MVOPENINGROOM_GOBJPROCS,
    MVOPENINGROOM_GOBJS,   sizeof(GObj),
    MVOPENINGROOM_XOBJS,
    /* the decomp names dLBCommonFuncMatrixList here; the port has no
     * such table (the matrix kinds are a switch in objdisplay.c, not a
     * function list), the same NULL every other ported scene's setup
     * carries */
    NULL,                               /* matrix function list */
    NULL,                               /* DObjVec eject function */
    MVOPENINGROOM_AOBJS,
    MVOPENINGROOM_MOBJS,
    MVOPENINGROOM_DOBJS,   sizeof(DObj),
    MVOPENINGROOM_SOBJS,   sizeof(SObj),
    MVOPENINGROOM_COBJS,   sizeof(CObj),

    mvOpeningRoomFuncStart              /* task start function */
};

/* The port's own bzero arm for dSCManagerOverlays[34] (src/dc/overlay.c),
 * the same role mvEndingOverlayLoad plays for overlay 54. */
void mvOpeningRoomOverlayLoad(void)
{
    OVERLAY_CLEAR(sMVOpeningRoomBossFigatreeHeap);
    OVERLAY_CLEAR(sMVOpeningRoomPluckedFigatreeHeap);
    OVERLAY_CLEAR(sMVOpeningRoomDroppedFigatreeHeap);
    OVERLAY_CLEAR(sMVOpeningRoomTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningRoomMainCameraGObj);
    OVERLAY_CLEAR(sMVOpeningRoomFighterCameraGObj);
    OVERLAY_CLEAR(sMVOpeningRoomLogoCameraGObj);
    OVERLAY_CLEAR(sMVOpeningRoomBossGObj);
    OVERLAY_CLEAR(sMVOpeningRoomPulledFighterKind);
    OVERLAY_CLEAR(sMVOpeningRoomDroppedFighterKind);
    OVERLAY_CLEAR(sMVOpeningRoomLogoGObj);
    OVERLAY_CLEAR(sMVOpeningRoomPulledFighterGObj);
    OVERLAY_CLEAR(sMVOpeningRoomDroppedFighterGObj);
    OVERLAY_CLEAR(sMVOpeningRoomGObj);
    OVERLAY_CLEAR(sMVOpeningRoomSunlightGObj);
    OVERLAY_CLEAR(sMVOpeningRoomDeskGObj);
    OVERLAY_CLEAR(sMVOpeningRoomOutsideGObj);
    OVERLAY_CLEAR(sMVOpeningRoomOutsideHazeGObj);
    OVERLAY_CLEAR(sMVOpeningRoomBooksGObj);
    OVERLAY_CLEAR(sMVOpeningRoomPencilsGObj);
    OVERLAY_CLEAR(sMVOpeningRoomLampGObj);
    OVERLAY_CLEAR(sMVOpeningRoomTissuesGObj);
    OVERLAY_CLEAR(sMVOpeningRoomBossShadowGObj);
    OVERLAY_CLEAR(sMVOpeningRoomOverlayAlpha);
    OVERLAY_CLEAR(sMVOpeningRoomOverlayGObj);
    OVERLAY_CLEAR(sMVOpeningRoomCloseUpIsDim);
    OVERLAY_CLEAR(sMVOpeningRoomSpotlightGObj);
    OVERLAY_CLEAR(sMVOpeningRoomBackgroundGObj);
    OVERLAY_CLEAR(sMVOpeningTransitionOutlineGObj);
    OVERLAY_CLEAR(sMVOpeningTransitionOverlayGObj);
    OVERLAY_CLEAR(sMVOpeningRoomWipeOverlayTris);
    OVERLAY_CLEAR(sMVOpeningRoomWipeOutlineTris);
    OVERLAY_CLEAR(sMVOpeningRoomWipeOverlayCount);
    OVERLAY_CLEAR(sMVOpeningRoomWipeOutlineCount);
    /* NULL already: freed at the 18 s eject or at StartScene's end */
    OVERLAY_CLEAR(sMVOpeningRoomWipePhoto);
    OVERLAY_CLEAR(sMVOpeningRoomWipeHasPhoto);
    OVERLAY_CLEAR(sMVOpeningRoomWipePhotoWin);
    OVERLAY_CLEAR(sMVOpeningRoomCameraGObj);
    OVERLAY_CLEAR(sMVOpeningRoomUnused0x80134D54);
    OVERLAY_CLEAR(sMVOpeningRoomPropPacks);
    OVERLAY_CLEAR(sMVOpeningRoomPropIsLoaded);
    OVERLAY_CLEAR(sMVOpeningRoomWallpaperBank);
    OVERLAY_CLEAR(sMVOpeningRoomWallpaperFileHead);
    OVERLAY_CLEAR(sMVOpeningRoomCamAnimBank);
}
