#ifndef SSB64DC_DCMEMCARD_H
#define SSB64DC_DCMEMCARD_H

/* dcmemcard.h -- the memory card's scene, nSCKindDCMemCard.
 *
 * The one scene the port has that the game does not (src/dc/scport.h).
 * A cartridge's save is always there, so the game never shows where it
 * is; a Dreamcast's can be on any of eight cards, or none, and the player
 * has to be able to see which. This scene is that page: every memory card
 * in the machine, what the game's save on it is, how much room is left
 * on it and which one the game is saving to.
 *
 * It is built the way the game builds Options' two children
 * (src/dc/mnbackupclear.c, src/dc/mnscreenadjust.c): a sprite camera over
 * the menu area, the "OPTION" plate from Backup Clear's header file in
 * the corner, the list below, the cursor in Backup Clear's own tab
 * colours, U/D and the stick through the same wait macros, and B back
 * to Options. What the game cannot supply is the words, so they are the
 * staff roll's font (src/dc/dctext.h), drawn by the scene's own display
 * proc in framebuffer pixels -- every line from src/dc/dcstrings.h.
 *
 * It has no overlay: nothing here is the game's, so there is no
 * syDmaLoadOverlay for scManagerRunScene to call and no xxxOverlayLoad
 * for it to clear (tools/check/overlay_check.py's PORT_ONLY). Every
 * static is set in dcMemCardFuncStart instead. That is also what lets
 * the boot check and the failure prompt run this
 * scene in between two of the game's scenes without zeroing anything
 * the next one's overlay holds.
 *
 * The cards are read once, when the scene starts, after the save's
 * writer has finished (sy_sram_quiesce): a read is a dozen maple frames
 * a card, so it is a load, not something to do every tic. */

#include <ssb_types.h>
#include <sys/taskman.h>

#include "vmucard.h"

/* the task this scene runs */
extern SYTaskmanSetup dDCMemCardTaskmanSetup;

/* What the scene read when it started, in maple order, and the row the
 * cursor is on. */
extern VMUCardInfo sDCMemCardCards[VMUCARD_MAX];
extern s32 sDCMemCardCount;
extern s32 sDCMemCardCursor;
/* the row the store is saving to, or -1 */
extern s32 sDCMemCardInUse;

/* A row's three lines, as the page draws them: "VMU A1"; the
 * save on it ("Saved 2026/09/24 21:41", "No save data", "Damaged save
 * data"); and its room ("186 blocks free", "... - in use"). Each buffer
 * holds DCMEMCARD_LINE bytes. */
#define DCMEMCARD_LINE 48
void dcMemCardRowText(s32 row, char *name, char *save, char *room);

/* The boot check: before the first scene (src/dc/scmanager.c
 * scManagerRunScene), blocking, ahead of the N64 logo. If the save store
 * found a VMU holding a save the game will take, nothing happens. Any
 * other answer (sy_sram_boot_state) is a question for the player, so the
 * store's writes are held, the scene the game was about to start is
 * remembered, and this scene runs in its place as that question:
 *
 *     no VMU        "No VMU is present. No progress will be saved."
 *     no save file  "There is no save data on VMU A1. Create it now?"
 *     damaged file  the same, offering to overwrite it
 *     no room       what is needed against what is free
 *
 * and the answer is final: yes writes the file, with the loop blocked
 * until the VMU has it; no (or B) leaves every VMU alone this session.
 * Either way the scene it interrupted starts as if this had not run.
 *
 * Under -DDB_BOOT_SCENE (a probe) it does nothing, and the game's default
 * write lands as before, unless -DDB_MEMCARD_PROBE asks for the check
 * there too; -DDB_MEMCARD_AUTO=1 or =2 (src/dc/db.h) answers it. */
void dcMemCardBootBegin(void);

void dcMemCardFuncStart(void);
void dcMemCardStartScene(void);

#endif /* SSB64DC_DCMEMCARD_H */
