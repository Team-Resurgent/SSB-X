/* itlizardon.c -- it/itmonster/itlizardon.c, verbatim: Charizard's
 * `ITDesc`, its three-row `ITStatusDesc` table (unused fall / fall /
 * attack) and their proc bodies, plus the Flame weapon's `WPDesc` and its
 * five procs. Function-for-function against the game's own ITStruct and
 * WPStruct (it/ittypes.h, wp/wptypes.h); every function names its decomp
 * line range.
 *
 * Charizard, the eleventh monster, is the one that PACES:
 *
 *  - `nITLizardonStatusFall` drops it to the floor and, on the landing
 *    frame, calls the attack status AND its init VARS in the same breath
 *    -- which is why the fall's proc_map calls two functions where every
 *    other monster's calls one.
 *  - `nITLizardonStatusAttack` walks: it fires a Flame every
 *    ITLIZARDON_FLAME_SPAWN_WAIT frames from a point offset by its own
 *    facing, and every ITLIZARDON_TURN_WAIT frames it FLIPS `lr`, emits an
 *    efManagerDustHeavyMakeEffect at its feet on the new side, and -- only
 *    when the summoner is Pippi -- turns the model 180 degrees. The turn
 *    is where the pacing lives: the flames come out of whichever side it
 *    is facing, so the pattern sweeps.
 *  - The lifetime is set by the COMMON update, not the attack: the rise
 *    writes `multi = ITLIZARDON_LIFETIME` on its way into the fall, which
 *    is the reverse of the other twelve.
 *  - `itLizardonAttackInitVars` hangs BOTH a joint's AnimJoint and a
 *    MObj's MatAnimJoint, and it reaches them through the decomp's
 *    two-step form -- `addr = attr->data - LizardonDataStart` and then
 *    `lbRelocGetFileData(..., addr, LizardonAnimJoint)` -- which is
 *    `itGetPData(ip, A, B)` written out. The port uses the macro.
 *
 * ONE THING IS NOT HERE AND IT IS THE MODEL. `dITLizardonWeaponFlameWeaponDesc`
 * is flags 0x00 -- one display list bound to one DObj, the fireball's and
 * the blaster's shape -- but its attributes carry NO relocation word at
 * their `data` site, so there is nothing for the exporter to bake and
 * `wpwarkrock.mdl`'s neighbours have no Lizardon entry: the flame's model
 * is a display list the descriptions never name as a block of its own.
 * The flame still LOOKS like a flame, because its two
 * `lbParticleMakePosVel` calls (scripts 2 and 0 of the item bank) are what
 * it is really drawn with -- so this is a missing quad, not a missing
 * effect. It is the third entry in tools/export/ssb_itemmodelexport.py's
 * KNOWN_UNBAKED and the only one whose item still works.
 *
 * DIVERGES: the two descriptors' offset fields are numbers rather than the
 * decomp's `&llITCommonData...` symbols -- see src/dc/itemoffsets.h -- and
 * the `itGetPData`/`itGetMonsterAnimNode` calls drop their `&`. No
 * function body diverges; the decomp's own `itLizardonFallUnused` "Unused"
 * marker is kept, including its `sb32` declaration with no return -- see
 * that function's own note.
 */
#include <it/item.h>
#include <wp/weapon.h>
#include <ft/fighter.h>
#include <ef/efmanager.h>       /* efManagerDustHeavyMakeEffect,
                                 * efManagerDustExpandSmallMakeEffect,
                                 * efManagerSparkleWhiteMakeEffect */
#include <lb/lbparticle.h>      /* lbParticleMakePosVel */

#include "itmonster.h"          /* itGetMonsterAnimNode */
#include "itemoffsets.h"        /* llITCommonData* offsets */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

/* func_800269C0_275C0, the FGM call -- see src/dc/ftcommon.h. */
#include "ftcommon.h"

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itlizardon.c:11-34 dITLizardonItemDesc, verbatim. */
ITDesc dITLizardonItemDesc =
{
    nITKindLizardon,                        /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataLizardonItemAttributes, /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindNull,                  /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itLizardonCommonProcUpdate,             /* Proc Update */
    itLizardonCommonProcMap,                /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itmonster/itlizardon.c:36-82 dITLizardonStatusDescs, verbatim. Row 0
 * is the decomp's own "Unused" and its two procs carry the same 0x8017F49C
 * address as row 1's second one, which is how a dead duplicate reads in a
 * decomp. */
ITStatusDesc dITLizardonStatusDescs[/* */] =
{
    /* Status 0 (Unused Fall) */
    {
        itLizardonFallUnusedProcUpdate,     /* Proc Update */
        itLizardonFallUnusedProcMap,        /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 1 (Air Fall) */
    {
        itLizardonFallProcUpdate,           /* Proc Update */
        itLizardonFallProcMap,              /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 2 (Neutral Attack) */
    {
        itLizardonAttackProcUpdate,         /* Proc Update */
        itLizardonAttackProcMap,            /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    }
};

/* it/itmonster/itlizardon.c:84-109 dITLizardonWeaponFlameWeaponDesc,
 * verbatim. Flags 0x00 and no proc_hop: the flame is destroyed by what it
 * touches and by its own lifetime, not by a bounce. */
WPDesc dITLizardonWeaponFlameWeaponDesc =
{
    0x00,                                   /* Render flags? */
    nWPKindLizardonFlame,                   /* Weapon Kind */
    &gITManagerCommonData,                  /* Pointer to character's loaded files? */
    (intptr_t)llITCommonDataLizardonFlameWeaponAttributes,  /* Offset of weapon attributes in loaded files */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyRSca,         /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    itLizardonWeaponFlameProcUpdate,        /* Proc Update */
    itLizardonWeaponFlameProcMap,           /* Proc Map */
    itLizardonWeaponFlameProcHit,           /* Proc Hit */
    itLizardonWeaponFlameProcHit,           /* Proc Shield */
    NULL,                                   /* Proc Hop */
    itLizardonWeaponFlameProcHit,           /* Proc Set-Off */
    itLizardonWeaponFlameProcReflector,     /* Proc Reflector */
    NULL                                    /* Proc Absorb */
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itLizardonStatus
{
    nITLizardonStatusFallUnused,            /* Unused */
    nITLizardonStatusFall,
    nITLizardonStatusAttack,
    nITLizardonStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itlizardon.c:117-125 itLizardonFallUnusedProcUpdate
 * 0x8017F470, verbatim: part of the decomp's own dead status pair. */
sb32 itLizardonFallUnusedProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITLIZARDON_GRAVITY, ITLIZARDON_TVEL);

    return FALSE;
}

/* it/itmonster/itlizardon.c:127-139 itLizardonFallUnusedProcMap
 * 0x8017F49C, verbatim. Its proc_map is the fall's own address in the
 * decomp's table too; ported as written. */
sb32 itLizardonFallUnusedProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMapCheckLanding(item_gobj, ITLIZARDON_MAP_REBOUND_COMMON, ITLIZARDON_MAP_REBOUND_GROUND, itLizardonAttackSetStatus);

    if (ip->coll_data.mask_curr & MAP_FLAG_FLOOR)
    {
        ip->physics.vel_air.y = 0.0F;
    }
    return FALSE;
}

/* it/itmonster/itlizardon.c:141-149 itLizardonFallUnusedSetStatus
 * 0x8017F49C (the decomp gives it its neighbour's proc_map's address too),
 * verbatim. TWO decomp slips are kept as written: it is declared `sb32`
 * with NO return statement -- undefined behaviour the original compiler's
 * calling convention papered over, exactly as itMainSearchRandomWeight's
 * missing return is kept in src/dc/itmain.c -- and nothing calls it, so
 * the port's copy is never entered either. A `return FALSE;` here would be
 * a rewrite of the decomp's text, which is the one thing this port does
 * not do. */
sb32 itLizardonFallUnusedSetStatus(GObj *item_gobj) /* Unused */
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITLizardonStatusDescs, nITLizardonStatusFallUnused);
}

/* it/itmonster/itlizardon.c:151-158 itLizardonFallProcUpdate 0x8017F53C,
 * verbatim. */
sb32 itLizardonFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITLIZARDON_GRAVITY, ITLIZARDON_TVEL);

    return FALSE;
}

/* it/itmonster/itlizardon.c:160-174 itLizardonFallProcMap 0x8017F568,
 * verbatim. The landing frame enters the attack state AND initialises it,
 * in that order -- the only place in the thirteen where a proc_map calls
 * both a SetStatus and an InitVars. */
sb32 itLizardonFallProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMapTestAllCheckCollEnd(item_gobj);

    if (ip->coll_data.mask_curr & MAP_FLAG_FLOOR)
    {
        ip->physics.vel_air.y = 0.0F;

        itLizardonAttackSetStatus(item_gobj);
        itLizardonAttackInitVars(item_gobj);
    }
    return FALSE;
}

/* it/itmonster/itlizardon.c:176-180 itLizardonFallSetStatus 0x8017F5C4,
 * verbatim. */
void itLizardonFallSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITLizardonStatusDescs, nITLizardonStatusFall);
}

/* it/itmonster/itlizardon.c:182-233 itLizardonAttackProcUpdate 0x8017F5EC,
 * verbatim. The flame's spawn point is recomputed every frame from the
 * CURRENT facing and the flame is only made on the frame the wait runs
 * out; the turn, when it comes, flips `lr` BEFORE the dust is placed, so
 * the dust lands on the side it just turned to. The model rotation is
 * gated on `nITKindPippi` -- a Charizard summoned by Clefairy does not
 * turn -- the same kind test Chansey's MakeItem uses on its parent. */
sb32 itLizardonAttackProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    Vec3f pos = dobj->translate.vec.f;

    if (ip->kind == nITKindLizardon)
    {
        pos.y += ITLIZARDON_LIZARDON_FLAME_OFF_Y;

        pos.x += (ITLIZARDON_LIZARDON_FLAME_OFF_X * ip->lr);
    }
    else pos.x += (ITLIZARDON_OTHER_FLAME_OFF_X * ip->lr);

    if (ip->item_vars.lizardon.flame_spawn_wait == 0)
    {
        itLizardonAttackMakeFlame(item_gobj, &pos, ip->lr);

        ip->item_vars.lizardon.flame_spawn_wait = ITLIZARDON_FLAME_SPAWN_WAIT;
    }
    ip->item_vars.lizardon.flame_spawn_wait--;

    if (ip->multi == 0)
    {
        return TRUE;
    }
    if (ip->item_vars.lizardon.turn_wait == 0)
    {
        ip->item_vars.lizardon.turn_wait = ITLIZARDON_TURN_WAIT;

        ip->lr = -ip->lr;

        pos = dobj->translate.vec.f;

        pos.y += ip->attr->map_coll_bottom;

        pos.x += (ip->attr->map_coll_width + ITLIZARDON_DUST_OFF_X) * -ip->lr;

        efManagerDustHeavyMakeEffect(&pos, -ip->lr);

        if (ip->kind == nITKindPippi)
        {
            dobj->rotate.vec.f.y += F_CST_DTOR32(180.0F);
        }
    }
    ip->item_vars.lizardon.turn_wait--;

    ip->multi--;

    return FALSE;
}

/* it/itmonster/itlizardon.c:235-241 itLizardonAttackProcMap 0x8017F7E8,
 * verbatim. */
sb32 itLizardonAttackProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itLizardonFallSetStatus);

    return FALSE;
}

/* it/itmonster/itlizardon.c:243-266 itLizardonAttackInitVars 0x8017F810,
 * verbatim. Both scripts come out of the same two-step address the decomp
 * builds by hand -- `addr = attr->data - LizardonDataStart`, then
 * `lbRelocGetFileData(..., addr, LizardonAnimJoint)` -- which is
 * `itGetPData(ip, A, B)` exactly, so the port spells it that way. The
 * MatAnimJoint hangs on the MObj. */
void itLizardonAttackInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    s32 unused[2];
    Vec3f pos;

    ip->item_vars.lizardon.turn_wait = ITLIZARDON_TURN_WAIT;

    pos = dobj->translate.vec.f;

    ip->item_vars.lizardon.pos = pos;
    ip->item_vars.lizardon.flame_spawn_wait = 0;

    ip->lr = -1;

    if (ip->kind == nITKindLizardon)
    {
        gcAddDObjAnimJoint(dobj, itGetPData(ip, llITCommonDataLizardonDataStart, llITCommonDataLizardonAnimJoint), 0.0F);
        gcAddMObjMatAnimJoint(dobj->mobj, itGetPData(ip, llITCommonDataLizardonDataStart, llITCommonDataLizardonMatAnimJoint), 0.0F);
        gcPlayAnimAll(item_gobj);
    }
}

/* it/itmonster/itlizardon.c:268-272 itLizardonAttackSetStatus 0x8017F8E4,
 * verbatim. It does NOT call InitVars -- the fall's proc_map does, right
 * after. */
void itLizardonAttackSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITLizardonStatusDescs, nITLizardonStatusAttack);
}

/* it/itmonster/itlizardon.c:274-302 itLizardonCommonProcUpdate 0x8017F90C,
 * verbatim. THE LIFETIME IS SET HERE, not by the attack: the rise writes
 * `multi = ITLIZARDON_LIFETIME` as it hands over to the fall, and the
 * fall's landing hands over to the attack with that same count. */
sb32 itLizardonCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        ip->multi = ITLIZARDON_LIFETIME;

        ip->physics.vel_air.y = 0.0F;

        if (ip->kind == nITKindLizardon)
        {
            func_800269C0_275C0(nSYAudioVoiceMBallLizardonAppear);
        }
        itLizardonFallSetStatus(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itlizardon.c:304-314 itLizardonCommonProcMap 0x8017F98C,
 * verbatim. */
sb32 itLizardonCommonProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR) != FALSE)
    {
        ip->physics.vel_air.y = 0.0F;
    }
    return FALSE;
}

/* it/itmonster/itlizardon.c:316-343 itLizardonMakeItem 0x8017F9CC,
 * verbatim. Note it does NOT call itMainClearOwnerStats, unlike Goldeen's
 * and Onix's. */
GObj* itLizardonMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITLizardonItemDesc, pos, vel, flags);
    DObj *dobj;
    ITStruct *ip;

    if (item_gobj != NULL)
    {
        dobj = DObjGetStruct(item_gobj);

        gcAddXObjForDObjFixed(dobj, nGCMatrixKindTraRotRpyR, 0);
        gcAddXObjForDObjFixed(dobj, 0x48, 0);

        dobj->translate.vec.f = *pos;

        ip = itGetStruct(item_gobj);

        ip->multi = ITMONSTER_RISE_STOP_WAIT;

        ip->physics.vel_air.x = ip->physics.vel_air.z = 0.0F;
        ip->physics.vel_air.y = ITMONSTER_RISE_VEL_Y;

        dobj->translate.vec.f.y -= ip->attr->map_coll_bottom;

        gcAddDObjAnimJoint(dobj, itGetMonsterAnimNode(ip, llITCommonDataLizardonDataStart), 0.0F);
    }
    return item_gobj;
}

/* ---- the Flame weapon ---------------------------------------------- */

/* it/itmonster/itlizardon.c:345-355 itLizardonWeaponFlameProcUpdate
 * 0x8017FACC, verbatim. */
sb32 itLizardonWeaponFlameProcUpdate(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);

    if (wpMainDecLifeCheckExpire(wp) != FALSE)
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itmonster/itlizardon.c:357-366 itLizardonWeaponFlameProcMap
 * 0x8017FAF8, verbatim: the flame dies on the floor and the floor gets a
 * puff at the same time. */
sb32 itLizardonWeaponFlameProcMap(GObj *weapon_gobj)
{
    if (wpMapTestAllCheckCollEnd(weapon_gobj) != FALSE)
    {
        efManagerDustExpandSmallMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f, 1.0F);

        return TRUE;
    }
    else return FALSE;
}

/* it/itmonster/itlizardon.c:368-375 itLizardonWeaponFlameProcHit
 * 0x8017FB3C, verbatim: sparks, and the flame SURVIVES the hit -- it
 * returns FALSE. */
sb32 itLizardonWeaponFlameProcHit(GObj *weapon_gobj)
{
    func_800269C0_275C0(nSYAudioFGMExplodeS);
    efManagerSparkleWhiteMakeEffect(&DObjGetStruct(weapon_gobj)->translate.vec.f);

    return FALSE;
}

/* it/itmonster/itlizardon.c:377-395 itLizardonWeaponFlameProcReflector
 * 0x8017FB74, verbatim. The lifetime is RESET here, which is the one
 * difference from its siblings' reflectors, and the two particle scripts
 * are 2 and 0 of the item bank -- the flame's own fire. */
sb32 itLizardonWeaponFlameProcReflector(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    FTStruct *fp = ftGetStruct(wp->owner_gobj);
    Vec3f *translate;

    wp->lifetime = ITLIZARDON_FLAME_LIFETIME;

    wpMainReflectorSetLR(wp, fp);

    translate = &DObjGetStruct(weapon_gobj)->translate.vec.f;

    lbParticleMakePosVel(gITManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 2, translate->x, translate->y, 0.0F, wp->physics.vel_air.x, wp->physics.vel_air.y, 0.0F);
    lbParticleMakePosVel(gITManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0, translate->x, translate->y, 0.0F, wp->physics.vel_air.x, wp->physics.vel_air.y, 0.0F);

    return FALSE;
}

/* it/itmonster/itlizardon.c:397-416 itLizardonWeaponFlameMakeWeapon
 * 0x8017FC38, verbatim, the decomp's own comment included. The two
 * `lbParticleMakePosVel` calls ARE the flame: scripts 2 and 0 of the item
 * bank, emitted at the spawn point with the weapon's own velocity. They
 * are why this weapon still reads as fire even though its model has no
 * pack -- see this file's header. */
GObj* itLizardonWeaponFlameMakeWeapon(GObj *item_gobj, Vec3f *pos, Vec3f *vel)
{
    GObj *weapon_gobj = wpManagerMakeWeapon(item_gobj, &dITLizardonWeaponFlameWeaponDesc, pos, WEAPON_FLAG_PARENT_ITEM);
    WPStruct *ip;

    if (weapon_gobj == NULL)
    {
        return NULL;
    }
    ip = wpGetStruct(weapon_gobj);

    ip->physics.vel_air = *vel;

    ip->lifetime = ITLIZARDON_FLAME_LIFETIME;

    lbParticleMakePosVel(gITManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 2, pos->x, pos->y, 0.0F, ip->physics.vel_air.x, ip->physics.vel_air.y, 0.0F); /* This needs to return something in v0 to match */
    lbParticleMakePosVel(gITManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0, pos->x, pos->y, 0.0F, ip->physics.vel_air.x, ip->physics.vel_air.y, 0.0F);

    return weapon_gobj;
}

/* it/itmonster/itlizardon.c:418-436 itLizardonAttackMakeFlame 0x8017FD2C,
 * verbatim. The angle is negative in it/itvars.h, so `__sinf` gives a
 * downward Y and the flame arcs away from Charizard rather than straight
 * out. `__cosf`/`__sinf` are libultra's (tools/export/ssb_trigexport.py),
 * checked word-for-word against the ROM's. */
void itLizardonAttackMakeFlame(GObj *item_gobj, Vec3f *pos, s32 lr)
{
    ITStruct *ip = itGetStruct(item_gobj);
    Vec3f vel;

    vel.x = __cosf(ITLIZARDON_FLAME_ANGLE) * ITLIZARDON_FLAME_VEL * lr;
    vel.y = __sinf(ITLIZARDON_FLAME_ANGLE) * ITLIZARDON_FLAME_VEL;
    vel.z = 0.0F;

    itLizardonWeaponFlameMakeWeapon(item_gobj, pos, &vel);

    func_800269C0_275C0(nSYAudioFGMLizardonFlame);
}
