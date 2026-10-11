/* mnoption.h -- the Options screen, mn/mnoption/mnoption.c.
 *
 * The third option on the mode select, and the spine under
 * two further screens: SCREEN ADJUST and BACKUP CLEAR. The screen itself
 * is three stacked option tabs -- SOUND, SCREEN ADJUST, BACKUP CLEAR --
 * over the same collage/decal/logo wallpaper mndata.c's DATA menu uses,
 * plus a fourth piece the DATA menu has none of: SOUND's own STEREO/MONO
 * toggle and the underline that marks which half is picked, redrawn
 * every tic under its own camera.
 *
 * SCREEN ADJUST and BACKUP CLEAR are both
 * ported now. Each option still sets gSCManagerSceneData.scene_curr as
 * the game does, so the scene manager's default arm bounces straight
 * back here and mnOptionInitVars puts the cursor on the tab that was
 * chosen -- the game's own return route (src/dc/scmanager.c), the same
 * shape mndata.h documents for its own children.
 *
 * Every function is the decomp's by name and body, the REGION_US arms;
 * the line numbers are the decomp's. What is not here, and where it is
 * said:
 *   - the lighting pre-render (mnOptionFuncLights) and the clear camera
 *     -- dMNOptionTaskmanSetup and mnOptionFuncStart;
 *   - syVideoInit and the arena_size line -- mnOptionStartScene;
 *   - the reloc setup: two sprite banks stand in for the two files --
 *     mnOptionLoadFiles;
 *   - the JP arm of mnOptionMakeMenuGObj, which is the whole function:
 *     the US build makes the GObj and hangs nothing on it, and
 *     mnOptionSetSubtitleSpriteColors goes unused with it.
 */
#ifndef SSB_DC_MNOPTION_H
#define SSB_DC_MNOPTION_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mnoption.c:1071 */
extern SYTaskmanSetup dMNOptionTaskmanSetup;

/* mnoption.c:124 sMNOptionFiles, the two files' bases: the game's
 * void*[2] with a SpriteBank* in each (sprite.h). Not static in the
 * decomp; the host test reads sprites out of them by offset. */
extern void *sMNOptionFiles[2];

/* mnoption.c:79, 82, 85, 109-115 -- the state the host test reads:
 * which tab the cursor is on, the mono/stereo toggle, the screen-flash
 * bit, the wait counter, and the 5-minute idle-return clock. */
extern s32 sMNOptionOption;
extern sb32 sMNOptionSoundMonoOrStereo;
extern sb32 sMNOptionIsScreenFlash;
extern sb32 sMNOptionIsProceedScene;
extern s32 sMNOptionOptionChangeWait;
extern s32 sMNOptionTotalTimeTics;
extern s32 sMNOptionReturnTic;

/* mnoption.c:67-97, the three option tabs' GObjs and the sound toggle's,
 * in nMNOptionOption order for the first three. */
extern GObj *sMNOptionOptionSoundGObj;
extern GObj *sMNOptionOptionScreenAdjustGObj;
extern GObj *sMNOptionOptionBackupClearGObj;
extern GObj *sMNOptionSoundOptionGObj;
extern GObj *sMNOptionMenuGObj;

/* mnoption.c:133-1123, in the decomp's order. */
void mnOptionSetOptionSpriteColors(GObj *gobj, s32 status);
void mnOptionMakeOptionTabs(GObj *gobj, f32 pos_x, f32 pos_y, s32 lrs);
void mnOptionSetSoundToggleSpriteColors(GObj *gobj, sb32 mono_or_stereo);
void mnOptionMakeSoundToggle(void);
void mnOptionMakeSound(void);
void mnOptionMakeScreenAdjust(void);
void mnOptionMakeBackupClear(void);
void mnOptionSetSubtitleSpriteColors(SObj *sobj);
void mnOptionMakeMenuGObj(void);
void mnOptionLabelsProcDisplay(GObj *gobj);
void mnOptionMakeLabels(void);
void mnOptionMakeDecals(void);
void mnOptionSoundUnderlineProcDisplay(GObj *gobj);
void mnOptionMakeSoundUnderline(void);
void mnOptionMakeSoundUnderlineCamera(void);
void mnOptionMakeLink4Camera(void);
void mnOptionMakeOptionsCamera(void);
void mnOptionMakeLabelsCamera(void);
void mnOptionMakeDecalsCamera(void);
void mnOptionInitVars(void);
void mnOptionWriteBackup(void);
void mnOptionFuncRun(GObj *gobj);
void mnOptionFuncStart(void);

/* mnoption.c:1115 0x801335C0: one task, and the scene is over when it
 * ends. */
void mnOptionStartScene(void);

/* The bzero arm of syDmaLoadOverlay for overlay 60 (src/dc/overlay.h). */
void mnOptionOverlayLoad(void);

#endif /* SSB_DC_MNOPTION_H */
