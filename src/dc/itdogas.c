/* itdogas.c -- it/itmonster/itdogas.c, verbatim: Koffing's `ITDesc`, its
 * two-row `ITStatusDesc` table (active / disappear) and their proc bodies,
 * plus the Smog weapon's `WPDesc` and that weapon's two procs.
 * Function-for-function against the game's own ITStruct and WPStruct
 * (it/ittypes.h, wp/wptypes.h); every function names its decomp line range.
 *
 * The seventh monster. Koffing is Meowth's shape with the
 * COUNT in `multi` instead of a schedule:
 *
 *  - `itDogasAttackInitVars` sets `multi = ITDOGAS_SMOG_SPAWN_COUNT` and a
 *    zero `smog_spawn_wait`, so the FIRST smog cloud comes out on the very
 *    next frame and each one after it resets the wait to
 *    ITDOGAS_SMOG_SPAWN_WAIT. `itDogasAttackUpdateSmog` is the payout and
 *    `itDogasAttackProcUpdate` is the counter -- and note the ORDER: the
 *    payout's own decrement runs, then the `multi == 0` test, then the
 *    wait's decrement. A cloud is thus spawned on the frame `multi` falls
 *    to zero, not on the one after.
 *  - Each cloud is placed at a random offset inside ±ITDOGAS_SMOG_MUL_OFF
 *    of Koffing and its velocity FLIPS PER AXIS to point away from the
 *    centre it was placed off -- so the puffs spread out rather than all
 *    drifting one way.
 *  - `nITDogasStatusDisappear` is a plain ITDOGAS_DESPAWN_WAIT countdown
 *    that returns TRUE at zero.
 *  - The anim hangs on the CHILD joint and only for Koffing's own kind,
 *    the way Chansey's does.
 *
 * The Smog weapon is flags 0x03 -- a DObjDesc tree whose nodes carry DL
 * links -- and it is the one weapon in the thirteen that RESIZES ITS OWN
 * HITBOX every frame: `itDogasWeaponSmogProcUpdate` writes
 * `attack_coll.size = dobj->scale.x * attr->size` off the CHILD joint's
 * scale, which is what makes a cloud that grows as it drifts. That is why
 * `itDogasWeaponSmogMakeWeapon` stores the weapon's own WPAttributes into
 * `weapon_vars.smog.attr` -- the same table `wpManagerMakeWeapon` already
 * read, kept so the per-frame update can reach `size` without a second
 * lookup. (The decomp's comment on that line is "Dude I had a stroke
 * trying to match this", which is kept: it is the decomp's own text.)
 *
 * DIVERGES: the two descriptors' offset fields are numbers rather than the
 * decomp's `&llITCommonData...` symbols -- see src/dc/itemoffsets.h -- and
 * the `itGetPData`/`itGetMonsterAnimNode` calls drop their `&`. No
 * function body diverges.
 */
#include <it/item.h>
#include <wp/weapon.h>

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

/* it/itmonster/itdogas.c:11-34 dITDogasItemDesc, verbatim. */
ITDesc dITDogasItemDesc =
{
    nITKindDogas,                           /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataDogasItemAttributes,    /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindNull,                  /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateOff,                      /* Hitbox Update State */
    itDogasCommonProcUpdate,                /* Proc Update */
    itDogasCommonProcMap,                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itmonster/itdogas.c:36-62 dITDogasStatusDescs, verbatim. The status
 * enum below spells these without the nIT prefix, which is the decomp's
 * own inconsistency and is kept. */
ITStatusDesc dITDogasStatusDescs[/* */] =
{
    /* Status 0 (Neutral Active) */
    {
        itDogasAttackProcUpdate,            /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 1 (Neutral Disappear) */
    {
        itDogasDisappearProcUpdate,         /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    }
};

/* it/itmonster/itdogas.c:64-87 dITDogasWeaponSmogWeaponDesc, verbatim. */
WPDesc dITDogasWeaponSmogWeaponDesc =
{
    0x03,                                   /* Render flags? */
    nWPKindDogasSmog,                       /* Weapon Kind */
    &gITManagerCommonData,                  /* Pointer to weapon's loaded files? */
    (intptr_t)llITCommonDataDogasSmogWeaponAttributes,  /* Offset of weapon attributes in loaded files */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,            /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    itDogasWeaponSmogProcUpdate,            /* Proc Update */
    NULL,                                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Absorb */
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itDogasStatus
{
    itDogasStatusAttack,
    itDogasStatusDisappear,
    itDogasStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itdogas.c:108-120 itDogasDisappearProcUpdate 0x80182C80,
 * verbatim. */
sb32 itDogasDisappearProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        return TRUE;
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itdogas.c:122-130 itDogasDisappearSetStatus 0x80182CA8,
 * verbatim. */
void itDogasDisappearSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi = ITDOGAS_DESPAWN_WAIT;

    itMainSetStatus(item_gobj, dITDogasStatusDescs, itDogasStatusDisappear);
}

/* it/itmonster/itdogas.c:132-166 itDogasAttackUpdateSmog 0x80182CDC,
 * verbatim. The two per-axis flips are what spread the cloud: the spawn
 * offset and the velocity are decided independently, and the velocity is
 * turned around whenever the offset came out on the far side of the
 * centre. The decomp's own `ITDGOAS_SMOG_SUB_OFF_Y` misspelling is kept --
 * it is the constant's name in it/itvars.h. */
void itDogasAttackUpdateSmog(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    Vec3f pos;
    Vec3f vel;

    if (ip->item_vars.dogas.smog_spawn_wait <= 0)
    {
        vel.x = ITDOGAS_SMOG_VEL;
        vel.y = ITDOGAS_SMOG_VEL;
        vel.z = 0.0F;

        pos = dobj->translate.vec.f;

        pos.x += (syUtilsRandFloat() * ITDOGAS_SMOG_MUL_OFF_X) - ITDOGAS_SMOG_SUB_OFF_X;
        pos.y += (syUtilsRandFloat() * ITDOGAS_SMOG_MUL_OFF_Y) - ITDGOAS_SMOG_SUB_OFF_Y;

        if (pos.x < dobj->translate.vec.f.x)
        {
            vel.x = -vel.x;
        }
        if (pos.y < dobj->translate.vec.f.y)
        {
            vel.y = -vel.y;
        }
        itDogasWeaponSmogMakeWeapon(item_gobj, &pos, &vel);
        func_800269C0_275C0(nSYAudioFGMDogasSmog);

        ip->item_vars.dogas.smog_spawn_wait = ITDOGAS_SMOG_SPAWN_WAIT;

        ip->multi--;
    }
}

/* it/itmonster/itdogas.c:168-184 itDogasAttackProcUpdate 0x80182E1C,
 * verbatim. The payout runs FIRST, so the cloud for the last count is
 * spawned on the same frame the disappear status is entered -- and the
 * `return FALSE` after it is the decomp's own early exit, which skips the
 * wait decrement. */
sb32 itDogasAttackProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itDogasAttackUpdateSmog(item_gobj);

    if (ip->multi == 0)
    {
        itDogasDisappearSetStatus(item_gobj);

        return FALSE;
    }
    ip->item_vars.dogas.smog_spawn_wait--;

    return FALSE;
}

/* it/itmonster/itdogas.c:186-205 itDogasAttackInitVars 0x80182E78,
 * verbatim. */
void itDogasAttackInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    ip->multi = ITDOGAS_SMOG_SPAWN_COUNT;

    ip->item_vars.dogas.smog_spawn_wait = 0;

    if (ip->kind == nITKindDogas)
    {
        ip->item_vars.dogas.pos = dobj->translate.vec.f;

        gcAddDObjAnimJoint(dobj->child, itGetPData(ip, llITCommonDataDogasDataStart, llITCommonDataDogasAnimJoint), 0.0F);

        gcPlayAnimAll(item_gobj);
        func_800269C0_275C0(nSYAudioVoiceMBallDogasAppear);
    }
}

/* it/itmonster/itdogas.c:207-212 itDogasAttackSetStatus 0x80182F0C,
 * verbatim. */
void itDogasAttackSetStatus(GObj *item_gobj)
{
    itDogasAttackInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITDogasStatusDescs, itDogasStatusAttack);
}

/* it/itmonster/itdogas.c:214-228 itDogasCommonProcUpdate 0x80182F40,
 * verbatim. */
sb32 itDogasCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        ip->physics.vel_air.x = ip->physics.vel_air.y = 0.0F;

        itDogasAttackSetStatus(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itdogas.c:230-240 itDogasCommonProcMap 0x80182F94,
 * verbatim. */
sb32 itDogasCommonProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR) != FALSE)
    {
        ip->physics.vel_air.y = 0.0F;
    }
    return FALSE;
}

/* it/itmonster/itdogas.c:242-271 itDogasMakeItem 0x80182FD4, verbatim.
 * Two XObjs: 0x28 on the ROOT and the full TraRotRpyRSca on the CHILD,
 * which is the joint the anim is hung on. */
GObj* itDogasMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITDogasItemDesc, pos, vel, flags);
    DObj *dobj;
    ITStruct *ip;

    if (item_gobj != NULL)
    {
        dobj = DObjGetStruct(item_gobj);

        gcAddXObjForDObjFixed(dobj, 0x28, 0);
        gcAddXObjForDObjFixed(dobj->child, nGCMatrixKindTraRotRpyRSca, 0);

        dobj->translate.vec.f = *pos;

        ip = itGetStruct(item_gobj);

        dobj->translate.vec.f.y -= ip->attr->map_coll_bottom;

        ip->multi = ITMONSTER_RISE_STOP_WAIT;

        ip->physics.vel_air.x = 0.0F;
        ip->physics.vel_air.z = 0.0F;
        ip->physics.vel_air.y = ITMONSTER_RISE_VEL_Y;

        gcAddDObjAnimJoint(dobj->child, itGetMonsterAnimNode(ip, llITCommonDataDogasDataStart), 0.0F);
    }
    return item_gobj;
}

/* it/itmonster/itdogas.c:273-286 itDogasWeaponSmogProcUpdate 0x801830DC,
 * verbatim. The cloud's hitbox is re-derived every frame from the CHILD
 * joint's own scale times the stored `attr->size` -- the only weapon in
 * the thirteen that resizes itself, and the reason the maker below keeps
 * the attributes. */
sb32 itDogasWeaponSmogProcUpdate(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    DObj *dobj = DObjGetStruct(weapon_gobj)->child;

    wp->attack_coll.size = dobj->scale.vec.f.x * wp->weapon_vars.smog.attr->size;

    if (wpMainDecLifeCheckExpire(wp) != FALSE)
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itmonster/itdogas.c:288-315 itDogasWeaponSmogMakeWeapon 0x80183144,
 * verbatim, including the decomp's own comment on the attributes line.
 * The attributes it stores are the SAME table wpManagerMakeWeapon already
 * read -- reached the same way, through the descriptor's file and offset,
 * because the port's wpManagerMakeWeapon does the read verbatim and keeps
 * no copy for the caller. */
GObj* itDogasWeaponSmogMakeWeapon(GObj *item_gobj, Vec3f *pos, Vec3f *vel)
{
    WPDesc *weapon_desc = &dITDogasWeaponSmogWeaponDesc;
    GObj *weapon_gobj = wpManagerMakeWeapon(item_gobj, &dITDogasWeaponSmogWeaponDesc, pos, WEAPON_FLAG_PARENT_ITEM);
    DObj *dobj;
    WPStruct *wp;

    if (weapon_gobj == NULL)
    {
        return NULL;
    }
    wp = wpGetStruct(weapon_gobj);

    wp->lifetime = ITDOGAS_SMOG_LIFETIME;

    wp->weapon_vars.smog.attr = (WPAttributes*) ((uintptr_t)*weapon_desc->p_weapon + (intptr_t)weapon_desc->o_attributes); /* Dude I had a stroke trying to match this */

    dobj = DObjGetStruct(weapon_gobj);

    wp->physics.vel_air = *vel;

    gcAddXObjForDObjFixed(dobj->child, 0x2C, 0);

    dobj->translate.vec.f = *pos;

    return weapon_gobj;
}
