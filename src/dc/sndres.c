/* sndres.c -- which samples are in sound RAM, scene by scene. See
 * sndres.h for why this exists at all; this file is the composition (what
 * a scene needs, target-independent, so the host test runs it) and the
 * upload (the trip to the AICA, target only). */
#ifdef _arch_dreamcast
#include <kos.h>
#include <dc/sound/sound.h>
#include <dc/spu.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sndres.h"
#include "loadcensus.h"
#include "assetroot.h"
#include "scmanager.h"
#include "scport.h"
#include "ftcommon.h"             /* ftManagerKindHasPack */
#include "sc1pgame.h"             /* dSC1PGameStageDesc, the rung table */
#ifdef _arch_dreamcast
#include "bgm.h"
#include "bgmbank.h"
#include "fgm.h"
#endif

#include <sc/scdef.h>
#include <ft/ftdef.h>
#include <gr/grdef.h>

#define SNDSETS_MAGIC "SSBSETS1"
#define FGMPAK_MAGIC "SSBSND2\0"

typedef struct
{
    char magic[8];
    uint32_t n_groups, budget;
} SndSetsHdr;

typedef struct
{
    uint32_t off;
    uint16_t n_fgm, n_bgm;
} SndSetsGroup;

/* tools/export/ssb_fgmexport.py's fgm_sounds.pak directory row. */
typedef struct
{
    uint32_t off, nsamples, rate;
    uint32_t loop_start, loop_end, loop_count;
    uint8_t bank, keybase, detune, rate_shift;
    uint16_t orig_index, pad2;
} SndResFgmRow;

_Static_assert(sizeof(SndSetsHdr) == 16, "sndsets header");
_Static_assert(sizeof(SndSetsGroup) == 8, "sndsets group");
_Static_assert(sizeof(SndResFgmRow) == 32, "fgm pack directory row");

static uint8_t *sSets;              /* sndsets.bin, whole */
static const SndSetsGroup *sGroups;
static uint32_t sBudget;
static SndResSet sResident;
static uint32_t sSwaps;

/* ---------------------------------------------------------------- *
 * Composition                                                       *
 * ---------------------------------------------------------------- */

const uint16_t *sndres_group_fgm(unsigned group, unsigned *count)
{
    if (sSets == NULL || group >= SNDRES_GROUPS)
    {
        *count = 0;
        return NULL;
    }
    *count = sGroups[group].n_fgm;
    return (const uint16_t *)(sSets + sGroups[group].off);
}

const uint16_t *sndres_group_bgm(unsigned group, unsigned *count)
{
    if (sSets == NULL || group >= SNDRES_GROUPS)
    {
        *count = 0;
        return NULL;
    }
    *count = sGroups[group].n_bgm;
    return (const uint16_t *)(sSets + sGroups[group].off +
                              2 * sGroups[group].n_fgm);
}

static void set_add_group(SndResSet *set, unsigned group)
{
    const uint16_t *list;
    unsigned i, n;

    list = sndres_group_fgm(group, &n);
    for (i = 0; i < n; i++)
    {
        if (list[i] < SNDRES_FGM_BITS)
            set->fgm[list[i] >> 5] |= 1u << (list[i] & 31);
    }
    list = sndres_group_bgm(group, &n);
    for (i = 0; i < n; i++)
    {
        if (list[i] < SNDRES_BGM_BITS)
            set->bgm[list[i] >> 5] |= 1u << (list[i] & 31);
    }
}

/* Whose voice group a fighter kind sounds out of. There are twelve
 * groups and they are the twelve VS fighters', because the sets
 * tools/export/ssb_sndsets.py measures are keyed on which decomp file names an
 * FGM id -- and a 1P kind names none of its own. Giant Donkey Kong's
 * FTAttributes (relocData 215 dGDonkeyMain_attr) carry Donkey Kong's
 * dead, damage, smash and heavy-get ids to the entry, so he IS Donkey
 * Kong's group, Metal Mario is Mario's the same way and
 * each Polygon is its base fighter's. Master Hand is the one
 * left, and he is none of them -- his are the boss stage's.
 *
 * A kind with no pack at all is the other arm: scManagerRunScene plays
 * it as Mario, so it sounds like him. */
static s32 sndres_voice_kind(s32 fkind)
{
    if (fkind == nFTKindGDonkey && ftManagerKindHasPack(nFTKindGDonkey))
        return nFTKindDonkey;
    if (fkind == nFTKindMMario && ftManagerKindHasPack(nFTKindMMario))
        return nFTKindMario;
    /* Each Polygon is its own fighter's voice: ft/ftdef.h lists the N
     * kinds in the playable kinds' own order, so the two ranges line up
     * entry for entry and nFTKindNStart is the whole of the mapping. */
    if (fkind >= nFTKindNStart && fkind <= nFTKindNEnd &&
        ftManagerKindHasPack(fkind))
        return nFTKindPlayableStart + (fkind - nFTKindNStart);
    if (fkind < 0 || fkind >= SNDRES_FIGHTERS || !ftManagerKindHasPack(fkind))
        return nFTKindMario;
    return fkind;
}

/* The fighters a battle or its results screen will have: every slot the
 * menus filled, each through the table above. */
static void set_add_players(SndResSet *set)
{
    s32 i;

    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        const SCPlayerData *pd = &gSCManagerTransferBattleState.players[i];
        s32 fkind = pd->fkind;

        if (pd->pkind == nFTPlayerKindNot || fkind == nFTKindNull)
            continue;
        set_add_group(set, SNDRES_GROUP_FIGHTER + sndres_voice_kind(fkind));
    }
}

int sndres_compose(s32 scene, SndResSet *set)
{
    s32 i;

    memset(set, 0, sizeof(*set));

    switch (scene)
    {
    /* XBOX: straight after the movie the title stops nothing
     * (mntitle.c mnTitleInitVars): the opening's music runs on into the
     * fanfare and the announcer's "Super Smash Bros." (Newcomers' one
     * long wave) plays over the logo. So the opening set stays wanted
     * beside the menu's -- tools/export/ssb_sndsets.py holds the two to
     * the budget as "title after movie" -- and sndres_enter_scene swaps
     * it in under the sound. */
    case nSCKindTitle:
        if (gSCManagerSceneData.scene_prev == nSCKindOpeningNewcomers)
            set_add_group(set, SNDRES_GROUP_OPENING);
        /* fall through */
    case nSCKindModeSelect:
    /* The 1P submenu is the mode select's other door and
     * plays the same music it does -- mn1PModeFuncStart starts
     * nSYAudioBGMModeSelect on any entry that did not come from the
     * mode select itself, which already had it playing. */
    case nSCKind1PMode:
    case nSCKindVSMode:
    case nSCKindVSOptions:
    case nSCKindVSItemSwitch:
    case nSCKindMaps:
    case nSCKindMessage:
    case nSCKindData:
    case nSCKindVSRecord:
    case nSCKindOption:
    case nSCKindScreenAdjust:
    case nSCKindBackupClear:
    /* The congratulations screen: one voice line
     * (nSYAudioVoiceAnnounceCongra or nSYAudioVoiceAnnounceIncredible),
     * no per-fighter sound of its own -- the same "menu" baseline every
     * screen around it gets. */
    case nSCKindCongra:
    /* Sound Test is not its own group: mnsoundtest.h's own DIVERGES note
     * says why (its ID tables reach nearly the whole tier-A corpus, far
     * past what 2 MB of sound RAM can hold at once). It gets the same
     * "menu" baseline as every screen around it; whatever the player
     * scrolls to that is outside that set logs "not resident" and does
     * not play, exactly as any other not-yet-staged sound would
     * (fgm-audio-census, memory). */
    case nSCKindSoundTest:
        set_add_group(set, SNDRES_GROUP_MENU);
        return 1;

    /* Any of the twelve can be picked, and the pick is announced and
     * posed the moment it happens. Character Data is the
     * same shape: any of the twelve can be paged to, and its demo
     * fighter plays that fighter's own motion-script voices the moment
     * the page turns, exactly as ftMainSetStatus does in a battle. */
    case nSCKindPlayersVS:
    /* Training mode's select is the VS select's twin and
     * announces a fighter the same way, so it takes the same set. */
    case nSCKindPlayers1PTraining:
    /* The 1P game's select is the same screen again: any
     * of the twelve can be picked, and the pick is announced. */
    case nSCKind1PGamePlayers:
    /* And the bonus stages' practice select, which is
     * that screen once more -- one scene for both bonus games, and
     * either one can be practised as any of the twelve. */
    case nSCKind1PBonus1Players:
    case nSCKind1PBonus2Players:
        set_add_group(set, SNDRES_GROUP_MENU);
        for (i = 0; i < SNDRES_FIGHTERS; i++)
            set_add_group(set, SNDRES_GROUP_FIGHTER + i);
        return 1;

    /* Character Data from the DATA menu is the shape above -- any of the
     * twelve can be paged to. The same scene in the attract loop
     * (mnTitleProceedDemoNext from How to Play, scene_prev
     * anything but nSCKindData) poses only the two demo fighters the
     * title picked (mncharacters.c's sMNCharactersIsDemo arm), under the
     * Explain track the title started three tics before the switch --
     * SNDRES_GROUP_ATTRACT, whose header note says why it is not on
     * MENU. Those first three tics play against the title's own set, so
     * the track's opening notes go quiet the way src/dc/aica/seqsyn.c leaves
     * a non-resident wave, and the rest lands once this set is staged. */
    case nSCKindCharacters:
        set_add_group(set, SNDRES_GROUP_MENU);
        if (gSCManagerSceneData.scene_prev == nSCKindData)
        {
            for (i = 0; i < SNDRES_FIGHTERS; i++)
                set_add_group(set, SNDRES_GROUP_FIGHTER + i);
        }
        else
        {
            set_add_group(set, SNDRES_GROUP_ATTRACT);
            for (i = 0; i < 2; i++)
            {
                if (gSCManagerSceneData.demo_fkind[i] < SNDRES_FIGHTERS)
                    set_add_group(set, SNDRES_GROUP_FIGHTER + gSCManagerSceneData.demo_fkind[i]);
            }
        }
        return 1;

    case nSCKindVSBattle:
        set_add_group(set, SNDRES_GROUP_CORE);
        set_add_players(set);
        if (gSCManagerSceneData.gkind < SNDRES_STAGES)
            set_add_group(set, SNDRES_GROUP_STAGE + gSCManagerSceneData.gkind);
        return 1;

    /* How to Play: a real battle, not a menu, so it needs
     * SNDRES_GROUP_CORE like nSCKindVSBattle above -- but it is not
     * reached through the normal VS setup flow set_add_players reads
     * (gSCManagerTransferBattleState), since scExplainSetBattleState
     * hardcodes its own two players directly, and it never has a real
     * gGCManagerSceneData.gkind to read either, since nGRKindExplain has
     * no exported pack and scExplainFuncStart always falls back to
     * Hyrule instead (src/dc/scexplain.h's own header note). Both
     * fighters and the stage are named directly here for exactly that
     * reason -- this scene's own composition can never vary. Without
     * this case, sndres_compose's
     * default (0, "no change") left whichever scene was resident before
     * the direct DB_BOOT_SCENE jump staged, and Mario/Luigi's hit and
     * voice sounds and Hyrule's own BGM wave all logged "not resident". */
    case nSCKindExplain:
        set_add_group(set, SNDRES_GROUP_CORE);
        set_add_group(set, SNDRES_GROUP_FIGHTER + nFTKindMario);
        set_add_group(set, SNDRES_GROUP_FIGHTER + nFTKindLuigi);
        set_add_group(set, SNDRES_GROUP_STAGE + nGRKindHyrule);
        return 1;

    /* The ladder's VS card. NOT a battle, and that is the
     * point: no items, no hits, no stage -- the two or four fighters
     * stand still in their card poses while the announcer names them.
     * So the card takes its own group and the fighters' own, and NOT
     * SNDRES_GROUP_CORE, which is the battle's hit and item corpus.
     *
     * That is a budget decision as well as a correctness one, and
     * tools/export/ssb_sndsets.py --check is what made it: with CORE the
     * card's worst composition (four fighters, the 22 announcer voices
     * of its own group) came to 1918 KB against a 1835 KB budget, 126
     * KB over. Without it there is room to spare. The announcer lines
     * the card actually plays are its own file's, so they are in
     * SNDRES_GROUP_1PINTRO (src/dc/sndres.h says why they are not on
     * CORE either).
     *
     * Which fighters is sc1PIntroSetupFighterFiles' question and this
     * answers it the same way: the human's, the rung's opponent, and on
     * the two rungs that have them the allies'. A rung whose opponent
     * the port has no fighter data for (Metal Mario, the Polygon team,
     * Master Hand) names nothing for it, which is right -- there is no
     * pack to make a sound. */
    /* A rung of the ladder. It is a real battle, so
     * SNDRES_GROUP_CORE and the stage, like nSCKindVSBattle above --
     * but its players are not in gSCManagerTransferBattleState either,
     * so set_add_players has nothing to say. sc1PGameSetupStageAll
     * fills gSCManager1PGameBattleState from dSC1PGameStageDesc AFTER
     * this runs, so the rung's cast is named from the same table the
     * scene is about to read: the human out of the select's battle
     * state, the opponents out of the row's own fkind[2], the allies
     * out of the ports the router picked. The team rungs stand one kind
     * many times over and cost no more than the one.
     *
     * The three bonus rungs get CORE and their stage and nothing else:
     * the row's fkind[] is empty because the player fights the stage.
     *
     * Five of the rungs stand on a 1P-only map -- Small Yoshi's Island,
     * Meta Crystal, Duel Zone, Race to the Finish, Final Destination.
     * All five are below SNDRES_STAGES and each has a stage group, so
     * their music is staged like any VS stage's. Until the four maps
     * besides Final Destination were exported those groups were empty,
     * and the Yoshi Team played with "bgm: wave N is not resident".
     * Final Destination's own track is in SNDRES_GROUP_1PGAME as well,
     * because this scene names it directly (sndres.h says why). */
    case nSCKind1PGame:
        {
            s32 stage = gSCManagerSceneData.spgame_stage;
            const SC1PGameStage *row;
            s32 want[4];
            s32 n = 0;

            set_add_group(set, SNDRES_GROUP_CORE);
            set_add_group(set, SNDRES_GROUP_1PGAME);

            if (stage < 0 || stage > nSC1PGameStageChallengerEnd)
                return 1;

            row = &dSC1PGameStageDesc[stage];
            if (row->gkind < SNDRES_STAGES)
                set_add_group(set, SNDRES_GROUP_STAGE + row->gkind);

            want[n++] = gSCManager1PGameBattleState.players[gSCManagerSceneData.player].fkind;
            for (i = 0; i < 2 && n < (s32)ARRAY_COUNT(want); i++)
            {
                if (row->fkind[i] != nFTKindNull)
                    want[n++] = row->fkind[i];
            }
            for (i = 0; i < row->ally_count && n < (s32)ARRAY_COUNT(want); i++)
            {
                want[n++] = gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[i]].fkind;
            }
            for (i = 0; i < n; i++)
            {
                if (want[i] == nFTKindNull)
                    continue;
                set_add_group(set,
                              SNDRES_GROUP_FIGHTER + sndres_voice_kind(want[i]));
            }
        }
        return 1;

    /* "CHALLENGER APPROACHING!": one track and one
     * fighter, the one the card is about. No CORE and no stage -- the
     * card has neither (src/dc/sndres.h's own note on
     * SNDRES_GROUP_1PCHALLENGER says why, and why the star stinger it
     * plays is in that group rather than borrowed from CORE). */
    /* The score screen after a rung: its own group and
     * nothing else. No CORE, no stage and no fighter -- the screen is
     * sprites from top to bottom (src/dc/sc1pstageclear.h). */
    case nSCKind1PScoreUnk:
    case nSCKind1PStageClear:
        set_add_group(set, SNDRES_GROUP_1PSTAGECLEAR);
        return 1;

    /* The "CONTINUE?" screen: its own group, plus the one
     * fighter it stands -- the human's, who is standing in the
     * spotlight. Its two BGM tracks and both announcer words are this
     * group's; no CORE and no stage. */
    case nSCKind1PContinue:
        {
            s32 fkind =
                gSCManager1PGameBattleState.players[gSCManagerSceneData.player].fkind;

            set_add_group(set, SNDRES_GROUP_1PCONTINUE);

            if (fkind >= 0 && fkind < SNDRES_FIGHTERS &&
                ftManagerKindHasPack(fkind))
            {
                set_add_group(set, SNDRES_GROUP_FIGHTER + fkind);
            }
        }
        return 1;

    case nSCKind1PChallenger:
        {
            s32 fkind = gSCManagerSceneData.challenger_fkind;

            set_add_group(set, SNDRES_GROUP_1PCHALLENGER);

            if (fkind >= 0 && fkind < SNDRES_FIGHTERS &&
                ftManagerKindHasPack(fkind))
            {
                set_add_group(set, SNDRES_GROUP_FIGHTER + fkind);
            }
        }
        return 1;

    case nSCKind1PIntro:
        {
            /* sc1pintro.c:1874-1889 dSC1PIntroFighterFkinds, in
             * nSC1PGameStage order; the four zeros are the three bonus
             * rungs and the gap after them, which stand no opponent. */
            static const s32 opponents[] =
            {
                nFTKindLink,  nFTKindYoshi, nFTKindFox,     -1,
                nFTKindMario, nFTKindPikachu, nFTKindDonkey, -1,
                nFTKindKirby, nFTKindSamus, -1,             -1,
                -1,           -1
            };
            s32 want[4];
            s32 n = 0;
            s32 stage = gSCManagerSceneData.spgame_stage;

            set_add_group(set, SNDRES_GROUP_1PINTRO);

            want[n++] = gSCManager1PGameBattleState.players[gSCManagerSceneData.player].fkind;

            if (stage >= 0 && stage < (s32)ARRAY_COUNT(opponents))
            {
                want[n++] = opponents[stage];
            }
            /* the Mario Bros. rung has one ally, Giant DK's has two */
            if (stage == nSC1PGameStageMario || stage == nSC1PGameStageDonkey)
            {
                want[n++] = gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[0]].fkind;
            }
            if (stage == nSC1PGameStageDonkey)
            {
                want[n++] = gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[1]].fkind;
            }
            for (i = 0; i < n; i++)
            {
                if (want[i] < 0 || want[i] >= SNDRES_FIGHTERS ||
                    !ftManagerKindHasPack(want[i]))
                    continue;
                set_add_group(set, SNDRES_GROUP_FIGHTER + want[i]);
            }
            /* The Mario Bros. rung stands Luigi beside Mario and the
             * table above names only the one the rung is called after. */
            if (stage == nSC1PGameStageMario && ftManagerKindHasPack(nFTKindLuigi))
            {
                set_add_group(set, SNDRES_GROUP_FIGHTER + nFTKindLuigi);
            }
        }
        return 1;

    /* A bonus stage: a real battle with one fighter, so
     * SNDRES_GROUP_CORE, the course's own stage group and this scene's
     * own -- the target's break sound and the announcer's COMPLETE! and
     * NEW RECORD lines are all in SNDRES_GROUP_1PBONUS.
     *
     * Like training mode below, this scene builds its OWN SCBattleState
     * (sc1PBonusStageInitVars) rather than reading
     * gSCManagerTransferBattleState, so set_add_players has nothing to
     * say and the one fighter is named from gSCManagerSceneData the way
     * InitVars names it: the ladder's `fkind` if the ladder sent us,
     * the practice select's `bonus_fkind` otherwise. That fighter's
     * index is also the COURSE, which is why the stage group falls out
     * of the same number. */
    case nSCKind1PBonusStage:
        {
            s32 fkind = (gSCManagerSceneData.scene_prev == nSCKind1PGame)
                            ? gSCManagerSceneData.fkind
                            : gSCManagerSceneData.bonus_fkind;
            s32 is_bonus1 =
                (gSCManagerSceneData.scene_prev == nSCKind1PGame)
                    ? (gSCManagerSceneData.spgame_stage == nSC1PGameStageBonus1)
                    : (gSCManagerSceneData.scene_prev == nSCKind1PBonus1Players);
            s32 gkind;

            if (fkind < 0 || fkind >= SNDRES_FIGHTERS ||
                !ftManagerKindHasPack(fkind))
                fkind = nFTKindMario;

            gkind = fkind + (is_bonus1 ? nGRKindBonus1Start : nGRKindBonus2Start);

            set_add_group(set, SNDRES_GROUP_CORE);
            set_add_group(set, SNDRES_GROUP_1PBONUS);
            set_add_group(set, SNDRES_GROUP_FIGHTER + fkind);

            if (gkind < SNDRES_STAGES)
                set_add_group(set, SNDRES_GROUP_STAGE + gkind);
        }
        return 1;

    /* Training mode: a real battle, so SNDRES_GROUP_CORE
     * and the stage -- but its two fighters are NOT in
     * gSCManagerTransferBattleState, which is what set_add_players
     * reads. sc1PTrainingModeInitVars builds its own SCBattleState out
     * of gSCManagerSceneData.training_man_fkind and _com_fkind, the
     * pair the select above it wrote, so they are named from there.
     * The scExplain case a few lines up is the same trap and was found
     * the same way: a battle scene that fills its own battle state
     * leaves set_add_players with nothing to say. */
    case nSCKind1PTrainingMode:
        {
            s32 fkinds[2];

            fkinds[0] = gSCManagerSceneData.training_man_fkind;
            fkinds[1] = gSCManagerSceneData.training_com_fkind;

            set_add_group(set, SNDRES_GROUP_CORE);

            for (i = 0; i < 2; i++)
            {
                s32 fkind = fkinds[i];

                if (fkind < 0 || fkind >= SNDRES_FIGHTERS ||
                    !ftManagerKindHasPack(fkind))
                    fkind = nFTKindMario;
                set_add_group(set, SNDRES_GROUP_FIGHTER + fkind);
            }
            if (gSCManagerSceneData.gkind < SNDRES_STAGES)
                set_add_group(set, SNDRES_GROUP_STAGE + gSCManagerSceneData.gkind);
        }
        return 1;

    /* The announcer names the winner and the fighters pose on the podium
     * with their motion scripts' voices; which win fanfare plays is the
     * results screen's to decide, so all of them come. */
    case nSCKindVSResults:
        set_add_group(set, SNDRES_GROUP_MENU);
        set_add_group(set, SNDRES_GROUP_RESULTS);
        set_add_players(set);
        return 1;

    /* The ending diorama: one posed fighter (the run's own,
     * mvEndingInitVars) and the door-close
     * stinger, nSYAudioFGMDoorClose. SNDRES_GROUP_CREDITS, not MENU --
     * sndres.h's own header note on that group says why (the two
     * scenes' own BGM is real weight, and MENU already has to fit
     * alongside every fighter at once for the character select screen).
     * No battle, so no SNDRES_GROUP_CORE either. */
    case nSCKindEnding:
        {
            /* the run's own fighter, the one mvEndingInitVars poses */
            s32 fkind =
                gSCManager1PGameBattleState.players[gSCManagerSceneData.player].fkind;

            set_add_group(set, SNDRES_GROUP_CREDITS);
            if (fkind >= 0 && fkind < SNDRES_FIGHTERS)
                set_add_group(set, SNDRES_GROUP_FIGHTER + fkind);
        }
        return 1;

    /* The staff roll: no fighters, no stage, just the
     * name-shooting "hit" cue (nSYAudioFGMTrainingSel) and the credits
     * theme -- SNDRES_GROUP_CREDITS the same way. */
    case nSCKindStaffroll:
        set_add_group(set, SNDRES_GROUP_CREDITS);
        return 1;

    /* The openings' first scene: one BGM track,
     * nSYAudioBGMOpening, and nothing else. The two trophy fighters are
     * drawn at random from the eight originals, but they never make a
     * sound -- each is posed with a demo status the pack has no
     * SubMotion for, so scSubsysFighterSetStatus turns it into a silent
     * nFTCommonStatusWait (src/dc/mvopeningroom.h) -- so no
     * SNDRES_GROUP_FIGHTER is composed for them; staging all eight on
     * the chance would cost ~1.2 MB for sounds that cannot play. No
     * battle either, so no SNDRES_GROUP_CORE. */
    case nSCKindOpeningRoom:
        set_add_group(set, SNDRES_GROUP_OPENING);
        return 1;

    /* The memory card's page (src/dc/dcmemcard.h): no change, on
     * purpose. From Options the menu set is already resident and the
     * page's sounds are all in it (tools/export/ssb_sndsets.py holds
     * dcmemcard.c to the menu group); before the first scene, or between
     * two of the game's, it is a pause that must not cost the next scene
     * an upload of its own. */
    case nSCKindDCMemCard:
        return 0;

    default:
        return 0;
    }
}

static int set_is_subset(const SndResSet *a, const SndResSet *b)
{
    unsigned i;

    for (i = 0; i < SNDRES_FGM_BITS / 32; i++)
    {
        if (a->fgm[i] & ~b->fgm[i])
            return 0;
    }
    for (i = 0; i < SNDRES_BGM_BITS / 32; i++)
    {
        if (a->bgm[i] & ~b->bgm[i])
            return 0;
    }
    return 1;
}

static int set_has(const uint32_t *bits, unsigned i)
{
    return (bits[i >> 5] >> (i & 31)) & 1;
}

uint32_t sndres_swaps(void)
{
    return sSwaps;
}

/* of the one sound RAM block, what the set filled (upload below) */
static uint32_t sResidentBytes;

uint32_t sndres_resident_bytes(void)
{
    return sResidentBytes;
}

int sndres_fgm_resident(unsigned bank_index)
{
    return bank_index < SNDRES_FGM_BITS && set_has(sResident.fgm, bank_index);
}

int sndres_bgm_resident(unsigned wave)
{
    return wave < SNDRES_BGM_BITS && set_has(sResident.bgm, wave);
}

/* ---------------------------------------------------------------- *
 * The upload                                                        *
 * ---------------------------------------------------------------- */

#ifdef _arch_dreamcast

static SndResFgmRow *sFgmDir;
static int16_t sFgmRowOf[SNDRES_FGM_BITS];
static uint32_t sBlock;             /* the one sound RAM allocation */
/* Sound RAM free at sndres_init, before this file allocated anything.
 * The log reports against this rather than asking snd_mem_available
 * again: KOS's answer is the largest block on its pool list, allocated
 * or not, so once the set's block is the largest it reports the set's
 * own size as "available". */
static uint32_t sBootFree;

#define READ_BUF 65536              /* > the largest sample, 32 KB */
#define SKIP_MAX 65536              /* read through a gap this small */

/* Reading samples one at a time off the disc, forward. A seek aborts the
 * GD-ROM's DMA stream (src/dc/assetroot.h), so a gap to the next wanted
 * sample is read through rather than sought over when it is small, and
 * every read starts on a 32-byte boundary. The two packs are laid out
 * together (tools/export/disc_layout.py), so a swap is two short forward
 * walks. */
typedef struct
{
    AssetFile af;
    long pos;
    int open;
    int warned;                     /* a short read has been logged */
} Reader;

static uint8_t *sReadBuf;

static int reader_open(Reader *r, const char *name)
{
    r->pos = 0;
    r->warned = 0;
    r->open = (asset_open(&r->af, name) == 0);
    return r->open ? 0 : -1;
}

static void reader_close(Reader *r)
{
    if (r->open)
        asset_close(&r->af);
    r->open = 0;
}

/* `n` bytes from `off`, at the start of sReadBuf, zero-padded out to the
 * next 32-byte line -- which is what spu_memload_sq moves. */
static const uint8_t *reader_get(Reader *r, uint32_t off, uint32_t n)
{
    uint32_t start = off & ~31u;
    uint32_t len = ((off + n + 31) & ~31u) - start;
    long got;

    if (len > READ_BUF)
        return NULL;
    if ((long)start < r->pos || (long)start - r->pos > SKIP_MAX)
    {
        asset_seek(&r->af, (long)start);
        r->pos = (long)start;
    }
    while (r->pos < (long)start)
    {
        got = asset_read(&r->af, sReadBuf, (long)start - r->pos);
        if (got <= 0)
            return NULL;
        r->pos += got;
    }
    got = asset_read(&r->af, sReadBuf, (long)len);
    if (got < 0)
        got = 0;
    /* Short at EOF is only the 32-byte rounding (the sample's and this
     * line's) running past the file's end, which the zero pad below is
     * for: asset_read clamps to EOF, so there the position IS the size.
     * Short anywhere else is a read that failed, and the sample would
     * upload as silence. */
    if ((uint32_t)got < (off - start) + n && r->af.pos < r->af.size &&
        !r->warned)
    {
        r->warned = 1;
        dbglog(DBG_ERROR, "sndres: %s: short read at %u -- %ld of %u "
               "bytes; the rest uploads as silence\n", r->af.path,
               (unsigned)start, got, (unsigned)((off - start) + n));
    }
    r->pos += got;
    if ((uint32_t)got < len)
        memset(sReadBuf + got, 0, len - (uint32_t)got);
    if (off != start)
    {
        memmove(sReadBuf, sReadBuf + (off - start), n);
        memset(sReadBuf + n, 0, len - n);
    }
    return sReadBuf;
}

static uint32_t fgm_bytes(const SndResFgmRow *row)
{
    return ((row->nsamples + 1) / 2 + 31) & ~31u;
}

static uint32_t bgm_bytes(uint32_t wave)
{
    return ((gBGMBank.waves[wave].nsamples + 1) / 2 + 31) & ~31u;
}

static const char *scene_name(s32 scene)
{
    switch (scene)
    {
    case nSCKindTitle:        return "title";
    case nSCKindModeSelect:   return "modeselect";
    case nSCKind1PMode:       return "1pmode";
    case nSCKind1PGamePlayers: return "1pgameselect";
    case nSCKindPlayers1PTraining: return "1ptraining";
    case nSCKind1PBonus1Players: return "1pbonus1select";
    case nSCKind1PBonus2Players: return "1pbonus2select";
    case nSCKind1PTrainingMode: return "trainingmode";
    case nSCKind1PChallenger: return "1pchallenger";
    case nSCKind1PContinue:   return "1pcontinue";
    case nSCKind1PScoreUnk:
    case nSCKind1PStageClear: return "1pstageclear";
    case nSCKind1PIntro:      return "1pintro";
    case nSCKind1PGame:       return "1pgame";
    case nSCKind1PBonusStage: return "1pbonus";
    case nSCKindVSMode:       return "vsmode";
    case nSCKindVSOptions:    return "vsoptions";
    case nSCKindVSItemSwitch: return "itemswitch";
    case nSCKindMaps:         return "maps";
    case nSCKindMessage:      return "message";
    case nSCKindData:         return "data";
    case nSCKindVSRecord:     return "vsrecord";
    case nSCKindCharacters:   return "characters";
    case nSCKindSoundTest:    return "soundtest";
    case nSCKindOption:       return "option";
    case nSCKindScreenAdjust: return "screenadjust";
    case nSCKindBackupClear:  return "backupclear";
    case nSCKindDCMemCard:    return "dcmemcard";
    case nSCKindCongra:       return "congra";
    case nSCKindPlayersVS:    return "playersvs";
    case nSCKindVSBattle:     return "vsbattle";
    case nSCKindVSResults:    return "vsresults";
    case nSCKindEnding:       return "ending";
    case nSCKindStaffroll:    return "staffroll";
    case nSCKindOpeningRoom:  return "openingroom";
    case nSCKindAutoDemo:     return "autodemo";
    default:                  return "?";
    }
}

/* Sound RAM as one arena, the sets' budget in size, allocated once and
 * kept. Each resident sample has a
 * place in it, and a swap reads only the samples the new set wants that
 * the arena does not already hold. The load census found every 1P ladder
 * hop -- card, rung, Stage Clear, card -- re-reading 0.8 to 2 MB of the
 * two packs, most of it samples the scene before had just thrown away.
 *
 * A sample the new set does not want stays where it is, and plays if
 * asked, until its room is needed. The first placement tries the free
 * space; the second evicts everything unwanted and tries again; the
 * third starts the arena from empty and places the whole set end to end,
 * which is what every swap did before and always fits, because the build
 * holds every scene's set to the budget (tools/export/ssb_sndsets.py). */
#define ARENA_RUNS 512

typedef struct { uint32_t off, len; } ArenaRun;

static ArenaRun sFree[ARENA_RUNS];
static int sFreeN;
static uint32_t sArena;                     /* the budget */
static uint32_t sFgmAt[SNDRES_FGM_BITS];    /* each sample's place */
static uint32_t sBgmAt[SNDRES_BGM_BITS];

static void arena_clear(void)
{
    sFree[0].off = 0;
    sFree[0].len = sArena;
    sFreeN = 1;
}

/* first fit; ~0u when nothing fits */
static uint32_t arena_take(uint32_t n)
{
    int i;

    for (i = 0; i < sFreeN; i++)
    {
        if (sFree[i].len >= n)
        {
            uint32_t at = sFree[i].off;

            sFree[i].off += n;
            sFree[i].len -= n;
            if (sFree[i].len == 0)
            {
                memmove(&sFree[i], &sFree[i + 1],
                        (size_t)(sFreeN - i - 1) * sizeof(ArenaRun));
                sFreeN--;
            }
            return at;
        }
    }
    return ~0u;
}

/* a run back to the free list, which stays sorted and merged */
static void arena_give(uint32_t off, uint32_t n)
{
    int i = 0;

    while (i < sFreeN && sFree[i].off < off)
        i++;
    if (i > 0 && sFree[i - 1].off + sFree[i - 1].len == off)
    {
        sFree[i - 1].len += n;
        if (i < sFreeN && sFree[i - 1].off + sFree[i - 1].len == sFree[i].off)
        {
            sFree[i - 1].len += sFree[i].len;
            memmove(&sFree[i], &sFree[i + 1],
                    (size_t)(sFreeN - i - 1) * sizeof(ArenaRun));
            sFreeN--;
        }
        return;
    }
    if (i < sFreeN && off + n == sFree[i].off)
    {
        sFree[i].off = off;
        sFree[i].len += n;
        return;
    }
    if (sFreeN == ARENA_RUNS)
        return;             /* lost until the next arena_clear */
    memmove(&sFree[i + 1], &sFree[i], (size_t)(sFreeN - i) * sizeof(ArenaRun));
    sFree[i].off = off;
    sFree[i].len = n;
    sFreeN++;
}

static void evict_fgm(unsigned i)
{
    arena_give(sFgmAt[i], fgm_bytes(&sFgmDir[sFgmRowOf[i]]));
    sResident.fgm[i >> 5] &= ~(1u << (i & 31));
}

static void evict_bgm(unsigned i)
{
    arena_give(sBgmAt[i], bgm_bytes(i));
    sResident.bgm[i >> 5] &= ~(1u << (i & 31));
    gBGMBank.wave_sram[i] = 0;
}

/* Place every wanted sample that is not resident; `load` marks the ones
 * placed, which the reads below then fill. 0 when one would not fit, the
 * places taken so far left for unplace. */
static int place(const SndResSet *want, SndResSet *load)
{
    unsigned i;

    memset(load, 0, sizeof(*load));
    for (i = 0; i < SNDRES_FGM_BITS; i++)
    {
        if (!set_has(want->fgm, i) || sFgmRowOf[i] < 0 ||
            set_has(sResident.fgm, i))
            continue;
        if ((sFgmAt[i] = arena_take(fgm_bytes(&sFgmDir[sFgmRowOf[i]]))) == ~0u)
            return 0;
        load->fgm[i >> 5] |= 1u << (i & 31);
        sResident.fgm[i >> 5] |= 1u << (i & 31);
    }
    for (i = 0; i < gBGMBank.n_waves && i < SNDRES_BGM_BITS; i++)
    {
        if (!set_has(want->bgm, i) || set_has(sResident.bgm, i))
            continue;
        if ((sBgmAt[i] = arena_take(bgm_bytes(i))) == ~0u)
            return 0;
        load->bgm[i >> 5] |= 1u << (i & 31);
        sResident.bgm[i >> 5] |= 1u << (i & 31);
    }
    return 1;
}

static void unplace(const SndResSet *load)
{
    unsigned i;

    for (i = 0; i < SNDRES_FGM_BITS; i++)
        if (set_has(load->fgm, i) && set_has(sResident.fgm, i))
            evict_fgm(i);
    for (i = 0; i < gBGMBank.n_waves && i < SNDRES_BGM_BITS; i++)
        if (set_has(load->bgm, i) && set_has(sResident.bgm, i))
            evict_bgm(i);
}

static uint32_t resident_bytes(void)
{
    uint32_t b = 0;
    unsigned i;

    for (i = 0; i < SNDRES_FGM_BITS; i++)
        if (set_has(sResident.fgm, i))
            b += fgm_bytes(&sFgmDir[sFgmRowOf[i]]);
    for (i = 0; i < gBGMBank.n_waves && i < SNDRES_BGM_BITS; i++)
        if (set_has(sResident.bgm, i))
            b += bgm_bytes(i);
    return b;
}

static void upload(s32 scene, const SndResSet *want, int carry)
{
    uint64_t t0 = timer_ms_gettime64();
    uint32_t fgm_n = 0, fgm_b = 0, bgm_n = 0, bgm_b = 0;
    const char *how = "added";
    SndResSet load;
    unsigned i;
    Reader r;
    int live = 0;

    /* XBOX: a scene that carries the last one's sound (sndres_enter_scene)
     * swaps without stopping it, when it can: if every new sample fits in
     * the arena's free space, nothing resident moves and nothing is
     * written under a sample a channel could be playing -- after any
     * swap only the resident set is registered, so only it can play.
     * place() is bookkeeping only, so it can be tried before deciding.
     * The eviction step is safe here too: what is still sounding is the
     * last scene's, which `want` carries, so no channel reads a sample
     * evicted for room. Only the repack moves what is resident; when it
     * comes to that, the swap is the usual stopping one below. */
    if (carry && sBlock)
    {
        live = place(want, &load);
        if (!live)
        {
            unplace(&load);
            for (i = 0; i < SNDRES_FGM_BITS; i++)
                if (set_has(sResident.fgm, i) && !set_has(want->fgm, i))
                    evict_fgm(i);
            for (i = 0; i < gBGMBank.n_waves && i < SNDRES_BGM_BITS; i++)
                if (set_has(sResident.bgm, i) && !set_has(want->bgm, i))
                    evict_bgm(i);
            live = place(want, &load);
            if (!live)
                unplace(&load);
        }
        if (live)
            how = "added under the sound";
        else
            dbglog(DBG_INFO, "sndres: %s: no room to swap under the sound "
                   "without a repack; stopping it\n", scene_name(scene));
    }

    /* Hold both engines still and silent. fgm_suspend forgets every
     * sound, so the whole resident set is registered again below, kept
     * samples and read ones alike. The stops are commands on the AICA's
     * queue, so give the ARM a moment to act on them before the memory
     * under a channel that was still playing is overwritten. */
    if (!live)
    {
        bgm_suspend();
        fgm_suspend();
        thd_sleep(30);
    }

    if (!sBlock)
    {
        sArena = sBudget;
        sBlock = snd_mem_malloc(sArena);
        memset(&sResident, 0, sizeof(sResident));
        if (gBGMBank.wave_sram != NULL)
            memset(gBGMBank.wave_sram, 0, gBGMBank.n_waves * sizeof(uint32_t));
        arena_clear();
        if (!sBlock)
        {
            dbglog(DBG_ERROR, "sndres: %s: no %u bytes of sound RAM for the "
                   "arena (largest free block %u) -- silent\n",
                   scene_name(scene), (unsigned)sArena,
                   (unsigned)snd_mem_available());
            goto out;
        }
    }
#ifdef DB_SNDRES_WHOLE
    /* -DDB_SNDRES_WHOLE (src/dc/db.h): every swap from empty, the way
     * they all were before the arena -- the A side of an A/B (XBOX: not
     * under the sound, which keeps what is resident by definition) */
    if (!live)
    {
        memset(&sResident, 0, sizeof(sResident));
        if (gBGMBank.wave_sram != NULL)
            memset(gBGMBank.wave_sram, 0, gBGMBank.n_waves * sizeof(uint32_t));
        arena_clear();
    }
#endif
    if (!live && !place(want, &load))
    {
        /* no room beside what is resident: give back what this set does
         * not want, and try again */
        unplace(&load);
        for (i = 0; i < SNDRES_FGM_BITS; i++)
            if (set_has(sResident.fgm, i) && !set_has(want->fgm, i))
                evict_fgm(i);
        for (i = 0; i < gBGMBank.n_waves && i < SNDRES_BGM_BITS; i++)
            if (set_has(sResident.bgm, i) && !set_has(want->bgm, i))
                evict_bgm(i);
        how = "evicted";
        if (!place(want, &load))
        {
            /* fragmented: from empty, which always fits */
            memset(&sResident, 0, sizeof(sResident));
            if (gBGMBank.wave_sram != NULL)
                memset(gBGMBank.wave_sram, 0,
                       gBGMBank.n_waves * sizeof(uint32_t));
            arena_clear();
            how = "repacked";
            if (!place(want, &load))
            {
                dbglog(DBG_ERROR, "sndres: %s's set is over the %u KB "
                       "arena -- the scene is silent\n", scene_name(scene),
                       (unsigned)(sArena / 1024));
                unplace(&load);
                goto out;
            }
        }
    }

    if (reader_open(&r, "fgm_sounds.pak") == 0)
    {
        for (i = 0; i < SNDRES_FGM_BITS; i++)
        {
            const uint8_t *data;
            uint32_t n;

            if (!set_has(load.fgm, i))
                continue;
            n = fgm_bytes(&sFgmDir[sFgmRowOf[i]]);
            data = reader_get(&r, sFgmDir[sFgmRowOf[i]].off, n);
            if (data == NULL)
                break;
            spu_memload_sq(sBlock + sFgmAt[i], (uint8_t *)data, n);
            fgm_n++;
            fgm_b += n;
        }
        reader_close(&r);
    }
    if (gBGMBank.n_waves && reader_open(&r, "bgm.pak") == 0)
    {
        for (i = 0; i < gBGMBank.n_waves && i < SNDRES_BGM_BITS; i++)
        {
            const uint8_t *data;
            uint32_t n;

            if (!set_has(load.bgm, i))
                continue;
            n = bgm_bytes(i);
            data = reader_get(&r, gBGMBank.off_blob + gBGMBank.waves[i].off / 2,
                              n);
            if (data == NULL)
                break;
            spu_memload_sq(sBlock + sBgmAt[i], (uint8_t *)data, n);
            bgm_n++;
            bgm_b += n;
        }
        reader_close(&r);
    }
    /* everything resident, kept or read, back to the engines (XBOX:
     * under the sound, with the FGM engine held for just this: a kept
     * sound is written the values it already has) */
    if (live)
        fgm_hold();
    for (i = 0; i < SNDRES_FGM_BITS; i++)
    {
        const SndResFgmRow *row;

        if (!set_has(sResident.fgm, i))
            continue;
        row = &sFgmDir[sFgmRowOf[i]];
        fgm_set_sound(row->orig_index, sBlock + sFgmAt[i], row->nsamples,
                      row->loop_count != 0, row->loop_start, row->loop_end,
                      row->rate_shift);
    }
    for (i = 0; i < gBGMBank.n_waves && i < SNDRES_BGM_BITS; i++)
    {
        if (set_has(sResident.bgm, i))
            gBGMBank.wave_sram[i] = sBlock + sBgmAt[i];
    }

out:
    if (live)
    {
        fgm_release();
        /* the music's wave table again, entries the playing track uses
         * rewritten with what they already hold */
        bgm_waves_changed();
    }
    else
    {
        fgm_resume();
        bgm_resume();
    }
    sSwaps++;
    sResidentBytes = sBlock ? resident_bytes() : 0;
    lc_sound(fgm_b, bgm_b,
             (uint32_t)(timer_ms_gettime64() - t0) * 1000u);
    dbglog(DBG_INFO, "sndres: %s -- read %u sound effects %u KB, %u music "
           "waves %u KB, %u ms (%s); %u of %u KB of the arena resident\n",
           scene_name(scene), (unsigned)fgm_n, (unsigned)(fgm_b / 1024),
           (unsigned)bgm_n, (unsigned)(bgm_b / 1024),
           (unsigned)(timer_ms_gettime64() - t0), how,
           (unsigned)(sResidentBytes / 1024), (unsigned)(sArena / 1024));
}

#endif /* _arch_dreamcast */

void sndres_enter_scene(s32 scene)
{
    SndResSet want;

    if (sSets == NULL || !sndres_compose(scene, &want))
        return;
    if (set_is_subset(&want, &sResident))
        return;
#ifdef _arch_dreamcast
    /* XBOX: the title straight after the movie carries its sound (see
     * sndres_compose's nSCKindTitle arm) */
    upload(scene, &want, scene == nSCKindTitle &&
           gSCManagerSceneData.scene_prev == nSCKindOpeningNewcomers);
#else
    /* Host: no AICA. The composition and the when-to-swap decision are
     * the whole of what the host test can see, so that is all this is. */
    sResident = want;
    sSwaps++;
#endif
}

/* XBOX: see sndres.h. The decomp's scAutoDemoFuncStart starts the
 * stage's music and nSYAudioVoicePublicExcited, and its four fighters
 * fight with every voice and hit sound they have; with no set of its own
 * the demo kept the title's, and all of it was missing. */
void sndres_enter_autodemo(void)
{
    SndResSet want;
    s32 i;

    if (sSets == NULL || gSCManagerBattleState == NULL)
        return;
    memset(&want, 0, sizeof(want));
    set_add_group(&want, SNDRES_GROUP_CORE);
    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        const SCPlayerData *pd = &gSCManagerBattleState->players[i];

        if (pd->pkind == nFTPlayerKindNot || pd->fkind == nFTKindNull)
            continue;
        set_add_group(&want, SNDRES_GROUP_FIGHTER + sndres_voice_kind(pd->fkind));
    }
    if (gSCManagerBattleState->gkind < SNDRES_STAGES)
        set_add_group(&want, SNDRES_GROUP_STAGE + gSCManagerBattleState->gkind);
    if (set_is_subset(&want, &sResident))
        return;
#ifdef _arch_dreamcast
    upload(nSCKindAutoDemo, &want, 0);
#else
    sResident = want;
    sSwaps++;
#endif
}

int sndres_init(int fgm_chn_base)
{
    long size;
    const SndSetsHdr *hd;

    sSets = asset_read_whole("sndsets.bin", &size);
    if (sSets == NULL || size < (long)sizeof(SndSetsHdr) ||
        memcmp(sSets, SNDSETS_MAGIC, 8) != 0)
        goto fail;
    hd = (const SndSetsHdr *)sSets;
    if (hd->n_groups != SNDRES_GROUPS ||
        size < (long)(sizeof(*hd) + SNDRES_GROUPS * sizeof(SndSetsGroup)))
        goto fail;
    sGroups = (const SndSetsGroup *)(sSets + sizeof(*hd));
    sBudget = hd->budget;
    memset(&sResident, 0, sizeof(sResident));

#ifdef _arch_dreamcast
    {
        Reader r;
        uint8_t head[16];
        uint32_t count, i;

        sBootFree = snd_mem_available();
        if (fgm_load_tables(NULL, fgm_chn_base) < 0)
            goto fail;
        sReadBuf = asset_alloc(READ_BUF);
        if (sReadBuf == NULL || reader_open(&r, "fgm_sounds.pak") < 0)
            goto fail;
        if (asset_read(&r.af, head, 16) != 16 ||
            memcmp(head, FGMPAK_MAGIC, 8) != 0)
        {
            reader_close(&r);
            goto fail;
        }
        memcpy(&count, head + 8, 4);
        sFgmDir = malloc(count * sizeof(SndResFgmRow));
        if (sFgmDir == NULL ||
            asset_read(&r.af, sFgmDir, (long)(count * sizeof(SndResFgmRow))) !=
                (long)(count * sizeof(SndResFgmRow)))
        {
            reader_close(&r);
            goto fail;
        }
        reader_close(&r);
        for (i = 0; i < SNDRES_FGM_BITS; i++)
            sFgmRowOf[i] = -1;
        for (i = 0; i < count; i++)
        {
            if (sFgmDir[i].orig_index < SNDRES_FGM_BITS)
                sFgmRowOf[sFgmDir[i].orig_index] = (int16_t)i;
        }
        dbglog(DBG_INFO, "sndres: %u sound effects staged, %u music waves; "
               "%u KB of sound RAM free, sets budgeted to %u KB\n",
               (unsigned)count, (unsigned)gBGMBank.n_waves,
               (unsigned)(snd_mem_available() / 1024),
               (unsigned)(sBudget / 1024));
    }
#else
    (void)fgm_chn_base;
#endif
    return 0;

fail:
    free(sSets);
    sSets = NULL;
    sGroups = NULL;
    return -1;
}
