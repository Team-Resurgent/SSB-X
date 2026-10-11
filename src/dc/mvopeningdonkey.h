/* mvopeningdonkey.h -- the openings' fourth scene, mv/mvopening/mvopeningdonkey.c
 * ported. Overlay 37.
 *
 * Room(34) -> Portraits(35) -> Mario(36) -> Donkey(37) -> ... (see
 * mvopeningroom.h for the full nineteen-scene chain). This is the second
 * of the eight per-fighter opening scenes (mvopeningmario.h's "tier 2"),
 * and reuses that scene's whole recipe verbatim: a name card holds for
 * 15 tics over a black screen, then it is ejected and three things
 * appear at once -- a posed fighter in a color-filled box, and beside it
 * a real VS stage with the same fighter walking toward the camera under
 * a canned FTKeyEvent script. At tic 60 it hands off to
 * nSCKindOpeningLink -- unported, so scManagerRunScene's default arm
 * loops this scene back into itself, the same reading every scene above
 * it in the chain already establishes. (Not nSCKindOpeningSamus or
 * nSCKindOpeningFox, even though those sit between Donkey and Link in
 * sc/scdef.h's own enum order and in the overlay numbering -- checked
 * directly against the decomp source, not assumed from the enum's
 * declaration order: mvOpeningDonkeyFuncRun's own tic-60 arm reads
 * `scene_curr = nSCKindOpeningLink` verbatim. The enum's declaration
 * order is not the chain's runtime order.)
 *
 * Only what differs from mvopeningmario.h's own recipe is documented
 * here; every substitution mario.h numbers 1-5 applies again, at the
 * same call sites, for the same reasons, and is not repeated.
 *
 * ---- What actually differs from Mario's scene (content, not recipe):
 *
 *   - Fighter nFTKindDonkey, stage nGRKindJungle (not nFTKindMario /
 *     nGRKindCastle).
 *   - The name card: REGION_US spells just "DK" (two letters, D then
 *     K, at x=120/160, y=100), not the five-letter "MARIO" Mario's
 *     scene spells -- checked against the decomp's own
 *     `#if defined (REGION_US)` arm in mvOpeningDonkeyMakeName, which
 *     this port takes (dMVOpeningDonkeyVideoSetup and every other file
 *     in this chain already builds -DREGION_US, src/game/ssb64/
 *     Makefile's own DECOMP_DEFS). The `#else` arm (ten letters,
 *     "DONKEYKONG" in two rows) is not ported, the same "one region"
 *     choice this whole build already makes everywhere else.
 *   - The screen is mirrored left-to-right relative to Mario's own
 *     layout: Mario puts the posed-fighter/wallpaper box on the LEFT
 *     third (x 10-110) and the real stage on the RIGHT two-thirds (x
 *     110-310); Donkey puts the posed-fighter/wallpaper box on the
 *     RIGHT third (x 210-310) and the real stage on the LEFT
 *     two-thirds (x 10-210) -- checked against
 *     mvOpeningDonkeyMakeNameCamera/MakePosedFighterCamera/
 *     MakePosedWallpaperCamera/MakeMotionCamera's own syRdpSetViewport
 *     calls, all mirrored from Mario's own. This is exactly the
 *     multiple-real-camera-different-viewport shape that the
 *     general fix (src/dc/objdisplay.c's viewport union,
 *     gcResetViewportUnion/gcExpandViewportUnion/gcGetViewportUnion)
 *     exists for -- no further change needed here, it was built to
 *     cover every such scene, not just Mario's own.
 *   - The posed-wallpaper fill color is a dark green
 *     (0x46, 0x5A, 0x00), not Mario's pale blue (0xA0, 0xAA, 0xFF) --
 *     a jungle backdrop tint, kept verbatim from the decomp.
 *   - dMVOpeningDonkeyCObjDescStart/End, dMVOpeningDonkeyKeyEvents, and
 *     the posed fighter's own y-position/translate sign
 *     (+600/-600, += speed vs Mario's -= speed) all differ, and are
 *     each kept verbatim from the decomp per-scene, the same as any
 *     two sibling scenes in this chain naturally differ in their own
 *     canned motion data.
 *   - The walking fighter's `desc.lr` is -1 here (Mario's is +1) --
 *     verbatim from each scene's own decomp source, presumably a
 *     stage-facing choice per fighter's spawn point.
 *   - camanim_get(&sMVOpeningDonkeyCamAnimBank, "Donkey") in place of
 *     "Mario" -- tools/export/ssb_camanimexport.py's own OpeningCommon table
 *     already carries a "Donkey" entry (offset 0x30 in the same
 *     romdisk/mvopeningcommon.cam file Mario's own scene reads), so no new
 *     export work is needed.
 *
 * ---- Substitution 6 (new here, beyond mario.h's 1-5): the overlay
 * load list.
 *
 * The decomp's own case nSCKindOpeningDonkey arm (sc/scmanager.c:1086-
 * 1089) loads only overlay 37 (itself) -- not overlay 2 or 3, unlike
 * Mario's own arm (which loads 3 and 36). That is because the real
 * console reaches Donkey only by falling straight through from Mario
 * within the same continuous run, where overlays 2 and 3 are already
 * resident; nothing in the chain ever unloads them in between. This
 * port's own case nSCKindOpeningMario arm already takes a liberty
 * beyond even the decomp's own list for exactly this reason (loading
 * OVERLAY_BATTLE, which decomp's own Mario arm does not ask for
 * either) -- "the same deliberate 'extra re-zero, costs nothing'
 * liberty room's own arm already takes," per that arm's own comment.
 * This scene's own port arm takes the same liberty one step further:
 * OVERLAY_BATTLE, OVERLAY_FIGHTING, and OVERLAY_OPENINGDONKEY, all
 * three, so a probe that boots directly into nSCKindOpeningDonkey via
 * DB_BOOT_SCENE (bypassing Mario's own scene entirely) still gets a
 * freshly zeroed fighter/stage/camera pool rather than depending on
 * whatever scene happened to run immediately before it.
 *
 * The standard cuts, as everywhere else: syVideoInit/SYVideoSetup/the
 * zbuffer allocation and arena_size -- mvOpeningDonkeyStartScene calls
 * syTaskmanStartTask directly, the same as every scene in this chain.
 *
 * Overlay 37 (ovl37_BSS_END), confirmed free in src/dc/overlay.h's own
 * numbering.
 */
#ifndef SSB_DC_MVOPENINGDONKEY_H
#define SSB_DC_MVOPENINGDONKEY_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* mvopeningdonkey.c:1379-1421-shaped dMVOpeningDonkeyTaskmanSetup,
 * DIVERGES the same shape mvopeningmario.h documents: the decomp's own
 * pool counts are a placeholder (zero), func_lights is NULL, and the DL
 * buffer/graphics-heap/RDP-output sizes are zeroed. The counts below
 * reuse mvopeningmario.h's own (one real VS stage, up to two real
 * fighters, both Donkey) rather than recompute narrower figures for a
 * two-letter name card instead of a five-letter one. */
extern SYTaskmanSetup dMVOpeningDonkeyTaskmanSetup;

/* The loaded ifannounce.spr bank's head, for hosttest_ft.c's own
 * openingdonkey_find (mvopeningdonkey.c's own note by its
 * definition) -- the same shape sMVOpeningMarioNamesFileHead already
 * establishes. */
extern void *sMVOpeningDonkeyNamesFileHead;

void mvOpeningDonkeySetupFiles(void);
void mvOpeningDonkeyInitName(SObj *sobj);
void mvOpeningDonkeyMakeName(void);
void mvOpeningDonkeyMotionCameraProcUpdate(GObj *camera_gobj);
void mvOpeningDonkeyMakeMotionCamera(Vec3f move);
void mvOpeningDonkeyMakeMotionWindow(void);
void mvOpeningDonkeyPosedWallpaperProcDisplay(GObj *gobj);
void mvOpeningDonkeyMakePosedWallpaper(void);
void mvOpeningDonkeyPosedFighterProcUpdate(GObj *fighter_gobj);
void mvOpeningDonkeyMakePosedFighter(void);
void mvOpeningDonkeyMakeNameCamera(void);
void mvOpeningDonkeyMakePosedFighterCamera(void);
void mvOpeningDonkeyMakePosedWallpaperCamera(void);
void mvOpeningDonkeyFuncRun(GObj *gobj);
void mvOpeningDonkeyInitVars(void);
void mvOpeningDonkeyFuncStart(void);
void mvOpeningDonkeyStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[37]. */
void mvOpeningDonkeyOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGDONKEY_H */
