/* mvopeningyoster.h -- the openings' fifteenth scene, mv/mvopening/
 * mvopeningyoster.c ported. Overlay 45.
 *
 * ... Yamabuki(48) -> Jungle(51) -> Yoster(45) -> Sector(52) ... At tic
 * 160 it hands off to nSCKindOpeningSector, read from
 * mvOpeningYosterMainProc; Sector is unported, so scManagerRunScene's
 * default arm loops this one back.
 *
 * Four Yoshis in his four costumes (statuses 0x1000F..0x10012) stand on
 * a nest and a moving ground of Yoshi's Island before its wallpaper; the
 * camera flies on the file's script.
 *
 * ---- What is substituted
 *
 *   - Files. relocData 67 (MVOpeningYoster: the nest, the ground with its
 *     AnimJoint, the camera script) and 93 (StageYoshi, whose one sprite
 *     is the wallpaper) are not loaded. The models are
 *     romdisk/mvopeningyoster{nest,nestrest,ground}.mdl (the nest is 73
 *     joints, so two packs; see mvOpeningYosterMakeNest)
 *     (tools/export/ssb_scenemodelexport.py, `--dllinks`); the camera is
 *     romdisk/mvopeningyoster.cam (bank OpeningYoster) and the wallpaper
 *     romdisk/mvopeningyoster.spr (file 93, offset 0x26C88).
 *   - Both models draw through dc_model_proc_display (the walk that
 *     gcDrawDObjTreeDLLinksForGObj does).
 *   - mvopeningmario.h's Substitutions 3b, 4 (the four figatree heaps
 *     cut, desc.figatree_heap NULL) and 5; func_lights is NULL; camera
 *     attach and pack builds are #ifdef FT_HOSTTEST-guarded.
 *   - The pools are Run's, not the decomp's 512s: the port makes one
 *     XObj set per fighter joint (mvopeningrun.c).
 */
#ifndef SSB_DC_MVOPENINGYOSTER_H
#define SSB_DC_MVOPENINGYOSTER_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

extern SYTaskmanSetup dMVOpeningYosterTaskmanSetup;

/* The loaded mvopeningyoster.spr bank's head, for hosttest_ft.c. */
extern void *sMVOpeningYosterWallpaperFileHead;

void mvOpeningYosterMakeNest(void);
void mvOpeningYosterMakeFighters(void);
void mvOpeningYosterMakeWallpaper(void);
void mvOpeningYosterMakeGround(void);
void mvOpeningYosterMakeMainCamera(void);
void mvOpeningYosterMakeWallpaperCamera(void);
void mvOpeningYosterInitTotalTimeTics(void);
void mvOpeningYosterMainProc(GObj *gobj);
void mvOpeningYosterFuncStart(void);
void mvOpeningYosterStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[45]. */
void mvOpeningYosterOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGYOSTER_H */
