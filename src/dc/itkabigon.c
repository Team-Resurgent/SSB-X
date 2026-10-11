/* itkabigon.c -- it/itmonster/itkabigon.c, verbatim: Snorlax's `ITDesc`,
 * its two-row `ITStatusDesc` table (jump / fall) and their proc bodies,
 * plus the display proc the fall swaps in. Function-for-function against
 * the game's own ITStruct (it/ittypes.h); every function names its decomp
 * line range.
 *
 * The fourth monster. Snorlax is the one that comes down:
 *
 *  - `itKabigonCommonProcUpdate` releases the rise into `ITKABIGON_DROP_WAIT`
 *    (60) frames of JUMP at ITKABIGON_JUMP_VEL_Y, spraying
 *    efManagerDustExpandLargeMakeEffect every ITKABIGON_EFFECT_SPAWN_INT
 *    frames from a random offset inside ±ITKABIGON_JUMP_GFX_*.
 *  - `itKabigonJumpProcUpdate` counts that wait down only while the item is
 *    above the map's top bound, then hands over to `nITKabigonStatusFall`:
 *    a drop at ITKABIGON_DROP_VEL_Y with an efManagerQuakeMakeEffect every
 *    ITKABIGON_RUMBLE_WAIT frames, until it passes the map's BOTTOM bound
 *    and returns TRUE -- Snorlax does not despawn by a lifetime or by
 *    landing, it falls off the stage.
 *  - The scale and the hitbox are the same number twice: `DROP_SIZE_KABIGON`
 *    (4.0) for Snorlax's own kind, `DROP_SIZE_OTHER` (5.2) for a Clefairy
 *    that summoned one -- the one place a monster's own appearance depends
 *    on who called it.
 *  - `itKabigonFallProcDisplay` is a SECOND display proc (XLU render mode
 *    instead of ZB_TEX_EDGE), swapped in for the fall by writing
 *    `item_gobj->proc_display` -- and both of them move the item's DL head
 *    to 18, the same priority Hitmonlee uses.
 *
 * DIVERGES, the one every item file has: `dITKabigonItemDesc`'s third
 * field is a number rather than the decomp's `&llITCommonData...` symbol,
 * and the `itGetMonsterAnimNode` call drops its `&`. See
 * src/dc/itemoffsets.h. No function body diverges.
 */
#include <it/item.h>
#include <sys/develop.h>
#include <ef/efmanager.h>       /* efManagerQuakeMakeEffect,
                                 * efManagerDustExpandLargeMakeEffect */

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

/* it/itmonster/itkabigon.c:11-34 dITKabigonItemDesc, verbatim. Its
 * proc_map is NULL: Snorlax's own two states each do their bound test in
 * the update, so the desc has no map step at all. */
ITDesc dITKabigonItemDesc =
{
    nITKindKabigon,                         /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataKabigonItemAttributes,  /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyRSca,         /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itKabigonCommonProcUpdate,              /* Proc Update */
    NULL,                                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itmonster/itkabigon.c:36-66 dITKabigonStatusDescs, verbatim. */
ITStatusDesc dITKabigonStatusDescs[/* */] =
{
    /* Status 0 (Neutral Jump) */
    {
        itKabigonJumpProcUpdate,            /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        NULL                                /* Proc Damage */
    },

    /* Status 1 (Neutral Fall) */
    {
        itKabigonFallProcUpdate,            /* Proc Update */
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

enum itKabigonStatus
{
    nITKabigonStatusJump,
    nITKabigonStatusFall,
    nITKabigonStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmonster/itkabigon.c:76-97 itKabigonFallProcUpdate 0x8017E070,
 * verbatim. The quake's magnitude is 0 -- the decomp passes one anyway
 * rather than calling a no-argument maker. */
sb32 itKabigonFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if (ip->item_vars.kabigon.rumble_wait == 0)
    {
        efManagerQuakeMakeEffect(0);

        ip->item_vars.kabigon.rumble_wait = ITKABIGON_RUMBLE_WAIT;
    }
    ip->item_vars.kabigon.rumble_wait--;

    if (dobj->translate.vec.f.y < (gMPCollisionGroundData->map_bound_bottom + ITKABIGON_MAP_OFF_Y))
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itmonster/itkabigon.c:99-130 itKabigonFallProcDisplay 0x8017E100,
 * verbatim: the desc's common display proc with G_RM_AA_XLU_SURF in place
 * of G_RM_AA_ZB_TEX_EDGE. The four arms and the two gDPPipeSync calls are
 * itPippiCommonProcDisplay's shape exactly. */
void itKabigonFallProcDisplay(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    gDPPipeSync(gSYTaskmanDLHeads[0]++);

    if (itDisplayCheckItemVisible(ip) != FALSE)
    {
        if ((ip->display_mode == nDBDisplayModeMaster) || (ip->is_hold))
        {
            gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);

            dc_model_proc_display(item_gobj);
        }
        else if (ip->display_mode == nDBDisplayModeMapCollision)
        {
            gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);

            dc_model_proc_display(item_gobj);
            itDisplayMapCollisions(item_gobj);
        }
        else if ((ip->damage_coll.hitstatus == nGMHitStatusNone) && (ip->attack_coll.attack_state == nGMAttackStateOff))
        {
            gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);

            dc_model_proc_display(item_gobj);
        }
        else itDisplayHitCollisions(item_gobj);
    }
    gDPPipeSync(gSYTaskmanDLHeads[0]++);
}

/* it/itmonster/itkabigon.c:132-161 itKabigonFallInitVars 0x8017E25C,
 * verbatim. The `x += rand*2000 - 1000` is the decomp's own framing of
 * "somewhere within a thousand units either side"; itMainRefreshAttackColl
 * is called because the size is about to change under it. */
void itKabigonFallInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    ip->physics.vel_air.y = ITKABIGON_DROP_VEL_Y;

    dobj->translate.vec.f.x += ((ITKABIGON_DROP_OFF_X_MUL * syUtilsRandFloat()) + ITKABIGON_DROP_OFF_X_ADD);

    itMainRefreshAttackColl(item_gobj);

    ip->item_vars.kabigon.rumble_wait = 0;

    func_800269C0_275C0(nSYAudioFGMKabigonFall);

    if (ip->kind == nITKindKabigon)
    {
        func_800269C0_275C0(nSYAudioVoiceMBallKabigonFall);

        dobj->scale.vec.f.x = dobj->scale.vec.f.y = ITKABIGON_DROP_SIZE_KABIGON;

        ip->attack_coll.size *= ITKABIGON_DROP_SIZE_KABIGON;
    }
    else
    {
        dobj->scale.vec.f.x = dobj->scale.vec.f.y = ITKABIGON_DROP_SIZE_OTHER;

        ip->attack_coll.size *= ITKABIGON_DROP_SIZE_OTHER;
    }
    item_gobj->proc_display = itKabigonFallProcDisplay;

    gcMoveGObjDLHead(item_gobj, 18, item_gobj->dl_link_priority);
}

/* it/itmonster/itkabigon.c:163-167 itKabigonFallSetStatus 0x8017E350,
 * verbatim. */
void itKabigonFallSetStatus(GObj *item_gobj)
{
    itKabigonFallInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITKabigonStatusDescs, nITKabigonStatusFall);
}

/* it/itmonster/itkabigon.c:169-201 itKabigonJumpProcUpdate 0x8017E384,
 * verbatim. Note the countdown only runs ABOVE the map's top bound: below
 * it `multi` holds and the jump waits. */
sb32 itKabigonJumpProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if (dobj->translate.vec.f.y >= (gMPCollisionGroundData->map_bound_top - ITKABIGON_MAP_OFF_Y))
    {
        ip->multi--;

        ip->physics.vel_air.y = 0.0F;

        if (ip->multi == 0)
        {
            itKabigonFallSetStatus(item_gobj);
        }
    }
    if (ip->item_vars.kabigon.dust_effect_int == 0)
    {
        Vec3f pos = dobj->translate.vec.f;

        pos.x += (syUtilsRandFloat() * ITKABIGON_JUMP_GFX_MUL_OFF) - ITKABIGON_JUMP_GFX_SUB_OFF;
        pos.y += (syUtilsRandFloat() * ITKABIGON_JUMP_GFX_MUL_OFF) - ITKABIGON_JUMP_GFX_SUB_OFF;

        efManagerDustExpandLargeMakeEffect(&pos);

        ip->item_vars.kabigon.dust_effect_int = ITKABIGON_EFFECT_SPAWN_INT;
    }
    ip->item_vars.kabigon.dust_effect_int--;

    return FALSE;
}

/* it/itmonster/itkabigon.c:203-233 itKabigonCommonProcDisplay 0x8017E4A4,
 * verbatim: the desc's display proc, ZB_TEX_EDGE, and what the fall swaps
 * away from. */
void itKabigonCommonProcDisplay(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    gDPPipeSync(gSYTaskmanDLHeads[0]++);

    if (itDisplayCheckItemVisible(ip) != FALSE)
    {
        if ((ip->display_mode == nDBDisplayModeMaster) || (ip->is_hold))
        {
            gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);

            dc_model_proc_display(item_gobj);
        }
        else if (ip->display_mode == nDBDisplayModeMapCollision)
        {
            gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);

            dc_model_proc_display(item_gobj);
            itDisplayMapCollisions(item_gobj);
        }
        else if ((ip->damage_coll.hitstatus == nGMHitStatusNone) && (ip->attack_coll.attack_state == nGMAttackStateOff))
        {
            gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);

            dc_model_proc_display(item_gobj);
        }
        else itDisplayHitCollisions(item_gobj);
    }
    gDPPipeSync(gSYTaskmanDLHeads[0]++);
}

/* it/itmonster/itkabigon.c:235-246 itKabigonJumpInitVars 0x8017E600,
 * verbatim. `itMainRefreshAttackColl` is NOT called here -- the size does
 * not change until the fall. */
void itKabigonJumpInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    func_800269C0_275C0(nSYAudioFGMKabigonJump);

    ip->multi = ITKABIGON_DROP_WAIT;

    ip->item_vars.kabigon.dust_effect_int = ITKABIGON_EFFECT_SPAWN_INT;

    ip->physics.vel_air.y = ITKABIGON_JUMP_VEL_Y;
}

/* it/itmonster/itkabigon.c:248-252 itKabigonJumpSetStatus 0x8017E648,
 * verbatim. */
void itKabigonJumpSetStatus(GObj *item_gobj)
{
    itKabigonJumpInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITKabigonStatusDescs, nITKabigonStatusJump);
}

/* it/itmonster/itkabigon.c:254-267 itKabigonCommonProcUpdate 0x8017E67C,
 * verbatim: the rise, then the jump. */
sb32 itKabigonCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi == 0)
    {
        itKabigonJumpSetStatus(item_gobj);
    }
    ip->multi--;

    return FALSE;
}

/* it/itmonster/itkabigon.c:269-306 itKabigonMakeItem 0x8017E6C0,
 * verbatim. The `interact_mask = GMHITCOLLISION_FLAG_FIGHTER` is Snorlax's
 * own -- a falling Snorlax hits fighters and nothing else -- and it is set
 * here rather than in the desc because itManagerMakeItem has already filled
 * the mask with GMHITCOLLISION_FLAG_ALL by this point. The anim is read
 * through the bank like the others; Snorlax's `AnimJoint` is the only
 * model-vocabulary block in the file it names (there is no
 * `llITCommonDataKabigonDataStart`). */
GObj* itKabigonMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITKabigonItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        DObj *dobj = DObjGetStruct(item_gobj);
        ITStruct *ip = itGetStruct(item_gobj);

        ip->multi = ITMONSTER_RISE_STOP_WAIT;

        ip->attack_coll.interact_mask = GMHITCOLLISION_FLAG_FIGHTER;

        ip->physics.vel_air.x = ip->physics.vel_air.z = 0.0F;
        ip->physics.vel_air.y = ITMONSTER_RISE_VEL_Y;

        gcAddDObjAnimJoint(dobj, itGetMonsterAnimNode(ip, llITCommonDataKabigonAnimJoint), 0.0F);

        if (ip->kind == nITKindKabigon)
        {
            func_800269C0_275C0(nSYAudioVoiceMBallKabigonAppear);
        }
        item_gobj->proc_display = itKabigonCommonProcDisplay;
    }
    return item_gobj;
}
