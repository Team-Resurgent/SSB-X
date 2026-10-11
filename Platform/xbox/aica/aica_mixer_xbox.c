/* aica_mixer_xbox.c -- the AICA's 64 channels and its DSP, in software, into
 * one DirectSound stream.
 *
 * Each channel plays what its registers say, as KallistiOS's aica.c wrote
 * them (aica_fw_xbox.c) and the port's firmware adjusted them
 * (src/dc/aica/seqsyn.c):
 *
 *   reg 0x00  KYONB (key on, handled at the KYONEX write), LPCTL (loop),
 *             PCMS (0 PCM16, 1 PCM8, 2/3 ADPCM)
 *   reg 0x08  LSA, reg 0x0C LEA -- loop start / end, in samples
 *   reg 0x18  OCT (bits 11-14, signed) and FNS (bits 0-9): the channel plays
 *             2^OCT * (1 + FNS/1024) samples per 44.1 kHz output sample
 *   reg 0x20  IMXL (bits 4-7): the send into the DSP, 3 dB a step, 0 = off
 *   reg 0x24  DIPAN (bits 0-4) and DISDL (bits 8-11, 3 dB a step, 0 = off)
 *   reg 0x28  TL (bits 8-15): attenuation, 16 steps to 6 dB
 *
 * The decoder is the AICA's Yamaha ADPCM (tools/lib/ssb_adpcm.py, the port's
 * encoder, models the same chip): a nibble moves the prediction by
 * (2m + 1) * scale / 8 and the scale by {230 x4, 307, 409, 512, 614} / 256,
 * clamped to 127..24576. On a loop the prediction and scale go back to what
 * they were at the loop start, as the chip does. Between samples the
 * channel is linearly interpolated.
 *
 * The DSP: every channel's send (sample x TL x IMXL) sums into the DSP's
 * input, and its output (EFREG 0) comes back at EFSDL/EFPAN (0x2000). The
 * program the port loads into the DSP is libultra's AL_FX_CUSTOM reverb; the
 * mixer runs that reverb's network itself (Platform/xbox/gen/xbox_reverb.h),
 * once dc_aica_dsp_load has turned the DSP's output up.
 *
 * Every 100 samples the clock word gains its 10 ticks of 4410 Hz and the
 * firmware takes a pass (aica_fw_pass), so its commands land, its sequencer
 * moves and its voices change on the boundaries they do on the Dreamcast. */
#include "../xbox_xtl.h"
#include <dsound.h>

#include <string.h>

#include <kos/dbglog.h>
#include "../aica_cmd_iface.h"
#include "aica_xbox.h"
#include "aica_reverb_net.h"

#include "aica_decode.h"

static Voice sV[64];
static CRITICAL_SECTION sLock;  /* key on/off from the firmware vs the mix */
static int sLockReady;

/* ==================================================== key on / key off */

void aica_mixer_key_on(int ch)
{
    Voice *v = &sV[ch];
    volatile unsigned char *r = xbox_aica_regs + 0x80 * ch;
    uint32_t r0;

    if (v->keyed)
        return;                 /* already on: KYONEX does not restart it */
    if (sLockReady)
        EnterCriticalSection(&sLock);
    r0 = *(volatile uint32_t *)r;
    v->fmt = (int)((r0 >> 7) & 3);
    v->loop = (r0 & 0x0200) != 0;
    v->lsa = *(volatile uint32_t *)(r + 8) & 0xFFFF;
    v->lea = *(volatile uint32_t *)(r + 12) & 0xFFFF;
    v->data = (const uint8_t *)aica_host(xbox_aica_chan_base[ch]);
    v->pos = 0;
    v->frac = 0;
    v->dec = 0;
    v->pred = 0;
    v->scale = 127;
    v->loop_saved = 0;
    v->keyed = 1;
    v->playing = (v->lea > 0 && v->data != NULL);
    if (v->playing)
    {
        v->cur = decode_next(v);
        v->nxt = (v->lea > 1 || v->loop) ? decode_next(v) : 0;
    }
    if (sLockReady)
        LeaveCriticalSection(&sLock);
}

void aica_mixer_key_off(int ch)
{
    sV[ch].keyed = 0;
    sV[ch].playing = 0;
}

uint32_t aica_mixer_pos(int ch)
{
    return sV[ch].pos;
}

/* ============================================================ levels */

/* 2^(-n/16): TL, 16 steps a halving */
static float sTlGain[256];
/* DISDL / IMXL / EFSDL / MVOL: 15 = 0 dB, 3 dB a step, 0 = off */
static float sLevelGain[16];
/* DIPAN's attenuation of the far side: 3 dB a step, 15 = off */
static float sPanGain[16];

/* Built by multiplication, not pow(): libm's pow drags libcpp's unwinder
 * into the link, and that wants __eh_frame bounds this XBE does not have
 * (the OoT-X port met the same thing with log10). 2^(-1/16) and 2^(-1/2)
 * to double precision; the powers are exact to float. */
static void tables_init(void)
{
    const double r16 = 0.95760328069857364693630563514;    /* 2^(-1/16) */
    const double r2 = 0.70710678118654752440084436210;     /* 2^(-1/2) */
    double g;
    int i;

    for (i = 0, g = 1.0; i < 256; i++, g *= r16)
        sTlGain[i] = (float)g;
    sLevelGain[0] = 0.0f;
    for (i = 15, g = 1.0; i >= 1; i--, g *= r2)
        sLevelGain[i] = (float)g;
    for (i = 0, g = 1.0; i < 15; i++, g *= r2)
        sPanGain[i] = (float)g;
    sPanGain[15] = 0.0f;
}

/* ============================================================ the DSP */

static XbReverb sReverb;

/* =========================================================== the mix */

/* `n` stereo samples into out[]. */
static void mix(int16_t *out, int n)
{
    float accL[AICA_PASS], accR[AICA_PASS], accS[AICA_PASS];
    int ch, i, any_send = 0;
    float mvol = sLevelGain[xbox_aica_regs[0x2800] & 0x0F];
    uint32_t ef = *(volatile uint32_t *)(xbox_aica_regs + 0x2000);
    float efsdl = sLevelGain[(ef >> 8) & 0x0F];
    int efpan = (int)(ef & 0x1F);
    float efL = efsdl, efR = efsdl;

    memset(accL, 0, sizeof(float) * n);
    memset(accR, 0, sizeof(float) * n);
    memset(accS, 0, sizeof(float) * n);

    EnterCriticalSection(&sLock);
    for (ch = 0; ch < 64; ch++)
    {
        Voice *v = &sV[ch];
        volatile unsigned char *r = xbox_aica_regs + 0x80 * ch;
        uint32_t pitch, step, r24;
        int oct, dipan;
        float tl, gL, gR, gS;

        if (!v->playing)
            continue;
        r24 = *(volatile uint32_t *)(r + 0x24);
        pitch = *(volatile uint32_t *)(r + 0x18);
        oct = (int)((pitch >> 11) & 0xF);
        if (oct & 8)
            oct -= 16;
        step = (1024u + (pitch & 0x3FF));       /* x 2^oct / 1024, in 16.16 */
        step = (oct >= -6) ? step << (oct + 6) : step >> (-oct - 6);
        tl = sTlGain[r[0x29]];
        dipan = (int)(r24 & 0x1F);
        gL = gR = tl * sLevelGain[(r24 >> 8) & 0x0F];
        if (dipan & 0x10)
            gR *= sPanGain[dipan & 0x0F];       /* to the left */
        else
            gL *= sPanGain[dipan & 0x0F];       /* to the right */
        gS = tl * sLevelGain[(*(volatile uint32_t *)(r + 0x20) >> 4) & 0x0F];
        if (gS != 0.0f)
            any_send = 1;

        for (i = 0; i < n && v->playing; i++)
        {
            float s = (float)v->cur + (float)(v->nxt - v->cur) * ((float)v->frac * (1.0f / 65536.0f));

            accL[i] += s * gL;
            accR[i] += s * gR;
            accS[i] += s * gS;
            v->frac += step;
            while (v->frac >= 65536u)
            {
                v->frac -= 65536u;
                v->pos++;
                if (v->pos >= v->lea)
                {
                    if (!v->loop)
                    {
                        /* a one-shot ends at LEA; the slot stays keyed on */
                        v->playing = 0;
                        break;
                    }
                    v->pos = v->lsa;
                }
                v->cur = v->nxt;
                v->nxt = (v->loop || v->dec < v->lea) ? decode_next(v) : 0;
            }
        }
    }
    LeaveCriticalSection(&sLock);

    if (efpan & 0x0F)
    {
        if (efpan & 0x10)
            efR *= sPanGain[efpan & 0x0F];
        else
            efL *= sPanGain[efpan & 0x0F];
    }
    for (i = 0; i < n; i++)
    {
        float wet = (any_send || efsdl != 0.0f) ? xb_reverb_sample(&sReverb, accS[i]) : 0.0f;
        float l = (accL[i] + wet * efL) * mvol;
        float rr = (accR[i] + wet * efR) * mvol;
        int il = (int)l, ir = (int)rr;

        out[2 * i] = (int16_t)(il > 32767 ? 32767 : il < -32768 ? -32768 : il);
        out[2 * i + 1] = (int16_t)(ir > 32767 ? 32767 : ir < -32768 ? -32768 : ir);
    }
}

/* ====================================================== the stream */

#define PACKETS        6
#define PACKET_PASSES  4                    /* 400 samples, 9 ms */
#define PACKET_SAMPLES (AICA_PASS * PACKET_PASSES)

static LPDIRECTSOUND sDS;
static LPDIRECTSOUNDSTREAM sStream;
static int16_t *sPcm[PACKETS];
static DWORD sStatus[PACKETS];
static HANDLE sDone[PACKETS];
static int sStarted;

static void fill_packet(int k)
{
    int p;

    for (p = 0; p < PACKET_PASSES; p++)
    {
        /* the clock word: 10 ticks of 4410 Hz per 100 samples */
        volatile uint32_t *clock = (volatile uint32_t *)aica_host(AICA_MEM_CLOCK);

        *clock += 10;
        aica_fw_pass();
        mix(sPcm[k] + p * AICA_PASS * 2, AICA_PASS);
    }
}

static void submit(int k)
{
    XMEDIAPACKET xp;

    memset(&xp, 0, sizeof(xp));
    xp.pvBuffer = sPcm[k];
    xp.dwMaxSize = PACKET_SAMPLES * 4;
    xp.pdwStatus = &sStatus[k];
    xp.hCompletionEvent = sDone[k];
    sStatus[k] = XMEDIAPACKET_STATUS_PENDING;
    IDirectSoundStream_Process(sStream, &xp, NULL);
}

void xbox_fpu_setup(void);

static DWORD WINAPI mixer_thread(LPVOID arg)
{
    int k = 0;

    (void)arg;
    xbox_fpu_setup();       /* denormals in a dying reverb tail cost a P3 dearly */
    for (;;)
    {
        /* packets complete in order: wait for the oldest, refill it */
        WaitForSingleObject(sDone[k], 20);
        DirectSoundDoWork();
        if (sStatus[k] == XMEDIAPACKET_STATUS_PENDING)
            continue;
        fill_packet(k);
        submit(k);
        k = (k + 1) % PACKETS;
    }
    return 0;
}

/* Without DirectSound the firmware still has to run -- the game waits on
 * its status block and its sequencer is the music's clock -- so the same
 * passes are made on a timer and the mix is thrown away. */
static DWORD WINAPI silent_thread(LPVOID arg)
{
    static int16_t scratch[AICA_PASS * 2];
    uint64_t next = 0;
    LARGE_INTEGER f, t;

    (void)arg;
    xbox_fpu_setup();
    QueryPerformanceFrequency(&f);
    for (;;)
    {
        volatile uint32_t *clock = (volatile uint32_t *)aica_host(AICA_MEM_CLOCK);

        QueryPerformanceCounter(&t);
        if (next == 0)
            next = (uint64_t)t.QuadPart;
        if ((uint64_t)t.QuadPart < next)
        {
            Sleep(1);
            continue;
        }
        next += (uint64_t)f.QuadPart * AICA_PASS / AICA_RATE;
        *clock += 10;
        aica_fw_pass();
        mix(scratch, AICA_PASS);
    }
    return 0;
}

int aica_mixer_start(void)
{
    WAVEFORMATEX wfx;
    DSSTREAMDESC desc;
    HANDLE th;
    int k;

    if (sStarted)
        return 0;
    sStarted = 1;
    if (!sLockReady)
    {
        InitializeCriticalSection(&sLock);
        sLockReady = 1;
    }
    tables_init();

    if (FAILED(DirectSoundCreate(NULL, &sDS, NULL)))
    {
        dbglog(DBG_ERROR, "aica: DirectSoundCreate failed -- running silent\n");
        goto silent;
    }
    /* DirectSound's defaults take 6 dB off a voice and a bit off each
     * mixbin; the AICA mix already carries the port's own headroom
     * (src/dc/aicamix.h), so both go to 0 to keep the Dreamcast's level */
    IDirectSound_SetMixBinHeadroom(sDS, DSMIXBIN_FRONT_LEFT, 0);
    IDirectSound_SetMixBinHeadroom(sDS, DSMIXBIN_FRONT_RIGHT, 0);

    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = AICA_RATE;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 4;
    wfx.nAvgBytesPerSec = AICA_RATE * 4;
    memset(&desc, 0, sizeof(desc));
    desc.dwMaxAttachedPackets = PACKETS;
    desc.lpwfxFormat = &wfx;
    if (FAILED(DirectSoundCreateStream(&desc, &sStream)))
    {
        dbglog(DBG_ERROR, "aica: DirectSoundCreateStream failed -- running silent\n");
        goto silent;
    }
    IDirectSoundStream_SetHeadroom(sStream, 0);
    for (k = 0; k < PACKETS; k++)
    {
        sPcm[k] = (int16_t *)XPhysicalAlloc(PACKET_SAMPLES * 4, (ULONG_PTR)-1, 0, PAGE_READWRITE);
        sDone[k] = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (sPcm[k] == NULL || sDone[k] == NULL)
        {
            dbglog(DBG_ERROR, "aica: no memory for the stream -- running silent\n");
            goto silent;
        }
        memset(sPcm[k], 0, PACKET_SAMPLES * 4);
    }
    /* prime: every packet in flight, the firmware already running */
    for (k = 0; k < PACKETS; k++)
    {
        fill_packet(k);
        submit(k);
    }
    th = CreateThread(NULL, 64 * 1024, mixer_thread, NULL, 0, NULL);
    if (th == NULL)
        goto silent;
    SetThreadPriority(th, THREAD_PRIORITY_TIME_CRITICAL);
    CloseHandle(th);
    dbglog(DBG_INFO, "aica: software AICA up -- 64 channels, %d Hz, %d ms of stream\n",
           AICA_RATE, PACKETS * PACKET_SAMPLES * 1000 / AICA_RATE);
    return 0;

silent:
    th = CreateThread(NULL, 64 * 1024, silent_thread, NULL, 0, NULL);
    if (th != NULL)
    {
        SetThreadPriority(th, THREAD_PRIORITY_HIGHEST);
        CloseHandle(th);
    }
    return -1;
}
