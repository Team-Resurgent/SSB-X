/* mvopeningkirby.h -- the openings' eighth scene, mv/mvopening/mvopeningkirby.c
 * ported. Overlay 43.
 *
 * Room(34) -> Portraits(35) -> Mario(36) -> Donkey(37) -> Link(40) ->
 * Samus(38) -> Yoshi(41) -> Kirby(43) -> ... (see mvopeningroom.h for the
 * full chain). Same recipe as mvopeningmario.h ("tier 2"): a name card
 * holds 15 tics, then a posed fighter in a color-filled box appears beside
 * a real VS stage with the fighter walking under a canned key script.
 * At tic 60 it hands off to nSCKindOpeningFox (39) -- read directly from
 * mvOpeningKirbyFuncRun, not assumed from the numbering. Fox is unported,
 * so scManagerRunScene's default arm loops this scene back into itself.
 *
 * Every substitution mvopeningmario.h numbers 1-5 (incl. 3b) and
 * mvopeningdonkey.h's 6 applies again unchanged; Substitution 7 (Samus)
 * does not recur.
 *
 * ---- Content differences:
 *
 *   - Fighter nFTKindKirby, stage nGRKindPupupu.
 *   - Name card "KIRBY" (K/I/R/B/Y at x=90/125/140/170/200, y=100 -- a
 *     Vec2f table + 90/100 origin, not a pos_x[] table). Offsets
 *     0x2F98/0x26B8/0x5418/0x9A8/0x7608. No repeated letter.
 *   - Screen split is a left/right column split like Mario's: motion
 *     camera on the LEFT (10,10,210,230, aspect 10/11), posed box on the
 *     RIGHT (210,10,310,230, aspect 5/11), fill color (0x50,0xAA,0xFF).
 *   - Posed fighter moves translate.vec.f.y -= speed, desc.pos
 *     {0,600,0}, no desc.lr (as Yoshi).
 *   - Key events: STICK(45, MAX, 1) then a single A_BUTTON tap.
 *   - Motion mapobject offset: pos.y += 30 only. CObjDesc start
 *     {0,400,2000},{0,400,0}; end {1100,400,1800},{1100,400,0}.
 *   - camanim_get(..., "Kirby"), the 0x150 entry of mvopeningcommon.cam.
 *   - The decomp's Kirby arm uses literal 16/13 for the camera/movie link
 *     ids; the port keeps the named constants (same values).
 *   - Overlay list: OVERLAY_BATTLE, OVERLAY_FIGHTING and
 *     OVERLAY_OPENINGKIRBY (Substitution 6).
 */
#ifndef SSB_DC_MVOPENINGKIRBY_H
#define SSB_DC_MVOPENINGKIRBY_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* mvopeningkirby.c:1379-1421-shaped dMVOpeningKirbyTaskmanSetup,
 * DIVERGES the same shape mvopeningmario.h documents: the decomp's own
 * pool counts are a placeholder (zero), func_lights is NULL, and the DL
 * buffer/graphics-heap/RDP-output sizes are zeroed. The counts below
 * reuse mvopeningmario.h's own (one real VS stage, up to two real
 * fighters, both Kirby). */
extern SYTaskmanSetup dMVOpeningKirbyTaskmanSetup;

/* The loaded ifannounce.spr bank's head, for hosttest_ft.c's own
 * openingkirby_find (mvopeningkirby.c's own note by its definition) --
 * the same shape sMVOpeningMarioNamesFileHead/sMVOpeningDonkeyNamesFileHead/
 * sMVOpeningLinkNamesFileHead/sMVOpeningSamusNamesFileHead already
 * establish. */
extern void *sMVOpeningKirbyNamesFileHead;

void mvOpeningKirbySetupFiles(void);
void mvOpeningKirbyInitName(SObj *sobj);
void mvOpeningKirbyMakeName(void);
void mvOpeningKirbyMotionCameraProcUpdate(GObj *camera_gobj);
void mvOpeningKirbyMakeMotionCamera(Vec3f move);
void mvOpeningKirbyMakeMotionWindow(void);
void mvOpeningKirbyPosedWallpaperProcDisplay(GObj *gobj);
void mvOpeningKirbyMakePosedWallpaper(void);
void mvOpeningKirbyPosedFighterProcUpdate(GObj *fighter_gobj);
void mvOpeningKirbyMakePosedFighter(void);
void mvOpeningKirbyMakeNameCamera(void);
void mvOpeningKirbyMakePosedFighterCamera(void);
void mvOpeningKirbyMakePosedWallpaperCamera(void);
void mvOpeningKirbyFuncRun(GObj *gobj);
void mvOpeningKirbyInitVars(void);
void mvOpeningKirbyFuncStart(void);
void mvOpeningKirbyStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[43]. */
void mvOpeningKirbyOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGKIRBY_H */
