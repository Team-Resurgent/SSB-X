/* itiwark.c -- it/itmonster/itiwark.c, verbatim: Onix's `ITDesc`, its
 * two-row `ITStatusDesc` table (fly / attack) and their proc bodies, plus
 * the Rock weapon's `WPDesc` and its five procs. Function-for-function
 * against the game's own ITStruct and WPStruct (it/ittypes.h,
 * wp/wptypes.h); every function names its decomp line range.
 *
 * The eighth monster and the biggest single state machine of
 * the thirteen. Onix is the one that goes UP:
 *
 *  - `nITIwarkStatusFly` is ITIWARK_FLY_WAIT frames of holding still, then
 *    `itIwarkAttackInitVars` sends it up at ITIWARK_FLY_VEL_Y with a count
 *    of rocks drawn from ITIWARK_ROCK_SPAWN_COUNT_MIN + rand(RANDOM).
 *  - `nITIwarkStatusAttack` is the whole attack: on every frame ABOVE
 *    `map_bound_top - ITIWARK_FLY_STOP_Y` (and the US build CLAMPS the
 *    position to that line rather than only testing it -- see the
 *    REGION_US arm, kept), it pays out one rock per `rock_spawn_wait`,
 *    quakes the screen every ITIWARK_ROCK_RUMBLE_WAIT while
 *    `rumble_frame` is non-zero, and returns TRUE once every rock is
 *    gone. `rumble_frame` is the ROCK's own count, incremented by the
 *    weapon's proc_map on a floor bounce -- so the item's screen shake is
 *    driven by its projectiles, which is the one cross-actor wire in the
 *    thirteen.
 *  - The model turns on a `multi` that is COUNTED UP, not down:
 *    `if (multi == MODEL_ROTATE_WAIT) { rotate += 180; multi = 0; }
 *    multi++;` -- the decomp's own idiom, and different from Meowth's.
 *
 * The Rock weapon is flags 0x01 (a tree with DL links) and its own three
 * things:
 *  - `itIwarkWeaponRockMakeWeapon` picks one of THREE starting Y-velocities
 *    from its `random` argument and writes that same number into
 *    `dobj->child->mobj->texture_id_curr` -- the rock's three models are
 *    three frames of one MObj, and `random` indexes both.
 *  - `itIwarkWeaponRockProcMap` bounces it: a NEW floor line reflects the
 *    velocity, scales it by WPIWARK_ROCK_COLLIDE_MUL_VEL_Y, plays the
 *    sound, emits dust, flips `lr` and bumps the OWNER ITEM's
 *    `rumble_frame`.
 *  - `itIwarkWeaponRockProcDead` is the only `proc_dead` in the thirteen:
 *    it increments the owner's `rock_spawn_count`, which is what lets the
 *    attack state end. `wp->weapon_vars.rock.owner_gobj` is stored for it,
 *    because `wp->owner_gobj` is not always the item.
 *
 * DIVERGES: the two descriptors' offset fields are numbers rather than the
 * decomp's `&llITCommonData...` symbols -- see src/dc/itemoffsets.h -- and
 * the `itGetPData`/`itGetMonsterAnimNode` calls drop their `&`. No
 * function body diverges; the two `#if !defined (DAIRANTOU_OPT0)` blocks
 * are kept because that switch is NOT defined, which is the arm the game
 * ships.
 */
#include <it/item.h>
#include <wp/weapon.h>
#include <ft/fighter.h>
#include <ef/efmanager.h>       /* efManagerQuakeMakeEffect,
                                 * efManagerDustHeavyDoubleMakeEffect,
                                 * efManagerDustLightMakeEffect */

#include "itmonster.h"          /* itGetMonsterAnimNode */
#include "itemoffsets.h"        /* llITCommonData* offsets */
#include "itemmodel.h"          /* itemModelSetDisplayList */

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

/* it/itmonster/itiwark.c:11-34 dITIwarkItemDesc, verbatim. */
ITDesc dITIwarkItemDesc =
{
    nITKindIwark,                           /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataWarkItemAttributes,     /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindNull,                  /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itIwarkCommonProcUpdate,                /* Proc Update */
    itIwarkCommonProcMap,                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itmonster/itiwark.c:36-64 dITIwarkStatusDescs, verbatim. */
ITStatusDesc dITIwarkStatusDescs[/* */] =
{
    /* Status 0 (Neutral Fly) */
    {
        itIwarkFlyProcUpdate,               /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 1 (Neutral Attack) */
    {
        itIwarkAttackProcUpdate,            /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    }
};

/* it/itmonster/itiwark.c:66-90 dITIwarkWeaponRockWeaponDesc, verbatim.
 * Note the asymmetry against its siblings: it has a proc_map and a
 * proc_hop, and no proc_hit at all -- a thrown rock is not destroyed by
 * what it lands on. */
WPDesc dITIwarkWeaponRockWeaponDesc =
{
    0x01,                                   /* Render flags? */
    nWPKindIwarkRock,                       /* Weapon Kind */
    &gITManagerCommonData,                  /* Pointer to weapon's loaded files? */
    (intptr_t)llITCommonDataWarkRockWeaponAttributes,   /* Offset of weapon attributes in loaded files */

    /* DObj transformation struct */
    {
        nGCMatrixKindNull,                  /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    itIwarkWeaponRockProcUpdate,            /* Proc Update */
    itIwarkWeaponRockProcMap,               /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    itIwarkWeaponRockProcHop,               /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    itIwarkWeaponRockProcReflector,         /* Proc Reflector */
    NULL                                    /* Proc Absorb */
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itIwarkStatus
{
    nITIwarkStatusFly,
    nITIwarkStatusAttack,
    nITIwarkStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itiwark.c:116-146 itIwarkAttackUpdateRock 0x8017D740,
 * verbatim. The `random` handed to the maker is BOTH the velocity pick and
 * the model frame, and the rock's `unk_0xC` is its index in the volley --
 * the decomp's own comment-free name, kept, and written twice: once when
 * the rock spawns and once, as -1, when it was the last one. */
void itIwarkAttackUpdateRock(GObj *iwark_gobj)
{
    ITStruct *ip = itGetStruct(iwark_gobj);
    DObj *dobj = DObjGetStruct(iwark_gobj);

    if (ip->item_vars.iwark.rock_spawn_wait <= 0)
    {
        WPStruct *wp;
        GObj *rock_gobj;
        Vec3f pos = dobj->translate.vec.f;

        pos.x += (ITIWARK_ROCK_SPAWN_OFF_X_MUL * syUtilsRandFloat()) + ITIWARK_ROCK_SPAWN_OFF_X_ADD;

        rock_gobj = itIwarkWeaponRockMakeWeapon(iwark_gobj, &pos, syUtilsRandIntRange(WPIWARK_ROCK_RANDOM_VEL_MAX));

        if (rock_gobj != NULL)
        {
            wp = wpGetStruct(rock_gobj);

        #if !defined (DAIRANTOU_OPT0)
            wp->weapon_vars.rock.unk_0xC = ip->item_vars.iwark.rock_spawn_max - ip->item_vars.iwark.rock_spawn_remain;
        #endif

            ip->item_vars.iwark.rock_spawn_remain--;

        #if !defined (DAIRANTOU_OPT0)
            if (ip->item_vars.iwark.rock_spawn_remain == 0)
            {
                wp->weapon_vars.rock.unk_0xC = -1;
            }
        #endif
            ip->item_vars.iwark.rock_spawn_wait = syUtilsRandIntRange(ITIWARK_ROCK_SPAWN_WAIT_MAX) + ITIWARK_ROCK_SPAWN_WAIT_MIN;
        }
    }
}

/* it/itmonster/itiwark.c:148-186 itIwarkAttackProcUpdate 0x8017D820,
 * verbatim, both REGION_US arms. The US arm CLAMPS the position to the
 * stop line and the other only tests it, which is the decomp's own
 * difference and is kept. The end condition is `rock_spawn_count ==
 * rock_spawn_max` -- both of them, because a volley of zero rocks would
 * otherwise never finish. */
sb32 itIwarkAttackProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
#if defined(REGION_US)
    f32 pos_y = gMPCollisionGroundData->map_bound_top - ITIWARK_FLY_STOP_Y;

    if (dobj->translate.vec.f.y >= pos_y)
    {
        dobj->translate.vec.f.y = pos_y;
#else
    if (dobj->translate.vec.f.y >= gMPCollisionGroundData->map_bound_top - ITIWARK_FLY_STOP_Y)
    {
#endif

        ip->physics.vel_air.y = 0.0F;

        if (ip->item_vars.iwark.rock_spawn_remain != 0)
        {
            itIwarkAttackUpdateRock(item_gobj);
        }
        else if (ip->item_vars.iwark.rock_spawn_count == ip->item_vars.iwark.rock_spawn_max)
        {
            return TRUE;
        }
        if ((ip->item_vars.iwark.rumble_wait == 0) && (ip->item_vars.iwark.rumble_frame != 0))
        {
            efManagerQuakeMakeEffect(0);

            ip->item_vars.iwark.rumble_wait = ITIWARK_ROCK_RUMBLE_WAIT;
        }
        if (ip->item_vars.iwark.rumble_frame != 0)
        {
            ip->item_vars.iwark.rumble_wait--;
        }
        ip->item_vars.iwark.rock_spawn_wait--;
    }
    if (ip->multi == ITIWARK_MODEL_ROTATE_WAIT)
    {
        dobj->rotate.vec.f.y += F_CST_DTOR32(180.0F);

        ip->multi = 0;
    }
    ip->multi++;

    return FALSE;
}

/* it/itmonster/itiwark.c:188-232 itIwarkAttackInitVars 0x8017D948,
 * verbatim. The DObj write is a display list -- which the port does not
 * draw from (gcSubmitDObj submits `dobj->dv`, the baked model) -- but the
 * read itself is real: it is the same `itGetPData` arithmetic every other
 * monster's anim uses, so the offset is kept honest by being exercised.
 * The dust's `lr` argument is -1, not Onix's own. */
void itIwarkAttackInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    Gfx *dl;
    Vec3f pos;

#if defined(REGION_US)
    ip->ga = nMPKineticsAir;
#endif

    ip->physics.vel_air.y = ITIWARK_FLY_VEL_Y;

    ip->item_vars.iwark.rock_spawn_remain = syUtilsRandIntRange(ITIWARK_ROCK_SPAWN_COUNT_RANDOM) + ITIWARK_ROCK_SPAWN_COUNT_MIN;
    ip->item_vars.iwark.rock_spawn_max = ip->item_vars.iwark.rock_spawn_remain;
    ip->item_vars.iwark.rock_spawn_count = 0;
    ip->item_vars.iwark.rock_spawn_wait = 0;
    ip->item_vars.iwark.rumble_frame = 0;
    ip->item_vars.iwark.rumble_wait = 0;

    ip->multi = 0;

    pos = dobj->translate.vec.f;

    if (ip->kind == nITKindIwark)
    {
        dl = (Gfx*) itGetPData(ip, llITCommonDataWarkDataStart, llITCommonDataWarkDisplayList);
        itemModelSetDisplayList(dobj, dl);    /* DIVERGES: itemmodel.h */

        pos.y += ITIWARK_IWARK_ADD_POS_Y;
    }
    else pos.y += ITIWARK_OTHER_ADD_POS_Y;

    efManagerDustHeavyDoubleMakeEffect(&pos, -1, 1.0F);

    if (ip->kind == nITKindIwark)
    {
        func_800269C0_275C0(nSYAudioVoiceMBallIwarkAppear);
    }
}

/* it/itmonster/itiwark.c:234-238 itIwarkAttackSetStatus 0x8017DA60,
 * verbatim. */
void itIwarkAttackSetStatus(GObj *item_gobj)
{
    itIwarkAttackInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITIwarkStatusDescs, nITIwarkStatusAttack);
}

/* it/itmonster/itiwark.c:240-254 itIwarkFlyProcUpdate 0x8017DA94,
 * verbatim: the rise's own countdown into the attack. */
sb32 itIwarkFlyProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        itIwarkAttackSetStatus(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itiwark.c:256-266 itIwarkFlySetStatus 0x8017DAD8,
 * verbatim. */
void itIwarkFlySetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi = ITIWARK_FLY_WAIT;

    ip->physics.vel_air.x = ip->physics.vel_air.y = 0.0F;

    itMainSetStatus(item_gobj, dITIwarkStatusDescs, nITIwarkStatusFly);
}

/* it/itmonster/itiwark.c:268-279 itIwarkCommonProcUpdate 0x8017DB18,
 * verbatim: the rise out of the ball, then the fly wait. */
sb32 itIwarkCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        itIwarkFlySetStatus(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itiwark.c:281-293 itIwarkCommonProcMap 0x8017DB5C,
 * verbatim. The `itMapSetGround` call is Onix's own -- the others only
 * cancel the rise. */
sb32 itIwarkCommonProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR) != FALSE)
    {
        ip->physics.vel_air.y = 0.0F;

        itMapSetGround(ip);
    }
    return FALSE;
}

/* it/itmonster/itiwark.c:295-326 itIwarkMakeItem 0x8017DBA0, verbatim.
 * `itMainClearOwnerStats` first, like Goldeen's, and the same
 * `interact_mask = GMHITCOLLISION_FLAG_FIGHTER` Snorlax narrows to. */
GObj* itIwarkMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITIwarkItemDesc, pos, vel, flags);
    DObj *dobj;
    ITStruct *ip;

    if (item_gobj != NULL)
    {
        itMainClearOwnerStats(item_gobj);

        dobj = DObjGetStruct(item_gobj);

        gcAddXObjForDObjFixed(dobj, nGCMatrixKindTraRotRpyR, 0);
        gcAddXObjForDObjFixed(dobj, 0x48, 0);

        dobj->translate.vec.f = *pos;

        ip = itGetStruct(item_gobj);

        ip->multi = ITMONSTER_RISE_STOP_WAIT;

        ip->attack_coll.interact_mask = GMHITCOLLISION_FLAG_FIGHTER;

        ip->physics.vel_air.x = ip->physics.vel_air.z = 0.0F;
        ip->physics.vel_air.y = ITMONSTER_RISE_VEL_Y;

        dobj->translate.vec.f.y -= ip->attr->map_coll_bottom;

        gcAddDObjAnimJoint(dobj, itGetMonsterAnimNode(ip, llITCommonDataWarkDataStart), 0.0F);
    }
    return item_gobj;
}

/* ---- the Rock weapon ---------------------------------------------- */

/* it/itmonster/itiwark.c:328-338 itIwarkWeaponRockProcDead 0x8017DCAC,
 * verbatim -- the only proc_dead in the thirteen, and the wire that lets
 * the attack state end. It reads the OWNER ITEM's struct out of the
 * weapon's own `weapon_vars.rock.owner_gobj`, not out of `wp->owner_gobj`
 * (which wpManagerMakeWeapon sets to the spawning actor and which the
 * reflector path rewrites). */
sb32 itIwarkWeaponRockProcDead(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    ITStruct *ip = itGetStruct(wp->weapon_vars.rock.owner_gobj);

    ip->item_vars.iwark.rock_spawn_count++;

    return TRUE;
}

/* it/itmonster/itiwark.c:340-354 itIwarkWeaponRockProcUpdate 0x8017DCCC,
 * verbatim. */
sb32 itIwarkWeaponRockProcUpdate(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    DObj *dobj;

    wpMainApplyGravityClampTVel(wp, WPIWARK_ROCK_GRAVITY, WPIWARK_ROCK_TVEL);

    dobj = DObjGetStruct(weapon_gobj);

    dobj->rotate.vec.f.z += WPIWARK_ROCK_ROTATE_STEP;

    return FALSE;
}

/* it/itmonster/itiwark.c:356-390 itIwarkWeaponRockProcMap 0x8017DD18,
 * verbatim. `floor_line_id` is the rock's own memory of the last line it
 * bounced off, initialised to -1 by the maker, and the bounce only happens
 * when the line CHANGED -- which is what stops a rock from re-bouncing
 * every frame it rests on one. The owner item's `rumble_frame` is bumped
 * here, and the item's proc_update is what turns that into a screen quake:
 * **the only place in the thirteen where a weapon drives its owner.** */
sb32 itIwarkWeaponRockProcMap(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    ITStruct *ip = itGetStruct(wp->weapon_vars.rock.owner_gobj);
    MPCollData *coll_data = &wp->coll_data;
    Vec3f pos = DObjGetStruct(weapon_gobj)->translate.vec.f;
    s32 line_id = wp->weapon_vars.rock.floor_line_id;

    wpMapTestAllCheckCollEnd(weapon_gobj);

    if (coll_data->mask_curr & MAP_FLAG_FLOOR)
    {
        if (line_id != coll_data->floor_line_id)
        {
            lbCommonReflect2D(&wp->physics.vel_air, &coll_data->floor_angle);
            lbCommonScale2D(&wp->physics.vel_air, WPIWARK_ROCK_COLLIDE_MUL_VEL_Y);

            wp->weapon_vars.rock.floor_line_id = coll_data->floor_line_id;

            func_800269C0_275C0(nSYAudioFGMIwarkRockMake);

            pos.y += WPIWARK_ROCK_COLLIDE_ADD_VEL_Y;

            efManagerDustLightMakeEffect(&pos, wp->lr, 1.0F);

            wp->lr = -wp->lr;

            ip->item_vars.iwark.rumble_frame++;
        }
    }
    return FALSE;
}

/* it/itmonster/itiwark.c:392-410 itIwarkWeaponRockProcHop 0x8017DE10,
 * verbatim: the airborne twin of the map bounce, without the owner item. */
sb32 itIwarkWeaponRockProcHop(GObj *weapon_gobj)
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

/* it/itmonster/itiwark.c:412-427 itIwarkWeaponRockProcReflector
 * 0x8017DEB8, verbatim. */
sb32 itIwarkWeaponRockProcReflector(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    FTStruct *fp = ftGetStruct(wp->owner_gobj);

    wpMainReflectorSetLR(wp, fp);

    DObjGetStruct(weapon_gobj)->rotate.vec.f.z = syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x);
    DObjGetStruct(weapon_gobj)->scale.vec.f.x = 1.0F;

    wp->lr = -wp->lr;

    return FALSE;
}

/* it/itmonster/itiwark.c:429-473 itIwarkWeaponRockMakeWeapon 0x8017DF28,
 * verbatim. `random` arrives as 0, 1 or 2 and is used TWICE: it picks the
 * starting Y-velocity and it is written straight into
 * `dobj->child->mobj->texture_id_curr`, because the rock's three models
 * are three frames of one MObj and the index is the same number. The
 * `vel_y` local is the decomp's own (a write-only variable the compiler
 * wanted), kept as written. `is_hitlag_victim` and `proc_dead` are the
 * rock's own; `owner_gobj` is stored a second time in the weapon's vars
 * because the item, not `wp->owner_gobj`, is what proc_dead reads. */
GObj* itIwarkWeaponRockMakeWeapon(GObj *parent_gobj, Vec3f *pos, u8 random)
{
    u32 random32;
    GObj *weapon_gobj = wpManagerMakeWeapon(parent_gobj, &dITIwarkWeaponRockWeaponDesc, pos, WEAPON_FLAG_PARENT_ITEM);
    DObj *dobj;
    f32 vel_y;
    WPStruct *wp;

    if (weapon_gobj == NULL)
    {
        return NULL;
    }
    wp = wpGetStruct(weapon_gobj);

    wp->weapon_vars.rock.floor_line_id = -1;

    random32 = random;

    if (random32 == 0)
    {
        wp->physics.vel_air.y = WPIWARK_ROCK_VEL_Y_START_A;
    }
    else wp->physics.vel_air.y = vel_y = (random32 == 1) ? WPIWARK_ROCK_VEL_Y_START_B : WPIWARK_ROCK_VEL_Y_START_C;

    if (syUtilsRandIntRange(2) == 0)
    {
        wp->lr = -1;
    }
    else wp->lr = +1;

    dobj = DObjGetStruct(weapon_gobj);

    gcAddXObjForDObjFixed(dobj, nGCMatrixKindTraRotRpyR, 0);
    gcAddXObjForDObjFixed(dobj, 0x46, 0);

    dobj->translate.vec.f = *pos;

    dobj->child->mobj->texture_id_curr = random;

    wp->weapon_vars.rock.owner_gobj = parent_gobj;

    wp->is_hitlag_victim = TRUE;

    wp->proc_dead = itIwarkWeaponRockProcDead;

    return weapon_gobj;
}
