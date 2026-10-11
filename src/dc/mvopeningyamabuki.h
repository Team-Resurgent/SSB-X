/* mvopeningyamabuki.h -- the openings' thirteenth scene, mv/mvopening/
 * mvopeningyamabuki.c ported. Overlay 48.
 *
 * ... Run(44) -> Cliff(46) -> Yamabuki(48) -> Jungle(51) ... At tic 160 it
 * hands off to nSCKindOpeningJungle, read from mvOpeningYamabukiFuncRun;
 * Jungle is unported, so scManagerRunScene's default arm loops this one
 * back.
 *
 * Pikachu (status 0x1000F) stands before a wallpaper; three models out of
 * relocData 71 (the legs that walk past, their shadow and a Poke Ball)
 * are flown by their own AnimJoint tables, and the camera by the file's
 * script.
 *
 * ---- What is substituted
 *
 *   - File 71 is not loaded. The three models are
 *     romdisk/mvopeningyamabuki{legs,legsshadow,mball}.mdl
 *     (tools/export/ssb_scenemodelexport.py; the shadow and ball are DObjDLLink
 *     trees, `--dllinks`); the camera is romdisk/mvopeningyamabuki.cam
 *     (bank OpeningYamabuki) and the wallpaper romdisk/mvopeningyamabuki.spr.
 *   - All three draw through dc_model_proc_display, which is the walk
 *     both of the game's procs do (gcDrawDObjTreeForGObj and
 *     gcDrawDObjTreeDLLinksForGObj).
 *   - The default camera's FILLCOLOR is dropped (fillcolor-camera note).
 *   - mvopeningmario.h's Substitutions 3b, 4 and 5; func_lights is NULL;
 *     camera attach and pack builds are #ifdef FT_HOSTTEST-guarded.
 */
#ifndef SSB_DC_MVOPENINGYAMABUKI_H
#define SSB_DC_MVOPENINGYAMABUKI_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

extern SYTaskmanSetup dMVOpeningYamabukiTaskmanSetup;

/* The loaded mvopeningyamabuki.spr bank's head, for hosttest_ft.c. */
extern void *sMVOpeningYamabukiWallpaperFileHead;

void mvOpeningYamabukiMakeWallpaper(void);
void mvOpeningYamabukiMakeFighter(void);
void mvOpeningYamabukiMakeLegs(void);
void mvOpeningYamabukiMakeLegsShadow(void);
void mvOpeningYamabukiMakeMBall(void);
void mvOpeningYamabukiMakeMainCamera(void);
void mvOpeningYamabukiMakeWallpaperCamera(void);
void mvOpeningYamabukiInitTotalTimeTics(void);
void mvOpeningYamabukiFuncRun(GObj *gobj);
void mvOpeningYamabukiFuncStart(void);
void mvOpeningYamabukiStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[46]. */
void mvOpeningYamabukiOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGYAMABUKI_H */
