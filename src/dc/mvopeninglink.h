/* mvopeninglink.h -- the openings' fifth scene, mv/mvopening/mvopeninglink.c
 * ported. Overlay 40.
 *
 * Room(34) -> Portraits(35) -> Mario(36) -> Donkey(37) -> Link(40) -> ...
 * (see mvopeningroom.h for the full nineteen-scene chain). This is the
 * third of the eight per-fighter opening scenes (mvopeningmario.h's
 * "tier 2"), and reuses that scene's whole recipe: a name card holds
 * for 15 tics over a black screen, then it is ejected and three things
 * appear at once -- a posed fighter in a color-filled box, and beside
 * it a real VS stage with the same fighter walking toward the camera
 * under a canned FTKeyEvent script. At tic 60 it hands off to
 * nSCKindOpeningSamus -- unported, so scManagerRunScene's default arm
 * loops this scene back into itself, the same reading every scene
 * above it in the chain already establishes. (Not nSCKindOpeningDonkey
 * -- the scene that hands off TO this one -- and not skipping ahead to
 * nSCKindOpeningFox either: checked directly against the decomp
 * source, not assumed from sc/scdef.h's enum order or the overlay
 * numbering (37/38/39/40 for Donkey/Samus/Fox/Link): Donkey hands
 * straight to Link (37 -> 40, skipping 38/39), and Link's own tic-60
 * arm reads `scene_curr = nSCKindOpeningSamus` verbatim (40 -> 38) --
 * the chain doubles back through the numbering rather than continuing
 * to climb it. The enum's declaration order and the overlay numbering
 * are both just the catalogue order; only each scene's own FuncRun
 * hand-off says what actually plays next.)
 *
 * Only what differs from mvopeningmario.h's own recipe is documented
 * here; every substitution mario.h numbers 1-5 applies again, at the
 * same call sites, for the same reasons (including Substitution 3b,
 * efManagerInitEffects -> efManagerLoadEffectBank+efManagerPreloadModels,
 * since this scene also carries a real fighter pack alongside a real
 * stage), and is not repeated. mvopeningdonkey.h's Substitution 6 (the
 * overlay load list) also applies again, unchanged in shape.
 *
 * ---- What actually differs from Mario's scene (content, not recipe):
 *
 *   - Fighter nFTKindLink, stage nGRKindHyrule (not nFTKindMario /
 *     nGRKindCastle).
 *   - The name card spells "LINK" (four letters, L/I/N/K at
 *     x=100/130/145/180, y=100) -- no region branch in the decomp's
 *     own mvOpeningLinkMakeName, unlike Donkey's two-arm
 *     `#if defined(REGION_US)` split (mvopeningdonkey.h's own note).
 *     Link's card is more like Mario's own unbranched five-letter one.
 *   - The screen split is horizontal (rows), not vertical (columns)
 *     like Mario's/Donkey's: the posed-fighter/wallpaper box occupies
 *     the TOP band (x 10-310, y 10-90, full width and short), and the
 *     real stage occupies the BOTTOM two-thirds (x 10-310, y 90-230,
 *     full width) -- checked against mvOpeningLinkMakeNameCamera (full
 *     10,10,310,230 -- same shape as Mario's/Donkey's own name
 *     camera), mvOpeningLinkMakePosedFighterCamera/
 *     MakePosedWallpaperCamera (both 10,10,310,90) and
 *     mvOpeningLinkMakeMotionCamera (10,90,310,230), all in the
 *     decomp's own syRdpSetViewport calls. This is a THIRD distinct
 *     multiple-real-camera-different-viewport shape (after Mario's
 *     left/stage-right and Donkey's mirrored stage-left/right) that
 *     the viewport-union fix (src/dc/objdisplay.c's
 *     gcResetViewportUnion/gcExpandViewportUnion/gcGetViewportUnion)
 *     already covers -- it makes no assumption about which axis the
 *     split runs along, so no further change is needed here either;
 *     the disc probe below is this scene's own empirical confirmation
 *     of that, same as Donkey's was for the mirrored-column case.
 *   - The posed fighter moves along X, not Y: Mario's/Donkey's posed
 *     fighters slide vertically in their tall column boxes
 *     (translate.vec.f.y); Link's slides horizontally in its wide, short
 *     row box (translate.vec.f.x -= speed), starting at pos.x=600.0F
 *     rather than a nonzero pos.y -- both verbatim from the decomp, and
 *     both make sense given each box's own aspect ratio.
 *   - dMVOpeningLinkCObjDescStart/End, dMVOpeningLinkKeyEvents, and the
 *     walking fighter's own `desc.lr = +1` (same sign as Mario's, unlike
 *     Donkey's -1) all differ/agree per the decomp's own per-scene data,
 *     kept verbatim.
 *   - The decomp's own gcMakeCameraGObj calls use the named constants
 *     nGCCommonLinkIDSceneCamera/nGCCommonLinkIDMovie/nGCCommonLinkIDCamera
 *     in place of the literal 16/13/100 Mario's and Donkey's own decomp
 *     source write at the same call sites -- confirmed the same numeric
 *     values (16/13, sys/objdef.h; the FILLCOLOR camera's own 100 is
 *     moot, see below) via src/dc/gmcamera.c's/src/dc/mncongra.c's
 *     existing use of the same enum, already compiled into this build.
 *     Kept as the named constants here since that is what this scene's
 *     own decomp source literally writes and it costs nothing.
 *   - The posed-wallpaper fill color is a lavender/purple
 *     (0x96, 0x78, 0xB4), not Mario's pale blue (0xA0, 0xAA, 0xFF) or
 *     Donkey's dark green (0x46, 0x5A, 0x00) -- a Hyrule Castle tint,
 *     kept verbatim from the decomp. Its fill rectangle is
 *     (10, 10, 310, 90), matching the horizontal split above.
 *   - camanim_get(&sMVOpeningLinkCamAnimBank, "Link") in place of
 *     "Mario"/"Donkey" -- tools/export/ssb_camanimexport.py's own
 *     OpeningCommon table already carries a "Link" entry (offset 0xC0
 *     in the same romdisk/mvopeningcommon.cam file the whole chain
 *     reads), so no new export work is needed.
 *   - The name-letter offsets are file 37's own L/I/N/K entries
 *     (src/dc/decomp/reloc_data.us.h: 0x3358/0x26B8/0x3E88/0x2F98) --
 *     the I offset is the exact literal Mario's own file already uses
 *     for its own "I", and the K offset is the exact literal Donkey's
 *     own file already uses for its own "K" -- same relocData file,
 *     same sprite bank, read at a different offset per letter.
 *
 * The standard cuts, as everywhere else: syVideoInit/SYVideoSetup/the
 * zbuffer allocation and arena_size -- mvOpeningLinkStartScene calls
 * syTaskmanStartTask directly, the same as every scene in this chain.
 *
 * Overlay 40 (ovl40_BSS_END), confirmed free in src/dc/overlay.h's own
 * numbering, and confirmed (sc/scmanager.c:1101-1104) to be the only
 * overlay decomp's own nSCKindOpeningLink arm loads -- the same "falls
 * through with 2/3 already resident" shape Donkey's own arm has, so
 * this port's own arm takes mvopeningdonkey.h's Substitution 6 liberty
 * again: OVERLAY_BATTLE, OVERLAY_FIGHTING, and OVERLAY_OPENINGLINK.
 */
#ifndef SSB_DC_MVOPENINGLINK_H
#define SSB_DC_MVOPENINGLINK_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* mvopeninglink.c:1379-1421-shaped dMVOpeningLinkTaskmanSetup,
 * DIVERGES the same shape mvopeningmario.h documents: the decomp's own
 * pool counts are a placeholder (zero), func_lights is NULL, and the DL
 * buffer/graphics-heap/RDP-output sizes are zeroed. The counts below
 * reuse mvopeningmario.h's own (one real VS stage, up to two real
 * fighters, both Link) rather than recompute narrower figures for a
 * four-letter name card instead of a five-letter one. */
extern SYTaskmanSetup dMVOpeningLinkTaskmanSetup;

/* The loaded ifannounce.spr bank's head, for hosttest_ft.c's own
 * openinglink_find (mvopeninglink.c's own note by its definition) --
 * the same shape sMVOpeningMarioNamesFileHead/sMVOpeningDonkeyNamesFileHead
 * already establish. */
extern void *sMVOpeningLinkNamesFileHead;

void mvOpeningLinkSetupFiles(void);
void mvOpeningLinkInitName(SObj *sobj);
void mvOpeningLinkMakeName(void);
void mvOpeningLinkMotionCameraProcUpdate(GObj *camera_gobj);
void mvOpeningLinkMakeMotionCamera(Vec3f move);
void mvOpeningLinkMakeMotionWindow(void);
void mvOpeningLinkPosedWallpaperProcDisplay(GObj *gobj);
void mvOpeningLinkMakePosedWallpaper(void);
void mvOpeningLinkPosedFighterProcUpdate(GObj *fighter_gobj);
void mvOpeningLinkMakePosedFighter(void);
void mvOpeningLinkMakeNameCamera(void);
void mvOpeningLinkMakePosedFighterCamera(void);
void mvOpeningLinkMakePosedWallpaperCamera(void);
void mvOpeningLinkFuncRun(GObj *gobj);
void mvOpeningLinkInitVars(void);
void mvOpeningLinkFuncStart(void);
void mvOpeningLinkStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[40]. */
void mvOpeningLinkOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGLINK_H */
