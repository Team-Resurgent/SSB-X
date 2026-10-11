/* bgm.h -- the port's BGM player: sys/audio.h:202-215 over the AICA,
 * running the game's own compressed-MIDI sequences against the game's
 * own instrument bank. Game code reaches these through the names it
 * already uses, so mpCollisionSetPlayBGM (mp/mpcollision.c:4013-4020)
 * starts a stage's music with nothing in between translating.
 *
 * This file is sys/audio.c's BGM half: the settings, the sequence
 * players' state machine out of syAudioThreadMain (sys/audio.c:1096-1160)
 * and the six entry points below. The sequencer itself is
 * src/dc/seq/seqcore.c, run in the AICA firmware (bgmarm.h), over the
 * bank bgmbank.h.
 *
 * Tracks are named by the game's own gmMusicID (gm/gmsound.h:30-81),
 * which is the index into S1_music.sbk's sequence array and therefore
 * the index into the pack.
 *
 * DIVERGES: the N64 runs this on its audio thread, woken by the
 * scheduler once a video frame, and renders the whole mix on the RSP.
 * Here a KOS thread ticks the sequencer about every millisecond and the
 * AICA renders; a video frame is too coarse a clock for a sequencer
 * whose ticks are a few milliseconds apart. The scheduler is put on a
 * 1 kHz quantum for it (thd_set_hz), which is what makes a 1 ms sleep
 * mean 1 ms.
 */
#ifndef SSB_DC_BGM_H
#define SSB_DC_BGM_H

#include <ssb_types.h>       /* s32/u32 as the decomp spells them */

/* sys/audio.h:8 */
#define SYAUDIO_BGMPLAYERS_NUM 1

/* sys/audio.c:880 syAudioMakeBGMPlayers: loads the bank, builds the
 * sequence players and starts the thread that clocks them. Safe to call
 * again; the game calls it whenever the audio settings change. */
void syAudioMakeBGMPlayers(void);

/* sys/audio.c:1283. Queues sequence `bgm` on `sngplayer`, replacing
 * whatever it was playing. Returns bgm, or -1 if the bank has no such
 * sequence -- the same contract as the N64's seqCount test. */
s32 syAudioPlayBGM(s32 sngplayer, u32 bgm);

/* sys/audio.c:1296 / 1272 */
void syAudioStopBGM(s32 sngplayer);
void syAudioStopBGMAll(void);

/* sys/audio.c:1303 / 1315. vol is the libultra 0..30720 scale; `time`
 * is in video frames. */
void syAudioSetBGMVolume(s32 sngplayer, u32 vol);
void syAudioSetBGMVolumeFade(s32 sngplayer, u32 vol, u32 time);

/* sys/audio.c:1358. FALSE once a one-shot sequence has run out. */
s32 syAudioCheckBGMPlaying(s32 sngplayer);

/* Not sys/audio.c's: the port's sound RAM holds a scene's samples, not
 * the cartridge's (src/dc/sndres.c). bgm_defer_samples, before
 * syAudioMakeBGMPlayers, makes it load the bank's tables and no waves.
 * bgm_suspend stops the music now and holds the audio thread -- the
 * sequencer's and the FGM engine's clock -- until bgm_resume; the swap
 * goes between them. A build that calls neither gets the whole bank
 * uploaded at start, as before. */
void bgm_defer_samples(void);
void bgm_suspend(void);
void bgm_resume(void);

/* XBOX: src/dc/sndres.c's swap under the sound: the new wave_sram to the
 * sequencer without stopping the music. Only for a swap that moved no
 * resident wave, so the playing track reads what it read before. */
void bgm_waves_changed(void);

#endif /* SSB_DC_BGM_H */
