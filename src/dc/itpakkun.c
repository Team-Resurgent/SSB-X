/* itpakkun.c -- it/itground/itpakkun.c, verbatim but for the four script
 * reads: Mushroom Kingdom's Piranha Plant, the last of that stage's hazards.
 *
 * A Piranha Plant pipes up out of a warp pipe, is hittable on the way,
 * and -- if the hit is hard enough -- is knocked out of the pipe, falls,
 * dies and comes back. Three statuses and three scripts:
 *
 *   Wait     a timer, and a check that no fighter is standing on the
 *            pipe (itPakkunCommonCheckNoFighter, a box
 *            ITPAKKUN_DETECT_SIZE_* wide around its own spawn);
 *   Appear   the plant rises, and its HURTBOX grows with it
 *            (itPakkunAppearUpdateDamageColl -- the plant is only
 *            hittable above ITPAKKUN_CLAMP_OFF_Y, and the box is scaled
 *            by how far it has come up);
 *   Damaged  thrown, gravity, and on `proc_dead` it teleports home and
 *            starts over.
 *
 * Its data is the same shape as the `?` block's (see
 * src/dc/itpowerblock.c's header for the whole argument): attributes in
 * Mushroom Kingdom's own map file at 0x120, carried by the item pack
 * under that key, `p_file` diverged to `&gITManagerCommonData`.
 *
 * TWO THINGS IT HAS THAT THE `?` BLOCK DOES NOT:
 *
 *  - AN MObj. Its `p_mobjsubs` is `mobjlink_0x0A68`, two MObjSubs, so
 *    the pack carries an MObj section and `dobj->mobj` is a real
 *    pointer -- which three functions here write through, and which the
 *    engine's gcSetupCustomDObjs gives every DObjDesc entry on the N64.
 *
 *  - MATANIM JOINTS, two of them, hung on that MObj rather than on the
 *    DObj: `gcAddMObjMatAnimJoint(dobj->mobj, ..., 0.0F)` is a PALETTE
 *    swap, the plant's undamaged colours and its damaged ones. The port
 *    carries both as named stage scripts and calls the same function --
 *    this is different from the shells' unlanded palette gap: there the
 *    pack's MObj has one alt and nothing selects another, whereas here
 *    the game names the script at the moment it happens and the port names
 *    the same one. What the pack's MObj bakes is the FIRST alt's colours;
 *    swapping to the second is the engine's own `mobj->matanim_joint.event32 =
 *    script`, which is the whole of what gcAddMObjMatAnimJoint does.
 *
 * DIVERGES, and every one of them is a script the game reaches by
 * offset arithmetic into the map file (`lbRelocGetFileData(AObjEvent32*,
 * gGRCommonStruct.inishie.map_head, &llGRInishieMap<Name>)`) and the
 * port by name, `stage_map_anim(...)`:
 *
 *     llGRInishieMapPakkunAppearAnimJoint        "PakkunAppearAnimJoint"
 *     llGRInishieMapPakkunAppearMatAnimJoint     "PakkunAppearMatAnimJoint"
 *     llGRInishieMapPakkunDamagedMatAnimJoint    "PakkunDamagedMatAnimJoint"
 *
 * The script attaches are `#ifdef FT_HOSTTEST`-guarded for grcastle.c's
 * own reason: `union AObjEvent32` is eight bytes on x86-64, so the anim
 * interpreter walks a script twice as fast as it should and never
 * terminates.
 */
#include <it/item.h>
#include <ft/fighter.h>
#include <gr/ground.h>
#include <gr/grcommon/grinishie.h> /* itPakkunCommonSetWaitFighter's caller */
#include <sys/objanim.h>          /* gcAddDObjAnimJoint, gcAddMObjMatAnimJoint */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

#include "stage.h"                /* stage_map_anim */

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/itpakkun.c:11-32 dITPakkunItemDesc, verbatim but for
 * `p_file` and the `o_attributes` spelling -- the same divergence
 * src/dc/itpowerblock.c's header argues for. `nGCMatrixKindTra` as the
 * main transform and `0x30` as the secondary are the decomp's own
 * values, kept as written (the port bakes its transforms into the model,
 * src/dc/objmodel.c, so neither is read on this side). */
ITDesc dITPakkunItemDesc =
{
    nITKindPakkun,                          /* Item Kind */
    /* DIVERGES: the decomp's `&gGRCommonStruct.inishie.item_head`. */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)0x120,                        /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTra,                   /* Main matrix transformations */
        0x30,                               /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateOff,                      /* Hitbox Update State */
    itPakkunWaitProcUpdate,                 /* Proc Update */
    NULL,                                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itground/itpakkun.c:34-72 dITPakkunStatusDescs, verbatim. Wait is
 * the pipe's own state (its `proc_update` is the SAME function the desc
 * carries, which is why the plant starts there), Appear is the rise and
 * Damaged is everything after the knockback. */
ITStatusDesc dITPakkunStatusDescs[/* */] =
{
    /* Status 0 (Dokan Wait) */
    {
        itPakkunWaitProcUpdate,             /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 1 (Dokan Appear) */
    {
        itPakkunAppearProcUpdate,           /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        itPakkunAppearProcDamage            /* Proc Damage */
    },

    /* Status 2 (Neutral Damage) */
    {
        itPakkunDamagedProcUpdate,          /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itPakkunStatus
{
    nITPakkunStatusWait,
    nITPakkunStatusAppear,
    nITPakkunStatusDamaged,
    nITPakkunStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/itpakkun.c:81-87 itPakkunWaitSetStatus 0x8017CF20,
 * verbatim: the Wait status and `proc_dead = NULL` together, which is
 * what stops a plant that has already died from dying again. */
void itPakkunWaitSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITPakkunStatusDescs, nITPakkunStatusWait);

    itGetStruct(item_gobj)->proc_dead = NULL;
}

/* it/itground/itpakkun.c:89-92 itPakkunAppearSetStatus 0x8017CF58,
 * verbatim. */
void itPakkunAppearSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITPakkunStatusDescs, nITPakkunStatusAppear);
}

/* it/itground/itpakkun.c:94-99 itPakkunDamagedSetStatus 0x8017CF80,
 * verbatim: the only place `proc_dead` is set, which is the death the
 * engine runs when the plant leaves the blast line. */
void itPakkunDamagedSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITPakkunStatusDescs, nITPakkunStatusDamaged);

    itGetStruct(item_gobj)->proc_dead = itPakkunDamagedProcDead;
}

/* it/itground/itpakkun.c:101-108 itPakkunCommonSetWaitFighter
 * 0x8017CFC0, verbatim: the STAGE calls this for every plant when a
 * fighter touches the pipe (grInishiePakkunSetWaitFighter), and all it
 * does is raise the flag the Wait state reads next tic. A NULL GObj --
 * a plant that failed to make -- is fine, and is the decomp's own
 * guard. */
void itPakkunCommonSetWaitFighter(GObj *item_gobj)
{
    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        ip->item_vars.pakkun.is_wait_fighter = TRUE;
    }
}

/* it/itground/itpakkun.c:110-143 itPakkunCommonCheckNoFighter 0x8017CFDC,
 * verbatim: is any fighter standing in the box around this plant's own
 * spawn point? The box is x within ITPAKKUN_DETECT_SIZE_WIDTH and y
 * between BOTTOM and TOP above `pos`, and the position compared against
 * is each fighter's TopN joint -- NOT his origin. A plant with a fighter
 * in the box does not come up. */
sb32 itPakkunCommonCheckNoFighter(GObj *item_gobj)
{
    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);
        GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
        f32 it_pos_x = ip->item_vars.pakkun.pos.x;
        f32 it_pos_y = ip->item_vars.pakkun.pos.y;

        while (fighter_gobj != NULL)
        {
            FTStruct *fp = ftGetStruct(fighter_gobj);
            DObj *dobj = fp->joints[nFTPartsJointTopN];
            f32 dist_x, ft_pos_y;

            if (dobj->translate.vec.f.x < it_pos_x)
            {
                dist_x = -(dobj->translate.vec.f.x - it_pos_x);
            }
            else dist_x = (dobj->translate.vec.f.x - it_pos_x);

            ft_pos_y = dobj->translate.vec.f.y;

            if ((dist_x < ITPAKKUN_DETECT_SIZE_WIDTH) && (ft_pos_y > (it_pos_y + ITPAKKUN_DETECT_SIZE_BOTTOM)) && (ft_pos_y < (it_pos_y + ITPAKKUN_DETECT_SIZE_TOP)))
            {
                return FALSE;
            }
            fighter_gobj = fighter_gobj->link_next;
        }
    }
    return TRUE;
}

/* it/itground/itpakkun.c:145-178 itPakkunWaitProcUpdate 0x8017D0A4,
 * verbatim but for the two script reads.
 *
 * The plant counts down (or re-arms, if the stage has just told it a
 * fighter is near), and on zero -- with nobody in the box -- raises
 * itself and plays the appear animation. The two attaches are the pair
 * the header describes: the DObj's own AnimJoint and the PALETTE
 * MatAnimJoint on its MObj. */
sb32 itPakkunWaitProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->item_vars.pakkun.is_wait_fighter != FALSE)
    {
        ip->multi = ITPAKKUN_APPEAR_WAIT;
        ip->item_vars.pakkun.is_wait_fighter = FALSE;
    }
    if (--ip->multi == 0)
    {
        if (itPakkunCommonCheckNoFighter(item_gobj) != FALSE)
        {
            DObj *dobj = DObjGetStruct(item_gobj);

#ifndef FT_HOSTTEST
            /* DIVERGES: the decomp's two lbRelocGetFileData reads into
             * the map file; the port's pack names them. See this file's
             * header. */
            gcAddDObjAnimJoint(dobj, stage_map_anim("PakkunAppearAnimJoint"), 0.0F);
            gcAddMObjMatAnimJoint(dobj->mobj, stage_map_anim("PakkunAppearMatAnimJoint"), 0.0F);
            gcPlayAnimAll(item_gobj);
#endif
            dobj->translate.vec.f.y += ip->item_vars.pakkun.pos.y;

            itPakkunAppearSetStatus(item_gobj);
        }
        else ip->multi = ITPAKKUN_APPEAR_WAIT;
    }
    return FALSE;
}

/* it/itground/itpakkun.c:180-193 itPakkunWaitInitVars 0x8017D190,
 * verbatim. The plant is not hittable while it is down the pipe, and its
 * height is its own spawn's y -- `item_vars.pakkun.pos.y` is the spawn
 * VECTOR, reused here as the plant's resting height. */
void itPakkunWaitInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi = ITPAKKUN_APPEAR_WAIT;

    itPakkunWaitSetStatus(item_gobj);

    ip->damage_coll.hitstatus = nGMHitStatusNone;
    ip->attack_coll.attack_state = nGMAttackStateOff;

    DObjGetStruct(item_gobj)->translate.vec.f.y = ip->item_vars.pakkun.pos.y;
}

/* it/itground/itpakkun.c:195-218 itPakkunAppearUpdateDamageColl
 * 0x8017D1DC, verbatim: the hurtbox grows with the plant. `off_y` is how
 * far above the pipe's lip the plant has come, clamped by
 * ITPAKKUN_CLAMP_OFF_Y -- below that there is no hurtbox at all, and
 * above it the box's height is proportional and its offset follows, so
 * the hittable part is the part that has come out of the pipe. */
void itPakkunAppearUpdateDamageColl(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    f32 pos_y = DObjGetStruct(item_gobj)->translate.vec.f.y - ip->item_vars.pakkun.pos.y;
    f32 off_y = pos_y + ITPAKKUN_APPEAR_OFF_Y;

    if (off_y <= ITPAKKUN_CLAMP_OFF_Y)
    {
        ip->damage_coll.hitstatus = nGMHitStatusNone;
        ip->attack_coll.attack_state = nGMAttackStateOff;
    }
    else
    {
        if (ip->damage_coll.hitstatus == nGMHitStatusNone)
        {
            ip->damage_coll.hitstatus = nGMHitStatusNormal;

            itMainRefreshAttackColl(item_gobj);
        }
        ip->damage_coll.size.y = (off_y - ITPAKKUN_CLAMP_OFF_Y) * ITPAKKUN_HURT_SIZE_MUL_Y;
        ip->damage_coll.offset.y = (ip->damage_coll.size.y + ITPAKKUN_CLAMP_OFF_Y) - pos_y;
    }
}

/* it/itground/itpakkun.c:220-244 itPakkunAppearProcUpdate 0x8017D298,
 * verbatim. The rise: while the appear animation runs the plant climbs
 * by its own spawn height every tic -- which is what makes the box
 * above grow -- and when the animation ends (anim_wait AOBJ_ANIM_NULL)
 * it drops back to the pipe. The stage's own flag cancels the rise. */
sb32 itPakkunAppearProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj;

    if (ip->item_vars.pakkun.is_wait_fighter != FALSE)
    {
        DObjGetStruct(item_gobj)->anim_wait = AOBJ_ANIM_NULL;

        itPakkunWaitInitVars(item_gobj);

        ip->item_vars.pakkun.is_wait_fighter = FALSE;
    }
    dobj = DObjGetStruct(item_gobj);

    if (dobj->anim_wait == AOBJ_ANIM_NULL)
    {
        itPakkunWaitInitVars(item_gobj);
    }
    else dobj->translate.vec.f.y += ip->item_vars.pakkun.pos.y;

    itPakkunAppearUpdateDamageColl(item_gobj);

    return FALSE;
}

/* it/itground/itpakkun.c:246-276 itPakkunAppearProcDamage 0x8017D334,
 * verbatim but for the MatAnimJoint read.
 *
 * A hit over ITPAKKUN_NDAMAGE_KNOCKBACK_MIN knocks the plant out of the
 * pipe: its second XObj becomes kind 0x46 (the decomp's own number, kept
 * as written -- the transform kind the damaged plant rides), it is
 * flipped upside down, thrown along the damage angle the fighter system
 * gives it, and its palette switches to the damaged one. */
sb32 itPakkunAppearProcDamage(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->damage_knockback >= ITPAKKUN_NDAMAGE_KNOCKBACK_MIN)
    {
        DObj *dobj = DObjGetStruct(item_gobj);
        f32 angle;

        dobj->xobjs[1]->kind = 0x46;

        dobj->rotate.vec.f.z = F_CST_DTOR32(180.0F);

        angle = ftCommonDamageGetKnockbackAngle(ip->damage_angle, ip->ga, ip->damage_knockback);

        ip->physics.vel_air.x = __cosf(angle) * ip->damage_knockback * -ip->damage_lr;
        ip->physics.vel_air.y = __sinf(angle) * ip->damage_knockback;

        ip->damage_coll.hitstatus = nGMHitStatusNone;
        ip->attack_coll.attack_state = nGMAttackStateOff;

        itPakkunDamagedSetStatus(item_gobj);

        dobj->anim_wait = AOBJ_ANIM_NULL;

#ifndef FT_HOSTTEST
        /* DIVERGES: the decomp's lbRelocGetFileData read; the pack names
         * the script. The plant's palette is the damaged one from here
         * on. */
        gcAddMObjMatAnimJoint(dobj->mobj, stage_map_anim("PakkunDamagedMatAnimJoint"), 0.0F);
        gcPlayAnimAll(item_gobj);
#endif
    }
    return FALSE;
}

/* it/itground/itpakkun.c:278-288 itPakkunDamagedProcUpdate 0x8017D434,
 * verbatim: a knocked-out plant falls, and nothing else. */
sb32 itPakkunDamagedProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITPAKKUN_GRAVITY, ITPAKKUN_TVEL);

    return FALSE;
}

/* it/itground/itpakkun.c:290-313 itPakkunDamagedProcDead 0x8017D460,
 * verbatim. This is the engine's `proc_dead`, run when the plant leaves
 * the blast line: it teleports home, is set back upright, has its MObj's
 * animation cancelled, and goes back to Wait -- which is the whole
 * lifecycle. A plant is not destroyed, it is recycled. */
sb32 itPakkunDamagedProcDead(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    dobj->translate.vec.f = ip->item_vars.pakkun.pos;

    ip->multi = ITPAKKUN_REBIRTH_WAIT;

    ip->physics.vel_air.x = 0.0F;
    ip->physics.vel_air.y = 0.0F;
    ip->physics.vel_air.z = 0.0F;

    dobj->rotate.vec.f.z = 0.0F;

    dobj->mobj->anim_wait = AOBJ_ANIM_NULL;

    itPakkunWaitSetStatus(item_gobj);

    ip->item_vars.pakkun.is_wait_fighter = FALSE;

    return FALSE;
}

/* it/itground/itpakkun.c:315-347 itPakkunMakeItem 0x8017D4D8, verbatim
 * but for `p_file`'s divergence. The spawn VECTOR is kept whole -- it is
 * both the plant's position and, in the state machine above, its resting
 * height -- and nothing else about a stage item is set: this one CAN be
 * knocked back (`is_allow_knockback`), which is what the whole Damaged
 * status is for, and the shield may re-hit it. */
GObj* itPakkunMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITPakkunItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        ip->item_vars.pakkun.pos = *pos;

        DObjGetStruct(item_gobj)->translate.vec.f = *pos;

        ip->multi = ITPAKKUN_APPEAR_WAIT;

        ip->is_allow_knockback = TRUE;

        ip->item_vars.pakkun.is_wait_fighter = FALSE;

        ip->attack_coll.can_rehit_shield = TRUE;
    }
    return item_gobj;
}
