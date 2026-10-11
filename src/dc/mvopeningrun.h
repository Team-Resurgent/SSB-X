/* mvopeningrun.h -- the openings' eleventh scene, mv/mvopening/mvopeningrun.c
 * ported. Overlay 44.
 *
 * ... Fox(39) -> Pikachu(42) -> Run(44) -> Cliff(46) -> ... At tic 220 it
 * hands off to nSCKindOpeningCliff, read from mvOpeningRunFuncRun; Cliff
 * is unported, so scManagerRunScene's default arm loops this scene back.
 *
 * NOT one of the per-fighter scenes: eight fighters (Mario, Fox, Donkey,
 * Samus, Link, Yoshi, Kirby, Pikachu) are alive at once and run across a
 * scrolling wallpaper. Each fighter is flown by a PROXY: a bare GObj with
 * one DObj carrying an AObjEvent32 script (gcAddDObjAnimJoint), whose
 * translate/rotate mvOpeningRunFighterProcUpdate copies onto the real
 * fighter's DObj each tic. At tic 45 Link switches status; at tic 190 the
 * crash model appears and the explosion sound plays.
 *
 * ---- What is substituted
 *
 *   - Files. relocData 55 (MVOpeningRun: eight DObj scripts + the
 *     wallpaper sprite), 60 (MVOpeningRunMain: the camera script) and 75
 *     (MVOpeningRunCrash) are not loaded. The eight scripts and the
 *     camera's are romdisk/mvopeningrun.cam (tools/export/ssb_camanimexport.py
 *     --bank OpeningRun; the exporter learned a per-entry label and
 *     track count, the DObj scripts set joint tracks, not the camera's
 *     ten), fetched by name with camanim_get; the wallpaper is
 *     romdisk/mvopeningrun.spr (file 55's one sprite, offset 0x58A0).
 *   - The crash model (file 75, tic 190) is romdisk/mvopeningruncrash.mdl
 *     (tools/export/ssb_effectexport.py `--what runcrash`): a
 *     5-joint DObjDesc + MObjSub + MatAnimJoint tree, which that tool's
 *     entry rows already carry (one MObj owning several draws, each with
 *     its own texture, is EFENTRY_MULTI). It is loaded when the crash is
 *     made and given back in OverlayLoad.
 *   - mvopeningmario.h's Substitutions 3b (efManagerInitEffects ->
 *     efManagerLoadEffectBank/PreloadModels), 4 (the figatree heaps: eight
 *     mallocs cut, desc.figatree_heap NULL) and 5 (the trailing tic-sync
 *     busy-wait); func_lights is NULL (the standing rule).
 *   - The pool counts are larger than the decomp's (see mvopeningrun.c; this scene's TaskmanSetup is
 *     not zeroed like the others'), not the per-fighter scenes' 48/288.
 *   - The camera attach is #ifdef FT_HOSTTEST-guarded like the rest of
 *     the chain.
 *
 * Memory: eight fighter packs resident at once is the openings' budget
 * event (scene-memory-strategy); the PVR fence in fighter_load guards it.
 */
#ifndef SSB_DC_MVOPENINGRUN_H
#define SSB_DC_MVOPENINGRUN_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

extern SYTaskmanSetup dMVOpeningRunTaskmanSetup;

/* The loaded mvopeningrun.spr bank's head, for hosttest_ft.c. */
extern void *sMVOpeningRunWallpaperFileHead;

void mvOpeningRunFighterProcUpdate(GObj *fighter_proxy_gobj);
void mvOpeningRunMakeFighters(void);
void mvOpeningRunWallpaperProcUpdate(GObj *gobj);
void mvOpeningRunMakeWallpaper(void);
void mvOpeningRunMakeCrash(void);
void mvOpeningRunInitMainCamera(GObj *camera_gobj);
void mvOpeningRunMakeMainCamera(void);
void mvOpeningRunMakeWallpaperCamera(void);
void mvOpeningRunInitVars(void);
void mvOpeningRunFuncRun(GObj *gobj);
void mvOpeningRunFuncStart(void);
void mvOpeningRunStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[44]. */
void mvOpeningRunOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGRUN_H */
