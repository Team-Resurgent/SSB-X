/* scmanager.h -- the port's side of sc/scmanager.c, the scene manager.
 *
 * The decomp's file is 1400 lines, and almost all of it is the overlay
 * table: sixty-five SYOverlay entries and a switch that DMAs the right
 * ones into RAM before calling a scene's StartScene. There are no
 * overlays here -- every scene that exists is linked into the ELF -- so
 * what is left of scManagerRunScene is its shape: a while (TRUE) that
 * reads gSCManagerSceneData.scene_curr, starts that scene, and comes
 * back when the scene is over to read it again. A scene sets scene_curr
 * on its way out, and that is the whole of the game's navigation.
 *
 * Six scenes are ported -- nSCKindTitle (src/dc/mntitle.c),
 * nSCKindModeSelect (src/dc/mnmodeselect.c), nSCKindVSMode
 * (src/dc/mnvsmode.c), nSCKindPlayersVS (src/dc/mnplayersvs.c),
 * nSCKindVSBattle (src/dc/scvsbattle.c) and nSCKindVSResults
 * (src/dc/scvsresults.c) -- and the loop actually loops: title, mode
 * select, VS mode, character select, START straight into the battle,
 * results, title. The rest of the switch is the DIVERGES recorded on
 * scManagerRunScene below.
 *
 * The four globals here are the scene manager's own state: the battle
 * the menus would have filled in, and the scene the manager is on.
 */
#ifndef SSB_DC_SCMANAGER_H
#define SSB_DC_SCMANAGER_H

#include <sc/scene.h>
#include <sys/taskman.h>

/* sc/scmanager.c:31 gSCManagerSceneData, the scene manager's own record:
 * which scene is current and which was previous, plus the settings the
 * menus leave behind for the scene about to run. The battle scene reads
 * gkind (the stage) and writes is_reset/is_suddendeath; :36
 * gSCManagerTransferBattleState is the battle the VS menus fill in and
 * hand to the battle scene, and :39 gSCManagerVSBattleState the one
 * sudden death is played from. gSCManagerBattleState points at whichever
 * is being played.
 *
 * All but gSCManagerVSBattleState are declared by the sc/scene.h shim,
 * because lb/lbbackup.c -- compiled unmodified -- reads them; that shim
 * also carries the three d-prefixed defaults this file defines. */
extern SCBattleState gSCManagerVSBattleState;
/* and :42 gSCManager1PGameBattleState, the 1P ladder's own, read by
 * src/dc/mnplayers1pgame.c since */
extern SCBattleState gSCManager1PGameBattleState;

/* sc/scmanager.c:830-853, the init half of scManagerRunLoop: the four
 * globals above from dSCManagerDefault*, then lbBackupIsSramValid and
 * lbBackupApplyOptions -- the save data off the store, or the defaults
 * written into it when there is nothing there to trust. Call it once,
 * before the first scene. The .c has what the rest of scManagerRunLoop
 * was. */
void scManagerInitData(void);

/* sc/scmanager.c:1286-1294, verbatim. func_update starts a scene's task
 * and does not return until the scene ends; func_draw is what every
 * scene's SYTaskmanSetup names as its frame draw. */
void scManagerFuncUpdate(SYTaskmanSetup *arg);
void scManagerFuncDraw(void);

/* sc/scmanager.c:867 the scene loop. Does not return: a scene the port
 * does not have is named on the log and the loop goes back to the scene
 * that asked for it. */
void scManagerRunScene(void);
/* a scene kind's name, as the serial log spells it (-DDB_LOAD_CENSUS) */
const char *scManagerSceneName(s32 kind);

#endif /* SSB_DC_SCMANAGER_H */
