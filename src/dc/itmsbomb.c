/* itmsbomb.c -- it/itcommon/itmsbomb.c, verbatim except one function left
 * out (see below): the Motion-Sensor Bomb's `ITDesc`, its eight-state
 * `ITStatusDesc` table (ground wait / air fall / fighter hold / fighter
 * throw / fighter drop / ground attach / air detach / explode), and their
 * proc bodies. Function-for-function against the game's own ITStruct
 * (it/ittypes.h); every function names its decomp line range.
 *
 * The roster's fourteenth entry. Checked against the
 * Container-item finding: no call anywhere in
 * this file reaches `itMainMakeContainerItem`.
 *
 * A picked-up-and-thrown MSBomb behaves like any other thrown item until
 * it lands on a floor/ceiling/wall (`itMSBombThrownProcMap`/
 * `itMSBombDroppedProcMap`, both `itMapCheckMapProcAll` into
 * `itMSBombAttachedSetStatus` -- unlike Bumper, EVERY landing surface
 * attaches, not just floors), where it pins itself with
 * `itMSBombAttachedUpdateSurface` (the same `syUtilsArcTan2`-on-collision-
 * normal shape as Bumper's own `AttachedSetModelPitch`, but picking
 * ceil/floor/lwall/rwall out of `coll_data->mask_curr` instead of assuming
 * floor) and starts a `ITMSBOMB_DETECT_FIGHTER_DELAY`-frame arming clock
 * (`ip->multi`). Once armed, `itMSBombAttachedProcUpdate` walks
 * `gGCCommonLinks[nGCCommonLinkIDFighter]` every frame -- the SAME linked-
 * list proximity shape Bumper's own `AttachedProcUpdate` uses, but
 * there it drove a bounce and here a squared-distance hit inside
 * `ITMSBOMB_DETECT_FIGHTER_RADIUS` (`syVectorDiff3D` against each
 * fighter's `map_coll.top`-adjusted centre) detonates the bomb
 * (`itMSBombExplodeInitStatusVars`). If the surface it was pinned to goes
 * away first (`itMSBombAttachedProcMap`'s own
 * `mpCollisionCheckExistLineID`), it falls into a second, DETACHED arming
 * state (`itMSBombDetachedProcUpdate`) that re-applies gravity while
 * running the identical fighter-proximity walk -- so the bomb can still
 * detonate mid-air, not just pinned. Either path's detonation
 * (`itMSBombExplodeInitStatusVars`) plays a ground-dust puff if resting on
 * a floor, a white sparkle burst, a screen quake, and refreshes the
 * item's own attack hitbox before hiding the model and switching to the
 * timed Explode state, which runs a four-entry scripted hitbox sequence
 * (below) for `ITMSBOMB_EXPLODE_LIFETIME` (16) frames before destroying
 * itself (returns TRUE).
 *
 * `itMSBombExplodeUpdateAttackEvent` reads through a FOURTH kind of
 * reloc-offset arithmetic, distinct from Green/Red Shell's and Bumper's
 * own `itGetPData(ip, off1, off2)` (a delta between two offsets within
 * the SAME file, `(ip->attr->data - off1) + off2`): `itGetAttackEvent(it_desc,
 * off)` (it/item.h) is `(ITAttackEvent*)(*(it_desc).p_file + off)` -- a
 * SIMPLE addition against the DEREFERENCED GLOBAL file pointer
 * (`*dITMSBombItemDesc.p_file`, i.e. `gITManagerCommonData`), not the
 * per-item `attr->data`.
 *
 * Both of this file's own offsets were confirmed to be real, declared blocks
 * in `tools/relocFileDescriptions.us.txt`'s "FILE CONTENTS" section and are
 * carried as numbers (src/dc/itemoffsets.h). `AttackEvents MSBomb` is 0x404
 * and `ItemAttributes MSBomb` 0x3bc, both in ITCommonData.
 *
 * The read itself does an UNCONDITIONAL, non-gated array index
 * (`ev[ip->event_id]`, four fields read straight out) every time it
 * runs, so whatever `gITManagerCommonData` points at has to be real
 * memory at 0x404. The host test points it at src/dc/hosttest's own
 * faked ITCommonData -- a zeroed 0x2000-byte buffer -- which is what
 * keeps that read in range.
 *
 * `itMSBombAttachedInitVars` reads `ftGetStruct(fighter_gobj)` to call
 * `ftParamMakeRumble`, and `itMSBombAttachedProcUpdate`/
 * `DetachedProcUpdate` both read `ftGetStruct(fighter_gobj)->attr->
 * map_coll.top` for every fighter on the proximity walk -- `ftGetStruct`
 * is a macro (`ft/fighter.h`), not a linked symbol, so the include is
 * required here from the start.
 *
 * `itMSBombAttachedProcMap` calls `mpCollisionCheckExistLineID(ip->
 * attach_line_id)` with NO outer `itMapCheck*` gate ahead of it -- unlike
 * every other `attach_line_id` read in this port (Bumper's own
 * `AttachedProcMap`), which is always behind an
 * `itMapCheckLRWallProcNoFloor` that short-circuits first on the honest
 * "no collision at all" test fixture every itMap-adjacent test in this
 * port uses. `attach_line_id` is a `u16` field
 * (it/ittypes.h) -- the usual int-literal `-2` sentinel that fixture
 * relies on (`mpCollisionCheckExistLineID`'s own debug branches) can
 * never be expressed through it, since a `u16` promotes to an `int` in
 * [0,65535] under `==`, never to -1 or -2 -- the SAME shape as this
 * port's own u8/u16 `!= -1` promotion trap, but surfacing
 * through an argument passed into another function's sentinel check
 * rather than through a direct comparison. This file's own host test
 * proves the branch with a real, minimal one-entry
 * `MPVertexInfoContainer`/`MPYakumonoDObj` pair instead of the
 * unreachable sentinel.
 *
 * `itMSBombMakeItem` is the decomp's line for line. Its tail calls
 * `ifCommonItemArrow-MakeInterface(ip)`, which reads the `IFCommonItem` file
 * (relocData 0x57), carried not as a
 * pack region but as an ordinary sprite bank, `romdisk/ifcommonitem.spr`,
 * because it is one sprite and nothing else (src/dc/ifcommon.c). So the
 * tail resolves and the body is verbatim. */
#include <it/item.h>
#include "itemoffsets.h"     /* llITCommonData* offsets */
#include <if/ifcommon.h>     /* ifCommonItemArrowMakeInterface */
#include <ft/fighter.h>
#include <sc/scene.h>

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

ITDesc dITMSBombItemDesc =
{
    nITKindMSBomb,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataMSBombItemAttributes,
    { nGCMatrixKindNull, nGCMatrixKindNull, 0 },
    nGMAttackStateOff,
    itMSBombFallProcUpdate,
    itMSBombFallProcMap,
    NULL, NULL, NULL, NULL, NULL, NULL
};

ITStatusDesc dITMSBombStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    { NULL, itMSBombWaitProcMap, NULL, NULL, NULL, NULL, NULL, NULL },
    /* Status 1 (Air Wait Fall) */
    { itMSBombFallProcUpdate, itMSBombFallProcMap, NULL, NULL, NULL, NULL, NULL, NULL },
    /* Status 2 (Fighter Hold) */
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    /* Status 3 (Fighter Throw) */
    { itMSBombThrownProcUpdate, itMSBombThrownProcMap, itMSBombCommonProcHit,
      itMSBombCommonProcHit, itMainCommonProcHop, itMSBombCommonProcHit, itMainCommonProcReflector, NULL },
    /* Status 4 (Fighter Drop) */
    { itMSBombFallProcUpdate, itMSBombDroppedProcMap, itMSBombCommonProcHit,
      itMSBombCommonProcHit, itMainCommonProcHop, itMSBombCommonProcHit, itMainCommonProcReflector, NULL },
    /* Status 5 (Ground Attach) */
    { itMSBombAttachedProcUpdate, itMSBombAttachedProcMap, NULL, NULL, NULL, NULL, NULL, itMSBombCommonProcDamage },
    /* Status 6 (Air Detach from Surface) */
    { itMSBombDetachedProcUpdate, itMSBombDroppedProcMap, NULL, NULL, NULL, NULL, NULL, itMSBombCommonProcDamage },
    /* Status 7 (Neutral Explosion) */
    { itMSBombExplodeProcUpdate, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

enum itMSBombStatus
{
    nITMSBombStatusWait,
    nITMSBombStatusFall,
    nITMSBombStatusHold,
    nITMSBombStatusThrown,
    nITMSBombStatusDropped,
    nITMSBombStatusAttached,
    nITMSBombStatusDetached,
    nITMSBombStatusExplode,
    nITMSBombStatusEnumCount
};

/* it/itcommon/itmsbomb.c:161-173 itMSBombFallProcUpdate 0x80176450, verbatim. */
sb32 itMSBombFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITMSBOMB_GRAVITY, ITMSBOMB_TVEL);
    itVisualsUpdateSpin(item_gobj);

    dobj->child->sib_next->rotate.vec.f.z = dobj->rotate.vec.f.z;

    return FALSE;
}

/* it/itcommon/itmsbomb.c:175-181 itMSBombWaitProcMap 0x801764A8, verbatim. */
sb32 itMSBombWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itMSBombFallSetStatus);

    return FALSE;
}

/* it/itcommon/itmsbomb.c:183-187 itMSBombFallProcMap 0x801764D0, verbatim. */
sb32 itMSBombFallProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITMSBOMB_MAP_REBOUND_COMMON, ITMSBOMB_MAP_REBOUND_GROUND, itMSBombWaitSetStatus);
}

/* it/itcommon/itmsbomb.c:189-194 itMSBombWaitSetStatus 0x80176504, verbatim. */
void itMSBombWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITMSBombStatusDescs, nITMSBombStatusWait);
}

/* it/itcommon/itmsbomb.c:196-205 itMSBombFallSetStatus 0x80176538, verbatim. */
void itMSBombFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITMSBombStatusDescs, nITMSBombStatusFall);
}

/* it/itcommon/itmsbomb.c:207-211 itMSBombHoldSetStatus 0x8017657C, verbatim. */
void itMSBombHoldSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITMSBombStatusDescs, nITMSBombStatusHold);
}

/* it/itcommon/itmsbomb.c:213-225 itMSBombThrownProcUpdate 0x801765A4, verbatim. */
sb32 itMSBombThrownProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITMSBOMB_GRAVITY, ITMSBOMB_TVEL);
    itVisualsUpdateSpin(item_gobj);

    dobj->child->sib_next->rotate.vec.f.z = dobj->rotate.vec.f.z;

    return FALSE;
}

/* it/itcommon/itmsbomb.c:227-231 itMSBombThrownProcMap 0x801765FC, verbatim. */
sb32 itMSBombThrownProcMap(GObj *item_gobj)
{
    return itMapCheckMapProcAll(item_gobj, itMSBombAttachedSetStatus);
}

/* it/itcommon/itmsbomb.c:233-239 itMSBombCommonProcHit 0x80176620, verbatim. */
sb32 itMSBombCommonProcHit(GObj *item_gobj)
{
    itMainVelSetRebound(item_gobj);

    return FALSE;
}

/* it/itcommon/itmsbomb.c:241-252 itMSBombThrownSetStatus 0x80176644, verbatim. */
void itMSBombThrownSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->coll_data.map_coll.top = ITMSBOMB_COLL_SIZE;
    ip->coll_data.map_coll.center = 0.0F;
    ip->coll_data.map_coll.bottom = -ITMSBOMB_COLL_SIZE;
    ip->coll_data.map_coll.width = ITMSBOMB_COLL_SIZE;

    itMainSetStatus(item_gobj, dITMSBombStatusDescs, nITMSBombStatusThrown);
}

/* it/itcommon/itmsbomb.c:254-258 itMSBombDroppedProcMap 0x80176694, verbatim. */
sb32 itMSBombDroppedProcMap(GObj *item_gobj)
{
    return itMapCheckMapProcAll(item_gobj, itMSBombAttachedSetStatus);
}

/* it/itcommon/itmsbomb.c:260-271 itMSBombDroppedSetStatus 0x801766B8, verbatim. */
void itMSBombDroppedSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->coll_data.map_coll.top = ITMSBOMB_COLL_SIZE;
    ip->coll_data.map_coll.center = 0.0F;
    ip->coll_data.map_coll.bottom = -ITMSBOMB_COLL_SIZE;
    ip->coll_data.map_coll.width = ITMSBOMB_COLL_SIZE;

    itMainSetStatus(item_gobj, dITMSBombStatusDescs, nITMSBombStatusDropped);
}

/* it/itcommon/itmsbomb.c:273-312 itMSBombAttachedUpdateSurface 0x80176708, verbatim. */
void itMSBombAttachedUpdateSurface(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    MPCollData *coll_data = &ip->coll_data;
    Vec3f angle;
    DObj *dobj = DObjGetStruct(item_gobj);

    if ((coll_data->mask_curr & MAP_FLAG_CEIL) || (coll_data->mask_curr & MAP_FLAG_FLOOR))
    {
        if (coll_data->mask_curr & MAP_FLAG_CEIL)
        {
            angle = coll_data->ceil_angle;

            ip->attach_line_id = coll_data->ceil_line_id;
        }
        if (coll_data->mask_curr & MAP_FLAG_FLOOR)
        {
            angle = coll_data->floor_angle;

            ip->attach_line_id = coll_data->floor_line_id;
        }
    }
    else
    {
        if (coll_data->mask_curr & MAP_FLAG_LWALL)
        {
            angle = coll_data->lwall_angle;

            ip->attach_line_id = coll_data->lwall_line_id;
        }
        if (coll_data->mask_curr & MAP_FLAG_RWALL)
        {
            angle = coll_data->rwall_angle;

            ip->attach_line_id = coll_data->rwall_line_id;
        }
    }
    dobj->rotate.vec.f.z = syUtilsArcTan2(angle.y, angle.x) - F_CST_DTOR32(90.0F);
}

/* it/itcommon/itmsbomb.c:314-350 itMSBombAttachedInitVars 0x80176840, verbatim. */
void itMSBombAttachedInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    ip->coll_data.map_coll.top = ITMSBOMB_COLL_SIZE;
    ip->coll_data.map_coll.center = 0.0F;
    ip->coll_data.map_coll.bottom = -ITMSBOMB_COLL_SIZE;
    ip->coll_data.map_coll.width = ITMSBOMB_COLL_SIZE;

    ip->physics.vel_air.x = ip->physics.vel_air.y = ip->physics.vel_air.z = 0;

    dobj->child->flags = DOBJ_FLAG_NONE;
    dobj->child->sib_next->flags = DOBJ_FLAG_HIDDEN;

    itMSBombAttachedUpdateSurface(item_gobj);

    ip->is_attach_surface = TRUE;

    ip->damage_coll.hitstatus = nGMHitStatusNormal;

    ip->attack_coll.attack_state = nGMAttackStateOff;

    if ((ip->player != -1) && (ip->player != GMCOMMON_PLAYERS_MAX))
    {
        GObj *fighter_gobj = gSCManagerBattleState->players[ip->player].fighter_gobj;

        if (fighter_gobj != NULL)
        {
            ftParamMakeRumble(ftGetStruct(fighter_gobj), 6, 0);
        }
    }
    func_800269C0_275C0(nSYAudioFGMMSBombAttach);

    itMainClearOwnerStats(item_gobj);
}

/* it/itcommon/itmsbomb.c:352-368 itMSBombExplodeMakeEffect 0x80176934, verbatim. */
void itMSBombExplodeMakeEffect(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    ITAttributes *attr = ip->attr;
    DObj *dobj = DObjGetStruct(item_gobj);
    s32 unused[4];

    if (ip->coll_data.mask_curr & MAP_FLAG_FLOOR)
    {
        Vec3f translate = dobj->translate.vec.f;

        translate.y += attr->map_coll_bottom;

        efManagerDustHeavyDoubleMakeEffect(&translate, ip->lr, 1.0F);
    }
}

/* it/itcommon/itmsbomb.c:370-393 itMSBombExplodeInitStatusVars 0x801769AC, verbatim. */
void itMSBombExplodeInitStatusVars(GObj *item_gobj, sb32 is_make_effect)
{
    LBParticle *pc;
    DObj *dobj = DObjGetStruct(item_gobj);

    if (is_make_effect != FALSE)
    {
        itMSBombExplodeMakeEffect(item_gobj);
    }
    pc = efManagerSparkleWhiteMultiExplodeMakeEffect(&dobj->translate.vec.f);

    if (pc != NULL)
    {
        pc->xf->scale.x = ITMSBOMB_EXPLODE_SCALE;
        pc->xf->scale.y = ITMSBOMB_EXPLODE_SCALE;
        pc->xf->scale.z = ITMSBOMB_EXPLODE_SCALE;
    }
    efManagerQuakeMakeEffect(1);
    itMainRefreshAttackColl(item_gobj);
    itMSBombExplodeSetStatus(item_gobj);

    DObjGetStruct(item_gobj)->flags = DOBJ_FLAG_HIDDEN;
}

/* it/itcommon/itmsbomb.c:395-402 itMSBombCommonProcDamage 0x80176A34, verbatim. */
sb32 itMSBombCommonProcDamage(GObj *item_gobj)
{
    func_800269C0_275C0(nSYAudioFGMExplodeL);
    itMSBombExplodeInitStatusVars(item_gobj, FALSE);

    return FALSE;
}

/* it/itcommon/itmsbomb.c:404-445 itMSBombAttachedProcUpdate 0x80176A68, verbatim. */
sb32 itMSBombAttachedProcUpdate(GObj *item_gobj)
{
    s32 unused[2];
    GObj *fighter_gobj;
    Vec3f *translate;
    Vec3f dist;
    Vec3f fighter_pos;
    DObj *item_dobj = DObjGetStruct(item_gobj);
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->multi < ITMSBOMB_DETECT_FIGHTER_DELAY)
    {
        ip->multi++;
    }
    else
    {
        fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

        translate = &item_dobj->translate.vec.f;

        while (fighter_gobj != NULL)
        {
            FTStruct *fp = ftGetStruct(fighter_gobj);
            DObj *fighter_dobj = DObjGetStruct(fighter_gobj);
            f32 var = fp->attr->map_coll.top * 0.5F;

            fighter_pos = fighter_dobj->translate.vec.f;

            fighter_pos.y += var;

            syVectorDiff3D(&dist, &fighter_pos, translate);

            if ((SQUARE(dist.x) + SQUARE(dist.y) + SQUARE(dist.z)) < ITMSBOMB_DETECT_FIGHTER_RADIUS)
            {
                itMSBombExplodeInitStatusVars(item_gobj, TRUE); /* We might want to break out of the loop here */
            }
            fighter_gobj = fighter_gobj->link_next;
        }
    }
    return FALSE;
}

/* it/itcommon/itmsbomb.c:447-452 itMSBombAttachedSetStatus 0x80176B94, verbatim. */
void itMSBombAttachedSetStatus(GObj *item_gobj)
{
    itMSBombAttachedInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITMSBombStatusDescs, nITMSBombStatusAttached);
}

/* it/itcommon/itmsbomb.c:454-466 itMSBombAttachedProcMap 0x80176BC8, verbatim. */
sb32 itMSBombAttachedProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (mpCollisionCheckExistLineID(ip->attach_line_id) == FALSE)
    {
        ip->is_attach_surface = FALSE;

        itMSBombDetachedSetStatus(item_gobj);
    }
    return FALSE;
}

/* it/itcommon/itmsbomb.c:468-493 itMSBombExplodeUpdateAttackEvent (unaddressed
 * in the decomp's own comments -- falls between AttachedProcMap and
 * DetachedInitVars), verbatim. */
void itMSBombExplodeUpdateAttackEvent(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    ITAttackEvent *ev = itGetAttackEvent(dITMSBombItemDesc, llITCommonDataMSBombAttackEvents);

    if (ip->multi == ev[ip->event_id].timer)
    {
        ip->attack_coll.angle  = ev[ip->event_id].angle;
        ip->attack_coll.damage = ev[ip->event_id].damage;
        ip->attack_coll.size   = ev[ip->event_id].size;

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

/* it/itcommon/itmsbomb.c:495-504 itMSBombDetachedInitVars 0x80176D00, verbatim. */
void itMSBombDetachedInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->damage_coll.hitstatus = nGMHitStatusNormal;
    ip->attack_coll.attack_state = nGMAttackStateOff;

    itMainClearOwnerStats(item_gobj);
}

/* it/itcommon/itmsbomb.c:506-549 itMSBombDetachedProcUpdate 0x80176D2C, verbatim. */
sb32 itMSBombDetachedProcUpdate(GObj *item_gobj)
{
    s32 unused[2];
    GObj *fighter_gobj;
    Vec3f *translate;
    Vec3f dist;
    Vec3f fighter_pos;
    DObj *item_dobj = DObjGetStruct(item_gobj);
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITMSBOMB_GRAVITY, ITMSBOMB_TVEL);

    if (ip->multi < ITMSBOMB_DETECT_FIGHTER_DELAY)
    {
        ip->multi++;
    }
    else
    {
        fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

        translate = &item_dobj->translate.vec.f;

        while (fighter_gobj != NULL)
        {
            FTStruct *fp = ftGetStruct(fighter_gobj);
            DObj *fighter_dobj = DObjGetStruct(fighter_gobj);
            f32 offset_y = fp->attr->map_coll.top * 0.5F;

            fighter_pos = fighter_dobj->translate.vec.f;

            fighter_pos.y += offset_y;

            syVectorDiff3D(&dist, &fighter_pos, translate);

            if ((SQUARE(dist.x) + SQUARE(dist.y) + SQUARE(dist.z)) < ITMSBOMB_DETECT_FIGHTER_RADIUS)
            {
                itMSBombExplodeInitStatusVars(item_gobj, FALSE); /* We might want to break out of the loop here */
            }
            fighter_gobj = fighter_gobj->link_next;
        }
    }
    return FALSE;
}

/* it/itcommon/itmsbomb.c:551-556 itMSBombDetachedSetStatus 0x80176E68, verbatim. */
void itMSBombDetachedSetStatus(GObj *item_gobj)
{
    itMSBombDetachedInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITMSBombStatusDescs, nITMSBombStatusDetached);
}

/* it/itcommon/itmsbomb.c:558-573 itMSBombExplodeInitVars 0x80176E9C, verbatim. */
void itMSBombExplodeInitVars(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->multi = 0;

    ip->event_id = 0;

    ip->attack_coll.throw_mul = ITEM_THROW_DEFAULT;
    ip->attack_coll.fgm_id = nSYAudioFGMExplodeL;

    ip->damage_coll.hitstatus = nGMHitStatusNone;

    itMSBombExplodeUpdateAttackEvent(item_gobj);
}

/* it/itcommon/itmsbomb.c:575-589 itMSBombExplodeProcUpdate 0x80176EE4, verbatim. */
sb32 itMSBombExplodeProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMSBombExplodeUpdateAttackEvent(item_gobj);

    ip->multi++;

    if (ip->multi == ITMSBOMB_EXPLODE_LIFETIME)
    {
        return TRUE;
    }
    else return FALSE;
}

/* it/itcommon/itmsbomb.c:591-596 itMSBombExplodeSetStatus 0x80176F2C, verbatim. */
void itMSBombExplodeSetStatus(GObj *item_gobj)
{
    itMSBombExplodeInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITMSBombStatusDescs, nITMSBombStatusExplode);
}

/* it/itcommon/itmsbomb.c:598-639 itMSBombMakeItem 0x80176F60 -- left out:
 * ends with `ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);`, the
 * IFCommonItem export dependency every roster item's own MakeItem
 * shares. */

/* it/itcommon/itmsbomb.c:599-639 itMSBombMakeItem 0x80176F60, verbatim. */
GObj* itMSBombMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITMSBombItemDesc, pos, vel, flags);
    DObj *dobj;
    ITStruct *ip;
#if defined(REGION_US)
    Vec3f translate;
#endif

    if (item_gobj != NULL)
    {
        dobj = DObjGetStruct(item_gobj);

        dobj->child->flags = DOBJ_FLAG_HIDDEN;
        dobj->child->sib_next->flags = DOBJ_FLAG_NONE;

#if defined(REGION_US)
        translate = dobj->translate.vec.f;
#endif

        gcAddXObjForDObjFixed(dobj, nGCMatrixKindTraRotRpyR, 0);
        gcAddXObjForDObjFixed(dobj->child->sib_next, 0x46, 0);

#if defined(REGION_US)
        dobj->translate.vec.f = translate;
#else
        dobj->translate.vec.f = *pos;
#endif

        ip = itGetStruct(item_gobj);

        ip->multi = 0;

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);

        dobj->rotate.vec.f.z = 0.0F;
    }
    return item_gobj;
}
