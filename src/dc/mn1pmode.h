/* mn1pmode.h -- the 1P submenu, mn/mn1pmode/mn1pmode.c.
 *
 * The first option on the mode select, and the gate in front of the
 * whole 1P game: the four things the stick chooses between here are the
 * four ways into the 1P modes. A tab each for 1P GAME and TRAINING MODE (the
 * three-piece option tab every menu shares, stretched), then shorter
 * all-in-one tabs for BONUS 1 PRACTICE and BONUS 2 PRACTICE, stacked
 * down the middle over the SMASH BROS. collage, two paper decals, a
 * dark controller icon, and the logo with the words 1P and GAME MODE in
 * an orange panel.
 *
 * A or START enters the chosen one -- nSCKind1PGamePlayers,
 * nSCKindPlayers1PTraining, nSCKind1PBonus1Players or
 * nSCKind1PBonus2Players. All four are ported (the two bonus selects are one scene). B goes
 * back to the mode select, and five minutes of no input at all goes to
 * the title (sMN1PModeReturnTic, rearmed by any input).
 *
 * Coming back from one of the four selects puts the cursor back on the
 * option that went there (mn1PModeInitVars reads scene_prev), so the
 * menu remembers where the player was.
 *
 * Every function is the decomp's by name and body, the REGION_US arms;
 * the line numbers are the decomp's. What is not here, and where it is
 * said:
 *   - the lighting pre-render (mn1PModeFuncLights) and the clear camera
 *     -- dMN1PModeTaskmanSetup and mn1PModeFuncStart;
 *   - syVideoInit and the arena_size line -- mn1PModeStartScene;
 *   - the reloc setup: two sprite banks stand in for the two files --
 *     mn1PModeLoadFiles;
 *   - the RDP commands of mn1PModeLabelsProcDisplay, in the port's
 *     spelling (src/dc/lbcommon.h);
 *   - the DMEM self-check in mn1PModeFuncStart -- there is no RSP, so
 *     gSYMainDmemOK has nothing to report; the arm is dropped and said
 *     at the site.
 *   - the JP arm of mn1PModeMakeSubtitle, which is the whole function:
 *     the US build makes the GObj and hangs nothing on it, and
 *     mn1PModeSetSubtitleSpriteColors goes unused with it, exactly as
 *     mnDataMakeMenuGObj's own arm does (src/dc/mndata.h).
 */
#ifndef SSB_DC_MN1PMODE_H
#define SSB_DC_MN1PMODE_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

extern SYTaskmanSetup dMN1PModeTaskmanSetup;

void mn1PModeSetOptionSpriteColors(GObj *gobj, s32 status, s32 option_id);
void mn1PModeMakeOptionTab(GObj *gobj, f32 pos_x, f32 pos_y, s32 lrs);
void mn1PModeMake1PGame(void);
void mn1PModeMakeTrainingMode(void);
void mn1PModeMakeBonus1Practice(void);
void mn1PModeMakeBonus2Practice(void);
void mn1PModeMakeSubtitle(void);
void mn1PModeLabelsProcDisplay(GObj *gobj);
void mn1PModeMakeLabels(void);
void mn1PModeMakeDecals(void);
void mn1PModeMakeLink3Camera(void);
void mn1PModeMakeOptionsCamera(void);
void mn1PModeMakeLabelsCamera(void);
void mn1PModeMakeDecalsCamera(void);
void mn1PModeInitVars(void);
void mn1PModeFuncRun(GObj *gobj);
void mn1PModeLoadFiles(void);
void mn1PModeFuncStart(void);
void mn1PModeStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[18]. */
void mn1PModeOverlayLoad(void);

#endif /* SSB_DC_MN1PMODE_H */
