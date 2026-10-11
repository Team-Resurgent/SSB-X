/* overlay_bounds_xbox.c -- the overlay noload segments' bounds.
 *
 * src/dc/overlay.c zeroes [__start_ovlN_noload, __stop_ovlN_noload) when a
 * scene loads overlay N, which is the N64's own reset of that overlay's
 * statics (sys/dma.c syDmaLoadOverlay). On the Dreamcast, objcopy renames
 * each such decomp object's .bss to ovlN_noload and GNU ld synthesizes the
 * two symbols around the orphan section.
 *
 * Here tools/xbox/gen_manifest.py compiles each of those objects under
 *     #pragma clang section bss=".bss$zzssbovlN_m"
 * and this file puts one marker before and one after, in "_a" and "_z". A
 * COFF linker merges every `.bss$suffix` input section into one output
 * section ordered by the full name, so the markers bracket exactly the
 * overlay's statics. Two things this depends on, both learnt from the
 * link map:
 *   - the markers must be made the SAME way as the statics (the pragma,
 *     not a section attribute): the two produced sections of different
 *     characteristics, and lld put them in different output sections;
 *   - the suffix must sort clear of the libraries' own `.bss$<symbol>`
 *     sections (they are compiled with -fdata-sections), hence "zzssbovl".
 * xbox_overlay_bounds_check() verifies the result at boot. */
#include <stddef.h>
#include <stdint.h>
#include <kos/dbglog.h>

#pragma clang section bss=".bss$zzssbovl1_a"
int __start_ovl1_noload[1];
#pragma clang section bss=".bss$zzssbovl1_z"
int __stop_ovl1_noload[1];
#pragma clang section bss=".bss$zzssbovl2_a"
int __start_ovl2_noload[1];
#pragma clang section bss=".bss$zzssbovl2_z"
int __stop_ovl2_noload[1];
#pragma clang section bss=".bss$zzssbovl3_a"
int __start_ovl3_noload[1];
#pragma clang section bss=".bss$zzssbovl3_z"
int __stop_ovl3_noload[1];
#pragma clang section bss=""

/* A bad link shows as a reversed or empty range; say so rather than clear
 * the wrong memory. Returns 0 when all three are sane. */
int xbox_overlay_bounds_check(void)
{
    static const struct { const int *a, *z; } r[3] = {
        { __start_ovl1_noload, __stop_ovl1_noload },
        { __start_ovl2_noload, __stop_ovl2_noload },
        { __start_ovl3_noload, __stop_ovl3_noload },
    };
    int i, bad = 0;

    for (i = 0; i < 3; i++)
    {
        uintptr_t a = (uintptr_t)r[i].a, z = (uintptr_t)r[i].z;

        if (z <= a || z - a > 4u * 1024u * 1024u)
        {
            dbglog(DBG_CRITICAL, "overlay %d: noload bounds %08lx..%08lx are not "
                   "a range -- the .bss$zzssbovl sections were not ordered\n", i + 1,
                   (unsigned long)a, (unsigned long)z);
            bad = 1;
        }
        else
        {
            dbglog(DBG_INFO, "overlay %d: noload %lu bytes at %08lx\n", i + 1,
                   (unsigned long)(z - a - sizeof(int)), (unsigned long)a);
        }
    }
    return bad ? -1 : 0;
}
