#ifndef _DC_SC1PMANAGER_H_
#define _DC_SC1PMANAGER_H_

/* sc/sc1pmode/sc1pmanager.c: the 1P ladder's ROUTER.
 *
 * Not a scene. scmanager.c:1023-1025's nSCKind1PGame arm starts nothing
 * and loads no overlay -- it calls sc1PManagerUpdateScene, and that
 * function then runs a whole run of the ladder inline: every rung's
 * card, the rung, the score screen, the challenger fight, the ending,
 * by calling each scene's own StartScene and letting it block. That
 * works here for the reason src/dc/taskman.c's syTaskmanLoadScene note
 * gives: the port's syTaskmanRunTask does not return until its scene is
 * over, exactly as the game's does, so this file's sequential shape is
 * its own and needs no rewriting.
 *
 * It also owns the run's carried state -- the totals the ladder is
 * scored on and the two Kirby Team decisions -- which src/dc/scmanager.c
 * held until this file existed, because src/dc/sc1pintro.c and
 * src/dc/sc1pgame.c read them and neither may define them: both are in
 * an overlay a rung reloads, so their statics are zero on every entry,
 * and the whole point of these is to carry a decision from one scene
 * into the next.
 *
 * This file is in an overlay too -- smashbrothers.us.yaml:402 puts
 * sc/sc1pmode/sc1pmanager at the FRONT of ovl2, the battle's bulk --
 * and that is exactly why the state is safe here rather than a happy
 * accident. The scene before the ladder loads overlay 2 (the 1P
 * character select's arm does, src/dc/scmanager.c:433-439), so
 * these start a run at zero; and then the router never loads overlay 2
 * again. Look at what it does load below: the card gets 1 and 24, the
 * rung 3 and 65, the score screen 1 and 56 -- never 2, which is still
 * in memory from before the run and holds the code the router is itself
 * executing out of. So the totals survive the run and are zeroed by the
 * next entry into 1P mode, which is what sc1PManagerOverlayLoad is for.
 */

#include <ssb_types.h>
#include <sc/scdef.h>

/* sc1pmanager.c:74-89. The running totals sc1PGameFuncUpdate adds each
 * rung's time, falls and damage into; the handicap the router drops the
 * CPU level by after two lost lives; and which ability the last Kirby of
 * the Kirby Team copies, with the hat that goes with it.
 *
 * <sc/sc1pmode/sc1pmanager.h> declares these too, and the seven
 * functions below with them -- the port's files reach them through
 * <sc/scene.h>. They are repeated here so a reader of this header sees
 * what the file holds, the way src/dc/sc1pintro.h repeats its scene's. */
extern u8 sSC1PManagerScenePrev;
extern u32 gSC1PManagerTotalTimeTics;
extern s32 gSC1PManagerTotalFalls;
extern s32 gSC1PManagerTotalDamage;
extern s32 gSC1PManagerLevelDrop;
extern u8 sSC1PManagerLevelGuard;
extern u8 gSC1PManagerKirbyTeamFinalCopy;
extern u8 gSC1PManagerKirbyTeamModelPartID;

extern s32 sc1PManagerGetFighterKindsNum(u16 mask);
extern s32 sc1PManagerGetShuffledFighterKind(u16 this_mask, u16 prev_mask,
                                             s32 random);
extern s32 sc1PManagerGetShuffledKirbyCopy(u16 flags, s32 random);
extern void sc1PManagerTrySetChallengers(void);
extern sb32 sc1PManagerCheckUnlockSoundTest(void);
extern void sc1PManagerTrySaveBackup(sb32 is_complete_spgame);

/* The router itself. Returns once the run is over, with
 * gSCManagerSceneData.scene_curr set to wherever the ladder left the
 * player -- the title, the 1P menu, or the first bonus select. */
extern void sc1PManagerUpdateScene(void);

/* dSCManagerOverlays[2]. src/dc/overlay.h has the argument. */
void sc1PManagerOverlayLoad(void);

#endif
