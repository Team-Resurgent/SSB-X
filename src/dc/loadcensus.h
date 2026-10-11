#ifndef DC_LOADCENSUS_H
#define DC_LOADCENSUS_H

/* -DDB_LOAD_CENSUS: the load census. Every
 * scene the game enters is a NODE, and a node records what it loaded,
 * from where, how long its load took, and what memory it held -- at its
 * start, once its first frame was drawn, at its peak while it ran, and
 * at its end -- so a run of scenes (a 1P ladder, a soak, the opening
 * movie) can be read offline as a chain of nodes by
 * tools/check/loadcensus.py, which keeps runs in an SQLite database and
 * looks for files that are given back and read again.
 *
 * Off by default and empty when off: every hook below is a macro that
 * compiles to nothing. Records are kept in RAM and printed when the NEXT
 * node begins (and the print's own time is recorded, so the analysis
 * takes it back out of the transition it sits in), never while a load
 * is running. Lines start "lc: ", at DBG_WARNING so DB_QUIET keeps them:
 *
 *   lc: H <node> <now_us64> <records> <dropped> <print_us of the last>
 *   lc: n <id> <string>              a name, the first time it is needed
 *   lc: <op> <t_us32> <id> <v0> <v1> <v2> <v3> <v4> <v5>
 *
 * ops (t is the event's start, low 32 bits of the microsecond clock):
 *   N node begins        id scene name; v0 scene kind; v1 router (0 the
 *                        scene manager, 1 the 1P ladder); v2 heap size
 *   F a file, at close   id path; v0 size; v1 bytes off the medium;
 *                        v2 medium us; v3 source (0 disc, 1 models.bnd,
 *                        2 hold list, 3 read ahead, 4 the loader thread
 *                        reading ahead); v4 phase (0 load, 1 play);
 *                        v5 reads
 *   M memory             id the moment ("begin", "loaded", "peak",
 *                        "end"); v0 malloc bytes in use; v1 malloc free
 *                        (holes + what sbrk can still give); v2 the top
 *                        (largest block sure to fit); v3 scene heap
 *                        used; v4 PVR bytes free; v5 sound RAM in use
 *   S sound upload       v0 effect bytes, v1 music bytes, v2 us
 *   X a marker           id what ("ftfree <kind>", "ftkeep <kind>",
 *                        "ftnew <kind>", "fthit <kind>"); v0 its value
 *   L first frame drawn  v0 tics run before it
 *   E node ends          v0 tics, v1 frames drawn, v2 t of the last
 *                        frame drawn (the transition starts there) */

#include <stdint.h>

#if defined(DB_LOAD_CENSUS) && defined(_arch_dreamcast)

void lc_node(int32_t kind, const char *name, int router);
void lc_file(const char *path, uint32_t t0, uint32_t size, uint32_t bytes,
             uint32_t us, int src, uint32_t reads);
void lc_sound(uint32_t fgm_bytes, uint32_t bgm_bytes, uint32_t us);
void lc_mark(const char *what, uint32_t value);
void lc_frame(int drew);
uint32_t lc_now(void);

#else

#define lc_node(kind, name, router) ((void)0)
#define lc_file(path, t0, size, bytes, us, src, reads) ((void)0)
#define lc_sound(f, b, us) ((void)0)
#define lc_mark(what, value) ((void)0)
#define lc_frame(drew) ((void)0)
#define lc_now() 0u

#endif

#endif
