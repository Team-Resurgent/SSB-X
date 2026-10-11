/* mncongra.c -- see mncongra.h. Every function is
 * mn/mncommon/mncongra.c's by name and body, REGION_US arms; the line
 * numbers are the decomp's. */
#include "mncongra.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"

#include <sys/debug.h>
#include <sys/controller.h>
#include <sys/rdp.h>
#include <sc/scdef.h>
#include <gm/gmsound.h>
#include <ft/fighter.h>
#include <lb/lbfade.h>
#include <mn/mndef.h>
#include <macros.h>

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/mnscreenadjust.c's own copy of this declaration. */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mnoption.c does the same). */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* dMNCongraPictures' bottom_offset/top_offset are the same one value
 * for all twelve fighters -- each of the 24 files (relocData 170-193)
 * holds exactly one sprite, confirmed with
 * tools/export/ssb_spriteexport.py --file <n> --list against every one of
 * them. */
#define llMNCongraPictureSprite 0x20718

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mncongra.c:18-91 dMNCongraPictures. The game's own table is a
 * file-id/offset pair per fighter, indexed by fkind (FTKind's own
 * playable order, Mario first) -- one lbRelocGetExternHeapFile per
 * visit, since only one fighter's pair is ever shown. The port's own
 * table is the bank-path pair sMNCongraLoadFiles loads on demand,
 * same order, same index: relocData 170-193 (tools/relocFile
 * Descriptions.us.txt), 24 whole-file exports, one bottom/top pair per
 * fighter (src/game/ssb64/Makefile's MNCONGRA_BANK_RULE template,
 * which shares $(FIGHTERS)'s own order with this table rather than
 * re-deriving it). */
typedef struct MNCongraPictureBanks
{
    const char *bottom_path;
    const char *top_path;
} MNCongraPictureBanks;

static const MNCongraPictureBanks sMNCongraPictureBanks[] =
{
    /* Mario */   { "mncongramariobottom.spr",   "mncongramariotop.spr" },
    /* Fox */     { "mncongrafoxbottom.spr",     "mncongrafoxtop.spr" },
    /* Donkey */  { "mncongradonkeybottom.spr",  "mncongradonkeytop.spr" },
    /* Samus */   { "mncongrasamusbottom.spr",   "mncongrasamustop.spr" },
    /* Luigi */   { "mncongraluigibottom.spr",   "mncongraluigitop.spr" },
    /* Link */    { "mncongralinkbottom.spr",    "mncongralinktop.spr" },
    /* Yoshi */   { "mncongrayoshibottom.spr",   "mncongrayoshitop.spr" },
    /* Captain */ { "mncongracaptainbottom.spr", "mncongracaptaintop.spr" },
    /* Kirby */   { "mncongrakirbybottom.spr",   "mncongrakirbytop.spr" },
    /* Pikachu */ { "mncongrapikachubottom.spr", "mncongrapikachutop.spr" },
    /* Purin */   { "mncongrapurinbottom.spr",   "mncongrapurintop.spr" },
    /* Ness */    { "mncongranessbottom.spr",    "mncongranesstop.spr" },
};

/* mncongra.c:94 */
SYColorRGBA dMNCongraFadeColor = { 0x00, 0x00, 0x00, 0xFF };

/* mncongra.c:97 D_ovl57_801321C4, a second SYColorRGBA the decomp
 * declares and never reads or writes anywhere in the file -- unlike
 * mnoption.c's/mnbackupclear.c's own kept dead statics, which are at
 * least written by something. Dropped, the same call the port's other
 * true padding words take (mnbackupclear.h's own note on
 * sMNBackupPad0x801330B8). */

/* mncongra.c:100-108 dMNCongraLights1/dMNCongraDisplayList: the
 * lighting the pre-render function would set for the 3D this scene
 * never draws. Dropped with mnCongraFuncLights (mncongra.h says why). */

/* mncongra.c:111-122 dMNCongraVideoSetup: the N64's video mode, set
 * once at boot here as in every other scene (mncongra.h says why it is
 * not ported at all, the same as mnScreenAdjustVideoSetup/
 * mnBackupClearVideoSetup). */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* mncongra.c:176 sMNCongraPad0x801322B0[2], two words the link map
 * names but nothing reads. Dropped: the port has no link map to keep a
 * hole in (mnoption.c's own sMNOptionPad0x801337B0 note). */

/* mncongra.c:179 sMNCongraStatusBuffer[5]: the reloc loader's per-file
 * status records. Dropped with lbRelocInitSetup (mnCongraLoadFiles). */

/* mncongra.c:182 */
s32 sMNCongraFighterKind;

/* mncongra.c:185, 188 */
s32 sMNCongraSkipWait;
s32 sMNCongraSceneChangeWait;

/* mncongra.c:191, 194 */
sb32 sMNCongraIsProceed;
sb32 sMNCongraIsProceedScene;

/* the bank behind sMNCongraFiles: [0] bottom, [1] top. Loaded fresh on
 * every visit (mnCongraLoadFiles below), since which fighter's pair is
 * wanted is not known until sMNCongraFighterKind is set. */
static SpriteBank sMNCongraBanks[2];
void *sMNCongraFiles[2];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mncongra.c:203-215, verbatim. */
sb32 mnCongraCheckPlayerControllerConnected(s32 player)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(gSYControllerDeviceStatuses); i++)
    {
        if (player == gSYControllerDeviceStatuses[i])
        {
            return TRUE;
        }
    }
    return FALSE;
}

/* mncongra.c:218-234, verbatim. */
s32 mnCongraGetPlayerTapButtons(u32 buttons)
{
    s32 player;

    for (player = 0; player < ARRAY_COUNT(gSYControllerDevices); player++)
    {
        if
        (
            (mnCongraCheckPlayerControllerConnected(player) != FALSE) &&
            (gSYControllerDevices[player].button_tap & buttons)
        )
        {
            return player + 1;
        }
    }
    return 0;
}

/* mncongra.c:237-256, verbatim. */
void mnCongraActorFuncRun(GObj *gobj)
{
    SYColorRGBA color;

    (void)gobj;

    if (sMNCongraSkipWait != 0)
    {
        sMNCongraSkipWait--;
    }
    if
    (
        (sMNCongraSkipWait == 0)       &&
        (sMNCongraIsProceed == FALSE)  &&
        (mnCongraGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
    )
    {
        sMNCongraIsProceed = TRUE;
        color = dMNCongraFadeColor;
        lbFadeMakeActor(nGCCommonKindTransition, nGCCommonLinkIDTransition, 10, &color, 90, FALSE, &sMNCongraIsProceedScene);
    }
}

/* mncongra.c:271-366 lbRelocInitSetup and the two
 * lbRelocGetExternHeapFile/lbRelocGetFileData calls, as every other
 * menu scene has them (src/dc/mndata.c): the current fighter's own two
 * banks, loaded out of the romdisk into the scene heap and VRAM --
 * not all twenty-four, since only one pair is ever shown per visit. */
static void mnCongraLoadFiles(void)
{
    const MNCongraPictureBanks *pic = &sMNCongraPictureBanks[sMNCongraFighterKind];

    if (sprite_bank_load(&sMNCongraBanks[0], pic->bottom_path) < 0)
    {
        syDebugPrintf("mnCongra: no bank for %s\n", pic->bottom_path);
        sMNCongraFiles[0] = NULL;
    }
    else
    {
        sMNCongraFiles[0] = &sMNCongraBanks[0];
    }

    if (sprite_bank_load(&sMNCongraBanks[1], pic->top_path) < 0)
    {
        syDebugPrintf("mnCongra: no bank for %s\n", pic->top_path);
        sMNCongraFiles[1] = NULL;
    }
    else
    {
        sMNCongraFiles[1] = &sMNCongraBanks[1];
    }
}

/* mncongra.c:259-366 0x80131CA4, in the game's order.
 *
 * DIVERGES: the LBRelocSetup block is mnCongraLoadFiles above, and
 * gcMakeDefaultCameraGObj -- the black clear camera -- is the frame
 * clear, which the PVR does itself (the same cut every menu scene's
 * FuncStart takes). */
void mnCongraFuncStart(void)
{
    CObj *cobj;
    GObj *gobj;
    SObj *sobj;

    sMNCongraSkipWait = 8;
    sMNCongraSceneChangeWait = 0;
    sMNCongraIsProceed = FALSE;
    sMNCongraIsProceedScene = 0;

    mnCongraLoadFiles();

    gcMakeGObjSPAfter(0, mnCongraActorFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

    cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindWallpaperCamera,
            NULL,
            nGCCommonLinkIDCamera,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            80,
            COBJ_MASK_DLLINK(0),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    gobj = gcMakeGObjSPAfter(nGCCommonKindWallpaper, NULL, nGCCommonLinkIDWallpaper, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 0, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCongraFiles[0], llMNCongraPictureSprite));
    sobj->sprite.attr &= ~SP_FASTCOPY;

    sobj->pos.x = 10.0F;
    sobj->pos.y = 120.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCongraFiles[1], llMNCongraPictureSprite));
    sobj->sprite.attr &= ~SP_FASTCOPY;

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;

    func_800269C0_275C0
    (
        (gSCManagerSceneData.spgame_score >= 1000000) ? nSYAudioVoiceAnnounceIncredible : nSYAudioVoiceAnnounceCongra
    );
}

/* mncongra.c:369-393. DIVERGES: the decomp's syVideoSetFlags(
 * SYVIDEO_FLAG_BLACKOUT) forces the VI to show nothing for the wait
 * below, an N64 video-mode call this port never implements --
 * syVideoInit is not ported either, and every scene that calls
 * syVideoGetFillColor or syVideoSetFlags drops the call the same way
 * (mnvsrecord.h's, mnsoundtest.h's own notes). The wait itself, and the scene-kind write at its end,
 * are real and kept: they are what actually ends the scene. */
void mnCongraFuncDraw(void)
{
    gcDrawAll();

    if (sMNCongraIsProceedScene != FALSE)
    {
        sMNCongraIsProceedScene = FALSE;

        sMNCongraSceneChangeWait = 5;
    }
    if (sMNCongraSceneChangeWait != 0)
    {
        sMNCongraSceneChangeWait--;

        if (sMNCongraSceneChangeWait == 0)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            syTaskmanSetLoadScene();
        }
    }
}

/* mncongra.c:124-167. The pool counts are the game's, all zero: a menu
 * scene takes its objects straight from the scene heap. DIVERGES as
 * every other menu scene's: the arena is the port's region, the draw
 * is this file's own mnCongraFuncDraw (it has more to do than
 * scManagerFuncDraw's plain gcDrawAll -- the fade-then-scene-change
 * sequence above), and mnCongraFuncLights is dropped.
 * dMNCongraVideoSetup itself is not ported: see mnCongraStartScene. */
SYTaskmanSetup dMNCongraTaskmanSetup =
{
    // Task Manager Buffer Setup
    {
        0,                              // ???
        gcRunAll,                       // Update function
        mnCongraFuncDraw,               // Frame draw function
        NULL,                           // Allocatable memory pool start
        0,                              // Allocatable memory pool size
        1,                              // ???
        2,                              // Number of contexts?
        0, 0, 0, 0,                     // the four DL buffer sizes
        0,                              // Graphics Heap Size
        2,                              // ???
        0,                              // RDP Output Buffer Size
        NULL,                           // Pre-render function
        syControllerFuncRead,           // Controller I/O function
    },

    0,                                  // Number of GObjThreads
    sizeof(u64) * 192,                  // Thread stack size
    0,                                  // Number of thread stacks
    0,                                  // ???
    0,                                  // Number of GObjProcesses
    0,                                  // Number of GObjs
    sizeof(GObj),                       // GObj size
    0,                                  // Number of XObjs
    NULL,                               // Matrix function list
    NULL,                               // DObjVec eject function
    0,                                  // Number of AObjs
    0,                                  // Number of MObjs
    0,                                  // Number of DObjs
    sizeof(DObj),                       // DObj size
    0,                                  // Number of SObjs
    sizeof(SObj),                       // SObj size
    0,                                  // Number of CObjs
    sizeof(CObj),                       // Camera size

    mnCongraFuncStart                   // Task start function
};

/* mncongra.c:402-424 mnCongraStartScene's own switch, verbatim.
 *
 * DIVERGES: syVideoInit, the zbuffer setup, the arena_size line and the
 * two raw framebuffer-clear loops either side of them are the N64's
 * video mode, set once at boot here as in every other scene
 * (mncongra.h says why dMNCongraVideoSetup is not ported at all). */
void mnCongraStartScene(void)
{
    switch (gSCManagerSceneData.scene_prev)
    {
    default:
        sMNCongraFighterKind = nFTKindMario;
        break;

    case nSCKind1PGame:
        sMNCongraFighterKind = gSCManagerSceneData.fkind;
        break;

    case nSCKindDebugBattle:
        sMNCongraFighterKind = gSCManagerTransferBattleState.players[0].fkind;
        break;
    }

    syTaskmanStartTask(&dMNCongraTaskmanSetup);
}

/* The bzero arm of syDmaLoadOverlay for overlay 57, this file:
 * src/dc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnCongraOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNCongraFighterKind);
    OVERLAY_CLEAR(sMNCongraSkipWait);
    OVERLAY_CLEAR(sMNCongraSceneChangeWait);
    OVERLAY_CLEAR(sMNCongraIsProceed);
    OVERLAY_CLEAR(sMNCongraIsProceedScene);
    OVERLAY_CLEAR(sMNCongraFiles);
    OVERLAY_CLEAR(sMNCongraBanks);
}
