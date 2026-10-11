/* ifcommon.c -- see ifcommon.h. Function-for-function from
 * ssb-decomp-re/src/if/ifcommon.c; every function names its line range.
 *
 * The HUD draws: the damage digits with each player's
 * emblem, the stock rows, the player tags, the countdown, GO! and GAME
 * SET are the game's SObjs on the game's interface camera, through
 * lb/lbcommon.c's sprite pass (src/dc/lbcommon.c); the
 * match clock is there too, and the pause menu, which
 * is also where the reset combo sets gSCManagerSceneData.is_reset. The
 * off-screen arrows are the one piece of the HUD
 * that is not a sprite, and the only one with a model and a camera of
 * its own. What is still not here is named at the head of its section:
 * the magnifying glasses (a per-fighter viewport re-render, which the
 * PVR has no answer for yet). The
 * announcer's voices are ft/ftpublic.c's queue, which that file
 * drains.
 *
 * The one substitution made everywhere rather than at each site: the
 * game reaches a HUD sprite as lbRelocGetFileData(Sprite*,
 * gGMCommonFiles[n], offset), the file's base plus the offset its
 * tables carry. gGMCommonFiles[n] is a SpriteBank here (src/dc/
 * gmcommon.c) and the macro is redefined below to look the offset up
 * in it, so every such line is the decomp's text.
 */
#include "ifcommon.h"
#include "overlay.h"

#include "fighter.h"
#include "ftcommon.h"
#include "gmcamera.h"
#include "gmcommon.h"
#include "lbcommon.h"
#include "objmodel.h"
#include "objpvr.h"
#include "sc1pgame.h"          /* sc1PGameSetCameraZoom: the 1P proc_set */
#include "sprite.h"
#include "stage.h"             /* grWallpaperResumeProcessAll */

#include <if/ifcommon.h>
#include <if/interface.h>
#include <it/item.h>            /* ITStruct, itGetStruct: the item arrow */
#include <gm/gmrumble.h>
#include <gm/gmsound.h>
#include <mp/map.h>
#include <sc/scene.h>
#include <sys/rdp.h>
#include <sys/taskman.h>
#include <sys/utils.h>

#include "taskman.h"
#include "perf.h"

/* lb/library.h:7 -- see the file comment */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* sys/taskman.h:82 gSYTaskmanDLHeads: the display-list heads every
 * lbCommonPrep* call takes and the port's ignore (src/dc/lbcommon.c);
 * NULL here, as in src/dc/mntitle.c, so the calls read as the game's. */
#define gSYTaskmanDLHeads NULL

#ifndef S8_MAX
#define S8_MAX 127
#endif

/* ---- ifcommon.c:15-402, the HUD's tables, verbatim. Where the game's
 * hold a link label (&llIFCommonPlayerDamageDigit0Sprite), the port's
 * hold the number it links to -- the sprite's offset in its relocData
 * file, from tools/relocFileDescriptions.us.txt -- which is what
 * lbRelocGetFileData adds to the file base on the N64 and what
 * sprite_bank_get looks up here. ------------------------------------ */

/* ifcommon.c:22-27 - Width of each digit? */
s32 dIFCommonPlayerDamageDigitWidths[/* */] =
{
    14,  9, 15, 14,
    15, 13, 15, 14,
    15, 15, 17, 20
};

/* ifcommon.c:30-36 - Player HUD digit colors */
u8 dIFCommonPlayerDamageDigitColorsR[/* */] = { 0xFF, 0xF0, 0xF0, 0xFF, 0xFF };
u8 dIFCommonPlayerDamageDigitColorsG[/* */] = { 0xF0, 0xFF, 0xF0, 0xFF, 0xFF };
u8 dIFCommonPlayerDamageDigitColorsB[/* */] = { 0xF0, 0xF0, 0xFF, 0xFF, 0xFF };

/* ifcommon.c:39 - Player HUD position X offsets */
s32 dIFCommonPlayerDamagePositionOffsetsX[/* */] = { 55, 125, 195, 265 };

/* ifcommon.c:42 - Player score popup X offsets */
s32 dIFCommonPlayerScorePositionOffsetsX[/* */] = { 0, 0, 0, 0 };

/* ifcommon.c:45-51 - Player emblem position offsets and scales */
s32 dIFCommonPlayerDamageEmblemOffsetsX[/* */] = { 3, 3, 3, 3 };
s32 dIFCommonPlayerDamageEmblemOffsetsY[/* */] = { -3, -3, -3, -3 };
f32 dIFCommonPlayerDamageEmblemScales[/* */] = { 1.0F, 1.0F, 1.0F, 1.0F };

/* ifcommon.c:54 - Player stock icon X offsets (when stock count <= 6) */
s32 dIFCommonPlayerStocksIconOffsetsX[/* */] = { -24, -24, -24 ,-24 };

/* ifcommon.c:57 - Player stock digit X offsets (when stock count > 6) */
s32 dIFCommonPlayerStocksDigitOffsetsX[/* */] = { 4, 4, 4, 4 };

/* ifcommon.c:60-84, the countdown's lamp colours */
u8 dIFCommonTrafficSpriteColorsR[/* */] = { 0xFE, 0xFF, 0x4B, 0xFF, 0xFF, 0x22, 0xFF, 0xFF, 0xFF };
u8 dIFCommonTrafficSpriteColorsG[/* */] = { 0x0C, 0xA2, 0x64, 0x38, 0xA2, 0x66, 0xFF, 0xFF, 0xFF };
u8 dIFCommonTrafficSpriteColorsB[/* */] = { 0x0C, 0x00, 0xFF, 0x38, 0x00, 0xFE, 0xFF, 0xFF, 0xFF };
u8 dIFCommonTrafficGoBacklightR[/* */] = { 0x00, 0x6A };
u8 dIFCommonTrafficGoBacklightG[/* */] = { 0x00, 0x6A };
u8 dIFCommonTrafficGoBacklightB[/* */] = { 0x00, 0x95 };
u8 dIFCommonTrafficGoShadowR[/* */] = { 0x00, 0x12 };
u8 dIFCommonTrafficGoShadowG[/* */] = { 0x00, 0x12 };
u8 dIFCommonTrafficGoShadowB[/* */] = { 0x00, 0x2E };

/* ifcommon.c:87-104 */
IFTraffic dIFCommonTrafficSpriteData[/* */] =
{
    { { 123, -13 }, 0x00 },
    { { 140, -11 }, 0x01 },
    { { 153, -11 }, 0x01 },
    { { 166, -11 }, 0x01 },
    { { 180, -15 }, 0x02 },
    { { 107, -33 }, 0x03 },
    { { 119,  21 }, 0x04 },
    { { 132,  21 }, 0x04 },
    { { 145,  21 }, 0x04 },
    { { 162,  20 }, 0x05 },
    { { 115, -26 }, 0x06 },
    { { 131,  32 }, 0x07 },
    { { 144,  32 }, 0x07 },
    { { 157,  32 }, 0x07 },
    { { 167,  23 }, 0x08 }
};

/* ifcommon.c:107-112: file 82's LampRedDim, LampYellowDim, LampBlueDim,
 * LampRedContour, LampYellowContour, LampBlueContour, LampRedLight,
 * LampYellowLight, LampBlueLight */
intptr_t dIFCommonTrafficSpriteOffsets[/* */] =
{
    0x21950, 0x21A10, 0x21BA8,
    0x23A28, 0x24620, 0x25290,
    0x22128, 0x22588, 0x22F18
};

/* ifcommon.c:115-120 - Announcer text: "GO!" (file 82's OrangeLetterG,
 * OrangeLetterO, OrangeExclamationMark) */
IFACharacter dIFCommonAnnounceGoSpriteData[/* */] =
{
    { {  82, 93 }, 0x4D78 },
    { { 144, 93 }, 0xA730 },
    { { 214, 93 }, 0xC370 }
};

/* ifcommon.c:122-136 - "SUDDEN DEATH", two rows of six, out of the
 * announcer's common alphabet (file 37, bank slot 7). Every other
 * banner in the ported game spells itself out of file 82's
 * pre-coloured letters; this one is the plain alphabet and takes its
 * colour from ifCommonAnnounceSetColors instead. */
IFACharacter dIFCommonAnnounceSuddenDeathSpriteData[/* */] =
{
    { {  74,  67 }, 0x057F0 },  /* S */
    { { 102,  67 }, 0x060D8 },  /* U */
    { { 132,  67 }, 0x01268 },  /* D */
    { { 163,  67 }, 0x01268 },  /* D */
    { { 193,  67 }, 0x01628 },  /* E */
    { { 217,  67 }, 0x03E88 },  /* N */
    { {  83, 113 }, 0x01268 },  /* D */
    { { 113, 113 }, 0x01628 },  /* E */
    { { 135, 113 }, 0x005E0 },  /* A */
    { { 165, 113 }, 0x05BD0 },  /* T */
    { { 192, 113 }, 0x02408 },  /* H */
    { { 227, 113 }, 0x07D98 }   /* ! */
};

/* ifcommon.c:140 - white letters with a black drop shadow */
SYColorRGBPair dIFCommonAnnounceSuddenDeathSpriteColors = { { 0xFF, 0xFF, 0xFF }, { 0x00, 0x00, 0x00 } };

/* ifcommon.c:357-368 - "COMPLETE!", one row of nine, out of the same
 * plain alphabet (file 37, bank slot 7) SUDDEN DEATH above spells
 * itself with -- so it takes its colour from ifCommonAnnounceSetColors
 * too, and a RED shadow rather than a black one. This is
 * what a cleared bonus stage puts on the screen; the banner is here
 * rather than with the rest of its scene because it is if/'s, and
 * gr/grbonus/grbonus3.c is the first caller. */
IFACharacter dIFCommonAnnounceCompleteSpriteData[/* */] =
{
    { {  46, 101 }, 0x00D80 },  /* C */
    { {  71, 101 }, 0x044B0 },  /* O */
    { { 104, 100 }, 0x03980 },  /* M */
    { { 143, 101 }, 0x04890 },  /* P */
    { { 168, 101 }, 0x03358 },  /* L */
    { { 189, 101 }, 0x01628 },  /* E */
    { { 212, 101 }, 0x05BD0 },  /* T */
    { { 237, 101 }, 0x01628 },  /* E */
    { { 267, 101 }, 0x07D98 }   /* ! */
};

/* ifcommon.c:371 - white letters with a red drop shadow */
SYColorRGBPair dIFCommonAnnounceCompleteSpriteColors = { { 0xFF, 0xFF, 0xFF }, { 0xFF, 0x00, 0x00 } };

/* ifcommon.c:342-352 - "FAILURE", the COMPLETE! banner's opposite number
 * and built exactly the same way: seven letters out of file 37's plain
 * alphabet, its own colours, a BLUE drop shadow rather than a red one.
 * It is raised by src/dc/sc1pbonusstage.c's clock, when a
 * ladder bonus rung runs out of time. */
IFACharacter dIFCommonAnnounceFailureSpriteData[/* */] =
{
    { {  77, 101 }, 0x01A00 },  /* F */
    { {  97, 101 }, 0x005E0 },  /* A */
    { { 130, 101 }, 0x026B8 },  /* I */
    { { 145, 101 }, 0x03358 },  /* L */
    { { 167, 101 }, 0x060D8 },  /* U */
    { { 197, 101 }, 0x05418 },  /* R */
    { { 225, 101 }, 0x01628 }   /* E */
};

/* ifcommon.c:354 - white letters with a blue drop shadow */
SYColorRGBPair dIFCommonAnnounceFailureSpriteColors = { { 0xFF, 0xFF, 0xFF }, { 0x00, 0x00, 0xFF } };

/* ifcommon.c:143-151 - "TIME UP" (file 82's BlueLetterT, I, M, E, U, P) */
IFACharacter dIFCommonAnnounceTimeUpSpriteData[/* */] =
{
    { {  45, 95 }, 0x0E4A8 },
    { {  82, 95 }, 0x0F740 },
    { { 100, 95 }, 0x127E0 },
    { { 151, 95 }, 0x144E0 },
    { { 195, 95 }, 0x16EB8 },
    { { 238, 95 }, 0x18FE8 }
};

/* ifcommon.c:154-163 - "GAME SET" (file 82's BlueLetterG, A, M, E, S,
 * E, T) */
IFACharacter dIFCommonAnnounceGameSetSpriteData[/* */] =
{
    { {  22, 95 }, 0x20788 },
    { {  62, 95 }, 0x1DE68 },
    { { 104, 95 }, 0x127E0 },
    { { 154, 95 }, 0x144E0 },
    { { 191, 95 }, 0x1B5F8 },
    { { 230, 95 }, 0x144E0 },
    { { 262, 95 }, 0x0E4A8 }
};

/* ifcommon.c:166-171, verbatim: how long the entry focus lingers on
 * each fighter, per focus id */
u16 dIFCommonEntryFocusSleepTics[/* */] =
{
    22,
    15,
    60
};

/* ifcommon.c:174-178 - X of the clock's four digits: minute tens,
 * minute ones, second tens, second ones */
s32 dIFCommonTimerDigitsSpritePositionsX[/* */] =
{
    232, 247, 273, 288
};

/* ifcommon.c:180-194 - Offset of twelve digits: numbers 0 through 9, %
 * sign and H.P. text (file 164's Digit0..9, SymbolPercent, SymbolHP) */
intptr_t dIFCommonPlayerDamageDigitSpriteOffsets[/* */] =
{
    0x0148, 0x02D8, 0x0500, 0x0698, 0x08C0,
    0x0A58, 0x0C80, 0x0E18, 0x1040, 0x1270,
    0x1458, 0x15D8
};

/* ifcommon.c:197-212 (file 165's Digit0..9, SymbolColon, SymbolSec,
 * SymbolCSec). The last two are the seconds-only face the 1P game's
 * bonus stages use; the VS clock draws four digits and the colon. */
intptr_t dIFCommonTimerDigitSpriteOffsets[/* */] =
{
    0x00138, 0x00228, 0x003A8, 0x00528, 0x006A8,
    0x00828, 0x009A8, 0x00B28, 0x00CA8, 0x00E28,
    0x00F08, 0x01140, 0x01238
};

/* ifcommon.c:215-228 (file 36's 0..9 and Cross) */
intptr_t dIFCommonPlayerStockDigitSpriteOffsets[/* */] =
{
    0x068, 0x118, 0x1C8, 0x278, 0x328,
    0x3D8, 0x488, 0x538, 0x5E8, 0x698,
    0x828
};

/* ifcommon.c:244-250 - Length of each time unit in tics, in the order
 * the digits are drawn: minute tens, minute ones, second tens, second
 * ones */
u16 dIFCommonTimerDigitsUnitLengths[/* */] =
{
    I_MIN_TO_TICS(10),
    I_MIN_TO_TICS(1),
    I_SEC_TO_TICS(10),
    I_SEC_TO_TICS(1)
};

/* ifcommon.c:253 */
u8 dIFCommonPlayerTeamColorIDs[/* */] = { 0, 1, 3, 4, 0 };

/* ifcommon.c:256-263, the announcer counting the last five seconds
 * down */
u16 dIFCommonAnnounceTimerVoiceIDs[/* */] =
{
    nSYAudioVoiceAnnounceOne,
    nSYAudioVoiceAnnounceTwo,
    nSYAudioVoiceAnnounceThree,
    nSYAudioVoiceAnnounceFour,
    nSYAudioVoiceAnnounceFive
};

/* ifcommon.c:288-311, the tags' colours per player colour */
u8 dIFCommonPlayerTagPrimColorsR[/* */] = { 0xED, 0x4E, 0xFF, 0x4E, 0xAC };
u8 dIFCommonPlayerTagPrimColorsG[/* */] = { 0x36, 0x4E, 0xDF, 0xB9, 0xAC };
u8 dIFCommonPlayerTagPrimColorsB[/* */] = { 0x36, 0xE9, 0x1A, 0x4E, 0xAC };
u8 dIFCommonPlayerTagEnvColorsR[/* */] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
u8 dIFCommonPlayerTagEnvColorsG[/* */] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
u8 dIFCommonPlayerTagEnvColorsB[/* */] = { 0x00, 0x00, 0x00, 0x00, 0x00 };

/* ifcommon.c:314-322 (file 38's 1P, 2P, 3P, 4P, CP, Ally) */
intptr_t dIFCommonPlayerTagSpriteOffsets[/* */] =
{
    0x258, 0x4F8, 0x798, 0xA38, 0xCD8, 0xEB8
};

/* ifcommon.c:266-272 (file 197's PlayerNum1P..4P), the "1P" through "4P"
 * in the pause banner's corner -- whose pause it is. */
intptr_t dIFCommonBattlePausePlayerNumSpriteOffsets[/* */] =
{
    0x078, 0x138, 0x1F8, 0x2B8
};

/* ifcommon.c:313-329, the pause menu's decals: where each sprite sits on
 * the 320x240 screen and the two colours it is drawn in. All of them are
 * file 197's, which holds nothing else -- the bank is the pause menu.
 *
 * The first twelve are the menu; ifCommonBattlePauseMakeSObjsAll draws
 * ten of them when the pausing player is out of the camera's reach and
 * the last two -- the control stick and the red arrows around it, the
 * "you may look around" hint -- only when they mean something. The last
 * two entries are the bonus stages' "L: RETRY", drawn only on a course
 * played from its own menu. */
IFPauseDecal dIFCommonBattlePauseDecalsSpriteData[/* */] =
{
    { 0x0438, { 232, 191 }, { { 0xFF, 0xFF, 0xFF }, { 0x00, 0x00, 0x00 } } },  /* PAUSE */
    { 0x0958, {  99, 203 }, { { 0x00, 0x95, 0xFF }, { 0x00, 0x05, 0xC7 } } },  /* A */
    { 0x0A88, { 122, 203 }, { { 0x36, 0xBF, 0x00 }, { 0x00, 0x30, 0x00 } } },  /* B */
    { 0x0BD8, { 145, 202 }, { { 0x80, 0x80, 0x80 }, { 0x21, 0x21, 0x21 } } },  /* Z */
    { 0x0CF8, { 164, 203 }, { { 0x80, 0x80, 0x80 }, { 0x21, 0x21, 0x21 } } },  /* R */
    { 0x04D8, { 113, 206 }, { { 0xFF, 0xFF, 0xFF }, { 0x00, 0x00, 0x00 } } },  /* + */
    { 0x04D8, { 136, 206 }, { { 0xFF, 0xFF, 0xFF }, { 0x00, 0x00, 0x00 } } },  /* + */
    { 0x04D8, { 155, 206 }, { { 0xFF, 0xFF, 0xFF }, { 0x00, 0x00, 0x00 } } },  /* + */
    { 0x0610, { 182, 205 }, { { 0xFF, 0xFF, 0xFF }, { 0x00, 0x00, 0x00 } } },  /* RESET */
    { 0x06D8, { 198, 191 }, { { 0xFF, 0xFF, 0xFF }, { 0x00, 0x00, 0x00 } } },  /* the smash ball */
    { 0x1538, {  21,  19 }, { { 0xFF, 0x00, 0x00 }, { 0x00, 0x00, 0x00 } } },  /* the arrows */
    { 0x17A8, {  31,  29 }, { { 0xFF, 0xFF, 0xFF }, { 0x14, 0x18, 0x11 } } },  /* the control stick */
    { 0x18C8, {  34, 203 }, { { 0x80, 0x80, 0x80 }, { 0x21, 0x21, 0x21 } } },  /* L */
    { 0x0828, {  51, 205 }, { { 0xFF, 0xFF, 0xFF }, { 0x00, 0x00, 0x00 } } }   /* RETRY */
};

/* ifcommon.c:332-339, the white frame the pause menu draws around the
 * screen: a top rail, two sides, and a bottom broken in two so the
 * PAUSE banner sits in the gap. The rectangles are the RDP's, whose
 * lower-right corner is inclusive; see ifCommonBattlePauseProcDisplay. */
SYRectangle dIFCommonBattlePauseBorderRectangle[/* */] =
{
    {  26,  24, 294,  26 },
    {  26,  24,  28, 199 },
    {  26, 197, 190, 199 },
    { 292,  24, 294, 199 },
    { 279, 197, 294, 199 }
};

/* ifcommon.c:275-281: the magnifying glass's colours, one per player
 * colour -- red, blue, yellow, green and the fifth, white */
u8 dIFCommonPlayerMagnifyColorsR[/* */] = { 0xEF, 0x00, 0xFF, 0x00, 0xFF };
u8 dIFCommonPlayerMagnifyColorsG[/* */] = { 0x0D, 0x00, 0xE1, 0xFF, 0xFF };
u8 dIFCommonPlayerMagnifyColorsB[/* */] = { 0x17, 0xFF, 0x00, 0x00, 0xFF };

/* ifcommon.c:411-431, the HUD's own variables */
IFPlayerCommon gIFCommonPlayerInterface;
IFPlayerDamage sIFCommonPlayerDamageInterface[GMCOMMON_PLAYERS_MAX];
s8 sIFCommonPlayerStocksNum[GMCOMMON_PLAYERS_MAX];
GObj *sIFCommonPlayerStocksGObj[GMCOMMON_PLAYERS_MAX];
IFPlayerSteal sIFCommonPlayerStealInterface[GMCOMMON_PLAYERS_MAX];

/* ifcommon.c:398, the magnifying glasses' own: where each player's
 * glass is, the viewport the fighter is drawn through inside it, the
 * GObj carrying its handle and the colour it is drawn in. Not static,
 * as the decomp's is not: the host test reads it. */
IFPlayerMagnify sIFCommonPlayerMagnifyInterface[GMCOMMON_PLAYERS_MAX];

/* ifcommon.c:467, the arrows' own: the FGM plays once every thirty tics
 * while anything is off screen. */
u8 sIFCommonPlayerMagnifySoundWait;

/* ifcommon.c:401, 410, 437, 440, 476, the match clock's own.
 * sIFCommonTimerIsStarted is here rather than with the end state's
 * variables below because it is the clock's, although the one place that
 * writes it is ifCommonBattleUpdateInterfaceAll. */
u8 sIFCommonTimerDigitsInterface[4];
u32 sIFCommonTimerLimit;
u32 sIFCommonTimerStamp;
u32 sIFCommonTimerIsStarted;
ub8 sIFCommonIsAnnouncedSecond[5];

/* ifcommon.c:3214-3217, verbatim */
void ifCommonBattleSetGameStatusWait(void)
{
    gSCManagerBattleState->game_status = nSCBattleGameStatusWait;
}

/* ifcommon.c:2004-2020 ifCommonAnnounceGoSetStatus 0x801121C4, verbatim.
 * The last line switches on the off-screen arrows: nothing points at a fighter
 * who has left the screen until the GO! is over. */
void ifCommonAnnounceGoSetStatus(void)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);

        ftParamUnlockPlayerControl(fighter_gobj);

        fp->camera_mode = nFTCameraModeDefault;

        fighter_gobj = fighter_gobj->link_next;
    }
    gSCManagerBattleState->game_status = nSCBattleGameStatusGo;

    gIFCommonPlayerInterface.is_magnify_display = TRUE;
}

/* ifcommon.c:1954-1961, verbatim: a banner's life, sixty tics */
void ifCommonAnnounceThread(GObj *interface_gobj)
{
    (void)interface_gobj;

    gcSleepCurrentGObjThread(60);

    gcEjectGObj(NULL);

    gcSleepCurrentGObjThread(1);
}

/* ifcommon.c:1964-1980, verbatim: a banner's letters, one SObj each */
void ifCommonAnnounceSetAttr(GObj *interface_gobj, s32 file_id, IFACharacter *character, s32 sprite_count)
{
    SObj *sobj;
    void *sprite_head = gGMCommonFiles[file_id];
    s32 i;

    for (i = 0; i < sprite_count; i++)
    {
        sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, sprite_head, character[i].offset));

        sobj->sprite.attr = SP_TEXSHUF | SP_TRANSPARENT;

        sobj->pos.x = character[i].pos.x;
        sobj->pos.y = character[i].pos.y;
    }
}

/* ifcommon.c:1983-2001 ifCommonAnnounceGoMakeInterface 0x801120D4,
 * verbatim. GO!, on the countdown's fifth second. */
void ifCommonAnnounceGoMakeInterface(void)
{
    void *sprite_head = gGMCommonFiles[1];
    GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);
    s32 i;

    gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(interface_gobj, ifCommonAnnounceThread, nGCProcessKindThread, 5);

    for (i = 0; i < ARRAY_COUNT(dIFCommonAnnounceGoSpriteData); i++)
    {
        SObj *sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, sprite_head, dIFCommonAnnounceGoSpriteData[i].offset));

        sobj->sprite.attr = SP_CLOUD | SP_TEXSHUF; // 0x1000 doesn't exist in base sp.h though?

        sobj->pos.x = dIFCommonAnnounceGoSpriteData[i].pos.x;
        sobj->pos.y = dIFCommonAnnounceGoSpriteData[i].pos.y;
    }
}

/* ifcommon.c:2024-2043, verbatim: one lamp of the countdown's traffic
 * light, by row of dIFCommonTrafficSpriteData */
SObj* ifCommonTrafficMakeSObj(GObj *interface_gobj, s32 id)
{
    SObj *sobj;
    s32 color_id;

    color_id = dIFCommonTrafficSpriteData[id].color_id;

    sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[1], dIFCommonTrafficSpriteOffsets[color_id]));

    sobj->sprite.attr = SP_CLOUD | SP_TEXSHUF;

    sobj->pos.x = dIFCommonTrafficSpriteData[id].pos.x;
    sobj->pos.y = dIFCommonTrafficSpriteData[id].pos.y;

    sobj->sprite.red   = dIFCommonTrafficSpriteColorsR[color_id];
    sobj->sprite.green = dIFCommonTrafficSpriteColorsG[color_id];
    sobj->sprite.blue  = dIFCommonTrafficSpriteColorsB[color_id];

    return sobj;
}

/* ifcommon.c:2046-2190 ifCommonCountdownThread 0x801122F4, verbatim.
 * The traffic light drops in over the first sixty tics, swaps a lamp
 * for a lit one at each second -- the new lamp three times its size
 * and shrinking by a fifth a tic -- and at the fifth second makes GO!,
 * unlocks the fighters and shows the damage digits; then it lifts back
 * out. The three-two-one-GO voices are the announcer's, and play if
 * the FGM bank loaded has them. `scale` is initialised here where the
 * decomp leaves it uninitialised: it is only read after the first lamp
 * swap sets it, and the compiler cannot see that. */
void ifCommonCountdownThread(GObj *interface_gobj)
{
    SObj *sobj;
    SObj *other_sobj;
    SObj *new_sobj;
    s32 timer;
    f32 rscale;
    f32 scale = 1.0F;
    s32 main_status;
    s32 lamp_status;
    SObj *child_sobj;

    for (timer = 0; timer < 60; timer++)
    {
        sobj = SObjGetStruct(interface_gobj);

        while (sobj != NULL)
        {
            sobj->pos.y += 0.8833333F;

            sobj = sobj->next;
        }
        gcSleepCurrentGObjThread(1);
    }
    sobj = ifGetSObj(interface_gobj);

    main_status = lamp_status = -1;

    child_sobj = sobj->next->next;

    while (TRUE)
    {
        switch (timer)
        {
        case I_SEC_TO_TICS(2):
            main_status = lamp_status = 6;

            func_800269C0_275C0(nSYAudioVoiceAnnounceThree);
            break;

        case I_SEC_TO_TICS(3):
            main_status = lamp_status = 7;

            func_800269C0_275C0(nSYAudioVoiceAnnounceTwo);
            break;

        case I_SEC_TO_TICS(4):
            main_status = lamp_status = 8;

            func_800269C0_275C0(nSYAudioVoiceAnnounceOne);
            break;

        case I_SEC_TO_TICS(5):
            ifCommonAnnounceGoMakeInterface();
            ifCommonAnnounceGoSetStatus();
            ifCommonPlayerDamageSetShowInterface();

            main_status = lamp_status = 9;

            func_800269C0_275C0(nSYAudioVoiceAnnounceGo);

            break;

        case I_SEC_TO_TICS(6):
            goto finish;
        }
        if (lamp_status != -1)
        {
            if (main_status != -1)
            {
                gcEjectSObj(sobj->next);
                gcEjectSObj(sobj);

                sobj = ifCommonTrafficMakeSObj(interface_gobj, lamp_status);

                sobj->sprite.scalex = sobj->sprite.scaley = scale = 3.0F;

                new_sobj = ifCommonTrafficMakeSObj(interface_gobj, lamp_status + 5);

                if (lamp_status == 9)
                {
                    other_sobj = child_sobj->prev;

                    child_sobj->sprite.red = dIFCommonTrafficGoBacklightR[1];
                    child_sobj->sprite.green = dIFCommonTrafficGoBacklightG[1];
                    child_sobj->sprite.blue = dIFCommonTrafficGoBacklightB[1];

                    child_sobj->envcolor.r = dIFCommonTrafficGoShadowR[1];
                    child_sobj->envcolor.g = dIFCommonTrafficGoShadowG[1];
                    child_sobj->envcolor.b = dIFCommonTrafficGoShadowB[1];

                    if (other_sobj != NULL)
                    {
                        other_sobj->next = child_sobj->next;
                    }
                    other_sobj = child_sobj->next;

                    if (other_sobj != NULL)
                    {
                        other_sobj->prev = child_sobj->prev;
                    }
                    new_sobj->next = child_sobj;

                    child_sobj->prev = new_sobj;
                    child_sobj->next = NULL;
                }
            }
            if (scale != 1.0F)
            {
                scale -= 0.2F;

                if (scale < 1.0F)
                {
                    scale = 1;
                }
                sobj->sprite.scalex = sobj->sprite.scaley = scale;

                rscale = (scale - 1.0F) * 0.5F;

                sobj->pos.x = dIFCommonTrafficSpriteData[lamp_status].pos.x - (rscale * sobj->sprite.width);
                sobj->pos.y = dIFCommonTrafficSpriteData[lamp_status].pos.y - (rscale * sobj->sprite.height);
            }
        }
        timer++;

        gcSleepCurrentGObjThread(1);

        main_status = -1;
    }
finish:
    for (timer = timer; timer < I_SEC_TO_TICS(7); timer++)
    {
        sobj = SObjGetStruct(interface_gobj);

        while (sobj != NULL)
        {
            sobj->pos.y += (-0.8833333F);

            sobj = sobj->next;
        }
        gcSleepCurrentGObjThread(1);
    }
    gcEjectGObj(NULL);
    gcSleepCurrentGObjThread(1);
}

/* ifcommon.c:2193-2241 ifCommonCountdownMakeInterface 0x80112668,
 * verbatim: the rod, its frame, the six dim lamps, the GO lamp's
 * backlight and the rod's shadow, all above the screen (file 82's
 * Rod 0x20990, Frame 0x21760, RodShadow 0x21878). */
SObj* ifCommonCountdownMakeInterface(void)
{
    GObj *interface_gobj;
    SObj *sobj;
    s32 i;

    interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(interface_gobj, ifCommonCountdownThread, nGCProcessKindThread, 5);

    sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[1], 0x20990));

    sobj->sprite.attr = SP_TEXSHUF | SP_TRANSPARENT;

    sobj->pos.x = 103.0F;
    sobj->pos.y = -57.0F;

    sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[1], 0x21760));

    sobj->sprite.attr = SP_TEXSHUF | SP_TRANSPARENT;

    sobj->pos.x = 111.0F;
    sobj->pos.y = -23.0F;

    for (i = 0; i < 6; i++)
    {
        sobj = ifCommonTrafficMakeSObj(interface_gobj, i);
    }
    ifSetSObj(interface_gobj, sobj);

    ifCommonTrafficMakeSObj(interface_gobj, 10);

    sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[1], 0x21878));

    sobj->sprite.attr = SP_TEXSHUF | SP_TRANSPARENT;

    sobj->pos.x = 182.0F;
    sobj->pos.y = -11.0F;

    sobj->sprite.red   = dIFCommonTrafficGoBacklightR[0];
    sobj->sprite.green = dIFCommonTrafficGoBacklightG[0];
    sobj->sprite.blue  = dIFCommonTrafficGoBacklightB[0];
    sobj->envcolor.r = dIFCommonTrafficGoShadowR[0];
    sobj->envcolor.g = dIFCommonTrafficGoShadowG[0];
    sobj->envcolor.b = dIFCommonTrafficGoShadowB[0];

    return sobj;
}

/* ifcommon.c:2253-2296 ifCommonEntryFocusThread 0x80112880, verbatim:
 * focus id 2 closes the camera in on each fighter as it appears. */
void ifCommonEntryFocusThread(GObj *interface_gobj)
{
    GObj *fighter_gobj;
    s32 id = interface_gobj->user_data.s;
    s32 sleep_tics = dIFCommonEntryFocusSleepTics[id];
    s32 count;

    if (id == 1)
    {
        gcSleepCurrentGObjThread(90);
    }
    count = gSCManagerBattleState->pl_count + gSCManagerBattleState->cp_count;

    if (count < 3)
    {
        gcSleepCurrentGObjThread(sleep_tics);
    }
    fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    while (fighter_gobj != NULL)
    {
        ftCommonAppearSetStatus(fighter_gobj);

        if (id == 2)
        {
            gcSleepCurrentGObjThread(30);
            gmCameraSetStatusPlayerZoom(fighter_gobj, 0.0F, 0.0F, ftGetStruct(fighter_gobj)->attr->closeup_camera_zoom, 0.1F, 28.0F);
            gcSleepCurrentGObjThread(sleep_tics - 30);
        }
        else gcSleepCurrentGObjThread(sleep_tics);

        fighter_gobj = fighter_gobj->link_next;
    }
    if (id == 2)
    {
        gcSleepCurrentGObjThread(30);
        gmCameraSetStatusDefault();
    }
    gcEjectGObj(NULL);
    gcSleepCurrentGObjThread(1);
}

/* ifcommon.c:2299-2305 ifCommonEntryFocusMakeInterface 0x801129DC, verbatim */
void ifCommonEntryFocusMakeInterface(s32 id)
{
    GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterfaceActor, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjProcess(interface_gobj, ifCommonEntryFocusThread, nGCProcessKindThread, 5);

    interface_gobj->user_data.s = id;
}

/* ifcommon.c:2308-2315 ifCommonEntryAllThread 0x80112A34, verbatim */
void ifCommonEntryAllThread(GObj *interface_gobj)
{
    (void)interface_gobj;

    gcSleepCurrentGObjThread(90);
    ifCommonCountdownMakeInterface();
    ifCommonEntryFocusMakeInterface(syUtilsRandIntRange(3));
    gcEjectGObj(NULL);
    gcSleepCurrentGObjThread(1);
}

/* ifcommon.c:2318-2323 ifCommonEntryAllMakeInterface 0x80112A80, verbatim */
void ifCommonEntryAllMakeInterface(void)
{
    gcAddGObjProcess(gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterfaceActor, GOBJ_PRIORITY_DEFAULT), ifCommonEntryAllThread, nGCProcessKindThread, 5);

    gSCManagerBattleState->game_status = nSCBattleGameStatusWait;
}

/* ifcommon.c:2326-2336 ifCommonSuddenDeathThread 0x80112AD0, verbatim.
 * The entry sequence's replacement in a sudden death: the fighters are
 * already standing (ftManagerMakeFighter's is_skip_entry arm puts them
 * in Wait or Fall), so there is no countdown and no entry focus -- the
 * thread sleeps the same 90 tics the entry-all thread does and then
 * goes straight to GO!. */
void ifCommonSuddenDeathThread(GObj *interface_gobj)
{
    (void)interface_gobj;

    gcSleepCurrentGObjThread(90);
    ifCommonAnnounceGoMakeInterface();
    ifCommonPlayerDamageSetShowInterface();
    ifCommonAnnounceGoSetStatus();
    func_800269C0_275C0(nSYAudioVoiceAnnounceGo);
    gcEjectGObj(NULL);
    gcSleepCurrentGObjThread(1);
}

/* ifcommon.c:2338-2354 ifCommonAnnounceSetColors 0x80112B24, verbatim:
 * paint every SObj of a banner the one colour pair, primitive and
 * environment. Only the banners drawn out of the plain alphabet need
 * it (this one, and 1P mode's SUCCESS and FAILURE). */
void ifCommonAnnounceSetColors(GObj *interface_gobj, SYColorRGBPair *colors)
{
    SObj *sobj = SObjGetStruct(interface_gobj);

    while (sobj != NULL)
    {
        sobj->sprite.red   = colors->prim.r;
        sobj->sprite.green = colors->prim.g;
        sobj->sprite.blue  = colors->prim.b;

        sobj->envcolor.r = colors->env.r;
        sobj->envcolor.g = colors->env.g;
        sobj->envcolor.b = colors->env.b;

        sobj = sobj->next;
    }
}

/* ifcommon.c:2356-2367 ifCommonSuddenDeathMakeInterface 0x80112B74,
 * verbatim. Unlike ifCommonEntryAllMakeInterface, whose GObj is a bare
 * runner on the actor link, this one is a banner as well: it carries
 * the twelve letters and draws them, and the thread on it is what ends
 * the wait. The thread ends with gcEjectGObj(NULL), and that ejects the
 * banner GObj with it: a NULL (or self) argument sets sGCRunStatus to
 * nGCRunStatusEject, and gcRunGObjProcess (objman.c:2175-2185) answers
 * that by calling gcEjectGObj on the process's *parent* GObj. Dropping
 * only the process is a different status, nGCRunStatusEnd. So SUDDEN
 * DEATH shows for the thread's 90 tics and then goes, which is what the
 * N64 does -- this thread is verbatim. */
void ifCommonSuddenDeathMakeInterface(void)
{
    GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(interface_gobj, ifCommonSuddenDeathThread, nGCProcessKindThread, 5);
    ifCommonAnnounceSetAttr(interface_gobj, 7, dIFCommonAnnounceSuddenDeathSpriteData, ARRAY_COUNT(dIFCommonAnnounceSuddenDeathSpriteData));
    ifCommonAnnounceSetColors(interface_gobj, &dIFCommonAnnounceSuddenDeathSpriteColors);
    func_800269C0_275C0(nSYAudioVoiceAnnounceSuddenDeath);

    gSCManagerBattleState->game_status = nSCBattleGameStatusWait;
}

/* ---- the match clock: if/ifcommon.c's timer, verbatim.
 * A TIME match is pickable from the VS mode screen.
 *
 * Two GObjs. ifCommonTimerMakeInterface puts a runner on the interface
 * actor link carrying, in its user_data, the function to call when the
 * clock reaches zero; ifCommonTimerMakeDigits puts four digits and a
 * colon on the interface link, on DL link 23 with the rest of the HUD.
 * The digits exist only in a TIME match with a finite limit, and the
 * runner exists always -- it is what fills time_passed, which the
 * results screen reads whatever the rule.
 *
 * DIVERGES: nothing in this section. The clock's two odd corners are the
 * game's own and are kept:
 *   - the displayed time is time_remain + 59 tics on every tic but the
 *     first, so a limit of 2:00 reads "2:00" for a whole second rather
 *     than flicking to 1:59 immediately;
 *   - at zero the function skips to the fourth digit and writes a 0 into
 *     it without touching the other three, which is why a finished clock
 *     reads the last minute-and-tens it drew with a 0 on the end.
 * ------------------------------------------------------------------ */

/* ifcommon.c:2244-2253 ifCommonAnnounceTimeUpMakeInterface 0x80112814,
 * verbatim: the TIME UP banner, the same six-letter machinery GAME SET
 * uses out of the same file. */
GObj* ifCommonAnnounceTimeUpMakeInterface(void)
{
    GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

    ifCommonAnnounceSetAttr(interface_gobj, 1, dIFCommonAnnounceTimeUpSpriteData, ARRAY_COUNT(dIFCommonAnnounceTimeUpSpriteData));

    return interface_gobj;
}

/* ifcommon.c:2369-2413 ifCommonTimerProcDisplay 0x80112C18, verbatim.
 * Runs on the draw, not the update: it re-cuts a digit's sprite only
 * when that digit changed, which is what sIFCommonTimerDigitsInterface
 * remembers (and why ifCommonTimerMakeDigits seeds it with 10, a value
 * no digit can be). */
void ifCommonTimerProcDisplay(GObj *interface_gobj)
{
    s32 digit;
    s32 i;
    s32 time;
    SObj *sobj;

    sobj = SObjGetStruct(interface_gobj);

    if (gSCManagerBattleState->time_remain == 0)
    {
        sobj = sobj->next->next->next;

        sobj->sprite = *lbRelocGetFileData(Sprite*, gGMCommonFiles[3], dIFCommonTimerDigitSpriteOffsets[0]);

        sobj->pos.x = (s32)(dIFCommonTimerDigitsSpritePositionsX[3] - (sobj->sprite.width * 0.5F));
        sobj->pos.y = (s32)(30.0F - (sobj->sprite.height * 0.5F));
    }
    else
    {
        if (gSCManagerBattleState->time_remain == sIFCommonTimerLimit)
        {
            time = gSCManagerBattleState->time_remain;
        }
        else time = gSCManagerBattleState->time_remain + 59;

        for (i = 0; i < ARRAY_COUNT(sIFCommonTimerDigitsInterface); i++, sobj = sobj->next)
        {
            digit = time / dIFCommonTimerDigitsUnitLengths[i];

            time -= (digit * dIFCommonTimerDigitsUnitLengths[i]);

            if (sIFCommonTimerDigitsInterface[i] != digit)
            {
                sobj->sprite = *lbRelocGetFileData(Sprite*, gGMCommonFiles[3], dIFCommonTimerDigitSpriteOffsets[digit]);

                sobj->pos.x = (s32)(dIFCommonTimerDigitsSpritePositionsX[i] - (sobj->sprite.width * 0.5F));
                sobj->pos.y = (s32)(30.0F - (sobj->sprite.height * 0.5F));

                sIFCommonTimerDigitsInterface[i] = digit;
            }
        }
    }
    lbCommonDrawSObjAttr(interface_gobj);
}

/* ifcommon.c:2418-2427, verbatim: the attr every clock sprite is drawn
 * with, written onto the bank's records once. */
void ifCommonTimerSetAttr(void)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(dIFCommonTimerDigitSpriteOffsets); i++)
    {
        lbRelocGetFileData(Sprite*, gGMCommonFiles[3], dIFCommonTimerDigitSpriteOffsets[i])->attr = SP_TEXSHUF | SP_TRANSPARENT;
    }
}

/* ifcommon.c:2429-2438, verbatim */
void ifCommonTimerInitAnnouncedSeconds(void)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sIFCommonIsAnnouncedSecond); i++)
    {
        sIFCommonIsAnnouncedSecond[i] = FALSE;
    }
}

/* ifcommon.c:2440-2470 ifCommonTimerMakeDigits 0x80112F68, verbatim.
 * Returns early on a stock match or an infinite limit, which is the one
 * place the game decides whether the clock is on screen at all. */
SObj* ifCommonTimerMakeDigits(void)
{
    GObj *interface_gobj;
    SObj *sobj;

    if (!(gSCManagerBattleState->game_rules & SCBATTLE_GAMERULE_TIME) || (gSCManagerBattleState->time_limit == SCBATTLE_TIMELIMIT_INFINITE))
    {
        return NULL;
    }

    ifCommonTimerSetAttr();

    interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(interface_gobj, ifCommonTimerProcDisplay, 23, GOBJ_PRIORITY_DEFAULT, ~0);

    lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[3], dIFCommonTimerDigitSpriteOffsets[0]));
    lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[3], dIFCommonTimerDigitSpriteOffsets[0]));
    lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[3], dIFCommonTimerDigitSpriteOffsets[0]));
    lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[3], dIFCommonTimerDigitSpriteOffsets[0]));

    sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[3], dIFCommonTimerDigitSpriteOffsets[10]));

    sobj->pos.x = (s32)(260.0F - (sobj->sprite.width * 0.5F));
    sobj->pos.y = (s32)(30.0F - (sobj->sprite.height * 0.5F));

    sIFCommonTimerDigitsInterface[0] = sIFCommonTimerDigitsInterface[1] = sIFCommonTimerDigitsInterface[2] = sIFCommonTimerDigitsInterface[3] = 10;

    return sobj;
}

/* ifcommon.c:2472-2532 ifCommonTimerFuncRun 0x80113104, verbatim: the
 * clock itself, one tic of it.
 *
 * It does not count its own tics -- it takes the difference between the
 * scheduler's retrace count and its own stamp, so a frame the game did
 * not get to still moves the clock by the right amount. The port counts
 * that in syTaskmanRunFrame (src/dc/taskman.c on why a retrace and a tic
 * are the same thing here).
 *
 * The last five seconds do three things: the announcer counts them, the
 * music fades from full volume down to 10240, and Mushroom Kingdom
 * switches to its hurry-up track at thirty, through the item-music
 * override (src/dc/ftparam.c) that puts it back. */
void ifCommonTimerFuncRun(GObj *interface_gobj)
{
    u32 time_delta;
    u32 time_update;
    s32 i;

    if (sIFCommonTimerIsStarted != FALSE)
    {
        time_update = sySchedulerGetTicCount();
        time_delta = time_update - sIFCommonTimerStamp;

        if (time_delta != 0)
        {
            sIFCommonTimerStamp = time_update;
            gSCManagerBattleState->time_passed += time_delta;

            if ((gSCManagerBattleState->game_rules & SCBATTLE_GAMERULE_TIME) && (gSCManagerBattleState->time_limit != SCBATTLE_TIMELIMIT_INFINITE))
            {
                if (gSCManagerBattleState->time_remain != 0)
                {
                    if (gSCManagerBattleState->time_remain < time_delta)
                    {
                        gSCManagerBattleState->time_remain = 0;
                    }
                    else gSCManagerBattleState->time_remain -= time_delta;

                    if
                    (
                        (gSCManagerBattleState->gkind == nGRKindInishie)          &&
                        (gSCManagerBattleState->time_remain <= I_SEC_TO_TICS(30)) &&
                        (gMPCollisionBGMDefault != nSYAudioBGMInishieHurry)
                    )
                    {
                        gMPCollisionBGMDefault = nSYAudioBGMInishieHurry;
                        ftParamTryUpdateItemMusic();
                    }
                    if (gSCManagerBattleState->time_remain <= I_SEC_TO_TICS(5))
                    {
                        if (gSCManagerBattleState->time_remain == 0)
                        {
                            ifGetProc(interface_gobj)();

                            gcEjectGObj(NULL);
                        }
                        else for (i = 0; i < ARRAY_COUNT(sIFCommonIsAnnouncedSecond); i++)
                        {
                            if ((sIFCommonIsAnnouncedSecond[i] == FALSE) && (gSCManagerBattleState->time_remain <= (I_SEC_TO_TICS(i) + I_SEC_TO_TICS(1))))
                            {
                                func_800269C0_275C0(dIFCommonAnnounceTimerVoiceIDs[i]);

                                sIFCommonIsAnnouncedSecond[i] = TRUE;
                            }
                        }
                        syAudioSetBGMVolume(0, ((gSCManagerBattleState->time_remain / F_SEC_TO_TICS(5)) * 20480.0F) + 10240.0F);
                    }
                }
            }
        }
    }
}

/* ifcommon.c:2534-2543 ifCommonTimerMakeInterface 0x80113398, verbatim.
 * scVSBattleStartBattle names ifCommonAnnounceTimeUpInitInterface as the
 * proc; the 1P game's stages name their own. */
void ifCommonTimerMakeInterface(void (*proc)(void))
{
    gSCManagerBattleState->time_remain = sIFCommonTimerLimit = I_MIN_TO_TICS(gSCManagerBattleState->time_limit);
    gSCManagerBattleState->time_passed = 0;

    sIFCommonTimerIsStarted = FALSE;

    ifCommonTimerInitAnnouncedSeconds();
    ifSetProc(gcMakeGObjSPAfter(nGCCommonKindInterface, ifCommonTimerFuncRun, nGCCommonLinkIDInterfaceActor, GOBJ_PRIORITY_DEFAULT), proc);
}

/* ---- the HUD: if/ifcommon.c's damage digits, stock rows
 * and player tags, verbatim on the port's sprite pass. Each row is a
 * GObj on the interface link with its SObjs chained under it, drawn by
 * the interface camera (src/dc/gmcamera.c gmCameraMakeInterfaceCamera)
 * on DL link 23.
 *
 * DIVERGES, listed here rather than at each site:
 *   - ifCommonPlayerDamageProcDisplay's one gDPSetCombineLERP is the
 *     port's lbCommonSpriteSetCombine (src/dc/lbcommon.h on the
 *     spelling): the flash frame draws the digits in the primitive
 *     colour rather than lit by it.
 * ------------------------------------------------------------------ */

/* ifcommon.c:485-496, verbatim */
void ifCommonPlayerDamageSetShowInterface(void)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sIFCommonPlayerDamageInterface); i++)
    {
        if (sIFCommonPlayerDamageInterface[i].interface_gobj != NULL)
        {
            sIFCommonPlayerDamageInterface[i].is_show_interface = TRUE;
        }
    }
}

/* ifcommon.c:499-523, verbatim - Gets position of special character (%
 * or H.P.) in damage display character array? */
s32 ifCommonPlayerDamageGetSpecialArrayID(s32 damage, u8 *digits)
{
    u8 *digits_start = digits;
    s32 digit_update = 1;

    if (damage >= 10)
    {
        do
        {
            digit_update *= 10;
        }
        while ((damage / digit_update) >= 10);
    }
    do
    {
        *digits++ = damage / digit_update;

        damage %= digit_update;

        digit_update /= 10;
    }
    while (digit_update != 0);

    return digits - digits_start;
}

/* ifcommon.c:526-533, verbatim */
s32 ifCommonPlayerDamageGetPercentArrayID(s32 damage, u8 *digits)
{
    s32 id = ifCommonPlayerDamageGetSpecialArrayID(damage, digits);

    digits[id] = 0xA;

    return id + 1;
}

/* ifcommon.c:536-543, verbatim */
s32 ifCommonPlayerDamageGetHitPointsArrayID(s32 damage, u8 *digits)
{
    s32 id = ifCommonPlayerDamageGetSpecialArrayID(damage, digits);

    digits[id] = 0xB;

    return id + 1;
}

/* ifcommon.c:546-555, verbatim */
s32 ifCommonPlayerDamageGetDigitOffset(s32 digit_count, u8 *digit_ids)
{
    s32 i, offset = 0;

    for (i = 0; i < digit_count; i++)
    {
        offset += dIFCommonPlayerDamageDigitWidths[digit_ids[i]];
    }
    return offset;
}

/* ifcommon.c:558-685 ifCommonPlayerDamageUpdateDigits 0x8010E8F4,
 * verbatim. The digits' per-tic update: a rise in damage scales them up
 * by a three-hundredth per point and flashes them white for a tic,
 * then they settle back a twentieth a tic; the characters are laid out
 * right to left from the row's centre. */
void ifCommonPlayerDamageUpdateDigits(GObj *interface_gobj)
{
    s32 player;
    IFDCharacter *ifchar;
    SObj *sobj;
    s32 damage_scale;
    s32 start_damage;
    s32 char_count;
    s32 hitpoints;
    s32 digit_id;
    s32 color_id; // I don't know
    s32 pos_adjust_wait;
    s32 flash_reset_wait;
    f32 scale;
    u8 digits[4];
    s32 sprite_id;
    f32 offset;
    s32 damage;
    f32 pos_x;

    player = ifGetPlayer(interface_gobj);
    damage = start_damage = gSCManagerBattleState->players[player].stock_damage_all;

    if (damage > 999)
    {
        damage = 999;
    }
    scale = sIFCommonPlayerDamageInterface[player].scale;
    damage_scale = start_damage - sIFCommonPlayerDamageInterface[player].damage;

    if (damage_scale == 0)
    {
        if (scale == 1.0F)
        {
            return;
        }
    }
    else if (damage_scale < 0)
    {
        scale = 1.0F;
    }
    else
    {
        sIFCommonPlayerDamageInterface[player].pos_adjust_wait = 4;
        sIFCommonPlayerDamageInterface[player].flash_reset_wait = 1;

        scale = (damage_scale / 300.0F) + 1.0F;
    }
    pos_adjust_wait = sIFCommonPlayerDamageInterface[player].pos_adjust_wait;
    flash_reset_wait = sIFCommonPlayerDamageInterface[player].flash_reset_wait;

    if (flash_reset_wait != 0)
    {
        color_id = GMCOMMON_PLAYERS_MAX;
    }
    else color_id = player;

    if (gSCManagerBattleState->players[player].fkind == nFTKindBoss)
    {
        hitpoints = 300 - damage;

        if (hitpoints < 0)
        {
            hitpoints = 0;
        }
        sIFCommonPlayerDamageInterface[player].char_display_count = char_count = ifCommonPlayerDamageGetHitPointsArrayID(hitpoints, digits);
    }
    else sIFCommonPlayerDamageInterface[player].char_display_count = char_count = ifCommonPlayerDamageGetPercentArrayID(damage, digits);

    pos_x = (ifCommonPlayerDamageGetDigitOffset(char_count, digits) * scale * 0.5F);

    pos_x = gIFCommonPlayerInterface.player_pos_x[player] + pos_x;

    if ((scale > 1.0F) && (pos_adjust_wait == 0))
    {
        scale -= 0.05F;

        if (scale < 1.0F)
        {
            scale = 1.0F;
        }
    }
    digit_id = char_count - 1;

    sobj = SObjGetStruct(interface_gobj)->next;

    while (sobj != NULL)
    {
        if (digit_id < 0)
        {
            sobj->sprite.attr |= SP_HIDDEN;
        }
        else
        {
            sprite_id = digits[digit_id];
            ifchar = sobj->user_data.p;

            ifchar->image_id = sprite_id;

            offset = dIFCommonPlayerDamageDigitWidths[sprite_id] * scale;

            ifchar->pos.x = (pos_x - (offset * 0.5F));
            ifchar->pos.y = gIFCommonPlayerInterface.player_pos_y;

            pos_x -= offset;

            sobj->sprite.attr &= ~SP_HIDDEN;
        }
        sobj = sobj->next;

        digit_id--;
    }
    if (pos_adjust_wait > 0)
    {
        pos_adjust_wait--;
    }
    if (flash_reset_wait > 0)
    {
        flash_reset_wait--;
    }
    sIFCommonPlayerDamageInterface[player].damage = start_damage;
    sIFCommonPlayerDamageInterface[player].scale = scale;
    sIFCommonPlayerDamageInterface[player].color_id = color_id;

    sIFCommonPlayerDamageInterface[player].pos_adjust_wait = pos_adjust_wait;
    sIFCommonPlayerDamageInterface[player].flash_reset_wait = flash_reset_wait;
}

/* ifcommon.c:688-746 ifCommonPlayerDamageUpdateAnim 0x8010EC50,
 * verbatim: the break animation, the digits shaken loose one every six
 * tics and falling under gravity, when a player loses a stock. */
void ifCommonPlayerDamageUpdateAnim(GObj *interface_gobj)
{
    s32 player;
    s32 char_id;
    s32 random;
    s32 modulo;
    s32 i, j;
    IFDCharacter *ifchar;
    SObj *sobj;

    player = ifGetPlayer(interface_gobj);

    if (sIFCommonPlayerDamageInterface[player].break_anim_frame < 19)
    {
        modulo = sIFCommonPlayerDamageInterface[player].break_anim_frame / 6;

        if (!(sIFCommonPlayerDamageInterface[player].break_anim_frame - modulo * 6))
        {
            char_id = sIFCommonPlayerDamageInterface[player].char_display_count - modulo;

            if (char_id > 0)
            {
                random = syUtilsRandIntRange(char_id);

                for (i = j = 0; i < sIFCommonPlayerDamageInterface[player].char_display_count; i++)
                {
                    if (sIFCommonPlayerDamageInterface[player].chars[i].is_lock_movement == FALSE)
                    {
                        if (j == random)
                        {
                            break;
                        }
                        else j++;
                    }
                }
                sIFCommonPlayerDamageInterface[player].chars[i].is_lock_movement = TRUE;
            }
        }
        sIFCommonPlayerDamageInterface[player].break_anim_frame++;
    }
    sobj = SObjGetStruct(interface_gobj)->next;

    while (sobj != NULL)
    {
        if (!(sobj->sprite.attr & SP_HIDDEN))
        {
            ifchar = sobj->user_data.p;

            if (ifchar->is_lock_movement != FALSE)
            {
                ifchar->vel.y++;

                ifchar->pos.x += ifchar->vel.x;
                ifchar->pos.y += ifchar->vel.y;
            }
        }
        sobj = sobj->next;
    }
}

/* ifcommon.c:749-769, verbatim */
void ifCommonPlayerDamageProcUpdate(GObj *interface_gobj)
{
    s32 player = ifGetPlayer(interface_gobj);

    if (gSCManagerBattleState->players[player].stock_count == -1)
    {
        if (sIFCommonPlayerDamageInterface[player].dead_stopupdate_wait != 0)
        {
            if (sIFCommonPlayerDamageInterface[player].is_update_anim != FALSE)
            {
                ifCommonPlayerDamageUpdateAnim(interface_gobj);
            }
            sIFCommonPlayerDamageInterface[player].dead_stopupdate_wait--;
        }
    }
    else if (sIFCommonPlayerDamageInterface[player].is_update_anim != FALSE)
    {
        ifCommonPlayerDamageUpdateAnim(interface_gobj);
    }
    else ifCommonPlayerDamageUpdateDigits(interface_gobj);
}

/* ifcommon.c:772-879 ifCommonPlayerDamageProcDisplay 0x8010EEFC, verbatim
 * but for the combiner line (see the section head). The first SObj is
 * the emblem, drawn whether or not the digits show; each digit SObj
 * takes the Sprite its character names and is drawn at the character's
 * position, in the player's colour darkened towards (0x64, 0x14, 0x14)
 * as the damage climbs to 300, or white on the flash frame. */
void ifCommonPlayerDamageProcDisplay(GObj *interface_gobj)
{
    f32 pos_x;
    f32 scale;
    f32 pos_y;
    f32 damage_scale;
    s32 player;
    s32 color_id;
    u8 color_r;
    u8 color_g;
    u8 color_b;
    SObj *sobj;
    IFDCharacter *ifchar;

    sobj = SObjGetStruct(interface_gobj);

    lbCommonPrepSObjAttr(gSYTaskmanDLHeads, sobj);
    lbCommonPrepSObjDraw(gSYTaskmanDLHeads, sobj);

    lbCommonSetExternSpriteParams(&sobj->sprite);

    player = ifGetPlayer(interface_gobj);

    if
    (
        (sIFCommonPlayerDamageInterface[player].is_show_interface != FALSE) &&
        (
            (gSCManagerBattleState->players[player].stock_count >= 0)       ||
            (sIFCommonPlayerDamageInterface[player].dead_stopupdate_wait != 0)
        )
    )
    {
        color_id = sIFCommonPlayerDamageInterface[player].color_id;
        scale = sIFCommonPlayerDamageInterface[player].scale;

        if (color_id == GMCOMMON_PLAYERS_MAX)
        {
            color_r = dIFCommonPlayerDamageDigitColorsR[color_id];
            color_g = dIFCommonPlayerDamageDigitColorsG[color_id];
            color_b = dIFCommonPlayerDamageDigitColorsB[color_id];
        }
        else
        {
            damage_scale = 1.0F - (sIFCommonPlayerDamageInterface[player].damage / 300.0F);

            if (damage_scale < 0.0F)
            {
                damage_scale = 0.0F;
            }
            color_r = (s32) ((dIFCommonPlayerDamageDigitColorsR[color_id] - 0x64) * damage_scale) + 0x64;
            color_g = (s32) ((dIFCommonPlayerDamageDigitColorsG[color_id] - 0x14) * damage_scale) + 0x14;
            color_b = (s32) ((dIFCommonPlayerDamageDigitColorsB[color_id] - 0x14) * damage_scale) + 0x14;
        }
        sobj = sobj->next;
        ifchar = sobj->user_data.p;

        sobj->sprite = *lbRelocGetFileData(Sprite*, gGMCommonFiles[2], dIFCommonPlayerDamageDigitSpriteOffsets[ifchar->image_id]);

        sobj->pos.x = (ifchar->pos.x - (sobj->sprite.width * 0.5F * scale));
        sobj->pos.y = (ifchar->pos.y - (sobj->sprite.height * 0.5F * scale));

        sobj->sprite.scalex = scale;
        sobj->sprite.scaley = scale;

        sobj->sprite.red = color_r;
        sobj->sprite.green = color_g;
        sobj->sprite.blue = color_b;

        lbCommonPrepSObjAttr(gSYTaskmanDLHeads, sobj);

        if (color_id == GMCOMMON_PLAYERS_MAX)
        {
            /* gDPSetCombineLERP(gSYTaskmanDLHeads[0]++, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0); */
            lbCommonSpriteSetCombine(nLBCommonCombineIPrim);
        }
        lbCommonPrepSObjDraw(gSYTaskmanDLHeads, sobj);

        sobj = sobj->next;

        while (sobj != NULL)
        {
            if (!(sobj->sprite.attr & SP_HIDDEN))
            {
                ifchar = sobj->user_data.p;

                sobj->sprite = *lbRelocGetFileData(Sprite*, gGMCommonFiles[2], dIFCommonPlayerDamageDigitSpriteOffsets[ifchar->image_id]);

                pos_x = ifchar->pos.x - (sobj->sprite.width * 0.5F * scale);
                pos_y = ifchar->pos.y - (sobj->sprite.height * 0.5F * scale);

                if ((scale == 1.0F) && (sIFCommonPlayerDamageInterface[player].is_update_anim == FALSE))
                {
                    sobj->pos.x = (s32)pos_x;
                    sobj->pos.y = (s32)pos_y;
                }
                else
                {
                    sobj->pos.x = pos_x;
                    sobj->pos.y = pos_y;
                }
                sobj->sprite.scalex = sobj->sprite.scaley = scale;

                lbCommonPrepSObjDraw(gSYTaskmanDLHeads, sobj);
            }
            sobj = sobj->next;
        }
        lbCommonClearExternSpriteParams();
    }
}

/* ifcommon.c:882-890, verbatim */
void ifCommonPlayerDamageSetDigitAttr(void)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(dIFCommonPlayerDamageDigitSpriteOffsets); i++)
    {
        lbRelocGetFileData(Sprite*, gGMCommonFiles[2], dIFCommonPlayerDamageDigitSpriteOffsets[i])->attr = SP_TEXSHUF | SP_TRANSPARENT;
    }
}

/* ifcommon.c:893-897, verbatim */
void ifCommonPlayerDamageSetDigitPositions(void)
{
    gIFCommonPlayerInterface.player_pos_x = dIFCommonPlayerDamagePositionOffsetsX;
    gIFCommonPlayerInterface.player_pos_y = 210;
}

/* ifcommon.c:900-979 ifCommonPlayerDamageInitInterface 0x8010F3C0,
 * verbatim. One GObj per player: the emblem SObj first (in the stage's
 * colour for that player, mp/mptypes.h emblem_colors, or a hidden
 * placeholder for a kind with no sprites), then four digit SObjs each
 * pointing at its IFDCharacter. */
void ifCommonPlayerDamageInitInterface(void)
{
    FTStruct *fp;
    FTSprites *ft_sprites;
    GObj *interface_gobj;
    SObj *sobj;
    s32 player;
    s32 emblem;

    ifCommonPlayerDamageSetDigitAttr();

    for (player = 0; player < ARRAY_COUNT(sIFCommonPlayerDamageInterface); player++)
    {
        if (gSCManagerBattleState->players[player].pkind == nFTPlayerKindNot)
        {
            sIFCommonPlayerDamageInterface[player].interface_gobj = NULL;
        }
        else
        {
            interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

            sIFCommonPlayerDamageInterface[player].interface_gobj = interface_gobj;

            gcAddGObjDisplay(interface_gobj, ifCommonPlayerDamageProcDisplay, 23, GOBJ_PRIORITY_DEFAULT, ~0);

            fp = ftGetStruct(gSCManagerBattleState->players[player].fighter_gobj);

            ft_sprites = fp->attr->sprites;

            if ((ft_sprites != NULL) && (ft_sprites->emblem != NULL))
            {
                sobj = lbCommonMakeSObjForGObj(interface_gobj, ft_sprites->emblem);

                sobj->pos.x = (s32)
                (
                    (gIFCommonPlayerInterface.player_pos_x[player] -
                    (sobj->sprite.width  * dIFCommonPlayerDamageEmblemScales[player] * 0.5F)) + dIFCommonPlayerDamageEmblemOffsetsX[player]
                );
                sobj->pos.y = (s32)
                (
                    (gIFCommonPlayerInterface.player_pos_y         -
                    (sobj->sprite.height * dIFCommonPlayerDamageEmblemScales[player] * 0.5F)) + dIFCommonPlayerDamageEmblemOffsetsY[player]
                );

                sobj->sprite.scalex = sobj->sprite.scaley = dIFCommonPlayerDamageEmblemScales[player];

                emblem = gSCManagerBattleState->players[player].color;

                sobj->sprite.red = gMPCollisionGroundData->emblem_colors[emblem].r;
                sobj->sprite.green = gMPCollisionGroundData->emblem_colors[emblem].g;
                sobj->sprite.blue = gMPCollisionGroundData->emblem_colors[emblem].b;

                sobj->sprite.attr = SP_TEXSHUF | SP_TRANSPARENT;
            }
            else
            {
                gcAddSObjForGObj(interface_gobj, NULL)->sprite.attr = SP_HIDDEN;
            }
            lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[2], dIFCommonPlayerDamageDigitSpriteOffsets[0]))->user_data.p = &sIFCommonPlayerDamageInterface[player].chars[0];
            lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[2], dIFCommonPlayerDamageDigitSpriteOffsets[0]))->user_data.p = &sIFCommonPlayerDamageInterface[player].chars[1];
            lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[2], dIFCommonPlayerDamageDigitSpriteOffsets[0]))->user_data.p = &sIFCommonPlayerDamageInterface[player].chars[2];
            lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[2], dIFCommonPlayerDamageDigitSpriteOffsets[0]))->user_data.p = &sIFCommonPlayerDamageInterface[player].chars[3];

            // The above functions should all return SObj*

            sIFCommonPlayerDamageInterface[player].damage = gSCManagerBattleState->players[player].stock_damage_all;
            sIFCommonPlayerDamageInterface[player].pos_adjust_wait = 0;
            sIFCommonPlayerDamageInterface[player].flash_reset_wait = 0;
            sIFCommonPlayerDamageInterface[player].scale = 1.04F;
            sIFCommonPlayerDamageInterface[player].is_update_anim = FALSE;
            sIFCommonPlayerDamageInterface[player].dead_stopupdate_wait = 180;
            sIFCommonPlayerDamageInterface[player].is_show_interface = FALSE;

            ifSetPlayer(interface_gobj, player); // Cast is probably redundant but I don't want any compilers screaming at me

            ifCommonPlayerDamageProcUpdate(interface_gobj);
            gcAddGObjProcess(interface_gobj, ifCommonPlayerDamageProcUpdate, nGCProcessKindFunc, 0);
        }
    }
}

/* ifcommon.c:982-996, verbatim: the digits fly apart when a stock is
 * lost (ft/ftcommon/ftcommondead.c) */
void ifCommonPlayerDamageStartBreakAnim(FTStruct *fp)
{
    s32 player = fp->player;
    s32 i;

    for (i = 0; i < sIFCommonPlayerDamageInterface[player].char_display_count; i++)
    {
        sIFCommonPlayerDamageInterface[player].chars[i].vel.x = (syUtilsRandFloat() * 2) + (-1.0F);
        sIFCommonPlayerDamageInterface[player].chars[i].vel.y = -10.0F;

        sIFCommonPlayerDamageInterface[player].chars[i].is_lock_movement = FALSE;
    }
    sIFCommonPlayerDamageInterface[player].break_anim_frame = 0;
    sIFCommonPlayerDamageInterface[player].is_update_anim = TRUE;
}

/* ifcommon.c:999-1005, verbatim: and come back at the respawn */
void ifCommonPlayerDamageStopBreakAnim(FTStruct *fp)
{
    s32 player = fp->player;

    sIFCommonPlayerDamageInterface[player].is_update_anim = FALSE;
    sIFCommonPlayerDamageInterface[player].scale = 1.04F;
}

/* ifcommon.c:1008-1119 ifCommonPlayerStockMultiProcDisplay 0x8010F878,
 * verbatim. The stock row, relaid whenever the count changes: up to six
 * icons ten pixels apart, each the fighter's stock sprite under the
 * costume's palette; past six, one icon, a cross and the count in
 * digits. */
void ifCommonPlayerStockMultiProcDisplay(GObj *interface_gobj)
{
    s32 player;
    FTStruct *fp;
    s32 stock_count;
    s32 digit_count;
    SObj *gt_sobj;
    SObj *lt_sobj;
    s32 stock_order, digit_order;
    s32 trunc_pos_x;
    u8 digits[3];

    player = ifGetPlayer(interface_gobj);
    stock_count = gSCManagerBattleState->players[player].stock_count;
    fp = ftGetStruct(gSCManagerBattleState->players[player].fighter_gobj);

    if (stock_count >= 0)
    {
        stock_count++;

        if (stock_count != sIFCommonPlayerStocksNum[player])
        {
            if (stock_count <= 6)
            {
                stock_order = 0;

                lt_sobj = SObjGetStruct(interface_gobj);

                while (lt_sobj != NULL)
                {
                    if (stock_order < stock_count)
                    {
                        lt_sobj->sprite = *fp->attr->sprites->stock_sprite;

                        lt_sobj->sprite.LUT = fp->attr->sprites->stock_luts[fp->costume];

                        lt_sobj->pos.x = ((gIFCommonPlayerInterface.player_pos_x[player] + dIFCommonPlayerStocksIconOffsetsX[player] + (stock_order * 10)) - (lt_sobj->sprite.width * 0.5F));
                        lt_sobj->pos.y = ((gIFCommonPlayerInterface.player_pos_y - (s32)(lt_sobj->sprite.height * 0.5F)) - 20);

                        lt_sobj->sprite.attr &= ~SP_HIDDEN;
                    }
                    else lt_sobj->sprite.attr |= SP_HIDDEN;

                    lt_sobj = lt_sobj->next;

                    stock_order++;
                }
            }
            else
            {
                digit_count = ifCommonPlayerDamageGetSpecialArrayID(stock_count, digits);

                trunc_pos_x = gIFCommonPlayerInterface.player_pos_x[player] + dIFCommonPlayerStocksDigitOffsetsX[player];

                gt_sobj = SObjGetStruct(interface_gobj);

                gt_sobj->sprite = *fp->attr->sprites->stock_sprite;

                gt_sobj->sprite.LUT = fp->attr->sprites->stock_luts[fp->costume];

                gt_sobj->pos.x = ((trunc_pos_x - 22) - (gt_sobj->sprite.width * 0.5F));
                gt_sobj->pos.y = ((gIFCommonPlayerInterface.player_pos_y - (s32)(gt_sobj->sprite.height * 0.5F)) - 20);

                gt_sobj->sprite.attr &= ~SP_HIDDEN;

                gt_sobj = gt_sobj->next;

                gt_sobj->sprite = *lbRelocGetFileData(Sprite*, gGMCommonFiles[4], dIFCommonPlayerStockDigitSpriteOffsets[10]);

                gt_sobj->pos.x = ((trunc_pos_x + -10.5F) - (gt_sobj->sprite.width * 0.5F));
                gt_sobj->pos.y = ((gIFCommonPlayerInterface.player_pos_y - 20) - (gt_sobj->sprite.height * 0.5F));

                gt_sobj->sprite.attr &= ~SP_HIDDEN;

                gt_sobj = gt_sobj->next;

                digit_order = 0;

                while (gt_sobj != NULL)
                {
                    if (digit_order < digit_count)
                    {
                        gt_sobj->sprite = *lbRelocGetFileData(Sprite*, gGMCommonFiles[4], dIFCommonPlayerStockDigitSpriteOffsets[digits[digit_order]]);

                        gt_sobj->pos.x = ((trunc_pos_x + (digit_order * 8)) - (gt_sobj->sprite.width * 0.5F));
                        gt_sobj->pos.y = ((gIFCommonPlayerInterface.player_pos_y - 20) - (gt_sobj->sprite.height * 0.5F));

                        gt_sobj->sprite.attr &= ~SP_HIDDEN;
                    }
                    else gt_sobj->sprite.attr |= SP_HIDDEN;

                    gt_sobj = gt_sobj->next;

                    digit_order++;
                }
            }
            sIFCommonPlayerStocksNum[player] = stock_count;
        }
        lbCommonDrawSObjAttr(interface_gobj);
    }
}

/* ifcommon.c:1122-1130, verbatim */
void ifCommonPlayerStockSetIconAttr(void)
{
    s32 i;

    for (i = 0; i < ARRAY_COUNT(dIFCommonPlayerStockDigitSpriteOffsets); i++)
    {
        lbRelocGetFileData(Sprite*, gGMCommonFiles[4], dIFCommonPlayerStockDigitSpriteOffsets[i])->attr = SP_TEXSHUF | SP_TRANSPARENT;
    }
}

/* ifcommon.c:1133-1157, verbatim: six SObjs, laid out by the display
 * proc above once the count is known */
void ifCommonPlayerStockMultiMakeInterface(s32 player)
{
    FTStruct *fp = ftGetStruct(gSCManagerBattleState->players[player].fighter_gobj);
    Sprite *sprite;

    if ((fp->attr->sprites != NULL) && (fp->attr->sprites->stock_sprite != NULL))
    {
        GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);
        gcAddGObjDisplay(interface_gobj, ifCommonPlayerStockMultiProcDisplay, 23, GOBJ_PRIORITY_DEFAULT, ~0);

        lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[4], dIFCommonPlayerStockDigitSpriteOffsets[0]));
        lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[4], dIFCommonPlayerStockDigitSpriteOffsets[0]));
        lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[4], dIFCommonPlayerStockDigitSpriteOffsets[0]));
        lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[4], dIFCommonPlayerStockDigitSpriteOffsets[0]));
        lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[4], dIFCommonPlayerStockDigitSpriteOffsets[0]));
        lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[4], dIFCommonPlayerStockDigitSpriteOffsets[0]));

        sIFCommonPlayerStocksNum[player] = S8_MAX;

        sprite = fp->attr->sprites->stock_sprite;
        sprite->attr = SP_TEXSHUF | SP_TRANSPARENT;

        ifSetPlayer(interface_gobj, player);
    }
}

/* ifcommon.c:1160-1169, verbatim */
void ifCommonPlayerStockSingleProcDisplay(GObj *interface_gobj)
{
    s32 player = ifGetPlayer(interface_gobj);
    s32 stocks = gSCManagerBattleState->players[player].stock_count;

    if (stocks != -1)
    {
        lbCommonDrawSObjAttr(interface_gobj);
    }
}

/* ifcommon.c:1172-1175, verbatim */
void ifCommonPlayerStockSetLUT(s32 player, s32 lut_id, FTAttributes *attr)
{
    SObjGetStruct(sIFCommonPlayerStocksGObj[player])->sprite.LUT = attr->sprites->stock_luts[lut_id];
}

/* ifcommon.c:1178-1200, verbatim: the single-icon row of the 1P game */
void ifCommonPlayerStockSingleMakeInterface(s32 player)
{
    FTStruct *fp = ftGetStruct(gSCManagerBattleState->players[player].fighter_gobj);
    GObj *interface_gobj;
    SObj *sobj;

    if ((fp->attr->sprites != NULL) && (fp->attr->sprites->stock_sprite != NULL))
    {
        sIFCommonPlayerStocksGObj[player] = interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

        gcAddGObjDisplay(interface_gobj, ifCommonPlayerStockSingleProcDisplay, 23, GOBJ_PRIORITY_DEFAULT, ~0);

        sobj = lbCommonMakeSObjForGObj(interface_gobj, fp->attr->sprites->stock_sprite);

        sobj->sprite.attr = SP_TEXSHUF | SP_TRANSPARENT;
        sobj->sprite.LUT = fp->attr->sprites->stock_luts[fp->costume];

        sobj->pos.x = ((gIFCommonPlayerInterface.player_pos_x[player] + dIFCommonPlayerStocksIconOffsetsX[player]) - (s32)(sobj->sprite.width * 0.5F));
        sobj->pos.y = ((gIFCommonPlayerInterface.player_pos_y - (s32)(sobj->sprite.height * 0.5F)) - 20);

        ifSetPlayer(interface_gobj, player);
    }
}

/* ifcommon.c:1203-1239 ifCommonPlayerStockStealProcUpdate 0x80110138,
 * verbatim: the stolen icon's thirty-tic arc from one row to the other,
 * and the sparkle where it lands */
void ifCommonPlayerStockStealProcUpdate(GObj *interface_gobj)
{
    f32 dist_x;
    f32 vel_x;
    f32 vel_y;
    SObj *sobj;
    IFPlayerSteal *s_steal = &sIFCommonPlayerStealInterface[ifGetPlayer(interface_gobj)];

    s_steal->anim_frames--;

    if (s_steal->anim_frames == 0)
    {
        efManagerStockStealEndMakeEffect
        (
            gIFCommonPlayerInterface.player_pos_x[ifGetPlayer(interface_gobj)] + 
            dIFCommonPlayerStocksIconOffsetsX[ifGetPlayer(interface_gobj)], 
            gIFCommonPlayerInterface.player_pos_y - 20
        );
        gcEjectGObj(interface_gobj);

        return;
    }
    sobj = SObjGetStruct(interface_gobj);

    dist_x = (s_steal->steal_pos_x - s_steal->target_pos_x);

    vel_x = (s_steal->anim_frames * dist_x) / 30.0F;

    if (vel_x < (dist_x * 0.5F))
    {
        vel_y = -(vel_x - (dist_x * 0.5F));
    }
    else vel_y = (vel_x - (dist_x * 0.5F));

    sobj->pos.x = (s_steal->target_pos_x + vel_x);
    sobj->pos.y = ((s_steal->steal_pos_y + ((15.0F / SQUARE(dist_x * 0.5F)) * vel_y * vel_y)) - 15.0F);
}

/* ifcommon.c:1242-1284 ifCommonPlayerStockStealMakeInterface 0x801102B0,
 * verbatim */
void ifCommonPlayerStockStealMakeInterface(s32 thief, s32 stolen)
{
    FTStruct *fp = ftGetStruct(gSCManagerBattleState->players[stolen].fighter_gobj);
    GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

    if (interface_gobj != NULL)
    {
        SObj *check_sobj, *sobj;

        gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);
        gcAddGObjProcess(interface_gobj, ifCommonPlayerStockStealProcUpdate, nGCProcessKindFunc, 0);

        check_sobj = lbCommonMakeSObjForGObj(interface_gobj, fp->attr->sprites->stock_sprite);

        if (check_sobj == NULL)
        {
            gcEjectGObj(interface_gobj);

            return;
        }
        else
        {
            sobj = check_sobj;

            sobj->sprite.attr = SP_TEXSHUF | SP_TRANSPARENT;
            sobj->sprite.LUT = fp->attr->sprites->stock_luts[fp->costume];

            sIFCommonPlayerStealInterface[thief].steal_pos_x = ((gIFCommonPlayerInterface.player_pos_x[stolen] + dIFCommonPlayerStocksIconOffsetsX[stolen]) - (s32)(sobj->sprite.width * 0.5F));
            sIFCommonPlayerStealInterface[thief].steal_pos_y = ((gIFCommonPlayerInterface.player_pos_y - (s32)(sobj->sprite.height * 0.5F)) - 20);

            sIFCommonPlayerStealInterface[thief].target_pos_x = ((gIFCommonPlayerInterface.player_pos_x[thief] + dIFCommonPlayerStocksIconOffsetsX[thief]) - (s32)(sobj->sprite.width * 0.5F));

            sobj->pos.x = sIFCommonPlayerStealInterface[thief].steal_pos_x;
            sobj->pos.y = sIFCommonPlayerStealInterface[thief].steal_pos_y;

            sIFCommonPlayerStealInterface[thief].anim_frames = 30;

            ifSetPlayer(interface_gobj, thief);

            efManagerStockStealStartMakeEffect(gIFCommonPlayerInterface.player_pos_x[stolen] + dIFCommonPlayerStocksIconOffsetsX[stolen], gIFCommonPlayerInterface.player_pos_y - 20);
        }
    }
}

/* ifcommon.c:1287-1309, verbatim */
void ifCommonPlayerStockInitInterface(void)
{
    s32 player;

    ifCommonPlayerStockSetIconAttr();

    for (player = 0; player < ARRAY_COUNT(gSCManagerBattleState->players); player++)
    {
        if (gSCManagerBattleState->players[player].pkind != nFTPlayerKindNot)
        {
            switch (gSCManagerBattleState->players[player].is_single_stockicon)
            {
            case FALSE:
                ifCommonPlayerStockMultiMakeInterface(player);
                break;

            case TRUE:
                ifCommonPlayerStockSingleMakeInterface(player);
                break;
            }
        }
    }
}

/* ---- ifcommon.c:1311-1630, the magnifying glasses ------
 *
 * When a fighter leaves the battle viewport the game draws it a second
 * time, small, inside a round glass at the edge of the screen nearest
 * to it, with a handle pointing the way. Three things make the glass on
 * the N64, and each is drawn from the fighter's own display proc while
 * the magnify camera (src/dc/gmcamera.c) runs it:
 *
 *   1. the frame: relocData file 166's 16x16 IA8 image, mirrored into a
 *      32x32 disc and drawn twice as a texture rectangle
 *      (ifCommonPlayerMagnifyUpdateRender). The first time the *Z-buffer*
 *      is the colour image and the texel's intensity becomes a depth --
 *      F inside the disc, 0 outside it -- so that everything drawn
 *      afterwards with the Z test on is accepted inside the circle and
 *      rejected outside it. The second time it is colour, under
 *      G_CC_BLENDPEDECALA: ENV * (1 - I) + PRIM * I over the texel's
 *      alpha, with ENV the player's colour and PRIM the stage's fog
 *      colour. What that draws is a fog-coloured disc with a
 *      player-coloured rim.
 *   2. the fighter, through an 18*scale pixel square viewport centred
 *      on the glass (ifCommonPlayerMagnifyUpdateViewport), under the
 *      camera's own ortho projection and with its TopN translation
 *      dropped, so it stands at the origin of that view; the Z test
 *      against the mask rounds it off.
 *   3. the handle, a single flat triangle at the glass, turned toward
 *      the fighter and scaled with the glass, in the player's colour
 *      (ifCommonPlayerMagnifyProcDisplay).
 *
 * The functions keep their names, signatures and order, and all of the
 * arithmetic -- where the glass goes, how big it is, where the handle
 * points -- is verbatim. What the RDP did with the results is not, and
 * that is DIVERGES:
 *   - the Z-buffer mask is the sixteen clip planes of src/dc/clip.c,
 *     applied to the fighter's triangles as they are drawn
 *     (src/dc/objmodel.c dc_model_proc_display's magnify path). The PVR
 *     has no way to write a depth without a colour, no stencil, and a
 *     user clip aligned to 32-pixel tiles; a regular 16-gon inscribed
 *     in the mask's own soft edge is the honest stand-in, and its edges
 *     lie under the rim.
 *   - the frame's one rectangle is two quads (ifCommonPlayerMagnifyDrawFrame):
 *     the PVR's combiner cannot lerp two vertex colours by a texel, so
 *     tools/export/ssb_magnifyexport.py ships the image as (1 - I, A) and
 *     (I, A), and the first is modulated by ENV and blended, the second
 *     modulated by PRIM and added -- the same sum, term by term.
 *   - the frame, the fighter and the handle each take the frame's next
 *     sprite depth (lbCommonSpriteNextDepth), which is how the port
 *     keeps Z-less draws in the order the RDP drew them: the fighter
 *     over the disc, the handle over the fighter, the HUD over all.
 *   - the scissor lines are kept for their arithmetic and set no
 *     scissor: the PVR is not scissored (src/dc/taskman.c paints the
 *     border instead), the glass is inset 20*scale from every edge by
 *     ifCommonPlayerMagnifyGetPosition, and the mask does the rest.
 *   - the handle is a clone per player of one pack rather than one
 *     display list bound to four DObjs, as the arrows are, and its prim
 *     colour is set on the clone (fighter_set_prim_color) where the
 *     game sets it on the RDP.
 *   - gcDrawDObjDLHead0 becomes dc_model_draw_tree_layered, for the
 *     reason dc_model_draw_tree exists (src/dc/objmodel.c).
 * ------------------------------------------------------------------ */

/* The handle's pack and its four clones. The game has no counterpart:
 * the file is loaded by lbRelocGetFileData and the display list bound
 * straight to a DObj. */
#define IFCOMMON_MODEL_MAGNIFY "ifmagnify.mdl"

static Fighter sIFCommonPlayerMagnifyPack;
static Fighter sIFCommonPlayerMagnifyModels[GMCOMMON_PLAYERS_MAX];
static sb32 sIFCommonPlayerMagnifyIsLoaded;

/* ifcommon.c:1312-1394 0x801105CC, verbatim: where on the edge of the
 * viewport the glass goes for a fighter at (player_pos_x, player_pos_y)
 * -- on the ray from the centre toward it, at the first edge of a
 * rectangle inset by the glass's own size. */
void ifCommonPlayerMagnifyGetPosition(f32 player_pos_x, f32 player_pos_y, Vec2f *magnify_pos)
{
    f32 left;
    f32 right;
    f32 up;
    f32 down;
    f32 bak_right;
    f32 bak_up;
    f32 diag_hz;
    f32 diag_vt;
    f32 div_xy;

    left = (-gGMCameraStruct.viewport_width / 2) + (20 * gIFCommonPlayerInterface.magnify_scale) + 5;
    bak_right = right = (+gGMCameraStruct.viewport_width / 2) - (20 * gIFCommonPlayerInterface.magnify_scale) - 5;
    bak_up = up = (+gGMCameraStruct.viewport_height / 2) - (20 * gIFCommonPlayerInterface.magnify_scale);
    down = (-gGMCameraStruct.viewport_height / 2) + (20 * gIFCommonPlayerInterface.magnify_scale);

    if (player_pos_x == 0.0F)
    {
        up = bak_up;

        if (player_pos_y > 0.0F)
        {
            magnify_pos->y = up;
        }
        else magnify_pos->y = down;

        magnify_pos->x = 0.0F;
    }
    else
    {
        div_xy = player_pos_y / player_pos_x;

        if (((up / right) < div_xy) || (-(up / right) > div_xy))
        {
            up = bak_up;

            if (player_pos_y > 0.0F)
            {
                magnify_pos->y = up;
            }
            else magnify_pos->y = down;

            diag_hz = (magnify_pos->y * player_pos_x) / player_pos_y;

            right = bak_right;

            if (diag_hz < left)
            {
                magnify_pos->x = left;
            }
            else if (diag_hz > right)
            {
                magnify_pos->x = right;
            }
            else magnify_pos->x = diag_hz;
        }
        else
        {
            right = bak_right;

            if (player_pos_x > 0.0F)
            {
                magnify_pos->x = right;
            }
            else magnify_pos->x = left;

            diag_vt = (magnify_pos->x * player_pos_y) / player_pos_x;

            up = bak_up;

            if (diag_vt < down)
            {
                magnify_pos->y = down;
            }
            else if (diag_vt > up)
            {
                magnify_pos->y = up;
            }
            else magnify_pos->y = diag_vt;
        }
    }
}

#ifndef FT_HOSTTEST
/* The frame's two quads -- see the DIVERGES at the head of the section.
 * Game pixels in, doubled onto the framebuffer as every sprite is
 * (lbcommon.h LB_SPRITE_SCREEN_SCALE); s and t in the RDP's 10.5 texel
 * units and dsdx in its 5.10, over a 16-texel tile the PVR mirrors on
 * its second repeat (PVR_UVFLIP_UV) as G_TX_MIRROR with mask 4 did. */
static void ifCommonPlayerMagnifyDrawFrame(s32 ulx, s32 uly, s32 lrx, s32 lry, s32 s, s32 t, f32 dsdx, u32 env_rgb, u32 prim_rgb)
{
    const Fighter *pack = &sIFCommonPlayerMagnifyPack;
    f32 u0 = (f32)s / 32.0F / 16.0F;
    f32 v0 = (f32)t / 32.0F / 16.0F;
    f32 u1 = u0 + ((f32)(lrx - ulx) * dsdx) / 1024.0F / 16.0F;
    f32 v1 = v0 + ((f32)(lry - uly) * dsdx) / 1024.0F / 16.0F;
    s32 pass;

    if (sIFCommonPlayerMagnifyIsLoaded == FALSE || pack->hd->tex_count < 2)
    {
        return;
    }
    for (pass = 0; pass < 2; pass++)
    {
        pvr_poly_cxt_t cxt;
        pvr_poly_hdr_t hdr;
        pvr_vertex_t v;
        s32 i;

        pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444, 16, 16,
                         pack->txr[pass], PVR_FILTER_BILINEAR);
        cxt.gen.culling = PVR_CULLING_NONE;
        cxt.txr.env = PVR_TXRENV_MODULATEALPHA;
        cxt.txr.uv_flip = PVR_UVFLIP_UV;
        cxt.txr.uv_clamp = PVR_UVCLAMP_NONE;
        cxt.blend.src = PVR_BLEND_SRCALPHA;
        cxt.blend.dst = pass ? PVR_BLEND_ONE : PVR_BLEND_INVSRCALPHA;
        cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
        cxt.depth.write = PVR_DEPTHWRITE_DISABLE;
        DBPERF_COMPILE();
        pvr_poly_compile(&hdr, &cxt);
        pvr_prim(&hdr, sizeof(hdr));

        v.argb = 0xFF000000 | (pass ? prim_rgb : env_rgb);
        v.oargb = 0;
        v.z = lbCommonSpriteNextDepth();
        for (i = 0; i < 4; i++)
        {
            v.flags = (i == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v.x = (f32)((i & 2) ? lrx : ulx) * LB_SPRITE_SCREEN_SCALE;
            v.y = (f32)((i & 1) ? uly : lry) * LB_SPRITE_SCREEN_SCALE;
            v.u = (i & 2) ? u1 : u0;
            v.v = (i & 1) ? v0 : v1;
            pvr_prim(&v, sizeof(v));
        }
    }
}
#endif

/* ifcommon.c:1397-1493 0x801107F0. The arithmetic is the decomp's --
 * the rectangle, its clamp to the viewport and the texel the clamped
 * edge starts at -- and the two texture rectangles are what DIVERGES
 * above says. */
void ifCommonPlayerMagnifyUpdateRender(Gfx **dls, s32 color_id, f32 ulx, f32 uly)
{
    SYColorRGB *color;
    f32 temp_f0;
    s32 var_uly;
    s32 var_lrx;
    s32 var_ulx;
    s32 var_lry;
    s32 temp_t0;
    s32 temp_t1;

    (void)dls;

    temp_f0 = (s32)((1024.0F / gIFCommonPlayerInterface.magnify_scale) + 0.5F);

    var_ulx = ulx;
    var_uly = uly;

    var_lrx = ((ulx == var_ulx) ? 0 : 1) + (s32)(ulx + (32.0F * gIFCommonPlayerInterface.magnify_scale));

    var_lry = ((uly == var_uly) ? 0 : 1) + (s32)(uly + (32.0F * gIFCommonPlayerInterface.magnify_scale));

    if (var_ulx < gGMCameraStruct.viewport_ulx)
    {
        var_ulx = gGMCameraStruct.viewport_ulx;
    }
    else if (var_lrx >= gGMCameraStruct.viewport_lrx)
    {
        var_lrx = gGMCameraStruct.viewport_lrx - 1;
    }
    if (var_uly < gGMCameraStruct.viewport_uly)
    {
        var_uly = gGMCameraStruct.viewport_uly;
    }
    else if (var_lry >= gGMCameraStruct.viewport_lry)
    {
        var_lry = gGMCameraStruct.viewport_lry - 1;
    }
    temp_t0 = ((s32)((var_ulx - ulx) * temp_f0) + 16) >> 5;
    temp_t1 = ((s32)((var_uly - uly) * temp_f0) + 16) >> 5;

    color = &gMPCollisionGroundData->fog_color;

#ifndef FT_HOSTTEST
    if (gcGetDrawList() == PVR_LIST_TR_POLY)
    {
        ifCommonPlayerMagnifyDrawFrame
        (
            var_ulx, var_uly, var_lrx, var_lry, temp_t0, temp_t1, temp_f0,
            ((u32)dIFCommonPlayerMagnifyColorsR[color_id] << 16) | ((u32)dIFCommonPlayerMagnifyColorsG[color_id] << 8) | dIFCommonPlayerMagnifyColorsB[color_id],
            ((u32)color->r << 16) | ((u32)color->g << 8) | color->b
        );
    }
#else
    (void)color;
    (void)color_id;
    (void)temp_t0;
    (void)temp_t1;
#endif
}

/* ifcommon.c:1495-1572 0x80110DD4. Runs from the fighter's display proc
 * under the magnify camera (src/dc/ftdisplaymain.c): place the glass,
 * draw its frame, and set the viewport the fighter is about to be drawn
 * through. The first fighter of the frame finds magnify_mode at 1 and
 * moves it to 2; every later one first puts the battle viewport back,
 * because the one before it left its own. */
void ifCommonPlayerMagnifyUpdateViewport(Gfx **dls, FTStruct *fp)
{
    f32 magnify_x;
    f32 magnify_y;
    IFPlayerMagnify *ifmag;
    CObj *cobj;
    f32 scale;
    s32 ulx;
    s32 uly;
    s32 lrx;
    s32 lry;

    if (gIFCommonPlayerInterface.is_magnify_display != FALSE)
    {
        ifmag = &sIFCommonPlayerMagnifyInterface[fp->player];

        magnify_x = fp->magnify_pos.x;
        magnify_y = fp->magnify_pos.y;

        ifCommonPlayerMagnifyGetPosition(magnify_x, magnify_y, &ifmag->pos);

        magnify_x = ifmag->pos.x + gGMCameraStruct.viewport_center_x;
        magnify_y = gGMCameraStruct.viewport_center_y - ifmag->pos.y;

        /* gSPMatrix(&CObjGetStruct(gGCCurrentCamera)->xobjs[0]->mtx, G_MTX_PROJECTION) */
        gmCameraLoadXObjMatrix(CObjGetStruct(gGCCurrentCamera)->xobjs[0]);

        if (gIFCommonPlayerInterface.magnify_mode != 1)
        {
            cobj = CObjGetStruct(gGMCameraGObj);

            /* gSPViewport(&cobj->viewport), gDPSetScissor(the battle viewport) */
            gcPrepCameraViewport(cobj, 0);
        }
        else gIFCommonPlayerInterface.magnify_mode = 2;

        scale = (16.0F * gIFCommonPlayerInterface.magnify_scale);

        ifCommonPlayerMagnifyUpdateRender(dls, ifmag->color_id, magnify_x - scale, magnify_y - scale);

        scale = (18.0F * gIFCommonPlayerInterface.magnify_scale);

        magnify_x -= (scale / 2);
        magnify_y -= (scale / 2);

        syRdpSetViewport(&ifmag->viewport, magnify_x, magnify_y, scale + magnify_x, scale + magnify_y);

        /* gSPViewport(&ifmag->viewport) */
        gcPrepViewport(&ifmag->viewport.vp);

        ulx = (ifmag->viewport.vp.vtrans[0] / 4) - (ifmag->viewport.vp.vscale[0] / 4);
        uly = (ifmag->viewport.vp.vtrans[1] / 4) - (ifmag->viewport.vp.vscale[1] / 4);
        lrx = (ifmag->viewport.vp.vtrans[0] / 4) + (ifmag->viewport.vp.vscale[0] / 4);
        lry = (ifmag->viewport.vp.vtrans[1] / 4) + (ifmag->viewport.vp.vscale[1] / 4);

        if (ulx < gGMCameraStruct.viewport_ulx)
        {
            ulx = gGMCameraStruct.viewport_ulx;
        }
        if (lrx > gGMCameraStruct.viewport_lrx)
        {
            lrx = gGMCameraStruct.viewport_lrx;
        }
        if (uly < gGMCameraStruct.viewport_uly)
        {
            uly = gGMCameraStruct.viewport_uly;
        }
        else if (lry > gGMCameraStruct.viewport_lry)
        {
            lry = gGMCameraStruct.viewport_lry;
        }
        /* gDPSetScissor(G_SC_NON_INTERLACE, ulx, uly, lrx, lry) -- see DIVERGES */
        (void)ulx;
        (void)uly;
        (void)lrx;
        (void)lry;
    }
}

/* ifcommon.c:1575-1609 0x801111A0. The handle: at the glass, turned
 * toward the fighter, half the glass's scale, in the player's colour,
 * under the camera's screen-unit ortho (its second XObj) and the battle
 * viewport, Z off, opaque. */
void ifCommonPlayerMagnifyProcDisplay(FTStruct *fp)
{
    GObj *interface_gobj;
    DObj *dobj;
    IFPlayerMagnify *ifmag;
    CObj *cobj;

    if (gIFCommonPlayerInterface.is_magnify_display != FALSE)
    {
        ifmag = &sIFCommonPlayerMagnifyInterface[fp->player];

        interface_gobj = ifmag->interface_gobj;

        dobj = DObjGetStruct(interface_gobj);

        dobj->translate.vec.f.x = ifmag->pos.x;
        dobj->translate.vec.f.y = ifmag->pos.y;

        dobj->rotate.vec.f.z = syUtilsArcTan2(fp->magnify_pos.y, fp->magnify_pos.x) - F_CST_DTOR32(90.0F);

        dobj->scale.vec.f.x = dobj->scale.vec.f.y = gIFCommonPlayerInterface.magnify_scale * 0.5F;

        cobj = CObjGetStruct(gGMCameraGObj);

        /* gSPViewport(&cobj->viewport), gDPSetScissor(the battle viewport) */
        gcPrepCameraViewport(cobj, 0);
        /* gSPMatrix(&CObjGetStruct(gGCCurrentCamera)->xobjs[1]->mtx, G_MTX_PROJECTION) */
        gmCameraLoadXObjMatrix(CObjGetStruct(gGCCurrentCamera)->xobjs[1]);
        /* gSPClearGeometryMode(G_ZBUFFER), gDPSetRenderMode(G_RM_AA_OPA_SURF),
         * gDPSetAlphaCompare(G_AC_NONE): baked
         * (tools/export/ssb_magnifyexport.py) */
        /* gDPSetPrimColor(the player's colour) */
        fighter_set_prim_color
        (
            &sIFCommonPlayerMagnifyModels[fp->player],
            0xFF000000 | ((u32)dIFCommonPlayerMagnifyColorsR[ifmag->color_id] << 16) | ((u32)dIFCommonPlayerMagnifyColorsG[ifmag->color_id] << 8) | dIFCommonPlayerMagnifyColorsB[ifmag->color_id]
        );
        dc_model_draw_tree_layered(interface_gobj);
    }
}

/* ifcommon.c:1612-1629 0x80111440, verbatim but for the pack load and
 * the clone per fighter (DIVERGES above). scvsbattle.c:208 calls it. */
void ifCommonPlayerMagnifyMakeInterface(void)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

#ifndef FT_HOSTTEST
    {
        int pal_bank = 0;

        if (sIFCommonPlayerMagnifyIsLoaded == FALSE &&
            fighter_load(&sIFCommonPlayerMagnifyPack,
                         IFCOMMON_MODEL_MAGNIFY, &pal_bank) != 0)
        {
            return;
        }
        sIFCommonPlayerMagnifyIsLoaded = TRUE;
    }
#endif

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);
        GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDMagnify, GOBJ_PRIORITY_DEFAULT);

        /* gcAddXObjForDObjFixed(gcAddDObjForGObj(interface_gobj, the
         * handle's display list), nGCMatrixKindTraRotRpyRSca, 0): the
         * pack's one joint under the three matrix kinds that compose
         * the same */
#ifndef FT_HOSTTEST
        fighter_clone(&sIFCommonPlayerMagnifyModels[fp->player],
                      &sIFCommonPlayerMagnifyPack,
                      syTaskmanMalloc(sizeof(float[4]) *
                                      sIFCommonPlayerMagnifyPack.hd->vert_count, 0x8));
        dc_model_add_dobjs(interface_gobj, NULL,
                           &sIFCommonPlayerMagnifyModels[fp->player], NULL);
#else
        /* as the arrows': no pack in the host link, and the placement
         * still needs one DObj to land on */
        gcAddDObjRpyR(interface_gobj, NULL);
#endif

        sIFCommonPlayerMagnifyInterface[fp->player].interface_gobj = interface_gobj;
        sIFCommonPlayerMagnifyInterface[fp->player].color_id = gSCManagerBattleState->players[fp->player].color;

        fighter_gobj = fighter_gobj->link_next;
    }
    gIFCommonPlayerInterface.is_magnify_display = FALSE;
}

/* ---- ifcommon.c:1632-1816, the off-screen arrows -------
 *
 * When a fighter leaves the battle viewport the game does two things
 * about it: it puts a magnifying glass at the edge of the screen showing
 * the fighter inside it, and -- if the fighter went out sideways -- it
 * flashes a red chevron arrow at that edge. This section is the arrows.
 *
 * They are three GObjs on DL link 8, drawn by an ortho camera of their
 * own (gmCameraMakePlayerArrowsCamera, src/dc/gmcamera.c), and the model
 * is relocData file 166's: a root and three chevrons at x = 5, 16 and 27,
 * one triangle each, with an AnimJoint animation whose only opcodes set
 * and clear DOBJ_FLAG_HIDDEN -- which is how the chevrons chase each
 * other outward. tools/export/ssb_arrowexport.py bakes the tree, the triangles
 * and that animation into one pack.
 *
 * The three GObjs are: one that queues dIFCommonPlayerArrowsDisplayList
 * (render state, and the only thing in the game that sets the arrows'
 * colour) and carries ifCommonPlayerArrowsFuncRun as its process, and
 * one each for the left and right arrow, at x = -134 and +134 with the
 * right one turned 180 degrees.
 *
 * DIVERGES:
 *   - dIFCommonPlayerArrowsDisplayList is not queued at runtime. It is
 *     eight lines of pure RDP state -- render mode, alpha compare,
 *     primitive colour, combiner, and the Z-buffer out of the geometry
 *     mode -- and every one of them is a property of the batches it
 *     precedes, so the exporter reads them and bakes them in
 *     (tools/export/ssb_arrowexport.py). ifCommonPlayerArrowsMainProcDisplay
 *     therefore has nothing to draw, and the GObj stays for its process,
 *     which is the run function.
 *   - gcDrawDObjTreeForGObj becomes dc_model_draw_tree (src/dc/
 *     objmodel.c): here the tree walk only composes matrices and the
 *     model is submitted after it, so the one line has to do both. The
 *     walk's own DOBJ_FLAG_HIDDEN test is what the animation drives, and
 *     dc_model_draw_tree carries it through to the batches.
 *   - the two arrows are two clones of one pack rather than two
 *     instances of one relocData file, which is the same thing said in
 *     the port's terms (src/dc/objmodel.c).
 * ------------------------------------------------------------------ */

/* The arrows' pack and the two models drawn from it. The game has no
 * counterpart: the file is loaded into the scene heap by
 * lbRelocGetExternHeapFile and the two GObjs read the one DObjDesc in
 * it. */
#define IFCOMMON_MODEL_ARROWS "ifarrows.mdl"

static Fighter sIFCommonPlayerArrowsPack;
static Fighter sIFCommonPlayerArrowsModels[2];
static sb32 sIFCommonPlayerArrowsIsLoaded;

/* The AObjEvent32** the game reads out of the file at
 * llIFCommonPlayerArrowsAnimJoint: one script pointer per DObj in the
 * tree's walk order, NULL where a joint has none. The pack carries the
 * same table as a word index per joint (FPackAnim.off_entries, -1 for
 * NULL) because a baked pointer would not survive the trip; this turns
 * it back into pointers once, at load. */
static AObjEvent32 *sIFCommonPlayerArrowsAnimJoint[FIGHTER_MAX_JOINTS];

static void ifCommonPlayerArrowsSetAnimJoint(void)
{
#ifndef FT_HOSTTEST
    const Fighter *f = &sIFCommonPlayerArrowsPack;
    const FPackAnim *anim;
    const s32 *entries;
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sIFCommonPlayerArrowsAnimJoint); i++)
    {
        sIFCommonPlayerArrowsAnimJoint[i] = NULL;
    }
    if (f->hd->anim_count == 0)
    {
        return;
    }
    anim = &f->anims[0];
    entries = (const s32 *)((const u8 *)f->blob + anim->off_entries);

    for (i = 0; i < f->hd->joint_count &&
                i < ARRAY_COUNT(sIFCommonPlayerArrowsAnimJoint); i++)
    {
        if (entries[i] >= 0 && (u32) entries[i] < anim->nwords)
        {
            sIFCommonPlayerArrowsAnimJoint[i] =
                (AObjEvent32 *)((u8 *)f->blob + anim->off_words) + entries[i];
        }
    }
#endif
}

/* ifcommon.c:1632-1637 0x80111554, and 1640-1646 0x80111588. Each arrow
 * draws only when its own bit of arrows_flags is up -- the bit the
 * fighters' display procs raised this frame. */
void ifCommonPlayerArrowsLeftProcDisplay(GObj *interface_gobj)
{
    if (gIFCommonPlayerInterface.arrows_flags & IFCOMMON_PLAYERARROWS_MASK_LEFT)
    {
        dc_model_draw_tree(interface_gobj);
    }
}

void ifCommonPlayerArrowsRightProcDisplay(GObj *interface_gobj)
{
    if (gIFCommonPlayerInterface.arrows_flags & IFCOMMON_PLAYERARROWS_MASK_RIGHT)
    {
        dc_model_draw_tree(interface_gobj);
    }
}

/* ifcommon.c:1649-1654 0x801115BC, verbatim. The table it hands
 * gcAddAnimJointAll is the file's own on the N64; here it is rebuilt
 * from the pack at load (ifCommonPlayerArrowsSetAnimJoint below), which
 * is the same four pointers. */
void ifCommonPlayerArrowsAddAnim(GObj *interface_gobj)
{
    gcAddAnimJointAll(interface_gobj, sIFCommonPlayerArrowsAnimJoint, 0.0F);
    gcPlayAnimAll(interface_gobj);
}

/* ifcommon.c:1657-1670 0x801115FC, and 1673-1686 0x80111640, verbatim.
 * Status 0 is "not shown": nothing runs. 1 is the frame the arrow came
 * up, which installs the animation and falls through to play it; 2 is
 * every frame after. */
void ifCommonPlayerArrowsLeftProcUpdate(GObj *interface_gobj)
{
    switch (gIFCommonPlayerInterface.arrows_left_status)
    {
    case 0:
        break;

    case 1:
        ifCommonPlayerArrowsAddAnim(interface_gobj);
        /* Fallthrough */
    default:
        gcPlayAnimAll(interface_gobj);
        break;
    }
}

void ifCommonPlayerArrowsRightProcUpdate(GObj *interface_gobj)
{
    switch (gIFCommonPlayerInterface.arrows_right_status)
    {
    case 0:
        break;

    case 1:
        ifCommonPlayerArrowsAddAnim(interface_gobj);
        /* Fallthrough */
    default:
        gcPlayAnimAll(interface_gobj);
        break;
    }
}

/* ifcommon.c:1689-1708 0x80111684. DIVERGES: gcSetupCustomDObjs over the
 * file's DObjDesc becomes dc_model_add_dobjs over the pack, which builds
 * one DObj per pack joint in that same tree's order and shape. The three
 * matrix kinds the game names are gcAddDObjMatrixSetsRpyR's, which is
 * what dc_model_add_dobjs installs. The signature is the decomp's; which
 * of the two clones to build is read off proc_display, which is what
 * tells the game's own two calls apart. */
GObj* ifCommonPlayerArrowsMakeInterface(void (*proc_display)(GObj*), void (*proc_update)(GObj*))
{
    GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);
    /* which of the two clones this is; the game has no such thing, and
     * its own two calls are told apart by exactly this argument */
    s32 id = (proc_display == ifCommonPlayerArrowsLeftProcDisplay) ? 0 : 1;

    gcAddGObjDisplay(interface_gobj, proc_display, 8, GOBJ_PRIORITY_DEFAULT, ~0);

#ifndef FT_HOSTTEST
    fighter_clone(&sIFCommonPlayerArrowsModels[id],
                  &sIFCommonPlayerArrowsPack,
                  syTaskmanMalloc(sizeof(float[4]) *
                                  sIFCommonPlayerArrowsPack.hd->vert_count, 0x8));
    dc_model_add_dobjs(interface_gobj, NULL,
                       &sIFCommonPlayerArrowsModels[id], NULL);
#else
    /* As the spotlight's: the host cross-test links the scene and not
     * the renderer, so there is no pack to build a tree from and
     * ifCommonPlayerArrowsInitInterface's placement still needs one
     * DObj to land on. */
    (void)id;
    gcAddDObjRpyR(interface_gobj, NULL);
#endif
    gcAddGObjProcess(interface_gobj, proc_update, nGCProcessKindFunc, 5);

    return interface_gobj;
}

/* ifcommon.c:1711-1768 0x8011171C, verbatim.
 *
 * Once a frame, off the interface link: which side of the screen has a
 * fighter off it. A fighter counts only if the display walk saw it leave
 * (is_magnify_show), it is not respawning, its player is not one of 1P
 * mode's magnify-exempt ones, and it went out sideways rather than up or
 * down -- |x| > |y| in the projected position the display proc left in
 * fp->magnify_pos.
 *
 * The three-state status per side is what makes the chevrons restart:
 * 0 while nothing is out, 1 on the frame one goes out (which installs
 * the animation), 2 from then on.
 *
 * DIVERGES: func_800269C0_275C0 is the port's fgm queue and plays
 * nSYAudioFGMMagnify at the same cadence -- one every thirty tics while
 * anything is off screen. */
void ifCommonPlayerArrowsFuncRun(GObj *interface_gobj)
{
    sb32 lr_right = FALSE;
    sb32 lr_left = FALSE;

    (void)interface_gobj;

    if (gIFCommonPlayerInterface.is_magnify_display != FALSE)
    {
        GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

        while (fighter_gobj != NULL)
        {
            FTStruct *fp = ftGetStruct(fighter_gobj);

            if (!(fp->is_magnify_ignore) && !(fp->is_rebirth) && (fp->is_magnify_show))
            {
                if (ABSF(fp->magnify_pos.x) > ABSF(fp->magnify_pos.y))
                {
                    if (fp->magnify_pos.x < 0.0F)
                    {
                        lr_left = TRUE;
                    }
                    else lr_right = TRUE;
                }
            }
            fighter_gobj = fighter_gobj->link_next;
        }
        if (lr_left == FALSE)
        {
            gIFCommonPlayerInterface.arrows_left_status = 0;
        }
        else if (gIFCommonPlayerInterface.arrows_left_status == 0)
        {
            gIFCommonPlayerInterface.arrows_left_status = 1;
        }
        else gIFCommonPlayerInterface.arrows_left_status = 2;

        if (lr_right == FALSE)
        {
            gIFCommonPlayerInterface.arrows_right_status = 0;
        }
        else if (gIFCommonPlayerInterface.arrows_right_status == 0)
        {
            gIFCommonPlayerInterface.arrows_right_status = 1;
        }
        else gIFCommonPlayerInterface.arrows_right_status = 2;
    }
    if ((lr_left != FALSE) || (lr_right != FALSE))
    {
        if (sIFCommonPlayerMagnifySoundWait == 0)
        {
            func_800269C0_275C0(nSYAudioFGMMagnify);

            sIFCommonPlayerMagnifySoundWait = 30;
        }
        sIFCommonPlayerMagnifySoundWait--;
    }
    else sIFCommonPlayerMagnifySoundWait = 0;
}

/* ifcommon.c:1771-1774 0x801118B4. DIVERGES: the display list is baked
 * into the pack -- see the section head. */
void ifCommonPlayerArrowsMainProcDisplay(GObj *interface_gobj)
{
    (void)interface_gobj;
}

/* ifcommon.c:1777-1805 0x801118E4, verbatim but for the pack load and
 * the id the port's make function takes. sc/sccommon/scvsbattle.c:206
 * calls it. */
void ifCommonPlayerArrowsInitInterface(void)
{
    DObj *dobj;

#ifndef FT_HOSTTEST
    {
        int pal_bank = 0;

        if (sIFCommonPlayerArrowsIsLoaded == FALSE &&
            fighter_load(&sIFCommonPlayerArrowsPack,
                         IFCOMMON_MODEL_ARROWS, &pal_bank) != 0)
        {
            return;
        }
        sIFCommonPlayerArrowsIsLoaded = TRUE;
    }
#endif
    ifCommonPlayerArrowsSetAnimJoint();

    gcAddGObjDisplay
    (
        gcMakeGObjSPAfter(nGCCommonKindInterface, ifCommonPlayerArrowsFuncRun, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT),
        ifCommonPlayerArrowsMainProcDisplay,
        8,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
    dobj = DObjGetStruct(ifCommonPlayerArrowsMakeInterface(ifCommonPlayerArrowsLeftProcDisplay, ifCommonPlayerArrowsLeftProcUpdate));

    dobj->translate.vec.f.x = -134.0F;
    dobj->translate.vec.f.y = 0.0F;

    gIFCommonPlayerInterface.arrows_left_status = 0;

    dobj = DObjGetStruct(ifCommonPlayerArrowsMakeInterface(ifCommonPlayerArrowsRightProcDisplay, ifCommonPlayerArrowsRightProcUpdate));

    dobj->translate.vec.f.x = 134.0F;
    dobj->translate.vec.f.y = 0.0F;
    dobj->rotate.vec.f.z = F_CST_DTOR32(180.0F);

    gIFCommonPlayerInterface.arrows_right_status = 0;

    sIFCommonPlayerMagnifySoundWait = 0;
}

/* ifcommon.c:1808-1818 0x801119AC, verbatim: which edge a fighter that
 * left the viewport went out of. Called from the fighter's own display
 * proc (src/dc/ftdisplaymain.c) with the projected position, and only
 * for a fighter the arrows are allowed to point at. */
void ifCommonPlayerArrowsUpdateFlags(f32 x, f32 y)
{
    if (ABSF(x) > ABSF(y))
    {
        if (x > 0.0F)
        {
            gIFCommonPlayerInterface.arrows_flags |= IFCOMMON_PLAYERARROWS_MASK_RIGHT;
        }
        else gIFCommonPlayerInterface.arrows_flags |= IFCOMMON_PLAYERARROWS_MASK_LEFT;
    }
}

/* ---- ifcommon.c:1909-1952, the ITEM arrow -------------------------
 *
 * The red pointer a dropped item shows while it can be picked up. The
 * decomp reaches it through two globals -- `sIFCommonItemArrowSprite`,
 * a Sprite* the file's own relocation puts at offset 0x50 of
 * IFCommonItem, and the file itself, which ifCommonItemArrowSetAttr
 * loads into the heap on the first itManagerInitItems. IFCommonItem is
 * relocData file 87 and holds one sprite and nothing else.
 *
 * The port has no N64 heap file to point at, so the file is a bank on
 * the romdisk (romdisk/ifcommonitem.spr, Makefile) and the offset is the
 * number `llIFCommonItemArrowSprite` holds -- the same substitution
 * every offset table above makes, and the reason this trio is here
 * rather than with the item files: it is an interface GObj, and what it
 * reads is a sprite.
 *
 * ifCommonItemArrowMakeInterface's GObj carries the ITStruct in
 * user_data so the display proc can read the item's own state; the
 * decomp's comment on that line ("the GObj with the most flexible
 * user_data assignments ever?") is its own.
 * ------------------------------------------------------------------ */

#define IFCOMMON_ITEM_ARROW_SPRITE 0x50  /* &llIFCommonItemArrowSprite */

/* The bank is the port's own; the sprite is the decomp's ifcommon.c:428,
 * which is a plain global there and is kept one here so the host test can
 * read what ifCommonItemArrowSetAttr wrote onto it (test_it_init_items). */
static SpriteBank sIFCommonItemBank;
Sprite *sIFCommonItemArrowSprite;

/* ifcommon.c:1909-1937 ifCommonItemArrowProcDisplay 0x80111D64,
 * verbatim. The arrow hovers 100 units above the item's collision top,
 * projected the same way the player tags are, and shows only once the
 * item has been on the ground fifteen tics (arrow_timer). */
void ifCommonItemArrowProcDisplay(GObj *interface_gobj)
{
    ITStruct *ip = itGetStruct(interface_gobj);
    SObj *sobj;
    f32 x;
    f32 y;
    Vec3f pos;

    if ((ip->is_allow_pickup) && (ip->arrow_timer >= 15))
    {
        sobj = SObjGetStruct(interface_gobj);

        pos = DObjGetStruct(ip->item_gobj)->translate.vec.f;

        pos.y += ip->coll_data.map_coll.top + 100.0F;

        func_ovl2_800EB924(CObjGetStruct(gGMCameraGObj), gGMCameraMatrix, &pos, &x, &y);

        if (gmCameraCheckTargetInBounds(x, y) != FALSE)
        {
            sobj->pos.x = (s32) ((gGMCameraStruct.viewport_center_x + x) - (sobj->sprite.width * 0.5F));
            sobj->pos.y = (s32) ((gGMCameraStruct.viewport_center_y - y) - sobj->sprite.height);

            lbCommonDrawSObjAttr(interface_gobj);
        }
    }
}

/* ifcommon.c:1917-1938 ifCommonItemArrowMakeInterface 0x80111EC0,
 * verbatim. The one DIVERGES is the sprite: the decomp names the global
 * the file load filled in, and the port names the bank lookup's result,
 * which is the same pointer by a different road. Everything else --
 * the display proc, the user_data handoff, the hidden-in-a-paused-
 * training-battle arm -- is the decomp's line for line. */
GObj* ifCommonItemArrowMakeInterface(ITStruct *ip)
{
    GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

    if (interface_gobj != NULL)
    {
        gcAddGObjDisplay(interface_gobj, ifCommonItemArrowProcDisplay, 23, GOBJ_PRIORITY_DEFAULT, ~0);

        if (lbCommonMakeSObjForGObj(interface_gobj, sIFCommonItemArrowSprite) != NULL)
        {
            interface_gobj->user_data.p = ip; /* Give it up for... the GObj with the most flexible user_data assignments ever? */

            if ((gSCManagerSceneData.scene_curr == nSCKind1PTrainingMode) && (gSCManagerBattleState->game_status == nSCBattleGameStatusPause))
            {
                interface_gobj->flags = GOBJ_FLAG_HIDDEN;
            }
            return interface_gobj;
        }
        else gcEjectGObj(interface_gobj);
    }
    return NULL;
}

/* ifcommon.c:1941-1952 ifCommonItemArrowSetAttr 0x80111F80. The decomp
 * is one lbRelocGetExternHeapFile and one lbRelocGetFileData -- the pair
 * every .spr in the port replaces with one sprite_bank_load, which is
 * also where the bank's textures go to VRAM (src/dc/sprite.c). Called
 * once, from itManagerInitItems.
 *
 * A bank that will not load leaves sIFCommonItemArrowSprite NULL, which
 * is what the game's own load-failure path leaves too (a NULL extern
 * heap file resolves to offset 0x50 of nothing); MakeInterface's
 * lbCommonMakeSObjForGObj then fails its own != NULL check and ejects
 * the GObj, so the arrow is simply absent rather than fatal. The loader
 * has already named the missing file. */
void ifCommonItemArrowSetAttr(void)
{
    Sprite *sprite;

    if (sprite_bank_load(&sIFCommonItemBank, "ifcommonitem.spr") != 0)
    {
        sIFCommonItemArrowSprite = NULL;
        return;
    }
    sprite = sIFCommonItemArrowSprite = sprite_bank_get(&sIFCommonItemBank, IFCOMMON_ITEM_ARROW_SPRITE);

    sprite->attr = SP_TEXSHUF | SP_TRANSPARENT;

    sprite->red   = 0xFF;
    sprite->green = 0x00;
    sprite->blue  = 0x00;
}

/* ifcommon.c:1821-1850 ifCommonPlayerTagProcDisplay 0x80111ACC, verbatim.
 * The tag floats over the fighter's TopN joint, raised by the kind's
 * camera_zoom_base, projected through gGMCameraMatrix (src/dc/
 * gmcamera.c) and drawn only inside the viewport; shown while the
 * fighter's playertag_wait is 1 -- the pipe entry and the Kirby capture
 * set it to 1 outright, a held crouch counts it down from 120
 * (ft/ftcommon/ftcommondokan.c, ftcommoncapturekirby.c, ftcommonsquat.c)
 * -- or the camera is far enough back that the fighters are small. */
void ifCommonPlayerTagProcDisplay(GObj *interface_gobj)
{
    s32 player = ifGetPlayer(interface_gobj);
    FTStruct *fp;
    f32 x;
    f32 y;
    Vec3f pos;

    fp = ftGetStruct(gSCManagerBattleState->players[player].fighter_gobj);

    if (!(fp->is_playertag_bossend) && !(fp->is_playertag_hide))
    {
        if ((fp->playertag_wait == 1) || (CObjGetStruct(gGMCameraGObj)->vec.eye.z > 6000.0F))
        {
            pos = fp->joints[nFTPartsJointTopN]->translate.vec.f;

            pos.y += fp->attr->camera_zoom_base;

            func_ovl2_800EB924(CObjGetStruct(gGMCameraGObj), gGMCameraMatrix, &pos, &x, &y);

            if (gmCameraCheckTargetInBounds(x, y) != FALSE)
            {
                SObjGetStruct(interface_gobj)->pos.x = (s32) ((gGMCameraStruct.viewport_center_x + x) - (SObjGetStruct(interface_gobj)->sprite.width * 0.5F));
                SObjGetStruct(interface_gobj)->pos.y = (s32) ((gGMCameraStruct.viewport_center_y - y) - SObjGetStruct(interface_gobj)->sprite.height);

                lbCommonDrawSObjAttr(interface_gobj);
            }
        }
    }
}

/* ifcommon.c:1853-1885 ifCommonPlayerTagMakeInterface 0x80111BE4,
 * verbatim: one tag per player, the sprite by the player's tag kind
 * (1P..4P, CP, the ally heart) in the player's colour */
void ifCommonPlayerTagMakeInterface(void)
{
    GObj *interface_gobj;
    SObj *sobj;
    s32 player;
    u8 color_id;

    for (player = 0; player < ARRAY_COUNT(gSCManagerBattleState->players); player++)
    {
        if (gSCManagerBattleState->players[player].pkind != nFTPlayerKindNot)
        {
            interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

            gcAddGObjDisplay(interface_gobj, ifCommonPlayerTagProcDisplay, 23, GOBJ_PRIORITY_DEFAULT, ~0);

            sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[6], dIFCommonPlayerTagSpriteOffsets[gSCManagerBattleState->players[player].tag]));

            sobj->sprite.attr = SP_TEXSHUF | SP_TRANSPARENT;

            color_id = gSCManagerBattleState->players[player].color;

            sobj->sprite.red   = dIFCommonPlayerTagPrimColorsR[color_id];
            sobj->sprite.green = dIFCommonPlayerTagPrimColorsG[color_id];
            sobj->sprite.blue  = dIFCommonPlayerTagPrimColorsB[color_id];

            sobj->envcolor.r = dIFCommonPlayerTagEnvColorsR[color_id];
            sobj->envcolor.g = dIFCommonPlayerTagEnvColorsG[color_id];
            sobj->envcolor.b = dIFCommonPlayerTagEnvColorsB[color_id];

            ifSetPlayer(interface_gobj, player);
        }
    }
}

/* ifcommon.c:3219-3234 ifCommonPlayerStockMakeStockSnap 0x80114968 and
 * ifCommonPlayerScoreMakeEffect 0x801149CC, verbatim: the burst on the
 * stock row and the +1/-1 score popup, both particles on
 * LBPARTICLE_MASK_GENLINK(2) in HUD coordinates times four. */
void ifCommonPlayerStockMakeStockSnap(FTStruct *fp)
{
    efManagerStockSnapMakeEffect(gIFCommonPlayerInterface.player_pos_x[fp->player], gIFCommonPlayerInterface.player_pos_y);
}

void ifCommonPlayerScoreMakeEffect(FTStruct *fp, s32 score)
{
    Vec3f pos;

    pos.x = ((gIFCommonPlayerInterface.player_pos_x[fp->player] + dIFCommonPlayerScorePositionOffsetsX[fp->player]) << 2);
    pos.y = ((gIFCommonPlayerInterface.player_pos_y + 13) << 2); // ??? Can't get this one to match unless we do bitwise instead of literal multiplication
    pos.z = 0.0F;

    efManagerBattleScoreMakeEffect(&pos, score);
}

/* ---- the end of a match: if/ifcommon.c ------------------
 * When the stocks run out nothing else notices, because the
 * code that notices is the HUD's. It is not a HUD job by accident: the
 * one function that redraws the stock row after a KO,
 * ifCommonBattleUpdateScoreStocks, is also the one that counts how many
 * of a team are still alive, writes each team's placement as it falls,
 * and -- when the last placement is handed out -- announces GAME SET.
 *
 * From there the game runs a fixed timeline, and the whole of it is the
 * three functions the scene's per-frame update dispatches on
 * game_status. Go is the match; End freezes everything but the effects
 * and the HUD and hands over to BossDefeat, which counts the announce
 * banner's 90 tics down; then Set counts three more and calls
 * syTaskmanSetLoadScene, which is how a scene says it is finished. The
 * port's frame loop asks syTaskmanCheckBreakLoop the same question the
 * decomp's while (TRUE) asks it.
 *
 * DIVERGES, in one place, listed here rather than repeated at each site:
 *   - the pause menu is here, with its own DIVERGES
 *     block at the head of its section below;
 *   - the announcer's "Player 1... defeated" voice queue is
 *     ft/ftpublic.c's; that file is compiled
 *     unmodified and its GObj drains the queue a voice at a time; the
 *     GAME SET voice goes through the battle-end queue beside it.
 * ------------------------------------------------------------------ */

/* ifcommon.c:374-380, verbatim: the announcer's per-player "Player N"
 * voice, said before "defeated" when a player is knocked out for good */
u16 dIFCommonAnnounceDefeatedVoiceIDs[/* */] =
{
    nSYAudioVoiceAnnouncePlayer1,
    nSYAudioVoiceAnnouncePlayer2,
    nSYAudioVoiceAnnouncePlayer3,
    nSYAudioVoiceAnnouncePlayer4
};

/* ifcommon.c:413-449, the pause menu's own: who pressed
 * START, the model detail that player was drawn at before the close-up
 * raised it, where the pause camera's two angles stood when the pause
 * began, and which of the three menus is up. */
u8 sIFCommonBattlePausePlayer;
u8 sIFCommonBattlePausePlayerDetail;
f32 sIFCommonBattlePauseCameraEyeXOrigin;
f32 sIFCommonBattlePauseCameraEyeYOrigin;
u8 sIFCommonBattlePauseKindInterface;

/* ifcommon.c:419-473, the end-state's own variables */
u16 sIFCommonBattlePauseCameraRestoreWait;
s32 sIFCommonBattlePlace;
u16 sIFCommonBattleEndSoundsQueue[16];
u8 sIFCommonBattleEndSoundsNum;
u8 dIFCommonBattleBossUpdateInterval;
u8 dIFCommonBattleBossUpdateWait;
void (*sIFCommonBattleInterfaceProcUpdate)(void);
void (*sIFCommonBattleInterfaceProcSet)(void);

/* ifcommon.c:2545-2555 ifCommonAnnounceGameSetMakeInterface 0x8011341C,
 * verbatim. The banner goes on the interface link, which is what
 * ifCommonBattleInterfaceProcUpdate keeps running while the rest of
 * the world is frozen. */
GObj* ifCommonAnnounceGameSetMakeInterface(void)
{
    GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

    ifCommonAnnounceSetAttr(interface_gobj, 1, dIFCommonAnnounceGameSetSpriteData, ARRAY_COUNT(dIFCommonAnnounceGameSetSpriteData));

    return interface_gobj;
}

/* ifcommon.c:2557-2598 ifCommonBattleInitPlacement 0x80113488, verbatim.
 * Counts the teams (in a free-for-all a team is a player) and starts the
 * placement counter one below that: with four players the first team out
 * places 3rd... and the last team standing is never given a placement at
 * all, which is how the winner keeps the 0 the battle state was zeroed
 * with. members[5] for four players is the decomp's own comment's
 * "HAL goof"; it is kept because a team id indexes it. */
void ifCommonBattleInitPlacement(void)
{
    s32 i;
    s32 members[5]; // HAL goof?
    s32 teams;

    for (i = 0; i < ARRAY_COUNT(members); i++)
    {
        members[i] = 0;
    }
    switch (gSCManagerBattleState->is_team_battle)
    {
    case FALSE:
        for (i = 0; i < ARRAY_COUNT(gSCManagerBattleState->players); i++)
        {
            if (gSCManagerBattleState->players[i].pkind != nFTPlayerKindNot)
            {
                members[i]++;
            }
        }
        break;

    case TRUE:
        for (i = 0; i < ARRAY_COUNT(gSCManagerBattleState->players); i++)
        {
            if (gSCManagerBattleState->players[i].pkind != nFTPlayerKindNot)
            {
                members[gSCManagerBattleState->players[i].team]++;
            }
        }
        break;
    }
    for (i = teams = 0; i < ARRAY_COUNT(members); i++)
    {
        if (members[i] != 0)
        {
            teams++;
        }
    }
    sIFCommonBattlePlace = teams - 1;
}

/* ifcommon.c:2601-2607, verbatim */
void ifCommonBattleInterfacePauseGObj(GObj *interface_gobj, u32 unused)
{
    (void)unused;

    gcPauseGObjProcessAll(interface_gobj);

    interface_gobj->flags |= GOBJ_FLAG_NORUN;
}

/* ifcommon.c:2610-2616, verbatim */
void ifCommonBattleInterfaceResumeGObj(GObj *interface_gobj, u32 unused)
{
    (void)unused;

    gcResumeGObjProcessAll(interface_gobj);

    interface_gobj->flags &= ~GOBJ_FLAG_NORUN;
}

/* ifcommon.c:2618-2637 ifCommonBattleInterfaceProcUpdate 0x801136A4, the
 * freeze itself: pause every GObj in the scene, then let back in the two
 * links that have to keep moving through the banner -- the special
 * effects and the interface. The rumble actor is woken with them, and
 * needs its own call: it is the one GObj on nGCCommonLinkIDRumble (13),
 * which the freeze above took down with the rest and which neither of
 * the two links here covers -- gmRumbleResumeProcessAll walks that link
 * looking for nGCCommonKindRumble (src/dc/gmrumble.c). The two particle
 * GObjs come back too, but skipping every particle link except 2 and 3:
 * the HUD's own bursts and popups */
void ifCommonBattleInterfaceProcUpdate(void)
{
    gcFuncGObjAll(ifCommonBattleInterfacePauseGObj, 0);

    gcFuncGObjByLink(nGCCommonLinkIDSpecialEffect, ifCommonBattleInterfaceResumeGObj, 0);
    gcFuncGObjByLink(nGCCommonLinkIDInterface, ifCommonBattleInterfaceResumeGObj, 0);
    gmRumbleResumeProcessAll();
    ifCommonBattleInterfaceResumeGObj(gEFParticleStructsGObj, 0);
    ifCommonBattleInterfaceResumeGObj(gEFParticleGeneratorsGObj, 0);
    efParticleGObjSetSkipAll();
    efParticleGObjClearSkipID(2);
    efParticleGObjClearSkipID(3);
    func_800266A0_272A0();
    ifCommonBattleEndPlaySoundQueue();
}

/* ifcommon.c:2648-2651, verbatim */
void ifCommonBattleEndInitSoundNum(void)
{
    sIFCommonBattleEndSoundsNum = 0;
}

/* ifcommon.c:2652-2661, verbatim but for the FGM call, which is
 * func_800269C0_275C0 by ROM address in the decomp and the port's own
 * FGM interpreter here (src/dc/sysshim.c) */
void ifCommonBattleEndPlaySoundQueue(void)
{
    s32 i;

    for (i = 0; i < sIFCommonBattleEndSoundsNum; i++)
    {
        func_800269C0_275C0(sIFCommonBattleEndSoundsQueue[i]);
    }
}

/* ifcommon.c:2663-2671, verbatim. The queue exists because the sounds
 * are raised while the world is still running and must not play until
 * the freeze; ifCommonBattleInterfaceProcUpdate is what plays them. */
void ifCommonBattleEndAddSoundQueueID(u16 sfx_id)
{
    if ((gSCManagerBattleState->game_status == nSCBattleGameStatusEnd) && (sIFCommonBattleEndSoundsNum < ARRAY_COUNT(sIFCommonBattleEndSoundsQueue)))
    {
        sIFCommonBattleEndSoundsQueue[sIFCommonBattleEndSoundsNum] = sfx_id;
        sIFCommonBattleEndSoundsNum++;
    }
}

/* ifcommon.c:2674-2680 ifCommonBattleEndSetBossDefeat 0x80113854,
 * verbatim. The end of Master Hand's defeat: sc1pgameboss.c's last
 * wallpaper fade calls it, and zeroing the restore wait is what lets
 * ifCommonBattleBossDefeatUpdateInterface run the proc_set the next tic
 * -- ifCommonBattleBossDefeatSetGameStatus had parked it at U16_MAX. */
void ifCommonBattleEndSetBossDefeat(void)
{
    func_ovl65_8018F6DC();

    gSCManagerBattleState->game_status = nSCBattleGameStatusBossDefeat;
    sIFCommonBattlePauseCameraRestoreWait = 0;
}

/* ifcommon.c:2682-2748 ifCommonBattleUpdateScoreStocks 0x8011388C,
 * verbatim. ftCommonDeadUpdateScore calls this on every KO under the
 * stock rule, after it has already decremented the stock; what it adds
 * is the question that decides the match. Count this fighter's team's
 * survivors: if none are left the team has just placed, and the
 * placement counter walks down. Reaching zero means one team is still
 * standing and the match is over. */
void ifCommonBattleUpdateScoreStocks(FTStruct *fp)
{
    s32 teammates_remain; // Live teammates remaining
    /* the decomp declares this bare; is_team_battle is FALSE or TRUE and
     * the switch below covers both, but the compiler cannot see that */
    s32 current_team = 0; // Current team being checked
    s32 team; // Input player's team
    s32 i;

    team = fp->team;

    for (i = teammates_remain = 0; i < ARRAY_COUNT(gSCManagerBattleState->players); i++)
    {
        if (gSCManagerBattleState->players[i].pkind == nFTPlayerKindNot)
        {
            continue;
        }
        switch (gSCManagerBattleState->is_team_battle)
        {
        case FALSE:
            current_team = i;
            break;

        case TRUE:
            current_team = gSCManagerBattleState->players[i].team;
            break;
        }
        if ((current_team == team) && (gSCManagerBattleState->players[i].stock_count != -1))
        {
            teammates_remain++;
        }
    }
    if (teammates_remain == 0) // No players left on this team
    {
        switch (gSCManagerBattleState->is_team_battle)
        {
        case FALSE:
            gSCManagerBattleState->players[team].place = sIFCommonBattlePlace;
            break;

        case TRUE:
            for (i = 0; i < ARRAY_COUNT(gSCManagerBattleState->players); i++)
            {
                if (gSCManagerBattleState->players[i].pkind == nFTPlayerKindNot)
                {
                    continue;
                }
                if (gSCManagerBattleState->players[i].team == team)
                {
                    gSCManagerBattleState->players[i].place = sIFCommonBattlePlace;
                }
            }
            break;
        }
        sIFCommonBattlePlace--;

        if (sIFCommonBattlePlace == 0)
        {
            ifCommonAnnounceEndMessage();
        }
    }
    if ((sIFCommonBattlePlace != 0) && (fp->stock_count == -1))
    {
        if (fp->pkind == nFTPlayerKindMan)
        {
            ftPublicDefeatedAddID(dIFCommonAnnounceDefeatedVoiceIDs[fp->player]);
        }
        else ftPublicDefeatedAddID(nSYAudioVoiceAnnounceComputerPlayer);

        ftPublicDefeatedAddID(nSYAudioVoiceAnnounceDefeated);
    }
}

/* ifcommon.c:2856-2866, verbatim */
void ifCommonInterfaceSetGObjFlagsAll(u32 flags)
{
    GObj *interface_gobj = gGCCommonLinks[nGCCommonLinkIDInterface];

    while (interface_gobj != NULL)
    {
        interface_gobj->flags = flags;

        interface_gobj = interface_gobj->link_next;
    }
}

/* ---- the pause menu -----------------------------------
 *
 * The whole of it, from the START that opens it to the START that closes
 * it, is here. What it is made of is two GObjs on their own link: one
 * that draws the white frame and one that carries the sprites, both at
 * display priority 24, above the HUD's 23. The HUD itself does not go
 * away -- ifCommonInterfaceSetGObjFlagsAll hides it, and the same call
 * with 0 brings it back.
 *
 * The interesting half is not the menu but what pausing does to the
 * world: ifCommonBattlePauseInitInterface sets the game status to Pause,
 * and the dispatcher's Pause arm does not call gcRunAll at all. That is
 * the freeze -- not a flag any fighter reads, just a tic on which the
 * object system is never run. The camera keeps moving because the pause
 * arm calls gmCameraRunFuncCamera directly, which is how the close-up
 * pans while everything inside it is still.
 *
 * DIVERGES, listed here rather than repeated at each site:
 *   - grWallpaperPausePerspUpdate and grWallpaperResumePerspUpdate are
 *     not called: they write a flag nothing in the game reads
 *     (gr/grwallpaper.c:307-313), and grWallpaperRunProcessAll's work is
 *     done by the port's camera-attached wallpaper draw, every frame,
 *     paused or not (src/dc/stage.c stage_proc_camera_wallpaper);
 *   - func_80026594_27194, which the game calls just before the pause
 *     chime to silence every voice whose priority carries bit 0x80, has
 *     nothing to call: the port's FGM engine (src/dc/fgm.c) carries no
 *     voice priority at all, so there is no flag to test. The chime
 *     itself, and the music dropping to 0x3C00 under it, are the game's;
 *   - is_magnify_display is written on both sides as the game writes it,
 *     and read where the game reads it (src/dc/ftdisplaymain.c).
 * ------------------------------------------------------------------ */

/* ifcommon.c:2755-2777 ifCommonBattlePauseProcDisplay 0x80113AA8. The
 * five white rails of the frame. The decomp fills them in G_CYC_FILL with
 * the render mode off, which on the RDP means "write these pixels and ask
 * nothing"; lbCommonSpriteFillRect is the port's own equivalent through
 * the PVR (src/dc/lbcommon.c), and takes an exclusive lower-right corner
 * where GPACK/gDPFillRectangle takes an inclusive one -- hence the two
 * +1s, the same convention src/dc/mnvsoptions.c's underline uses.
 *
 * The colour is GPACK_RGBA5551(0xFF, 0xFF, 0xFF, 0x01): white, and the
 * alpha bit set, which is the 5551 framebuffer's coverage rather than a
 * blend. Opaque white is what it comes to. */
void ifCommonBattlePauseProcDisplay(GObj *interface_gobj)
{
    s32 i;

    (void)interface_gobj;

    for (i = 0; i < ARRAY_COUNT(dIFCommonBattlePauseBorderRectangle); i++)
    {
        lbCommonSpriteFillRect
        (
            dIFCommonBattlePauseBorderRectangle[i].ulx,
            dIFCommonBattlePauseBorderRectangle[i].uly,
            dIFCommonBattlePauseBorderRectangle[i].lrx + 1,
            dIFCommonBattlePauseBorderRectangle[i].lry + 1,
            0xFF, 0xFF, 0xFF, 0xFF
        );
    }
    lbCommonClearExternSpriteParams();
}

/* ifcommon.c:2779-2789 ifCommonBattlePausePlayerNumMakeSObj 0x80113CF8,
 * verbatim: whose pause it is, in the banner's right-hand corner. */
void ifCommonBattlePausePlayerNumMakeSObj(GObj *interface_gobj, s32 player)
{
    SObj *sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[5], dIFCommonBattlePausePlayerNumSpriteOffsets[player]));

    sobj->sprite.red   = 0xFF;
    sobj->sprite.green = 0xFF;
    sobj->sprite.blue  = 0xFF;

    sobj->sprite.attr = SP_TEXSHUF | SP_TRANSPARENT;

    sobj->pos.x = 213.0F;
    sobj->pos.y = 191.0F;
}

/* ifcommon.c:2792-2810 ifCommonBattlePauseDecalMakeSObjID 0x80113D60,
 * verbatim: one decal, at its place and in its two colours. */
void ifCommonBattlePauseDecalMakeSObjID(GObj *interface_gobj, s32 id)
{
    SObj *sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[5], dIFCommonBattlePauseDecalsSpriteData[id].offset));

    sobj->pos.x = dIFCommonBattlePauseDecalsSpriteData[id].pos.x;
    sobj->pos.y = dIFCommonBattlePauseDecalsSpriteData[id].pos.y;

    sobj->sprite.red   = dIFCommonBattlePauseDecalsSpriteData[id].colors.prim.r;
    sobj->sprite.green = dIFCommonBattlePauseDecalsSpriteData[id].colors.prim.g;
    sobj->sprite.blue  = dIFCommonBattlePauseDecalsSpriteData[id].colors.prim.b;

    sobj->envcolor.r = dIFCommonBattlePauseDecalsSpriteData[id].colors.env.r;
    sobj->envcolor.g = dIFCommonBattlePauseDecalsSpriteData[id].colors.env.g;
    sobj->envcolor.b = dIFCommonBattlePauseDecalsSpriteData[id].colors.env.b;

    sobj->sprite.attr = SP_TEXSHUF | SP_TRANSPARENT;
}

/* ifcommon.c:2812-2830 ifCommonBattlePauseMakeSObjsAll 0x80113E04.
 * Ten decals or twelve: the control stick and its arrows are only drawn
 * for the default menu, because they say "the stick moves the camera"
 * and the other two menus have no camera to move. A bonus stage played
 * from its own menu (not from a 1P run) adds the "L: RETRY" pair. */
void ifCommonBattlePauseMakeSObjsAll(GObj *interface_gobj)
{
    s32 draw_count = (sIFCommonBattlePauseKindInterface != nIFPauseKindDefault) ? 10 : 12;
    s32 i;

    for (i = 0; i < draw_count; i++)
    {
        ifCommonBattlePauseDecalMakeSObjID(interface_gobj, i);
    }
    // If we're in Bonus Practice, display "L: RETRY" in the bottom left corner
    if ((gSCManagerSceneData.scene_curr == nSCKind1PBonusStage) && (gSCManagerSceneData.scene_prev != nSCKind1PGame))
    {
        // WARNING: This needs to be updated in case the pause menu icon array is expanded
        for (i = 12; i < ARRAY_COUNT(dIFCommonBattlePauseDecalsSpriteData); i++)
        {
            ifCommonBattlePauseDecalMakeSObjID(interface_gobj, i);
        }
    }
}

/* ifcommon.c:2833-2847 ifCommonBattlePauseMakeInterface 0x80113EB4,
 * verbatim. Two GObjs, both on the pause-menu link so that ejecting the
 * link ejects the menu: the frame, and the sprites on top of it. */
void ifCommonBattlePauseMakeInterface(s32 player)
{
    GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindPauseMenu, NULL, nGCCommonLinkIDPauseMenu, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(interface_gobj, ifCommonBattlePauseProcDisplay, 24, GOBJ_PRIORITY_DEFAULT, ~0);

    interface_gobj = gcMakeGObjSPAfter(nGCCommonKindPauseMenu, NULL, nGCCommonLinkIDPauseMenu, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 24, GOBJ_PRIORITY_DEFAULT, ~0);

    ifCommonBattlePausePlayerNumMakeSObj(interface_gobj, player);
    ifCommonBattlePauseMakeSObjsAll(interface_gobj);
}

/* ifcommon.c:2850-2853, verbatim */
void ifCommonBattlePauseEjectGObjs(void)
{
    lbCommonEjectGObjLinkedList(gGCCommonLinks[nGCCommonLinkIDPauseMenu]);
}

/* ifcommon.c:2869-2879, verbatim */
void ifCommonBattlePauseSetGObjFlagsAll(u32 flags)
{
    GObj *gobj = gGCCommonLinks[nGCCommonLinkIDPauseMenu];

    while (gobj != NULL)
    {
        gobj->flags = flags;

        gobj = gobj->link_next;
    }
}

/* ifcommon.c:2882-2897 ifCommonBattlePauseInitInterface 0x80113FC4.
 * The pause itself, in the order the game does it: hide the HUD, stop the
 * rumble, set the status the dispatcher reads, remember who paused, ring
 * the chime, duck the music to half, and build the menu. DIVERGES:
 * grWallpaperPausePerspUpdate, between the rumble and the magnify flag,
 * is not called; see the block above. */
void ifCommonBattlePauseInitInterface(s32 player)
{
    ifCommonInterfaceSetGObjFlagsAll(GOBJ_FLAG_HIDDEN);
    gmRumbleInitPlayers();

    gIFCommonPlayerInterface.is_magnify_display = FALSE;
    gSCManagerBattleState->game_status = nSCBattleGameStatusPause;

    sIFCommonBattlePausePlayer = player;

    func_800269C0_275C0(nSYAudioFGMGamePause);
    syAudioSetBGMVolume(0, 0x3C00);
    ifCommonBattlePauseMakeInterface(player);
}

/* ifcommon.c:2899-2971 ifCommonBattleGoUpdateInterface 0x80114060.
 * Every tic of a running match passes through here, and all it is is a
 * scan of the four controllers for START. The player who presses it gets
 * the menu; the last line, reached when nobody did, is the match.
 *
 * Three things can stop a press from pausing. A fighter asleep may or may
 * not be allowed to (ftCommonSleepCheckIgnorePauseMenu), is_menu_ignore
 * covers the frames a fighter must not be interrupted on, and a player
 * whose slot is empty falls to the goto-less tail below -- kind 0, which
 * is nIFPauseKindPlayerNA, the menu with no camera. That last arm is why
 * pressing START on an unplugged pad still pauses.
 *
 * A bonus stage pauses on the whole course instead: the camera zooms out
 * between the map's zoom_start and zoom_end (STG5 carries them), and the
 * menu is nIFPauseKindBonus. On Race to the Finish a COM cannot pause. */
void ifCommonBattleGoUpdateInterface(void)
{
    GObj *fighter_gobj;
    FTStruct *fp;
    s32 player;
    Vec3f sp68;
    Vec3f sp5C;

    for (player = 0; player < (ARRAY_COUNT(gSCManagerBattleState->players) + ARRAY_COUNT(gSYControllerDevices)) / 2; player++) // WARNING: GMCOMMON_PLAYERS_MAX and MAX_CONTROLLERS should be identical
    {
        if (gSYControllerDevices[player].button_tap & START_BUTTON)
        {
            if (gSCManagerBattleState->players[player].pkind != nFTPlayerKindNot)
            {
                if ((gSCManagerBattleState->gkind != nGRKindBonus3) || (gSCManagerBattleState->players[player].pkind != nFTPlayerKindCom))
                {
                    fighter_gobj = gSCManagerBattleState->players[player].fighter_gobj;

                    fp = ftGetStruct(fighter_gobj);

                    if ((fp->status_id == nFTCommonStatusSleep) && (ftCommonSleepCheckIgnorePauseMenu(fighter_gobj) != FALSE))
                    {
                        continue;
                    }
                    if (!(fp->is_menu_ignore))
                    {
                        if (gSCManagerBattleState->game_type == nSCBattleGameTypeBonus)
                        {
                            sp68.x = gMPCollisionGroundData->zoom_start.x;
                            sp68.y = gMPCollisionGroundData->zoom_start.y;
                            sp68.z = gMPCollisionGroundData->zoom_start.z;

                            sp5C.x = gMPCollisionGroundData->zoom_end.x;
                            sp5C.y = gMPCollisionGroundData->zoom_end.y;
                            sp5C.z = gMPCollisionGroundData->zoom_end.z;

                            gmCameraSetStatusMapZoom(&sp68, &sp5C);

                            sIFCommonBattlePauseKindInterface = nIFPauseKindBonus;
                        }
                        else if (gmCameraCheckPausePlayerOutBounds(&DObjGetStruct(fighter_gobj)->translate.vec.f) != FALSE)
                        {
                            sIFCommonBattlePauseKindInterface = nIFPauseKindPlayerNA;
                        }
                        else
                        {
                            gmCameraSetStatusPlayerZoom(fighter_gobj, 0.0F, 0.0F, ftGetStruct(fighter_gobj)->attr->closeup_camera_zoom, 0.1F, 29.0F);

                            sIFCommonBattlePauseCameraEyeXOrigin = gGMCameraPauseCameraEyeX;
                            sIFCommonBattlePauseCameraEyeYOrigin = gGMCameraPauseCameraEyeY;

                            sIFCommonBattlePauseKindInterface = nIFPauseKindDefault;

                            sIFCommonBattlePausePlayerDetail = fp->detail_curr;

                            ftParamSetModelPartDetailAll(fighter_gobj, nFTPartsDetailHigh);
                        }
                        ifCommonBattlePauseInitInterface(player);

                        return;
                    }
                }
            }
            sIFCommonBattlePauseKindInterface = 0;

            ifCommonBattlePauseInitInterface(player);

            return;
        }
    }
    gcRunAll();
}

/* ifcommon.c:2973-2981, verbatim */
void ifCommonBattleInterfaceProcSet(void)
{
    ifCommonInterfaceSetGObjFlagsAll(GOBJ_FLAG_HIDDEN);

    gSCManagerBattleState->game_status = nSCBattleGameStatusSet;

    sIFCommonBattlePauseCameraRestoreWait = 3;
}

/* ifcommon.c:2984-3070 ifCommonBattlePauseUpdateInterface 0x801142EC.
 * A paused tic. Note what is not here: gcRunAll. The world is frozen
 * because this arm of the dispatcher never runs the object system, and
 * the camera moves because the last two lines run it by hand.
 *
 * The stick pans the close-up, +-50 degrees across and +-20 up and down,
 * at 0.000333 radians per unit of stick per tic. START unpauses -- by
 * putting the camera back and setting the status to Unpause, which is the
 * next function. A+B+R+Z held together is the reset: is_reset goes true,
 * the world is frozen for good, the menu is hidden rather than ejected,
 * and the match ends into ifCommonBattleInterfaceProcSet's three tics.
 * gSCManagerSceneData.is_reset is what the results screen reads to show
 * NO CONTEST (src/dc/scvsresults.c) and what scVSBattleStartScene reads
 * to know not to run sudden death (src/dc/scvsbattle.c). *
 * On a bonus stage entered from its menu rather than from a 1P run, L
 * retries: the task ends with the status still Pause, which is what
 * sc1PBonusStageStartScene reads to run the course again.
 *
 * DIVERGES: grWallpaperRunProcessAll, whose work the camera's wallpaper
 * draw does. */
void ifCommonBattlePauseUpdateInterface(void)
{
    u16 button_tap = gSYControllerDevices[sIFCommonBattlePausePlayer].button_tap;
    u16 button_hold = gSYControllerDevices[sIFCommonBattlePausePlayer].button_hold;

    if (sIFCommonBattlePauseKindInterface == nIFPauseKindDefault)
    {
        s32 stick_x = gSYControllerDevices[sIFCommonBattlePausePlayer].stick_range.x;
        s32 stick_y = gSYControllerDevices[sIFCommonBattlePausePlayer].stick_range.y;

        if (ABS(stick_x) > 8.0F)
        {
            gGMCameraPauseCameraEyeX += (stick_x * 0.000333F);

            if (gGMCameraPauseCameraEyeX > F_CLC_DTOR32(50.0F))
            {
                gGMCameraPauseCameraEyeX = F_CLC_DTOR32(50.0F);
            }
            else if (gGMCameraPauseCameraEyeX < F_CLC_DTOR32(-50.0F))
            {
                gGMCameraPauseCameraEyeX = F_CLC_DTOR32(-50.0F);
            }
        }
        if (ABS(stick_y) > 8.0F)
        {
            gGMCameraPauseCameraEyeY -= (stick_y * 0.000333F);

            if (gGMCameraPauseCameraEyeY > F_CLC_DTOR32(20.0F))
            {
                gGMCameraPauseCameraEyeY = F_CLC_DTOR32(20.0F);
            }
            else if (gGMCameraPauseCameraEyeY < F_CLC_DTOR32(-20.0F))
            {
                gGMCameraPauseCameraEyeY = F_CLC_DTOR32(-20.0F);
            }
        }
    }
    if (button_tap)
    {
        if (button_tap & START_BUTTON)
        {
            if (sIFCommonBattlePauseKindInterface != nIFPauseKindPlayerNA)
            {
                gmCameraSetStatusPrev();

                sIFCommonBattlePauseCameraRestoreWait = 20;
            }
            else sIFCommonBattlePauseCameraRestoreWait = 0;

            gSCManagerBattleState->game_status = nSCBattleGameStatusUnpause;

            return;
        }
        else button_hold = gSYControllerDevices[sIFCommonBattlePausePlayer].button_hold;

        if
        (
            (button_hold & A_BUTTON) &&
            (button_hold & B_BUTTON) &&
            (button_hold & R_TRIG) &&
            (button_hold & Z_TRIG)
        )
        {
            gSCManagerSceneData.is_reset = TRUE;

            gcFuncGObjAll(ifCommonBattleInterfacePauseGObj, 0);
            func_800266A0_272A0();
            gmRumbleInitPlayers();
            ifCommonBattlePauseSetGObjFlagsAll(GOBJ_FLAG_HIDDEN);
            ifCommonBattleInterfaceProcSet();

            return;
        }
        if ((button_tap & L_TRIG) && (gSCManagerSceneData.scene_curr == nSCKind1PBonusStage) && (gSCManagerSceneData.scene_prev != nSCKind1PGame))
        {
            func_800266A0_272A0();
            gmRumbleInitPlayers();
            syTaskmanSetLoadScene();

            return;
        }
    }
    if (sIFCommonBattlePauseKindInterface != nIFPauseKindPlayerNA)
    {
        gmCameraRunFuncCamera(gGMCameraGObj);
    }
}

/* ifcommon.c:3073-3106 ifCommonBattlePauseRestoreInterfaceAll 0x80114588.
 * Unpausing takes twenty tics, not one: gmCameraSetStatusPrev has already
 * put the camera back on the match, and these are the tics its two pause
 * angles ease back to where they stood, a tenth of the remaining distance
 * each. Only when the wait runs out is the menu ejected, the HUD shown,
 * the music brought back up and the status set to Go -- and then, and only
 * then, gcRunAll, the first tic the world moves on again.
 *
 * The eject is by link: lbCommonEjectGObjLinkedList walks the pause-menu
 * link to its end and ejects backward, so both GObjs and every SObj they
 * carry go at once (src/dc/lbcommon.c).
 *
 * DIVERGES: grWallpaperResumePerspUpdate and func_800264A4_270A4, both
 * for the reasons in the block above. */
void ifCommonBattlePauseRestoreInterfaceAll(void)
{
    if (sIFCommonBattlePauseCameraRestoreWait != 0)
    {
        sIFCommonBattlePauseCameraRestoreWait--;

        gGMCameraPauseCameraEyeX += (sIFCommonBattlePauseCameraEyeXOrigin - gGMCameraPauseCameraEyeX) * 0.1F;
        gGMCameraPauseCameraEyeY += (sIFCommonBattlePauseCameraEyeYOrigin - gGMCameraPauseCameraEyeY) * 0.1F;

        gmCameraRunFuncCamera(gGMCameraGObj);

        return;
    }
    ifCommonBattlePauseEjectGObjs();
    ifCommonInterfaceSetGObjFlagsAll(0);

    gIFCommonPlayerInterface.is_magnify_display = TRUE;

    gSCManagerBattleState->game_status = nSCBattleGameStatusGo;

    gGMCameraPauseCameraEyeX = sIFCommonBattlePauseCameraEyeXOrigin;
    gGMCameraPauseCameraEyeY = sIFCommonBattlePauseCameraEyeYOrigin;

    syAudioSetBGMVolume(0, 0x7800);

    if (sIFCommonBattlePauseKindInterface == nIFPauseKindDefault)
    {
        ftParamSetModelPartDetailAll(gSCManagerBattleState->players[sIFCommonBattlePausePlayer].fighter_gobj, sIFCommonBattlePausePlayerDetail);
    }
    gcRunAll();
}

/* ifcommon.c:3108-3116, verbatim. One frame long: it freezes the world,
 * then hands the rest of the wait to the function below by setting the
 * status the dispatcher's fallthrough is about to run anyway. */
void ifCommonBattleEndUpdateInterface(void)
{
    sIFCommonBattleInterfaceProcUpdate();

    gSCManagerBattleState->game_status = nSCBattleGameStatusBossDefeat;

    dIFCommonBattleBossUpdateInterval = dIFCommonBattleBossUpdateWait = 0;
}

/* ifcommon.c:3288-3296 ifCommonBattleBossDefeatSetGameStatus 0x80114BE4,
 * verbatim. Master Hand's own way into the status above: the ladder's
 * boss scene calls it when the hand is beaten (src/dc/sc1pgame.c,
 * sc1PGameBossDefeatInterfaceProcSet), where a VS match reaches the
 * same status through ifCommonBattleEndUpdateInterface just above. The
 * interval of 2 is what makes the defeat play at half speed. */
void ifCommonBattleBossDefeatSetGameStatus(void)
{
    gSCManagerBattleState->game_status = nSCBattleGameStatusBossDefeat;

    sIFCommonBattlePauseCameraRestoreWait = U16_MAX;

    sIFCommonBattleInterfaceProcSet = ifCommonBattleInterfaceProcSet;
    dIFCommonBattleBossUpdateInterval = 2;
}

/* ifcommon.c:3118-3141 ifCommonBattleBossDefeatUpdateInterface 0x80114724.
 * The 90 tics of the GAME SET banner. The interval is Master Hand's --
 * ifCommonBattleBossDefeatSetGameStatus sets it to 2 so his defeat runs
 * the scene every other tic -- and is 0 for a VS match, so the scene runs
 * every tic, over the GObjs the freeze left running.
 *
 * DIVERGES: grWallpaperRunProcessAll, the wallpaper's own per-tic update
 * on the tics the else arm skips the scene, is not called: the port
 * draws the wallpaper from the camera every frame (src/dc/stage.c).
 * The camera call beside it is the port's own. */
void ifCommonBattleBossDefeatUpdateInterface(void)
{
    if (sIFCommonBattlePauseCameraRestoreWait != 0)
    {
        sIFCommonBattlePauseCameraRestoreWait--;
    }
    else sIFCommonBattleInterfaceProcSet();

    if (dIFCommonBattleBossUpdateWait == 0)
    {
        gcRunAll();

        dIFCommonBattleBossUpdateWait = dIFCommonBattleBossUpdateInterval;
    }
    else
    {
        gmCameraRunFuncCamera(gGMCameraGObj);

        dIFCommonBattleBossUpdateWait--;
    }
}

/* ifcommon.c:3143-3153, verbatim. The last three tics of a match. */
void ifCommonBattleSetUpdateInterface(void)
{
    if (sIFCommonBattlePauseCameraRestoreWait != 0)
    {
        sIFCommonBattlePauseCameraRestoreWait--;
    }
    else syTaskmanSetLoadScene();

    gcRunAll();
}

/* ifcommon.c:3155-3164 ifCommonSetMaxNumGObj 0x80114800, verbatim: once
 * the scene heap is down to its last 25 KB, cap the GObj count where it
 * stands so a late spawn cannot take the rest. gcGetMaxNumGObj is -1
 * until this fires, and it fires at most once. */
void ifCommonSetMaxNumGObj(void)
{
    size_t free_space = (size_t) ((uintptr_t)gSYTaskmanGeneralHeap.end - (uintptr_t)gSYTaskmanGeneralHeap.ptr);

    if ((gcGetMaxNumGObj() == -1) && (free_space < 25 * 1024))
    {
        gcSetMaxNumGObj(gcGetGObjsActiveNum());
    }
}

/* ifcommon.c:3166-3210 ifCommonBattleUpdateInterfaceAll 0x8011485C, the
 * VS battle scene's whole per-frame update (scvsbattle.c:75
 * scVSBattleFuncUpdate is one line, and this is the line). Every arm
 * ends in a gcRunAll, directly or through the function it calls, so the
 * object system is run exactly once per tic whatever the game status --
 * that is the invariant this dispatcher exists to keep.
 *
 * Every arm of it is the game's. The two lines above the switch are the match
 * clock's: the retrace count goes back to zero on the tic
 * the status becomes Go, and the clock's stamp with it, so the first
 * delta it sees is one tic and not one battle's worth of them -- and,
 * because a pause leaves the status Pause, the clock restarts from zero
 * on the tic the match resumes rather than counting the pause. */
void ifCommonBattleUpdateInterfaceAll(void)
{
    if (gSCManagerBattleState->game_status != nSCBattleGameStatusGo)
    {
        sIFCommonTimerIsStarted = FALSE;
    }
    else if (sIFCommonTimerIsStarted == FALSE)
    {
        sIFCommonTimerIsStarted = TRUE;
        sIFCommonTimerStamp = 0;

        sySchedulerSetTicCount(0);
    }
    switch (gSCManagerBattleState->game_status)
    {
    case nSCBattleGameStatusWait:
        gcRunAll();
        break;

    case nSCBattleGameStatusGo:
        ifCommonBattleGoUpdateInterface();
        break;

    case nSCBattleGameStatusPause:
        ifCommonBattlePauseUpdateInterface();
        break;

    case nSCBattleGameStatusUnpause:
        ifCommonBattlePauseRestoreInterfaceAll();
        break;

    case nSCBattleGameStatusEnd:
        ifCommonBattleEndUpdateInterface();
        /* fallthrough */

    case nSCBattleGameStatusBossDefeat:
        ifCommonBattleBossDefeatUpdateInterface();
        break;

    case nSCBattleGameStatusSet:
        ifCommonBattleSetUpdateInterface();
        break;
    }
    ifCommonSetMaxNumGObj();
}

/* ifcommon.c:3270-3283 ifCommonBattleSetInterface 0x80114B80, verbatim.
 * The one entry point into the end state: whoever ends a match names the
 * two functions the timeline will call and how long the banner holds. */
void ifCommonBattleSetInterface(void (*proc_update)(void), void (*proc_set)(void), u16 sfx_id, u16 restore_wait)
{
    gSCManagerBattleState->game_status = nSCBattleGameStatusEnd;
    sIFCommonBattlePauseCameraRestoreWait = restore_wait;

    sIFCommonBattleInterfaceProcUpdate = proc_update;
    sIFCommonBattleInterfaceProcSet = proc_set;

    ifCommonBattleEndInitSoundNum();

    if (sfx_id != nSYAudioFGMVoiceEnd)
    {
        ifCommonBattleEndAddSoundQueueID(sfx_id);
    }
}

/* ifcommon.c:3262-3268 ifCommonBonusInterfaceProcUpdate 0x80114B40: the
 * freeze a TIME UP or a bonus-stage banner runs under. Unlike
 * ifCommonBattleInterfaceProcUpdate above it lets nothing back in -- when
 * the clock runs out, everything stops -- and that includes the pads: the
 * rumble actor stays frozen with the rest, but the motors themselves are
 * stopped outright rather than left where they were. */
void ifCommonBonusInterfaceProcUpdate(void)
{
    gcFuncGObjAll(ifCommonBattleInterfacePauseGObj, 0);
    gmRumbleInitPlayers();
    func_800266A0_272A0();
    ifCommonBattleEndPlaySoundQueue();
}

/* ifcommon.c:3250-3259 ifCommonAnnounceCompleteMakeInterface 0x80114A80,
 * verbatim: the COMPLETE! banner, built the way SUDDEN
 * DEATH is rather than the way TIME UP is -- out of file 7's plain
 * alphabet with its own colours, not file 1's pre-coloured letters --
 * and with no thread on it, because what takes it away is the 90-tic
 * restore_wait its Init below hands ifCommonBattleSetInterface. */
GObj* ifCommonAnnounceCompleteMakeInterface(void)
{
    GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);
    ifCommonAnnounceSetAttr(interface_gobj, 7, dIFCommonAnnounceCompleteSpriteData, ARRAY_COUNT(dIFCommonAnnounceCompleteSpriteData));
    ifCommonAnnounceSetColors(interface_gobj, &dIFCommonAnnounceCompleteSpriteColors);

    return interface_gobj;
}

/* ifcommon.c:3335-3339 ifCommonAnnounceCompleteInitInterface 0x80114D5C,
 * verbatim: the same shape as TIME UP below, freeze and all -- the
 * bonus stage stops dead for 90 tics with COMPLETE! over it -- but the
 * voice line is the CALLER's, because the three bonus stages say
 * different things when they are cleared. gr/grbonus/grbonus3.c passes
 * nSYAudioVoiceAnnounceComplete. */
void ifCommonAnnounceCompleteInitInterface(u16 sfx_id)
{
    ifCommonBattleSetInterface(ifCommonBonusInterfaceProcUpdate, ifCommonBattleInterfaceProcSet, sfx_id, 90);
    ifCommonAnnounceCompleteMakeInterface();
}

/* ifcommon.c:3238-3247 ifCommonAnnounceFailureMakeInterface 0x80114A48,
 * verbatim: COMPLETE!'s twin, one letter table over. */
GObj* ifCommonAnnounceFailureMakeInterface(void)
{
    GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);
    ifCommonAnnounceSetAttr(interface_gobj, 7, dIFCommonAnnounceFailureSpriteData, ARRAY_COUNT(dIFCommonAnnounceFailureSpriteData));
    ifCommonAnnounceSetColors(interface_gobj, &dIFCommonAnnounceFailureSpriteColors);

    return interface_gobj;
}

/* ifcommon.c:3349-3359 ifCommonAnnounceFailureInitInterface 0x80114DD4,
 * verbatim, the REGION_US arm: the guard is what keeps a clock that
 * runs out on the same tic the tenth target breaks from painting
 * FAILURE over COMPLETE! -- the completion arm has already set
 * nSCBattleGameStatusEnd by then. The JP build has no guard and can. */
void ifCommonAnnounceFailureInitInterface(void)
{
    if (gSCManagerBattleState->game_status != nSCBattleGameStatusEnd)
    {
        ifCommonBattleSetInterface(ifCommonBonusInterfaceProcUpdate, ifCommonBattleInterfaceProcSet, nSYAudioVoiceAnnounceFailure, 90);
        ifCommonAnnounceFailureMakeInterface();
    }
}

/* ifcommon.c:3342-3347 ifCommonAnnounceTimeUpInitInterface 0x80114D98,
 * verbatim. What the clock calls on the tic it reaches zero: end the
 * match the same way GAME SET does, with TIME UP over it for 90 tics. */
void ifCommonAnnounceTimeUpInitInterface(void)
{
    ifCommonBattleSetInterface(ifCommonBonusInterfaceProcUpdate, ifCommonBattleInterfaceProcSet, nSYAudioVoiceAnnounceTimeUp, 90);
    ifCommonAnnounceTimeUpMakeInterface();
}

/* ifcommon.c:3299-3312 ifCommon1PGameInterfaceProcSet 0x80114C20,
 * verbatim: what a 1P match freezes into instead of
 * ifCommonBattleInterfaceProcSet above. Three differences, all of them
 * the ending's: the wallpaper starts moving again, the camera pulls out
 * to sc1PGameSetCameraZoom's frame over 45 tics rather than snapping
 * back in 3, and the magnifier is put away.
 *
 * DIVERGES: none of the calls. grWallpaperResumeProcessAll is in
 * src/dc/stage.c, which is where this port keeps gr/grwallpaper.c. */
void ifCommon1PGameInterfaceProcSet(void)
{
    gcFuncGObjByLink(nGCCommonLinkIDCamera, ifCommonBattleInterfaceResumeGObj, 0);
    grWallpaperResumeProcessAll();
    ifCommonInterfaceSetGObjFlagsAll(GOBJ_FLAG_HIDDEN);

    gSCManagerBattleState->game_status = nSCBattleGameStatusSet;

    sIFCommonBattlePauseCameraRestoreWait = 45;

    sc1PGameSetCameraZoom();

    gIFCommonPlayerInterface.is_magnify_display = FALSE;
}

/* ifcommon.c:3314-3336 ifCommonAnnounceEndMessage 0x80114C80, verbatim
 * the match is over, say so.
 *
 * Three arms, and which one runs is the whole point. A bonus stage --
 * every gkind at or past nGRKindBonusStageStart -- gets FAILURE, because
 * the only way a bonus stage reaches here is the player falling off it
 * (ft/ftcommon/ftcommondead.c:88-91 calls this under
 * SCBATTLE_GAMERULE_BONUS). A 1P match the player still has stocks in
 * gets GAME SET over the 1P proc_set above. Everything else, including
 * a 1P match the player is out of stocks in, gets the VS one. */
void ifCommonAnnounceEndMessage(void)
{
    if (gSCManagerBattleState->gkind >= nGRKindBonusStageStart)
    {
        ifCommonBattleSetInterface(ifCommonBattleInterfaceProcUpdate, ifCommonBattleInterfaceProcSet, nSYAudioVoiceAnnounceFailure, 90);
        ifCommonAnnounceFailureMakeInterface();
    }
    else
    {
        if ((gSCManagerBattleState->game_type == nSCBattleGameType1PGame) && (gSCManagerBattleState->players[gSCManagerSceneData.player].stock_count != -1))
        {
            ifCommonBattleSetInterface(ifCommonBattleInterfaceProcUpdate, ifCommon1PGameInterfaceProcSet, nSYAudioVoiceAnnounceGameSet, 90);
        }
        else ifCommonBattleSetInterface(ifCommonBattleInterfaceProcUpdate, ifCommonBattleInterfaceProcSet, nSYAudioVoiceAnnounceGameSet, 90);

        ifCommonAnnounceGameSetMakeInterface();
    }
}

/* The bzero arm of syDmaLoadOverlay for overlay 2, of which this
 * file is a part: sc/scmanager.c calls it on the way into every
 * scene that loads that overlay. src/dc/overlay.h says why the port
 * needs it written out. */
void ifCommonOverlayLoad(void)
{
    OVERLAY_CLEAR(dIFCommonPlayerTagEnvColorsR);
    OVERLAY_CLEAR(dIFCommonPlayerTagEnvColorsG);
    OVERLAY_CLEAR(dIFCommonPlayerTagEnvColorsB);
    OVERLAY_CLEAR(dIFCommonPlayerScorePositionOffsetsX);
    OVERLAY_CLEAR(gIFCommonPlayerInterface);
    OVERLAY_CLEAR(sIFCommonPlayerDamageInterface);
    OVERLAY_CLEAR(sIFCommonPlayerStocksNum);
    OVERLAY_CLEAR(sIFCommonPlayerStocksGObj);
    OVERLAY_CLEAR(sIFCommonPlayerStealInterface);
    OVERLAY_CLEAR(sIFCommonPlayerMagnifySoundWait);
#ifndef FT_HOSTTEST
    /* Before the clear, as the spotlight's (src/dc/mnplayersvs.c): the
     * release needs the pointers the clear is about to drop, and the two
     * instances hold nothing of their own -- fighter_clone shares the
     * pack's blob and never owns it. The game gets the file back the
     * same way, by the scene heap going with the scene. */
    fighter_release(&sIFCommonPlayerArrowsPack);
    fighter_release(&sIFCommonPlayerMagnifyPack);
#endif
    OVERLAY_CLEAR(sIFCommonPlayerArrowsPack);
    OVERLAY_CLEAR(sIFCommonPlayerArrowsModels);
    OVERLAY_CLEAR(sIFCommonPlayerArrowsAnimJoint);
    OVERLAY_CLEAR(sIFCommonPlayerArrowsIsLoaded);
    /* The item arrow. The decomp has no counterpart to
     * clear: it loads IFCommonItem into the scene heap and the pointer
     * dies with the scene, while here the file is a bank whose records
     * sprite_bank_release_all drains (src/dc/sprite.c) -- so these are
     * the same two clears gmcommon.c makes of its own eight. Without
     * them the next scene's first MakeInterface would hand
     * lbCommonMakeSObjForGObj a Sprite in memory that scene no longer
     * owns. overlay_check caught exactly that. */
    OVERLAY_CLEAR(sIFCommonItemArrowSprite);
    OVERLAY_CLEAR(sIFCommonItemBank);
    OVERLAY_CLEAR(sIFCommonPlayerMagnifyInterface);
    OVERLAY_CLEAR(sIFCommonPlayerMagnifyPack);
    OVERLAY_CLEAR(sIFCommonPlayerMagnifyModels);
    OVERLAY_CLEAR(sIFCommonPlayerMagnifyIsLoaded);
    OVERLAY_CLEAR(sIFCommonBattlePausePlayer);
    OVERLAY_CLEAR(sIFCommonBattlePausePlayerDetail);
    OVERLAY_CLEAR(sIFCommonBattlePauseCameraEyeXOrigin);
    OVERLAY_CLEAR(sIFCommonBattlePauseCameraEyeYOrigin);
    OVERLAY_CLEAR(sIFCommonBattlePauseKindInterface);
    OVERLAY_CLEAR(sIFCommonBattlePauseCameraRestoreWait);
    OVERLAY_CLEAR(sIFCommonBattlePlace);
    OVERLAY_CLEAR(sIFCommonTimerIsStarted);
    OVERLAY_CLEAR(sIFCommonTimerDigitsInterface);
    OVERLAY_CLEAR(sIFCommonTimerLimit);
    OVERLAY_CLEAR(sIFCommonTimerStamp);
    OVERLAY_CLEAR(sIFCommonIsAnnouncedSecond);
    OVERLAY_CLEAR(sIFCommonBattleEndSoundsQueue);
    OVERLAY_CLEAR(sIFCommonBattleEndSoundsNum);
    OVERLAY_CLEAR(dIFCommonBattleBossUpdateInterval);
    OVERLAY_CLEAR(dIFCommonBattleBossUpdateWait);
    OVERLAY_CLEAR(sIFCommonBattleInterfaceProcUpdate);
    OVERLAY_CLEAR(sIFCommonBattleInterfaceProcSet);
}
