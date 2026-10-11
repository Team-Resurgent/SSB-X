/* Fighter pack loading + bind pose + drawing. All per-model state lives in
 * the Fighter, so any number of loaded packs draw side by side; posing
 * them is the game's (ftanim, gcParseDObjAnimJoint).
 * See fighter.h for the pack layout and tools/export/ssb_packexport.py for the
 * writer.
 */
/* FT_HOSTTEST: the host cross-test links this file for its loader --
 * fighter_load, fighter_init and their relocation, fighter_release,
 * fighter_clone -- so the real packs go through the real loader into
 * the decomp's own engine there. The PVR halves, the pose
 * player and the draw are left out of that build; the guards say
 * which. */
#ifdef FT_HOSTTEST
#include "hoststubs.h"
#include <stdio.h>
#define dbglog(lvl, ...) fprintf(stderr, __VA_ARGS__)
#else
#include <kos.h>
#include "dcpvr.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>            /* memalign: hdr_alloc */
#include <string.h>

#include "assetroot.h"
#include "clip.h"
#include "fighter.h"
#include "ftshade.h"
#include "mtx.h"
#include "perf.h"
#include "taskman.h"

/* ---- matrices: mtx.h holds the ops and the backend switch ---- */

MTX_ACTIVE_STORAGE;

/* syMatrixTraRotRpyRScaF's transpose: scale the joint's own axes, rotate
 * roll/pitch/yaw, translate -- see mtx.h for the derivation. */
#ifndef SSB_NO_DRAW
static void joint_local(float *m, const float *t, const float *r,
                        const float *sc)
{
    float sr, cr, sp, cp, sy, cy;

    mtx_sincos(r[0], &sr, &cr);
    mtx_sincos(r[1], &sp, &cp);
    mtx_sincos(r[2], &sy, &cy);

    m[0] = cp * cy * sc[0];
    m[1] = (sr * sp * cy - cr * sy) * sc[1];
    m[2] = (cr * sp * cy + sr * sy) * sc[2];
    m[3] = t[0];
    m[4] = cp * sy * sc[0];
    m[5] = (sr * sp * sy + cr * cy) * sc[1];
    m[6] = (cr * sp * sy - sr * cy) * sc[2];
    m[7] = t[1];
    m[8] = -sp * sc[0];
    m[9] = sr * cp * sc[1];
    m[10] = cr * cp * sc[2];
    m[11] = t[2];
    m[12] = 0.0f;
    m[13] = 0.0f;
    m[14] = 0.0f;
    m[15] = 1.0f;
}
#endif /* !SSB_NO_DRAW */

static void build_mtx(Fighter *f);
static void fighter_pose_bind(Fighter *f);

/* ---- loading ---- */

/* The one FPACK_TEX_EXTERN texture, and what the PVR needs to know about
 * it. The game's equivalent is sLBTransitionPhotoHeap and the segment-1
 * binding lbTransitionProcDisplay does every frame (lbtransition.c:150);
 * here the header is compiled once at load, so the picture has to be
 * bound before the pack that reads it is. */
static pvr_ptr_t sExternTxr;
static uint32_t sExternFmt;
static int sExternW, sExternH;
static float sExternWin[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

void fighter_set_extern_texture(pvr_ptr_t txr, uint32_t txrfmt, int w, int h)
{
    sExternTxr = txr;
    sExternFmt = txrfmt;
    sExternW = w;
    sExternH = h;
    fighter_set_extern_window(0.0f, 0.0f, 1.0f, 1.0f);
}

void fighter_set_extern_window(float u0, float v0, float su, float sv)
{
    sExternWin[0] = u0;
    sExternWin[1] = v0;
    sExternWin[2] = su;
    sExternWin[3] = sv;
}

static const void *at(const Fighter *f, uint32_t off)
{
    return (const uint8_t *)f->blob + off;
}

/* The blobs relocated where they lie (fighter_load): a romdisk file is
 * already in RAM, so a pack that needs its offsets made pointers is
 * relocated in the romdisk image itself, once -- the second scene to
 * load it finds the pointers already made and must not make them
 * again. The image is static for the run, so a blob's address names it
 * for good; a blob read into a heap buffer is never listed here, since
 * a fresh read is a fresh copy of the file's offsets. */
#ifndef SSB_NO_DRAW
#define FIGHTER_INPLACE_MAX 32
static const void *sInPlaceBlobs[FIGHTER_INPLACE_MAX];
static int sInPlaceCount;

static int inplace_find(const void *blob)
{
    int i;

    for (i = 0; i < sInPlaceCount; i++)
    {
        if (sInPlaceBlobs[i] == blob)
        {
            return 1;
        }
    }
    return 0;
}

static int inplace_add(const void *blob)
{
    if (sInPlaceCount >= FIGHTER_INPLACE_MAX)
    {
        return -1;
    }
    sInPlaceBlobs[sInPlaceCount++] = blob;
    return 0;
}
#endif /* !SSB_NO_DRAW */

#if defined(DB_ANIM_CHECK) && !defined(SSB_NO_DRAW)
/* -DDB_ANIM_CHECK (src/dc/db.h): every pack's blob while it is loaded,
 * and the last ones released, so an animation script can be checked
 * against them before it is run. A script in no live blob is wrong --
 * and one inside a released blob is a stale pointer: the memory is free, or someone
 * else's, and what it reads as a script is whatever is there now --
 * which is how a stale script shows without this, as a parser spinning
 * at random a scene or a loop later. */
#define ANIMCHECK_LIVE 512
#define ANIMCHECK_DEAD 1024
static struct { const uint8_t *lo, *hi; char name[16]; } sAnimLive[ANIMCHECK_LIVE],
                                                         sAnimDead[ANIMCHECK_DEAD];
static int sAnimDeadNext;

void fighter_animcheck_add(const void *lo, uint32_t size, const char *name)
{
    int i;

    for (i = 0; i < ANIMCHECK_LIVE; i++)
    {
        if (sAnimLive[i].lo == NULL)
        {
            sAnimLive[i].lo = lo;
            sAnimLive[i].hi = (const uint8_t *)lo + size;
            strncpy(sAnimLive[i].name, name, sizeof(sAnimLive[i].name) - 1);
            sAnimLive[i].name[sizeof(sAnimLive[i].name) - 1] = '\0';
            return;
        }
    }
    dbglog(DBG_WARNING, "animcheck: more than %d packs loaded\n", ANIMCHECK_LIVE);
}

static void animcheck_live(const Fighter *f)
{
    char name[sizeof(f->hd->name) + 1];

    memcpy(name, f->hd->name, sizeof(f->hd->name));
    name[sizeof(f->hd->name)] = '\0';
    fighter_animcheck_add(f->blob, f->blob_size, name);
}

void fighter_animcheck_remove(const void *lo)
{
    int i;

    for (i = 0; i < ANIMCHECK_LIVE; i++)
    {
        if (sAnimLive[i].lo == lo)
        {
            sAnimDead[sAnimDeadNext] = sAnimLive[i];
            sAnimDeadNext = (sAnimDeadNext + 1) % ANIMCHECK_DEAD;
            sAnimLive[i].lo = NULL;
            return;
        }
    }
}

int fighter_animcheck(const void *script, const char *what)
{
    const uint8_t *p = script;
    int i;

    if (p == NULL)
        return 0;
    for (i = 0; i < ANIMCHECK_LIVE; i++)
    {
        if (sAnimLive[i].lo != NULL && p >= sAnimLive[i].lo && p < sAnimLive[i].hi)
            return 0;
    }
    {
        int k, hits = 0;

        /* newest first: every released pack that held this address, the
         * oldest being the one the pointer was taken from */
        for (k = 1; k <= ANIMCHECK_DEAD; k++)
        {
            i = (sAnimDeadNext - k + ANIMCHECK_DEAD) % ANIMCHECK_DEAD;
            if (sAnimDead[i].lo != NULL && p >= sAnimDead[i].lo &&
                p < sAnimDead[i].hi)
            {
                dbglog(DBG_ERROR, "animcheck: %s script %p is in released "
                       "pack %s (%p..%p, +0x%x), %d releases ago\n", what,
                       script, sAnimDead[i].name,
                       (const void *)sAnimDead[i].lo,
                       (const void *)sAnimDead[i].hi,
                       (unsigned)(p - sAnimDead[i].lo), k);
                hits++;
            }
        }
        if (hits == 0)
        {
            dbglog(DBG_ERROR, "animcheck: %s script %p is in no loaded pack "
                   "and no released one\n", what, script);
        }
        return -1;
    }
}
#endif

#ifndef SSB_NO_DRAW
/* One batch's pair of poly headers: the list its bucket names, and the
 * translucent list for fighter_draw_layered. `tex` is which of the
 * pack's textures it draws with -- the batch's own, or one frame of the
 * sprite array its MObj steps through. */
static void compile_batch_txr(Fighter *f, int i, int tex, pvr_ptr_t txr,
                              pvr_poly_hdr_t *own, pvr_poly_hdr_t *tr)
{
    static const pvr_list_t of_bucket[4] = {
        PVR_LIST_OP_POLY, PVR_LIST_PT_POLY, PVR_LIST_TR_POLY,
        PVR_LIST_TR_POLY        /* unused; the exporter emits 0-2 */
    };
    int list = of_bucket[f->batches[i].bucket & FPACK_LIST_MASK];
    int pass;

    /* the batch's own list, and the translucent list for
     * fighter_draw_layered */
    for (pass = 0; pass < 2; pass++)
    {
        pvr_poly_cxt_t cxt;
        int l = pass ? PVR_LIST_TR_POLY : list;

        if (tex >= 0)
        {
            const FPackTex *t = &f->texs[tex];
            uint32_t txrfmt;

            int tw = t->w, th = t->h;
            /* sys/rdp.c sSYRdpResetDisplayList -- the display list
             * every N64 frame begins with -- programs other-mode H
             * whole and leaves G_TF_BILERP in it, so the RDP filters
             * every model in the game unless a display list says
             * otherwise. None of the ones this port bakes does
             * (tools/lib/ssb_assets.py tracks the field and no pack's
             * bytes moved when it started), but the bit is carried so
             * that the day one does, it arrives. */
            int filter = (f->batches[i].bucket & FPACK_FILT_POINT)
                             ? PVR_FILTER_NEAREST
                             : PVR_FILTER_BILINEAR;

            switch (t->fmt)
            {
            case FPACK_TEX_ARGB1555:
                txrfmt = PVR_TXRFMT_ARGB1555;
                break;
            case FPACK_TEX_ARGB4444:
                txrfmt = PVR_TXRFMT_ARGB4444;
                break;
            case FPACK_TEX_EXTERN:
                /* The UVs run 0..1 over the picture, and the picture
                 * is the whole texture, so the size the PVR needs is
                 * the texture's own. */
                txrfmt = sExternFmt;
                tw = sExternW;
                th = sExternH;
                break;
            default:
                txrfmt = PVR_TXRFMT_PAL4BPP |
                         PVR_TXRFMT_4BPP_PAL(f->pal_base + t->pal);
                break;
            }
            pvr_poly_cxt_txr(&cxt, l, txrfmt,
                             tw, th, (txr != NULL) ? txr : f->txr[tex],
                             filter);
            cxt.txr.uv_clamp = (pvr_uv_clamp_t)t->clamp;
            /* MODULATEALPHA multiplies the texel's alpha by the
             * vertex's; MODULATE takes the vertex's alone. Which the
             * RDP did is what FPACK_ALPHA_TEX records -- KOS picks
             * MODULATEALPHA for every non-opaque list, which is right
             * only where the alpha cycle actually reads the texel. */
            cxt.txr.env = (f->batches[i].alpha_src & FPACK_ALPHA_TEX)
                              ? PVR_TXRENV_MODULATEALPHA
                              : PVR_TXRENV_MODULATE;
        }
        else
        {
            pvr_poly_cxt_col(&cxt, l);
        }
        /* tools/lib/ssb_assets.py's DL replay bakes the N64's G_CULL_BACK/
         * G_CULL_FRONT geometry-mode bits into the batch's bucket (default
         * cull_back=1: sys/rdp.c's sSYRdpResetDisplayList sets G_CULL_BACK
         * every frame, so most batches get real culling without their own
         * display list ever mentioning it). PVR_CULLING_CW matches
         * G_CULL_BACK for this port's winding -- verified against Mario's
         * 320 baked triangles. Both bits set (nothing in the game's data does
         * this) falls back to NONE rather than trying to express "draw
         * nothing" through the cull mode. */
        {
            uint16_t cull = f->batches[i].bucket &
                            (FPACK_CULL_BACK | FPACK_CULL_FRONT);
            cxt.gen.culling = (cull == FPACK_CULL_BACK) ? PVR_CULLING_CW
                             : (cull == FPACK_CULL_FRONT) ? PVR_CULLING_CCW
                             : PVR_CULLING_NONE;
        }
        /* lerp(ENV, PRIM, TEXEL0) is texel * (PRIM - ENV) + ENV, and the
         * PVR's offset colour is the `+ ENV`: enabling it is what lets
         * material_of hand the vertex a base colour and an offset rather
         * than one flat colour. src/dc/lbcommon.c does the same for a
         * sprite drawn with the same combiner. Every batch has it, not
         * only FPACK_ENVLERP ones: the fog (Fighter.fog_live) is an
         * offset too, and can fall on any batch a fighter draws; the
         * others carry an offset of zero. */
        cxt.gen.specular = PVR_SPECULAR_ENABLE;
        /* fighter.h FPACK_ZALWAYS */
        if (f->batches[i].bucket & FPACK_ZALWAYS)
        {
            cxt.depth.comparison = PVR_DEPTHCMP_ALWAYS;
            cxt.depth.write = PVR_DEPTHWRITE_DISABLE;
        }
#ifdef DB_HDR_ALIGN
        /* -DDB_HDR_ALIGN: KOS pvr_poly_compile opens with movca.l, which
         * allocates the header's cache line WITHOUT reading memory, so
         * every byte of that line outside the header is lost -- on a
         * console. */
        if ((((uintptr_t)(pass ? tr : own)) & 31u) != 0)
        {
            static int seen;

            if (seen++ < 6)
                dbglog(DBG_ERROR, "hdralign: fighter header %p is %u mod 32\n",
                       (void *)(pass ? tr : own),
                       (unsigned)(((uintptr_t)(pass ? tr : own)) & 31u));
        }
#endif
        pvr_poly_compile(pass ? tr : own, &cxt);
    }
}

/* A polygon header lives on a 32-byte line, and KOS's pvr_poly_compile
 * knows it: it opens with movca.l (dcache_alloc_line_with_value), which claims
 * the header's cache line WITHOUT reading it from memory. Every byte of that
 * line outside the header is then lost when the line is written back. From
 * malloc's 8-byte alignment that line also holds the chunk's own header and
 * the tail of the block before it, so the first header compiled into a
 * malloc'd array zeroed dlmalloc's bookkeeping -- on a console (-DDB_HDR_ALIGN
 * counts the misaligned destinations). This was the hardware-only
 * "malloc_consolidate" crash. Every heap array of pvr_poly_hdr_t comes from
 * here, and free() takes it as it takes any other block. */
static pvr_poly_hdr_t *hdr_alloc(size_t n)
{
    return memalign(32, n * sizeof(pvr_poly_hdr_t));
}

static void compile_batch(Fighter *f, int i, int tex,
                          pvr_poly_hdr_t *own, pvr_poly_hdr_t *tr)
{
    compile_batch_txr(f, i, tex, NULL, own, tr);
}

/* See Fighter.hdr_inv. Every FPACK_ENVLERP batch that draws one of the
 * pack's own ARGB textures through f->hdr gets a second pair of headers
 * over that texture with its colour inverted (the alpha is kept: the
 * combiner's alpha cycle reads the texel's as it was). Whether a draw
 * uses it is material_of's call, frame by frame, since an MObj may move
 * either colour. Yoshi's Island's two background clouds are why: PRIM
 * orange and light blue over ENV white, which the plain form drew white. */
static int make_inverted(Fighter *f)
{
    const FPackHeader *hd = f->hd;
    int i;

    if (f->costumes != NULL || hd->batch_count == 0)
        return 0;
    for (i = 0; i < (int)hd->batch_count; i++)
    {
        const FPackBatch *b = &f->batches[i];
        const FPackTex *t;

        if (!(b->bucket & FPACK_ENVLERP) || b->tex < 0)
            continue;
        if (f->hdr_frames != NULL && f->texpart_batches[i].part != 0xFF)
            continue;
        if (f->hdr_mobj != NULL && f->hdr_mobj[i] != NULL)
            continue;
        t = &f->texs[b->tex];
        if (t->fmt != FPACK_TEX_ARGB1555 && t->fmt != FPACK_TEX_ARGB4444)
            continue;

        if (f->hdr_inv == NULL)
        {
            f->hdr_inv = hdr_alloc(2 * hd->batch_count);
            f->has_inv = calloc(hd->batch_count, 1);
            f->txr_inv = calloc(hd->tex_count, sizeof(pvr_ptr_t));
            if (!f->hdr_inv || !f->has_inv || !f->txr_inv)
                return -1;
        }
        if (f->txr_inv[b->tex] == NULL)
        {
            uint16_t mask = (t->fmt == FPACK_TEX_ARGB1555) ? 0x7FFF : 0x0FFF;
            uint16_t *tmp = malloc(t->size);
            uint32_t k;

            /* fighter_init has fenced already; this is its own
             * allocation all the same (dcpvr.h dc_pvr_vram_fence) */
            dc_pvr_vram_fence();
            f->txr_inv[b->tex] = pvr_mem_malloc(t->size);
            if (!tmp || !f->txr_inv[b->tex])
            {
                free(tmp);
                dbglog(DBG_ERROR, "fighter: no room for an inverted "
                                  "texture (%u bytes)\n",
                       (unsigned)t->size);
                return -1;
            }
            memcpy(tmp, (const uint8_t *)at(f, hd->off_texdata) + t->off,
                   t->size);
            for (k = 0; k < t->size / 2; k++)
                tmp[k] ^= mask;
            pvr_txr_load(tmp, f->txr_inv[b->tex], t->size);
            free(tmp);
        }
        compile_batch_txr(f, i, b->tex, f->txr_inv[b->tex],
                          &f->hdr_inv[2 * i], &f->hdr_inv[2 * i + 1]);
        f->has_inv[i] = 1;
    }
    return 0;
}
#endif /* !SSB_NO_DRAW */

/* The one relocation (fighter.h), over the words at `base` -- the pack's
 * own, or its .anm's (fighter_attach_anm) -- which are `size` bytes. A
 * tier's prefix of the .anm (`partial`) holds some animations and not
 * the rest, which are left for the read that brings them. */
static int anims_relocate(Fighter *f, uint8_t *base, uint32_t size,
                          int partial)
{
    const FPackHeader *hd = f->hd;
    int i;

    /* An AnimJoint script names the
     * target of a Jump, SetAnim or SetInterp by address, and the
     * exporter stored those words as word indices. Make them addresses
     * now, once, as lbRelocGetForceExternHeapFile's reloc walk does for
     * the game (lb/lbreloc.c) -- gcParseDObjAnimJoint reads them back
     * through AObjEvent32.p, unmodified. */
    for (i = 0; i < (int)hd->anim_count; i++)
    {
        const FPackAnim *a = &f->anims[i];
        uint32_t *w = (uint32_t *)(base + a->off_words);
        const uint32_t *rl = (const uint32_t *)at(f, a->off_reloc);
        uint32_t k;

        if (a->off_words + fighter_anim_bytes(a) > size)
        {
            if (partial)
            {
                continue;
            }
            dbglog(DBG_ERROR, "fighter: %s: %s's %u words at %u run past "
                              "their %u bytes\n", hd->name, a->name,
                   (unsigned)a->nwords, (unsigned)a->off_words,
                   (unsigned)size);
            return -1;
        }
        if (a->kind == FPACK_ANIM_FIGATREE)
        {
            /* A figatree's only pointers are in the spline tables a
             * SetTranslateInterp names (Samus's rolls): 32-bit, two u16
             * words each, low word first, holding the target's word index
             * (tools/lib/ssb_meshexport.py native_figatree_splines). ftanim.c
             * points AObj.interpolate at the table in place, so the words
             * are the struct. A 64-bit host's SYInterpDesc is wider than
             * the table; it leaves them as they are and plays no spline. */
            uint16_t *w16 = (uint16_t *)(base + a->off_words);

            for (k = 0; k < a->nreloc; k++)
            {
                uint32_t idx = (rl[k] + 1 < a->nwords) ?
                    (uint32_t)w16[rl[k]] | ((uint32_t)w16[rl[k] + 1] << 16) :
                    a->nwords;

                if (idx >= a->nwords)
                {
                    dbglog(DBG_ERROR, "fighter: %s: %s spline reloc %u -> %u "
                                      "is outside its %u words\n",
                           hd->name, a->name, (unsigned)rl[k], (unsigned)idx,
                           (unsigned)a->nwords);
                    return -1;
                }
#ifndef SSB_NO_DRAW
                {
                    uint32_t addr = (uint32_t)(uintptr_t)(w16 + idx);

                    w16[rl[k]] = (uint16_t)addr;
                    w16[rl[k] + 1] = (uint16_t)(addr >> 16);
                }
#endif
            }
            continue;
        }
        for (k = 0; k < a->nreloc; k++)
        {
            if (rl[k] >= a->nwords || w[rl[k]] >= a->nwords)
            {
                dbglog(DBG_ERROR, "fighter: %s: %s reloc %u -> %u is "
                                  "outside its %u words\n",
                       hd->name, a->name, (unsigned)rl[k],
                       (unsigned)(rl[k] < a->nwords ? w[rl[k]] : 0),
                       (unsigned)a->nwords);
                return -1;
            }
            w[rl[k]] = (uint32_t)(uintptr_t)(w + w[rl[k]]);
        }
    }
    return 0;
}

int fighter_attach_anm(Fighter *f, void *anm, uint32_t size, int owned)
{
    const FPackAttr *pa = f->attr;
    const FPackAnm *ah = (const FPackAnm *)anm;
    int partial;
    int t;

    if (pa == NULL || pa->anm_size == 0)
    {
        dbglog(DBG_ERROR, "fighter: %s: its animations are in the pack, not "
                          "an .anm\n", f->hd->name);
        return -1;
    }
    for (t = 0; t < FPACK_ANM_TIERS; t++)
    {
        if (size == sizeof(FPackAnm) + pa->anm_tier_end[t])
        {
            break;
        }
    }
    partial = (size != sizeof(FPackAnm) + pa->anm_size);
    if (size < sizeof(FPackAnm) ||
        memcmp(ah->magic, FPACK_ANM_MAGIC, sizeof(ah->magic)) != 0 ||
        ah->id != pa->anm_id || ah->size != pa->anm_size ||
        (partial && t == FPACK_ANM_TIERS))
    {
        dbglog(DBG_ERROR, "fighter: %s: the .anm (%u bytes, id %08x) is not "
                          "this pack's (%u bytes, id %08x)\n", f->hd->name,
               (unsigned)size,
               (unsigned)(size >= sizeof(FPackAnm) ? ah->id : 0),
               (unsigned)(pa->anm_size + sizeof(FPackAnm)),
               (unsigned)pa->anm_id);
        return -1;
    }
    if (anims_relocate(f, (uint8_t *)anm, size, partial) < 0)
    {
        return -1;
    }
    f->anm = anm;
    f->anm_size = size;
    f->anm_owned = owned;
#if defined(DB_ANIM_CHECK) && !defined(SSB_NO_DRAW)
    {
        char name[sizeof(f->hd->name) + 1];

        memcpy(name, f->hd->name, sizeof(f->hd->name));
        name[sizeof(f->hd->name)] = '\0';
        fighter_animcheck_add(anm, size, name);
    }
#endif
    return 0;
}

/* The bytes of f's .anm that tier `tier` is, header and all. */
static uint32_t fighter_anm_tier_size(const Fighter *f, int tier)
{
    return (uint32_t)sizeof(FPackAnm) +
           ((tier >= 0 && tier < FPACK_ANM_TIERS) ? f->attr->anm_tier_end[tier]
                                                  : f->attr->anm_size);
}

/* The first `size` bytes of f's .anm, in a buffer the caller frees. */
static void *fighter_anm_read(Fighter *f, uint32_t size)
{
    AssetFile af;
    void *buf;
    long got;

    if (size == sizeof(FPackAnm) + f->attr->anm_size)
    {
        long whole;

        buf = asset_read_whole(f->anm_name, &whole);
        if (buf != NULL && (uint32_t)whole != size)
        {
            free(buf);
            buf = NULL;
        }
    }
    else if (asset_open(&af, f->anm_name) < 0)
    {
        buf = NULL;
    }
    else
    {
        buf = asset_alloc((long)size);
        got = (buf != NULL) ? asset_read(&af, buf, (long)size) : -1;
        asset_close(&af);
        if (got != (long)size)
        {
            free(buf);
            buf = NULL;
        }
    }
    if (buf == NULL)
    {
        dbglog(DBG_ERROR, "fighter: cannot read %u bytes of %s\n",
               (unsigned)size, f->anm_name);
    }
    return buf;
}

/* fighter_load's second file: <stem>.anm beside `name`, for a pack whose
 * animations are not in it (fighter.h FPackAnm) -- the first `tier` of
 * it (FPackAttr.anm_tier_end), or all of it. */
static int fighter_load_anm(Fighter *f, const char *name, int tier)
{
    const char *dot = strrchr(name, '.');
    size_t stem = (dot != NULL) ? (size_t)(dot - name) : strlen(name);
    uint32_t size;
    void *anm;

    if (f->attr == NULL || f->attr->anm_size == 0)
    {
        return 0;
    }
    if (stem + sizeof(".anm") > sizeof(f->anm_name))
    {
        dbglog(DBG_ERROR, "fighter: %s: name too long for its .anm\n", name);
        return -1;
    }
    memcpy(f->anm_name, name, stem);
    memcpy(f->anm_name + stem, ".anm", sizeof(".anm"));
    size = fighter_anm_tier_size(f, tier);
    anm = fighter_anm_read(f, size);
    if (anm == NULL)
    {
        return -1;
    }
    if (fighter_attach_anm(f, anm, size, 1) < 0)
    {
        free(anm);
        return -1;
    }
    return 0;
}

int fighter_anm_grow(Fighter *f, int tier, int retire)
{
    uint32_t size;
    void *old = f->anm;
    int old_owned = f->anm_owned;
    void *anm;

    if (f->attr == NULL || f->attr->anm_size == 0 || f->anm_src != NULL)
    {
        return 0;
    }
    size = fighter_anm_tier_size(f, tier);
    if (f->anm != NULL && f->anm_size >= size)
    {
        return 0;
    }
    /* the one prefix a pack keeps for its clones: a second miss would
     * need two, and cannot happen -- the first one reads the whole file */
    if (retire && f->anm_retired != NULL)
    {
        dbglog(DBG_ERROR, "fighter: %s: a second prefix to retire\n",
               f->anm_name);
        return -1;
    }
    if (!retire && old != NULL && old_owned)
    {
        /* nothing plays from it: give it back before the bigger read */
#if defined(DB_ANIM_CHECK) && !defined(SSB_NO_DRAW)
        fighter_animcheck_remove(old);
#endif
        free(old);
        old = NULL;
        f->anm = NULL;
        f->anm_size = 0;
    }
    anm = fighter_anm_read(f, size);
    if (anm == NULL || fighter_attach_anm(f, anm, size, 1) < 0)
    {
        /* with the prefix given back first, the pack plays nothing
         * now, and each bind after says so */
        free(anm);
        return -1;
    }
    if (old != NULL && old_owned)
    {
        f->anm_retired = old;
    }
    return 0;
}

const void *fighter_anim_words_need(Fighter *f, const FPackAnim *a)
{
    const void *words = fighter_anim_words(f, a);
    Fighter *o = (f->anm_src != NULL) ? f->anm_src : f;

    if (words != NULL || o->attr == NULL || o->attr->anm_size == 0 ||
        o->anm == NULL)
    {
        return words;
    }
    dbglog(DBG_WARNING, "fighter: anim %d (%s) of %s is not resident; "
                        "reading the rest of %s\n",
           (int)(a - o->anims), a->name, o->hd->name, o->anm_name);
    if (fighter_anm_grow(o, FIGHTER_ANM_FULL, 1) < 0)
    {
        return NULL;
    }
    return fighter_anim_words(f, a);
}

int fighter_init(Fighter *f, void *blob, uint32_t size, int *pal_bank)
{
    return fighter_init_ex(f, blob, size, pal_bank, 0);
}

int fighter_init_ex(Fighter *f, void *blob, uint32_t size, int *pal_bank,
                    int relocated)
{
    const FPackHeader *hd;
    int i, k;

    memset(f, 0, sizeof(*f));
    f->blob = blob;
    f->blob_size = size;

    hd = f->hd = (const FPackHeader *)f->blob;
    if (memcmp(hd->magic, FPACK_MAGIC, 8) != 0)
    {
        /* SAY WHICH of the three it is. A bare "not a pack" hid
         * two unrelated failures (tools/check/stgtex_check.py's header tells
         * one: a pack that is fine but whose textures overflow the array), so
         * the two looked like one bug. The magic is printed as bytes because a
         * wrong one is the interesting case: it says whether the blob is
         * another file, a short read, or nothing at all. */
        dbglog(DBG_ERROR, "fighter: not a pack: magic %02x%02x%02x%02x"
                          "%02x%02x%02x%02x at %p, %u bytes\n",
               ((const unsigned char *)hd->magic)[0],
               ((const unsigned char *)hd->magic)[1],
               ((const unsigned char *)hd->magic)[2],
               ((const unsigned char *)hd->magic)[3],
               ((const unsigned char *)hd->magic)[4],
               ((const unsigned char *)hd->magic)[5],
               ((const unsigned char *)hd->magic)[6],
               ((const unsigned char *)hd->magic)[7],
               f->blob, (unsigned)size);
        return -1;
    }
    if (hd->joint_count > FIGHTER_MAX_JOINTS ||
        hd->tex_count > sizeof(f->txr) / sizeof(f->txr[0]))
    {
        dbglog(DBG_ERROR, "fighter: %.8s: %u joints (max %u) and %u textures "
                          "(max %u) -- this build is too small for the "
                          "pack\n",
               hd->name, (unsigned)hd->joint_count,
               (unsigned)FIGHTER_MAX_JOINTS, (unsigned)hd->tex_count,
               (unsigned)(sizeof(f->txr) / sizeof(f->txr[0])));
        return -1;
    }
    f->joints = (const FPackJoint *)at(f, hd->off_joints);
    f->verts = (const FPackVtx *)at(f, hd->off_verts);
    f->tris = (const uint16_t (*)[3])at(f, hd->off_tris);
    f->batches = (const FPackBatch *)at(f, hd->off_batches);
    f->texs = (const FPackTex *)at(f, hd->off_texs);
    f->pals = (const uint16_t (*)[16])at(f, hd->off_pals);
    f->anims = (const FPackAnim *)at(f, hd->off_anims);
    f->attr = hd->off_attr ? (const FPackAttr *)at(f, hd->off_attr) : NULL;
    f->motions = hd->off_motion ? (const FPackMotion *)at(f, hd->off_motion)
                                : NULL;

    /* Whether any batch is sphere-mapped, asked once here so that
     * fighter_frame's per-joint loop -- which runs for every model on
     * screen, every frame -- does not pay two dot products a joint for
     * the eleven fighters and every stage that never ask. */
    f->has_texgen = 0;
    {
        uint32_t i;

        for (i = 0; i < hd->batch_count; i++)
        {
            if (f->batches[i].bucket & FPACK_TEXGEN)
            {
                f->has_texgen = 1;
                break;
            }
        }
    }

    /* The joint tables a fighter pack carries (fighter.h FPackAttr): the
     * hidden-part rows must fit the array they are declared in, and each
     * animation's raw slot table must name words the animation has --
     * the tree walk that deals the slots out reads them unchecked, as
     * the game reads its file. */
    f->slots = NULL;
    f->shield = NULL;
    if (f->attr != NULL)
    {
        const FPackAttr *pa = f->attr;

        if (pa->hiddenpart_count < 0 ||
            pa->hiddenpart_count > FPACK_HIDDENPART_MAX ||
            pa->off_slots == 0 ||
            pa->off_slots + hd->anim_count * sizeof(FPackAnimSlots) > size)
        {
            dbglog(DBG_ERROR, "fighter: %s: joint tables missing or "
                              "malformed (%d hidden parts, slots at %u)\n",
                   hd->name, (int)pa->hiddenpart_count,
                   (unsigned)pa->off_slots);
            return -1;
        }
        f->slots = (const FPackAnimSlots *)at(f, pa->off_slots);
        /* A masked-out entry is never built (objmodel.c), so its baked
         * geometry -- a hidden-part root's, drawn only while the part is
         * linked -- starts hidden for a draw that walks no tree. A walk
         * resets the mask itself. */
        for (i = 0; i < (int)hd->joint_count && i < 64; i++)
        {
            int on = (i < 32) ? (pa->setup_parts[0] >> (31 - i)) & 1
                              : (pa->setup_parts[1] >> (63 - i)) & 1;

            if (!on)
            {
                f->joint_hide |= (uint64_t)1 << i;
            }
        }
        for (i = 0; i < (int)hd->anim_count; i++)
        {
            const FPackAnim *a = &f->anims[i];
            const FPackAnimSlots *s = &f->slots[i];
            const int32_t *tab;
            uint32_t k;

            if (s->off + s->count * sizeof(int32_t) > size ||
                s->count > FIGHTER_MAX_JOINTS)
            {
                dbglog(DBG_ERROR, "fighter: %s: %s slot table %u+%u is "
                                  "outside the pack\n",
                       hd->name, a->name, (unsigned)s->off,
                       (unsigned)s->count);
                return -1;
            }
            tab = (const int32_t *)at(f, s->off);
            for (k = 0; k < s->count; k++)
            {
                if (tab[k] != -1 && (uint32_t)tab[k] >= a->nwords)
                {
                    dbglog(DBG_ERROR, "fighter: %s: %s slot %u -> word %d "
                                      "of %u\n",
                           hd->name, a->name, (unsigned)k, (int)tab[k],
                           (unsigned)a->nwords);
                    return -1;
                }
            }
        }
        /* The shield pose (fighter.h FPackShieldPose): the rows and the
         * eight tables must lie in the file's words and every table
         * entry name a word; the game reads its file unchecked, and the
         * guard indexes both by the count of DObjs it links, which is
         * what table_count was sized to (tools/lib/ssb_meshexport.py). The
         * tables are made pointers here, once per pack, the way an
         * AnimJoint animation's reloc words are above, and the rows
         * copied into the game's own struct -- one allocation each,
         * shared by every clone and freed with the pack. */
        if (pa->off_shield == 0 ||
            pa->off_shield + sizeof(FPackShieldPose) > size)
        {
            dbglog(DBG_ERROR, "fighter: %s: shield pose at %u is outside "
                              "the pack\n", hd->name, (unsigned)pa->off_shield);
            return -1;
        }
        f->shield = (const FPackShieldPose *)at(f, pa->off_shield);
        {
            const FPackShieldPose *sp = f->shield;
            uint32_t *w = (uint32_t *)((uint8_t *)f->blob + sp->off_words);
            const uint32_t *rl = (const uint32_t *)at(f, sp->off_reloc);
            uint32_t k, j;

            /* An empty pose is a pack whose fighter cannot be made to
             * guard. Master Hand is the one: BossMain
             * leaves dobj_lookup and all eight shield_anim_joints NULL,
             * his extern chain names no ShieldPose file, and the
             * exporter writes zeros rather than inventing rows. Every
             * count below is 0 and nothing indexes them, because
             * ftCommonGuardInitJoints is reached only through a guard
             * status he has no row for. Checked as all-zero and not as
             * "lookup_count == 0", so a pack that lost only part of its
             * pose still falls through to the malformed arm. */
            if (sp->nwords == 0 && sp->nreloc == 0 &&
                sp->lookup_count == 0 && sp->table_count == 0)
            {
                goto shield_done;
            }
            if (sp->off_words + sp->nwords * 4 > size ||
                sp->off_reloc + sp->nreloc * 4 > size ||
                sp->lookup + sp->lookup_count * 11 > sp->nwords ||
                sp->lookup_count != sp->table_count + 1 ||
                sp->table_count > FIGHTER_MAX_JOINTS)
            {
                dbglog(DBG_ERROR, "fighter: %s: shield pose %u words, %u rows, "
                                  "tables of %u: malformed\n",
                       hd->name, (unsigned)sp->nwords,
                       (unsigned)sp->lookup_count, (unsigned)sp->table_count);
                return -1;
            }
            for (k = 0; k < 8; k++)
            {
                if (sp->table[k] + sp->table_count > sp->nwords)
                {
                    dbglog(DBG_ERROR, "fighter: %s: shield table %u at word "
                                      "%u is outside its %u words\n",
                           hd->name, (unsigned)k, (unsigned)sp->table[k],
                           (unsigned)sp->nwords);
                    return -1;
                }
                for (j = 0; j < sp->table_count; j++)
                {
                    if (w[sp->table[k] + j] >= sp->nwords)
                    {
                        dbglog(DBG_ERROR, "fighter: %s: shield table %u "
                                          "entry %u -> word %u of %u\n",
                               hd->name, (unsigned)k, (unsigned)j,
                               (unsigned)w[sp->table[k] + j],
                               (unsigned)sp->nwords);
                        return -1;
                    }
                }
            }
            for (k = 0; k < sp->nreloc && !relocated; k++)
            {
                if (rl[k] >= sp->nwords || w[rl[k]] >= sp->nwords)
                {
                    dbglog(DBG_ERROR, "fighter: %s: shield reloc %u -> %u is "
                                      "outside its %u words\n",
                           hd->name, (unsigned)rl[k],
                           (unsigned)(rl[k] < sp->nwords ? w[rl[k]] : 0),
                           (unsigned)sp->nwords);
                    return -1;
                }
                w[rl[k]] = (uint32_t)(uintptr_t)(w + w[rl[k]]);
            }
            f->shield_joints[0] = calloc(8 * sp->table_count,
                                         sizeof(void *));
            f->shield_lookup = calloc(sp->lookup_count,
                                      sizeof(FPackDObjDesc));
            if (f->shield_joints[0] == NULL || f->shield_lookup == NULL)
            {
                dbglog(DBG_ERROR, "fighter: %s: no room for the shield "
                                  "pose\n", hd->name);
                return -1;
            }
            for (k = 0; k < 8; k++)
            {
                f->shield_joints[k] = f->shield_joints[0] + k * sp->table_count;
                for (j = 0; j < sp->table_count; j++)
                {
                    uint32_t idx = w[sp->table[k] + j];

                    f->shield_joints[k][j] = idx ? (void *)(w + idx) : NULL;
                }
            }
            for (k = 0; k < sp->lookup_count; k++)
            {
                const uint32_t *row = w + sp->lookup + 11 * k;
                FPackDObjDesc *d = &f->shield_lookup[k];

                d->id = (int32_t)row[0];
                d->dl = NULL;
                memcpy(d->translate, row + 2, sizeof(d->translate));
                memcpy(d->rotate, row + 5, sizeof(d->rotate));
                memcpy(d->scale, row + 8, sizeof(d->scale));
            }
        shield_done: ;
        }
    }
    /* The model parts (fighter.h FPackParts): every run must lie in the
     * pack, every tag name a joint and a vertex run, and every joint's
     * ids lie in the id table -- the setters in src/dc/ftparam.c index
     * them unchecked. The tree's own part is on until a walk says
     * otherwise. */
    f->parts = NULL;
    if (f->attr != NULL && f->attr->off_parts != 0)
    {
        const FPackParts *pp;
        uint32_t k;

        if (f->attr->off_parts + sizeof(FPackParts) > size)
        {
            dbglog(DBG_ERROR, "fighter: %s: parts at %u are outside the "
                              "pack\n", hd->name, (unsigned)f->attr->off_parts);
            return -1;
        }
        pp = f->parts = (const FPackParts *)at(f, f->attr->off_parts);
        if (pp->tag_count >= FPACK_PART_NONE ||
            pp->accessory_tag > pp->tag_count ||
            pp->off_tags + pp->tag_count * sizeof(FPackPartTag) > size ||
            pp->off_joints + hd->joint_count * sizeof(FPackPartJoint) > size ||
            pp->off_ids + pp->id_count * sizeof(FPackPartID) > size)
        {
            dbglog(DBG_ERROR, "fighter: %s: part tables malformed\n",
                   hd->name);
            return -1;
        }
        f->part_tags = (const FPackPartTag *)at(f, pp->off_tags);
        f->part_joints = (const FPackPartJoint *)at(f, pp->off_joints);
        f->part_ids = (const FPackPartID *)at(f, pp->off_ids);
        for (k = 0; k < pp->tag_count; k++)
        {
            const FPackPartTag *pt = &f->part_tags[k];

            if (pt->joint >= hd->joint_count ||
                pt->vert_first + pt->vert_count > hd->vert_count)
            {
                dbglog(DBG_ERROR, "fighter: %s: part %u malformed\n",
                       hd->name, (unsigned)k + 1);
                return -1;
            }
        }
        for (k = 0; k < hd->joint_count; k++)
        {
            const FPackPartJoint *pj = &f->part_joints[k];

            if (pj->id_first + pj->id_count > pp->id_count ||
                (pj->tree != FPACK_PART_NONE && pj->tree > pp->tag_count))
            {
                dbglog(DBG_ERROR, "fighter: %s: joint %u's parts "
                                  "malformed\n", hd->name, (unsigned)k);
                return -1;
            }
            f->part_cur[k] = pj->tree;
        }
        for (k = 0; k < pp->id_count; k++)
        {
            if (f->part_ids[k].tag != FPACK_PART_NONE &&
                (f->part_ids[k].tag == 0 ||
                 f->part_ids[k].tag > pp->tag_count))
            {
                dbglog(DBG_ERROR, "fighter: %s: part id %u -> tag %u\n",
                       hd->name, (unsigned)k, (unsigned)f->part_ids[k].tag);
                return -1;
            }
        }
    }
    /* The costumes (fighter.h FPackCostumes): every row's texture must
     * be one the pack has. */
    f->costumes = NULL;
    f->costume_mats = NULL;
    f->costume = 0;
    if (f->attr != NULL && f->attr->off_costumes != 0)
    {
        const FPackCostumes *pc;
        uint32_t k, n;

        if (f->attr->off_costumes + sizeof(FPackCostumes) > size)
        {
            dbglog(DBG_ERROR, "fighter: %s: costumes at %u are outside the "
                              "pack\n", hd->name,
                   (unsigned)f->attr->off_costumes);
            return -1;
        }
        pc = (const FPackCostumes *)at(f, f->attr->off_costumes);
        n = pc->costume_count * hd->batch_count;
        if (pc->costume_count == 0 ||
            pc->off_mats + n * sizeof(FPackCostumeMat) > size)
        {
            dbglog(DBG_ERROR, "fighter: %s: costume table malformed\n",
                   hd->name);
            return -1;
        }
        f->costumes = pc;
        f->costume_mats = (const FPackCostumeMat *)at(f, pc->off_mats);
        for (k = 0; k < n; k++)
        {
            if (f->costume_mats[k].tex >= (int)hd->tex_count)
            {
                dbglog(DBG_ERROR, "fighter: %s: costume row %u -> texture "
                                  "%d\n", hd->name, (unsigned)k,
                       (int)f->costume_mats[k].tex);
                return -1;
            }
        }
    }
    /* The texture parts (fighter.h FPackTexParts): each batch's run must
     * lie in the frame table and each entry name a texture. */
    f->texparts = NULL;
    f->texpart_batches = NULL;
    f->texpart_frames = NULL;
    f->hdr_frames = NULL;
    f->texpart_frame[0] = f->texpart_frame[1] = 0;
    if (f->attr != NULL && f->attr->off_texparts != 0)
    {
        const FPackTexParts *tp;
        int nc = (f->costumes != NULL) ? (int)f->costumes->costume_count : 1;
        uint32_t k;

        if (f->attr->off_texparts + sizeof(FPackTexParts) > size)
        {
            dbglog(DBG_ERROR, "fighter: %s: texture parts at %u are outside "
                              "the pack\n", hd->name,
                   (unsigned)f->attr->off_texparts);
            return -1;
        }
        tp = (const FPackTexParts *)at(f, f->attr->off_texparts);
        if (tp->off_batches + hd->batch_count * sizeof(FPackTexPartBatch) > size ||
            tp->off_frames + tp->frame_total * sizeof(int16_t) > size)
        {
            dbglog(DBG_ERROR, "fighter: %s: texture part tables malformed\n",
                   hd->name);
            return -1;
        }
        f->texparts = tp;
        f->texpart_batches = (const FPackTexPartBatch *)at(f, tp->off_batches);
        f->texpart_frames = (const int16_t *)at(f, tp->off_frames);
        for (k = 0; k < hd->batch_count; k++)
        {
            const FPackTexPartBatch *tb = &f->texpart_batches[k];

            if (tb->part == 0xFF)
            {
                continue;
            }
            if (tb->part > 1 || tp->frame_count[tb->part] == 0 ||
                tb->frame_first + (uint32_t)nc * tp->frame_count[tb->part] >
                    tp->frame_total)
            {
                dbglog(DBG_ERROR, "fighter: %s: batch %u's texture part "
                                  "frames malformed\n", hd->name, (unsigned)k);
                return -1;
            }
        }
        for (k = 0; k < tp->frame_total; k++)
        {
            if (f->texpart_frames[k] >= (int)hd->tex_count)
            {
                dbglog(DBG_ERROR, "fighter: %s: texture part frame %u -> "
                                  "texture %d\n", hd->name, (unsigned)k,
                       (int)f->texpart_frames[k]);
                return -1;
            }
        }
    }
    f->motion_count = hd->motion_count;

    /* The demo statuses' motion table, which rides in the
     * attribute block rather than the header -- see fighter.h
     * FPackAttr.off_submotion. Only a fighter pack has one. */
    if (f->attr != NULL && f->attr->off_submotion != 0 &&
        f->attr->submotion_count != 0)
    {
        f->submotions = (const FPackMotion *)at(f, f->attr->off_submotion);
        f->submotion_count = f->attr->submotion_count;
    }

    /* The animated-material section, present only on an emblem pack
     * (fighter.h FPackMObjs). Its MatAnimJoint words carry the same one
     * relocation an animation's do, and are made addresses here for the
     * same reason: gcParseMObjMatAnimJoint reads them back through
     * AObjEvent32.p, unmodified. */
    f->mobjs = hd->off_mobjs ? (const FPackMObjs *)at(f, hd->off_mobjs)
                             : NULL;
    if (f->mobjs != NULL)
    {
        const FPackMObjs *mo = f->mobjs;
        uint32_t *w = (uint32_t *)((uint8_t *)f->blob + mo->off_words);
        const uint32_t *rl = (const uint32_t *)at(f, mo->off_reloc);
        uint32_t k;

        /* dc_model_add_mobjs reads alt_count * mobj_count entries: a
         * table that is short runs off the pack into the heap, and what
         * it finds there becomes a script pointer */
        if ((uint64_t)mo->off_entry +
                (uint64_t)mo->alt_count * mo->mobj_count * 4 > size)
        {
            dbglog(DBG_ERROR, "fighter: %s: %u material entries run past "
                              "its %u bytes\n", hd->name,
                   (unsigned)(mo->alt_count * mo->mobj_count),
                   (unsigned)size);
            return -1;
        }
        for (k = 0; k < mo->alt_count * mo->mobj_count; k++)
        {
            int32_t e = ((const int32_t *)at(f, mo->off_entry))[k];

            if (e < -1 || (e >= 0 && (uint32_t)e >= mo->words))
            {
                dbglog(DBG_ERROR, "fighter: %s: material entry %u is %ld, "
                                  "outside its %u words\n", hd->name,
                       (unsigned)k, (long)e, (unsigned)mo->words);
                return -1;
            }
        }
        f->mobj_subs = (const FPackMObjSub *)at(f, mo->off_subs);
        f->mobj_joint = (const int16_t *)at(f, mo->off_joint);
        f->mobj_batch = (const int16_t *)at(f, mo->off_batch);
        f->dpals = mo->dpal_count
                 ? (const FPackDPal *)at(f, mo->off_dpal) : NULL;
        for (k = 0; k < mo->reloc_count && !relocated; k++)
        {
            if (rl[k] >= mo->words || w[rl[k]] >= mo->words)
            {
                dbglog(DBG_ERROR, "fighter: %s: material reloc %u -> %u is "
                                  "outside its %u words\n",
                       hd->name, (unsigned)rl[k],
                       (unsigned)(rl[k] < mo->words ? w[rl[k]] : 0),
                       (unsigned)mo->words);
                return -1;
            }
            w[rl[k]] = (uint32_t)(uintptr_t)(w + w[rl[k]]);
        }
        f->mobj_color = calloc(mo->mobj_count, sizeof(*f->mobj_color));
        if (f->mobj_color == NULL)
        {
            dbglog(DBG_ERROR, "fighter: %s: no room for %u materials\n",
                   hd->name, (unsigned)mo->mobj_count);
            return -1;
        }
    }

    /* The animations' script pointers, when they are in the pack; an
     * .anm's are made when it is attached. */
    if (!relocated && !(f->attr != NULL && f->attr->anm_size != 0) &&
        anims_relocate(f, (uint8_t *)f->blob, size, 0) < 0)
    {
        return -1;
    }

    /* The motion scripts' pointers (fighter.h FPackHeader.off_script):
     * the exporter left each one as an offset into the file, so make it
     * an address into the file as loaded, once, the way the game's
     * lbRelocLoadAndRelocFile does for the file's intern chain. */
    if (hd->off_script != 0 && !relocated)
    {
        uint32_t *w = (uint32_t *)((uint8_t *)f->blob + hd->off_script);
        const uint32_t *rl = (const uint32_t *)at(f, hd->off_script_reloc);

        for (k = 0; k < (int)hd->script_reloc_count; k++)
        {
            if (rl[k] >= hd->script_words ||
                w[rl[k]] >= hd->script_words * 4)
            {
                dbglog(DBG_ERROR, "fighter: %s: script reloc %u -> %u is "
                                  "outside its %u words\n",
                       hd->name, (unsigned)rl[k],
                       (unsigned)(rl[k] < hd->script_words ? w[rl[k]] : 0),
                       (unsigned)hd->script_words);
                return -1;
            }
            w[rl[k]] = (uint32_t)(uintptr_t)((uint8_t *)w + w[rl[k]]);
        }
    }

    /* Palettes into this pack's own PVR banks. The PVR has 1024 palette
     * entries, which is 64 banks of 16, and every pack resident at once
     * shares them; past 64 the writes wrap and silently repaint an
     * earlier pack's textures in someone else's colours. That is a
     * glitch with no error behind it, so it is an error here instead.
     *
     * 64 banks is the ceiling on how much CI4 content can be resident:
     * a VS stage costs 6 to 17 banks, a fighter 2 to 3. What lifts it is
     * assigning banks on bind rather than on load, since only the bound
     * stage's are ever read. */
#ifndef SSB_NO_DRAW
    /* The renderer may still be reading what this is about to overwrite
     * (dcpvr.h dc_pvr_vram_fence). */
    dc_pvr_vram_fence();

    if (*pal_bank + (int)hd->pal_count > FPACK_PAL_BANKS)
    {
        dbglog(DBG_ERROR, "%s: %u palette banks past bank %d; the PVR has "
               "%d\n", hd->name, (unsigned)hd->pal_count, *pal_bank,
               FPACK_PAL_BANKS);
        return -1;
    }
    f->pal_base = *pal_bank;
    *pal_bank += (int)hd->pal_count;
    pvr_set_pal_format(PVR_PAL_ARGB1555);
    for (i = 0; i < (int)hd->pal_count; i++)
        for (k = 0; k < 16; k++)
            pvr_set_pal_entry((uint32_t)((f->pal_base + i) * 16 + k),
                              f->pals[i][k]);

    for (i = 0; i < (int)hd->tex_count; i++)
    {
        const FPackTex *t = &f->texs[i];

        /* The pack has no texels for an extern texture; the runtime made
         * one and said so. A pack that wants one before anybody supplied
         * it is a build-order bug, so it fails here rather than drawing
         * whatever the PVR happens to hold. */
        if (t->fmt == FPACK_TEX_EXTERN)
        {
            if (sExternTxr == NULL)
            {
                dbglog(DBG_ERROR, "%s: texture %d is extern and none is "
                                  "bound\n", hd->name, i);
                return -1;
            }
            f->txr[i] = sExternTxr;
            memcpy(f->ext_win, sExternWin, sizeof(f->ext_win));
            continue;
        }
        f->txr[i] = pvr_mem_malloc(t->size);
        if (!f->txr[i])
        {
            dbglog(DBG_ERROR, "fighter: pvr_mem_malloc(%u) failed\n",
                   (unsigned)t->size);
            return -1;
        }
        pvr_txr_load((const uint8_t *)at(f, hd->off_texdata) + t->off,
                     f->txr[i], t->size);
    }

    /* A pack with no batches is a pack that carries only textures --
     * romdisk/ftshadow.mdl is one, the blob the shadow renderer draws by
     * hand. malloc(0) is allowed to return NULL, and that
     * would fail a load with nothing wrong with it. */
    {
        int nc = (f->costumes != NULL) ? (int)f->costumes->costume_count : 1;
        int c;

        if (hd->batch_count != 0)
        {
            f->hdr = hdr_alloc(nc * hd->batch_count);
            f->hdr_tr = hdr_alloc(nc * hd->batch_count);
            if (!f->hdr || !f->hdr_tr)
                return -1;
        }
        /* a header per costume per batch: the texture is in the header */
        for (c = 0; c < nc; c++)
        {
            for (i = 0; i < (int)hd->batch_count; i++)
            {
                int k = c * (int)hd->batch_count + i;
                int tex = (f->costumes != NULL) ? f->costume_mats[k].tex
                                                : f->batches[i].tex;

                compile_batch(f, i, tex, &f->hdr[k], &f->hdr_tr[k]);
            }
        }
        /* and a pair per texture part frame, compiled for the batch that
         * owns the entry (fighter.h FPackTexParts) */
        if (f->texparts != NULL && f->texparts->frame_total != 0)
        {
            f->hdr_frames = hdr_alloc(2 * f->texparts->frame_total);
            if (!f->hdr_frames)
                return -1;
            for (i = 0; i < (int)hd->batch_count; i++)
            {
                const FPackTexPartBatch *tb = &f->texpart_batches[i];
                int e, n;

                if (tb->part == 0xFF)
                    continue;
                n = nc * f->texparts->frame_count[tb->part];
                for (e = tb->frame_first; e < tb->frame_first + n; e++)
                {
                    int tex = f->texpart_frames[e];

                    if (tex < 0)
                        tex = f->batches[i].tex;
                    compile_batch(f, i, tex, &f->hdr_frames[2 * e],
                                  &f->hdr_frames[2 * e + 1]);
                }
            }
        }
    }
    /* A batch whose MObj carries a sprite array gets that pair again for
     * every frame of it. The picture is the one thing objanim.c's material
     * parser can change that a header compiled here cannot follow -- the
     * texture's address is in the header -- so every frame is compiled now
     * and draw_batches picks. Nothing but the damage slash has one, and a
     * pack with no MObjs does not reach this at all. */
    if (f->mobjs != NULL)
    {
        f->hdr_mobj = calloc(hd->batch_count, sizeof(pvr_poly_hdr_t *));
        if (!f->hdr_mobj)
            return -1;
        for (i = 0; i < (int)hd->batch_count; i++)
        {
            int m = f->mobj_batch[i];
            int n = (m < 0) ? 0 : f->mobj_subs[m].tex_count;
            int k, first;

            if (n < 2)
                continue;
            f->hdr_mobj[i] = hdr_alloc(2 * n);
            if (!f->hdr_mobj[i])
                return -1;
            /* a batch that steps its own run (fighter.h FPACK_OWNRUN)
             * starts it at its own texture, not the MObj's */
            first = (f->batches[i].bucket & FPACK_OWNRUN)
                            ? f->batches[i].tex
                            : f->mobj_subs[m].tex_first;

            for (k = 0; k < n; k++)
                compile_batch(f, i, first + k,
                              &f->hdr_mobj[i][k * 2],
                              &f->hdr_mobj[i][k * 2 + 1]);
        }
    }
    if (make_inverted(f) < 0)
        return -1;

#else
    (void)pal_bank;         /* no PVR: no palettes, textures or headers */
#endif
    /* Bind pose first, so a pack with no animations (a stage) renders. */
    for (i = 0; i < (int)hd->joint_count; i++)
    {
        const FPackJoint *j = &f->joints[i];
        f->pose[i][0] = j->t[0];
        f->pose[i][1] = j->t[1];
        f->pose[i][2] = j->t[2];
        f->pose[i][3] = j->r[0];
        f->pose[i][4] = j->r[1];
        f->pose[i][5] = j->r[2];
        f->pose[i][6] = j->s[0];
        f->pose[i][7] = j->s[1];
        f->pose[i][8] = j->s[2];
    }
    /* A pack with no vertices is legitimate: the
     * screen quake is one joint and four AnimJoint scripts, with nothing
     * to draw (tools/export/ssb_effectexport.py). malloc(0) is allowed to
     * return NULL, so ask it nothing. */
    f->vclip = hd->vert_count
             ? malloc((size_t)hd->vert_count * sizeof(*f->vclip)) : NULL;
    if (hd->vert_count && !f->vclip)
    {
        dbglog(DBG_ERROR, "fighter: no room for %u clip verts\n",
               (unsigned)hd->vert_count);
        return -1;
    }

#ifndef SSB_NO_DRAW
    /* The bind pose: a pack loaded here is a template nothing poses
     * (fighter.h). */
    fighter_pose_bind(f);
    build_mtx(f);
#endif
    dbglog(DBG_INFO, "fighter: %s: %u joints, %u tris, %u anims, %u bytes\n",
           hd->name, (unsigned)hd->joint_count, (unsigned)hd->tri_count,
           (unsigned)hd->anim_count, (unsigned)f->blob_size);
#if defined(DB_ANIM_CHECK) && !defined(SSB_NO_DRAW)
    animcheck_live(f);
#endif
    return 0;
}

/* The pack's bytes, without a copy where there need not be one -- the
 * same trade as src/dc/stage.c's stage_bytes, and for the same reason:
 * fighter_init points into the blob and keeps it, and on the romdisk the
 * blob is already in RAM as part of the ELF. Mario's pack is 448 KB, so
 * the copy is 448 KB of a 16 MB machine spent to hold what is already
 * held. Falls back to the copy where the mapping cannot be used. */
/* Whether fighter_init will write into the blob.
 *
 * It does for exactly four things, all of them the same one relocation:
 * an AnimJoint animation's pointer words, a figatree's spline-table
 * pointers, a motion script's, and an emblem's MatAnimJoint words. Each turns a word index into an address,
 * which is a one-way change -- so a pack that has any of them cannot be
 * loaded straight out of a mapping, because the mapping is the romdisk
 * itself and the second load would relocate what the first already did.
 *
 * A pack loaded once and kept (every fighter, every stage) never noticed.
 * The wipes were the first per-scene model and would have: a wipe rolled
 * twice in one run comes up unrelocated the second time. The emblem is
 * the first that is *always* the same file, so it fails every lap, which
 * is how this was found. */
#ifndef SSB_NO_DRAW
static int pack_relocates(const FPackHeader *hd, const void *blob)
{
    const FPackAnim *a = (const FPackAnim *)((const uint8_t *)blob +
                                             hd->off_anims);
    uint32_t i;

    if (hd->off_script != 0 || hd->off_mobjs != 0)
    {
        return 1;
    }
    for (i = 0; i < hd->anim_count; i++)
    {
        if (a[i].kind == FPACK_ANIM_ANIMJOINT || a[i].nreloc != 0)
        {
            return 1;
        }
    }
    return 0;
}

/* Once fighter_init_ex has put the textures in VRAM nothing reads the
 * pack's copy of them again (make_inverted's was the last read), so a
 * pack whose texels are its last section (tools/export/ssb_packexport.py)
 * gives them back by cutting them off its own buffer: 1.9 MB of the
 * Characters scene's twelve packs. realloc shrinks in place, so every
 * pointer into the pack stays good. */
static void fighter_drop_texels(Fighter *f)
{
    const FPackHeader *hd = f->hd;
    uint32_t keep = hd->off_texdata;
    uint32_t end = keep + hd->texdata_size;
    void *p;

    if (hd->texdata_size == 0 || keep < sizeof(FPackHeader) ||
        end > f->blob_size || f->blob_size - end >= 4)
    {
        return;
    }
#ifdef DB_ANIM_CHECK
    fighter_animcheck_remove(f->blob);
#endif
    p = realloc(f->blob, keep);
    if (p != f->blob)
    {
        /* newlib never moves a block it shrinks; if it did, every
         * pointer relocated into the pack would name freed memory */
        dbglog(DBG_CRITICAL, "fighter: %s: realloc moved the pack\n",
               hd->name);
        arch_abort();
    }
    f->blob_size = keep;
#ifdef DB_ANIM_CHECK
    animcheck_live(f);
#endif
}
#endif /* !SSB_NO_DRAW */

#ifdef FT_HOSTTEST
/* The host's: the file into a buffer of its own, relocated there --
 * which is the disc branch below, and the game's own order of things. */
int fighter_load_tier(Fighter *f, const char *name, int *pal_bank, int tier)
{
    long size;
    void *blob;
    int rc;

    blob = asset_read_whole(name, &size);
    if (blob == NULL)
    {
        dbglog(DBG_ERROR, "fighter: cannot read %s\n", name);
        return -1;
    }
    rc = fighter_init_ex(f, blob, (uint32_t)size, pal_bank, 0);
    f->blob_owned = 1;
    if (rc == 0 && fighter_load_anm(f, name, tier) < 0)
    {
        fighter_release(f);
        return -1;
    }
    if (rc == 0)
    {
        /* The motion scripts are detached on the host. The decomp's
         * event structs carry void * fields (ft/fttypes.h p_goto,
         * p_subroutine), 4 bytes wide on the N64 and in the packs' u32
         * word streams and 8 on a 64-bit host, so the engine built for
         * this host would read a script's pointer words as half of
         * something else. Without them a status runs its animation and
         * its physics and none of its hitboxes, sounds or effects, which
         * is enough for a fighter to stand, jump and land through his
         * own motion table; a 32-bit host build (gcc -m32) would lift
         * this. The shipped table is still in the file for a test that
         * wants to see it.
         *
         * BOTH tables. The SubMotion rows carry scripts
         * too now, and a demo status installs one by the same arm of
         * ftMainSetStatus -- so leaving them attached crashed the host
         * in ftMainUpdateMotionEventsAll the moment a test posed Mario,
         * on the first Subroutine word the pack holds. */
        FPackMotion *m = (FPackMotion *)f->motions;
        FPackMotion *sm = (FPackMotion *)f->submotions;
        int i;

        for (i = 0; i < (int)f->motion_count; i++)
        {
            m[i].script = 0;
        }
        for (i = 0; sm != NULL && i < (int)f->submotion_count; i++)
        {
            sm[i].script = 0;
        }
    }
    return rc;
}
#else
int fighter_load_tier(Fighter *f, const char *name, int *pal_bank, int tier)
{
    /* A name, not a path: the medium, the alignment and the counters are
     * the asset root's business (src/dc/assetroot.h). */
    AssetFile af;
    long size;
    void *blob;
    int owned = 0;
    int relocated = 0;
    int rc;

    if (asset_open(&af, name) < 0)
    {
        dbglog(DBG_ERROR, "fighter: cannot open %s\n", name);
        return -1;
    }
    size = asset_size(&af);
    /* only the romdisk answers this: the pack relocated where it lies
     * (NULL on a disc, and for a file out of models.bnd) */
    blob = asset_mmap(&af);

    if (blob != NULL && ((uintptr_t)blob & 3) == 0 &&
        size >= (long)sizeof(FPackHeader) &&
        memcmp(blob, FPACK_MAGIC, 8) == 0 && pack_relocates(blob, blob))
    {
        /* The file is in RAM already (the romdisk is linked into the
         * ELF, in a writable section -- src/game/ssb64/Makefile's romdisk.o
         * rule), so the load writes its pointers into the image where it lies
         * rather than into a private copy, and remembers the blob so the next
         * scene to load this pack skips the relocation instead of relocating
         * pointers. Copying a pack with scripts into the heap would cost too
         * much with twelve packs loaded per scene; a pack read off a disc
         * takes the branch below and is relocated in its own buffer, which is
         * the game's own order of things -- lbRelocLoadAndRelocFile DMAs the
         * file into the heap and walks its chain there. */
        relocated = inplace_find(blob);
        if (!relocated && inplace_add(blob) < 0)
        {
            dbglog(DBG_ERROR, "fighter: %s: more than %d packs relocated in "
                              "place\n", name, FIGHTER_INPLACE_MAX);
            asset_close(&af);
            return -1;
        }
    }
    else if (blob == NULL || ((uintptr_t)blob & 3) != 0)
    {
        /* the loader's copy when it read the pack ahead (assetroot.h) */
        owned = 1;
        blob = asset_read_all(&af);
        if (!blob)
        {
            dbglog(DBG_ERROR, "fighter: short read on %s (%ld bytes)\n",
                   name, size);
            asset_close(&af);
            return -1;
        }
    }
    asset_close(&af);
    rc = fighter_init_ex(f, blob, (uint32_t)size, pal_bank, relocated);
    f->blob_owned = owned;
    if (rc == 0 && owned)
    {
        fighter_drop_texels(f);
    }
    if (rc == 0 && fighter_load_anm(f, name, tier) < 0)
    {
        fighter_release(f);
        return -1;
    }
    return rc;
}
#endif /* FT_HOSTTEST */

const DCMObjColor *fighter_batch_mobj(const Fighter *f, int batch)
{
    int m;

    if (f->mobjs == NULL || batch < 0 || batch >= (int)f->hd->batch_count)
    {
        return NULL;
    }
    m = f->mobj_batch[batch];

    return (m < 0) ? NULL : &f->mobj_color[m];
}

DCMObjColor *fighter_mobj_color(Fighter *f, int joint, int index)
{
    int first, count;

    if (f->mobjs == NULL || joint < 0 || joint >= (int)f->hd->joint_count)
    {
        return NULL;
    }
    first = f->mobj_joint[joint * 2];
    count = f->mobj_joint[joint * 2 + 1];

    return (index < 0 || index >= count) ? NULL : &f->mobj_color[first + index];
}

int fighter_load(Fighter *f, const char *name, int *pal_bank)
{
    return fighter_load_tier(f, name, pal_bank, FIGHTER_ANM_FULL);
}

void fighter_release(Fighter *f)
{
    int i;

    if (f->hd == NULL)
    {
        return;
    }
    /* src/dc/db.h's DB_HEAP_WALK: both hardware crashes died in the
     * free()s below, on a header something else had written; check the
     * whole heap before this pack's blocks are handed back */
    ASSET_HEAP_WALK("fighter_release");
#ifndef SSB_NO_DRAW
    /* The renderer may still be reading what this is about to overwrite
     * (dcpvr.h dc_pvr_vram_fence). */
    dc_pvr_vram_fence();

    for (i = 0; i < (int)f->hd->tex_count; i++)
    {
        if (f->txr[i] != NULL && f->texs[i].fmt != FPACK_TEX_EXTERN)
        {
            pvr_mem_free(f->txr[i]);
        }
        if (f->txr_inv != NULL && f->txr_inv[i] != NULL)
        {
            pvr_mem_free(f->txr_inv[i]);
        }
    }
    free(f->hdr_inv);
    free(f->has_inv);
    free(f->txr_inv);
    f->hdr_inv = NULL;
    f->has_inv = NULL;
    f->txr_inv = NULL;
#else
    (void)i;
#endif
    free(f->hdr);
    free(f->hdr_tr);
    free(f->hdr_frames);
    if (f->hdr_mobj != NULL)
    {
        for (i = 0; i < (int)f->hd->batch_count; i++)
        {
            free(f->hdr_mobj[i]);
        }
        free(f->hdr_mobj);
    }
    free(f->mobj_color);
    free(f->shield_joints[0]);
    free(f->shield_lookup);
    free(f->vclip);
#if defined(DB_ANIM_CHECK) && !defined(SSB_NO_DRAW)
    fighter_animcheck_remove(f->blob);
    if (f->anm != NULL)
    {
        fighter_animcheck_remove(f->anm);
    }
    if (f->anm_retired != NULL)
    {
        fighter_animcheck_remove(f->anm_retired);
    }
#endif
    if (f->anm_owned)
    {
        free(f->anm);
        free(f->anm_retired);
    }
    if (f->blob_owned)
    {
        free(f->blob);
    }
    memset(f, 0, sizeof(*f));
}

/* fighter_load_scene's registry: the packs to give back at the next scene
 * change. 48 is more than any one scene loads. */
#define FIGHTER_SCENE_PACKS 48
static Fighter *sFighterScenePacks[FIGHTER_SCENE_PACKS];
static int sFighterScenePackCount;

static void fighter_scene_release_all(void)
{
    int i;

    for (i = 0; i < sFighterScenePackCount; i++)
    {
        fighter_release(sFighterScenePacks[i]);
        sFighterScenePacks[i] = NULL;
    }
    sFighterScenePackCount = 0;
}

int fighter_load_scene(Fighter *f, const char *name, int *pal_bank)
{
    int r = fighter_load(f, name, pal_bank);

    if (r == 0 && sFighterScenePackCount < FIGHTER_SCENE_PACKS)
    {
        sFighterScenePacks[sFighterScenePackCount++] = f;
        syTaskmanAddHeapResetHook(fighter_scene_release_all);
    }
    return r;
}

void fighter_clone(Fighter *dst, const Fighter *src, float (*vclip)[4])
{
    *dst = *src;
    dst->vclip = vclip;
    /* the pack's .anm, which is the pack's to free and to grow */
    dst->anm_src = (src->anm_src != NULL) ? src->anm_src : (Fighter *)src;
    dst->anm_owned = 0;
    dst->anm_retired = NULL;
    /* A clone is its own model: dc_model_add_dobjs cuts it payloads that
     * name it, not the ones a tree off `src` may already hang from. */
    dst->disp = NULL;
    dst->disp_epoch = 0;
#ifndef SSB_NO_DRAW
    fighter_pose_bind(dst);
    build_mtx(dst);
#endif
}

#ifndef SSB_NO_DRAW
/* ---- bind pose ---- */

/* Every joint at its bind transform: what a loaded pack and a fresh clone
 * get, and all the game ever needs -- posing a model is ftanim's job
 * through its FTParts, or gcParseDObjAnimJoint's, not this file's. */
static void fighter_pose_bind(Fighter *f)
{
    int i;

    for (i = 0; i < (int)f->hd->joint_count; i++)
    {
        const FPackJoint *j = &f->joints[i];
        f->pose[i][0] = j->t[0];
        f->pose[i][1] = j->t[1];
        f->pose[i][2] = j->t[2];
        f->pose[i][3] = j->r[0];
        f->pose[i][4] = j->r[1];
        f->pose[i][5] = j->r[2];
        f->pose[i][6] = j->s[0];
        f->pose[i][7] = j->s[1];
        f->pose[i][8] = j->s[2];
    }
}

static void build_mtx(Fighter *f)
{
    int i;

    for (i = 0; i < (int)f->hd->joint_count; i++)
    {
        float local[16];

        joint_local(local, &f->pose[i][0], &f->pose[i][3], &f->pose[i][6]);
        if (f->joints[i].parent < 0)
            memcpy(f->mtx[i], local, sizeof(local));
        else
            mtx_mul(f->mtx[i], f->mtx[f->joints[i].parent], local);
    }
}

/* ---- per-frame camera fold + lighting (per instance) ---- */

static void fighter_frame_impl(Fighter *f, const float *mv, const float *proj,
                               float lx, float ly, float lz);

void fighter_frame(Fighter *f, const float *mv, const float *proj,
                   float lx, float ly, float lz)
{
    DBPERF_BEGIN(DBP_FRAME);
    fighter_frame_impl(f, mv, proj, lx, ly, lz);
    DBPERF_END(DBP_FRAME);
}

static void fighter_frame_impl(Fighter *f, const float *mv, const float *proj,
                               float lx, float ly, float lz)
{
    int i, c, t;
    int n, cur;
    DBPERF_LAP_BEGIN();

    for (i = 0; i < (int)f->hd->joint_count; i++)
    {
        const float *m = f->mtx[i];
        /* Columns of M dotted with L give (M^T L); normalising each column
         * strips the scale so the light does not brighten with a limb. */
        for (c = 0; c < 3; c++)
        {
            float x = m[c], y = m[4 + c], z = m[8 + c];
            float lensq = mtx_dot3(x, y, z, x, y, z);
            float inv = (lensq > 1e-12f) ? mtx_rsqrt(lensq) : 0.0f;
            f->light[i][c] = mtx_dot3(x, y, z, lx, ly, lz) * inv;
            /* G_TEXTURE_GEN's two lookat vectors through the same
             * (M^T v) fold, so a sphere-mapped batch spends two dot
             * products a vertex and no matrix work. mv is row-major and
             * maps world to eye, so its first two rows ARE the camera's
             * right and up in world space -- which is what
             * gmCameraLookAtFuncMatrix sends as gSPLookAtX/Y
             * (syMatrixLookAtReflectF builds them off the same eye/at/up
             * the view matrix is built from). */
            if (f->has_texgen)
            {
                f->texgen[i][0][c] =
                    mtx_dot3(x, y, z, mv[0], mv[1], mv[2]) * inv;
                f->texgen[i][1][c] =
                    mtx_dot3(x, y, z, mv[4], mv[5], mv[6]) * inv;
            }
        }
    }

    DBPERF_LAP(DBP_FRAMEJ);
    /* The exporter emits vertices grouped by owning joint, so this rebuilds
     * the chain once per group (14 times for Mario, not 25) and spends one
     * transform per vertex. The compare keeps it correct, and merely slower,
     * if a pack ever arrives ungrouped. */
    /* A model part's vertices are one run (fighter.h FPackPartTag), and a
     * part the joint does not have on draws nothing, so its run is
     * stepped over: Kirby's fifteen copy hats are 1700 vertices of which
     * at most one hat's are ever drawn. */
    n = (int)f->hd->vert_count;
    cur = -1;
    i = 0;
    for (t = 0; t <= (int)(f->parts ? f->parts->tag_count : 0); t++)
    {
        int end = n, next = n;

        if (f->parts != NULL && t < (int)f->parts->tag_count)
        {
            const FPackPartTag *pt = &f->part_tags[t];

            if (fighter_part_on(f, t + 1, pt->joint))
                continue;
            end = pt->vert_first;
            next = pt->vert_first + pt->vert_count;
        }
        for (; i < end; i++)
        {
            const FPackVtx *v = &f->verts[i];
#ifdef DB_PREFETCH
            /* OFF BY DEFAULT: a soak build with this on crashed at the first
             * results screen on real hardware three runs in three (three
             * different faults), and none of fifteen screens
             * without it did. Cause not found. It bought ~0.3 ms a frame.
             * Every vertex is read once and its clip vector written once,
             * both cold: the 36-byte vertices and the 16-byte outputs are
             * ~86 KB a frame over four fighters, several times the 16 KB
             * operand cache. PREF is a hint and never faults, so running
             * past the array's end is harmless. */
            __builtin_prefetch(&f->verts[i + 6], 0);
            __builtin_prefetch(&f->vclip[i + 8], 1);
#endif
            if ((int)v->joint != cur)
            {
                cur = v->joint;
                mtx_load3(proj, mv, f->mtx[cur]);
            }
            mtx_xform(v->x, v->y, v->z, f->vclip[i]);
        }
        i = next;
    }
    DBPERF_LAP(DBP_FRAMEV);
    gDBPerfCalls_framev_add(n);
}

/* ---- draw (clip + shade + emit; the clip is src/dc/clip.c) ---- */

typedef struct
{
    float base[3], amb[3], dif[3];
    FtShadeFx fx;               /* the same three, as ft_shade_lit_fx wants them */
    int shaded;
    int alpha_shade;            /* the RDP's alpha cycle reads the vertex */
    uint32_t alpha;
    uint32_t offset;            /* the PVR's offset colour, 0x00RRGGBB:
                                 * the `+ ENV` of an FPACK_ENVLERP batch
                                 * and 0 for every other one */
    int inv;                    /* draw through Fighter.hdr_inv */
} Material;

/* `mc` is the live MObj behind the batch, or NULL where the batch has
 * none -- which is every batch of every pack but an emblem's. Where there
 * is one, its flags say which of the four colours it drives and the rest
 * stay the batch's baked ones: that is what gcDrawMObjForDObj does with
 * its gSPLightColor, gDPSetPrimColor and gDPSetEnvColor lines, emitting
 * only the ones the flags ask for and leaving the display list's own to
 * stand. */
/* A live PRIM over the batch's baked one, each half only where the
 * batch's combiner reads it (fighter.h FPACK_COLOR_PRIM, FPACK_ALPHA_
 * PRIM). The N64 writes the whole register and the cycles take what
 * they were programmed to: an MObj that fades a SHADE-coloured list by
 * PRIM's alpha must not tint it with PRIM's RGB, and a list whose alpha
 * is TEXEL0 alone must not fade with a PRIM it never reads. */
static uint32_t prim_halves(const FPackBatch *b, uint32_t baked,
                            uint32_t live)
{
    uint32_t rgb = (b->alpha_src & FPACK_COLOR_PRIM) ? live : baked;
    uint32_t a = (b->alpha_src & FPACK_ALPHA_PRIM) ? live : baked;

    return (rgb & 0xFFFFFF00u) | (a & 0xFFu);
}

static void material_of(Material *m, const Fighter *f, const FPackBatch *b,
                        const DCMObjColor *mc)
{
    uint32_t prim = b->prim, light1 = b->light1, light2 = b->light2;
    uint32_t env = b->env;

    /* the costume's colours over costume 0's (fighter.h FPackCostumes) */
    if (f->costumes != NULL)
    {
        const FPackCostumeMat *cm =
            &f->costume_mats[f->costume * (int)f->hd->batch_count +
                             (int)(b - f->batches)];

        prim = cm->prim;
        light1 = cm->light1;
        light2 = cm->light2;
        env = cm->env;
    }

    /* The two live colours a display proc set ahead of the draw
     * (fighter_set_prim_color, fighter_set_env_color): the game's
     * gDPSetPrimColor and gDPSetEnvColor before a gcDrawDObj*, which
     * stand until the display list's own or an MObj's overwrite them --
     * so they go under the MObj's below, as the RDP's order has it. */
    if (f->prim_live_on)
    {
        prim = prim_halves(b, prim, f->prim_live);
    }
    if (f->env_live_on && (b->bucket & FPACK_ENVLERP))
    {
        env = f->env_live >> 8;
    }
    if (mc != NULL && mc->live)
    {
        if (mc->flags & FPACK_MOBJ_PRIM)
        {
            prim = prim_halves(b, prim, mc->prim);
        }
        if ((mc->flags & FPACK_MOBJ_ENV) && (b->bucket & FPACK_ENVLERP))
        {
            /* DCMObjColor.env is the game's packed RGBA and the batch's
             * is 0x00RRGGBB, which is the same three bytes one shift
             * apart. Only a batch whose combiner reads ENV takes it: an
             * MObj may set the flag over a display list that never
             * asked (ef/efmanager.c writes the player's colour into all
             * of an explosion's, and gcDrawMObjForDObj emits it for
             * all of them too). */
            env = mc->env >> 8;
        }
        if (mc->flags & FPACK_MOBJ_LIGHT1)
        {
            light1 = mc->light1;
        }
        if (mc->flags & FPACK_MOBJ_LIGHT2)
        {
            light2 = mc->light2;
        }
    }
    /* lerp(ENV, PRIM, TEXEL0) = TEXEL0 * (PRIM - ENV) + ENV, and the PVR
     * multiplies the base colour by the texel and adds the offset --
     * which is what src/dc/lbcommon.c's nLBCommonCombineIAPrimEnv does
     * for a sprite drawn with this combiner.
     *
     * The base cannot go negative, so a channel where ENV is the
     * brighter of the two clamps here and draws ENV where the RDP would
     * have fallen to PRIM. That is the port's one inexactness in the
     * dead explosion: tools/check/dexp_check.py measures it over all four
     * players, all three shards and every tic -- eight of the twelve
     * pairs exact, three inside 17 of 255, one (player 2's third shard)
     * at 159 -- and fails the build if it grows. An exact two-pass version
     * would cost more than it buys. */
    m->inv = 0;
    if (b->bucket & FPACK_ENVLERP)
    {
        uint32_t r = (prim >> 24) & 0xFF, g = (prim >> 16) & 0xFF;
        uint32_t bl = (prim >> 8) & 0xFF;
        uint32_t er = (env >> 16) & 0xFF, eg = (env >> 8) & 0xFF;
        uint32_t eb = env & 0xFF;
        int bi = (int)(b - f->batches);

        /* ENV the brighter in every channel, and an inverted texture to
         * hand (Fighter.hdr_inv): (ENV - PRIM) * (1 - T) + PRIM, exact
         * where the form below would draw flat ENV */
        if (f->has_inv != NULL && f->has_inv[bi] &&
            er >= r && eg >= g && eb >= bl && (er > r || eg > g || eb > bl))
        {
            prim = ((er - r) << 24) | ((eg - g) << 16) |
                   ((eb - bl) << 8) | (prim & 0xFF);
            m->offset = (r << 16) | (g << 8) | bl;
            m->inv = 1;
        }
        else
        {
            prim = ((r > er ? r - er : 0) << 24) |
                   ((g > eg ? g - eg : 0) << 16) |
                   ((bl > eb ? bl - eb : 0) << 8) | (prim & 0xFF);
            m->offset = env & 0x00FFFFFF;
        }
    }
    else m->offset = 0;

    ft_shade_unpack(prim, m->base);
    /* G_RM_FOG_PRIM_A (ftshade.h ft_shade_fog), by Fighter.fog_live --
     * exact for every combiner here, since the blend is applied after
     * it. Alpha is left alone, as the blender leaves it. A joint whose
     * part is FIGHTER_PART_NOFOG is drawn G_RM_PASS, out of the fog. */
    if (((f->fog_live & 0xFF) != 0) &&
        !((b->joint < FIGHTER_MAX_JOINTS) &&
          (f->part_flags[b->joint] & FIGHTER_PART_NOFOG)))
    {
        ft_shade_fog(m->base, &m->offset, f->fog_live);
    }
    m->alpha = prim & 0xFF;
    ft_shade_unpack(light1, m->dif);
    ft_shade_unpack(light2, m->amb);
    m->shaded = b->shaded;
    m->alpha_shade = (b->alpha_src & FPACK_ALPHA_SHADE) ? 1 : 0;
#ifdef DB_SHADE_FIXED
    ft_shade_fx_prep(&m->fx, m->base, m->amb, m->dif);
#endif
}

static inline __attribute__((always_inline))
uint32_t shade(const Material *m, const FPackVtx *v,
              float lx, float ly, float lz)
{
    float d;

    if (!m->shaded)
        return (m->alpha << 24) | (ft_shade_chan(m->base[0]) << 16) |
               (ft_shade_chan(m->base[1]) << 8) | ft_shade_chan(m->base[2]);
    if (m->shaded == 2)
        /* G_LIGHTING off: the normal slots carry the vertex colour. The
         * alpha byte beside it is only the vertex's when the RDP's alpha
         * cycle reads SHADE; where it does not, the byte is whatever the
         * modeller left and must not reach the blender. PRIM's alpha
         * multiplies either: m->alpha is 0xFF unless the alpha cycle
         * reads PRIM (material_of); this arm applies it, so unlit
         * geometry fades by PRIM (Master Hand's shadow in the opening
         * Room). */
        return ((uint32_t)(m->alpha_shade ? (v->alpha * m->alpha) / 0xFF
                                          : m->alpha) << 24) |
               (ft_shade_chan(m->base[0] * v->nx) << 16) |
               (ft_shade_chan(m->base[1] * v->ny) << 8) |
               ft_shade_chan(m->base[2] * v->nz);
    /* ftshade.h: SHADE is clamped before PRIM multiplies it, as the
     * RSP and RDP split the work. tools/check/shade_check.py holds it to
     * that. */
    d = v->nx * lx + v->ny * ly + v->nz * lz;
#ifdef DB_SHADE_FIXED
    /* -DDB_SHADE_FIXED: the integer lighting (ftshade.h ft_shade_lit_fx).
     * Off by default: it measured 3% slower than the float path off-console,
     * so it waits for a console A/B */
    return ft_shade_lit_fx(&m->fx, d, m->alpha);
#else
    return ft_shade_lit(m->base, m->amb, m->dif, d, m->alpha);
#endif
}

static uint32_t gTris;

/* The offset colour every vertex of the current batch carries, out of
 * material_of: an FPACK_ENVLERP batch's ENV, a fogged one's fog * a,
 * zero otherwise (and zero for an untextured batch, whose vertex colours
 * carry it instead; draw_batch says why). It is a
 * file static for the same reason gZBias is: emit_tri is called per
 * triangle by the clipper and takes no material. */
static uint32_t gOffsetARGB;

/* Depth multiplier for z-off layers. The game draws those layers with
 * the z-buffer off in painting order; their geometry still has real 3D
 * depths, and the z-off is what lets coplanar overlays (wall trim) win
 * over what they overlay. On a deferred renderer: keep the real 1/w
 * depth and nudge each successive batch fractionally closer, so
 * coplanar pieces keep the painting order while everything else
 * resolves by true depth. DEPTHCMP_ALWAYS punches through real
 * geometry; a constant far depth loses the layer's own 3D layout. */
static float gZBias;

/* fighter_draw_layered: the depth every vertex starts from, and how much
 * of the real 1/w is added to it (0 and 1 for the two ordinary draws) */
static float gZBase = 0.0f;
static float gZScale = 1.0f;

/* fighter_draw_layered_ortho: depth is gZBase + gZOrthoScale *
 * (gZOrthoMax - clip z / w) while gZOrtho is set -- the model's own
 * range of clip z, nearest highest, onto the sprite step -- and the
 * planes every triangle is clipped to after the near plane, or NULL. */
static int gZOrtho = 0;
static float gZOrthoScale = 0.0f;
static float gZOrthoMax = 0.0f;
static const float (*gClipPlanes)[3] = NULL;
static int gClipNPlanes = 0;
static float gClipInradius = 0.0f;

/* The slot fighter_draw_layered gave the model, as a floor and a ceiling
 * emit_tri holds every vertex to while gZBand is set. The scale and the
 * anchor there put the model's OWN vertices inside it, but the vertices
 * emit_tri actually draws are not all of those: draw_batch clips each
 * triangle against the near plane first, and a vertex the clipper makes
 * on that plane is nearer than anything the model measured, so its 1/w
 * is above the band's top. One such vertex drawn a step high is a limb
 * in the next model's slot, which is the artefact the anchor exists to
 * prevent -- so the band is a promise rather than an expectation. */
static int gZBand = 0;

/* fighter_set_dim: every colour emit_tri hands the PVR, base and offset,
 * scaled by gDim / 256 -- 256 is none. The alpha stays. */
static unsigned gDim = 256;

static inline uint32_t dim_rgb(uint32_t c)
{
    uint32_t rb = (((c & 0x00FF00FFu) * gDim) >> 8) & 0x00FF00FFu;
    uint32_t g = (((c & 0x0000FF00u) * gDim) >> 8) & 0x0000FF00u;

    return (c & 0xFF000000u) | rb | g;
}

void fighter_set_dim(unsigned k)
{
    gDim = (k > 256) ? 256 : k;
}
static float gZBandLo = 0.0f, gZBandHi = 0.0f;

/* fighter_draw_layered_paint: while above zero, draw_batches puts every
 * vertex of batch i at gZBandLo + (i + 0.5) * gZPaintStep -- one flat
 * depth per batch, rising in the pack's (the display list's) order --
 * and the real 1/w plays no part. */
static float gZPaintStep = 0.0f;

/* fighter_draw_layered_scaled: while above zero, batch i's 1/w is scaled
 * by 1 + i * gZBatchNudge on top of gZScale -- a uniform scale per batch,
 * so later batches sit a hair nearer at every pixel the way a Z-less
 * display list draws them over the earlier ones, and the texture's
 * perspective correction (the same 1/w) is untouched. */
static float gZBatchNudge = 0.0f;

/* A header's TSP word (the TA polygon parameter's third word, KOS's
 * mode2 -- by index, so the host stub's plain words take it too) with
 * its blend replaced: source and destination factors, and the two
 * secondary-accumulation-buffer selects, SRC_SELECT (bit 25: the blend's
 * source is the secondary buffer, not the polygon) and DST_SELECT (bit
 * 24: the blend reads and writes the secondary buffer, not the frame). */
static void hdr_blend(pvr_poly_hdr_t *out, const pvr_poly_hdr_t *in,
                      uint32_t src, uint32_t dst, int src_sel, int dst_sel)
{
    uint32_t *w = (uint32_t *)out;

    *out = *in;
    w[2] = (w[2] & ~((7u << 29) | (7u << 26) | (1u << 25) | (1u << 24))) |
           (src << 29) | (dst << 26) | ((uint32_t)(src_sel != 0) << 25) |
           ((uint32_t)(dst_sel != 0) << 24);
}

/* see fighter.h; the whole 640x480 until a camera pass says otherwise */
DCViewport gDCViewport = { 320.0f, 240.0f, 320.0f, 240.0f };

#ifdef DB_PVR_BUDGET
#include "objpvr.h"
/* -DDB_PVR_BUDGET, off by default: what one frame asks of the TA, in the
 * two currencies the hardware actually meters.
 *
 *   1. The vertex buffer. Every parameter every pass submits is stored
 *      there, and dc_pvr_init sizes it once.
 *   2. The object pointer buffer, which is per TILE and per LIST. The TA
 *      writes one pointer for each STRIP that touches a 32x32 tile, and
 *      emit_tri ends every triangle with PVR_CMD_VERTEX_EOL -- so in
 *      this renderer a strip is one triangle and a pointer is one
 *      triangle. dc_pvr_init asks for PVR_BINSIZE_16 (16 words) with
 *      three overflow blocks, which is DB_PVR_OPB_CAP pointers a tile.
 *
 * These registers are not readable from the CPU in a useful way
 * (PVR_TA_VERTBUF_POS, the OPBs), so this counts what the port
 * SUBMITS instead, which is the same number, and bins each
 * triangle's screen bounding box into tiles the way the TA bins the
 * triangle itself. It over-counts a triangle whose bounding box covers a
 * tile its edges miss, and counts nothing but model geometry -- sprites
 * and the HUD are on top of this. */
/* The TA gives every tile of every list ONE block up front -- that is
 * what opb_sizes buys -- and takes any further block a tile needs from a
 * single pool shared by the whole frame, which is what opb_overflow_count
 * buys (KOS pvr_buffers.c: PVR_TA_OPB_START..END, one moving POS). So the
 * per-tile number below is a shape, and the number that can actually run
 * out is the pool: DB_PVR_OPB_POOL blocks for the frame.
 *
 * A 16-word block holds 15 object pointers and spends its last word on
 * the link to the next, so a tile wanting n pointers takes ceil(n / 15)
 * blocks, one of which is the one it already had. */
#define DB_PVR_OPB_PER_BLOCK 15
/* DB_PVR_TW/TH and DB_PVR_OPB_POOL are in fighter.h. */
/* What the TA stores for one independent triangle: the object's own
 * global parameter and three vertices, 32 bytes each. */
#define DB_PVR_TRI_BYTES 128

static uint16_t gDbTile[3][DB_PVR_TW * DB_PVR_TH];
static uint32_t gDbTris[3];
static uint32_t gDbWorstTile[3], gDbWorstBytes, gDbWorstPool;

static int db_pvr_list_slot(void)
{
    switch (gcGetDrawList())
    {
    case PVR_LIST_PT_POLY: return 1;
    case PVR_LIST_TR_POLY: return 2;
    default:               return 0;
    }
}

static void db_pvr_budget_tri(float x0, float y0, float x1, float y1,
                              float x2, float y2)
{
    int slot = db_pvr_list_slot();
    float lo_x = x0, hi_x = x0, lo_y = y0, hi_y = y0;
    int tx0, tx1, ty0, ty1, tx, ty;

    if (x1 < lo_x) lo_x = x1;
    if (x1 > hi_x) hi_x = x1;
    if (x2 < lo_x) lo_x = x2;
    if (x2 > hi_x) hi_x = x2;
    if (y1 < lo_y) lo_y = y1;
    if (y1 > hi_y) hi_y = y1;
    if (y2 < lo_y) lo_y = y2;
    if (y2 > hi_y) hi_y = y2;

    gDbTris[slot]++;

    if (hi_x < 0.0f || hi_y < 0.0f || lo_x > 639.0f || lo_y > 479.0f)
        return;
    tx0 = (lo_x < 0.0f) ? 0 : (int)(lo_x) / 32;
    ty0 = (lo_y < 0.0f) ? 0 : (int)(lo_y) / 32;
    tx1 = (hi_x > 639.0f) ? DB_PVR_TW - 1 : (int)(hi_x) / 32;
    ty1 = (hi_y > 479.0f) ? DB_PVR_TH - 1 : (int)(hi_y) / 32;

    for (ty = ty0; ty <= ty1; ty++)
        for (tx = tx0; tx <= tx1; tx++)
            gDbTile[slot][ty * DB_PVR_TW + tx]++;
}

/* Called from the frame loop after pvr_scene_finish: fold this frame
 * into the high-water marks and start the next one clean. */
void db_pvr_budget_frame(unsigned *worst_tile, unsigned *worst_bytes,
                         unsigned *tris, unsigned *pool)
{
    uint32_t bytes = 0, blocks = 0;
    int slot, i;

    for (slot = 0; slot < 3; slot++)
    {
        for (i = 0; i < DB_PVR_TW * DB_PVR_TH; i++)
        {
            unsigned n = gDbTile[slot][i];

            if (n > gDbWorstTile[slot])
                gDbWorstTile[slot] = n;
            if (n > DB_PVR_OPB_PER_BLOCK)
                blocks += (n + DB_PVR_OPB_PER_BLOCK - 1) /
                          DB_PVR_OPB_PER_BLOCK - 1;
            gDbTile[slot][i] = 0;
        }
        bytes += gDbTris[slot] * DB_PVR_TRI_BYTES;
        worst_tile[slot] = gDbWorstTile[slot];
        tris[slot] = gDbTris[slot];
        gDbTris[slot] = 0;
    }
    if (bytes > gDbWorstBytes)
        gDbWorstBytes = bytes;
    if (blocks > gDbWorstPool)
        gDbWorstPool = blocks;
    *worst_bytes = gDbWorstBytes;
    *pool = gDbWorstPool;
}
#endif /* DB_PVR_BUDGET */

/* One vertex of a triangle, as the PVR takes it: the divide, the viewport
 * map, the depth, the dim. Everything here is a function of the vertex and
 * of state that is constant across a batch, so draw_batch can compute it
 * once per vertex (vcache) instead of once per triangle corner. */
static inline __attribute__((always_inline))
void emit_vtx(pvr_vertex_t *pv, const ClipVtx *p)
{
    /* Every input is read before the first store: pv and the globals are
     * both float memory, so a store to pv->x would otherwise make the
     * compiler reload gDCViewport, gZBias and the rest for each field.
     * The arithmetic and its order are unchanged. */
    const float vhw = gDCViewport.hw, vcx = gDCViewport.cx;
    const float vcy = gDCViewport.cy, vhh = gDCViewport.hh;
    const float pcx = p->cx, pcy = p->cy, pcz = p->cz, pcw = p->cw;
    const int ortho = gZOrtho, band = gZBand;
    const unsigned dim = gDim;
    const float zbase = gZBase;
    uint32_t argb = p->argb, oargb = gOffsetARGB;
    /* One reciprocal, three ways. SH4 FDIV is ~11-13 cycles and does
     * not pipeline, so three divides per vertex is the single most
     * expensive thing in this loop. */
    float invw = 1.0f / pcw;
    float x = pcx * invw * vhw + vcx;
    float y = vcy - pcy * invw * vhh;
    float z = ortho ? zbase + gZOrthoScale * (gZOrthoMax - pcz * invw)
                    : zbase + gZBias * gZScale * invw;

    if (band)
    {
        if (z < gZBandLo)
            z = gZBandLo;
        else if (z > gZBandHi)
            z = gZBandHi;
    }
    if (dim != 256)
    {
        argb = dim_rgb(argb);
        oargb = dim_rgb(oargb);
    }
    pv->flags = PVR_CMD_VERTEX;
    pv->x = x;
    pv->y = y;
    pv->z = z;
    pv->u = p->u;
    pv->v = p->v;
    pv->argb = argb;
    pv->oargb = oargb;
}

static void emit_tri(const ClipVtx *p0, const ClipVtx *p1, const ClipVtx *p2)
{
    pvr_vertex_t pv[3];

    emit_vtx(&pv[0], p0);
    emit_vtx(&pv[1], p1);
    emit_vtx(&pv[2], p2);
    pv[2].flags = PVR_CMD_VERTEX_EOL;
    pvr_prim(pv, sizeof(pv));
    gTris++;
#ifdef DB_PVR_BUDGET
    db_pvr_budget_tri(pv[0].x, pv[0].y, pv[1].x, pv[1].y, pv[2].x, pv[2].y);
#endif
}

/* One batch, once: the header, then its triangles clipped and emitted.
 *
 * `du`/`dv` translate the texture coordinates and `fade` scales the alpha
 * every vertex carries, in 0..256. `fade` is 256 for every batch but the
 * second pass of a cross-fade (FPACK_TEXLERP), which is why this is a
 * function at all: that batch is drawn twice, from the same vertices and
 * the same clip positions, with a different tile. The translation is
 * zero but where a material script scrolls the tile the pass draws
 * (FPackMObjSub.uv_base, uv0_base). */
/* What draw_batch holds constant while it walks one batch's triangles. */
typedef struct
{
    Fighter *f;
    const Material *mat;
    int fold, fade, texgen;
    float du, dv, su, sv;
} BatchCtx;

/* One triangle corner: the vertex's clip position, its shade and its
 * texture coordinates. */
static inline __attribute__((always_inline))
void batch_corner(const BatchCtx *c, int vi, ClipVtx *out)
{
    const Fighter *f = c->f;
    const FPackVtx *v = &f->verts[vi];
    const float *l = f->light[v->joint];
    const float *cp = f->vclip[vi];
    const float du = c->du, dv = c->dv;
    const int fade = c->fade;
    float u, w;
    uint32_t argb = shade(c->mat, v, l[0], l[1], l[2]);

    if (c->fold)
        argb = ft_shade_add_offset(argb, c->mat->offset);

    if (fade != 256)
    {
        uint32_t a = ((argb >> 24) * (uint32_t)fade) >> 8;

        argb = (argb & 0x00FFFFFFu) | (a << 24);
    }
    if (c->texgen)
    {
        const float (*g)[3] = f->texgen[v->joint];

        u = 0.5f + 0.5f * (v->nx * g[0][0] +
                           v->ny * g[0][1] +
                           v->nz * g[0][2]) + du;
        w = 0.5f - 0.5f * (v->nx * g[1][0] +
                           v->ny * g[1][1] +
                           v->nz * g[1][2]) + dv;
    }
    else
    {
        u = v->u * c->su + du;
        w = v->v * c->sv + dv;
    }
    out->cx = cp[0];
    out->cy = cp[1];
    out->cz = cp[2];
    out->cw = cp[3];
    out->u = u;
    out->v = w;
    out->argb = argb;
}

/* Software backface culling ahead of the shade. The PVR culls by the
 * header's mode (compile_batch_txr: CW for G_CULL_BACK, CCW for
 * G_CULL_FRONT), so a triangle it will throw away still cost this loop its
 * shade, its UVs, its divides and 96 bytes on the wire. This is the same
 * test made from the clip-space vertices fighter_frame already holds, with
 * no divide: for w > 0 the sign of the determinant of the three (x, y, w)
 * rows is the sign of the NDC area. It skips a triangle only when that area
 * is well clear of zero (1e-5 of the NDC square), and only when all three
 * vertices are in front of the near plane, so a sliver, a clipped triangle
 * or one the two tests might split on stays with the PVR, which still has
 * the mode in its header. */
/* -1 is the sign that matches the PVR's CW/CCW for this port's winding,
 * found by the frame diff: +1 culls the faces that should show (the
 * characters vanish). -DDB_CULL_SIGN=1.0f reproduces that for a look. */
#ifndef DB_CULL_SIGN
#define DB_CULL_SIGN (-1.0f)
#endif
static inline int tri_culled(const Fighter *f, int v0, int v1, int v2, float dir)
{
    const float *a = f->vclip[v0], *b = f->vclip[v1], *c = f->vclip[v2];
    const float w0 = a[3], w1 = b[3], w2 = c[3];
    float d;

    if (!(w0 > 0.0f && w1 > 0.0f && w2 > 0.0f) ||
        !(a[2] + w0 >= 0.0f && b[2] + w1 >= 0.0f && c[2] + w2 >= 0.0f))
        return 0;
    d = a[0] * (b[1] * w2 - c[1] * w1) - a[1] * (b[0] * w2 - c[0] * w1) +
        w0 * (b[0] * c[1] - c[0] * b[1]);
    return dir * d > 1.0e-5f * w0 * w1 * w2;
}

/* The per-batch vertex cache. A vertex that is in front of the near plane
 * has its PVR vertex built once (shade, UV, divide, viewport, depth, dim)
 * and every triangle that uses it copies the 32 bytes. sVCGen is bumped per
 * batch so nothing is cleared between batches; a model with more vertices
 * than VCACHE_MAX just takes the old path. Kirby's pack is the largest at
 * 2107. */
#define VCACHE_MAX 2560
static pvr_vertex_t sVC[VCACHE_MAX] __attribute__((aligned(32)));
static uint16_t sVCStamp[VCACHE_MAX];
static uint8_t sVCIn[VCACHE_MAX];
static uint16_t sVCGen;

/* Bit 0: vertex vi is in front of the near plane (the test clip_near
 * makes). Bit 1: it is on the inside of every user clip plane (the test
 * clip_planes makes), which is always so when the draw has none. A
 * triangle whose three vertices have both bits is what neither clipper
 * would touch, so the cached vertices are the ones it would emit. Builds
 * the cached PVR vertex on the first look. */
static inline int vcache_get(const BatchCtx *c, int vi)
{
    if (sVCStamp[vi] != sVCGen)
    {
        ClipVtx cv;
        int in, flags;

        DBPERF_FILL();
        batch_corner(c, vi, &cv);
        in = (cv.cz + cv.cw >= 0);
        flags = in;
        if (in)
        {
            int p, inside = 1;

            for (p = 0; p < gClipNPlanes; p++)
            {
                if (gClipPlanes[p][0] * cv.cx + gClipPlanes[p][1] * cv.cy +
                    gClipPlanes[p][2] * cv.cw < 0.0f)
                {
                    inside = 0;
                    break;
                }
            }
            flags |= inside << 1;
            emit_vtx(&sVC[vi], &cv);
        }
        sVCIn[vi] = (uint8_t)flags;
        sVCStamp[vi] = sVCGen;
    }
    return sVCIn[vi];
}

static void draw_batch(Fighter *f, const FPackBatch *b,
                       const pvr_poly_hdr_t *hdr, const Material *mat,
                       float du, float dv, int fade)
{
    ClipVtx clip[3];
    ClipVtx poly[CLIP_MAX_VERTS];
    ClipVtx scratch[CLIP_MAX_VERTS];
    int t, k;

    /* An untextured polygon gets no offset colour from the PVR, so a
     * fog's `+ fog * a` would be lost and the batch would only darken
     * toward black while the textured ones brightened toward the fog
     * (the respawn flash): it goes into the vertex colour instead. */
    int fold = (mat->offset != 0) && !(hdr->cmd & PVR_TA_CMD_TXRENABLE);
    /* G_TEXTURE_GEN: the coordinates in the pack are not this batch's.
     * The RSP makes them from the vertex normal against the two
     * gSPLookAt vectors, which fighter_frame has already put in this
     * joint's own space, so each is one dot product mapped from the
     * dot's -1..1 onto the texture's 0..1 -- gu's guLookAtReflect
     * convention, and the sphere map is drawn to it. */
    int texgen = (b->bucket & FPACK_TEXGEN) && f->has_texgen;
    /* the extern texture's window (fighter_set_extern_window); the
     * identity, exactly, for every other batch */
    float su = 1.0f, sv = 1.0f;

    if (b->tex >= 0 && f->texs[b->tex].fmt == FPACK_TEX_EXTERN)
    {
        du += f->ext_win[0];
        dv += f->ext_win[1];
        su = f->ext_win[2];
        sv = f->ext_win[3];
    }

    gOffsetARGB = fold ? 0 : mat->offset;
    pvr_prim(hdr, sizeof(*hdr));

    /* the whole fighter's alpha (Fighter.alpha_cut), 255 -> 256 */
    if (f->alpha_cut != 0)
    {
        uint32_t a = 0xFFu - f->alpha_cut;

        fade = (fade * (int)(a + (a >> 7))) >> 8;
    }

    {
        BatchCtx bc;
        /* Vertices come in from up to six triangles each, and everything
         * shade, UV and emit_vtx compute is a function of the vertex and
         * of this batch, so it is computed once a batch, not once a
         * corner. Only a triangle wholly in front of the near plane and
         * inside every user clip plane takes the cached path; anything
         * either clipper would cut runs the corner-by-corner path below.
         * The results are the same bits either way. */
#ifdef DB_NO_VCACHE
        /* -DDB_NO_VCACHE: the pre-cache path, for an A/B on the same source */
        int cached = 0;
#else
        int cached = ((int)f->hd->vert_count <= VCACHE_MAX);
#endif

        if (!cached)
            DBPERF_NOCACHE();
        /* +1 culls what the CW mode culls (G_CULL_BACK alone), -1 the CCW
         * one, 0 a batch the PVR draws both faces of */
        float cdir = 0.0f;

        {
            uint16_t cull = b->bucket & (FPACK_CULL_BACK | FPACK_CULL_FRONT);

            if (cull == FPACK_CULL_BACK)
                cdir = DB_CULL_SIGN;
            else if (cull == FPACK_CULL_FRONT)
                cdir = -DB_CULL_SIGN;
            /* a draw with user clip planes (the layered ones) never
             * culled, when it ran the corner path below; keep its output
             * what it was */
            if (gClipPlanes != NULL)
                cdir = 0.0f;
        }
        bc.f = f;
        bc.mat = mat;
        bc.fold = fold;
        bc.fade = fade;
        bc.texgen = texgen;
        bc.du = du;
        bc.dv = dv;
        bc.su = su;
        bc.sv = sv;
        if (cached && ++sVCGen == 0)
        {
            memset(sVCStamp, 0, sizeof(sVCStamp));
            sVCGen = 1;
        }

        DBPERF_LAP_BEGIN();
        for (t = b->tri_first; t < b->tri_first + b->tri_count; t++)
        {
            int n;

            if (cached)
            {
                int v0 = f->tris[t][0], v1 = f->tris[t][1], v2 = f->tris[t][2];
                int in0, in1, in2;

#ifndef DB_NO_CULL
                if (cdir != 0.0f && tri_culled(f, v0, v1, v2, cdir))
                {
                    DBPERF_CULLED();
#ifndef DB_CULL_DRY
                    /* -DDB_CULL_DRY: make the test and count it, skip nothing,
                     * so a frame can be timed like the uncached path */
                    continue;
#endif
                }
#endif
                in0 = vcache_get(&bc, v0);
                in1 = vcache_get(&bc, v1);
                in2 = vcache_get(&bc, v2);

                DBPERF_LAP(DBP_SHADE);
                if ((in0 & in1 & in2 & 3) == 3)
                {
                    pvr_vertex_t pv[3];

                    pv[0] = sVC[v0];
                    pv[1] = sVC[v1];
                    pv[2] = sVC[v2];
                    pv[2].flags = PVR_CMD_VERTEX_EOL;
                    pvr_prim(pv, sizeof(pv));
                    gTris++;
#ifdef DB_PVR_BUDGET
                    db_pvr_budget_tri(pv[0].x, pv[0].y, pv[1].x, pv[1].y, pv[2].x, pv[2].y);
#endif
                    DBPERF_LAP(DBP_EMIT);
                    continue;
                }
            }
            for (k = 0; k < 3; k++)
                batch_corner(&bc, f->tris[t][k], &clip[k]);
            DBPERF_LAP(DBP_SHADE);
            n = clip_near(clip, poly);
            if (n >= 3 && gClipPlanes != NULL)
                n = clip_planes(poly, n, gClipPlanes, gClipNPlanes,
                                gClipInradius, scratch);
            DBPERF_LAP(DBP_CLIP);
            if (n < 3)
                continue;
            for (k = 1; k + 1 < n; k++)
                emit_tri(&poly[0], &poly[k], &poly[k + 1]);
            DBPERF_LAP(DBP_EMIT);
        }
    }
}

/* want_list: one FPACK_LIST_* -- the batches whose bucket names that list,
 * into it -- or -1 for every batch into the translucent list, which is
 * fighter_draw_layered's whole point. */
static uint32_t draw_batches_impl(Fighter *f, int want_list);

static uint32_t draw_batches(Fighter *f, int want_list)
{
    uint32_t n;
    DBPERF_BEGIN(DBP_BATCH);
    n = draw_batches_impl(f, want_list);
    DBPERF_END(DBP_BATCH);
    return n;
}

int fighter_has_list(Fighter *f, int fpack_list)
{
    if (!f->list_mask_ok)
    {
        uint8_t mask = 0;
        int i;

        for (i = 0; i < (int)f->hd->batch_count; i++)
        {
            uint16_t bucket = f->batches[i].bucket;

            mask |= (uint8_t)(1u << (bucket & FPACK_LIST_MASK));
            if (bucket & FPACK_TEXLERP)
                mask |= (uint8_t)(1u << FPACK_LIST_TR);
        }
        f->list_mask = mask;
        f->list_mask_ok = 1;
    }
    return (f->list_mask >> fpack_list) & 1;
}

static uint32_t draw_batches_impl(Fighter *f, int want_list)
{
    int i;

    gTris = 0;
#ifndef DB_NO_LISTMASK
    /* a pass into a list none of this model's batches belong to */
    if (want_list >= 0 && !fighter_has_list(f, want_list))
        return 0;
#endif
    for (i = 0; i < (int)f->hd->batch_count; i++)
    {
        const FPackBatch *b = &f->batches[i];
        int hk = f->costume * (int)f->hd->batch_count + i;
        const pvr_poly_hdr_t *hdr = (want_list < 0) ? &f->hdr_tr[hk]
                                                   : &f->hdr[hk];

        /* the face frame its texture part is on (fighter.h
         * FPackTexParts); a hole in the sprite array keeps the tile */
        if (f->hdr_frames != NULL && f->texpart_batches[i].part != 0xFF)
        {
            const FPackTexPartBatch *tb = &f->texpart_batches[i];
            int fc = f->texparts->frame_count[tb->part];
            int fr = f->texpart_frame[tb->part];

            if (fr < fc)
            {
                int e = tb->frame_first + f->costume * fc + fr;

                if (f->texpart_frames[e] >= 0)
                    hdr = &f->hdr_frames[2 * e + (want_list < 0 ? 1 : 0)];
            }
        }
        const DCMObjColor *mc = fighter_batch_mobj(f, i);
        const FPackMObjSub *ms = NULL;
        Material mat;
        int own = (int)(b->bucket & FPACK_LIST_MASK);
        int pass_a, pass_b;

        if (f->hdr_mobj != NULL && f->hdr_mobj[i] != NULL && mc != NULL)
            ms = &f->mobj_subs[f->mobj_batch[i]];
        /* A cross-fading batch is drawn TWICE -- tile A in the list its
         * bucket names, tile B over it at alpha lfrac in the translucent
         * list, which composites to A*(1-lfrac) + B*lfrac and is what the
         * N64's one-pass combiner computes (fighter.h FPACK_TEXLERP).
         * Dream Land's clouds are punch-through, so B cannot go in their
         * own list: nothing blends there. The translucent list runs after
         * and its polys test depth without writing it, so B lands on top
         * of A at exactly A's depth. */
        pass_b = (ms != NULL && (b->bucket & FPACK_TEXLERP) &&
                  (want_list < 0 || want_list == FPACK_LIST_TR));
        pass_a = (want_list < 0 || own == want_list);
        if (!pass_a && !pass_b)
            continue;
        /* joint_hide is a 64-bit mask and a stage pack has more joints
         * than that; it is zero for every pack but the arrows, whose
         * four fit, so a batch above 63 is simply never hidden. */
        if (b->joint < 64 && (f->joint_hide & ((uint64_t)1 << b->joint)))
            continue;
        /* a model part the joint does not have on (fighter.h FPackParts) */
        if (!fighter_part_on(f, b->part, b->joint))
            continue;
        gZBias = (b->bucket & FPACK_NOZ) ? 1.0f + (float)i * 2e-4f : 1.0f;
        if (gZBatchNudge > 0.0f)
        {
            gZBias = 1.0f + (float)i * gZBatchNudge;
        }
        if (gZPaintStep > 0.0f)
        {
            gZBase = gZBandLo + ((float)i + 0.5f) * gZPaintStep;
        }
        material_of(&mat, f, b, mc);
        if (mat.inv)
            hdr = &f->hdr_inv[2 * i + (want_list < 0 ? 1 : 0)];
        /* A cross-fade whose tile A is itself translucent (the boss
         * stage's fog, Effects1: two IA8 alpha masks, the colour all
         * SHADE) cannot be two blended passes: A then B-at-lfrac covers
         * 1 - (1 - a)(1 - b*lfrac) of the frame where the N64 covers
         * a*(1 - lfrac) + b*lfrac. Three passes through the PVR's
         * secondary accumulation buffer give exactly the N64's:
         *   1. tile A at alpha (1 - lfrac), ONE/ZERO into the secondary
         *      buffer -- it holds the colour and a*(1 - lfrac);
         *   2. tile B at alpha lfrac with its colour scaled to black,
         *      ONE/ONE into it -- the alpha is now the lerp, the colour
         *      is still A's;
         *   3. the same triangles once more, the secondary buffer as the
         *      source, SRCALPHA/INVSRCALPHA onto the frame.
         * The colour is tile A's throughout, which is the lerp's where
         * the two tiles' colours agree -- the fog's, whose intensity is
         * 15 in every texel of both (the one translucent cross-fade in
         * the game; Dream Land's clouds are punch-through and take the
         * two-pass path below). Each pass sits a tenth of the batch's
         * paint step nearer than the last where there is one, so even a
         * sort that breaks depth ties some other way keeps the order. */
        if (pass_a && pass_b && own == FPACK_LIST_TR)
        {
            int fa = (mc->tex >= (uint16_t)ms->tex_count) ? ms->tex_count - 1
                                                          : (int)mc->tex;
            int fb = (mc->tex2 >= (uint16_t)ms->tex_count) ? ms->tex_count - 1
                                                           : (int)mc->tex2;
            float lf = mc->lfrac;
            int fade, k;
            unsigned dim = gDim;
            float zbase = gZBase, nudge = gZPaintStep * 0.1f;
            float bias = gZBias;
            pvr_poly_hdr_t h __attribute__((aligned(32)));

            lf = (lf < 0.0f) ? 0.0f : (lf > 1.0f) ? 1.0f : lf;
            fade = (int)(lf * 256.0f);
            for (k = 0; k < 3; k++)
            {
                const pvr_poly_hdr_t *src =
                    &f->hdr_mobj[i][(k == 1 ? fb : fa) * 2 + 1];

                /* the paint draw's step is a depth to add; the stack draw's
                 * (no paint step) is a scale, so the passes stay
                 * perspective-correct there */
                gZBase = zbase + nudge * (float)k;
                if (nudge == 0.0f)
                    gZBias = bias * (1.0f + 1e-5f * (float)k);
                if (k == 0)
                {
                    hdr_blend(&h, src, PVR_BLEND_ONE, PVR_BLEND_ZERO, 0, 1);
                    draw_batch(f, b, &h, &mat, mc->du0, mc->dv0, 256 - fade);
                }
                else if (k == 1)
                {
                    hdr_blend(&h, src, PVR_BLEND_ONE, PVR_BLEND_ONE, 0, 1);
                    gDim = 0;
                    draw_batch(f, b, &h, &mat, mc->du, mc->dv, fade);
                    gDim = dim;
                }
                else
                {
                    hdr_blend(&h, src, PVR_BLEND_SRCALPHA,
                              PVR_BLEND_INVSRCALPHA, 1, 0);
                    draw_batch(f, b, &h, &mat, mc->du0, mc->dv0, 256);
                }
            }
            gZBase = zbase;
            gZBias = bias;
            continue;
        }
        /* The frame of the sprite array the MObj's MatAnimJoint has it
         * on, if it has one: the picture is in the header, so a moving
         * picture is a different header. Out-of-range is clamped rather
         * than refused -- objanim.c interpolates texture_id_curr like
         * any other track and the last command of the slash's script
         * leaves it exactly on the last frame. */
        if (ms != NULL)
        {
            int fr = (mc->tex >= (uint16_t)ms->tex_count) ? ms->tex_count - 1
                                                          : (int)mc->tex;

            hdr = &f->hdr_mobj[i][fr * 2 + (want_list < 0 ? 1 : 0)];
        }
        /* Tile 0's own scroll (FPackMObjSub.uv0_base), zero for every
         * MObj whose script leaves trau/trav alone. */
        if (pass_a)
            draw_batch(f, b, hdr, &mat, (mc != NULL) ? mc->du0 : 0.0f,
                       (mc != NULL) ? mc->dv0 : 0.0f, 256);
        if (pass_b)
        {
            int fr = (mc->tex2 >= (uint16_t)ms->tex_count) ? ms->tex_count - 1
                                                           : (int)mc->tex2;
            float lf = mc->lfrac;
            int fade;

            /* objdisplay.c:1243 hands the RDP `lfrac * 255` as a byte, so
             * the hardware saturates at both ends and so does this. */
            lf = (lf < 0.0f) ? 0.0f : (lf > 1.0f) ? 1.0f : lf;
            fade = (int)(lf * 256.0f);
            if (fade > 0)
                draw_batch(f, b, &f->hdr_mobj[i][fr * 2 + 1], &mat,
                           mc->du, mc->dv, fade);
        }
    }
    return gTris;
}

uint32_t fighter_draw(Fighter *f)
{
    return draw_batches(f, FPACK_LIST_OP);
}

uint32_t fighter_draw_pt(Fighter *f)
{
    return draw_batches(f, FPACK_LIST_PT);
}

uint32_t fighter_draw_tr(Fighter *f)
{
    return draw_batches(f, FPACK_LIST_TR);
}

/* a*cx + b*cy + c*cw >= 0 for -cw <= cx <= cw and -cw <= cy <= cw */
static const float kFrustumSidePlanes[4][3] =
{
    {  1.0f,  0.0f, 1.0f },
    { -1.0f,  0.0f, 1.0f },
    {  0.0f,  1.0f, 1.0f },
    {  0.0f, -1.0f, 1.0f },
};

/* The lowest and highest, over this frame's vertices, of 1/w (ortho 0)
 * or of clip z / w (ortho 1): the two layered draws' idea of how deep
 * the model is.
 *
 * Walks exactly the vertex runs fighter_frame (above) transformed
 * this frame, not every vertex the pack carries: an inactive part's
 * run -- a costume's unworn hat, Kirby's fifteen copy hats, Link's
 * own fifteen, every fighter's electric skeleton -- is stepped over
 * there, so its f->vclip entry is whatever an earlier frame or costume
 * left there, or, for a run never yet transformed, whatever malloc
 * handed fighter_init. Reading it anyway divides by a stale or zero cw
 * and corrupts the whole spread; an infinity there makes every depth
 * the draw emits a NaN, and the model vanishes (Fox in the magnifying
 * glass: 33 of his 475 vertices, his unused blaster hands and his
 * skeleton, read +-inf).
 *
 * BEHIND THE EYE, and so not drawn: draw_batch clips every triangle
 * against the near plane (clip.h, cz + cw >= 0) before emit_tri sees
 * it. Measuring such a vertex's 1/w anyway reads a negative one, which
 * is not a depth this draw will ever emit and is far outside the range
 * of the ones it will -- one of them alone makes the spread hundreds of
 * times the model's real one, the scale hundreds of times too small,
 * and the whole fighter collapses onto a single depth where his
 * triangles tie and the renderer's own sort decides which of his limbs
 * is in front. That is the second half of the opening movie's fighters
 * drawing through each other: Clash flies them at the
 * camera, and the two nearest reach it, so anchoring the band alone
 * left exactly those two broken. Under an ortho camera nothing is
 * behind the eye. Returns 0 when no vertex was measured. */
static int live_extent(const Fighter *f, int ortho, float *lo, float *hi)
{
    int i = 0, t, vn = (int)f->hd->vert_count;
    int seen = 0;

    for (t = 0; t <= (int)(f->parts ? f->parts->tag_count : 0); t++)
    {
        int end = vn, next = vn;

        if (f->parts != NULL && t < (int)f->parts->tag_count)
        {
            const FPackPartTag *pt = &f->part_tags[t];

            if (fighter_part_on(f, t + 1, pt->joint))
                continue;
            end = pt->vert_first;
            next = pt->vert_first + pt->vert_count;
        }
        for (; i < end; i++)
        {
            const float *cp = f->vclip[i];
            float x;

            if (ortho)
            {
                x = (cp[3] != 0.0f) ? cp[2] / cp[3] : cp[2];
            }
            else
            {
                if (cp[3] <= 0.0f)
                    continue;
                x = 1.0f / cp[3];
            }
            if (!seen || x < *lo)
                *lo = x;
            if (!seen || x > *hi)
                *hi = x;
            seen = 1;
        }
        i = next;
    }
    return seen;
}

uint32_t fighter_draw_layered(Fighter *f, float depth)
{
    return fighter_draw_layered_band(f, depth, 1.0f / 512.0f);
}

uint32_t fighter_draw_layered_band(Fighter *f, float depth, float width)
{
    uint32_t n;
    float invw_min = 0.0f, invw_max = 0.0f;

    /* The model's own real 1/w spread this frame, at its current pose.
     * Replaces a fixed guess (1/w near 2e-4 at the menus' 5000-unit eye
     * distance, scaled 50x) that assumed every fighter's bind pose
     * spreads about the same: Link's reaches nearly double Kirby's, the
     * next-widest fighter's (his sheathed sword reaches far enough from
     * the body that a scale sized for him left every other fighter 3-6x
     * more margin than it needed, and still left Link's own shield and
     * sheath close enough to tie). */
    live_extent(f, 0, &invw_min, &invw_max);

    /* onto half a sprite step (lbcommon.h LB_SPRITE_Z_STEP is 1/256),
     * fighter_draw_layered_ortho's own budget below, for the same
     * reason: room left for the pass after this one.
     *
     * emit_tri computes gZBase + gZBias * gZScale * invw, so the SCALE
     * alone only fixes the WIDTH of the band: the model lands at
     * depth + invw_min / spread / 512, and that offset is unbounded --
     * it is half a sprite step for every multiple of its own depth
     * spread the model stands from the eye. A fighter three times as
     * deep as he is thick displaces by three halves of a step, one that
     * fills a tenth of his distance by five. Nothing notices while one
     * model is drawn between two sprite passes, which is what this draw
     * was written for and what every menu asks of it: the band may sit
     * anywhere above its slot as long as it is alone up there.
     *
     * The opening movie is where it stops being alone -- Clash poses
     * eight fighters at eight distances under one camera, each taking
     * the next slot and each displaced by a different multiple of it,
     * so the bands interleave and the fighters are drawn through each
     * other: Yoshi's and Kirby's polygons inside Samus's helmet, Link's
     * through Fox, every frame. Subtracting invw_min
     * anchors the band, so a model occupies exactly (depth, depth +
     * 1/512] whatever its distance or its thickness, and the slot it
     * was given is the slot it draws in.
     *
     * gZBias (the FPACK_NOZ nudge, draw_batches) multiplies the scaled
     * term and not the anchor, so a nudged batch still leaves the band
     * by the fraction of a step it means to; no fighter pack carries a
     * NOZ batch today and the stage packs that do are never drawn
     * layered. */
    {
        float spread = invw_max - invw_min;

        gZScale = (spread > 0.0f) ? width / spread : 50.0f;
    }
    gZBase = depth - gZScale * invw_min;
    gZBandLo = depth;
    gZBandHi = depth + width;
    gZBand = 1;
    /* THE CAMERA'S SCISSOR, which the PVR does not have. The RDP cut
     * every camera's drawing at its viewport; here an opaque model that
     * runs past its camera's viewport is hidden by whatever the frame
     * puts over it -- the border (taskman.c syTaskmanDrawBorder), or a
     * neighbouring camera's sprites and layered models. A layered model
     * IS that top layer, so nothing hides its own overrun: the posed
     * fighter of the per-fighter openings, zoomed past its 100-pixel
     * column, lay across the motion window beside it. The
     * viewport is the frustum's image, so the scissor is the frustum's
     * four side planes, and a triangle wholly inside the unit circle
     * of NDC is inside all four (clip.h's inradius). */
    gClipPlanes = kFrustumSidePlanes;
    gClipNPlanes = 4;
    gClipInradius = 1.0f;
    n = draw_batches(f, -1);
    gClipPlanes = NULL;
    gClipNPlanes = 0;
    gClipInradius = 0.0f;
    gZBand = 0;
    gZBase = 0.0f;
    gZScale = 1.0f;
    return n;
}

#define LAYERED_STACK_NUDGE 1e-4f

/* See fighter.h. */
float fighter_layered_ratio(const Fighter *f)
{
    float mn = 0.0f, mx = 0.0f;
    int nb = (int)f->hd->batch_count;

    if (!live_extent(f, 0, &mn, &mx) || mn <= 0.0f)
        return 0.0f;
    /* the model's own spread, and room for the batches' nudges on top */
    return (mx / mn) *
           (1.0f + LAYERED_STACK_NUDGE * (float)(nb > 0 ? nb : 1));
}

/* See fighter.h. */
uint32_t fighter_draw_layered_scaled(Fighter *f, float base)
{
    uint32_t n;
    float mn = 0.0f, mx = 0.0f;

    if (!live_extent(f, 0, &mn, &mx) || mn <= 0.0f)
        return 0;
    gZBatchNudge = LAYERED_STACK_NUDGE;
    gZScale = base / mn;
    gZBase = 0.0f;
    gZBand = 0;
    gClipPlanes = kFrustumSidePlanes;
    gClipNPlanes = 4;
    gClipInradius = 1.0f;
    n = draw_batches(f, -1);
    gClipPlanes = NULL;
    gClipNPlanes = 0;
    gClipInradius = 0.0f;
    gZBatchNudge = 0.0f;
    gZScale = 1.0f;
    return n;
}

/* See fighter.h. The band draw with the scale at zero, so emit_tri's
 * depth is gZBase alone, and draw_batches moving gZBase per batch. */
uint32_t fighter_draw_layered_paint(Fighter *f, float depth, float width)
{
    uint32_t n;

    gZScale = 0.0f;
    gZBase = depth;
    gZBandLo = depth;
    gZBandHi = depth + width;
    gZBand = 1;
    gZPaintStep = width / (float)(f->hd->batch_count ? f->hd->batch_count : 1);
    gClipPlanes = kFrustumSidePlanes;
    gClipNPlanes = 4;
    gClipInradius = 1.0f;
    n = draw_batches(f, -1);
    gClipPlanes = NULL;
    gClipNPlanes = 0;
    gClipInradius = 0.0f;
    gZPaintStep = 0.0f;
    gZBand = 0;
    gZBase = 0.0f;
    gZScale = 1.0f;
    return n;
}

uint32_t fighter_draw_layered_ortho(Fighter *f, float depth,
                                    const float (*planes)[3], int nplanes,
                                    float inradius)
{
    uint32_t n;
    float zmin = 0.0f, zmax = 0.0f;

    /* the model's own extent in clip z, which under an ortho camera is
     * affine in view depth */
    live_extent(f, 1, &zmin, &zmax);
    gZOrtho = 1;
    gZOrthoMax = zmax;
    /* onto half a sprite step (lbcommon.h LB_SPRITE_Z_STEP is 1/256):
     * the model spans (depth, depth + 1/512], under the next slot */
    gZOrthoScale = (zmax > zmin) ? (1.0f / 512.0f) / (zmax - zmin) : 0.0f;
    gZBase = depth;
    /* the same promise the perspective draw above makes, for the same
     * reason: this one is anchored to gZOrthoMax rather than displaced,
     * so only the clipper can put a vertex outside the slot */
    gZBandLo = depth;
    gZBandHi = depth + 1.0f / 512.0f;
    gZBand = 1;
    gClipPlanes = planes;
    gClipNPlanes = nplanes;
    gClipInradius = inradius;
    n = draw_batches(f, -1);
#ifdef DB_MAGNIFY_PROBE
    {
        static unsigned calls2;

        if ((calls2++ % 60) == 0)
            dbglog(DBG_INFO, "magnify: %.8s drew %u tris, base %g scale %g max %g\n",
                   f->hd->name, (unsigned)n, gZBase, gZOrthoScale, gZOrthoMax);
    }
#endif
    gZBand = 0;
    gClipPlanes = NULL;
    gClipNPlanes = 0;
    gClipInradius = 0.0f;
    gZBase = 0.0f;
    gZOrtho = 0;
    return n;
}
#endif /* !SSB_NO_DRAW */
