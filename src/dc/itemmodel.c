/* itemmodel.c -- see itemmodel.h. The table below was emitted by
 * tools/export/ssb_itemmodelexport.py --emit-table, keyed by the item's
 * `o_attributes`; re-run it rather than editing a row by hand.
 *
 * Every one of the 34 rows has a pack. Heart and Sword were NULL rows for
 * one step -- their `dl` is a DObjDLLink array rather than a display list,
 * so read as commands it disassembled to gsDPNoOpTag and gsDPNoOp and
 * baked to nothing (tools/export/ssb_itemmodelexport.py dobj_dl_links has the
 * whole story). A NULL row is still the shape a loader wants for an item
 * with no model, and itManagerMakeItem treats -1 as "no data" either way.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/debug.h>            /* syDebugPrintf, both builds */

#include "fighter.h"
#include "itemmodel.h"
#include "objmodel.h"
#include "objpvr.h"               /* DCDisplay: which pack joint a DObj is */
#include "itempack.h"             /* itemPackBlock: where a swapped-in list is */

#include <sys/objman.h>           /* gcGetTreeDObjNext */

typedef struct ITModelRow
{
    intptr_t o_attributes;        /* the key: the item's own table offset */
    const char *pack;             /* NULL when this item has no model yet */
    Fighter *loaded;
    sb32 is_loaded;
} ITModelRow;

/* NOT const: `loaded` and `is_loaded` are this table's own state,
 * written on first use. A `const` one lands in .rodata and the first
 * load dies storing into it. */
static ITModelRow sITModels[] =
{
    { 0x50,    "itcapsule.mdl",       NULL, FALSE },   /* Capsule */
    { 0xB8,    "ittomato.mdl",        NULL, FALSE },   /* Tomato */
    { 0x100,   "itheart.mdl",         NULL, FALSE },   /* Heart */
    { 0x148,   "itstar.mdl",          NULL, FALSE },   /* Star */
    { 0x190,   "itsword.mdl",         NULL, FALSE },   /* Sword */
    { 0x1D8,   "itbat.mdl",           NULL, FALSE },   /* Bat */
    { 0x220,   "itharisen.mdl",       NULL, FALSE },   /* Harisen */
    { 0x268,   "itlgun.mdl",          NULL, FALSE },   /* LGun */
    { 0x2E4,   "itfflower.mdl",       NULL, FALSE },   /* FFlower */
    { 0x374,   "ithammer.mdl",        NULL, FALSE },   /* Hammer */
    { 0x3BC,   "itmsbomb.mdl",        NULL, FALSE },   /* MSBomb */
    { 0x424,   "itbombhei.mdl",       NULL, FALSE },   /* BombHei */
    { 0x48C,   "itstarrod.mdl",       NULL, FALSE },   /* StarRod */
    { 0x53C,   "itgshell.mdl",        NULL, FALSE },   /* GShell */
    { 0x584,   "itrshell.mdl",        NULL, FALSE },   /* RShell */
    { 0x5CC,   "itbox.mdl",           NULL, FALSE },   /* Box */
    { 0x634,   "ittaru.mdl",          NULL, FALSE },   /* Taru */
    { 0x6E4,   "itmball.mdl",         NULL, FALSE },   /* MBall */
    { 0x72C,   "itwark.mdl",          NULL, FALSE },   /* Wark */
    { 0x7A8,   "itkabigon.mdl",       NULL, FALSE },   /* Kabigon */
    { 0x7F0,   "ittosakinto.mdl",     NULL, FALSE },   /* Tosakinto */
    { 0x838,   "itmew.mdl",           NULL, FALSE },   /* Mew */
    { 0x880,   "itnyars.mdl",         NULL, FALSE },   /* Nyars */
    { 0x8FC,   "itlizardon.mdl",      NULL, FALSE },   /* Lizardon */
    { 0x98C,   "itspear.mdl",         NULL, FALSE },   /* Spear */
    { 0xA08,   "itkamex.mdl",         NULL, FALSE },   /* Kamex */
    { 0xA84,   "itmlucky.mdl",        NULL, FALSE },   /* MLucky */
    { 0xACC,   "itegg.mdl",           NULL, FALSE },   /* Egg */
    { 0xB34,   "itstarmie.mdl",       NULL, FALSE },   /* Starmie */
    { 0xBB0,   "itsawamura.mdl",      NULL, FALSE },   /* Sawamura */
    { 0xBF8,   "itdogas.mdl",         NULL, FALSE },   /* Dogas */
    { 0xC74,   "itpippi.mdl",         NULL, FALSE },   /* Pippi */
    { 0xCF0,   "itgbumper.mdl",       NULL, FALSE },   /* GBumper */
    { 0x69C,   "itnbumper.mdl",       NULL, FALSE },   /* NBumper */
    /* The STAGE items: the same table, keyed by the
     * offsets their own `o_attributes` carry in their STAGE's map file
     * rather than by ITCommonData's. The pack carries their attributes
     * under those keys (tools/export/ssb_itemexport.py's STAGE_ITEMS), so the
     * key `itemModelAddToGObj` is handed is the same number either way,
     * and a row here is all the model path needs. */
    { 0xD8,    "itpowerblock.mdl",    NULL, FALSE },   /* PowerBlock */
    { 0x120,   "itpakkun.mdl",        NULL, FALSE },   /* Pakkun */
    { 0xBC,    "itglucky.mdl",        NULL, FALSE },   /* GLucky */
    { 0x104,   "itmarumine.mdl",      NULL, FALSE },   /* Marumine */
    { 0x1FC,   "ithitokage.mdl",      NULL, FALSE },   /* Hitokage */
    { 0x278,   "itfushigibana.mdl",   NULL, FALSE },   /* Fushigibana */
    { 0x16C,   "itporygon.mdl",       NULL, FALSE },   /* Porygon */
    /* Race to the Finish's barrel bomb, the eighth
     * stage item and the only one whose stage is a bonus stage. Its
     * ITAttributes is at 0xA8 of relocData 295 and its tree at 0x788
     * of 162; ittarubomb.c's ITDesc carries the same 0xA8. */
    { 0xA8,    "ittarubomb.mdl",      NULL, FALSE },   /* TaruBomb */
    /* Break the Targets' target, the ninth stage item and
     * the only one whose key is ZERO -- which is the decomp's own
     * number, because its ITAttributes is the WHOLE of relocData 253 and
     * so sits at offset 0 of it. Nothing else in this table can collide:
     * ITCommonData's own lowest table is 0x50. Its tree is relocData
     * 150's at 0x10F8. */
    { 0x0,     "ittarget.mdl",        NULL, FALSE },   /* Target */
    /* PK Fire's pillar, keyed by the 0x34 its table sits at in NessSpecial1 --
     * the key the item pack's record carries too (tools/export/ssb_itemexport.py
     * FIGHTER_ITEMS) -- and baked out of NessSpecial3 with its AnimJoint table
     * beside the tree (tools/export/ssb_itemmodelexport.py FIGHTER_ITEMS). */
    { 0x34,    "itnesspkfire.mdl",    NULL, FALSE },   /* NessPKFire */
    /* Link's Bomb at 0x40 in LinkMain, its tree and AnimJoint table
     * out of relocData 353. */
    { 0x40,    "itlinkbomb.mdl",      NULL, FALSE },   /* LinkBomb */
};

/* One palette bank for every item pack, shared: an item's textures are
 * its own 16-colour tables and nothing swaps them per player the way a
 * fighter's costume does, so the bank is handed back to 0 before each
 * load rather than walked. */
#define ITEM_PAL_BANK 0

static ITModelRow *item_model_row(intptr_t o_attributes)
{
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sITModels); i++)
    {
        if ((sITModels[i].o_attributes == o_attributes) &&
            (sITModels[i].pack != NULL))
        {
            return &sITModels[i];
        }
    }
    return NULL;
}

static Fighter *item_model_load(ITModelRow *row)
{
    int pal_bank = ITEM_PAL_BANK;
    Fighter *pack;

    if (row->is_loaded)
    {
        return row->loaded;
    }
    pack = malloc(sizeof(Fighter));

    if (pack == NULL)
    {
        syDebugPrintf("itemmodel: no room for %s's Fighter\n", row->pack);
        return NULL;
    }
    if (fighter_load(pack, row->pack, &pal_bank) != 0)
    {
        free(pack);
        return NULL;
    }
    row->loaded = pack;
    row->is_loaded = TRUE;

    return pack;
}

int itemModelAddToGObj(GObj *gobj, intptr_t o_attributes)
{
    ITModelRow *row = item_model_row(o_attributes);
    Fighter *pack;
    int n;

    if (row == NULL)
    {
        return -1;
    }
    pack = item_model_load(row);

    if (pack == NULL)
    {
        return -1;
    }
    /* `parent` NULL: an item's whole tree is the model, the same shape a
     * stage's is (src/dc/stage.c's bind), not a fighter's, which starts
     * at a TopN the pack does not carry.
     *
     * The transform kinds are dc_model_add_dobjs's own three -- Tra,
     * RotRpyR, Sca -- and NOT the item desc's `transform_types` triple,
     * which the decomp hands gcAddDObj3TransformsKind. Those triples are
     * `nGCMatrixKindTraRotRpyR, Null, Null` on every desc this port has
     * (itstar.c, ittomato.c, itsword.c, ...), which is the same T+R+S the
     * port's three XObjs already are; the difference is the count, not
     * the transform. Rewriting the kinds here would be rewriting them to
     * what they already are. */
    n = dc_model_add_dobjs(gobj, NULL, pack, NULL);

    if (n < 0)
    {
        return n;
    }
    /* THE MObjs, which the game attaches while it builds the tree and
     * which this port's item path used not to attach at all.
     * Twelve of the 34 tables carry `p_mobjsubs`, and four ported files
     * write through `dobj->mobj` unconditionally -- itgshell.c,
     * itrshell.c, itbombhei.c, itnbumper.c -- so with no MObj the target
     * stores `palette_id` past address zero.
     *
     * IT MUST BE CALLED HERE, before itManagerMakeItem's eject, and not
     * after: dc_model_add_mobjs_alt indexes `p_subs` by PACK JOINT INDEX
     * and gcAddMObjAll walks the tree in that same order, so the tree has
     * to still be the one dc_model_add_dobjs just built. After the eject
     * every joint is one lower and every MObj lands on the wrong DObj --
     * silently, with the picture still drawn. The same reason the eject
     * has to stay where it is (see itManagerMakeItem).
     *
     * `0.0F`: an item has no MatAnimJoint of its own to start on; its
     * attr->anim_joints are played by gcAddAnimAll in itManagerMakeItem,
     * which is the decomp's own split. */
    dc_model_add_mobjs(gobj, pack, 0.0F);

    return n;
}

/* itemmodel.h says why. The pack's animation 0 as one AObjEvent32* per
 * pack joint, rebuilt from its word indexes (FPackAnim.off_entries, -1 for
 * a joint with no script) the way src/dc/efmanager.c efModelAnimJoint does
 * for an effect's, into a table of this file's own that gcAddAnimAll reads
 * once and does not keep. */
static AObjEvent32 *sITModelAnimJoints[FIGHTER_MAX_JOINTS];

AObjEvent32 **itemModelAnimJoints(intptr_t o_attributes)
{
#ifndef FT_HOSTTEST
    ITModelRow *row = item_model_row(o_attributes);
    const Fighter *f;
    const FPackAnim *anim;
    const s32 *entries;
    u32 i;

    if ((row == NULL) || (row->is_loaded == FALSE) || (row->loaded == NULL))
    {
        return NULL;
    }
    f = row->loaded;

    if (f->hd->anim_count == 0)
    {
        return NULL;
    }
    anim = &f->anims[0];
    entries = (const s32 *)((const u8 *)f->blob + anim->off_entries);

    for (i = 0; i < ARRAY_COUNT(sITModelAnimJoints); i++)
    {
        sITModelAnimJoints[i] = NULL;

        if ((i < f->hd->joint_count) && (entries[i] >= 0) &&
            ((u32)entries[i] < anim->nwords))
        {
            sITModelAnimJoints[i] =
                (AObjEvent32 *)((u8 *)f->blob + anim->off_words) + entries[i];
        }
    }
    return sITModelAnimJoints;
#else
    /* The host's AObjEvent32 holds a void * and is eight bytes, where the
     * pack's words are four: the interpreter would walk them at double
     * stride. Nothing to attach, as efModelAnimJoint's host arm. */
    (void)o_attributes;
    (void)sITModelAnimJoints;
    return NULL;
#endif
}

void itemModelResetTransforms(DObj *root)
{
    DObj *dobj;

    for (dobj = root; dobj != NULL; dobj = gcGetTreeDObjNext(dobj))
    {
        const DCDisplay *disp = dobj->dv;
        const Fighter *pack;
        const FPackJoint *pj;

        /* The hand joint the hold splices in above the item carries no
         * payload, and neither does a DObj some other path made. Ask
         * each DObj which pack joint it IS rather than counting the walk:
         * itManagerMakeItem ejects the tree's root after building it, so
         * a count would be one out from the first joint on, silently. */
        if (disp == NULL || disp->model == NULL)
        {
            continue;
        }
        pack = disp->model;

        if (disp->joint < 0 || (u32)disp->joint >= pack->hd->joint_count)
        {
            continue;
        }
        pj = &pack->joints[disp->joint];

        dobj->translate.vec.f.x = pj->t[0];
        dobj->translate.vec.f.y = pj->t[1];
        dobj->translate.vec.f.z = pj->t[2];
        dobj->rotate.vec.f.x = pj->r[0];
        dobj->rotate.vec.f.y = pj->r[1];
        dobj->rotate.vec.f.z = pj->r[2];
        dobj->scale.vec.f.x = pj->s[0];
        dobj->scale.vec.f.y = pj->s[1];
        dobj->scale.vec.f.z = pj->s[2];
    }
}

/* The one named effect (itemmodel.h). Not in the table above: that table is
 * keyed by an item's `o_attributes`, and this has none. */
static Fighter *sITBoxSmash;

/* ... and Race to the Finish's, which is the same thing one stage over:
 * itTaruBombContainerSmashMakeEffect is itBoxContainerSmashMakeEffect
 * with `DisplayList TaruBombEffect` in place of `EffectDisplayList Box`.
 * A quad either way, so the port's side is this
 * function with a different name. */
static Fighter *sITTaruBombSmash;

Fighter *itemModelTaruBombSmash(void)
{
    int pal_bank = ITEM_PAL_BANK;
    Fighter *pack;

    if (sITTaruBombSmash != NULL)
    {
        return sITTaruBombSmash;
    }
    pack = malloc(sizeof(Fighter));

    if (pack == NULL)
    {
        syDebugPrintf("itemmodel: no room for ittarubombsmash.mdl's "
                      "Fighter\n");
        return NULL;
    }
    if (fighter_load(pack, "ittarubombsmash.mdl", &pal_bank) != 0)
    {
        free(pack);
        return NULL;
    }
    sITTaruBombSmash = pack;

    return pack;
}

Fighter *itemModelBoxSmash(void)
{
    int pal_bank = ITEM_PAL_BANK;
    Fighter *pack;

    if (sITBoxSmash != NULL)
    {
        return sITBoxSmash;
    }
    pack = malloc(sizeof(Fighter));

    if (pack == NULL)
    {
        syDebugPrintf("itemmodel: no room for itboxsmash.mdl's Fighter\n");
        return NULL;
    }
    if (fighter_load(pack, "itboxsmash.mdl", &pal_bank) != 0)
    {
        free(pack);
        return NULL;
    }
    sITBoxSmash = pack;

    return pack;
}

/* The display lists an item swaps in, by the ITCommonObject block the
 * decomp's itGetPData lands on, and the one-joint pack each was baked to
 * (tools/export/ssb_itemmodelexport.py ALT_DISPLAY_LISTS --alts). The payloads
 * are this table's own, not the scene heap's, because the packs are kept for
 * the run like every item's. */
typedef struct ITModelAlt
{
    const char *block;
    const char *pack;
    Fighter *loaded;
    DCDisplay disp;
    sb32 told;                    /* the first switch is logged */

} ITModelAlt;

static ITModelAlt sITModelAlts[] =
{
    { "DisplayList BombHeiWalkLeft",  "italtbombheiwalkl.mdl", NULL, { 0 }, FALSE },
    { "DisplayList BombHeiWalkRight", "italtbombheiwalkr.mdl", NULL, { 0 }, FALSE },
    { "DisplayList NBumperWait",      "italtnbumperwait.mdl",  NULL, { 0 } },
    { "DisplayList Kamex",            "italtkamex.mdl",        NULL, { 0 } },
    { "DisplayList Sawamura",         "italtsawamura.mdl",     NULL, { 0 } },
    { "DisplayList Wark",             "italtwark.mdl",         NULL, { 0 } },
};

#ifndef FT_HOSTTEST
static Fighter *item_model_alt_load(ITModelAlt *alt)
{
    int pal_bank = ITEM_PAL_BANK;
    Fighter *pack;

    if (alt->loaded != NULL)
    {
        return alt->loaded;
    }
    pack = malloc(sizeof(Fighter));

    if (pack == NULL)
    {
        syDebugPrintf("itemmodel: no room for %s's Fighter\n", alt->pack);
        return NULL;
    }
    if (fighter_load(pack, alt->pack, &pal_bank) != 0)
    {
        free(pack);
        return NULL;
    }
    dc_model_init_payload(&alt->disp, pack, 0);
    alt->loaded = pack;

    return pack;
}
#endif

/* itemmodel.h says why, and what the host arm is. */
void itemModelSetDisplayList(DObj *dobj, Gfx *dl)
{
#ifdef FT_HOSTTEST
    dobj->dl = dl;
#else
    static sb32 warned = FALSE;
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sITModelAlts); i++)
    {
        ITModelAlt *alt = &sITModelAlts[i];

        if ((const void *)dl !=
            itemPackBlock(ITEM_PACK_REGION_MODELS, alt->block, NULL))
        {
            continue;
        }
        if (item_model_alt_load(alt) != NULL)
        {
            if (dobj->dv != &alt->disp && alt->told == FALSE)
            {
                alt->told = TRUE;
                syDebugPrintf("itemmodel: %s switched to %s\n",
                              alt->block, alt->pack);
            }
            dobj->dv = &alt->disp;
        }
        return;
    }
    /* A list with no baked pack: keep drawing what the item had, which is
     * still a model, rather than N64 bytes. */
    if (warned == FALSE)
    {
        warned = TRUE;
        syDebugPrintf("itemmodel: no baked model for the display list at "
                      "%p; the item keeps its own\n", (void *)dl);
    }
#endif
}

/* Every item's pack and the Container debris, now (itemmodel.h).
 *
 * The preload, and why (DIVERGES). On the N64 none of this is a load
 * at all: the models live in relocData files the battle has loaded
 * before the first tic -- the common effect and item files, each
 * fighter's Special files -- and making one is pointer arithmetic. The
 * port bakes a pack per model and used to read each off the disc the
 * first time it was made, in the middle of a match: a pause on the game
 * thread while the drive seeks, and the read path behind the Poke Ball
 * freeze (src/dc/assetroot.c asset_read). scVSBattleStartBattle calls
 * this beside the rest of the battle's loading, so a match reads
 * nothing; what is loaded is kept for the run, as it always was. Every
 * row, not the ones this roster and this stage can reach: the set is
 * small, and a census would be one more thing to fall out of date. */
void itemModelPreloadAll(void)
{
#ifndef FT_HOSTTEST
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sITModels); i++)
    {
        if (sITModels[i].pack != NULL)
        {
            (void)item_model_load(&sITModels[i]);
        }
    }
    (void)itemModelBoxSmash();
    /* Race to the Finish's barrels: the first one broken used to read
     * this off the disc mid-match; gameplay does no disc reads */
    (void)itemModelTaruBombSmash();
    for (i = 0; i < ARRAY_COUNT(sITModelAlts); i++)
    {
        (void)item_model_alt_load(&sITModelAlts[i]);
    }
#endif
}

void itemModelRelease(void)
{
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sITModels); i++)
    {
        ITModelRow *row = &sITModels[i];

        if (!row->is_loaded)
        {
            continue;
        }
        fighter_release(row->loaded);
        free(row->loaded);
        row->loaded = NULL;
        row->is_loaded = FALSE;
    }
    if (sITBoxSmash != NULL)
    {
        fighter_release(sITBoxSmash);
        free(sITBoxSmash);
        sITBoxSmash = NULL;
    }
    if (sITTaruBombSmash != NULL)
    {
        fighter_release(sITTaruBombSmash);
        free(sITTaruBombSmash);
        sITTaruBombSmash = NULL;
    }
    for (i = 0; i < ARRAY_COUNT(sITModelAlts); i++)
    {
        if (sITModelAlts[i].loaded != NULL)
        {
            fighter_release(sITModelAlts[i].loaded);
            free(sITModelAlts[i].loaded);
            sITModelAlts[i].loaded = NULL;
        }
    }
}
