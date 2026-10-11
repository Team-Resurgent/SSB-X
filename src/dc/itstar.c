/* itstar.c -- it/itcommon/itstar.c, verbatim: Star Man's ITDesc and its
 * four proc bodies (update, map, hit, and the maker). Function-for-
 * function against the game's own ITStruct (it/ittypes.h); every
 * function names its decomp line range.
 *
 * Star Man is the smallest single item file (130 lines). itMainRefreshAttackColl
 * (it/itmain.c:228-237) lives in src/dc/itmain.c alongside it.
 *
 * dITStarItemDesc.p_file = &gITManagerCommonData needed that global
 * SYMBOL to exist, which src/dc/itmanager.c now defines (zero-initialised,
 * see its own header there) -- taking a static initialiser's address
 * needs no live value. o_attributes needs llITCommonDataStarItemAttributes,
 * which is a real offset into ITCommonData -- the number the decomp's own
 * description gives, written out in src/dc/itemoffsets.h.
 *
 * None of this is exercised by a live call path yet: itStarMakeItem's
 * only real-game caller is the still-blocked roster table, and its own
 * body dereferences *gITManagerCommonData the moment it runs -- filled
 * from the item pack by itManagerInitItems, which no scene
 * calls yet. The host
 * test below hand-installs a dummy ITAttributes at gITManagerCommonData
 * to exercise itStarMakeItem for real, the same shape stage_bind_collision
 * already uses to override src/dc/stage.c's MPGroundData stand-in for
 * collision tests.
 */
#include <it/item.h>
#include "itemoffsets.h"     /* llITCommonData* offsets */
#include <sc/scene.h>

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

/* it/itcommon/itstar.c: the offset the decomp names by symbol. On the N64
 * `llITCommonData*` are the reloc file's own boundary labels, so `&sym`
 * IS the offset; the port has no such labels and writes the number, which
 * is the value reloc_data.us.h records (see the file header).
 * Read as `file + this`, where `file` is gITManagerCommonData -- which
 * itManagerInitItems points at the item pack's region 0. */

/* it/itcommon/itstar.c:11-33 dITStarItemDesc, verbatim. */
ITDesc dITStarItemDesc =
{
    nITKindStar,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataStarItemAttributes,

    {
        nGCMatrixKindTraRotRpyR,
        nGCMatrixKindNull,
        0
    },

    nGMAttackStateOff,
    itStarCommonProcUpdate,
    itStarCommonProcMap,
    itStarCommonProcHit,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/itstar.c:41-57 itStarCommonProcUpdate 0x80174930, verbatim. */
sb32 itStarCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITSTAR_GRAVITY, ITSTAR_TVEL);

    ip->multi--;

    if (ip->multi == 0)
    {
        itMainRefreshAttackColl(item_gobj);
    }
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itstar.c:59-77 itStarCommonProcMap 0x80174990, verbatim. */
sb32 itStarCommonProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    sb32 is_collide_floor = itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR);

    if (itMapCheckCollideAllRebound(item_gobj, (MAP_FLAG_CEIL | MAP_FLAG_RWALL | MAP_FLAG_LWALL), ITSTAR_MAP_REBOUND_COMMON, NULL) != FALSE)
    {
        itMainSetSpinVelLR(item_gobj);
    }
    if (is_collide_floor != FALSE)
    {
        ip->physics.vel_air.y = ITSTAR_BOUNCE_Y;

        func_800269C0_275C0(nSYAudioFGMStarMapCollide);
    }
    return FALSE;
}

/* it/itcommon/itstar.c:79-83 itStarCommonProcHit 0x80174A0C, verbatim. */
sb32 itStarCommonProcHit(GObj *item_gobj)
{
    return TRUE;
}

/* it/itcommon/itstar.c:85-130 itStarMakeItem 0x80174A18, verbatim. */
GObj* itStarMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    CObj *cobj = CObjGetStruct(gGMCameraGObj);
    GObj *item_gobj;
    DObj *dobj;
    ITStruct *ip;
    Vec3f vel_real;
#if defined(REGION_US)
    Vec3f translate;
#endif

    vel_real.x = (pos->x < cobj->vec.at.x) ? ITSTAR_VEL_X : -ITSTAR_VEL_X;
    vel_real.y = ITSTAR_BOUNCE_Y;
    vel_real.z = 0.0F;

    item_gobj = itManagerMakeItem(parent_gobj, &dITStarItemDesc, pos, &vel_real, flags);

    if (item_gobj != NULL)
    {
        dobj = DObjGetStruct(item_gobj);

#if defined(REGION_US)
        translate = dobj->translate.vec.f;
#endif

        ip = itGetStruct(item_gobj);

        ip->attack_coll.interact_mask = GMHITCOLLISION_FLAG_FIGHTER; /* Star Man can only interact with fighters */

        ip->multi = ITSTAR_INTERACT_DELAY;

        ip->is_unused_item_bool = TRUE;

        gcAddXObjForDObjFixed(dobj, 0x2E, 0);

        dobj->rotate.vec.f.z = 0.0F;

#if defined(REGION_US)
        dobj->translate.vec.f = translate;
#else
        dobj->translate.vec.f = *pos;
#endif
    }
    return item_gobj;
}
