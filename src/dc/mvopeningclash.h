/* mvopeningclash.h -- the openings' eighteenth scene, mv/mvopening/
 * mvopeningclash.c ported. Overlay 49.
 *
 * ... Sector(50) -> Standoff(47) -> Clash(49) -> Newcomers ... At tic
 * 160 it hands off to nSCKindOpeningNewcomers, read from
 * mvOpeningClashFuncRun; Newcomers is unported, so scManagerRunScene's
 * default arm loops this one back.
 *
 * All eight of the original roster (Mario, Kirby, Link, Yoshi, Fox,
 * Donkey Kong, Samus, Pikachu, in nFTDemoStatusClash) at one origin
 * under a scripted camera, before a wallpaper of four animated quadrants
 * (each a display list with an MObj and both animation kinds), with a
 * white "void" flash from tic 144 and a sound at tics 15, 75, 90 and 105.
 *
 * ---- What is substituted
 *
 *   - Files. relocData 72 (MVOpeningClashFighters: the fighters' camera
 *     script) and 66 (MVOpeningClashWallpaper: the four quadrants and
 *     their camera script) are not loaded. The quadrants are
 *     romdisk/mvopeningclashwall{ll,lr,ul,ur}.mdl
 *     (tools/export/ssb_effectexport.py `--what clashwallll` ...: one display
 *     list on one DObj with an MObj, a MatAnimJoint and an AnimJoint --
 *     the MObj owns five draws with a texture each, which the tool now
 *     accepts when the MObj has no sprite array, EFENTRY_MULTI); the
 *     cameras' scripts are romdisk/mvopeningclash.cam (bank OpeningClash,
 *     "Fighters" and "Wall").
 *   - The quadrants draw through dc_model_proc_display (the walk
 *     gcDrawDObjDLHead0 does).
 *   - mvopeningmario.h's Substitutions 3b, 4 (the figatree heaps cut,
 *     desc.figatree_heap NULL) and 5; func_lights is NULL; camera attach
 *     and pack builds are #ifdef FT_HOSTTEST-guarded.
 *   - The pools are Run's (eight fighters at once, mvopeningrun.c).
 */
#ifndef SSB_DC_MVOPENINGCLASH_H
#define SSB_DC_MVOPENINGCLASH_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

extern SYTaskmanSetup dMVOpeningClashTaskmanSetup;

void mvOpeningClashMakeFighters(void);
void mvOpeningClashVoidProcDisplay(GObj *gobj);
void mvOpeningClashMakeVoid(void);
void mvOpeningClashMakeWallpaper(void);
void mvOpeningClashMakeFightersCamera(void);
void mvOpeningClashMakeVoidCamera(void);
void mvOpeningClashWallpaperProcDisplay(GObj *gobj);
void mvOpeningClashMakeWallpaperCamera(void);
void mvOpeningClashInitTotalTimeTics(void);
void mvOpeningClashFuncRun(GObj *gobj);
void mvOpeningClashFuncStart(void);
void mvOpeningClashStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[49]. */
void mvOpeningClashOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGCLASH_H */
