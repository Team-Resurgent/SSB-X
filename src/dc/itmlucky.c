/* itmlucky.c -- it/itmonster/itmlucky.c, verbatim: Chansey's `ITDesc`, its
 * four-row `ITStatusDesc` table (fall / appear / egg spawn / disappear) and
 * their proc bodies. Function-for-function against the game's own ITStruct
 * (it/ittypes.h); every function names its decomp line range.
 *
 * The fifth monster, and the only one whose state machine
 * TOUCHES THE ITEM ROSTER. Chansey is the Poké Ball monster that lays Eggs:
 *
 *  - `nITMLuckyStatusMakeEgg` is `itMLuckyMakeEggProcUpdate`, and it is
 *    the one place in the thirteen that calls `itManagerMakeItemSetupCommon`
 *    -- the same keystone a Container's drop and the item switch itself go
 *    through. It spawns up to ITMLUCKY_EGG_SPAWN_COUNT (3)
 *    Eggs, one every ITMLUCKY_EGG_SPAWN_WAIT_CONST (30) frames, each with a
 *    random velocity in a ±8 ring plus its two offsets, and it CHECKS THE
 *    TOGGLE first: no Eggs if the Egg is switched off or items are set to
 *    none. Its `proc_damage` (`itMLuckyMakeEggProcDamage`) is the only
 *    monster proc in the set -- being hit ADDS 4 to the wait, so a Chansey
 *    under fire lays its Eggs more slowly.
 *  - `nITMLuckyStatusDisappear` is what runs when the three are spent:
 *    ITMLUCKY_LIFETIME (90) frames with the hurtbox turned off, then TRUE.
 *  - `nITMLuckyStatusFall` is reached from either map proc's left/right
 *    wall check and is the only status whose entry clears `is_allow_pickup`
 *    and calls `itMapSetAir`.
 *  - `itMLuckyMakeItem` is the first of the set to hang its anim on
 *    `dobj->child` and to give that child an XObj (0x2C, a scale/rotate
 *    pair) rather than the root: Chansey's egg-laying pose is the child
 *    joint's.
 *
 * The same procs serve Saffron City's own Chansey (`it/itground/itglucky.c`,
 * not ported), which is why every one of the four `ip->kind == nITKindMLucky`
 * tests is there.
 *
 * DIVERGES, the one every item file has: `dITMLuckyItemDesc`'s third field
 * is a number rather than the decomp's `&llITCommonData...` symbol, and the
 * `itGetPData`/`itGetMonsterAnimNode` calls drop their `&`. See
 * src/dc/itemoffsets.h. No function body diverges.
 */
#include <it/item.h>
#include <sc/scene.h>           /* gSCManagerBattleState, nSCBattleItemSwitchNone */
#include <ef/efmanager.h>       /* efManagerDustLightMakeEffect */

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

/* it/itmonster/itmlucky.c:11-34 dITMLuckyItemDesc, verbatim. */
ITDesc dITMLuckyItemDesc =
{
    nITKindMLucky,                          /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataMLuckyItemAttributes,   /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,            /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateOff,                      /* Hitbox Update State */
    itMLuckyCommonProcUpdate,               /* Proc Update */
    itMLuckyCommonProcMap,                  /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itmonster/itmlucky.c:36-91 dITMLuckyStatusDescs, verbatim. Note row 3
 * reuses row 2's proc_map outright and that row 2 is the only row in the
 * whole thirteen with a Proc Damage. */
ITStatusDesc dITMLuckyStatusDescs[/* */] =
{
    /* Status 0 (Air Fall) */
    {
        itMLuckyFallProcUpdate,             /* Proc Update */
        itMLuckyFallProcMap,                /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 1 (Neutral Appear) */
    {
        itMLuckyAppearProcUpdate,           /* Proc Update */
        itMLuckyAppearProcMap,              /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 2 (Neutral Egg Spawn) */
    {
        itMLuckyMakeEggProcUpdate,          /* Proc Update */
        itMLuckyMakeEggProcMap,             /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        itMLuckyMakeEggProcDamage           /* Proc Damage */
    },

    /* Status 3 (Neutral Disappear) */
    {
        itMLuckyDisappearProcUpdate,        /* Proc Update */
        itMLuckyMakeEggProcMap,             /* Proc Map */
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

enum itMLuckyStatus
{
    nITMLuckyStatusFall,
    nITMLuckyStatusAppear,
    nITMLuckyStatusMakeEgg,
    nITMLuckyStatusDisappear,
    nITMLuckyStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itmlucky.c:105-123 itMLuckyMakeEggInitVars 0x80180FC0,
 * verbatim. The anim hang is gated on Chansey's own kind -- Saffron's
 * Chansey has a different model -- and it is the CHILD joint's, the one
 * itMLuckyMakeItem gave an XObj. `gcPlayAnimAll` right after is what
 * applies it before the first Egg frame. */
void itMLuckyMakeEggInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if (ip->kind == nITKindMLucky)
    {
        gcAddDObjAnimJoint(dobj->child, itGetPData(ip, llITCommonDataLuckyDataStart, llITCommonDataLuckyAnimJoint), 0.0F);
        gcPlayAnimAll(item_gobj);
    }
    ip->damage_coll.hitstatus = nGMHitStatusNormal;

    ip->item_vars.mlucky.egg_spawn_wait = ITMLUCKY_EGG_SPAWN_WAIT_CONST;

    ip->multi = ITMLUCKY_EGG_SPAWN_COUNT;
}

/* it/itmonster/itmlucky.c:125-133 itMLuckyFallProcUpdate 0x80181048,
 * verbatim. */
sb32 itMLuckyFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITMLUCKY_GRAVITY, ITMLUCKY_TVEL);

    return FALSE;
}

/* it/itmonster/itmlucky.c:135-155 itMLuckyFallProcMap 0x80181074, verbatim.
 * The `multi != 0` split is where the fall chooses between laying eggs and
 * vanishing. */
sb32 itMLuckyFallProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMapTestAllCheckCollEnd(item_gobj);

    if (ip->coll_data.mask_curr & MAP_FLAG_FLOOR)
    {
        ip->physics.vel_air.y = 0.0F;

        if (ip->multi != 0)
        {
            itMLuckyMakeEggSetStatus(item_gobj);
        }
        else itMLuckyDisappearSetStatus(item_gobj);
    }
    return FALSE;
}

/* it/itmonster/itmlucky.c:157-166 itMLuckyFallSetStatus 0x801810E0,
 * verbatim. */
void itMLuckyFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITMLuckyStatusDescs, nITMLuckyStatusFall);
}

/* it/itmonster/itmlucky.c:168-176 itMLuckyAppearProcUpdate 0x80181124,
 * verbatim. */
sb32 itMLuckyAppearProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITMLUCKY_GRAVITY, ITMLUCKY_TVEL);

    return FALSE;
}

/* it/itmonster/itmlucky.c:178-194 itMLuckyAppearProcMap 0x80181150,
 * verbatim. Landing in the appear state sets the egg state AND initialises
 * it in the same frame -- that is where the 3 count comes from. */
sb32 itMLuckyAppearProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMapTestAllCheckCollEnd(item_gobj);

    if (ip->coll_data.mask_curr & MAP_FLAG_FLOOR)
    {
        ip->physics.vel_air.y = 0.0F;

        itMLuckyMakeEggSetStatus(item_gobj);

        itMLuckyMakeEggInitVars(item_gobj);
    }
    return FALSE;
}

/* it/itmonster/itmlucky.c:196-206 itMLuckyAppearSetStatus 0x801811AC,
 * verbatim. */
void itMLuckyAppearSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->kind == nITKindMLucky)
    {
        func_800269C0_275C0(nSYAudioVoiceMBallLuckyAppear);
    }
    itMainSetStatus(item_gobj, dITMLuckyStatusDescs, nITMLuckyStatusAppear);
}

/* it/itmonster/itmlucky.c:208-264 itMLuckyMakeEggProcUpdate 0x80181200,
 * verbatim. THE EGG SPAWN, and the only call in the thirteen into
 * itManagerMakeItemSetupCommon -- the item switch's own keystone. Two
 * things to keep: the toggle test is on the EGG's kind specifically (a
 * Chansey whose Eggs are switched off still burns one of its three), and
 * the wait/`multi` decrements happen on BOTH arms, so a switched-off Egg
 * costs Chansey an egg's worth of time rather than nothing. The dust is
 * emitted only when a real Egg came out. */
sb32 itMLuckyMakeEggProcUpdate(GObj *lucky_gobj)
{
    ITStruct *lucky_ip = itGetStruct(lucky_gobj), *egg_ip;
    DObj *dobj = DObjGetStruct(lucky_gobj);
    GObj *egg_gobj;
    s32 unused;
    Vec3f pos;
    Vec3f vel;

    if (lucky_ip->multi == 0)
    {
        itMLuckyDisappearSetStatus(lucky_gobj);

        return FALSE;
    }
    else
    {
        if (!lucky_ip->item_vars.mlucky.egg_spawn_wait)
        {
            if ((gSCManagerBattleState->item_toggles & ITEM_TOGGLE_MASK_KIND(nITKindEgg)) && (gSCManagerBattleState->item_appearance_rate != nSCBattleItemSwitchNone))
            {
                pos = dobj->translate.vec.f;

                vel.x = (syUtilsRandFloat() * ITMLUCKY_EGG_SPAWN_BASE_VEL) + ITMLUCKY_EGG_SPAWN_ADD_VEL_X;
                vel.y = (syUtilsRandFloat() * ITMLUCKY_EGG_SPAWN_BASE_VEL) + ITMLUCKY_EGG_SPAWN_ADD_VEL_Y;
                vel.z = 0.0F;

                egg_gobj = itManagerMakeItemSetupCommon(lucky_gobj, nITKindEgg, &pos, &vel, (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_ITEM));

                if (egg_gobj != NULL)
                {
                    egg_ip = itGetStruct(egg_gobj);

                    func_800269C0_275C0(nSYAudioFGMKirbySpecialLwStart);

                    lucky_ip->item_vars.mlucky.egg_spawn_wait = ITMLUCKY_EGG_SPAWN_WAIT_CONST;
                    lucky_ip->multi--;

                    efManagerDustLightMakeEffect(&pos, egg_ip->lr, 1.0F);
                }
            }
            else
            {
                lucky_ip->item_vars.mlucky.egg_spawn_wait = ITMLUCKY_EGG_SPAWN_WAIT_CONST;
                lucky_ip->multi--;
            }
        }
        if (lucky_ip->item_vars.mlucky.egg_spawn_wait > 0)
        {
            lucky_ip->item_vars.mlucky.egg_spawn_wait--;
        }
    }
    return FALSE;
}

/* it/itmonster/itmlucky.c:266-274 itMLuckyMakeEggProcMap 0x80181368,
 * verbatim. Shared with the disappear status. */
sb32 itMLuckyMakeEggProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itMLuckyFallSetStatus);

    return FALSE;
}

/* it/itmonster/itmlucky.c:276-285 itMLuckyMakeEggProcDamage 0x80181390,
 * verbatim -- the only monster proc_damage in the thirteen: being hit
 * DELAYS the next Egg. */
sb32 itMLuckyMakeEggProcDamage(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->item_vars.mlucky.egg_spawn_wait += ITMLUCKY_EGG_SPAWN_WAIT_ADD;

    return FALSE;
}

/* it/itmonster/itmlucky.c:287-291 itMLuckyMakeEggSetStatus 0x801813A8,
 * verbatim. */
void itMLuckyMakeEggSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITMLuckyStatusDescs, nITMLuckyStatusMakeEgg);
}

/* it/itmonster/itmlucky.c:293-305 itMLuckyDisappearProcUpdate 0x801813D0,
 * verbatim. */
sb32 itMLuckyDisappearProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->item_vars.mlucky.lifetime == 0)
    {
        return TRUE;
    }
    ip->item_vars.mlucky.lifetime--;

    return FALSE;
}

/* it/itmonster/itmlucky.c:307-317 itMLuckyDisappearSetStatus 0x801813F8,
 * verbatim: the hurtbox off, the lifetime on. */
void itMLuckyDisappearSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->item_vars.mlucky.lifetime = ITMLUCKY_LIFETIME;

    ip->damage_coll.hitstatus = nGMHitStatusNone;

    itMainSetStatus(item_gobj, dITMLuckyStatusDescs, nITMLuckyStatusDisappear);
}

/* it/itmonster/itmlucky.c:319-335 itMLuckyCommonProcUpdate 0x80181430,
 * verbatim. */
sb32 itMLuckyCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        ip->physics.vel_air.y = 0.0F;

        itMLuckyAppearSetStatus(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itmlucky.c:337-349 itMLuckyCommonProcMap 0x80181480,
 * verbatim. */
sb32 itMLuckyCommonProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR) != FALSE)
    {
        ip->physics.vel_air.y = 0.0F;
    }
    return FALSE;
}

/* it/itmonster/itmlucky.c:351-369 itMLuckyMakeItem 0x801814C0, verbatim.
 * Everything is on the CHILD: the XObj (0x2C) and the anim. Chansey's
 * own `map_coll_bottom` still comes off the root, the way it does for
 * every monster. */
GObj* itMLuckyMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITMLuckyItemDesc, pos, vel, flags);
    DObj *dobj;
    ITStruct *ip;

    if (item_gobj != NULL)
    {
        dobj = DObjGetStruct(item_gobj);

        gcAddXObjForDObjFixed(dobj->child, 0x2C, 0);

        dobj->translate.vec.f = *pos;

        ip = itGetStruct(item_gobj);

        ip->multi = ITMONSTER_RISE_STOP_WAIT;

        ip->physics.vel_air.x = ip->physics.vel_air.z = 0.0F;
        ip->physics.vel_air.y = ITMONSTER_RISE_VEL_Y;

        dobj->translate.vec.f.y -= ip->attr->map_coll_bottom;

        gcAddDObjAnimJoint(dobj->child, itGetMonsterAnimNode(ip, llITCommonDataLuckyDataStart), 0.0F);
    }
    return item_gobj;
}
