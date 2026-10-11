/* ftparam.c -- ft/ftparam.c, the pieces the motion-command scripts and
 * hit detection reach: the attack-collision bookkeeping, the hit-status
 * and hurtbox setters, the stale-move queue, the sound handles. Every
 * function names its decomp line range and is the decomp's text unless
 * marked DIVERGES. The rest of ft/ftparam.c -- ftParamInitPlayerBattleStats,
 * the control lock, ftParamUpdateAnimKeys -- still lives in
 * src/dc/ftcommon.c from the steps that ported it.
 *
 * At the end are the leaves the scripts call into effects (ef/), model
 * and texture parts and the colour animations. They were stubs once; they
 * are the game's now, and the parts functions carry the DIVERGES of a
 * pack that has no display lists or MObjs.
 */
#include "ftcommon.h"

#include <sys/utils.h>
#include <gm/gmrumble.h>
#include <gm/gmcollision.h>
#include <ft/ftcommondata.h>
#include <if/ifcommon.h>
#include <it/item.h>            /* itGetStruct: the held Hammer, for the item music */
#include <it/itvars.h>          /* ITSTAR_WARN_BEGIN_FRAME, the two BGM durations */
#include <mp/map.h>             /* gMPCollisionBGMCurrent/Default */
#include <sc/sc1pmode/sc1pgame.h> /* the 1P game's bonus counters */
#include "overlay.h"
#include "objmodel.h"           /* dc_model_part_display: the model parts */

/* ft/ftparam.c:50-53 */
f32 dFTParamStaleTable[/* */] =
{
    0.75F, 0.82F, 0.89F, 0.96F
};

/* ft/ftparam.c:15 */
u8 dFTParamShuffleFrameIndexMax[/* */] = { 4, 3 };

/* ft/ftparam.c:234-242 ftParamSetDamageShuffle 0x800E7F70, verbatim */
void ftParamSetDamageShuffle(FTStruct *fp, sb32 is_electric, s32 damage, s32 status_id, f32 hitlag_mul)
{
    s32 shuffle_tics = ftParamGetHitLag(damage, status_id, hitlag_mul) * FTCOMMON_DAMAGE_SHUFFLE_MUL;

    fp->shuffle_tics = shuffle_tics;
    fp->shuffle_frame_index = 0;
    fp->is_shuffle_electric = is_electric;
    fp->shuffle_index_max = dFTParamShuffleFrameIndexMax[is_electric];
}

/* ft/ftparam.c:341-352 ftParamStopVoiceRunProcDamage 0x800E8244, verbatim */
void ftParamStopVoiceRunProcDamage(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftParamStopVoice(fp);

    if (fp->proc_damage != NULL)
    {
        fp->proc_damage(fighter_gobj);
    }
}

/* ft/ftparam.c:251-254 ftParamSetStickLR 0x800E8044, verbatim: face
 * the way the stick points. The dash is the caller. */
void ftParamSetStickLR(FTStruct *fp)
{
    fp->lr = (fp->input.pl.stick_range.x >= 0) ? +1 : -1;
}

/* ft/ftparam.c:257-263 ftParamMakeRumble 0x800E8058, verbatim (the
 * rumble is src/dc/gmrumble.c's) */
void ftParamMakeRumble(FTStruct *fp, s32 rumble_id, s32 length)
{
    if (fp->pkind == nFTPlayerKindMan)
    {
        gmRumbleSetPlayerRumbleParams(fp->player, rumble_id, length);
    }
}

/* ft/ftparam.c:266-269 ftParamSetCaptureImmuneMask 0x800E8098 */
void ftParamSetCaptureImmuneMask(FTStruct *fp, u8 capture_immune_mask)
{
    fp->capture_immune_mask = capture_immune_mask;
}

/* ft/ftparam.c:272-280 ftParamSetCatchParams 0x800E80A4: arms the catch
 * search. is_catchstatus TRUE is what makes ftMainProcSearchCatch look for
 * a fighter to grab each frame; the two procs are what it fires on a find
 * -- proc_catch on the catcher, proc_capture on the caught. The common
 * grab hands it ftCommonCatchPullProcCatch / ftCommonCapturePulledProcCapture
 * Verbatim. */
void ftParamSetCatchParams(FTStruct *fp, u8 catch_mask, void (*proc_catch)(GObj *), void (*proc_capture)(GObj *, GObj *))
{
    fp->is_catchstatus = TRUE;

    fp->catch_mask = catch_mask;
    fp->proc_catch = proc_catch;
    fp->proc_capture = proc_capture;
}

/* ft/ftparam.c:282-292 ftParamSetThrowParams 0x800E80C0: records who threw
 * this fighter (kind/player/team), read back by ftCommonThrownProcStatus and
 * the hit-log stats. Set on the thrown fighter as its throw resolves.
 * Verbatim. */
void ftParamSetThrowParams(FTStruct *this_fp, GObj *throw_gobj)
{
    FTStruct *throw_fp = ftGetStruct(throw_gobj); // Fighter throwing this player

    this_fp->throw_gobj = throw_gobj;
    this_fp->throw_fkind = throw_fp->fkind;
    this_fp->throw_player = throw_fp->player;
    this_fp->throw_player_num = throw_fp->player_num;
    this_fp->throw_team = throw_fp->team;
}

/* ft/ftparam.c:294-299 ftParamPlayVoice 0x800E80F0 */
void ftParamPlayVoice(FTStruct *fp, u16 voice_id)
{
    fp->p_voice = func_800269C0_275C0(voice_id);
    fp->voice_id = (fp->p_voice != NULL) ? fp->p_voice->sfx_id : 0;
}

/* ft/ftparam.c:302-313 ftParamStopVoice 0x800E8138 */
void ftParamStopVoice(FTStruct *fp)
{
    if (fp->p_voice != NULL)
    {
        if ((fp->p_voice->sfx_id != 0) && (fp->p_voice->sfx_id == fp->voice_id))
        {
            func_80026738_27338(fp->p_voice);
        }
    }
    fp->p_voice = NULL;
    fp->voice_id = 0;
}

/* ft/ftparam.c:316-324 ftParamPlayLoopSFX 0x800E8190 */
void ftParamPlayLoopSFX(FTStruct *fp, u16 sfx_id)
{
    if (fp->p_loop_sfx == NULL)
    {
        fp->p_loop_sfx = func_800269C0_275C0(sfx_id);
        fp->loop_sfx_id = (fp->p_loop_sfx != NULL) ? fp->p_loop_sfx->sfx_id : 0;
    }
}

/* ft/ftparam.c:327-338 ftParamStopLoopSFX 0x800E81E4 */
void ftParamStopLoopSFX(FTStruct *fp)
{
    if (fp->p_loop_sfx != NULL)
    {
        if ((fp->p_loop_sfx->sfx_id != 0) && (fp->p_loop_sfx->sfx_id == fp->loop_sfx_id))
        {
            func_80026738_27338(fp->p_loop_sfx);
        }
    }
    fp->p_loop_sfx = NULL;
    fp->loop_sfx_id = 0;
}

/* ft/ftparam.c:480-492 ftParamClearAttackCollAll 0x800E84E4 */
void ftParamClearAttackCollAll(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 i;

    for (i = 0; i < ARRAY_COUNT(fp->attack_colls); i++)
    {
        FTAttackColl *attack_coll = &fp->attack_colls[i];

        attack_coll->attack_state = nGMAttackStateOff;
    }
    fp->is_attack_active = FALSE;
}

/* ft/ftparam.c:495-511 ftParamClearAttackRecordID 0x800E853C */
void ftParamClearAttackRecordID(FTStruct *fp, s32 attack_id)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(fp->attack_colls[attack_id].attack_records); i++)
    {
        GMAttackRecord *record = &fp->attack_colls[attack_id].attack_records[i];

        record->victim_gobj = NULL;
        record->victim_flags.is_interact_hurt = record->victim_flags.is_interact_shield = FALSE;
        record->victim_flags.timer_rehit = 0;
        record->victim_flags.group_id = 7;
    }
}

/* ft/ftparam.c:514-523 ftParamRefreshAttackCollID 0x800E8668 */
void ftParamRefreshAttackCollID(GObj *fighter_gobj, s32 attack_id)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->attack_colls[attack_id].attack_state = nGMAttackStateNew;
    fp->is_attack_active = TRUE;

    ftParamClearAttackRecordID(fp, attack_id);
}

/* ft/ftparam.c:526-532 ftParamSetVelPush 0x800E86B4 */
void ftParamSetVelPush(GObj *fighter_gobj, Vec3f *vel_push)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->coll_data.vel_push = *vel_push;
}

/* ft/ftparam.c:534-541 ftParamGetJointID 0x800E86D4 */
s32 ftParamGetJointID(FTStruct *fp, s32 joint_id)
{
    if (joint_id == -2)
    {
        joint_id = fp->attr->joint_itemlight_id;
    }
    return joint_id;
}

/* ft/ftparam.c:569-585 ftParamSetHitStatusColAnim 0x800E87A8, verbatim */
void ftParamSetHitStatusColAnim(GObj *fighter_gobj, s32 hitstatus)
{
    switch (hitstatus)
    {
    case nGMHitStatusNormal:
        ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterHitStatusNormal, 0);
        break;

    case nGMHitStatusInvincible:
        ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterHitStatusInvincible, 0);
        break;

    case nGMHitStatusIntangible:
        ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterHitStatusIntangible, 0);
        break;
    }
}

/* ft/ftparam.c:588-605 ftParamSetHitStatusPartAll 0x800E880C */
void ftParamSetHitStatusPartAll(GObj *fighter_gobj, s32 hitstatus)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 i;

    for (i = 0; i < ARRAY_COUNT(fp->damage_colls); i++)
    {
        FTDamageColl *damage_coll = &fp->damage_colls[i];

        if (damage_coll->hitstatus != nGMHitStatusNone)
        {
            damage_coll->hitstatus = hitstatus;
        }
    }
    fp->is_hitstatus_nodamage = (hitstatus == nGMHitStatusNormal) ? FALSE : TRUE;

    ftParamSetHitStatusColAnim(fighter_gobj, hitstatus);
}

/* ft/ftparam.c:608-632 ftParamSetHitStatusPartID 0x800E8884 */
void ftParamSetHitStatusPartID(GObj *fighter_gobj, s32 joint_id, s32 hitstatus)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 i;

    for (i = 0; i < ARRAY_COUNT(fp->damage_colls); i++)
    {
        FTDamageColl *damage_coll = &fp->damage_colls[i];

        if ((damage_coll->hitstatus != nGMHitStatusNone) && (joint_id == damage_coll->joint_id))
        {
            damage_coll->hitstatus = hitstatus;

            if (damage_coll->hitstatus != nGMHitStatusNormal)
            {
                fp->is_hitstatus_nodamage = TRUE;
            }
            return;
        }
    }
}

/* ft/ftparam.c:635-642 ftParamSetHitStatusAll 0x800E8A24 */
void ftParamSetHitStatusAll(GObj *fighter_gobj, s32 hitstatus)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->hitstatus = hitstatus;

    ftParamSetHitStatusColAnim(fighter_gobj, hitstatus);
}

/* ft/ftparam.c:645-672 ftParamGetBestHitStatusPart 0x800E8A48 */
s32 ftParamGetBestHitStatusPart(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 hitstatus_base = fp->hitstatus;
    s32 hitstatus_best = fp->damage_colls[0].hitstatus;
    s32 i;

    if (hitstatus_best != nGMHitStatusNormal)
    {
        for (i = 1; i < ARRAY_COUNT(fp->damage_colls); i++)
        {
            FTDamageColl *damage_coll = &fp->damage_colls[i];

            if (damage_coll->hitstatus == nGMHitStatusNone)
            {
                break;
            }
            else if (hitstatus_best > damage_coll->hitstatus)
            {
                hitstatus_best = damage_coll->hitstatus;
            }
        }
    }
    if (hitstatus_base < hitstatus_best)
    {
        hitstatus_base = hitstatus_best;
    }
    return hitstatus_base;
}

/* ft/ftparam.c:675-690 ftParamGetBestHitStatusAll 0x800E8AAC */
s32 ftParamGetBestHitStatusAll(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 hitstatus_best = ftParamGetBestHitStatusPart(fighter_gobj);

    if (hitstatus_best < fp->star_hitstatus)
    {
        hitstatus_best = fp->star_hitstatus;
    }
    if (hitstatus_best < fp->special_hitstatus)
    {
        hitstatus_best = fp->special_hitstatus;
    }
    return hitstatus_best;
}

/* ft/ftparam.c:693-719 ftParamResetFighterDamageCollsAll 0x800E8B00 */
void ftParamResetFighterDamageCollsAll(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTDamageColl *damage_coll = &fp->damage_colls[0];
    FTDamageCollDesc *damage_coll_desc = &fp->attr->damage_coll_descs[0];
    s32 i;

    for (i = 0; i < (ARRAY_COUNT(fp->damage_colls) + ARRAY_COUNT(fp->attr->damage_coll_descs)) / 2; i++, damage_coll++, damage_coll_desc++)
    {
        if (damage_coll_desc->joint_id != -1)
        {
            damage_coll->joint_id = damage_coll_desc->joint_id;
            damage_coll->joint = fp->joints[damage_coll->joint_id];
            damage_coll->placement = damage_coll_desc->placement;
            damage_coll->is_grabbable = damage_coll_desc->is_grabbable;
            damage_coll->offset = damage_coll_desc->offset;
            damage_coll->size = damage_coll_desc->size;

            damage_coll->size.x *= 0.5F;
            damage_coll->size.y *= 0.5F;
            damage_coll->size.z *= 0.5F;
        }
    }
    fp->is_damage_coll_modify = FALSE;
}

/* ft/ftparam.c:722-745 ftParamModifyDamageCollID 0x800E8BC8 */
void ftParamModifyDamageCollID(GObj *fighter_gobj, s32 joint_id, Vec3f *offset, Vec3f *size)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 i;

    for (i = 0; i < ARRAY_COUNT(fp->damage_colls); i++)
    {
        FTDamageColl *damage_coll = &fp->damage_colls[i];

        if (joint_id == damage_coll->joint_id)
        {
            damage_coll->offset = *offset;
            damage_coll->size = *size;

            fp->damage_colls[i].size.x *= 0.5F;
            fp->damage_colls[i].size.y *= 0.5F;
            fp->damage_colls[i].size.z *= 0.5F;

            fp->is_damage_coll_modify = TRUE;

            return;
        }
    }
}

/* ft/ftparam.c:1575-1605 ftParamGetStale 0x800EA430 */
f32 ftParamGetStale(s32 player, s32 attack_id, u16 motion_count)
{
    s32 stale_id;
    s32 start_array_id;
    s32 current_array_id;
    s32 i;

    stale_id = gSCManagerBattleState->players[player].stale_id;

    if (attack_id != nFTMotionAttackIDNone)
    {
        current_array_id = start_array_id = (stale_id != 0) ? stale_id - 1 : ARRAY_COUNT(gSCManagerBattleState->players[player].stale_info) - 1;

        for (i = 0; i < ARRAY_COUNT(dFTParamStaleTable); i++)
        {
            if (attack_id == gSCManagerBattleState->players[player].stale_info[current_array_id].attack_id)
            {
                if (motion_count != gSCManagerBattleState->players[player].stale_info[current_array_id].motion_count)
                {
                    return dFTParamStaleTable[i];
                }
                else if (current_array_id == start_array_id)
                {
                    i--;
                }
            }
            current_array_id = (current_array_id != 0) ? current_array_id - 1 : ARRAY_COUNT(gSCManagerBattleState->players[player].stale_info) - 1;
        }
    }
    return 1.0F;
}

/* ft/ftparam.c:1608-1617 ftParamGetStaledDamage 0x800EA54C */
s32 ftParamGetStaledDamage(s32 player, s32 damage, s32 attack_id, u16 motion_count)
{
    f32 stale = ftParamGetStale(player, attack_id, motion_count);

    if (stale != 1.0F)
    {
        damage = (damage * stale) + 0.999F;
    }
    return damage;
}

/* ft/ftparam.c:1620-1629 ftParamGetMotionCount 0x800EA5BC */
u16 ftParamGetMotionCount(void)
{
    u16 motion_count = gFTManagerMotionCount++;

    if (gFTManagerMotionCount == 0)
    {
        gFTManagerMotionCount = 1;
    }
    return motion_count;
}

/* ft/ftparam.c:1632-1636 ftParamSetMotionID 0x800EA5E8 */
void ftParamSetMotionID(FTStruct *fp, s32 attack_id)
{
    fp->motion_attack_id = attack_id;
    fp->motion_count = ftParamGetMotionCount();
}

/* ft/ftparam.c:1639-1665 ftParamUpdateStaleQueue 0x800EA614 */
void ftParamUpdateStaleQueue(s32 attack_player, s32 defend_player, s32 attack_id, u16 motion_count)
{
    if ((attack_player != ARRAY_COUNT(gSCManagerBattleState->players)) && (attack_player != defend_player))
    {
        s32 i, stale_id = gSCManagerBattleState->players[attack_player].stale_id;

        for (i = 0; i < ARRAY_COUNT(gSCManagerBattleState->players[attack_player].stale_info); i++)
        {
            if
            (
                (attack_id    == gSCManagerBattleState->players[attack_player].stale_info[i].attack_id) && 
                (motion_count == gSCManagerBattleState->players[attack_player].stale_info[i].motion_count)
            )
            {
                return;
            }
        }
        gSCManagerBattleState->players[attack_player].stale_info[stale_id].attack_id    = attack_id;
        gSCManagerBattleState->players[attack_player].stale_info[stale_id].motion_count = motion_count;

        if (stale_id == (ARRAY_COUNT(gSCManagerBattleState->players[attack_player].stale_info) - 1))
        {
            gSCManagerBattleState->players[attack_player].stale_id = 0;
        }
        else gSCManagerBattleState->players[attack_player].stale_id = stale_id + 1;
    }
}

/* ft/ftparam.c:1668-1677 ftParamGetStatUpdateCount 0x800EA758 */
u16 ftParamGetStatUpdateCount(void)
{
    u16 update_count = gFTManagerStatUpdateCount++; 

    if (gFTManagerStatUpdateCount == 0)
    {
        gFTManagerStatUpdateCount = 1;
    }
    return update_count;
}

/* ft/ftparam.c:1680-1684 ftParamSetStatUpdate 0x800EA784 */
void ftParamSetStatUpdate(FTStruct *fp, u16 flags)
{
    fp->stat_flags = *(GMStatFlags*)&flags;
    fp->stat_count = ftParamGetStatUpdateCount();
}

/* ---- ft/ftparam.c:1451-1781, the numbers a hit turns into ------------ */

/* ft/ftparam.c:1451-1476 ftParamGetCommonKnockback 0x800E9D78, verbatim.
 * dFTCommonDataHandicapTable is ft/ftcommondata.c, compiled unmodified. */
f32 ftParamGetCommonKnockback(s32 percent_damage, s32 recent_damage, s32 hit_damage, s32 knockback_weight, s32 knockback_scale, s32 knockback_base, f32 weight, s32 attack_handicap, s32 defend_handicap)
{
    f32 knockback;

    if (knockback_weight != 0)
    {
        knockback = ( ( ( ( ( ( 1 + ( 10.0F * knockback_weight * 0.05F ) ) * weight * 1.4F ) + 18.0F ) * ( knockback_scale * 0.01F ) ) + knockback_base ) * ( gSCManagerBattleState->damage_ratio * 0.01F ) * dFTCommonDataHandicapTable[attack_handicap - 1][0] ) * dFTCommonDataHandicapTable[defend_handicap - 1][1];
    } 
    else 
    {
        f32 damage_add = percent_damage + recent_damage;

        knockback = ( ( ( ( ( ( ( damage_add * 0.1F ) + ( damage_add * hit_damage * 0.05F ) ) * weight * 1.4F ) + 18.0F ) * ( knockback_scale * 0.01F ) ) + knockback_base ) * ( gSCManagerBattleState->damage_ratio * 0.01F ) * dFTCommonDataHandicapTable[attack_handicap - 1][0] ) * dFTCommonDataHandicapTable[defend_handicap - 1][1];
    }
    if (knockback >= 2500.0F)
    {
        knockback = 2500.0F;
    }
    if (gSCManagerBackupData.error_flags & LBBACKUP_ERROR_RANDOMKNOCKBACK)
    {
        knockback = syUtilsRandFloat() * 200.0F;
    }
    return knockback;
}

/* ft/ftparam.c:1477-1504 ftParamGetGroundHazardKnockback 0x800E9FC0,
 * verbatim -- for Kongo Jungle's barrel cannon,
 * which is what the decomp's own comment above it says it is for. It is
 * ftParamGetCommonKnockback's neighbour and its twin, with one real
 * difference: where the common curve carries a
 * `* (gSCManagerBattleState->damage_ratio * 0.01F)`, this one carries a
 * bare `* 1` -- the damage ratio does NOT scale a ground hazard's
 * knockback. That is the decomp's own US arm (it is REGION_US-only, the
 * JP build having no such function), so the `* 1` is ported as written
 * rather than folded away. */
f32 ftParamGetGroundHazardKnockback(s32 percent_damage, s32 recent_damage, s32 hit_damage, s32 knockback_weight, s32 knockback_scale, s32 knockback_base, f32 weight, s32 attack_handicap, s32 defend_handicap)
{
    f32 knockback;

    if (knockback_weight != 0)
    {
        knockback = ( ( ( ( ( ( 1 + ( 10.0F * knockback_weight * 0.05F ) ) * weight * 1.4F ) + 18.0F ) * ( knockback_scale * 0.01F ) ) + knockback_base ) * 1 * dFTCommonDataHandicapTable[attack_handicap - 1][0] ) * dFTCommonDataHandicapTable[defend_handicap - 1][1];
    }
    else
    {
        f32 damage_add = percent_damage + recent_damage;

        knockback = ( ( ( ( ( ( ( damage_add * 0.1F ) + ( damage_add * hit_damage * 0.05F ) ) * weight * 1.4F ) + 18.0F ) * ( knockback_scale * 0.01F ) ) + knockback_base) * 1 * dFTCommonDataHandicapTable[attack_handicap - 1][0] ) * dFTCommonDataHandicapTable[defend_handicap - 1][1];
    }
    if (knockback >= 2500.0F)
    {
        knockback = 2500.0F;
    }
    if (gSCManagerBackupData.error_flags & LBBACKUP_ERROR_RANDOMKNOCKBACK)
    {
        knockback = syUtilsRandFloat() * 200.0F;
    }
    return knockback;
}

/* ft/ftparam.c:1505-1508 ftParamGetHitStun 0x800EA1B0, verbatim */
f32 ftParamGetHitStun(f32 knockback)
{
    return knockback / 1.875F;
}

/* ft/ftparam.c:1511-1525 ftParamGetHitLag 0x800EA1C0, verbatim (US) */
s32 ftParamGetHitLag(s32 damage, s32 status_id, f32 hitlag_mul)
{
    s32 hitlag_tics = (s32) ( (damage * (1.0F / 3.0F) ) + 5.0F ) * hitlag_mul;

    if ((status_id == nFTCommonStatusSquat) || (status_id == nFTCommonStatusSquatWait))
    {
        hitlag_tics *= (2.0F / 3.0F);
    }
    return hitlag_tics;
}

/* ft/ftparam.c:1527-1555 ftParamUpdateDamage 0x800EA248, verbatim.
 * Its second half is a held item shaken loose by a big enough
 * hit (or, half the time, an empty shooter). */
void ftParamUpdateDamage(FTStruct *fp, s32 damage)
{
    fp->percent_damage += damage;

    gSCManagerBattleState->players[fp->player].total_damage_all += damage;

    if (fp->percent_damage > 999)
    {
        fp->percent_damage = 999;
    }
    gSCManagerBattleState->players[fp->player].stock_damage_all = fp->percent_damage;

    if (fp->item_gobj != NULL)
    {
        if ((fp->damage_knockback != 0.0F) && ((fp->hitlag_tics == 0) || !(fp->is_knockback_paused) || !(fp->damage_knockback < (fp->damage_knockback_stack + 30.0F))))
        {
            ITStruct *ip = itGetStruct(fp->item_gobj);

            if ((ip->weight != nITWeightHeavy) || (fp->fkind != nFTKindDonkey) && (fp->fkind != nFTKindNDonkey) && (fp->fkind != nFTKindGDonkey))
            {
                if ((damage > syUtilsRandIntRange(60)) || ((itMainCheckShootNoAmmo(fp->item_gobj) != FALSE) && (syUtilsRandIntRange(2) == 0)))
                {
                    ftSetupDropItem(fp);
                }
            }
        }
    }
}

/* ft/ftparam.c:1565-1573 ftParamGetCapturedDamage 0x800EA40C, verbatim */
s32 ftParamGetCapturedDamage(FTStruct *fp, s32 damage)
{
    if (fp->capture_gobj != NULL)
    {
        damage = (damage * 0.5F) + 0.999F;
    }
    return (damage * fp->damage_mul) + 0.999F;
}

/* ft/ftparam.c:1744-1755 ftParamUpdatePlayerBattleStats 0x800EA9C8, verbatim */
void ftParamUpdatePlayerBattleStats(s32 attack_player, s32 defend_player, s32 attack_damage)
{
    if ((attack_player != GMCOMMON_PLAYERS_MAX) && (attack_player != defend_player))
    {
        gSCManagerBattleState->players[attack_player].total_damage_given += attack_damage;

        gSCManagerBattleState->players[defend_player].total_damage_players[attack_player] += attack_damage;
        gSCManagerBattleState->players[defend_player].combo_damage_foe += attack_damage;
        gSCManagerBattleState->players[defend_player].combo_count_foe++;
    }
}

/* ft/ftparam.c:1757-1781 ftParamUpdate1PGameDamageStats 0x800EAA2C,
 * verbatim. The 1P game's defend-side bonus counters were cut here
 * before the 1P game existed; they live in src/dc/sc1pgame.c and the
 * score screen reads them. */
void ftParamUpdate1PGameDamageStats(FTStruct *fp, s32 damage_player, s32 damage_object_class, s32 damage_object_kind, u16 flags, u16 damage_stat_count)
{
    fp->damage_player = damage_player;
    fp->damage_object_class = damage_object_class;
    fp->damage_object_kind = damage_object_kind;
    fp->damage_count++;

    if (!(damage_stat_count) || (fp->damage_stat_count != damage_stat_count))
    {
        fp->damage_stat_flags = *(GMStatFlags*)&flags;
        fp->damage_stat_count = damage_stat_count;

        if (gSCManagerBattleState->game_type == nSCBattleGameType1PGame)
        {
            if ((gSCManagerSceneData.player == damage_player) && (fp->damage_stat_flags.attack_id != nFTStatusAttackIDNone))
            {
                gSC1PGameBonusDefendIDCount[fp->damage_stat_flags.attack_id]++;
                gSC1PGameBonusDefendIsSmashCount[fp->damage_stat_flags.is_smash_attack]++;
                gSC1PGameBonusDefendGroundAirCount[fp->damage_stat_flags.ga]++;
                gSC1PGameBonusDefendIsProjectileCount[fp->damage_stat_flags.is_projectile]++;
            }
        }
    }
}

/* ft/ftparam.c:1687-1704 ftParamUpdate1PGameAttackStats 0x800EA8B0,
 * verbatim: when the 1P player's attack changes, the one it is leaving
 * is counted, by kind, for the score screen's bonuses. It was a stub
 * from before the 1P game existed. */
void ftParamUpdate1PGameAttackStats(FTStruct *fp, u16 flags)
{
    GMStatFlags stat_flags = *(GMStatFlags*)&flags;

    if ((fp->pkind != nFTPlayerKindDemo) && (gSCManagerBattleState->game_type == nSCBattleGameType1PGame) && (fp->player == gSCManagerSceneData.player))
    {
        if ((fp->stat_flags.attack_id != nFTStatusAttackIDNone) && (fp->stat_flags.attack_id != stat_flags.attack_id))
        {
            gSC1PGameBonusAttackIDCount[fp->stat_flags.attack_id]++;

            gSC1PGameBonusAttackIsSmashCount[fp->stat_flags.is_smash_attack]++;

            gSC1PGameBonusAttackGroundAirCount[fp->stat_flags.ga]++;

            gSC1PGameBonusAttackIsProjectileCount[fp->stat_flags.is_projectile]++;
        }
    }
}

/* ---- ft/ftparam.c:2161-2420, the FTParts transform bookkeeping ------- */

/* ft/ftparam.c:2161-2281 ftParamsUpdateFighterPartsTransformAll
 * 0x800EB4E8, verbatim: walk the tree from root_dobj marking every
 * joint's cached world matrix stale (gm/gmcollision.c rebuilds one
 * when a hitbox or hurtbox asks) and unlocking mode-1 joints. */
void ftParamsUpdateFighterPartsTransformAll(DObj *root_dobj)
{
    DObj *parent_sibling;
    DObj *current_parent_sibling;
    DObj *child;
    DObj *sibling;
    DObj *parent;
    DObj *current_child;
    DObj *current_sibling;
    DObj *current_parent;
    DObj *origin;
    DObj *current;
    FTParts *parts;
    FTParts *current_parts;

    origin = root_dobj;
    parts = root_dobj->user_data.p;

    if (parts != NULL)
    {
        if (parts->transform_update_mode == 1)
        {
            parts->transform_update_mode = 0;
        }
        parts->unk_dobjtrans_word = 0;
    }
    child = root_dobj->child;

    if (child != NULL)
    {
        current = child;
    }
    else if (origin == root_dobj)
    {
        current = NULL;
    }
    else
    {
        sibling = root_dobj->sib_next;

        if (sibling != NULL)
        {
            current = sibling;
        }
        else while (TRUE)
        {
            parent = origin->parent;

            if (root_dobj == parent)
            {
                current = NULL;

                break;
            }
            else
            {
                parent_sibling = parent->sib_next;

                if (parent_sibling != NULL)
                {
                    current = parent_sibling;

                    break;
                }
                else origin = parent;
            }
        }
    }
    while (current != NULL)
    {
        current_parts = current->user_data.p;

        if (current_parts != NULL)
        {
            current_parts->unk_dobjtrans_word = 0;
        }
        current_child = current->child;

        if (current_child != NULL)
        {
            current = current_child;
        }
        else if (current == root_dobj)
        {
            current = NULL;
        }
        else
        {
            current_sibling = current->sib_next;

            if (current_sibling != NULL)
            {
                current = current_sibling;
            }
            else while (TRUE)
            {
                current_parent = current->parent;

                if (root_dobj == current_parent)
                {
                    current = NULL;

                    break;
                }
                else
                {
                    current_parent_sibling = current_parent->sib_next;

                    if (current_parent_sibling != NULL)
                    {
                        current = current_parent_sibling;

                        break;
                    }
                    else current = current_parent;
                }
            }
        }
    }
}

/* ft/ftparam.c:2283-2350 ftParamsUpdateFighterPartsTransform 0x800EB648,
 * verbatim: the same walk, unlocking mode 1 on every joint */
void ftParamsUpdateFighterPartsTransform(DObj *root_dobj)
{
    DObj *parent_sibling;
    DObj *child;
    DObj *sibling;
    DObj *parent;
    DObj *current;
    FTParts *parts;

    current = root_dobj;

    while (current != NULL)
    {
        parts = current->user_data.p;

        if (parts != NULL)
        {
            if (parts->transform_update_mode == 1)
            {
                parts->transform_update_mode = 0;
            }
            parts->unk_dobjtrans_word = 0;
        }
        child = current->child;

        if (child != NULL)
        {
            current = child;
        }
        else if (current == root_dobj)
        {
            current = NULL;
        }
        else
        {
            sibling = current->sib_next;

            if (sibling != NULL)
            {
                current = sibling;
            }
            else while (TRUE)
            {
                parent = current->parent;

                if (root_dobj == parent)
                {
                    current = NULL;

                    break;
                }
                else
                {
                    parent_sibling = parent->sib_next;

                    if (parent_sibling != NULL)
                    {
                        current = parent_sibling;

                        break;
                    }
                    else current = parent;
                }
            }
        }
    }
}

/* ft/ftparam.c:2352-2395 ftParamSetAnimLocks 0x800EB6EC, verbatim: the
 * joints FTAttributes.animlock names freeze their world matrix at the
 * pose they have now (mode 3). The XObj flag the game sets beside it
 * is what its display walk reads to stop animating the joint; the
 * port's display walk (src/dc/fighter.c build_mtx) does not read it
 * yet, so a locked joint still animates on screen while its hitboxes
 * and hurtboxes hold -- noted in ftcommon.h. */
void ftParamSetAnimLocks(FTStruct *fp)
{
    u32 flags0;
    u32 flags1;
    u32 current_flags;
    FTParts *parts;
    s32 i;
    u32 *animlock;
    FTAttributes *attr = fp->attr;

    animlock = attr->animlock;
    flags0 = animlock[0];
    flags1 = animlock[1];

    for (i = nFTPartsJointCommonStart; ((flags0 != 0) || (flags1 != 0)); i++)
    {
        if (i < (ARRAY_COUNT(fp->joints) - 1))
        {
            current_flags = flags0;
        }
        else current_flags = flags1;

        if (current_flags & (1 << 31))
        {
            if (fp->joints[i] != NULL)
            {
                parts = fp->joints[i]->user_data.p;

                if (parts != NULL)
                {
                    gmCollisionTransformMatrixAll(fp->joints[i], parts, parts->unk_dobjtrans_0x10);
                    parts->transform_update_mode = 3;
                    fp->joints[i]->xobjs[0]->unk05 = 1;
                }
            }
        }
        if (i < ARRAY_COUNT(fp->joints) - 1)
        {
            flags0 <<= 1;
        }
        else flags1 <<= 1;
    }
}

/* ft/ftparam.c:2397-2418 ftParamClearAnimLocks 0x800EB7F4, verbatim */
void ftParamClearAnimLocks(FTStruct *fp)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(fp->joints); i++)
    {
        if (fp->joints[i] != NULL)
        {
            FTParts *parts = fp->joints[i]->user_data.p;

            if (parts != NULL)
            {
                if (parts->transform_update_mode == 3)
                {
                    parts->transform_update_mode = 0;

                    fp->joints[i]->xobjs[0]->unk05 = 0;
                }
            }
        }
    }
}

/* ft/ftparam.c:1783-2112, verbatim: the gate. A motion
 * script's effect event ends in ftMainPlayAnimEvents' effect arm, which
 * calls ftParamMakeEffect with the event's effect id, joint, offset and
 * scatter; this switch is what turns that id into an effect. It is the
 * only caller most of ef/efmanager.c's makers have, which is why thirty of
 * them are reached only from here.
 *
 * All forty-five arms are the decomp's text. (The
 * star rod's spark and Yoshi's egg escape need no new
 * model -- efManagerStarRodSparkMakeEffect reuses the damage
 * sparks' own efspark.mdl and
 * efManagerYoshiEggEscapeMakeEffect reuses the Yoshi shield's own
 * efyegg.mdl. The small shock burst and the fire spark
 * use efshock.mdl and effirespark.mdl, both
 * out of EFCommonEffects2's own file at different offsets. The last,
 * func_ovl2_8010183C (nEFKindCrashTheGame -- an unnamed,
 * likely debug-only kind, never observed reachable from normal input),
 * is ported and wired but stays genuinely unmodeled: its EFDesc reads
 * gFTDataPikachuSpecial2, a real global ft/ftchar/ftpikachu/ftpikachu.c
 * defines, but nothing in the port ever assigns
 * it -- no fighter's own Special-file streaming exists yet, for any of
 * the twelve -- so it carries no sEFManagerModels row.) */
// 0x800EAB40
void ftParamGetEffectJointPosition(FTStruct *fp, Vec3f *pos)
{
    FTAttributes *attr = fp->attr;

    fp->effect_joint_array_id++;

    if (fp->effect_joint_array_id == ARRAY_COUNT(attr->effect_joint_ids))
    {
        fp->effect_joint_array_id = 0;
    }
    pos->x = pos->y = pos->z = 0.0F;

    gmCollisionGetFighterPartsWorldPosition(fp->joints[attr->effect_joint_ids[fp->effect_joint_array_id]], pos);
}

// 0x800EABDC
void* ftParamMakeEffect(GObj *fighter_gobj, s32 effect_id, s32 joint_id, Vec3f *effect_pos, Vec3f *effect_scatter, s32 lr, sb32 is_scale_pos, u32 arg7)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    Vec3f pos;
    Vec3f effect_pos_mod;
    void *effect;
    f32 scale;
    LBParticle *pc;

    effect = NULL;

    switch (effect_id)
    {
    case nEFKindChargeSparkle:
        switch (fp->fkind)
        {
        case nFTKindSamus:
            joint_id = FTSAMUS_CHARGE_EFFECT_JOINT;

            effect_pos_mod.z = effect_pos_mod.y = 0.0F;
            effect_pos_mod.x = 180.0F;

            effect_pos = &effect_pos_mod;
            break;

        case nFTKindDonkey:
            joint_id = FTDONKEY_CHARGE_EFFECT_JOINT;

            effect_pos_mod.z = effect_pos_mod.y = 0.0F;
            effect_pos_mod.x = 100.0F;

            effect_pos = &effect_pos_mod;
            break;

        case nFTKindKirby:
            joint_id = FTKIRBY_CHARGE_EFFECT_JOINT;

            effect_pos_mod.z = effect_pos_mod.y = 0.0F;
            effect_pos_mod.x = 50.0F;

            effect_pos = &effect_pos_mod;
            break;

        case nFTKindGDonkey:
            joint_id = FTDONKEY_CHARGE_EFFECT_JOINT;

            effect_pos_mod.z = effect_pos_mod.y = 0.0F;
            effect_pos_mod.x = 100.0F;

        default: // Falthrough for final case; effect_pos becomes uninitialized data if jumping straight to default
        #if !defined (AVOID_UB)
            effect_pos = &effect_pos_mod;
        #endif
            break;
        }
        break;

    default:
        break;
    }
    if (joint_id != -1)
    {
        if (effect_pos != NULL)
        {
            pos = *effect_pos;
        }
        else pos.x = pos.y = pos.z = 0.0F;

        if (effect_scatter != NULL)
        {
            if (effect_scatter->x != 0)
            {
                pos.x += (syUtilsRandFloat() - 0.5F) * (effect_scatter->x * 2.0F);
            }
            if (effect_scatter->y != 0)
            {
                pos.y += (syUtilsRandFloat() - 0.5F) * (effect_scatter->y * 2.0F);
            }
            if (effect_scatter->z != 0)
            {
                pos.z += (syUtilsRandFloat() - 0.5F) * (effect_scatter->z * 2.0F);
            }
        }
        if (is_scale_pos != FALSE)
        {
            scale = 1.0F / fp->attr->size;

            pos.x *= scale;
            pos.y *= scale;
            pos.z *= scale;
        }
#ifdef FT_HOSTTEST
        /* The host's mock fighter is a three-DObj tree; a colour
         * animation's effect on a joint it lacks is placed at TopN. Every
         * pack on the target has the joint. */
        if (fp->joints[joint_id] == NULL)
        {
            joint_id = nFTPartsJointTopN;
        }
#endif
        gmCollisionGetFighterPartsWorldPosition(fp->joints[joint_id], &pos);
    }
    switch (effect_id)
    {
    case nEFKindDamageNormal:
        ftParamGetEffectJointPosition(fp, &pos);
        effect = efManagerDamageNormalLightMakeEffect(&pos, fp->player, 10, FALSE);
        break;

    case nEFKindFlameLR:
        ftParamGetEffectJointPosition(fp, &pos);
        effect = efManagerFlameLRMakeEffect(&pos, lr);
        break;

    case nEFKindFlameRandom:
        ftParamGetEffectJointPosition(fp, &pos);
        effect = efManagerFlameRandomMakeEffect(&pos);
        break;

    case nEFKindFlameStatic:
        ftParamGetEffectJointPosition(fp, &pos);
        effect = efManagerFlameStaticMakeEffect(&pos);
        break;

    case nEFKindShockSmall:
        ftParamGetEffectJointPosition(fp, &pos);
        effect = efManagerShockSmallMakeEffect(&pos);
        break;

    case nEFKindDustLight:
        effect = efManagerDustLightMakeEffect(&pos, lr, 1.0F);
        break;

    case nEFKindDustLightRapid:
        effect = efManagerDustLightMakeEffect(&pos, lr, 2.0F);
        break;

    case nEFKindDustHeavyDouble:
        effect = efManagerDustHeavyDoubleMakeEffect(&pos, lr, 1.0F);
        break;

    case nEFKindDustHeavyDoubleRapid:
        effect = efManagerDustHeavyDoubleMakeEffect(&pos, lr, 1.7F);
        break;

    case nEFKindDustHeavy:
        effect = efManagerDustHeavyMakeEffect(&pos, lr);
        break;

    case nEFKindDustHeavyReverse:
        effect = efManagerDustHeavyMakeEffect(&pos, -lr);
        break;

    case nEFKindDustExpandLarge:
        pos.x += ((syUtilsRandFloat() * 160.0F) - 80.0F);
        pos.y += ((syUtilsRandFloat() * 160.0F) - 80.0F);

        effect = efManagerDustExpandLargeMakeEffect(&pos);
        break;

    case nEFKindDustExpandSmall:
        effect = efManagerDustExpandSmallMakeEffect(&pos, 1.0F);
        break;

    case nEFKindDustDashSmall:
        effect = efManagerDustDashMakeEffect(&pos, lr, 1.0F);
        break;

    case nEFKindDustDashLarge:
        effect = efManagerDustDashMakeEffect(&pos, lr, 1.5F);
        break;

    case nEFKindDamageFlyOrbs:
        effect = efManagerDamageSpawnOrbsMakeEffect(&pos);
        break;

    case nEFKindImpactWave:
        if ((fp->ga == nMPKineticsGround) && (fp->coll_data.floor_line_id != -1) && (fp->coll_data.floor_line_id != -2))
        {
            effect = efManagerImpactWaveMakeEffect(&pos, 4, syUtilsArcTan2(-fp->coll_data.floor_angle.x, fp->coll_data.floor_angle.y));
        }
        else effect = efManagerImpactAirWaveMakeEffect(&pos, 4);
        break;

    case nEFKindStarRodSpark:
        effect = efManagerStarRodSparkMakeEffect(&pos, -lr);
        break;

    case nEFKindDamageFlySparks:
        effect = efManagerDamageSpawnSparksMakeEffect(&pos, lr);
        break;

    case nEFKindDamageFlySparksReverse:
        effect = efManagerDamageSpawnSparksMakeEffect(&pos, -lr);
        break;

    case nEFKindDamageFlyMDust:
        effect = efManagerDamageSpawnMDustMakeEffect(&pos, lr);
        break;

    case nEFKindDamageFlyMDustReverse:
        effect = efManagerDamageSpawnMDustMakeEffect(&pos, -lr);
        break;

    case nEFKindSparkleWhite:
        effect = efManagerSparkleWhiteMakeEffect(&pos);
        break;

    case nEFKindSparkleWhiteMultiExplode:
        effect = efManagerSparkleWhiteMultiExplodeMakeEffect(&pos);
        break;

    case nEFKindSparkleWhiteMulti:
        effect = efManagerSparkleWhiteMultiMakeEffect(&pos);
        break;

    case nEFKindSparkleWhiteScale:
        effect = efManagerSparkleWhiteScaleMakeEffect(&pos, 1.0F);
        break;

    case nEFKindQuakeMag0:
        if (fp->pkind != nFTPlayerKindDemo)
        {
            effect = efManagerQuakeMakeEffect(0);
        }
        break;

    case nEFKindQuakeMag1:
        if (fp->pkind != nFTPlayerKindDemo)
        {
            effect = efManagerQuakeMakeEffect(1);
        }
        break;

    case nEFKindQuakeMag2:
        if (fp->pkind != nFTPlayerKindDemo)
        {
            effect = efManagerQuakeMakeEffect(2);
        }
        break;

    case nEFKindPsionic:
        effect = efManagerPsionicMakeEffect(&pos);
        break;

    case nEFKindFlashSmall:
        effect = efManagerFlashSmallMakeEffect(&pos);
        break;

    case nEFKindFlashMiddle:
        effect = efManagerFlashMiddleMakeEffect(&pos);
        break;

    case nEFKindFlashLarge:
        effect = efManagerFlashLargeMakeEffect(&pos);
        break;

    case nEFKindFuraSparkle:
        effect = efManagerFuraSparkleMakeEffect(&pos);
        break;

    case nEFKindKirbyStar:
        effect = efManagerKirbyStarMakeEffect(&pos);
        break;

    case nEFKindCrashTheGame:
        effect = func_ovl2_8010183C(&pos, arg7);
        break;

    case nEFKindChargeSparkle:
        pc = efManagerSparkleWhiteScaleMakeEffect(&pos, 0.7F);

        if (pc != NULL)
        {
            pc->primcolor.a = 0xC0;
        }
        break;

    case nEFKindThunderAmp:
        effect = efManagerThunderAmpMakeEffect(&pos);
        break;

    case nEFKindRipple:
        effect = efManagerRippleMakeEffect(&pos);
        break;

    case 0x4C:
        effect = func_ovl2_801031E0(&pos);
        break;

    case 0x4D:
        effect = func_ovl2_80103280(&pos);
        break;

    case nEFKindHealSparkles:
        effect = efManagerHealSparklesMakeEffect(&pos);
        break;

    case nEFKindBoxSmash:
        itBoxContainerSmashMakeEffect(&pos);
        break;

    case nEFKindMusicNote:
        efManagerMusicNoteMakeEffect(&pos);
        break;

    case nEFKindEggBreak:
        efManagerEggBreakMakeEffect(&pos);
        break;

    case nEFKindYoshiEggEscape:
        if (fp->fkind == nFTKindYoshi)
        {
            efManagerYoshiEggEscapeMakeEffect(fighter_gobj);
        }
        break;

    case nEFKindFireSpark:
        effect = efManagerFireSparkMakeEffect(fighter_gobj);
        break;
    }
    return effect;
}

/* ft/ftparam.c:2114-2159 ftParamKirbyTryMakeMapStarEffect 0x800EB39C,
 * verbatim: the stars Kirby throws off a wall, ceiling
 * or floor he newly touches, run from ftMainProcPhysicsMap. */
void ftParamKirbyTryMakeMapStarEffect(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    u16 mask = (fp->coll_data.mask_prev ^ fp->coll_data.mask_curr) & fp->coll_data.mask_curr & MAP_FLAG_MAIN_MASK;
    Vec3f pos;

    if (mask)
    {
        if (mask & MAP_FLAG_LWALL)
        {
            pos = DObjGetStruct(fighter_gobj)->translate.vec.f;

            pos.x += fp->coll_data.map_coll.width;
            pos.y += fp->coll_data.map_coll.center;

            efManagerKirbyStarMakeEffect(&pos);
        }
        if (mask & MAP_FLAG_RWALL)
        {
            pos = DObjGetStruct(fighter_gobj)->translate.vec.f;

            pos.x -= fp->coll_data.map_coll.width;
            pos.y += fp->coll_data.map_coll.center;

            efManagerKirbyStarMakeEffect(&pos);
        }
        if (mask & MAP_FLAG_CEIL)
        {
            pos = DObjGetStruct(fighter_gobj)->translate.vec.f;

            pos.y += fp->coll_data.map_coll.top;

            efManagerKirbyStarMakeEffect(&pos);
        }
        if (mask & MAP_FLAG_FLOOR)
        {
            pos = DObjGetStruct(fighter_gobj)->translate.vec.f;

            pos.y += fp->coll_data.map_coll.bottom;

            efManagerKirbyStarMakeEffect(&pos);
        }
    }
}

/* ---- the leaves: effects, parts, colour animations ------------------ */


/* ft/ftparam.c:1354-1423, verbatim: the seven functions that reach a
 * fighter's own effects. Every one of them walks the effect link for the
 * GObjs whose EFStruct names this fighter, so all seven need
 * ef/efmanager.c's pool.
 *
 * It matters most for the rebirth halo: the halo is made
 * when the fighter drops onto the platform and it is ftMainSetStatus's
 * call to ftParamProcStopEffect that takes it away again when the
 * fighter leaves RebirthWait. Without these the halo would be made once
 * per death and never ejected.
 *
 * ftParamStopEffect ejects the effect's particles too, by generator id,
 * which is why lbParticleEjectStructID is the one thing here that is not
 * the object system. */
// 0x800E9B64
void ftParamRunProcEffect(GObj *fighter_gobj, void (*proc)(GObj*, EFStruct*))
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->is_effect_attach)
    {
        GObj *effect_gobj = gGCCommonLinks[nGCCommonLinkIDEffect];

        while (effect_gobj != NULL)
        {
            EFStruct *ep = efGetStruct(effect_gobj);

            GObj *next_effect_gobj = effect_gobj->link_next;

            if ((ep != NULL) && (fighter_gobj == ep->fighter_gobj))
            {
                proc(effect_gobj, ep);
            }
            effect_gobj = next_effect_gobj;
        }
    }
}

// 0x800E9BE8
void ftParamStopEffect(GObj *effect_gobj, EFStruct *ep)
{
    LBTransform *einfo = ep->xf;

    if (einfo != NULL)
    {
        lbParticleEjectStructID(einfo->generator_id, ep->bank_id >> 3);
    }
    efManagerSetPrevStructAlloc(ep);
    gcEjectGObj(effect_gobj);
}

// 0x800E9C3C
void ftParamProcStopEffect(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftParamRunProcEffect(fighter_gobj, ftParamStopEffect);

    fp->is_effect_attach = FALSE;
}

// 0x800E9C78
void ftParamPauseEffect(GObj *effect_gobj, EFStruct *ep)
{
    ep->is_pause_effect = TRUE;
}

// 0x800E9C78
void ftParamProcPauseEffect(GObj *effect_gobj)
{
    ftParamRunProcEffect(effect_gobj, ftParamPauseEffect);
}

// 0x800E9CB0
void ftParamResumeEffect(GObj *effect_gobj, EFStruct *ep)
{
    ep->is_pause_effect = FALSE;
}

// 0x800E9CC4
void ftParamProcResumeEffect(GObj *fighter_gobj)
{
    ftParamRunProcEffect(fighter_gobj, ftParamResumeEffect);
}

/* ft/ftparam.c:1425-1449 ftParamVelDamageTransferGround 0x800E9CE8, verbatim:
 * on the first frame after a launch lands, the horizontal launch velocity is
 * folded into the along-the-floor damage velocity (clamped +/-250) and re-split
 * along the floor normal, so a knockdown slides in the direction it was flying.
 * Ported for the knockdown floor; used by ftCommonDownBounceSetStatus. */
void ftParamVelDamageTransferGround(FTStruct *fp)
{
    if (fp->ga == nMPKineticsGround)
    {
        Vec3f *floor_angle = &fp->coll_data.floor_angle;

        if (fp->physics.vel_damage_ground == 0.0F)
        {
            fp->physics.vel_damage_ground = fp->physics.vel_damage_air.x;

            if (fp->physics.vel_damage_ground > 250.0F)
            {
                fp->physics.vel_damage_ground = 250.0F;
            }
            if (fp->physics.vel_damage_ground < -250.0F)
            {
                fp->physics.vel_damage_ground = -250.0F;
            }
            fp->physics.vel_damage_air.x = floor_angle->y * fp->physics.vel_damage_ground;
            fp->physics.vel_damage_air.y = -floor_angle->x * fp->physics.vel_damage_ground;
        }
    }
}

/* The port's read of a joint's FTModelPart row for its MObjs: the pack's
 * animated part MObjs (dc_model_part_mobjs) where the joint has parts and
 * draws one, NULL for both otherwise -- the commonparts arm and a row with
 * no display list, whose costume MObjs the pack bakes. */
static void ftParamGetPartMObjs(GObj *fighter_gobj, DObj *joint, s32 k, MObjSub ***mobjsubs, AObjEvent32 ***main_matanim_joints)
{
    Fighter *model = dc_model_of(fighter_gobj);

    *mobjsubs = NULL;
    *main_matanim_joints = NULL;

    if ((model != NULL) && (joint->dv != NULL) && dc_model_has_parts(model, k))
    {
        dc_model_part_mobjs(model, k, mobjsubs, main_matanim_joints);
    }
}

/* ft/ftparam.c:748-818 ftParamSetModelPartID 0x800E8C70.
 * The control flow is the decomp's; what DIVERGES is what a part is. The
 * game puts the FTModelPart row's display list on the joint's DObj and
 * adds its MObjs; the pack has no display lists, only batches the
 * exporter baked for every part and tagged (src/dc/fighter.h
 * FPackParts), so the DObj gets the payload that draws the part's tag
 * instead (dc_model_part_display), and a row with no display list leaves
 * it NULL exactly as the game's `joint->dl = NULL` does. The row's flags
 * come with it. `modelparts_desc[i] != NULL` is dc_model_has_parts, and
 * the commonparts arm -- a joint without parts, back to its tree entry --
 * is the payload dc_model_add_dobjs gave it. The detail level is the
 * pack's one, high: the part ids are the rows the exporter baked.
 *
 * ftGetParts(joint) moves below the NULL check: the game reads a NULL
 * joint's user_data without faulting, and the Dreamcast would. The MObjs
 * lbCommonAddMObjForFighterPartsDObj adds are the pack's for a part that
 * plays material scripts of its own (dc_model_part_mobjs: Samus's grapple
 * beam) and none for any other, whose costume MObjs are baked; a part the
 * pack draws nothing for adds none either. */
void ftParamSetModelPartID(GObj *fighter_gobj, s32 joint_id, s32 modelpart_id)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTModelPartStatus *modelpart_status;
    FTParts *parts;
    DObj *joint;
    MObjSub **mobjsubs;
    AObjEvent32 **main_matanim_joints;
    u8 flags;

    joint = fp->joints[joint_id];
    modelpart_status = &fp->modelpart_status[joint_id - nFTPartsJointCommonStart];

    if (joint != NULL)
    {
        parts = ftGetParts(joint);

        if (modelpart_status->modelpart_id_curr != modelpart_id)
        {
            modelpart_status->modelpart_id_curr = modelpart_id;

            gcRemoveMObjAll(joint);

            if (modelpart_id != -1)
            {
                joint->dv = dc_model_part_display(dc_model_of(fighter_gobj), joint_id - nFTPartsJointCommonStart, modelpart_id, &flags);

                ftParamGetPartMObjs(fighter_gobj, joint, joint_id - nFTPartsJointCommonStart, &mobjsubs, &main_matanim_joints);
                lbCommonAddMObjForFighterPartsDObj(joint, mobjsubs, NULL, main_matanim_joints, fp->costume);

                parts->flags = flags;
            }
            else joint->dl = NULL;

            fp->is_modelpart_modify = TRUE;
        }
    }
}

/* ft/ftparam.c:820-828 ftParamSetModelPartDefaultID 0x800E8E9C, VERBATIM
 * -- and note it is not cut the way its neighbour above is.
 * The neighbour walks FTAttributes' part containers to swap a joint's
 * display list, which the pack cannot do; this one only writes the
 * default id into fp->modelpart_status and raises the modify flag, and
 * that field is the decomp's own FTStruct, so the write costs nothing and
 * keeps the state honest even while nothing draws from it.
 *
 * Kirby's copy inhale is what wanted it: ftKirbySpecialNCopyInitCopyVars
 * sets the mouth Kirby wears by writing the swallowed fighter's
 * copy_modelpart_id through here. */
void ftParamSetModelPartDefaultID(GObj *fighter_gobj, s32 joint_id, s32 modelpart_id)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->modelpart_status[joint_id - nFTPartsJointCommonStart].modelpart_id_base = modelpart_id;

    fp->is_modelpart_modify = TRUE;
}

/* ft/ftparam.c:830-893 ftParamResetModelPartAll 0x800E8ECC, as
 * ftParamSetModelPartID above: every joint back to its base part. */
void ftParamResetModelPartAll(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTModelPartStatus *modelpart_status;
    FTParts *parts;
    DObj *joint;
    MObjSub **mobjsubs;
    AObjEvent32 **main_matanim_joints;
    s32 i;
    u8 flags;

    for (i = 0; i < ARRAY_COUNT(fp->joints) - nFTPartsJointCommonStart; i++)
    {
        joint = fp->joints[i + nFTPartsJointCommonStart];

        if (joint != NULL)
        {
            modelpart_status = &fp->modelpart_status[i];

            if (modelpart_status->modelpart_id_curr != modelpart_status->modelpart_id_base)
            {
                modelpart_status->modelpart_id_curr = modelpart_status->modelpart_id_base;

                gcRemoveMObjAll(joint);

                if (modelpart_status->modelpart_id_curr == -1)
                {
                    joint->dl = NULL;
                }
                else
                {
                    parts = ftGetParts(joint);

                    joint->dv = dc_model_part_display(dc_model_of(fighter_gobj), i, modelpart_status->modelpart_id_curr, &flags);

                    ftParamGetPartMObjs(fighter_gobj, joint, i, &mobjsubs, &main_matanim_joints);
                    lbCommonAddMObjForFighterPartsDObj(joint, mobjsubs, NULL, main_matanim_joints, fp->costume);

                    parts->flags = flags;
                }
            }
        }
    }
    fp->is_modelpart_modify = FALSE;
}

/* ft/ftparam.c:895-926 ftParamHideModelPartAll 0x800E90F8, verbatim. */
void ftParamHideModelPartAll(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 i;

    for (i = nFTPartsJointCommonStart; i < ARRAY_COUNT(fp->joints); i++)
    {
        DObj *joint = fp->joints[i];

        if (joint != NULL)
        {
            if (fp->modelpart_status[i - nFTPartsJointCommonStart].modelpart_id_curr == -1) 
            {
                continue;
            }
            else
            {
                fp->modelpart_status[i - nFTPartsJointCommonStart].modelpart_id_curr = -1;

                gcRemoveMObjAll(joint);

                joint->dl = NULL;
            }
        }
    }
    fp->is_modelpart_modify = TRUE;
}

/* ft/ftparam.c:1052-1145 ftParamInitTexturePartAll 0x800E9598,
 * ftParamSetTexturePartID 0x800E962C and ftParamResetTexturePartAll
 * 0x800E96B0. The control flow is the decomp's. DIVERGES:
 * each walks the joint's MObj chain to the part's detail and writes the
 * MObj's texture_id_curr; the pack has no MObjs, and bakes a tile per
 * frame beside the batches that MObj drew (src/dc/fighter.h
 * FPackTexParts), so the write is the fighter's model putting the part on
 * the frame. "No MObj at that detail" is the pack having no frames for
 * the part (fighter_set_texture_frame), and leaves everything alone as
 * the game's walk does. Donkey Kong and Samus have no container, which
 * the game never asks about; the port returns rather than reading one
 * that is not there. */
void ftParamInitTexturePartAll(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTTexturePartStatus *texturepart_status;
    FTTexturePart *texturepart;
    Fighter *model = dc_model_of(fighter_gobj);
    DObj *joint;
    s32 i;

    if (fp->attr->textureparts_container == NULL)
    {
        return;
    }
    for (i = 0, texturepart_status = &fp->texturepart_status[i], texturepart = &fp->attr->textureparts_container->textureparts[i]; i < ARRAY_COUNT(fp->texturepart_status); i++, texturepart_status++, texturepart++)
    {
        if (texturepart_status->texture_id_curr != texturepart_status->texture_id_base)
        {
            joint = fp->joints[texturepart->joint_id];

            if ((joint != NULL) && (model != NULL))
            {
                fighter_set_texture_frame(model, i, texturepart_status->texture_id_curr);
            }
        }
    }
    fp->is_texturepart_modify = TRUE;
}

void ftParamSetTexturePartID(GObj *fighter_gobj, s32 texturepart_id, s32 texture_id)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTTexturePart *texturepart;
    Fighter *model = dc_model_of(fighter_gobj);
    DObj *joint;

    if (fp->attr->textureparts_container == NULL)
    {
        return;
    }
    texturepart = &fp->attr->textureparts_container->textureparts[texturepart_id];
    joint = fp->joints[texturepart->joint_id];

    if ((joint != NULL) && (model != NULL))
    {
        if (fighter_set_texture_frame(model, texturepart_id, texture_id))
        {
            fp->texturepart_status[texturepart_id].texture_id_curr = texture_id;

            fp->is_texturepart_modify = TRUE;
        }
    }
}

void ftParamResetTexturePartAll(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTTexturePartStatus *texturepart_status;
    FTTexturePart *texturepart;
    Fighter *model = dc_model_of(fighter_gobj);
    DObj *joint;
    s32 i;

    if (fp->attr->textureparts_container == NULL)
    {
        fp->is_texturepart_modify = FALSE;
        return;
    }
    for (i = 0, texturepart_status = &fp->texturepart_status[i], texturepart = &fp->attr->textureparts_container->textureparts[i]; i < ARRAY_COUNT(fp->texturepart_status); i++, texturepart_status++, texturepart++)
    {
        if (texturepart_status->texture_id_curr != texturepart_status->texture_id_base)
        {
            texturepart_status->texture_id_curr = texturepart_status->texture_id_base;

            joint = fp->joints[texturepart->joint_id];

            if ((joint != NULL) && (model != NULL))
            {
                fighter_set_texture_frame(model, i, texturepart_status->texture_id_curr);
            }
        }
    }
    fp->is_texturepart_modify = FALSE;
}

/* ft/ftparam.c:1191-1214 ftParamCheckSetColAnimID 0x800E974C, verbatim:
 * a colour animation starts if its script's priority is at least the
 * running one's (gm/gmcolscripts.c dGMColScriptsDescs, the decomp's
 * data compiled unmodified ). Generic over the GMColAnim
 * it is handed: if/ifscreenflash.c's, an item's and each fighter's. */
// 0x800E974C
sb32 ftParamCheckSetColAnimID(GMColAnim *colanim, s32 colanim_id, s32 length)
{
    if (dGMColScriptsDescs[colanim_id].priority >= dGMColScriptsDescs[colanim->colanim_id].priority)
    {
        s32 i;

        colanim->colanim_id = colanim_id;
        colanim->length = length;
        colanim->cs[0].p_script = dGMColScriptsDescs[colanim_id].p_script;
        colanim->cs[0].color_event_timer = 0;
        colanim->cs[0].script_id = 0;

        for (i = 1; i < ARRAY_COUNT(colanim->cs); i++)
        {
            colanim->cs[i].p_script = NULL;
        }
        colanim->is_use_color1 = colanim->is_use_light = colanim->is_use_color2 = colanim->skeleton_id = 0;

        return TRUE;
    }
    return FALSE;
}

/* ft/ftparam.c:1216-1222 ftParamCheckSetFighterColAnimID 0x800E9814,
 * verbatim: the fighter's own colour animation. The
 * scripts do more than tint -- they play sounds, make the fire, electric,
 * charge, heal and fast-fall effects, and set the light angle -- so they
 * run whether or not the tint is drawn. */
// 0x800E9814
sb32 ftParamCheckSetFighterColAnimID(GObj *fighter_gobj, s32 colanim_id, s32 length)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    return ftParamCheckSetColAnimID(&fp->colanim, colanim_id, length);
}

/* ft/ftparam.c:17-31 dFTParamSkeletonColAnimIDs 0x8012B7B4 and
 * :1325-1331 ftParamCheckSetSkeletonColAnimID 0x800E9AF4, verbatim: the
 * electric hit's colanim, whose id is per fighter kind. */
// 0x8012B7B4
s32 dFTParamSkeletonColAnimIDs[/* */] = 
{
    0x14,   // Mario
    0x14,   // Fox
    0x14,   // Donkey Kong
    0x1C,   // Samus
    0x14,   // Luigi
    0x14,   // Link
    0x14,   // Yoshi
    0x14,   // Captain Falcon
    0x18,   // Kirby
    0x14,   // Pikachu
    0x18,   // Jigglypuff
    0x14,   // Ness
    0x10,   // Master Hand
    0x10,   // Metal Mario
    0x10,   // Poly Mario
    0x10,   // Poly Fox 
    0x10,   // Poly Donkey Kong
    0x10,   // Poly Samus
    0x10,   // Poly Luigi
    0x10,   // Poly Link
    0x10,   // Poly Yoshi
    0x10,   // Poly Captain Falcon
    0x10,   // Poly Kirby
    0x10,   // Poly Pikachu
    0x10,   // Poly Jigglypuff
    0x10,   // Poly Ness
    0x10    // Giant Donkey Kong
};

// 0x800E9AF4
sb32 ftParamCheckSetSkeletonColAnimID(GObj *fighter_gobj, s32 damage_level)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    return ftParamCheckSetFighterColAnimID(fighter_gobj, dFTParamSkeletonColAnimIDs[fp->fkind] + damage_level, 0);
}

/* ft/ftparam.c:1332-1341 ftParamSetKey/ftParamCheckHaveKey 0x800E9B30/
 * 0x800E9B40, verbatim: arm (or query) a Key/GameKey fighter's canned
 * input script for ftKeyProcessKeyEvents (src/dc/ftcommon.c) to walk. */
// 0x800E9B30 - Set automatic input sequence
void ftParamSetKey(GObj *fighter_gobj, FTKeyEvent *script)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->key.input_wait = 0;
    fp->key.script = script;
}

// 0x800E9B40 - Check if automatic input sequence exists
sb32 ftParamCheckHaveKey(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->key.script != NULL)
    {
        return TRUE;
    }
    else return FALSE;
}

/* ft/ftparam.c:226-231 ftParamSetPlayerTagWait, verbatim */
void ftParamSetPlayerTagWait(GObj *fighter_gobj, s32 playertag_wait)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    fp->playertag_wait = playertag_wait;
}

/* ft/ftparam.c:1699-1730 ftParamSetTimedHitStatusInvincible 0x800EA894,
 * verbatim */
void ftParamSetTimedHitStatusInvincible(FTStruct *fp, s32 invincible_tics)
{
    if (fp->invincible_tics < invincible_tics)
    {
        fp->invincible_tics = invincible_tics;
    }
    if (fp->intangible_tics != 0)
    {
        fp->special_hitstatus = nGMHitStatusIntangible;
    }
    else fp->special_hitstatus = nGMHitStatusInvincible;

    ftParamCheckSetFighterColAnimID(fp->fighter_gobj, nGMColAnimFighterNoDamage, 0);
}

/* ft/ftparam.c:1706-1713 ftParamSetStarHitStatusInvincible 0x800EA8B0 and
 * :1556-1562 ftParamSetHealDamage 0x800EA3D4, verbatim: what a Star
 * Man and a Chansey egg do to the fighter they touch, called from
 * ftMainUpdateDamageStatItem's Touch arm. The star's own countdown
 * (star_invincible_tics) is already ticked by ftMainProcParams; the heal
 * lands through damage_heal the same way the Maxim Tomato's does. */
void ftParamSetStarHitStatusInvincible(FTStruct *fp, s32 star_invincible_tics)
{
    fp->star_hitstatus = nGMHitStatusInvincible;
    fp->star_invincible_tics = star_invincible_tics;

    ftParamCheckSetFighterColAnimID(fp->fighter_gobj, nGMColAnimFighterStar, 0);
}

void ftParamSetHealDamage(FTStruct *fp, s32 heal)
{
    fp->damage_heal += heal;

    ftParamCheckSetFighterColAnimID(fp->fighter_gobj, nGMColAnimFighterHeal, 0);
}

/* ft/ftparam.c:1732-1741 ftParamSetTimedHitStatusIntangible 0x800EA948,
 * verbatim: raise the fighter's intangibility timer to at least
 * intangible_tics and set its hit-status intangible. The wall
 * bounce grants FTCOMMON_WALLDAMAGE_INTANGIBLE_TIMER (15) frames so a
 * slammed fighter is not re-hit against the wall. */
void ftParamSetTimedHitStatusIntangible(FTStruct *fp, s32 intangible_tics)
{
    if (fp->intangible_tics < intangible_tics)
    {
        fp->intangible_tics = intangible_tics;
    }
    fp->special_hitstatus = nGMHitStatusIntangible;

    ftParamCheckSetFighterColAnimID(fp->fighter_gobj, nGMColAnimFighterNoDamage, 0);
}

/* ft/ftparam.c:? ftParamGetPlayerNumGObj: the fighter link list
 * (already the game's own, sys/objman.c), walked by player_num rather
 * than by port -- damage_player_num survives a KO/respawn where a port
 * index would not. Verbatim. */
GObj *ftParamGetPlayerNumGObj(s32 player_num)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);

        if (player_num == fp->player_num)
        {
            return fighter_gobj;
        }
        else fighter_gobj = fighter_gobj->link_next;
    }
    return NULL;
}

/* ft/ftparam.c:1223-1236 ftParamResetColAnim 0x800E9838, verbatim: the screen
 * flash's reset, between scripts and at the end of
 * one. */
// 0x800E9838
void ftParamResetColAnim(GMColAnim *colanim)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(colanim->cs); i++)
    {
        colanim->cs[i].p_script = NULL;
    }
    colanim->length = 0;
    colanim->colanim_id = 0;

    colanim->is_use_color1 = colanim->is_use_light = colanim->is_use_color2 = colanim->skeleton_id = 0;
}

/* ft/ftparam.c:1238-1323 ftParamResetFighterColAnim 0x800E98B0 and
 * ftParamResetStatUpdateColAnim 0x800E98D4, and ft/ftmain.c:1203-1211
 * ftMainRunUpdateColAnim 0x800E11C8, verbatim: the
 * reset over fp->colanim, the standing colanims put back after it
 * (invincibility, a full charge, PSI Magnet, healing, the star, the
 * hammer), and the tic that runs fp->colanim through ftMainUpdateColAnim
 * until a script stops ending. */
// 0x800E98B0
void ftParamResetFighterColAnim(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftParamResetColAnim(&fp->colanim);
}

// 0x800E98D4
void ftParamResetStatUpdateColAnim(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftParamResetFighterColAnim(fighter_gobj);

    switch (ftParamGetBestHitStatusPart(fighter_gobj))
    {
    case nGMHitStatusInvincible:
        ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterHitStatusInvincible, 0);
        break;

    case nGMHitStatusIntangible:
        ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterHitStatusIntangible, 0);
        break;
    }
    switch (fp->fkind)
    {
    case nFTKindDonkey:
    case nFTKindNDonkey:
    case nFTKindGDonkey:
        if (fp->passive_vars.donkey.charge_level == FTDONKEY_GIANTPUNCH_CHARGE_MAX)
        {
            ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterCommonSpecialNCharge, 0);
        }
        break;

    case nFTKindSamus:
    case nFTKindNSamus:
        if (fp->passive_vars.samus.charge_level == FTSAMUS_CHARGE_MAX)
        {
            ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterCommonSpecialNCharge, 0);
        }
        break;

    case nFTKindKirby:
        if ((fp->passive_vars.kirby.copy_id == nFTKindSamus) || (fp->passive_vars.kirby.copy_id == nFTKindNSamus))
        {
            if (fp->passive_vars.kirby.copysamus_charge_level == FTKIRBY_COPYSAMUS_CHARGE_MAX)
            {
                ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterCommonSpecialNCharge, 0);
            }
        }
        if ((fp->passive_vars.kirby.copy_id == nFTKindDonkey) || (fp->passive_vars.kirby.copy_id == nFTKindNDonkey) || (fp->passive_vars.kirby.copy_id == nFTKindGDonkey))
        {
            if (fp->passive_vars.kirby.copydonkey_charge_level == FTKIRBY_COPYDONKEY_GIANTPUNCH_CHARGE_MAX)
            {
                ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterCommonSpecialNCharge, 0);
            }
        }
        break;

    case nFTKindNess:
    case nFTKindNNess:
        if (fp->is_absorb)
        {
            ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterNessSpecialLwHold, 0);
        }
        break;
    }
    if (fp->damage_heal != 0)
    {
        ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterHeal, 0);
    }
    if (fp->star_invincible_tics != 0)
    {
        ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterStar, 0);
    }
    if ((fp->invincible_tics != 0) || (fp->intangible_tics != 0))
    {
        ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterNoDamage, 0);
    }
    if (ftHammerCheckStatusHammerAll(fighter_gobj) != FALSE)
    {
        ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterHammer, 0);
    }
}

// 0x800E11C8
void ftMainRunUpdateColAnim(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

#ifdef FT_HOSTTEST
    /* The host cannot run a colour script that jumps: its copy of
     * gm/gmcolscripts.c has the address words zeroed, since a 64-bit
     * address does not fit the word (hosttest_gmcolscripts.c), and the
     * fighters' scripts jump. The target runs them; the host checks
     * what sets and resets fp->colanim. */
    return;
#endif

    while (ftMainUpdateColAnim(&fp->colanim, fighter_gobj, fp->is_muted, fp->is_effect_skip) != FALSE)
    {
        ftParamResetStatUpdateColAnim(fighter_gobj);
    }
}

/* ft/ftparam.c:354-360 ftParamMoveDLLink 0x800E827C, verbatim: gcMoveGObjDL
 * is what the character select uses, and the one
 * caller that matters, Captain Falcon's entrance, reads fp->dl_link back
 * to decide when to come home (src/dc/ftcommon.c
 * ftCaptainAppearStartProcUpdate). The field must be kept for that test.
 *
 * The link it moves him to is 1, which in the game is a background layer
 * and in this port is STAGE_DLLINK (src/dc/stage.h: the game gives the
 * stage's four layers 4, 6, 13 and 17; the port bakes them into one pack
 * on link 1 because all of it is behind everything else). The two agree
 * about what link 1 MEANS -- drawn first, behind the stage's own
 * geometry -- which is where the Blue Falcon drives in from. */
void ftParamMoveDLLink(GObj *fighter_gobj, u8 dl_link)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    gcMoveGObjDL(fighter_gobj, dl_link, GOBJ_PRIORITY_DEFAULT);

    fp->dl_link = dl_link;
}

/* ft/ftparam.c:93-150 ftParamGetItemMusicLength, ftParamTryPlayItemMusic
 * and ftParamTryUpdateItemMusic, verbatim: the star/hammer music override.
 * TryPlay is what a Star touching a fighter asks for
 * (ftMainUpdateDamageStatItem); TryUpdate is what
 * ftCommonDeadResetSpecialStats calls on every KO and itMainDestroy- Item when
 * a held Hammer goes, to put the stage's own track back. The length table is
 * the decomp's, Star and Hammer rows swapped as they are in the game (10 and
 * 20 seconds either way, so the longer one wins). */
s32 ftParamGetItemMusicLength(u32 bgm_id)
{
    switch (bgm_id)
    {
    case nSYAudioBGMStar:
        return ITHAMMER_BGM_DURATION;

    case nSYAudioBGMHammer:
        return ITSTAR_BGM_DURATION;

    default:
        return 0;
    }
}

void ftParamTryPlayItemMusic(u32 bgm_id)
{
    if (ftParamGetItemMusicLength(bgm_id) >= ftParamGetItemMusicLength(gMPCollisionBGMCurrent))
    {
        syAudioPlayBGM(0, bgm_id);

        gMPCollisionBGMCurrent = bgm_id;
    }
}

void ftParamTryUpdateItemMusic(void)
{
    u32 bgm_play = gMPCollisionBGMDefault;
    s32 length = ftParamGetItemMusicLength(bgm_play);
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);
        u32 bgm_id = gMPCollisionBGMDefault;
        s32 length_new;

        if ((fp->item_gobj != NULL) && (itGetStruct(fp->item_gobj)->kind == nITKindHammer))
        {
            bgm_id = nSYAudioBGMHammer;
        }
        if (fp->star_invincible_tics > ITSTAR_WARN_BEGIN_FRAME)
        {
            bgm_id = nSYAudioBGMStar;
        }
        length_new = ftParamGetItemMusicLength(bgm_id);

        if (length < length_new)
        {
            length = length_new;
            bgm_play = bgm_id;
        }
        fighter_gobj = fighter_gobj->link_next;
    }
    if (bgm_play != gMPCollisionBGMCurrent)
    {
        syAudioPlayBGM(0, bgm_play);
        gMPCollisionBGMCurrent = bgm_play;
    }
}

/* ft/ftparam.c:555-565 ftParamSetHammerParams 0x800E8744, verbatim: put
 * a Link's hands back the way a hammer-free Link has them, and re-pick
 * the item music. Ported with itMainDestroyItem, which
 * calls it when an item a fighter is HOLDING is destroyed -- a Hammer
 * that leaves a Link's hands mid-swing is exactly this case.
 *
 * The two model-part ids are the hands: 21 and 19 are swapped for Link
 * only, because the Hammer's own action script is what puts them on
 * every other fighter, and a Link who loses the hammer keeps them on
 * until they are set back here. */
void ftParamSetHammerParams(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->fkind == nFTKindLink) || (fp->fkind == nFTKindNLink))
    {
        ftParamSetModelPartDefaultID(fighter_gobj, 21, -1);
        ftParamSetModelPartDefaultID(fighter_gobj, 19, 0);
    }
    ftParamTryUpdateItemMusic();
}

/* ft/ftparam.c:543-553 ftParamLinkResetShieldModelParts 0x800E86F4,
 * verbatim -- ftParamSetHammerParams's mirror, and its neighbour in the
 * decomp. The same two Link-only hand parts, the other way round: 21 on
 * and 19 off, which is how a Link who is HOLDING something carries it.
 * itMainSetFighterHold calls it at the end of a pickup (it/itmain.c:466);
 * ftCommonCatchWaitSetStatus (ftcommon.c) already does the same swap by
 * hand at a grab, through ftParamSetModelPartID rather than the default
 * setter, exactly as the game does in both places. */
void ftParamLinkResetShieldModelParts(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->fkind == nFTKindLink) || (fp->fkind == nFTKindNLink))
    {
        ftParamSetModelPartDefaultID(fighter_gobj, 21, 0);
        ftParamSetModelPartDefaultID(fighter_gobj, 19, -1);
    }
}

/* ft/ftparam.c:976-1050 ftParamInitAllParts 0x800E9248: re-deal a
 * fighter's parts for a costume and shade, which the character select
 * does on every costume change. The shade half is verbatim. DIVERGES:
 * the game re-adds every joint's MObjs with their costume scripts played
 * to the new frame; the pack carries each costume's materials already
 * baked (src/dc/fighter.h FPackCostumes), so the costume is
 * the fighter's model picking its row and the MObjs re-added are only a
 * worn part's animated ones (ftParamGetPartMObjs), and the accessory GObj's display
 * list and MObjs are the attributes' NULLs, the accessory being a part of
 * the pack that ftDisplayMainDrawAccessory turns on. */
void ftParamInitAllParts(GObj *fighter_gobj, s32 costume, s32 shade)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;
    Fighter *model = dc_model_of(fighter_gobj);
    DObj *joint;
    s32 i;

    FTAccessPart *accesspart = attr->accesspart;
    FTParts *parts;
    GObj *parts_gobj;

    for (i = 0; i < ARRAY_COUNT(fp->joints) - nFTPartsJointCommonStart; i++)
    {
        joint = fp->joints[i + nFTPartsJointCommonStart];

        if (joint != NULL)
        {
            gcRemoveMObjAll(joint);

            if (fp->modelpart_status[i].modelpart_id_curr != -1)
            {
                MObjSub **mobjsubs;
                AObjEvent32 **main_matanim_joints;

                ftParamGetPartMObjs(fighter_gobj, joint, i, &mobjsubs, &main_matanim_joints);
                lbCommonAddMObjForFighterPartsDObj(joint, mobjsubs, NULL, main_matanim_joints, costume);
            }
            if ((accesspart != NULL) && ((i + nFTPartsJointCommonStart) == accesspart->joint_id))
            {
                parts = ftGetParts(joint);

                if (parts->gobj != NULL)
                {
                    gcEjectGObj(parts->gobj);

                    parts->gobj = NULL;
                }
                if (costume != 0)
                {
                    parts_gobj = gcMakeGObjSPAfter(nGCCommonKindFighterParts, NULL, nGCCommonLinkIDFighterParts, GOBJ_PRIORITY_DEFAULT);
                    parts->gobj = parts_gobj;

                    gcAddDObjForGObj(parts_gobj, accesspart->dl);

                    lbCommonAddMObjForFighterPartsDObj(DObjGetStruct(parts->gobj), accesspart->mobjsubs, accesspart->costume_matanim_joints, NULL, costume);
                }
            }
        }
    }
    if (model != NULL)
    {
        fighter_set_costume(model, costume);
    }
    fp->costume = costume;
    fp->shade = shade;

    fp->shade_color.r = ((attr->shade_color[fp->shade - 1].r * attr->shade_color[fp->shade - 1].a) / 0xFF);
    fp->shade_color.g = ((attr->shade_color[fp->shade - 1].g * attr->shade_color[fp->shade - 1].a) / 0xFF);
    fp->shade_color.b = ((attr->shade_color[fp->shade - 1].b * attr->shade_color[fp->shade - 1].a) / 0xFF);

    ftParamInitTexturePartAll(fighter_gobj);
}

/* ft/ftparam.c:942-956 ftParamSetModelPartDetailAll: the high/low part
 * swap, same cut as ftParamSetModelPartID above -- the pack carries one
 * baked batch range per joint, not a container of detail levels. */
void ftParamSetModelPartDetailAll(GObj *fighter_gobj, u8 detail)
{
    (void)fighter_gobj; (void)detail;
}

/* ft/ftparam.c:2659-2663 ftParamInitGame 0x800EC130, verbatim: what a
 * scene calls once before it spawns anyone. Both its lines are ported --
 * the ground obstacle/hazard tables are cleared (src/dc/ftmain.c), and
 * the placement counter is set from the number of teams that will play
 * (src/dc/ifcommon.c). */
void ftParamInitGame(void)
{
    ftMainClearGroundElementsAll();
    ifCommonBattleInitPlacement();
}

/* ft/ftparam.c:56-86 dFTParamCostumeIDs 0x8012B830, verbatim: per
 * fighter, the costume ids the four "royal" (free-for-all) slots and the
 * three team colours map to, and the debug one. The VS menu reads them
 * (mn/mnvsmode/mnvsmode.c mnVSModeSetCostumesAndShades) to give players
 * of the same character different colours. */
FTCostume dFTParamCostumeIDs[/* */] =
{
    { { 0, 1, 2, 3 }, { 0, 3, 4 }, 4 },     // Mario
    { { 0, 1, 2, 3 }, { 1, 2, 3 }, 3 },     // Fox
    { { 0, 1, 2, 3 }, { 2, 3, 4 }, 4 },     // Donkey Kong
    { { 0, 1, 2, 3 }, { 0, 4, 3 }, 4 },     // Samus
    { { 0, 1, 2, 3 }, { 3, 2, 0 }, 3 },     // Luigi
    { { 0, 2, 3, 1 }, { 2, 3, 0 }, 3 },     // Link
    { { 0, 1, 2, 3 }, { 1, 2, 0 }, 5 },     // Yoshi
    { { 0, 4, 1, 3 }, { 1, 5, 2 }, 5 },     // Captain Falcon
    { { 0, 1, 2, 3 }, { 3, 2, 4 }, 4 },     // Kirby
    { { 0, 1, 2, 3 }, { 1, 2, 3 }, 3 },     // Pikachu
    { { 0, 1, 2, 3 }, { 1, 2, 3 }, 3 },     // Jigglypuff
    { { 0, 1, 2, 3 }, { 0, 2, 3 }, 3 },     // Ness
    { { 0, 0, 0, 0 }, { 0, 0, 0 }, 0 },     // Master Hand
    { { 0, 0, 0, 0 }, { 0, 0, 0 }, 0 },     // Metal Mario
    { { 0, 0, 0, 0 }, { 0, 0, 0 }, 0 },     // Poly Mario
    { { 0, 0, 0, 0 }, { 0, 0, 0 }, 0 },     // Poly Fox
    { { 0, 0, 0, 0 }, { 0, 0, 0 }, 0 },     // Poly Donkey Kong
    { { 0, 0, 0, 0 }, { 0, 0, 0 }, 0 },     // Poly Samus
    { { 0, 0, 0, 0 }, { 0, 0, 0 }, 0 },     // Poly Luigi
    { { 0, 0, 0, 0 }, { 0, 0, 0 }, 0 },     // Poly Link
    { { 0, 0, 0, 0 }, { 0, 0, 0 }, 0 },     // Poly Yoshi
    { { 0, 0, 0, 0 }, { 0, 0, 0 }, 0 },     // Poly Captain Falcon
    { { 0, 0, 0, 0 }, { 0, 0, 0 }, 0 },     // Poly Kirby
    { { 0, 0, 0, 0 }, { 0, 0, 0 }, 0 },     // Poly Pikachu
    { { 0, 0, 0, 0 }, { 0, 0, 0 }, 0 },     // Poly Jigglypuff
    { { 0, 0, 0, 0 }, { 0, 0, 0 }, 0 },     // Poly Ness
    { { 0, 1, 2, 3 }, { 2, 3, 4 }, 4 }      // Giant Donkey Kong
};

/* ft/ftparam.c:2641-2657, verbatim */
// 0x800EC0EC
s32 ftParamGetCostumeCommonID(s32 fkind, s32 color)
{
    return dFTParamCostumeIDs[fkind].royal[color];
}

// 0x800EC104
s32 ftParamGetCostumeTeamID(s32 fkind, s32 color)
{
    return dFTParamCostumeIDs[fkind].team[color];
}

// 0x800EC11C
s32 ftParamGetCostumeDebug(s32 fkind)
{
    return dFTParamCostumeIDs[fkind].develop;
}

/* ft/ftparam.c:2421-2439 func_ovl2_800EB924 0x800EB924, verbatim: a world
 * point through gGMCameraMatrix (src/dc/gmcamera.c) to the camera's
 * viewport, in pixels from its centre. What the player tags and the
 * magnifiers position themselves with. The viewport's vscale is 10.2
 * fixed point (sys/rdp.c syRdpSetViewport), hence the / 4. */
void func_ovl2_800EB924(CObj *cobj, Mtx44f mtx, Vec3f *vec, f32 *rx, f32 *ry)
{
    // My math doodoo but ChatGPT says this is projecting a 3D view onto a 2D screen
    f32 x = vec->x;
    f32 y = vec->y;
    f32 z = vec->z;
    f32 tempx = ((mtx[0][0] * x) + (mtx[1][0] * y) + (mtx[2][0] * z)) + mtx[3][0];
    f32 tempy = ((mtx[0][1] * x) + (mtx[1][1] * y) + (mtx[2][1] * z)) + mtx[3][1];
    f32 scale = ((mtx[0][3] * x) + (mtx[1][3] * y) + (mtx[2][3] * z)) + mtx[3][3];

    if (ABSF(scale) < 0.1F)
    {
        scale = (scale < 0.0F) ? -0.1F : 0.1F;
    }
    scale = 1.0F / scale;

    *rx = (cobj->viewport.vp.vscale[0] / 4) * (tempx * scale);
    *ry = (cobj->viewport.vp.vscale[1] / 4) * (tempy * scale);
}

/* ft/ftparam.c:2441-2639 func_ovl2_800EBA6C / 800EBB3C / 800EBC0C /
 * 800EBD08, verbatim: the leg IK behind the slope contour.
 * mpCommonUpdateFighterSlopeContour (src/dc/mpcommon.c) asks 800EBC0C
 * where a foot is and how its knee is twisted, moves the point onto the
 * floor, and 800EBD08 solves the two-bone leg (the foot joint's child and
 * grandchild) to reach it, refreshing both bones' FTParts matrices. */
// 0x800EBA6C
f32 func_ovl2_800EBA6C(Vec3f *arg0, Vec3f *arg1)
{
    Vec3f sp1C = *arg1;
    f32 scale;

    syVectorNorm3D(&sp1C);

    scale = (arg0->x * sp1C.x) + (sp1C.y * arg0->y) + (sp1C.z * arg0->z);

    syVectorScale3D(&sp1C, scale);

    arg0->x -= sp1C.x;
    arg0->y -= sp1C.y;
    arg0->z -= sp1C.z;

    return SQUARE(arg0->x) + SQUARE(arg0->y) + SQUARE(arg0->z);
}

// 0x800EBB3C
f32 func_ovl2_800EBB3C(Vec3f *arg0, Vec3f *arg1, Vec3f *arg2)
{
    Vec3f sp1C;

    if (func_ovl2_800EBA6C(arg0, arg2) < 0.0004F)
    {
        return 0.0F;
    }
    else if (func_ovl2_800EBA6C(arg1, arg2) < 0.0004F)
    {
        return 0.0F;
    }
    else lbCommonCross3D(arg0, arg1, &sp1C);

    if (lbCommonSim3D(&sp1C, arg2) < 0.0F)
    {
        return -syVectorAngleDiff3D(arg1, arg0);
    }
    else return syVectorAngleDiff3D(arg1, arg0);
}

// 0x800EBC0C
void func_ovl2_800EBC0C(s32 arg0, Vec3f *arg1, f32 *arg2, f32 arg3, DObj *dobj)
{
    s32 unused1[2];
    FTParts *parts;
    Vec3f sp50;
    Vec3f sp44;
    Vec3f sp38;
    Vec3f sp2C;
    s32 unused2[2];

    sp50.z = 0.0F;
    sp50.y = 0.0F;
    sp50.x = arg3;

    gmCollisionGetFighterPartsWorldPosition(dobj->child->child, &sp50);

    *arg1 = sp50;

    func_ovl2_800EE018(dobj, &sp50);

    sp2C.z = 0.0F;
    sp2C.x = 0.0F;
    sp2C.y = 1.0F;

    syVectorNormCross3D(&sp50, &sp2C, &sp44);

    parts = ftGetParts(dobj->child);

    sp38.x = parts->unk_dobjtrans_0x10[2][0];
    sp38.y = parts->unk_dobjtrans_0x10[2][1];
    sp38.z = parts->unk_dobjtrans_0x10[2][2];

    syVectorNorm3D(&sp44);
    syVectorNorm3D(&sp38);
    syVectorNorm3D(&sp50);

    *arg2 = func_ovl2_800EBB3C(&sp44, &sp38, &sp50);
}

// 0x800EBD08
void func_ovl2_800EBD08(DObj *root_dobj, f32 arg1, Vec3f *vec, f32 arg3)
{
    // I feel like this is kind of a fakematch (as in, matching stack allocation is achieved by nonsense) but I hate this function so I'll take it
    DObj *child1_dobj;
    DObj *child2_dobj;
    f32 sqrtxyz;
    f32 unused;
    f32 sin_arg3;
    f32 cos_arg3;
    f32 sp38;
    f32 normal;
    f32 trax;
    f32 zmulnorm;
    f32 new_var;
    f32 square_xy;
    f32 imbadatmathhelp;
    f32 inverse_xy;
    f32 inverse_xy_2;
    f32 inverse_xy_3;
    f32 zmul;
    f32 square_arg1;
    f32 xmul;
    f32 sp58;
    f32 ymul;
    f32 sp50;
    f32 inverse_xyz;
    f32 square_trax;

    child1_dobj = root_dobj->child;
    child2_dobj = child1_dobj->child;
    trax = child2_dobj->translate.vec.f.x;

    square_xy = SQUARE(vec->x) + SQUARE(vec->y);

    sqrtxyz = sqrtf((vec->z * vec->z) + square_xy);

    square_xy = sqrtf(square_xy);

    sin_arg3 = lbCommonSin(arg3);

    cos_arg3 = lbCommonCos(arg3);

    sp50 = sqrtxyz * sqrtxyz;

    square_trax = SQUARE(trax);

    square_arg1 = SQUARE(arg1);

    normal = ((SQUARE(sqrtxyz) + square_trax) - square_arg1) / (2.0F * sqrtxyz * trax);

    if (normal < 0.0F)
    {
        normal = 0.0F;
    }
    if (normal > 1.0F)
    {
        normal = 1.0F;
    }
    sp38 = -sqrtf(1.0F - SQUARE(normal));

    if (sqrtxyz == 0.0F)
    {
        sqrtxyz = 0.00000001F;
    }
    inverse_xy_3 = normal;

    inverse_xyz = 1.0F / sqrtxyz;

    if (square_xy == 0.0F)
    {
        square_xy = 0.00000001F;
    }
    inverse_xy = 1.0F / square_xy;

    xmul = vec->x * inverse_xyz;
    ymul = vec->y * inverse_xyz;
    zmul = vec->z * inverse_xyz;

    sp58 = (-xmul * vec->z * sin_arg3 * inverse_xy) - (vec->y * cos_arg3 * inverse_xy);

    zmulnorm = zmul * inverse_xy_3;

    sp50 = (-ymul * vec->z * sin_arg3 * inverse_xy) + (vec->x * cos_arg3 * inverse_xy);

    imbadatmathhelp = square_xy * sin_arg3 * inverse_xyz;

    new_var = imbadatmathhelp * sp38;

    inverse_xy_2 = (zmulnorm + new_var);

    if ((inverse_xy_2 == -1.0F) || (inverse_xy_2 == 1.0F))
    {
        if (inverse_xy_2 == -1.0F)
        {
            child1_dobj->rotate.vec.f.y = F_CLC_DTOR32(90.0F);

            child1_dobj->rotate.vec.f.x = syUtilsArcTan2((-sp38 * xmul) + (sp58 * normal), (-sp38 * ymul) + (sp50 * normal));
        }
        else
        {
            child1_dobj->rotate.vec.f.y = F_CLC_DTOR32(-90.0F);

            child1_dobj->rotate.vec.f.x = syUtilsArcTan2(-((-sp38 * xmul) + (sp58 * normal)), (-sp38 * ymul) + (sp50 * normal));
        }
        child1_dobj->rotate.vec.f.z = 0.0F;
    }
    else
    {
        child1_dobj->rotate.vec.f.y = syUtilsArcSin(-inverse_xy_2);
        child1_dobj->rotate.vec.f.x = syUtilsArcTan2((-sp38 * zmul) + (imbadatmathhelp * normal), square_xy * cos_arg3 * inverse_xyz);
        child1_dobj->rotate.vec.f.z = syUtilsArcTan2((ymul * normal) + (sp50 * sp38), (xmul * normal) + (sp58 * sp38));
    }
    child2_dobj->rotate.vec.f.z = syUtilsArcCos(((SQUARE(sqrtxyz) - square_trax) - square_arg1) / (2.0F * trax * arg1));

    ftParamsUpdateFighterPartsTransformAll(child1_dobj);
    ftParamsUpdateFighterPartsTransformAll(child2_dobj);
}

/* The bzero arm of syDmaLoadOverlay for overlay 2, of which ft/ftparam is
 * a part (smashbrothers.us.yaml). It is empty, and that is the point: the
 * file keeps no state of its own today -- everything it touches lives in
 * an FTStruct, which is scene-heap memory taskman empties between scenes.
 * The entry exists so that a static added here tomorrow is a build
 * failure in tools/check/overlay_check.py rather than a second-visit fault. */
void ftParamOverlayLoad(void)
{
}
