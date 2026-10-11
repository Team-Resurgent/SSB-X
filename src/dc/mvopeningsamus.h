/* mvopeningsamus.h -- the openings' sixth scene, mv/mvopening/mvopeningsamus.c
 * ported. Overlay 38.
 *
 * Room(34) -> Portraits(35) -> Mario(36) -> Donkey(37) -> Link(40) ->
 * Samus(38) -> ... (see mvopeningroom.h for the full nineteen-scene
 * chain). This is the fourth of the eight per-fighter opening scenes
 * (mvopeningmario.h's "tier 2"), and reuses that scene's whole recipe:
 * a name card holds for 15 tics over a black screen, then it is ejected
 * and three things appear at once -- a posed fighter in a color-filled
 * box, and beside it a real VS stage with the same fighter walking
 * toward the camera under a canned FTKeyEvent script. At tic 60 it
 * hands off to nSCKindOpeningYoshi -- unported, so scManagerRunScene's
 * default arm loops this scene back into itself, the same reading every
 * scene above it in the chain already establishes. (Not
 * nSCKindOpeningFox, even though Fox's own overlay (39) sits between
 * Samus's (38) and Link's (40): checked directly against the decomp
 * source, not assumed -- mvOpeningSamusFuncRun's own tic-60 arm reads
 * `scene_curr = nSCKindOpeningYoshi` verbatim (38 -> 41). Link hands
 * off to Samus (40 -> 38, mvopeninglink.h's own note), and Samus hands
 * off to Yoshi, skipping Fox entirely again -- the same "the chain
 * doubles back through the numbering, only each scene's own FuncRun
 * says what plays next" reading mvopeninglink.h already establishes.)
 *
 * Only what differs from mvopeningmario.h's own recipe is documented
 * here; every substitution mario.h numbers 1-5 applies again, at the
 * same call sites, for the same reasons (including Substitution 3b),
 * and is not repeated. mvopeningdonkey.h's Substitution 6 (the overlay
 * load list) also applies again, unchanged in shape.
 *
 * ---- What actually differs from Mario's scene (content, not recipe):
 *
 *   - Fighter nFTKindSamus, stage nGRKindZebes (not nFTKindMario /
 *     nGRKindCastle).
 *   - The name card spells "SAMUS" (five letters, S/A/M/U/S at
 *     x=80/110/150/190/220, y=100, i.e. pos_x={0,30,70,110,140}+80.0F)
 *     -- no region branch, the same unbranched five-letter shape
 *     Mario's own card already has. The first and last letters are
 *     both "S", read from the same relocData offset twice.
 *   - The screen split is numerically identical to Mario's own: the
 *     posed-fighter/wallpaper box on the LEFT third (x 10-110) and the
 *     real stage on the RIGHT two-thirds (x 110-310) -- checked against
 *     mvOpeningSamusMakeMotionCamera (110,10,310,230, aspect 10/11) and
 *     MakePosedFighterCamera/MakePosedWallpaperCamera (both
 *     10,10,110,230), all matching Mario's own syRdpSetViewport calls
 *     exactly (unlike Donkey's mirrored columns or Link's row split).
 *   - The posed-wallpaper fill color is a dark navy
 *     (0x00, 0x00, 0x50), a Zebes cave/space tint, kept verbatim from
 *     the decomp. Its fill rectangle is (10, 10, 110, 230), matching
 *     the left-column split above.
 *   - The posed fighter moves along Y exactly like Mario's own
 *     (translate.vec.f.y -= speed, pos.y=600.0F, desc.lr=+1) -- not
 *     Donkey's mirrored +=/-600/-1.
 *   - The decomp's own gcMakeCameraGObj/gcMakeGObjSPAfter calls use the
 *     named constants nGCCommonLinkIDSceneCamera/nGCCommonLinkIDMovie in
 *     place of the literal 16/13 Mario's/Donkey's own decomp source
 *     writes at the same call sites -- the same choice
 *     mvopeninglink.h's own note already makes for the same reason
 *     (costs nothing, matches what this scene's own decomp source
 *     literally writes).
 *   - camanim_get(&sMVOpeningSamusCamAnimBank, "Samus") in place of
 *     "Mario"/"Donkey"/"Link" -- tools/export/ssb_camanimexport.py's own
 *     OpeningCommon table already carries a "Samus" entry (offset 0x60
 *     in the same romdisk/mvopeningcommon.cam file the whole chain
 *     reads), so no new export work is needed.
 *   - The name-letter offsets are file 37's own S/A/M/U entries
 *     (src/dc/decomp/reloc_data.us.h: 0x57F0/0x5E0/0x3980/0x60D8) -- A
 *     and M are the exact literals Mario's own file already uses for
 *     its own "A"/"M"; S and U are new to this scene.
 *   - mvOpeningSamusFuncStart calls scSubsysFighterSetLightParams(45.0F,
 *     45.0F, 0xFF, 0xFF, 0xFF, 0xFF) (right before
 *     ftManagerSetupFilesAllKind, the decomp's own position), a call
 *     none of Mario's/Donkey's/Link's own FuncStart makes -- already a
 *     ported, already-called-elsewhere function (src/dc/ftmanager.c,
 *     src/dc/mnplayersvs.c, src/dc/mvopeningroom.c among others), so it
 *     is kept verbatim, at the same call site, with no substitution
 *     needed.
 *
 * ---- Substitution 7 (new here, beyond mario.h's 1-5 and donkey.h's
 * 6): the Zebes spotlight DObj-tree fixup, cut.
 *
 * mvOpeningSamusMakeMotionWindow's own decomp body walks
 * gGRCommonLayerGObjs[1]'s whole DObj tree (via
 * lbCommonGetTreeDObjNextFromRoot) right after grCommonSetupInitAll(),
 * rewriting every XObj of kind 0x30 to kind 0x25 -- the decomp's own
 * comment names it outright: "This fixes the spot light things on
 * Zebes." This has no port-side equivalent to act on:
 * src/dc/stage.h's own grCommonSetupInitAll returns one merged GObj,
 * not the decomp's four-entry gGRCommonLayerGObjs[] layer array (stage.h's
 * own DIVERGES note on the function already says why), so there is no
 * "layer 1" DObj tree here to walk in the first place. Cut outright,
 * along with the loop's own locals (root_dobj/next_dobj/j/unused) --
 * the same "no hook to act through" reasoning src/dc/stage.c's own
 * per-stage DIVERGES notes already use elsewhere for game-specific
 * fixups the port's simplified ground model has no matching object for.
 * grWallpaperMakeDecideKind()/grCommonSetupInitAll() themselves still
 * move to after mvOpeningSamusMakeMotionCamera(), the ordinary
 * Substitution 3 reordering -- only the spotlight loop between them is
 * new and cut.
 *
 * ---- Substitution 6 reused: the overlay load list.
 *
 * The decomp's own case nSCKindOpeningSamus arm (sc/scmanager.c:1091-
 * 1094) loads only overlay 38 (itself) -- the same "falls through with
 * battle/fighting already resident" shape Donkey's and Link's own arms
 * have, so this port's own arm takes the same liberty: OVERLAY_BATTLE,
 * OVERLAY_FIGHTING, and OVERLAY_OPENINGSAMUS.
 *
 * The standard cuts, as everywhere else: syVideoInit/SYVideoSetup/the
 * zbuffer allocation and arena_size -- mvOpeningSamusStartScene calls
 * syTaskmanStartTask directly, the same as every scene in this chain.
 *
 * Overlay 38 (ovl38_BSS_END), confirmed free in src/dc/overlay.h's own
 * numbering, and confirmed (sc/scmanager.c:1091-1094) to be the only
 * overlay decomp's own nSCKindOpeningSamus arm loads.
 */
#ifndef SSB_DC_MVOPENINGSAMUS_H
#define SSB_DC_MVOPENINGSAMUS_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* mvopeningsamus.c:1379-1421-shaped dMVOpeningSamusTaskmanSetup,
 * DIVERGES the same shape mvopeningmario.h documents: the decomp's own
 * pool counts are a placeholder (zero), func_lights is NULL, and the DL
 * buffer/graphics-heap/RDP-output sizes are zeroed. The counts below
 * reuse mvopeningmario.h's own (one real VS stage, up to two real
 * fighters, both Samus). */
extern SYTaskmanSetup dMVOpeningSamusTaskmanSetup;

/* The loaded ifannounce.spr bank's head, for hosttest_ft.c's own
 * openingsamus_find (mvopeningsamus.c's own note by its definition) --
 * the same shape sMVOpeningMarioNamesFileHead/sMVOpeningDonkeyNamesFileHead/
 * sMVOpeningLinkNamesFileHead already establish. */
extern void *sMVOpeningSamusNamesFileHead;

void mvOpeningSamusSetupFiles(void);
void mvOpeningSamusInitName(SObj *sobj);
void mvOpeningSamusMakeName(void);
void mvOpeningSamusMotionCameraProcUpdate(GObj *camera_gobj);
void mvOpeningSamusMakeMotionCamera(Vec3f move);
void mvOpeningSamusMakeMotionWindow(void);
void mvOpeningSamusPosedWallpaperProcDisplay(GObj *gobj);
void mvOpeningSamusMakePosedWallpaper(void);
void mvOpeningSamusPosedFighterProcUpdate(GObj *fighter_gobj);
void mvOpeningSamusMakePosedFighter(void);
void mvOpeningSamusMakeNameCamera(void);
void mvOpeningSamusMakePosedFighterCamera(void);
void mvOpeningSamusMakePosedWallpaperCamera(void);
void mvOpeningSamusFuncRun(GObj *gobj);
void mvOpeningSamusInitVars(void);
void mvOpeningSamusFuncStart(void);
void mvOpeningSamusStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[38]. */
void mvOpeningSamusOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGSAMUS_H */
