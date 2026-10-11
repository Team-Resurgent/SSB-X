/* seqsyn.h -- the music sequencer's voices on the ARM; see seqsyn.c. */
#ifndef DC_AICA_SEQSYN_H
#define DC_AICA_SEQSYN_H

#include "shared.h"
#include "../seq/seqcore.h"

extern const SeqSynth gSeqSynAica;

void seqsyn_init(volatile DCAicaStatus *status);
/* the bank image the voices read waves from (DC_SEQ_BANK) */
void seqsyn_bank(const DCSeqImage *image);
/* stop every music channel now */
void seqsyn_silence(void);
/* walk the volume ramps on */
void seqsyn_advance(int32_t us);
/* a sound effect's reverb send (DC_SEQ_FXSEND): the N64's FX mix and pan
 * for channel `ch`, which fgm.c drives through KOS's commands */
void seqsyn_fx_send(int ch, int fxmix, int pan);

#endif /* DC_AICA_SEQSYN_H */
