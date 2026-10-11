/* itegg.c -- it/itcommon/itegg.c, verbatim: the Egg's `ITDesc`, its
 * six-state `ITStatusDesc` table (ground wait / air fall / fighter hold /
 * fighter throw / fighter drop / neutral explosion) and their proc bodies.
 * Function-for-function against the game's own ITStruct (it/ittypes.h);
 * every function names its decomp line range.
 *
 * The second Container. Capsule's twin in almost every
 * respect -- same six states, same `itMainMakeContainerItem` call in
 * CommonProcHit and ThrownProcMap, same explode tail -- with three things
 * of its own, each worth naming because they are the differences and not
 * the sameness:
 *
 *  - `itEggWaitSetModelVars` and both Fall/Thrown updates write
 *    `dobj->child->rotate.vec.f.z = dobj->rotate.vec.f.z`. The Egg's model
 *    has a child joint that has to spin WITH its parent, which is what
 *    this copies; nothing else in this file touches it, so the line is
 *    the whole mechanism.
 *  - `itEggCommonProcHit` plays `efManagerEggBreakMakeEffect` at the
 *    shell's own position on the arm that DROPPED something, where
 *    Capsule's plays nothing. An egg that gives up its contents breaks;
 *    one that has nothing to give explodes.
 *  - `itEggMakeItem`'s tail: an Egg laid by Chansey (`nITKindMLucky`, the
 *    Poké Ball one) has a one-in-two chance of coming out mirrored --
 *    rotate.y 180, vel_air.x negated, `lr` flipped -- which is how a
 *    Chansey's eggs scatter to both sides.
 *
 * `itEggExplodeInitVars` calls `itGetAttackEvent(dITEggItemDesc,
 * llITCommonDataCapsuleAttackEvents)` -- the CAPSULE's table, in the Egg's
 * own explode initialiser, and the decomp's own comment on that line asks
 * "Should this be llITCommonDataEggAttackEvents?". It is ported verbatim
 * because it is what the game does: an Egg that explodes runs the
 * Capsule's hitbox sequence. `itEggExplodeProcUpdate`, ten lines up, uses
 * the Egg's own `llITCommonDataEggAttackEvents` -- so the two disagree,
 * in the decomp, on purpose-or-not, and the PORT-NOT-REMAKE doctrine says
 * keep the disagreement.
 *
 * `func_ovl3_80181894` is the decomp's own "Unused" marker, kept for the
 * reason every other dead roster function is.
 */
#include <it/item.h>
#include <if/ifcommon.h>        /* ifCommonItemArrowMakeInterface */
#include <ef/effect.h>          /* efManagerQuakeMakeEffect */
#include <ef/efmanager.h>       /* efManagerSparkleWhiteMultiExplodeMakeEffect,
                                   efManagerEggBreakMakeEffect */

#include "itemoffsets.h"        /* llITCommonData* offsets */
#include "itemmodel.h"          /* the baked item models */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/itegg.c:10-32 dITEggItemDesc, verbatim. Note the transform
 * triple: `nGCMatrixKindTraRotRpyRSca`, not Capsule's TraRotRpyR -- an Egg
 * SCALES, which is what itEggWaitSetModelVars' own scale write is for. */
ITDesc dITEggItemDesc =
{
    nITKindEgg,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataEggItemAttributes,

    {
        nGCMatrixKindTraRotRpyRSca,
        nGCMatrixKindNull,
        0
    },

    nGMAttackStateOff,
    itEggFallProcUpdate,
    itEggFallProcMap,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    itEggCommonProcHit
};

/* it/itcommon/itegg.c:34-118 dITEggStatusDescs, verbatim. Every state
 * names `itEggCommonProcHit` as its proc_damage, and Wait does too -- an
 * Egg can be broken by a hit while it is sitting on the ground. */
ITStatusDesc dITEggStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    {
        NULL,
        itEggWaitProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        itEggCommonProcHit
    },

    /* Status 1 (Air Fall Wait) */
    {
        itEggFallProcUpdate,
        itEggFallProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        itEggCommonProcHit
    },

    /* Status 2 (Fighter Hold) */
    {
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 3 (Fighter Throw) */
    {
        itEggThrownProcUpdate,
        itEggThrownProcMap,
        itEggCommonProcHit,
        itEggCommonProcHit,
        itMainCommonProcHop,
        itEggCommonProcHit,
        itEggCommonProcHit,
        itEggCommonProcHit
    },

    /* Status 4 (Fighter Drop) */
    {
        itEggFallProcUpdate,
        itEggDroppedProcMap,
        itEggCommonProcHit,
        itEggCommonProcHit,
        itMainCommonProcHop,
        itEggCommonProcHit,
        itEggCommonProcHit,
        itEggCommonProcHit
    },

    /* Status 5 (Neutral Explosion) */
    {
        itEggExplodeProcUpdate,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    }
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itEggStatus
{
    nITEggStatusWait,
    nITEggStatusFall,
    nITEggStatusHold,
    nITEggStatusThrown,
    nITEggStatusDropped,
    nITEggStatusExplode,
    nITEggStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/itegg.c:133-146 itEggFallProcUpdate 0x801815C0, verbatim. */
sb32 itEggFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITEGG_GRAVITY, ITEGG_TVEL);
    itVisualsUpdateSpin(item_gobj);

    dobj->child->rotate.vec.f.z = dobj->rotate.vec.f.z;

    return FALSE;
}

/* it/itcommon/itegg.c:147-153 itEggWaitProcMap 0x80181618, verbatim. */
sb32 itEggWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itEggFallSetStatus);

    return FALSE;
}

/* it/itcommon/itegg.c:155-167 itEggCommonProcHit 0x80181640, verbatim.
 * The shell breaks only on the arm that HAD something to give. */
sb32 itEggCommonProcHit(GObj *item_gobj)
{
    if (itMainMakeContainerItem(item_gobj) != FALSE)
    {
        efManagerEggBreakMakeEffect(&DObjGetStruct(item_gobj)->translate.vec.f);

        return TRUE;
    }
    else itEggExplodeMakeEffectGotoSetStatus(item_gobj);

    return FALSE;
}

/* it/itcommon/itegg.c:169-173 itEggFallProcMap 0x80181688, verbatim. */
sb32 itEggFallProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITEGG_MAP_REBOUND_COMMON, ITEGG_MAP_REBOUND_GROUND, itEggWaitSetStatus);
}

/* it/itcommon/itegg.c:175-183 itEggWaitSetModelVars 0x801816B8, verbatim:
 * the scale the Wait state restores, and the child's spin re-synced to its
 * parent's. */
void itEggWaitSetModelVars(GObj *item_gobj)
{
    DObj *dobj = DObjGetStruct(item_gobj);

    dobj->scale.vec.f.x = dobj->scale.vec.f.y = dobj->scale.vec.f.z = 1.0F;

    dobj->child->rotate.vec.f.z = dobj->rotate.vec.f.z;
}

/* it/itcommon/itegg.c:185-191 itEggWaitSetStatus 0x801816E0, verbatim. */
void itEggWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itEggWaitSetModelVars(item_gobj);
    itMainSetStatus(item_gobj, dITEggStatusDescs, nITEggStatusWait);
}

/* it/itcommon/itegg.c:193-207 itEggFallSetStatus 0x8018171C, verbatim. */
void itEggFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    ip->damage_coll.hitstatus = nGMHitStatusNormal;
    ip->attack_coll.attack_state = nGMAttackStateOff;

    ip->is_damage_all = TRUE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITEggStatusDescs, nITEggStatusFall);
}

/* it/itcommon/itegg.c:209-213 itEggHoldSetStatus 0x80181778, verbatim. */
void itEggHoldSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITEggStatusDescs, nITEggStatusHold);
}

/* it/itcommon/itegg.c:215-227 itEggThrownProcUpdate 0x801817A0,
 * verbatim. */
sb32 itEggThrownProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITEGG_GRAVITY, ITEGG_TVEL);
    itVisualsUpdateSpin(item_gobj);

    dobj->child->rotate.vec.f.z = dobj->rotate.vec.f.z;

    return FALSE;
}

/* it/itcommon/itegg.c:229-243 itEggThrownProcMap 0x801817F8, verbatim. */
sb32 itEggThrownProcMap(GObj *item_gobj)
{
    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_MAIN_MASK) != FALSE)
    {
        if (itMainMakeContainerItem(item_gobj) != FALSE)
        {
            efManagerEggBreakMakeEffect(&DObjGetStruct(item_gobj)->translate.vec.f);

            return TRUE;
        }
        else itEggExplodeMakeEffectGotoSetStatus(item_gobj);
    }
    return FALSE;
}

/* it/itcommon/itegg.c:245-253 itEggThrownSetStatus 0x80181854, verbatim. */
void itEggThrownSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_damage_all = TRUE;

    ip->damage_coll.hitstatus = nGMHitStatusNormal;

    itMainSetStatus(item_gobj, dITEggStatusDescs, nITEggStatusThrown);
}

/* it/itcommon/itegg.c:255-261 func_ovl3_80181894 0x80181894, verbatim.
 * The decomp marks it Unused; nothing in this file names it. */
sb32 func_ovl3_80181894(GObj *item_gobj)
{
    itMainVelSetRebound(item_gobj);

    return FALSE;
}

/* it/itcommon/itegg.c:263-267 itEggDroppedProcMap 0x801818B8, verbatim. */
sb32 itEggDroppedProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITEGG_MAP_REBOUND_COMMON, ITEGG_MAP_REBOUND_GROUND, itEggWaitSetStatus);
}

/* it/itcommon/itegg.c:269-279 itEggDroppedSetStatus 0x801818E8, verbatim. */
void itEggDroppedSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_damage_all = TRUE;

    ip->damage_coll.hitstatus = nGMHitStatusNormal;

    itMainSetStatus(item_gobj, dITEggStatusDescs, nITEggStatusDropped);
}

/* it/itcommon/itegg.c:283-296 itEggExplodeProcUpdate 0x80181928, verbatim:
 * eight frames of the Egg's OWN AttackEvents row (0xb14), then the shell
 * break and TRUE -- which the item main loop reads as "destroy this". */
sb32 itEggExplodeProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi++;

    if (ip->multi == ITEGG_EXPLODE_EFFECT_WAIT)
    {
        efManagerEggBreakMakeEffect(&DObjGetStruct(item_gobj)->translate.vec.f);

        return TRUE;
    }
    itMainUpdateAttackEvent(item_gobj, itGetAttackEvent(dITEggItemDesc, llITCommonDataEggAttackEvents));

    return FALSE;
}

/* it/itcommon/itegg.c:298-334 itEggMakeItem 0x80181998, verbatim. The
 * `nITKindMLucky` arm is Chansey's: an Egg she lays comes out mirrored
 * half the time, so a pair of them scatters to both sides. */
GObj* itEggMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITEggItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        DObj *dobj = DObjGetStruct(item_gobj);
        ITStruct *egg_ip = itGetStruct(item_gobj);

        egg_ip->is_unused_item_bool = TRUE;

        egg_ip->arrow_gobj = ifCommonItemArrowMakeInterface(egg_ip);

        gcAddXObjForDObjFixed(dobj->child, 0x2E, 0);

        dobj->translate.vec.f = *pos;

        if (flags & ITEM_FLAG_PARENT_ITEM)
        {
            ITStruct *spawn_ip = itGetStruct(parent_gobj);

            if ((spawn_ip->kind == nITKindMLucky) && (syUtilsRandIntRange(2) == 0))
            {
                dobj->child->rotate.vec.f.y = F_CST_DTOR32(180.0F);

                egg_ip->physics.vel_air.x = -egg_ip->physics.vel_air.x;

                egg_ip->lr = -egg_ip->lr;
            }
        }
    }
    return item_gobj;
}

/* it/itcommon/itegg.c:336-361 itEggExplodeInitVars 0x80181AA8, verbatim,
 * INCLUDING the Capsule's table on the last line -- see the file header.
 * The decomp's own comment there asks whether it should be the Egg's. */
void itEggExplodeInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi = 0;

    ip->event_id = 0;

    ip->attack_coll.fgm_id = nSYAudioFGMExplodeL;
    ip->attack_coll.throw_mul = ITEM_THROW_DEFAULT;

    func_800269C0_275C0(nSYAudioFGMExplodeL);

    ip->attack_coll.can_rehit_item = TRUE;
    ip->attack_coll.can_hop = FALSE;
    ip->attack_coll.can_reflect = FALSE;
    ip->attack_coll.can_setoff = FALSE;
    ip->attack_coll.element = nGMHitElementFire;

    ip->damage_coll.hitstatus = nGMHitStatusNone;

    itMainClearOwnerStats(item_gobj);
    itMainRefreshAttackColl(item_gobj);
    itMainUpdateAttackEvent(item_gobj, itGetAttackEvent(dITEggItemDesc, llITCommonDataCapsuleAttackEvents)); /* Should this be llITCommonDataEggAttackEvents? */
}

/* it/itcommon/itegg.c:363-368 itEggExplodeSetStatus 0x80181B5C, verbatim. */
void itEggExplodeSetStatus(GObj *item_gobj)
{
    itEggExplodeInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITEggStatusDescs, nITEggStatusExplode);
}

/* it/itcommon/itegg.c:370-393 itEggExplodeMakeEffectGotoSetStatus
 * 0x80181B90, verbatim. */
void itEggExplodeMakeEffectGotoSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    LBParticle *ep;

    ip->attack_coll.attack_state = nGMAttackStateOff;

    ip->physics.vel_air.x = 0.0F;
    ip->physics.vel_air.y = 0.0F;
    ip->physics.vel_air.z = 0.0F;

    ep = efManagerSparkleWhiteMultiExplodeMakeEffect(&dobj->translate.vec.f);

    if (ep != NULL)
    {
        ep->xf->scale.x = ep->xf->scale.y = ep->xf->scale.z = ITEGG_EXPLODE_EFFECT_SCALE;
    }
    efManagerQuakeMakeEffect(1);

    DObjGetStruct(item_gobj)->flags = DOBJ_FLAG_HIDDEN;

    itEggExplodeSetStatus(item_gobj);
}
