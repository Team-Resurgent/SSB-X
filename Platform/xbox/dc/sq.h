/* dc/sq.h -- Xbox shim: the SH-4 store queues. The only user is the boot's
 * DB_BOOT_JUNK fill of texture memory (src/game/ssb64/main.c), which is a
 * 32-bit memset of the emulated VRAM here. */
#ifndef XBOX_DC_SQ_H
#define XBOX_DC_SQ_H

#include <kos/cdefs.h>

__BEGIN_DECLS

void *sq_set32(void *dest, uint32_t c, size_t n);
void *sq_cpy(void *dest, const void *src, size_t n);

__END_DECLS

#endif /* XBOX_DC_SQ_H */
