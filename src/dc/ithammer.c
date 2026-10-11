/* ithammer.c -- it/itcommon/ithammer.c, verbatim except one function left
 * out (see below): the Hammer's `ITDesc`, its five-state `ITStatusDesc`
 * table (ground wait / air fall / fighter hold / fighter throw / fighter
 * drop), and their proc bodies. Function-for-function against the game's
 * own ITStruct (it/ittypes.h); every function names its decomp line
 * range.
 *
 * The roster's sixth entry -- read fresh rather than
 * assumed. Close to Sword/Bat's five-state shape, but with two real
 * differences, not a twin:
 *
 * - The Thrown/Dropped rows wire NO `proc_hop`/`proc_reflector` at all
 *   (both NULL) -- the Hammer doesn't get hopped over or reflected the
 *   way a thrown Sword/Bat does. `itMainCommonProcHop`/`ProcReflector`
 *   are simply absent from this item's table, not a blocker.
 * - `itHammerThrownSetStatus`/`DroppedSetStatus` both call
 *   `ftParamTryUpdateItemMusic()` (ft/ftparam.c:121-131, the star/hammer
 *   music switch, already ported) in addition to the same unconditional
 *   `DObjGetStruct(item_gobj)->child` dereference Sword/Bat's own pair
 *   has -- still dead code today for the same reason (only reachable via
 *   `itMainSetFighterThrow`/`Drop`), but this file's
 *   own version of that dead code has a real side effect to prove, not
 *   just the dereference. `itHammerDroppedSetStatus` additionally calls
 *   `itMainClearColAnim` (it/itmain.c) first.
 * - One extra function neither Sword nor Bat has:
 *   `itHammerCommonSetColAnim`, a one-line wrapper over
 *   `itMainCheckSetColAnimID` -- not
 *   referenced from anywhere else in THIS file (no `ITStatusDesc` row
 *   names it, no other function calls it), so it is dead code in the
 *   decomp itself, same status as every other `*DroppedSetStatus` this
 *   port has kept verbatim, ported here for the same reason.
 *
 * `itHammerMakeItem` is ported now, and it is the decomp's line for
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

/* it/itcommon/ithammer.c: llITCommonDataHammerItemAttributes -- reloc
 * stand-in, the same kind itstar.c/ittomato.c/itheart.c/itsword.c/itbat.c's
 * own `ITDesc` counterparts need (see itstar.c's file
 * header for the full reasoning). Only ever read as an offset from
 * gITManagerCommonData, which is NULL until
 * itManagerInitItems runs. */

/* it/itcommon/ithammer.c:10-32 dITHammerItemDesc, verbatim. */
ITDesc dITHammerItemDesc =
{
    nITKindHammer,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataHammerItemAttributes,

    {
        nGCMatrixKindTraRotRpyR,
        nGCMatrixKindNull,
        0
    },

    nGMAttackStateOff,
    itHammerFallProcUpdate,
    itHammerFallProcMap,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/* it/itcommon/ithammer.c:34-94 dITHammerStatusDescs, verbatim. Note the
 * Thrown/Dropped rows' proc_hop/proc_reflector are both NULL, unlike
 * Sword/Bat's own -- the Hammer doesn't hop or reflect. */
ITStatusDesc dITHammerStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    {
        NULL,
        itHammerWaitProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 1 (Air Wait Fall) */
    {
        itHammerFallProcUpdate,
        itHammerFallProcMap,
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
        itHammerThrownProcUpdate,
        itHammerThrownProcMap,
        itHammerCommonProcHit,
        itHammerCommonProcHit,
        NULL,
        itHammerCommonProcHit,
        NULL,
        NULL
    },

    /* Status 4 (Fighter Drop) */
    {
        itHammerFallProcUpdate,
        itHammerDroppedProcMap,
        itHammerCommonProcHit,
        itHammerCommonProcHit,
        NULL,
        itHammerCommonProcHit,
        NULL,
        NULL
    }
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itHammerStatus
{
    nITHammerStatusWait,
    nITHammerStatusFall,
    nITHammerStatusHold,
    nITHammerStatusThrown,
    nITHammerStatusDropped,
    nITHammerStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/ithammer.c:119-123 itHammerCommonSetColAnim 0x80176110,
 * verbatim. Dead code today -- no ITStatusDesc row and no other function
 * in this file names it, same status as every other roster item's own
 * dead SetStatus function. */
void itHammerCommonSetColAnim(GObj *item_gobj)
{
    itMainCheckSetColAnimID(item_gobj, nGMColAnimItemHammerEnd, 0);
}

/* it/itcommon/ithammer.c:125-134 itHammerFallProcUpdate 0x80176134,
 * verbatim. */
sb32 itHammerFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITHAMMER_GRAVITY, ITHAMMER_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/ithammer.c:136-142 itHammerWaitProcMap 0x8017616C,
 * verbatim. */
sb32 itHammerWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itHammerFallSetStatus);

    return FALSE;
}

/* it/itcommon/ithammer.c:144-148 itHammerFallProcMap 0x80176194,
 * verbatim. */
sb32 itHammerFallProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITHAMMER_MAP_REBOUND_COMMON, ITHAMMER_MAP_REBOUND_GROUND, itHammerWaitSetStatus);
}

/* it/itcommon/ithammer.c:150-155 itHammerWaitSetStatus 0x801761C4,
 * verbatim. */
void itHammerWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITHammerStatusDescs, nITHammerStatusWait);
}

/* it/itcommon/ithammer.c:157-166 itHammerFallSetStatus 0x801761F8,
 * verbatim. */
void itHammerFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITHammerStatusDescs, nITHammerStatusFall);
}

/* it/itcommon/ithammer.c:168-174 itHammerHoldSetStatus 0x8017623C,
 * verbatim. */
void itHammerHoldSetStatus(GObj *item_gobj)
{
    DObjGetStruct(item_gobj)->rotate.vec.f.y = F_CST_DTOR32(0.0F);

    itMainSetStatus(item_gobj, dITHammerStatusDescs, nITHammerStatusHold);
}

/* it/itcommon/ithammer.c:176-185 itHammerThrownProcUpdate 0x80176270,
 * verbatim. Same body as itHammerFallProcUpdate -- kept as its own
 * function, same shape itBatThrownProcUpdate has. */
sb32 itHammerThrownProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITHAMMER_GRAVITY, ITHAMMER_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/ithammer.c:187-191 itHammerThrownProcMap 0x801762A8,
 * verbatim. */
sb32 itHammerThrownProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITHAMMER_MAP_REBOUND_COMMON, ITHAMMER_MAP_REBOUND_GROUND, itHammerWaitSetStatus);
}

/* it/itcommon/ithammer.c:193-203 itHammerCommonProcHit 0x801762D8,
 * verbatim. */
sb32 itHammerCommonProcHit(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->attack_coll.attack_state = nGMAttackStateOff;

    itMainVelSetRebound(item_gobj);

    return FALSE;
}

/* it/itcommon/ithammer.c:205-213 itHammerThrownSetStatus 0x80176300,
 * verbatim. Dead code today (see the file header) but its
 * ftParamTryUpdateItemMusic() call is a real, already-ported side effect
 * to prove, not just the ->child dereference. */
void itHammerThrownSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITHammerStatusDescs, nITHammerStatusThrown);

    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = F_CST_DTOR32(90.0F);

    ftParamTryUpdateItemMusic();
}

/* it/itcommon/ithammer.c:215-219 itHammerDroppedProcMap 0x80176348,
 * verbatim. */
sb32 itHammerDroppedProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITHAMMER_MAP_REBOUND_COMMON, ITHAMMER_MAP_REBOUND_GROUND, itHammerWaitSetStatus);
}

/* it/itcommon/ithammer.c:221-230 itHammerDroppedSetStatus 0x80176378,
 * verbatim. Dead code today, same caveat as ThrownSetStatus above. */
void itHammerDroppedSetStatus(GObj *item_gobj)
{
    itMainClearColAnim(item_gobj);
    itMainSetStatus(item_gobj, dITHammerStatusDescs, nITHammerStatusDropped);

    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = F_CST_DTOR32(90.0F);

    ftParamTryUpdateItemMusic();
}

/* it/itcommon/ithammer.c:233-248 itHammerMakeItem 0x801763C8.
 * The decomp's own comment for this one reads 0x8017633C8 -- nine hex
 * digits, a stray 3. Its neighbours are 0x30 and 0x50 apart and
 * 0x80176378 + 0x50 is 0x801763C8, which is the address written here. */
GObj* itHammerMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITHammerItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        DObjGetStruct(item_gobj)->rotate.vec.f.y = F_CST_DTOR32(90.0F);

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}
