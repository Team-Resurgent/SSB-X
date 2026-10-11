/* assetroot.h -- where the game's files are.
 *
 * The romdisk is part of the ELF and the disc build puts the same tree
 * on the GD-ROM, where it is /cd, and both builds are the same code: the root is
 * probed once, at the first load, and the loaders -- not their callers --
 * join it with the bare name they are given ("mario.pack", "ifstatus.spr").
 * So a call site names a file and nothing else knows which medium this
 * build reads from.
 *
 * The probe is /cd/disc.id, which scripts/make_cdi.sh writes into the
 * image with the build stamp in it. Its contents go in the boot log, so a
 * serial log always says which tree the run read from -- the failure that
 * guards against is a stale romdisk quietly serving a disc run.
 *
 * A third root, /pc, is KOS's dcload host-filesystem passthrough (a
 * serial coder's cable or BBA, dc-tool on the other end) -- src/dc/db.h's
 * DB_PC_ROOT. It is a runtime probe like /cd's, not a build-time choice:
 * a DB_PC_ROOT build still falls through to /cd or /rd when no dcload
 * link is up. Unverified on real hardware as of this writing. */
#ifndef SSB_DC_ASSETROOT_H
#define SSB_DC_ASSETROOT_H

#include <stddef.h>
#include <stdint.h>

#ifdef _arch_dreamcast
#include <kos/fs.h>
#else
#include <stdio.h>
#endif

/* Long enough for the root, the longest name the game asks for
 * ("mnitemswitch.spr"), and the separator. */
#define ASSET_PATH_MAX 128

/* "/pc" on a DB_PC_ROOT build with a live dcload link, else "/cd" on a
 * disc build, "/rd" on a romdisk build, "romdisk" on the host, decided on
 * the first call and never again. */
const char *asset_root(void);

/* root + "/" + name into buf, which the caller owns. A name that already
 * contains a '/' is a path and is copied through unchanged -- the host
 * tests open "romdisk/mario.pack" directly, and bgm.c's fallback list
 * names both media. Returns buf. */
const char *asset_path(char *buf, size_t n, const char *name);

/* ---------------------------------------------------------------- *
 * Reading one, and how long it took *
 * ---------------------------------------------------------------- *
 *
 * A loader that opens its own file through newlib stdio is cheap on the romdisk (the
 * file is already in RAM) but on a disc it decides which of KOS's two
 * ISO9660 read paths the bytes take. iso_read
 * ($KOS_BASE/kernel/arch/dreamcast/fs/fs_iso9660.c:744) hands a read to
 * the GD-ROM's DMA stream only when the destination is 32-byte aligned,
 * the file pointer is on a 32-byte boundary and at least 32 bytes are
 * wanted; anything else goes sector at a time through a 16-block cache.
 * stdio fails all three: it reads into its own 1 KB malloc'd buffer and
 * copies out of it.
 *
 * So the open/read/close lives here instead, once, with the medium and
 * the alignment in the same place -- and with the counters, because the
 * only honest way to argue about a load screen is to have measured one.
 * fs_mmap is the one call kept apart: fighter.c and stage.c ask for it
 * by name, on the romdisk, through asset_mmap below. */
typedef struct AssetFile
{
#ifdef _arch_dreamcast
    file_t fd;
#else
    FILE *fp;
#endif
    long size;
    int bundled;                /* served out of models.bnd, below: no
                                 * handle of its own, and fd/fp unset */
    long base;                  /* a bundled file's first byte in the
                                 * bundle */
    const uint8_t *held;        /* served out of a RAM copy (the hold
                                 * list, below): no handle, fd/fp unset */
    uint32_t ahead;             /* and that copy is the loader's (reading
                                 * ahead, below): freed at close, or
                                 * handed over by asset_read_all */
    long pos;                   /* where the file has read to: every read
                                 * is clamped to size - pos before the
                                 * medium sees it */
    uint32_t reads;             /* asset_read calls on this file */
    uint32_t us0, bytes0;       /* the window's counters at open, for
                                 * the per-file line -DDB_IO_TRACE prints
                                 * at close (src/dc/db.h) */
    uint32_t lc_t0;             /* -DDB_LOAD_CENSUS (loadcensus.h): when */
    uint8_t lc_src;             /* it opened, and what served it */
    char path[ASSET_PATH_MAX];  /* what asset_path made of the name */
} AssetFile;

/* Open `name` under the asset root. 0 on success, -1 if it is not there;
 * the caller reports it, because this file is linked into host builds
 * that have no syDebugPrintf. */
int asset_open(AssetFile *af, const char *name);

/* fs_mmap for the loaders that ask for it by name (fighter.c, stage.c):
 * the file in place on the romdisk, NULL on a disc, off the SH-4 and for
 * a bundled file, which is a range of another file and has no mapping of
 * its own. */
void *asset_mmap(AssetFile *af);

/* ---------------------------------------------------------------- *
 * models.bnd: many small files, one extent                          *
 * ---------------------------------------------------------------- *
 *
 * The battle's load opens 124 small effect, weapon and item models back
 * to back; loose on a disc each is a directory lookup, a new DMA stream
 * and most of a sector of padding. tools/export/ssb_bundle.py packs them into
 * one file (its docstring has the layout), and asset_root mounts
 * <root>/models.bnd when there is one: an asset_open of a name in its
 * index is served from inside it, over ONE handle kept open for the
 * run, and every other name goes to the medium as before. Only the disc
 * carries a bundle (scripts/make_cdi.sh); the romdisk keeps its loose
 * files and runs exactly as it did.
 *
 * SSB_DISC_NO_BUNDLE=1 builds the loose disc.
 *
 * Mount is public for the host suite, which mounts a bundle of its own;
 * 0 on success, -1 if `path` is missing or is not a bundle (nothing is
 * mounted then). Unmount closes it and forgets the index. */
int asset_bundle_mount(const char *path);
void asset_bundle_unmount(void);
/* entries in the mounted bundle, 0 when none is */
int asset_bundle_count(void);

/* The file's whole length, known at open. */
long asset_size(const AssetFile *af);

/* Absolute seek. Aborts the disc driver's stream, so a loader that wants
 * the DMA path reads forward. */
int asset_seek(AssetFile *af, long off);

/* Bytes actually read, or -1. Never more than size minus the position:
 * a request past EOF is cut to the end of the file before the disc
 * driver sees it. */
long asset_read(AssetFile *af, void *buf, long n);

/* Closes. The file was counted when it opened. */
void asset_close(AssetFile *af);

/* memalign(32) on the Dreamcast, malloc elsewhere: the alignment the
 * stream path wants. free() gives it back either way. */
void *asset_alloc(long n);

/* The whole file in a fresh asset_alloc'd buffer, or NULL. The caller
 * frees it. */
void *asset_read_whole(const char *name, long *size_out);

/* The same for a file already open and not yet read: the rest of it in a
 * buffer the caller frees, or NULL on a short read. When the loader read
 * the file ahead (below) the buffer is the loader's own, handed over
 * rather than copied. */
void *asset_read_all(AssetFile *af);

/* The instrument. asset_io_reset starts a window (the scene manager
 * opens one per scene); asset_io_report closes it onto the log as
 *
 *   io: <what> -- 12 files, 5875776 bytes, 4210.4 ms  (total ...)
 *
 * Bytes are what crossed the medium, so a file that came back from
 * fs_mmap counts as a file and no bytes -- which is exactly the
 * difference between the two builds. Milliseconds are open-to-close,
 * summed over fs_open/fs_read/fs_close alone, so what a loader does
 * between two reads is not charged to the medium). */
void asset_io_reset(void);

/* Until the next asset_io_reset, every file opened is a mistake worth a
 * line: "io: <name> opened during <what>". The battle sets it once its
 * loading is done, since a read mid-match stalls the game thread on a
 * seek. The line is always on; it costs nothing when nothing is read. */
void asset_io_expect_none(const char *what);

/* The memory card's turn, for a build running over a dcload cable. KOS's
 * dcload_syscall_native runs every /pc call with interrupts masked (its
 * dcload_syscalls.c irq_disable_scoped), and the loader's 64 KB read is
 * ~0.4 s of that on serial; the maple bus is driven from the retrace
 * interrupt, so a card read that overlaps one times out at KOS's 100 ms and
 * leaves its frame on the queue (the next use of that card's frame aborts
 * in maple_frame_init). Holding the I/O lock across a card operation keeps
 * the two apart. Returns 1 when it took the lock (a dcload build), 0 when
 * it did nothing (a disc, where nothing masks interrupts and the card
 * stays off the lock so a save never stands in front of a load); hand the
 * result back to asset_io_card_end. */
int asset_io_card_begin(void);
void asset_io_card_end(int took);
void asset_io_report(const char *what);

/* ---------------------------------------------------------------- *
 * The hold list: files kept in main RAM across scene changes         *
 * ---------------------------------------------------------------- *
 *
 * A run of scenes that all read the same files -- the opening movie
 * reads the effect bank and the announcer's sprites in nearly every one
 * of its scenes --
 * names them here, and the first asset_open of a held name reads the
 * whole file into main RAM, once; every open after that, in any scene,
 * is served from the copy and never reaches the disc. Only whole names
 * are held (no '/'). Reads from a copy count in neither "files" nor
 * "bytes"; the per-scene line reports them as "more from RAM".
 *
 * asset_hold_set replaces the list: copies of names still on it stay,
 * the rest are freed. `names` must outlive the call (a static table);
 * NULL/0 frees everything. asset_hold_bytes is what the copies cost. */
void asset_hold_set(const char *const *names, int count);
void asset_hold_fill(void);
long asset_hold_bytes(void);

/* ---------------------------------------------------------------- *
 * Reading ahead: later scenes' files while this one plays            *
 * ---------------------------------------------------------------- *
 *
 * The drive sits idle while a scene plays. A run of scenes that knows
 * what each will open (the opening movie; src/dc/scmanager.c) queues
 * the lot, scene by scene, and a loader thread reads them, whole, into
 * main RAM, as far ahead as RAM allows. A scene's asset_open of a name
 * queued for it is served from the copy, which close frees -- or which
 * asset_read_all hands over, so a file read whole costs no second
 * buffer -- and every scene passed gives back what it did not use, so
 * the loader runs on into the room each scene leaves. An open of a name
 * the loader is reading waits for it; one it has not reached reads the
 * medium as it always did.
 *
 * asset_prefetch_add(tag, names, count): queue one scene's files after
 * everything queued so far. `tag` names the scene (the scene manager's
 * scene kind); `names` must outlive the queue (a static table).
 *
 * asset_prefetch_enter(tag): the scene `tag` is starting. Copies for the
 * scenes queued before it are freed, and its own become the ones an
 * asset_open can be served from. The loader pauses between reads until
 * asset_prefetch_go, so this scene's own loads have the drive. FALSE,
 * with nothing changed, when `tag` is not queued: the caller clears.
 *
 * asset_prefetch_go(): the scene's loads are done. What was read for it
 * and not opened is freed, and the loader goes on to the scenes after.
 * Safe to call more than once per scene.
 *
 * asset_prefetch_clear(): all of it freed and forgotten -- the run of
 * scenes left early (the player pressed START).
 *
 * The loader reads 64 KB per turn of the I/O lock and yields between,
 * at a priority below the game's, and waits rather than leave less than
 * ASSET_PREFETCH_RESERVE of main RAM free in one piece at the top of the
 * heap, where the game's next big buffer will come from. asset_io_report says what it
 * did in the window, and what the scene made of what it read before:
 *
 *   io: <what> -- read ahead 5 files, 1214468 bytes, 23.9 ms;
 *       served 3, missed 0, unused 0, holding 3405120 bytes
 *
 * Neither the loader's reads nor the served opens are in the window's
 * "files"/"bytes"; the loader's are in the totals.
 *
 * Off the SH-4 there is no thread: asset_prefetch_go reads everything
 * queued then and there, which is what the host suite tests. */
#define ASSET_PREFETCH_RESERVE (1536L * 1024L)
void asset_prefetch_add(int tag, const char *const *names, int count);
int asset_prefetch_enter(int tag);
void asset_prefetch_go(void);
void asset_prefetch_clear(void);
/* asset_prefetch_drop(name): the copies of `name` queued for the scenes
 * after this one are not wanted after all -- the game has the file
 * already (the Room's two random desk fighters, whose packs the movie
 * keeps). Freed, or taken off the queue. */
void asset_prefetch_drop(const char *name);
/* bytes the loader holds, read and not yet opened */
long asset_prefetch_bytes(void);

#ifdef _arch_dreamcast
/* What the hang watchdog (src/dc/db.c) knows of the disc: `calls`
 * counts every asset_* call finished since boot, and while one is under
 * way, tid is its thread (-1 when the lock is free), op and what name
 * it (op is "open", "read", ...; what is the path, or NULL), and ms is
 * how long it has held the lock. */
typedef struct
{
    uint32_t calls;
    int tid;
    const char *op;
    const char *what;
    uint32_t ms;
} AssetIOState;

void asset_io_state(AssetIOState *st);
#endif

/* src/dc/db.h's -DDB_IO_SELFTEST and -DDB_IO_STRESS:
 * every read shape checked against a second KOS path once at boot, and a
 * second reader thread. Both called from db_install. */
#ifdef DB_IO_SELFTEST
void asset_io_selftest(void);
#endif
#ifdef DB_IO_STRESS
void asset_io_stress_start(void);
#endif

/* src/dc/db.h's -DDB_HEAP_WALK: check every dlmalloc chunk's header and
 * free-list links. 0 when clean or skipped (another thread is inside
 * malloc), -1 after the first bad chunk, which it reports once, with its
 * neighbours, under `where`. Called at every frame end, before each
 * fighter pack is freed and at each scene's I/O report. */
#if defined(DB_HEAP_WALK) && defined(_arch_dreamcast)
int asset_heap_walk(const char *where);
#define ASSET_HEAP_WALK(where) asset_heap_walk(where)
#else
#define ASSET_HEAP_WALK(where) ((void)0)
#endif

#endif /* SSB_DC_ASSETROOT_H */
