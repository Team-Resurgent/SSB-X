/* mvopeningnewcomers.h -- the openings' nineteenth and last scene,
 * mv/mvopening/mvopeningnewcomers.c ported. Overlay 52.
 *
 * ... Standoff(47) -> Clash(49) -> Newcomers(52) -> title. It ends the
 * movie: at tic 40 (a black fade from tic 30) it hands off to nSCKindTitle,
 * the same place a button press at any tic from 10 goes.
 *
 * Four silhouettes -- Jigglypuff, Captain Falcon, Luigi and Ness -- stand
 * before a white screen, each a lit Show model or a black Hidden one by
 * whether the player has unlocked that fighter yet
 * (gSCManagerBackupData.fighter_mask), animated by one AnimJoint each,
 * under a camera whose eye, target and field of view are constants.
 *
 * ---- What is substituted
 *
 *   - Files. relocData 61 and 62 (MVOpeningNewcomers1/2: the Show and
 *     Hidden display lists and their AnimJoints) are not loaded. Each of
 *     the eight lists is a pack, romdisk/mvopeningnc<name>{,hide}
 *     (short names: the disc filesystem cuts one at 27 characters)
 *     .mdl (tools/export/ssb_effectexport.py `--what newcomerspurin`,
 *     `newcomerspurinhide` ... -- a display list on one DObj, and its
 *     AnimJoint symbol is the script itself, not a table of them, which
 *     that tool learned as EFENTRY_DIRECTANIM); only the four the mask
 *     picks are loaded.
 *   - The white screen. The default camera's COBJ_FLAG_FILLCOLOR is
 *     dropped (fillcolor-camera note) which leaves the clear colour, and
 *     this is the one opening scene whose picture is on white: so
 *     pvr_set_bg_color puts the PVR's background plane at white for the
 *     scene (the border quads still black what the viewport does not
 *     cover, as on the N64), and both exits put it back to black before
 *     they leave -- an overlay's own OverlayLoad runs when it is entered,
 *     not when it is left.
 *   - The pools are the models' own, not the decomp's zeroes (its task
 *     set-up gives every count as 0, which the game evidently
 *     tolerates; the port's pool set-up does not).
 *   - mvopeningmario.h's Substitutions 3b and 5; func_lights is NULL;
 *     the two sounds played after the busy-wait play at its place, the
 *     end of FuncStart; pack builds are #ifdef FT_HOSTTEST-guarded.
 */
#ifndef SSB_DC_MVOPENINGNEWCOMERS_H
#define SSB_DC_MVOPENINGNEWCOMERS_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

extern SYTaskmanSetup dMVOpeningNewcomersTaskmanSetup;

sb32 mvOpeningNewcomersCheckLocked(s32 fkind);
void mvOpeningNewcomersMakeAll(void);
void mvOpeningNewcomersHideProcDisplay(GObj *gobj);
void mvOpeningNewcomersMakeHide(void);
void mvOpeningNewcomersMakeNewcomersCamera(void);
void mvOpeningNewcomersMakeHideCamera(void);
void mvOpeningNewcomersInitVars(void);
void mvOpeningNewcomersFuncRun(GObj *gobj);
void mvOpeningNewcomersFuncStart(void);
void mvOpeningNewcomersStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[52]. */
void mvOpeningNewcomersOverlayLoad(void);

#endif /* SSB_DC_MVOPENINGNEWCOMERS_H */
