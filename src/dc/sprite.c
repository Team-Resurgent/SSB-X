/* sprite.c -- see sprite.h. The file format is
 * tools/export/ssb_spriteexport.py's; the two record layouts below are its
 * SPRITE_RECORD and BITMAP_RECORD, little-endian, and the static asserts hold
 * the C to them. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assetroot.h"
#include "sprite.h"
#include "taskman.h"

#include <sys/debug.h>
#include <PR/gbi.h>

#ifndef FT_HOSTTEST
#include <kos.h>
#include "dcpvr.h"
#endif

typedef struct SprHeader
{
    char magic[8];              /* "SSBSPR1\0" */
    u32 count;
    u32 file_id;
    u32 bitmaps_off;
    u32 tex_off;
    u32 tex_size;
    u32 luts_off;               /* one u32 per texture, the palette's file
                                 * offset (0 in a bank written before it) */
} SprHeader;

typedef struct SprRecord
{
    char name[16];
    u32 file_off;
    s16 x, y, width, height;
    f32 scalex, scaley;
    s16 expx, expy;
    u16 attr;
    s16 zdepth;
    u8 red, green, blue, alpha;
    s16 startTLUT, nTLUT;
    s16 istart, istep, nbitmaps, ndisplist, bmheight, bmHreal;
    u8 bmfmt, bmsiz, pvrfmt, nluts;
    u16 texw, texh, imgw, imgh;
    u32 tex_off, tex_size;      /* the first texture, and each one's size */
    u32 bitmaps_off;
} SprRecord;

typedef struct SprBitmap
{
    s16 width, width_img, s, t;
    u32 buf;
    s16 actualHeight, LUToffset;
} SprBitmap;

typedef char spr_header_size_check[sizeof(SprHeader) == 32 ? 1 : -1];
typedef char spr_record_size_check[sizeof(SprRecord) == 88 ? 1 : -1];
typedef char spr_bitmap_size_check[sizeof(SprBitmap) == 16 ? 1 : -1];

/* How many banks can be resident at once. The character select loads every fighter's stock-icon bank on top
 * of its own six (mnPlayersVSFuncStart's twelve ftManagerSetupFilesAllKind
 * calls, which are the game's): a bank past the limit is loaded but
 * never given back, and its records outlive the heap they are in. */
#define SPRITE_BANKS_MAX 32

static SpriteBank *sBanks[SPRITE_BANKS_MAX];
static int sBankCount;

static void sprite_bank_release(SpriteBank *bank);
static void sprite_bank_forget(SpriteBank *bank);

/* The records' allocator: the scene heap, or for a resident bank the C
 * heap, which outlives every scene. */
static void *sprite_alloc(sb32 resident, size_t size)
{
    return resident ? malloc(size) : syTaskmanMalloc(size, 0x8);
}

static int sprite_bank_load_in(SpriteBank *bank, const char *name,
                               sb32 resident);

int sprite_bank_load(SpriteBank *bank, const char *name)
{
    return sprite_bank_load_in(bank, name, FALSE);
}

int sprite_bank_load_resident(SpriteBank *bank, const char *name)
{
    if (bank->is_loaded)
    {
        return 0;
    }
    return sprite_bank_load_in(bank, name, TRUE);
}

static int sprite_bank_load_in(SpriteBank *bank, const char *name,
                               sb32 resident)
{
    /* The caller names a file; where the files are, and how they are
     * read, is the asset root's business (src/dc/assetroot.h) -- /rd on
     * a romdisk build and /cd on a disc one, through an
     * aligned fs_read (newlib stdio's 1 KB buffer would put every disc
     * sector through the driver's cache). */
    long size;
    u8 *blob;
    const SprHeader *hd;
    DCSpriteTex *texs;
    int **luts;
    u32 *lut_offs;
    u32 i, bm, nt;

    /* Whoever empties the scene heap has to unload the banks first: the
     * records below are syTaskmanMalloc'd out of it, and is_loaded is
     * what ftManagerSetupFilesAllKind reads as "already resident". The
     * scene manager does it at every scene change; this is for the case
     * it cannot see, a scene that re-inits the heap without ending
     * (src/dc/taskman.h). Installed here so a build with no bank in it
     * never has the hook at all. */
    syTaskmanAddHeapResetHook(sprite_bank_release_all);

#ifndef FT_HOSTTEST
    /* Every texel below goes into VRAM the renderer may still be reading
     * (dcpvr.h dc_pvr_vram_fence). One fence covers the whole load: it is
     * the release and the uploads, and nothing between them submits a
     * scene. */
    dc_pvr_vram_fence();
#endif

    /* a bank loaded twice is loaded again: give the first upload's VRAM
     * back and take its registry slot with it (sprite_bank_release) */
    if (bank->is_loaded)
    {
        sprite_bank_release(bank);
        sprite_bank_forget(bank);
    }
    memset(bank, 0, sizeof(*bank));
    /* the file is read whole and kept only until the textures are in
     * VRAM; on the host, where there is no VRAM, the texels are never
     * looked at and the buffer is freed just the same */
    blob = asset_read_whole(name, &size);
    if (blob == NULL)
    {
        syDebugPrintf("sprite: cannot read %s\n", name);
        return -1;
    }

    hd = (const SprHeader *)blob;
    if (size < (long)sizeof(*hd) || memcmp(hd->magic, "SSBSPR1", 8) != 0)
    {
        syDebugPrintf("sprite: %s: bad magic\n", name);
        free(blob);
        return -1;
    }
    bank->file_id = hd->file_id;
    bank->count = hd->count;
    bank->entries = sprite_alloc(resident, sizeof(DCSpriteEntry) * hd->count);
    bm = 0;
    nt = 0;
    for (i = 0; i < hd->count; i++)
    {
        const SprRecord *r = (const SprRecord *)(blob + sizeof(*hd)) + i;

        bm += r->nbitmaps;
        nt += (r->nluts != 0) ? r->nluts : 1;
    }
    bank->bitmaps = sprite_alloc(resident, sizeof(Bitmap) * bm);
    texs = sprite_alloc(resident, sizeof(DCSpriteTex) * nt);
    luts = sprite_alloc(resident, sizeof(int *) * nt);
    lut_offs = sprite_alloc(resident, sizeof(u32) * nt);
    if (bank->entries == NULL || bank->bitmaps == NULL || texs == NULL ||
        luts == NULL || lut_offs == NULL)
    {
        syDebugPrintf("sprite: %s: no %s for %lu sprites\n", name,
                      resident ? "memory" : "scene heap",
                      (unsigned long)hd->count);
        free(blob);
        return -1;
    }
    bm = 0;
    nt = 0;
    for (i = 0; i < hd->count; i++)
    {
        const SprRecord *r = (const SprRecord *)(blob + sizeof(*hd)) + i;
        const SprBitmap *sb = (const SprBitmap *)(blob + r->bitmaps_off);
        DCSpriteEntry *e = &bank->entries[i];
        Sprite *sp = &e->sprite;
        u32 nl = (r->nluts != 0) ? r->nluts : 1;
        u32 l;
        s32 k;

        memcpy(e->name, r->name, sizeof(e->name));
        e->name[sizeof(e->name) - 1] = '\0';
        e->file_off = r->file_off;

        memset(sp, 0, sizeof(*sp));
        sp->x = r->x;
        sp->y = r->y;
        sp->width = r->width;
        sp->height = r->height;
        sp->scalex = r->scalex;
        sp->scaley = r->scaley;
        sp->expx = r->expx;
        sp->expy = r->expy;
        sp->attr = r->attr;
        sp->zdepth = r->zdepth;
        sp->red = r->red;
        sp->green = r->green;
        sp->blue = r->blue;
        sp->alpha = r->alpha;
        sp->startTLUT = r->startTLUT;
        sp->nTLUT = r->nTLUT;
        sp->LUT = NULL;         /* set below: a CI sprite's names its texture */
        sp->istart = r->istart;
        sp->istep = r->istep;
        sp->nbitmaps = r->nbitmaps;
        sp->ndisplist = r->ndisplist;
        sp->bmheight = r->bmheight;
        sp->bmHreal = r->bmHreal;
        sp->bmfmt = r->bmfmt;
        sp->bmsiz = r->bmsiz;
        sp->rsp_dl = sp->rsp_dl_next = NULL;
        sp->frac_s = sp->frac_t = 0;

        e->texs = &texs[nt];
        e->luts = &luts[nt];
        e->lut_offs = &lut_offs[nt];
        for (l = 0; l < nl; l++)
        {
            e->lut_offs[l] = (hd->luts_off != 0)
                ? ((const u32 *)(blob + hd->luts_off))[nt + l] : 0;
        }
        e->nluts = nl;
        nt += nl;
        for (l = 0; l < nl; l++)
        {
            DCSpriteTex *t = &e->texs[l];

            t->texw = r->texw;
            t->texh = r->texh;
            t->imgw = r->imgw;
            t->imgh = r->imgh;
            t->fmt = r->pvrfmt;
            t->txr = NULL;
            e->luts[l] = (int *)t;
#ifndef FT_HOSTTEST
            t->txr = pvr_mem_malloc((u32)r->texw * r->texh * 2);
            if (t->txr == NULL)
            {
                syDebugPrintf("sprite: %s: no VRAM for %s (%ux%u)\n", name,
                              e->name, (unsigned)r->texw, (unsigned)r->texh);
                free(blob);
                return -1;
            }
            pvr_txr_load_ex(blob + hd->tex_off + r->tex_off + l * r->tex_size,
                            t->txr, r->texw, r->texh, PVR_TXRLOAD_16BPP);
            bank->vram_bytes += (u32)r->texw * r->texh * 2;
#endif
        }
        if (sp->bmfmt == G_IM_FMT_CI)
        {
            sp->LUT = e->luts[0];
        }
        e->bitmaps = &bank->bitmaps[bm];
        sp->bitmap = e->bitmaps;
        for (k = 0; k < r->nbitmaps; k++)
        {
            Bitmap *b = &e->bitmaps[k];

            b->width = sb[k].width;
            b->width_img = sb[k].width_img;
            b->s = sb[k].s;
            b->t = sb[k].t;
            b->buf = &e->texs[0];
            b->actualHeight = sb[k].actualHeight;
            b->LUToffset = sb[k].LUToffset;
        }
        bm += r->nbitmaps;
    }
    free(blob);
    bank->is_loaded = TRUE;

    if (resident)
    {
        /* never registered, so no scene change releases it */
        syDebugPrintf("sprite: %s: file %lu, %lu sprites, %lu bytes of "
                      "VRAM, resident\n", name, (unsigned long)bank->file_id,
                      (unsigned long)bank->count,
                      (unsigned long)bank->vram_bytes);
        return 0;
    }
    if (sBankCount < SPRITE_BANKS_MAX)
    {
        sBanks[sBankCount++] = bank;
    }
    else
    {
        syDebugPrintf("sprite: %s: more than %d banks loaded; this one's "
                      "VRAM will leak\n", name, SPRITE_BANKS_MAX);
    }
    syDebugPrintf("sprite: %s: file %lu, %lu sprites, %lu bytes of VRAM\n",
                  name, (unsigned long)bank->file_id,
                  (unsigned long)bank->count,
                  (unsigned long)bank->vram_bytes);
    return 0;
}

DCSpriteEntry *sprite_bank_entry(SpriteBank *bank, u32 file_off)
{
    u32 i;

    if (bank == NULL || !bank->is_loaded)
    {
        syDebugPrintf("sprite: bank not loaded (offset %#lx)\n",
                      (unsigned long)file_off);
        return NULL;
    }
    for (i = 0; i < bank->count; i++)
    {
        if (bank->entries[i].file_off == file_off)
        {
            return &bank->entries[i];
        }
    }
    syDebugPrintf("sprite: file %lu has no sprite at %#lx\n",
                  (unsigned long)bank->file_id, (unsigned long)file_off);
    return NULL;
}

Sprite *sprite_bank_get(SpriteBank *bank, u32 file_off)
{
    DCSpriteEntry *e = sprite_bank_entry(bank, file_off);

    return (e != NULL) ? &e->sprite : NULL;
}

int *sprite_bank_lut(SpriteBank *bank, u32 lut_off)
{
    u32 i, l;

    if (bank == NULL || !bank->is_loaded)
    {
        syDebugPrintf("sprite: bank not loaded (palette %#lx)\n",
                      (unsigned long)lut_off);
        return NULL;
    }
    for (i = 0; i < bank->count; i++)
    {
        DCSpriteEntry *e = &bank->entries[i];

        for (l = 0; l < e->nluts; l++)
        {
            if (e->lut_offs[l] == lut_off)
            {
                return e->luts[l];
            }
        }
    }
    syDebugPrintf("sprite: file %lu has no sprite drawn with the palette at %#lx\n",
                  (unsigned long)bank->file_id, (unsigned long)lut_off);
    return NULL;
}

/* One bank's textures given back. Split out of sprite_bank_release_all
 * because that is not the only way a bank is released:
 * a scene that loads the same bank twice inside one visit has to give
 * the first upload back, or the VRAM is lost until the scene ends and
 * the second sprite_bank_release_all frees a pointer that is already
 * gone. The unlock message is the scene that does it -- its task runs
 * once per queued message, and every run re-inits the scene heap the
 * records live in, so every run re-loads (src/dc/mnmessage.c). */
static void sprite_bank_release(SpriteBank *bank)
{
#ifndef FT_HOSTTEST
    u32 k;

    /* The renderer may still be reading what this is about to overwrite
     * (dcpvr.h dc_pvr_vram_fence). */
    dc_pvr_vram_fence();

    for (k = 0; k < bank->count; k++)
    {
        u32 l;

        for (l = 0; l < bank->entries[k].nluts; l++)
        {
            if (bank->entries[k].texs[l].txr != NULL)
            {
                pvr_mem_free(bank->entries[k].texs[l].txr);
            }
        }
    }
#endif
    /* the entries are in the scene heap, which the next scene's setup
     * re-inits; nothing to free, but nothing to keep either */
    bank->entries = NULL;
    bank->bitmaps = NULL;
    bank->count = 0;
    bank->vram_bytes = 0;
    bank->is_loaded = FALSE;
}

/* Drop one bank from the registry, keeping the rest in order: the other
 * half of a re-load, so the bank is registered once however many times
 * it is loaded. */
static void sprite_bank_forget(SpriteBank *bank)
{
    int i, n = 0;

    for (i = 0; i < sBankCount; i++)
    {
        if (sBanks[i] != bank)
        {
            sBanks[n++] = sBanks[i];
        }
    }
    sBankCount = n;
}

void sprite_bank_release_all(void)
{
    int i;

    for (i = 0; i < sBankCount; i++)
    {
        sprite_bank_release(sBanks[i]);
        sBanks[i] = NULL;
    }
    sBankCount = 0;
}
