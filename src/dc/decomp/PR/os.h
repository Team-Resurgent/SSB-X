/* Shadows ssb-decomp-re/include/PR/os.h, the libultra OS header, for the
 * sys/ files the port compiles out of the decomp (sys/objman.c and
 * sys/taskman.c).
 *
 * This is the boundary the whole shim tree exists to sit on: libultra's
 * OS is the one part of the N64 the port replaces outright, so rather
 * than let 1051 lines of thread/PI/SI/VI/cache declarations through, this
 * header carries the twelve names the object system actually reaches for
 * and nothing else. Their Dreamcast bodies are in src/dc/sys/osshim.c.
 *
 * It also settles a collision the real header causes on this toolchain:
 * PR/os.h:1026-1028 declares bcopy/bcmp/bzero with `int` lengths, newlib's
 * <strings.h> declares them with size_t, and any translation unit that
 * reaches both fails to compile. Neither the decomp's sys/ files nor the
 * port call them, so they are simply absent here.
 *
 * The struct shapes are the decomp's, at its own names and member order,
 * except OSThread -- nothing in the ported files reads a member of it, and
 * a Dreamcast thread has nothing in common with a MIPS thread context, so
 * it is opaque. sys/objtypes.h:138-144 embeds one by value in GObjThread,
 * which is why it needs a size at all.
 */
#ifndef _OS_H_
#define _OS_H_

#include <PR/ultratypes.h>

/* os.h:48-49 */
typedef s32 OSPri;
typedef s32 OSId;

/* os.h:80-102 OSThread_s. DIVERGES: opaque. The game's object system
 * uses a thread as a coroutine -- gcRunGObjProcess resumes it with
 * osStartThread and blocks on gGCMesgQueue until it yields with
 * osSendMesg + osStopThread(NULL) (objman.c:2161-2162,
 * objhelper.c:141-142) -- and that is what src/dc/sysshim.c implements
 * behind these four calls: a ucontext coroutine on the host, a KOS
 * thread handshaking on two semaphores on the target. `impl` is that
 * state. `stack` is the game's own MIPS stack from the GObjStack pool,
 * kept because gcSleepCurrentGObjThread reads its guard word
 * (objhelper.c:135); the coroutine runs on a stack of its own. */
typedef struct OSThread_s
{
    void *impl;
    OSId id;
    OSPri priority;
    void (*entry)(void *);
    void *arg;
    void *stack;
} OSThread;

/* os.h:39 */
typedef u64 OSTime;

/* os.h:100 */
typedef void *OSMesg;

/* os.h:106-116, member for member */
typedef struct OSMesgQueue_s
{
    OSThread *mtqueue;
    OSThread *fullqueue;
    s32 validCount;
    s32 first;
    s32 msgCount;
    OSMesg *msg;
} OSMesgQueue;

/* os.h:425-426 */
#define OS_MESG_NOBLOCK 0
#define OS_MESG_BLOCK   1

/* os.h:571-575. The retail console is not _HW_VERSION_1, so four. This is
 * the array bound on gSYControllerDevices (sys/controller.h:118) and the
 * loop bound the port's own src/dc/input.c walks. */
#define MAXCONTROLLERS 4

/* os.h:611-640, Nintendo's official button names, by value. The CONT_*
 * spellings they alias in libultra are deliberately absent: KOS's
 * dc/maple/controller.h defines CONT_A and friends with other values,
 * and src/dc/input.c reaches both headers (its N64_* names are these
 * same values under a prefix). ft/ftmain.c reads the controller by
 * these names. */
#define A_BUTTON     0x8000
#define B_BUTTON     0x4000
#define Z_TRIG       0x2000
#define START_BUTTON 0x1000
#define U_JPAD       0x0800
#define D_JPAD       0x0400
#define L_JPAD       0x0200
#define R_JPAD       0x0100
#define L_TRIG       0x0020
#define R_TRIG       0x0010
#define U_CBUTTONS   0x0008
#define D_CBUTTONS   0x0004
#define L_CBUTTONS   0x0002
#define R_CBUTTONS   0x0001

/* os.h:410-417, the two the object system passes as thread priorities */
#define OS_PRIORITY_APPMAX 127
#define OS_PRIORITY_IDLE   0

/* os.h:1101 OSPiHandle. DIVERGES: opaque, and for one reason only --
 * sys/dma.h:32,35,39 name it in three declarations, so without it that
 * header cannot be included at all and src/dc/dma.c cannot check its
 * syDmaReadRom against the game's. Nothing in the port holds one: the
 * PI is the cartridge bus and there is no cartridge. */
typedef struct OSPiHandle_s OSPiHandle;

/* os.h:936 osGetTime, which sys/utils.c seeds its time-based randoms
 * from. Microseconds since boot here; the N64's is CPU counter ticks,
 * and nothing that reads it cares which. */
extern OSTime osGetTime(void);

/* os.h:842-856, 876, 1033 -- the whole surface sys/objman.c uses */
extern void osCreateThread(OSThread *t, OSId id, void (*entry)(void *),
                           void *arg, void *sp, OSPri pri);
extern void osDestroyThread(OSThread *t);
extern void osStartThread(OSThread *t);
extern void osStopThread(OSThread *t);
extern void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
extern s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
extern s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);
extern void osWritebackDCacheAll(void);
extern u32 osGetCount(void);

/* os.h: the reset kind, 0 cold / 1 warm. src/dc/sysshim.c defines it as
 * a cold boot. */
extern s32 osResetType;

#endif /* _OS_H_ */
