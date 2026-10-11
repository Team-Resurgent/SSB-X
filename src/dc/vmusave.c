/* vmusave.c -- see vmusave.h.
 *
 * ---- sys/dma.c:148,154 syDmaReadSram / syDmaWriteSram ----------------
 *
 * The cartridge's battery-backed SRAM, which is the whole of the game's
 * storage: lb/lbbackup.c writes two copies of LBBackupData into it and
 * reads them back, and nothing else in the game touches it. On the N64
 * these are osEPiStartDma on the SRAM PI handle.
 *
 * The store is a byte array here. What this file
 * adds is the other side: the array is read off a VMU at boot and
 * written back to it when it changes, so the two bodies are still the
 * only thing that moved and everything the game does over them is the
 * decompilation's. The card itself -- the maple bus, vmufs, the file and
 * its pictures, and the host's fake of all of it -- is src/dc/vmucard.c.
 *
 * A store of zeroes is a cartridge that has never been written: the
 * checksum matches (0 == 0) but the signature does not (0 != 666), so
 * lbBackupIsChecksumValid says no for both copies and the game installs
 * dSCManagerDefaultBackupData and writes it. That is the path a fresh
 * cartridge takes, and it is now also the path a blank memory card
 * takes -- the write it ends in is what puts the first file on the card,
 * and no code here decides that. A damaged file takes it too: vmucard.c
 * judges a file before any of it is loaded, and one the game would refuse
 * leaves the store as zeroes rather than as its bytes, so the defaults
 * that replace it are the game's, reached the way a fresh cartridge
 * reaches them.
 *
 * SRAM_SIZE covers lb/lbbackup.c's two offsets: ALIGN(sizeof, 0x0) is 0
 * -- ALIGN's mask is ~(0-1), which is 0 -- and ALIGN(sizeof, 0x10)
 * rounds 1516 up to 1520, so the second copy ends at 3036. The N64's
 * SRAM is 32 KB; a read or write outside what is modelled here is a
 * fault in the caller and is reported rather than wrapped.
 *
 * SRAM_SAVED is that 3036, spelled the way lbBackupWrite spells it, and
 * it is what goes on the card rather than the whole 4096: a VMU has 200
 * blocks of 512 bytes and the difference is two of them. The two copies
 * go across together, because the redundancy is the game's -- a card
 * that loses the first copy is exactly the cartridge lb/lbbackup.c
 * already recovers from.
 *
 * ---- DIVERGES: when the write happens ---------------------------------
 *
 * On the N64 syDmaWriteSram is a DMA that has completed by the time it
 * returns, and lbBackupWrite calls it twice. A VMU write is maple
 * traffic and block erases -- fourteen blocks here, a good fraction of a
 * second on real hardware -- and doing it twice for one save would be
 * paying that price for nothing, because both copies are inside the one
 * file. So a write marks the store dirty and lands in RAM; at the end of
 * the tic the pump copies the store and hands the copy to a writer
 * thread, which writes the card once while the game goes on.
 *
 * That is a real deviation and it has a real edge: the console losing
 * power between the game's write and the card's -- the rest of the tic
 * and the second the card takes -- loses the save the N64 would have
 * kept, and the corner notice ("SAVING...") is how the
 * player knows the window is open. The alternative -- writing the card
 * inside the shim -- stalls the game for that second, twice.
 */
#include "vmusave.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <sys/obj.h>
#include <sys/debug.h>
#include <lb/lbtypes.h>

#include "vmucard.h"

/* sys/dma.h is not included: its other declarations are libultra's PI
 * handles (OSPiHandle), which have no counterpart here and no header to
 * come from. The two signatures below are that file's, verbatim. */
extern void syDmaReadSram(uintptr_t rom_src, void *ram_dst, size_t size);
extern void syDmaWriteSram(void *ram_src, uintptr_t rom_dst, size_t size);

#define SRAM_SIZE 4096

/* ALIGN(sizeof(LBBackupData), 0x10) + sizeof(LBBackupData): the second
 * copy's offset plus its length, which is lbBackupWrite's own arithmetic
 * (lb/lbbackup.c:39-40) and so cannot drift from it. */
#define SRAM_SAVED \
    ((((sizeof(LBBackupData) + 0xFu) & ~0xFu)) + sizeof(LBBackupData))

static u8 sSYDmaSram[SRAM_SIZE];

/* Set by a write, cleared by the flush that acts on it. Not a count:
 * the store is one object and the card holds all of it, so any write
 * since the last flush means the same thing. */
static int sSYDmaSramDirty;

static int sram_range_ok(const char *what, uintptr_t off, size_t size)
{
    if (off > SRAM_SIZE || size > SRAM_SIZE - off)
    {
        syDebugPrintf("%s: %u bytes at %u is outside the %u-byte store\n",
                      what, (unsigned)size, (unsigned)off,
                      (unsigned)SRAM_SIZE);
        return 0;
    }
    return 1;
}

void syDmaReadSram(uintptr_t rom_src, void *ram_dst, size_t size)
{
    if (sram_range_ok("syDmaReadSram", rom_src, size))
    {
        memcpy(ram_dst, &sSYDmaSram[rom_src], size);
    }
}

void syDmaWriteSram(void *ram_src, uintptr_t rom_dst, size_t size)
{
    if (sram_range_ok("syDmaWriteSram", rom_dst, size))
    {
        memcpy(&sSYDmaSram[rom_dst], ram_src, size);
        sSYDmaSramDirty = 1;
    }
}

int sy_sram_is_dirty(void)
{
    return sSYDmaSramDirty;
}

/* The card the store is backed by, or port -1: none. */
static int sSavePort = -1;
static int sSaveUnit;

/* ---- the writer -------------------------------------------
 *
 * The store is the main thread's alone: the game writes it inside a tic
 * and the pump, at the end of the tic (src/dc/taskman.h's frame-end
 * hook), copies it into the shot -- the one thing the two threads share,
 * with the three counters beside it, all under sSaveLock. The writer
 * thread writes the newest shot and goes back to sleep; a shot published
 * while it was busy is written when it finishes, and two published while
 * it was busy are written once, as the newer (shot_gen runs ahead of
 * done_gen by any amount, and one write catches it up). The writer never
 * logs: it leaves its answer beside the counters and the next pump says
 * it, on the main thread.
 *
 * The host has no threads. There the writer runs inside the pump, at
 * once, unless a test has paused it (sy_sram_host_pause_writer) to see
 * what piles up; sy_sram_quiesce runs it regardless. */
static uint8_t sSaveShot[SRAM_SAVED];
static int sShotPort, sShotUnit;
static uint32_t sShotGen;   /* shots published */
static uint32_t sDoneGen;   /* the newest shot the writer has finished */
static int sDoneRv;         /* and its answer, an nVMUCard* */
static int sDonePort, sDoneUnit;
static uint32_t sDoneMs;    /* and how long it took */
static uint32_t sSaidGen;   /* the newest answer the pump has logged */
static uint32_t sWrites;    /* writes made, landed or not */
static uint32_t sPumps;     /* tics the pump has seen, for the log */
static uint32_t sShotTic, sDoneTic;

static int sSaveHold;       /* sy_sram_hold()s not yet released */
static int sBootState;      /* nSYSramBoot*, from sy_sram_init */
static int sBootFree = -1;
static int sSaveFailed;
static int sNoticeArmed;
static int sNoticeLoad;     /* the notice says LOADING, not SAVING */
static int sNoticeLoadNext; /* the next shot is the boot-time one */
static uint64_t sNoticeFrom;

#ifdef DB_MEMCARD_FAIL_WRITE
static int sFailLeft = DB_MEMCARD_FAIL_WRITE;
#endif

#ifndef FT_HOSTTEST

#include <kos/thread.h>
#include <kos/mutex.h>
#include <kos/cond.h>
#include <arch/timer.h>

#include "stackguard.h"

static mutex_t sSaveLock = MUTEX_INITIALIZER;
static condvar_t sSaveWake = COND_INITIALIZER;
static condvar_t sSaveDone = COND_INITIALIZER;
static kthread_t *sSaveThread;

#define save_lock() mutex_lock(&sSaveLock)
#define save_unlock() mutex_unlock(&sSaveLock)

static uint64_t save_now_ms(void)
{
    return timer_ms_gettime64();
}

#else /* FT_HOSTTEST */

static uint64_t sHostNowMs;
static int sHostPaused;

#define save_lock() ((void)0)
#define save_unlock() ((void)0)

static uint64_t save_now_ms(void)
{
    return sHostNowMs;
}

#endif /* FT_HOSTTEST */

/* One write, of the newest shot. Entered and left with sSaveLock held;
 * dropped for the write itself, so the pump can publish another shot
 * meanwhile. */
static void save_write_newest(void)
{
    static uint8_t buf[SRAM_SAVED];
    uint32_t gen = sShotGen, tic = sShotTic;
    int port = sShotPort, unit = sShotUnit;
    uint64_t t0;
    int rv;

    memcpy(buf, sSaveShot, SRAM_SAVED);
    save_unlock();

    t0 = save_now_ms();
#ifdef DB_MEMCARD_FAIL_WRITE
    if (sFailLeft > 0)
    {
        sFailLeft--;
        rv = nVMUCardErrWrite;
    }
    else
#endif
    rv = vmucard_write(port, unit, buf, SRAM_SAVED);
#if defined(DB_MEMCARD_SLOW_MS) && !defined(FT_HOSTTEST)
    /* -DDB_MEMCARD_SLOW_MS (src/dc/db.h): a card as slow as a real one */
    if (save_now_ms() - t0 < (uint64_t)(DB_MEMCARD_SLOW_MS))
    {
        thd_sleep((int)((DB_MEMCARD_SLOW_MS) - (save_now_ms() - t0)));
    }
#endif

    save_lock();
    sWrites++;
    sDoneGen = gen;
    sDoneRv = rv;
    sDonePort = port;
    sDoneUnit = unit;
    sDoneTic = tic;
    sDoneMs = (uint32_t)(save_now_ms() - t0);
#ifndef FT_HOSTTEST
    cond_broadcast(&sSaveDone);
#endif
}

#ifndef FT_HOSTTEST
static void *save_thread(void *arg)
{
    (void)arg;
    save_lock();
    for (;;)
    {
        while (sDoneGen == sShotGen)
        {
            cond_wait(&sSaveWake, &sSaveLock);
        }
        save_write_newest();
    }
    return NULL;
}
#endif

static void save_start_writer(void)
{
#ifndef FT_HOSTTEST
    if (sSaveThread != NULL)
    {
        return;
    }
    /* below the game thread, as the disc's read-ahead loader is: it runs
     * while the game waits for the retrace, and a write that takes a
     * second costs no frame */
    sSaveThread = DC_THD_CREATE(1, save_thread, NULL, "vmu");
    if (sSaveThread != NULL)
    {
        thd_set_label(sSaveThread, "vmu");
        thd_set_prio(sSaveThread, PRIO_DEFAULT + 1);
    }
    else
    {
        syDebugPrintf("save: no writer thread -- saves write in the frame\n");
    }
#endif
}

/* Hand the store to the writer. Main thread, sSaveLock held. */
static void save_publish(void)
{
    memcpy(sSaveShot, sSYDmaSram, SRAM_SAVED);
    sShotPort = sSavePort;
    sShotUnit = sSaveUnit;
    sShotTic = sPumps;
    /* A shot published while the notice is up extends it; one published
     * after it has gone starts it again, for at least a second from
     * now (sy_sram_notice_visible). */
    if ((sDoneGen == sShotGen) &&
        ((sNoticeArmed == 0) || (save_now_ms() - sNoticeFrom >= 1000)))
    {
        sNoticeFrom = save_now_ms();
    }
    /* the boot-time shot is the card being read in and stamped, so its
     * notice says LOADING; any other shot turns a live notice to SAVING */
    sNoticeLoad = sNoticeLoadNext;
    sNoticeLoadNext = 0;
    sNoticeArmed = 1;
    sShotGen++;
#ifndef FT_HOSTTEST
    if (sSaveThread == NULL)
    {
        save_write_newest(); /* no thread: the frame pays, as before */
        return;
    }
    cond_signal(&sSaveWake);
#endif
}

/* Say what the writer has finished since the last time. Main thread. */
static void save_report(void)
{
    uint32_t gen;
    int rv, port, unit;
    uint32_t ms, tic;

    save_lock();
    tic = sDoneTic;
    gen = sDoneGen;
    rv = sDoneRv;
    port = sDonePort;
    unit = sDoneUnit;
    ms = sDoneMs;
    save_unlock();

    if (gen == sSaidGen)
    {
        return;
    }
    sSaidGen = gen;
    if (rv == nVMUCardOK)
    {
        /* a later write that landed makes an earlier failure moot: the
         * card has the newest store */
        sSaveFailed = 0;
        /* the tics are the game's while the card was written: the
         * pump's since the shot, which a write in the frame would hold
         * at one */
        syDebugPrintf("save: wrote %d bytes to %s (%d blocks, %u ms, %u "
                      "tics)\n", (int)SRAM_SAVED, vmucard_where(port, unit),
                      vmucard_file_blocks(SRAM_SAVED), (unsigned)ms,
                      (unsigned)(sPumps - tic));
    }
    else
    {
        sSaveFailed = 1;
        syDebugPrintf("save: writing %s failed: %s\n",
                      vmucard_where(port, unit), vmucard_error_name(rv));
    }
}

#ifdef FT_HOSTTEST
static void save_run_host_writer(int even_paused)
{
    while ((sDoneGen != sShotGen) && ((sHostPaused == 0) || even_paused))
    {
        save_write_newest();
    }
}
#endif

void sy_sram_quiesce(void)
{
#ifndef FT_HOSTTEST
    save_lock();
    while (sDoneGen != sShotGen)
    {
        cond_wait(&sSaveDone, &sSaveLock);
    }
    save_unlock();
#else
    save_run_host_writer(1);
#endif
    save_report();
}

void sy_sram_init(void)
{
    VMUCardInfo cards[VMUCARD_MAX];
    VMUCardInfo info;
    const VMUCardInfo *pick = NULL;
    int n, i;

    /* a write still going belongs to the store this is about to
     * replace: let it land first */
    sy_sram_quiesce();
    memset(sSYDmaSram, 0, sizeof(sSYDmaSram));
    sSYDmaSramDirty = 0;
    sSavePort = -1;
    sSaveHold = 0;
    sSaveFailed = 0;
    sNoticeArmed = 0;
    sNoticeLoad = 0;
    sNoticeLoadNext = 0;
    sBootState = nSYSramBootNoCard;
    sBootFree = -1;
    vmucard_init();
    save_start_writer();

    /* The card with a save the game will take, whichever slot that is;
     * failing that one with a file it will not, which the game's defaults
     * then replace as they always did; failing that the first card there
     * is, which is where the first save goes. */
    n = vmucard_scan(cards, VMUCARD_MAX);
    for (i = 0; (i < n) && (pick == NULL); i++)
    {
        if (cards[i].state == nVMUCardValid)
        {
            pick = &cards[i];
        }
    }
    for (i = 0; (i < n) && (pick == NULL); i++)
    {
        if (cards[i].state == nVMUCardCorrupt)
        {
            pick = &cards[i];
        }
    }
    if ((pick == NULL) && (n != 0))
    {
        pick = &cards[0];
    }
    for (i = 0; i < n; i++)
    {
        if ((cards[i].state == nVMUCardCorrupt) && (&cards[i] != pick))
        {
            syDebugPrintf("save: %s is damaged (%s); left alone\n",
                          vmucard_where(cards[i].port, cards[i].unit),
                          cards[i].why);
        }
    }
    if (pick == NULL)
    {
        syDebugPrintf("save: no VMU -- this session's records will "
                      "not outlive it\n");
        return;
    }
    sSavePort = pick->port;
    sSaveUnit = pick->unit;
    sBootFree = pick->free_blocks;

    switch (pick->state)
    {
    case nVMUCardValid:
        /* read again, into the store: the scan judged it and kept none */
        if (vmucard_read(sSavePort, sSaveUnit, sSYDmaSram, SRAM_SAVED,
                         &info) == nVMUCardValid)
        {
            syDebugPrintf("save: %s, %d bytes, saved %04d-%02d-%02d "
                          "%02d:%02d\n",
                          vmucard_where(sSavePort, sSaveUnit),
                          (int)SRAM_SAVED, info.year, info.month, info.day,
                          info.hour, info.min);
            sBootState = nSYSramBootSaved;
            break;
        }
        /* a card that changed between the two reads is one with nothing
         * to load: the store stays a cartridge never written */
        memset(sSYDmaSram, 0, sizeof(sSYDmaSram));
        syDebugPrintf("save: %s did not read back; not loaded\n",
                      vmucard_where(sSavePort, sSaveUnit));
        sBootState = nSYSramBootDamaged;
        break;

    case nVMUCardCorrupt:
        syDebugPrintf("save: %s is damaged (%s) -- not loaded; the game's "
                      "defaults will replace it\n",
                      vmucard_where(sSavePort, sSaveUnit), pick->why);
        sBootState = nSYSramBootDamaged;
        break;

    default:
        syDebugPrintf("save: no file on %s yet; the first save writes one "
                      "(%d blocks, %d free)\n",
                      vmucard_where(sSavePort, sSaveUnit),
                      vmucard_file_blocks(SRAM_SAVED), pick->free_blocks);
        sBootState = ((pick->free_blocks >= 0) &&
                      (pick->free_blocks < vmucard_file_blocks(SRAM_SAVED)))
                         ? nSYSramBootNoSpace : nSYSramBootNoSave;
        break;
    }
}

int sy_sram_boot_state(void)
{
    return sBootState;
}

int sy_sram_boot_free_blocks(void)
{
    return sBootFree;
}

int sy_sram_boot_need_blocks(void)
{
    return vmucard_file_blocks(SRAM_SAVED);
}

void sy_sram_boot_create(void)
{
    sy_sram_release();
    /* blocking: the store is on the VMU, or the write has failed, when
     * this returns. The notice a later scene would show for it is
     * not wanted -- the player watched it happen. */
    sy_sram_save_now();
    sy_sram_quiesce();
    sNoticeArmed = 0;
    sNoticeLoadNext = 0;
    sNoticeLoad = 0;
}

void sy_sram_boot_decline(void)
{
    sy_sram_release();
    sSavePort = -1;
    sSYDmaSramDirty = 0;
    syDebugPrintf("save: no save file on a VMU this session\n");
}

void sy_sram_pump(void)
{
    sPumps++;
    save_lock();
    if ((sSYDmaSramDirty != 0) && (sSaveHold == 0))
    {
        /* Cleared when the shot is taken, not when it lands: one attempt
         * per change. A card that is full or gone would otherwise be
         * retried every frame for the rest of the session. */
        sSYDmaSramDirty = 0;
        if (sSavePort >= 0)
        {
            save_publish();
        }
    }
    save_unlock();
#ifdef FT_HOSTTEST
    save_run_host_writer(0);
#endif
    save_report();
}

void sy_sram_flush(void)
{
    sy_sram_pump();
    sy_sram_quiesce();
}

void sy_sram_save_now(void)
{
    save_lock();
    sSYDmaSramDirty = 0;
    if (sSavePort >= 0)
    {
        save_publish();
    }
    save_unlock();
#ifdef FT_HOSTTEST
    save_run_host_writer(0);
#endif
    save_report();
}

void sy_sram_hold(void)
{
    sSaveHold++;
}

void sy_sram_release(void)
{
    if (sSaveHold > 0)
    {
        sSaveHold--;
    }
}

int sy_sram_failed(void)
{
    return sSaveFailed;
}

void sy_sram_clear_failure(void)
{
    sSaveFailed = 0;
}

int sy_sram_busy(void)
{
    int busy;

    save_lock();
    busy = (sDoneGen != sShotGen);
    save_unlock();

    return busy;
}

int sy_sram_notice_visible(void)
{
    if (sy_sram_busy() != 0)
    {
        return 1;
    }
    return (sNoticeArmed != 0) && (save_now_ms() - sNoticeFrom < 1000);
}

void sy_sram_next_write_is_load(void)
{
    sNoticeLoadNext = 1;
}

int sy_sram_notice_is_load(void)
{
    return sNoticeLoad;
}

const char *sy_sram_backing(void)
{
    return (sSavePort >= 0) ? vmucard_where(sSavePort, sSaveUnit) : NULL;
}

int sy_sram_card(int *port, int *unit)
{
    if (sSavePort < 0)
    {
        return 0;
    }
    *port = sSavePort;
    *unit = sSaveUnit;
    return 1;
}

#ifdef FT_HOSTTEST
/* The one-card machine the save tests use: card
 * A1 is a plain file (src/dc/vmucard.c's host fake), and the store is
 * backed by it from now, as it always was -- a test may put the card in
 * mid-session without a boot. NULL takes it out, and the store with it. */
void sy_sram_set_host_path(const char *path)
{
    vmucard_host_set_file(path);
    sSavePort = (path != NULL) ? 0 : -1;
    sSaveUnit = 1;
}

void sy_sram_host_pause_writer(int paused)
{
    sHostPaused = paused;
}

void sy_sram_host_set_clock(uint64_t ms)
{
    sHostNowMs = ms;
}

uint32_t sy_sram_host_writes(void)
{
    return sWrites;
}
#endif
