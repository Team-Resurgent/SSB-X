/* bgmarm.h -- the SH-4's half of the music sequencer on the ARM.
 * src/dc/bgm.c keeps the game-facing calls, the status machine and the fades,
 * and uses these to drive the player: the player itself (src/dc/seq/seqcore.c)
 * runs in the port's AICA firmware (src/dc/aica/), clocked there. */
#ifndef DC_BGMARM_H
#define DC_BGMARM_H

#include <stdint.h>

#include "bgmbank.h"

/* Build the bank image (tables, oscillator shapes, waves) in sound RAM,
 * take a sequence buffer of `max_seq` bytes there, and hand both to the
 * firmware. 0, or -1 with nothing kept. */
int bgm_arm_setup(const BGMBank *bank, uint32_t max_seq);

/* Copy a sequence into the sound-RAM buffer. Only while stopped. */
void bgm_arm_load_seq(const uint8_t *data, uint32_t size);

/* What sys/audio.c's status 2 posts: set the sequence, play, and every
 * channel's priority. */
void bgm_arm_play(int priority);
void bgm_arm_stop(void);
void bgm_arm_volume(int16_t vol);

/* The player's state (seqcore.h SEQ_*). Until the firmware has run every
 * command sent, the answer is the one those commands are heading for --
 * a play it has not seen yet is not a song that ended. */
int bgm_arm_state(void);

/* Silence the music now and reset the player, and wait until the
 * firmware has done it: src/dc/sndres.c is about to move the waves. */
void bgm_arm_kill(void);

/* The waves moved (src/dc/sndres.c rewrote bank->wave_sram): give the
 * firmware the new addresses. */
void bgm_arm_waves_changed(void);

/* Log the notes the firmware found on waves not in sound RAM since the
 * last call. */
void bgm_arm_log_misses(void);

#endif /* DC_BGMARM_H */
