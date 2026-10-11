/* sc1pbonusstage.h -- the bonus stages themselves,
 * sc/sc1pmode/sc1pbonusstage.c. Overlay 6.
 *
 * ONE SCENE FOR TWO GAMES. nSCKind1PBonusStage plays either Break the
 * Targets or Board the Platforms, and which one is decided by nothing
 * more than the stage kind: `gkind` in [nGRKindBonus1Start,
 * nGRKindBonus1End] is a target course, past nGRKindBonus2Start a
 * platform course, and every shared function in the file tests that one
 * range. The twenty-four courses are twelve of each, one per playable
 * fighter, and both games run the same spine:
 *
 *   one human fighter, no CPUs, no stocks, a clock that counts UP from
 *   zero (or, from the ladder, DOWN from two minutes), ten tasks to
 *   clear, a row of ten task sprites at the top of the screen that
 *   loses one per task, and an announcer who says COMPLETE! when the
 *   tenth goes. SCBATTLE_BONUSGAME_TASK_MAX is that ten and the game
 *   HANGS THE CONSOLE if a course carries any other number.
 *
 * It is reached two ways, which is the other thing every function here
 * tests. From the 1P ladder (`scene_prev == nSCKind1PGame`) it is a
 * rung: two minutes on the clock, the score screen after it, and no
 * backup record written for a practice run. From the bonus practice
 * select (nSCKind1PBonus1Players / nSCKind1PBonus2Players) it is its
 * own mode: an untimed run whose completion time goes into
 * gSCManagerBackupData.spgame_records, and clearing all twelve target
 * courses is what unlocks LUIGI.
 *
 * BOTH GAMES ARE HERE. Break the Targets and Board the Platforms are
 * both complete.
 *
 * The two games are not the same shape, and the difference is worth
 * knowing before reading either half. A TARGET is an item: the course's
 * BTG1 block is a list of ten positions, the scene makes ten
 * nITKindTarget items at them, and breaking one is the item's own damage
 * proc calling back. A PLATFORM is part of the MAP: every floor line
 * whose material is nMPMaterialDetect is one, the scene hangs a model on
 * the yakumono DObj the collision system already keeps for that line,
 * and boarding one is a per-frame sweep of the fighters asking what each
 * is standing on. So a target course's task count comes from a table the
 * game HANGS on if it disagrees, and a platform course's comes from its
 * collision, where there is nothing to disagree with.
 *
 * The six platform models (three sizes, each with a boarded twin) are
 * shared by all twelve courses and live in romdisk/bonus2plat.pak, which
 * a course's Stage opens on its way in.
 *
 * Every function is the decomp's by name and body, the REGION_US arms;
 * the line numbers are the decomp's. What is not here, and where it is
 * said:
 *   - the lighting pre-render (sc1PBonusStageFuncLights) --
 *     dSC1PBonusStageTaskmanSetup, as src/dc/scvsbattle.c's;
 *   - syVideoInit, the z-buffer and the arena_size line --
 *     sc1PBonusStageStartScene, the same two cuts;
 *   - the reloc loader (sc1pbonusstagefiles.c's whole 26 lines) --
 *     sc1PBonusStageSetupFiles, which keeps the load under it;
 *   - sc1PBonusStageBonus1LoadFile, the target's ITAttributes are
 *     in the item pack.
 */
#ifndef SSB_DC_SC1PBONUSSTAGE_H
#define SSB_DC_SC1PBONUSSTAGE_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

extern SYTaskmanSetup dSC1PBonusStageTaskmanSetup;

void sc1PBonusStageFuncUpdate(void);
void sc1PBonusStageInitVars(void);
void sc1PBonusStageSetupFiles(void);
void sc1PBonusStageMakeTargets(void);
void sc1PBonusStageUpdateTargetInterface(void);
void sc1PBonusStageUpdateTargetCount(void);
void sc1PBonusStageMakeBonus1Ground(void);
void sc1PBonusStageBonus2LoadFile(void);
s32 sc1PBonusStageGetPlatformKind(s32 line_id);
void sc1PBonusStageInitPlatforms(s32 line_id);
void sc1PBonusStageMakePlatforms(void);
void sc1PBonusStageUpdatePlatformInterface(void);
void sc1PBonusStageUpdatePlatformCount(DObj *dobj);
void sc1PBonusStageBonus2ProcUpdate(GObj *ground_gobj);
void sc1PBonusStageMakeBonus2Ground(void);
void sc1PBonusStageMakeBumpers(void);
void sc1PBonusStageInitBonus2(void);
void sc1PBonusStageInterfaceThreadUpdate(GObj *interface_gobj);
void sc1PBonusStageMakeInterface(void);
void sc1PBonusStageInitCamera(void);
void sc1PBonusStageMakeTargetSprites(void);
void sc1PBonusStageMakePlatformSprites(void);
void sc1PBonusStageMakeTaskSprites(void);
void sc1PBonusStageGetPlayerStartPosition(Vec3f *pos);
void sc1PBonusStageTimerProcUpdate(GObj *interface_gobj);
void sc1PBonusStageSetTimeUp(void);
void sc1PBonusStageTimeUpProcUpdate(GObj *interface_gobj);
void sc1PBonusStageMakeTimeUp(void);
void sc1PBonusStageMakeTimer(void);
void sc1PBonusStageSetPlayerInterfacePositions(void);
void sc1PBonusStageFuncStart(void);
void sc1PBonusStageSetBonusStats(s32 tasks_remain);
void sc1PBonusStageWriteBackup(sb32 is_tasks_fail, s32 fkind);
void sc1PBonusStageStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[6]. */
void sc1PBonusStageOverlayLoad(void);

#endif /* SSB_DC_SC1PBONUSSTAGE_H */
