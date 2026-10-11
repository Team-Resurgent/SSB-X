/* itbox.c -- it/itcommon/itbox.c, verbatim: the Crate's `ITDesc`, its
 * six-state `ITStatusDesc` table (ground wait / air fall / fighter hold /
 * fighter throw / fighter drop / neutral explosion) and their proc
 * bodies, plus the smash effect the Containers share.
 * Function-for-function against the game's own ITStruct (it/ittypes.h);
 * every function names its decomp line range.
 *
 * The third Container, and the one that carries a piece of
 * the subsystem the other three only borrow.
 *
 * THE SMASH EFFECT IS SHARED, and this is where it lives.
 * `itBoxContainerSmashMakeEffect` is what a Crate AND a Barrel play when
 * they break: ITCONTAINER_EFFECT_COUNT DObjs, each with its own random
 * scale and spin, run by a custom per-frame process for
 * ITCONTAINER_GFX_LIFETIME frames. ittaru.c reaches it by name, so it is
 * ported here once and both files use it.
 *
 * THE ONE DIVERGES. The decomp reads a raw display list out of the item
 * file -- `attr->data - BoxDataStart + BoxEffectDisplayList`, an
 * `itGetPData`-shaped expression over an `ITAttributes` read straight off
 * region 0 -- and hands that `Gfx*` to every piece. The port draws nothing
 * from a display list (gcSubmitDObj submits `dobj->dv`, the baked model),
 * so `EffectDisplayList Box` is baked instead: it is one block, 3,416
 * bytes at 0x68f0 of ITCommonObject, and tools/export/ssb_itemmodelexport.py's
 * `--effect boxsmash` bakes it as a one-joint model -- the same graft
 * shape the stage exporter uses for a map object with no DObjDesc of its
 * own. Each piece then gets that joint's payload through
 * dc_model_hidden_payload, which is the accessor src/dc/objmodel.h has for
 * a DObj made outside dc_model_add_dobjs. Four verts and two triangles:
 * a debris quad.
 *
 * The rest is the Containers' usual machine. `itBoxCommonProcDamage` is
 * this one's own: a Crate TAKES damage (ITBOX_HEALTH_MAX, 15) and breaks
 * on the hit that fills it, where Capsule and Egg break on any hit at
 * all. `itBoxCommonCheckSpawnItems` is the richest drop in the four --
 * one, two or three items out of `dITBoxItemSpawnVelocities`, with a
 * 1-in-32 chance of them being identical and the rest of the time each
 * one re-rolled against a weights table with its last entry removed, so
 * a Crate never gives the same item twice in a row.
 *
 * `func_ovl3_801798B8` is the decomp's own "Unused" marker, kept for the
 * reason every other dead roster function is.
 */
#include <it/item.h>
#include <sc/scene.h>           /* gSCManagerBattleState */
#include <if/ifcommon.h>        /* ifCommonItemArrowMakeInterface */
#include <ef/effect.h>          /* efManagerQuakeMakeEffect, EFStruct */
#include <ef/efmanager.h>       /* efManagerGetEffectNoForce,
                                   efManagerSparkleWhiteMultiExplodeMakeEffect */

#include "itemoffsets.h"        /* llITCommonData* offsets */
#include "itemmodel.h"          /* the baked models */
#include "objmodel.h"           /* dc_model_hidden_payload */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/itbox.c:11-19 dITBoxItemSpawnVelocities 0x8018A320,
 * verbatim: six rows, and itBoxCommonCheckSpawnItems picks the group by
 * how many items it is about to drop (1, 2 or 3, from indices 0, 1 and
 * 3). The y is 48 for all six and the x spreads them. */
Vec2f dITBoxItemSpawnVelocities[/* */] =
{
    {  0.0F, 48.0F },
    { -2.0F, 48.0F },
    {  2.0F, 48.0F },
    { -5.0F, 48.2F },
    {  0.0F, 48.2F },
    {  5.2F, 48.2F }
};

/* it/itcommon/itbox.c:22-42 dITBoxItemDesc 0x8018A350, verbatim. Its
 * proc_damage is NULL and the status table carries the real one: a Crate
 * only takes damage in the states that name itBoxCommonProcDamage. */
ITDesc dITBoxItemDesc =
{
    nITKindBox,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataBoxItemAttributes,

    {
        nGCMatrixKindTraRotRpyR,
        nGCMatrixKindNull,
        0
    },

    nGMAttackStateOff,
    itBoxFallProcUpdate,
    itBoxFallProcMap,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/* it/itcommon/itbox.c:45-125 dITBoxStatusDescs 0x8018A384, verbatim. */
ITStatusDesc dITBoxStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    {
        NULL,
        itBoxWaitProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        itBoxCommonProcDamage
    },

    /* Status 1 (Air Wait Fall) */
    {
        itBoxFallProcUpdate,
        itBoxFallProcMap,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 2 (Fighter Hold) */
    {
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* Status 3 (Fighter Throw) */
    {
        itBoxFallProcUpdate,
        itBoxThrownProcMap,
        itBoxCommonProcHit,
        itBoxCommonProcHit,
        NULL,
        itBoxCommonProcHit,
        itBoxCommonProcHit,
        itBoxCommonProcDamage
    },

    /* Status 4 (Fighter Drop) */
    {
        itBoxFallProcUpdate,
        itBoxDroppedProcMap,
        itBoxCommonProcHit,
        itBoxCommonProcHit,
        NULL,
        itBoxCommonProcHit,
        itBoxCommonProcHit,
        itBoxCommonProcDamage
    },

    /* Status 5 (Neutral Explosion) */
    {
        itBoxExplodeProcUpdate,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    }
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itBoxStatus
{
    nITBoxStatusWait,
    nITBoxStatusFall,
    nITBoxStatusHold,
    nITBoxStatusThrown,
    nITBoxStatusDropped,
    nITBoxStatusExplode,
    nITBoxStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itcommon/itbox.c:145-172 itBoxContainerSmashUpdateEffect 0x80179120,
 * verbatim: the debris pieces drift up and apart, shrinking, for
 * ITCONTAINER_GFX_LIFETIME frames. The decomp's own comments on the three
 * rotation lines ("??? Seems to be rotation step, but only in this
 * case?") are its own; the fields are DObj.anim_wait/anim_speed/anim_frame
 * reused as per-piece spin rates, which is why nothing else touches them
 * here. */
void itBoxContainerSmashUpdateEffect(GObj *effect_gobj) /* Barrel/Crate smash GFX process */
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

/* it/itcommon/itbox.c:174-216 itBoxContainerSmashMakeEffect 0x801791F4,
 * with the one DIVERGES the file header describes: the decomp computes a
 * raw `Gfx*` and gives it to every piece, the port gives each one the
 * baked model's joint-0 payload. Everything else -- the EFStruct, the
 * display proc, the count, the random scale and spin, the lifetime, the
 * process -- is the decomp's line for line. */
void itBoxContainerSmashMakeEffect(Vec3f *pos)
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

            /* the DIVERGES: one baked debris quad, shared by every piece.
             * NULL means the pack would not load, and then the pieces are
             * made bare -- visible as nothing, which is what the decomp's
             * own raw `dl` amounts to on the port anyway.
             *
             * The FIRST piece goes through dc_model_add_dobjs and the
             * rest take joint 0's payload, because that call is what
             * ALLOCATES the model's display table: dc_model_hidden_payload
             * reads `model->disp`, and before a tree has been built from a
             * pack that pointer is NULL. (The host test found
             * that -- seven pieces came out with four having no payload.)
             * The tree dc_model_add_dobjs builds for a one-joint model is
             * one DObj on the GObj, so all seven are siblings either way,
             * which is the shape the update process's `dobj->sib_next`
             * walk expects. */
            pack = itemModelBoxSmash();

            if ((pack != NULL) &&
                (dc_model_add_dobjs(effect_gobj, NULL, pack, NULL) < 0))
            {
                pack = NULL;
            }
            for (i = 0; i < ITCONTAINER_EFFECT_COUNT; i++)
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
            ep->effect_vars.container.lifetime = ITCONTAINER_GFX_LIFETIME;

            effect_gobj->user_data.p = ep;

            gcAddGObjProcess(effect_gobj, itBoxContainerSmashUpdateEffect, 1, 3);
        }
    }
}

/* it/itcommon/itbox.c:218-310 itBoxCommonCheckSpawnItems 0x80179424,
 * verbatim: the richest drop in the four Containers. One, two or three
 * items (a 2-in-5, 1-in-5, 2-in-5 split), the first kind rolled against
 * the full weights table and each later one against a table with its last
 * entry REMOVED -- so the same kind cannot come twice -- unless a 1-in-32
 * roll says the whole batch is identical, in which case they all are.
 * The removed entry is put back before returning, which is why the
 * valids_num/weights_sum save and restore are here at all. */
sb32 itBoxCommonCheckSpawnItems(GObj *item_gobj)
{
    s32 random, spawn_item_num, kind;
    s32 i, j;
    Vec2f *spawn_pos;
    Vec3f vel_identical;
    s32 unused;
    s32 weights_sum;
    s32 item_count;
    Vec3f vel_different;

    func_800269C0_275C0(nSYAudioFGMContainerSmash);

    itBoxContainerSmashMakeEffect(&DObjGetStruct(item_gobj)->translate.vec.f);

    if (gITManagerRandomWeights.weights_sum != 0)
    {
        kind = itMainGetWeightedItemKind(&gITManagerRandomWeights);

        if (kind <= nITKindCommonEnd)
        {
            random = syUtilsRandIntRange(5);

            if (random < 2)
            {
                spawn_item_num = 1;

                spawn_pos = &dITBoxItemSpawnVelocities[0];
            }
            else if (random < 3)
            {
                spawn_item_num = 2;

                spawn_pos = &dITBoxItemSpawnVelocities[1];
            }
            else
            {
                spawn_item_num = 3;

                spawn_pos = &dITBoxItemSpawnVelocities[3];
            }
            if (syUtilsRandIntRange(32) == 0) /* 1 in 32 chance to spawn identical items */
            {
                vel_identical.z = 0.0F;

                for (i = 0; i < spawn_item_num; i++)
                {
                    vel_identical.x = spawn_pos[i].x;
                    vel_identical.y = spawn_pos[i].y;

                    itManagerMakeItemSetupCommon(item_gobj, kind, &DObjGetStruct(item_gobj)->translate.vec.f, &vel_identical, (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_ITEM));
                }
            }
            else
            {
                weights_sum = gITManagerRandomWeights.weights_sum;
                item_count = gITManagerRandomWeights.valids_num - 1;

                gITManagerRandomWeights.weights_sum = gITManagerRandomWeights.blocks[item_count];
                gITManagerRandomWeights.valids_num--;

                vel_different.z = 0.0F;

                for (j = 0; j < spawn_item_num; j++)
                {
                    if (j != 0)
                    {
                        kind = itMainGetWeightedItemKind(&gITManagerRandomWeights);
                    }
                    vel_different.x = spawn_pos[j].x;
                    vel_different.y = spawn_pos[j].y;

                    itManagerMakeItemSetupCommon(item_gobj, kind, &DObjGetStruct(item_gobj)->translate.vec.f, &vel_different, (ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_ITEM));
                }
                gITManagerRandomWeights.valids_num++;
                gITManagerRandomWeights.weights_sum = weights_sum;
            }
            func_800269C0_275C0(nSYAudioFGMFireFlowerShoot);

            return TRUE;
        }
    }
    return FALSE;
}

/* it/itcommon/itbox.c:312-322 itBoxFallProcUpdate 0x8017963C, verbatim. */
sb32 itBoxFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITBOX_GRAVITY, ITBOX_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itbox.c:324-330 itBoxWaitProcMap 0x80179674, verbatim. */
sb32 itBoxWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itBoxFallSetStatus);

    return FALSE;
}

/* it/itcommon/itbox.c:332-342 itBoxCommonProcHit 0x8017969C, verbatim. */
sb32 itBoxCommonProcHit(GObj *item_gobj)
{
    if (itBoxCommonCheckSpawnItems(item_gobj) != FALSE)
    {
        return TRUE;
    }
    else itBoxExplodeMakeEffectGotoSetStatus(item_gobj);

    return FALSE;
}

/* it/itcommon/itbox.c:344-355 itBoxCommonProcDamage 0x801796D8, verbatim.
 * The Crate's own: it has hit points, and breaks on the hit that fills
 * them. Capsule and Egg have no equivalent -- any hit breaks those. */
sb32 itBoxCommonProcDamage(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->percent_damage >= ITBOX_HEALTH_MAX)
    {
        return itBoxCommonProcHit(item_gobj);
    }
    else return FALSE;
}

/* it/itcommon/itbox.c:357-361 itBoxFallProcMap 0x80179718, verbatim. */
sb32 itBoxFallProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITBOX_MAP_REBOUND_COMMON, ITBOX_MAP_REBOUND_GROUND, itBoxWaitSetStatus);
}

/* it/itcommon/itbox.c:363-374 itBoxWaitSetStatus 0x80179748, verbatim.
 * The first line is the Crate's own: it settles onto the floor's ANGLE,
 * where Capsule and Egg stay upright. */
void itBoxWaitSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    DObjGetStruct(item_gobj)->rotate.vec.f.z = syUtilsArcTan2(ip->coll_data.floor_angle.y, ip->coll_data.floor_angle.x) - F_CST_DTOR32(90.0F);

    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITBoxStatusDescs, nITBoxStatusWait);
}

/* it/itcommon/itbox.c:376-386 itBoxFallSetStatus 0x801797A4, verbatim. */
void itBoxFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITBoxStatusDescs, nITBoxStatusFall);
}

/* it/itcommon/itbox.c:388-394 itBoxHoldSetStatus 0x801797E8, verbatim. */
void itBoxHoldSetStatus(GObj *item_gobj)
{
    DObjGetStruct(item_gobj)->child->rotate.vec.f.z = 0.0F;
    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = 0.0F;

    itMainSetStatus(item_gobj, dITBoxStatusDescs, nITBoxStatusHold);
}

/* it/itcommon/itbox.c:396-407 itBoxThrownProcMap 0x8017982C, verbatim. */
sb32 itBoxThrownProcMap(GObj *item_gobj)
{
    if (itMapTestAllCollisionFlag(item_gobj, MAP_FLAG_MAIN_MASK) != FALSE)
    {
        if (itBoxCommonCheckSpawnItems(item_gobj) != FALSE)
        {
            return TRUE;
        }
        else itBoxExplodeMakeEffectGotoSetStatus(item_gobj);
    }
    return FALSE;
}

/* it/itcommon/itbox.c:409-415 itBoxThrownSetStatus 0x8017987C, verbatim. */
void itBoxThrownSetStatus(GObj *item_gobj)
{
    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = F_CST_DTOR32(90.0F);

    itMainSetStatus(item_gobj, dITBoxStatusDescs, nITBoxStatusThrown);
}

/* it/itcommon/itbox.c:417-423 func_ovl3_801798B8 0x801798B8, verbatim. The
 * decomp marks it Unused; nothing in this file names it. */
sb32 func_ovl3_801798B8(GObj *item_gobj)
{
    itMainVelSetRebound(item_gobj);

    return FALSE;
}

/* it/itcommon/itbox.c:425-429 itBoxDroppedProcMap 0x801798DC, verbatim. */
sb32 itBoxDroppedProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITBOX_MAP_REBOUND_COMMON, ITBOX_MAP_REBOUND_GROUND, itBoxWaitSetStatus);
}

/* it/itcommon/itbox.c:431-438 itBoxDroppedSetStatus 0x8017990C, verbatim. */
void itBoxDroppedSetStatus(GObj *item_gobj)
{
    DObjGetStruct(item_gobj)->child->rotate.vec.f.y = F_CST_DTOR32(90.0F);

    itMainSetStatus(item_gobj, dITBoxStatusDescs, nITBoxStatusDropped);
}

/* it/itcommon/itbox.c:440-452 itBoxExplodeProcUpdate 0x80179948,
 * verbatim: eight frames of the Box's own AttackEvents row (0x614). */
sb32 itBoxExplodeProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi++;

    if (ip->multi == ITBOX_EXPLODE_FRAME_END)
    {
        return TRUE;
    }
    else itMainUpdateAttackEvent(item_gobj, itGetAttackEvent(dITBoxItemDesc, llITCommonDataBoxAttackEvents));

    return FALSE;
}

/* it/itcommon/itbox.c:454-471 itBoxMakeItem 0x801799A4, verbatim. Note
 * `is_damage_all = TRUE`: a Crate is damageable the moment it exists,
 * where Capsule and Egg only become so once falling. */
GObj* itBoxMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITBoxItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        DObjGetStruct(item_gobj)->rotate.vec.f.y = F_CST_DTOR32(90.0F);

        ip->is_damage_all = TRUE;
        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}

/* it/itcommon/itbox.c:473-501 itBoxExplodeInitVars 0x80179A34, verbatim. */
void itBoxExplodeInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->event_id = 0;
    ip->multi = 0;

    ip->attack_coll.fgm_id = nSYAudioFGMExplodeL;

    ip->attack_coll.can_rehit_item = TRUE;
    ip->attack_coll.can_hop = FALSE;
    ip->attack_coll.can_reflect = FALSE;

    ip->attack_coll.throw_mul = ITEM_THROW_DEFAULT;
    ip->attack_coll.element = nGMHitElementFire;

    ip->attack_coll.can_setoff = FALSE;

    ip->damage_coll.hitstatus = nGMHitStatusNone;

    itMainClearOwnerStats(item_gobj);
    itMainRefreshAttackColl(item_gobj);
    itMainUpdateAttackEvent(item_gobj, itGetAttackEvent(dITBoxItemDesc, llITCommonDataBoxAttackEvents));
}

/* it/itcommon/itbox.c:503-508 itBoxExplodeSetStatus 0x80179AD4, verbatim. */
void itBoxExplodeSetStatus(GObj *item_gobj)
{
    itBoxExplodeInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITBoxStatusDescs, nITBoxStatusExplode);
}

/* it/itcommon/itbox.c:510-534 itBoxExplodeMakeEffectGotoSetStatus
 * 0x80179B08, verbatim. The decomp's three-line assignment to
 * `pc->xf->scale` is written as three statements here, which is what it
 * means; the value is the same. */
void itBoxExplodeMakeEffectGotoSetStatus(GObj *item_gobj)
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
        pc->xf->scale.x = ITBOX_EXPLODE_SCALE;
        pc->xf->scale.y = ITBOX_EXPLODE_SCALE;
        pc->xf->scale.z = ITBOX_EXPLODE_SCALE;
    }
    efManagerQuakeMakeEffect(1);

    DObjGetStruct(item_gobj)->flags = DOBJ_FLAG_HIDDEN;

    itBoxExplodeSetStatus(item_gobj);
}
