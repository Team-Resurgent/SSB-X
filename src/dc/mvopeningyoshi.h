/* mvopeningyoshi.h -- the openings' seventh scene, mv/mvopening/mvopeningyoshi.c
 * ported. Overlay 41.
 *
 * Room(34) -> Portraits(35) -> Mario(36) -> Donkey(37) -> Link(40) ->
 * Samus(38) -> Yoshi(41) -> ... (see mvopeningroom.h for the full
 * nineteen-scene chain). This is the fifth of the eight per-fighter
 * opening scenes (mvopeningmario.h's "tier 2"), and reuses that scene's
 * whole recipe: a name card holds for 15 tics over a black screen, then
 * it is ejected and three things appear at once -- a posed fighter in a
 * color-filled box, and beside it a real VS stage with the same fighter
 * walking toward the camera under a canned FTKeyEvent script. At tic 60
 * it hands off to nSCKindOpeningKirby -- unported, so scManagerRunScene's
 * default arm loops this scene back into itself, the same reading every
 * scene above it in the chain already establishes. (Not
 * nSCKindOpeningPikachu, even though Pikachu's own overlay (42) sits
 * between Yoshi's (41) and Kirby's (43): checked directly against the
 * decomp source, not assumed -- mvOpeningYoshiFuncRun's own tic-60 arm
 * reads `scene_curr = nSCKindOpeningKirby` verbatim (41 -> 43). Samus
 * hands off to Yoshi (38 -> 41, mvopeningsamus.h's own note), and Yoshi
 * hands off to Kirby, skipping Pikachu entirely -- the same "the chain
 * doubles back through the numbering, only each scene's own FuncRun says
 * what plays next" reading mvopeninglink.h/mvopeningsamus.h already
 * establish.)
 *
 * Only what differs from mvopeningmario.h's own recipe is documented
 * here; every substitution mario.h numbers 1-5 applies again, at the
 * same call sites, for the same reasons (including Substitution 3b),
 * and is not repeated. mvopeningdonkey.h's Substitution 6 (the overlay
 * load list) also applies again, unchanged in shape. Unlike Samus, this
 * scene's own decomp source carries no stage-specific fixup beyond the
 * shared recipe -- Substitution 7 (mvopeningsamus.h) does not recur
 * here.
 *
 * ---- What actually differs from Mario's scene (content, not recipe):
 *
 *   - Fighter nFTKindYoshi, stage nGRKindYoster (not nFTKindMario /
 *     nGRKindCastle).
 *   - The name card spells "YOSHI" (five letters, Y/O/S/H/I at
 *     x=80/110/145/175/208, i.e. pos_x={0,30,65,95,128}+80.0F, y=100).
 *     No region branch, no repeated letter this time (unlike Samus's
 *     "SAMUS").
 *   - The screen split is a NEW shape, not matching Mario's left/right
 *     column split, Donkey's mirror, or Link's row split at Link's own
 *     boundary: the motion camera (mvOpeningYoshiMakeMotionCamera) takes
 *     the TOP two-thirds, full width (10,10,310,150, aspect 15/7); the
 *     posed-fighter/wallpaper box (MakePosedFighterCamera/
 *     MakePosedWallpaperCamera) takes the BOTTOM third, also full width
 *     (10,150,310,230, aspect 26.25/7 for the fighter camera) -- the
 *     same row-split idea Link's scene already uses, but flipped (motion
 *     on top here, on bottom there) and at a different boundary (y=150
 *     here, y=90 there). Checked directly against this scene's own
 *     syRdpSetViewport calls, not assumed from a sibling's shape.
 *   - The posed-wallpaper fill color is an orange/tan
 *     (0xFF, 0xBE, 0x5A), a Yoshi's Island tint, kept verbatim from the
 *     decomp. Its fill rectangle is (10, 150, 310, 230), matching the
 *     bottom-row split above.
 *   - The posed fighter moves along X (translate.vec.f.x +=), not Y --
 *     a third axis/sign combination beyond Mario's own (Y -=) and
 *     Donkey's mirrored one (Y +=, on -600.0F/-1 desc.pos/lr): checked
 *     directly against mvOpeningYoshiPosedFighterProcUpdate, kept
 *     verbatim. mvOpeningYoshiMakePosedFighter's own desc.pos is
 *     {-600.0F, 0.0F, 0.0F} (no desc.lr assignment at all -- the
 *     decomp's own default from dFTManagerDefaultFighterDesc is used
 *     unmodified, unlike every sibling scene so far which does set it).
 *   - dMVOpeningYoshiKeyEvents carries one more entry than every sibling
 *     scene's own key script: a leading
 *     FTKEY_EVENT_STICK(I_CONTROLLER_RANGE_MAX, 0, 20) before the same
 *     Z_TRIG/A_BUTTON tap pair Mario/Donkey/Link/Samus all use.
 *     FTKEY_EVENT_STICK is already a ported macro (src/dc/scexplain.c's
 *     own canned scripts use it extensively), so this needs no
 *     substitution -- kept verbatim.
 *   - The decomp's own gcMakeCameraGObj/gcMakeGObjSPAfter calls use the
 *     named constants nGCCommonLinkIDSceneCamera/nGCCommonLinkIDMovie,
 *     the same choice mvopeninglink.h/mvopeningsamus.h's own notes
 *     already make for the same reason.
 *   - camanim_get(&sMVOpeningYoshiCamAnimBank, "Yoshi") in place of
 *     "Mario"/"Donkey"/"Link"/"Samus" -- tools/export/ssb_camanimexport.py's
 *     own OpeningCommon table already carries a "Yoshi" entry (offset 0xF0
 *     in the same romdisk/mvopeningcommon.cam file the whole chain
 *     reads), so no new export work is needed.
 *   - The name-letter offsets are file 37's own Y/O/S/H/I entries
 *     (src/dc/decomp/reloc_data.us.h: 0x7608/0x44B0/0x57F0/0x2408/0x26B8)
 *     -- S is the exact literal Samus's own file already uses for its
 *     own "S"; Y/O/H/I are new to this scene.
 *   - mvOpeningYoshiFuncStart makes no scSubsysFighterSetLightParams
 *     call -- Samus's own addition (mvopeningsamus.h's own note) does
 *     not recur here; this scene's own decomp source matches Mario's
 *     shape exactly at this call site.
 *
 * ---- Substitution 6 reused: the overlay load list.
 *
 * The decomp's own case nSCKindOpeningYoshi arm (sc/scmanager.c:1106-
 * 1109) loads only overlay 41 (itself) -- the same "falls through with
 * battle/fighting already resident" shape Donkey's/Link's/Samus's own
 * arms have, so this port's own arm takes the same liberty:
 * OVERLAY_BATTLE, OVERLAY_FIGHTING, and OVERLAY_OPENINGYOSHI.
 *
 * The standard cuts, as everywhere else: syVideoInit/SYVideoSetup/the
 * zbuffer allocation and arena_size -- mvOpeningYoshiStartScene calls
 * syTaskmanStartTask directly, the same as every scene in this chain.
 *
 * Overlay 41 (ovl41_BSS_END), confirmed free in src/dc/overlay.h's own
 * numbering, and confirmed (sc/scmanager.c:1106-1109) to be the only
 * overlay decomp's own nSCKindOpeningYoshi arm loads.
 */
#ifndef SSB_DC_MVOPENINGYOSHI_H
#define SSB_DC_MVOPENINGYOSHI_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* mvopeningyoshi.c:1379-1421-shaped dMVOpeningYoshiTaskmanSetup,
 * DIVERGES the same shape mvopeningmario.h documents: the decomp's own
 * pool counts are a placeholder (zero), func_lights is NULL, and the DL
 * buffer/graphics-heap/RDP-output sizes are zeroed. The counts below
 * reuse mvopeningmario.h's own (one real VS stage, up to two real
 * fighters, both Yoshi). */
extern SYTaskmanSetup dMVOpeningYoshiTaskmanSetup;

/* The loaded ifannounce.spr bank's head, for hosttest_ft.c's own
 * openingyoshi_find (mvopeningyoshi.c's own note by its definition) --
 * the same shape sMVOpeningMarioNamesFileHead/sMVOpeningDonkeyNamesFileHead/
 * sMVOpeningLinkNamesFileHead/sMVOpeningSamusNamesFileHead already
 * establish. */
extern void *sMVOpeningYoshiNamesFileHead;

void mvOpeningYoshiSetupFiles(void);
void mvOpeningYoshiInitName(SObj *sobj);
void mvOpeningYoshiMakeName(void);
void mvOpeningYoshiMotionCameraProcUpdate(GObj *camera_gobj);
void mvOpeningYoshiMakeMotionCamera(Vec3f move);
void mvOpeningYoshiMakeMotionWindow(void);
void mvOpeningYoshiPosedWallpaperProcDisplay(GObj *gobj);
void mvOpeningYoshiMakePosedWallpaper(void);
void mvOpeningYoshiPosedFighterProcUpdate(GObj *fighter_gobj);
void mvOpeningYoshiMakePosedFighter(void);
void mvOpeningYoshiMakeNameCamera(void);
void mvOpeningYoshiMakePosedFighterCamera(void);
void mvOpeningYoshiMakePosedWallpaperCamera(void);
void mvOpeningYoshiFuncRun(GObj *gobj);
void mvOpeningYoshiInitVars(void);
void mvOpeningYoshiFuncStart(void);
void mvOpeningYoshiStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[41]. */
void mvOpeningYoshiOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGYOSHI_H */
