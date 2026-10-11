/* loadcensus.c -- see loadcensus.h. Only DB_LOAD_CENSUS builds have
 * anything in it. */
#include "loadcensus.h"

#if defined(DB_LOAD_CENSUS) && defined(_arch_dreamcast)

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <kos.h>

#include "sndres.h"
#include "taskman.h"

extern void *mm_sbrk(unsigned long increment);

typedef struct
{
    uint32_t t;
    uint16_t id;
    uint8_t op;
    uint8_t pad;
    uint32_t v[6];
} LCRec;

_Static_assert(sizeof(LCRec) == 32, "record size");

/* A node is a scene's whole life, load and play: a 1P rung with its
 * enemies' entrances runs to a few hundred records, a soak match fewer.
 * Peak samples are folded into one record at the end, not kept. */
#define LC_MAX      4096
#define LC_NAMES    1024
#define LC_NAME_LEN 64
/* how often the peak is sampled while a scene plays, in tics: mallinfo
 * walks the bins, which is cheap but not free */
#define LC_SAMPLE   8

static LCRec sRec[LC_MAX];
static uint32_t sCount, sDropped;
static char sName[LC_NAMES][LC_NAME_LEN];
static uint8_t sNamePrinted[LC_NAMES];
static uint16_t sNames = 1;

static uint32_t sNode;              /* 0 until the first node begins */
static uint32_t sTics, sFrames, sLoaded;
static uint32_t sPeak[6];
static uint32_t sPrintUs;           /* the last dump's own cost */
static uint32_t sLastFrameT;        /* when the node last drew */

uint32_t lc_now(void)
{
    return (uint32_t)timer_us_gettime64();
}

static uint16_t lc_intern(const char *s)
{
    uint16_t i;

    for (i = 1; i < sNames; i++)
    {
        if (strncmp(sName[i], s, LC_NAME_LEN - 1) == 0)
            return i;
    }
    if (sNames >= LC_NAMES)
        return 0;
    strncpy(sName[sNames], s, LC_NAME_LEN - 1);
    return sNames++;
}

static void lc_add(uint8_t op, uint32_t t, uint16_t id, uint32_t v0,
                   uint32_t v1, uint32_t v2, uint32_t v3, uint32_t v4,
                   uint32_t v5)
{
    int old = irq_disable();

    if (sCount < LC_MAX)
    {
        LCRec *r = &sRec[sCount++];

        r->t = t;
        r->id = id;
        r->op = op;
        r->v[0] = v0;
        r->v[1] = v1;
        r->v[2] = v2;
        r->v[3] = v3;
        r->v[4] = v4;
        r->v[5] = v5;
    }
    else
    {
        sDropped++;
    }
    irq_restore(old);
}

/* assetroot.c's pf_free and pf_top, measured the same way */
static void lc_mem(uint32_t m[6])
{
    struct mallinfo mi = mallinfo();
    long sbrk_room = (long)(_arch_mem_top - THD_KERNEL_STACK_SIZE) -
                     (long)(uintptr_t)mm_sbrk(0);

    m[0] = (uint32_t)mi.uordblks;
    m[1] = (uint32_t)(sbrk_room + (long)mi.fordblks);
    m[2] = (uint32_t)(sbrk_room + (long)mi.keepcost);
    m[3] = (uint32_t)syTaskmanGeneralHeapUsed();
    m[4] = (uint32_t)pvr_mem_available();
    m[5] = sndres_resident_bytes();
}

/* A peak is the most in use and the least free, each on its own: the
 * worst of each, which need not be one moment's. */
static void lc_fold(const uint32_t m[6])
{
    int i;

    for (i = 0; i < 6; i++)
    {
        int more_is_worse = (i == 0 || i == 3 || i == 5);

        if (more_is_worse ? (m[i] > sPeak[i]) : (m[i] < sPeak[i]))
            sPeak[i] = m[i];
    }
}

static void lc_snapshot(const char *at)
{
    uint32_t m[6];

    lc_mem(m);
    lc_fold(m);
    lc_add('M', lc_now(), lc_intern(at), m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void lc_dump(void)
{
    uint64_t now = timer_us_gettime64();
    uint32_t i;

    dbglog(DBG_WARNING, "lc: H %u %llu %u %u %u\n", (unsigned)sNode,
           (unsigned long long)now, (unsigned)sCount, (unsigned)sDropped,
           (unsigned)sPrintUs);
    for (i = 0; i < sCount; i++)
    {
        const LCRec *r = &sRec[i];

        if (r->id != 0 && !sNamePrinted[r->id])
        {
            sNamePrinted[r->id] = 1;
            dbglog(DBG_WARNING, "lc: n %u %s\n", (unsigned)r->id,
                   sName[r->id]);
        }
        dbglog(DBG_WARNING, "lc: %c %u %u %u %u %u %u %u %u\n", r->op,
               (unsigned)r->t, (unsigned)r->id, (unsigned)r->v[0],
               (unsigned)r->v[1], (unsigned)r->v[2], (unsigned)r->v[3],
               (unsigned)r->v[4], (unsigned)r->v[5]);
    }
    sCount = sDropped = 0;
    sPrintUs = (uint32_t)(timer_us_gettime64() - now);
}

void lc_node(int32_t kind, const char *name, int router)
{
    if (sNode != 0)
    {
        lc_snapshot("end");
        lc_add('M', lc_now(), lc_intern("peak"), sPeak[0], sPeak[1],
               sPeak[2], sPeak[3], sPeak[4], sPeak[5]);
        lc_add('E', lc_now(), 0, sTics, sFrames, sLastFrameT, 0, 0, 0);
        lc_dump();
    }
    sNode++;
    sTics = sFrames = sLoaded = 0;
    sPeak[0] = sPeak[3] = sPeak[5] = 0;
    sPeak[1] = sPeak[2] = sPeak[4] = 0xFFFFFFFFu;
    lc_add('N', lc_now(), lc_intern(name != NULL ? name : "?"),
           (uint32_t)kind, (uint32_t)router,
           (uint32_t)syTaskmanGeneralHeapSize(), 0, 0, 0);
    lc_snapshot("begin");
}

void lc_file(const char *path, uint32_t t0, uint32_t size, uint32_t bytes,
             uint32_t us, int src, uint32_t reads)
{
    uint32_t m[6];

    if (sNode == 0)
        return;
    /* a load's peak is between frames, where lc_frame cannot see it */
    lc_mem(m);
    lc_fold(m);
    lc_add('F', t0, lc_intern(path), size, bytes, us, (uint32_t)src,
           sLoaded ? 1u : 0u, reads);
}

void lc_sound(uint32_t fgm_bytes, uint32_t bgm_bytes, uint32_t us)
{
    if (sNode == 0)
        return;
    lc_add('S', lc_now() - us, 0, fgm_bytes, bgm_bytes, us, 0, 0, 0);
}

void lc_mark(const char *what, uint32_t value)
{
    if (sNode == 0)
        return;
    lc_add('X', lc_now(), lc_intern(what), value, 0, 0, 0, 0, 0);
}

void lc_frame(int drew)
{
    uint32_t m[6];

    if (sNode == 0)
        return;
    sTics++;
    if (drew)
    {
        sFrames++;
        sLastFrameT = lc_now();
    }
    if (drew && !sLoaded)
    {
        sLoaded = 1;
        lc_add('L', lc_now(), 0, sTics, 0, 0, 0, 0, 0);
        lc_snapshot("loaded");
        return;
    }
    if ((sTics % LC_SAMPLE) == 0)
    {
        lc_mem(m);
        lc_fold(m);
    }
}

#endif
