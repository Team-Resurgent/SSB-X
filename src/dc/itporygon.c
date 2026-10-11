/* itporygon.c -- it/itground/itporygon.c, verbatim but for the one table
 * read: Saffron City's Porygon.
 *
 * Porygon comes out of the gate and shakes: its `offset` is added to the
 * DObj's translate every tic (the walk), and its hitbox is moved through
 * a scripted sequence by `itPorygonCommonUpdateMonsterEvent` -- a table
 * of `ITMonsterEvent`s indexed by `ip->event_id`, each naming a TIMER and
 * the hitbox numbers to install when `ip->multi` reaches it. The sequence
 * is two events that alternate: event 0's numbers, then event 1's, then
 * back to 1 for the rest of the item's life (the `event_id == 2` branch
 * below is the decomp's own wrap).
 *
 * Its data is the stage's, the same shape as Chansey's (see
 * src/dc/itglucky.c's header): `ItemAttributes Porygon`, 0x16C of
 * Saffron City's map, which the item pack carries under that key.
 *
 * DIVERGES, one: THE EVENT TABLE. `itGetMonsterEvent(desc, off)` is
 * `*desc.p_file + off` -- an offset into the stage's map file, which the
 * port does not have -- and the ROM's `ITMonsterEvent` is BITFIELDS, so
 * its bytes cannot be read as a host struct either. The port carries the
 * table PARSED, in the pack's own MONSTER EVENTS section;
 * `itemPackMonsterEvent` reads one event out of it. Same numbers, same
 * order, no bitfields.
 *
 * THE KEY IS THE DESC's `o_attributes`, not the game's own. The game
 * passes `&llGRYamabukiMapPorygonHitParties` -- 0x1B4 of Saffron City's
 * map, the events table's own offset, which is 0x48 past the attributes
 * -- and the port's desc carries no such symbol. It carries 0x16C, the
 * number `itemPackAttr` and `itemPackMonsterEvent` both key on, and that
 * is what this file passes. The association is the same one either way:
 * the table belongs to the item, and the item is named by its attributes.
 *
 * The appear script is attached by name in MakeItem, the same divergence
 * src/dc/itglucky.c makes and for the same reason: the pack's record
 * carries `anim_joints` as NO_PTR.
 */
#include <it/item.h>
#include <ft/fighter.h>
#include <gr/ground.h>
#include <gr/grcommon/gryamabuki.h> /* grYamabukiGateSetClosedWait */
#include <ef/effect.h>            /* efManagerDustLightMakeEffect */
#include <sys/objanim.h>          /* gcAddDObjAnimJoint, gcPlayAnimAll */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

#include "itempack.h"             /* itemPackMonsterEvent */
#include "stage.h"                /* stage_map_anim */
#include "ftcommon.h"             /* func_800269C0_275C0 */

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/itporygon.c:11-32 dITPorygonItemDesc, verbatim but for
 * `p_file` and the `o_attributes` spelling. */
ITDesc dITPorygonItemDesc =
{
    nITKindPorygon,                         /* Item Kind */
    /* DIVERGES: the decomp's `&gGRCommonStruct.yamabuki.item_head`. */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)0x16C,                        /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,            /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itPorygonCommonProcUpdate,              /* Proc Update */
    NULL,                                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/itporygon.c:42-77 itPorygonCommonUpdateMonsterEvent
 * 0x80183B10, verbatim but for where the event comes from.
 *
 * `ip->multi` is a tic counter and `ip->event_id` is which event is
 * armed; when the counter equals the armed event's timer, that event's
 * hitbox numbers are written into `ip->attack_coll` and the NEXT event is
 * armed. A hitbox installed this way is not live until
 * `itMainRefreshAttackColl` says so -- the same keystone call every other
 * item's attack state goes through.
 *
 * The dust puff is on the SHAKE's own schedule: when the counter reaches
 * ITPORYGON_SHAKE_STOP_WAIT the plant/animal's shake stops, and the effect
 * is put at the item's own x and z with y zeroed, which is the floor it
 * is standing on rather than its own raised height.
 *
 * The one thing here the decomp does not have is the early return: a pack
 * with no events record for this item is a build error, and returning
 * leaves the item's hitbox at its ITDesc's `nGMAttackStateNew` defaults
 * rather than reading a table that is not there. Nothing else in the
 * function changes -- including the `multi` tick, which the caller's own
 * ProcUpdate does not depend on to animate. */
void itPorygonCommonUpdateMonsterEvent(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    ITItemEvent ev;

    if (itemPackMonsterEvent((intptr_t)0x16C, (u32)ip->event_id, &ev) != 0)
    {
        return;
    }
    if (ip->multi == (s32)ev.timer)
    {
        ip->attack_coll.angle            = (s32)ev.angle;
        ip->attack_coll.damage           = (u32)ev.damage;
        ip->attack_coll.size             = (f32)ev.size;
        ip->attack_coll.knockback_scale  = (f32)ev.knockback_scale;
        ip->attack_coll.knockback_weight = (u32)ev.knockback_weight;
        ip->attack_coll.knockback_base   = (u32)ev.knockback_base;
        ip->attack_coll.element          = (s32)ev.element;
        ip->attack_coll.can_setoff       = (u8)ev.can_setoff;
        ip->attack_coll.shield_damage    = (u8)ev.shield_damage;
        ip->attack_coll.fgm_id           = (u16)ev.fgm_id;

        ip->event_id++;

        if (ip->event_id == 2)
        {
            ip->event_id = 1;
        }
    }
    ip->multi++;

    if (ip->multi == ITPORYGON_SHAKE_STOP_WAIT)
    {
        Vec3f pos = DObjGetStruct(item_gobj)->translate.vec.f;

        pos.y = 0.0F;

        efManagerDustLightMakeEffect(&pos, -1, 1.0F);
    }
}

/* it/itground/itporygon.c:80-98 itPorygonCommonProcUpdate 0x80183C84,
 * verbatim: the walk (the spawn vector added every tic, the same shape
 * Chansey's is), the event machine, and the end of the animation -- which
 * closes the gate and destroys the item. */
sb32 itPorygonCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    dobj->translate.vec.f.x += ip->item_vars.porygon.offset.x;
    dobj->translate.vec.f.y += ip->item_vars.porygon.offset.y;

    itPorygonCommonUpdateMonsterEvent(item_gobj);

    if (dobj->anim_wait == AOBJ_ANIM_NULL)
    {
        grYamabukiGateSetClosedWait();

        return TRUE;
    }
    else return FALSE;
}

/* it/itground/itporygon.c:100-119 itPorygonMakeItem 0x80183D00, verbatim
 * but for the appear script's attach (see this file's header). `event_id`
 * starts at 0 -- the FIRST event -- and `multi` at 0, so the first event
 * fires on the tic its own timer names. */
GObj* itPorygonMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITPorygonItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        ip->item_vars.porygon.offset = *pos;

        ip->is_allow_knockback = TRUE;

        ip->multi = 0;

        ip->event_id = 0;

#ifndef FT_HOSTTEST
        /* DIVERGES: the decomp's `attr->anim_joints`, an array of one
         * script, played by itManagerMakeItem's own gcAddAnimAll. */
        gcAddDObjAnimJoint(DObjGetStruct(item_gobj), stage_map_anim("PorygonAppear"), 0.0F);
        gcPlayAnimAll(item_gobj);
#endif
        func_800269C0_275C0(nSYAudioVoiceYamabukiPorygon);
    }
    return item_gobj;
}
