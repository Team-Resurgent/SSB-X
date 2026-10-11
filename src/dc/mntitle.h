/* mntitle.h -- the title screen, mn/mncommon/mntitle.c, first cut.
 *
 * The title is one sprite end to end: fourteen sprites in one file, in four of
 * the ROM's ten sprite formats, drawn by the game's own sprite camera
 * and coloured by its own combiner settings. So this is the game's title
 * scene reduced to its sprites: the logo's three words and their drop
 * shadow, the trademark, the copyright footer, the border, PRESS START,
 * and the red logo silhouette behind it all, positioned and coloured by
 * mnTitleSetPosition and mnTitleSetColors and revealed on the game's
 * own schedule (mnTitleTransitionsFuncRun: labels at tic 170, final
 * layout at 220, PRESS START at 280, which is also when a press may
 * proceed).
 *
 * It also has the rest of what the title shows
 * when it is not coming from the opening movie: the fill colour behind
 * everything easing to a new pastel every 260 tics, the two flames
 * (file 168's thirty frames), the labels flying in and PRESS START
 * pulsing on file 167's AnimJoint trees (mnTitlePlayAnim copies a DObj's
 * translate and scale into the sprite each tic), and the three black
 * frames before mode select.
 *
 * It is also the title after the opening movie: the
 * logo animating in on file 167's tree with the three black cutouts
 * riding it and the full logo fading over them, the logo fire's
 * particles (the mntitle bank) climbing the fire tree, the slash
 * (tools/export/ssb_effectexport.py's titleslash row) cutting across under its
 * own ortho camera, and the hand-over to the fire layout at tic 111;
 * and the idle demonstration, mnTitleProceedDemoNext at 650 tics (1190
 * after the movie), which picks the next two demo fighters and leaves
 * for How to Play, or back to the N64 logo from mode select or the
 * auto-demo. What is still not here is a DIVERGES at the function it
 * belongs to: the JP-only logo model, and the scheduler-tic wait after
 * the movie (mnTitleFuncStart).
 *
 * Where it goes on START is the game's mode select (nSCKindModeSelect,
 * src/dc/mnmodeselect.c); the scene-chain cut that
 * was here is the scene manager's now (src/dc/scmanager.c), one scene
 * further along. With the results screen coming back here, the loop is
 * title -> mode select -> match -> results -> title.
 */
#ifndef SSB_DC_MNTITLE_H
#define SSB_DC_MNTITLE_H

#include <sys/obj.h>
#include <sys/taskman.h>
#include <mn/mntypes.h>

/* mntitle.c:139 */
extern SYTaskmanSetup dMNTitleTaskmanSetup;

/* mntitle.c:64. The offsets are the game's link labels
 * (&llMNTitleCutoutSprite ...), written out. */
extern MNTitleSpriteDesc dMNTitleCommonSpriteDescs[/* */];

s32 mnTitleGetFighterKindsNum(u16 mask);
s32 mnTitleGetShuffledFighterKind(u16 this_mask, u16 prev_mask, s32 random);
void mnTitleSetDemoFighterKinds(void);
void mnTitleInitVars(void);
void mnTitleSetEndLogoPosition(void);
void mnTitleSetEndLayout(void);
void mnTitleProceedDemoNext(void);
void mnTitleProceedModeSelect(void);
void mnTitleFuncRun(GObj *gobj);
void mnTitleUpdateFireVars(void);
void mnTitleTransitionFromFireLogo(void);
void mnTitleShowGObjLinkID(s32 link_id);
void mnTitleAdvanceLayout(void);
void mnTitleSetAllowProceedWait(void);
void mnTitleTransitionsFuncRun(GObj *gobj);
void mnTitlePlayAnim(GObj *gobj);
void mnTitlePressStartProcUpdate(GObj *gobj);
void mnTitleProcUpdate(GObj *gobj);
void mnTitleUpdateLabelsPosition(GObj *gobj);
void mnTitleSetPosition(DObj *dobj, SObj *sobj, s32 kind);
void mnTitleSetColors(SObj *sobj, s32 kind);
void mnTitleFireProcDisplay(GObj *fire_gobj);
void mnTitleFireFuncRun(GObj *gobj);
void mnTitleShowFire(GObj *gobj);
void mnTitleUpdateFireSprite(SObj *sobj, sb32 is_next);
void mnTitleFireProcUpdate(GObj *gobj);
void mnTitleMakeFire(void);
void mnTitleLogoProcUpdate(GObj *gobj);
void mnTitleLogoProcDisplay(GObj *gobj);
void mnTitleFadeOutLogoFuncRun(GObj *gobj);
void mnTitleMakeLogoNoOpening(void);
void mnTitleMakeLogo(void);
void mnTitleMakeLabels(void);
void mnTitleMakePressStart(void);
void mnTitleMakeSlash(void);
void mnTitleFireCameraProcUpdate(GObj *gobj);
s32 mnTitleMakeCameras(void);
void mnTitleLogoFireProcDisplay(GObj *gobj);
void mnTitleMakeLogoFire(void);
void mnTitleMakeLogoFireParticles(void);
void mnTitleMakeActors(void);
void mnTitleFuncStart(void);
void mnTitleFuncUpdate(void);
void mnTitleStartScene(void);
void mnTitleLoadFiles(void);

#endif /* SSB_DC_MNTITLE_H */
