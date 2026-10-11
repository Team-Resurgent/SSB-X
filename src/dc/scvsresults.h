/* scvsresults.h -- the VS results screen, mn/mnvsmode/mnvsresults.c.
 *
 * The second scene the port had, and it exists mostly to prove there
 * can be one. Its file in the decomp is 3458 lines: about a tenth of
 * them are the rankings -- who won, in what order the others placed,
 * how many KOs and falls each has, and when the screen may be left --
 * and the rest are the screen those numbers are drawn on.
 *
 * The rankings came first. They read exactly the fields
 * the battle scene wrote -- `place`, `score`, `falls` on
 * gSCManagerTransferBattleState -- and are the only consumer of them in
 * the game, so porting them is what makes the end of a match mean
 * something rather than just happen.
 *
 * The screen has the wallpaper in the winner's colour
 * and the three black tints that fade off it, the ranked rows of KOs,
 * falls, points and places with their growing white rule, the mode
 * label, the column heads with each player's stock icon, the player
 * tags, and the winner's name spelled out of the announcer's alphabet
 * with WINS! after it -- over the announcer's own schedule and the
 * winner's series theme. Six sprite cameras draw it, on DL links 26
 * (wallpaper), 27 (tags), 29 (the name), 30 (the tint), 31 (the ranked
 * rows and the label) and 34 and 35 (the two fade-in sheets), sorted by
 * the priorities the game gives them.
 *
 * What is still not here, each named at the function it belongs to:
 *
 *  - the podium fighters' win and lose poses. The fighters themselves
 *    stand on their spots, facing the winner and fading in, but the demo statuses they are set to play out of
 *    the SubMotion file the pack does not carry, so each stands in Wait
 *    (src/dc/scsubsysfighter.c scSubsysFighterSetStatus).
 *  - the winner's series emblem, which is a model with a material
 *    animation out of relocData file 35 -- the port has no exporter for
 *    one, as with the character select's spotlight.
 *  - the transition wipe
 *    (lb/lbtransition.c, which photocopies the last framebuffer into a
 *    texture and folds it up; it wants a framebuffer read the PVR does
 *    differently, and eleven more model files).
 *  - mnVSResultsUpdateAutoHandicap, which sets the next match's
 *    handicaps from this one's result. The handicap screen's rather
 *    than the save data's; mnVSResultsSaveBackup, which writes the
 *    match into the records,.
 *
 * Where it goes next was the last behavioural cut and is gone: the game
 * goes to the VS character select (nSCKindPlayersVS), or to the unlock
 * message when a record was broken, and both scenes
 * exist and the port does the same. A stand-in route (a straight
 * rematch, or the title) is what makes the scene manager's loop
 * observable: a match, its results and the screen that starts the next
 * one, out of one scene heap.
 */
#ifndef SSB_DC_SCVSRESULTS_H
#define SSB_DC_SCVSRESULTS_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

/* mnvsresults.c:3397 dMNVSResultsTaskmanSetup, :3443 the scene entry. */
extern SYTaskmanSetup dMNVSResultsTaskmanSetup;
void mnVSResultsStartScene(void);

/* mnvsresults.c:3227 the scene's one GObj, :3321 its start function. */
void mnVSResultsFuncRun(GObj *gobj);
void mnVSResultsFuncStart(void);

/* The rankings, read by the screen below and by the host test. Places
 * are the screen's own ordering (0 first), not the battle state's. */
s32 mnVSResultsGetWinPlayer(void);
s32 mnVSResultsGetPlace(s32 player);
s32 mnVSResultsGetKOs(s32 player);
s32 mnVSResultsGetTKO(s32 player);
s32 mnVSResultsGetPoints(s32 player);
s32 mnVSResultsGetFighterKind(s32 player);

/* The screen. Only the pieces something outside this file reaches are
 * here -- the host test drives the two fill-quad display procs directly
 * and reads the layout off the scene's GObjs -- and the rest is
 * declared by the decomp's own mn/mnvsmode/mnvsresults.h, which the
 * port does not include: a good many of the functions it declares
 * extern are static in scvsresults.c. */
void mnVSResultsLoadFiles(void);
extern void *sMNVSResultsFiles[8];

s32 mnVSResultsGetPresentCount(void);
s32 mnVSResultsGetPresentLowerCount(s32 player);
s32 mnVSResultsGetSpot(s32 player);
s32 mnVSResultsGetPlayerDistanceID(s32 player);
s32 mnVSResultsGetDisplayPlace(s32 player);
f32 mnVSResultsGetColumnX(s32 player);
s32 mnVSResultsGetNumberColorID(s32 player);
s32 mnVSResultsGetCharacterID(char c);

void mnVSResultsMakePointsRow(void);

void mnVSResultsBarProcDisplay(GObj *gobj);
void mnVSResultsLabelProcDisplay(GObj *gobj);
void mnVSResultsTintProcDisplay(GObj *gobj);
void mnVSResultsWallpaperTintProcDisplay(GObj *gobj);
void mnVSResultsWallpaperTint2ProcDisplay(GObj *gobj);
void mnVSResultsWallpaperProcDisplay(GObj *gobj);

/* sc/scvsresults.c:2674 and :2941. Both are void and both are called
 * from the host test as well as from this file -- undeclared there, they
 * got an implicit `int` return, which is the trap this port's host test
 * has met before: a wrong assumed return type is invisible until the
 * day the value matters. */
void mnVSResultsMakeFighterCamera(void);
void mnVSResultsMakeConfetti(void);

#endif /* SSB_DC_SCVSRESULTS_H */
