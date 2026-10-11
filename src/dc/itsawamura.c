/* itsawamura.c -- it/itmonster/itsawamura.c, verbatim: Hitmonlee's
 * `ITDesc`, its three-row `ITStatusDesc` table (fall / wait / kick) and
 * their proc bodies. Function-for-function against the game's own ITStruct
 * (it/ittypes.h); every function names its decomp line range.
 *
 * The third monster, and the first with an ATTACK. Its shape
 * is Goldeen's rise-out-of-the-ball with a real state machine under it:
 *
 *  - `nITSawamuraStatusWait` is ITSAWAMURA_KICK_WAIT (40) frames on the
 *    floor, and `itSawamuraWaitProcMap` bounces back to Fall off a left or
 *    right wall.
 *  - `nITSawamuraStatusAttack` is the kick itself. `itSawamuraAttackInitVars`
 *    walks the FIGHTER link and picks the nearest one that is not its own
 *    owner and is not on its team -- the `players` counter and the
 *    `dist_xy` running minimum are the decomp's own, including the odd
 *    first-iteration special case that stores `dist_xy` for `players == 0`
 *    before the counter is bumped. It then hands the chosen GObj to
 *    `itSawamuraAttackSetFollowPlayerLR`, which aims the kick at it.
 *  - The kick's own exit is three-way: `ip->multi` running out, or reaching
 *    ITSAWAMURA_DESPAWN_OFF_X from the map bound on the side it faces.
 *  - `itSawamuraMakeItem` is the only one of the three so far that moves
 *    the item's display list head (`gcMoveGObjDLHead`) -- Hitmonlee draws
 *    at priority 18.
 *
 * The decomp's two REGION_US arms are both kept. `itSawamuraAttackInitVars`
 * is the one place in the thirteen where they differ in more than variable
 * order: the US build compares `fp->team` against the item's OWN team, and
 * the other reads the owner's team out of `ip->owner_gobj`. Both are
 * written here, in the decomp's order.
 *
 * DIVERGES, the one every item file has: `dITSawamuraItemDesc`'s third
 * field is a number rather than the decomp's `&llITCommonData...` symbol,
 * and the `itGetPData`/`itGetMonsterAnimNode` calls drop their `&`. See
 * src/dc/itemoffsets.h. No function body diverges.
 */
#include <it/item.h>
#include <ft/fighter.h>
#include <gr/ground.h>

#include "itmonster.h"          /* itGetMonsterAnimNode */
#include "itemoffsets.h"        /* llITCommonData* offsets */
#include "itemmodel.h"          /* itemModelSetDisplayList */

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

/* it/itmonster/itsawamura.c:11-34 dITSawamuraItemDesc, verbatim. */
ITDesc dITSawamuraItemDesc =
{
    nITKindSawamura,                        /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataSawamuraItemAttributes, /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,            /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itSawamuraCommonProcUpdate,             /* Proc Update */
    itSawamuraCommonProcMap,                /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itmonster/itsawamura.c:36-80 dITSawamuraStatusDescs, verbatim. Rows 0
 * and 1 carry the decomp's own identical status comment; they are not the
 * same row -- Fall has gravity and Wait does not. */
ITStatusDesc dITSawamuraStatusDescs[/* */] =
{
    /* Status 0 (Air Fall) */
    {
        itSawamuraFallProcUpdate,           /* Proc Update */
        itSawamuraFallProcMap,              /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 1 (Air Fall) */
    {
        itSawamuraWaitProcUpdate,           /* Proc Update */
        itSawamuraWaitProcMap,              /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 2 (Neutral Attack) */
    {
        itSawamuraAttackProcUpdate,         /* Proc Update */
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

enum itSawamuraStatus
{
    nITSawamuraStatusFall,
    nITSawamuraStatusWait,
    nITSawamuraStatusAttack,
    nITSawamuraStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itsawamura.c:88-96 itSawamuraFallProcUpdate 0x80182630,
 * verbatim. */
sb32 itSawamuraFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITSAWAMURA_GRAVITY, ITSAWAMURA_TVEL);

    return FALSE;
}

/* it/itmonster/itsawamura.c:98-112 itSawamuraFallProcMap 0x80182660,
 * verbatim. */
sb32 itSawamuraFallProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR) != FALSE)
    {
        ip->physics.vel_air.y = 0.0F;

        itSawamuraWaitSetStatus(item_gobj);
    }
    return FALSE;
}

/* it/itmonster/itsawamura.c:114-118 itSawamuraFallSetStatus 0x801826A8,
 * verbatim. */
void itSawamuraFallSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITSawamuraStatusDescs, nITSawamuraStatusFall);
}

/* it/itmonster/itsawamura.c:120-134 itSawamuraWaitProcUpdate 0x801826D0,
 * verbatim. */
sb32 itSawamuraWaitProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        itSawamuraAttackSetStatus(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itsawamura.c:136-146 itSawamuraWaitProcMap 0x80182714,
 * verbatim. */
sb32 itSawamuraWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itSawamuraFallSetStatus);

    return FALSE;
}

/* it/itmonster/itsawamura.c:148-152 itSawamuraWaitSetStatus 0x8018273C,
 * verbatim. */
void itSawamuraWaitSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITSawamuraStatusDescs, nITSawamuraStatusWait);
}

/* it/itmonster/itsawamura.c:154-183 itSawamuraAttackProcUpdate
 * 0x80182764, verbatim. Note the despawn test runs BEFORE the lifetime
 * test, so a kick that leaves the stage dies on the frame it crosses the
 * bound rather than waiting out its 600. */
sb32 itSawamuraAttackProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITSAWAMURA_GRAVITY, ITSAWAMURA_TVEL);

    if ((ip->lr == +1) && (dobj->translate.vec.f.x >= (gMPCollisionGroundData->map_bound_right - ITSAWAMURA_DESPAWN_OFF_X)))
    {
        return TRUE;
    }
    else if ((ip->lr == -1) && (dobj->translate.vec.f.x <= (gMPCollisionGroundData->map_bound_left + ITSAWAMURA_DESPAWN_OFF_X)))
    {
        return TRUE;
    }
    else if (ip->multi == 0)
    {
        return TRUE;
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itsawamura.c:185-212 itSawamuraAttackSetFollowPlayerLR
 * 0x8018285C, verbatim. The `lr == +1` arm turns the model 180 degrees,
 * and the -1 arm leaves it alone -- so the animation faces one way and the
 * model's own forward is the other. */
void itSawamuraAttackSetFollowPlayerLR(GObj *item_gobj, GObj *fighter_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    FTStruct *fp = ftGetStruct(fighter_gobj);
    DObj *ij = DObjGetStruct(item_gobj);
    DObj *fj = DObjGetStruct(fighter_gobj);
    s32 unused;
    Vec3f dist;
    Vec3f target_pos;

    target_pos = fj->translate.vec.f;

    target_pos.y += ITSAWAMURA_TARGET_POS_OFF_Y - fp->coll_data.map_coll.bottom;

    syVectorDiff3D(&dist, &target_pos, &ij->translate.vec.f);

    ip->physics.vel_air.y = ip->physics.vel_air.z = 0.0F;
    ip->physics.vel_air.x = ITSAWAMURA_KICK_VEL_X;

    syVectorRotate3D(&ip->physics.vel_air, SYVECTOR_AXIS_Z, syUtilsArcTan2(dist.y, dist.x));

    ip->lr = (dist.x < 0.0F) ? -1 : +1;

    if (ip->lr == +1)
    {
        ij->rotate.vec.f.y = F_CST_DTOR32(180.0F);
    }
}

/* it/itmonster/itsawamura.c:214-292 itSawamuraAttackInitVars 0x80182958,
 * verbatim, both REGION_US arms in the decomp's order. The broken-looking
 * first iteration is the decomp's: `players == 0` stores `dist_xy` before
 * `players++`, so the running minimum starts at the FIRST candidate's
 * distance and the comparison that follows is against itself. Kept as
 * written -- PORT not REMAKE. `victim_gobj` is the decomp's own
 * uninitialised local: a Poké Ball in a VS battle always has a fighter on
 * the link that is neither its owner nor its team, so the loop writes it
 * before the read below. It is kept uninitialised here rather than
 * defaulted, because a default would turn the one case it cannot cover --
 * an empty link -- into a silent no-op kick instead of the crash the
 * decomp would have. The target build says so on every compile
 * (`'victim_gobj' may be used uninitialized`), and that warning is the
 * honest state of this function rather than something to silence. */
void itSawamuraAttackInitVars(GObj *item_gobj)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
#if defined(REGION_US)
    ITStruct *ip = itGetStruct(item_gobj);
    GObj *victim_gobj;
    s32 unused2[3];
    DObj *dobj = DObjGetStruct(item_gobj);
    f32 square_xy;
    f32 dist_x;
    f32 dist_xy;
    Vec3f dist;
#else
    s32 unused1;
    GObj *victim_gobj;
    s32 unused2[2];
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    FTStruct *owner_fp = ftGetStruct(ip->owner_gobj);
    f32 square_xy;
    f32 dist_x;
    f32 dist_xy;
    Vec3f dist;
#endif
    s32 players = 0;

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);

#if defined(REGION_US)
        if ((fighter_gobj != ip->owner_gobj) && (fp->team != ip->team))
#else
        if ((fighter_gobj != ip->owner_gobj) && (fp->team != owner_fp->team))
#endif
        {
            syVectorDiff3D(&dist, &DObjGetStruct(fighter_gobj)->translate.vec.f, &dobj->translate.vec.f);

            if (players == 0)
            {
                dist_xy = SQUARE(dist.x) + SQUARE(dist.y);
            }
            players++;

            square_xy = SQUARE(dist.x) + SQUARE(dist.y);

            if (square_xy <= dist_xy)
            {
                dist_xy = square_xy;

                victim_gobj = fighter_gobj;
            }
        }
        fighter_gobj = fighter_gobj->link_next;

        continue;
    }
    itSawamuraAttackSetFollowPlayerLR(item_gobj, victim_gobj);

    if (ip->kind == nITKindSawamura)
    {
        Gfx *dl = (Gfx*) itGetPData(ip, llITCommonDataSawamuraDataStart, llITCommonDataSawamuraDisplayList);

        itemModelSetDisplayList(dobj, dl);    /* DIVERGES: itemmodel.h */

        func_800269C0_275C0(nSYAudioVoiceMBallSawamuraKick);
    }
    ip->multi = ITSAWAMURA_LIFETIME;

    ip->attack_coll.size = ITSAWAMURA_KICK_SIZE;
}

/* it/itmonster/itsawamura.c:294-298 itSawamuraAttackSetStatus 0x80182AAC,
 * verbatim. */
void itSawamuraAttackSetStatus(GObj *item_gobj)
{
    itSawamuraAttackInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITSawamuraStatusDescs, nITSawamuraStatusAttack);
}

/* it/itmonster/itsawamura.c:300-317 itSawamuraCommonProcUpdate 0x80182AE0,
 * verbatim. The `KICK_WAIT` write on the way out of the rise is what makes
 * Hitmonlee stand still for 40 frames before it kicks. */
sb32 itSawamuraCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        ip->multi = ITSAWAMURA_KICK_WAIT;

        ip->physics.vel_air.y = 0.0F;

        itSawamuraFallSetStatus(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itsawamura.c:319-329 itSawamuraCommonProcMap 0x80182B34,
 * verbatim. */
sb32 itSawamuraCommonProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR) != FALSE)
    {
        ip->physics.vel_air.y = 0.0F;
    }
    return FALSE;
}

/* it/itmonster/itsawamura.c:331-356 itSawamuraMakeItem 0x80182B74,
 * verbatim. The one XObj (0x48) and the display-list head move are
 * Hitmonlee's own; `gcMoveGObjDLHead(gobj, 18, ...)` is the same priority
 * its metronome display proc uses when Pippi rolls a Hitmonlee. */
GObj* itSawamuraMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITSawamuraItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        DObj *dobj = DObjGetStruct(item_gobj);
        ITStruct *ip = itGetStruct(item_gobj);

        ip->multi = ITMONSTER_RISE_STOP_WAIT;

        ip->physics.vel_air.x = ip->physics.vel_air.z = 0.0F;
        ip->physics.vel_air.y = ITMONSTER_RISE_VEL_Y;

        gcAddXObjForDObjFixed(dobj, 0x48, 0);

        dobj->translate.vec.f = *pos;

        dobj->translate.vec.f.y -= ip->attr->map_coll_bottom;

        gcAddDObjAnimJoint(dobj, itGetMonsterAnimNode(ip, llITCommonDataSawamuraDataStart), 0.0F);

        func_800269C0_275C0(nSYAudioVoiceMBallSawamuraAppear);

        gcMoveGObjDLHead(item_gobj, 18, item_gobj->dl_link_priority);
    }
    return item_gobj;
}
