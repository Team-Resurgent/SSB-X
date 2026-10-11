/* bakepvr.c -- the console the BAKER build (make BAKER=1) draws
 * on: the PVR's address space, mapped, and the pvr_* the port's draw half
 * calls, recorded instead of sent to a TA.
 *
 * KOS's own <dc/pvr.h> is what the build compiles against (Makefile,
 * -idirafter), so a poly header and a vertex are the console's bytes. What
 * makes the rest work unchanged is that this is a 32-bit process: the port
 * writes texels and palette entries through the addresses the SH-4 uses
 * (video RAM at 0xA4000000, the registers and palette RAM at 0xA05F8000), so
 * those pages are mapped here and the writes land. Nothing else fakes them.
 *
 * pvr_poly_compile and pvr_poly_cxt_* are KOS's own source, included below;
 * pvr_prim keeps what it is given (src/dc/primdump.c), which
 * tools/lib/pvrsoft.py draws.
 */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#include <dc/pvr.h>
#include <arch/arch.h>

#include "primdump.h"

#define VRAM_BASE 0xA4000000u
#define VRAM_SIZE (8u * 1024 * 1024)
#define REG_BASE 0xA05F8000u
#define REG_SIZE 0x2000u

static void map_fixed(uintptr_t at, size_t len, const char *what)
{
    void *p = mmap((void *)at, len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);

    if (p != (void *)at)
    {
        fprintf(stderr, "bakepvr: cannot map %s at %#lx (needs a 32-bit build)\n",
                what, (unsigned long)at);
        exit(2);
    }
}

__attribute__((constructor)) static void bakepvr_map(void)
{
    map_fixed(VRAM_BASE, VRAM_SIZE, "video RAM");
    map_fixed(REG_BASE, REG_SIZE, "PVR registers");
}

/* ---- video RAM: a first-fit allocator over the mapped pages ---------------
 * The console's textures start above its vertex buffers; the offsets a
 * texture gets do not matter to the picture, only that they are distinct
 * and 32-byte aligned. Frees are kept so a scene's release and reload
 * reuse the room. */
typedef struct
{
    uint32_t off, size;
    int used;
} Block;

static Block sBlocks[4096];
static int sNBlocks;

pvr_ptr_t pvr_mem_malloc(size_t size)
{
    uint32_t sz = (uint32_t)((size + 31) & ~31u);
    uint32_t at = 0x00100000;
    int i;

    for (i = 0; i < sNBlocks; i++)
    {
        if (!sBlocks[i].used && sBlocks[i].size >= sz)
        {
            sBlocks[i].used = 1;
            return (pvr_ptr_t)(uintptr_t)(VRAM_BASE + sBlocks[i].off);
        }
        at = sBlocks[i].off + sBlocks[i].size;
    }
    if (sNBlocks == 4096 || at + sz > VRAM_SIZE)
    {
        return NULL;
    }
    sBlocks[sNBlocks].off = at;
    sBlocks[sNBlocks].size = sz;
    sBlocks[sNBlocks].used = 1;
    sNBlocks++;
    return (pvr_ptr_t)(uintptr_t)(VRAM_BASE + at);
}

void pvr_mem_free(pvr_ptr_t p)
{
    int i;

    for (i = 0; i < sNBlocks; i++)
    {
        if (VRAM_BASE + sBlocks[i].off == (uintptr_t)p)
        {
            sBlocks[i].used = 0;
            return;
        }
    }
}

void pvr_txr_load(const void *src, pvr_ptr_t dst, size_t count)
{
    memcpy((void *)dst, src, count);
}

/* PVR_PALETTE_CFG is the register at REG_BASE + 0x108 */
void pvr_set_pal_format(pvr_palfmt_t fmt)
{
    *(volatile uint32_t *)(REG_BASE + 0x108) = (uint32_t)fmt;
}

int pvr_prim(const void *data, size_t size)
{
    primdump_record(data, size);
    return 0;
}

/* the fence is for a renderer that is not here */
void dc_pvr_vram_fence(void) {}

/* ---- KOS's polygon compiler, verbatim -------------------------------------
 * The Makefile cuts pvr_poly_compile and pvr_poly_cxt_col/txr out of KOS's
 * pvr_prim.c (between its "Compile a polygon context" and "untextured sprite
 * context" comments) into kos_poly.inc, so the header the port compiles here
 * is the header KOS compiles there. The sprite and modifier compilers, and
 * the driver's internal header they sit beside, are not needed. */
#define dcache_alloc_line_with_value(dst, v) \
    do { memset((dst), 0, 32); *(uint32_t *)(dst) = (v); } while (0)
#define assert_msg(c, m) assert((c) && (m))
#include "kos_poly.inc"

/* ---- the rest of what the link names ------------------------------------ */
void arch_abort(void) { abort(); }

uint64_t timer_us_gettime64(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

/* src/dc/stage.c is the stage renderer, which a crowd does not need; what
 * objdisplay.c and friends call of it answers "no stage". */
void *stage_bound(void) { return NULL; }
void *stage_map_anim(const char *name) { (void)name; return NULL; }
void *stage_bind_map_model(void *g, int l) { (void)g; (void)l; return NULL; }
void *stage_bind_map_object(void *g, int i, int l) { (void)g; (void)i; (void)l; return NULL; }
void *stage_bind_map_cloud(void *g, void *p, int a) { (void)g; (void)p; (void)a; return NULL; }
void stage_cloud_set_anim(void *d, int a) { (void)d; (void)a; }
int stage_platform_pack(void *s, const char *n) { (void)s; (void)n; return -1; }
void *stage_map_joint(int o, int i) { (void)o; (void)i; return NULL; }

/* PVR_RAM_SIZE asks whether this is a retail console (8 MB) */
int hardware_sys_mode(int *region)
{
    if (region != NULL)
    {
        *region = 0;
    }
    return HW_TYPE_RETAIL;
}
