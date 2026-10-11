/* itpowerblock.c -- it/itground/itpowerblock.c, verbatim but for the two
 * script reads: Mushroom Kingdom's `?` block, the first of the STAGE
 * items.
 *
 * The `?` block is the stage's other hazard besides the scales. It sits
 * in a `nMPMapObjKindPowerBlock` position, is made on a timer
 * (grInishiePowerBlockUpdateMake, 1800 tics after the battle starts and
 * again after every hit), and when a grounded fighter touches it the
 * block plays its damage animation, shakes the screen and registers a
 * `nGMHitEnvironmentPowerBlock` hazard -- whose consumer has been in
 * src/dc/ftmain.c with nothing to fire it.
 *
 * THREE THINGS ABOUT IT ARE NOT LIKE THE OTHER ITEMS, and all three come
 * from where its data lives:
 *
 *  - Its `ItemAttributes` is NOT in ITCommonData. It is Mushroom
 *    Kingdom's own map file -- `ItemAttributes PowerBlock` at 0xD8 of
 *    reloc file 155, which is also the file the stage's layers come from
 *    -- and the port's item pack carries it under that same offset
 *    (tools/export/ssb_itemexport.py's STAGE_ITEMS). So the desc's `p_file`
 *    DIVERGES from the decomp's `&gGRCommonStruct.inishie.item_head` to
 *    `&gITManagerCommonData`: `itemPackAttr` (src/dc/itempack.c) is the
 *    function that answers for an item's attributes and it answers for
 *    exactly one file. The `o_attributes` is the decomp's OWN number.
 *
 *  - Its scripts are the STAGE's, reached in the game by offset
 *    arithmetic into the map file:
 *    `itGetPData(ip, &llGRInishieMapPowerBlockDataStart,
 *    &llGRInishieMapPowerBlockAnimJoint)` is `attr->data - 0x11F8 +
 *    0x1288`. The port has no such file image to subtract from, so both
 *    scripts are named blocks in the stage pack instead
 *    (`stage_anims`, tools/export/ssb_stageexport.py): "PowerBlock" is the
 *    damage one and "PowerBlockIdle" the one the block bobs with. Its
 *    `attr->anim_joints` -- the array at 0x13B0 the game feeds
 *    gcAddAnimAll from itManagerMakeItem -- is NO_PTR for the same
 *    reason, so the idle script is attached HERE, in MakeItem, where the
 *    attach is visible.
 *
 *  - DObj JOINT 1 IS THE BLOCK. The tree is three entries: a throwaway
 *    root, the block, and a NULL link. The idle array is per-joint --
 *    `{ NULL, AnimJoint_0x13B8 }` -- so it lands on the second, and
 *    itManagerMakeItem's own eject then makes that second entry the
 *    port's ROOT. Both of the attaches below are to
 *    DObjGetStruct(item_gobj), which is the block either way.
 *
 * The model is `itpowerblock.mdl`, baked out of the same file's DObjDesc
 * at 0x11F8 and keyed in src/dc/itemmodel.c by the same 0xD8.
 */
#include <it/item.h>
#include <gr/ground.h>
#include <gr/grcommon/grinishie.h> /* the stage's own side of the `?` block:
                                     SetWait and SetDamage are the two the
                                     item calls, and the decomp's header
                                     declares both (grinishie.h:19, :24) */
#include <sys/objanim.h>          /* gcAddDObjAnimJoint, gcPlayAnimAll */
#include <sc/scene.h>             /* gSCManagerBattleState */

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

/* it/itground/itpowerblock.c:11-33 dITPowerBlockItemDesc, verbatim but
 * for `p_file` and the `o_attributes` spelling.
 *
 * `0x44` for the main transform kind is the DECOMP'S OWN VALUE and not
 * an enum: the file writes a bare hex number where every other item
 * names `nGCMatrixKindTra...`. It is kept as written -- guessing which
 * kind 0x44 was meant to be would be inventing a fact the ROM does not
 * state -- and the port bakes its transform into the model anyway
 * (src/dc/objmodel.c), so the field is not read on this side.
 *
 * The `o_attributes` number is 0xD8: `&llGRInishieMapPowerBlockItem-
 * Attributes` (reloc_data.us.h:4306), the offset of this item's table in
 * the STAGE's map file, which is also the key the item pack carries it
 * under. */
ITDesc dITPowerBlockItemDesc =
{
    nITKindPowerBlock,                      /* Item Kind */
    /* DIVERGES: the decomp's `&gGRCommonStruct.inishie.item_head` is the
     * stage's own file. See this file's header. */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)0xD8,                         /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        0x44,                               /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateOff,                      /* Hitbox Update State */
    itPowerBlockCommonProcUpdate,           /* Proc Update */
    NULL,                                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* it/itground/itpowerblock.c:35-48 dITPowerBlockStatusDescs, verbatim:
 * ONE status, and the only thing it carries is the damage callback --
 * the wait state is a state of the STAGE, not of the item. */
ITStatusDesc dITPowerBlockStatusDescs[/* */] =
{
    /* Status 0 (Neutral Wait) */
    {
        NULL,                               /* Proc Update */
        NULL,                               /* Proc Map */
        NULL,                               /* Proc Hit */
        NULL,                               /* Proc Shield */
        NULL,                               /* Proc Hop */
        NULL,                               /* Proc Set-Off */
        NULL,                               /* Proc Reflector */
        itPowerBlockWaitProcDamage          /* Proc Damage */
    }
};

/* // // // // // // // // // // // //
 *                                   //
 *          ENUMERATORS              //
 *                                   //
 * // // // // // // // // // // // // */

enum itPowerBlockStatus
{
    nITPowerBlockStatusWait,
    nITPowerBlockStatusEnumCount
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/itpowerblock.c:68-76 itPowerBlockCommonProcUpdate 0x8017C090,
 * verbatim: the block waits for its idle animation to run out and then
 * settles into the Wait status, which is where it can be hit. A block
 * whose script is missing -- which on this side means a stage pack with
 * no "PowerBlockIdle" -- never reaches it, so it is attached in MakeItem
 * rather than left to the game's own gcAddAnimAll. */
sb32 itPowerBlockCommonProcUpdate(GObj *item_gobj)
{
    if (DObjGetStruct(item_gobj)->anim_wait == AOBJ_ANIM_NULL)
    {
        itPowerBlockWaitSetStatus(item_gobj);
    }
    return FALSE;
}

/* it/itground/itpowerblock.c:78-86 itPowerBlockWaitSetStatus 0x8017C0D4,
 * verbatim. */
void itPowerBlockWaitSetStatus(GObj *item_gobj)
{
    ITStruct *ip;

    itMainSetStatus(item_gobj, dITPowerBlockStatusDescs, nITPowerBlockStatusWait);

    ip = itGetStruct(item_gobj), ip->damage_coll.hitstatus = nGMHitStatusNormal;
}

/* it/itground/itpowerblock.c:88-98 itPowerBlockNDamageProcUpdate
 * 0x8017C110, verbatim: after the damage animation has played out, the
 * STAGE is told -- grInishiePowerBlockSetWait arms the next block -- and
 * the item destroys itself by returning TRUE. */
sb32 itPowerBlockNDamageProcUpdate(GObj *item_gobj)
{
    if (DObjGetStruct(item_gobj)->anim_wait == AOBJ_ANIM_NULL)
    {
        grInishiePowerBlockSetWait();

        return TRUE;
    }
    else return FALSE;
}

/* it/itground/itpowerblock.c:100-115 itPowerBlockWaitProcDamage
 * 0x8017C15C, verbatim but for the script read. What getting hit does:
 * the item stops being hittable (hitstatus None), plays the damage
 * animation, makes the FGM and a three-strength quake, and tells the
 * stage it is damaged.
 *
 * DIVERGES: the decomp's `itGetPData(ip, &llGRInishieMapPowerBlock-
 * DataStart, &llGRInishieMapPowerBlockAnimJoint)` is an offset
 * arithmetic into the stage's map file -- `attr->data - 0x11F8 + 0x1288`
 * -- and the port has no such image; the script is the pack's own named
 * block. Same script, named. */
sb32 itPowerBlockWaitProcDamage(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    ip->proc_update = itPowerBlockNDamageProcUpdate;
    ip->damage_coll.hitstatus = nGMHitStatusNone;

#ifndef FT_HOSTTEST
    /* the host cannot walk a script (see grcastle.c's own note): the
     * attach is the target's, and the host asserts the pack's words */
    gcAddDObjAnimJoint(DObjGetStruct(item_gobj), stage_map_anim("PowerBlock"), 0.0F);
    gcPlayAnimAll(item_gobj);
#endif
    func_800269C0_275C0(nSYAudioFGMInishiePowerBlock);
    efManagerQuakeMakeEffect(3);
    grInishiePowerBlockSetDamage();

    return FALSE;
}

/* it/itground/itpowerblock.c:117-129 itPowerBlockMakeItem 0x8017C1E0,
 * verbatim but for the idle script's attach.
 *
 * DIVERGES: the decomp attaches nothing here -- `attr->anim_joints` is
 * the array at 0x13B0 and itManagerMakeItem's own gcAddAnimAll plays it.
 * The port's record carries NO_PTR there (tools/export/ssb_itemexport.py's
 * STAGE_ITEMS has the whole reason), so the one script that array names
 * is attached here, to the joint it belongs to: entry 1 of
 * `{ NULL, AnimJoint_0x13B8 }`, which is pack joint 1 and the DObj the
 * eject above has just made the root. */
GObj* itPowerBlockMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITPowerBlockItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        ip->damage_coll.interact_mask = GMHITCOLLISION_FLAG_FIGHTER;

#ifndef FT_HOSTTEST
        gcAddDObjAnimJoint(DObjGetStruct(item_gobj), stage_map_anim("PowerBlockIdle"), 0.0F);
        gcPlayAnimAll(item_gobj);
#endif
    }
    return item_gobj;
}
