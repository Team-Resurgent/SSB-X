/* itmain.c -- it/itmain.c, the item's small per-frame/per-event helpers:
 * spin speed (appearing, held, thrown), the gravity-clamp-to-terminal-
 * velocity step, player/owner bookkeeping, damage-output/ammo queries,
 * status dispatch, the col-anim wrappers, the weighted random-item
 * search, a scripted attack event's field refresh, and two of the
 * common `proc_hop`/`proc_reflector` bodies. Function-for-function
 * against the game's own ITStruct (it/ittypes.h); every function names
 * its decomp line range.
 *
 * Left out of this file, and why:
 *
 *  - itMainDestroyItem, SetFighterRelease, SetFighterDrop, SetFighterThrow
 *    and SetFighterHold need two ft/ftparam.c functions not yet in
 *    src/dc/ftparam.c (SetHammerParams, LinkResetShieldModelParts), and
 *    -- Drop/Throw/Hold specifically -- the three `dITMainProc*List`
 *    tables, each 21 entries deep into items this port has not touched.
 *    (SetGroundAllowPickup itself needed only itMapSetGround, it/itmap.c
 *    -- see below; it never needed the ftparam.c
 *    functions or the roster tables its neighbours here do.)
 *  - itMainMakeMonster is here, beside itMainMakeContainerItem below: it
 *    wants 1P-mode's gSC1PGameBonusMewCatcher and all thirteen monster
 *    table entries.
 *  - The three `dITMainProc*List` tables themselves: every entry names a
 *    per-item SetStatus this port has not written.
 *
 * Everything else -- twenty-two functions (itMainRefreshAttackColl needs
 * itProcessUpdateAttackPositions from it/itprocess.c;
 * itMainSetGroundAllowPickup needs itMapSetGround from it/itmap.c) -- touches
 * only the ITStruct, its DObj, and already-ported lb/sy/ftParam leaves
 * (lbCommonMag2D/NormDist2D/Scale2D, syVectorMag3D, syVectorRotateAbout3D --
 * sys/vector.c compiles unmodified --
 * ftParamGetStatUpdateCount/CheckSetColAnimID/ResetColAnim, all in
 * src/dc/ftparam.c). One divergence-shaped note, not a divergence:
 * itMainSearchRandomWeight's decomp text has no `return` on its last recursive
 * call (`else itMainSearchRandomWeight(...)`) -- undefined behaviour in C,
 * tolerated here exactly as written because every other decomp function this
 * port has carried across keeps its text verbatim even where the original MIPS
 * compiler's calling convention (the callee's return value already sitting in
 * the right register) papered over it; SH-4 GCC does the same in practice for
 * a tail call in this shape, and the host cross-test below proves the value
 * that comes back is correct. */
#include <it/item.h>
#include "itemoffsets.h"        /* llITCommonData* offsets */
#include <ft/fighter.h>
#include <gm/generic.h>
#include <sc/scene.h>           /* gSCManagerBattleState, gSCManagerSceneData,
                                 * nSCBattleGameType1PGame */
#include <lb/lbbackup.h>        /* gSCManagerBackupData,
                                 * LBBACKUP_UNLOCK_MASK_NEWCOMERS */
#include <gm/gmcollision.h>     /* gmCollisionGetFighterPartsWorldPosition */
#include <mp/mpcommon.h>        /* mpCommonRunItemCollisionDefault */
#include <sc/sc1pmode/sc1pgame.h> /* gSC1PGameBonusMewCatcher and the two
                                 * counters below it, src/dc/sc1pgame.c */
#include "itemmodel.h"          /* itemModelResetTransforms, the port's own
                                 * gcSetDObjTransformsForGObj (see the hold) */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

/* // // // // // // // // // // // //
 *                                   //
 *        INITIALIZED DATA           //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmain.c:21-114, the three roster tables, verbatim: what an item
 * does to ITSELF when a fighter drops it, throws it, or picks it up.
 * Indexed by ITStruct.kind, so the order is nITKind*'s own and a row must
 * never move. itMainSetFighterDrop/Throw/Hold call the entry if it is not
 * NULL; a NULL row is an item that has nothing of its own to do (a Star
 * Man is never held, a Maxim Tomato is eaten rather than thrown).
 *
 * Nineteen of the twenty-one named items are in src/dc/it*.c and the
 * game itself leaves Star Man and Ness's PK Fire NULL.
 *
 * Link's Bomb, the last row, is it/itfighter/itlinkbomb.c, compiled
 * unmodified. */
void (*dITMainProcDroppedList[/* */])(GObj*) =
{
    /* Containers */
    itBoxDroppedSetStatus,              /* Box */
    itTaruDroppedSetStatus,             /* Barrel */
    itCapsuleDroppedSetStatus,          /* Capsule */
    itEggDroppedSetStatus,              /* Egg */

    /* Usable items */
    itTomatoDroppedSetStatus,           /* Maxim Tomato */
    itHeartDroppedSetStatus,            /* Heart Container */
    NULL,                               /* Star Man */
    itSwordDroppedSetStatus,            /* Beam Sword */
    itBatDroppedSetStatus,              /* Home Run Bat */
    itHarisenDroppedSetStatus,          /* Fan */
    itStarRodDroppedSetStatus,          /* Star Rod */
    itLGunDroppedSetStatus,             /* Ray Gun */
    itFFlowerDroppedSetStatus,          /* Fire Flower */
    itHammerDroppedSetStatus,           /* Hammer */
    itMSBombDroppedSetStatus,           /* Motion-sensor Bomb */
    itBombHeiDroppedSetStatus,          /* Bob-omb */
    itNBumperDroppedSetStatus,          /* Normal Bumper */
    itGShellDroppedSetStatus,           /* Green Shell */
    itRShellDroppedSetStatus,           /* Red Shell */
    itMBallDroppedSetStatus,            /* Poké Ball */

    /* Fighter items */
    NULL,                               /* Ness PK Fire */
    itLinkBombDroppedSetStatus          /* Link Bomb */
};

void (*dITMainProcThrownList[/* */])(GObj*) =
{
    /* Containers */
    itBoxThrownSetStatus,               /* Box */
    itTaruThrownSetStatus,              /* Barrel */
    itCapsuleThrownSetStatus,           /* Capsule */
    itEggThrownSetStatus,               /* Egg */

    /* Usable items */
    NULL,                               /* Maxim Tomato */
    NULL,                               /* Heart Container */
    NULL,                               /* Star Man */
    itSwordThrownSetStatus,             /* Beam Sword */
    itBatThrownSetStatus,               /* Home Run Bat */
    itHarisenThrownSetStatus,           /* Fan */
    itStarRodThrownSetStatus,           /* Star Rod */
    itLGunThrownSetStatus,              /* Ray Gun */
    itFFlowerThrownSetStatus,           /* Fire Flower */
    itHammerThrownSetStatus,            /* Hammer */
    itMSBombThrownSetStatus,            /* Motion-sensor Bomb */
    itBombHeiThrownSetStatus,           /* Bob-omb */
    itNBumperThrownSetStatus,           /* Normal Bumper */
    itGShellThrownSetStatus,            /* Green Shell */
    itRShellThrownSetStatus,            /* Red Shell */
    itMBallThrownSetStatus,             /* Poké Ball */

    /* Fighter items */
    NULL,                               /* Ness PK Fire */
    itLinkBombThrownSetStatus           /* Link Bomb */
};

void (*dITMainProcHoldList[/* */])(GObj*) =
{
    /* Containers */
    itBoxHoldSetStatus,                /* Box */
    itTaruHoldSetStatus,               /* Barrel */
    itCapsuleHoldSetStatus,            /* Capsule */
    itEggHoldSetStatus,                /* Egg */

    /* Usable items */
    NULL,                              /* Maxim Tomato */
    NULL,                              /* Heart Container */
    NULL,                              /* Star Man */
    itSwordHoldSetStatus,              /* Beam Sword */
    itBatHoldSetStatus,                /* Home Run Bat */
    itHarisenHoldSetStatus,            /* Fan */
    itStarRodHoldSetStatus,            /* Star Rod */
    itLGunHoldSetStatus,               /* Ray Gun */
    itFFlowerHoldSetStatus,            /* Fire Flower */
    itHammerHoldSetStatus,             /* Hammer */
    itMSBombHoldSetStatus,             /* Motion-sensor Bomb */
    itBombHeiHoldSetStatus,            /* Bob-omb */
    itNBumperHoldSetStatus,            /* Normal Bumper */
    itGShellHoldSetStatus,             /* Green Shell */
    itRShellHoldSetStatus,             /* Red Shell */
    itMBallHoldSetStatus,              /* Poké Ball */

    /* Fighter items */
    NULL,                              /* Ness PK Fire */
    itLinkBombHoldSetStatus            /* Link Bomb */
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmain.c:122-133 itMainSetCommonSpin 0x80172310, verbatim. */
void itMainSetCommonSpin(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->spin_step = (ip->attr->spin_speed != 0) ? (F_PCT_TO_DEC(ip->attr->spin_speed) * ITEM_SPIN_SPEED_COMMON) : 0.0F;

    if (ip->lr == -1)
    {
        ip->spin_step = -ip->spin_step;
    }
}

/* it/itmain.c:135-155 itMainSetAppearSpin 0x80172394, verbatim. */
void itMainSetAppearSpin(GObj *item_gobj, sb32 slow_or_fast)
{
    /* slow_or_fast = 0 if slow, 1 if fast */

    ITStruct *ip = itGetStruct(item_gobj);

    if (slow_or_fast == 0)
    {
        if (ip->attr->spin_speed != 0)
        {
            ip->spin_step = F_PCT_TO_DEC(ip->attr->spin_speed) * ITEM_SPIN_SPEED_APPEAR_SLOW;
        }
        else ip->spin_step = 0.0F;
    }
    else if (ip->attr->spin_speed != 0)
    {
        ip->spin_step = F_PCT_TO_DEC(ip->attr->spin_speed) * ITEM_SPIN_SPEED_APPEAR_FAST;
    }
    else ip->spin_step = 0.0F;
}

/* it/itmain.c:157-169 itMainSetThrownSpin 0x8017245C, verbatim. */
void itMainSetThrownSpin(GObj *item_gobj, Vec3f *vel, sb32 is_smash_throw)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->spin_step = (is_smash_throw != FALSE) ? ITEM_SPIN_SPEED_SMASH_THROW : ITEM_SPIN_SPEED_NORMAL_THROW;

    if (vel->x < 0) /* Facing direction, sort of */
    {
        ip->spin_step = -ip->spin_step;
    }
    ip->spin_step = (ip->attr->spin_speed != 0) ? (F_PCT_TO_DEC(ip->attr->spin_speed) * ip->spin_step) : 0.0F;
}

/* it/itmain.c:171-179 itMainSetSpinVelLR 0x80172508, verbatim. */
void itMainSetSpinVelLR(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->lr = (ip->physics.vel_air.x >= 0.0F) ? +1 : -1;

    itMainSetCommonSpin(item_gobj);
}

/* it/itmain.c:181-191 itMainApplyGravityClampTVel 0x80172558, verbatim. */
void itMainApplyGravityClampTVel(ITStruct *ip, f32 gravity, f32 terminal_velocity)
{
    ip->physics.vel_air.y -= gravity;

    if (lbCommonMag2D(&ip->physics.vel_air) > terminal_velocity)
    {
        lbCommonNormDist2D(&ip->physics.vel_air);
        lbCommonScale2D(&ip->physics.vel_air, terminal_velocity);
    }
}

/* it/itmain.c:193-206 itMainResetPlayerVars 0x801725BC, verbatim. */
void itMainResetPlayerVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->owner_gobj = NULL;
    ip->team = ITEM_TEAM_DEFAULT;
    ip->player = ITEM_PORT_DEFAULT;
    ip->handicap = ITEM_HANDICAP_DEFAULT;
    ip->player_num = 0;
    ip->attack_coll.throw_mul = ITEM_THROW_DEFAULT;

    ip->display_mode = gITManagerDisplayMode;
}

/* it/itmain.c:208-225 itMainClearAttackRecord 0x801725F8, verbatim. */
void itMainClearAttackRecord(ITStruct *ip)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(ip->attack_coll.attack_records); i++)
    {
        GMAttackRecord *record = &ip->attack_coll.attack_records[i];

        record->victim_gobj = NULL;

        record->victim_flags.is_interact_hurt = record->victim_flags.is_interact_shield = record->victim_flags.is_interact_reflect = FALSE;

        record->victim_flags.timer_rehit = 0;

        record->victim_flags.group_id = 7;
    }
}

/* it/itmain.c:227-237 itMainRefreshAttackColl 0x8017275C, verbatim.
 * Needs itProcessUpdateAttackPositions (it/itprocess.c). */
void itMainRefreshAttackColl(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainClearAttackRecord(ip);

    ip->attack_coll.attack_state = nGMAttackStateNew;

    itProcessUpdateAttackPositions(item_gobj);
}

/* it/itmain.c:239-249 itMainClearOwnerStats 0x8017279C, verbatim. */
void itMainClearOwnerStats(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_damage_all = TRUE;

    ip->owner_gobj = NULL;

    ip->team = ITEM_TEAM_DEFAULT;
}

/* it/itmain.c:251-262 itMainCopyDamageStats 0x801727BC, verbatim (the
 * decomp's own `ip->player_num = ip->player_num;` self-assignment kept
 * as written -- its own comment flags it as possibly meant to read
 * damage_player_num, but that is not this port's call to make). */
void itMainCopyDamageStats(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->owner_gobj = ip->damage_gobj;
    ip->team = ip->damage_team;
    ip->player = ip->damage_port;
    ip->player_num = ip->player_num; /* Could potentially cause a bug? Didn't they mean damage_player_num? */
    ip->handicap = ip->damage_handicap;
    ip->display_mode = ip->damage_display_mode;
}

/* it/itmain.c:264-278 itMainGetDamageOutput 0x801727F4, verbatim. */
s32 itMainGetDamageOutput(ITStruct *ip)
{
    s32 damage;

    if (ip->is_thrown)
    {
        f32 mag = syVectorMag3D(&ip->physics.vel_air) * 0.1F;

        damage = (ip->attack_coll.damage + mag) * ip->attack_coll.throw_mul;
    }
    else damage = ip->attack_coll.damage;

    return (damage * ip->attack_coll.stale) + 0.999F;
}

/* it/itmain.c:280-290 itMainCheckShootNoAmmo 0x80172890, verbatim. */
sb32 itMainCheckShootNoAmmo(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (((ip->kind == nITKindStarRod) || (ip->kind == nITKindLGun) || (ip->kind == nITKindFFlower)) && (ip->multi == 0))
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itmain.c:482-496 itMainSetGroundAllowPickup 0x80172E74, verbatim.
 * Needs itMapSetGround (it/itmap.c); unlike SetFighterRelease/
 * Drop/Throw/Hold/DestroyItem it reaches for no
 * ftparam.c function and no 21-entry roster table, but it is bundled
 * with its neighbours
 * in the decomp's file order. */
void itMainSetGroundAllowPickup(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->attack_coll.attack_state = nGMAttackStateOff;

    ip->physics.vel_air.x = ip->physics.vel_air.y = ip->physics.vel_air.z = 0.0F;

    ip->is_allow_pickup = TRUE;

    ip->times_landed = 0;

    itMainResetPlayerVars(item_gobj);
    itMapSetGround(ip);
}

/* it/itmain.c:498-518 itMainSetStatus 0x80172EC8, verbatim. */
void itMainSetStatus(GObj *item_gobj, ITStatusDesc *status_desc, s32 status_id)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->proc_update    = status_desc[status_id].proc_update;
    ip->proc_map       = status_desc[status_id].proc_map;
    ip->proc_hit       = status_desc[status_id].proc_hit;
    ip->proc_shield    = status_desc[status_id].proc_shield;
    ip->proc_hop       = status_desc[status_id].proc_hop;
    ip->proc_setoff    = status_desc[status_id].proc_setoff;
    ip->proc_reflector = status_desc[status_id].proc_reflector;
    ip->proc_damage    = status_desc[status_id].proc_damage;

    ip->is_thrown = FALSE;

    ip->attack_coll.stat_flags.attack_id = nFTStatusAttackIDNull;
    ip->attack_coll.stat_flags.is_smash_attack = ip->attack_coll.stat_flags.ga = ip->attack_coll.stat_flags.is_projectile = FALSE;

    ip->attack_coll.stat_count = ftParamGetStatUpdateCount();
}

/* it/itmain.c:520-526 itMainCheckSetColAnimID 0x80172F98, verbatim. */
sb32 itMainCheckSetColAnimID(GObj *item_gobj, s32 colanim_id, s32 duration)
{
    ITStruct *ip = itGetStruct(item_gobj);

    return ftParamCheckSetColAnimID(&ip->colanim, colanim_id, duration);
}

/* it/itmain.c:528-534 itMainClearColAnim 0x80172FBC, verbatim. */
void itMainClearColAnim(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ftParamResetColAnim(&ip->colanim);
}

/* it/itmain.c:536-543 itMainVelSetRebound 0x80172FE0, verbatim. */
void itMainVelSetRebound(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->physics.vel_air.x *= -0.06F;
    ip->physics.vel_air.y = (ip->physics.vel_air.y * -0.3F) + 25.0F;
}

/* it/itmain.c:545-566 itMainSearchRandomWeight 0x8017301C, verbatim --
 * recursive binary search, and the decomp's own missing `return` on the
 * final recursive call (see the file header). */
s32 itMainSearchRandomWeight(s32 random, ITRandomWeights *weights, u32 min, u32 max) /* Recursive! */
{
    if (max == (min + 1))
    {
        return min;
    }
    else
    {
        s32 avg = (s32) (min + max) / 2;

        if (random < weights->blocks[avg])
        {
            itMainSearchRandomWeight(random, weights, min, avg);
        }
        else if (random < weights->blocks[avg + 1])
        {
            return avg;
        }
        else itMainSearchRandomWeight(random, weights, avg, max);
    }
}

/* it/itmain.c:568-572 itMainGetWeightedItemKind 0x80173090, verbatim. */
s32 itMainGetWeightedItemKind(ITRandomWeights *weights)
{
    return weights->kinds[itMainSearchRandomWeight(syUtilsRandIntRange(weights->weights_sum), weights, 0, weights->valids_num)];
}

/* it/itmain.c:575-612 itMainMakeContainerItem 0x80172FB0, verbatim.
 *
 * A Container's own drop: the hit that breaks one open (or the wall a
 * thrown one meets) rolls a kind out of `gITManagerRandomWeights` and
 * spawns it as a child item at the container's own position. That table
 * is built by itManagerSetupContainerDrops from the stage's
 * MPGroundData.item_weights, which is why a stage with no weights row --
 * makes this return FALSE and the container explode instead of
 * dropping.
 *
 * The velocity is a byte read and stays one: `ContainerVelocitiesY` is a
 * pointer-free f32 row at 0x0 of ITCommonData, indexed by kind, so the
 * port reads the same bytes off region 0 as the game does. (The decomp's
 * own comment calls the whole expression "quite ridiculous especially
 * since llITCommonDataContainerVelocitiesY is 0" -- it is arithmetically
 * the same as `((f32 *)gITManagerCommonData)[kind]`, which is what it
 * means.) The row is zero, so every container's drop starts with no
 * vertical velocity.
 *
 * The child is spawned with ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_ITEM
 * and the container is left on the field with its appearance spin turned
 * ON -- itMainSetAppearSpin(parent, TRUE) is the container's own tell
 * that it is now empty and about to despawn.
 *
 * itMainMakeMonster, its twin in this file, is ported directly below --
 * it needs the thirteen monster table entries and the 1P flag it writes.
 * See the file header.
 */
sb32 itMainMakeContainerItem(GObj *parent_gobj)
{
    s32 unused;
    s32 kind;
    Vec3f vel; /* Item's spawn velocity when falling out of a container */

    if (gITManagerRandomWeights.weights_sum != 0)
    {
        kind = itMainGetWeightedItemKind(&gITManagerRandomWeights);

        if (kind <= nITKindCommonEnd)
        {
            vel.x = 0.0F;

            /* Quite ridiculous especially since llITCommonDataContainerVelocitiesY is 0 */
            vel.y = *(f32*) ((intptr_t)llITCommonDataContainerVelocitiesY + ((uintptr_t) &((f32*)gITManagerCommonData)[kind]));
            vel.z = 0;

            if
            (
                itManagerMakeItemSetupCommon
                (
                    parent_gobj,
                    kind,
                    &DObjGetStruct(parent_gobj)->translate.vec.f,
                    &vel,
                    (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_ITEM)
                )
                != NULL
            )
            {
                itMainSetAppearSpin(parent_gobj, TRUE);
            }
            return TRUE;
        }
    }
    return FALSE;
}

/* sc1pgame.c's bonus counters gSC1PGameBonusMewCatcher (written by
 * itMainMakeMonster above), gSC1PGameBonusTomatoCount and
 * gSC1PGameBonusHeartCount (bumped by ft/ftcommon/ftcommonget.c) are
 * defined in src/dc/sc1pgame.c, where the decomp has them. */

/* it/itmain.c:635-702 itMainMakeMonster 0x80173228, verbatim.
 * Container-drop's twin and the Poké Ball's own spawner: it picks a
 * monster KIND and hands it to itManagerMakeItemKind.
 *
 * THREE THINGS IT HAS THAT ITS TWIN DOES NOT:
 *
 *  - Mew. It is not in the weighted pool at all -- it is a 1-in-151 roll
 *    gated on the save file's `LBBACKUP_UNLOCK_MASK_NEWCOMERS` (Luigi,
 *    Purin, Captain Falcon and Ness all unlocked) and on Mew not having
 *    been the CURRENT or the PREVIOUS monster. That pair of "not the last
 *    two" checks is what makes Mew feel rare rather than random.
 *  - The pool. Twelve common monsters (nITKindMBallCommonStart..End) with
 *    the same last-two exclusion, compacted into
 *    `gITManagerMonsterData.monster_id` and sampled against `monsters_num`
 *    -- which is DECREMENTED whenever it is not 10, so the pool shrinks
 *    for a while after a repeat is avoided and then settles. The two
 *    fields it maintains (`monster_curr`, `monster_prev`) are the reason
 *    Mew's gate above can ask.
 *  - The 1P bonus. A monster that is Mew AND belongs to the human player
 *    sets `gSC1PGameBonusMewCatcher` -- dead in a VS battle, and a global
 *    the port defines rather than drops because this function is its only
 *    writer.
 *
 * The spawn is `itManagerMakeItemKind(item_gobj, index, ...)`, which is
 * the dITManagerProcMakeList dispatch a Container's drop also goes
 * through: a kind with no table entry is an immediate crash, which is why
 * all thirteen monster entries had to be real before this could land. */
GObj* itMainMakeMonster(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    GObj *monster_gobj;
    ITStruct *mp;
    s32 i, j;
    s32 index;
    s32 unused;
    Vec3f vel;

    vel.x = 0.0F;
    vel.y = 16.0F;
    vel.z = 0.0F;

    /* Is this checking to spawn Mew? Can only spawn once at least one character has been unlocked. */
    if
    (
        (gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_NEWCOMERS) &&
        (syUtilsRandIntRange(151) == 0) &&
        (gITManagerMonsterData.monster_curr != nITKindMew) &&
        (gITManagerMonsterData.monster_prev != nITKindMew)
    )
    {
        index = nITKindMew;
    }
    else
    {
        for (i = j = nITKindMBallCommonStart; i <= nITKindMBallCommonEnd; i++) /* Pokémon IDs */
        {
            if ((i != gITManagerMonsterData.monster_curr) && (i != gITManagerMonsterData.monster_prev))
            {
                gITManagerMonsterData.monster_id[j - nITKindMBallMonsterStart] = i;
                j++;
            }
        }
        index = gITManagerMonsterData.monster_id[syUtilsRandIntRange(gITManagerMonsterData.monsters_num)];
    }
    if (gITManagerMonsterData.monsters_num != 10)
    {
        gITManagerMonsterData.monsters_num--;
    }
    gITManagerMonsterData.monster_prev = gITManagerMonsterData.monster_curr;
    gITManagerMonsterData.monster_curr = index;

    monster_gobj = itManagerMakeItemKind(item_gobj, index, &DObjGetStruct(item_gobj)->translate.vec.f, &vel, (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_ITEM));

    if (monster_gobj != NULL)
    {
        mp = itGetStruct(monster_gobj);

        mp->owner_gobj = ip->owner_gobj;
        mp->team = ip->team;
        mp->player = ip->player;
        mp->handicap = ip->handicap;
        mp->player_num = ip->player_num;
        mp->display_mode = ip->display_mode;

        if (gSCManagerBattleState->game_type == nSCBattleGameType1PGame)
        {
            if ((mp->player == gSCManagerSceneData.player) && (mp->kind == nITKindMew))
            {
                gSC1PGameBonusMewCatcher = TRUE;
            }
        }
    }
    return monster_gobj;
}

/* it/itmain.c:614-632 itMainUpdateAttackEvent 0x80173180, verbatim. */
void itMainUpdateAttackEvent(GObj *item_gobj, ITAttackEvent *ev)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == ev[ip->event_id].timer)
    {
        ip->attack_coll.angle  = ev[ip->event_id].angle;
        ip->attack_coll.damage = ev[ip->event_id].damage;
        ip->attack_coll.size   = ev[ip->event_id].size;

        ip->event_id++;

        if (ip->event_id == 4)
        {
            ip->event_id = 3;
        }
    }
}

/* it/itmain.c:703-712 itMainCommonProcHop 0x801733E4, verbatim. */
sb32 itMainCommonProcHop(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    syVectorRotateAbout3D(&ip->physics.vel_air, &ip->shield_collide_dir, ip->shield_collide_angle * 2);
    itMainSetSpinVelLR(item_gobj);

    return FALSE;
}

/* it/itmain.c:714-725 itMainCommonProcReflector 0x80173434, verbatim. */
sb32 itMainCommonProcReflector(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    FTStruct *fp = ftGetStruct(ip->owner_gobj);

    if ((ip->physics.vel_air.x * fp->lr) < 0.0F)
    {
        ip->physics.vel_air.x = -ip->physics.vel_air.x;
    }
    return FALSE;
}

/* it/itmain.c:293-315 itMainDestroyItem 0x80172940, verbatim.
 *
 * THE ITEM SUBSYSTEM'S FREE. `itProcessProcItemMain` destroys
 * an item exactly when its `proc_update` returns TRUE; without this the
 * GObj would keep its process, keep running its update every tic, and
 * never leave the field (Saffron City's Charmander would walk away for
 * ever, because the walk in its own update runs before the check). Every
 * item that ends by returning TRUE depends on it -- the Poké
 * Ball monsters, the containers, the bob-ombs after their explosions.
 *
 * The three arms are the decomp's own, and each is a visible thing: a HELD
 * item releases its owner's hands (and re-picks the item music); anything
 * that is not a Gate monster puffs dust where it stood, while a monster
 * coming out of Saffron City's door does not, because its own explosion or
 * its walk back into the frame is the visible event; and the ITStruct goes
 * back to the pool BEFORE the GObj is ejected -- the decomp's own order,
 * and the reason a freed item's struct is already on the free list by the
 * time anything the eject runs can look.
 *
 * DIVERGES, none. The two things it calls are ported:
 * ftParamSetHammerParams (ftparam.c) and
 * efManagerDustExpandLargeMakeEffect. */
/* The decomp's own range test, as a function rather than a condition, for
 * the same reason wpManagerIsModelLess is one: it is the
 * ONE part of the free a host test cannot observe, because with no particle
 * bank the large dust is allocated and handed straight back -- the effect
 * pool's count is identical whether the arm ran or was skipped. Shared, the
 * decision is checkable on the machine that cannot run the arm. */
sb32 itMainIsGroundMonster(ITStruct *ip)
{
    return ((ip->kind >= nITKindGroundMonsterStart) &&
            (ip->kind <= nITKindGroundMonsterEnd)) ? TRUE : FALSE;
}

void itMainDestroyItem(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if ((ip->is_hold) && (ip->owner_gobj != NULL))
    {
        FTStruct *fp = ftGetStruct(ip->owner_gobj);

        fp->item_gobj = NULL;

        ftParamSetHammerParams(ip->owner_gobj);
    }
    else if (itMainIsGroundMonster(ip) == FALSE)
    {
        efManagerDustExpandLargeMakeEffect(&DObjGetStruct(item_gobj)->translate.vec.f);
    }
    if (ip->arrow_gobj != NULL)
    {
        gcEjectGObj(ip->arrow_gobj);
    }
    itManagerSetPrevStructAlloc(ip);
    gcEjectGObj(item_gobj);
}

/* ---- it/itmain.c:318-479, the hold and the three ways out of it.
 *
 * Needs the three roster tables above, and two ft/ftparam.c leaves in
 * src/dc/ftparam.c -- ftParamSetHammerParams and
 * ftParamLinkResetShieldModelParts (beside it).
 *
 * Between them these four are everything that happens to the ITEM when a
 * fighter takes hold of it or lets go. What happens to the FIGHTER is
 * ft/ftcommon/ftcommonget.c and ftcommonitemthrow.c, which call in here.
 * ---- */

/* it/itmain.c:318-359 itMainSetFighterRelease 0x80172984, verbatim: the
 * common tail of a drop and a throw. Takes the item out of the hand's
 * DObj tree, plants it at that hand's world position, hands it back to
 * the map collision, gives it the release velocity scaled by its own
 * vel_scale, and arms its hitbox with the thrower's stat flags so the
 * damage it deals is credited to him.
 *
 * The decomp's own comment on the line above it: Link's Bomb redeclares
 * this function without stat_flags and stat_count. Nothing this port
 * compiles does that, so the prototype here is it/itmain.h's. */
void itMainSetFighterRelease(GObj *item_gobj, Vec3f *vel, f32 throw_mul, u16 stat_flags, u16 stat_count)
{
    ITStruct *ip = itGetStruct(item_gobj);
    GObj *fighter_gobj = ip->owner_gobj;
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f pos;
    s32 joint_id;

    lbCommonEjectTreeDObj(DObjGetStruct(item_gobj));

    pos.x = pos.y = pos.z = 0.0F;

    joint_id = (ip->weight == nITWeightHeavy) ? fp->attr->joint_itemheavy_id : fp->attr->joint_itemlight_id;

    gmCollisionGetFighterPartsWorldPosition(fp->joints[joint_id], &pos);

    DObjGetStruct(item_gobj)->translate.vec.f.x = pos.x;
    DObjGetStruct(item_gobj)->translate.vec.f.y = pos.y;
    DObjGetStruct(item_gobj)->translate.vec.f.z = 0.0F;

    mpCommonRunItemCollisionDefault(item_gobj, fp->coll_data.p_translate, &fp->coll_data);

    fp->item_gobj = NULL;

    ip->is_hold = FALSE;

    ip->physics.vel_air = *vel;

    syVectorScale3D(&ip->physics.vel_air, ip->vel_scale);

    ip->times_thrown++;
    ip->is_thrown = TRUE;

    ip->attack_coll.throw_mul = throw_mul;

    ip->attack_coll.stat_flags = *(GMStatFlags*)&stat_flags;
    ip->attack_coll.stat_count = stat_count;

    ftParamSetHammerParams(fighter_gobj);
    itMainRefreshAttackColl(item_gobj);
}

/* it/itmain.c:361-375 itMainSetFighterDrop 0x80172AEC, verbatim: a drop
 * is a throw with no smash, no sparkle and the item's own drop sound,
 * and its hitbox is credited as nFTStatusAttackIDItemThrow rather than
 * as whatever attack the fighter was in. */
void itMainSetFighterDrop(GObj *item_gobj, Vec3f *vel, f32 throw_mul)
{
    ITStruct *ip = itGetStruct(item_gobj);
    GObj *owner_gobj = ip->owner_gobj;
    FTStruct *fp = ftGetStruct(owner_gobj);

    if (dITMainProcDroppedList[ip->kind] != NULL)
    {
        dITMainProcDroppedList[ip->kind](item_gobj);
    }
    itMainSetFighterRelease(item_gobj, vel, throw_mul, nFTStatusAttackIDItemThrow, fp->stat_count);

    func_800269C0_275C0(ip->drop_sfx);
}

/* it/itmain.c:377-404 itMainSetFighterThrow 0x80172B78, verbatim: the
 * rumble (heavier for a heavy item, heavier again for a smash throw), the
 * item's own thrown status, the release, the white sparkle, the sound and
 * the spin. */
void itMainSetFighterThrow(GObj *item_gobj, Vec3f *vel, f32 throw_mul, sb32 is_smash_throw)
{
    ITStruct *ip = itGetStruct(item_gobj);
    GObj *owner_gobj = ip->owner_gobj;
    FTStruct *fp = ftGetStruct(owner_gobj);

    if (ip->weight == nITWeightLight)
    {
        if (is_smash_throw != FALSE)
        {
            ftParamMakeRumble(fp, 6, 0);
        }
    }
    else ftParamMakeRumble(fp, (is_smash_throw != FALSE) ? 9 : 6, 0);

    if (dITMainProcThrownList[ip->kind] != NULL)
    {
        dITMainProcThrownList[ip->kind](item_gobj);
    }
    itMainSetFighterRelease(item_gobj, vel, throw_mul, fp->stat_flags.halfword, fp->stat_count);

    efManagerSparkleWhiteScaleMakeEffect(&DObjGetStruct(item_gobj)->translate.vec.f, 1.0F);

    func_800269C0_275C0((is_smash_throw != FALSE) ? ip->smash_sfx : ip->throw_sfx);

    itMainSetThrownSpin(item_gobj, vel, is_smash_throw);
}

/* it/itmain.c:406-479 itMainSetFighterHold 0x80172CA4: the pickup's item
 * half, called from ft/ftcommon/ftcommonget.c's ftCommonGetProcUpdate on
 * the animation frame that closes the hand.
 *
 * The item stops being a thing in the world and becomes a thing in a
 * tree: it takes the fighter's team, player and handicap so that damage
 * it deals is his, stops moving, goes airborne as far as the map is
 * concerned, and gets a NEW parent DObj spliced above its own -- one
 * whose user_data points at the fighter's hand joint and whose single
 * XObj is matrix kind 0x52 (0x51 on the JP ROM), the kind that makes a
 * DObj's local matrix out of another DObj's world matrix. That is what
 * carries the item through the fighter's animation; src/dc/objdisplay.c's
 * gcDObjLocalMatrix is where the port implements it.
 *
 * DIVERGES, one line. The game's `gcSetDObjTransformsForGObj(item_gobj,
 * ip->attr->data)` re-reads the DObjDesc array it built the item's tree
 * from, putting every joint back to its rest pose before the item goes
 * into the hand -- an item that was spinning on the floor must not carry
 * the spin into the grip. In this port `attr->data` is a NULL/non-NULL
 * marker and the tree comes from the baked item-model pack
 * (src/dc/itmanager.c:762-796), so the same reset reads the pack's own
 * FPackJoint rows instead: itemModelResetTransforms, keyed by the same
 * `o_attributes` the tree was built by, starting at the new parent joint
 * exactly as the game's walk does. */
void itMainSetFighterHold(GObj *item_gobj, GObj *fighter_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    FTStruct *fp = ftGetStruct(fighter_gobj);
    DObj *joint;
    Vec3f pos;
    s32 joint_id;

    fp->item_gobj = item_gobj;
    ip->owner_gobj = fighter_gobj;

    ip->is_allow_pickup = FALSE;
    ip->is_hold = TRUE;

    ip->team = fp->team;
    ip->player = fp->player;
    ip->handicap = fp->handicap;
    ip->player_num = fp->player_num;

    ip->physics.vel_air.x = 0.0F;
    ip->physics.vel_air.y = 0.0F;
    ip->physics.vel_air.z = 0.0F;

    ip->display_mode = fp->display_mode;

    itMapSetAir(ip);

    joint = gcAddDObjForGObj(item_gobj, NULL);

    joint->sib_prev->sib_next = NULL;
    joint->sib_prev = NULL;
    joint->child = DObjGetStruct(item_gobj);

    DObjGetStruct(item_gobj)->parent = joint;

    item_gobj->obj = joint;

    gcAddXObjForDObjFixed(joint, 0x52, 0);

    joint_id = (ip->weight == nITWeightHeavy) ? fp->attr->joint_itemheavy_id : fp->attr->joint_itemlight_id;

    joint->user_data.p = fp->joints[joint_id];

    pos.x = 0.0F;
    pos.y = 0.0F;
    pos.z = 0.0F;

    gmCollisionGetFighterPartsWorldPosition(fp->joints[joint_id], &pos);
    efManagerItemGetSwirlProcUpdate(&pos);
    itemModelResetTransforms(joint);

    if (dITMainProcHoldList[ip->kind] != NULL)
    {
        dITMainProcHoldList[ip->kind](item_gobj);
    }
    ftParamLinkResetShieldModelParts(fighter_gobj);

    if (ip->weight == nITWeightLight)
    {
        func_800269C0_275C0(nSYAudioFGMItemGet);
    }
    else if (fp->attr->heavyget_sfx != nSYAudioFGMVoiceEnd)
    {
        func_800269C0_275C0(fp->attr->heavyget_sfx);
    }
    ftParamMakeRumble(fp, 6, 0);

    ip->pickup_wait = ITEM_PICKUP_WAIT_DEFAULT;
}
