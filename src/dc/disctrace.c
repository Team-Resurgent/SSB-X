/* disctrace.c -- see disctrace.h. Only DB_DISC_TRACE builds have anything
 * in it. */
#include "disctrace.h"

#if defined(DB_DISC_TRACE) && defined(_arch_dreamcast)

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <kos.h>
#include <dc/cdrom.h>

typedef struct
{
    uint32_t t;     /* start, us, low 32 bits */
    uint32_t dur;   /* us */
    uint32_t off;
    uint32_t len;
    int32_t ret;
    uint16_t id;
    uint8_t op;
    uint8_t tid;
} DTRec;

_Static_assert(sizeof(DTRec) == 24, "record size");

#define DT_NAMES 1024
#define DT_NAME_LEN 64
#define DT_FDS 1024

static DTRec sRec[DT_MAX];
static uint32_t sCount, sDropped;

static char sName[DT_NAMES][DT_NAME_LEN];
static uint8_t sNamePrinted[DT_NAMES];
static uint16_t sNames = 1;             /* id 0 is "none" */

static char sThdLabel[256][16];
static uint8_t sThdSeen[256], sThdPrinted[256];

static struct { uint16_t id; uint32_t pos; } sFd[DT_FDS];

static uint64_t dt_now(void)
{
    return timer_us_gettime64();
}

/* The id of a path or marker, made on first sight. Called with interrupts
 * on: it is a search over at most DT_NAMES short strings. */
static uint16_t dt_intern(const char *s)
{
    uint16_t i;

    for (i = 1; i < sNames; i++)
    {
        if (strncmp(sName[i], s, DT_NAME_LEN - 1) == 0)
            return i;
    }
    if (sNames >= DT_NAMES)
        return 0;
    strncpy(sName[sNames], s, DT_NAME_LEN - 1);
    return sNames++;
}

static uint8_t dt_tid(void)
{
    kthread_t *t = thd_get_current();
    uint8_t tid = (t != NULL) ? (uint8_t)(t->tid & 0xFF) : 0;

    if (!sThdSeen[tid])
    {
        const char *l = (t != NULL) ? thd_get_label(t) : NULL;

        sThdSeen[tid] = 1;
        strncpy(sThdLabel[tid], (l != NULL) ? l : "?", sizeof(sThdLabel[tid]) - 1);
    }
    return tid;
}

static void dt_add(uint8_t op, uint64_t t0, uint16_t id, uint32_t off,
                   uint32_t len, int32_t ret)
{
    uint64_t t1 = dt_now();
    uint8_t tid = dt_tid();
    int old = irq_disable();

    if (sCount < DT_MAX)
    {
        DTRec *r = &sRec[sCount++];

        r->t = (uint32_t)t0;
        r->dur = (uint32_t)(t1 - t0);
        r->off = off;
        r->len = len;
        r->ret = ret;
        r->id = id;
        r->op = op;
        r->tid = tid;
    }
    else
    {
        sDropped++;
    }
    irq_restore(old);
}

/* ---- the game's view: fs_* through assetroot.c ---- */

file_t dt_fs_open(const char *path, int mode)
{
    uint64_t t0 = dt_now();
    file_t fd = fs_open(path, mode);
    uint16_t id = dt_intern(path);

    if ((fd >= 0) && (fd < DT_FDS))
    {
        sFd[fd].id = id;
        sFd[fd].pos = 0;
    }
    dt_add('O', t0, id, (uint32_t)mode, 0, (int32_t)fd);

    return fd;
}

ssize_t dt_fs_read(file_t fd, void *buf, size_t n)
{
    uint64_t t0 = dt_now();
    uint32_t pos = ((fd >= 0) && (fd < DT_FDS)) ? sFd[fd].pos : 0;
    ssize_t got = fs_read(fd, buf, n);

    if ((got > 0) && (fd >= 0) && (fd < DT_FDS))
        sFd[fd].pos += (uint32_t)got;
    dt_add('R', t0, ((fd >= 0) && (fd < DT_FDS)) ? sFd[fd].id : 0, pos,
           (uint32_t)n, (int32_t)got);

    return got;
}

off_t dt_fs_seek(file_t fd, off_t off, int whence)
{
    uint64_t t0 = dt_now();
    off_t r = fs_seek(fd, off, whence);

    if ((r >= 0) && (fd >= 0) && (fd < DT_FDS))
        sFd[fd].pos = (uint32_t)r;
    dt_add('S', t0, ((fd >= 0) && (fd < DT_FDS)) ? sFd[fd].id : 0,
           (uint32_t)off, (uint32_t)whence, (int32_t)r);

    return r;
}

int dt_fs_close(file_t fd)
{
    uint64_t t0 = dt_now();
    uint16_t id = ((fd >= 0) && (fd < DT_FDS)) ? sFd[fd].id : 0;
    int r = fs_close(fd);

    dt_add('C', t0, id, 0, 0, (int32_t)r);

    return r;
}

void dt_event(const char *what, int arg)
{
    dt_add('E', dt_now(), dt_intern(what), (uint32_t)arg, 0, 0);
}

/* ---- the drive's view: KOS's iso9660 calling cdrom.c ----
 * Reached by -Wl,--wrap= (disctrace.h); without it these are never called
 * and only the game's view is recorded. */

int __real_cdrom_read_sectors_ex(void *buffer, uint32_t sector, size_t cnt, bool dma);
int __real_cdrom_stream_start(int sector, int cnt, bool dma);
int __real_cdrom_stream_request(void *buffer, size_t size, bool block);

int __wrap_cdrom_read_sectors_ex(void *buffer, uint32_t sector, size_t cnt, bool dma)
{
    uint64_t t0 = dt_now();
    int r = __real_cdrom_read_sectors_ex(buffer, sector, cnt, dma);

    dt_add('K', t0, dma ? 1 : 0, sector, (uint32_t)cnt, r);

    return r;
}

int __wrap_cdrom_stream_start(int sector, int cnt, bool dma)
{
    uint64_t t0 = dt_now();
    int r = __real_cdrom_stream_start(sector, cnt, dma);

    dt_add('Z', t0, dma ? 1 : 0, (uint32_t)sector, (uint32_t)cnt, r);

    return r;
}

int __wrap_cdrom_stream_request(void *buffer, size_t size, bool block)
{
    uint64_t t0 = dt_now();
    int r = __real_cdrom_stream_request(buffer, size, block);

    dt_add('Q', t0, block ? 1 : 0, 0, (uint32_t)size, r);

    return r;
}

/* ---- the dump ---- */

void dt_dump(const char *scene)
{
    uint32_t n, i;
    uint64_t now = dt_now();
    int old = irq_disable();

    n = sCount;
    irq_restore(old);

    dbglog(DBG_WARNING, "dt: H %s %llu %lu %lu\n", scene,
           (unsigned long long)now, (unsigned long)n, (unsigned long)sDropped);
    for (i = 0; i < n; i++)
    {
        const DTRec *r = &sRec[i];

        if (!sThdPrinted[r->tid])
        {
            sThdPrinted[r->tid] = 1;
            dbglog(DBG_WARNING, "dt: T %u %s\n", (unsigned)r->tid,
                   sThdLabel[r->tid]);
        }
        if (((r->op == 'O') || (r->op == 'E') || (r->id != 0)) &&
            (r->id < DT_NAMES) && !sNamePrinted[r->id] &&
            (r->op != 'K') && (r->op != 'Z') && (r->op != 'Q'))
        {
            sNamePrinted[r->id] = 1;
            dbglog(DBG_WARNING, "dt: F %u %s\n", (unsigned)r->id, sName[r->id]);
        }
        dbglog(DBG_WARNING, "dt: %c %lu %lu %u %u %lu %lu %ld\n", (char)r->op,
               (unsigned long)r->t, (unsigned long)r->dur, (unsigned)r->tid,
               (unsigned)r->id, (unsigned long)r->off, (unsigned long)r->len,
               (long)r->ret);
    }
    /* what arrived while it printed goes to the front for the next dump */
    old = irq_disable();
    memmove(sRec, &sRec[n], (size_t)(sCount - n) * sizeof(sRec[0]));
    sCount -= n;
    sDropped = 0;
    irq_restore(old);
}

#endif
