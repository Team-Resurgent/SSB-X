/* itmarumine.c -- it/itground/itmarumine.c, verbatim but for the one table
 * read: Saffron City's Electrode.
 *
 * Marumine rolls out of the gate (its `offset` added to the DObj's
 * translate every tic, the same walk Chansey's and Porygon's have), and
 * when its appear script ENDS it explodes: a sparkle, a screen quake, the
 * model hidden, its owner's stats cleared, and a six-tic Explode state
 * whose hitbox walks a four-entry `ITAttackEvent` script before the gate
 * is told it can close again.
 *
 * The hitbox is a BURST that shrinks: event 0 is the widest (size 700,
 * damage 30) and each of the next three is smaller, and the index wraps
 * from 4 back to 3 -- so the last event RE-FIRES rather than the sequence
 * restarting. It is also the one item here that clears `can_reflect` and
 * `can_shield` on itself and forces its element to Fire, all inside the
 * event arm.
 *
 * Its data is the stage's, the same shape as Chansey's and Porygon's (see
 * src/dc/itporygon.c's header for the whole argument): `ItemAttributes
 * Marumine`, 0x104 of Saffron City's map, which the item pack carries
 * under that key.
 *
 * DIVERGES, two, and both are table reads the port cannot do as the game
 * does:
 *
 *  1. THE ATTRIBUTES, exactly as the other stage items (the `p_file` line
 *     and the `o_attributes` spelling).
 *  2. THE ATTACK EVENTS. `itGetAttackEvent(desc, off)` is
 *     `*desc.p_file + off` -- `&llGRYamabukiMapMarumineAttackEvents`,
 *     0x14C of the stage's map file -- and the ROM's `ITAttackEvent` is
 *     BITFIELDS, so its bytes cannot be read as a host struct either.
 *     The pack carries the table PARSED, in its own ATTACK EVENTS
 *     section, keyed by the same `o_attributes` this desc carries;
 *     `itemPackAttackEvent` reads one event out of it. Same numbers, same
 *     order, no bitfields. (The base items -- MSBomb, Bomb Hei -- still
 *     read `itGetAttackEvent` over region 0's own bytes, which is a
 *     different divergence with its own row.)
 *
 * The appear script is attached by name in MakeItem, the same divergence
 * the Gate's other two monsters make and for the same reason: the pack's
 * record carries `anim_joints` as NO_PTR.
 */
#include <it/item.h>
#include <ft/fighter.h>
#include <gr/ground.h>
#include <gr/grcommon/gryamabuki.h> /* grYamabukiGateSetClosedWait */
#include <ef/effect.h>            /* efManagerSparkleWhiteMultiExplodeMakeEffect,
                                     efManagerQuakeMakeEffect */
#include <sys/objanim.h>          /* gcAddDObjAnimJoint, gcPlayAnimAll */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

#include "itempack.h"             /* itemPackAttackEvent */
#include "stage.h"                /* stage_map_anim */
#include "ftcommon.h"             /* func_800269C0_275C0 */

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/itmarumine.c:11-34 dITMarumineItemDesc, verbatim but for
 * `p_file` and the `o_attributes` spelling. `nGCMatrixKindTra` -- not the
 * TraRotRpyR the other two monsters ask for: an Electrode does not turn,
 * it ROLLS, and the roll is in its script's own RotZ. */
ITDesc dITMarumineItemDesc =
{
    nITKindMarumine,                        /* Item Kind */
    /* DIVERGES: the decomp's `&gGRCommonStruct.yamabuki.item_head`. */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)0x104,                        /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTra,                   /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateOff,                      /* Hitbox Update State */
    itMarumineCommonProcUpdate,             /* Proc Update */
    NULL,                                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itground/itmarumine.c:36-50 dITMarumineStatusDescs, verbatim: ONE
 * status, the exploding one. */
ITStatusDesc dITMarumineStatusDescs[/* */] =
{
    /* Status 0 (Neutral Explosion) */
    {
        itMarumineExplodeProcUpdate,        /* Proc Update */
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

enum itMarumineStatus
{
    nITMarumineStatusExplode,
    nITMarumineStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/itmarumine.c:72-94 itMarumineExplodeMakeEffectGotoSetStatus
 * 0x801837A0, verbatim. The hurtbox goes intangible FIRST -- an exploding
 * Electrode is not something a fighter can hit -- then the sparkle, scaled
 * to ITMARUMINE_EXPLODE_EFFECT_SCALE, then the quake, then the model is
 * hidden and the hitbox refreshed with the explosion's own FGM. */
void itMarumineExplodeMakeEffectGotoSetStatus(GObj *item_gobj)
{
    s32 unused;
    LBParticle *pc;
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    ip->damage_coll.hitstatus = nGMHitStatusNone;

    pc = efManagerSparkleWhiteMultiExplodeMakeEffect(&dobj->translate.vec.f);

    if (pc != NULL)
    {
        pc->xf->scale.x = ITMARUMINE_EXPLODE_EFFECT_SCALE;
        pc->xf->scale.y = ITMARUMINE_EXPLODE_EFFECT_SCALE;
        pc->xf->scale.z = ITMARUMINE_EXPLODE_EFFECT_SCALE;
    }
    efManagerQuakeMakeEffect(1);

    DObjGetStruct(item_gobj)->flags = DOBJ_FLAG_HIDDEN;

    ip->attack_coll.fgm_id = nSYAudioFGMExplodeL;

    itMainRefreshAttackColl(item_gobj);
    itMarumineExplodeSetStatus(item_gobj);
}

/* it/itground/itmarumine.c:96-121 itMarumineExplodeUpdateAttackEvent
 * 0x80183830, verbatim but for where the event comes from.
 *
 * THE BURST SHRINKS AND THE INDEX WRAPS TO THE LAST EVENT, not to zero:
 * `event_id` counts 0, 1, 2, 3, and the fourth increment takes it to 4 and
 * straight back to 3, so event 3's hitbox is the one that stays for the
 * rest of the six tics. A fighter caught on tic 0 takes 30% at size 700;
 * the same fighter on tic 5 takes 10% at size 200. */
void itMarumineExplodeUpdateAttackEvent(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    ITItemAttackEvent ev;

    if (itemPackAttackEvent((intptr_t)0x104, (u32)ip->event_id, &ev) != 0)
    {
        return;
    }
    if (ip->multi == (s32)ev.timer)
    {
        ip->attack_coll.angle  = (s32)ev.angle;
        ip->attack_coll.damage = (u32)ev.damage;
        ip->attack_coll.size   = (f32)ev.size;

        ip->attack_coll.can_reflect = FALSE;
        ip->attack_coll.can_shield = FALSE;

        ip->attack_coll.element = nGMHitElementFire;

        ip->attack_coll.can_setoff = FALSE;

        ip->event_id++;

        if (ip->event_id == 4)
        {
            ip->event_id = 3;
        }
    }
}

/* it/itground/itmarumine.c:123-144 itMarumineCommonProcUpdate 0x80183914,
 * verbatim: the roll (the spawn vector added every tic), and the END of
 * the appear script is the explosion. `itMainClearOwnerStats` runs BEFORE
 * the status change, so the fighter who threw it is not credited for the
 * blast -- and the walk vector is zeroed, because the explosion has its
 * own six-tick state and does not travel. */
sb32 itMarumineCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    dobj->translate.vec.f.x += ip->item_vars.marumine.offset.x;
    dobj->translate.vec.f.y += ip->item_vars.marumine.offset.y;

    if (dobj->anim_wait == AOBJ_ANIM_NULL)
    {
        itMainRefreshAttackColl(item_gobj);
        itMainClearOwnerStats(item_gobj);

        ip->item_vars.marumine.offset.x = 0.0F;
        ip->item_vars.marumine.offset.y = 0.0F;

        itMarumineExplodeMakeEffectGotoSetStatus(item_gobj);
        func_800269C0_275C0(nSYAudioFGMExplodeL);
    }
    return FALSE;
}

/* it/itground/itmarumine.c:146-166 itMarumineExplodeProcUpdate 0x801839A8,
 * verbatim: the same walk (now zeroed) and the event machine, and the end
 * of the six tics is what closes the gate and destroys the item. */
sb32 itMarumineExplodeProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    dobj->translate.vec.f.x += ip->item_vars.marumine.offset.x;
    dobj->translate.vec.f.y += ip->item_vars.marumine.offset.y;

    itMarumineExplodeUpdateAttackEvent(item_gobj);

    ip->multi++;

    if (ip->multi == ITMARUMINE_EXPLODE_LIFETIME)
    {
        grYamabukiGateSetClosedWait();

        return TRUE;
    }
    else return FALSE;
}

/* it/itground/itmarumine.c:168-180 itMarumineExplodeSetStatus 0x80183A20,
 * verbatim. `multi` and `event_id` are cleared and the FIRST event applied
 * in the same call, so event 0's numbers are installed on the tic the
 * status is entered -- not one tic later. */
void itMarumineExplodeSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi = 0;

    ip->attack_coll.throw_mul = 1.0F;

    ip->event_id = 0;

    itMarumineExplodeUpdateAttackEvent(item_gobj);
    itMainSetStatus(item_gobj, dITMarumineStatusDescs, nITMarumineStatusExplode);
}

/* it/itground/itmarumine.c:182-202 itMarumineMakeItem 0x80183A74, verbatim
 * but for the appear script's attach (see this file's header).
 *
 * `gcAddXObjForDObjFixed(dobj, 0x46, 0)` is the Electrode's own XObj -- a
 * kind-0x46 matrix on joint 0, which is what gives it its spin on the
 * target (the renderer's own DObj rotation is not it). */
GObj* itMarumineMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITMarumineItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);
        DObj *dobj = DObjGetStruct(item_gobj);

        ip->item_vars.marumine.offset = *pos;

        ip->is_allow_knockback = TRUE;

        gcAddXObjForDObjFixed(dobj, 0x46, 0);
        func_800269C0_275C0(nSYAudioVoiceYamabukiMarumine);
    }
    return item_gobj;
}
