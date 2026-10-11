/* scvsbattle.h -- the VS battle scene, sc/sccommon/scvsbattle.c ported.
 *
 * This is the scene that owns a match. Its six functions are the whole of it -- the setup that builds
 * every GObj a battle needs, the one-line per-tic update, the facing
 * rule each fighter spawns with, and the scene entry the scene manager
 * calls, and sudden death -- and all but the two setups are close to
 * verbatim. What is cut is cut because the module it calls is not
 * ported yet, and every cut is named where it happens.
 *
 * Sudden death is here, which is why the scene has
 * six functions and not four: scVSBattleSetScoreCheckSuddenDeath sorts
 * a TIME match's winners and, on a tie, builds the state for a second
 * battle, and scVSBattleStartSuddenDeath is that battle's setup -- the
 * tied players only, one stock each, spawned at 300% with the entry
 * sequence skipped. Both are verbatim.
 */
#ifndef SSB_DC_SCVSBATTLE_H
#define SSB_DC_SCVSBATTLE_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* scvsbattle.c:24-64 dSCVSBattleTaskmanSetup, the scene's own setup:
 * what to run each tic, what to draw, and how many of each object the
 * scene may make. */
extern SYTaskmanSetup dSCVSBattleTaskmanSetup;

/* scvsbattle.c:74 the scene's per-tic update, :86 the start facing,
 * :123 the setup, :4553 the scene entry. */
void scVSBattleFuncUpdate(void);
void scVSBattleSetupFiles(void);
s32 scVSBattleGetStartPlayerLR(s32 this_player);
void scVSBattleStartBattle(void);
void scVSBattleStartScene(void);

/* scvsbattle.c:227 the tie check, which fills gSCManagerVSBattleState
 * and sets gSCManagerSceneData.is_suddendeath, and :412 the second
 * battle's setup. scVSBattleStartScene calls both; the
 * host test calls the first on its own. */
sb32 scVSBattleSetScoreCheckSuddenDeath(void);
void scVSBattleStartSuddenDeath(void);

/* The port's own, with no decomp counterpart: called at the end of
 * scVSBattleStartBattle, once every GObj the battle needs exists and
 * before the first tic runs. src/dc/db.c hangs its debug drawing and its
 * serial log off this -- the scene has the frame loop inside it now, so
 * there is no "after syTaskmanStartTask returns" left to build them in.
 * NULL in a real build. */
extern void (*gSCVSBattleFuncDebug)(void);

#endif /* SSB_DC_SCVSBATTLE_H */
