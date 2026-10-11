/* itfflower.c -- it/itcommon/itfflower.c, verbatim except one function
 * left out (see below): the Fire Flower's `ITDesc`, its five-state
 * `ITStatusDesc` table (ground wait / air fall / fighter hold / fighter
 * throw / fighter drop), its flame weapon's own `WPDesc`, and their proc
 * bodies. Function-for-function against the game's own ITStruct
 * (it/ittypes.h) and WPStruct (wp/wptypes.h); every function names its
 * decomp line range.
 *
 * The roster's eighth entry, and the first with a companion WEAPON -- unlike
 * every prior "Usable item" (Star through Fan), the Fire Flower doesn't just
 * get thrown; a fighter holding it shoots a Flame weapon
 * (`itFFlowerWeaponFlameMakeWeapon`), which is `wp/`'s own machinery
 * (`WPDesc`, `wpManagerMakeWeapon`, `wpGetStruct`, `wpMainDecLifeCheckExpire`,
 * `wpMapTestAllCheckCollEnd`, `wpMainReflectorSetLR`), already fully in the
 * build. Genuinely different from Star through Fan's five-state
 * fall/hold/throw shape -- not another twin, and nothing here reaches
 * `itMainMakeContainerItem`/`itManagerMakeItemSetupCommon`.
 *
 * Two reloc-offset stand-ins beyond the usual per-item `ITDesc.o_attributes`
 * one (`llITCommonDataFFlowerItemAttributes`, the same shape every other
 * roster item's own has): `dITFFlowerWeaponFlameWeaponDesc.o_attributes`
 * needs its own (`llITCommonDataFFlowerFlameWeaponAttributes` -- the
 * flame's WPDesc reads the SAME `gITManagerCommonData` file the item's
 * own ITDesc does, not a separate per-character file the way a
 * fighter's own special-move weapon would, e.g. `wp/wpmario/wpmariofireball.c`'s
 * `gFTMarioFileSpecial1`), and `itFFlowerShootFlame`'s own angle table
 * needs a third (`llITCommonDataFFlowerFlameAngles`, the same shape
 * `itcapsule.c`'s abandoned attempt and `itharisen.c`'s own
 * `dITHarisenAnimJoint[]` reader both needed). All three honest dead
 * arithmetic today, same as every other offset stand-in, until
 * `gITManagerCommonData` is populated for real.
 *
 * `gITManagerParticleBankID` (`it/item.h:13`, `it/itmanager.c:109` in
 * the decomp) is a global
 * zero-initialised in `src/dc/itmanager.c` alongside
 * `gITManagerDisplayMode`, the same "real field, not yet given its real
 * starting value" shape; only ever read here as a bank id passed to
 * `lbParticleMakePosVel`, which itself already tolerates an
 * out-of-range/placeholder bank id the same way every other pre-init
 * global this port stands in for does.
 *
 * `itFFlowerShootFlame` is not called from anywhere else this port
 * currently builds (its real caller is a fighter's own Item-Attack
 * special, not yet ported) -- dead code today, kept per the established
 * doctrine, same status as every prior roster item's own dead
 * `SetStatus` functions.
 *
 * `itFFlowerMakeItem` is the decomp's line for
 * line. Its tail calls `ifCommonItemArrow-
 * MakeInterface(ip)`, which needs the `IFCommonItem` file
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

/* it/itcommon/itfflower.c: llITCommonDataFFlowerItemAttributes -- reloc
 * stand-in, the same kind every other roster item's own `ITDesc`
 * counterpart needs (see itstar.c's file header for the
 * full reasoning). Only ever read as an offset from
 * gITManagerCommonData, which is NULL until
 * itManagerInitItems runs. */

/* it/itcommon/itfflower.c: llITCommonDataFFlowerFlameWeaponAttributes --
 * the flame weapon's own reloc-offset stand-in, same shape, reading the
 * SAME gITManagerCommonData file the item's own ITDesc does (see the
 * file header). */

/* it/itcommon/itfflower.c: llITCommonDataFFlowerFlameAngles -- a THIRD,
 * independent reloc-offset stand-in for itFFlowerShootFlame's own angle
 * table, read through raw pointer arithmetic on dITFFlowerItemDesc.p_file,
 * the same shape itcapsule.c's own AttackEvents stand-in and
 * itharisen.c's own dITHarisenAnimJoint[] reader both needed. */

/* it/itcommon/itfflower.c:12-34 dITFFlowerItemDesc, verbatim. */
ITDesc dITFFlowerItemDesc =
{
    nITKindFFlower,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataFFlowerItemAttributes,

    {
        nGCMatrixKindTraRotRpyR,
        nGCMatrixKindNull,
        0
    },

    nGMAttackStateOff,
    itFFlowerFallProcUpdate,
    itFFlowerFallProcMap,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/* it/itcommon/itfflower.c:36-97 dITFFlowerStatusDescs, verbatim. Note
 * the Thrown/Dropped rows both reuse itFFlowerFallProcUpdate directly
 * for proc_update -- there is no separate itFFlowerThrownProcUpdate the
 * way Bat/Hammer/Harisen's own tables each had one. */
ITStatusDesc dITFFlowerStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    {
        NULL,
        itFFlowerWaitProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 1 (Air Wait Fall) */
    {
        itFFlowerFallProcUpdate,
        itFFlowerFallProcMap,
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
        itFFlowerFallProcUpdate,
        itFFlowerThrownProcMap,
        itFFlowerCommonProcHit,
        itFFlowerCommonProcHit,
        itMainCommonProcHop,
        itFFlowerCommonProcHit,
        itMainCommonProcReflector,
        NULL
    },

    /* Status 4 (Fighter Drop) */
    {
        itFFlowerFallProcUpdate,
        itFFlowerDroppedProcMap,
        itFFlowerCommonProcHit,
        itFFlowerCommonProcHit,
        itMainCommonProcHop,
        itFFlowerCommonProcHit,
        itMainCommonProcReflector,
        NULL
    }
};

/* it/itcommon/itfflower.c:99-127 dITFFlowerWeaponFlameWeaponDesc,
 * verbatim -- the flame weapon's own WPDesc, wp/'s shape (wp/wptypes.h),
 * the first one any it/itcommon file has needed. */
WPDesc dITFFlowerWeaponFlameWeaponDesc =
{
    0x00,
    nWPKindFFlowerFlame,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataFFlowerFlameWeaponAttributes,

    {
        nGCMatrixKindTraRotRpyRSca,
        nGCMatrixKindNull,
        0
    },

    itFFlowerWeaponFlameProcUpdate,
    itFFlowerWeaponFlameProcMap,
    itFFlowerWeaponFlameProcHit,
    itFFlowerWeaponFlameProcHit,
    NULL,
    itFFlowerWeaponFlameProcHit,
    itFFlowerWeaponFlameProcReflector,
    NULL
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itFFlowerStatus
{
    nITFFlowerStatusWait,
    nITFFlowerStatusFall,
    nITFFlowerStatusHold,
    nITFFlowerStatusThrown,
    nITFFlowerStatusDropped,
    nITFFlowerStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/itfflower.c:145-154 itFFlowerFallProcUpdate 0x80175B20,
 * verbatim. */
sb32 itFFlowerFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITFFLOWER_GRAVITY, ITFFLOWER_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itfflower.c:156-162 itFFlowerWaitProcMap 0x80175B5C,
 * verbatim. */
sb32 itFFlowerWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itFFlowerFallSetStatus);

    return FALSE;
}

/* it/itcommon/itfflower.c:164-168 itFFlowerFallProcMap 0x80175B84,
 * verbatim. */
sb32 itFFlowerFallProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITFFLOWER_MAP_REBOUND_COMMON, ITFFLOWER_MAP_REBOUND_GROUND, itFFlowerWaitSetStatus);
}

/* it/itcommon/itfflower.c:170-175 itFFlowerWaitSetStatus 0x80175BB0,
 * verbatim. */
void itFFlowerWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITFFlowerStatusDescs, nITFFlowerStatusWait);
}

/* it/itcommon/itfflower.c:177-186 itFFlowerFallSetStatus 0x80175BE4,
 * verbatim. */
void itFFlowerFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITFFlowerStatusDescs, nITFFlowerStatusFall);
}

/* it/itcommon/itfflower.c:188-192 itFFlowerHoldSetStatus 0x80175C28,
 * verbatim. */
void itFFlowerHoldSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITFFlowerStatusDescs, nITFFlowerStatusHold);
}

/* it/itcommon/itfflower.c:194-204 itFFlowerThrownProcMap 0x80175C50,
 * verbatim. First roster item whose Thrown map proc branches on
 * ip->multi -- the flame ammo counter doubles as a "still landing"
 * flag here. */
sb32 itFFlowerThrownProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        return itMapCheckDestroyLanding(item_gobj, ITFFLOWER_MAP_REBOUND_COMMON);
    }
    else return itMapCheckDestroyDropped(item_gobj, ITFFLOWER_MAP_REBOUND_COMMON, ITFFLOWER_MAP_REBOUND_GROUND, itFFlowerWaitSetStatus);
}

/* it/itcommon/itfflower.c:206-216 itFFlowerCommonProcHit 0x80175C9C,
 * verbatim. */
sb32 itFFlowerCommonProcHit(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->attack_coll.attack_state = nGMAttackStateOff;

    itMainVelSetRebound(item_gobj);

    return FALSE;
}

/* it/itcommon/itfflower.c:218-222 itFFlowerThrownSetStatus 0x80175CC4,
 * verbatim. */
void itFFlowerThrownSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITFFlowerStatusDescs, nITFFlowerStatusThrown);
}

/* it/itcommon/itfflower.c:224-234 itFFlowerDroppedProcMap 0x80175CEC,
 * verbatim. */
sb32 itFFlowerDroppedProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        return itMapCheckDestroyLanding(item_gobj, ITFFLOWER_MAP_REBOUND_COMMON);
    }
    else return itMapCheckDestroyDropped(item_gobj, ITFFLOWER_MAP_REBOUND_COMMON, ITFFLOWER_MAP_REBOUND_GROUND, itFFlowerWaitSetStatus);
}

/* it/itcommon/itfflower.c:236-240 itFFlowerDroppedSetStatus 0x80175D38,
 * verbatim. */
void itFFlowerDroppedSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITFFlowerStatusDescs, nITFFlowerStatusDropped);
}


/* it/itcommon/itfflower.c:243-258 itFFlowerMakeItem 0x80175D60, verbatim. */
GObj* itFFlowerMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITFFlowerItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        ip->multi = ITFFLOWER_AMMO_MAX;

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}
/* it/itcommon/itfflower.c:260-270 itFFlowerWeaponFlameProcUpdate
 * 0x80175DDC, verbatim. */
sb32 itFFlowerWeaponFlameProcUpdate(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    if (wpMainDecLifeCheckExpire(wp) != FALSE)
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itcommon/itfflower.c:272-282 itFFlowerWeaponFlameProcMap
 * 0x801750E8 (as printed in the decomp comment -- likely a transcribed
 * typo against its 0x80175xxx neighbours, kept exactly as the source
 * names it rather than invented-corrected), verbatim. */
sb32 itFFlowerWeaponFlameProcMap(GObj *weapon_gobj)
{
    if (wpMapTestAllCheckCollEnd(weapon_gobj) != FALSE)
    {
        efManagerDustExpandSmallMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f, 1.0F);

        return TRUE;
    }
    else return FALSE;
}

/* it/itcommon/itfflower.c:284-291 itFFlowerWeaponFlameProcHit
 * 0x80175E4C, verbatim. */
sb32 itFFlowerWeaponFlameProcHit(GObj *weapon_gobj)
{
    func_800269C0_275C0(nSYAudioFGMExplodeS);
    efManagerSparkleWhiteMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f);

    return FALSE;
}

/* it/itcommon/itfflower.c:293-310 itFFlowerWeaponFlameProcReflector
 * 0x80175E84, verbatim. */
sb32 itFFlowerWeaponFlameProcReflector(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    FTStruct *fp = ftGetStruct(wp->owner_gobj);
    Vec3f *translate;

    wp->lifetime = ITFFLOWER_AMMO_LIFETIME;

    wpMainReflectorSetLR(wp, fp);

    translate = &DObjGetStruct(weapon_gobj)->translate.vec.f;

    lbParticleMakePosVel(gITManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 2, translate->x, translate->y, 0.0F, wp->physics.vel_air.x, wp->physics.vel_air.y, 0.0F);
    lbParticleMakePosVel(gITManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0, translate->x, translate->y, 0.0F, wp->physics.vel_air.x, wp->physics.vel_air.y, 0.0F);

    return FALSE;
}

/* it/itcommon/itfflower.c:312-334 itFFlowerWeaponFlameMakeWeapon
 * 0x80175F48, verbatim. */
GObj* itFFlowerWeaponFlameMakeWeapon(GObj *fighter_gobj, Vec3f *pos, Vec3f *vel)
{
    GObj *weapon_gobj = wpManagerMakeWeapon(fighter_gobj, &dITFFlowerWeaponFlameWeaponDesc, pos, (WEAPON_FLAG_COLLPROJECT | WEAPON_FLAG_PARENT_FIGHTER));
    WPStruct *wp;

    if (weapon_gobj == NULL)
    {
        return NULL;
    }
    wp = wpGetStruct(weapon_gobj);

    wp->physics.vel_air.x = vel->x * wp->lr;
    wp->physics.vel_air.y = vel->y;
    wp->physics.vel_air.z = vel->z;

    wp->lifetime = ITFFLOWER_AMMO_LIFETIME;

    lbParticleMakePosVel(gITManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 2, pos->x, pos->y, 0.0F, wp->physics.vel_air.x, wp->physics.vel_air.y, 0.0F);
    lbParticleMakePosVel(gITManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0, pos->x, pos->y, 0.0F, wp->physics.vel_air.x, wp->physics.vel_air.y, 0.0F);

    return weapon_gobj;
}

/* it/itcommon/itfflower.c:336-350 itFFlowerShootFlame 0x8017604C,
 * verbatim. Dead code today -- its real caller is a fighter's own
 * Item-Attack special, not yet ported (see the file header). */
void itFFlowerShootFlame(GObj *fighter_gobj, Vec3f *pos, s32 index, s32 ammo_sub)
{
    ITStruct *ip = itGetStruct(ftGetStruct(fighter_gobj)->item_gobj);
    Vec3f vel;
    f32 *angle = (f32*)((uintptr_t)*dITFFlowerItemDesc.p_file + (intptr_t)llITCommonDataFFlowerFlameAngles);

    vel.x = __cosf(angle[index]) * ITFFLOWER_AMMO_VEL;
    vel.y = __sinf(angle[index]) * ITFFLOWER_AMMO_VEL;
    vel.z = 0.0F;

    itFFlowerWeaponFlameMakeWeapon(fighter_gobj, pos, &vel);

    ip->multi -= ammo_sub;
}

