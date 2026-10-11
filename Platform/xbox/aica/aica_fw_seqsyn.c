/* aica_fw_seqsyn.c -- src/dc/aica/seqsyn.c, the music voices of the port's
 * AICA firmware, compiled unchanged for the x86 firmware
 * (aica_fw_xbox.c).
 *
 * One address in it is a fixed AICA address the ARM dereferences directly:
 * the voice-start trace at DC_AICA_TRACE_AT, which the ARM sees at that
 * address because sound RAM starts at 0 for it. On the Xbox sound RAM
 * starts at xbox_aica_ram, so the trace's address is redefined to the same
 * offset in it before the file is read. Its other channel accesses go
 * through `chans` and the register macros, which the Xbox's aica.h and
 * aica_fw_xbox.c already point at the emulated chip. */
#include "../../../src/dc/aica/shared.h"
#include "aica_xbox.h"

#undef DC_AICA_TRACE_AT
#define DC_AICA_TRACE_AT ((uintptr_t)xbox_aica_ram + 0x021200u)

#include "../../../src/dc/aica/seqsyn.c"
