/* stage.c -- see stage.h. The container is 16 bytes of header, the model
 * pack (loaded through fighter_init, which is what draws it too), then the
 * little-endian STG6 block: a 100-byte header, then MPLineInfo[],
 * MPVertexLinks[], u16 vertex_id[], MPVertexData[], MPMapObjData[]
 * back to back (the layout tools/export/ssb_stageexport.py writes). */
#include <kos.h>

#include "dcpvr.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include <ef/effect.h>            /* efGroundMakeAppearActor */
#include <gr/grtypes.h>           /* GRStruct, the per-stage ground vars */
#include <gr/grcommon/grhyrule.h> /* grHyruleMakeGround */
#include <gr/grcommon/grjungle.h> /* grJungleMakeGround */
#include <gr/grcommon/grzebes.h>  /* grZebesMakeGround */
#include <gr/grcommon/gryoster.h> /* grYosterMakeGround */
#include <gr/grcommon/grpupupu.h> /* grPupupuMakeGround */
#include <gr/grcommon/grinishie.h> /* grInishieMakeGround */
#include <gr/grcommon/grcastle.h> /* grCastleMakeGround */
#include <gr/grcommon/gryamabuki.h> /* grYamabukiMakeGround */
#include <gr/grcommon/grsector.h> /* grSectorMakeGround */
#include <gr/grbonus/grbonus3.h>  /* grBonus3MakeGround */
#include "sc1pbonusstage.h"       /* sc1PBonusStageMakeBonus1Ground */

/* The port's, not the decomp's: gr/grcommon/grsector.h is the decomp's
 * header and this function has no decomp counterpart (see grOverlayLoad).
 * src/dc/ftshadow.c's ftShadowOverlayLoad is the same shape. */
extern void grSectorOverlayLoad(void);
#include <sys/debug.h>            /* syDebugPrintf */
#include <sys/objanim.h>          /* gcPlayAnimAll, gcAddAnimJointAll */
#include <sys/objdisplay.h>       /* gcDrawDObjTreeForGObj */
#include <sys/objman.h>           /* gcAddGObjProcess */
#include "scmanager.h"            /* gSCManagerBattleState->gkind */

#include "assetroot.h"
#include "bgm.h"
#include "mpcommon.h"
#include "stage.h"
#include "lbcommon.h"             /* LB_Z_WALLPAPER */
#include "grwallpaper.h"
#include "sc1pgameboss.h"    /* the nGRKindLast arm of grWallpaperMakeDecideKind */
#include "sc1ptrainingmode.h" /* its training arm */
#include "overlay.h"

typedef struct
{
    char magic[8];              /* "SSBSTAG1" */
    uint32_t pack_size;
    uint32_t extra_off;
} StgHeader;

typedef struct
{
    char magic[4];              /* "STG6" */
    uint16_t yakumono_count;    /* MPGeometryData.yakumono_count */
    uint16_t line_count;        /* sum of every MPLineData.line_count */
    uint16_t vertex_id_count;
    uint16_t vertex_count;
    uint16_t mapobj_count;      /* MPGeometryData.mapobj_count */
    /* Where the COLLISION layer landed among the merged joints. The game
     * gives layer 1 its own GObj and hands gcSetupCustomDObjs the
     * yakumono array as its out-parameter, so yakumono id k IS layer-1
     * joint k (objanim.c:2363-2366); the port merges the layers, so it
     * needs to be told where layer 1 starts. These two were pad0/pad1 --
     * the block is still 68 bytes and still STG4, and a stage built
     * before them reads 0/0, which means "no layer 1" and leaves
     * mpcommon.c's stand-in DObjs in place. */
    uint16_t layer1_jbase;
    int16_t cam[4];             /* top, bottom, right, left */
    int16_t map[4];
    int16_t alt_warning;
    uint16_t layer1_joints;
    uint32_t bgm;
    uint32_t fog_rgb;
    float light_angle[3];       /* MPGroundData.light_angle */
    uint8_t emblem[4][3];       /* MPGroundData.emblem_colors */
    /* STG5: MPGroundData's 1P-game team bounds and the bonus pause's
     * zoom pair (mp/mptypes.h:201-210), which the team rungs' camera and
     * blast line read */
    int16_t cam_team[4];        /* top, bottom, right, left */
    int16_t map_team[4];
    int16_t zoom_start[3];
    int16_t zoom_end[3];
    /* STG6: emblem_colors[4], the CPU's -- see stage.h */
    uint8_t emblem_cp[3];
    uint8_t pad;
} StgExtra;

/* the exporter writes exactly this much before the tables */
typedef char stg_extra_size_check[sizeof(StgExtra) == 100 ? 1 : -1];
/* the CPU's emblem colour is read one past emblem_colors, so `unused`
 * has to follow the array with no gap, as it does on the N64 */
typedef char mp_emblem_unused_check[
    offsetof(MPGroundData, unused) ==
    offsetof(MPGroundData, emblem_colors) + 4 * sizeof(SYColorRGB) ? 1 : -1];
typedef char mp_line_info_size_check[sizeof(MPLineInfo) == 18 ? 1 : -1];
typedef char mp_vertex_data_size_check[sizeof(MPVertexData) == 6 ? 1 : -1];

/* The .stg's bytes, without a copy where there need not be one.
 *
 * A stage is pointed into rather than parsed out: the collision tables,
 * the model's vertices and the wallpaper's texels are read where they
 * lie (see the file header), so the blob stays for the life of the
 * stage. On the romdisk it is already in RAM -- it is part of the ELF --
 * and fs_mmap hands back a pointer straight into it, so the malloc-and-
 * read that every other loader does would keep a second copy of every
 * stage for nothing. With four stages resident that second copy is 1.2
 * MB on a 16 MB machine, which is what made the fourth one fail to load.
 *
 * Anything the mapping cannot answer -- a filesystem with no mmap, an
 * alignment the SH-4 cannot fetch floats from -- falls back to the copy.
 * `owned` says which happened, so a free path can tell them apart; there
 * is no free path yet, because stages load once and stay until a
 * lifetime is defined for them. */
static void *stage_bytes(const char *name, long *size_out, int *owned)
{
    /* a name, joined with whichever medium this build reads from, and
     * read through it (src/dc/assetroot.h) */
    AssetFile af;
    void *map;
    void *copy;
    long size;

    if (asset_open(&af, name) < 0)
    {
        dbglog(DBG_ERROR, "stage: cannot open %s\n", name);
        return NULL;
    }
    size = asset_size(&af);
    map = asset_mmap(&af);

    if (map != NULL && ((uintptr_t)map & 3) == 0)
    {
        *size_out = size;
        *owned = 0;
        asset_close(&af);       /* romdisk keeps the mapping; it is the ELF */
        return map;
    }
    /* the loader's copy when it read the stage ahead (assetroot.h) */
    copy = asset_read_all(&af);
    if (copy == NULL)
    {
        dbglog(DBG_ERROR, "stage: short read on %s (%ld bytes)\n",
               name, size);
        asset_close(&af);
        return NULL;
    }
    asset_close(&af);
    *size_out = size;
    *owned = 1;
    return copy;
}

/* The optional MPK1 block (stage.h): the stage's map file, as the port
 * carries it -- the map object's tree baked, its AnimJoint scripts as
 * words, and MPItemWeights.
 *
 * The exporter writes it and this reads it back; the two field lists are
 * the same list. What the loader has to do that a plain read does not is
 * hand out the two things the pack cannot hold as bytes: a DObj tree
 * (dc_model_add_dobjs, at scene time -- the tree's DObjs come off the
 * object system's own pool, which the stage's four visual layers already
 * wait on, so this is deferred to stage_bind_map below) and the scripts'
 * one self-referential pointer, written once here into a copy, because
 * the pack's bytes are the romdisk's.
 */
static int stage_load_map(Stage *st, const uint8_t *p, long size)
{
    uint32_t pack_size, anim_count, fixup_count, weight_count;
    uint32_t attack_count, graft_size, object_count;
    uint32_t off_pack, off_anims, off_fixups, off_weights, off_attack;
    uint32_t off_graft, off_objects;
    const uint8_t *end = (const uint8_t *)st->blob + size;
    uint32_t i;
    uint32_t words = 0;             /* total script words, for the copy */

    if (end - p < 60 || memcmp(p, "MPK1", 4) != 0)
    {
        return 0;                   /* no map file in this pack */
    }
    memcpy(&pack_size, p + 4, 4);
    memcpy(&anim_count, p + 8, 4);
    memcpy(&fixup_count, p + 12, 4);
    memcpy(&weight_count, p + 16, 4);
    memcpy(&attack_count, p + 20, 4);
    memcpy(&graft_size, p + 24, 4);
    memcpy(&object_count, p + 28, 4);
    memcpy(&off_pack, p + 32, 4);
    memcpy(&off_anims, p + 36, 4);
    memcpy(&off_fixups, p + 40, 4);
    memcpy(&off_weights, p + 44, 4);
    memcpy(&off_attack, p + 48, 4);
    memcpy(&off_graft, p + 52, 4);
    memcpy(&off_objects, p + 56, 4);

    if ((uintptr_t)(p + off_weights) > (uintptr_t)end ||
        anim_count > STAGE_MAP_ANIMS_MAX ||
        object_count > STAGE_MAP_OBJECTS_MAX ||
        weight_count > STAGE_ITEM_WEIGHTS_COUNT ||
        attack_count > STAGE_ATTACK_COLL_S32S)
    {
        dbglog(DBG_ERROR, "stage: %s: bad MPK1 block\n", st->model.hd->name);
        return -1;
    }
    {
        uint32_t at = off_anims;    /* each record: u32 count, char
                                       name[16], then the words */

        for (i = 0; i < anim_count; i++)
        {
            uint32_t n;
            memcpy(&n, p + at, 4);
            at += 40 + n * 4;
            words += n;
        }
        if ((uintptr_t)(p + at) > (uintptr_t)end ||
            (uintptr_t)(p + off_fixups + fixup_count * 12) >
                (uintptr_t)end ||
            (uintptr_t)(p + off_objects + object_count * 8) >
                (uintptr_t)end ||
            (uintptr_t)(p + off_pack + pack_size) > (uintptr_t)end)
        {
            dbglog(DBG_ERROR, "stage: %s: MPK1 sections run past the pack\n",
                   st->model.hd->name);
            return -1;
        }
    }
    st->map_owned = malloc(anim_count * sizeof(AObjEvent32 *) + words * 4 +
                           weight_count +
                           ((attack_count != 0) ? sizeof(GRAttackColl) : 0));
    if (st->map_owned == NULL)
    {
        dbglog(DBG_ERROR, "stage: %s: map file alloc failed\n",
               st->model.hd->name);
        return -1;
    }
    {
        uint32_t *dst = (uint32_t *)((uint8_t *)st->map_owned +
                                     anim_count * sizeof(AObjEvent32 *));
        const uint8_t *src = p + off_anims;

        for (i = 0; i < anim_count; i++)
        {
            uint32_t n, k, joint;
            memcpy(&n, src, 4);
            memcpy(&joint, src + 4, 4);
            st->map_anim_names[i] = (const char *)(src + 8);
            st->map_anims[i] = (AObjEvent32 *)dst;
            st->map_anim_joints[i] = (int)joint;
            for (k = 0; k < n; k++)
            {
                memcpy(&dst[k], src + 40 + k * 4, 4);
            }
            dst += n;
            src += 40 + n * 4;
        }
        /* the scripts' relocated pointers (see the exporter): written
         * last, so a script that names another one finds the other's
         * copy already addressed. Every one of these is SetAnim's own
         * word, the loop back to the script's first command. */
        for (i = 0; i < fixup_count; i++)
        {
            uint32_t a, w, t;
            memcpy(&a, p + off_fixups + i * 12, 4);
            memcpy(&w, p + off_fixups + i * 12 + 4, 4);
            memcpy(&t, p + off_fixups + i * 12 + 8, 4);
            if (a >= anim_count || t >= anim_count)
            {
                dbglog(DBG_ERROR, "stage: %s: fixup %u out of range\n",
                       st->model.hd->name, i);
                return -1;
            }
            ((uint32_t *)st->map_anims[a])[w] = (uint32_t)(uintptr_t)
                st->map_anims[t];
        }
        if (weight_count != 0)
        {
            uint8_t *w = (uint8_t *)dst;
            memcpy(w, p + off_weights, weight_count);
            st->item_weights = w;
        }
    }
    if (attack_count != 0)
    {
        s32 v[STAGE_ATTACK_COLL_S32S];

        if ((uintptr_t)(p + off_attack + attack_count * 4) >
                (uintptr_t)end ||
            attack_count != STAGE_ATTACK_COLL_S32S)
        {
            dbglog(DBG_ERROR, "stage: %s: bad MPK1 attack coll\n",
                   st->model.hd->name);
            return -1;
        }
        memcpy(v, p + off_attack, sizeof(v));
        st->attack_coll = (GRAttackColl *)((uint8_t *)st->map_owned +
                                           anim_count *
                                               sizeof(AObjEvent32 *) +
                                           words * 4 + weight_count);
        memcpy(st->attack_coll, v, sizeof(v));
    }
    st->map_anim_count = (int)anim_count;

    /* The map objects: one baked pack each, and the objects table names
     * where each one sits inside the pack area. Every stage but Dream
     * Land has exactly one; Dream Land's own file builds four GObojs off
     * four DObjDesc blocks of this one map file, and stage_bind_map_object
     * is what picks one. */
    for (i = 0; i < object_count; i++)
    {
        uint32_t oo, os;

        memcpy(&oo, p + off_objects + i * 8, 4);
        memcpy(&os, p + off_objects + i * 8 + 4, 4);

        if (os == 0 || oo + os > pack_size)
        {
            dbglog(DBG_ERROR, "stage: %s: bad MPK1 object %u\n",
                   st->model.hd->name, i);
            return -1;
        }
        if (fighter_init(&st->map_models[i], (uint8_t *)p + off_pack + oo,
                         (long)os, &st->map_pal_banks[i]) < 0)
        {
            dbglog(DBG_ERROR, "stage: %s: MAP OBJECT %u would not load\n",
                   st->model.hd->name, i);
            return -1;
        }
    }
    st->map_object_count = (int)object_count;
    /* The graft: the object a stage's own code builds at runtime off a
     * bare map-file display list rather than off a DObjDesc of its own
     * (Yoshi's Island's clouds). 0/0 for a stage that has none. */
    if (graft_size != 0 &&
        fighter_init(&st->graft_model, (uint8_t *)p + off_graft, graft_size,
                     &st->graft_pal_bank) < 0)
    {
        return -1;
    }
    return 0;
}

/* The optional GRA1 block (stage.h): the stage's ground actors -- one or
 * more baked packs, one EFGroundDesc-equivalent entry per spawnable
 * variant, and the EFGroundParam weight table that picks among them, all
 * written by tools/export/ssb_stageexport.py right after the (optional) WLP1
 * block and before MPK1. Absent for Hyrule, which ef/efground.c gives no
 * table at all.
 *
 * The header is 56 bytes, all u32 after the magic: model_count,
 * desc_count, param_count, anim_count, fixup_count, off_pack, pack_size,
 * off_models, off_descs, off_params, off_anims, off_fixups, block_size
 * (this whole block's length, so a caller with no use for any of it can
 * still skip it). Records:
 *   - a model is (u32 offset, u32 size) into the off_pack region, one
 *     fighter_init call each -- off_objects' own shape in MPK1.
 *   - a desc is 60 bytes: f32 alt_high, alt_low, pos_z, scale; u16
 *     effect_status, update_kind (0 efGroundCommonProcUpdate, 1
 *     efGroundUpdateEffectYaw, 2 efGroundUpdateStepPositions -- which
 *     three-function switch efground.c's own EFGroundDesc.effect_desc.
 *     proc_update picks between); u32 model_index; u8 dl_link,
 *     setup_kind (0 none, 1 efGroundSetStepPositions -- which two-value
 *     switch EFGroundDesc.proc_groundeffect picks between: the hook
 *     efGroundUpdatePhysics runs once at spawn, NULL for every desc in
 *     the game but Sector Z's first rocket), matanim_alt (which of the
 *     pack's MatAnimJoints this desc plays -- 0 for all but Dream Land's
 *     right-facing Bronto, whose pack carries two), billboard (bit k:
 *     the pack's joint k is a billboard joint, its DObjDesc id's 0xF000
 *     -- efGroundSetupEffectDObjs's flag; STAGE_GROUND_JOINTS_MAX is 8);
 *     char anim_name[STAGE_MAP_ANIM_NAME_MAX] (empty for none).
 *   - a param is 12 bytes: u16 effect_id, make_queue; s32 lr; u8
 *     effect_weight, pad[3] -- EFGroundParam's own fields.
 *   - the anim table is MPK1's off_anims/off_fixups shape exactly (40-
 *     byte header + n words per script, named, with the same SetAnim
 *     self-reference fixups) -- a desc's anim_name is one of these
 *     names, expanded once here into ground_anim_tables' per-joint row.
 */
static int stage_load_ground(Stage *st, const uint8_t *p, const uint8_t *end,
                             uint32_t *consumed)
{
    uint32_t model_count, desc_count, param_count, anim_count, fixup_count;
    uint32_t off_pack, pack_size, off_models, off_descs, off_params;
    uint32_t off_anims, off_fixups, block_size;
    uint32_t i, words;
    /* the named script pool, local: only its expansion into
     * ground_anim_tables (per desc, per joint) needs to survive this
     * call */
    enum { GRA_ANIMS_LOCAL_MAX = 16 };
    AObjEvent32 *anim_scripts[GRA_ANIMS_LOCAL_MAX];
    const char *anim_names[GRA_ANIMS_LOCAL_MAX];
    int anim_joint_idx[GRA_ANIMS_LOCAL_MAX];

    *consumed = 0;

    if (end - p < 56 || memcmp(p, "GRA1", 4) != 0)
    {
        return 0;                   /* no ground actors in this pack */
    }
    memcpy(&model_count, p + 4, 4);
    memcpy(&desc_count, p + 8, 4);
    memcpy(&param_count, p + 12, 4);
    memcpy(&anim_count, p + 16, 4);
    memcpy(&fixup_count, p + 20, 4);
    memcpy(&off_pack, p + 24, 4);
    memcpy(&pack_size, p + 28, 4);
    memcpy(&off_models, p + 32, 4);
    memcpy(&off_descs, p + 36, 4);
    memcpy(&off_params, p + 40, 4);
    memcpy(&off_anims, p + 44, 4);
    memcpy(&off_fixups, p + 48, 4);
    memcpy(&block_size, p + 52, 4);

    if (model_count > STAGE_GROUND_MODELS_MAX ||
        desc_count > STAGE_GROUND_DESCS_MAX ||
        param_count > STAGE_GROUND_PARAMS_MAX ||
        anim_count > GRA_ANIMS_LOCAL_MAX ||
        (uintptr_t)(p + off_pack + pack_size) > (uintptr_t)end ||
        (uintptr_t)(p + off_models + model_count * 8) > (uintptr_t)end ||
        (uintptr_t)(p + off_descs + desc_count * 60) > (uintptr_t)end ||
        (uintptr_t)(p + off_params + param_count * 12) > (uintptr_t)end ||
        (uintptr_t)(p + off_fixups + fixup_count * 12) > (uintptr_t)end ||
        (uintptr_t)(p + block_size) > (uintptr_t)end)
    {
        dbglog(DBG_ERROR, "stage: %s: bad GRA1 block\n", st->model.hd->name);
        return -1;
    }
    {
        uint32_t at = off_anims;

        words = 0;
        for (i = 0; i < anim_count; i++)
        {
            uint32_t n;
            memcpy(&n, p + at, 4);
            at += 40 + n * 4;
            words += n;
        }
        if ((uintptr_t)(p + at) > (uintptr_t)end)
        {
            dbglog(DBG_ERROR, "stage: %s: GRA1 anims run past the pack\n",
                   st->model.hd->name);
            return -1;
        }
    }
    st->ground_owned = malloc(anim_count * sizeof(AObjEvent32 *) + words * 4);
    if (st->ground_owned == NULL && (anim_count != 0 || words != 0))
    {
        dbglog(DBG_ERROR, "stage: %s: ground actor script alloc failed\n",
               st->model.hd->name);
        return -1;
    }
    {
        uint32_t *dst = (uint32_t *)((uint8_t *)st->ground_owned +
                                     anim_count * sizeof(AObjEvent32 *));
        const uint8_t *src = p + off_anims;

        for (i = 0; i < anim_count; i++)
        {
            uint32_t n, k, joint;

            memcpy(&n, src, 4);
            memcpy(&joint, src + 4, 4);
            anim_names[i] = (const char *)(src + 8);
            anim_scripts[i] = (AObjEvent32 *)dst;
            anim_joint_idx[i] = (int)joint;
            for (k = 0; k < n; k++)
            {
                memcpy(&dst[k], src + 40 + k * 4, 4);
            }
            dst += n;
            src += 40 + n * 4;
        }
        for (i = 0; i < fixup_count; i++)
        {
            uint32_t a, w, t;

            memcpy(&a, p + off_fixups + i * 12, 4);
            memcpy(&w, p + off_fixups + i * 12 + 4, 4);
            memcpy(&t, p + off_fixups + i * 12 + 8, 4);
            if (a >= anim_count || t >= anim_count)
            {
                dbglog(DBG_ERROR, "stage: %s: GRA1 fixup %u out of range\n",
                       st->model.hd->name, i);
                return -1;
            }
            ((uint32_t *)anim_scripts[a])[w] = (uint32_t)(uintptr_t)
                anim_scripts[t];
        }
    }
    for (i = 0; i < model_count; i++)
    {
        uint32_t oo, os;

        memcpy(&oo, p + off_models + i * 8, 4);
        memcpy(&os, p + off_models + i * 8 + 4, 4);
        if (os == 0 || oo + os > pack_size)
        {
            dbglog(DBG_ERROR, "stage: %s: bad GRA1 model %u\n",
                   st->model.hd->name, i);
            return -1;
        }
        if (fighter_init(&st->ground_models[i], (uint8_t *)p + off_pack + oo,
                         (long)os, &st->ground_pal_banks[i]) < 0)
        {
            dbglog(DBG_ERROR, "stage: %s: GROUND ACTOR %u would not load\n",
                   st->model.hd->name, i);
            return -1;
        }
    }
    st->ground_model_count = (int)model_count;

    for (i = 0; i < param_count; i++)
    {
        uint16_t effect_id, make_queue;
        int32_t lr;
        uint8_t effect_weight;
        const uint8_t *r = p + off_params + i * 12;

        memcpy(&effect_id, r, 2);
        memcpy(&make_queue, r + 2, 2);
        memcpy(&lr, r + 4, 4);
        effect_weight = r[8];

        st->ground_params[i].effect_id = effect_id;
        st->ground_params[i].make_queue = make_queue;
        st->ground_params[i].lr = lr;
        st->ground_params[i].effect_weight = effect_weight;
    }
    st->ground_param_count = (int)param_count;

    for (i = 0; i < desc_count; i++)
    {
        float alt_high, alt_low, pos_z, scale;
        uint16_t effect_status, update_kind;
        uint32_t model_index;
        uint8_t dl_link, setup_kind, matanim_alt, billboard;
        const char *anim_name;
        const uint8_t *r = p + off_descs + i * 60;
        int j;

        memcpy(&alt_high, r, 4);
        memcpy(&alt_low, r + 4, 4);
        memcpy(&pos_z, r + 8, 4);
        memcpy(&scale, r + 12, 4);
        memcpy(&effect_status, r + 16, 2);
        memcpy(&update_kind, r + 18, 2);
        memcpy(&model_index, r + 20, 4);
        dl_link = r[24];
        setup_kind = r[25];
        matanim_alt = r[26];
        billboard = r[27];
        anim_name = (const char *)(r + 28);

        if (model_index >= model_count)
        {
            dbglog(DBG_ERROR, "stage: %s: GRA1 desc %u model %u out of "
                              "range\n",
                   st->model.hd->name, i, model_index);
            return -1;
        }
        st->ground_descs[i].alt_high = alt_high;
        st->ground_descs[i].alt_low = alt_low;
        st->ground_descs[i].pos_z = pos_z;
        st->ground_descs[i].scale = scale;
        st->ground_descs[i].effect_status = effect_status;
        /* efGroundUpdatePhysics runs this once, right after it has put
         * the actor at its spawn edge; Sector Z's growing rocket is the
         * only desc in the game that asks for one. */
        st->ground_descs[i].proc_groundeffect =
            (setup_kind == 1) ? efGroundSetStepPositions : NULL;
        st->ground_descs[i].effect_desc.flags = 0;
        st->ground_descs[i].effect_desc.dl_link = dl_link;
        st->ground_descs[i].effect_desc.file_head = NULL;
        st->ground_descs[i].effect_desc.transform_types1.tk1 = 0;
        st->ground_descs[i].effect_desc.transform_types1.tk2 = 0;
        st->ground_descs[i].effect_desc.transform_types1.tk3 = 0;
        st->ground_descs[i].effect_desc.transform_types2.tk1 = 0;
        st->ground_descs[i].effect_desc.transform_types2.tk2 = 0;
        st->ground_descs[i].effect_desc.transform_types2.tk3 = 0;
        switch (update_kind)
        {
        case 1:
            st->ground_descs[i].effect_desc.proc_update = efGroundUpdateEffectYaw;
            break;
        case 2:
            st->ground_descs[i].effect_desc.proc_update = efGroundUpdateStepPositions;
            break;
        default:
            st->ground_descs[i].effect_desc.proc_update = efGroundCommonProcUpdate;
            break;
        }
        st->ground_descs[i].effect_desc.proc_display = gcDrawDObjTreeForGObj;
        st->ground_descs[i].effect_desc.o_dobjsetup = 0;
        st->ground_descs[i].effect_desc.o_mobjsub = 0;
        st->ground_descs[i].effect_desc.o_anim_joint = 0;
        st->ground_descs[i].effect_desc.o_matanim_joint = 0;

        st->ground_assets[i].pack = &st->ground_models[model_index];
        /* the material scripts ride in the pack, not here -- see
         * EFGroundActorAsset in src/dc/efground.h */
        st->ground_assets[i].matanim_joint = NULL;
        st->ground_assets[i].matanim_alt = (int)matanim_alt;
        st->ground_assets[i].billboard = billboard;

        for (j = 0; j < STAGE_GROUND_JOINTS_MAX; j++)
        {
            st->ground_anim_tables[i][j] = NULL;
        }
        if (anim_name[0] != '\0')
        {
            uint32_t k;

            for (k = 0; k < anim_count; k++)
            {
                if (strcmp(anim_names[k], anim_name) == 0 &&
                    anim_joint_idx[k] >= 0 &&
                    anim_joint_idx[k] < STAGE_GROUND_JOINTS_MAX)
                {
                    st->ground_anim_tables[i][anim_joint_idx[k]] = anim_scripts[k];
                }
            }
            st->ground_assets[i].anim_joint = st->ground_anim_tables[i];
        }
        else st->ground_assets[i].anim_joint = NULL;
    }
    st->ground_desc_count = (int)desc_count;

    *consumed = block_size;
    return 0;
}

/* The optional BWP1 block (stage.h): the trees sc/sc1pmode/
 * sc1pgameboss.c instances Final Destination's animated background
 * from, written by tools/export/ssb_stageexport.py right after GRA1 and before
 * MPK1. Absent for every stage but Final Destination.
 *
 * The header is 40 bytes, all u32 after the magic: model_count,
 * anim_count, fixup_count, off_pack, pack_size, off_models, off_anims,
 * off_fixups, block_size (this whole block's length, so a caller with no
 * use for any of it can still skip it).
 *
 * This is GRA1's models + anims and nothing else. There is no desc array
 * because a boss effect has no EFGroundDesc: the plans, spawn counts,
 * speeds and damage-driven row swaps are sc1pgameboss.c's own .data,
 * which the port compiles rather than exports. Records:
 *   - a model is 56 bytes: u32 offset, size (into the off_pack region,
 *     one fighter_init call each -- GRA1's own two fields), anim_first,
 *     anim_count (this pack's run of the shared anim table, rather than
 *     GRA1's name-per-desc lookup: a boss pack's AnimJoint is its OWN,
 *     never shared with another pack), billboard (bit k: pack joint k is
 *     a billboard joint; STAGE_BOSS_JOINTS_MAX is 16 because two of the
 *     five trees are nine joints), matanim_count (how many MatAnimJoints
 *     the pack carries as alts); char name[STAGE_MAP_ANIM_NAME_MAX].
 *   - the anim table is GRA1's off_anims/off_fixups shape exactly (40-
 *     byte header of word count, joint and name, then the words, with
 *     the same SetAnim self-reference fixups).
 */
/* Where one of the two tree-pack blocks lands in a Stage. BWP1's boss
 * effects and BPL1's platforms are the same block -- see stage.h -- so
 * they are read by one body and told apart only by which of these it is
 * handed. */
typedef struct
{
    const char *magic;
    Fighter *models;
    int *pal_banks;
    char (*names)[STAGE_MAP_ANIM_NAME_MAX];
    uint16_t *billboard;
    uint8_t *matanim_count;
    AObjEvent32 *(*anim_tables)[STAGE_BOSS_JOINTS_MAX];
    int *model_count;
    void **owned;
    uint32_t models_max;
    const char *what;
} StageTreePackDst;

static int stage_load_packs(Stage *st, const uint8_t *p, const uint8_t *end,
                            uint32_t *consumed,
                            const StageTreePackDst *dst)
{
    uint32_t model_count, anim_count, fixup_count;
    uint32_t off_pack, pack_size, off_models, off_anims, off_fixups;
    uint32_t block_size;
    uint32_t i, words;
    /* the script pool, local: only its expansion into boss_anim_tables
     * (per pack, per joint) needs to survive this call */
    AObjEvent32 *anim_scripts[STAGE_BOSS_ANIMS_MAX];
    int anim_joint_idx[STAGE_BOSS_ANIMS_MAX];

    *consumed = 0;

    if (end - p < 40 || memcmp(p, dst->magic, 4) != 0)
    {
        return 0;                   /* no boss wallpaper in this pack */
    }
    memcpy(&model_count, p + 4, 4);
    memcpy(&anim_count, p + 8, 4);
    memcpy(&fixup_count, p + 12, 4);
    memcpy(&off_pack, p + 16, 4);
    memcpy(&pack_size, p + 20, 4);
    memcpy(&off_models, p + 24, 4);
    memcpy(&off_anims, p + 28, 4);
    memcpy(&off_fixups, p + 32, 4);
    memcpy(&block_size, p + 36, 4);

    if (model_count > dst->models_max ||
        anim_count > STAGE_BOSS_ANIMS_MAX ||
        (uintptr_t)(p + off_pack + pack_size) > (uintptr_t)end ||
        (uintptr_t)(p + off_models + model_count * 56) > (uintptr_t)end ||
        (uintptr_t)(p + off_fixups + fixup_count * 12) > (uintptr_t)end ||
        (uintptr_t)(p + block_size) > (uintptr_t)end)
    {
        dbglog(DBG_ERROR, "stage: %s: bad %s block\n",
               st->model.hd->name, dst->magic);
        return -1;
    }
    {
        uint32_t at = off_anims;

        words = 0;
        for (i = 0; i < anim_count; i++)
        {
            uint32_t n;
            memcpy(&n, p + at, 4);
            at += 40 + n * 4;
            words += n;
        }
        if ((uintptr_t)(p + at) > (uintptr_t)end)
        {
            dbglog(DBG_ERROR, "stage: %s: %s anims run past the pack\n",
                   st->model.hd->name, dst->magic);
            return -1;
        }
    }
    *dst->owned = malloc(anim_count * sizeof(AObjEvent32 *) + words * 4);
    if (*dst->owned == NULL && (anim_count != 0 || words != 0))
    {
        dbglog(DBG_ERROR, "stage: %s: %s script alloc failed\n",
               st->model.hd->name, dst->what);
        return -1;
    }
    {
        uint32_t *w = (uint32_t *)((uint8_t *)*dst->owned +
                                   anim_count * sizeof(AObjEvent32 *));
        const uint8_t *src = p + off_anims;

        for (i = 0; i < anim_count; i++)
        {
            uint32_t n, k, joint;

            memcpy(&n, src, 4);
            memcpy(&joint, src + 4, 4);
            anim_scripts[i] = (AObjEvent32 *)w;
            anim_joint_idx[i] = (int)joint;
            for (k = 0; k < n; k++)
            {
                memcpy(&w[k], src + 40 + k * 4, 4);
            }
            w += n;
            src += 40 + n * 4;
        }
        for (i = 0; i < fixup_count; i++)
        {
            uint32_t a, fw, t;

            memcpy(&a, p + off_fixups + i * 12, 4);
            memcpy(&fw, p + off_fixups + i * 12 + 4, 4);
            memcpy(&t, p + off_fixups + i * 12 + 8, 4);
            if (a >= anim_count || t >= anim_count)
            {
                dbglog(DBG_ERROR, "stage: %s: %s fixup %u out of range\n",
                       st->model.hd->name, dst->magic, (unsigned)i);
                return -1;
            }
            ((uint32_t *)anim_scripts[a])[fw] = (uint32_t)(uintptr_t)
                anim_scripts[t];
        }
    }
    for (i = 0; i < model_count; i++)
    {
        uint32_t oo, os, first, n, billboard, alts;
        const uint8_t *r = p + off_models + i * 56;
        uint32_t k;
        int j;

        memcpy(&oo, r, 4);
        memcpy(&os, r + 4, 4);
        memcpy(&first, r + 8, 4);
        memcpy(&n, r + 12, 4);
        memcpy(&billboard, r + 16, 4);
        memcpy(&alts, r + 20, 4);
        if (os == 0 || oo + os > pack_size ||
            first + n > anim_count)
        {
            dbglog(DBG_ERROR, "stage: %s: bad %s model %u\n",
                   st->model.hd->name, dst->magic, (unsigned)i);
            return -1;
        }
        if (fighter_init(&dst->models[i], (uint8_t *)p + off_pack + oo,
                         (long)os, &dst->pal_banks[i]) < 0)
        {
            dbglog(DBG_ERROR, "stage: %s: %s %u would not load\n",
                   st->model.hd->name, dst->what, (unsigned)i);
            return -1;
        }
        memcpy(dst->names[i], r + 24, STAGE_MAP_ANIM_NAME_MAX);
        dst->names[i][STAGE_MAP_ANIM_NAME_MAX - 1] = '\0';
        dst->billboard[i] = (uint16_t)billboard;
        dst->matanim_count[i] = (uint8_t)alts;

        for (j = 0; j < STAGE_BOSS_JOINTS_MAX; j++)
        {
            dst->anim_tables[i][j] = NULL;
        }
        for (k = first; k < first + n; k++)
        {
            if (anim_joint_idx[k] < 0 ||
                anim_joint_idx[k] >= STAGE_BOSS_JOINTS_MAX)
            {
                dbglog(DBG_ERROR, "stage: %s: %s model %u script %u is "
                                  "on joint %d\n",
                       st->model.hd->name, dst->magic, (unsigned)i,
                       (unsigned)k, anim_joint_idx[k]);
                return -1;
            }
            dst->anim_tables[i][anim_joint_idx[k]] = anim_scripts[k];
        }
    }
    *dst->model_count = (int)model_count;

    dbglog(DBG_INFO, "stage: %s: %s: %u pack(s), %u anim(s)\n",
           st->model.hd->name, dst->what, (unsigned)model_count,
           (unsigned)anim_count);

    *consumed = block_size;
    return 0;
}

/* Where one of the two optional placement blocks lands in a Stage.
 * BTG1's targets and BMP1's bumpers are the same block one magic apart
 * -- see stage.h -- so they are read by one body and told apart only by
 * which of these it is handed. */
typedef struct
{
    const char *magic;
    Vec3f *pos;
    int8_t *anim;
    uint8_t *count;
    uint8_t *anim_count;
    AObjEvent32 **anims;
    void **owned;
    uint32_t pos_max;
    uint32_t anims_max;
    const char *what;
} StagePlacementDst;

/* See stage.h: BWP1, the block Final Destination's .stg carries. */
static int stage_load_boss(Stage *st, const uint8_t *p, const uint8_t *end,
                           uint32_t *consumed)
{
    StageTreePackDst dst;

    dst.magic = "BWP1";
    dst.models = st->boss_models;
    dst.pal_banks = st->boss_pal_banks;
    dst.names = st->boss_names;
    dst.billboard = st->boss_billboard;
    dst.matanim_count = st->boss_matanim_count;
    dst.anim_tables = st->boss_anim_tables;
    dst.model_count = &st->boss_model_count;
    dst.owned = &st->boss_owned;
    dst.models_max = STAGE_BOSS_MODELS_MAX;
    dst.what = "boss wallpaper";

    return stage_load_packs(st, p, end, consumed, &dst);
}

/* The optional BTG1 and BMP1 blocks (stage.h): a bonus course's item
 * placements and the scripts the moving ones follow. THE TWO PARSERS
 * MOVE TOGETHER: this one and bonus_placements_section in
 * tools/export/ssb_stageexport.py.
 *
 * Simpler than stage_load_boss above because there is nothing to bake:
 * neither block carries a pack, since a target's and a bumper's model
 * belong to the item and not to the course. What is left is the
 * placements, the scripts and the same fixup pass the other two blocks
 * use.
 */
static int stage_load_placements(Stage *st, const uint8_t *p,
                                 const uint8_t *end, uint32_t *consumed,
                                 const StagePlacementDst *dst)
{
    uint32_t placement_count, anim_count, fixup_count;
    uint32_t off_pos, off_anims, off_fixups, block_size;
    uint32_t i, words;

    *consumed = 0;

    if (end - p < 32 || memcmp(p, dst->magic, 4) != 0)
    {
        return 0;                   /* not a bonus course */
    }
    memcpy(&placement_count, p + 4, 4);
    memcpy(&anim_count, p + 8, 4);
    memcpy(&fixup_count, p + 12, 4);
    memcpy(&off_pos, p + 16, 4);
    memcpy(&off_anims, p + 20, 4);
    memcpy(&off_fixups, p + 24, 4);
    memcpy(&block_size, p + 28, 4);

    if (placement_count > dst->pos_max ||
        anim_count > dst->anims_max ||
        (uintptr_t)(p + off_pos + placement_count * 16) > (uintptr_t)end ||
        (uintptr_t)(p + off_fixups + fixup_count * 12) > (uintptr_t)end ||
        (uintptr_t)(p + block_size) > (uintptr_t)end)
    {
        dbglog(DBG_ERROR, "stage: %s: bad %s block\n",
               st->model.hd->name, dst->magic);
        return -1;
    }
    {
        uint32_t at = off_anims;

        words = 0;
        for (i = 0; i < anim_count; i++)
        {
            uint32_t n;
            memcpy(&n, p + at, 4);
            at += 36 + n * 4;
            words += n;
        }
        if ((uintptr_t)(p + at) > (uintptr_t)end)
        {
            dbglog(DBG_ERROR, "stage: %s: %s anims run past the pack\n",
                   st->model.hd->name, dst->magic);
            return -1;
        }
    }
    *dst->owned = malloc(words * 4);
    if (*dst->owned == NULL && words != 0)
    {
        dbglog(DBG_ERROR, "stage: %s: %s script alloc failed\n",
               st->model.hd->name, dst->what);
        return -1;
    }
    {
        uint32_t *w = (uint32_t *)*dst->owned;
        const uint8_t *src = p + off_anims;

        for (i = 0; i < anim_count; i++)
        {
            uint32_t n, k;

            memcpy(&n, src, 4);
            dst->anims[i] = (AObjEvent32 *)w;
            for (k = 0; k < n; k++)
            {
                memcpy(&w[k], src + 36 + k * 4, 4);
            }
            w += n;
            src += 36 + n * 4;
        }
        for (i = 0; i < fixup_count; i++)
        {
            uint32_t a, fw, t;

            memcpy(&a, p + off_fixups + i * 12, 4);
            memcpy(&fw, p + off_fixups + i * 12 + 4, 4);
            memcpy(&t, p + off_fixups + i * 12 + 8, 4);
            if (a >= anim_count || t >= anim_count)
            {
                dbglog(DBG_ERROR, "stage: %s: %s fixup %u out of range\n",
                       st->model.hd->name, dst->magic, (unsigned)i);
                return -1;
            }
            ((uint32_t *)dst->anims[a])[fw] = (uint32_t)(uintptr_t)
                dst->anims[t];
        }
    }
    for (i = 0; i < placement_count; i++)
    {
        const uint8_t *t = p + off_pos + i * 16;
        int32_t a;

        memcpy(&dst->pos[i].x, t, 4);
        memcpy(&dst->pos[i].y, t + 4, 4);
        memcpy(&dst->pos[i].z, t + 8, 4);
        memcpy(&a, t + 12, 4);
        if (a >= (int32_t)anim_count)
        {
            dbglog(DBG_ERROR, "stage: %s: %s placement %u names script "
                              "%d\n",
                   st->model.hd->name, dst->magic, (unsigned)i, (int)a);
            return -1;
        }
        dst->anim[i] = (int8_t)a;
    }
    *dst->count = (uint8_t)placement_count;
    *dst->anim_count = (uint8_t)anim_count;

    dbglog(DBG_INFO, "stage: %s: %u %s(s), %u script(s)\n",
           st->model.hd->name, (unsigned)placement_count, dst->what,
           (unsigned)anim_count);

    *consumed = block_size;
    return 0;
}


/* See stage.h. The two blocks, in the order the exporter writes them. */
static int stage_load_targets(Stage *st, const uint8_t *p, const uint8_t *end,
                              uint32_t *consumed)
{
    StagePlacementDst dst;

    dst.magic = "BTG1";
    dst.pos = st->targets;
    dst.anim = st->target_anim;
    dst.count = &st->target_count;
    dst.anim_count = &st->target_anim_count;
    dst.anims = st->target_anims;
    dst.owned = &st->target_owned;
    dst.pos_max = STAGE_TARGETS_MAX;
    dst.anims_max = STAGE_TARGET_ANIMS_MAX;
    dst.what = "target";

    return stage_load_placements(st, p, end, consumed, &dst);
}

static int stage_load_bumpers(Stage *st, const uint8_t *p, const uint8_t *end,
                              uint32_t *consumed)
{
    StagePlacementDst dst;

    dst.magic = "BMP1";
    dst.pos = st->bumpers;
    dst.anim = st->bumper_anim;
    dst.count = &st->bumper_count;
    dst.anim_count = &st->bumper_anim_count;
    dst.anims = st->bumper_anims;
    dst.owned = &st->bumper_owned;
    dst.pos_max = STAGE_BUMPERS_MAX;
    dst.anims_max = STAGE_BUMPER_ANIMS_MAX;
    dst.what = "bumper";

    return stage_load_placements(st, p, end, consumed, &dst);
}

/* See stage.h: BPL1, and the one file it is the whole of.
 *
 * Every other block a Stage carries is a span of its own .stg, read
 * where it lies. This one is not: the six trees belong to all twelve
 * Board the Platforms courses, so they are a file the course's Stage
 * opens on its way in and lets go of on its way out. Everything past
 * the open is the same parse BWP1 gets.
 *
 * A stage with no platforms is not an error -- the file is only read
 * for a course that has any -- so the caller asks for this and nothing
 * else does. */
static int stage_load_platforms(Stage *st)
{
    StageTreePackDst dst;
    uint32_t consumed = 0;
    long size;

    st->platform_blob = stage_bytes("bonus2plat.pak", &size,
                                    &st->platform_blob_owned);
    if (st->platform_blob == NULL)
    {
        return -1;
    }
    dst.magic = "BPL1";
    dst.models = st->platform_models;
    dst.pal_banks = st->platform_pal_banks;
    dst.names = st->platform_names;
    dst.billboard = st->platform_billboard;
    dst.matanim_count = st->platform_matanim_count;
    dst.anim_tables = st->platform_anim_tables;
    dst.model_count = &st->platform_model_count;
    dst.owned = &st->platform_owned;
    dst.models_max = STAGE_PLATFORM_MODELS_MAX;
    dst.what = "platforms";

    if (stage_load_packs(st, (const uint8_t *)st->platform_blob,
                         (const uint8_t *)st->platform_blob + size,
                         &consumed, &dst) < 0)
    {
        return -1;
    }
    if (consumed == 0)
    {
        dbglog(DBG_ERROR, "stage: %s: bonus2plat.pak is not a BPL1 file\n",
               st->model.hd->name);
        return -1;
    }
    return 0;
}

/* See stage.h. */
int stage_platform_pack(Stage *st, const char *name)
{
    int i;

    for (i = 0; i < st->platform_model_count; i++)
    {
        if (strcmp(st->platform_names[i], name) == 0)
        {
            return i;
        }
    }
    return -1;
}

/* See stage.h. */
int stage_boss_pack(Stage *st, const char *name)
{
    int i;

    for (i = 0; i < st->boss_model_count; i++)
    {
        if (strcmp(st->boss_names[i], name) == 0)
        {
            return i;
        }
    }
    return -1;
}

/* The stage's map object, as a GObj: the game builds it with
 * gr/grmodelsetup.c's grModelSetupGroundDObjs straight off the map
 * file's DObjDesc, and the port's tree is the same tree baked, so this
 * is dc_model_add_dobjs over the pack plus the game's own proc_display
 * and anim wiring. Deferred out of stage_load for the reason its own
 * comment gives: the DObjs come off the object pool.
 *
 * Returns the tree's root DObj, or NULL. */
int stage_load(Stage *st, const char *name, int *pal_bank)
{
    long size;
    const StgHeader *hd;
    const StgExtra *ex;

    memset(st, 0, sizeof(*st));
    st->blob = stage_bytes(name, &size, &st->blob_owned);
    if (st->blob == NULL)
    {
        return -1;
    }

    hd = (const StgHeader *)st->blob;
    if (memcmp(hd->magic, "SSBSTAG1", 8) != 0)
    {
        dbglog(DBG_ERROR, "stage: %s: bad magic\n", name);
        return -1;
    }
    if (fighter_init(&st->model, (uint8_t *)st->blob + sizeof(StgHeader),
                     hd->pack_size, pal_bank) < 0)
    {
        /* named, because fighter_init's own message does not say WHICH
         * pack it refused and a stage has up to six */
        dbglog(DBG_ERROR, "stage: %s: the LAYER pack would not load\n",
               name);
        return -1;
    }

    ex = (const StgExtra *)((uint8_t *)st->blob + hd->extra_off);
    if (memcmp(ex->magic, "STG6", 4) != 0)
    {
        dbglog(DBG_ERROR, "stage: %s: bad extras magic (want STG6)\n",
               name);
        return -1;
    }
    st->line_count = ex->line_count;
    st->vertex_id_count = ex->vertex_id_count;
    st->vertex_count = ex->vertex_count;
    st->cam_top = ex->cam[0];
    st->cam_bottom = ex->cam[1];
    st->cam_right = ex->cam[2];
    st->cam_left = ex->cam[3];
    st->map_top = ex->map[0];
    st->map_bottom = ex->map[1];
    st->map_right = ex->map[2];
    st->map_left = ex->map[3];
    st->alt_warning = ex->alt_warning;
    st->layer1_jbase = ex->layer1_jbase;
    st->layer1_joints = ex->layer1_joints;
    st->bgm = ex->bgm;
    st->light_angle[0] = ex->light_angle[0];
    st->light_angle[1] = ex->light_angle[1];
    st->light_angle[2] = ex->light_angle[2];
    memcpy(st->emblem, ex->emblem, sizeof(ex->emblem));
    memcpy(st->emblem[GMCOMMON_PLAYERS_MAX], ex->emblem_cp,
           sizeof(ex->emblem_cp));
    st->fog_rgb = ex->fog_rgb;
    memcpy(st->cam_team, ex->cam_team, sizeof(st->cam_team));
    memcpy(st->map_team, ex->map_team, sizeof(st->map_team));
    memcpy(st->zoom_start, ex->zoom_start, sizeof(st->zoom_start));
    memcpy(st->zoom_end, ex->zoom_end, sizeof(st->zoom_end));
    {
        /* the tables, back to back after the header; every element is
         * 2-byte aligned, which the blob (malloc) guarantees */
        const uint8_t *p = (const uint8_t *)(ex + 1);

        /* MPGeometryData's table pointers are void* in the decomp
         * (mp/mptypes.h:73-79); the blob is ours to write, so no
         * const to shed */
        st->geo.yakumono_count = ex->yakumono_count;
        st->geo.line_info = (MPLineInfo *)p;
        p += (size_t)ex->yakumono_count * sizeof(MPLineInfo);
        st->geo.vertex_links = (void *)p;
        p += (size_t)ex->line_count * sizeof(MPVertexLinks);
        st->geo.vertex_id = (void *)p;
        p += (size_t)ex->vertex_id_count * sizeof(uint16_t);
        st->geo.vertex_data = (void *)p;
        p += (size_t)ex->vertex_count * sizeof(MPVertexData);
        st->geo.mapobj_count = ex->mapobj_count;
        st->geo.mapobjs = (void *)p;
        p += (size_t)ex->mapobj_count * sizeof(MPMapObjData);
        p += (4 - ((uintptr_t)p & 3)) & 3;
        ex = (const StgExtra *)p;    /* reused below as the cursor */
    }

    /* Optional WLP1 block: the stage background image, ARGB1555 padded
     * to power-of-two dims. Upload twiddled, compile an OP header. */
    {
        const uint8_t *p = (const uint8_t *)ex;
        const uint8_t *end = (const uint8_t *)st->blob + size;
        if (end - p >= 12 && memcmp(p, "WLP1", 4) == 0)
        {
            pvr_poly_cxt_t cxt;
            uint16_t d[4];
            memcpy(d, p + 4, 8);
            st->wp_texw = d[0];
            st->wp_texh = d[1];
            st->wp_w = d[2];
            st->wp_h = d[3];
            /* The renderer may still be reading what this is about
             * to overwrite (dcpvr.h dc_pvr_vram_fence). */
            dc_pvr_vram_fence();
            st->wp_txr = pvr_mem_malloc((uint32_t)d[0] * d[1] * 2);
            if (!st->wp_txr)
            {
                dbglog(DBG_ERROR, "stage: wallpaper alloc failed\n");
                return -1;
            }
            pvr_txr_load_ex((void *)(p + 12), st->wp_txr, d[0], d[1],
                            PVR_TXRLOAD_16BPP);
            pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_ARGB1555,
                             d[0], d[1], st->wp_txr, PVR_FILTER_BILINEAR);
            cxt.txr.uv_clamp = PVR_UVCLAMP_UV;
            cxt.gen.culling = PVR_CULLING_NONE;
#ifdef DB_HDR_ALIGN
            if ((((uintptr_t)&st->wp_hdr) & 31u) != 0)
                dbglog(DBG_ERROR, "hdralign: stage wp_hdr %p is %u mod 32\n",
                       (void *)&st->wp_hdr, (unsigned)(((uintptr_t)&st->wp_hdr) & 31u));
#endif
            pvr_poly_compile(&st->wp_hdr, &cxt);
            /* the block is the header and the image, and the loader is
             * its only reader: WLP1 carries no length of its own */
            ex = (const StgExtra *)(p + 12 + (size_t)d[0] * d[1] * 2);
        }
    }
    {
        uint32_t consumed = 0;

        if (stage_load_ground(st, (const uint8_t *)ex,
                              (const uint8_t *)st->blob + size,
                              &consumed) < 0)
        {
            return -1;
        }
        ex = (const StgExtra *)((const uint8_t *)ex + consumed);
    }
    {
        uint32_t consumed = 0;

        if (stage_load_boss(st, (const uint8_t *)ex,
                            (const uint8_t *)st->blob + size,
                            &consumed) < 0)
        {
            return -1;
        }
        ex = (const StgExtra *)((const uint8_t *)ex + consumed);
    }
    {
        uint32_t consumed = 0;

        if (stage_load_targets(st, (const uint8_t *)ex,
                               (const uint8_t *)st->blob + size,
                               &consumed) < 0)
        {
            return -1;
        }
        ex = (const StgExtra *)((const uint8_t *)ex + consumed);
    }
    {
        uint32_t consumed = 0;

        if (stage_load_bumpers(st, (const uint8_t *)ex,
                               (const uint8_t *)st->blob + size,
                               &consumed) < 0)
        {
            return -1;
        }
        ex = (const StgExtra *)((const uint8_t *)ex + consumed);
    }
    if (stage_load_map(st, (const uint8_t *)ex, size) < 0)
    {
        return -1;
    }
    /* and the one block that is not in this file: a Board the Platforms
     * course's platforms, which live in a file all twelve share. The
     * pack name is what says whether this is one -- the Stage is built
     * before anything hands it a nGRKind, and dGRStageFiles names the
     * twelve "bonus2<fighter>.stg" for exactly this kind of reading. */
    if (strncmp(name, "bonus2", 6) == 0 && stage_load_platforms(st) < 0)
    {
        return -1;
    }

    dbglog(DBG_INFO, "stage: %s: %u lines in %u groups, %u vertices, "
                     "%u map objects, bgm %lu, cam [%d %d %d %d], "
                     "team cam [%d %d %d %d] map [%d %d %d %d]\n",
           st->model.hd->name, (unsigned)st->line_count,
           (unsigned)st->geo.yakumono_count, (unsigned)st->vertex_count,
           (unsigned)st->geo.mapobj_count, (unsigned long)st->bgm,
           st->cam_top, st->cam_bottom, st->cam_right, st->cam_left,
           st->cam_team[0], st->cam_team[1], st->cam_team[2], st->cam_team[3],
           st->map_team[0], st->map_team[1], st->map_team[2], st->map_team[3]);
    return 0;
}

/* See stage.h. Filled by grStageAcquire, emptied by grStageRelease. */
Stage *gGRStages[GR_STAGES_MAX];

/* See stage.h. One Sprite and one Bitmap per stage, made on the first
 * ask and kept: the game's wallpaper is a Sprite sitting in the ground
 * file, so the pointer a caller gets has to stay good as long as the
 * stage does. The fields are what tools/export/ssb_spriteexport.py writes for
 * a one-strip RGBA16 sprite, over the texture stage_load already
 * uploaded -- so lb/lbcommon.c's arithmetic runs on it unchanged and
 * lbCommonDrawSObjBitmap finds a DCSpriteTex behind Bitmap.buf, which
 * is what it reads there for every other sprite. */
Sprite *stage_wallpaper_sprite(Stage *st)
{
    if (st == NULL || st->wp_txr == NULL)
    {
        return NULL;
    }
    if (st->wp_sprite.bitmap == NULL)
    {
        st->wp_tex.txr = st->wp_txr;
        st->wp_tex.texw = st->wp_texw;
        st->wp_tex.texh = st->wp_texh;
        st->wp_tex.imgw = st->wp_w;
        st->wp_tex.imgh = st->wp_h;
        st->wp_tex.fmt = nDCSpriteTexFmtARGB1555;

        st->wp_bitmap.width = (s16)st->wp_w;
        st->wp_bitmap.width_img = (s16)st->wp_w;
        st->wp_bitmap.s = 0;
        st->wp_bitmap.t = 0;
        st->wp_bitmap.buf = &st->wp_tex;
        st->wp_bitmap.actualHeight = (s16)st->wp_h;
        st->wp_bitmap.LUToffset = 0;

        st->wp_sprite.width = (s16)st->wp_w;
        st->wp_sprite.height = (s16)st->wp_h;
        st->wp_sprite.scalex = 1.0F;
        st->wp_sprite.scaley = 1.0F;
        st->wp_sprite.attr = SP_TRANSPARENT;
        st->wp_sprite.red = 0xFF;
        st->wp_sprite.green = 0xFF;
        st->wp_sprite.blue = 0xFF;
        st->wp_sprite.alpha = 0xFF;
        st->wp_sprite.istep = 1;
        st->wp_sprite.nbitmaps = 1;
        st->wp_sprite.bmheight = (s16)st->wp_h;
        st->wp_sprite.bmHreal = (s16)st->wp_h;
        st->wp_sprite.bmfmt = G_IM_FMT_RGBA;
        st->wp_sprite.bmsiz = G_IM_SIZ_16b;
        st->wp_sprite.bitmap = &st->wp_bitmap;
    }
    return &st->wp_sprite;
}

/* The port's stand-in for the ground file's MPGroundData
 * (mp/mptypes.h:150-215). The .stg carries the scalars the game logic
 * reads off it, and the pointers -- gr_desc, map_geometry, map_nodes --
 * stay NULL because the port reaches the geometry through
 * mpCollisionLoadGeometry and follows none of the others.
 * gMPCollisionGroundData (mp/map.h:9) is how mpcollision.c reads it.
 *
 * item_weights is NOT one of those. The exporter writes
 * every stage's twenty weights and
 * stage_load_map sets st->item_weights from them; they must be
 * copied into this struct, or gMPCollisionGroundData->item_weights
 * is NULL: itManagerSetupContainerDrops takes its "no
 * weights" arm, weights_sum stays zero, and that is the value
 * itMainGetWeightedItemKind samples against -- so no item could ever spawn,
 * whatever the roster. It is the decomp's own pointer
 * (mp/mptypes.h:198), a row of twenty u8 the randomizer indexes by kind,
 * and it is copied below. */
static MPGroundData sGroundData;

/* The stage the scene is playing on. The game has no such pointer: its
 * ground is a ROM file the scene loads into gMPCollisionGroundData, and
 * everything that draws it reaches it through gr_desc. The port's
 * layers are a .pack the scene acquired (grStageAcquire below), so
 * something has to remember which one, and this is it -- set by
 * stage_bind, read by the two gr/ stand-ins below. */
static Stage *sStage;

/* gr/grcommonsetup.c:0x801313F0 gGRCommonStruct -- the union of the nine
 * stages' own ground variables (gr/grtypes.h), which each stage's own
 * gr/grcommon file keeps its hazard state in. The port's gr/ has one
 * tenant so far, Hyrule's tornado (src/dc/grhyrule.c), and this is where
 * the game puts the union: grcommonsetup.c. Cleared on the way into every
 * scene with the rest of this file's statics -- the twister's position
 * array is a scene-heap pointer, so it must not survive one. */
GRStruct gGRCommonStruct;

/* The port's dMNMapsFileInfos (mn/mnmaps/mnmaps.c:28-39), which is the
 * game's table of nine map files and the offset of MPGroundData in each.
 * The port's stage is one pack with the ground data baked into its STG5
 * block, so there is no offset to carry; what is left is the name, and
 * all nine export. */
static const char *dGRStageFiles[GR_STAGES_MAX] =
{
    [nGRKindCastle] = "castle.stg",
    [nGRKindJungle] = "jungle.stg",
    [nGRKindZebes]  = "zebes.stg",
    [nGRKindHyrule] = "hyrule.stg",
    [nGRKindSector] = "sector.stg",
    [nGRKindYoster] = "yoster.stg",
    [nGRKindPupupu] = "pupupu.stg",
    /* Every stage needs an entry here: a missing one makes
     * DB_BOOT_STAGE print "no pack for boot stage N -- starting at the
     * title instead". The pack, the exporter's entry, the Makefile's
     * rule and tools/export/disc_layout.py's manifest each list the stage; this list --
     * which is what the DEBUG BOOT asks and nothing else in the build
     * does -- must too, and the stage select reaches a stage through the
     * scene's own kind rather than through this. The two lists are the
     * exporter's `STAGES` keys and these, and nothing checks them
     * against each other. */
    [nGRKindYamabuki] = "yamabuki.stg",
    [nGRKindInishie] = "inishie.stg",
    /* Final Destination, the Master Hand rung's stage and
     * the first entry here past nGRKindBattleEnd -- the stage select
     * cannot reach it, only the 1P ladder can. It has no stage logic of
     * its own (there is no gr/grcommon/grlast.c), so unlike the nine
     * above it this line is the whole of the port's Final Destination
     * but for the background, which is sc/sc1pmode/sc1pgameboss.c's. */
    [nGRKindLast] = "last.stg",
    /* The other three the 1P ladder plays and no select can reach : the Duel Zone on the Fighting Polygons' rung, Meta
     * Crystal on Metal Mario's and the small Yoshi's Island on the
     * Yoshi Team's. (gr/grdef.h:30 names the first one; its own map
     * file calls its assets dStageBattlefield*.)
     * Like Final Destination above, none of the three has a
     * gr/grcommon/gr*.c of its own, so these lines plus the pack are the
     * whole of each one. Without them the rung would run on whatever stage the debug
     * boot had left bound -- rung 12 would abort in
     * sc1PGameGetStartPosition on "mpGetMapObjNumId(38) = 0", the
     * Polygons' own spawn platforms asked of a stage that has none. */
    [nGRKindZako] = "zako.stg",
    [nGRKindMetal] = "metal.stg",
    [nGRKindYosterSmall] = "yostersmall.stg",
    /* Break the Targets, one course per fighter. Past
     * nGRKindCommonEnd, which is why src/dc/stage.h's GR_STAGES_MAX had
     * to grow to reach them; nothing but the 1P ladder's bonus rungs and
     * the bonus-practice select plays one, so like Final Destination
     * above they are unreachable from the stage select on purpose.
     *
     * In nGRKindBonus1Start order, which is FTKind's playable order --
     * the same order tools/export/ssb_stageexport.py's twelve entries and the
     * Makefile's BONUS1_COURSES are written in, so the three lists read
     * as one. */
    [nGRKindBonus1Mario]   = "bonus1mario.stg",
    [nGRKindBonus1Fox]     = "bonus1fox.stg",
    [nGRKindBonus1Donkey]  = "bonus1donkey.stg",
    [nGRKindBonus1Samus]   = "bonus1samus.stg",
    [nGRKindBonus1Luigi]   = "bonus1luigi.stg",
    [nGRKindBonus1Link]    = "bonus1link.stg",
    [nGRKindBonus1Yoshi]   = "bonus1yoshi.stg",
    [nGRKindBonus1Captain] = "bonus1captain.stg",
    [nGRKindBonus1Kirby]   = "bonus1kirby.stg",
    [nGRKindBonus1Pikachu] = "bonus1pikachu.stg",
    [nGRKindBonus1Purin]   = "bonus1purin.stg",
    [nGRKindBonus1Ness]    = "bonus1ness.stg",
    /* Board the Platforms, the same twelve and the
     * same order. */
    [nGRKindBonus2Mario]   = "bonus2mario.stg",
    [nGRKindBonus2Fox]     = "bonus2fox.stg",
    [nGRKindBonus2Donkey]  = "bonus2donkey.stg",
    [nGRKindBonus2Samus]   = "bonus2samus.stg",
    [nGRKindBonus2Luigi]   = "bonus2luigi.stg",
    [nGRKindBonus2Link]    = "bonus2link.stg",
    [nGRKindBonus2Yoshi]   = "bonus2yoshi.stg",
    [nGRKindBonus2Captain] = "bonus2captain.stg",
    [nGRKindBonus2Kirby]   = "bonus2kirby.stg",
    [nGRKindBonus2Pikachu] = "bonus2pikachu.stg",
    [nGRKindBonus2Purin]   = "bonus2purin.stg",
    [nGRKindBonus2Ness]    = "bonus2ness.stg",
    /* Race to the Finish, which sits with the
     * COMMON stages in nGRKind rather than with the bonus ones. */
    [nGRKindBonus3]        = "bonus3.stg",
};

/* How many holders each entry of gGRStages has. The game needs no such
 * count: a map file is force-loaded into a heap the caller named, so two
 * callers wanting the same stage get two copies of it. The port has one
 * table and one copy, and the stage select's two previews can be the
 * same kind as each other or as the battle's, so somebody has to say
 * when the last holder let go. */
static u8 sGRStageRefs[GR_STAGES_MAX];

const char *grStageFileName(s32 gkind)
{
    if (gkind < 0 || gkind >= GR_STAGES_MAX)
    {
        return NULL;
    }
    return dGRStageFiles[gkind];
}

void stage_release(Stage *st)
{
    if (st == NULL)
    {
        return;
    }
    /* A stage that is still the bound one leaves gMPCollisionGroundData
     * pointing at scalars copied out of it, which stay valid -- they are
     * this file's statics, not the blob's -- but sStage would dangle.
     * Nothing should release a bound stage; saying so here costs a
     * compare and turns a use-after-free into a NULL. */
    if (sStage == st)
    {
        sStage = NULL;
    }
    if (st->wp_txr != NULL)
    {
        /* The renderer may still be reading what this is about to hand
         * back (dcpvr.h dc_pvr_vram_fence). */
        dc_pvr_vram_fence();
        pvr_mem_free(st->wp_txr);
        st->wp_txr = NULL;
    }
    /* the model's textures, headers and clip buffer; its blob is a
     * pointer into ours, so fighter_init left blob_owned clear */
    fighter_release(&st->model);
    {
        int i;

        for (i = 0; i < st->map_object_count; i++)
        {
            fighter_release(&st->map_models[i]);
        }
    }
    if (st->graft_model.hd != NULL)
    {
        fighter_release(&st->graft_model);
    }
    {
        int i;

        for (i = 0; i < st->ground_model_count; i++)
        {
            fighter_release(&st->ground_models[i]);
        }
    }
    {
        int i;

        for (i = 0; i < st->boss_model_count; i++)
        {
            fighter_release(&st->boss_models[i]);
        }
    }
    {
        int i;

        for (i = 0; i < st->platform_model_count; i++)
        {
            fighter_release(&st->platform_models[i]);
        }
    }
    free(st->ground_owned);
    free(st->boss_owned);
    free(st->target_owned);
    free(st->bumper_owned);
    free(st->platform_owned);
    free(st->map_owned);
    if (st->platform_blob_owned)
    {
        free(st->platform_blob);
    }
    if (st->blob_owned)
    {
        free(st->blob);
    }
    memset(st, 0, sizeof(*st));
}

/* See stage.h: object 0, which is the tree MPGroundData.map_nodes names
 * -- the one the game's own ground setup builds before any stage logic
 * runs, and the only one every stage has. */
DObj *stage_bind_map_model(GObj *gobj, s32 dl_link)
{
    return stage_bind_map_object(gobj, 0, dl_link);
}

/* See stage.h. One baked pack, its DObjs added to `gobj` and its joints
 * recorded in map_joints[index] -- which is what the scripts that address
 * the tree by joint index are read against. */
DObj *stage_bind_map_object(GObj *gobj, s32 index, s32 dl_link)
{
    Fighter *m;

    if (sStage == NULL || index < 0 || index >= sStage->map_object_count)
    {
        return NULL;
    }
    m = &sStage->map_models[index];

    if (m->hd == NULL)
    {
        return NULL;
    }
    if (dc_model_add_dobjs(gobj, NULL, m, sStage->map_joints[index]) < 0)
    {
        return NULL;
    }
    /* An object whose materials are ANIMATED carries them as MObjs with
     * one `alt` per state, and the stage switches between them with
     * stage_map_set_anim as its own state machine runs. alt 0 is what
     * every object starts on, which is the decomp's own first state. */
    if (m->mobjs != NULL)
    {
        dc_model_add_mobjs_alt(gobj, m, 0.0F, 0);
    }
    /* DIVERGES: dc_model_proc_display, not the decomp's
     * gcDrawDObjTreeForGObj/gcDrawDObjTreeDLLinksForGObj (which one, per
     * the object's own `dl_links` export flag, is beside the point) --
     * both only record each joint's matrix (dc_joint_submit) and never
     * hand the baked batches to the PVR, the same gap src/dc/itdisplay.c's
     * header describes for items. Every map object this binds -- Kongo
     * Jungle's TaruCann, Zebes' acid, Dream Land's Whispy parts -- was
     * animating correctly in memory and never drawing a single triangle
     * without this (TaruCann's
     * position/anim data checked out exactly right, and it still never
     * appeared). */
    gcAddGObjDisplay(gobj, dc_model_proc_display, dl_link,
                     GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 5);

    return DObjGetStruct(gobj);
}

/* See stage.h: the stage the scene is playing on. */
Stage *stage_bound(void)
{
    return sStage;
}

/* See stage.h. The graft: one pack, one DObj, grafted onto whatever
 * DObj the stage's own code names -- Yoshi's Island's cloud onto each of
 * its skeleton's cloud joints. `alt` picks which of the pack's
 * MatAnimJoints the material starts on (the FPackMObjs `alt`s, which is
 * dGRYosterCloudMatAnimJoints' pair). Returns the graft's root DObj, or
 * NULL for a stage with no graft. */
DObj *stage_bind_map_cloud(GObj *gobj, DObj *parent, int alt)
{
    DObj *made[STAGE_GRAFT_JOINTS_MAX];
    int n, j;

    if (sStage == NULL || sStage->graft_model.hd == NULL)
    {
        return NULL;
    }
    n = dc_model_add_dobjs(gobj, parent, &sStage->graft_model, made);
    if (n < 0)
    {
        return NULL;
    }
    /* DIVERGES: the graft draws itself, because the GObj's own display
     * proc (gcDrawDObjTreeForGObj, from stage_bind_map_model) is the
     * game's and only records matrices -- and all nine clouds share the
     * one Stage.graft_model, so there is no per-instance place to record
     * them into. This is where the pack's submission is wired instead;
     * see dc_model_graft_display. */
    for (j = 0; j < n; j++)
    {
        dc_model_graft_display(made[j]);
    }
    /* the DObj-rooted attach, not dc_model_add_mobjs_alt: the graft is
     * one DObj of a bigger tree, and the game's own walk starts at the
     * cloud itself (lbCommonAddMObjForTreeDObjs) */
    dc_model_add_mobjs_dobj(made[0], &sStage->graft_model, 0.0F, alt, 0);

    return made[0];

}

/* The shared half of stage_cloud_set_anim and stage_map_set_anim: both
 * re-point an already-built tree's MObjs at another of its pack's
 * MatAnimJoint `alt`s, and only the pack they read differs.
 *
 * The scripts live in the pack's own word array (FPackMObjs.off_words)
 * as host-order words with their pointers already word indices, so
 * BOTH builds can hand the game one -- unlike the trees' own
 * AnimJoints, which the host cannot walk at all (AObjEvent32 is eight
 * bytes there; see grJungleMakeTaruCann). This one is guarded anyway,
 * because the pointer arithmetic below reads the words as AObjEvent32. */
static void stage_model_set_anim(const Fighter *m, DObj *dobj, int alt)
{
    const FPackMObjs *mo;
    MObj *mobj;
    u32 k;

    if (m == NULL || m->mobjs == NULL)
    {
        return;
    }
    mo = m->mobjs;

    if (alt < 0 || (u32)alt >= mo->alt_count)
    {
        alt = 0;
    }
    /* The WHOLE TREE, with `k` running across it in tree order -- the
     * same walk and the same indexing dc_model_add_mobjs_alt built the
     * MObjs with, which is why the two agree on which entry is whose. A
     * map object's MObjs need not be on its root: Dream Land's eyes
     * carry theirs on the joint the script is on. */
    k = 0;

    while (dobj != NULL)
    {
        for (mobj = dobj->mobj;
             (mobj != NULL) && (k < mo->mobj_count);
             mobj = mobj->next, k++)
        {
#ifndef FT_HOSTTEST
            s32 e = ((const s32 *)((const u8 *)m->blob + mo->off_entry))
                [(u32)alt * mo->mobj_count + k];

            if (e >= 0)
            {
                gcAddMObjMatAnimJoint(mobj,
                    (AObjEvent32 *)((const u8 *)m->blob + mo->off_words +
                                    (u32)e * 4), 0.0F);
            }
#else
            /* DIVERGES, host build only: AObjEvent32 is eight bytes here,
             * so the script this would hand the game cannot be walked.
             * See grJungleMakeTaruCann's own note. */
#endif
        }
        dobj = gcGetTreeDObjNext(dobj);
    }
}

/* See stage.h. The graft's material script: what
 * grYosterUpdateCloudAnim does when a cloud changes state. */
void stage_cloud_set_anim(DObj *dobj, int alt)
{
    if (sStage == NULL)
    {
        return;
    }
    stage_model_set_anim(&sStage->graft_model, dobj, alt);
}

/* See stage.h. The same, for one of the stage's map objects: what
 * grPupupuUpdateGObjAnims does when Whispy's eyes or mouth change
 * state, where the game re-adds the tree's whole MatAnimJoint table and
 * the port re-points the MObjs the bind already built. */
void stage_map_set_anim(DObj *dobj, int index, int alt)
{
    if (sStage == NULL || index < 0 || index >= sStage->map_object_count)
    {
        return;
    }
    stage_model_set_anim(&sStage->map_models[index], dobj, alt);
}

/* One of the stage's map-file AnimJoint scripts by name -- the port's
 * stand-in for the decomp's `map_head + &llGR<Stage>Map<Name>AnimJoint`,
 * which is a linker symbol the port has no address space for. NULL if
 * the stage's map file has no script by that name. */
AObjEvent32 *stage_map_anim(const char *name)
{
    int i;

    if (sStage == NULL)
    {
        return NULL;
    }
    for (i = 0; i < sStage->map_anim_count; i++)
    {
        if (strcmp(sStage->map_anim_names[i], name) == 0)
        {
            return sStage->map_anims[i];
        }
    }
    return NULL;
}

/* See stage.h: every script the map file has under that name, as the
 * per-DObj array the game's own walk wants. A name is a TABLE, not a
 * script -- Dream Land's `WhispyEyesRight1Texture` is two scripts, on
 * joints 3 and 9 of the flowers-front tree's ten -- and the array is the
 * tree's length whatever the table holds, because a NULL entry is what
 * the walk reads as "nothing to play on this joint" where an entry past
 * the end would be read as a script. */
void stage_map_anim_array(const char *name, AObjEvent32 **out)
{
    int i;

    for (i = 0; i < STAGE_MAP_JOINTS_MAX; i++)
    {
        out[i] = NULL;
    }
    if (sStage == NULL)
    {
        return;
    }
    for (i = 0; i < sStage->map_anim_count; i++)
    {
        int j;

        if (strcmp(sStage->map_anim_names[i], name) != 0)
        {
            continue;
        }
        j = sStage->map_anim_joints[i];

        if (j >= 0 && j < STAGE_MAP_JOINTS_MAX)
        {
            out[j] = sStage->map_anims[i];
        }
    }
}

/* The geometry layers' own AnimJoints: the stage's background, moving.
 *
 * The game gives each layer its own GObj and, when MPGroundDesc.anim_joints
 * is non-NULL, hangs the layer's per-DObj table off it with gcAddAnimAll
 * and runs dGRDisplayDescs[layer].proc_update -- gcPlayAnimAll for layers
 * 0, 2 and 3 (gr/grdisplay.c:182-219). The port bakes all four layers into
 * one pack, so the four tables are exported as ONE FPackAnim over the
 * merged joints (tools/export/ssb_stageexport.py read_layer_anims) and there is
 * one attach here rather than four.
 *
 * Layer 1's table is deliberately not exported -- it is the collision
 * layer, whose DObjs are the yakumono platforms the port stands in for
 * with zeroed DObjs (mpcommon.c). Its joints therefore reach this walk
 * with a NULL entry and play nothing, which is what the stand-ins want.
 *
 * The rebuild below is efmanager.c's efModelAnimJoint, for the same
 * reason it exists there: the pack stores a word index per joint because
 * a baked pointer would not survive the trip, and the array is read only
 * during gcAddAnimJointAll's own walk, so one static scratch does. */
#ifndef FT_HOSTTEST
static AObjEvent32 *sStageLayerAnim[FIGHTER_MAX_JOINTS];

/* The merged model's DObjs, one per pack joint, kept because the
 * COLLISION layer's are also the yakumono platforms and mp/ addresses
 * those by id for the life of the match. dc_model_add_dobjs already fills
 * an array like this for a map object; the stage's own model asks for
 * NULL. */
static DObj *sStageLayerJoints[FIGHTER_MAX_JOINTS];

/* Point gMPCollisionYakumonoDObjs at the pack's layer-1 joints.
 *
 * The game never does this explicitly: grCommonSetupInitAll passes that
 * array straight into grDisplayMakeGeometryLayer for layer 1 and
 * gcSetupCustomDObjs writes one entry per DObjDesc as it builds the tree
 * (objanim.c:2363-2366), so yakumono id k IS layer-1 joint k. The port
 * builds its tree from FPackJoint instead, so the same binding is spelled
 * out here from the two offsets the exporter recorded.
 *
 * mpcommon.c's zeroed stand-ins stay as the fallback: an id the pack
 * cannot answer for keeps the DObj it already had, so nothing in mp/ ever
 * dereferences NULL. A stage file written before layer1_joints existed
 * reads 0 and keeps every stand-in, which is exactly the old behaviour.
 */
static void stage_bind_yakumono(void)
{
    int bound = 0;
    int i;

    if (gMPCollisionYakumonoDObjs == NULL)
    {
        return;
    }
    for (i = 0; i < gMPCollisionYakumonosNum; i++)
    {
        uint32_t j = (uint32_t)sStage->layer1_jbase + (uint32_t)i;

        if (i >= (int)sStage->layer1_joints || j >= FIGHTER_MAX_JOINTS ||
            sStageLayerJoints[j] == NULL)
        {
            continue;
        }
        gMPCollisionYakumonoDObjs->dobjs[i] = sStageLayerJoints[j];
        bound++;
    }
    syDebugPrintf("yakumono: %d of %d id(s) bound to layer-1 joints "
                  "(base %d, %d joint(s))\n", bound,
                  (int)gMPCollisionYakumonosNum, (int)sStage->layer1_jbase,
                  (int)sStage->layer1_joints);
}

/* The attach itself, over any loaded stage's merged tree on `gobj`:
 * gcAddAnimJointAll over the pack's per-joint table, rebuilt into
 * sStageLayerAnim. Returns how many joints carry a script, 0 when the
 * pack has no layer animation (Hyrule) and nothing is attached. */
static int stage_layer_anim_attach(const Stage *st, GObj *gobj)
{
    const Fighter *f = &st->model;
    const FPackAnim *a;
    const int32_t *entries;
    uint32_t i;
    int n = 0;

    if (f->hd->anim_count == 0)
    {
        return 0;               /* Hyrule: nothing on any layer animates */
    }
    a = &f->anims[0];

    if (a->kind != FPACK_ANIM_ANIMJOINT)
    {
        return 0;
    }
    entries = (const int32_t *)((const uint8_t *)f->blob + a->off_entries);

    for (i = 0; i < FIGHTER_MAX_JOINTS; i++)
    {
        sStageLayerAnim[i] = NULL;
    }
    for (i = 0; i < f->hd->joint_count && i < FIGHTER_MAX_JOINTS; i++)
    {
        if (entries[i] >= 0 && (uint32_t)entries[i] < a->nwords)
        {
            sStageLayerAnim[i] =
                (AObjEvent32 *)((uint8_t *)f->blob + a->off_words)
                + entries[i];
            n++;
        }
    }
    if (n != 0)
    {
        gcAddAnimJointAll(gobj, sStageLayerAnim, 0.0F);
    }
    return n;
}

/* See stage.h. */
sb32 stage_add_layer_anims(Stage *st, GObj *gobj)
{
    int mobjs, joints;

    if (st == NULL || DObjGetStruct(gobj) == NULL)
    {
        return FALSE;
    }
    mobjs = dc_model_add_mobjs(gobj, &st->model, 0.0F);
    joints = stage_layer_anim_attach(st, gobj);

    return ((mobjs > 0) || (joints > 0)) ? TRUE : FALSE;
}

/* Returns whether the COLLISION layer is among the layers that animate,
 * which is what decides the caller's update proc. */
static int stage_layer_anim_play(GObj *ground_gobj)
{
    const Fighter *f = &sStage->model;
    uint32_t i;
    int n, layer1 = 0;

    n = stage_layer_anim_attach(sStage, ground_gobj);

    if (n == 0)
    {
        return 0;
    }

    /* Whether the COLLISION layer is one of the layers with a script is
     * what picks the update proc in grCommonSetupInitAll, exactly as
     * grDisplayMakeGeometryLayer picks between dGRDisplayDescs[1]'s
     * mpCollisionPlayYakumonoAnim and mpCollisionAdvanceUpdateTic. It is
     * read back off the tree rather than carried as a flag, because the
     * entries are already the authority on which joints animate. */
    for (i = 0; i < sStage->layer1_joints; i++)
    {
        uint32_t j = sStage->layer1_jbase + i;

        if (j < FIGHTER_MAX_JOINTS && sStageLayerAnim[j] != NULL)
        {
            layer1 = 1;
            break;
        }
    }

    /* The port's own line: the target is the only place the attach can be
     * checked at all, and a count is what separates "the table landed" from
     * "the table is there and every entry is NULL". Disc probe: `--serial`,
     * grep "stage anim:". */
    syDebugPrintf("stage anim: %d of %d layer joint(s) carry a script%s\n",
                  n, (int)f->hd->joint_count,
                  layer1 ? ", collision layer among them" : "");
    return layer1;
}
#endif

/* See stage.h: which joint of its tree that script belongs on. */
int stage_map_anim_joint(const char *name)
{
    int i;

    if (sStage == NULL)
    {
        return -1;
    }
    for (i = 0; i < sStage->map_anim_count; i++)
    {
        if (strcmp(sStage->map_anim_names[i], name) == 0)
        {
            return sStage->map_anim_joints[i];
        }
    }
    return -1;
}

/* The stage's map object tree's joints, in the map file's own order --
 * what the port uses where the game says DObjGetStruct(gobj)->child. */
DObj *stage_map_joint(int obj, int index)
{
    if (sStage == NULL || obj < 0 || obj >= STAGE_MAP_OBJECTS_MAX ||
        index < 0 || index >= STAGE_MAP_JOINTS_MAX)
    {
        return NULL;
    }
    return sStage->map_joints[obj][index];
}

Stage *grStageAcquire(s32 gkind)
{
    Stage *st;
    int pal_bank = 0;

    if (gkind < 0 || gkind >= GR_STAGES_MAX || dGRStageFiles[gkind] == NULL)
    {
        return NULL;
    }
    if (gGRStages[gkind] != NULL)
    {
        sGRStageRefs[gkind]++;
        return gGRStages[gkind];
    }
    /* Stage holds a pvr_poly_hdr_t by value (wp_hdr), so the struct
     * itself is 32-byte aligned; see hdr_alloc in fighter.c for why a
     * header at malloc's 8 bytes eats the heap on a console */
    st = memalign(32, (sizeof(Stage) + 31u) & ~31u);
    if (st == NULL)
    {
        dbglog(DBG_ERROR, "stage: no room for %s\n", dGRStageFiles[gkind]);
        return NULL;
    }
    if (stage_load(st, dGRStageFiles[gkind], &pal_bank) < 0)
    {
        stage_release(st);      /* whatever it got as far as taking */
        free(st);
        return NULL;
    }
    /* fighter_init hands out PVR palette banks from *pal_bank up and
     * nothing gives them back, so a stage that kept its palettes would
     * leak a bank per load. Every stage pack bakes them out (pal_count
     * 0) and this says so rather than assuming it, as
     * ftManagerSetupFilesAllKind does for the fighters. */
    if (st->model.hd->pal_count != 0)
    {
        dbglog(DBG_ERROR, "stage: %s keeps %u palette banks; a stage that "
                          "is loaded and freed can take none\n",
               dGRStageFiles[gkind], (unsigned)st->model.hd->pal_count);
        stage_release(st);
        free(st);
        return NULL;
    }
    gGRStages[gkind] = st;
    sGRStageRefs[gkind] = 1;

    /* Ground actors (Part B): give efground.c's own dEFGroundDatas
     * (all-NULL by default, see that file's header) gkind's real table --
     * st's own ground_descs/ground_params, both the decomp's own types --
     * plus the port-own assets table (efground.h) alongside it, since
     * neither EFGroundDesc nor EFDesc carries a pack or a resolved script.
     * NULL/0 for a stage with none, which is every stage but Castle so
     * far and is exactly the no-op efGroundMakeAppearActor's own guard
     * already handles. */
    if (st->ground_desc_count != 0)
    {
        dEFGroundDatas[gkind].params_num = (u8)st->ground_param_count;
        dEFGroundDatas[gkind].effect_params = st->ground_params;
        dEFGroundDatas[gkind].o_data = 0;
        dEFGroundDatas[gkind].effect_descs = st->ground_descs;
        efGroundSetActorAssets(gkind, st->ground_assets, st->ground_desc_count);
    }
    return st;
}

void grStageRelease(s32 gkind)
{
    if (gkind < 0 || gkind >= GR_STAGES_MAX || gGRStages[gkind] == NULL)
    {
        return;
    }
    if (sGRStageRefs[gkind] > 1)
    {
        sGRStageRefs[gkind]--;
        return;
    }
    dEFGroundDatas[gkind].effect_params = NULL;
    dEFGroundDatas[gkind].effect_descs = NULL;
    efGroundSetActorAssets(gkind, NULL, 0);

    stage_release(gGRStages[gkind]);
    free(gGRStages[gkind]);
    gGRStages[gkind] = NULL;
    sGRStageRefs[gkind] = 0;
}

void stage_bind(Stage *st)
{
    int i;

    sStage = st;

    /* mp/mptypes.h:188-199, the fields mpCollision* and the camera read */
    sGroundData.camera_bound_top = st->cam_top;
    sGroundData.camera_bound_bottom = st->cam_bottom;
    sGroundData.camera_bound_right = st->cam_right;
    sGroundData.camera_bound_left = st->cam_left;
    sGroundData.map_bound_top = st->map_top;
    sGroundData.map_bound_bottom = st->map_bottom;
    sGroundData.map_bound_right = st->map_right;
    sGroundData.map_bound_left = st->map_left;
    sGroundData.alt_warning = st->alt_warning;
    sGroundData.bgm_id = st->bgm;
    /* mp/mptypes.h:201-210: the 1P game's team-rung camera and blast
     * boxes, and the bonus pause's zoom pair. Left at 0,
     * gmcamera.c's is_spgame_enemy arm would clamp a team rung's camera
     * to an empty box. */
    sGroundData.camera_bound_team_top = st->cam_team[0];
    sGroundData.camera_bound_team_bottom = st->cam_team[1];
    sGroundData.camera_bound_team_right = st->cam_team[2];
    sGroundData.camera_bound_team_left = st->cam_team[3];
    sGroundData.map_bound_team_top = st->map_team[0];
    sGroundData.map_bound_team_bottom = st->map_team[1];
    sGroundData.map_bound_team_right = st->map_team[2];
    sGroundData.map_bound_team_left = st->map_team[3];
    sGroundData.zoom_start.x = st->zoom_start[0];
    sGroundData.zoom_start.y = st->zoom_start[1];
    sGroundData.zoom_start.z = st->zoom_start[2];
    sGroundData.zoom_end.x = st->zoom_end[0];
    sGroundData.zoom_end.y = st->zoom_end[1];
    sGroundData.zoom_end.z = st->zoom_end[2];
    /* MPGroundData.fog_color (mp/mptypes.h:183). The pack has carried
     * this since the stage files landed and nothing had ever handed it
     * over, so it read 0,0,0 -- and the ONE thing in the game that reads
     * it is the off-screen magnifying glass, whose disc is drawn in the
     * stage's fog colour under G_CC_BLENDPEDECALA
     * (ifCommonPlayerMagnifyUpdateRender -> ifCommonPlayerMagnifyDrawFrame).
     * Every glass in the port came up black inside instead of the stage's
     * own sky: Dream Land's is 0x6E,0xD2,0xFF. `fog_alpha` is the struct's
     * own "unused padding" and nothing reads it, so it stays zero. */
    sGroundData.fog_color.r = (u8)(st->fog_rgb >> 16);
    sGroundData.fog_color.g = (u8)(st->fog_rgb >> 8);
    sGroundData.fog_color.b = (u8)st->fog_rgb;
    sGroundData.light_angle.x = st->light_angle[0];
    sGroundData.light_angle.y = st->light_angle[1];
    sGroundData.light_angle.z = st->light_angle[2];
    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        sGroundData.emblem_colors[i].r = st->emblem[i][0];
        sGroundData.emblem_colors[i].g = st->emblem[i][1];
        sGroundData.emblem_colors[i].b = st->emblem[i][2];
    }
    /* emblem_colors[4], the CPU's (colour index 4, nSCBattlePlayerColorCP):
     * the decomp indexes one past the declared array, into the first three
     * bytes of the `unused` word after it, which the map file fills with
     * the grey (0xDCDCDC00 on most stages). Byte order in memory is what
     * that read sees, so the bytes go in as bytes. Left at zero, every
     * computer player's emblem came up black. */
    memcpy(&sGroundData.unused, st->emblem[GMCOMMON_PLAYERS_MAX], 3);
    /* MPGroundData.item_weights (mp/mptypes.h:198): the stage's own
     * randomizer row, which the pack has carried since the map file
     * landed and which nothing had ever handed over -- the stand-in's
     * comment claimed it "stays NULL", and every item spawn in the game
     * is gated on this pointer being non-NULL (see the note above the
     * struct). NULL for a stage whose map file names none, which is the same NULL
     * the game has. */
    sGroundData.item_weights = (MPItemWeights *)st->item_weights;

    gMPCollisionGroundData = &sGroundData;
}

/* The port's stand-in for mpCollisionInitGroundData (mpcollision.c:3961),
 * which the battle scene calls at scvsbattle.c:155. The real one is in
 * the link -- mpcollision.c compiles unmodified -- but its body reads the
 * stage's ground file off the ROM through lbReloc, so it cannot be the
 * one that runs; the port's tables are already in RAM and
 * mpCollisionLoadGeometry (mpcommon.h) is the same work from there.
 *
 * It is a separate call from stage_bind because of *when* it has to
 * happen: everything it allocates comes out of the scene's general heap,
 * and syTaskmanStartTask empties that heap on the way into every scene.
 * So the scalars can be published before a scene (they are static) and
 * the tables cannot. The game has the same split for the same reason. */
void stage_bind_collision(void)
{
    if (sStage == NULL)
    {
        return;
    }
    mpCollisionLoadGeometry(&sStage->geo);
}

/* ---- the two gr/ calls the battle scene makes -------------------------
 *
 * sc/sccommon/scvsbattle.c calls six functions the port does not have
 * modules for yet; two of them do work the port can already do, and
 * these are those two, under the game's names and called from the
 * game's place in the sequence (src/dc/scvsbattle.c). They live here
 * because a stage is what they build, and stage.c is where the port's
 * stage is. When gr/ is ported for real -- grcommonsetup.c, grdisplay.c
 * and grwallpaper.c, all of them sprite and DObjDesc code that wants
 * a 2D renderer -- these go.
 */

/* gr/grcommonsetup.c:23 grCommonSetupInitAll, which makes one GObj per
 * geometry layer through grDisplayMakeGeometryLayer (grdisplay.c:182)
 * and then the ground proper.
 *
 * DIVERGES: a stage's visual layers are baked into one pack by
 * tools/export/ssb_stageexport.py, so there is one GObj here and not four, its
 * DObj tree comes from FPackJoint rather than the layer's DObjDesc, and
 * its display proc is the port's dc_model_proc_display rather than one of
 * dGRDisplayDescs' eight. What is the decomp's is the shape: the kind and
 * link (nGCCommonKindGroundDisplay, link 2 -- ahead of the fighters on
 * link 3, which is why the update tic below lands before they collide)
 * and, from grdisplay.c:212-215, the mpCollisionAdvanceUpdateTic process
 * that layer 1 carries when the layer has no animation.
 *
 * The pack is not anim-less: Hyrule is the only one of the nine stages
 * whose every geometry layer is static, and every other stage's
 * background moves (Yoshi's Island's heart swings, Sector Z's Great Fox
 * moves).
 * All four layers' AnimJoints are carried (stage_layer_anim_play
 * above), layer 1 among them, and layer 1's joints ARE the yakumono
 * DObjs (stage_bind_yakumono above).
 * The MatAnimJoint half of gcAddAnimAll is carried too: the pack keeps
 * the layers' MObjSubs per joint and their scripts as one MatAnimJoint,
 * and dc_model_add_mobjs below is gcAddMObjAll + gcAddMatAnimJointAll
 * over them. Five stages have one -- Brinstar's acid palette, Mushroom
 * Kingdom's, Saffron City's and Yoshi's Island's texture flips, and Dream
 * Land's clouds.
 *
 * Also gone: grMainSetupMakeGround (the per-stage ground makers are
 * ftchar-shaped per-stage code). efGroundMakeAppearActor
 * (ef/efground.c) is called here now, in the game's place at the tail of
 * this function; it is its own no-op today, because
 * dEFGroundDatas has nowhere to point yet -- see that file's header.
 *
 * itManagerMakeAppearActor is called at the tail. */
GObj *grCommonSetupInitAll(void)
{
    GObj *ground_gobj;

    if (sStage == NULL)
    {
        return NULL;
    }
    ground_gobj = gcMakeGObjSPAfter(nGCCommonKindGroundDisplay, NULL,
                                    nGCCommonLinkIDGroundDisplay,
                                    GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(ground_gobj, dc_model_proc_display, STAGE_DLLINK,
                     GOBJ_PRIORITY_DEFAULT, ~0);

    if (dc_model_add_dobjs(ground_gobj, NULL, &sStage->model,
                           sStageLayerJoints) < 0)
    {
        return NULL;
    }
#ifdef FT_HOSTTEST
    gcAddGObjProcess(ground_gobj, mpCollisionAdvanceUpdateTic,
                     nGCProcessKindFunc, 4);
#else
    /* The collision layer's DObjs, before anything reads them: the game
     * hands gcSetupCustomDObjs the yakumono array as layer 1's
     * out-parameter, so this is the same binding under the port's own
     * tree. It has to happen before mpCollisionInitYakumonoAll below,
     * which reads those DObjs to size the stage bounds. */
    stage_bind_yakumono();

    /* The layers' MObjs, and with them the MatAnimJoint half of
     * gcAddAnimAll -- the acid palette Brinstar cycles, the texture
     * Mushroom Kingdom, Saffron City and Yoshi's Island flip. The pack
     * carries the MObjSubs per joint and their scripts as one
     * MatAnimJoint, so this is gcAddMObjAll + gcAddMatAnimJointAll over
     * exactly the arrays a relocData file would have held, and it returns
     * 0 for the five stages whose layers have no material animation.
     *
     * Nothing else is needed to make them PLAY: gcPlayAnimAll already
     * walks dobj->mobj and runs gcParseMObjMatAnimJoint/gcPlayMObjMatAnim
     * (sys/objanim.c:1438-1446), mpCollisionPlayYakumonoAnim is its
     * superset, and dc_model_add_dobjs above installed dc_joint_material
     * on every joint the moment the pack came with MObjs. */
    dc_model_add_mobjs(ground_gobj, &sStage->model, 0.0F);

    /* The layers' own animation, which the game attaches in
     * grDisplayMakeGeometryLayer per layer. Not in the host build for the
     * reason grJungleMakeTaruCann gives: `union AObjEvent32` is eight
     * bytes on x86-64, so the host cannot walk a script. */
    if (stage_layer_anim_play(ground_gobj))
    {
        /* dGRDisplayDescs[1].proc_update. The game gives layer 1 this one
         * when the layer has an animation table, and it is a strict
         * SUPERSET of gcPlayAnimAll: it parses and plays every DObj's
         * AnimJoint and every MObj's MatAnimJoint over the whole tree,
         * and additionally derives gMPCollisionSpeeds for the yakumono
         * DObjs -- what carries a fighter standing on a moving platform
         * -- and ends with the two bounds updates and the tic increment.
         * So it REPLACES both processes here rather than joining them;
         * registering gcPlayAnimAll as well would advance every
         * background joint twice a tic. */
        gcAddGObjProcess(ground_gobj, mpCollisionPlayYakumonoAnim,
                         nGCProcessKindFunc, 4);
    }
    else
    {
        /* grdisplay.c:212-215, the arm layer 1 takes when it has no
         * animation of its own -- Yoshi's Island, Mushroom Kingdom, Dream
         * Land, Hyrule. Their platforms are moved by their own gr/ logic
         * through mpCollisionSetYakumonoPosID, which computes the speeds
         * itself; running the yakumono animator over them instead would
         * zero those speeds every tic and stop the platforms carrying
         * anybody. */
        gcAddGObjProcess(ground_gobj, mpCollisionAdvanceUpdateTic,
                         nGCProcessKindFunc, 4);
        gcAddGObjProcess(ground_gobj, gcPlayAnimAll, nGCProcessKindFunc, 4);
    }
    /* Either way, once, before the first frame is drawn: what puts each
     * joint on its script's first keyframe rather than leaving it at the
     * bind pose for a tic. */
    gcPlayAnimAll(ground_gobj);

    /* gr/grcommonsetup.c:29-31, in the game's own order and for the
     * game's own reason: both of these read the yakumono DObjs, so they
     * belong after the layers are built and not at the tail of
     * mpCollisionLoadGeometry, since the second one
     * needs DObjs that must exist already. */
    {
        /* mpCollisionClearYakumonoAll is the one of the two that cannot
         * be called: its loop is bounded by
         * `gMPCollisionGroundData->gr_desc[1].dobjdesc`
         * (mpcollision.c:4029-4039), the layer's DObjDesc array, and the
         * port has no MPGroundData holding one -- the first read resets
         * the console. mpcommon.c open-codes the same loop over the
         * yakumono COUNT for exactly this reason; this is that loop,
         * where the game does it. mpCollisionInitYakumonoAll below is
         * called for real: it reads only gMPCollisionGeometry and the
         * DObjs, both of which are now what the game would have. */
        int i;

        for (i = 0; i < gMPCollisionYakumonosNum; i++)
        {
            gMPCollisionYakumonoDObjs->dobjs[i]->user_data.s =
                nMPYakumonoStatusNone;
        }
        gMPCollisionUpdateTic = 0;
    }
    mpCollisionInitYakumonoAll();
#endif

    /* gr/grcommonsetup.c:24 grMainSetupMakeGround: the stage's own ground
     * GObj, which is where a stage keeps its hazard. The game indexes
     * dGRMainSetupProcMakeList by gkind; the port has one stage's logic
     * ported (src/dc/grhyrule.c), so the table read is this
     * test. It sits after mpCollisionAdvanceUpdateTic for the same reason
     * the game puts it after mpCollisionClearYakumonoAll: the twister
     * asks the collision tables where its own floor line is before it
     * makes anything. Stage logic for the other kind goes on this
     * test as it arrives. */
    if (gSCManagerBattleState != NULL)
    {
        if (gSCManagerBattleState->gkind == nGRKindHyrule)
        {
            grHyruleMakeGround();
        }
        else if (gSCManagerBattleState->gkind == nGRKindJungle)
        {
            grJungleMakeGround();
        }
        else if (gSCManagerBattleState->gkind == nGRKindZebes)
        {
            grZebesMakeGround();
        }
        else if (gSCManagerBattleState->gkind == nGRKindYoster)
        {
            grYosterMakeGround();
        }
        else if (gSCManagerBattleState->gkind == nGRKindPupupu)
        {
            grPupupuMakeGround();
        }
        else if (gSCManagerBattleState->gkind == nGRKindInishie)
        {
            grInishieMakeGround();
        }
        else if (gSCManagerBattleState->gkind == nGRKindCastle)
        {
            grCastleMakeGround();
        }
        else if (gSCManagerBattleState->gkind == nGRKindYamabuki)
        {
            grYamabukiMakeGround();
        }
        else if (gSCManagerBattleState->gkind == nGRKindSector)
        {
            grSectorMakeGround();
        }
        /* Race to the Finish. It is on this test and not
         * on a bonus-stage one of its own because `nGRKindBonus3` is
         * among the COMMON stage kinds -- see src/dc/grbonus3.c.
         * Its maker returns NULL, which this chain
         * has never read. */
        else if (gSCManagerBattleState->gkind == nGRKindBonus3)
        {
            grBonus3MakeGround();
        }
        /* The bonus stages. The twenty-four
         * courses are two RANGES rather than kinds, and unlike every arm
         * above them the functions are not gr/'s at all: the decomp's
         * gr/grmainsetup.c tests the same two ranges in the same order
         * and calls the SCENE's own. Board the Platforms is first
         * because its range is the higher one. */
        else if (gSCManagerBattleState->gkind >= nGRKindBonus2Start)
        {
            sc1PBonusStageInitBonus2();
        }
        else if ((gSCManagerBattleState->gkind >= nGRKindBonus1Start) &&
                 (gSCManagerBattleState->gkind <= nGRKindBonus1End))
        {
            sc1PBonusStageMakeBonus1Ground();
        }
    }
    /* gr/grcommonsetup.c:33 itManagerMakeAppearActor: the item SPAWNER,
     * in the game's own place at the tail -- after the ground, because it
     * reads the stage's `nMPMapObjKindItem` positions to decide where an
     * item may appear.     *
     * It returns a GObj the game ignores (the scene ejects it on the way
     * out); what matters is the process it installs, which is what makes
     * items appear on their own once the battle starts and the player's
     * item toggles allow it. */
    itManagerMakeAppearActor();

    efGroundMakeAppearActor();

    return ground_gobj;
}

/* Training mode's flat background (sc1PTrainingModeLoadWallpaper,
 * src/dc/sc1ptrainingmode.c): the game writes a sprite out of relocData
 * 26, 27 or 28 over MPGroundData.wallpaper, and grWallpaperMakeDecideKind
 * then draws it with grWallpaperMakeStatic. The Stage keeps its own
 * texture (the stage select still wants it); this is the replacement,
 * owned by the training scene's sprite bank, and the overlay-2 clear
 * forgets it at every scene change. */
static const DCSpriteTex *sStageWallpaperOverride;
static pvr_poly_hdr_t sStageWallpaperOverrideHdr;

void stage_set_wallpaper_override(const DCSpriteTex *tex)
{
    pvr_poly_cxt_t cxt;

    sStageWallpaperOverride = NULL;

    if ((tex == NULL) || (tex->txr == NULL) ||
        (tex->fmt != nDCSpriteTexFmtARGB1555))
    {
        dbglog(DBG_WARNING, "stage: training wallpaper not usable; "
               "the stage's own stays\n");
        return;
    }
    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_ARGB1555,
                     tex->texw, tex->texh, tex->txr, PVR_FILTER_BILINEAR);
    cxt.txr.uv_clamp = PVR_UVCLAMP_UV;
    cxt.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&sStageWallpaperOverrideHdr, &cxt);
    sStageWallpaperOverride = tex;

    dbglog(DBG_INFO, "stage: training wallpaper %ux%u over the stage's own\n",
           (unsigned)tex->imgw, (unsigned)tex->imgh);
}

/* CObj.func_camera, which gcRunFuncCamera calls once the camera's
 * matrices are set and before any GObj draws (objdisplay.c:3212-3218) --
 * where the game puts its own camera-attached drawing. The wallpaper is
 * opaque, so it takes the first of the back end's two passes. */
static void stage_proc_camera_wallpaper(CObj *cobj, s32 dl_id)
{
    (void)dl_id;

    if (gcGetDrawList() != PVR_LIST_OP_POLY)
    {
        return;
    }
    stage_draw_wallpaper(sStage, gSCManagerBattleState->gkind,
                         cobj->vec.eye.x, cobj->vec.eye.y,
                         cobj->vec.eye.z, cobj->vec.at.x, cobj->vec.at.y,
                         cobj->vec.at.z);
}

/* gr/grwallpaper.c:267 grWallpaperMakeDecideKind: pick the background
 * this stage wants and hang it on the wallpaper camera.
 *
 * DIVERGES twice. There is no wallpaper camera -- gmCameraMakeWallpaperCamera
 * is a second CObj drawing a scrolling SObj -- so the port draws the one baked quad the .stg carries from the battle
 * camera's own func_camera. That moves the call after
 * gmCameraMakeBattleCamera in the scene's sequence, where the game has
 * it before, because the camera it attaches to is the one that has to
 * exist first. And the kind is not decided here: a stage has exactly the one
 * wallpaper the exporter found for it; how it moves is decided per frame
 * instead, by src/dc/grwallpaper.h's stage_wallpaper_place. Race to
 * the Finish's arm (grWallpaperMakeBonus3) is a black fill rectangle
 * and nothing else; that stage ships no wallpaper, so the return below
 * leaves the PVR's black background plane in its place. */
void grWallpaperMakeDecideKind(void)
{
    /* gr/grwallpaper.c:293-296: Final Destination's arm makes the boss
     * background before the common wallpaper, and does not replace it --
     * the stage keeps its baked quad and the boss trees are drawn over
     * it by their own two cameras. */
    if ((sStage != NULL) && (gSCManagerBattleState != NULL) &&
        (gSCManagerBattleState->gkind == nGRKindLast))
    {
        sc1PGameBossInitWallpaper();
    }
    /* gr/grwallpaper.c:269-273: Training swaps in its flat background
     * first, and draws it static (stage_draw_wallpaper, below). */
    if (gSCManagerSceneData.scene_curr == nSCKind1PTrainingMode)
    {
        sc1PTrainingModeLoadWallpaper();
    }
    if (sStage == NULL || gGMCameraGObj == NULL ||
        (sStage->wp_txr == NULL && sStageWallpaperOverride == NULL))
    {
        return;
    }
    CObjGetStruct(gGMCameraGObj)->func_camera = stage_proc_camera_wallpaper;
}

/* gr/grwallpaper.c:335-347 grWallpaperResumeProcessAll, verbatim: let every wallpaper actor run again. Its one caller is
 * ifCommon1PGameInterfaceProcSet, the freeze a 1P match ends on.
 *
 * In a battle this sweeps nothing, because the port's own
 * grWallpaperMakeDecideKind above draws the stage's wallpaper as one
 * baked quad from the battle camera and makes no GObj for it. It is
 * here rather than cut because Final Destination's boss background IS
 * a wallpaper GObj (src/dc/sc1pgameboss.c), and it is the one the 1P
 * game would end on. */
void grWallpaperResumeProcessAll(void)
{
    GObj *gobj = gGCCommonLinks[nGCCommonLinkIDWallpaper];

    while (gobj != NULL)
    {
        if (gobj->id == nGCCommonKindWallpaper)
        {
            gcResumeGObjProcessAll(gobj);
        }
        gobj = gobj->link_next;
    }
}

/* Wallpaper depth: behind everything real, but in front of the PVR
 * background plane -- which KOS puts at PVR_MIN_Z (1e-4) with depth
 * write on, so main.c sets the zclip under this or the background plane
 * occludes the wallpaper (and any geometry beyond 10000 units). Both
 * values, and the backdrop stack between this and the 3D, are in
 * lbcommon.h. */
#define WP_Z LB_Z_WALLPAPER

/* Where the sprite goes is src/dc/grwallpaper.h's, per the stage's
 * wallpaper kind; this turns it into the one quad. N64 screen coords
 * double to 640x480. */
void stage_draw_wallpaper(Stage *st, s32 gkind, float eye_x, float eye_y,
                          float eye_z, float at_x, float at_y, float at_z)
{
    float px, py, scale, w, h;
    float x0, y0, x1, y1, u1, v1;
    pvr_vertex_t v;
    int i;
    const pvr_poly_hdr_t *hdr = &st->wp_hdr;
    uint16_t texw = st->wp_texw, texh = st->wp_texh;
    uint16_t imgw = st->wp_w, imgh = st->wp_h;

    if (sStageWallpaperOverride != NULL)
    {
        /* grwallpaper.c:271-272: the training background, always
         * grWallpaperMakeStatic's placement whatever the stage */
        hdr = &sStageWallpaperOverrideHdr;
        texw = sStageWallpaperOverride->texw;
        texh = sStageWallpaperOverride->texh;
        imgw = sStageWallpaperOverride->imgw;
        imgh = sStageWallpaperOverride->imgh;
        px = 10.0f;
        py = 10.0f;
        scale = 1.0f;
    }
    else if (!st->wp_txr)
        return;
    else stage_wallpaper_place(gkind, eye_x - at_x, eye_y - at_y,
                               eye_z - at_z, &px, &py, &scale);
    w = 300.0f * scale;
    h = 220.0f * scale;

    x0 = px * 2.0f;
    y0 = py * 2.0f;
    x1 = (px + w) * 2.0f;
    y1 = (py + h) * 2.0f;
    u1 = (float)imgw / (float)texw;
    v1 = (float)imgh / (float)texh;

    pvr_prim((void *)hdr, sizeof(*hdr));
    memset(&v, 0, sizeof(v));
    v.argb = 0xFFFFFFFF;
    v.z = WP_Z;
    for (i = 0; i < 4; i++)
    {
        v.flags = (i == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = (i & 2) ? x1 : x0;
        v.y = (i & 1) ? y0 : y1;
        v.u = (i & 2) ? u1 : 0.0f;
        v.v = (i & 1) ? 0.0f : v1;
        pvr_prim(&v, sizeof(v));
    }
}

/* The bzero arm of syDmaLoadOverlay for overlay 2, of which the gr/ files
 * this one stands in for are a part: gr/grcommonsetup, gr/grwallpaper and
 * the nine gr/grcommon/ stages all have noload segments in it
 * (smashbrothers.us.yaml). src/dc/overlay.c calls it on the way into every
 * scene that loads overlay 2.
 *
 * gGRStages and sGRStageRefs are deliberately not here, and they are the
 * reason this file needs a written-out clear rather than a linker
 * segment: the port's file holds both halves. sGroundData and sStage are
 * the overlay's, made fresh by stage_bind at every battle. The table and
 * its count are the port's stand-in for the map files themselves -- data
 * the N64 keeps in the ROM and DMAs -- and a hold taken before a scene
 * starts (src/dc/db.c's boot stage) spans this clear, so clearing the
 * table would free nothing and lose the stage the battle is about to
 * play on. grStageRelease is what empties it. tools/check/overlay_check.py
 * carries both in EXCLUDE with that reason. */
void grOverlayLoad(void)
{
    OVERLAY_CLEAR(sGroundData);
    OVERLAY_CLEAR(sStage);
    OVERLAY_CLEAR(gGRCommonStruct);
    /* the training background's bank is the training scene's, freed by
     * sprite_bank_release_all at the same scene change */
    OVERLAY_CLEAR(sStageWallpaperOverride);
    OVERLAY_CLEAR(sStageWallpaperOverrideHdr);
#ifndef FT_HOSTTEST
    /* Scratch, rewritten end to end before every read (stage_layer_anim_play
     * NULLs the whole array first), so clearing it changes nothing -- but
     * the rule is that a scene module's statics are zero on every entry,
     * and an exception argued file by file is how one gets missed. */
    OVERLAY_CLEAR(sStageLayerAnim);
    /* Not scratch, unlike sStageLayerAnim: mp/ holds pointers out of this
     * array for the life of the match, so a stale entry from the previous
     * stage is a wild DObj the collision code would follow. */
    OVERLAY_CLEAR(sStageLayerJoints);
#endif

    /* ... and the one stage logic file that owns a pack of its own:
     * src/dc/grsector.c loads the Arwing out of `efarwing.mdl`, and this
     * is where it gives it back. The decomp has no such hook -- its
     * Arwing tree is a pointer into a buffer the FIGHTER manager owns --
     * so the function is the port's, and it is called here because the
     * gr/ files are overlay 2. */
    grSectorOverlayLoad();
}
