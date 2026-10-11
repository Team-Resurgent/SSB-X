/* primdump.c -- see primdump.h. Empty without -DDB_PRIM_DUMP, which is the
 * console's; the host baker (FT_BAKER) writes the same lines to a
 * file, over the address space hoststubs/bakepvr.c maps. */
#if defined(DB_PRIM_DUMP) || defined(FT_BAKER)
#ifdef FT_BAKER
#include <kos.h>                /* hoststubs/kos.h */
FILE *gPrimDumpOut;
#define PD_LOG(...) fprintf(gPrimDumpOut, __VA_ARGS__)
#else
#include <kos.h>
#define PD_LOG(...) dbglog(DBG_INFO, __VA_ARGS__)
#endif
#include <dc/pvr.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "primdump.h"

#define PD_MAX_UNITS (128 * 1024)       /* 4 MB of 32-byte units */
#define PD_TEX_MAX 512

#ifdef DB_PRIM_DUMP
int __real_pvr_prim(const void *data, size_t size);
#endif

static uint8_t *sBuf;
static size_t sUnits;
static int sOn, sDone, sOverflow;

void primdump_record(const void *data, size_t size)
{
    if (sOn && (size % 32) == 0)
    {
        if (sUnits + size / 32 <= PD_MAX_UNITS)
        {
            memcpy(sBuf + sUnits * 32, data, size);
            sUnits += size / 32;
        }
        else
        {
            sOverflow = 1;
        }
    }
}

#ifdef DB_PRIM_DUMP
int __wrap_pvr_prim(const void *data, size_t size)
{
    primdump_record(data, size);
    return __real_pvr_prim(data, size);
}
#endif

static void hex(const uint8_t *p, int n, char *out)
{
    static const char d[] = "0123456789abcdef";
    int i;

    for (i = 0; i < n; i++)
    {
        out[2 * i] = d[p[i] >> 4];
        out[2 * i + 1] = d[p[i] & 15];
    }
    out[2 * n] = 0;
}

/* the bytes of the texture a header names, from the size and format words */
static uint32_t tex_bytes(uint32_t m2, uint32_t m3)
{
    uint32_t w = 8u << ((m2 >> 3) & 7), h = 8u << (m2 & 7);
    uint32_t fmt = (m3 >> 27) & 7;

    if ((m3 >> 30) & 1)
    {
        return 2048 + w * h / 4;
    }
    if (fmt == 5)
    {
        return w * h / 2;
    }
    if (fmt == 6)
    {
        return w * h;
    }
    return w * h * 2;
}

static void flush(int tic)
{
    uint32_t seen[PD_TEX_MAX][2];
    int nseen = 0;
    size_t i;
    char line[600];

    PD_LOG("primdump: begin %d %u%s\n", tic, (unsigned)sUnits,
           sOverflow ? " OVERFLOW" : "");
    for (i = 0; i < sUnits; i += 4)
    {
        int n = (sUnits - i < 4) ? (int)(sUnits - i) : 4;

        hex(sBuf + i * 32, n * 32, line);
        PD_LOG("pd %u %s\n", (unsigned)i, line);
    }
    for (i = 0; i < sUnits; i++)
    {
        const uint32_t *u = (const uint32_t *)(sBuf + i * 32);
        uint32_t off, len, o;
        int k, dup = 0;

        if ((u[0] >> 29) != 4 || !(u[0] & 8))     /* header, textured */
        {
            continue;
        }
        off = (u[3] & 0x1FFFFF) << 3;
        len = tex_bytes(u[2], u[3]);
        if (u[3] >> 31)
        {
            dbglog(DBG_WARNING, "primdump: mipmapped texture at %x\n", (unsigned)off);
        }
        for (k = 0; k < nseen; k++)
        {
            dup |= (seen[k][0] == off && seen[k][1] == len);
        }
        if (dup || nseen == PD_TEX_MAX)
        {
            continue;
        }
        seen[nseen][0] = off;
        seen[nseen][1] = len;
        nseen++;
        PD_LOG("pdt %u %u\n", (unsigned)off, (unsigned)len);
        for (o = 0; o < len; o += 64)
        {
            hex((const uint8_t *)(0xA4000000u + off + o),
                (len - o < 64) ? (int)(len - o) : 64, line);
            PD_LOG("pdx %u %s\n", (unsigned)(off + o), line);
        }
    }
    for (i = 0; i < 1024; i += 8)
    {
        hex((const uint8_t *)(0xA05F9000u + i * 4), 32, line);
        PD_LOG("pdp %u %s\n", (unsigned)i, line);
    }
    PD_LOG("primdump: pal cfg %u\n",
           (unsigned)(*(volatile uint32_t *)0xA05F8108u & 3));
    PD_LOG("primdump: end %d\n", tic);
}

#ifdef FT_BAKER
void primdump_start(void)
{
    if (sBuf == NULL)
    {
        sBuf = malloc((size_t)PD_MAX_UNITS * 32);
    }
    sUnits = 0;
    sOverflow = 0;
    sOn = 1;
}

void primdump_finish(int tic)
{
    sOn = 0;
    flush(tic);
}
#else
void primdump_track(int tic)
{
    if (sDone)
    {
        return;
    }
    if (tic == DB_PRIM_DUMP)
    {
        if (sBuf == NULL)
        {
            sBuf = malloc((size_t)PD_MAX_UNITS * 32);
        }
        sOn = (sBuf != NULL);
    }
    else if (sOn)
    {
        sOn = 0;
        sDone = 1;
        flush(DB_PRIM_DUMP);
    }
}
#endif
#endif /* DB_PRIM_DUMP || FT_BAKER */
