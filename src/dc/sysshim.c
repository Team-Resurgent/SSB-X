/* sysshim.c -- the N64 system services the game's object system reaches
 * for, on the Dreamcast.
 *
 * src/dc/mpshim.c does this job for mp/mpcollision.c and mpprocess.c.
 * This file does it for sys/objman.c, sys/objhelper.c and sys/objscript.c,
 * which are compiled unmodified out of the decomp and between them need
 * exactly nine names from outside the object system: seven libultra OS
 * calls, one RDP viewport helper, and the video resolution behind it.
 *
 * Each entry names the game routine it stands in for and how it differs.
 */
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/obj.h>
#include <sys/debug.h>
#include <sys/rdp.h>
#include <sys/video.h>
#include <sys/audio.h>
#include <gm/gmsound.h>

#ifndef FT_HOSTTEST
#include <kos.h>
#include "stackguard.h"
#endif

/* ---- sys/video.c:18,21 ------------------------------------------------
 *
 * The N64 sets these from the scene's SYVideoSetup, which is
 * SYVIDEO_SETUP_DEFAULT() in every scene: 320x240 (sys/video.h:44-45).
 * They stay the game's numbers rather than the Dreamcast's 640x480,
 * because what reads them is game state -- syRdpSetDefaultViewport writes
 * a CObj's viewport, and the fighter interface positions itself in those
 * units (if/ifcommon.c:1410). The PVR back end scales to the framebuffer
 * at the end, where the output is a pixel.
 */
s32 gSYVideoResWidth = 320;
s32 gSYVideoResHeight = 240;

/* How many framebuffer pixels one game pixel covers: 2 at the game's
 * usual 320x240. The staff roll is the one scene that asks for 640x480
 * (sc/sccommon/scstaffroll.c dSCStaffrollVideoSetup), and every 2D
 * position and 3D viewport in it is written in those pixels -- at 2 its
 * crosshair and textbox landed off the right of the screen. */
f32 gDCScreenScale = 2.0F;

/* What syVideoInit's resolution half does, for the one scene that
 * changes it; the rest of syVideoInit is the PVR's business. */
void dcVideoSetResolution(s32 width, s32 height)
{
    gSYVideoResWidth = width;
    gSYVideoResHeight = height;
    gDCScreenScale = 640.0F / (f32)width;
}

/* ---- sys/video.c:33-42, 110-119 syVideoSetCenterOffsets --------------
 *
 * How far in from the VI's centre the visible picture starts, on each of
 * the four edges. The options screen's SCREEN ADJUST writes them
 * (mn/mnoption/mnscreenadjust.c:244) and lb/lbbackup.c restores them from
 * the save data at boot; syVideoInitViTask hands them to the VI task.
 *
 * DIVERGES: nothing reads them here. The N64's video interface can shift
 * and scale the analogue picture inside the TV's overscan, which is what
 * the option is for; the PVR's output rectangle is fixed by the video
 * mode KOS sets. So the port keeps the state -- it is save data, and it
 * has to survive a write and a read -- and the picture does not move.
 * Both defaults are 0 (sc/scmanager.c:135), so today nothing would. */
s16 gSYVideoOffsetLeft;
s16 gSYVideoOffsetRight;
s16 gSYVideoOffsetTop;
s16 gSYVideoOffsetBottom;

void syVideoSetCenterOffsets(s16 left, s16 right, s16 top, s16 bottom)
{
    gSYVideoOffsetLeft = left;
    gSYVideoOffsetRight = right;
    gSYVideoOffsetTop = top;
    gSYVideoOffsetBottom = bottom;
}

/* ---- sys/dma.c:148,154 syDmaReadSram / syDmaWriteSram ----------------
 *
 * The cartridge's battery-backed SRAM, the whole of the game's storage.
 * Both bodies live in src/dc/vmusave.c, because they have a memory
 * card behind them and a file's worth of argument with it. They are
 * exactly two N64 services with two Dreamcast ones in
 * their place, and lb/lbbackup.c above them is still compiled
 * unmodified. */

/* ---- sys/main.c:101,133-141 gSYMainImemOK ----------------------------
 *
 * syMainSetImemStatus reads the RSP's instruction memory at boot and
 * remembers whether it holds the number the game's own boot code left
 * there. It is one of the anti-tamper switches: mn/mnvsmode/mnvsmode.c
 * mnVSModeFuncStart, on a save older than 21 boots, turns knockback
 * random for good if the answer was no. There is no RSP here and the
 * boot code is the game's own, so the answer is yes. */
ub8 gSYMainImemOK = TRUE;

/* sys/rdp.c:81-86, verbatim. gcAddCameraForGObj (objman.c:1636) calls it
 * on every new camera, so it runs long before anything draws. */
void syRdpSetDefaultViewport(Vp *vp)
{
    vp->vp.vscale[0] = vp->vp.vtrans[0] = gSYVideoResWidth * 2;
    vp->vp.vscale[1] = vp->vp.vtrans[1] = gSYVideoResHeight * 2;
    vp->vp.vscale[2] = vp->vp.vtrans[2] = G_MAXZ / 2;
}

/* sys/rdp.c:68-79, verbatim. A camera's viewport as a screen rectangle
 * in the game's 320x240; the sprite camera (lb/lbcommon.c
 * lbCommonDrawSprite) reads it back as its scissor. */
void syRdpSetViewport(Vp *viewport, f32 ulx, f32 uly, f32 lrx, f32 lry)
{
    f32 h = (ulx + lrx) / 2.0F;
    f32 v = (uly + lry) / 2.0F;

    viewport->vp.vscale[0] = ((s32) ((lrx - h) * 4.0F)) & 0xFFFF;
    viewport->vp.vscale[1] = ((s32) ((lry - v) * 4.0F)) & 0xFFFF;
    viewport->vp.vtrans[0] = ((s32) (h * 4.0F)) & 0xFFFF;
    viewport->vp.vtrans[1] = ((s32) (v * 4.0F)) & 0xFFFF;

    viewport->vp.vscale[2] = viewport->vp.vtrans[2] = G_MAXZ / 2;
}

/* libultra's osResetType: 0 for a cold boot, 1 for the reset button.
 * The title screen reads it to decide whether a press may skip its
 * animation (mn/mncommon/mntitle.c:531). The Dreamcast has no warm
 * reset the program sees, so this is a cold boot forever. */
s32 osResetType = 0;

/* ---- libultra libm and gu, and where they went -----------------------
 *
 * __sinf and __cosf are not here: libultra's are a range-reduced
 * polynomial that agrees with a correctly rounded sinf only in its last
 * bits, and 24 of the particle bank's 119 scripts came out differently
 * on the Dreamcast and on the build host with the C library's, every one
 * calling this pair. They are ssb-decomp-re/src/libultra/gu's sinf.c and
 * cosf.c, written for the port at build time by
 * tools/export/ssb_trigexport.py.
 *
 * guMtxIdentF, guMtxCatF and guNormalize are not here either: they are
 * libultra/gu/mtxutil.c, mtxcatf.c and normalize.c, which compile
 * unmodified and are direct dependencies in the Makefile.
 */

/* ---- libultra OS ------------------------------------------------------
 *
 * PR/os.h's shim (src/dc/decomp/PR/os.h) declares these; here are their
 * bodies. The object system reaches them from two places only:
 *
 *   - gcSetupObjman (objman.c:2425) creates gGCMesgQueue unconditionally,
 *     so the queue calls have to work.
 *   - the GObjThread coroutine path (objman.c:812/875/918/2161,
 *     objhelper.c:141-142) creates, runs and sleeps threads. The scene
 *     setups' zero GObjThread counts (scvsbattle.c:47-49, mnvsmode.c:
 *     1676-1678) mean "allocate on demand", not "none" --
 *     gcGetGObjThread takes one off the scene heap when the pool is
 *     empty (objman.c:117-120) -- and the battle's entry sequence,
 *     countdown and announcements are threads (if/ifcommon.c
 *     ifCommonEntryAllThread, ifCommonEntryFocusThread,
 *     ifCommonCountdownThread, ifCommonAnnounceThread), as are the
 *     fades. The switch is below, under "threads as coroutines".
 */

/* os.h:842-856. The message queue is the caller's ring buffer; these are
 * libultra's semantics on it, minus the thread queues, which a
 * single-threaded port has no use for. */
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count)
{
    mq->mtqueue = NULL;
    mq->fullqueue = NULL;
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msg;
}

/* DIVERGES: a blocking send on a full queue, or a blocking receive on an
 * empty one, waits for another thread. There isn't one, so it would be a
 * silent hang; say which call it was and stop. Nothing on a ported path
 * reaches either. */
static void osshim_deadlock(const char *what)
{
    syDebugPrintf("osshim: %s would block with no other thread\n", what);
    abort();
}

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag)
{
    if (mq->validCount >= mq->msgCount)
    {
        if (flag == OS_MESG_NOBLOCK)
            return -1;
        osshim_deadlock("osSendMesg");
    }
    mq->msg[(mq->first + mq->validCount) % mq->msgCount] = msg;
    mq->validCount++;
    return 0;
}

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag)
{
    if (mq->validCount == 0)
    {
        if (flag == OS_MESG_NOBLOCK)
            return -1;
        osshim_deadlock("osRecvMesg");
    }
    if (msg != NULL)
        *msg = mq->msg[mq->first];

    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;
    return 0;
}

/* ---- os.h:876, 1033: threads as coroutines ---------------------------
 *
 * How the object system uses a thread (objman.c:804-826 creates it,
 * 2161-2162 runs it, objhelper.c:132-145 sleeps it, objman.c:918
 * destroys it):
 *
 *   main:    osStartThread(t)            -- resume t; libultra switches
 *                                           at once because t's priority
 *                                           (51) beats the main thread's
 *            osRecvMesg(&q, BLOCK)       -- by now t has sent, so this
 *                                           only takes the message
 *   thread:  osSendMesg(&q, 1, NOBLOCK)  -- "I am done for this tic"
 *            osStopThread(NULL)          -- suspend self: back to main
 *   main:    osDestroyThread(t)          -- always from main, on a
 *                                           suspended t: gcEjectGObj from
 *                                           inside t only marks the eject
 *                                           and sleeps (objman.c:900-908),
 *                                           main does the destroying
 *
 * So a thread is a coroutine that main resumes once per tic, and that is
 * what this is. On the host it is a ucontext with its own stack; on the
 * target a KOS thread that waits on `run` and signals `yield`, with main
 * doing the opposite, so exactly one of the two is ever runnable and
 * KOS's preemption has nothing to choose between. A thread procedure in
 * the game never returns -- each ends with gcEjectGObj(NULL) and a sleep
 * -- but one that did would simply end the coroutine.
 *
 * The coroutine's stack is its own (malloc on the host, KOS's thread
 * stack on the target, 16K: the MIPS stacks the game's pool hands out are
 * 1.5K, sized for libultra frames, and a status change from inside a
 * thread runs the animation parsers). What the game allocated for the
 * thread's MIPS stack stays on OSThread.stack for the guard-word check in
 * gcSleepCurrentGObjThread. */

#define OSSHIM_STACK_SIZE (16 * 1024)

#ifdef FT_HOSTTEST
#include <ucontext.h>

typedef struct OSShimCoroutine
{
    ucontext_t ctx;
    void *stack;
    sb32 is_ended;
} OSShimCoroutine;

static ucontext_t sOSShimMainCtx;
#else
#include <kos/thread.h>
#include <kos/sem.h>

typedef struct OSShimCoroutine
{
    kthread_t *thd;
    semaphore_t run;            /* main -> thread: your tic */
    semaphore_t yield;          /* thread -> main: done for now */
    sb32 is_ended;
    sb32 is_destroy;            /* main wants the thread gone: exit on wake */
#ifdef DB_STACK_GUARD
    void *stack;                /* src/dc/stackguard.h's, not KOS's */
#endif
} OSShimCoroutine;
#endif

static OSThread *sOSShimCurrentThread;

static void osshim_threadfail(const char *what)
{
    syDebugPrintf("osshim: %s\n", what);
    abort();
}

#ifdef FT_HOSTTEST
static void osshim_trampoline(void)
{
    OSThread *t = sOSShimCurrentThread;

    t->entry(t->arg);
    ((OSShimCoroutine *)t->impl)->is_ended = TRUE;
    /* uc_link takes it back to main's swapcontext in osStartThread */
}
#else
static void *osshim_trampoline(void *param)
{
    OSThread *t = param;
    OSShimCoroutine *co = t->impl;

    sem_wait(&co->run);
    if (!co->is_destroy)
    {
        t->entry(t->arg);
    }
    co->is_ended = TRUE;
    sem_signal(&co->yield);
    return NULL;
}
#endif

void osCreateThread(OSThread *t, OSId id, void (*entry)(void *), void *arg,
                    void *sp, OSPri pri)
{
    OSShimCoroutine *co = calloc(1, sizeof(*co));

    if (co == NULL)
    {
        osshim_threadfail("osCreateThread: no memory for the coroutine");
    }
    t->impl = co;
    t->id = id;
    t->priority = pri;
    t->entry = entry;
    t->arg = arg;
    t->stack = sp;

#ifdef FT_HOSTTEST
    co->stack = malloc(OSSHIM_STACK_SIZE * 4);
    if (co->stack == NULL)
    {
        osshim_threadfail("osCreateThread: no memory for the stack");
    }
    getcontext(&co->ctx);
    co->ctx.uc_stack.ss_sp = co->stack;
    co->ctx.uc_stack.ss_size = OSSHIM_STACK_SIZE * 4;
    co->ctx.uc_link = &sOSShimMainCtx;
    makecontext(&co->ctx, osshim_trampoline, 0);
#else
    {
        kthread_attr_t attr;

        memset(&attr, 0, sizeof(attr));
        attr.stack_size = OSSHIM_STACK_SIZE;
        attr.label = "GObjThread";
#ifdef DB_STACK_GUARD
        /* -DDB_STACK_GUARD: a guarded, painted stack of the same size,
         * named by the thread's entry procedure (src/dc/stackguard.h) */
        co->stack = stack_guard_alloc(OSSHIM_STACK_SIZE, "GObjThread",
                                      (const void *)entry);
        if (co->stack == NULL)
        {
            osshim_threadfail("osCreateThread: no memory for the stack");
        }
        attr.stack_ptr = co->stack;
#endif
        sem_init(&co->run, 0);
        sem_init(&co->yield, 0);
        co->thd = thd_create_ex(&attr, osshim_trampoline, t);
        if (co->thd == NULL)
        {
            osshim_threadfail("osCreateThread: thd_create_ex failed");
        }
    }
#endif
}

/* Resume `t` until it yields or ends. Only main resumes; a thread that
 * started another would be a scheduling question this has no answer to,
 * and the game never asks it (gcAddGObjProcess creates, gcRunGObjProcess
 * starts). */
void osStartThread(OSThread *t)
{
    OSShimCoroutine *co = t->impl;

    if (sOSShimCurrentThread != NULL)
    {
        osshim_threadfail("osStartThread from inside a thread");
    }
    if (co->is_ended)
    {
        osshim_threadfail("osStartThread on a thread whose entry returned");
    }
    sOSShimCurrentThread = t;
#ifdef FT_HOSTTEST
    swapcontext(&sOSShimMainCtx, &co->ctx);
#else
    sem_signal(&co->run);
    sem_wait(&co->yield);
#ifdef DB_STACK_GUARD
    /* its tic is over: whatever it wrote below its stack is there now */
    stack_guard_check_one(co->stack, "GObj thread yield");
#endif
#endif
    sOSShimCurrentThread = NULL;
}

/* Suspend: NULL or the current thread yields to main. Any other thread is
 * already suspended (only one ever runs), so there is nothing to do --
 * libultra would pull it off the run queue, and it is not on one. */
void osStopThread(OSThread *t)
{
    OSShimCoroutine *co;

    if (t == NULL)
    {
        t = sOSShimCurrentThread;
    }
    if (t == NULL || t != sOSShimCurrentThread)
    {
        return;
    }
    co = t->impl;
#ifdef FT_HOSTTEST
    swapcontext(&co->ctx, &sOSShimMainCtx);
#else
    sem_signal(&co->yield);
    sem_wait(&co->run);
    if (co->is_destroy)
    {
        thd_exit(NULL);
    }
#endif
}

/* From main, on a suspended thread (objman.c:918 gcEjectGObjProcess). */
void osDestroyThread(OSThread *t)
{
    OSShimCoroutine *co = t->impl;

    if (t == sOSShimCurrentThread)
    {
        osshim_threadfail("osDestroyThread on the running thread");
    }
#ifdef FT_HOSTTEST
    free(co->stack);
#else
    if (!co->is_ended)
    {
        /* wake it into osStopThread's exit path, then reap it */
        co->is_destroy = TRUE;
        sem_signal(&co->run);
    }
    thd_join(co->thd, NULL);
    sem_destroy(&co->run);
    sem_destroy(&co->yield);
#ifdef DB_STACK_GUARD
    stack_guard_free(co->stack);    /* KOS does not own it */
#endif
#endif
    free(co);
    t->impl = NULL;
}

/* os.h:936 */
OSTime osGetTime(void)
{
#ifdef FT_HOSTTEST
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (OSTime)ts.tv_sec * 1000000u + (OSTime)(ts.tv_nsec / 1000);
#else
    return timer_us_gettime64();
#endif
}

/* ---- sys/audio.c and libultra/n_audio/n_env.c: the FGM calls ----------
 * The game's sound players and the n_env entry points under them live
 * in src/dc/syaudio.c, the port of sys/audio.c's FGM half over
 * src/dc/fgm.c.
 */
