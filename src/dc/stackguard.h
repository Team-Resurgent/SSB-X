/* stackguard.h -- -DDB_STACK_GUARD (src/dc/db.h): thread stacks the port
 * owns, with a guard band under each, so a stack that overflows into the
 * heap is reported in the frame it happened in.
 *
 * KOS allocates a thread's stack with aligned_alloc from the same heap as
 * everything else and puts nothing under it; its only check is at a
 * context switch, on the saved SP (external/kos/kernel/thread/thread.c
 * thd_schedule_inner), so a deep call that comes back up before the
 * thread yields leaves no trace but the chunk it wrote over. Under this
 * flag every thread the port creates gets its stack from here instead:
 *
 *   [guard band, STACK_GUARD_BAND bytes][stack, painted]
 *
 * The band and the stack are filled with two different words when the
 * stack is made. stack_guard_check sweeps every band (and the bottom of
 * the main thread's stack, which sits directly above the heap's limit);
 * a band word that changed is an overflow, reported once per stack with
 * its label and owner (addr2line the owner: a GObj thread's is its entry
 * procedure). stack_guard_report prints how deep each kind of stack has
 * ever gone -- the first word of paint that changed, counted from the
 * top -- so a stack running close to its size shows up before it
 * overflows.
 *
 * Without the flag DC_THD_CREATE is plain thd_create and nothing here is
 * compiled. */
#ifndef SSB_DC_STACKGUARD_H
#define SSB_DC_STACKGUARD_H

#if defined(DB_STACK_GUARD) && defined(_arch_dreamcast)
#include <stddef.h>
#include <kos/thread.h>

#define STACK_GUARD_BAND 1024

/* A painted stack of `size` bytes with a guard band under it: the
 * address to hand thd_create_ex as attr.stack_ptr (its lowest byte), or
 * NULL when the heap is out. `label` must outlive the stack (a string
 * literal); `owner` is what the report names it by. */
void *stack_guard_alloc(size_t size, const char *label, const void *owner);

/* Check it one last time, keep its depth for the report, and free it.
 * Only after the thread using it is gone (thd_join). */
void stack_guard_free(void *stack);

/* Every band, the main thread's included. 0 when clean, -1 when any
 * stack has overflowed (each reported once, under `where`). */
int stack_guard_check(const char *where);

/* One stack's band only, for a caller that knows which thread just ran
 * (sysshim.c's osStartThread). */
int stack_guard_check_one(void *stack, const char *where);

/* The deepest use of each kind of stack since boot, one line per label. */
void stack_guard_report(const char *where);

/* Paint the main thread's stack below the caller's frame. Once, first
 * thing in main(). */
void stack_guard_paint_main(void);

/* thd_create on a guarded stack of THD_STACK_SIZE bytes. `label` names
 * it in the report; the thread's KOS label is still the caller's to set. */
kthread_t *stack_guard_thd_create(int detach, void *(*routine)(void *),
                                  void *param, const char *label);

#define DC_THD_CREATE(detach, routine, param, label) \
    stack_guard_thd_create((detach), (routine), (param), (label))
#else
#define DC_THD_CREATE(detach, routine, param, label) \
    thd_create((detach), (routine), (param))
#endif

#endif /* SSB_DC_STACKGUARD_H */
