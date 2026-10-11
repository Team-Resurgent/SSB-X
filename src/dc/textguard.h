/* textguard.h -- -DDB_TEXT_GUARD (src/dc/db.h): the program's code and
 * constant data, checksummed in 1 KB blocks, so a stray write into them
 * is reported with its address instead of turning up later as an
 * illegal instruction somewhere that ran fine a thousand times before.
 *
 * Why it exists: the hardware VS soak of 2026-09-27 stopped with SIGILL
 * on an rts in mnVSResultsSetPlayerTagPosition, the 29th time that
 * function ran, with PR and the stack pointer both correct. Nothing but
 * the instruction bytes themselves can have been wrong, and the heap
 * walk (which only sees the heap) was clean. dcload's gdb relay cannot
 * read memory, so the check has to run on the console.
 *
 * The sums are taken at the first check (after gdb_init, so a gdb
 * breakpoint already written into the code is part of the baseline).
 * text_guard_check then sweeps a few blocks a frame, round robin, or
 * every block when asked for 0. It reads through the uncached P2 window:
 * a DMA write lands in RAM behind the operand cache, and RAM is what an
 * instruction fetch that misses the I-cache gets. A block that changed
 * is reported once -- its range, the frame, and all 256 words, for a
 * diff against the ELF (sh-elf-objdump -s --start-address) -- and then
 * re-summed, so a second write to it is reported too. After
 * TEXT_GUARD_REPORTS reports it stays quiet. gdb can
 * `break text_guard_failed`.
 *
 * KOS keeps its 4 KB interrupt stack inside .text, under krn_stack
 * (entry.s), with nothing to stop an overflow but the code below it. Its
 * words are left out of the sums and painted at the first check; the
 * per-scene line "text guard: <scene> -- clean ... interrupt stack
 * deepest N of 4096" says how deep interrupts have gone, and an overflow
 * shows up as the code block under it changing. KOS's irq_srt_addr, a
 * variable in .text rewritten at every context switch, is left out too.
 *
 * Without the flag the macros are empty and nothing here is compiled. */
#ifndef SSB_DC_TEXTGUARD_H
#define SSB_DC_TEXTGUARD_H

#if defined(DB_TEXT_GUARD) && defined(_arch_dreamcast)

/* Blocks checked per frame end: 64 KB, a full sweep in about 19 frames. */
#ifndef TEXT_GUARD_BLOCKS_PER_FRAME
#define TEXT_GUARD_BLOCKS_PER_FRAME 64
#endif

/* Check `blocks` blocks (0: all of them). 0 when clean, -1 when a block
 * changed (reported under `where`). */
int text_guard_check(const char *where, unsigned blocks);

/* Called once per changed block; a place for a gdb breakpoint. */
void text_guard_failed(void);

#define TEXT_GUARD_FRAME(where) \
    text_guard_check((where), TEXT_GUARD_BLOCKS_PER_FRAME)
#define TEXT_GUARD_ALL(where) text_guard_check((where), 0)
#else
#define TEXT_GUARD_FRAME(where) ((void)0)
#define TEXT_GUARD_ALL(where) ((void)0)
#endif

#endif /* SSB_DC_TEXTGUARD_H */
