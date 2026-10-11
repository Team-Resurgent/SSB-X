/* itmanager.c -- it/itmanager.c, the item manager: the ITStruct pool every
 * spawned item is drawn from, and the two pieces of it/'s own bookkeeping
 * that touch no unexported asset -- the container-drop weight table and
 * the Poké Ball monster index. Function-for-function against the game's
 * own ITStruct (it/ittypes.h); every function names its decomp line range.
 *
 * itManagerInitItems unconditionally calls lbRelocGetExternHeapFile on
 * llITCommonDataFileID (ITCommonData) and ifCommonItemArrowSetAttr (which
 * itself loads IFCommonItem), with no guard. Both files are exported --
 * ITCommonData as region 0 of the item pack (src/dc/itempack.h) and
 * IFCommonItem as an ordinary sprite bank (romdisk/ifcommonitem.spr) --
 * so it ports whole, and the host test calls the real one. The pool it
 * builds is the same free list itManagerGetNextStructAlloc/SetPrevStructAlloc walk,
 * which is what those two touch and nothing else, exactly as their wp
 * twins do.
 *
 * itManagerSetupContainerDrops and itManagerInitMonsterVars are ported
 * whole and verbatim: neither touches an asset. SetupContainerDrops reads
 * gMPCollisionGroundData->item_weights, which src/dc/stage.c's own
 * MPGroundData stand-in fills from the stage pack; a ground file with no
 * weights takes the decomp's own `item_weights == NULL` early-out and
 * zeroes gITManagerRandomWeights.weights_sum.
 *
 * The keystone itManagerMakeItem takes its ITDesc
 * (the file pointer and attribute offset) as a PARAMETER rather than
 * reaching for an unexported global, the same shape wpManagerAddModel
 * used for WPDesc. So it ports whole and verbatim, along
 * with its one self-contained helper (itManagerSetupItemDObjs, used only
 * on the `attr->is_item_dobjs` arm) and itManagerSetItemSpawnWait (touches
 * only ITAppearActor.spawn_wait and the two appearance-rate tables, no
 * roster). src/dc/lbcommon.c's lbCommonEjectTreeDObj serves MakeItem's
 * asset-attached branch.
 *
 * The table's shape is settled by a LOOP BOUND:
 * itManagerMakeAppearActor builds the appearance weights over kinds 0 to
 * 19 (`nITKindCommonStart`..`nITKindCommonEnd`), so those twenty are the
 * ones that must be real and the other twenty-five are unreachable -- the
 * port's stage files have all omitted the call sites that would reach
 * them. See dITManagerProcMakeList's own comment.
 *
 * The SPAWNER is here: itManagerAppearActorProcUpdate
 * and itManagerMakeAppearActor, which are what put an item on the field
 * without a stage or a container asking for one. grCommonSetupInitAll
 * calls the maker at the tail, in the game's own place, so a battle with
 * the switch on has items appearing on their own.
 *
 * itManagerInitItems carries the one guard its own file can fail
 * (itemPackLoad) and the `#ifdef FT_HOSTTEST` arm the other three bank
 * loads in the port also carry.
 *
 * itManagerMakeItem's call graph includes mpcommon.c's
 * mpCommonRunItemCollisionDefault (mp/mpcommon.c:968-976), the item twin
 * of mpCommonRunFighterCollisionDefault/RunWeaponCollisionDefault (both
 * already in the build) that its `flags & ITEM_FLAG_COLLPROJECT` arm
 * calls by name -- the same three-line shape as its two siblings. The display procs
 * it can pick via `attr->is_display_colanim`/`is_display_xlu` are all in
 * itdisplay.c.
 */
#include <it/item.h>
#include <wp/weapon.h>
#include <ft/fighter.h>
#include <mp/map.h>
#include <sc/scene.h>
#include <lb/library.h>
#include <reloc_data.h>
#include <ef/efparticle.h>      /* efParticleGetLoadBankID */
#include <if/ifcommon.h>        /* ifCommonItemArrowSetAttr */

#include "itempack.h"           /* the item pack, in place of ITCommonData */
#include "itemmodel.h"          /* the baked item models */
#include <ef/effect.h>          /* efManagerItemSpawnSwirlMakeEffect */
#include "lbpartex.h"           /* lbpTexLoadBank */

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

#include <sys/taskman.h>
#include <sys/develop.h>
#include <sys/debug.h>        /* syDebugPrintf, the itweights probe */

/* // // // // // // // // // // // //
 *                                   //
 *   GLOBAL / STATIC VARIABLES       //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmanager.c:19-27 dITManagerAppearanceRatesMin 0x80189454, verbatim.
 * Indexed by gSCManagerBattleState->item_appearance_rate. */
u16 dITManagerAppearanceRatesMin[] =
{
    I_SEC_TO_TICS(0),
    I_SEC_TO_TICS(30),
    I_SEC_TO_TICS(25),
    I_SEC_TO_TICS(20),
    I_SEC_TO_TICS(15),
    I_SEC_TO_TICS(10)
};

/* it/itmanager.c:30-38 dITManagerAppearanceRatesMax 0x80189460, verbatim. */
u16 dITManagerAppearanceRatesMax[] =
{
    I_SEC_TO_TICS(0),
    I_SEC_TO_TICS(30) + 90,
    I_SEC_TO_TICS(25) + 75,
    I_SEC_TO_TICS(20) + 60,
    I_SEC_TO_TICS(15) + 45,
    I_SEC_TO_TICS(10) + 30
};

/* it/itmanager.c:43-112 dITManagerProcMakeList 0x8018946C, in nITKind
 * order and nITKindEnumCount (45) entries long.
 *
 * THE ENTRIES THE ITEM SWITCH CAN PICK MUST BE REAL, and the rest are
 * NULL. That boundary is not a judgement call, it is a loop bound:
 * itManagerMakeAppearActor builds the appearance weights over
 * `i = nITKindCommonStart` to `nITKindCommonEnd` (it/itmanager.c:598), so
 * the switch selects kinds 0 to 19 and nothing else -- the four Containers
 * and the sixteen utility items. Those twenty are the ones this port must
 * have. The other twenty-five are reached only by stage code, and the
 * port's stage files have all omitted those call sites deliberately (each
 * one's header says so), so no NULL here is reachable today.
 *
 * A NULL IS NOT A SILENT NO-OP. itManagerMakeItemSetupCommon below calls
 * the entry UNGUARDED, so reaching one is an immediate crash -- which is
 * louder than the compile error a half-written table would give and much
 * louder than an item that quietly does nothing, and is the right failure
 * for a kind the port cannot make.
 *
 * The two NULLs the GAME itself writes -- Ness' PK Fire and Link's Bomb,
 * kinds 20 and 21 -- stay NULL for the game's own reason: those are
 * items a fighter's own move makes through a different path, and the
 * decomp's initialiser says so.
 *
 * GBumper (kind 24) is the one stage item with an entry: Peach's Castle's
 * hazard (gr/grcommon/grcastle.c:57), whose attributes and model are in
 * the pack the port ships. The other nine wait on the stage pack carrying
 * their attributes, not on anything in it/. */
GObj* (*dITManagerProcMakeList[/* */])(GObj*, Vec3f*, Vec3f*, u32) =
{
    /* Containers -- the first four the switch can pick */
    itBoxMakeItem,          /* Crate */
    itTaruMakeItem,         /* Barrel */
    itCapsuleMakeItem,      /* Capsule */
    itEggMakeItem,          /* Egg */

    /* Usable items */
    itTomatoMakeItem,       /* Maxim Tomato */
    itHeartMakeItem,        /* Heart Container */
    itStarMakeItem,         /* Star Man */
    itSwordMakeItem,        /* Beam Sword */
    itBatMakeItem,          /* Home Run Bat */
    itHarisenMakeItem,      /* Fan */
    itStarRodMakeItem,      /* Star Rod */
    itLGunMakeItem,         /* Ray Gun */
    itFFlowerMakeItem,      /* Fire Flower */
    itHammerMakeItem,       /* Hammer */
    itMSBombMakeItem,       /* Motion-sensor Bomb */
    itBombHeiMakeItem,      /* Bob-omb */
    itNBumperMakeItem,      /* Normal Bumper */
    itGShellMakeItem,       /* Green Shell */
    itRShellMakeItem,       /* Red Shell */
    itMBallMakeItem,        /* Poké Ball */

    /* Fighter items -- NULL in the game's own initialiser */
    NULL,                   /* Ness' PK Fire */
    NULL,                   /* Link's Bomb */

    /* Stage items -- reached by gr/'s own files, two of which call in */
    itPowerBlockMakeItem,   /* Mushroom Kingdom POW Block */
    itGBumperMakeItem,      /* Common Stage Bumper */
    itPakkunMakeItem,       /* Mushroom Kingdom Piranha Plant */
    itTargetMakeItem,       /* Bonus Stage Target */
    itTaruBombMakeItem,     /* Race to the Finish Bomb */
    itGLuckyMakeItem,       /* Saffron City Chansey */
    itMarumineMakeItem,     /* Saffron City Electrode */
    itHitokageMakeItem,     /* Saffron City Charmander */
    itFushigibanaMakeItem,  /* Saffron City Venusaur */
    itPorygonMakeItem,      /* Saffron City Porygon */

    /* Poké Ball monsters -- reached by itMainMakeMonster */
    itIwarkMakeItem,        /* Onix */
    itKabigonMakeItem,      /* Snorlax */
    itTosakintoMakeItem,    /* Goldeen */
    itNyarsMakeItem,        /* Meowth */
    itLizardonMakeItem,     /* Charizard */
    itSpearMakeItem,        /* Beedrill */
    itKamexMakeItem,        /* Blastoise */
    itMLuckyMakeItem,       /* Chansey */
    itStarmieMakeItem,      /* Starmie */
    itSawamuraMakeItem,     /* Hitmonlee */
    itDogasMakeItem,        /* Koffing */
    itPippiMakeItem,        /* Clefairy */
    itMewMakeItem           /* Mew */
};

/* 0x8018D040 - itManagerInitItems fills this in and it is the only
* writer. It is region 0 of the item pack, which is what
 * every item's `lbRelocGetFileData(ITAttributes*, gITManagerCommonData,
 * off)` resolves against -- see src/dc/itempack.h. NULL until that
 * function has run, and NULL again after itemPackRelease.
 *
 * dITStarItemDesc.p_file (src/dc/itstar.c) takes this global's ADDRESS at
 * static-init time, which needs no live value, only the symbol to exist. */
void *gITManagerCommonData;

/* it/item.h:33's debug hook, defined here beside the manager's other
 * globals. The decomp writes it only from a debugger -- nothing in the
 * game's own code assigns it -- so zero is what a battle runs with, and
 * that is the arm that calls itMainMakeMonster. itMBallOpenProcUpdate
 * and its air twin are the only readers. */
s32 dITManagerForceMonsterKind;

/* 0x8018D048 */
ITRandomWeights gITManagerRandomWeights;

/* itManagerMakeAppearActor (below) fills this in; itManagerSetItemSpawnWait only ever touches its own
 * spawn_wait field, which needs no real starting value from the rest of
 * the struct to do that. Zero-initialised the same "real field, not yet
 * given its real starting value" shape gITManagerDisplayMode below has. */
ITAppearActor gITManagerAppearActor;

/* 0x8018D060 */
ITMonsterData gITManagerMonsterData;

/* 0x8018D090 - itManagerInitItems seeds this to nDBDisplayModeMaster;
 * before that function runs it is zero. Read by
 * src/dc/itmain.c's itMainResetPlayerVars. */
s32 gITManagerDisplayMode;

/* 0x8018D08C - itManagerInitItems names this from efParticleGetLoadBankID,
 * which on the host build is the `#ifdef FT_HOSTTEST` arm
 * that names slot 0 instead -- see the function. Read by
 * src/dc/itfflower.c's weapon-side particle calls. */
s32 gITManagerParticleBankID;

/* 0x8018D094 - points to next available item struct */
ITStruct *gITManagerStructsAllocFree;

/* // // // // // // // // // // // //
 *                                   //
 *           FUNCTIONS               //
 *                                   //
 * // // // // // // // // // // // // */

/* it/itmanager.c:133-160 itManagerInitItems 0x8016DEA0.
 *
 * The item pool, and everything a scene needs resident before it can
 * spawn anything. Three DIVERGES, all in the middle block:
 *
 *   - the decomp's gITManagerCommonData is one lbRelocGetExternHeapFile
 *     (llITCommonDataFileID, relocData file 0xFB) into a heap block it
 *     allocates itself. The port's is itemPackLoad, which reads the pack
 *     tools/export/ssb_itemexport.py writes and publishes region 0 of it at the
 *     same global -- and that is what makes every item's
 *     `lbRelocGetFileData(ITAttributes*, gITManagerCommonData, off)`
 *     resolve the way the game's does. src/dc/itempack.h is where the
 *     four pointer fields' width problem is dealt with, which is the
 *     reason the pack exists at all.
 *   - the pack can be missing, and the decomp's file load cannot (it is
 *     a link-time constant). So this returns early when the load fails,
 *     leaving the pool allocated and everything else untouched: a scene
 *     that cannot spawn items still boots, where a NULL
 *     gITManagerCommonData dereferenced by the first spawn would not.
 *     The loader has already named the file it could not find.
 *   - the decomp registers the particle bank's RANGE here and nothing
 *     else; the port has the bank itself to read, so it loads it too --
 *     the same pair efDisplayInitAll makes for efcommon
 *     (src/dc/efdisplay.c:149-158), and without it the first item
 *     ScriptID has no textures on the disc. The registration itself is
 *     `#ifdef FT_HOSTTEST`ed away for the reason efManagerLoadEffectBank
 *     gives: the walk cannot run at this machine's pointer width.
 *
 * Everything else is the decomp's line for line, including the
 * `if (ip != NULL)` guard around the last link, which is the only thing
 * the game does about a failed syTaskmanMalloc.
 *
 * Binding it to a scene stays the caller's job, as the
 * decomp's is: sc1pgame.c, the four mv/ openings and the bonus stage
 * each call it once, on the way in. */
void itManagerInitItems(void)
{
    ITStruct *ip;
    s32 i;

    gITManagerStructsAllocFree = ip = syTaskmanMalloc(sizeof(ITStruct) * ITEM_ALLOC_MAX, 0x8);

    for (i = 0; i < (ITEM_ALLOC_MAX - 1); i++)
    {
        ip[i].next = &ip[i + 1];
    }
    if (ip != NULL)
    {
        ip[i].next = NULL;
    }
    if (itemPackLoad("itcommon.itp") != 0)
    {
        return;
    }
#ifdef FT_HOSTTEST
    /* A bank's descriptor is a ROM image of 32-bit structures -- `T *[]`
     * over what the file holds as a `u32[]` of offsets into itself -- and
     * lbParticleSetupBankID walks it pointerizing in place, at this
     * machine's pointer width. On x86-64 that is twice the stride and off
     * the end of the block, so no bank can be loaded in this build at all
     * (src/dc/efmanager.c efManagerLoadEffectBank has the long version).
     * The host suite builds one by hand instead (hosttest_ft.c
     * host_load_particle_bank) and this names slot 0, the same slot it
     * uses for Hyrule's, Yoster's and Pupupu's. Zero rather than -1 for
     * the reason src/dc/ftmanager.c gives: a zero id at least names a
     * bank that exists there. */
    gITManagerParticleBankID = 0;
#else
    gITManagerParticleBankID = efParticleGetLoadBankID
    (
        (uintptr_t)&lITManagerParticleScriptBankLo,
        (uintptr_t)&lITManagerParticleScriptBankHi,
        (uintptr_t)&lITManagerParticleTextureBankLo,
        (uintptr_t)&lITManagerParticleTextureBankHi
    );
#endif

    lbpTexLoadBank(gITManagerParticleBankID, "itcommon");

    itManagerSetupContainerDrops();
    itManagerInitMonsterVars();
    ifCommonItemArrowSetAttr();

    gITManagerDisplayMode = nDBDisplayModeMaster;
}

/* it/itmanager.c:165-179 itManagerGetNextStructAlloc 0x8016DFAC, verbatim. */
ITStruct* itManagerGetNextStructAlloc(void)
{
    ITStruct *new_item = gITManagerStructsAllocFree;
    ITStruct *get_item;

    if (new_item == NULL)
    {
        return NULL;
    }
    get_item = new_item;

    gITManagerStructsAllocFree = new_item->next;

    return get_item;
}

/* it/itmanager.c:182-187 itManagerSetPrevStructAlloc 0x8016DFDC, verbatim. */
void itManagerSetPrevStructAlloc(ITStruct *ip)
{
    ip->next = gITManagerStructsAllocFree;

    gITManagerStructsAllocFree = ip;
}

/* it/itmanager.c:480-483 itManagerGetCurrentAlloc 0x8016EB00, verbatim. */
ITStruct* itManagerGetCurrentAlloc(void)
{
    return gITManagerStructsAllocFree;
}

/* it/itmanager.c:632-707 itManagerSetupContainerDrops 0x8016EF40, verbatim.
 *
 * Its whole body is gated on gMPCollisionGroundData->item_weights, and
 * that pointer comes from the stage pack via src/dc/stage.c's ground
 * stand-in. With no row this takes the final `else`, weights_sum is zero,
 * and itMainGetWeightedItemKind samples against exactly that -- no item could
 * ever have spawned, whatever the roster looked like. See the note above
 * sGroundData for the whole story. */
void itManagerSetupContainerDrops(void)
{
    s32 item_tenth_round;
    s32 item_tenth_floor;
    s32 item_any_weights;   /* Sum of all toggled item weights of ANY value */
    u32 item_any_toggles;
    u32 item_valid_toggles;
    s32 item_valid_weights; /* Sum of all toggled NON-ZERO item weights */
    s32 weights_sum;
    MPItemWeights *p_any_weights;
    MPItemWeights *p_valid_weights;
    s32 i;

    if ((gSCManagerBattleState->item_appearance_rate != nSCBattleItemSwitchNone) && (gSCManagerBattleState->item_toggles != 0) && (gMPCollisionGroundData->item_weights != NULL))
    {
        item_any_toggles = gSCManagerBattleState->item_toggles >> nITKindUtilityStart;
        p_any_weights = gMPCollisionGroundData->item_weights;

        item_any_weights = 0;

        for (i = nITKindUtilityStart; i <= nITKindUtilityEnd; i++, item_any_toggles >>= 1)
        {
            if (item_any_toggles & 1)
            {
                item_any_weights += p_any_weights->values[i];
            }
        }
        gITManagerRandomWeights.weights_sum = item_any_weights;

        if (item_any_weights != 0)
        {
            item_valid_toggles = gSCManagerBattleState->item_toggles >> nITKindUtilityStart;
            p_valid_weights = gMPCollisionGroundData->item_weights;

            for (item_valid_weights = 0, i = nITKindUtilityStart; i <= nITKindUtilityEnd; i++, item_valid_toggles >>= 1)
            {
                if ((item_valid_toggles & 1) && (p_valid_weights->values[i] != 0))
                {
                    item_valid_weights++;
                }
            }
            gITManagerRandomWeights.valids_num = ++item_valid_weights;
            gITManagerRandomWeights.kinds = (u8*) syTaskmanMalloc(item_valid_weights * sizeof(*gITManagerRandomWeights.kinds), 0x0);
            gITManagerRandomWeights.blocks = (u16*) syTaskmanMalloc(item_valid_weights * sizeof(*gITManagerRandomWeights.blocks), 0x2);

            item_valid_toggles = gSCManagerBattleState->item_toggles >> nITKindUtilityStart;
            weights_sum = 0;

            for (item_valid_weights = 0, i = nITKindUtilityStart; i <= nITKindUtilityEnd; i++, item_valid_toggles >>= 1)
            {
                if ((item_valid_toggles & 1) && (p_valid_weights->values[i] != 0))
                {
                    gITManagerRandomWeights.kinds[item_valid_weights] = i;
                    gITManagerRandomWeights.blocks[item_valid_weights] = weights_sum;

                    weights_sum += p_valid_weights->values[i];
                    item_valid_weights++;
                }
            }
            gITManagerRandomWeights.kinds[item_valid_weights] = nITKindMBallMonsterStart;
            gITManagerRandomWeights.blocks[item_valid_weights] = weights_sum;

            item_tenth_round = (gITManagerRandomWeights.weights_sum * 0.1F);

            if (item_tenth_round != 0)
            {
                item_tenth_floor = item_tenth_round;
            }
            else item_tenth_floor = 1;

            gITManagerRandomWeights.weights_sum += item_tenth_floor;
        }
    }
    else gITManagerRandomWeights.weights_sum = 0;

#ifndef FT_HOSTTEST
    /* The port's one line, not the decomp's -- the same trade
     * src/dc/grjungle.c's "tarucann:" line makes, for the same reason:
     * this function's output is invisible until an item actually spawns,
     * and whether it produced anything at all is the one thing a serial
     * log can settle. `weights_sum` is what itMainGetWeightedItemKind
     * samples against, so a zero here means no item can spawn; the
     * pointer's own state says whether the STAGE handed its row over.
     * Disc probe: `--serial`, grep
     * "itweights:". */
    /* the toggles are a u32 bitmask and print as one: `%d` on it gave
     * "-1 kind(s) toggled on" */
    syDebugPrintf("itweights: %s, sum %d, toggles 0x%X\n",
                  (gMPCollisionGroundData->item_weights != NULL)
                      ? "stage row" : "NO STAGE ROW",
                  (int)gITManagerRandomWeights.weights_sum,
                  (unsigned)gSCManagerBattleState->item_toggles);
#endif
}

/* it/itmanager.c:709-714 itManagerInitMonsterVars 0x8016F218, verbatim. */
void itManagerInitMonsterVars(void)
{
    gITManagerMonsterData.monster_curr = gITManagerMonsterData.monster_prev = U8_MAX;
    gITManagerMonsterData.monsters_num = (nITKindMBallMonsterEnd - nITKindMBallMonsterStart);
}

/* it/itmanager.c:717-720 itManagerMakeItemKind 0x8016F260, verbatim: the
 * table dispatch every per-kind spawn goes through. */
GObj* itManagerMakeItemKind(GObj *parent_gobj, s32 kind, Vec3f *pos, Vec3f *vel, u32 flags)
{
    return dITManagerProcMakeList[kind](parent_gobj, pos, vel, flags);
}

/* it/itmanager.c:722-733 itManagerMakeItemSetupCommon 0x8016F280, verbatim.
 * The keystone the Containers, the appearance actor and the stage hazards
 * all come through -- and the reason dITManagerProcMakeList's entries
 * cannot be NULL: line 1 calls one unguarded.
 *
 * The swirl is `index <= nITKindCommonEnd`, i.e. the same 0..19 the
 * switch selects: a common item announces itself with
 * efManagerItemSpawnSwirlMakeEffect and does NOT appear spinning, where a
 * stage hazard or a Pokémon arrives without the swirl and spinning. */
GObj* itManagerMakeItemSetupCommon(GObj *parent_gobj, s32 index, Vec3f *pos, Vec3f *vel, u32 spawn_flags)
{
    GObj *item_gobj = dITManagerProcMakeList[index](parent_gobj, pos, vel, spawn_flags);

    if (item_gobj != NULL)
    {
        if (index <= nITKindCommonEnd)
        {
            efManagerItemSpawnSwirlMakeEffect(pos);
            itMainSetAppearSpin(item_gobj, FALSE);
        }
    }
    return item_gobj;
}

/* it/itmanager.c:190-226 itManagerSetupItemDObjs 0x8016E0AC, verbatim:
 * builds an item's custom DObj tree straight off a DObjDesc array (the
 * `attr->is_item_dobjs` arm of itManagerMakeItem below), one child per
 * entry threaded by the previous entry's own DObj. */
void itManagerSetupItemDObjs(GObj *gobj, DObjDesc *dobjdesc, DObj **dobjs, u8 transform_kind)
{
    s32 i, id;
    DObj *dobj, *array_dobjs[DOBJ_ARRAY_MAX];

    for (i = 0; i < ARRAY_COUNT(array_dobjs); i++)
    {
        array_dobjs[i] = NULL;
    }
    for (i = 0; dobjdesc->id != ARRAY_COUNT(array_dobjs); i++, dobjdesc++)
    {
        id = dobjdesc->id & 0xFFF;

        if (id != 0)
        {
            dobj = array_dobjs[id] = gcAddChildForDObj(array_dobjs[id - 1], dobjdesc->dl);
        }
        else dobj = array_dobjs[0] = gcAddDObjForGObj(gobj, dobjdesc->dl);

        if (i == 1)
        {
            gcDecideDObj3TransformsKind(dobj, transform_kind, nGCMatrixKindNull, nGCMatrixKindNull, nGCMatrixKindNull);
        }
        else if (transform_kind != nGCMatrixKindNull)
        {
            gcAddXObjForDObjFixed(dobj, transform_kind, nGCMatrixKindNull);
        }
        dobj->translate.vec.f = dobjdesc->translate;
        dobj->rotate.vec.f = dobjdesc->rotate;
        dobj->scale.vec.f = dobjdesc->scale;

        if (dobjs != NULL)
        {
            dobjs[i] = dobj;
        }
    }
}

/* it/itmanager.c:229-461 itManagerMakeItem 0x8016E174, verbatim: the
 * keystone every per-item MakeItem wrapper (it/itcommon/, still ahead)
 * calls, given an ITDesc naming its own attributes file and offset.
 * Builds the item's GObj, reads its ITAttributes, sets its proc_display
 * from the display-mode/XLU bits (itdisplay.c), resets its
 * ITStruct fields, builds its DObj tree (custom-MObj or item-DObjDesc,
 * per `is_item_dobjs`), wires its three per-frame processes (itprocess.c) and its ITDesc-supplied proc_* pointers, and -- for an item
 * spawned already touching its parent's collision line -- links it into
 * that collision the way mpCommonRunItemCollisionDefault already lets a
 * weapon do. */
GObj* itManagerMakeItem(GObj *parent_gobj, ITDesc *item_desc, Vec3f *pos, Vec3f *vel, u32 flags)
{
    ITStruct *ip = itManagerGetNextStructAlloc();
    GObj *item_gobj;
    ITAttributes *attr;
    void (*proc_display)(GObj*);
    s32 unused[4];

    if (ip == NULL)
    {
        return NULL;
    }
    item_gobj = gcMakeGObjSPAfter(nGCCommonKindItem, NULL, nGCCommonLinkIDItem, GOBJ_PRIORITY_DEFAULT);

    if (item_gobj == NULL)
    {
        itManagerSetPrevStructAlloc(ip);

        return NULL;
    }
    /* DIVERGES: the decomp is
     *     attr = lbRelocGetFileData(ITAttributes*, *item_desc->p_file,
     *                               item_desc->o_attributes);
     * -- `file + offset`, where the offset is a real one into ITCommonData
     * but the bytes there are the N64's. `ITAttributes`
     * opens with four POINTERS -- 16 bytes of them on the N64 and 32 here
     * -- so reading the raw bytes as this struct puts every field below
     * sixteen bytes out on a 64-bit host. itemPackAttr answers for the
     * item pack's own region 0 out of the 34 tables it ships PARSED
     * (src/dc/itempack.h); a caller handing MakeItem a file of its own
     * gets NULL and keeps the decomp's arithmetic, which is what the item
     * host tests did before the pack existed. */
    attr = itemPackAttr(*item_desc->p_file, item_desc->o_attributes);

    if (attr == NULL)
    {
        attr = lbRelocGetFileData(ITAttributes*, *item_desc->p_file, item_desc->o_attributes);
    }

    if (attr->is_display_colanim)
    {
        proc_display = (attr->is_display_xlu) ? itDisplayColAnimXLUProcDisplay : itDisplayColAnimOPAProcDisplay;
    }
    else proc_display = (attr->is_display_xlu) ? itDisplayXLUProcDisplay : itDisplayOPAProcDisplay;

    gcAddGObjDisplay(item_gobj, proc_display, 11, GOBJ_PRIORITY_DEFAULT, ~0);

    item_gobj->user_data.p = ip;

    ip->item_gobj = item_gobj;
    ip->owner_gobj = NULL;

    ip->kind = item_desc->kind;
    ip->type = attr->type;

    ip->physics.vel_air = *vel;
    ip->physics.vel_ground = 0.0F;

    ip->attr = attr;

    itMainSetSpinVelLR(item_gobj);
    itMainResetPlayerVars(item_gobj);

    ip->is_allow_pickup     = FALSE;
    ip->is_hold             = FALSE;
    ip->is_allow_knockback  = FALSE;
    ip->is_unused_item_bool = FALSE;
    ip->is_static_damage    = FALSE;

    ip->pickup_wait         = ITEM_PICKUP_WAIT_DEFAULT;

    ip->percent_damage      = 0;
    ip->hitlag_tics         = 0;
    ip->damage_highest      = 0;
    ip->damage_knockback    = 0.0F;
    ip->damage_queue        = 0;
    ip->damage_lag          = 0;

    ip->times_landed        = 0;
    ip->times_thrown        = 0;

    ip->weight              = attr->weight;
    ip->is_hitlag_victim    = attr->is_give_hitlag;
    ip->drop_sfx            = attr->drop_sfx;
    ip->throw_sfx           = attr->throw_sfx;
    ip->smash_sfx           = attr->smash_sfx;

    ip->vel_scale           = F_PCT_TO_DEC(attr->vel_scale);

    ip->is_damage_all       = FALSE;
    ip->is_thrown           = FALSE; /* Applies magnitude and stale multiplier if TRUE and hitbox is active? */
    ip->is_attach_surface   = FALSE;

    ip->spin_step         = 0.0F;

    ip->arrow_gobj          = NULL;
    ip->arrow_timer         = 0;

    ip->attack_coll.attack_state     = item_desc->attack_state;
    ip->attack_coll.damage           = attr->damage;
    ip->attack_coll.throw_mul        = 1.0F;
    ip->attack_coll.stale            = 1.0F;
    ip->attack_coll.element          = attr->element;
    ip->attack_coll.offsets[0].x     = attr->attack_offset0_x;
    ip->attack_coll.offsets[0].y     = attr->attack_offset0_y;
    ip->attack_coll.offsets[0].z     = attr->attack_offset0_z;
    ip->attack_coll.offsets[1].x     = attr->attack_offset1_x;
    ip->attack_coll.offsets[1].y     = attr->attack_offset1_y;
    ip->attack_coll.offsets[1].z     = attr->attack_offset1_z;
    ip->attack_coll.size             = attr->size * 0.5F;
    ip->attack_coll.angle            = attr->angle;
    ip->attack_coll.knockback_scale  = attr->knockback_scale;
    ip->attack_coll.knockback_weight = attr->knockback_weight;
    ip->attack_coll.knockback_base   = attr->knockback_base;
    ip->attack_coll.can_setoff       = attr->can_setoff;
    ip->attack_coll.shield_damage    = attr->shield_damage;
    ip->attack_coll.fgm_id           = attr->hit_sfx;
    ip->attack_coll.priority         = attr->priority;
    ip->attack_coll.can_rehit_item   = attr->can_rehit_item;
    ip->attack_coll.can_rehit_fighter= attr->can_rehit_fighter;
    ip->attack_coll.can_rehit_shield = FALSE;
    ip->attack_coll.can_hop          = attr->can_hop;
    ip->attack_coll.can_reflect      = attr->can_reflect;
    ip->attack_coll.can_shield       = attr->can_shield;
    ip->attack_coll.attack_count     = attr->attack_count;
    ip->attack_coll.interact_mask    = GMHITCOLLISION_FLAG_ALL;

    ip->attack_coll.motion_attack_id           = nFTMotionAttackIDNone;
    ip->attack_coll.motion_count               = ftParamGetMotionCount();
    ip->attack_coll.stat_flags.attack_id       = nFTStatusAttackIDNull;
    ip->attack_coll.stat_flags.is_smash_attack = ip->attack_coll.stat_flags.ga = ip->attack_coll.stat_flags.is_projectile = 0;
    ip->attack_coll.stat_count                 = ftParamGetStatUpdateCount();

    itMainClearAttackRecord(ip);

    ip->damage_coll.hitstatus     = attr->hitstatus;
    ip->damage_coll.offset.x      = attr->damage_coll_offset.x;
    ip->damage_coll.offset.y      = attr->damage_coll_offset.y;
    ip->damage_coll.offset.z      = attr->damage_coll_offset.z;
    ip->damage_coll.size.x        = attr->damage_coll_size.x * 0.5F;
    ip->damage_coll.size.y        = attr->damage_coll_size.y * 0.5F;
    ip->damage_coll.size.z        = attr->damage_coll_size.z * 0.5F;
    ip->damage_coll.interact_mask = GMHITCOLLISION_FLAG_ALL;

    ip->shield_collide_angle = 0.0F;
    ip->shield_collide_dir.x = 0.0F;
    ip->shield_collide_dir.y = 0.0F;
    ip->shield_collide_dir.z = 0.0F;

    ip->hit_normal_damage  = 0;
    ip->hit_refresh_damage = 0;
    ip->hit_attack_damage  = 0;
    ip->hit_shield_damage  = 0;

    ip->reflect_gobj = NULL;

    if (attr->data != NULL)
    {
        /* DIVERGES: the decomp walks `attr->data` as an N64 DObjDesc array
         * -- gcSetupCustomDObjsWithMObj on the common arm,
         * itManagerSetupItemDObjs on the is_item_dobjs one -- handing each
         * entry's own `dl` to gcAddDObjForGObj. The port draws nothing
         * from a display list: gcSubmitDObj (src/dc/objdisplay.c) submits
         * `dobj->dv`, the baked model, and gcDrawDObjForGObj skips a DObj
         * whose `dv` is NULL. So the tree comes from the pack
         * tools/export/ssb_itemmodelexport.py baked, keyed by the same
         * `o_attributes` this function found its attributes by.
         *
         * The two arms differ only in how the game threads MObjs onto the
         * tree, and the bake carries the MObj chain with it (13 of the 34
         * items have one), so one call covers both.
         *
         * -1 is an item the pack has no model for -- Heart and Sword bake
         * to nothing -- and falls through to the same bare GObj the decomp
         * gives an item whose `data` is NULL. Everything after this point
         * is unchanged, including the eject below: the game's DObjDesc[0]
         * is a throwaway root with no display list on every item (the
         * bake says the same -- joint 0 has no batches), so the tree the
         * anim scripts address is the one AFTER the eject, and the port
         * must eject too or its joints are one level off. */
        AObjEvent32 **anim_joints = attr->anim_joints;

        if (itemModelAddToGObj(item_gobj, item_desc->o_attributes) < 0)
        {
            gcAddDObjForGObj(item_gobj, NULL);
        }
        /* DIVERGES: a table whose `anim_joints` points into a file the
         * item pack does not carry -- PK Fire's pillar's, in NessSpecial3
         * -- gets the one its model pack carries, over the same joints in
         * the same order, before the eject as the game's does. NULL for
         * every other item (itemmodel.h). */
        if (anim_joints == NULL)
        {
            anim_joints = itemModelAnimJoints(item_desc->o_attributes);
        }
        /* DIVERGES: gcAddAnimJointAll (the AnimJoint/bone half only) in
         * place of gcAddAnimAll. attr->p_matanim_joints names the item's
         * MatAnimJoint table in ITCommonData/ITCommonObject -- real N64
         * bytes, correctly relocated as a POINTER (item_pack_setup_attr),
         * but gcAddMatAnimJointAll walks it one level further, into an
         * AObjEvent32** array whose OWN entries this port's model-region
         * fixup table (tools/export/ssb_itemexport.py's `ifixes`) carries no
         * targets for -- handing it to gcAddMObjMatAnimJoint would store
         * an unrelocated N64 address as the MObj's script and the port's
         * own interpreter would step off of it. Star's and
         * Fire Flower's scripts (both step palette_id) are baked into
         * their own .mdl pack instead, tools/export/ssb_itemmodelexport.py's
         * `scripts`/dpal path, mirroring a weapon's own MatAnimJoint
         * (wp/wpmanager.c). Played by itemModelAddToGObj's
         * dc_model_add_mobjs (itemmodel.c), which already ran before this
         * eject -- nothing further to call here for the material half. */
        if (anim_joints != NULL)
        {
            gcAddAnimJointAll(item_gobj, anim_joints, 0.0F);
            gcPlayAnimAll(item_gobj);
        }
        lbCommonEjectTreeDObj(DObjGetStruct(item_gobj));
    }
    else gcAddDObjForGObj(item_gobj, NULL);

    ip->coll_data.p_translate       = &DObjGetStruct(item_gobj)->translate.vec.f;
    ip->coll_data.p_lr              = &ip->lr;
    ip->coll_data.map_coll.top      = attr->map_coll_top;
    ip->coll_data.map_coll.center   = attr->map_coll_center;
    ip->coll_data.map_coll.bottom   = attr->map_coll_bottom;
    ip->coll_data.map_coll.width    = attr->map_coll_width;
    ip->coll_data.p_map_coll        = &ip->coll_data.map_coll;
    ip->coll_data.ignore_line_id    = -1;
    ip->coll_data.update_tic   = gMPCollisionUpdateTic;
    ip->coll_data.mask_curr    = 0;
    ip->coll_data.vel_push.x        = 0.0F;
    ip->coll_data.vel_push.y        = 0.0F;
    ip->coll_data.vel_push.z        = 0.0F;

    gcAddGObjProcess(item_gobj, itProcessProcItemMain, nGCProcessKindFunc, 3);
    gcAddGObjProcess(item_gobj, itProcessProcSearchHitAll, nGCProcessKindFunc, 1);
    gcAddGObjProcess(item_gobj, itProcessProcHitCollisions, nGCProcessKindFunc, 0);

    ip->proc_update    = item_desc->proc_update;
    ip->proc_map       = item_desc->proc_map;
    ip->proc_hit       = item_desc->proc_hit;
    ip->proc_shield    = item_desc->proc_shield;
    ip->proc_hop       = item_desc->proc_hop;
    ip->proc_setoff    = item_desc->proc_setoff;
    ip->proc_reflector = item_desc->proc_reflector;
    ip->proc_damage    = item_desc->proc_damage;
    ip->proc_dead      = NULL;

    ip->coll_data.pos_prev = DObjGetStruct(item_gobj)->translate.vec.f = *pos;

    if (flags & ITEM_FLAG_COLLPROJECT)
    {
        switch (flags & ITEM_MASK_PARENT)
        {
        case ITEM_FLAG_PARENT_GROUND:
        case ITEM_FLAG_PARENT_DEFAULT: /* Default? */
            break;

        case ITEM_FLAG_PARENT_FIGHTER:
            mpCommonRunItemCollisionDefault(item_gobj, ftGetStruct(parent_gobj)->coll_data.p_translate, &ftGetStruct(parent_gobj)->coll_data);
            break;

        case ITEM_FLAG_PARENT_WEAPON:
            mpCommonRunItemCollisionDefault(item_gobj, wpGetStruct(parent_gobj)->coll_data.p_translate, &wpGetStruct(parent_gobj)->coll_data);
            break;

        case ITEM_FLAG_PARENT_ITEM:
            mpCommonRunItemCollisionDefault(item_gobj, itGetStruct(parent_gobj)->coll_data.p_translate, &itGetStruct(parent_gobj)->coll_data);
            break;

        default:
            break;
        }
    }
    ip->ga = nMPKineticsAir;

    itProcessUpdateAttackPositions(item_gobj);
    itMainClearColAnim(item_gobj);

    return item_gobj;
}

/* it/itmanager.c:486-494 itManagerSetItemSpawnWait 0x8016EB0C, verbatim.
 * Touches only ITAppearActor.spawn_wait and the two appearance-rate
 * tables above -- no roster needed, unlike its caller itManagerAppear-
 * ActorProcUpdate (still blocked, see the file header). */
void itManagerSetItemSpawnWait(void)
{
    gITManagerAppearActor.spawn_wait =
    dITManagerAppearanceRatesMin[gSCManagerBattleState->item_appearance_rate] +
    syUtilsRandIntRange
    (
        dITManagerAppearanceRatesMax[gSCManagerBattleState->item_appearance_rate] - dITManagerAppearanceRatesMin[gSCManagerBattleState->item_appearance_rate]
    );
}

/* it/itmanager.c:497-522 itManagerAppearActorProcUpdate 0x8016EB78,
 * verbatim. THE SPAWNER -- the only thing in the game that
 * puts an item on the field with neither a Container nor a stage asking
 * for one, and therefore the function the item switch's sixteen toggles
 * actually control.
 *
 * Its four gates are each someone else's state: the battle must not be in
 * `nSCBattleGameStatusWait` (the pre-match hold), the appearance timer must
 * have run out, and the ITStruct POOL must have a free slot
 * (`itManagerGetCurrentAlloc`). Only then does it roll a kind out of the
 * weights this file's maker built, take the position of one of the map
 * objects the stage tagged for items, and hand both to
 * `itManagerMakeItemSetupCommon` -- the same thirteen-line keystone a
 * Container's drop and Clefairy's Eggs go through.
 *
 * Note the two different `spawn_wait` paths: a spawn that HAPPENS resets
 * the timer from the appearance-rate tables, and one that does not (the
 * pool is full) also resets it -- so a full pool costs a full interval
 * rather than retrying every frame. */
void itManagerAppearActorProcUpdate(GObj *item_gobj)
{
    s32 unused;
    s32 kind;
    Vec3f pos;
    Vec3f vel;

    if (gSCManagerBattleState->game_status != nSCBattleGameStatusWait)
    {
        if (gITManagerAppearActor.spawn_wait > 0)
        {
            gITManagerAppearActor.spawn_wait--;

            return;
        }
        if (itManagerGetCurrentAlloc() != NULL)
        {
            kind = itMainGetWeightedItemKind(&gITManagerAppearActor.weights);

            mpCollisionGetMapObjPositionID(gITManagerAppearActor.mapobjs[syUtilsRandIntRange(gITManagerAppearActor.mapobjs_num)], &pos);

            vel.x = vel.y = vel.z = 0.0F;

            func_800269C0_275C0(nSYAudioFGMItemSpawn1);

            /* The port's own line, not the decomp's -- the same trade
             * `itweights:` above and grjungle.c's `tarucann:` make. This
             * function's output is INVISIBLE: an item on
             * the field is a picture, and whether the spawner ran at all
             * is all a serial log can settle. It prints the kind, the
             * position and the valids_num it sampled against, so a run
             * with the switch on shows the spawner working and one with
             * it off shows nothing at all. */
            syDebugPrintf("itspawn: kind %d at (%.1f, %.1f), %d kind(s) in "
                          "the pool\n", kind, pos.x, pos.y,
                          gITManagerAppearActor.weights.valids_num);

            itManagerMakeItemSetupCommon(NULL, kind, &pos, &vel, ITEM_FLAG_PARENT_DEFAULT);
        }
        itManagerSetItemSpawnWait();
    }
}

/* it/itmanager.c:528-612 itManagerMakeAppearActor 0x8016EC40, verbatim.
 * It builds the spawner: four gates, then three tables.
 *
 *  - `item_appearance_rate != nSCBattleItemSwitchNone` and
 *    `item_toggles != 0` are the switch itself -- the two settings the VS
 *    options screen writes. With either off, no spawner is made at all.
 *  - `gMPCollisionGroundData->item_weights != NULL` is the stage's own
 *    twenty-row weight table.
 *  - The first loop sums the toggled weights to reject a stage whose every
 *    toggled-on row is zero; the last two build `gITManagerAppearActor.weights`
 *    as the (kinds, blocks) pair `itMainGetWeightedItemKind` samples -- the
 *    same shape a Container's drop table has.
 *  - The map objects are the stage's `nMPMapObjKindItem` positions, read
 *    into a malloc'd `u8` array. The over-limit arm spins on
 *    syDebugPrintf + scManagerRunPrintGObjStatus forever, which is the
 *    decomp's own hard stop and is kept: a stage that tags more than 30
 *    item positions is a data bug, not a runtime one.
 *
 * It returns the GObj so the scene can eject it, and it is THAT GObj the
 * proc above runs on. Its caller is scvsbattle.c, which is why the port's
 * battle now has items appearing on their own. */
GObj* itManagerMakeAppearActor(void)
{
    GObj *gobj;
    s32 i;
    s32 item_any_weights;   /* Sum of all toggled item weights of ANY value */
    MPItemWeights *p_any_weights;
    s32 weights_sum;
    s32 mapobjs_num;
    s32 item_mapobj_ids[30];
    s32 unused;
    s32 item_valid_weights; /* Sum of all toggled NON-ZERO item weights */
    u32 item_valid_toggles;
    MPItemWeights *p_valid_weights;
    u32 item_any_toggles;

    if (gSCManagerBattleState->item_appearance_rate != nSCBattleItemSwitchNone)
    {
        if (gSCManagerBattleState->item_toggles != 0)
        {
            if (gMPCollisionGroundData->item_weights != NULL)
            {
                p_any_weights = gMPCollisionGroundData->item_weights;
                item_any_toggles = gSCManagerBattleState->item_toggles;

                item_any_weights = 0;

                for (i = nITKindCommonStart; i <= nITKindCommonEnd; i++, item_any_toggles >>= 1)
                {
                    if (item_any_toggles & 1)
                    {
                        item_any_weights += p_any_weights->values[i];
                    }
                }
                if (item_any_weights == 0)
                {
                    return NULL;
                }
                gITManagerAppearActor.weights.weights_sum = item_any_weights;

                mapobjs_num = mpCollisionGetMapObjCountKind(nMPMapObjKindItem);

                if (mapobjs_num == 0)
                {
                    return NULL;
                }
                if (mapobjs_num > ARRAY_COUNT(item_mapobj_ids))
                {
                    while (TRUE)
                    {
                        syDebugPrintf("Item positions are over %d!\n", ARRAY_COUNT(item_mapobj_ids));
                        scManagerRunPrintGObjStatus();
                    }
                }
                gITManagerAppearActor.mapobjs_num = mapobjs_num;
                gITManagerAppearActor.mapobjs = (u8*) syTaskmanMalloc(mapobjs_num * sizeof(*gITManagerAppearActor.mapobjs), 0);

                mpCollisionGetMapObjIDsKind(nMPMapObjKindItem, item_mapobj_ids);

                for (i = 0; i < mapobjs_num; i++)
                {
                    gITManagerAppearActor.mapobjs[i] = item_mapobj_ids[i];
                }
                gobj = gcMakeGObjSPAfter(nGCCommonKindItem, NULL, nGCCommonLinkIDItemActor, GOBJ_PRIORITY_DEFAULT);

                gcAddGObjProcess(gobj, itManagerAppearActorProcUpdate, nGCProcessKindFunc, 3);

                item_valid_toggles = gSCManagerBattleState->item_toggles;
                p_valid_weights = gMPCollisionGroundData->item_weights;

                for (i = nITKindCommonStart, item_valid_weights = 0; i <= nITKindCommonEnd; i++, item_valid_toggles >>= 1)
                {
                    if ((item_valid_toggles & 1) && (p_valid_weights->values[i] != 0))
                    {
                        item_valid_weights++;
                    }
                }
                gITManagerAppearActor.weights.valids_num = item_valid_weights;
                gITManagerAppearActor.weights.kinds = (u8*) syTaskmanMalloc(item_valid_weights * sizeof(*gITManagerAppearActor.weights.kinds), 0x0);
                gITManagerAppearActor.weights.blocks = (u16*) syTaskmanMalloc(item_valid_weights * sizeof(*gITManagerAppearActor.weights.blocks), 0x2);

                item_valid_toggles = gSCManagerBattleState->item_toggles;
                weights_sum = 0;

                for (i = nITKindCommonStart, item_valid_weights = 0; i <= nITKindCommonEnd; i++, item_valid_toggles >>= 1)
                {
                    if ((item_valid_toggles & 1) && (p_valid_weights->values[i] != 0))
                    {
                        gITManagerAppearActor.weights.kinds[item_valid_weights] = i;
                        gITManagerAppearActor.weights.blocks[item_valid_weights] = weights_sum;
                        weights_sum += p_valid_weights->values[i];

                        item_valid_weights++;
                    }
                }
                itManagerSetItemSpawnWait();

                return gobj;
            }
        }
    }
    return NULL;
}
