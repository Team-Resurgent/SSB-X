/* ithitokage.c -- it/itground/ithitokage.c, verbatim but for the two
 * table reads: Saffron City's Charmander.
 *
 * Charmander crosses the roof breathing fire: for the middle of its
 * appear script (frames 40 to 120) it opens its mouth -- which is the
 * MObj's `texture_id_curr` flipped to 1, an open-mouthed texture where the
 * closed one was -- and every ITHITOKAGE_FLAME_SPAWN_WAIT tics it spits a
 * FLAME WEAPON forward and down (45 units at -12 degrees), each living
 * ITHITOKAGE_FLAME_LIFETIME tics.
 *
 * WHICH WAY IT BREATHES IS NOT ITS OWN CHOICE. `dGRYamabukiMonsterAttackKind`
 * is the Gate's memory of what the LAST monster did, and Charmander draws
 * a mode that differs from it -- `GRYAMABUKI_MONSTER_WEAPON_INSTANT` (fire
 * from the first frame), `_WAIT` (only inside the 40..120 window) or
 * `_ALL` (both) -- so two Charmanders in a row do not breathe alike. That
 * global is the Gate's (`src/dc/gryamabuki.c`), and this file is its only
 * other reader.
 *
 * Its data is the stage's, the same shape as the Gate's other monsters
 * (see src/dc/itporygon.c's header): `ItemAttributes Hitokage`, 0x1FC of
 * Saffron City's map, which the item pack carries under that key.
 *
 * DIVERGES, two, and they are the same two the other monsters make:
 *
 *  1. THE ATTRIBUTES. The decomp's `&gGRCommonStruct.yamabuki.item_head`;
 *     the port's `&gITManagerCommonData`, keyed 0x1FC.
 *  2. THE WEAPON'S ATTRIBUTES. `dITHitokageWeaponFlameWeaponDesc.p_weapon`
 *     is the same stage file and its `o_attributes` names
 *     `dGRYamabukiMap_HitokageFlame_WeaponAttributes`, 0x244 of the map --
 *     a `WPAttributes`, which wpManagerMakeWeapon reads. The port carries
 *     it in the item pack's WEAPON ATTRIBUTES section under that same
 *     number, and `wpManagerMakeWeapon` consults `itemPackWeaponAttr`
 *     before falling back to the byte overlay a fighter's own weapon
 *     still uses.
 *
 * The appear script is attached by name in MakeItem, as the other three
 * monsters' are.
 */
#include <it/item.h>
#include <wp/weapon.h>
#include <ft/fighter.h>
#include <gr/ground.h>
#include <gr/grcommon/gryamabuki.h> /* grYamabukiGateSetClosedWait,
                                       grYamabukiGateClearMonsterGObj,
                                       dGRYamabukiMonsterAttackKind */
#include <ef/effect.h>            /* efManagerDustExpandSmallMakeEffect,
                                     efManagerSparkleWhiteMakeEffect */
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

/* it/itground/ithitokage.c:12-36 dITHitokageItemDesc, verbatim but for
 * `p_file` and the `o_attributes` spelling. */
ITDesc dITHitokageItemDesc =
{
    nITKindHitokage,                        /* Item Kind */
    /* DIVERGES: the decomp's `&gGRCommonStruct.yamabuki.item_head`. */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)0x1FC,                        /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,            /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itHitokageCommonProcUpdate,             /* Proc Update */
    NULL,                                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    itHitokageCommonProcDamage              /* Proc Damage */
};

/* it/itground/ithitokage.c:38-52 dITHitokageStatusDescs, verbatim: ONE
 * status, the knocked-out one. */
ITStatusDesc dITHitokageStatusDescs[/* */] =
{
    /* Status 0 (Neutral Damage) */
    {
        itHitokageDamagedProcUpdate,        /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    }
};

/* it/itground/ithitokage.c:54-76 dITHitokageWeaponFlameWeaponDesc,
 * verbatim but for `p_weapon` (see this file's header). Note the third
 * transform kind the flame asks for -- `nGCMatrixKindTraRotRpyRSca` --
 * and that its Proc Hit is ALSO its Proc Shield and Proc Set-Off: a flame
 * that is blocked, clanged with or set off behaves the same way. */
WPDesc dITHitokageWeaponFlameWeaponDesc =
{
    0x00,                                   /* Render flags? */
    nWPKindHitokageFlame,                   /* Weapon Kind */
    /* DIVERGES: the decomp's `&gGRCommonStruct.yamabuki.item_head`. */
    &gITManagerCommonData,                  /* Pointer to character's loaded files? */
    (intptr_t)0x244,                        /* Offset of weapon attributes in loaded files */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyRSca,         /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    itHitokageWeaponFlameProcUpdate,        /* Proc Update */
    itHitokageWeaponFlameProcMap,           /* Proc Map */
    itHitokageWeaponFlameProcHit,           /* Proc Hit */
    itHitokageWeaponFlameProcHit,           /* Proc Shield */
    NULL,                                   /* Proc Hop */
    itHitokageWeaponFlameProcHit,           /* Proc Set-Off */
    itHitokageWeaponFlameProcReflector,     /* Proc Reflector */
    NULL                                    /* Proc Absorb */
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itHitokageStatus
{
    itHitokageStatusDamaged,
    itHitokageStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/ithitokage.c:84-89 itHitokageDamagedSetStatus 0x80183DA0,
 * verbatim. */
void itHitokageDamagedSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITHitokageStatusDescs, itHitokageStatusDamaged);

    itGetStruct(item_gobj)->proc_dead = itHitokageDamagedProcDead;
}

/* it/itground/ithitokage.c:91-139 itHitokageCommonProcUpdate 0x80183DE0,
 * verbatim.
 *
 * THE MOUTH IS THE MOBJ'S TEXTURE. `texture_id_curr` is 1 for exactly the
 * frames the flame can be spat and 0 otherwise -- outside that window the
 * closed-mouth palette is wound back on every tic -- and it is set BEFORE
 * the spawn check, so the mouth opens on the frame the flame is due rather
 * than a frame later.
 *
 * The `flags` test is the Gate's memory, and it is two tests in one
 * expression: `_INSTANT` fires from frame 0, `_WAIT`/`_ALL` fire inside
 * the window, and `_ALL` satisfies both. A mode of `_NONE` would fire
 * never, which is why the Gate's own draw never picks it. */
sb32 itHitokageCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    Vec3f pos;

    dobj->translate.vec.f.x += ip->item_vars.hitokage.offset.x;
    dobj->translate.vec.f.y += ip->item_vars.hitokage.offset.y;

    pos = dobj->translate.vec.f;

    pos.x += ITHITOKAGE_FLAME_SPAWN_OFF_X;

    if
    (
        (ip->item_vars.hitokage.flags == GRYAMABUKI_MONSTER_WEAPON_INSTANT)                                                  ||
        ((ip->item_vars.hitokage.flags & GRYAMABUKI_MONSTER_WEAPON_WAIT) && (dobj->anim_frame >= ITHITOKAGE_FLAME_SPAWN_BEGIN)) &&
        (dobj->anim_frame <= ITHITOKAGE_FLAME_SPAWN_END)
    )
    {
        dobj->mobj->texture_id_curr = 1;

        if (ip->item_vars.hitokage.flame_spawn_wait <= 0)
        {
            itHitokageCommonMakeFlame(item_gobj, &pos);

            ip->item_vars.hitokage.flame_spawn_wait = ITHITOKAGE_FLAME_SPAWN_WAIT;
        }
        else ip->item_vars.hitokage.flame_spawn_wait--;
    }
    else dobj->mobj->texture_id_curr = 0;

    if (dobj->anim_wait == AOBJ_ANIM_NULL)
    {
        grYamabukiGateSetClosedWait();

        return TRUE;
    }
    return FALSE;
}

/* it/itground/ithitokage.c:141-155 itHitokageDamagedProcUpdate 0x80183F20,
 * verbatim: gravity and a spin whose direction is the item's facing. */
sb32 itHitokageDamagedProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj;

    itMainApplyGravityClampTVel(ip, ITHITOKAGE_GRAVITY, ITHITOKAGE_TVEL);

    dobj = DObjGetStruct(item_gobj);

    dobj->rotate.vec.f.z -= (ITHITOKAGE_HIT_ROTATE_Z * ip->lr);

    return FALSE;
}

/* it/itground/ithitokage.c:157-161 itHitokageDamagedProcDead 0x80183F88,
 * verbatim, and it is one line: TRUE. */
sb32 itHitokageDamagedProcDead(GObj *item_gobj)
{
    return TRUE;
}

/* it/itground/ithitokage.c:163-189 itHitokageCommonProcDamage 0x80183F94,
 * verbatim -- the same tail GLucky's has: past the threshold the Charmander
 * is thrown, its animation is stopped, the Gate is told it has no monster,
 * and it enters the damaged status. */
sb32 itHitokageCommonProcDamage(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if (ip->damage_knockback >= ITHITOKAGE_NDAMAGE_KNOCKBACK_MIN)
    {
        f32 angle = ftCommonDamageGetKnockbackAngle(ip->damage_angle, ip->ga, ip->damage_knockback);

        ip->physics.vel_air.x = __cosf(angle) * ip->damage_knockback * -ip->damage_lr;
        ip->physics.vel_air.y = __sinf(angle) * ip->damage_knockback;

        ip->attack_coll.attack_state = nGMAttackStateOff;
        ip->damage_coll.hitstatus = nGMHitStatusNone;

        dobj->anim_wait = AOBJ_ANIM_NULL;

        grYamabukiGateClearMonsterGObj();
        itHitokageDamagedSetStatus(item_gobj);
    }
    return FALSE;
}

/* it/itground/ithitokage.c:191-224 itHitokageMakeItem 0x80184058, verbatim
 * but for the appear script's attach (see this file's header).
 *
 * The DRAW here is the Gate's: `syUtilsRandIntRange(GRYAMABUKI_MONSTER_WEAPON_MAX)`
 * is 0..3, and a draw that repeats or is contained by the last monster's
 * mode is stepped forward one and wrapped -- so the new Charmander's mode
 * is always DIFFERENT from the previous monster's. `_INSTANT` also opens
 * the mouth immediately, because its flame starts on frame 1. */
GObj* itHitokageMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITHitokageItemDesc, pos, vel, flags);
    s32 unused;
    DObj *dobj;
    ITStruct *ip;

    if (item_gobj != NULL)
    {
        ip = itGetStruct(item_gobj);
        dobj = DObjGetStruct(item_gobj);

        ip->item_vars.hitokage.flame_spawn_wait = 0;
        ip->item_vars.hitokage.offset = *pos;

        ip->is_allow_knockback = TRUE;

        ip->item_vars.hitokage.flags = syUtilsRandIntRange(GRYAMABUKI_MONSTER_WEAPON_MAX);

        if ((dGRYamabukiMonsterAttackKind == ip->item_vars.hitokage.flags) || (ip->item_vars.hitokage.flags & dGRYamabukiMonsterAttackKind))
        {
            ip->item_vars.hitokage.flags++;

            ip->item_vars.hitokage.flags %= GRYAMABUKI_MONSTER_WEAPON_MAX;
        }
        if (ip->item_vars.hitokage.flags == GRYAMABUKI_MONSTER_WEAPON_INSTANT)
        {
            dobj->mobj->texture_id_curr = 1;
        }
        dGRYamabukiMonsterAttackKind = ip->item_vars.hitokage.flags;

#ifndef FT_HOSTTEST
        /* DIVERGES: the decomp's `attr->anim_joints`, the second entry of
         * file 159's mobjlink_0x1A20, played by itManagerMakeItem's own
         * gcAddAnimAll. The pack names it. */
        gcAddDObjAnimJoint(dobj, stage_map_anim("HitokageAppear"), 0.0F);
        gcPlayAnimAll(item_gobj);
#endif
        func_800269C0_275C0(nSYAudioVoiceYamabukiHitokage);
    }
    return item_gobj;
}

/* it/itground/ithitokage.c:226-234 itHitokageWeaponFlameProcUpdate
 * 0x8018415C, verbatim: the flame's whole life is its timer. */
sb32 itHitokageWeaponFlameProcUpdate(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    if (wpMainDecLifeCheckExpire(wp) != FALSE)
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itground/ithitokage.c:236-246 itHitokageWeaponFlameProcMap
 * 0x80184188, verbatim: a flame that hits the floor puffs and dies. */
sb32 itHitokageWeaponFlameProcMap(GObj *weapon_gobj)
{
    if (wpMapTestAllCheckCollEnd(weapon_gobj) != FALSE)
    {
        efManagerDustExpandSmallMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f, 1.0F);

        return TRUE;
    }
    else return FALSE;
}

/* it/itground/ithitokage.c:248-254 itHitokageWeaponFlameProcHit
 * 0x801841CC, verbatim: a spark and a small explosion sound, and the flame
 * SURVIVES (FALSE) -- it is not consumed by what it hits. */
sb32 itHitokageWeaponFlameProcHit(GObj *weapon_gobj)
{
    func_800269C0_275C0(nSYAudioFGMExplodeS);
    efManagerSparkleWhiteMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f);

    return FALSE;
}

/* it/itground/ithitokage.c:256-274 itHitokageWeaponFlameProcReflector
 * 0x80184204, verbatim: a reflected flame turns to face its new owner and
 * gets its life back, and the two particles are the puff of it turning. */
sb32 itHitokageWeaponFlameProcReflector(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    FTStruct *fp = ftGetStruct(wp->owner_gobj);
    Vec3f *translate;

    wp->lifetime = ITHITOKAGE_FLAME_LIFETIME;

    wpMainReflectorSetLR(wp, fp);

    translate = &DObjGetStruct(weapon_gobj)->translate.vec.f;

    lbParticleMakePosVel(gITManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 2, translate->x, translate->y, 0.0F, wp->physics.vel_air.x, wp->physics.vel_air.y, 0.0F);
    lbParticleMakePosVel(gITManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0, translate->x, translate->y, 0.0F, wp->physics.vel_air.x, wp->physics.vel_air.y, 0.0F);

    return FALSE;
}

/* it/itground/ithitokage.c:276-298 itHitokageWeaponFlameMakeWeapon
 * 0x801842C8, verbatim.
 *
 * `parent_gobj` is the ITEM, so `WEAPON_FLAG_PARENT_ITEM` -- the flag
 * combination whose ownership rules come from an item rather than a
 * fighter (like Chansey's eggs). `wp->lr` is
 * forced to -1: the flame always flies the way the Gate faces. */
GObj* itHitokageWeaponFlameMakeWeapon(GObj *item_gobj, Vec3f *pos, Vec3f *vel)
{
    GObj *weapon_gobj = wpManagerMakeWeapon(item_gobj, &dITHitokageWeaponFlameWeaponDesc, pos, WEAPON_FLAG_PARENT_ITEM);
    WPStruct *wp;

    if (weapon_gobj == NULL)
    {
        return NULL;
    }
    wp = wpGetStruct(weapon_gobj);

    wp->physics.vel_air = *vel;

    wp->lifetime = ITHITOKAGE_FLAME_LIFETIME;

    wp->lr = -1;

    lbParticleMakePosVel(gITManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 2, pos->x, pos->y, 0.0F, wp->physics.vel_air.x, wp->physics.vel_air.y, 0.0F);
    lbParticleMakePosVel(gITManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0, pos->x, pos->y, 0.0F, wp->physics.vel_air.x, wp->physics.vel_air.y, 0.0F);

    return weapon_gobj;
}

/* it/itground/ithitokage.c:300-311 itHitokageCommonMakeFlame 0x801843C4,
 * verbatim: the launch vector -- 45 units at -12 degrees, which is forward
 * and slightly DOWN, the way a flame falls as it is spat -- and the roar. */
void itHitokageCommonMakeFlame(GObj *item_gobj, Vec3f *pos)
{
    ITStruct *ip;
    Vec3f vel;

    vel.x = __cosf(ITHITOKAGE_FLAME_SPAWN_ANGLE) * -ITHITOKAGE_FLAME_VEL_BASE;
    vel.y = __sinf(ITHITOKAGE_FLAME_SPAWN_ANGLE) * ITHITOKAGE_FLAME_VEL_BASE;
    vel.z = 0.0F;

    itHitokageWeaponFlameMakeWeapon(item_gobj, pos, &vel);

    func_800269C0_275C0(nSYAudioFGMLizardonFlame);
}
