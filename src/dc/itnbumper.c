/* itnbumper.c -- it/itcommon/itnbumper.c, verbatim except one function left
 * out (see below): the Bumper's `ITDesc`, its eight-state `ITStatusDesc`
 * table (ground wait / air fall / fighter hold / fighter throw / fighter
 * drop / ground active-attached / airborne-after-hit / ground despawn),
 * and their proc bodies. Function-for-function against the game's own
 * ITStruct (it/ittypes.h); every function names its decomp line range.
 *
 * The roster's thirteenth entry, and the first genuinely
 * standalone shape since Green/Red Shell -- not a
 * companion-weapon twin, not a shell twin. Checked for the
 * Container-item shape: no call anywhere in this
 * file reaches `itMainMakeContainerItem`.
 *
 * A picked-up-and-thrown Bumper behaves like any other thrown item until
 * it lands, but a Bumper that lands WITHOUT being thrown at a target
 * (`itNBumperFallProcMap`'s own `itMapCheckDestroyDropped` -> Wait ->
 * `itNBumperWaitProcMap`'s own `itMapCheckLRWallProcNoFloor`, which never
 * fires on a flat landing) never becomes Active on its own -- it is the
 * SHIELD-bounce and reflector paths, and specifically a fighter's own
 * pickup-then-set-down, that route it there. Once Active
 * (`itNBumperAttachedSetStatus`/`AttachedInitVars`), it pins itself to
 * whatever floor line it landed on (`ip->attach_line_id`,
 * `ip->is_attach_surface`), orients its model to the floor's own slope
 * (`itNBumperAttachedSetModelPitch`, `syUtilsArcTan2` on the floor
 * normal), and becomes a live hazard for `ITBUMPER_LIFETIME` (360 frames)
 * -- any fighter that touches it (`itNBumperAttachedProcHit`) gets
 * bounced away at a fixed velocity and the Bumper itself flips into a
 * three-frame "hit" palette and a brief HitAir arc
 * (`itNBumperHitAirProcUpdate`/`SetStatus`) before settling back to
 * Attached. At end of life it blinks itself out over
 * `ITBUMPER_DESPAWN_TIMER` frames (`itNBumperGDisappearProcUpdate`,
 * toggling `DOBJ_FLAG_HIDDEN` every other frame) and destroys itself
 * (returns TRUE) rather than being destroyed by anything else.
 *
 * `itNBumperAttachedInitVars` needed the SAME third kind of reloc-offset
 * arithmetic Green/Red Shell used (`itGetPData(ip, off1, off2)`,
 * it/item.h) -- but for a DIFFERENT pair of targets: not an AObjEvent32
 * anim script, but a `Gfx *` display list and an `MObjSub *` swapped onto
 * the model directly (`dobj->dl = ...; gcAddMObjForDObj(dobj, mobjsub)`).
 * Confirmed all four of this file's own reloc symbols
 * (`ItemAttributes`/`DataStart`/`MObjSub NBumperWait`/
 * `DisplayList NBumperWait`) are real, declared blocks in `tools/
 * relocFileDescriptions.us.txt`'s "FILE CONTENTS" section before writing
 * this file.
 *
 * The MObjSub read is the one that cannot be faked with a bare word:
 * `gcAddMObjForDObj` (`sys/objman.c`, unmodified) does an unconditional
 * `mobj->sub = *mobjsub;` -- a FULL STRUCT COPY, not an opcode-driven
 * parse that happens to stop after one word the way `gcPlayAnimAll`'s
 * AObjEvent32 walk does on a zeroed script. Reading `sizeof(MObjSub)`
 * bytes from a 4-byte `int` is real undefined behaviour, which is why
 * it is the offset it actually is (0x7a38 into
 * ITCommonObject, src/dc/itemoffsets.h) and the guarantee moved to the
 * host test's faked model file: 0x7a38 into a zeroed 0x8000 buffer is a
 * real zeroed MObjSub, which is the same protection by a different road.
 * **The rule that survives: a read that is NOT opcode-gated has to land
 * on real memory of its own size, wherever that memory comes from.**
 *
 * `item_vars.bumper.damage_all_delay` is a `u16`, not Shell's `u8` -- the
 * SAME `!= -1` integer-promotion shape Green Shell has (`u16` 0xFFFF
 * promotes to `int` 65535, never equal to the `int` literal -1, so a
 * value JUST set to -1 falls straight through to the same call's own
 * decrement) recurs here at a different field width. Applied directly to
 * this file's own test values.
 *
 * `itNBumperMakeItem` is ported now, and it is the decomp's line for
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
#include <ft/fighter.h>

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

ITDesc dITNBumperItemDesc =
{
    nITKindNBumper,
    &gITManagerCommonData,
    (intptr_t)llITCommonDataNBumperItemAttributes,
    { nGCMatrixKindTra, nGCMatrixKindNull, 0 },
    nGMAttackStateOff,
    itNBumperFallProcUpdate,
    itNBumperFallProcMap,
    NULL, NULL, NULL, NULL, NULL, NULL
};

ITStatusDesc dITNBumperStatusDescs[/* */] =
{
    /* Status 0 (Ground Wait) */
    { NULL, itNBumperWaitProcMap, NULL, NULL, NULL, NULL, NULL, NULL },
    /* Status 1 (Air Wait Fall) */
    { itNBumperFallProcUpdate, itNBumperFallProcMap, NULL, NULL, NULL, NULL, NULL, NULL },
    /* Status 2 (Fighter Hold) */
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    /* Status 3 (Fighter Throw) */
    { itNBumperThrownProcUpdate, itNBumperThrownProcMap, itNBumperThrownProcHit,
      itNBumperThrownProcShield, itMainCommonProcHop, NULL, itNBumperThrownProcReflector, NULL },
    /* Status 4 (Fighter Drop) */
    { itNBumperThrownProcUpdate, itNBumperThrownProcMap, itNBumperThrownProcHit,
      itNBumperThrownProcShield, itMainCommonProcHop, NULL, itNBumperThrownProcReflector, NULL },
    /* Status 5 (Ground Active Wait) */
    { itNBumperAttachedProcUpdate, itNBumperAttachedProcMap, itNBumperAttachedProcHit,
      NULL, NULL, NULL, itNBumperAttachedProcReflector, NULL },
    /* Status 6 (Airborne after Ground Active Wait) */
    { itNBumperHitAirProcUpdate, itNBumperThrownProcMap, itNBumperThrownProcHit,
      itNBumperThrownProcShield, NULL, NULL, itNBumperThrownProcReflector, NULL },
    /* Status 7 (Despawn) */
    { itNBumperGDisappearProcUpdate, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

enum itNBumperStatus
{
    nITNBumperStatusWait,
    nITNBumperStatusFall,
    nITNBumperStatusHold,
    nITNBumperStatusThrown,
    nITNBumperStatusDropped,
    nITNBumperStatusAttached,
    nITNBumperStatusHitAir,
    nITNBumperStatusGDisappear,
    nITNBumperStatusEnumCount
};

/* it/itcommon/itnbumper.c:161-190 itNBumperFallProcUpdate 0x8017B430, verbatim. */
sb32 itNBumperFallProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITBUMPER_GRAVITY_NORMAL, ITBUMPER_TVEL);

    if (ip->multi != 0)
    {
        dobj->scale.vec.f.x = dobj->scale.vec.f.y = dobj->scale.vec.f.z = (2.0F - (10 - ip->multi) * 0.1F);

        ip->multi--;
    }
    else dobj->scale.vec.f.x = dobj->scale.vec.f.y = dobj->scale.vec.f.z = 1.0F;

    if (!ip->item_vars.bumper.damage_all_delay)
    {
        itMainClearOwnerStats(item_gobj);

        ip->item_vars.bumper.damage_all_delay = -1;
    }
    if (ip->item_vars.bumper.damage_all_delay != -1)
    {
        ip->item_vars.bumper.damage_all_delay--;
    }
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itnbumper.c:192-198 itNBumperWaitProcMap 0x8017B520, verbatim. */
sb32 itNBumperWaitProcMap(GObj *item_gobj)
{
    itMapCheckLRWallProcNoFloor(item_gobj, itNBumperFallSetStatus);

    return FALSE;
}

/* it/itcommon/itnbumper.c:200-204 itNBumperFallProcMap 0x8017B548, verbatim. */
sb32 itNBumperFallProcMap(GObj *item_gobj)
{
    return itMapCheckDestroyDropped(item_gobj, ITBUMPER_MAP_REBOUND_COMMON, ITBUMPER_MAP_REBOUND_GROUND, itNBumperWaitSetStatus);
}

/* it/itcommon/itnbumper.c:206-228 itNBumperThrownProcHit 0x8017B57C, verbatim. */
sb32 itNBumperThrownProcHit(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    dobj->scale.vec.f.x = 2.0F;
    dobj->scale.vec.f.y = 2.0F;
    dobj->scale.vec.f.z = 2.0F;

    ip->item_vars.bumper.hit_anim_length = ITBUMPER_HIT_ANIM_LENGTH;

    dobj->mobj->palette_id = 1.0F;

    ip->physics.vel_air.x = ITBUMPER_REBOUND_AIR_X * ip->hit_lr;
    ip->physics.vel_air.y = ITBUMPER_REBOUND_AIR_Y;

    ip->multi = ITBUMPER_HIT_SCALE;

    itNBumperHitAirSetStatus(item_gobj);

    return FALSE;
}

/* it/itcommon/itnbumper.c:230-235 itNBumperWaitSetStatus 0x8017B600, verbatim. */
void itNBumperWaitSetStatus(GObj *item_gobj)
{
    itMainSetGroundAllowPickup(item_gobj);
    itMainSetStatus(item_gobj, dITNBumperStatusDescs, nITNBumperStatusWait);
}

/* it/itcommon/itnbumper.c:237-246 itNBumperFallSetStatus 0x8017B634, verbatim. */
void itNBumperFallSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->is_allow_pickup = FALSE;

    itMapSetAir(ip);
    itMainSetStatus(item_gobj, dITNBumperStatusDescs, nITNBumperStatusFall);
}

/* it/itcommon/itnbumper.c:248-252 itNBumperHoldSetStatus 0x8017B678, verbatim. */
void itNBumperHoldSetStatus(GObj *item_gobj)
{
    itMainSetStatus(item_gobj, dITNBumperStatusDescs, nITNBumperStatusHold);
}

/* it/itcommon/itnbumper.c:254-274 itNBumperThrownProcUpdate 0x8017B6A0, verbatim. */
sb32 itNBumperThrownProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    itMainApplyGravityClampTVel(ip, ITBUMPER_GRAVITY_NORMAL, ITBUMPER_TVEL);

    if (!(ip->item_vars.bumper.damage_all_delay))
    {
        itMainClearOwnerStats(item_gobj);

        ip->item_vars.bumper.damage_all_delay = -1;
    }
    if (ip->item_vars.bumper.damage_all_delay != -1)
    {
        ip->item_vars.bumper.damage_all_delay--;
    }
    itVisualsUpdateSpin(item_gobj);

    return FALSE;
}

/* it/itcommon/itnbumper.c:276-280 itNBumperThrownProcMap 0x8017B720, verbatim. */
sb32 itNBumperThrownProcMap(GObj *item_gobj)
{
    return itMapCheckMapReboundProcNoFloor(item_gobj, 0.8F, itNBumperAttachedSetStatus);
}

/* it/itcommon/itnbumper.c:282-289 itNBumperThrownProcShield 0x8017B74C, verbatim. */
sb32 itNBumperThrownProcShield(GObj *item_gobj)
{
    itMainVelSetRebound(item_gobj);
    itMainClearOwnerStats(item_gobj);

    return FALSE;
}

/* it/itcommon/itnbumper.c:291-304 itNBumperThrownProcReflector 0x8017B778, verbatim. */
sb32 itNBumperThrownProcReflector(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    FTStruct *fp = ftGetStruct(ip->owner_gobj);

    if ((ip->physics.vel_air.x * fp->lr) < 0.0F)
    {
        ip->physics.vel_air.x = -ip->physics.vel_air.x;
    }
    itMainClearOwnerStats(item_gobj);

    return FALSE;
}

/* it/itcommon/itnbumper.c:306-317 itNBumperThrownSetStatus 0x8017B7DC, verbatim. */
void itNBumperThrownSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->item_vars.bumper.damage_all_delay = ITBUMPER_DAMAGE_ALL_WAIT;

    ip->coll_data.map_coll.top = ITBUMPER_COLL_SIZE;
    ip->coll_data.map_coll.bottom = -ITBUMPER_COLL_SIZE;

    itMainSetStatus(item_gobj, dITNBumperStatusDescs, nITNBumperStatusThrown);
}

/* it/itcommon/itnbumper.c:319-330 itNBumperDroppedSetStatus 0x8017B828, verbatim. */
void itNBumperDroppedSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->item_vars.bumper.damage_all_delay = ITBUMPER_DAMAGE_ALL_WAIT;

    ip->coll_data.map_coll.top = ITBUMPER_COLL_SIZE;
    ip->coll_data.map_coll.bottom = -ITBUMPER_COLL_SIZE;

    itMainSetStatus(item_gobj, dITNBumperStatusDescs, nITNBumperStatusDropped);
}

/* it/itcommon/itnbumper.c:332-345 itNBumperAttachedSetModelPitch 0x8017B874, verbatim. */
void itNBumperAttachedSetModelPitch(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    s32 unused;
    Vec3f floor_angle;
    DObj *dobj = DObjGetStruct(item_gobj);

    floor_angle = ip->coll_data.floor_angle;

    ip->attach_line_id = ip->coll_data.floor_line_id;

    dobj->rotate.vec.f.z = syUtilsArcTan2(floor_angle.y, floor_angle.x) - F_CLC_DTOR32(90.0F);
}

/* it/itcommon/itnbumper.c:347-384 itNBumperAttachedInitVars 0x8017B8DC, verbatim. */
void itNBumperAttachedInitVars(GObj *item_gobj)
{
    s32 unused[2];
    DObj *dobj;
    ITStruct *ip;
    MObjSub *mobjsub;
    Gfx *dl;

    ip = itGetStruct(item_gobj);
    dobj = DObjGetStruct(item_gobj);

    ip->physics.vel_air.x = 0.0F;
    ip->physics.vel_air.y = 0.0F;
    ip->physics.vel_air.z = 0.0F;

    dl = itGetPData(ip, llITCommonDataNBumperDataStart, llITCommonDataNBumperWaitDisplayList);

    itemModelSetDisplayList(dobj, dl);    /* DIVERGES: itemmodel.h */

    mobjsub = itGetPData(ip, llITCommonDataNBumperDataStart, llITCommonDataNBumperWaitMObjSub);

    gcRemoveMObjAll(dobj);
    gcAddMObjForDObj(dobj, mobjsub);

    dobj->scale.vec.f.x = dobj->scale.vec.f.y = dobj->scale.vec.f.z = 1.0F;

    ip->coll_data.map_coll.top = ITBUMPER_COLL_SIZE;
    ip->coll_data.map_coll.bottom = -ITBUMPER_COLL_SIZE;

    itNBumperAttachedSetModelPitch(item_gobj);

    ip->is_attach_surface = TRUE;

    ip->lifetime = ITBUMPER_LIFETIME;

    itMainClearOwnerStats(item_gobj);
}

/* it/itcommon/itnbumper.c:386-406 itNBumperAttachedProcHit 0x8017B9C8, verbatim. */
sb32 itNBumperAttachedProcHit(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    dobj->scale.vec.f.x = 2.0F;
    dobj->scale.vec.f.z = 2.0F;

    ip->item_vars.bumper.hit_anim_length = ITBUMPER_HIT_ANIM_LENGTH;

    dobj->mobj->palette_id = 1.0F;

    ip->lr = -ip->hit_lr;

    ip->physics.vel_air.x = ip->hit_lr * ITBUMPER_REBOUND_VEL_X;

    ip->multi = ITBUMPER_HIT_SCALE;

    return FALSE;
}

/* it/itcommon/itnbumper.c:408-462 itNBumperAttachedProcUpdate 0x8017BA2C, verbatim. */
sb32 itNBumperAttachedProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    ITAttributes *attr = ip->attr;
    DObj *dobj = DObjGetStruct(item_gobj);
    Vec3f edge_pos;

    if ((ip->item_vars.bumper.hit_anim_length == 0) && (dobj->mobj->palette_id == 1.0F))
    {
        dobj->mobj->palette_id = 0.0F;
    }
    else ip->item_vars.bumper.hit_anim_length--;

    if (mpCollisionCheckExistLineID(ip->coll_data.floor_line_id) != FALSE)
    {
        if (ip->lr == -1)
        {
            mpCollisionGetFloorEdgeL(ip->coll_data.floor_line_id, &edge_pos);

            if (edge_pos.x >= (dobj->translate.vec.f.x - attr->map_coll_width))
            {
                ip->physics.vel_air.x = 0.0F;
            }
        }
        else
        {
            mpCollisionGetFloorEdgeR(ip->coll_data.floor_line_id, &edge_pos);

            if (edge_pos.x <= (dobj->translate.vec.f.x + attr->map_coll_width))
            {
                ip->physics.vel_air.x = 0.0F;
            }
        }
    }
    if (ip->multi < ITBUMPER_STOPVEL_WAIT)
    {
        ip->physics.vel_air.x = 0.0F;
    }
    if (ip->multi != 0)
    {
        dobj->scale.vec.f.x = dobj->scale.vec.f.z = 2.0F - ((10 - ip->multi) * 0.1F);

        ip->multi--;
    }
    else dobj->scale.vec.f.x = dobj->scale.vec.f.y = dobj->scale.vec.f.z = 1.0F;

    if (ip->lifetime == 0)
    {
        itNBumperGDisappearSetStatus(item_gobj);
    }
    ip->lifetime--;

    return FALSE;
}

/* it/itcommon/itnbumper.c:464-490 itNBumperAttachedProcMap 0x8017BBFC, verbatim. */
sb32 itNBumperAttachedProcMap(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *joint = DObjGetStruct(item_gobj);

    if (itMapCheckLRWallProcNoFloor(item_gobj, itNBumperDroppedSetStatus) != FALSE)
    {
        if (mpCollisionCheckExistLineID(ip->attach_line_id) == FALSE)
        {
            ip->is_attach_surface = FALSE;

            itNBumperDroppedSetStatus(item_gobj);

            joint->scale.vec.f.x = joint->scale.vec.f.y = joint->scale.vec.f.z = 1.0F;

            joint->mobj->palette_id = 0.0F;
        }
        else if (ip->multi == 0)
        {
            itNBumperAttachedSetModelPitch(item_gobj);
        }
    }
    return FALSE;
}

/* it/itcommon/itnbumper.c:492-515 itNBumperAttachedProcReflector 0x8017BCC0, verbatim. */
sb32 itNBumperAttachedProcReflector(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    FTStruct *fp = ftGetStruct(ip->owner_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    dobj->scale.vec.f.x = 2.0F;
    dobj->scale.vec.f.z = 2.0F;

    ip->item_vars.bumper.hit_anim_length = 3;

    dobj->mobj->palette_id = 1.0F;

    ip->physics.vel_air.x = (-fp->lr * ITBUMPER_REBOUND_VEL_X);

    ip->lr = fp->lr;

    ip->multi = ITBUMPER_HIT_SCALE;

    itMainClearOwnerStats(item_gobj);

    return FALSE;
}

/* it/itcommon/itnbumper.c:517-522 itNBumperAttachedSetStatus 0x8017BD4C, verbatim. */
void itNBumperAttachedSetStatus(GObj *item_gobj)
{
    itNBumperAttachedInitVars(item_gobj);
    itMainSetStatus(item_gobj, dITNBumperStatusDescs, nITNBumperStatusAttached);
}

/* it/itcommon/itnbumper.c:524-557 itNBumperHitAirProcUpdate 0x8017BD80, verbatim. */
sb32 itNBumperHitAirProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if ((ip->item_vars.bumper.hit_anim_length == 0) && (dobj->mobj->palette_id == 1.0F))
    {
        dobj->mobj->palette_id = 0.0F;
    }
    else ip->item_vars.bumper.hit_anim_length--;

    itMainApplyGravityClampTVel(ip, ITBUMPER_GRAVITY_HIT, ITBUMPER_TVEL);

    if (ip->multi != 0)
    {
        dobj->scale.vec.f.x = dobj->scale.vec.f.y = dobj->scale.vec.f.z = (2.0F - (10 - ip->multi) * 0.1F);

        ip->multi--;
    }
    else dobj->scale.vec.f.x = dobj->scale.vec.f.y = dobj->scale.vec.f.z = 1;

    if (!ip->item_vars.bumper.damage_all_delay)
    {
        itMainClearOwnerStats(item_gobj);

        ip->item_vars.bumper.damage_all_delay = -1;
    }
    if (ip->item_vars.bumper.damage_all_delay != -1)
    {
        ip->item_vars.bumper.damage_all_delay--;
    }
    return FALSE;
}

/* it/itcommon/itnbumper.c:559-567 itNBumperHitAirSetStatus 0x8017BEA0, verbatim. */
void itNBumperHitAirSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->item_vars.bumper.damage_all_delay = ITBUMPER_DAMAGE_ALL_WAIT;

    itMainSetStatus(item_gobj, dITNBumperStatusDescs, nITNBumperStatusHitAir);
}

/* it/itcommon/itnbumper.c:569-587 itNBumperGDisappearProcUpdate 0x8017BED4, verbatim. */
sb32 itNBumperGDisappearProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ip->lifetime == 0)
    {
        return TRUE;
    }
    else if ((ip->lifetime % 2) != 0)
    {
        DObj *dobj = DObjGetStruct(item_gobj);

        dobj->flags ^= DOBJ_FLAG_HIDDEN;
    }
    ip->lifetime--;

    return FALSE;
}

/* it/itcommon/itnbumper.c:589-612 itNBumperGDisappearSetStatus 0x8017BF1C, verbatim. */
void itNBumperGDisappearSetStatus(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    dobj->mobj->palette_id = 0;

    dobj->scale.vec.f.x = 1.0F;
    dobj->scale.vec.f.y = 1.0F;
    dobj->scale.vec.f.z = 1.0F;

    ip->lifetime = ITBUMPER_DESPAWN_TIMER;

    dobj->flags = DOBJ_FLAG_NONE;

    ip->attack_coll.attack_state = nGMAttackStateOff;

    ip->physics.vel_air.x = 0.0F;
    ip->physics.vel_air.y = 0.0F;
    ip->physics.vel_air.z = 0.0F;

    itMainSetStatus(item_gobj, dITNBumperStatusDescs, nITNBumperStatusGDisappear);
}

/* it/itcommon/itnbumper.c:614-652 itNBumperMakeItem 0x8017BF8C -- left out:
 * ends with `ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);`, the
 * IFCommonItem export dependency every roster item's own MakeItem
 * shares. */

/* it/itcommon/itnbumper.c:615-652 itNBumperMakeItem 0x8017BF8C, verbatim. */
GObj* itNBumperMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITNBumperItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        DObj *dobj = DObjGetStruct(item_gobj);
        ITStruct *ip;
#if defined(REGION_US)
        Vec3f translate = dobj->translate.vec.f;
#endif

        ip = itGetStruct(item_gobj);

        ip->multi = 0;

        ip->attack_coll.interact_mask = GMHITCOLLISION_FLAG_FIGHTER;

        ip->attack_coll.can_rehit_shield = TRUE;

        dobj->mobj->palette_id = 0.0F;

        gcAddXObjForDObjFixed(dobj, 0x2E, 0);

#if defined(REGION_US)
        dobj->translate.vec.f = translate;
#else
        dobj->translate.vec.f = *pos;
#endif

        dobj->rotate.vec.f.z = 0.0F;

        ip->is_unused_item_bool = TRUE;

        ip->arrow_gobj = ifCommonItemArrowMakeInterface(ip);
    }
    return item_gobj;
}
