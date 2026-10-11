/* ifcommon.h -- if/ifcommon.c, the battle HUD, ported piece by piece.
 * This is a PORT: functions keep the decomp's names and signatures;
 * what each leaves out (the stock-steal effects; the announcer's voices
 * ft/ftpublic.c queues) is marked at its head.
 *
 * What is here, in three pieces:
 *
 *  - the entry sequence -- three GObj threads that wake the fighters'
 *    warp-ins and the countdown's GO! (scvsbattle.c:216
 *    ifCommonEntryAllMakeInterface), the first code in the port to run
 *    on the object system's threads (src/dc/sysshim.c); the countdown draws its traffic light and GO! its banner;
 *
 *  - the HUD proper: the damage digits with the emblem,
 *    the stock rows, the player tags, drawn by the interface camera
 *    (src/dc/gmcamera.c) through lb/lbcommon.c's sprite pass, verbatim;
 *
 *  - the match clock: the four digits and the colon, the
 *    runner that counts them down off the scheduler's retrace count, the
 *    announcer's last five seconds and the music fading under them, and
 *    TIME UP, which ends a TIME match the way GAME SET ends a stock one;
 *
 *  - SUDDEN DEATH: the banner and the thread that stands
 *    in for the entry sequence in the second battle a tied TIME match
 *    starts (src/dc/scvsbattle.c);

 *  - the off-screen arrows: the two red chevron arrows at
 *    the edges of the screen, their ortho camera and the model and
 *    flag-animation out of relocData file 166, and the run function that
 *    decides each tic which side has a fighter off it -- off what the
 *    fighters' own display procs left behind (src/dc/ftdisplaymain.c);
 *
 *  - the magnifying glasses: the round glass at the edge
 *    of the screen an off-screen fighter is drawn again inside, with
 *    its frame, its handle and the camera and viewport of its own it is
 *    drawn through -- the RDP's Z-buffer mask replaced by sixteen clip
 *    planes (src/dc/clip.c);
 *
 *  - the pause menu: the START scan every running tic goes
 *    through, the white frame and its decals, the close-up camera the
 *    stick pans, and the A+B+R+Z reset that sets
 *    gSCManagerSceneData.is_reset -- the flag the results screen has been
 *    waiting on to show NO CONTEST (src/dc/scvsresults.c). The freeze
 *    itself is not a flag: the Pause arm of the dispatcher simply does
 *    not run the object system;
 *
 *  - the end-state timeline: the battle's whole per-frame
 *    update, ifCommonBattleUpdateInterfaceAll, which is the one line of
 *    scVSBattleFuncUpdate and the scene's SYTaskmanSetup.func_update;
 *    the placement counter that ifCommonBattleUpdateScoreStocks walks
 *    down on every KO that empties a team; and GAME SET, which freezes
 *    the world, holds the banner 90 tics, hides the HUD and calls
 *    syTaskmanSetLoadScene. The DIVERGES for all of it is written once,
 *    at the head of that section in ifcommon.c, rather than repeated at
 *    each function.
 *
 * A note on what this file's declarations are for: nearly every function
 * here is declared by the decomp's own if/ifcommon.h, which the port
 * includes, and none of those are repeated below. What is below is the
 * handful the host test and src/dc/db.c reach for by name. */
#ifndef SSB_DC_IFCOMMON_H
#define SSB_DC_IFCOMMON_H

#include <sys/obj.h>

/* ifcommon.c:3214 */
void ifCommonBattleSetGameStatusWait(void);

/* ifcommon.c:2004-2020: unlock every fighter and set the game status --
 * the GO! */
void ifCommonAnnounceGoSetStatus(void);

/* ifcommon.c:2299-2305: the entry focus thread, id 0-2 */
void ifCommonEntryFocusMakeInterface(s32 id);

/* ifcommon.c:2668: the countdown thread (its sprites are not ported) */
SObj *ifCommonCountdownMakeInterface(void);

/* ifcommon.c:2318-2323: the whole entry sequence, and the game status
 * to Wait */
void ifCommonEntryAllMakeInterface(void);

/* ifcommon.c:2534: the match clock's runner, which carries the function
 * to call when it reaches zero */
void ifCommonTimerMakeInterface(void (*proc)(void));

/* ifcommon.c:2440: the clock's four digits and its colon, or NULL on a
 * match with no finite time limit */
SObj *ifCommonTimerMakeDigits(void);

/* ifcommon.c:3342: the proc scVSBattleStartBattle hands the clock -- end
 * the match with TIME UP over it */
void ifCommonAnnounceTimeUpInitInterface(void);

/* ifcommon.c:3250,3335: the COMPLETE! banner and the freeze it runs
 * under -- what a cleared bonus stage calls where a finished match
 * calls the two above. The voice line is the caller's:
 * the three bonus stages say different things. */
GObj *ifCommonAnnounceCompleteMakeInterface(void);
void ifCommonAnnounceCompleteInitInterface(u16 sfx_id);
GObj *ifCommonAnnounceFailureMakeInterface(void);
void ifCommonAnnounceFailureInitInterface(void);

/* ifcommon.c:3299: the proc_set a 1P match freezes into -- the camera
 * pulls out over 45 tics instead of snapping back in 3.
 * ifCommonAnnounceEndMessage picks it. */
void ifCommon1PGameInterfaceProcSet(void);

/* ifcommon.c:2356-2367: the SUDDEN DEATH banner, the thread on it that
 * replaces the entry sequence, and the game status to Wait -- what
 * scVSBattleStartSuddenDeath calls where a first battle calls
 * ifCommonEntryAllMakeInterface */
void ifCommonSuddenDeathMakeInterface(void);

/* ifcommon.c:2882-2897: pause the match on behalf of `player` -- what the
 * START scan calls, and what the host test calls
 * in its place */
void ifCommonBattlePauseInitInterface(s32 player);

#endif /* SSB_DC_IFCOMMON_H */
