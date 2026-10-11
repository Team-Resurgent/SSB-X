/* See sc1pmanager.h. sc/sc1pmode/sc1pmanager.c, the whole file.
 *
 * It is the decomp's line for line, with three kinds of change:
 *
 *   1. The overlay DMAs. The decomp declares one SYOverlay per overlay
 *      it loads (sc1pmanager.c:13-48) and passes its address;
 *      src/dc/overlay.h's syDmaLoadOverlay takes the index instead, and
 *      that header says why. The indices are unchanged, and so is which
 *      overlay is loaded where -- including everywhere the router
 *      deliberately does NOT load one. It never reloads overlay 2,
 *      which is its own, and it leaves overlay 2's fighters alone for
 *      the card and the score screen the same way.
 *
 *   2. The eight globals below moved here out of src/dc/scmanager.c,
 *      which held them until this file existed; sc1pmanager.h has that
 *      argument, and src/dc/scmanager.c's note where they used to be
 *      had it first.
 *
 *   3. sc1PManagerEnterScene before every sub-scene (a port line; see
 *      it below). The router runs the ladder's scenes itself, inside
 *      the one nSCKind1PGame arm of scManagerRunScene, so the per-scene
 *      work that loop does before each StartScene -- staging the
 *      scene's sounds above all -- never happened for any of them.
 *
 *   4. NO CUTS: every rung a run can reach is ported, including the
 *      Continue prompt (src/dc/mn1pcontinue.c) and the two bonus
 *      stages (src/dc/sc1pbonusstage.c). What still stops a run short is not in
 *      this file: ft/ftchar/ftboss/ -- Master Hand himself.
 *
 * The REGION_JP arms are not here: the port is REGION_US
 * (src/game/ssb64/Makefile:165 -DREGION_US), so this is the US file
 * with its #if defined(REGION_US) branches taken and the #else ones
 * dropped -- Captain Falcon's twelve minutes rather than twenty,
 * nSCKindStartup rather than nSCKindOpeningRoom, and the Congratulations
 * screen after the staff roll. Every other decomp .c the port compiles
 * unmodified resolves the same defines the same way.
 */

#include "loadcensus.h"
#include "sc1pmanager.h"
#include "overlay.h"
#include "scmanager.h"
#include "ftcommon.h"
/* The scenes the router starts. The game reaches every one of these
 * through <sc/scene.h>, which declares the whole engine at once; the
 * port's scenes each have a header of their own, so the router names
 * them all here. Without these the calls below get an implicit int
 * declaration and compile anyway -- they are all void(void), so nothing
 * breaks, but the warning is the port's tripwire for a missing one. */
#include "mn1pcontinue.h"
#include "mnmessage.h"
#include "sc1pintro.h"
#include "sc1pgame.h"
#include "sc1pbonusstage.h"
#include "sc1pstageclear.h"
#include "sc1pchallenger.h"
#include "mvending.h"
#include "scstaffroll.h"
#include "mncongra.h"
#include "sndres.h"
#include "assetroot.h"
#include "objmodel.h"         /* dc_model_set_demo_opaque */
#include "taskman.h"          /* syTaskmanWantExitPhoto */

#include <macros.h>
#include <gr/ground.h>
#include <ft/fighter.h>
#include <sc/scene.h>
#include <lb/lbbackup.h>
#include <sys/utils.h>

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* CUT: the eleven SYOverlay definitions at sc1pmanager.c:13-48. They are
 * this file's whole .data, and the port has no use for them -- an
 * overlay is an index here, not a ROM address range. src/dc/overlay.c
 * holds the one table that is left of them. */

// 0x80116DA0
u8 dSC1PManagerKirbyTeamModelPartIDs[/* */] =
{
    12,  7,  4,  8,
    11, 10,  5,  9,
     0,  6,  3, 13
};

// 0x80116DAC
s32 dSC1PManagerChallangerFighterKinds[/* */] = { nFTKindLuigi, nFTKindNess, nFTKindPurin, nFTKindCaptain };

// 0x80116DBC
u32 dSC1PManagerUnlockNewcomerKinds[/* */] = { nLBBackupUnlockLuigi, nLBBackupUnlockNess, nLBBackupUnlockPurin, nLBBackupUnlockCaptain };

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x80130D60
u8 sSC1PManagerScenePrev;

// 0x80130D64 - Total time (in frames) taken to complete 1P Game
u32 gSC1PManagerTotalTimeTics;

// 0x80130D68 - Total times fallen in 1P Game
s32 gSC1PManagerTotalFalls;

// 0x80130D6C - Total damage taken in 1P Game
s32 gSC1PManagerTotalDamage;

// 0x80130D70 - Starts at 0, increments by 1 each time a continue is used; lowers CP difficulty (on current stage?)
s32 gSC1PManagerLevelDrop;

// 0x80130D74 - Starts at 2, each time this reaches 0 it increments gSC1PManagerLevelDrop
u8 sSC1PManagerLevelGuard;

// 0x80130D75 - Copy ability of final Kirby on Kirby Team in 1P Game
u8 gSC1PManagerKirbyTeamFinalCopy;

// 0x80130D76
u8 gSC1PManagerKirbyTeamModelPartID;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

// 0x800D6490
s32 sc1PManagerGetFighterKindsNum(u16 mask)
{
    s32 i, j;

    for (i = 0, j = 0; i < (sizeof(u16) * 8); i++, mask = mask >> 1)
    {
        if (mask & 1)
        {
            j++;
        }
    }
    return j;
}

// 0x800D6508
s32 sc1PManagerGetShuffledFighterKind(u16 this_mask, u16 prev_mask, s32 random)
{
    s32 fkind = -1;

    random++;

    do
    {
        fkind++;

        if ((this_mask & (1 << fkind)) && !(prev_mask & (1 << fkind)))
        {
            random--;
        }
    }
    while (random != 0);

    return fkind;
}

// 0x800D6554
s32 sc1PManagerGetShuffledKirbyCopy(u16 flags, s32 random)
{
    s32 ret = -1;

    random++;

    do
    {
        ret++;

        if (flags & (1 << ret))
        {
            random--;
        }
    }
    while (random != 0);

    return ret;
}

// 0x800D6590
void sc1PManagerTrySetChallengers(void)
{
    if
    (
        !(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_NESS) &&
        (gSCManagerBackupData.spgame_difficulty >= nSC1PGameDifficultyNormal) &&
        (gSCManagerSceneData.continues_used == 0) &&
        (gSCManagerBackupData.spgame_stock_count < 3)
    )
    {
        gSCManagerSceneData.spgame_stage = nSC1PGameStageNess;
    }
    else if (!(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_CAPTAIN) && (gSC1PManagerTotalTimeTics < I_MIN_TO_TICS(12)))
    {
        // Captain Falcon's unlock criteria is 12 minutes instead of the reported 20???
        gSCManagerSceneData.spgame_stage = nSC1PGameStageCaptain;
    }
    else if (!(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_PURIN))
    {
        gSCManagerSceneData.spgame_stage = nSC1PGameStagePurin;
    }
}

// 0x800D6630
sb32 sc1PManagerCheckUnlockSoundTest(void)
{
    s32 fkind;
    u16 bonus_record_count;

    if (!(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_SOUNDTEST))
    {
        for (fkind = 0, bonus_record_count = 0; fkind < ARRAY_COUNT(gSCManagerBackupData.spgame_records); fkind++)
        {
            if (gSCManagerBackupData.spgame_records[fkind].bonus1_task_count == SCBATTLE_BONUSGAME_TASK_MAX) // Check if fighter has broken all targets
            {
                bonus_record_count |= (1 << fkind);
            }
        }
        if ((bonus_record_count & LBBACKUP_CHARACTER_MASK_ALL) == LBBACKUP_CHARACTER_MASK_ALL)
        {
            for (fkind = 0, bonus_record_count = 0; fkind < ARRAY_COUNT(gSCManagerBackupData.spgame_records); fkind++)
            {
                if (gSCManagerBackupData.spgame_records[fkind].bonus2_task_count == SCBATTLE_BONUSGAME_TASK_MAX) // Check if fighter has boarded all platforms
                {
                    bonus_record_count |= (1 << fkind);
                }
            }
            if ((bonus_record_count & LBBACKUP_CHARACTER_MASK_ALL) == LBBACKUP_CHARACTER_MASK_ALL)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

// 0x800D6738
void sc1PManagerTrySaveBackup(sb32 is_complete_spgame)
{
    sb32 is_write_data = FALSE;

    if (gSCManagerBackupData.spgame_records[gSCManagerSceneData.fkind].spgame_hiscore < gSCManagerSceneData.spgame_score)
    {
        gSCManagerBackupData.spgame_records[gSCManagerSceneData.fkind].spgame_hiscore   = gSCManagerSceneData.spgame_score;
        gSCManagerBackupData.spgame_records[gSCManagerSceneData.fkind].spgame_continues = gSCManagerSceneData.continues_used;
        gSCManagerBackupData.spgame_records[gSCManagerSceneData.fkind].spgame_total_bonuses   = gSCManagerSceneData.bonus_count;

        if (is_complete_spgame != FALSE)
        {
            gSCManagerBackupData.spgame_records[gSCManagerSceneData.fkind].spgame_best_difficulty = gSCManagerBackupData.spgame_difficulty + 1;
        }
        else gSCManagerBackupData.spgame_records[gSCManagerSceneData.fkind].spgame_best_difficulty = 0;

        is_write_data = TRUE;
    }
    if ((gSCManagerBackupData.spgame_records[gSCManagerSceneData.fkind].is_spgame_complete == FALSE) && (is_complete_spgame != 0))
    {
        gSCManagerBackupData.spgame_records[gSCManagerSceneData.fkind].is_spgame_complete = TRUE;

        is_write_data = TRUE;
    }
    if (is_write_data != FALSE)
    {
        lbBackupWrite();
    }
}

/* Not the game's. What src/dc/scmanager.c's scManagerRunScene does
 * before each scene's StartScene, for the scenes this router starts
 * itself: stage that scene's sounds (src/dc/sndres.c) and clear the
 * layered-fighter opacity a movie may have left set. Without it every
 * scene of a run played on the set staged for nSCKind1PGame -- the VS
 * card's own music, the score screen's jingles and the Continue prompt's
 * announcer lines all logged "not resident". The kind is passed, not
 * read, because the router does not always set scene_curr first: the
 * rung runs with it still nSCKind1PIntro, and the Continue prompt with
 * it still whatever the rung left.
 *
 * It also opens a fresh I/O window (src/dc/assetroot.h), as
 * scManagerRunScene does, and first: the router's scenes never pass back
 * through that loop, so a match's "nothing should be read" would
 * otherwise flag the next scene's own loads, starting with the sounds
 * staged here. */
/* The port's own. The files the load
 * census found every ladder hop reading again: the card's own sprites
 * and camera, the effect and item banks the card and every rung load,
 * the battle HUD, and Stage Clear's sprites -- about 2.5 MB of RAM for
 * about 2.6 MB of disc reads per rung. Held from the first ladder scene
 * to the last, read in at the first while the heap is empty; the
 * ending, the staff roll and the scene manager's own scenes give them
 * back (scManagerHoldFilesForScene clears the list). */
static const char *const sSC1PManagerLadderHeld[] = {
    "sc1pintro.spr", "sc1pintro.cam", "characternames.spr",
    "bonuspicture.spr", "bonuspictureplatform.spr",
    "efcommon.scb", "efcommon.txb", "efcommon.txp",
    "itcommon.itp", "itcommon.scb", "itcommon.txb", "itcommon.txp",
    "ifcommonitem.spr", "ifstatus.spr", "ifdamage.spr", "iftimer.spr",
    "ifdigits.spr", "ifpause.spr", "iftags.spr", "ifannounce.spr",
    "ftshadow.mdl", "ifarrows.mdl", "ifmagnify.mdl",
    "sc1pstageclear1.spr", "sc1pstageclear2.spr", "sc1pstageclear3.spr",
};

static void sc1PManagerEnterScene(s32 scene)
{
    lc_node(scene, scManagerSceneName(scene), 1);
    /* which packs survive into this scene (src/dc/ftmanager.c): the
     * release is the heap reset this scene's start runs */
    ftManagerKeepFilesForScene(scene);
    /* ...except around the Fighting Polygon Team, whose rung the census
     * measured at 10.6 MB of malloc in use and 2.7 MB free: the list is
     * given back for its card and rung, and read again after */
    if ((ftManagerSceneIs1PLadder(scene) != FALSE) &&
        !((gSCManagerSceneData.spgame_stage == nSC1PGameStageZako) &&
          ((scene == nSCKind1PIntro) || (scene == nSCKind1PGame))))
    {
        asset_hold_set(sSC1PManagerLadderHeld,
                       (int)ARRAY_COUNT(sSC1PManagerLadderHeld));
    }
    else
    {
        asset_hold_set(NULL, 0);
    }
    asset_io_reset();
    /* After the window opens, so the census and the io line count it as
     * this scene's load; and only on the way into a card. A card is
     * entered from Stage Clear or the ladder's start, with no rung's packs
     * left in RAM, so the copies land in a lean heap. Filled at the Stage
     * Clear after the Polygon Team's rung, they were read with its thirty
     * packs still resident and left 0.7 MB free (the census, 2026-09-30).
     * Anywhere else a held file that is not in RAM yet is read at its
     * first open, as before. */
    if (scene == nSCKind1PIntro)
    {
        asset_hold_fill();
    }
    dc_model_set_demo_opaque(FALSE);
    dc_model_set_layered_rewalk(FALSE);
    sndres_enter_scene(scene);
}

// 0x800D67DC
void sc1PManagerUpdateScene(void)
{
    s32 i, j;
    u16 this_mask;
    s32 bonus_stat_count;
    sb32 is_player_lose;
    u16 spgame_characters_complete;
    u32 bonus_stat_mask;
    s32 random;
    s32 player;

    sSC1PManagerScenePrev = gSCManagerSceneData.scene_prev;

    gSCManager1PGameBattleState.is_team_battle = TRUE;
    gSCManager1PGameBattleState.game_rules = (SCBATTLE_GAMERULE_1PGAME | SCBATTLE_GAMERULE_TIME);
    gSCManager1PGameBattleState.damage_ratio = 100;
    gSCManager1PGameBattleState.is_show_score = FALSE;
    gSCManager1PGameBattleState.is_not_teamshadows = TRUE;

    if (gSCManagerBackupData.error_flags & LBBACKUP_ERROR_1PGAMEMARIO)
    {
        gSCManagerSceneData.fkind = nFTKindMario;
        gSCManagerSceneData.costume = 0;
    }
    gSCManager1PGameBattleState.players[gSCManagerSceneData.player].handicap = FTCOMMON_HANDICAP_DEFAULT;
    gSCManager1PGameBattleState.players[gSCManagerSceneData.player].pkind = nFTPlayerKindMan;
#ifdef DB_1P_COM
    /* -DDB_1P_COM (src/dc/db.h): the ladder's own player is a
     * level-9 CPU, so an unattended run fights its rungs instead of
     * standing in them */
    gSCManager1PGameBattleState.players[gSCManagerSceneData.player].pkind = nFTPlayerKindCom;
    gSCManager1PGameBattleState.players[gSCManagerSceneData.player].level = 9;
#endif
    gSCManager1PGameBattleState.players[gSCManagerSceneData.player].team = 0;
    gSCManager1PGameBattleState.players[gSCManagerSceneData.player].shade = 0;
    gSCManager1PGameBattleState.players[gSCManagerSceneData.player].color = gSCManagerSceneData.player;
    gSCManager1PGameBattleState.players[gSCManagerSceneData.player].tag = gSCManagerSceneData.player;
    gSCManager1PGameBattleState.players[gSCManagerSceneData.player].fkind = gSCManagerSceneData.fkind;
    gSCManager1PGameBattleState.players[gSCManagerSceneData.player].costume = gSCManagerSceneData.costume;
    gSCManager1PGameBattleState.players[gSCManagerSceneData.player].stock_count = gSCManagerBackupData.spgame_stock_count;
    gSCManager1PGameBattleState.players[gSCManagerSceneData.player].is_spgame_enemy = FALSE;

    gSCManagerSceneData.spgame_score = 0;
    gSCManagerSceneData.continues_used = 0;
    gSCManagerSceneData.bonus_count = 0;

    gSC1PManagerTotalTimeTics = 0;

    gSC1PManagerTotalFalls = 0;
    gSC1PManagerTotalDamage = 0;
    gSC1PManagerLevelDrop = 0;
    sSC1PManagerLevelGuard = 2;

    player = gSCManagerSceneData.player;

    for (i = 0; i < ARRAY_COUNT(gSCManagerSceneData.ally_players); i++)
    {
        if (player == (GMCOMMON_PLAYERS_MAX - 1))
        {
            player = 0;
        }
        else player++;

        gSCManagerSceneData.ally_players[i] = player;
    }
    if (gSCManagerSceneData.spgame_stage >= nSC1PGameStageChallengerStart)
    {
        goto skip_main_stages;
    }
    else
    {
        while (gSCManagerSceneData.spgame_stage <= nSC1PGameStageCommonEnd)
        {
            this_mask = (gSCManagerBackupData.fighter_mask | LBBACKUP_CHARACTER_MASK_STARTER) & ~(1 << gSCManagerSceneData.fkind);

            is_player_lose = FALSE;

            switch (gSCManagerSceneData.spgame_stage)
            {
            case nSC1PGameStageMario:
                this_mask &= ~1;

                gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[0]].fkind = sc1PManagerGetShuffledFighterKind(this_mask, 0, syUtilsRandIntRange(sc1PManagerGetFighterKindsNum(this_mask)));

                if (gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[0]].fkind == nFTKindLuigi)
                {
                    gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[0]].costume = ftParamGetCostumeCommonID(nFTKindLuigi, 1);
                }
                else gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[0]].costume = 0;

                gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[0]].shade = 0;
                break;

            case nSC1PGameStageDonkey:
                random = sc1PManagerGetFighterKindsNum(this_mask);

                gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[0]].fkind = sc1PManagerGetShuffledFighterKind(this_mask, 0, syUtilsRandIntRange(random));
                gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[0]].costume = 0;
                gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[0]].shade = 0;

                gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[1]].fkind = sc1PManagerGetShuffledFighterKind(this_mask, (1 << gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[0]].fkind), syUtilsRandIntRange(random - 1));
                gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[1]].costume = 0;
                gSCManager1PGameBattleState.players[gSCManagerSceneData.ally_players[1]].shade = 0;
                break;

            case nSC1PGameStageKirby:
                this_mask = (gSCManagerBackupData.fighter_mask | LBBACKUP_MASK_FIGHTER(nFTKindKirby));

                gSC1PManagerKirbyTeamFinalCopy = sc1PManagerGetShuffledKirbyCopy(this_mask, syUtilsRandIntRange(sc1PManagerGetFighterKindsNum(this_mask)));

                gSC1PManagerKirbyTeamModelPartID = dSC1PManagerKirbyTeamModelPartIDs[gSC1PManagerKirbyTeamFinalCopy];
                break;
            }
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_1PINTRO);

            gSCManagerSceneData.scene_prev = nSCKind1PGame;
            gSCManagerSceneData.scene_curr = nSCKind1PIntro;

            sc1PManagerEnterScene(nSCKind1PIntro);
            sc1PIntroStartScene();

            /* DIVERGES (port only): the stage's last frame is Stage
             * Clear's wallpaper, and the N64 keeps it in the framebuffer
             * for free. Here the frame loop has to be asked to render it
             * into a texture (taskman.h syTaskmanWantExitPhoto). Both
             * arms below can lead to Stage Clear. */
            syTaskmanWantExitPhoto();

            switch (gSCManagerSceneData.spgame_stage)
            {
            /* sc1pmanager.c:370-380, the two bonus rungs, verbatim: one scene plays either game and decides which
             * off the stage kind sc1PBonusStageInitVars computes, so the
             * two arms are the same four lines.
             *
             * is_player_lose is left alone by both, the decomp's own
             * doing: a bonus stage cannot be LOST, only failed, so the
             * score screen runs and the ladder steps on either way. */
            case nSC1PGameStageBonus2:
            case nSC1PGameStageBonus1:
                syDmaLoadOverlay(OVERLAY_FIGHTING);
                syDmaLoadOverlay(OVERLAY_1PBONUSSTAGE);

                gSCManagerSceneData.scene_prev = nSCKind1PGame;
                gSCManagerSceneData.scene_curr = nSCKind1PBonusStage;

                sc1PManagerEnterScene(nSCKind1PBonusStage);
                sc1PBonusStageStartScene();
                break;

            default:
                syDmaLoadOverlay(OVERLAY_FIGHTING);
                syDmaLoadOverlay(OVERLAY_1PGAMEPLAY);

                sc1PManagerEnterScene(nSCKind1PGame);
                sc1PGameStartScene();

                if (gSCManagerSceneData.spgame_stage != nSC1PGameStageBonus3)
                {
                    if ((gSCManager1PGameBattleState.players[gSCManagerSceneData.player].stock_count == -1) || (gSCManager1PGameBattleState.time_remain == 0))
                    {
                        is_player_lose = TRUE;
                    }
                }
                break;
            }
            if (gSCManagerSceneData.is_reset != FALSE)
            {
                gSCManagerSceneData.scene_prev = nSCKind1PGame;
                gSCManagerSceneData.scene_curr = nSCKind1PMode;

                return;
            }
            if (is_player_lose != FALSE)
            {
                syDmaLoadOverlay(OVERLAY_SUBSYS);
                syDmaLoadOverlay(OVERLAY_1PCONTINUE);

                sc1PManagerEnterScene(nSCKind1PContinue);
                mnPlayers1PGameContinueStartScene();

                if (gSCManagerSceneData.is_continue != FALSE)
                {
                    gSCManagerSceneData.continues_used++;

                    gSCManager1PGameBattleState.players[gSCManagerSceneData.player].stock_count = gSCManagerBackupData.spgame_stock_count;

                    gSCManagerSceneData.spgame_stage--;

                    if (--sSC1PManagerLevelGuard == 0)
                    {
                        sSC1PManagerLevelGuard = 2;

                        gSC1PManagerLevelDrop++;

                        if (gSC1PManagerLevelDrop > 9)
                        {
                            gSC1PManagerLevelDrop = 9;
                        }
                    }
                }
                else
                {
                    sc1PManagerTrySaveBackup(FALSE);

                    gSCManagerSceneData.scene_prev = nSCKind1PGame;
                    gSCManagerSceneData.scene_curr = nSCKindStartup;

                    return;
                }
            }
            else
            {
                gSC1PManagerLevelDrop = 0;
                sSC1PManagerLevelGuard = 2;

                bonus_stat_count = 0;

                for (i = 0; i < ARRAY_COUNT(gSCManagerSceneData.bonus_get_mask); i++)
                {
                    bonus_stat_mask = gSCManagerSceneData.bonus_get_mask[i];

                    for (j = 0; j < (sizeof(u32) * 8); j++, bonus_stat_mask >>= 1)
                    {
                        if (bonus_stat_mask & 1)
                        {
                            bonus_stat_count++;
                        }
                    }
                }
                gSCManagerSceneData.bonus_count += bonus_stat_count;

                syDmaLoadOverlay(OVERLAY_SUBSYS);
                syDmaLoadOverlay(OVERLAY_1PSTAGECLEAR);

                gSCManagerSceneData.scene_prev = nSCKind1PGame;
                gSCManagerSceneData.scene_curr = nSCKind1PStageClear;

                sc1PManagerEnterScene(nSCKind1PStageClear);
                sc1PStageClearStartScene();
            }
            gSCManagerSceneData.spgame_stage++;
        }
        sc1PManagerTrySaveBackup(TRUE);

        syDmaLoadOverlay(OVERLAY_SUBSYS);
        syDmaLoadOverlay(OVERLAY_ENDING);

        gSCManagerSceneData.scene_prev = nSCKind1PGame;
        gSCManagerSceneData.scene_curr = nSCKindEnding;

        sc1PManagerEnterScene(nSCKindEnding);
        mvEndingStartScene();

        syDmaLoadOverlay(OVERLAY_STAFFROLL);

        gSCManagerSceneData.scene_prev = nSCKind1PGame;
        gSCManagerSceneData.scene_curr = nSCKindStaffroll;

        sc1PManagerEnterScene(nSCKindStaffroll);
        scStaffrollStartScene();

        syDmaLoadOverlay(OVERLAY_CONGRA);

        gSCManagerSceneData.scene_prev = nSCKind1PGame;
        gSCManagerSceneData.scene_curr = nSCKindCongra;

        sc1PManagerEnterScene(nSCKindCongra);
        mnCongraStartScene();

        gSCManagerSceneData.spgame_stage--;

        sc1PManagerTrySetChallengers();
    }
skip_main_stages:
    if (gSCManagerSceneData.spgame_stage >= nSC1PGameStageChallengerStart)
    {
        gSCManagerSceneData.challenger_fkind = dSC1PManagerChallangerFighterKinds[gSCManagerSceneData.spgame_stage - nSC1PGameStageChallengerStart];

        syDmaLoadOverlay(OVERLAY_SUBSYS);
        syDmaLoadOverlay(OVERLAY_1PCHALLENGER);

        gSCManagerSceneData.scene_prev = nSCKind1PGame;
        gSCManagerSceneData.scene_curr = nSCKind1PChallenger;

        sc1PManagerEnterScene(nSCKind1PChallenger);
        sc1PChallengerStartScene();

        gSCManager1PGameBattleState.players[gSCManagerSceneData.player].stock_count = 0;

        syDmaLoadOverlay(OVERLAY_FIGHTING);
        syDmaLoadOverlay(OVERLAY_1PGAMEPLAY);

        sc1PManagerEnterScene(nSCKind1PGame);
        sc1PGameStartScene();

        if (gSCManagerSceneData.is_reset != FALSE)
        {
            gSCManagerSceneData.scene_prev = nSCKind1PGame;
            gSCManagerSceneData.scene_curr = nSCKind1PMode;

            return;
        }
        if ((gSCManager1PGameBattleState.players[gSCManagerSceneData.player].stock_count != -1) && (gSCManager1PGameBattleState.time_remain != 0))
        {
            gSCManagerSceneData.challenger_level_drop = dSCManagerDefaultSceneData.challenger_level_drop;
            gSCManagerSceneData.unlock_messages[0] = dSC1PManagerUnlockNewcomerKinds[gSCManagerSceneData.spgame_stage - nSC1PGameStageChallengerStart];

            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_MESSAGE);

            gSCManagerSceneData.scene_prev = nSCKind1PGame;
            gSCManagerSceneData.scene_curr = nSCKindMessage;

            sc1PManagerEnterScene(nSCKindMessage);
            mnMessageStartScene();
        }
        else if (gSCManagerSceneData.challenger_level_drop < 9)
        {
            gSCManagerSceneData.challenger_level_drop += 2;
        }
        if (gSCManagerSceneData.spgame_stage == nSC1PGameStageLuigi)
        {
            gSCManagerSceneData.scene_prev = nSCKind1PBonusStage;
            gSCManagerSceneData.scene_curr = nSCKind1PBonus1Players;
            return;
        }
    }
    if (!(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_INISHIE))
    {
        if ((gSCManagerBackupData.ground_mask & LBBACKUP_GROUND_MASK_ALL) == LBBACKUP_GROUND_MASK_ALL)
        {
            for (i = 0, spgame_characters_complete = 0; i < ARRAY_COUNT(gSCManagerBackupData.spgame_records); i++)
            {
                if (gSCManagerBackupData.spgame_records[i].is_spgame_complete != FALSE)
                {
                    spgame_characters_complete |= (1 << i);
                }
            }
            if ((spgame_characters_complete & LBBACKUP_CHARACTER_MASK_STARTER) == LBBACKUP_CHARACTER_MASK_STARTER)
            {
                gSCManagerSceneData.unlock_messages[0] = nLBBackupUnlockInishie;

                syDmaLoadOverlay(OVERLAY_SUBSYS);
                syDmaLoadOverlay(OVERLAY_MESSAGE);

                gSCManagerSceneData.scene_prev = nSCKind1PGame;
                gSCManagerSceneData.scene_curr = nSCKindMessage;

                sc1PManagerEnterScene(nSCKindMessage);
                mnMessageStartScene();
            }
        }
    }
    gSCManagerSceneData.scene_prev = nSCKind1PGame;
    gSCManagerSceneData.scene_curr = nSCKindStartup;
}

/* dSCManagerOverlays[2]. This file's whole .bss, which is the run's
 * carried state and the two statics beside it -- smashbrothers.us.yaml's
 * ovl2 bss list has sc/sc1pmode/sc1pmanager in it, so the N64 bzeroes
 * exactly these eight on every load of overlay 2.
 *
 * The router itself never loads overlay 2 (sc1pmanager.h says why), so
 * within a run this never fires; what fires it is the scene that starts
 * the NEXT run -- the 1P character select, src/dc/scmanager.c:433-439 --
 * and a fresh run finding a fresh zero is the point. */
void sc1PManagerOverlayLoad(void)
{
    OVERLAY_CLEAR(sSC1PManagerScenePrev);
    OVERLAY_CLEAR(gSC1PManagerTotalTimeTics);
    OVERLAY_CLEAR(gSC1PManagerTotalFalls);
    OVERLAY_CLEAR(gSC1PManagerTotalDamage);
    OVERLAY_CLEAR(gSC1PManagerLevelDrop);
    OVERLAY_CLEAR(sSC1PManagerLevelGuard);
    OVERLAY_CLEAR(gSC1PManagerKirbyTeamFinalCopy);
    OVERLAY_CLEAR(gSC1PManagerKirbyTeamModelPartID);
}
