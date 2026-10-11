/* ittarubomb.c -- it/itground/ittarubomb.c, verbatim but for two table
 * reads and the smash effect's display list: Race to the Finish's barrel
 * bomb.
 *
 * The barrel falls, lands, and ROLLS down whatever slope it landed on --
 * `itTaruBombRollProcUpdate` reads the floor's angle every tic and
 * pushes vel_air.x by how far that angle is from vertical, so the barrel
 * accelerates downhill and its model spins at the speed it is travelling
 * (`roll_rotate_step`, the one field in its `item_vars`). It explodes on
 * any hit, on a wall while rolling, on 10% of damage, and -- in theory --
 * on landing at 90 units/tic, a branch the decomp's own comment says it
 * cannot reach. There is no lifetime: grbonus3.c makes a new one every
 * 180 tics and the old ones go when they go.
 *
 * THREE STATES: Fall (0), Explode (1), Roll (2). Explode has no map,
 * hit, shield or damage proc at all -- once it is going off, nothing can
 * touch it -- and it lives ITTARUBOMB_EXPLODE_LIFETIME (6) tics walking
 * a four-entry `ITAttackEvent` burst.
 *
 * Its data is the stage's, the same shape as Saffron City's five (see
 * src/dc/itporygon.c's header for the whole argument): `ItemAttributes
 * TaruBomb`, 0xA8 of Race to the Finish's map, which the item pack
 * carries under that key.
 *
 * DIVERGES, three.
 *
 *  1. THE ATTRIBUTES, exactly as the other stage items (the `p_file`
 *     line and the `o_attributes` spelling).
 *  2. THE ATTACK EVENTS. `itGetAttackEvent(desc, off)` is
 *     `*desc.p_file + off` -- `&llGRBonus3MapTaruBombAttackEvents`, 0xF0
 *     of the stage's map file -- and the ROM's `ITAttackEvent` is
 *     BITFIELDS, so its bytes cannot be read as a host struct either.
 *     The pack carries the table PARSED, keyed by the same
 *     `o_attributes` this desc carries, and `tarubomb_update_attack_event`
 *     below is `itMainUpdateAttackEvent` (it/itmain.c:614-632) reading
 *     through `itemPackAttackEvent` instead of over a pointer. Same
 *     numbers, same order, same 4-wraps-to-3, no bitfields. This is the
 *     divergence src/dc/itmarumine.c already makes; the difference is
 *     only that the Electrode's version of the function is its own in
 *     the game too, where the barrel's is itmain's.
 *  3. THE SMASH EFFECT's display list, which is
 *     src/dc/itbox.c's DIVERGES one stage over. The decomp computes a
 *     raw `Gfx*` out of the item file (`attr->data - TaruBombDataStart +
 *     TaruBombEffectDisplayList`) and hands it to all seven pieces; the
 *     port draws nothing from a display list, so `DisplayList
 *     TaruBombEffect` is baked as the one-joint pack
 *     `itemModelTaruBombSmash` loads (ittarubombsmash.mdl) and each piece
 *     takes that joint's payload. Six verts and two
 *     triangles -- a debris quad, two more verts than the Crate's.
 *
 * The barrel's own tree comes through the item model table, keyed by the
 * same 0xA8; its record carries `anim_joints` as NO_PTR and it needs
 * none, since nothing about it is scripted.
 */
#include <it/item.h>
#include <ft/fighter.h>
#include <gr/ground.h>
#include <ef/effect.h>            /* efManagerSparkleWhiteMultiExplodeMakeEffect,
                                     efManagerQuakeMakeEffect, EFStruct */
#include <ef/efmanager.h>         /* efManagerGetEffectNoForce */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

#include "itempack.h"             /* itemPackAttackEvent */
#include "itemmodel.h"            /* itemModelTaruBombSmash */
#include "objmodel.h"             /* dc_model_add_dobjs, dc_model_hidden_payload */
#include "ftcommon.h"             /* func_800269C0_275C0 */

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/ittarubomb.c:22-43 dITTaruBombItemDesc, verbatim but for
 * `p_file` and the `o_attributes` spelling. It opens in Fall, and its
 * hit, shield, set-off and reflector procs are ALL the same function:
 * whatever touches a barrel, the barrel explodes. */
ITDesc dITTaruBombItemDesc =
{
    nITKindTaruBomb,                        /* Item Kind */
    /* DIVERGES: the decomp's `&gGRCommonStruct.bonus3.item_head`. */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)0xA8,                         /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,            /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itTaruBombFallProcUpdate,               /* Proc Update */
    itTaruBombFallProcMap,                  /* Proc Map */
    itTaruBombCommonProcHit,                /* Proc Hit */
    itTaruBombCommonProcHit,                /* Proc Shield */
    NULL,                                   /* Proc Hop */
    itTaruBombCommonProcHit,                /* Proc Set-Off */
    itTaruBombCommonProcHit,                /* Proc Reflector */
    itTaruBombCommonProcDamage              /* Proc Damage */
};

/* it/itground/ittarubomb.c:45-82 dITTaruBombStatusDescs 0x8018E0C0,
 * verbatim. */
ITStatusDesc dITTaruBombStatusDescs[/* */] =
{
    /* Status 0 (Air Wait Fall) */
    {
        itTaruBombFallProcUpdate,           /* Proc Update */
        itTaruBombFallProcMap,              /* Proc Map */
        itTaruBombCommonProcHit,            /* Proc Hit */
        itTaruBombCommonProcHit,            /* Proc Shield */
        NULL,                               /* Proc Hop */
        itTaruBombCommonProcHit,            /* Proc Set-Off */
        itTaruBombCommonProcHit,            /* Proc Reflector */
        itTaruBombCommonProcDamage          /* Proc Damage */
    },

    /* Status 1 (Neutral Explosion) */
    {
        itTaruBombExplodeProcUpdate,        /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 2 (Ground Roll) */
    {
        itTaruBombRollProcUpdate,           /* Proc Update */
        itTaruBombRollProcMap,              /* Proc Map */
        itTaruBombCommonProcHit,            /* Proc Hit */
        itTaruBombCommonProcHit,            /* Proc Shield */
        NULL,                               /* Proc Hop */
        itTaruBombCommonProcHit,            /* Proc Set-Off */
        itTaruBombCommonProcHit,            /* Proc Reflector */
        itTaruBombCommonProcDamage          /* Proc Damage */
    }
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itTaruBombStatus
{
    nITTaruBombStatusFall,
    nITTaruBombStatusExplode,
    nITTaruBombStatusRoll,
    nITTaruBombStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* THE DIVERGES (2): it/itmain.c:614-632 itMainUpdateAttackEvent, reading
 * the pack's parsed table rather than an `ITAttackEvent*` the caller
 * worked out of the stage's map file. Body for body the same, including
 * the wrap that takes `event_id` 4 back to 3 -- so the burst's LAST
 * event is the one that stays for the rest of the six tics, not the
 * first. The barrel's four are 16% at size 350, 11% at 250, 8% at 150
 * and finally 1% at size 0: a hitbox that has stopped, which is how the
 * blast stops hurting while its sparkle is still playing. */
static void tarubomb_update_attack_event(GObj *item_gobj,
                                         intptr_t o_attributes)
{
    ITStruct *ip = itGetStruct(item_gobj);
    ITItemAttackEvent ev;

    if (itemPackAttackEvent(o_attributes, (u32)ip->event_id, &ev) != 0)
    {
        return;
    }
    if (ip->multi == (s32)ev.timer)
    {
        ip->attack_coll.angle  = (s32)ev.angle;
        ip->attack_coll.damage = (u32)ev.damage;
        ip->attack_coll.size   = (f32)ev.size;

        ip->event_id++;

        if (ip->event_id == 4)
        {
            ip->event_id = 3;
        }
    }
}

/* it/itground/ittarubomb.c:104-129 itTaruBombContainerSmashUpdateEffect
 * 0x80184A70, verbatim: the same custom per-frame walk itbox.c's
 * Container debris takes. `scale` is the piece's VELOCITY and
 * anim_wait/anim_speed/anim_frame are its three spin rates -- fields
 * repurposed because this effect has no AObj of its own. */
void itTaruBombContainerSmashUpdateEffect(GObj *effect_gobj) /* RTTF bomb explode GFX process */
{
    EFStruct *ep = efGetStruct(effect_gobj);
    DObj *dobj = DObjGetStruct(effect_gobj);

    ep->effect_vars.container.lifetime--;

    if (ep->effect_vars.container.lifetime == 0)
    {
        efManagerSetPrevStructAlloc(ep);
        gcEjectGObj(effect_gobj);
    }
    else while (dobj != NULL)
    {
        dobj->scale.vec.f.y -= 1.3F;

        dobj->translate.vec.f.x += dobj->scale.vec.f.x; /* This makes no sense, seems this custom effect is very... custom */
        dobj->translate.vec.f.y += dobj->scale.vec.f.y;
        dobj->translate.vec.f.z += dobj->scale.vec.f.z;

        dobj->rotate.vec.f.x += dobj->anim_wait; /* ??? Seems to be rotation step, but only in this case? Otherwise -FLOAT32_MAX? */
        dobj->rotate.vec.f.y += dobj->anim_speed;
        dobj->rotate.vec.f.z += dobj->anim_frame;

        dobj = dobj->sib_next;
    }
}

/* it/itground/ittarubomb.c:132-172 itTaruBombContainerSmashMakeEffect
 * 0x80184B44, with the file header's DIVERGES (3). Seven pieces, each
 * with its own random velocity and spin, alive for
 * ITTARUBOMB_GFX_LIFETIME frames.
 *
 * The FIRST piece goes through dc_model_add_dobjs and the rest take
 * joint 0's payload, for the reason src/dc/itbox.c spells out: that call
 * is what allocates the model's display table, and
 * dc_model_hidden_payload reads it. */
void itTaruBombContainerSmashMakeEffect(Vec3f *pos)
{
    GObj *effect_gobj;
    EFStruct *ep = efManagerGetEffectNoForce();
    Fighter *pack;
    DObj *dobj;
    s32 i;

    if (ep != NULL)
    {
        effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

        if (effect_gobj != NULL)
        {
            /* DIVERGES: dc_model_proc_display, not the decomp's
             * gcDrawDObjTreeForGObj -- see src/dc/itdisplay.c's header
             * for why the raw walk alone never submits a baked model's
             * triangles to the PVR. */
            gcAddGObjDisplay(effect_gobj, dc_model_proc_display, 11, GOBJ_PRIORITY_DEFAULT, ~0);

            pack = itemModelTaruBombSmash();

            if ((pack != NULL) &&
                (dc_model_add_dobjs(effect_gobj, NULL, pack, NULL) < 0))
            {
                pack = NULL;
            }
            for (i = 0; i < ITTARUBOMB_EFFECT_COUNT; i++)
            {
                dobj = ((pack != NULL) && (i == 0))
                           ? DObjGetStruct(effect_gobj)
                           : gcAddDObjForGObj(effect_gobj, NULL);

                if ((pack != NULL) && (i != 0))
                {
                    dobj->dv = dc_model_hidden_payload(pack, 0);
                }
                gcAddXObjForDObjFixed(dobj, nGCMatrixKindTraRotRpyR, 0);

                dobj->translate.vec.f = *pos;

                dobj->scale.vec.f.x = (syUtilsRandFloat() * 48.0F) + -24.0F;
                dobj->scale.vec.f.y = (syUtilsRandFloat() * 50.0F) + 10.0F;
                dobj->scale.vec.f.z = (syUtilsRandFloat() * 32.0F) + -16.0F;

                dobj->anim_wait = F_CLC_DTOR32((syUtilsRandFloat() * 100.0F) + -50.0F);
                dobj->anim_speed = F_CLC_DTOR32((syUtilsRandFloat() * 100.0F) + -50.0F);
                dobj->anim_frame = F_CLC_DTOR32((syUtilsRandFloat() * 100.0F) + -50.0F);
            }
            ep->effect_vars.container.lifetime = ITTARUBOMB_GFX_LIFETIME;

            effect_gobj->user_data.p = ep;

            gcAddGObjProcess(effect_gobj, itTaruBombContainerSmashUpdateEffect, 1, 3);
        }
    }
}

/* it/itground/ittarubomb.c:175-185 itTaruBombFallProcUpdate 0x80184D74,
 * verbatim: gravity, and the barrel keeps the spin it had when it left
 * the ground -- `roll_rotate_step` is not recomputed in the air. */
sb32 itTaruBombFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj;

    itMainApplyGravityClampTVel(ip, ITTARUBOMB_GRAVITY, ITTARUBOMB_TVEL);

    dobj = DObjGetStruct(item_gobj);
    dobj->rotate.vec.f.z += ip->item_vars.tarubomb.roll_rotate_step;

    return FALSE;
}

/* it/itground/ittarubomb.c:188-195 itTaruBombCommonProcHit 0x80184DC4,
 * verbatim: the sound, the debris, and the explosion. This one function
 * is the desc's hit, shield, set-off AND reflector proc. */
sb32 itTaruBombCommonProcHit(GObj *item_gobj)
{
    func_800269C0_275C0(nSYAudioFGMTaruBombHit);
    itTaruBombContainerSmashMakeEffect(&DObjGetStruct(item_gobj)->translate.vec.f);
    itTaruBombExplodeMakeEffectGotoSetStatus(item_gobj);

    return FALSE;
}

/* it/itground/ittarubomb.c:198-207 itTaruBombCommonProcDamage 0x80184E04,
 * verbatim: a barrel takes ITTARUBOMB_HEALTH_MAX (10%) of damage before
 * it goes, where a Crate takes 15. */
sb32 itTaruBombCommonProcDamage(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->percent_damage >= ITTARUBOMB_HEALTH_MAX)
    {
        return itTaruBombCommonProcHit(item_gobj);
    }
    return FALSE;
}

/* it/itground/ittarubomb.c:210-217 itTaruBombRollSetStatus 0x80184E44,
 * verbatim. */
void itTaruBombRollSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->physics.vel_air.y = 0.0F;

    itMainSetStatus(item_gobj, dITTaruBombStatusDescs, nITTaruBombStatusRoll);
}

/* it/itground/ittarubomb.c:220-235 itTaruBombFallCheckCollideGround
 * 0x80184E78, verbatim, `unused` and all: a ceiling or a wall rebounds
 * the barrel and re-rolls its spin, and the return value is only about
 * the FLOOR. */
sb32 itTaruBombFallCheckCollideGround(GObj *item_gobj, f32 common_rebound)
{
    s32 unused;
    ITStruct *ip;
    sb32 is_collide_floor = itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR);

    if (itMapCheckCollideAllRebound(item_gobj, (MAP_FLAG_CEIL | MAP_FLAG_RWALL | MAP_FLAG_LWALL), common_rebound, NULL) != FALSE)
    {
        itMainSetSpinVelLR(item_gobj);
    }
    if (is_collide_floor != FALSE)
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itground/ittarubomb.c:238-269 itTaruBombFallProcMap 0x80184EDC,
 * verbatim, including the branch the decomp's own comment says is dead:
 * `vel_air.y >= 90.0F` on a floor collision is a landing SPEED test
 * written with the sign the wrong way round, and a falling barrel's
 * vel_air.y is negative. Below 30 it settles into the roll; between the
 * two it bounces off the floor angle at a fifth of its speed. */
sb32 itTaruBombFallProcMap(GObj *item_gobj)
{
    if (itTaruBombFallCheckCollideGround(item_gobj, ITTARUBOMB_MAP_REBOUND_COMMON) != FALSE)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        if (ip->physics.vel_air.y >= 90.0F) /* Is it even possible to meet this condition? Didn't they mean <= inverse of this value? */
        {
            itTaruBombCommonProcHit(item_gobj); /* This causes the bomb to smash on impact when landing from too high; doesn't seem possible to trigger */

            return TRUE;
        }
        else if (ip->physics.vel_air.y < 30.0F)
        {
            itTaruBombRollSetStatus(item_gobj);
        }
        else
        {
            lbCommonReflect2D(&ip->physics.vel_air, &ip->coll_data.floor_angle);

            ip->physics.vel_air.y *= 0.2F;

            itMainSetSpinVelLR(item_gobj);
        }
        func_800269C0_275C0(nSYAudioFGMTaruBombMap);
        itMainClearOwnerStats(item_gobj);
    }
    return FALSE;
}

/* it/itground/ittarubomb.c:273-281 itTaruBombCommonSetMapCollisionBox
 * 0x80184FAC, verbatim: the barrel is laid on its side (a quarter turn
 * about x) and its map collision box is made SQUARE, top and bottom both
 * taken from the width the attributes give.  */
void itTaruBombCommonSetMapCollisionBox(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    DObjGetStruct(item_gobj)->rotate.vec.f.x = F_CLC_DTOR32(90.0F);

    ip->coll_data.map_coll.top = ip->coll_data.map_coll.width;
    ip->coll_data.map_coll.bottom = -ip->coll_data.map_coll.width;
}

/* it/itground/ittarubomb.c:284-297 itTaruBombExplodeProcUpdate
 * 0x80184FD4, verbatim but for the DIVERGES (2) call: six tics, then the
 * item is destroyed (TRUE), and every tic before that steps the burst. */
sb32 itTaruBombExplodeProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi++;

    if (ip->multi == ITTARUBOMB_EXPLODE_LIFETIME)
    {
        return TRUE;
    }
    else tarubomb_update_attack_event(item_gobj,
                                dITTaruBombItemDesc.o_attributes);

    return FALSE;
}

/* it/itground/ittarubomb.c:300-317 itTaruBombRollProcUpdate 0x80185030,
 * verbatim: the roll. The floor's angle away from vertical is what
 * pushes the barrel along, so it accelerates downhill and stands still
 * on the flat, and its spin is its SPEED -- the length of the whole
 * velocity vector -- turned the way it is going. */
sb32 itTaruBombRollProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    f32 roll_rotate_step;
    f32 sqrt_vel;

    ip->physics.vel_air.x += (-(syUtilsArcTan2(ip->coll_data.floor_angle.y, ip->coll_data.floor_angle.x) - F_CLC_DTOR32(90.0F) /*HALF_PI32*/) * ITTARUBOMB_MUL_VEL_X);

    ip->lr = (ip->physics.vel_air.x >= 0.0F) ? +1 : -1;

    sqrt_vel = sqrtf(SQUARE(ip->physics.vel_air.x) + SQUARE(ip->physics.vel_air.y));

    roll_rotate_step = ((ip->lr == -1) ? ITTARUBOMB_ROLL_ROTATE_MUL : -ITTARUBOMB_ROLL_ROTATE_MUL) * sqrt_vel;

    ip->item_vars.tarubomb.roll_rotate_step = roll_rotate_step;

    DObjGetStruct(item_gobj)->rotate.vec.f.z += roll_rotate_step;

    return FALSE;
}

/* it/itground/ittarubomb.c:320-333 itTaruBombRollProcMap 0x8018511C,
 * verbatim: roll off the end of the floor and it falls again; roll INTO
 * a wall and it explodes. */
sb32 itTaruBombRollProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestLRWallCheckFloor(item_gobj) == FALSE)
    {
        itMainSetStatus(item_gobj, dITTaruBombStatusDescs, nITTaruBombStatusFall);
    }
    else if (ip->coll_data.mask_curr & (MAP_FLAG_RWALL | MAP_FLAG_LWALL))
    {
        return itTaruBombCommonProcHit(item_gobj);
    }
    return FALSE;
}

/* it/itground/ittarubomb.c:336-349 itTaruBombMakeItem 0x8018518C,
 * verbatim: the spin starts at zero and the collision box is squared up
 * before the barrel has taken a single tic. */
GObj* itTaruBombMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITTaruBombItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        ip->item_vars.tarubomb.roll_rotate_step = 0.0F;

        itTaruBombCommonSetMapCollisionBox(item_gobj);
    }
    return item_gobj;
}

/* it/itground/ittarubomb.c:352-372 itTaruBombExplodeInitVars 0x801851F4,
 * verbatim but for the DIVERGES (2) call: the blast is Fire, it can
 * re-hit an item, it cannot be reflected and it cannot be set off, and
 * the barrel's own hurtbox is switched off before the first event is
 * installed. */
void itTaruBombExplodeInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi = 0;
    ip->event_id = 0;

    ip->attack_coll.fgm_id = nSYAudioFGMExplodeL;

    ip->attack_coll.can_rehit_item = TRUE;
    ip->attack_coll.can_reflect = FALSE;

    ip->attack_coll.throw_mul = ITEM_THROW_DEFAULT;
    ip->attack_coll.element = nGMHitElementFire;

    ip->attack_coll.can_setoff = FALSE;

    ip->damage_coll.hitstatus = nGMHitStatusNone;

    itMainRefreshAttackColl(item_gobj);
    tarubomb_update_attack_event(item_gobj,
                                dITTaruBombItemDesc.o_attributes);
}

/* it/itground/ittarubomb.c:375-379 itTaruBombExplodeSetStatus 0x80185284,
 * verbatim. */
void itTaruBombExplodeSetStatus(GObj *item_gobj)
{
    itTaruBombExplodeInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITTaruBombStatusDescs, nITTaruBombStatusExplode);
}

/* it/itground/ittarubomb.c:382-405 itTaruBombExplodeMakeEffectGotoSetStatus
 * 0x801852B8, verbatim: the hitbox is turned off and the barrel stopped
 * dead before the sparkle is made, the model is hidden (the debris is
 * what is on screen from here), and the quake is the one-argument kind.
 * The sparkle is scaled ITTARUBOMB_EXPLODE_EFFECT_SCALE (1.4x) through
 * its particle's own XF, which is why the NULL check is here at all. */
void itTaruBombExplodeMakeEffectGotoSetStatus(GObj *item_gobj)
{
    LBParticle *pc;
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    ip->attack_coll.attack_state = nGMAttackStateOff;

    ip->physics.vel_air.x = 0.0F;
    ip->physics.vel_air.y = 0.0F;
    ip->physics.vel_air.z = 0.0F;

    pc = efManagerSparkleWhiteMultiExplodeMakeEffect(&dobj->translate.vec.f);

    if (pc != NULL)
    {
        pc->xf->scale.x =
        pc->xf->scale.y =
        pc->xf->scale.z = ITTARUBOMB_EXPLODE_EFFECT_SCALE;
    }
    efManagerQuakeMakeEffect(1);

    DObjGetStruct(item_gobj)->flags = DOBJ_FLAG_HIDDEN;

    itTaruBombExplodeSetStatus(item_gobj);
}
