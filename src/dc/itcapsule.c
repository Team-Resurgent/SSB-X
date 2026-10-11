/* itcapsule.c -- it/itcommon/itcapsule.c, verbatim: the Capsule's `ITDesc`,
 * its six-state `ITStatusDesc` table (ground wait / air fall / fighter hold
 * / fighter throw / fighter drop / explode) and their proc bodies.
 * Function-for-function against the game's own ITStruct (it/ittypes.h);
 * every function names its decomp line range.
 *
 * The first of the four Containers -- the items the item
 * switch selects from kinds 0..3, which is why they matter more than their
 * size suggests: `itManagerMakeAppearActor` builds the appearance weights
 * over `nITKindCommonStart` to `nITKindCommonEnd`, the Containers are the
 * first four of those, and `dITManagerProcMakeList` calls its entry
 * UNGUARDED. So a kind the switch can pick with no MakeItem behind it is
 * not a missing feature, it is a crash.
 *
 * WHAT IS NEW HERE, against every roster item ported before it: this is
 * the first whose `proc_hit`/`proc_map` reaches `itMainMakeContainerItem`
 * (it/itmain.c:575), the function that rolls the container's own drop out
 * of `gITManagerRandomWeights` and spawns it as a child item. Every prior
 * file's header carries a line saying "nothing in this file reaches
 * itMainMakeContainerItem", and each of those was a deliberate check; this
 * is the file where it stops being true.
 *
 * The rest is the shape Sword/Bat/Hammer/Fan already established: the
 * usual per-item `ITDesc.o_attributes` offset, a `fall`/`wait`/`hold`/
 * `thrown`/`dropped` machine over `itMainSetStatus`, and `itMapCheck*` /
 * `itMainApplyGravityClampTVel` / `itVisualsUpdateSpin` leaves.
 * The `explode` state is the Capsule's
 * own: a six-frame hitbox sequence out of `AttackEvents Capsule` (0x98),
 * then `efManagerSparkleWhiteMultiExplodeMakeEffect` and the screen quake.
 *
 * `itCapsuleExplodeProcUpdate`'s attack table comes through
 * `itGetAttackEvent(dITCapsuleItemDesc, llITCommonDataCapsuleAttackEvents)`
 * -- `*p_file + off` against the dereferenced global, not the per-item
 * `attr->data` -- so it reads region 0 of the item pack at the offset
 * src/dc/itemoffsets.h records. That offset is 0x98, a block in
 * ITCommonData, and the row is pointer-free.
 *
 * `func_ovl3_801741F0` is the decomp's own "Unused" marker and is ported
 * anyway, per the standing doctrine (same status as every other dead
 * SetStatus this port keeps).
 */
#include <it/item.h>
#include <if/ifcommon.h>        /* ifCommonItemArrowMakeInterface */
#include <ef/effect.h>          /* efManagerQuakeMakeEffect */
#include <ef/efmanager.h>       /* efManagerSparkleWhiteMultiExplodeMakeEffect */

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

/* it/itcommon/itcapsule.c:10-32 dITCapsuleItemDesc, verbatim. */
ITDesc dITCapsuleItemDesc =
{
    nITKindCapsule,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataCapsuleItemAttributes,

    {
        nGCMatrixKindTraRotRpyR,
        nGCMatrixKindNull,
        0
    },

    nGMAttackStateOff,
    itCapsuleFallProcUpdate,
    itCapsuleFallProcMap,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    itCapsuleCommonProcHit
};

/* it/itcommon/itcapsule.c:34-118 dITCapsuleStatusDescs, verbatim.. Its hit, shield,
 * hop, set-off, reflector and damage slots are all filled, so a Capsule
 * on the ground or dropped breaks when hit and a thrown one hops
 * off a shield. */
ITStatusDesc dITCapsuleStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    {
        NULL,                               /* Proc Update */
        itCapsuleWaitProcMap,               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        itCapsuleCommonProcHit              /* Proc Damage */
    },

    /* Status 1 (Air Fall Wait) */
    {
        itCapsuleFallProcUpdate,            /* Proc Update */
        itCapsuleFallProcMap,               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        itCapsuleCommonProcHit              /* Proc Damage */
    },

    /* Status 2 (Fighter Hold) */
    {
        NULL,                               /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 3 (Fighter Throw) */
    {
        itCapsuleThrownProcUpdate,          /* Proc Update */
        itCapsuleThrownProcMap,             /* Proc Map */
        itCapsuleCommonProcHit,             /* Proc Hit */
        itCapsuleCommonProcHit,             /* Proc Shield */
        itMainCommonProcHop,                /* Proc Hop */
        itCapsuleCommonProcHit,             /* Proc Set-Off */
        itCapsuleCommonProcHit,             /* Proc Reflector */
        itCapsuleCommonProcHit              /* Proc Damage */
    },

    /* Status 4 (Fighter Drop) */
    {
        itCapsuleFallProcUpdate,            /* Proc Update */
        itCapsuleDroppedProcMap,            /* Proc Map */
        itCapsuleCommonProcHit,             /* Proc Hit */
        itCapsuleCommonProcHit,             /* Proc Shield */
        itMainCommonProcHop,                /* Proc Hop */
        itCapsuleCommonProcHit,             /* Proc Set-Off */
        itCapsuleCommonProcHit,             /* Proc Reflector */
        itCapsuleCommonProcHit              /* Proc Damage */
    },

    /* Status 5 (Explode; the decomp's comment says Fighter Hold) */
    {
        itCapsuleExplodeProcUpdate,         /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    }
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itCapsuleStatus
{
    nITCapsuleStatusWait,
    nITCapsuleStatusFall,
    nITCapsuleStatusHold,
    nITCapsuleStatusThrown,
    nITCapsuleStatusDropped,
    nITCapsuleStatusExplode,
    nITCapsuleStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/itcapsule.c:133-141 itCapsuleFallProcUpdate 0x80173F90,
 * verbatim. */
sb32 itCapsuleFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITCAPSULE_GRAVITY, ITCAPSULE_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itcapsule.c:144-150 itCapsuleWaitProcMap 0x80173FCC, verbatim. */
sb32 itCapsuleWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itCapsuleFallSetStatus);

    return FALSE;
}

/* it/itcommon/itcapsule.c:152-162 itCapsuleCommonProcHit 0x80173FF4,
 * verbatim. THE CONTAINER'S OWN: the hit either rolls a drop out of
 * gITManagerRandomWeights (itMainMakeContainerItem) and keeps the Capsule
 * alive, or it has nothing to give and the Capsule explodes. Both the
 * Thrown map and the Desc's own proc_damage name this. */
sb32 itCapsuleCommonProcHit(GObj *item_gobj)
{
    if (itMainMakeContainerItem(item_gobj) != FALSE)
    {
        return TRUE;
    }
    else itCapsuleExplodeMakeEffectGotoSetStatus(item_gobj);

    return FALSE;
}

/* it/itcommon/itcapsule.c:164-168 itCapsuleFallProcMap 0x80174030,
 * verbatim. */
sb32 itCapsuleFallProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITCAPSULE_MAP_REBOUND_COMMON, ITCAPSULE_MAP_REBOUND_GROUND, itCapsuleWaitSetStatus);
}

/* it/itcommon/itcapsule.c:170-175 itCapsuleWaitSetStatus 0x80174064,
 * verbatim. */
void itCapsuleWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITCapsuleStatusDescs, nITCapsuleStatusWait);
}

/* it/itcommon/itcapsule.c:177-191 itCapsuleFallSetStatus 0x80174098,
 * verbatim. Two lines no other roster item's Fall carries: a Container
 * takes damage from everything while airborne (`is_damage_all`) and its
 * hurtbox is live (`hitstatus` Normal), which is how a Capsule is broken
 * open by a hit rather than only by landing. */
void itCapsuleFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);

    ip->is_damage_all = TRUE;

    ip->damage_coll.hitstatus = nGMHitStatusNormal;

    itMainSetStatus(item_gobj, dITCapsuleStatusDescs, nITCapsuleStatusFall);
}

/* it/itcommon/itcapsule.c:193-197 itCapsuleHoldSetStatus 0x801740FC,
 * verbatim. */
void itCapsuleHoldSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITCapsuleStatusDescs, nITCapsuleStatusHold);
}

/* it/itcommon/itcapsule.c:199-208 itCapsuleThrownProcUpdate 0x80174124,
 * verbatim. */
sb32 itCapsuleThrownProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITCAPSULE_GRAVITY, ITCAPSULE_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itcapsule.c:210-222 itCapsuleThrownProcMap 0x80174160,
 * verbatim. Unlike FallProcMap this one goes through the full collision
 * mask first -- a thrown Capsule breaks on a wall as well as a floor. */
sb32 itCapsuleThrownProcMap(GObj *item_gobj)
{
    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_MAIN_MASK) != FALSE)
    {
        if (itMainMakeContainerItem(item_gobj) != FALSE)
        {
            return TRUE;
        }
        else itCapsuleExplodeMakeEffectGotoSetStatus(item_gobj);
    }
    return FALSE;
}

/* it/itcommon/itcapsule.c:224-234 itCapsuleThrownSetStatus 0x801741B0,
 * verbatim. */
void itCapsuleThrownSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_damage_all = TRUE;

    ip->damage_coll.hitstatus = nGMHitStatusNormal;

    itMainSetStatus(item_gobj, dITCapsuleStatusDescs, nITCapsuleStatusThrown);
}

/* it/itcommon/itcapsule.c:236-242 func_ovl3_801741F0 0x801741F0, verbatim.
 * The decomp marks it Unused and nothing in this file names it; kept for
 * the reason every other dead roster function is (see the file header). */
sb32 func_ovl3_801741F0(GObj *item_gobj)
{
    itMainVelSetRebound(item_gobj);

    return FALSE;
}

/* it/itcommon/itcapsule.c:244-248 itCapsuleDroppedProcMap 0x80174214,
 * verbatim. */
sb32 itCapsuleDroppedProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITCAPSULE_MAP_REBOUND_COMMON, ITCAPSULE_MAP_REBOUND_GROUND, itCapsuleWaitSetStatus);
}

/* it/itcommon/itcapsule.c:250-254 itCapsuleDroppedSetStatus 0x80174248,
 * verbatim. */
void itCapsuleDroppedSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITCapsuleStatusDescs, nITCapsuleStatusDropped);
}

/* it/itcommon/itcapsule.c:256-269 itCapsuleExplodeProcUpdate 0x80174270,
 * verbatim: six frames of the AttackEvents Capsule row (0x98 into
 * ITCommonData, src/dc/itemoffsets.h), then TRUE -- which the item main
 * loop reads as "destroy this item". */
sb32 itCapsuleExplodeProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi++;

    if (ip->multi == ITCAPSULE_EXPLODE_FRAME_END)
    {
        return TRUE;
    }
    itMainUpdateAttackEvent(item_gobj, itGetAttackEvent(dITCapsuleItemDesc, llITCommonDataCapsuleAttackEvents));

    return FALSE;
}

/* it/itcommon/itcapsule.c:271-285 itCapsuleMakeItem 0x801742AC, verbatim.
 * No DObj tail: a Capsule's tree is the baked model (src/dc/itemmodel.h,
 * keyed by this Desc's own o_attributes) plus whatever the base model
 * carries. */
GObj* itCapsuleMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITCapsuleItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}

/* it/itcommon/itcapsule.c:287-313 itCapsuleExplodeInitVars 0x80174340,
 * verbatim. */
void itCapsuleExplodeInitVars(GObj *item_gobj)
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

    ip->attack_coll.element = nGMHitElementFire;

    ip->attack_coll.can_setoff = FALSE;

    ip->damage_coll.hitstatus = nGMHitStatusNone;

    itMainClearOwnerStats(item_gobj);
    itMainRefreshAttackColl(item_gobj);

    itMainUpdateAttackEvent(item_gobj, itGetAttackEvent(dITCapsuleItemDesc, llITCommonDataCapsuleAttackEvents));
}

/* it/itcommon/itcapsule.c:315-320 itCapsuleExplodeSetStatus 0x801743F4,
 * verbatim. */
void itCapsuleExplodeSetStatus(GObj *item_gobj)
{
    itCapsuleExplodeInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITCapsuleStatusDescs, nITCapsuleStatusExplode);
}

/* it/itcommon/itcapsule.c:322-345 itCapsuleExplodeMakeEffectGotoSetStatus
 * 0x80174428, verbatim. `efManagerSparkleWhiteMultiExplodeMakeEffect`
 * returns the LBParticle it made, and the Capsule scales it -- the one
 * caller in this port that does, which is why the effect maker hands the
 * pointer back at all. */
void itCapsuleExplodeMakeEffectGotoSetStatus(GObj *item_gobj)
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
        ep->xf->scale.x = ep->xf->scale.y = ep->xf->scale.z = ITCAPSULE_EXPLODE_SCALE;
    }
    efManagerQuakeMakeEffect(1);

    DObjGetStruct(item_gobj)->flags = DOBJ_FLAG_HIDDEN;

    itCapsuleExplodeSetStatus(item_gobj);
}
