/* mnnocontroller.h -- the "please insert a controller" screen,
 * mn/mncommon/mnnocontroller.c. Its file-loading half in the decomp is a
 * sibling file, mnnocontrollerfiles.c; the port folds
 * mnNoControllerSetupFiles into this file as mnNoControllerLoadFiles,
 * the same shape every other menu scene's own LoadFiles takes.
 *
 * The decomp sets gSCManagerSceneData.scene_curr = nSCKindNoController
 * exactly once, at boot, only if gSYControllerConnectedNum == 0
 * (sc/scmanager.c:862-864) -- before the scene loop even starts. This
 * file's own three functions (mnNoControllerFuncStart,
 * mnNoControllerMakeCamera, mnNoControllerMakeImage) have no exit logic
 * whatsoever: both GObjs they make take a NULL update function, and
 * nothing here ever writes scene_curr or calls syTaskmanSetLoadScene().
 * On real hardware, reaching this scene means staying on it until the
 * console is power-cycled with a pad plugged in.
 *
 * The port's own scManagerInitData (src/dc/scmanager.c) does not carry
 * the gSYControllerConnectedNum == 0 check -- that line says this scene
 * is "one the port does not have," on the grounds that the port assumes
 * a pad is always connected. This file is ported for completeness, not
 * to re-add the boot-time gate, which would need its own design pass for
 * how the port detects "no controller" on the DC. So this
 * scene is reachable only by a direct DB_BOOT_SCENE boot, same as every
 * other file in this port until something calls it -- except nothing
 * ever will, by design.
 *
 * Every function is the decomp's by name and body, the REGION_US arms;
 * the line numbers are the decomp's. What is not here, and where it is
 * said:
 *   - the black clear camera -- mnNoControllerFuncStart, the PVR clears
 *     its own frame;
 *   - syVideoInit, the zbuffer setup and the arena_size line --
 *     mnNoControllerStartScene;
 *   - the reloc setup: one sprite bank stands in for the scene's one
 *     file -- mnNoControllerLoadFiles.
 */
#ifndef SSB_DC_MNNOCONTROLLER_H
#define SSB_DC_MNNOCONTROLLER_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mnnocontroller.c:19 */
extern SYTaskmanSetup dMNNoControllerTaskmanSetup;

/* mnnocontrollerfiles.c:22 gMNNoControllerFiles, the one file's base:
 * the game's void*[1] with a SpriteBank* in it (sprite.h). Not static in
 * the decomp (its own 'g' prefix, unlike every other menu file's own
 * 's'-prefixed files array); the host test reads the sprite out of it by
 * offset. */
extern void *gMNNoControllerFiles[1];

/* mnnocontroller.c:79-126, in the decomp's order. */
GObj *mnNoControllerMakeCamera(void);
void mnNoControllerMakeImage(void);
void mnNoControllerFuncStart(void);

/* mnnocontroller.c:129-136 0x800D6604: one task, and the scene never
 * ends -- see the header note above. */
void mnNoControllerStartScene(void);

/* The bzero arm of syDmaLoadOverlay for overlay 11 (src/dc/overlay.h). */
void mnNoControllerOverlayLoad(void);

#endif /* SSB_DC_MNNOCONTROLLER_H */
