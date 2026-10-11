/* itrshell.c -- it/itcommon/itrshell.c, verbatim except one function
 * left out (see below): the Red Shell's `ITDesc`, its seven-state
 * `ITStatusDesc` table (ground wait / air fall / fighter hold / fighter
 * throw / fighter drop / ground spin / air spin), and their proc bodies.
 * Function-for-function against the game's own ITStruct (it/ittypes.h);
 * every function names its decomp line range.
 *
 * The roster's twelfth entry, and Green Shell's
 * own Red Shell twin -- confirmed by `src/relocData/251_ITCommonData.c`
 * itself, whose `RShell_ItemAttributes` reuses the SAME `.data`/
 * `.p_mobjsubs` pointers as `GShell_ItemAttributes`, and by
 * `itRShellSpinAddAnim`'s own decomp comment ("Identical to Green Shell
 * function") -- but NOT a mere reskin: it is a real, substantially
 * different item read fresh, not guessed from name/size alone. Checked
 * for the Container-item shape: no call anywhere in this
 * file reaches `itMainMakeContainerItem`.
 *
 * What makes the Red Shell genuinely different from the Green Shell's
 * own Spin state, all confirmed by reading this file's own text, not by
 * assuming the twin transferred unchanged:
 *
 * - The Red Shell HOMES: `itRShellSpinSearchFollowPlayer` scans every
 *   GObj on `nGCCommonLinkIDFighter` for the nearest by squared XY
 *   distance and hands it to `itRShellSpinUpdateFollowPlayer`, which
 *   nudges `vel_air.x` toward that fighter each frame while grounded.
 *   Green Shell's own Spin never reads another GObj's position at all.
 * - It bounces off STAGE EDGES, not walls: `itRShellSpinCheckCollisionEdge`
 *   reads `mpCollisionGetFloorEdgeL`/`GetFloorEdgeR` directly against the
 *   item's own half-width (`ITAttributes.map_coll_width`) and inverts
 *   `vel_air.x`/`item_vars.shell.vel_x` through `itRShellSpinEdgeInvertVelLR`
 *   -- a second, edge-specific collision path Green Shell's own
 *   `itMapCheckLRWallProcNoFloor`/`itMapCheckCollideAllRebound` pair
 *   never needed.
 * - It carries a SEPARATE `interact` counter (`ITRSHELL_INTERACT_MAX`,
 *   24) on top of the shared `health` field -- `itRShellCommonProcHit`/
 *   `SpinProcDamage`/`CommonProcReflector` all decrement it and destroy
 *   the shell outright (return TRUE) at zero, independent of `health`'s
 *   own randomized reset.
 * - It has its OWN dedicated reflector, `itRShellCommonProcReflector` --
 *   not the shared `itMainCommonProcReflector` every other companion
 *   item and Green Shell's own Spin/SpinAir rows use -- which biases the
 *   shell's `lr`/`vel_air.x` toward whichever side of the reflecting
 *   fighter it lands on, then adds a fixed `ITRSHELL_ADD_VEL_X` kick.
 * - `itRShellCommonSetStatusWaitOrSpin`'s three branches match Green
 *   Shell's own `WaitInitVars` shape closely, but this file collapses
 *   the `WaitSetStatus` wrapper into `itRShellCommonProcStatusWaitOrSpin`
 *   (used as the `itMapCheckDestroyDropped`/`itMapCheckLanding` callback
 *   directly) rather than keeping a separate `WaitSetStatus` name.
 * - `ITDesc.proc_damage` is wired directly to `itRShellCommonProcDamage`
 *   at the top level (a freshly spawned, not-yet-statused item can take
 *   damage) -- Green Shell's own `ITDesc.proc_damage` is `NULL` there,
 *   only wired per status row.
 *
 * Same reloc-offset arithmetic as Green Shell's own `itGShellSpinAddAnim`
 * (`itGetPData`, `it/item.h`) over the SAME three symbols
 * (`llITCommonDataShellDataStart`/`AnimJoint`/`MatAnimJoint`) -- no new
 * stand-ins needed beyond this file's own `ITDesc.o_attributes` one
 * (`llITCommonDataRShellItemAttributes`).
 *
 * Twenty-eight of the file's twenty-nine functions ported;
 * `itRShellMakeItem` is ported now, and it is the decomp's line for
 * line. It was the last function in this file held out, and it was
 * held out for exactly one reason: its tail calls `ifCommonItemArrow-
 * MakeInterface(ip)`, which needed the unexported `IFCommonItem` file
 * (relocData 0x57). That file is exported -- not as a
 * pack region but as an ordinary sprite bank, `romdisk/ifcommonitem.spr`,
 * because it is one sprite and nothing else (src/dc/ifcommon.c). So the
 * tail resolves and the body is verbatim.
 *
 * The paragraph that stood here described the gap at length, including
 * why no honest refuse-first guard existed for it (`gcAddSObjForGObj`
 * never returns NULL for a NULL sprite, so the decomp's own `!= NULL`
 * check cannot catch a missing one). That reasoning was sound and the
 * blocker it argued from is now gone; it is replaced rather than kept,
 * because a note that says a ported function is left out is worse than
 * no note at all.
 */
#include <it/item.h>
#include "itemoffsets.h"     /* llITCommonData* offsets */
#include <if/ifcommon.h>     /* ifCommonItemArrowMakeInterface */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

ITDesc dITRShellItemDesc =
{
    nITKindRShell,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataRShellItemAttributes,
    { nGCMatrixKindNull, nGCMatrixKindNull, 0 },
    nGMAttackStateOff,
    itRShellFallProcUpdate,
    itRShellFallProcMap,
    NULL, NULL, NULL, NULL, NULL,
    itRShellCommonProcDamage
};

ITStatusDesc dITRShellStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    { NULL, itRShellWaitProcMap, NULL, NULL, NULL, NULL, NULL, itRShellCommonProcDamage },
    /* Status 1 (Air Wait Fall) */
    { itRShellFallProcUpdate, itRShellFallProcMap, NULL, NULL, NULL, NULL, NULL, NULL },
    /* Status 2 (Fighter Hold) */
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    /* Status 3 (Fighter Throw) */
    { itRShellFallProcUpdate, itRShellThrownProcMap, itRShellCommonProcHit,
      itRShellCommonProcShield, itMainCommonProcHop, itRShellCommonProcShield,
      itRShellCommonProcReflector, itRShellCommonProcDamage },
    /* Status 4 (Fighter Drop) */
    { itRShellFallProcUpdate, itRShellThrownProcMap, itRShellCommonProcHit,
      itRShellCommonProcShield, itMainCommonProcHop, itRShellCommonProcShield,
      itRShellCommonProcReflector, itRShellCommonProcDamage },
    /* Status 5 (Ground Spin) */
    { itRShellSpinProcUpdate, itRShellSpinProcMap, itRShellCommonProcHit,
      itRShellCommonProcHit, NULL, NULL, itRShellCommonProcReflector, itRShellSpinProcDamage },
    /* Status 6 (Air Spin) */
    { itRShellFallProcUpdate, itRShellThrownProcMap, itRShellCommonProcHit,
      itRShellCommonProcHit, NULL, NULL, itRShellCommonProcReflector, itRShellCommonProcDamage }
};

enum itRShellStatus
{
    nITRShellStatusWait, nITRShellStatusFall, nITRShellStatusHold,
    nITRShellStatusThrown, nITRShellStatusDropped, nITRShellStatusSpin,
    nITRShellStatusSpinAir, nITRShellStatusEnumCount
};

/* 0x8017A3A0 */
void itRShellSpinUpdateFollowPlayer(GObj *item_gobj, GObj *fighter_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    f32 vel_x;
    f32 dist_x;
    s32 lr_vel;
    s32 lr_dist;

    if (ip->ga == nMPKineticsGround)
    {
        dist_x = (DObjGetStruct(fighter_gobj)->translate.vec.f.x - DObjGetStruct(item_gobj)->translate.vec.f.x);

        lr_dist = (dist_x < 0.0F) ? -1 : +1;

        vel_x = lr_dist * ITRSHELL_MUL_VEL_X;

        ip->item_vars.shell.vel_x = vel_x;

        ip->physics.vel_air.x += vel_x;

        lr_vel = (ip->physics.vel_air.x < 0.0F) ? -1 : +1;

        lr_dist = (ip->item_vars.shell.vel_x < 0.0F) ? -1 : +1;

        if (lr_dist == lr_vel)
        {
            if (ABSF(ip->physics.vel_air.x) > ITRSHELL_CLAMP_VEL_X)
            {
                ip->physics.vel_air.x = ip->lr * ITRSHELL_CLAMP_VEL_X;
            }
        }
        if (ip->attack_coll.attack_state == nGMAttackStateOff)
        {
            if (ABSF(ip->physics.vel_air.x) <= ITRSHELL_HIT_INITVEL_X)
            {
                ip->attack_coll.attack_state = nGMAttackStateNew;

                itProcessUpdateAttackPositions(item_gobj);
            }
        }
        ip->lr = (ip->physics.vel_air.x < 0.0F) ? -1 : +1;
    }
}

/* 0x8017A534 */
void itRShellSpinSearchFollowPlayer(GObj *item_gobj)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
    GObj *nearest_gobj;
    DObj *dobj = DObjGetStruct(item_gobj);
    Vec3f *translate = &dobj->translate.vec.f;
    s32 ft_count = 0;
    f32 next_dist;
    f32 nearest_dist;
    Vec3f dist;

    while (fighter_gobj != NULL)
    {
        syVectorDiff3D(&dist, &DObjGetStruct(fighter_gobj)->translate.vec.f, translate);

        if (ft_count == 0)
        {
            nearest_dist = SQUARE(dist.x) + SQUARE(dist.y);
        }
        next_dist = SQUARE(dist.x) + SQUARE(dist.y);

        if (nearest_dist >= next_dist)
        {
            nearest_dist = next_dist;

            nearest_gobj = fighter_gobj;
        }
        fighter_gobj = fighter_gobj->link_next;

        ft_count++;
    }
    itRShellSpinUpdateFollowPlayer(item_gobj, nearest_gobj);
}

/* 0x8017A610 */
void itRShellSpinUpdateGFX(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if (ip->item_vars.shell.dust_effect_int == 0)
    {
        Vec3f pos = dobj->translate.vec.f;

        pos.y += ip->attr->map_coll_bottom;

        efManagerDustLightMakeEffect(&pos, ip->lr, 1.0F);

        ip->item_vars.shell.dust_effect_int = ITRSHELL_EFFECT_SPAWN_INT;
    }
    ip->item_vars.shell.dust_effect_int--;
}

/* 0x8017A6A0 -- identical to Green Shell's own itGShellSpinAddAnim,
 * per the decomp's own comment; same three reloc-offset symbols. */
void itRShellSpinAddAnim(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    gcAddDObjAnimJoint(dobj, itGetPData(ip, llITCommonDataShellDataStart, llITCommonDataShellAnimJoint), 0.0F);
    gcAddMObjMatAnimJoint(dobj->mobj, itGetPData(ip, llITCommonDataShellDataStart, llITCommonDataShellMatAnimJoint), 0.0F);
    gcPlayAnimAll(item_gobj);
}

/* 0x8017A734 */
void itRShellCommonClearAnim(GObj *item_gobj)
{
    DObjGetStruct(item_gobj)->mobj->matanim_joint.event32 = NULL;
    DObjGetStruct(item_gobj)->anim_joint.event32 = NULL;
}

/* 0x8017A74C */
sb32 itRShellFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITRSHELL_GRAVITY, ITRSHELL_TVEL);

    if (!(ip->item_vars.shell.damage_all_delay))
    {
        itMainClearOwnerStats(item_gobj);

        ip->item_vars.shell.damage_all_delay = -1;
    }
    if (ip->item_vars.shell.damage_all_delay != -1)
    {
        ip->item_vars.shell.damage_all_delay--;
    }
    return FALSE;
}

/* 0x8017A7C4 */
sb32 itRShellWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itRShellFallSetStatus);

    return FALSE;
}

/* 0x8017A7EC */
sb32 itRShellFallProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->item_vars.shell.health == 0)
    {
        return itMapCheckDestroyLanding(item_gobj, ITRSHELL_MAP_REBOUND_COMMON);
    }
    itMapCheckDestroyDropped(item_gobj, ITRSHELL_MAP_REBOUND_COMMON, ITRSHELL_MAP_REBOUND_GROUND, itRShellCommonProcStatusWaitOrSpin);

    return FALSE;
}

/* 0x8017A83C */
void itRShellCommonSetStatusWaitOrSpin(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMapSetGround(ip);

    if (ABSF(ip->physics.vel_air.x) < ITRSHELL_STOP_VEL_X)
    {
        itMainSetGroundAllowPickup(item_gobj);

        ip->item_vars.shell.is_damage = FALSE;
        ip->physics.vel_air.x = 0.0F;

        itMainClearOwnerStats(item_gobj);

        ip->damage_coll.hitstatus = nGMHitStatusNormal;
        ip->attack_coll.attack_state = nGMAttackStateOff;

        itRShellCommonClearAnim(item_gobj);
        itMainSetStatus(item_gobj, dITRShellStatusDescs, nITRShellStatusWait);
    }
    else if (ip->item_vars.shell.is_damage != FALSE)
    {
        ip->attack_coll.attack_state = nGMAttackStateNew;

        itProcessUpdateAttackPositions(item_gobj);
        itRShellSpinSetStatus(item_gobj);
    }
    else
    {
        itMainSetGroundAllowPickup(item_gobj);

        ip->physics.vel_air.x = 0.0F;

        itMainClearOwnerStats(item_gobj);

        ip->damage_coll.hitstatus = nGMHitStatusNormal;
        ip->attack_coll.attack_state = nGMAttackStateOff;

        itRShellCommonClearAnim(item_gobj);
        itMainSetStatus(item_gobj, dITRShellStatusDescs, nITRShellStatusWait);
    }
}

/* 0x8017A964 -- kept as its own function, verbatim, since it is the
 * callback name `itMapCheckDestroyDropped`/`itMapCheckLanding` need
 * (a thin wrapper, unlike Green Shell's own separately-named
 * WaitSetStatus). */
void itRShellCommonProcStatusWaitOrSpin(GObj *item_gobj)
{
    itRShellCommonSetStatusWaitOrSpin(item_gobj);
}

/* 0x8017A984 */
void itRShellFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    ip->damage_coll.hitstatus = nGMHitStatusNormal;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITRShellStatusDescs, nITRShellStatusFall);
}

/* 0x8017A9D0 */
sb32 itRShellCommonProcDamage(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->physics.vel_air.x = ip->damage_queue * ITRSHELL_DAMAGE_MUL_NORMAL * (-ip->damage_lr);

    if (ABSF(ip->physics.vel_air.x) > ITRSHELL_STOP_VEL_X)
    {
        ip->item_vars.shell.is_damage = TRUE;

        ip->attack_coll.attack_state = nGMAttackStateNew;

        itProcessUpdateAttackPositions(item_gobj);
        itMainCopyDamageStats(item_gobj);

        if (ip->ga != nMPKineticsGround)
        {
            itRShellSpinAirSetStatus(item_gobj);
        }
        else itRShellSpinSetStatus(item_gobj);
    }
    else
    {
        ip->physics.vel_air.x = 0.0F;

        ip->attack_coll.attack_state = nGMAttackStateOff;
    }
    return FALSE;
}

/* 0x8017AABC */
void itRShellHoldSetStatus(GObj *item_gobj)
{
    DObjGetStruct(item_gobj)->rotate.vec.f.y = F_CST_DTOR32(0.0F);

    itMainSetStatus(item_gobj, dITRShellStatusDescs, nITRShellStatusHold);
}

/* 0x8017AAF0 */
void itRShellThrownSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->item_vars.shell.health = 1;
    ip->item_vars.shell.is_damage = TRUE;
    ip->item_vars.shell.damage_all_delay = ITRSHELL_DAMAGE_ALL_WAIT;

    ip->times_thrown = 0;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITRShellStatusDescs, nITRShellStatusThrown);
}

/* 0x8017AB48 */
void itRShellDroppedSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->item_vars.shell.health = 1;
    ip->item_vars.shell.is_damage = TRUE;
    ip->item_vars.shell.damage_all_delay = ITRSHELL_DAMAGE_ALL_WAIT;

    ip->times_thrown = 0;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITRShellStatusDescs, nITRShellStatusDropped);
}

/* 0x8017ABA0 */
sb32 itRShellThrownProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapCheckLanding(item_gobj, 0.25F, 0.5F, itRShellSpinSetStatus) != FALSE)
    {
        if (ip->physics.vel_air.x < 0.0F)
        {
            ip->lr = -1;
        }
        else ip->lr = +1;

        ip->physics.vel_air.x = ((ip->lr * -8.0F) + -10.0F) * 0.7F;
    }
    return FALSE;
}

/* 0x8017AC40 -- lr parameter: 0 = left, 1 = right. */
void itRShellSpinEdgeInvertVelLR(GObj *item_gobj, ub8 lr)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->physics.vel_air.x = -ip->physics.vel_air.x;

    ip->item_vars.shell.vel_x = -ip->item_vars.shell.vel_x;

    if (lr != 0)
    {
        ip->lr = +1;
    }
    else ip->lr = -1;
}

/* 0x8017AC84 */
void itRShellSpinCheckCollisionEdge(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    ITAttributes *attr = ip->attr;
    DObj *joint = DObjGetStruct(item_gobj);
    Vec3f pos;

    if (mpCollisionCheckExistLineID(ip->coll_data.floor_line_id) != FALSE)
    {
        if (ip->lr == -1)
        {
            mpCollisionGetFloorEdgeL(ip->coll_data.floor_line_id, &pos);

            if (pos.x >= (joint->translate.vec.f.x - attr->map_coll_width))
            {
                itRShellSpinEdgeInvertVelLR(item_gobj, 1);
            }
        }
        else
        {
            mpCollisionGetFloorEdgeR(ip->coll_data.floor_line_id, &pos);

            if (pos.x <= (joint->translate.vec.f.x + attr->map_coll_width))
            {
                itRShellSpinEdgeInvertVelLR(item_gobj, 0);
            }
        }
    }
}

/* 0x8017AD7C */
sb32 itRShellSpinProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itRShellSpinUpdateGFX(item_gobj);
    itRShellSpinSearchFollowPlayer(item_gobj);
    itRShellSpinCheckCollisionEdge(item_gobj);

    if (ip->lifetime == 0)
    {
        return TRUE;
    }
    else ip->lifetime--;

    return FALSE;
}

/* 0x8017ADD4 */
sb32 itRShellSpinProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if ((itMapCheckLRWallProcNoFloor(item_gobj, itRShellSpinAirSetStatus) != FALSE) && (ip->coll_data.mask_curr & (MAP_FLAG_RWALL | MAP_FLAG_LWALL)))
    {
        ip->physics.vel_air.x = -ip->physics.vel_air.x;

        itMainSetSpinVelLR(item_gobj);
        itMainClearOwnerStats(item_gobj);

        ip->item_vars.shell.vel_x = -ip->item_vars.shell.vel_x;
    }
    return FALSE;
}

/* 0x8017AE48 */
sb32 itRShellCommonProcHit(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->item_vars.shell.interact--;

    if (ip->item_vars.shell.interact == 0)
    {
        return TRUE;
    }
    ip->damage_coll.hitstatus = nGMHitStatusNormal;

    ip->item_vars.shell.health = syUtilsRandIntRange(ITRSHELL_HEALTH_MAX);

    ip->physics.vel_air.x = ((ip->physics.vel_air.x * -1.0F) + (ITRSHELL_RECOIL_VEL_X * ip->hit_lr)) * ITRSHELL_RECOIL_MUL_X;

    itRShellCommonClearAnim(item_gobj);

    if (ip->ga != nMPKineticsGround)
    {
        itRShellSpinAirSetStatus(item_gobj);
    }
    else itRShellSpinSetStatus(item_gobj);

    return FALSE;
}

/* 0x8017AF18 */
sb32 itRShellSpinProcDamage(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->item_vars.shell.interact--;

    if (ip->item_vars.shell.interact == 0)
    {
        return TRUE;
    }
    ip->physics.vel_air.x += (ip->damage_queue * 2.0F) * -ip->damage_lr;

    if (ABSF(ip->physics.vel_air.x) > ITRSHELL_STOP_VEL_X)
    {
        ip->attack_coll.attack_state = nGMAttackStateNew;

        itProcessUpdateAttackPositions(item_gobj);
        itMainCopyDamageStats(item_gobj);
        itRShellSpinSetStatus(item_gobj);
    }
    else
    {
        ip->attack_coll.attack_state = nGMAttackStateOff;
    }
    return FALSE;
}

/* 0x8017AFEC */
void itRShellSpinInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;
    ip->pickup_wait = ITEM_PICKUP_WAIT_DEFAULT;

    if (ip->physics.vel_air.x > ITRSHELL_CLAMP_VEL_X)
    {
        ip->physics.vel_air.x = ITRSHELL_CLAMP_VEL_X;
    }
    if (ip->physics.vel_air.x < -ITRSHELL_CLAMP_VEL_X)
    {
        ip->physics.vel_air.x = -ITRSHELL_CLAMP_VEL_X;
    }
    ip->physics.vel_air.y = 0.0F;

    if (ip->physics.vel_air.x < 0.0F)
    {
        ip->lr = -1;
    }
    else ip->lr = +1;

    if (ip->item_vars.shell.is_setup_vars == FALSE)
    {
        ip->lifetime = ITRSHELL_LIFETIME;

        ip->item_vars.shell.is_setup_vars = TRUE;

        ip->item_vars.shell.interact = ITRSHELL_INTERACT_MAX;
    }
    ip->item_vars.shell.dust_effect_int = ITRSHELL_EFFECT_SPAWN_INT;

    itRShellSpinAddAnim(item_gobj);
    func_800269C0_275C0(nSYAudioFGMBombHeiWalkStart);
    itMainClearOwnerStats(item_gobj);
    itMapSetGround(ip);
}

/* 0x8017B0D4 */
void itRShellSpinSetStatus(GObj *item_gobj)
{
    itRShellSpinInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITRShellStatusDescs, nITRShellStatusSpin);
}

/* 0x8017B108 */
void itRShellSpinAirInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    if (ip->physics.vel_air.x > ITRSHELL_CLAMP_AIR_X)
    {
        ip->physics.vel_air.x = ITRSHELL_CLAMP_AIR_X;
    }
    if (ip->physics.vel_air.x < -ITRSHELL_CLAMP_AIR_X)
    {
        ip->physics.vel_air.x = -ITRSHELL_CLAMP_AIR_X;
    }
    if (ip->physics.vel_air.x < 0.0F)
    {
        ip->lr = -1;
    }
    else ip->lr = +1;

    itMainClearOwnerStats(item_gobj);
    itMapSetAir(ip);
}

/* 0x8017B1A4 */
void itRShellSpinAirSetStatus(GObj *item_gobj)
{
    itRShellSpinAirInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITRShellStatusDescs, nITRShellStatusSpinAir);
}

/* it/itcommon/itrshell.c:675-715 itRShellMakeItem 0x8017B1D8, verbatim.
 *
 * (The function is ported; a stale "LEFT OUT" marker said it was
 * absent while the function sat directly below it. Removed rather than
 * reworded, because the reason it existed is gone.) */
GObj* itRShellMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITRShellItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        DObj *dobj = DObjGetStruct(item_gobj);
        ITStruct *ip;
#if defined(REGION_US)
        Vec3f translate = dobj->translate.vec.f;
#endif

        dobj->rotate.vec.f.y = F_CST_DTOR32(90.0F);

        gcAddXObjForDObjFixed(dobj, nGCMatrixKindTraRotRpyR, 0);
        gcAddXObjForDObjFixed(dobj, 0x48, 0);

#if defined(REGION_US)
        dobj->translate.vec.f = translate;
#else
        dobj->translate.vec.f = *pos;
#endif

        dobj->mobj->palette_id = 0.0F;

        ip = itGetStruct(item_gobj);

        ip->attack_coll.can_rehit_shield = TRUE;

        ip->item_vars.shell.health = 1;
        ip->item_vars.shell.is_setup_vars = FALSE;
        ip->item_vars.shell.is_damage = FALSE;
        ip->item_vars.shell.damage_all_delay = -1;
        ip->item_vars.shell.vel_x = 0;

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}
/* 0x8017B2F8 */
sb32 itRShellCommonProcShield(GObj *item_gobj)
{
    itMainVelSetRebound(item_gobj);

    return FALSE;
}

/* 0x8017B31C -- Red Shell's own dedicated reflector, unlike Green
 * Shell's shared itMainCommonProcReflector: biases lr/vel_air.x toward
 * whichever side of the reflecting fighter (ip->owner_gobj) the shell
 * lands on, then adds a fixed kick. */
sb32 itRShellCommonProcReflector(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *item_dobj = DObjGetStruct(item_gobj), *fighter_dobj = DObjGetStruct(ip->owner_gobj);

    ip->item_vars.shell.interact--;

    if (ip->item_vars.shell.interact == 0)
    {
        return TRUE;
    }

    if (item_dobj->translate.vec.f.x < fighter_dobj->translate.vec.f.x)
    {
        ip->lr = -1;

        if (ip->physics.vel_air.x >= 0.0F)
        {
            ip->physics.vel_air.x = -ip->physics.vel_air.x;
            ip->item_vars.shell.vel_x = -ip->item_vars.shell.vel_x;
        }
    }
    else
    {
        ip->lr = +1;

        if (ip->physics.vel_air.x < 0.0F)
        {
            ip->physics.vel_air.x = -ip->physics.vel_air.x;
            ip->item_vars.shell.vel_x = -ip->item_vars.shell.vel_x;
        }
    }
    ip->physics.vel_air.x += (ITRSHELL_ADD_VEL_X * ip->lr);

    itMainClearOwnerStats(item_gobj);

    return FALSE;
}

