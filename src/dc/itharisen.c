/* itharisen.c -- it/itcommon/itharisen.c, verbatim except two functions
 * left out (see below): the Fan's `ITDesc`, its five-state
 * `ITStatusDesc` table (ground wait / air fall / fighter hold / fighter
 * throw / fighter drop), and their proc bodies. Function-for-function
 * against the game's own ITStruct (it/ittypes.h); every function names
 * its decomp line range.
 *
 * The roster's seventh entry -- read fresh. Close to Sword/Bat's five-state
 * shape (Thrown/ Dropped wired to `itMainCommonProcHop`/`ProcReflector`, like
 * Sword's own, unlike Hammer's), but with two genuinely new things:
 *
 * - `itHarisenHoldSetStatus` calls `gcAddXObjForDObjFixed` (sys/objman.c,
 *   already in the build) to add a
 *   scale-only XObj on pickup -- the first roster item whose Hold
 *   transition does more than reset rotate.y.
 * - `func_ovl3_80175408` (0x80175408), the decomp's own "Unused" marker,
 *   reads `llITCommonDataHarisenDataStart` -- a symbol that appears
 *   NOWHERE else in the pinned decomp (not in `itharisen.h`'s extern
 *   list, not defined in any .c file): it is one of the N64 build's own
 *   per-relocData-file linker-generated boundary symbols (the reloc
 *   toolchain's convention for "the start of this file's data section"),
 *   which this port's ordinary-global reloc-stand-in technique has no
 *   counterpart for -- unlike `llITCommonDataHarisenItemAttributes`
 *   (used by every roster item's own `ITDesc`), this one is never
 *   assigned a stand-in anywhere in the decomp tree for this port to
 *   reuse. Genuinely unresolvable without inventing a symbol the decomp
 *   itself never named, so THIS ONE dead function is left out -- not
 *   the whole file, just the one function whose only caller was already
 *   nobody (the decomp's own "Unused" comment) and whose only dependency
 *   this port cannot honestly stand in for. `dITHarisenAnimJoint[]`,
 *   that function's own data table, is kept -- it has no such
 *   dependency itself, only its one dead reader does.
 *
 * `itHarisenMakeItem` is ported now, and it is the decomp's line for
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

/* it/itcommon/itharisen.c: llITCommonDataHarisenItemAttributes -- reloc
 * stand-in, the same kind every other roster item's own `ITDesc`
 * counterpart needs (see itstar.c's file header for the
 * full reasoning). Only ever read as an offset from
 * gITManagerCommonData, which is NULL until
 * itManagerInitItems runs. */

/* it/itcommon/itharisen.c:10-13 dITHarisenAnimJoint, verbatim. Dead data
 * -- its one reader, func_ovl3_80175408, is left out below (see the file
 * header); kept anyway since the array itself has no unresolvable
 * dependency, same "byte-for-byte, even dead code" doctrine everything
 * else dead in this port follows. */
intptr_t dITHarisenAnimJoint[/* */] =
{
    0x2250, 0x2270
};

/* it/itcommon/itharisen.c:16-38 dITHarisenItemDesc, verbatim. */
ITDesc dITHarisenItemDesc =
{
    nITKindHarisen,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataHarisenItemAttributes,

    {
        nGCMatrixKindTraRotRpyRSca,
        nGCMatrixKindNull,
        0
    },

    nGMAttackStateOff,
    itHarisenFallProcUpdate,
    itHarisenFallProcMap,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/* it/itcommon/itharisen.c:40-100 dITHarisenStatusDescs, verbatim. */
ITStatusDesc dITHarisenStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    {
        NULL,
        itHarisenWaitProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 1 (Air Wait Fall) */
    {
        itHarisenFallProcUpdate,
        itHarisenFallProcMap,
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
        itHarisenThrownProcUpdate,
        itHarisenThrownProcMap,
        itHarisenCommonProcHit,
        itHarisenCommonProcHit,
        itMainCommonProcHop,
        itHarisenCommonProcHit,
        itMainCommonProcReflector,
        NULL
    },

    /* Status 4 (Fighter Drop) */
    {
        itHarisenFallProcUpdate,
        itHarisenDroppedProcMap,
        itHarisenCommonProcHit,
        itHarisenCommonProcHit,
        itMainCommonProcHop,
        itHarisenCommonProcHit,
        itMainCommonProcReflector,
        NULL
    }
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itHarisenStatus
{
    nITHarisenStatusWait,
    nITHarisenStatusFall,
    nITHarisenStatusHold,
    nITHarisenStatusThrown,
    nITHarisenStatusDropped,
    nITHarisenStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/itharisen.c:125-131 itHarisenCommonSetScale 0x80175140,
 * verbatim. */
void itHarisenCommonSetScale(GObj *item_gobj, f32 scale)
{
    DObjGetStruct(item_gobj)->scale.vec.f.x = scale;
    DObjGetStruct(item_gobj)->scale.vec.f.y = scale;
    DObjGetStruct(item_gobj)->scale.vec.f.z = scale;
}

/* it/itcommon/itharisen.c:133-142 itHarisenFallProcUpdate 0x80175160,
 * verbatim. */
sb32 itHarisenFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITHARISEN_GRAVITY, ITHARISEN_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itharisen.c:144-150 itHarisenWaitProcMap 0x80175198,
 * verbatim. */
sb32 itHarisenWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itHarisenFallSetStatus);

    return FALSE;
}

/* it/itcommon/itharisen.c:152-158 itHarisenFallProcMap 0x801751C0,
 * verbatim. */
sb32 itHarisenFallProcMap(GObj *item_gobj)
{
    itMapCheckDestroyDropped(item_gobj, ITHARISEN_MAP_REBOUND_COMMON, ITHARISEN_MAP_REBOUND_GROUND, itHarisenWaitSetStatus);

    return FALSE;
}

/* it/itcommon/itharisen.c:160-165 itHarisenWaitSetStatus 0x801751F4,
 * verbatim. */
void itHarisenWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITHarisenStatusDescs, nITHarisenStatusWait);
}

/* it/itcommon/itharisen.c:167-176 itHarisenFallSetStatus 0x80175228,
 * verbatim. */
void itHarisenFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITHarisenStatusDescs, nITHarisenStatusFall);
}

/* it/itcommon/itharisen.c:178-188 itHarisenHoldSetStatus 0x8017526C,
 * verbatim. First roster item whose Hold transition does more than
 * reset rotate.y -- adds a scale-only XObj via gcAddXObjForDObjFixed
 * (already ported). */
void itHarisenHoldSetStatus(GObj *item_gobj)
{
    DObj *dobj = DObjGetStruct(item_gobj);

    gcAddXObjForDObjFixed(dobj, nGCMatrixKindSca, 0);

    dobj->rotate.vec.f.y = 0.0F;

    itMainSetStatus(item_gobj, dITHarisenStatusDescs, nITHarisenStatusHold);
}

/* it/itcommon/itharisen.c:190-199 itHarisenThrownProcUpdate 0x801752C0,
 * verbatim. Same body as itHarisenFallProcUpdate -- kept as its own
 * function, same shape itBatThrownProcUpdate/itHammerThrownProcUpdate
 * take. */
sb32 itHarisenThrownProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITHARISEN_GRAVITY, ITHARISEN_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itharisen.c:201-205 itHarisenThrownProcMap 0x801752F8,
 * verbatim. */
sb32 itHarisenThrownProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITHARISEN_MAP_REBOUND_COMMON, ITHARISEN_MAP_REBOUND_GROUND, itHarisenWaitSetStatus);
}

/* it/itcommon/itharisen.c:207-217 itHarisenCommonProcHit 0x80175328,
 * verbatim. */
sb32 itHarisenCommonProcHit(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->attack_coll.attack_state = nGMAttackStateOff;

    itMainVelSetRebound(item_gobj);

    return FALSE;
}

/* it/itcommon/itharisen.c:219-225 itHarisenThrownSetStatus 0x80175350,
 * verbatim. Dead code today, same status as every other roster item's
 * own -- only reachable via itMainSetFighterThrow/
 * Drop. */
void itHarisenThrownSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITHarisenStatusDescs, nITHarisenStatusThrown);

    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = F_CST_DTOR32(-90.0F);
}

/* it/itcommon/itharisen.c:227-231 itHarisenDroppedProcMap 0x80175394,
 * verbatim. */
sb32 itHarisenDroppedProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITHARISEN_MAP_REBOUND_COMMON, ITHARISEN_MAP_REBOUND_GROUND, itHarisenWaitSetStatus);
}

/* it/itcommon/itharisen.c:233-239 itHarisenDroppedSetStatus 0x801753C4,
 * verbatim. Dead code today, same caveat as ThrownSetStatus above. */
void itHarisenDroppedSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITHarisenStatusDescs, nITHarisenStatusDropped);

    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = F_CST_DTOR32(-90.0F);
}

/* it/itcommon/itharisen.c:251-266 itHarisenMakeItem 0x80175460, verbatim. */
GObj* itHarisenMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITHarisenItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        DObjGetStruct(item_gobj)->rotate.vec.f.y = F_CST_DTOR32(-90.0F);

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}
