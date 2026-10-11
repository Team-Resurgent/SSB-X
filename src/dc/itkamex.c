/* itkamex.c -- it/itmonster/itkamex.c, verbatim: Blastoise's `ITDesc`, its
 * three-row `ITStatusDesc` table (fall / appear / attack) and their proc
 * bodies, plus the Hydro Pump weapon's `WPDesc` and its four procs.
 * Function-for-function against the game's own ITStruct and WPStruct
 * (it/ittypes.h, wp/wptypes.h); every function names its decomp line
 * range.
 *
 * The thirteenth and last of the monsters before Pippi, which
 * binds them all. Blastoise is the one that makes the summoner do the
 * facing:
 *
 *  - `itKamexMakeItem` does NOT pick its own `lr`. It reads the PARENT's
 *    ITStruct -- the Poké Ball's, one frame before the monster exists --
 *    for `owner_gobj` and (US build) `team`, then calls
 *    `itKamexCommonFindTargetsSetLR`, a nearest-enemy walk over the
 *    fighter link that turns it toward whichever side the closest
 *    opponent is on. It is Hitmonlee's and Starmie's walk a third time,
 *    with the same `players == 0` first iteration and the same
 *    uninitialised `victim_gobj` -- and, this being MakeItem, it runs on
 *    every spawn rather than only when an attack state is entered.
 *  - `nITKamexStatusFall` and `nITKamexStatusAppear` differ only in one
 *    argument: both land into `itKamexAttackInitVars`, with TRUE and FALSE
 *    for `is_ignore_setup`. FALSE means the item is arriving from the
 *    POKÉ BALL's throw and gets the lifetime, the display list and the
 *    enlarged collision box; TRUE means it is re-entering from a wall
 *    bounce and keeps everything it already has. The BOX is 341 units on
 *    every side (ITKAMEX_COLL_SIZE), which is what makes Blastoise the
 *    biggest thing the item system moves.
 *  - `nITKamexStatusAttack` fires a Hydro Pump every CONST + rand(RANDOM)
 *    frames, sets `is_apply_push` so the NEXT frames' updates add
 *    `hydro_push_vel_x` to its X velocity, and pushes itself back
 *    ITKAMEX_CONSTVEL_X on every shot -- the same retreat-as-it-fires
 *    shape Starmie's has, in the opposite direction (it sets the velocity
 *    outright where Starmie accumulates).
 *
 * The Hydro Pump is flags 0x01 and is the one monster weapon whose
 * proc_update writes into its own ATTACK BOX rather than its size:
 * `wp->attack_coll.offsets[0].x = dobj->child->translate.vec.f.x * wp->lr`
 * -- the child joint's X translation, filtered by lr, is the hitbox's own
 * offset. Its `proc_hit` is a bare `return FALSE;`: the stream survives
 * everything it touches.
 *
 * DIVERGES: the two descriptors' offset fields are numbers rather than the
 * decomp's `&llITCommonData...` symbols -- see src/dc/itemoffsets.h -- and
 * the `itGetPData`/`itGetMonsterAnimNode` calls drop their `&`. No
 * function body diverges.
 */
#include <it/item.h>
#include <wp/weapon.h>
#include <ft/fighter.h>
#include <ef/efmanager.h>       /* efManagerDamageSpawnSparksMakeEffect,
                                 * efManagerDustHeavyMakeEffect,
                                 * efManagerSparkleWhiteScaleMakeEffect */

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

/* it/itmonster/itkamex.c:11-34 dITKamexItemDesc, verbatim. */
ITDesc dITKamexItemDesc =
{
    nITKindKamex,                           /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataKamexItemAttributes,    /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,            /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itKamexCommonProcUpdate,                /* Proc Update */
    itKamexCommonProcMap,                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itmonster/itkamex.c:36-80 dITKamexStatusDescs, verbatim. All three
 * rows carry a proc_map -- unlike most of the thirteen, whose states do
 * their bound tests in the update. */
ITStatusDesc dITKamexStatusDescs[/* */] =
{
    /* Status 0 (Air Fall) */
    {
        itKamexFallProcUpdate,              /* Proc Update */
        itKamexFallProcMap,                 /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 1 (Neutral Appear) */
    {
        itKamexAppearProcUpdate,            /* Proc Update */
        itKamexAppearProcMap,               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 2 (Neutral Attack) */
    {
        itKamexAttackProcUpdate,            /* Proc Update */
        itKamexAttackProcMap,               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    }
};

/* it/itmonster/itkamex.c:82-105 dITKamexWeaponHydroWeaponDesc, verbatim.
 * Its proc_hop is NULL -- a stream of water does not bounce -- and its
 * proc_hit is shared three ways. */
WPDesc dITKamexWeaponHydroWeaponDesc =
{
    0x01,                                      /* Render flags? */
    nWPKindKamexHydro,                         /* Weapon Kind */
    &gITManagerCommonData,                     /* Pointer to weapon's loaded files? */
    (intptr_t)llITCommonDataKamexHydroWeaponAttributes, /* Offset of weapon attributes in loaded files */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyRSca,         /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    itKamexWeaponHydroProcUpdate,           /* Proc Update */
    NULL,                                   /* Proc Map */
    itKamexWeaponHydroProcHit,              /* Proc Hit */
    itKamexWeaponHydroProcHit,              /* Proc Shield */
    NULL,                                   /* Proc Hop */
    itKamexWeaponHydroProcHit,              /* Proc Set-Off */
    itKamexWeaponHydroProcReflector,        /* Proc Reflector */
    itKamexWeaponHydroProcHit               /* Proc Absorb */
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itKamexStatus
{
    nITKamexStatusFall,
    nITKamexStatusAppear,
    nITKamexStatusAttack
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itkamex.c:122-165 itKamexAttackUpdateHydro 0x80180630,
 * verbatim. The shot places the weapon at Blastoise's own kind offsets,
 * sparks the spawn point, sets the retreat velocity OUTRIGHT (not by
 * accumulation), and lays down dust at its feet on the far side -- the
 * last of which only Blastoise's own kind gets, since the dust's X offset
 * is inside the `kind` test while the push is not. */
void itKamexAttackUpdateHydro(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if (ip->item_vars.kamex.hydro_spawn_wait <= 0)
    {
        Vec3f pos = dobj->translate.vec.f;

        if (ip->kind == nITKindKamex)
        {
            pos.x += ITKAMEX_KAMEX_HYDRO_SPAWN_OFF_X * ip->lr;
            pos.y += ITKAMEX_KAMEX_HYDRO_SPAWN_OFF_Y;
        }
        else pos.x += ITKAMEX_OTHER_HYDRO_SPAWN_OFF_X * ip->lr;

        itKamexAttackMakeHydro(item_gobj, &pos);
        efManagerDamageSpawnSparksMakeEffect(&pos, ip->lr);
        func_800269C0_275C0(nSYAudioFGMKamexHydro);

        ip->item_vars.kamex.hydro_spawn_wait = syUtilsRandIntRange(ITKAMEX_HYDRO_SPAWN_WAIT_RANDOM) + ITKAMEX_HYDRO_SPAWN_WAIT_CONST;

        pos = dobj->translate.vec.f;

        pos.y += ip->attr->map_coll_bottom;

        if (ip->kind == nITKindKamex)
        {
            pos.x += (ip->attr->map_coll_width + ITKAMEX_DUST_SPAWN_OFF_X) * -ip->lr;
        }
        ip->item_vars.kamex.is_apply_push = TRUE;

        ip->physics.vel_air.x = -ip->lr * ITKAMEX_CONSTVEL_X;

        efManagerDustHeavyMakeEffect(&pos, -ip->lr);
    }
}

/* it/itmonster/itkamex.c:167-175 itKamexFallProcUpdate 0x801807DC,
 * verbatim. */
sb32 itKamexFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITKAMEX_GRAVITY, ITKAMEX_TVEL);

    return FALSE;
}

/* it/itmonster/itkamex.c:177-190 itKamexFallProcMap 0x80180808, verbatim.
 * The mask refresh is asked for CEIL and the two WALLS and the test is for
 * FLOOR, which is the decomp's own shape -- a refresh is a request, and
 * the mask that comes back holds everything it found. TRUE for
 * `is_ignore_setup`: this state is the WALL-BOUNCE re-entry, so the item
 * keeps what it has. */
sb32 itKamexFallProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMapTestAllCollisionFlag(item_gobj, (MAP_FLAG_CEIL | MAP_FLAG_RWALL | MAP_FLAG_LWALL));

    if (ip->coll_data.mask_curr & MAP_FLAG_FLOOR)
    {
        itKamexAttackInitVars(item_gobj, TRUE);
        itKamexAttackSetStatus(item_gobj);
    }
    return FALSE;
}

/* it/itmonster/itkamex.c:192-204 itKamexFallInitVars 0x80180860,
 * verbatim. */
void itKamexFallInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMapSetAir(ip);

    ip->physics.vel_air.x = ip->physics.vel_air.y = 0.0F;

    ip->is_allow_pickup = FALSE;

    ip->item_vars.kamex.hydro_push_vel_x = 0.0F;
}

/* it/itmonster/itkamex.c:206-210 itKamexFallSetStatus 0x801808A4,
 * verbatim. */
void itKamexFallSetStatus(GObj *item_gobj)
{
    itKamexFallInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITKamexStatusDescs, nITKamexStatusFall);
}

/* it/itmonster/itkamex.c:212-220 itKamexAppearProcUpdate 0x801808D8,
 * verbatim. */
sb32 itKamexAppearProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITKAMEX_GRAVITY, ITKAMEX_TVEL);

    return FALSE;
}

/* it/itmonster/itkamex.c:222-236 itKamexAppearProcMap 0x80180904,
 * verbatim. FALSE here: the arrival from the ball DOES set up. */
sb32 itKamexAppearProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMapTestAllCollisionFlag(item_gobj, (MAP_FLAG_CEIL | MAP_FLAG_RWALL | MAP_FLAG_LWALL));

    if (ip->coll_data.mask_curr & MAP_FLAG_FLOOR)
    {
        ip->physics.vel_air.y = 0.0F;

        itKamexAttackInitVars(item_gobj, FALSE);
        itKamexAttackSetStatus(item_gobj);
    }
    return FALSE;
}

/* it/itmonster/itkamex.c:238-248 itKamexAppearSetStatus 0x80180964,
 * verbatim. The lifetime is set here AND in itKamexAttackInitVars's FALSE
 * arm -- the same number twice, which is why the appear state can hand
 * over to the attack without losing its clock. */
void itKamexAppearSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi = ITKAMEX_LIFETIME;

    if (ip->kind == nITKindKamex)
    {
        func_800269C0_275C0(nSYAudioVoiceMBallKamexAppear);
    }
    itMainSetStatus(item_gobj, dITKamexStatusDescs, nITKamexStatusAppear);
}

/* it/itmonster/itkamex.c:250-270 itKamexAttackProcUpdate 0x801809BC,
 * verbatim. The push is applied on the frames AFTER a shot, gated by
 * `is_apply_push`; the wait only decrements here, so the shot's own reset
 * of it is what sets the cadence. */
sb32 itKamexAttackProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        return TRUE;
    }
    itKamexAttackUpdateHydro(item_gobj);

    if (ip->item_vars.kamex.is_apply_push != FALSE)
    {
        ip->physics.vel_air.x += ip->item_vars.kamex.hydro_push_vel_x;
    }
    ip->item_vars.kamex.hydro_spawn_wait--;

    ip->multi--;

    return FALSE;
}

/* it/itmonster/itkamex.c:272-278 itKamexAttackProcMap 0x80180A30,
 * verbatim. */
sb32 itKamexAttackProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itKamexFallSetStatus);

    return FALSE;
}

/* it/itmonster/itkamex.c:280-311 itKamexAttackInitVars 0x80180A58,
 * verbatim. The `is_ignore_setup == FALSE` arm is the whole difference
 * between arriving and re-entering: the lifetime, the display list and the
 * 341-unit box are set only for an arrival. The box write is what makes
 * Blastoise big enough to shove fighters around; the `dobj->dl` write
 * goes through itemModelSetDisplayList, because `dl` IS the payload the
 * port draws from (itemmodel.h). */
void itKamexAttackInitVars(GObj *item_gobj, sb32 is_ignore_setup)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if (is_ignore_setup == FALSE)
    {
        ip->multi = ITKAMEX_LIFETIME;

        if (ip->kind == nITKindKamex)
        {
            Gfx *dl = (Gfx*) itGetPData(ip, llITCommonDataKamexDataStart, llITCommonDataKamexDisplayList);

            itemModelSetDisplayList(dobj, dl);    /* DIVERGES: itemmodel.h */

            ip->coll_data.map_coll.top = ITKAMEX_COLL_SIZE;
            ip->coll_data.map_coll.center = 0.0F;
            ip->coll_data.map_coll.bottom = -ITKAMEX_COLL_SIZE;
            ip->coll_data.map_coll.width = ITKAMEX_COLL_SIZE;
        }
    }
    ip->physics.vel_air.x = ip->physics.vel_air.y = 0;

    ip->item_vars.kamex.hydro_push_vel_x = ip->lr * ITKAMEX_PUSH_VEL_X;
    ip->item_vars.kamex.hydro_spawn_wait = 0;
    ip->item_vars.kamex.is_apply_push = FALSE;
}

/* it/itmonster/itkamex.c:313-317 itKamexAttackSetStatus 0x80180AF4,
 * verbatim. */
void itKamexAttackSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITKamexStatusDescs, nITKamexStatusAttack);
}

/* it/itmonster/itkamex.c:319-333 itKamexCommonProcUpdate 0x80180B1C,
 * verbatim. */
sb32 itKamexCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        ip->physics.vel_air.y = 0.0F;

        itKamexAppearSetStatus(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itkamex.c:335-345 itKamexCommonProcMap 0x80180B6C,
 * verbatim. */
sb32 itKamexCommonProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR) != FALSE)
    {
        ip->physics.vel_air.y = 0.0F;
    }
    return FALSE;
}

/* it/itmonster/itkamex.c:347-395 itKamexCommonFindTargetsSetLR 0x80180BAC,
 * verbatim, both REGION_US arms. The third copy of the nearest-enemy walk
 * -- the same one Hitmonlee's attack and Starmie's follow state make --
 * with the same `players == 0` first iteration and the same uninitialised
 * `victim_gobj`. Its difference from those two is that it does not stop
 * at the walk: it re-reads the victim's X to set `lr`, and it is called
 * from MakeItem, so a Poké Ball whose ball is popped with no fighters left
 * on the link would read an uninitialised pointer here. */
void itKamexCommonFindTargetsSetLR(GObj *item_gobj)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
    s32 unused1;
    GObj *victim_gobj;
    s32 unused2[3];
    ITStruct *ip = itGetStruct(item_gobj);
#if defined(REGION_JP)
    FTStruct *owner_fp = ftGetStruct(ip->owner_gobj);
#endif
    DObj *dobj = DObjGetStruct(item_gobj);
    f32 dist_xy;
    f32 dist_x;
    Vec3f dist;
    f32 square_xy;
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
    }
    dist_x = DObjGetStruct(victim_gobj)->translate.vec.f.x - dobj->translate.vec.f.x;

    ip->lr = (dist_x < 0.0F) ? -1 : +1;
}

/* it/itmonster/itkamex.c:397-437 itKamexMakeItem 0x80180CDC, verbatim.
 * The one MakeItem in the thirteen that reads its PARENT's ITStruct: the
 * Poké Ball is still alive at this point, and `owner_gobj`/`team` (the
 * latter on the US build only) come off it so the monster inherits the
 * thrower's allegiance before the ball is destroyed. */
GObj* itKamexMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITKamexItemDesc, pos, vel, flags);
    DObj *dobj;
    ITStruct *kamex_ip;
    ITStruct *mball_ip;

    if (item_gobj != NULL)
    {
        dobj = DObjGetStruct(item_gobj);

        gcAddXObjForDObjFixed(dobj, 0x48, 0);

        dobj->translate.vec.f = *pos;

        kamex_ip = itGetStruct(item_gobj);

        kamex_ip->multi = ITMONSTER_RISE_STOP_WAIT;

        kamex_ip->physics.vel_air.x = kamex_ip->physics.vel_air.z = 0.0F;
        kamex_ip->physics.vel_air.y = ITMONSTER_RISE_VEL_Y;

        mball_ip = itGetStruct(parent_gobj);

        kamex_ip->owner_gobj = mball_ip->owner_gobj;
#if defined(REGION_US)
        kamex_ip->team = mball_ip->team;
#endif

        itKamexCommonFindTargetsSetLR(item_gobj);

        if (kamex_ip->lr == -1)
        {
            dobj->rotate.vec.f.y = F_CST_DTOR32(180.0F);
        }
        dobj->translate.vec.f.y -= kamex_ip->attr->map_coll_bottom;

        gcAddDObjAnimJoint(dobj, itGetMonsterAnimNode(kamex_ip, llITCommonDataKamexDataStart), 0.0F);
    }
    return item_gobj;
}

/* ---- the Hydro Pump weapon ----------------------------------------- */

/* it/itmonster/itkamex.c:439-451 itKamexWeaponHydroProcUpdate 0x80180E10,
 * verbatim. THE ONE monster weapon that writes its ATTACK OFFSET rather
 * than its size: the child joint's X translation times `lr` is where the
 * stream's hitbox sits, which is what makes a reflected stream sweep back
 * the other way. */
sb32 itKamexWeaponHydroProcUpdate(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    DObj *dobj = DObjGetStruct(weapon_gobj);

    wp->attack_coll.offsets[0].x = dobj->child->translate.vec.f.x * wp->lr;

    if (wpMainDecLifeCheckExpire(wp) != FALSE)
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itmonster/itkamex.c:453-456 itKamexWeaponHydroProcHit 0x80180E60,
 * verbatim: a bare FALSE. The stream is not destroyed by what it touches,
 * and it has no spark of its own. */
sb32 itKamexWeaponHydroProcHit(GObj *weapon_gobj)
{
    return FALSE;
}

/* it/itmonster/itkamex.c:458-471 itKamexWeaponHydroProcReflector
 * 0x80180E6C, verbatim. */
sb32 itKamexWeaponHydroProcReflector(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    FTStruct *fp = ftGetStruct(wp->owner_gobj);

    wpMainReflectorSetLR(wp, fp);

    DObjGetStruct(weapon_gobj)->rotate.vec.f.z = syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x);
    DObjGetStruct(weapon_gobj)->scale.vec.f.x = 1.0F;

    wp->lr = -wp->lr;

    return FALSE;
}

/* it/itmonster/itkamex.c:473-503 itKamexWeaponHydroMakeWeapon 0x80180EDC,
 * verbatim. The sparkle is emitted at the weapon's OWN translate, which
 * wpManagerMakeWeapon has already set from `pos`; the 180-degree turn is
 * on `lr == -1` (the reverse of Starmie's, whose Swift turns on +1), and
 * the two `unk_0x0`/`unk_0x4` writes carry the decomp's own "Set but never
 * used?" comments. */
GObj* itKamexWeaponHydroMakeWeapon(GObj *item_gobj, Vec3f *pos)
{
    ITStruct *ip = itGetStruct(item_gobj);
    GObj *weapon_gobj = wpManagerMakeWeapon(item_gobj, &dITKamexWeaponHydroWeaponDesc, pos, WEAPON_FLAG_PARENT_ITEM);
    DObj *dobj;
    s32 unused;
    WPStruct *wp;
    Vec3f translate;

    if (weapon_gobj == NULL)
    {
        return NULL;
    }
    wp = wpGetStruct(weapon_gobj);

    wp->lr = ip->lr;

    dobj = DObjGetStruct(weapon_gobj);

    translate = dobj->translate.vec.f;

    efManagerSparkleWhiteScaleMakeEffect(&translate, 1.0F);

    if (wp->lr == -1)
    {
        dobj->rotate.vec.f.y = F_CST_DTOR32(180.0F);
    }
    wp->weapon_vars.hydro.unk_0x0 = 0; /* Set but never used? */
    wp->weapon_vars.hydro.unk_0x4 = 0; /* Set but never used? */

    wp->lifetime = ITKAMEX_HYDRO_LIFETIME;

    return weapon_gobj;
}

/* it/itmonster/itkamex.c:505-508 itKamexAttackMakeHydro 0x80180F9C,
 * verbatim: a one-line wrapper, as Starmie's Swift and Beedrill's swarm
 * have. */
void itKamexAttackMakeHydro(GObj *item_gobj, Vec3f *pos)
{
    itKamexWeaponHydroMakeWeapon(item_gobj, pos);
}
