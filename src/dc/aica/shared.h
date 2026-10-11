/* shared.h -- what the SH-4 and the port's AICA firmware agree on.
 * Included by the firmware in src/dc/aica/ (ARM) and by src/dc/dcaica.c and
 * src/dc/bgmarm.c (SH-4); both are 32-bit little-endian with the same struct
 * layout rules, and every address below is an AICA address -- the ARM sees
 * sound RAM at 0.
 */
#ifndef DC_AICA_SHARED_H
#define DC_AICA_SHARED_H

#include <stdint.h>

/* The firmware's status block, in the range KOS's aica_cmd_iface.h
 * reserves above its clock word. */
#define DC_AICA_STATUS_AT 0x021100
#define DC_AICA_MAGIC 0x41425353u        /* "SSBA" */

typedef struct
{
    uint32_t magic;
    uint32_t passes;        /* main-loop passes since boot */
    uint32_t packets;       /* command packets run */
    uint32_t seq_cmds;      /* DC_AICA_CMD_SEQ packets run */
    uint32_t seq_state;     /* SEQ_STOPPED / PLAYING / STOPPING */
    int32_t seq_time;       /* the player's clock, microseconds */
    uint32_t seq_missed;    /* notes on a wave not in sound RAM */
    uint32_t seq_missed_wave;   /* the last such wave */
    int32_t seq_now;        /* the time the player has been fed */
    int32_t seq_call_at;    /* its next handler call */
    uint32_t clk_max_step;  /* the largest clock step one pass saw */
    uint32_t clk_big_steps; /* passes that saw more than 1000 ticks */
    uint32_t big_now, big_last; /* the last big step's two readings */
    uint32_t trace_n;       /* voice starts recorded at DC_AICA_TRACE_AT */
    uint32_t min_free_voices;   /* the fewest free voice states seen */
    uint32_t max_queue;         /* the longest event queue seen */
    uint32_t fx_sends;          /* DC_SEQ_FXSEND packets run */
} DCAicaStatus;

/* A debugging trace of the first DC_AICA_TRACE_N voice starts, written
 * by the firmware: { player time, wave, ratio, vol << 8 | pan } each. */
#define DC_AICA_TRACE_AT 0x021200
#define DC_AICA_TRACE_N 64

/* A sequencer command: KOS's aica_cmd_t with cmd = DC_AICA_CMD_SEQ,
 * cmd_id = one of the below and the arguments in misc[]. KOS's own
 * commands are 0..3. */
#define DC_AICA_CMD_SEQ 0x00000010
enum
{
    DC_SEQ_BANK = 1,        /* misc[0]: the bank image (DCSeqImage) */
    DC_SEQ_SETSEQ,          /* misc[0]: the sequence bytes, writable */
    DC_SEQ_PLAY,
    DC_SEQ_STOP,
    DC_SEQ_VOLUME,          /* misc[0]: 0..0x7FFF */
    DC_SEQ_PRIORITY,        /* misc[0]: channel, misc[1]: priority */
    DC_SEQ_KILL,            /* stop every music channel and reset the
                             * player now: sound RAM is about to move */
    DC_SEQ_FXSEND,          /* misc[0]: a channel outside the music's,
                             * misc[1]: FX mix 0..127 | N64 pan << 8 --
                             * a sound effect's reverb send (fgm.c) */
};

/* One wave as the music synthesizer needs it. `sram` is 0 while the wave
 * is not in sound RAM (src/dc/sndres.c decides), and the SH-4 rewrites
 * the table whenever that changes. */
typedef struct
{
    uint32_t sram;
    uint32_t nsamples;
    uint32_t loop_start, loop_end;
    uint8_t loop;           /* loops at all */
    uint8_t rate_shift;     /* the wave's rate is the bank's >> this */
    uint8_t pad[2];
} DCSeqWave;

/* The bank image's header. The SeqBank inside it (src/dc/seq/seqcore.h)
 * points, with AICA addresses, at the instrument, sound and oscillator
 * tables that follow it in the same allocation. */
typedef struct
{
    uint32_t bank[5];       /* a SeqBank, laid out as on the ARM */
    uint32_t rate;          /* the bank's sample rate */
    uint32_t n_waves;
    uint32_t waves;         /* DCSeqWave[n_waves] */
} DCSeqImage;

/* The AICA channels the music owns: 0 .. 23 (src/dc/fgm.c has 32..55). */
#define DC_SEQ_CHN_BASE 0

/* The reverb's delay line: 32K words, the smallest of the DSP's ring
 * sizes that holds its longest tap. It sits in the gap KOS's layout leaves
 * above the clock word and this file's blocks, and reaches 8 KB into what
 * would be sample memory, so src/dc/dcaica.c starts snd_mem at its end.
 * The DSP's base register counts 2 KB. */
#define DC_AICA_RING_AT 0x022000
#define DC_AICA_RING_BYTES 0x010000
#define DC_AICA_RAM_START (DC_AICA_RING_AT + DC_AICA_RING_BYTES)

_Static_assert(DC_AICA_TRACE_AT + 16 * DC_AICA_TRACE_N <= DC_AICA_RING_AT,
               "the voice-start trace runs into the reverb's ring");

#endif /* DC_AICA_SHARED_H */
