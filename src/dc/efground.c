/* efground.c -- ef/efground.c, the background ground actors: the Lakitu
 * over Peach's Castle, Kongo Jungle's birds, the Arwing and its escort
 * over Sector Z, Zebes' creatures, Yoshi's Island's clouds and Poppo,
 * Dream Land's Whispy leaves and Kirby's own extras, Saffron City's
 * blimps and birds. One GObj (nGCCommonLinkIDEffect link 7,
 * EFGroundActorProcUpdate) picks a random entry from the current
 * stage's weighted table on a random timer and spawns it; the spawned
 * actor drifts across the map at its table's altitude and scale until
 * it walks off the map's bound, then ejects itself
 * (efGroundCheckEffectInBounds).
 *
 * Of the twelve functions below, eight are the decomp's
 * own, unmodified: efGroundCheckEffectInBounds, efGroundCommonProcUpdate,
 * efGroundUpdateEffectYaw, efGroundUpdateStepPositions,
 * efGroundSetStepPositions, efGroundUpdatePhysics,
 * EFGroundActorProcUpdate, efGroundSetupRandomWeights. Four are not, as
 * of the background ground actors: efGroundMakeEffectID
 * and efGroundMakeAppearActor keep their decomp signatures and shape but
 * no longer derive a ROM overlay's base address, because there is none
 * on this port to derive (see their own DIVERGES notes below); the
 * decomp's efGroundSetupEffectDObjs and efGroundMakeEffect are UNUSED --
 * their prototypes still come from ef/efground.h, transitively, and
 * nothing calls them -- replaced by dcGroundSetupEffectDObjs and
 * dcGroundMakeEffect, new port-own functions with a different signature
 * (a resolved pack and scripts, not raw ROM offsets to walk), which is
 * also why they are new names rather than a redeclaration.
 *
 * DIVERGES: the decomp's dEFGroundDatas is seven stage tables (Castle,
 * Sector, Jungle, Zebes, Yoster, Pupupu, Yamabuki; Hyrule carries none)
 * of EFGroundDesc entries, and every entry names a symbol out of that
 * stage's own ground overlay -- &llGRCastleMapLakituDObjDesc and its
 * kin, ~90 of them across the seven tables. Part B exports all seven,
 * as tools/export/ssb_stageexport.py's GRA1 block (src/dc/stage.c), and stage.c
 * fills dEFGroundDatas[gkind] in for real once that stage loads
 * (grStageAcquire); a gkind with no table loaded is left NULL, so
 * efGroundMakeAppearActor's own guard (the decomp's, unchanged) turns
 * that stage's call into a no-op -- which is Hyrule's case, the one
 * stage the decomp gives no table at all. That guard is also why the
 * decomp's one-past-the-end read for nGRKindInishie
 * (dEFGroundDatas has 8 entries, 0..7, but the guard
 * admits gkind == 8) is not reproduced: ours is bounds-safe by
 * construction, and there is no eighth table to miscount.
 *
 * The engine itself needed two lb/lbcommon.c functions the port had
 * not ported (lbCommonAddTreeDObjsAnimAll, lbCommonAddMObjForTreeDObjs
 * -- src/dc/lbcommon.c), and one more
 * already ported for every other model on the port: dc_model_add_dobjs
 * (src/dc/objmodel.c) is what dcGroundSetupEffectDObjs builds a ground
 * actor's tree with now, in place of the decomp's own gcAddChildForDObj
 * walk over a raw DObjDesc[] -- see that function's own DIVERGES note
 * for why. */
#include <ssb_types.h>
#include <ef/effect.h>
#include <gr/ground.h>
#include <mp/map.h>
#include <sys/obj.h>
#include <sys/utils.h>

#include "efground.h"
#include "objmodel.h"
#include "scmanager.h"
#include "stage.h"

/* ef/efground.c:1059-1131, the per-stage weight tables -- pure data, no
 * asset symbols, so these came across whole. Unreachable until a
 * dEFGroundDatas slot below points at one; kept so the day a stage's
 * DObj tables export, only that table and this file's dEFGroundDatas
 * entry need to change. */

// 0x8012F8C0
static EFGroundParam dEFGroundCastleParams[] =
{
    { 0, 0, 0, 2 },
    { 1, 0, 0, 1 }
};

// 0x8012F8D8
static EFGroundParam dEFGroundJungleParams[] =
{
    { 0, 0, 0, 2 },
    { 0, 1, 0, 1 }
};

// 0x8012F8F0
static EFGroundParam dEFGroundZebesParams[] =
{
    { 0, 0, 0, 1 },
    { 0, 1, 0, 1 },
    { 1, 0, 0, 1 }
};

// 0x8012F914
static EFGroundParam dEFGroundSectorParams[] =
{
    { 3, 0, -1,  3 },
    { 4, 0, +1, 3 },
    { 4, 1, +1, 4 },
    { 0, 0, -1,  3 },
    { 1, 0, -1,  2 },
    { 2, 0, -1,  2 },
    { 2, 0, +1, 1 }
};

// 0x8012F968
static EFGroundParam dEFGroundYosterParams[] =
{
    { 3, 0, 0, 2 },
    { 3, 1, 0, 1 },
    { 1, 0, 3*-1, 1 },
    { 2, 0, 3*+1, 1 },
    { 4, 0, 0, 1 },
    { 5, 0, 0, 1 },
    { 0, 0, 0, 4 }
};

// 0x8012F9BC
static EFGroundParam dEFGroundPupupuParams[] =
{
    { 0, 0, 0, 2 },
    { 0, 1, 0, 1 },
    { 1, 0, 0, 2 },
    { 1, 1, 0, 1 },
    { 2, 0, 0, 1 },
    { 3, 0, 0, 1 }
};

// 0x8012FA04
static EFGroundParam dEFGroundYamabukiParams[] =
{
    { 0, 0, 0, 6 },
    { 0, 1, 0, 6 },
    { 3, 0, 0, 8 },
    { 3, 1, 0, 8 },
    { 2, 0, 0, 10 },
    { 1, 0, 0, 1 }
};

/* ef/efground.c:991-1055 dEFGroundDatas 0x8012F840. DIVERGES: every
 * effect_descs/o_data pair is NULL/0 -- see the file header -- so the
 * effect_params/params_num above are unreachable dead data for now,
 * kept next to their real weight tables for when a stage's actors
 * export. Sized GR_STAGES_MAX (stage.h), not the decomp's 8: the extra
 * slot is nGRKindInishie's, which the decomp's own table omits (see the
 * file header's note on the one-past-the-end read this does not
 * reproduce). */
EFGroundData dEFGroundDatas[GR_STAGES_MAX];

/* 0x80131AD8 sEFGroundActor: the one ground-actor GObj a battle ever
 * has, exactly as the decomp keeps exactly one. */
static EFGroundActor sEFGroundActor;

/* PORT-OWN (efground.h): dEFGroundDatas[gkind].effect_descs' parallel
 * table of baked packs/scripts, one entry per gkind -- set by stage.c
 * when gkind's Stage loads (grStageAcquire) or NULL-out when it is
 * released (grStageRelease), and read by efGroundMakeAppearActor at the
 * one point sEFGroundActor needs to remember which table is current. */
static const EFGroundActorAsset *sEFGroundActorAssets[GR_STAGES_MAX];
static int sEFGroundActorAssetCounts[GR_STAGES_MAX];

/* the current battle's own table and effect_id -> asset lookup, cached
 * by efGroundMakeAppearActor so efGroundMakeEffectID need not re-index
 * sEFGroundActorAssets by gkind on every spawn. */
static const EFGroundActorAsset *sEFGroundActorCurrentAssets;
static int sEFGroundActorCurrentAssetCount;

void efGroundSetActorAssets(s32 gkind, const EFGroundActorAsset *assets,
                            int count)
{
    if (gkind < 0 || gkind >= GR_STAGES_MAX)
    {
        return;
    }
    sEFGroundActorAssets[gkind] = assets;
    sEFGroundActorAssetCounts[gkind] = (assets != NULL) ? count : 0;
}

/* ef/efground.c:1121-1142 efGroundCheckEffectInBounds 0x80115E80,
 * verbatim: TRUE while the actor's root DObj is still 500 units short
 * of the map bound it is walking toward, FALSE (and ejected) once it
 * crosses it. */
sb32 efGroundCheckEffectInBounds(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    DObj *dobj = DObjGetStruct(effect_gobj);

    if (ep->effect_vars.ground_effect.lr == -1)
    {
        if (dobj->translate.vec.f.x <= (gMPCollisionGroundData->map_bound_left + 500.0F))
        {
            efManagerSetPrevStructAlloc(ep);
            gcEjectGObj(effect_gobj);

            return FALSE;
        }
    }
    else if (dobj->translate.vec.f.x >= (gMPCollisionGroundData->map_bound_right - 500.0F))
    {
        efManagerSetPrevStructAlloc(ep);
        gcEjectGObj(effect_gobj);

        return FALSE;
    }
    return TRUE;
}

// 0x80115F5C
void efGroundCommonProcUpdate(GObj *effect_gobj)
{
    DObj *root_dobj = DObjGetStruct(effect_gobj);
    DObj *child_dobj = root_dobj->child;
    EFStruct *ep = efGetStruct(effect_gobj);

    if (efGroundCheckEffectInBounds(effect_gobj) != FALSE)
    {
        gcPlayAnimAll(effect_gobj);

        if (child_dobj->anim_wait == AOBJ_ANIM_NULL)
        {
            if ((ep->effect_vars.ground_effect.effect_status == -1) || (ep->effect_vars.ground_effect.effect_status != 0))
            {
                Vec3f pos = child_dobj->translate.vec.f;

                if (ep->effect_vars.ground_effect.lr == -1)
                {
                    root_dobj->translate.vec.f.x += (pos.x * root_dobj->scale.vec.f.x);
                }
                else root_dobj->translate.vec.f.x -= (pos.x * root_dobj->scale.vec.f.x);

                root_dobj->translate.vec.f.y += (pos.y * root_dobj->scale.vec.f.y);

                child_dobj->translate.vec.f.x = 0.0F;
                child_dobj->translate.vec.f.y = 0.0F;

                lbCommonAddTreeDObjsAnimAll(child_dobj, ep->effect_vars.ground_effect.anim_joint, ep->effect_vars.ground_effect.matanim_joint, 0.0F);
                gcPlayAnimAll(effect_gobj);
            }
        }
    }
}

// 0x80116090
void efGroundUpdateEffectYaw(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    DObj *root_dobj = DObjGetStruct(effect_gobj);

    efGroundCommonProcUpdate(effect_gobj);

    if (ep->effect_vars.ground_effect.lr == +1)
    {
        DObj *child_dobj = root_dobj->child->child;

        child_dobj->rotate.vec.f.z = -child_dobj->rotate.vec.f.z;
    }
}

// 0x801160E8
void efGroundUpdateStepPositions(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    DObj *dobj = DObjGetStruct(effect_gobj);

    efGroundCommonProcUpdate(effect_gobj);

    if
    (
        ((ep->effect_vars.ground_effect.lr == +1) && (ep->effect_vars.ground_effect.pos.x <= dobj->translate.vec.f.x)) ||
        ((ep->effect_vars.ground_effect.lr == -1) && (ep->effect_vars.ground_effect.pos.x >= dobj->translate.vec.f.x))
    )
    {
        ep->effect_vars.ground_effect.scale_step = 0.0F;
    }
    dobj->scale.vec.f.x += ep->effect_vars.ground_effect.scale_step;
    dobj->scale.vec.f.y += ep->effect_vars.ground_effect.scale_step;
    dobj->scale.vec.f.z += ep->effect_vars.ground_effect.scale_step;
}

// 0x801161A0
void efGroundSetStepPositions(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    DObj *dobj = DObjGetStruct(effect_gobj);
    f32 step_div;

    if (ep->effect_vars.ground_effect.lr == +1)
    {
        step_div = 1000.0F - dobj->translate.vec.f.x;
        ep->effect_vars.ground_effect.pos.x = 1000.0F;
    }
    else
    {
        step_div = 1000.0F + dobj->translate.vec.f.x;
        ep->effect_vars.ground_effect.pos.x = -1000.0F;
    }
    ep->effect_vars.ground_effect.scale_step = (dobj->scale.vec.f.x * 10.0F) / step_div;
}

// 0x80116204
void efGroundUpdatePhysics(GObj *effect_gobj, s32 effect_id)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    DObj *dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f.z = sEFGroundActor.effect_data->effect_descs[effect_id].pos_z;

    dobj->translate.vec.f.y =

    ((sEFGroundActor.effect_data->effect_descs[effect_id].alt_high - sEFGroundActor.effect_data->effect_descs[effect_id].alt_low) * syUtilsRandFloat()) +

    sEFGroundActor.effect_data->effect_descs[effect_id].alt_low;

    dobj->scale.vec.f.x = dobj->scale.vec.f.y = dobj->scale.vec.f.z = sEFGroundActor.effect_data->effect_descs[effect_id].scale;

    if (ep->effect_vars.ground_effect.lr == +1)
    {
        dobj->rotate.vec.f.y = F_CST_DTOR32(180.0F);
        dobj->translate.vec.f.x = gMPCollisionGroundData->map_bound_left + 500.0F;
    }
    else dobj->translate.vec.f.x = gMPCollisionGroundData->map_bound_right - 500.0F;

    if (sEFGroundActor.effect_data->effect_descs[effect_id].proc_groundeffect != NULL)
    {
        sEFGroundActor.effect_data->effect_descs[effect_id].proc_groundeffect(effect_gobj);
    }
}

/* PORT-OWN, not the decomp's efGroundSetupEffectDObjs (its fixed
 * signature -- a DObjDesc pointer, a DObj out-pointer, three transform
 * kinds -- is ef/efground.h's, included transitively through
 * ef/effect.h, and this needs a different one, so this is a new name
 * rather than a conflicting redeclaration of that one).
 *
 * The decomp reads `dobjdesc` as a raw DObjDesc[] out of a live ROM
 * ground overlay and hands each entry's `dl` straight to
 * gcAddChildForDObj, which stores it as the DObj's own `dv`/`dl` union
 * (sys/objtypes.h) -- on this port that union is what gcSubmitDObj
 * reads back as a DCDisplay pointer (src/dc/objdisplay.c), so a raw
 * display-list pointer there is not a rendering shortcut, it is a
 * wrong-typed read. Every other asset-bearing GObj in this port avoids
 * that by building its tree from a baked Fighter pack through
 * dc_model_add_dobjs (src/dc/objmodel.c), which allocates one real
 * DCDisplay per joint; a ground actor's pack is exactly that shape
 * (tools/export/ssb_stageexport.py, src/dc/stage.c), so this does the same
 * thing stage_bind_map_cloud does for the graft -- one shared, stage-
 * owned pack, instanced fresh under `parent` each spawn -- and hangs its
 * MObjs (if it has any) off the joints they belong to the same way. */
static void dcGroundSetupEffectDObjs(GObj *effect_gobj, DObj *parent,
                                     Fighter *pack, int matanim_alt,
                                     u32 billboard, s32 lr)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    DObj *joints[STAGE_GROUND_JOINTS_MAX];
    f32 rotate_step = 0.0F;
    int n, j;

    n = dc_model_add_dobjs(effect_gobj, parent, pack, joints);

    /* ef/efground.c:1328-1348, the decomp's own arm over each entry: a
     * billboard joint (its id's 0xF000, carried as `billboard`) appends
     * 0x2E or 0x48, and a right-facing actor turns that joint and every
     * one after it 180 degrees -- rotate_step is never reset. The append
     * claims rotate and scale for 0x2E (sys/objman.c), resetting them, so
     * the bind pose goes back on after it, as the decomp writes it after
     * its own gcAddXObjForDObjFixed. */
    for (j = 0; j < n; j++)
    {
        const FPackJoint *pj = &pack->joints[j];
        DObj *current_dobj = joints[j];

        if (current_dobj == NULL)
        {
            continue;
        }
        if (billboard & (1U << j))
        {
            if (ep->effect_vars.ground_effect.lr_bool != 0)
            {
                gcAddXObjForDObjFixed(current_dobj, 0x2E, 0);
            }
            else gcAddXObjForDObjFixed(current_dobj, 0x48, 0);

            if (lr == +1)
            {
                rotate_step = F_CST_DTOR32(180.0F);
            }
        }
        current_dobj->translate.vec.f.x = pj->t[0];
        current_dobj->translate.vec.f.y = pj->t[1];
        current_dobj->translate.vec.f.z = pj->t[2];
        current_dobj->rotate.vec.f.x = pj->r[0];
        current_dobj->rotate.vec.f.y = pj->r[1];
        current_dobj->rotate.vec.f.z = pj->r[2];
        current_dobj->rotate.vec.f.y += rotate_step;
        current_dobj->scale.vec.f.x = pj->s[0];
        current_dobj->scale.vec.f.y = pj->s[1];
        current_dobj->scale.vec.f.z = pj->s[2];
    }

    if (pack->mobjs == NULL)
    {
        return;
    }
    /* Every joint, not just the first: the animated MObj of all fifteen
     * ground actors that have one is on joint 1 (the joint that draws),
     * and lbCommonAddMObjForTreeDObjs -- the decomp's own -- walks the
     * whole subtree too. `matanim_alt` picks which of the pack's
     * MatAnimJoints plays; only Dream Land's Bronto carries two. */
    for (j = 0; j < n; j++)
    {
        dc_model_add_mobjs_dobj(joints[j], pack, 0.0F, matanim_alt, j);
    }
}

/* PORT-OWN, not the decomp's efGroundMakeEffect (same reason as
 * dcGroundSetupEffectDObjs's own note: its fixed signature is
 * ef/efground.h's, and this one takes a resolved pack and scripts
 * instead of an EFDesc to walk raw). `pack` is the resolved Fighter* for
 * this entry (NULL for one with no visual, matching the decomp's own
 * proc_display == NULL early return); `anim_joint`/`matanim_joint` are
 * already the per-joint tables lbCommonAddTreeDObjsAnimAll wants, not
 * offsets to resolve here. What is kept, unchanged: making the GObj and
 * its EFStruct, the lr_bool flag every ground_effect field after this
 * reads, the bare wrapper DObj physics writes into directly
 * (efGroundUpdatePhysics et al.), and the anim wiring/gcPlayAnimAll
 * call once the tree exists. */
static GObj *dcGroundMakeEffect(EFDesc *effect_desc, Fighter *pack,
                                AObjEvent32 **anim_joint,
                                AObjEvent32 ***matanim_joint,
                                int matanim_alt, u32 billboard, s32 lr)
{
    GObj *effect_gobj;
    DObj *main_dobj;
    EFStruct *ep;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    ep->proc_update = effect_desc->proc_update;

    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, efManagerFuncRun, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);
        return NULL;
    }
    effect_gobj->user_data.p = ep;

    ep->effect_vars.ground_effect.lr_bool = ((lr != -3) && (lr != 3)) ? FALSE : TRUE;

    if (effect_desc->proc_display == NULL)
    {
        return effect_gobj;
    }

    gcAddGObjDisplay(effect_gobj, effect_desc->proc_display, effect_desc->dl_link, 2, -1);
    gcMoveGObjDLHead(effect_gobj, effect_desc->dl_link, GOBJ_PRIORITY_DEFAULT);

    /* the bare wrapper always goes on -- efGroundUpdatePhysics and every
     * other physics function assume DObjGetStruct(effect_gobj) is never
     * NULL once proc_display is set, exactly as the decomp's own
     * efGroundMakeEffect guarantees (its equivalent branch is
     * unconditional there too). A NULL pack only skips the tree/anim
     * below -- a real EFGroundDesc entry always has one; this is the
     * same defensive slack the decomp's own o_dobjsetup == 0 case left. */
    main_dobj = gcAddDObjForGObj(effect_gobj, NULL);

    lbCommonInitDObj3Transforms(main_dobj, nGCMatrixKindTraRotRpyRSca, nGCMatrixKindNull, 0x00);

    if (pack == NULL)
    {
        return effect_gobj;
    }

    dcGroundSetupEffectDObjs(effect_gobj, main_dobj, pack, matanim_alt,
                             billboard, lr);

    if ((anim_joint != NULL) || (matanim_joint != NULL))
    {
        lbCommonAddTreeDObjsAnimAll(main_dobj->child, anim_joint, matanim_joint, 0.0F);
        gcPlayAnimAll(effect_gobj);
    }
    return effect_gobj;
}

/* DIVERGES: rewritten (background ground actors). The decomp derives file_head
 * for efGroundMakeEffect to walk from the currently loaded ground
 * overlay's own base address (efGroundMakeAppearActor's
 * gr_desc[1].dobjdesc - o_data), because on the N64 a stage's ground
 * actor assets move with whatever overlay buffer last held them. This
 * port's packs never move -- they are stage.c's own Stage, loaded once
 * and released once -- so there is no base address to derive: this
 * looks the resolved pack/scripts up in sEFGroundActorCurrentAssets,
 * the table efGroundMakeAppearActor pointed at gkind's own Stage, by
 * effect_id, the same index sEFGroundActor.effect_data->effect_descs
 * uses. */
void efGroundMakeEffectID(s32 effect_id)
{
    GObj *effect_gobj;
    EFStruct *ep;
    EFGroundDesc *desc;
    const EFGroundActorAsset *asset;

    desc = &sEFGroundActor.effect_data->effect_descs[effect_id];
    asset = (effect_id >= 0 && effect_id < sEFGroundActorCurrentAssetCount)
                ? &sEFGroundActorCurrentAssets[effect_id] : NULL;

    effect_gobj = dcGroundMakeEffect(&desc->effect_desc,
                                     (asset != NULL) ? asset->pack : NULL,
                                     (asset != NULL) ? asset->anim_joint : NULL,
                                     (asset != NULL) ? asset->matanim_joint : NULL,
                                     (asset != NULL) ? asset->matanim_alt : 0,
                                     (asset != NULL) ? asset->billboard : 0,
                                     sEFGroundActor.lr);

    if (effect_gobj != NULL)
    {
        ep = efGetStruct(effect_gobj);
        ep->effect_vars.ground_effect.effect_status = desc->effect_status;
        ep->effect_vars.ground_effect.anim_joint = (asset != NULL) ? asset->anim_joint : NULL;
        ep->effect_vars.ground_effect.matanim_joint = (asset != NULL) ? asset->matanim_joint : NULL;
        ep->effect_vars.ground_effect.lr = sEFGroundActor.lr;

        switch (ep->effect_vars.ground_effect.lr)
        {
        case (3 * -1):
            ep->effect_vars.ground_effect.lr = -1;
            break;

        case (3 * +1):
            ep->effect_vars.ground_effect.lr = +1;
            break;
        }
        efGroundUpdatePhysics(effect_gobj, effect_id);
    }
    if (sEFGroundActor.make_queue == 0)
    {
        sEFGroundActor.make_wait = syUtilsRandIntRange(10000) + 6000;
    }
    else sEFGroundActor.make_wait = 30;
}

// 0x801168CC
void EFGroundActorProcUpdate(GObj *gobj)
{
    s32 param_id;
    s32 effect_id;
    EFGroundParam *param;

    (void)gobj;

    if (sEFGroundActor.make_wait == 0)
    {
        effect_id = sEFGroundActor.effect_id;

        if (sEFGroundActor.make_queue == 0)
        {
            param_id = syUtilsRandIntRange(sEFGroundActor.effect_count);

            param_id = sEFGroundActor.effect_ids[param_id];

            param = sEFGroundActor.effect_data->effect_params;

            effect_id = sEFGroundActor.effect_id = (param + param_id)->effect_id;

            sEFGroundActor.lr = (param + param_id)->lr;

            if (sEFGroundActor.lr == 0)
            {
                sEFGroundActor.lr = (syUtilsRandIntRange(2) == 0) ? -1 : +1;
            }
            sEFGroundActor.make_queue = (param + param_id)->make_queue;

            if (sEFGroundActor.make_queue != 0)
            {
                sEFGroundActor.make_queue += syUtilsRandIntRange(2);
            }
        }
        else sEFGroundActor.make_queue--;

        efGroundMakeEffectID(effect_id);
    }
    sEFGroundActor.make_wait--;
}

// 0x801169CC
void efGroundSetupRandomWeights(void)
{
    u8 *effect_ids;
    u8 j, i, k;
    u8 effect_weights;
    EFGroundParam *param;

    param = sEFGroundActor.effect_data->effect_params;

    for (i = 0, j = 0; i < sEFGroundActor.effect_data->params_num; i++)
    {
        j += (param + i)->effect_weight;
    }
    sEFGroundActor.effect_ids = effect_ids = syTaskmanMalloc(j * (sizeof(*sEFGroundActor.effect_ids) | sizeof(*effect_ids)), 0x0);
    sEFGroundActor.effect_count = j;

    effect_weights = param->effect_weight;

    for (i = 0, k = 0; i < j; i++, effect_ids++)
    {
        if (effect_weights == i)
        {
            k++;

            effect_weights += (param + k)->effect_weight;
        }
        *effect_ids = k;
    }
}

/* DIVERGES: the file_head derivation is gone (background ground actors) -- see
 * efGroundMakeEffectID's own note. sEFGroundActorCurrentAssets/Count are
 * this port's stand-in: the same gkind index efGroundMakeEffectID reads
 * from, cached once here rather than re-looked-up every spawn. */
// 0x80116AD0
void efGroundMakeAppearActor(void)
{
    GObj *effect_gobj;

    if ((gSCManagerBattleState->gkind <= nGRKindBattleEnd) && (gSCManagerSceneData.scene_curr != nSCKind1PTrainingMode) && (dEFGroundDatas[gSCManagerBattleState->gkind].effect_params != NULL))
    {
        effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, 7, GOBJ_PRIORITY_DEFAULT);

        if (effect_gobj != NULL)
        {
            gcAddGObjProcess(effect_gobj, EFGroundActorProcUpdate, nGCProcessKindFunc, 1);

            effect_gobj->user_data.p = &sEFGroundActor;

            sEFGroundActor.make_wait = syUtilsRandIntRange(10000) + 6000;
            sEFGroundActor.effect_id = 0;
            sEFGroundActor.effect_data = &dEFGroundDatas[gSCManagerBattleState->gkind];

            sEFGroundActorCurrentAssets = sEFGroundActorAssets[gSCManagerBattleState->gkind];
            sEFGroundActorCurrentAssetCount = sEFGroundActorAssetCounts[gSCManagerBattleState->gkind];
            sEFGroundActor.make_queue = 0;

            efGroundSetupRandomWeights();
        }
    }
}

/* silence -Wunused-variable on the params tables above until a
 * dEFGroundDatas slot points at one */
static const EFGroundParam *const sEFGroundUnusedParamsCheck[] =
{
    dEFGroundCastleParams, dEFGroundJungleParams, dEFGroundZebesParams,
    dEFGroundSectorParams, dEFGroundYosterParams, dEFGroundPupupuParams,
    dEFGroundYamabukiParams,
};
