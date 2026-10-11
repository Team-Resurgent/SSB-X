/* The game's FGM sound-effect engine, on AICA channels.
 *
 * A sound effect is three layers of bytecode, not a sample: fgm.ucd[id] is
 * the voice script (articulation select, volume/pan, packed note rows),
 * each note runs fgm.tbl[n] per frame (envelope ramps, `trigger` = start a
 * B1_sounds2 bank sound), and notes carry fgm.unk LFOs (vibrato, sweeps,
 * randomised shapes). This is a port of the three interpreters in
 * src/libultra/n_audio/n_env.c (func_80026B90 / func_80027460 /
 * func_800293A8), command for command, over AICA channels instead of the
 * RSP synthesizer.
 *
 * The pitch rule learned from the original: n_alSynStartVoiceParams takes
 * alCents2Ratio(cents) with no sample-rate factor, so 0 cents plays a
 * sample at the synthesizer's *output* rate -- the game runs
 * osAiSetFrequency(32000) -- regardless of the bank's 44.1 kHz tag. Hence
 * FGM_BASE_RATE below, not the pack's per-sound rate.
 *
 * The FGM files stay big-endian (be.h) like all logic data; the samples
 * come from tools/export/ssb_fgmexport.py as Yamaha ADPCM (AICA_SM_ADPCM),
 * which the AICA decodes in hardware -- 4 bits a sample against the 2 MB of
 * sound RAM the music bank already half fills.
 */
#ifndef DC_FGM_H
#define DC_FGM_H

#include <stdint.h>

#define FGM_BASE_RATE 32000
#define FGM_MAX_SOUNDS 512      /* B1_sounds2 has 322 */

/* How often the engine's frame runs, and it is not the video frame.
 *
 * On the N64 this engine is an ALPlayer client of the synthesizer, the
 * same way the sequence player is (n_env.c:5346 vs :2459), and a client
 * says when it wants to be called again: func_800293A8 returns
 * D_8009EDD0_406D0.unk_alsound_0x44, set at n_env.c:5339 to
 * `184000000 / n_syn->outputRate` microseconds. n_alAudioFrame turns
 * that back into samples with _n_timeToSamplesNoRound (n_env.c:2329,
 * `micros * outputRate / 1000000`), so the rate cancels and the period
 * is 184 samples of output whatever it is -- 5750 us at the 32000 Hz
 * sys/audio.c:95 asks osAiSetFrequency for. That is 174 Hz.
 *
 * The port ran it at 60 for its first three steps, on the assumption
 * that "the audio thread's frame" meant the retrace; every FGM timer,
 * envelope ramp and LFO therefore ran 2.9x slow. tools/check/fgm_check.py
 * holds this constant to the decomp's own two numbers. */
#define FGM_TICK_US 5750

/* Register one bank sound's uploaded samples: `sram` is its AICA address,
 * loop points in samples. `rate_shift` is how many 2:1 decimations the
 * exporter needed to bring the wave inside the channel's 16-bit loop
 * registers -- the engine plays it back that many octaves faster, which
 * restores the pitch. Sounds never registered simply do not play. */
void fgm_set_sound(unsigned index, uint32_t sram, uint32_t nsamples,
                   int loop, uint32_t loop_start, uint32_t loop_end,
                   unsigned rate_shift);

/* `ucd`, `tbl`, `unk` are the three fgm files, verbatim big-endian.
 * `chn_base` is the first of 24 AICA channels the engine may own, one
 * per note in its pool (fgm.c). */
int fgm_init(const void *ucd, const void *tbl, const void *unk,
             int chn_base);

/* A playing sound effect. This is the game's alSoundEffect
 * (gm/gmsound.h) and n_env.c's ALWhatever8009EDD0_siz34: one struct,
 * three names, because on the N64 the handle the game holds *is* the
 * engine's voice. The port keeps that -- src/dc/fgm.c pins the layout
 * to the original's offsets with static assertions -- so game code that
 * stores a handle (ft/fttypes.h:1487-1491 p_sfx / p_voice /
 * p_loop_sfx) and reads sfx_id or writes balance through it reaches the
 * live voice, as it does on the N64. */
typedef struct FgmVoice FgmVoice;

/* n_env.c:4996 func_800269C0_275C0: start voice script `id` and put it
 * on the live list. NULL if the id is out of range or the pool is full.
 * This is what syAudioPlayFGM is built on (src/dc/syaudio.c). */
FgmVoice *fgm_start(unsigned id);

/* n_env.c:4987 func_80026A10_27610 and :5070 func_800267F4_273F4: the
 * same start, split in two. fgm_alloc hands back a voice that is not on
 * the live list yet, so the caller can set it up before it makes a
 * sound, and fgm_commit puts it there. lb/lbcommon.c's
 * lbCommonMakePositionFGM is the pair's only caller in the game: it
 * writes the balance between them. A voice allocated and never
 * committed is lost to the pool until fgm_init, exactly as on the
 * N64. */
FgmVoice *fgm_alloc(unsigned id);
void fgm_commit(FgmVoice *v);

/* n_env.c:5082 func_80026738_27338: stop one voice -- and every voice
 * forked from it, which is what the parent field is for. Its note is
 * put into the fade release_note gives it and the voice goes back to
 * the pool with timer and serial 0, which is how syAudioSweepFGMPlayers
 * later sees the slot is free. Safe on a handle whose voice has already
 * ended: the list walk simply does not find it. */
void fgm_stop(FgmVoice *v);

/* n_env.c:5353 / :5386 / :5419 func_80026174_26D74, func_80026104_26D04
 * and func_80026094_26C94: a live voice's volume scale, pan and effect
 * level, each applied to the voice, its current note, and every voice
 * forked from it. sys/audio.c:1400-1450 is what calls them, clamping
 * first. */
void fgm_set_volscale(FgmVoice *v, unsigned x);
void fgm_set_pan(FgmVoice *v, unsigned x);
void fgm_set_fx(FgmVoice *v, unsigned x);

/* The engine's sound count, n_env.c's fgm_ucode_count: no start is
 * allowed at or past it, so 0 refuses every new sound while live ones
 * play on (fgm.c says who sets it). */
unsigned fgm_get_count(void);
void fgm_set_count(unsigned count);

/* n_env.c:5452 func_80026070_26C70: the engine's master volume, 0..127.
 * sys/audio.c:1403 func_80020E64 is the game's way in. */
void fgm_set_master(unsigned x);

/* The synthesizer's side of it: `dt_us` microseconds have passed, so run
 * as many engine frames as that buys. This is what n_alAudioFrame's
 * client loop does for the FGM player (n_env.c:2153-2159), and the port
 * calls it from the same thread that advances the sequencer -- the two
 * are one synthesizer's two clients, and neither has anything to do with
 * which scene is running. */
void fgm_advance(int32_t dt_us);

/* dbglog()/printf() is not
 * thread-safe on this target -- KOS's newlib stdio keeps no lock
 * around stdout's shared FILE state -- so a warning fired from the
 * audio thread (note_missing, fgm.c) could race a same-moment
 * main-thread dbglog call and corrupt nearby static state, including
 * this file's own sLock. The audio thread now only ever formats into
 * its own slot (fgm_defer_dbglog, fgm.c); the real dbglog() call is
 * made from here, on whichever thread calls it -- syTaskmanRunFrame
 * (src/dc/taskman.c), once a frame, on the main thread. */
void fgm_defer_pump(void);

/* The live voice count as of the last frame, for callers that only want
 * to look (the serial log). */
int fgm_live(void);

/* n_env.c:5128 func_800266A0_272A0: stop every FGM at once. Every live
 * voice is ended and returned to the pool, and each one's note is put
 * into its fade so the next frame silences the channel. The game calls
 * it on the way into the title, the character select and the results. */
void fgm_stop_all(void);

/* The three tables and fgm_init on them, no samples: the game's path,
 * where src/dc/sndres.c registers each scene's sounds. 0 or -1. */
int fgm_load_tables(const char *dir, int chn_base);

/* src/dc/sndres.c's bracket around a sample swap. fgm_suspend stops
 * every channel the engine owns now, returns the pools to their boot
 * state (every handle the game holds reads as ended), forgets every
 * sound, and holds the engine's lock -- nothing plays or advances until
 * fgm_resume. Between the two, register the new set with fgm_set_sound
 * and call nothing else here. A note that later asks for a sound not
 * registered stays silent and logs the bank index, once per swap. */
void fgm_suspend(void);
void fgm_resume(void);

/* XBOX: src/dc/sndres.c's swap under the sound -- just the engine's lock,
 * around registering the new set with fgm_set_sound, with nothing
 * stopped or forgotten. */
void fgm_hold(void);
void fgm_release(void);

#endif /* DC_FGM_H */
