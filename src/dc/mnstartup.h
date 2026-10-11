/* mnstartup.h -- the N64 logo boot screen, mn/mncommon/mnstartup.c.
 * Overlay 58. The shortest scene in the game: one sprite drops in on a
 * parabola, sits, fades to black and hands off.
 *
 * Why it is worth porting ahead of the remaining eighteen opening
 * scenes: this is the ONLY route into the openings chain on a US build.
 * Three call sites reach nSCKindStartup and one leaves it --
 *
 *   mn/mncommon/mntitle.c:473-477 (the title's demo rotation, US arm)
 *   sc/sccommon/scautodemo.c:386-389 (US arm; the port is already
 *       faithful at src/dc/scautodemo.c:438, so it lands in the scene
 *       manager's not-ported fallback today)
 *   mn/mncommon/mnstartup.c:171 -> nSCKindOpeningRoom
 *
 * -- and nothing outside mv/mvopening/ ever sets an nSCKindOpening*
 * scene itself. Without this file the whole chain is reachable only by
 * a direct DB_BOOT_SCENE boot, however many opening scenes get ported.
 * It does not make the chain reachable from a cold boot on its own:
 * nSCKindStartup is the US build's default boot scene's neighbour, not
 * the default itself (dSCManagerDefaultSceneData's US arm is
 * nSCKindStartup -- scmanager.c:487-495 -- so a cold boot DOES start
 * here), but the title's own idle rotation back into it still needs
 * mnTitleProceedDemoNext, which is a separate deferral documented at
 * src/dc/mntitle.c:434-448.
 *
 * Every function is the decomp's by name and body; the line numbers are
 * the decomp's. What is not here, and where it is said:
 *   - the black clear camera (gcMakeDefaultCameraGObj with
 *     COBJ_FLAG_FILLCOLOR) -- mnStartupFuncStart. Two reasons, not the
 *     usual one: the PVR clears its own frame, AND on this port that
 *     flag is a scene-wide black screen rather than a clear, because it
 *     becomes an lbCommonSpriteFillRect in the translucent list at a
 *     sprite depth and every opaque depth sits behind sprite depths
 *     (src/dc/objdisplay.c's gcPrepCameraViewport says it at length).
 *     Keeping it would have painted over the logo this scene exists to
 *     show;
 *   - mnStartupFuncLights, dMNStartupLights1 and dMNStartupDisplayList
 *     -- the pre-render lighting list, which needs an RSP;
 *   - syVideoInit, the zbuffer setup and the arena_size line --
 *     mnStartupStartScene;
 *   - the LBRelocSetup block: one sprite bank stands in for the scene's
 *     one file -- mnStartupLoadFiles.
 *
 * One thing that is NOT a divergence, because it looked like one:
 * dMNStartupTaskmanSetup declares 0 GObjThreads while mnStartupFuncStart
 * adds a real one (gcAddGObjProcess ... nGCProcessKindThread). That is
 * fine on both targets and the counts are kept verbatim -- the fields
 * are pool sizes, not ceilings, and sys/objman.c:117-120 (the thread)
 * and :162-185 (its stack) both fall back to syTaskmanMalloc out of the
 * scene heap when the free list is empty.
 */
#ifndef SSB_DC_MNSTARTUP_H
#define SSB_DC_MNSTARTUP_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mnstartup.c:37 */
extern SYTaskmanSetup dMNStartupTaskmanSetup;

/* mnstartup.c:17, 20: the fade this scene opens and closes on. */
extern SYColorRGBA dMNStartupEndFadeColor;
extern SYColorRGBA dMNStartupStartFadeColor;

/* The scene's one file (194, llN64LogoFileID): the game's void*[1] with
 * a SpriteBank* in it (sprite.h). The decomp has no array at all here --
 * mnStartupFuncStart reads the file straight into a local -- so this is
 * the port's own, named the way every other ported scene's is, and it
 * exists because sprite_bank_load needs somewhere durable to put the
 * bank and because the host test reads the sprite back out of it. */
extern void *gMNStartupFiles[1];

/* mnstartup.c:94, 97: how many frames before A/B/START may skip, and
 * whether the logo thread has reached its hand-off. The host test drives
 * both. */
extern s32 sMNStartupSkipAllowWait;
extern sb32 sMNStartupIsProceedOpening;

/* mnstartup.c:106-256, in the decomp's order. */
void mnStartupLogoThreadUpdate(GObj *gobj);
void mnStartupActorFuncRun(GObj *gobj);
void mnStartupFuncStart(void);

/* mnstartup.c:265-274 0x80131EF0. */
void mnStartupStartScene(void);

/* The bzero arm of syDmaLoadOverlay for overlay 58 (src/dc/overlay.h). */
void mnStartupOverlayLoad(void);

#endif /* SSB_DC_MNSTARTUP_H */
