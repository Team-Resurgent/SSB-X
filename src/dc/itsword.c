/* itsword.c -- it/itcommon/itsword.c, verbatim except one function left
 * out (see below): the Beam Sword's `ITDesc`, its five-state
 * `ITStatusDesc` table (ground wait / air fall / fighter hold / fighter
 * throw / fighter drop), and their proc bodies. Function-for-function
 * against the game's own ITStruct (it/ittypes.h); every function names
 * its decomp line range.
 *
 * The roster's fourth entry, and the first held/thrown item
 * -- unlike Tomato/Heart's plain fall/pickup cycle, the
 * Sword adds a Hold state (equipped, no update/map at all) and a Thrown/
 * Dropped pair that can hit fighters (`itSwordThrownProcHit`, wired to
 * `proc_hit`/`proc_shield`/`proc_setoff` alike) and reflect
 * (`itMainCommonProcReflector`) and hop (`itMainCommonProcHop`) off one.
 * Eleven of the twelve functions port verbatim: everything beyond the
 * Wait/Fall pair touches only the ITStruct,
 * its DObj, and leaves already in the build -- `itMainVelSetRebound`/
 * `CommonProcHop`/`CommonProcReflector`, `itMainSetStatus`/
 * `SetGroundAllowPickup`, `itMapCheckLRWallProcNoFloor`/
 * `CheckDestroyDropped`/`SetAir`.
 *
 * `itSwordThrownSetStatus`/`DroppedSetStatus` dereference `DObjGetStruct
 * (item_gobj)->child` unconditionally -- the blade DObj a real model pack
 * would attach as a child of the root. Neither is called from anywhere
 * in THIS file (both are dispatched externally, by `itMainSetFighterThrow`/
 * `Drop`, itself still blocked on two `ftparam.c` functions and the
 * roster tables -- see itmain.c's own header) or from anywhere else this
 * port currently builds, so this is dead code today, the same status
 * Tomato/Heart's own `DroppedSetStatus` carry -- kept verbatim regardless,
 * per that same doctrine, and host-tested with a real stack child DObj so
 * the test itself does not crash on the dereference.
 *
 * `itSwordMakeItem` is the decomp's line for line. Its tail calls
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

/* it/itcommon/itsword.c: llITCommonDataSwordItemAttributes -- reloc
 * stand-in, the same kind itstar.c/ittomato.c/itheart.c's own `ITDesc`
 * counterparts needed (see itstar.c's file header for the
 * full reasoning). Only ever read as an offset from gITManagerCommonData,
 * which itManagerInitItems fills in from the item pack. */

/* it/itcommon/itsword.c:10-32 dITSwordItemDesc, verbatim. */
ITDesc dITSwordItemDesc =
{
    nITKindSword,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataSwordItemAttributes,

    {
        nGCMatrixKindTraRotRpyRSca,
        nGCMatrixKindNull,
        0
    },

    nGMAttackStateOff,
    itSwordFallProcUpdate,
    itSwordFallProcMap,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/* it/itcommon/itsword.c:34-95 dITSwordStatusDescs, verbatim. */
ITStatusDesc dITSwordStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    {
        NULL,
        itSwordWaitProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 1 (Air Wait Fall) */
    {
        itSwordFallProcUpdate,
        itSwordFallProcMap,
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
        itSwordFallProcUpdate,
        itSwordThrownProcMap,
        itSwordThrownProcHit,
        itSwordThrownProcHit,
        itMainCommonProcHop,
        itSwordThrownProcHit,
        itMainCommonProcReflector,
        NULL
    },

    /* Status 4 (Fighter Drop) */
    {
        itSwordFallProcUpdate,
        itSwordDroppedProcMap,
        itSwordThrownProcHit,
        itSwordThrownProcHit,
        itMainCommonProcHop,
        itSwordThrownProcHit,
        itMainCommonProcReflector,
        NULL
    }
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itSwordStatus
{
    nITSwordStatusWait,
    nITSwordStatusFall,
    nITSwordStatusHold,
    nITSwordStatusThrown,
    nITSwordStatusDropped,
    nITSwordStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/itsword.c:120-128 itSwordFallProcUpdate 0x80174B50, verbatim. */
sb32 itSwordFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITSWORD_GRAVITY, ITSWORD_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itsword.c:131-136 itSwordWaitProcMap 0x80174B8C, verbatim. */
sb32 itSwordWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itSwordFallSetStatus);

    return FALSE;
}

/* it/itcommon/itsword.c:139-142 itSwordFallProcMap 0x80174BB4, verbatim. */
sb32 itSwordFallProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITSWORD_MAP_REBOUND_COMMON, ITSWORD_MAP_REBOUND_GROUND, itSwordWaitSetStatus);
}

/* it/itcommon/itsword.c:145-149 itSwordWaitSetStatus 0x80174BE4, verbatim. */
void itSwordWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITSwordStatusDescs, nITSwordStatusWait);
}

/* it/itcommon/itsword.c:152-160 itSwordFallSetStatus 0x80174C18, verbatim. */
void itSwordFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITSwordStatusDescs, nITSwordStatusFall);
}

/* it/itcommon/itsword.c:163-168 itSwordHoldSetStatus 0x80174C5C, verbatim. */
void itSwordHoldSetStatus(GObj *item_gobj)
{
    DObjGetStruct(item_gobj)->rotate.vec.f.y = F_CST_DTOR32(0.0F);

    itMainSetStatus(item_gobj, dITSwordStatusDescs, nITSwordStatusHold);
}

/* it/itcommon/itsword.c:171-174 itSwordThrownProcMap 0x80174C90, verbatim. */
sb32 itSwordThrownProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITSWORD_MAP_REBOUND_COMMON, ITSWORD_MAP_REBOUND_GROUND, itSwordWaitSetStatus);
}

/* it/itcommon/itsword.c:177-186 itSwordThrownProcHit 0x80174CC0, verbatim. */
sb32 itSwordThrownProcHit(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->attack_coll.attack_state = nGMAttackStateOff;

    itMainVelSetRebound(item_gobj);

    return FALSE;
}

/* it/itcommon/itsword.c:189-194 itSwordThrownSetStatus 0x80174CE8,
 * verbatim. Dead code today -- see the file header. */
void itSwordThrownSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITSwordStatusDescs, nITSwordStatusThrown);

    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = F_CST_DTOR32(90.0F);
}

/* it/itcommon/itsword.c:197-200 itSwordDroppedProcMap 0x80174D2C, verbatim. */
sb32 itSwordDroppedProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITSWORD_MAP_REBOUND_COMMON, ITSWORD_MAP_REBOUND_GROUND, itSwordWaitSetStatus);
}

/* it/itcommon/itsword.c:203-208 itSwordDroppedSetStatus 0x80174D5C,
 * verbatim. Dead code today -- see the file header. */
void itSwordDroppedSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITSwordStatusDescs, nITSwordStatusDropped);

    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = F_CST_DTOR32(90.0F);
}

/* it/itcommon/itsword.c:211-226 itSwordMakeItem 0x80174DA0, verbatim. */
GObj* itSwordMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITSwordItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        DObjGetStruct(item_gobj)->rotate.vec.f.y = F_CST_DTOR32(90.0F);

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}
