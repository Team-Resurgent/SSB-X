/* dma.c -- see dma.h. */
#include <string.h>

#include <ssb_types.h>
#include <sys/dma.h>

#include <stdlib.h>
#include <string.h>
#ifdef _arch_dreamcast
#include <malloc.h>
#endif

#include "assetroot.h"
#include "dma.h"

#ifdef _arch_dreamcast
#include <kos.h>
#else
#include <stdio.h>
#define dbglog(lvl, ...) fprintf(stderr, __VA_ARGS__)
#define DBG_ERROR 0
#endif

/* The eighteen particle-bank segments of smashbrothers.us.yaml, and the
 * files tools/export/ssb_particleexport.py cut them into. Kept in ROM order,
 * so the lookup below can stop early; tools/check/particle_check.py checks the
 * addresses against the yaml and against the decomp's own
 * symbols/linker_constants.txt. */
typedef struct SYDmaRomRange
{
    uintptr_t lo;
    uintptr_t hi;
    const char *name;
} SYDmaRomRange;

static const SYDmaRomRange sSYDmaRomRanges[] = {
    { 0x00AC7340, 0x00AC9DE0, "efcommon.scb"       },
    { 0x00AC9DE0, 0x00B16C80, "efcommon.txb"       },
    { 0x00B16C80, 0x00B17060, "particles_unk0.scb" },
    { 0x00B17060, 0x00B174A0, "particles_unk0.txb" },
    { 0x00B174A0, 0x00B176A0, "particles_unk1.scb" },
    { 0x00B176A0, 0x00B19700, "particles_unk1.txb" },
    { 0x00B19700, 0x00B19850, "particles_unk2.scb" },
    { 0x00B19850, 0x00B1BCA0, "particles_unk2.txb" },
    { 0x00B1BCA0, 0x00B1BDE0, "itcommon.scb"       },
    { 0x00B1BDE0, 0x00B1E640, "itcommon.txb"       },
    { 0x00B1E640, 0x00B1E7E0, "grpupupu.scb"       },
    { 0x00B1E7E0, 0x00B1F960, "grpupupu.txb"       },
    { 0x00B1F960, 0x00B1FC80, "grhyrule.scb"       },
    { 0x00B1FC80, 0x00B22980, "grhyrule.txb"       },
    { 0x00B22980, 0x00B22A00, "gryoster.scb"       },
    { 0x00B22A00, 0x00B22C30, "gryoster.txb"       },
    { 0x00B22C30, 0x00B22D40, "mntitle.scb"        },
    { 0x00B22D40, 0x00B277B0, "mntitle.txb"        },
};

static uint32_t sSYDmaReads;
static uint32_t sSYDmaBytes;

static const SYDmaRomRange *sy_dma_find(uintptr_t rom_src)
{
    size_t i;

    for (i = 0; i < sizeof(sSYDmaRomRanges) / sizeof(sSYDmaRomRanges[0]); i++)
    {
        const SYDmaRomRange *r = &sSYDmaRomRanges[i];

        if (rom_src >= r->lo && rom_src < r->hi)
        {
            return r;
        }
    }
    return NULL;
}

/* The bounce buffer for a destination the DMA stream cannot be given.
 * 32 KB: sixteen sectors a request, so efcommon.txb is ten requests and
 * one memcpy rather than 154 sector reads through the cache. */
#define SY_DMA_BOUNCE   (32 * 1024)

static void *sy_dma_bounce_alloc(void)
{
#ifdef _arch_dreamcast
    return memalign(32, SY_DMA_BOUNCE);
#else
    return malloc(SY_DMA_BOUNCE);
#endif
}

/* sys/dma.c:116 syDmaReadRom. DIVERGES: the PI transfer becomes a read
 * out of the file the range was exported to. The destination is the
 * game's own syTaskmanMalloc block, 8-aligned rather than the 32 the
 * GD-ROM's stream path wants (src/dc/assetroot.h), so the read goes
 * through a 32-byte-aligned bounce buffer a chunk at a time and is
 * copied out: the first chunk begins at offset zero of the file, which
 * is what starts the stream, and each after it begins on a multiple of
 * the chunk, which keeps it. Read straight into the block,
 * efcommon.txb's 315 KB would go sector by sector through the cache;
 * through the bounce it is ten requests and a memcpy. A destination that happens to be aligned is
 * read straight. sy_dma_reads is the number either way. */
void syDmaReadRom(uintptr_t rom_src, void *ram_dst, size_t size)
{
    const SYDmaRomRange *r = sy_dma_find(rom_src);
    AssetFile af;
    long got;
    uint8_t *bounce = NULL;

    /* A failed read leaves the destination zero rather than whatever the
     * scene heap last held. The N64's DMA cannot fail this way, so there
     * is no game behaviour to be faithful to -- and a bank of zeroes is a
     * bank of no scripts, which the game's own loops handle, where a
     * block of stale heap is 119 pointers into nothing. */
    if (r == NULL)
    {
        dbglog(DBG_ERROR, "dma: no rom range holds %u..%u -- read dropped\n",
               (unsigned)rom_src, (unsigned)(rom_src + size));
        memset(ram_dst, 0, size);
        return;
    }
    if (asset_open(&af, r->name) != 0)
    {
        dbglog(DBG_ERROR, "dma: %s is not there\n", r->name);
        memset(ram_dst, 0, size);
        return;
    }
    if (asset_seek(&af, (long)(rom_src - r->lo)) != 0)
    {
        dbglog(DBG_ERROR, "dma: %s: seek to %u failed\n", r->name,
               (unsigned)(rom_src - r->lo));
        asset_close(&af);
        memset(ram_dst, 0, size);
        return;
    }
    if (((uintptr_t)ram_dst & 31) == 0 ||
        (bounce = sy_dma_bounce_alloc()) == NULL)
    {
        got = asset_read(&af, ram_dst, (long)size);
    }
    else
    {
        uint8_t *dst = ram_dst;
        size_t left = size;

        got = 0;

        while (left > 0)
        {
            long want = (long)((left > SY_DMA_BOUNCE) ? SY_DMA_BOUNCE : left);
            long n = asset_read(&af, bounce, want);

            if (n <= 0)
            {
                break;
            }
            memcpy(dst, bounce, (size_t)n);
            dst += n;
            left -= (size_t)n;
            got += n;

            if (n != want)
            {
                break;
            }
        }
        free(bounce);
    }
    asset_close(&af);

    /* This is also what refuses a read that runs off the end of its
     * segment. On the N64 that is simply the next bytes of the same ROM;
     * here each segment is its own file, so the read comes up short and
     * nothing is half-served. */
    if (got != (long)size)
    {
        dbglog(DBG_ERROR, "dma: %s: %ld of %u bytes\n", r->name, got,
               (unsigned)size);
        memset(ram_dst, 0, size);
        return;
    }
    sSYDmaReads++;
    sSYDmaBytes += (uint32_t)size;
}

uint32_t sy_dma_reads(uint32_t *bytes)
{
    if (bytes != NULL)
    {
        *bytes = sSYDmaBytes;
    }
    return sSYDmaReads;
}
