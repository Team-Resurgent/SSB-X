/* itglucky.c -- it/itground/itglucky.c, verbatim but for one script read:
 * Saffron City's Chansey and the first of the Gate's five.
 *
 * The Gate opens, a Pokémon comes out, crosses the roof and goes back in
 * (src/dc/gryamabuki.c drives all three states). Chansey is the friendly
 * one: it walks out laying EGGS -- one every ten tics through the middle
 * of its animation, `ITGLUCKY_EGG_SPAWN_COUNT` of them -- and each egg
 * is a real EGG item made through itManagerMakeItemSetupCommon, which is
 * the port's roster entry. Whether it lays
 * any at all is the ITEM SWITCH's business: the game checks
 * `item_toggles & ITEM_TOGGLE_MASK_KIND(nITKindEgg)` and the appearance
 * rate, so a match with Eggs off gets a Chansey that walks out and back
 * laying nothing.
 *
 * Its data is the stage's, the same shape as the `?` block's and the
 * Piranha Plant's (see src/dc/itpowerblock.c's header for the whole
 * argument): `ItemAttributes GLucky`, 0xBC of Saffron City's map, which
 * the item pack carries under that key.
 *
 * DIVERGES, and it is the one script: `attr->anim_joints` for this item
 * is the array `mobjlink_0x03F0` ({ NULL, AnimJoint_0x03F8 } -- one
 * script, on joint 1), which `itManagerMakeItem` would play with its own
 * gcAddAnimAll. The pack's record carries NO_PTR there (see
 * tools/export/ssb_itemexport.py's STAGE_ITEMS), so the port attaches that one
 * script by name in MakeItem, where the attach is visible. The name is
 * the pack's `GLuckyAppear`.
 *
 * THE ITEM HAS NO STATUS TABLE OF ITS OWN beyond the damaged one, and
 * its `proc_dead` returns TRUE -- a Chansey that leaves the blast line
 * is destroyed like any other item, unlike the Piranha Plant's recycle.
 */
#include <it/item.h>
#include <ft/fighter.h>
#include <gr/ground.h>
#include <gr/grcommon/gryamabuki.h> /* grYamabukiGateSetClosedWait,
                                       grYamabukiGateClearMonsterGObj */
#include <ef/effect.h>            /* efManagerDustLightMakeEffect */
#include <sc/scene.h>             /* gSCManagerBattleState */
#include <sys/objanim.h>          /* gcAddDObjAnimJoint, gcPlayAnimAll */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

#include "stage.h"                /* stage_map_anim */
#include "ftcommon.h"             /* func_800269C0_275C0 */

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/itglucky.c:11-34 dITGLuckyItemDesc, verbatim but for
 * `p_file` and the `o_attributes` spelling. `nGMAttackStateNew` -- not
 * Off: a Chansey's hitbox is live from the moment it appears, which is
 * what the gate's monster is. */
ITDesc dITGLuckyItemDesc =
{
    nITKindGLucky,                          /* Item Kind */
    /* DIVERGES: the decomp's `&gGRCommonStruct.yamabuki.item_head`. */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)0xBC,                         /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,            /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itGLuckyCommonProcUpdate,               /* Proc Update */
    NULL,                                   /* Proc Map */
    itGLuckyCommonProcHit,                  /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    itGLuckyCommonProcDamage                /* Proc Damage */
};

/* it/itground/itglucky.c:36-50 dITGLuckyStatusDescs, verbatim: ONE
 * status, the knocked-out one. Everything else about a Chansey is its
 * desc's own procs. */
ITStatusDesc dITGLuckyStatusDescs[/* */] =
{
    /* Status 0 (Neutral Damage) */
    {
        itGLuckyDamagedProcUpdate,          /* Proc Update */
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

enum itGLuckyStatus
{
    nITGLuckyStatusDamaged,
    nITGLuckyStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/itglucky.c:72-76 itGLuckyDamagedSetStatus 0x8017C240,
 * verbatim. */
void itGLuckyDamagedSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITGLuckyStatusDescs, nITGLuckyStatusDamaged);

    itGetStruct(item_gobj)->proc_dead = itGLuckyDamagedProcDead;
}

/* it/itground/itglucky.c:78-132 itGLuckyCommonUpdateEggSpawn 0x8017C280,
 * verbatim.
 *
 * THE EGG LAID IS AN ITEM, made by the game's own
 * itManagerMakeItemSetupCommon with `ITEM_FLAG_COLLPROJECT |
 * ITEM_FLAG_PARENT_ITEM` -- a parent that is an ITEM, which is the flag
 * combination the port's own test had to learn the hard way (a bare
 * COLLPROJECT reads a fighter out of an item). And the ITEM SWITCH
 * decides: the toggles' Egg bit and the appearance rate are both checked
 * before one is made, so a match with Eggs off still spends the count --
 * the else arm decrements `egg_spawn_count` and sets the same ten-tick
 * delay, and the Chansey simply walks out empty. */
void itGLuckyCommonUpdateEggSpawn(GObj *lucky_gobj)
{
    ITStruct *lucky_ip = itGetStruct(lucky_gobj);
    ITStruct *egg_ip;
    s32 unused;
    DObj *dobj = DObjGetStruct(lucky_gobj);
    GObj *egg_gobj;
    Vec3f pos;
    Vec3f vel;

    if (lucky_ip->multi == 0)
    {
        if (lucky_ip->item_vars.glucky.egg_spawn_count != 0)
        {
            if ((gSCManagerBattleState->item_toggles & ITEM_TOGGLE_MASK_KIND(nITKindEgg)) && (gSCManagerBattleState->item_appearance_rate != nSCBattleItemSwitchNone))
            {
                pos = dobj->translate.vec.f;

                pos.x -= ITGLUCKY_EGG_SPAWN_OFF_X;
                pos.y += ITGLUCKY_EGG_SPAWN_OFF_Y;

                vel.x = -((syUtilsRandFloat() * ITGLUCKY_EGG_SPAWN_MUL) + ITGLUCKY_EGG_SPAWN_ADD_X);
                vel.y = (syUtilsRandFloat() * ITGLUCKY_EGG_SPAWN_MUL) + ITGLUCKY_EGG_SPAWN_ADD_Y;
                vel.z = 0.0F;

                egg_gobj = itManagerMakeItemSetupCommon(lucky_gobj, nITKindEgg, &pos, &vel, (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_ITEM));

                if (egg_gobj != NULL)
                {
                    egg_ip = itGetStruct(egg_gobj);

                    func_800269C0_275C0(nSYAudioFGMKirbySpecialLwStart);

                    lucky_ip->multi = 10;
                    lucky_ip->item_vars.glucky.egg_spawn_count--;

                    efManagerDustLightMakeEffect(&pos, egg_ip->lr, 1.0F);
                }
            }
            else
            {
                lucky_ip->multi = 10;
                lucky_ip->item_vars.glucky.egg_spawn_count--;
            }
        }
    }
    if (lucky_ip->item_vars.glucky.egg_spawn_count != 0)
    {
        if (lucky_ip->multi > 0)
        {
            lucky_ip->multi--;
        }
    }
}

/* it/itground/itglucky.c:134-154 itGLuckyCommonProcUpdate 0x8017C400,
 * verbatim. THE WALK IS A TRANSLATION added every tic -- the appear
 * script's own TraX is the shape of the walk and
 * `item_vars.glucky.pos` is the SPEED, which is why the same vector is
 * stored in MakeItem and added here. The egg interval is read off the
 * DObj's own `anim_frame`, and the end of the animation is what closes
 * the gate again (`grYamabukiGateSetClosedWait`) and destroys the item
 * (TRUE). */
sb32 itGLuckyCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    dobj->translate.vec.f.x += ip->item_vars.glucky.pos.x;
    dobj->translate.vec.f.y += ip->item_vars.glucky.pos.y;

    if ((dobj->anim_frame >= ITGLUCKY_EGG_SPAWN_BEGIN) && (dobj->anim_frame <= ITGLUCKY_EGG_SPAWN_END))
    {
        itGLuckyCommonUpdateEggSpawn(item_gobj);
    }
    if (dobj->anim_wait == AOBJ_ANIM_NULL)
    {
        grYamabukiGateSetClosedWait();

        return TRUE;
    }
    else return FALSE;
}

/* it/itground/itglucky.c:156-164 itGLuckyCommonProcHit 0x8017C4AC,
 * verbatim: being hit takes its hitbox off, and nothing else -- Chansey
 * is not knocked back until the damage is heavy enough, which is
 * CommonProcDamage below. */
sb32 itGLuckyCommonProcHit(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->attack_coll.attack_state = nGMAttackStateOff;

    return FALSE;
}

/* it/itground/itglucky.c:166-179 itGLuckyDamagedProcUpdate 0x8017C4BC,
 * verbatim: a knocked-out Chansey falls and spins, and its facing
 * (`ip->lr`) is what decides which way. */
sb32 itGLuckyDamagedProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj;

    itMainApplyGravityClampTVel(ip, ITGLUCKY_GRAVITY, ITGLUCKY_TVEL);

    dobj = DObjGetStruct(item_gobj);

    dobj->rotate.vec.f.z -= ITGLUCKY_HIT_ROTATE_Z * ip->lr;

    return FALSE;
}

/* it/itground/itglucky.c:181-185 itGLuckyDamagedProcDead 0x8017C524,
 * verbatim, and it is one line: TRUE. A Chansey that leaves the blast
 * line is destroyed -- it is NOT the Piranha Plant, which recycles. */
sb32 itGLuckyDamagedProcDead(GObj *item_gobj)
{
    return TRUE;
}

/* it/itground/itglucky.c:187-209 itGLuckyCommonProcDamage 0x8017C530,
 * verbatim. Past the threshold a Chansey is thrown like any fighter --
 * the same knockback-angle arithmetic the Piranha Plant's own damage
 * proc uses -- its animation is stopped, the GATE is told it has no
 * monster any more (which is what lets it close), and it enters the
 * damaged status. */
sb32 itGLuckyCommonProcDamage(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if (ip->damage_knockback >= ITGLUCKY_NDAMAGE_KNOCKBACK_MIN)
    {
        f32 angle = ftCommonDamageGetKnockbackAngle(ip->damage_angle, ip->ga, ip->damage_knockback);

        ip->physics.vel_air.x = (__cosf(angle) * ip->damage_knockback * -ip->damage_lr);
        ip->physics.vel_air.y = (__sinf(angle) * ip->damage_knockback);

        ip->attack_coll.attack_state = nGMAttackStateOff;
        ip->damage_coll.hitstatus = nGMHitStatusNone;

        dobj->anim_wait = AOBJ_ANIM_NULL;

        grYamabukiGateClearMonsterGObj();
        itGLuckyDamagedSetStatus(item_gobj);
    }
    return FALSE;
}

/* it/itground/itglucky.c:211-232 itGLuckyMakeItem 0x8017C5F4, verbatim
 * but for the one script attach (see this file's header).
 *
 * `pos` is kept as the WALK SPEED as well as the spawn position -- the
 * Gate spawns a monster with a zero velocity, so a Chansey walks in
 * place until CommonProcUpdate adds nothing; what actually moves it is
 * the appear script, and this vector is what the gate's own position
 * arithmetic is measured against. */
GObj* itGLuckyMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITGLuckyItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        ip->attack_coll.interact_mask = GMHITCOLLISION_FLAG_FIGHTER;

        ip->item_vars.glucky.pos = *pos;

        ip->is_allow_knockback = TRUE;

        ip->multi = 0;

        ip->item_vars.glucky.egg_spawn_count = ITGLUCKY_EGG_SPAWN_COUNT;

#ifndef FT_HOSTTEST
        /* DIVERGES: the decomp's `attr->anim_joints`, an array of one
         * script on joint 1 (mobjlink_0x03F0), played by
         * itManagerMakeItem's own gcAddAnimAll. The pack names it. */
        gcAddDObjAnimJoint(DObjGetStruct(item_gobj), stage_map_anim("GLuckyAppear"), 0.0F);
        gcPlayAnimAll(item_gobj);
#endif
        func_800269C0_275C0(nSYAudioVoiceYamabukiLucky);
    }
    return item_gobj;
}
