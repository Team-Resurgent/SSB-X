/* mvopeningcliff.h -- the openings' twelfth scene, mv/mvopening/
 * mvopeningcliff.c ported. Overlay 46.
 *
 * ... Pikachu(42) -> Run(44) -> Cliff(46) -> Yamabuki(48) ... At tic 160
 * it hands off to nSCKindOpeningYamabuki, read from mvOpeningCliffFuncRun;
 * that scene is unported, so scManagerRunScene's default arm loops this
 * one back.
 *
 * Link stands on a cliff (status 0x1000F) before a scrolling wallpaper;
 * the hills behind him and the ocarina in front are two small models out
 * of relocData 68, and the camera flies on file 68's script.
 *
 * ---- What is substituted
 *
 *   - Files. relocData 68 (the hills, the ocarina, the camera script) and
 *     70 (the wallpaper sprite) are not loaded. The two models are
 *     romdisk/mvopeningcliffhills.mdl and mvopeningcliffocarina.mdl
 *     (tools/export/ssb_scenemodelexport.py, the general form of the arrows'
 *     exporter: textures carried, the hills' Z-buffer-off baked as
 *     FPACK_NOZ, the ocarina's AnimJoint table as the pack's one
 *     animation); the camera is romdisk/mvopeningcliff.cam (bank
 *     OpeningCliff), the wallpaper romdisk/mvopeningcliff.spr.
 *   - The models draw through dc_model_proc_display; the display procs'
 *     own state changes (mvOpeningCliffHillsProcDisplay's pipe sync,
 *     cycle type and render mode) are what the bake carries instead.
 *   - mvopeningmario.h's Substitutions 3b, 4 (the figatree heap malloc
 *     cut, desc.figatree_heap NULL) and 5 (the busy-wait); func_lights is
 *     NULL (the standing rule). The camera attach is
 *     #ifdef FT_HOSTTEST-guarded like the rest of the chain, and so is
 *     the ocarina's pack build (the host links no renderer).
 */
#ifndef SSB_DC_MVOPENINGCLIFF_H
#define SSB_DC_MVOPENINGCLIFF_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

extern SYTaskmanSetup dMVOpeningCliffTaskmanSetup;

/* The loaded mvopeningcliff.spr bank's head, for hosttest_ft.c. */
extern void *sMVOpeningCliffWallpaperFileHead;

void mvOpeningCliffMakeHills(void);
void mvOpeningCliffMakeFighter(void);
void mvOpeningCliffWallpaperProcDisplay(GObj *gobj);
void mvOpeningCliffMakeWallpaper(void);
void mvOpeningCliffMakeOcarina(void);
void mvOpeningCliffMakeMainCamera(void);
void mvOpeningCliffMakeWallpaperCamera(void);
void mvOpeningCliffInitTotalTimeTics(void);
void mvOpeningCliffFuncRun(GObj *gobj);
void mvOpeningCliffFuncStart(void);
void mvOpeningCliffStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[46]. */
void mvOpeningCliffOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGCLIFF_H */
