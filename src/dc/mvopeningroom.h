/* mvopeningroom.h -- the openings' first scene and their spine,
 * mv/mvopening/mvopeningroom.c ported.
 *
 * The attract movie is not one scene but nineteen, each its own decomp
 * file with its own overlay (34-52), chained linearly by setting
 * scene_curr and calling syTaskmanSetLoadScene(), each skippable to the
 * title with A/B/START:
 *
 *   Room(34) -> Portraits(35) -> Mario(36) -> Donkey(37) -> Link(40) ->
 *   Samus(38) -> Yoshi(41) -> Kirby(43) -> Fox(39) -> Pikachu(42) ->
 *   Run(44) -> Cliff(46) -> Yamabuki(48) -> Jungle(51) -> Yoster(45) ->
 *   Sector(50) -> Standoff(47) -> Clash(49) -> Newcomers(52) -> Title
 *
 * This file is the first of them, and the game's own default boot scene
 * (sc/scmanager.c:493-494: dSCManagerDefaultSceneData carries
 * nSCKindOpeningRoom as both scene_curr and scene_prev). It is Master
 * Hand's desk: the room set fades up, the hand plucks a trophy of a
 * random fighter off the desk, drops a second one, the camera cuts
 * through four framings and a wipe transition, and at tic 22s it hands
 * off to nSCKindOpeningPortraits. The whole scene is one tic counter,
 * sMVOpeningRoomTotalTimeTics, read by mvOpeningRoomFuncRun and by every
 * prop's own update proc.
 *
 * ---- The relocData scene-graph files.
 *
 * dMVOpeningRoomFileIDs lists eight relocData files. Three of them
 * (llMVCommonFileID 52, llMVOpeningRoomTransitionFileID 63,
 * llMVOpeningRunCrashFileID 75) are read through lbRelocGetFileData as
 * DObjDesc/MObjSub/MatAnimJoint/AObjEvent32/DisplayList scene-graph
 * trees. Each is baked by an exporter instead:
 *   - plain DObjDesc trees (Desk, Books, Pencils, Lamp, the camera
 *     "snap") and lone display lists bound to one DObj by
 *     gcAddDObjForGObj (Sunlight, Outside, Haze, Tissues, the boss
 *     shadow, the spotlight) go through
 *     tools/export/ssb_scenemodelexport.py: --dl, --dllinks for a block
 *     that is a DObjDLLink array, --animdl for an AnimJoint handed over
 *     as one script, --dlhead for the DL head the display proc queues
 *     it into, and --mobjsub/--matanim for the MObjSub records and
 *     MatAnimJoint scripts passed to gcAddMObjAll /
 *     gcAddMatAnimJointAll (baked as the FPackMObjs block
 *     ssb_emblemexport.py first wrote). See the ROOM_MODELS rules in
 *     src/game/ssb64/Makefile and mvOpeningRoomSetupPropTree.
 *   - the room Background is 51 joints against FIGHTER_MAX_JOINTS's 40,
 *     so it ships as TWO packs off the one DObjDesc array (--entries
 *     0-17 and 0,18-50, entry 0 being the array's one root) on two
 *     GObjs at the same origin, the split scstaffroll.h and
 *     mvopeningyoster.c both argue out. The exporter carries the
 *     MObjSub and MatAnimJoint tables' per-joint indices across the
 *     renumbering, since both are indexed by the FILE's entry number.
 *   - the wipe transition's outline/overlay pair (file 63 is two flat
 *     display lists): tools/export/ssb_roomwipeexport.py bakes their
 *     triangles into romdisk/mvroomwipe.bin, and the two display procs
 *     -- which point the N64's colour image at its Z buffer -- are
 *     re-expressed as depth-ALWAYS opaque polygons plus a photocopy of
 *     the room's last frame; mvopeningroom.c's wipe comment has the
 *     whole argument.
 *
 * Three of the eight files are not scene-graph trees.
 *
 * llMVOpeningRoomScene1..4FileID (56-59) held only one thing this scene
 * ever read out of them: an AObjEvent32 camera-anim script each, and
 * those are exported by name into
 * romdisk/mvopeningroom.cam and handed to gcAddCObjCamAnimJoint /
 * gcPlayCamAnim -- which are the decomp's own functions, compiled
 * unmodified out of sys/objanim.c, not ported or re-derived
 * (src/dc/camanim.h says how, and why the words are relocated rather
 * than memcpy'd). So the five camera-anim calls this file makes -- one
 * in each of mvOpeningRoomInitScene1..4Cameras, and a fifth in
 * mvOpeningRoomMakeLogoCamera, which replays scene 1's script -- are
 * verbatim, and all four shots move the way the game's do. Scenes 3 and 4
 * set eye/at/up/fovy/near/far in the C source and the script drifts them
 * from there; scenes 1 and 2 set nothing but a viewport, so the script
 * moves them from gcAddXObjForCamera's own default look-at (eye
 * (0,0,1500), at origin, up +Y).
 *
 * The eighth file, llMVOpeningRoomWallpaperFileID (90), is a
 * .spritelist -- one Sprite, llMVOpeningRoomWallpaperSprite at 0x26c88
 * -- and that one IS ported, exported by tools/export/ssb_spriteexport.py
 * --file 90 into romdisk/mvopeningroomwallpaper.spr and reached through
 * the same sprite_bank_get redefinition of lbRelocGetFileData every
 * ported menu already uses.
 *
 * ---- Master Hand and the trophy fighters.
 *
 * mvOpeningRoomMakeBoss builds an nFTKindBoss fighter through the
 * ordinary ftManagerMakeFighter path, and with it
 * mvOpeningFighterProcUpdate and scSubsysFighterOpeningProcUpdate -- the
 * hand actually holds the plucked trophy. mvOpeningRoomFuncRun's two
 * scSubsysFighterSetStatus calls on him (tics 560 and 860) and its
 * gcEjectGObj (tic 1040) keep their NULL guards, because a pack that
 * fails to load still answers NULL. His shadow
 * (llMVCommonRoomBossShadowDisplayList and its AnimJoint, a lone display
 * list plus one script in file 52) and mvOpeningRoomMakeDeskGround are
 * the decomp's too.
 *
 * The two trophy fighters are drawn at random from the eight originals,
 * both made through ftManagerMakeFighter; nFTDemoStatusFigurePulled/
 * Dropped/Stand each play their own pose.
 *
 * ---- DIVERGES: the N64 swap-buffer hook.
 *
 * mvOpeningRoomCheckSetFramebuffer is this scene's own VI callback,
 * installed with syTaskmanSetFuncSwapBuffer at the top of FuncStart and
 * torn down with syTaskmanSetFuncSwapBuffer(NULL) at the bottom of
 * StartScene. It reads osViGetNextFramebuffer/osViGetCurrentFramebuffer
 * and rotates gSYSchedulerNextFramebuffer around gSYFramebufferSets[3]:
 * N64 triple-buffer bookkeeping, whole and entire. The PVR owns its own
 * framebuffer (sys/video.c is on the port's REPLACED list for exactly
 * this), syTaskmanSetFuncSwapBuffer does not exist port-side at all,
 * and nothing would call the hook if it did. The function and both
 * calls are cut, the same cut every scene's own syVideoInit already
 * takes.
 *
 * ---- The standard cuts, as everywhere else.
 *
 *   - syVideoInit/SYVideoSetup/the zbuffer allocation, and arena_size
 *     (a link-map address difference with no port meaning):
 *     mvOpeningRoomStartScene runs scManagerFuncUpdate directly.
 *   - lbRelocInitSetup/lbRelocLoadFilesListed and the two status
 *     buffers: replaced by the one sprite-bank load above.
 *   - the three syTaskmanMalloc(gFTManagerFigatreeHeapSize, 0x10) calls:
 *     the port's ftmanager.c does not define that global and
 *     ftManagerAllocFigatreeHeapKind always returns NULL, so all three
 *     heaps stay NULL -- which is exactly what FTDesc.figatree_heap
 *     already gets everywhere else. Fourth occurrence of this gap after
 *     mncharacters.c/mnplayersvs.c/mvending.c; treat it as known.
 *   - dMVOpeningRoomLights11/12 and mvOpeningRoomFuncLights: the
 *     standing rule (a Lights1/FuncLights pair never survives
 *     test-host) plus the fact that the two tables are unreferenced
 *     even in the decomp and the proc's whole body is the dropped
 *     ftDisplayLightsDrawReflect. func_lights is NULL below.
 *   - func_ovl34_801322C8/80132320/80132328, marked "Unused?" in the
 *     decomp and genuinely unreferenced.
 *   - gSCManagerUnkown0x800A50F0: a decomp-only global with no port
 *     definition and no port reader.
 *
 * ---- The hand-off.
 *
 * mvOpeningRoomFuncRun's tic-22s arm is verbatim: scene_curr =
 * nSCKindOpeningPortraits, the next scene in the chain.
 *
 * Overlay 34 (ovl34_BSS_END), confirmed free in src/dc/overlay.h's own
 * numbering.
 */
#ifndef SSB_DC_MVOPENINGROOM_H
#define SSB_DC_MVOPENINGROOM_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* mvopeningroom.c:1367-1409 dMVOpeningRoomTaskmanSetup, DIVERGES the
 * same shape src/dc/mvending.c's own dMVEndingTaskmanSetup documents:
 * every decomp pool count is zero (a placeholder -- scvsbattle.c:24-64
 * names the same pattern), func_lights is NULL, and the DL buffer/
 * graphics-heap/RDP-output sizes are zeroed because there is no RSP or
 * RDP here. The counts below are the port's own -- see the pools note
 * in mvopeningroom.c. */
extern SYTaskmanSetup dMVOpeningRoomTaskmanSetup;

void mvOpeningRoomMakeBackground(void);
void mvOpeningRoomMakeSunlight(void);
void mvOpeningRoomMakeDesk(void);
void mvOpeningRoomMakeOutside(void);
void mvOpeningRoomMakeHaze(void);
void mvOpeningRoomCommonProcUpdate(GObj *gobj);
void mvOpeningRoomTissuesProcUpdate(GObj *gobj);
void mvOpeningRoomDeskGroundProcUpdate(GObj *gobj);
void mvOpeningRoomBackgroundProcUpdate(GObj *gobj);
void mvOpeningRoomMakeBooks(void);
void mvOpeningRoomMakePencils(void);
void mvOpeningRoomMakeLamp(void);
void mvOpeningRoomMakeTissues(void);
void mvOpeningRoomMakeBoss(void);
void mvOpeningFighterProcUpdate(GObj *gobj);
void mvOpeningRoomMakePulledFighter(s32 fkind);
void mvOpeningRoomLogoWallpaperProcDisplay(GObj *gobj);
void mvOpeningRoomMakeLogoWallpaper(void);
void mvOpeningRoomMakeLogo(void);
void mvOpeningRoomMakeSnap(void);
void mvOpeningRoomMakeCloseUpEffect(void);
void mvOpeningRoomMakeDroppedFighter(s32 fkind);
void mvOpeningRoomMakeBossShadow(void);
void mvOpeningRoomMakeDeskGround(void);
void mvOpeningRoomCloseUpOverlayProcDisplay(GObj *gobj);
void mvOpeningRoomMakeCloseUpOverlay(void);
void mvOpeningRoomMakeCloseUpOverlayCamera(void);
void mvOpeningRoomWallpaperProcDisplay(GObj *gobj);
void mvOpeningRoomMakeWallpaper(void);
void mvOpeningRoomSetSpotlightPosition(GObj *gobj, s32 fkind);
void mvOpeningRoomMakeSpotlight(void);
void mvOpeningRoomEjectRoomGObjs(void);
void mvOpeningRoomInitScene1Cameras(GObj *gobj);
void mvOpeningRoomMakeScene1Cameras(void);
void mvOpeningRoomInitScene2Cameras(GObj *gobj);
void mvOpeningRoomMakeScene2Cameras(void);
void mvOpeningRoomInitScene3Cameras(GObj *gobj);
void mvOpeningRoomMakeScene3Cameras(void);
void mvOpeningRoomInitScene4Cameras(GObj *gobj);
void mvOpeningRoomMakeScene4Cameras(void);
void mvOpeningRoomMakeWallpaperCamera(void);
void mvOpeningRoomMakeLogoCamera(void);
void mvOpeningRoomMakeTransition(void);
/* The wipe's two stars off the disc (mvroomwipe.bin), and, for the host
 * test, what was read: the corners as {x, y, argb} triples of f32/f32/
 * u32, the triangle count and the first corner's colour. */
sb32 mvOpeningRoomLoadWipe(void);
const f32 *mvOpeningRoomGetWipeTris(s32 outline, s32 *count, u32 *argb0);
void mvOpeningRoomMakeTransitionCamera(void);
void mvOpeningRoomEjectCameraGObjs(void);
s32 mvOpeningRoomGetDroppedFighterKind(void);
s32 mvOpeningRoomGetPulledFighterKind(void);
void mvOpeningRoomInitVars(void);
void mvOpeningRoomFuncRun(GObj *gobj);
void mvOpeningRoomFuncStart(void);
void mvOpeningRoomStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[34]. */
void mvOpeningRoomOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGROOM_H */
