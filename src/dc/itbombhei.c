/* itbombhei.c -- it/itcommon/itbombhei.c, verbatim except one function left
 * out (see below): the Bob-omb's `ITDesc`, its nine-state `ITStatusDesc`
 * table (ground wait / air fall / fighter hold / fighter throw / fighter
 * drop / ground walk / map-collision explosion / neutral-hit explosion /
 * walk-explosion stall), the "unused?" DL-offsets table, and their proc
 * bodies. Function-for-function against the game's own ITStruct
 * (it/ittypes.h); every function names its decomp line range.
 *
 * The roster's fifteenth entry, and the last one read
 * outside the Container-item family (Box/Taru/Capsule/Egg/Poké Ball
 * use `itMainMakeContainerItem`/`itManagerInitItems`). Checked for that
 * shape: no call anywhere in this file reaches `itMainMakeContainerItem`.
 *
 * A picked-up-and-thrown Bob-omb behaves like any other thrown item
 * until it lands (`itMapCheckDestroyDropped` on a wait-landing,
 * `itMapCheckMapProcAll` on a thrown/dropped one -- the thrown path
 * detonates on ANY landing, `itBombHeiExplodeMapSetStatus`, while a
 * ground-wait landing instead starts a 180-frame countdown
 * (`ITBOMBHEI_WALK_WAIT`) before getting up and walking
 * (`itBombHeiWaitProcUpdate`/`WalkSetStatus`). `itBombHeiWalkGetLR`
 * walks `gGCCommonLinks[nGCCommonLinkIDFighter]` -- the SAME linked-list
 * shape MSBomb's own proximity walk and Bumper's own
 * AttachedProcUpdate use, but here summing a per-fighter
 * +1/-1 (which side of the Bob-omb each fighter is on) into a single
 * consensus direction, so it starts walking toward the crowded side (a
 * tie breaks via `syUtilsRandIntRange`). Once walking, it paces back and
 * forth off `itBombHeiWalkProcMap`'s own wall/floor-edge checks
 * (`itBombHeiCommonSetWalkLR`, which also swaps the model's own display
 * list to match facing) for `ITBOMBHEI_FLASH_WAIT` (480) frames, then
 * stalls for `ITBOMBHEI_EXPLODE_WAIT` (90, a FLOAT despite `ip->multi`
 * being a `u16` -- an exact integer/float comparison for values this
 * small, not a promotion trap, just an odd choice the decomp's own
 * comment already flags) with a critical-flash col-anim
 * (`itMainCheckSetColAnimID`, the same call shape Hammer's own
 * uses) before detonating on its own. Every detonation path
 * (map collision, a hit while walking, the walk timer, or a direct hit
 * anywhere else) funnels through the SAME `itBombHeiCommonSetExplode`/
 * `itBombHeiCommonClearVelSetExplode` pair -- a white sparkle burst, a
 * screen quake, a ground-dust puff if resting on a floor -- into a
 * 6-frame Explode state running a four-entry scripted hitbox sequence
 * before destroying itself outright.
 *
 * That scripted sequence (`itBombHeiCommonUpdateAttackEvent`) reads
 * through the same `itGetAttackEvent(it_desc, off)` MSBomb's own
 * uses -- `*it_desc.p_file + off`, against the dereferenced GLOBAL
 * file pointer rather than the per-item `attr->data`. Its index into the
 * four-entry table is ungated, so whatever `gITManagerCommonData` points
 * at has to be real memory at the block's own 0x46c; the host test points
 * it at its faked ITCommonData, a zeroed buffer.
 *
 * The other five offsets this file needs -- `ItemAttributes`, `DataStart`,
 * the two `DisplayList`s (`BombHeiWalkRight`/`Left`), and `MatAnimJoint
 * BombHeiWalk` -- are all confirmed declared blocks in
 * `tools/relocFileDescriptions.us.txt` and are real numbers
 * (src/dc/itemoffsets.h). The two display lists are only ever ASSIGNED to
 * `dobj->dl` (never dereferenced by anything this file's own code calls),
 * and the mat-anim-joint script is only handed to `gcAddMObjMatAnimJoint`
 * (`sys/objanim.c`, unmodified -- a plain pointer assignment, not a
 * dereference) followed by `gcPlayAnimAll`'s own opcode-gated walk, which
 * stops at the first zeroed word.
 *
 * The decomp's own top-of-file `dITBombHeiDisplayListOffsets[]` table is
 * commented "0x80189F90 - unused?" and no function in this file (or
 * anywhere else in the decomp) reads it -- ported anyway, verbatim, as
 * genuine initialized data the game's own ROM carries, the same
 * "everything else verbatim" doctrine every static table in this port
 * follows regardless of whether the port's own code happens to read it.
 *
 * `itBombHeiWaitProcUpdate`'s own two branches are asymmetric in the
 * decomp's own text: the `lr < 0` branch (walk right, `ip->lr = +1`)
 * never touches `dobj->dl` at all, while the `else` branch (walk left,
 * `ip->lr = -1`) sets it to the left display list -- so the FIRST walk
 * a Bob-omb ever takes, if it happens to be rightward, keeps whatever DL
 * it already had rather than switching to `WalkRight`. This reads like a
 * genuine quirk in the original game, not undefined behaviour or a
 * porting hazard -- ported verbatim, unfixed, per this port's own PORT-
 * not-REMAKE doctrine.
 *
 * `itBombHeiMakeItem` is ported now, and it is the decomp's line for
 * line. It was the last function in this file held out, and it was
 * held out for exactly one reason: its tail calls `ifCommonItemArrow-
 * MakeInterface(ip)`, which needed the unexported `IFCommonItem` file
 * (relocData 0x57). That file is exported -- not as a
 * pack region but as an ordinary sprite bank, `romdisk/ifcommonitem.spr`,
 * because it is one sprite and nothing else (src/dc/ifcommon.c). So the
 * tail resolves and the body is verbatim.
 *
 * The paragraph that stood here described the gap at length, including
 * why no honest refuse-first guard existed for it (`gcAddSObjForGObj`
 * never returns NULL for a NULL sprite, so the decomp's own `!= NULL`
 * check cannot catch a missing one). That reasoning was sound and the
 * blocker it argued from is now gone; it is replaced rather than kept,
 * because a note that says a ported function is left out is worse than
 * no note at all.
 */
#include <it/item.h>
#include "itemoffsets.h"     /* llITCommonData* offsets */
#include "itemmodel.h"       /* itemModelSetDisplayList */
#include <if/ifcommon.h>     /* ifCommonItemArrowMakeInterface */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

intptr_t dITBombHeiDisplayListOffsets[/* */] =
{
    (intptr_t)llITCommonDataBombHeiWalkRightDisplayList,
    (intptr_t)llITCommonDataBombHeiWalkLeftDisplayList
};

ITDesc dITBombHeiItemDesc =
{
    nITKindBombHei,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataBombHeiItemAttributes,
    { nGCMatrixKindTra, nGCMatrixKindNull, 0 },
    nGMAttackStateOff,
    itBombHeiFallProcUpdate,
    itBombHeiFallProcMap,
    NULL, NULL, NULL, NULL, NULL, NULL
};

ITStatusDesc dITBombHeiStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    { itBombHeiWaitProcUpdate, itBombHeiWaitProcMap, itBombHeiCommonProcHit,
      NULL, NULL, NULL, NULL, itBombHeiCommonProcHit },
    /* Status 1 (Air Wait Fall) */
    { itBombHeiFallProcUpdate, itBombHeiFallProcMap, NULL, NULL, NULL, NULL, NULL, itBombHeiCommonProcHit },
    /* Status 2 (Fighter Hold) */
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    /* Status 3 (Fighter Throw) */
    { itBombHeiThrownProcUpdate, itBombHeiThrownProcMap, itBombHeiCommonProcHit,
      itBombHeiCommonProcHit, itMainCommonProcHop, itBombHeiCommonProcHit, itMainCommonProcReflector, itBombHeiCommonProcHit },
    /* Status 4 (Fighter Drop) */
    { itBombHeiFallProcUpdate, itBombHeiDroppedProcMap, itBombHeiCommonProcHit,
      itBombHeiCommonProcHit, itMainCommonProcHop, itBombHeiCommonProcHit, itMainCommonProcReflector, itBombHeiCommonProcHit },
    /* Status 5 (Ground Walk) */
    { itBombHeiWalkProcUpdate, itBombHeiWalkProcMap, itBombHeiExplodeCommonProcHit,
      itBombHeiExplodeCommonProcHit, NULL, itBombHeiExplodeCommonProcHit, itBombHeiExplodeCommonProcHit, itBombHeiExplodeCommonProcHit },
    /* Status 6 (Map Collision Explosion) */
    { itBombHeiExplodeMapProcUpdate, NULL, itBombHeiExplodeMapProcUpdate,
      itBombHeiExplodeMapProcUpdate, NULL, NULL, itBombHeiExplodeMapProcUpdate, itBombHeiExplodeMapProcUpdate },
    /* Status 7 (Neutral / Hit Explosion) */
    { itBombHeiExplodeProcUpdate, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    /* Status 8 (Ground Walk Explosion Stall) */
    { itBombHeiExplodeWaitProcUpdate, itBombHeiExplodeWaitProcMap, itBombHeiExplodeCommonProcHit,
      itBombHeiExplodeCommonProcHit, NULL, itBombHeiExplodeCommonProcHit, itBombHeiExplodeCommonProcHit, itBombHeiExplodeCommonProcHit }
};

enum itBombHeiStatus
{
    nITBombHeiStatusWait,
    nITBombHeiStatusFall,
    nITBombHeiStatusHold,
    nITBombHeiStatusThrown,
    nITBombHeiStatusDropped,
    nITBombHeiStatusWalk,
    nITBombHeiStatusExplodeMap,
    nITBombHeiStatusExplode,
    nITBombHeiStatusExplodeWait,
    nITBombHeiStatusEnumCount
};

/* it/itcommon/itbombhei.c:180-207 itBombHeiCommonSetExplode 0x80177060, verbatim. */
void itBombHeiCommonSetExplode(GObj *item_gobj, u8 unused_arg)
{
    s32 unused;
    DObj *dobj = DObjGetStruct(item_gobj);
    ITStruct *ip = itGetStruct(item_gobj);
    LBParticle *pc;

    itBombHeiCommonSetHitStatusNone(item_gobj);

    pc = efManagerSparkleWhiteMultiExplodeMakeEffect(&dobj->translate.vec.f);

    if (pc != NULL)
    {
        pc->xf->scale.x = ITBOMBHEI_EXPLODE_SCALE;
        pc->xf->scale.y = ITBOMBHEI_EXPLODE_SCALE;
        pc->xf->scale.z = ITBOMBHEI_EXPLODE_SCALE;
    }
    efManagerQuakeMakeEffect(1);

    DObjGetStruct(item_gobj)->flags = DOBJ_FLAG_HIDDEN;

    ip->attack_coll.fgm_id = nSYAudioFGMExplodeL;

    itMainRefreshAttackColl(item_gobj);
    itMainClearOwnerStats(item_gobj);
    itBombHeiExplodeSetStatus(item_gobj);
}

/* it/itcommon/itbombhei.c:209-231 itBombHeiCommonSetWalkLR 0x80177104, verbatim. */
void itBombHeiCommonSetWalkLR(GObj *item_gobj, ub8 lr)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    Gfx *dll = itGetPData(ip, llITCommonDataBombHeiDataStart, llITCommonDataBombHeiWalkLeftDisplayList);
    Gfx *dlr = itGetPData(ip, llITCommonDataBombHeiDataStart, llITCommonDataBombHeiWalkRightDisplayList);

    if (lr != 0)
    {
        ip->lr = +1;
        ip->physics.vel_air.x = ITBOMBHEI_WALK_VEL_X;

        itemModelSetDisplayList(dobj, dlr);   /* DIVERGES: itemmodel.h */
    }
    else
    {
        ip->lr = -1;
        ip->physics.vel_air.x = -ITBOMBHEI_WALK_VEL_X;

        itemModelSetDisplayList(dobj, dll);   /* DIVERGES: itemmodel.h */
    }
}

/* it/itcommon/itbombhei.c:233-249 itBombHeiCommonCheckMakeDustEffect 0x80177180, verbatim. */
void itBombHeiCommonCheckMakeDustEffect(GObj *item_gobj, u8 override)
{
    s32 unused[4];
    ITStruct *ip = itGetStruct(item_gobj);
    ITAttributes *attr = ip->attr;
    DObj *dobj = DObjGetStruct(item_gobj);

    if ((ip->coll_data.mask_curr & MAP_FLAG_FLOOR) || (override != FALSE))
    {
        Vec3f pos = dobj->translate.vec.f;

        pos.y += attr->map_coll_bottom;

        efManagerDustHeavyDoubleMakeEffect(&pos, ip->lr, 1.0F);
    }
}

/* it/itcommon/itbombhei.c:251-257 itBombHeiCommonSetHitStatusNormal 0x80177208, verbatim. */
void itBombHeiCommonSetHitStatusNormal(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->damage_coll.hitstatus = nGMHitStatusNormal;
}

/* it/itcommon/itbombhei.c:259-265 itBombHeiCommonSetHitStatusNone 0x80177218, verbatim. */
void itBombHeiCommonSetHitStatusNone(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->damage_coll.hitstatus = nGMHitStatusNone;
}

/* it/itcommon/itbombhei.c:267-276 itBombHeiFallProcUpdate 0x80177224, verbatim. */
sb32 itBombHeiFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITBOMBHEI_GRAVITY, ITBOMBHEI_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itbombhei.c:278-304 itBombHeiWalkGetLR 0x80177260, verbatim. */
s32 itBombHeiWalkGetLR(GObj *item_gobj)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
    Vec3f *translate;
    s32 lr;
    s32 ret_lr = 0;
    Vec3f dist;
    DObj *item_dobj = DObjGetStruct(item_gobj);
    DObj *fighter_dobj;

    while (fighter_gobj != NULL)
    {
        translate = &item_dobj->translate.vec.f;

        fighter_dobj = DObjGetStruct(fighter_gobj);

        syVectorDiff3D(&dist, translate, &fighter_dobj->translate.vec.f);

        lr = (dist.x < 0.0F) ? -1 : +1;

        fighter_gobj = fighter_gobj->link_next;

        ret_lr += lr;
    }
    return ret_lr;
}

/* it/itcommon/itbombhei.c:306-340 itBombHeiWaitProcUpdate 0x80177304, verbatim. */
sb32 itBombHeiWaitProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);
    void *dll = itGetPData(ip, llITCommonDataBombHeiDataStart, llITCommonDataBombHeiWalkLeftDisplayList);
    s32 lr;

    if (ip->multi == ITBOMBHEI_WALK_WAIT)
    {
        lr = itBombHeiWalkGetLR(item_gobj);

        if (lr == 0)
        {
            lr = syUtilsRandIntRange(2) - 1;
        }
        if (lr < 0)
        {
            ip->lr = +1;
            ip->physics.vel_air.x = ITBOMBHEI_WALK_VEL_X;
        }
        else
        {
            ip->physics.vel_air.x = -ITBOMBHEI_WALK_VEL_X;

            itemModelSetDisplayList(dobj, dll);   /* DIVERGES: itemmodel.h */

            ip->lr = -1;
        }
        itBombHeiWalkSetStatus(item_gobj);
    }
    ip->multi++;

    return FALSE;
}

/* it/itcommon/itbombhei.c:342-348 itBombHeiWaitProcMap 0x801773F4, verbatim. */
sb32 itBombHeiWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itBombHeiFallSetStatus);

    return FALSE;
}

/* it/itcommon/itbombhei.c:350-356 itBombHeiCommonProcHit 0x8017741C, verbatim. */
sb32 itBombHeiCommonProcHit(GObj *item_gobj)
{
    itBombHeiCommonClearVelSetExplode(item_gobj, TRUE);

    return FALSE;
}

/* it/itcommon/itbombhei.c:358-362 itBombHeiFallProcMap 0x80177440, verbatim. */
sb32 itBombHeiFallProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITBOMBHEI_MAP_REBOUND_COMMON, ITBOMBHEI_MAP_REBOUND_GROUND, itBombHeiWaitSetStatus);
}

/* it/itcommon/itbombhei.c:364-370 itBombHeiWaitSetStatus 0x80177474, verbatim. */
void itBombHeiWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itBombHeiCommonSetHitStatusNormal(item_gobj);
    itMainSetStatus(item_gobj, dITBombHeiStatusDescs, nITBombHeiStatusWait);
}

/* it/itcommon/itbombhei.c:372-382 itBombHeiFallSetStatus 0x801774B0, verbatim. */
void itBombHeiFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itBombHeiCommonSetHitStatusNormal(item_gobj);
    itMainSetStatus(item_gobj, dITBombHeiStatusDescs, nITBombHeiStatusFall);
}

/* it/itcommon/itbombhei.c:384-389 itBombHeiHoldSetStatus 0x801774FC, verbatim. */
void itBombHeiHoldSetStatus(GObj *item_gobj)
{
    itBombHeiCommonSetHitStatusNone(item_gobj);
    itMainSetStatus(item_gobj, dITBombHeiStatusDescs, nITBombHeiStatusHold);
}

/* it/itcommon/itbombhei.c:391-400 itBombHeiThrownProcUpdate 0x80177530, verbatim. */
sb32 itBombHeiThrownProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITBOMBHEI_GRAVITY, ITBOMBHEI_TVEL);
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itbombhei.c:402-406 itBombHeiThrownProcMap 0x8017756C, verbatim. */
sb32 itBombHeiThrownProcMap(GObj *item_gobj)
{
    return itMapCheckMapProcAll(item_gobj, itBombHeiExplodeMapSetStatus);
}

/* it/itcommon/itbombhei.c:408-413 itBombHeiThrownSetStatus 0x80177590, verbatim. */
void itBombHeiThrownSetStatus(GObj *item_gobj)
{
    itBombHeiCommonSetHitStatusNormal(item_gobj);
    itMainSetStatus(item_gobj, dITBombHeiStatusDescs, nITBombHeiStatusThrown);
}

/* it/itcommon/itbombhei.c:415-419 itBombHeiDroppedProcMap 0x801775C4, verbatim. */
sb32 itBombHeiDroppedProcMap(GObj *item_gobj)
{
    return itMapCheckMapProcAll(item_gobj, itBombHeiExplodeMapSetStatus);
}

/* it/itcommon/itbombhei.c:421-426 itBombHeiDroppedSetStatus 0x801775E8, verbatim. */
void itBombHeiDroppedSetStatus(GObj *item_gobj)
{
    itBombHeiCommonSetHitStatusNormal(item_gobj);
    itMainSetStatus(item_gobj, dITBombHeiStatusDescs, nITBombHeiStatusDropped);
}

/* it/itcommon/itbombhei.c:428-445 itBombHeiWalkUpdateEffect 0x8017761C, verbatim. */
void itBombHeiWalkUpdateEffect(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if (ip->item_vars.bombhei.smoke_delay == 0)
    {
        Vec3f pos = dobj->translate.vec.f;

        pos.y += 120.0F;

        efManagerDustLightMakeEffect(&pos, ip->lr, 1.0F);

        ip->item_vars.bombhei.smoke_delay = ITBOMBHEI_SMOKE_WAIT;
    }
    ip->item_vars.bombhei.smoke_delay--;
}

/* it/itcommon/itbombhei.c:447-487 itBombHeiWalkProcUpdate 0x801776A0, verbatim. */
sb32 itBombHeiWalkProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    ITAttributes *attr = ip->attr;
    DObj *dobj = DObjGetStruct(item_gobj);
    Vec3f pos;

    itBombHeiWalkUpdateEffect(item_gobj);

    if (mpCollisionCheckExistLineID(ip->coll_data.floor_line_id) != FALSE)
    {
        if (ip->lr == -1)
        {
            mpCollisionGetFloorEdgeL(ip->coll_data.floor_line_id, &pos);

            if (pos.x >= (dobj->translate.vec.f.x - attr->map_coll_width))
            {
                itBombHeiCommonSetWalkLR(item_gobj, 1);
            }
        }
        else
        {
            mpCollisionGetFloorEdgeR(ip->coll_data.floor_line_id, &pos);

            if (pos.x <= (dobj->translate.vec.f.x + attr->map_coll_width))
            {
                itBombHeiCommonSetWalkLR(item_gobj, 0);
            }
        }
    }
    if (ip->multi == ITBOMBHEI_FLASH_WAIT)
    {
        ip->physics.vel_air.x = ip->physics.vel_air.y = ip->physics.vel_air.z = 0.0F;

        itBombHeiExplodeWaitSetStatus(item_gobj);
    }
    ip->multi++;

    return FALSE;
}

/* it/itcommon/itbombhei.c:489-505 itBombHeiWalkProcMap 0x801777D8, verbatim. */
sb32 itBombHeiWalkProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMapCheckLRWallProcNoFloor(item_gobj, itBombHeiDroppedSetStatus);

    if (ip->coll_data.mask_curr & MAP_FLAG_LWALL)
    {
        itBombHeiCommonSetWalkLR(item_gobj, 0);
    }
    if (ip->coll_data.mask_curr & MAP_FLAG_RWALL)
    {
        itBombHeiCommonSetWalkLR(item_gobj, 1);
    }
    return FALSE;
}

/* it/itcommon/itbombhei.c:507-554 itBombHeiWalkInitVars 0x80177848, verbatim. */
void itBombHeiWalkInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    ITAttributes *attr = ip->attr;
    DObj *dobj = DObjGetStruct(item_gobj);
    AObjEvent32 *matanim_joint;
    s32 unused;
    Vec3f pos;

    ip->is_allow_pickup = FALSE;

    ip->multi = 0;

    ip->item_vars.bombhei.smoke_delay = ITBOMBHEI_SMOKE_WAIT;

    itMainRefreshAttackColl(item_gobj);

    matanim_joint = itGetPData(ip, llITCommonDataBombHeiDataStart, llITCommonDataBombHeiWalkMatAnimJoint);

    gcAddMObjMatAnimJoint(dobj->mobj, matanim_joint, 0.0F);
    gcPlayAnimAll(item_gobj);

    if (mpCollisionCheckExistLineID(ip->coll_data.floor_line_id) != FALSE)
    {
        if (ip->lr == -1)
        {
            mpCollisionGetFloorEdgeL(ip->coll_data.floor_line_id, &pos);

            if (pos.x >= (dobj->translate.vec.f.x - attr->map_coll_width))
            {
                itBombHeiCommonSetWalkLR(item_gobj, 1);
            }
        }
        else
        {
            mpCollisionGetFloorEdgeR(ip->coll_data.floor_line_id, &pos);

            if (pos.x <= (dobj->translate.vec.f.x + attr->map_coll_width))
            {
                itBombHeiCommonSetWalkLR(item_gobj, 0);
            }
        }
    }
    itMainClearOwnerStats(item_gobj);

    func_800269C0_275C0(nSYAudioFGMBombHeiFuse);
}

/* it/itcommon/itbombhei.c:556-562 itBombHeiWalkSetStatus 0x801779A8, verbatim. */
void itBombHeiWalkSetStatus(GObj *item_gobj)
{
    itBombHeiCommonSetHitStatusNormal(item_gobj);
    itBombHeiWalkInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITBombHeiStatusDescs, nITBombHeiStatusWalk);
}

/* it/itcommon/itbombhei.c:564-574 itBombHeiCommonClearVelSetExplode 0x801779E4, verbatim. */
void itBombHeiCommonClearVelSetExplode(GObj *item_gobj, u8 unused)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->physics.vel_air.x = ip->physics.vel_air.y = ip->physics.vel_air.z = 0.0F;

    itBombHeiCommonSetExplode(item_gobj, unused);

    func_800269C0_275C0(nSYAudioFGMExplodeL);
}

/* it/itcommon/itbombhei.c:576-602 itBombHeiCommonUpdateAttackEvent (unaddressed
 * in the decomp's own comments), verbatim. */
void itBombHeiCommonUpdateAttackEvent(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    ITAttackEvent *ev = itGetAttackEvent(dITBombHeiItemDesc, llITCommonDataBombHeiAttackEvents);

    if (ip->multi == ev[ip->event_id].timer)
    {
        ip->attack_coll.angle = ev[ip->event_id].angle;
        ip->attack_coll.damage = ev[ip->event_id].damage;
        ip->attack_coll.size = ev[ip->event_id].size;

        ip->attack_coll.can_rehit_item = TRUE;
        ip->attack_coll.can_hop = FALSE;
        ip->attack_coll.can_reflect = FALSE;
        ip->attack_coll.can_setoff = FALSE;

        ip->attack_coll.element = nGMHitElementFire;

        ip->event_id++;

        if (ip->event_id == 4)
        {
            ip->event_id = 3;
        }
    }
}

/* it/itcommon/itbombhei.c:604-611 itBombHeiExplodeMapProcUpdate 0x80177B10, verbatim. */
sb32 itBombHeiExplodeMapProcUpdate(GObj *item_gobj)
{
    itBombHeiCommonCheckMakeDustEffect(item_gobj, FALSE);
    itBombHeiCommonClearVelSetExplode(item_gobj, TRUE);

    return FALSE;
}

/* it/itcommon/itbombhei.c:613-620 itBombHeiExplodeCommonProcHit 0x80177B44, verbatim. */
sb32 itBombHeiExplodeCommonProcHit(GObj *item_gobj)
{
    itBombHeiCommonCheckMakeDustEffect(item_gobj, TRUE);
    itBombHeiCommonClearVelSetExplode(item_gobj, FALSE);

    return FALSE;
}

/* it/itcommon/itbombhei.c:622-627 itBombHeiExplodeMapSetStatus 0x80177B78, verbatim. */
void itBombHeiExplodeMapSetStatus(GObj *item_gobj)
{
    itBombHeiCommonSetHitStatusNormal(item_gobj);
    itMainSetStatus(item_gobj, dITBombHeiStatusDescs, nITBombHeiStatusExplodeMap);
}

/* it/itcommon/itbombhei.c:629-641 itBombHeiExplodeInitVars 0x80177BAC, verbatim. */
void itBombHeiExplodeInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi = 0;

    ip->attack_coll.throw_mul = ITEM_THROW_DEFAULT;

    ip->event_id = 0;

    itBombHeiCommonUpdateAttackEvent(item_gobj);
}

/* it/itcommon/itbombhei.c:643-657 itBombHeiExplodeProcUpdate 0x80177BE8, verbatim. */
sb32 itBombHeiExplodeProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itBombHeiCommonUpdateAttackEvent(item_gobj);

    ip->multi++;

    if (ip->multi == ITBOMBHEI_EXPLODE_LIFETIME)
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itcommon/itbombhei.c:659-664 itBombHeiExplodeSetStatus 0x80177C30, verbatim. */
void itBombHeiExplodeSetStatus(GObj *item_gobj)
{
    itBombHeiExplodeInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITBombHeiStatusDescs, nITBombHeiStatusExplode);
}

/* it/itcommon/itbombhei.c:666-682 itBombHeiExplodeWaitProcUpdate 0x80177C64, verbatim. */
sb32 itBombHeiExplodeWaitProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itBombHeiWalkUpdateEffect(item_gobj);

    if (ip->multi == ITBOMBHEI_EXPLODE_WAIT)
    {
        itBombHeiCommonCheckMakeDustEffect(item_gobj, TRUE);
        itBombHeiCommonClearVelSetExplode(item_gobj, 0);
        func_800269C0_275C0(nSYAudioFGMExplodeL);
    }
    ip->multi++;

    return FALSE;
}

/* it/itcommon/itbombhei.c:684-690 itBombHeiExplodeWaitProcMap 0x80177D00, verbatim. */
sb32 itBombHeiExplodeWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itBombHeiDroppedSetStatus);

    return FALSE;
}

/* it/itcommon/itbombhei.c:692-703 itBombHeiExplodeWaitInitVars 0x80177D28, verbatim. */
void itBombHeiExplodeWaitInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    ip->multi = 0;

    dobj->mobj->matanim_joint.event32 = NULL;

    itMainCheckSetColAnimID(item_gobj, nGMColAnimItemBombHeiCritical, ITBOMBHEI_EXPLODE_COLANIM_DURATION);
}

/* it/itcommon/itbombhei.c:705-711 itBombHeiExplodeWaitSetStatus 0x80177D60, verbatim. */
void itBombHeiExplodeWaitSetStatus(GObj *item_gobj)
{
    itBombHeiCommonSetHitStatusNormal(item_gobj);
    itBombHeiExplodeWaitInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITBombHeiStatusDescs, nITBombHeiStatusExplodeWait);
}

/* it/itcommon/itbombhei.c:713-752 itBombHeiMakeItem 0x801779D9C -- left out:
 * ends with `ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);`, the
 * IFCommonItem export dependency every roster item's own MakeItem
 * shares. */

/* it/itcommon/itbombhei.c:714-752 itBombHeiMakeItem 0x80177D9C, verbatim. */
GObj* itBombHeiMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITBombHeiItemDesc, pos, vel, flags);
    DObj *dobj;
    ITStruct *ip;
#if defined(REGION_US)
    Vec3f translate;
#endif

    if (item_gobj != NULL)
    {
        dobj = DObjGetStruct(item_gobj);

#if defined(REGION_US)
        translate = dobj->translate.vec.f;
#endif

        ip = itGetStruct(item_gobj);

        ip->multi = 0;

        itMainClearOwnerStats(item_gobj);

        gcAddXObjForDObjFixed(dobj, 0x2E, 0);

#if defined(REGION_US)
        dobj->translate.vec.f = translate;
#else
        dobj->translate.vec.f = *pos;
#endif

        ip->is_unused_item_bool = TRUE;

        dobj->rotate.vec.f.z = 0.0F;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}
