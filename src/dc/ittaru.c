/* ittaru.c -- it/itcommon/ittaru.c, verbatim: the Barrel's `ITDesc`, its
 * seven-state `ITStatusDesc` table (ground wait / air fall / fighter hold /
 * fighter throw / fighter drop / neutral explosion / GROUND ROLL) and their
 * proc bodies. Function-for-function against the game's own ITStruct
 * (it/ittypes.h); every function names its decomp line range.
 *
 * The fourth and last Container, and the only one that
 * ROLLS. Crate's twin for most of its length -- same hit points, same
 * `itBoxContainerSmashMakeEffect` on the break, same `itMainMakeContainerItem`
 * drop -- with one extra state and one piece of state that exists only
 * for it:
 *
 *  - `nITTaruStatusRoll` is the seventh state. A thrown Barrel that lands
 *    slowly enough (< ITTARU_VEL_MIN) keeps rolling along the floor angle
 *    instead of settling, with `roll_rotate_step` driving its spin and a
 *    LIFETIME (ITTARU_LIFETIME) counting down -- and once that falls
 *    below ITTARU_DESPAWN_FLASH_START the Barrel blinks, one frame on and
 *    one off, until it vanishes. Nothing else in it/ has a lifetime it
 *    spends visibly.
 *  - `itTaruThrownInitVars` reshapes the collision box (`map_coll.top` and
 *    `.bottom` become ±`.width`) and tips the child joint 90 degrees, so a
 *    thrown Barrel is a different shape from a standing one. It runs on
 *    BOTH the Thrown and Dropped transitions, and Dropped shares Thrown's
 *    proc_map outright -- which is why `dITTaruStatusDescs[4]` and `[3]`
 *    are the same row twice.
 *
 * The decomp's own comments on `itTaruThrownProcMap`'s `>= 90.0F` arm are
 * kept: it asks whether they meant `ABSF(...)`, and notes the branch looks
 * unreachable. It is ported exactly as written, per PORT-not-REMAKE.
 *
 * `func_ovl3_80179F50` is the decomp's own "Unused" marker, kept for the
 * reason every other dead roster function is.
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

/* itbox.c: itBoxContainerSmashMakeEffect, which a Barrel reaches by name.
 * Declared by the decomp's own it/itcommon/itbox.h, which this file's
 * `<it/item.h>` chain does not pull in. */
void itBoxContainerSmashMakeEffect(Vec3f *pos);

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/ittaru.c:11-31 dITTaruItemDesc, verbatim. */
ITDesc dITTaruItemDesc =
{
    nITKindTaru,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataTaruItemAttributes,

    {
        nGCMatrixKindTraRotRpyR,
        nGCMatrixKindNull,
        0
    },

    nGMAttackStateOff,
    itTaruFallProcUpdate,
    itTaruFallProcMap,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/* it/itcommon/ittaru.c:33-126 dITTaruStatusDescs, verbatim. Note rows 3
 * and 4 are identical: a dropped Barrel stays a thrown one in every way
 * but the transition that got it there. */
ITStatusDesc dITTaruStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    {
        NULL,
        itTaruWaitProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        itTaruCommonProcDamage
    },

    /* Status 1 (Air Wait Fall) */
    {
        itTaruFallProcUpdate,
        itTaruFallProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
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
        itTaruFallProcUpdate,
        itTaruThrownProcMap,
        itTaruCommonProcHit,
        itTaruCommonProcHit,
        NULL,
        itTaruCommonProcHit,
        itTaruCommonProcHit,
        itTaruCommonProcDamage
    },

    /* Status 4 (Fighter Drop) */
    {
        itTaruFallProcUpdate,
        itTaruThrownProcMap,
        itTaruCommonProcHit,
        itTaruCommonProcHit,
        NULL,
        itTaruCommonProcHit,
        itTaruCommonProcHit,
        itTaruCommonProcDamage
    },

    /* Status 5 (Neutral Explosion) */
    {
        itTaruExplodeProcUpdate,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 6 (Ground Roll) */
    {
        itTaruRollProcUpdate,
        itTaruRollProcMap,
        itTaruCommonProcHit,
        itTaruCommonProcHit,
        NULL,
        itTaruCommonProcHit,
        itTaruCommonProcHit,
        itTaruCommonProcDamage
    }
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itTaruStatus
{
    nITTaruStatusWait,
    nITTaruStatusFall,
    nITTaruStatusHold,
    nITTaruStatusThrown,
    nITTaruStatusDropped,
    nITTaruStatusExplode,
    nITTaruStatusRoll,
    nITTaruStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/ittaru.c:146-157 itTaruFallProcUpdate 0x80179BA0, verbatim.
 * The middle line is the Barrel's own: it carries a roll_rotate_step even
 * while falling, which is what makes a rolling Barrel keep spinning after
 * it leaves the ground. */
sb32 itTaruFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITTARU_GRAVITY, ITTARU_TVEL);

    DObjGetStruct(item_gobj)->rotate.vec.f.z += ip->item_vars.taru.roll_rotate_step;

    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/ittaru.c:160-166 itTaruWaitProcMap 0x80179BF8, verbatim. */
sb32 itTaruWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itTaruFallSetStatus);

    return FALSE;
}

/* it/itcommon/ittaru.c:168-182 itTaruCommonProcHit 0x80179C20, verbatim.
 * The smash sound and effect come FIRST here, before the drop is rolled --
 * where Capsule and Egg play nothing on the drop arm and Egg plays the
 * shell break only on it. A Barrel always shatters. */
sb32 itTaruCommonProcHit(GObj *item_gobj)
{
    func_800269C0_275C0(nSYAudioFGMContainerSmash);

    itBoxContainerSmashMakeEffect(&DObjGetStruct(item_gobj)->translate.vec.f);

    if (itMainMakeContainerItem(item_gobj) != FALSE)
    {
        return TRUE;
    }
    else itTaruExplodeMakeEffectGotoSetStatus(item_gobj);

    return FALSE;
}

/* it/itcommon/ittaru.c:184-194 itTaruCommonProcDamage 0x80179C78,
 * verbatim: the Barrel has hit points like the Crate. */
sb32 itTaruCommonProcDamage(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->percent_damage >= ITTARU_HEALTH_MAX)
    {
        return itTaruCommonProcHit(item_gobj);
    }
    else return FALSE;
}

/* it/itcommon/ittaru.c:196-200 itTaruFallProcMap 0x80179CB8, verbatim. */
sb32 itTaruFallProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITTARU_MAP_REBOUND_COMMON, ITTARU_MAP_REBOUND_GROUND, itTaruWaitSetStatus);
}

/* it/itcommon/ittaru.c:202-207 itTaruWaitSetStatus 0x80179CE8, verbatim. */
void itTaruWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITTaruStatusDescs, nITTaruStatusWait);
}

/* it/itcommon/ittaru.c:209-218 itTaruFallSetStatus 0x80179D1C, verbatim. */
void itTaruFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITTaruStatusDescs, nITTaruStatusFall);
}

/* it/itcommon/ittaru.c:220-224 itTaruHoldSetStatus 0x80179D60, verbatim. */
void itTaruHoldSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITTaruStatusDescs, nITTaruStatusHold);
}

/* it/itcommon/ittaru.c:226-242 itTaruThrownCheckMapCollision 0x80179D88,
 * verbatim: a thrown Barrel rebounds off walls and ceilings but NOT off
 * the floor -- the floor is what turns it into a roll, which is why this
 * only reports whether the floor was hit and leaves the deciding to its
 * caller. */
sb32 itTaruThrownCheckMapCollision(GObj *item_gobj, f32 common_rebound)
{
    s32 unused;
    ITStruct *ip;
    sb32 is_collide_floor = itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR);

    if (itMapCheckCollideAllRebound(item_gobj, (MAP_FLAG_CEIL | MAP_FLAG_RWALL | MAP_FLAG_LWALL), common_rebound, NULL) != FALSE)
    {
        itMainSetSpinVelLR(item_gobj);
    }
    if (is_collide_floor != FALSE)
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itcommon/ittaru.c:244-254 itTaruRollSetStatus 0x80179DEC, verbatim:
 * the lifetime a rolling Barrel spends, and the zeroed y velocity that
 * keeps it on the ground. */
void itTaruRollSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->lifetime = ITTARU_LIFETIME;

    ip->physics.vel_air.y = 0.0F;

    itMainSetStatus(item_gobj, dITTaruStatusDescs, nITTaruStatusRoll);
}

/* it/itcommon/ittaru.c:256-284 itTaruThrownProcMap 0x80179E28, verbatim,
 * including the decomp's own two questions about the `>= 90.0F` arm -- it
 * asks whether `ABSF` was meant and notes the branch looks unreachable,
 * and both are kept because the game keeps them. */
sb32 itTaruThrownProcMap(GObj *item_gobj)
{
    if (itTaruThrownCheckMapCollision(item_gobj, 0.5F) != FALSE)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        if (ip->physics.vel_air.y >= 90.0F) /* Is it even possible to meet this condition? Didn't they mean ABSF(ip->physics.vel_air.y)? */
        {
            itTaruCommonProcHit(item_gobj); /* This causes the barrel to smash on impact when landing from too high; doesn't seem possible to trigger */

            return TRUE;
        }
        else if (ip->physics.vel_air.y < 30.0F)
        {
            itTaruRollSetStatus(item_gobj);
        }
        else
        {
            lbCommonReflect2D(&ip->physics.vel_air, &ip->coll_data.floor_angle);

            ip->physics.vel_air.y *= 0.2F;

            itMainSetSpinVelLR(item_gobj);
        }
        itMainClearOwnerStats(item_gobj);
    }
    return FALSE;
}

/* it/itcommon/ittaru.c:286-295 itTaruThrownInitVars 0x80179EF0, verbatim:
 * the tip and the reshaped box. */
void itTaruThrownInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    DObjGetStruct(item_gobj)->child->rotate.vec.f.x = F_CST_DTOR32(90.0F);

    ip->coll_data.map_coll.top = ip->coll_data.map_coll.width;
    ip->coll_data.map_coll.bottom = -ip->coll_data.map_coll.width;
}

/* it/itcommon/ittaru.c:297-301 itTaruThrownSetStatus 0x80179F1C,
 * verbatim. */
void itTaruThrownSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITTaruStatusDescs, nITTaruStatusThrown);
    itTaruThrownInitVars(item_gobj);
}

/* it/itcommon/ittaru.c:304-310 func_ovl3_80179F50 0x80179F50, verbatim.
 * The decomp marks it Unused; nothing in this file names it. */
sb32 func_ovl3_80179F50(GObj *item_gobj)
{
    itMainVelSetRebound(item_gobj);

    return FALSE;
}

/* it/itcommon/ittaru.c:312-317 itTaruDroppedSetStatus 0x80179F74,
 * verbatim: the same tip and box as Thrown. */
void itTaruDroppedSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITTaruStatusDescs, nITTaruStatusDropped);
    itTaruThrownInitVars(item_gobj);
}

/* it/itcommon/ittaru.c:319-333 itTaruExplodeProcUpdate 0x80179FA8,
 * verbatim: eight frames of the Taru's own AttackEvents row (0x67c). */
sb32 itTaruExplodeProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi++;

    if (ip->multi == ITTARU_EXPLODE_LIFETIME)
    {
        return TRUE;
    }
    else itMainUpdateAttackEvent(item_gobj, itGetAttackEvent(dITTaruItemDesc, llITCommonDataTaruAttackEvents));

    return FALSE;
}

/* it/itcommon/ittaru.c:335-371 itTaruRollProcUpdate 0x8017A004,
 * verbatim: the whole rolling state. The first line accelerates along the
 * floor's own angle, the last two spin the model by the resulting speed,
 * and the middle block is the lifetime -- which flips `DOBJ_FLAG_HIDDEN`
 * every other frame below ITTARU_DESPAWN_FLASH_START, so the Barrel
 * blinks itself out. */
sb32 itTaruRollProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    f32 roll_rotate_step;
    f32 sqrt_vel;

    ip->physics.vel_air.x += (-(syUtilsArcTan2(ip->coll_data.floor_angle.y, ip->coll_data.floor_angle.x) - F_CLC_DTOR32(90.0F)) * ITTARU_MUL_VEL_X);

    ip->lr = (ip->physics.vel_air.x >= 0.0F) ? +1 : -1;

    sqrt_vel = sqrtf(SQUARE(ip->physics.vel_air.x) + SQUARE(ip->physics.vel_air.y));

    if (sqrt_vel < ITTARU_VEL_MIN)
    {
        ip->lifetime--;

        if (ip->lifetime < ITTARU_DESPAWN_FLASH_START)
        {
            if (ip->lifetime == 0)
            {
                return TRUE;
            }
            else if ((ip->lifetime % 2) != 0)
            {
                DObjGetStruct(item_gobj)->flags ^= DOBJ_FLAG_HIDDEN;
            }
        }
    }
    roll_rotate_step = ((ip->lr == -1) ? ITTARU_ROLL_ROTATE_MUL : -ITTARU_ROLL_ROTATE_MUL) * sqrt_vel;

    ip->item_vars.taru.roll_rotate_step = roll_rotate_step;

    DObjGetStruct(item_gobj)->rotate.vec.f.z += roll_rotate_step;

    return FALSE;
}

/* it/itcommon/ittaru.c:373-385 itTaruRollProcMap 0x8017A148, verbatim:
 * rolling off a ledge drops it back to falling; rolling into a wall
 * breaks it. */
sb32 itTaruRollProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestLRWallCheckFloor(item_gobj) == FALSE)
    {
        itMainSetStatus(item_gobj, dITTaruStatusDescs, nITTaruStatusDropped);
    }
    else if (ip->coll_data.mask_curr & (MAP_FLAG_RWALL | MAP_FLAG_LWALL))
    {
        return itTaruCommonProcHit(item_gobj);
    }
    return FALSE;
}

/* it/itcommon/ittaru.c:387-404 itTaruMakeItem 0x8017A1B8, verbatim. No
 * DObj tail and no rotate: a Barrel's tree is the baked model plus
 * whatever the base carries, and its shape is set by the transition into
 * Thrown, not at birth. */
GObj* itTaruMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITTaruItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        ip->item_vars.taru.roll_rotate_step = 0.0F;

        ip->is_damage_all = TRUE;

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}

/* it/itcommon/ittaru.c:406-431 itTaruExplodeInitVars 0x8017A240,
 * verbatim. Note the two lines the other Containers' twins do not have:
 * `can_hop` is left alone (Box, Capsule and Egg all write FALSE) and
 * `can_rehit_item` is TRUE. */
void itTaruExplodeInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi = 0;
    ip->event_id = 0;

    ip->attack_coll.fgm_id = nSYAudioFGMExplodeL;

    ip->attack_coll.can_rehit_item = TRUE;
    ip->attack_coll.can_reflect = FALSE;

    ip->attack_coll.throw_mul = ITEM_THROW_DEFAULT;
    ip->attack_coll.element = nGMHitElementFire;

    ip->attack_coll.can_setoff = FALSE;

    ip->damage_coll.hitstatus = nGMHitStatusNone;

    itMainClearOwnerStats(item_gobj);
    itMainRefreshAttackColl(item_gobj);
    itMainUpdateAttackEvent(item_gobj, itGetAttackEvent(dITTaruItemDesc, llITCommonDataTaruAttackEvents));
}

/* it/itcommon/ittaru.c:433-438 itTaruExplodeSetStatus 0x8017A2D8,
 * verbatim. */
void itTaruExplodeSetStatus(GObj *item_gobj)
{
    itTaruExplodeInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITTaruStatusDescs, nITTaruStatusExplode);
}

/* it/itcommon/ittaru.c:440-464 itTaruExplodeMakeEffectGotoSetStatus
 * 0x8017A30C, verbatim. */
void itTaruExplodeMakeEffectGotoSetStatus(GObj *item_gobj)
{
    LBParticle *pc;
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    ip->attack_coll.attack_state = nGMAttackStateOff;

    ip->physics.vel_air.x = 0.0F;
    ip->physics.vel_air.y = 0.0F;
    ip->physics.vel_air.z = 0.0F;

    pc = efManagerSparkleWhiteMultiExplodeMakeEffect(&dobj->translate.vec.f);

    if (pc != NULL)
    {
        pc->xf->scale.x = pc->xf->scale.y = pc->xf->scale.z = ITTARU_EXPLODE_SCALE;
    }
    efManagerQuakeMakeEffect(1);

    DObjGetStruct(item_gobj)->flags = DOBJ_FLAG_HIDDEN;

    itTaruExplodeSetStatus(item_gobj);
}
