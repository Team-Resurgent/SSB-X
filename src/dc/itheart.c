/* itheart.c -- it/itcommon/itheart.c, verbatim except one function left
 * out (see below): the Heart's `ITDesc`, its three-state `ITStatusDesc`
 * table (ground wait / air fall / fighter drop), and their proc bodies.
 * Function-for-function against the game's own ITStruct (it/ittypes.h);
 * every function names its decomp line range.
 *
 * The roster's third entry, nearly line-for-line identical
 * to `it/itcommon/ittomato.c` -- same shape, same dependencies,
 * same blocker, just Heart's own kind/constants/DObj tail (one extra
 * line, `dobj->rotate.vec.f.z = 0.0F;`, that ittomato.c's MakeItem
 * doesn't have). Seven of the eight functions port verbatim, touching
 * only the ITStruct and leaves already in the build: `itMainApplyGravity-
 * ClampTVel`/`itVisualsUpdateSpin`, `itMapCheckLRWallProcNo-
 * Floor`/`CheckDestroyDropped`/`SetAir`/`SetGround`,
 * `itMainSetStatus`, `itMainSetGroundAllowPickup`.
 *
 * `itHeartMakeItem` is the decomp's line for line. Its tail calls
 * `ifCommonItemArrowMakeInterface(ip)`, which reads the `IFCommonItem`
 * file: an ordinary sprite bank, `romdisk/ifcommonitem.spr`, because it
 * is one sprite and nothing else (src/dc/ifcommon.c).
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

/* it/itcommon/itheart.c: llITCommonDataHeartItemAttributes -- reloc
 * stand-in, the same kind itstar.c/ittomato.c's own `ITDesc` counterparts
 * needed (see itstar.c's file header for the full
 * reasoning). Only ever read as an offset from gITManagerCommonData,
 * which itManagerInitItems fills in from the item pack. */

/* it/itcommon/itheart.c:10-30 dITHeartItemDesc, verbatim. */
ITDesc dITHeartItemDesc =
{
    nITKindHeart,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataHeartItemAttributes,

    {
        nGCMatrixKindTraRotRpyR,
        nGCMatrixKindNull,
        0
    },

    nGMAttackStateOff,
    itHeartFallProcUpdate,
    itHeartFallProcMap,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/* it/itcommon/itheart.c:32-69 dITHeartStatusDescs, verbatim. */
ITStatusDesc dITHeartStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    {
        NULL,
        itHeartWaitProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 1 (Air Wait Fall) */
    {
        itHeartFallProcUpdate,
        itHeartFallProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 2 (Fighter Drop) */
    {
        itHeartFallProcUpdate,
        itHeartDroppedProcMap,
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

enum itHeartStatus
{
    nITHeartStatusWait,
    nITHeartStatusFall,
    nITHeartStatusDropped,
    nITHeartStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/itheart.c:89-97 itHeartFallProcUpdate 0x801746F0, verbatim. */
sb32 itHeartFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITHEART_GRAVITY, ITHEART_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itheart.c:99-103 itHeartWaitProcMap 0x80174728, verbatim. */
sb32 itHeartWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itHeartFallSetStatus);

    return FALSE;
}

/* it/itcommon/itheart.c:105-108 itHeartFallProcMap 0x80174750, verbatim. */
sb32 itHeartFallProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITHEART_MAP_REBOUND_COMMON, ITHEART_MAP_REBOUND_GROUND, itHeartWaitSetStatus);
}

/* it/itcommon/itheart.c:110-114 itHeartWaitSetStatus 0x80174780, verbatim. */
void itHeartWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITHeartStatusDescs, nITHeartStatusWait);
}

/* it/itcommon/itheart.c:116-124 itHeartFallSetStatus 0x801747B4, verbatim. */
void itHeartFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITHeartStatusDescs, nITHeartStatusFall);
}

/* it/itcommon/itheart.c:126-129 itHeartDroppedProcMap 0x801747F8, verbatim. */
sb32 itHeartDroppedProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITHEART_MAP_REBOUND_COMMON, ITHEART_MAP_REBOUND_GROUND, itHeartWaitSetStatus);
}

/* it/itcommon/itheart.c:131-134 itHeartDroppedSetStatus 0x80174828,
 * verbatim. Dead code in the decomp itself, same as ittomato.c's own
 * DroppedSetStatus -- see the file header. */
void itHeartDroppedSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITHeartStatusDescs, nITHeartStatusDropped);
}

/* it/itcommon/itheart.c:149-184 itHeartMakeItem 0x80174850, verbatim. */
GObj* itHeartMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITHeartItemDesc, pos, vel, flags);
    DObj *dobj;
#if defined(REGION_US)
    Vec3f translate;
#endif
    ITStruct *ip;

    if (item_gobj != NULL)
    {
#if defined(REGION_US)
        dobj = DObjGetStruct(item_gobj);
        ip = itGetStruct(item_gobj);
        translate = dobj->translate.vec.f;

        gcAddXObjForDObjFixed(dobj, 0x2E, 0);

        dobj->translate.vec.f = translate;
#else
        ip = itGetStruct(item_gobj);
        dobj = DObjGetStruct(item_gobj);

        gcAddXObjForDObjFixed(dobj, 0x2E, 0);

        dobj->translate.vec.f = *pos;
#endif

        dobj->rotate.vec.f.z = 0.0F;

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}
