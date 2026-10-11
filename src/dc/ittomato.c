/* ittomato.c -- it/itcommon/ittomato.c, verbatim except one function left
 * out (see below): the Tomato's `ITDesc`, its three-state `ITStatusDesc`
 * table (ground wait / air fall / fighter drop), and their proc bodies.
 * Function-for-function against the game's own ITStruct (it/ittypes.h);
 * every function names its decomp line range.
 *
 * The roster's second entry. Seven of the eight functions
 * port verbatim: `itTomatoFallProcUpdate`/`WaitProcMap`/`FallProcMap`/
 * `DroppedProcMap` touch only the ITStruct and already-ported it/itmap.c
 * leaves (`itMapCheckLRWallProcNoFloor`/`CheckDestroyDropped`/`SetAir`,
 * all already ported); `WaitSetStatus`/`FallSetStatus`/`DroppedSetStatus`
 * dispatch through `itMainSetStatus` and
 * `itMainSetGroundAllowPickup` (`src/dc/itmain.c`), which needs only
 * `itMapSetGround`, not the ftparam.c functions or roster tables its
 * neighbours (SetFighterRelease/Drop/Throw/Hold) depend on.
 * `itTomatoDroppedSetStatus` is
 * dead code in the decomp itself (nothing in this file calls it -- no
 * `ITStatusDesc` row names it, no Map function transitions to it) but is
 * kept, same "byte-for-byte, even dead code" doctrine as itmap.c's
 * `func_ovl3_80173E9C`.
 *
 * `itTomatoMakeItem` is ported now and is the decomp's line for line. Its
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

/* it/itcommon/ittomato.c: llITCommonDataTomatoItemAttributes -- reloc
 * stand-in, the same kind ittomato.c's own dITStarItemDesc counterpart
 * needs (see itstar.c's file header for the full reasoning).
 * Only ever read as an offset from gITManagerCommonData, which
 * itManagerInitItems fills in from the item pack. */

/* it/itcommon/ittomato.c:10-30 dITTomatoItemDesc, verbatim. */
ITDesc dITTomatoItemDesc =
{
    nITKindTomato,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataTomatoItemAttributes,

    {
        nGCMatrixKindTraRotRpyR,
        nGCMatrixKindNull,
        0
    },

    nGMAttackStateOff,
    itTomatoFallProcUpdate,
    itTomatoFallProcMap,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/* it/itcommon/ittomato.c:32-69 dITTomatoStatusDescs, verbatim. */
ITStatusDesc dITTomatoStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    {
        NULL,
        itTomatoWaitProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 1 (Air Wait Fall) */
    {
        itTomatoFallProcUpdate,
        itTomatoFallProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 2 (Fighter Drop) */
    {
        itTomatoFallProcUpdate,
        itTomatoDroppedProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    }
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itTomatoStatus
{
    nITTomatoStatusWait,
    nITTomatoStatusFall,
    nITTomatoStatusDropped,
    nITTomatoStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/ittomato.c:89-97 itTomatoFallProcUpdate 0x801744C0, verbatim. */
sb32 itTomatoFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITTOMATO_GRAVITY, ITTOMATO_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/ittomato.c:99-103 itTomatoWaitProcMap 0x801744FC, verbatim. */
sb32 itTomatoWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itTomatoFallSetStatus);

    return FALSE;
}

/* it/itcommon/ittomato.c:105-108 itTomatoFallProcMap 0x80174524, verbatim. */
sb32 itTomatoFallProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITTOMATO_MAP_REBOUND_COMMON, ITTOMATO_MAP_REBOUND_GROUND, itTomatoWaitSetStatus);
}

/* it/itcommon/ittomato.c:110-114 itTomatoWaitSetStatus 0x80174554, verbatim. */
void itTomatoWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITTomatoStatusDescs, nITTomatoStatusWait);
}

/* it/itcommon/ittomato.c:116-124 itTomatoFallSetStatus 0x80174588, verbatim. */
void itTomatoFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITTomatoStatusDescs, nITTomatoStatusFall);
}

/* it/itcommon/ittomato.c:126-129 itTomatoDroppedProcMap 0x801745CC, verbatim. */
sb32 itTomatoDroppedProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITTOMATO_MAP_REBOUND_COMMON, ITTOMATO_MAP_REBOUND_GROUND, itTomatoWaitSetStatus);
}

/* it/itcommon/ittomato.c:131-134 itTomatoDroppedSetStatus 0x801745FC,
 * verbatim. Dead code in the decomp itself -- see the file header. */
void itTomatoDroppedSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITTomatoStatusDescs, nITTomatoStatusDropped);
}

/* it/itcommon/ittomato.c:149-182 itTomatoMakeItem 0x80174624, verbatim. */
GObj* itTomatoMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITTomatoItemDesc, pos, vel, flags);
    DObj *joint;
#if defined(REGION_US)
    Vec3f translate;
#endif
    ITStruct *ip;

    if (item_gobj != NULL)
    {
#if defined(REGION_US)
        joint = DObjGetStruct(item_gobj);
        ip = itGetStruct(item_gobj);
        translate = joint->translate.vec.f;

        gcAddXObjForDObjFixed(joint, 0x2E, 0);

        joint->translate.vec.f = translate;
#else
        ip = itGetStruct(item_gobj);
        joint = DObjGetStruct(item_gobj);

        gcAddXObjForDObjFixed(joint, 0x2E, 0);

        joint->translate.vec.f = *pos;
#endif

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}
