/* syaudio.c -- the game's sound players, on the Dreamcast.
 *
 * sys/audio.c is two things. Most of it is the audio thread: the DMA
 * tic, the RSP task, the sequence-player state machine. src/dc/bgm.c is
 * that half. The rest is the FGM interface the whole game calls --
 * sSYAudioSoundPlayers, a table of 24 slots that syAudioPlayFGM hands
 * indices out of (audio.c:1371) and every later per-sound call indexes
 * back into (audio.c:1417-1472) -- and this file is that half, function
 * for function, over the engine in src/dc/fgm.c. Under it sit the four
 * n_env.c entry points the fighter, menu and library code call by ROM
 * address rather than through sys/audio.h at all.
 *
 * Every handle is real, so syAudioStopFGM, the per-voice
 * volume/pan/balance calls, lb/lbcommon.c's positional panning and the
 * p_sfx / p_voice / p_loop_sfx fields a fighter keeps
 * (ft/fttypes.h:1487-1491) all work.
 */
#include <string.h>

#include <macros.h>
#include <gm/gmsound.h>
#include <sys/audio.h>

#include "syaudio.h"

/* An alSoundEffect *is* the engine's voice -- one struct, three names
 * (fgm.h). The casts below are that identity, and src/dc/fgm.c holds
 * the layout to the original's offsets with static assertions so they
 * stay one struct. */
#ifndef FT_HOSTTEST
#include <kos.h>

#include "fgm.h"

/* The table is read on the audio thread (the sweep) and written on the
 * game thread (play and stop), where the N64 had the same split between
 * its audio thread and the game and no lock at all. This is that, with a
 * mutex -- the same DIVERGES the engine's own lock carries (fgm.c). The
 * lock is never held across fgm.c's, and fgm.c's is never held across
 * this one, so the two cannot deadlock. */
static mutex_t sTableLock = MUTEX_INITIALIZER;

#define TABLE_LOCK()   mutex_lock(&sTableLock)
#define TABLE_UNLOCK() mutex_unlock(&sTableLock)

/* n_env.c:4996 func_800269C0_275C0 and :4987 func_80026A10_27610. */
alSoundEffect *func_800269C0_275C0(u32 fgm_id)
{
    return (alSoundEffect *)fgm_start(fgm_id);
}

alSoundEffect *func_80026A10_27610(u32 fgm_id)
{
    return (alSoundEffect *)fgm_alloc(fgm_id);
}

/* n_env.c:5070 func_800267F4_273F4 and :5082 func_80026738_27338. */
void func_800267F4_273F4(alSoundEffect *sfx)
{
    fgm_commit((FgmVoice *)sfx);
}

void func_80026738_27338(alSoundEffect *sfx)
{
    if (sfx != NULL)
    {
        fgm_stop((FgmVoice *)sfx);
    }
}

/* n_env.c:5128 func_800266A0_272A0, stop every FGM at once. */
void func_800266A0_272A0(void)
{
    fgm_stop_all();
}

static void set_volscale(alSoundEffect *sfx, u8 x) { fgm_set_volscale((FgmVoice *)sfx, x); }
static void set_pan(alSoundEffect *sfx, u8 x)      { fgm_set_pan((FgmVoice *)sfx, x); }
static void set_fx(alSoundEffect *sfx, u8 x)       { fgm_set_fx((FgmVoice *)sfx, x); }
static void set_master(u8 x)                       { fgm_set_master(x); }

#else /* FT_HOSTTEST */

/* There is no engine on the host: fgm.c is AICA code. What is testable
 * here is everything above it -- the slot table's own bookkeeping, which
 * scenes stop every sound, which callers keep a handle and hand it back,
 * and what lbCommonMakePositionFGM writes into one -- so the host build
 * gets a pool of the same 24 alSoundEffects with func_80026844_27444's
 * bookkeeping (timer 1, a serial that skips 0) and nothing that makes a
 * sound. It is a test double, not a port: no voice script runs, so a
 * host voice lives until something stops it, which is exactly the
 * condition the tests want to check the stops against. */
#define TABLE_LOCK()   ((void)0)
#define TABLE_UNLOCK() ((void)0)

static alSoundEffect sHostVoices[SYAUDIO_SNDPLAYERS_NUM];
static u16 sHostSerial;

int gHostFGMStopAllCount;

/* What the game asked to be said, in order. ft/ftpublic.c's entire
 * output is a sequence of voice ids -- a gasp, a cheer, "Player 1",
 * "defeated" -- and with no engine under it that sequence is the only
 * thing about the crowd a host test can observe, so the double keeps
 * it. Only voices that got a slot are logged, because that is what
 * "started" means to the caller. */
u16 gHostFGMStartedLog[64];
int gHostFGMStartedCount;

static alSoundEffect *host_log(u32 fgm_id, alSoundEffect *sfx)
{
    if (sfx != NULL)
    {
        if (gHostFGMStartedCount < (int) ARRAY_COUNT(gHostFGMStartedLog))
        {
            gHostFGMStartedLog[gHostFGMStartedCount] = (u16) fgm_id;
        }
        gHostFGMStartedCount++;
    }
    return sfx;
}

static alSoundEffect *host_alloc(void)
{
    s32 i;

    for (i = 0; i < SYAUDIO_SNDPLAYERS_NUM; i++)
    {
        alSoundEffect *sfx = &sHostVoices[i];

        if (sfx->unk_0x10 == 0)
        {
            memset(sfx, 0, sizeof(*sfx));
            sfx->unk_0x10 = 1;
            sfx->balance = 0x80;

            if (++sHostSerial == 0)
            {
                sHostSerial++;
            }
            sfx->sfx_id = sHostSerial;

            return sfx;
        }
    }
    return NULL;
}

/* fgm.c's sound count, which the double has no table to take it from:
 * every id is in range until the game lowers it (sc1pgame.c's boss
 * defeat zeroes it), and from then on the two starts refuse an id at or
 * past it the way fgm_start and fgm_alloc do. */
static unsigned sHostFGMCount = 0xFFFF;

unsigned fgm_get_count(void)
{
    return sHostFGMCount;
}

void fgm_set_count(unsigned count)
{
    sHostFGMCount = count;
}

alSoundEffect *func_800269C0_275C0(u32 fgm_id)
{
    if (fgm_id >= sHostFGMCount)
    {
        return NULL;
    }
    return host_log(fgm_id, host_alloc());
}

alSoundEffect *func_80026A10_27610(u32 fgm_id)
{
    if (fgm_id >= sHostFGMCount)
    {
        return NULL;
    }
    return host_log(fgm_id, host_alloc());
}

void func_800267F4_273F4(alSoundEffect *sfx)
{
    (void)sfx;
}

void func_80026738_27338(alSoundEffect *sfx)
{
    if (sfx != NULL)
    {
        sfx->unk_0x10 = 0;
        sfx->sfx_id = 0;
    }
}

void func_800266A0_272A0(void)
{
    s32 i;

    gHostFGMStopAllCount++;

    for (i = 0; i < SYAUDIO_SNDPLAYERS_NUM; i++)
    {
        func_80026738_27338(&sHostVoices[i]);
    }
}

/* Volume scale (0x2E) and effect level (0x30) have no name in
 * gm/gmsound.h -- they fall inside its filler -- and nothing in the port
 * calls the three setters yet, so the double leaves them alone and only
 * balance, which game code does write by name, is real. */
static void set_volscale(alSoundEffect *sfx, u8 x) { (void)sfx; (void)x; }
static void set_pan(alSoundEffect *sfx, u8 x)      { sfx->balance = x; }
static void set_fx(alSoundEffect *sfx, u8 x)       { (void)sfx; (void)x; }
static void set_master(u8 x)                       { (void)x; }

#endif /* FT_HOSTTEST */

/* ---- sys/audio.c: the sound players ---------------------------------
 *
 * Verbatim from here down, except that the table walks take the lock
 * above. sSYAudioCurrentSettings.sndplayers_num is a constant here: the
 * game only ever loads dSYAudioPublicSettings (audio.c:91), and the
 * settings-change path that could alter it is the audio thread's
 * restart, which this port does not have.
 */
static alSoundEffect *sSYAudioSoundPlayers[SYAUDIO_SNDPLAYERS_NUM];

/* sys/audio.c:1371. The slot is taken whether or not a voice came back,
 * as it is on the N64: a full engine still costs a sound player, and the
 * sweep frees it on the next tic. */
s32 syAudioPlayFGM(u32 fgm)
{
    s32 i;

    TABLE_LOCK();
    for (i = 0; i < SYAUDIO_SNDPLAYERS_NUM; i++)
    {
        if (sSYAudioSoundPlayers[i] == NULL)
        {
            sSYAudioSoundPlayers[i] = func_800269C0_275C0(fgm);
            TABLE_UNLOCK();
            return i;
        }
    }
    TABLE_UNLOCK();
    return -1;
}

/* sys/audio.c:1092-1098. A voice's timer is 0 only once it is back on
 * the free list -- the engine's reaper, fgm_stop and fgm_stop_all all
 * leave it there -- so this is the slot's release. DIVERGES nowhere, but
 * note the read of the voice's timer is unsynchronised against the
 * engine, exactly as on the N64: a slot freed a tic late is a slot
 * freed a tic late. */
void syAudioSweepFGMPlayers(void)
{
    s32 i;

    TABLE_LOCK();
    for (i = 0; i < SYAUDIO_SNDPLAYERS_NUM; i++)
    {
        if ((sSYAudioSoundPlayers[i] != NULL) &&
            (sSYAudioSoundPlayers[i]->unk_0x10 == 0))
        {
            sSYAudioSoundPlayers[i] = NULL;
        }
    }
    TABLE_UNLOCK();
}

/* sys/audio.c:1458. No bounds check, as on the N64: syAudioPlayFGM's -1
 * is the caller's to notice. */
void syAudioStopFGM(s32 sndplayer)
{
    TABLE_LOCK();
    if (sSYAudioSoundPlayers[sndplayer] != NULL)
    {
        func_80026738_27338(sSYAudioSoundPlayers[sndplayer]);
        sSYAudioSoundPlayers[sndplayer] = NULL;
    }
    TABLE_UNLOCK();
}

/* sys/audio.c:1387 func_80020E10: the game's own stub. */
s32 func_80020E10(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    (void)arg0, (void)arg1, (void)arg2, (void)arg3;

    return -1;
}

/* sys/audio.c:1392 func_80020E28. The loop after the stop is empty in
 * the decomp too -- the table is not cleared here, the sweep does it. */
void func_80020E28(void)
{
    func_800266A0_272A0();
}

/* sys/audio.c:1403 func_80020E64: the engine's master volume, from the
 * options screen's 0..30720. */
void func_80020E64(u32 volume)
{
    u8 vol = (volume > 30720) ? AL_VOL_FULL : volume >> 8;

    set_master(vol);
}

/* sys/audio.c:1409 / :1424 / :1438: one sound's volume, pan and effect
 * level by slot index. */
void func_80020EA0(s32 sndplayer, u32 arg1)
{
    if (arg1 > 32767)
    {
        arg1 = 32767;
    }
    if (sSYAudioSoundPlayers[sndplayer] != NULL)
    {
        set_volscale(sSYAudioSoundPlayers[sndplayer], arg1 >> 8);
    }
}

void func_80020EF8(s32 sndplayer, s32 arg1)
{
    u8 var = arg1;

    if (var > 127)
    {
        var = 127;
    }
    if (sSYAudioSoundPlayers[sndplayer] != NULL)
    {
        set_pan(sSYAudioSoundPlayers[sndplayer], var);
    }
}

void func_80020F4C(s32 sndplayer, s32 arg1)
{
    u8 var = arg1;

    if (var > 127)
    {
        var = 127;
    }
    if (sSYAudioSoundPlayers[sndplayer] != NULL)
    {
        set_fx(sSYAudioSoundPlayers[sndplayer], var);
    }
}

/* sys/audio.c:1452 func_80020FA0_21BA0: the game's own stub. */
void func_80020FA0_21BA0(s32 sndplayer, s32 arg1)
{
    (void)sndplayer, (void)arg1;
}

/* sys/audio.c:1468 func_80020FFC: the voice's transpose key. The decomp
 * writes ->unk_0x1F, which gm/gmsound.h has no field for -- its 0x1E is
 * one u16 covering both bytes -- so the port writes the byte the N64's
 * big-endian layout puts there, which is the engine's `key`
 * (src/dc/fgm.c, 0x1F). Nothing in the port calls this yet. */
void func_80020FFC(s32 sndplayer, u8 arg1)
{
    if (sSYAudioSoundPlayers[sndplayer] != NULL)
    {
        ((u8 *)sSYAudioSoundPlayers[sndplayer])[0x1F] = arg1;
    }
}

/* sys/audio.c:76, 1245, 1251, 1255-1259: the sound mode, 0 mono and 1
 * stereo. lb/lbbackup.c's lbBackupApplyOptions restores it from the save
 * data at boot and the options screen writes it (mn/mnoption/mnoption.c
 * :982, 1004, 1021); the default is stereo (sc/scmanager.c:135).
 *
 * DIVERGES: the N64 makes it mono in the audio thread, by averaging the
 * two halves of every frame it has just rendered (audio.c:1070-1078) --
 * there is one mixer and it owns the whole output buffer. Here there is
 * no mixer: the AICA pans each of its own channels, so mono would be
 * every voice's pan forced to centre, in src/dc/fgm.c and
 * src/dc/aica/seqsyn.c rather than in one place. Worth doing when the
 * options screen lands and there is a way to ask for it; until then the
 * value is kept so it survives a save write and a read, and the
 * default -- the only value anything sets -- is the stereo the port
 * already plays. */
sb32 dSYAudioSoundQuality = 1;

void syAudioSetQuality(s32 quality)
{
    dSYAudioSoundQuality = quality;
}
