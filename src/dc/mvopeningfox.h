/* mvopeningfox.h -- the openings' ninth scene, mv/mvopening/mvopeningfox.c
 * ported. Overlay 39.
 *
 * Room(34) -> Portraits(35) -> Mario(36) -> Donkey(37) -> Link(40) ->
 * Samus(38) -> Yoshi(41) -> Kirby(43) -> Fox(39) -> ... Same recipe as
 * mvopeningmario.h ("tier 2"). At tic 60 it hands off to
 * nSCKindOpeningPikachu (42), read from mvOpeningFoxFuncRun; Pikachu is
 * unported, so scManagerRunScene's default arm loops this scene back.
 *
 * Every substitution mvopeningmario.h numbers 1-5 (incl. 3b) and
 * mvopeningdonkey.h's 6 applies again unchanged; Substitution 7 (Samus)
 * does not recur.
 *
 * ---- Content differences (Kirby's scene is the closest sibling):
 *
 *   - Fighter nFTKindFox, stage nGRKindSector.
 *   - Name card "FOX" (F/O/X at x=110/140/185, y=100), offsets
 *     0x1A00/0x44B0/0x7108.
 *   - Same column split as Kirby's (motion left 10,10,210,230; posed
 *     right 210,10,310,230); fill color (0x00,0x3C,0x28).
 *   - The walking fighter spawns with desc.lr = -1 (siblings: +1), and
 *     the motion mapobject offset is unmodified (Kirby adds y += 30).
 *   - Posed fighter as Kirby's: y -= speed from {0,600,0}, no desc.lr.
 *   - Key events: STICK(-50,0,1), then two B taps each followed by a
 *     12-tic release.
 *   - CObjDesc start {-400,320,100},{0,320,0}; end {-3000,300,250},
 *     {0,300,-200}, up 0.7.
 *   - camanim_get(..., "Fox"), the 0x90 entry of mvopeningcommon.cam.
 *   - Overlay list: OVERLAY_BATTLE, OVERLAY_FIGHTING and
 *     OVERLAY_OPENINGFOX (Substitution 6).
 */
#ifndef SSB_DC_MVOPENINGFOX_H
#define SSB_DC_MVOPENINGFOX_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* mvopeningfox.c:1379-1421-shaped dMVOpeningFoxTaskmanSetup,
 * DIVERGES the same shape mvopeningmario.h documents: the decomp's own
 * pool counts are a placeholder (zero), func_lights is NULL, and the DL
 * buffer/graphics-heap/RDP-output sizes are zeroed. The counts below
 * reuse mvopeningmario.h's own (one real VS stage, up to two real
 * fighters, both Fox). */
extern SYTaskmanSetup dMVOpeningFoxTaskmanSetup;

/* The loaded ifannounce.spr bank's head, for hosttest_ft.c's own
 * openingfox_find (mvopeningfox.c's own note by its definition) --
 * the same shape sMVOpeningMarioNamesFileHead/sMVOpeningDonkeyNamesFileHead/
 * sMVOpeningLinkNamesFileHead/sMVOpeningSamusNamesFileHead already
 * establish. */
extern void *sMVOpeningFoxNamesFileHead;

void mvOpeningFoxSetupFiles(void);
void mvOpeningFoxInitName(SObj *sobj);
void mvOpeningFoxMakeName(void);
void mvOpeningFoxMotionCameraProcUpdate(GObj *camera_gobj);
void mvOpeningFoxMakeMotionCamera(Vec3f move);
void mvOpeningFoxMakeMotionWindow(void);
void mvOpeningFoxPosedWallpaperProcDisplay(GObj *gobj);
void mvOpeningFoxMakePosedWallpaper(void);
void mvOpeningFoxPosedFighterProcUpdate(GObj *fighter_gobj);
void mvOpeningFoxMakePosedFighter(void);
void mvOpeningFoxMakeNameCamera(void);
void mvOpeningFoxMakePosedFighterCamera(void);
void mvOpeningFoxMakePosedWallpaperCamera(void);
void mvOpeningFoxFuncRun(GObj *gobj);
void mvOpeningFoxInitVars(void);
void mvOpeningFoxFuncStart(void);
void mvOpeningFoxStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[39]. */
void mvOpeningFoxOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGFOX_H */
