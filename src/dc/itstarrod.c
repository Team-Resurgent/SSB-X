/* itstarrod.c -- it/itcommon/itstarrod.c, nineteen of its twenty
 * functions (see below for the one left out): the Star Rod's `ITDesc`,
 * its five-state `ITStatusDesc` table (ground wait / air fall / fighter
 * hold / fighter throw / fighter drop), its ammo star's own `WPDesc`,
 * and their proc bodies, plus the two higher-level wrappers a fighter's
 * own attack calls to fire an ammo star. Function-for-function against
 * the game's own ITStruct (it/ittypes.h) and WPStruct (wp/wptypes.h);
 * every function names its decomp line range.
 *
 * The roster's tenth entry, and a third companion-weapon
 * item in the Fire Flower / Ray Gun family: same
 * five-state fall/hold/throw shape, same ammo weapon spawned through
 * wpManagerMakeWeapon itself with WEAPON_FLAG_COLLPROJECT |
 * WEAPON_FLAG_PARENT_FIGHTER, same MakeItem-tail IFCommonItem blocker.
 * Checked for the Container-item shape: no call anywhere in this file reaches
 * itMainMakeContainerItem.
 *
 * What is genuinely new here, read fresh rather than assumed from the
 * twin shape: (1) the item itself carries an ammo counter (`ip->multi`,
 * ITSTARROD_AMMO_MAX = 20) that ThrownProcMap/DroppedProcMap branch on
 * directly -- landing while ammo remains rebounds it as a live thrown
 * item, landing at 0 destroys it outright, the same two-branch shape
 * Ray Gun's own ThrownProcMap/DroppedProcMap already have (worth
 * naming because Fire Flower's own pair has no such branch at all). (2) the ammo
 * weapon's WPDesc takes an `is_smash` parameter at MakeWeapon time that
 * SWAPS its own o_attributes offset to a second, alternate value
 * (llITCommonDataStarRodSmashWeaponAttributes) before firing -- a third
 * such offset this file needs, on top of the usual per-item
 * one and the ammo weapon's own tilt-variant one, and the first time any
 * roster item's own weapon has needed two DIFFERENT attribute tables for
 * the same WPDesc depending on how it's fired. (3) itStarRodMakeStar
 * (the fighter-attack-facing entry point, itLGunMakeAmmo's twin) reads
 * `ip->multi` from ANOTHER GObj's own item slot
 * (ftGetStruct(fighter_gobj)->item_gobj) rather than a parameter passed
 * to it directly -- same shape itLGunMakeAmmo already has, not new, but
 * worth confirming again since it's exactly the kind of "looks similar,
 * verify identical" case.
 *
 * Three reloc offsets beyond the usual per-item `ITDesc.o_attributes`
 * one (`llITCommonDataStarRodItemAttributes`, 0x48c -- the same shape
 * every other roster item's own needs): the ammo
 * weapon's own tilt-fire attributes (`llITCommonDataStarRodWeaponAttributes`,
 * 0x4d4, the WPDesc's own initializer value) and its smash-fire
 * attributes (`llITCommonDataStarRodSmashWeaponAttributes`, 0x508,
 * swapped in by itStarRodWeaponStarMakeWeapon's own is_smash branch
 * before the spawn). All three are real numbers
 * (src/dc/itemoffsets.h); they are honest dead arithmetic until
 * `gITManagerCommonData` is populated for real.
 *
 * itStarRodMakeStar is not called from anywhere else this port currently
 * builds (its real caller is a fighter's own Item-Attack special, not
 * yet ported) -- dead code today, kept per the established doctrine,
 * same status as itLGunMakeAmmo/itFFlowerShootFlame.
 *
 * `itStarRodMakeItem` is ported now, and it is the decomp's line for
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
#include <wp/weapon.h>
#include <ft/fighter.h>

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

/* it/itcommon/itstarrod.c: llITCommonDataStarRodItemAttributes -- reloc
 * stand-in, the same kind every other roster item's own `ITDesc`
 * counterpart needs (see itstar.c's file header for the
 * full reasoning). Only ever read as an offset from
 * gITManagerCommonData, which is NULL until
 * itManagerInitItems runs. */

/* it/itcommon/itstarrod.c: llITCommonDataStarRodWeaponAttributes -- the
 * ammo weapon's own tilt-fire reloc-offset stand-in, same shape, reading
 * the SAME gITManagerCommonData file the item's own ITDesc does (see the
 * file header, and itlgun.c's own ammo weapon). This is the
 * WPDesc's own initializer value -- the non-smash fire path. */

/* it/itcommon/itstarrod.c: llITCommonDataStarRodSmashWeaponAttributes --
 * the ammo weapon's ALTERNATE, smash-fire reloc-offset stand-in, swapped
 * into dITStarRodWeaponStarWeaponDesc.o_attributes by
 * itStarRodWeaponStarMakeWeapon's own is_smash branch immediately before
 * the spawn (see the file header). Same honest dead-arithmetic status as
 * the other two until gITManagerCommonData is populated for real. */

/* it/itcommon/itstarrod.c:12-34 dITStarRodItemDesc, verbatim. */
ITDesc dITStarRodItemDesc =
{
    nITKindStarRod,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataStarRodItemAttributes,

    {
        nGCMatrixKindTraRotRpyR,
        nGCMatrixKindNull,
        0
    },

    nGMAttackStateOff,
    itStarRodFallProcUpdate,
    itStarRodFallProcMap,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/* it/itcommon/itstarrod.c:36-101 dITStarRodStatusDescs, verbatim -- the
 * same five-state shape Fire Flower/Ray Gun's own tables have:
 * Thrown and Dropped both wire itStarRodFallProcUpdate directly
 * for proc_update, no separate ThrownProcUpdate. */
ITStatusDesc dITStarRodStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    {
        NULL,
        itStarRodWaitProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 1 (Air Wait Fall) */
    {
        itStarRodFallProcUpdate,
        itStarRodFallProcMap,
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
        itStarRodThrownProcUpdate,
        itStarRodThrownProcMap,
        itStarRodThrownProcHit,
        itStarRodThrownProcHit,
        itMainCommonProcHop,
        itStarRodThrownProcHit,
        itMainCommonProcReflector,
        NULL
    },

    /* Status 4 (Fighter Drop) */
    {
        itStarRodFallProcUpdate,
        itStarRodDroppedProcMap,
        itStarRodThrownProcHit,
        itStarRodThrownProcHit,
        itMainCommonProcHop,
        itStarRodThrownProcHit,
        itMainCommonProcReflector,
        NULL
    }
};

/* it/itcommon/itstarrod.c:103-125 dITStarRodWeaponStarWeaponDesc,
 * verbatim -- like Ray Gun's own ammo WPDesc, every proc is
 * wired including proc_hop (a real shield bounce) and proc_absorb (the
 * same function as proc_hit). */
WPDesc dITStarRodWeaponStarWeaponDesc =
{
    0x00,
    nWPKindStarRodStar,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataStarRodWeaponAttributes,

    {
        nGCMatrixKindTraRotRpyRSca,
        nGCMatrixKindNull,
        0
    },

    itStarRodWeaponStarProcUpdate,
    itStarRodWeaponStarProcMap,
    itStarRodWeaponStarProcHit,
    itStarRodWeaponStarProcHit,
    itStarRodWeaponStarProcHop,
    itStarRodWeaponStarProcHit,
    itStarRodWeaponStarProcReflector,
    itStarRodWeaponStarProcHit
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itStarRodStatus
{
    nITStarRodStatusWait,
    nITStarRodStatusFall,
    nITStarRodStatusHold,
    nITStarRodStatusThrown,
    nITStarRodStatusDropped,
    nITStarRodStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/itstarrod.c:146-155 itStarRodFallProcUpdate 0x80177E80,
 * verbatim. */
sb32 itStarRodFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITSTARROD_GRAVITY, ITSTARROD_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itstarrod.c:157-163 itStarRodWaitProcMap 0x80177EBC,
 * verbatim. */
sb32 itStarRodWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itStarRodFallSetStatus);

    return FALSE;
}

/* it/itcommon/itstarrod.c:165-171 itStarRodFallProcMap 0x80177EE4,
 * verbatim. */
sb32 itStarRodFallProcMap(GObj *item_gobj)
{
    itMapCheckDestroyDropped(item_gobj, ITSTARROD_MAP_REBOUND_COMMON, ITSTARROD_MAP_REBOUND_GROUND, itStarRodWaitSetStatus);

    return FALSE;
}

/* it/itcommon/itstarrod.c:173-178 itStarRodWaitSetStatus 0x80177F18,
 * verbatim. */
void itStarRodWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITStarRodStatusDescs, nITStarRodStatusWait);
}

/* it/itcommon/itstarrod.c:180-189 itStarRodFallSetStatus 0x80177F4C,
 * verbatim. */
void itStarRodFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITStarRodStatusDescs, nITStarRodStatusFall);
}

/* it/itcommon/itstarrod.c:191-197 itStarRodHoldSetStatus 0x80177F90,
 * verbatim. */
void itStarRodHoldSetStatus(GObj *item_gobj)
{
    DObjGetStruct(item_gobj)->rotate.vec.f.y = F_CST_DTOR32(0.0F);

    itMainSetStatus(item_gobj, dITStarRodStatusDescs, nITStarRodStatusHold);
}

/* it/itcommon/itstarrod.c:199-208 itStarRodThrownProcUpdate 0x80177FC4,
 * verbatim. */
sb32 itStarRodThrownProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITSTARROD_GRAVITY, ITSTARROD_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itstarrod.c:210-214 itStarRodThrownProcMap 0x80178000,
 * verbatim. */
sb32 itStarRodThrownProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITSTARROD_MAP_REBOUND_COMMON, ITSTARROD_MAP_REBOUND_GROUND, itStarRodWaitSetStatus);
}

/* it/itcommon/itstarrod.c:216-226 itStarRodThrownProcHit 0x80178030,
 * verbatim. */
sb32 itStarRodThrownProcHit(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->attack_coll.attack_state = nGMAttackStateOff;

    itMainVelSetRebound(item_gobj);

    return FALSE;
}

/* it/itcommon/itstarrod.c:228-233 itStarRodThrownSetStatus 0x80178058,
 * verbatim. Dead code today (no ported caller reaches Thrown, the same
 * status every prior roster item's own Thrown/Dropped setters carry) --
 * dereferences DObjGetStruct(item_gobj)->child unconditionally, the
 * frame DObj a real model pack would attach, same shape Sword/Bat/
 * Hammer/Harisen's own pairs have. Unlike Ray Gun's own pair,
 * this one never reads the owner's lr -- always +90 degrees. */
void itStarRodThrownSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITStarRodStatusDescs, nITStarRodStatusThrown);
    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = F_CST_DTOR32(90.0F);
}

/* it/itcommon/itstarrod.c:235-245 itStarRodDroppedProcMap 0x8017809C,
 * verbatim -- the ammo-counter branch: with ammo left, rebound and
 * return to Wait when it lands; with none, destroy outright. */
sb32 itStarRodDroppedProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        return itMapCheckDestroyLanding(item_gobj, ITSTARROD_MAP_REBOUND_COMMON);
    }
    else return itMapCheckDestroyDropped(item_gobj, ITSTARROD_MAP_REBOUND_COMMON, ITSTARROD_MAP_REBOUND_GROUND, itStarRodWaitSetStatus);
}

/* it/itcommon/itstarrod.c:247-252 itStarRodDroppedSetStatus 0x801780F0,
 * verbatim. Same dead-code/child-dereference status as ThrownSetStatus
 * above. */
void itStarRodDroppedSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITStarRodStatusDescs, nITStarRodStatusDropped);
    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = F_CST_DTOR32(90.0F);
}


/* it/itcommon/itstarrod.c:254-269 itStarRodMakeItem 0x80178134, verbatim. */
GObj* itStarRodMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITStarRodItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        ip->multi = ITSTARROD_AMMO_MAX;

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}
/* it/itcommon/itstarrod.c:272-296 itStarRodWeaponStarProcUpdate
 * 0x801781B0, verbatim. */
sb32 itStarRodWeaponStarProcUpdate(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    Vec3f pos;
    DObj *dobj;

    if (wp->weapon_vars.star.lifetime == 0)
    {
        DObjGetStruct(weapon_gobj)->flags = DOBJ_FLAG_HIDDEN;

        efManagerSparkleWhiteScaleMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f, 1.0F);

        return TRUE;
    }

    wp->weapon_vars.star.lifetime--;

    dobj = DObjGetStruct(weapon_gobj);

    dobj->rotate.vec.f.z += (-0.2F * wp->lr);

    if (wp->weapon_vars.star.lifetime % 2)
    {
        pos.x = DObjGetStruct(weapon_gobj)->translate.vec.f.x;
        pos.y = syUtilsRandIntRange(250) + (DObjGetStruct(weapon_gobj)->translate.vec.f.y - 125.0F);
        pos.z = 0.0F;

        efManagerStarRodSparkMakeEffect(&pos, wp->lr * -1.0F);
    }
    return FALSE;
}

/* it/itcommon/itstarrod.c:305-320 itStarRodWeaponStarProcMap 0x801782D4,
 * verbatim. */
sb32 itStarRodWeaponStarProcMap(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    if (wpMapTestAllCheckCollEnd(weapon_gobj) != FALSE)
    {
        efManagerStarSplashMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f, wp->lr);

        func_800269C0_275C0(nSYAudioFGMStarMapCollide);

        return TRUE;
    }
    else return FALSE;
}

/* it/itcommon/itstarrod.c:322-330 itStarRodWeaponStarProcHit 0x8017832C,
 * verbatim. */
sb32 itStarRodWeaponStarProcHit(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    efManagerStarSplashMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f, wp->lr);

    return TRUE;
}

/* it/itcommon/itstarrod.c:332-349 itStarRodWeaponStarProcHop 0x8017835C,
 * verbatim -- the same shield-bounce reflection Ray Gun's own ammo
 * needs. */
sb32 itStarRodWeaponStarProcHop(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    syVectorRotateAbout3D(&wp->physics.vel_air, &wp->shield_collide_dir, wp->shield_collide_angle * 2);

    DObjGetStruct(weapon_gobj)->rotate.vec.f.z = syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x);
    DObjGetStruct(weapon_gobj)->scale.vec.f.x = 1.0F;

    if (wp->physics.vel_air.x > 0.0F)
    {
        wp->lr = +1;
    }
    else wp->lr = -1;

    return FALSE;
}

/* it/itcommon/itstarrod.c:351-364 itStarRodWeaponStarProcReflector
 * 0x80178404, verbatim. */
sb32 itStarRodWeaponStarProcReflector(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    FTStruct *fp = ftGetStruct(wp->owner_gobj);

    wpMainReflectorSetLR(wp, fp);

    DObjGetStruct(weapon_gobj)->rotate.vec.f.z = syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x);
    DObjGetStruct(weapon_gobj)->scale.vec.f.x = 1.0F;

    wp->lr = -wp->lr;

    return FALSE;
}

/* it/itcommon/itstarrod.c:366-393 itStarRodWeaponStarMakeWeapon
 * 0x80178474, verbatim -- the same WEAPON_FLAG_COLLPROJECT |
 * WEAPON_FLAG_PARENT_FIGHTER spawn itLGunWeaponAmmoMakeWeapon/
 * itFFlowerWeaponFlameMakeWeapon use, so the same NULL-p_translate
 * host-test rule applies to any test driving this for real: the owner
 * FTStruct's own coll_data.p_translate must be set before this call.
 * The is_smash branch swaps the WPDesc's own o_attributes reloc offset
 * before the spawn -- see the file header. */
GObj* itStarRodWeaponStarMakeWeapon(GObj *fighter_gobj, Vec3f *pos, ub8 is_smash)
{
    GObj *weapon_gobj;
    DObj *dobj;
    WPStruct *wp;

    if (is_smash == TRUE)
    {
        dITStarRodWeaponStarWeaponDesc.o_attributes = (intptr_t)llITCommonDataStarRodSmashWeaponAttributes;
    }
    weapon_gobj = wpManagerMakeWeapon(fighter_gobj, &dITStarRodWeaponStarWeaponDesc, pos, (WEAPON_FLAG_COLLPROJECT | WEAPON_FLAG_PARENT_FIGHTER));

    if (weapon_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(weapon_gobj);
    wp = wpGetStruct(weapon_gobj);

    wp->physics.vel_air.x = ((!(is_smash)) ? ITSTARROD_AMMO_TILTVEL_X : ITSTARROD_AMMO_SMASH_VEL_X) * wp->lr;

    wp->weapon_vars.star.lifetime = (!(is_smash)) ? ITSTARROD_AMMO_TILT_LIFETIME : ITSTARROD_AMMO_SMASH_LIFETIME;

    gcAddXObjForDObjFixed(dobj, 0x2E, 0);

    dobj->translate.vec.f = *pos;
    dobj->translate.vec.f.z = 0.0F;

    return weapon_gobj;
}

/* it/itcommon/itstarrod.c:395-401 itStarRodMakeStar 0x80178594, verbatim.
 * Dead code today -- its real caller is a fighter's own Item-Attack
 * special, not yet ported (see the file header). */
void itStarRodMakeStar(GObj *fighter_gobj, Vec3f *pos, ub8 is_smash)
{
    ITStruct *ip = itGetStruct(ftGetStruct(fighter_gobj)->item_gobj);

    itStarRodWeaponStarMakeWeapon(fighter_gobj, pos, is_smash);

    ip->multi--;
}

