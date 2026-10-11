/* itlgun.c -- it/itcommon/itlgun.c, eighteen of its nineteen functions
 * (see below for the one left out): the Ray Gun's `ITDesc`, its
 * five-state `ITStatusDesc` table (ground wait / air fall / fighter
 * hold / fighter throw / fighter drop), its ammo weapon's own `WPDesc`,
 * and their proc bodies. Function-for-function against the game's own
 * ITStruct (it/ittypes.h) and WPStruct (wp/wptypes.h); every function
 * names its decomp line range.
 *
 * The roster's ninth entry, and Fire Flower's closest twin: a
 * companion weapon spawned through `wpManagerMakeWeapon`
 * itself rather than a hand-written per-item maker, same five-state
 * fall/hold/throw shape, same `WEAPON_FLAG_COLLPROJECT | WEAPON_FLAG_
 * PARENT_FIGHTER` spawn. Checked against Container-item
 * finding before being written: no call anywhere in this file reaches
 * `itMainMakeContainerItem`. Two real differences from Fire Flower's
 * own shape, both found reading fresh, not assumed: the ammo weapon's
 * `WPDesc` wires ALL EIGHT procs (Fire Flower's flame left `proc_hop`
 * NULL; the Ray Gun's ammo bounces off a shield via `syVectorRotate-
 * About3D`, the same `ProcHop` shape `wp/`'s own fighter-special
 * weapons use, and its `proc_absorb` is wired too, to the same function
 * `itLGunMakeItem` is the decomp's line for line. Its tail calls
 * `ifCommonItemArrowMakeInterface(ip)`, which reads the `IFCommonItem`
 * file: an ordinary sprite bank, `romdisk/ifcommonitem.spr`, because it
 * is one sprite and nothing else (src/dc/ifcommon.c).
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

/* it/itcommon/itlgun.c: llITCommonDataLGunItemAttributes -- reloc
 * stand-in, the same kind every prior roster item's own `ITDesc`
 * counterpart needs (see itstar.c's file header for the
 * full reasoning). Only ever read as an offset from
 * gITManagerCommonData, which itManagerInitItems fills in. */

/* it/itcommon/itlgun.c: llITCommonDataLGunAmmoWeaponAttributes -- the
 * ammo weapon's own reloc-offset stand-in, same shape, reading the SAME
 * gITManagerCommonData file the item's own ITDesc does (see the file
 * header, and itfflower.c's own flame weapon). */

/* it/itcommon/itlgun.c:12-34 dITLGunItemDesc, verbatim. */
ITDesc dITLGunItemDesc =
{
    nITKindLGun,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataLGunItemAttributes,

    {
        nGCMatrixKindTraRotRpyR,
        nGCMatrixKindNull,
        0
    },

    nGMAttackStateOff,
    itLGunFallProcUpdate,
    itLGunFallProcMap,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/* it/itcommon/itlgun.c:36-97 dITLGunStatusDescs, verbatim -- the same
 * five-state shape Fire Flower's own table has: Thrown and
 * Dropped both wire itLGunFallProcUpdate directly for proc_update, no
 * separate ThrownProcUpdate. */
ITStatusDesc dITLGunStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    {
        NULL,
        itLGunWaitProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 1 (Air Wait Fall) */
    {
        itLGunFallProcUpdate,
        itLGunFallProcMap,
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
        itLGunFallProcUpdate,
        itLGunThrownProcMap,
        itLGunCommonProcHit,
        itLGunCommonProcHit,
        itMainCommonProcHop,
        itLGunCommonProcHit,
        itMainCommonProcReflector,
        NULL
    },

    /* Status 4 (Fighter Drop) */
    {
        itLGunFallProcUpdate,
        itLGunDroppedProcMap,
        itLGunCommonProcHit,
        itLGunCommonProcHit,
        itMainCommonProcHop,
        itLGunCommonProcHit,
        itMainCommonProcReflector,
        NULL
    }
};

/* it/itcommon/itlgun.c:99-121 dITLGunAmmoWeaponDesc, verbatim -- unlike
 * Fire Flower's own flame WPDesc, the ammo
 * wires every proc including proc_hop (a real shield bounce) and
 * proc_absorb (the same function as proc_hit). */
WPDesc dITLGunAmmoWeaponDesc =
{
    0x00,
    nWPKindLGunAmmo,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataLGunAmmoWeaponAttributes,

    {
        nGCMatrixKindTraRotRpyRSca,
        nGCMatrixKindNull,
        0
    },

    itLGunWeaponAmmoProcUpdate,
    itLGunWeaponAmmoProcMap,
    itLGunWeaponAmmoProcHit,
    itLGunWeaponAmmoProcHit,
    itLGunWeaponAmmoProcHop,
    itLGunWeaponAmmoProcHit,
    itLGunWeaponAmmoProcReflector,
    itLGunWeaponAmmoProcHit
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itLGunStatus
{
    nITLGunStatusWait,
    nITLGunStatusFall,
    nITLGunStatusHold,
    nITLGunStatusThrown,
    nITLGunStatusDropped,
    nITLGunStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/itlgun.c:145-154 itLGunFallProcUpdate 0x801754F0, verbatim. */
sb32 itLGunFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITLGUN_GRAVITY, ITLGUN_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itlgun.c:156-162 itLGunWaitProcMap 0x80175528, verbatim. */
sb32 itLGunWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itLGunFallSetStatus);

    return FALSE;
}

/* it/itcommon/itlgun.c:164-168 itLGunFallProcMap 0x80175550, verbatim. */
sb32 itLGunFallProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITLGUN_MAP_REBOUND_COMMON, ITLGUN_MAP_REBOUND_GROUND, itLGunWaitSetStatus);
}

/* it/itcommon/itlgun.c:170-175 itLGunWaitSetStatus 0x80175584, verbatim. */
void itLGunWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITLGunStatusDescs, nITLGunStatusWait);
}

/* it/itcommon/itlgun.c:177-186 itLGunFallSetStatus 0x801755B8, verbatim. */
void itLGunFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITLGunStatusDescs, nITLGunStatusFall);
}

/* it/itcommon/itlgun.c:188-194 itLGunHoldSetStatus 0x801755FC, verbatim. */
void itLGunHoldSetStatus(GObj *item_gobj)
{
    DObjGetStruct(item_gobj)->rotate.vec.f.y = F_CST_DTOR32(0.0F);

    itMainSetStatus(item_gobj, dITLGunStatusDescs, nITLGunStatusHold);
}

/* it/itcommon/itlgun.c:196-206 itLGunThrownProcMap 0x80175630, verbatim. */
sb32 itLGunThrownProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        return itMapCheckDestroyLanding(item_gobj, ITLGUN_MAP_REBOUND_COMMON);
    }
    else return itMapCheckDestroyDropped(item_gobj, ITLGUN_MAP_REBOUND_COMMON, ITLGUN_MAP_REBOUND_GROUND, itLGunWaitSetStatus);
}

/* it/itcommon/itlgun.c:208-218 itLGunCommonProcHit 0x80175684, verbatim. */
sb32 itLGunCommonProcHit(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->attack_coll.attack_state = nGMAttackStateOff;

    itMainVelSetRebound(item_gobj);

    return FALSE;
}

/* it/itcommon/itlgun.c:220-228 itLGunThrownSetStatus 0x801756AC,
 * verbatim. Dead code today (no ported caller reaches Thrown, the same
 * status every prior roster item's own Thrown/Dropped setters carry) --
 * dereferences DObjGetStruct(item_gobj)->child unconditionally, the
 * blade/frame DObj a real model pack would attach, same shape Sword/
 * Bat/Hammer/Harisen's own pairs have. */
void itLGunThrownSetStatus(GObj *item_gobj)
{
    s32 lr = ftGetStruct(itGetStruct(item_gobj)->owner_gobj)->lr;

    itMainSetStatus(item_gobj, dITLGunStatusDescs, nITLGunStatusThrown);

    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = (lr == -1) ? F_CST_DTOR32(-90.0F) : F_CST_DTOR32(90.0F);
}

/* it/itcommon/itlgun.c:230-240 itLGunDroppedProcMap 0x8017572C, verbatim. */
sb32 itLGunDroppedProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        return itMapCheckDestroyLanding(item_gobj, ITLGUN_MAP_REBOUND_COMMON);
    }
    else return itMapCheckDestroyDropped(item_gobj, ITLGUN_MAP_REBOUND_COMMON, ITLGUN_MAP_REBOUND_GROUND, itLGunWaitSetStatus);
}

/* it/itcommon/itlgun.c:242-250 itLGunDroppedSetStatus 0x80175780,
 * verbatim. Same dead-code/child-dereference status as ThrownSetStatus
 * above. */
void itLGunDroppedSetStatus(GObj *item_gobj)
{
    s32 lr = ftGetStruct(itGetStruct(item_gobj)->owner_gobj)->lr;

    itMainSetStatus(item_gobj, dITLGunStatusDescs, nITLGunStatusDropped);

    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = (lr == -1) ? F_CST_DTOR32(-90.0F) : F_CST_DTOR32(90.0F);
}

/* it/itcommon/itlgun.c:293-303 itLGunWeaponAmmoProcMap 0x80175914,
 * verbatim. */
sb32 itLGunWeaponAmmoProcMap(GObj *weapon_gobj)
{
    if (wpMapTestAllCheckCollEnd(weapon_gobj) != FALSE)
    {
        efManagerDustExpandSmallMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f, 1.0F);

        return TRUE;
    }
    else return FALSE;
}

/* it/itcommon/itlgun.c:305-313 itLGunWeaponAmmoProcHit 0x80175958,
 * verbatim. */
sb32 itLGunWeaponAmmoProcHit(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    efManagerImpactShockMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f, wp->attack_coll.damage);

    return TRUE;
}

/* it/itcommon/itlgun.c:315-326 itLGunWeaponAmmoProcHop 0x80175988,
 * verbatim -- the shield-bounce reflection Fire Flower's own flame
 * weapon never needed (its own WPDesc left proc_hop NULL). */
sb32 itLGunWeaponAmmoProcHop(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    syVectorRotateAbout3D(&wp->physics.vel_air, &wp->shield_collide_dir, wp->shield_collide_angle * 2);

    DObjGetStruct(weapon_gobj)->rotate.vec.f.z = syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x);
    DObjGetStruct(weapon_gobj)->scale.vec.f.x = 1.0F;

    return FALSE;
}

/* it/itcommon/itlgun.c:328-340 itLGunWeaponAmmoProcReflector 0x80175A00,
 * verbatim. */
sb32 itLGunWeaponAmmoProcReflector(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    FTStruct *fp = ftGetStruct(wp->owner_gobj);

    wpMainReflectorSetLR(wp, fp);

    DObjGetStruct(weapon_gobj)->rotate.vec.f.z = syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x);
    DObjGetStruct(weapon_gobj)->scale.vec.f.x = 1.0F;

    return FALSE;
}


/* it/itcommon/itlgun.c:253-270 itLGunMakeItem 0x80175800, verbatim. */
GObj* itLGunMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITLGunItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        ip->multi = ITLGUN_AMMO_MAX;

        DObjGetStruct(item_gobj)->rotate.vec.f.y = ((syUtilsRandUShort() % 2) != 0) ? F_CST_DTOR32(90.0F) : F_CST_DTOR32(-90.0F);

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}
/* it/itcommon/itlgun.c:272-291 itLGunWeaponAmmoProcUpdate, verbatim
 * including the decomp's own `#if !defined (DAIRANTOU_OPT0)` re-fetch
 * (the game is never built with that flag, so this branch always
 * compiles -- kept exactly as the decomp text has it, not simplified
 * away, per this port's byte-for-byte doctrine). */
sb32 itLGunWeaponAmmoProcUpdate(GObj *weapon_gobj)
{
    DObj *dobj = DObjGetStruct(weapon_gobj);

    if (dobj->scale.vec.f.x < ITLGUN_AMMO_CLAMP_SCALE_X)
    {
        dobj->scale.vec.f.x += ITLGUN_AMMO_STEP_SCALE_X;

#if !defined (DAIRANTOU_OPT0)
        dobj = DObjGetStruct(weapon_gobj); /* Y tho lol */
#endif

        if (dobj->scale.vec.f.x > ITLGUN_AMMO_CLAMP_SCALE_X)
        {
            dobj->scale.vec.f.x = ITLGUN_AMMO_CLAMP_SCALE_X;
        }
    }
    return FALSE;
}

/* it/itcommon/itlgun.c:342-359 itLGunWeaponAmmoMakeWeapon 0x80175A60,
 * verbatim -- the same WEAPON_FLAG_COLLPROJECT | WEAPON_FLAG_PARENT_
 * FIGHTER spawn itFFlowerWeaponFlameMakeWeapon uses, so the
 * same NULL-p_translate host-test rule applies to any test driving this
 * for real: the owner FTStruct's own coll_data.p_translate must be set
 * before this call, or wpManagerMakeWeapon's own tail (mpCommonRun-
 * WeaponCollisionDefault) dereferences NULL inside
 * mpCommonCopyCollDataStats. */
GObj* itLGunWeaponAmmoMakeWeapon(GObj *fighter_gobj, Vec3f *pos)
{
    GObj *weapon_gobj = wpManagerMakeWeapon(fighter_gobj, &dITLGunAmmoWeaponDesc, pos, (WEAPON_FLAG_COLLPROJECT | WEAPON_FLAG_PARENT_FIGHTER));
    WPStruct *wp;

    if (weapon_gobj == NULL)
    {
        return NULL;
    }
    wp = wpGetStruct(weapon_gobj);

    wp->physics.vel_air.x = wp->lr * ITLGUN_AMMO_VEL_X;

    DObjGetStruct(weapon_gobj)->rotate.vec.f.z = syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x);

    return weapon_gobj;
}

/* it/itcommon/itlgun.c:361-369 itLGunMakeAmmo 0x80175AD8, verbatim.
 * Dead code today -- its real caller is a fighter's own Item-Attack
 * special, not yet ported (see the file header). */
void itLGunMakeAmmo(GObj *fighter_gobj, Vec3f *pos)
{
    ITStruct *ip = itGetStruct(ftGetStruct(fighter_gobj)->item_gobj);

    itLGunWeaponAmmoMakeWeapon(fighter_gobj, pos);

    ip->multi--;
}

