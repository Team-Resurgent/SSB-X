/* sc1pgame.h -- the 1P ladder's battle scene, sc/sc1pmode/sc1pgame.c.
 * Overlay 65.
 *
 * The scene that plays a rung. src/dc/scvsbattle.c is its template --
 * it IS a VS battle, built by the same calls in the same order -- and
 * every cut this file shares with that one is that file's cut, for that
 * file's reason. What it adds on top is the ladder:
 *
 *   - dSC1PGameStageDesc and dSC1PGameComputerDesc, one row per rung:
 *     who the enemies are, where they stand, the stage, the item
 *     switch, and the five CPU levels and handicaps the difficulty
 *     slider picks between. sc1PGameSetupStageAll reads these into
 *     gSCManager1PGameBattleState before a single GObj is made.
 *   - the team rungs. Yoshi Team is eighteen enemies, Kirby Team eight
 *     and the Fighting Polygon Team thirty, but only a few stand on the
 *     stage at a time: the rest wait in sSC1PGamePlayerSetups and
 *     sc1PGameSpawnEnemyTeamNext swaps a fresh one in as each falls.
 *     sc1PGameInitTeamStockDisplay draws the remainder as a grid of
 *     heads in the corner.
 *   - the boss rung's own interface (sc1PGameBoss*): the tag is hidden,
 *     the player's control is locked for the approach, and the map
 *     bounds are ignored while Master Hand is on screen.
 *   - sc1PGameAppendBonusStats, 650 lines of accounting -- every attack
 *     landed and taken, by id, ground or air, smash or not, projectile
 *     or not, plus the tomatoes, hearts and stars picked up. It is what
 *     the bonus list on the stage-clear screen is computed from.
 *
 * WHAT CANNOT BE PLAYED YET, measured by disc probe and not guessed. A
 * rung runs when the port has BOTH its fighters' packs and its map's,
 * because sc1PGameGetStartPosition takes the spawn points out of the
 * map and spins in the game's own error loop if the map it is handed
 * has none of the kind it asks for.
 *
 * Seven of the eighteen rows run today: Link (Hyrule), Fox (Sector),
 * Mario Bros. (Castle), Pikachu (Saffron), Giant DK (Kongo), Kirby Team
 * (Dream Land) and Samus (Zebes), and with them the four Challenger
 * rungs once those fighters are unlocked. What blocks the rest is data,
 * not this file:
 *   - five maps have no exported pack -- Small Yoshi's Island (Yoshi
 *     Team), Meta Crystal (Metal Mario), Duel Zone (the Polygons), Race
 *     to the Finish and Final Destination (Master Hand). A rung on one
 *     of them plays Hyrule instead, says so on the log, and then stops
 *     on the first missing spawn point.
 *   - Metal Mario, the Fighting Polygon Team and Master Hand are among
 *     fourteen files of gFTData* rows with no functions.
 *   - the three bonus rungs belong to sc1pbonusstage.c.
 * Every one of their rows, cameras and stock counters is here and
 * correct; nothing in this file is waiting on this file.
 *
 * Every function is the decomp's by name and body, the REGION_US arms;
 * the line numbers are the decomp's. What is not here, and where it is
 * said:
 *   - the lighting pre-render (sc1PGameFuncLights) --
 *     dSC1PGameTaskmanSetup;
 *   - syVideoInit, the z-buffer and the arena line -- sc1PGameStartScene;
 *   - the reloc setup and its two status buffers -- sc1PGameSetupFiles,
 *     which keeps the HUD load underneath them;
 *   - the cartridge check -- sc1PGameFuncStart, which says what it was
 *     and why there is nothing to port;
 *   - Kirby's copied-Ness particle load -- sc1PGameFuncStart;
 *   - ftManagerSetupFilesPlayablesAll and gmCameraMakeWallpaperCamera --
 *     src/dc/scvsbattle.c's header block, which names both;
 *   - the Polygon team's figatree measurement -- sc1PGameFuncStart, at
 *     the nSC1PGameStageZako arm;
 *   - the two lines that mute the sound driver for the boss defeat --
 *     sc1PGameBossDefeatInterfaceProcUpdate;
 *
 * TWO LINES THAT ARE THE PORT'S AND NOT THE GAME'S, both in
 * sc1PGameFuncStart and both next to gmCameraMakeBattleCamera: the
 * camera is made AFTER the spawn loop, not before it, and its
 * camera_mask is set by hand. src/dc/scvsbattle.c:399 and :416 carry
 * the same pair for the same reasons. Without the first the wallpaper
 * has nothing to hang off; without the second the camera captures no
 * DL link and the match draws nothing at all.
 *
 * TWO FUNCTIONS THIS STEP ADDED ELSEWHERE, because this is the first
 * file to call them: ftCommonAppearSetPosition (src/dc/ftcommon.c,
 * ftcommonentry.c:271 -- how each next member of a team rung arrives)
 * and ifCommonBattleBossDefeatSetGameStatus (src/dc/ifcommon.c,
 * ifcommon.c:3288). Three of sc1pgame.c's bonus counters lived in
 * defined here, where the decomp defines them.
 */
#ifndef SSB_DC_SC1PGAME_H
#define SSB_DC_SC1PGAME_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>
#include <ft/fighter.h>
#include <sc/sc1pmode/sc1pgame.h>

extern SYTaskmanSetup dSC1PGameTaskmanSetup;

/* sc1pgame.c:378, one row per rung -- the stage, the item switch and
 * who stands on it. The decomp's own header does not declare it (the
 * file is the only one that reads it there); src/dc/sndres.c reads it
 * to know which stage and which fighters a rung's sounds have to be
 * staged for, before the scene that fills the battle state runs. The
 * eighteen rows are nSC1PGameStageLink .. nSC1PGameStageCaptain. */
extern SC1PGameStage dSC1PGameStageDesc[];

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[65]. */
void sc1PGameOverlayLoad(void);

#endif /* SSB_DC_SC1PGAME_H */
