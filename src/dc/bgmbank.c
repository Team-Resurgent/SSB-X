/* The music bank in AICA sound RAM -- see bgmbank.h.
 *
 * Everything but the sample upload is target-independent, so the file
 * also builds for the host: tools/check/seqcore_oracle.c reads a real bank
 * through this same parser, and only the trip to sound RAM is skipped
 * (there is no AICA to put it in). */
#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/sound/sound.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assetroot.h"
#include "bgmbank.h"

#define BGM_MAGIC "SSBBGM1\0"
#define BGM_CHUNK 32768

BGMBank gBGMBank;

typedef struct
{
    char magic[8];
    uint32_t n_insts, n_sounds, n_waves, n_seqs;
    uint32_t off_insts, off_sounds, off_waves, off_seqs;
    uint32_t off_seqdata, size_seqdata;
    uint32_t off_blob, size_blob;
    uint32_t rate;
} BGMPakHdr;

/* The pack is written by tools/export/ssb_bgmpack.py's struct formats; if a row
 * ever grows on one side and not the other the file reads as garbage. */
_Static_assert(sizeof(BGMPakHdr) == 60, "pack header");
_Static_assert(sizeof(BGMInstrument) == 20, "pack instrument");
_Static_assert(sizeof(BGMSound) == 28, "pack sound");
_Static_assert(sizeof(BGMWaveTable) == 24, "pack wavetable");
_Static_assert(sizeof(BGMSeqEntry) == 32, "pack sequence entry");

/* The tables half of bgm_bank_load, and the whole of
 * bgm_bank_load_tables. The file is left open in `af` for the caller to
 * read the blob from, or closed on failure. */
static int bank_read_tables(BGMBank *bank, const char *name, AssetFile *af_out,
                            BGMPakHdr *hd_out)
{
    BGMPakHdr hd;
    AssetFile af;
    uint8_t *tables = NULL;
    uint32_t tables_size;
    int i;

    memset(bank, 0, sizeof(*bank));
    bank->percussion = -1;

    if (asset_open(&af, name) < 0)
        return -1;
    if (asset_read(&af, &hd, sizeof(hd)) != (long)sizeof(hd) ||
        memcmp(hd.magic, BGM_MAGIC, 8) != 0)
    {
        asset_close(&af);
        return -1;
    }

    /* Everything but the samples, in one read from the start of the file:
     * the tables are contiguous from off_insts through the end of the
     * sequence bytes, and a read that begins at zero into a 32-byte
     * aligned buffer is the one KOS's ISO9660 driver will DMA
     * (src/dc/assetroot.h). Reading from off_insts instead -- 60 bytes
     * in, which is not a 32-byte boundary -- put all 82 sectors of the
     * tables through the driver's sector cache, and that was the whole
     * of the port's 1.38 second boot read. The 60 bytes of
     * header the buffer now carries along are the price. */
    tables_size = hd.off_seqdata + hd.size_seqdata;
    tables = asset_alloc(tables_size);
    if (!tables)
    {
        asset_close(&af);
        return -1;
    }
    asset_seek(&af, 0);
    if (asset_read(&af, tables, (long)tables_size) != (long)tables_size)
        goto fail;

    bank->tables = tables;
    bank->insts = (BGMInstrument *)(tables + hd.off_insts);
    bank->sounds = (BGMSound *)(tables + hd.off_sounds);
    bank->waves = (BGMWaveTable *)(tables + hd.off_waves);
    bank->seqs = (BGMSeqEntry *)(tables + hd.off_seqs);
    bank->seqdata = tables + hd.off_seqdata;
    bank->n_insts = hd.n_insts;
    bank->n_sounds = hd.n_sounds;
    bank->n_waves = hd.n_waves;
    bank->n_seqs = hd.n_seqs;
    bank->rate = (int32_t)hd.rate;

    /* The percussion instrument is the pack's last program; the game hangs
     * it on channel 9 (n_seqplayer.c:1085 __n_initFromBank). */
    for (i = (int)hd.n_insts - 1; i >= 0; i--)
    {
        if (bank->insts[i].valid)
        {
            bank->percussion = i;
            break;
        }
    }

    bank->off_blob = hd.off_blob;
    bank->sram_size = hd.size_blob;
    bank->wave_sram = calloc(hd.n_waves ? hd.n_waves : 1, sizeof(uint32_t));
    if (!bank->wave_sram)
        goto fail;
    *af_out = af;
    *hd_out = hd;
    return 0;

fail:
    free(tables);
    asset_close(&af);
    memset(bank, 0, sizeof(*bank));
    bank->percussion = -1;
    return -1;
}

int bgm_bank_load_tables(BGMBank *bank, const char *name)
{
    AssetFile af;
    BGMPakHdr hd;

    if (bank_read_tables(bank, name, &af, &hd) < 0)
        return -1;
    asset_close(&af);
    return 0;
}

int bgm_bank_load(BGMBank *bank, const char *name)
{
    AssetFile af;
    BGMPakHdr hd;
#ifdef _arch_dreamcast
    uint8_t *chunk = NULL;
    uint32_t done, i;
#endif

    if (bank_read_tables(bank, name, &af, &hd) < 0)
        return -1;

#ifndef _arch_dreamcast
    /* Host build: the tables are all the oracle needs. */
    bank->sram = 0;
    asset_close(&af);
    return 0;
#else
    /* The samples go straight to sound RAM, a chunk at a time, so the
     * 890 KB blob is never resident in main RAM. */
    bank->sram = snd_mem_malloc(hd.size_blob);
    if (!bank->sram)
    {
        dbglog(DBG_ERROR, "bgm: no room in sound RAM for %u bytes\n",
               (unsigned)hd.size_blob);
        goto fail;
    }

    /* 32-byte aligned, and so is off_blob (tools/export/ssb_bgmpack.py:247
     * rounds it up), so every one of these reads but the last takes the
     * disc driver's DMA stream rather than its sector cache. */
    chunk = asset_alloc(BGM_CHUNK);
    if (!chunk)
        goto fail;
    asset_seek(&af, (long)hd.off_blob);
    for (done = 0; done < hd.size_blob; )
    {
        uint32_t n = hd.size_blob - done;

        if (n > BGM_CHUNK)
            n = BGM_CHUNK;
        if (asset_read(&af, chunk, (long)n) != (long)n)
            goto fail;
        spu_memload_sq(bank->sram + done, chunk, (n + 31) & ~31u);
        done += n;
    }
    free(chunk);
    asset_close(&af);
    for (i = 0; i < bank->n_waves; i++)
        bank->wave_sram[i] = bank->sram + bank->waves[i].off / 2;
    return 0;

fail:
    free(chunk);
    asset_close(&af);
    bgm_bank_free(bank);
    return -1;
#endif
}

void bgm_bank_free(BGMBank *bank)
{
#ifdef _arch_dreamcast
    if (bank->sram)
        snd_mem_free(bank->sram);
#endif
    free(bank->wave_sram);
    free(bank->tables);
    memset(bank, 0, sizeof(*bank));
    bank->percussion = -1;
}

const BGMInstrument *bgm_bank_program(const BGMBank *bank, unsigned prog)
{
    if (prog >= bank->n_insts || !bank->insts[prog].valid)
        return NULL;
    return &bank->insts[prog];
}

/* A note on picks its sample the way the game does: a binary search
 * over the instrument's sounds, not a scan (n_seqplayer.c:332). With
 * overlapping key or velocity ranges the two can pick different sounds,
 * so this probes the same entries in the same order -- the lower of two
 * middles -- and turns left exactly when the game does: the key is below
 * the probe's range, or inside it with the velocity below. */
int bgm_bank_lookup_sound(const BGMBank *bank, const BGMInstrument *inst,
                          uint8_t key, uint8_t vel)
{
    const BGMSound *base = &bank->sounds[inst->soundFirst];
    int lo = 0, hi = inst->soundCount;  /* the candidates are [lo, hi) */

    while (lo < hi)
    {
        int mid = lo + (hi - lo - 1) / 2;
        const BGMSound *s = &base[mid];
        int key_in = key >= s->keyMin && key <= s->keyMax;

        if (key_in && vel >= s->velocityMin && vel <= s->velocityMax)
            return (int)inst->soundFirst + mid;
        if (key < s->keyMin || (key_in && vel < s->velocityMin))
            hi = mid;
        else
            lo = mid + 1;
    }
    return -1;
}
