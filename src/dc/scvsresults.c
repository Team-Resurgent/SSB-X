/* scvsresults.c -- see scvsresults.h. Function-for-function from
 * ssb-decomp-re/src/mn/mnvsmode/mnvsresults.c; every function names its
 * line range.
 */
#include "scvsresults.h"
#include "overlay.h"

#include "bgm.h"
#include "scmanager.h"
#include "taskman.h"

#include <gm/gmsound.h>
#include <mn/mntypes.h>
#include <sc/scsubsys/scsubsys.h> /* scSubsysFighterSetStatus, dSCSubsysFighterScales */
#include <sys/controller.h>
#include <sys/debug.h>
#include "lbcommon.h"
#include "sprite.h"
#include "ftcommon.h"
#include "fighter.h"
#include "objmodel.h"
#include "efmanager.h"
#include <ef/effect.h>
#include <ef/efparticle.h>

#include <if/ifcommon.h>
#include <lb/lbdef.h>
#include <lb/lbtypes.h>
#include <lb/lbtransition.h>
#include <lb/lbbackup.h>
#include <sys/utils.h>
#include <macros.h>
#include <sys/rdp.h>

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/sysshim.c defines it over the FGM engine */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mnvsoptions.c does the same). Each `&llXxxSprite`
 * below is written `llXxxSprite`, the number, with the label's name
 * kept as the macro's. The values are relocData files 34 (MNVSResults),
 * 37 (IFCommonAnnounceCommon), 38 (IFCommonPlayerTags), 36
 * (IFCommonDigits), 164 (IFCommonPlayerDamage) and 18
 * (MNPlayersGameModes) as tools/export/ssb_spriteexport.py --list reads them
 * off the ROM. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* the decomp's share of unused locals: four tables of colours and
 * pointers this file builds and never reads, the `unused` slots IDO's
 * stack layout needed, and the JP arms' strings */
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
/* and mnVSResultsMakeWallpaper's win_player, which the decomp assigns in
 * two `if` arms testing the same flag against FALSE and against TRUE:
 * between them they cover it, and the compiler cannot see that. */
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

#define llMNVSResultsTKOTextSprite   0x00358
#define llMNVSResultsPlaceTextSprite 0x00990
#define llMNVSResultsKOsTextSprite   0x00D38
#define llMNVSResultsPtsTextSprite   0x010D8
#define llMNVSResults1PArrowSprite   0x049E8
#define llMNVSResults2PArrowSprite   0x04B08
#define llMNVSResults3PArrowSprite   0x04C28
#define llMNVSResults4PArrowSprite   0x04D48
#define llMNVSResultsWallpaperSprite 0x0D5C8
#define llMNVSResultsWinnerSprite    0x0E2A0

#define llIFCommonAnnounceCommonLetterASprite 0x005E0
#define llIFCommonAnnounceCommonLetterBSprite 0x009A8
#define llIFCommonAnnounceCommonLetterCSprite 0x00D80
#define llIFCommonAnnounceCommonLetterDSprite 0x01268
#define llIFCommonAnnounceCommonLetterESprite 0x01628
#define llIFCommonAnnounceCommonLetterFSprite 0x01A00
#define llIFCommonAnnounceCommonLetterGSprite 0x01F08
#define llIFCommonAnnounceCommonLetterHSprite 0x02408
#define llIFCommonAnnounceCommonLetterISprite 0x026B8
#define llIFCommonAnnounceCommonLetterJSprite 0x02A90
#define llIFCommonAnnounceCommonLetterKSprite 0x02F98
#define llIFCommonAnnounceCommonLetterLSprite 0x03358
#define llIFCommonAnnounceCommonLetterMSprite 0x03980
#define llIFCommonAnnounceCommonLetterNSprite 0x03E88
#define llIFCommonAnnounceCommonLetterOSprite 0x044B0
#define llIFCommonAnnounceCommonLetterPSprite 0x04890
#define llIFCommonAnnounceCommonLetterQSprite 0x04F10
#define llIFCommonAnnounceCommonLetterRSprite 0x05418
#define llIFCommonAnnounceCommonLetterSSprite 0x057F0
#define llIFCommonAnnounceCommonLetterTSprite 0x05BD0
#define llIFCommonAnnounceCommonLetterUSprite 0x060D8
#define llIFCommonAnnounceCommonLetterVSprite 0x065D8
#define llIFCommonAnnounceCommonLetterWSprite 0x06C00
#define llIFCommonAnnounceCommonLetterXSprite 0x07108
#define llIFCommonAnnounceCommonLetterYSprite 0x07608
#define llIFCommonAnnounceCommonLetterZSprite 0x07AE8
#define llIFCommonAnnounceCommonSymbolExclaimSprite 0x07D98
#define llIFCommonAnnounceCommonSymbolPeriodSprite  0x07E50

#define llIFCommonPlayerTags1PSprite 0x00258
#define llIFCommonPlayerTags2PSprite 0x004F8
#define llIFCommonPlayerTags3PSprite 0x00798
#define llIFCommonPlayerTags4PSprite 0x00A38
#define llIFCommonPlayerTagsCPSprite 0x00CD8

#define llIFCommonDigits0Sprite    0x0068
#define llIFCommonDigits1Sprite    0x0118
#define llIFCommonDigits2Sprite    0x01C8
#define llIFCommonDigits3Sprite    0x0278
#define llIFCommonDigits4Sprite    0x0328
#define llIFCommonDigits5Sprite    0x03D8
#define llIFCommonDigits6Sprite    0x0488
#define llIFCommonDigits7Sprite    0x0538
#define llIFCommonDigits8Sprite    0x05E8
#define llIFCommonDigits9Sprite    0x0698
#define llIFCommonDigitsDashSprite 0x0710

#define llIFCommonPlayerDamageDigit0Sprite 0x0148
#define llIFCommonPlayerDamageDigit1Sprite 0x02D8
#define llIFCommonPlayerDamageDigit2Sprite 0x0500
#define llIFCommonPlayerDamageDigit3Sprite 0x0698
#define llIFCommonPlayerDamageDigit4Sprite 0x08C0
#define llIFCommonPlayerDamageDigit5Sprite 0x0A58
#define llIFCommonPlayerDamageDigit6Sprite 0x0C80
#define llIFCommonPlayerDamageDigit7Sprite 0x0E18
#define llIFCommonPlayerDamageDigit8Sprite 0x1040
#define llIFCommonPlayerDamageDigit9Sprite 0x1270

#define llMNPlayersGameModesFreeForAllTextSprite 0x00280
#define llMNPlayersGameModesTeamBattleTextSprite 0x004E0

/* mnvsresults.c:71-81 dMNVSResultsFileIDs (0x80138F70), the eight link
 * labels' values written out. Four of the eight are files the HUD reads
 * too (src/dc/gmcommon.c has the same numbers); a scene loads its own
 * copy, because a scene's banks go with the scene. */
u32 dMNVSResultsFileIDs[/* */] =
{
     34,                            /* &llMNVSResultsFileID */
     38,                            /* &llIFCommonPlayerTagsFileID */
     18,                            /* &llMNPlayersGameModesFileID */
    164,                            /* &llIFCommonPlayerDamageFileID */
     35,                            /* &llFTEmblemModelsFileID */
     36,                            /* &llIFCommonDigitsFileID */
     37,                            /* &llIFCommonAnnounceCommonFileID */
     25                             /* &llFTStocksZakoFileID */
};

/* mnvsresults.c:177 */
void *sMNVSResultsFiles[ARRAY_COUNT(dMNVSResultsFileIDs)];

/* the banks behind them. Two have none: file 35 is the winner's emblem,
 * which is a model and not sprites (mnVSResultsMakeEmblem below says
 * where it went), and file 25 the game loads and never reads. */
static SpriteBank sMNVSResultsBanks[ARRAY_COUNT(dMNVSResultsFileIDs)];

static const char *const sMNVSResultsBankPaths[ARRAY_COUNT(dMNVSResultsFileIDs)] =
{
    "mnresults.spr",
    "iftags.spr",
    "mngamemodes.spr",
    "ifdamage.spr",
    NULL,
    "ifdigits.spr",
    "ifannounce.spr",
    NULL
};

/* mnvsresults.c:105-147, the screen's own copy of the battle's numbers.
 * It works off these rather than off gSCManagerTransferBattleState
 * directly because the *screen's* places are not the battle's: a stock
 * match copies them across, but a time match recomputes them from KOs
 * minus falls, and a no-contest flattens them all to first. */
static sb32 sMNVSResultsIsPresent[GMCOMMON_PLAYERS_MAX];
static s32 sMNVSResultsKOs[GMCOMMON_PLAYERS_MAX];
static s32 sMNVSResultsTKO[GMCOMMON_PLAYERS_MAX];
static s32 sMNVSResultsPoints[GMCOMMON_PLAYERS_MAX];
static s32 sMNVSResultsPlaces[GMCOMMON_PLAYERS_MAX];
static s32 sMNVSResultsFighterKinds[GMCOMMON_PLAYERS_MAX];
static sb32 sMNVSResultsIsSharedWinner[GMCOMMON_PLAYERS_MAX];
static sb32 sMNVSResultsIsTeamBattle;
static s32 sMNVSResultsKind;
static s32 sMNVSResultsTotalTimeTics;
static s32 sMNVSResultsAllowExitWait;
static s32 sMNVSResultsDrawWallpaperTic;
static s32 sMNVSResultsMakeResultsTic;
static s32 sMNVSResultsInitFightersAllTic;

/* mnvsresults.c:107, :121, :124, :127 -- the screen's own four: the
 * score rule's width and the three tints' alphas. */
static s32 sMNVSResultsBarWidth;
static u32 sMNVSResultsTintAlpha;
static s32 sMNVSResultsWallpaperTintAlpha;
static s32 sMNVSResultsWallpaperTint2Alpha;

/* mnvsresults.c:159, :162 and :165: the podium fighters'
 * fade-in, the four fighters, and their figatree heaps. The heaps stay
 * NULL, as the character select's slots' do (src/dc/mnplayersvs.c
 * mnPlayersVSFuncStart): a pack plays its animations in place
 * (src/dc/ftmanager.c ftManagerAllocFigatreeHeapKind). */
static s32 sMNVSResultsCharacterAlpha;
static GObj *sMNVSResultsFighterGObjs[GMCOMMON_PLAYERS_MAX];
static void *sMNVSResultsFigatreeHeaps[GMCOMMON_PLAYERS_MAX];

/* mnvsresults.c:1891-1899 and :1951-1959, verbatim: the screen has three
 * digits to draw them in. */
s32 mnVSResultsGetKOs(s32 player)
{
    if (sMNVSResultsKOs[player] > 999)
    {
        return 999;
    }
    else return sMNVSResultsKOs[player];
}

s32 mnVSResultsGetTKO(s32 player)
{
    if (sMNVSResultsTKO[player] > 999)
    {
        return 999;
    }
    else return sMNVSResultsTKO[player];
}

/* mnvsresults.c:777-780 and :783-786, verbatim */
s32 mnVSResultsGetPlace(s32 player)
{
    return sMNVSResultsPlaces[player];
}

s32 mnVSResultsGetFighterKind(s32 player)
{
    return sMNVSResultsFighterKinds[player];
}

/* mnvsresults.c:789-800, verbatim. The decomp's loop bounds all through
 * this file are `(ARRAY_COUNT(a) + ARRAY_COUNT(b)) / 2` over arrays of
 * the same length -- an IDO matching artefact, kept because it is what
 * the source says and it evaluates to the same 4. */
static void mnVSResultsSetFighterKindAll(void)
{
    s32 i;

    for (i = 0; i < (ARRAY_COUNT(sMNVSResultsIsPresent) + ARRAY_COUNT(sMNVSResultsFighterKinds) + ARRAY_COUNT(gSCManagerTransferBattleState.players)) / 3; i++)
    {
        if (sMNVSResultsIsPresent[i] != FALSE)
        {
            sMNVSResultsFighterKinds[i] = gSCManagerTransferBattleState.players[i].fkind;
        }
    }
}

/* mnvsresults.c:2499-2537, verbatim: the battle's own score and falls,
 * and the points that rank them. */
static void mnVSResultsSetKOs(void)
{
    s32 i;

    for (i = 0; i < (ARRAY_COUNT(sMNVSResultsIsPresent) + ARRAY_COUNT(sMNVSResultsKOs) + ARRAY_COUNT(gSCManagerTransferBattleState.players)) / 3; i++)
    {
        if (sMNVSResultsIsPresent[i] != FALSE)
        {
            sMNVSResultsKOs[i] = gSCManagerTransferBattleState.players[i].score;
        }
    }
}

static void mnVSResultsSetTKO(void)
{
    s32 i;

    for (i = 0; i < (ARRAY_COUNT(sMNVSResultsIsPresent) + ARRAY_COUNT(sMNVSResultsTKO) + ARRAY_COUNT(gSCManagerTransferBattleState.players)) / 3; i++)
    {
        if (sMNVSResultsIsPresent[i] != FALSE)
        {
            sMNVSResultsTKO[i] = gSCManagerTransferBattleState.players[i].falls;
        }
    }
}

static void mnVSResultsSetPoints(void)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sMNVSResultsPoints); i++)
    {
        sMNVSResultsPoints[i] = mnVSResultsGetKOs(i) - mnVSResultsGetTKO(i);
    }
}

/* mnvsresults.c:2538-2542, verbatim */
static s32 mnVSResultsGetPointsDirect(s32 player)
{
    return sMNVSResultsKOs[player] - sMNVSResultsTKO[player];
}

/* mnvsresults.c:479-573 mnVSResultsGetWinPlayer 0x8013234C, verbatim.
 *
 * The first arm reads like a type confusion and is not one: it compares
 * sMNVSResultsKind against nSCBattleGameRuleTime/Stock, which are 0 and
 * 1, and nMNVSResultsKindTimeRoyal/StockRoyal are also 0 and 1. So the
 * test means "free-for-all", as the decomp's own comment says, and in a
 * free-for-all the winner is simply whoever the battle placed first.
 * The team arm below it ranks by KOs minus falls and breaks ties on
 * KOs; it is unreachable until team battles are (there is no menu to
 * ask for one), and it is here because it is self-contained logic and
 * leaving it out would be the deviation. */
s32 mnVSResultsGetWinPlayer(void)
{
    s32 i;
    sb32 winners_possible[GMCOMMON_PLAYERS_MAX];
    s32 score[GMCOMMON_PLAYERS_MAX];
    s32 win_player = 666;
    sb32 winners_multi[GMCOMMON_PLAYERS_MAX];
    sb32 is_winners_multi;

    if ((sMNVSResultsKind == nSCBattleGameRuleTime) || (sMNVSResultsKind == nSCBattleGameRuleStock))
    {
        for (i = 0; i < (ARRAY_COUNT(sMNVSResultsIsPresent) + ARRAY_COUNT(sMNVSResultsPlaces)) / 2; i++)
        {
            if ((sMNVSResultsIsPresent[i] == TRUE) && (sMNVSResultsPlaces[i] == 0))
            {
                return i;
            }
        }
        return 0;
    }
    else
    {
        for (i = 0; i < ARRAY_COUNT(score); i++)
        {
            score[i] = mnVSResultsGetPointsDirect(i);
        }
        for (i = 0; i < (ARRAY_COUNT(sMNVSResultsIsPresent) + ARRAY_COUNT(sMNVSResultsPlaces) + ARRAY_COUNT(winners_possible)) / 3; i++)
        {
            if ((sMNVSResultsIsPresent[i] != FALSE) && (sMNVSResultsPlaces[i] == 0))
            {
                if (win_player == 666)
                {
                    win_player = i;
                }
                winners_possible[i] = TRUE;
            }
            else winners_possible[i] = FALSE;
        }
        for (i = 0; i < ARRAY_COUNT(winners_multi); i++)
        {
            winners_multi[i] = FALSE;
        }
        for (i = win_player + 1; i < (ARRAY_COUNT(winners_possible) + ARRAY_COUNT(score)) / 2; i++)
        {
            if ((winners_possible[i] != FALSE) && (score[win_player] < score[i]))
            {
                win_player = i;
            }
        }
        is_winners_multi = FALSE;

        for (i = win_player + 1; i < (ARRAY_COUNT(winners_possible) + ARRAY_COUNT(score) + ARRAY_COUNT(winners_multi)) / 3; i++)
        {
            if ((winners_possible[i] != FALSE) && (score[win_player] == score[i]))
            {
                winners_multi[win_player] = winners_multi[i] = TRUE;
                is_winners_multi = TRUE;
            }
        }
        if (is_winners_multi != FALSE)
        {
            for (i = win_player + 1; i < (ARRAY_COUNT(winners_multi) + ARRAY_COUNT(sMNVSResultsKOs)) / 2; i++)
            {
                if ((winners_multi[i]) && (sMNVSResultsKOs[win_player] < sMNVSResultsKOs[i]))
                {
                    win_player = i;
                }
            }
            for (i = 0; i < ARRAY_COUNT(sMNVSResultsIsSharedWinner); i++)
            {
                sMNVSResultsIsSharedWinner[i] = FALSE;
            }
            for (i = win_player + 1; i < (ARRAY_COUNT(sMNVSResultsKOs) + ARRAY_COUNT(sMNVSResultsIsSharedWinner)) / 2; i++)
            {
                if (sMNVSResultsKOs[win_player] == sMNVSResultsKOs[i])
                {
                    sMNVSResultsIsSharedWinner[i] = TRUE;
                    sMNVSResultsIsSharedWinner[win_player] = TRUE;
                }
            }
        }
        return win_player;
    }
}

/* mnvsresults.c:2698-2707 mnVSResultsSetPlaceStock, verbatim: a stock
 * match's places are the ones the battle already handed out
 * (ifCommonBattleUpdateScoreStocks wrote them as each team fell). */
static void mnVSResultsSetPlaceStock(void)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sMNVSResultsIsPresent); i++)
    {
        if (sMNVSResultsIsPresent[i] != FALSE)
        {
            sMNVSResultsPlaces[i] = gSCManagerTransferBattleState.players[i].place;
        }
    }
}

/* mnvsresults.c:2545-2570 mnVSResultsOrderResults 0x80136C2C, verbatim:
 * a selection sort, highest score first. The second arm only fires in
 * sudden death, where two entries on the same score are broken by the
 * place the battle handed out -- which in sudden death is the order the
 * tied players were knocked out in. */
static void mnVSResultsOrderResults(MNVSResultsScore *results, s32 players_num)
{
    MNVSResultsScore temp;
    s32 i, j;

    for (i = 0; i < players_num; i++)
    {
        for (j = i + 1; j < players_num; j++)
        {
            if
            (
                (results[i].score < results[j].score) ||
                (
                    (gSCManagerSceneData.is_suddendeath != FALSE) &&
                    (results[i].score == results[j].score) &&
                    (results[j].place < results[i].place)
                )
            )
            {
                temp = results[i];
                results[i] = results[j];
                results[j] = temp;
            }
        }
    }
}

/* mnvsresults.c:2573-2604 mnVSResultsSetRoyalPlace 0x80136D28, verbatim.
 * A TIME match's free-for-all ranking: sort by KOs minus falls, then walk
 * the sorted list handing out places, and give equal scores the same
 * place -- which is how a TIME match ends in a draw. */
static void mnVSResultsSetRoyalPlace(void)
{
    MNVSResultsScore results[GMCOMMON_PLAYERS_MAX];
    s32 place;
    s32 score;
    s32 winner;
    s32 players_num;
    s32 i;

    for (i = 0, players_num = 0; i < (ARRAY_COUNT(sMNVSResultsIsPresent) + ARRAY_COUNT(gSCManagerVSBattleState.players)) / 2; i++)
    {
        if (sMNVSResultsIsPresent[i])
        {
            results[players_num].score = mnVSResultsGetPointsDirect(i);
            results[players_num].place = gSCManagerVSBattleState.players[i].place;
            results[players_num].player = i;
            players_num++;
        }
    }
    mnVSResultsOrderResults(results, players_num);

    for (i = 0, place = 0, score = results[0].score, winner = results[0].place; i < players_num; i++)
    {
        if ((score != results[i].score) || ((gSCManagerSceneData.is_suddendeath) && (winner != results[i].place)))
        {
            place++;
            score = results[i].score;
            winner = results[i].place;
        }
        sMNVSResultsPlaces[results[i].player] = place;
    }
}

/* mnvsresults.c:2607-2620, verbatim */
static s32 mnVSResultsGetTeamTotalPoints(s32 team)
{
    s32 i;
    s32 total = 0;

    for (i = 0; i < (ARRAY_COUNT(sMNVSResultsIsPresent) + ARRAY_COUNT(gSCManagerTransferBattleState.players)) / 2; i++)
    {
        if ((sMNVSResultsIsPresent[i] != FALSE) && (team == gSCManagerTransferBattleState.players[i].team))
        {
            total += mnVSResultsGetPointsDirect(i);
        }
    }
    return total;
}

/* mnvsresults.c:2623-2634, verbatim */
static void mnVSResultsSetTeamPlace(s32 team, s32 place)
{
    s32 i;

    for (i = 0; i < (ARRAY_COUNT(sMNVSResultsIsPresent) + ARRAY_COUNT(gSCManagerTransferBattleState.players)) / 2; i++)
    {
        if ((sMNVSResultsIsPresent[i] != FALSE) && (team == gSCManagerTransferBattleState.players[i].team))
        {
            sMNVSResultsPlaces[i] = place;
        }
    }
}

/* mnvsresults.c:2637-2649, verbatim: the first player on a team, one
 * based, or 0 for a team nobody is on. */
static s32 mnVSResultsGetTeamFirstPlayer(s32 team)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(gSCManagerTransferBattleState.players); i++)
    {
        if ((team == gSCManagerTransferBattleState.players[i].team) && (gSCManagerTransferBattleState.players[i].pkind != nFTPlayerKindNot))
        {
            return i + 1;
        }
    }
    return 0;
}

/* mnvsresults.c:2652-2685 mnVSResultsSetTeamPlaceAll 0x801371B8,
 * verbatim: the same walk as the free-for-all, over teams rather than
 * players, with each team's score the sum of its players'. Unreachable
 * until team battles are, and here for the same reason
 * mnVSResultsGetWinPlayer's team arm is. */
static void mnVSResultsSetTeamPlaceAll(void)
{
    MNVSResultsScore results[GMCOMMON_PLAYERS_MAX];
    s32 place;
    s32 score;
    s32 winner;
    s32 players;
    s32 i;

    for (i = nSCBattleTeamIDBattleStart, players = 0; i <= nSCBattleTeamIDBattleEnd; i++)
    {
        s32 j = mnVSResultsGetTeamFirstPlayer(i);

        if (j != 0)
        {
            results[players].score = mnVSResultsGetTeamTotalPoints(i);
            results[players].place = gSCManagerVSBattleState.players[j - 1].place;
            results[players].player = i;
            players++;
        }
    }
    mnVSResultsOrderResults(results, players);

    for (i = 0, place = 0, score = results[0].score, winner = results[0].place; i < players; i++)
    {
        if ((score != results[i].score) || ((gSCManagerSceneData.is_suddendeath) && (winner != results[i].place)))
        {
            place++;
            score = results[i].score;
            winner = results[i].place;
        }
        mnVSResultsSetTeamPlace(results[i].player, place);
    }
}

/* mnvsresults.c:2688-2695, verbatim: a TIME match's ranking. */
static void mnVSResultsSetPlaceTime(void)
{
    if (sMNVSResultsIsTeamBattle == FALSE)
    {
        mnVSResultsSetRoyalPlace();
    }
    else mnVSResultsSetTeamPlaceAll();
}

/* mnvsresults.c:2711-2736 mnVSResultsInitRankings.
 *
 * Both arms are the game's: a stock match's places are
 * the ones the battle handed out as each team fell, a TIME match's are
 * sorted here out of KOs minus falls. *
 * The no-contest flattening below it is kept: is_reset is what sets that
 * kind, and although nothing sets is_reset yet (the reset-button combo is
 * the same one syTaskmanCheckBreakLoop's second arm waits for), the two
 * arrive together. */
static void mnVSResultsInitRankings(void)
{
    mnVSResultsSetKOs();
    mnVSResultsSetTKO();
    mnVSResultsSetPoints();

    if (gSCManagerTransferBattleState.game_rules == SCBATTLE_GAMERULE_STOCK)
    {
        mnVSResultsSetPlaceStock();
    }
    else mnVSResultsSetPlaceTime();

    if (sMNVSResultsKind == nMNVSResultsKindNoContest)
    {
        s32 i;

        for (i = 0; i < ARRAY_COUNT(sMNVSResultsPlaces); i++)
        {
            sMNVSResultsPlaces[i] = 0;
        }
    }
    mnVSResultsSetFighterKindAll();
}

/* mnvsresults.c:2739-2749, verbatim */
static void mnVSResultsSetIsPresent(void)
{
    s32 i;

    for (i = 0; i < (ARRAY_COUNT(sMNVSResultsIsPresent) + ARRAY_COUNT(gSCManagerTransferBattleState.players)) / 2; i++)
    {
        if (gSCManagerTransferBattleState.players[i].pkind == nFTPlayerKindNot)
        {
            sMNVSResultsIsPresent[i] = FALSE;
        }
        else sMNVSResultsIsPresent[i] = TRUE;
    }
}

/* mnvsresults.c:2799-2847 mnVSResultsInitVars, verbatim. The three tic
 * marks are the screen's schedule -- wallpaper at 80, the ranked rows and
 * the confetti at 120, the fighters at 120 -- and they are kept even
 * though nothing draws yet, because mnVSResultsFuncRun's shape is built
 * on them and the exit wait is measured from the same clock. */
static void mnVSResultsInitVars(void)
{
    s32 i;

    sMNVSResultsTotalTimeTics = 0;
    sMNVSResultsBarWidth = 0;
    sMNVSResultsCharacterAlpha = 0x00;
    sMNVSResultsIsTeamBattle = gSCManagerTransferBattleState.is_team_battle;

    for (i = 0; i < ARRAY_COUNT(sMNVSResultsIsSharedWinner); i++)
    {
        sMNVSResultsIsSharedWinner[i] = FALSE;
    }
    if (gSCManagerTransferBattleState.game_rules == SCBATTLE_GAMERULE_TIME)
    {
        if (sMNVSResultsIsTeamBattle == FALSE)
        {
            sMNVSResultsKind = nMNVSResultsKindTimeRoyal;
        }
        else sMNVSResultsKind = nMNVSResultsKindTimeTeam;

        sMNVSResultsAllowExitWait = 410;
    }
    else
    {
        if (sMNVSResultsIsTeamBattle == FALSE)
        {
            sMNVSResultsKind = nMNVSResultsKindStockRoyal;
        }
        else sMNVSResultsKind = nMNVSResultsKindStockTeam;

        sMNVSResultsAllowExitWait = 370;
    }
    if (gSCManagerSceneData.is_reset != FALSE)
    {
        sMNVSResultsKind = nMNVSResultsKindNoContest;
        sMNVSResultsAllowExitWait = 200;

        sMNVSResultsDrawWallpaperTic = 1;
        sMNVSResultsMakeResultsTic = 1;
        sMNVSResultsInitFightersAllTic = 1;
    }
    else
    {
        sMNVSResultsDrawWallpaperTic = 80;
        sMNVSResultsMakeResultsTic = 120;
        sMNVSResultsInitFightersAllTic = 120;
    }
}

/* mnvsresults.c:192-195 mnVSResultsGetPlayerCount, verbatim. */
static s32 mnVSResultsGetPlayerCount(void)
{
    return gSCManagerTransferBattleState.pl_count +
           gSCManagerTransferBattleState.cp_count;
}

/* mnvsresults.c:198-262 mnVSResultsSaveBackup 0x80131B90, verbatim, and
 * the first thing in the port that writes the save data rather than
 * reading it. Every match that reaches this screen counts: the total,
 * the stage in ground_mask (Mushroom Kingdom's gate is all eight of
 * them), the item-switch counter, and per character the time, the
 * damage given and taken, the self-destructs, the games played, and
 * against each other character the KOs, the tallies and the meetings.
 * Then lbBackupWrite, which is the only place in the whole game that
 * writes the cartridge outside the options screens.
 *
 * The store it writes to is a byte array (src/dc/sysshim.c), so the records survive a session and not a
 * power cycle. The arithmetic is the game's either way, and it is what
 * the unlock check in mnVSResultsFuncRun reads back. */
void mnVSResultsSaveBackup(void)
{
    s32 i, j;

    gSCManagerBackupData.vs_total_battles++;
    gSCManagerBackupData.ground_mask |=
        (1 << gSCManagerTransferBattleState.gkind);

    if (gSCManagerBackupData.vs_itemswitch_battles < U8_MAX)
    {
        gSCManagerBackupData.vs_itemswitch_battles++;
    }
    for (i = 0; i < ARRAY_COUNT(gSCManagerTransferBattleState.players); i++)
    {
        if (gSCManagerTransferBattleState.players[i].pkind != nFTPlayerKindNot)
        {
            u8 this_fkind = gSCManagerTransferBattleState.players[i].fkind;

            gSCManagerBackupData.vs_records[this_fkind].time_used +=
                (gSCManagerTransferBattleState.time_passed / UPDATE_INTERVAL);

            if (gSCManagerBackupData.vs_records[this_fkind].time_used >
                I_MIN_TO_TICS(1000) - 1)
            {
                gSCManagerBackupData.vs_records[this_fkind].time_used =
                    I_MIN_TO_TICS(1000) - 1;
            }
            gSCManagerBackupData.vs_records[this_fkind].damage_given +=
                gSCManagerTransferBattleState.players[i].total_damage_given;

            if (gSCManagerBackupData.vs_records[this_fkind].damage_given > 999999)
            {
                gSCManagerBackupData.vs_records[this_fkind].damage_given = 999999;
            }
            gSCManagerBackupData.vs_records[this_fkind].damage_taken +=
                gSCManagerTransferBattleState.players[i].total_damage_all;

            if (gSCManagerBackupData.vs_records[this_fkind].damage_taken > 999999)
            {
                gSCManagerBackupData.vs_records[this_fkind].damage_taken = 999999;
            }
            gSCManagerBackupData.vs_records[this_fkind].selfdestructs +=
                gSCManagerTransferBattleState.players[i].total_selfdestructs;

            if (gSCManagerBackupData.vs_records[this_fkind].selfdestructs > 9999)
            {
                gSCManagerBackupData.vs_records[this_fkind].selfdestructs = 9999;
            }
            gSCManagerBackupData.vs_records[this_fkind].games_played++;
            gSCManagerBackupData.vs_records[this_fkind].player_count_tally +=
                mnVSResultsGetPlayerCount();

            for (j = 0; j < ARRAY_COUNT(gSCManagerTransferBattleState.players); j++)
            {
                if ((i != j) &&
                    (gSCManagerTransferBattleState.players[j].pkind != nFTPlayerKindNot))
                {
                    u8 vs_fkind = gSCManagerTransferBattleState.players[j].fkind;

                    gSCManagerBackupData.vs_records[this_fkind].ko_count[vs_fkind] +=
                        gSCManagerTransferBattleState.players[i].total_kos_players[j];

                    if (gSCManagerBackupData.vs_records[this_fkind].ko_count[vs_fkind] > 9999)
                    {
                        gSCManagerBackupData.vs_records[this_fkind].ko_count[vs_fkind] = 9999;
                    }
                    gSCManagerBackupData.vs_records[this_fkind].player_count_tallies[vs_fkind] +=
                        mnVSResultsGetPlayerCount();
                    gSCManagerBackupData.vs_records[this_fkind].played_against[vs_fkind]++;
                }
            }
        }
    }
    lbBackupWrite();
}

/* mnvsresults.c:266-281 mnVSResultsCheckExit, verbatim: after the wait,
 * START on any of the four pads leaves. */
static sb32 mnVSResultsCheckExit(void)
{
    s32 i;

    if (sMNVSResultsTotalTimeTics >= sMNVSResultsAllowExitWait)
    {
        for (i = 0; i < ARRAY_COUNT(gSYControllerDevices); i++)
        {
            if (gSYControllerDevices[i].button_tap & START_BUTTON)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}


/* ---- mnvsresults.c's screen -----------------------------
 *
 * Everything from here to mnVSResultsFuncRun is the screen the scene
 * kept a clock for and did not draw: the wallpaper in the winner's
 * colour, the three tints that fade over it, the ranked rows of KOs,
 * falls, points and places, the label, the player tags, the winner's
 * name spelled out of the announcer's alphabet, and the announcer and
 * the victory theme over it. The functions are the decomp's by name and
 * body; each names its line range. What is still not here is named at
 * the head of scvsresults.h and at the function it belongs to.
 * -------------------------------------------------------------------- */

/* mnvsresults.c:361-374 0x8013205C, verbatim */
s32 mnVSResultsGetPresentCount(void)
{
    s32 i, sum = 0;

    for (i = 0; i < ARRAY_COUNT(sMNVSResultsIsPresent); i++)
    {
        if (sMNVSResultsIsPresent[i] == TRUE)
        {
            sum++;
        }
    }
    return sum;
}

/* mnvsresults.c:376-393 0x801320B8, verbatim: how many present players
 * come before this one, which is the column it is drawn in. */
s32 mnVSResultsGetPresentLowerCount(s32 player)
{
    s32 i, sum = 0;

    for (i = 0; i < ARRAY_COUNT(sMNVSResultsIsPresent); i++)
    {
        if (player != i)
        {
            if (sMNVSResultsIsPresent[i] == TRUE)
            {
                sum++;
            }
        }
        else break;
    }
    return sum;
}

/* mnvsresults.c:395-407 0x80132100, verbatim */
s32 mnVSResultsGetPlacePlayer(s32 place)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sMNVSResultsIsPresent); i++)
    {
        if ((sMNVSResultsIsPresent[i] != FALSE) && (place == sMNVSResultsPlaces[i]))
        {
            return i;
        }
    }
    return -1;
}

/* :933 below, which the decomp reaches through its own header */
s32 mnVSResultsGetPlayerCountPlace(s32 place);

/* mnvsresults.c:410-476 0x801321AC, verbatim. Which of the podium's
 * standing spots a player takes -- the game spaces them by how many
 * players there are and shuffles the winner to the middle -- and the
 * fighters stand on them (mnVSResultsSetFighterPosition) and the player
 * tags sit above them. */
s32 mnVSResultsGetPlayerDistanceID(s32 player)
{
    s32 foes = mnVSResultsGetPresentLowerCount(player);

    if (mnVSResultsGetPlayerCountPlace(0) == 1)
    {
        switch (mnVSResultsGetPresentCount())
        {
        case 2:
            break;

        case 3:
            if (sMNVSResultsPlaces[player] == 0)
            {
                switch (mnVSResultsGetPresentLowerCount(player))
                {
                case 0:
                    foes = 1;
                    break;

                case 2:
                    foes = 1;
                    break;
                }
            }
            else if (mnVSResultsGetPresentLowerCount(player) == 1)
            {
                foes = mnVSResultsGetPresentLowerCount(mnVSResultsGetPlacePlayer(0));
            }
            break;

        default:
            switch (player)
            {
            case 0:
                if ((sMNVSResultsPlaces[0] == 0) && (sMNVSResultsPlaces[1] != 0))
                {
                    foes = 1;
                }
                break;

            case 1:
                if ((sMNVSResultsPlaces[0] == 0) && (sMNVSResultsPlaces[1] != 0))
                {
                    foes = 0;
                }
                break;

            case 2:
                if ((sMNVSResultsPlaces[3] == 0) && (sMNVSResultsPlaces[2] != 0))
                {
                    foes = 3;
                }
                break;

            case 3:
                if ((sMNVSResultsPlaces[3] == 0) && (sMNVSResultsPlaces[2] != 0))
                {
                    foes = 2;
                }
                break;
            }
            break;
        }
    }
    return foes;
}

/* mnvsresults.c:577-580 0x80132A2C, verbatim */
u8 mnVSResultsGetWinTeam(void)
{
    return gSCManagerTransferBattleState.players[mnVSResultsGetWinPlayer()].team;
}

/* mnvsresults.c:624-647's three parallel tables, collapsed into one.
 * The game holds a DObjDesc, an MObjSub and a MatAnimJoint per fighter
 * kind, all three out of relocData 35 and all three naming the same
 * emblem; here each of those is a pack, so one row is one path.
 *
 * Twelve kinds, ten emblems: Luigi wears Mario's and Jigglypuff wears
 * Pikachu's, which is the duplication the decomp's tables have too. */
static const char *const dMNVSResultsEmblemPacks[/* */] =
{
    "mario.mdl",            /* Mario */
    "fox.mdl",              /* Fox */
    "donkey.mdl",           /* Donkey Kong */
    "metroid.mdl",          /* Samus */
    "mario.mdl",            /* Luigi */
    "zelda.mdl",            /* Link */
    "yoshi.mdl",            /* Yoshi */
    "fzero.mdl",            /* Captain Falcon */
    "kirby.mdl",            /* Kirby */
    "pmonsters.mdl",        /* Pikachu */
    "pmonsters.mdl",        /* Jigglypuff */
    "mother.mdl",           /* Ness */
};

/* The pack behind whichever one the winner earned. One at a time, the
 * way the game holds one relocData file: mnVSResultsMakeEmblem reads
 * the winner's kind and loads that row.
 *
 * DIVERGES on when it is given back. The game's copy is in the scene
 * heap and goes when the scene does; the port frees it on the way *in*
 * to the scene instead (mnVSResultsOverlayLoad), so between the results
 * screen and the next one it holds the pack's few kilobytes of RAM --
 * and no PVR at all, because an emblem is untextured. */
static Fighter sMNVSResultsEmblemModel;

/* mnvsresults.c:583-613 0x80132A68, verbatim. The emblem zooms out from
 * scale 25 to 10 at 0.15 a tic and rises from y 100 to y 1000 at 11 a
 * tic, both from tic 40. */
void mnVSResultsEmblemProcUpdate(GObj *gobj)
{
    f32 new_scale;
    f32 min_scale = 10.0F;
    f32 max_y = 1000.0F;

    if (sMNVSResultsTotalTimeTics >= 40)
    {
        if (min_scale < DObjGetStruct(gobj)->scale.vec.f.x)
        {
            new_scale = DObjGetStruct(gobj)->scale.vec.f.x - 0.15F;

            if (new_scale < min_scale)
            {
                new_scale = min_scale;
            }
            DObjGetStruct(gobj)->scale.vec.f.x = new_scale;
            DObjGetStruct(gobj)->scale.vec.f.y = new_scale;
        }
        if (DObjGetStruct(gobj)->translate.vec.f.y < max_y)
        {
            DObjGetStruct(gobj)->translate.vec.f.y += 11.0F;

            if (DObjGetStruct(gobj)->translate.vec.f.y > max_y)
            {
                DObjGetStruct(gobj)->translate.vec.f.y = max_y;
            }
        }
    }
}

/* mnvsresults.c:615-676 0x80132B20. The winner's series emblem, and the
 * port's first model with a material: it is untextured lit geometry
 * whose colour is the two light colours its MObj chain carries, and the
 * MatAnimJoint is a table of five of them a frame apart. So
 * gcAddMatAnimJointAll's `color` -- the winning player's number, or the
 * winning team's out of colors[] -- is not a start frame in any moving
 * sense; it is which colour, and gcPlayAnimAll seeks the script there
 * and stops. Red, blue, yellow, green.
 *
 * DIVERGES in one place, the same seam every model in the port has: the
 * game calls gcSetupCommonDObjs on a DObjDesc it read out of relocData
 * 35, and the port has no runtime display-list interpreter, so
 * tools/export/ssb_emblemexport.py bakes each of the ten at build time and
 * dc_model_add_dobjs builds the same tree from the pack. The MObjs and
 * their scripts ride in the pack beside it and dc_model_add_mobjs hands
 * them to gcAddMObjAll and gcAddMatAnimJointAll unchanged, so which
 * colour the emblem comes up in is the game's parser's answer.
 *
 * The other line that is not the decomp's is the display proc: the game
 * gives it gcDrawDObjTreeForGObj and the port gives it the layered one,
 * because the emblem is 3D standing between two sprite passes -- the
 * wallpaper under it at camera priority 80, everything else over it --
 * and the PVR's opaque list draws under every sprite. The character
 * select's fighters on their gates are the same case. */
void mnVSResultsMakeEmblem(void)
{
    GObj *gobj;
    s32 win_player;
    s32 win_fkind;
    s32 color;

    s32 colors[/* */] = { 0, 1, 3 };

    if (sMNVSResultsIsTeamBattle == FALSE)
    {
        win_player = mnVSResultsGetWinPlayer();
        win_fkind = mnVSResultsGetFighterKind(win_player);
        color = win_player;
    }
    if (sMNVSResultsIsTeamBattle == TRUE)
    {
        win_fkind = mnVSResultsGetFighterKind(mnVSResultsGetWinPlayer());
        color = colors[mnVSResultsGetWinTeam()];
    }
    if (win_fkind < 0 ||
        win_fkind >= (s32) ARRAY_COUNT(dMNVSResultsEmblemPacks))
    {
        syDebugPrintf("mnVSResultsMakeEmblem: no emblem for kind %d\n",
                      (int)win_fkind);
        return;
    }
#ifndef FT_HOSTTEST
    {
        int pal_bank = 0;

        if (fighter_load_scene(&sMNVSResultsEmblemModel,
                         dMNVSResultsEmblemPacks[win_fkind],
                         &pal_bank) != 0)
        {
            return;
        }
    }
#endif
    gobj = gcMakeGObjSPAfter(0, NULL, 23, GOBJ_PRIORITY_DEFAULT);

#ifndef FT_HOSTTEST
    dc_model_add_dobjs(gobj, NULL, &sMNVSResultsEmblemModel, NULL);
    gcAddGObjDisplay(gobj, dc_model_proc_display_layered, 33,
                     GOBJ_PRIORITY_DEFAULT, ~0);
    dc_model_add_mobjs(gobj, &sMNVSResultsEmblemModel, (f32) color);
#else
    /* The host cross-test links the scene, not the renderer, so there is
     * no pack to build a tree from. What is left is the half the test can
     * see -- the GObj, its camera, its placement and the proc update's
     * arithmetic -- and those all hang off one DObj, so it gets a bare
     * one. src/dc/lbtransition.c compiles its model half out the same
     * way. */
    (void)color;
    gcAddDObjRpyR(gobj, NULL);
#endif
    gcPlayAnimAll(gobj);
    gcAddGObjProcess(gobj, mnVSResultsEmblemProcUpdate, nGCProcessKindFunc, 1);

    DObjGetStruct(gobj)->translate.vec.f.x = 0.0F;
    DObjGetStruct(gobj)->translate.vec.f.y = 100.0F;
    DObjGetStruct(gobj)->translate.vec.f.z = -11000.0F;
    DObjGetStruct(gobj)->scale.vec.f.x = 25.0F;
    DObjGetStruct(gobj)->scale.vec.f.y = 25.0F;
}

/* mnvsresults.c:679-691 0x80132D84. The wallpaper is one I4 sprite
 * tinted between two colours: RGB = lerp(ENV, PRIM, I), which is the
 * port's nLBCommonCombineIAPrimEnv (src/dc/lbcommon.h). The game's
 * alpha side is the constant 1 and the port's is PRIM's alpha, which
 * lbCommonMakeSObjForGObj leaves at 0xFF: the same opaque wallpaper. */
void mnVSResultsWallpaperProcDisplay(GObj *gobj)
{
    SObj *sobj = SObjGetStruct(gobj);

    lbCommonSpriteSetPrimColor(sobj->sprite.red, sobj->sprite.green,
                               sobj->sprite.blue, sobj->sprite.alpha);
    lbCommonSpriteSetEnvColor(sobj->envcolor.r, sobj->envcolor.g,
                              sobj->envcolor.b, sobj->envcolor.a);
    lbCommonSpriteSetCombine(nLBCommonCombineIAPrimEnv);

    lbCommonDrawSObjNoAttr(gobj);
}

/* mnvsresults.c:694-774 0x80132EA8. Makes its own camera, as the game
 * does: the wallpaper arrives 80 tics in and its camera with it. The
 * four colour pairs are the winner's player colour, or the winning
 * team's; a no contest picks one at random. */
void mnVSResultsMakeWallpaper(void)
{
    GObj *gobj;
    SObj *sobj;
    GObj *camera_gobj;
    s32 unused[2];
    s32 win_player;

    SYColorRGBPair unused_colors[/* */] =
    {
        { { 0xAB, 0x31, 0x25 }, { 0xAF, 0x56, 0x4E } },
        { { 0x00, 0x3F, 0xFF }, { 0x39, 0x6A, 0xFF } },
        { { 0xDE, 0xB2, 0x00 }, { 0xFF, 0xD7, 0x33 } },
        { { 0x17, 0x7E, 0x43 }, { 0x2A, 0x98, 0x45 } }
    };
    s32 team_colors[/* */] = { 0, 1, 3 };

    SYColorRGBPair colors[/* */] =
    {
        { { 0x5C, 0x2B, 0x27 }, { 0x98, 0x6F, 0x6C } },
        { { 0x39, 0x39, 0x99 }, { 0x86, 0x86, 0xD1 } },
        { { 0x69, 0x58, 0x2B }, { 0x9B, 0x8E, 0x6C } },
        { { 0x2B, 0x44, 0x36 }, { 0x71, 0x82, 0x78 } }
    };
    CObj *cobj;

    camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSprite,
        80,
        COBJ_MASK_DLLINK(26),
        ~0,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    cobj = CObjGetStruct(camera_gobj);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    if (sMNVSResultsKind == nMNVSResultsKindNoContest)
    {
        win_player = syUtilsRandIntRange(GMCOMMON_PLAYERS_MAX);
    }
    else
    {
        if (sMNVSResultsIsTeamBattle == FALSE)
        {
            win_player = mnVSResultsGetWinPlayer();
        }
        if (sMNVSResultsIsTeamBattle == TRUE)
        {
            win_player = team_colors[mnVSResultsGetWinTeam()];
        }
    }
    gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, mnVSResultsWallpaperProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSResultsFiles[0], llMNVSResultsWallpaperSprite));

    SObjGetStruct(gobj)->pos.x = 10.0F;
    SObjGetStruct(gobj)->pos.y = 10.0F;
    sobj->envcolor.r = colors[win_player].prim.r;
    sobj->envcolor.g = colors[win_player].prim.g;
    sobj->envcolor.b = colors[win_player].prim.b;
    sobj->sprite.red = colors[win_player].env.r;
    sobj->sprite.green = colors[win_player].env.g;
    sobj->sprite.blue = colors[win_player].env.b;
}

/* mnvsresults.c:803-930, verbatim: where a podium fighter
 * stands for its spot, turning the losers to the winner, and the win or
 * lose pose. The poses are demo statuses scSubsysFighterSetStatus still
 * stands as Wait (src/dc/scsubsysfighter.c), which is its own gap. */
void mnVSResultsSetFighterPosition(GObj* fighter_gobj, s32 player, s32 place)
{
	f32 pos_x_2p[/* */][4] =
	{
		-150.0F, -350.0F, -700.0F, -1000.0F,
		 100.0F,  250.0F,  600.0F,  1000.0F
	};
	f32 pos_x_3p[/* */][4] =
	{
		-450.0F, -900.0F, -2000.0F, -3000.0F,
		   0.0F,    0.0F,     0.0F,     0.0F,
		 400.0F,  800.0F,  1800.0F,  2800.0F
	};
	f32 pos_x_4p[/* */][4] =
	{
		-450.0F, -900.0F, -2000.0F, -3000.0F,
		-150.0F, -350.0F,  -700.0F, -1000.0F,
		 150.0F,  300.0F,   700.0F,  1000.0F,
		 400.0F,  800.0F,  1800.0F,  2800.0F
	};
	f32 pos_yz[/* */][2] =
	{
		{ -350.0F, 	   0.0F },
		{ -450.0F, -2000.0F },
		{ -700.0F, -5000.0F },
		{ -900.0F, -9000.0F }
	};

	switch (mnVSResultsGetPresentCount())
	{
	case 2:
		DObjGetStruct(fighter_gobj)->translate.vec.f.x = pos_x_2p[mnVSResultsGetPlayerDistanceID(player)][place];
		break;

	case 3:
		DObjGetStruct(fighter_gobj)->translate.vec.f.x = pos_x_3p[mnVSResultsGetPlayerDistanceID(player)][place];
		break;

	case 4:
	default:
		DObjGetStruct(fighter_gobj)->translate.vec.f.x = pos_x_4p[mnVSResultsGetPlayerDistanceID(player)][place];
		break;
	}
	DObjGetStruct(fighter_gobj)->translate.vec.f.y = pos_yz[place][0];
	DObjGetStruct(fighter_gobj)->translate.vec.f.z = pos_yz[place][1];
}

// 0x801333E4
void mnVSResultsFaceWinner(GObj *fighter_gobj, s32 player, s32 place)
{
	s32 win_player = mnVSResultsGetWinPlayer();

	if (place != 0)
	{
		DObj *fighter_dobj = DObjGetStruct(fighter_gobj);
		DObj *winner_dobj = DObjGetStruct(sMNVSResultsFighterGObjs[win_player]);
		f32 x1 = fighter_dobj->translate.vec.f.x;
		f32 z1 = fighter_dobj->translate.vec.f.z;
		f32 x2 = winner_dobj->translate.vec.f.x;
		f32 z2 = winner_dobj->translate.vec.f.z;

		DObjGetStruct(fighter_gobj)->rotate.vec.f.y = syUtilsArcTan2(x2 - x1, z2 - z1);
	}
}

// 0x8013345C
s32 mnVSResultsGetStatusWin(s32 fkind)
{
	s32 status_ids[/* */] =
	{
		nFTDemoStatusWin1,
		nFTDemoStatusWin2,
		nFTDemoStatusWin3
	};

	if (fkind == nFTKindKirby)
	{
		return status_ids[syUtilsRandIntRange(2)];
	}
	else return status_ids[syUtilsRandIntRange(3)];
}

// 0x801334CC - bruh
s32 mnVSResultsGetStatusLose(s32 fkind)
{
	return nFTDemoStatusLose;
}

// 0x801334DC
void mnVSResultsSetFighterStatus(GObj *fighter_gobj, s32 player)
{
	if (sMNVSResultsKind == nMNVSResultsKindNoContest)
	{
		scSubsysFighterSetStatus(fighter_gobj, mnVSResultsGetStatusLose(mnVSResultsGetFighterKind(player)));
	}
	else switch (mnVSResultsGetPresentCount())
	{
	case 2:
		switch (sMNVSResultsPlaces[player])
		{
		case 0:
			scSubsysFighterSetStatus(fighter_gobj, mnVSResultsGetStatusWin(mnVSResultsGetFighterKind(player)));
			break;
			
		case 1:
			scSubsysFighterSetStatus(fighter_gobj, mnVSResultsGetStatusLose(mnVSResultsGetFighterKind(player)));
			break;
		}
		break;

	case 3:
		if (sMNVSResultsPlaces[player] == 0)
		{
			scSubsysFighterSetStatus(fighter_gobj, mnVSResultsGetStatusWin(mnVSResultsGetFighterKind(player)));
		}
		else scSubsysFighterSetStatus(fighter_gobj, mnVSResultsGetStatusLose(mnVSResultsGetFighterKind(player)));
		break;
		
	case 4:
	default:
		if (sMNVSResultsPlaces[player] == 0)
		{
			scSubsysFighterSetStatus(fighter_gobj, mnVSResultsGetStatusWin(mnVSResultsGetFighterKind(player)));
		}
		else scSubsysFighterSetStatus(fighter_gobj, mnVSResultsGetStatusLose(mnVSResultsGetFighterKind(player)));
		break;
	}
}


/* mnvsresults.c:933-946 0x80133684, verbatim */
s32 mnVSResultsGetPlayerCountPlace(s32 place)
{
    s32 num = 0;
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sMNVSResultsIsPresent); i++)
    {
        if ((sMNVSResultsIsPresent[i] != FALSE) && (place == sMNVSResultsPlaces[i]))
        {
            num++;
        }
    }
    return num;
}

/* mnvsresults.c:949-962 0x80133718, verbatim */
s32 mnVSResultsGetPlayerCountAhead(s32 player)
{
    s32 num = 0;
    s32 i;

    for (i = 0; i < (ARRAY_COUNT(sMNVSResultsIsPresent) + ARRAY_COUNT(sMNVSResultsPlaces)) / 2; i++)
    {
        if ((player != i) && (sMNVSResultsIsPresent[i] != FALSE) && (sMNVSResultsPlaces[i] < sMNVSResultsPlaces[player]))
        {
            num++;
        }
    }
    return num;
}

/* mnvsresults.c:965-971 0x80133810, verbatim */
s32 mnVSResultsGetSpot(s32 player)
{
    sb32 aheads[/* */] = { 0, 0, 1, 1 };
    sb32 places[/* */] = { 0, 0, 1, 1, 1 };

    return sMNVSResultsPlaces[player] + aheads[sMNVSResultsPlaces[player] - mnVSResultsGetPlayerCountAhead(player)] + places[mnVSResultsGetPlayerCountPlace(sMNVSResultsPlaces[player])];
}

/* mnvsresults.c:974-992, verbatim: the fighter's size on the
 * podium, and the fighter itself, a Demo out of the battle state's kind,
 * costume and shade. */
void mnVSResultsSetFighterScale(GObj *fighter_gobj, s32 player, s32 fkind, s32 place)
{
	DObjGetStruct(fighter_gobj)->scale.vec.f.x = dSCSubsysFighterScales[fkind];
	DObjGetStruct(fighter_gobj)->scale.vec.f.y = dSCSubsysFighterScales[fkind];
	DObjGetStruct(fighter_gobj)->scale.vec.f.z = dSCSubsysFighterScales[fkind];
}

// 0x8013392C
void mnVSResultsMakeFighter(s32 player)
{
	s32 unused[3];
	FTDesc desc = dFTManagerDefaultFighterDesc;

	desc.fkind = mnVSResultsGetFighterKind(player);
	desc.costume = gSCManagerTransferBattleState.players[player].costume;
	desc.shade = gSCManagerTransferBattleState.players[player].shade;
	desc.figatree_heap = sMNVSResultsFigatreeHeaps[player];
	sMNVSResultsFighterGObjs[player] = ftManagerMakeFighter(&desc);
}

/* mnvsresults.c:995-1045 0x801339F4, verbatim. Three tables of four
 * spots each, picked by how many players there are; the fourth column
 * of each is the spot's own, and pos_y_kinds is the per-character nudge
 * the game left at zero for every one of the twelve. */
void mnVSResultsSetPlayerTagPosition(GObj *gobj, s32 player)
{
    s32 spot, dist;

    Vec2f pos_xy_2p[/* */][4] =
    {
        { { 115.0F, 50.0F }, { 112.0F, 75.0F }, { 115.0F, 96.0F }, { 115.0F, 103.0F } },
        { { 173.0F, 50.0F }, { 177.0F, 75.0F }, { 183.0F, 96.0F }, { 186.0F, 103.0F } }
    };
    Vec2f pos_xy_3p[/* */][4] =
    {
        { {  38.0F, 50.0F }, { 50.0F,  75.0F }, {  38.0F, 96.0F }, { 38.0F,  103.0F } },
        { { 150.0F, 50.0F }, { 150.0F, 75.0F }, { 150.0F, 96.0F }, { 150.0F, 103.0F } },
        { { 245.0F, 50.0F }, { 237.0F, 75.0F }, { 254.0F, 96.0F }, { 258.0F, 103.0F } }
    };
    Vec2f pos_xy_4p[/* */][4] =
    {
        { {  38.0F, 50.0F }, {  50.0F, 75.0F }, {  35.0F, 96.0F }, {  35.0F, 103.0F } },
        { { 115.0F, 50.0F }, { 112.0F, 75.0F }, { 115.0F, 96.0F }, { 115.0F, 103.0F } },
        { { 173.0F, 50.0F }, { 177.0F, 75.0F }, { 188.0F, 96.0F }, { 186.0F, 103.0F } },
        { { 245.0F, 50.0F }, { 237.0F, 75.0F }, { 258.0F, 96.0F }, { 258.0F, 103.0F } }
    };
    f32 pos_y_kinds[/* */][4] =
    {
        { 0.0F, 0.0F, 0.0F, 0.0F }, { 0.0F, 0.0F, 0.0F, 0.0F }, { 0.0F, 0.0F, 0.0F, 0.0F }, { 0.0F, 0.0F, 0.0F, 0.0F },
        { 0.0F, 0.0F, 0.0F, 0.0F }, { 0.0F, 0.0F, 0.0F, 0.0F }, { 0.0F, 0.0F, 0.0F, 0.0F }, { 0.0F, 0.0F, 0.0F, 0.0F },
        { 0.0F, 0.0F, 0.0F, 0.0F }, { 0.0F, 0.0F, 0.0F, 0.0F }, { 0.0F, 0.0F, 0.0F, 0.0F }, { 0.0F, 0.0F, 0.0F, 0.0F }
    };

    spot = mnVSResultsGetSpot(player);
    dist = mnVSResultsGetPlayerDistanceID(player);

    switch (mnVSResultsGetPresentCount())
    {
    case 2:
        SObjGetStruct(gobj)->pos.x = pos_xy_2p[dist][spot].x;
        SObjGetStruct(gobj)->pos.y = pos_xy_2p[dist][spot].y + pos_y_kinds[mnVSResultsGetFighterKind(player)][dist];
        break;

    case 3:
        SObjGetStruct(gobj)->pos.x = pos_xy_3p[dist][spot].x;
        SObjGetStruct(gobj)->pos.y = pos_xy_3p[dist][spot].y + pos_y_kinds[mnVSResultsGetFighterKind(player)][dist];
        break;

    case 4:
    default:
        SObjGetStruct(gobj)->pos.x = pos_xy_4p[dist][spot].x;
        SObjGetStruct(gobj)->pos.y = pos_xy_4p[dist][spot].y + pos_y_kinds[mnVSResultsGetFighterKind(player)][dist];
        break;
    }
}

/* mnvsresults.c:1048-1095 0x80133C58, verbatim: 1P..4P for a human, CP
 * for a computer, in the player's colour. */
void mnVSResultsMakePlayerTag(s32 player, s32 color_id)
{
    GObj *gobj;
    SObj *sobj;

    SYColorRGBPair colors[/* */] =
    {
        { { 0x00, 0x00, 0x00 }, { 0xED, 0x36, 0x36 } },
        { { 0x00, 0x00, 0x00 }, { 0x4E, 0x4E, 0xE9 } },
        { { 0x00, 0x00, 0x00 }, { 0xFF, 0xDF, 0x1A } },
        { { 0x00, 0x00, 0x00 }, { 0x4E, 0xB9, 0x4E } }
    };
    intptr_t offsets[/* */] =
    {
        llIFCommonPlayerTags1PSprite, llIFCommonPlayerTags2PSprite,
        llIFCommonPlayerTags3PSprite, llIFCommonPlayerTags4PSprite
    };

    gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    if (gSCManagerTransferBattleState.players[player].pkind == nFTPlayerKindMan)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSResultsFiles[1], offsets[player]));
        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;
        sobj->envcolor.r = dIFCommonPlayerTagEnvColorsR[color_id];
        sobj->envcolor.g = dIFCommonPlayerTagEnvColorsG[color_id];
        sobj->envcolor.b = dIFCommonPlayerTagEnvColorsB[color_id];
        sobj->sprite.red = dIFCommonPlayerTagPrimColorsR[color_id];
        sobj->sprite.green = dIFCommonPlayerTagPrimColorsG[color_id];
        sobj->sprite.blue = dIFCommonPlayerTagPrimColorsB[color_id];
    }
    else
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSResultsFiles[1], llIFCommonPlayerTagsCPSprite));
        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;
        sobj->envcolor.r = dIFCommonPlayerTagEnvColorsR[color_id];
        sobj->envcolor.g = dIFCommonPlayerTagEnvColorsG[color_id];
        sobj->envcolor.b = dIFCommonPlayerTagEnvColorsB[color_id];
        sobj->sprite.red = dIFCommonPlayerTagPrimColorsR[color_id];
        sobj->sprite.green = dIFCommonPlayerTagPrimColorsG[color_id];
        sobj->sprite.blue = dIFCommonPlayerTagPrimColorsB[color_id];
    }

    mnVSResultsSetPlayerTagPosition(gobj, player);
}

/* mnvsresults.c:1098-1120 0x80133E7C, verbatim */
void mnVSResultsMakePlayerTagCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            30,
            COBJ_MASK_DLLINK(27),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mnvsresults.c:1123-1139 0x80133F1C, verbatim: the announcer's
 * alphabet is A..Z then ! . and space, so a letter is its distance from
 * 'A' and the three others are named. */
s32 mnVSResultsGetCharacterID(char c)
{
    switch (c)
    {
    case '!':
        return 0x1A;

    case '.':
        return 0x1B;

    case ' ':
        return 0x1C;

    default:
        return c - 'A';
    }
}

/* mnvsresults.c:1142-1226 0x80133F6C, verbatim. One GObj holds the
 * whole string, one SObj a letter. The digits in the strings below are
 * not drawn: a digit in the source string is a kerning nudge, added to
 * the pen and skipped, which is how "LU1I1G1I" spells LUIGI with its
 * two I's pulled a pixel apart. */
void mnVSResultsMakeString(const char *str, f32 x, f32 y, s32 color_id, f32 scale)
{
    GObj *gobj;
    SObj *sobj;
    s32 i;
    f32 current_x;
    s32 char_id;

    f32 widths[/* */] =
    {
        35.0F, 24.0F, 24.0F, 28.0F, 22.0F, 20.0F, 31.0F, 27.0F, 9.0F, 20.0F, 27.0F, 20.0F, 37.0F, 29.0F,
        34.0F, 24.0F, 37.0F, 27.0F, 24.0F, 24.0F, 26.0F, 28.0F, 39.0F, 31.0F, 29.0F, 30.0F, 10.0F, 8.0F
    };
    intptr_t offsets[/* */] =
    {
        llIFCommonAnnounceCommonLetterASprite, llIFCommonAnnounceCommonLetterBSprite,
        llIFCommonAnnounceCommonLetterCSprite, llIFCommonAnnounceCommonLetterDSprite,
        llIFCommonAnnounceCommonLetterESprite, llIFCommonAnnounceCommonLetterFSprite,
        llIFCommonAnnounceCommonLetterGSprite, llIFCommonAnnounceCommonLetterHSprite,
        llIFCommonAnnounceCommonLetterISprite, llIFCommonAnnounceCommonLetterJSprite,
        llIFCommonAnnounceCommonLetterKSprite, llIFCommonAnnounceCommonLetterLSprite,
        llIFCommonAnnounceCommonLetterMSprite, llIFCommonAnnounceCommonLetterNSprite,
        llIFCommonAnnounceCommonLetterOSprite, llIFCommonAnnounceCommonLetterPSprite,
        llIFCommonAnnounceCommonLetterQSprite, llIFCommonAnnounceCommonLetterRSprite,
        llIFCommonAnnounceCommonLetterSSprite, llIFCommonAnnounceCommonLetterTSprite,
        llIFCommonAnnounceCommonLetterUSprite, llIFCommonAnnounceCommonLetterVSprite,
        llIFCommonAnnounceCommonLetterWSprite, llIFCommonAnnounceCommonLetterXSprite,
        llIFCommonAnnounceCommonLetterYSprite, llIFCommonAnnounceCommonLetterZSprite,

        llIFCommonAnnounceCommonSymbolExclaimSprite,
        llIFCommonAnnounceCommonSymbolPeriodSprite
    };
    SYColorRGBPair colors[/* */] =
    {
        { { 0xFF, 0x00, 0x00 }, { 0xFF, 0xFF, 0xFF } },
        { { 0x12, 0x00, 0xD9 }, { 0xFF, 0xFF, 0xFF } },
        { { 0x03, 0x73, 0x00 }, { 0xFF, 0xFF, 0xFF } },
        { { 0x60, 0x03, 0xD4 }, { 0xFF, 0xFF, 0xFF } },
        { { 0x00, 0x00, 0x00 }, { 0xFF, 0xFF, 0xFF } }
    };

    current_x = x;
    gobj = gcMakeGObjSPAfter(0, NULL, 20, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 29, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; str[i] != '\0'; i++)
    {
        if ((str[i] >= '0') && (str[i] <= '9'))
        {
            current_x += str[i] - '0';
        }
        else
        {
            char_id = mnVSResultsGetCharacterID(str[i]);

            if (char_id == 0x1C) // space
            {
                current_x += 10.0F * scale;
            }
            else
            {
                sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSResultsFiles[6], offsets[char_id]));
                sobj->sprite.scalex = scale;
                sobj->pos.x = current_x;

                if (char_id == 0x1B) // .
                {
                    sobj->pos.y = y + 26.0F;
                }
                else sobj->pos.y = y;

                sobj->sprite.attr &= ~SP_FASTCOPY;
                sobj->sprite.attr |= SP_TRANSPARENT;
                sobj->envcolor.r = colors[color_id].prim.r;
                sobj->envcolor.g = colors[color_id].prim.g;
                sobj->envcolor.b = colors[color_id].prim.b;
                sobj->sprite.red = colors[color_id].env.r;
                sobj->sprite.green = colors[color_id].env.g;
                sobj->sprite.blue = colors[color_id].env.b;

                current_x += widths[char_id] * scale;
            }
        }
    }
}

/* mnvsresults.c:1229-1283 0x8013423C, REGION_US: WINS!, at an x the
 * winner's name ends at. */
void mnVSResultsMakeWinnerText(s32 winner)
{
    char win[/* */] = "W1I1N1!";
    char wins[/* */] = "W1I1N1S1!";

    f32 x_fkinds[/* */] =
    {
        175.0F,
        160.0F,
        150.0F,
        176.0F,
        163.0F,
        160.0F,
        170.0F,
        178.0F,
        165.0F,
        172.0F,
        173.0F,
        160.0F
    };
    f32 x_teams[/* */] =
    {
        160.0F,
        170.0F,
        180.0F
    };

    if (sMNVSResultsIsTeamBattle == TRUE)
    {
        mnVSResultsMakeString(wins, x_teams[winner], 180.0F, 3, 1.0F);
    }
    if (sMNVSResultsIsTeamBattle == FALSE)
    {
        mnVSResultsMakeString(wins, x_fkinds[winner], 180.0F, 3, 1.0F);
    }
}

/* mnvsresults.c:1286-1289 0x80134364, verbatim */
s32 mnVSResultGetWinFighterKind(void)
{
    return mnVSResultsGetFighterKind(mnVSResultsGetWinPlayer());
}

/* mnvsresults.c:1292-1373 0x8013438C, REGION_US arms: the winner's name
 * and WINS! after it. */
void mnVSResultMakeFighterName(void)
{
    s32 fkind;

    char *names[/* */] =
    {
        "MARIO",
        "FOX",
        "D3K",
        "SAMUS",
        "LU1I1G1I",
        "L1I1N1K",
        "YOSH3I",
        "C2.2FA1L1C1O1N",
        "K1I1RBY",
        "P4I4KAC3H3U",
        "JIGGLYPUFF",
        "N2E2S2S"
    };
    f32 pos_x[/* */] =
    {
        30.0F,
        60.0F,
        70.0F,
        25.0F,
        50.0F,
        55.0F,
        30.0F,
        27.0F,
        40.0F,
        30.0F,
        27.0F,
        50.0F
    };
    f32 scales[/* */] = {

        1.0F,
        1.0F,
        1.0F,
        1.0F,
        1.0F,
        1.0F,
        1.0F,
        0.7F,
        1.0F,
        0.7F,
        0.6F,
        1.0F
    };

    fkind = mnVSResultGetWinFighterKind();

    mnVSResultsMakeString(names[fkind], pos_x[fkind], 180.0F, 0, scales[fkind]);
    mnVSResultsMakeWinnerText(fkind);
}

/* mnvsresults.c:1376-1403 0x80134480, verbatim */
void mnVSResultMakeTeamName(void)
{
    u32 team;

    char *names[/* */] =
    {
        "RED",
        "BLUE",
        "GREEN",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    };
    f32 pos_x[/* */] =
    {
        70.0F, 60.0F, 30.0F,
         0.0F,  0.0F,  0.0F,
         0.0F,  0.0F,  0.0F
    };

    team = mnVSResultsGetWinTeam();

    mnVSResultsMakeString(names[team], pos_x[team], 180.0F, team, 1.0F);
    mnVSResultsMakeWinnerText(team);
}

/* mnvsresults.c:1406-1409 0x80134540, verbatim */
void mnVSResultMakeNoContestText(void)
{
    mnVSResultsMakeString("NO CONTEST", 30.0F, 180.0F, 4, 1.0F);
}

/* mnvsresults.c:1412-1429 0x8013457C, verbatim */
void mnVSResultsMakeResultsText(void)
{
    if (sMNVSResultsKind == nMNVSResultsKindNoContest)
    {
        mnVSResultMakeNoContestText();
    }
    else
    {
        if (sMNVSResultsIsTeamBattle == FALSE)
        {
            mnVSResultMakeFighterName();
        }
        if (sMNVSResultsIsTeamBattle == TRUE)
        {
            mnVSResultMakeTeamName();
        }
    }
}

/* mnvsresults.c:1432-1454 0x801345E8, verbatim */
void mnVSResultsMakeResultsTextCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            20,
            COBJ_MASK_DLLINK(29),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mnvsresults.c:1457-1484 0x80134688, :801346C0 and :80134718,
 * verbatim: the three digits of a signed score, sign carried on each. */
s32 mnVSResultsGetHundredsDigit(s32 number)
{
    if (number < 0)
    {
        return -(number / 100);
    }
    else return number / 100;
}

s32 mnVSResultsGetTensDigit(s32 number)
{
    if (number < 0)
    {
        return -((number % 100) / 10);
    }
    else return (number % 100) / 10;
}

s32 mnVSResultsGetOnesDigit(s32 number)
{
    if (number < 0)
    {
        return -((number % 100) % 10);
    }
    else return (number % 100) % 10;
}

/* mnvsresults.c:1487-1504 0x80134770, verbatim: white in a free-for-all
 * (colour 4), the team's tint in a team battle. */
void mnVSResultsSetNumberColor(SObj *sobj, s32 color_id)
{
    SYColorRGBPair colors[/* */] =
    {
        { { 0x00, 0x00, 0x00 }, { 0xFF, 0x82, 0x82 } },
        { { 0x00, 0x00, 0x00 }, { 0x91, 0xC0, 0xFF } },
        { { 0x00, 0x00, 0x00 }, { 0xFF, 0xDF, 0x1A } },
        { { 0x00, 0x00, 0x00 }, { 0x9F, 0xFF, 0x9F } },
        { { 0x00, 0x00, 0x00 }, { 0xFF, 0xFF, 0xFF } }
    };

    sobj->envcolor.r = colors[color_id].prim.r;
    sobj->envcolor.g = colors[color_id].prim.g;
    sobj->envcolor.b = colors[color_id].prim.b;
    sobj->sprite.red = colors[color_id].env.r;
    sobj->sprite.green = colors[color_id].env.g;
    sobj->sprite.blue = colors[color_id].env.b;
}

/* mnvsresults.c:1507-1534 0x80134808, verbatim: the score rows' digits,
 * out of file 36. */
SObj* mnVSResultsMakeDigit(GObj *gobj, s32 digit, s32 color_id)
{
    SObj *sobj;

    intptr_t offsets[/* */] =
    {
        llIFCommonDigits0Sprite, llIFCommonDigits1Sprite,
        llIFCommonDigits2Sprite, llIFCommonDigits3Sprite,
        llIFCommonDigits4Sprite, llIFCommonDigits5Sprite,
        llIFCommonDigits6Sprite, llIFCommonDigits7Sprite,
        llIFCommonDigits8Sprite, llIFCommonDigits9Sprite
    };
    SYColorRGBPair unused_colors[/* */] =
    {
        { { 0x00, 0x00, 0x00 }, { 0xED, 0x36, 0x36 } },
        { { 0x00, 0x00, 0x00 }, { 0x4E, 0x4E, 0xE9 } },
        { { 0x00, 0x00, 0x00 }, { 0xFF, 0xDF, 0x1A } },
        { { 0x00, 0x00, 0x00 }, { 0x4E, 0xB9, 0x4E } },
        { { 0x00, 0x00, 0x00 }, { 0x4E, 0xB9, 0x4E } }
    };

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSResultsFiles[5], offsets[digit]));
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;
    mnVSResultsSetNumberColor(sobj, color_id);

    return sobj;
}

/* mnvsresults.c:1537-1590 0x801348F8, verbatim: a place is one of the
 * HUD's fat damage digits, except first, which is the WINNER sprite --
 * and in a team battle only for the players on the winning team. */
SObj* mnVSResultsMakePlaceNumber(GObj *gobj, s32 player, s32 place, s32 color_id)
{
    SObj *sobj;

    intptr_t offsets[/* */] =
    {
        llIFCommonPlayerDamageDigit0Sprite, llIFCommonPlayerDamageDigit1Sprite,
        llIFCommonPlayerDamageDigit2Sprite, llIFCommonPlayerDamageDigit3Sprite,
        llIFCommonPlayerDamageDigit4Sprite, llIFCommonPlayerDamageDigit5Sprite,
        llIFCommonPlayerDamageDigit6Sprite, llIFCommonPlayerDamageDigit7Sprite,
        llIFCommonPlayerDamageDigit8Sprite, llIFCommonPlayerDamageDigit9Sprite
    };
    SYColorRGBPair unused_colors[/* */] =
    {
        { { 0x00, 0x00, 0x00 }, { 0xED, 0x36, 0x36 } },
        { { 0x00, 0x00, 0x00 }, { 0x4E, 0x4E, 0xE9 } },
        { { 0x00, 0x00, 0x00 }, { 0xFF, 0xDF, 0x1A } },
        { { 0x00, 0x00, 0x00 }, { 0x4E, 0xB9, 0x4E } },
        { { 0x00, 0x00, 0x00 }, { 0x4E, 0xB9, 0x4E } }
    };

    if (place == 1)
    {
        if (sMNVSResultsIsTeamBattle == TRUE)
        {
            if ((mnVSResultsGetWinPlayer() == player) || (sMNVSResultsIsSharedWinner[player] != FALSE))
            {
                sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSResultsFiles[0], llMNVSResultsWinnerSprite));
                sobj->user_data.s = 1;
            }
            else
            {
                sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSResultsFiles[3], llIFCommonPlayerDamageDigit1Sprite));
                sobj->user_data.s = 0;
                mnVSResultsSetNumberColor(sobj, color_id);
            }
        }
        else
        {
            sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSResultsFiles[0], llMNVSResultsWinnerSprite));
            sobj->user_data.s = 1;
        }
        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;
    }
    else
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSResultsFiles[3], offsets[place]));
        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;
        mnVSResultsSetNumberColor(sobj, color_id);
    }
    return sobj;
}

/* mnvsresults.c:1593-1640 0x80134AC4, verbatim: up to three digits,
 * right-aligned, with a minus sign in front of a negative score -- and
 * the sign's own x depends on how many digits follow it. */
SObj* mnVSResultsMakeNumber(GObj *gobj, f32 x, f32 y, s32 number, s32 color_id)
{
    SObj *sobj;
    s32 hundreds_digit;
    s32 tens_digit;

    if (number < 0)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSResultsFiles[5], llIFCommonDigitsDashSprite));

        if (mnVSResultsGetHundredsDigit(number) != 0)
        {
            sobj->pos.x = x;
        }
        else if (mnVSResultsGetTensDigit(number) != 0)
        {
            sobj->pos.x = x + 8.0F;
        }
        else sobj->pos.x = x + 16.0F;

        sobj->pos.y = y + 3.0F;
        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        mnVSResultsSetNumberColor(sobj, color_id);
    }
    hundreds_digit = mnVSResultsGetHundredsDigit(number);

    if (hundreds_digit != 0)
    {
        sobj = mnVSResultsMakeDigit(gobj, hundreds_digit, color_id);
        sobj->pos.x = x + 8.0F;
        sobj->pos.y = y;
    }
    tens_digit = mnVSResultsGetTensDigit(number);

    if ((tens_digit != 0) || (hundreds_digit != 0))
    {
        sobj = mnVSResultsMakeDigit(gobj, tens_digit, color_id);
        sobj->pos.x = x + 16.0F;
        sobj->pos.y = y;
    }
    sobj = mnVSResultsMakeDigit(gobj, mnVSResultsGetOnesDigit(number), color_id);
    sobj->pos.x = x + 24.0F;
    sobj->pos.y = y;

    return sobj;
}

/* mnvsresults.c:1643-1664 0x80134C5C, the tint that darkens the
 * wallpaper behind the ranked rows: black, fading up nine steps a tic
 * to half. Emitted in G_CYC_1CYCLE, where the lower-right corner is
 * exclusive, which is the corner lbCommonSpriteFillRect takes -- so the
 * numbers are the decomp's unchanged (contrast the bar and the label
 * below, which are FILL). */
void mnVSResultsTintProcDisplay(GObj *gobj)
{
    if (sMNVSResultsTintAlpha < 0x80)
    {
        sMNVSResultsTintAlpha += 0x09;

        if (sMNVSResultsTintAlpha > 0x80)
        {
            sMNVSResultsTintAlpha = 0x80;
        }
    }
    lbCommonSpriteFillRect(10, 10, 310, 230, 0x00, 0x00, 0x00, sMNVSResultsTintAlpha);

    lbCommonClearExternSpriteParams();
}

/* mnvsresults.c:1667-1671 0x80134DA0, verbatim */
void mnVSResultsMakeTint(void)
{
    sMNVSResultsTintAlpha = 0x00;
    gcAddGObjDisplay(gcMakeGObjSPAfter(0, NULL, 21, GOBJ_PRIORITY_DEFAULT), mnVSResultsTintProcDisplay, 30, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mnvsresults.c:1674-1696 0x80134DF4, verbatim */
void mnVSResultsMakeTintCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            17,
            COBJ_MASK_DLLINK(30),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mnvsresults.c:1699-1719 0x80134E94 and :1753-1773 0x801350C8: the two
 * black sheets the screen fades in from, five steps a tic, each
 * ejecting itself when it reaches zero. They are two and not one
 * because they start at different tics -- the first with the scene, the
 * second with the wallpaper -- and the second is skipped on a no
 * contest. 1-cycle, as the tint above. */
void mnVSResultsWallpaperTintProcDisplay(GObj *gobj)
{
    if (sMNVSResultsWallpaperTintAlpha > 0x00)
    {
        sMNVSResultsWallpaperTintAlpha -= 0x05;

        if (sMNVSResultsWallpaperTintAlpha < 0x00)
        {
            gcEjectGObj(gobj);
        }
    }
    lbCommonSpriteFillRect(10, 10, 310, 230, 0x00, 0x00, 0x00, sMNVSResultsWallpaperTintAlpha);
}

/* mnvsresults.c:1721-1725 0x80134FD0, verbatim */
void mnVSResultsMakeWallpaperTint(void)
{
    sMNVSResultsWallpaperTintAlpha = 0xFF;
    gcAddGObjDisplay(gcMakeGObjSPAfter(0, NULL, 25, GOBJ_PRIORITY_DEFAULT), mnVSResultsWallpaperTintProcDisplay, 35, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mnvsresults.c:1728-1750 0x80135028, verbatim */
void mnVSResultsMakeWallpaperTintCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            55,
            COBJ_MASK_DLLINK(35),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mnvsresults.c:1753-1773 0x801350C8, verbatim */
void mnVSResultsWallpaperTint2ProcDisplay(GObj *gobj)
{
    if (sMNVSResultsWallpaperTint2Alpha > 0x00)
    {
        sMNVSResultsWallpaperTint2Alpha -= 0x05;

        if (sMNVSResultsWallpaperTint2Alpha < 0x00)
        {
            gcEjectGObj(gobj);
        }
    }
    lbCommonSpriteFillRect(10, 10, 310, 230, 0x00, 0x00, 0x00, sMNVSResultsWallpaperTint2Alpha);
}

/* mnvsresults.c:1775-1779 0x80135204, verbatim */
void mnVSResultsMakeWallpaperTint2(void)
{
    sMNVSResultsWallpaperTint2Alpha = 0xFF;
    gcAddGObjDisplay(gcMakeGObjSPAfter(0, NULL, 24, GOBJ_PRIORITY_DEFAULT), mnVSResultsWallpaperTint2ProcDisplay, 34, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mnvsresults.c:1782-1804 0x8013525C, verbatim */
void mnVSResultsMakeWallpaperTint2Camera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            70,
            COBJ_MASK_DLLINK(34),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mnvsresults.c:1807-1825 0x801352FC, verbatim: the x of a player's
 * column, spaced by how many players there are. */
f32 mnVSResultsGetColumnX(s32 player)
{
    f32 column_x_2p[/* */] = { 135.0F, 215.0F };
    f32 column_x_3p[/* */] = { 125.0F, 175.0F, 225.0F };
    f32 column_x_4p[/* */] = { 115.0F, 155.0F, 195.0F, 235.0F };

    switch (mnVSResultsGetPresentCount())
    {
    case 2:
        return column_x_2p[mnVSResultsGetPresentLowerCount(player)];

    case 3:
        return column_x_3p[mnVSResultsGetPresentLowerCount(player)];

    case 4:
    default:
        return column_x_4p[mnVSResultsGetPresentLowerCount(player)];
    }
}

/* mnvsresults.c:1828-1837 0x801353F4, verbatim */
s32 mnVSResultsGetNumberColorID(s32 player)
{
    s32 color_ids[/* */] = { 0, 1, 3 };

    if (sMNVSResultsIsTeamBattle != TRUE)
    {
        return 4;
    }
    return color_ids[gSCManagerTransferBattleState.players[player].team];
}

/* mnvsresults.c:1840-1850 0x80135468, verbatim */
void mnVSResultsSetPlayerArrowColors(SObj *sobj)
{
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;
    sobj->envcolor.r = 0x00;
    sobj->envcolor.g = 0x00;
    sobj->envcolor.b = 0x00;
    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0xFF;
    sobj->sprite.blue = 0xFF;
}

/* mnvsresults.c:1853-1889 0x8013549C, the column heads: each present
 * player's arrow with their stock icon beside it -- the icon comes off the fighter's
 * own attributes and costume. Every kind's pack carries its FTSprites
 * (src/dc/ftmanager.c), so the game's unguarded reach holds. */
void mnVSResultsMakeHeader(void)
{
    SObj *stock_sobj;
    SObj *arrow_sobj;
    GObj *gobj;

    intptr_t offsets[/* */] =
    {
        llMNVSResults1PArrowSprite, llMNVSResults2PArrowSprite,
        llMNVSResults3PArrowSprite, llMNVSResults4PArrowSprite
    };
    s32 i;
    FTStruct *fp;

    gobj = gcMakeGObjSPAfter(0, NULL, 22, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 31, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; i < (ARRAY_COUNT(sMNVSResultsIsPresent) + ARRAY_COUNT(sMNVSResultsFighterGObjs)) / 2; i++)
    {
        if (sMNVSResultsIsPresent[i] != FALSE)
        {
            arrow_sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSResultsFiles[0], offsets[i]));
            arrow_sobj->pos.x = mnVSResultsGetColumnX(i) + 17.0F;
            arrow_sobj->pos.y = 49.0F;
            mnVSResultsSetPlayerArrowColors(arrow_sobj);

            fp = ftGetStruct(sMNVSResultsFighterGObjs[i]);

            stock_sobj = lbCommonMakeSObjForGObj(gobj, fp->attr->sprites->stock_sprite);
            stock_sobj->sprite.LUT = fp->attr->sprites->stock_luts[fp->costume];
            stock_sobj->sprite.attr &= ~SP_FASTCOPY;
            stock_sobj->sprite.attr |= SP_TRANSPARENT;
            stock_sobj->pos.x = arrow_sobj->pos.x - 10.0F;
            stock_sobj->pos.y = arrow_sobj->pos.y;
        }
    }
}

/* mnvsresults.c:1902-1951 0x8013569C, verbatim: the KOs row -- its
 * label, then one number per present player under their column. (The
 * decomp writes the label's colour black and then white; both stand.) */
void mnVSResultsMakeKOs(s32 y)
{
    GObj *gobj = lbCommonMakeSpriteGObj
    (
        0,
        NULL,
        22,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSObjAttr,
        31,
        GOBJ_PRIORITY_DEFAULT,
        ~0,
        lbRelocGetFileData
        (
            Sprite*,
            sMNVSResultsFiles[0],
            llMNVSResultsKOsTextSprite
        ),
        nGCProcessKindFunc,
        NULL,
        1
    );
    SObjGetStruct(gobj)->pos.x = 26.0F;
    SObjGetStruct(gobj)->pos.y = y;
    SObjGetStruct(gobj)->sprite.attr &= ~SP_FASTCOPY;
    SObjGetStruct(gobj)->sprite.attr |= SP_TRANSPARENT;
    SObjGetStruct(gobj)->sprite.red = 0x00;
    SObjGetStruct(gobj)->sprite.green = 0x00;
    SObjGetStruct(gobj)->sprite.blue = 0x00;
    SObjGetStruct(gobj)->sprite.red = 0xFF;
    SObjGetStruct(gobj)->sprite.green = 0xFF;
    SObjGetStruct(gobj)->sprite.blue = 0xFF;

    if (sMNVSResultsIsPresent[0] != FALSE)
    {
        mnVSResultsMakeNumber(gobj, mnVSResultsGetColumnX(0), y, mnVSResultsGetKOs(0), mnVSResultsGetNumberColorID(0));
    }
    if (sMNVSResultsIsPresent[1] != FALSE)
    {
        mnVSResultsMakeNumber(gobj, mnVSResultsGetColumnX(1), y, mnVSResultsGetKOs(1), mnVSResultsGetNumberColorID(1));
    }
    if (sMNVSResultsIsPresent[2] != FALSE)
    {
        mnVSResultsMakeNumber(gobj, mnVSResultsGetColumnX(2), y, mnVSResultsGetKOs(2), mnVSResultsGetNumberColorID(2));
    }
    if (sMNVSResultsIsPresent[3] != FALSE)
    {
        mnVSResultsMakeNumber(gobj, mnVSResultsGetColumnX(3), y, mnVSResultsGetKOs(3), mnVSResultsGetNumberColorID(3));
    }
}

/* mnvsresults.c:1964-2021 0x801358F0, verbatim: the falls row, with the
 * dash the game draws between the label and the columns. */
void mnVSResultsMakeTKO(s32 y)
{
    GObj *gobj = lbCommonMakeSpriteGObj
    (
        0,
        NULL,
        22,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSObjAttr,
        31,
        GOBJ_PRIORITY_DEFAULT,
        ~0,
        lbRelocGetFileData
        (
            Sprite*,
            sMNVSResultsFiles[0],
            llMNVSResultsTKOTextSprite
        ),
        nGCProcessKindFunc,
        NULL,
        1
    );
    SObjGetStruct(gobj)->pos.x = 26.0F;
    SObjGetStruct(gobj)->pos.y = y;
    SObjGetStruct(gobj)->sprite.attr &= ~SP_FASTCOPY;
    SObjGetStruct(gobj)->sprite.attr |= SP_TRANSPARENT;
    SObjGetStruct(gobj)->sprite.red = 0x00;
    SObjGetStruct(gobj)->sprite.green = 0x00;
    SObjGetStruct(gobj)->sprite.blue = 0x00;
    SObjGetStruct(gobj)->sprite.red = 0xFF;
    SObjGetStruct(gobj)->sprite.green = 0xFF;
    SObjGetStruct(gobj)->sprite.blue = 0xFF;

    if (sMNVSResultsKind != nMNVSResultsKindNoContest)
    {
        SObj *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSResultsFiles[5], llIFCommonDigitsDashSprite));
        sobj->pos.x = 90.0F;
        sobj->pos.y = y + 3;
        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;
    }
    if (sMNVSResultsIsPresent[0] != FALSE)
    {
        mnVSResultsMakeNumber(gobj, mnVSResultsGetColumnX(0), y, mnVSResultsGetTKO(0), mnVSResultsGetNumberColorID(0));
    }
    if (sMNVSResultsIsPresent[1] != FALSE)
    {
        mnVSResultsMakeNumber(gobj, mnVSResultsGetColumnX(1), y, mnVSResultsGetTKO(1), mnVSResultsGetNumberColorID(1));
    }
    if (sMNVSResultsIsPresent[2] != FALSE)
    {
        mnVSResultsMakeNumber(gobj, mnVSResultsGetColumnX(2), y, mnVSResultsGetTKO(2), mnVSResultsGetNumberColorID(2));
    }
    if (sMNVSResultsIsPresent[3] != FALSE)
    {
        mnVSResultsMakeNumber(gobj, mnVSResultsGetColumnX(3), y, mnVSResultsGetTKO(3), mnVSResultsGetNumberColorID(3));
    }
}

/* mnvsresults.c:2024-2043 0x80135B78, the white rule that grows ten
 * pixels a tic under the two score rows to 190 wide.
 *
 * DIVERGES, and it is the same deviation as the VS options' underline:
 * the rectangle is emitted in G_CYC_FILL, where the RDP
 * takes the lower-right corner as *inclusive* -- which is why
 * sys/objdisplay.c:2657 writes `lrx--, lry--` before its own fill rects,
 * and why this rectangle is one pixel tall with uly == lry. The port's
 * lbCommonSpriteFillRect takes the corner as exclusive, so the quad is
 * drawn one further right and down and the rule keeps its pixel. */
void mnVSResultsBarProcDisplay(GObj *gobj)
{
    f32 y = gobj->user_data.s;

    sMNVSResultsBarWidth += 10;

    if (sMNVSResultsBarWidth > 190)
    {
        sMNVSResultsBarWidth = 190;
    }
    lbCommonSpriteFillRect(87, (s32)y, 87 + sMNVSResultsBarWidth + 1, (s32)y + 1,
                           0xFF, 0xFF, 0xFF, 0xFF);

    lbCommonClearExternSpriteParams();
}

/* mnvsresults.c:2046-2051 0x80135D58, verbatim */
void mnVSResultsMakeBar(s32 y)
{
    GObj *gobj = gcMakeGObjSPAfter(0, NULL, 22, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, mnVSResultsBarProcDisplay, 31, GOBJ_PRIORITY_DEFAULT, ~0);
    gobj->user_data.s = y;
}

/* mnvsresults.c:2054-2057 0x80135DB8, verbatim */
s32 mnVSResultsGetPoints(s32 player)
{
    return sMNVSResultsPoints[player];
}

/* mnvsresults.c:2060-2109 0x80135DCC, verbatim: KOs minus falls, under
 * the rule. */
void mnVSResultsMakePointsRow(void)
{
    GObj *gobj = lbCommonMakeSpriteGObj
    (
        0,
        NULL,
        22,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSObjAttr,
        31,
        GOBJ_PRIORITY_DEFAULT,
        ~0,
        lbRelocGetFileData
        (
            Sprite*,
            sMNVSResultsFiles[0],
            llMNVSResultsPtsTextSprite
        ),
        nGCProcessKindFunc,
        NULL,
        1
    );
    SObjGetStruct(gobj)->pos.x = 26.0F;
    SObjGetStruct(gobj)->pos.y = 104.0F;
    SObjGetStruct(gobj)->sprite.attr &= ~SP_FASTCOPY;
    SObjGetStruct(gobj)->sprite.attr |= SP_TRANSPARENT;
    SObjGetStruct(gobj)->sprite.red = 0x00;
    SObjGetStruct(gobj)->sprite.green = 0x00;
    SObjGetStruct(gobj)->sprite.blue = 0x00;
    SObjGetStruct(gobj)->sprite.red = 0xFF;
    SObjGetStruct(gobj)->sprite.green = 0xFF;
    SObjGetStruct(gobj)->sprite.blue = 0xFF;

    if (sMNVSResultsIsPresent[0] != FALSE)
    {
        mnVSResultsMakeNumber(gobj, mnVSResultsGetColumnX(0), 104.0F, mnVSResultsGetPoints(0), mnVSResultsGetNumberColorID(0));
    }
    if (sMNVSResultsIsPresent[1] != FALSE)
    {
        mnVSResultsMakeNumber(gobj, mnVSResultsGetColumnX(1), 104.0F, mnVSResultsGetPoints(1), mnVSResultsGetNumberColorID(1));
    }
    if (sMNVSResultsIsPresent[2] != FALSE)
    {
        mnVSResultsMakeNumber(gobj, mnVSResultsGetColumnX(2), 104.0F, mnVSResultsGetPoints(2), mnVSResultsGetNumberColorID(2));
    }
    if (sMNVSResultsIsPresent[3] != FALSE)
    {
        mnVSResultsMakeNumber(gobj, mnVSResultsGetColumnX(3), 104.0F, mnVSResultsGetPoints(3), mnVSResultsGetNumberColorID(3));
    }
}

/* mnvsresults.c:2118-2130 0x80135FF0, verbatim: WINNER is wider than a
 * digit, so it sits thirteen pixels further left. */
void mnVSResultsSetPlacePosition(SObj *sobj, s32 player, s32 place, f32 y)
{
    if ((place == 1) && (sobj->user_data.s != 0))
    {
        sobj->pos.x = mnVSResultsGetColumnX(player) + 2.0F;
        sobj->pos.y = y;
    }
    else
    {
        sobj->pos.x = mnVSResultsGetColumnX(player) + 15.0F;
        sobj->pos.y = y;
    }
}

/* mnvsresults.c:2133-2146 0x8013607C, verbatim: four players, two of
 * them tied for second, and the last is shown fourth rather than third. */
s32 mnVSResultsGetDisplayPlace(s32 player)
{
    if
    (
        (mnVSResultsGetPresentCount() == GMCOMMON_PLAYERS_MAX) &&
        (sMNVSResultsIsTeamBattle == FALSE) &&
        (mnVSResultsGetPlayerCountPlace(1) == 2) &&
        (sMNVSResultsPlaces[player] == 2)
    )
    {
        return GMCOMMON_PLAYERS_MAX;
    }
    else return sMNVSResultsPlaces[player] + 1;
}

/* mnvsresults.c:2149-2204 0x80136100, verbatim */
void mnVSResultsMakePlaceRow(s32 y)
{
    GObj *gobj;
    s32 i;

    gobj = lbCommonMakeSpriteGObj
    (
        0,
        NULL,
        22,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSObjAttr,
        31,
        GOBJ_PRIORITY_DEFAULT,
        ~0,
        lbRelocGetFileData
        (
            Sprite*,
            sMNVSResultsFiles[0],
            llMNVSResultsPlaceTextSprite
        ),
        nGCProcessKindFunc,
        NULL,
        1
    );
    SObjGetStruct(gobj)->pos.x = 10.0F;
    SObjGetStruct(gobj)->pos.y = y;
    SObjGetStruct(gobj)->sprite.attr &= ~SP_FASTCOPY;
    SObjGetStruct(gobj)->sprite.attr |= SP_TRANSPARENT;
    SObjGetStruct(gobj)->sprite.red = 0x00;
    SObjGetStruct(gobj)->sprite.green = 0x00;
    SObjGetStruct(gobj)->sprite.blue = 0x00;
    SObjGetStruct(gobj)->sprite.red = 0xFF;
    SObjGetStruct(gobj)->sprite.green = 0xFF;
    SObjGetStruct(gobj)->sprite.blue = 0xFF;

    for (i = 0; i < ARRAY_COUNT(sMNVSResultsIsPresent); i++)
    {
        if (sMNVSResultsIsPresent[i] != FALSE)
        {
            mnVSResultsSetPlacePosition
            (
                mnVSResultsMakePlaceNumber
                (
                    gobj,
                    i,
                    mnVSResultsGetDisplayPlace(i),
                    mnVSResultsGetNumberColorID(i)
                ),
                i,
                mnVSResultsGetDisplayPlace(i),
                y
            );
        }
    }
}

/* mnvsresults.c:2207-2326, the five schedules the label's process runs,
 * one per results kind, verbatim. This is the screen's second clock:
 * the rows arrive twenty tics apart, in an order that differs between a
 * time match (KOs first, places last) and a stock one (places first). */
void mnVSResultsDrawResultsTimeRoyal(GObj *gobj)
{
    if (sMNVSResultsTotalTimeTics == 180)
    {
        mnVSResultsMakeTint();
    }
    if (sMNVSResultsTotalTimeTics == 210)
    {
        mnVSResultsMakeHeader();
        mnVSResultsMakeKOs(66);
    }
    if (sMNVSResultsTotalTimeTics == 230)
    {
        mnVSResultsMakeTKO(81);
    }
    if (sMNVSResultsTotalTimeTics == 250)
    {
        mnVSResultsMakeBar(98);
    }
    if (sMNVSResultsTotalTimeTics == 270)
    {
        mnVSResultsMakePointsRow();
    }
    if (sMNVSResultsTotalTimeTics == 290)
    {
        mnVSResultsMakePlaceRow(124);
    }
}

void mnVSResultsDrawResultsStockRoyal(GObj *gobj)
{
    if (sMNVSResultsTotalTimeTics == 180)
    {
        mnVSResultsMakeTint();
    }
    if (sMNVSResultsTotalTimeTics == 210)
    {
        mnVSResultsMakePlaceRow(66);
    }
    if (sMNVSResultsTotalTimeTics == 230)
    {
        mnVSResultsMakeBar(110);
    }
    if (sMNVSResultsTotalTimeTics == 250)
    {
        mnVSResultsMakeHeader();
        mnVSResultsMakeKOs(124);
    }
}

void mnVSResultsDrawResultsTimeTeam(GObj *gobj)
{
    if (sMNVSResultsTotalTimeTics == 180)
    {
        mnVSResultsMakeTint();
    }
    if (sMNVSResultsTotalTimeTics == 210)
    {
        mnVSResultsMakeHeader();
        mnVSResultsMakeKOs(66);
    }
    if (sMNVSResultsTotalTimeTics == 230)
    {
        mnVSResultsMakeTKO(81);
    }
    if (sMNVSResultsTotalTimeTics == 250)
    {
        mnVSResultsMakeBar(98);
    }
    if (sMNVSResultsTotalTimeTics == 270)
    {
        mnVSResultsMakePointsRow();
    }
    if (sMNVSResultsTotalTimeTics == 290)
    {
        mnVSResultsMakePlaceRow(124);
    }
}

void mnVSResultsDrawResultsStockTeam(GObj *gobj)
{
    if (sMNVSResultsTotalTimeTics == 180)
    {
        mnVSResultsMakeTint();
    }
    if (sMNVSResultsTotalTimeTics == 210)
    {
        mnVSResultsMakePlaceRow(66);
    }
    if (sMNVSResultsTotalTimeTics == 230)
    {
        mnVSResultsMakeBar(110);
    }
    if (sMNVSResultsTotalTimeTics == 250)
    {
        mnVSResultsMakeHeader();
        mnVSResultsMakeKOs(124);
    }
}

void mnVSResultsDrawResultsNoContest(GObj *gobj)
{
    if (sMNVSResultsTotalTimeTics == 30)
    {
        mnVSResultsMakeTint();
    }
    if (sMNVSResultsTotalTimeTics == 60)
    {
        mnVSResultsMakeHeader();
        mnVSResultsMakeKOs(66);
    }
    if (sMNVSResultsTotalTimeTics == 80)
    {
        mnVSResultsMakeTKO(81);
    }
}

/* mnvsresults.c:2329-2332 0x801365B4, verbatim */
u8 mnVSResultsCheckTeamBattle(void)
{
    return sMNVSResultsIsTeamBattle;
}

/* mnvsresults.c:2335-2349 0x801365C0, the white rule under the mode
 * label. DIVERGES exactly as the bar above: G_CYC_FILL, so the corner
 * the decomp gives inclusive is passed one further right and down. */
void mnVSResultsLabelProcDisplay(GObj *gobj)
{
    lbCommonSpriteFillRect(32, 42, 283, 45, 0xFF, 0xFF, 0xFF, 0xFF);

    lbCommonClearExternSpriteParams();
    lbCommonDrawSObjAttr(gobj);
}

/* mnvsresults.c:2355-2399 0x801366F0, verbatim: FREE FOR ALL or TEAM
 * BATTLE, and the GObj that carries it is also the one whose process is
 * the ranked rows' schedule. */
void mnVSResultsMakeLabel(void)
{
    GObj *gobj;

    intptr_t offsets[/* */] =
    {
        llMNPlayersGameModesFreeForAllTextSprite, llMNPlayersGameModesTeamBattleTextSprite
    };
    void (*procs[/* */])(GObj*) =
    {
        mnVSResultsDrawResultsTimeRoyal,
        mnVSResultsDrawResultsStockRoyal,
        mnVSResultsDrawResultsTimeTeam,
        mnVSResultsDrawResultsStockTeam,
        mnVSResultsDrawResultsNoContest
    };

    gobj = lbCommonMakeSpriteGObj
    (
        0,
        NULL,
        22,
        GOBJ_PRIORITY_DEFAULT,
        mnVSResultsLabelProcDisplay,
        31,
        GOBJ_PRIORITY_DEFAULT,
        ~0,
        lbRelocGetFileData
        (
            Sprite*,
            sMNVSResultsFiles[2],
            offsets[mnVSResultsCheckTeamBattle()]
        ),
        nGCProcessKindFunc,
        procs[sMNVSResultsKind],
        1
    );
    SObjGetStruct(gobj)->pos.x = 32.0F;
    SObjGetStruct(gobj)->pos.y = 29.0F;
    SObjGetStruct(gobj)->sprite.attr &= ~SP_FASTCOPY;
    SObjGetStruct(gobj)->sprite.attr |= SP_TRANSPARENT;
    SObjGetStruct(gobj)->sprite.red = 0xFF;
    SObjGetStruct(gobj)->sprite.green = 0xFF;
    SObjGetStruct(gobj)->sprite.blue = 0xFF;
}

/* mnvsresults.c:2402-2424 0x80136830, verbatim */
void mnVSResultsMakeHeaderCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            15,
            COBJ_MASK_DLLINK(31),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mnvsresults.c:2427-2459 0x801366F8, verbatim. The scene's first
 * camera that is not a sprite pass: a real perspective view down
 * z, 1800 units back, over the whole 300x220 the others use, drawing
 * DL link 33. Its priority (60) puts it just after the wallpaper's 80
 * and before every other pass, which is where the emblem belongs -- up
 * out of the blue and under the text.
 *
 * mnVSResultsMakeFighterCamera below it is the podium's, and it is here for what it captures besides the podium: DL links
 * 18, 15 and 10 are ef/efdisplay.c's display GObjs, which is how the
 * confetti reaches the screen. The podium fighters are on link 9. */
void mnVSResultsMakeEmblemCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
            GOBJ_PRIORITY_DEFAULT,
            func_80017DBC,
            60,
            COBJ_MASK_DLLINK(33),
            ~0,
            TRUE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->vec.eye.x = 0.0F;
    cobj->vec.eye.y = 0.0F;
    cobj->vec.eye.z = 1800.0F;
    cobj->vec.at.x = 0.0F;
    cobj->vec.at.y = 0.0F;
    cobj->vec.at.z = 0.0F;
    cobj->vec.up.x = 0.0F;
    cobj->vec.up.y = 1.0F;
    cobj->vec.up.z = 0.0F;
}

/* mnvsresults.c:2461-2500 0x8013687C, verbatim: the same
 * view as the emblem's, at priority 50, over the podium's link and the
 * three effect display links. */
// 0x801369B4
void mnVSResultsMakeFighterCamera(void)
{
	CObj *cobj = CObjGetStruct
	(
		gcMakeCameraGObj
		(
			nGCCommonKindSceneCamera,
			NULL,
			16,
			GOBJ_PRIORITY_DEFAULT,
			func_80017DBC,
			50,
			COBJ_MASK_DLLINK(18) | COBJ_MASK_DLLINK(15) |
			COBJ_MASK_DLLINK(10) | COBJ_MASK_DLLINK(9),
			~0,
			TRUE,
			nGCProcessKindFunc,
			NULL,
			1,
			FALSE
		)
	);
	syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

	cobj->vec.eye.x = 0.0F;
	cobj->vec.eye.y = 0.0F;
	cobj->vec.eye.z = 1800.0F;

	cobj->vec.at.x = 0.0F;
	cobj->vec.at.y = 0.0F;
	cobj->vec.at.z = 0.0F;
	
	cobj->vec.up.x = 0.0F;
	cobj->vec.up.y = 1.0F;
	cobj->vec.up.z = 0.0F;
}

/* mnvsresults.c:284-358 0x80131EB0, the announcer's schedule, verbatim.
 * A free-for-all: THIS GAME'S WINNER IS at 81, the winner's name at
 * 210, the crowd at 270. Which voices are resident is the results
 * scene's sound set (tools/export/ssb_sndsets.py, src/dc/sndres.c); a
 * voice that is not is a no-op with a "not resident" line in the log. */
void mnVSResultsAnnounceWinner(void)
{
    u32 announce_names[/* */] =
    {
        nSYAudioVoiceAnnounceMario,
        nSYAudioVoiceAnnounceFox,
        nSYAudioVoiceAnnounceDonkey,
        nSYAudioVoiceAnnounceSamus,
        nSYAudioVoiceAnnounceLuigi,
        nSYAudioVoiceAnnounceLink,
        nSYAudioVoiceAnnounceYoshi,
        nSYAudioVoiceAnnounceCaptain,
        nSYAudioVoiceAnnounceKirby,
        nSYAudioVoiceAnnouncePikachu,
        nSYAudioVoiceAnnouncePurin,
        nSYAudioVoiceAnnounceNess
    };
    u32 announcer_teams[/* */] =
    {
        nSYAudioVoiceAnnounceRedTeam,
        nSYAudioVoiceAnnounceBlueTeam,
        nSYAudioVoiceAnnounceGreenTeam
    };

    if (sMNVSResultsKind == nMNVSResultsKindNoContest)
    {
        // No Contest
        switch (sMNVSResultsTotalTimeTics)
        {
        case 2:
            func_800269C0_275C0(nSYAudioVoiceAnnounceNoContest);
            break;

        case 71:
            func_800269C0_275C0(nSYAudioVoicePublicNoContest);
            break;
        }
    }
    else if (sMNVSResultsIsTeamBattle == FALSE)
    {
        // FFA - "This Game's Winner Is..."
        switch (sMNVSResultsTotalTimeTics)
        {
        case 81:
            func_800269C0_275C0(nSYAudioVoiceAnnounceWinnerIs);
            break;

        case 210:
            func_800269C0_275C0(announce_names[mnVSResultsGetFighterKind(mnVSResultsGetWinPlayer())]);
            break;

        case 270:
            func_800269C0_275C0(nSYAudioVoicePublicExcited);
            break;
        }
    }
    else
    {
        // Teams - "Red/Blue/Grean Team Wins!"
        switch (sMNVSResultsTotalTimeTics)
        {
        case 81:
            func_800269C0_275C0(announcer_teams[mnVSResultsGetWinTeam()]);
            break;

        case 130:
            func_800269C0_275C0(nSYAudioVoiceAnnounceWins);
            break;

        case 150:
            func_800269C0_275C0(nSYAudioVoicePublicExcited);
            break;
        }
    }
}

/* mnvsresults.c:2850-2871 0x801377C0, a GObj thread: wait for the win
 * jingle to start, then wait for it to end and put the results theme on
 * behind the screen.
 *
 * DIVERGES in the test only. The decomp reads gSYAudioCSPlayers[0]->state
 * against AL_STOPPED, which is libultra's sequence player struct; the
 * port's BGM player is its own (src/dc/bgm.c) and sys/audio.c's own
 * wrapper for the same question is syAudioCheckBGMPlaying, which is
 * what this asks. */
void mnVSResultsAudioThreadUpdate(GObj *gobj)
{
    while (syAudioCheckBGMPlaying(0) == FALSE)
    {
        gcSleepCurrentGObjThread(1);
    }
    while (TRUE)
    {
        if (syAudioCheckBGMPlaying(0) == FALSE)
        {
            syAudioPlayBGM(0, nSYAudioBGMResults);
            gcEjectGObj(NULL);
        }
        gcSleepCurrentGObjThread(1);
    }
}

/* mnvsresults.c:2868-2871 0x80137854, verbatim */
void mnVSResultsMakeAudioThread(void)
{
    gcAddGObjProcess(gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT), mnVSResultsAudioThreadUpdate, nGCProcessKindThread, 1);
}

/* mnvsresults.c:2874-2917, the three rumble functions. The winner's pad
 * buzzes every other tic for two seconds while the results come up, and
 * the other three are stopped; the four-pad loop that func_ovl31_80137938
 * is not covers the rest.
 *
 * This is a thread, not a process: gcAddGObjProcess with
 * nGCProcessKindThread makes a real one, and gcSleepCurrentGObjThread(1)
 * yields it a tic at a time. At tic 120 it ejects itself -- gcEjectGObj
 * with a NULL target is "the GObj running right now" (sys/objman.c), and
 * setting sGCRunStatus takes it down at the end of that run. The sleep
 * after the eject is the game's and is never reached with the GObj gone;
 * it is kept because the decomp keeps it, and because the two lines are
 * one thought. */
void func_ovl31_80137898(GObj *gobj)
{
    u32 tic = 0;
    s32 winner = mnVSResultsGetWinPlayer();

    while (TRUE)
    {
        tic++;

        if (tic == 120)
        {
            syControllerInitRumble(winner);
            syControllerStopRumble(winner);

            gcEjectGObj(NULL);
            gcSleepCurrentGObjThread(1);
        }
        if (tic % 2)
        {
            syControllerStartRumble(winner);
        }
        else syControllerStopRumble(winner);

        gcSleepCurrentGObjThread(1);
    }
}

/* mnvsresults.c:2902-2905 0x80137938, verbatim. */
void func_ovl31_80137938(void)
{
    gcAddGObjProcess(gcMakeGObjSPAfter(0, NULL, 15, GOBJ_PRIORITY_DEFAULT), func_ovl31_80137898, nGCProcessKindThread, 1);
}

/* mnvsresults.c:2908-2917 0x8013797C, verbatim. */
void func_ovl31_8013797C(void)
{
    s32 i;

    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        syControllerInitRumble(i);
        syControllerStopRumble(i);
    }
}

/* mnvsresults.c:3157-3207 0x80138714, verbatim: the winner's series
 * theme. Every one of the eleven is in the port's bank, which carries
 * all 47 of the ROM's sequences. */
void mnVSResultsPlayWinBGM(void)
{
    switch (mnVSResultsGetFighterKind(mnVSResultsGetWinPlayer()))
    {
    case nFTKindMario:
    case nFTKindLuigi:
        syAudioPlayBGM(0, nSYAudioBGMWinMario);
        break;

    case nFTKindFox:
        syAudioPlayBGM(0, nSYAudioBGMWinFox);
        break;

    case nFTKindDonkey:
        syAudioPlayBGM(0, nSYAudioBGMWinDonkey);
        break;

    case nFTKindSamus:
        syAudioPlayBGM(0, nSYAudioBGMWinMetroid);
        break;

    case nFTKindLink:
        syAudioPlayBGM(0, nSYAudioBGMWinZelda);
        break;

    case nFTKindYoshi:
        syAudioPlayBGM(0, nSYAudioBGMWinYoshi);
        break;

    case nFTKindCaptain:
        syAudioPlayBGM(0, nSYAudioBGMWinFZero);
        break;

    case nFTKindPikachu:
    case nFTKindPurin:
        syAudioPlayBGM(0, nSYAudioBGMWinPMonsters);
        break;

    case nFTKindKirby:
        syAudioPlayBGM(0, nSYAudioBGMWinKirby);
        break;

    case nFTKindNess:
        syAudioPlayBGM(0, nSYAudioBGMWinMother);
        break;

    default:
        syAudioPlayBGM(0, nSYAudioBGMWinDefault);
        break;
    }
}

/* mnvsresults.c:3209-3218, verbatim. */
// 0x80138830
void mnVSResultsMakeConfetti(void)
{
	Vec3f pos1 = { 0.0F, 1000.0F,  -400.0F };
	Vec3f pos0 = { 0.0F, 1000.0F, -1000.0F };
	s32 unused;

	efManagerConfettiMakeEffect(&pos0, FALSE);
	efManagerConfettiMakeEffect(&pos1, TRUE);
}

/* mnvsresults.c:2756-2763 mnVSResultsInitFighter 0x801374F4, verbatim */
void mnVSResultsInitFighter(s32 player)
{
    mnVSResultsMakeFighter(player);
    mnVSResultsSetFighterPosition(sMNVSResultsFighterGObjs[player], player, mnVSResultsGetSpot(player));
    mnVSResultsSetFighterScale(sMNVSResultsFighterGObjs[player], player, mnVSResultsGetFighterKind(player), mnVSResultsGetSpot(player));
    mnVSResultsMakePlayerTag(player, gSCManagerTransferBattleState.players[player].color);
    mnVSResultsSetFighterStatus(sMNVSResultsFighterGObjs[player], player);
}

/* mnvsresults.c:2766-2797 0x801375AC, verbatim */
void mnVSResultsInitFightersAll(void)
{
    s32 i;

    if (sMNVSResultsIsPresent[0] != FALSE)
    {
        mnVSResultsInitFighter(0);
    }
    if (sMNVSResultsIsPresent[1] != FALSE)
    {
        mnVSResultsInitFighter(1);
    }
    if (sMNVSResultsIsPresent[2] != FALSE)
    {
        mnVSResultsInitFighter(2);
    }
    if (sMNVSResultsIsPresent[3] != FALSE)
    {
        mnVSResultsInitFighter(3);
    }
    if (sMNVSResultsKind != nMNVSResultsKindNoContest)
    {
        for (i = 0; i < (ARRAY_COUNT(sMNVSResultsIsPresent) + ARRAY_COUNT(sMNVSResultsFighterGObjs)) / 2; i++)
        {
            if (sMNVSResultsIsPresent[i] != FALSE)
            {
                mnVSResultsFaceWinner(sMNVSResultsFighterGObjs[i], i, mnVSResultsGetPlace(i));
            }
        }
    }
}

/* mnvsresults.c:3330-3339: lbRelocInitSetup and lbRelocLoadFilesListed
 * over dMNVSResultsFileIDs; here, the banks. A file with no bank leaves
 * its entry NULL and sprite_bank_get says so per sprite. */
void mnVSResultsLoadFiles(void)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(dMNVSResultsFileIDs); i++)
    {
        sMNVSResultsFiles[i] = NULL;

        if (sMNVSResultsBankPaths[i] == NULL)
        {
            continue;
        }
        if (sprite_bank_load(&sMNVSResultsBanks[i], sMNVSResultsBankPaths[i]) < 0)
        {
            syDebugPrintf("mnVSResults: no bank for file %d (%s)\n",
                          (int)dMNVSResultsFileIDs[i], sMNVSResultsBankPaths[i]);
            continue;
        }
        sMNVSResultsFiles[i] = &sMNVSResultsBanks[i];
    }
}

/* mnvsresults.c:3227-3318 mnVSResultsFuncRun 0x801388AC, the scene's
 * one GObj: a clock, the things it starts at fixed tics, and the exit.
 * The tics it counts have something hanging off them
 * -- the wallpaper and its second tint at 80, the winner's name, the
 * label and its schedule at 120, the player tags at 120, and the win
 * theme with its audio thread at 120.
 *
 * The confetti, and the podium fighters with
 * their fade-in -- sMNVSResultsCharacterAlpha climbing 0x16 a tic into
 * scSubsysFighterSetLightParams. *
 * The unlock check is the game's and it reads a save
 * data that is really written (mnVSResultsSaveBackup above): the item
 * switch after a hundred VS matches, and Mushroom Kingdom once every
 * stage has been played and every starter has finished the 1P game. Both
 * queue a message in gSCManagerSceneData.unlock_messages.
 *
 * Where it goes next is the game's too: the unlock
 * message when something unlocked (src/dc/mnmessage.c, which reads that
 * queue and comes back here's destination anyway), the character select
 * otherwise. */
void mnVSResultsFuncRun(GObj *gobj)
{
    (void)gobj;

    sMNVSResultsTotalTimeTics++;

    if (sMNVSResultsTotalTimeTics == sMNVSResultsDrawWallpaperTic)
    {
        if (sMNVSResultsKind != nMNVSResultsKindNoContest)
        {
            mnVSResultsMakeWallpaperTint2();
        }
        mnVSResultsMakeWallpaper();
    }
    if (sMNVSResultsTotalTimeTics == sMNVSResultsMakeResultsTic)
    {
        mnVSResultsMakeResultsText();
        mnVSResultsMakeLabel();

        if (sMNVSResultsKind != nMNVSResultsKindNoContest)
        {
            mnVSResultsMakeConfetti();
        }
    }
    if (sMNVSResultsTotalTimeTics == sMNVSResultsInitFightersAllTic)
    {
        mnVSResultsInitFightersAll();
    }
    if (sMNVSResultsInitFightersAllTic < sMNVSResultsTotalTimeTics)
    {
        if (sMNVSResultsCharacterAlpha < 0xFF)
        {
            sMNVSResultsCharacterAlpha += 0x16;

            if (sMNVSResultsCharacterAlpha > 0xFF)
            {
                sMNVSResultsCharacterAlpha = 0xFF;
            }
        }
        scSubsysFighterSetLightParams(10.0F, 10.0F, 0xFF, 0xFF, 0xFF, sMNVSResultsCharacterAlpha);
    }
    mnVSResultsAnnounceWinner();

    if ((sMNVSResultsKind != nMNVSResultsKindNoContest) && (sMNVSResultsTotalTimeTics == 120))
    {
        mnVSResultsPlayWinBGM();
        mnVSResultsMakeAudioThread();
    }
    if (mnVSResultsCheckExit() != FALSE)
    {
        /* mnvsresults.c:3279-3305, verbatim. */
        s32 i, unlocks_num = 0;
        u16 spgame_complete_mask;

        if (!(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_ITEMSWITCH) &&
            (gSCManagerBackupData.vs_itemswitch_battles >= 100))
        {
            gSCManagerSceneData.unlock_messages[unlocks_num] =
                nLBBackupUnlockItemSwitch;
            unlocks_num = 1;
        }
        if (!(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_INISHIE))
        {
            if ((gSCManagerBackupData.ground_mask & LBBACKUP_GROUND_MASK_ALL) ==
                LBBACKUP_GROUND_MASK_ALL)
            {
                for (i = nFTKindPlayableStart, spgame_complete_mask = 0;
                     i <= nFTKindPlayableEnd; i++)
                {
                    if (gSCManagerBackupData.spgame_records[i].is_spgame_complete)
                    {
                        spgame_complete_mask |= (1 << i);
                    }
                }
                if ((spgame_complete_mask & LBBACKUP_CHARACTER_MASK_STARTER) ==
                    LBBACKUP_CHARACTER_MASK_STARTER)
                {
                    gSCManagerSceneData.unlock_messages[unlocks_num] =
                        nLBBackupUnlockInishie;
                    unlocks_num++;
                }
            }
        }
        if (unlocks_num != 0)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindMessage;
        }
        else
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindPlayersVS;
        }

        func_800266A0_272A0();
        syAudioStopBGMAll();
        syTaskmanSetLoadScene();
    }
}

/* mnvsresults.c:2919-3154, the Auto handicap, verbatim
 * but for the one DIVERGES in mnVSResultsGetBestManExcept. After a match
 * with two or more human players, mnVSResultsUpdateAutoHandicap lowers the
 * best human's handicap and raises the worst's, each clamped to 1..9, and
 * uses the next-best human when one end is already at its limit. Ties go
 * by handicap. The new values stay in gSCManagerTransferBattleState for
 * the next match. */
// 0x801379C4
s32 mnVSResultsGetManCount(void)
{
    s32 i, total = 0;

    for (i = 0; i < ARRAY_COUNT(gSCManagerTransferBattleState.players); i++)
    {
        if (gSCManagerTransferBattleState.players[i].pkind == nFTPlayerKindMan)
        {
            total++;
        }
    }
    return total;
}

// 0x80137A1C
s32 mnVSResultsGetBestMan(void)
{
    s32 i;
    sb32 is_human[GMCOMMON_PLAYERS_MAX];
    s32 first_man;
    sb32 tie_exists;

    // determine if human or cpu
    for (i = 0; i < ARRAY_COUNT(is_human); i++)
    {
        is_human[i] = (gSCManagerTransferBattleState.players[i].pkind == nFTPlayerKindMan) ? TRUE : FALSE;
    }

    // determine player of first human
    for (i = 0; i < ARRAY_COUNT(is_human); i++)
    {
        if (is_human[i] != FALSE)
        {
            first_man = i;
            break;
        }
    }
    tie_exists = FALSE;

    // determine the human with the most points and if any humans have the same points total
    for (i = first_man; i < (ARRAY_COUNT(is_human) + ARRAY_COUNT(sMNVSResultsPoints)) / 2; i++)
    {
        if (is_human[i] != FALSE)
        {
            if (sMNVSResultsPoints[first_man] == sMNVSResultsPoints[i])
            {
                tie_exists = TRUE;
            }
            if (sMNVSResultsPoints[first_man] < sMNVSResultsPoints[i])
            {
                first_man = i;
            }
        }
    }

    // break the tie based on the handicap level
    if (tie_exists != FALSE)
    {
        for (i = first_man; i < (ARRAY_COUNT(is_human) + ARRAY_COUNT(sMNVSResultsPoints) + ARRAY_COUNT(gSCManagerTransferBattleState.players)) / 3; i++)
        {
            if
            (
                (is_human[i] != FALSE) &&
                (sMNVSResultsPoints[first_man] == sMNVSResultsPoints[i]) &&
                (gSCManagerTransferBattleState.players[first_man].handicap < gSCManagerTransferBattleState.players[i].handicap)
            )
            {
                first_man = i;
            }
        }
    }
    return first_man;
}

// 0x80137E34
s32 mnVSResultsGetBestManExcept(s32 player)
{
    s32 i;
    sb32 is_human[GMCOMMON_PLAYERS_MAX];
    s32 first_man;
    s32 found;

    /* DIVERGES: the decomp's loop below starts at first_man, which it
     * never sets -- on the N64 whatever the stack held. 0 walks every
     * player, which is what the loop's comment says it looks for. */
    first_man = 0;

    // determine if human or cpu
    for (i = 0; i < ARRAY_COUNT(is_human); i++)
    {
        is_human[i] = (gSCManagerTransferBattleState.players[i].pkind == nFTPlayerKindMan) ? TRUE : FALSE;
    }

    // if a human exists with the same score, return that one
    for (i = first_man; i < (ARRAY_COUNT(is_human) + ARRAY_COUNT(sMNVSResultsPoints)) / 2; i++)
    {
        if ((is_human[i] != FALSE) && (player != i) && (sMNVSResultsPoints[player] == sMNVSResultsPoints[i]))
        {
            return i;
        }
    }
    found = 666;

    for (i = 0; i < ARRAY_COUNT(is_human); i++)
    {
        if ((is_human[i] != FALSE) && (player != i))
        {
            found = i;
        }
    }

    // return 666 if there are no other humans
    if (found == 666)
    {
        return 666;
    }

    // return the other human with the highest score
    for (i = 0; i < (ARRAY_COUNT(is_human) + ARRAY_COUNT(sMNVSResultsPoints)) / 2; i++)
    {
        if ((is_human[i] != FALSE) && (player != i) && (found != i) && (sMNVSResultsPoints[found] < sMNVSResultsPoints[i]))
        {
            found = i;
        }
    }
    return found;
}

// 0x80138130
s32 mnVSResultsGetWorstMan(void)
{
    s32 i;
    sb32 is_human[GMCOMMON_PLAYERS_MAX];
    s32 first_man;
    sb32 tie_exists;

    // determine if human or cpu
    for (i = 0; i < ARRAY_COUNT(is_human); i++)
    {
        is_human[i] = (gSCManagerTransferBattleState.players[i].pkind == nFTPlayerKindMan) ? TRUE : FALSE;
    }
    // determine player of first human
    for (i = 0; i < ARRAY_COUNT(is_human); i++)
    {
        if (is_human[i] != FALSE)
        {
            first_man = i;
            break;
        }
    }
    tie_exists = FALSE;

    // determine the human with the least points and if any humans have the same points total
    for (i = first_man; i < ARRAY_COUNT(sMNVSResultsPoints); i++)
    {
        if (is_human[i] != FALSE)
        {
            if (sMNVSResultsPoints[first_man] == sMNVSResultsPoints[i])
            {
                tie_exists = TRUE;
            }
            if (sMNVSResultsPoints[first_man] > sMNVSResultsPoints[i])
            {
                first_man = i;
            }
        }
    }
    // break the tie based on the handicap level
    if (tie_exists != FALSE)
    {
        for (i = first_man; i < (ARRAY_COUNT(is_human) + ARRAY_COUNT(sMNVSResultsPoints) + ARRAY_COUNT(gSCManagerTransferBattleState.players)) / 3; i++)
        {
            if
            (
                (is_human[i] != FALSE) &&
                (sMNVSResultsPoints[first_man] == sMNVSResultsPoints[i]) &&
                (gSCManagerTransferBattleState.players[first_man].handicap > gSCManagerTransferBattleState.players[i].handicap)
            )
            {
                first_man = i;
            }
        }
    }
    return first_man;
}

// 0x80138548
void mnVSResultsSetAutoHandicaps(s32 best, s32 worst)
{
    s32 handicap_best = gSCManagerTransferBattleState.players[best].handicap;
    s32 handicap_worst = gSCManagerTransferBattleState.players[worst].handicap;
    s32 other;

    if ((handicap_best == 1) && (handicap_worst == 9))
    {
        return;
    }
    else if ((handicap_best > 1) && (handicap_worst < 9))
    {
        gSCManagerTransferBattleState.players[best].handicap--;
        gSCManagerTransferBattleState.players[worst].handicap++;
    }
    else if ((handicap_best == 1) && (handicap_worst < 8))
    {
        gSCManagerTransferBattleState.players[worst].handicap += 2;
    }
    else if ((handicap_best > 2) && (handicap_worst == 9))
    {
        gSCManagerTransferBattleState.players[best].handicap -= 2;
    }
    else if ((handicap_best == 1) && (handicap_worst == 8))
    {
        other = mnVSResultsGetBestManExcept(best);

        if (other != 666)
        {
            gSCManagerTransferBattleState.players[other].handicap--;
            gSCManagerTransferBattleState.players[worst].handicap++;
        }
    }
    else if ((handicap_best == 2) && (handicap_worst == 9))
    {
        other = mnVSResultsGetBestManExcept(best);

        if (other != 666)
        {
            gSCManagerTransferBattleState.players[best].handicap--;
            gSCManagerTransferBattleState.players[other].handicap--;
        }
    }
}

// 0x801386BC
void mnVSResultsUpdateAutoHandicap(void)
{
    if ((sMNVSResultsKind != nMNVSResultsKindNoContest) && (mnVSResultsGetManCount() >= 2))
    {
        mnVSResultsSetAutoHandicaps(mnVSResultsGetBestMan(), mnVSResultsGetWorstMan());
    }
}

/* mnvsresults.c:3321-3392 mnVSResultsFuncStart 0x80138B70, in the
 * game's order.
 *
 * DIVERGES: gcMakeDefaultCameraGObj (the PVR clears its own
 * framebuffer), and the four figatree heaps are NULL rather than
 * gFTManagerFigatreeHeapSize each (see sMNVSResultsFigatreeHeaps).
 * efParticleInitAll and efManagerInitEffects are here, the second through the port's loader
 * (src/dc/efmanager.c efManagerLoadEffectBank), as in the battle.
 *
 * The twelve ftManagerSetupFilesAllKind calls are the game's own loop,
 * kept: a kind whose pack is not on the disc returns from it without a
 * word (src/dc/ftmanager.c), so the loop loads exactly the banks the
 * header can draw an icon from. */
void mnVSResultsFuncStart(void)
{
    s32 i;

    mnVSResultsLoadFiles();

    gcMakeGObjSPAfter(0, mnVSResultsFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

    /* mnvsresults.c:3339-3340: the particle pools and the effect pool
     * with its display GObjs and the common bank, for the confetti. */
    efParticleInitAll();
    efManagerLoadEffectBank();
    ftManagerAllocFighter(FTDATA_FLAG_SUBMOTION, 4);
    /* the port's: the SubMotion statuses alone, as the flag says --
     * tier 0 of each fighter's .anm (ftcommon.h ftManagerSetAnmTier) */
    ftManagerSetAnmTier(FIGHTER_ANM_TIER_MENU);

    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        ftManagerSetupFilesAllKind(i);
    }
    for (i = 0; i < ARRAY_COUNT(sMNVSResultsFigatreeHeaps); i++)
    {
        sMNVSResultsFigatreeHeaps[i] = NULL;
    }
    /* mnvsresults.c:3351: before the rankings, because the records it
     * writes are this match's and the rankings are about to read the
     * same fields off the battle state. */
    mnVSResultsSaveBackup();
    mnVSResultsInitVars();
    mnVSResultsSetIsPresent();
    mnVSResultsInitRankings();

    if (gSCManagerTransferBattleState.handicap == nSCBattleHandicapAuto)
    {
        mnVSResultsUpdateAutoHandicap();
    }
    mnVSResultsMakeEmblemCamera();
    mnVSResultsMakeFighterCamera();
    mnVSResultsMakePlayerTagCamera();
    mnVSResultsMakeResultsTextCamera();
    mnVSResultsMakeTintCamera();
    mnVSResultsMakeHeaderCamera();
    mnVSResultsMakeWallpaperTintCamera();
    mnVSResultsMakeWallpaperTint2Camera();

    /* mnvsresults.c:3375-3378: the winner's series emblem, on the camera
     * above, and not on a no-contest -- which has no winner to have
     * one. */
    if (sMNVSResultsKind != nMNVSResultsKindNoContest)
    {
        mnVSResultsMakeEmblem();
    }
    mnVSResultsMakeWallpaperTint();
    func_ovl31_8013797C();

    /* mnvsresults.c:3381-3385: and, over that, the winner's own pad --
     * but only when a human won it. A no-contest has nobody to
     * congratulate, and a COM or demo player has no pad to buzz. */
    if ((sMNVSResultsKind != nMNVSResultsKindNoContest) &&
        (gSCManagerTransferBattleState.players[mnVSResultsGetWinPlayer()].pkind == nFTPlayerKindMan))
    {
        func_ovl31_80137938();
    }
    scSubsysFighterSetLightParams(10.0F, 10.0F, 0xFF, 0xFF, 0xFF, sMNVSResultsCharacterAlpha);

    /* mnvsresults.c:3361-3365. The wipe in: a copy of the last frame the
     * battle drew, on one of eleven little models that fold it up
     * (src/dc/lbtransition.c). It is made after the screen's own cameras
     * so that its own camera is the last on the list and draws over
     * them, and the photocopy is taken before the transition is loaded
     * because that is when the framebuffer still holds the battle.
     *
     * A no-contest result skips it, as the game does: nobody won, and
     * the screen the wipe would reveal is the one that says so. */
    if (sMNVSResultsKind != nMNVSResultsKindNoContest)
    {
        lbTransitionSetupTransition();
        lbTransitionMakeCamera(0x20000002, 0, 10, COBJ_MASK_DLLINK(32));
        lbTransitionMakeTransition(syUtilsRandIntRange(ARRAY_COUNT(dLBTransitionDescs)),
                                   0x20000000, 0, lbTransitionProcDisplay, 32,
                                   lbTransitionProcUpdate);
    }
    if (sMNVSResultsKind != nMNVSResultsKindNoContest)
    {
        func_800269C0_275C0(nSYAudioVoicePublicWin);
    }
}

/* mnvsresults.c:3397-3440 dMNVSResultsTaskmanSetup. The same cuts as
 * dSCVSBattleTaskmanSetup (src/dc/scvsbattle.c) -- no RSP, no RDP, no
 * link-map arena -- and the pool counts are the port's own. They are
 * small on purpose: this scene makes one GObj, and it will grow when the
 * screen does.
 *
 * Its update function is gcRunAll and not a dispatcher, because a menu
 * has no game_status to dispatch on. That is the decomp's.
 *
 * The pool counts were the port's own eights while the scene made one
 * GObj. They are the game's own zeros since the screen arrived: the
 * screen makes six cameras, a dozen GObjs and something like seventy
 * SObjs, and a zero pool is not "no objects" -- gcGetGObj and its
 * siblings take one off the scene heap whenever the free list is empty
 * (sys/objman.c:112-245), which is how every ported menu runs. */
SYTaskmanSetup dMNVSResultsTaskmanSetup =
{
    {
        0,                              /* flags */
        gcRunAll,                       /* update function */
        scManagerFuncDraw,              /* frame draw function */
        NULL,                           /* allocatable memory pool start */
        0,                              /* allocatable memory pool size */
        1,                              /* ??? */
        2,                              /* number of contexts? */
        0, 0, 0, 0,                     /* the four DL buffer sizes */
        0,                              /* graphics heap size */
        2,                              /* ??? */
        0,                              /* RDP output buffer size */
        NULL,                           /* pre-render function */
        syControllerFuncRead,           /* controller I/O function */
    },

    0,                                  /* number of GObjThreads */
    sizeof(u64) * 192,                  /* thread stack size */
    0,                                  /* number of thread stacks */
    0,                                  /* ??? */
    0,                                  /* GObjProcesses */
    0,  sizeof(GObj),
    0,                                  /* XObjs */
    NULL,                               /* matrix function list */
    NULL,                               /* DObjVec eject function */
    0,                                  /* AObjs */
    0,                                  /* MObjs */
    0,  sizeof(DObj),
    0,  sizeof(SObj),
    0,  sizeof(CObj),

    mnVSResultsFuncStart                /* task start function */
};

/* The bzero arm of syDmaLoadOverlay for overlay 31, this
 * file: sc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnVSResultsOverlayLoad(void)
{
    /* Before the clears, because one of them is the emblem's pack and
     * zeroing it would lose what fighter_release has to free. The N64
     * has no line here: its copy went out with the scene heap on the way
     * out of the scene. */
#ifndef FT_HOSTTEST
    fighter_release(&sMNVSResultsEmblemModel);
#endif

    OVERLAY_CLEAR(sMNVSResultsEmblemModel);
    OVERLAY_CLEAR(sMNVSResultsFiles);
    OVERLAY_CLEAR(sMNVSResultsBanks);
    OVERLAY_CLEAR(sMNVSResultsIsPresent);
    OVERLAY_CLEAR(sMNVSResultsKOs);
    OVERLAY_CLEAR(sMNVSResultsTKO);
    OVERLAY_CLEAR(sMNVSResultsPoints);
    OVERLAY_CLEAR(sMNVSResultsPlaces);
    OVERLAY_CLEAR(sMNVSResultsFighterKinds);
    OVERLAY_CLEAR(sMNVSResultsIsSharedWinner);
    OVERLAY_CLEAR(sMNVSResultsIsTeamBattle);
    OVERLAY_CLEAR(sMNVSResultsKind);
    OVERLAY_CLEAR(sMNVSResultsTotalTimeTics);
    OVERLAY_CLEAR(sMNVSResultsAllowExitWait);
    OVERLAY_CLEAR(sMNVSResultsDrawWallpaperTic);
    OVERLAY_CLEAR(sMNVSResultsMakeResultsTic);
    OVERLAY_CLEAR(sMNVSResultsInitFightersAllTic);
    OVERLAY_CLEAR(sMNVSResultsBarWidth);
    OVERLAY_CLEAR(sMNVSResultsTintAlpha);
    OVERLAY_CLEAR(sMNVSResultsWallpaperTintAlpha);
    OVERLAY_CLEAR(sMNVSResultsWallpaperTint2Alpha);
    OVERLAY_CLEAR(sMNVSResultsCharacterAlpha);
    OVERLAY_CLEAR(sMNVSResultsFighterGObjs);
    OVERLAY_CLEAR(sMNVSResultsFigatreeHeaps);
}

/* mnvsresults.c:3443-3457 mnVSResultsStartScene 0x80138E64.
 *
 * DIVERGES: syVideoInit and the zbuffer allocation are the N64's video
 * mode, set per scene -- the Dreamcast's is set once at boot and every
 * scene here shares it, which is a difference that only matters when a
 * scene wants a different resolution and none does. The arena_size line
 * is the link map. */
void mnVSResultsStartScene(void)
{
    s32 i;

    scManagerFuncUpdate(&dMNVSResultsTaskmanSetup);

    /* mnvsresults.c:3455-3456: every pad stopped before the screen the
     * match handed over to starts drawing, so nothing is left buzzing
     * through it. */
    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        syControllerInitRumble(i);
        syControllerStopRumble(i);
    }
}
