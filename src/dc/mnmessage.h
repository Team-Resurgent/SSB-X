/* mnmessage.h -- the unlock message, mn/mncommon/mnmessage.c.
 *
 * The screen the game puts between a record being broken and whatever
 * comes next: the SMASH BROS. collage as a wallpaper, a blue sheet over
 * it, a "!" decal, and one sprite saying what has just been unlocked.
 * A, B or START after two seconds applies the unlock -- the bit in
 * gSCManagerBackupData.unlock_mask, and for the four hidden fighters
 * their bit in fighter_mask and the cursor's kind -- writes the save
 * data, and ends the scene.
 *
 * It is the smallest scene in the game and it is the one that makes the
 * save data reachable by a player: every other unlock path in the port
 * writes a queue nobody reads. sc/scmanager.c enters it from the VS
 * results screen and from 1P mode, once per queued message, and the
 * queue is gSCManagerSceneData.unlock_messages -- seven slots, each
 * holding an LBBackupUnlock or nLBBackupUnlockEnumCount for "nothing
 * here" (src/dc/scmanagerdata.c is where those defaults come from).
 *
 * Every function is the decomp's by name and body, the REGION_US arms.
 * What is not here, and where it is said:
 *   - the clear camera and the lighting pre-render (mnMessageFuncLights)
 *     -- dMNMessageTaskmanSetup;
 *   - syVideoInit and the arena_size line -- mnMessageStartScene;
 *   - the reloc setup: two sprite banks stand in for the two files --
 *     mnMessageLoadFiles;
 *   - the RDP commands of mnMessageTintProcDisplay, in the port's
 *     spelling (src/dc/lbcommon.h);
 *   - the JP arms: the sound-test message the JP build queues after
 *     each one (sc1PManagerCheckUnlockSoundTest, 1P mode's), and
 *     nSCKindOpeningRoom as the not-from-results destination.
 *
 * When it did not come from the VS results screen it goes to
 * nSCKindStartup (src/dc/mnstartup.c); that is the 1P game's way in,
 * whose router decides for itself where the run goes next. */
#ifndef SSB_DC_MNMESSAGE_H
#define SSB_DC_MNMESSAGE_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mnmessage.c:381 */
extern SYTaskmanSetup dMNMessageTaskmanSetup;

/* mnmessage.c:52 sMNMessageFiles, the two files' bases: the game's
 * void*[2] with a SpriteBank* in each (sprite.h). Not static in the
 * decomp; the host test reads sprites out of them by offset. */
extern void *sMNMessageFiles[2];

/* mnmessage.c:37, 39: which message is being shown, and which slot of
 * gSCManagerSceneData.unlock_messages it was taken out of. The host
 * test reads both. */
extern s32 sMNMessageUnlockID;
extern s32 sMNMessageQueueID;

/* mnmessage.c:58-346, in the decomp's order. */
void mnMessageMakeWallpaper(void);
void mnMessageTintProcDisplay(GObj *gobj);
void mnMessageMakeTint(void);
void mnMessageMakeExclaim(void);
void mnMessageMakeMessage(s32 message);
void mnMessageMakeTintCamera(void);
void mnMessageMakeMessageCamera(void);
void mnMessageMakeWallpaperCamera(void);
void mnMessageMakeExclaimCamera(void);
void mnMessageInitVars(void);
void mnMessageApplyUnlock(void);
void mnMessageFuncRun(GObj *gobj);
void mnMessageFuncStart(void);

/* mnmessage.c:421 0x801323F8: one task per queued message, then the
 * scene it goes to. */
void mnMessageStartScene(void);

/* The bzero arm of syDmaLoadOverlay for overlay 22 (src/dc/overlay.h). */
void mnMessageOverlayLoad(void);

#endif /* SSB_DC_MNMESSAGE_H */
