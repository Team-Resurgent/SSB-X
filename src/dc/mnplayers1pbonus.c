/* mnplayers1pbonus.c -- see mnplayers1pbonus.h. Every function is
 * mn/mnplayers/mnplayers1pbonus.c's by name and body unless marked
 * DIVERGES; the line numbers are the decomp's. */
#include "mnplayers1pbonus.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"
#include "ftcommon.h"
#include "bgm.h"
#include "fighter.h"
#include "objmodel.h"
#include "efmanager.h"

#include <sys/debug.h>
#include <sys/controller.h>
#include <sys/rdp.h>
#include <sys/utils.h>
#include <sys/objdisplay.h>
#include <sc/scdef.h>
#include <sc/scsubsys/scsubsys.h>
#include <gm/gmsound.h>
#include <mn/mndef.h>
#include <mn/mntypes.h>
#include <if/ifcommon.h>
#include <ef/efparticle.h>
#include <lb/lbdef.h>
#include <PR/os.h>

/* n_env.c's start-a-voice-script and stop-a-sound, which no decomp
 * header declares; src/dc/sysshim.c defines them over the FGM engine
 * (the third of them, func_800266A0_272A0, is in ftcommon.h) */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);
void func_80026738_27338(alSoundEffect *sfx);

/* ft/ftparam.h:2642,2648 (src/dc/ftparam.c); the header itself wants
 * the effect types behind it, which the port has no header for */
s32 ftParamGetCostumeCommonID(s32 fkind, s32 color);
s32 ftParamGetCostumeTeamID(s32 fkind, s32 color);
void ftParamInitAllParts(GObj *fighter_gobj, s32 costume, s32 shade);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset, exactly as the other three selects do it -- this is the
 * FOURTH of them, and 82 of its 100 named functions are
 * src/dc/mnplayers1pgame.c's with a different prefix. Each
 * `&llXxxSprite` and `&llXxxLUT` is written `llXxxSprite`, the number;
 * a palette comes back through sprite_bank_lut. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* the decomp's share of unused locals, as the other three selects' */
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wreturn-type"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

/* The eleven files, as src/dc/decomp/reloc_data.us.h reads them off the
 * ROM. They are the 1P GAME select's eleven, in the same order and to
 * the entry -- the two screens want exactly the same art -- so every
 * bank here already ships for src/dc/mnplayers1pgame.c and this step
 * adds no asset of its own. */
#define MNPLAYERS1PBONUS_BANK_COMMON     "mnplayers.spr"
#define MNPLAYERS1PBONUS_BANK_EMBLEMS    "ftemblems.spr"
#define MNPLAYERS1PBONUS_BANK_SELECT     "mnselect.spr"
#define MNPLAYERS1PBONUS_BANK_GAMEMODES  "mngamemodes.spr"
#define MNPLAYERS1PBONUS_BANK_PORTRAITS  "mnportraits.spr"
#define MNPLAYERS1PBONUS_BANK_1PMODE     "mn1pselect.spr"
#define MNPLAYERS1PBONUS_BANK_DIFFICULTY "mn1pdiff.spr"
#define MNPLAYERS1PBONUS_BANK_STOCKS     "ftstockszako.spr"
#define MNPLAYERS1PBONUS_BANK_FONTS      "mnfonts.spr"
#define MNPLAYERS1PBONUS_BANK_DIGITS     "ifdigits.spr"
#define MNPLAYERS1PBONUS_MODEL_SPOTLIGHT "spotlite.mdl"

#define llFTEmblemSpritesDonkeySprite                  0x00c78
#define llFTEmblemSpritesFZeroSprite                   0x032b8
#define llFTEmblemSpritesFoxSprite                     0x01938
#define llFTEmblemSpritesKirbySprite                   0x01f98
#define llFTEmblemSpritesMarioSprite                   0x00618
#define llFTEmblemSpritesMetroidSprite                 0x012d8
#define llFTEmblemSpritesMotherSprite                  0x03f78
#define llFTEmblemSpritesPMonstersSprite               0x03918
#define llFTEmblemSpritesYoshiSprite                   0x02c58
#define llFTEmblemSpritesZeldaSprite                   0x025f8
#define llIFCommonDigits0Sprite                        0x00068
#define llIFCommonDigits1Sprite                        0x00118
#define llIFCommonDigits2Sprite                        0x001c8
#define llIFCommonDigits3Sprite                        0x00278
#define llIFCommonDigits4Sprite                        0x00328
#define llIFCommonDigits5Sprite                        0x003d8
#define llIFCommonDigits6Sprite                        0x00488
#define llIFCommonDigits7Sprite                        0x00538
#define llIFCommonDigits8Sprite                        0x005e8
#define llIFCommonDigits9Sprite                        0x00698
#define llMNCommonFontsLetterASprite                   0x00040
#define llMNCommonFontsLetterBSprite                   0x000d0
#define llMNCommonFontsLetterCSprite                   0x00160
#define llMNCommonFontsLetterDSprite                   0x001f0
#define llMNCommonFontsLetterESprite                   0x00280
#define llMNCommonFontsLetterFSprite                   0x00310
#define llMNCommonFontsLetterGSprite                   0x003a0
#define llMNCommonFontsLetterHSprite                   0x00430
#define llMNCommonFontsLetterISprite                   0x004c0
#define llMNCommonFontsLetterJSprite                   0x00550
#define llMNCommonFontsLetterKSprite                   0x005e0
#define llMNCommonFontsLetterLSprite                   0x00670
#define llMNCommonFontsLetterMSprite                   0x00700
#define llMNCommonFontsLetterNSprite                   0x00790
#define llMNCommonFontsLetterOSprite                   0x00820
#define llMNCommonFontsLetterPSprite                   0x008b0
#define llMNCommonFontsLetterQSprite                   0x00940
#define llMNCommonFontsLetterRSprite                   0x009d0
#define llMNCommonFontsLetterSSprite                   0x00a60
#define llMNCommonFontsLetterTSprite                   0x00af0
#define llMNCommonFontsLetterUSprite                   0x00b80
#define llMNCommonFontsLetterVSprite                   0x00c10
#define llMNCommonFontsLetterWSprite                   0x00ca0
#define llMNCommonFontsLetterXSprite                   0x00d30
#define llMNCommonFontsLetterYSprite                   0x00dc0
#define llMNCommonFontsLetterZSprite                   0x00e50
#define llMNCommonFontsSymbolApostropheSprite          0x00ed0
#define llMNCommonFontsSymbolPercentSprite             0x00f60
#define llMNCommonFontsSymbolPeriodSprite              0x00fd0
#define llMNPlayers1PModeBestTimeTextSprite            0x12e0
#define llMNPlayers1PModeCSecSprite                    0x1fc8
#define llMNPlayers1PModePlatformsTextSprite           0x1898
#define llMNPlayers1PModeRedCardSprite                 0x032a8
#define llMNPlayers1PModeSecSprite                     0x1f48
#define llMNPlayers1PModeTargetsTextSprite             0x1658
#define llMNPlayers1PModeTotalBestTimeTextSprite       0x1410
#define llMNPlayersCommon0DarkSprite                   0x05388
#define llMNPlayersCommon1DarkSprite                   0x05440
#define llMNPlayersCommon1PPuckSprite                  0x09048
#define llMNPlayersCommon1PTextGradientSprite          0x08268
#define llMNPlayersCommon1PTextSprite                  0x00878
#define llMNPlayersCommon2DarkSprite                   0x05558
#define llMNPlayersCommon2PPuckSprite                  0x09b28
#define llMNPlayersCommon2PTextGradientSprite          0x08368
#define llMNPlayersCommon2PTextSprite                  0x00a58
#define llMNPlayersCommon3DarkSprite                   0x05668
#define llMNPlayersCommon3PPuckSprite                  0x0a608
#define llMNPlayersCommon3PTextGradientSprite          0x08468
#define llMNPlayersCommon3PTextSprite                  0x00c38
#define llMNPlayersCommon4DarkSprite                   0x05778
#define llMNPlayersCommon4PPuckSprite                  0x0b0e8
#define llMNPlayersCommon4PTextGradientSprite          0x08568
#define llMNPlayersCommon4PTextSprite                  0x00e18
#define llMNPlayersCommon5DarkSprite                   0x05888
#define llMNPlayersCommon6DarkSprite                   0x05998
#define llMNPlayersCommon7DarkSprite                   0x05aa8
#define llMNPlayersCommon8DarkSprite                   0x05bb8
#define llMNPlayersCommon9DarkSprite                   0x05cc8
#define llMNPlayersCommonBackButtonSprite              0x115c8
#define llMNPlayersCommonButtonTextSprite              0x01428
#define llMNPlayersCommonCPPuckSprite                  0x0bbc8
#define llMNPlayersCommonCaptainFalconTextSprite       0x03998
#define llMNPlayersCommonCursorHandGrabSprite          0x076e8
#define llMNPlayersCommonCursorHandHoverSprite         0x08168
#define llMNPlayersCommonCursorHandPointSprite         0x06f88
#define llMNPlayersCommonDKTextSprite                  0x01ff8
#define llMNPlayersCommonFoxTextSprite                 0x025b8
#define llMNPlayersCommonGateMan1PLUT                  0x103f8
#define llMNPlayersCommonGateMan2PLUT                  0x10420
#define llMNPlayersCommonGateMan3PLUT                  0x10470
#define llMNPlayersCommonGateMan4PLUT                  0x10448
#define llMNPlayersCommonJigglypuffTextSprite          0x03db8
#define llMNPlayersCommonKirbyTextSprite               0x028e8
#define llMNPlayersCommonLinkTextSprite                0x02ba0
#define llMNPlayersCommonLuigiTextSprite               0x01b18
#define llMNPlayersCommonMarioTextSprite               0x01838
#define llMNPlayersCommonNessTextSprite                0x035b0
#define llMNPlayersCommonPikachuTextSprite             0x032f8
#define llMNPlayersCommonPressTextSprite               0x014d8
#define llMNPlayersCommonPushTextSprite                0x012c8
#define llMNPlayersCommonReadyBannerSprite             0x0f530
#define llMNPlayersCommonReadyToFightTextSprite        0x0f448
#define llMNPlayersCommonSamusTextSprite               0x02358
#define llMNPlayersCommonStartTextSprite               0x01378
#define llMNPlayersCommonYoshiTextSprite               0x02ed8
#define llMNPlayersGameModesBonus1BreakTheTargetsTextSprite 0xbd8
#define llMNPlayersGameModesBonus2BoardThePlatformsTextSprite 0x1058
#define llMNPlayersPortraitsCaptainShadowSprite        0x1e2e8
#define llMNPlayersPortraitsCaptainSprite              0x19e48
#define llMNPlayersPortraitsCrossSprite                0x002b8
#define llMNPlayersPortraitsDonkeySprite               0x08bc8
#define llMNPlayersPortraitsFoxSprite                  0x0d068
#define llMNPlayersPortraitsKirbySprite                0x0f2b8
#define llMNPlayersPortraitsLinkSprite                 0x11508
#define llMNPlayersPortraitsLuigiShadowSprite          0x20538
#define llMNPlayersPortraitsLuigiSprite                0x06978
#define llMNPlayersPortraitsMarioSprite                0x04728
#define llMNPlayersPortraitsNessShadowSprite           0x22788
#define llMNPlayersPortraitsNessSprite                 0x17bf8
#define llMNPlayersPortraitsPikachuSprite              0x159a8
#define llMNPlayersPortraitsPortraitFireBgSprite       0x024d0
#define llMNPlayersPortraitsPortraitQuestionMarkSprite 0x00f68
#define llMNPlayersPortraitsPurinShadowSprite          0x249d8
#define llMNPlayersPortraitsPurinSprite                0x1c098
#define llMNPlayersPortraitsSamusSprite                0x0ae18
#define llMNPlayersPortraitsWhiteSquareSprite          0x006f0
#define llMNPlayersPortraitsYoshiSprite                0x13758
#define llMNSelectCommonStoneBackgroundSprite          0x00440
#define llRelocFileCount                               2132


// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

// 0x80136F50
/* DIVERGES, as all three of the other selects': the game's entries are
 * the link labels, whose value is the reloc file's index; the port
 * writes the numbers, which is what those labels are. */
u32 dMNPlayers1PBonusFileIDs[/* */] =
{
	17,	// MNPlayersCommon
	20,	// FTEmblemSprites
	21,	// MNSelectCommon
	18,	// MNPlayersGameModes
	19,	// MNPlayersPortraits
	23,	// MNPlayers1PMode
	24,	// MNPlayersDifficulty
	25,	// FTStocksZako
	33,	// MNCommonFonts
	36,	// IFCommonDigits
	22	// MNPlayersSpotlight
};

/* one SpriteBank per file, the eleventh (the spotlight) a baked pack */
static SpriteBank sMNPlayers1PBonusBanks[ARRAY_COUNT(dMNPlayers1PBonusFileIDs)];
static Fighter sMNPlayers1PBonusSpotlightPack;
static Fighter sMNPlayers1PBonusSpotlightModel;

/* CUT: dMNPlayers1PBonusLights11 and ...Lights12
 * (mnplayers1pbonus.c:32-36), the two reflected-light sets the
 * pre-render below emits. They are the 1P game select's own two, to
 * the byte. */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x80137640
s32 sMNPlayers1PBonusPad0x80137640[2];

// 0x80137648
MNPlayersSlotBonus sMNPlayers1PBonusSlot;

// 0x801376D0
s32 sMNPlayers1PBonusPad0x801376D0;

// 0x801376D4 - set but never used
s32 sMNPlayers1PBonusUnknown0x801376D4;

// 0x801376D8
s32 sMNPlayers1PBonusDevicesConnected[GMCOMMON_PLAYERS_MAX];

// 0x801376E8 - timer after selecting character before auto-starting
s32 sMNPlayers1PBonusStartWait;

// 0x801376EC
sb32 sMNPlayers1PBonusIsSelected;

// 0x801376F0
sb32 sMNPlayers1PBonusIsTeamBattle;

// 0x801376F4
s32 sMNPlayers1PBonusGameRules;

// 0x801376F8
s32 sMNPlayers1PBonusManPlayer;

// 0x801376FC
GObj *sMNPlayers1PBonusHiScoreGObj;

// 0x80137700
s32 sMNPlayers1PBonusPad0x80137700[4];

// 0x80137710
void *sMNPlayers1PBonusFigatreeHeap;

// 0x80137714 - 0 = Break the Targets, 1 = Board the Platforms
s32 sMNPlayers1PBonusBonusKind;

// 0x80137718 - title and back button
GObj *sMNPlayers1PBonusGameModeGObj;

// 0x8013771C - total time highscore
GObj *sMNPlayers1PBonusTotalTimeGObj;

// 0x80137720 - flag indicating which unlockable fighters are available
u16 sMNPlayers1PBonusFighterMask;

// 0x80137724 - frames elapsed
s32 sMNPlayers1PBonusTotalTimeTics;

// 0x80137728 - frames to wait until exiting
s32 sMNPlayers1PBonusReturnTic;

// 0x8013772C - looping timer that helps determine blink rate of Press Start (and Ready to Fight?)
s32 sMNPlayers1PBonusReadyBlinkWait;

// 0x80137730
s32 sMNPlayers1PBonusPad0x80137730[180];

/* CUT: mnplayers1pbonus.c:112-115, the reloc loader's two status
 * buffers -- the table of which ROM files are resident.
 * mnPlayers1PBonusLoadFiles, their only reader, says why. */

// 0x80137DF8
void *sMNPlayers1PBonusFiles[ARRAY_COUNT(dMNPlayers1PBonusFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* CUT: mnplayers1pbonus.c:123-127 mnPlayers1PBonusFuncLights, the
 * pre-render, as on all three of the other selects: it emits a
 * geometry-mode word and the reflected fighter lights into a display
 * list, and the port's lighting is the PVR back end's
 * (src/dc/objdisplay.c). dMNPlayers1PBonusTaskmanSetup carries NULL.
 * dMNPlayers1PBonusLights11/12 above go with it. */

// 0x80131B58
s32 mnPlayers1PBonusGetPowerOf(s32 base, s32 exp)
{
	s32 raised = base;
	s32 i;

	if (exp == 0)
	{
		return 1;
	}
	i = exp;

	while (i > 1)
	{
		i--;
		raised *= base;
	}
	return raised;
}

// 0x80131BF8
void mnPlayers1PBonusSetDigitColors(SObj *sobj, u32 *colors)
{
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->envcolor.r = colors[0];
	sobj->envcolor.g = colors[1];
	sobj->envcolor.b = colors[2];
	sobj->sprite.red = colors[3];
	sobj->sprite.green = colors[4];
	sobj->sprite.blue = colors[5];
}

// 0x80131C40
s32 mnPlayers1PBonusGetNumberDigitCount(s32 num, s32 digit_count_max)
{
    s32 digit_count_curr = digit_count_max;

    while (digit_count_curr > 0)
    {
        s32 digit = (mnPlayers1PBonusGetPowerOf(10, digit_count_curr - 1) != 0) ? num / mnPlayers1PBonusGetPowerOf(10, digit_count_curr - 1) : 0;

        if (digit != 0)
        {
            return digit_count_curr;
        }
        else digit_count_curr--;
    }
    return 0;
}

// 0x80131CEC
void mnPlayers1PBonusMakeNumber(GObj *gobj, s32 number, f32 x, f32 y, u32 *colors, s32 digit_count_max, sb32 is_fixed_digit_count)
{
	intptr_t offsets[/* */] =
	{
		llIFCommonDigits0Sprite, llIFCommonDigits1Sprite,
		llIFCommonDigits2Sprite, llIFCommonDigits3Sprite,
		llIFCommonDigits4Sprite, llIFCommonDigits5Sprite,
		llIFCommonDigits6Sprite, llIFCommonDigits7Sprite,
		llIFCommonDigits8Sprite, llIFCommonDigits9Sprite
	};
	SObj *sobj;
	f32 left_x = x;
	s32 i;
	s32 unused;
	s32 digit;

	if (number < 0)
	{
		number = 0;
	}
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[9], offsets[number % 10]));
	mnPlayers1PBonusSetDigitColors(sobj, colors);
	left_x -= 8;
	sobj->pos.x = left_x;
	sobj->pos.y = y;

	for (i = 1; i < ((is_fixed_digit_count != FALSE) ? digit_count_max : mnPlayers1PBonusGetNumberDigitCount(number, digit_count_max)); i++)
	{
		digit = (mnPlayers1PBonusGetPowerOf(10, i) != 0) ? number / mnPlayers1PBonusGetPowerOf(10, i) : 0;

		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[9], offsets[digit % 10]));
		mnPlayers1PBonusSetDigitColors(sobj, colors);
		left_x -= 8;
		sobj->pos.x = left_x;
		sobj->pos.y = y;
	}
}

// 0x80136FD8
intptr_t dMNPlayers1PBonusFontOffsets[/* */] =
{
	llMNCommonFontsLetterASprite, llMNCommonFontsLetterBSprite,
	llMNCommonFontsLetterCSprite, llMNCommonFontsLetterDSprite,
	llMNCommonFontsLetterESprite, llMNCommonFontsLetterFSprite,
	llMNCommonFontsLetterGSprite, llMNCommonFontsLetterHSprite,
	llMNCommonFontsLetterISprite, llMNCommonFontsLetterJSprite,
	llMNCommonFontsLetterKSprite, llMNCommonFontsLetterLSprite,
	llMNCommonFontsLetterMSprite, llMNCommonFontsLetterNSprite,
	llMNCommonFontsLetterOSprite, llMNCommonFontsLetterPSprite,
	llMNCommonFontsLetterQSprite, llMNCommonFontsLetterRSprite,
	llMNCommonFontsLetterSSprite, llMNCommonFontsLetterTSprite,
	llMNCommonFontsLetterUSprite, llMNCommonFontsLetterVSprite,
	llMNCommonFontsLetterWSprite, llMNCommonFontsLetterXSprite,
	llMNCommonFontsLetterYSprite, llMNCommonFontsLetterZSprite,

	llMNCommonFontsSymbolApostropheSprite,
	llMNCommonFontsSymbolPercentSprite,
	llMNCommonFontsSymbolPeriodSprite
};

// 0x8013704C
f32 dMNPlayers1PBonusFontWidths[/* */] =
{
	5.0F, 4.0F, 4.0F, 4.0F, 4.0F, 4.0F, 4.0F, 4.0F, 3.0F, 4.0F, 4.0F, 4.0F, 5.0F, 5.0F, 4.0F,
	4.0F, 5.0F, 4.0F, 4.0F, 5.0F, 4.0F, 5.0F, 5.0F, 5.0F, 5.0F, 4.0F, 2.0F, 7.0F, 3.0F
};

// 0x80131F5C - Unused?
void func_ovl29_80131F5C(void)
{
	return;
}

// 0x80131F64 - Unused?
void func_ovl29_80131F64(void)
{
	return;
}

// 0x80131F6C - Unused?
void func_ovl29_80131F6C(void)
{
	return;
}

// 0x80131F74
void mnPlayers1PBonusSelectFighterPuck(s32 player, s32 select_button)
{
	s32 held_player = sMNPlayers1PBonusSlot.held_player;
	s32 costume = ftParamGetCostumeCommonID(sMNPlayers1PBonusSlot.fkind, select_button);

	ftParamInitAllParts(sMNPlayers1PBonusSlot.player, costume, 0);

	sMNPlayers1PBonusSlot.costume = costume;
	sMNPlayers1PBonusSlot.is_selected = TRUE;
	sMNPlayers1PBonusSlot.holder_player = GMCOMMON_PLAYERS_MAX;
	sMNPlayers1PBonusSlot.cursor_status = nMNPlayersCursorStatusHover;

	mnPlayers1PBonusUpdateCursor(sMNPlayers1PBonusSlot.cursor, player, sMNPlayers1PBonusSlot.cursor_status);

	sMNPlayers1PBonusSlot.held_player = -1;
	sMNPlayers1PBonusSlot.is_fighter_selected = TRUE;

	mnPlayers1PBonusUpdateCursorPlacementPriorities(held_player);
	mnPlayers1PBonusAnnounceFighter(player, held_player);
	mnPlayers1PBonusMakePortraitFlash(held_player);

	sMNPlayers1PBonusStartWait = 140;
	sMNPlayers1PBonusIsSelected = TRUE;
}

// 0x80132030
f32 mnPlayers1PBonusGetNextPortraitX(s32 portrait, f32 current_pos_x)
{
	f32 portrait_pos_x[/* */] =
	{
		25.0F, 70.0F, 115.0F, 160.0F, 205.0F, 250.0F,
		25.0F, 70.0F, 115.0F, 160.0F, 205.0F, 250.0F
	};

	f32	portrait_vel[/* */] =
	{
		1.9F, 3.9F, 7.8F, -7.8F, -3.8F, -1.8F,
		1.8F, 3.8F, 7.8F, -7.8F, -3.8F, -1.8F
	};

	if (current_pos_x == portrait_pos_x[portrait])
	{
		return -1.0F;
	}
	else if (portrait_pos_x[portrait] < current_pos_x)
	{
		return ((current_pos_x + portrait_vel[portrait]) <= portrait_pos_x[portrait]) ? 
		portrait_pos_x[portrait] :
		current_pos_x + portrait_vel[portrait];
	}
	else return (current_pos_x + portrait_vel[portrait]) >= portrait_pos_x[portrait] ?
	portrait_pos_x[portrait] :
	current_pos_x + portrait_vel[portrait];
}

// 0x80132144 - bruh
sb32 mnPlayers1PBonusCheckFighterCrossed(s32 fkind)
{
	return FALSE;
}

// 0x80132150
void mnPlayers1PBonusPortraitProcUpdate(GObj *gobj)
{
	f32 new_pos_x = mnPlayers1PBonusGetNextPortraitX(gobj->user_data.s, SObjGetStruct(gobj)->pos.x);

	if (new_pos_x != -1.0F)
	{
		SObjGetStruct(gobj)->pos.x = new_pos_x;

		if (SObjGetStruct(gobj)->next != NULL)
		{
			SObjGetStruct(gobj)->next->pos.x = SObjGetStruct(gobj)->pos.x + 4.0F;
		}
	}
}

// 0x801321CC
void mnPlayers1PBonusSetPortraitWallpaperPosition(SObj *sobj, s32 portrait)
{
	Vec2f pos[/* */] =
	{
		{ -35.0F, 36.0F }, { -35.0F, 36.0F },
		{ -35.0F, 36.0F }, { 310.0F, 36.0F },
		{ 310.0F, 36.0F }, { 310.0F, 36.0F },
		{ -35.0F, 79.0F }, { -35.0F, 79.0F },
		{ -35.0F, 79.0F }, { 310.0F, 79.0F },
		{ 310.0F, 79.0F }, { 310.0F, 79.0F }
	};

	sobj->pos.x = pos[portrait].x;
	sobj->pos.y = pos[portrait].y;
}

// 0x80132228
void mnPlayers1PBonusPortraitAddCross(GObj *gobj, s32 portrait)
{
	SObj *sobj = SObjGetStruct(gobj);
	f32 x = sobj->pos.x;
	f32 y = sobj->pos.y;

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[4], llMNPlayersPortraitsCrossSprite));

	sobj->pos.x = x + 4.0F;
	sobj->pos.y = y + 12.0F;
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->sprite.red = 0xFF;
	sobj->sprite.green = 0x00;
	sobj->sprite.blue = 0x00;
}

// 0x801322BC
sb32 mnPlayers1PBonusCheckFighterLocked(s32 fkind)
{
	switch (fkind)
	{
	case nFTKindNess:
		return (sMNPlayers1PBonusFighterMask & (1 << nFTKindNess)) ? FALSE : TRUE;

	case nFTKindPurin:
		return (sMNPlayers1PBonusFighterMask & (1 << nFTKindPurin)) ? FALSE : TRUE;

	case nFTKindCaptain:
		return (sMNPlayers1PBonusFighterMask & (1 << nFTKindCaptain)) ? FALSE : TRUE;

	case nFTKindLuigi:
		return (sMNPlayers1PBonusFighterMask & (1 << nFTKindLuigi)) ? FALSE : TRUE;

	default:
		return FALSE;
	}
}

// 0x80137180
s32 dMNPlayers1PBonusUnkown0x80137180[/* */] =
{
	0xC55252C5,
	0xA6524294,
	0x595252C5,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000
};

// 0x80132388 - Unused?
void func_ovl29_80132388(void)
{
	return;
}

// 0x80132390
s32 mnPlayers1PBonusGetFighterKind(s32 portrait)
{
	s32 fkinds[/* */] =
	{
		nFTKindLuigi, nFTKindMario, nFTKindDonkey, nFTKindLink, nFTKindSamus,   nFTKindCaptain,
		nFTKindNess,  nFTKindYoshi, nFTKindKirby,  nFTKindFox,  nFTKindPikachu, nFTKindPurin
	};

	return fkinds[portrait];
}

// 0x801323E0
s32 mnPlayers1PBonusGetPortrait(s32 fkind)
{
	s32 portraits[/* */] =
	{
		1, 9, 2, 4, 0, 3,
		7, 5, 8, 10, 11, 6
	};

	return portraits[fkind];
}

// 0x80132430
void mnPlayers1PBonusPortraitProcDisplay(GObj *gobj)
{
	gDPPipeSync(gSYTaskmanDLHeads[0]++);
	gDPSetCycleType(gSYTaskmanDLHeads[0]++, G_CYC_1CYCLE);
	gDPSetPrimColor(gSYTaskmanDLHeads[0]++, 0, 0, 0x30, 0x30, 0x30, 0xFF);
	gDPSetCombineLERP(gSYTaskmanDLHeads[0]++, NOISE, TEXEL0, PRIMITIVE, TEXEL0, 0, 0, 0, TEXEL0, NOISE, TEXEL0, PRIMITIVE, TEXEL0,  0, 0, 0, TEXEL0);
	gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);

	lbCommonDrawSObjNoAttr(gobj);
}

// 0x801324F0
void mnPlayers1PBonusMakePortraitShadow(s32 portrait)
{
	GObj *gobj;
	SObj *sobj;
	intptr_t offsets[/* */] =
	{
		0x0, 									0x0,
		0x0, 									0x0,
		llMNPlayersPortraitsLuigiShadowSprite, 	0x0,
		0x0, 									llMNPlayersPortraitsCaptainShadowSprite,
		0x0, 									0x0,
		llMNPlayersPortraitsPurinShadowSprite, 	llMNPlayersPortraitsNessShadowSprite
	};

	gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, mnPlayers1PBonusPortraitProcUpdate, nGCProcessKindFunc, 1);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[4], llMNPlayersPortraitsPortraitFireBgSprite));
	sobj->pos.x = (((portrait >= 6) ? portrait - 6 : portrait) * 45) + 25;
	sobj->pos.y = (((portrait >= 6) ? 1 : 0) * 43) + 36;

	mnPlayers1PBonusSetPortraitWallpaperPosition(sobj, portrait);
	gobj->user_data.s = portrait;

	gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, mnPlayers1PBonusPortraitProcDisplay, 27, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, mnPlayers1PBonusPortraitProcUpdate, nGCProcessKindFunc, 1);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[4], offsets[mnPlayers1PBonusGetFighterKind(portrait)]));
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	gobj->user_data.s = portrait;
	mnPlayers1PBonusSetPortraitWallpaperPosition(sobj, portrait);

	gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, mnPlayers1PBonusPortraitProcUpdate, nGCProcessKindFunc, 1);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[4], llMNPlayersPortraitsPortraitQuestionMarkSprite));
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->envcolor.r = 0x5B;
	sobj->envcolor.g = 0x41;
	sobj->envcolor.b = 0x33;
	sobj->sprite.red = 0xC4;
	sobj->sprite.green = 0xB9;
	sobj->sprite.blue = 0xA9;

	gobj->user_data.s = portrait;
	mnPlayers1PBonusSetPortraitWallpaperPosition(sobj, portrait);
}

// 0x80132798
void mnPlayers1PBonusMakePortrait(s32 portrait)
{
	GObj *portrait_gobj, *wallpaper_gobj;
	SObj *sobj;
	intptr_t offsets[/* */] =
	{
		llMNPlayersPortraitsMarioSprite,   llMNPlayersPortraitsFoxSprite,
		llMNPlayersPortraitsDonkeySprite,  llMNPlayersPortraitsSamusSprite,
		llMNPlayersPortraitsLuigiSprite,   llMNPlayersPortraitsLinkSprite,
		llMNPlayersPortraitsYoshiSprite,   llMNPlayersPortraitsCaptainSprite,
		llMNPlayersPortraitsKirbySprite,   llMNPlayersPortraitsPikachuSprite,
		llMNPlayersPortraitsPurinSprite,   llMNPlayersPortraitsNessSprite
	};

	if (mnPlayers1PBonusCheckFighterLocked(mnPlayers1PBonusGetFighterKind(portrait)) != FALSE)
	{
		mnPlayers1PBonusMakePortraitShadow(portrait);
	}
	else
	{
		wallpaper_gobj = gcMakeGObjSPAfter(0, NULL, 25, GOBJ_PRIORITY_DEFAULT);
		gcAddGObjDisplay(wallpaper_gobj, lbCommonDrawSObjAttr, 32, GOBJ_PRIORITY_DEFAULT, ~0);
		wallpaper_gobj->user_data.s = portrait;
		gcAddGObjProcess(wallpaper_gobj, mnPlayers1PBonusPortraitProcUpdate, nGCProcessKindFunc, 1);

		sobj = lbCommonMakeSObjForGObj(wallpaper_gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[4], llMNPlayersPortraitsPortraitFireBgSprite));
		sobj->pos.x = (((portrait >= 6) ? portrait - 6 : portrait) * 45) + 25;
		sobj->pos.y = (((portrait >= 6) ? 1 : 0) * 43) + 36;

		mnPlayers1PBonusSetPortraitWallpaperPosition(sobj, portrait);

		portrait_gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);
		gcAddGObjDisplay(portrait_gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);
		gcAddGObjProcess(portrait_gobj, mnPlayers1PBonusPortraitProcUpdate, nGCProcessKindFunc, 1);

		sobj = lbCommonMakeSObjForGObj(portrait_gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[4], offsets[mnPlayers1PBonusGetFighterKind(portrait)]));
		sobj->sprite.attr &= ~SP_FASTCOPY;
		sobj->sprite.attr |= SP_TRANSPARENT;
		sobj->pos.x = (((portrait >= 6) ? portrait - 6 : portrait) * 45) + 25;
		sobj->pos.y = (((portrait >= 6) ? 1 : 0) * 43) + 36;
		portrait_gobj->user_data.s = portrait;

		if (mnPlayers1PBonusCheckFighterCrossed(mnPlayers1PBonusGetFighterKind(portrait)) != FALSE)
		{
			mnPlayers1PBonusPortraitAddCross(portrait_gobj, portrait);
		}
		mnPlayers1PBonusSetPortraitWallpaperPosition(sobj, portrait);
	}
}

// 0x80132A58
void mnPlayers1PBonusMakePortraitAll(void)
{
	s32 i;

	for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
	{
		mnPlayers1PBonusMakePortrait(i);
	}
}

// 0x80132A98
void mnPlayers1PBonusMakeNameAndEmblem(GObj *gobj, s32 player, s32 fkind)
{
	SObj *sobj;
	Vec2f pos[/* */] =
	{
		{ 13.0F, 28.0F }, {  6.0F, 25.0F },
		{  5.0F, 25.0F }, { 13.0F, 25.0F },
		{ 13.0F, 28.0F }, { 13.0F, 28.0F },
		{ 16.0F, 25.0F }, {  4.0F, 25.0F },
		{ 13.0F, 25.0F }, { 13.0F, 25.0F },
		{ 13.0F, 25.0F }, { 13.0F, 25.0F }
	};
	intptr_t emblem_offsets[/* */] =
	{
		llFTEmblemSpritesMarioSprite,     llFTEmblemSpritesFoxSprite,
		llFTEmblemSpritesDonkeySprite,    llFTEmblemSpritesMetroidSprite,
		llFTEmblemSpritesMarioSprite,     llFTEmblemSpritesZeldaSprite,
		llFTEmblemSpritesYoshiSprite,     llFTEmblemSpritesFZeroSprite,
		llFTEmblemSpritesKirbySprite,     llFTEmblemSpritesPMonstersSprite,
		llFTEmblemSpritesPMonstersSprite, llFTEmblemSpritesMotherSprite
	};
	intptr_t name_offsets[/* */] =
	{
		llMNPlayersCommonMarioTextSprite,      llMNPlayersCommonFoxTextSprite,
		llMNPlayersCommonDKTextSprite,         llMNPlayersCommonSamusTextSprite,
		llMNPlayersCommonLuigiTextSprite,      llMNPlayersCommonLinkTextSprite,
		llMNPlayersCommonYoshiTextSprite,      llMNPlayersCommonCaptainFalconTextSprite,
		llMNPlayersCommonKirbyTextSprite,      llMNPlayersCommonPikachuTextSprite,
		llMNPlayersCommonJigglypuffTextSprite, llMNPlayersCommonNessTextSprite
	};

	if (fkind != nFTKindNull)
	{
		gcRemoveSObjAll(gobj);

		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[1], emblem_offsets[fkind]));
		sobj->sprite.attr &= ~SP_FASTCOPY;
		sobj->sprite.attr |= SP_TRANSPARENT;
		sobj->sprite.red = 0x00;
		sobj->sprite.green = 0x00;
		sobj->sprite.blue = 0x00;
		sobj->pos.x = 68.0F;
		sobj->pos.y = 144.0F;

		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[0], name_offsets[fkind]));
		sobj->sprite.attr &= ~SP_FASTCOPY;
		sobj->sprite.attr |= SP_TRANSPARENT;
		sobj->pos.x = 66.0F;
		sobj->pos.y = 202.0F;
	}
}

// 0x80132C14
void mnPlayers1PBonusMakePortraitCamera(void)
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
			40,
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

// 0x80132CB4
void mnPlayers1PBonusMakePortraitWallpaperCamera(void)
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
			60,
			COBJ_MASK_DLLINK(32),
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

// 0x80132D54
void mnPlayers1PBonusMakePortraitFlashCamera(void)
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
			50,
			COBJ_MASK_DLLINK(33),
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

// 0x80132DF4
void mnPlayers1PBonusMakeGateCamera(void)
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
			COBJ_MASK_DLLINK(28),
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

// 0x80132E94
void mnPlayers1PBonusSetGateLUT(GObj *gobj, s32 player)
{
	SObj *sobj;

	intptr_t offsets[/* */] =
	{
		llMNPlayersCommonGateMan1PLUT, llMNPlayersCommonGateMan2PLUT,
		llMNPlayersCommonGateMan3PLUT, llMNPlayersCommonGateMan4PLUT
	};

	sobj = SObjGetStruct(gobj);
	/* DIVERGES as all three of the other selects': a palette is not a
	 * sprite in the bank, so it comes back through sprite_bank_lut
	 * rather than the lbRelocGetFileData the decomp reads it with. A
	 * LUT offset sent down the sprite path shows up on the serial log
	 * as "sprite: file 17 has no sprite at 0x103f8".
	 * And the palette is file 17's but the card is file 23's (82 wide);
	 * file 17's bank holds the palette baked on its own 66-wide VS
	 * card, so it is looked up in file 23's, which holds this card in
	 * it (the Makefile's mn1pselect.spr). */
	SObjGetSprite(sobj)->LUT = sprite_bank_lut((SpriteBank *)sMNPlayers1PBonusFiles[5], offsets[player]);
}

// 0x80132EEC
void mnPlayers1PBonusMakeGate(s32 player)
{
	GObj *gobj;
	SObj *sobj;

	intptr_t offsets[/* */] =
	{
		llMNPlayersCommon1PTextSprite, llMNPlayersCommon2PTextSprite,
		llMNPlayersCommon3PTextSprite, llMNPlayersCommon4PTextSprite
	};
	f32 pos_x[/* */] = { 8.0F, 5.0F, 5.0F, 5.0F };

	gobj = lbCommonMakeSpriteGObj
	(
		0,
		NULL,
		22,
		GOBJ_PRIORITY_DEFAULT,
		lbCommonDrawSObjAttr,
		28,
		GOBJ_PRIORITY_DEFAULT,
		~0,
		lbRelocGetFileData
		(
			Sprite*,
			sMNPlayers1PBonusFiles[5],
			llMNPlayers1PModeRedCardSprite
		),
		nGCProcessKindFunc,
		NULL,
		1
	);
	SObjGetStruct(gobj)->pos.x = 58.0F;
	SObjGetStruct(gobj)->pos.y = 127.0F;
	SObjGetStruct(gobj)->sprite.attr &= ~SP_FASTCOPY;
	SObjGetStruct(gobj)->sprite.attr |= SP_TRANSPARENT;

	sMNPlayers1PBonusSlot.panel = gobj;

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[0], offsets[player]));
	sobj->pos.x = pos_x[player] + 58.0F;
	sobj->pos.y = 132.0F;
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->sprite.red = 0x00;
	sobj->sprite.green = 0x00;
	sobj->sprite.blue = 0x00;

	mnPlayers1PBonusSetGateLUT(gobj, player);

	gobj = gcMakeGObjSPAfter(0, NULL, 22, GOBJ_PRIORITY_DEFAULT);
	sMNPlayers1PBonusSlot.name_emblem_gobj = gobj;
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 28, GOBJ_PRIORITY_DEFAULT, ~0);
}

// 0x80137354
intptr_t dMNPlayers1PBonusDigitOffsets[/* */] =
{
	llMNPlayersCommon0DarkSprite,
	llMNPlayersCommon1DarkSprite,
	llMNPlayersCommon2DarkSprite,
	llMNPlayersCommon3DarkSprite,
	llMNPlayersCommon4DarkSprite,
	llMNPlayersCommon5DarkSprite,
	llMNPlayersCommon6DarkSprite,
	llMNPlayersCommon7DarkSprite,
	llMNPlayersCommon8DarkSprite,
	llMNPlayersCommon9DarkSprite
};

// 0x80138A5C
f32 dMNPlayers1PBonusDigitWidths[/* */] =
{
	8.0F, 6.0F, 9.0F, 8.0F, 8.0F,
	9.0F, 8.0F, 8.0F, 8.0F, 9.0F
};

// 0x801330C4 - Unused?
void func_ovl29_801330C4(void)
{
	return;
}

// 0x801330CC
void mnPlayers1PBonusMakeWallpaper(void)
{
	GObj *gobj;
	SObj *sobj;

	gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 26, GOBJ_PRIORITY_DEFAULT, ~0);
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[2], llMNSelectCommonStoneBackgroundSprite));
	sobj->cms = G_TX_WRAP;
	sobj->cmt = G_TX_WRAP;
	sobj->masks = 6;
	sobj->maskt = 5;
	sobj->lrs = 300;
	sobj->lrt = 220;
	sobj->pos.x = 10.0F;
	sobj->pos.y = 10.0F;
}

// 0x80133170
void mnPlayers1PBonusMakeWallpaperCamera(void)
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
			80,
			COBJ_MASK_DLLINK(26),
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

// 0x80133210 - Unused?
void func_ovl29_80133210(void)
{
	return;
}

// 0x80133218
void mnPlayers1PBonusMakeLabels(void)
{
	GObj *gobj;
	SObj *sobj;

	sMNPlayers1PBonusGameModeGObj = gobj = gcMakeGObjSPAfter(0, NULL, 23, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 34, GOBJ_PRIORITY_DEFAULT, ~0);

	if (sMNPlayers1PBonusBonusKind == 0)
	{
		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[3], llMNPlayersGameModesBonus1BreakTheTargetsTextSprite));
	}
	else sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[3], llMNPlayersGameModesBonus2BoardThePlatformsTextSprite));
	
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->pos.x = 27.0F;
	sobj->pos.y = 24.0F;
	sobj->sprite.red = 0xE3;
	sobj->sprite.green = 0xAC;
	sobj->sprite.blue = 0x04;

	if (sMNPlayers1PBonusBonusKind == 0)
	{
		func_800269C0_275C0(nSYAudioVoiceAnnounceBreakTheTargets);
	}
	else func_800269C0_275C0(nSYAudioVoiceAnnounceBoardThePlatforms);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[0], llMNPlayersCommonBackButtonSprite));
	sobj->pos.x = 244.0F;
	sobj->pos.y = 23.0F;
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
}

// 0x80133370
void mnPlayers1PBonusMakeLabelsCamera(void)
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

// 0x80133410
u32 mnPlayers1PBonusGetBestTime(s32 fkind)
{
	if (sMNPlayers1PBonusBonusKind == 0)
	{
		u32 time = gSCManagerBackupData.spgame_records[fkind].bonus1_time;

		if (time > I_HRS_TO_TICS(1) - 1)
		{
			return I_HRS_TO_TICS(1) - 1;
		}
		else return time;
	}
	else
	{
		u32 time = gSCManagerBackupData.spgame_records[fkind].bonus2_time;

		if (time > I_HRS_TO_TICS(1) - 1)
		{
			return I_HRS_TO_TICS(1) - 1;
		}
		else return time;
	}
}

// 0x80133488
s32 mnPlayers1PBonusGetMins(s32 tics)
{
	return tics / TIME_MIN;
}

// 0x8013349C
s32 mnPlayers1PBonusGetSec(s32 tics)
{
	return (tics % TIME_MIN) / TIME_SEC;
}

// 0x801334C0
s32 mnPlayers1PBonusGetCSec(s32 tics)
{
	s32 seconds = tics % TIME_MIN;
	s32 tenths = (seconds % TIME_SEC) / 6 * 10;
	s32 hundredths = (seconds % 6) / 0.554F;

	return tenths + hundredths;
}

// 0x80133570
s32 mnPlayers1PBonusGetTotalMins(void)
{
	s32 i;
	s32 sum = 0;

	for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
	{
		if (mnPlayers1PBonusCheckFighterLocked(i) == FALSE)
		{
			sum += mnPlayers1PBonusGetMins(mnPlayers1PBonusGetBestTime(i));
		}
	}
	return sum;
}

// 0x801335DC
s32 mnPlayers1PBonusGetTotalSec(void)
{
	s32 i;
	s32 sum = 0;

	for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
	{
		if (mnPlayers1PBonusCheckFighterLocked(i) == FALSE)
		{
			sum += mnPlayers1PBonusGetSec(mnPlayers1PBonusGetBestTime(i));
		}
	}
	return sum;
}

// 0x80133648
s32 mnPlayers1PBonusGetTotalCSec(void)
{
	s32 i;
	s32 sum = 0;

	for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
	{
		if (mnPlayers1PBonusCheckFighterLocked(i) == FALSE)
		{
			sum += mnPlayers1PBonusGetCSec(mnPlayers1PBonusGetBestTime(i));
		}
	}
	return sum;
}

// 0x801336B4
void mnPlayers1PBonusMakeBestTime(void)
{
	GObj *gobj;
	SObj *sobj;
	s32 unused[2];
	u32 colors1[/* */] = { 0xC5, 0xB6, 0xA7 };
	u32 colors2[/* */] = { 0x00, 0x00, 0x00, 0x7E, 0x7C, 0x77 };
	u32 best_time;
	s32 fkind = mnPlayers1PBonusGetForcePuckFighterKind();

	if (sMNPlayers1PBonusHiScoreGObj != NULL)
	{
		gcEjectGObj(sMNPlayers1PBonusHiScoreGObj);
		sMNPlayers1PBonusHiScoreGObj = NULL;
	}
	if (fkind != nFTKindNull)
	{
		best_time = mnPlayers1PBonusGetBestTime(fkind);

		sMNPlayers1PBonusHiScoreGObj = gobj = gcMakeGObjSPAfter(0, NULL, 23, GOBJ_PRIORITY_DEFAULT);
		gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 26, GOBJ_PRIORITY_DEFAULT, ~0);

		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[5], llMNPlayers1PModeBestTimeTextSprite));
		sobj->pos.x = 177.0F;
		sobj->pos.y = 198.0F;
		sobj->sprite.attr &= ~SP_FASTCOPY;
		sobj->sprite.attr |= SP_TRANSPARENT;
		sobj->sprite.red = 0x7E;
		sobj->sprite.green = 0x7C;
		sobj->sprite.blue = 0x77;

		mnPlayers1PBonusMakeNumber(gobj, mnPlayers1PBonusGetMins(best_time), 237.0F, 195.0F, colors2, 2, TRUE);

		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[5], llMNPlayers1PModeSecSprite));
		sobj->pos.x = 239.0F;
		sobj->pos.y = 195.0F;
		sobj->sprite.attr &= ~SP_FASTCOPY;
		sobj->sprite.attr |= SP_TRANSPARENT;
		sobj->envcolor.r = 0x00;
		sobj->envcolor.g = 0x00;
		sobj->envcolor.b = 0x00;
		sobj->sprite.red = 0x7E;
		sobj->sprite.green = 0x7C;
		sobj->sprite.blue = 0x77;

		mnPlayers1PBonusMakeNumber(gobj, mnPlayers1PBonusGetSec(best_time), 259.0F, 195.0F, colors2, 2, TRUE);

		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[5], llMNPlayers1PModeCSecSprite));
		sobj->pos.x = 261.0F;
		sobj->pos.y = 195.0F;
		sobj->sprite.attr &= ~SP_FASTCOPY;
		sobj->sprite.attr |= SP_TRANSPARENT;
		sobj->envcolor.r = 0x00;
		sobj->envcolor.g = 0x00;
		sobj->envcolor.b = 0x00;
		sobj->sprite.red = 0x7E;
		sobj->sprite.green = 0x7C;
		sobj->sprite.blue = 0x77;

		mnPlayers1PBonusMakeNumber(gobj, mnPlayers1PBonusGetCSec(best_time), 283.0F, 195.0F, colors2, 2, TRUE);
	}
}

// 0x80133990
u8 mnPlayers1PBonusGetBestTaskCount(s32 fkind)
{
	if (sMNPlayers1PBonusBonusKind == 0)
	{
		return gSCManagerBackupData.spgame_records[fkind].bonus1_task_count;
	}
	else return gSCManagerBackupData.spgame_records[fkind].bonus2_task_count;
}

// 0x801339C8
void mnPlayers1PBonusMakeBestTaskCount(void)
{
	GObj *gobj;
	SObj *sobj;
	s32 unused[2];
	u32 colors1[/* */] = { 0xC5, 0xB6, 0xA7 };
	u32 colors2[/* */] = { 0x00, 0x00, 0x00, 0x7E, 0x7C, 0x77 };
	s32 fkind = mnPlayers1PBonusGetForcePuckFighterKind();

	if (sMNPlayers1PBonusHiScoreGObj != NULL)
	{
		gcEjectGObj(sMNPlayers1PBonusHiScoreGObj);
		sMNPlayers1PBonusHiScoreGObj = NULL;
	}
	if (fkind != nFTKindNull)
	{
		sMNPlayers1PBonusHiScoreGObj = gobj = gcMakeGObjSPAfter(0, NULL, 23, GOBJ_PRIORITY_DEFAULT);
		gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 26, GOBJ_PRIORITY_DEFAULT, ~0);

		if (sMNPlayers1PBonusBonusKind == 0)
		{
			sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[5], llMNPlayers1PModeTargetsTextSprite));
		}
		else sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[5], llMNPlayers1PModePlatformsTextSprite));
		
#if defined(REGION_US)
		sobj->pos.x = 235.0F;
#else
		sobj->pos.x = 187.0F;
#endif
		sobj->pos.y = 195.0F;
		sobj->sprite.attr &= ~SP_FASTCOPY;
		sobj->sprite.attr |= SP_TRANSPARENT;
		sobj->sprite.red = 0x7E;
		sobj->sprite.green = 0x7C;
		sobj->sprite.blue = 0x77;

#if defined(REGION_US)
		mnPlayers1PBonusMakeNumber(gobj, mnPlayers1PBonusGetBestTaskCount(fkind), 225.0F, 194.0F, colors2, 2, TRUE);
#else
		mnPlayers1PBonusMakeNumber(gobj, mnPlayers1PBonusGetBestTaskCount(fkind), 257.0F, 194.0F, colors2, 2, TRUE);
#endif
	}
}

// 0x80133B7C
sb32 mnPlayers1PBonusCheckBonusComplete(s32 fkind)
{
	u8 count;

	if (sMNPlayers1PBonusBonusKind == 0)
	{
		count = gSCManagerBackupData.spgame_records[fkind].bonus1_task_count;
	}
	else count = gSCManagerBackupData.spgame_records[fkind].bonus2_task_count;

	if (count == SCBATTLE_BONUSGAME_TASK_MAX)
	{
		return TRUE;
	}
	else return FALSE;
}

// 0x80133BCC
void mnPlayers1PBonusMakeHiScore(void)
{
	if (mnPlayers1PBonusCheckBonusComplete(mnPlayers1PBonusGetForcePuckFighterKind()))
	{
		mnPlayers1PBonusMakeBestTime();
	}
	else mnPlayers1PBonusMakeBestTaskCount();
}

// 0x80133C14 - Unused?
void func_ovl29_80133C14(void)
{
	return;
}

// 0x80133C1C
void mnPlayers1PBonusMakeTotalTime(void)
{
	GObj *gobj;
	SObj *sobj;
	s32 unused[2];
	u32 colors1[/* */] = { 0xC5, 0xB6, 0xA7 };
	u32 colors2[/* */] = { 0x00, 0x00, 0x00, 0x7E, 0x7C, 0x77 };
	s32 centiseconds;
	s32 remainder;
	s32 seconds;

	sMNPlayers1PBonusTotalTimeGObj = gobj = gcMakeGObjSPAfter(0, NULL, 23, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 26, GOBJ_PRIORITY_DEFAULT, ~0);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[5], llMNPlayers1PModeTotalBestTimeTextSprite));
	sobj->pos.x = 142.0F;
	sobj->pos.y = 209.0F;
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->sprite.red = 0x7E;
	sobj->sprite.green = 0x7C;
	sobj->sprite.blue = 0x77;

	centiseconds = mnPlayers1PBonusGetTotalCSec();
	remainder = centiseconds / 100;
	mnPlayers1PBonusMakeNumber(gobj, centiseconds % 100, 283.0F, 206.0F, colors2, 2, TRUE);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[5], llMNPlayers1PModeCSecSprite));
	sobj->pos.x = 261.0F;
	sobj->pos.y = 206.0F;
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->envcolor.r = 0x00;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;
	sobj->sprite.red = 0x7E;
	sobj->sprite.green = 0x7C;
	sobj->sprite.blue = 0x77;

	seconds = mnPlayers1PBonusGetTotalSec() + remainder;
	remainder = seconds / TIME_SEC;
	seconds %= TIME_SEC;
	mnPlayers1PBonusMakeNumber(gobj, seconds, 259.0F, 206.0F, colors2, 2, TRUE);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[5], llMNPlayers1PModeSecSprite));
	sobj->pos.x = 239.0F;
	sobj->pos.y = 206.0F;
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->envcolor.r = 0x00;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;
	sobj->sprite.red = 0x7E;
	sobj->sprite.green = 0x7C;
	sobj->sprite.blue = 0x77;

	mnPlayers1PBonusMakeNumber(gobj, mnPlayers1PBonusGetTotalMins() + remainder, 237.0F, 206.0F, colors2, 3, TRUE);
}

// 0x80133F4C - Unused?
void func_ovl29_80133F4C(void)
{
	return;
}

// 0x80133F54 - Unused?
void func_ovl29_80133F54(void)
{
	return;
}

// 0x80133F5C
s32 mnPlayers1PBonusGetCostume(s32 fkind, s32 select_button)
{
	ftParamGetCostumeCommonID(fkind, ftParamGetCostumeCommonID(fkind, select_button));
}

// 0x80133F88
s32 mnPlayers1PBonusGetStatusSelected(s32 fkind)
{
	switch (fkind)
	{
	case nFTKindFox:
	case nFTKindSamus:	
		return nFTDemoStatusWin4;

	case nFTKindDonkey:
	case nFTKindLuigi:
	case nFTKindLink:
	case nFTKindCaptain:
		return nFTDemoStatusWin1;

	case nFTKindYoshi:
	case nFTKindPurin:
	case nFTKindNess:
		return nFTDemoStatusWin2;
		
	case nFTKindMario:
	case nFTKindKirby:
		return nFTDemoStatusWin3;
		
	default:
		return nFTDemoStatusWin1;
	}
}

// 0x80133FE8
void mnPlayers1PBonusFighterProcUpdate(GObj *fighter_gobj)
{
	FTStruct* fp = ftGetStruct(fighter_gobj);
	s32 player = fp->player;

	if (sMNPlayers1PBonusSlot.is_fighter_selected == 1)
	{
		if (DObjGetStruct(fighter_gobj)->rotate.vec.f.y < F_CLC_DTOR32(0.1F))
		{
			if (sMNPlayers1PBonusSlot.is_status_selected == FALSE)
			{
				scSubsysFighterSetStatus(sMNPlayers1PBonusSlot.player, mnPlayers1PBonusGetStatusSelected(sMNPlayers1PBonusSlot.fkind));

				sMNPlayers1PBonusSlot.is_status_selected = TRUE;
			}
		}
		else
		{
			DObjGetStruct(fighter_gobj)->rotate.vec.f.y += F_CST_DTOR32(20.0F);

			if (DObjGetStruct(fighter_gobj)->rotate.vec.f.y > F_CLC_DTOR32(360.0F))
			{
				DObjGetStruct(fighter_gobj)->rotate.vec.f.y = 0.0F;

				scSubsysFighterSetStatus(sMNPlayers1PBonusSlot.player, mnPlayers1PBonusGetStatusSelected(sMNPlayers1PBonusSlot.fkind));

				sMNPlayers1PBonusSlot.is_status_selected = TRUE;
			}
		}
	}
	else
	{
		DObjGetStruct(fighter_gobj)->rotate.vec.f.y += F_CST_DTOR32(2.0F);

		if (DObjGetStruct(fighter_gobj)->rotate.vec.f.y > F_CST_DTOR32(360.0F))
		{
			DObjGetStruct(fighter_gobj)->rotate.vec.f.y -= F_CST_DTOR32(360.0F);
		}
	}
}

// 0x80134108
void mnPlayers1PBonusMakeFighter(GObj *fighter_gobj, s32 player, s32 fkind)
{
	f32 rot_y;
	FTDesc desc = dFTManagerDefaultFighterDesc;

	if (fkind != nFTKindNull)
	{
		if (fighter_gobj != NULL)
		{
			rot_y = DObjGetStruct(fighter_gobj)->rotate.vec.f.y;
			ftManagerDestroyFighter(fighter_gobj);
		}
		else rot_y = F_CST_DTOR32(0.0F);

		desc.fkind = fkind;
		sMNPlayers1PBonusSlot.costume = desc.costume = mnPlayers1PBonusGetCostume(fkind, 0);
		desc.figatree_heap = sMNPlayers1PBonusFigatreeHeap;
		desc.player = player;
		sMNPlayers1PBonusSlot.player = fighter_gobj = ftManagerMakeFighter(&desc);

		gcAddGObjProcess(fighter_gobj, mnPlayers1PBonusFighterProcUpdate, nGCProcessKindFunc, 1);

		DObjGetStruct(fighter_gobj)->translate.vec.f.x = -700.0F;
		DObjGetStruct(fighter_gobj)->translate.vec.f.y = -850.0F;

		DObjGetStruct(fighter_gobj)->rotate.vec.f.y = rot_y;

		DObjGetStruct(fighter_gobj)->scale.vec.f.x = dSCSubsysFighterScales[fkind];
		DObjGetStruct(fighter_gobj)->scale.vec.f.y = dSCSubsysFighterScales[fkind];
		DObjGetStruct(fighter_gobj)->scale.vec.f.z = dSCSubsysFighterScales[fkind];
	}
}

// 0x80134274
void mnPlayers1PBonusMakeFighterCamera(void)
{
	CObj *cobj = CObjGetStruct
	(
		gcMakeCameraGObj
		(
			nGCCommonKindSceneCamera,
			NULL,
			16,
			GOBJ_PRIORITY_DEFAULT,
			func_80017EC0,
			20,
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
	cobj->vec.eye.z = 5000.0F;

	cobj->flags = COBJ_FLAG_DLBUFFERS;

	cobj->vec.at.x = 0.0F;
	cobj->vec.at.y = 0.0F;
	cobj->vec.at.z = 0.0F;

	cobj->vec.up.x = 0.0F;
	cobj->vec.up.z = 0.0F;
	cobj->vec.up.y = 1.0F;
}

// 0x80134364
void mnPlayers1PBonusUpdateCursor(GObj *gobj, s32 player, s32 cursor_status)
{
	SObj *sobj;
	f32 start_pos_x, start_pos_y;
	SYColorRGBPair colors[/* */] =
	{
		{ { 0xE0, 0x15, 0x15 }, { 0x5B, 0x00, 0x00 } },
		{ { 0x00, 0x00, 0xFB }, { 0x00, 0x00, 0x52 } },
		{ { 0xCA, 0x94, 0x08 }, { 0x62, 0x3C, 0x00 } },
		{ { 0x00, 0x91, 0x00 }, { 0x00, 0x4F, 0x00 } }
	};
	intptr_t num_offsets[/* */] =
	{
		llMNPlayersCommon1PTextGradientSprite,
		llMNPlayersCommon2PTextGradientSprite,
		llMNPlayersCommon3PTextGradientSprite,
		llMNPlayersCommon4PTextGradientSprite
	};
	intptr_t cursor_offsets[/* */] =
	{
		llMNPlayersCommonCursorHandPointSprite,
		llMNPlayersCommonCursorHandGrabSprite,
		llMNPlayersCommonCursorHandHoverSprite
	};
	Vec2i pos[/* */] =
	{
		{ 7, 15 },
		{ 9, 10 },
		{ 9, 15 }
	};

	start_pos_x = SObjGetStruct(gobj)->pos.x;
	start_pos_y = SObjGetStruct(gobj)->pos.y;

	gcRemoveSObjAll(gobj);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[0], cursor_offsets[cursor_status]));
	sobj->pos.x = start_pos_x;
	sobj->pos.y = start_pos_y;
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[0], num_offsets[player]));
	sobj->pos.x = SObjGetPrev(sobj)->pos.x + pos[cursor_status].x;
	sobj->pos.y = SObjGetPrev(sobj)->pos.y + pos[cursor_status].y;

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->sprite.red = colors[player].prim.r;
	sobj->sprite.green = colors[player].prim.g;
	sobj->sprite.blue = colors[player].prim.b;

	sobj->envcolor.r = colors[player].env.r;
	sobj->envcolor.g = colors[player].env.g;
	sobj->envcolor.b = colors[player].env.b;
}

// 0x80134574 - Unused?
void mnPlayers1PBonusCheckTimeArrowRInRange(void)
{
	return;
}

// 0x8013457C - Unused?
void mnPlayers1PBonusCheckTimeArrowLInRange(void)
{
	return;
}

// 0x80134584
sb32 mnPlayers1PBonusCheckBackInRange(GObj *gobj)
{
	f32 pos_x, pos_y;
	sb32 is_in_range;
	SObj *sobj;

	sobj = SObjGetStruct(gobj);

	pos_y = sobj->pos.y + 3.0F;

	is_in_range = ((pos_y < 13.0F) || (pos_y > 34.0F)) ? TRUE : FALSE;

	if (is_in_range != FALSE)
	{
		return FALSE;
	}
	pos_x = sobj->pos.x + 20.0F;

	is_in_range = ((pos_x >= 244.0F) && (pos_x <= 292.0F)) ? TRUE : FALSE;

	if (is_in_range != FALSE)
	{
		return TRUE;
	}
	else return FALSE;
}

// 0x8013464C
sb32 mnPlayers1PBonusCheckPuckInRange(GObj *gobj, s32 cursor_player, s32 player)
{
	f32 pos_x, pos_y;
	sb32 is_in_range;
	SObj *cursor_sobj = SObjGetStruct(gobj);
	SObj *puck_sobj = SObjGetStruct(sMNPlayers1PBonusSlot.puck);

	pos_x = cursor_sobj->pos.x + 25.0F;
	is_in_range = ((pos_x >= puck_sobj->pos.x) && (pos_x <= puck_sobj->pos.x + 26.0F)) ? TRUE : FALSE;

	if (is_in_range != FALSE)
	{
		pos_y = cursor_sobj->pos.y + 3.0F;
		is_in_range = ((pos_y >= puck_sobj->pos.y) && (pos_y <= puck_sobj->pos.y + 24.0F)) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			return TRUE;
		}
	}
	return FALSE;
}

// 0x80134724
void mnPlayers1PBonusResetPlayer(s32 player)
{
	sMNPlayers1PBonusSlot.is_selected = FALSE;
	sMNPlayers1PBonusSlot.fkind = nFTKindNull;
	sMNPlayers1PBonusSlot.holder_player = player;
	sMNPlayers1PBonusSlot.held_player = player;
	sMNPlayers1PBonusSlot.is_fighter_selected = FALSE;

	mnPlayers1PBonusUpdateCursorGrabPriorities(player, player);

	sMNPlayers1PBonusSlot.is_cursor_adjusting = FALSE;
}

// 0x8013476C
void mnPlayers1PBonusUpdateFighter(s32 player)
{
	sb32 is_skip_fighter = FALSE;

	if ((sMNPlayers1PBonusSlot.fkind == nFTKindNull) && (sMNPlayers1PBonusSlot.is_selected == FALSE))
	{
		sMNPlayers1PBonusSlot.player->flags = GOBJ_FLAG_HIDDEN;
		mnPlayers1PBonusMakeHiScore();
		is_skip_fighter = TRUE;
	}
	if (is_skip_fighter == FALSE)
	{
		mnPlayers1PBonusMakeFighter(sMNPlayers1PBonusSlot.player, player, sMNPlayers1PBonusSlot.fkind);
		mnPlayers1PBonusMakeHiScore();
		sMNPlayers1PBonusSlot.player->flags = GOBJ_FLAG_NONE;
		sMNPlayers1PBonusSlot.is_status_selected = FALSE;
	}
}

// 0x801347F0 - Unused?
void func_ovl29_801347F0(void)
{
	return;
}

// 0x801347F8
void mnPlayers1PBonusUpdateNameAndEmblem(s32 player)
{
	if ((sMNPlayers1PBonusSlot.fkind == nFTKindNull) && (sMNPlayers1PBonusSlot.is_selected == FALSE))
	{
		sMNPlayers1PBonusSlot.name_emblem_gobj->flags = GOBJ_FLAG_HIDDEN;
	}
	else
	{
		sMNPlayers1PBonusSlot.name_emblem_gobj->flags = GOBJ_FLAG_NONE;
		mnPlayers1PBonusMakeNameAndEmblem(sMNPlayers1PBonusSlot.name_emblem_gobj, player, sMNPlayers1PBonusSlot.fkind);
	}
}

// 0x80134858
void mnPlayers1PBonusDestroyPortraitFlash(s32 player)
{
	GObj *gobj = sMNPlayers1PBonusSlot.flash;

	if (gobj != NULL)
	{
		sMNPlayers1PBonusSlot.flash = NULL;
		gcEjectGObj(gobj);
	}
}

// 0x80134890
void mnPlayers1PBonusPortraitFlashThreadUpdate(GObj *gobj)
{
	s32 length = 16;
	s32 wait_tics = 1;

	while (TRUE)
	{
		length--, wait_tics--;

		if (length == 0)
		{
			mnPlayers1PBonusDestroyPortraitFlash(gobj->user_data.s);
		}
		if (wait_tics == 0)
		{
			wait_tics = 1;
			gobj->flags = (gobj->flags == GOBJ_FLAG_HIDDEN) ? GOBJ_FLAG_NONE : GOBJ_FLAG_HIDDEN;
		}
		gcSleepCurrentGObjThread(1);
	}
}

// 0x8013491C
void mnPlayers1PBonusMakePortraitFlash(s32 player)
{
	GObj *gobj;
	SObj *sobj;
	s32 portrait = mnPlayers1PBonusGetPortrait(sMNPlayers1PBonusSlot.fkind);

	mnPlayers1PBonusDestroyPortraitFlash(player);

	sMNPlayers1PBonusSlot.flash = gobj = gcMakeGObjSPAfter(0, NULL, 26, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 33, GOBJ_PRIORITY_DEFAULT, ~0);
	gobj->user_data.s = player;
	gcAddGObjProcess(gobj, mnPlayers1PBonusPortraitFlashThreadUpdate, nGCProcessKindThread, 1);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[4], llMNPlayersPortraitsWhiteSquareSprite));
	sobj->pos.x = (((portrait >= 6) ? portrait - 6 : portrait) * 45) + 26;
	sobj->pos.y = (((portrait >= 6) ? 1 : 0) * 43) + 37;
}

// 0x80134A50
void mnPlayers1PBonusAnnounceFighter(s32 player, s32 slot)
{
	u16 announce_names[/* */] =
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

	if (sMNPlayers1PBonusSlot.p_sfx != NULL)
	{
		if ((sMNPlayers1PBonusSlot.p_sfx->sfx_id != 0) && (sMNPlayers1PBonusSlot.p_sfx->sfx_id == sMNPlayers1PBonusSlot.sfx_id))
		{
			func_80026738_27338(sMNPlayers1PBonusSlot.p_sfx);
		}
	}
	func_800269C0_275C0(nSYAudioFGMMarioDash);

	sMNPlayers1PBonusSlot.p_sfx = func_800269C0_275C0(announce_names[sMNPlayers1PBonusSlot.fkind]);

	if (sMNPlayers1PBonusSlot.p_sfx != NULL)
	{
		sMNPlayers1PBonusSlot.sfx_id = sMNPlayers1PBonusSlot.p_sfx->sfx_id;
	}
}

// 0x80134B1C - Unused?
void func_ovl29_80134B1C(void)
{
	return;
}

// 0x80134B24
sb32 mnPlayers1PBonusCheckSelectFighter(GObj *gobj, s32 player, s32 unused, s32 select_button)
{
	if (sMNPlayers1PBonusSlot.cursor_status != nMNPlayersCursorStatusGrab)
	{
		return FALSE;
	}
	else if (sMNPlayers1PBonusSlot.fkind != nFTKindNull)
	{
		mnPlayers1PBonusSelectFighterPuck(player, select_button);
		sMNPlayers1PBonusSlot.recall_end_tic = sMNPlayers1PBonusTotalTimeTics + 30;
		func_800269C0_275C0(nSYAudioFGMStageSelect);

		return TRUE;
	}
	else func_800269C0_275C0(nSYAudioFGMMenuDenied);

	return FALSE;
}

// 0x80134BB0
void mnPlayers1PBonusUpdateCursorGrabPriorities(s32 player, s32 puck)
{
	// Display orders for cursors on puck pickup
	u32 priorities[/* */] = { 6, 4, 2, 0 };

	gcMoveGObjDL(sMNPlayers1PBonusSlot.puck, 30, priorities[player] + 1);
}

// 0x80134C1C
void mnPlayers1PBonusUpdateCursorPlacementPriorities(s32 player)
{
	// Display orders for cursors not holding pucks on puck placement
	u32 priorities[/* */] = { 3, 2, 1, 0 };

	gcMoveGObjDL(sMNPlayers1PBonusSlot.puck, 31, priorities[player]);
}

// 0x80134C80
void mnPlayers1PBonusSetCursorPuckOffset(s32 player)
{
	sMNPlayers1PBonusSlot.cursor_pickup_x = SObjGetStruct(sMNPlayers1PBonusSlot.puck)->pos.x - 11.0F;
	sMNPlayers1PBonusSlot.cursor_pickup_y = SObjGetStruct(sMNPlayers1PBonusSlot.puck)->pos.y - -14.0F;
}

// 0x80134CC4
void mnPlayers1PBonusSetCursorGrab(s32 player)
{
	sMNPlayers1PBonusSlot.holder_player = player;
	sMNPlayers1PBonusSlot.is_selected = FALSE;
	sMNPlayers1PBonusSlot.cursor_status = nMNPlayersCursorStatusGrab;
	sMNPlayers1PBonusSlot.held_player = player;
	sMNPlayers1PBonusSlot.is_fighter_selected = FALSE;

	mnPlayers1PBonusUpdateFighter(player);
	mnPlayers1PBonusUpdateCursorGrabPriorities(player, player);
	mnPlayers1PBonusSetCursorPuckOffset(player);
	mnPlayers1PBonusUpdateCursor(sMNPlayers1PBonusSlot.cursor, player, sMNPlayers1PBonusSlot.cursor_status);

	sMNPlayers1PBonusSlot.is_cursor_adjusting = TRUE;

	func_800269C0_275C0(nSYAudioFGMSamusDash);

	mnPlayers1PBonusDestroyPortraitFlash(player);
	mnPlayers1PBonusUpdateNameAndEmblem(player);
}

// 0x80134D54
sb32 mnPlayers1PBonusCheckCursorPuckGrab(GObj *gobj, s32 player)
{
	MNPlayersSlotBonus *pslot = &sMNPlayers1PBonusSlot;

	if (sMNPlayers1PBonusTotalTimeTics < sMNPlayers1PBonusSlot.recall_end_tic)
	{
		return FALSE;
	}
	else if (sMNPlayers1PBonusSlot.cursor_status != nMNPlayersCursorStatusHover)
	{
		return FALSE;
	}
	else if ((sMNPlayers1PBonusSlot.holder_player == GMCOMMON_PLAYERS_MAX) && (mnPlayers1PBonusCheckPuckInRange(gobj, player, player) != FALSE))
	{
		sMNPlayers1PBonusSlot.holder_player = player;
		sMNPlayers1PBonusSlot.is_selected = FALSE;
		sMNPlayers1PBonusSlot.cursor_status = nMNPlayersCursorStatusGrab;
		pslot->held_player = player;
		sMNPlayers1PBonusSlot.is_fighter_selected = FALSE;

		mnPlayers1PBonusUpdateFighter(player);
		mnPlayers1PBonusUpdateCursorGrabPriorities(player, player);
		mnPlayers1PBonusSetCursorPuckOffset(player);
		mnPlayers1PBonusUpdateCursor(gobj, player, sMNPlayers1PBonusSlot.cursor_status);

		sMNPlayers1PBonusSlot.is_cursor_adjusting = TRUE;

		func_800269C0_275C0(nSYAudioFGMSamusDash);
		mnPlayers1PBonusDestroyPortraitFlash(player);
		mnPlayers1PBonusUpdateNameAndEmblem(player);

		return TRUE;
	}
	else return FALSE;
}

// 0x80134E50
s32 mnPlayers1PBonusGetForcePuckFighterKind(void)
{
	SObj *sobj = SObjGetStruct(sMNPlayers1PBonusSlot.puck);
	s32 pos_x = (s32) sobj->pos.x + 13;
	s32 pos_y = (s32) sobj->pos.y + 12;
	s32 fkind;
	sb32 is_in_range = ((pos_y > 35) && (pos_y < 79)) ? TRUE : FALSE;

	if (is_in_range != FALSE)
	{
		is_in_range = ((pos_x > 24) && (pos_x < 295)) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			return mnPlayers1PBonusGetFighterKind((pos_x - 25) / 45);
		}
	}
	is_in_range = ((pos_y > 78) && (pos_y < 122)) ? TRUE : FALSE;

	if (is_in_range != FALSE)
	{
		is_in_range = ((pos_x > 24) && (pos_x < 295)) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			return mnPlayers1PBonusGetFighterKind(((pos_x - 25) / 45) + 6);
		}
	}
	return nFTKindNull;
}

// 0x80134F6C
s32 mnPlayers1PBonusGetPuckFighterKind(s32 player)
{
	SObj *sobj = SObjGetStruct(sMNPlayers1PBonusSlot.puck);
	s32 pos_x = (s32) sobj->pos.x + 13;
	s32 pos_y = (s32) sobj->pos.y + 12;
	s32 fkind;
	sb32 is_in_range = ((pos_y > 35) && (pos_y < 79)) ? TRUE : FALSE;

	if (is_in_range != FALSE)
	{
		is_in_range = ((pos_x > 24) && (pos_x < 295)) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			fkind = mnPlayers1PBonusGetFighterKind((pos_x - 25) / 45);

			if ((mnPlayers1PBonusCheckFighterCrossed(fkind) != FALSE) || (mnPlayers1PBonusCheckFighterLocked(fkind) != FALSE))
			{
				return nFTKindNull;
			}
			else return fkind;
		}
	}
	is_in_range = ((pos_y > 78) && (pos_y < 122)) ? TRUE : FALSE;

	if (is_in_range != FALSE)
	{
		is_in_range = ((pos_x > 24) && (pos_x < 295)) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			fkind = mnPlayers1PBonusGetFighterKind(((pos_x - 25) / 45) + 6);

			if ((mnPlayers1PBonusCheckFighterCrossed(fkind) != FALSE) || (mnPlayers1PBonusCheckFighterLocked(fkind) != FALSE))
			{
				return nFTKindNull;
			}
			else return fkind;
		}
	}
	return nFTKindNull;
}

// 0x801350E4
void mnPlayers1PBonusAdjustCursor(GObj *gobj, s32 player)
{
	s32 unused;
	Vec2i pos[/* */] =
	{
		{ 7, 15 },
		{ 9, 10 },
		{ 9, 15 }
	};
	f32 delta;
	sb32 is_in_range;

	if (sMNPlayers1PBonusSlot.is_cursor_adjusting != FALSE)
	{
		delta = (sMNPlayers1PBonusSlot.cursor_pickup_x - SObjGetStruct(sMNPlayers1PBonusSlot.cursor)->pos.x) / 5.0F;
		is_in_range = ((delta >= -1.0F) && (delta <= 1.0F)) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			SObjGetStruct(sMNPlayers1PBonusSlot.cursor)->pos.x = sMNPlayers1PBonusSlot.cursor_pickup_x;
		}
		else SObjGetStruct(sMNPlayers1PBonusSlot.cursor)->pos.x += delta;

		delta = (sMNPlayers1PBonusSlot.cursor_pickup_y - SObjGetStruct(sMNPlayers1PBonusSlot.cursor)->pos.y) / 5.0F;
		is_in_range = ((delta >= -1.0F) && (delta <= 1.0F)) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			SObjGetStruct(sMNPlayers1PBonusSlot.cursor)->pos.y = sMNPlayers1PBonusSlot.cursor_pickup_y;
		}
		else SObjGetStruct(sMNPlayers1PBonusSlot.cursor)->pos.y += delta;

		if
		(
			(SObjGetStruct(sMNPlayers1PBonusSlot.cursor)->pos.x == sMNPlayers1PBonusSlot.cursor_pickup_x) &&
			(SObjGetStruct(sMNPlayers1PBonusSlot.cursor)->pos.y == sMNPlayers1PBonusSlot.cursor_pickup_y)
		)
		{
			sMNPlayers1PBonusSlot.is_cursor_adjusting = FALSE;
		}
		SObjGetStruct(gobj)->next->pos.x = SObjGetStruct(gobj)->pos.x + pos[sMNPlayers1PBonusSlot.cursor_status].x;
		SObjGetStruct(gobj)->next->pos.y = SObjGetStruct(gobj)->pos.y + pos[sMNPlayers1PBonusSlot.cursor_status].y;
	}
	else if (sMNPlayers1PBonusSlot.is_recalling == FALSE)
	{
		is_in_range = ((gSYControllerDevices[player].stick_range.x < -8) || (gSYControllerDevices[player].stick_range.x > 8)) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			delta = (gSYControllerDevices[player].stick_range.x / 20.0F) + SObjGetStruct(gobj)->pos.x;
			is_in_range = ((delta >= 0.0F) && (delta <= 280.0F)) ? TRUE : FALSE;

			if (is_in_range != FALSE)
			{
				SObjGetStruct(gobj)->pos.x = delta;
				SObjGetStruct(gobj)->next->pos.x = SObjGetStruct(gobj)->pos.x + pos[sMNPlayers1PBonusSlot.cursor_status].x;
			}
		}
		is_in_range = ((gSYControllerDevices[player].stick_range.y < -8) || (gSYControllerDevices[player].stick_range.y > 8)) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			delta = (gSYControllerDevices[player].stick_range.y / -20.0F) + SObjGetStruct(gobj)->pos.y;
			is_in_range = ((delta >= 10.0F) && (delta <= 205.0F)) ? TRUE : FALSE;

			if (is_in_range != FALSE)
			{
				SObjGetStruct(gobj)->pos.y = delta;
				SObjGetStruct(gobj)->next->pos.y = SObjGetStruct(gobj)->pos.y + pos[sMNPlayers1PBonusSlot.cursor_status].y;
			}
		}
	}
}

// 0x8013545C
void mnPlayers1PBonusUpdateCursorNoRecall(GObj *gobj, s32 player)
{
	s32 i;

	if ((SObjGetStruct(gobj)->pos.y > 124.0F) || (SObjGetStruct(gobj)->pos.y < 38.0F))
	{
		if (sMNPlayers1PBonusSlot.cursor_status != nMNPlayersCursorStatusPointer)
		{
			mnPlayers1PBonusUpdateCursor(gobj, player, nMNPlayersCursorStatusPointer);
			sMNPlayers1PBonusSlot.cursor_status = nMNPlayersCursorStatusPointer;
		}
	}
	else if (sMNPlayers1PBonusSlot.held_player == -1)
	{
		if (sMNPlayers1PBonusSlot.cursor_status != nMNPlayersCursorStatusHover)
		{
			mnPlayers1PBonusUpdateCursor(gobj, player, nMNPlayersCursorStatusHover);
			sMNPlayers1PBonusSlot.cursor_status = nMNPlayersCursorStatusHover;
		}
	}
	else if (sMNPlayers1PBonusSlot.cursor_status != nMNPlayersCursorStatusGrab)
	{
		mnPlayers1PBonusUpdateCursor(gobj, player, nMNPlayersCursorStatusGrab);
		sMNPlayers1PBonusSlot.cursor_status = nMNPlayersCursorStatusGrab;
	}
	if ((sMNPlayers1PBonusSlot.cursor_status == nMNPlayersCursorStatusPointer) && (sMNPlayers1PBonusSlot.is_selected != FALSE))
	{
		for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
		{
			if ((sMNPlayers1PBonusSlot.is_selected == TRUE) && (mnPlayers1PBonusCheckPuckInRange(gobj, player, i) != FALSE))
			{
				mnPlayers1PBonusUpdateCursor(gobj, player, nMNPlayersCursorStatusHover);
				sMNPlayers1PBonusSlot.cursor_status = nMNPlayersCursorStatusHover;

				break;
			}
		}
	}
}

// 0x801355E0
void mnPlayers1PBonusUpdateCostume(s32 player, s32 select_button)
{
	s32 costume = ftParamGetCostumeCommonID(sMNPlayers1PBonusSlot.fkind, select_button);

	ftParamInitAllParts(sMNPlayers1PBonusSlot.player, costume, 0);

	sMNPlayers1PBonusSlot.costume = costume;

	func_800269C0_275C0(nSYAudioFGMMenuScroll2);
}

// 0x80135634
sb32 mnPlayers1PBonusCheckManFighterSelected(s32 unused)
{
	if (sMNPlayers1PBonusSlot.is_selected != FALSE)
	{
		return TRUE;
	}
	else return FALSE;
}

// 0x8013565C
void mnPlayers1PBonusRecallPuck(s32 player)
{
	sMNPlayers1PBonusSlot.is_fighter_selected = FALSE;
	sMNPlayers1PBonusSlot.is_selected = FALSE;
	sMNPlayers1PBonusSlot.is_recalling = TRUE;
	sMNPlayers1PBonusSlot.recall_tics = 0;
	sMNPlayers1PBonusSlot.recall_start_x = SObjGetStruct(sMNPlayers1PBonusSlot.puck)->pos.x;
	sMNPlayers1PBonusSlot.recall_start_y = SObjGetStruct(sMNPlayers1PBonusSlot.puck)->pos.y;

	sMNPlayers1PBonusSlot.recall_end_x = SObjGetStruct(sMNPlayers1PBonusSlot.cursor)->pos.x + 20.0F;

	if (sMNPlayers1PBonusSlot.recall_end_x > 280.0F)
	{
		sMNPlayers1PBonusSlot.recall_end_x = 280.0F;
	}
	sMNPlayers1PBonusSlot.recall_end_y = SObjGetStruct(sMNPlayers1PBonusSlot.cursor)->pos.y + -15.0F;

	if (sMNPlayers1PBonusSlot.recall_end_y < 10.0F)
	{
		sMNPlayers1PBonusSlot.recall_end_y = 10.0F;
	}
	if (sMNPlayers1PBonusSlot.recall_end_y < sMNPlayers1PBonusSlot.recall_start_y)
	{
		sMNPlayers1PBonusSlot.recall_mid_y = sMNPlayers1PBonusSlot.recall_end_y - 20.0F;
	}
	else sMNPlayers1PBonusSlot.recall_mid_y = sMNPlayers1PBonusSlot.recall_start_y - 20.0F;
}

// 0x80135740
void mnPlayers1PBonusBackTo1PMode(void)
{
	if (sMNPlayers1PBonusBonusKind == 0)
	{
		gSCManagerSceneData.scene_prev = nSCKind1PBonus1Players;
	}
	else gSCManagerSceneData.scene_prev = nSCKind1PBonus2Players;

	gSCManagerSceneData.scene_curr = nSCKind1PMode;

	mnPlayers1PBonusSetSceneData();
	syAudioStopBGMAll();
	func_800266A0_272A0();
	syTaskmanSetLoadScene();
}

// 0x801357AC
void mnPlayers1PBonusDetectBack(s32 player)
{
	if ((sMNPlayers1PBonusTotalTimeTics >= 10) && (gSYControllerDevices[player].button_tap & B_BUTTON))
	{
		mnPlayers1PBonusBackTo1PMode();
	}
}

// 0x80135800
sb32 mnPlayers1PBonusCheckGameModeInRange(GObj *gobj)
{
	f32 pos_x, pos_y;
	sb32 is_in_range;
	SObj *sobj;

	sobj = SObjGetStruct(gobj);

	pos_x = sobj->pos.x + 20.0F;
	is_in_range = ((pos_x >= 27.0F) && (pos_x <= 207.0F)) ? TRUE : FALSE;

	if (is_in_range != FALSE)
	{
		pos_y = sobj->pos.y + 3.0F;
		is_in_range = ((pos_y >= 14.0F) && (pos_y <= 35.0F)) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			return TRUE;
		}
	}
	return FALSE;
}

// 0x801358C4
void mnPlayers1PBonusUpdateGameMode(void)
{
	if (sMNPlayers1PBonusBonusKind == 0)
	{
		sMNPlayers1PBonusBonusKind = 1;
	}
	else sMNPlayers1PBonusBonusKind = 0;

	gcEjectGObj(sMNPlayers1PBonusGameModeGObj);
	mnPlayers1PBonusMakeLabels();
	mnPlayers1PBonusMakeHiScore();

	if (sMNPlayers1PBonusTotalTimeGObj != NULL)
	{
		gcEjectGObj(sMNPlayers1PBonusTotalTimeGObj);
		sMNPlayers1PBonusTotalTimeGObj = NULL;
	}
	if (mnPlayers1PBonusCheckBonusCompleteAll() != FALSE)
	{
		mnPlayers1PBonusMakeTotalTime();
	}
}

// 0x80135950
void mnPlayers1PBonusCursorProcUpdate(GObj *gobj)
{
	s32 player = gobj->user_data.s;
	s32 unused[5];

	mnPlayers1PBonusAdjustCursor(gobj, player);

	if
	(
		(gSYControllerDevices[player].button_tap & A_BUTTON) &&
		(mnPlayers1PBonusCheckSelectFighter(gobj, player, sMNPlayers1PBonusSlot.held_player, 0) == FALSE) &&
		(mnPlayers1PBonusCheckCursorPuckGrab(gobj, player) == FALSE)
	)
	{
		if (mnPlayers1PBonusCheckGameModeInRange(gobj) != FALSE)
		{
			mnPlayers1PBonusUpdateGameMode();
		}
		else if (mnPlayers1PBonusCheckBackInRange(gobj) != FALSE)
		{
			mnPlayers1PBonusBackTo1PMode();
			func_800269C0_275C0(nSYAudioFGMMenuScroll2);
		}
	}
	if
	(
		(gSYControllerDevices[player].button_tap & U_CBUTTONS) &&
		(mnPlayers1PBonusCheckSelectFighter(gobj, player, sMNPlayers1PBonusSlot.held_player, 0) == FALSE) &&
		(sMNPlayers1PBonusSlot.is_fighter_selected != FALSE)
	)
	{
		mnPlayers1PBonusUpdateCostume(player, 0);
	}
	if
	(
		(gSYControllerDevices[player].button_tap & R_CBUTTONS) &&
		(mnPlayers1PBonusCheckSelectFighter(gobj, player, sMNPlayers1PBonusSlot.held_player, 1) == FALSE) &&
		(sMNPlayers1PBonusSlot.is_fighter_selected != FALSE)
	)
	{
		mnPlayers1PBonusUpdateCostume(player, 1);
	}
	if
	(
		(gSYControllerDevices[player].button_tap & D_CBUTTONS) &&
		(mnPlayers1PBonusCheckSelectFighter(gobj, player, sMNPlayers1PBonusSlot.held_player, 2) == FALSE) &&
		(sMNPlayers1PBonusSlot.is_fighter_selected != FALSE)
	)
	{
		mnPlayers1PBonusUpdateCostume(player, 2);
	}
	if
	(
		(gSYControllerDevices[player].button_tap & L_CBUTTONS) &&
		(mnPlayers1PBonusCheckSelectFighter(gobj, player, sMNPlayers1PBonusSlot.held_player, 3) == FALSE) &&
		(sMNPlayers1PBonusSlot.is_fighter_selected != FALSE)
	)
	{
		mnPlayers1PBonusUpdateCostume(player, 3);
	}
	if ((gSYControllerDevices[player].button_tap & B_BUTTON) && (mnPlayers1PBonusCheckManFighterSelected(player) != FALSE))
	{
		mnPlayers1PBonusRecallPuck(player);
	}
	if (sMNPlayers1PBonusSlot.is_recalling == FALSE)
	{
		mnPlayers1PBonusDetectBack(player);
	}
	if (sMNPlayers1PBonusSlot.is_recalling == FALSE)
	{
		mnPlayers1PBonusUpdateCursorNoRecall(gobj, player);
	}
}

// 0x801374AC
intptr_t dMNPlayers1PBonusPuckSpriteOffsets[/* */] =
{
	llMNPlayersCommon1PPuckSprite,
	llMNPlayersCommon2PPuckSprite,
	llMNPlayersCommon3PPuckSprite,
	llMNPlayersCommon4PPuckSprite,
	llMNPlayersCommonCPPuckSprite
};

// 0x80135BA4 - Unused?
void func_ovl29_80135BA4(void)
{
	return;
}

// 0x80135BAC - Unused?
void func_ovl29_80135BAC(void)
{
	return;
}

// 0x80135BB4
void mnPlayers1PBonusMovePuck(s32 player)
{
	SObjGetStruct(sMNPlayers1PBonusSlot.puck)->pos.x += sMNPlayers1PBonusSlot.puck_vel_x;
	SObjGetStruct(sMNPlayers1PBonusSlot.puck)->pos.y += sMNPlayers1PBonusSlot.puck_vel_y;
}

// 0x80135BF4
void mnPlayers1PBonusPuckProcUpdate(GObj *gobj)
{
	s32 fkind;
	s32 player = gobj->user_data.s;

	if
	(
		(sMNPlayers1PBonusSlot.cursor_status != nMNPlayersCursorStatusPointer) ||
		(sMNPlayers1PBonusSlot.is_selected == TRUE) ||
		(sMNPlayers1PBonusSlot.is_recalling == TRUE)
	)
	{
		gobj->flags = GOBJ_FLAG_NONE;
	}
	else gobj->flags = GOBJ_FLAG_HIDDEN;

	if
	(
		(sMNPlayers1PBonusSlot.is_selected == FALSE) &&
		(sMNPlayers1PBonusSlot.holder_player != GMCOMMON_PLAYERS_MAX)
	)
	{
		if (sMNPlayers1PBonusSlot.is_cursor_adjusting == FALSE)
		{
			SObjGetStruct(gobj)->pos.x = SObjGetStruct(sMNPlayers1PBonusSlot.cursor)->pos.x + 11.0F;
			SObjGetStruct(gobj)->pos.y = SObjGetStruct(sMNPlayers1PBonusSlot.cursor)->pos.y + -14.0F;
		}
	}
	else mnPlayers1PBonusMovePuck(player);

	fkind = mnPlayers1PBonusGetPuckFighterKind(player);

	if ((sMNPlayers1PBonusSlot.is_selected == FALSE) && (fkind != sMNPlayers1PBonusSlot.fkind))
	{
		sMNPlayers1PBonusSlot.fkind = fkind;

		mnPlayers1PBonusUpdateFighter(player);
		mnPlayers1PBonusUpdateNameAndEmblem(player);
	}
	mnPlayers1PBonusMakeHiScore();
}

// 0x80135D08
void mnPlayers1PBonusMakeReadyCamera(void)
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
			10,
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

// 0x80135DA8
void mnPlayers1PBonusMakeCursorCamera(void)
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
			13,
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

// 0x80135E48
void mnPlayers1PBonusMakePuckCamera(void)
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

// 0x80135EE8
void mnPlayers1PBonusMakeCursor(s32 player)
{
	GObj *gobj;
	s32 unused;

	// ???
	intptr_t unused_offsets[/* */] =
	{
		llMNPlayersCommon1PTextGradientSprite,
		llMNPlayersCommon2PTextGradientSprite,
		llMNPlayersCommon3PTextGradientSprite,
		llMNPlayersCommon4PTextGradientSprite
	};
	u32 priorities[/* */] = { 6, 4, 2, 0 };

	gobj = lbCommonMakeSpriteGObj
	(
		0,
		NULL,
		19,
		GOBJ_PRIORITY_DEFAULT,
		lbCommonDrawSObjAttr,
		30,
		priorities[player],
		~0,
		lbRelocGetFileData
		(
			Sprite*,
			sMNPlayers1PBonusFiles[0],
			llMNPlayersCommonCursorHandGrabSprite
		),
		nGCProcessKindFunc,
		mnPlayers1PBonusCursorProcUpdate,
		2
	);
	gobj->user_data.s = player;
	sMNPlayers1PBonusSlot.cursor = gobj;

	SObjGetStruct(gobj)->pos.x = 80.0F;
	SObjGetStruct(gobj)->pos.y = 170.0F;
	SObjGetStruct(gobj)->sprite.attr &= ~SP_FASTCOPY;
	SObjGetStruct(gobj)->sprite.attr |= SP_TRANSPARENT;

	mnPlayers1PBonusUpdateCursor(gobj, player, nMNPlayersCursorStatusPointer);
	sMNPlayers1PBonusSlot.is_selected = FALSE;
}

// 0x80136034
void mnPlayers1PBonusMakePuck(s32 player)
{
	GObj *gobj;
	s32 unused;

	intptr_t offsets[/* */] =
	{
		llMNPlayersCommon1PPuckSprite,
		llMNPlayersCommon2PPuckSprite,
		llMNPlayersCommon3PPuckSprite,
		llMNPlayersCommon4PPuckSprite
	};

	// Display orders for pucks on initial load
	u32 priorities[/* */] = { 3, 2, 1, 0 };

	gobj = lbCommonMakeSpriteGObj
	(
		0,
		NULL,
		20,
		GOBJ_PRIORITY_DEFAULT,
		lbCommonDrawSObjAttr,
		31,
		priorities[player],
		~0,
		lbRelocGetFileData
		(
			Sprite*,
			sMNPlayers1PBonusFiles[0],
			offsets[player]
		),
		nGCProcessKindFunc,
		mnPlayers1PBonusPuckProcUpdate,
		1
	);
	gobj->user_data.s = player;
	sMNPlayers1PBonusSlot.puck = gobj;

	SObjGetStruct(gobj)->pos.x = 51.0F;
	SObjGetStruct(gobj)->pos.y = 161.0F;
	SObjGetStruct(gobj)->sprite.attr &= ~SP_FASTCOPY;
	SObjGetStruct(gobj)->sprite.attr |= SP_TRANSPARENT;

	if (sMNPlayers1PBonusDevicesConnected[player] != -1)
	{
		sMNPlayers1PBonusSlot.holder_player = player;
	}
	else sMNPlayers1PBonusSlot.holder_player = GMCOMMON_PLAYERS_MAX;
}

// 0x801361A4 - Unused?
void func_ovl29_801361A4(void)
{
	return;
}

// 0x801361AC
/* DIVERGES in its SIGNATURE only. The decomp types the parameter
 * `GObj *gobj` here and `s32 player` in the identical
 * mnPlayers1PGamePuckAdjustPortraitEdge, and its own caller two
 * functions down passes an s32 -- so one of the two typings is a
 * decomp artifact and this is it. The parameter is read by neither
 * body. Left as written the port builds with an int-to-pointer
 * conversion at every call. */
void mnPlayers1PBonusPuckAdjustPortraitEdge(s32 player)
{
	s32 portrait = mnPlayers1PBonusGetPortrait(sMNPlayers1PBonusSlot.fkind);
	f32 portrait_edge_x = ((portrait >= 6) ? portrait - 6 : portrait) * 45 + 25;
	f32 portrait_edge_y = ((portrait >= 6) ? 1 : 0) * 43 + 36;
	f32 new_pos_x = SObjGetStruct(sMNPlayers1PBonusSlot.puck)->pos.x + sMNPlayers1PBonusSlot.puck_vel_x + 13.0F;
	f32 new_pos_y = SObjGetStruct(sMNPlayers1PBonusSlot.puck)->pos.y + sMNPlayers1PBonusSlot.puck_vel_y + 12.0F;

	if (new_pos_x < (portrait_edge_x + 5.0F))
	{
		sMNPlayers1PBonusSlot.puck_vel_x = ((portrait_edge_x + 5.0F) - new_pos_x) / 10.0F;
	}
	if (((portrait_edge_x + 45.0F) - 5.0F) < new_pos_x)
	{
		sMNPlayers1PBonusSlot.puck_vel_x = ((new_pos_x - ((portrait_edge_x + 45.0F) - 5.0F)) * -1.0F) / 10.0F;
	}
	if (new_pos_y < (portrait_edge_y + 5.0F))
	{
		sMNPlayers1PBonusSlot.puck_vel_y = ((portrait_edge_y + 5.0F) - new_pos_y) / 10.0F;
	}
	if (((portrait_edge_y + 43.0F) - 5.0F) < new_pos_y)
	{
		sMNPlayers1PBonusSlot.puck_vel_y = ((new_pos_y - ((portrait_edge_y + 43.0F) - 5.0F)) * -1.0F) / 10.0F;
	}
}

// 0x8013635C
void mnPlayers1PBonusPuckAdjustPlaced(s32 player)
{
	mnPlayers1PBonusPuckAdjustPortraitEdge(player);
}

// 0x8013637C
void mnPlayers1PBonusPuckAdjustRecall(s32 player)
{
	f32 vel_y, vel_x;

	sMNPlayers1PBonusSlot.recall_tics++;

	if (sMNPlayers1PBonusSlot.recall_tics < 11)
	{
		vel_x = (sMNPlayers1PBonusSlot.recall_end_x - sMNPlayers1PBonusSlot.recall_start_x) / 10.0F;

		if (sMNPlayers1PBonusSlot.recall_tics < 6)
		{
			vel_y = (sMNPlayers1PBonusSlot.recall_mid_y - sMNPlayers1PBonusSlot.recall_start_y) / 5.0F;
		}
		else vel_y = (sMNPlayers1PBonusSlot.recall_end_y - sMNPlayers1PBonusSlot.recall_mid_y) / 5.0F;
		
		sMNPlayers1PBonusSlot.puck_vel_x = vel_x;
		sMNPlayers1PBonusSlot.puck_vel_y = vel_y;
	}
	else if (sMNPlayers1PBonusSlot.recall_tics == 11)
	{
		mnPlayers1PBonusSetCursorGrab(player);

		sMNPlayers1PBonusSlot.puck_vel_x = 0.0F;
		sMNPlayers1PBonusSlot.puck_vel_y = 0.0F;
	}
	if (sMNPlayers1PBonusSlot.recall_tics == 30)
	{
		sMNPlayers1PBonusSlot.is_recalling = FALSE;
	}
}

// 0x80136450
void mnPlayers1PBonusPuckAdjustProcUpdate(GObj *gobj)
{
	if (sMNPlayers1PBonusSlot.is_recalling != FALSE)
	{
		mnPlayers1PBonusPuckAdjustRecall(sMNPlayers1PBonusManPlayer);
	}
	if (sMNPlayers1PBonusSlot.is_selected != FALSE)
	{
		mnPlayers1PBonusPuckAdjustPlaced(0);
	}
}

// 0x8013649C
void mnPlayers1PBonusMakePuckAdjust(void)
{
	gcAddGObjProcess(gcMakeGObjSPAfter(0, NULL, 24, GOBJ_PRIORITY_DEFAULT), mnPlayers1PBonusPuckAdjustProcUpdate, nGCProcessKindFunc, 1);
}

// 0x801364E0
void mnPlayers1PBonusSpotlightProcUpdate(GObj *gobj)
{
	f32 sizes[/* */] =
	{
		1.5F, 1.5F, 2.0F, 1.5F, 1.5F, 1.5F,
		1.5F, 1.5F, 1.5F, 1.5F, 1.5F, 1.5F
	};

	if ((sMNPlayers1PBonusSlot.is_fighter_selected == FALSE) && (sMNPlayers1PBonusSlot.fkind != nFTKindNull))
	{
		gobj->flags = (gobj->flags == GOBJ_FLAG_HIDDEN) ? GOBJ_FLAG_NONE : GOBJ_FLAG_HIDDEN;

		DObjGetStruct(gobj)->scale.vec.f.x = sizes[sMNPlayers1PBonusSlot.fkind];
		DObjGetStruct(gobj)->scale.vec.f.y = sizes[sMNPlayers1PBonusSlot.fkind];
		DObjGetStruct(gobj)->scale.vec.f.y = sizes[sMNPlayers1PBonusSlot.fkind];
	}
	else gobj->flags = GOBJ_FLAG_HIDDEN;
}

// 0x801365B8
void mnPlayers1PBonusMakeSpotlight(void)
{
	GObj *gobj;

	/* DIVERGES exactly as all three of the other selects': relocData
	 * file 22 is the spotlight's DObjDesc and MObjSubs, and the port
	 * has no reloc heap to read them out of -- sMNPlayers1PBonusFiles
	 * [10] is deliberately NULL. The model is a baked pack instead,
	 * its tree built by dc_model_add_dobjs, its materials by
	 * dc_model_add_mobjs and its drawing by the port's own layered
	 * display proc.
	 *
	 * Left as the decomp writes it this is a HANG, not a bad picture:
	 * gcSetupCommonDObjs walks a DObjDesc read through a bank that was
	 * never loaded and the scene dies in the allocator rather than at
	 * the read. */
#ifndef FT_HOSTTEST
	{
		int pal_bank = 0;

		if (fighter_load_scene(&sMNPlayers1PBonusSpotlightPack,
		                 MNPLAYERS1PBONUS_MODEL_SPOTLIGHT, &pal_bank) != 0)
		{
			return;
		}
	}
#endif
	gobj = gcMakeGObjSPAfter(0, NULL, 21, GOBJ_PRIORITY_DEFAULT);

#ifndef FT_HOSTTEST
	fighter_clone(&sMNPlayers1PBonusSpotlightModel,
	              &sMNPlayers1PBonusSpotlightPack,
	              syTaskmanMalloc(sizeof(float[4]) *
	                              sMNPlayers1PBonusSpotlightPack.hd->vert_count,
	                              0x8));
	dc_model_add_dobjs(gobj, NULL, &sMNPlayers1PBonusSpotlightModel, NULL);
	gcAddGObjDisplay(gobj, dc_model_proc_display_layered, 9,
	                 GOBJ_PRIORITY_DEFAULT, ~0);
	dc_model_add_mobjs(gobj, &sMNPlayers1PBonusSpotlightModel, 0.0F);
#else
	/* the host cross-test links the scene and not the renderer, so
	 * there is no pack to build a tree from and the placement below
	 * still needs one DObj to land on */
	gcAddDObjRpyR(gobj, NULL);
#endif
	gcAddGObjProcess(gobj, mnPlayers1PBonusSpotlightProcUpdate, nGCProcessKindFunc, 1);
	gcPlayAnimAll(gobj);

	DObjGetStruct(gobj)->translate.vec.f.x = -700.0F;
	DObjGetStruct(gobj)->translate.vec.f.y = -850.0F;
	DObjGetStruct(gobj)->translate.vec.f.z = 0.0F;
}

// 0x80136698
void mnPlayers1PBonusReadyProcUpdate(GObj *gobj)
{
	if (sMNPlayers1PBonusIsSelected != FALSE)
	{
		sMNPlayers1PBonusReadyBlinkWait++;

		if (sMNPlayers1PBonusReadyBlinkWait == 40)
		{
			sMNPlayers1PBonusReadyBlinkWait = 0;
		}
		gobj->flags = (sMNPlayers1PBonusReadyBlinkWait < 30) ? GOBJ_FLAG_NONE : GOBJ_FLAG_HIDDEN;
	}
	else
	{
		gobj->flags = GOBJ_FLAG_HIDDEN;
		sMNPlayers1PBonusReadyBlinkWait = 0;
	}
}

// 0x80136704
void mnPlayers1PBonusMakeReady(void)
{
	GObj *gobj;
	SObj *sobj;

	gobj = gcMakeGObjSPAfter(0, NULL, 28, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 35, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, mnPlayers1PBonusReadyProcUpdate, nGCProcessKindFunc, 1);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[0], llMNPlayersCommonReadyBannerSprite));
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->envcolor.r = 0x00;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;
	sobj->sprite.red = 0xF4;
	sobj->sprite.green = 0x56;
	sobj->sprite.blue = 0x7F;
	sobj->cms = 0;
	sobj->cmt = 0;
	sobj->masks = 3;
	sobj->maskt = 0;
	sobj->lrs = 320;
	sobj->lrt = 17;
	sobj->pos.x = 0.0F;
	sobj->pos.y = 71.0F;

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[0], llMNPlayersCommonReadyToFightTextSprite));
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->envcolor.r = 0xFF;
	sobj->envcolor.g = 0xCA;
	sobj->envcolor.b = 0x13;
	sobj->sprite.red = 0xFF;
	sobj->sprite.green = 0xFF;
	sobj->sprite.blue = 0x9D;
	sobj->pos.x = 50.0F;
	sobj->pos.y = 76.0F;

	gobj = gcMakeGObjSPAfter(0, NULL, 22, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 28, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, mnPlayers1PBonusReadyProcUpdate, nGCProcessKindFunc, 1);

#if defined(REGION_US)
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[0], llMNPlayersCommonPressTextSprite));
#else
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[0], llMNPlayersCommonPushTextSprite));
#endif
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->sprite.red = 0xD6;
	sobj->sprite.green = 0xDD;
	sobj->sprite.blue = 0xC6;
#if defined(REGION_US)
	sobj->pos.x = 133.0F;
#else
	sobj->pos.x = 120.0F;
#endif
	sobj->pos.y = 219.0F;

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[0], llMNPlayersCommonStartTextSprite));
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->sprite.red = 0xFF;
	sobj->sprite.green = 0x56;
	sobj->sprite.blue = 0x92;
#if defined(REGION_US)
	sobj->pos.x = 162.0F;
#else
	sobj->pos.x = 143.0F;
#endif
	sobj->pos.y = 219.0F;

#if defined(REGION_JP)
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PBonusFiles[0], llMNPlayersCommonButtonTextSprite));
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->sprite.red = 0xD6;
	sobj->sprite.green = 0xDD;
	sobj->sprite.blue = 0xC6;
	sobj->pos.x = 171.0F;
	sobj->pos.y = 219.0F;
#endif
}

// 0x80136980 - Unused?
void func_ovl29_80136980(void)
{
	return;
}

// 0x80136988 - Unused?
void func_ovl29_80136988(void)
{
	return;
}

// 0x80136990 - Unused?
void func_ovl29_80136990(void)
{
	return;
}

// 0x80136998
void mnPlayers1PBonusSetSceneData(void)
{
	gSCManagerSceneData.player = sMNPlayers1PBonusManPlayer;
	gSCManagerSceneData.bonus_fkind = sMNPlayers1PBonusSlot.fkind;
	gSCManagerSceneData.bonus_costume = sMNPlayers1PBonusSlot.costume;
}

// 0x801369C8 - Unused?
void func_ovl29_801369C8(void)
{
	return;
}

// 0x801369D0
void mnPlayers1PBonusFuncRun(GObj *gobj)
{
	sMNPlayers1PBonusTotalTimeTics++;

	if (sMNPlayers1PBonusTotalTimeTics == sMNPlayers1PBonusReturnTic)
	{
		gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
		gSCManagerSceneData.scene_curr = nSCKindTitle;

		mnPlayers1PBonusSetSceneData();
		syTaskmanSetLoadScene();
	}
	else
	{
		if (scSubsysControllerCheckNoInputAll() == FALSE)
		{
			sMNPlayers1PBonusReturnTic = sMNPlayers1PBonusTotalTimeTics + I_MIN_TO_TICS(5);
		}
		if ((sMNPlayers1PBonusIsSelected != FALSE) && (sMNPlayers1PBonusSlot.is_fighter_selected == FALSE))
		{
			sMNPlayers1PBonusIsSelected = FALSE;
		}
		if ((sMNPlayers1PBonusIsSelected != FALSE) && ((--sMNPlayers1PBonusStartWait == 0) || (gSYControllerDevices[sMNPlayers1PBonusManPlayer].button_tap & START_BUTTON)))
		{
			if (sMNPlayers1PBonusBonusKind == 0)
			{
				gSCManagerSceneData.scene_prev = nSCKind1PBonus1Players;
			}
			else gSCManagerSceneData.scene_prev = nSCKind1PBonus2Players;

			gSCManagerSceneData.scene_curr = nSCKind1PBonusStage;

			mnPlayers1PBonusSetSceneData();
			syTaskmanSetLoadScene();
		}
	}
}

// 0x80136B14 - Unused?
void func_ovl29_80136B14(void)
{
	return;
}

// 0x80136B1C - Unused?
void func_ovl29_80136B1C(void)
{
	return;
}

// 0x80136B24
void mnPlayers1PBonusInitPlayer(void)
{
	sMNPlayers1PBonusSlot.held_player = -1;
	sMNPlayers1PBonusSlot.flash = NULL;
	sMNPlayers1PBonusSlot.p_sfx = NULL;
	sMNPlayers1PBonusSlot.sfx_id = 0;
	sMNPlayers1PBonusSlot.is_selected = FALSE;
	sMNPlayers1PBonusSlot.is_recalling = FALSE;
	sMNPlayers1PBonusSlot.fkind = nFTKindNull;
}

// 0x80136B54
void mnPlayers1PBonusInitVars(void)
{
	sMNPlayers1PBonusTotalTimeTics = 0;
	sMNPlayers1PBonusReturnTic = sMNPlayers1PBonusTotalTimeTics + I_MIN_TO_TICS(5);
	sMNPlayers1PBonusUnknown0x801376D4 = 5;
	sMNPlayers1PBonusIsSelected = FALSE;
	sMNPlayers1PBonusManPlayer = gSCManagerSceneData.player;
	sMNPlayers1PBonusTotalTimeGObj = NULL;
	sMNPlayers1PBonusIsTeamBattle = gSCManager1PGameBattleState.is_team_battle;
	sMNPlayers1PBonusGameRules = gSCManager1PGameBattleState.game_rules;

	mnPlayers1PBonusInitPlayer();

	sMNPlayers1PBonusSlot.recall_end_tic = 0;
	sMNPlayers1PBonusFighterMask = gSCManagerBackupData.fighter_mask;

	if (gSCManagerSceneData.scene_curr == nSCKind1PBonus1Players)
	{
		sMNPlayers1PBonusBonusKind = 0;
	}
	else sMNPlayers1PBonusBonusKind = 1;
}

// 0x80136C14
void mnPlayers1PBonusInitSlot(s32 player)
{
	if (sMNPlayers1PBonusDevicesConnected[player] != -1)
	{
		mnPlayers1PBonusMakeCursor(player);
	}
	else sMNPlayers1PBonusSlot.cursor = NULL;

	mnPlayers1PBonusMakePuck(player);
	mnPlayers1PBonusMakeGate(player);
	mnPlayers1PBonusResetPlayer(player);
}

// 0x80136C80 - Unused?
void func_ovl29_80136C80(void)
{
	return;
}

// 0x80136C88
sb32 mnPlayers1PBonusCheckBonusCompleteAll(void)
{
	s32 i;

	for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
	{
		if (mnPlayers1PBonusCheckBonusComplete(i) == FALSE)
		{
			return FALSE;
		}
	}
	return TRUE;
}

/* DIVERGES. mnplayers1pbonus.c:2829-2836 is the reloc loader's setup
 * and its load of the eleven files; the port's files are banks, one
 * per entry of dMNPlayers1PBonusFileIDs, and the eleventh -- the
 * spotlight -- is a baked pack loaded by MakeSpotlight instead. Split
 * out of FuncStart under the name the other three selects' own have.
 *
 * A bank that will not load leaves its slot NULL, which is what the
 * game's own load-failure path leaves; the sprites that came out of it
 * are then absent rather than fatal, and the loader has already named
 * the missing file. */
void mnPlayers1PBonusLoadFiles(void)
{
	static const char *const paths[ARRAY_COUNT(dMNPlayers1PBonusFileIDs)] =
	{
		MNPLAYERS1PBONUS_BANK_COMMON,
		MNPLAYERS1PBONUS_BANK_EMBLEMS,
		MNPLAYERS1PBONUS_BANK_SELECT,
		MNPLAYERS1PBONUS_BANK_GAMEMODES,
		MNPLAYERS1PBONUS_BANK_PORTRAITS,
		MNPLAYERS1PBONUS_BANK_1PMODE,
		MNPLAYERS1PBONUS_BANK_DIFFICULTY,
		MNPLAYERS1PBONUS_BANK_STOCKS,
		MNPLAYERS1PBONUS_BANK_FONTS,
		MNPLAYERS1PBONUS_BANK_DIGITS,
		NULL
	};
	s32 i;

	for (i = 0; i < ARRAY_COUNT(dMNPlayers1PBonusFileIDs); i++)
	{
		sMNPlayers1PBonusFiles[i] = NULL;

		if (paths[i] == NULL)
		{
			continue;
		}
		if (sprite_bank_load(&sMNPlayers1PBonusBanks[i], paths[i]) < 0)
		{
			syDebugPrintf("mnPlayers1PBonus: no bank for file %d (%s)\n",
			              (int)dMNPlayers1PBonusFileIDs[i], paths[i]);
			continue;
		}
		sMNPlayers1PBonusFiles[i] = &sMNPlayers1PBonusBanks[i];
	}
}

// 0x80136CD8
void mnPlayers1PBonusFuncStart(void)
{
	s32 unused1[2];
	s32 unused2;
	s32 i, j;

	mnPlayers1PBonusLoadFiles();

	gcMakeGObjSPAfter(nGCCommonKindPlayerSelect, mnPlayers1PBonusFuncRun, nGCCommonLinkIDPlayerSelect, GOBJ_PRIORITY_DEFAULT);
	/* CUT: gcMakeDefaultCameraGObj, the clear camera -- the PVR clears
	 * its own framebuffer, as on all three of the other selects. */
	efParticleInitAll();
	/* DIVERGES: efManagerInitEffects is the port's
	 * efManagerLoadEffectBank (src/dc/efmanager.h), as in the battle,
	 * on the results screen and on the three other selects. */
	efManagerLoadEffectBank();
	ftManagerAllocFighter(FTDATA_FLAG_SUBMOTION, 1);
	/* the port's: the SubMotion statuses alone, as the flag says --
	 * tier 0 of each fighter's .anm (ftcommon.h ftManagerSetAnmTier) */
	ftManagerSetAnmTier(FIGHTER_ANM_TIER_MENU);

	for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
	{
		ftManagerSetupFilesAllKind(i);
	}
	/* DIVERGES: gFTManagerFigatreeHeapSize is the ROM loader's, and the
	 * port has no figatree heap -- the pack plays its animations in
	 * place (src/dc/mnplayersvs.c says it first). */
	sMNPlayers1PBonusFigatreeHeap = NULL;

	mnPlayers1PBonusInitVars();
	mnPlayers1PBonusMakePortraitCamera();
	mnPlayers1PBonusMakeCursorCamera();
	mnPlayers1PBonusMakePuckCamera();
	mnPlayers1PBonusMakeGateCamera();
	mnPlayers1PBonusMakeFighterCamera();
	mnPlayers1PBonusMakePortraitWallpaperCamera();
	mnPlayers1PBonusMakePortraitFlashCamera();
	mnPlayers1PBonusMakeWallpaperCamera();
	mnPlayers1PBonusMakeLabelsCamera();
	mnPlayers1PBonusMakeReadyCamera();
	mnPlayers1PBonusMakeWallpaper();
	mnPlayers1PBonusMakePortraitAll();
	mnPlayers1PBonusInitSlot(sMNPlayers1PBonusManPlayer);
	mnPlayers1PBonusMakeLabels();

	if (mnPlayers1PBonusCheckBonusCompleteAll() != FALSE)
	{
		mnPlayers1PBonusMakeTotalTime();
	}
	mnPlayers1PBonusMakePuckAdjust();
	mnPlayers1PBonusMakeSpotlight();
	mnPlayers1PBonusMakeReady();
	scSubsysFighterSetLightParams(45.0F, 45.0F, 0xFF, 0xFF, 0xFF, 0xFF);

	if (gSCManagerSceneData.scene_prev != nSCKindMaps)
	{
		syAudioPlayBGM(0, nSYAudioBGMBattleSelect);
	}
}

// 0x80137530
/* CUT: dMNPlayers1PBonusVideoSetup, the N64's video mode --
 * mnPlayers1PBonusStartScene says why. */

// 0x8013754C
SYTaskmanSetup dMNPlayers1PBonusTaskmanSetup =
{
    // Task Manager Buffer Setup
    {
        0,                          // ???
        gcRunAll,              		// Update function
        gcDrawAll,        			// Frame draw function
        NULL,                       // Allocatable memory pool start
        0,                          // Allocatable memory pool size
        1,                          // ???
        2,                          // Number of contexts?
        sizeof(Gfx) * 2375,         // Display List Buffer 0 Size
        sizeof(Gfx) * 64,          	// Display List Buffer 1 Size
        0,                          // Display List Buffer 2 Size
        0,                          // Display List Buffer 3 Size
        0x8000,                     // Graphics Heap Size
        2,                          // ???
        0x8000,                     // RDP Output Buffer Size
        NULL,                       // Pre-render function (cut, above)
        syControllerFuncRead,       // Controller I/O function
    },

    0,                              // Number of GObjThreads
    sizeof(u64) * 32,              	// Thread stack size
    0,                              // Number of thread stacks
    0,                              // ???
    0,                              // Number of GObjProcesses
    0,                              // Number of GObjs
    sizeof(GObj),                   // GObj size
    0,                              // Number of XObjs
    NULL,                           // Matrix function list (waits for
                                    // the matrix stack, as every
                                    // other ported scene's)
    NULL,                           // DObjVec eject function
    0,                              // Number of AObjs
    0,                              // Number of MObjs
    0,                              // Number of DObjs
    sizeof(DObj),                   // DObj size
    0,                              // Number of SObjs
    sizeof(SObj),                   // SObj size
    0,                              // Number of CObjs
    sizeof(CObj),                 	// CObj size
    
    mnPlayers1PBonusFuncStart      	// Task start function
};

// 0x80136EF4
void mnPlayers1PBonusStartScene(void)
{
	/* DIVERGES as every ported scene's: syVideoInit and the z-buffer
	 * are the N64's video mode, set once at boot here, and the
	 * arena_size line is the link map. What is left is the last
	 * line -- scManagerFuncUpdate, as the three other selects'. */
	scManagerFuncUpdate(&dMNPlayers1PBonusTaskmanSetup);
}

/* The bzero arm of syDmaLoadOverlay for overlay 29, this file:
 * sc/scmanager.c calls it on the way into the scene. src/dc/overlay.h
 * says why the port needs it written out. */
void mnPlayers1PBonusOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNPlayers1PBonusBanks);
    OVERLAY_CLEAR(sMNPlayers1PBonusBonusKind);
    OVERLAY_CLEAR(sMNPlayers1PBonusDevicesConnected);
    OVERLAY_CLEAR(sMNPlayers1PBonusFigatreeHeap);
    OVERLAY_CLEAR(sMNPlayers1PBonusFighterMask);
    OVERLAY_CLEAR(sMNPlayers1PBonusFiles);
    OVERLAY_CLEAR(sMNPlayers1PBonusGameModeGObj);
    OVERLAY_CLEAR(sMNPlayers1PBonusGameRules);
    OVERLAY_CLEAR(sMNPlayers1PBonusHiScoreGObj);
    OVERLAY_CLEAR(sMNPlayers1PBonusIsSelected);
    OVERLAY_CLEAR(sMNPlayers1PBonusIsTeamBattle);
    OVERLAY_CLEAR(sMNPlayers1PBonusManPlayer);
    OVERLAY_CLEAR(sMNPlayers1PBonusPad0x80137640);
    OVERLAY_CLEAR(sMNPlayers1PBonusPad0x801376D0);
    OVERLAY_CLEAR(sMNPlayers1PBonusPad0x80137700);
    OVERLAY_CLEAR(sMNPlayers1PBonusPad0x80137730);
    OVERLAY_CLEAR(sMNPlayers1PBonusReadyBlinkWait);
    OVERLAY_CLEAR(sMNPlayers1PBonusReturnTic);
    OVERLAY_CLEAR(sMNPlayers1PBonusSlot);
    OVERLAY_CLEAR(sMNPlayers1PBonusSpotlightModel);
    OVERLAY_CLEAR(sMNPlayers1PBonusSpotlightPack);
    OVERLAY_CLEAR(sMNPlayers1PBonusStartWait);
    OVERLAY_CLEAR(sMNPlayers1PBonusTotalTimeGObj);
    OVERLAY_CLEAR(sMNPlayers1PBonusTotalTimeTics);
    OVERLAY_CLEAR(sMNPlayers1PBonusUnknown0x801376D4);
}
