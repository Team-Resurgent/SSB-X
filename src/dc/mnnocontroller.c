/* mnnocontroller.c -- see mnnocontroller.h. Every function is
 * mn/mncommon/mnnocontroller.c's by name and body, REGION_US arms; the
 * line numbers are the decomp's. mnnocontrollerfiles.c's own
 * mnNoControllerSetupFiles becomes mnNoControllerLoadFiles below. */
#include "mnnocontroller.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"

#include <sys/debug.h>
#include <sys/rdp.h>
#include <sys/controller.h>
#include <macros.h>

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mnscreenadjust.c does the same). Both values are
 * relocData file 169 (MNNoController) as tools/export/ssb_spriteexport.py
 * --list reads it off the ROM. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

#define llMNNoControllerSprite 0x08460

/* The scene's own one bank. */
#define MNNOCONTROLLER_BANK "mnnocontroller.spr"

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mnnocontrollerfiles.c:13 (0x800D67B0): { &llMNNoControllerFileID }, the
 * one link label's value written out. */
u32 dMNNoControllerFileIDs[/* */] = { 169 };

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* mnnocontroller.c:70 sMNNoControllerPad0x800D67C0, an s32 the decomp
 * declares and never reads or writes anywhere in the file -- unlike
 * mnoption.c's/mnscreenadjust.c's own kept statics, which are at least
 * written by something (src/dc/mncongra.c's own note on
 * D_ovl57_801321C4, the same kind of truly dead declaration). Dropped. */

/* mnnocontrollerfiles.c:22 gMNNoControllerFiles, the one file's base.
 * Not static in the decomp (its own 'g' prefix, where every other
 * file's own files array is 's'-prefixed). */
void *gMNNoControllerFiles[ARRAY_COUNT(dMNNoControllerFileIDs)];

/* the bank behind gMNNoControllerFiles */
static SpriteBank sMNNoControllerBanks[ARRAY_COUNT(dMNNoControllerFileIDs)];

/* mnnocontrollerfiles.c:25, 28 sMNNoControllerStatusBuffer,
 * sMNNoControllerForceStatusBuffer: the reloc loader's per-file status
 * records. Dropped with lbRelocInitSetup (mnNoControllerLoadFiles
 * below). */

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mnnocontroller.c:79-102 0x800D6490, verbatim. The camera every menu
 * scene's guide/instruction sprites share (src/dc/mnscreenadjust.c's own
 * mnScreenAdjustMakeSpriteCamera is the same shape); this scene has only
 * the one sprite this camera draws. */
GObj *mnNoControllerMakeCamera(void)
{
	GObj *gobj = gcMakeCameraGObj
	(
		1000,
		NULL,
		0,
		GOBJ_PRIORITY_DEFAULT,
		lbCommonDrawSprite,
		100,
		COBJ_MASK_DLLINK(0),
		~0,
		FALSE,
		nGCProcessKindFunc,
		NULL,
		1,
		FALSE
	);
	CObj *cobj = CObjGetStruct(gobj);

	syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

	return gobj;
}

/* mnnocontroller.c:105-117 0x800D6538, verbatim. */
void mnNoControllerMakeImage(void)
{
	GObj *gobj;
	SObj *sobj;

	gobj = gcMakeGObjSPAfter(1001, NULL, 1, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 0, GOBJ_PRIORITY_DEFAULT, ~0);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, gMNNoControllerFiles[0], llMNNoControllerSprite));

	sobj->pos.x = 10.0F;
	sobj->pos.y = 10.0F;
}

/* mnnocontroller.c:419-435-shaped (mnscreenadjust.c's own line numbers
 * for the equivalent), replacing mnnocontrollerfiles.c's
 * lbRelocInitSetup/lbRelocLoadFilesListed: one sprite bank stands in for
 * the scene's one file, loaded out of the romdisk into the scene heap
 * and VRAM. */
static void mnNoControllerLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMNNoControllerFileIDs)] =
    {
        MNNOCONTROLLER_BANK
    };
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(dMNNoControllerFileIDs); i++)
    {
        if (sprite_bank_load(&sMNNoControllerBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mnNoController: no bank for file %d (%s)\n",
                          (int)dMNNoControllerFileIDs[i], paths[i]);
            gMNNoControllerFiles[i] = NULL;
            continue;
        }
        gMNNoControllerFiles[i] = &sMNNoControllerBanks[i];
    }
}

/* mnnocontroller.c:120-126 0x800D65B8, in the game's order.
 *
 * DIVERGES: mnNoControllerSetupFiles is mnNoControllerLoadFiles above,
 * and gcMakeDefaultCameraGObj -- the black clear camera -- is the frame
 * clear, which the PVR does itself (the same cut as every other ported
 * menu scene's own FuncStart).
 *
 * What is NOT diverged: this function has no exit branch, on either
 * region, in the decomp or here. Nothing past this point ever writes
 * gSCManagerSceneData.scene_curr or calls syTaskmanSetLoadScene() --
 * mnnocontroller.h's own header note says why that is faithful, not a
 * bug. */
void mnNoControllerFuncStart(void)
{
	mnNoControllerLoadFiles();
	mnNoControllerMakeCamera();
	mnNoControllerMakeImage();
}

/* mnnocontroller.c:19-61 (0x800D671C). The pool counts are the game's,
 * all zero: a menu scene takes its objects straight from the scene heap.
 * DIVERGES as every other menu scene's: the arena is the port's region
 * (NULL here, see src/dc/taskman.c), the draw is the scene manager's
 * (scManagerFuncDraw is gcDrawAll, src/dc/mntitle.c's own note), and the
 * four DL buffer sizes/Graphics Heap Size/RDP Output Buffer Size are
 * zero rather than the decomp's own RSP display-list numbers -- there is
 * no RSP here, the same reason the arena line goes. This file never had
 * a lighting pre-render to begin with (Pre-render function is NULL in
 * the decomp too), so there is nothing to drop there. */
SYTaskmanSetup dMNNoControllerTaskmanSetup =
{
    // Task Manager Buffer Setup
    {
        0,                          // ???
        gcRunAll,          			// Update function
        scManagerFuncDraw,          // Frame draw function
        NULL,                       // Allocatable memory pool start
        0,                          // Allocatable memory pool size
        1,                          // ???
        2,                          // Number of contexts?
        0, 0, 0, 0,                 // the four DL buffer sizes
        0,                          // Graphics Heap Size
        2,                          // ???
        0,                          // RDP Output Buffer Size
        NULL,         				// Pre-render function
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

    mnNoControllerFuncStart         // Task start function
};

/* The bzero arm of syDmaLoadOverlay for overlay 11, this file:
 * src/dc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnNoControllerOverlayLoad(void)
{
    OVERLAY_CLEAR(gMNNoControllerFiles);
    OVERLAY_CLEAR(sMNNoControllerBanks);
}

/* mnnocontroller.c:129-136 0x800D6604. DIVERGES: syVideoInit, the
 * zbuffer and the arena_size line are the N64's video mode and its link
 * map, set once at boot here as in every other scene.
 *
 * This call does not return until syTaskmanSetLoadScene() is called
 * (src/dc/taskman.c's own note on syTaskmanLoadScene) -- which nothing
 * in this scene ever does. A caller that reaches this function blocks
 * here, on the target exactly as on the N64; src/game/ssb64/hosttest_ft.c
 * drives its own frames instead of calling this, for exactly that
 * reason. */
void mnNoControllerStartScene(void)
{
	syTaskmanStartTask(&dMNNoControllerTaskmanSetup);
}
