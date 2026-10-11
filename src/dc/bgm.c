/* The port's BGM player -- see bgm.h.
 *
 * sys/audio.c keeps SYAUDIO_BGMPLAYERS_NUM compressed-MIDI players, one
 * sequence each, and drives them from its audio thread; the six calls
 * the rest of the game makes are all in bgm.h. What is here is that
 * bookkeeping: the per-player status machine out of syAudioThreadMain
 * (sys/audio.c:1096-1160), the volume fades under it, and the thread
 * that clocks it and the FGM engine. The sequencer itself runs in the
 * AICA firmware (src/dc/seq/seqcore.c); bgmarm.c is the
 * SH-4's half of it.
 */
#include "bgm.h"

#include <kos.h>
#include <dc/sound/sound.h>
#include <stdlib.h>
#include <string.h>

#include "bgmbank.h"
#include "fgm.h"
#include "syaudio.h"
#include "stackguard.h"
#include "dcaica.h"
#include "bgmarm.h"
#include <dc/g2bus.h>
#include "seq/seqcore.h"

/* sys/audio.c:91 dSYAudioPublicSettings, the priority field. */
#define BGM_PRIORITY 20

/* The sequencer runs on its own clock; the status machine and the volume
 * fades keep the N64's video-frame one (PR/libaudio.h:69). */
#define BGM_FRAME_US 16000      /* AL_USEC_PER_FRAME */

/* sys/audio.c's player statuses (PR/libaudio.h) */
#define AL_STOPPED 0
#define AL_PLAYING 1

/* The longest sequence in the pack: the largest the status machine will
 * hand the firmware (bgm_arm_load_seq copies it into sound RAM, where the
 * loop counters it rewrites live). */
static uint32_t sSeqBufSize;

static int32_t sStatus;        /* sSYAudioCSPlayerStatuses[0] */
static int32_t sPlayingID = -1;/* sSYAudioBGMPlayingIDs[0]    */
static float sVolume = 30720.0f;
static float sVolRate;
static int32_t sVolTimer;

static mutex_t sLock = MUTEX_INITIALIZER;
static kthread_t *sThread;
static volatile int sRun;
static int sReady;
/* sReady is "this has been done"; sHaveBank is "there is a sequence
 * player to talk to". They came apart when the thread became the FGM
 * engine's clock as well, which has to run whether or not the music
 * bank loaded. */
static int sHaveBank;
/* src/dc/sndres.c owns the samples (bgm_defer_samples): load the tables
 * only. */
static int sDeferSamples;

/* The player under the status machine: the port's own, in the AICA
 * firmware. */
#define PLAYER_STOPPED() (bgm_arm_state() == SEQ_STOPPED)
#define PLAYER_STOP() bgm_arm_stop()
#define PLAYER_VOL(v) bgm_arm_volume(v)

/* ---------------------------------------------------------------- *
 * sys/audio.c:1096-1160, the audio thread's BGM half                *
 * ---------------------------------------------------------------- */

static void bgm_frame(void)
{
    switch (sStatus)
    {
    case 1:
        if (!PLAYER_STOPPED())
        {
            PLAYER_STOP();
            break;
        }
        else if (sPlayingID < 0)
        {
            sStatus--;
            break;
        }
        else
        {
            const BGMSeqEntry *e = &gBGMBank.seqs[sPlayingID];

            /* sys/audio.c:1124 syAudioReadRom, out of the pack. */
            if (e->size > sSeqBufSize)
            {
                sStatus = AL_STOPPED;
                sPlayingID = -1;
                break;
            }
            bgm_arm_load_seq(gBGMBank.seqdata + e->off, e->size);
            sStatus++;
            break;
        }

    case 2:
#ifdef DB_AICA_STATUS
        dbglog(DBG_INFO, "bgm: song %ld\n", (long)sPlayingID);
#endif
        bgm_arm_play(BGM_PRIORITY);
        sStatus++;
        break;

    case 3:
        if (PLAYER_STOPPED())
        {
            sStatus = AL_STOPPED;
            sPlayingID = -1;
        }
        break;
    }

    /* sys/audio.c:1142 */
    if (sVolTimer != 0)
    {
        sVolTimer--;
        sVolume += sVolRate;

        if (sVolume < 0.0f)
            sVolume = 0.0f;
        else if (sVolume > 30720.0f)
            sVolume = 30720.0f;

        PLAYER_VOL((int16_t)sVolume);
    }
}

static void *bgm_thread(void *arg)
{
    uint64_t prev = timer_us_gettime64();
    int32_t frame_accum = 0;

    (void)arg;

    while (sRun)
    {
        uint64_t now = timer_us_gettime64();
        int32_t dt = (int32_t)(now - prev);

        prev = now;
        /* A long stall (a scene load) must not make the sequencer
         * replay minutes of music at once. */
        if (dt > 4 * BGM_FRAME_US)
            dt = 4 * BGM_FRAME_US;

        if (sHaveBank)
        {
            mutex_lock(&sLock);
            for (frame_accum += dt; frame_accum >= BGM_FRAME_US;
                 frame_accum -= BGM_FRAME_US)
                bgm_frame();
            /* the player clocks itself, on the ARM */
            bgm_arm_log_misses();
            mutex_unlock(&sLock);
        }

        /* The synthesizer's other client. n_alAudioFrame runs the FGM
         * player and the sequence player off one loop (n_env.c:2153-2159)
         * and neither knows what scene is up, so this thread is where the
         * port's FGM frame belongs; it kept the sound effects of the last
         * battle running through the results screen and the title when it
         * lived in a scene's own update instead. */
        fgm_advance(dt);
        /* And the sound players the frame may have just freed
         * (sys/audio.c:1092-1098, src/dc/syaudio.c). It goes after the
         * frame for the reason the N64 puts it after its own: a voice
         * that ended this tic should release its slot this tic. */
        syAudioSweepFGMPlayers();

#ifdef DB_AICA_STATUS
        /* -DDB_AICA_STATUS (src/dc/db.h): the firmware's own counters
         * every five seconds -- the ARM has no serial of its own */
        {
            static uint64_t next;

            if (now >= next)
            {
                DCAicaStatus st;

                dc_aica_status(&st);
                dbglog(DBG_INFO, "aica: at %lu ms the ARM clock word is "
                       "%lu\n", (unsigned long)(now / 1000),
                       (unsigned long)g2_read_32(0xA0821000));
                dbglog(DBG_INFO, "aica: magic %08lx, %lu passes, %lu "
                       "packets; seq %lu cmds, state %lu, clock %ld us, "
                       "%lu missed; now %ld, next call %ld; clock "
                       "steps max %lu, %lu big (last %lu after %lu); "
                       "voices free >= %lu, queue <= %lu; effect sends "
                       "%lu\n",
                       (unsigned long)st.magic,
                       (unsigned long)st.passes, (unsigned long)st.packets,
                       (unsigned long)st.seq_cmds,
                       (unsigned long)st.seq_state, (long)st.seq_time,
                       (unsigned long)st.seq_missed, (long)st.seq_now,
                       (long)st.seq_call_at,
                       (unsigned long)st.clk_max_step,
                       (unsigned long)st.clk_big_steps,
                       (unsigned long)st.big_now, (unsigned long)st.big_last,
                       (unsigned long)st.min_free_voices,
                       (unsigned long)st.max_queue,
                       (unsigned long)st.fx_sends);
                next = now + 5000000;
            }
        }
#endif

        thd_sleep(1);
    }
    return NULL;
}

/* ---------------------------------------------------------------- *
 * sys/audio.c's BGM entry points                                    *
 * ---------------------------------------------------------------- */

/* sys/audio.c:880 */
void syAudioMakeBGMPlayers(void)
{
    uint32_t i, max = 0;

    if (sReady)
        return;

    /* the port's own AICA firmware */
    dc_aica_init();

    /* Where the pack lives -- the disc when this build booted from one,
     * else the romdisk -- is src/dc/assetroot.h's answer, not this
     * file's. */
    if (sDeferSamples)
        bgm_bank_load_tables(&gBGMBank, "bgm.pak");
    else
        bgm_bank_load(&gBGMBank, "bgm.pak");
    /* A missing bank costs the music and nothing else: the thread below
     * is also the FGM engine's clock, so it starts either way. */
    if (!gBGMBank.n_seqs)
    {
        dbglog(DBG_WARNING, "bgm: no music bank; the game is silent\n");
    }
    else
    {
        for (i = 0; i < gBGMBank.n_seqs; i++)
        {
            if (gBGMBank.seqs[i].size > max)
                max = gBGMBank.seqs[i].size;
        }
        sSeqBufSize = max;
        if (bgm_arm_setup(&gBGMBank, max) == 0)
            sHaveBank = 1;
    }

    /* A 1 ms scheduler quantum, so the sequencer's thread really does
     * wake every millisecond -- see bgm.h. */
    thd_set_hz(1000);

    sRun = 1;
    sThread = DC_THD_CREATE(0, bgm_thread, NULL, "bgm");
    if (!sThread)
    {
        dbglog(DBG_ERROR, "bgm: no thread for the sequencer\n");
        sRun = 0;
        return;
    }
    thd_set_label(sThread, "audio");
    thd_set_prio(sThread, PRIO_DEFAULT / 2);
    sReady = 1;
}

/* ---------------------------------------------------------------- *
 * Sound RAM changing hands (src/dc/sndres.c)                        *
 * ---------------------------------------------------------------- */

void bgm_defer_samples(void)
{
    sDeferSamples = 1;
}

/* The player's lock is taken and kept, so the thread's next tick waits
 * on it -- and that same tick is the FGM engine's clock, which is the
 * other half of what the swap needs held still. The music stops now:
 * the player is killed, the channels are stopped, and the status
 * machine forgets its track, because every scene that swaps sets also
 * starts its own music (the swap never happens between two scenes that
 * share a track -- those share a set). */
void bgm_suspend(void)
{
    mutex_lock(&sLock);
    if (sHaveBank)
    {
        bgm_arm_kill();
    }
    sStatus = AL_STOPPED;
    sPlayingID = -1;
}

void bgm_resume(void)
{
    /* src/dc/sndres.c has just rewritten wave_sram */
    if (sHaveBank)
        bgm_arm_waves_changed();
    mutex_unlock(&sLock);
}

/* XBOX: see bgm.h */
void bgm_waves_changed(void)
{
    mutex_lock(&sLock);
    if (sHaveBank)
        bgm_arm_waves_changed();
    mutex_unlock(&sLock);
}

/* sys/audio.c:1283 */
s32 syAudioPlayBGM(s32 sngplayer, u32 bgm)
{
    (void)sngplayer;

    if (!sHaveBank)
        return -1;

    if (bgm < gBGMBank.n_seqs)
    {
        mutex_lock(&sLock);
        sStatus = AL_PLAYING;
        sPlayingID = (int32_t)bgm;
        mutex_unlock(&sLock);

        return (s32)bgm;
    }
    else return -1;
}

/* sys/audio.c:1296 */
void syAudioStopBGM(s32 sngplayer)
{
    (void)sngplayer;

    if (!sHaveBank)
        return;

    mutex_lock(&sLock);
    sStatus = AL_PLAYING;
    sPlayingID = -1;
    mutex_unlock(&sLock);
}

/* sys/audio.c:1272 */
void syAudioStopBGMAll(void)
{
    s32 i;

    for (i = 0; i < SYAUDIO_BGMPLAYERS_NUM; i++)
        syAudioStopBGM(i);
}

/* sys/audio.c:1303 */
void syAudioSetBGMVolume(s32 sngplayer, u32 vol)
{
    (void)sngplayer;

    if (!sHaveBank)
        return;
    if (vol > 30720)
        vol = 30720;

    mutex_lock(&sLock);
    PLAYER_VOL((int16_t)vol);
    sVolume = (float)vol;
    sVolTimer = 0;
    mutex_unlock(&sLock);
}

/* sys/audio.c:1315 */
void syAudioSetBGMVolumeFade(s32 sngplayer, u32 vol, u32 time)
{
    if (!sHaveBank)
        return;
    if (vol > 30720)
        vol = 30720;

    if (time != 0)
    {
        mutex_lock(&sLock);
        sVolTimer = (int32_t)time;
        sVolRate = ((float)vol - sVolume) / (float)time;
        mutex_unlock(&sLock);
    }
    else syAudioSetBGMVolume(sngplayer, vol);
}

/* sys/audio.c:1358 */
s32 syAudioCheckBGMPlaying(s32 sngplayer)
{
    (void)sngplayer;

    if (!sHaveBank)
        return 0;

    return !PLAYER_STOPPED();
}
