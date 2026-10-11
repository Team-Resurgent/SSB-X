/* bgmbank.h -- the music bank, resident in the AICA's sound RAM.
 *
 * tools/export/ssb_bgmpack.py writes one file holding B1_sounds1's ALBankFile
 * flattened little-endian, every compressed-MIDI sequence out of
 * S1_music.sbk verbatim, and every waveform re-encoded as Yamaha 4-bit
 * ADPCM. Loading it puts the samples where the AICA decodes them in
 * hardware and keeps nothing but the tables and the sequence bytes -- a
 * little over 190 KB -- in main RAM. The pre-rendered WAV route this
 * replaces cost 3.3 MB of main RAM per track.
 *
 * The struct names are the libaudio types they flatten (PR/libaudio.h:
 * ALBank 282, ALInstrument 263, ALSound 253, ALKeyMap, ALEnvelope,
 * ALWaveTable 240), and the field names are theirs, so bgm.c reads the
 * bank with the same expressions the decomp's sequence player uses.
 */
#ifndef DC_BGMBANK_H
#define DC_BGMBANK_H

#include <stdint.h>

/* PR/libaudio.h:263 ALInstrument, minus soundArray -- the sounds are a
 * range in the flat table, kept in soundArray order because
 * __n_lookupSoundQuick (n_seqplayer.c:332) binary-searches it. */
typedef struct
{
    uint8_t volume, pan, priority, flags;
    uint8_t tremType, tremRate, tremDepth, tremDelay;
    uint8_t vibType, vibRate, vibDepth, vibDelay;
    int16_t bendRange;
    uint16_t soundFirst, soundCount;
    uint16_t valid;             /* 0 for the bank's NULL program slots */
} BGMInstrument;

/* ALEnvelope + ALKeyMap + ALSound, flattened. */
typedef struct
{
    int32_t attackTime, decayTime, releaseTime;
    uint8_t attackVolume, decayVolume;
    uint8_t velocityMin, velocityMax;
    uint8_t keyMin, keyMax, keyBase;
    int8_t detune;
    uint8_t samplePan, sampleVolume, flags, pad;
    uint16_t wave;
    uint16_t pad2;
} BGMSound;

/* PR/libaudio.h:240 ALWaveTable, as the AICA needs it. `sram` is filled
 * in by bgm_bank_load; everything else comes from the pack. */
typedef struct
{
    uint32_t off;               /* samples into the ADPCM blob */
    uint32_t nsamples;
    uint32_t loopStart, loopEnd;
    uint32_t loopCount;         /* 0 none, 0xFFFFFFFF forever */
    uint8_t rateShift;          /* wave rate = bank rate >> this */
    uint8_t pad[3];
} BGMWaveTable;

typedef struct
{
    uint32_t off, size;         /* into the sequence blob */
    char name[24];
} BGMSeqEntry;

typedef struct
{
    BGMInstrument *insts;
    BGMSound *sounds;
    BGMWaveTable *waves;
    BGMSeqEntry *seqs;
    uint8_t *seqdata;           /* writable: the loop counters live here */
    uint32_t n_insts, n_sounds, n_waves, n_seqs;
    int percussion;             /* instrument index, or -1 */
    int32_t rate;               /* the bank's sample rate (32000) */
    uint32_t sram;              /* AICA address of the whole blob, when
                                 * bgm_bank_load put it there; else 0 */
    uint32_t sram_size;         /* the blob's size in the pack */
    uint32_t off_blob;          /* where the blob starts in the pack */
    /* Where each wave is in sound RAM, 0 if it is not there. bgm_bank_load
     * fills every entry; after bgm_bank_load_tables they are all 0 until
     * src/dc/sndres.c uploads a scene's waves and writes theirs. A note on
     * a wave that is not resident is silent (aica/seqsyn.c) and logged. */
    uint32_t *wave_sram;
    void *tables;               /* the one allocation the above point into */
} BGMBank;

/* The one bank the port keeps resident, the way the game keeps one. */
extern BGMBank gBGMBank;

/* Read the named .pak (tools/export/ssb_bgmpack.py's), off whichever medium
 * the asset root names (src/dc/assetroot.h), upload its samples to
 * sound RAM and fill in `bank`. The blob never lands in main RAM: it is
 * streamed to the AICA a chunk at a time. Returns 0, or -1 with nothing
 * allocated. snd_init must have run. */
int bgm_bank_load(BGMBank *bank, const char *name);

/* The same, but no samples: the tables, and wave_sram all 0. The game's
 * path since the audio step -- sound RAM cannot hold the music bank and
 * the VS game's sound effects at once, so src/dc/sndres.c uploads the
 * waves each scene's tracks select. 0, or -1 with nothing allocated. */
int bgm_bank_load_tables(BGMBank *bank, const char *name);

/* Release the tables and, if bgm_bank_load uploaded it, the sound RAM. */
void bgm_bank_free(BGMBank *bank);

/* The instrument a program change selects, NULL if the slot is empty.
 * n_env.c:2742 AL_MIDI_ProgramChange. */
const BGMInstrument *bgm_bank_program(const BGMBank *bank, unsigned prog);

/* The sample a note on plays: the same binary search over the
 * instrument's sounds the game makes (n_seqplayer.c:332), so overlapping
 * ranges resolve as they do on the N64. Returns an index into
 * bank->sounds, or -1. */
int bgm_bank_lookup_sound(const BGMBank *bank, const BGMInstrument *inst,
                          uint8_t key, uint8_t vel);

#endif /* DC_BGMBANK_H */
