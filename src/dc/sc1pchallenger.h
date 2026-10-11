/* sc1pchallenger.h -- "CHALLENGER APPROACHING!", the card the ladder
 * shows when an unlockable fighter comes for you.
 * sc/sc1pmode/sc1pchallenger.c. Overlay 23.
 *
 * The smallest scene in sc1pmode/ and the only one that is nothing but
 * a picture: a silhouetted fighter turning 2 degrees a tic on the right
 * of the screen, WARNING in red over CHALLENGER / APPROACHING in
 * orange on the left, an exclamation decal above them and a dark blue
 * bar behind the fighter. It plays nSYAudioBGM1PChallenger and the star
 * -KO stinger once, waits 120 tics, and then A, B or START takes the
 * game to the title. sc/sc1pmode/sc1pmanager.c is what decides a
 * challenger has been earned and sets gSCManagerSceneData.challenger_fkind
 * before coming here; a direct boot goes through src/dc/db.c's own boot arm (below).
 *
 * The silhouette is not this file's doing and needs nothing from the
 * port: ftParamCheckSetFighterColAnimID(fighter_gobj,
 * nGMColAnimFighterChallenger, 0) names dGMColScriptsFighterChallenger
 * (gm/gmcolscripts.c:1021, in the link already and compiled unmodified),
 * a 60-tic colour script that holds the fighter black.
 *
 * All four challengers run: Luigi, Ness, Jigglypuff and Captain Falcon
 * all have exported packs, so unlike the rungs of sc1pgame.c there is
 * no data hole here.
 *
 * Every function is the decomp's by name and body; the line numbers are
 * the decomp's. What is not here, and where it is said:
 *   - sc1PChallengerFuncLights and the Lights1 display list it pushes
 *     -- dSC1PChallengerTaskmanSetup, which also says what lights the
 *     fighter instead;
 *   - the reloc setup and its two status buffers --
 *     sc1PChallengerLoadFiles, which replaces the loader they belong to;
 *   - syVideoInit, the z-buffer and the arena line --
 *     sc1PChallengerStartScene;
 *   - gcMakeDefaultCameraGObj, the black COBJ_FLAG_FILLCOLOR camera --
 *     sc1PChallengerFuncStart;
 *   - the figatree heap -- sc1PChallengerFuncStart.
 * One function is rewritten rather than cut:
 * sc1PChallengerDecalsProcDisplay's eight raw gDP commands become the
 * one lbCommonSpriteFillRect they add up to, the same substitution
 * src/dc/mn1pmode.c:406 and src/dc/mndata.c:404 already make.
 */
#ifndef SSB_DC_SC1PCHALLENGER_H
#define SSB_DC_SC1PCHALLENGER_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

extern SYTaskmanSetup dSC1PChallengerTaskmanSetup;

void sc1PChallengerStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[23]. */
void sc1PChallengerOverlayLoad(void);

#endif /* SSB_DC_SC1PCHALLENGER_H */
