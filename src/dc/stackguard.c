/* stackguard.c -- see stackguard.h. */
#include "stackguard.h"

#if defined(DB_STACK_GUARD) && defined(_arch_dreamcast)
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <kos.h>
#include <arch/stack.h>

#define SG_BAND_WORD  0x5754A4D5u   /* the band: never a pointer, count or float */
#define SG_PAINT_WORD 0xA5C3A5C3u   /* the stack, until something uses it */
#define SG_SLOTS      64
#define SG_KINDS      16

typedef struct
{
    uint8_t *stack;             /* lowest usable byte; the band is below */
    size_t size;
    const char *label;
    const void *owner;
    int bad;
} SGEnt;

/* the deepest each label has gone, over every stack that ever had it */
typedef struct
{
    const char *label;
    size_t size;
    size_t deepest;
    const void *deepest_owner;
    unsigned made;
} SGKind;

static SGEnt sSG[SG_SLOTS];
static SGKind sSGKind[SG_KINDS];
static int sSGMainPainted;
static int sSGMainBad;

static uint8_t *sg_main_lo(void)
{
    return (uint8_t *)(_arch_mem_top - THD_KERNEL_STACK_SIZE);
}

/* bytes used, counted from the top: the first painted word that changed */
static size_t sg_depth(const uint8_t *stack, size_t size)
{
    const uint32_t *w = (const uint32_t *)stack;
    size_t i, n = size / 4u;

    for (i = 0; i < n && w[i] == SG_PAINT_WORD; i++)
        ;
    return size - i * 4u;
}

static void sg_note_depth(const char *label, size_t size, size_t depth,
                          const void *owner, int made)
{
    int i;

    for (i = 0; i < SG_KINDS; i++)
    {
        if (sSGKind[i].label == NULL || sSGKind[i].label == label)
            break;
    }
    if (i == SG_KINDS)
        return;
    sSGKind[i].label = label;
    sSGKind[i].size = size;
    sSGKind[i].made += (unsigned)made;
    if (sSGKind[i].deepest_owner == NULL)
        sSGKind[i].deepest_owner = owner;
    if (depth > sSGKind[i].deepest)
    {
        sSGKind[i].deepest = depth;
        sSGKind[i].deepest_owner = owner;
    }
}

/* 0 when the band is whole; else the report, once for this entry */
static int sg_check_ent(SGEnt *e, const char *where)
{
    const uint32_t *band = (const uint32_t *)(e->stack - STACK_GUARD_BAND);
    unsigned i, n = STACK_GUARD_BAND / 4u, low = n, changed = 0;

    for (i = 0; i < n; i++)
    {
        if (band[i] != SG_BAND_WORD)
        {
            if (low == n)
                low = i;
            changed++;
        }
    }
    if (low == n)
        return 0;
    if (!e->bad)
    {
        e->bad = 1;
        /* how far below the stack it reached: the lowest changed word; a
         * return address there (0x8c0...) names a function in the chain */
        dbglog(DBG_ERROR, "stack guard: %s: %s stack %p (%u bytes, owner "
               "%p) overflowed -- %u words of its guard band written, the "
               "lowest %u bytes below the stack, holding %08x\n", where,
               e->label, e->stack, (unsigned)e->size, e->owner, changed,
               (unsigned)(STACK_GUARD_BAND - low * 4u), (unsigned)band[low]);
    }
    return -1;
}

void *stack_guard_alloc(size_t size, const char *label, const void *owner)
{
    uint8_t *raw;
    uint32_t *w;
    size_t i;
    int k, old;

    size = (size + 31u) & ~(size_t)31u;
    raw = memalign(32, STACK_GUARD_BAND + size);
    if (raw == NULL)
        return NULL;
    w = (uint32_t *)raw;
    for (i = 0; i < STACK_GUARD_BAND / 4u; i++)
        w[i] = SG_BAND_WORD;
    for (; i < (STACK_GUARD_BAND + size) / 4u; i++)
        w[i] = SG_PAINT_WORD;

    old = irq_disable();
    for (k = 0; k < SG_SLOTS; k++)
    {
        if (sSG[k].stack == NULL)
        {
            sSG[k].size = size;
            sSG[k].label = label;
            sSG[k].owner = owner;
            sSG[k].bad = 0;
            sSG[k].stack = raw + STACK_GUARD_BAND;
            break;
        }
    }
    irq_restore(old);
    if (k == SG_SLOTS)
        dbglog(DBG_WARNING, "stack guard: table full, %s stack %p unwatched\n",
               label, raw + STACK_GUARD_BAND);
    sg_note_depth(label, size, 0, owner, 1);
    return raw + STACK_GUARD_BAND;
}

void stack_guard_free(void *stack)
{
    int k, old;
    SGEnt e;

    if (stack == NULL)
        return;
    old = irq_disable();
    for (k = 0; k < SG_SLOTS; k++)
    {
        if (sSG[k].stack == stack)
            break;
    }
    if (k < SG_SLOTS)
    {
        e = sSG[k];
        sSG[k].stack = NULL;
    }
    irq_restore(old);
    if (k < SG_SLOTS)
    {
        sg_check_ent(&e, "stack freed");
        sg_note_depth(e.label, e.size, sg_depth(e.stack, e.size), e.owner, 0);
    }
    free((uint8_t *)stack - STACK_GUARD_BAND);
}

int stack_guard_check_one(void *stack, const char *where)
{
    int k;

    for (k = 0; k < SG_SLOTS; k++)
    {
        if (sSG[k].stack == stack)
            return sg_check_ent(&sSG[k], where);
    }
    return 0;
}

int stack_guard_check(const char *where)
{
    int k, r = 0;

    for (k = 0; k < SG_SLOTS; k++)
    {
        if (sSG[k].stack != NULL && sg_check_ent(&sSG[k], where) < 0)
            r = -1;
    }
    /* the main thread's own bottom STACK_GUARD_BAND bytes: under them is
     * the heap's limit (KOS mm_sbrk stops there), so paint reaching
     * this far down is a main stack about to run into the heap */
    if (sSGMainPainted)
    {
        const uint32_t *w = (const uint32_t *)sg_main_lo();
        unsigned i;

        for (i = 0; i < STACK_GUARD_BAND / 4u; i++)
        {
            if (w[i] != SG_PAINT_WORD)
            {
                if (!sSGMainBad)
                {
                    sSGMainBad = 1;
                    dbglog(DBG_ERROR, "stack guard: %s: main stack reached "
                           "%u bytes of the heap's limit (%p)\n", where,
                           i * 4u, (void *)sg_main_lo());
                }
                r = -1;
                break;
            }
        }
    }
    return r;
}

void stack_guard_report(const char *where)
{
    int k;

    /* the live stacks' depths go into their kinds first */
    for (k = 0; k < SG_SLOTS; k++)
    {
        if (sSG[k].stack != NULL)
            sg_note_depth(sSG[k].label, sSG[k].size,
                          sg_depth(sSG[k].stack, sSG[k].size),
                          sSG[k].owner, 0);
    }
    if (sSGMainPainted)
        dbglog(DBG_INFO, "stack: %s -- main deepest %u of %u\n", where,
               (unsigned)sg_depth(sg_main_lo(), THD_KERNEL_STACK_SIZE),
               (unsigned)THD_KERNEL_STACK_SIZE);
    for (k = 0; k < SG_KINDS && sSGKind[k].label != NULL; k++)
    {
        dbglog(DBG_INFO, "stack: %s -- %s deepest %u of %u (owner %p), "
               "%u made\n", where, sSGKind[k].label,
               (unsigned)sSGKind[k].deepest, (unsigned)sSGKind[k].size,
               sSGKind[k].deepest_owner, sSGKind[k].made);
    }
}

void stack_guard_paint_main(void)
{
    uint32_t *w = (uint32_t *)sg_main_lo();
    uint32_t *sp = (uint32_t *)__builtin_frame_address(0);

    /* stop well short of this frame: nothing lives below the stack
     * pointer (KOS takes interrupts on a stack of its own) */
    sp -= 64;
    if ((uint8_t *)sp <= (uint8_t *)w)
        return;
    while (w < sp)
        *w++ = SG_PAINT_WORD;
    sSGMainPainted = 1;
}

kthread_t *stack_guard_thd_create(int detach, void *(*routine)(void *),
                                  void *param, const char *label)
{
    kthread_attr_t attr;
    kthread_t *t;

    memset(&attr, 0, sizeof(attr));
    attr.create_detached = detach != 0;
    attr.stack_size = THD_STACK_SIZE;
    attr.stack_ptr = stack_guard_alloc(THD_STACK_SIZE, label,
                                       (const void *)routine);
    if (attr.stack_ptr == NULL)
        return NULL;
    t = thd_create_ex(&attr, routine, param);
    /* KOS does not own the stack, so a thread that ends leaves it here,
     * still watched: a leak of THD_STACK_SIZE per ended thread, in a
     * debug build, for threads that end only at power-off */
    if (t == NULL)
        stack_guard_free(attr.stack_ptr);
    return t;
}
#endif
