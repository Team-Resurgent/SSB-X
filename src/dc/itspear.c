/* itspear.c -- it/itmonster/itspear.c, verbatim: Beedrill's `ITDesc`, its
 * two-row `ITStatusDesc` table (appear / fly) and their proc bodies, plus
 * the Swarm weapon's two `WPDesc`s and that weapon's procs.
 * Function-for-function against the game's own ITStruct and WPStruct
 * (it/ittypes.h, wp/wptypes.h); every function names its decomp line
 * range.
 *
 * The twelfth monster, and the file that carries TWO
 * descriptors: `dITSpearWeaponSwarmWeaponDesc` for Beedrill's own swarm
 * and `dITPippiWeaponSwarmWeaponDesc` for the one Clefairy summons. They
 * are the same kind (`nWPKindSpearSwarm`), the same procs and the same
 * model; what differs is which `WeaponAttributes` table they read, and
 * that difference is written into a knock-on that reaches the renderer --
 * see below.
 *
 *  - `nITSpearStatusAppear` is the shortest state in the thirteen and the
 *    only one that ends on an ANIMATION FRAME rather than a counter:
 *    `itSpearAppearProcUpdate` watches `item_gobj->anim_frame` for
 *    ITSPEAR_SWARM_CALL_WAIT and, on that frame, NULLs the child joint's
 *    `anim_joint.event32` (which is what STOPS the script) and hands over
 *    to the fly state.
 *  - `nITSpearStatusFly` accelerates toward the wall it faces until it
 *    reaches ITSPEAR_SWARM_CALL_OFF_X from the bound, then pays out
 *    ITSPEAR_SPAWN_COUNT swarm members from a random height in
 *    ±ITSPEAR_SPAWN_OFF_Y_MUL of where it stopped -- and returns TRUE the
 *    moment the count is spent.
 *  - Each member is spawned with `wp->lr = -ip->lr`: the swarm flies BACK
 *    toward Beedrill's origin. Beedrill's own members hang their XObj on
 *    `dobj->child->child` and turn on `lr == -1`; Clefairy's hang it on
 *    `dobj->child` and turn on `lr == +1` -- opposite joints and opposite
 *    tests, which is the one place the two descriptors' paths diverge.
 *  - Clefairy's swarm also REPLACES the weapon's whole display proc with
 *    `itPippiWeaponSwarmProcDisplay`, which brackets the tree draw in
 *    gDPPipeSync and an XLU render mode via `wpDisplayMain`. Beedrill's
 *    keeps whatever wpManagerMakeWeapon picked.
 *
 * DIVERGES: the three descriptors' offset fields are numbers rather than
 * the decomp's `&llITCommonData...` symbols -- see src/dc/itemoffsets.h --
 * and the `itGetPData`/`itGetMonsterAnimNode` calls drop their `&`. No
 * function body diverges. `itSpearWeaponSwarmProcUpdate` is a plain
 * bound test that would be better named `ProcDead`; the decomp's name is
 * kept because it is what the WPDesc's Proc Update slot holds.
 */
#include <it/item.h>
#include <wp/weapon.h>
#include <gr/ground.h>

#include "itmonster.h"          /* itGetMonsterAnimNode */
#include "itemoffsets.h"        /* llITCommonData* offsets */
#include "objmodel.h"           /* dc_model_proc_display */

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

/* it/itmonster/itspear.c:11-34 dITSpearItemDesc, verbatim. */
ITDesc dITSpearItemDesc =
{
    nITKindSpear,                           /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataSpearItemAttributes,    /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,            /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itSpearCommonProcUpdate,                /* Proc Update */
    itSpearCommonProcMap,                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itmonster/itspear.c:36-64 dITSpearStatusDescs, verbatim. Both rows
 * have NULL proc_maps: every state's work is in its update. */
ITStatusDesc dITSpearStatusDescs[/* */] =
{
    /* Status 0 (Neutral Appear) */
    {
        itSpearAppearProcUpdate,            /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 1 (Neutral Fly) */
    {
        itSpearFlyProcUpdate,               /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    }
};

/* it/itmonster/itspear.c:66-88 dITSpearWeaponSwarmWeaponDesc, verbatim.
 * Flags 0x01 (a tree with DL links), and it carries NO proc_hit, no
 * proc_map and no proc_setoff: a swarm member is destroyed only by the
 * bound test in its own proc_update. */
WPDesc dITSpearWeaponSwarmWeaponDesc =
{
    0x01,                                     /* Render flags? */
    nWPKindSpearSwarm,                        /* Weapon Kind */
    &gITManagerCommonData,                    /* Pointer to character's loaded files? */
    (intptr_t)llITCommonDataSpearSwarmWeaponAttributes,  /* Offset of weapon attributes in loaded files */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,            /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    itSpearWeaponSwarmProcUpdate,           /* Proc Update */
    NULL,                                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Absorb */
};

/* it/itmonster/itspear.c:90-112 dITPippiWeaponSwarmWeaponDesc, verbatim.
 * Clefairy's swarm: the SAME kind and the same procs as Beedrill's, and a
 * different attributes table -- which is the only reason two descriptors
 * exist for one weapon kind. Its pack is a different file
 * (wppippiswarm.mdl), so the port's model table has a row for each. */
WPDesc dITPippiWeaponSwarmWeaponDesc =
{
    0x01,                                     /* Render flags? */
    nWPKindSpearSwarm,                        /* Weapon Kind */
    &gITManagerCommonData,                    /* Pointer to character's loaded files? */
    (intptr_t)llITCommonDataPippiSwarmWeaponAttributes,  /* Offset of weapon attributes in loaded files */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyR,            /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    itSpearWeaponSwarmProcUpdate,           /* Proc Update */
    NULL,                                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Absorb */
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itSpearStatus
{
    nITSpearStatusAppear,
    nITSpearStatusFly,
    nITSpearStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itspear.c:130-145 itSpearFlyCallSwarmMember 0x8017FDC0,
 * verbatim. The member's height is re-randomised around the height
 * Beedrill STOPPED at, not around its current position -- `spear_spawn_pos_y`
 * is captured once by itSpearFlyInitVars. */
void itSpearFlyCallSwarmMember(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if (ip->item_vars.spear.spear_spawn_wait <= 0)
    {
        Vec3f pos = dobj->translate.vec.f;
        s32 unused;

        pos.y = ip->item_vars.spear.spear_spawn_pos_y;

        pos.y += (ITSPEAR_SPAWN_OFF_Y_MUL * syUtilsRandFloat()) + ITSPEAR_SPAWN_OFF_Y_ADD;

        itSpearFlyMakeSwarm(item_gobj, &pos, ip->kind);

        ip->item_vars.spear.spear_spawn_count--;
        ip->item_vars.spear.spear_spawn_wait = syUtilsRandIntRange(ITSPEAR_SPAWN_WAIT_RANDOM) + ITSPEAR_SPAWN_WAIT_CONST;
    }
}

/* it/itmonster/itspear.c:147-158 itSpearAppearProcUpdate 0x8017FE70,
 * verbatim. THE ONE STATE IN THE THIRTEEN THAT ENDS ON A FRAME, not a
 * counter: `anim_frame` is the GObj's own (the same one
 * lbCommonAddDObjAnimJointAll writes), and clearing the child's
 * `anim_joint.event32` is what stops the script. */
sb32 itSpearAppearProcUpdate(GObj *item_gobj)
{
    DObj *dobj = DObjGetStruct(item_gobj);

    if (item_gobj->anim_frame == ITSPEAR_SWARM_CALL_WAIT)
    {
        dobj->child->anim_joint.event32 = NULL;

        itSpearFlySetStatus(item_gobj);
    }
    return FALSE;
}

/* it/itmonster/itspear.c:160-186 itSpearAppearInitVars 0x8017FEB8,
 * verbatim. Both scripts hang on the CHILD joint, and the MatAnimJoint
 * hangs on that child's MObj. */
void itSpearAppearInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    ip->multi = 0;

    ip->physics.vel_air.y = 0;

    if (ip->kind == nITKindSpear)
    {
        void *anim_joint;
        void *matanim_joint;

        anim_joint = itGetPData(ip, llITCommonDataSpearDataStart, llITCommonDataSpearAnimJoint);

        gcAddDObjAnimJoint(dobj->child, anim_joint, 0.0F);

        matanim_joint = itGetPData(ip, llITCommonDataSpearDataStart, llITCommonDataSpearMatAnimJoint);

        gcAddMObjMatAnimJoint(dobj->child->mobj, matanim_joint, 0.0F);
        gcPlayAnimAll(item_gobj);
        func_800269C0_275C0(nSYAudioVoiceMBallSpearAppear);
    }
}

/* it/itmonster/itspear.c:188-192 itSpearAppearSetStatus 0x8017FF74,
 * verbatim. */
void itSpearAppearSetStatus(GObj *item_gobj)
{
    itSpearAppearInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITSpearStatusDescs, nITSpearStatusAppear);
}

/* it/itmonster/itspear.c:194-244 itSpearFlyProcUpdate 0x8017FFA8,
 * verbatim. The two facing arms are the same code twice -- the decomp
 * writes them out rather than folding them into one bound test, and the
 * port keeps that. Note the `spear_spawn_wait--` is INSIDE the `count != 0`
 * arm, so the wait only counts down while there are members left to call. */
sb32 itSpearFlyProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITSPEAR_GRAVITY, ITSPEAR_TVEL);

    ip->physics.vel_air.x += ITSPEAR_SWARM_CALL_VEL_X * ip->lr;

    if (ip->lr == +1)
    {
        if (dobj->translate.vec.f.x >= (gMPCollisionGroundData->map_bound_right - ITSPEAR_SWARM_CALL_OFF_X))
        {
            ip->physics.vel_air.x = 0.0F;
            ip->physics.vel_air.y = 0.0F;

            if (ip->item_vars.spear.spear_spawn_count != 0)
            {
                itSpearFlyCallSwarmMember(item_gobj);
            }
            else return TRUE;

            ip->item_vars.spear.spear_spawn_wait--;
        }
    }
    if (ip->lr == -1)
    {
        if (dobj->translate.vec.f.x <= (gMPCollisionGroundData->map_bound_left + ITSPEAR_SWARM_CALL_OFF_X))
        {
            ip->physics.vel_air.x = 0.0F;
            ip->physics.vel_air.y = 0.0F;

            if (ip->item_vars.spear.spear_spawn_count != 0)
            {
                itSpearFlyCallSwarmMember(item_gobj);
            }
            else return TRUE;

            ip->item_vars.spear.spear_spawn_wait--;
        }
    }
    return FALSE;
}

/* it/itmonster/itspear.c:246-260 itSpearFlyInitVars 0x8018010C,
 * verbatim. `spear_spawn_pos_y` is captured HERE, once, and every member
 * after the first is placed relative to it. */
void itSpearFlyInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->physics.vel_air.y = ITSPEAR_SWARM_CALL_VEL_Y;

    ip->item_vars.spear.spear_spawn_pos_y = DObjGetStruct(item_gobj)->translate.vec.f.y;
    ip->item_vars.spear.spear_spawn_wait = 0;
    ip->item_vars.spear.spear_spawn_count = ITSPEAR_SPAWN_COUNT;

    if (ip->kind == nITKindSpear)
    {
        func_800269C0_275C0(nSYAudioVoiceMBallSpearSwarm);
    }
}

/* it/itmonster/itspear.c:262-266 itSpearFlySetStatus 0x80180160,
 * verbatim. */
void itSpearFlySetStatus(GObj *item_gobj)
{
    itSpearFlyInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITSpearStatusDescs, nITSpearStatusFly);
}

/* it/itmonster/itspear.c:268-281 itSpearCommonProcUpdate 0x80180194,
 * verbatim. */
sb32 itSpearCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        itSpearAppearSetStatus(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itspear.c:283-294 itSpearCommonProcMap 0x801801D8,
 * verbatim. */
sb32 itSpearCommonProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_FLOOR) != FALSE)
    {
        ip->physics.vel_air.y = 0.0F;
    }
    return FALSE;
}

/* it/itmonster/itspear.c:296-331 itSpearMakeItem 0x80180218, verbatim.
 * Beedrill is the second monster to pick a RANDOM starting facing -- the
 * 180-degree child rotation and `lr = -1` go together, and the coin is
 * what makes two Beedrills summoned together fly opposite ways. */
GObj* itSpearMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITSpearItemDesc, pos, vel, flags);
    DObj *dobj;
    ITStruct *ip;

    if (item_gobj != NULL)
    {
        ip = itGetStruct(item_gobj);

        itMainClearOwnerStats(item_gobj);

        dobj = DObjGetStruct(item_gobj);

        gcAddXObjForDObjFixed(dobj->child, 0x48, 0);

        dobj->translate.vec.f = *pos;

        if (syUtilsRandIntRange(2) == 0)
        {
            dobj->child->rotate.vec.f.y = F_CST_DTOR32(180.0F);

            ip->lr = -1;
        }
        else ip->lr = +1;

        ip->multi = ITMONSTER_RISE_STOP_WAIT;

        ip->attack_coll.interact_mask = GMHITCOLLISION_FLAG_FIGHTER;

        ip->physics.vel_air.x = ip->physics.vel_air.z = 0.0F;
        ip->physics.vel_air.y = ITMONSTER_RISE_VEL_Y;

        dobj->translate.vec.f.y -= ip->attr->map_coll_bottom;

        gcAddDObjAnimJoint(dobj->child, itGetMonsterAnimNode(ip, llITCommonDataSpearDataStart), 0.0F);
    }
    return item_gobj;
}

/* ---- the Swarm weapon ---------------------------------------------- */

/* it/itmonster/itspear.c:333-349 itSpearWeaponSwarmProcUpdate 0x80180354,
 * verbatim. It is in the WPDesc's Proc Update slot and it is really a
 * death test: a member that reaches the bound is destroyed. The name is
 * the decomp's and is kept. */
sb32 itSpearWeaponSwarmProcUpdate(GObj *weapon_gobj)
{
    WPStruct *wp = wpGetStruct(weapon_gobj);
    DObj *dobj = DObjGetStruct(weapon_gobj);

    if ((wp->lr == +1) && (dobj->translate.vec.f.x >= (gMPCollisionGroundData->map_bound_right - ITSPEAR_SWARM_CALL_OFF_X)))
    {
        return TRUE;
    }
    else if ((wp->lr == -1) && (dobj->translate.vec.f.x <= (gMPCollisionGroundData->map_bound_left + ITSPEAR_SWARM_CALL_OFF_X)))
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itmonster/itspear.c:351-361 itPippiWeaponSwarmRenderSwarm 0x80180400,
 * verbatim: the tree draw bracketed in a pipe sync and an XLU render
 * mode. */
void itPippiWeaponSwarmRenderSwarm(GObj *item_gobj)
{
    gDPPipeSync(gSYTaskmanDLHeads[0]++);

    gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);

    dc_model_proc_display(item_gobj);

    gDPPipeSync(gSYTaskmanDLHeads[0]++);
}

/* it/itmonster/itspear.c:363-367 itPippiWeaponSwarmProcDisplay
 * 0x80180480, verbatim: wpDisplayMain's second form, where the caller
 * supplies the DObj draw. */
void itPippiWeaponSwarmProcDisplay(GObj *item_gobj)
{
    wpDisplayMain(item_gobj, itPippiWeaponSwarmRenderSwarm);
}

/* it/itmonster/itspear.c:369-410 itSpearWeaponSwarmMakeWeapon 0x801804A4,
 * verbatim. THE TWO DESCRIPTORS' PATHS DIVERGE HERE and nowhere else: the
 * descriptor is picked by `kind`; the member flies at `-ip->lr` (back
 * toward Beedrill's origin) so the swarm crosses the stage; and Beedrill's
 * member hangs its XObj on `child->child` and turns on `lr == -1` where
 * Clefairy's hangs it on `child` and turns on `lr == +1`. Clefairy's also
 * replaces the whole display proc. */
GObj* itSpearWeaponSwarmMakeWeapon(GObj *item_gobj, Vec3f *pos, s32 kind)
{
    ITStruct *ip = itGetStruct(item_gobj);
    GObj *weapon_gobj = wpManagerMakeWeapon(item_gobj, ((kind == nITKindSpear) ? &dITSpearWeaponSwarmWeaponDesc : &dITPippiWeaponSwarmWeaponDesc), pos, WEAPON_FLAG_PARENT_ITEM);
    DObj *dobj;
    s32 unused;
    WPStruct *wp;

    if (weapon_gobj == NULL)
    {
        return NULL;
    }
    wp = wpGetStruct(weapon_gobj);

    wp->lr = -ip->lr;

    wp->physics.vel_air.x = wp->lr * ITSPEAR_SWARM_FLY_VEL_X;

    dobj = DObjGetStruct(weapon_gobj);

    if (kind == nITKindSpear)
    {
        gcAddXObjForDObjFixed(dobj->child->child, 0x48, 0);

        if (wp->lr == -1)
        {
            dobj->child->child->rotate.vec.f.y = F_CST_DTOR32(180.0F);
        }
    }
    else
    {
        weapon_gobj->proc_display = itPippiWeaponSwarmProcDisplay;

        gcAddXObjForDObjFixed(dobj->child, 0x48, 0);

        if (wp->lr == +1)
        {
            dobj->child->rotate.vec.f.y = F_CST_DTOR32(180.0F);
        }
    }
    dobj->translate.vec.f = *pos;

    wp->is_hitlag_victim = TRUE;

    return weapon_gobj;
}

/* it/itmonster/itspear.c:412-415 itSpearFlyMakeSwarm 0x80180608,
 * verbatim: a one-line wrapper that exists so the caller reads as a
 * summon rather than a weapon make. */
void itSpearFlyMakeSwarm(GObj *item_gobj, Vec3f *pos, s32 kind)
{
    itSpearWeaponSwarmMakeWeapon(item_gobj, pos, kind);
}
