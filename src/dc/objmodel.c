/* objmodel.c -- see objmodel.h. */
#include "objmodel.h"

#include <sys/obj.h>
#include <sys/debug.h>
#include <sys/taskman.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef SSB_NO_DRAW
#include <dc/pvr.h>
#include <kos.h>
#include "dcpvr.h"                  /* dc_pvr_vram_fence */
#else
/* The host cross-test builds the tree and never draws it; PVR_LIST_*
 * only has to exist, and the draw half below is compiled out. */
#define PVR_LIST_OP_POLY 0
#endif

#include "clip.h"
#include "mtx.h"
#include "objpvr.h"
#include "taskman.h"               /* syTaskmanGeneralHeapEpoch */
#include "lbcommon.h"
#include "ftcommon.h"
#include "perf.h"
#include "ftshade.h"

#include <mp/map.h>                 /* gMPCollisionLightAngleX/Y */
#include <sc/scsubsys/scsubsys.h>   /* scSubsysFighterGetLightAngleX/Y */

/* A fighter takes its scene's own light (fighter_light below); only a
 * model that is not one sees what a scene set here. */
static float sLight[3] = { 0.4f, 0.6f, 0.7f };

static unsigned sTris;
/* of which, the magnifying glass's: the fighter inside it and the
 * handle. The serial log's only way to tell the copy from the fighter it
 * stands in for, since the two have the same triangles. */
static unsigned sTrisMagnify;
/* and the microseconds the glass's two draws took, the clip included */
static unsigned sUsMagnify;

void dc_model_set_light(float x, float y, float z)
{
    sLight[0] = x;
    sLight[1] = y;
    sLight[2] = z;
}

void dc_model_set_light_angles(float angle_x, float angle_y)
{
    ft_light_dir(angle_x, angle_y, sLight);
}

unsigned dc_model_tris(void) { return sTris; }
unsigned dc_model_tris_magnify(void) { return sTrisMagnify; }
unsigned dc_model_us_magnify(void) { return sUsMagnify; }
static sb32 sDemoOpaque;

void dc_model_set_demo_opaque(sb32 on) { sDemoOpaque = on; }

/* See objmodel.h: the layered draw walks and transforms again in the
 * translucent pass, where it submits. */
static sb32 sLayeredRewalk;

void dc_model_set_layered_rewalk(sb32 on) { sLayeredRewalk = on; }

void dc_model_tris_reset(void) { sTris = sTrisMagnify = sUsMagnify = 0; }

/* The DObj payload's proc_submit (objpvr.h). The N64 would run a display
 * list here against the matrix the RSP has on its stack; the port instead
 * hands the matrix to the model, because src/dc/fighter.c transforms a
 * model's vertices in one pass over the whole pack rather than per part
 * -- a vertex's clip position depends only on itself and its joint, and
 * Mario's 320 triangles reference his 233 vertices 960 times (see the
 * note on Fighter.vclip in fighter.h). So this writes the joint's matrix
 * and the model is drawn once, after the walk, by dc_model_proc_display.
 *
 * Which means model->mtx[] holds joint->world here, where
 * src/dc/fighter.c's own build_mtx leaves joint->model. That is the
 * point: the DObj tree carries the placement,
 * and fighter_frame is handed the view alone.
 *
 * A joint of -1 is the tree's stand (dc_model_add_root): it has no pack
 * joint and no geometry, and only exists so the walk has somewhere to
 * put the fighter's position. */
static void dc_joint_submit(DCDisplay *disp, const float *mtx)
{
    Fighter *f = disp->model;

    if (disp->joint < 0)
    {
        return;
    }
    memcpy(f->mtx[disp->joint], mtx, sizeof(f->mtx[0]));
    f->part_cur[disp->joint] = (u8)disp->part;

    /* The walk reached this joint, so it is not hidden. See
     * Fighter.joint_hide: the mask is set to all ones ahead of every
     * walk in this file and cleared here a joint at a time. */
    if (f->joint_hide != 0 && disp->joint < 64)
    {
        f->joint_hide &= ~((uint64_t)1 << disp->joint);
    }
}

/* DCDisplay.proc_material (objpvr.h): read the DObj's live MObj chain
 * into the model, where draw_batches will find it.
 *
 * The N64 emits GBI here and the display list that follows draws with it.
 * The port cannot: dc_joint_submit only records the joint's matrix, and
 * the model is drawn once after the whole walk (see the note there). So
 * the colours are stored per MObj rather than left as pipeline state,
 * and a batch says which MObj to take them from -- which is what the
 * pack's off_batch table is for.
 *
 * The chain's order is gcAddMObjForDObj's append order, which is the
 * order dc_model_add_mobjs added them in, which is the pack's. */
#ifdef SSB_NO_DRAW
/* No renderer in the host link and so no fighter_mobj_color; nothing in
 * that build attaches an MObj either, since a pack cannot be loaded. */
static void dc_joint_material(DCDisplay *disp, MObj *chain)
{
    (void)disp;
    (void)chain;
}
#else
/* A UV translation folded into [0, 2). A scrolling track only ever grows
 * (the fog's trav runs ten units in twenty seconds, and on for as long as
 * the scene does), and a float UV that large has lost the fraction the
 * picture is actually on. A period of 2 is a whole number of repeats for
 * a wrapping tile and a whole mirror period for a mirrored one, so the
 * picture drawn is the same. */
static float uv_wrap(float d)
{
    return d - 2.0f * floorf(d * 0.5f);
}

static void dc_joint_material(DCDisplay *disp, MObj *chain)
{
    Fighter *f = disp->model;
    MObj *mobj;
    int k = 0;

    for (mobj = chain; mobj != NULL; mobj = mobj->next, k++)
    {
        DCMObjColor *c = fighter_mobj_color(f, disp->joint, k);

        if (c == NULL)
        {
            continue;
        }
        c->prim = mobj->sub.primcolor.pack;
        c->env = mobj->sub.envcolor.pack;
        c->light1 = mobj->sub.light1color.pack;
        c->light2 = mobj->sub.light2color.pack;
        /* Which sprite of the MObj's array is current. On the N64 this
         * chooses a G_SETTIMG address (objdisplay.c:1288); here it
         * chooses one of the poly headers src/dc/fighter.c compiled for
         * the batch, one per frame. The MObj carries it because
         * objanim.c's material parser put it there -- track
         * nGCAnimTrackTextureIDCurrent -- and that parser is the
         * decomp's own, compiled unmodified.
         *
         * A PALETTE-flagged MObj instead picks its frame by palette_id
         * (objdisplay.c:1182-1184: mobj->sub.palettes[(s32)palette_id]
         * loaded as the TLUT over the same CI4 tile), a different array
         * than texture_id_curr's sprite one -- pack_slash's own bake
         * refuses a MObjSub that sets both, so the two never compete for
         * this field. The fireball is the one model that reaches this:
         * It bakes its two palettes (Mario's orange, Luigi's
         * green) as tex_count 2 the same way the slash's frames are. */
        c->tex = (mobj->sub.flags & MOBJ_FLAG_PALETTE)
                     ? (uint16_t)mobj->palette_id
                     : mobj->texture_id_curr;
        /* The second tile of a cross-fade, and the fade itself. The N64
         * loads that tile from sprites[texture_id_next] and lerps the two
         * by the primitive colour's LOD byte (objdisplay.c:1243, 1283);
         * here both tiles are frames of the same pack run, so the index is
         * all draw_batches needs. Its UV moves with the scrollu/scrollv
         * the material parser has just written into the MObjSub -- the
         * port translates the coordinates because it cannot re-load a
         * tile -- against the baseline the bake froze in. */
        c->tex2 = mobj->texture_id_next;
        c->lfrac = mobj->lfrac;
        {
            /* Which MObj this is, in the pack's own numbering: the colour
             * array and the MObjSub array are the same list in the same
             * order (dc_model_add_mobjs_alt built both), so the slot
             * fighter_mobj_color just handed back is the index. */
            const FPackMObjSub *ps = &f->mobj_subs[c - f->mobj_color];

            c->du = uv_wrap((mobj->sub.scrollu - ps->uv_base[0]) *
                            ps->uv_scale[0]);
            c->dv = uv_wrap((mobj->sub.scrollv - ps->uv_base[1]) *
                            ps->uv_scale[1]);
            /* The first tile's origin moves the same way, with trau and
             * trav (objdisplay.c:1353-1382), and every pass is drawn
             * through it. */
            c->du0 = uv_wrap((mobj->sub.trau - ps->uv0_base[0]) *
                             ps->uv0_scale[0]);
            c->dv0 = uv_wrap((mobj->sub.trav - ps->uv0_base[1]) *
                             ps->uv0_scale[1]);

            /* MOBJ_FLAG_PALETTE dynamic recolour: a script
             * that steps palette_id through more than a couple of values
             * (Run's crash, Clash's wallpaper quadrants) never got a
             * static bank per step the way the fireball's two did --
             * FPACK_PAL_BANKS is 64 and shared by every pack resident at
             * once, and Run's crash alone would need 130. Instead
             * dpal_bank names the ONE bank every batch this MObj draws
             * was baked against, and this rewrites that bank's 16
             * entries in place from the pack's own raw frame data,
             * exactly the DMA the N64 does on its own TLUT swap --
             * cheap enough (16 pvr_set_pal_entry calls) that the only
             * guard needed is not repeating it for a frame already
             * there. */
            if (ps->dpal_count > 1)
            {
                int fr = (mobj->palette_id <= 0.0F) ? 0
                       : (mobj->palette_id >= (f32)ps->dpal_count)
                             ? ps->dpal_count - 1
                             : (int)mobj->palette_id;

                if (fr != (int)c->dpal_frame)
                {
                    const FPackDPal *dp = &f->dpals[ps->dpal_first + fr];
                    int b;

                    /* Palette RAM is video RAM like a texture's: this
                     * runs in the draw's tree walk, after the draw's own
                     * pvr_wait_ready started the previous frame's render,
                     * which may still be reading the bank about to change
                     * (dcpvr.h dc_pvr_vram_fence).
                     * Only paid when the frame actually moved, which the
                     * check above already limits to script pacing, not
                     * every frame. */
                    dc_pvr_vram_fence();
                    for (b = 0; b < 16; b++)
                        pvr_set_pal_entry((uint32_t)
                            ((f->pal_base + ps->dpal_bank) * 16 + b),
                            dp->c[b]);
                    c->dpal_frame = (int16_t)fr;
                }
            }
        }
        c->flags = mobj->sub.flags;
        c->live = 1;
    }
}
#endif

int dc_model_add_mobjs(GObj *gobj, Fighter *model, f32 anim_frame)
{
    return dc_model_add_mobjs_alt(gobj, model, anim_frame, 0);
}

/* `alt` picks one of the pack's whole MatAnimJoints (FPackMObjs.alt_count).
 * The game has one per relocData block and swaps which the EFDesc names
 * before it makes the effect -- ef/efmanager.c:4808 writes
 * dEFManagerDeadExplodeMatAnimJoints[player] into
 * dEFManagerDeadExplodeEffectDesc.o_matanim_joint -- and the port cannot,
 * because the descriptor's offsets are all zero here and the scripts live
 * in the pack. So the four alternates are baked side by side and this is
 * which of them plays. Every other pack carries one, and 0 is it. */
int dc_model_add_mobjs_alt(GObj *gobj, Fighter *model, f32 anim_frame,
                           int alt)
{
    const FPackMObjs *mo = model->mobjs;
    MObjSub *subs;
    MObjSub **rows;
    MObjSub ***p_subs;
    AObjEvent32 **scripts;
    AObjEvent32 ***p_scripts;
    u32 n, j, k, cursor;

    if (mo == NULL)
    {
        return 0;
    }
    /* A pack with MObjs but no MatAnimJoint -- the spotlight -- leaves
     * every entry -1 below, and gcAddMatAnimJointAll skips a NULL script
     * per MObj (objanim.c:207), so the one call does for both. */
    n = mo->mobj_count;

    if (alt < 0 || (u32)alt >= mo->alt_count)
    {
        alt = 0;
    }

    /* The two shapes a relocData file would have held: `MObjSub *[]` per
     * DObj with a NULL terminator, indexed by `MObjSub **[]` in tree
     * order, and the same for the scripts. Built so that the walk that
     * consumes them is the decomp's own (sys/objanim.c gcAddMObjAll,
     * gcAddMatAnimJointAll) and not a lookalike -- which is the whole
     * reason the pack keeps the MObjs per joint and in order.
     *
     * They outlive neither walk: gcAddMObjForDObj copies the MObjSub
     * into the MObj (sys/objman.c:1322) and gcAddMObjMatAnimJoint keeps
     * the script, which is in the pack. So they come from malloc and go
     * back below. Out of the scene heap, which only empties between
     * scenes, every item, weapon and effect spawned in a match would
     * keep its copy: 40 KB in a four-CPU minute and a half. */
    subs = malloc(sizeof(MObjSub) * n);
    rows = malloc(sizeof(MObjSub *) * (n + model->hd->joint_count));
    p_subs = malloc(sizeof(MObjSub **) * model->hd->joint_count);
    scripts = malloc(sizeof(AObjEvent32 *) * n);
    p_scripts = malloc(sizeof(AObjEvent32 **) * model->hd->joint_count);

    if ((subs == NULL) || (rows == NULL) || (p_subs == NULL) ||
        (scripts == NULL) || (p_scripts == NULL))
    {
        syDebugPrintf("objmodel: %s: no memory for %u MObjs\n",
                      model->hd->name, (unsigned)n);
        free(subs);
        free(rows);
        free(p_subs);
        free(scripts);
        free(p_scripts);
        return 0;
    }

    for (k = 0; k < n; k++)
    {
        const FPackMObjSub *ps = &model->mobj_subs[k];
        s32 e = ((const s32 *)((const u8 *)model->blob + mo->off_entry))
            [(u32)alt * n + k];

        /* Everything the port does not carry is the zero the game's own
         * struct holds for an emblem: no palettes and no tile of its
         * own. The exporters are what make that true, by refusing a pack
         * whose MObjSub sets those flags. `sprites` stays NULL even for
         * the slash, whose MObjs do have one: nothing here dereferences
         * it, because the array has been baked into the pack's texture
         * table and what the parser leaves behind is the *index*
         * (MObj.texture_id_curr), which is all the renderer needs. */
        memset(&subs[k], 0, sizeof(subs[k]));
        subs[k].flags = ps->flags;
        subs[k].prim_l = ps->prim_l;
        subs[k].primcolor.pack = ps->primcolor;
        subs[k].envcolor.pack = ps->envcolor;
        subs[k].blendcolor.pack = ps->blendcolor;
        subs[k].light1color.pack = ps->light1color;
        subs[k].light2color.pack = ps->light2color;
        scripts[k] = (e < 0) ? NULL
            : (AObjEvent32 *)((u32 *)((u8 *)model->blob + mo->off_words) + e);
    }
    cursor = 0;
    for (j = 0; j < model->hd->joint_count; j++)
    {
        s32 first = model->mobj_joint[j * 2];
        s32 count = model->mobj_joint[j * 2 + 1];

        if (count == 0)
        {
            p_subs[j] = NULL;
            p_scripts[j] = NULL;
            continue;
        }
        p_subs[j] = &rows[cursor];
        p_scripts[j] = &scripts[first];

        for (k = 0; k < (u32)count; k++)
        {
            rows[cursor++] = &subs[first + k];
        }
        rows[cursor++] = NULL;
    }
#if defined(DB_ANIM_CHECK) && !defined(SSB_NO_DRAW)
    for (k = 0; k < n; k++)
    {
        if ((scripts[k] != NULL) &&
            (fighter_animcheck(scripts[k], model->hd->name) < 0))
        {
            dbglog(DBG_ERROR, "animcheck: %s MObj %u of %u, blob %p+%lu, "
                   "entry %ld\n", model->hd->name, (unsigned)k, (unsigned)n,
                   model->blob, (unsigned long)model->blob_size,
                   (long)((const s32 *)((const u8 *)model->blob +
                                        mo->off_entry))[(u32)alt * n + k]);
        }
    }
#endif
    gcAddMObjAll(gobj, p_subs);
    gcAddMatAnimJointAll(gobj, p_scripts, anim_frame);

    free(subs);
    free(rows);
    free(p_subs);
    free(scripts);
    free(p_scripts);

    return (int)n;
}

/* dc_model_add_mobjs_alt, rooted at an arbitrary DObj rather than at a
 * GObj: what a stage's GRAFT needs. The game's own
 * lbCommonAddMObjForTreeDObjs walks the CLOUD's subtree -- it is handed
 * the DObj the cloud hangs off -- while gcAddMObjAll starts at
 * DObjGetStruct(gobj), which for a graft is the whole stage object and
 * would read past the pack's own per-joint table. A ground actor is the
 * same problem one level up: its tree hangs off a bare wrapper DObj that
 * is no joint of the pack at all (dcGroundMakeEffect), so the walk has to
 * be given each joint's own DObj.
 *
 * `joint` is which joint of the pack `root` is -- a graft is 0, a ground
 * actor's caller passes each in turn. MObjs go on in the pack's order
 * within that joint, which is the order gcAddMObjAll would have added
 * them in.
 *
 * The MObjSub field fill is dc_model_add_mobjs_alt's own, repeated
 * because that one fills an array indexed by the pack and this one makes
 * each MObjSub as it goes. */
int dc_model_add_mobjs_dobj(DObj *root, Fighter *model, f32 anim_frame,
                            int alt, int joint)
{
    const FPackMObjs *mo = model->mobjs;
    s32 first, count, k;

    if ((mo == NULL) || (root == NULL) || (joint < 0) ||
        ((u32)joint >= model->hd->joint_count))
    {
        return 0;
    }
    if (alt < 0 || (u32)alt >= mo->alt_count)
    {
        alt = 0;
    }
    first = model->mobj_joint[joint * 2];
    count = model->mobj_joint[joint * 2 + 1];

    for (k = 0; k < count; k++)
    {
        const FPackMObjSub *ps = &model->mobj_subs[first + k];
        s32 e = ((const s32 *)((const u8 *)model->blob + mo->off_entry))
            [(u32)alt * mo->mobj_count + first + k];
        MObjSub sub;            /* copied into the MObj, so a local */
        MObj *mobj;

        memset(&sub, 0, sizeof(sub));
        sub.flags = ps->flags;
        sub.prim_l = ps->prim_l;
        sub.primcolor.pack = ps->primcolor;
        sub.envcolor.pack = ps->envcolor;
        sub.blendcolor.pack = ps->blendcolor;
        sub.light1color.pack = ps->light1color;
        sub.light2color.pack = ps->light2color;

        mobj = gcAddMObjForDObj(root, &sub);

        if ((e >= 0) && (mobj != NULL))
        {
            gcAddMObjMatAnimJoint(mobj,
                (AObjEvent32 *)((const u8 *)model->blob + mo->off_words +
                                (u32)e * 4), anim_frame);
        }
    }
    return count;
}

#ifndef SSB_NO_DRAW
/* See objmodel.h. The graft's own proc_submit: the walk has just
 * composed this DObj's matrix and read its live MObj chain into the pack
 * (gcDrawMObjForDObj runs ahead of gcSubmitDObj, which is the order the
 * material a graft animates arrives in), so all that is left is to
 * transform and submit -- now, before the walk reaches the next cloud,
 * because the pack's matrices and clip positions are one shared set.
 *
 * joint_hide is set to "everything but this DObj's joint", which is the
 * walk's own answer to which part of the tree it reached: a pack whose
 * graft is several joints draws one of them per DObj rather than all of
 * them once per DObj. A single-joint graft -- Yoshi's cloud, the only
 * one -- is unaffected. */
static void dc_graft_submit(DCDisplay *disp, const float *mtx)
{
    Fighter *f = disp->model;

    if (disp->joint < 0)
    {
        return;
    }
    memcpy(f->mtx[disp->joint], mtx, sizeof(f->mtx[0]));

    f->joint_hide = ~(uint64_t)0;
    if (disp->joint < 64)
    {
        f->joint_hide &= ~((uint64_t)1 << disp->joint);
    }
    fighter_frame(f, gcGetViewF(), gcGetProjF(),
                  sLight[0], sLight[1], sLight[2]);
    if (gcGetDrawList() == PVR_LIST_OP_POLY)
    {
        sTris += fighter_draw(f);
    }
    else if (gcGetDrawList() == PVR_LIST_PT_POLY)
    {
        sTris += fighter_draw_pt(f);
    }
    else
    {
        sTris += fighter_draw_tr(f);
    }
    f->joint_hide = 0;
}
#endif /* !SSB_NO_DRAW */

/* See objmodel.h. ft/ftdisplaymain.c:1205-1216 and :1228-1231:
 * a fighter in hitlag has its whole draw jogged by one of
 * dFTDisplayMainShufflePositions' offsets, which the game pushes onto the
 * modelview stack with G_MTX_MUL -- a translation in world space, ahead
 * of the camera's view. The port's walk leaves world matrices in the pack
 * and the transform takes the view separately, so the jog is folded into
 * the view the fighter's models are transformed by. Its held item takes
 * the same jog through matrix kind 0x52 (src/dc/objdisplay.c). */
extern Vec2f dFTDisplayMainShufflePositions[/* */][4];

const float *dc_model_view_for(GObj *gobj)
{
    static mtx4_t shuffled;
    mtx4_t jog;
    FTStruct *fp;

    if (gobj == NULL || gobj->id != nGCCommonKindFighter)
    {
        return gcGetViewF();
    }
    fp = ftGetStruct(gobj);

    if (fp == NULL || fp->shuffle_tics == 0)
    {
        return gcGetViewF();
    }
    mtx_translate(jog,
                  dFTDisplayMainShufflePositions[fp->is_shuffle_electric][fp->shuffle_frame_index].x,
                  dFTDisplayMainShufflePositions[fp->is_shuffle_electric][fp->shuffle_frame_index].y,
                  0.0F);
    mtx_mul(shuffled, gcGetViewF(), jog);

    return shuffled;
}

void dc_model_graft_display(DObj *dobj)
{
#ifndef SSB_NO_DRAW
    if (dobj == NULL || dobj->dv == NULL)
    {
        return;
    }
    ((DCDisplay *)dobj->dv)->proc_submit = dc_graft_submit;
#else
    /* The host cross-test builds the graft's tree and never draws it;
     * what it checks is the tree and the MObj, not the submission. */
    (void)dobj;
#endif
}

/* The DCDisplay a GObj's model hangs off, which is NOT always the GObj's
 * own root DObj.
 *
 * The decomp's draw walks a GObj's DObj tree and reads each DObj's own
 * `dl` -- a display list per joint -- so it does not care which DObj in
 * the tree is which. The port's draw is the other shape: the whole model
 * is one baked `Fighter` pack, reached through ONE `DCDisplay` handle,
 * and a site that takes that handle off the root and stops if
 * the root has none would miss it.
 *
 * it/itmain.c's itMainSetFighterHold is what breaks that assumption. To
 * hang an item in a fighter's hand it PREPENDS a fresh DObj above the
 * item's own tree, points `item_gobj->obj` at the new one and gives it
 * the hand-tracking matrix (kind 0x52) -- so from that moment the GObj's
 * root is a bare joint with no display of its own, and the pack handle is
 * one level down. Reading only the root made every held item invisible:
 * the pickup worked, the item went in the hand, and nothing drew.
 *
 * Walking for it costs one pointer test per DObj on a tree that is a
 * handful of joints, and it is right whatever else splices a parent in
 * later. */
static DCDisplay *dc_model_display_of(DObj *root)
{
    DObj *dobj;

    for (dobj = root; dobj != NULL; dobj = gcGetTreeDObjNext(dobj))
    {
        if (dobj->dv != NULL)
        {
            return dobj->dv;
        }
    }
    return NULL;
}

Fighter *dc_model_of(GObj *gobj)
{
    DObj *root = DObjGetStruct(gobj);
    DCDisplay *disp;

    if (root == NULL)
    {
        return NULL;
    }
    disp = dc_model_display_of(root);

    return (disp != NULL) ? disp->model : NULL;
}

DObj *dc_model_add_root(GObj *gobj, Fighter *model)
{
    DCDisplay *disp = syTaskmanMalloc(sizeof(DCDisplay), 0x4);
    DObj *dobj;

    disp->proc_submit = dc_joint_submit;
    disp->proc_material = NULL;
    disp->model = model;
    disp->joint = -1;
    disp->part = 0;

    dobj = gcAddDObjRpyR(gobj, disp);

    if (dobj == NULL)
    {
        syDebugPrintf("objmodel: no DObj for %s's root\n", model->hd->name);
        return NULL;
    }
    dobj->translate.vec.f.x = dobj->translate.vec.f.y =
        dobj->translate.vec.f.z = 0.0F;
    dobj->rotate.vec.f.x = dobj->rotate.vec.f.y = dobj->rotate.vec.f.z = 0.0F;
    dobj->scale.vec.f.x = dobj->scale.vec.f.y = dobj->scale.vec.f.z = 1.0F;

    return dobj;
}

int dc_model_add_dobjs(GObj *gobj, DObj *parent, Fighter *model,
                       DObj **joints_out)
{
    DObj *made[FIGHTER_MAX_JOINTS];
    DCDisplay *disp;
    int n = (int)model->hd->joint_count;
    int tags = (model->parts != NULL) ? (int)model->parts->tag_count : 0;
    const uint32_t *mask = (model->attr != NULL) ? model->attr->setup_parts
                                                 : NULL;
    int i;

    if (n > FIGHTER_MAX_JOINTS)
    {
        syDebugPrintf("objmodel: %s has %d joints, max %d\n",
                      model->hd->name, n, FIGHTER_MAX_JOINTS);
        return -1;
    }
    /* One payload per joint, out of the scene heap: the object system's
     * own pools are for its own structs, and this is the port's. Every
     * joint gets one, built or not: a hidden part made later takes its
     * entry's (dc_model_hidden_payload).
     *
     * One set per model per scene heap, not per tree: a payload holds
     * the model and the joint and nothing of the GObj, so a second tree
     * off the same pack can hang off the first's. Cut fresh for every
     * tree, the heap -- which only empties between scenes -- lost a set
     * to every item, weapon and effect a match spawned. The fields are
     * written again below, which puts back a graft's proc_submit
     * (dc_model_graft_display) the way a fresh set would have had it. */
    if ((model->disp != NULL) &&
        (model->disp_epoch == syTaskmanGeneralHeapEpoch()))
    {
        disp = model->disp;
    }
    else
    {
        disp = syTaskmanMalloc(sizeof(DCDisplay) * (u32)(n + tags), 0x4);
        model->disp = disp;
        model->disp_epoch = syTaskmanGeneralHeapEpoch();
    }
    /* and one after them per model part, which ftParamSetModelPartID
     * puts on the joint in place of the part's display list
     * (dc_model_part_display) */
    for (i = 0; i < tags; i++)
    {
        dc_model_init_payload(&disp[n + i], model,
                              model->part_tags[i].joint);
        disp[n + i].part = i + 1;
    }

    for (i = 0; i < n; i++)
    {
        const FPackJoint *pj = &model->joints[i];
        DObj *dobj;
        /* The tree entry's own display list, as a part tag: 0 for a
         * joint without parts, and FPACK_PART_NONE where the entry has
         * no display list and the joint's parts only come on when
         * something sets one -- the DObj then has no payload, which is
         * the game's dl NULL (ftmanager.c's modelpart id -1). */
        int tree = (model->parts != NULL) ? model->part_joints[i].tree : 0;
        DCDisplay *dv = (tree == FPACK_PART_NONE) ? NULL : &disp[i];

        disp[i].proc_submit = dc_joint_submit;
        disp[i].proc_material = (model->mobjs != NULL) ? dc_joint_material
                                                       : NULL;
        disp[i].model = model;
        disp[i].joint = i;
        disp[i].part = (tree == FPACK_PART_NONE) ? 0 : tree;

        /* lb/lbcommon.c:1030-1032: a clear setup_parts bit's entry is
         * not built and its slot stays NULL. The mask is MSB first, entry
         * 32 onward in the second word. */
        if (mask != NULL &&
            !((i < 32) ? (mask[0] >> (31 - i)) & 1
                       : (mask[1] >> (63 - i)) & 1))
        {
            made[i] = NULL;
            if (joints_out != NULL)
            {
                joints_out[i] = NULL;
            }
            continue;
        }
        if (pj->parent >= 0 && made[pj->parent] == NULL)
        {
            syDebugPrintf("objmodel: %s joint %d hangs off joint %d, which "
                          "the mask leaves out\n",
                          model->hd->name, i, pj->parent);
            return -1;
        }
        /* The pack orders joints parent-before-child, so the parent's
         * DObj always exists by the time its children are read. A root
         * goes on the GObj (or under `parent`): gcAddDObjForGObj appends
         * it after any root already there as a sibling, which is how a
         * stage with three separate layers is one tree. */
        if (pj->parent < 0)
        {
            dobj = (parent != NULL) ? gcAddDObjChildRpyR(parent, dv)
                                    : gcAddDObjRpyR(gobj, dv);
        }
        else
        {
            dobj = gcAddDObjChildRpyR(made[pj->parent], dv);
        }
        if (dobj == NULL)
        {
            syDebugPrintf("objmodel: %s ran out of DObjs at joint %d\n",
                          model->hd->name, i);
            return -1;
        }
        dobj->translate.vec.f.x = pj->t[0];
        dobj->translate.vec.f.y = pj->t[1];
        dobj->translate.vec.f.z = pj->t[2];
        dobj->rotate.vec.f.x = pj->r[0];
        dobj->rotate.vec.f.y = pj->r[1];
        dobj->rotate.vec.f.z = pj->r[2];
        dobj->scale.vec.f.x = pj->s[0];
        dobj->scale.vec.f.y = pj->s[1];
        dobj->scale.vec.f.z = pj->s[2];

        made[i] = dobj;
        if (joints_out != NULL)
        {
            joints_out[i] = dobj;
        }
    }
    return n;
}

void *dc_model_hidden_payload(Fighter *model, int entry)
{
    DCDisplay *disp = model->disp;
    uint32_t b;

    if (disp == NULL || entry < 0 || entry >= (int)model->hd->joint_count)
    {
        return NULL;
    }
    /* a joint whose tree entry has no display list but whose parts do
     * (Samus's grapple beam): its part batches are not the entry's, and
     * the game's dl is NULL, which deals the joint part -1 */
    if (dc_model_has_parts(model, entry) &&
        model->part_joints[entry].tree == FPACK_PART_NONE)
    {
        return NULL;
    }
    for (b = 0; b < model->hd->batch_count; b++)
    {
        if (model->batches[b].joint == entry)
        {
            return &disp[entry];
        }
    }
    return NULL;
}

DCDisplay *dc_model_part_display(Fighter *model, int joint, int part_id,
                                 u8 *flags)
{
    const FPackPartJoint *pj;
    const FPackPartID *id;

    *flags = 0;

    if (model->disp == NULL || joint < 0 ||
        joint >= (int)model->hd->joint_count)
    {
        return NULL;
    }
    if (!dc_model_has_parts(model, joint))
    {
        return &model->disp[joint];
    }
    pj = &model->part_joints[joint];

    if (part_id < 0 || part_id >= pj->id_count)
    {
        syDebugPrintf("objmodel: %s joint %d has no model part %d\n",
                      model->hd->name, joint, part_id);
        return NULL;
    }
    id = &model->part_ids[pj->id_first + part_id];
    *flags = id->flags;

    if (id->tag == FPACK_PART_NONE)
    {
        return NULL;
    }
    if (id->tag == pj->tree)
    {
        return &model->disp[joint];
    }
    return &model->disp[model->hd->joint_count + id->tag - 1];
}

void dc_model_part_mobjs(Fighter *model, int joint, MObjSub ***mobjsubs,
                         AObjEvent32 ***main_matanim_joints)
{
    static MObjSub subs[DC_MODEL_PART_MOBJS];
    static MObjSub *sub_rows[DC_MODEL_PART_MOBJS + 1];
    static AObjEvent32 *scripts[DC_MODEL_PART_MOBJS];
    const FPackMObjs *mo = model->mobjs;
    s32 first, count, k;

    *mobjsubs = NULL;
    *main_matanim_joints = NULL;

    if ((mo == NULL) || (joint < 0) || ((u32)joint >= model->hd->joint_count))
    {
        return;
    }
    first = model->mobj_joint[joint * 2];
    count = model->mobj_joint[joint * 2 + 1];

    if (count <= 0)
    {
        return;
    }
    if (count > DC_MODEL_PART_MOBJS)
    {
        syDebugPrintf("objmodel: %s joint %d has %d part MObjs, %d kept\n",
                      model->hd->name, joint, count, DC_MODEL_PART_MOBJS);
        count = DC_MODEL_PART_MOBJS;
    }
    for (k = 0; k < count; k++)
    {
        const FPackMObjSub *ps = &model->mobj_subs[first + k];
        s32 e = ((const s32 *)((const u8 *)model->blob + mo->off_entry))
            [first + k];

        memset(&subs[k], 0, sizeof(subs[k]));
        subs[k].flags = ps->flags;
        subs[k].prim_l = ps->prim_l;
        subs[k].primcolor.pack = ps->primcolor;
        subs[k].envcolor.pack = ps->envcolor;
        subs[k].blendcolor.pack = ps->blendcolor;
        subs[k].light1color.pack = ps->light1color;
        subs[k].light2color.pack = ps->light2color;
        sub_rows[k] = &subs[k];
        scripts[k] = (e >= 0)
            ? (AObjEvent32 *)((const u8 *)model->blob + mo->off_words +
                              (u32)e * 4)
            : NULL;
    }
    sub_rows[count] = NULL;

    *mobjsubs = sub_rows;
    *main_matanim_joints = scripts;
}

sb32 dc_model_has_parts(const Fighter *model, int joint)
{
    return (model->parts != NULL) && (joint >= 0) &&
           (joint < (int)model->hd->joint_count) &&
           (model->part_joints[joint].id_count != 0);
}

void dc_model_init_payload(DCDisplay *disp, Fighter *model, int joint)
{
    disp->proc_submit = dc_joint_submit;
    disp->proc_material = (model->mobjs != NULL) ? dc_joint_material : NULL;
    disp->model = model;
    disp->joint = joint;
    disp->part = 0;
}

int dc_model_models_of(GObj *gobj, Fighter **out, int max)
{
    DObj *dobj;
    int n = 0;
    int i;

    for (dobj = DObjGetStruct(gobj); dobj != NULL;
         dobj = gcGetTreeDObjNext(dobj))
    {
        DCDisplay *disp = dobj->dv;

        if (disp == NULL || disp->model == NULL)
        {
            continue;
        }
#ifndef SSB_NO_DRAW
        /* a graft draws itself as the walk reaches it (dc_graft_submit);
         * drawn again as a whole, its pack would show only the last
         * instance the walk left in it */
        if (disp->proc_submit == dc_graft_submit)
        {
            continue;
        }
#endif
        for (i = 0; i < n; i++)
        {
            if (out[i] == disp->model)
            {
                break;
            }
        }
        if (i == n && n < max)
        {
            out[n++] = disp->model;
        }
    }
    return n;
}

#ifdef SSB_NO_DRAW
/* The host cross-test builds the tree and never draws it, but the
 * fighter manager's default FTDesc names this as the display proc, so
 * it has to exist there. */
void dc_model_proc_display(GObj *gobj)
{
    (void)gobj;
}

void dc_model_proc_display_layered(GObj *gobj)
{
    (void)gobj;
}

void dc_model_proc_display_backdrop(GObj *gobj)
{
    (void)gobj;
}

void dc_model_proc_display_zless(GObj *gobj, s32 band)
{
    (void)gobj;
    (void)band;
}

void dc_model_draw_tree(GObj *gobj)
{
    (void)gobj;
}

void dc_model_draw_tree_layered(GObj *gobj)
{
    (void)gobj;
}
#else
/* ft/ftdisplaymain.c:1168-1242 ftDisplayMainProcDisplay, the lines that
 * choose the light a fighter is drawn under. Before ftDisplayMainDrawAll
 * a fighter whose colanim carries a light of its own (is_use_light) puts
 * that on, turned the way the fighter faces -- a menu's Demo fighter has
 * no lr, and its DObj's own Y rotation stands in. After the draw, lines
 * 1240 and 1242 put the standing pair back: the map's for a player, the
 * menu's (scSubsysFighterSetLightParams) for a Demo fighter. So the
 * colanim pair is an override for the one draw, the standing pair is
 * what every later model sees, and both run here in that order around
 * fighter_frame, which is where the port's vertices are lit. */
static void fighter_light_stand(GObj *gobj)
{
    FTStruct *fp = ftGetStruct(gobj);

    if (fp->pkind != nFTPlayerKindDemo)
    {
        dc_model_set_light_angles(gMPCollisionLightAngleX,
                                  gMPCollisionLightAngleY);
    }
    else
    {
        dc_model_set_light_angles(scSubsysFighterGetLightAngleX(),
                                  scSubsysFighterGetLightAngleY());
    }
}

static void fighter_light(GObj *gobj)
{
    FTStruct *fp = ftGetStruct(gobj);

    if (!fp->colanim.is_use_light)
    {
        fighter_light_stand(gobj);
    }
    else if (fp->pkind != nFTPlayerKindDemo)
    {
        dc_model_set_light_angles(fp->lr * fp->colanim.light_angle_x,
                                  fp->colanim.light_angle_y);
    }
    else
    {
        dc_model_set_light_angles(
            F_CLC_RTOD32(DObjGetStruct(gobj)->rotate.vec.f.y) +
                fp->colanim.light_angle_x,
            fp->colanim.light_angle_y);
    }
}

/* See objmodel.h. The opaque list is drawn under every sprite, so a
 * model a menu puts between two sprite passes cannot go in it: the whole
 * model goes into the translucent list at the frame's next sprite depth
 * (fighter_draw_layered), which the passes before it are below and the
 * passes after it above. The character select's fighters on their gates
 * and the stage select's stage in its preview window are both this. */
/* `zless` is 0 for the two ordinary draws (is_backdrop picks the band)
 * and a DC_MODEL_ZLESS_* band for dc_model_proc_display_zless's. */
static void model_display_layered_impl(GObj *gobj, sb32 is_backdrop,
                                       sb32 rewalk, s32 zless);

static void model_display_layered(GObj *gobj, sb32 is_backdrop, sb32 rewalk,
                                  s32 zless)
{
    DBPERF_BEGIN(DBP_LAYERED);
    model_display_layered_impl(gobj, is_backdrop, rewalk, zless);
    DBPERF_END(DBP_LAYERED);
}

void dc_model_proc_display_layered(GObj *gobj)
{
    model_display_layered(gobj, FALSE, sLayeredRewalk, 0);
}

/* See objmodel.h. */
void dc_model_proc_display_zless(GObj *gobj, s32 band)
{
    model_display_layered(gobj, band != DC_MODEL_ZLESS_FRONT, TRUE, band);
}

/* See objmodel.h. The same capture, drawn in lbcommon.h's backdrop
 * band: under every 3D pixel and over the stage wallpaper. The slot is
 * half a backdrop step, as the front band's is half a sprite step. */
void dc_model_proc_display_backdrop(GObj *gobj)
{
    model_display_layered(gobj, TRUE, sLayeredRewalk, 0);
}

static void model_display_layered_impl(GObj *gobj, sb32 is_backdrop,
                                       sb32 rewalk, s32 zless)
{
    DObj *root = DObjGetStruct(gobj);
    DCDisplay *disp;
    Fighter *f;

    if (root == NULL)
    {
        return;
    }
    disp = dc_model_display_of(root);

    if (disp == NULL)
    {
        return;
    }
    f = disp->model;

    if (gcGetDrawList() == PVR_LIST_OP_POLY)
    {
        int is_fighter = gobj->id == nGCCommonKindFighter;

        /* with the re-walk on, the translucent pass does all of this at
         * the pose it draws, and this one's would be overwritten */
        if (rewalk)
        {
            return;
        }
        /* what the walk does not reach stays hidden through the passes
         * that follow: a hidden-part root's baked geometry until the
         * part is linked (Fighter.joint_hide) */
        f->joint_hide = ~(uint64_t)0;
        gcDrawDObjTreeForGObj(gobj);
        if (is_fighter)
        {
            fighter_light(gobj);
        }
        fighter_frame(f, dc_model_view_for(gobj), gcGetProjF(),
                      sLight[0], sLight[1], sLight[2]);
        if (is_fighter)
        {
            fighter_light_stand(gobj);
        }
    }
    else if (gcGetDrawList() == PVR_LIST_TR_POLY)
    {
        /* A scene that poses one fighter several times in a frame and
         * draws it after each pose (the Fighting Polygon Team's card,
         * sc1pintro.c) has its later poses in Fighter.vclip by now: the
         * opaque pass transformed all of them into the one buffer, and
         * every submit here would draw the last. The joints are back at
         * this call's pose, so it walks and transforms again. */
        if (rewalk)
        {
            int is_fighter = gobj->id == nGCCommonKindFighter;

            f->joint_hide = ~(uint64_t)0;
            gcDrawDObjTreeForGObj(gobj);
            if (is_fighter)
            {
                fighter_light(gobj);
            }
            fighter_frame(f, dc_model_view_for(gobj), gcGetProjF(),
                          sLight[0], sLight[1], sLight[2]);
            if (is_fighter)
            {
                fighter_light_stand(gobj);
            }
        }
        /* The whole model goes in one list, so the punch-through pass
         * gets nothing -- drawing it there as well would draw it twice. */
        if (zless == DC_MODEL_ZLESS_BACKDROP_UNDER ||
            zless == DC_MODEL_ZLESS_BACKDROP)
        {
            float ratio = fighter_layered_ratio(f);

            if (ratio > 0.0f)
            {
                sTris += fighter_draw_layered_scaled(f,
                             lbCommonBackdropStackTake(
                                 zless == DC_MODEL_ZLESS_BACKDROP_UNDER,
                                 ratio));
            }
        }
        else if (zless == DC_MODEL_ZLESS_FRONT)
        {
            sTris += fighter_draw_layered_paint(f, lbCommonSpriteNextDepth(),
                                                LB_SPRITE_Z_STEP * 0.5F);
        }
        else if (is_backdrop)
        {
            sTris += fighter_draw_layered_band(f,
                         lbCommonSpriteNextBackdropDepth(),
                         LB_SPRITE_Z_BACKDROP_STEP * 0.5F);
        }
        else sTris += fighter_draw_layered(f, lbCommonSpriteNextDepth());
    }
}

/* See objmodel.h. The batches are translucent and Z-less by the time
 * they get here -- the exporter reads the render mode the game's own
 * display list sets -- so the whole model goes in the translucent list
 * at its true 1/w, which for the ortho camera the arrows are drawn by is
 * the same depth for every vertex. */
void dc_model_draw_tree(GObj *gobj)
{
    DObj *root = DObjGetStruct(gobj);
    Fighter *f;

    if (root == NULL || dc_model_display_of(root) == NULL)
    {
        return;
    }
    f = dc_model_display_of(root)->model;

    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    f->joint_hide = ~(uint64_t)0;
    gcDrawDObjTreeForGObj(gobj);
    fighter_frame(f, gcGetViewF(), gcGetProjF(),
                  sLight[0], sLight[1], sLight[2]);
    sTris += fighter_draw_tr(f);
    f->joint_hide = 0;
}

/* See objmodel.h. The fighter inside the magnifying glass: the walk and
 * the transform in the opaque pass, as the layered draw, and the
 * submission in the translucent pass at a sprite depth, clipped. Nothing
 * else reads this fighter's clip positions between the two -- the main
 * camera does not draw a fighter that is off screen
 * (src/dc/ftdisplaymain.c), which is the only time this runs. */
static void dc_model_proc_display_magnify(GObj *gobj)
{
    DObj *root = DObjGetStruct(gobj);
    Fighter *f = dc_model_display_of(root)->model;

    if (gcGetDrawList() == PVR_LIST_OP_POLY)
    {
        f->joint_hide = ~(uint64_t)0;
        gcDrawDObjTreeForGObj(gobj);
        fighter_light(gobj);
        fighter_frame(f, dc_model_view_for(gobj), gcGetProjF(),
                      sLight[0], sLight[1], sLight[2]);
        fighter_light_stand(gobj);
    }
    else if (gcGetDrawList() == PVR_LIST_TR_POLY)
    {
        uint64_t t0 = timer_us_gettime64();
        unsigned n = fighter_draw_layered_ortho(f, lbCommonSpriteNextDepth(),
                                                CLIP_MAGNIFY_PLANES,
                                                CLIP_MAGNIFY_NPLANES,
                                                CLIP_MAGNIFY_INRADIUS);

        sUsMagnify += (unsigned)(timer_us_gettime64() - t0);
        sTris += n;
        sTrisMagnify += n;
    }
}

void dc_model_draw_tree_layered(GObj *gobj)
{
    DObj *root = DObjGetStruct(gobj);
    Fighter *f;

    if (root == NULL || dc_model_display_of(root) == NULL)
    {
        return;
    }
    f = dc_model_display_of(root)->model;

    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    f->joint_hide = ~(uint64_t)0;
    gcDrawDObjTreeForGObj(gobj);
    fighter_frame(f, gcGetViewF(), gcGetProjF(),
                  sLight[0], sLight[1], sLight[2]);
    {
        uint64_t t0 = timer_us_gettime64();
        unsigned n = fighter_draw_layered_ortho(f, lbCommonSpriteNextDepth(), NULL, 0, 0.0f);

        sUsMagnify += (unsigned)(timer_us_gettime64() - t0);
        sTris += n;
        sTrisMagnify += n;
    }
    f->joint_hide = 0;
}

/* A tree with grafts in it (Yoshi's Island's clouds) is walked again in
 * the punch-through and translucent passes. The one walk below happens
 * in the opaque pass, and a graft submits as the walk reaches it, so
 * a graft's own punch-through and translucent batches were never drawn:
 * the cloud is translucent, and eight of its nine puffs never showed.
 * The second walk only re-records the tree's matrices, the same values
 * the first left, for models whose clip positions are already made. */
static void dc_model_walk_grafts(GObj *gobj)
{
    DObj *dobj;

    for (dobj = DObjGetStruct(gobj); dobj != NULL;
         dobj = gcGetTreeDObjNext(dobj))
    {
        DCDisplay *disp = dobj->dv;

        if (disp != NULL && disp->proc_submit == dc_graft_submit)
        {
            gcDrawDObjTreeForGObj(gobj);
            return;
        }
    }
}

/* The FPACK_LIST_* a PVR list is. */
static int dc_model_list_of(int pvr_list)
{
    return (pvr_list == PVR_LIST_OP_POLY) ? FPACK_LIST_OP
         : (pvr_list == PVR_LIST_PT_POLY) ? FPACK_LIST_PT : FPACK_LIST_TR;
}

void dc_model_proc_display(GObj *gobj)
{
    DObj *root = DObjGetStruct(gobj);
    DCDisplay *disp;
    Fighter *f;

    /* An item whose kind has no ITModelRow (itemmodel.c) never calls
     * dc_model_add_dobjs, so its DObj's dv stays NULL for the item's
     * whole life -- the same "no model yet" state gcDrawDObjTree's own
     * `this_dobj->dv != NULL` guard already made safe for the decomp
     * dispatch this replaces (src/dc/itdisplay.c). */
    if (root == NULL)
    {
        return;
    }
    disp = dc_model_display_of(root);

    if (disp == NULL)
    {
        return;
    }
    f = disp->model;

    /* A menu's fighter -- the Demo kind, ft/ftmanager.c:865 -- stands
     * between sprite passes, so it takes the layered draw below. */
    if (gobj->id == nGCCommonKindFighter &&
        ftGetStruct(gobj)->pkind == nFTPlayerKindDemo && !sDemoOpaque)
    {
        dc_model_proc_display_layered(gobj);
        return;
    }
    if (gobj->id == nGCCommonKindFighter && gGCCurrentCamera != NULL &&
        gGCCurrentCamera->id == nGCCommonKindPlayerMagnifyCamera)
    {
        dc_model_proc_display_magnify(gobj);
        return;
    }

    /* An effect's pack is one Fighter shared by every live instance of
     * the effect (src/dc/efmanager.c efModelLoad loads it once), and a
     * walk left in Fighter.vclip by one instance's opaque pass would be
     * the next instance's by the time the translucent pass drew it -- two
     * players' shield bubbles on one fighter. So an effect walks and
     * transforms again in every pass it is drawn in. Its models are a
     * quad or a few (the halo is the largest at sixteen triangles), so
     * the second and third walks cost nothing worth measuring; a fighter
     * or a stage keeps the one walk below. */
    if (gobj->id == nGCCommonKindEffect)
    {
#ifndef DB_NO_LISTMASK
        /* the walk and the transform below are per pass; a pass into a list
         * the effect has no batch for would only repeat them for nothing */
        if (!fighter_has_list(f, dc_model_list_of(gcGetDrawList())))
            return;
#endif
        f->joint_hide = ~(uint64_t)0;
        gcDrawDObjTreeForGObj(gobj);
        fighter_frame(f, gcGetViewF(), gcGetProjF(),
                      sLight[0], sLight[1], sLight[2]);
        if (gcGetDrawList() == PVR_LIST_OP_POLY)
        {
            sTris += fighter_draw(f);
        }
        else if (gcGetDrawList() == PVR_LIST_PT_POLY)
        {
            sTris += fighter_draw_pt(f);
        }
        else
        {
            sTris += fighter_draw_tr(f);
        }
        return;
    }

    /* The camera pass runs this once per PVR list. The matrices and the
     * vertex transform are per frame, not per list, so they happen in
     * the first pass and the later ones only submit -- the punch-through
     * and translucent batches read the clip positions the first pass
     * left in Fighter.vclip. Between them the three draws cover every
     * batch exactly once. */
    /* A tree whose DObjs all point into one pack draws that pack. An item
     * that has switched a DObj to another pack's joint -- the Bob-omb's
     * walk, a monster's attack pose (src/dc/itemmodel.c
     * itemModelSetDisplayList) -- draws both: every model the walk reaches
     * is hidden first, walked once, and drawn, and the original pack's
     * switched-away joint simply is not reached. */
    {
        Fighter *models[DC_MODEL_TREE_MODELS];
        int n = dc_model_models_of(gobj, models, DC_MODEL_TREE_MODELS);
        int i;

        if (n == 0)
        {
            models[0] = f;
            n = 1;
        }
        /* An item's or weapon's pack is shared by every live one of its
         * kind (itemmodel.c loads it once), so a later pass cannot trust
         * the clip positions the opaque pass left: they are the last
         * instance's. It walks again, the way an effect does above --
         * or ten Break the Targets targets all draw at the tenth. */
        int rewalk = (gobj->id == nGCCommonKindItem ||
                      gobj->id == nGCCommonKindWeapon) &&
                     gcGetDrawList() != PVR_LIST_OP_POLY;

#ifndef DB_NO_LISTMASK
        if (rewalk)
        {
            /* the same: a re-walk for a list none of its models is in */
            int any = 0;

            for (i = 0; i < n; i++)
                any |= fighter_has_list(models[i], dc_model_list_of(gcGetDrawList()));
            if (!any)
                return;
        }
#endif

        if (gcGetDrawList() == PVR_LIST_OP_POLY || rewalk)
        {
            int is_fighter = gobj->id == nGCCommonKindFighter;

            /* what the walk does not reach stays hidden through the passes
             * that follow: a hidden-part root's baked geometry until the
             * part is linked (Fighter.joint_hide) */
            for (i = 0; i < n; i++)
            {
                models[i]->joint_hide = ~(uint64_t)0;
            }
            gcDrawDObjTreeForGObj(gobj);
            if (is_fighter)
            {
                fighter_light(gobj);
            }
            for (i = 0; i < n; i++)
            {
                fighter_frame(models[i], dc_model_view_for(gobj), gcGetProjF(),
                              sLight[0], sLight[1], sLight[2]);
            }
            if (is_fighter)
            {
                fighter_light_stand(gobj);
            }
        }
        if (gcGetDrawList() == PVR_LIST_OP_POLY)
        {
            for (i = 0; i < n; i++)
            {
                sTris += fighter_draw(models[i]);
            }
        }
        else if (gcGetDrawList() == PVR_LIST_PT_POLY)
        {
            for (i = 0; i < n; i++)
            {
                sTris += fighter_draw_pt(models[i]);
            }
            dc_model_walk_grafts(gobj);
        }
        else
        {
            for (i = 0; i < n; i++)
            {
                sTris += fighter_draw_tr(models[i]);
            }
            dc_model_walk_grafts(gobj);
        }
    }
}
#endif /* !SSB_NO_DRAW */
