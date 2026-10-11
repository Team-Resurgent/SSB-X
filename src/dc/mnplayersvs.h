/* mnplayersvs.h -- the VS character select, mn/mnplayers/mnplayersvs.c.
 *
 * The screen after VS START: twelve portraits in two rows over a stone
 * wall, a gate per player below them -- a card in the player's colour
 * with two shutter doors, the 1P/2P/3P/4P badge, the HMN/CP/NA button,
 * the fighter's name and series emblem, and the fighter standing in 3D
 * on the gate -- and, for each pad plugged in, a hand cursor that
 * carries the player's puck. Drop the puck on a portrait and that is
 * the fighter (the fighter appears on the gate, the announcer says the
 * name, the portrait flashes); the C buttons pick a costume; B recalls
 * the puck; a puck dropped on an HMN button makes that slot a CPU. The
 * time or stock count sits at the top with arrows, the game mode
 * (free-for-all or team) beside it, BACK on the right. With two fighters
 * placed READY TO FIGHT appears and START goes on -- to the stage select
 * or, with the stage select off, to a random stage and the battle -- a
 * second after the START, leaving the whole battle state filled in
 * (mnPlayersVSSetSceneData). Five idle minutes go to the title. Thirteen
 * sprite cameras, one per DL link 26-38, and one 3D camera over the
 * gates' fighters.
 *
 * Every function is the decomp's by name and body; the REGION_JP arms
 * stay under their #if as the decomp has them. What is not here, and
 * where it is said:
 *   - the clear camera, the lighting pre-render (mnPlayersVSFuncLights),
 *     the effect pools and the figatree heaps -- mnPlayersVSFuncStart,
 *     dMNPlayersVSTaskmanSetup;
 *   - syVideoInit and the arena_size line -- mnPlayersVSStartScene;
 *   - the reloc setup: six sprite banks stand in for six of the seven
 *     files -- mnPlayersVSLoadFiles;
 *   - the spotlight model, the seventh file -- mnPlayersVSMakeSpotlight;
 *   - a fighter the port has no pack for is not shown --
 *     mnPlayersVSMakeFighter;
 *   - the RDP commands of the two display procs -- the portrait static
 *     and the puck's glow -- in the port's spelling (src/dc/lbcommon.h);
 *   - the device-status walk bounded -- mnPlayersVSUpdateControllerOrders.
 * The fighter on the gate plays its idle animation where the game poses
 * it: src/dc/scsubsysfighter.c on the demo statuses.
 *
 * Where START leads is the scene manager's business
 * (src/dc/scmanager.c): the scene asks for nSCKindMaps or
 * nSCKindVSBattle as the game's does, and the manager decides what the
 * port has for it. */
#ifndef SSB_DC_MNPLAYERSVS_H
#define SSB_DC_MNPLAYERSVS_H

#include <sys/obj.h>
#include <sys/taskman.h>
#include <mn/mndef.h>

/* mnplayersvs.c:4805 */
extern SYTaskmanSetup dMNPlayersVSTaskmanSetup;

/* mnplayersvs.c:111 sMNPlayersVSFiles, the seven files' bases: the
 * game's void*[7] with a SpriteBank* in each (sprite.h), NULL for the
 * spotlight model. Not static in the decomp; the host test reads
 * sprites out of them by offset. */
extern void *sMNPlayersVSFiles[7];

s32 mnPlayersVSGetShade(s32 player);
void mnPlayersVSSelectFighterPuck(s32 player, s32 select_button);
f32 mnPlayersVSGetNextPortraitX(s32 portrait, f32 current_pos_x);
sb32 mnPlayersVSCheckFighterCrossed(s32 fkind);
void mnPlayersVSPortraitProcUpdate(GObj *gobj);
void mnPlayersVSSetPortraitWallpaperPosition(SObj *sobj, s32 portrait);
void mnPlayersVSPortraitAddCross(GObj *gobj, s32 portrait);
sb32 mnPlayersVSCheckFighterLocked(s32 fkind);
s32 mnPlayersVSGetFighterKind(s32 portrait);
s32 mnPlayersVSGetPortrait(s32 fkind);
void mnPlayersVSPortraitProcDisplay(GObj *gobj);
void mnPlayersVSMakePortraitShadow(s32 portrait);
void mnPlayersVSMakePortrait(s32 portrait);
void mnPlayersVSMakePortraitAll(void);
void mnPlayersVSMakeTeamSelect(s32 team, s32 player);
void mnPlayersVSDestroyTeamSelect(s32 player);
void mnPlayersVSUpdateTeamSelect(s32 team, s32 player);
void mnPlayersVSDestroyTeamSelectAll(void);
void mnPlayersVSMakeTeamSelectAll(void);
void mnPlayersVSUpdatePlayerKindSelect(GObj *gobj, s32 player, s32 pkind);
void mnPlayersVSMakeNameAndEmblem(GObj *gobj, s32 player, s32 fkind);
void mnPlayersVSUpdateShutter(s32 player);
void mnPlayersVSShutterProcUpdate(GObj *gobj);
void mnPlayersVSMakePortraitCamera(void);
void mnPlayersVSMakePortraitWallpaperCamera(void);
void mnPlayersVSMakePortraitFlashCamera(void);
void mnPlayersVSMakeGateCamera(void);
void mnPlayersVSMakePlayerKindSelectCamera(void);
void mnPlayersVSMakePlayerKindCamera(void);
void mnPlayersVSMakeTeamSelectCamera(void);
void mnPlayersVSShutter1PProcDisplay(GObj *gobj);
void mnPlayersVSShutter2PProcDisplay(GObj *gobj);
void mnPlayersVSShutter3PProcDisplay(GObj *gobj);
void mnPlayersVSShutter4PProcDisplay(GObj *gobj);
void mnPlayersVSSetGateLUT(GObj *gobj, s32 player, s32 pkind);
void mnPlayersVSMakePlayerKindSelect(s32 player);
void mnPlayersVSMakePlayerKind(s32 player);
void mnPlayersVSMakeGate(s32 player);
s32 mnPlayersVSGetPowerOf(s32 base, s32 exp);
void mnPlayersVSSetDigitColors(SObj *sobj, u32 *colors);
s32 mnPlayersVSGetNumberDigitCount(s32 number, s32 digit_count_max);
void mnPlayersVSMakeGameRuleNumber(GObj *gobj, s32 number, f32 x, f32 y, u32 *colors, s32 digit_count_max, sb32 is_fixed_digit_count);
void mnPlayersVSMakeTimeSetting(s32 number);
void mnPlayersVSMakeTimeSelect(s32 number);
void mnPlayersVSMakeStockSetting(s32 number);
void mnPlayersVSMakeStockSelect(s32 number);
void mnPlayersVSMakeWallpaper(void);
void mnPlayersVSMakeLabels(void);
s32 mnPlayersVSGetFighterKindCount(s32 fkind);
sb32 mnPlayersVSCheckCostumeUsed(s32 fkind, s32 player, s32 costume);
s32 mnPlayersVSGetFreeCostumeRoyal(s32 fkind, s32 player);
s32 mnPlayersVSGetFreeCostume(s32 fkind, s32 player);
s32 mnPlayersVSGetStatusSelected(s32 fkind);
void mnPlayersVSFighterProcUpdate(GObj *fighter_gobj);
void mnPlayersVSMakeFighter(GObj *fighter_gobj, s32 player, s32 fkind, s32 costume);
void mnPlayersVSMakeFighterCamera(void);
void mnPlayersVSUpdateCursor(GObj *gobj, s32 player, s32 cursor_status);
sb32 mnPlayersVSCheckTimeArrowRInRange(GObj *gobj);
s32 mnPlayersVSCheckTimeArrowLInRange(GObj *gobj);
void mnPlayersVSUpdateGateAll(void);
sb32 mnPlayersVSCheckGameModeInRange(GObj *gobj);
void mnPlayersVSUpdateGameMode(void);
s32 mnPlayersVSCheckTeamSelectInRange(GObj *gobj, s32 player);
sb32 mnPlayersVSCheckTeamSelectInRangeAll(GObj *gobj, s32 cursor_player);
sb32 mnPlayersVSCheckHandicapArrowInRangeAll(GObj *gobj, s32 cursor_player);
sb32 mnPlayersVSCheckHandicapArrowRInRange(GObj *gobj, s32 player);
sb32 mnPlayersVSCheckHandicapArrowLInRange(GObj *gobj, s32 player);
s32 mnPlayersVSCheckPlayerKindSelectInRange(GObj *gobj, s32 player);
sb32 mnPlayersVSCheckPuckInRange(GObj *gobj, s32 cursor_player, s32 player);
void mnPlayersVSUpdatePlayerKind(s32 player);
void mnPlayersVSUpdatePuckDisplay(GObj *gobj, s32 player);
void mnPlayersVSUpdateFighter(s32 player);
void mnPlayersVSUpdateCursorDisplay(GObj *gobj, s32 player);
void mnPlayersVSUpdateNameAndEmblem(s32 player);
void mnPlayersVSDestroyPortraitFlash(s32 player);
void mnPlayersVSPortraitFlashThreadUpdate(GObj *gobj);
void mnPlayersVSMakePortraitFlash(s32 player);
sb32 mnPlayersVSCheckPlayerKindSelect(GObj *gobj, s32 player, s32 select_player);
sb32 mnPlayersVSCheckPlayerKindSelectAllPlayer(GObj *gobj, s32 player);
void mnPlayersVSAnnounceFighter(s32 player, s32 slot);
void mnPlayersVSHideFighterName(s32 player);
void mnPlayersVSDestroyHandicapLevel(s32 player);
SObj* mnPlayersVSGetArrowSObj(GObj *gobj, sb32 left_or_right);
void mnPlayersVSArrowThreadUpdate(GObj *gobj);
void mnPlayersVSHandicapLevelProcUpdate(GObj *gobj);
void mnPlayersVSMakeHandicapLevel(s32 player);
void mnPlayersVSMakeHandicapLevelValue(s32 player);
void mnPlayersVSUpdateHandicapLevel(s32 player);
sb32 mnPlayersVSCheckHandicapOn(void);
sb32 mnPlayersVSCheckHandicapAuto(void);
sb32 mnPlayersVSCheckHandicap(void);
sb32 mnPlayersVSSelectFighter(GObj *gobj, s32 player, s32 unused, s32 select_button);
void mnPlayersVSUpdateCursorGrabPriorities(s32 player, s32 puck);
s32 mnPlayersVSUpdateCursorPlacementPriorities(s32 player, s32 puck);
void mnPlayersVSSetCursorPuckOffset(s32 player);
void mnPlayersVSSetCursorGrab(s32 player, s32 held_player);
sb32 mnPlayersVSCheckCursorPuckGrab(GObj *gobj, s32 player);
s32 mnPlayersVSGetPuckFighterKind(s32 player);
void mnPlayersVSAdjustCursor(GObj *gobj, s32 player);
void mnPlayersVSUpdateCursorNoRecall(GObj *gobj, s32 player);
void mnPlayersVSUpdateCostume(s32 player, s32 select_button);
sb32 mnPlayersVSCheckManFighterSelected(s32 player);
void mnPlayersVSRecallPuck(s32 player);
void mnPlayersVSBackToVSMode(void);
void mnPlayersVSDetectBack(s32 player);
s32 mnPlayersVSCheckBackInRange(GObj *gobj);
void mnPlayersVSCursorProcUpdate(GObj *gobj);
void mnPlayersVSUpdatePuck(GObj *gobj, s32 puck);
void mnPlayersVSCenterPuckInPortrait(GObj *gobj, s32 fkind);
s32 mnPlayersVSRandFighterKind(GObj *gobj);
void mnPlayersVSMovePuck(s32 player);
void mnPlayersVSPuckProcUpdate(GObj *gobj);
void mnPlayersVSMakeCursorCamera(void);
void mnPlayersVSMakePuckCamera(void);
void mnPlayersVSMakeHandicapLevelCamera(void);
void mnPlayersVSMakeReadyCamera(void);
void mnPlayersVSMakeCursor(s32 player);
void mnPlayersVSPuckProcDisplay(GObj *gobj);
void mnPlayersVSMakePuck(s32 player);
f32 mnPlayersVSGetPuckDistance(s32 this_player, s32 other_player);
void mnPlayersVSPuckAdjustOverlap(s32 this_player, s32 other_player, f32 unused);
void mnPlayersVSPuckAdjustPortraitEdge(s32 player);
void mnPlayersVSPuckAdjustPlaced(s32 player);
void mnPlayersVSPuckAdjustRecall(s32 player);
void mnPlayersVSPuckAdjustProcUpdate(GObj *gobj);
void mnPlayersVSMakePuckAdjust(void);
void mnPlayersVSUpdatePuckGlowColor(GObj *gobj);
void mnPlayersVSMakePuckGlow(void);
void mnPlayersVSCostumeSyncProcUpdate(GObj *gobj);
void mnPlayersVSMakeCostumeSync(void);
void mnPlayersVSSpotlightProcUpdate(GObj *gobj);
void mnPlayersVSMakeSpotlight(void);
void mnPlayersVSReadyProcUpdate(GObj *gobj);
void mnPlayersVSMakeReady(void);
void mnPlayersVSUpdateGate(s32 player);
void mnPlayersVSUpdateControllerOrders(void);
s32 mnPlayersVSGetReadyPlayerCount(void);
void mnPlayersVSSetPlayerNot(s32 player);
void mnPlayersVSSetIdlePlayerNotAll(void);
sb32 mnPlayersVSCheckSingleTeam(void);
sb32 mnPlayersVSCheckNoPuckOnPortraitAll(void);
sb32 mnPlayersVSCheckReady(void);
void mnPlayersVSSetSceneData(void);
void mnPlayersVSPauseSlotProcesses(void);
void mnPlayersVSFuncRun(GObj *gobj);
s32 mnPlayersVSGetNextTimeValue(s32 current_value);
s32 mnPlayersVSGetPrevTimeValue(s32 current_value);
void mnPlayersVSInitPlayer(s32 player);
void mnPlayersVSResetPlayer(s32 player);
void mnPlayersVSInitVars(void);
void mnPlayersVSInitSlot(s32 player);
void mnPlayersVSInitSlotAll(void);
void mnPlayersVSFuncStart(void);
void mnPlayersVSStartScene(void);
void mnPlayersVSLoadFiles(void);

#endif /* SSB_DC_MNPLAYERSVS_H */
