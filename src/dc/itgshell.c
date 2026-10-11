/* itgshell.c -- it/itcommon/itgshell.c, verbatim except one function left
 * out (see below): the Green Shell's `ITDesc`, its seven-state
 * `ITStatusDesc` table (ground wait / air fall / fighter hold / fighter
 * throw / fighter drop / ground spin / air spin), and their proc bodies.
 * Function-for-function against the game's own ITStruct (it/ittypes.h);
 * every function names its decomp line range.
 *
 * The roster's eleventh entry, and the first item with a
 * genuine self-propelled "spin" attack state and its own hit-point pool
 * -- a bigger state machine than any prior item (seven states, against
 * Sword/Bat/Hammer/Fan's five): a kicked or thrown shell slides to a
 * Wait, but a shell that took enough damage instead becomes a live,
 * damage-dealing Spin (ground) or SpinAir projectile that only stops
 * when its own `health` (1-4, randomized on the hit that starts a new
 * Spin cycle) or its slide speed runs out. Checked against the
 * Container-item finding: no call anywhere in this
 * file reaches `itMainMakeContainerItem`.
 *
 * `itGShellSpinAddAnim` needed a THIRD kind of reloc-offset arithmetic
 * this port hadn't used yet: `itGetPData(ip, off1, off2)` (it/item.h),
 * which computes `(ip->attr->data - off1) + off2` -- a delta between two
 * offsets in the SAME reloc file, rather than the single-offset
 * `o_attributes` pattern every prior item/weapon `Desc` has used. Its
 * three symbols (`llITCommonDataShellDataStart`/`AnimJoint`/
 * `MatAnimJoint`) are real, declared blocks (`tools/
 * relocFileDescriptions.us.txt`'s "FILE CONTENTS" section, not a per-file
 * header extern) -- confirmed before writing this file, since a symbol
 * with no declaration anywhere would need the narrower
 * treatment instead. Three more `int llFoo;` stand-ins, same shape as
 * every `o_attributes` stand-in, joining this file's own
 * `llITCommonDataGShellItemAttributes`. Unlike `o_attributes`, these two
 * are actually DEREFERENCED at their real call site
 * (`gcAddDObjAnimJoint`/`gcAddMObjMatAnimJoint`'s own tail through
 * `gcPlayAnimAll`, unmodified `sys/objanim.c`) -- worth noting, though it
 * needed no change here: a BSS-zeroed `int llFoo;`, read back as the
 * `AObjEvent32` word the real engine expects, decodes to opcode 0
 * (`nGCAnimEvent32End`) by construction, so it is already a safe,
 * minimal "do nothing, end immediately" script with no further work.
 *
 * `itGShellMakeItem` is the decomp's line for line. Its tail calls `ifCommonItemArrow-
 * MakeInterface(ip)`, which reads the `IFCommonItem` file
 * (relocData 0x57), carried not as a
 * pack region but as an ordinary sprite bank, `romdisk/ifcommonitem.spr`,
 * because it is one sprite and nothing else (src/dc/ifcommon.c). So the
 * tail resolves and the body is verbatim. */
#include <it/item.h>
#include "itemoffsets.h"     /* llITCommonData* offsets */
#include <if/ifcommon.h>     /* ifCommonItemArrowMakeInterface */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif


ITDesc dITGShellItemDesc =
{
    nITKindGShell,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataGShellItemAttributes,
    { nGCMatrixKindNull, nGCMatrixKindNull, 0 },
    nGMAttackStateOff,
    itGShellFallProcUpdate,
    itGShellFallProcMap,
    NULL, NULL, NULL, NULL, NULL, NULL
};

ITStatusDesc dITGShellStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    { NULL, itGShellWaitProcMap, NULL, NULL, NULL, NULL, NULL, itGShellCommonProcDamage },
    /* Status 1 (Air Wait Fall) */
    { itGShellFallProcUpdate, itGShellFallProcMap, NULL, NULL, NULL, NULL, NULL, NULL },
    /* Status 2 (Fighter Hold) */
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    /* Status 3 (Fighter Throw) */
    { itGShellThrownProcUpdate, itGShellThrownProcMap, itGShellCommonProcHit,
      itGShellCommonProcShield, itMainCommonProcHop, itGShellCommonProcShield,
      itMainCommonProcReflector, itGShellCommonProcDamage },
    /* Status 4 (Fighter Drop) */
    { itGShellFallProcUpdate, itGShellThrownProcMap, itGShellCommonProcHit,
      itGShellCommonProcShield, itMainCommonProcHop, itGShellCommonProcShield,
      itMainCommonProcReflector, itGShellCommonProcDamage },
    /* Status 5 (Ground Spin) */
    { itGShellSpinProcUpdate, itGShellSpinProcMap, itGShellCommonProcHit,
      itGShellCommonProcHit, NULL, NULL, itMainCommonProcReflector, itGShellSpinProcDamage },
    /* Status 6 (Air Spin) */
    { itGShellFallProcUpdate, itGShellThrownProcMap, itGShellCommonProcHit,
      itGShellCommonProcHit, NULL, NULL, itMainCommonProcReflector, itGShellSpinProcDamage }
};

enum itGShellStatus
{
    nITGShellStatusWait, nITGShellStatusFall, nITGShellStatusHold,
    nITGShellStatusThrown, nITGShellStatusDropped, nITGShellStatusSpin,
    nITGShellStatusSpinAir, nITGShellStatusEnumCount
};

/* 0x801785E0 */
void itGShellSpinUpdateEffect(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    Vec3f pos;

    if (ip->item_vars.shell.dust_effect_int == 0)
    {
        pos = dobj->translate.vec.f;

        pos.y += ip->attr->map_coll_bottom;

        efManagerDustLightMakeEffect(&pos, ip->lr, 1.0F);

        ip->item_vars.shell.dust_effect_int = ITGSHELL_EFFECT_SPAWN_INT;
    }
    ip->item_vars.shell.dust_effect_int--;
}

/* 0x80178670 */
void itGShellSpinAddAnim(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    gcAddDObjAnimJoint(dobj, itGetPData(ip, llITCommonDataShellDataStart, llITCommonDataShellAnimJoint), 0.0F);
    gcAddMObjMatAnimJoint(dobj->mobj, itGetPData(ip, llITCommonDataShellDataStart, llITCommonDataShellMatAnimJoint), 0.0F);
    gcPlayAnimAll(item_gobj);
}

/* 0x80178704 */
void itGShellCommonClearAnim(GObj *item_gobj)
{
    DObjGetStruct(item_gobj)->mobj->matanim_joint.event32 = NULL;
    DObjGetStruct(item_gobj)->anim_joint.event32 = NULL;
}

/* 0x8017871C */
sb32 itGShellFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITGSHELL_GRAVITY, ITGSHELL_TVEL);

    return FALSE;
}

/* 0x8017874C */
sb32 itGShellWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itGShellFallSetStatus);

    return FALSE;
}

/* 0x80178774 */
sb32 itGShellFallProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->item_vars.shell.health == 0)
    {
        return itMapCheckDestroyLanding(item_gobj, ITGSHELL_MAP_REBOUND_COMMON);
    }
    else itMapCheckDestroyDropped(item_gobj, ITGSHELL_MAP_REBOUND_COMMON, ITGSHELL_MAP_REBOUND_GROUND, itGShellWaitSetStatus);

    return FALSE;
}

/* 0x801787CC */
void itGShellWaitInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMapSetGround(ip);

    if (ABSF(ip->physics.vel_air.x) < ITGSHELL_STOP_VEL_X)
    {
        itMainSetGroundAllowPickup(item_gobj);

        ip->item_vars.shell.is_damage = FALSE;

        ip->is_damage_all = TRUE;

        ip->damage_coll.hitstatus = nGMHitStatusNormal;
        ip->attack_coll.attack_state = nGMAttackStateOff;

        ip->physics.vel_air.x = 0.0F;

        itGShellCommonClearAnim(item_gobj);
        itMainSetStatus(item_gobj, dITGShellStatusDescs, nITGShellStatusWait);
    }
    else if (ip->item_vars.shell.is_damage != FALSE)
    {
        ip->attack_coll.attack_state = nGMAttackStateNew;

        itProcessUpdateAttackPositions(item_gobj);
        itGShellSpinSetStatus(item_gobj);
    }
    else
    {
        itMainSetGroundAllowPickup(item_gobj);

        ip->is_damage_all = TRUE;

        ip->damage_coll.hitstatus = nGMHitStatusNormal;
        ip->attack_coll.attack_state = nGMAttackStateOff;

        ip->physics.vel_air.x = 0.0F;

        itGShellCommonClearAnim(item_gobj);
        itMainSetStatus(item_gobj, dITGShellStatusDescs, nITGShellStatusWait);
    }
}

/* 0x80178910 */
void itGShellWaitSetStatus(GObj *item_gobj)
{
    itGShellWaitInitVars(item_gobj);
}

/* 0x80178930 */
void itGShellFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->damage_coll.hitstatus = nGMHitStatusNone;

    ip->attack_coll.attack_state = nGMAttackStateOff;

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITGShellStatusDescs, nITGShellStatusFall);
}

/* 0x8017897C */
sb32 itGShellCommonProcDamage(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->physics.vel_air.x = (ip->damage_queue * ITGSHELL_DAMAGE_MUL_NORMAL * -ip->damage_lr);

    if (ABSF(ip->physics.vel_air.x) > ITGSHELL_STOP_VEL_X)
    {
        ip->item_vars.shell.is_damage = TRUE;

        ip->attack_coll.attack_state = nGMAttackStateNew;

        itProcessUpdateAttackPositions(item_gobj);

        ip->damage_coll.hitstatus = nGMHitStatusNone;

        itMainCopyDamageStats(item_gobj);

        if (ip->ga != nMPKineticsGround)
        {
            itGShellSpinAirSetStatus(item_gobj);
        }
        else itGShellSpinSetStatus(item_gobj);
    }
    else
    {
        ip->physics.vel_air.x = 0.0F;

        if (ip->ga != nMPKineticsGround)
        {
            itGShellFallSetStatus(item_gobj);
        }
        else itGShellWaitSetStatus(item_gobj);
    }
    return FALSE;
}

/* 0x80178A90 */
void itGShellHoldSetStatus(GObj *item_gobj)
{
    DObjGetStruct(item_gobj)->rotate.vec.f.y = 0.0F;

    itMainSetStatus(item_gobj, dITGShellStatusDescs, nITGShellStatusHold);
}

/* 0x80178AC4 */
sb32 itGShellThrownProcMap(GObj *item_gobj)
{
    itMapCheckLanding(item_gobj, ITGSHELL_MAP_REBOUND_COMMON, ITGSHELL_MAP_REBOUND_GROUND, itGShellWaitSetStatus);

    return FALSE;
}

/* 0x80178AF8 */
sb32 itGShellThrownProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITGSHELL_GRAVITY, ITGSHELL_TVEL);

    return FALSE;
}

/* 0x80178B28 */
void itGShellThrownSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->item_vars.shell.health = 1;
    ip->item_vars.shell.is_damage = TRUE;

    itMainSetStatus(item_gobj, dITGShellStatusDescs, nITGShellStatusThrown);
}

/* 0x80178B60 */
void itGShellDroppedSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->item_vars.shell.health = 1;
    ip->item_vars.shell.is_damage = TRUE;

    itMainSetStatus(item_gobj, dITGShellStatusDescs, nITGShellStatusDropped);
}

/* 0x80178B98 */
sb32 itGShellSpinProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itGShellSpinUpdateEffect(item_gobj);

    if (!(ip->item_vars.shell.damage_all_delay))
    {
        ip->is_damage_all = TRUE;

        ip->item_vars.shell.damage_all_delay = -1;
    }
    if (ip->item_vars.shell.damage_all_delay != -1)
    {
        ip->item_vars.shell.damage_all_delay--;
    }
    if (ip->lifetime == 0)
    {
        return TRUE;
    }
    else ip->lifetime--;

    return FALSE;
}

/* 0x80178C10
 * OVERSIGHT (?), kept verbatim per doctrine: this sets the shell's status
 * to Fall when leaving a grounded spin over an edge, so its hitbox goes
 * off while airborne. The decomp's own comment names the presumed fix
 * (itGShellSpinAirSetStatus) but keeps the original call.
 */
sb32 itGShellSpinProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itGShellFallSetStatus);

    if (itMapCheckCollideAllRebound(item_gobj, (MAP_FLAG_CEIL | MAP_FLAG_RWALL | MAP_FLAG_LWALL), 0.2F, NULL) != FALSE)
    {
        itMainSetSpinVelLR(item_gobj);
        itMainClearOwnerStats(item_gobj);
    }
    return FALSE;
}

/* 0x80178C6C */
sb32 itGShellCommonProcHit(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->damage_coll.hitstatus = nGMHitStatusNormal;

    ip->item_vars.shell.health = syUtilsRandIntRange(ITGSHELL_HEALTH_MAX);

    ip->physics.vel_air.y = ITGSHELL_REBOUND_VEL_Y;

    ip->physics.vel_air.x = syUtilsRandFloat() * (-ip->physics.vel_air.x * ITGSHELL_REBOUND_MUL_X);

    itMainClearOwnerStats(item_gobj);
    itGShellCommonClearAnim(item_gobj);
    itGShellFallSetStatus(item_gobj);

    return FALSE;
}

/* 0x80178CF8 */
sb32 itGShellSpinProcDamage(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->physics.vel_air.x += (ip->damage_queue * ITGSHELL_DAMAGE_MUL_ADD * -ip->damage_lr);

    if (ABSF(ip->physics.vel_air.x) > ITGSHELL_STOP_VEL_X)
    {
        ip->attack_coll.attack_state = nGMAttackStateNew;

        itProcessUpdateAttackPositions(item_gobj);
        itMainCopyDamageStats(item_gobj);

        if (ip->ga != nMPKineticsGround)
        {
            itGShellSpinAirSetStatus(item_gobj);
        }
        else itGShellSpinSetStatus(item_gobj);
    }
    else
    {
        ip->physics.vel_air.x = 0.0F;

        if (ip->ga != nMPKineticsGround)
        {
            itGShellFallSetStatus(item_gobj);
        }
        else itGShellWaitSetStatus(item_gobj);
    }
    return FALSE;
}

/* 0x80178E04 */
void itGShellSpinInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    ip->pickup_wait = ITEM_PICKUP_WAIT_DEFAULT;

    if (ip->physics.vel_air.x > ITGSHELL_CLAMP_VEL_X)
    {
        ip->physics.vel_air.x = ITGSHELL_CLAMP_VEL_X;
    }
    if (ip->physics.vel_air.x < -ITGSHELL_CLAMP_VEL_X)
    {
        ip->physics.vel_air.x = -ITGSHELL_CLAMP_VEL_X;
    }
    ip->physics.vel_air.y = 0.0F;

    if (ip->physics.vel_air.x < 0.0F)
    {
        ip->lr = -1;
    }
    else ip->lr = +1;

    ip->item_vars.shell.dust_effect_int = ITGSHELL_EFFECT_SPAWN_INT;
    ip->item_vars.shell.damage_all_delay = ITGSHELL_DAMAGE_ALL_WAIT;

    itGShellSpinAddAnim(item_gobj);

    ip->is_damage_all = FALSE;

    itMainRefreshAttackColl(item_gobj);
    func_800269C0_275C0(nSYAudioFGMBombHeiWalkStart);
}

/* 0x80178EDC */
void itGShellSpinSetStatus(GObj *item_gobj)
{
    itGShellSpinInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITGShellStatusDescs, nITGShellStatusSpin);
}

/* 0x80178F10 */
void itGShellSpinAirInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->physics.vel_air.x > ITGSHELL_CLAMP_VEL_X)
    {
        ip->physics.vel_air.x = ITGSHELL_CLAMP_VEL_X;
    }
    if (ip->physics.vel_air.x < -ITGSHELL_CLAMP_VEL_X)
    {
        ip->physics.vel_air.x = -ITGSHELL_CLAMP_VEL_X;
    }
    if (ip->physics.vel_air.x < 0.0F)
    {
        ip->lr = -1;
    }
    else ip->lr = +1;

    ip->is_damage_all = FALSE;

    itMainRefreshAttackColl(item_gobj);
}

/* 0x80178FA8 */
void itGShellSpinAirSetStatus(GObj *item_gobj)
{
    itGShellSpinAirInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITGShellStatusDescs, nITGShellStatusSpinAir);
}

/* it/itcommon/itgshell.c:542-581 itGShellMakeItem 0x80178FDC, verbatim. */
GObj* itGShellMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITGShellItemDesc, pos, vel, flags);

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

        dobj->mobj->palette_id = 1.0F;

        ip = itGetStruct(item_gobj);

        ip->attack_coll.can_rehit_shield = TRUE;

        ip->item_vars.shell.health = 1;
        ip->item_vars.shell.is_damage = FALSE;

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);

        ip->lifetime = ITGSHELL_LIFETIME;
    }
    return item_gobj;
}
/* 0x801790F4 */
sb32 itGShellCommonProcShield(GObj *item_gobj)
{
    itMainVelSetRebound(item_gobj);

    return FALSE;
}

