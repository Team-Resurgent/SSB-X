/* itnyars.c -- it/itmonster/itnyars.c, verbatim: Meowth's `ITDesc`, its
 * one-row `ITStatusDesc` table and its five proc bodies, plus the Coin
 * weapon's `WPDesc` and that weapon's own five procs. Function-for-function
 * against the game's own ITStruct and WPStruct (it/ittypes.h,
 * wp/wptypes.h); every function names its decomp line range.
 *
 * The sixth monster and the FIRST that spawns a weapon. The
 * monster half is Mew's shape exactly (rise, then one status) with one
 * thing added:
 *
 *  - `nITNyarsStatusAttack` pays out ITNYARS_COIN_SPAWN_MAX coins on a
 *    schedule: `coin_spawn_wait` starts at `LIFETIME - SPAWN_WAIT/2` and,
 *    every time `multi` reaches it, one `itNyarsAttackMakeCoin` fires and
 *    the wait is pushed another ITNYARS_COIN_SPAWN_WAIT out. Each call
 *    makes ITNYARS_COIN_SPAWN_MAX separate weapons, fanned around the
 *    circle by `coin_number * ITNYARS_COIN_ANGLE_DIFF` plus the running
 *    `coin_rotate_step * ITNYARS_COIN_ANGLE_STEP` -- so the ring walks as
 *    the attack goes on.
 *  - The model itself turns 180 degrees every ITNYARS_MODEL_ROTATE_WAIT
 *    frames, which is the decomp's way of saying "spin".
 *
 * The weapon half is what makes this the first of the seven that needed
 * one. `dITNyarsWeaponCoinWeaponDesc` is flags 0x01 (WEAPON_FLAG_DOBJLINKS
 * -- see wp/wpdef.h), so `wpManagerMakeWeapon` takes its
 * gcAddDObjForGObj branch and the renderer is wpDisplayDObjDLLinks; the
 * port's own weapon model table (src/dc/wpmanager.c) carries the pack.
 * The coin's procs are its own, and `itNyarsWeaponCoinProcHop` and
 * `ProcReflector` are the two that reshape it: both rotate the velocity
 * about Z and re-derive the DObj's own rotation from the result, which is
 * what makes a bounced coin lie flat along its path.
 *
 * DIVERGES: `dITNyarsItemDesc`'s third field and `dITNyarsWeaponCoinWeaponDesc`'s
 * fourth are numbers rather than the decomp's `&llITCommonData...` symbols
 * -- see src/dc/itemoffsets.h -- and the `itGetMonsterAnimNode` call drops
 * its `&`. No function body diverges.
 */
#include <it/item.h>
#include <wp/weapon.h>
#include <ft/fighter.h>
#include <ef/efmanager.h>       /* efManagerDamageCoinMakeEffect */

#include "itmonster.h"          /* itGetMonsterAnimNode */
#include "itemoffsets.h"        /* llITCommonData* offsets */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

/* func_800269C0_275C0, the FGM call -- see src/dc/ftcommon.h. */
#include "ftcommon.h"

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itnyars.c:12-35 dITNyarsItemDesc, verbatim. */
ITDesc dITNyarsItemDesc =
{
    nITKindNyars,                           /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataNyarsItemAttributes,    /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyRSca,         /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itNyarsCommonProcUpdate,                /* Proc Update */
    itNyarsCommonProcMap,                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itmonster/itnyars.c:37-51 dITNyarsStatusDescs, verbatim: one row. */
ITStatusDesc dITNyarsStatusDescs[/* */] =
{
    /* Status 0 (Neutral Attack) */
    {
        itNyarsAttackProcUpdate,            /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    }
};

/* it/itmonster/itnyars.c:53-76 dITNyarsWeaponCoinWeaponDesc, verbatim.
 * `p_weapon` is the item manager's file, not a fighter's: the Coin is one
 * of the monster weapons, and its attributes are the `WeaponAttributes
 * NyarsCoin` block of ITCommonData at 0x8c8. Flags 0x01 means
 * WEAPON_FLAG_DOBJLINKS -- the renderer walks `dobj->dl_link` rather than
 * each DObj's own `dl` (wp/wpdef.h, and the Pikachu Thunder packs are the
 * port's other two). */
WPDesc dITNyarsWeaponCoinWeaponDesc =
{
    0x01,                                     /* Render flags? */
    nWPKindNyarsCoin,                         /* Weapon Kind */
    &gITManagerCommonData,                    /* Pointer to character's loaded files? */
    (intptr_t)llITCommonDataNyarsCoinWeaponAttributes,  /* Offset of weapon attributes in loaded files */

    /* DObj transformation struct */
    {
        nGCMatrixKindNull,                  /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    itNyarsWeaponCoinProcUpdate,            /* Proc Update */
    NULL,                                   /* Proc Map */
    itNyarsWeaponCoinProcHit,               /* Proc Hit */
    itNyarsWeaponCoinProcHit,               /* Proc Shield */
    itNyarsWeaponCoinProcHop,               /* Proc Hop */
    itNyarsWeaponCoinProcHit,               /* Proc Set-Off */
    itNyarsWeaponCoinProcReflector,         /* Proc Reflector */
    itNyarsWeaponCoinProcHit                /* Proc Absorb */
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itNyarsStatus
{
    nITNyarsStatusAttack,
    nITNyarsStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itnyars.c:96-126 itNyarsAttackProcUpdate 0x8017EEB0,
 * verbatim. The wait is recomputed from `multi` on every payout rather
 * than accumulated, so the schedule stays in step with the lifetime even
 * if a frame is missed. */
sb32 itNyarsAttackProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        return TRUE;
    }
    if (ip->multi == ip->item_vars.nyars.coin_spawn_wait)
    {
        itNyarsAttackMakeCoin(item_gobj, ip->item_vars.nyars.coin_rotate_step * ITNYARS_COIN_ANGLE_STEP);

        ip->item_vars.nyars.coin_rotate_step++;
        ip->item_vars.nyars.coin_spawn_wait = ip->multi - ITNYARS_COIN_SPAWN_WAIT;

        func_800269C0_275C0(nSYAudioFGMNyarsCoin);
    }
    if (ip->item_vars.nyars.model_rotate_wait == 0)
    {
        dobj->rotate.vec.f.y += F_CST_DTOR32(180.0F);

        ip->item_vars.nyars.model_rotate_wait = ITNYARS_MODEL_ROTATE_WAIT;
    }
    ip->item_vars.nyars.model_rotate_wait--;

    ip->multi--;

    return FALSE;
}

/* it/itmonster/itnyars.c:128-138 itNyarsAttackInitVars 0x8017EFA0,
 * verbatim. The first payout is at half the spawn wait, which is what the
 * `- SPAWN_WAIT/2` is. */
void itNyarsAttackInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi = ITNYARS_LIFETIME;

    ip->item_vars.nyars.coin_spawn_wait = ip->multi - (ITNYARS_COIN_SPAWN_WAIT / 2);
    ip->item_vars.nyars.coin_rotate_step = 0;
    ip->item_vars.nyars.model_rotate_wait = ITNYARS_MODEL_ROTATE_WAIT;
}

/* it/itmonster/itnyars.c:140-145 itNyarsAttackSetStatus 0x8017EFC4,
 * verbatim. */
void itNyarsAttackSetStatus(GObj *item_gobj)
{
    itNyarsAttackInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITNyarsStatusDescs, nITNyarsStatusAttack);
}

/* it/itmonster/itnyars.c:147-161 itNyarsCommonProcUpdate 0x8017EFF8,
 * verbatim: the rise, then the attack. */
sb32 itNyarsCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        ip->physics.vel_air.x = ip->physics.vel_air.y = 0.0F;

        itNyarsAttackSetStatus(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itnyars.c:163-173 itNyarsCommonProcMap 0x8017F04C,
 * verbatim. */
sb32 itNyarsCommonProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR) != FALSE)
    {
        ip->physics.vel_air.y = 0.0F;
    }
    return FALSE;
}

/* it/itmonster/itnyars.c:175-202 itNyarsMakeItem 0x8017F08C, verbatim.
 * Meowth's is the shortest MakeItem of the thirteen: one XObj, the rise,
 * and the anim. Note there is no `dobj->translate.vec.f = *pos` -- like
 * Mew it relies on itManagerMakeItem -- but unlike Mew it IS rounded down
 * by its box height. */
GObj* itNyarsMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITNyarsItemDesc, pos, vel, flags);
    DObj *dobj;
    ITStruct *ip;

    if (item_gobj != NULL)
    {
        dobj = DObjGetStruct(item_gobj);

        gcAddXObjForDObjFixed(dobj, 0x48, 0);

        dobj->translate.vec.f = *pos;

        ip = itGetStruct(item_gobj);

        ip->multi = ITMONSTER_RISE_STOP_WAIT;

        ip->physics.vel_air.x = ip->physics.vel_air.z = 0.0F;
        ip->physics.vel_air.y = ITMONSTER_RISE_VEL_Y;

        dobj->translate.vec.f.y -= ip->attr->map_coll_bottom;

        gcAddDObjAnimJoint(dobj, itGetMonsterAnimNode(ip, llITCommonDataNyarsAnimJoint), 0.0F);
    }
    return item_gobj;
}

/* ---- the Coin weapon ---------------------------------------------- */

/* it/itmonster/itnyars.c:204-216 itNyarsWeaponCoinProcUpdate 0x8017F17C,
 * verbatim: a plain lifetime, checked before the decrement. */
sb32 itNyarsWeaponCoinProcUpdate(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    if (wp->weapon_vars.coin.lifetime == 0)
    {
        return TRUE;
    }
    wp->weapon_vars.coin.lifetime--;

    return FALSE;
}

/* it/itmonster/itnyars.c:218-224 itNyarsWeaponCoinProcHit 0x8017F1A4,
 * verbatim: the coin's only effect is its own sparkle, and it dies on
 * contact. */
sb32 itNyarsWeaponCoinProcHit(GObj *weapon_gobj)
{
    efManagerDamageCoinMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f);

    return TRUE;
}

/* it/itmonster/itnyars.c:226-243 itNyarsWeaponCoinProcHop 0x8017F1CC,
 * verbatim. `syVectorRotateAbout3D` reflects the velocity about the
 * shield's collide direction by twice the angle, and the DObj's Z rotation
 * is then re-derived from the same velocity -- which is the coin lying
 * flat along its new path. The `scale.x = 1.0F` undoes the flip `lr` does
 * in the renderer. */
sb32 itNyarsWeaponCoinProcHop(GObj *weapon_gobj)
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

/* it/itmonster/itnyars.c:245-259 itNyarsWeaponCoinProcReflector
 * 0x8017F274, verbatim. */
sb32 itNyarsWeaponCoinProcReflector(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    FTStruct *fp = ftGetStruct(wp->owner_gobj);

    wpMainReflectorSetLR(wp, fp);

    DObjGetStruct(weapon_gobj)->rotate.vec.f.z = syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x);
    DObjGetStruct(weapon_gobj)->scale.vec.f.x = 1.0F;

    wp->lr = -wp->lr;

    return FALSE;
}

/* it/itmonster/itnyars.c:261-289 itNyarsWeaponCoinMakeWeapon 0x8017F2E4,
 * verbatim. `wpManagerMakeWeapon` already places the weapon's DObj at the
 * parent item's position; the explicit write below repeats it, which is
 * the decomp's own redundancy and is kept. The two XObjs are the DObj's
 * transform policy -- the first is the same nGCMatrixKindTraRotRpyRSca the
 * item tree uses. */
GObj* itNyarsWeaponCoinMakeWeapon(GObj *item_gobj, u8 coin_number, f32 rotate_angle)
{
    WPStruct *wp;
    GObj *weapon_gobj = wpManagerMakeWeapon(item_gobj, &dITNyarsWeaponCoinWeaponDesc, &DObjGetStruct(item_gobj)->translate.vec.f, WEAPON_FLAG_PARENT_ITEM);
    DObj *dobj;

    if (weapon_gobj == NULL)
    {
        return NULL;
    }
    wp = wpGetStruct(weapon_gobj);

    wp->weapon_vars.coin.lifetime = ITNYARS_COIN_LIFETIME;

    wp->physics.vel_air.y = wp->physics.vel_air.z = 0.0F;
    wp->physics.vel_air.x = ITNYARS_COIN_VEL_X;

    syVectorRotate3D(&wp->physics.vel_air, SYVECTOR_AXIS_Z, F_CLC_DTOR32((coin_number * ITNYARS_COIN_ANGLE_DIFF) + rotate_angle));

    dobj = DObjGetStruct(weapon_gobj);

    gcAddXObjForDObjFixed(dobj, nGCMatrixKindTraRotRpyRSca, 0);
    gcAddXObjForDObjFixed(dobj, 0x46, 0);

    dobj->translate.vec.f = DObjGetStruct(item_gobj)->translate.vec.f;

    return weapon_gobj;
}

/* it/itmonster/itnyars.c:291-300 itNyarsAttackMakeCoin 0x8017F408,
 * verbatim: the ring, one weapon per slot. */
void itNyarsAttackMakeCoin(GObj *item_gobj, f32 angle)
{
    s32 coin_count;

    for (coin_count = 0; coin_count < ITNYARS_COIN_SPAWN_MAX; coin_count++)
    {
        itNyarsWeaponCoinMakeWeapon(item_gobj, coin_count, angle);
    }
}
