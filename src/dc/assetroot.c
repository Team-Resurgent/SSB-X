/* assetroot.c -- see assetroot.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assetroot.h"

/* _arch_dreamcast rather than FT_HOSTTEST: the host test is not the only
 * off-target build of this code. tools/check/bgm_check.py compiles bgmbank.c on
 * the host too, and everything off the SH-4 reads out of the game's
 * romdisk directory, which is where those runs are started from. */
#ifdef _arch_dreamcast
#include <kos.h>
#include <kos/mm.h>
#include <arch/stack.h>
#include <malloc.h>

#include "stackguard.h"
#if defined(DB_DISC_TRACE)
#define fs_open dt_fs_open
#define fs_read dt_fs_read
#define fs_seek dt_fs_seek
#define fs_close dt_fs_close
#endif

/* The file the disc carries and the romdisk does not. scripts/make_cdi.sh
 * writes it into the ISO tree it stages, never into the game's own
 * romdisk directory. */
#define ASSET_DISC_ID "/cd/disc.id"
#endif
#include "disctrace.h"
#include "loadcensus.h"

static const char *sAssetRoot;

/* ---------------------------------------------------------------- *
 * The one I/O lock                                                  *
 * ---------------------------------------------------------------- *
 *
 * KOS's /cd is not safe for two threads at once (an fh/cache lock-order
 * deadlock, a directory cache used after unlock, fs_close freeing a
 * handle under a concurrent fs_read), nor is the bundle's one shared
 * handle and position. So every public call below takes this mutex for
 * its whole length and does its work in a *_locked body; nothing under
 * it takes another lock, sleeps on a GObjThread handoff, or runs in an
 * IRQ. It is a NORMAL mutex on purpose: with asserts on, a thread that
 * takes it twice aborts, which is how a nested call would show itself.
 *
 * Lock order: bgm's sLock, then fgm's sLock, then this one (src/dc/
 * sndres.c's upload holds both audio locks while it reads).
 *
 * It is static, and only asset_* takes it. The memory card does not take
 * it: the card is vmufs (src/dc/vmucard.c), which has its own mutex and
 * no file table, so a second-long card write does not stand in front
 * of a scene load.
 *
 * sIOHeld says who holds it, doing what, since when -- what the hang
 * watchdog prints when the game stops inside a read (src/dc/db.c). */
#ifdef _arch_dreamcast
static mutex_t sAssetIOLock = MUTEX_INITIALIZER;
static struct
{
    kthread_t *owner;
    const char *op;
    const char *what;
    uint64_t t0;
} sIOHeld;

/* Calls completed under the lock since boot: the hang watchdog's sign
 * that a stalled game is loading, not stuck. The stress thread's reads
 * (-DDB_IO_STRESS) never stop, so they do not count. */
static volatile uint32_t sIOProgress;
#ifdef DB_IO_STRESS
static kthread_t *sSXThread;
#endif
/* the loader's (reading ahead, below), and what it and its waiters
 * wait on: the queue opening, and a file finishing */
static kthread_t *sPFThread;
static condvar_t sPFWake = COND_INITIALIZER;
static condvar_t sPFDone = COND_INITIALIZER;
#define IO_ON_LOADER() (sPFThread != NULL && thd_get_current() == sPFThread)

static void io_take(const char *op, const char *what)
{
    mutex_lock(&sAssetIOLock);
    sIOHeld.owner = thd_get_current();
    sIOHeld.op = op;
    sIOHeld.what = what;
    sIOHeld.t0 = timer_ms_gettime64();
}

static void io_give(void)
{
#ifdef DB_IO_STRESS
    if (sIOHeld.owner != sSXThread)
#endif
        sIOProgress++;
    sIOHeld.owner = NULL;
    mutex_unlock(&sAssetIOLock);
}

#ifdef DB_PC_ROOT
int asset_io_card_begin(void)
{
    if (dcload_type != DCLOAD_TYPE_SER && dcload_type != DCLOAD_TYPE_IP)
        return 0;
    io_take("card", "memory card");
    return 1;
}

void asset_io_card_end(int took)
{
    if (took)
        io_give();
}
#else
int asset_io_card_begin(void) { return 0; }
void asset_io_card_end(int took) { (void)took; }
#endif

/* cond_wait on the I/O lock, which gives it up while it sleeps: the
 * record goes with it, and comes back when the lock does */
static void io_wait(condvar_t *cv)
{
    const char *op = sIOHeld.op, *what = sIOHeld.what;

    sIOHeld.owner = NULL;
    cond_wait(cv, &sAssetIOLock);
    sIOHeld.owner = thd_get_current();
    sIOHeld.op = op;
    sIOHeld.what = what;
    sIOHeld.t0 = timer_ms_gettime64();
}

/* Read from the watchdog's thread without the lock -- it must not wait
 * on the call it is reporting. The fields can be torn while the lock
 * changes hands, but the watchdog only prints them after seconds of
 * no progress, when nothing is changing them. */
void asset_io_state(AssetIOState *st)
{
    kthread_t *owner = sIOHeld.owner;

    st->calls = sIOProgress;
    st->tid = (owner != NULL) ? (int)owner->tid : -1;
    st->op = sIOHeld.op;
    st->what = sIOHeld.what;
    st->ms = (owner != NULL)
        ? (uint32_t)(timer_ms_gettime64() - sIOHeld.t0) : 0;
}
#else
/* off the SH-4 there is one thread, and the loader's work is done on it
 * inside asset_prefetch_go */
#define io_take(op, what) ((void)0)
#define io_give() ((void)0)
static int sPFOnHost;
#define IO_ON_LOADER() (sPFOnHost)
#endif

static int bundle_mount_locked(const char *path);

#ifdef _arch_dreamcast
static char sAssetStamp[64];

#ifdef DB_PC_ROOT
/* TRUE when a dcload link (serial or BBA) is up and /pc is live. KOS
 * mounts it itself (fs_dcload.c) whenever dc-tool wrote its magic value
 * at upload -- this is a runtime check, not a compile-time one, so a
 * DB_PC_ROOT build that boots from a real disc, or runs with no cable,
 * just falls through to the /cd or /rd probe
 * below and fails loudly there instead (src/dc/db.h). */
static int asset_probe_pc(void)
{
    switch (dcload_type)
    {
        case DCLOAD_TYPE_SER:
            strcpy(sAssetStamp, "dcload-serial");
            return 1;
        case DCLOAD_TYPE_IP:
            strcpy(sAssetStamp, "dcload-ip");
            return 1;
        default:
            return 0;
    }
}
#endif

/* TRUE when this build booted from a disc, with the stamp read out of it.
 * fs_open on a romdisk-only build simply fails: KOS mounts /cd when there
 * is a disc in the drive and the path does not resolve when there is not. */
static int asset_probe_disc(void)
{
    file_t fd = fs_open(ASSET_DISC_ID, O_RDONLY);
    ssize_t n;
    char *nl;

    if (fd == FILEHND_INVALID)
    {
        return 0;
    }
    n = fs_read(fd, sAssetStamp, sizeof(sAssetStamp) - 1);
    fs_close(fd);
    if (n < 0)
    {
        n = 0;
    }
    sAssetStamp[n] = '\0';

    nl = strchr(sAssetStamp, '\n');
    if (nl != NULL)
    {
        *nl = '\0';
    }
    return 1;
}
#endif

static const char *root_locked(void)
{
    if (sAssetRoot == NULL)
    {
#ifdef _arch_dreamcast
#ifdef DB_PC_ROOT
        if (asset_probe_pc())
        {
            sAssetRoot = "/pc";
            dbglog(DBG_INFO, "assets: /pc -- %s\n", sAssetStamp);
        }
        else
#endif
        if (asset_probe_disc())
        {
            sAssetRoot = "/cd";
            dbglog(DBG_INFO, "assets: /cd -- %s\n", sAssetStamp);
        }
        else
        {
            sAssetRoot = "/rd";
            dbglog(DBG_INFO, "assets: /rd -- the romdisk in the ELF\n");
        }
#else
        /* the game's own romdisk directory, which is where the host
         * suite and the oracles run from */
        sAssetRoot = "romdisk";
#endif
        /* the disc's bundle, when this medium has one (assetroot.h);
         * the romdisk and the host tree do not, and that is not an
         * error */
        {
            char bnd[ASSET_PATH_MAX];

            snprintf(bnd, sizeof(bnd), "%s/models.bnd", sAssetRoot);
            bundle_mount_locked(bnd);
        }
    }
    return sAssetRoot;
}

const char *asset_root(void)
{
    const char *root;

    /* set once and never again, so the answer needs no lock */
    if (sAssetRoot != NULL)
        return sAssetRoot;
    io_take("root", NULL);
    root = root_locked();
    io_give();
    return root;
}

static const char *path_into(char *buf, size_t n, const char *name,
                             const char *(*root)(void))
{
    if (buf == NULL || n == 0)
    {
        return NULL;
    }
    if (name == NULL)
    {
        buf[0] = '\0';
        return buf;
    }
    /* ASSET_PATH_MAX holds every name the game opens. A longer one is cut
     * short by snprintf; the result is tested only so that GCC, which
     * cannot see the names' lengths, knows the cut is expected
     * (-Wformat-truncation). */
    if (strchr(name, '/') != NULL)
    {
        if (snprintf(buf, n, "%s", name) >= (int)n)
            buf[n - 1] = '\0';
    }
    else
    {
        if (snprintf(buf, n, "%s/%s", root(), name) >= (int)n)
            buf[n - 1] = '\0';
    }
    return buf;
}

const char *asset_path(char *buf, size_t n, const char *name)
{
    return path_into(buf, n, name, asset_root);
}

/* ---------------------------------------------------------------- *
 * Reading one, and how long it took *
 * ---------------------------------------------------------------- */

static uint32_t sIOFiles, sIOBytes, sIOUs;              /* this window */
static uint32_t sIOBundled, sIOBundledAll;              /* bundle opens */
static uint32_t sIOFromRAM, sIOFromRAMAll;              /* held opens */
static const char *sIOExpectNone;                       /* asset_io_expect_none */
static int sIOLoadsDone;                                /* asset_prefetch_go */
static uint32_t sIOFilesAll, sIOBytesAll, sIOUsAll;     /* since boot */

#if defined(DB_IO_STRESS) && defined(_arch_dreamcast)
/* -DDB_IO_STRESS's reader (the end of this file, sSXThread above), whose
 * opens are not the game's and so are not "opened during" anything */
#define IO_EXPECTING() (sIOExpectNone != NULL && !IO_ON_LOADER() && \
                        thd_get_current() != sSXThread)
#else
/* nor are the loader's, which the game did not ask for either */
#define IO_EXPECTING() (sIOExpectNone != NULL && !IO_ON_LOADER())
#endif

static uint64_t asset_now_us(void)
{
#ifdef _arch_dreamcast
    return timer_us_gettime64();
#else
    /* No clock off target, and nothing to time: the host suite reads out
     * of the build directory. */
    return 0;
#endif
}

/* Only the time inside fs_open/fs_read/fs_close is the medium's. Time
 * a loader spends between one read and the next (bgm_bank_load's
 * spu_memload_sq, say) is the loader's work, not the drive's. */
static void asset_io_charge(uint64_t t0)
{
    uint32_t us = (uint32_t)(asset_now_us() - t0);

    sIOUs += us;
    sIOUsAll += us;
}

#if defined(DB_IO_SLOW) && defined(_arch_dreamcast)
#ifndef DB_IO_SLOW_SEEK
#define DB_IO_SLOW_SEEK 0
#endif
/* -DDB_IO_SLOW (src/dc/db.h): make a trip to the medium that began at t0
 * take as long as `bytes` at DB_IO_SLOW KB/s, plus a seek's
 * DB_IO_SLOW_SEEK ms when it moved the head. Slept under the I/O lock,
 * because a drive that is busy is busy for every thread. */
static void io_slow(uint64_t t0, long bytes, int seeks)
{
    uint64_t want = (uint64_t)(bytes > 0 ? bytes : 0) * 1000000u /
                    ((uint64_t)(DB_IO_SLOW) * 1024u) +
                    (uint64_t)seeks * (DB_IO_SLOW_SEEK) * 1000u;
    uint64_t took = timer_us_gettime64() - t0;

    if (want > took)
        thd_sleep((int)((want - took + 999u) / 1000u));
}
#define IO_SLOW_SLEEP(t0, bytes, seeks) io_slow((t0), (bytes), (seeks))
#else
#define IO_SLOW_SLEEP(t0, bytes, seeks) ((void)0)
#endif

#if defined(DB_IO_IRQOFF) && defined(_arch_dreamcast)
#ifndef DB_IO_IRQOFF_SEEK
#define DB_IO_IRQOFF_SEEK 15
#endif
/* -DDB_IO_IRQOFF (src/dc/db.h): the dev cable's reads. Under
 * dcload every fs_open and fs_read of /pc is a syscall into dcload's own
 * code, which runs with interrupts masked for as long as the bytes take
 * over the cable -- about 250 KB/s on dcload-serial (the hardware logs'
 * io: lines, 150-640 KB/s a scene), so the Room's 3.7 MB is 14 s with
 * no timer, no vblank, no PVR list/render-done interrupt and no thread
 * switch, taken in one piece per read. Both hardware crashes were
 * dcload builds. After each trip to the medium this spins with
 * interrupts off for what the cable would have taken: bytes at
 * DB_IO_IRQOFF KB/s plus DB_IO_IRQOFF_SEEK ms per open or move.
 *
 * The clock: with interrupts off, TMU2's once-a-second interrupt never
 * runs, so the underflow flag covers one wrap and a second one reads as
 * time going back a second (external/kos kernel/arch/dreamcast/kernel/
 * timer.c) -- counted here as a wrap. KOS's own clock comes back short
 * by those seconds afterwards, exactly as it does under dcload. */
static uint64_t sIOIrqOffUs, sIOIrqOffLongest;
static uint32_t sIOIrqOffTrips;

static void io_irqoff(long bytes, int seeks)
{
    uint64_t want = (uint64_t)(bytes > 0 ? bytes : 0) * 1000000u /
                    ((uint64_t)(DB_IO_IRQOFF) * 1024u) +
                    (uint64_t)seeks * (DB_IO_IRQOFF_SEEK) * 1000u;
    uint64_t done = 0, prev, now;
    int old;

    if (want == 0)
        return;
    old = irq_disable();
    prev = timer_us_gettime64();
    while (done < want)
    {
        now = timer_us_gettime64();
        done += (now >= prev) ? now - prev : now + 1000000u - prev;
        prev = now;
    }
    irq_restore(old);
    sIOIrqOffUs += want;
    sIOIrqOffTrips++;
    if (want > sIOIrqOffLongest)
        sIOIrqOffLongest = want;
}
#define IO_IRQOFF(bytes, seeks) io_irqoff((bytes), (seeks))
#else
#define IO_IRQOFF(bytes, seeks) ((void)0)
#endif

/* After each trip to the medium: the slow disc's sleep, then the dev
 * cable's masked spin, each only when its flag is on. */
#define IO_SLOW(t0, bytes, seeks) \
    do { IO_SLOW_SLEEP(t0, bytes, seeks); IO_IRQOFF(bytes, seeks); } while (0)

/* ---- models.bnd (assetroot.h) ---- */

/* tools/export/ssb_bundle.py's layout: a 16-byte header, then 32-byte
 * entries */
#define BND_MAGIC       "BND2"
#define BND_HEADER      16
#define BND_ENTRY       40
#define BND_NAME        32

typedef struct AssetBundleEntry
{
    char name[BND_NAME];
    uint32_t off, size;
} AssetBundleEntry;

static struct
{
#ifdef _arch_dreamcast
    file_t fd;
#else
    FILE *fp;
#endif
    AssetBundleEntry *index;
    int count;
    long size;
    long pos;           /* where the ONE handle is: a read that follows on
                         * from the last one does not seek */
} sAssetBundle;

static uint32_t bnd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* The handle's own reads and seeks, one per medium. */
static long bnd_raw_read(void *buf, long n);
static int bnd_raw_seek(long off)
{
#ifdef _arch_dreamcast
#if defined(DB_IO_SLOW)
    uint64_t t0 = timer_us_gettime64();
#endif

    if (fs_seek(sAssetBundle.fd, off, SEEK_SET) < 0)
        return -1;
    IO_SLOW(t0, 0, 1);
#else
    if (fseek(sAssetBundle.fp, off, SEEK_SET) != 0)
        return -1;
#endif
    sAssetBundle.pos = off;
    return 0;
}

static void bundle_unmount_locked(void)
{
    if (sAssetBundle.index == NULL)
    {
        return;
    }
#ifdef _arch_dreamcast
    fs_close(sAssetBundle.fd);
#else
    fclose(sAssetBundle.fp);
#endif
    free(sAssetBundle.index);
    memset(&sAssetBundle, 0, sizeof(sAssetBundle));
}

int asset_bundle_count(void)
{
    return sAssetBundle.count;
}

static int bundle_mount_locked(const char *path)
{
    uint8_t hd[BND_HEADER];
    uint8_t *raw = NULL;
    uint32_t count, index_off;
    uint32_t i;

    bundle_unmount_locked();
#ifdef _arch_dreamcast
    sAssetBundle.fd = fs_open(path, O_RDONLY);
    if (sAssetBundle.fd == FILEHND_INVALID)
    {
        return -1;
    }
    sAssetBundle.size = (long)fs_total(sAssetBundle.fd);
#else
    sAssetBundle.fp = fopen(path, "rb");
    if (sAssetBundle.fp == NULL)
    {
        return -1;
    }
    fseek(sAssetBundle.fp, 0, SEEK_END);
    sAssetBundle.size = ftell(sAssetBundle.fp);
    fseek(sAssetBundle.fp, 0, SEEK_SET);
#endif
    sAssetBundle.pos = 0;
    if (bnd_raw_read(hd, BND_HEADER) != BND_HEADER ||
        memcmp(hd, BND_MAGIC, 4) != 0)
    {
        goto bad;
    }
    count = bnd_u32(hd + 4);
    index_off = bnd_u32(hd + 8);
    if (count == 0 || count > 4096 ||
        index_off + (long)count * BND_ENTRY > (uint32_t)sAssetBundle.size)
    {
        goto bad;
    }
    sAssetBundle.index = malloc(sizeof(AssetBundleEntry) * count);
    raw = malloc((size_t)count * BND_ENTRY);
    if (sAssetBundle.index == NULL || raw == NULL ||
        bnd_raw_seek((long)index_off) < 0 ||
        bnd_raw_read(raw, (long)count * BND_ENTRY) != (long)count * BND_ENTRY)
    {
        goto bad;
    }
    for (i = 0; i < count; i++)
    {
        AssetBundleEntry *e = &sAssetBundle.index[i];
        const uint8_t *r = raw + i * BND_ENTRY;

        memcpy(e->name, r, BND_NAME);
        e->name[BND_NAME - 1] = '\0';
        e->off = bnd_u32(r + BND_NAME);
        e->size = bnd_u32(r + BND_NAME + 4);
        if ((long)e->off + (long)e->size > sAssetBundle.size)
        {
            goto bad;
        }
    }
    free(raw);
    sAssetBundle.count = (int)count;
#ifdef _arch_dreamcast
    dbglog(DBG_INFO, "assets: %s -- %d files, %ld bytes\n", path,
           sAssetBundle.count, sAssetBundle.size);
#endif
    return 0;

bad:
#ifdef _arch_dreamcast
    dbglog(DBG_ERROR, "assets: %s is not a bundle -- ignored\n", path);
#endif
    free(raw);
#ifdef _arch_dreamcast
    fs_close(sAssetBundle.fd);
#else
    fclose(sAssetBundle.fp);
#endif
    free(sAssetBundle.index);
    memset(&sAssetBundle, 0, sizeof(sAssetBundle));
    return -1;
}

void asset_bundle_unmount(void)
{
    io_take("unmount", NULL);
    bundle_unmount_locked();
    io_give();
}

int asset_bundle_mount(const char *path)
{
    int r;

    io_take("mount", path);
    /* The root first: its first call mounts the medium's own bundle, and
     * that must not come after -- and so replace -- this one. root_locked
     * sets the root before it mounts, so this does not recurse. */
    (void)root_locked();
    r = bundle_mount_locked(path);
    io_give();
    return r;
}

static const AssetBundleEntry *bnd_find(const char *name)
{
    int i;

    /* A path names a place, not a bundled file (asset_path). */
    if (sAssetBundle.count == 0 || strchr(name, '/') != NULL)
    {
        return NULL;
    }
    for (i = 0; i < sAssetBundle.count; i++)
    {
        if (strcmp(sAssetBundle.index[i].name, name) == 0)
        {
            return &sAssetBundle.index[i];
        }
    }
    return NULL;
}

/* The hold list (assetroot.h). An entry's data is NULL until its first
 * open fills it. */
#define ASSET_HOLD_MAX 32
static struct
{
    const char *name;
    uint8_t *data;
    long size;
} sAssetHold[ASSET_HOLD_MAX];
static int sAssetHoldCount;

static int hold_find(const char *name)
{
    int i;

    if (name == NULL || strchr(name, '/') != NULL)
    {
        return -1;
    }
    for (i = 0; i < sAssetHoldCount; i++)
    {
        if (strcmp(sAssetHold[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

static int open_medium_locked(AssetFile *af, const char *name);
static long read_locked(AssetFile *af, void *buf, long n);
static void close_locked(AssetFile *af);

/* ---- reading ahead (assetroot.h) ---- */

/* Every open the whole opening movie makes (170, src/dc/scprefetch.h),
 * and room over: Mario's alone is 58, most of them effect models out of
 * the bundle. */
#define PF_MAX          256
#define PF_PLAN_MAX     32
#define PF_CHUNK        (64 * 1024)

enum { PF_FREE, PF_QUEUED, PF_FILLING, PF_FULL, PF_DROPPED };

static struct
{
    const char *name;
    uint8_t *data;
    long size;
    int ord;                    /* its scene's place in sPFPlan */
    int opens;                  /* asset_opens served from it, not closed */
    uint32_t seq;               /* never reused: what AssetFile.ahead keeps */
    int state;
} sPF[PF_MAX];
static int sPFPlan[PF_PLAN_MAX];        /* the queued scenes' tags, in order */
static int sPFPlanCount;
static int sPFCur = -1;                 /* the scene now running, in sPFPlan */
static int sPFGo;                       /* its loads are done */
static uint32_t sPFSeq;
/* this window's, for asset_io_report */
static uint32_t sPFFiles, sPFBytes, sPFUs;
static uint32_t sPFServed, sPFMissed, sPFUnused, sPFFailed;
static uint32_t sPFServedAll;

#ifdef _arch_dreamcast
/* the loader has something new to look at: a queue, a scene, room */
static void pf_wake(void)
{
    cond_broadcast(&sPFWake);
}
#else
#define pf_wake() ((void)0)
#endif

static int pf_by_seq(uint32_t seq)
{
    int i;

    for (i = 0; seq != 0 && i < PF_MAX; i++)
    {
        if (sPF[i].state != PF_FREE && sPF[i].seq == seq)
            return i;
    }
    return -1;
}

/* the running scene's entry for `name`, read or not */
static int pf_find(const char *name)
{
    int i;

    if (name == NULL || sPFCur < 0 || strchr(name, '/') != NULL)
        return -1;
    for (i = 0; i < PF_MAX; i++)
    {
        if (sPF[i].ord == sPFCur && (sPF[i].state == PF_QUEUED ||
                                     sPF[i].state == PF_FILLING ||
                                     sPF[i].state == PF_FULL) &&
            strcmp(sPF[i].name, name) == 0)
            return i;
    }
    return -1;
}

/* An entry given back: a copy an open file is still reading goes when
 * that file closes, and one the loader is filling when the loader next
 * looks. */
static void pf_drop_locked(int i)
{
    if (sPF[i].state == PF_FILLING)
    {
        /* the loader frees it; wake it, in case it is paused on the
         * gate mid-file (pf_between_locked), or it never sees this */
        sPF[i].state = PF_DROPPED;
        pf_wake();
        return;
    }
    if (sPF[i].state == PF_FULL && sPF[i].opens > 0)
        return;
    if (sPF[i].state == PF_FULL)
        sPFUnused++;
    free(sPF[i].data);
    sPF[i].data = NULL;
    sPF[i].state = PF_FREE;
    pf_wake();
}

/* The running scene's copy of `name`, waited for if the loader is still
 * reading it: its index, or -1 for none -- and a name the loader has
 * not reached comes off its queue, since the caller is about to read it
 * itself. */
static int pf_claim_locked(const char *name)
{
    int i;

    while ((i = pf_find(name)) >= 0 && sPF[i].state == PF_FILLING)
    {
#ifdef _arch_dreamcast
        io_wait(&sPFDone);
#endif
    }
    if (i >= 0 && sPF[i].state == PF_QUEUED)
    {
        sPF[i].state = PF_FREE;
        sPFMissed++;
        return -1;
    }
    return i;
}

#ifdef _arch_dreamcast
/* Main RAM free, for the report: the arena's free chunks plus what sbrk
 * can still give before the kernel stack ($KOS_BASE/kernel/mm/mm.c
 * mm_sbrk). */
static long pf_free(void)
{
    struct mallinfo mi = mallinfo();
    long top = (long)(_arch_mem_top - THD_KERNEL_STACK_SIZE) -
               (long)(uintptr_t)mm_sbrk(0);

    return top + (long)mi.fordblks;
}
#endif

#if defined(DB_HEAP_CALLERS) && defined(_arch_dreamcast)
/* -DDB_HEAP_CALLERS (src/dc/db.h): who allocated each chunk, by return
 * address, for the heap map in asset_io_report. The link wraps the
 * allocator for it, so the flags go in together:
 *   "-DDB_HEAP_MAP -DDB_HEAP_CALLERS -Wl,--wrap=malloc,--wrap=free,
 *    --wrap=calloc,--wrap=realloc,--wrap=memalign"
 * and addr2line on the ELF names each caller. */
#define HM_SLOTS 16384
static struct { void *p; void *ra; } sHM[HM_SLOTS];

void *__real_malloc(size_t n);
void __real_free(void *p);
void *__real_calloc(size_t n, size_t m);
void *__real_realloc(void *p, size_t n);
void *__real_memalign(size_t a, size_t n);

static unsigned hm_hash(void *p)
{
    return ((uintptr_t)p >> 3) * 2654435761u % HM_SLOTS;
}

static void hm_note(void *p, void *ra)
{
    unsigned i, k;
    int old;

    if (p == NULL)
        return;
    old = irq_disable();
    for (i = hm_hash(p), k = 0; k < HM_SLOTS; k++, i = (i + 1) % HM_SLOTS)
    {
        if (sHM[i].p == NULL || sHM[i].p == p)
        {
            sHM[i].p = p;
            sHM[i].ra = ra;
            break;
        }
    }
    irq_restore(old);
}

static void hm_forget(void *p)
{
    unsigned i, j, k, h;
    int old;

    if (p == NULL)
        return;
    old = irq_disable();
    for (i = hm_hash(p), k = 0; k < HM_SLOTS; k++, i = (i + 1) % HM_SLOTS)
    {
        if (sHM[i].p == NULL)
            break;
        if (sHM[i].p == p)
        {
            /* backward-shift delete, so no tombstones pile up */
            sHM[i].p = NULL;
            for (j = (i + 1) % HM_SLOTS; sHM[j].p != NULL;
                 j = (j + 1) % HM_SLOTS)
            {
                h = hm_hash(sHM[j].p);
                if ((j > i && (h <= i || h > j)) ||
                    (j < i && (h <= i && h > j)))
                {
                    sHM[i] = sHM[j];
                    sHM[j].p = NULL;
                    i = j;
                }
            }
            break;
        }
    }
    irq_restore(old);
}

static void *hm_caller(void *p)
{
    unsigned i, k;

    for (i = hm_hash(p), k = 0; k < HM_SLOTS; k++, i = (i + 1) % HM_SLOTS)
    {
        if (sHM[i].p == NULL)
            return NULL;
        if (sHM[i].p == p)
            return sHM[i].ra;
    }
    return NULL;
}

void *__wrap_malloc(size_t n)
{
    void *p = __real_malloc(n);

    hm_note(p, __builtin_return_address(0));
    return p;
}

void __wrap_free(void *p)
{
    hm_forget(p);
    __real_free(p);
}

void *__wrap_calloc(size_t n, size_t m)
{
    void *p = __real_calloc(n, m);

    hm_note(p, __builtin_return_address(0));
    return p;
}

void *__wrap_realloc(void *p, size_t n)
{
    void *q;

    hm_forget(p);
    q = __real_realloc(p, n);
    hm_note(q != NULL ? q : p, __builtin_return_address(0));
    return q;
}

void *__wrap_memalign(size_t a, size_t n)
{
    void *p = __real_memalign(a, n);

    hm_note(p, __builtin_return_address(0));
    return p;
}
#endif

#if defined(DB_HEAP_GUARD) && defined(_arch_dreamcast)
/* -DDB_HEAP_GUARD (src/dc/db.h): a canary on both sides of every heap
 * block, to catch the out-of-bounds write that corrupts dlmalloc's own
 * bookkeeping -- the opening room's `malloc_consolidate` fault, which
 * names no writer because the write is long before the crash. Build it
 * with the allocator wrapped:
 *
 *   EXTRA_CFLAGS=-DDB_HEAP_GUARD \
 *   EXTRA_LDFLAGS="-Wl,--wrap=malloc,--wrap=free,--wrap=calloc,\
 * --wrap=realloc,--wrap=memalign"
 *
 * A block is [meta][prefix canary][user bytes][suffix canary] inside one
 * real memalign(32) chunk, so the returned pointer keeps the 32-byte
 * alignment the DMA path wants (assetroot.h), and a write past the end
 * of a block hits the suffix canary before it reaches dlmalloc's own
 * header. Every allocation sweeps every live block; every free checks
 * its own. A canary that is no longer the fill prints the block's size,
 * the return address that allocated it (addr2line it) and which canary
 * and offset went bad, once, and keeps going so the run still reaches
 * whatever it would have crashed on. The sweep is O(live blocks) per
 * allocation: it is a find-the-writer build, not a shipping one.
 * __wrap_free and __wrap_realloc fall through to the real allocator for
 * a pointer that is not one of ours (KOS libc allocates some of its
 * own), so a mixed heap is safe. */
#define HG_MAGIC  0x48454147u       /* 'HEAG' */
#define HG_META   16                /* magic, size, pre, pad */
#define HG_GUARD  32
#define HG_FILL   0x5Au
#define HG_SLOTS  8192
#define HG_QN     256               /* recently-freed blocks, for double-free */

typedef struct
{
    void *user;
    void *ra;
    uint32_t size;
    uint32_t pre;
} HGEnt;

static HGEnt sHG[HG_SLOTS];
static int sHGIn;
static int sHGBad;
static int sHGMax;

void *__real_malloc(size_t n);
void __real_free(void *p);
void *__real_calloc(size_t n, size_t m);
void *__real_realloc(void *p, size_t n);
void *__real_memalign(size_t a, size_t n);

static void hg_report(const char *what, uint32_t off, const HGEnt *e)
{
    if (sHGBad++)
        return;
    dbglog(DBG_ERROR, "heap guard: %s canary clobbered at +%u of a %u-byte "
           "block %p (allocated from %p)\n", what, (unsigned)off,
           (unsigned)e->size, e->user, e->ra);
}

/* 0 when clean, -1 and a report when not */
static int hg_check(const HGEnt *e)
{
    const uint8_t *real = (const uint8_t *)e->user - e->pre;
    const uint8_t *user = e->user;
    uint32_t j;

    if (*(const uint32_t *)real != HG_MAGIC)
    {
        hg_report("header", 0, e);
        return -1;
    }
    for (j = HG_META; j < e->pre; j++)
    {
        if (real[j] != HG_FILL)
        {
            hg_report("prefix", j - HG_META, e);
            return -1;
        }
    }
    for (j = 0; j < HG_GUARD; j++)
    {
        if (user[e->size + j] != HG_FILL)
        {
            hg_report("suffix", j, e);
            return -1;
        }
    }
    return 0;
}

/* user is written last and cleared first: the sweep holds no lock, so a
 * slot must never read back half-filled. 0 on success, -1 when the table
 * is full (the caller then keeps the block unguarded, which is safe:
 * hg_untrack will not find it and free falls through). */
static int hg_note(void *user, uint32_t size, uint32_t pre, void *ra)
{
    int old = irq_disable();
    int i, r = -1;

    for (i = 0; i < HG_SLOTS; i++)
    {
        if (sHG[i].user == NULL)
        {
            sHG[i].size = size;
            sHG[i].pre = pre;
            sHG[i].ra = ra;
            sHG[i].user = user;
            if (i + 1 > sHGMax)
                sHGMax = i + 1;
            r = 0;
            break;
        }
    }
    irq_restore(old);
    return r;
}

static int hg_slot(void *user)
{
    int old = irq_disable();
    int i, r = -1;

    for (i = 0; i < sHGMax; i++)
    {
        if (sHG[i].user == user)
        {
            r = i;
            break;
        }
    }
    irq_restore(old);
    return r;
}

static void hg_sweep(void)
{
    int i;

    if (sHGIn || sHGBad)
        return;
    sHGIn = 1;
    for (i = 0; i < sHGMax && !sHGBad; i++)
    {
        if (sHG[i].user != NULL)
            hg_check(&sHG[i]);
    }
    sHGIn = 0;
}

static void *hg_alloc(size_t a, size_t n, void *ra)
{
    uint8_t *real;
    uint32_t pre, total;

    if (a < 32)
        a = 32;
    pre = (uint32_t)((HG_META + HG_GUARD + a - 1) & ~(a - 1));
    total = pre + (uint32_t)n + HG_GUARD;
    real = __real_memalign(a, total);
    if (real == NULL)
        return NULL;
    *(uint32_t *)(real + 0) = HG_MAGIC;
    *(uint32_t *)(real + 4) = (uint32_t)n;
    *(uint32_t *)(real + 8) = pre;
    memset(real + HG_META, HG_FILL, pre - HG_META);
    memset(real + pre + n, HG_FILL, HG_GUARD);
#if defined(DB_HEAP_ZERO)
    /* -DDB_HEAP_ZERO: hand out zeroed blocks, to test whether the hardware crash is a read of uninitialized
     * memory (a garbage pointer/size that only garbage RAM produces). */
    memset(real + pre, 0, n);
#elif defined(DB_HEAP_POISON)
    /* -DDB_HEAP_POISON: the opposite -- fill with a value no pointer, count
     * or float wants (the same 0xDE src/dc/taskman.c's SCENE_HEAP_POISON
     * uses), so a read of uninitialized memory faults at its first use. */
    memset(real + pre, 0xDE, n);
#endif
    if (hg_note(real + pre, (uint32_t)n, pre, ra) < 0)
    {
        __real_free(real);      /* table full: an unguarded block is safe */
        return __real_memalign(a, n);
    }
    /* Track every block, always; sHGIn only stops a nested sweep (the
     * report's own malloc) from recursing. It is a single flag shared by
     * every thread, so it must never gate the bookkeeping: doing that
     * once left blocks allocated by the loader thread while the main
     * thread swept, untracked, and their later realloc fell through to the
     * real one and moved (fighter_drop_texels then aborts). */
    if (!sHGIn)
        hg_sweep();
    return real + pre;
}

/* Recently freed blocks. A double free corrupts dlmalloc's own free list
 * -- the malloc_consolidate fault -- and a canary never sees it, so a
 * second free of a block in this ring reports instead. Magic is checked
 * at user - pre, so a pointer that merely happens to equal a freed one is
 * not a false positive. */
static struct
{
    void *user;
    void *ra;
    uint32_t pre;
    uint32_t size;
} sHGQ[HG_QN];
static int sHGQi;

static void hg_q_push(void *user, uint32_t pre, uint32_t size, void *ra)
{
    sHGQ[sHGQi].user = user;
    sHGQ[sHGQi].ra = ra;
    sHGQ[sHGQi].pre = pre;
    sHGQ[sHGQi].size = size;
    sHGQi = (sHGQi + 1) % HG_QN;
}

static int hg_q_was_freed(void *user, void *free_ra)
{
    int i;

    for (i = 0; i < HG_QN; i++)
    {
        if (sHGQ[i].user == user && sHGQ[i].pre != 0 &&
            *(const uint32_t *)((const uint8_t *)user - sHGQ[i].pre) ==
                HG_MAGIC)
        {
            if (!sHGBad++)
                dbglog(DBG_ERROR, "heap guard: double free of %p (%u-byte "
                       "block allocated from %p) from %p\n", user,
                       (unsigned)sHGQ[i].size, sHGQ[i].ra, free_ra);
            return 1;
        }
    }
    return 0;
}

/* the real block for one of ours, or NULL when it is not ours */
static uint8_t *hg_untrack(void *user)
{
    int i = hg_slot(user);
    uint8_t *real;

    if (i < 0)
        return NULL;
    real = (uint8_t *)user - sHG[i].pre;
    hg_check(&sHG[i]);          /* report a clobber before it is reused */
    hg_q_push(sHG[i].user, sHG[i].pre, sHG[i].size, sHG[i].ra);
    sHG[i].user = NULL;
    return real;
}

void *__wrap_malloc(size_t n)
{
    return hg_alloc(32, n, __builtin_return_address(0));
}

void *__wrap_calloc(size_t n, size_t m)
{
    void *p = hg_alloc(32, n * m, __builtin_return_address(0));

    if (p != NULL)
        memset(p, 0, n * m);
    return p;
}

void *__wrap_memalign(size_t a, size_t n)
{
    return hg_alloc(a, n, __builtin_return_address(0));
}

void __wrap_free(void *p)
{
    uint8_t *real;

    if (p == NULL)
        return;
    real = hg_untrack(p);
    if (real == NULL && hg_q_was_freed(p, __builtin_return_address(0)))
        return;                 /* double free: reported, and not passed on */
    __real_free(real != NULL ? (void *)real : p);
}

void *__wrap_realloc(void *p, size_t n)
{
    int i;
    uint32_t size, pre, total;
    uint8_t *real, *nr;

    if (p == NULL)
        return hg_alloc(32, n, __builtin_return_address(0));
    i = hg_slot(p);
    if (i < 0)
        return __real_realloc(p, n);
    size = sHG[i].size;
    pre = sHG[i].pre;
    real = (uint8_t *)p - pre;
    hg_check(&sHG[i]);          /* report a clobber before it is reused */
    if (n <= size)
    {
        /* The port shrinks a fighter pack and asserts the pointer does not
         * move (src/dc/fighter.c fighter_drop_texels), because relocated
         * pointers point into the pack. So a shrink keeps the real block
         * and only moves the tail canary -- never the real realloc, which
         * may move a memaligned chunk. The tail past the new size is just
         * left allocated for the rest of the run. */
        sHG[i].size = (uint32_t)n;
        memset((uint8_t *)p + n, HG_FILL, HG_GUARD);
        return p;
    }
    /* Grow: the real realloc, which may move the block (normal realloc
     * semantics); any pointer relocated into it is the caller's problem. */
    total = pre + (uint32_t)n + HG_GUARD;
    nr = __real_realloc(real, total);
    if (nr == NULL)
        return NULL;            /* the old block is left untouched */
    *(uint32_t *)(nr + 0) = HG_MAGIC;
    *(uint32_t *)(nr + 4) = (uint32_t)n;
    *(uint32_t *)(nr + 8) = pre;
    memset(nr + HG_META, HG_FILL, pre - HG_META);
    memset(nr + pre + n, HG_FILL, HG_GUARD);
#if defined(DB_HEAP_ZERO)
    memset(nr + pre + size, 0, n - size);
#elif defined(DB_HEAP_POISON)
    memset(nr + pre + size, 0xDE, n - size);
#endif
    sHG[i].user = nr + pre;
    sHG[i].size = (uint32_t)n;
    return nr + pre;
}
#endif

#if defined(DB_HEAP_WALK) && defined(_arch_dreamcast)
/* -DDB_HEAP_WALK (src/dc/db.h): walk every chunk of KOS's dlmalloc and
 * check its bookkeeping, so a clobbered header is reported in the frame
 * it happened in and not at the scene change that frees it. The guard
 * above sees only blocks that went through the wrapped allocator; this
 * sees every chunk, KOS's own thread stacks and kthread_t included, and
 * needs no link flags.
 *
 * The layout is DB_HEAP_MAP's (asset_io_report): chunks run from the
 * 8-aligned end of the image to the break, each [prev_size][size|P], the
 * last one the top chunk ending exactly at mm_sbrk(0). A chunk is free
 * when the next one's P bit is clear (a fastbin chunk still looks in
 * use). The checks are the ones free() leans on without checking:
 *   - the size is a multiple of 8, at least 16, with no stray bits (no
 *     mmap here, so bit 1 is never set), and ends inside the heap
 *   - the first chunk and the top chunk say the chunk before is in use
 *     (dlmalloc never leaves a free chunk against either)
 *   - no two free chunks touch (they would have been merged)
 *   - a free chunk's footer (the next chunk's prev_size) is its size
 *   - its fd and bk point into RAM, and fd->bk and bk->fd point back
 *     (the list heads live in malloc.c's .bss, so a pointer below the
 *     heap is fine as long as it points back)
 *
 * The walk runs with interrupts off, so no thread moves the heap under
 * it, and is skipped for that call when another thread holds malloc's
 * lock (malloc_irq_safe) -- it is mid-operation and the heap is not
 * consistent. The first bad chunk is reported with its neighbours'
 * first eight words and, with DB_HEAP_CALLERS or DB_HEAP_GUARD, the
 * return address that allocated each (addr2line it); after that the
 * walker stays quiet so the run reaches whatever it would have crashed
 * on. Put a gdb breakpoint on asset_heap_walk_failed to stop there. */
#define HW_RAM_LO 0x8c000000u
#define HW_DUMP_WORDS 8

static int sHWBad;
static unsigned sHWWalks, sHWBusy, sHWChunksMax;

/* gdb: `break asset_heap_walk_failed` stops at the first bad chunk */
void __attribute__((noinline)) asset_heap_walk_failed(void)
{
    __asm__ volatile("" ::: "memory");
}

static int hw_ptr_ok(uint32_t v, uintptr_t brk)
{
    return v >= HW_RAM_LO && v < brk && (v & 3u) == 0;
}

/* the return address that allocated the chunk at c, or NULL */
static void *hw_owner(uintptr_t c)
{
#if defined(DB_HEAP_CALLERS)
    return hm_caller((void *)(c + 8u));
#elif defined(DB_HEAP_GUARD)
    int i;

    for (i = 0; i < sHGMax; i++)
    {
        if (sHG[i].user != NULL &&
            (uintptr_t)sHG[i].user - sHG[i].pre - 8u == c)
            return sHG[i].ra;
    }
    return NULL;
#else
    (void)c;
    return NULL;
#endif
}

typedef struct
{
    const char *tag;
    uintptr_t at;
    uint32_t w[HW_DUMP_WORDS];
} HWDump;

static void hw_grab(HWDump *d, const char *tag, uintptr_t at, uintptr_t brk)
{
    int k;

    d->tag = tag;
    d->at = at;
    for (k = 0; k < HW_DUMP_WORDS; k++)
    {
        uintptr_t a = at + 4u * (uintptr_t)k;

        d->w[k] = (at != 0 && a >= HW_RAM_LO && a < brk)
            ? *(const uint32_t *)a : 0;
    }
}

int asset_heap_walk(const char *where)
{
    uintptr_t lo = ((uintptr_t)end + 7u) & ~(uintptr_t)7u;
    uintptr_t brk, p, prev = 0;
    const char *why = NULL;
    unsigned chunks = 0;
    int old, prev_free = 0, k;
    HWDump dump[3];

    if (sHWBad)
        return -1;
    old = irq_disable();
    if (!malloc_irq_safe())
    {
        irq_restore(old);
        sHWBusy++;
        return 0;
    }
    brk = (uintptr_t)mm_sbrk(0);
    for (p = lo; p < brk; prev = p, p += ((const uint32_t *)p)[1] & ~7u)
    {
        const uint32_t *c = (const uint32_t *)p;
        uint32_t head = c[1], size = head & ~7u;
        int is_free;

        chunks++;
        if ((head & 6u) != 0)
            why = "stray bits in the size word";
        else if (size < 16u)
            why = "size under the minimum";
        else if (p + size > brk)
            why = "size runs past the break";
        else if (p == lo && (head & 1u) == 0)
            why = "first chunk says the one before it is free";
        if (why != NULL)
            break;
        if (p + size == brk)
        {
            if ((head & 1u) == 0)
                why = "a free chunk against the top chunk";
            break;
        }
        is_free = (((const uint32_t *)(p + size))[1] & 1u) == 0;
        if (is_free)
        {
            uint32_t fd = c[2], bk = c[3];

            if (prev_free)
                why = "two free chunks in a row";
            else if (((const uint32_t *)(p + size))[0] != size)
                why = "free chunk's footer is not its size";
            else if (!hw_ptr_ok(fd, brk) || !hw_ptr_ok(bk, brk))
                why = "free chunk's fd/bk point out of RAM";
            else if (((const uint32_t *)fd)[3] != p ||
                     ((const uint32_t *)bk)[2] != p)
                why = "free list does not point back";
            if (why != NULL)
                break;
        }
        prev_free = is_free;
    }
    if (why != NULL)
    {
        uint32_t size = ((const uint32_t *)p)[1] & ~7u;

        hw_grab(&dump[0], "before", prev, brk);
        hw_grab(&dump[1], "BAD   ", p, brk);
        hw_grab(&dump[2], "after ",
                (size >= 16u && p + size < brk) ? p + size : 0, brk);
    }
    irq_restore(old);

    sHWWalks++;
    if (chunks > sHWChunksMax)
        sHWChunksMax = chunks;
    if (why == NULL)
        return 0;
    sHWBad = 1;
    dbglog(DBG_ERROR, "heap walk: %s: %s at chunk %08x (walk %u, chunk %u, "
           "heap %08x..%08x)\n", where, why, (unsigned)p, sHWWalks, chunks,
           (unsigned)lo, (unsigned)brk);
    for (k = 0; k < 3; k++)
    {
        const uint32_t *w = dump[k].w;

        if (dump[k].at == 0)
            continue;
        dbglog(DBG_ERROR, "heap walk:   %s %08x: %08x %08x %08x %08x %08x "
               "%08x %08x %08x  (allocated from %08x)\n", dump[k].tag,
               (unsigned)dump[k].at, (unsigned)w[0], (unsigned)w[1],
               (unsigned)w[2], (unsigned)w[3], (unsigned)w[4],
               (unsigned)w[5], (unsigned)w[6], (unsigned)w[7],
               (unsigned)(uintptr_t)hw_owner(dump[k].at));
    }
    asset_heap_walk_failed();
    return -1;
}
#endif

/* The top of the heap: sbrk's headroom and the arena's top chunk
 * (mallinfo's keepcost), the one piece the game's next big buffer is
 * sure to fit in. */
static long pf_top(void)
{
#ifdef _arch_dreamcast
    struct mallinfo mi = mallinfo();

    return (long)(_arch_mem_top - THD_KERNEL_STACK_SIZE) -
           (long)(uintptr_t)mm_sbrk(0) + (long)mi.keepcost;
#else
    return 0x7FFFFFFFL;
#endif
}

/* A buffer for a copy of `size` bytes, or NULL when there is no room
 * for it yet. The reserve is kept at the top, in one piece: a block the
 * allocator finds in a hole costs the top nothing and stays (even when
 * the game has already taken the top below the reserve), and one it
 * cuts off the top and leaves less than the reserve goes back. Counting
 * every free chunk instead left 2.4 MB in holes and 0.5 MB at the top,
 * and the Pikachu scene's 614 KB stage could not be allocated; counting
 * the top alone stalled the loader at Yoshi with 3-5 MB free in holes
 * (-DDB_IO_SLOW=1200). Never more than the top holds, so
 * sbrk is never asked for what it cannot give. */
static uint8_t *pf_alloc(long size)
{
    long top = pf_top(), after;
    uint8_t *data;

    if (size > top)
        return NULL;
    data = asset_alloc(size);
    after = pf_top();
    if (data != NULL && after < top && after < ASSET_PREFETCH_RESERVE)
    {
        free(data);
        data = NULL;
    }
    return data;
}

/* What the loader reads next: the first queued file of the running
 * scene or a later one, once the running scene's loads are done. */
static int pf_pick_locked(void)
{
    int i, best = -1;

    if (!sPFGo || sPFCur < 0)
        return -1;
    for (i = 0; i < PF_MAX; i++)
    {
        if (sPF[i].state == PF_QUEUED && sPF[i].ord >= sPFCur &&
            (best < 0 || sPF[i].seq < sPF[best].seq))
            best = i;
    }
    return best;
}

/* The loader's reads are not the window's: what the medium did for them
 * moves to the loader's own counters (the totals keep it). */
typedef struct
{
    uint32_t files, bundled, bytes, us;
} IOWindow;

static void pf_window_save(IOWindow *w)
{
    w->files = sIOFiles;
    w->bundled = sIOBundled;
    w->bytes = sIOBytes;
    w->us = sIOUs;
}

static void pf_window_move(const IOWindow *w)
{
    sPFBytes += sIOBytes - w->bytes;
    sPFUs += sIOUs - w->us;
    sIOFiles = w->files;
    sIOBundled = w->bundled;
    sIOBytes = w->bytes;
    sIOUs = w->us;
}

/* Between two chunks: the lock goes to whoever is waiting for it, and a
 * scene that is loading has the drive to itself unless it is waiting
 * on this very file. */
static void pf_between_locked(int i, const char *name)
{
#ifdef _arch_dreamcast
    io_give();
    thd_pass();
    io_take("ahead", name);
    while (sPF[i].state == PF_FILLING && !sPFGo && sPF[i].ord != sPFCur)
        io_wait(&sPFWake);
#else
    (void)i;
    (void)name;
#endif
}

/* Read entry i whole. FALSE when there is no room for it yet, and it
 * stays queued; otherwise it ends full, or freed. Called and returns
 * with the lock held, and gives it up between chunks. */
static int pf_fill_locked(int i)
{
    AssetFile tmp;
    IOWindow w;
    const char *name = sPF[i].name;
    uint8_t *data;
    long pos = 0;
    int ok;

    memset(&tmp, 0, sizeof(tmp));
    path_into(tmp.path, sizeof(tmp.path), name, root_locked);
    pf_window_save(&w);
    ok = (open_medium_locked(&tmp, name) == 0);
    pf_window_move(&w);
    if (!ok)
    {
        sPF[i].state = PF_FREE;
        sPFFailed++;
        return 1;
    }
    if ((data = pf_alloc(tmp.size)) == NULL)
    {
        pf_window_save(&w);
        close_locked(&tmp);
        pf_window_move(&w);
        return 0;
    }
    sPF[i].state = PF_FILLING;
    while (pos < tmp.size && sPF[i].state == PF_FILLING)
    {
        long n = tmp.size - pos, got;

        if (n > PF_CHUNK)
            n = PF_CHUNK;
        pf_window_save(&w);
        got = read_locked(&tmp, data + pos, n);
        pf_window_move(&w);
        if (got != n)
            break;
        pos += got;
        pf_between_locked(i, name);
    }
    pf_window_save(&w);
    close_locked(&tmp);
    pf_window_move(&w);
    if (pos == tmp.size && sPF[i].state == PF_FILLING)
    {
        sPF[i].data = data;
        sPF[i].size = tmp.size;
        sPF[i].state = PF_FULL;
        sPFFiles++;
    }
    else
    {
        if (sPF[i].state == PF_FILLING)
            sPFFailed++;
        free(data);
        sPF[i].state = PF_FREE;
    }
#ifdef _arch_dreamcast
    cond_broadcast(&sPFDone);
#endif
    return 1;
}

#ifdef _arch_dreamcast
/* The loader: below the game's priority, so it has the CPU only while
 * the game waits (on the vblank, or on the drive), and never holding
 * the lock while it waits for work or room. */
static void *pf_thread(void *arg)
{
    (void)arg;
    io_take("ahead", NULL);
    for (;;)
    {
        int i = pf_pick_locked();

        if (i < 0 || !pf_fill_locked(i))
            io_wait(&sPFWake);
    }
    return NULL;
}
#endif

/* The one trip to the medium a held name costs: the whole file, through
 * the bundle or the disc like any other open, so the window counts it.
 * A failure leaves the entry empty and the open falls through to the
 * medium as though the name were not held. */
static void hold_fill_locked(int h)
{
    AssetFile tmp;
    uint8_t *data;
    int p = pf_claim_locked(sAssetHold[h].name);

    /* the loader read it already: its copy becomes the held one */
    if (p >= 0 && sPF[p].opens == 0)
    {
        sAssetHold[h].data = sPF[p].data;
        sAssetHold[h].size = sPF[p].size;
        sPF[p].data = NULL;
        sPF[p].state = PF_FREE;
        sPFServed++;
        sPFServedAll++;
        pf_wake();
        return;
    }
    memset(&tmp, 0, sizeof(tmp));
    path_into(tmp.path, sizeof(tmp.path), sAssetHold[h].name, root_locked);
    if (open_medium_locked(&tmp, sAssetHold[h].name) != 0)
    {
        return;
    }
    data = asset_alloc(tmp.size);
    if (data != NULL && read_locked(&tmp, data, tmp.size) == tmp.size)
    {
        sAssetHold[h].data = data;
        sAssetHold[h].size = tmp.size;
    }
    else
    {
        free(data);
    }
    close_locked(&tmp);
}

static void *mmap_locked(AssetFile *af)
{
#ifdef _arch_dreamcast
    if (af->bundled || af->held != NULL)
    {
        return NULL;
    }
    return fs_mmap(af->fd);
#else
    (void)af;
    return NULL;
#endif
}

static int open_locked(AssetFile *af, const char *name)
{
    int h;

    if (af == NULL)
    {
        return -1;
    }
    memset(af, 0, sizeof(*af));
    path_into(af->path, sizeof(af->path), name, root_locked);

    if ((h = hold_find(name)) >= 0)
    {
        if (sAssetHold[h].data == NULL)
        {
            hold_fill_locked(h);
        }
        if (sAssetHold[h].data != NULL)
        {
            af->held = sAssetHold[h].data;
            af->size = sAssetHold[h].size;
            af->lc_t0 = lc_now();
            af->lc_src = 2;
#if defined(DB_IO_TRACE) && defined(_arch_dreamcast)
            dbglog(DBG_INFO, "io: open %s (%ld bytes) at %u ms from RAM\n",
                   name, af->size, (unsigned)timer_ms_gettime64());
#endif
            sIOFromRAM++;
            sIOFromRAMAll++;
            af->us0 = sIOUs;
            af->bytes0 = sIOBytes;
            return 0;
        }
    }
    if ((h = pf_claim_locked(name)) >= 0)
    {
        af->held = sPF[h].data;
        af->size = sPF[h].size;
        af->ahead = sPF[h].seq;
        sPF[h].opens++;
        af->lc_t0 = lc_now();
        af->lc_src = 3;
#if defined(DB_IO_TRACE) && defined(_arch_dreamcast)
        dbglog(DBG_INFO, "io: open %s (%ld bytes) at %u ms read ahead\n",
               name, af->size, (unsigned)timer_ms_gettime64());
#endif
        sPFServed++;
        sPFServedAll++;
        af->us0 = sIOUs;
        af->bytes0 = sIOBytes;
        return 0;
    }
    return open_medium_locked(af, name);
}

/* open_locked's trip to the medium: models.bnd, else the loose file. */
static int open_medium_locked(AssetFile *af, const char *name)
{
    uint64_t t0;

    if (name != NULL)
    {
        const AssetBundleEntry *e = bnd_find(name);

        if (e != NULL)
        {
            af->bundled = 1;
            af->base = (long)e->off;
            af->size = (long)e->size;
            af->lc_t0 = lc_now();
            af->lc_src = 1;
#ifdef _arch_dreamcast
            if (IO_EXPECTING())
            {
                dbglog(DBG_WARNING, "io: %s opened during %s\n", name,
                       sIOExpectNone);
            }
#endif
#if defined(DB_IO_TRACE) && defined(_arch_dreamcast)
            if (!IO_ON_LOADER())
                dbglog(DBG_INFO, "io: open %s (%ld bytes) at %u ms in "
                                 "models.bnd+%ld\n", name, af->size,
                       (unsigned)timer_ms_gettime64(), af->base);
#endif
            sIOBundled++;
            sIOBundledAll++;
            af->us0 = sIOUs;
            af->bytes0 = sIOBytes;
            return 0;
        }
    }
    t0 = asset_now_us();
    af->lc_t0 = lc_now();
    af->lc_src = 0;

#ifdef _arch_dreamcast
    af->fd = fs_open(af->path, O_RDONLY);
    if (af->fd == FILEHND_INVALID)
    {
        return -1;
    }
    IO_SLOW(t0, 0, 1);
    af->size = (long)fs_total(af->fd);
#else
    af->fp = fopen(af->path, "rb");
    if (af->fp == NULL)
    {
        return -1;
    }
    fseek(af->fp, 0, SEEK_END);
    af->size = ftell(af->fp);
    fseek(af->fp, 0, SEEK_SET);
#endif
    asset_io_charge(t0);
#ifdef _arch_dreamcast
    if (IO_EXPECTING())
    {
        dbglog(DBG_WARNING, "io: %s opened during %s\n", name,
               sIOExpectNone);
    }
#endif
#if defined(DB_IO_TRACE) && defined(_arch_dreamcast)
    /* every open, named and stamped, to tell a scene's load from what a
     * match reads while it plays -- the game's own: the loader thread
     * must not print (dbglog is not safe from two threads here) */
    if (!IO_ON_LOADER())
        dbglog(DBG_INFO, "io: open %s (%ld bytes) at %u ms\n", name,
               af->size, (unsigned)timer_ms_gettime64());
#endif
    sIOFiles++;
    sIOFilesAll++;
    af->us0 = sIOUs;
    af->bytes0 = sIOBytes;
    return 0;
}

long asset_size(const AssetFile *af)
{
    return af->size;
}

static int seek_locked(AssetFile *af, long off)
{
    if (af->bundled || af->held != NULL)
    {
        /* the entry's own position; the handle moves at the next read */
        if (off < 0 || off > af->size)
            return -1;
        af->pos = off;
        return 0;
    }
    if (off < 0 || off > af->size)
        return -1;
#ifdef _arch_dreamcast
    if (fs_seek(af->fd, off, SEEK_SET) != off)
        return -1;
#else
    if (fseek(af->fp, off, SEEK_SET) != 0)
        return -1;
#endif
    af->pos = off;
    return 0;
}

#if defined(DB_IO_SELFTEST) && defined(_arch_dreamcast)
/* -DDB_IO_SELFTEST's detector. KOS clamps a
 * read to what is left of the file before it picks a branch
 * (fs_iso9660.c:719), so what decides whether the tail branch can be
 * reached is the clamped length, not the one asked for: a read that is a
 * multiple of 32 but runs past EOF gets no split below and still leaves
 * KOS a remainder under 32. Counted, and the first few named. */
static uint32_t sIOTailShapes;

static void asset_io_check_tail(file_t fd, long n)
{
    off_t pos = fs_tell(fd);
    long left = (long)fs_total(fd) - (long)pos;
    long eff = (n < left) ? n : left;

    if ((n & 31) == 0 && (eff & 31) != 0)
    {
        if (sIOTailShapes++ < 8)
            dbglog(DBG_WARNING, "io selftest: read of %ld at %ld leaves "
                   "KOS a %ld-byte tail (%ld to EOF)\n", n, (long)pos,
                   eff & 31, left);
    }
}
#endif

/* One read off a medium handle: the DMA-stream head and the cached tail
 * on the SH-4 (the note below), fread elsewhere. */
#ifdef _arch_dreamcast
static long asset_fd_read(file_t fd, void *buf, long n)
#else
static long asset_fd_read(FILE *fp, void *buf, long n)
#endif
{
    long got;
#if defined(DB_IO_SLOW) && defined(_arch_dreamcast)
    uint64_t t0 = timer_us_gettime64();
#endif

#ifdef _arch_dreamcast
    /* The last n % 32 bytes of a read never go through the GD-ROM stream.
     * KOS's iso_read ($KOS_BASE/kernel/arch/dreamcast/fs/fs_iso9660.c:797)
     * serves a sub-32-byte remainder of a streamed read with a 32-byte PIO
     * stream request and then thd_polls for it to finish -- and when those
     * 32 bytes are the stream's last (the file's size mod 2048 is 2017 to
     * 2047 and not a multiple of 32), the poll never returns and the game
     * thread waits forever. One file on the disc is that shape,
     * efmballrays.mdl at 4092 bytes, so every Poke Ball that opened froze
     * the match with the music playing on; found by the hang watchdog
     * (src/dc/db.c) during the audio step, and present before it.
     *
     * So: the 32-byte-aligned head is read as before, straight into the
     * caller's buffer on the DMA stream; then the file position is moved
     * off and back, which is iso_seek's way of ending the stream
     * (fs_iso9660.c:911), and the tail is an ordinary read through the
     * sector cache. On the romdisk the two seeks cost nothing.
     *
     * `n` must already be clamped to what is left of the file
     * (asset_read does it): KOS clamps before it picks a branch
     * (fs_iso9660.c:719), so a request that is a multiple of 32 but runs
     * past EOF would pass the test below unsplit and still leave KOS a
     * tail under 32. -DDB_IO_SELFTEST reproduces exactly that hang,
     * reading 1st_read.bin in chunks of 32. */
#ifdef DB_IO_SELFTEST
    asset_io_check_tail(fd, n);
#endif
    if ((n & 31) != 0)
    {
        long head = n & ~31L;
        off_t pos;

        got = 0;
        if (head > 0)
        {
            got = (long)fs_read(fd, buf, (size_t)head);
            if (got != head)
            {
                IO_SLOW(t0, got, 0);
                return got;
            }
        }
        /* a failed move leaves the stream up: stop short rather than
         * hand KOS the tail */
        pos = fs_tell(fd);
        if (pos < 0 || fs_seek(fd, 0, SEEK_SET) < 0 ||
            fs_seek(fd, pos, SEEK_SET) != pos)
        {
            IO_SLOW(t0, got, 0);
            return got;
        }
        {
            long tail = (long)fs_read(fd, (uint8_t *)buf + head,
                                      (size_t)(n - head));

            if (tail > 0)
                got += tail;
        }
    }
    else
        got = (long)fs_read(fd, buf, (size_t)n);
    IO_SLOW(t0, got, 0);
#else
    got = (long)fread(buf, 1, (size_t)n, fp);
#endif
    return got;
}

static long bnd_raw_read(void *buf, long n)
{
#ifdef _arch_dreamcast
    long got = asset_fd_read(sAssetBundle.fd, buf, n);
#else
    long got = asset_fd_read(sAssetBundle.fp, buf, n);
#endif

    if (got > 0)
        sAssetBundle.pos += got;
    return got;
}

static long read_locked(AssetFile *af, void *buf, long n)
{
    uint64_t t0 = asset_now_us();
    long got;

    if (af->held != NULL)
    {
        /* a RAM copy: no medium, so neither the clock nor the bytes */
        if (n > af->size - af->pos)
            n = af->size - af->pos;
        if (n < 0)
            n = 0;
        memcpy(buf, af->held + af->pos, (size_t)n);
        af->pos += n;
        af->reads++;
        return n;
    }
    if (af->bundled)
    {
        /* clamped to the entry, and a seek only when the one handle is
         * not already where this file left off -- a load that reads its
         * files whole and in bundle order (the battle's preloads) never
         * seeks at all */
        if (n > af->size - af->pos)
            n = af->size - af->pos;
        got = 0;
        if (n > 0 && (sAssetBundle.pos == af->base + af->pos ||
                      bnd_raw_seek(af->base + af->pos) == 0))
        {
            got = bnd_raw_read(buf, n);
        }
        if (got > 0)
            af->pos += got;
    }
    else
    {
        /* clamped here, before asset_fd_read decides whether to split */
        if (n > af->size - af->pos)
            n = af->size - af->pos;
        got = 0;
        if (n > 0)
        {
#ifdef _arch_dreamcast
            got = asset_fd_read(af->fd, buf, n);
#else
            got = asset_fd_read(af->fp, buf, n);
#endif
        }
        if (got > 0)
            af->pos += got;
    }
    asset_io_charge(t0);
    af->reads++;
    if (got > 0)
    {
        sIOBytes += (uint32_t)got;
        sIOBytesAll += (uint32_t)got;
    }
    return got;
}

static void close_locked(AssetFile *af)
{
    uint64_t t0 = asset_now_us();

    if (af->ahead != 0)
    {
        /* the loader's copy, done with: its room goes back to it */
        int p = pf_by_seq(af->ahead);

        if (p >= 0 && --sPF[p].opens <= 0)
        {
            sPF[p].opens = 0;
            free(sPF[p].data);
            sPF[p].data = NULL;
            sPF[p].state = PF_FREE;
            pf_wake();
        }
        af->ahead = 0;
    }
    else if (af->bundled || af->held != NULL)
    {
        /* the handle is the bundle's, and stays open; a RAM copy has
         * none */
    }
    else
    {
#ifdef _arch_dreamcast
        fs_close(af->fd);
        af->fd = FILEHND_INVALID;
#else
        fclose(af->fp);
        af->fp = NULL;
#endif
    }
    asset_io_charge(t0);
    /* -DDB_LOAD_CENSUS (loadcensus.h): the file's row, what the medium
     * cost it included; the loader thread's own reads are source 4 */
    lc_file(af->path, af->lc_t0, (uint32_t)af->size,
            sIOBytes - af->bytes0, sIOUs - af->us0,
            IO_ON_LOADER() ? 4 : af->lc_src, af->reads);
#ifdef DB_IO_TRACE
    /* One line per file, the medium's time only, so a slow window can
     * be laid at one file's door and then at one read shape's. */
    if (!IO_ON_LOADER())
    {
        uint32_t us = sIOUs - af->us0;

        dbglog(DBG_INFO, "io:   %s -- %u reads, %u bytes, %u.%u ms\n",
               af->path, (unsigned)af->reads,
               (unsigned)(sIOBytes - af->bytes0),
               (unsigned)(us / 1000), (unsigned)((us % 1000) / 100));
    }
#endif
}

void *asset_mmap(AssetFile *af)
{
    void *p;

    io_take("mmap", af->path);
    p = mmap_locked(af);
    io_give();
    return p;
}

int asset_open(AssetFile *af, const char *name)
{
    int r;

    io_take("open", name);
    r = open_locked(af, name);
    io_give();
    return r;
}

int asset_seek(AssetFile *af, long off)
{
    int r;

    io_take("seek", af->path);
    r = seek_locked(af, off);
    io_give();
    return r;
}

long asset_read(AssetFile *af, void *buf, long n)
{
    long r;

    io_take("read", af->path);
    r = read_locked(af, buf, n);
    io_give();
#if defined(DB_HEAP_WALK_READS) && defined(_arch_dreamcast)
    /* -DDB_HEAP_WALK_READS (with DB_HEAP_WALK): walk the heap after every
     * file read, so the first BAD names the read that left it bad. */
    dbglog(DBG_INFO, "hw read: %s %ld bytes into %08x..%08x\n", af->path, r,
           (unsigned)(uintptr_t)buf, (unsigned)((uintptr_t)buf + (uintptr_t)(r > 0 ? r : 0)));
    asset_heap_walk(af->path);
#endif
    return r;
}

void asset_close(AssetFile *af)
{
    io_take("close", af->path);
    close_locked(af);
    io_give();
}

void *asset_alloc(long n)
{
    if (n < 1)
    {
        n = 1;
    }
#if defined(_arch_dreamcast) && !defined(TARGET_XBOX)
    /* 32, because that is what iso_read wants of a destination before it
     * will DMA into it (assetroot.h). The size is not rounded up: the
     * driver reads the tail through its cache and the caller's buffer
     * must not claim bytes the file does not have. */
    return memalign(32, (size_t)n);
#elif defined(TARGET_XBOX)
    /* XBOX: no DMA wants the 32, and an over-aligned block is one RXDK's
     * realloc always moves -- shrinking too (libc/xbox/heapalloc.c) -- so
     * fighter_drop_texels's in-place trim of a pack would move the pack
     * out from under every pointer relocated into it. A plain block is
     * the NT heap's, which shrinks in place as newlib does; and it is 16
     * aligned, which is all the Xbox side reads with. */
    return malloc((size_t)n);
#else
    return malloc((size_t)n);
#endif
}

/* The loader's copy of an open file, handed over whole: the caller owns
 * it from here and close has nothing to free. NULL when the file is not
 * the loader's, or is part read, or another open is reading the copy. */
static void *pf_take_locked(AssetFile *af)
{
    int p = pf_by_seq(af->ahead);
    void *buf;

    if (p < 0 || af->pos != 0 || sPF[p].opens != 1)
        return NULL;
    buf = sPF[p].data;
    sPF[p].data = NULL;
    sPF[p].opens = 0;
    sPF[p].state = PF_FREE;
    af->ahead = 0;
    af->pos = af->size;
    return buf;
}

/* The game could not allocate a buffer: give back everything the
 * loader holds for later scenes, and wait for it to let go of a file it
 * is reading. What was given back is read by its own scene when it
 * opens it (`missed`), and the next scene change queues the rest of
 * the movie again. Returns the bytes given back. */
static long pf_reclaim_locked(void)
{
    long n = 0;
    int i, busy;

    for (i = 0; i < PF_MAX; i++)
    {
        if ((sPF[i].state == PF_QUEUED || sPF[i].state == PF_FILLING ||
             sPF[i].state == PF_FULL) && sPF[i].ord > sPFCur &&
            sPF[i].opens == 0)
        {
            if (sPF[i].data != NULL)
                n += sPF[i].size;
            pf_drop_locked(i);
        }
    }
    do
    {
        busy = 0;
        for (i = 0; i < PF_MAX; i++)
        {
            if (sPF[i].state == PF_DROPPED)
                busy = 1;
        }
#ifdef _arch_dreamcast
        if (busy)
            io_wait(&sPFDone);
#else
        busy = 0;
#endif
    } while (busy);
    return n;
}

void *asset_read_all(AssetFile *af)
{
    long n = af->size - af->pos;
    void *buf;

    io_take("read", af->path);
    buf = pf_take_locked(af);
    io_give();
    if (buf != NULL)
        return buf;
    buf = asset_alloc(n);
    if (buf == NULL)
    {
        long back;

        io_take("ahead", af->path);
        back = pf_reclaim_locked();
        io_give();
#ifdef _arch_dreamcast
        dbglog(DBG_WARNING, "io: no room for %ld bytes of %s; the loader "
               "gave back %ld\n", n, af->path, back);
#endif
        if (back > 0)
            buf = asset_alloc(n);
    }
    if (buf == NULL || asset_read(af, buf, n) != n)
    {
        free(buf);
        return NULL;
    }
    return buf;
}

void *asset_read_whole(const char *name, long *size_out)
{
    AssetFile af;
    void *buf;

    if (asset_open(&af, name) < 0)
    {
        return NULL;
    }
    buf = asset_read_all(&af);
    asset_close(&af);
    if (buf == NULL)
    {
        return NULL;
    }
    if (size_out != NULL)
    {
        *size_out = af.size;
    }
    return buf;
}

void asset_hold_set(const char *const *names, int count)
{
    int i, k, n = 0;

    io_take("hold", NULL);
    if (count > ASSET_HOLD_MAX)
    {
        count = ASSET_HOLD_MAX;
    }
    /* a copy whose name is not on the new list goes */
    for (i = 0; i < sAssetHoldCount; i++)
    {
        for (k = 0; k < count; k++)
        {
            if (strcmp(sAssetHold[i].name, names[k]) == 0)
                break;
        }
        if (k == count)
        {
            free(sAssetHold[i].data);
            sAssetHold[i].data = NULL;
        }
    }
    /* the new list, in its order, carrying over the copies that stay */
    {
        uint8_t *data[ASSET_HOLD_MAX];
        long size[ASSET_HOLD_MAX];

        for (k = 0; k < count; k++)
        {
            data[k] = NULL;
            size[k] = 0;
            for (i = 0; i < sAssetHoldCount; i++)
            {
                if (sAssetHold[i].data != NULL &&
                    strcmp(sAssetHold[i].name, names[k]) == 0)
                {
                    data[k] = sAssetHold[i].data;
                    size[k] = sAssetHold[i].size;
                    sAssetHold[i].data = NULL;
                    break;
                }
            }
        }
        for (k = 0; k < count; k++)
        {
            sAssetHold[n].name = names[k];
            sAssetHold[n].data = data[k];
            sAssetHold[n].size = size[k];
            n++;
        }
    }
    sAssetHoldCount = n;
    io_give();
}

/* Read every held file that is not in RAM yet, now, rather than at its
 * first open: called where the heap is at its emptiest, so the copies
 * sit low and never split the free space a scene's load then wants
 * (the 1P ladder's list, src/dc/sc1pmanager.c). */
void asset_hold_fill(void)
{
    int h;

    io_take("hold fill", NULL);
    for (h = 0; h < sAssetHoldCount; h++)
    {
        if (sAssetHold[h].data == NULL)
            hold_fill_locked(h);
    }
    io_give();
}

long asset_hold_bytes(void)
{
    long total = 0;
    int i;

    for (i = 0; i < sAssetHoldCount; i++)
    {
        if (sAssetHold[i].data != NULL)
            total += sAssetHold[i].size;
    }
    return total;
}

void asset_prefetch_add(int tag, const char *const *names, int count)
{
    int i = 0, k;

#ifdef DB_NO_PREFETCH
    /* -DDB_NO_PREFETCH (src/dc/db.h): no loader thread and no queue --
     * every open reads the medium on the calling thread (asset_read_all).
     * It isolates the read-ahead loader when a
     * hardware-only crash may be its doing. */
    (void)tag;
    (void)names;
    (void)count;
    (void)i;
    (void)k;
    return;
#else
#if defined(_arch_dreamcast) && !defined(DB_PREFETCH_SYNC)
    if (sPFThread == NULL && count > 0)
    {
        sPFThread = DC_THD_CREATE(0, pf_thread, NULL, "loader");
        if (sPFThread != NULL)
        {
            thd_set_label(sPFThread, "loader");
            thd_set_prio(sPFThread, PRIO_DEFAULT + 1);
        }
    }
#endif
    io_take("ahead", NULL);
    if (sPFPlanCount < PF_PLAN_MAX)
    {
        int ord = sPFPlanCount++;

        sPFPlan[ord] = tag;
        for (k = 0; k < count; k++)
        {
            while (i < PF_MAX && sPF[i].state != PF_FREE)
                i++;
            if (i == PF_MAX)
                break;
            sPF[i].name = names[k];
            sPF[i].data = NULL;
            sPF[i].size = 0;
            sPF[i].ord = ord;
            sPF[i].opens = 0;
            sPF[i].seq = ++sPFSeq;
            sPF[i].state = PF_QUEUED;
        }
    }
    pf_wake();
    io_give();
#endif /* DB_NO_PREFETCH */
}

int asset_prefetch_enter(int tag)
{
    int k, i;

    io_take("ahead", NULL);
    dt_event("prefetch_enter", tag);
    for (k = sPFCur + 1; k < sPFPlanCount && sPFPlan[k] != tag; k++)
        ;
    if (k >= sPFPlanCount)
    {
        io_give();
        return 0;
    }
    sPFCur = k;
    sPFGo = 0;
    for (i = 0; i < PF_MAX; i++)
    {
        if (sPF[i].state != PF_FREE && sPF[i].state != PF_DROPPED &&
            sPF[i].ord < k)
            pf_drop_locked(i);
    }
    pf_wake();
    io_give();
    return 1;
}

void asset_prefetch_go(void)
{
    io_take("ahead", NULL);
    dt_event("prefetch_go", 0);
#if defined(DB_IO_TRACE) && defined(_arch_dreamcast)
    /* where a scene's load ends and its play begins, for the lists the
     * scene manager queues (tools/export/prefetch_lists.py) */
    if (!sIOLoadsDone)
        dbglog(DBG_INFO, "io: loads done at %u ms\n",
               (unsigned)timer_ms_gettime64());
#endif
    sIOLoadsDone = 1;
    sPFGo = 1;
#if defined(_arch_dreamcast) && !defined(DB_PREFETCH_SYNC)
    pf_wake();
#else
    {
        int i;

#ifndef _arch_dreamcast
        sPFOnHost = 1;
#endif
        /* -DDB_PREFETCH_SYNC on the Dreamcast fills here, on this thread,
         * exactly as the host build does -- the same pool and the same
         * entries as the loader thread, but without the second thread.
         * It bisects a hardware-only pool crash into "the loader thread"
         * and "the pool bookkeeping". */
        while ((i = pf_pick_locked()) >= 0 && pf_fill_locked(i))
            ;
#ifndef _arch_dreamcast
        sPFOnHost = 0;
#endif
    }
#endif
    io_give();
}

void asset_prefetch_clear(void)
{
    int i;

    io_take("ahead", NULL);
    dt_event("prefetch_clear", 0);
    for (i = 0; i < PF_MAX; i++)
    {
        if (sPF[i].state != PF_FREE && sPF[i].state != PF_DROPPED)
            pf_drop_locked(i);
        /* an open file's copy is freed at its close; it is no scene's */
        sPF[i].ord = PF_PLAN_MAX;
    }
    sPFPlanCount = 0;
    sPFCur = -1;
    sPFGo = 0;
    pf_wake();
    io_give();
}

void asset_prefetch_drop(const char *name)
{
    int i;

    io_take("ahead", name);
    for (i = 0; i < PF_MAX; i++)
    {
        if ((sPF[i].state == PF_QUEUED || sPF[i].state == PF_FILLING ||
             sPF[i].state == PF_FULL) && sPF[i].ord > sPFCur &&
            strcmp(sPF[i].name, name) == 0)
            pf_drop_locked(i);
    }
    io_give();
}

long asset_prefetch_bytes(void)
{
    long total = 0;
    int i;

    io_take("ahead", NULL);
    for (i = 0; i < PF_MAX; i++)
    {
        if (sPF[i].state == PF_FULL)
            total += sPF[i].size;
    }
    io_give();
    return total;
}

void asset_io_expect_none(const char *what)
{
    io_take("expect", what);
    sIOExpectNone = what;
    io_give();
}

void asset_io_reset(void)
{
    /* the 1P ladder's scenes change through here without an
     * asset_io_report, so a -DDB_DISC_TRACE build empties its records
     * here as well (disctrace.h) */
    dt_dump("reset");
    io_take("reset", NULL);
    sIOExpectNone = NULL;
    sIOFiles = 0;
    sIOBundled = 0;
    sIOFromRAM = 0;
    sIOLoadsDone = 0;
    sPFFiles = sPFBytes = sPFUs = 0;
    sPFServed = sPFMissed = sPFUnused = sPFFailed = 0;
    sIOBytes = 0;
    sIOUs = 0;
    io_give();
}

void asset_io_report(const char *what)
{
    /* One line, in the shape every other per-scene line in this port
     * takes, so a serial log can be read with grep. */
#ifdef _arch_dreamcast
    dbglog(DBG_INFO,
           "io: %s -- %u files, %u bytes, %u.%u ms  "
           "(total %u files, %u bytes, %u.%u ms)\n",
           what, (unsigned)sIOFiles, (unsigned)sIOBytes,
           (unsigned)(sIOUs / 1000u), (unsigned)((sIOUs % 1000u) / 100u),
           (unsigned)sIOFilesAll, (unsigned)sIOBytesAll,
           (unsigned)(sIOUsAll / 1000u), (unsigned)((sIOUsAll % 1000u) / 100u));
    /* the bundle's opens, which are not trips to the medium and so are
     * not in "files" above (assetroot.h) */
    if (sIOBundled != 0)
    {
        dbglog(DBG_INFO, "io: %s -- %u more out of models.bnd  (total %u)\n",
               what, (unsigned)sIOBundled, (unsigned)sIOBundledAll);
    }
    /* and the hold list's, which are not trips to anything */
    if (sIOFromRAM != 0)
    {
        dbglog(DBG_INFO, "io: %s -- %u more from RAM, %ld bytes held  "
               "(total %u)\n", what, (unsigned)sIOFromRAM,
               asset_hold_bytes(), (unsigned)sIOFromRAMAll);
    }
    /* and the loader's: what it read in this window for the scenes
     * after, and what this scene made of what it read before */
    if ((sPFFiles | sPFServed | sPFMissed | sPFUnused | sPFFailed) != 0)
    {
        dbglog(DBG_INFO, "io: %s -- read ahead %u files, %u bytes, "
               "%u.%u ms; served %u, missed %u, unused %u, failed %u, "
               "holding %ld bytes, %ld free, %ld at the top  "
               "(total served %u)\n", what,
               (unsigned)sPFFiles, (unsigned)sPFBytes,
               (unsigned)(sPFUs / 1000u), (unsigned)((sPFUs % 1000u) / 100u),
               (unsigned)sPFServed, (unsigned)sPFMissed,
               (unsigned)sPFUnused, (unsigned)sPFFailed,
               asset_prefetch_bytes(), pf_free(), pf_top(),
               (unsigned)sPFServedAll);
    }
#ifdef DB_HEAP_MAP
    {
        /* KOS's dlmalloc 2.7: chunks from the 8-aligned end of the
         * image to the top chunk, each [prev_size][size|PREV_INUSE];
         * a chunk is in use when the next one's PREV_INUSE is set.
         * Big chunks one per line, runs of small ones summed. */
        struct mallinfo mi = mallinfo();
        uintptr_t p = ((uintptr_t)end + 7u) & ~(uintptr_t)7u;
        uintptr_t top = (uintptr_t)mm_sbrk(0) - (uintptr_t)mi.keepcost;
        unsigned run_n = 0, run_b = 0, hole_n = 0, hole_b = 0;
#ifdef DB_HEAP_CALLERS
        int past = 0, pins = 0;
#endif

        while (p + 8u <= top)
        {
            uint32_t size = ((uint32_t *)p)[1] & ~7u;
            int used = (((uint32_t *)(p + size))[1] & 1u) != 0;

            if (size < 16u || p + size > top)
            {
                dbglog(DBG_INFO, "heap: %s -- bad chunk at %08x size %u\n",
                       what, (unsigned)p, (unsigned)size);
                break;
            }
#ifdef DB_HEAP_CALLERS
            /* past the first big hole, every chunk still in use is one
             * that splits free space: name its caller */
            if (!used && size >= 262144u)
                past = 1;
            if (used && past && pins < 400)
            {
                pins++;
                dbglog(DBG_INFO, "heap:   pin %08x %u by %08x\n",
                       (unsigned)p, (unsigned)size,
                       (unsigned)(uintptr_t)hm_caller((void *)(p + 8u)));
            }
#endif
            if (size >= 32768u)
            {
                if (run_n != 0 || hole_n != 0)
                    dbglog(DBG_INFO, "heap:   small: %u used (%u bytes), "
                           "%u free (%u bytes)\n", run_n, run_b, hole_n,
                           hole_b);
                run_n = run_b = hole_n = hole_b = 0;
                dbglog(DBG_INFO, "heap:   %08x %s %u\n", (unsigned)p,
                       used ? "USED" : "free", (unsigned)size);
            }
            else if (used)
                run_n++, run_b += size;
            else
                hole_n++, hole_b += size;
            p += size;
        }
        if (run_n != 0 || hole_n != 0)
            dbglog(DBG_INFO, "heap:   small: %u used (%u bytes), %u free "
                   "(%u bytes)\n", run_n, run_b, hole_n, hole_b);
        dbglog(DBG_INFO, "heap: %s -- top chunk at %08x, %ld at the top\n",
               what, (unsigned)top, pf_top());
    }
#endif
#ifdef DB_IO_IRQOFF
    dbglog(DBG_INFO, "io irqoff: %s -- %u trips, %u ms with interrupts off "
           "so far, longest %u ms\n", what, (unsigned)sIOIrqOffTrips,
           (unsigned)(sIOIrqOffUs / 1000u),
           (unsigned)(sIOIrqOffLongest / 1000u));
#endif
#ifdef DB_HEAP_WALK
    /* the proof it ran: a clean scene prints this and nothing else */
    asset_heap_walk(what);
    dbglog(DBG_INFO, "heap walk: %s -- %s, %u walks, %u skipped with malloc "
           "busy, up to %u chunks\n", what, sHWBad ? "BAD" : "clean",
           sHWWalks, sHWBusy, sHWChunksMax);
#endif
    dt_dump(what);
#else
    (void)what;
#endif
}

#if defined(DB_IO_SELFTEST) && defined(_arch_dreamcast)
/* ---------------------------------------------------------------- *
 * -DDB_IO_SELFTEST: every read shape against a second KOS path      *
 * ---------------------------------------------------------------- *
 *
 * Run once at boot from db_install (src/dc/db.c). For every file under
 * the root and every models.bnd entry it reads one or two regions of the
 * file -- all of it when small, else the first 4 KB and the last 8 KB,
 * which is where the tail and EOF cases live -- in the shapes the
 * loaders use, through asset_read, and compares each against the same
 * bytes read a second way: 2 KB at a time into a buffer one byte off
 * alignment, which iso_read can only serve through its sector cache
 * (iso_read's cache branch), never the stream. The shapes:
 *
 *   one read of the region, into an aligned and an unaligned buffer
 *   chunks of 7, 31, 32, 33, 2048 and 32768, asking for a whole chunk
 *     even past EOF when the region ends there, as reader_get does
 *   one multiple-of-32 read past EOF
 *   100 bytes from 1 and from 33 bytes into the region
 *
 * It also lists every file of the tail-hang size, and the
 * detector above counts reads that leave KOS a tail under 32. */
#define ST_SMALL        (16 * 1024)
#define ST_HEAD         4096
#define ST_TAIL         8192
/* a region, plus room for a whole chunk asked for past its end */
#define ST_REGION_MAX   (ST_SMALL + 32768 + 64)

static uint8_t *sSTRef, *sSTBuf, *sSTRaw;
static uint32_t sSTCases, sSTBad;

/* The reference: `n` bytes from `off` of `path`, 2 KB at a time into a
 * buffer one byte off alignment. */
static int st_ref(const char *path, long off, long n)
{
    file_t fd;
    uint8_t *odd = sSTRaw + 1;
    long got = 0;

    io_take("selftest", path);
    fd = fs_open(path, O_RDONLY);
    if (fd == FILEHND_INVALID)
    {
        io_give();
        return -1;
    }
    if (fs_seek(fd, off, SEEK_SET) != off)
    {
        fs_close(fd);
        io_give();
        return -1;
    }
    while (got < n)
    {
        long want = (n - got > 2048) ? 2048 : n - got;
        ssize_t r = fs_read(fd, odd, (size_t)want);

        if (r <= 0)
            break;
        memcpy(sSTRef + got, odd, (size_t)r);
        got += r;
    }
    fs_close(fd);
    io_give();
    return (got == n) ? 0 : -1;
}

static void st_case(const char *name, const char *shape, long at,
                    const uint8_t *got, long n_got, long n_want)
{
    sSTCases++;
    if (n_got != n_want || memcmp(got, sSTRef + at, (size_t)n_want) != 0)
    {
        if (sSTBad++ < 16)
            dbglog(DBG_ERROR, "io selftest: MISMATCH %s: %s at +%ld -- got "
                   "%ld of %ld bytes\n", name, shape, at, n_got, n_want);
    }
}

/* The shapes over [a, b) of `name`, whose reference is in sSTRef[0..). */
static void st_region(const char *name, long size, long a, long b)
{
    static const long chunks[] = { 7, 31, 32, 33, 2048, 32768 };
    AssetFile af;
    long len = b - a, n, pos;
    unsigned i;

    if (asset_open(&af, name) < 0)
    {
        dbglog(DBG_ERROR, "io selftest: %s did not open\n", name);
        sSTBad++;
        return;
    }
    /* one read, aligned then not */
    asset_seek(&af, a);
    n = asset_read(&af, sSTBuf, len);
    st_case(name, "one read", 0, sSTBuf, n, len);
    asset_seek(&af, a);
    n = asset_read(&af, sSTBuf + 4, len);
    st_case(name, "one read, unaligned", 0, sSTBuf + 4, n, len);
    /* chunks */
    for (i = 0; i < sizeof(chunks) / sizeof(chunks[0]); i++)
    {
        long c = chunks[i];

        asset_seek(&af, a);
        for (pos = 0; pos < len; )
        {
            long want = (b == size) ? c : ((len - pos < c) ? len - pos : c);
            long r;

            if (pos + want > ST_REGION_MAX)
                want = ST_REGION_MAX - pos;
            r = asset_read(&af, sSTBuf + pos, want);
            if (r <= 0)
                break;
            pos += r;
        }
        st_case(name, (c == 7) ? "chunks of 7" : (c == 31) ? "chunks of 31"
                : (c == 32) ? "chunks of 32" : (c == 33) ? "chunks of 33"
                : (c == 2048) ? "chunks of 2048" : "chunks of 32768",
                0, sSTBuf, pos, len);
    }
    /* a multiple of 32 past EOF */
    if (b == size)
    {
        long want = ((len + 31) & ~31L) + 32;

        asset_seek(&af, a);
        n = asset_read(&af, sSTBuf, want);
        st_case(name, "32-multiple past EOF", 0, sSTBuf, n, len);
    }
    /* odd starts */
    if (len > 133)
    {
        asset_seek(&af, a + 1);
        n = asset_read(&af, sSTBuf, 100);
        st_case(name, "100 from +1", 1, sSTBuf, n, 100);
        asset_seek(&af, a + 33);
        n = asset_read(&af, sSTBuf, 100);
        st_case(name, "100 from +33", 33, sSTBuf, n, 100);
    }
    asset_close(&af);
}

/* Both regions of one file. `path` is where the reference reads from and
 * `base` where the file starts in it (a bundled entry's offset). */
static void st_file(const char *name, const char *path, long base,
                    long size)
{
    long a, b;

    if (size <= 0)
        return;
    if (size <= ST_SMALL)
    {
        if (st_ref(path, base, size) == 0)
            st_region(name, size, 0, size);
        else
            sSTBad++;
        return;
    }
    if (st_ref(path, base, ST_HEAD) == 0)
        st_region(name, size, 0, ST_HEAD);
    else
        sSTBad++;
    /* the tail region starts on a sector, where a stream can start */
    a = (size - ST_TAIL) & ~2047L;
    b = size;
    if (st_ref(path, base + a, b - a) == 0)
        st_region(name, size, a, b);
    else
        sSTBad++;
}

void asset_io_selftest(void)
{
    const char *root = asset_root();
    char path[ASSET_PATH_MAX];
    uint64_t t0 = timer_ms_gettime64();
    uint32_t files = 0, shaped = 0;
    const dirent_t *de;
    file_t dir;
    int i;

    sSTRef = memalign(32, ST_SMALL + 64);
    sSTBuf = memalign(32, ST_REGION_MAX + 64);
    sSTRaw = memalign(32, 2048 + 64);
    if (sSTRef == NULL || sSTBuf == NULL || sSTRaw == NULL)
    {
        dbglog(DBG_ERROR, "io selftest: no memory\n");
        return;
    }
    dbglog(DBG_INFO, "io selftest: %s, %d bundled\n", root,
           sAssetBundle.count);
    io_take("selftest", root);
    dir = fs_open(root, O_RDONLY | O_DIR);
    if (dir == FILEHND_INVALID)
    {
        io_give();
        dbglog(DBG_ERROR, "io selftest: cannot list %s\n", root);
        return;
    }
    /* the names first, then the reads: the listing shares the drive */
    {
        static char names[512][32];
        static long sizes[512];
        int n = 0;

        while ((de = fs_readdir(dir)) != NULL && n < 512)
        {
            long r;

            if (de->size < 0 || strlen(de->name) >= sizeof(names[0]))
                continue;
            r = de->size % 2048;
            if (r >= 2017 && (de->size & 31) != 0)
            {
                shaped++;
                dbglog(DBG_WARNING, "io selftest: %s is the tail-hang "
                       "shape (%d bytes)\n", de->name, de->size);
            }
            strcpy(names[n], de->name);
            sizes[n++] = de->size;
        }
        fs_close(dir);
        io_give();
        for (i = 0; i < n; i++)
        {
            asset_path(path, sizeof(path), names[i]);
            st_file(names[i], path, 0, sizes[i]);
            files++;
        }
    }
    snprintf(path, sizeof(path), "%s/models.bnd", root);
    for (i = 0; i < sAssetBundle.count; i++)
    {
        const AssetBundleEntry *e = &sAssetBundle.index[i];

        st_file(e->name, path, (long)e->off, (long)e->size);
    }
    dbglog(DBG_INFO, "io selftest: %u files + %d bundled, %u cases, %u "
           "mismatches, %u tail-hang-shaped files, %u reads that leave KOS "
           "a tail -- %u ms\n", (unsigned)files, sAssetBundle.count,
           (unsigned)sSTCases, (unsigned)sSTBad, (unsigned)shaped,
           (unsigned)sIOTailShapes,
           (unsigned)(timer_ms_gettime64() - t0));
    free(sSTRef);
    free(sSTBuf);
    free(sSTRaw);
    asset_io_reset();
}
#endif

#if defined(DB_IO_STRESS) && defined(_arch_dreamcast)
/* ---------------------------------------------------------------- *
 * -DDB_IO_STRESS: a second reader                                   *
 * ---------------------------------------------------------------- *
 *
 * The port has one reader thread, and KOS's /cd is not safe for two
 * (its close can free a handle under a concurrent read); nor is the bundle's one shared handle.
 * This thread is the second reader on purpose: it loops over every file
 * under the root and every models.bnd entry, reads up to the first 16 KB
 * of each through asset_open/asset_read/asset_close, and checks each
 * pass's CRC against its first. A mismatch is a race; a stall is caught
 * by the hang watchdog (src/dc/db.c). Its lines can interleave with the
 * main thread's on the serial log (dbglog is printf, src/dc/fgm.c's
 * note), so they are few: a progress line every 256 reads. */
#define SX_MAX      512
#define SX_BYTES    (16 * 1024)

static char sSXNames[SX_MAX][32];
static uint32_t sSXCrc[SX_MAX];
static uint8_t sSXSeen[SX_MAX];
static uint32_t sSXTable[256];

static uint32_t sx_crc(const uint8_t *p, long n)
{
    uint32_t c = 0xFFFFFFFFu;

    while (n-- > 0)
        c = sSXTable[(c ^ *p++) & 0xFF] ^ (c >> 8);
    return ~c;
}

static void *sx_thread(void *arg)
{
    uint8_t *buf = memalign(32, SX_BYTES);
    uint32_t reads = 0, bad = 0;
    int n = 0, i;
    file_t dir;
    const dirent_t *de;

    (void)arg;
    for (i = 0; i < 256; i++)
    {
        uint32_t c = (uint32_t)i;
        int k;

        for (k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
        sSXTable[i] = c;
    }
    {
        const char *root = asset_root();

        io_take("stress", root);
        dir = fs_open(root, O_RDONLY | O_DIR);
        if (dir != FILEHND_INVALID)
        {
            while ((de = fs_readdir(dir)) != NULL && n < SX_MAX)
                if (de->size > 0 && strlen(de->name) < sizeof(sSXNames[0]))
                    strcpy(sSXNames[n++], de->name);
            fs_close(dir);
        }
        io_give();
    }
    if (buf == NULL || dir == FILEHND_INVALID)
    {
        dbglog(DBG_ERROR, "io stress: cannot start\n");
        return NULL;
    }
    for (i = 0; i < sAssetBundle.count && n < SX_MAX; i++)
        strcpy(sSXNames[n++], sAssetBundle.index[i].name);
    dbglog(DBG_INFO, "io stress: %d files\n", n);
    for (;;)
    {
        for (i = 0; i < n; i++)
        {
            AssetFile af;
            long want, got;
            uint32_t c;

            if (asset_open(&af, sSXNames[i]) < 0)
            {
                bad++;
                continue;
            }
            want = (af.size < SX_BYTES) ? af.size : SX_BYTES;
            got = asset_read(&af, buf, want);
            asset_close(&af);
            c = sx_crc(buf, got);
            if (got != want)
                c = ~c;
            if (!sSXSeen[i])
            {
                sSXSeen[i] = 1;
                sSXCrc[i] = c;
            }
            else if (c != sSXCrc[i] && bad++ < 16)
            {
                dbglog(DBG_ERROR, "io stress: MISMATCH %s (%ld of %ld)\n",
                       sSXNames[i], got, want);
            }
            if ((++reads & 255) == 0)
                dbglog(DBG_INFO, "io stress: %u reads, %u mismatches\n",
                       (unsigned)reads, (unsigned)bad);
            thd_pass();
        }
    }
    return NULL;
}

void asset_io_stress_start(void)
{
    sSXThread = DC_THD_CREATE(1, sx_thread, NULL, "io stress");
    if (sSXThread != NULL)
        thd_set_label(sSXThread, "io stress");
}
#endif
