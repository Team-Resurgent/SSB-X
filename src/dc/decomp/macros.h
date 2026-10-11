/* Shadows ssb-decomp-re/include/macros.h to undo one line of it.
 *
 * macros.h:32 is
 *
 *     #define __attribute__(x)
 *
 * -- every GCC attribute deleted, everywhere, for the rest of the
 * translation unit. In the decomp that is harmless and probably
 * necessary: it is built by IDO, which does not know the keyword, and the
 * only thing that reaches for it is ALIGNED(x) on a handful of libultra
 * globals the port does not compile.
 *
 * Here it is a trap, because the port's own headers use attributes and
 * the decomp's headers get included first. src/dc/mtx.h declares
 *
 *     typedef float mtx4_t[16] __attribute__((aligned(8)));
 *
 * so a file that includes <sys/obj.h> before "fighter.h" sees mtx4_t
 * with alignment 4, and Fighter.mtx lands at offset 1628 instead of 1632
 * -- in that file only. Two translation units then disagree about where
 * a fighter's matrices live, and one writes them four bytes away from
 * where the other reads them. It cost an afternoon:
 * the stage rendered 159 of its 206 triangles and every matrix dumped
 * clean, because the dump and the write were both on the wrong side.
 *
 * Undoing it also makes the decomp's own ALIGNED(x) real, which is what
 * it says on the tin and only ever increases an object's alignment.
 * Nothing the port compiles uses it (only src/libultra/, which it does
 * not), so no game structure moves.
 *
 * src/dc/mtx.h carries a _Static_assert on that alignment, so if this
 * header is ever bypassed the build stops rather than rendering wrong.
 */
#ifndef SSB_DC_DECOMP_MACROS_H
#define SSB_DC_DECOMP_MACROS_H

/* The real one, from $(SSB_DECOMP_DIR)/include -- everything in it is
 * wanted, including ALIGNED, UNUSED and PRINTF_CHECK, which only become
 * real once the line below is undone. */
#include_next <macros.h>

#undef __attribute__

#endif /* SSB_DC_DECOMP_MACROS_H */
