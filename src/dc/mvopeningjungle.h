/* mvopeningjungle.h -- the openings' fourteenth scene, mv/mvopening/
 * mvopeningjungle.c ported. Overlay 51.
 *
 * ... Cliff(46) -> Yamabuki(48) -> Jungle(51) -> Yoster(45) ... At tic 320
 * it hands off to nSCKindOpeningYoster, read from mvOpeningJungleFuncRun;
 * Yoster is unported, so scManagerRunScene's default arm loops this one
 * back.
 *
 * A real stage (Jungle Japes) with two key-driven fighters -- Donkey Kong
 * (damage 200, charged to level 9) and Samus (damage 40, level 6), each
 * given its own canned input -- watched by a stage camera that flies on
 * the file's script. No name card, no posed column: the whole screen is
 * the motion window.
 *
 * Every substitution mvopeningmario.h numbers 1-5 (incl. 3b) and
 * mvopeningdonkey.h's 6 applies again unchanged; the Zebes fixup of
 * mvopeningsamus.h's 7 does not recur.
 *
 * ---- Content differences and cuts:
 *
 *   - Files. IFCommonAnnounceCommon (the name letters) is not loaded: the
 *     scene draws none. File 64 (MVOpeningJungle) held only the camera's
 *     script, romdisk/mvopeningjungle.cam (tools/export/ssb_camanimexport.py
 *     --bank OpeningJungle), fetched with camanim_get(..., "Cam").
 *   - The default camera (FILLCOLOR) and gmCameraMakeWallpaperCamera are
 *     cut: the stage acquire is what brings the backdrop, and FILLCOLOR
 *     paints over the 3D lists here.
 *   - gmCameraMakeMovieCamera(NULL) has its camera_mask set after; its
 *     two unused CObjDesc copies are cut.
 *   - Overlay list: OVERLAY_BATTLE, OVERLAY_FIGHTING and
 *     OVERLAY_OPENINGJUNGLE (Substitution 6).
 */
#ifndef SSB_DC_MVOPENINGJUNGLE_H
#define SSB_DC_MVOPENINGJUNGLE_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

extern SYTaskmanSetup dMVOpeningJungleTaskmanSetup;

void mvOpeningJungleSetupFiles(void);
void mvOpeningJungleMakeGroundViewport(Vec3f unused);
void mvOpeningJungleMakeFighters(void);
void mvOpeningJungleFuncRun(GObj *gobj);
void mvOpeningJungleFuncStart(void);
void mvOpeningJungleStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[51]. */
void mvOpeningJungleOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGJUNGLE_H */
