/* taskman.c -- see taskman.h. The scene heap half of sys/taskman.c. */
#include "loadcensus.h"
#include "taskman.h"
#include "perf.h"
#include "objpvr.h"
#include "assetroot.h"            /* asset_prefetch_go */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sys/video.h>          /* gSYVideoResWidth/Height */

/* sys/taskman.h and sys/malloc.h do not include their own prerequisites;
 * the decomp's own files reach them through "common.h". sys/obj.h is
 * that path here -- it pulls ssb_types.h, the PR headers and taskman.h
 * itself, in the order objtypes.h establishes. */
#include <sys/obj.h>
#include <sys/malloc.h>
#include <sys/taskman.h>

#ifndef FT_HOSTTEST
#include <kos.h>
#include <dc/pvr.h>
#include "dcpvr.h"
#include "fgm.h"
#ifdef DB_PVR_BUDGET
#include "fighter.h"
#endif
#else
#include <stdio.h>
#define dbglog(lvl, ...) fprintf(stderr, __VA_ARGS__)
#define DBG_ERROR 0
#endif

/* sys/taskman.c:159-162 */
SYMallocRegion gSYTaskmanGraphicsHeap;
SYMallocRegion gSYTaskmanGeneralHeap;

/* The block the region is cut from. The N64 had no such pointer: its heap
 * was whatever followed the overlay's BSS, so there was nothing to own. */
static void *sGeneralHeapBlock;

/* The port's own -- see taskman.h. */
static u32 sGeneralHeapEpoch;

/* sys/taskman.c:257-260, verbatim. The id is the game's own 0x10000,
 * which only ever appears in syMallocSet's overflow message. */
/* The port's own -- see taskman.h. */
static void (*sSYTaskmanHeapResetHooks[SY_TASKMAN_HEAP_RESET_HOOKS])(void);
static int sSYTaskmanHeapResetHookCount;

/* The port's own -- see taskman.h. */
static void (*sSYTaskmanDrawPassHook)(void);

void syTaskmanSetDrawPassHook(void (*hook)(void))
{
    sSYTaskmanDrawPassHook = hook;
}

/* The port's own -- see taskman.h. */
static void (*sSYTaskmanFrameEndHook)(void);
static void (*sSYTaskmanOverlayHook)(int list);

void syTaskmanSetFrameEndHook(void (*hook)(void))
{
    sSYTaskmanFrameEndHook = hook;
}

void syTaskmanSetOverlayHook(void (*hook)(int list))
{
    sSYTaskmanOverlayHook = hook;
}

void syTaskmanAddHeapResetHook(void (*hook)(void))
{
    int i;

    for (i = 0; i < sSYTaskmanHeapResetHookCount; i++)
    {
        if (sSYTaskmanHeapResetHooks[i] == hook)
        {
            return;
        }
    }
    if (sSYTaskmanHeapResetHookCount >= SY_TASKMAN_HEAP_RESET_HOOKS)
    {
        /* Never quietly: a dropped hook is a module whose scene-heap
         * pointers outlive the heap, and the next scene writes through
         * them. Five of the slots are used today (taskman.h lists them);
         * raise the header constant. */
        dbglog(DBG_ERROR, "taskman: no room for heap-reset hook %p, all %d "
               "are taken -- raise SY_TASKMAN_HEAP_RESET_HOOKS\n",
               (void *)hook, SY_TASKMAN_HEAP_RESET_HOOKS);
        abort();
    }
    sSYTaskmanHeapResetHooks[sSYTaskmanHeapResetHookCount++] = hook;
}

void syTaskmanInitGeneralHeap(void *start, u32 size)
{
    syMallocInit(&gSYTaskmanGeneralHeap, 0x10000, start, size);
    sGeneralHeapEpoch++;
}

u32 syTaskmanGeneralHeapEpoch(void)
{
    return sGeneralHeapEpoch;
}

/* sys/taskman.c:263-267 is `return syMallocSet(&gSYTaskmanGeneralHeap,
 * size, align)`.
 *
 * DIVERGES: the alignment is applied here rather than left to
 * syMallocSet. sys/malloc.c:20 computes it as
 *
 *     aligned = (u8*)(((uintptr_t)bp->ptr + offset) & ~(offset));
 *
 * where `offset` is a u32, so `~offset` is a 32-bit 0xFFFFFFFE. On the
 * N64 and on the SH-4 `uintptr_t` is 32 bits and that is exact. On the
 * x86-64 host the cross-tests run on it is zero-extended to
 * 0x00000000FFFFFFFE, which masks away the top half of every pointer the
 * allocator returns -- the tests segfault on the first table they build.
 *
 * Doing the same arithmetic in uintptr_t here and then asking syMallocSet
 * for alignment 0 (its `else aligned = bp->ptr` branch) gives byte-for-
 * byte the same addresses on target, keeps sys/malloc.c unmodified, and
 * keeps host and target on one path instead of an #ifdef. Everything else
 * -- the bump, the bounds check and the overflow halt -- is still the
 * decomp's. */
void *syTaskmanMalloc(size_t size, u32 align)
{
    SYMallocRegion *bp = &gSYTaskmanGeneralHeap;

    if (align != 0)
    {
        uintptr_t mask = (uintptr_t)align - 1;

        bp->ptr = (void *)(((uintptr_t)bp->ptr + mask) & ~mask);
    }
    return syMallocSet(bp, size, 0);
}

int syTaskmanMakeGeneralHeap(size_t size)
{
    void *block = malloc(size);

    if (block == NULL)
    {
        dbglog(DBG_ERROR, "taskman: no room for a %u byte scene heap\n",
               (unsigned)size);
        return -1;
    }
    free(sGeneralHeapBlock);
    sGeneralHeapBlock = block;
    syTaskmanInitGeneralHeap(block, (u32)size);
    return 0;
}

/* sys/malloc.c:6-9 syMallocReset, which is what the scene manager reaches
 * for between scenes.
 *
 * The sprite banks go with it. A bank's records are syTaskmanMalloc'd
 * out of this heap on purpose -- src/dc/sprite.h: "so they vanish with
 * it, like the game's file" -- and ftManagerSetupFilesAllKind's test for
 * "already resident" is the bank's is_loaded flag, so a bank left marked
 * loaded across a reset is a pointer into a region about to be handed
 * out again. The scene manager makes the same call at every scene
 * change and that covered every case there was until sudden death: two
 * battles inside one scene entry reset the heap without going through
 * it, and the second battle skipped reloading the fighter's stock-icon
 * bank because the first battle's still said loaded. So
 * the hook fires here as well, where the heap is actually emptied --
 * before syMallocReset, because a bank's texture pointers are in the
 * region about to go. Between scenes it finds nothing left to release.
 */
void syTaskmanResetGeneralHeap(void)
{
    int i;

    for (i = 0; i < sSYTaskmanHeapResetHookCount; i++)
    {
        sSYTaskmanHeapResetHooks[i]();
    }

    /* -DSCENE_HEAP_POISON, off by default: fill the region with a value
     * no pointer, count or float wants before handing it out again.
     * A scene that kept a pointer into the last scene's heap reads the
     * same shape either way -- the bump allocator hands the same
     * addresses back -- so the fault it causes depends on what the new
     * scene happens to have put there, which is why it took a second
     * lap of the whole chain to show. Poisoned, it is the first read.
     * src/dc/overlay.h is what it found. */
#ifdef SCENE_HEAP_POISON
    if (gSYTaskmanGeneralHeap.start != NULL)
    {
        memset(gSYTaskmanGeneralHeap.start, 0xDE,
               (size_t)((uintptr_t)gSYTaskmanGeneralHeap.end -
                        (uintptr_t)gSYTaskmanGeneralHeap.start));
    }
#endif
    syMallocReset(&gSYTaskmanGeneralHeap);
    sGeneralHeapEpoch++;
}

size_t syTaskmanGeneralHeapUsed(void)
{
    if (gSYTaskmanGeneralHeap.start == NULL)
        return 0;
    return (size_t)((uintptr_t)gSYTaskmanGeneralHeap.ptr -
                    (uintptr_t)gSYTaskmanGeneralHeap.start);
}

size_t syTaskmanGeneralHeapSize(void)
{
    if (gSYTaskmanGeneralHeap.start == NULL)
        return 0;
    return (size_t)((uintptr_t)gSYTaskmanGeneralHeap.end -
                    (uintptr_t)gSYTaskmanGeneralHeap.start);
}


/* ---- the frame loop --------------------------------------------------
 *
 * sys/taskman.c:32-40, verbatim. The decomp keeps this type inside
 * taskman.c and so does the port.
 */
typedef struct SYTaskFunction
{
    /* 0x00 */ u16 flags;
    /* 0x04 */ void (*scene_update)();
    /* 0x08 */ void (*task_update)(struct SYTaskFunction *);
    /* 0x0C */ void (*scene_draw)();
    /* 0x10 */ void (*task_draw)(struct SYTaskFunction *);

} SYTaskFunction; /* size == 0x14 */

/* sys/taskman.c:118, 216, 218-219, 128-129 */
static SYTaskFunction sSYTaskmanDefaultFunction;
static void (*sSYTaskmanFuncController)(void);
static u16 sSYTaskmanUpdateInterval = 1;
static u16 sSYTaskmanFrameInterval = 1;

u32 dSYTaskmanUpdateCount;
u32 dSYTaskmanFrameCount;

/* sys/scheduler.c:101, 109-118 sSYSchedulerTicCount and its two
 * accessors, which live here rather than in a scheduler.c the port does
 * not have: this file is where the port's tic is.
 *
 * On the N64 this counts VI retraces, incremented in the scheduler's
 * retrace handler (scheduler.c:1042) and read by whoever wants elapsed
 * time. The only reader in the ported game is the match clock
 * (ifCommonTimerFuncRun, src/dc/ifcommon.c), which takes the difference
 * against its own stamp; the battle runs at an update interval of 1, so
 * a retrace and a tic are the same thing and the count is incremented
 * beside dSYTaskmanUpdateCount in syTaskmanRunFrame. Unlike that one it
 * is not reset per scene, because the game's is not: the battle sets it
 * to zero itself, at GO. */
static u32 sSYSchedulerTicCount;
static u32 sSYSchedulerTicBaseCount;
static OSTime sSYSchedulerTicBaseTime;
static sb32 sSYSchedulerTicBaseValid;

/* taskman.h */
volatile u32 gSYTaskmanAlive;

/* sys/scheduler.c:109-112, verbatim */
void sySchedulerSetTicCount(u32 tics)
{
    sSYSchedulerTicCount = tics;
    sSYSchedulerTicBaseCount = tics;
    sSYSchedulerTicBaseTime = osGetTime();
    sSYSchedulerTicBaseValid = TRUE;
}

/* The opening's `while (sySchedulerGetTicCount() < N) continue;`, which
 * ends every mvopening scene's FuncStart but the Room's (mvopening*.c,
 * 18 of them, N absolute since the Room set the count to 0 as it started
 * the BGM). On the N64 the count is the retrace interrupt's, so it runs
 * through the scene loads too: the wait holds a scene back until the music
 * has reached N/60 s, and that is the pause between the characters -- what
 * remains of a fixed budget after the overlay DMA, the file loads and the
 * object setup are paid. Mario's targets 1515 and Donkey's 1605 with a
 * 60 tic scene between them leave a 30 tic pause, if the load fits.
 *
 * DIVERGES: the port's count is per update, and stands still through a
 * load, so it cannot be spun on; the wait is on the wall clock instead,
 * from the moment the count was last set, at the NTSC retrace rate
 * (60/1.001 Hz). It sleeps on the vblank so the audio thread runs, and
 * leaves the count at N. Does nothing when nothing set the count first (a
 * scene booted on its own for a probe), or when the target is more than
 * 10 s away, which only a stale base can be. The log line is the port's
 * own measure of the pause: `arrived` is the tic the scene start got to,
 * `held` how long the wait then slept. */
void sySchedulerWaitTicCount(u32 target)
{
#ifndef FT_HOSTTEST
    OSTime now, deadline;
    u32 arrived;

    /* the scene's loads are done by the time it waits for its cue, so
     * the loader may have the drive for the scenes after (assetroot.h) */
    asset_prefetch_go();

    if (sSYSchedulerTicBaseValid == FALSE)
    {
        return;
    }
    now = osGetTime();
    deadline = sSYSchedulerTicBaseTime +
               (OSTime)(target - sSYSchedulerTicBaseCount) * 1001000 / 60;

    arrived = sSYSchedulerTicBaseCount +
              (u32)((now - sSYSchedulerTicBaseTime) * 60 / 1001000);
    if (deadline > now + 10000000)
    {
        dbglog(DBG_WARNING, "taskman: tic wait to %u skipped, %u ms away\n",
               target, (unsigned)((deadline - now) / 1000));
        return;
    }
    while (osGetTime() < deadline)
    {
        fgm_defer_pump();
        gSYTaskmanAlive++;
        vid_waitvbl();
    }
    dbglog(DBG_INFO, "taskman: tic wait to %u: arrived at %u, held %d tics\n",
           target, arrived, (int)target - (int)arrived);
    sSYSchedulerTicCount = target;
#else
    (void)target;
#endif
}

/* sys/scheduler.c:115-118, verbatim */
u32 sySchedulerGetTicCount(void)
{
    return sSYSchedulerTicCount;
}

/* sys/taskman.c:995-1010's sSYTaskmanUpdateTimeDelta and
 * sSYTaskmanFrameTimeDelta: how long the last update and the last draw
 * took. The decomp measures them with osGetCount and divides by 2971 --
 * an unexplained constant, and one of the few places the game times
 * itself. DIVERGES: the port measures in microseconds through osGetTime
 * (src/dc/sysshim.c), because that is a unit a log line can be read in, measured where the game measures them. */
u32 dSYTaskmanUpdateTimeDelta;
u32 dSYTaskmanFrameTimeDelta;

/* sys/taskman.c:99, the taskman's own status. Only two of its three
 * values can happen here -- see syTaskmanCheckBreakLoop. */
static s32 sSYTaskmanStatus = nSYTaskmanStatusDefault;

/* sys/taskman.c:1286-1290, verbatim. */
void syTaskmanSetIntervals(u16 update, u16 framedraw)
{
    sSYTaskmanUpdateInterval = update;
    sSYTaskmanFrameInterval = framedraw;
}

/* sys/taskman.c:891-894, verbatim. The one way a scene says it is over:
 * ifCommonBattleSetUpdateInterface calls this three tics after GAME SET,
 * and the frame loop stops running the scene. */
void syTaskmanSetLoadScene(void)
{
    sSYTaskmanStatus = nSYTaskmanStatusLoadScene;
}

/* sys/taskman.c:904-928 syTaskmanCheckBreakLoop asks whether the scene
 * has asked to end or the reset button was pressed, so the loop can
 * leave and gcEjectAll can run. The first arm is live:
 * syTaskmanSetLoadScene above is what a finished battle calls.
 *
 * DIVERGES: nSYTaskmanStatusUnk2 is the reset-button path, and reaches
 * the scheduler through func_80000970 to swap the framebuffer. There is
 * no reset handler here, so nothing ever sets that status and the arm is
 * gone rather than half-written. */
sb32 syTaskmanCheckBreakLoop(void)
{
    switch (sSYTaskmanStatus)
    {
    case nSYTaskmanStatusLoadScene:
        return TRUE;

    default:
        return FALSE;
    }
}

/* The port's own, alongside syTaskmanRunFrame/syTaskmanSetupPools above:
 * the one line syTaskmanRunTask's own loop runs before its while (TRUE)
 * (line 807 in this file), broken out for a caller that never enters
 * that loop. Only the host test wants it -- a scene that calls
 * syTaskmanSetLoadScene itself (src/dc/scautodemo.c's scAutoDemoExit) and is then driven tic-by-tic by hand, the way
 * test_autodemo drives scAutoDemoFuncRun, leaves sSYTaskmanStatus set
 * for the rest of the process with nothing else in this file able to
 * clear it back; a real scene never needs this because starting the
 * next one always runs through syTaskmanRunTask's own reset first. */
void syTaskmanResetBreakLoop(void)
{
    sSYTaskmanStatus = nSYTaskmanStatusDefault;
}

#ifdef DB_GOBJ_GUARD
#include <sys/debug.h>
#include "scmanager.h"
/* -DDB_GOBJ_GUARD, off by default: walk what gcRunAll is about to walk
 * and say where a link list stops pointing at GObjs.
 *
 * gcRunAll calls gobj->func_run without looking (objman.c:2132), so a
 * list that runs into something that is not a GObj shows up as a jump
 * into nowhere, with the panic naming gcRunGObj and nothing else. This
 * names the link, the position in it, the GObj before the bad one, and
 * the first words of what it found -- which is how the character
 * select's second visit turned from `PC 0010004c` into "link 25 runs
 * into a 13x90 sprite" and from there into src/dc/overlay.h. It also
 * dumps every link list on the first update of each scene, which is
 * what shows a list carrying a node from the scene before. */

static int taskman_guard_gobj_ok(const GObj *gobj)
{
    uintptr_t p = (uintptr_t)gobj;
    uintptr_t fp;

    if ((p & 3) != 0)
        return 0;
    if (p < (uintptr_t)gSYTaskmanGeneralHeap.start ||
        p >= (uintptr_t)gSYTaskmanGeneralHeap.end)
        return 0;

    fp = (uintptr_t)gobj->func_run;
    if (fp != 0 && (fp < 0x8c000000u || fp >= (uintptr_t)&_etext || (fp & 1) != 0))
        return 0;

    return 1;
}

static void taskman_guard_dump(void)
{
    s32 i;

    syDebugPrintf("guard: scene %d update 0, sizeof(GObj) %d\n",
                  (int)gSCManagerSceneData.scene_curr, (int)sizeof(GObj));
    for (i = 0; i < (s32)ARRAY_COUNT(gGCCommonLinks); i++)
    {
        GObj *gobj = gGCCommonLinks[i];
        s32 n = 0;

        while (gobj != NULL && n < 64)
        {
            syDebugPrintf("guard:  link %d [%d] %p id %lu run %p disp %p"
                          " prev %p next %p\n",
                          (int)i, (int)n, (void *)gobj,
                          (unsigned long)gobj->id, (void *)gobj->func_run,
                          (void *)gobj->proc_display, (void *)gobj->link_prev,
                          (void *)gobj->link_next);
            n++;
            if (!taskman_guard_gobj_ok(gobj))
                break;
            gobj = gobj->link_next;
        }
    }
}

static void taskman_guard_links(void)
{
    s32 i;

    if (dSYTaskmanUpdateCount == 0)
    {
        taskman_guard_dump();
    }

    for (i = 0; i < (s32)ARRAY_COUNT(gGCCommonLinks); i++)
    {
        GObj *gobj = gGCCommonLinks[i];
        GObj *prev = NULL;
        s32 n = 0;

        while (gobj != NULL)
        {
            if (!taskman_guard_gobj_ok(gobj))
            {
                syDebugPrintf("guard: link %d entry %d gobj %p func_run %p"
                              " id %lu, after gobj %p id %lu func_run %p\n",
                              (int)i, (int)n, (void *)gobj,
                              (void *)gobj->func_run,
                              (unsigned long)gobj->id, (void *)prev,
                              (prev != NULL) ? (unsigned long)prev->id : 0ul,
                              (prev != NULL) ? (void *)prev->func_run : NULL);
                {
                    const u32 *w = (const u32 *)gobj;
                    s32 k;

                    for (k = 0; k < 12; k += 4)
                    {
                        syDebugPrintf("guard:  +%02d %08lx %08lx %08lx %08lx\n",
                                      (int)(k * 4), (unsigned long)w[k],
                                      (unsigned long)w[k + 1],
                                      (unsigned long)w[k + 2],
                                      (unsigned long)w[k + 3]);
                    }
                }
                break;
            }
            if (++n > 4096)
            {
                syDebugPrintf("guard: link %d does not end\n", (int)i);
                break;
            }
            prev = gobj;
            gobj = gobj->link_next;
        }
    }
}
#endif

/* The photo's two requests (taskman.h): the next frame drawn, and the
 * running scene's last. The rest of the photo is further down. */
static sb32 sSYTaskmanPhotoWanted;
static sb32 sSYTaskmanExitPhotoWanted;
static sb32 syTaskmanBeginPhoto(sb32 *wanted);
static void syTaskmanCaptureExitPhoto(SYTaskFunction *tfunc);

/* sys/taskman.c:1093-1103, verbatim but for the photo (taskman.h),
 * which has to be taken here: the ejection below is the last moment the
 * scene's GObjs exist. */
static void syTaskmanCommonTaskUpdate(SYTaskFunction *tfunc)
{
    sSYTaskmanFuncController();
#ifdef DB_GOBJ_GUARD
    taskman_guard_links();
#endif
    tfunc->scene_update();

    if (syTaskmanCheckBreakLoop() != FALSE)
    {
        syTaskmanCaptureExitPhoto(tfunc);
        gcEjectAll();
    }
}

/* sys/taskman.c:1105-1121. Four of its five lines bracket the scene's
 * draw: the graphics-heap reset and func_80004AB0 open a display-list
 * buffer and write the frame's leading RDP commands into it, and
 * func_800053CC, syVideoApplySettingsNoBlock and func_80004EFC close it
 * and hand the finished task to the RSP.
 *
 * The Dreamcast's bracket is the same idea one hardware down: wait for
 * the tile accelerator to be ready for a new scene, open it, let the
 * scene draw, close it and let the PVR render. So the lines are replaced
 * rather than dropped -- which also settles where a frame's PVR calls
 * belong. They belong here, because this is where the game says a frame
 * is drawn, and everything src/dc/objdisplay.c submits happens inside
 * scene_draw. */
/* What the N64's clear camera (gcMakeDefaultCameraGObj, scvsbattle.c:
 * 158) leaves black: everything outside the cameras' viewports. Each
 * camera here draws into the viewport it published (objpvr.h
 * gcGetViewport) and nothing scissors the PVR, so the frame paints the
 * border itself, four opaque quads nearer than anything -- the sprite
 * pass clamps to the same ten pixels (lb/lbcommon.c lbCommonDrawSprite),
 * so nothing it draws is covered. */
#ifndef FT_HOSTTEST
static void syTaskmanDrawBorder(void)
{
    static pvr_poly_hdr_t hdr;
    static sb32 compiled = FALSE;
    const DCViewport *vp = gcGetViewportUnion();
    float x0 = vp->cx - vp->hw, y0 = vp->cy - vp->hh;
    float x1 = vp->cx + vp->hw, y1 = vp->cy + vp->hh;
    float w = (float)gSYVideoResWidth * gDCScreenScale, h = (float)gSYVideoResHeight * gDCScreenScale;
    float rects[4][4];
    pvr_vertex_t v;
    int r, i;

    if (x0 <= 0.0F && y0 <= 0.0F && x1 >= w && y1 >= h)
    {
        return;
    }
    if (!compiled)
    {
        pvr_poly_cxt_t cxt;

        pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
        cxt.gen.culling = PVR_CULLING_NONE;
        cxt.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        DBPERF_COMPILE();
        pvr_poly_compile(&hdr, &cxt);
        compiled = TRUE;
    }
    rects[0][0] = 0.0F; rects[0][1] = 0.0F; rects[0][2] = w;    rects[0][3] = y0;
    rects[1][0] = 0.0F; rects[1][1] = y1;   rects[1][2] = w;    rects[1][3] = h;
    rects[2][0] = 0.0F; rects[2][1] = y0;   rects[2][2] = x0;   rects[2][3] = y1;
    rects[3][0] = x1;   rects[3][1] = y0;   rects[3][2] = w;    rects[3][3] = y1;

    pvr_prim(&hdr, sizeof(hdr));
    memset(&v, 0, sizeof(v));
    v.argb = 0xFF000000;
    v.z = 10000.0F;
    for (r = 0; r < 4; r++)
    {
        if (rects[r][2] <= rects[r][0] || rects[r][3] <= rects[r][1])
        {
            continue;
        }
        for (i = 0; i < 4; i++)
        {
            v.flags = (i == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v.x = (i & 2) ? rects[r][2] : rects[r][0];
            v.y = (i & 1) ? rects[r][1] : rects[r][3];
            pvr_prim(&v, sizeof(v));
        }
    }
}
#endif


#ifdef DB_PERF
/* -DDB_PERF (perf.h) */
uint64_t gDBPerfNs[DBP_COUNT];
uint32_t gDBPerfCalls[DBP_COUNT];
uint32_t gDBPerfCompile;
static uint64_t gDBPerfPassNs[3], gDBPerfWaitNs, gDBPerfFinishNs;
static uint32_t gDBPerfFrames;
static uint32_t gDBPerfCompileNs;   /* one pvr_poly_compile, measured at boot */
uint32_t gDBPerfReadNs;
/* The slowest frame of the window: the time between two scene finishes,
 * what the passes and the wait made of it, and dSYTaskmanUpdateCount when
 * it happened (match it against the `db: watch` lines). A gap over 250 ms
 * is a scene load, not a frame. Frames over 20, 30 and 40 ms are counted:
 * the vsync is 16.7, so those are one, two and three dropped. */
static uint64_t gDBPerfLastEnd, gDBPerfWorstNs, gDBPerfWorstPass[3], gDBPerfWorstWait;
static uint64_t gDBPerfPrevPass[3], gDBPerfPrevWait;
static uint32_t gDBPerfWorstTic, gDBPerfOver[3];

uint64_t db_perf_now(void)
{
#ifndef FT_HOSTTEST
    return timer_ns_gettime64();
#else
    return 0;
#endif
}

/* One typical textured header, compiled 2000 times. This is the unit the
 * per-frame compile count is priced at. */
void db_perf_calibrate(void)
{
#ifndef FT_HOSTTEST
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr __attribute__((aligned(32)));
    uint64_t t0, t1;
    int i;

    t0 = db_perf_now();
    for (i = 0; i < 2000; i++)
    {
        pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444, 64, 64,
                         (pvr_ptr_t)(uintptr_t)(0x05000000u + (uint32_t)i * 32u),
                         PVR_FILTER_BILINEAR);
        pvr_poly_compile(&hdr, &cxt);
    }
    t1 = db_perf_now();
    gDBPerfCompileNs = (uint32_t)((t1 - t0) / 2000u);
    t0 = db_perf_now();
    for (i = 0; i < 2000; i++)
        (void)db_perf_now();
    t1 = db_perf_now();
    gDBPerfReadNs = (uint32_t)((t1 - t0) / 2000u);
    dbglog(DBG_INFO, "perf: cxt_txr + compile = %lu ns each, one clock read = %lu ns\n",
           (unsigned long)gDBPerfCompileNs, (unsigned long)gDBPerfReadNs);
#endif
}

/* One line, per-frame averages, then reset. Called from db.c beside its
 * `db:` line (every 300 frames), so the serial volume does not change. */
void db_perf_report(void)
{
    static const char *const kName[DBP_COUNT] = {
        "layered", "frame", "batch", "shade", "clip", "emit", "nocache", "fill", "culled", "camprep", "camfunc", "capture", "framej", "framev", "particle", "sprite", "shadow", "after"
    };
    uint32_t n = gDBPerfFrames ? gDBPerfFrames : 1;
    char line[720];
    int len, k;

    len = snprintf(line, sizeof(line),
                   "perf: %lu frames; wait %lu us, pass op %lu pt %lu tr %lu, "
                   "finish %lu; compile %lu/f (%lu us)",
                   (unsigned long)gDBPerfFrames,
                   (unsigned long)(gDBPerfWaitNs / n / 1000u),
                   (unsigned long)(gDBPerfPassNs[0] / n / 1000u),
                   (unsigned long)(gDBPerfPassNs[1] / n / 1000u),
                   (unsigned long)(gDBPerfPassNs[2] / n / 1000u),
                   (unsigned long)(gDBPerfFinishNs / n / 1000u),
                   (unsigned long)(gDBPerfCompile / n),
                   (unsigned long)((uint64_t)gDBPerfCompile * gDBPerfCompileNs /
                                   n / 1000u));
    for (k = 0; k < DBP_COUNT && len > 0 && len < (int)sizeof(line); k++)
    {
        len += snprintf(line + len, sizeof(line) - (size_t)len,
                        "; %s %lu us (%lu calls)", kName[k],
                        (unsigned long)(gDBPerfNs[k] / n / 1000u),
                        (unsigned long)(gDBPerfCalls[k] / n));
    }
    if (len > 0 && len < (int)sizeof(line))
    {
        /* batch minus its three laps and the clock reads they cost: the
         * per-batch work (material_of, header pick, skips) and the loop */
        uint64_t laps = gDBPerfNs[DBP_SHADE] + gDBPerfNs[DBP_CLIP] +
                        gDBPerfNs[DBP_EMIT];
        uint64_t reads = (uint64_t)gDBPerfCalls[DBP_SHADE] +
                         gDBPerfCalls[DBP_CLIP] + gDBPerfCalls[DBP_EMIT];
        uint64_t used = laps + reads * gDBPerfReadNs;
        uint64_t setup = (gDBPerfNs[DBP_BATCH] > used) ? gDBPerfNs[DBP_BATCH] - used : 0;

        snprintf(line + len, sizeof(line) - (size_t)len,
                 "; batch setup %lu us, clock reads %lu/f",
                 (unsigned long)(setup / n / 1000u), (unsigned long)(reads / n));
    }
    dbglog(DBG_INFO, "%s\n", line);
    dbglog(DBG_INFO, "perf: worst frame %lu us at update %lu (pass op %lu pt %lu tr %lu, wait %lu); over 20 ms %lu, 30 ms %lu, 40 ms %lu\n",
           (unsigned long)(gDBPerfWorstNs / 1000u), (unsigned long)gDBPerfWorstTic,
           (unsigned long)(gDBPerfWorstPass[0] / 1000u),
           (unsigned long)(gDBPerfWorstPass[1] / 1000u),
           (unsigned long)(gDBPerfWorstPass[2] / 1000u),
           (unsigned long)(gDBPerfWorstWait / 1000u),
           (unsigned long)gDBPerfOver[0], (unsigned long)gDBPerfOver[1],
           (unsigned long)gDBPerfOver[2]);
    gDBPerfWorstNs = 0;
    gDBPerfOver[0] = gDBPerfOver[1] = gDBPerfOver[2] = 0;
#ifdef DB_PERF_PROCS
    {
        extern void db_perf_proc_report(uint32_t frames);

        db_perf_proc_report(gDBPerfFrames);
    }
#endif
    memset(gDBPerfNs, 0, sizeof(gDBPerfNs));
    memset(gDBPerfCalls, 0, sizeof(gDBPerfCalls));
    memset(gDBPerfPassNs, 0, sizeof(gDBPerfPassNs));
    gDBPerfWaitNs = gDBPerfFinishNs = 0;
    gDBPerfPrevWait = 0;
    gDBPerfLastEnd = 0;   /* the frame after this print is late by the print */
    memset(gDBPerfPrevPass, 0, sizeof(gDBPerfPrevPass));
    gDBPerfCompile = 0;
    gDBPerfFrames = 0;
}
#endif /* DB_PERF */

/* The scene's draw, list by list, into whatever scene the caller began:
 * the screen's (syTaskmanCommonTaskDraw) or the exit photo's
 * (syTaskmanBeginPhoto). `to_screen` is FALSE for a photo, which keeps
 * the port's overlay layer out of it (taskman.h). */
/* Whether a PVR scene is open -- between the draw's first pvr_list_begin
 * and its last pvr_list_finish -- for dc_pvr_vram_fence (src/dc/dcpvr.h),
 * which must not wait for a busy TA to start rendering while the busy
 * scene is the one being drawn. */
int gDCPVRSceneOpen;

static void syTaskmanDrawPasses(SYTaskFunction *tfunc, sb32 to_screen)
{
    /* Opaque first, then punch-through, then translucent -- the order
     * the PVR renders a tile in, so the passes read in the order their
     * output stacks up. Opaque stays first for syTaskmanDrawBorder
     * below, which fires on that pass alone. */
    static const int lists[3] = { PVR_LIST_OP_POLY, PVR_LIST_PT_POLY,
                                  PVR_LIST_TR_POLY };
    int pass;

    gcSetViewportFullScreen();
    gcResetViewportUnion();
    gDCPVRSceneOpen = TRUE;

    /* DIVERGES: the scene's draw runs once per PVR list. The N64 draws
     * the frame once into display lists and the RDP takes them in
     * order; the PVR takes opaque, punch-through and translucent
     * polygons as separate lists, each openable once per frame, and the
     * port's display procs submit only into the open one (objpvr.h
     * gcGetDrawList). Running the whole draw three times is what lets a
     * scene with several cameras -- a 3D one and a sprite one -- fill every list without any camera opening one. The walk
     * is cheap next to the geometry; a per-list vertex buffer (KOS's DMA
     * mode) would take the extra walks back if they ever show in the
     * budget. */
    for (pass = 0; pass < 3; pass++)
    {
#ifdef DB_PERF
        uint64_t perf_t0 = db_perf_now();
#endif
        gcSetDrawList(lists[pass]);
        if (sSYTaskmanDrawPassHook != NULL)
        {
            sSYTaskmanDrawPassHook();
        }
#ifndef FT_HOSTTEST
        pvr_list_begin(lists[pass]);
#endif
        tfunc->scene_draw();
#ifndef FT_HOSTTEST
        if (lists[pass] == PVR_LIST_OP_POLY)
        {
            syTaskmanDrawBorder();
        }
#endif
        if ((to_screen != FALSE) && (sSYTaskmanOverlayHook != NULL))
        {
            sSYTaskmanOverlayHook(lists[pass]);
        }
#ifndef FT_HOSTTEST
        pvr_list_finish();
#endif
#ifdef DB_PERF
        gDBPerfPassNs[pass] += db_perf_now() - perf_t0;
#endif
    }
    gDCPVRSceneOpen = FALSE;
}

static void syTaskmanCommonTaskDraw(SYTaskFunction *tfunc)
{
    sb32 to_screen = TRUE;
#ifdef DB_PERF
    uint64_t perf_t0;
#endif

    /* a scene that asked for a photo mid-scene gets this frame in it */
    if (syTaskmanBeginPhoto(&sSYTaskmanPhotoWanted) != FALSE)
    {
        to_screen = FALSE;
    }
    else
    {
#ifndef FT_HOSTTEST
#ifdef DB_PERF
        perf_t0 = db_perf_now();
#endif
        pvr_wait_ready();
#ifdef DB_PERF
        gDBPerfWaitNs += db_perf_now() - perf_t0;
#endif
        pvr_scene_begin();
#endif
    }
    syTaskmanDrawPasses(tfunc, to_screen);

#ifndef FT_HOSTTEST
#ifdef DB_PERF
    perf_t0 = db_perf_now();
#endif
    pvr_scene_finish();
#ifdef DB_PERF
    gDBPerfFinishNs += db_perf_now() - perf_t0;
    gDBPerfFrames++;
    {
        uint64_t now = db_perf_now();
        uint64_t dt = now - gDBPerfLastEnd;
        uint64_t pass[3], wait = gDBPerfWaitNs - gDBPerfPrevWait;
        int i;

        for (i = 0; i < 3; i++)
            pass[i] = gDBPerfPassNs[i] - gDBPerfPrevPass[i];
        if (gDBPerfLastEnd != 0 && dt < 250000000u)
        {
            if (dt > gDBPerfWorstNs && gDBPerfWaitNs >= gDBPerfPrevWait)
            {
                gDBPerfWorstNs = dt;
                gDBPerfWorstTic = dSYTaskmanUpdateCount;
                gDBPerfWorstWait = wait;
                for (i = 0; i < 3; i++)
                    gDBPerfWorstPass[i] = pass[i];
            }
            gDBPerfOver[0] += (dt > 20000000u);
            gDBPerfOver[1] += (dt > 30000000u);
            gDBPerfOver[2] += (dt > 40000000u);
        }
        gDBPerfLastEnd = now;
        gDBPerfPrevWait = gDBPerfWaitNs;
        for (i = 0; i < 3; i++)
            gDBPerfPrevPass[i] = gDBPerfPassNs[i];
    }
#endif
#endif

#if defined(DB_FB_DUMP) && !defined(FT_HOSTTEST)
    /* -DDB_FB_DUMP=f1,f2,...: the displayed frame onto serial at those
     * render frames, for tools/check/fbdump.py (src/dc/db.c
     * db_fb_dump_frame) */
    {
        extern void db_fb_dump_frame(void);

        db_fb_dump_frame();
    }
#endif

#if defined(DB_PVR_BUDGET) && !defined(FT_HOSTTEST)
    /* -DDB_PVR_BUDGET, off by default: what the frame cost the TA.
     *
     * The TA writes every parameter this draw submits into ONE vertex
     * buffer, sized once in dc_pvr_init, and on real hardware it simply
     * stops storing when that buffer is full -- the rest of the frame's
     * polygons are not drawn, and pvr_scene_finish then puts the
     * background plane at PVR_TA_VERTBUF_POS (KOS pvr_misc.c:203), which
     * by then is past the end and inside the object pointer blocks. So
     * an overrun is first missing and flickering geometry and then a
     * renderer following corrupted pointers. pvr_get_stats reads the high-water mark out of
     * PVR_TA_VERTBUF_POS (PVR_SYNC_REGDONE), so the NUMBER is measurable.
     *
     * One line a second: the last frame, the worst frame since boot, and
     * what is left of texture RAM. */
    {
        static uint32_t frames;
        unsigned worst_tile[3], worst_bytes, tris[3], pool;

        db_pvr_budget_frame(worst_tile, &worst_bytes, tris, &pool);

        if ((++frames % 60u) == 0u)
        {
            dbglog(DBG_INFO, "pvrbudget: tris op %u pt %u tr %u; worst tile "
                   "op %u pt %u tr %u; opb pool %u of %u blocks; "
                   "vtx %u of %u (%u%%); %u texture bytes free\n",
                   tris[0], tris[1], tris[2],
                   worst_tile[0], worst_tile[1], worst_tile[2],
                   pool, (unsigned)DB_PVR_OPB_POOL,
                   worst_bytes, (unsigned)DC_PVR_VERTEX_BUF_SIZE,
                   worst_bytes * 100u / DC_PVR_VERTEX_BUF_SIZE,
                   (unsigned)pvr_mem_available());
        }
    }
#endif

    if (syTaskmanCheckBreakLoop() != FALSE)
    {
        gcEjectAll();
    }
}

/* The photo (taskman.h). `age` counts the scene starts since the
 * capture: the scene that took it (age 0) and the one after (age 1) may
 * read it, and the start after that (age 2) frees it. */
#define SY_TASKMAN_PHOTO_W 640
#define SY_TASKMAN_PHOTO_H 480

static void *sSYTaskmanPhoto;
static s32 sSYTaskmanPhotoAge;

void syTaskmanWantPhoto(void)
{
    sSYTaskmanPhotoWanted = TRUE;
}

void syTaskmanWantExitPhoto(void)
{
    sSYTaskmanExitPhotoWanted = TRUE;
}

void syTaskmanReleasePhoto(void)
{
    if (sSYTaskmanPhoto != NULL)
    {
#ifndef FT_HOSTTEST
        /* a render may still be reading it, or writing it (dcpvr.h) */
        dc_pvr_vram_fence();
        pvr_mem_free(sSYTaskmanPhoto);
#endif
        sSYTaskmanPhoto = NULL;
    }
}

void *syTaskmanGetPhoto(s32 *w, s32 *h)
{
    if ((sSYTaskmanPhoto == NULL) || (sSYTaskmanPhotoAge > 1))
    {
        return NULL;
    }
#ifndef FT_HOSTTEST
    /* one register for every strided texture; the photo is the only one */
    pvr_txr_set_stride(SY_TASKMAN_PHOTO_W);
#endif
    *w = SY_TASKMAN_PHOTO_W;
    *h = SY_TASKMAN_PHOTO_H;

    return sSYTaskmanPhoto;
}

/* If `wanted`, clear it, begin the PVR scene into the photo rather than
 * onto the screen, and say so. The frame drawn into it is never shown: the screen
 * keeps the frame before it for one more retrace. FALSE -- and the
 * request dropped, with a log line -- when VRAM has no room. */
static sb32 syTaskmanBeginPhoto(sb32 *wanted)
{
    if (*wanted == FALSE)
    {
        return FALSE;
    }
    *wanted = FALSE;

    syTaskmanReleasePhoto();
#ifndef FT_HOSTTEST
    /* the allocation is a VRAM mutation too (dcpvr.h) */
    dc_pvr_vram_fence();
    sSYTaskmanPhoto = pvr_mem_malloc(SY_TASKMAN_PHOTO_W *
                                     SY_TASKMAN_PHOTO_H * sizeof(u16));
    if (sSYTaskmanPhoto == NULL)
    {
        dbglog(DBG_WARNING, "taskman: no VRAM for the photo "
               "(%u bytes free)\n", (unsigned)pvr_mem_available());
        return FALSE;
    }
    pvr_wait_ready();
    if (pvr_scene_begin_rtt(sSYTaskmanPhoto, SY_TASKMAN_PHOTO_W,
                            SY_TASKMAN_PHOTO_H, SY_TASKMAN_PHOTO_W) < 0)
    {
        dbglog(DBG_WARNING, "taskman: photo render refused\n");
        pvr_mem_free(sSYTaskmanPhoto);
        sSYTaskmanPhoto = NULL;
        return FALSE;
    }
#endif
    sSYTaskmanPhotoAge = 0;

    return TRUE;
}

/* The tic a scene ends has no draw (syTaskmanRunFrame), so the exit
 * photo is drawn here, one update past the frame on screen -- the frame
 * the N64's reader would find. It counts as a frame for the per-frame
 * counters (the sprite depth band) and nothing else. */
static void syTaskmanCaptureExitPhoto(SYTaskFunction *tfunc)
{
    if (syTaskmanBeginPhoto(&sSYTaskmanExitPhotoWanted) == FALSE)
    {
        return;
    }
    dSYTaskmanFrameCount++;
    syTaskmanDrawPasses(tfunc, FALSE);
#ifndef FT_HOSTTEST
    pvr_scene_finish();
#endif
}

/* sys/taskman.c:1159-1216 syTaskmanLoadScene, minus the N64.
 *
 * The decomp's body is: record the scene's update/draw pair, then cut the
 * task-graphics structures, the four display-list buffers, the graphics
 * heap and the RDP output buffer out of the general heap, install the
 * lighting and controller callbacks, zero the counters and call the
 * scene's start function. Everything between the first step and the last
 * two is display-list memory for an RSP that is not here, so it is gone
 * -- with it goes the 0xD000 the battle scene's graphics arena would have
 * taken out of the scene heap.
 *
 * It ends where the decomp ends it, in syTaskmanRunTask below -- the
 * scene's frame loop, which does not return until the scene is over.
 * The scene manager's own loop (src/dc/scmanager.c) is the decomp's
 * while (TRUE) and wants the scene to block the way the game's does.
 */
static void syTaskmanLoadScene(SYTaskmanSceneSetup *tscene,
                               void (*func_start)(void))
{
    sSYTaskmanDefaultFunction.flags = tscene->flags;
    sSYTaskmanDefaultFunction.scene_update = tscene->func_update;
    sSYTaskmanDefaultFunction.scene_draw = tscene->func_draw;

    sSYTaskmanFuncController = tscene->func_controller;

    dSYTaskmanUpdateCount = dSYTaskmanFrameCount = 0;

    /* the photo lives through the scene after the one that took it */
    if (++sSYTaskmanPhotoAge > 1)
    {
        syTaskmanReleasePhoto();
    }
    if (func_start != NULL)
    {
        func_start();
    }
#ifndef FT_HOSTTEST
    /* and at the latest here, for a scene that does not wait for a cue:
     * whatever it opens from now on is not its load (assetroot.h). Not
     * on the host, where the oracles link this file without assetroot.c */
    asset_prefetch_go();
#endif
    syTaskmanRunTask();
}

/* sys/taskman.c:1227-1284 syTaskmanStartTask, verbatim but for the heap
 * and the two lines syTaskmanLoadScene covers above. This is the function
 * that turns a scene's SYTaskmanSetup into the object system's pools:
 * every count in that struct becomes one syTaskmanMalloc out of the scene
 * heap, and gcSetupObjman threads them into the free lists.
 *
 * DIVERGES: the arena. On the N64 SYTaskmanSetup.arena_start is the link
 * map -- &ovl4_BSS_END in dSCVSBattleTaskmanSetup -- which has no meaning
 * in an ELF KallistiOS laid out, so a NULL arena_start means "keep the
 * region syTaskmanMakeGeneralHeap already made" instead.
 *
 * The heap still empties, and that matters: syTaskmanInitGeneralHeap is
 * syMallocInit, which sets ptr back to start, so on the N64 *every scene
 * starts with an empty general heap* -- that is where the game frees the
 * previous scene's collision tables, fighter instances and object pools,
 * all at once and without a single free(). A NULL arena_start keeps the
 * region and resets it, which is the same thing for everything but the
 * address. Anything a scene wants to survive into the next one has to
 * live somewhere else; the port's stage and fighter packs do (the C
 * heap, loaded before any scene runs).
 *
 * Worth knowing when reading those setups: the decomp's copies of them
 * carry zero for every pool count and for arena_size (scvsbattle.c:47-63,
 * mnvsmode.c:1676-1692 and every other scene alike), so the real numbers
 * are still in the ROM's data and not in the decomp. Callers here pick
 * their own and print the high-water mark.
 */
/* DIVERGES: the port cuts syTaskmanStartTask in two here. This half is
 * everything up to gcSetupObjman -- the pools -- and the whole of it is
 * the decomp's; the other half is the two task callbacks and
 * syTaskmanLoadScene, which now ends in the frame loop and does not
 * return. Nothing on the target calls this half alone. The host test
 * does: it drives its own tics and wants the object system without a
 * scene around it (src/game/ssb64/hosttest_ft.c). */
void syTaskmanSetupPools(SYTaskmanSetup *tsetup)
{
    GCSetup gcsetup;

    if (tsetup->scene_setup.arena_start != NULL)
    {
        syTaskmanInitGeneralHeap(tsetup->scene_setup.arena_start,
                                 tsetup->scene_setup.arena_size);
    }
    else syTaskmanResetGeneralHeap();

    gcsetup.gobjthreads = syTaskmanMalloc(sizeof(GObjThread) * tsetup->gobjthreads_num, 0x8);
    gcsetup.gobjthreads_num = tsetup->gobjthreads_num;
    gcsetup.gobjthreadstack_size = tsetup->gobjthreadstack_size;

    if (tsetup->gobjthreadstack_size != 0)
    {
        gcsetup.gobjthreadstacks = syTaskmanMalloc((tsetup->gobjthreadstack_size + offsetof(GObjStack, stack)) * tsetup->gobjthreadstacks_num, 0x8);
    }
    else gcsetup.gobjthreadstacks = NULL;

    gcsetup.gobjthreadstacks_num = tsetup->gobjthreadstacks_num;
    gcsetup.unk_gcsetup_0x14 = tsetup->unk4C;

    gcsetup.gobjprocs = syTaskmanMalloc(sizeof(GObjProcess) * tsetup->gobjprocs_num, 0x4);
    gcsetup.gobjprocs_num = tsetup->gobjprocs_num;

    gcsetup.gobjs = syTaskmanMalloc(tsetup->gobj_size * tsetup->gobjs_num, 0x8);
    gcsetup.gobjs_num = tsetup->gobjs_num;
    gcsetup.gobj_size = tsetup->gobj_size;

    gcsetup.xobjs = syTaskmanMalloc(sizeof(XObj) * tsetup->xobjs_num, 0x8);
    gcsetup.xobjs_num = tsetup->xobjs_num;

    gcSetMatrixFuncList(tsetup->matrix_func_list);
    gcsetup.func_eject = tsetup->func_eject;

    gcsetup.aobjs = syTaskmanMalloc(sizeof(AObj) * tsetup->aobjs_num, 0x4);
    gcsetup.aobjs_num = tsetup->aobjs_num;

    gcsetup.mobjs = syTaskmanMalloc(sizeof(MObj) * tsetup->mobjs_num, 0x4);
    gcsetup.mobjs_num = tsetup->mobjs_num;

    gcsetup.dobjs = syTaskmanMalloc(tsetup->dobj_size * tsetup->dobjs_num, 0x8);
    gcsetup.dobjs_num = tsetup->dobjs_num;
    gcsetup.dobj_size = tsetup->dobj_size;

    gcsetup.sobjs = syTaskmanMalloc(tsetup->sobj_size * tsetup->sobjs_num, 0x8);
    gcsetup.sobjs_num = tsetup->sobjs_num;
    gcsetup.sobj_size = tsetup->sobj_size;

    gcsetup.cameras = syTaskmanMalloc(tsetup->cobj_size * tsetup->cobjs_num, 0x8);
    gcsetup.cobjs_num = tsetup->cobjs_num;
    gcsetup.cobj_size = tsetup->cobj_size;

    gcSetupObjman(&gcsetup);
}

void syTaskmanStartTask(SYTaskmanSetup *tsetup)
{
    syTaskmanSetupPools(tsetup);

    sSYTaskmanDefaultFunction.task_update = syTaskmanCommonTaskUpdate;
    sSYTaskmanDefaultFunction.task_draw = syTaskmanCommonTaskDraw;

    syTaskmanLoadScene(&tsetup->scene_setup, tsetup->func_start);
}

/* One turn of syTaskmanRunTask's loop (sys/taskman.c:973-1049).
 *
 * What survives of that loop is its arithmetic, and it is the whole point
 * of it: run the controller read and the scene update every tic, count
 * the tics, and draw on every sSYTaskmanFrameInterval-th one -- unless
 * the update ended the scene, in which case the draw is skipped and the
 * loop is over. That is how the N64 game runs its logic at 60 Hz and
 * draws at 30 when a scene asks it to. Everything else there is
 * osRecvMesg on the scheduler's game-tic queue, the RSP context switch
 * and the stack probes, none of which exists here: the wait for the next
 * tic is the PVR's, inside syTaskmanCommonTaskDraw.
 *
 * The decomp keeps this inline in two near-identical copies of the loop,
 * picked by tfunc->flags & 1 (the difference is one syTaskmanSwitchContext
 * argument, an RSP concern). One body is enough here.
 *
 * Returns TRUE on a tic that drew. syTaskmanRunTask below is the loop
 * around it; a caller that drives its own tics -- the host test -- calls
 * this directly. */
sb32 syTaskmanRunFrame(void)
{
    OSTime start = osGetTime();
    sb32 drew = FALSE;

    gSYTaskmanAlive++;

#ifndef FT_HOSTTEST
    /* drain whatever the
     * audio thread queued since last frame and print it for real,
     * here, on the main thread -- dbglog()/printf() is not safe to
     * call from more than one KOS thread on this target (see fgm.c's
     * note_missing/fgm_defer_dbglog). Every frame, whatever the scene,
     * so a queued line is never held back for more than a frame. */
    fgm_defer_pump();
#endif


    sSYTaskmanDefaultFunction.task_update(&sSYTaskmanDefaultFunction);

    dSYTaskmanUpdateCount++;
    sSYSchedulerTicCount++;
    dSYTaskmanUpdateTimeDelta = (u32)(osGetTime() - start);

    if ((syTaskmanCheckBreakLoop() == FALSE) &&
        (dSYTaskmanUpdateCount % sSYTaskmanFrameInterval == 0))
    {
        start = osGetTime();

        sSYTaskmanDefaultFunction.task_draw(&sSYTaskmanDefaultFunction);

        dSYTaskmanFrameCount++;
        dSYTaskmanFrameTimeDelta = (u32)(osGetTime() - start);

        drew = TRUE;
    }
    /* The port's frame boundary (taskman.h syTaskmanSetFrameEndHook):
     * after the draw, so the save store's pump never sits in front of
     * one, and on the scene's last tic too, which has none. */
    if (sSYTaskmanFrameEndHook != NULL)
    {
        sSYTaskmanFrameEndHook();
    }
#ifndef FT_HOSTTEST
    /* -DDB_LOAD_CENSUS (loadcensus.h): the node's first drawn frame ends
     * its load, and its tics are when its peak is sampled */
    lc_frame(drew);
#endif
    return drew;
}

/* sys/taskman.c:946-1049 syTaskmanRunTask, the scene's frame loop: run
 * tics until the scene says it is finished, then return to whoever
 * started the scene -- the scene manager, which loads the next one.
 *
 * Both of the decomp's break checks are in syTaskmanRunFrame above, so
 * what is left here is the while (TRUE) itself and the one line before
 * it. Everything the decomp does around the loop -- draining three
 * message queues, resetting the framebuffer id and the task id, zeroing
 * D_80046638 -- belongs to the RSP and the scheduler.
 *
 * The save flush is the frame-end hook at the bottom of
 * syTaskmanRunFrame (taskman.h), so the oracles that link this file need
 * no save store. */
void syTaskmanRunTask(void)
{
    /* sys/taskman.c:963: the status is cleared where the run loop starts,
     * so a scene that ended does not end the next one on its first tic. */
    sSYTaskmanStatus = nSYTaskmanStatusDefault;

    while (TRUE)
    {
        syTaskmanRunFrame();

        if (syTaskmanCheckBreakLoop() != FALSE)
        {
            break;
        }
    }
}
