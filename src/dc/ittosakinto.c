/* ittosakinto.c -- it/itmonster/ittosakinto.c, verbatim: Goldeen's
 * `ITDesc`, its two-row `ITStatusDesc` table (appear / splash) and their
 * proc bodies. Function-for-function against the game's own ITStruct
 * (it/ittypes.h); every function names its decomp line range.
 *
 * The second monster. The shape is Mew's with one state added
 * and one thing Mew does not do:
 *
 *  - `nITTosakintoStatusAppear` is the fall out of the ball: gravity, and
 *    a proc_map that flips to Bounce the moment its collision mask sees a
 *    floor. `nITTosakintoStatusBounce` is the helpless flopping that made
 *    Goldeen the joke of the roster -- ITTOSAKINTO_LIFETIME frames of
 *    gravity with a FLAP_VEL_Y kick every time it lands, a coin-flip on the
 *    X velocity, and the splash sound. Returning TRUE at zero is the whole
 *    of its exit.
 *  - It is the first monster that ANIMATES. `itTosakintoBounceInitVars`
 *    hangs the AnimJoint on the CHILD joint and the MatAnimJoint on that
 *    child's MObj, which is what makes the tail flip; both are read out of
 *    the file with `itGetPData` and only when `ip->kind` is Goldeen's own
 *    (this same file's procs serve Saffron City's Goldeen).
 *  - `itMainClearOwnerStats` is called first thing in `MakeItem`: Goldeen
 *    is not a thrown item, so the thrower's stats must not follow it.
 *
 * DIVERGES, the one every item file has: `dITTosakintoItemDesc`'s third
 * field is `(intptr_t)llITCommonDataTosakintoItemAttributes` rather than
 * the decomp's `&...`, and the two `itGetPData`/`itGetMonsterAnimNode`
 * calls drop their `&` -- on the N64 those symbols' ADDRESSES are the
 * offsets. See src/dc/itemoffsets.h. No function body diverges.
 */
#include <it/item.h>

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

/* it/itmonster/ittosakinto.c:10-33 dITTosakintoItemDesc, verbatim. */
ITDesc dITTosakintoItemDesc =
{
    nITKindTosakinto,                       /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataTosakintoItemAttributes,    /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindNull,                  /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateOff,                      /* Hitbox Update State */
    itTosakintoCommonProcUpdate,            /* Proc Update */
    itTosakintoCommonProcMap,               /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itmonster/ittosakinto.c:35-61 dITTosakintoStatusDescs, verbatim. */
ITStatusDesc dITTosakintoStatusDescs[/* */] =
{
    /* Status 0 (Neutral Appear) */
    {
        itTosakintoAppearProcUpdate,        /* Proc Update */
        itTosakintoAppearProcMap,           /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 1 (Neutral Splash) */
    {
        itTosakintoBounceProcUpdate,        /* Proc Update */
        itTosakintoBounceProcMap,           /* Proc Map */
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

enum itTosakintoStatus
{
    nITTosakintoStatusAppear,
    nITTosakintoStatusBounce,
    nITTosakintoStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/ittosakinto.c:82-90 itTosakintoAppearProcUpdate 0x8017E7A0,
 * verbatim. */
sb32 itTosakintoAppearProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITTOSAKINTO_GRAVITY, ITTOSAKINTO_TVEL);

    return FALSE;
}

/* it/itmonster/ittosakinto.c:92-108 itTosakintoAppearProcMap 0x8017E7CC,
 * verbatim. `itMapTestAllCheckCollEnd` is the mask REFRESH;
 * `ip->coll_data.mask_curr` is read after it, not instead of it. */
sb32 itTosakintoAppearProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMapTestAllCheckCollEnd(item_gobj);

    if (ip->coll_data.mask_curr & MAP_FLAG_FLOOR)
    {
        ip->physics.vel_air.y = ITTOSAKINTO_FLAP_VEL_Y;

        itTosakintoBounceSetStatus(item_gobj);

        func_800269C0_275C0(nSYAudioFGMTosakintoSplash);
    }
    return FALSE;
}

/* it/itmonster/ittosakinto.c:110-122 itTosakintoAppearSetStatus 0x8017E828,
 * verbatim. The cry is gated on Goldeen's own kind for the same reason
 * Mew's is. */
void itTosakintoAppearSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi = ITTOSAKINTO_LIFETIME;

    if (ip->kind == nITKindTosakinto)
    {
        func_800269C0_275C0(nSYAudioVoiceMBallTosakintoAppear);
    }
    itMainSetStatus(item_gobj, dITTosakintoStatusDescs, nITTosakintoStatusAppear);
}

/* it/itmonster/ittosakinto.c:124-138 itTosakintoBounceProcUpdate
 * 0x8017E880, verbatim. */
sb32 itTosakintoBounceProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITTOSAKINTO_GRAVITY, ITTOSAKINTO_TVEL);

    if (ip->multi == 0)
    {
        return TRUE;
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/ittosakinto.c:140-158 itTosakintoBounceProcMap 0x8017E8CC,
 * verbatim. The un-flipped `-ip->physics.vel_air.x` when the coin says so
 * is the whole of Goldeen's steering. */
sb32 itTosakintoBounceProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMapTestAllCheckCollEnd(item_gobj);

    if (ip->coll_data.mask_curr & MAP_FLAG_FLOOR)
    {
        ip->physics.vel_air.y = ITTOSAKINTO_FLAP_VEL_Y;

        if (syUtilsRandIntRange(2) != 0)
        {
            ip->physics.vel_air.x = -ip->physics.vel_air.x;
        }
        func_800269C0_275C0(nSYAudioFGMTosakintoSplash);
    }
    return FALSE;
}

/* it/itmonster/ittosakinto.c:160-186 itTosakintoBounceInitVars 0x8017E93C,
 * verbatim. The anim pair is read out of the file through `itGetPData`,
 * whose two offsets are both in ITCommonObject -- so the arithmetic cancels
 * to a fixed distance past `attr->data` (src/dc/itemoffsets.h). Both hang
 * on the CHILD joint: `dobj->child` is the one the desc's transform left
 * free for the tail. `gcPlayAnimAll` is what applies them before the first
 * frame is drawn. */
void itTosakintoBounceInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    void *anim_joint;
    void *matanim_joint;
    s32 unused;

    ip->item_vars.tosakinto.pos = dobj->translate.vec.f;

    ip->physics.vel_air.y = ITTOSAKINTO_FLAP_VEL_Y;
    ip->physics.vel_air.x = ITTOSAKINTO_FLAP_VEL_X;

    if (ip->kind == nITKindTosakinto)
    {
        anim_joint = itGetPData(ip, llITCommonDataTosakintoDataStart, llITCommonDataTosakintoAnimJoint);

        gcAddDObjAnimJoint(dobj->child, anim_joint, 0.0F);

        matanim_joint = itGetPData(ip, llITCommonDataTosakintoDataStart, llITCommonDataTosakintoMatAnimJoint);

        gcAddMObjMatAnimJoint(dobj->child->mobj, matanim_joint, 0.0F);

        gcPlayAnimAll(item_gobj);
    }
}

/* it/itmonster/ittosakinto.c:188-193 itTosakintoBounceSetStatus 0x8017EA14,
 * verbatim. */
void itTosakintoBounceSetStatus(GObj *item_gobj)
{
    itTosakintoBounceInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITTosakintoStatusDescs, nITTosakintoStatusBounce);
}

/* it/itmonster/ittosakinto.c:195-209 itTosakintoCommonProcUpdate
 * 0x8017EA48, verbatim. */
sb32 itTosakintoCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        ip->physics.vel_air.y = 0.0F;

        itTosakintoAppearSetStatus(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/ittosakinto.c:211-221 itTosakintoCommonProcMap 0x8017EA98,
 * verbatim. */
sb32 itTosakintoCommonProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR) != FALSE)
    {
        ip->physics.vel_air.y = 0.0F;
    }
    return FALSE;
}

/* it/itmonster/ittosakinto.c:223-253 itTosakintoMakeItem 0x8017EAD8,
 * verbatim. Two XObjs before the model: the transform pair the anim needs.
 * Unlike Mew's, this one never sets the child's translation back to `pos`
 * -- `itMainClearOwnerStats` first is what tells the two apart. */
GObj* itTosakintoMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITTosakintoItemDesc, pos, vel, flags);
    DObj *dobj;
    ITStruct *ip;

    if (item_gobj != NULL)
    {
        itMainClearOwnerStats(item_gobj);

        dobj = DObjGetStruct(item_gobj);

        gcAddXObjForDObjFixed(dobj, nGCMatrixKindTraRotRpyR, 0);
        gcAddXObjForDObjFixed(dobj, 0x48, 0);

        dobj->translate.vec.f = *pos;

        ip = itGetStruct(item_gobj);

        ip->multi = ITMONSTER_RISE_STOP_WAIT;

        ip->physics.vel_air.x = ip->physics.vel_air.z = 0.0F;
        ip->physics.vel_air.y = ITMONSTER_RISE_VEL_Y;

        dobj->translate.vec.f.y -= ip->attr->map_coll_bottom;

        gcAddDObjAnimJoint(dobj, itGetMonsterAnimNode(ip, llITCommonDataTosakintoDataStart), 0.0F);
    }
    return item_gobj;
}
