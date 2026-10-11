/* itfushigibana.c -- it/itground/itfushigibana.c, verbatim but for the two
 * table reads: Saffron City's Venusaur.
 *
 * The last of the Gate's five, and the one with the most machinery: it
 * walks the same walk the other four do, runs the same two-entry
 * `ITMonsterEvent` hitbox machine Porygon's does, opens its mouth through
 * its MObj's `texture_id_curr` the way Charmander's does -- and every
 * ITFUSHIGIBANA_RAZOR_SPAWN_WAIT tics it spits a RAZOR, which is a real
 * weapon with a real model that flies, curves, and slices.
 *
 * THE RAZOR IS DIFFERENT FROM THE FLAME, and that is the point of it: the
 * Charmander's flame has no `data` at all and is drawn entirely by
 * particles, while the razor's `WPAttributes.data` names a DObjDesc tree
 * whose body entry is a DLLink -- so it is a WEAPON THIS PORT BAKES
 * (romdisk/wpfushigibanarazor.mdl, out of the stage's model file) and
 * draws as a tree. Its Proc Hop and Proc Reflector both aim it: the
 * razor's z rotation is set from its own velocity every bounce, which is
 * what makes a reflected razor turn to fly the other way.
 *
 * `WEAPON_FLAG_PARENT_ITEM` again -- the item is the parent, and the razor
 * carries the item's team.
 *
 * Its data is the stage's, as the other four's: `ItemAttributes
 * Fushigibana`, 0x278 of Saffron City's map.
 *
 * DIVERGES, three, and the first two are the ones every Gate monster
 * makes:
 *
 *  1. THE ATTRIBUTES. The decomp's `&gGRCommonStruct.yamabuki.item_head`;
 *     the port's `&gITManagerCommonData`, keyed 0x278.
 *  2. THE MONSTER EVENTS. `itGetMonsterEvent(desc, off)` is
 *     `*desc.p_file + &llGRYamabukiMapFushigibanaHitParties` -- 0x2C0 of
 *     the map -- and the ROM's `ITMonsterEvent` is BITFIELDS. The pack
 *     carries the table PARSED (its MONSTER EVENTS section), keyed by the
 *     same `o_attributes` this desc carries.
 *  3. THE WEAPON'S ATTRIBUTES. Same shape as Charmander's, and the same
 *     answer: `dITFushigibanaWeaponRazorWeaponDesc.p_weapon` is the stage
 *     file and its `o_attributes` names
 *     `dGRYamabukiMap_FushigibanaRazor_WeaponAttributes`, 0x308, which the
 *     pack carries in its WEAPON ATTRIBUTES section. Its `data` is real,
 *     so the pack writes a non-NULL marker there and the port's own weapon
 *     table (src/dc/wpmanager.c, keyed by this WPDesc) names the baked
 *     model -- which is the one thing here a fighter's weapon never needs.
 */
#include <it/item.h>
#include <wp/weapon.h>
#include <ft/fighter.h>
#include <gr/ground.h>
#include <gr/grcommon/gryamabuki.h> /* grYamabukiGateSetClosedWait,
                                       dGRYamabukiMonsterAttackKind */
#include <ef/effect.h>            /* efManagerDustLightMakeEffect,
                                     efManagerDustCollideMakeEffect,
                                     efManagerDamageSlashMakeEffect */
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

/* it/itground/itfushigibana.c:25-49 dITFushigibanaItemDesc, verbatim but
 * for `p_file` and the `o_attributes` spelling. */
ITDesc dITFushigibanaItemDesc =
{
    nITKindFushigibana,                     /* Item Kind */
    /* DIVERGES: the decomp's `&gGRCommonStruct.yamabuki.item_head`. */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)0x278,                        /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,            /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itFushigibanaCommonProcUpdate,          /* Proc Update */
    NULL,                                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itground/itfushigibana.c:51-75 dITFushigibanaWeaponRazorWeaponDesc,
 * verbatim but for `p_weapon` (see this file's header). `Render flags` 0x03
 * is WEAPON_FLAG_DOBJDESC | WEAPON_FLAG_DOBJLINKS -- a DObjDesc TREE whose
 * display lists are DL links, which is what `DLLink_0x2A38` in its body
 * entry is. And note Proc Hit is also Proc Shield, Proc Set-Off AND Proc
 * Absorb: a razor blocked or absorbed still slices. */
WPDesc dITFushigibanaWeaponRazorWeaponDesc =
{
    0x03,                                   /* Render flags? */
    nWPKindFushigibanaRazor,                /* Weapon Kind */
    /* DIVERGES: the decomp's `&gGRCommonStruct.yamabuki.item_head`. */
    &gITManagerCommonData,                  /* Pointer to item's loaded files? */
    (intptr_t)0x308,                        /* Offset of weapon attributes in loaded files */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyRSca,         /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    itFushigibanaWeaponRazorProcUpdate,     /* Proc Update */
    NULL,                                   /* Proc Map */
    itFushigibanaWeaponRazorProcHit,        /* Proc Hit */
    itFushigibanaWeaponRazorProcHit,        /* Proc Shield */
    itFushigibanaWeaponRazorProcHop,        /* Proc Hop */
    itFushigibanaWeaponRazorProcHit,        /* Proc Set-Off */
    itFushigibanaWeaponRazorProcReflector,  /* Proc Reflector */
    itFushigibanaWeaponRazorProcHit         /* Proc Absorb */
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/itfushigibana.c:77-124 itFushigibanaCommonUpdateMonsterEvent
 * 0x80184440, verbatim but for where the events come from.
 *
 * Porygon's machine exactly -- `multi` tic-counted against the armed
 * event's timer, the hitbox installed when they match, `event_id`
 * advanced, and 2 wrapping back to 1 -- plus the shake-stop dust at
 * ITFUSHIGIBANA_RETURN_WAIT, which is where the Venusaur stops rocking and
 * plants itself. */
void itFushigibanaCommonUpdateMonsterEvent(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    ITItemEvent ev;

    if (itemPackMonsterEvent((intptr_t)0x278, (u32)ip->event_id, &ev) != 0)
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

    if (ip->multi == ITFUSHIGIBANA_RETURN_WAIT)
    {
        Vec3f pos = DObjGetStruct(item_gobj)->translate.vec.f;

        pos.y = 0.0F;

        efManagerDustLightMakeEffect(&pos, -1, 1.0F);
    }
}

/* it/itground/itfushigibana.c:126-172 itFushigibanaCommonProcUpdate
 * 0x801845B4, verbatim.
 *
 * THE SPAWN TIMER HERE COUNTS DOWN ON THE SAME TIC IT IS SEEDED, unlike
 * Charmander's -- the `if (razor_spawn_wait > 0)` is a separate statement
 * after the spawn, so a razor fires, the wait is set and immediately
 * decremented in one call. The effect and the roar are the spawn's own:
 * `efManagerDustCollideMakeEffect` at the spawn point is the puff of the
 * razor leaving, and it is made only on the tic one is actually fired. */
sb32 itFushigibanaCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    Vec3f pos;

    dobj->translate.vec.f.x += ip->item_vars.fushigibana.offset.x;
    dobj->translate.vec.f.y += ip->item_vars.fushigibana.offset.y;

    itFushigibanaCommonUpdateMonsterEvent(item_gobj);

    pos = dobj->translate.vec.f;

    pos.x += ITFUSHIGIBANA_RAZOR_SPAWN_OFF_X;

    if
    (
        (ip->item_vars.fushigibana.flags == GRYAMABUKI_MONSTER_WEAPON_INSTANT)                                                     ||
        ((ip->item_vars.fushigibana.flags & GRYAMABUKI_MONSTER_WEAPON_WAIT) && (dobj->anim_frame >= ITFUSHIGIBANA_RAZOR_SPAWN_BEGIN)) &&
        (dobj->anim_frame <= ITFUSHIGIBANA_RAZOR_SPAWN_END)
    )
    {
        dobj->mobj->texture_id_curr = 1;

        if (!ip->item_vars.fushigibana.razor_spawn_wait)
        {
            itFushigibanaWeaponRazorMakeWeapon(item_gobj, &pos);

            ip->item_vars.fushigibana.razor_spawn_wait = ITFUSHIGIBANA_RAZOR_SPAWN_WAIT;

            func_800269C0_275C0(nSYAudioFGMMonsterShoot);

            efManagerDustCollideMakeEffect(&pos);
        }
        if (ip->item_vars.fushigibana.razor_spawn_wait > 0)
        {
            ip->item_vars.fushigibana.razor_spawn_wait--;
        }
    }
    else dobj->mobj->texture_id_curr = 0;

    if (dobj->anim_wait == AOBJ_ANIM_NULL)
    {
        grYamabukiGateSetClosedWait();

        return TRUE;
    }
    return FALSE;
}

/* it/itground/itfushigibana.c:174-217 itFushigibanaMakeItem 0x8018470C,
 * verbatim but for the appear script's attach (see this file's header).
 *
 * The Gate's attack-mode draw again -- see itHitokageMakeItem -- and note
 * this one ALSO clears `event_id` and `multi` first, because unlike the
 * Charmander it runs the monster-event machine. */
GObj* itFushigibanaMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITFushigibanaItemDesc, pos, vel, flags);
    s32 unused;
    DObj *dobj;
    ITStruct *ip;

    if (item_gobj != NULL)
    {
        ip = itGetStruct(item_gobj);
        dobj = DObjGetStruct(item_gobj);

        ip->event_id = 0;

        ip->multi = 0;

        ip->item_vars.fushigibana.razor_spawn_wait = 0;
        ip->item_vars.fushigibana.offset = *pos;

        ip->is_allow_knockback = TRUE;

        ip->item_vars.fushigibana.flags = syUtilsRandIntRange(GRYAMABUKI_MONSTER_WEAPON_MAX);

        if ((dGRYamabukiMonsterAttackKind == ip->item_vars.fushigibana.flags) || (ip->item_vars.fushigibana.flags & dGRYamabukiMonsterAttackKind))
        {
            ip->item_vars.fushigibana.flags++;

            ip->item_vars.fushigibana.flags %= GRYAMABUKI_MONSTER_WEAPON_MAX;
        }
        if (ip->item_vars.fushigibana.flags == GRYAMABUKI_MONSTER_WEAPON_INSTANT)
        {
            dobj->mobj->texture_id_curr = 1;
        }
        dGRYamabukiMonsterAttackKind = ip->item_vars.fushigibana.flags;

#ifndef FT_HOSTTEST
        /* DIVERGES: the decomp's `attr->anim_joints`, the second entry of
         * file 159's mobjlink_0x23D0, played by itManagerMakeItem's own
         * gcAddAnimAll. The pack names it. */
        gcAddDObjAnimJoint(dobj, stage_map_anim("FushigibanaAppear"), 0.0F);
        gcPlayAnimAll(item_gobj);
#endif
        func_800269C0_275C0(nSYAudioVoiceYamabukiFushigibana);
    }
    return item_gobj;
}

/* it/itground/itfushigibana.c:219-231 itFushigibanaWeaponRazorProcUpdate
 * 0x80184820, verbatim: the razor ACCELERATES along x, in its own facing,
 * every tic -- which is why it curves: the acceleration is applied on top
 * of whatever velocity it has, so a hop or a reflection changes the shape
 * of everything after it. */
sb32 itFushigibanaWeaponRazorProcUpdate(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    wp->physics.vel_air.x += ITFUSHIGIBANA_RAZOR_ADD_VEL_X * wp->lr;

    if (wpMainDecLifeCheckExpire(wp) != FALSE)
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itground/itfushigibanaWeaponRazorProcHit 0x80184874, verbatim: a slash
 * spark and the razor is CONSUMED (TRUE), unlike the flame which survives
 * what it hits. The spark's size is the razor's own damage and its
 * direction the razor's facing, so a 3% razor and a 20% one look
 * different. */
sb32 itFushigibanaWeaponRazorProcHit(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    efManagerDamageSlashMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f, wp->attack_coll.damage, wp->lr);

    return TRUE;
}

/* it/itground/itfushigibana.c:233-255 itFushigibanaWeaponRazorProcHop
 * 0x801848BC, verbatim.
 *
 * THE BOUNCE AIMS IT. The velocity is rotated about the shield collision
 * normal, the z rotation is set from the NEW velocity's angle, and the
 * facing follows the sign of x -- so a razor that bounces off a shield
 * leaves pointing the way it is going, and the x scale is reset to 1
 * because a mirrored razor is drawn by its rotation, not by a negative
 * scale. */
sb32 itFushigibanaWeaponRazorProcHop(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    syVectorRotateAbout3D(&wp->physics.vel_air, &wp->shield_collide_dir, wp->shield_collide_angle * 2);

    DObjGetStruct(weapon_gobj)->rotate.vec.f.z = syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x) + F_CLC_DTOR32(180.0F);
    DObjGetStruct(weapon_gobj)->scale.vec.f.x = 1.0F;

    if (wp->physics.vel_air.x > 0.0F)
    {
        wp->lr = +1;
    }
    else wp->lr = -1;

    return FALSE;
}

/* it/itground/itfushigibana.c:257-276 itFushigibanaWeaponRazorProcReflector
 * 0x80184970, verbatim: a reflector turns it to face its new owner and
 * flips its facing -- the same aim as the hop, and the flip is what sends
 * it back the way it came. */
sb32 itFushigibanaWeaponRazorProcReflector(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    FTStruct *fp = ftGetStruct(wp->owner_gobj);

    wpMainReflectorSetLR(wp, fp);

    DObjGetStruct(weapon_gobj)->rotate.vec.f.z = syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x) + F_CLC_DTOR32(180.0F);
    DObjGetStruct(weapon_gobj)->scale.vec.f.x = 1.0F;

    wp->lr = -wp->lr;

    return FALSE;
}

/* it/itground/itfushigibana.c:278-299 itFushigibanaWeaponRazorMakeWeapon
 * 0x801849EC, verbatim. The razor is placed at the spawn point rather than
 * offset from the item's own translate -- `pos` already carries
 * ITFUSHIGIBANA_RAZOR_SPAWN_OFF_X -- and its velocity is the constant
 * forward speed, which ProcUpdate then accelerates. */
GObj* itFushigibanaWeaponRazorMakeWeapon(GObj *item_gobj, Vec3f *pos)
{
    GObj *weapon_gobj = wpManagerMakeWeapon(item_gobj, &dITFushigibanaWeaponRazorWeaponDesc, pos, WEAPON_FLAG_PARENT_ITEM);
    DObj *dobj;
    WPStruct *wp;

    if (weapon_gobj == NULL)
    {
        return NULL;
    }
    wp = wpGetStruct(weapon_gobj);

    wp->lr = -1;

    wp->physics.vel_air.x = ITFUSHIGIBANA_RAZOR_VEL_X;

    dobj = DObjGetStruct(weapon_gobj);

    dobj->translate.vec.f = *pos;

    wp->lifetime = ITFUSHIGIBANA_RAZOR_LIFETIME;

    return weapon_gobj;
}
