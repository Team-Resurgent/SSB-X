#ifndef _DC_MN1PCONTINUE_H_
#define _DC_MN1PCONTINUE_H_

/* mn/mn1pmode/mn1pcontinue.c: the "CONTINUE?" screen, overlay 55.
 *
 * The one thing standing between a lost rung and the title. The ladder
 * reaches it from exactly one place -- sc1pmanager.c's is_player_lose
 * arm, which src/dc/sc1pmanager.c CUT until this file existed -- and it
 * answers with one bit, gSCManagerSceneData.is_continue, written by
 * mnPlayers1PGameContinueStartScene as its last act.
 *
 * Shape: a spotlit room drawn as three sprites (Room, Spotlight,
 * Shadow), the player's fighter standing in it, the word CONTINUE, YES
 * and NO with a cursor, the run's score along the bottom, and -- when
 * NO is chosen or forty seconds pass -- the word GAME OVER spelled out
 * of the announcer's letter sprites while the room shrinks away. Seven
 * cameras, one per display link, priority-ordered so the room is laid
 * down first and the text drawn last.
 *
 * It is a menu scene that stands one fighter, so src/dc/sc1pchallenger.c
 * is the template for every port-side decision here: the
 * black clear camera is cut, the pre-render lighting list is cut,
 * lbRelocGetFileData is a sprite bank, and the figatree heap is NULL.
 *
 * KNOWN GAP, not this file's: the spotlight is empty. The fighter is
 * made at y 2070 (mn1pcontinue.c:442, the decomp's own number) and
 * posed with nFTDemoStatusFigureDropped, which on the N64 is the
 * animation that drops it into the light. That status is a SubMotion
 * animation the packs do not carry, so src/dc/scsubsysfighter.c's
 * scSubsysFighterSetStatus falls it back to nFTCommonStatusWait -- the
 * figure stands where it was placed instead of dropping, which is four
 * frame-heights above the spotlight for this camera (eye y 1000, at y
 * 400, fovy 30). The same gap is why src/dc/mvopeningroom.h says its
 * two trophies "stand rather than being plucked" and why
 * src/dc/mvending.c's clear character never lands.
 *
 * It is worth saying what was checked, because "no fighter on screen"
 * has a second, much worse cause -- a model that is never submitted at
 * all. A -DDB_PVR_BUDGET probe of this scene reads "tris op 0 pt 0 tr
 * 338; worst tile op 0 pt 0 tr 0": 338 of Link's triangles go to the TA
 * every frame and land in no tile. The model is built, lit, and drawn;
 * it is simply outside the viewport. When the packs carry SubMotion,
 * this scene gets its figure back with no change to this file.
 */

#include <ssb_types.h>
#include <sc/scdef.h>
#include <sys/objtypes.h>
#include <sys/taskman.h>

/* mn1pcontinue.c:1358. src/dc/db.c reaches in and replaces this
 * scene's func_controller with its own, which is what feeds the hang
 * watchdog's heartbeat; a scene missing from that block reads as hung
 * while running perfectly. src/dc/sc1pgame.h declares its own for the
 * same reason. */
extern SYTaskmanSetup dMN1PContinueTaskmanSetup;

/* sc/scmanager.c:1213-1218's arm, and src/dc/sc1pmanager.c's -- the
 * only two callers. */
void mnPlayers1PGameContinueStartScene(void);

/* dSCManagerOverlays[55]. src/dc/overlay.h says why. */
void mnPlayers1PGameContinueOverlayLoad(void);

#endif
