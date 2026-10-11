/* seqcore.h -- the port's own music sequencer.
 *
 * Written from the N64 sequence format,
 * not from the libultra port it replaced. Integer and fixed-point only, no OS calls and
 * no library beyond memset: the same source runs on the AICA's ARM7 (no
 * FPU), on the host inside tools/check/bgm_check.py, and on the SH-4.
 *
 * The player reads a compressed-MIDI sequence (spec 1), keeps the N64's
 * microsecond event queue (spec 2), and drives voices through SeqSynth --
 * the layer that turns a voice's wave, pitch ratio, volume, pan and sends
 * into whatever the hardware takes. What a float would do (the vibrato
 * and tremolo shapes, spec 8) is computed once, outside, into SeqOsc
 * tables (src/dc/seq/seqprep.c).
 */
#ifndef DC_SEQCORE_H
#define DC_SEQCORE_H

#include <stdint.h>

#include "../bgmbank.h"

#ifndef SEQ_VOICES
#define SEQ_VOICES 24           /* voice states per player, spec 7.1 */
#endif
#ifndef SEQ_EVENTS
#define SEQ_EVENTS 64           /* event-queue items, spec 2.4 */
#endif
#define SEQ_CHANNELS 16
#define SEQ_OSCS 32             /* oscillator states, spec 8.1 */

/* Pitch ratios are unsigned Q8.24: 1.0 = 1 << 24. */
#define SEQ_RATIO_ONE (1u << 24)

/* One oscillator shape for one instrument, prepared from the float maths
 * of spec 8.2. type 0: the instrument has none. Each update advances
 * `count` by one, wrapping to 0 at `len`, and takes values[count]:
 * a ratio (Q8.24) for vibrato, 0..127 for tremolo. */
typedef struct
{
    uint8_t type;               /* the bank's tremType / vibType */
    uint8_t len;                /* table length, the wrap point */
    uint8_t count0;             /* the counter before the first update */
    uint8_t pad;
    int32_t delay_us;           /* to the first update: delay x 16384 */
    int32_t interval_us;        /* between updates */
    int32_t start;              /* the value before the first update */
    const int32_t *values;      /* [len] */
} SeqOsc;

/* The bank as the sequencer reads it: the pack's own tables (bgmbank.h)
 * and the prepared oscillators, two per instrument (tremolo, vibrato). */
typedef struct
{
    const BGMInstrument *insts;
    const BGMSound *sounds;
    const SeqOsc *osc;          /* [n_insts * 2] */
    uint32_t n_insts;
    int32_t percussion;         /* instrument index, or -1 */
} SeqBank;

/* What the player asks of the voices. `voice` is 0..SEQ_VOICES-1; pan is
 * the voice's 0..127 (spec 6); vol is spec 5.1's value, which the
 * synthesizer squares; ramps are microseconds. */
typedef struct
{
    void (*start)(void *ctx, int voice, int wave, uint32_t ratio, int vol,
                  int pan, int fxmix, int wet, int dry, int32_t ramp_us);
    void (*vol)(void *ctx, int voice, int vol, int32_t ramp_us);
    void (*pitch)(void *ctx, int voice, uint32_t ratio);
    void (*pan)(void *ctx, int voice, int pan);
    void (*fxmix)(void *ctx, int voice, int fxmix);         /* CC 91 */
    void (*wetdry)(void *ctx, int voice, int wet, int dry); /* CC 22/23 */
    void (*stop)(void *ctx, int voice);
} SeqSynth;

/* ---- the compressed-MIDI reader, spec 1 ------------------------------ */

typedef struct
{
    uint8_t *base;              /* writable: loop counters live in it */
    uint32_t cursor[SEQ_CHANNELS];
    uint32_t bref[SEQ_CHANNELS];      /* back-reference read point */
    uint8_t bref_left[SEQ_CHANNELS];
    uint8_t status[SEQ_CHANNELS];     /* running status */
    uint32_t next_tick[SEQ_CHANNELS]; /* absolute tick of the next event */
    uint32_t valid;                   /* bit per track */
    uint32_t tick;
    uint32_t division;
} SeqReader;

/* ---- the player ------------------------------------------------------ */

typedef struct
{
    uint8_t type, chan, key, pad;
    int32_t value;
    int16_t voice;
    int16_t next;               /* item index, -1 = none */
    int32_t delta;              /* from the item before */
} SeqEvent;

typedef struct
{
    const BGMInstrument *inst;
    uint8_t vol, vol2, pan, priority;
    uint8_t fxmix, sustain, vib_scale, wet;
    uint8_t dry, pad[3];
    int16_t bend_range;
    uint32_t bend;              /* ratio, Q8.24 */
} SeqChannel;

typedef struct
{
    int8_t next;                /* in the allocated or the free list */
    uint8_t chan, key, vel;
    uint8_t phase, sustain, env_gain, trem;
    int16_t sound;
    int8_t osc[2];              /* owned oscillator state, -1 none */
    int32_t env_end;            /* curTime of the envelope segment end */
    uint32_t base;              /* the key's ratio, Q8.24 */
    uint32_t vib;               /* last vibrato value, Q8.24 */
} SeqVoice;

typedef struct
{
    uint8_t count, len;
    int8_t next;                /* free list */
    uint8_t which;              /* 0 tremolo, 1 vibrato */
    const SeqOsc *shape;
} SeqOscState;

typedef struct
{
    const SeqBank *bank;
    const SeqSynth *syn;
    void *ctx;

    SeqReader seq;
    int have_seq;
    uint8_t *next_data;         /* seq_set_sequence's, until it runs */

    SeqEvent ev[SEQ_EVENTS];
    int16_t ev_head, ev_free;
    SeqEvent pending;           /* the event the next call handles */

    SeqChannel chan[SEQ_CHANNELS];
    SeqVoice voice[SEQ_VOICES];
    int8_t v_alloc, v_free;     /* allocated list is oldest first */
    SeqOscState osc[SEQ_OSCS];
    int8_t osc_free;

    int32_t cur_time;           /* the player's clock, spec 2.4 */
    int32_t call_at;            /* when the next handler call is due */
    int32_t now;                /* the outside clock seq_advance moves */
    int32_t uspt;
    int16_t vol;                /* sequence volume */
    uint8_t master_vol;
    uint8_t state;              /* SEQ_STOPPED / PLAYING / STOPPING */
} SeqPlayer;

enum { SEQ_STOPPED = 0, SEQ_PLAYING = 1, SEQ_STOPPING = 2 };

void seq_init(SeqPlayer *p, const SeqBank *bank, const SeqSynth *syn,
              void *ctx);

/* The game-facing calls (spec 10). Each is posted to the queue and runs
 * in the next handler call, as on the N64. `data` must stay writable and
 * alive while it plays. */
void seq_set_sequence(SeqPlayer *p, uint8_t *data);
void seq_play(SeqPlayer *p);
void seq_stop(SeqPlayer *p);
void seq_set_volume(SeqPlayer *p, int16_t vol);
void seq_set_chan_priority(SeqPlayer *p, int chan, int priority);
void seq_set_chan_fxmix(SeqPlayer *p, int chan, int fxmix);

/* Move the outside clock on by `us` and make every handler call that
 * falls due. */
void seq_advance(SeqPlayer *p, int32_t us);

/* 2^(cents/1200) as Q8.24, for |cents| < 9600. */
uint32_t seq_cents_ratio(int32_t cents);

#endif /* DC_SEQCORE_H */
