/* itmball.c -- it/itcommon/itmball.c, verbatim: the Poké Ball's `ITDesc`,
 * its seven-row `ITStatusDesc` table (ground wait / air fall / fighter hold
 * / fighter throw / fighter drop / ground open / air open) and their proc
 * bodies. Function-for-function against the game's own ITStruct
 * (it/ittypes.h); every function names its decomp line range.
 *
 * The last of the twenty kinds the item switch can pick.
 * The Poké Ball is the item that IS a gateway: its whole purpose is to hand
 * the match a monster, and everything it needed to do that arrived in the
 * the thirteen `it/itmonster/` files and
 * their table entries, the monster weapon models,
 * the MObj section the monsters' Mats hang on, and the rays it
 * opens with.
 *
 * THE TWO OPEN STATES are where that happens, and they differ only in
 * whether the ball is on the floor:
 *
 *  - `nITMBallStatusOpen` (ground): `itMBallOpenInitVars` zeroes the
 *    velocity, swaps which of the ball's two children is hidden, plays the
 *    opening sound, RUMBLES the throwing player's pad (8 frames at 20 Hz),
 *    makes the rays, clears the lid's MatAnimJoint -- and takes the ball's
 *    own floor line as `attach_line_id`, so the open animation stays put
 *    while `itMBallOpenProcMap` watches that line stop existing.
 *  - `nITMBallStatusOpenAir` is the same proc with `itMBallOpenAirProcMap`
 *    instead, which is the dropped-on-the-floor test.
 *  - Both then count `ip->multi` down from ITMBALL_SPAWN_WAIT (30), and on
 *    zero call `itMainMakeMonster` -- the ball is spent, and the monster
 *    takes its owner, team, player and display mode with it.
 *
 * `dITManagerForceMonsterKind` IS THE DEBUG HOOK, and both open states have
 * the same two arms: if it is zero -- which it always is, since nothing but
 * a debugger writes it and the port defines it at zero -- the ball calls
 * `itMainMakeMonster` and picks at random. If it is not, the ball calls
 * `itManagerMakeItemKind` DIRECTLY with `dITManagerForceMonsterKind +
 * (nITKindMBallMonsterStart - 1)` and copies the ball's own owner stats
 * onto the result by hand. The two arms exist because the forced path
 * bypasses `itMainMakeMonster`'s bookkeeping entirely. Kept as written.
 *
 * `ip->item_vars.mball.effect_gobj` is the rays, made by
 * efManagerMBallRaysMakeEffect and DRAGGED along with the ball
 * every frame until the open animation ends -- the one effect in the port
 * whose lifetime belongs to an item rather than to itself.
 *
 * DIVERGES: `dITMBallItemDesc`'s third field is a number rather than the
 * decomp's `&llITCommonDataMBallItemAttributes` symbol -- see
 * src/dc/itemoffsets.h -- and the two `itGetPData` calls drop their `&`. No
 * function body diverges.
 */
#include <it/item.h>
#include <ft/fighter.h>
#include <sc/scene.h>
#include <if/ifcommon.h>        /* ifCommonItemArrowMakeInterface */
#include <ef/effect.h>          /* the effect GObj itMBallOpenInitVars holds */

#include "itemoffsets.h"        /* llITCommonData* offsets */
#include "ftcommon.h"           /* ftParamMakeRumble, func_800269C0_275C0 */

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

/* it/itcommon/itmball.c:12-34 dITMBallItemDesc, verbatim. Its transform is
 * Null/Null -- the ball's own `SetItem` sets up the two XObjs by hand --
 * and its proc set is the AIR FALL state, which is what a ball thrown
 * straight up does. */
ITDesc dITMBallItemDesc =
{
    nITKindMBall,                           /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataMBallItemAttributes,    /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindNull,                  /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateOff,                      /* Hitbox Update State */
    itMBallFallProcUpdate,                  /* Proc Update */
    itMBallFallProcMap,                     /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itcommon/itmball.c:36-121 dITMBallStatusDescs, verbatim. Seven rows,
 * and the two OPEN rows are the ones this file exists for. */
ITStatusDesc dITMBallStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    {
        NULL,                               /* Proc Update */
        itMBallWaitProcMap,                 /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 1 (Air Wait Fall) */
    {
        itMBallFallProcUpdate,              /* Proc Update */
        itMBallFallProcMap,                 /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 2 (Fighter Hold) */
    {
        NULL,                               /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 3 (Fighter Throw) */
    {
        itMBallThrownProcUpdate,            /* Proc Update */
        itMBallThrownProcMap,               /* Proc Map */
        itMBallCommonProcHit,               /* Proc Hit */
        itMBallCommonProcHit,               /* Proc Shield */
        itMainCommonProcHop,                /* Proc Hop */
        itMBallCommonProcHit,               /* Proc Set-Off */
        itMBallCommonProcReflector,         /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 4 (Fighter Drop) */
    {
        itMBallFallProcUpdate,              /* Proc Update */
        itMBallThrownProcMap,               /* Proc Map */
        itMBallCommonProcHit,               /* Proc Hit */
        itMBallCommonProcHit,               /* Proc Shield */
        itMainCommonProcHop,                /* Proc Hop */
        itMBallCommonProcHit,               /* Proc Set-Off */
        itMBallCommonProcReflector,         /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 5 (Ground Open) */
    {
        itMBallOpenProcUpdate,              /* Proc Update */
        itMBallOpenProcMap,                 /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 6 (Air Open) */
    {
        itMBallOpenAirProcUpdate,           /* Proc Update */
        itMBallOpenAirProcMap,              /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        itMBallCommonProcReflector,         /* Proc Reflector */
        NULL                                /* Proc Damage */
    }
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itMBallStatus
{
    nITMBallStatusWait,
    nITMBallStatusFall,
    nITMBallStatusHold,
    nITMBallStatusThrown,
    nITMBallStatusDropped,
    nITMBallStatusOpen,
    nITMBallStatusOpenAir,
    nITMBallStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/itmball.c:147-156 itMBallOpenAddAnim 0x8017C690, verbatim.
 * The last two `child` steps are the ball's LID, and the MatAnimJoint is
 * what opens it. */
void itMBallOpenAddAnim(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    void *matanim_joint = itGetPData(ip, llITCommonDataMBallDataStart, llITCommonDataMBallMatAnimJoint);

    gcAddMObjMatAnimJoint(dobj->child->child->sib_next->mobj, matanim_joint, 0.0F);
    gcPlayAnimAll(item_gobj);
}

/* it/itcommon/itmball.c:158-164 itMBallOpenClearAnim 0x8017C6F8,
 * verbatim: the undo, called on the way INTO the open state. Note the
 * joint it clears is `child->sib_next`, one step short of the one the
 * function above fills -- the decomp's own asymmetry, kept. */
void itMBallOpenClearAnim(GObj *item_gobj)
{
    DObj *dobj = DObjGetStruct(item_gobj);

    dobj->child->sib_next->mobj->matanim_joint.event32 = NULL;
}

/* it/itcommon/itmball.c:166-178 itMBallFallProcUpdate 0x8017C710,
 * verbatim. The `child->sib_next` spin is the lid following the ball. */
sb32 itMBallFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITMBALL_GRAVITY, ITMBALL_TVEL);
    itVisualsUpdateSpin(item_gobj);

    dobj->child->sib_next->rotate.vec.f.z = dobj->rotate.vec.f.z;

    return FALSE;
}

/* it/itcommon/itmball.c:180-186 itMBallWaitProcMap 0x8017C768, verbatim. */
sb32 itMBallWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itMBallFallSetStatus);

    return FALSE;
}

/* it/itcommon/itmball.c:188-194 itMBallFallProcMap 0x8017C790, verbatim. */
sb32 itMBallFallProcMap(GObj *item_gobj)
{
    itMapCheckDestroyDropped(item_gobj, ITMBALL_MAP_REBOUND_COMMON, ITMBALL_MAP_REBOUND_GROUND, itMBallWaitSetStatus);

    return FALSE;
}

/* it/itcommon/itmball.c:196-201 itMBallWaitSetStatus 0x8017C7C8,
 * verbatim. */
void itMBallWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITMBallStatusDescs, nITMBallStatusWait);
}

/* it/itcommon/itmball.c:203-212 itMBallFallSetStatus 0x8017C7FC,
 * verbatim. */
void itMBallFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITMBallStatusDescs, nITMBallStatusFall);
}

/* it/itcommon/itmball.c:214-224 itMBallHoldSetStatus 0x8017C840, verbatim.
 * The `owner_gobj` copy is what `itMBallCommonProcReflector` later reads to
 * give a reflected ball back to the fighter who threw it. */
void itMBallHoldSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    DObjGetStruct(item_gobj)->rotate.vec.f.y = 0.0F;

    ip->item_vars.mball.owner_gobj = ip->owner_gobj;

    itMainSetStatus(item_gobj, dITMBallStatusDescs, nITMBallStatusHold);
}

/* it/itcommon/itmball.c:226-238 itMBallThrownProcUpdate 0x8017C880,
 * verbatim. */
sb32 itMBallThrownProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITMBALL_GRAVITY, ITMBALL_TVEL);
    itVisualsUpdateSpin(item_gobj);

    dobj->child->sib_next->rotate.vec.f.z = dobj->rotate.vec.f.z;

    return FALSE;
}

/* it/itcommon/itmball.c:240-252 itMBallThrownProcMap 0x8017C8D8,
 * verbatim. `is_rebound` decides whether a thrown ball MAY land (and open)
 * or is destroyed on contact -- set by the two proc_hits below. */
sb32 itMBallThrownProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->item_vars.mball.is_rebound != FALSE)
    {
        itMapCheckLanding(item_gobj, ITMBALL_MAP_REBOUND_COMMON, ITMBALL_MAP_REBOUND_GROUND, itMBallOpenSetStatus);
    }
    else itMapCheckDestroyDropped(item_gobj, ITMBALL_MAP_REBOUND_COMMON, ITMBALL_MAP_REBOUND_GROUND, itMBallOpenSetStatus);

    return FALSE;
}

/* it/itcommon/itmball.c:254-266 itMBallCommonProcHit 0x8017C94C,
 * verbatim. */
sb32 itMBallCommonProcHit(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->attack_coll.attack_state = nGMAttackStateOff;

    ip->item_vars.mball.is_rebound = TRUE;

    itMainVelSetRebound(item_gobj);

    return FALSE;
}

/* it/itcommon/itmball.c:268-291 itMBallCommonProcReflector 0x8017C97C,
 * verbatim. THE ONE PLACE A REFLECTED BALL CHANGES OWNER: the four stat
 * fields come off `item_vars.mball.owner_gobj`, which the HOLD state
 * stored -- so a ball reflected after being held belongs to the fighter who
 * held it, and one reflected in flight belongs to the one who threw it. */
sb32 itMBallCommonProcReflector(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    FTStruct *fp;
    GObj *fighter_gobj;

    ip->attack_coll.attack_state = nGMAttackStateOff;

    ip->item_vars.mball.is_rebound = TRUE;

    itMainVelSetRebound(item_gobj);

    fighter_gobj = ip->item_vars.mball.owner_gobj;
    ip->owner_gobj = fighter_gobj;
    fp = ftGetStruct(fighter_gobj);

    ip->team = fp->team;
    ip->player = fp->player;
    ip->player_num = fp->player_num;
    ip->handicap = fp->handicap;

    return FALSE;
}

/* it/itcommon/itmball.c:293-298 itMBallThrownSetStatus 0x8017C9E0,
 * verbatim. Both throw transitions add the opening animation FIRST, so a
 * ball that opens in flight already has its lid's script. */
void itMBallThrownSetStatus(GObj *item_gobj)
{
    itMBallOpenAddAnim(item_gobj);
    itMainSetStatus(item_gobj, dITMBallStatusDescs, nITMBallStatusThrown);
}

/* it/itcommon/itmball.c:300-305 itMBallDroppedSetStatus 0x8017CA14,
 * verbatim. */
void itMBallDroppedSetStatus(GObj *item_gobj)
{
    itMBallOpenAddAnim(item_gobj);
    itMainSetStatus(item_gobj, dITMBallStatusDescs, nITMBallStatusDropped);
}

/* it/itcommon/itmball.c:307-348 itMBallOpenProcUpdate 0x8017CA48,
 * verbatim. THE GROUND OPEN. The `multi == 0` arm is the whole point of
 * the item: 30 frames after the ball's lid opens, a monster comes out. */
sb32 itMBallOpenProcUpdate(GObj *mball_gobj)
{
    ITStruct *mball_ip = itGetStruct(mball_gobj);
    ITStruct *monster_ip;
    GObj *monster_gobj;
    Vec3f vel;
    s32 unused[2];

    if (mball_ip->multi == 0)
    {
        vel.x = vel.y = vel.z = 0.0F;

        if (dITManagerForceMonsterKind == 0)
        {
            itMainMakeMonster(mball_gobj);

            return TRUE;
        }
        monster_gobj = itManagerMakeItemKind(mball_gobj, dITManagerForceMonsterKind + (nITKindMBallMonsterStart - 1), &DObjGetStruct(mball_gobj)->translate.vec.f, &vel, (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_ITEM));

        if (monster_gobj != NULL)
        {
            monster_ip = itGetStruct(monster_gobj);

            monster_ip->owner_gobj = mball_ip->owner_gobj;
            monster_ip->team = mball_ip->team;
            monster_ip->player = mball_ip->player;
            monster_ip->handicap = mball_ip->handicap;
            monster_ip->player_num = mball_ip->player_num;
            monster_ip->display_mode = mball_ip->display_mode;
        }
        return TRUE;
    }
    mball_ip->multi--;

    if (mball_ip->item_vars.mball.effect_gobj != NULL)
    {
        DObjGetStruct(mball_ip->item_vars.mball.effect_gobj)->translate.vec.f = DObjGetStruct(mball_gobj)->translate.vec.f;
    }
    return FALSE;
}

/* it/itcommon/itmball.c:350-362 itMBallOpenProcMap 0x8017CB38, verbatim.
 * The ball's own `attach_line_id` is the floor line it opened on; when that
 * line stops existing (the floor went away) the ball becomes an AIR open. */
sb32 itMBallOpenProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (mpCollisionCheckExistLineID(ip->attach_line_id) == FALSE)
    {
        ip->is_attach_surface = FALSE;

        itMBallOpenAirSetStatus(item_gobj);
    }
    return FALSE;
}

/* it/itcommon/itmball.c:364-406 itMBallOpenInitVars 0x8017CB84, verbatim.
 * Six things happen here and every one is the ball's own:
 *
 *  1. the velocity is zeroed and BOTH children's hidden flags are TOGGLED
 *     (the ball swaps which half is visible as it opens),
 *  2. the opening sound,
 *  3. the throwing player's pad rumbles -- 8 frames at 20 Hz, and the
 *     player lookup is guarded twice because an item spawned by a stage has
 *     `player == -1` and one in a 4-player battle can sit at
 *     GMCOMMON_PLAYERS_MAX,
 *  4. the rays, kept in `item_vars.mball.effect_gobj`,
 *  5. the lid's MatAnimJoint cleared, and
 *  6. the attack collision turned off and made unreflectable -- the ball
 *     stops being a projectile the moment it opens. */
void itMBallOpenInitVars(GObj *item_gobj)
{
    s32 unused[2];
    DObj *dobj = DObjGetStruct(item_gobj);
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *child;
    DObj *sibling;

    ip->physics.vel_air.x = 0.0F;
    ip->physics.vel_air.y = 0.0F;
    ip->physics.vel_air.z = 0.0F;

    child = dobj->child;
    child->flags ^= DOBJ_FLAG_HIDDEN;

    sibling = dobj->child->sib_next;
    sibling->flags ^= DOBJ_FLAG_HIDDEN;

    func_800269C0_275C0(nSYAudioFGMMBallOpen);

    ip->attach_line_id = ip->coll_data.floor_line_id;

    ip->is_attach_surface = TRUE;

    if ((ip->player != -1) && (ip->player != GMCOMMON_PLAYERS_MAX))
    {
        GObj *fighter_gobj = gSCManagerBattleState->players[ip->player].fighter_gobj;

        if (fighter_gobj != NULL)
        {
            FTStruct *fp = ftGetStruct(fighter_gobj);

            ftParamMakeRumble(fp, 8, 20);
        }
    }
    ip->item_vars.mball.effect_gobj = efManagerMBallRaysMakeEffect(&dobj->translate.vec.f);

    itMBallOpenClearAnim(item_gobj);

    ip->attack_coll.attack_state = nGMAttackStateOff;
    ip->attack_coll.can_reflect = FALSE;
}

/* it/itcommon/itmball.c:408-413 itMBallOpenSetStatus 0x8017CC88,
 * verbatim. */
void itMBallOpenSetStatus(GObj *item_gobj)
{
    itMBallOpenInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITMBallStatusDescs, nITMBallStatusOpen);
}

/* it/itcommon/itmball.c:415-456 itMBallOpenAirProcUpdate 0x8017CCBC,
 * verbatim: the ground open's proc, verbatim, for the air status. The two
 * functions are the same twelve lines twice in the decomp and are kept
 * that way. */
sb32 itMBallOpenAirProcUpdate(GObj *mball_gobj)
{
    ITStruct *mball_ip = itGetStruct(mball_gobj);
    ITStruct *monster_ip;
    GObj *monster_gobj;
    Vec3f vel;
    s32 unused[2];

    if (mball_ip->multi == 0)
    {
        vel.x = vel.y = vel.z = 0.0F;

        if (dITManagerForceMonsterKind == 0)
        {
            itMainMakeMonster(mball_gobj);

            return TRUE;
        }
        monster_gobj = itManagerMakeItemKind(mball_gobj, dITManagerForceMonsterKind + (nITKindMBallMonsterStart - 1), &DObjGetStruct(mball_gobj)->translate.vec.f, &vel, (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_ITEM));

        if (monster_gobj != NULL)
        {
            monster_ip = itGetStruct(monster_gobj);

            monster_ip->owner_gobj = mball_ip->owner_gobj;
            monster_ip->team = mball_ip->team;
            monster_ip->player = mball_ip->player;
            monster_ip->handicap = mball_ip->handicap;
            monster_ip->player_num = mball_ip->player_num;
            monster_ip->display_mode = mball_ip->display_mode;
        }
        return TRUE;
    }
    mball_ip->multi--;

    if (mball_ip->item_vars.mball.effect_gobj != NULL)
    {
        DObjGetStruct(mball_ip->item_vars.mball.effect_gobj)->translate.vec.f = DObjGetStruct(mball_gobj)->translate.vec.f;
    }
    return FALSE;
}

/* it/itcommon/itmball.c:458-464 itMBallOpenAirProcMap 0x8017CDAC,
 * verbatim. */
sb32 itMBallOpenAirProcMap(GObj *item_gobj)
{
    itMapCheckDestroyDropped(item_gobj, ITMBALL_MAP_REBOUND_COMMON, ITMBALL_MAP_REBOUND_GROUND, itMBallOpenSetStatus);

    return FALSE;
}

/* it/itcommon/itmball.c:466-470 itMBallOpenAirSetStatus 0x8017CDE4,
 * verbatim. */
void itMBallOpenAirSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITMBallStatusDescs, nITMBallStatusOpenAir);
}

/* it/itcommon/itmball.c:472-508 itMBallMakeItem 0x8017CE0C, verbatim.
 *
 * The ball's own setup is the two XObjs -- `nGCMatrixKindTraRotRpyR` on the
 * ball and `0x46` on its LID -- and the two hidden-flag writes that make
 * the OPEN pair the visible one at spawn (`child` hidden, `child->sib_next`
 * shown; itMBallOpenInitVars toggles both). The US build's save/restore of
 * `translate` around the XObj calls is the decomp's own workaround for
 * `gcAddXObjForDObjFixed` clobbering it, kept in both arms. */
GObj* itMBallMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITMBallItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        DObj *dobj = DObjGetStruct(item_gobj);
        ITStruct *ip = itGetStruct(item_gobj);
#if defined(REGION_US)
        Vec3f translate = dobj->translate.vec.f;
#endif

        dobj->child->flags = DOBJ_FLAG_HIDDEN;
        dobj->child->sib_next->flags = DOBJ_FLAG_NONE;

        gcAddXObjForDObjFixed(dobj, nGCMatrixKindTraRotRpyR, 0);
        gcAddXObjForDObjFixed(dobj->child->sib_next, 0x46, 0);

#if defined(REGION_US)
        dobj->translate.vec.f = translate;
#else
        dobj->translate.vec.f = *pos;
#endif

        ip->multi = ITMBALL_SPAWN_WAIT;

        ip->item_vars.mball.is_rebound = FALSE;

        ip->is_unused_item_bool = TRUE;

        dobj->rotate.vec.f.z = 0.0F;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}
