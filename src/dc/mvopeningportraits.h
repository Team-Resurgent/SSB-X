/* mvopeningportraits.h -- the openings' second scene,
 * mv/mvopening/mvopeningportraits.c ported. Overlay 35.
 *
 * mv/mvopening/mvopeningroom.h names the whole nineteen-scene chain and
 * how it is skippable; this is the tier-1 scene that header called
 * "portable today" -- the only one of the eighteen still-unported
 * scenes whose relocData reads are pure `Sprite*`, nothing
 * `DObjDesc`/`MObjSub`-shaped. Two sets of four fighter portraits
 * (Samus/Mario/Fox/Pikachu, then Link/Kirby/Donkey/Yoshi) slide onto a
 * 4-row grid one row at a time behind a wipe-cover sprite, hold, then
 * hand off to nSCKindOpeningMario -- the first per-fighter scene, still
 * unported, so the port's scene manager loops back here exactly the way
 * it already loops room back to itself.
 *
 * Every function is the decomp's by name and body; the line numbers are
 * the decomp's. What is not here, and where it is said:
 *   - the black clear camera (gcMakeDefaultCameraGObj with
 *     COBJ_FLAG_FILLCOLOR) -- mvOpeningPortraitsFuncStart. Same reason
 *     src/dc/mnstartup.h already gives, verified for this file
 *     specifically: this camera is made first, at priority 100, and
 *     mvOpeningRoomFuncStart's identical-looking call is safe only
 *     because mvOpeningRoomFuncRun later clears COBJ_FLAG_FILLCOLOR
 *     (tic 1037) before anything is drawn under it (the wallpaper sprite
 *     at tic 1040). This file never clears the flag anywhere in its own
 *     FuncRun, so left in place it would sit above both of this scene's
 *     real cameras (Portraits at priority 80, Cover at priority 60) for
 *     the scene's whole 150-tic run and paint over every portrait, the
 *     same way it painted over the N64 logo. Dropped, with both this
 *     scene's own reasoning and the cross-reference to room's written at
 *     the call site;
 *   - mvOpeningPortraitsFuncLights, and the two Lights1 tables
 *     (dMVOpeningPortraitsLights11/12) they would feed -- the standing
 *     rule (a Lights1/FuncLights pair never survives test-host), and
 *     the pair is unreferenced by anything else in this file besides
 *     each other;
 *   - syVideoInit, the zbuffer setup and the arena_size line --
 *     mvOpeningPortraitsStartScene;
 *   - lbRelocInitSetup/lbRelocLoadFilesListed and the two status
 *     buffers (sMVOpeningPortraitsStatusBuffer/ForceStatusBuffer):
 *     replaced by mvOpeningPortraitsLoadFiles, one sprite_bank_load per
 *     entry of dMVOpeningPortraitsFileIDs into sMVOpeningPortraitsBanks,
 *     the same substitution src/dc/mncharacters.c's own
 *     mnCharactersLoadFiles already makes for a multi-file scene;
 *   - sMVOpeningPortraitsPad0x801329E0[2]: N64 struct-layout padding
 *     between two statics, read and written by nothing in the whole
 *     file. Dropped outright, unlike sMVOpeningPortraitsUnused0x801329F4
 *     just below it in the decomp, which IS kept -- FuncRun really does
 *     decrement and reset that one, dead effect or not.
 *
 * ---- The trailing tic-sync busy-wait: replaced by
 * sySchedulerWaitTicCount (the text below explains why the loop cannot
 * be kept verbatim).
 *
 * mvOpeningPortraitsFuncStart ends with
 * `while (sySchedulerGetTicCount() < 1335) continue;`. Every one of the
 * eighteen decomp files after room ends its own FuncStart the same way,
 * each against its own constant (mario: 1515, donkey: 1605, ...) --
 * room is the only one without it, because room is the chain's first
 * scene and resets the counter to 0 at its own end rather than reading
 * it. On the N64 the counter advances on its own during the spin (a
 * hardware VI-retrace interrupt increments it, sys/scheduler.c:1042,
 * regardless of what the CPU is doing), so the loop is a real wait for
 * real elapsed frames -- it pads FuncStart's own asset-load time so the
 * chain's total elapsed tic count lands on a fixed value at the same
 * point in every playthrough, independent of how long loading took.
 *
 * This port's tic counter (src/dc/taskman.c's sSYSchedulerTicCount) is
 * software, incremented once inside syTaskmanRunFrame -- and
 * syTaskmanLoadScene calls func_start() (this function) BEFORE the
 * frame loop that contains that increment ever runs
 * (src/dc/taskman.c:647-651). Nothing advances the counter while this
 * function is on the stack, so kept verbatim the loop would spin
 * forever: a real hang, not a slow scene, on the very first opening
 * scene after room. Every one of the
 * eighteen files ends with sySchedulerWaitTicCount(<its constant>),
 * which waits on the retrace count and so keeps the chain's timing.
 *
 * One thing that is NOT a divergence: sMVOpeningPortraitsGObj is
 * reused as BOTH sets' driver GObj (MakeSet1 and MakeSet2 each assign
 * it fresh), and FuncRun ejects the set-1 GObj by that same name right
 * before calling MakeSet2 at tic 75 -- verbatim, not a leak; the second
 * assignment simply overwrites the pointer to the second GObj MakeSet2
 * just made.
 */
#ifndef SSB_DC_MVOPENINGPORTRAITS_H
#define SSB_DC_MVOPENINGPORTRAITS_H

#include <sys/obj.h>
#include <sys/taskman.h>

/* mvopeningportraits.c:17 */
extern u32 dMVOpeningPortraitsFileIDs[2];

/* mvopeningportraits.c:57-58 sMVOpeningPortraitsFiles, the port's own,
 * named the way every other multi-file ported scene's is: two file
 * bases, loaded by mvOpeningPortraitsLoadFiles. Not static -- the host
 * test's scene_sprite_find reads it the same way it already reads
 * src/dc/mnmodeselect.c's sMNModeSelectFiles. */
extern void *sMVOpeningPortraitsFiles[2];

/* mvopeningportraits.c:37 */
extern SYTaskmanSetup dMVOpeningPortraitsTaskmanSetup;

/* mvopeningportraits.c:71-378, in the decomp's order. */
void mvOpeningPortraitsMakeSet1(void);
void mvOpeningPortraitsMakeSet2(void);
void mvOpeningPortraitsBlockRow0(void);
void mvOpeningPortraitsBlockRow1(void);
void mvOpeningPortraitsBlockRow2(void);
void mvOpeningPortraitsBlockRow3(void);
void mvOpeningPortraitsBlockPartialRow(s32 row, s32 pos_x);
void mvOpeningPortraitsCoverProcDisplay(GObj *gobj);
void mvOpeningPortraitsCoverProcUpdate(GObj *gobj);
void mvOpeningPortraitsMakeCover(void);
void mvOpeningPortraitsMakePortraitsCamera(void);
void mvOpeningPortraitsMakeCoverCamera(void);
void mvOpeningPortraitsInitVars(void);
void mvOpeningPortraitsFuncRun(GObj *gobj);
void mvOpeningPortraitsFuncStart(void);

void mvOpeningPortraitsStartScene(void);
void mvOpeningPortraitsOverlayLoad(void);       /* dSCManagerOverlays[35] */

#endif
