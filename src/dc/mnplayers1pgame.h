/* mnplayers1pgame.h -- the 1P game's character select,
 * mn/mnplayers/mnplayers1pgame.c. Overlay 27.
 *
 * The first door out of the 1P submenu (src/dc/mn1pmode.c) and the
 * front of the ladder: it picks the one fighter the player takes
 * through the 1P game, the difficulty the ladder runs at and the stock
 * count it starts with, then hands off to nSCKind1PIntro.
 *
 * It is the third of the four selects, and the two already ported are
 * its template: of its 119 named functions, 87 share a name with one of
 * mn/mnplayers/mnplayersvs.c's or mnplayers1ptraining.c's, so
 * src/dc/mnplayersvs.c and src/dc/mnplayers1ptraining.c answer every
 * convention here. What those two do NOT have, and this does:
 *
 *   - one slot, not four, and no CPU panel -- the puck the player
 *     carries is the only one on the screen;
 *   - the LEVEL row, a five-name slider (VERY EASY .. VERY HARD) over
 *     relocData file 24 (MakeLevel, LevelThreadUpdate,
 *     CheckLevelArrowPress);
 *   - the STOCK row, a 1-5 slider drawn as that many grey heads out of
 *     relocData file 25 (MakeStock, StockThreadUpdate);
 *   - the records panel beside the portrait grid -- this fighter's best
 *     time, its bonus count and its hi score, and the totals over all
 *     twelve -- spelled out of the alphabet in file 33 and the digits
 *     in file 36 (MakeFighterRecord, MakeTotalRecord, MakeString,
 *     MakeNumber, MakeTimeNumber).
 *
 * Every function is the decomp's by name and body, the REGION_US arms;
 * the line numbers are the decomp's. What is not here, and where it is
 * said:
 *   - the lighting pre-render (mnPlayers1PGameFuncLights) -- at the cut;
 *   - the clear camera the PVR does itself -- mnPlayers1PGameFuncStart;
 *   - syVideoInit, the z-buffer and the arena_size line --
 *     mnPlayers1PGameStartScene;
 *   - the reloc setup: ten sprite banks and the spotlight model stand
 *     in for the eleven files -- mnPlayers1PGameLoadFiles.
 */
#ifndef SSB_DC_MNPLAYERS1PGAME_H
#define SSB_DC_MNPLAYERS1PGAME_H

/* Narrow, as src/dc/mnplayersvs.h and src/dc/mnplayers1ptraining.h
 * are: <mn/menu.h> would pull the decomp's own mnmaps.h in behind it,
 * whose mnMapsMakeLayer and mnMapsMakeModel take the reloc ground data
 * the port replaced, and src/dc/scmanager.c includes both that and
 * src/dc/mnmaps.h. */
#include <sys/obj.h>
#include <sys/taskman.h>
#include <mn/mndef.h>

extern SYTaskmanSetup dMNPlayers1PGameTaskmanSetup;
extern u32 dMNPlayers1PGameFileIDs[];
extern void *sMNPlayers1PGameFiles[];

s32 mnPlayers1PGameGetPowerOf(s32 base, s32 exp);
void mnPlayers1PGameSetDigitColors(SObj *sobj, u32 *colors);
s32 mnPlayers1PGameGetNumberDigitCount(s32 number, s32 digit_count_max);
void mnPlayers1PGameMakeNumber(GObj *gobj, s32 number, f32 x, f32 y, u32 *colors, s32 digit_count_max, sb32 is_fixed_digit_count);
s32 mnPlayers1PGameGetCharacterID(const char c);
f32 mnPlayers1PGameGetCharacterSpacing(const char *str, s32 c);
void mnPlayers1PGameMakeString(GObj *gobj, const char *str, f32 x, f32 y, u32 *colors);
void mnPlayers1PGameSelectFighterPuck(s32 player, s32 select_button);
f32 mnPlayers1PGameGetNextPortraitX(s32 portrait, f32 current_pos_x);
sb32 mnPlayers1PGameCheckFighterCrossed(s32 fkind);
void mnPlayers1PGamePortraitProcUpdate(GObj *gobj);
void mnPlayers1PGameSetPortraitWallpaperPosition(SObj *sobj, s32 portrait);
void mnPlayers1PGamePortraitAddCross(GObj *gobj, s32 portrait);
sb32 mnPlayers1PGameCheckFighterLocked(s32 fkind);
void func_ovl27_80132794(void);
s32 mnPlayers1PGameGetFighterKind(s32 portrait);
s32 mnPlayers1PGameGetPortrait(s32 fkind);
void mnPlayers1PGamePortraitProcDisplay(GObj *gobj);
void mnPlayers1PGameMakePortraitShadow(s32 portrait);
void mnPlayers1PGameMakePortrait(s32 portrait);
void mnPlayers1PGameMakePortraitAll(void);
void mnPlayers1PGameMakeNameAndEmblem(GObj *gobj, s32 player, s32 fkind);
void mnPlayers1PGameMakePortraitCamera(void);
void mnPlayers1PGameMakePortraitWallpaperCamera(void);
void mnPlayers1PGameMakePortraitFlashCamera(void);
void mnPlayers1PGameMakePlayerKindCamera(void);
void mnPlayers1PGameSetGateLUT(GObj *gobj, s32 player);
void mnPlayers1PGameMakeGate(s32 player);
void mnPlayers1PGameMakeTimeNumber(GObj *gobj, s32 number, f32 x, f32 y, u32 *colors, s32 digit_count_max, sb32 is_fixed_digit_count);
void mnPlayers1PGameMakeTimeSetting(s32 number);
void mnPlayers1PGameMakeTimeSelect(s32 number);
void mnPlayers1PGameMakeWallpaper(void);
void mnPlayers1PGameMakeWallpaperCamera(void);
void mnPlayers1PGameLabelsProcDisplay(GObj *gobj);
SObj* mnPlayers1PGameGetArrowSObj(GObj *gobj, s32 direction);
void mnPlayers1PGameLevelThreadUpdate(GObj *gobj);
void mnPlayers1PGameMakeLevel(s32 level);
void mnPlayers1PGameMakeLevelOption(void);
void mnPlayers1PGameStockThreadUpdate(GObj *gobj);
void mnPlayers1PGameMakeStock(s32 stock, s32 fkind);
void mnPlayers1PGameMakeStockOption(void);
void mnPlayers1PGameMakeLabels(void);
void mnPlayers1PGameMakeLabelsCamera(void);
u32 mnPlayers1PGameGetHiScore(s32 fkind);
void mnPlayers1PGameMakeHiScore(void);
u32 mnPlayers1PGameGetBonusCount(s32 fkind);
void mnPlayers1PGameMakeBonusCount(void);
void mnPlayers1PGameMakeFighterRecord(void);
u32 mnPlayers1PGameGetTotalHiScore(void);
void mnPlayers1PGameMakeTotalHiScore(void);
u32 mnPlayers1PGameGetTotalBonusCount(void);
void mnPlayers1PGameMakeTotalBonusCount(void);
void mnPlayers1PGameMakeTotalRecord(void);
void func_ovl27_80134EB0(void);
void func_ovl27_80134EB8(void);
s32 mnPlayers1PGameGetFreeCostume(s32 fkind, s32 select_button);
s32 mnPlayers1PGameGetStatusSelected(s32 fkind);
void mnPlayers1PGameFighterProcUpdate(GObj *fighter_gobj);
void mnPlayers1PGameMakeFighter(GObj *fighter_gobj, s32 player, s32 fkind, s32 costume);
void mnPlayers1PGameMakeFighterCamera(void);
void mnPlayers1PGameUpdateCursor(GObj *gobj, s32 player, s32 cursor_status);
sb32 mnPlayers1PGameCheckTimeArrowRInRange(GObj *gobj);
sb32 mnPlayers1PGameCheckTimeArrowLInRange(GObj *gobj);
sb32 mnPlayers1PGameCheckBackInRange(GObj *gobj);
sb32 mnPlayers1PGameCheckPuckInRange(GObj *gobj, s32 cursor_player, s32 player);
void func_ovl27_801357FC(void);
void mnPlayers1PGameUpdateFighter(s32 player);
void func_ovl27_801358BC(void);
void mnPlayers1PGameUpdateNameAndEmblem(s32 player);
void mnPlayers1PGameDestroyPortraitFlash(s32 player);
void mnPlayers1PGamePortraitFlashThreadUpdate(GObj *gobj);
void mnPlayers1PGameMakePortraitFlash(s32 player);
void mnPlayers1PGameAnnounceFighter(s32 player, s32 slot);
void func_ovl27_80135BFC(void);
sb32 mnPlayers1PGameCheckSelectFighter(GObj *gobj, s32 player, s32 unused, s32 select_button);
void mnPlayers1PGameUpdateCursorGrabPriorities(s32 player, s32 puck);
void mnPlayers1PGameUpdateCursorPlacementPriorities(s32 player);
void mnPlayers1PGameSetCursorPuckOffset(s32 player);
void mnPlayers1PGameSetCursorGrab(s32 player);
sb32 mnPlayers1PGameCheckCursorPuckGrab(GObj *gobj, s32 player);
s32 mnPlayers1PGameGetForcePuckFighterKind(void);
s32 mnPlayers1PGameGetPuckFighterKind(s32 player);
void mnPlayers1PGameAdjustCursor(GObj *gobj, s32 player);
void mnPlayers1PGameUpdateCursorNoRecall(GObj *gobj, s32 player);
sb32 mnPlayers1PGameCheckLevelArrowRInRange(GObj *gobj);
sb32 mnPlayers1PGameCheckLevelArrowLInRange(GObj *gobj);
sb32 mnPlayers1PGameCheckLevelArrowPress(GObj *gobj);
sb32 mnPlayers1PGameCheckStockArrowRInRange(GObj *gobj);
sb32 mnPlayers1PGameCheckStockArrowLInRange(GObj *gobj);
sb32 mnPlayers1PGameCheckStockArrowPress(GObj *gobj);
void mnPlayers1PGameUpdateCostume(s32 player, s32 select_button);
sb32 mnPlayers1PGameCheckManFighterSelected(s32 player);
void mnPlayers1PGameRecallPuck(s32 player);
void mnPlayers1PGameBackTo1PMode(void);
void mnPlayers1PGameDetectBack(s32 player);
void mnPlayers1PGameCursorProcUpdate(GObj *gobj);
void func_ovl27_8013702C(void);
void mnPlayers1PGameCenterPuckInPortrait(GObj *gobj, s32 fkind);
void func_ovl27_801370E4(void);
void mnPlayers1PGameMovePuck(s32 player);
void mnPlayers1PGamePuckProcUpdate(GObj *gobj);
void mnPlayers1PGameMakeCursorCamera(void);
void mnPlayers1PGameMakePuckCamera(void);
void mnPlayers1PGameMakeReadyCamera(void);
void mnPlayers1PGameMakeCursor(s32 player);
void mnPlayers1PGameMakePuck(s32 player);
void func_ovl27_801376F0(void);
void mnPlayers1PGamePuckAdjustPortraitEdge(s32 player);
void mnPlayers1PGamePuckAdjustPlaced(s32 player);
void mnPlayers1PGamePuckAdjustRecall(s32 player);
void mnPlayers1PGamePuckAdjustProcUpdate(GObj *gobj);
void mnPlayers1PGameMakePuckAdjust(void);
void mnPlayers1PGameSpotlightProcUpdate(GObj *gobj);
void mnPlayers1PGameMakeSpotlight(void);
void mnPlayers1PGameReadyProcUpdate(GObj *gobj);
void mnPlayers1PGameMakeReady(void);
void func_ovl27_80137EE0(void);
void func_ovl27_80137EE8(void);
sb32 mnPlayers1PGameCheckReady(void);
void mnPlayers1PGameSetSceneData(void);
void mnPlayers1PGamePauseSlotProcesses(void);
void mnPlayers1PGameFuncRun(GObj *gobj);
s32 mnPlayers1PGameGetNextTimeValue(s32 value);
s32 mnPlayers1PGameGetPrevTimeValue(s32 value);
void mnPlayers1PGameInitPlayer(s32 player);
void func_ovl27_801381D0(void);
void mnPlayers1PGameInitVars(void);
void mnPlayers1PGameInitSlot(s32 player);
void mnPlayers1PGameLoadFiles(void);
void mnPlayers1PGameFuncStart(void);
void mnPlayers1PGameStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[27]. */
void mnPlayers1PGameOverlayLoad(void);

#endif /* SSB_DC_MNPLAYERS1PGAME_H */
