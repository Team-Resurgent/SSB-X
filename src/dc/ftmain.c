/* ftmain.c -- ft/ftmain.c's script engine and hit half, ported:
 * ftMainParseMotionEvent (the event interpreter, ft/ftmain.c:151-693),
 * the three script-stepping loops (:695-926), the two animation entry
 * points that run them (:928-957), and the hit search
 * and hit stats (:1575-1738, :1929-2212, :2593-2643, :2666-3226,
 * :3605-4085) with the four GObjProcesses that run them, and
 * the weapon and item halves of the same (:2213-2592, :2644-2661,
 * :3227-3604, the wp/it arms of :2666-3226): a fighter walking the
 * weapon and item links for what is hitting it. Every line
 * below the include is the decomp's text, unmodified. What the
 * interpreter and
 * the stats reach that the port has not built yet -- model and texture
 * parts, rumble, the fighter's side of the colour animations -- ends in
 * a stub in src/dc/ftparam.c that says so (the colanim interpreter
 * itself, ftMainUpdateColAnim,); the sound calls end in
 * src/dc/sysshim.c's FGM hooks. The script words themselves come out of
 * the pack (src/dc/fighter.h: FPackHeader.off_script), the fighter's
 * MainMotion file with its pointers relocated at load, and
 * ftMainSetStatus (src/dc/ftcommon.c) starts them the way the game's
 * does. The event structs' bit-fields follow the ABI through
 * docker/patches/0005.
 */
#include "ftcommon.h"

#include <sys/utils.h>
#include <gm/gmrumble.h>
#include <wp/weapon.h>          /* WPStruct, wpMainGetStaledDamage, wpProcessUpdateHitInteractStats */
#include <it/item.h>            /* ITStruct, itMainGetDamageOutput, itProcessSetHitInteractStats */
#include <it/itvars.h>          /* ITSTAR_INVINCIBLE_TIME */

// 0x800DF0F0
void ftMainParseMotionEvent(GObj *fighter_gobj, FTStruct *fp, FTMotionScript *ms, u32 ev_kind)
{
    s32 unused1;
    s32 effect_id;
    s32 i, j;
    s32 unused2;
    FTAttackColl *attack_coll;
    s32 attack_id;
    s32 group_id;
    u32 sfx_id;
    s32 joint_id;
    Vec3f effect_offset;
    Vec3f effect_scatter;
    u32 flag;
    Vec3f damage_coll_offset;
    Vec3f damage_coll_size;
    FTAttributes *attr;
    FTMotionDamageScript *p_damage;
    s32 fkind;
    s32 script_id;
    s32 slope_contour;
    sb32 unused3;

    switch (ev_kind)
    {
    case nFTMotionEventEnd:
        ms->p_script = NULL;
        break;

    case nFTMotionEventSyncWait:
        ms->script_wait += ftMotionEventCast(ms, FTMotionEventDefault)->value;

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventAsyncWait:
        ms->script_wait = ftMotionEventCast(ms, FTMotionEventDefault)->value - fighter_gobj->anim_frame;

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventMakeAttackColl:
    case nFTMotionEventMakeAttackCollScaled:
        if (fp->pkind != nFTPlayerKindDemo)
        {
            attack_id = ftMotionEventCast(ms, FTMotionEventMakeAttack1)->attack_id;
            attack_coll = &fp->attack_colls[attack_id];

            if ((attack_coll->attack_state == nGMAttackStateOff) || (attack_coll->group_id != ftMotionEventCast(ms, FTMotionEventMakeAttack1)->group_id))
            {
                attack_coll->group_id = ftMotionEventCast(ms, FTMotionEventMakeAttack1)->group_id;
                attack_coll->attack_state = nGMAttackStateNew;
                fp->is_attack_active = TRUE;

                for (i = 0; i < ARRAY_COUNT(fp->attack_colls); i++)
                {
                    if ((i != attack_id) && (fp->attack_colls[i].attack_state != nGMAttackStateOff) && (attack_coll->group_id == fp->attack_colls[i].group_id))
                    {
                        for (j = 0; j < ARRAY_COUNT(attack_coll->attack_records); j++)
                        {
                            attack_coll->attack_records[j] = fp->attack_colls[i].attack_records[j];
                        }
                        break;
                    }
                }
                if (i == ARRAY_COUNT(fp->attack_colls))
                {
                    ftParamClearAttackRecordID(fp, attack_id);
                }
            }
            attack_coll->joint_id = ftParamGetJointID(fp, ftMotionEventCast(ms, FTMotionEventMakeAttack1)->joint_id);
            attack_coll->joint = fp->joints[attack_coll->joint_id];
            attack_coll->damage = ftMotionEventCast(ms, FTMotionEventMakeAttack1)->damage;
            attack_coll->can_rebound = ftMotionEventCast(ms, FTMotionEventMakeAttack1)->can_rebound;
            attack_coll->element = ftMotionEventCast(ms, FTMotionEventMakeAttack1)->element;

            ftMotionEventAdvance(ms, FTMotionEventMakeAttack1);

            attack_coll->size = ftMotionEventCast(ms, FTMotionEventMakeAttack2)->size * 0.5F;
            attack_coll->offset.x = ftMotionEventCast(ms, FTMotionEventMakeAttack2)->off_x;

            ftMotionEventAdvance(ms, FTMotionEventMakeAttack2);

            attack_coll->offset.y = ftMotionEventCast(ms, FTMotionEventMakeAttack3)->off_y;
            attack_coll->offset.z = ftMotionEventCast(ms, FTMotionEventMakeAttack3)->off_z;

            ftMotionEventAdvance(ms, FTMotionEventMakeAttack3);

            attack_coll->angle = ftMotionEventCast(ms, FTMotionEventMakeAttack4)->angle;
            attack_coll->knockback_scale = ftMotionEventCast(ms, FTMotionEventMakeAttack4)->knockback_scale;
            attack_coll->knockback_weight = ftMotionEventCast(ms, FTMotionEventMakeAttack4)->knockback_weight;

            attack_coll->is_hit_air = ftMotionEventCast(ms, FTMotionEventMakeAttack4)->is_hit_ground_air & 1;           // Why?
            attack_coll->is_hit_ground = (ftMotionEventCast(ms, FTMotionEventMakeAttack4)->is_hit_ground_air & 2) >> 1; // ???

            ftMotionEventAdvance(ms, FTMotionEventMakeAttack4);

            attack_coll->shield_damage = ftMotionEventCast(ms, FTMotionEventMakeAttack5)->shield_damage;

            attack_coll->fgm_level = ftMotionEventCast(ms, FTMotionEventMakeAttack5)->fgm_level;
            attack_coll->fgm_kind = ftMotionEventCast(ms, FTMotionEventMakeAttack5)->fgm_kind;

            attack_coll->knockback_base = ftMotionEventCast(ms, FTMotionEventMakeAttack5)->knockback_base;

            ftMotionEventAdvance(ms, FTMotionEventMakeAttack5);

            attack_coll->is_scale_pos = (ev_kind == nFTMotionEventMakeAttackCollScaled) ? TRUE : FALSE;

            attack_coll->motion_attack_id = fp->motion_attack_id;

            attack_coll->motion_count = fp->motion_count;

            attack_coll->damage = ftParamGetStaledDamage(fp->player, attack_coll->damage, attack_coll->motion_attack_id, attack_coll->motion_count);
        }
        else ftMotionEventAdvance(ms, FTMotionEventMakeAttack);
        break;

    case nFTMotionEventSetAttackCollOffset:
        attack_id = ftMotionEventCast(ms, FTMotionEventSetAttackOffset1)->attack_id;

        attack_coll = &fp->attack_colls[attack_id];

        attack_coll->offset.x = ftMotionEventCast(ms, FTMotionEventSetAttackOffset1)->off_x;

        ftMotionEventAdvance(ms, FTMotionEventSetAttackOffset1);

        attack_coll->offset.y = ftMotionEventCast(ms, FTMotionEventSetAttackOffset2)->off_y;
        attack_coll->offset.z = ftMotionEventCast(ms, FTMotionEventSetAttackOffset2)->off_z;

        ftMotionEventAdvance(ms, FTMotionEventSetAttackOffset2);
        break;

    case nFTMotionEventSetAttackCollDamage:
        if (fp->pkind != nFTPlayerKindDemo)
        {
            attack_id = ftMotionEventCast(ms, FTMotionEventSetAttackCollDamage)->attack_id;

            fp->attack_colls[attack_id].damage = ftMotionEventCast(ms, FTMotionEventSetAttackCollDamage)->damage;

            ftMotionEventAdvance(ms, FTMotionEventSetAttackCollDamage);

            fp->attack_colls[attack_id].damage = ftParamGetStaledDamage(fp->player, fp->attack_colls[attack_id].damage, fp->attack_colls[attack_id].motion_attack_id, fp->attack_colls[attack_id].motion_count);
        }
        else ftMotionEventAdvance(ms, FTMotionEventSetAttackCollDamage);
        break;

    case nFTMotionEventSetAttackCollSize:
        attack_id = ftMotionEventCast(ms, FTMotionEventSetAttackCollSize)->attack_id;

        fp->attack_colls[attack_id].size = ftMotionEventCast(ms, FTMotionEventSetAttackCollSize)->size * 0.5F;

        ftMotionEventAdvance(ms, FTMotionEventSetAttackCollSize);
        break;

    case nFTMotionEventSetAttackCollSoundLevel:
        attack_id = ftMotionEventCast(ms, FTMotionEventSetAttackCollSound)->attack_id;

        fp->attack_colls[attack_id].fgm_level = ftMotionEventCast(ms, FTMotionEventSetAttackCollSound)->fgm_level;

        ftMotionEventAdvance(ms, FTMotionEventSetAttackCollSound);
        break;

    case nFTMotionEventRefreshAttackCollID:
        attack_id = ftMotionEventCast(ms, FTMotionEventDefault)->value;

        ftMotionEventAdvance(ms, FTMotionEventDefault);

        ftParamRefreshAttackCollID(fighter_gobj, attack_id);
        break;

    case nFTMotionEventClearAttackCollID:
        attack_id = ftMotionEventCast(ms, FTMotionEventDefault)->value;

        ftMotionEventAdvance(ms, FTMotionEventDefault);

        fp->attack_colls[attack_id].attack_state = nGMAttackStateOff;
        break;

    case nFTMotionEventClearAttackCollAll:
        ftParamClearAttackCollAll(fighter_gobj);

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventSetThrow:
        ftMotionEventAdvance(ms, FTMotionEventSetThrow1);

        fp->throw_desc = ftMotionEventCast(ms, FTMotionEventSetThrow2)->throw_desc;

        ftMotionEventAdvance(ms, FTMotionEventSetThrow2);
        break;

    case nFTMotionEventPlayFGMStoreInfo:
        if (!(fp->is_muted))
        {
            fp->p_sfx = func_800269C0_275C0(ftMotionEventCastAdvance(ms, FTMotionEventDefault)->value);

            fp->sfx_id = (fp->p_sfx != NULL) ? fp->p_sfx->sfx_id : 0;
        }
        else ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventPlayFGM:
        if (!(fp->is_muted))
        {
            func_800269C0_275C0(ftMotionEventCastAdvance(ms, FTMotionEventDefault)->value);
        }
        else ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventPlayLoopSFXStoreInfo:
        if (!(fp->is_muted))
        {
            ftParamPlayLoopSFX(fp, ftMotionEventCastAdvance(ms, FTMotionEventDefault)->value);
        }
        else ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventStopLoopSFX:
        ftParamStopLoopSFX(fp), ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventPlayVoiceStoreInfo:
        if (!(fp->is_muted) && (fp->attr->is_have_voice))
        {
            ftParamPlayVoice(fp, ftMotionEventCastAdvance(ms, FTMotionEventDefault)->value);
        }
        else ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventPlayLoopVoiceStoreInfo:
        if (!(fp->is_muted) && (fp->attr->is_have_voice))
        {
            ftParamPlayLoopSFX(fp, ftMotionEventCastAdvance(ms, FTMotionEventDefault)->value);
        }
        else ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventPlaySmashVoice:
        if (!(fp->is_muted))
        {
            ftParamPlayVoice(fp, fp->attr->smash_sfx[syUtilsRandIntRange(ARRAY_COUNT(fp->attr->smash_sfx))]);

            ftMotionEventAdvance(ms, FTMotionEventDefault);
        }
        else ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventSetAirJumpAdd:
        fp->ga = nMPKineticsAir;

        fp->physics.vel_air.z = DObjGetStruct(fighter_gobj)->translate.vec.f.z = 0.0F;

        fp->jumps_used++;

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventSetAirJumpMax:
        attr = fp->attr;

        fp->ga = nMPKineticsAir;

        fp->physics.vel_air.z = DObjGetStruct(fighter_gobj)->translate.vec.f.z = 0.0F;

        fp->jumps_used = attr->jumps_max;

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventEffect:
    case nFTMotionEventEffectItemHold:
        if (!(fp->is_effect_skip))
        {
            joint_id = ftParamGetJointID(fp, ftMotionEventCast(ms, FTMotionEventMakeEffect1)->joint_id);
            effect_id = ftMotionEventCast(ms, FTMotionEventMakeEffect1)->effect_id;
            flag = ftMotionEventCast(ms, FTMotionEventMakeEffect1)->flag;

            ftMotionEventAdvance(ms, FTMotionEventMakeEffect1);

            effect_offset.x = ftMotionEventCast(ms, FTMotionEventMakeEffect2)->off_x;
            effect_offset.y = ftMotionEventCast(ms, FTMotionEventMakeEffect2)->off_y;

            ftMotionEventAdvance(ms, FTMotionEventMakeEffect2);

            effect_offset.z = ftMotionEventCast(ms, FTMotionEventMakeEffect3)->off_z;
            effect_scatter.x = ftMotionEventCast(ms, FTMotionEventMakeEffect3)->rng_x;

            ftMotionEventAdvance(ms, FTMotionEventMakeEffect3);

            effect_scatter.y = ftMotionEventCast(ms, FTMotionEventMakeEffect4)->rng_y;
            effect_scatter.z = ftMotionEventCast(ms, FTMotionEventMakeEffect4)->rng_z;

            ftMotionEventAdvance(ms, FTMotionEventMakeEffect4);

            ftParamMakeEffect(fighter_gobj, effect_id, joint_id, &effect_offset, &effect_scatter, fp->lr, (ev_kind == nFTMotionEventEffectItemHold) ? TRUE : FALSE, flag);
        }
        else ftMotionEventAdvance(ms, FTMotionEventMakeEffect);
        break;

    case nFTMotionEventSetHitStatusPartAll:
        ftParamSetHitStatusPartAll(fighter_gobj, ftMotionEventCast(ms, FTMotionEventDefault)->value);

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventSetHitStatusPartID:
        ftParamSetHitStatusPartID(fighter_gobj, ftParamGetJointID(fp, ftMotionEventCast(ms, FTMotionEventSetHitStatusPartID)->joint_id), ftMotionEventCast(ms, FTMotionEventSetHitStatusPartID)->hitstatus);

        ftMotionEventAdvance(ms, FTMotionEventSetHitStatusPartID);
        break;

    case nFTMotionEventSetHitStatusAll:
        ftParamSetHitStatusAll(fighter_gobj, ftMotionEventCast(ms, FTMotionEventDefault)->value);

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventResetDamageCollPartAll:
        ftParamResetFighterDamageCollsAll(fighter_gobj);

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventSetDamageCollPartID:
        joint_id = ftParamGetJointID(fp, ftMotionEventCast(ms, FTMotionEventSetDamageCollPartID1)->joint_id);

        ftMotionEventAdvance(ms, FTMotionEventSetDamageCollPartID1);

        damage_coll_offset.x = ftMotionEventCast(ms, FTMotionEventSetDamageCollPartID2)->off_x;
        damage_coll_offset.y = ftMotionEventCast(ms, FTMotionEventSetDamageCollPartID2)->off_y;

        ftMotionEventAdvance(ms, FTMotionEventSetDamageCollPartID2);

        damage_coll_offset.z = ftMotionEventCast(ms, FTMotionEventSetDamageCollPartID3)->off_z;
        damage_coll_size.x = ftMotionEventCast(ms, FTMotionEventSetDamageCollPartID3)->size_x;

        ftMotionEventAdvance(ms, FTMotionEventSetDamageCollPartID3);

        damage_coll_size.y = ftMotionEventCast(ms, FTMotionEventSetDamageCollPartID4)->size_y;
        damage_coll_size.z = ftMotionEventCast(ms, FTMotionEventSetDamageCollPartID4)->size_z;

        ftMotionEventAdvance(ms, FTMotionEventSetDamageCollPartID4);

        ftParamModifyDamageCollID(fighter_gobj, joint_id, &damage_coll_offset, &damage_coll_size);
        break;

    case nFTMotionEventLoopBegin:
        ms->p_goto[ms->script_id] = lbRelocGetFileData(void*, ms->p_script, sizeof(FTMotionEventDefault));

        ms->script_id++;

        ms->loop_count[ms->script_id++ - 1] = ftMotionEventCast(ms, FTMotionEventDefault)->value, ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventLoopEnd:
        if (--ms->loop_count[ms->script_id - 2] != 0)
        {
            ms->p_script = ms->p_goto[ms->script_id - 2];
        }
        else ftMotionEventAdvance(ms, FTMotionEventDefault), ms->script_id -= 2; // Seems fake, but also impossible to match otherwise???
        break;

    case nFTMotionEventSubroutine:
        ftMotionEventAdvance(ms, FTMotionEventSubroutine1);

        ms->p_goto[ms->script_id] = lbRelocGetFileData(void*, ms->p_script, sizeof(FTMotionEventSubroutine2));

        ms->script_id++;

        ms->p_script = ftMotionEventCast(ms, FTMotionEventSubroutine2)->p_goto;
        break;

    case nFTMotionEventSetDamageThrown:
        if (fp->throw_gobj != NULL)
        {
            fkind = fp->throw_fkind;

            ftMotionEventAdvance(ms, FTMotionEventSetDamageThrown1);

            p_damage = ftMotionEventCast(ms, FTMotionEventSetDamageThrown2)->p_subroutine;

            if (p_damage->p_script[fp->status_vars.common.damage.script_id][fkind] != NULL)
            {
                ms->p_goto[ms->script_id] = lbRelocGetFileData(void*, ms->p_script, sizeof(FTMotionEventSetDamageThrown2));

                ms->script_id++;

                ms->p_script = p_damage->p_script[fp->status_vars.common.damage.script_id][fkind];
            }
            else ftMotionEventAdvance(ms, FTMotionEventSetDamageThrown2);
        }
        else ftMotionEventAdvance(ms, FTMotionEventSetDamageThrown);
        break;

    case nFTMotionEventReturn:
        ms->p_script = ms->p_goto[--ms->script_id];
        break;

    case nFTMotionEventGoto:
        ftMotionEventAdvance(ms, FTMotionEventGoto1);

        ms->p_script = ftMotionEventCast(ms, FTMotionEventGoto2)->p_goto;
        break;

    case nFTMotionEventSetParallelScript:
        ftMotionEventAdvance(ms, FTMotionEventParallel1);

        if (fp->motion_scripts[0][1].p_script == NULL)
        {
            fp->motion_scripts[0][1].p_script = fp->motion_scripts[1][1].p_script = ftMotionEventCast(ms, FTMotionEventParallel2)->p_goto;
            fp->motion_scripts[0][1].script_wait = fp->motion_scripts[1][1].script_wait = DObjGetStruct(fighter_gobj)->anim_speed - fighter_gobj->anim_frame;
            fp->motion_scripts[0][1].script_id = fp->motion_scripts[1][1].script_id = 0;
        }
        ftMotionEventAdvance(ms, FTMotionEventParallel2);
        break;

    case nFTMotionEventPauseScript:
        ftMotionEventAdvance(ms, FTMotionEventDefault);

        ms->script_wait = F32_MAX;
        break;

    case nFTMotionEventSetModelPartID:
        ftParamSetModelPartID
        (
            fighter_gobj, 
            ftParamGetJointID(fp, ftMotionEventCast(ms, FTMotionEventSetModelPartID)->joint_id), 
            ftMotionEventCast(ms, FTMotionEventSetModelPartID)->modelpart_id
        );
        ftMotionEventAdvance(ms, FTMotionEventSetModelPartID);
        break;

    case nFTMotionEventResetModelPartAll:
        ftParamResetModelPartAll(fighter_gobj);

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventHideModelPartAll:
        ftParamHideModelPartAll(fighter_gobj);

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventSetTexturePartID:
        ftParamSetTexturePartID
        (
            fighter_gobj, 
            ftMotionEventCast(ms, FTMotionEventSetTexturePartID)->texturepart_id, 
            ftMotionEventCast(ms, FTMotionEventSetTexturePartID)->frame
        );
        ftMotionEventAdvance(ms, FTMotionEventSetTexturePartID);
        break;

    case nFTMotionEventSetColAnim:
        ftParamCheckSetFighterColAnimID
        (
            fighter_gobj, 
            ftMotionEventCast(ms, FTMotionEventSetColAnimID)->colanim_id, 
            ftMotionEventCast(ms, FTMotionEventSetColAnimID)->length
        );
        ftMotionEventAdvance(ms, FTMotionEventSetColAnimID);
        break;

    case nFTMotionEventResetColAnim:
        ftParamResetStatUpdateColAnim(fighter_gobj);

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventSetFlag0:
        fp->motion_vars.flags.flag0 = ftMotionEventCast(ms, FTMotionEventDefault)->value;

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventSetFlag1:
        fp->motion_vars.flags.flag1 = ftMotionEventCast(ms, FTMotionEventDefault)->value;

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventSetFlag2:
        fp->motion_vars.flags.flag2 = ftMotionEventCast(ms, FTMotionEventDefault)->value;

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventSetFlag3:
        fp->motion_vars.flags.flag3 = ftMotionEventCast(ms, FTMotionEventDefault)->value;

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventSetSlopeContour:
        slope_contour = fp->slope_contour;

        fp->slope_contour = ftMotionEventCastAdvance(ms, FTMotionEventSetSlopeContour)->flags;

        if (!(slope_contour & fp->slope_contour & FTSLOPECONTOUR_FLAG_FULL))
        {
            DObjGetStruct(fighter_gobj)->rotate.vec.f.x = F_CLC_DTOR32(0.0F);
        }
        break;

    case nFTMotionEventHideItem:
        fp->is_item_show = ftMotionEventCast(ms, FTMotionEventDefault)->value;

        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;

    case nFTMotionEventSetAfterImage:
        fp->afterimage.is_itemswing = ftMotionEventCast(ms, FTMotionEventSetAfterImage)->is_itemswing;
        fp->afterimage.drawstatus = ftMotionEventCast(ms, FTMotionEventSetAfterImage)->drawstatus;

        ftMotionEventAdvance(ms, FTMotionEventSetAfterImage);
        break;

    case nFTMotionEventMakeRumble:
        if (fp->pkind != nFTPlayerKindDemo)
        {
            ftParamMakeRumble
            (
                fp, 
                ftMotionEventCast(ms, FTMotionEventMakeRumble)->rumble_id, 
                ftMotionEventCast(ms, FTMotionEventMakeRumble)->length
            );
        }
        ftMotionEventAdvance(ms, FTMotionEventMakeRumble);
        break;

    case nFTMotionEventStopRumble:
        if (fp->pkind != nFTPlayerKindDemo)
        {
            gmRumbleStopRumbleID(fp->player, ftMotionEventCast(ms, FTMotionEventDefault)->value);
        }
        ftMotionEventAdvance(ms, FTMotionEventDefault);
        break;
    }
}

// 0x800E02A8 - Run all motion events. Effects are parsed only if events aren't queued.
// If events are not queued, fp->motion_scripts[0][i] is copied to fp->motion_scripts[1][i]
// for later execution in the Physics / Map update process.
void ftMainUpdateMotionEventsAll(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 i;

    for (i = 0; i < ARRAY_COUNT(fp->motion_scripts); i++)
    {
        FTMotionScript *ms = &fp->motion_scripts[0][i];
        u32 ev_kind;

        if (ms->p_script != NULL)
        {
            if (ms->script_wait != F32_MAX)
            {
                ms->script_wait -= DObjGetStruct(fighter_gobj)->anim_speed;
            }
            while (TRUE)
            {
                if (ms->p_script == NULL)
                {
                    break;
                }
                else
                {
                    if (ms->script_wait == F32_MAX)
                    {
                        if ((DObjGetStruct(fighter_gobj)->anim_speed <= fighter_gobj->anim_frame)) 
                        {
                            break;
                        }
                        else ms->script_wait = -fighter_gobj->anim_frame;
                    }
                    else if (ms->script_wait > 0.0F) 
                    {
                        break;
                    }
                    ev_kind = ftMotionEventCast(ms, FTMotionEventMakeEffect1)->opcode;
    
                    if ((ev_kind == nFTMotionEventEffect || ev_kind == nFTMotionEventEffectItemHold) && (fp->is_events_forward))
                    {
                        ftMotionEventAdvance(ms, FTMotionEventMakeEffect);
                    }
                    else ftMainParseMotionEvent(fighter_gobj, fp, ms, ev_kind);
                }
            }
        }
    }
    if (!(fp->is_events_forward))
    {
        for (i = 0; i < ARRAY_COUNT(fp->motion_scripts); i++)
        {
            fp->motion_scripts[1][i] = fp->motion_scripts[0][i];
        }
    }
}

// 0x800E0478 - Fast-forward most fighter-specific events and update core events (sync timer, async timer, goto, subroutine, etc.).
// fp->motion_scripts[0][i] is always moved to fp->motion_scripts[1][i] for later execution in the Physics / Map update process.
void ftMainUpdateMotionEventsForward(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 i;

    for (i = 0; i < ARRAY_COUNT(fp->motion_scripts[0]); i++)
    {
        FTMotionScript *ms = &fp->motion_scripts[0][i];
        u32 ev_kind;

        if (ms->p_script != NULL)
        {
            if (ms->script_wait != F32_MAX)
            {
                ms->script_wait -= DObjGetStruct(fighter_gobj)->anim_speed;
            }
            while (TRUE)
            {
                if (ms->p_script == NULL)
                {
                    break;
                }
                else
                {
                    if (ms->script_wait == F32_MAX)
                    {
                        if ((DObjGetStruct(fighter_gobj)->anim_speed <= fighter_gobj->anim_frame))
                        {
                            break;
                        }
                        else ms->script_wait = -fighter_gobj->anim_frame;
                    }
                    else if (ms->script_wait > 0.0F)
                    {
                        break;
                    }
                    ev_kind = ftMotionEventCast(ms, FTMotionEventDefault)->opcode;

                    switch (ev_kind)
                    {
                    case nFTMotionEventClearAttackCollID:
                    case nFTMotionEventClearAttackCollAll:
                    case nFTMotionEventSetAttackCollDamage:
                    case nFTMotionEventSetAttackCollSize:
                    case nFTMotionEventSetAttackCollSoundLevel:
                    case nFTMotionEventRefreshAttackCollID:
                    case nFTMotionEventPlayFGM:
                    case nFTMotionEventPlayLoopSFXStoreInfo:
                    case nFTMotionEventStopLoopSFX:
                    case nFTMotionEventPlayVoiceStoreInfo:
                    case nFTMotionEventPlayLoopVoiceStoreInfo:
                    case nFTMotionEventPlayFGMStoreInfo:
                    case nFTMotionEventPlaySmashVoice:
                    case nFTMotionEventSetFlag0:
                    case nFTMotionEventSetFlag1:
                    case nFTMotionEventSetFlag2:
                    case nFTMotionEventSetAirJumpAdd:
                    case nFTMotionEventSetAirJumpMax:
                    case nFTMotionEventSetColAnim:
                    case nFTMotionEventResetColAnim:
                    case nFTMotionEventMakeRumble:
                    case nFTMotionEventStopRumble:
                    case nFTMotionEventSetAfterImage:
                        ftMotionEventAdvance(ms, FTMotionEventDefault);
                        break;

                    case nFTMotionEventEffect:
                    case nFTMotionEventEffectItemHold:
                        ftMotionEventAdvance(ms, FTMotionEventMakeEffect);
                        break;

                    case nFTMotionEventMakeAttackColl:
                    case nFTMotionEventMakeAttackCollScaled:
                        ftMotionEventAdvance(ms, FTMotionEventMakeAttack);
                        break;

                    case nFTMotionEventSetAttackCollOffset:
                        ftMotionEventAdvance(ms, FTMotionEventSetAttackOffset);
                        break;

                    default:
                        ftMainParseMotionEvent(fighter_gobj, fp, ms, ev_kind);
                        break;
                    }
                }
            }
        }
    }
    for (i = 0; i < ARRAY_COUNT(fp->motion_scripts[0]); i++)
    {
        fp->motion_scripts[1][i] = fp->motion_scripts[0][i];
    }
}

// 0x800E0654 - Fast-forward all events except core, throw setup and effect spawn events.
// This makes it so that effects are run last, after all four main processes
// (proc_update, proc_interrupt, proc_physics, proc_map).
void ftMainUpdateMotionEventsForwardEffect(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 i;

    for (i = 0; i < ARRAY_COUNT(fp->motion_scripts[1]); i++)
    {
        FTMotionScript *ms = &fp->motion_scripts[1][i];
        u32 ev_kind;

        if (ms->p_script != NULL)
        {
            if (ms->script_wait != F32_MAX)
            {
                ms->script_wait -= DObjGetStruct(fighter_gobj)->anim_speed;
            }
            while (TRUE)
            {
                if (ms->p_script == NULL)
                {
                    break;
                }
                else
                {
                    if (ms->script_wait == F32_MAX)
                    {
                        if (DObjGetStruct(fighter_gobj)->anim_speed <= fighter_gobj->anim_frame)
                        {
                            break;
                        }
                        else ms->script_wait = -fighter_gobj->anim_frame;
                    }
                    else if (ms->script_wait > 0.0F)
                    {
                        break;
                    }
                    ev_kind = ftMotionEventCast(ms, FTMotionEventDefault)->opcode;

                    switch (ev_kind)
                    {
                    case nFTMotionEventEnd:
                    case nFTMotionEventSyncWait:
                    case nFTMotionEventAsyncWait:
                    case nFTMotionEventSetDamageThrown:
                    case nFTMotionEventLoopBegin:
                    case nFTMotionEventLoopEnd:
                    case nFTMotionEventSubroutine:
                    case nFTMotionEventReturn:
                    case nFTMotionEventGoto:
                    case nFTMotionEventPauseScript:
                    case nFTMotionEventEffect:
                    case nFTMotionEventEffectItemHold:
                        ftMainParseMotionEvent(fighter_gobj, fp, ms, ev_kind);
                        break;

                    case nFTMotionEventMakeAttackColl:
                    case nFTMotionEventMakeAttackCollScaled:
                        ftMotionEventAdvance(ms, FTMotionEventMakeAttack);
                        break;

                    case nFTMotionEventSetAttackCollOffset:
                    case nFTMotionEventSetThrow:
                    case nFTMotionEventSetParallelScript:
                        ftMotionEventAdvance(ms, FTMotionEventDouble);
                        break;

                    case nFTMotionEventSetDamageCollPartID:
                        ftMotionEventAdvance(ms, FTMotionEventSetDamageCollPartID);
                        break;

                    default:
                        ftMotionEventAdvance(ms, FTMotionEventDefault);
                        break;
                    }
                }
            }
        }
    }
}

// 0x800E07D4
void ftMainPlayAnim(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->anim_desc.flags.is_use_transn_joint)
    {
        fp->anim_vel = fp->joints[nFTPartsJointTransN]->translate.vec.f;
    }
    ftParamUpdateAnimKeys(fighter_gobj);
    ftParamsUpdateFighterPartsTransform(fp->joints[nFTPartsJointTopN]);
}

// 0x800E0830 - Play fighter animation and run motion scripts normally
void ftMainPlayAnimEventsAll(GObj *fighter_gobj)
{
    ftMainPlayAnim(fighter_gobj);
    ftMainUpdateMotionEventsAll(fighter_gobj);
}

// 0x800E0858 - Play fighter animation and fast-forward motion scripts to current frame
void ftMainPlayAnimEventsForward(GObj *fighter_gobj)
{
    ftMainPlayAnim(fighter_gobj);
    ftMainUpdateMotionEventsForward(fighter_gobj);
}

/* ft/ftmain.c:959-1200 ftMainUpdateColAnim, verbatim: the colour
 * animation interpreter, one tic of a GMColAnim's two scripts
 * (gm/gmcolscripts.c's words, gm/gmdef.h's opcodes) and the per-tic
 * colour step after them. In the link for the screen
 * flash (src/dc/ifscreenflash.c), which is a GMColAnim of its own; the
 * fighter's colanim (fp->colanim) is not fed to it yet -- see
 * ftParamCheckSetFighterColAnimID in src/dc/ftparam.c. Every arm is
 * live: the effect arm ends in ftParamMakeEffect and the FGM arm in
 * func_800269C0_275C0, both the port's. */
// 0x800E0880
sb32 ftMainUpdateColAnim(GMColAnim *colanim, GObj *fighter_gobj, sb32 is_muted, sb32 is_effect_skip)
{
    s32 i, j;
    FTStruct *fp;
    GMColScript *cs;
    void *p_script;
    s32 effect_id;
    s32 joint_id;
    u32 flag;
    GMColEventDefault *def;
    Vec3f effect_offset;
    Vec3f effect_scatter;
    u32 ev_kind;
    s32 blend_frames;

    for (i = 0; i < ARRAY_COUNT(colanim->cs); i++)
    {
        cs = &colanim->cs[i]; // What's the point bruh

        if ((colanim->cs[i].p_script != NULL) && (colanim->cs[i].color_event_timer != 0))
        {
            colanim->cs[i].color_event_timer--;
        }
        while ((colanim->cs[i].p_script != NULL) && (cs->color_event_timer == 0))
        {
            ev_kind = gmColEventCast(colanim->cs[i].p_script, GMColEventDefault)->opcode;

            switch (ev_kind)
            {
            case nGMColEventEnd:
                for (j = 0; j < ARRAY_COUNT(colanim->cs); j++)
                {
                    if ((j != i) && (colanim->cs[j].p_script != NULL))
                    {
                        break;
                    }
                }
                if (j == ARRAY_COUNT(colanim->cs))
                {
                    return TRUE;
                }
                else colanim->cs[i].p_script = NULL;
                break;

            case nGMColEventWait:
                colanim->cs[i].color_event_timer = gmColEventCast(colanim->cs[i].p_script, GMColEventDefault)->value,
                gmColEventAdvance(colanim->cs[i].p_script, GMColEventDefault);
                break;

            case nGMColEventGoto:
                gmColEventAdvance(colanim->cs[i].p_script, GMColEventGoto1);
                colanim->cs[i].p_script = gmColEventCast(colanim->cs[i].p_script, GMColEventGoto2)->p_goto;
                break;

            case nGMColEventLoopBegin:
                colanim->cs[i].p_subroutine[colanim->cs[i].script_id++] = lbRelocGetFileData(void*, colanim->cs[i].p_script, sizeof(GMColEventDefault));
                colanim->cs[i].p_subroutine[colanim->cs[i].script_id++] = gmColEventCast(colanim->cs[i].p_script, GMColEventDefault)->value,
                gmColEventAdvance(colanim->cs[i].p_script, GMColEventDefault);
                break;

            case nGMColEventLoopEnd:
                if (--colanim->cs[i].loop_count[colanim->cs[i].script_id - 2] != 0)
                {
                    colanim->cs[i].p_script = colanim->cs[i].p_subroutine[colanim->cs[i].script_id - 2];
                }
                else gmColEventAdvance(colanim->cs[i].p_script, GMColEventDefault), colanim->cs[i].script_id -= 2;
                break;

            case nGMColEventSubroutine:
                gmColEventAdvance(colanim->cs[i].p_script, GMColEventSubroutine1);
                colanim->cs[i].p_subroutine[colanim->cs[i].script_id++] = lbRelocGetFileData(void*, colanim->cs[i].p_script, sizeof(GMColEventSubroutine1));
                colanim->cs[i].p_script = gmColEventCast(colanim->cs[i].p_script, GMColEventSubroutine2)->p_subroutine;
                break;

            case nGMColEventReturn:
                colanim->cs[i].p_script = colanim->cs[i].p_subroutine[--colanim->cs[i].script_id];
                break;

            case nGMColEventSetParallelScript:
                gmColEventAdvance(colanim->cs[i].p_script, GMColEventParallel1);

                if (colanim->cs[1].p_script == NULL)
                {
                    colanim->cs[1].p_script = gmColEventCast(colanim->cs[i].p_script, GMColEventParallel2)->p_script;
                    colanim->cs[1].color_event_timer = 0;
                    colanim->cs[1].script_id = 0;
                }
                gmColEventAdvance(colanim->cs[i].p_script, GMColEventParallel2);
                break;

            case nGMColEventClearColorAll:
                colanim->is_use_color1 = colanim->is_use_color2 = colanim->skeleton_id = 0;

                gmColEventAdvance(colanim->cs[i].p_script, GMColEventDefault);
                break;

            case nGMColEventSetColor1:
                colanim->is_use_color1 = TRUE;

                gmColEventAdvance(colanim->cs[i].p_script, GMColEventSetRGBA1);

                colanim->color1.r = gmColEventCast(colanim->cs[i].p_script, GMColEventSetRGBA2)->r;
                colanim->color1.g = gmColEventCast(colanim->cs[i].p_script, GMColEventSetRGBA2)->g;
                colanim->color1.b = gmColEventCast(colanim->cs[i].p_script, GMColEventSetRGBA2)->b;
                colanim->color1.a = gmColEventCast(colanim->cs[i].p_script, GMColEventSetRGBA2)->a;

                colanim->color1.ir = colanim->color1.ig = colanim->color1.ib = colanim->color1.ia = 0;

                gmColEventAdvance(colanim->cs[i].p_script, GMColEventSetRGBA2);
                break;

            case nGMColEventSetColor2:
                colanim->is_use_color2 = TRUE;

                gmColEventAdvance(colanim->cs[i].p_script, GMColEventSetRGBA1);

                colanim->color2.r = gmColEventCast(colanim->cs[i].p_script, GMColEventSetRGBA2)->r;
                colanim->color2.g = gmColEventCast(colanim->cs[i].p_script, GMColEventSetRGBA2)->g;
                colanim->color2.b = gmColEventCast(colanim->cs[i].p_script, GMColEventSetRGBA2)->b;
                colanim->color2.a = gmColEventCast(colanim->cs[i].p_script, GMColEventSetRGBA2)->a;

                colanim->color2.ir = colanim->color2.ig = colanim->color2.ib = colanim->color2.ia = 0;

                gmColEventAdvance(colanim->cs[i].p_script, GMColEventSetRGBA2);
                break;

            case nGMColEventBlendColor1:
                blend_frames = gmColEventCast(colanim->cs[i].p_script, GMColEventBlendRGBA1)->blend_frames;

                gmColEventAdvance(colanim->cs[i].p_script, GMColEventBlendRGBA1);

                colanim->color1.ir = (s32) (gmColEventCast(colanim->cs[i].p_script, GMColEventBlendRGBA2)->r - colanim->color1.r) / blend_frames;
                colanim->color1.ig = (s32) (gmColEventCast(colanim->cs[i].p_script, GMColEventBlendRGBA2)->g - colanim->color1.g) / blend_frames;
                colanim->color1.ib = (s32) (gmColEventCast(colanim->cs[i].p_script, GMColEventBlendRGBA2)->b - colanim->color1.b) / blend_frames;
                colanim->color1.ia = (s32) (gmColEventCast(colanim->cs[i].p_script, GMColEventBlendRGBA2)->a - colanim->color1.a) / blend_frames;

                gmColEventAdvance(colanim->cs[i].p_script, GMColEventBlendRGBA2);
                break;

            case nGMColEventBlendColor2:
                blend_frames = gmColEventCast(colanim->cs[i].p_script, GMColEventBlendRGBA1)->blend_frames;

                gmColEventAdvance(colanim->cs[i].p_script, GMColEventBlendRGBA1);

                colanim->color2.ir = (s32) (gmColEventCast(colanim->cs[i].p_script, GMColEventBlendRGBA2)->r - colanim->color2.r) / blend_frames;
                colanim->color2.ig = (s32) (gmColEventCast(colanim->cs[i].p_script, GMColEventBlendRGBA2)->g - colanim->color2.g) / blend_frames;
                colanim->color2.ib = (s32) (gmColEventCast(colanim->cs[i].p_script, GMColEventBlendRGBA2)->b - colanim->color2.b) / blend_frames;
                colanim->color2.ia = (s32) (gmColEventCast(colanim->cs[i].p_script, GMColEventBlendRGBA2)->a - colanim->color2.a) / blend_frames;

                gmColEventAdvance(colanim->cs[i].p_script, GMColEventBlendRGBA2);
                break;

            case nGMColEventEffect:
            case nGMColEventEffectItemHoldOffset:
                if (is_effect_skip == FALSE)
                {
                    fp = ftGetStruct(fighter_gobj);

                    joint_id = ftParamGetJointID(fp, gmColEventCast(colanim->cs[i].p_script, GMColEventMakeEffect1)->joint_id);
                    effect_id = gmColEventCast(colanim->cs[i].p_script, GMColEventMakeEffect1)->effect_id;
                    flag = gmColEventCast(colanim->cs[i].p_script, GMColEventMakeEffect1)->flag;

                    gmColEventAdvance(colanim->cs[i].p_script, GMColEventMakeEffect1);

                    effect_offset.x = gmColEventCast(colanim->cs[i].p_script, GMColEventMakeEffect2)->off_x;
                    effect_offset.y = gmColEventCast(colanim->cs[i].p_script, GMColEventMakeEffect2)->off_y;

                    gmColEventAdvance(colanim->cs[i].p_script, GMColEventMakeEffect2);

                    effect_offset.z = gmColEventCast(colanim->cs[i].p_script, GMColEventMakeEffect3)->off_z;
                    effect_scatter.x = gmColEventCast(colanim->cs[i].p_script, GMColEventMakeEffect3)->rng_x;

                    gmColEventAdvance(colanim->cs[i].p_script, GMColEventMakeEffect3);

                    effect_scatter.y = gmColEventCast(colanim->cs[i].p_script, GMColEventMakeEffect4)->rng_y;
                    effect_scatter.z = gmColEventCast(colanim->cs[i].p_script, GMColEventMakeEffect4)->rng_z;

                    gmColEventAdvance(colanim->cs[i].p_script, GMColEventMakeEffect4);

                    ftParamMakeEffect(fighter_gobj, effect_id, joint_id, &effect_offset, &effect_scatter, fp->lr, (ev_kind == nGMColEventEffectItemHoldOffset) ? TRUE : FALSE, flag);
                }
                else gmColEventAdvance(colanim->cs[i].p_script, GMColEventMakeEffect);
                break;

            case nGMColEventSetLight:
                colanim->is_use_light = TRUE;

                colanim->light_angle_x = gmColEventCast(colanim->cs[i].p_script, GMColEventSetLight)->light1;
                colanim->light_angle_y = gmColEventCast(colanim->cs[i].p_script, GMColEventSetLight)->light2;

                gmColEventAdvance(colanim->cs[i].p_script, GMColEventSetLight);
                break;

            case nGMColEventClearLight:
                colanim->is_use_light = FALSE;

                gmColEventAdvance(colanim->cs[i].p_script, GMColEventDefault);
                break;

            case nGMColEventPlayFGM:
                if (is_muted == FALSE)
                {
                    func_800269C0_275C0(gmColEventCastAdvance(colanim->cs[i].p_script, GMColEventDefault)->value);
                }
                else gmColEventAdvance(colanim->cs[i].p_script, GMColEventDefault);
                break;

            case nGMColEventSetSkeletonID:
                colanim->skeleton_id = gmColEventCastAdvance(colanim->cs[i].p_script, GMColEventDefault)->value;
                break;

            default:
                break;
            }
        }
    }
    if (colanim->is_use_color1)
    {
        colanim->color1.r += colanim->color1.ir;
        colanim->color1.g += colanim->color1.ig;
        colanim->color1.b += colanim->color1.ib;
        colanim->color1.a += colanim->color1.ia;
    }
    if (colanim->is_use_color2)
    {
        colanim->color2.r += colanim->color2.ir;
        colanim->color2.g += colanim->color2.ig;
        colanim->color2.b += colanim->color2.ib;
        colanim->color2.a += colanim->color2.ia;
    }
    if (colanim->length != 0)
    {
        colanim->length--;

        if (colanim->length == 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/* ====================================================================
 * The hit half of ft/ftmain.c -- the fighter's other four
 * GObjProcesses (ft/ftmanager.c:860-863) and what they run, verbatim,
 * the 1P game's bonus counts included. What a landed hit turns into
 * -- the damage statuses, shield break, rebound -- is
 * ft/ftcommon/ftcommondamage.c and friends, in src/dc/ftcommon.c.
 * ==================================================================== */
#include <gr/ground.h>
#include <gm/gmcollision.h>
#include <lbcommon.h>            /* lbCommonMakePositionFGM */
#include "overlay.h"


/* ft/ftmain.c:20-49 */

u16 dFTMainHitCollisionFGMs[/* */][nGMHitLevelEnumCount] =
{
    { nSYAudioFGMPunchS,             nSYAudioFGMPunchM,             nSYAudioFGMPunchL             },    // Punch
    { nSYAudioFGMKickS,              nSYAudioFGMKickM,              nSYAudioFGMKickL              },    // Kick
    { nSYAudioFGMMarioSpecialHiCoin, nSYAudioFGMMarioSpecialHiCoin, nSYAudioFGMMarioSpecialHiCoin },    // Coin
    { nSYAudioFGMBurnS,              nSYAudioFGMBurnM,              nSYAudioFGMBurnL              },    // Burn
    { nSYAudioFGMShockS,             nSYAudioFGMShockM,             nSYAudioFGMShockL             },    // Shock
    { nSYAudioFGMSlashS,             nSYAudioFGMSlashM,             nSYAudioFGMSlashL             },    // Slash
    { nSYAudioFGMHarisenHit,         nSYAudioFGMHarisenHit,         nSYAudioFGMHarisenHit         },    // Fan / Slap
    { nSYAudioFGMPunchM,             nSYAudioFGMPunchL,             nSYAudioFGMBatHit             }     // Bat
};

GRAttackColl dFTMainGroundHitCollisionAttributes[/* */] =
{
    {  4,  1, 361, 100, 100, 0, nGMHitElementFire  },
    {  5, 10,  90, 100, 200, 0, nGMHitElementFire  },
    {  6, 10,  90, 100, 100, 0, nGMHitElementFire  },
    {  7, 10, 361, 100,  80, 0, nGMHitElementSlash },
    {  8,  1,  90, 100, 100, 0, nGMHitElementFire  },
    {  9,  1,  90, 100, 100, 0, nGMHitElementFire  }
};


/* ft/ftmain.c:130-149 */
GRObstacle sFTMainGroundObstacles[2];
GRHazard sFTMainGroundHazards[1];
s32 sFTMainGroundObstaclesNum;
s32 sFTMainGroundHazardsNum;
sb32 gFTMainIsDamageDetect[GMCOMMON_PLAYERS_MAX];
sb32 gFTMainIsAttackDetect[GMCOMMON_PLAYERS_MAX];
s32 sFTMainHitLogID;
FTHitLog sFTMainHitLogs[10];


void ftMainClearGroundElementsAll(void)
{
    s32 i;

    sFTMainGroundObstaclesNum = sFTMainGroundHazardsNum = 0;

    for (i = 0; i < ARRAY_COUNT(sFTMainGroundObstacles); i++)
    {
        sFTMainGroundObstacles[i].gobj = NULL;
    }
    for (i = 0; i < ARRAY_COUNT(sFTMainGroundHazards); i++)
    {
        sFTMainGroundHazards[i].gobj = NULL;
    }
}

sb32 ftMainCheckAddGroundObstacle(GObj *gobj, sb32(*proc_update)(GObj*, GObj*, s32*))
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sFTMainGroundObstacles); i++)
    {
        if (sFTMainGroundObstacles[i].gobj == NULL)
        {
            sFTMainGroundObstacles[i].gobj = gobj;
            sFTMainGroundObstacles[i].proc_update = proc_update;
            sFTMainGroundObstaclesNum++;

            return TRUE;
        }
    }
    return FALSE;
}

void ftMainClearGroundObstacle(GObj *gobj)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sFTMainGroundObstacles); i++)
    {
        if (sFTMainGroundObstacles[i].gobj == gobj)
        {
            sFTMainGroundObstacles[i].gobj = NULL;
            sFTMainGroundObstaclesNum--;

            break;
        }
    }
}

sb32 ftMainCheckAddGroundHazard(GObj *gobj, sb32(*proc_update)(GObj*, GObj*, GRAttackColl**, s32*))
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sFTMainGroundHazards); i++)
    {
        if (sFTMainGroundHazards[i].gobj == NULL)
        {
            sFTMainGroundHazards[i].gobj = gobj;
            sFTMainGroundHazards[i].proc_update = proc_update;

            sFTMainGroundHazardsNum++;

            return TRUE;
        }
    }
    return FALSE;
}

void ftMainClearHazard(GObj *gobj)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sFTMainGroundHazards); i++)
    {
        if (sFTMainGroundHazards[i].gobj == gobj)
        {
            sFTMainGroundHazards[i].gobj = NULL;
            sFTMainGroundHazardsNum--;

            break;
        }
    }
}

void ftMainSetHitHazard(GObj *gobj, GObj *fighter_gobj, FTStruct *fp, s32 kind)
{
    switch (kind)
    {
    case nGMHitEnvironmentTwister:
        ftCommonTwisterSetStatus(fighter_gobj, gobj);
        break;

    case nGMHitEnvironmentTaruCann:
        ftCommonTaruCannSetStatus(fighter_gobj, gobj);
        break;
    }
}

void ftMainSearchHitHazard(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    GRObstacle *gh = &sFTMainGroundObstacles[0];

    if (!(fp->is_ghost))
    {
        s32 i;

        if (!(fp->hitlag_tics))
        {
            if (fp->twister_wait)
            {
                fp->twister_wait--;
            }
            if (fp->tarucann_wait)
            {
                fp->tarucann_wait--;
            }
        }
        for (i = 0; i < sFTMainGroundObstaclesNum; i++, gh++)
        {
            if (gh->gobj != NULL)
            {
                s32 kind;

                if (gh->proc_update(gh->gobj, fighter_gobj, &kind) != FALSE)
                {
                    ftMainSetHitHazard(gh->gobj, fighter_gobj, fp, kind);
                }
            }
        }
    }
}

void ftMainUpdateVelDamageGround(FTStruct *fp, f32 move)
{
    if (fp->physics.vel_damage_ground < 0.0F)
    {
        fp->physics.vel_damage_ground += move;

        if (fp->physics.vel_damage_ground > 0.0F)
        {
            fp->physics.vel_damage_ground = 0.0F;
        }
    }
    else
    {
        fp->physics.vel_damage_ground -= move;

        if (fp->physics.vel_damage_ground < 0.0F)
        {
            fp->physics.vel_damage_ground = 0.0F;
        }
    }
}

void ftMainProcPhysicsMapCapture(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if ((fp->capture_gobj != NULL && !(fp->is_catch_or_capture)) || (fp->catch_gobj != NULL && fp->is_catch_or_capture))
    {
        ftMainProcPhysicsMap(fighter_gobj);
    }
}

void ftMainSetHitInteractStats(FTStruct *fp, u32 attack_group_id, GObj *victim_gobj, s32 attack_type, u32 victim_group_id, sb32 ignore_damage_or_hit)
{
    s32 i, j;

    for (i = 0; i < ARRAY_COUNT(fp->attack_colls); i++)
    {
        if (i == ARRAY_COUNT(fp->attack_colls)); // WAT

        if ((fp->attack_colls[i].attack_state != nGMAttackStateOff) && (attack_group_id == fp->attack_colls[i].group_id))
        {
            for (j = 0; j < ARRAY_COUNT(fp->attack_colls[i].attack_records); j++)
            {
                if (victim_gobj == fp->attack_colls[i].attack_records[j].victim_gobj)
                {
                    switch (attack_type)
                    {
                    case nGMHitTypeDamage:
                        fp->attack_colls[i].attack_records[j].victim_flags.is_interact_hurt = TRUE;
                        break;

                    case nGMHitTypeShield:
                        fp->attack_colls[i].attack_records[j].victim_flags.is_interact_shield = TRUE;
                        break;

                    case nGMHitTypeAttack:
                        fp->attack_colls[i].attack_records[j].victim_flags.group_id = victim_group_id;
                        break;

                    default:
                        break;
                    }
                    break;
                }
            }
            if (j == ARRAY_COUNT(fp->attack_colls[i].attack_records))
            {
                for (j = 0; j < ARRAY_COUNT(fp->attack_colls[i].attack_records); j++)
                {
                    if (fp->attack_colls[i].attack_records[j].victim_gobj == NULL) break;
                }
                if (j == ARRAY_COUNT(fp->attack_colls[i].attack_records)) j = 0;

                fp->attack_colls[i].attack_records[j].victim_gobj = victim_gobj;

                switch (attack_type)
                {
                case nGMHitTypeDamage:
                    fp->attack_colls[i].attack_records[j].victim_flags.is_interact_hurt = TRUE;
                    break;

                case nGMHitTypeShield:
                    fp->attack_colls[i].attack_records[j].victim_flags.is_interact_shield = TRUE;
                    break;

                case nGMHitTypeAttack:
                    fp->attack_colls[i].attack_records[j].victim_flags.group_id = victim_group_id;
                    break;

                default:
                    break;
                }
            }
            if (ignore_damage_or_hit == 0)
            {
                gFTMainIsDamageDetect[i] = FALSE;
            }
            else gFTMainIsAttackDetect[i] = FALSE;
        }
    }
}

void ftMainSetHitRebound(GObj *attacker_gobj, FTStruct *fp, FTAttackColl *attack_coll, GObj *victim_gobj)
{
    if (fp->attack_shield_push < attack_coll->damage)
    {
        fp->attack_shield_push = attack_coll->damage;

        if ((attack_coll->can_rebound) && (fp->ga == nMPKineticsGround))
        {
#if defined(REGION_US)
            fp->attack_rebound = (fp->attack_shield_push * 1.62F) + 4.0F;
#else
            fp->attack_rebound = (fp->attack_shield_push * 1.75F) + 4.0F;
#endif
            fp->hit_lr = (DObjGetStruct(attacker_gobj)->translate.vec.f.x < DObjGetStruct(victim_gobj)->translate.vec.f.x) ? +1 : -1;
        }
    }
}

void ftMainUpdateAttackStatFighter(FTStruct *other_fp, FTAttackColl *other_hit, FTStruct *this_fp, FTAttackColl *this_hit, GObj *other_gobj, GObj *this_gobj)
{
    Vec3f impact_pos;

    gmCollisionGetFighterAttacksPosition(&impact_pos, this_hit, other_hit);

    if ((this_hit->damage - 10) < other_hit->damage)
    {
        ftMainSetHitInteractStats(this_fp, this_hit->group_id, other_gobj, nGMHitTypeAttack, other_hit->group_id, 1);
        ftMainSetHitRebound(this_gobj, this_fp, this_hit, other_gobj);
        efManagerSetOffMakeEffect(&impact_pos, this_hit->damage);

        if ((gSCManagerBattleState->game_type == nSCBattleGameType1PGame) && (this_hit->damage >= 20) && (other_fp->player == gSCManagerSceneData.player))
        {
            gSC1PGameBonusGiantImpact = TRUE;
        }
    }
    if ((other_hit->damage - 10) < this_hit->damage)
    {
        ftMainSetHitInteractStats(other_fp, other_hit->group_id, this_gobj, nGMHitTypeAttack, this_hit->group_id, 0);
        ftMainSetHitRebound(other_gobj, other_fp, other_hit, this_gobj);
        efManagerSetOffMakeEffect(&impact_pos, other_hit->damage);

        if ((gSCManagerBattleState->game_type == nSCBattleGameType1PGame) && (other_hit->damage >= 20) && (this_fp->player == gSCManagerSceneData.player))
        {
            gSC1PGameBonusGiantImpact = TRUE;
        }
    }
}

void ftMainUpdateShieldStatFighter(FTStruct *attacker_fp, FTAttackColl *attack_coll, FTStruct *victim_fp, GObj *attacker_gobj, GObj *victim_gobj)
{
    Vec3f impact_pos;

    ftMainSetHitInteractStats(attacker_fp, attack_coll->group_id, victim_gobj, nGMHitTypeShield, 0, 0);

    if (attacker_fp->attack_shield_push < attack_coll->damage)
    {
        attacker_fp->attack_shield_push = attack_coll->damage;
    }
    victim_fp->shield_damage_total += (attack_coll->damage + attack_coll->shield_damage);

    if (victim_fp->shield_damage < attack_coll->damage)
    {
        victim_fp->shield_damage = attack_coll->damage;

        victim_fp->shield_lr = (DObjGetStruct(victim_gobj)->translate.vec.f.x < DObjGetStruct(attacker_gobj)->translate.vec.f.x) ? +1 : -1;

        victim_fp->shield_player = attacker_fp->player;
    }
    gmCollisionGetFighterAttackShieldPosition(&impact_pos, attack_coll, victim_gobj, victim_fp->joints[nFTPartsJointYRotN]);
    efManagerSetOffMakeEffect(&impact_pos, attack_coll->damage);
}

void ftMainUpdateCatchStatFighter(FTStruct *attacker_fp, FTAttackColl *attack_coll, FTStruct *victim_fp, GObj *attacker_gobj, GObj *victim_gobj)
{
    f32 dist;

    ftMainSetHitInteractStats(attacker_fp, attack_coll->group_id, victim_gobj, nGMHitTypeDamage, 0, 1);

    if (DObjGetStruct(victim_gobj)->translate.vec.f.x < DObjGetStruct(attacker_gobj)->translate.vec.f.x)
    {
        dist = -(DObjGetStruct(victim_gobj)->translate.vec.f.x - DObjGetStruct(attacker_gobj)->translate.vec.f.x);
    }
    else dist = DObjGetStruct(victim_gobj)->translate.vec.f.x - DObjGetStruct(attacker_gobj)->translate.vec.f.x;

    if (dist < attacker_fp->search_gobj_dist)
    {
        attacker_fp->search_gobj_dist = dist;
        attacker_fp->search_gobj = victim_gobj;
    }
}

void ftMainPlayHitSFX(FTStruct *fp, FTAttackColl *attack_coll)
{
    if ((fp->p_sfx != NULL) && (fp->p_sfx->sfx_id != 0) && (fp->p_sfx->sfx_id == fp->sfx_id))
    {
        func_80026738_27338(fp->p_sfx);
    }
    fp->p_sfx = NULL, fp->sfx_id = 0;

    lbCommonMakePositionFGM(dFTMainHitCollisionFGMs[attack_coll->fgm_kind][attack_coll->fgm_level], fp->joints[nFTPartsJointTopN]->translate.vec.f.x);
}

sb32 ftMainCheckGetUpdateDamage(FTStruct *fp, s32 *damage)
{
    if (fp->is_damage_resist)
    {
        fp->damage_resist -= *damage;

        if (fp->damage_resist <= 0)
        {
            fp->is_damage_resist = FALSE;

            *damage = -fp->damage_resist;
        }
    }
    if (!(fp->is_damage_resist))
    {
        fp->damage_queue += *damage;

        if (fp->damage_lag < *damage)
        {
            fp->damage_lag = *damage;
        }
        return TRUE;
    }
    else return FALSE;
}

void ftMainUpdateDamageStatFighter(FTStruct *attacker_fp, FTAttackColl *attack_coll, FTStruct *victim_fp, FTDamageColl *damage_coll, GObj *attacker_gobj, GObj *victim_gobj)
{
    s32 damage;
    s32 attacker_player;
    s32 attacker_player_num;
    s32 unused;
    Vec3f impact_pos;

    ftMainSetHitInteractStats(attacker_fp, attack_coll->group_id, victim_gobj, nGMHitTypeDamage, 0, FALSE);

    damage = ftParamGetCapturedDamage(victim_fp, attack_coll->damage);

    if (attacker_fp->attack_damage < damage)
    {
        attacker_fp->attack_damage = damage;
    }
    if
    (
        (victim_fp->special_hitstatus == nGMHitStatusNormal) &&
        (victim_fp->star_hitstatus == nGMHitStatusNormal) &&
        (victim_fp->hitstatus == nGMHitStatusNormal)      &&
        (damage_coll->hitstatus == nGMHitStatusNormal)
    )
    {
        if (ftMainCheckGetUpdateDamage(victim_fp, &damage) != FALSE)
        {
            if (attacker_fp->throw_gobj != NULL)
            {
                attacker_player = attacker_fp->throw_player;
                attacker_player_num = attacker_fp->throw_player_num;
            }
            else
            {
                attacker_player = attacker_fp->player;
                attacker_player_num = attacker_fp->player_num;
            }
            if (sFTMainHitLogID < ARRAY_COUNT(sFTMainHitLogs))
            {
                FTHitLog *hitlog = &sFTMainHitLogs[sFTMainHitLogID];

                hitlog->attacker_object_class = nFTHitLogObjectFighter;
                hitlog->attack_coll = attack_coll;
                hitlog->attacker_gobj = attacker_gobj;
                hitlog->damage_coll = damage_coll;
                hitlog->attacker_player = attacker_player;
                hitlog->attacker_player_num = attacker_player_num;

                sFTMainHitLogID++;
            }
            ftParamUpdatePlayerBattleStats(attacker_player, victim_fp->player, damage);
            ftParamUpdateStaleQueue(attacker_player, victim_fp->player, attack_coll->motion_attack_id, attack_coll->motion_count);
        }
        else
        {
            gmCollisionGetFighterAttackDamagePosition(&impact_pos, attack_coll, damage_coll);
            efManagerSetOffMakeEffect(&impact_pos, damage);
        }
    }
    else
    {
        gmCollisionGetFighterAttackDamagePosition(&impact_pos, attack_coll, damage_coll);
        efManagerSetOffMakeEffect(&impact_pos, damage);
    }
    ftMainPlayHitSFX(attacker_fp, attack_coll);
}

/* ft/ftmain.c:2213-2592, the weapon and item halves of the hit stats,
 * verbatim. Each is the fighter version above with the attacker's side
 * read off the WPStruct/ITStruct instead of a second FTStruct; the
 * weapon's and item's own bookkeeping (attack records, hit_*_damage,
 * reflect/absorb owners) goes through wp/wpprocess.c's and it/itprocess.c's
 * own setters. */
void ftMainUpdateAttackStatWeapon(WPStruct *ip, WPAttackColl *wp_attack_coll, s32 index, FTStruct *fp, FTAttackColl *ft_attack_coll, GObj *weapon_gobj, GObj *fighter_gobj)
{
    s32 damage = wpMainGetStaledDamage(ip);
    Vec3f impact_pos;

    gmCollisionGetWeaponAttackFighterAttackPosition(&impact_pos, wp_attack_coll, index, ft_attack_coll);

    if ((ft_attack_coll->damage - 10) < damage)
    {
        ftMainSetHitInteractStats(fp, ft_attack_coll->group_id, weapon_gobj, nGMHitTypeAttack, 0, TRUE);
        ftMainSetHitRebound(fighter_gobj, fp, ft_attack_coll, weapon_gobj);
        efManagerSetOffMakeEffect(&impact_pos, ft_attack_coll->damage);
    }

    if ((damage - 10) < ft_attack_coll->damage)
    {
        wpProcessUpdateHitInteractStats(ip, wp_attack_coll, fighter_gobj, nGMHitTypeAttack, ft_attack_coll->group_id);

        if (ip->hit_attack_damage < damage)
        {
            ip->hit_attack_damage = damage;
        }
        efManagerSetOffMakeEffect(&impact_pos, damage);

        if ((gSCManagerBattleState->game_type == nSCBattleGameType1PGame) && ((damage - 10) >= 10) && (fp->player == gSCManagerSceneData.player))
        {
            gSC1PGameBonusGiantImpact = TRUE;
        }
    }
}

void ftMainUpdateShieldStatWeapon(WPStruct *wp, WPAttackColl *wp_attack_coll, s32 attack_id, FTStruct *fp, GObj *weapon_gobj, GObj *fighter_gobj, f32 angle, Vec3f *dir)
{
    s32 damage = wpMainGetStaledDamage(wp);
    Vec3f impact_pos;

    wpProcessUpdateHitInteractStats(wp, wp_attack_coll, fighter_gobj, (wp_attack_coll->can_rehit_shield) ? nGMHitTypeShieldRehit : nGMHitTypeShield, 0);

    if (wp->hit_shield_damage < damage)
    {
        wp->hit_shield_damage = damage;

        wp->shield_collide_angle = angle;

        wp->shield_collide_dir.x = 0.0F;
        wp->shield_collide_dir.y = 0.0F;
        wp->shield_collide_dir.z = (fp->lr == +1) ? -dir->x : dir->x;

        syVectorNorm3D(&wp->shield_collide_dir);
    }
    fp->shield_damage_total += damage + wp_attack_coll->shield_damage;

    if (fp->shield_damage < damage)
    {
        fp->shield_damage = damage;

        fp->shield_lr = (wp->physics.vel_air.x < 0.0F) ? +1 : -1;

        fp->shield_player = wp->player;
    }
    gmCollisionGetWeaponAttackShieldPosition(&impact_pos, wp_attack_coll, attack_id, fighter_gobj, fp->joints[nFTPartsJointYRotN]);
    efManagerSetOffMakeEffect(&impact_pos, wp_attack_coll->shield_damage + damage);
}

void ftMainUpdateReflectorStatWeapon(WPStruct *wp, WPAttackColl *wp_attack_coll, FTStruct *fp, GObj *fighter_gobj)
{
    s32 damage = wpMainGetStaledDamage(wp);

    wpProcessUpdateHitInteractStats(wp, wp_attack_coll, fighter_gobj, nGMHitTypeReflect, 0);

    if (fp->special_coll->damage_resist < damage)
    {
        if (wp_attack_coll->can_rehit_fighter)
        {
            if (wp->hit_refresh_damage < damage)
            {
                wp->hit_refresh_damage = damage;
            }
        }
        else if (wp->hit_normal_damage < damage)
        {
            wp->hit_normal_damage = damage;
        }
        fp->reflect_damage = damage;

        fp->reflect_lr = (DObjGetStruct(fighter_gobj)->translate.vec.f.x < DObjGetStruct(wp->weapon_gobj)->translate.vec.f.x) ? +1 : -1;
    }
    else
    {
        wp->reflect_gobj = fighter_gobj;

        /*
         * Oversight (the decomp's REGION_US note): attack_id is not set to
         * that of the reflecting fighter. This causes whatever move the
         * original attacker used to summon the weapon to be added to the
         * reflecting fighter's staling queue if valid.
         *
         * e.g. Fox reflects Mario's Fire Ball -> Fox's Blaster stales, even though he hasn't used it.
         */
        wp->reflect_stat_flags = fp->stat_flags;
        wp->reflect_stat_count = fp->stat_count;

        fp->reflect_lr = (DObjGetStruct(fighter_gobj)->translate.vec.f.x < DObjGetStruct(wp->weapon_gobj)->translate.vec.f.x) ? +1 : -1;
    }
}

void ftMainUpdateAbsorbStatWeapon(WPStruct *wp, WPAttackColl *wp_attack_coll, FTStruct *fp, GObj *fighter_gobj)
{
    s32 damage = wpMainGetStaledDamage(wp);

    wpProcessUpdateHitInteractStats(wp, wp_attack_coll, fighter_gobj, nGMHitTypeAbsorb, 0);

    wp->absorb_gobj = fighter_gobj;

    fp->absorb_lr = (DObjGetStruct(fighter_gobj)->translate.vec.f.x < DObjGetStruct(wp->weapon_gobj)->translate.vec.f.x) ? +1 : -1;

    if (!(wp_attack_coll->can_not_heal))
    {
        fp->percent_damage -= (s32)(damage * 2.0F);

        if (fp->percent_damage < 0)
        {
            fp->percent_damage = 0;
        }
        gSCManagerBattleState->players[fp->player].stock_damage_all = fp->percent_damage;
    }
}

void ftMainUpdateDamageStatWeapon(WPStruct *wp, WPAttackColl *wp_attack_coll, s32 wp_attack_id, FTStruct *fp, FTDamageColl *damage_coll, GObj *weapon_gobj, GObj *fighter_gobj)
{
    s32 temp_damage = wpMainGetStaledDamage(wp);
    s32 damage;

    wpProcessUpdateHitInteractStats(wp, wp_attack_coll, fighter_gobj, (wp_attack_coll->can_rehit_fighter) ? nGMHitTypeDamageRehit : nGMHitTypeDamage, 0);

    damage = ftParamGetCapturedDamage(fp, temp_damage);

    if (wp_attack_coll->can_rehit_fighter)
    {
        if (wp->hit_refresh_damage < damage)
        {
            wp->hit_refresh_damage = damage;
        }
    }
    else if (wp->hit_normal_damage < damage)
    {
        wp->hit_normal_damage = damage;
    }
    if
    (
        (fp->special_hitstatus == nGMHitStatusNormal)   &&
        (fp->star_hitstatus == nGMHitStatusNormal)      &&
        (fp->hitstatus == nGMHitStatusNormal)           &&
        (damage_coll->hitstatus == nGMHitStatusNormal)  &&
        (ftMainCheckGetUpdateDamage(fp, &damage) != FALSE)
    )
    {
        if (sFTMainHitLogID < ARRAY_COUNT(sFTMainHitLogs))
        {
            FTHitLog *hitlog = &sFTMainHitLogs[sFTMainHitLogID];

            hitlog->attacker_object_class = nFTHitLogObjectWeapon;
            hitlog->attack_coll = wp_attack_coll;
            hitlog->attack_id = wp_attack_id;
            hitlog->attacker_gobj = weapon_gobj;
            hitlog->damage_coll = damage_coll;
            hitlog->attacker_player = wp->player;
            hitlog->attacker_player_num = wp->player_num;

            sFTMainHitLogID++;
        }
        ftParamUpdatePlayerBattleStats(wp->player, fp->player, damage);
        ftParamUpdateStaleQueue(wp->player, fp->player, wp_attack_coll->motion_attack_id, wp_attack_coll->motion_count);
    }
    func_800269C0_275C0(wp_attack_coll->fgm_id);
}

void ftMainUpdateAttackStatItem(ITStruct *ip, ITAttackColl *it_attack_coll, s32 it_attack_id, FTStruct *fp, FTAttackColl *ft_attack_coll, GObj *item_gobj, GObj *fighter_gobj)
{
    s32 damage = itMainGetDamageOutput(ip);
    Vec3f impact_pos;

    gmCollisionGetItemAttackFighterAttackPosition(&impact_pos, it_attack_coll, it_attack_id, ft_attack_coll);

    if ((ft_attack_coll->damage - 10) < damage)
    {
        ftMainSetHitInteractStats(fp, ft_attack_coll->group_id, item_gobj, nGMHitTypeAttack, 0, 1);
        ftMainSetHitRebound(fighter_gobj, fp, ft_attack_coll, item_gobj);
        efManagerSetOffMakeEffect(&impact_pos, ft_attack_coll->damage);
    }
    if ((damage - 10) < ft_attack_coll->damage)
    {
        itProcessSetHitInteractStats(it_attack_coll, fighter_gobj, nGMHitTypeAttack, ft_attack_coll->group_id);

        if (ip->hit_attack_damage < damage)
        {
            ip->hit_attack_damage = damage;
        }
        efManagerSetOffMakeEffect(&impact_pos, damage);

        if ((gSCManagerBattleState->game_type == nSCBattleGameType1PGame) && ((damage - 10) >= 10) && (fp->player == gSCManagerSceneData.player))
        {
            gSC1PGameBonusGiantImpact = TRUE;
        }
    }
}

void ftMainUpdateShieldStatItem(ITStruct *ip, ITAttackColl *it_attack_coll, s32 attack_id, FTStruct *fp, GObj *item_gobj, GObj *fighter_gobj, f32 angle, Vec3f *vec)
{
    s32 damage = itMainGetDamageOutput(ip);
    Vec3f impact_pos;

    itProcessSetHitInteractStats(it_attack_coll, fighter_gobj, (it_attack_coll->can_rehit_shield) ? nGMHitTypeShieldRehit : nGMHitTypeShield, 0);

    if (ip->hit_shield_damage < damage)
    {
        ip->hit_shield_damage = damage;

        ip->shield_collide_angle = angle;

        ip->shield_collide_dir.x = 0.0F;
        ip->shield_collide_dir.y = 0.0F;
        ip->shield_collide_dir.z = (fp->lr == +1) ? -vec->x : vec->x;

        syVectorNorm3D(&ip->shield_collide_dir);
    }
    fp->shield_damage_total += damage + it_attack_coll->shield_damage;

    if (fp->shield_damage < damage)
    {
        fp->shield_damage = damage;

        fp->shield_lr = (ip->physics.vel_air.x < 0.0F) ? +1 : -1;

        fp->shield_player = ip->player;
    }
    gmCollisionGetItemAttackShieldPosition(&impact_pos, it_attack_coll, attack_id, fighter_gobj, fp->joints[nFTPartsJointYRotN]);
    efManagerSetOffMakeEffect(&impact_pos, it_attack_coll->shield_damage + damage);
}

void ftMainUpdateReflectorStatItem(ITStruct *ip, ITAttackColl *it_attack_coll, FTStruct *fp, GObj *fighter_gobj)
{
    s32 damage = itMainGetDamageOutput(ip);

    itProcessSetHitInteractStats(it_attack_coll, fighter_gobj, nGMHitTypeReflect, 0);

    if (fp->special_coll->damage_resist < damage)
    {
        if (it_attack_coll->can_rehit_fighter)
        {
            if (ip->hit_refresh_damage < damage)
            {
                ip->hit_refresh_damage = damage;
            }
        }
        else if (ip->hit_normal_damage < damage)
        {
            ip->hit_normal_damage = damage;
        }
        fp->reflect_damage = damage;

        fp->reflect_lr = (DObjGetStruct(fighter_gobj)->translate.vec.f.x < DObjGetStruct(ip->item_gobj)->translate.vec.f.x) ? +1 : -1;
    }
    else
    {
        ip->reflect_gobj = fighter_gobj;

        /*
         * Oversight (the decomp's REGION_US note): attack_id is not set to
         * that of the reflecting fighter. This causes whatever move the
         * original attacker used the item with to be added to the
         * reflecting fighter's staling queue if valid.
         *
         * e.g. Fox reflects an item thrown from LightThrowF4 -> Fox's own LightThrowF4 stales, even though he hasn't used it.
         */
        ip->reflect_stat_flags = fp->stat_flags;
        ip->reflect_stat_count = fp->stat_count;

        fp->reflect_lr = (DObjGetStruct(fighter_gobj)->translate.vec.f.x < DObjGetStruct(ip->item_gobj)->translate.vec.f.x) ? +1 : -1;
    }
}

void ftMainUpdateDamageStatItem(ITStruct *ip, ITAttackColl *it_attack_coll, s32 attack_id, FTStruct *fp, FTDamageColl *damage_coll, GObj *item_gobj, GObj *fighter_gobj)
{
    s32 damage_temp = itMainGetDamageOutput(ip);
    s32 damage;
    s32 hit_lr;

    itProcessSetHitInteractStats(it_attack_coll, fighter_gobj, (it_attack_coll->can_rehit_fighter) ? nGMHitTypeDamageRehit : nGMHitTypeDamage, 0);

    if (ip->type == nITTypeTouch)
    {
        switch (ip->kind)
        {
        case nITKindStar:
            it_attack_coll->attack_state = nGMAttackStateOff;
            ip->hit_normal_damage = 1;

            ftParamSetStarHitStatusInvincible(fp, ITSTAR_INVINCIBLE_TIME);
            ftParamTryPlayItemMusic(nSYAudioBGMStar);
            func_800269C0_275C0(nSYAudioFGMStarGet);
#if defined (REGION_US)
            if ((gSCManagerBattleState->game_type == nSCBattleGameType1PGame) && (fp->player == gSCManagerSceneData.player) && (gSC1PGameBonusStarCount < U8_MAX))
            {
                gSC1PGameBonusStarCount++;
            }
#endif
            break;

        case nITKindGLucky:
            ftParamSetHealDamage(fp, it_attack_coll->damage);
            break;
        }
    }
    else
    {
        damage = ftParamGetCapturedDamage(fp, damage_temp);

        if (it_attack_coll->can_rehit_fighter)
        {
            if (ip->hit_refresh_damage < damage)
            {
                ip->hit_refresh_damage = damage;
            }
        }
        else if (ip->hit_normal_damage < damage)
        {
            ip->hit_normal_damage = damage;
        }
        if (ABSF(ip->physics.vel_air.x) < 5.0F)
        {
            ip->hit_lr = hit_lr = (DObjGetStruct(fighter_gobj)->translate.vec.f.x < DObjGetStruct(item_gobj)->translate.vec.f.x) ? -1 : +1;
        }
        else
        {
            hit_lr = (ip->physics.vel_air.x < 0) ? -1 : +1;

            ip->hit_lr = hit_lr;
        }
        if
        (
            (fp->special_hitstatus == nGMHitStatusNormal)   &&
            (fp->star_hitstatus == nGMHitStatusNormal)      &&
            (fp->hitstatus == nGMHitStatusNormal)           &&
            (damage_coll->hitstatus == nGMHitStatusNormal)  &&
            (ftMainCheckGetUpdateDamage(fp, &damage) != FALSE)
        )
        {
            if (sFTMainHitLogID < ARRAY_COUNT(sFTMainHitLogs))
            {
                FTHitLog *hitlog = &sFTMainHitLogs[sFTMainHitLogID];

                hitlog->attacker_object_class = nFTHitLogObjectItem;
                hitlog->attack_coll = it_attack_coll;
                hitlog->attack_id = attack_id;
                hitlog->attacker_gobj = item_gobj;
                hitlog->damage_coll = damage_coll;
                hitlog->attacker_player = ip->player;
                hitlog->attacker_player_num = ip->player_num;

                sFTMainHitLogID++;
            }
            ftParamUpdatePlayerBattleStats(ip->player, fp->player, damage);
            ftParamUpdateStaleQueue(ip->player, fp->player, it_attack_coll->motion_attack_id, it_attack_coll->motion_count);
        }
        func_800269C0_275C0(it_attack_coll->fgm_id);
    }
}

void ftMainUpdateDamageStatGround(GObj *special_gobj, GObj *fighter_gobj, FTStruct *fp, GRAttackColl *gr_attack_coll, s32 kind)
{
    s32 damage = ftParamGetCapturedDamage(fp, gr_attack_coll->damage);
    sb32 is_take_damage = ftMainCheckGetUpdateDamage(fp, &damage);

    if ((is_take_damage != FALSE) && (sFTMainHitLogID < ARRAY_COUNT(sFTMainHitLogs)))
    {
        FTHitLog *hitlog = &sFTMainHitLogs[sFTMainHitLogID];

        hitlog->attacker_object_class = nFTHitLogObjectGround;
        hitlog->attack_coll = gr_attack_coll;
        hitlog->attacker_gobj = special_gobj;

        sFTMainHitLogID++;
    }
    switch (kind)
    {
    case nGMHitEnvironmentAcid:
        fp->acid_wait = 30;

        func_800269C0_275C0(nSYAudioFGMFloorDamageFire);
        break;

    case nGMHitEnvironmentPowerBlock:
        if (is_take_damage != FALSE)
        {
            ftParamUpdatePlayerBattleStats(itGetStruct(special_gobj)->damage_port, fp->player, damage);
        }
        break;

    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
        fp->damagefloor_wait = 16;

        if (kind == 7)
        {
            func_800269C0_275C0(nSYAudioFGMShockML);
        }
        else func_800269C0_275C0(nSYAudioFGMFloorDamageFire);
        break;

    default:
        break;
    }
}

/* ft/ftmain.c:2644-2661 ftMainGetBumperDamageAngle 0x800E3D6C, verbatim:
 * a weapon or item whose table says angle 362 -- the Bumper --
 * launches away from wherever it was relative to the fighter. */
void ftMainGetBumperDamageAngle(GObj *fighter_gobj, GObj *attacker_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (fp->damage_angle == 362)
    {
        f32 dist_x = DObjGetStruct(fighter_gobj)->translate.vec.f.x - DObjGetStruct(attacker_gobj)->translate.vec.f.x;
        f32 dist_y;

        fp->damage_lr = (dist_x < 0) ? +1 : -1;

        if (dist_x < 0.0F)
        {
            dist_x = -dist_x;
        }
        dist_y = (DObjGetStruct(fighter_gobj)->translate.vec.f.y + fp->coll_data.map_coll.center) - DObjGetStruct(attacker_gobj)->translate.vec.f.y;

        fp->damage_angle = (dist_x == 0) ? 0 : F_CLC_RTOD32(syUtilsArcTan(dist_y / dist_x));
    }
}

void ftMainProcessHitCollisionStatsMain(GObj *fighter_gobj)
{
    FTStruct *this_fp = ftGetStruct(fighter_gobj);
    FTStruct *attacker_fp;
    WPStruct *wp;
    ITStruct *ip;
    FTAttributes *attr = this_fp->attr;
    FTHitLog *hitlog;
    s32 i, j;
    f32 knockback_temp;
    f32 knockback;
    FTAttackColl *ft_attack_coll;
    WPAttackColl *wp_attack_coll;
    ITAttackColl *it_attack_coll;
    GRAttackColl *gr_attack_coll;
    Vec3f pos;
    s32 damage;
    u8 gr_handicap;
    GObj *attacker_gobj;
    s32 damage_lr;

    knockback = -1.0F;

    for (i = 0; i < sFTMainHitLogID; i++)
    {
        hitlog = &sFTMainHitLogs[i];

        switch (hitlog->attacker_object_class)
        {
        case nFTHitLogObjectFighter:
            ft_attack_coll = hitlog->attack_coll;
            attacker_fp = ftGetStruct(hitlog->attacker_gobj);

            knockback_temp = ftParamGetCommonKnockback
            (
                this_fp->percent_damage, 
                this_fp->damage_queue, ft_attack_coll->damage, 
                ft_attack_coll->knockback_weight, 
                ft_attack_coll->knockback_scale, 
                ft_attack_coll->knockback_base, 
                attr->weight, 
                attacker_fp->handicap, 
                this_fp->handicap
            );

            gmCollisionGetFighterAttackDamagePosition(&pos, ft_attack_coll, hitlog->damage_coll);

            switch (ft_attack_coll->element)
            {
            case nGMHitElementFire:
                efManagerDamageFireMakeEffect(&pos, ft_attack_coll->damage);
                break;

            case nGMHitElementElectric:
                efManagerDamageElectricMakeEffect(&pos, ft_attack_coll->damage);
                break;

            case nGMHitElementCoin:
                efManagerDamageCoinMakeEffect(&pos);
                break;

            case nGMHitElementSlash:
                efManagerDamageSlashMakeEffect(&pos, ft_attack_coll->damage, gmCollisionGetDamageSlashRotation(attacker_fp, ft_attack_coll));
                break;

            default:
                if (knockback_temp < 180.0F)
                {
                    efManagerDamageNormalLightMakeEffect(&pos, hitlog->attacker_player, ft_attack_coll->damage, 0);
                }
                else efManagerDamageNormalHeavyMakeEffect(&pos, hitlog->attacker_player, ft_attack_coll->damage);

                if (ft_attack_coll->fgm_level > 0) // Changed this to > 0 for now, makes a bit more sense to me since it only does this on moves with hit SFX levels greater than weak (0)
                {
                    efManagerDamageSpawnOrbsRandomMakeEffect(&pos);

                    switch (this_fp->attr->is_metallic)
                    {
                    case FALSE:
                        efManagerDamageSpawnSparksRandomMakeEffect(&pos, this_fp->lr);
                        break;

                    case TRUE:
                        efManagerDamageSpawnMDustRandomMakeEffect(&pos, this_fp->lr);
                        break;
                    }
                }
                break;
            }
            break;

        case nFTHitLogObjectWeapon:
            wp_attack_coll = hitlog->attack_coll;
            wp = wpGetStruct(hitlog->attacker_gobj);
            damage = wpMainGetStaledDamage(wp);

            knockback_temp = ftParamGetCommonKnockback(this_fp->percent_damage, this_fp->damage_queue, damage, wp_attack_coll->knockback_weight, wp_attack_coll->knockback_scale, wp_attack_coll->knockback_base, attr->weight, wp->handicap, this_fp->handicap);

            if (wp->is_hitlag_victim)
            {
                gmCollisionGetWeaponAttackFighterDamagePosition(&pos, wp_attack_coll, hitlog->attack_id, hitlog->damage_coll);

                switch (wp_attack_coll->element)
                {
                case nGMHitElementFire:
                    efManagerDamageFireMakeEffect(&pos, damage);
                    break;

                case nGMHitElementElectric:
                    efManagerDamageElectricMakeEffect(&pos, damage);
                    break;

                case nGMHitElementCoin:
                    efManagerDamageCoinMakeEffect(&pos);
                    break;

                default:
                    if (knockback_temp < 180.0F)
                    {
                        efManagerDamageNormalLightMakeEffect(&pos, hitlog->attacker_player, damage, NULL);
                    }
                    else efManagerDamageNormalHeavyMakeEffect(&pos, hitlog->attacker_player, damage);
                    break;
                }
            }
            break;

        case nFTHitLogObjectItem:
            it_attack_coll = hitlog->attack_coll;
            ip = itGetStruct(hitlog->attacker_gobj);

            damage = itMainGetDamageOutput(ip);

            knockback_temp = ftParamGetCommonKnockback(this_fp->percent_damage, this_fp->damage_queue, damage, it_attack_coll->knockback_weight, it_attack_coll->knockback_scale, it_attack_coll->knockback_base, attr->weight, ip->handicap, this_fp->handicap);

            if (ip->is_hitlag_victim)
            {
                gmCollisionGetItemAttackFighterDamagePosition(&pos, it_attack_coll, hitlog->attack_id, hitlog->damage_coll);

                switch (it_attack_coll->element)
                {
                case nGMHitElementFire:
                    efManagerDamageFireMakeEffect(&pos, damage);
                    break;

                case nGMHitElementElectric:
                    efManagerDamageElectricMakeEffect(&pos, damage);
                    break;

                case nGMHitElementCoin:
                    efManagerDamageCoinMakeEffect(&pos);
                    break;

                default:
                    if (knockback_temp < 180.0F)
                    {
                        efManagerDamageNormalLightMakeEffect(&pos, hitlog->attacker_player, damage, NULL);
                    }
                    else efManagerDamageNormalHeavyMakeEffect(&pos, hitlog->attacker_player, damage);
                    break;
                }
            }
            break;

        case nFTHitLogObjectGround:
            gr_attack_coll = hitlog->attack_coll;

            if (gr_attack_coll->kind == nGMHitEnvironmentPowerBlock) // POW Block?
            {
                gr_handicap = itGetStruct(hitlog->attacker_gobj)->damage_handicap;
            }
            else gr_handicap = 9;

            knockback_temp = ftParamGetCommonKnockback(this_fp->percent_damage, this_fp->damage_queue, gr_attack_coll->damage, gr_attack_coll->knockback_weight, gr_attack_coll->knockback_scale, gr_attack_coll->knockback_base, attr->weight, gr_handicap, this_fp->handicap);
            break;

        default:
            break;
        }
        if (knockback < knockback_temp)
        {
            knockback = knockback_temp;

            j = i;
        }
    }
    hitlog = &sFTMainHitLogs[j];
    attacker_gobj = hitlog->attacker_gobj;

    switch (hitlog->attacker_object_class)
    {
    case nFTHitLogObjectFighter:
        ft_attack_coll = hitlog->attack_coll;
        attacker_fp = ftGetStruct(attacker_gobj);
        this_fp->damage_angle = ft_attack_coll->angle;
        this_fp->damage_element = ft_attack_coll->element;

        this_fp->damage_lr = (DObjGetStruct(fighter_gobj)->translate.vec.f.x < DObjGetStruct(attacker_gobj)->translate.vec.f.x) ? +1 : -1;

        this_fp->damage_player_num = hitlog->attacker_player_num;

        ftParamUpdate1PGameDamageStats(this_fp, hitlog->attacker_player, hitlog->attacker_object_class, attacker_fp->fkind, attacker_fp->stat_flags.halfword & ~0x400, attacker_fp->stat_count);

        this_fp->damage_joint_id = hitlog->damage_coll->joint_id;
        this_fp->damage_index = hitlog->damage_coll->placement;

        if (this_fp->damage_element == nGMHitElementElectric)
        {
            attacker_fp->hitlag_mul = 1.5F;
        }
        break;

    case nFTHitLogObjectWeapon:
        wp_attack_coll = hitlog->attack_coll;
        wp = wpGetStruct(attacker_gobj);
        this_fp->damage_angle = wp_attack_coll->angle;
        this_fp->damage_element = wp_attack_coll->element;

        if (ABSF(wp->physics.vel_air.x) < 5.0F)
        {
            this_fp->damage_lr = damage_lr = (DObjGetStruct(fighter_gobj)->translate.vec.f.x < DObjGetStruct(attacker_gobj)->translate.vec.f.x) ? +1 : -1;
        }
        else
        {
            damage_lr = (wp->physics.vel_air.x < 0) ? +1 : -1;

            this_fp->damage_lr = damage_lr;
        }
        if (this_fp->player == hitlog->attacker_player)
        {
            this_fp->damage_player_num = 0;

            ftParamUpdate1PGameDamageStats(this_fp, GMCOMMON_PLAYERS_MAX, hitlog->attacker_object_class, wp->kind, 0, 0);
        }
        else
        {
            this_fp->damage_player_num = hitlog->attacker_player_num;

            ftParamUpdate1PGameDamageStats(this_fp, hitlog->attacker_player, hitlog->attacker_object_class, wp->kind, wp_attack_coll->stat_flags.halfword, wp_attack_coll->stat_count);
        }
        this_fp->damage_joint_id = hitlog->damage_coll->joint_id;
        this_fp->damage_index = hitlog->damage_coll->placement;

        ftMainGetBumperDamageAngle(fighter_gobj, attacker_gobj);
        break;

    case nFTHitLogObjectItem:
        it_attack_coll = hitlog->attack_coll;
        ip = itGetStruct(attacker_gobj);

        this_fp->damage_angle = it_attack_coll->angle;
        this_fp->damage_element = it_attack_coll->element;

        if (ABSF(ip->physics.vel_air.x) < 5.0F)
        {
            this_fp->damage_lr = damage_lr = (DObjGetStruct(fighter_gobj)->translate.vec.f.x < DObjGetStruct(attacker_gobj)->translate.vec.f.x) ? +1 : -1;
        }
        else
        {
            damage_lr = (ip->physics.vel_air.x < 0) ? +1 : -1;

            this_fp->damage_lr = damage_lr;
        }

        if (this_fp->player == hitlog->attacker_player)
        {
            this_fp->damage_player_num = 0;

            ftParamUpdate1PGameDamageStats(this_fp, GMCOMMON_PLAYERS_MAX, hitlog->attacker_object_class, ip->kind, 0, 0);
        }
        else
        {
            this_fp->damage_player_num = hitlog->attacker_player_num;
            ftParamUpdate1PGameDamageStats(this_fp, hitlog->attacker_player, hitlog->attacker_object_class, ip->kind, it_attack_coll->stat_flags.halfword, it_attack_coll->stat_count);
        }
        this_fp->damage_joint_id = hitlog->damage_coll->joint_id;
        this_fp->damage_index = hitlog->damage_coll->placement;

        ftMainGetBumperDamageAngle(fighter_gobj, attacker_gobj);
        break;

    case nFTHitLogObjectGround:
        gr_attack_coll = hitlog->attack_coll;

        this_fp->damage_angle = gr_attack_coll->angle;
        this_fp->damage_element = gr_attack_coll->element;

        this_fp->damage_lr = this_fp->lr;

        switch (gr_attack_coll->kind)
        {
        case nGMHitEnvironmentAcid:
            this_fp->damage_player_num = 0;

            if (this_fp->damage_player == -1)
            {
                this_fp->damage_player = GMCOMMON_PLAYERS_MAX;
            }
            ftParamUpdate1PGameDamageStats(this_fp, this_fp->damage_player, hitlog->attacker_object_class, gr_attack_coll->kind, 0, 0);
            break;

        case nGMHitEnvironmentPowerBlock:
            ip = itGetStruct(attacker_gobj);

            this_fp->damage_player_num = ip->damage_player_num;

            ftParamUpdate1PGameDamageStats(this_fp, ip->damage_port, hitlog->attacker_object_class, gr_attack_coll->kind, 0, 0);

            break;

        default:
            this_fp->damage_player_num = 0;

            ftParamUpdate1PGameDamageStats(this_fp, GMCOMMON_PLAYERS_MAX, hitlog->attacker_object_class, gr_attack_coll->kind, 0, 0);
            break;
        }
        this_fp->damage_joint_id = 0;
        this_fp->damage_index = 0;

        break;

    default:
        break;
    }
    this_fp->damage_knockback = knockback;

    if (this_fp->damage_element == nGMHitElementElectric)
    {
        this_fp->hitlag_mul = 1.5F;
    }
}

void ftMainSearchHitFighter(GObj *this_gobj)
{
    GObj *other_gobj;
    FTStruct *this_fp;
    FTStruct *other_fp;
    s32 i, j, k, l, m, n;
    GMHitFlags those_flags;
    GMHitFlags these_flags;
    FTAttackColl *this_attack_coll;
    f32 angle;
    FTAttackColl *other_attack_coll;
    FTDamageColl *damage_coll;
    sb32 is_check_self;

    this_fp = ftGetStruct(this_gobj);
    other_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
    is_check_self = FALSE;

    while (other_gobj != NULL)
    {
        if (other_gobj == this_gobj)
        {
            is_check_self = TRUE;

            goto next_gobj;
        }
        else if (other_gobj == this_fp->capture_gobj) goto next_gobj;

        other_fp = ftGetStruct(other_gobj);

        if (((gSCManagerBattleState->is_team_battle == TRUE) && (gSCManagerBattleState->is_team_attack == FALSE)) && (((other_fp->throw_gobj != NULL) ? other_fp->throw_team : other_fp->team) == this_fp->team))
        {
            goto next_gobj;
        }
        if (!(other_fp->is_catchstatus))
        {
            if ((other_fp->throw_gobj != NULL) && (this_gobj == other_fp->throw_gobj))
            {
                goto next_gobj;
            }
            else
            {
                k = 0;

                for (i = 0; i < ARRAY_COUNT(other_fp->attack_colls); i++)
                {
                    other_attack_coll = &other_fp->attack_colls[i];

                    if (other_attack_coll->attack_state != nGMAttackStateOff)
                    {
                        if ((this_fp->ga == nMPKineticsAir) && (other_attack_coll->is_hit_air) || (this_fp->ga == nMPKineticsGround) && (other_attack_coll->is_hit_ground))
                        {
                            these_flags.is_interact_hurt = these_flags.is_interact_shield = FALSE;

                            these_flags.group_id = 7;

                            for (m = 0; m < ARRAY_COUNT(other_attack_coll->attack_records); m++)
                            {
                                if (this_gobj == other_attack_coll->attack_records[m].victim_gobj)
                                {
                                    these_flags = other_attack_coll->attack_records[m].victim_flags;

                                    break;
                                }
                            }
                            if ((!(these_flags.is_interact_hurt)) && (!(these_flags.is_interact_shield)) && (these_flags.group_id == 7))
                            {
                                gFTMainIsDamageDetect[i] = TRUE;

                                k++;

                                continue;
                            }
                        }
                    }
                    gFTMainIsDamageDetect[i] = FALSE;
                }
                if (k == 0) goto next_gobj;

                else
                {
                    k = 0;

                    if ((is_check_self != FALSE) && (this_gobj != other_fp->capture_gobj) && (other_fp->ga == nMPKineticsGround) && (this_fp->ga == nMPKineticsGround) && !(this_fp->is_catchstatus))
                    {
                        if ((this_fp->throw_gobj == NULL) || (other_gobj != this_fp->throw_gobj) && (((gSCManagerBattleState->is_team_battle != TRUE) || (gSCManagerBattleState->is_team_attack != FALSE)) || (((other_fp->throw_gobj != NULL) ? other_fp->throw_team : other_fp->team) != this_fp->throw_team)))
                        {
                            l = 0;

                            for (i = 0; i < ARRAY_COUNT(this_fp->attack_colls); i++)
                            {
                                this_attack_coll = &this_fp->attack_colls[i];

                                if (this_attack_coll->attack_state != nGMAttackStateOff)
                                {
                                    if ((other_fp->ga == nMPKineticsAir) && (this_attack_coll->is_hit_air) || (other_fp->ga == nMPKineticsGround) && (this_attack_coll->is_hit_ground))
                                    {
                                        those_flags.is_interact_hurt = those_flags.is_interact_shield = FALSE;

                                        those_flags.group_id = 7;

                                        for (n = 0; n < ARRAY_COUNT(this_attack_coll->attack_records); n++)
                                        {
                                            if (other_gobj == this_attack_coll->attack_records[n].victim_gobj)
                                            {
                                                those_flags = this_attack_coll->attack_records[n].victim_flags;

                                                break;
                                            }
                                        }
                                        if ((!(those_flags.is_interact_hurt)) && (!(those_flags.is_interact_shield)) && (those_flags.group_id == 7))
                                        {
                                            gFTMainIsAttackDetect[i] = TRUE;

                                            l++;

                                            continue;
                                        }
                                    }
                                }
                                gFTMainIsAttackDetect[i] = FALSE;
                            }
                            if (l != 0)
                            {
                                for (i = 0; i < ARRAY_COUNT(other_fp->attack_colls); i++)
                                {
                                    other_attack_coll = &other_fp->attack_colls[i];

                                    if (gFTMainIsDamageDetect[i] == FALSE) 
                                    {
                                        continue;
                                    }
                                    else for (j = 0; j < ARRAY_COUNT(this_fp->attack_colls); j++)
                                    {
                                        this_attack_coll = &this_fp->attack_colls[j];

                                        if (gFTMainIsAttackDetect[j] == FALSE) 
                                        {
                                            continue;
                                        }
                                        else if (gmCollisionCheckFighterAttacksCollide(other_attack_coll, this_attack_coll) != FALSE)
                                        {
                                            ftMainUpdateAttackStatFighter(other_fp, other_attack_coll, this_fp, this_attack_coll, other_gobj, this_gobj);

                                            if (gFTMainIsDamageDetect[i] == FALSE)
                                            {
                                                break;
                                            }
                                            else continue;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    for (i = 0; i < ARRAY_COUNT(other_fp->attack_colls); i++)
                    {
                        other_attack_coll = &other_fp->attack_colls[i];

                        if (gFTMainIsDamageDetect[i] == FALSE) 
                        {
                            continue;
                        }
                        gFTMainIsDamageDetect[i] = gmCollisionCheckFighterInFighterRange(other_attack_coll, this_gobj);

                        if (gFTMainIsDamageDetect[i] != FALSE) 
                        {
                            k++;
                        }
                    }
                    if (k != 0)
                    {
                        if (this_fp->is_shield)
                        {
                            for (i = 0; i < ARRAY_COUNT(other_fp->attack_colls); i++)
                            {
                                other_attack_coll = &other_fp->attack_colls[i];

                                if (gFTMainIsDamageDetect[i] == FALSE) 
                                {
                                    continue;
                                }
                                else if (gmCollisionCheckFighterAttackShieldCollide(other_attack_coll, this_gobj, this_fp->joints[nFTPartsJointYRotN], &angle) != FALSE)
                                {
                                    ftMainUpdateShieldStatFighter(other_fp, other_attack_coll, this_fp, other_gobj, this_gobj);
                                }
                            }
                        }
                        if ((this_fp->special_hitstatus != nGMHitStatusIntangible) && (this_fp->star_hitstatus != nGMHitStatusIntangible) && (this_fp->hitstatus != nGMHitStatusIntangible))
                        {
                            for (i = 0; i < ARRAY_COUNT(other_fp->attack_colls); i++)
                            {
                                other_attack_coll = &other_fp->attack_colls[i];

                                if (gFTMainIsDamageDetect[i] == FALSE) 
                                {
                                    continue;
                                }
                                else for (j = 0; j < ARRAY_COUNT(this_fp->damage_colls); j++)
                                {
                                    damage_coll = &this_fp->damage_colls[j];

                                    if (damage_coll->hitstatus == nGMHitStatusNone) 
                                    {
                                        break;
                                    }
                                    if (damage_coll->hitstatus != nGMHitStatusIntangible)
                                    {
                                        if (gmCollisionCheckFighterAttackDamageCollide(other_attack_coll, damage_coll) != FALSE)
                                        {
                                            ftMainUpdateDamageStatFighter(other_fp, other_attack_coll, this_fp, damage_coll, other_gobj, this_gobj);

                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    next_gobj:
        other_gobj = other_gobj->link_next;
    }
}


/* ft/ftmain.c:3227-3423 ftMainSearchHitWeapon 0x800E4E9C and :3424-3604
 * ftMainSearchHitItem 0x800E55DC, verbatim (the decomp's one unused
 * local dropped). The fighter's side of every projectile and item hit:
 * each frame ftMainProcSearchHitAll walks the weapon and item links for a
 * live hitbox that is not this fighter's own, and in the game's order --
 * the clash of hitboxes (can_setoff), then a reflector, an absorber, the
 * shield, and last the hurtboxes -- hands what it finds to the
 * ftMainUpdate*Stat{Weapon,Item} above. Without it every fighter
 * projectile and item would fly through fighters. */
void ftMainSearchHitWeapon(GObj *fighter_gobj)
{
    GObj *weapon_gobj;
    s32 i, j, k, l, m, n;
    GMHitFlags fighter_flags;
    GMHitFlags weapon_flags;
    FTStruct *fp;
    WPStruct *wp;
    WPAttackColl *wp_attack_coll;
    f32 angle;
    Vec3f vec;
    FTDamageColl *damage_coll;
    FTAttackColl *ft_attack_coll;

    fp = ftGetStruct(fighter_gobj);
    weapon_gobj = gGCCommonLinks[nGCCommonLinkIDWeapon];

    while (weapon_gobj != NULL)
    {
        wp = wpGetStruct(weapon_gobj);
        wp_attack_coll = &wp->attack_coll;

        if ((fighter_gobj != wp->owner_gobj) && ((gSCManagerBattleState->is_team_battle != TRUE) || (gSCManagerBattleState->is_team_attack != FALSE) || (fp->team != wp->team)) && (wp_attack_coll->attack_state != nGMAttackStateOff))
        {
            if (wp_attack_coll->interact_mask & GMHITCOLLISION_FLAG_FIGHTER)
            {
                weapon_flags.is_interact_hurt = weapon_flags.is_interact_shield = weapon_flags.is_interact_reflect = weapon_flags.is_interact_absorb = FALSE;

                weapon_flags.group_id = 7;

                for (m = 0; m < ARRAY_COUNT(wp_attack_coll->attack_records); m++)
                {
                    if (fighter_gobj == wp_attack_coll->attack_records[m].victim_gobj)
                    {
                        weapon_flags = wp_attack_coll->attack_records[m].victim_flags;

                        break;
                    }
                }
                if (!(weapon_flags.is_interact_hurt) && !(weapon_flags.is_interact_shield) && !(weapon_flags.is_interact_reflect) && !(weapon_flags.is_interact_absorb) && (weapon_flags.group_id == 7))
                {
                    if ((wp_attack_coll->can_setoff) && !(fp->is_catchstatus) && ((fp->throw_gobj == NULL) || (fp->throw_gobj != wp->owner_gobj) && ((gSCManagerBattleState->is_team_battle != TRUE) || (gSCManagerBattleState->is_team_attack != FALSE) || (fp->throw_team != wp->team))))
                    {

                        if (!(fp->is_reflect) || !(wp_attack_coll->can_reflect))
                        {
                            k = 0;

                            for (i = 0; i < ARRAY_COUNT(fp->attack_colls); i++)
                            {
                                ft_attack_coll = &fp->attack_colls[i];

                                if ((ft_attack_coll->attack_state != nGMAttackStateOff) && ((wp->ga == nMPKineticsAir) && (ft_attack_coll->is_hit_air) || (wp->ga == nMPKineticsGround) && (ft_attack_coll->is_hit_ground)))
                                {
                                    fighter_flags.group_id = 7;

                                    for (n = 0; n < ARRAY_COUNT(ft_attack_coll->attack_records); n++)
                                    {
                                        if (weapon_gobj == ft_attack_coll->attack_records[n].victim_gobj)
                                        {
                                            fighter_flags = ft_attack_coll->attack_records[n].victim_flags;

                                            break;
                                        }
                                    }
                                    if (fighter_flags.group_id == 7)
                                    {
                                        gFTMainIsAttackDetect[i] = TRUE;

                                        k++;

                                        continue;
                                    }
                                }
                                gFTMainIsAttackDetect[i] = FALSE;
                            }
                            if (k != 0)
                            {
                                for (i = 0; i < wp_attack_coll->attack_count; i++)
                                {
                                    for (j = 0; j < ARRAY_COUNT(fp->attack_colls); j++)
                                    {
                                        ft_attack_coll = &fp->attack_colls[j];

                                        if (gFTMainIsAttackDetect[j] == FALSE) 
                                        {
                                            continue;
                                        }
                                        else if (gmCollisionCheckWeaponAttackFighterAttackCollide(wp_attack_coll, i, ft_attack_coll) != FALSE)
                                        {
                                            ftMainUpdateAttackStatWeapon(wp, wp_attack_coll, i, fp, ft_attack_coll, weapon_gobj, fighter_gobj);

                                            if (wp->hit_attack_damage != 0) 
                                            {
                                                goto next_gobj;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                    }
                    for (i = 0, l = 0; i < wp_attack_coll->attack_count; i++)
                    {
                        gFTMainIsDamageDetect[i] = gmCollisionCheckWeaponInFighterRange(wp_attack_coll, i, fighter_gobj);

                        if (gFTMainIsDamageDetect[i] != FALSE)
                        {
                            l++;
                        }
                    }
                    if (l != 0)
                    {
                        if ((fp->is_reflect) && (wp_attack_coll->can_reflect))
                        {
                            for (i = 0; i < wp_attack_coll->attack_count; i++)
                            {
                                if (gFTMainIsDamageDetect[i] == FALSE) continue;

                                else if (gmCollisionCheckWeaponAttackSpecialCollide(wp_attack_coll, i, fp, fp->special_coll) != FALSE)
                                {
                                    ftMainUpdateReflectorStatWeapon(wp, wp_attack_coll, fp, fighter_gobj);

                                    goto next_gobj;
                                }
                            }
                        }
                        if ((fp->is_absorb) && (wp_attack_coll->can_absorb))
                        {
                            for (i = 0; i < wp_attack_coll->attack_count; i++)
                            {
                                if (gFTMainIsDamageDetect[i] == FALSE) 
                                {
                                    continue;
                                }
                                else if (gmCollisionCheckWeaponAttackSpecialCollide(wp_attack_coll, i, fp, fp->special_coll) != FALSE)
                                {
                                    ftMainUpdateAbsorbStatWeapon(wp, wp_attack_coll, fp, fighter_gobj);

                                    goto next_gobj;
                                }
                            }
                        }
                        if ((fp->is_shield) && (wp_attack_coll->can_shield))
                        {
                            for (i = 0; i < wp_attack_coll->attack_count; i++)
                            {
                                if (gFTMainIsDamageDetect[i] == FALSE) 
                                {
                                    continue;
                                }
                                else if (gmCollisionCheckWeaponAttackShieldCollide(wp_attack_coll, i, fighter_gobj, fp->joints[nFTPartsJointYRotN], &angle, &vec) != FALSE)
                                {
                                    ftMainUpdateShieldStatWeapon(wp, wp_attack_coll, i, fp, weapon_gobj, fighter_gobj, angle, &vec);

                                    goto next_gobj;
                                }
                            }
                        }
                        if ((fp->special_hitstatus != nGMHitStatusIntangible) && (fp->star_hitstatus != nGMHitStatusIntangible) && (fp->hitstatus != nGMHitStatusIntangible))
                        {
                            for (i = 0; i < wp_attack_coll->attack_count; i++)
                            {
                                if (gFTMainIsDamageDetect[i] == FALSE) 
                                {
                                    continue;
                                }
                                else for (j = 0; j < ARRAY_COUNT(fp->damage_colls); j++)
                                {
                                    damage_coll = &fp->damage_colls[j];

                                    if (damage_coll->hitstatus == nGMHitStatusNone) break;

                                    if (damage_coll->hitstatus != nGMHitStatusIntangible)
                                    {
                                        if (gmCollisionCheckWeaponAttackFighterDamageCollide(wp_attack_coll, i, damage_coll) != FALSE)
                                        {
                                            ftMainUpdateDamageStatWeapon(wp, wp_attack_coll, i, fp, damage_coll, weapon_gobj, fighter_gobj);

                                            goto next_gobj;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    next_gobj:
        weapon_gobj = weapon_gobj->link_next;
    }
}

// 0x800E55DC
void ftMainSearchHitItem(GObj *fighter_gobj)
{
    GObj *item_gobj;
    s32 i, j, k, l, m, n;
    GMHitFlags fighter_flags;
    GMHitFlags item_flags;
    FTStruct *fp;
    ITStruct *ip;
    ITAttackColl *it_attack_coll;
    f32 angle;
    Vec3f vec;
    FTDamageColl *damage_coll;
    FTAttackColl *ft_attack_coll;

    fp = ftGetStruct(fighter_gobj);
    item_gobj = gGCCommonLinks[nGCCommonLinkIDItem];

    while (item_gobj != NULL)
    {
        ip = itGetStruct(item_gobj);
        it_attack_coll = &ip->attack_coll;

        if ((fighter_gobj != ip->owner_gobj) && ((gSCManagerBattleState->is_team_battle != TRUE) || (gSCManagerBattleState->is_team_attack != FALSE) || (fp->team != ip->team)) && (it_attack_coll->attack_state != nGMAttackStateOff))
        {
            if (it_attack_coll->interact_mask & GMHITCOLLISION_FLAG_FIGHTER)
            {
                item_flags.is_interact_hurt = item_flags.is_interact_shield = item_flags.is_interact_reflect = FALSE;

                item_flags.group_id = 7;

                for (m = 0; m < ARRAY_COUNT(it_attack_coll->attack_records); m++)
                {
                    if (fighter_gobj == it_attack_coll->attack_records[m].victim_gobj)
                    {
                        item_flags = it_attack_coll->attack_records[m].victim_flags;

                        break;
                    }
                }
                if (!(item_flags.is_interact_hurt) && !(item_flags.is_interact_shield) && !(item_flags.is_interact_reflect) && (item_flags.group_id == 7))
                {
                    if ((it_attack_coll->can_setoff) && !(fp->is_catchstatus) && ((fp->throw_gobj == NULL) || (fp->throw_gobj != ip->owner_gobj) && ((gSCManagerBattleState->is_team_battle != TRUE) || (gSCManagerBattleState->is_team_attack != FALSE) || (fp->throw_team != ip->team))))
                    {
                        if (!(fp->is_reflect) || !(it_attack_coll->can_reflect))
                        {
                            k = 0;

                            for (i = 0; i < ARRAY_COUNT(fp->attack_colls); i++)
                            {
                                ft_attack_coll = &fp->attack_colls[i];

                                if ((ft_attack_coll->attack_state != nGMAttackStateOff) && ((ip->ga == nMPKineticsAir) && (ft_attack_coll->is_hit_air) || (ip->ga == nMPKineticsGround) && (ft_attack_coll->is_hit_ground)))
                                {
                                    fighter_flags.group_id = 7;

                                    for (n = 0; n < ARRAY_COUNT(ft_attack_coll->attack_records); n++)
                                    {
                                        if (item_gobj == ft_attack_coll->attack_records[n].victim_gobj)
                                        {
                                            fighter_flags = ft_attack_coll->attack_records[n].victim_flags;

                                            break;
                                        }
                                    }
                                    if (fighter_flags.group_id == 7)
                                    {
                                        gFTMainIsAttackDetect[i] = TRUE;

                                        k++;

                                        continue;
                                    }
                                }
                                gFTMainIsAttackDetect[i] = FALSE;
                            }
                            if (k != 0)
                            {
                                for (i = 0; i < it_attack_coll->attack_count; i++)
                                {
                                    for (j = 0; j < ARRAY_COUNT(fp->attack_colls); j++)
                                    {
                                        ft_attack_coll = &fp->attack_colls[j];

                                        if (gFTMainIsAttackDetect[j] == FALSE) 
                                        {
                                            continue;
                                        }
                                        else if (gmCollisionCheckItemAttackFighterAttackCollide(it_attack_coll, i, ft_attack_coll) != FALSE)
                                        {
                                            ftMainUpdateAttackStatItem(ip, it_attack_coll, i, fp, ft_attack_coll, item_gobj, fighter_gobj);

                                            if (ip->hit_attack_damage != 0) goto next_gobj;
                                        }
                                    }
                                }
                            }
                        }

                    }
                    for (i = 0, l = 0; i < it_attack_coll->attack_count; i++)
                    {
                        gFTMainIsDamageDetect[i] = gmCollisionCheckItemInFighterRange(it_attack_coll, i, fighter_gobj);

                        if (gFTMainIsDamageDetect[i] != FALSE)
                        {
                            l++;
                        }
                    }
                    if (l != 0)
                    {
                        if ((fp->is_reflect) && (it_attack_coll->can_reflect))
                        {
                            for (i = 0; i < it_attack_coll->attack_count; i++)
                            {
                                if (gFTMainIsDamageDetect[i] == FALSE) 
                                {
                                    continue;
                                }
                                else if (gmCollisionCheckItemAttackSpecialCollide(it_attack_coll, i, fp, fp->special_coll) != FALSE)
                                {
                                    ftMainUpdateReflectorStatItem(ip, it_attack_coll, fp, fighter_gobj);

                                    goto next_gobj;
                                }
                            }
                        }
                        if ((fp->is_shield) && (it_attack_coll->can_shield))
                        {
                            for (i = 0; i < it_attack_coll->attack_count; i++)
                            {
                                if (gFTMainIsDamageDetect[i] == FALSE)
                                {
                                    continue;
                                }
                                else if (gmCollisionCheckItemAttackShieldCollide(it_attack_coll, i, fighter_gobj, fp->joints[nFTPartsJointYRotN], &angle, &vec) != FALSE)
                                {
                                    ftMainUpdateShieldStatItem(ip, it_attack_coll, i, fp, item_gobj, fighter_gobj, angle, &vec);

                                    goto next_gobj;
                                }
                            }
                        }
                        if ((fp->special_hitstatus != nGMHitStatusIntangible) && (fp->star_hitstatus != nGMHitStatusIntangible) && (fp->hitstatus != nGMHitStatusIntangible))
                        {
                            for (i = 0; i < it_attack_coll->attack_count; i++)
                            {
                                if (gFTMainIsDamageDetect[i] == FALSE) 
                                {
                                    continue;
                                }
                                else for (j = 0; j < ARRAY_COUNT(fp->damage_colls); j++)
                                {
                                    damage_coll = &fp->damage_colls[j];

                                    if (damage_coll->hitstatus == nGMHitStatusNone) 
                                    {
                                        break;
                                    }
                                    if (damage_coll->hitstatus != nGMHitStatusIntangible)
                                    {
                                        if (gmCollisionCheckItemAttackFighterDamageCollide(it_attack_coll, i, damage_coll) != FALSE)
                                        {
                                            ftMainUpdateDamageStatItem(ip, it_attack_coll, i, fp, damage_coll, item_gobj, fighter_gobj);

                                            goto next_gobj;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    next_gobj:
        item_gobj = item_gobj->link_next;
    }
}


/* ft/ftmain.c:3679-3774 ftMainSearchFighterCatch 0x800E5F30.
 * Once a fighter is is_catchstatus (ftParamSetCatchParams),
 * this walks the fighter link each frame for a grabbable body inside one of
 * its own active collisions -- the catch box the Catch animation raises --
 * and records the nearest in search_gobj, which ftMainProcSearchCatch then
 * hands to proc_catch/proc_capture. Verbatim (the decompiler's `^ 0` no-op on the attack_coll
 * loop bound dropped). */
void ftMainSearchFighterCatch(GObj *this_gobj)
{
    GObj *other_gobj;
    FTStruct *this_fp;
    FTStruct *other_fp;
    s32 i, j, m;
    FTDamageColl *damage_coll;
    GMHitFlags catch_mask;
    FTAttackColl *attack_coll;

    this_fp = ftGetStruct(this_gobj);

    this_fp->search_gobj = NULL;
    this_fp->search_gobj_dist = F32_MAX;

    other_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    while (other_gobj != NULL)
    {
        if (other_gobj == this_gobj)
        {
            goto next_gobj;
        }
        other_fp = ftGetStruct(other_gobj);

        if (other_fp->is_ghost)
        {
            goto next_gobj;
        }
        if (other_fp->fkind == nFTKindBoss)
        {
            goto next_gobj;
        }
        if ((gSCManagerBattleState->is_team_battle == TRUE) && (gSCManagerBattleState->is_team_attack == FALSE) && (this_fp->team == other_fp->team))
        {
            goto next_gobj;
        }
        if (other_fp->capture_immune_mask & this_fp->catch_mask)
        {
            goto next_gobj;
        }
        if ((other_fp->special_hitstatus != nGMHitStatusNormal) || (other_fp->star_hitstatus != nGMHitStatusNormal) || (other_fp->hitstatus != nGMHitStatusNormal))
        {
            goto next_gobj;
        }
        for (i = 0; i < ARRAY_COUNT(this_fp->attack_colls); i++)
        {
            attack_coll = &this_fp->attack_colls[i];

            if (attack_coll->attack_state == nGMAttackStateOff)
            {
                continue;
            }
            if ((other_fp->ga == nMPKineticsAir) && !(attack_coll->is_hit_air) || (other_fp->ga == nMPKineticsGround) && !(attack_coll->is_hit_ground))
            {
                continue;
            }
            catch_mask.is_interact_hurt = catch_mask.is_interact_shield = FALSE;

            catch_mask.group_id = 7;

            for (m = 0; m < ARRAY_COUNT(attack_coll->attack_records); m++)
            {
                if (other_gobj == attack_coll->attack_records[m].victim_gobj)
                {
                    catch_mask = attack_coll->attack_records[m].victim_flags;

                    break;
                }
            }
            if ((catch_mask.is_interact_hurt) || (catch_mask.is_interact_shield) || (catch_mask.group_id != 7)) continue;

            for (j = 0; j < ARRAY_COUNT(other_fp->damage_colls); j++)
            {
                damage_coll = &other_fp->damage_colls[j];

                if (damage_coll->hitstatus == nGMHitStatusNone)
                {
                    break;
                }
                if ((damage_coll->hitstatus != nGMHitStatusIntangible) && (damage_coll->hitstatus != nGMHitStatusInvincible))
                {
                    if ((damage_coll->is_grabbable != FALSE) && (gmCollisionCheckFighterAttackDamageCollide(attack_coll, damage_coll) != FALSE))
                    {
                        ftMainUpdateCatchStatFighter(this_fp, attack_coll, other_fp, this_gobj, other_gobj);

                        goto next_gobj;
                    }
                }
            }
        }
    next_gobj:
        other_gobj = other_gobj->link_next;
    }
}


sb32 ftMainGetGroundHitObstacle(FTStruct *fp, GRAttackColl **p_gr_attack_coll)
{
    if ((fp->damagefloor_wait == 0) && (fp->ga == nMPKineticsGround) && (fp->coll_data.floor_line_id != -1) && (fp->coll_data.floor_line_id != -2))
    {
        switch (fp->coll_data.floor_flags & MAP_VERTEX_MAT_MASK)
        {
        case nMPMaterialFireWeakS1:
            *p_gr_attack_coll = &dFTMainGroundHitCollisionAttributes[0];
            return TRUE;

        case nMPMaterialFireStrongS1:
            *p_gr_attack_coll = &dFTMainGroundHitCollisionAttributes[1];
            return TRUE;

        case nMPMaterialFireWeakHi1:
            *p_gr_attack_coll = &dFTMainGroundHitCollisionAttributes[2];
            return TRUE;

        case nMPMaterialSpikes:
            *p_gr_attack_coll = &dFTMainGroundHitCollisionAttributes[3];
            return TRUE;

        case nMPMaterialFireWeakHi2:
            *p_gr_attack_coll = &dFTMainGroundHitCollisionAttributes[4];
            return TRUE;

        case nMPMaterialFireWeakHi3:
            *p_gr_attack_coll = &dFTMainGroundHitCollisionAttributes[5];
            return TRUE;

        default:
            return FALSE;
        }
    }
    else return FALSE;
}

void ftMainSearchGroundHit(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    GRHazard *ge = &sFTMainGroundHazards[0];
    s32 i;
    GRAttackColl *gr_attack_coll;
    s32 kind;

    if (fp->hitlag_tics == 0)
    {
        if (fp->acid_wait != 0)
        {
            fp->acid_wait--;
        }
        if (fp->damagefloor_wait != 0)
        {
            fp->damagefloor_wait--;
        }
    }
    if (ftParamGetBestHitStatusAll(fighter_gobj) == nGMHitStatusNormal)
    {
        for (i = 0; i < sFTMainGroundHazardsNum; i++, ge++)
        {
            if ((ge->gobj != NULL) && (ge->proc_update(ge->gobj, fighter_gobj, &gr_attack_coll, &kind) != FALSE))
            {
                ftMainUpdateDamageStatGround(ge->gobj, fighter_gobj, fp, gr_attack_coll, kind);
            }
        }
        if (ftMainGetGroundHitObstacle(fp, &gr_attack_coll) != FALSE)
        {
            ftMainUpdateDamageStatGround(NULL, fighter_gobj, fp, gr_attack_coll, gr_attack_coll->kind);
        }
    }
}

void ftMainProcSearchCatch(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    ftMainSearchHitHazard(fighter_gobj);

    if (fp->is_catchstatus)
    {
        ftMainSearchFighterCatch(fighter_gobj);

        if (fp->search_gobj != NULL)
        {
            fp->proc_catch(fighter_gobj);
            fp->proc_capture(fp->search_gobj, fighter_gobj);
        }
    }
}

void ftMainProcSearchHitAll(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (!(fp->is_ghost))
    {
        sFTMainHitLogID = 0;

        ftMainSearchHitFighter(fighter_gobj);
        ftMainSearchHitItem(fighter_gobj);
        ftMainSearchHitWeapon(fighter_gobj);
        ftMainSearchGroundHit(fighter_gobj);

        if (sFTMainHitLogID != 0)
        {
            ftMainProcessHitCollisionStatsMain(fighter_gobj);
        }
    }
}

void ftMainProcParams(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 damage;
    s32 status_id;
    f32 knockback_resist;
    sb32 is_shieldbreak;
    u32 hitlag_tics;
    sb32 is_knockback_paused;

    damage = 0;
    is_shieldbreak = FALSE;
    status_id = fp->status_id;
    hitlag_tics = fp->hitlag_tics;
    is_knockback_paused = FALSE;

    if (fp->unk_ft_0x7AC != 0)
    {
        fp->unk_ft_0x3C += fp->unk_ft_0x7AC;
    }
    if (!(fp->is_shield) && (fp->shield_health < 55))
    {
        fp->shield_heal_wait--;

        if (fp->shield_heal_wait == 0.0F)
        {
            fp->shield_health++;

            fp->shield_heal_wait = 10.0F;
        }
    }
    fp->shield_health -= fp->shield_damage_total;

    if (fp->shield_health <= 0)
    {
        fp->shield_health = (fp->fkind == nFTKindYoshi) ? 30 : 30;

        is_shieldbreak = TRUE;
    }
    if (fp->damage_knockback != 0.0F)
    {
        if ((fp->status_id == nFTCommonStatusSquat) || (fp->status_id == nFTCommonStatusSquatWait))
        {
            fp->damage_knockback *= (2.0F / 3.0F);
        }
        if (fp->status_id == nFTCommonStatusTwister)
        {
            fp->damage_kind = nFTDamageKindColAnim;
        }
        if (fp->knockback_resist_status < fp->knockback_resist_passive)
        {
            knockback_resist = fp->knockback_resist_passive;
        }
        else knockback_resist = fp->knockback_resist_status;

        fp->damage_knockback -= knockback_resist;

        if (fp->damage_knockback <= 0)
        {
            fp->damage_knockback = 0;
        }
        ftParamUpdateDamage(fp, fp->damage_queue);

        if (fp->proc_trap != NULL)
        {
            fp->proc_trap(fighter_gobj);
        }
        if (fp->fkind != nFTKindBoss)
        {
            switch (fp->damage_kind)
            {
            case nFTDamageKindNone:
                break;

            case nFTDamageKindStatus:
                ftParamStopVoiceRunProcDamage(fighter_gobj);
                ftCommonDamageGotoDamageStatus(fighter_gobj);
                break;

            case nFTDamageKindColAnim:
                ftCommonDamageSetDamageColAnim(fighter_gobj);
                break;

            case nFTDamageKindCatch:
                ftParamStopVoiceRunProcDamage(fighter_gobj);
                ftCommonDamageUpdateCatchResist(fighter_gobj);
                break;

            default:
                ftCommonDamageUpdateMain(fighter_gobj);
                break;
            }
        }
        else
        {
            ftCommonDamageSetDamageColAnim(fighter_gobj);
            ftBossCommonUpdateDamageStats(fighter_gobj);
        }
        damage = fp->damage_lag;
        is_knockback_paused = TRUE;

        ftParamSetDamageShuffle(fp, (fp->damage_element == nGMHitElementElectric) ? TRUE : FALSE, damage, status_id, fp->hitlag_mul);

        if ((s32)((fp->damage_queue * 0.75F) + 4.0F) > 0)
        {
            ftParamMakeRumble(fp, 0, (s32)((fp->damage_queue * 0.75F) + 4.0F));
        }
    }
    else if (fp->shield_damage != 0)
    {
        if (is_shieldbreak != FALSE)
        {
            ftCommonShieldBreakFlyCommonSetStatus(fighter_gobj);
        }
        else ftCommonGuardSetOffSetStatus(fighter_gobj);

        damage = fp->shield_damage;
    }
    else if (fp->attack_shield_push != 0)
    {
        if (fp->proc_shield != NULL)
        {
            fp->proc_shield(fighter_gobj);
        }
        if ((fp->attack_rebound != 0) && (fp->catch_gobj == NULL) && (fp->capture_gobj == NULL))
        {
            ftParamStopVoiceRunProcDamage(fighter_gobj);
            ftCommonReboundWaitSetStatus(fighter_gobj);
        }
        damage = fp->attack_shield_push;
    }
    else if (fp->attack_damage != 0)
    {
        if (fp->proc_hit != NULL)
        {
            fp->proc_hit(fighter_gobj);
        }
        damage = fp->attack_damage;

        if (fp->stat_flags.attack_id == nFTStatusAttackIDBatSwing4)
        {
            ftParamMakeRumble(fp, 10, 0);
        }
        else if ((s32) ((fp->attack_damage * 0.5F) + 2.0F) > 0)
        {
            ftParamMakeRumble(fp, 5, (s32)((fp->attack_damage * 0.5F) + 2.0F));
        }
    }
    else if (fp->reflect_damage != 0)
    {
        ftCommonShieldBreakFlyReflectorSetStatus(fighter_gobj);
    }
    else if (fp->reflect_lr != 0)
    {
        switch (fp->special_coll->kind)
        {
        case nFTSpecialCollKindFoxReflector:
            ftFoxSpecialLwHitSetStatus(fighter_gobj);
            break;

        case nFTSpecialCollKindNessReflector:
            func_800269C0_275C0(nSYAudioFGMBatHit);
            break;
        }
    }
    else if (fp->absorb_lr != 0)
    {
        ftNessSpecialLwProcAbsorb(fighter_gobj);
    }
    if (damage != 0)
    {
        fp->hitlag_tics = ftParamGetHitLag(damage, status_id, fp->hitlag_mul);

        if ((fp->hitlag_tics != 0) && (is_knockback_paused != FALSE))
        {
            fp->is_knockback_paused = TRUE;
        }
        fp->input.pl.button_tap = fp->input.pl.button_release = 0;

        if (fp->proc_lagstart != NULL)
        {
            fp->proc_lagstart(fighter_gobj);
        }
    }
    fp->unk_ft_0x7AC = 0;
    fp->attack_damage = 0;
    fp->attack_shield_push = 0;
    fp->shield_damage = 0;
    fp->shield_damage_total = 0;
    fp->damage_lag = 0;
    fp->damage_queue = 0;
    fp->damage_kind = nFTDamageKindDefault;

    fp->reflect_lr = 0;
    fp->reflect_damage = 0;

    fp->absorb_lr = 0;

    fp->unk_ft_0x7A0 = 0;
    fp->attack_rebound = 0;
    fp->damage_knockback = 0;
    fp->hitlag_mul = 1.0F;

    /* ft/ftmain.c:4020-4083, verbatim: a swing's trail
     * samples Link's sword joint, or the held Beam Sword's light joint, once
     * a frame outside hitlag. ftDisplayMainDrawAfterImage draws the three
     * samples it keeps. */
    if ((hitlag_tics == 0) && (fp->afterimage.drawstatus != -1))
    {
        switch (fp->afterimage.is_itemswing)
        {
        case FALSE:
            if ((fp->fkind == nFTKindLink) && (fp->modelpart_status[11 - nFTPartsJointCommonStart].modelpart_id_curr == 0))
            {
                FTParts *parts = fp->joints[11]->user_data.p;

                func_ovl2_800EDBA4(fp->joints[11]);

                fp->afterimage.desc[fp->afterimage.desc_id].translate_x = parts->mtx_translate[3][0];
                fp->afterimage.desc[fp->afterimage.desc_id].translate_y = parts->mtx_translate[3][1];
                fp->afterimage.desc[fp->afterimage.desc_id].translate_z = parts->mtx_translate[3][2];
                fp->afterimage.desc[fp->afterimage.desc_id].vec.x = parts->mtx_translate[2][0];
                fp->afterimage.desc[fp->afterimage.desc_id].vec.y = parts->mtx_translate[2][1];
                fp->afterimage.desc[fp->afterimage.desc_id].vec.z = parts->mtx_translate[2][2];

                if (fp->afterimage.desc_id == 2)
                {
                    fp->afterimage.desc_id = 0;
                }
                else fp->afterimage.desc_id++;

                if (fp->afterimage.drawstatus <= 2)
                {
                    fp->afterimage.drawstatus++;
                }
            }
            break;

        case TRUE:
            if ((fp->item_gobj != NULL) && (fp->is_item_show) && (itGetStruct(fp->item_gobj)->kind == nITKindSword))
            {
                s32 unused;
                Mtx44f mtx;

                func_ovl0_800C9A38(mtx, fp->joints[fp->attr->joint_itemlight_id]);

                fp->afterimage.desc[fp->afterimage.desc_id].translate_x = mtx[3][0];
                fp->afterimage.desc[fp->afterimage.desc_id].translate_y = mtx[3][1];
                fp->afterimage.desc[fp->afterimage.desc_id].translate_z = mtx[3][2];
                fp->afterimage.desc[fp->afterimage.desc_id].vec.x = mtx[1][0];
                fp->afterimage.desc[fp->afterimage.desc_id].vec.y = mtx[1][1];
                fp->afterimage.desc[fp->afterimage.desc_id].vec.z = mtx[1][2];

                syVectorNorm3D(&fp->afterimage.desc[fp->afterimage.desc_id].vec);

                if (fp->afterimage.desc_id == 2)
                {
                    fp->afterimage.desc_id = 0;
                }
                else fp->afterimage.desc_id++;

                if (fp->afterimage.drawstatus <= 2)
                {
                    fp->afterimage.drawstatus++;
                }
            }
            break;
        }
    }
}

/* The bzero arm of syDmaLoadOverlay for overlay 2, of which ft/ftmain is
 * a part (smashbrothers.us.yaml): src/dc/overlay.c calls it on the way
 * into every scene that loads that overlay. src/dc/overlay.h says why the
 * port needs it written out.
 *
 * The hit log is the one that matters. ftMainCheckHitLog (ftmain.c:1084)
 * refuses a hit whose attacker and victim are already in sFTMainHitLogs,
 * so a log left full from the previous battle would eat the first hits of
 * the next one between the same two players. */
void ftMainOverlayLoad(void)
{
    OVERLAY_CLEAR(gFTMainIsAttackDetect);
    OVERLAY_CLEAR(gFTMainIsDamageDetect);
    OVERLAY_CLEAR(sFTMainGroundHazards);
    OVERLAY_CLEAR(sFTMainGroundHazardsNum);
    OVERLAY_CLEAR(sFTMainGroundObstacles);
    OVERLAY_CLEAR(sFTMainGroundObstaclesNum);
    OVERLAY_CLEAR(sFTMainHitLogs);
    OVERLAY_CLEAR(sFTMainHitLogID);
}
