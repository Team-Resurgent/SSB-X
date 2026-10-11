/* itbat.c -- it/itcommon/itbat.c, verbatim except one function left out
 * (see below): the Home-Run Bat's `ITDesc`, its five-state
 * `ITStatusDesc` table (ground wait / air fall / fighter hold / fighter
 * throw / fighter drop), and their proc bodies. Function-for-function
 * against the game's own ITStruct (it/ittypes.h); every function names
 * its decomp line range.
 *
 * The roster's fifth entry, and Sword's closest
 * twin -- same five-state shape, same Hold-state-has-no-procs,
 * same Thrown/Dropped pair wired to `itMainCommonProcHop`/
 * `ProcReflector`/`itBatThrownProcHit` (this file's own name for the
 * pattern `itSwordThrownProcHit` set). One real difference: the decomp
 * keeps a SEPARATE `itBatThrownProcUpdate` alongside `itBatFallProcUpdate`
 * even though their bodies are identical (both call
 * `itMainApplyGravityClampTVel`/`itVisualsUpdateSpin` with the same
 * constants) -- `itsword.c` reused `itSwordFallProcUpdate` directly in
 * its own Thrown row instead. Ported as two distinct functions, matching
 * the decomp text exactly rather than collapsing them.
 *
 * `itBatThrownSetStatus`/`DroppedSetStatus` dereference `DObjGetStruct
 * (item_gobj)->child` unconditionally, same as Sword's own pair -- dead
 * code today (only reachable via
 * `itMainSetFighterThrow`/`Drop`), kept verbatim, host-tested with a
 * real stack child DObj.
 *
 * `itBatMakeItem` is ported now and is the decomp's line for line. Its
 * tail calls `ifCommonItemArrowMakeInterface(ip)`, using the exported
 * `IFCommonItem` file as an ordinary sprite bank, `romdisk/ifcommonitem.spr`,
 * because it is one sprite and nothing else (src/dc/ifcommon.c). The body
 * is verbatim.
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

/* it/itcommon/itbat.c: llITCommonDataBatItemAttributes -- reloc
 * stand-in, the same kind itstar.c/ittomato.c/itheart.c/itsword.c's own
 * `ITDesc` counterparts need (see itstar.c's file header
 * for the full reasoning). Only ever read as an offset from
 * gITManagerCommonData, which is NULL until
 * itManagerInitItems runs. */

/* it/itcommon/itbat.c:10-32 dITBatItemDesc, verbatim. */
ITDesc dITBatItemDesc =
{
    nITKindBat,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataBatItemAttributes,

    {
        nGCMatrixKindTraRotRpyR,
        nGCMatrixKindNull,
        0
    },

    nGMAttackStateOff,
    itBatFallProcUpdate,
    itBatFallProcMap,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/* it/itcommon/itbat.c:35-95 dITBatStatusDescs, verbatim. */
ITStatusDesc dITBatStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    {
        NULL,
        itBatWaitProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 1 (Air Wait Fall) */
    {
        itBatFallProcUpdate,
        itBatFallProcMap,
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
        itBatThrownProcUpdate,
        itBatThrownProcMap,
        itBatThrownProcHit,
        itBatThrownProcHit,
        itMainCommonProcHop,
        itBatThrownProcHit,
        itMainCommonProcReflector,
        NULL
    },

    /* Status 4 (Fighter Drop) */
    {
        itBatFallProcUpdate,
        itBatDroppedProcMap,
        itBatThrownProcHit,
        itBatThrownProcHit,
        itMainCommonProcHop,
        itBatThrownProcHit,
        itMainCommonProcReflector,
        NULL
    }
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itBatStatus
{
    nITBatStatusWait,
    nITBatStatusFall,
    nITBatStatusHold,
    nITBatStatusThrown,
    nITBatStatusDropped,
    nITBatStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/itbat.c:121-130 itBatFallProcUpdate 0x80174E30, verbatim. */
sb32 itBatFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITBAT_GRAVITY, ITBAT_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itbat.c:132-138 itBatWaitProcMap 0x80174E68, verbatim. */
sb32 itBatWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itBatFallSetStatus);

    return FALSE;
}

/* it/itcommon/itbat.c:140-146 itBatFallProcMap 0x80174E90, verbatim. */
sb32 itBatFallProcMap(GObj *item_gobj)
{
    itMapCheckDestroyDropped(item_gobj, ITBAT_MAP_REBOUND_COMMON, ITBAT_MAP_REBOUND_GROUND, itBatWaitSetStatus);

    return FALSE;
}

/* it/itcommon/itbat.c:148-153 itBatWaitSetStatus 0x80174EC4, verbatim. */
void itBatWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITBatStatusDescs, nITBatStatusWait);
}

/* it/itcommon/itbat.c:155-164 itBatFallSetStatus 0x80174EF8, verbatim. */
void itBatFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITBatStatusDescs, nITBatStatusFall);
}

/* it/itcommon/itbat.c:166-172 itBatHoldSetStatus 0x80174F3C, verbatim. */
void itBatHoldSetStatus(GObj *item_gobj)
{
    DObjGetStruct(item_gobj)->rotate.vec.f.y = F_CST_DTOR32(0.0F);

    itMainSetStatus(item_gobj, dITBatStatusDescs, nITBatStatusHold);
}

/* it/itcommon/itbat.c:174-181 itBatThrownProcUpdate 0x80174F70, verbatim.
 * Same body as itBatFallProcUpdate -- the decomp keeps this a separate
 * function rather than reusing FallProcUpdate the way itsword.c's own
 * Thrown row reused itSwordFallProcUpdate; ported as the decomp has it. */
sb32 itBatThrownProcUpdate(GObj *item_gobj)
{
    itMainApplyGravityClampTVel(itGetStruct(item_gobj), ITBAT_GRAVITY, ITBAT_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itbat.c:183-187 itBatThrownProcMap 0x80174FA8, verbatim. */
sb32 itBatThrownProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITBAT_MAP_REBOUND_COMMON, ITBAT_MAP_REBOUND_GROUND, itBatWaitSetStatus);
}

/* it/itcommon/itbat.c:189-199 itBatThrownProcHit 0x80174FD8, verbatim. */
sb32 itBatThrownProcHit(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->attack_coll.attack_state = nGMAttackStateOff;

    itMainVelSetRebound(item_gobj);

    return FALSE;
}

/* it/itcommon/itbat.c:201-207 itBatThrownSetStatus 0x80175000, verbatim.
 * Dead code today -- see the file header. */
void itBatThrownSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITBatStatusDescs, nITBatStatusThrown);

    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = F_CST_DTOR32(90.0F);
}

/* it/itcommon/itbat.c:209-213 itBatDroppedProcMap 0x80175044, verbatim. */
sb32 itBatDroppedProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITBAT_MAP_REBOUND_COMMON, ITBAT_MAP_REBOUND_GROUND, itBatWaitSetStatus);
}

/* it/itcommon/itbat.c:215-221 itBatDroppedSetStatus 0x80175074, verbatim.
 * Dead code today -- see the file header. */
void itBatDroppedSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITBatStatusDescs, nITBatStatusDropped);

    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = F_CST_DTOR32(90.0F);
}

/* it/itcommon/itbat.c:224-239 itBatMakeItem 0x801750B8, verbatim. */
GObj* itBatMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITBatItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        DObjGetStruct(item_gobj)->rotate.vec.f.y = F_CST_DTOR32(90.0F);

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}
