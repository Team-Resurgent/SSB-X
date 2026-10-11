/* The port's own AICA firmware, loaded in place of KallistiOS's stream.drv. */
#ifndef DC_DCAICA_H
#define DC_DCAICA_H

#include <stdint.h>

#include "aica/shared.h"

/* snd_init's job with src/dc/aica/'s firmware: stop the ARM, clear its
 * low memory, load the program, start it, and hand the rest of sound RAM
 * to snd_mem. The command queue it serves is KOS's, so snd_sh4_to_aica
 * and everything above it work unchanged. */
void dc_aica_init(void);

/* The firmware's status block (src/dc/aica/shared.h), as the SH-4 reads
 * it: magic is DC_AICA_MAGIC once the ARM runs the port's main loop. */
void dc_aica_status(DCAicaStatus *out);

/* Send a DC_AICA_CMD_SEQ packet (shared.h): what, and two arguments. */
void dc_aica_seq_cmd(uint32_t what, uint32_t a0, uint32_t a1);

/* Load a DSP program (MPRO 128 x 4, COEF 128, MADRS 64, as the registers
 * take them) with its ring at shared.h's DC_AICA_RING_AT, `rbl` the ring
 * size code (8K words << rbl), filled with `silence` first; then open the
 * program's output 0 at full level. Music and effects reach it through
 * each channel's send (src/dc/aica/seqsyn.c). */
void dc_aica_dsp_load(const uint16_t *mpro, const uint16_t *coef,
                      const uint16_t *madrs, int rbl, uint16_t silence);

#endif /* DC_DCAICA_H */
