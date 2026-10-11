/* mnplayers1ptraining.h -- Training mode's character select,
 * mn/mnplayers/mnplayers1ptraining.c. Overlay 28.
 *
 * The second door out of the 1P submenu (src/dc/mn1pmode.c), and the
 * front half of the one 1P mode that needs no boss, no Polygon team,
 * no ladder and no bonus map: it picks the player's fighter and the
 * CPU's, then hands off to nSCKindTrainingMode.
 *
 * It is the VS character select's twin. Of its 120 functions, 99 share
 * a name with one of mn/mnplayers/mnplayersvs.c's, and 68 of those have
 * the same body once the prefix is normalised -- so src/dc/mnplayersvs.c
 * is this file's template, and every convention here is that file's.
 * What the twin does NOT have, and this does:
 *
 *   - one human and one CPU slot rather than four free ones, with the
 *     puck the player is not holding parked on the CPU's panel
 *     (mnPlayers1PTrainingResetPlayerNot, SetGateLUTAll);
 *   - the pulsing glow behind the held puck
 *     (mnPlayers1PTrainingPuckGlowProcUpdate);
 *   - the READY banner's own blink (ReadyProcDisplay);
 *   - B going back to the 1P submenu rather than the mode select
 *     (BackTo1PMode).
 *
 * Every function is the decomp's by name and body, the REGION_US arms;
 * the line numbers are the decomp's. What is not here, and where it is
 * said:
 *   - the lighting pre-render (mnPlayers1PTrainingFuncLights) and its
 *     two Lights1 objects -- dMNPlayers1PTrainingTaskmanSetup;
 *   - the clear camera the PVR does itself -- mnPlayers1PTrainingFuncStart;
 *   - syVideoInit, the z-buffer and the arena_size line --
 *     mnPlayers1PTrainingStartScene;
 *   - the reloc setup: seven sprite banks and the spotlight model stand
 *     in for the eight files -- mnPlayers1PTrainingLoadFiles.
 */
#ifndef SSB_DC_MNPLAYERS1PTRAINING_H
#define SSB_DC_MNPLAYERS1PTRAINING_H

/* Narrow, as src/dc/mnplayersvs.h is: <mn/menu.h> would pull the
 * decomp's own mnmaps.h in behind it, whose mnMapsMakeLayer and
 * mnMapsMakeModel take the reloc ground data the port replaced, and
 * src/dc/scmanager.c includes both that and src/dc/mnmaps.h. */
#include <sys/obj.h>
#include <sys/taskman.h>
#include <mn/mndef.h>

extern SYTaskmanSetup dMNPlayers1PTrainingTaskmanSetup;
extern u32 dMNPlayers1PTrainingFileIDs[];
extern void *sMNPlayers1PTrainingFiles[];

void mnPlayers1PTrainingSelectFighterPuck(s32 player, s32 select_button);
f32 mnPlayers1PTrainingGetNextPortraitX(s32 portrait, f32 current_pos_x);
sb32 mnPlayers1PTrainingCheckFighterCrossed(s32 fkind);
void mnPlayers1PTrainingPortraitProcUpdate(GObj *gobj);
void mnPlayers1PTrainingSetPortraitWallpaperPosition(SObj *sobj, s32 portrait);
void mnPlayers1PTrainingPortraitAddCross(GObj *gobj, s32 portrait);
sb32 mnPlayers1PTrainingCheckFighterLocked(s32 fkind);
void func_ovl28_80131FC8(void);
s32 mnPlayers1PTrainingGetFighterKind(s32 portrait);
s32 mnPlayers1PTrainingGetPortrait(s32 fkind);
void mnPlayers1PTrainingPortraitProcDisplay(GObj *gobj);
void mnPlayers1PTrainingMakePortraitShadow(s32 portrait);
void mnPlayers1PTrainingMakePortrait(s32 portrait);
void mnPlayers1PTrainingMakePortraitAll(void);
void mnPlayers1PTrainingMakeNameAndEmblem(GObj *gobj, s32 player, s32 fkind);
void mnPlayers1PTrainingMakePortraitCamera(void);
void mnPlayers1PTrainingMakePortraitWallpaperCamera(void);
void mnPlayers1PTrainingMakePortraitFlashCamera(void);
void mnPlayers1PTrainingMakeGateCamera(void);
void mnPlayers1PTrainingMakePlayerKindSelectCamera(void);
void mnPlayers1PTrainingMakePlayerKindCamera(void);
void mnPlayers1PTrainingMakeTeamSelectCamera(void);
void mnPlayers1PTrainingSetGateLUT(GObj *gobj, s32 player);
void mnPlayers1PTrainingMakePlayerKind(s32 player);
void mnPlayers1PTrainingMakeGate(s32 player);
void func_ovl28_80132FF4(void);
void func_ovl28_80132FFC(void);
void func_ovl28_80133004(void);
void func_ovl28_8013300C(void);
void mnPlayers1PTrainingMakeWallpaper(void);
void mnPlayers1PTrainingMakeLabels(void);
void func_ovl28_801332C4(void);
void func_ovl28_801332CC(void);
void func_ovl28_801332D4(void);
s32 mnPlayers1PTrainingGetFighterKindCount(s32 fkind);
sb32 mnPlayers1PTrainingCheckCostumeUsed(s32 fkind, s32 player, s32 costume);
s32 mnPlayers1PTrainingGetFreeCostumeRoyal(s32 fkind, s32 player);
s32 mnPlayers1PTrainingGetFreeCostume(s32 fkind, s32 player);
s32 mnPlayers1PTrainingGetStatusSelected(s32 fkind);
void mnPlayers1PTrainingFighterProcUpdate(GObj *fighter_gobj);
void mnPlayers1PTrainingMakeFighter(GObj *fighter_gobj, s32 player, s32 fkind, s32 costume);
void mnPlayers1PTrainingMakeFighterCamera(void);
void mnPlayers1PTrainingUpdateCursor(GObj *gobj, s32 player, s32 cursor_status);
void func_ovl28_80133CA0(void);
void mnPlayers1PTrainingSetGateLUTAll(void);
sb32 mnPlayers1PTrainingCheckPuckInRange(GObj *gobj, s32 cursor_player, s32 player);
void mnPlayers1PTrainingUpdateFighter(s32 player);
void func_ovl28_80133ED8(void);
void mnPlayers1PTrainingUpdateNameAndEmblem(s32 player);
void mnPlayers1PTrainingDestroyPortraitFlash(s32 player);
void mnPlayers1PTrainingPortraitFlashThreadUpdate(GObj *gobj);
void mnPlayers1PTrainingMakePortraitFlash(s32 player);
void mnPlayers1PTrainingAnnounceFighter(s32 player, s32 slot);
void func_ovl28_801342B0(void);
void mnPlayers1PTrainingDestroyHandicapLevel(s32 player);
SObj* mnPlayers1PTrainingGetArrowSObj(GObj *gobj, sb32 left_or_right);
void mnPlayers1PTrainingArrowThreadUpdate(GObj *gobj);
void mnPlayers1PTrainingHandicapLevelProcUpdate(GObj *gobj);
void mnPlayers1PTrainingMakeHandicapLevel(s32 player);
void func_ovl28_80134830(void);
void func_ovl28_80134838(void);
void func_ovl28_80134840(void);
sb32 mnPlayers1PTrainingSelectFighter(GObj *gobj, s32 player, s32 unused, s32 select_button);
void mnPlayers1PTrainingUpdateCursorGrabPriorities(s32 player, s32 puck);
void mnPlayers1PTrainingUpdateCursorPlacementPriorities(s32 player, s32 puck);
void mnPlayers1PTrainingSetCursorPuckOffset(s32 player);
void mnPlayers1PTrainingSetCursorGrab(s32 player, s32 held_player);
sb32 mnPlayers1PTrainingCheckCursorPuckGrab(GObj *gobj, s32 player);
s32 mnPlayers1PTrainingGetPuckFighterKind(s32 player);
void mnPlayers1PTrainingAdjustCursor(GObj *gobj, s32 player);
void mnPlayers1PTrainingUpdateCursorNoRecall(GObj *gobj, s32 player);
void mnPlayers1PTrainingUpdateCostume(s32 player, s32 select_button);
sb32 mnPlayers1PTrainingCheckManFighterSelected(s32 player);
void mnPlayers1PTrainingRecallPuck(s32 player);
void mnPlayers1PTrainingBackTo1PMode(void);
void mnPlayers1PTrainingDetectBack(s32 player);
sb32 mnPlayers1PTrainingCheckBackInRange(GObj *gobj);
void mnPlayers1PTrainingCursorProcUpdate(GObj *gobj);
void mnPlayers1PTrainingUpdatePuck(GObj *gobj, s32 puck);
void mnPlayers1PTrainingCenterPuckInPortrait(GObj *gobj, s32 fkind);
void func_ovl28_80135D7C(void);
void mnPlayers1PTrainingMovePuck(s32 player);
void mnPlayers1PTrainingPuckProcUpdate(GObj *gobj);
void mnPlayers1PTrainingMakeCursorCamera(void);
void mnPlayers1PTrainingMakePuckCamera(void);
void mnPlayers1PTrainingMakeHandicapLevelCamera(void);
void mnPlayers1PTrainingMakeReadyCamera(void);
void mnPlayers1PTrainingMakeCursor(s32 player);
void mnPlayers1PTrainingPuckProcDisplay(GObj *gobj);
void mnPlayers1PTrainingMakePuck(s32 player);
f32 mnPlayers1PTrainingGetPuckDistance(s32 this_player, s32 other_player);
void mnPlayers1PTrainingPuckAdjustOverlap(s32 this_player, s32 other_player, f32 unused);
void mnPlayers1PTrainingPuckAdjustPortraitEdge(s32 player);
void mnPlayers1PTrainingPuckAdjustPlaced(s32 player);
void mnPlayers1PTrainingPuckAdjustRecall(s32 player);
void mnPlayers1PTrainingPuckAdjustProcUpdate(GObj *gobj);
void mnPlayers1PTrainingMakePuckAdjust(void);
void mnPlayers1PTrainingPuckGlowProcUpdate(GObj *gobj);
void mnPlayers1PTrainingMakePuckGlow(void);
void mnPlayers1PTrainingCostumeSyncProcUpdate(GObj *gobj);
void mnPlayers1PTrainingMakeCostumeSync(void);
void mnPlayers1PTrainingSpotlightProcUpdate(GObj *gobj);
void mnPlayers1PTrainingMakeSpotlight(void);
void mnPlayers1PTrainingReadyProcDisplay(GObj *gobj);
sb32 mnPlayers1PTrainingCheckReady(void);
void mnPlayers1PTrainingReadyProcUpdate(GObj *gobj);
void mnPlayers1PTrainingMakeReady(void);
void func_ovl28_801375D0(void);
void mnPlayers1PTrainingSetSceneData(void);
void mnPlayers1PTrainingPauseSlotProcesses(void);
void mnPlayers1PTrainingFuncRun(GObj *gobj);
void mnPlayers1PTrainingInitPlayer(s32 player);
void mnPlayers1PTrainingResetPlayer(s32 player);
void mnPlayers1PTrainingResetPlayerNot(s32 player);
void mnPlayers1PTrainingInitVars(void);
void mnPlayers1PTrainingInitSlot(s32 player);
void mnPlayers1PTrainingInitSlotAll(void);
void mnPlayers1PTrainingLoadFiles(void);
void mnPlayers1PTrainingFuncStart(void);
void mnPlayers1PTrainingStartScene(void);
void mnPlayers1PTrainingOverlayLoad(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[28]. */
void mnPlayers1PTrainingOverlayLoad(void);

#endif /* SSB_DC_MNPLAYERS1PTRAINING_H */
