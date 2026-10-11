/* mvopeningmario.h -- the openings' third scene, mv/mvopening/mvopeningmario.c
 * ported. Overlay 36.
 *
 * Room(34) -> Portraits(35) -> Mario(36) -> Donkey(37) -> ... (see
 * mvopeningroom.h for the full nineteen-scene chain). This is the first
 * of the eight per-fighter opening scenes (mvopeningroom.h's "tier 2":
 * a real VS stage and a real fighter, gated only on the camera-anim
 * bank. A "MARIO" name card holds for 15
 * tics over a black screen, then the card is ejected and three things
 * appear at once: a posed Mario standing in a color-filled box on the
 * left third of the screen, and on the right two-thirds a real
 * Peach's Castle (nGRKindCastle) with Mario walking toward the camera
 * under a canned FTKeyEvent script (not a player). At tic 60 it hands
 * off to nSCKindOpeningDonkey -- unported, so scManagerRunScene's
 * default arm loops this scene back into itself, the same reading
 * mvopeningroom.h and mvopeningportraits.h already establish.
 *
 * ---- Substitution 1: the two relocData files, neither loaded the
 * decomp's way.
 *
 * dMVOpeningMarioFileIDs is { &llIFCommonAnnounceCommonFileID (37),
 * &llMVOpeningCommonFileID (65) }. Neither is read as a raw relocData
 * file here:
 *
 *   - File 37 is only ever read for five sprites (the letters M, A, R,
 *     I, O) via lbRelocGetFileData -- ordinary sprite content, the same
 *     shape mnstartup.c/mvopeningportraits.c already load. It is also
 *     not a new export: src/game/ssb64/Makefile already builds
 *     romdisk/ifannounce.spr from this exact file for the HUD and
 *     scvsresults.c's own results screen, and "a scene loads its own
 *     copy, because a scene's banks go with the scene" is
 *     scvsresults.c's own established rule for it. This file follows
 *     it: sprite_bank_load("ifannounce.spr") into a bank owned here.
 *   - File 65 is only ever read for one thing, a camera-anim script
 *     (&llMVOpeningCommonMarioCamAnimJoint), and that gap is already
 *     closed: tools/export/ssb_camanimexport.py's "OpeningCommon"
 *     bank was built for exactly this -- one export, eight scripts,
 *     one per per-fighter opening scene, Mario's at romdisk/
 *     mvopeningcommon.cam under the name "Mario" -- because "each [of
 *     the eight] reads sMVOpening<F>Files[1] + &llMVOpeningCommon<F>
 *     CamAnimJoint. So one export serves all eight, and it is done
 *     now rather than eight times later" (tools/export/ssb_camanimexport.py's
 *     own comment on the OpeningCommon table). So
 *     camanim_bank_load("mvopeningcommon.cam") + camanim_get(bank,
 *     "Mario") stands in for lbRelocGetFileData on this file, the same
 *     substitution src/dc/mvopeningroom.c already makes for its own
 *     four Scene1..4 scripts (src/dc/camanim.h).
 *
 * dMVOpeningMarioFileIDs itself is not carried into the port (the same
 * choice mvopeningroom.c already makes for its own eight-entry array):
 * nothing here reads a file by numeric index once both loads above are
 * direct.
 *
 * ---- Substitution 2: FILLCOLOR, dropped -- checked, not assumed.
 *
 * mvOpeningMarioFuncStart's gcMakeDefaultCameraGObj call carries
 * COBJ_FLAG_FILLCOLOR|COBJ_FLAG_ZBUFFER, the same call shape
 * mnstartup.c/mvopeningportraits.c/mvopeningroom.c all make.
 * mvOpeningMarioFuncRun never references this camera or its flag
 * anywhere in its own body (checked directly, the same way room's vs. portraits' own FuncRun against each other) -- so
 * the mnstartup.c/mvopeningportraits.c reading applies: dropped
 * outright. This also matches the general rule scAutoDemoFuncStart's
 * own header already states for a scene that draws a real 3D world:
 * "gcMakeDefaultCameraGObj (the black clear camera) is gone -- the PVR
 * clears its own framebuffer."
 *
 * ---- Substitution 3: the real ground/real fighter recipe
 * scVSBattleStartBattle/scAutoDemoFuncStart already established, not a
 * new one invented here.
 *
 *   - mpCollisionInitGroundData() is cut, in favour of the stage
 *     acquire scAutoDemoFuncStart's own header names: grStageAcquire
 *     (nGRKindCastle) + stage_bind(stage) (new here, since gkind is
 *     static for this scene rather than discovered at runtime) and
 *     stage_bind_collision() in the call's own former place.
 *   - gmCameraMakeWallpaperCamera() is cut outright (not moved): the
 *     decomp's own version pre-makes a dedicated wallpaper CObj for
 *     grWallpaperMakeDecideKind to configure later; the port's own
 *     grWallpaperMakeDecideKind (src/dc/stage.c) hangs the one baked
 *     wallpaper quad on gGMCameraGObj instead and needs no camera of
 *     its own, the same DIVERGES stage.h's own header already names.
 *   - gmCameraMakeMovieCamera(NULL) is the decomp's own call again
 *     (src/dc/gmcamera.c): matrix kind 8, whose up.x is a roll angle
 *     (objdisplay.c gcCameraLookAtF). It is also what sets
 *     gGMCameraGObj, which the next point depends on.
 *   - grWallpaperMakeDecideKind()/grCommonSetupInitAll(), inside
 *     mvOpeningMarioMakeMotionWindow, move from before
 *     mvOpeningMarioMakeMotionCamera to after it, the same
 *     "moved after [the camera], where the camera it attaches to is
 *     the one that has to exist first" reordering stage.h's own
 *     grWallpaperMakeDecideKind DIVERGES note already documents for
 *     scvsbattle.c/scautodemo.c.
 *   - gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK is set
 *     right after the motion camera is made, the same line
 *     scAutoDemoFuncStart makes for its own single real camera. The
 *     decomp has no equivalent (the mask is a port-only DL-link
 *     routing table, gmcamera.h says why), but without it neither the
 *     stage nor the walking Mario would draw through this camera at
 *     all.
 *
 * ---- Substitution 3b: efManagerInitEffects.
 *
 * mvOpeningMarioFuncStart's own efManagerInitEffects() call is cut too,
 * for the same "real stage, real fighter" reason as Substitution 3:
 * scVSBattleStartBattle's own DIVERGES list already cuts this exact
 * function ("efManagerInitEffects -- ef/efmanager.c, 4.1k lines.
 * efParticleInitAll is here, and so is the one line of
 * efManagerInitEffects that does not need it: see
 * scVSBattleLoadEffectBank below") and replaces it with
 * efManagerLoadEffectBank() + efManagerPreloadModels(), which this file
 * does too. The concrete failure efManagerLoadEffectBank's own
 * FT_HOSTTEST guard documents (src/dc/efmanager.c) is a real one, not
 * theoretical: with a real Mario pack loaded, the raw
 * efManagerInitEffects() call segfaults the host build inside
 * lbParticleSetupBankID, an LBScript-pointer/LBTexture-pointer array walk that is
 * only safe on a 32-bit target. mvopeningroom.c's own verbatim call to
 * the same function has never hit this because none of its posed
 * fighters' packs happens to carry a particle range that trips it --
 * not evidence that the raw call is safe in general.
 *
 * ---- Substitution 4: the figatree heap.
 *
 * The one `syTaskmanMalloc(gFTManagerFigatreeHeapSize, 0x10)` call
 * (for sMVOpeningMarioFigatreeHeap, the POSED fighter's heap) is cut,
 * the same cut mvopeningroom.h already names for its own three: the
 * port's ftmanager.c does not define gFTManagerFigatreeHeapSize, so the
 * field stays NULL, which is already what every ftManagerAllocFigatreeHeapKind
 * call returns everywhere else. sMVOpeningMarioFigatreeHeap is kept as
 * a field (always NULL) rather than removed, matching room's own
 * choice, so mvOpeningMarioMakePosedFighter's read of it stays
 * verbatim. The WALKING fighter's own heap, inside the
 * mvOpeningMarioMakeMotionWindow loop, already goes through
 * ftManagerAllocFigatreeHeapKind() -- the ordinary, already-ported
 * function -- and needs no cut at all.
 *
 * ---- Substitution 5: the trailing tic-sync busy-wait. RETIRED.
 *
 * mvOpeningMarioFuncStart's own `while (sySchedulerGetTicCount() <
 * 1515) continue;` was first cut outright, because this port's tic
 * counter only advances inside syTaskmanRunFrame, which runs after
 * func_start returns, so kept verbatim it hangs forever. It is back as
 * sySchedulerWaitTicCount(1515), which waits on the retrace count; every
 * opening scene ends its FuncStart with one.
 *
 * ---- The standard cuts, as everywhere else: syVideoInit/
 * SYVideoSetup/the zbuffer allocation and arena_size (a link-map
 * address difference with no port meaning) -- mvOpeningMarioStartScene
 * calls syTaskmanStartTask directly, the same as
 * mvopeningroom.c/mvopeningportraits.c.
 *
 * Overlay 36 (ovl36_BSS_END), confirmed free in src/dc/overlay.h's own
 * numbering.
 */
#ifndef SSB_DC_MVOPENINGMARIO_H
#define SSB_DC_MVOPENINGMARIO_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* mvopeningmario.c:1367-1409-shaped dMVOpeningMarioTaskmanSetup,
 * DIVERGES the same shape mvopeningportraits.h/mvopeningroom.h
 * document: the decomp's own pool counts are a placeholder (zero),
 * func_lights is NULL (the Lights1/FuncLights pair never survives
 * test-host -- see mvopeningmario.c), and the DL buffer/graphics-heap/
 * RDP-output sizes are zeroed (no RSP/RDP here). The counts below are
 * the port's own: this scene carries one real VS stage and up to two
 * real fighters (the posed one and the walking one, both Mario), the
 * same shape scAutoDemoFuncStart's own header already sized for up to
 * four -- reused here rather than recomputed, the same call that
 * file's own header makes reusing scvsbattle.c's figures. */
extern SYTaskmanSetup dMVOpeningMarioTaskmanSetup;

/* The loaded ifannounce.spr bank's head, for hosttest_ft.c's own
 * openingmario_find (mvopeningmario.c's own note by its definition). */
extern void *sMVOpeningMarioNamesFileHead;

void mvOpeningMarioSetupFiles(void);
void mvOpeningMarioInitName(SObj *sobj);
void mvOpeningMarioMakeName(void);
void mvOpeningMarioMotionCameraProcUpdate(GObj *camera_gobj);
void mvOpeningMarioMakeMotionCamera(Vec3f move);
void mvOpeningMarioMakeMotionWindow(void);
void mvOpeningMarioPosedWallpaperProcDisplay(GObj *gobj);
void mvOpeningMarioMakePosedWallpaper(void);
void mvOpeningMarioPosedFighterProcUpdate(GObj *fighter_gobj);
void mvOpeningMarioMakePosedFighter(void);
void mvOpeningMarioMakeNameCamera(void);
void mvOpeningMarioMakePosedFighterCamera(void);
void mvOpeningMarioMakePosedWallpaperCamera(void);
void mvOpeningMarioFuncRun(GObj *gobj);
void mvOpeningMarioInitVars(void);
void mvOpeningMarioFuncStart(void);
void mvOpeningMarioStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[36]. */
void mvOpeningMarioOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGMARIO_H */
