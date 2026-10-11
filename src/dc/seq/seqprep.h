/* seqprep.h -- the float half of the port's sequencer.
 *
 * src/dc/seq/seqcore.c is integer-only so it can run on the AICA's ARM7.
 * What the N64 computes with floats while a note plays -- the vibrato and
 * tremolo shapes -- depends only
 * on each instrument's bank fields, so it is computed here, once, on a
 * machine with an FPU (the SH-4, or the host), into tables the core
 * steps through.
 */
#ifndef DC_SEQPREP_H
#define DC_SEQPREP_H

#include <stddef.h>

#include "seqcore.h"

/* Fill osc[2 * n_insts] (tremolo, vibrato per instrument) from the bank,
 * with their value tables in values[0..cap). Returns the number of
 * values used, or -1 if cap is too small. */
int seq_prep_osc(const BGMInstrument *insts, uint32_t n_insts, SeqOsc *osc,
                 int32_t *values, size_t cap);

#endif /* DC_SEQPREP_H */
