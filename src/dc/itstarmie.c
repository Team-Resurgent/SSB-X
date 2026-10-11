/* itstarmie.c -- it/itmonster/itstarmie.c, verbatim: Starmie's `ITDesc`,
 * its two-row `ITStatusDesc` table (follow / attack) and their proc
 * bodies, plus the Swift weapon's `WPDesc` and its five procs.
 * Function-for-function against the game's own ITStruct and WPStruct
 * (it/ittypes.h, wp/wptypes.h); every function names its decomp line
 * range.
 *
 * The tenth monster. Starmie is the one that HUNTS:
 *
 *  - `nITStarmieStatusNFollow` is a straight run at a point BESIDE the
 *    victim, not at the victim -- `itStarmieNFollowFindFollowPlayerLR`
 *    puts the target one collision-width plus ITSTARMIE_TARGET_POS_OFF_X
 *    to the near side of whichever fighter is closest, which is the same
 *    nearest-enemy walk OfF Hitmonlee's attack does (the same running
 *    minimum, the same uninitialised `victim_gobj`, the same US/JP split
 *    over whose team is compared). It reaches that point and only then
 *    turns to the attack.
 *  - It is the second monster to hang a MatAnimJoint on a DObj's **MObj**
 *    (`gcAddMObjMatAnimJoint(item_dobj->mobj, ...)`) rather than on the
 *    joint, which is what steps its own texture frame -- and that is a
 *    write through a pointer that must be real.
 *  - `nITStarmieStatusAttack` fires Swifts on a CONST + rand(RANDOM)
 *    schedule while accelerating away at `add_vel_x` and pushing itself
 *    back ITSTARMIE_PUSH_VEL_X on every shot -- so it retreats as it
 *    fires.
 *
 * The Swift weapon is flags 0x03 (a tree with DL links) and carries the
 * three procs every projectile in the set has, with `itStarmieWeaponSwiftProcHit`
 * the one thing of its own: an efManagerStarSplashMakeEffect of its own
 * `lr`, which is the star burst. Its `proc_update` opens with the decomp's
 * own `wp->physics.vel_air.x = wp->physics.vel_air.x; // Bruh`, which is
 * kept -- it is the decomp's text and removing it would be a rewrite.
 *
 * DIVERGES: the two descriptors' offset fields are numbers rather than the
 * decomp's `&llITCommonData...` symbols -- see src/dc/itemoffsets.h -- and
 * the `itGetPData`/`itGetMonsterAnimNode` calls drop their `&`. No
 * function body diverges.
 */
#include <it/item.h>
#include <wp/weapon.h>
#include <ft/fighter.h>
#include <ef/efmanager.h>       /* efManagerStarSplashMakeEffect,
                                 * efManagerSparkleWhiteScaleMakeEffect */

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

/* it/itmonster/itstarmie.c:11-34 dITStarmieItemDesc, verbatim. */
ITDesc dITStarmieItemDesc =
{
    nITKindStarmie,                         /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataStarmieItemAttributes,  /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,            /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itStarmieCommonProcUpdate,              /* Proc Update */
    itStarmieCommonProcMap,                 /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itmonster/itstarmie.c:36-64 dITStarmieStatusDescs, verbatim. */
ITStatusDesc dITStarmieStatusDescs[/* */] =
{
    /* Status 0 (Neutral Follow) */
    {
        itStarmieNFollowProcUpdate,         /* Proc Update */
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
        itStarmieAttackProcUpdate,          /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    }
};

/* it/itmonster/itstarmie.c:66-91 dITStarmieWeaponSwiftWeaponDesc,
 * verbatim. The only monster weapon whose proc_setoff and proc_absorb are
 * its own proc_hit rather than NULL. */
WPDesc dITStarmieWeaponSwiftWeaponDesc =
{
    0x03,                                   /* Render flags? */
    nWPKindStarmieSwift,                    /* Weapon Kind */
    &gITManagerCommonData,                  /* Pointer to character's loaded files? */
    (intptr_t)llITCommonDataStarmieSwiftWeaponAttributes,   /* Offset of weapon attributes in loaded files */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyRSca,         /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    itStarmieWeaponSwiftProcUpdate,         /* Proc Update */
    NULL,                                   /* Proc Map */
    itStarmieWeaponSwiftProcHit,            /* Proc Hit */
    itStarmieWeaponSwiftProcHit,            /* Proc Shield */
    itStarmieWeaponSwiftProcHop,            /* Proc Hop */
    itStarmieWeaponSwiftProcHit,            /* Proc Set-Off */
    itStarmieWeaponSwiftProcReflector,      /* Proc Reflector */
    itStarmieWeaponSwiftProcHit             /* Proc Absorb */
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itStarmieStatus
{
    nITStarmieStatusNFollow,
    nITStarmieStatusAttack,
    nITStarmieStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itstarmie.c:109-134 itStarmieAttackUpdateSwift 0x80181C20,
 * verbatim. Starmie's own kind spawns the Swift to its FRONT and above;
 * every other kind that can summon one (Clefairy) spawns it ahead only.
 * The push-back is set here, once per shot, not accumulated. */
void itStarmieAttackUpdateSwift(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if (ip->item_vars.starmie.swift_spawn_wait <= 0)
    {
        Vec3f pos = dobj->translate.vec.f;

        if (ip->kind == nITKindStarmie)
        {
            pos.x += ITSTARMIE_STARMIE_SWIFT_SPAWN_OFF_X * ip->lr;
            pos.y += ITSTARMIE_STARMIE_SWIFT_SPAWN_OFF_Y;
        }
        else pos.x += ITSTARMIE_OTHER_SWIFT_SPAWN_OFF_X * ip->lr;

        itStarmieAttackMakeSwift(item_gobj, &pos);

        func_800269C0_275C0(nSYAudioFGMMonsterShoot);

        ip->item_vars.starmie.swift_spawn_wait = (syUtilsRandIntRange(ITSTARMIE_SWIFT_SPAWN_WAIT_RANDOM) + ITSTARMIE_SWIFT_SPAWN_WAIT_CONST);

        ip->physics.vel_air.x = -ip->lr * ITSTARMIE_PUSH_VEL_X;
    }
}

/* it/itmonster/itstarmie.c:136-153 itStarmieAttackProcUpdate 0x80181D24,
 * verbatim. `multi == 0` is tested FIRST here -- unlike Meowth's and
 * Koffing's -- so the last shot is not fired on the frame it ends. */
sb32 itStarmieAttackProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        return TRUE;
    }
    itStarmieAttackUpdateSwift(item_gobj);

    ip->item_vars.starmie.swift_spawn_wait--;

    ip->physics.vel_air.x += ip->item_vars.starmie.add_vel_x;

    ip->multi--;

    return FALSE;
}

/* it/itmonster/itstarmie.c:155-170 itStarmieAttackInitVars 0x80181D8C,
 * verbatim. The turn is a 180-degree model rotation rather than a scale
 * flip, and only when the facing actually CHANGED. */
void itStarmieAttackInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    s32 lr_bak = ip->lr;

    ip->lr = (ip->item_vars.starmie.victim_pos.x < dobj->translate.vec.f.x) ? -1 : +1;

    if (ip->lr != lr_bak)
    {
        dobj->rotate.vec.f.y += F_CST_DTOR32(180.0F);
    }
    ip->multi = ITSTARMIE_LIFETIME;

    ip->item_vars.starmie.swift_spawn_wait = 0;
    ip->item_vars.starmie.add_vel_x = ip->lr * ITSTARMIE_ADD_VEL_X;
}

/* it/itmonster/itstarmie.c:172-176 itStarmieAttackSetStatus 0x80181E0C,
 * verbatim. */
void itStarmieAttackSetStatus(GObj *item_gobj)
{
    itStarmieAttackInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITStarmieStatusDescs, nITStarmieStatusAttack);
}

/* it/itmonster/itstarmie.c:178-198 itStarmieNFollowProcUpdate 0x80181E40,
 * verbatim: the arrival test, one arm per facing. */
sb32 itStarmieNFollowProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if ((ip->lr == +1) && (dobj->translate.vec.f.x >= ip->item_vars.starmie.target_pos.x))
    {
        ip->physics.vel_air.x = 0.0F;
        ip->physics.vel_air.y = 0.0F;

        itStarmieAttackSetStatus(item_gobj);
    }
    if ((ip->lr == -1) && (dobj->translate.vec.f.x <= ip->item_vars.starmie.target_pos.x))
    {
        ip->physics.vel_air.x = 0.0F;
        ip->physics.vel_air.y = 0.0F;

        itStarmieAttackSetStatus(item_gobj);
    }
    return FALSE;
}

/* it/itmonster/itstarmie.c:200-241 itStarmieNFollowFindFollowPlayerLR
 * 0x80181EF4, verbatim. The target is beside the victim, not on it: the
 * offset is the victim's own collision WIDTH plus TARGET_POS_OFF_X, taken
 * on whichever side Starmie is approaching from. `victim_pos` is stored
 * separately from `target_pos` because the ATTACK re-derives its facing
 * from the former. */
void itStarmieNFollowFindFollowPlayerLR(GObj *item_gobj, GObj *fighter_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    FTStruct *fp = ftGetStruct(fighter_gobj);
    DObj *item_dobj = DObjGetStruct(item_gobj);
    DObj *fighter_dobj = DObjGetStruct(fighter_gobj);
    Vec3f dist;
    Vec3f target_pos;
    Vec3f *victim_pos;

    target_pos = fighter_dobj->translate.vec.f;

    dist.x = fighter_dobj->translate.vec.f.x - item_dobj->translate.vec.f.x;

    target_pos.y += ITSTARMIE_TARGET_POS_OFF_Y - fp->coll_data.map_coll.bottom;

    target_pos.x -= (fp->coll_data.map_coll.width + ITSTARMIE_TARGET_POS_OFF_X) * ((dist.x < 0.0F) ? -1 : +1);

    victim_pos = &fighter_dobj->translate.vec.f;

    syVectorDiff3D(&dist, &target_pos, &item_dobj->translate.vec.f);

    ip->physics.vel_air.y = ip->physics.vel_air.z = 0.0F;
    ip->physics.vel_air.x = ITSTARMIE_FOLLOW_VEL_X;

    syVectorRotate3D(&ip->physics.vel_air, SYVECTOR_AXIS_Z, syUtilsArcTan2(dist.y, dist.x));

    ip->item_vars.starmie.target_pos = target_pos;

    ip->item_vars.starmie.victim_pos = *victim_pos;

    ip->lr = (dist.x < 0.0F) ? -1 : +1;

    if (ip->lr == +1)
    {
        item_dobj->rotate.vec.f.y = F_CST_DTOR32(180.0F);
    }
    if (ip->kind == nITKindStarmie)
    {
        gcAddMObjMatAnimJoint(item_dobj->mobj, itGetPData(ip, llITCommonDataStarmieDataStart, llITCommonDataStarmieMatAnimJoint), 0);

        gcPlayAnimAll(item_gobj);
    }
}

/* it/itmonster/itstarmie.c:243-315 itStarmieNFollowInitVars 0x801820CC,
 * verbatim, both REGION_US arms in the decomp's order. The same nearest-
 * enemy walk as Hitmonlee's, with the same broken-looking first iteration
 * (`players == 0` storing `dist_xy` before the counter is bumped) and the
 * same uninitialised `victim_gobj` -- kept as written, PORT not REMAKE. */
void itStarmieNFollowInitVars(GObj *item_gobj)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
#if defined(REGION_US)
    ITStruct *ip = itGetStruct(item_gobj);
    GObj *victim_gobj;
    s32 unused2[2];
    DObj *dobj = DObjGetStruct(item_gobj);
    f32 square_xy;
    f32 dist_x;
    f32 dist_xy;
    Vec3f dist;
#else
    /* TODO: regswap */
    s32 unused1;
    GObj *victim_gobj;
    s32 unused2[2];
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    FTStruct *owner_fp = ftGetStruct(ip->owner_gobj);
    f32 square_xy;
    f32 dist_xy;
    Vec3f dist;
    f32 dist_x;
#endif
    s32 players = 0;

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);

#if defined(REGION_US)
        if ((fighter_gobj != ip->owner_gobj) && (fp->team != ip->team))
#else
        if ((fighter_gobj != ip->owner_gobj) && (fp->team != owner_fp->team))
#endif
        {
            syVectorDiff3D(&dist, &DObjGetStruct(fighter_gobj)->translate.vec.f, &dobj->translate.vec.f);

            if (players == 0)
            {
                dist_xy = SQUARE(dist.x) + SQUARE(dist.y);
            }
            players++;

            square_xy = SQUARE(dist.x) + SQUARE(dist.y);

            if (square_xy <= dist_xy)
            {
                dist_xy = square_xy;

                victim_gobj = fighter_gobj;
            }
        }
        fighter_gobj = fighter_gobj->link_next;

        continue;
    }
    itStarmieNFollowFindFollowPlayerLR(item_gobj, victim_gobj);

    if (ip->kind == nITKindStarmie)
    {
        func_800269C0_275C0(nSYAudioVoiceMBallStarmieAppear);
    }
}

/* it/itmonster/itstarmie.c:317-321 itStarmieNFollowSetStatus 0x801821E8,
 * verbatim. */
void itStarmieNFollowSetStatus(GObj *item_gobj)
{
    itStarmieNFollowInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITStarmieStatusDescs, nITStarmieStatusNFollow);
}

/* it/itmonster/itstarmie.c:323-337 itStarmieCommonProcUpdate 0x8018221C,
 * verbatim. */
sb32 itStarmieCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        ip->physics.vel_air.x = ip->physics.vel_air.y = 0.0F;

        itStarmieNFollowSetStatus(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itstarmie.c:339-349 itStarmieCommonProcMap 0x80182270,
 * verbatim. */
sb32 itStarmieCommonProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR) != FALSE)
    {
        ip->physics.vel_air.y = 0.0F;
    }
    return FALSE;
}

/* it/itmonster/itstarmie.c:351-378 itStarmieMakeItem 0x801822B0, verbatim.
 * One XObj and a display-list head move to 18, the priority Hitmonlee and
 * Snorlax use too. */
GObj* itStarmieMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITStarmieItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        DObj *dobj = DObjGetStruct(item_gobj);
        ITStruct *ip = itGetStruct(item_gobj);

        ip->multi = ITMONSTER_RISE_STOP_WAIT;

        ip->physics.vel_air.x = ip->physics.vel_air.z = 0.0F;
        ip->physics.vel_air.y = ITMONSTER_RISE_VEL_Y;

        gcAddXObjForDObjFixed(dobj, 0x48, 0);

        dobj->translate.vec.f = *pos;

        dobj->translate.vec.f.y -= ip->attr->map_coll_bottom;

        gcAddDObjAnimJoint(dobj, itGetMonsterAnimNode(ip, llITCommonDataStarmieDataStart), 0.0F);

        gcMoveGObjDLHead(item_gobj, 18, item_gobj->dl_link_priority);
    }
    return item_gobj;
}

/* ---- the Swift weapon ---------------------------------------------- */

/* it/itmonster/itstarmie.c:380-392 itStarmieWeaponSwiftProcUpdate
 * 0x801823B4, verbatim -- including the decomp's own self-assignment,
 * `wp->physics.vel_air.x = wp->physics.vel_air.x`, which carries its own
 * `// Bruh`. */
sb32 itStarmieWeaponSwiftProcUpdate(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    wp->physics.vel_air.x = wp->physics.vel_air.x; /* Bruh */

    if (wpMainDecLifeCheckExpire(wp) != FALSE)
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itmonster/itstarmie.c:394-401 itStarmieWeaponSwiftProcHit 0x801823E8,
 * verbatim: the star burst, oriented by the weapon's own facing. */
sb32 itStarmieWeaponSwiftProcHit(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    efManagerStarSplashMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f, wp->lr);

    return TRUE;
}

/* it/itmonster/itstarmie.c:403-421 itStarmieWeaponSwiftProcHop 0x80182418,
 * verbatim. */
sb32 itStarmieWeaponSwiftProcHop(GObj *weapon_gobj)
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

/* it/itmonster/itstarmie.c:423-438 itStarmieWeaponSwiftProcReflector
 * 0x801824C0, verbatim. */
sb32 itStarmieWeaponSwiftProcReflector(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    FTStruct *fp = ftGetStruct(wp->owner_gobj);

    wpMainReflectorSetLR(wp, fp);

    DObjGetStruct(weapon_gobj)->rotate.vec.f.z = syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x);
    DObjGetStruct(weapon_gobj)->scale.vec.f.x = 1.0F;

    wp->lr = -wp->lr;

    return FALSE;
}

/* it/itmonster/itstarmie.c:440-471 itStarmieWeaponSwiftMakeWeapon
 * 0x80182530, verbatim. The weapon's `lr` is the ITEM's, not re-derived,
 * and the 180-degree turn on a rightward shot is the same convention the
 * item's own facing uses. `wpMainDecLifeCheckExpire` is what the lifetime
 * counts down through. */
GObj* itStarmieWeaponSwiftMakeWeapon(GObj *item_gobj, Vec3f *pos)
{
    ITStruct *ip = itGetStruct(item_gobj);
    GObj *weapon_gobj = wpManagerMakeWeapon(item_gobj, &dITStarmieWeaponSwiftWeaponDesc, pos, WEAPON_FLAG_PARENT_ITEM);
    DObj *dobj;
    s32 unused;
    WPStruct *wp;

    if (weapon_gobj == NULL)
    {
        return NULL;
    }
    wp = wpGetStruct(weapon_gobj);

    wp->lr = ip->lr;

    wp->physics.vel_air.x = wp->lr * ITSTARMIE_SWIFTVEL_X;

    dobj = DObjGetStruct(weapon_gobj);

    dobj->translate.vec.f = *pos;

    efManagerSparkleWhiteScaleMakeEffect(&dobj->translate.vec.f, 1.0F);

    wp->lifetime = ITSTARMIE_SWIFT_LIFETIME;

    if (wp->lr == +1)
    {
        dobj->rotate.vec.f.y = F_CST_DTOR32(180.0F);
    }
    return weapon_gobj;
}

/* it/itmonster/itstarmie.c:473-476 itStarmieAttackMakeSwift 0x80182608,
 * verbatim: a one-line wrapper, and the only `Make*` in the thirteen that
 * makes exactly one weapon. */
void itStarmieAttackMakeSwift(GObj *item_gobj, Vec3f *pos)
{
    itStarmieWeaponSwiftMakeWeapon(item_gobj, pos);
}
