/* itgbumper.c -- it/itground/itgbumper.c, verbatim: the Bumper Peach's
 * Castle puts on the field (`gGRCommonStruct.castle.bumper_gobj =
 * itManagerMakeItemSetupCommon(NULL, nITKindGBumper, ...)`,
 * gr/grcommon/grcastle.c:57). Its `ITDesc` and its two proc bodies;
 * function-for-function against the game's own ITStruct (it/ittypes.h),
 * and every function names its decomp line range.
 *
 * This is the FIRST of the ten "stage items" -- entries 22
 * to 31 of dITManagerProcMakeList, the block its own comment labels
 * "Stage items" and every one of which was NULL -- and it is
 * the one of the ten that needs NO new assets at all. Its
 * `ItemAttributes` is ITCommonData 0xcf0 and its model block is
 * ITCommonObject 0x7648, and both have been inside `romdisk/itcommon.itp`
 * since the pack was built: `tools/export/ssb_itemmodelexport.py --list` has
 * carried a `GBumper 0x7648` row the whole time, because GBumper's
 * attributes sit in ITCommonData like every common item's. So this file
 * is pure logic on assets the port already ships.
 *
 * THE OTHER NINE ARE NOT, and that is the shape of what is left. Their
 * attributes live in the STAGE files -- `llGRInishieMap*` for the
 * Piranha Plant and the POW Block, `llGRYamabukiMap*` for Saffron City's
 * five Pokémon, `llGRBonus3Map*` for Race to the Finish -- and the port's
 * stage pack carries a map tree, its scripts, its item weights and its
 * attack coll, and nothing else. Carrying an item's attributes and its
 * model inside a .stg is the next step's work.
 *
 * Why this one is worth doing on its own: it is Peach's Castle's hazard.
 * It is also 116 lines with no new dependency -- every call
 * it makes (itManagerMakeItem, itMainClearOwnerStats, the item pack's own
 * attribute lookup) is already in the build.
 *
 * DIVERGES: none. The two `dobj->mobj->palette_id` writes are the MObj
 * class -- `gcSetupCustomDObjs` gives every DObj
 * of a table with `p_mobjsubs` an MObj, GBumper's table is one of the
 * twelve, and the pack now carries the section and itemModelAddToGObj
 * now calls dc_model_add_mobjs. The writes land on real memory.
 *
 * The desc's own third field is written `(intptr_t)llITCommonDataGBumper-
 * ItemAttributes` rather than the decomp's `&llITCommonDataGBumperItem-
 * Attributes`: the port's macro is the NUMBER (see itemoffsets.h), and
 * `&0xcf0` is not an lvalue. Every other ported item writes the same cast.
 */
#include <it/item.h>
#include "itemoffsets.h"     /* llITCommonDataGBumperItemAttributes */
#include <gr/ground.h>       /* nGRKindCastle */
#include <sc/scene.h>        /* gSCManagerBattleState */

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

/* it/itground/itgbumper.c:12-34 dITGBumperItemDesc, verbatim.
 *
 * `nGMAttackStateNew` puts the hitbox in its startup state, which is what
 * makes a fresh Bumper harmless to stand next to -- it is `itMain`'s own
 * attack-state machine, unchanged from the N64, that turns it on. */
ITDesc dITGBumperItemDesc =
{
    nITKindGBumper,                         /* Item Kind */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)llITCommonDataGBumperItemAttributes,   /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyRSca,         /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateNew,                      /* Hitbox Update State */
    itGBumperCommonProcUpdate,              /* Proc Update */
    NULL,                                   /* Proc Map */
    itGBumperCommonProcHit,                 /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    NULL                                    /* Proc Damage */
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/itgbumper.c:42-63 itGBumperCommonProcUpdate 0x8017D590,
 * verbatim.
 *
 * The whole of Peach's Castle's Bumper, frame by frame: after a hit, the
 * model holds at double scale for ITBUMPER_HIT_ANIM_LENGTH frames with
 * its second palette (the flash), then drops back to the first; and
 * `ip->multi` counts the hit's own growth down ten steps of a tenth, so
 * the Bumper springs back to its resting size over ten frames.
 *
 * The two states are mutually exclusive in the decomp's own if/else and
 * the port keeps it: the palette test is `hit_anim_length == 0`, so a
 * Bumper that was never hit takes the ELSE branch and DECREMENTS
 * `hit_anim_length` from zero every frame it is out. That is the game's
 * arithmetic, not the port's, and it is harmless -- the field is only
 * ever read for equality with zero -- but it is why the branch cannot be
 * "tidied" into a straight test. */
sb32 itGBumperCommonProcUpdate(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    if ((ip->item_vars.bumper.hit_anim_length == 0) && (dobj->mobj->palette_id == 1.0F))
    {
        dobj->mobj->palette_id = 0;
    }
    else ip->item_vars.bumper.hit_anim_length--;

    if (ip->multi != 0)
    {
        dobj->scale.vec.f.x = dobj->scale.vec.f.y = ( 2.0F - ( (10 - ip->multi) * 0.1F ) );

        ip->multi--;
    }
    else dobj->scale.vec.f.x = dobj->scale.vec.f.y = 1;

    return FALSE;
}

/* it/itground/itgbumper.c:65-81 itGBumperCommonProcHit 0x8017D63C,
 * verbatim: what a fighter touching it does to it. The scale is written
 * to 2.0 outright rather than through the interpolator above -- the
 * ProcUpdate's own `ip->multi` arm is what walks it back down -- and
 * `ITBUMPER_HIT_SCALE` (10) is the count that arm counts from. */
sb32 itGBumperCommonProcHit(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    dobj->scale.vec.f.x = 2.0F;
    dobj->scale.vec.f.y = 2.0F;

    ip->item_vars.bumper.hit_anim_length = ITBUMPER_HIT_ANIM_LENGTH;

    dobj->mobj->palette_id = 1.0F;

    ip->multi = ITBUMPER_HIT_SCALE;

    return FALSE;
}

/* it/itground/itgbumper.c:83-116 itGBumperMakeItem 0x8017D67C, verbatim.
 *
 * A stage item and not a fighter's: `itMainClearOwnerStats` throws away
 * whatever parent the spawn carried, and the collision is set to FIGHTER
 * only -- a Bumper cannot be picked up, thrown, or hit by another item,
 * and the flag is what says so. Its velocity is zeroed because the
 * caller's own (`&yakumono_pos`'s sibling `vel` in grcastle.c) is the
 * stage builder's, and Peach's Castle's Bumpers do not drift.
 *
 * `ITBUMPER_CASTLE_KNOCKBACK`/`_ANGLE` are applied HERE and not in the
 * table, which is the whole reason this function reads
 * `gSCManagerBattleState->gkind`: the same Bumper kind is a weaker
 * object on the bonus stages (gr/grbonus/grbonus3.c:32), and the stage is
 * what decides. `ITBUMPER_CASTLE_ANGLE` is 361, the game's own sentinel
 * for "the default angle, computed from the hit's own geometry" rather
 * than a real direction. */
GObj* itGBumperMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITGBumperItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip;
        DObj *dobj;

        itMainClearOwnerStats(item_gobj);

        ip = itGetStruct(item_gobj);
        dobj = DObjGetStruct(item_gobj);

        ip->multi = 0;

        ip->attack_coll.interact_mask = GMHITCOLLISION_FLAG_FIGHTER;
        ip->attack_coll.can_rehit_shield = TRUE;

        ip->physics.vel_air.x = 0.0F;
        ip->physics.vel_air.y = 0.0F;
        ip->physics.vel_air.z = 0.0F;

        dobj->mobj->palette_id = 0;

        if (gSCManagerBattleState->gkind == nGRKindCastle)
        {
            ip->attack_coll.knockback_weight = ITBUMPER_CASTLE_KNOCKBACK;
            ip->attack_coll.angle = ITBUMPER_CASTLE_ANGLE;
        }
    }
    return item_gobj;
}
