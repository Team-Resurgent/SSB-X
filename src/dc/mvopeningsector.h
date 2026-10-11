/* mvopeningsector.h -- the openings' sixteenth scene, mv/mvopening/
 * mvopeningsector.c ported. Overlay 50.
 *
 * ... Jungle(51) -> Yoster(45) -> Sector(50) -> Standoff ... At tic
 * 160 it hands off to nSCKindOpeningStandoff, read from
 * mvOpeningSectorFuncRun; Standoff is unported, so scManagerRunScene's
 * default arm loops this one back.
 *
 * No fighter: the Great Fox and three Arwings fly across a scrolling
 * starfield (a 2x2 tiling of one sprite), and at tic 120 a cockpit
 * sprite scales up over the shot.
 *
 * ---- What is substituted
 *
 *   - Files. relocData 73 (MVOpeningSector: the Great Fox, the Arwing
 *     tables, the cockpit sprite, the camera script), 161 (FoxSpecial3,
 *     whose one DObjDesc is the Arwing) and 74 (the wallpaper) are not
 *     loaded. The models are romdisk/mvopeningsector{greatfox,arwing}.mdl
 *     (tools/export/ssb_scenemodelexport.py, `--dllinks`; the Arwing is file
 *     161's tree with file 73's three tables as its three animations,
 *     `--anim OFF@73`), the camera is romdisk/mvopeningsector.cam (bank
 *     OpeningSector), the cockpit romdisk/mvopeningsector.spr (file 73)
 *     and the wallpaper romdisk/mvopeningsectorwallpaper.spr (file 74).
 *   - The four wallpaper SObjs are made in a loop (they differ in
 *     position only).
 *   - mvopeningmario.h's Substitutions 4 and 5; func_lights is NULL; the
 *     ambient sound the busy-wait preceded plays at the end of
 *     FuncStart; camera attach and pack builds are #ifdef
 *     FT_HOSTTEST-guarded.
 *   - This scene names its task set-up mvOpeningSectorTaskmanSetup (no
 *     `d` prefix), as the decomp does.
 */
#ifndef SSB_DC_MVOPENINGSECTOR_H
#define SSB_DC_MVOPENINGSECTOR_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

extern SYTaskmanSetup mvOpeningSectorTaskmanSetup;

/* The loaded sprite banks' heads, for hosttest_ft.c. */
extern void *sMVOpeningSectorCockpitFileHead;
extern void *sMVOpeningSectorWallpaperFileHead;

void mvOpeningSectorWallpaperProcUpdate(GObj *wallpaper_gobj);
void mvOpeningSectorMakeWallpaper(void);
void mvOpeningSectorMakeGreatFox(void);
void mvOpeningSectorCockpitProcDisplay(GObj *cockpit_gobj);
void mvOpeningSectorCockpitProcUpdate(GObj *cockpit_gobj);
void mvOpeningSectorMakeCockpit(void);
void mvOpeningSectorMakeArwings(void);
void mvOpeningSectorCameraProcUpdate(GObj *camera_gobj);
void mvOpeningSectorMakeMainCamera(void);
void mvOpeningSectorMakeWallpaperCamera(void);
void mvOpeningSectorMakeCockpitCamera(void);
void mvOpeningSectorInitTotalTimeTics(void);
void mvOpeningSectorFuncRun(GObj *gobj);
void mvOpeningSectorFuncStart(void);
void mvOpeningSectorStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[50]. */
void mvOpeningSectorOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGSECTOR_H */
