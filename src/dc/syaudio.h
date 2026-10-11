/* syaudio.h -- sys/audio.c's sound players, on the Dreamcast.
 *
 * The rest of sys/audio.c's public interface reaches the port through
 * the decomp's own sys/audio.h; this header is only for the two names
 * that have no decomp declaration, because on the N64 they are internal
 * to the audio thread's tic loop.
 */
#ifndef DC_SYAUDIO_H
#define DC_SYAUDIO_H

/* sys/audio.c:1092-1098, out of the audio thread's tic loop: release
 * every sound-player slot whose voice has ended. The port's audio
 * thread is src/dc/bgm.c, and calls this once per tic where the N64
 * does -- after the frame that may have ended a voice, not before. */
void syAudioSweepFGMPlayers(void);

/* n_env.c:5082 func_80026738_27338: stop one voice. The game calls it by
 * ROM address, and on the N64 no header declares it -- IDO took the
 * implicit declaration, which is what ft/ftpublic.c:106 still relies on.
 * GCC will not, so the game's Makefile -includes this header when it compiles
 * that file. src/dc/syaudio.c is the definition. */
struct alSoundEffect;
void func_80026738_27338(struct alSoundEffect *sfx);

/* How many slots there are: sys/audio.c:114
 * dSYAudioPublicSettings.sndplayers_num. Exposed for the host test. */
#define SYAUDIO_SNDPLAYERS_NUM 24

#endif /* DC_SYAUDIO_H */
