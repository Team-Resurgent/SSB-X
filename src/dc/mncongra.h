/* mncongra.h -- the congratulations screen, mn/mncommon/mncongra.c.
 *
 * Reached from 1P mode (or
 * Debug Battle) at the end of a clear: which fighter's picture it shows
 * is read off gSCManagerSceneData.scene_prev, not passed as an argument
 * -- nSCKind1PGame reads gSCManagerSceneData.fkind, nSCKindDebugBattle
 * reads gSCManagerTransferBattleState.players[0].fkind, anything else
 * (a direct scene jump) falls to Mario. Neither route is ported yet, so
 * this screen is reachable only by a direct DB_BOOT_SCENE jump for now,
 * which is exactly the "anything else" arm -- Mario's picture, by
 * design, not a bug (src/dc/db.c has no seed to add here the way
 * nSCKindCharacters and nSCKindOption needed, because the fallback is
 * already the sane one). A/B/START all fade to black and hand off to
 * nSCKindTitle -- the game's own destination, not a stand-in for a
 * scene this port lacks.
 *
 * Every function is the decomp's by name and body, the REGION_US arms
 * (mnCongraStartScene's switch on scene_prev, and the whole file --
 * REGION_JP builds a different screen this decomp file does not carry);
 * the line numbers are the decomp's. What is not here, and where it is
 * said:
 *   - the lighting pre-render (mnCongraFuncLights) and its display
 *     list/Lights1 data -- dropped outright, the same cut every menu
 *     scene's pre-render takes (mnscreenadjust.h, mnbackupclear.h);
 *     dMNCongraTaskmanSetup's pre-render slot is NULL;
 *   - the black clear camera (gcMakeDefaultCameraGObj) --
 *     mnCongraFuncStart, the PVR clears its own frame;
 *   - syVideoInit, the zbuffer setup and the arena_size line --
 *     mnCongraStartScene;
 *   - the reloc setup: dMNCongraPictures' file-id/offset pairs become a
 *     bank-path table, sMNCongraPictureBanks, and the two
 *     lbRelocGetExternHeapFile/lbRelocGetFileData calls in
 *     mnCongraFuncStart become mnCongraLoadFiles, loading only the
 *     current fighter's own two banks -- not all twenty-four at once,
 *     since only one fighter's pair is ever shown in a single visit
 *     (mncongra.c below says why this screen's own file table could not
 *     just become a fixed MENU_BANKS list the way every earlier menu
 *     screen's did).
 *
 * The scale of this screen is almost entirely its pictures: twenty-four
 * relocData files (170-193), one whole file per fighter's bottom or top
 * half, each holding exactly one sprite at a fixed offset (0x20718) --
 * confirmed with tools/export/ssb_spriteexport.py --file <n> --list against
 * every one of them. dMNCongraPictures' own order (Mario, Fox, Donkey,
 * Samus, Luigi, Link, Yoshi, Captain, Kirby, Pikachu, Purin, Ness) is
 * FTKind's own playable order, index for index -- the same order
 * src/game/ssb64/Makefile's own $(FIGHTERS) list already walks for
 * every fighter's pack and stock/emblem bank, so sMNCongraPictureBanks
 * below and the Makefile's MNCONGRA_BANK_RULE template share it without
 * a translation table.
 */
#ifndef SSB_DC_MNCONGRA_H
#define SSB_DC_MNCONGRA_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mncongra.c:125 */
extern SYTaskmanSetup dMNCongraTaskmanSetup;

/* mncongra.c:182, 259-269 -- the state the host test reads: which
 * fighter's picture is up, the pre-input skip, the post-fade scene-
 * change countdown, and the two proceed flags. */
extern s32 sMNCongraFighterKind;
extern s32 sMNCongraSkipWait;
extern s32 sMNCongraSceneChangeWait;
extern sb32 sMNCongraIsProceed;
extern sb32 sMNCongraIsProceedScene;

/* the two banks behind the current fighter's pictures: [0] bottom,
 * [1] top. Not static in the decomp's own shape either -- sMNCongraFiles
 * there is a void*[2] the host test reads by offset the same way every
 * other menu scene's own sMNXxxFiles is read. */
extern void *sMNCongraFiles[2];

/* mncongra.c:203-432, in the decomp's order. */
sb32 mnCongraCheckPlayerControllerConnected(s32 player);
s32 mnCongraGetPlayerTapButtons(u32 buttons);
void mnCongraActorFuncRun(GObj *gobj);
void mnCongraFuncStart(void);
void mnCongraFuncDraw(void);

/* mncongra.c:402-432 0x8013200C: one task, and the scene is over when
 * it ends. */
void mnCongraStartScene(void);

/* The bzero arm of syDmaLoadOverlay for overlay 57 (src/dc/overlay.h). */
void mnCongraOverlayLoad(void);

#endif /* SSB_DC_MNCONGRA_H */
