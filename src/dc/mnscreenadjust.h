/* mnscreenadjust.h -- the Screen Adjust screen, mn/mnoption/mnscreenadjust.c.
 *
 * The first of Options' two children -- reached from the
 * SCREEN ADJUST tab (mnoption.c's own mnOptionFuncRun) and nothing else.
 * The whole screen is a crosshair and a grey border frame drawn over a
 * guide picture, nudged by the stick or the D-pad/C-buttons; A, B or
 * START all write the offsets to the backup and return to Options with
 * the cursor already on this tab (mnOptionInitVars reads scene_prev the
 * same way it reads nSCKindBackupClear for the other child).
 *
 * Every function is the decomp's by name and body, the REGION_US arms;
 * the line numbers are the decomp's. What is not here, and where it is
 * said:
 *   - the lighting pre-render (mnScreenAdjustFuncLights) and its
 *     display list/Lights1 data -- dMNScreenAdjustTaskmanSetup, the
 *     same cut every menu scene's pre-render takes;
 *   - the black clear camera -- mnScreenAdjustFuncStart, the PVR clears
 *     its own frame;
 *   - syVideoInit, the zbuffer setup and the arena_size line --
 *     mnScreenAdjustStartScene;
 *   - the reloc setup: one sprite bank stands in for the scene's one
 *     file -- mnScreenAdjustLoadFiles.
 *
 * The screen adjust offsets themselves are real state
 * (gSCManagerBackupData.screen_adjust_h/v, src/dc/sysshim.c's own
 * DIVERGES note on gSYVideoOffsetLeft/Top): they are written and read
 * back correctly, they just move nothing, since the PVR's output
 * rectangle is fixed by the video mode KOS sets rather than shiftable
 * inside a CRT's overscan the way the N64's VI is.
 */
#ifndef SSB_DC_MNSCREENADJUST_H
#define SSB_DC_MNSCREENADJUST_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mnscreenadjust.c:34 */
extern SYTaskmanSetup dMNScreenAdjustTaskmanSetup;

/* mnscreenadjust.c:109 sMNScreenAdjustFiles, the one file's base: the
 * game's void*[1] with a SpriteBank* in it (sprite.h). Not static in
 * the decomp; the host test reads sprites out of it by offset. */
extern void *sMNScreenAdjustFiles[1];

/* mnscreenadjust.c:88-100 -- the state the host test reads: the two
 * offsets, the stick-hold-suppress counter, the total-time clock and
 * the 5-minute idle-return tic. */
extern f32 sMNScreenAdjustOffsetH;
extern f32 sMNScreenAdjustOffsetV;
extern s32 sMNScreenAdjustButtonHoldWait;
extern s32 sMNScreenAdjustTotalTimeTics;
extern s32 sMNScreenAdjustReturnTic;

/* mnscreenadjust.c:118-455, in the decomp's order. */
void mnScreenAdjustFrameProcDisplay(GObj *gobj);
void mnScreenAdjustMakeFrame(void);
void mnScreenAdjustMakeGuide(void);
void mnScreenAdjustMakeInstruction(void);
void mnScreenAdjustMakeFrameCamera(void);
void mnScreenAdjustMakeSpriteCamera(void);
void mnScreenAdjustApplyCenterOffsets(s16 h, s16 v);
void mnScreenAdjustInitVars(void);
void mnScreenAdjustBackupOffsets(void);
void mnScreenAdjustFuncRun(GObj *gobj);
void mnScreenAdjustFuncStart(void);

/* mnscreenadjust.c:447-455 0x801327D8: one task, and the scene is over
 * when it ends. */
void mnScreenAdjustStartScene(void);

/* The bzero arm of syDmaLoadOverlay for overlay 25 (src/dc/overlay.h). */
void mnScreenAdjustOverlayLoad(void);

#endif /* SSB_DC_MNSCREENADJUST_H */
