/* mpshim.c -- the runtime side of src/dc/decomp/: every external symbol
 * ssb-decomp-re/src/mp/mpcollision.c and mpprocess.c reach for outside
 * the mp module, so the two files link into the port unmodified.
 *
 * Each entry names the game routine it stands in for and how it differs.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <mp/map.h>
#include <gr/ground.h>
#include <sc/scene.h>
#include <reloc_data.h>

#ifndef FT_HOSTTEST
#include <kos.h>
#endif

/* sys/debug.c syDebugPrintf: the game's osSyncPrintf. Goes to the serial
 * log on target, stderr on the host. */
void syDebugPrintf(const char *fmt, ...)
{
#if defined(SSB_RELEASE) && !defined(FT_HOSTTEST)
    /* a release build says nothing (the Makefile's RELEASE=1) */
    (void)fmt;
    return;
#else
    va_list ap;
    char buf[256];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
#ifdef FT_HOSTTEST
    fputs(buf, stderr);
#else
    dbglog(DBG_CRITICAL, "%s", buf);
#endif
#endif
}

/* sys/vector.c is compiled unmodified; syVectorNorm3D is the decomp's. */

/* sys/audio.c syAudioPlayBGM, reached from mpCollisionSetPlayBGM
 * (mpcollision.c:4013-4020). On target that is src/dc/bgm.c, which
 * streams the pre-rendered track; the host test has no audio and no
 * stage, so it gets a stub that says the id it was asked for. */
#ifdef FT_HOSTTEST
/* the last track asked for, which is what the results screen's test
 * reads: the win jingle at tic 120 and the results theme after it */
s32 gHostLastBGM = -1;

s32 syAudioPlayBGM(s32 sngplayer, u32 bgm)
{
    (void)sngplayer;
    gHostLastBGM = (s32)bgm;
    return (s32)bgm;
}
#endif

/* The four gc*Anim* entry points mpCollisionPlayYakumonoAnim
 * (mpcollision.c:3684-3776) runs over the collision layer's DObjs are
 * the decomp's, from sys/objanim.c. The port still does not animate the collision layer (every
 * yakumono is static) and still calls mpCollisionAdvanceUpdateTic
 * (mpcollision.c:3778-3781) rather than the player, so nothing reaches
 * them yet; what changed is that when something does, it will be the
 * game's code that runs.
 */

/* lb/lbreloc.c: the ROM file loader behind mpCollisionInitGroundData
 * (mpcollision.c:3961-4011). Never called -- the port's geometry is
 * already in RAM (mpCollisionLoadGeometry) -- so these only satisfy
 * the link. Calling one is a bug; they say so. */
size_t lbRelocGetFileSize(u32 id)
{
    syDebugPrintf("mpshim: lbRelocGetFileSize(%u) reached\n", (unsigned)id);
    abort();
}

void *lbRelocGetExternHeapFile(u32 id, void *heap)
{
    (void)heap;
    syDebugPrintf("mpshim: lbRelocGetExternHeapFile(%u) reached\n",
                  (unsigned)id);
    abort();
}

/* gSCManagerBattleState, gSCManagerBackupData and
 * scManagerRunPrintGObjStatus belong to the scene manager and they live in it now
 * (src/dc/scmanager.c), which is a real module rather than three
 * symbols the collision code needed to link. */

/* reloc_data.us.h: the map file ids and header offsets whose addresses
 * fill dMPCollisionGroundFileInfos (mpcollision.c:45-88). Zero: the
 * table is never indexed.
 *
 * `int`, not intptr_t: the generated declarations these
 * satisfy are the decomp's own `extern int`, and the definition has to
 * agree with them. The game gives these symbols their values in the link
 * (their address is the file id); the port does not yet, and this is where
 * that would change. */
#define MPSHIM_DEFINE(name) \
    int llGR##name##MapFileID; \
    int llGR##name##MapMapHeader;
MPSHIM_GROUND_FILES(MPSHIM_DEFINE)
#undef MPSHIM_DEFINE
