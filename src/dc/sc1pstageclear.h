/* sc1pstageclear.h -- the 1P ladder's score screen,
 * sc/sc1pmode/sc1pstageclear.c. Overlay 56.
 *
 * What runs after every rung: the frozen last frame of the match dimmed
 * to half behind STAGE CLEAR (or GAME CLEAR, or RESULT for a bonus
 * stage), then the score built up a line at a time -- the seconds left
 * times a multiplier, the damage dealt times ten, and then the
 * - BONUS - table, ten rows to a page, of the fifty-seven special
 * bonuses sc1PGameAppendBonusStats spent 650 lines counting. Each row
 * appears on its own tic, adds its points to the running total, and A,
 * B or START skips ahead; when the last page is done the same buttons
 * leave. gSCManagerSceneData.spgame_score carries the total out.
 *
 * Two scene kinds start it, nSCKind1PScoreUnk and nSCKind1PStageClear,
 * by the same call with the same overlays (scmanager.c:1220-1233); what
 * it shows is decided from gSCManagerSceneData.spgame_stage, not from
 * which kind arrived.
 *
 * THE ONE REAL PIECE OF PORTING IS THE WALLPAPER, and it is worth
 * reading sc1PStageClearCopyFramebufToWallpaper before anything else
 * here. The game's backdrop is not an image in a file: it is the
 * framebuffer the match left behind, read out of
 * gSYSchedulerCurrentFramebuffer and written over the pixels of file 26
 * (GRWallpaperTrainingBlack) so the sprite code can draw it. The port
 * does the same thing from the framebuffer KOS is displaying, into a
 * texture of its own -- so file 26 is not loaded at all, and the Sprite
 * the wallpaper GObj is given is one this file builds by hand over that
 * texture, the way src/dc/stage.c stage_wallpaper_sprite builds one
 * over a stage's. src/dc/lbtransition.c's wipe photocopy is the same
 * grab and is where the vram_s reasoning is written out.
 *
 * WHAT IS NOT REACHABLE YET, and none of it is this file's doing. The
 * three bonus-stage arms (the target and platform counts, the Race to
 * the Finish timer) are complete here and wait on
 * sc/sc1pmode/sc1pbonusstage.c; the scene that would send the player
 * here after a rung is sc/sc1pmode/sc1pmanager.c, Booting it directly goes through src/dc/db.c's own boot arm, which deals
 * the whole of gSCManagerSceneData's 1P block by hand.
 *
 * Every function is the decomp's by name and body, the REGION_US arms
 * where it has them; the line numbers are the decomp's. What is not
 * here, and where it is said:
 *   - sc1PStageClearFuncLights and the two Lights1 it sets -- their own
 *     notes, and dGM1PStageClearTaskmanSetup;
 *   - the reloc setup and its two status buffers --
 *     sc1PStageClearLoadFiles;
 *   - gcMakeDefaultCameraGObj -- sc1PStageClearFuncStart, which also
 *     says why this one is the mildest copy of that cut in the port;
 *   - syVideoInit, the z-buffer and the arena line --
 *     sc1PStageClearStartScene;
 *   - file 26 -- dSC1PStageClearFileIDs.
 */
#ifndef SSB_DC_SC1PSTAGECLEAR_H
#define SSB_DC_SC1PSTAGECLEAR_H

#include <sc/scene.h>
#include <sys/obj.h>
#include <sys/taskman.h>

extern SYTaskmanSetup dGM1PStageClearTaskmanSetup;

void sc1PStageClearStartScene(void);

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[56]. */
void sc1PStageClearOverlayLoad(void);

#endif /* SSB_DC_SC1PSTAGECLEAR_H */
