#ifndef DC_DISCTRACE_H
#define DC_DISCTRACE_H

/* -DDB_DISC_TRACE: every trip to the disc, in order, for offline analysis of
 * what the loaders read and how the drive served it (which files are read
 * together, in what order, how far the head moves between them). Off by
 * default and empty when off. Build with
 *
 *   EXTRA_LDFLAGS="-Wl,--wrap=cdrom_read_sectors_ex -Wl,--wrap=cdrom_stream_start -Wl,--wrap=cdrom_stream_request"
 *
 * as well, for the drive's own view. One record is 24 bytes; they are kept
 * in RAM and printed, at DBG_WARNING so DB_QUIET keeps them, when a scene's
 * load ends (asset_io_report) -- never while a load is running, since the
 * serial line at 115200 baud costs 87 us a byte and would sit inside the
 * thing being measured. A scene with more than DT_MAX records drops the rest
 * and says how many.
 *
 * Every line starts "dt: ":
 *   H <scene> <now_us> <records> <dropped>   header of one dump, now_us the
 *                                            64-bit clock at the dump, so
 *                                            the 32-bit times below unwrap
 *   F <id> <path>                            a file, the first time a dump
 *                                            needs its id
 *   T <tid> <label>                          a thread, likewise
 *   <op> <t> <dur> <tid> <id> <off> <len> <ret>
 *        t    start, us, low 32 bits of the clock
 *        dur  us, start to return
 *        id   file id (F lines); 0 for the drive's own ops
 *   ops, by layer -- the game's (fs_*, from assetroot.c):
 *        O open (ret: fd)        C close
 *        S seek (off, ret: where it went)
 *        R read (off: file position before, len: asked, ret: got)
 *   and the drive's (KOS's iso9660 calling cdrom.c; sector as the ISO sees
 *   it plus 150, which is what the drive is asked):
 *        K read sectors (off: sector, len: count, id: 1 when DMA)
 *        Z start a stream (off: sector, len: count)
 *        Q stream request (len: bytes, id: 1 when it blocks, ret: 0 ok)
 *   and E, a marker (id: an F-numbered string, off: its argument): the
 *   loader's queue opening, going, being cleared. */

#if defined(DB_DISC_TRACE) && defined(_arch_dreamcast)
#include <kos/fs.h>

#define DT_MAX 24000

file_t dt_fs_open(const char *path, int mode);
ssize_t dt_fs_read(file_t fd, void *buf, size_t n);
off_t dt_fs_seek(file_t fd, off_t off, int whence);
int dt_fs_close(file_t fd);

void dt_event(const char *what, int arg);
void dt_dump(const char *scene);
#else
#define dt_event(what, arg) ((void)0)
#define dt_dump(scene) ((void)0)
#endif

#endif
