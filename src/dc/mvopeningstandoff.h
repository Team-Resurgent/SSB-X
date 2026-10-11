/* mvopeningstandoff.h -- the openings' seventeenth scene, mv/mvopening/
 * mvopeningstandoff.c ported. Overlay 47.
 *
 * ... Yoster(45) -> Sector(50) -> Standoff(47) -> Clash ... At tic 320
 * it hands off to nSCKindOpeningClash, read from mvOpeningStandoffFuncRun;
 * Clash is unported, so scManagerRunScene's default arm loops this one
 * back.
 *
 * Mario and Kirby (status 0x1000F each, at one origin) face off on a
 * ground before a scrolling wallpaper, under a scripted camera; a
 * lightning bolt animates behind them and the screen flashes white at
 * tics 20, 150 and 261.
 *
 * ---- What is substituted
 *
 *   - Files. relocData 69 (MVOpeningStandoff: the ground, the lightning,
 *     the camera script) and 70 (the wallpaper) are not loaded. The
 *     models are romdisk/mvopeningstandoff{ground,lightning}.mdl
 *     (tools/export/ssb_effectexport.py, `--what standoffground` and `--what
 *     standofflight`: the lightning has MObjs and a MatAnimJoint, which
 *     that tool's entry rows already carry); the camera is
 *     romdisk/mvopeningstandoff.cam (bank OpeningStandoff), and the
 *     wallpaper is file 70's sprite, which is mvopeningcliff.c's too, so
 *     romdisk/mvopeningcliff.spr is loaded and not a second copy.
 *   - The lightning's sprite arrays are {A, NULL, B, C, D} and its
 *     scripts never step to the hole; the exporter fills it with the
 *     frame before (EFENTRY_HOLES), and the six MObjs that step the same
 *     five pictures share one run of them.
 *   - Both models draw through dc_model_proc_display (the walk
 *     gcDrawDObjDLHead0 and gcDrawDObjTreeDLLinksForGObj do).
 *   - mvopeningmario.h's Substitutions 3b, 4 (the figatree heaps cut,
 *     desc.figatree_heap NULL) and 5; func_lights is NULL; camera attach
 *     and pack builds are #ifdef FT_HOSTTEST-guarded.
 *   - The pools are Run's, not the decomp's 512s: the port makes one
 *     XObj set per fighter joint (mvopeningrun.c).
 */
#ifndef SSB_DC_MVOPENINGSTANDOFF_H
#define SSB_DC_MVOPENINGSTANDOFF_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

extern SYTaskmanSetup dMVOpeningStandoffTaskmanSetup;

/* The loaded wallpaper bank's head, for hosttest_ft.c. */
extern void *sMVOpeningStandoffWallpaperFileHead;

void mvOpeningStandoffMakeGround(void);
void mvOpeningStandoffMakeFighters(void);
void mvOpeningStandoffWallpaperProcUpdate(GObj *wallpaper_gobj);
void mvOpeningStandoffMakeWallpaper(void);
void mvOpeningStandoffMakeLightning(void);
void mvOpeningStandoffLightningFlashProcDisplay(GObj *gobj);
void mvOpeningStandoffMakeLightningFlash(void);
void mvOpeningStandoffMakeLightningFlashCamera(void);
void mvOpeningStandoffMakeMainCamera(void);
void mvOpeningStandoffMakeWallpaperCamera(void);
void mvOpeningStandoffInitTotalTimeTics(void);
void mvOpeningStandoffFuncRun(GObj *gobj);
void mvOpeningStandoffFuncStart(void);
void mvOpeningStandoffStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[47]. */
void mvOpeningStandoffOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGSTANDOFF_H */
