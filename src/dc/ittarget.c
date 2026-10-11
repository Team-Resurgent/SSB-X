/* ittarget.c -- it/itground/ittarget.c, verbatim but for the ITDesc's
 * `p_file`: the target a Break the Targets course stands ten of.
 *
 * FIFTY-FIVE LINES IN THE DECOMP and not one of them is physics. A
 * target does nothing at all: no update proc, no map proc, no hit proc,
 * `nGMAttackStateOff` so it never swings a hitbox of its own. It has
 * exactly one behaviour, `itTargetCommonProcDamage` -- when something
 * damages it, it makes two effects, plays one sound, tells the scene to
 * decrement, and returns TRUE, which is what tells itMain to destroy
 * it. Ten of those and the course is over.
 *
 * `itTargetMakeItem` sets `ga = nMPKineticsGround` and
 * `floor_line_id = -1`: the target is a GROUND item that is on no floor
 * line. That pair is what pins it exactly where the course put it --
 * ground kinetics run no gravity, and the invalid line id means nothing
 * ever asks a floor to hold it up. A target hangs in mid-air because
 * the game tells it to.
 *
 * DIVERGES, one:
 *
 *   The decomp's `p_file` is `&gSC1PBonusStageItemFile`, the pointer
 *   sc1PBonusStageBonus1LoadFile fills with relocData 253 off the heap,
 *   and its offset is 0 because that file IS the ITAttributes table.
 *   The port has no reloc heap; the attributes are in the item pack
 *   under the decomp's own key, so `p_file` is `&gITManagerCommonData`
 *   and the offset stays 0 -- the same pair every stage item makes
 *   (src/dc/ittarubomb.c's first divergence, and the whole argument in
 *   src/dc/itporygon.c's header).
 *
 * The tree is the pack's too, keyed by the same 0 (relocData 150 at
 * 0x10F8 -> romdisk/ittarget.mdl). Nothing here is scripted: a target
 * that MOVES follows a script the COURSE hands it, attached by
 * sc1PBonusStageMakeTargets after the item exists, not one of its own.
 */
#include <it/item.h>
#include <ef/effect.h>            /* efManagerShieldBreakMakeEffect */
#include <ef/efmanager.h>         /* efManagerFireGrindMakeEffect */
#include <sc/scene.h>
#include <mp/mpdef.h>             /* nMPKineticsGround */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

#include "ftcommon.h"             /* func_800269C0_275C0 */
#include "sc1pbonusstage.h"       /* sc1PBonusStageUpdateTargetCount */

/* // // // // // // // // // // // //
 *                                   //
 *       INITIALIZED DATA            //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/ittarget.c:5-27 dITTargetItemDesc 0x8018F130, verbatim but
 * for `p_file`. Nine of its ten procs are NULL. */
ITDesc dITTargetItemDesc =
{
    nITKindTarget,                          /* Item Kind */
    /* DIVERGES: the decomp's `&gSC1PBonusStageItemFile`. */
    &gITManagerCommonData,                  /* Pointer to item file data? */
    (intptr_t)0,                            /* Offset of item attributes in file? */

    /* DObj transformation struct */
    {
        nGCMatrixKindTraRotRpyRSca,         /* Main matrix transformations */
        nGCMatrixKindNull,                  /* Secondary matrix transformations? */
        0                                   /* ??? */
    },

    nGMAttackStateOff,                      /* Hitbox Update State */
    NULL,                                   /* Proc Update */
    NULL,                                   /* Proc Map */
    NULL,                                   /* Proc Hit */
    NULL,                                   /* Proc Shield */
    NULL,                                   /* Proc Hop */
    NULL,                                   /* Proc Set-Off */
    NULL,                                   /* Proc Reflector */
    itTargetCommonProcDamage                /* Proc Damage */
};

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itground/ittarget.c:30-40 itTargetCommonProcDamage 0x8018EE10,
 * verbatim.
 *
 * The shield-break burst and the fire grind are the target's whole
 * death: the first is the white ring ftCommon plays when a shield goes
 * (src/dc/efmanager.c:7004), the second the sparks a fighter drags
 * along a floor (:3634). Returning TRUE is what destroys the item --
 * it/itmain.c reads this proc's return as "is this item finished". */
sb32 itTargetCommonProcDamage(GObj *item_gobj)
{
    efManagerShieldBreakMakeEffect(&DObjGetStruct(item_gobj)->translate.vec.f);
    efManagerFireGrindMakeEffect(&DObjGetStruct(item_gobj)->translate.vec.f);

    func_800269C0_275C0(nSYAudioFGMBonus1TargetBreak);

    sc1PBonusStageUpdateTargetCount();

    return TRUE;
}

/* it/itground/ittarget.c:43-55 itTargetMakeItem 0x8018EE5C, verbatim.
 * The two writes under the make are this file's whole physics; the
 * header says what they do. */
GObj* itTargetMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags)
{
    GObj *item_gobj = itManagerMakeItem(parent_gobj, &dITTargetItemDesc, pos, vel, flags);

    if (item_gobj != NULL)
    {
        ITStruct *ip = itGetStruct(item_gobj);

        ip->ga = nMPKineticsGround;
        ip->coll_data.floor_line_id = -1;
    }
    return item_gobj;
}
