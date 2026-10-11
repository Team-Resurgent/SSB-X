/* The FGM sound-effect engine -- see fgm.h. Reference:
 * src/libultra/n_audio/n_env.c in the decomp; the voice interpreter is
 * func_80026B90, the note interpreter func_80027460, the frame pump
 * func_800293A8. Field names here are what the bytecode reference doc
 * (MUSIC_AND_SFX_DISCOVERIES.md) calls things; the offsets they mirror
 * are noted where it helps cross-reading.
 */
#include <kos.h>
#include <dc/sound/aica_comm.h>
#include <dc/sound/sound.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "aicamix.h"
#include "assetroot.h"
#include "dcaica.h"
#include "be.h"
#include "fgm.h"

/* The three pools, and they are the game's own numbers. sys/audio.c:112
 * dSYAudioPublicSettings gives 48, 24 and 24 for unk31, unk32 and
 * sndplayers_num, and n_env.c:5303-5326 func_80026204_26E04 allocates
 * one array from each: modulators, notes, voices. The port sizes them the same,
 * so a busy moment does not run out of voices where the
 * N64 had room -- and the voice pool is the same 24 that sizes
 * sSYAudioSoundPlayers (src/dc/syaudio.c), which is why a full table
 * and a full pool are one condition on the N64 and here.
 *
 * Notes are AICA channels, one each from fgm_init's chn_base: 24 of
 * them now, 32..55, still clear of the 24 the music owns from 0. */
#define FGM_VOICES 24
#define FGM_NOTES 24
#define FGM_MODS 48

typedef struct FgmMod
{
    struct FgmMod *next;
    uint8_t id, shape, target, postproc;
    float period, amp, offset;              /* 0x8 / 0xC / 0x10 */
    float phase;                            /* 0x14 */
    float rnd_period, rnd_prev, rnd_value;  /* 0x18 / 0x1C / 0x20 */
} FgmMod;

typedef struct FgmNote
{
    struct FgmNote *next;
    int chn;
    const uint8_t *pc, *loopmark;           /* 0x20 / 0x24 */
    uint16_t timer;                         /* 0x28 */
    uint8_t state;                          /* 0x2A: 0 dead 1 live 2 fade 3 start */
    uint8_t key;                            /* 0x2B */
    int16_t art_cents, art_cents_prev;      /* 0x2C / 0x2E */
    int16_t note_cents;                     /* 0x30 */
    uint8_t vol, vol_prev;                  /* 0x32 / 0x33 */
    uint8_t pan, pan_prev;                  /* 0x34 / 0x35 */
    uint8_t fx, fx_prev;                    /* 0x36 / 0x37 */
    uint8_t volscale, volscale_prev;        /* 0x38 / 0x39 */
    uint8_t panbase, panbase_prev;          /* 0x3A / 0x3B */
    uint8_t fxbase, fxbase_prev;            /* 0x3C / 0x3D */
    int wave;                               /* 0x40; bank-2 sound index */
    FgmMod *mods;                           /* 0x44 */
} FgmNote;

struct FgmVoice
{
    struct FgmVoice *next;
    struct FgmVoice *parent;                /* 0x4 */
    const uint8_t *pc, *loopmark;           /* 0x8 / 0xC */
    uint16_t timer;                         /* 0x10; 0xFFFF = ended */
    uint16_t durtab[6];                     /* 0x12 */
    uint8_t unk1E, key;                     /* 0x1E / 0x1F */
    uint8_t staccato, cutpending;           /* 0x20 / 0x21 */
    uint8_t vol, pan;                       /* 0x22 / 0x23 */
    uint16_t artic;                         /* 0x24 */
    uint16_t serial;                        /* 0x26 */
    FgmNote *note;                          /* 0x28 */
    uint8_t fxdefault, group;               /* 0x2C / 0x2D */
    uint8_t volscale;                       /* 0x2E */
    uint8_t pan_override, fx_override;      /* 0x2F / 0x30 */
};

/* The handle the game holds is this struct -- see fgm.h. n_env.c calls
 * it ALWhatever8009EDD0_siz34 and gm/gmsound.h calls it alSoundEffect,
 * and the two names game code reaches through it are sfx_id at 0x26 and
 * balance at 0x2F. Pin the layout to the original's offsets so the cast
 * in src/dc/syaudio.c is a cast and not a reinterpretation. */
_Static_assert(offsetof(FgmVoice, timer) == 0x10, "FgmVoice timer");
_Static_assert(offsetof(FgmVoice, serial) == 0x26, "FgmVoice sfx_id");
_Static_assert(offsetof(FgmVoice, pan_override) == 0x2F, "FgmVoice balance");
_Static_assert(sizeof(FgmVoice) == 0x34, "FgmVoice is siz34");

typedef struct
{
    uint32_t sram, nsamples, ls, le;
    uint8_t loop, present;
    uint8_t rate_shift;         /* halvings the exporter had to apply */
} FgmSound;

static struct
{
    const uint8_t *ucd, *tbl, *unk;
    uint32_t ucd_count, tbl_count, unk_count;
    int chn_base;
    uint16_t serial_v, serial_n;    /* 0x4A / 0x48 */
    uint8_t master;                 /* 0x5A */
    uint8_t default_key;            /* 0x4C */
    uint16_t default_durtab[6];     /* 0x4E */
    FgmVoice voices[FGM_VOICES];
    FgmNote notes[FGM_NOTES];
    FgmMod mods[FGM_MODS];
    FgmVoice *voice_free, *voice_live;      /* 0x38 / 0x40 */
    FgmNote *note_free, *note_live;         /* 0x34 / 0x3C */
    FgmMod *mod_free;                       /* 0x30 */
    FgmSound sounds[FGM_MAX_SOUNDS];
    uint32_t seed1, seed2;
} G;

/* The game reaches this engine from two threads where the N64 reached it
 * from two contexts: syAudioPlayFGM runs on the game thread and the
 * frame pump on the audio one, exactly as here. The N64 shuts interrupts
 * out for the list surgery (osSetIntMask(1) at n_env.c:5135 and 5243);
 * this is that, with a mutex, and it covers the same three entry points.
 */
static mutex_t sLock = MUTEX_INITIALIZER;
static int32_t sTickAccum;      /* microseconds owed to the pump */
static volatile int sLiveVoices;

/* The game's own LCG, both streams. */
static float rand1(void)
{
    G.seed2 = G.seed2 * 0x343FD + 0x269EC3;
    return (float)((G.seed2 >> 16) & 0xFFFF) / 65536.0f;
}

static float rand2(void)
{
    G.seed1 = G.seed1 * 0x343FD + 0x269EC3;
    return (float)((G.seed1 >> 16) & 0xFFFF) / 65536.0f;
}

static uint32_t pkg_off(const uint8_t *pkg, uint32_t i)
{
    return be32u(pkg + 4 + i * 4);
}

/* A note asked for a bank sound that is not in sound RAM. It is
 * silent, exactly as it has always been, but no longer quietly: once a
 * sound, the log says which, because since the audio step every sound a
 * scene can reach is supposed to be resident (src/dc/sndres.c), and a
 * line here means tools/export/ssb_sndsets.py's census missed one. The bitmap
 * clears when the set changes, so a miss shows once per scene.
 *
 * This fires from fgm_pump, which only ever runs on the audio thread
 * (bgm_thread, src/dc/bgm.c), so it cannot call dbglog() directly.
 * dbglog() is printf() (kos/dbglog.h), and this KOS build's newlib stdio
 * keeps no lock around stdout's shared FILE state, so a call landing
 * here at the same real moment as a dbglog() call on the main thread
 * races it: two unrelated lines' characters interleave mid-word.
 * dbglog()/printf() is not safe to call from more than one KOS thread
 * on this target, and this was the one call site outside the main
 * thread anywhere in the port.
 * fgm_defer_dbglog formats into a slot here (thread-local work, no
 * shared state); fgm_defer_pump, called once a frame from the main
 * thread (src/dc/taskman.c's syTaskmanRunFrame), makes the real
 * dbglog() call. */
static uint32_t sMissed[(FGM_MAX_SOUNDS + 31) / 32];

#define FGM_DEFER_MAX 8
typedef struct { int level; char msg[80]; } FgmDeferred;
static FgmDeferred sDeferred[FGM_DEFER_MAX];
static volatile int sDeferHead, sDeferTail;

static void fgm_defer_dbglog(int level, const char *fmt, ...)
{
    int head = sDeferHead;
    int next = (head + 1) % FGM_DEFER_MAX;
    va_list ap;

    if (next == sDeferTail)
        return; /* the queue is full -- a dropped diagnostic line beats
                  * the crash this whole mechanism exists to avoid */

    va_start(ap, fmt);
    vsnprintf(sDeferred[head].msg, sizeof(sDeferred[head].msg), fmt, ap);
    va_end(ap);
    sDeferred[head].level = level;
    sDeferHead = next;
}

static void note_missing(int wave)
{
    if (!(sMissed[wave >> 5] & (1u << (wave & 31))))
    {
        sMissed[wave >> 5] |= 1u << (wave & 31);
        fgm_defer_dbglog(DBG_WARNING, "fgm: bank sound %d is not resident\n",
                          wave);
    }
}

/* ---- AICA ---- */

static void chan_cmd(int chn, uint32_t base, uint32_t nsamples, int loop,
                     uint32_t ls, uint32_t le, uint32_t freq, int vol,
                     int pan, uint32_t op)
{
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);
    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = op;
    chan->base = base;
    chan->type = AICA_SM_ADPCM;
    chan->length = (nsamples > 65534) ? 65534 : nsamples;
    chan->loop = loop;
    chan->loopstart = ls;
    chan->loopend = le ? le : chan->length;
    chan->freq = freq;
    chan->vol = vol;
    chan->pan = pan;
    snd_sh4_to_aica(tmp, cmd->size);
}

static uint32_t cents_freq(int cents)
{
    return (uint32_t)((float)FGM_BASE_RATE *
                      powf(2.0f, (float)cents / 1200.0f));
}

/* (vol 0..127) * (volscale 0..255) * master >> 7, then to AICA's 0..255,
 * less the mix headroom the music takes too (aicamix.h). */
static int mix_vol(const FgmNote *n)
{
    int v = ((int)n->vol * n->volscale * G.master >> 7) >>
            (7 + AICA_MIX_HEADROOM_SHIFT);
    return (v > 255) ? 255 : v;
}

static int mix_pan(const FgmNote *n)
{
    int p = ((int)n->pan + n->panbase) - 64;
    if (p < 0)
        p = 0;
    if (p > 127)
        p = 127;
    return p * 2;
}

/* A note's reverb send, as the N64 sets it at a start and whenever fx or
 * its base moves (n_env.c:4362-4374, 4394-4410): (fx * (base >> 1)) >> 7
 * into the FX mix. The port's own AICA firmware runs the
 * reverb; its pan scales the send too, so a pan
 * change sends it again. */
static void fx_send(const FgmNote *n)
{
    int s = ((int)n->fx * (n->fxbase >> 1)) >> 7;

    if (s > 127)
        s = 127;
    dc_aica_seq_cmd(DC_SEQ_FXSEND, (uint32_t)n->chn,
                    (uint32_t)s | ((uint32_t)(mix_pan(n) >> 1) << 8));
}

/* ---- pools ---- */

static void release_note(FgmNote *n)     /* func_8002668C */
{
    n->timer = 0;
    n->state = 2;
}

static FgmNote *alloc_note(uint16_t artic)   /* func_80026B40/26A6C */
{
    FgmNote *n;
    if (artic >= G.tbl_count)
        return NULL;
    n = G.note_free;
    if (n == NULL)
        return NULL;
    G.note_free = n->next;
    n->next = G.note_live;
    G.note_live = n;
    n->timer = 1;
    n->pc = n->loopmark = G.tbl + pkg_off(G.tbl, artic);
    n->state = 3;
    n->vol = 0x7F;
    n->pan = 0x40;
    n->fx = 0;
    n->art_cents = 0;
    n->key = G.default_key;
    n->mods = NULL;
    n->note_cents = 0;
    n->volscale = 0xFF;
    n->panbase = 0x40;
    n->fxbase = 0;
    n->wave = -1;
    return n;
}

/* n_env.c:5024 func_80026844_27444. Takes a voice off the free list and
 * sets it up, but does *not* put it on the live list: that is
 * commit_voice below, and the two exist apart because
 * lbCommonMakePositionFGM configures a voice between them. */
static FgmVoice *alloc_voice(const uint8_t *script)
{
    FgmVoice *v = G.voice_free;
    int i;
    if (v == NULL)
        return NULL;
    G.voice_free = v->next;
    v->timer = 1;
    v->pc = v->loopmark = script;
    v->unk1E = 0x30;
    v->key = G.default_key;
    for (i = 0; i < 6; i++)
        v->durtab[i] = G.default_durtab[i];
    v->staccato = v->cutpending = 0;
    v->note = NULL;
    v->parent = NULL;
    v->vol = 0xFF;
    v->pan = 0x40;
    v->fxdefault = 0x40;
    v->volscale = 0x7F;
    v->pan_override = 0x80;
    v->fx_override = 0x80;
    if (++G.serial_v == 0)
        G.serial_v++;
    v->serial = G.serial_v;
    v->group = 0;
    return v;
}

/* n_env.c:5070 func_800267F4_273F4, and the second half of
 * func_80026958_27558 (:5004). */
static void commit_voice(FgmVoice *v)
{
    if (v != NULL)
    {
        v->next = G.voice_live;
        G.voice_live = v;
    }
}

/* ---- the voice-script interpreter (func_80026B90) ---- */

static uint16_t rd_varint(const uint8_t **pc)
{
    uint16_t v = *(*pc)++;
    if (v & 0x80)
        v = *(*pc)++ + ((v & 0x7F) << 8);
    return v;
}

static void tick_voice(FgmVoice *v)
{
    const uint8_t *pc;
    int16_t t5 = 0;
    uint16_t timer = 0;

    if (v->timer == 0xFFFF || v->timer == 0)
        return;
    if (--v->timer != 0)
        return;

    if (v->cutpending)
    {
        v->cutpending = 0;
        if (v->note != NULL)
        {
            release_note(v->note);
            v->note = NULL;
        }
        v->timer = 1;
        return;
    }

    pc = v->pc;
    do
    {
        uint8_t instr = *pc++;
        if ((instr & 0xF8) >= 0xD0)
        {
            uint16_t param;
            timer = 0;
            switch (instr)
            {
            case 0xD0:                          /* stop_voice */
                timer = 0xFFFF;
                if (v->note != NULL)
                {
                    release_note(v->note);
                    v->note = NULL;
                }
                break;
            case 0xD1:                          /* set_articulation */
                v->artic = rd_varint(&pc);
                break;
            case 0xD2:
                param = *pc++;
                v->unk1E = param & 0x7F;
                v->staccato = (param & 0x80) ? 1 : 0;
                break;
            case 0xD3:
                v->key = *pc++;
                break;
            case 0xD4:                          /* set_dur_table */
            {
                int i;
                for (i = 0; i < 6; i++)
                    v->durtab[i] = rd_varint(&pc);
                break;
            }
            case 0xD5:
                v->vol = *pc++;
                break;
            case 0xD6:
            {
                int nv = (int8_t)*pc++ + v->vol;
                v->vol = (nv < 0) ? 0 : (nv > 0xFF) ? 0xFF : nv;
                break;
            }
            case 0xD7:
                v->pan = *pc++;
                break;
            case 0xD8:
            {
                int np = (int8_t)*pc++ + v->pan;
                v->pan = (np < 0) ? 0 : (np > 0x7F) ? 0x7F : np;
                break;
            }
            case 0xD9:                          /* fork_voice */
            {
                FgmVoice *f;
                param = rd_varint(&pc);
                if (param >= G.ucd_count)
                    break;
                f = G.voice_free;
                if (f != NULL)
                {
                    FgmVoice *tail = v;
                    G.voice_free = f->next;
                    /* The fork copies the parent's whole state, then gets
                     * its own script, serial and note; appended after the
                     * parent in the live list so it ticks this frame. */
                    *f = *v;
                    f->next = NULL;
                    while (tail->next != NULL)
                        tail = tail->next;
                    tail->next = f;
                    f->timer = 1;
                    f->pc = f->loopmark = G.ucd + pkg_off(G.ucd, param);
                    if (++G.serial_v == 0)
                        G.serial_v++;
                    f->serial = G.serial_v;
                    f->note = NULL;
                    if (f->parent == NULL)
                        f->parent = v;
                }
                break;
            }
            case 0xDA:
                v->loopmark = pc;
                break;
            case 0xDB:
                pc = v->loopmark;
                break;
            case 0xDC:
                v->fxdefault = *pc++;
                break;
            case 0xDD:
            {
                int nf = (int8_t)*pc++ + v->fxdefault;
                v->fxdefault = (nf < 0) ? 0 : (nf > 0x7F) ? 0x7F : nf;
                break;
            }
            case 0xDE:                          /* group steal */
            {
                FgmVoice *o;
                v->group = *pc++;
                if (v->group == 0)
                    break;
                for (o = G.voice_live; o != NULL; o = o->next)
                {
                    if (o == v || o->group != v->group)
                        continue;
                    if ((v->key & 0x7F) >= (o->key & 0x7F))
                    {
                        o->timer = 0xFFFF;
                        if (o->note != NULL)
                        {
                            release_note(o->note);
                            o->note = NULL;
                        }
                    }
                    else
                    {
                        timer = 0xFFFF;
                        if (v->note != NULL)
                        {
                            release_note(v->note);
                            v->note = NULL;
                        }
                    }
                }
                break;
            }
            case 0xDF:
                t5 = -0x960;
                break;
            case 0xE0:
                t5 = -0x12C0;
                break;
            }
        }
        else                                    /* note row */
        {
            timer = instr & 7;
            if (timer >= 1 && timer <= 6)
                timer = v->durtab[timer - 1];
            else if (timer == 7)
                timer = rd_varint(&pc);

            if (!(instr & 0xF8))                /* pitch_code 0: rest */
            {
                if (v->note != NULL)
                {
                    release_note(v->note);
                    v->note = NULL;
                }
            }
            else
            {
                if (v->note != NULL)
                {
                    /* Re-trigger on the live note: new pitch, and dirty
                     * the prev so the update lands. */
                    v->note->note_cents =
                        (int16_t)(((instr >> 3) * 100) - 1300) + t5;
                    t5 = 0;
                    v->note->art_cents_prev = v->note->art_cents + 1;
                }
                else
                {
                    v->note = alloc_note(v->artic);
                    if (v->note != NULL)
                    {
                        FgmNote *n = v->note;
                        n->key = v->key;
                        n->note_cents =
                            (int16_t)(((instr >> 3) * 100) - 1300) + t5;
                        t5 = 0;
                        n->volscale = (v->vol * v->volscale) >> 7;
                        n->panbase = (v->pan_override != 0x80)
                                         ? v->pan_override : v->pan;
                        n->fxbase = (v->fx_override != 0x80)
                                        ? (uint8_t)(v->fx_override * 2)
                                        : v->fxdefault;
                    }
                }
                if (timer > 1 && v->staccato)
                {
                    timer--;
                    v->cutpending = 1;
                }
            }
        }
    } while (timer == 0);

    v->pc = pc;
    v->timer = timer;
}

/* ---- the note interpreter (func_80027460) ---- */

static void spawn_mod(FgmNote *n, uint8_t id, uint16_t idx)
{
    const uint8_t *e;
    FgmMod *m, *prev, *cur;

    if (idx >= G.unk_count)
        return;
    e = G.unk + 4 + idx * 16;

    /* Keep the list ordered by slot id; reuse a slot in place. */
    prev = NULL;
    cur = n->mods;
    m = NULL;
    for (;;)
    {
        if (cur == NULL || id < cur->id)
        {
            m = G.mod_free;
            if (m != NULL)
            {
                G.mod_free = m->next;
                if (prev != NULL)
                {
                    m->next = prev->next;
                    prev->next = m;
                }
                else
                {
                    m->next = n->mods;
                    n->mods = m;
                }
            }
            break;
        }
        if (id == cur->id)
        {
            m = cur;
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    if (m == NULL)
        return;

    m->id = id;
    m->shape = e[0];
    m->target = e[1];
    m->postproc = e[2];
    m->period = bef32(e + 4);
    m->amp = bef32(e + 8);
    m->offset = bef32(e + 12);
    m->phase = m->period * (float)e[3] * (1.0f / 256.0f);
    switch (m->shape)
    {
    case 4:
        m->rnd_value = rand1() * m->amp + m->offset;
        m->rnd_period = rand1() * m->period;
        m->phase = m->rnd_period * (float)e[3] * (1.0f / 256.0f);
        break;
    case 5:
        m->rnd_prev = 0.0f;
        m->rnd_value = rand1() * m->amp + m->offset;
        m->rnd_period = rand1() * m->period + 0.5f;
        m->phase = m->rnd_period * (float)e[3] * (1.0f / 256.0f);
        break;
    case 8:
        m->rnd_value = rand2() * m->amp + m->offset;
        m->rnd_period = rand2() * m->period;
        m->phase = m->rnd_period * (float)e[3] * (1.0f / 256.0f);
        break;
    }
}

static void stop_mod(FgmNote *n, uint8_t id)
{
    FgmMod *prev = NULL, *cur = n->mods;
    while (cur != NULL)
    {
        if (cur->id == id)
        {
            if (prev == NULL)
                n->mods = cur->next;
            else
                prev->next = cur->next;
            cur->next = G.mod_free;
            G.mod_free = cur;
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

/* One triangle-ish period out of the game's 2048-entry sine table; a real
 * sinf is closer to the hardware table than any reimplementation. */
static float mod_sine(float frac)
{
    return sinf(frac * 2.0f * 3.14159265f);
}

static void tick_note(FgmNote *n)
{
    float regs[10] = { 0 };
    FgmMod *m;

    if (n->timer != 0 && --n->timer == 0)
    {
        const uint8_t *pc = n->pc;
        uint16_t timer = 0;
        do
        {
            uint8_t instr = *pc++;
            uint16_t t = instr & 0xF;
            if (t & 0x8)
            {
                uint16_t p2 = *pc++;
                t = ((t & 0x7) << 7) + (p2 & 0x7F);
                if (p2 & 0x80)
                    t = (t << 8) + *pc++;
            }
            timer = t;
            switch (instr & 0xF0)
            {
            case 0x00:                          /* vol */
            {
                uint16_t p = *pc++;
                if (p <= 127)
                    n->vol = p;
                else
                {
                    int nv = (int)p - 192 + n->vol;
                    n->vol = (nv < 0) ? 0 : (nv > 127) ? 127 : nv;
                }
                break;
            }
            case 0x10:                          /* pan */
            {
                uint16_t p = *pc++;
                if (p <= 127)
                    n->pan = p;
                else
                {
                    int np = (int)p - 192 + n->pan;
                    n->pan = (np < 0) ? 0 : (np > 127) ? 127 : np;
                }
                break;
            }
            case 0x20:                          /* pitch, s16 cents */
            {
                int16_t p = (int16_t)((pc[0] << 8) | pc[1]);
                pc += 2;
                if (p <= 1200)
                    n->art_cents = (p < -1200) ? -1200 : p;
                else
                {
                    int np = p - 2400 + n->art_cents;
                    n->art_cents = (np < -1200) ? -1200
                                 : (np > 1200) ? 1200 : np;
                }
                break;
            }
            case 0x30:                          /* unk36 -> fx */
            {
                uint16_t p = *pc++;
                if (p <= 127)
                    n->fx = p;
                else
                {
                    int nf = (int)p - 192 + n->fx;
                    n->fx = (nf < 0) ? 0 : (nf > 127) ? 127 : nf;
                }
                break;
            }
            case 0x40:                          /* spawn_mod */
            {
                uint8_t id = *pc++;
                spawn_mod(n, id, rd_varint(&pc));
                break;
            }
            case 0x50:
                stop_mod(n, *pc++);
                break;
            case 0x60:                          /* trigger */
            {
                uint16_t p = rd_varint(&pc);
                if (p < FGM_MAX_SOUNDS && G.sounds[p].present)
                {
                    n->wave = p;
                    n->state = 3;
                }
                break;
            }
            case 0x70:                          /* end */
                n->state = 2;
                timer = 10000;
                break;
            case 0x80:
                n->loopmark = pc;
                break;
            case 0x90:
                pc = n->loopmark;
                break;
            }
        } while (timer == 0);
        n->pc = pc;
        n->timer = timer;
    }

    /* Modulators, every frame. */
    for (m = n->mods; m != NULL; m = m->next)
    {
        float out;
        if (m->shape < 4)
        {
            m->phase += 1.0f;
            if (m->period < m->phase)
                m->phase -= m->period;
        }
        switch (m->shape)
        {
        case 0:
            out = mod_sine(m->phase / m->period) * m->amp + m->offset;
            break;
        case 1:
            out = (m->period / 2.0f < m->phase) ? m->amp : m->offset;
            break;
        case 2:
            out = m->amp * m->phase / m->period + m->offset;
            break;
        case 3:
            out = m->amp * (m->period - m->phase) / m->period + m->offset;
            break;
        case 4:
            m->phase += 1.0f;
            if (m->rnd_period < m->phase)
            {
                m->rnd_value = rand1() * m->amp + m->offset;
                m->rnd_period = rand1() * m->period;
                m->phase = 0;
            }
            out = m->rnd_value;
            break;
        case 5:
            m->phase += 1.0f;
            if (m->rnd_period < m->phase)
            {
                m->rnd_prev = m->rnd_value;
                m->rnd_value = rand1() * m->amp + m->offset;
                m->rnd_period = rand1() * m->period + 0.5f;
                m->phase = 0;
            }
            out = (m->rnd_value - m->rnd_prev) * m->phase / m->rnd_period +
                  m->rnd_prev;
            break;
        case 6:
            m->phase += 1.0f;
            if (m->period < m->phase)
                m->phase = m->period;
            out = m->amp * m->phase / m->period + m->offset;
            break;
        case 7:
            m->phase += 1.0f;
            if (m->period < m->phase)
                m->phase = m->period;
            out = m->amp * (m->period - m->phase) / m->period + m->offset;
            break;
        case 8:
            m->phase += 1.0f;
            if (m->rnd_period < m->phase)
            {
                m->rnd_value = rand2() * m->amp + m->offset;
                m->rnd_period = rand2() * m->period;
                m->phase = 0;
            }
            out = m->rnd_value;
            break;
        default:
            out = 0;
            break;
        }
        switch (m->postproc >> 4)
        {
        case 1:
            out += regs[m->postproc & 0xF];
            break;
        case 2:
            out *= regs[m->postproc & 0xF];
            break;
        }
        switch (m->target)
        {
        case 11:
            out += n->vol;
            /* fallthrough */
        case 10:
            n->vol = (out < 0) ? 0 : (out > 127) ? 127 : (uint8_t)out;
            break;
        case 13:
            out += n->art_cents;
            /* fallthrough */
        case 12:
            n->art_cents = (out < -1200) ? -1200
                         : (out > 1200) ? 1200 : (int16_t)out;
            break;
        case 15:
            out += n->pan;
            /* fallthrough */
        case 14:
            n->pan = (out < 0) ? 0 : (out > 127) ? 127 : (uint8_t)out;
            break;
        default:
            if (m->target < 10)
                regs[m->target] = out;
            else if (m->target >= 16 && m->target < 24)
            {
                /* self-modify: 16/17 period, 18/19 amp, 20/21 offset,
                 * 22/23 phase (set / add). */
                float *f = (m->target < 18) ? &m->period
                         : (m->target < 20) ? &m->amp
                         : (m->target < 22) ? &m->offset : &m->phase;
                if (m->target & 1)
                    *f += out;
                else
                    *f = out;
            }
            else if (m->target >= 24)
            {
                /* modify another mod, by slot id. */
                int slot = (m->target - 24) / 8;
                int which = (m->target - 24) % 8 + 16;
                FgmMod *o = n->mods;
                while (o != NULL && o->id != slot)
                    o = o->next;
                if (o != NULL)
                {
                    float *f = (which < 18) ? &o->period
                             : (which < 20) ? &o->amp
                             : (which < 22) ? &o->offset : &o->phase;
                    if (which & 1)
                        *f += out;
                    else
                        *f = out;
                }
            }
            break;
        }
    }

    /* Output stage. */
    switch (n->state)
    {
    case 1:
    {
        const FgmSound *s = &G.sounds[n->wave];
        /* XBOX: >> rate_shift, not <<. A wave the exporter decimated 2:1
         * (tools/export/ssb_fgmexport.py halve) holds half the samples per
         * second of sound, so it plays at HALF the rate to keep its pitch --
         * as the music does (src/dc/aica/seqsyn.c: rate >> rate_shift). The
         * << played the one such sound (a 214,832-sample B1_sounds2 wave,
         * halved twice) four times too fast. */
        if (n->art_cents != n->art_cents_prev)
            chan_cmd(n->chn, 0, 0, 0, 0, 0,
                     cents_freq(n->art_cents + n->note_cents)
                         >> s->rate_shift, 0, 0,
                     AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_FREQ);
        if (n->vol != n->vol_prev || n->volscale != n->volscale_prev)
            chan_cmd(n->chn, 0, 0, 0, 0, 0, 0, mix_vol(n), 0,
                     AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_VOL);
        if (n->pan != n->pan_prev || n->panbase != n->panbase_prev)
            chan_cmd(n->chn, 0, 0, 0, 0, 0, 0, 0, mix_pan(n),
                     AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_PAN);
        if (n->fx != n->fx_prev || n->fxbase != n->fxbase_prev ||
            n->pan != n->pan_prev || n->panbase != n->panbase_prev)
            fx_send(n);
        break;
    }
    case 2:
        chan_cmd(n->chn, 0, 0, 0, 0, 0, 0, 0, 0,
                 AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_VOL);
        n->state = 0;
        break;
    case 3:
        if (n->wave >= 0 && n->wave < FGM_MAX_SOUNDS &&
            !G.sounds[n->wave].present)
            note_missing(n->wave);
        if (n->wave >= 0 && G.sounds[n->wave].present)
        {
            const FgmSound *s = &G.sounds[n->wave];
            /* XBOX: >> rate_shift, see the update above */
            chan_cmd(n->chn, s->sram, s->nsamples, s->loop, s->ls, s->le,
                     cents_freq(n->art_cents + n->note_cents)
                         >> s->rate_shift,
                     mix_vol(n), mix_pan(n), AICA_CH_CMD_START);
            fx_send(n);
            n->state = 1;
        }
        else
            n->state = 0;
        break;
    }
    n->art_cents_prev = n->art_cents;
    n->vol_prev = n->vol;
    n->pan_prev = n->pan;
    n->fx_prev = n->fx;
    n->volscale_prev = n->volscale;
    n->panbase_prev = n->panbase;
    n->fxbase_prev = n->fxbase;
}

/* ---- public ---- */

void fgm_set_sound(unsigned index, uint32_t sram, uint32_t nsamples,
                   int loop, uint32_t loop_start, uint32_t loop_end,
                   unsigned rate_shift)
{
    if (index >= FGM_MAX_SOUNDS)
        return;
    G.sounds[index].sram = sram;
    G.sounds[index].nsamples = nsamples;
    G.sounds[index].loop = loop;
    G.sounds[index].ls = loop_start;
    G.sounds[index].le = loop_end;
    G.sounds[index].rate_shift = (uint8_t)rate_shift;
    G.sounds[index].present = 1;
}

/* The three pools back to their boot state: every voice, note and
 * modulator free, in index order, and each note on its own channel. */
static void pools_init(void)
{
    int i;

    memset(&G.voices, 0, sizeof(G.voices));
    memset(&G.notes, 0, sizeof(G.notes));
    memset(&G.mods, 0, sizeof(G.mods));
    for (i = 0; i < FGM_VOICES - 1; i++)
        G.voices[i].next = &G.voices[i + 1];
    G.voice_free = &G.voices[0];
    G.voice_live = NULL;
    for (i = 0; i < FGM_NOTES - 1; i++)
        G.notes[i].next = &G.notes[i + 1];
    for (i = 0; i < FGM_NOTES; i++)
        G.notes[i].chn = G.chn_base + i;
    G.note_free = &G.notes[0];
    G.note_live = NULL;
    for (i = 0; i < FGM_MODS - 1; i++)
        G.mods[i].next = &G.mods[i + 1];
    G.mod_free = &G.mods[0];
    sLiveVoices = 0;
}

int fgm_init(const void *ucd, const void *tbl, const void *unk,
             int chn_base)
{
    G.ucd = ucd;
    G.tbl = tbl;
    G.unk = unk;
    G.ucd_count = be32u(ucd);
    G.tbl_count = be32u(tbl);
    G.unk_count = be32u(unk);
    G.chn_base = chn_base;
    G.master = 127;
    G.seed1 = 0;
    G.seed2 = 1;
    sTickAccum = 0;
    pools_init();
    return 0;
}

/* ---- Sound RAM changing hands (src/dc/sndres.c) ---------------------
 * A scene change can move every sample the engine plays, so for the
 * length of the swap the engine must be silent and must not start
 * anything. fgm_suspend takes the engine's lock and keeps it: the audio
 * thread's next fgm_advance waits on it, and the game thread is the one
 * doing the swap. Every live voice is ended the way fgm_stop_all ends
 * them, but the channels are stopped now rather than on the next frame
 * (the next frame is after the samples under them have gone), and the
 * pools go back to their boot state -- no note is left mid-fade on a
 * channel whose sample is about to be overwritten. Handles the game
 * still holds point at voices whose timer is 0, which is what
 * syAudioSweepFGMPlayers and fgm_stop both already treat as ended.
 * Every sound is forgotten; the caller registers the new set with
 * fgm_set_sound and calls fgm_resume. */
void fgm_suspend(void)
{
    int i;

    mutex_lock(&sLock);
    for (i = 0; i < FGM_NOTES; i++)
        chan_cmd(G.chn_base + i, 0, 0, 0, 0, 0, 0, 0, 0, AICA_CH_CMD_STOP);
    pools_init();
    for (i = 0; i < FGM_MAX_SOUNDS; i++)
        G.sounds[i].present = 0;
    memset(sMissed, 0, sizeof(sMissed));
}

void fgm_resume(void)
{
    sTickAccum = 0;
    mutex_unlock(&sLock);
}

/* XBOX: see fgm.h */
void fgm_hold(void)
{
    mutex_lock(&sLock);
}

void fgm_release(void)
{
    mutex_unlock(&sLock);
}

/* n_env.c:4987 func_80026A10_27610: the id check, then func_80026844's
 * allocation with no commit. */
FgmVoice *fgm_alloc(unsigned id)
{
    FgmVoice *v;

    if (id >= G.ucd_count)
        return NULL;
    mutex_lock(&sLock);
    v = alloc_voice(G.ucd + pkg_off(G.ucd, id));
    mutex_unlock(&sLock);
    return v;
}

void fgm_commit(FgmVoice *v)
{
    mutex_lock(&sLock);
    commit_voice(v);
    mutex_unlock(&sLock);
}

/* n_env.c:4996 func_800269C0_275C0 over :5004 func_80026958_27558 --
 * the same allocation, committed before it comes back. */
FgmVoice *fgm_start(unsigned id)
{
    FgmVoice *v;

    if (id >= G.ucd_count)
        return NULL;
    mutex_lock(&sLock);
    v = alloc_voice(G.ucd + pkg_off(G.ucd, id));
    commit_voice(v);
    mutex_unlock(&sLock);
    return v;
}

/* n_env.c:5082 func_80026738_27338, line for line: walk the live list
 * for the voice and for everything forked from it, end each one, put its
 * note into the fade, and splice it back onto the free list. */
void fgm_stop(FgmVoice *sfx)
{
    FgmVoice *v, *prev = NULL;

    mutex_lock(&sLock);
    v = G.voice_live;
    while (v != NULL)
    {
        FgmVoice *next = v->next;

        if ((v == sfx) || (v->parent == sfx))
        {
            v->timer = 0;
            v->serial = 0;
            if (v->note != NULL)
                release_note(v->note);
            if (prev != NULL)
                prev->next = v->next;
            else
                G.voice_live = v->next;
            v->next = G.voice_free;
            G.voice_free = v;
        }
        else
            prev = v;

        v = next;
    }
    mutex_unlock(&sLock);
}

/* n_env.c:5353 / :5386 / :5419: the three per-voice setters. Each writes
 * the voice's own field, pushes it at the note the voice is holding, and
 * repeats for every fork of it. The volume one scales by the *handle's*
 * vol in the fork loop, not the fork's -- that is what the decomp does
 * (it reads arg0->0x22 inside the loop), so it is what happens here.
 *
 * DIVERGES: the mutex. The N64 does not mask interrupts for these three
 * the way it does for the stops, so on the N64 they race the audio
 * interrupt; here they take the same lock as everything else that walks
 * the live list. Nothing observable changes -- the writes are the same
 * writes -- but the list cannot be walked while the pump is editing it.
 */
void fgm_set_volscale(FgmVoice *sfx, unsigned x)
{
    FgmVoice *v;

    if (x > 127)
        x = 127;
    mutex_lock(&sLock);
    sfx->volscale = (uint8_t)x;
    if (sfx->note != NULL)
        sfx->note->volscale = (uint8_t)((sfx->vol * x) >> 7);
    for (v = G.voice_live; v != NULL; v = v->next)
    {
        if (v->parent == sfx)
        {
            v->volscale = (uint8_t)x;
            if (v->note != NULL)
                v->note->volscale = (uint8_t)((sfx->vol * x) >> 7);
        }
    }
    mutex_unlock(&sLock);
}

void fgm_set_pan(FgmVoice *sfx, unsigned x)
{
    FgmVoice *v;

    if (x > 127)
        x = 127;
    mutex_lock(&sLock);
    sfx->pan_override = (uint8_t)x;
    if (sfx->note != NULL)
        sfx->note->panbase = (uint8_t)x;
    for (v = G.voice_live; v != NULL; v = v->next)
    {
        if (v->parent == sfx)
        {
            v->pan_override = (uint8_t)x;
            if (v->note != NULL)
                v->note->panbase = (uint8_t)x;
        }
    }
    mutex_unlock(&sLock);
}

void fgm_set_fx(FgmVoice *sfx, unsigned x)
{
    FgmVoice *v;

    if (x > 127)
        x = 127;
    mutex_lock(&sLock);
    sfx->fx_override = (uint8_t)x;
    if (sfx->note != NULL)
        sfx->note->fxbase = (uint8_t)x;
    for (v = G.voice_live; v != NULL; v = v->next)
    {
        if (v->parent == sfx)
        {
            v->fx_override = (uint8_t)x;
            if (v->note != NULL)
                v->note->fxbase = (uint8_t)x;
        }
    }
    mutex_unlock(&sLock);
}

/* D_8009EDD0_406D0.fgm_ucode_count (n_env.c:216-242, offset 0x28): the
 * number of sounds the engine will start. Every start is checked against
 * it -- fgm_alloc, fgm_start and a script's fork (0xD9) alike -- so 0 lets
 * nothing new start while live voices play on. sc1pgame.c is the one
 * writer the game has: it reaches the same halfword as
 * alSoundEffect.sfx_max to silence everything after Master Hand's
 * defeat sounds, and puts the count back when the defeat ends. */
unsigned fgm_get_count(void)
{
    return G.ucd_count;
}

void fgm_set_count(unsigned count)
{
    /* the game thread's, and twice a boss defeat: safe to log here */
    dbglog(DBG_INFO, "fgm: sound count %u -> %u\n",
           (unsigned)G.ucd_count, count);
    mutex_lock(&sLock);
    G.ucd_count = count;
    mutex_unlock(&sLock);
}

/* n_env.c:5452 func_80026070_26C70. */
void fgm_set_master(unsigned x)
{
    if (x > 127)
        x = 127;
    G.master = (uint8_t)x;
}

static int fgm_pump(void)      /* func_800293A8's pump, in its order */
{
    FgmNote *n, *nprev;
    FgmVoice *v, *vprev;
    int live = 0;

    /* 1. Reap dead notes: silence the channel, orphan their voices,
     * release their modulators. */
    nprev = NULL;
    n = G.note_live;
    while (n != NULL)
    {
        FgmNote *next = n->next;
        if (n->state == 0)
        {
            chan_cmd(n->chn, 0, 0, 0, 0, 0, 0, 0, 0, AICA_CH_CMD_STOP);
            for (v = G.voice_live; v != NULL; v = v->next)
                if (v->note == n)
                    v->note = NULL;
            if (n->mods != NULL)
            {
                FgmMod *tail = n->mods;
                while (tail->next != NULL)
                    tail = tail->next;
                tail->next = G.mod_free;
                G.mod_free = n->mods;
                n->mods = NULL;
            }
            if (nprev != NULL)
                nprev->next = next;
            else
                G.note_live = next;
            n->next = G.note_free;
            G.note_free = n;
        }
        else
            nprev = n;
        n = next;
    }

    /* 2. Voice scripts, 3. note scripts + output. */
    for (v = G.voice_live; v != NULL; v = v->next)
        tick_voice(v);
    for (n = G.note_live; n != NULL; n = n->next)
        tick_note(n);

    /* 4. Reap ended voices. */
    vprev = NULL;
    v = G.voice_live;
    while (v != NULL)
    {
        FgmVoice *next = v->next;
        if (v->timer == 0xFFFF)
        {
            v->timer = 0;
            v->serial = 0;
            if (vprev != NULL)
                vprev->next = next;
            else
                G.voice_live = next;
            v->next = G.voice_free;
            G.voice_free = v;
        }
        else
        {
            vprev = v;
            live++;
        }
        v = next;
    }
    rand2();
    sLiveVoices = live;
    return live;
}

/* See fgm.h and fgm_defer_dbglog above note_missing.
 * Drains whatever the audio thread has queued since the last frame and
 * prints it for real, here, on the caller's thread -- syTaskmanRunFrame
 * calls this once a frame, always on the main thread. */
void fgm_defer_pump(void)
{
    while (sDeferTail != sDeferHead)
    {
        dbglog(sDeferred[sDeferTail].level, "%s", sDeferred[sDeferTail].msg);
        sDeferTail = (sDeferTail + 1) % FGM_DEFER_MAX;
    }
}

void fgm_advance(int32_t dt_us)
{
    /* A long stall -- a scene load, a pack read off the disc -- must not
     * be paid back as a burst of engine frames, which would run every
     * envelope on the game's own timers at whatever speed the loop can
     * manage. The N64 has the same bound for the same reason: the
     * synthesizer only ever calls a client for the frame it is
     * rendering. Four is what bgm.c allows its own clock. */
    if (dt_us > 4 * FGM_TICK_US)
        dt_us = 4 * FGM_TICK_US;

    mutex_lock(&sLock);
    for (sTickAccum += dt_us; sTickAccum >= FGM_TICK_US;
         sTickAccum -= FGM_TICK_US)
        fgm_pump();
    mutex_unlock(&sLock);
}

int fgm_live(void)
{
    return sLiveVoices;
}

/* n_env.c:5128 func_800266A0_272A0. The decomp's first call is
 * func_800264A4_270A4, which folds the two lists an interrupt-time
 * allocation parks its new voices and notes on into the live ones; this
 * port allocates straight into the live lists under the mutex above, so
 * there is nothing to fold and that half has no body here. The rest is
 * line for line: end every live voice, put its note into the fade that
 * release_note gives it, and hand the whole live list back to the pool
 * in one splice. The channels go quiet on the next frame, which is where
 * the decomp silences them too. */
void fgm_stop_all(void)
{
    FgmVoice *v, *last = NULL;

    mutex_lock(&sLock);
    for (v = G.voice_live; v != NULL; v = v->next)
    {
        v->timer = 0;
        v->serial = 0;
        if (v->note != NULL)
            release_note(v->note);
        last = v;
    }
    if (last != NULL)
    {
        last->next = G.voice_free;
        G.voice_free = G.voice_live;
        G.voice_live = NULL;
    }
    sLiveVoices = 0;
    mutex_unlock(&sLock);
}

/* ---- The bank, off the disc ------------------------------------------
 * The game keeps its FGM tables in the ROM's audio segment and its
 * samples in the RSP's bank; here tools/export/ssb_fgmexport.py has staged
 * both: the three bytecode files verbatim and the bank sounds as AICA
 * ADPCM. fgm_load_tables brings the engine up on the tables alone;
 * src/dc/sndres.c uploads each scene's samples. */
#include <stdio.h>
#include <stdlib.h>

/* dir == NULL means the asset root; a caller that names a directory gets
 * a path, which asset_read_whole passes through as one (assetroot.h). */
static void *fgm_read_whole(const char *dir, const char *name)
{
    char path[ASSET_PATH_MAX];
    long n;

    if (dir != NULL)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, name);
        return asset_read_whole(path, &n);
    }
    return asset_read_whole(name, &n);
}

int fgm_load_tables(const char *dir, int chn_base)
{
    void *ucd, *tbl, *unk;

    ucd = fgm_read_whole(dir, "fgm.ucd");
    tbl = fgm_read_whole(dir, "fgm.tbl");
    unk = fgm_read_whole(dir, "fgm.unk");
    if (!ucd || !tbl || !unk)
    {
        free(ucd);
        free(tbl);
        free(unk);
        return -1;
    }
    return fgm_init(ucd, tbl, unk, chn_base);
}
