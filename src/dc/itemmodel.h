/* itemmodel.h -- an item's DObj tree, out of the baked pack.
 *
 * `itManagerMakeItem` builds every item's tree off `ITAttributes.data`,
 * which is a `DObjDesc` array in ITCommonObject whose entries carry N64
 * display lists (`gcSetupCustomDObjsWithMObj` on the common arm,
 * `itManagerSetupItemDObjs` on the `is_item_dobjs` one). The port draws
 * nothing from a display list -- `gcSubmitDObj` (src/dc/objdisplay.c)
 * submits `dobj->dv`, the BAKED model, and `gcDrawDObjForGObj` skips a
 * DObj whose `dv` is NULL -- so an item built that way has the right
 * attributes, the right collision and no picture at all. This is the
 * other half: the trees baked by tools/export/ssb_itemmodelexport.py, loaded
 * and hung on the GObj the way src/dc/objmodel.c hangs a stage's.
 */
#ifndef SSB_DC_ITEMMODEL_H
#define SSB_DC_ITEMMODEL_H

#include <sys/obj.h>

#include "fighter.h"          /* Fighter, the port's loaded-model type */

/* Hang the baked model for the item whose attribute table lives at
 * `o_attributes` in ITCommonData -- the same offset this pack's table is
 * keyed by and the same one the item pack's own records carry, so the two
 * packs agree by construction and neither needs a name-to-kind mapping.
 *
 * The pack is loaded on first use and kept, the way src/dc/efmanager.c's
 * efModelLoad keeps an effect's (`fighter_load` into a malloc'd Fighter,
 * not a static -- a Fighter is 17,400 bytes and mostly 40-joint arrays,
 * and the effect path learned that one static of them in .bss costs a
 * thread). 34 of those is 591 KB if every kind is ever spawned, which is
 * the number to watch if this ever wants to shrink.
 *
 * Returns the number of DObjs made, or -1 with the reason logged -- an
 * `o_attributes` this pack has no model for (Heart and Sword bake to
 * nothing; see tools/export/ssb_itemmodelexport.py), or one whose pack will not
 * load. The caller falls back to a bare GObj, which is what the game does
 * for an item with no `data` at all. */
int itemModelAddToGObj(GObj *gobj, intptr_t o_attributes);

/* The AnimJoint table an item's pack carries, as the per-joint
 * AObjEvent32* array `ITAttributes.anim_joints` would be, or NULL when the
 * item has no pack, its pack is not loaded or it carries no animation --
 * which is every item but PK Fire's pillar. Call after itemModelAddToGObj.
 *
 * DIVERGES: the pillar's table is in NessSpecial3, which the item pack
 * does not carry, so its record's `anim_joints` is NO_PTR; and
 * it/itfighter/itnesspkfire.c is compiled unmodified, so there is no
 * MakeItem of the port's own to attach a named script in, the way
 * src/dc/itpowerblock.c does. The table is baked into itnesspkfire.mdl
 * instead (tools/export/ssb_itemmodelexport.py FIGHTER_ITEMS) and
 * itManagerMakeItem feeds this to its own gcAddAnimAll. The table is
 * this file's, overwritten by the next call. NULL on the host. */
AObjEvent32 **itemModelAnimJoints(intptr_t o_attributes);

/* Put an item's joints back to the pose the pack gave them, walking the
 * tree from `root` the way the object system does.
 *
 * This is the port's `gcSetDObjTransformsForGObj(item_gobj,
 * ip->attr->data)` (sys/objanim.c:2457), which it/itmain.c's
 * itMainSetFighterHold calls before an item goes into a hand so that
 * whatever the item was doing on the floor -- spinning, mainly -- is not
 * carried into the fighter's grip. The game re-reads each entry of the
 * DObjDesc array it built the tree from; this port's `attr->data` is a
 * NULL/non-NULL marker rather than an array (src/dc/itmanager.c), so the
 * rest pose comes from the baked pack's own FPackJoint rows instead.
 *
 * Which row belongs to which DObj is asked of the DObj, not counted off
 * the walk: each one built by dc_model_add_dobjs carries a DCDisplay
 * naming its pack and its joint index, and itManagerMakeItem ejects the
 * tree's root afterwards, so counting would be one out from the first
 * joint on. A DObj with no payload -- the hand joint the hold splices in
 * above the item -- is skipped, and an item the pack has no model for is
 * left alone, which is the same nothing the game does with a NULL
 * `data`. */
void itemModelResetTransforms(DObj *root);

/* The Container smash debris -- `EFFECTS` in
 * tools/export/ssb_itemmodelexport.py, a bare display list in ITCommonObject
 * that is NOT an item's and so has no `o_attributes` to be keyed by. One model,
 * one joint, and itBoxContainerSmashMakeEffect gives that joint's payload to
 * each of ITCONTAINER_EFFECT_COUNT hand-made DObjs through
 * dc_model_hidden_payload. Loaded on first use and kept, like the items; NULL
 * if it will not load, in which case the debris is simply not created. */
Fighter *itemModelBoxSmash(void);

/* ... and Race to the Finish's barrel bomb's, the same one-quad pack. */
Fighter *itemModelTaruBombSmash(void);

/* The decomp's `dobj->dl = <display list>`, for the five items that swap
 * one in: the Bob-omb's walk left/right, the Bumper once it attaches,
 * and Blastoise's, Hitmonlee's and Onix's attack poses. `dl` and the
 * port's DCDisplay payload are the SAME union member (sys/objtypes.h,
 * DObj `dv`), so the verbatim write replaced the item's baked model
 * with raw N64 bytes, and the next gcSubmitDObj called through the
 * first of them -- 0xE7, gsDPPipeSync -- and took the machine down (a
 * four-CPU Saffron City match, tic 2986).
 *
 * DIVERGES: on the Dreamcast the list is looked up among the ones the
 * exporter baked (tools/export/ssb_itemmodelexport.py ALT_DISPLAY_LISTS, one
 * one-joint pack each) and the DObj is switched to that pack's joint;
 * dc_model_proc_display draws every pack a tree points into. A list with
 * no baked pack leaves the item's own model, logged once. The host has no
 * payload -- its item models are the raw file (hosttest_ft.c
 * sHostItemModels) -- so there it is the decomp's write, and the tests of
 * the offset arithmetic still see it. */
void itemModelSetDisplayList(DObj *dobj, Gfx *dl);

/* Load every item's pack and the debris now rather than on first use, so a
 * match reads nothing off the disc (src/dc/efmanager.c on why). Kept, like
 * a pack loaded on first use. A no-op on the host. */
void itemModelPreloadAll(void);

/* Give every loaded pack back. Safe to call twice. */
void itemModelRelease(void);

#endif /* SSB_DC_ITEMMODEL_H */
