#ifndef DC_PERF_H
#define DC_PERF_H

/* -DDB_PERF: where the console's frame goes. Off by default and free
 * when off: every macro is empty.
 *
 * Phase timers live in taskman.c (draw passes, the PVR wait, the scene
 * finish). Subsystem timers bracket one GObj's display proc, never a single
 * draw, so the timer reads stay well under the work they measure. The
 * header-compile count is DBPERF_COMPILE() beside each call; its unit cost is
 * timed once at boot (db_perf_calibrate), so the cost is count * unit and
 * no timer sits inside the hot path.
 *
 * db_perf_report() prints one line per call and resets, and db.c calls it
 * beside its own 300-frame `db:` line, so the serial volume is unchanged. */

#ifdef DB_PERF
#include <stdint.h>

enum
{
    DBP_LAYERED,    /* model_display_layered: models drawn in the sprite band */
    DBP_FRAME,      /* fighter_frame: joint matrices, light dots, vertex transform */
    DBP_BATCH,      /* draw_batches: shade, clip, emit_tri, every baked model */
    DBP_SHADE,      /* inside draw_batch: shade() + UVs, three corners a triangle */
    DBP_CLIP,       /* inside draw_batch: clip_near and clip_planes */
    DBP_EMIT,       /* inside draw_batch: emit_tri, divide, screen map, pvr_prim */
    DBP_NOCACHE,    /* batches drawn without the vertex cache (calls only) */
    DBP_FILL,       /* vertex-cache fills (calls only) */
    DBP_CULLED,     /* triangles skipped by tri_culled (calls only) */
    DBP_CAMPREP,    /* func_80017D3C: gcPrepCameraViewport + gcPrepCameraMatrix (calls = cameras) */
    DBP_CAMFUNC,    /* func_80017D3C: the camera's own function (a wallpaper) */
    DBP_CAPTURE,    /* func_80017D3C: the link-list walk that runs every display proc */
    DBP_FRAMEJ,     /* inside fighter_frame: the per-joint light and texgen columns */
    DBP_FRAMEV,     /* inside fighter_frame: the per-vertex transform (calls = vertices) */
    DBP_PARTICLE,   /* efDisplayZPersp*ProcDisplay around lbParticleDrawTextures */
    DBP_SPRITE,     /* lbCommonDrawSObjAttr / NoAttr: HUD, menus, text sprites */
    DBP_SHADOW,     /* ftShadowProcDisplay */
    DBP_AFTER,      /* ftDisplayMainDrawAfterImage */
    DBP_COUNT
};

extern uint64_t gDBPerfNs[DBP_COUNT];
extern uint32_t gDBPerfCalls[DBP_COUNT];
extern uint32_t gDBPerfCompile;
extern uint32_t gDBPerfReadNs;   /* one db_perf_now(), timed at boot */

uint64_t db_perf_now(void);
void db_perf_proc_add(void *fn, uint64_t ns);
void db_perf_calibrate(void);
void db_perf_report(void);

#define DBPERF_BEGIN(k) uint64_t dbperf_t0_##k = db_perf_now()
#define DBPERF_END(k) \
    do { gDBPerfNs[k] += db_perf_now() - dbperf_t0_##k; gDBPerfCalls[k]++; } while (0)

/* A decomp function timed without losing its name: the body stays under the
 * decomp's name (tools that read what a function calls
 * find it by that name), and the port attaches or calls
 * DBPERF_TIMED(fn) in its place. DBPERF_DEFINE_TIMED, at file scope after fn
 * is declared and with no semicolon, defines the one-argument wrapper; off,
 * both are nothing and DBPERF_TIMED(fn) is fn. */
#define DBPERF_DEFINE_TIMED(k, fn, argtype) \
    static void fn##_timed(argtype a) { DBPERF_BEGIN(k); fn(a); DBPERF_END(k); }
#define DBPERF_TIMED(fn) fn##_timed

#define DBPERF_COMPILE() (gDBPerfCompile++)
#define DBPERF_NOCACHE() (gDBPerfCalls[DBP_NOCACHE]++)
#define DBPERF_FILL() (gDBPerfCalls[DBP_FILL]++)
#define gDBPerfCalls_framev_add(n) (gDBPerfCalls[DBP_FRAMEV] += (uint32_t)(n) - 1u)
#define DBPERF_CULLED() (gDBPerfCalls[DBP_CULLED]++)

/* Sub-phases of one loop: DBPERF_LAP_BEGIN reads the clock once, each
 * DBPERF_LAP(k) charges the time since the last read to k, less the
 * cost of the read itself (gDBPerfReadNs), and re-arms. */
#define DBPERF_LAP_BEGIN() uint64_t dbperf_lap = db_perf_now()
#define DBPERF_LAP(k) \
    do { uint64_t dbperf_n = db_perf_now(); \
         uint64_t dbperf_d = dbperf_n - dbperf_lap; \
         gDBPerfNs[k] += (dbperf_d > gDBPerfReadNs) ? dbperf_d - gDBPerfReadNs : 0; \
         gDBPerfCalls[k]++; dbperf_lap = dbperf_n; } while (0)
#else
#define DBPERF_BEGIN(k) ((void)0)
#define DBPERF_END(k) ((void)0)
#define DBPERF_DEFINE_TIMED(k, fn, argtype)
#define DBPERF_TIMED(fn) fn
#define DBPERF_COMPILE() ((void)0)
#define DBPERF_NOCACHE() ((void)0)
#define DBPERF_FILL() ((void)0)
#define gDBPerfCalls_framev_add(n) ((void)0)
#define DBPERF_CULLED() ((void)0)
#define DBPERF_LAP_BEGIN() ((void)0)
#define DBPERF_LAP(k) ((void)0)
#endif

#endif
