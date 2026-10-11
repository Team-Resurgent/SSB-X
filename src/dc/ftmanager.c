/* ftmanager.c -- ft/ftmanager.c, the fighter manager: the FTStruct pool,
 * and making a fighter from an FTDesc the way scVSBattleStartBattle
 * asks for one. Function-for-function against the game's own FTStruct
 * (ftcommon.h); every function names its decomp line range, and what is
 * left out is marked DIVERGES where it happens.
 *
 * What the game's manager also does, and this does not (yet):
 *  - the ROM file bookkeeping (ftManagerSetupFileSize, the
 *    ftManagerSetupFiles* family, the per-kind figatree heap sizes):
 *    the port's fighters are packs loaded whole, one per kind, into
 *    gFTManagerModels by ftManagerSetupFilesAllKind -- per scene, as
 *    the game loads a kind's files -- and fp->attr is
 *    filled from the pack (ftManagerSetupAttributes) rather than read
 *    off a file.
 *  - on each joint's FTParts, the parts flags (fog/detail bits
 *    ftdisplaymain.c reads, which the pack does not carry) and the
 *    accessory GObj for costumes; the shield pose.
 *  - the passive_vars setup for Kirby, Link and the Boss, which reach
 *    into their own files; the computer player, the Key script, the
 *    Demo kind, and the shadow.
 * The FTParts pool is the game's (one per joint, on
 * user_data.p, where gm/gmcollision.c finds it), the anim locks are
 * set, the hit-collision reset at the end of ftManagerInitFighter runs,
 * and a fighter gets all six of its GObjProcesses.
 */
#include "loadcensus.h"
#include "ftcommon.h"
#include "ftshadow.h"
#include "ftdisplaymain.h"

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#include <malloc.h>              /* mallinfo, for the pack loader's
                                  * out-of-memory message */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpcommon.h"
#include "objmodel.h"
#include "sprite.h"
#include "taskman.h"
#include "overlay.h"
#include "lbpartex.h"            /* the per-fighter particle banks */

#include <ef/efparticle.h>

#include <sys/debug.h>
#include <sys/develop.h>         /* nDBDisplayModeMaster */
#include <reloc_data.h>          /* llKirbyMainMotionSpecialNFTKirbyCopy */
#include <sc/scsubsys/scsubsys.h>

/* ft/ftmanager.c:22-31 */
static FTStruct *sFTManagerStructsAllocFree;
static FTStruct *sFTManagerStructsAllocBuf;
static FTParts *sFTManagerPartsAllocFree;
static FTParts *sFTManagerPartsAllocBuf;

/* The port's counterpart of the FTParts pool (ft/ftmanager.c:28-31)
 * and the figatree heap size (:46): one Fighter instance per FTStruct
 * slot, and behind it a clip buffer of the pack's own vert_count. The
 * pool is cut before the scene knows which packs it will load -- the
 * character select cuts it and then loads all twelve, as the game does
 * -- so the instances are sized by the slot and the clip buffers are
 * not: each one is malloc'd against the pack the fighter turns out to
 * be (ftManagerMakeFighter) and freed with it (ftManagerDestroyFighter),
 * the same way fighter_load/fighter_free own a loaded pack's.
 *
 * DIVERGES: a fixed share of one .bss arena per slot, eight slots of
 * FTMANAGER_VCLIP_MAX (ftcommon.h), would be enough for the eight
 * fighters mvopeningrun.c and mvopeningclash.c hold at once, but 288 K
 * whether a scene used them or not. The 1P intro card asks for far more than
 * eight: sc1pintro.c:1828 sc1PIntroGetFighterAllocsNum wants 21 on the
 * Yoshi Team's rung, 13 on the Fighting Polygons' and 9 on Kirby's, and
 * sizing a fixed share for the worst of those (21 slots that each have
 * to fit Kirby, the one pack the player may bring, at 2107 vertices)
 * would be 708 K of .bss to draw 21 Yoshis of 548. A clip buffer is
 * main RAM, not the scene heap, and main RAM is
 * where a pack already lives, so asking for exactly what the instance
 * needs costs less than the arena did and has no ceiling to hit.
 *
 * The instances themselves are main RAM too, for the same
 * reason. A Fighter is 20816 bytes -- the pose, the
 * skeleton, the per-joint matrices and now the texgen vectors -- and
 * the 1P intro card asks for 21 of them, 437 K, where the scene heap is
 * 1280 K all told and the card's own sprites, particles and camera
 * scripts want most of what is left. The Yoshi Team's rung overflowed
 * it ("ml : alloc overflow #65536"). The FTStruct
 * and FTParts pools above stay where the game puts them, the scene
 * heap: they are the decomp's own structs at the decomp's own sizes
 * (3600 and 37 x 232 a slot), and 21 slots of those are 256 K, which
 * fits. This pool is the port's own and its size is the port's own
 * doing, so it is the port that carries it. */
static Fighter *sFTManagerModelsAllocBuf;
/* Whose buffer is whose, by slot, and in .bss rather than beside the
 * instance it belongs to: the instances go when the scene heap does and
 * ftManagerReleaseVClipAll is what frees them. The host suite
 * proved the difference -- syTaskmanSetupPools hands the mock a fresh
 * region each time (hosttest_ft.c load_stage), so reading the old
 * slots to find the pointers segfaulted where the target, whose heap is
 * one fixed block, would have got away with it. This array is valid
 * whatever the heap is doing. Twenty-one is the most any scene asks for:
 * sc1pintro.c:1828 sc1PIntroGetFighterAllocsNum on the Yoshi Team's
 * rung, and the ladder's card is the one place past eight. */
#define FTMANAGER_ALLOC_MAX 21
static float (*sFTManagerVClipOwned[FTMANAGER_ALLOC_MAX])[4];

/* The port's counterpart of the FTAttributes the game reads out of each
 * fighter's main file (ft/ftmanager.c:690): one per kind the scene has
 * loaded, cut from the scene heap when the kind's files are set up (or
 * at its first spawn, for a caller that installed a pack by hand) and
 * filled from the pack at every spawn. */
static FTAttributes *sFTManagerAttrs[nFTKindEnumCount];

/* The pack and sprite bank a kind loads, by name: ft/ftdata.c's
 * dFTManagerDataFiles gives every kind its file ids, and the port's
 * exporters name their outputs after the fighter. Every kind the game
 * has names one: the G (giant), M (metal), twelve N (polygon) and Boss kinds included.
 *
 * Master Hand is made of files of his own -- BossModel, BossMainMotion,
 * BossMain and 35 BossAnim* -- and is the smallest pack of all: 18
 * joints, 19 batches, 474 triangles and not one texture, because he is
 * flat-shaded white. Naming him here only makes the pack loadable; the
 * kind still has no status table and no motions, which the rest
 * of the 1P game builds on top of it.
 *
 * Neither of the first two is made of files of its own. Giant Donkey Kong
 * is every byte of Donkey Kong's geometry and all of his animations:
 * what makes him giant is dGDonkeyMain's FTAttributes -- size 2.0 to
 * Donkey's 1.25, and twenty more numbers with it -- and the port's
 * attributes come out of the pack (ftManagerSetupAttributes below).
 * dFTGDonkeyData names llDonkeyModelFileID and llDonkeyMainMotionFileID
 * and has no model or motion file at all. Metal Mario has a model file
 * (dMMarioModel, Mario's joints under one metal texture and no MObjSub
 * table) and a small MainMotion of his own, but 123 of his 168 scripted
 * motion rows carry FTANIM_FLAG_SUBMOTION_SCRIPT and read MARIO's
 * MainMotion instead; the pack carries both files and the exporter lays
 * each row onto the right one.
 *
 * The exporter reads whose files a kind is made of off ft/ftdata.c
 * rather than off its name (tools/lib/ssb_meshexport.py's motion_sources
 * and build_fighter's model lookup), so all four groups build from the
 * same two rules. */
static const char *const sFTManagerKindNames[nFTKindEnumCount] =
{
    [nFTKindMario] = "mario",     [nFTKindFox] = "fox",
    [nFTKindDonkey] = "donkey",   [nFTKindSamus] = "samus",
    [nFTKindLuigi] = "luigi",     [nFTKindLink] = "link",
    [nFTKindYoshi] = "yoshi",     [nFTKindCaptain] = "captain",
    [nFTKindKirby] = "kirby",     [nFTKindPikachu] = "pikachu",
    [nFTKindPurin] = "purin",     [nFTKindNess] = "ness",
    [nFTKindGDonkey] = "gdonkey", [nFTKindMMario] = "mmario",
    [nFTKindNMario] = "nmario",   [nFTKindNFox] = "nfox",
    [nFTKindNDonkey] = "ndonkey", [nFTKindNSamus] = "nsamus",
    [nFTKindNLuigi] = "nluigi",   [nFTKindNLink] = "nlink",
    [nFTKindNYoshi] = "nyoshi",   [nFTKindNCaptain] = "ncaptain",
    [nFTKindNKirby] = "nkirby",   [nFTKindNPikachu] = "npikachu",
    [nFTKindNPurin] = "npurin",   [nFTKindNNess] = "nness",
    [nFTKindBoss] = "boss",
};

/* ft/ftmanager.c:34-40 */
u32 gFTManagerPlayersNum;
u16 gFTManagerMotionCount;
u16 gFTManagerStatUpdateCount;

/* the port's dFTManagerDataFiles (ft/ftdata.c): see ftcommon.h */
Fighter *gFTManagerModels[nFTKindEnumCount];

/* The fighter's stock icon and emblem, which the game reads out of its
 * model file through FTAttributes.sprites (ft/fttypes.h:971; the main
 * file's FTSprites, e.g. relocData/203_MarioMain.c dMarioMain_sprites)
 * and the HUD draws beside the damage (if/ifcommon.c). The port's are
 * a sprite bank per kind, ft<name>.spr, cut from the model file by
 * tools/export/ssb_spriteexport.py --fighter with the stock icon once per
 * costume palette; ftManagerSetupFilesAllKind loads it into the scene
 * and fills the FTSprites the attributes point at. */
static SpriteBank sFTManagerSpriteBanks[nFTKindEnumCount];
static FTSprites sFTManagerSprites[nFTKindEnumCount];

/* tools/relocFileDescriptions.us.txt: every fighter's model file
 * catalogues its two sprites under these names, and the exporter keys
 * them by their offsets, which differ per file; the bank is asked by
 * name here instead, the one place the port does that. */
static DCSpriteEntry *ftManagerFindSprite(SpriteBank *bank, const char *name)
{
    u32 i;

    for (i = 0; i < bank->count; i++)
    {
        if (strcmp(bank->entries[i].name, name) == 0)
        {
            return &bank->entries[i];
        }
    }
    return NULL;
}

/* Whether ftManagerSetupFilesAllKind has a pack to load for a kind: the
 * twelve playables have one, 1P's kinds do not. What the scene manager
 * asks before it starts a battle. */
sb32 ftManagerKindHasPack(s32 fkind)
{
    return fkind >= 0 && fkind < nFTKindEnumCount &&
           sFTManagerKindNames[fkind] != NULL;
}

/* The port's FTAttributes for a kind, cut from the scene heap the first
 * time the scene asks -- ftManagerSetupFilesAllKind, or a spawn of a
 * kind whose pack a caller installed by hand (the host test's mock).
 * Gone with the heap; ftManagerReleaseFilesAll drops the pointers. */
static FTAttributes *ftManagerKindAttributes(s32 fkind)
{
    if (sFTManagerAttrs[fkind] == NULL)
    {
        sFTManagerAttrs[fkind] = syTaskmanMalloc(sizeof(FTAttributes), 0x8);
    }
    return sFTManagerAttrs[fkind];
}

/* Every pack ftManagerSetupFilesAllKind loaded, given back: its VRAM,
 * its headers and its clip buffer (fighter_release), and the Fighter
 * that held it. Where the game's files go with the scene heap -- the
 * scene manager between scenes, and syTaskmanResetGeneralHeap for a
 * heap reset inside a scene (sudden death), through the hook the first
 * load installs, the same two places the sprite banks are released
 * from (src/dc/sprite.c). A pack a caller installed in gFTManagerModels
 * by hand is not the manager's to free: only the kinds it loaded itself
 * are touched, and it knows them by the flag beside each. */
static u8 sFTManagerLoadedByManager[nFTKindEnumCount];

/* DIVERGES, and this one is a deviation from the game rather than a
 * substitution for it.
 *
 * The N64's fighter files come out of a scene's status buffer
 * (lb/lbreloc.c: a scene declares its file ids in SCSceneSetup, the
 * loader DMAs them into the scene heap, lbRelocGetStatusBufferFile
 * hands out the addresses). That buffer is per scene and is rebuilt
 * from the ROM every time, so the game genuinely re-reads Mario's model
 * on the way from the character select into the battle. On a cartridge
 * that is a DMA of a few hundred kilobytes at bus speed with nothing to
 * seek to; on a GD-ROM it is 911,064 bytes and two file opens for the
 * two fighters a VS match plays, and up to 1,311,336 for a
 * four-player match, in a scene change that has nothing else to do.
 *
 * So the port keeps them. A kind marked here survives the release the
 * scene change runs, and nothing else changes: the pack is still the
 * one ftManagerSetupFilesAllKind loaded, still freed by
 * ftManagerReleaseFilesAll once nobody has marked it, still absent from
 * the overlay clear. The mark is not a reference count -- there is only
 * ever one writer, ftManagerKeepFilesForScene below, and it recomputes
 * the whole set at every scene change, so a mark cannot outlive the
 * path it was taken for.
 *
 * What is *not* kept is everything that comes out of the scene heap:
 * the attributes (dropped below and re-cut at the next setup) and the
 * kind's sprite bank (src/dc/sprite.c releases those on the same two
 * occasions). Only the pack, whose blob, VRAM and headers are malloc'd
 * and pvr_mem_malloc'd and belong to no heap. */
static u8 sFTManagerKeepKinds[nFTKindEnumCount];

/* The opening movie's scenes after the Room, up to Clash: the eight
 * fighters' own scenes each load one pack, and Run and Clash load all
 * eight at once, so a movie that gave its packs back at every cut read
 * each of them from the disc four or five times -- about 20 MB of the
 * 42 MB the movie reads. Scene enum order is not
 * the play order, hence the list. The Room is not here: what it is
 * entered from (the startup, the title, the attract loop) owes it
 * nothing. Nor is Newcomers, the first scene after Clash, which uses no
 * pack, so the movie's packs are all given back on the way into it --
 * and on the way into the title when a player skips the movie. */
sb32 ftManagerSceneIsOpeningChain(s32 scene)
{
    switch (scene)
    {
    case nSCKindOpeningPortraits:
    case nSCKindOpeningMario:
    case nSCKindOpeningDonkey:
    case nSCKindOpeningSamus:
    case nSCKindOpeningFox:
    case nSCKindOpeningLink:
    case nSCKindOpeningYoshi:
    case nSCKindOpeningPikachu:
    case nSCKindOpeningKirby:
    case nSCKindOpeningRun:
    case nSCKindOpeningYoster:
    case nSCKindOpeningCliff:
    case nSCKindOpeningStandoff:
    case nSCKindOpeningYamabuki:
    case nSCKindOpeningClash:
    case nSCKindOpeningSector:
    case nSCKindOpeningJungle:
        return TRUE;

    default:
        return FALSE;
    }
}

/* The scenes the 1P ladder's router runs between rungs, and the rungs. */
sb32 ftManagerSceneIs1PLadder(s32 scene)
{
    switch (scene)
    {
    case nSCKind1PIntro:
    case nSCKind1PGame:
    case nSCKind1PBonusStage:
    case nSCKind1PStageClear:
    case nSCKind1PContinue:
    case nSCKind1PChallenger:
        return TRUE;

    default:
        return FALSE;
    }
}

/* Two paths keep packs.
 *
 * - The scenes on the way to a battle, whose fighters the battle state
 *   already names: the stage select the character select hands to, and
 *   the battle itself.
 * - The opening movie (above): every playable kind already loaded stays
 *   until Clash is over, which is the last scene to want one. Master
 *   Hand, which only the Room loads, is not playable and goes.
 *
 * Everything else -- the title, the menus, the results screen -- clears
 * the set, so a player who backs out of the stage select is not carrying
 * a match's packs around the menus. */
void ftManagerKeepFilesForScene(s32 scene)
{
    s32 i;

    for (i = 0; i < nFTKindEnumCount; i++)
    {
        sFTManagerKeepKinds[i] = 0;
    }
    if (ftManagerSceneIsOpeningChain(scene) != FALSE)
    {
        for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
        {
            sFTManagerKeepKinds[i] = sFTManagerLoadedByManager[i];
        }
        return;
    }
    /* The 1P ladder: the router runs
     * card, rung and Stage Clear over and over with the same human, and
     * the card loads exactly the fighters its rung then plays. So the
     * human's pack is kept across every ladder scene, and on the way from
     * the card into its rung everything the card loaded is kept too. The
     * census measured each rung reading the human's pack three times and
     * the enemies' twice. */
    if (ftManagerSceneIs1PLadder(scene) != FALSE)
    {
        s32 human = gSCManagerSceneData.fkind;

        if ((human >= nFTKindPlayableStart) && (human <= nFTKindPlayableEnd))
        {
            sFTManagerKeepKinds[human] = sFTManagerLoadedByManager[human];
        }
        if ((scene == nSCKind1PGame) &&
            (gSCManagerSceneData.scene_prev == nSCKind1PIntro ||
             gSCManagerSceneData.scene_curr == nSCKind1PIntro))
        {
            for (i = 0; i < nFTKindEnumCount; i++)
            {
                sFTManagerKeepKinds[i] |= sFTManagerLoadedByManager[i];
            }
        }
        return;
    }
    /* The results screen and the character select load all twelve packs
     * (at the menu tier), so a pack already resident -- the four the
     * match just played -- is one they would otherwise free and read back
     * at once. The census measured every VS results screen doing exactly
     * that. Keeping them cannot raise the
     * peak: both scenes hold every kind anyway. */
    if ((scene == nSCKindVSResults) || (scene == nSCKindPlayersVS))
    {
        for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
        {
            sFTManagerKeepKinds[i] = sFTManagerLoadedByManager[i];
        }
        return;
    }
    if ((scene != nSCKindMaps) && (scene != nSCKindVSBattle))
    {
        return;
    }
    /* gSCManagerTransferBattleState, not gSCManagerBattleState: the
     * battle sets that pointer at scVSBattleStartScene, and this runs
     * before the battle's scene starts. The transfer state is what
     * mnPlayersVSSetSceneData wrote and what scVSBattleStartScene will
     * copy from, so it names the same fighters the battle's own
     * ftManagerSetupFilesAllKind loop will ask for. */
    for (i = 0; i < ARRAY_COUNT(gSCManagerTransferBattleState.players); i++)
    {
        s32 fkind = gSCManagerTransferBattleState.players[i].fkind;

        if (gSCManagerTransferBattleState.players[i].pkind == nFTPlayerKindNot)
        {
            continue;
        }
        if ((fkind >= 0) && (fkind < nFTKindEnumCount))
        {
            sFTManagerKeepKinds[fkind] = 1;
        }
    }
}

/* Whether a kind's pack would survive the next release: the instrument
 * and the host test ask, and nothing in the game does. */
sb32 ftManagerKindIsKept(s32 fkind)
{
    return (fkind >= 0) && (fkind < nFTKindEnumCount) &&
           sFTManagerKeepKinds[fkind];
}

/* -DDB_FT_RELEASE_TRACE, the port's own: mallinfo either
 * side of every pack load and of this release, which is what a scene
 * that runs out of main RAM needs and what the failure print below
 * cannot give -- that one says only how full the heap was, not what
 * filled it or whether the scene before gave its packs back.
 *
 * Reading it: a "release: N freed" line whose two numbers differ by
 * about the packs the last scene loaded means the scene change is
 * clean, and the heap that the next scene then fails in is being spent
 * on something other than packs. That is exactly what it said for the
 * Fighting Polygons' rung, and the something was the Fighter struct
 * (src/dc/fighter.h). Off by default: it prints per load. */
/* ftManagerSetAnmTier (ftcommon.h): what this scene's loads read of each
 * .anm, back to the whole file at every release below */
static s32 sFTManagerAnmTier = FIGHTER_ANM_FULL;

void ftManagerSetAnmTier(s32 tier)
{
    sFTManagerAnmTier = tier;
}

/* Whether a fighter of `fkind` is live, playing from its pack's .anm. */
static sb32 ftManagerKindIsLive(s32 fkind)
{
    GObj *gobj;

    for (gobj = gGCCommonLinks[nGCCommonLinkIDFighter]; gobj != NULL; gobj = gobj->link_next)
    {
        if (ftGetStruct(gobj)->fkind == fkind)
        {
            return TRUE;
        }
    }
    return FALSE;
}

void ftManagerReleaseFilesAll(void)
{
    s32 i;

    sFTManagerAnmTier = FIGHTER_ANM_FULL;
#if !defined(FT_HOSTTEST) && defined(DB_FT_RELEASE_TRACE)
    s32 freed = 0, kept = 0;
    unsigned before = (unsigned)mallinfo().uordblks;
#endif

    for (i = 0; i < nFTKindEnumCount; i++)
    {
        if (sFTManagerLoadedByManager[i] && gFTManagerModels[i] != NULL)
        {
            /* kept: the pack and the flag that says whose it is stay,
             * so the release after this one -- the scene change that
             * does not mark it -- is the one that frees it */
            if (sFTManagerKeepKinds[i])
            {
                lc_mark(ftManagerKindName(i) ? ftManagerKindName(i) : "?", 2);
                sFTManagerAttrs[i] = NULL;
#if !defined(FT_HOSTTEST) && defined(DB_FT_RELEASE_TRACE)
                kept++;
#endif
                continue;
            }
            lc_mark(ftManagerKindName(i) ? ftManagerKindName(i) : "?", 0);
            fighter_release(gFTManagerModels[i]);
            free(gFTManagerModels[i]);
            gFTManagerModels[i] = NULL;
#if !defined(FT_HOSTTEST) && defined(DB_FT_RELEASE_TRACE)
            freed++;
#endif
        }
        sFTManagerLoadedByManager[i] = 0;
        sFTManagerAttrs[i] = NULL;
    }
#if !defined(FT_HOSTTEST) && defined(DB_FT_RELEASE_TRACE)
    syDebugPrintf("ft : release: %d freed, %d kept, %u -> %u bytes in use\n",
                  (int)freed, (int)kept, before,
                  (unsigned)mallinfo().uordblks);
#endif
}

/* ft/ftmanager.c:352-362 ftManagerSetupFilesAllKind 0x800D786C: the
 * game's loads a kind's main file and its model, motion, shield-pose
 * and special files out of the ROM if the main file is not resident
 * yet. The port's kind is one pack (ftcommon.h) and one sprite bank,
 * both named after the fighter, both loaded here if not resident and
 * both released with the scene (ftManagerReleaseFilesAll): the pack's
 * blob is read where it lies and its textures go to VRAM
 * (src/dc/fighter.c fighter_load), and the bank -- the stock icon and
 * emblem the HUD draws -- takes VRAM and scene heap. The test for
 * "already resident" is gFTManagerModels for the pack and the bank's
 * flag for the bank. Every caller is the game's own: the battle for
 * each player's kind, the character select and the results screen for
 * all twelve. DIVERGES: a kind with no pack on the disc (any fighter a
 * build leaves out) is skipped with a line on the log, where the game would DMA its files; a kind with a pack but no
 * bank plays with no stock icon and no emblem, which is what the game
 * does for a kind whose FTSprites is NULL
 * (ifCommonPlayerDamageInitInterface, ifCommonPlayerStockMultiMakeInterface
 * test for it). */
/* The port's own, and the only one in this file that is: a kind's
 * sprites without a fighter to hang them on.
 *
 * The game reaches a stock icon through ftGetStruct(gobj)->attr->sprites
 * (mn/mnvsmode/mnvsresults.c:1874 mnVSResultsMakeHeader), which the port
 * does too for the podium fighters. This is the
 * same FTSprites without a fighter to reach it through, for the host
 * tests -- the one ftManagerSetupFilesAllKind filled above. NULL for a kind whose bank is not on the disc, which is
 * what attr->sprites is for that kind too. */
FTSprites *ftManagerGetKindSprites(s32 fkind)
{
    if ((fkind < 0) || (fkind >= nFTKindEnumCount))
    {
        return NULL;
    }
    return sFTManagerSpriteBanks[fkind].is_loaded ? &sFTManagerSprites[fkind]
                                                  : NULL;
}

/* ---- ft/ftmanager.c:281-296 ftManagerSetupFilesMainKind, the particle
 * half ----
 *
 * Three fighters carry a particle bank of their own, and the game loads
 * it here, out of the FTData row the port does not compile:
 *
 *     if (data->particles_script_lo != 0x0)
 *         *data->p_particle = efParticleGetLoadBankID(
 *             data->particles_script_lo, data->particles_script_hi,
 *             data->particles_texture_lo, data->particles_texture_hi);
 *
 * Kirby's is particles_unk0, Ness's unk1 and Yoshi's unk2
 * (ssb-decomp-re/src/ft/ftdata.c:3891 and :4957), and each row names the
 * four segment symbols directly rather than an l<Name>Particle...
 * constant -- which is why tools/export/ssb_particleexport.py did not emit them
 * by name. All three banks are on the disc, cut
 * by the same exporter as efcommon's.
 *
 * DIVERGES: this table stands in for the four FTData fields, as
 * sFTManagerKindNames above stands in for file_model_id. It carries the
 * three fighters whose bank has a consumer in the build; Ness's joined
 * with his specials, whose PK Fire pillar is its first
 * script (it/itfighter/itnesspkfire.c itNessPKFireMakeItem).
 *
 * The bank id matters more than it looks. gFTDataKirbyParticleBankID is
 * bss-zero until this runs, and bank 0 is efcommon's -- so a maker that
 * called through an unloaded id would not draw nothing, it would draw
 * the COMMON bank's script of that number, silently and wrongly. ---- */
typedef struct
{
    s32 fkind;
    s32 *p_bank_id;
    uintptr_t script_lo, script_hi, texture_lo, texture_hi;
    const char *name;
} FTManagerParticleBank;

extern s32 gFTDataKirbyParticleBankID;
extern s32 gFTDataYoshiParticleBankID;
extern s32 gFTNessParticleBankID;

extern u8 particles_unk0_scb_ROM_START[], particles_unk0_scb_ROM_END[];
extern u8 particles_unk0_txb_ROM_START[], particles_unk0_txb_ROM_END[];
extern u8 particles_unk1_scb_ROM_START[], particles_unk1_scb_ROM_END[];
extern u8 particles_unk1_txb_ROM_START[], particles_unk1_txb_ROM_END[];
extern u8 particles_unk2_scb_ROM_START[], particles_unk2_scb_ROM_END[];
extern u8 particles_unk2_txb_ROM_START[], particles_unk2_txb_ROM_END[];

static const FTManagerParticleBank dFTManagerParticleBanks[] =
{
    { nFTKindKirby, &gFTDataKirbyParticleBankID,
      (uintptr_t)particles_unk0_scb_ROM_START,
      (uintptr_t)particles_unk0_scb_ROM_END,
      (uintptr_t)particles_unk0_txb_ROM_START,
      (uintptr_t)particles_unk0_txb_ROM_END, "particles_unk0" },
    { nFTKindNess, &gFTNessParticleBankID,
      (uintptr_t)particles_unk1_scb_ROM_START,
      (uintptr_t)particles_unk1_scb_ROM_END,
      (uintptr_t)particles_unk1_txb_ROM_START,
      (uintptr_t)particles_unk1_txb_ROM_END, "particles_unk1" },
    { nFTKindYoshi, &gFTDataYoshiParticleBankID,
      (uintptr_t)particles_unk2_scb_ROM_START,
      (uintptr_t)particles_unk2_scb_ROM_END,
      (uintptr_t)particles_unk2_txb_ROM_START,
      (uintptr_t)particles_unk2_txb_ROM_END, "particles_unk2" },
};

/* Kirby's copy of another fighter does not change whose bank his own
 * effects come out of, so this is keyed on the kind that ASKED. Called
 * once per kind per scene; efParticleGetLoadBankID caches by ROM address
 * within a scene and lbpTexLoadBank by name, so a second ask is two
 * lookups. */
static void ftManagerSetupParticleBankKind(s32 fkind)
{
    u32 i;

#ifdef FT_HOSTTEST
    /* DIVERGES, and only in this build, for the reason
     * efManagerLoadEffectBank states at length (src/dc/efmanager.c): the
     * host cross-tests run on x86-64, where an `LBScript *` is eight
     * bytes and the bank's own arrays are four, so lbParticleSetupBankID
     * walks them at twice the stride and off the end of the block. The
     * common bank is not loaded in this build for the same reason and
     * these two are no different. Left bss-zero rather than -1: nothing
     * in the host suite calls a maker that reads one, and a zero id at
     * least names a bank that exists. */
    (void)fkind;
    (void)i;
    return;
#else
    for (i = 0; i < ARRAY_COUNT(dFTManagerParticleBanks); i++)
    {
        const FTManagerParticleBank *b = &dFTManagerParticleBanks[i];

        if (b->fkind != fkind)
        {
            continue;
        }
        *b->p_bank_id = efParticleGetLoadBankID(b->script_lo, b->script_hi,
                                                b->texture_lo, b->texture_hi);

        if (lbpTexLoadBank(*b->p_bank_id, b->name) != 0)
        {
            syDebugPrintf("ft : %s.txp did not load for kind %d\n",
                          b->name, (int)fkind);
        }
        else
        {
            syDebugPrintf("particles: %s bank %d for kind %d\n",
                          b->name, (int)*b->p_bank_id, (int)fkind);
        }
        return;
    }
#endif
}

const char *ftManagerKindName(s32 fkind)
{
    if (fkind < 0 || fkind >= nFTKindEnumCount)
    {
        return NULL;
    }
    return sFTManagerKindNames[fkind];
}

void ftManagerSetupFilesAllKind(s32 fkind)
{
    SpriteBank *bank;
    FTSprites *sprites;
    const char *name;
    char path[48];
    DCSpriteEntry *stock, *emblem;

    if (fkind < 0 || fkind >= nFTKindEnumCount ||
        (name = sFTManagerKindNames[fkind]) == NULL)
    {
        return;
    }
    /* Installed on every ask, not only on a load: a
     * scene can find every pack it wants already resident and load
     * none, and the hook still has to run -- the attributes below come
     * out of the heap it resets, and ftManagerReleaseFilesAll is what
     * drops them. The hook is a set and never removed
     * (src/dc/taskman.c), so this only stops the install depending on
     * some earlier scene having missed the cache. */
    syTaskmanAddHeapResetHook(ftManagerReleaseFilesAll);

    /* ft/ftmanager.c:304: the game loads the kind's MainMotion file here
     * whatever the scene, and ftManagerMakeFighter reads Kirby's copy
     * table out of it. A menu loads overlay 2, which zeroes the pointer,
     * and not overlay 3, whose reload was the only thing binding it: a
     * menu's Kirby read the table from address 0, the boot ROM, and his
     * body's model part was whatever byte that BIOS has there. */
    ftCommonBindMainMotionFiles();

    if (gFTManagerModels[fkind] == NULL)
    {
        Fighter *pack = malloc(sizeof(Fighter));
        int pal_bank = 0;

        snprintf(path, sizeof(path), "%s.pack", name);
        if (pack == NULL ||
            fighter_load_tier(pack, path, &pal_bank, sFTManagerAnmTier) < 0)
        {
            syDebugPrintf("ft : no pack for kind %d at %s\n", fkind, path);
#ifndef FT_HOSTTEST
            /* The port's own: a pack that will not load is
             * almost always main RAM rather than the disc, and the two
             * read alike in the log ("short read on <pack>" is what an
             * out-of-memory blob prints). The Fighting Polygons' rung is
             * where the port first ran out -- twelve packs and the
             * human's -- so the message says how much was in use when
             * it failed rather than leaving the next reader to guess. */
            syDebugPrintf("ft : %u malloc bytes in use, %u arena\n",
                          (unsigned)mallinfo().uordblks,
                          (unsigned)mallinfo().arena);
#endif
            free(pack);
            return;
        }
        /* fighter_init assigns PVR palette banks from *pal_bank up, and
         * bank 0 is a stage's; a fighter pack bakes its palettes out
         * (pal_count 0), and one that did not would need the banks
         * assigned on bind that src/dc/fighter.c's note describes. */
        if (pack->hd->pal_count != 0 || pack->hd->vert_count > FTMANAGER_VCLIP_MAX)
        {
            syDebugPrintf("ft : %s: %u palette banks, %u vertices; the manager "
                          "takes 0 and at most %d\n", path,
                          (unsigned)pack->hd->pal_count,
                          (unsigned)pack->hd->vert_count, FTMANAGER_VCLIP_MAX);
            fighter_release(pack);
            free(pack);
            return;
        }
        gFTManagerModels[fkind] = pack;
        sFTManagerLoadedByManager[fkind] = 1;
        lc_mark(name, 1);
        syDebugPrintf("ft : %s loaded: %u joints, %u tris, %u anims, %u bytes\n",
                      path, (unsigned)pack->hd->joint_count,
                      (unsigned)pack->hd->tri_count,
                      (unsigned)pack->hd->anim_count,
                      (unsigned)pack->blob_size);
#if !defined(FT_HOSTTEST) && defined(DB_FT_RELEASE_TRACE)
        syDebugPrintf("ft : after %s: %u bytes in use, %u arena\n", path,
                      (unsigned)mallinfo().uordblks,
                      (unsigned)mallinfo().arena);
#endif
    }
    else if (fighter_anm_grow(gFTManagerModels[fkind], sFTManagerAnmTier,
                              ftManagerKindIsLive(fkind)) < 0)
    {
        /* kept from a menu that read less of it, and out of RAM for
         * the rest: every bind after this says what it could not play */
        syDebugPrintf("ft : %s.anm did not grow for kind %d\n", name, fkind);
    }
    ftManagerKindAttributes(fkind);
    ftManagerSetupParticleBankKind(fkind);

    bank = &sFTManagerSpriteBanks[fkind];
    sprites = &sFTManagerSprites[fkind];
    if (bank->is_loaded)
    {
        return;
    }
    snprintf(path, sizeof(path), "ft%s.spr", name);

    sprites->stock_sprite = NULL;
    sprites->stock_luts = NULL;
    sprites->emblem = NULL;

    if (sprite_bank_load(bank, path) != 0)
    {
        return;
    }
    stock = ftManagerFindSprite(bank, "Stock");
    emblem = ftManagerFindSprite(bank, "FTEmblem");

    if (stock != NULL)
    {
        sprites->stock_sprite = &stock->sprite;
        sprites->stock_luts = stock->luts;
    }
    if (emblem != NULL)
    {
        sprites->emblem = &emblem->sprite;
    }
}

/* ft/ftdata.c:75-96 dFTManagerDefaultFighterDesc 0x80116DD0. The
 * decomp's is a positional initialiser whose words do not line up with
 * FTDesc's fields (its 0x00090300 lands in copy_kind), so this one is
 * read field by field off the ROM (file offset 0x925D8): the display
 * proc is the game's own (src/dc/ftdisplaymain.c).
 *
 * The decomp's list taken one byte late would leave
 * copy_kind 0. That field is what every VS fighter and every rebirth
 * (ftcommonrebirth.c:23) starts Kirby's copy_id from, and 0 is Mario:
 * Kirby spawned, and came back from every KO, with Mario's fireball. */
FTDesc dFTManagerDefaultFighterDesc =
{
    nFTKindNull,
    { 0.0f, 0.0f, 0.0f },
    +1,                             /* lr */
    0x00, 0x00, 0x01, 0x00, 0x00,   /* team, player, detail, costume, shade */
    0x09, 0x03, 0x00, 0x00,         /* handicap, level, stock_count, unk_1C */
    0x03, 0x00,                     /* unk_1D, team_order */
    0x00,                           /* is_skip_entry */
    0x00,                           /* is_skip_shadow_setup */
    0x00,                           /* is_magnify_ignore */
    nFTKindKirby,                   /* copy_kind */
    0x00000000,                     /* damage */
    nFTPlayerKindDemo,              /* pkind: the menus' fighters -- the
                                     * character select's, the results
                                     * screen's -- take the description
                                     * as it is, and run on
                                     * scSubsysFighterProcUpdate with no
                                     * physics (ft/ftmanager.c:865) */
    NULL,                           /* controller */
    A_BUTTON,
    B_BUTTON,
    Z_TRIG,
    L_TRIG,
    NULL,                           /* figatree_heap */
    ftDisplayMainProcDisplay
};

/* The pack's FPackAttr (src/dc/fighter.h) into the game's FTAttributes
 * (ft/fttypes.h:870). DIVERGES: the game reads the whole struct off the
 * ROM file; the pack carries the fields the ported code reads, and the
 * rest is zero -- with one exception that keeps the game's own loop
 * honest about it: the word past cliff_status_ga that the game reads
 * for CliffEscapeSlow (fttypes.h:952 unused_0x2CC, fighter.h on why) is
 * the pack's sixth entry. The hurtboxes, the hit-detect range, the
 * is_have bits and the animlock words are the pack's; animlock is
 * a pointer in the game and points at the pack's two words here, which
 * live as long as the pack does. */
_Static_assert(FPACK_JOINT_NUM_MAX == FTPARTS_JOINT_NUM_MAX,
               "FPackAttr.translate_scales has a row per joint");
_Static_assert(sizeof(Vec3f) == 3 * sizeof(float),
               "a translate_scales row is a Vec3f");

/* fttypes.h:972 FTAttributes.skeleton's three words, for
 * the one reader, ftDisplayMainDrawAll: word 0 the joint id it checks,
 * words 1 and 2 non-NULL for the skeleton ids the fighter has. DIVERGES:
 * the game's words 1 and 2 are FTSkeleton tables of display lists per
 * joint, which the pack bakes as tags (FPackPartTag.skeleton), so they
 * point at an empty FTSkeleton here. The rows depend only on the joint
 * and the mask, so one row per pair serves every kind and every spawn
 * without touching the scene heap or the const pack. NULL for none. */
static FTSkeleton sFTManagerSkeletonNone;
static FTSkeleton *sFTManagerSkeletonRows[nFTKindEnumCount][3];
static s32 sFTManagerSkeletonRowsNum;

FTSkeleton **ftManagerGetSkeletonWords(s32 joint, u32 ids)
{
    FTSkeleton **row;
    s32 i;

    if (ids == 0)
    {
        return NULL;
    }
    for (i = 0; i < sFTManagerSkeletonRowsNum; i++)
    {
        row = sFTManagerSkeletonRows[i];

        if (((intptr_t)row[0] == joint) &&
            ((row[1] != NULL) == ((ids & 2) != 0)) &&
            ((row[2] != NULL) == ((ids & 4) != 0)))
        {
            return row;
        }
    }
    if (sFTManagerSkeletonRowsNum == ARRAY_COUNT(sFTManagerSkeletonRows))
    {
        syDebugPrintf("ft : no room for skeleton row (%d, %x)\n", joint, (unsigned)ids);
        return NULL;
    }
    row = sFTManagerSkeletonRows[sFTManagerSkeletonRowsNum++];
    row[0] = (FTSkeleton *)(intptr_t)joint;
    row[1] = (ids & 2) ? &sFTManagerSkeletonNone : NULL;
    row[2] = (ids & 4) ? &sFTManagerSkeletonNone : NULL;

    return row;
}

void ftManagerSetupAttributes(FTAttributes *attr, const FPackAttr *pa)
{
    s32 i;

    memset(attr, 0, sizeof(*attr));

    attr->is_metallic = pa->is_metallic;
    attr->hit_detect_range.x = pa->hit_detect_range[0];
    attr->hit_detect_range.y = pa->hit_detect_range[1];
    attr->hit_detect_range.z = pa->hit_detect_range[2];
    attr->animlock = (u32 *)pa->animlock;
    /* the joint tables: the setup mask and the hidden-part
     * rows are pointers in the game, into the fighter's main file; here
     * they point into the pack, which lives as long as the fighter kind
     * is loaded. Nothing writes through either. The rows are
     * FTHiddenPart's four s32 in its order (fighter.h). */
    attr->setup_parts = (u32 *)pa->setup_parts;
    attr->hiddenparts = (FTHiddenPart *)pa->hiddenparts;
    /* fttypes.h:969, a completed
     * grab's release (ftCommonThrowSetStatus, ftcommon.c) reads this
     * pointer unconditionally, indexed by the caught fighter's own
     * fkind. Left unwired, the memset above left it NULL, and that
     * dereference landed near address zero and read back whatever
     * incidental bytes live there rather than real per-fighter throw
     * data -- garbage in, and ftMainSetStatus's own "no table row"
     * bounds check caught the resulting nonsense status id and the
     * target aborted, the first time a real grab -> throw ever actually
     * completed on it. */
    attr->thrown_status = (FTThrownStatusArray *)pa->thrown_status;

    /* fttypes.h:964 translate_scales: Luigi's per-joint
     * translation scales, or NULL for everyone else, which is what
     * ftManagerMakeFighter's is_have_translate_scale and ftMainSetStatus's
     * per-motion toggle both test. Vec3f is three floats, as the pack's
     * rows are. */
    attr->translate_scales = (pa->is_have_translate_scales != 0)
        ? (Vec3f *)pa->translate_scales : NULL;

    /* fttypes.h:967 textureparts_container: the faces'
     * joint and MObj index, the game's bytes in the pack, or NULL for
     * Donkey Kong and Samus, who have none */
    attr->textureparts_container = (pa->is_have_textureparts != 0)
        ? (FTTexturePartContainer *)pa->textureparts : NULL;

    /* fttypes.h:966 accesspart: Pikachu's hat and
     * Jigglypuff's bow. The joint is the game's; the display list, MObjs
     * and costume scripts are NULL, the accessory being a part of the
     * pack (fighter.h FPackParts.accessory_tag). */
    _Static_assert(sizeof(FTAccessPart) <= sizeof(pa->accesspart), "FPackAttr.accesspart holds an FTAccessPart");
    attr->accesspart = (pa->is_have_accesspart != 0)
        ? (FTAccessPart *)pa->accesspart : NULL;
    attr->skeleton = ftManagerGetSkeletonWords(pa->skeleton_joint, pa->skeleton_ids);

    /* fttypes.h:917-920 and :968/:970, the item half: the pickup rectangle ftCommonGetFindItem tests, the two
     * percentages a thrown item's speed and damage are scaled by, the
     * heavy-lift grunt, and which joint a held item hangs off. */
    attr->item_pickup.pickup_offset_light.x = pa->item_pickup[0];
    attr->item_pickup.pickup_offset_light.y = pa->item_pickup[1];
    attr->item_pickup.pickup_range_light.x  = pa->item_pickup[2];
    attr->item_pickup.pickup_range_light.y  = pa->item_pickup[3];
    attr->item_pickup.pickup_offset_heavy.x = pa->item_pickup[4];
    attr->item_pickup.pickup_offset_heavy.y = pa->item_pickup[5];
    attr->item_pickup.pickup_range_heavy.x  = pa->item_pickup[6];
    attr->item_pickup.pickup_range_heavy.y  = pa->item_pickup[7];
    attr->itemthrow_vel_scale = pa->itemthrow_vel_scale;
    attr->itemthrow_damage_scale = pa->itemthrow_damage_scale;
    attr->heavyget_sfx = pa->heavyget_sfx;
    /* fttypes.h:913-916, the voice half (the audio step): before it the
     * memset above left all seven 0, nSYAudioFGMExplodeS, which is a
     * real sound -- so the "!= nSYAudioFGMVoiceEnd" tests in front of
     * every one of them passed and each voice played an explosion. */
    attr->dead_fgm_ids[0] = pa->dead_fgm_ids[0];
    attr->dead_fgm_ids[1] = pa->dead_fgm_ids[1];
    attr->deadup_sfx = pa->deadup_sfx;
    attr->damage_sfx = pa->damage_sfx;
    attr->smash_sfx[0] = pa->smash_sfx[0];
    attr->smash_sfx[1] = pa->smash_sfx[1];
    attr->smash_sfx[2] = pa->smash_sfx[2];
    attr->joint_itemheavy_id = pa->joint_itemheavy_id;
    attr->joint_itemlight_id = pa->joint_itemlight_id;
    /* fttypes.h:957-963: the feet
     * mpCommonUpdateFighterSlopeContour bends onto a slope */
    attr->joint_rfoot_id = pa->joint_rfoot_id;
    attr->joint_rfoot_rotate = pa->joint_rfoot_rotate;
    attr->joint_lfoot_id = pa->joint_lfoot_id;
    attr->joint_lfoot_rotate = pa->joint_lfoot_rotate;
    attr->unk_0x31C = pa->unk_0x31C;
    attr->unk_0x320 = pa->unk_0x320;

    /* the ROM's one word, MSB first (fttypes.h:924-945 in order) */
#define FT_HAVE(name, bit) attr->is_have_##name = (pa->is_have >> (bit)) & 1
    FT_HAVE(attack11, 31);     FT_HAVE(attack12, 30);
    FT_HAVE(attackdash, 29);   FT_HAVE(attacks3, 28);
    FT_HAVE(attackhi3, 27);    FT_HAVE(attacklw3, 26);
    FT_HAVE(attacks4, 25);     FT_HAVE(attackhi4, 24);
    FT_HAVE(attacklw4, 23);    FT_HAVE(attackairn, 22);
    FT_HAVE(attackairf, 21);   FT_HAVE(attackairb, 20);
    FT_HAVE(attackairhi, 19);  FT_HAVE(attackairlw, 18);
    FT_HAVE(specialn, 17);     FT_HAVE(specialairn, 16);
    FT_HAVE(specialhi, 15);    FT_HAVE(specialairhi, 14);
    FT_HAVE(speciallw, 13);    FT_HAVE(specialairlw, 12);
    FT_HAVE(catch, 11);        FT_HAVE(voice, 10);
#undef FT_HAVE

    for (i = 0; i < ARRAY_COUNT(attr->damage_coll_descs); i++)
    {
        attr->damage_coll_descs[i].joint_id = pa->damage_coll_descs[i].joint_id;
        attr->damage_coll_descs[i].placement = pa->damage_coll_descs[i].placement;
        attr->damage_coll_descs[i].is_grabbable = pa->damage_coll_descs[i].is_grabbable;
        attr->damage_coll_descs[i].offset.x = pa->damage_coll_descs[i].offset[0];
        attr->damage_coll_descs[i].offset.y = pa->damage_coll_descs[i].offset[1];
        attr->damage_coll_descs[i].offset.z = pa->damage_coll_descs[i].offset[2];
        attr->damage_coll_descs[i].size.x = pa->damage_coll_descs[i].size[0];
        attr->damage_coll_descs[i].size.y = pa->damage_coll_descs[i].size[1];
        attr->damage_coll_descs[i].size.z = pa->damage_coll_descs[i].size[2];
    }

    attr->size = pa->size;
    attr->walkslow_anim_length = pa->walkslow_anim_length;
    attr->walkmiddle_anim_length = pa->walkmiddle_anim_length;
    attr->walkfast_anim_length = pa->walkfast_anim_length;
    attr->rebound_anim_length = pa->rebound_anim_length;
    attr->walk_speed_mul = pa->walk_speed_mul;
    attr->traction = pa->traction;
    attr->dash_speed = pa->dash_speed;
    attr->dash_decel = pa->dash_decel;
    attr->run_speed = pa->run_speed;
    attr->kneebend_anim_length = pa->kneebend_anim_length;
    attr->jump_vel_x = pa->jump_vel_x;
    attr->jump_height_mul = pa->jump_height_mul;
    attr->jump_height_base = pa->jump_height_base;
    attr->jumpaerial_vel_x = pa->jumpaerial_vel_x;
    attr->jumpaerial_height = pa->jumpaerial_height;
    attr->air_accel = pa->air_accel;
    attr->air_speed_max_x = pa->air_speed_max_x;
    attr->air_friction = pa->air_friction;
    attr->gravity = pa->gravity;
    attr->tvel_base = pa->tvel_base;
    attr->tvel_fast = pa->tvel_fast;
    attr->jumps_max = pa->jumps_max;
    attr->weight = pa->weight;
    attr->attack1_followup_frames = pa->attack1_followup_frames;
    attr->dash_to_run = pa->dash_to_run;
    attr->shield_size = pa->shield_size;
    attr->shield_break_vel_y = pa->shield_break_vel_y;
    attr->shadow_size = pa->shadow_size;
    attr->jostle_width = pa->jostle_width;
    attr->jostle_x = pa->jostle_x;
    attr->cam_offset_y = pa->cam_offset_y;
    attr->closeup_camera_zoom = pa->closeup_camera_zoom;
    attr->camera_zoom = pa->camera_zoom;
    attr->camera_zoom_base = pa->camera_zoom_base;
    attr->map_coll.top = pa->map_coll_top;
    attr->map_coll.center = pa->map_coll_center;
    attr->map_coll.bottom = pa->map_coll_bottom;
    attr->map_coll.width = pa->map_coll_width;
    attr->cliffcatch_coll.x = pa->cliffcatch_coll_x;
    attr->cliffcatch_coll.y = pa->cliffcatch_coll_y;
    /* fttypes.h:921. ft/ftcommon/ftcommonrebirth.c hands it to
     * efManagerRebirthHaloMakeEffect, which makes a
     * halo whose size it is. */
    attr->halo_size = pa->halo_size;
    /* fttypes.h:922-923, the team shade and the tint scale */
    for (i = 0; i < 3; i++)
    {
        attr->shade_color[i].r = pa->shade_color[i * 4 + 0];
        attr->shade_color[i].g = pa->shade_color[i * 4 + 1];
        attr->shade_color[i].b = pa->shade_color[i * 4 + 2];
        attr->shade_color[i].a = pa->shade_color[i * 4 + 3];
    }
    attr->fog_color.r = pa->fog_color[0];
    attr->fog_color.g = pa->fog_color[1];
    attr->fog_color.b = pa->fog_color[2];
    attr->fog_color.a = pa->fog_color[3];
    /* fttypes.h:876-878: the cargo walk's three lengths, which
     * ftDonkeyThrowFWalk divides by when the walk changes speed */
    attr->throw_walkslow_anim_length = pa->throw_walkslow_anim_length;
    attr->throw_walkmiddle_anim_length = pa->throw_walkmiddle_anim_length;
    attr->throw_walkfast_anim_length = pa->throw_walkfast_anim_length;

    /* fttypes.h:950, the five joints
     * ftParamGetEffectJointPosition cycles, which the gate into
     * ef/efmanager.c reads on twenty of its forty-five arms. */
    for (i = 0; i < ARRAY_COUNT(attr->effect_joint_ids); i++)
    {
        attr->effect_joint_ids[i] = pa->effect_joint_ids[i];
    }

    for (i = 0; i < ARRAY_COUNT(attr->cliff_status_ga); i++)
    {
        attr->cliff_status_ga[i] = pa->cliff_status_ga[i];
    }
    attr->unused_0x2CC = pa->cliff_status_ga[i];
}

/* FTAttributes.dobj_lookup and shield_anim_joints (ft/fttypes.h:955-956),
 * the two pointers the game's attributes carry into the fighter's
 * ShieldPose file, out of the pack (fighter.h FPackShieldPose): the loader made the rows the game's DObjDesc and the eight tables
 * pointer arrays, once per pack, and they live as long as the pack does.
 * The guard (ftcommon.c, ft/ftcommon/ftcommonguard1.c) reads both. */
void ftManagerSetupShieldPose(FTAttributes *attr, const Fighter *model)
{
    s32 i;

    attr->dobj_lookup = (DObjDesc *)model->shield_lookup;

    for (i = 0; i < ARRAY_COUNT(attr->shield_anim_joints); i++)
    {
        attr->shield_anim_joints[i] = (AObjEvent32 **)model->shield_joints[i];
    }
}

/* The port's own, and the other half of the instance pool's story (the
 * block at the top of the file): give back every clip buffer the slots
 * still hold, and the instances behind them, when the scene heap goes.
 * Only the selects and sc1pgame.c:1434 ever call
 * ftManagerDestroyFighter -- every other fighter in the game dies with
 * its scene, GObj and all -- so this is what keeps a rung's thirteen
 * from outliving the rung. It runs on the scene heap's reset and not on
 * a heap of its own because that IS the pools' lifetime: they are cut
 * in ftManagerAllocFighter, which a scene calls once as it starts. */
static void ftManagerReleaseVClipAll(void)
{
    s32 i;

    for (i = 0; i < FTMANAGER_ALLOC_MAX; i++)
    {
        free(sFTManagerVClipOwned[i]);
        sFTManagerVClipOwned[i] = NULL;
    }
    free(sFTManagerModelsAllocBuf);
    sFTManagerModelsAllocBuf = NULL;
}

/* ft/ftmanager.c:133-212 ftManagerAllocFighter 0x800D7180, the pool
 * half. DIVERGES: the figatree heap size (171-201) becomes the instance
 * pool above; the common and moveset files (167-169), the per-kind file
 * sizes are not carried; see the file comment. The menus' light setup
 * (209-212) is needed the light's alpha. */
void ftManagerAllocFighter(u32 data_flags, s32 allocs_num)
{
    s32 i;

    sFTManagerStructsAllocBuf = sFTManagerStructsAllocFree = syTaskmanMalloc(sizeof(FTStruct) * allocs_num, 0x8);

    bzero(sFTManagerStructsAllocBuf, sizeof(FTStruct) * allocs_num);

    for (i = 0; i < (allocs_num - 1); i++)
    {
        sFTManagerStructsAllocBuf[i].next = &sFTManagerStructsAllocBuf[i + 1];
    }
    sFTManagerStructsAllocBuf[i].next = NULL;

    /* lines 154-160: one FTParts per joint slot per fighter */
    sFTManagerPartsAllocFree = sFTManagerPartsAllocBuf = syTaskmanMalloc(sizeof(FTParts) * allocs_num * FTPARTS_JOINT_NUM_MAX, 0x8);

    for (i = 0; i < (allocs_num * FTPARTS_JOINT_NUM_MAX) - 1; i++)
    {
        sFTManagerPartsAllocBuf[i].next = &sFTManagerPartsAllocBuf[i + 1];
    }
    sFTManagerPartsAllocBuf[i].next = NULL;

    /* the instances, see above: main RAM, not the scene heap, and each
     * one's clip buffer comes later with the pack that says how long it
     * has to be. The free is for a scene that cuts the pools twice
     * without a heap reset between (scvsbattle.c has two call sites);
     * after a scene change the pointer is already NULL. */
    if (allocs_num > FTMANAGER_ALLOC_MAX)
    {
        syDebugPrintf("ft : %d fighter slots, the clip pointers %d\n",
                      allocs_num, FTMANAGER_ALLOC_MAX);
        scManagerRunPrintGObjStatus();
    }
    free(sFTManagerModelsAllocBuf);
    sFTManagerModelsAllocBuf = malloc(sizeof(Fighter) * allocs_num);

    if (sFTManagerModelsAllocBuf == NULL)
    {
        syDebugPrintf("ft : no room for %d fighter instances, %u bytes\n",
                      allocs_num, (unsigned)(sizeof(Fighter) * allocs_num));
        scManagerRunPrintGObjStatus();
    }
    syTaskmanAddHeapResetHook(ftManagerReleaseVClipAll);

    gFTManagerPlayersNum = 1;
    gFTManagerMotionCount = 1;
    gFTManagerStatUpdateCount = 1;

    if (data_flags & FTDATA_FLAG_SUBMOTION)
    {
        scSubsysFighterSetLightParams(45.0F, 45.0F, 0xFF, 0xFF, 0xFF, 0xFF);
    }
}

/* ft/ftmanager.c:215-233 ftManagerGetNextStructAlloc 0x800D7594, verbatim */
FTStruct *ftManagerGetNextStructAlloc(void)
{
    FTStruct *current_fighter;
    FTStruct *new_fighter = sFTManagerStructsAllocFree;

    if (new_fighter == NULL)
    {
        while (TRUE)
        {
            syDebugPrintf("couldn\'t get Fighter struct.\n");
            scManagerRunPrintGObjStatus();
        }
    }
    else current_fighter = new_fighter;

    sFTManagerStructsAllocFree = new_fighter->next;

    return current_fighter;
}

/* ft/ftmanager.c:236-240 ftManagerSetPrevStructAlloc 0x800D75EC, verbatim */
void ftManagerSetPrevStructAlloc(FTStruct *fp)
{
    fp->next = sFTManagerStructsAllocFree;
    sFTManagerStructsAllocFree = fp;
}

/* ft/ftmanager.c:243-271 ftManagerGetNextPartsAlloc 0x800D7600, verbatim */
FTParts *ftManagerGetNextPartsAlloc(void)
{
    FTParts *current_part;
    FTParts *new_part;

    new_part = sFTManagerPartsAllocFree;

    if (new_part == NULL)
    {
        while (TRUE)
        {
            syDebugPrintf("couldn\'t get FighterParts struct.\n");
            scManagerRunPrintGObjStatus();
        }
    }
    current_part = new_part;

    sFTManagerPartsAllocFree = new_part->next;

    new_part->transform_update_mode =
    new_part->unk_dobjtrans_0x5 =
    new_part->unk_dobjtrans_0x7 = 0;
    new_part->unk_dobjtrans_0x6 = 0;

    new_part->gobj = NULL;
    new_part->is_have_anim = FALSE;

    return current_part;
}

/* ft/ftmanager.c:274-278 ftManagerSetPrevPartsAlloc 0x800D7684, verbatim */
void ftManagerSetPrevPartsAlloc(FTParts *parts)
{
    parts->next = sFTManagerPartsAllocFree;
    sFTManagerPartsAllocFree = parts;
}

/* ft/ftmanager.c:364-369 ftManagerAllocFigatreeHeapKind 0x800D78D0.
 * DIVERGES: NULL. The game takes data->file_anim_size off the scene heap
 * for the largest animation the fighter can play and copies each one
 * into it (ft/ftmain.c:4622 lbRelocGetForceExternHeapFile); the port
 * plays the pack's animation in place. The call stays where the scene
 * makes it so the spawn reads as the game's. */
void *ftManagerAllocFigatreeHeapKind(s32 fkind)
{
    (void)fkind;

    return NULL;
}

/* ft/ftmanager.c:372-396 ftManagerDestroyFighter 0x800D78E8, verbatim
 * but for the shadow (no parts GObj is ever made: see
 * ftManagerMakeFighter). gcEjectGObj takes the DObj tree down with the
 * GObj. */
void ftManagerDestroyFighter(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 i;

    /* DIVERGES: the shadow ftManagerMakeFighter made goes with the
     * fighter. The game leaves it on link 13 for the scene heap to take
     * (src/dc/ftshadow.c ftShadowEjectShadow says why the port cannot). */
    ftShadowEjectShadow(fighter_gobj);

    if (fp->is_effect_attach)
    {
        ftParamProcStopEffect(fighter_gobj);
    }
    for (i = 0; i < ARRAY_COUNT(fp->joints); i++)
    {
        if (fp->joints[i] != NULL)
        {
            FTParts *parts = fp->joints[i]->user_data.p;

            if (parts->gobj != NULL)
            {
                gcEjectGObj(parts->gobj);
            }
            ftManagerSetPrevPartsAlloc(parts);
        }
    }
    /* DIVERGES: the clip buffer ftManagerMakeFighter asked for, which
     * nothing owns past this point -- the next fighter in the slot may
     * be a different kind and a different length. fighter_clone never
     * frees what it is handed (src/dc/fighter.h), so the manager does. */
    {
        s32 slot = fp - sFTManagerStructsAllocBuf;

        free(sFTManagerVClipOwned[slot]);
        sFTManagerVClipOwned[slot] = NULL;
    }
    ftManagerSetPrevStructAlloc(fp);
    gcEjectGObj(fighter_gobj);
}

/* ft/ftmanager.c:540-573, the floor projection in ftManagerInitFighter,
 * verbatim: stand on the floor below when one is within 300 units, else
 * start airborne. A function here rather than inline because the port's
 * respawn (ftMainRespawn) settles the same way. */
static void ftManagerProjectFighterFloor(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    sb32 is_collide_floor = mpCollisionCheckProjectFloor
    (
        &DObjGetStruct(fighter_gobj)->translate.vec.f,
        &fp->coll_data.floor_line_id,
        &fp->coll_data.floor_dist,
        &fp->coll_data.floor_flags,
        &fp->coll_data.floor_angle
    );

    if (is_collide_floor == FALSE)
    {
        fp->coll_data.floor_line_id = -1;
    }
    if ((is_collide_floor != FALSE) && (fp->coll_data.floor_dist > -300.0F) && (fp->fkind != nFTKindBoss))
    {
        fp->ga = nMPKineticsGround;

        DObjGetStruct(fighter_gobj)->translate.vec.f.y += fp->coll_data.floor_dist;

        fp->coll_data.floor_dist = 0;
    }
    else
    {
        fp->ga = nMPKineticsAir;
        fp->jumps_used = 1;
    }
    fp->coll_data.pos_prev = DObjGetStruct(fighter_gobj)->translate.vec.f;
}

/* ft/ftmanager.c:418-668 ftManagerInitFighter 0x800D79F0, verbatim,
 * including the Boss arm of the passive_vars switch. The floor projection is the helper above.
 * Non-static as the decomp's is (ft/ftmanager.h declares it by address,
 * func_ovl2_800D79F0): ftCommonRebirthDownSetStatus calls it to rebuild
 * a fighter on the respawn platform. */
void ftManagerInitFighter(GObj *fighter_gobj, FTDesc *desc)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    FTAttributes *attr = fp->attr;

    fp->lr = desc->lr;
    fp->percent_damage = desc->damage;

    if (fp->pkind != nFTPlayerKindDemo)
    {
        gSCManagerBattleState->players[fp->player].stock_damage_all = fp->percent_damage;
    }
    fp->shield_health = (fp->fkind == nFTKindYoshi) ? 55 : 55;

#ifdef BUGFIX_CRASH_SELFDESTRUCT
    fp->shield_player = -1;
#endif

    fp->unk_ft_0x38 = 0.0F;
    fp->hitlag_tics = 0;
    fp->is_knockback_paused = FALSE;

    ftPhysicsStopVelAll(fighter_gobj);

    fp->jumps_used = 0;
    fp->is_reflect = FALSE;
    fp->is_absorb = FALSE;
    fp->is_shield = FALSE;
    fp->is_effect_attach = FALSE;
    fp->is_jostle_ignore = FALSE;

    fp->cliffcatch_wait = 0;
    fp->tics_since_last_z = 0;

    fp->acid_wait = fp->twister_wait = fp->tarucann_wait = fp->damagefloor_wait = 0;

    fp->unk_ft_0x7AC = 0;
    fp->attack_damage = 0;
    fp->attack_count = 0;
    fp->attack_shield_push = 0;
    fp->shield_damage = 0;
    fp->damage_lag = 0;
    fp->damage_queue = 0;
    fp->damage_player = -1;
    fp->damage_object_class = 0;
    fp->damage_object_kind = 0;
    fp->damage_count = 0;
    fp->damage_kind = nFTDamageKindDefault;
    fp->damage_heal = 0;
    fp->damage_joint_id = 0;
    fp->invincible_tics = 0;
    fp->intangible_tics = 0;
    fp->star_invincible_tics = 0;

    fp->hitstatus = nGMHitStatusNormal;
    fp->star_hitstatus = nGMHitStatusNormal;
    fp->special_hitstatus = nGMHitStatusNormal;

    fp->throw_gobj = NULL;
    fp->catch_gobj = NULL;
    fp->capture_gobj = NULL;
    fp->is_catch_or_capture = FALSE;

    fp->item_gobj = NULL;

    fp->reflect_lr = 0;
    fp->absorb_lr = 0;

    fp->reflect_damage = 0;

    fp->special_coll = NULL;

    fp->attack1_followup_frames = 0.0F;
    fp->unk_ft_0x7A0 = 0.0F;
    fp->attack_knockback = 0.0F;
    fp->attack_rebound = 0.0F;
    fp->damage_knockback_stack = 0.0F;
    fp->knockback_resist_status = 0.0F;
    fp->knockback_resist_passive = 0.0F;
    fp->damage_knockback = 0.0F;
    fp->hitlag_mul = 1.0F;
    fp->shield_heal_wait = 10.0F;

    fp->is_fastfall = FALSE;

    fp->player_num = gFTManagerPlayersNum++;

    fp->public_knockback = 0.0F;

    fp->is_hitstun = FALSE;
    fp->is_use_animlocks = FALSE;

    fp->shuffle_frame_index = fp->shuffle_index_max = 0;

    fp->is_use_fogcolor = FALSE;

    fp->is_shuffle_electric = FALSE;
    fp->shuffle_tics = 0;

    fp->motion_attack_id = nFTMotionAttackIDNone;
    fp->motion_count = 0;
    fp->stat_flags.attack_id = nFTStatusAttackIDNone;
    fp->stat_flags.is_smash_attack = fp->stat_flags.ga = fp->stat_flags.is_projectile = 0;

    fp->stat_count = fp->damage_stat_count = 0;
    fp->damage_stat_flags = fp->stat_flags;

    fp->afterimage.desc_id = 0;

    DObjGetStruct(fighter_gobj)->translate.vec.f = desc->pos;
    DObjGetStruct(fighter_gobj)->scale.vec.f.x = DObjGetStruct(fighter_gobj)->scale.vec.f.y = DObjGetStruct(fighter_gobj)->scale.vec.f.z = attr->size;

    if (fp->pkind != nFTPlayerKindDemo)
    {
        ftManagerProjectFighterFloor(fighter_gobj);
    }
    else
    {
        fp->ga = nMPKineticsAir;
        fp->jumps_used = 1;
        fp->coll_data.pos_prev = DObjGetStruct(fighter_gobj)->translate.vec.f;
    }
    switch (fp->fkind)
    {
    case nFTKindMMario:
        fp->knockback_resist_passive = 30.0F;

        /* fallthrough */

    case nFTKindMario:
    case nFTKindNMario:
        fp->passive_vars.mario.is_expend_tornado = FALSE;
        break;

    case nFTKindGDonkey:
        fp->knockback_resist_passive = 48.0F;

        /* fallthrough */

    case nFTKindDonkey:
    case nFTKindNDonkey:
        fp->passive_vars.donkey.charge_level = 0;
        break;

    case nFTKindSamus:
    case nFTKindNSamus:
        fp->passive_vars.samus.charge_level = 0;
        fp->passive_vars.samus.charge_recoil = 0;
        break;

    case nFTKindLuigi:
    case nFTKindNLuigi:
        fp->passive_vars.mario.is_expend_tornado = FALSE;
        break;

    case nFTKindCaptain:
    case nFTKindNCaptain:
        fp->passive_vars.captain.falcon_punch_unk = 0;
        break;

    case nFTKindKirby:
    case nFTKindNKirby:
        fp->passive_vars.kirby.copy_id = desc->copy_kind;

        fp->passive_vars.kirby.copysamus_charge_level = 0;
        fp->passive_vars.kirby.copysamus_charge_recoil = 0;
        fp->passive_vars.kirby.copydonkey_charge_level = 0;
        fp->passive_vars.kirby.copycaptain_falcon_punch_unk = 0;
        fp->passive_vars.kirby.copypurin_unk = 0;
        fp->passive_vars.kirby.copylink_boomerang_gobj = NULL;

        if (desc->copy_kind == nFTKindKirby)
        {
            fp->passive_vars.kirby.is_ignore_losecopy = FALSE;
        }
        else fp->passive_vars.kirby.is_ignore_losecopy = TRUE;

        if (fp->fkind == nFTKindKirby)
        {
            FTKirbyCopy *copy = lbRelocGetFileData(FTKirbyCopy*, gFTDataKirbyMainMotion, &llKirbyMainMotionSpecialNFTKirbyCopy);

            /* The port's own: read from address 0, the table is the boot
             * ROM's bytes, and Kirby draws with no body on hardware */
            if (gFTDataKirbyMainMotion == NULL)
            {
                syDebugPrintf("ft : Kirby's copy table is not bound (scene %d)\n",
                              (int)gSCManagerSceneData.scene_curr);
            }
            ftParamSetModelPartDefaultID(fighter_gobj, FTKIRBY_COPY_MODELPARTS_JOINT, copy[fp->passive_vars.kirby.copy_id].copy_modelpart_id);
        }
        break;

    case nFTKindLink:
    case nFTKindNLink:
        fp->passive_vars.link.boomerang_gobj = NULL;

        ftParamSetModelPartDefaultID(fighter_gobj, 21, -1);
        ftParamSetModelPartDefaultID(fighter_gobj, 19, 0);
        break;

    case nFTKindPurin:
    case nFTKindNPurin:
        fp->passive_vars.purin.unk_0x0 = 0;
        break;

    /* ft/ftmanager.c:653-662, verbatim. Master Hand's
     * passive state, and the one line of it that matters most is the
     * first: passive_vars.boss.p points at passive_vars.boss.s, the
     * struct beside it. Everything of his that reads passive state --
     * ftbosswait.c's chooser, the target ftCommonAppearSetStatus finds
     * him, ftbosscommon.c's attack timer -- goes through that pointer,
     * so with the arm cut it was NULL and the first thing to touch it
     * would have written through NULL. */
    case nFTKindBoss:
        fp->passive_vars.boss.p = &fp->passive_vars.boss.s;
        fp->passive_vars.boss.p->wait_div = 1.0F;
        fp->passive_vars.boss.p->status_id = -1;
        fp->passive_vars.boss.p->status_id_random = -1;
        fp->passive_vars.boss.p->status_id_guard = 0;

        if (fp->pkind != nFTPlayerKindDemo)
        {
            ftBossCommonSetNextAttackWait(fighter_gobj);
            ftBossCommonSetDefaultLineID(fighter_gobj);
        }
        break;
    }
    ftParamClearAttackCollAll(fighter_gobj);
    ftParamSetHitStatusPartAll(fighter_gobj, nGMHitStatusNormal);
    ftParamResetFighterColAnim(fighter_gobj);
}

/* ft/ftmanager.c:671-910 ftManagerMakeFighter 0x800D7F3C. Verbatim but
 * for what is marked DIVERGES:
 *  - fp->data stays NULL: the fighter's files are the pack, and the
 *    fighter gets its own instance of it (fighter_clone) behind the TopN
 *    DObj's payload (dc_model_of), where the game gives each fighter its
 *    own parts under one shared FTData. fp->attr is the port's
 *    FTAttributes for the kind, filled from the pack.
 *  - the parts tree is built by dc_model_add_root/add_dobjs where the
 *    game calls lbCommonInitDObj3Transforms and
 *    lbCommonSetupFighterPartsDObjs, with the game's base: DObjDesc
 *    entry k is joints[nFTPartsJointCommonStart + k], and an entry the
 *    setup_parts mask leaves out gets no DObj and a NULL slot. Every joint then gets its FTParts as the game
 *    gives it (lines 778-812), except that parts->flags stays 0 -- the
 *    pack carries no FTCommonPart.flags, only ft/ftdisplaymain.c reads
 *    them -- and the costume accessory GObj (lines 790-806) is not made.
 *  - the shadow (905-908) is src/dc/ftshadow.c's,
 *    and ftComputerSetupAll src/dc/ftcomputer.c's, at
 *    the same call sites the decomp uses. */
GObj *ftManagerMakeFighter(FTDesc *desc)
{
    FTStruct *fp;
    GObj *fighter_gobj;
    s32 i;
    FTAttributes *attr;
    DObj *topn_joint;
    FTParts *parts;
    Fighter *pack;
    Fighter *model;
    float (*vclip)[4];

    pack = (desc->fkind >= 0 && desc->fkind < nFTKindEnumCount) ? gFTManagerModels[desc->fkind] : NULL;

    if (pack == NULL)
    {
        syDebugPrintf("ft : no pack loaded for fighter kind %d\n", desc->fkind);
        scManagerRunPrintGObjStatus();
    }
    if (pack->hd->joint_count > FTPARTS_JOINT_NUM_MAX - nFTPartsJointCommonStart)
    {
        syDebugPrintf("ft : %s has %d joints, max %d\n", pack->hd->name,
                      (s32)pack->hd->joint_count,
                      FTPARTS_JOINT_NUM_MAX - nFTPartsJointCommonStart);
        scManagerRunPrintGObjStatus();
    }
    fighter_gobj = gcMakeGObjSPAfter(nGCCommonKindFighter, NULL, nGCCommonLinkIDFighter, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(fighter_gobj, desc->proc_display, FTDISPLAY_DLLINK_DEFAULT, GOBJ_PRIORITY_DEFAULT, ~0);

    fp = ftManagerGetNextStructAlloc();

    fighter_gobj->user_data.p = fp;

    fp->pkind = desc->pkind;
    fp->fighter_gobj = fighter_gobj;
    fp->fkind = desc->fkind;

    /* this fighter's instance of the pack: its own pose and skeleton in
     * the instance the slot owns, its own clip buffer against the pack
     * it turns out to be (see the pool's block at the top of the file);
     * the pack's data shared */
    i = fp - sFTManagerStructsAllocBuf;
    model = &sFTManagerModelsAllocBuf[i];
    /* the slot's last fighter's, if it had one: a fighter that dies with
     * its scene never reaches ftManagerDestroyFighter */
    free(sFTManagerVClipOwned[i]);
    vclip = pack->hd->vert_count != 0
          ? malloc((size_t)pack->hd->vert_count * sizeof(*vclip)) : NULL;
    sFTManagerVClipOwned[i] = vclip;

    if (pack->hd->vert_count != 0 && vclip == NULL)
    {
        syDebugPrintf("ft : %s has no clip buffer, %u vertices\n",
                      pack->hd->name, (unsigned)pack->hd->vert_count);
        scManagerRunPrintGObjStatus();
    }
    fighter_clone(model, pack, vclip);

    fp->data = NULL;
    attr = fp->attr = ftManagerKindAttributes(fp->fkind);
    ftManagerSetupAttributes(attr, model->attr);
    ftManagerSetupShieldPose(attr, model);

    /* ft/fttypes.h:971 FTAttributes.sprites, which the game's attributes
     * carry in the main file: the kind's bank if
     * ftManagerSetupFilesAllKind loaded one for this scene */
    attr->sprites = sFTManagerSpriteBanks[fp->fkind].is_loaded ? &sFTManagerSprites[fp->fkind] : NULL;

    fp->figatree_heap = desc->figatree_heap;
    fp->team = desc->team;
    fp->player = desc->player;
    fp->stock_count = desc->stock_count;

    if (fp->pkind != nFTPlayerKindDemo)
    {
        gSCManagerBattleState->players[fp->player].stock_count = desc->stock_count;
    }
    fp->detail_curr = fp->detail_base = desc->detail;
    fp->costume = desc->costume;
    /* the costume's baked materials (fighter.h FPackCostumes), where the
     * game plays each MObj's costume script to this frame as it adds the
     * parts below */
    fighter_set_costume(model, fp->costume);
    fp->shade = desc->shade;
    fp->shade_color.r = (attr->shade_color[fp->shade - 1].r * attr->shade_color[fp->shade - 1].a) / 0xFF;
    fp->shade_color.g = (attr->shade_color[fp->shade - 1].g * attr->shade_color[fp->shade - 1].a) / 0xFF;
    fp->shade_color.b = (attr->shade_color[fp->shade - 1].b * attr->shade_color[fp->shade - 1].a) / 0xFF;
    fp->handicap = desc->handicap;
    fp->level = desc->level;
    fp->card_anim_frame_id = 0;
    fp->unk_ft_0x3C = 0;
    fp->anim_desc.word = 0;
    fp->p_sfx = NULL;
    fp->sfx_id = 0;
    fp->p_voice = NULL;
    fp->voice_id = 0;
    fp->p_loop_sfx = NULL;
    fp->loop_sfx_id = 0;
    fp->effect_joint_array_id = 0;
    fp->is_invisible = FALSE;
    fp->is_shadow_hide = FALSE;
    fp->display_mode = nDBDisplayModeMaster;
    fp->is_muted = FALSE;
    fp->is_events_forward = FALSE;
    fp->proc_status = NULL;
    fp->unk_ft_0x149 = desc->unk_rebirth_0x1C;
    fp->team_order = desc->team_order;
    fp->dl_link = FTDISPLAY_DLLINK_DEFAULT;
    fp->is_magnify_ignore = desc->is_magnify_ignore;
    fp->status_total_tics = 0;
    fp->camera_zoom_frame = attr->camera_zoom;
    fp->camera_zoom_range = 1.0F;
    fp->is_playertag_bossend = FALSE;
    fp->is_limit_map_bounds = FALSE;
    fp->is_have_translate_scale = (attr->translate_scales != NULL) ? TRUE : FALSE;

    for (i = 0; i < ARRAY_COUNT(fp->joints); i++)
    {
        fp->joints[i] = NULL;
    }
    /* ft/ftmanager.c:759-777: a TopN DObj straight on the GObj, then the
     * model's own tree under it from joints[CommonStart], the entries
     * the mask names. TopN carries the position, the facing and the
     * fighter's scale; nothing below it knows where on the stage it is.
     * Slots 1 to 3 stay NULL: TransN, XRotN and YRotN are hidden parts
     * the first status makes (ftcommon.c ftMainUpdateHiddenPartID). */
    topn_joint = dc_model_add_root(fighter_gobj, model);
    fp->joints[nFTPartsJointTopN] = topn_joint;

    dc_model_add_dobjs(fighter_gobj, topn_joint, model, &fp->joints[nFTPartsJointCommonStart]);

    /* ft/ftmanager.c:778-812. The accessory GObj is the game's, and
     * lbCommonAddMObjForFighterPartsDObj is handed the attributes' NULLs:
     * the accessory's display list and materials are baked into the pack,
     * and ftDisplayMainDrawAccessory turns them on for a joint that
     * carries this GObj. */
    for (i = 0; i < ARRAY_COUNT(fp->joints); i++)
    {
        if (fp->joints[i] != NULL)
        {
            fp->joints[i]->user_data.p = ftManagerGetNextPartsAlloc();

            parts = fp->joints[i]->user_data.p;
            parts->flags = 0;
            parts->joint_id = i;

            if (fp->costume != 0)
            {
                if ((attr->accesspart != NULL) && (i == attr->accesspart->joint_id))
                {
                    FTAccessPart *accesspart = attr->accesspart;

                    parts->gobj = gcMakeGObjSPAfter(nGCCommonKindFighterParts, NULL, nGCCommonLinkIDFighterParts, GOBJ_PRIORITY_DEFAULT);

                    gcAddDObjForGObj(parts->gobj, accesspart->dl);
                    lbCommonAddMObjForFighterPartsDObj(DObjGetStruct(parts->gobj), accesspart->mobjsubs, accesspart->costume_matanim_joints, NULL, fp->costume);
                }
            }
        }
    }
    /* ft/ftmanager.c:814-824. A DObj has a display payload wherever the
     * game's has a display list, so a tree entry with none -- the joints
     * whose model parts only come on when something sets one, Link's
     * sheathed sword -- reads -1 as the game's does (dc_model_add_dobjs). */
    for (i = nFTPartsJointCommonStart; i < ARRAY_COUNT(fp->joints); i++)
    {
        if (fp->joints[i] != NULL)
        {
            fp->modelpart_status[i - nFTPartsJointCommonStart].modelpart_id_base = 
            fp->modelpart_status[i - nFTPartsJointCommonStart].modelpart_id_curr = (fp->joints[i]->dl != NULL) ? 0 : -1;
        }
    }
    for (i = 0; i < ARRAY_COUNT(fp->texturepart_status); i++)
    {
        fp->texturepart_status[i].texture_id_base = fp->texturepart_status[i].texture_id_curr = 0;
    }
    ftParamSetAnimLocks(fp);

    fp->input.pl.stick_range.x = fp->input.pl.stick_range.y = fp->input.pl.stick_prev.x = fp->input.pl.stick_prev.y = fp->input.cp.stick_range.x = fp->input.cp.stick_range.y = 0;
    fp->input.pl.button_hold = fp->input.pl.button_tap = fp->input.cp.button_inputs = 0;

    fp->input.controller = desc->controller;

    fp->input.button_mask_a = desc->button_mask_a;
    fp->input.button_mask_b = desc->button_mask_b;
    fp->input.button_mask_z = desc->button_mask_z;
    fp->input.button_mask_l = desc->button_mask_l;

    fp->tap_stick_x = fp->tap_stick_y = fp->hold_stick_x = fp->hold_stick_y = FTINPUT_STICKBUFFER_TICS_MAX;

    /* ft/ftmanager.c:830-846: every hurtbox's
     * joint exists under the game's base, so no extra condition is needed
     * for Kirby's and Jigglypuff's hat hurtboxes. */
    for (i = 0; i < ARRAY_COUNT(fp->damage_colls); i++)
    {
        if (attr->damage_coll_descs[i].joint_id != -1)
        {
            fp->damage_colls[i].hitstatus = nGMHitStatusNormal;
            fp->damage_colls[i].joint_id = attr->damage_coll_descs[i].joint_id;
            fp->damage_colls[i].joint = fp->joints[fp->damage_colls[i].joint_id];
            fp->damage_colls[i].placement = attr->damage_coll_descs[i].placement;
            fp->damage_colls[i].is_grabbable = attr->damage_coll_descs[i].is_grabbable;
            fp->damage_colls[i].offset = attr->damage_coll_descs[i].offset;
            fp->damage_colls[i].size = attr->damage_coll_descs[i].size;

            fp->damage_colls[i].size.x *= 0.5F;
            fp->damage_colls[i].size.y *= 0.5F;
            fp->damage_colls[i].size.z *= 0.5F;
        }
        else fp->damage_colls[i].hitstatus = nGMHitStatusNone;
    }
    fp->coll_data.p_translate = &DObjGetStruct(fighter_gobj)->translate.vec.f;
    fp->coll_data.p_lr = &fp->lr;
    fp->coll_data.map_coll = attr->map_coll;
    fp->coll_data.p_map_coll = &fp->coll_data.map_coll;
    fp->coll_data.cliffcatch_coll = attr->cliffcatch_coll;
    fp->coll_data.ignore_line_id = -1;
    fp->coll_data.update_tic = gMPCollisionUpdateTic;
    fp->coll_data.mask_curr = 0;

    if (fp->pkind != nFTPlayerKindDemo)
    {
        gcAddGObjProcess(fighter_gobj, ftMainProcUpdateInterrupt, nGCProcessKindFunc, 5);
        gcAddGObjProcess(fighter_gobj, ftMainProcPhysicsMapDefault, nGCProcessKindFunc, 4);
        gcAddGObjProcess(fighter_gobj, ftMainProcPhysicsMapCapture, nGCProcessKindFunc, 3);
        gcAddGObjProcess(fighter_gobj, ftMainProcSearchCatch, nGCProcessKindFunc, 2);
        gcAddGObjProcess(fighter_gobj, ftMainProcSearchHitAll, nGCProcessKindFunc, 1);
        gcAddGObjProcess(fighter_gobj, ftMainProcParams, nGCProcessKindFunc, 0);
    }
    else gcAddGObjProcess(fighter_gobj, scSubsysFighterProcUpdate, nGCProcessKindFunc, 5);

    ftManagerInitFighter(fighter_gobj, desc);

    /* ft/ftmanager.c:869-872 -- src/dc/ftcomputer.c
     * now carries ftComputerSetupAll. coll_data.floor_line_id/floor_dist
     * above and joints[TopN] (dc_model_add_root, earlier in this
     * function) are both live by this point, which is what
     * ftComputerSetupAll reads. */
    if (fp->pkind == nFTPlayerKindCom)
    {
        ftComputerSetupAll(fighter_gobj);
    }
    if ((fp->pkind == nFTPlayerKindKey) || (fp->pkind == nFTPlayerKindGameKey))
    {
        fp->key.script = NULL;
        fp->key.input_wait = 0;
    }
    switch (fp->pkind)
    {
    case nFTPlayerKindDemo:
        scSubsysFighterSetStatus(fighter_gobj, nFTDemoStatusNull);
        break;

    case nFTPlayerKindKey:
        mpCommonSetFighterWaitOrFall(fighter_gobj);
        break;

    default:
        if (desc->is_skip_entry)
        {
            mpCommonSetFighterWaitOrFall(fighter_gobj);
            ftParamLockPlayerControl(fighter_gobj);
        }
        else
        {
            ftCommonEntrySetStatus(fighter_gobj);
            ftParamLockPlayerControl(fighter_gobj);
        }
        break;
    }
    /* ft/ftmanager.c:899-901 -- gated on Man or
     * Com (not unconditional): every human or CPU fighter gets a real
     * damage_coll_size, the box func_ovl3_80135B78's own incoming-attack
     * prediction sizes against. */
    if ((fp->pkind == nFTPlayerKindMan) || (fp->pkind == nFTPlayerKindCom))
    {
        ftComputerSetFighterDamageDetectSize(fighter_gobj);
    }
    /* ft/ftmanager.c:905-908, the last thing the game does with a new
     * fighter. */
    if ((fp->pkind != nFTPlayerKindDemo) && !(desc->is_skip_shadow_setup))
    {
        ftShadowMakeShadow(fighter_gobj);
    }
    return fighter_gobj;
}

/* The port's teleport (ftcommon.h): back to (x, y), stopped, settled
 * the way the spawn was. This is NOT the respawn -- the blast line goes
 * through ftCommonRebirthDownSetStatus and the Rebirth statuses. It is a debug and test
 * helper: src/dc/db.c's cliff warp, and the host tests that need
 * a fighter put somewhere without a KO in between. It exists because
 * moving a fighter by writing translate alone leaves coll_data's floor
 * line stale, which the real mpcollision.c then aborts on. */
void ftMainRespawn(GObj *fighter_gobj, float x, float y)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);

    DObjGetStruct(fighter_gobj)->translate.vec.f.x = x;
    DObjGetStruct(fighter_gobj)->translate.vec.f.y = y;
    DObjGetStruct(fighter_gobj)->translate.vec.f.z = 0.0F;
    fp->physics.vel_air.x = fp->physics.vel_air.y = fp->physics.vel_air.z = 0.0F;
    fp->physics.vel_ground.x = 0.0F;
    /* the knockback carries in its own vectors, which ftPhysicsStopVelAll
     * (ft/ftphysics.c) does not touch -- a teleport that left them set
     * would fly the fighter straight back out of the stage */
    fp->physics.vel_damage_air.x = fp->physics.vel_damage_air.y = fp->physics.vel_damage_air.z = 0.0F;
    fp->physics.vel_damage_ground = 0.0F;
    fp->is_fastfall = FALSE;
    fp->is_cliff_hold = FALSE;
    fp->cliffcatch_wait = 0;
    fp->jumps_used = 0;

    ftManagerProjectFighterFloor(fighter_gobj);
    mpCommonSetFighterWaitOrFall(fighter_gobj);
}

/* The bzero arm of syDmaLoadOverlay for overlay 2, of which ft/ftmanager
 * is a part (smashbrothers.us.yaml): src/dc/overlay.c calls it on the way
 * into every scene that loads that overlay. src/dc/overlay.h says why the
 * port needs it written out.
 *
 * gFTManagerModels is deliberately not here. On the N64 the model files
 * are dFTManagerDataFiles in ft/ftdata.c, which is overlay 2's *.data* --
 * restored from the ROM by the DMA, not bzeroed. The port's table is
 * filled per scene by ftManagerSetupFilesAllKind and emptied by
 * ftManagerReleaseFilesAll where the scene's files go, not here: a
 * caller that installed a pack by hand (the host test's mock) keeps it
 * across an overlay load, as the game's table keeps its file ids;
 * tools/check/overlay_check.py carries it in EXCLUDE with that reason.
 * Everything below it is per-scene: the pools and the attributes are
 * cut out of the scene heap, which taskman empties between scenes. */
void ftManagerOverlayLoad(void)
{
    OVERLAY_CLEAR(sFTManagerSkeletonNone);
    OVERLAY_CLEAR(sFTManagerSkeletonRows);
    OVERLAY_CLEAR(sFTManagerSkeletonRowsNum);
    OVERLAY_CLEAR(sFTManagerStructsAllocFree);
    OVERLAY_CLEAR(sFTManagerStructsAllocBuf);
    OVERLAY_CLEAR(sFTManagerPartsAllocFree);
    OVERLAY_CLEAR(sFTManagerPartsAllocBuf);
    /* not sFTManagerModelsAllocBuf or sFTManagerVClipOwned: the
     * buffers are main RAM, and the pointers that name them have to
     * survive this load, which comes first at a scene change
     * (scmanager.c's syDmaLoadOverlay, then the scene's heap reset and
     * its hook, ftManagerReleaseVClipAll). Clearing the instance buffer
     * here leaked one per fighter scene: thirteen by the end of the
     * attract loop's first pass, pinned across the heap until a 600 KB
     * pack no longer fit (2026-09-22). */
    OVERLAY_CLEAR(sFTManagerAttrs);
    OVERLAY_CLEAR(gFTManagerPlayersNum);
    OVERLAY_CLEAR(gFTManagerMotionCount);
    OVERLAY_CLEAR(gFTManagerStatUpdateCount);
    OVERLAY_CLEAR(sFTManagerSpriteBanks);
    OVERLAY_CLEAR(sFTManagerSprites);
    /* not sFTManagerLoadedByManager, nor
     * sFTManagerKeepKinds: both go with gFTManagerModels, which
     * ftManagerReleaseFilesAll clears between scenes and the overlay
     * load leaves alone (see above). The keep set in particular is
     * taken before the scene starts and has to outlive the overlay
     * load the scene's start runs, or a heap reset inside the scene
     * (sudden death) would throw away the packs the scene change kept. */
}
