/* textguard.c -- see textguard.h. */
#include "textguard.h"

#if defined(DB_TEXT_GUARD) && defined(_arch_dreamcast)
#include <stdint.h>

#include <kos.h>

#define TG_BLOCK   1024u
#define TG_WORDS   (TG_BLOCK / 4u)
#define TG_BLOCKS  2048              /* 2 MB of code and constants, at most */
#define TEXT_GUARD_REPORTS 16

/* The linker's (KOS's shlelf.xc): the first writable byte after
 * .rodata and .eh_frame. _executable_start, the first of .text, is
 * declared by <arch/arch.h>. */
extern char _tdata_start;

/* KOS's interrupt stack: the 4096 bytes under krn_stack, which is
 * irq_save_regs's address (kernel/arch/dreamcast/kernel/entry.s). It sits
 * in .text, so its words are left out of the sums; they are painted
 * instead, and the report says how deep interrupts have reached. Past
 * the bottom is code, which the sums do cover: an overflow shows up as
 * the block under TG_IRQ_LO changing. */
extern char irq_save_regs;
#define TG_IRQ_STACK 4096u
#define TG_IRQ_HI    ((uintptr_t)&irq_save_regs)
#define TG_IRQ_LO    (TG_IRQ_HI - TG_IRQ_STACK)
#define TG_PAINT     0xA5C3A5C3u

/* And one variable KOS keeps in .text (entry.s): the running thread's
 * register save area, rewritten at every context switch. The frame end
 * runs in whichever thread the scene is on, so it differs from one check
 * to the next (the first hardware run reported it at frame 15). The
 * other words KOS's assembly keeps in .text are constants, or written
 * only at startup, by arch_exec, or on a TLB miss (the MMU is off). */
extern void *irq_srt_addr;
#define TG_SRT       ((uintptr_t)&irq_srt_addr)

static int tg_skipped(uintptr_t wa)
{
    return (wa >= TG_IRQ_LO && wa < TG_IRQ_HI) || wa == TG_SRT;
}

extern uint32_t dSYTaskmanUpdateCount;   /* sys/taskman.h, u32 */

static uint32_t sTGSum[TG_BLOCKS];
static unsigned sTGCount;
static unsigned sTGNext;
static unsigned sTGReports;
static unsigned sTGSweeps;
static int sTGReady;

/* the block's words through P2, which bypasses the operand cache */
static const volatile uint32_t *tg_block(unsigned b)
{
    uintptr_t a = (uintptr_t)&_executable_start + (uintptr_t)b * TG_BLOCK;

    return (const volatile uint32_t *)((a & 0x1FFFFFFFu) | 0xA0000000u);
}

static uint32_t tg_sum(unsigned b)
{
    const volatile uint32_t *w = tg_block(b);
    uintptr_t a = (uintptr_t)&_executable_start + (uintptr_t)b * TG_BLOCK;
    uint32_t s = 0x5EED1234u;
    unsigned i;

    if ((a + TG_BLOCK <= TG_IRQ_LO || a >= TG_IRQ_HI) &&
        (TG_SRT < a || TG_SRT >= a + TG_BLOCK))
    {
        for (i = 0; i < TG_WORDS; i++)
            s = ((s << 5) | (s >> 27)) + w[i] + i;
        return s;
    }
    /* a block holding part of the interrupt stack or irq_srt_addr:
     * the rest of it only */
    for (i = 0; i < TG_WORDS; i++)
    {
        if (!tg_skipped(a + i * 4u))
            s = ((s << 5) | (s >> 27)) + w[i] + i;
    }
    return s;
}

/* bytes of the interrupt stack used since the paint, from the top */
static unsigned tg_irq_depth(void)
{
    const uint32_t *w = (const uint32_t *)TG_IRQ_LO;
    unsigned i, n = TG_IRQ_STACK / 4u;

    for (i = 0; i < n && w[i] == TG_PAINT; i++)
        ;
    return TG_IRQ_STACK - i * 4u;
}

static void tg_init(void)
{
    uintptr_t lo = (uintptr_t)&_executable_start;
    uintptr_t hi = (uintptr_t)&_tdata_start;
    uint32_t *w;
    unsigned b;
    int old;

    /* whole blocks only: the tail past the last one is .eh_frame's end,
     * and the block that would straddle .data would change with it */
    sTGCount = (unsigned)((hi - lo) / TG_BLOCK);
    if (sTGCount > TG_BLOCKS)
        sTGCount = TG_BLOCKS;
    for (b = 0; b < sTGCount; b++)
        sTGSum[b] = tg_sum(b);
    /* nothing is on the interrupt stack outside an interrupt */
    old = irq_disable();
    for (w = (uint32_t *)TG_IRQ_LO; w < (uint32_t *)TG_IRQ_HI; w++)
        *w = TG_PAINT;
    irq_restore(old);
    sTGReady = 1;
    dbglog(DBG_INFO, "text guard: %u blocks, %p to %p, summed; interrupt "
           "stack %p to %p painted\n", sTGCount, (void *)lo,
           (void *)(lo + sTGCount * TG_BLOCK), (void *)TG_IRQ_LO,
           (void *)TG_IRQ_HI);
}

void __attribute__((noinline)) text_guard_failed(void)
{
    __asm__ volatile ("" ::: "memory");
}

static void tg_report(unsigned b, uint32_t now, const char *where)
{
    const volatile uint32_t *w = tg_block(b);
    uintptr_t a = (uintptr_t)&_executable_start + (uintptr_t)b * TG_BLOCK;
    unsigned i;

    dbglog(DBG_ERROR, "text guard: %s: block %08x-%08x changed at frame "
           "%lu (sweep %u), sum %08x was %08x -- its words follow\n",
           where, (unsigned)a, (unsigned)(a + TG_BLOCK),
           (unsigned long)dSYTaskmanUpdateCount, sTGSweeps,
           (unsigned)now, (unsigned)sTGSum[b]);
    for (i = 0; i < TG_WORDS; i += 8)
        dbglog(DBG_ERROR, "text guard:  %08x: %08x %08x %08x %08x %08x "
               "%08x %08x %08x\n", (unsigned)(a + i * 4u),
               (unsigned)w[i], (unsigned)w[i + 1], (unsigned)w[i + 2],
               (unsigned)w[i + 3], (unsigned)w[i + 4], (unsigned)w[i + 5],
               (unsigned)w[i + 6], (unsigned)w[i + 7]);
    text_guard_failed();
}

#ifdef DB_TEXT_GUARD_SELFTEST
/* -DDB_TEXT_GUARD_SELFTEST: one bit of this .rodata string flipped
 * through P2 at frame 300, which the guard has to report within a sweep */
static const char kTGSelfTest[] = "text guard self-test: this string is flipped";
#endif

int text_guard_check(const char *where, unsigned blocks)
{
    unsigned n;
    int r = 0, all;

    if (!sTGReady)
        tg_init();
#ifdef DB_TEXT_GUARD_SELFTEST
    static int flipped;

    if (dSYTaskmanUpdateCount == 300 && !flipped++)
    {
        volatile uint32_t *w = (volatile uint32_t *)
            ((((uintptr_t)kTGSelfTest + 3u) & ~3u & 0x1FFFFFFFu) | 0xA0000000u);

        dbglog(DBG_INFO, "text guard: self-test flips a bit at %p\n",
               (void *)kTGSelfTest);
        *w ^= 0x00010000u;
    }
#endif
    all = blocks == 0 || blocks >= sTGCount;
    if (all)
        blocks = sTGCount;
    for (n = 0; n < blocks; n++)
    {
        unsigned b = sTGNext;
        uint32_t now = tg_sum(b);

        if (++sTGNext == sTGCount)
        {
            sTGNext = 0;
            sTGSweeps++;
        }
        if (now == sTGSum[b])
            continue;
        r = -1;
        if (sTGReports < TEXT_GUARD_REPORTS)
        {
            sTGReports++;
            tg_report(b, now, where);
        }
        sTGSum[b] = now;
    }
    if (all)
        dbglog(DBG_INFO, "text guard: %s -- %s, %u sweeps, %u reported; "
               "interrupt stack deepest %u of %u\n", where,
               r == 0 ? "clean" : "CHANGED", sTGSweeps, sTGReports,
               tg_irq_depth(), TG_IRQ_STACK);
    return r;
}
#endif
