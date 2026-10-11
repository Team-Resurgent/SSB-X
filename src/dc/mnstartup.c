/* mnstartup.c -- see mnstartup.h. Every function is
 * mn/mncommon/mnstartup.c's by name and body; the line numbers are the
 * decomp's. This scene has no *files.c half in the decomp -- it reads
 * its one file inline, in mnStartupFuncStart -- so unlike
 * mnnocontroller.c/scautodemo.c/scexplain.c there is nothing to fold in,
 * and mnStartupLoadFiles below is entirely the port's own. */
#include "mnstartup.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"
#include "bgm.h"

#include <lb/lbfade.h>

#include <sys/debug.h>
#include <sys/rdp.h>
#include <sys/controller.h>
#include <sc/scsubsys/scsubsys.h>  /* scSubsysControllerGetPlayerTapButtons */
#include <macros.h>

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mnnocontroller.c does the same). Both values are
 * relocData file 194 (N64Logo) as tools/export/ssb_spriteexport.py --list reads
 * it off the ROM: its one and only sprite sits at 0x73c0, which is
 * exactly llN64LogoSprite. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

#define llN64LogoSprite 0x073C0

/* The scene's own one bank, named for the ROM file rather than for this
 * scene: file 194 is llN64LogoFileID, not an MNStartup file, the same
 * way characternames.spr is named for its file and not for the
 * scene that reads it. */
#define N64LOGO_BANK "n64logo.spr"

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mnstartup.c:17 (0x80131F50) */
SYColorRGBA dMNStartupEndFadeColor = { 0x00, 0x00, 0x00, 0xFF };

/* mnstartup.c:20 (0x80131F54) */
SYColorRGBA dMNStartupStartFadeColor = { 0x00, 0x00, 0x00, 0x00 };

/* mnstartup.c:23, 26 dMNStartupLights1 and dMNStartupDisplayList: the
 * two-command pre-render list mnStartupFuncLights hands the RSP. Dropped
 * with the pre-render function itself (dMNStartupTaskmanSetup below),
 * the same cut every ported scene with a Lights1 takes. */

/* mnstartup.c:34 dMNStartupVideoSetup: the N64 video mode, set once at
 * boot here (mnStartupStartScene below). */

/* The port's own, not the decomp's: the file id this scene's one bank
 * stands for, written out the way every other ported scene's FileIDs
 * array is, so tools/export/disc_layout.py and the host test have a name for
 * it. The decomp has no array -- mnStartupFuncStart reads
 * &llN64LogoFileID straight into a local. */
u32 dMNStartupFileIDs[/* */] = { 194 };

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* mnstartup.c:88 sMNStartupPad0x80132040[2], an s32 pair the decomp
 * declares and never reads or writes anywhere in the file -- the same
 * truly dead declaration src/dc/mnnocontroller.c's own note names, and
 * dropped for the same reason. */

/* mnstartup.c:91 sMNStartupStatusBuffer[5]: the reloc loader's per-file
 * status records. Dropped with lbRelocInitSetup (mnStartupLoadFiles
 * below). */

/* mnstartup.c:94 (0x80132070) - Delay frames before N64 logo can be
 * skipped */
s32 sMNStartupSkipAllowWait;

/* mnstartup.c:97 (0x80132074) - if TRUE, proceed to the opening movie */
sb32 sMNStartupIsProceedOpening;

/* the one file's base, and the bank behind it */
void *gMNStartupFiles[ARRAY_COUNT(dMNStartupFileIDs)];
static SpriteBank sMNStartupBanks[ARRAY_COUNT(dMNStartupFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mnstartup.c:106-152 0x80131B00, verbatim. The logo's whole
 * performance, and the scene's clock: sixteen tics of a parabola
 * dropping the sprite from off the bottom of the screen to y 65,
 * twenty-four tics sitting still, a ten-tic fade to black, thirteen more
 * tics, then sMNStartupIsProceedOpening and an endless sleep --
 * mnStartupActorFuncRun below is what notices and leaves.
 *
 * The parabola is the decomp's arithmetic untouched, including that it
 * is written as a subtraction of a negative: with step counting 16 down
 * to 1, -(38.75/64) * step * step is negative, so y starts at
 * 65 - (-155) = 220 (the sprite's own starting pos.y, set in
 * mnStartupFuncStart) and eases to 65 - (-0.605) at step 1, one tic
 * before the explicit y = 65 that follows the loop. */
void mnStartupLogoThreadUpdate(GObj *gobj)
{
	f32 step;
	s32 i;
	SObj *sobj;
	SYColorRGBA color;

	sobj = SObjGetStruct(gobj);

	i = 0;

	while (i < 16)
	{
		step = 16 - i;
		sobj->pos.y = 65.0F - ((-(38.75F / 64.0F) * step) * step);

		gcSleepCurrentGObjThread(1);

		i++;
	}
	sobj->pos.y = 65.0F;

	i = 0;

	while (i < 24)
	{
		gcSleepCurrentGObjThread(1);
		i++;
	}
	color = dMNStartupEndFadeColor;

	lbFadeMakeActor(nGCCommonKindTransition, nGCCommonLinkIDTransition, 10, &color, 10, FALSE, NULL);

	i = 0;

	while (i < 13)
	{
		gcSleepCurrentGObjThread(1);
		i++;
	}
	sMNStartupIsProceedOpening = TRUE;

	while (TRUE)
	{
		gcSleepCurrentGObjThread(1);
	}
}

/* mnstartup.c:155-174 0x80131C20, verbatim. Both exits set scene_prev
 * before scene_curr and then call syTaskmanSetLoadScene, the game's own
 * order. The second one is the whole reason this file is ported: it is
 * the only place outside mv/mvopening/ that ever names an opening scene
 * (src/dc/mnstartup.h's header note). */
void mnStartupActorFuncRun(GObj *gobj)
{
	if (sMNStartupSkipAllowWait != 0)
	{
		sMNStartupSkipAllowWait--;
	}
	if ((sMNStartupSkipAllowWait == 0) && (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE))
	{
		gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
		gSCManagerSceneData.scene_curr = nSCKindTitle;

		syTaskmanSetLoadScene();
	}
	else if (sMNStartupIsProceedOpening != FALSE)
	{
		gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
		gSCManagerSceneData.scene_curr = nSCKindOpeningRoom;
		syTaskmanSetLoadScene();
	}
}

/* Replaces mnstartup.c:189-198's LBRelocSetup block and the
 * lbRelocGetExternHeapFile call at :232-243: one sprite bank stands in
 * for the scene's one file, loaded out of the romdisk into the scene
 * heap and VRAM.
 *
 * Worth noting against the openings' own recipe: this scene does NOT use
 * lbRelocLoadFilesListed. It calls lbRelocGetExternHeapFile directly,
 * with a syTaskmanMalloc(lbRelocGetFileSize(...), 0x10) for the
 * destination -- a one-file load with no file list at all. Port-side
 * that difference disappears, since sprite_bank_load does its own
 * allocation either way, but it is why this function takes no
 * FileIDs-array walk the way src/dc/mnnocontroller.c's does. */
static void mnStartupLoadFiles(void)
{
    if (sprite_bank_load(&sMNStartupBanks[0], N64LOGO_BANK) < 0)
    {
        syDebugPrintf("mnStartup: no bank for file %d (%s)\n",
                      (int)dMNStartupFileIDs[0], N64LOGO_BANK);
        gMNStartupFiles[0] = NULL;
        return;
    }
    gMNStartupFiles[0] = &sMNStartupBanks[0];
}

/* mnstartup.c:177-256 0x80131CB8, in the game's order.
 *
 * DIVERGES: the LBRelocSetup block and the lbRelocGetExternHeapFile call
 * are mnStartupLoadFiles above, and gcMakeDefaultCameraGObj is gone.
 * That second cut is the usual one -- the PVR clears its own frame --
 * but here it also matters for a second, sharper reason: the decomp
 * passes COBJ_FLAG_FILLCOLOR, and on this port that flag is not a clear
 * at all. It becomes an lbCommonSpriteFillRect in the TRANSLUCENT list
 * at a SPRITE depth, and every opaque depth sits behind sprite depths,
 * so the quad paints over every other camera in the scene
 * (src/dc/objdisplay.c's gcPrepCameraViewport, which predicted this
 * before the staff roll proved it). Kept, it would have covered the one
 * sprite this scene exists to show. Six other ported scenes drop the
 * flag the same way. */
void mnStartupFuncStart(void)
{
	CObj *cobj;
	GObj *gobj;
	SObj *sobj;
	Sprite *sprite;
	SYColorRGBA color;

	sMNStartupSkipAllowWait = 8;
	sMNStartupIsProceedOpening = FALSE;

	mnStartupLoadFiles();

	gcMakeGObjSPAfter(0, mnStartupActorFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

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

	gcAddGObjProcess(gobj, mnStartupLogoThreadUpdate, nGCProcessKindThread, 1);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 0, GOBJ_PRIORITY_DEFAULT, ~0);

	sprite = lbRelocGetFileData(Sprite*, gMNStartupFiles[0], llN64LogoSprite);

	sobj = lbCommonMakeSObjForGObj(gobj, sprite);

	sobj->sprite.attr &= ~SP_FASTCOPY;

	sobj->pos.x = 96.0F;
	sobj->pos.y = 220.0F;

	color = dMNStartupStartFadeColor;

	lbFadeMakeActor(nGCCommonKindTransition, nGCCommonLinkIDTransition, 10, &color, 16, TRUE, NULL);
}

/* mnstartup.c:259-262 mnStartupFuncLights: gSPDisplayList of this file's
 * own two-command lighting list. Dropped with the list itself -- there is
 * no RSP here. */

/* mnstartup.c:37-79 (0x80131FB4). The pool counts are the game's, all
 * zero: a menu scene takes its objects straight from the scene heap, and
 * that includes the GObjThread this scene's own FuncStart adds (see
 * src/dc/mnstartup.h's note on why zero is not a crash).
 *
 * DIVERGES as every other menu scene's: the arena is the port's region
 * (NULL here, see src/dc/taskman.c), the draw is the scene manager's
 * (scManagerFuncDraw is gcDrawAll, src/dc/mntitle.c's own note), the
 * four DL buffer sizes/Graphics Heap Size/RDP Output Buffer Size are
 * zero rather than the decomp's own RSP display-list numbers, and the
 * Pre-render function is NULL rather than mnStartupFuncLights. */
SYTaskmanSetup dMNStartupTaskmanSetup =
{
    // Task Manager Buffer Setup
    {
        0,                          // ???
        gcRunAll,              		// Update function
        scManagerFuncDraw,          // Frame draw function
        NULL,                       // Allocatable memory pool start
        0,                          // Allocatable memory pool size
        1,                          // ???
        2,                          // Number of contexts?
        0, 0, 0, 0,                 // the four DL buffer sizes
        0,                          // Graphics Heap Size
        2,                          // ???
        0,                          // RDP Output Buffer Size
        NULL,                       // Pre-render function
        syControllerFuncRead,       // Controller I/O function
    },

    0,                              // Number of GObjThreads
    sizeof(u64) * 192,              // Thread stack size
    0,                              // Number of thread stacks
    0,                              // ???
    0,                              // Number of GObjProcesses
    0,                              // Number of GObjs
    sizeof(GObj),                   // GObj size
    0,                              // Number of XObjs
    NULL,                           // Matrix function list
    NULL,                           // DObjVec eject function
    0,                              // Number of AObjs
    0,                              // Number of MObjs
    0,                              // Number of DObjs
    sizeof(DObj),                   // DObj size
    0,                              // Number of SObjs
    sizeof(SObj),                   // SObj size
    0,                              // Number of CObjs
    sizeof(CObj),                 	// CObj size

    mnStartupFuncStart              // Task start function
};

/* The bzero arm of syDmaLoadOverlay for overlay 58, this file:
 * src/dc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnStartupOverlayLoad(void)
{
    OVERLAY_CLEAR(gMNStartupFiles);
    OVERLAY_CLEAR(sMNStartupBanks);
    OVERLAY_CLEAR(sMNStartupSkipAllowWait);
    OVERLAY_CLEAR(sMNStartupIsProceedOpening);
    /* Not a static: dMNStartupStartFadeColor is initialized data in the
     * decomp, but its initializer is all zeroes, so the compiler puts it
     * in .bss and it lands in the overlay's noload segment like any
     * other -- tools/check/overlay_check.py reads the segment, not the
     * declaration, and is right to ask for it. Clearing it restores
     * exactly the value mnstartup.c:20 declares. Its neighbour
     * dMNStartupEndFadeColor has a non-zero alpha, so it really is in
     * .data and really is reloaded with the overlay. */
    OVERLAY_CLEAR(dMNStartupStartFadeColor);
}

/* mnstartup.c:265-274 0x80131EF0. DIVERGES: syVideoInit, the zbuffer and
 * the arena_size line are the N64's video mode and its link map, set
 * once at boot here as in every other scene. syAudioStopBGMAll is real
 * and kept -- this is the first scene a cold boot runs, so on the N64 it
 * is stopping nothing, but it is also where the title's own idle demo
 * rotation comes back through with music playing. */
void mnStartupStartScene(void)
{
	syAudioStopBGMAll();

	syTaskmanStartTask(&dMNStartupTaskmanSetup);
}
