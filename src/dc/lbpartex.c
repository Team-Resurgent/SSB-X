/* lbpartex.c -- see lbpartex.h. The .txp reader.
 *
 * The file's shape is tools/export/ssb_particletexexport.py's docstring; this
 * is the other half of that sentence. Nothing here is the game's: the whole
 * file is the DIVERGES lbpartex.h describes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ssb_types.h>
#include <lb/library.h>

#include "assetroot.h"
#include "lbpartex.h"

#ifdef _arch_dreamcast
#include <kos.h>
#include <malloc.h>
#endif
#include "dcpvr.h"

#define LBPTEX_BANKS    8       /* lb/lbparticle.c:1807 masks bank_id with 7 */
#define LBPTEX_MAGIC    0x50585450  /* 'P','T','X','P' little-endian */

/* The exporter's PVRTEX_*; translated to the PVR's own on load so that
 * nothing downstream has to know the file's spelling. */
#define TXP_ARGB1555    1
#define TXP_ARGB4444    2

#ifdef _arch_dreamcast
_Static_assert((__typeof__(((LBPTex *)0)->fmt))PVR_TXRFMT_ARGB4444 ==
                   PVR_TXRFMT_ARGB4444,
               "LBPTex.fmt must hold the PVR's texture format constants");
#endif

typedef struct TxpHeader
{
    uint32_t magic;
    uint32_t textures_num;
    uint32_t frames_num;
    uint32_t texels_off;
} TxpHeader;

typedef struct TxpTexture
{
    uint32_t first_frame;
    uint16_t frames;
    uint16_t fmt;
    uint16_t w, h;
    uint16_t src_w, src_h;
} TxpTexture;

typedef struct LBPTexBank
{
    char       name[24];
    int        textures_num;
    int        frames_num;
    TxpTexture *texes;          /* textures_num of them */
    LBPTex     *frames;         /* frames_num of them, flat */
    uint32_t   bytes;           /* what the PVR holds for this bank */
} LBPTexBank;

static LBPTexBank sLBPTexBanks[LBPTEX_BANKS];

/* lb/lbparticle.c's own bank arrays, which the game declares without
 * `static` and src/dc/lbparticle.c keeps that way. */
extern s32 sLBParticleTextureBanksNum[];
extern LBTexture **sLBParticleTextureBanks[];

static void bank_free(LBPTexBank *b)
{
    int i;

    /* The renderer may still be reading what this is about to overwrite
     * (dcpvr.h dc_pvr_vram_fence). */
    dc_pvr_vram_fence();

    for (i = 0; i < b->frames_num; i++)
    {
#ifdef _arch_dreamcast
        if (b->frames[i].txr != NULL)
        {
            pvr_mem_free(b->frames[i].txr);
        }
#endif
    }
    free(b->frames);
    free(b->texes);
    memset(b, 0, sizeof(*b));
}

void lbpTexFreeAll(void)
{
    int i;

    for (i = 0; i < LBPTEX_BANKS; i++)
    {
        bank_free(&sLBPTexBanks[i]);
    }
}

const LBPTex *lbpTexGet(int bank_id, int texture_id, int frame_id)
{
    const LBPTexBank *b;
    const TxpTexture *t;

    if (bank_id < 0 || bank_id >= LBPTEX_BANKS)
    {
        return NULL;
    }
    b = &sLBPTexBanks[bank_id & (LBPTEX_BANKS - 1)];

    if (texture_id < 0 || texture_id >= b->textures_num)
    {
        return NULL;
    }
    t = &b->texes[texture_id];

    if (frame_id < 0 || frame_id >= (int)t->frames)
    {
        return NULL;
    }
    return &b->frames[t->first_frame + frame_id];
}

/* Every loaded bank's frames, by the address the RDP would have been
 * pointed at. Linear, because the renderer asks only when the address
 * changes -- lb/lbparticle.c:1929 emits a texture load only then -- and
 * because the longest bank is 202 frames. */
const LBPTex *lbpTexForImage(const void *image)
{
    int i, f;

    if (image == NULL)
    {
        return NULL;
    }
    for (i = 0; i < LBPTEX_BANKS; i++)
    {
        const LBPTexBank *b = &sLBPTexBanks[i];

        for (f = 0; f < b->frames_num; f++)
        {
            if (b->frames[f].img == image)
            {
                return &b->frames[f];
            }
        }
    }
    return NULL;
}

/* The bank the game pointerized, walked once so that every frame of the
 * pack knows the .txb address it came from. It is also a check: the pack
 * was cut from that same file at build time, so a disagreement about how
 * many textures a bank has, or how many frames one of them has, is a
 * pack that does not belong to this ROM. The bank is absent in the host
 * build (src/dc/scvsbattle.c says why) and then every img stays NULL,
 * which lbpTexForImage reads as "no such texture". */
static int bank_bind_images(LBPTexBank *b, int bank_id)
{
    int i, f;

    if (sLBParticleTextureBanks[bank_id] == NULL)
    {
        return 0;
    }
    if (sLBParticleTextureBanksNum[bank_id] != b->textures_num)
    {
        return -1;
    }
    for (i = 0; i < b->textures_num; i++)
    {
        const LBTexture *rt = sLBParticleTextureBanks[bank_id][i];

        if (rt == NULL || (int)rt->count != (int)b->texes[i].frames)
        {
            return -1;
        }
        for (f = 0; f < (int)b->texes[i].frames; f++)
        {
            b->frames[b->texes[i].first_frame + f].img = rt->data[f];
        }
    }
    return 0;
}

uint32_t lbpTexBytes(int *frames_out)
{
    uint32_t bytes = 0;
    int frames = 0, i;

    for (i = 0; i < LBPTEX_BANKS; i++)
    {
        bytes += sLBPTexBanks[i].bytes;
        frames += sLBPTexBanks[i].frames_num;
    }
    if (frames_out != NULL)
    {
        *frames_out = frames;
    }
    return bytes;
}

/* memalign(32) where it matters: asset_read hands a 32-byte aligned
 * destination to the GD-ROM's DMA path and copies sector at a time
 * otherwise (src/dc/assetroot.h). */
static void *stage_alloc(size_t n)
{
#ifdef _arch_dreamcast
    return memalign(32, n);
#else
    return malloc(n);
#endif
}

int lbpTexLoadBank(int bank_id, const char *name)
{
    char file[32];
    AssetFile af;
    TxpHeader hd;
    LBPTexBank *b;
    uint32_t *offs = NULL;
    uint8_t *stage = NULL;
    uint8_t *front = NULL;
    uint8_t head[32] __attribute__((aligned(32)));
    size_t stage_size = 0;
    uint32_t pos = 0;
    long want;
    int i, f, rc = -1;

    if (bank_id < 0 || bank_id >= LBPTEX_BANKS || name == NULL)
    {
        return -1;
    }
    b = &sLBPTexBanks[bank_id & (LBPTEX_BANKS - 1)];

    /* ef/efparticle.c caches a loaded bank by its ROM address for the
     * rest of the scene, so a second battle in the same scene asks for
     * the one it already has. This says yes the same way -- but it binds
     * the images again, because the *bank* may be a different copy: a
     * sudden death restarts the scene through
     * SYTaskmanSceneSetup.func_start, which empties the heap and makes
     * efParticleInitAll clear that cache, so the .txb is read again and
     * every LBTexture.data[] is at a new address. The texels in VRAM are
     * the same texels and are kept; what is stale is only the way back
     * from an address to one of them. */
    if (b->frames_num != 0 && strncmp(b->name, name, sizeof(b->name)) == 0)
    {
        return bank_bind_images(b, bank_id);
    }
    snprintf(file, sizeof(file), "%s.txp", name);

    /* Opened and vetted before the bank it replaces is given up: a load
     * that cannot happen leaves the one already there alone. */
    if (asset_open(&af, file) != 0)
    {
        return -1;
    }
    /* The first 32 bytes from offset zero into an aligned buffer, then
     * the rest of the header and both tables in one more read. That is
     * not tidiness: the first read is the one that puts the file on the
     * GD-ROM's DMA stream (iso_read starts one only for a read of at
     * least 32 bytes that begins on a sector boundary, and offset zero
     * is one), and every read after it stays on the stream because it
     * begins on a 32-byte boundary -- the exporter starts the blob on
     * one and each frame is a multiple of 128. A 16-byte header,
     * two tables and a seek would end the tables 8 mod 32 and send all
     * 202 of efcommon's frames sector by sector through the cache. */
    if (asset_read(&af, head, sizeof(head)) != (long)sizeof(head))
    {
        asset_close(&af);
        return -1;
    }
    memcpy(&hd, head, sizeof(hd));

    if (hd.magic != LBPTEX_MAGIC || hd.textures_num == 0 || hd.frames_num == 0 ||
        hd.texels_off < sizeof(hd) + hd.textures_num * sizeof(TxpTexture) +
                        hd.frames_num * sizeof(uint32_t) ||
        (hd.texels_off & 31) != 0)
    {
        asset_close(&af);
        return -1;
    }
    front = stage_alloc(hd.texels_off);

    if (front == NULL)
    {
        asset_close(&af);
        return -1;
    }
    memcpy(front, head, sizeof(head));
    want = (long)(hd.texels_off - sizeof(head));

    if (asset_read(&af, front + sizeof(head), want) != want)
    {
        free(front);
        asset_close(&af);
        return -1;
    }
    bank_free(b);
    b->textures_num = (int)hd.textures_num;
    b->frames_num = (int)hd.frames_num;
    b->texes = malloc(hd.textures_num * sizeof(TxpTexture));
    b->frames = calloc(hd.frames_num, sizeof(LBPTex));
    offs = malloc(hd.frames_num * sizeof(uint32_t));

    if (b->texes == NULL || b->frames == NULL || offs == NULL)
    {
        goto done;
    }
    memcpy(b->texes, front + sizeof(hd), hd.textures_num * sizeof(TxpTexture));
    memcpy(offs, front + sizeof(hd) + hd.textures_num * sizeof(TxpTexture),
           hd.frames_num * sizeof(uint32_t));
    free(front);
    front = NULL;
    /* The staging buffer is one frame, the largest: the pack is 481 KB
     * for efcommon and none of it needs to sit in main RAM once the PVR
     * has it. */
    for (i = 0; i < b->textures_num; i++)
    {
        size_t n = (size_t)b->texes[i].w * b->texes[i].h * 2;

        if (n > stage_size)
        {
            stage_size = n;
        }
    }
    stage = stage_alloc(stage_size);

    if (stage == NULL)
    {
        goto done;
    }
    /* Every frame below is uploaded into VRAM the renderer may still be
     * reading (dcpvr.h dc_pvr_vram_fence). bank_free above fenced too;
     * the wait is idempotent and this is where the uploads start. */
    dc_pvr_vram_fence();

    /* Read forward and never seek: the file pointer is at texels_off
     * already, and the exporter writes the frames in this order and pads
     * each to eight bytes, so what separates one from the next is at
     * most seven bytes and consuming them costs less than the seek
     * would -- asset_seek aborts the GD-ROM's stream, and this loader
     * wants the DMA path (src/dc/assetroot.h). */
    pos = 0;

    for (i = 0; i < b->textures_num; i++)
    {
        const TxpTexture *t = &b->texes[i];
        size_t n = (size_t)t->w * t->h * 2;

        for (f = 0; f < (int)t->frames; f++)
        {
            LBPTex *out = &b->frames[t->first_frame + f];
            uint32_t at = offs[t->first_frame + f];

            if (at < pos || (at - pos) > 8)
            {
                goto done;      /* not the file this loader was written for */
            }
            if (at != pos && asset_read(&af, stage, (long)(at - pos)) !=
                                 (long)(at - pos))
            {
                goto done;
            }
            if (asset_read(&af, stage, (long)n) != (long)n)
            {
                goto done;
            }
            pos = at + (uint32_t)n;
            out->w = t->w;
            out->h = t->h;
            out->src_w = t->src_w;
            out->src_h = t->src_h;
#ifdef _arch_dreamcast
            out->fmt = (t->fmt == TXP_ARGB1555) ? PVR_TXRFMT_ARGB1555
                                                : PVR_TXRFMT_ARGB4444;
            out->txr = pvr_mem_malloc(n);

            if (out->txr == NULL)
            {
                goto done;
            }
            pvr_txr_load(stage, out->txr, n);
#else
            /* The host build has no PVR. It reads the file and checks
             * its shape, which is what a host test can honestly do with
             * one; the texels are tools/check/particletex_check.py's. */
            out->fmt = t->fmt;
            out->txr = NULL;
#endif
            b->bytes += (uint32_t)n;
        }
    }
    if (bank_bind_images(b, bank_id) != 0)
    {
        goto done;
    }
    strncpy(b->name, name, sizeof(b->name) - 1);
    rc = 0;

done:
    free(front);
    free(offs);
    free(stage);
    asset_close(&af);

    if (rc != 0)
    {
        bank_free(b);
    }
    return rc;
}
