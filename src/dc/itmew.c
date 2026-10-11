/* itmew.c -- it/itmonster/itmew.c, verbatim: Mew's `ITDesc`, its one-row
 * `ITStatusDesc` table and its five proc bodies. Function-for-function
 * against the game's own ITStruct (it/ittypes.h); every function names its
 * decomp line range.
 *
 * The first of the thirteen Poké Ball monsters -- and the
 * smallest at 176 lines, which is why it is the one that establishes the
 * shape the other twelve follow:
 *
 *  - An `ITDesc` whose proc set is the monster's APPEARANCE state, not its
 *    real one: `itMewCommonProcUpdate` counts `ip->multi` down from
 *    ITMONSTER_RISE_STOP_WAIT (22, it/itvars.h) while the item levitates
 *    at ITMONSTER_RISE_VEL_Y (16.0F) and `itMewCommonProcMap` cancels that
 *    rise on a floor, then hands over to `itMewFlySetStatus` -- the
 *    monster's own state machine, which for Mew is one row.
 *  - `dITMewItemDesc.o_attributes` is Mew's own table in ITCommonData, and
 *    `itManagerMakeItem` is what turns it into a model. Its `data` block is
 *    at 0xbcc0 in ITCommonObject and Mew's anim scripts at 0x13624 --
 *    `itGetMonsterAnimNode`'s one bank, reached with the offset of the
 *    model block (src/dc/itmonster.h).
 *
 * DIVERGES, and there is only one, the same one every item file has:
 * `dITMewItemDesc`'s third field is `(intptr_t)llITCommonDataMewItemAttributes`
 * rather than the decomp's `&llITCommonDataMewItemAttributes`, because on
 * the N64 that symbol's ADDRESS is the offset -- see
 * src/dc/itemoffsets.h. Nothing in the function bodies diverges.
 */
#include <it/item.h>
#include <ef/efmanager.h>       /* efManagerHealSparklesMakeEffect,
                                 * efManagerRippleMakeEffect */

#include "itmonster.h"          /* itGetMonsterAnimNode */
#include "itemoffsets.h"        /* llITCommonData* offsets */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

/* func_800269C0_275C0, the FGM call. No decomp header declares it -- the
 * decomp relies on IDO's implicit declaration -- so the port's own
 * (src/dc/ftcommon.h) is the one used, rather than letting the call fall
 * through to an implicit int return. */
#include "ftcommon.h"

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itmew.c:11-39 dITMewItemDesc, verbatim. */
ITDesc dITMewItemDesc =
{
    nITKindMew,                             /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataMewItemAttributes,  /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyRSca,         /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateOff,                      /* Hitbox Update State */
    itMewCommonProcUpdate,                  /* Proc Update */
    itMewCommonProcMap,                     /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itmonster/itmew.c:41-56 dITMewStatusDescs, verbatim: one row. A
 * monster with no attack has nothing else to say. */
ITStatusDesc dITMewStatusDescs[/* */] =
{
    /* Status 0 (Neutral Fly) */
    {
        itMewFlyProcUpdate,                 /* Proc Update */
        NULL,                               /* Proc Map */
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

enum itMewStatus
{
    nITMewStatusFly,
    nITMewStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itmew.c:64-88 itMewFlyProcUpdate 0x8017EBE0, verbatim. The
 * spawn-int check is the decomp's own name for a modulo counter: Mew drops
 * one efManagerHealSparklesMakeEffect every ITMEW_EFFECT_SPAWN_INT frames
 * of its flight, and `multi` is the flight's remaining lifetime -- returning
 * TRUE at zero is what takes the item off the field. */
sb32 itMewFlyProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    Vec3f pos = DObjGetStruct(item_gobj)->translate.vec.f;

    if (ip->multi == 0)
    {
        return TRUE;
    }
    if (ip->item_vars.mew.esper_gfx_int == 0)
    {
        ip->item_vars.mew.esper_gfx_int = ITMEW_EFFECT_SPAWN_INT;

        efManagerHealSparklesMakeEffect(&pos);
    }
    ip->item_vars.mew.esper_gfx_int--;

    ip->multi--;

    ip->physics.vel_air.y += ITMEW_FLY_ADD_VEL_Y;

    return FALSE;
}

/* it/itmonster/itmew.c:91-114 itMewFlyInitVars 0x8017EC84, verbatim. Note
 * the second sound: only a Mew that IS the Poké Ball's monster cries; the
 * same script serves Saffron City's own Mew, which does not. */
void itMewFlyInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi = ITMEW_LIFETIME;

    if (syUtilsRandIntRange(2) != 0)
    {
        ip->physics.vel_air.x = ITMEW_STARTVEL_X;
    }
    else ip->physics.vel_air.x = -ITMEW_STARTVEL_X;

    ip->physics.vel_air.y = ITMEW_STARTVEL_Y;

    func_800269C0_275C0(nSYAudioFGMMewFly);

    if (ip->kind == nITKindMew)
    {
        func_800269C0_275C0(nSYAudioVoiceMBallMewAppear);
    }
    efManagerRippleMakeEffect(&DObjGetStruct(item_gobj)->translate.vec.f);

    ip->item_vars.mew.esper_gfx_int = 0;
}

/* it/itmonster/itmew.c:117-121 itMewFlySetStatus 0x8017ED20, verbatim */
void itMewFlySetStatus(GObj *item_gobj)
{
    itMewFlyInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITMewStatusDescs, nITMewStatusFly);
}

/* it/itmonster/itmew.c:124-139 itMewCommonProcUpdate 0x8017ED54, verbatim.
 * This is the Poké Ball's exit: `multi` here is ITMONSTER_RISE_STOP_WAIT
 * set by itMewMakeItem, and the zero-velocity write is what stops the
 * levitation the desc's own proc_map was cancelling frame by frame. */
sb32 itMewCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        ip->physics.vel_air.y = 0.0F;

        itMewFlySetStatus(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itmew.c:142-153 itMewCommonProcMap 0x8017EDA4, verbatim */
sb32 itMewCommonProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR) != FALSE)
    {
        ip->physics.vel_air.y = 0.0F;
    }
    return FALSE;
}

/* it/itmonster/itmew.c:156-176 itMewMakeItem 0x8017EDE4, verbatim. The
 * `map_coll_bottom` subtraction is every monster's: the spawner hands the
 * Poké Ball's own centre, and a monster stands ON the floor, so its model
 * has to come down by the height of its collision box before the anim
 * starts. */
GObj* itMewMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITMewItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        DObj *dobj = DObjGetStruct(item_gobj);
        ITStruct *ip = itGetStruct(item_gobj);

        ip->multi = ITMONSTER_RISE_STOP_WAIT;

        ip->physics.vel_air.x = ip->physics.vel_air.z = 0.0F;
        ip->physics.vel_air.y = ITMONSTER_RISE_VEL_Y;

        dobj->translate.vec.f.y -= ip->attr->map_coll_bottom;

        gcAddDObjAnimJoint(dobj, itGetMonsterAnimNode(ip, llITCommonDataMewDataStart), 0.0F);
    }
    return item_gobj;
}
