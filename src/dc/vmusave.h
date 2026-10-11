#ifndef SSB64DC_VMUSAVE_H
#define SSB64DC_VMUSAVE_H

#include <stdint.h>

/* vmusave.h -- the game's save store, and the VMU behind it.
 *
 * lb/lbbackup.c is compiled unmodified, and everything it does -- the
 * checksum, the signature, the two copies at two offsets, the fall back
 * to the second when the first does not verify, the write-back -- runs
 * over sys/dma.c's syDmaReadSram / syDmaWriteSram. Those two bodies are
 * the whole seam, and this file is them: a byte store the game reads and
 * writes exactly as it reads and writes the cartridge's SRAM, backed by
 * a file on a VMU (src/dc/vmucard.h).
 *
 * Nothing above the two bodies changes, which is the point. The port
 * does not decide what a save is, when one is valid, or what to do with
 * a bad one; the game decides all three, out of the decompilation, and
 * this file only decides where the bytes sleep.
 *
 * The rest below is the port's own, and is the price of the VMU being
 * slow where SRAM was not -- see src/dc/vmusave.c.
 */

/* Read the card into the store. Runs at boot, before the game's first
 * syDmaReadSram (sc/scmanager.c scManagerInitData -> lbBackupIsSramValid),
 * so that what the game reads back is what the last session wrote. A
 * store with no card behind it is a cartridge that has never been
 * written, which lb/lbbackup.c already knows what to do with. */
void sy_sram_init(void);

/* What sy_sram_init found, for the boot check (src/dc/dcmemcard.h). The
 * game's own save is only ever loaded from a VMU that holds one; every
 * other answer is a question for the player, and until it is answered the
 * store's changes are held in RAM (sy_sram_hold). */
enum
{
    nSYSramBootSaved,   /* a save the game will take: loaded, nothing to ask */
    nSYSramBootNoCard,  /* no VMU at all: nothing can be saved */
    nSYSramBootNoSave,  /* a VMU with room and no save file on it */
    nSYSramBootNoSpace, /* a VMU with no save file and no room for one */
    nSYSramBootDamaged  /* a save file the game will not take */
};

/* What the last sy_sram_init found. */
int sy_sram_boot_state(void);

/* The VMU's free blocks as the boot check saw them (-1 unknown), and what
 * the save file needs of them. */
int sy_sram_boot_free_blocks(void);
int sy_sram_boot_need_blocks(void);

/* The player said yes: release the check's hold and write the store to the VMU
 * now, blocking until it is on it (or has failed: sy_sram_failed). The
 * file is made before the first scene, not under a notice in one. */
void sy_sram_boot_create(void);

/* The player said no, or the VMU cannot take a file: nothing is written
 * to any VMU this session, and the hold is released. */
void sy_sram_boot_decline(void);

/* The frame's end (src/dc/taskman.h's frame-end hook, installed by
 * src/game/ssb64/main.c): if a syDmaWriteSram has changed the store since
 * the last pump and writes are not held, hand a copy of it to the writer
 * thread, which writes the card while the game goes on; and log what the
 * writer has finished since the last pump. Main thread only. */
void sy_sram_pump(void);

/* The pump, and then wait for the card: the store is on it (or has
 * failed to get there) when this returns. The host tests' synchronous
 * write. */
void sy_sram_flush(void);

/* Write the store now, changed or not -- the player asked (the MEMORY
 * CARD page). Asynchronous like the pump; held or not. */
void sy_sram_save_now(void);

/* Wait until the writer has nothing left to write. Before anything that
 * changes which card the store is backed by, or replaces the store
 * (sy_sram_init does it itself), and before the console is reset. */
void sy_sram_quiesce(void);

/* Keep the store's changes in RAM: the pump publishes nothing while a
 * hold is taken, and the change waits for the release (the boot check,
 * while the player has not yet said which card). Holds count. */
void sy_sram_hold(void);
void sy_sram_release(void);

/* TRUE when a write has failed and none has landed since; cleared by
 * sy_sram_clear_failure (or the next write that lands). A failed write is
 * not retried: the next change to the store is the next attempt. */
int sy_sram_failed(void);
void sy_sram_clear_failure(void);

/* Whether the writer is writing, or has a shot queued. */
int sy_sram_busy(void);

/* Whether the "SAVING..." notice should be up: while the writer is busy,
 * and for at least a second from the shot that started it, so a write
 * that finishes in a millisecond is still a notice a player can
 * read. */
int sy_sram_notice_visible(void);

/* Label the next shot's notice "LOADING..." instead of "SAVING...": for the
 * write the game makes as it starts up (the title's boot counter), which
 * a player sees as the card being read in. Any later shot says SAVING. */
void sy_sram_next_write_is_load(void);
int sy_sram_notice_is_load(void);

/* Whether the store holds a change the card does not. For the tests and
 * the log; no game code asks. */
int sy_sram_is_dirty(void);

/* The file the store is backed by -- "/vmu/a1/SSB64DC.SAV", the host's
 * stand-in path or fake card ("host:b1"), or NULL when there is nowhere
 * to write. Valid after sy_sram_init. */
const char *sy_sram_backing(void);

/* The card the store is backed by: TRUE with *port (0-3) and *unit (1-2)
 * filled, or FALSE when there is none. Valid after sy_sram_init. */
int sy_sram_card(int *port, int *unit);

#ifdef FT_HOSTTEST
/* The host has no maple bus, so card A1 can be a plain file and the test
 * says where (src/dc/vmucard.h has the rest of the host's cards). The
 * store is backed by it from this call on, without waiting for
 * sy_sram_init; NULL (the default) takes the card out. It does not itself
 * read or write anything. */
void sy_sram_set_host_path(const char *path);

/* The host's writer runs inside the pump, at once. Paused, it runs only
 * in sy_sram_quiesce (and sy_sram_flush), so a test can pile shots up
 * and watch them coalesce. */
void sy_sram_host_pause_writer(int paused);

/* The clock sy_sram_notice_visible reads, in ms; the host's never moves
 * on its own. */
void sy_sram_host_set_clock(uint64_t ms);

/* Writes the writer has made since boot, landed or not. */
uint32_t sy_sram_host_writes(void);
#endif

#endif /* SSB64DC_VMUSAVE_H */
