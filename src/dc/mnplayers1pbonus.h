/* mnplayers1pbonus.h -- the bonus stages' practice select,
 * mn/mnplayers/mnplayers1pbonus.c. Overlay 29.
 *
 * The second and third doors out of the 1P submenu
 * (src/dc/mn1pmode.c), and ONE scene for both of them: it picks the
 * fighter whose Break the Targets or Board the Platforms course is
 * about to be played, and which of the two games it is comes in as the
 * SCENE KIND -- nSCKind1PBonus1Players or nSCKind1PBonus2Players --
 * which mnPlayers1PBonusInitVars turns into sMNPlayers1PBonusBonusKind
 * (0 targets, 1 platforms). Every readout on the screen then asks that
 * one variable which half of the backup record to show. It hands off
 * to nSCKind1PBonusStage (src/dc/sc1pbonusstage.c),
 * which reads the same pair back.
 *
 * It is the LAST of the four selects, and the 1P game's
 * (src/dc/mnplayers1pgame.c) is its template: 82 of its
 * 100 named functions share a name with one of that file's, and its
 * eleven files are that screen's eleven to the entry -- so this step
 * exports no new file. It does widen one bank: romdisk/mn1pselect.spr
 * (relocData 23) was carrying nine of that file's fifteen sprites, and
 * the six left out were exactly the records panel's, so the bank is now
 * the whole file, which is what the game loads for either select.
 * What mnplayers1pgame.c does NOT have, and this does:
 *
 *   - the GAME MODE row instead of LEVEL and STOCK: the title over the
 *     grid is BREAK THE TARGETS or BOARD THE PLATFORMS, and the player
 *     can flip it (CheckGameModeInRange, UpdateGameMode) -- which is
 *     also what makes one scene serve two doors;
 *   - a records panel counted in TIME rather than points -- this
 *     fighter's best time on this game and its task count (GetBestTime,
 *     GetMins/GetSec/GetCSec, MakeBestTime, GetBestTaskCount,
 *     MakeBestTaskCount, MakeHiScore), and, once all twelve have
 *     cleared the game (CheckBonusCompleteAll asks all twelve), a
 *     TOTAL BEST TIME line summing the UNLOCKED ones only
 *     (GetTotal*, which skip CheckFighterLocked, and MakeTotalTime);
 *   - a gate camera of its own (MakeGateCamera) where the 1P game
 *     select has a player-kind camera.
 *
 * Every function is the decomp's by name and body, the REGION_US arms;
 * the line numbers are the decomp's. What is not here, and where it is
 * said:
 *   - the lighting pre-render (mnPlayers1PBonusFuncLights) and the two
 *     Lights1 sets it emits -- at the cut;
 *   - the clear camera the PVR does itself -- mnPlayers1PBonusFuncStart;
 *   - syVideoInit, the z-buffer and the arena_size line --
 *     mnPlayers1PBonusStartScene;
 *   - the reloc setup: ten sprite banks and the spotlight model stand
 *     in for the eleven files -- mnPlayers1PBonusLoadFiles.
 *
 * Probing it: -DDB_BOOT_SCENE=nSCKind1PBonus1Players (or ...2Players)
 * boots straight here, and src/dc/db.c's db_feed_pad1 carries the puck
 * onto Mario's portrait and drops it, which is the only way the records
 * panel draws at all (it needs a fighter under the puck).
 * -DDB_BOOT_BONUS_RECORDS=1 deals a save with both games finished by
 * everybody, which is the only way the best-TIME arm of that panel and
 * the TOTAL BEST TIME line draw.
 */
#ifndef SSB_DC_MNPLAYERS1PBONUS_H
#define SSB_DC_MNPLAYERS1PBONUS_H

/* Narrow, as the three other selects' headers are: <mn/menu.h> would
 * pull the decomp's own mnmaps.h in behind it, whose mnMapsMakeLayer
 * and mnMapsMakeModel take the reloc ground data the port replaced. */
#include <sys/obj.h>
#include <sys/taskman.h>
#include <mn/mndef.h>

extern SYTaskmanSetup dMNPlayers1PBonusTaskmanSetup;
extern u32 dMNPlayers1PBonusFileIDs[];
extern void *sMNPlayers1PBonusFiles[];

s32 mnPlayers1PBonusGetPowerOf(s32 base, s32 exp);
void mnPlayers1PBonusSetDigitColors(SObj *sobj, u32 *colors);
s32 mnPlayers1PBonusGetNumberDigitCount(s32 num, s32 digit_count_max);
void mnPlayers1PBonusMakeNumber(GObj *gobj, s32 number, f32 x, f32 y, u32 *colors, s32 digit_count_max, sb32 is_fixed_digit_count);
void mnPlayers1PBonusSelectFighterPuck(s32 player, s32 select_button);
f32 mnPlayers1PBonusGetNextPortraitX(s32 portrait, f32 current_pos_x);
sb32 mnPlayers1PBonusCheckFighterCrossed(s32 fkind);
void mnPlayers1PBonusPortraitProcUpdate(GObj *gobj);
void mnPlayers1PBonusSetPortraitWallpaperPosition(SObj *sobj, s32 portrait);
void mnPlayers1PBonusPortraitAddCross(GObj *gobj, s32 portrait);
sb32 mnPlayers1PBonusCheckFighterLocked(s32 fkind);
s32 mnPlayers1PBonusGetFighterKind(s32 portrait);
s32 mnPlayers1PBonusGetPortrait(s32 fkind);
void mnPlayers1PBonusPortraitProcDisplay(GObj *gobj);
void mnPlayers1PBonusMakePortraitShadow(s32 portrait);
void mnPlayers1PBonusMakePortrait(s32 portrait);
void mnPlayers1PBonusMakePortraitAll(void);
void mnPlayers1PBonusMakeNameAndEmblem(GObj *gobj, s32 player, s32 fkind);
void mnPlayers1PBonusMakePortraitCamera(void);
void mnPlayers1PBonusMakePortraitWallpaperCamera(void);
void mnPlayers1PBonusMakePortraitFlashCamera(void);
void mnPlayers1PBonusMakeGateCamera(void);
void mnPlayers1PBonusSetGateLUT(GObj *gobj, s32 player);
void mnPlayers1PBonusMakeGate(s32 player);
void mnPlayers1PBonusMakeWallpaper(void);
void mnPlayers1PBonusMakeWallpaperCamera(void);
void mnPlayers1PBonusMakeLabels(void);
void mnPlayers1PBonusMakeLabelsCamera(void);
u32 mnPlayers1PBonusGetBestTime(s32 fkind);
s32 mnPlayers1PBonusGetMins(s32 tics);
s32 mnPlayers1PBonusGetSec(s32 tics);
s32 mnPlayers1PBonusGetCSec(s32 tics);
s32 mnPlayers1PBonusGetTotalMins(void);
s32 mnPlayers1PBonusGetTotalSec(void);
s32 mnPlayers1PBonusGetTotalCSec(void);
void mnPlayers1PBonusMakeBestTime(void);
u8 mnPlayers1PBonusGetBestTaskCount(s32 fkind);
void mnPlayers1PBonusMakeBestTaskCount(void);
sb32 mnPlayers1PBonusCheckBonusComplete(s32 fkind);
void mnPlayers1PBonusMakeHiScore(void);
void mnPlayers1PBonusMakeTotalTime(void);
s32 mnPlayers1PBonusGetCostume(s32 fkind, s32 select_button);
s32 mnPlayers1PBonusGetStatusSelected(s32 fkind);
void mnPlayers1PBonusFighterProcUpdate(GObj *fighter_gobj);
void mnPlayers1PBonusMakeFighter(GObj *fighter_gobj, s32 player, s32 fkind);
void mnPlayers1PBonusMakeFighterCamera(void);
void mnPlayers1PBonusUpdateCursor(GObj *gobj, s32 player, s32 cursor_status);
void mnPlayers1PBonusCheckTimeArrowRInRange(void);
void mnPlayers1PBonusCheckTimeArrowLInRange(void);
sb32 mnPlayers1PBonusCheckBackInRange(GObj *gobj);
sb32 mnPlayers1PBonusCheckPuckInRange(GObj *gobj, s32 cursor_player, s32 player);
void mnPlayers1PBonusResetPlayer(s32 player);
void mnPlayers1PBonusUpdateFighter(s32 player);
void mnPlayers1PBonusUpdateNameAndEmblem(s32 player);
void mnPlayers1PBonusDestroyPortraitFlash(s32 player);
void mnPlayers1PBonusPortraitFlashThreadUpdate(GObj *gobj);
void mnPlayers1PBonusMakePortraitFlash(s32 player);
void mnPlayers1PBonusAnnounceFighter(s32 player, s32 slot);
sb32 mnPlayers1PBonusCheckSelectFighter(GObj *gobj, s32 player, s32 unused, s32 select_button);
void mnPlayers1PBonusUpdateCursorGrabPriorities(s32 player, s32 puck);
void mnPlayers1PBonusUpdateCursorPlacementPriorities(s32 player);
void mnPlayers1PBonusSetCursorPuckOffset(s32 player);
void mnPlayers1PBonusSetCursorGrab(s32 player);
sb32 mnPlayers1PBonusCheckCursorPuckGrab(GObj *gobj, s32 player);
s32 mnPlayers1PBonusGetForcePuckFighterKind(void);
s32 mnPlayers1PBonusGetPuckFighterKind(s32 player);
void mnPlayers1PBonusAdjustCursor(GObj *gobj, s32 player);
void mnPlayers1PBonusUpdateCursorNoRecall(GObj *gobj, s32 player);
void mnPlayers1PBonusUpdateCostume(s32 player, s32 select_button);
sb32 mnPlayers1PBonusCheckManFighterSelected(s32 unused);
void mnPlayers1PBonusRecallPuck(s32 player);
void mnPlayers1PBonusBackTo1PMode(void);
void mnPlayers1PBonusDetectBack(s32 player);
sb32 mnPlayers1PBonusCheckGameModeInRange(GObj *gobj);
void mnPlayers1PBonusUpdateGameMode(void);
void mnPlayers1PBonusCursorProcUpdate(GObj *gobj);
void mnPlayers1PBonusMovePuck(s32 player);
void mnPlayers1PBonusPuckProcUpdate(GObj *gobj);
void mnPlayers1PBonusMakeReadyCamera(void);
void mnPlayers1PBonusMakeCursorCamera(void);
void mnPlayers1PBonusMakePuckCamera(void);
void mnPlayers1PBonusMakeCursor(s32 player);
void mnPlayers1PBonusMakePuck(s32 player);
void mnPlayers1PBonusPuckAdjustPortraitEdge(s32 player);
void mnPlayers1PBonusPuckAdjustPlaced(s32 player);
void mnPlayers1PBonusPuckAdjustRecall(s32 player);
void mnPlayers1PBonusPuckAdjustProcUpdate(GObj *gobj);
void mnPlayers1PBonusMakePuckAdjust(void);
void mnPlayers1PBonusSpotlightProcUpdate(GObj *gobj);
void mnPlayers1PBonusMakeSpotlight(void);
void mnPlayers1PBonusReadyProcUpdate(GObj *gobj);
void mnPlayers1PBonusMakeReady(void);
void mnPlayers1PBonusSetSceneData(void);
void mnPlayers1PBonusFuncRun(GObj *gobj);
void mnPlayers1PBonusInitPlayer(void);
void mnPlayers1PBonusInitVars(void);
void mnPlayers1PBonusInitSlot(s32 player);
sb32 mnPlayers1PBonusCheckBonusCompleteAll(void);
void mnPlayers1PBonusLoadFiles(void);
void mnPlayers1PBonusFuncStart(void);
void mnPlayers1PBonusStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[29]. */
void mnPlayers1PBonusOverlayLoad(void);

#endif /* SSB_DC_MNPLAYERS1PBONUS_H */
