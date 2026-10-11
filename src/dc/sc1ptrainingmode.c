/* sc1ptrainingmode.c -- see sc1ptrainingmode.h. Every function is
 * sc/sc1pmode/sc1ptrainingmode.c's by name and body unless marked
 * DIVERGES; the line numbers are the decomp's. The scene's shape --
 * a battle under a pause menu -- is src/dc/scvsbattle.c's, and every
 * cut it shares with that file says so. */
#include "sc1ptrainingmode.h"
#include "overlay.h"

#include "ftcommon.h"
#include "gmcamera.h"
#include "gmcommon.h"
#include "ifcommon.h"
#include "lbcommon.h"
#include "scmanager.h"
#include "sprite.h"
#include "stage.h"
#include "bgm.h"
#include "dma.h"
#include "efmanager.h"
#include "itemmodel.h"
#include "wpattrs.h"
#include "ftdisplaymain.h"
#include "input.h"
#include "taskman.h"
#include "assetroot.h"

#include <ef/efparticle.h>
#include <ft/fighter.h>
#include <ft/ftdef.h>
#include <ft/ftparam.h>
#include <ft/ftpublic.h>
#include <gm/gmrumble.h>
#include <gm/gmsound.h>
#include <gr/ground.h>
#include <if/ifcommon.h>
#include <if/interface.h>
#include <if/ifscreenflash.h>
#include <wp/wpmanager.h>
#include <it/item.h>
#include <mp/map.h>
#include <sys/controller.h>
#include <lb/lbfade.h>
#include <macros.h>

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/sysshim.c defines it over the FGM engine (its partner,
 * func_800266A0_272A0, is in ftcommon.h). */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* the decomp's share of unused locals */
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

/* ---- the one bank ---------------------------------------------------
 *
 * The screen's text is 84 sprites of relocData file 29 IFCommonItemNames
 * -- the shared glyph atlas -- named by file 254's four layout tables.
 * The port exports those 84 into one bank and keeps the tables in C
 * below, each entry holding the sprite's offset where the game held a
 * resolved pointer; sc1PTrainingModeLoadSprites turns one into the
 * other. */
#define SC1PTRAININGMODE_BANK "sc1ptrain.spr"

static SpriteBank sSC1PTrainingModeBank;

/* One row of file 254's SC1PTrainingModeSprites tables (sc/sctypes.h:117)
 * before the bank resolves it: the same Vec2h, and the sprite's offset
 * where the struct carries a Sprite *. */
typedef struct SC1PTrainingModeSpritesOff
{
    Vec2h pos;
    u32 offset;

} SC1PTrainingModeSpritesOff;

/* The 84 sprites of relocData file 29 IFCommonItemNames that this
 * screen's four layout tables name, as src/dc/decomp/reloc_data.us.h
 * reads their labels off the ROM. */
#define llIFCommonItemNamesArrowLSprite             0x10890
#define llIFCommonItemNamesArrowRSprite             0x10940
#define llIFCommonItemNamesAttackTextSprite         0x06c38
#define llIFCommonItemNamesAttackText2Sprite        0x0d7e8
#define llIFCommonItemNamesBeamSwordTextSprite      0x01f58
#define llIFCommonItemNamesBeamSwordText2Sprite     0x09048
#define llIFCommonItemNamesBobOmbTextSprite         0x043d8
#define llIFCommonItemNamesBobOmbText2Sprite        0x0b158
#define llIFCommonItemNamesBoxBottomSprite          0x107e0
#define llIFCommonItemNamesBoxLeftSprite            0x10060
#define llIFCommonItemNamesBoxRightSprite           0x105b0
#define llIFCommonItemNamesBoxTopSprite             0x0fb10
#define llIFCommonItemNamesBumperTextSprite         0x04738
#define llIFCommonItemNamesBumperText2Sprite        0x0b538
#define llIFCommonItemNamesCPColonTextSprite        0x0db90
#define llIFCommonItemNamesCloseUpTextSprite        0x0f588
#define llIFCommonItemNamesClosingBracketSprite     0x06df8
#define llIFCommonItemNamesComboColonTextSprite     0x07698
#define llIFCommonItemNamesDamageColonTextSprite    0x07338
#define llIFCommonItemNamesDigit0Sprite             0x00098
#define llIFCommonItemNamesDigit1Sprite             0x00178
#define llIFCommonItemNamesDigit2Sprite             0x002d8
#define llIFCommonItemNamesDigit3Sprite             0x003b8
#define llIFCommonItemNamesDigit4Sprite             0x00518
#define llIFCommonItemNamesDigit5Sprite             0x00678
#define llIFCommonItemNamesDigit6Sprite             0x007d8
#define llIFCommonItemNamesDigit7Sprite             0x00938
#define llIFCommonItemNamesDigit8Sprite             0x00a98
#define llIFCommonItemNamesDigit9Sprite             0x00bf8
#define llIFCommonItemNamesEnemyColonTextSprite     0x079f8
#define llIFCommonItemNamesEvadeTextSprite          0x065f8
#define llIFCommonItemNamesEvadeText2Sprite         0x0d1a0
#define llIFCommonItemNamesExitTextSprite           0x0dec8
#define llIFCommonItemNamesFanTextSprite            0x02698
#define llIFCommonItemNamesFanText2Sprite           0x09720
#define llIFCommonItemNamesFireFlowerTextSprite     0x033b8
#define llIFCommonItemNamesFireFlowerText2Sprite    0x0a318
#define llIFCommonItemNamesGreenShellTextSprite     0x04c98
#define llIFCommonItemNamesGreenShellText2Sprite    0x0ba18
#define llIFCommonItemNamesHammerTextSprite         0x03798
#define llIFCommonItemNamesHammerText2Sprite        0x0a6f8
#define llIFCommonItemNamesHeartTextSprite          0x01798
#define llIFCommonItemNamesHeartText2Sprite         0x088d8
#define llIFCommonItemNamesHomerunBatTextSprite     0x024b8
#define llIFCommonItemNamesHomerunBatText2Sprite    0x09558
#define llIFCommonItemNamesItemColonTextSprite      0x0e270
#define llIFCommonItemNamesJumpTextSprite           0x068d8
#define llIFCommonItemNamesJumpText2Sprite          0x0d488
#define llIFCommonItemNamesMaximTomatoTextSprite    0x014b8
#define llIFCommonItemNamesMaximTomatoText2Sprite   0x085f8
#define llIFCommonItemNamesMotionSensorBombSprite   0x03ff8
#define llIFCommonItemNamesMotionSensorBomb2Sprite  0x0ad30
#define llIFCommonItemNamesNoneTextSprite           0x00e58
#define llIFCommonItemNamesNoneText2Sprite          0x07f90
#define llIFCommonItemNamesNormalTextSprite         0x0f8e8
#define llIFCommonItemNamesOneSlashFourTextSprite   0x05d58
#define llIFCommonItemNamesOneSlashFourText2Sprite  0x0c928
#define llIFCommonItemNamesOneSlashOneTextSprite    0x05738
#define llIFCommonItemNamesOneSlashOneText2Sprite   0x0c388
#define llIFCommonItemNamesOneSlashTwoTextSprite    0x05918
#define llIFCommonItemNamesOneSlashTwoText2Sprite   0x0c568
#define llIFCommonItemNamesOpeningBracketSprite     0x06d18
#define llIFCommonItemNamesPercentageSignSprite     0x06f58
#define llIFCommonItemNamesPokeballTextSprite       0x05558
#define llIFCommonItemNamesPokeballText2Sprite      0x0c1a0
#define llIFCommonItemNamesRayGunTextSprite         0x02e58
#define llIFCommonItemNamesRayGunText2Sprite        0x09e38
#define llIFCommonItemNamesRedShellTextSprite       0x050f8
#define llIFCommonItemNamesRedShellText2Sprite      0x0bdf8
#define llIFCommonItemNamesRedSphereSprite          0x0e6d8
#define llIFCommonItemNamesResetTextSprite          0x0ea08
#define llIFCommonItemNamesSpeedColonTextSprite     0x07cd8
#define llIFCommonItemNamesSpeedTextSprite          0x0edb0
#define llIFCommonItemNamesStandTextSprite          0x06038
#define llIFCommonItemNamesStandText2Sprite         0x0cc08
#define llIFCommonItemNamesStarRodTextSprite        0x02af8
#define llIFCommonItemNamesStarRodText2Sprite       0x09b08
#define llIFCommonItemNamesStarTextSprite           0x019f8
#define llIFCommonItemNamesStarText2Sprite          0x08b38
#define llIFCommonItemNamesTwoSlashThreeTextSprite  0x05b78
#define llIFCommonItemNamesTwoSlashThreeText2Sprite 0x0c748
#define llIFCommonItemNamesViewColonTextSprite      0x0f160
#define llIFCommonItemNamesWalkTextSprite           0x06318
#define llIFCommonItemNamesWalkText2Sprite          0x0cee8

static SC1PTrainingModeSpritesOff dSC1PTrainingModeDisplayLabelPosSprites[4] =
{
	{ { 20, 20 }, llIFCommonItemNamesDamageColonTextSprite },
	{ { 20, 36 }, llIFCommonItemNamesComboColonTextSprite },
	{ { 148, 20 }, llIFCommonItemNamesEnemyColonTextSprite },
	{ { 236, 20 }, llIFCommonItemNamesSpeedColonTextSprite },
};

static const u32 dSC1PTrainingModeDisplayOptionSprites[39] =
{
	llIFCommonItemNamesDigit0Sprite,
	llIFCommonItemNamesDigit1Sprite,
	llIFCommonItemNamesDigit2Sprite,
	llIFCommonItemNamesDigit3Sprite,
	llIFCommonItemNamesDigit4Sprite,
	llIFCommonItemNamesDigit5Sprite,
	llIFCommonItemNamesDigit6Sprite,
	llIFCommonItemNamesDigit7Sprite,
	llIFCommonItemNamesDigit8Sprite,
	llIFCommonItemNamesDigit9Sprite,
	llIFCommonItemNamesNoneTextSprite,
	llIFCommonItemNamesMaximTomatoTextSprite,
	llIFCommonItemNamesHeartTextSprite,
	llIFCommonItemNamesStarTextSprite,
	llIFCommonItemNamesBeamSwordTextSprite,
	llIFCommonItemNamesHomerunBatTextSprite,
	llIFCommonItemNamesFanTextSprite,
	llIFCommonItemNamesStarRodTextSprite,
	llIFCommonItemNamesRayGunTextSprite,
	llIFCommonItemNamesFireFlowerTextSprite,
	llIFCommonItemNamesHammerTextSprite,
	llIFCommonItemNamesMotionSensorBombSprite,
	llIFCommonItemNamesBobOmbTextSprite,
	llIFCommonItemNamesBumperTextSprite,
	llIFCommonItemNamesGreenShellTextSprite,
	llIFCommonItemNamesRedShellTextSprite,
	llIFCommonItemNamesPokeballTextSprite,
	llIFCommonItemNamesOneSlashOneTextSprite,
	llIFCommonItemNamesTwoSlashThreeTextSprite,
	llIFCommonItemNamesOneSlashTwoTextSprite,
	llIFCommonItemNamesOneSlashFourTextSprite,
	llIFCommonItemNamesStandTextSprite,
	llIFCommonItemNamesWalkTextSprite,
	llIFCommonItemNamesEvadeTextSprite,
	llIFCommonItemNamesJumpTextSprite,
	llIFCommonItemNamesAttackTextSprite,
	llIFCommonItemNamesOpeningBracketSprite,
	llIFCommonItemNamesClosingBracketSprite,
	llIFCommonItemNamesPercentageSignSprite,
};

static SC1PTrainingModeSpritesOff dSC1PTrainingModeMenuLabelPosSprites[10] =
{
	{ { 86, 65 }, llIFCommonItemNamesCPColonTextSprite },
	{ { 86, 85 }, llIFCommonItemNamesItemColonTextSprite },
	{ { 86, 105 }, llIFCommonItemNamesSpeedTextSprite },
	{ { 86, 125 }, llIFCommonItemNamesViewColonTextSprite },
	{ { 86, 143 }, llIFCommonItemNamesResetTextSprite },
	{ { 86, 165 }, llIFCommonItemNamesExitTextSprite },
	{ { 64, 43 }, llIFCommonItemNamesBoxTopSprite },
	{ { 64, 47 }, llIFCommonItemNamesBoxLeftSprite },
	{ { 253, 47 }, llIFCommonItemNamesBoxRightSprite },
	{ { 64, 198 }, llIFCommonItemNamesBoxBottomSprite },
};

static const u32 dSC1PTrainingModeMenuOptionSprites[31] =
{
	llIFCommonItemNamesNoneText2Sprite,
	llIFCommonItemNamesMaximTomatoText2Sprite,
	llIFCommonItemNamesHeartText2Sprite,
	llIFCommonItemNamesStarText2Sprite,
	llIFCommonItemNamesBeamSwordText2Sprite,
	llIFCommonItemNamesHomerunBatText2Sprite,
	llIFCommonItemNamesFanText2Sprite,
	llIFCommonItemNamesStarRodText2Sprite,
	llIFCommonItemNamesRayGunText2Sprite,
	llIFCommonItemNamesFireFlowerText2Sprite,
	llIFCommonItemNamesHammerText2Sprite,
	llIFCommonItemNamesMotionSensorBomb2Sprite,
	llIFCommonItemNamesBobOmbText2Sprite,
	llIFCommonItemNamesBumperText2Sprite,
	llIFCommonItemNamesGreenShellText2Sprite,
	llIFCommonItemNamesRedShellText2Sprite,
	llIFCommonItemNamesPokeballText2Sprite,
	llIFCommonItemNamesOneSlashOneText2Sprite,
	llIFCommonItemNamesTwoSlashThreeText2Sprite,
	llIFCommonItemNamesOneSlashTwoText2Sprite,
	llIFCommonItemNamesOneSlashFourText2Sprite,
	llIFCommonItemNamesStandText2Sprite,
	llIFCommonItemNamesWalkText2Sprite,
	llIFCommonItemNamesEvadeText2Sprite,
	llIFCommonItemNamesJumpText2Sprite,
	llIFCommonItemNamesAttackText2Sprite,
	llIFCommonItemNamesCloseUpTextSprite,
	llIFCommonItemNamesNormalTextSprite,
	llIFCommonItemNamesArrowLSprite,
	llIFCommonItemNamesArrowRSprite,
	llIFCommonItemNamesRedSphereSprite,
};

/* what sc1PTrainingModeLoadSprites fills, and what the decomp's
 * SC1PTrainingModeMenu pointers are then aimed at */
static SC1PTrainingModeSprites sSC1PTrainingModeDisplayLabel[ARRAY_COUNT(dSC1PTrainingModeDisplayLabelPosSprites)];
static SC1PTrainingModeSprites sSC1PTrainingModeMenuLabel[ARRAY_COUNT(dSC1PTrainingModeMenuLabelPosSprites)];
static Sprite *sSC1PTrainingModeDisplayOption[ARRAY_COUNT(dSC1PTrainingModeDisplayOptionSprites)];
static Sprite *sSC1PTrainingModeMenuOption[ARRAY_COUNT(dSC1PTrainingModeMenuOptionSprites)];

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

// 0x80190770
s32 dSC1PTrainingModeUnknown0x80190770[/* */] =
{
	 0,  0,
	 4,  5,
	 6,  7,
	 8,  9,
	10, 11,
	12, 13,
	14, 15,
	16, 17,
	18, 19
};

/* CUT: sc1ptrainingmode.c:32-38 dSC1PTrainingModeWallpaperHeapOffsets,
 * nine copies of the one reloc-heap offset the training wallpaper was
 * forced back into -- the port's three wallpapers are sprite banks
 * (sc1PTrainingModeLoadWallpaper), and a bank has no heap slot. */

// 0x801907DC
u16 dSC1PTrainingModeDamagePositionsX[/* */] = { 75, 85, 95 };

// 0x801907E4
u8 dSC1PTrainingModeDamageUnitLengths[/* */] = { 100, 10, 1 };

// 0x801907E8
u16 dSC1PTrainingModeComboPositionsX[/* */] = { 69, 79 };

// 0x801907EC
u8 dSC1PTrainingModeComboUnitLengths[/* */] = { 10, 1 };

// 0x801907F0
sb32 (*dSC1PTrainingModeMenuUpdateFuncList[/* */])(void) =
{
	sc1PTrainingModeUpdateCPOption,
	sc1PTrainingModeUpdateItemOption,
	sc1PTrainingModeUpdateSpeedOption,
	sc1PTrainingModeUpdateViewOption,
	sc1PTrainingModeUpdateResetOption,
	sc1PTrainingModeUpdateExitOption
};

// 0x80190808
s32 dSC1PTrainingModeDummyBehaviors[/* */] =
{
	nFTComputerBehaviorStand,
	nFTComputerBehaviorWalk,
	nFTComputerBehaviorEvade,
	nFTComputerBehaviorJump,
	nFTComputerBehaviorDefault
};

u8 dSC1PTrainingModeLagIntervals[/* */][2] =
{
	{ 0, 0 }, { 1, 1 },
	{ 0, 1 }, { 0, 3 }
};

/* The link labels written out: llGRWallpaperTraining{Black,Yellow,
 * Blue}FileID are relocData 26, 27 and 28, and each file's one sprite,
 * llGRWallpaperTraining*Sprite, is at 0x20718 (the files' .spritelist). */
// 0x80190824
SC1PTrainingModeFiles dSC1PTrainingModeWallpaperDescs[/* */] =
{
	{ 26, 0x20718, { 0x00, 0x00, 0x00 } },
	{ 27, 0x20718, { 0xEE, 0x9E, 0x06 } },
	{ 28, 0x20718, { 0xAF, 0xF5, 0xFF } }
};

// 0x80190848
s32 dSC1PTrainingModeWallpaperIDs[/* */] =
{
	2,	// Peach's Castle
	0, 	// Sector Z
	0, 	// Kongo Jungle
	0, 	// Planet Zebes
	2, 	// Hyrule Castle
	1, 	// Yoshi's Story
	2, 	// Dream Land
	2, 	// Saffron City
	2  	// Mushroom Kingdom
};

// 0x8019086C
SYColorRGBA dSC1PTrainingModeFadeColor = { 0x00, 0x00, 0x00, 0x00 };

/* CUT: sc1ptrainingmode.c:100 dSC1PTrainingModeVideoSetup, the N64's
 * video mode -- sc1PTrainingModeStartScene says why. */

/* ---- the pools ------------------------------------------------------
 *
 * The same argument as src/dc/scvsbattle.c's, and the same numbers: the
 * decomp's own setup carries zero for every count, the real ones are
 * still in the ROM's data, and these are the port's with headroom. This
 * scene wants a little more of the interface than a VS battle does --
 * the four stat rows, the six menu labels, the six option rows, the
 * arrows, the cursor and the underline are all SObjs on GObjs of their
 * own, and they are made and unmade every time the menu opens -- so the
 * GObj, GObjProc and SObj counts are the VS battle's plus that. */
#define SC1PTRAININGMODE_GOBJS       64
#define SC1PTRAININGMODE_GOBJPROCS   64
#define SC1PTRAININGMODE_XOBJS      288
#define SC1PTRAININGMODE_AOBJS      576
#define SC1PTRAININGMODE_MOBJS       32
#define SC1PTRAININGMODE_DOBJS       96
#define SC1PTRAININGMODE_SOBJS      160
#define SC1PTRAININGMODE_COBJS        8

/* sc1ptrainingmode.c:103-146. DIVERGES exactly as
 * dSCVSBattleTaskmanSetup does: the display-list buffers, the graphics
 * arena and the RDP output buffer are zeroed (no RSP, no RDP),
 * func_lights is NULL (the cut below), arena_start is NULL so
 * syTaskmanStartTask keeps the region syTaskmanMakeGeneralHeap already
 * made rather than following &ovl7_BSS_END, a link-map address with no
 * meaning here, and the matrix function list waits for the matrix
 * stack. */
// 0x8019088C
SYTaskmanSetup dSC1PTrainingModeTaskmanSetup =
{
    // Task Manager Buffer Setup
    {
        0,                          // ???
        sc1PTrainingModeFuncUpdate, // Update function
        scManagerFuncDraw,          // Frame draw function
        NULL,                       // Allocatable memory pool start
        0,                          // Allocatable memory pool size
        1,                          // ???
        2,                          // Number of contexts?
        0, 0, 0, 0,                 // the four DL buffer sizes
        0,                          // Graphics Heap Size
        2,                          // ???
        0,                          // RDP Output Buffer Size
        NULL,                       // Pre-render function
        syControllerFuncRead,       // Controller I/O function
    },

    0,                              // Number of GObjThreads
    sizeof(u64) * 192,              // Thread stack size
    0,                              // Number of thread stacks
    0,                              // ???
    SC1PTRAININGMODE_GOBJPROCS,
    SC1PTRAININGMODE_GOBJS,   sizeof(GObj),
    SC1PTRAININGMODE_XOBJS,
    NULL,                           // Matrix function list
    NULL,                           // DObjVec eject function
    SC1PTRAININGMODE_AOBJS,
    SC1PTRAININGMODE_MOBJS,
    SC1PTRAININGMODE_DOBJS,   sizeof(DObj),
    SC1PTRAININGMODE_SOBJS,   sizeof(SObj),
    SC1PTRAININGMODE_COBJS,   sizeof(CObj),

    sc1PTrainingModeFuncStart     	// Task start function
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x80190960
s32 sSC1PTrainingModePad0x80190960[2];

// 0x80190968
SCBattleState sSC1PTrainingModeBattleState;

// 0x80190B58
SC1PTrainingModeMenu sSC1PTrainingModeMenu;

/* CUT: sc1ptrainingmode.c:169-175, the reloc loader's two status
 * buffers -- sc1PTrainingModeSetupFiles, their only reader, says why. */

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

// 0x8018D0C0
void sc1PTrainingModeSetMenuGObjFlags(u32 flags)
{
	GObj *pause_gobj = gGCCommonLinks[nGCCommonLinkIDPauseMenu];

	while (pause_gobj != NULL)
	{
		pause_gobj->flags = flags;
		pause_gobj = pause_gobj->link_next;
	}
}

// 0x8018D0E8
void sc1PTrainingModeCheckEnterMenu(void)
{
	s32 player = gSCManagerSceneData.player;

	if (gSYControllerDevices[player].button_tap & START_BUTTON)
	{
		GObj *fighter_gobj = gSCManagerBattleState->players[player].fighter_gobj;
		FTStruct *fp = ftGetStruct(fighter_gobj);

		if (!(fp->is_menu_ignore))
		{
			ifCommonInterfaceSetGObjFlagsAll(GOBJ_FLAG_HIDDEN);
			sc1PTrainingModeSetMenuGObjFlags(GOBJ_FLAG_NONE);
			gmRumbleInitPlayers();
			ftParamLockPlayerControl(gSCManagerBattleState->players[player].fighter_gobj);
			ftParamLockPlayerControl(gSCManagerBattleState->players[sSC1PTrainingModeMenu.dummy].fighter_gobj);

			gSCManagerBattleState->game_status = nSCBattleGameStatusPause;

			func_800269C0_275C0(nSYAudioFGMGamePause);
			syAudioSetBGMVolume(0, 0x3C00);

			sSC1PTrainingModeMenu.is_read_menu_inputs = FALSE;
		}
	}
}

// 0x8018D1F0
void sc1PTrainingModeCheckLeaveMenu(void)
{
	s32 player = gSCManagerSceneData.player;
	GObj *fighter_gobj;

	if (gSYControllerDevices[player].button_tap & (B_BUTTON | START_BUTTON))
	{
		ifCommonInterfaceSetGObjFlagsAll(GOBJ_FLAG_NONE);
		sc1PTrainingModeSetMenuGObjFlags(GOBJ_FLAG_HIDDEN);

		gSCManagerBattleState->game_status = nSCBattleGameStatusGo;

		ftParamUnlockPlayerControl(gSCManagerBattleState->players[sSC1PTrainingModeMenu.dummy].fighter_gobj);
		fighter_gobj = gSCManagerBattleState->players[player].fighter_gobj;
		ftParamUnlockPlayerControl(fighter_gobj);

		if (gSYControllerDevices[player].button_tap & B_BUTTON)
		{
			FTStruct *fp = ftGetStruct(fighter_gobj);

			fp->input.pl.button_hold |= B_BUTTON;
		}
		syAudioSetBGMVolume(0, 0x7800);
	}
}

// 0x8018D2F0
void sc1PTrainingModeUpdateMenuInputs(void)
{
	u16 buttons = 0;
	s32 player = gSCManagerSceneData.player;

	if (gSYControllerDevices[player].stick_range.x > 40)
	{
		buttons |= R_JPAD;
	}
	if (gSYControllerDevices[player].stick_range.x < -40)
	{
		buttons |= L_JPAD;
	}
	if (gSYControllerDevices[player].stick_range.y > 40)
	{
		buttons |= U_JPAD;
	}
	if (gSYControllerDevices[player].stick_range.y < -40)
	{
		buttons |= D_JPAD;
	}
	if (sSC1PTrainingModeMenu.is_read_menu_inputs == FALSE)
	{
		if (!(buttons))
		{
			sSC1PTrainingModeMenu.is_read_menu_inputs = TRUE;
		}
	}
	else
	{
		sSC1PTrainingModeMenu.button_tap = (buttons ^ sSC1PTrainingModeMenu.button_hold) & buttons;

		if (buttons ^ sSC1PTrainingModeMenu.button_hold)
		{
			sSC1PTrainingModeMenu.button_queue = sSC1PTrainingModeMenu.button_tap;
			sSC1PTrainingModeMenu.rapid_scroll_wait = 30;
		}
		else
		{
			sSC1PTrainingModeMenu.rapid_scroll_wait--;

			if (sSC1PTrainingModeMenu.rapid_scroll_wait > 0)
			{
				sSC1PTrainingModeMenu.button_queue = 0;
			}
			else
			{
				sSC1PTrainingModeMenu.button_queue = buttons;
				sSC1PTrainingModeMenu.rapid_scroll_wait = 5;
			}
		}
		sSC1PTrainingModeMenu.button_hold = buttons;
	}
}

// 0x8018D3DC
void sc1PTrainingModeUpdateScroll(void)
{
	sc1PTrainingModeUpdateOptionArrows();
	sc1PTrainingModeUpdateUnderline();
	func_800269C0_275C0(nSYAudioFGMMenuScroll2);
#if defined(REGION_JP)
	func_ovl7_8018F874();
	func_ovl7_8018FA54();
#endif
}

// 0x8018D40C
sb32 sc1PTrainingModeCheckUpdateOptionID(s32 *option, s32 option_min, s32 option_max)
{
	if (sSC1PTrainingModeMenu.button_queue & (L_JPAD | R_JPAD))
	{
		if (sSC1PTrainingModeMenu.button_queue & L_JPAD)
		{
			if (--(*option) < option_min)
			{
				*option = option_max - 1;
				return TRUE;
			}
		}
		else if (++(*option) >= option_max)
		{
			*option = option_min;
		}
		return TRUE;
	}
	else return FALSE;
}

// 0x8018D478
sb32 sc1PTrainingModeUpdateCPOption(void)
{
	if (sc1PTrainingModeCheckUpdateOptionID(&sSC1PTrainingModeMenu.cp_menu_option, nSC1PTrainingModeMenuCPEnumStart, nSC1PTrainingModeMenuCPEnumCount) != FALSE)
	{
		sc1PTrainingModeUpdateDummyBehavior();
		sc1PTrainingModeUpdateCPDisplaySprite();
		sc1PTrainingModeUpdateCPOptionSprite();
		sc1PTrainingModeUpdateScroll();
#if defined(REGION_JP)
		func_ovl7_8018FA54();
#endif
	}
	return FALSE;
}

// 0x8018D4D0
s32 sc1PTrainingModeGetItemCount(void)
{
	GObj *item_gobj = gGCCommonLinks[nGCCommonLinkIDItem];
	s32 item_count;

	for (item_count = 0; item_gobj != NULL; item_gobj = item_gobj->link_next)
	{
		if
		(
			(itGetStruct(item_gobj)->kind <= nITKindCommonEnd) ||
			(itGetStruct(item_gobj)->kind >= nITKindMBallMonsterStart)
		)
		{
			item_count++;
		}
	}
	return item_count;
}

// 0x8018D518
sb32 sc1PTrainingModeUpdateItemOption(void)
{
	Vec3f pos;
	Vec3f vel;

	if (sc1PTrainingModeCheckUpdateOptionID(&sSC1PTrainingModeMenu.item_menu_option, nSC1PTrainingModeMenuItemEnumStart, nSC1PTrainingModeMenuItemEnumCount) != FALSE)
	{
		sc1PTrainingModeUpdateItemOptionSprite();
		sc1PTrainingModeUpdateScroll();
#if defined(REGION_JP)
		func_ovl7_8018FA54();
#endif
	}
	if (sSC1PTrainingModeMenu.item_spawn_wait == 0)
	{
		if
		(
			(gSYControllerDevices[gSCManagerSceneData.player].button_tap & A_BUTTON) &&
			(sSC1PTrainingModeMenu.item_menu_option != nSC1PTrainingModeMenuItemNone)
		)
		{
			if (sc1PTrainingModeGetItemCount() < 4)
			{
				vel.x = vel.z = 0.0F;
				vel.y = 30.0F;

				pos = DObjGetStruct(gSCManagerBattleState->players[gSCManagerSceneData.player].fighter_gobj)->translate.vec.f;

				pos.y += 200.0F;
				pos.z = 0.0F;

				itManagerMakeItemSetupCommon(NULL, sSC1PTrainingModeMenu.item_menu_option + (nITKindUtilityStart - 1), &pos, &vel, ITEM_FLAG_PARENT_DEFAULT);
				func_800269C0_275C0(nSYAudioFGMMenuSelect);
				sSC1PTrainingModeMenu.item_spawn_wait = 8;
			}
			else func_800269C0_275C0(nSYAudioFGMMenuDenied);
		}
	}
	else sSC1PTrainingModeMenu.item_spawn_wait--;

	return FALSE;
}

// 0x8018D684
sb32 sc1PTrainingModeUpdateSpeedOption(void)
{
	if (sc1PTrainingModeCheckUpdateOptionID(&sSC1PTrainingModeMenu.speed_menu_option, nSC1PTrainingModeMenuSpeedEnumStart, nSC1PTrainingModeMenuSpeedEnumCount) != FALSE)
	{
		sSC1PTrainingModeMenu.lagtic_wait = sSC1PTrainingModeMenu.frameadvance_wait = 0;

		sc1PTrainingModeUpdateSpeedDisplaySprite();
		sc1PTrainingModeUpdateSpeedOptionSprite();
		sc1PTrainingModeUpdateScroll();
#if defined(REGION_JP)
		func_ovl7_8018FA54();
#endif
	}
	return FALSE;
}

// 0x8018D6DC
sb32 sc1PTrainingModeUpdateViewOption(void)
{
	if (sc1PTrainingModeCheckUpdateOptionID(&sSC1PTrainingModeMenu.view_menu_option, nSC1PTrainingModeMenuViewEnumStart, nSC1PTrainingModeMenuViewEnumCount) != FALSE)
	{
		if (sSC1PTrainingModeMenu.view_menu_option == nSC1PTrainingModeMenuViewNormal)
		{
			gmCameraSetStatusDefault();
			sSC1PTrainingModeMenu.magnify_wait = 180;
		}
		else
		{
			GObj *fighter_gobj = gSCManagerBattleState->players[gSCManagerSceneData.player].fighter_gobj;

			gmCameraSetStatusPlayerZoom(fighter_gobj, 0.0F, 0.0F, ftGetStruct(fighter_gobj)->attr->closeup_camera_zoom, 0.1F, 28.0F);

			gIFCommonPlayerInterface.is_magnify_display = FALSE;
			sSC1PTrainingModeMenu.magnify_wait = 0;
		}
		sc1PTrainingModeUpdateViewOptionSprite();
		sc1PTrainingModeUpdateScroll();
#if defined(REGION_JP)
		func_ovl7_8018FA54();
#endif
	}
	return FALSE;
}

// 0x8018D7B8
sb32 sc1PTrainingModeUpdateResetOption(void)
{
	if (gSYControllerDevices[gSCManagerSceneData.player].button_tap & A_BUTTON)
	{
		sSC1PTrainingModeMenu.exit_or_reset = 1;

		func_800266A0_272A0();
		func_800269C0_275C0(nSYAudioFGMTrainingSel2);
		syAudioSetBGMVolume(0, 0x7800);
		syTaskmanSetLoadScene();

		return TRUE;
	}
	else return FALSE;
}

// 0x8018D830
sb32 sc1PTrainingModeUpdateExitOption(void)
{
	if (gSYControllerDevices[gSCManagerSceneData.player].button_tap & A_BUTTON)
	{
		func_800266A0_272A0();
		func_800269C0_275C0(nSYAudioFGMTrainingSel2);
		syTaskmanSetLoadScene();

		return TRUE;
	}
	else return FALSE;
}

// 0x8018D898
void sc1PTrainingModeUpdateMainOption(void)
{
	if (sSC1PTrainingModeMenu.button_queue & (U_JPAD | D_JPAD))
	{
		if (sSC1PTrainingModeMenu.button_queue & U_JPAD)
		{
			if (--sSC1PTrainingModeMenu.main_menu_option < nSC1PTrainingModeMenuMainEnumStart)
			{
				sSC1PTrainingModeMenu.main_menu_option = nSC1PTrainingModeMenuMainEnumCount - 1;
			}
		}
		else if (++sSC1PTrainingModeMenu.main_menu_option >= nSC1PTrainingModeMenuMainEnumCount)
		{
			sSC1PTrainingModeMenu.main_menu_option = nSC1PTrainingModeMenuMainEnumStart;
		}
		sc1PTrainingModeUpdateCursorPosition();
		sc1PTrainingModeUpdateScroll();
		func_800269C0_275C0(nSYAudioFGMMenuScroll2);
	}
}

// 0x8018D91C
void sc1PTrainingModeUpdateMenu(void)
{
	sc1PTrainingModeUpdateMenuInputs();

	if (dSC1PTrainingModeMenuUpdateFuncList[sSC1PTrainingModeMenu.main_menu_option]() == FALSE)
	{
		sc1PTrainingModeUpdateMainOption();
		sc1PTrainingModeCheckLeaveMenu();
	}
}

// 0x8018D974
sb32 sc1PTrainingModeCheckLagTic(void)
{
	if (sSC1PTrainingModeMenu.lagtic_wait == 0)
	{
		if (sSC1PTrainingModeMenu.frameadvance_wait == 0)
		{
			sSC1PTrainingModeMenu.lagtic_wait = dSC1PTrainingModeLagIntervals[sSC1PTrainingModeMenu.speed_menu_option][0];
		}
		else
		{
			sSC1PTrainingModeMenu.frameadvance_wait--;
			
			return TRUE;
		}
	}
	else sSC1PTrainingModeMenu.lagtic_wait--;

	if (sSC1PTrainingModeMenu.lagtic_wait == 0)
	{
		sSC1PTrainingModeMenu.frameadvance_wait = dSC1PTrainingModeLagIntervals[sSC1PTrainingModeMenu.speed_menu_option][1];
	}
	return FALSE;
}

// 0x8018D9F0
void sc1PTrainingModeUpdateAll(void)
{
	switch (gSCManagerBattleState->game_status)
	{
	case nSCBattleGameStatusGo:
		sc1PTrainingModeCheckEnterMenu();
		break;

	case nSCBattleGameStatusPause:
		sc1PTrainingModeUpdateMenu();
		break;
	}
	if (sc1PTrainingModeCheckLagTic() == FALSE)
	{
		gcRunAll();
	}
	else gmCameraRunFuncCamera(gGMCameraGObj);

	ifCommonSetMaxNumGObj();
}

// 0x8018DA78
void sc1PTrainingModeFuncUpdate(void)
{
	sc1PTrainingModeUpdateAll();
}

// 0x8018DA98
void sc1PTrainingModeInitVars(void)
{
	s32 dummy;
	s32 player;

	sSC1PTrainingModeBattleState = dSCManagerDefaultBattleState;
	gSCManagerBattleState = &sSC1PTrainingModeBattleState;

	gSCManagerBattleState->game_type = nSCBattleGameTypeTraining;
	gSCManagerBattleState->gkind = gSCManagerSceneData.gkind;
	gSCManagerBattleState->time_limit = SCBATTLE_TIMELIMIT_INFINITE;
	gSCManagerBattleState->is_show_score = FALSE;
	gSCManagerBattleState->item_toggles = 0;

	for (player = 0; player < ARRAY_COUNT(gSCManagerBattleState->players); player++)
	{
		if (player == gSCManagerSceneData.player)
		{
			gSCManagerBattleState->players[player].pkind = nFTPlayerKindMan;
			gSCManagerBattleState->players[player].fkind = gSCManagerSceneData.training_man_fkind;
			gSCManagerBattleState->players[player].costume = gSCManagerSceneData.training_man_costume;
			gSCManagerBattleState->players[player].team = 0;
			gSCManagerBattleState->players[player].color = player;
		}
		else gSCManagerBattleState->players[player].pkind = nFTPlayerKindNot;
	}
	dummy = (gSCManagerSceneData.player == 0) ? 1 : 0;

	gSCManagerBattleState->players[dummy].pkind = nFTPlayerKindCom;
	gSCManagerBattleState->players[dummy].tag = nIFPlayerTagKindCP;
	gSCManagerBattleState->players[dummy].fkind = gSCManagerSceneData.training_com_fkind;
	gSCManagerBattleState->players[dummy].costume = gSCManagerSceneData.training_com_costume;
	gSCManagerBattleState->players[dummy].level = 3;
	gSCManagerBattleState->players[dummy].team = 1;
	gSCManagerBattleState->players[dummy].color = 4;
	gSCManagerBattleState->pl_count = 1;
	gSCManagerBattleState->cp_count = 1;

	sSC1PTrainingModeMenu.main_menu_option = nSC1PTrainingModeMenuMainCP;
	sSC1PTrainingModeMenu.damage = 0;
	sSC1PTrainingModeMenu.combo = 0;
	sSC1PTrainingModeMenu.item_hold = -1;
	sSC1PTrainingModeMenu.cp_menu_option = nSC1PTrainingModeMenuCPStand;
	sSC1PTrainingModeMenu.speed_menu_option = nSC1PTrainingModeMenuSpeedFull;
	sSC1PTrainingModeMenu.view_menu_option = nSC1PTrainingModeMenuViewNormal;
	sSC1PTrainingModeMenu.lagtic_wait = 0;
	sSC1PTrainingModeMenu.frameadvance_wait = 0;
	sSC1PTrainingModeMenu.item_spawn_wait = 0;
	sSC1PTrainingModeMenu.item_menu_option = nSC1PTrainingModeMenuItemNone;
	sSC1PTrainingModeMenu.dummy = dummy;
	sSC1PTrainingModeMenu.button_hold = sSC1PTrainingModeMenu.button_tap = sSC1PTrainingModeMenu.button_queue = 0;
	sSC1PTrainingModeMenu.rapid_scroll_wait = 30;
	sSC1PTrainingModeMenu.damage_reset_wait = 0;
	sSC1PTrainingModeMenu.combo_reset_wait = 0;
	sSC1PTrainingModeMenu.exit_or_reset = 0;
	sSC1PTrainingModeMenu.magnify_wait = 0;
	sSC1PTrainingModeMenu.is_read_menu_inputs = FALSE;
}

// 0x8018DD0C
void sc1PTrainingModeLoadSprites(void)
{
	s32 i;

	/* DIVERGES. The game reads file 254 -- four tables of positions and
	 * cross-file Sprite pointers into IFCommonItemNames (file 29) -- out
	 * of the reloc heap, with fixRelocChain.py having resolved each
	 * pointer at link time. The port has no reloc chain, so the four
	 * tables are above, in C, with each sprite named by its offset in
	 * the one bank; this is where the offsets become pointers. The two
	 * tables it does not carry are said at the cut. */
	if (sprite_bank_load(&sSC1PTrainingModeBank, SC1PTRAININGMODE_BANK) != 0)
	{
		return;
	}
	for (i = 0; i < (s32) ARRAY_COUNT(dSC1PTrainingModeDisplayLabelPosSprites); i++)
	{
		sSC1PTrainingModeDisplayLabel[i].pos = dSC1PTrainingModeDisplayLabelPosSprites[i].pos;
		sSC1PTrainingModeDisplayLabel[i].sprite =
		    sprite_bank_get(&sSC1PTrainingModeBank, dSC1PTrainingModeDisplayLabelPosSprites[i].offset);
	}
	for (i = 0; i < (s32) ARRAY_COUNT(dSC1PTrainingModeMenuLabelPosSprites); i++)
	{
		sSC1PTrainingModeMenuLabel[i].pos = dSC1PTrainingModeMenuLabelPosSprites[i].pos;
		sSC1PTrainingModeMenuLabel[i].sprite =
		    sprite_bank_get(&sSC1PTrainingModeBank, dSC1PTrainingModeMenuLabelPosSprites[i].offset);
	}
	for (i = 0; i < (s32) ARRAY_COUNT(dSC1PTrainingModeDisplayOptionSprites); i++)
	{
		sSC1PTrainingModeDisplayOption[i] =
		    sprite_bank_get(&sSC1PTrainingModeBank, dSC1PTrainingModeDisplayOptionSprites[i]);
	}
	for (i = 0; i < (s32) ARRAY_COUNT(dSC1PTrainingModeMenuOptionSprites); i++)
	{
		sSC1PTrainingModeMenuOption[i] =
		    sprite_bank_get(&sSC1PTrainingModeBank, dSC1PTrainingModeMenuOptionSprites[i]);
	}
	sSC1PTrainingModeMenu.display_label_sprites = sSC1PTrainingModeDisplayLabel;
	sSC1PTrainingModeMenu.display_option_sprites = sSC1PTrainingModeDisplayOption;
	sSC1PTrainingModeMenu.menu_label_sprites = sSC1PTrainingModeMenuLabel;
	sSC1PTrainingModeMenu.menu_option_sprites = sSC1PTrainingModeMenuOption;
	sSC1PTrainingModeMenu.unk_trainmenu_0x34 = NULL;
	sSC1PTrainingModeMenu.unk_trainmenu_0x38 = NULL;
}

// 0x8018DDB0
/* sc1ptrainingmode.c:652-665 sc1PTrainingModeLoadWallpaper. Its one
 * caller is gr/grwallpaper.c:271 grWallpaperMakeDecideKind (the port's
 * is src/dc/stage.c's). DIVERGES: the game forces the wallpaper's file
 * into the reloc heap slot the stage's own wallpaper came from and
 * points MPGroundData.wallpaper at the sprite in it. The port's three
 * files are sprite banks, one per relocData file, and the stage keeps
 * its own texture: stage_set_wallpaper_override hands the sprite's
 * texture to the stage's wallpaper draw instead, which then places it
 * the way grWallpaperMakeStatic does. */
static SpriteBank sSC1PTrainingModeWallpaperBank;

void sc1PTrainingModeLoadWallpaper(void)
{
	SC1PTrainingModeFiles *desc = &dSC1PTrainingModeWallpaperDescs[dSC1PTrainingModeWallpaperIDs[gSCManagerBattleState->gkind]];
	const char *name;
	DCSpriteEntry *entry;

	switch (desc->file_id)
	{
	case 26: name = "grwptrainingblack.spr"; break;
	case 27: name = "grwptrainingyellow.spr"; break;
	default: name = "grwptrainingblue.spr"; break;
	}
	if (sprite_bank_load(&sSC1PTrainingModeWallpaperBank, name) != 0)
	{
		return;
	}
	entry = sprite_bank_entry(&sSC1PTrainingModeWallpaperBank, (u32)desc->offset);

	stage_set_wallpaper_override((entry != NULL) ? &entry->texs[0] : NULL);
}

// 0x8018DE60
void sc1PTrainingModeInitDisplayVars(void)
{
	gMPCollisionGroundData->fog_color = dSC1PTrainingModeWallpaperDescs[dSC1PTrainingModeWallpaperIDs[gSCManagerBattleState->gkind]].fog_color;
	ifCommonPlayerMagnifyMakeInterface();
	gIFCommonPlayerInterface.is_magnify_display = TRUE;
}

// 0x8018DEDC
SObj* sc1PTrainingModeMakeStatDisplay(GObj *interface_gobj, SC1PTrainingModeSprites *ts)
{
	SObj *sobj = lbCommonMakeSObjForGObj(interface_gobj, ts->sprite);
	
	sobj->pos.x = ts->pos.x;
	sobj->pos.y = ts->pos.y;

	return sobj;
}

// 0x8018DF30
void sc1PTrainingModeMakeStatDisplayText(void)
{
	s32 i;
	GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

	for (i = 0; i < 4; i++)
	{
		SObj *sobj = sc1PTrainingModeMakeStatDisplay(interface_gobj, &sSC1PTrainingModeMenu.display_label_sprites[i]);

		sobj->sprite.red = 0xAF;
		sobj->sprite.green = 0xAE;
		sobj->sprite.blue = 0xDD;

		sobj->envcolor.r = 0;
		sobj->envcolor.g = 0;
		sobj->envcolor.b = 0;

		sobj->sprite.attr = SP_TEXSHUF | SP_TRANSPARENT;
	}
}

// 0x8018E014
void sc1PTrainingModeUpdateDamageDisplay(GObj *interface_gobj, s32 damage)
{
	SObj *sobj = SObjGetStruct(interface_gobj);
	s32 i;

	for (i = 0; i < (ARRAY_COUNT(dSC1PTrainingModeDamagePositionsX) + ARRAY_COUNT(dSC1PTrainingModeDamageUnitLengths)) / 2; i++)
	{
		s32 modulo = damage / dSC1PTrainingModeDamageUnitLengths[i];
		damage -= modulo * dSC1PTrainingModeDamageUnitLengths[i];

		sobj->sprite = *sSC1PTrainingModeMenu.display_option_sprites[modulo];
		sobj->pos.x = (s32) (dSC1PTrainingModeDamagePositionsX[i] - (sobj->sprite.width * 0.5F));
		sobj = sobj->next;
	}
}

// 0x8018E138
void sc1PTrainingModeDamageDisplayProcDisplay(GObj *interface_gobj)
{
	s32 damage = gSCManagerBattleState->players[sSC1PTrainingModeMenu.dummy].combo_damage_foe;

	if (damage > 999)
	{
		damage = 999;
	}
	if (damage == 0)
	{
		if (sSC1PTrainingModeMenu.damage != 0)
		{
			sSC1PTrainingModeMenu.damage_reset_wait = 90;
			sSC1PTrainingModeMenu.damage = 0;
		}
		if (sSC1PTrainingModeMenu.damage_reset_wait == 0)
		{
			sSC1PTrainingModeMenu.damage = 1;
		}
	}
	if (damage != sSC1PTrainingModeMenu.damage)
	{
		sc1PTrainingModeUpdateDamageDisplay(interface_gobj, damage);
		sSC1PTrainingModeMenu.damage = damage;
	}
	lbCommonDrawSObjAttr(interface_gobj);
}

// 0x8018E1F8
void sc1PTrainingModeDamageDisplayProcUpdate(GObj *interface_gobj)
{
	if (sSC1PTrainingModeMenu.damage_reset_wait != 0)
	{
		sSC1PTrainingModeMenu.damage_reset_wait--;
	}
}

// 0x8018E21C
void sc1PTrainingModeInitStatDisplayCharacterSprites(void)
{
	s32 i;

	for (i = 0; i < 39; i++)
	{
		Sprite *sprite = sSC1PTrainingModeMenu.display_option_sprites[i];

		sprite->red = 0x6C;
		sprite->green = 0xFF;
		sprite->blue = 0x6C;

		sprite->attr = SP_TEXSHUF | SP_TRANSPARENT;
	}
}

// 0x8018E300
void sc1PTrainingModeInitSpriteEnvColors(SObj *sobj)
{
	sobj->envcolor.r = 0x00;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;
}

// 0x8018E310
void sc1PTrainingModeMakeDamageDisplay(void)
{
	GObj *interface_gobj;
	SObj *sobj;
	s32 i;

	sSC1PTrainingModeMenu.damage_display_gobj = interface_gobj = gcMakeGObjSPAfter
	(
		nGCCommonKindInterface,
		NULL,
		nGCCommonLinkIDInterface,
		GOBJ_PRIORITY_DEFAULT
	);
	gcAddGObjDisplay(interface_gobj, sc1PTrainingModeDamageDisplayProcDisplay, 23, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(interface_gobj, sc1PTrainingModeDamageDisplayProcUpdate, nGCProcessKindFunc, 4);

	for (i = 0; i < 3; i++)
	{
		sobj = lbCommonMakeSObjForGObj(interface_gobj, sSC1PTrainingModeMenu.display_option_sprites[0]);
		sc1PTrainingModeInitSpriteEnvColors(sobj);
		sobj->pos.y = 20.0F;
	}
	sc1PTrainingModeUpdateDamageDisplay(interface_gobj, 0);
	sobj = lbCommonMakeSObjForGObj(interface_gobj, sSC1PTrainingModeMenu.display_option_sprites[38]);
	sc1PTrainingModeInitSpriteEnvColors(sobj);

	sobj->pos.y = 20.0F;
	sobj->pos.x = 100.0F;
}

// 0x8018E424
void sc1PTrainingModeUpdateComboDisplay(GObj *interface_gobj, s32 combo)
{
	SObj *sobj = SObjGetStruct(interface_gobj);
	s32 i;

	for (i = 0; i < (ARRAY_COUNT(dSC1PTrainingModeComboPositionsX) + ARRAY_COUNT(dSC1PTrainingModeComboUnitLengths)) / 2; i++)
	{
		s32 modulo = combo / dSC1PTrainingModeComboUnitLengths[i];
		combo -= (modulo * dSC1PTrainingModeComboUnitLengths[i]);

		sobj->sprite = *sSC1PTrainingModeMenu.display_option_sprites[modulo];
		sobj->pos.x = (s32) (dSC1PTrainingModeComboPositionsX[i] - (sobj->sprite.width * 0.5F));
		sobj = sobj->next;
	}
}

// 0x8018E548
void sc1PTrainingModeComboDisplayProcUpdate(GObj *interface_gobj)
{
	if (sSC1PTrainingModeMenu.combo_reset_wait != 0)
	{
		sSC1PTrainingModeMenu.combo_reset_wait--;
	}
}

// 0x8018E56C
void sc1PTrainingModeComboDisplayProcDisplay(GObj *interface_gobj)
{
	s32 combo = gSCManagerBattleState->players[sSC1PTrainingModeMenu.dummy].combo_count_foe;

	if (combo > 99)
	{
		combo = 99;
	}
	if (combo == 0)
	{
		if (sSC1PTrainingModeMenu.combo != 0)
		{
			sSC1PTrainingModeMenu.combo_reset_wait = 90;
			sSC1PTrainingModeMenu.combo = 0;
		}
		if (sSC1PTrainingModeMenu.combo_reset_wait == 0)
			sSC1PTrainingModeMenu.combo = 1;
	}
	if (combo != sSC1PTrainingModeMenu.combo)
	{
		sc1PTrainingModeUpdateComboDisplay(interface_gobj, combo);
		sSC1PTrainingModeMenu.combo = combo;
	}
	lbCommonDrawSObjAttr(interface_gobj);
}

// 0x8018E62C
void sc1PTrainingModeMakeComboDisplay(void)
{
	GObj *interface_gobj;
	s32 i;

	sSC1PTrainingModeMenu.combo_display_gobj = interface_gobj = gcMakeGObjSPAfter
	(
		nGCCommonKindInterface,
		NULL,
		nGCCommonLinkIDInterface,
		GOBJ_PRIORITY_DEFAULT
	);
	gcAddGObjDisplay(interface_gobj, sc1PTrainingModeComboDisplayProcDisplay, 23, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(interface_gobj, sc1PTrainingModeComboDisplayProcUpdate, nGCProcessKindFunc, 4);

	for (i = 0; i < 2; i++)
	{
		SObj *sobj = lbCommonMakeSObjForGObj(interface_gobj, *sSC1PTrainingModeMenu.display_option_sprites);
		sc1PTrainingModeInitSpriteEnvColors(sobj);
		sobj->pos.y = 36.0F;
	}
	sc1PTrainingModeUpdateComboDisplay(interface_gobj, 0);
}

// 0x8018E714
void sc1PTrainingModeUpdateSpeedDisplaySprite(void)
{
	SObj *sobj = SObjGetStruct(sSC1PTrainingModeMenu.speed_display_gobj);
	sobj->sprite = *sSC1PTrainingModeMenu.display_option_sprites[sSC1PTrainingModeMenu.speed_menu_option + 27];
}

// 0x8018E774
void sc1PTrainingModeMakeSpeedDisplay(void)
{
	GObj *interface_gobj;
	SObj *sobj;

	sSC1PTrainingModeMenu.speed_display_gobj = interface_gobj = gcMakeGObjSPAfter
	(
		nGCCommonKindInterface,
		NULL,
		nGCCommonLinkIDInterface,
		GOBJ_PRIORITY_DEFAULT
	);
	gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

	sobj = lbCommonMakeSObjForGObj(interface_gobj, sSC1PTrainingModeMenu.display_option_sprites[sSC1PTrainingModeMenu.speed_menu_option + 27]);

	sobj->pos.x = 276.0F;
	sobj->pos.y = 20.0F;

	sc1PTrainingModeInitSpriteEnvColors(sobj);
}

// 0x8018E810
void sc1PTrainingModeUpdateCPDisplaySprite(void)
{
	SObj *sobj = SObjGetStruct(sSC1PTrainingModeMenu.cp_display_gobj);
	sobj->sprite = *sSC1PTrainingModeMenu.display_option_sprites[sSC1PTrainingModeMenu.cp_menu_option + 31];
}

// 0x8018E870
void sc1PTrainingModeMakeCPDisplay(void)
{
	GObj *interface_gobj;
	SObj *sobj;

	sSC1PTrainingModeMenu.cp_display_gobj = interface_gobj = gcMakeGObjSPAfter
	(
		nGCCommonKindInterface,
		NULL,
		nGCCommonLinkIDInterface,
		GOBJ_PRIORITY_DEFAULT
	);
	gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

	sobj = lbCommonMakeSObjForGObj(interface_gobj, sSC1PTrainingModeMenu.display_option_sprites[sSC1PTrainingModeMenu.cp_menu_option + 31]);

#if defined(REGION_US)
	sobj->pos.x = 191.0F;
#else
	sobj->pos.x = 193.0F;
#endif
	sobj->pos.y = 20.0F;

	sc1PTrainingModeInitSpriteEnvColors(sobj);
}

// 0x8018E90C
void sc1PTrainingModeUpdateItemDisplaySprite(void)
{
	SObj *root_sobj = SObjGetStruct(sSC1PTrainingModeMenu.item_display_gobj)->next, *next_sobj = root_sobj->next;

	root_sobj->sprite = *sSC1PTrainingModeMenu.display_option_sprites[sSC1PTrainingModeMenu.item_hold + 10];

	root_sobj->pos.x = 292 - root_sobj->sprite.width;
	next_sobj->pos.x = root_sobj->pos.x - next_sobj->sprite.width;
}

// 0x8018E9AC
void sc1PTrainingModeItemDisplayProcDisplay(GObj *interface_gobj)
{
	FTStruct *fp = ftGetStruct(gSCManagerBattleState->players[gSCManagerSceneData.player].fighter_gobj);
	GObj *item_gobj = fp->item_gobj;
	s32 kind;

	if (item_gobj != NULL)
	{
		ITStruct *ip = itGetStruct(item_gobj);

		if (ip->kind <= nITKindContainerEnd)
		{
			while (TRUE)
			{
				syDebugPrintf("Error : wrong item! %d\n", ip->kind);
				scManagerRunPrintGObjStatus();
			}
		}
		kind = (ip->kind <= nITKindCommonEnd) ? ip->kind - (nITKindUtilityStart - 1) : nSC1PTrainingModeMenuItemNone;
	}
	else kind = nSC1PTrainingModeMenuItemNone;

	if (sSC1PTrainingModeMenu.item_hold != kind)
	{
		sSC1PTrainingModeMenu.item_hold = kind;
		sc1PTrainingModeUpdateItemDisplaySprite();
	}
	lbCommonDrawSObjAttr(interface_gobj);
}

// 0x8018EA88
void sc1PTrainingModeMakeItemDisplay(void)
{
	GObj *interface_gobj;
	SObj *sobj;

	sSC1PTrainingModeMenu.item_display_gobj = interface_gobj = gcMakeGObjSPAfter
	(
		nGCCommonKindInterface,
		NULL,
		nGCCommonLinkIDInterface,
		GOBJ_PRIORITY_DEFAULT
	);
	gcAddGObjDisplay(interface_gobj, sc1PTrainingModeItemDisplayProcDisplay, 23, GOBJ_PRIORITY_DEFAULT, ~0);

	sobj = lbCommonMakeSObjForGObj(interface_gobj, sSC1PTrainingModeMenu.display_option_sprites[37]);
	sobj->pos.x = 292.0F;
	sobj->pos.y = 36.0F;
	sc1PTrainingModeInitSpriteEnvColors(sobj);

	sobj = lbCommonMakeSObjForGObj(interface_gobj, sSC1PTrainingModeMenu.display_option_sprites[0]);
	sobj->pos.y = 36.0F;
	sc1PTrainingModeInitSpriteEnvColors(sobj);

	sobj = lbCommonMakeSObjForGObj(interface_gobj, sSC1PTrainingModeMenu.display_option_sprites[36]);
	sobj->pos.y = 36.0F;
	sc1PTrainingModeInitSpriteEnvColors(sobj);
}

// 0x8018EB64
void sc1PTrainingModeMakeStatDisplayAll(void)
{
	sc1PTrainingModeMakeStatDisplayText();
	sc1PTrainingModeInitStatDisplayCharacterSprites();
	sc1PTrainingModeMakeDamageDisplay();
	sc1PTrainingModeMakeComboDisplay();
	sc1PTrainingModeMakeSpeedDisplay();
	sc1PTrainingModeMakeCPDisplay();
	sc1PTrainingModeMakeItemDisplay();
}

// 0x8018EBB4
void sc1PTrainingModeMakeMenuLabels(void)
{
	GObj *interface_gobj;
	s32 i;

	sSC1PTrainingModeMenu.menu_label_gobj = interface_gobj = gcMakeGObjSPAfter
	(
		nGCCommonKindInterface,
		NULL,
		nGCCommonLinkIDPauseMenu,
		GOBJ_PRIORITY_DEFAULT
	);
	gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

	for (i = 0; i < 10; i++)
	{
		SObj *sobj = sc1PTrainingModeMakeStatDisplay(interface_gobj, &sSC1PTrainingModeMenu.menu_label_sprites[i]);

		if (i < 6)
		{
			sobj->sprite.red = 0xF3;
			sobj->sprite.green = 0xA7;
			sobj->sprite.blue = 0x6A;

			sobj->envcolor.r = 0x00;
			sobj->envcolor.g = 0x00;
			sobj->envcolor.b = 0x00;
		}
		sobj->sprite.attr = SP_TEXSHUF | SP_TRANSPARENT;
	}
}

// 0x8018ECA4
void sc1PTrainingModeInitMenuOptionSpriteAttrs(void)
{
	s32 i;

	for (i = 0; i < 31; i++)
	{
		sSC1PTrainingModeMenu.menu_option_sprites[i]->attr = SP_TEXSHUF | SP_TRANSPARENT;
	}
}

// 0x8018ED2C
void sc1PTrainingModeMenuProcDisplay(GObj *interface_gobj)
{
	/* the menu's translucent blue panel: G_CC_PRIMITIVE under
	 * G_RM_CLD_SURF is one flat quad, which is what
	 * lbCommonSpriteFillRect is (src/dc/lbcommon.c). */
	lbCommonSpriteFillRect(68, 47, 253, 198, 0x00, 0x64, 0xFF, 0x64);
}

// 0x8018EE10
void sc1PTrainingModeMakeMenu(void)
{
	gcAddGObjDisplay
	(
		gcMakeGObjSPAfter
		(
			nGCCommonKindInterface,
			NULL,
			nGCCommonLinkIDPauseMenu,
			GOBJ_PRIORITY_DEFAULT
		),
		sc1PTrainingModeMenuProcDisplay,
		22,
		GOBJ_PRIORITY_DEFAULT,
		~0
	);
}

// 0x8018EE5C
void sc1PTrainingModeInitCPOptionSpriteColors(void)
{
	s32 i;

	for (i = nSC1PTrainingModeMenuOptionSpriteCPStart; i <= nSC1PTrainingModeMenuOptionSpriteCPEnd; i++)
	{
		Sprite *sprite = sSC1PTrainingModeMenu.menu_option_sprites[i];

		sprite->red = 0xFF;
		sprite->green = 0xFF;
		sprite->blue = 0xFF;
	}
}

// 0x8018EEE8
void sc1PTrainingModeUpdateCPOptionSprite(void)
{
	SObj *sobj = SObjGetStruct(sSC1PTrainingModeMenu.cp_option_gobj);

	sobj->sprite = *sSC1PTrainingModeMenu.menu_option_sprites[sSC1PTrainingModeMenu.cp_menu_option + nSC1PTrainingModeMenuOptionSpriteCPStart];
#if defined(REGION_US)
	sobj->pos.x = 191 - (sobj->sprite.width / 2);
#endif
}

// 0x8018EF78
void sc1PTrainingModeMakeCPOption(void)
{
	GObj *interface_gobj;
	SObj *sobj;

	sSC1PTrainingModeMenu.cp_option_gobj = interface_gobj = gcMakeGObjSPAfter
	(
		nGCCommonKindInterface,
		NULL,
		nGCCommonLinkIDPauseMenu,
		GOBJ_PRIORITY_DEFAULT
	);
	gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

	sobj = lbCommonMakeSObjForGObj
	(
		interface_gobj,
		sSC1PTrainingModeMenu.menu_option_sprites[sSC1PTrainingModeMenu.cp_menu_option + nSC1PTrainingModeMenuOptionSpriteCPStart]
	);

#if defined(REGION_US)
	sobj->pos.x = 191 - (sobj->sprite.width / 2);
	sobj->pos.y = 65.0F;
#else
	sobj->pos.x = 137.0F;
	sobj->pos.y = 63.0F;
#endif

	sobj->envcolor.r = 0x4A;
	sobj->envcolor.g = 0x2E;
	sobj->envcolor.b = 0x60;
}

// 0x8018F040
void sc1PTrainingModeUpdateItemOptionSprite(void)
{
	SObj *sobj = SObjGetStruct(sSC1PTrainingModeMenu.item_option_gobj);

	sobj->sprite = *sSC1PTrainingModeMenu.menu_option_sprites[sSC1PTrainingModeMenu.item_menu_option + nSC1PTrainingModeMenuOptionSpriteItemStart];

#if defined(REGION_US)
	sobj->pos.x = 191 - (sobj->sprite.width / 2);
	sobj->pos.y = (sSC1PTrainingModeMenu.item_menu_option == nSC1PTrainingModeMenuItemMotionSensorBomb) ? 83.0F : 85.0F;
#else
	sobj->pos.y = (sSC1PTrainingModeMenu.item_menu_option == nSC1PTrainingModeMenuItemMotionSensorBomb) ? 82.0F : 84.0F;
#endif
}

// 0x8018F0FC
void sc1PTrainingModeInitItemOptionSpriteColors(void)
{
	s32 i;

	for (i = nSC1PTrainingModeMenuOptionSpriteItemStart; i <= nSC1PTrainingModeMenuOptionSpriteItemEnd; i++)
	{
		Sprite *sprite = sSC1PTrainingModeMenu.menu_option_sprites[i];

		sprite->red = 0xFF;
		sprite->green = 0xFF;
		sprite->blue = 0xFF;
	}
}

// 0x8018F194
void sc1PTrainingModeMakeItemOption(void)
{
	GObj *interface_gobj;
	SObj *sobj;

	sSC1PTrainingModeMenu.item_option_gobj = interface_gobj = gcMakeGObjSPAfter
	(
		nGCCommonKindInterface,
		NULL,
		nGCCommonLinkIDPauseMenu,
		GOBJ_PRIORITY_DEFAULT
	);
	gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

	sobj = lbCommonMakeSObjForGObj
	(
		interface_gobj,
		sSC1PTrainingModeMenu.menu_option_sprites[sSC1PTrainingModeMenu.item_menu_option + nSC1PTrainingModeMenuOptionSpriteItemStart]
	);

#if defined(REGION_US)
	sobj->pos.x = 191 - (sobj->sprite.width / 2);
#else
	sobj->pos.x = 137.0F;
#endif

	sc1PTrainingModeUpdateItemOptionSprite();

	sobj->envcolor.r = 0x4A;
	sobj->envcolor.g = 0x2E;
	sobj->envcolor.b = 0x60;
}

// 0x8018F264
void sc1PTrainingModeInitSpeedOptionSpriteColors(void)
{
	s32 i;

	for (i = nSC1PTrainingModeMenuOptionSpriteSpeedStart; i <= nSC1PTrainingModeMenuOptionSpriteSpeedEnd; i++)
	{
		Sprite *sprite = sSC1PTrainingModeMenu.menu_option_sprites[i];

		sprite->red = 0xFF;
		sprite->green = 0xFF;
		sprite->blue = 0xFF;
	}
}

// 0x8018F2C4
void sc1PTrainingModeUpdateSpeedOptionSprite(void)
{
	SObj *sobj = SObjGetStruct(sSC1PTrainingModeMenu.speed_option_gobj);

	sobj->sprite = *sSC1PTrainingModeMenu.menu_option_sprites[sSC1PTrainingModeMenu.speed_menu_option + nSC1PTrainingModeMenuOptionSpriteSpeedStart];
#if defined(REGION_US)
	sobj->pos.x = 191 - (sobj->sprite.width / 2);
#endif
}

// 0x8018F354
void sc1PTrainingModeMakeSpeedOption(void)
{
	GObj *interface_gobj;
	SObj *sobj;

	sSC1PTrainingModeMenu.speed_option_gobj = interface_gobj = gcMakeGObjSPAfter
	(
		nGCCommonKindInterface,
		NULL,
		nGCCommonLinkIDPauseMenu,
		GOBJ_PRIORITY_DEFAULT
	);
	gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

	sobj = lbCommonMakeSObjForGObj
	(
		interface_gobj,
		sSC1PTrainingModeMenu.menu_option_sprites[sSC1PTrainingModeMenu.speed_menu_option + nSC1PTrainingModeMenuOptionSpriteSpeedStart]
	);

#if defined(REGION_US)
	sobj->pos.x = 191 - (sobj->sprite.width / 2);
#else
	sobj->pos.x = 140.0F;
#endif
	sobj->pos.y = 105.0F;

	sobj->envcolor.r = 0x4A;
	sobj->envcolor.g = 0x2E;
	sobj->envcolor.b = 0x60;
}

// 0x8018F41C
void func_ovl7_8018F41C(void)
{
	return;
}

// 0x8018F424
void sc1PTrainingModeUpdateViewOptionSprite(void)
{
	SObj *sobj = SObjGetStruct(sSC1PTrainingModeMenu.view_option_gobj);

	sobj->sprite = *sSC1PTrainingModeMenu.menu_option_sprites[sSC1PTrainingModeMenu.view_menu_option + nSC1PTrainingModeMenuOptionSpriteViewStart];
#if defined(REGION_US)
	sobj->pos.x = 191 - (sobj->sprite.width / 2);
#endif
}

// 0x8018F4B4
void sc1PTrainingModeViewOptionProcUpdate(GObj *interface_gobj)
{
	if (sSC1PTrainingModeMenu.magnify_wait != 0)
	{
		sSC1PTrainingModeMenu.magnify_wait--;

		if (sSC1PTrainingModeMenu.magnify_wait == 0)
		{
			gIFCommonPlayerInterface.is_magnify_display = TRUE;
		}
	}
}

// 0x8018F4EC
void sc1PTrainingModeMakeViewOption(void)
{
	GObj *interface_gobj;
	SObj *sobj;

	sSC1PTrainingModeMenu.view_option_gobj = interface_gobj = gcMakeGObjSPAfter
	(
		nGCCommonKindInterface,
		NULL,
		nGCCommonLinkIDPauseMenu,
		GOBJ_PRIORITY_DEFAULT
	);
	gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

	sobj = lbCommonMakeSObjForGObj
	(
		interface_gobj,
		sSC1PTrainingModeMenu.menu_option_sprites[sSC1PTrainingModeMenu.view_menu_option + nSC1PTrainingModeMenuOptionSpriteViewStart]
	);

#if defined(REGION_US)
	sobj->pos.x = 191 - (sobj->sprite.width / 2);
	sobj->pos.y = 125.0F;
#else
	sobj->pos.x = 140.0F;
	sobj->pos.y = 124.0F;
#endif

	sobj->envcolor.r = 0x4A;
	sobj->envcolor.g = 0x2E;
	sobj->envcolor.b = 0x60;

	gcAddGObjProcess(interface_gobj, sc1PTrainingModeViewOptionProcUpdate, nGCProcessKindFunc, 4);
}

// 0x8018F5CC
void sc1PTrainingModeSetHScrollOptionSObjs(void)
{
	sSC1PTrainingModeMenu.hscroll_option_sobj[0] = SObjGetStruct(sSC1PTrainingModeMenu.cp_option_gobj);
	sSC1PTrainingModeMenu.hscroll_option_sobj[1] = SObjGetStruct(sSC1PTrainingModeMenu.item_option_gobj);
	sSC1PTrainingModeMenu.hscroll_option_sobj[2] = SObjGetStruct(sSC1PTrainingModeMenu.speed_option_gobj);
	sSC1PTrainingModeMenu.hscroll_option_sobj[3] = SObjGetStruct(sSC1PTrainingModeMenu.view_option_gobj);
}

// 0x8018F608
void sc1PTrainingModeInitOptionArrowSpriteColors(SObj *sobj)
{
	sobj->sprite.red = 0xF3;
	sobj->sprite.green = 0x10;
	sobj->sprite.blue = 0xE;

	sobj->envcolor.r = 0x00;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;
}

// 0x8018F630
void sc1PTrainingModeUpdateOptionArrows(void)
{
	SObj *root_sobj = SObjGetStruct(sSC1PTrainingModeMenu.arrow_option_gobj); 	// Left arrow
	SObj *next_sobj = root_sobj->next;											// Right arrow

	if (sSC1PTrainingModeMenu.main_menu_option <= nSC1PTrainingModeMenuMainScrollEnd)
	{
		SObj *option_sobj = sSC1PTrainingModeMenu.hscroll_option_sobj[sSC1PTrainingModeMenu.main_menu_option];

#if defined(REGION_US)
		root_sobj->pos.x = 137.0F;
		next_sobj->pos.x = 237.0F;

		if
		(
			(sSC1PTrainingModeMenu.main_menu_option == nSC1PTrainingModeMenuMainItem) &&
			(sSC1PTrainingModeMenu.item_menu_option == nSC1PTrainingModeMenuItemMotionSensorBomb)
		)
		{
			root_sobj->pos.y = next_sobj->pos.y = (s32) (option_sobj->pos.y + 5.0F);
		}
		else root_sobj->pos.y = next_sobj->pos.y = (s32) (option_sobj->pos.y + 3.0F);
#else
		root_sobj->pos.x = option_sobj->pos.x - root_sobj->sprite.width - 3.0F;
		next_sobj->pos.x = option_sobj->pos.x + option_sobj->sprite.width + 3.0F;

		root_sobj->pos.y = next_sobj->pos.y = (s32) (option_sobj->pos.y + (option_sobj->sprite.height / 2.0F) - (root_sobj->sprite.height / 2.0F));
#endif

		root_sobj->sprite.attr &= ~SP_HIDDEN;
		next_sobj->sprite.attr &= ~SP_HIDDEN;
	}
	else
	{
		root_sobj->sprite.attr |= SP_HIDDEN;
		next_sobj->sprite.attr |= SP_HIDDEN;
	}
}

// 0x8018F730
void sc1PTrainingModeMakeOptionArrows(void)
{
	GObj *interface_gobj;

	sSC1PTrainingModeMenu.arrow_option_gobj = interface_gobj = gcMakeGObjSPAfter
	(
		nGCCommonKindInterface,
		NULL,
		nGCCommonLinkIDPauseMenu,
		GOBJ_PRIORITY_DEFAULT
	);
	gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

	sc1PTrainingModeInitOptionArrowSpriteColors
	(
		lbCommonMakeSObjForGObj(interface_gobj, sSC1PTrainingModeMenu.menu_option_sprites[nSC1PTrainingModeMenuOptionSpriteLeftArrow])
	);
	sc1PTrainingModeInitOptionArrowSpriteColors
	(
		lbCommonMakeSObjForGObj(interface_gobj, sSC1PTrainingModeMenu.menu_option_sprites[nSC1PTrainingModeMenuOptionSpriteRightArrow])
	);
	sc1PTrainingModeUpdateOptionArrows();
}

/* CUT: sc1ptrainingmode.c:1457-1574, the seven helpers over
 * unk_trainmenu_0x34/0x38 -- the two JP layout tables. Every
 * caller is inside a REGION_JP arm (lines 309, 345, 382, 426,
 * 454) or is itself one of the seven, so the US build never
 * reaches them, and their tables are the two of file 254's six
 * this port does not carry. The mn1PModeSetSubtitleSpriteColors
 * precedent (src/dc/mn1pmode.c). */






// 0x8018F9E8
s32 sc1PTrainingModeGetOptionSpriteID(void)
{
	switch (sSC1PTrainingModeMenu.main_menu_option)
	{
	case nSC1PTrainingModeMenuMainCP:
		return sSC1PTrainingModeMenu.cp_menu_option + nSC1PTrainingModeMenuOptionSpriteCPStart;

	case nSC1PTrainingModeMenuMainItem:
		return sSC1PTrainingModeMenu.item_menu_option + nSC1PTrainingModeMenuOptionSpriteItemStart;

	case nSC1PTrainingModeMenuMainSpeed:
		return sSC1PTrainingModeMenu.speed_menu_option + nSC1PTrainingModeMenuOptionSpriteSpeedStart;

	case nSC1PTrainingModeMenuMainView:
		return sSC1PTrainingModeMenu.view_menu_option + nSC1PTrainingModeMenuOptionSpriteViewStart;

	case nSC1PTrainingModeMenuMainReset:
		return nSC1PTrainingModeMenuOptionSpriteEnumCount;

	case nSC1PTrainingModeMenuMainExit:
		return nSC1PTrainingModeMenuOptionSpriteEnumCount;
	}
}



// 0x8018FBB0
void sc1PTrainingModeUpdateCursorPosition(void)
{
	SObj *cursor_sobj = SObjGetStruct(sSC1PTrainingModeMenu.cursor_gobj);
	SObj *text_sobj = sSC1PTrainingModeMenu.vscroll_option_sobj[sSC1PTrainingModeMenu.main_menu_option][0];

#if defined(REGION_US)
	cursor_sobj->pos.y = (s32) (text_sobj->pos.y - 1.0F);
#else
	cursor_sobj->pos.y = (s32) (text_sobj->pos.y + (text_sobj->sprite.height / 2.0F) - (cursor_sobj->sprite.height / 2.0F));
#endif
}

// 0x8018FC00
void sc1PTrainingModeMakeCursor(void)
{
	GObj *interface_gobj;
	SObj *target_sprite;

	sSC1PTrainingModeMenu.cursor_gobj = interface_gobj = gcMakeGObjSPAfter
	(
		nGCCommonKindInterface,
		NULL,
		nGCCommonLinkIDPauseMenu,
		GOBJ_PRIORITY_DEFAULT
	);
	gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

	target_sprite = lbCommonMakeSObjForGObj
	(
		interface_gobj,
		sSC1PTrainingModeMenu.menu_option_sprites[nSC1PTrainingModeMenuOptionSpriteCursor]
	);
#if defined(REGION_US)
	target_sprite->pos.x = 71.0F;
#else
	target_sprite->pos.x = 69.0F;
#endif

	sc1PTrainingModeUpdateCursorPosition();
}

// 0x8018FC7C
void sc1PTrainingModeSetVScrollOptionSObjs(void)
{
	SObj *arrow_sobj = SObjGetStruct(sSC1PTrainingModeMenu.arrow_option_gobj)->next;

	sSC1PTrainingModeMenu.vscroll_option_sobj[0][0] = SObjGetStruct(sSC1PTrainingModeMenu.menu_label_gobj);
	sSC1PTrainingModeMenu.vscroll_option_sobj[0][1] = arrow_sobj;

	sSC1PTrainingModeMenu.vscroll_option_sobj[1][0] = sSC1PTrainingModeMenu.vscroll_option_sobj[0][0]->next;
	sSC1PTrainingModeMenu.vscroll_option_sobj[1][1] = arrow_sobj;

	sSC1PTrainingModeMenu.vscroll_option_sobj[2][0] = sSC1PTrainingModeMenu.vscroll_option_sobj[1][0]->next;
	sSC1PTrainingModeMenu.vscroll_option_sobj[2][1] = arrow_sobj;

	sSC1PTrainingModeMenu.vscroll_option_sobj[3][0] = sSC1PTrainingModeMenu.vscroll_option_sobj[2][0]->next;
	sSC1PTrainingModeMenu.vscroll_option_sobj[3][1] = arrow_sobj;

	sSC1PTrainingModeMenu.vscroll_option_sobj[4][0] =
	sSC1PTrainingModeMenu.vscroll_option_sobj[4][1] = sSC1PTrainingModeMenu.vscroll_option_sobj[3][0]->next;

	sSC1PTrainingModeMenu.vscroll_option_sobj[5][0] =
	sSC1PTrainingModeMenu.vscroll_option_sobj[5][1] = sSC1PTrainingModeMenu.vscroll_option_sobj[4][0]->next;
}

// 0x8018FCE0
void sc1PTrainingModeUnderlineProcDisplay(GObj *interface_gobj)
{
	/* the cursor underline: G_CYC_FILL with G_RM_NOOP is an opaque
	 * quad, so the alpha the port passes is 0xFF. The two lines after
	 * the rectangle put the cycle type and render mode back for whoever
	 * draws next, which the PVR back end does per primitive. */
	lbCommonSpriteFillRect(sSC1PTrainingModeMenu.cursor_ulx, sSC1PTrainingModeMenu.cursor_uly,
	                       sSC1PTrainingModeMenu.cursor_lrx, sSC1PTrainingModeMenu.cursor_lry,
	                       0xFF, 0x00, 0x00, 0xFF);
}

// 0x8018FE40
void sc1PTrainingModeUpdateUnderline(void)
{
	SObj *text_sobj = sSC1PTrainingModeMenu.vscroll_option_sobj[sSC1PTrainingModeMenu.main_menu_option][0];
	SObj *arrow_sobj = sSC1PTrainingModeMenu.vscroll_option_sobj[sSC1PTrainingModeMenu.main_menu_option][1];
	s32 offset;

#if defined(REGION_US)
	sSC1PTrainingModeMenu.cursor_ulx = text_sobj->pos.x - 13.0F;

	offset = 
	(
		(sSC1PTrainingModeMenu.main_menu_option == nSC1PTrainingModeMenuMainReset) ||
		(sSC1PTrainingModeMenu.main_menu_option == nSC1PTrainingModeMenuMainExit)
	)
	? 2 : -2;

	sSC1PTrainingModeMenu.cursor_lrx = offset + (arrow_sobj->pos.x + arrow_sobj->sprite.width);
	sSC1PTrainingModeMenu.cursor_uly = text_sobj->pos.y + text_sobj->sprite.height + -1.0F;
	sSC1PTrainingModeMenu.cursor_lry = sSC1PTrainingModeMenu.cursor_uly + 1;
#else
	sSC1PTrainingModeMenu.cursor_ulx = text_sobj->pos.x - 2.0F;

	sSC1PTrainingModeMenu.cursor_lrx = (arrow_sobj->pos.x + arrow_sobj->sprite.width) + 2.0F;
	sSC1PTrainingModeMenu.cursor_uly = text_sobj->pos.y + text_sobj->sprite.height + 1.0F;
	sSC1PTrainingModeMenu.cursor_lry = sSC1PTrainingModeMenu.cursor_uly + 2;
#endif
}

// 0x80190070
void sc1PTrainingModeMakeUnderline(void)
{
	gcAddGObjDisplay
	(
		gcMakeGObjSPAfter
		(
			nGCCommonKindInterface,
			NULL,
			nGCCommonLinkIDPauseMenu,
			GOBJ_PRIORITY_DEFAULT
		),
		sc1PTrainingModeUnderlineProcDisplay,
		22,
		GOBJ_PRIORITY_DEFAULT,
		~0
	);
	sc1PTrainingModeUpdateUnderline();
}

// 0x801900C4
void sc1PTrainingModeMakeMenuAll(void)
{
	sc1PTrainingModeMakeMenuLabels();
	sc1PTrainingModeInitMenuOptionSpriteAttrs();
	sc1PTrainingModeMakeMenu();
	sc1PTrainingModeInitCPOptionSpriteColors();
	sc1PTrainingModeMakeCPOption();
	sc1PTrainingModeInitItemOptionSpriteColors();
	sc1PTrainingModeMakeItemOption();
	sc1PTrainingModeInitSpeedOptionSpriteColors();
	sc1PTrainingModeMakeSpeedOption();
	func_ovl7_8018F41C();
	sc1PTrainingModeMakeViewOption();
	sc1PTrainingModeSetHScrollOptionSObjs();
	sc1PTrainingModeMakeOptionArrows();
#if defined(REGION_JP)
	func_ovl7_8018F804();
	func_ovl7_8018F8FC();
	func_ovl7_8018F984();
	func_ovl7_8018FB40();
#endif
	sc1PTrainingModeSetVScrollOptionSObjs();
	sc1PTrainingModeMakeCursor();
	sc1PTrainingModeMakeUnderline();
	sc1PTrainingModeSetMenuGObjFlags(GOBJ_FLAG_HIDDEN);
}

// 0x80190164
void sc1PTrainingModeSetPlayDefaultBGM(void)
{
	gMPCollisionBGMDefault = nSYAudioBGMTrainingMode;
	syAudioPlayBGM(0, gMPCollisionBGMDefault);
	gMPCollisionBGMCurrent = gMPCollisionBGMDefault;
}

// 0x801901A0
void sc1PTrainingModeSetGameStatusGo(void)
{
	GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

	while (fighter_gobj != NULL)
	{
		ftParamUnlockPlayerControl(fighter_gobj);
		fighter_gobj = fighter_gobj->link_next;
	}
	gSCManagerBattleState->game_status = nSCBattleGameStatusGo;
}

// 0x801901F4
void sc1PTrainingModeUpdateDummyBehavior(void)
{
	FTStruct *fp = ftGetStruct(gSCManagerBattleState->players[sSC1PTrainingModeMenu.dummy].fighter_gobj);

	if (fp->pkind == nFTPlayerKindCom)
	{
		fp->computer.behavior = dSC1PTrainingModeDummyBehaviors[sSC1PTrainingModeMenu.cp_menu_option];
		fp->computer.trait = nFTComputerTraitNone;
	}
}

// 0x80190260
void sc1PTrainingModeFuncStart(void)
{
	GObj *fighter_gobj;
	FTDesc desc;
	s32 player;
	SYColorRGBA color;

	sc1PTrainingModeInitVars();
	sc1PTrainingModeSetupFiles();
	sc1PTrainingModeLoadSprites();

	/* CUT: gcMakeDefaultCameraGObj, the black clear camera -- the PVR
	 * clears its own framebuffer. src/dc/scvsbattle.c's
	 * scVSBattleStartBattle cuts the same call, and the five below it
	 * are that function's cuts too, for the reasons its header block
	 * gives at length: mpCollisionInitGroundData becomes the port's
	 * stage_bind_collision, gmCameraMakeWallpaperCamera goes because
	 * there is no wallpaper camera, grWallpaperMakeDecideKind moves
	 * after the battle camera it now attaches to, efManagerInitEffects
	 * becomes efManagerLoadEffectBank plus the port's preloads, and
	 * ftManagerSetupFilesPlayablesAll goes because the packs are
	 * already in RAM. */
	efParticleInitAll();
	ftParamInitGame();
	stage_bind_collision();
	gmCameraSetViewportDimensions(10, 10, 310, 230);
	gmCameraMakeBattleCamera();
	gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK;
	grWallpaperMakeDecideKind();
	itManagerInitItems();
	grCommonSetupInitAll();
	ftManagerAllocFighter(FTDATA_FLAG_MAINMOTION, GMCOMMON_PLAYERS_MAX);
	wpManagerAllocWeapons();
	efManagerLoadEffectBank();
	efManagerPreloadModels();
	wpManagerPreloadModels();
	itemModelPreloadAll();
	ifScreenFlashMakeInterface(0xFF);
	gmRumbleMakeActor();
	ftPublicMakeActor();

	for (player = 0; player < ARRAY_COUNT(gSCManagerBattleState->players); player++)
	{
		desc = dFTManagerDefaultFighterDesc;

		if (gSCManagerBattleState->players[player].pkind == nFTPlayerKindNot)
		{
			continue;
		}
		ftManagerSetupFilesAllKind(gSCManagerBattleState->players[player].fkind);

		desc.fkind = gSCManagerBattleState->players[player].fkind;

		mpCollisionGetPlayerMapObjPosition(player, &desc.pos);

		desc.lr = (desc.pos.x >= 0.0F) ? -1 : +1;
		desc.team = gSCManagerBattleState->players[player].team;
		desc.player = player;
		desc.detail = ((gSCManagerBattleState->pl_count + gSCManagerBattleState->cp_count) < 3) ? nFTPartsDetailHigh : nFTPartsDetailLow;
		desc.costume = gSCManagerBattleState->players[player].costume;
		desc.shade = gSCManagerBattleState->players[player].shade;
		desc.handicap = gSCManagerBattleState->players[player].handicap;
		desc.level = gSCManagerBattleState->players[player].level;
		desc.stock_count = gSCManagerBattleState->stocks;
		desc.damage = 0;
		desc.pkind = gSCManagerBattleState->players[player].pkind;
		desc.controller = &gSYControllerDevices[player];
		desc.figatree_heap = ftManagerAllocFigatreeHeapKind(gSCManagerBattleState->players[player].fkind);
		desc.is_skip_entry = TRUE;
		fighter_gobj = ftManagerMakeFighter(&desc);

		ftParamInitPlayerBattleStats(player, fighter_gobj);
	}
	sc1PTrainingModeUpdateDummyBehavior();
	/* CUT: ftManagerSetupFilesPlayablesAll, the ROM loader for every
	 * playable kind -- src/dc/scvsbattle.c's header block. */
	sc1PTrainingModeSetGameStatusGo();
	gmCameraMakePlayerArrowsCamera();
	ifCommonPlayerArrowsInitInterface();
	gmCameraMakePlayerMagnifyCamera();
	sc1PTrainingModeInitDisplayVars();
	gmCameraScreenFlashMakeCamera();
	gmCameraMakeInterfaceCamera();
	gmCameraMakeEffectCamera();
	ifCommonPlayerTagMakeInterface();
	ifCommonPlayerDamageSetDigitPositions();
	ifCommonPlayerDamageInitInterface();
	ifCommonPlayerDamageSetShowInterface();
	ifCommonPlayerStockInitInterface();
	sc1PTrainingModeMakeStatDisplayAll();
	sc1PTrainingModeMakeMenuAll();
	sc1PTrainingModeSetPlayDefaultBGM();
	func_800266A0_272A0();
	func_800269C0_275C0(nSYAudioVoicePublicExcited);

	color = dSC1PTrainingModeFadeColor;

	lbFadeMakeActor(nGCCommonKindTransition, nGCCommonLinkIDTransition, 10, &color, 12, TRUE, NULL);
	/* From here to the scene's end nothing should be read: any file that
	 * is gets a line naming it (src/dc/assetroot.h), the same guard
	 * scVSBattleStartBattle ends on. */
	asset_io_expect_none("Training mode");
}

// 0x801905A8
/* CUT: sc1ptrainingmode.c:1852-1856 sc1PTrainingModeFuncLights, the
 * pre-render, exactly as src/dc/scvsbattle.c cuts scVSBattleFuncLights:
 * it emits a geometry-mode word and the reflected fighter lights into a
 * display list, and the port's lighting is the PVR back end's
 * (src/dc/objdisplay.c). dSC1PTrainingModeTaskmanSetup carries NULL. */

// 0x801905F4
void sc1PTrainingModeStartScene(void)
{
	/* DIVERGES. syVideoInit and the z-buffer allocation are the N64's
	 * video mode, which src/dc/main.c set once for the PVR, and the
	 * arena_size line is the link map again -- the same three lines
	 * every ported scene drops (src/dc/scvsbattle.c:845). What is left
	 * is the loop, which is this scene's whole shape: RESET and EXIT
	 * both end scManagerFuncUpdate, and exit_or_reset is what tells
	 * them apart -- a reset runs FuncStart again with the same two
	 * fighters, an exit falls out to the character select. */
	s32 stage_kind;
	Stage *stage;

	/* DIVERGES, the same way src/dc/scvsbattle.c's scVSBattleStartScene
	 * does: the game's ground is the map file its overlay load brings,
	 * and the port's is a stage pack the scene acquires and binds itself,
	 * here once for the whole loop (a RESET plays the same stage) and
	 * given back after it. Without this the scene
	 * bound nothing: db.c's boot bound a stage before grOverlayLoad
	 * cleared sStage, and a menu entry never bound one, so every training
	 * match had no ground drawn and no collision -- both fighters fell
	 * and respawned for ever (found by -DDB_FB_DUMP + -DDB_FT_WATCH). A
	 * pack that will not load plays Hyrule, as a VS battle does;
	 * sc1PTrainingModeInitVars reads the kind out of gSCManagerSceneData,
	 * so the fallback is written there. */
	stage_kind = gSCManagerSceneData.gkind;
	stage = grStageAcquire(stage_kind);

	if (stage == NULL)
	{
		syDebugPrintf("sc1PTrainingModeStartScene: no pack for stage %d, "
		              "playing Hyrule\n", (int)stage_kind);
		stage_kind = gSCManagerSceneData.gkind = nGRKindHyrule;
		stage = grStageAcquire(stage_kind);
	}
	if (stage != NULL)
	{
		stage_bind(stage);
	}
	dSC1PTrainingModeTaskmanSetup.func_start = sc1PTrainingModeFuncStart;

	do
	{
		scManagerFuncUpdate(&dSC1PTrainingModeTaskmanSetup);
		gmRumbleInitPlayers();
	}
	while (sSC1PTrainingModeMenu.exit_or_reset != 0);

	if (stage != NULL)
	{
		grStageRelease(stage_kind);
	}

	syAudioStopBGMAll();

	while (syAudioCheckBGMPlaying(0) != FALSE)
	{
		continue;
	}
	syAudioSetBGMVolume(0, 0x7800);

	gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
	gSCManagerSceneData.scene_curr = nSCKindPlayers1PTraining;
}

// 0x801906D0
void sc1PTrainingModeSetupFiles(void)
{
	/* DIVERGES, and it is src/dc/scvsbattle.c's scVSBattleSetupFiles
	 * word for word: the game hands lbRelocInitSetup this scene's two
	 * status buffers and then loads the eight common files. There is no
	 * reloc loader here; what remains is the load, and it is the
	 * port's (src/dc/gmcommon.c).
	 *
	 * The line is load-bearing, not bookkeeping: dGMCommonFileIDs is
	 * where the battle HUD's own sprite banks come from, and with this
	 * function left empty the first disc probe of this step ran the
	 * whole scene correctly while logging "sprite: bank not loaded" at
	 * offsets 0x148 and 0x1458 -- ifdamage.spr's '0' and '%' -- once
	 * per frame, with no damage readout on screen. */
	gmCommonLoadFiles();
}

/* The port's own, src/dc/overlay.c's dSCManagerOverlays[7]: the bzero
 * of this overlay's noload segment, which is every file-scope object
 * above. The game gets it from syDmaLoadOverlay reloading the segment;
 * the port's files are ordinary objects in the resident image, so each
 * one clears itself (src/dc/overlay.h has the argument). */
void sc1PTrainingModeOverlayLoad(void)
{
    /* all-zero, so the linker puts it in .bss with the statics; the
     * game's overlay bzero would clear it too */
    OVERLAY_CLEAR(dSC1PTrainingModeFadeColor);
    OVERLAY_CLEAR(sSC1PTrainingModeBank);
    OVERLAY_CLEAR(sSC1PTrainingModeWallpaperBank);
    OVERLAY_CLEAR(sSC1PTrainingModeDisplayLabel);
    OVERLAY_CLEAR(sSC1PTrainingModeMenuLabel);
    OVERLAY_CLEAR(sSC1PTrainingModeDisplayOption);
    OVERLAY_CLEAR(sSC1PTrainingModeMenuOption);
    OVERLAY_CLEAR(sSC1PTrainingModePad0x80190960);
    OVERLAY_CLEAR(sSC1PTrainingModeBattleState);
    OVERLAY_CLEAR(sSC1PTrainingModeMenu);
}
