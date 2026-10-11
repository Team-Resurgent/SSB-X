/* mvopeningpikachu.h -- the openings' tenth scene,
 * mv/mvopening/mvopeningpikachu.c ported. Overlay 42.
 *
 * ... Kirby(43) -> Fox(39) -> Pikachu(42) -> ... Same recipe as
 * mvopeningmario.h ("tier 2"). At tic 60 it hands off to
 * nSCKindOpeningRun, read from mvOpeningPikachuFuncRun; Run is
 * unported, so scManagerRunScene's default arm loops this scene back.
 *
 * Every substitution mvopeningmario.h numbers 1-5 (incl. 3b) and
 * mvopeningdonkey.h's 6 applies again unchanged; Substitution 7 (Samus)
 * does not recur.
 *
 * ---- Content differences:
 *
 *   - Fighter nFTKindPikachu, stage nGRKindYamabuki (Saffron).
 *   - Name card "PIKACHU", seven letters at pos_x = {0,30,45,75,110,
 *     140,170} + 65, y=100. Offsets P/I/K/A/C/H/U =
 *     0x4890/0x26B8/0x2F98/0x5E0/0xD80/0x2408/0x60D8. No repeats.
 *   - Column split MIRRORED from Kirby's: motion camera on the RIGHT
 *     (110,10,310,230, aspect 10/11), posed box on the LEFT
 *     (10,10,110,230, aspect 5/11); fill (0x6E,0xAA,0x6E).
 *   - Posed fighter moves translate.vec.f.y += speed from {0,-600,0}
 *     (Donkey's direction); walking fighter desc.lr = +1.
 *   - The key script is empty (just FTKEY_EVENT_END): no input.
 *   - CObjDesc start {0,0,20000},{0,0,0}; end {50,-1640,1000},
 *     {50,-1640,0}. No mapobject offset.
 *   - camanim_get(..., "Pikachu"), the 0x120 entry of
 *     mvopeningcommon.cam.
 *   - Overlay list: OVERLAY_BATTLE, OVERLAY_FIGHTING and
 *     OVERLAY_OPENINGPIKACHU (Substitution 6).
 */
#ifndef SSB_DC_MVOPENINGPIKACHU_H
#define SSB_DC_MVOPENINGPIKACHU_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* mvopeningpikachu.c:1379-1421-shaped dMVOpeningPikachuTaskmanSetup,
 * DIVERGES the same shape mvopeningmario.h documents: the decomp's own
 * pool counts are a placeholder (zero), func_lights is NULL, and the DL
 * buffer/graphics-heap/RDP-output sizes are zeroed. The counts below
 * reuse mvopeningmario.h's own (one real VS stage, up to two real
 * fighters, both Pikachu). */
extern SYTaskmanSetup dMVOpeningPikachuTaskmanSetup;

/* The loaded ifannounce.spr bank's head, for hosttest_ft.c's own
 * openingpikachu_find (mvopeningpikachu.c's own note by its definition) --
 * the same shape sMVOpeningMarioNamesFileHead/sMVOpeningDonkeyNamesFileHead/
 * sMVOpeningLinkNamesFileHead/sMVOpeningSamusNamesFileHead already
 * establish. */
extern void *sMVOpeningPikachuNamesFileHead;

void mvOpeningPikachuSetupFiles(void);
void mvOpeningPikachuInitName(SObj *sobj);
void mvOpeningPikachuMakeName(void);
void mvOpeningPikachuMotionCameraProcUpdate(GObj *camera_gobj);
void mvOpeningPikachuMakeMotionCamera(Vec3f move);
void mvOpeningPikachuMakeMotionWindow(void);
void mvOpeningPikachuPosedWallpaperProcDisplay(GObj *gobj);
void mvOpeningPikachuMakePosedWallpaper(void);
void mvOpeningPikachuPosedFighterProcUpdate(GObj *fighter_gobj);
void mvOpeningPikachuMakePosedFighter(void);
void mvOpeningPikachuMakeNameCamera(void);
void mvOpeningPikachuMakePosedFighterCamera(void);
void mvOpeningPikachuMakePosedWallpaperCamera(void);
void mvOpeningPikachuFuncRun(GObj *gobj);
void mvOpeningPikachuInitVars(void);
void mvOpeningPikachuFuncStart(void);
void mvOpeningPikachuStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[42]. */
void mvOpeningPikachuOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGPIKACHU_H */
