/* introcrowd.c -- see introcrowd.h. */
#include "introcrowd.h"

#ifdef FT_HOSTTEST

/* the host draws no pictures: the card builds its models */
sb32 introcrowd_load(const char *card, u32 key)
{
    (void)card;
    (void)key;

    return FALSE;
}

void introcrowd_proc_display(GObj *gobj)
{
    (void)gobj;
}

#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kos.h>
#include <dc/pvr.h>

#include "assetroot.h"
#include "dcpvr.h"
#include "lbcommon.h"           /* lbCommonSpriteNextDepth, gDCScreenScale */
#include "objpvr.h"             /* gcGetDrawList */
#include "taskman.h"            /* syTaskmanAddHeapResetHook */

#define IC_MAGIC "ICRW"
#define IC_VERSION 1
#define IC_TILES_MAX 12         /* ssb_introcrowd.py TILES_MAX */
#define IC_CODEBOOK 2048
#define IC_HEADER 20            /* magic, version, x0 y0 w h, tiles, variants */

typedef struct
{
    pvr_ptr_t txr;
    u16 dx, draw_w, tex_w, tex_h;
} ICTile;

static struct
{
    sb32 ready;
    u16 x0, y0, w, h;
    int ntiles;
    ICTile tiles[IC_TILES_MAX];
} sIC;

/* the polygon headers go to the TA by store queue, which wants them on a
 * 32-byte boundary (DB_HDR_ALIGN) */
static pvr_poly_hdr_t sICHdr[IC_TILES_MAX] __attribute__((aligned(32)));

static u16 rd16(const u8 *p)
{
    u16 v;

    memcpy(&v, p, sizeof(v));

    return v;
}

static u32 rd32(const u8 *p)
{
    u32 v;

    memcpy(&v, p, sizeof(v));

    return v;
}

/* The heap-reset hook: gives the textures back with the scene. */
static void introcrowd_release(void)
{
    int i;

    if (!sIC.ready && (sIC.ntiles == 0))
    {
        return;
    }
    dc_pvr_vram_fence();

    for (i = 0; i < sIC.ntiles; i++)
    {
        if (sIC.tiles[i].txr != NULL)
        {
            pvr_mem_free(sIC.tiles[i].txr);
            sIC.tiles[i].txr = NULL;
        }
    }
    sIC.ntiles = 0;
    sIC.ready = FALSE;
}

/* The bytes of a VQ texture the PVR reads: the codebook, then an index for
 * every 2x2 texels. */
static u32 introcrowd_tile_bytes(u32 tw, u32 th)
{
    return IC_CODEBOOK + (tw * th) / 4;
}

sb32 introcrowd_load(const char *card, u32 key)
{
    char name[32];
    long size;
    u8 *file;
    const u8 *table;
    u32 tiles, variants, i, v;
    long need;

    /* the pictures are 640x480 pixels; the staff roll's 1:1 scale is not it */
    if (gDCScreenScale != 2.0F)
    {
        return FALSE;
    }
    snprintf(name, sizeof(name), "introcrowd_%s.bin", card);
    file = asset_read_whole(name, &size);

    if (file == NULL)
    {
        return FALSE;
    }
    if ((size < IC_HEADER) || (memcmp(file, IC_MAGIC, 4) != 0) ||
        (rd32(file + 4) != IC_VERSION))
    {
        dbglog(DBG_WARNING, "introcrowd: %s is not an ICRW v%d file\n", name,
               IC_VERSION);
        free(file);

        return FALSE;
    }
    sIC.x0 = rd16(file + 8);
    sIC.y0 = rd16(file + 10);
    sIC.w = rd16(file + 12);
    sIC.h = rd16(file + 14);
    tiles = rd16(file + 16);
    variants = rd16(file + 18);
    need = IC_HEADER + (long)tiles * 8 + (long)variants * (4 + 8 * (long)tiles);

    if ((tiles < 1) || (tiles > IC_TILES_MAX) || (variants < 1) || (size < need))
    {
        dbglog(DBG_WARNING, "introcrowd: %s has %u tiles and %u variants in "
               "%ld bytes\n", name, (unsigned)tiles, (unsigned)variants, size);
        free(file);

        return FALSE;
    }
    table = file + IC_HEADER + tiles * 8;

    for (v = 0; v < variants; v++)
    {
        if (rd16(table) == key)
        {
            break;
        }
        table += 4 + 8 * tiles;
    }
    if (v == variants)
    {
        dbglog(DBG_WARNING, "introcrowd: %s has no variant %u\n", name,
               (unsigned)key);
        free(file);

        return FALSE;
    }
    table += 4;

    /* what the last card left goes first, then the renderer is done with
     * VRAM before anything is written to it (dcpvr.h) */
    introcrowd_release();
    dc_pvr_vram_fence();

    for (i = 0; i < tiles; i++)
    {
        const u8 *td = file + IC_HEADER + i * 8;
        ICTile *t = &sIC.tiles[i];
        u32 off = rd32(table + i * 8);
        u32 len = rd32(table + i * 8 + 4);
        pvr_poly_cxt_t cxt;

        t->dx = rd16(td);
        t->draw_w = rd16(td + 2);
        t->tex_w = rd16(td + 4);
        t->tex_h = rd16(td + 6);
        t->txr = NULL;

        if ((len != introcrowd_tile_bytes(t->tex_w, t->tex_h)) ||
            ((long)off + (long)len > size) || ((off & 31u) != 0))
        {
            dbglog(DBG_WARNING, "introcrowd: %s tile %u is not a %ux%u VQ "
                   "texture\n", name, (unsigned)i, (unsigned)t->tex_w,
                   (unsigned)t->tex_h);
            sIC.ntiles = (int)i;
            introcrowd_release();
            free(file);

            return FALSE;
        }
        t->txr = pvr_mem_malloc(len);

        if (t->txr == NULL)
        {
            dbglog(DBG_WARNING, "introcrowd: no VRAM for %s tile %u\n", name,
                   (unsigned)i);
            sIC.ntiles = (int)i;
            introcrowd_release();
            free(file);

            return FALSE;
        }
        pvr_txr_load(file + off, t->txr, len);

        /* the sprites' own header (lbcommon.c lbCommonSpriteSubmitHeader):
         * blended by alpha, over the 3D under it and under the sprite passes
         * after it; unfiltered, because the tiles are drawn one texel to a
         * pixel and bilinear would blur the seam between two of them */
        pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
                         PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_VQ_ENABLE,
                         t->tex_w, t->tex_h, t->txr, PVR_FILTER_NONE);
        cxt.gen.specular = PVR_SPECULAR_ENABLE;
        cxt.gen.culling = PVR_CULLING_NONE;
        cxt.txr.env = PVR_TXRENV_MODULATEALPHA;
        cxt.txr.uv_clamp = PVR_UVCLAMP_UV;
        cxt.blend.src = PVR_BLEND_SRCALPHA;
        cxt.blend.dst = PVR_BLEND_INVSRCALPHA;
        cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
        cxt.depth.write = PVR_DEPTHWRITE_DISABLE;
        pvr_poly_compile(&sICHdr[i], &cxt);
    }
    sIC.ntiles = (int)tiles;
    sIC.ready = TRUE;
    free(file);
    syTaskmanAddHeapResetHook(introcrowd_release);

    dbglog(DBG_INFO, "introcrowd: %s variant %u, %d tile(s), box %u,%u %ux%u\n",
           name, (unsigned)key, sIC.ntiles, (unsigned)sIC.x0, (unsigned)sIC.y0,
           (unsigned)sIC.w, (unsigned)sIC.h);

    return TRUE;
}

void introcrowd_proc_display(GObj *gobj)
{
    f32 z;
    int i;

    (void)gobj;

    /* the translucent pass only: a header for that list sent while the
     * opaque one is open spoils the frame (lbcommon.c on the same) */
    if (!sIC.ready || (gcGetDrawList() != PVR_LIST_TR_POLY))
    {
        return;
    }
    z = lbCommonSpriteNextDepth();

    for (i = 0; i < sIC.ntiles; i++)
    {
        const ICTile *t = &sIC.tiles[i];
        f32 x0 = (f32)(sIC.x0 + t->dx);
        f32 x1 = x0 + (f32)t->draw_w;
        f32 y0 = (f32)sIC.y0;
        f32 y1 = y0 + (f32)sIC.h;
        f32 u1 = (f32)t->draw_w / (f32)t->tex_w;
        f32 v1 = (f32)sIC.h / (f32)t->tex_h;
        pvr_vertex_t v;
        int k;

        pvr_prim(&sICHdr[i], sizeof(sICHdr[i]));

        v.argb = 0xFFFFFFFF;
        v.oargb = 0;
        v.z = z;

        for (k = 0; k < 4; k++)
        {
            v.flags = (k == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v.x = (k & 2) ? x1 : x0;
            v.y = (k & 1) ? y0 : y1;
            v.u = (k & 2) ? u1 : 0.0F;
            v.v = (k & 1) ? 0.0F : v1;
            pvr_prim(&v, sizeof(v));
        }
    }
}

#endif /* FT_HOSTTEST */
