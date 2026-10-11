/* mnplayers1ptraining.c -- see mnplayers1ptraining.h. Every function is
 * mn/mnplayers/mnplayers1ptraining.c's by name and body unless marked
 * DIVERGES; the line numbers are the decomp's. */
#include "mnplayers1ptraining.h"
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
 * offset, exactly as src/dc/mnplayersvs.c does it -- this screen is
 * that one's twin, and 68 of its functions are that file's with a
 * different prefix. Each `&llXxxSprite` and `&llXxxLUT` is written
 * `llXxxSprite`, the number; a palette comes back through
 * sprite_bank_lut. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* the decomp's share of unused locals, as the VS select's */
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wreturn-type"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

/* The eight files, as src/dc/decomp/reloc_data.us.h reads them off the
 * ROM: 17 MNPlayersCommon, 23 MNPlayers1PMode, 0 MNCommon, 20
 * FTEmblemSprites, 21 MNSelectCommon, 18 MNPlayersGameModes, 19
 * MNPlayersPortraits, 22 MNPlayersSpotlight. */
#define llFTEmblemSpritesDonkeySprite                  0xc78
#define llFTEmblemSpritesFZeroSprite                   0x32b8
#define llFTEmblemSpritesFoxSprite                     0x1938
#define llFTEmblemSpritesKirbySprite                   0x1f98
#define llFTEmblemSpritesMarioSprite                   0x618
#define llFTEmblemSpritesMetroidSprite                 0x12d8
#define llFTEmblemSpritesMotherSprite                  0x3f78
#define llFTEmblemSpritesPMonstersSprite               0x3918
#define llFTEmblemSpritesYoshiSprite                   0x2c58
#define llFTEmblemSpritesZeldaSprite                   0x25f8
#define llMNCommonColonSprite                          0xdcf0
#define llMNPlayers1PModeGateCPLUT                     0x3238
#define llMNPlayers1PModeRedCardSprite                 0x32a8
#define llMNPlayersCommon0DarkSprite                   0x5388
#define llMNPlayersCommon1DarkSprite                   0x5440
#define llMNPlayersCommon1PPuckSprite                  0x9048
#define llMNPlayersCommon1PTextGradientSprite          0x8268
#define llMNPlayersCommon1PTextSprite                  0x878
#define llMNPlayersCommon2DarkSprite                   0x5558
#define llMNPlayersCommon2PPuckSprite                  0x9b28
#define llMNPlayersCommon2PTextGradientSprite          0x8368
#define llMNPlayersCommon2PTextSprite                  0xa58
#define llMNPlayersCommon3DarkSprite                   0x5668
#define llMNPlayersCommon3PPuckSprite                  0xa608
#define llMNPlayersCommon3PTextGradientSprite          0x8468
#define llMNPlayersCommon3PTextSprite                  0xc38
#define llMNPlayersCommon4DarkSprite                   0x5778
#define llMNPlayersCommon4PPuckSprite                  0xb0e8
#define llMNPlayersCommon4PTextGradientSprite          0x8568
#define llMNPlayersCommon4PTextSprite                  0xe18
#define llMNPlayersCommon5DarkSprite                   0x5888
#define llMNPlayersCommon6DarkSprite                   0x5998
#define llMNPlayersCommon7DarkSprite                   0x5aa8
#define llMNPlayersCommon8DarkSprite                   0x5bb8
#define llMNPlayersCommon9DarkSprite                   0x5cc8
#define llMNPlayersCommonArrowLSprite                  0xece8
#define llMNPlayersCommonArrowRSprite                  0xedc8
#define llMNPlayersCommonBackButtonSprite              0x115c8
#define llMNPlayersCommonButtonTextSprite              0x1428
#define llMNPlayersCommonCPLevelTextSprite             0x1218
#define llMNPlayersCommonCPPuckSprite                  0xbbc8
#define llMNPlayersCommonCPTextSprite                  0xff8
#define llMNPlayersCommonCaptainFalconTextSprite       0x3998
#define llMNPlayersCommonCursorHandGrabSprite          0x76e8
#define llMNPlayersCommonCursorHandHoverSprite         0x8168
#define llMNPlayersCommonCursorHandPointSprite         0x6f88
#define llMNPlayersCommonDKTextSprite                  0x1ff8
#define llMNPlayersCommonFoxTextSprite                 0x25b8
#define llMNPlayersCommonGateMan1PLUT                  0x103f8
#define llMNPlayersCommonGateMan2PLUT                  0x10420
#define llMNPlayersCommonGateMan3PLUT                  0x10470
#define llMNPlayersCommonGateMan4PLUT                  0x10448
#define llMNPlayersCommonHandicapTextSprite            0x1108
#define llMNPlayersCommonJigglypuffTextSprite          0x3db8
#define llMNPlayersCommonKirbyTextSprite               0x28e8
#define llMNPlayersCommonLinkTextSprite                0x2ba0
#define llMNPlayersCommonLuigiTextSprite               0x1b18
#define llMNPlayersCommonMarioTextSprite               0x1838
#define llMNPlayersCommonNessTextSprite                0x35b0
#define llMNPlayersCommonPikachuTextSprite             0x32f8
#define llMNPlayersCommonPressTextSprite               0x14d8
#define llMNPlayersCommonPushTextSprite                0x12c8
#define llMNPlayersCommonReadyBannerSprite             0xf530
#define llMNPlayersCommonReadyToFightTextSprite        0xf448
#define llMNPlayersCommonSamusTextSprite               0x2358
#define llMNPlayersCommonStartTextSprite               0x1378
#define llMNPlayersCommonYoshiTextSprite               0x2ed8
#define llMNPlayersGameModesTrainingModeTextSprite     0x758
#define llMNPlayersPortraitsCaptainShadowSprite        0x1e2e8
#define llMNPlayersPortraitsCaptainSprite              0x19e48
#define llMNPlayersPortraitsCrossSprite                0x2b8
#define llMNPlayersPortraitsDonkeySprite               0x8bc8
#define llMNPlayersPortraitsFoxSprite                  0xd068
#define llMNPlayersPortraitsKirbySprite                0xf2b8
#define llMNPlayersPortraitsLinkSprite                 0x11508
#define llMNPlayersPortraitsLuigiShadowSprite          0x20538
#define llMNPlayersPortraitsLuigiSprite                0x6978
#define llMNPlayersPortraitsMarioSprite                0x4728
#define llMNPlayersPortraitsNessShadowSprite           0x22788
#define llMNPlayersPortraitsNessSprite                 0x17bf8
#define llMNPlayersPortraitsPikachuSprite              0x159a8
#define llMNPlayersPortraitsPortraitFireBgSprite       0x24d0
#define llMNPlayersPortraitsPortraitQuestionMarkSprite 0xf68
#define llMNPlayersPortraitsPurinShadowSprite          0x249d8
#define llMNPlayersPortraitsPurinSprite                0x1c098
#define llMNPlayersPortraitsSamusSprite                0xae18
#define llMNPlayersPortraitsWhiteSquareSprite          0x6f0
#define llMNPlayersPortraitsYoshiSprite                0x13758
#define llMNPlayersSpotlightDObjDesc                   0x568
#define llMNPlayersSpotlightMObjSub                    0x408
#define llMNSelectCommonStoneBackgroundSprite          0x440

/* The banks the files became. The spotlight (22) is a model, not a
 * sprite file, and is loaded the VS select's way -- it has no bank. */
#define MNPLAYERS1PTRAINING_BANK_COMMON    "mnplayers.spr"
#define MNPLAYERS1PTRAINING_BANK_1PMODE    "mn1pselect.spr"
#define MNPLAYERS1PTRAINING_BANK_MNCOMMON  "mncommon.spr"
#define MNPLAYERS1PTRAINING_BANK_EMBLEMS   "ftemblems.spr"
#define MNPLAYERS1PTRAINING_BANK_SELECT    "mnselect.spr"
#define MNPLAYERS1PTRAINING_BANK_GAMEMODES "mngamemodes.spr"
#define MNPLAYERS1PTRAINING_BANK_PORTRAITS "mnportraits.spr"
#define MNPLAYERS1PTRAINING_MODEL_SPOTLIGHT "spotlite.mdl"

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

// 0x80137F60
u32 dMNPlayers1PTrainingFileIDs[/* */] = { 17, 23, 0, 20, 21, 18, 19, 22 };


// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x80138550
s32 sMNPlayers1PTrainingPad0x80138550[2];

// 0x80138558
MNPlayersSlotTraining sMNPlayers1PTrainingSlots[GMCOMMON_PLAYERS_MAX];

// 0x80138838
s32 sMNPlayers1PTrainingPad0x80138838[10];

// 0x80138860
s32 sMNPlayers1PTrainingStartProceedWait;

// 0x80138864
sb32 sMNPlayers1PTrainingIsStart;

// 0x80138868
sb32 sMNPlayers1PTrainingDefaultCostumeOnly;

// 0x8013886C
s32 sMNPlayers1PTrainingPad0x8013886C;

// 0x80138870
GObj *sMNPlayers1PTrainingGameModeGObj;

// 0x80138874
s32 sMNPlayers1PTrainingPuckGlowColor;

// 0x80138878
sb32 sMNPlayers1PTrainingIsPuckGlowIncreasing;

// 0x8013887C
u16 sMNPlayers1PTrainingFighterMask;

// 0x80138880
s32 sMNPlayers1PTrainingPad0x80138880;

// 0x80138884
s32 sMNPlayers1PTrainingReadyBlinkWait;

// 0x80138888
s32 sMNPlayers1PTrainingPad0x80138888;

// 0x8013888C frames elapsed on CSS
s32 sMNPlayers1PTrainingTotalTimeTics;

// 0x80138890
s32 sMNPlayers1PTrainingReturnTic;

// 0x80138894
s32 sMNPlayers1PTrainingManPlayer;

// 0x80138898
s32 sMNPlayers1PTrainingComPlayer;

/* 0x801388A0 sMNPlayers1PTrainingForceStatusBuffer[7] and 0x801388D8
 * sMNPlayers1PTrainingStatusBuffer[120], the reloc status buffers, have
 * no port; 0x80138C98 is the files' bases, here the banks. */
void *sMNPlayers1PTrainingFiles[ARRAY_COUNT(dMNPlayers1PTrainingFileIDs)];

/* the banks behind sMNPlayers1PTrainingFiles */
static SpriteBank sMNPlayers1PTrainingBanks[ARRAY_COUNT(dMNPlayers1PTrainingFileIDs)];

/* The spotlight is relocData 22, a model rather than a sprite file, so
 * it has no bank: it is a pack, loaded and cloned per slot exactly as
 * src/dc/mnplayersvs.c does it. */
static Fighter sMNPlayers1PTrainingSpotlightPack;
static Fighter sMNPlayers1PTrainingSpotlightModels[GMCOMMON_PLAYERS_MAX];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //


// 0x80131B58
void mnPlayers1PTrainingSelectFighterPuck(s32 player, s32 select_button)
{
	s32 held_player = sMNPlayers1PTrainingSlots[player].held_player, costume;

	if (select_button != nMNPlayersSelectButtonA)
	{
		costume = ftParamGetCostumeCommonID(sMNPlayers1PTrainingSlots[held_player].fkind, select_button);

		if (mnPlayers1PTrainingCheckCostumeUsed(sMNPlayers1PTrainingSlots[held_player].fkind, held_player, costume))
		{
			func_800269C0_275C0(nSYAudioFGMMenuDenied);
			return;
		}
		ftParamInitAllParts(sMNPlayers1PTrainingSlots[held_player].player, costume, 0);
		sMNPlayers1PTrainingSlots[held_player].costume = costume;
	}
	sMNPlayers1PTrainingSlots[held_player].is_selected = TRUE;

	mnPlayers1PTrainingUpdateCursorPlacementPriorities(player, held_player);

	sMNPlayers1PTrainingSlots[held_player].holder_player = GMCOMMON_PLAYERS_MAX;
	sMNPlayers1PTrainingSlots[player].cursor_status = nMNPlayersCursorStatusHover;

	mnPlayers1PTrainingUpdateCursor(sMNPlayers1PTrainingSlots[player].cursor, player, sMNPlayers1PTrainingSlots[player].cursor_status);

	sMNPlayers1PTrainingSlots[player].held_player = -1;
	sMNPlayers1PTrainingSlots[held_player].is_fighter_selected = TRUE;

	mnPlayers1PTrainingAnnounceFighter(player, held_player);

	mnPlayers1PTrainingMakePortraitFlash(held_player);
}

// 0x80131C70
f32 mnPlayers1PTrainingGetNextPortraitX(s32 portrait, f32 current_pos_x)
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
	else return ((current_pos_x + portrait_vel[portrait]) >= portrait_pos_x[portrait]) ?
	portrait_pos_x[portrait] :
	current_pos_x + portrait_vel[portrait];
}

// 0x80131D84
sb32 mnPlayers1PTrainingCheckFighterCrossed(s32 fkind)
{
	return FALSE;
}

// 0x80131D90
void mnPlayers1PTrainingPortraitProcUpdate(GObj *gobj)
{
	f32 new_pos_x = mnPlayers1PTrainingGetNextPortraitX(gobj->user_data.s, SObjGetStruct(gobj)->pos.x);

	if (new_pos_x != -1.0F)
	{
		SObjGetStruct(gobj)->pos.x = new_pos_x;

		if (SObjGetStruct(gobj)->next != NULL)
		{
			SObjGetStruct(gobj)->next->pos.x = SObjGetStruct(gobj)->pos.x + 4.0F;
		}
	}
}

// 0x80131E0C
void mnPlayers1PTrainingSetPortraitWallpaperPosition(SObj *sobj, s32 portrait)
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

// 0x80131E68
void mnPlayers1PTrainingPortraitAddCross(GObj *gobj, s32 portrait)
{
	SObj *sobj = SObjGetStruct(gobj);
	f32 x = sobj->pos.x;
	f32 y = sobj->pos.y;

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[6], llMNPlayersPortraitsCrossSprite));

	sobj->pos.x = x + 4.0F;
	sobj->pos.y = y + 12.0F;
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->sprite.red = 0xFF;
	sobj->sprite.green = 0x00;
	sobj->sprite.blue = 0x00;
}

// 0x80131EFC
sb32 mnPlayers1PTrainingCheckFighterLocked(s32 fkind)
{
	switch (fkind)
	{
	case nFTKindNess:
		return (sMNPlayers1PTrainingFighterMask & (1 << nFTKindNess)) ? FALSE : TRUE;

	case nFTKindPurin:
		return (sMNPlayers1PTrainingFighterMask & (1 << nFTKindPurin)) ? FALSE : TRUE;

	case nFTKindCaptain:
		return (sMNPlayers1PTrainingFighterMask & (1 << nFTKindCaptain)) ? FALSE : TRUE;

	case nFTKindLuigi:
		return (sMNPlayers1PTrainingFighterMask & (1 << nFTKindLuigi)) ? FALSE : TRUE;

	default:
		return FALSE;
	}
}

// 0x80138070
s32 dMNPlayers1PTrainingUnknown0x80138070[/* */] =
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

// 0x80131FC8 - Unused?
void func_ovl28_80131FC8(void)
{
	return;
}

// 0x80131FD0
s32 mnPlayers1PTrainingGetFighterKind(s32 portrait)
{
	s32 fkinds[/* */] =
	{
		nFTKindLuigi, nFTKindMario, nFTKindDonkey, nFTKindLink, nFTKindSamus,   nFTKindCaptain,
		nFTKindNess,  nFTKindYoshi, nFTKindKirby,  nFTKindFox,  nFTKindPikachu, nFTKindPurin
	};

	return fkinds[portrait];
}

// 0x80132020
s32 mnPlayers1PTrainingGetPortrait(s32 fkind)
{
	s32 portraits[/* */] =
	{
		1, 9, 2, 4, 0, 3,
		7, 5, 8, 10, 11, 6
	};

	return portraits[fkind];
}

// 0x80132070
void mnPlayers1PTrainingPortraitProcDisplay(GObj *gobj)
{
	/* Five RDP commands -- prim (0x30, 0x30, 0x30, 0xFF), the noise lerp
	 * (NOISE - TEXEL0) * PRIM + TEXEL0 and G_RM_AA_XLU_SURF -- in the
	 * port's spelling (src/dc/lbcommon.h; DIVERGES there: the PVR has no
	 * noise, the portrait's texel stands), as the VS select's own
	 * mnPlayersVSPortraitProcDisplay writes them. */
	lbCommonSpriteSetPrimColor(0x30, 0x30, 0x30, 0xFF);
	lbCommonSpriteSetCombine(nLBCommonCombineDecal);

	lbCommonDrawSObjNoAttr(gobj);
}

// 0x80132130
void mnPlayers1PTrainingMakePortraitShadow(s32 portrait)
{
	GObj *gobj;
	SObj *sobj;
	intptr_t offsets[/* */] =
	{
		0x0, 									0x0,
		0x0, 									0x0,
		llMNPlayersPortraitsLuigiShadowSprite, 0x0,
		0x0, 									llMNPlayersPortraitsCaptainShadowSprite,
		0x0, 									0x0,
		llMNPlayersPortraitsPurinShadowSprite, llMNPlayersPortraitsNessShadowSprite
	};

	gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, mnPlayers1PTrainingPortraitProcUpdate, nGCProcessKindFunc, 1);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[6], llMNPlayersPortraitsPortraitFireBgSprite));
	sobj->pos.x = (((portrait >= 6) ? portrait - 6 : portrait) * 45) + 25;
	sobj->pos.y = (((portrait >= 6) ? 1 : 0) * 43) + 36;

	mnPlayers1PTrainingSetPortraitWallpaperPosition(sobj, portrait);
	gobj->user_data.s = portrait;

	gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, mnPlayers1PTrainingPortraitProcDisplay, 27, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, mnPlayers1PTrainingPortraitProcUpdate, nGCProcessKindFunc, 1);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[6], offsets[mnPlayers1PTrainingGetFighterKind(portrait)]));
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	gobj->user_data.s = portrait;
	mnPlayers1PTrainingSetPortraitWallpaperPosition(sobj, portrait);

	gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, mnPlayers1PTrainingPortraitProcUpdate, nGCProcessKindFunc, 1);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[6], llMNPlayersPortraitsPortraitQuestionMarkSprite));
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->envcolor.r = 0x5B;
	sobj->envcolor.g = 0x41;
	sobj->envcolor.b = 0x33;
	sobj->sprite.red = 0xC4;
	sobj->sprite.green = 0xB9;
	sobj->sprite.blue = 0xA9;

	gobj->user_data.s = portrait;
	mnPlayers1PTrainingSetPortraitWallpaperPosition(sobj, portrait);
}

// 0x801323D8
void mnPlayers1PTrainingMakePortrait(s32 portrait)
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

	if (mnPlayers1PTrainingCheckFighterLocked(mnPlayers1PTrainingGetFighterKind(portrait)) != FALSE)
	{
		mnPlayers1PTrainingMakePortraitShadow(portrait);
	}
	else
	{
		wallpaper_gobj = gcMakeGObjSPAfter(0, NULL, 29, GOBJ_PRIORITY_DEFAULT);
		gcAddGObjDisplay(wallpaper_gobj, lbCommonDrawSObjAttr, 36, GOBJ_PRIORITY_DEFAULT, ~0);
		wallpaper_gobj->user_data.s = portrait;
		gcAddGObjProcess(wallpaper_gobj, mnPlayers1PTrainingPortraitProcUpdate, nGCProcessKindFunc, 1);

		sobj = lbCommonMakeSObjForGObj(wallpaper_gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[6], llMNPlayersPortraitsPortraitFireBgSprite));
		mnPlayers1PTrainingSetPortraitWallpaperPosition(sobj, portrait);

		portrait_gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);
		gcAddGObjDisplay(portrait_gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);
		gcAddGObjProcess(portrait_gobj, mnPlayers1PTrainingPortraitProcUpdate, nGCProcessKindFunc, 1);

		sobj = lbCommonMakeSObjForGObj(portrait_gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[6], offsets[mnPlayers1PTrainingGetFighterKind(portrait)]));
		sobj->sprite.attr &= ~SP_FASTCOPY;
		sobj->sprite.attr |= SP_TRANSPARENT;
		portrait_gobj->user_data.s = portrait;

		if (mnPlayers1PTrainingCheckFighterCrossed(mnPlayers1PTrainingGetFighterKind(portrait)) != FALSE)
		{
			mnPlayers1PTrainingPortraitAddCross(portrait_gobj, portrait);
		}
		mnPlayers1PTrainingSetPortraitWallpaperPosition(sobj, portrait);
	}
}

// 0x80132594
void mnPlayers1PTrainingMakePortraitAll(void)
{
	s32 i;

	for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
	{
		mnPlayers1PTrainingMakePortrait(i);
	}
}

// 0x801325D4
void mnPlayers1PTrainingMakeNameAndEmblem(GObj *gobj, s32 player, s32 fkind)
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

		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[3], emblem_offsets[fkind]));
		sobj->sprite.attr &= ~SP_FASTCOPY;
		sobj->sprite.attr |= SP_TRANSPARENT;

		if (player == sMNPlayers1PTrainingManPlayer)
		{
			sobj->sprite.red = 0x1E;
			sobj->sprite.green = 0x1E;
			sobj->sprite.blue = 0x1E;
			sobj->pos.x = 63.0F;
			sobj->pos.y = 144.0F;
		}
		else
		{
			sobj->sprite.red = 0x44;
			sobj->sprite.green = 0x44;
			sobj->sprite.blue = 0x44;
			sobj->pos.x = 195.0F;
			sobj->pos.y = 144.0F;
		}
		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], name_offsets[fkind]));
		sobj->sprite.attr &= ~SP_FASTCOPY;
		sobj->sprite.attr |= SP_TRANSPARENT;

		if (player == sMNPlayers1PTrainingManPlayer)
		{
			sobj->pos.x = 61.0F;
			sobj->pos.y = 202.0F;
		}
		else
		{
			sobj->pos.x = 193.0F;
			sobj->pos.y = 202.0F;
		}
	}
}

// 0x801327C4
void mnPlayers1PTrainingMakePortraitCamera(void)
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

// 0x80132864
void mnPlayers1PTrainingMakePortraitWallpaperCamera(void)
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
			75,
			COBJ_MASK_DLLINK(36),
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

// 0x80132904
void mnPlayers1PTrainingMakePortraitFlashCamera(void)
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
			73,
			COBJ_MASK_DLLINK(37),
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

// 0x801329A4
void mnPlayers1PTrainingMakeGateCamera(void)
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

// 0x80132A44
void mnPlayers1PTrainingMakePlayerKindSelectCamera(void)
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
			35,
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

// 0x80132AE4
void mnPlayers1PTrainingMakePlayerKindCamera(void)
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

// 0x80132B84
void mnPlayers1PTrainingMakeTeamSelectCamera(void)
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
			45,
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

// 0x80132C24
void mnPlayers1PTrainingSetGateLUT(GObj *gobj, s32 player)
{
	SObj *sobj;

	intptr_t offsets[/* */] =
	{
		llMNPlayersCommonGateMan1PLUT, llMNPlayersCommonGateMan2PLUT,
		llMNPlayersCommonGateMan3PLUT, llMNPlayersCommonGateMan4PLUT
	};

	sobj = SObjGetStruct(gobj);

	if (player == sMNPlayers1PTrainingManPlayer)
	{
		/* DIVERGES: the palette is file 17's but the card is file 23's
		 * (82 wide); file 17's bank holds the palette baked on its own
		 * 66-wide VS card, so it is looked up in file 23's, which holds
		 * this card in it (the Makefile's mn1pselect.spr). */
		SObjGetSprite(sobj)->LUT = sprite_bank_lut((SpriteBank *)sMNPlayers1PTrainingFiles[1], offsets[sMNPlayers1PTrainingManPlayer]);
	}
	else SObjGetSprite(sobj)->LUT = sprite_bank_lut((SpriteBank *)sMNPlayers1PTrainingFiles[1], llMNPlayers1PModeGateCPLUT);
}

// 0x80132CA4
void mnPlayers1PTrainingMakePlayerKind(s32 player)
{
	GObj *gobj;
	SObj *sobj;

	intptr_t offsets[/* */] =
	{
		llMNPlayersCommon1PTextSprite, llMNPlayersCommon2PTextSprite,
		llMNPlayersCommon3PTextSprite, llMNPlayersCommon4PTextSprite
	};
	f32 pos_x[/* */] = { 8.0F, 5.0F, 5.0F, 5.0F };

	sMNPlayers1PTrainingSlots[player].type = gobj = gcMakeGObjSPAfter(0, NULL, 22, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 28, GOBJ_PRIORITY_DEFAULT, ~0);

	if (player == sMNPlayers1PTrainingManPlayer)
	{
		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], offsets[sMNPlayers1PTrainingManPlayer]));
		sobj->pos.x = pos_x[player] + 53.0F;
		sobj->pos.y = 131.0F;
	}
	else
	{
		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], llMNPlayersCommonCPTextSprite));
		sobj->pos.x = 192.0F;
		sobj->pos.y = 132.0F;
	}
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->sprite.red = 0x00;
	sobj->sprite.green = 0x00;
	sobj->sprite.blue = 0x00;
}

// 0x80132E24
void mnPlayers1PTrainingMakeGate(s32 player)
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
			sMNPlayers1PTrainingFiles[1],
			llMNPlayers1PModeRedCardSprite
		),
		nGCProcessKindFunc,
		NULL,
		1
	);
	sMNPlayers1PTrainingSlots[player].panel = gobj;
	SObjGetStruct(gobj)->sprite.attr &= ~SP_FASTCOPY;
	SObjGetStruct(gobj)->sprite.attr |= SP_TRANSPARENT;

	if (player == sMNPlayers1PTrainingManPlayer)
	{
		SObjGetStruct(gobj)->pos.x = 53.0F;
		SObjGetStruct(gobj)->pos.y = 127.0F;
	}
	else
	{
		SObjGetStruct(gobj)->pos.x = 185.0F;
		SObjGetStruct(gobj)->pos.y = 127.0F;
	}
	mnPlayers1PTrainingSetGateLUT(gobj, player);
	mnPlayers1PTrainingMakePlayerKind(player);

	gobj = gcMakeGObjSPAfter(0, NULL, 22, GOBJ_PRIORITY_DEFAULT);
	sMNPlayers1PTrainingSlots[player].name_emblem_gobj = gobj;
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 28, GOBJ_PRIORITY_DEFAULT, ~0);

	mnPlayers1PTrainingUpdateNameAndEmblem(player);
}

// 0x80132FF4 - Unused?
void func_ovl28_80132FF4(void)
{
	return;
}

// 0x80132FFC - Unused?
void func_ovl28_80132FFC(void)
{
	return;
}

// 0x80133004 - Unused?
void func_ovl28_80133004(void)
{
	return;
}

// 0x8013300C - Unused?
void func_ovl28_8013300C(void)
{
	return;
}

// 0x80133014
void mnPlayers1PTrainingMakeWallpaper(void)
{
	GObj *gobj;
	SObj *sobj;

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

	gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 26, GOBJ_PRIORITY_DEFAULT, ~0);
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[4], llMNSelectCommonStoneBackgroundSprite));
	sobj->cms = G_TX_WRAP;
	sobj->cmt = G_TX_WRAP;
	sobj->masks = 6;
	sobj->maskt = 5;
	sobj->lrs = 300;
	sobj->lrt = 220;
	sobj->pos.x = 10.0F;
	sobj->pos.y = 10.0F;
}

// 0x80133140
void mnPlayers1PTrainingMakeLabels(void)
{
	GObj *gobj;
	SObj *sobj;
	s32 unused;

	gobj = lbCommonMakeSpriteGObj
	(
		0,
		NULL,
		25,
		GOBJ_PRIORITY_DEFAULT,
		lbCommonDrawSObjAttr,
		26,
		GOBJ_PRIORITY_DEFAULT,
		~0,
		lbRelocGetFileData
		(
			Sprite*,
			sMNPlayers1PTrainingFiles[5],
			llMNPlayersGameModesTrainingModeTextSprite
		),
		nGCProcessKindFunc,
		NULL,
		1
	);
	SObjGetStruct(gobj)->pos.x = 27.0F;
	SObjGetStruct(gobj)->pos.y = 24.0F;
	SObjGetStruct(gobj)->sprite.attr &= ~SP_FASTCOPY;
	SObjGetStruct(gobj)->sprite.attr |= SP_TRANSPARENT;
	SObjGetStruct(gobj)->sprite.red = 0xE3;
	SObjGetStruct(gobj)->sprite.green = 0xAC;
	SObjGetStruct(gobj)->sprite.blue = 0x04;
	sMNPlayers1PTrainingGameModeGObj = gobj;

	gobj = lbCommonMakeSpriteGObj
	(
		0,
		NULL,
		25,
		GOBJ_PRIORITY_DEFAULT,
		lbCommonDrawSObjAttr,
		26,
		GOBJ_PRIORITY_DEFAULT,
		~0,
		lbRelocGetFileData
		(
			Sprite*,
			sMNPlayers1PTrainingFiles[0],
			llMNPlayersCommonBackButtonSprite
		),
		nGCProcessKindFunc,
		NULL,
		1
	);
	SObjGetStruct(gobj)->pos.x = 244.0F;
	SObjGetStruct(gobj)->pos.y = 23.0F;
	SObjGetStruct(gobj)->sprite.attr &= ~SP_FASTCOPY;
	SObjGetStruct(gobj)->sprite.attr |= SP_TRANSPARENT;
}

// 0x80138264
intptr_t dMNPlayers1PTrainingDigitOffsets[/* */] =
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

// 0x8013828C
f32 dMNPlayers1PTrainingDigitWidths[/* */] =
{
	8.0F, 6.0F, 9.0F, 8.0F, 8.0F,
	9.0F, 8.0F, 8.0F, 8.0F, 9.0F
};

// 0x801332C4 - Unused?
void func_ovl28_801332C4(void)
{
	return;
}

// 0x801332CC - Unused?
void func_ovl28_801332CC(void)
{
	return;
}

// 0x801332D4 - Unused?
void func_ovl28_801332D4(void)
{
	return;
}

// 0x801332DC
s32 mnPlayers1PTrainingGetFighterKindCount(s32 fkind)
{
	s32 count = 0;

	if (fkind == sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].fkind)
	{
		count++;
	}
	if (fkind == sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].fkind)
	{
		count++;
	}
	return (count != 0) ? count - 1 : count;
}

// 0x80133350
sb32 mnPlayers1PTrainingCheckCostumeUsed(s32 fkind, s32 player, s32 costume)
{
	if (player == sMNPlayers1PTrainingManPlayer)
	{
		if
		(
			(fkind == sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].fkind) &&
			(costume == sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].costume)
		)
		{
			return TRUE;
		}
		else return FALSE;
	}
	if (player == sMNPlayers1PTrainingComPlayer)
	{
		if
		(
			(fkind == sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].fkind) &&
			(costume == sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].costume)
		)
		{
			return TRUE;
		}
		else return FALSE;
	}
}

// 0x80133408
s32 mnPlayers1PTrainingGetFreeCostumeRoyal(s32 fkind, s32 player)
{
	s32 i, j, k, l;
	sb32 is_costume_used[GMCOMMON_PLAYERS_MAX];

	for (i = 0; i < ARRAY_COUNT(is_costume_used); i++)
	{
		is_costume_used[i] = FALSE;
	}
	if (player == sMNPlayers1PTrainingManPlayer)
	{
		if (fkind == sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].fkind)
		{
			for (j = 0; j < ARRAY_COUNT(is_costume_used); j++)
			{
				if (ftParamGetCostumeCommonID(fkind, j) == sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].costume)
				{
					is_costume_used[j] = TRUE;
				}
			}
		}
	}
	else if (fkind ==sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].fkind)
	{
		for (k = 0; k < ARRAY_COUNT(is_costume_used); k++)
		{
			if (ftParamGetCostumeCommonID(fkind, k) == sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].costume)
			{
				is_costume_used[k] = TRUE;
			}
		}
	}
	for (l = 0; l < ARRAY_COUNT(is_costume_used); l++)
	{
		if (is_costume_used[l] == FALSE)
		{
			return l;
		}
	}
}

// 0x801335F0
s32 mnPlayers1PTrainingGetFreeCostume(s32 fkind, s32 player)
{
	return ftParamGetCostumeCommonID(fkind, mnPlayers1PTrainingGetFreeCostumeRoyal(fkind, player));
}

// 0x8013361C
s32 mnPlayers1PTrainingGetStatusSelected(s32 fkind)
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

// 0x8013367C
void mnPlayers1PTrainingFighterProcUpdate(GObj *fighter_gobj)
{
	FTStruct *fp = ftGetStruct(fighter_gobj);
	s32 player = fp->player;

	if (sMNPlayers1PTrainingSlots[player].is_fighter_selected == TRUE)
	{
		if (DObjGetStruct(fighter_gobj)->rotate.vec.f.y < F_CLC_DTOR32(0.1F))
		{
			if (sMNPlayers1PTrainingSlots[player].is_status_selected == FALSE)
			{
				scSubsysFighterSetStatus(sMNPlayers1PTrainingSlots[player].player, mnPlayers1PTrainingGetStatusSelected(sMNPlayers1PTrainingSlots[player].fkind));

				sMNPlayers1PTrainingSlots[player].is_status_selected = TRUE;
			}
		}
		else
		{
			DObjGetStruct(fighter_gobj)->rotate.vec.f.y += F_CST_DTOR32(20.0F);

			if (DObjGetStruct(fighter_gobj)->rotate.vec.f.y > F_CLC_DTOR32(360.0F))
			{
				DObjGetStruct(fighter_gobj)->rotate.vec.f.y = 0.0F;

				scSubsysFighterSetStatus(sMNPlayers1PTrainingSlots[player].player, mnPlayers1PTrainingGetStatusSelected(sMNPlayers1PTrainingSlots[player].fkind));

				sMNPlayers1PTrainingSlots[player].is_status_selected = TRUE;
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

// 0x801337BC
void mnPlayers1PTrainingMakeFighter(GObj *fighter_gobj, s32 player, s32 fkind, s32 costume)
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
		sMNPlayers1PTrainingSlots[player].costume = desc.costume = costume;
		desc.shade = 0;
		desc.figatree_heap = sMNPlayers1PTrainingSlots[player].figatree_heap;
		desc.player = player;
		fighter_gobj = ftManagerMakeFighter(&desc);

		sMNPlayers1PTrainingSlots[player].player = fighter_gobj;

		gcAddGObjProcess(fighter_gobj, mnPlayers1PTrainingFighterProcUpdate, nGCProcessKindFunc, 1);

		if (player == sMNPlayers1PTrainingManPlayer)
		{
			DObjGetStruct(fighter_gobj)->translate.vec.f.x = -830.0F;
			DObjGetStruct(fighter_gobj)->translate.vec.f.y = -870.0F;
		}
		else
		{
			DObjGetStruct(fighter_gobj)->translate.vec.f.x = 830.0F;
			DObjGetStruct(fighter_gobj)->translate.vec.f.y = -870.0F;
		}
		DObjGetStruct(fighter_gobj)->rotate.vec.f.y = rot_y;

		DObjGetStruct(fighter_gobj)->scale.vec.f.x = dSCSubsysFighterScales[fkind];
		DObjGetStruct(fighter_gobj)->scale.vec.f.y = dSCSubsysFighterScales[fkind];
		DObjGetStruct(fighter_gobj)->scale.vec.f.z = dSCSubsysFighterScales[fkind];

		if (player == sMNPlayers1PTrainingComPlayer)
		{
			ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterComPlayer, 0);
		}
	}
}

// 0x801339A0
void mnPlayers1PTrainingMakeFighterCamera(void)
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
			30,
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

// 0x80133A90
void mnPlayers1PTrainingUpdateCursor(GObj *gobj, s32 player, s32 cursor_status)
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

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], cursor_offsets[cursor_status]));
	sobj->pos.x = start_pos_x;
	sobj->pos.y = start_pos_y;
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], num_offsets[player]));
	sobj->pos.x = sobj->prev->pos.x + pos[cursor_status].x;
	sobj->pos.y = sobj->prev->pos.y + pos[cursor_status].y;
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->sprite.red = colors[player].prim.r;
	sobj->sprite.green = colors[player].prim.g;
	sobj->sprite.blue = colors[player].prim.b;
	sobj->envcolor.r = colors[player].env.r;
	sobj->envcolor.g = colors[player].env.g;
	sobj->envcolor.b = colors[player].env.b;
}

// 0x80133CA0 - Unused?
void func_ovl28_80133CA0(void)
{
	return;
}

// 0x80133CA8
void mnPlayers1PTrainingSetGateLUTAll(void)
{
	s32 i;
	s32 ids[/* */] = { 0, 1, 2, 3 };

	mnPlayers1PTrainingSetGateLUT(sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].panel, sMNPlayers1PTrainingManPlayer);
	mnPlayers1PTrainingSetGateLUT(sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].panel, sMNPlayers1PTrainingComPlayer);
}

// 0x80133D44
sb32 mnPlayers1PTrainingCheckPuckInRange(GObj *gobj, s32 cursor_player, s32 player)
{
	f32 pos_x, pos_y;
	sb32 is_in_range;
	SObj *cursor_sobj = SObjGetStruct(gobj);
	SObj *puck_sobj = SObjGetStruct(sMNPlayers1PTrainingSlots[player].puck);

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

// 0x80133E30
void mnPlayers1PTrainingUpdateFighter(s32 player)
{
	sb32 is_skip_fighter = FALSE;

	if (sMNPlayers1PTrainingSlots[player].player != NULL)
	{
		if ((sMNPlayers1PTrainingSlots[player].fkind == nFTKindNull) && (sMNPlayers1PTrainingSlots[player].is_selected == FALSE))
		{
			sMNPlayers1PTrainingSlots[player].player->flags = GOBJ_FLAG_HIDDEN;
			is_skip_fighter = TRUE;
		}
	}
	if (is_skip_fighter == FALSE)
	{
		mnPlayers1PTrainingMakeFighter
		(
			sMNPlayers1PTrainingSlots[player].player,
			player,
			sMNPlayers1PTrainingSlots[player].fkind,
			mnPlayers1PTrainingGetFreeCostume(sMNPlayers1PTrainingSlots[player].fkind, player)
		);
		sMNPlayers1PTrainingSlots[player].is_status_selected = FALSE;
	}
}

// 0x80133ED8 - Unused?
void func_ovl28_80133ED8(void)
{
	return;
}

// 0x80133EE0
void mnPlayers1PTrainingUpdateNameAndEmblem(s32 player)
{
	if
	(
		(sMNPlayers1PTrainingSlots[player].pkind == nFTPlayerKindNot) ||
		(sMNPlayers1PTrainingSlots[player].fkind == nFTKindNull) &&
		(sMNPlayers1PTrainingSlots[player].is_selected == FALSE)
	)
	{
		sMNPlayers1PTrainingSlots[player].name_emblem_gobj->flags = GOBJ_FLAG_HIDDEN;
	}
	else
	{
		sMNPlayers1PTrainingSlots[player].name_emblem_gobj->flags = GOBJ_FLAG_NONE;
		mnPlayers1PTrainingMakeNameAndEmblem(sMNPlayers1PTrainingSlots[player].name_emblem_gobj, player, sMNPlayers1PTrainingSlots[player].fkind);
	}
}

// 0x80133F68
void mnPlayers1PTrainingDestroyPortraitFlash(s32 player)
{
	GObj *gobj = sMNPlayers1PTrainingSlots[player].flash;

	if (gobj != NULL)
	{
		sMNPlayers1PTrainingSlots[player].flash = NULL;
		gcEjectGObj(gobj);
	}
}

// 0x80133FB4
void mnPlayers1PTrainingPortraitFlashThreadUpdate(GObj *gobj)
{
	s32 length = 16;
	s32 wait_tics = 1;

	while (TRUE)
	{
		length--, wait_tics--;

		if (length == 0)
		{
			mnPlayers1PTrainingDestroyPortraitFlash(gobj->user_data.s);
		}
		if (wait_tics == 0)
		{
			wait_tics = 1;
			gobj->flags = (gobj->flags == GOBJ_FLAG_HIDDEN) ? GOBJ_FLAG_NONE : GOBJ_FLAG_HIDDEN;
		}
		gcSleepCurrentGObjThread(1);
	}
}

// 0x8013405C
void mnPlayers1PTrainingMakePortraitFlash(s32 player)
{
	GObj *gobj;
	SObj *sobj;
	s32 portrait = mnPlayers1PTrainingGetPortrait(sMNPlayers1PTrainingSlots[player].fkind);

	mnPlayers1PTrainingDestroyPortraitFlash(player);

	sMNPlayers1PTrainingSlots[player].flash = gobj = gcMakeGObjSPAfter(0, NULL, 30, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 37, GOBJ_PRIORITY_DEFAULT, ~0);
	gobj->user_data.s = player;
	gcAddGObjProcess(gobj, mnPlayers1PTrainingPortraitFlashThreadUpdate, nGCProcessKindThread, 1);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[6], llMNPlayersPortraitsWhiteSquareSprite));
	sobj->pos.x = (((portrait >= 6) ? portrait - 6 : portrait) * 45) + 26;
	sobj->pos.y = (((portrait >= 6) ? 1 : 0) * 43) + 37;
}

// 0x801341B0
void mnPlayers1PTrainingAnnounceFighter(s32 player, s32 slot)
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

	if (sMNPlayers1PTrainingSlots[player].p_sfx != NULL)
	{
		if ((sMNPlayers1PTrainingSlots[player].p_sfx->sfx_id != 0) && (sMNPlayers1PTrainingSlots[player].p_sfx->sfx_id == sMNPlayers1PTrainingSlots[player].sfx_id))
		{
			func_80026738_27338(sMNPlayers1PTrainingSlots[player].p_sfx);
		}
	}
	func_800269C0_275C0(nSYAudioFGMMarioDash);

	sMNPlayers1PTrainingSlots[player].p_sfx = func_800269C0_275C0(announce_names[sMNPlayers1PTrainingSlots[slot].fkind]);

	if (sMNPlayers1PTrainingSlots[player].p_sfx != NULL)
	{
		sMNPlayers1PTrainingSlots[player].sfx_id = sMNPlayers1PTrainingSlots[player].p_sfx->sfx_id;
	}
}

// 0x801342B0 - Unused?
void func_ovl28_801342B0(void)
{
	return;
}

// 0x801342B8
void mnPlayers1PTrainingDestroyHandicapLevel(s32 player)
{
	if (sMNPlayers1PTrainingSlots[player].handicap_cpu_level != NULL)
	{
		gcEjectGObj(sMNPlayers1PTrainingSlots[player].handicap_cpu_level);
	}
	if (sMNPlayers1PTrainingSlots[player].arrows != NULL)
	{
		gcEjectGObj(sMNPlayers1PTrainingSlots[player].arrows);
	}
	if (sMNPlayers1PTrainingSlots[player].handicap_cpu_level_value != NULL)
	{
		gcEjectGObj(sMNPlayers1PTrainingSlots[player].handicap_cpu_level_value);
	}
	sMNPlayers1PTrainingSlots[player].handicap_cpu_level = NULL;
	sMNPlayers1PTrainingSlots[player].arrows = NULL;
	sMNPlayers1PTrainingSlots[player].handicap_cpu_level_value = NULL;
}

// 0x80134340
SObj* mnPlayers1PTrainingGetArrowSObj(GObj *gobj, sb32 left_or_right)
{
	if (SObjGetStruct(gobj) != NULL)
	{
		if (left_or_right == SObjGetStruct(gobj)->user_data.s)
		{
			return SObjGetStruct(gobj);
		}
		if ((SObjGetStruct(gobj)->next != NULL) && (left_or_right == SObjGetStruct(gobj)->next->user_data.s))
		{
			return SObjGetStruct(gobj)->next;
		}
	}
	return NULL;
}

// 0x8013438C - Unused?
void mnPlayers1PTrainingArrowThreadUpdate(GObj *gobj)
{
	SObj *sobj;
	s32 player = gobj->user_data.s;
	s32 blink_wait = 10;
	s32 value;

	while (TRUE)
	{
		blink_wait--;

		if (blink_wait == 0)
		{
			blink_wait = 10;
			gobj->flags = (gobj->flags == GOBJ_FLAG_HIDDEN) ? GOBJ_FLAG_NONE : GOBJ_FLAG_HIDDEN;
		}
		value = (sMNPlayers1PTrainingSlots[player].pkind == nFTPlayerKindMan) ? sMNPlayers1PTrainingSlots[player].handicap : sMNPlayers1PTrainingSlots[player].cpu_level;

		if (value == 1)
		{
			sobj = mnPlayers1PTrainingGetArrowSObj(gobj, 0);

			if (sobj != NULL)
			{
				gcEjectSObj(sobj);
			}
		}
		else if (mnPlayers1PTrainingGetArrowSObj(gobj, 0) == NULL)
		{
			sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], llMNPlayersCommonArrowLSprite));
			sobj->pos.x = (player * 69) + 25;
			sobj->pos.y = 201.0F;
			sobj->sprite.attr &= ~SP_FASTCOPY;
			sobj->sprite.attr |= SP_TRANSPARENT;
			sobj->user_data.s = 0;
		}
		if (value == 9)
		{
			sobj = mnPlayers1PTrainingGetArrowSObj(gobj, 1);

			if (sobj != NULL)
			{
				gcEjectSObj(sobj);
			}
		}
		else if (mnPlayers1PTrainingGetArrowSObj(gobj, 1) == NULL)
		{
			sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], llMNPlayersCommonArrowRSprite));
			sobj->pos.x = (player * 69) + 79;
			sobj->pos.y = 201.0F;
			sobj->sprite.attr &= ~SP_FASTCOPY;
			sobj->sprite.attr |= SP_TRANSPARENT;
			sobj->user_data.s = 1;
		}
		gcSleepCurrentGObjThread(1);
	}
}

// 0x801345B8
void mnPlayers1PTrainingHandicapLevelProcUpdate(GObj *gobj)
{
	s32 player = gobj->user_data.s;

	if (sMNPlayers1PTrainingSlots[player].is_fighter_selected == FALSE)
	{
		mnPlayers1PTrainingDestroyHandicapLevel(player);
	}
	else if (SObjGetStruct(gobj)->user_data.s != sMNPlayers1PTrainingSlots[player].pkind)
	{
		mnPlayers1PTrainingMakeHandicapLevel(player);
	}
}

// 0x8013462C
void mnPlayers1PTrainingMakeHandicapLevel(s32 player)
{
	GObj *gobj;
	SObj *sobj;

	if (sMNPlayers1PTrainingSlots[player].handicap_cpu_level != NULL)
	{
		gcEjectGObj(sMNPlayers1PTrainingSlots[player].handicap_cpu_level);
		sMNPlayers1PTrainingSlots[player].handicap_cpu_level = NULL;
	}
	sMNPlayers1PTrainingSlots[player].handicap_cpu_level = gobj = gcMakeGObjSPAfter(0, NULL, 28, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 35, GOBJ_PRIORITY_DEFAULT, ~0);
	gobj->user_data.s = player;
	gcAddGObjProcess(gobj, mnPlayers1PTrainingHandicapLevelProcUpdate, nGCProcessKindFunc, 1);

	if (sMNPlayers1PTrainingSlots[player].pkind == nFTPlayerKindMan)
	{
		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], llMNPlayersCommonHandicapTextSprite));
		sobj->pos.x = (player * 69) + 35;
		sobj->user_data.s = nFTPlayerKindMan;
	}
	else
	{
		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], llMNPlayersCommonCPLevelTextSprite));
		sobj->pos.x = (player * 69) + 34;
		sobj->user_data.s = nFTPlayerKindCom;
	}
	sobj->pos.y = 201.0F;
	sobj->sprite.red = 0xC2;
	sobj->sprite.green = 0xBD;
	sobj->sprite.blue = 0xAD;
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[2], llMNCommonColonSprite));
	sobj->pos.x = (player * 69) + 61;
	sobj->pos.y = 202.0F;
	sobj->sprite.red = 0xFF;
	sobj->sprite.green = 0xFF;
	sobj->sprite.blue = 0xFF;
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
}

// 0x80138328
intptr_t dMNPlayers1PTrainingUnused0x80138328[/* */] =
{
	0xD310,
	0xD3E0,
	0xD4B0,
	0xD580,
	0xD650,
	0xD720,
	0xD7F0,
	0xD8C0,
	0xD990,
	0xDA60
};

// 0x80134830 - Unused?
void func_ovl28_80134830(void)
{
	return;
}

// 0x80134838 - Unused?
void func_ovl28_80134838(void)
{
	return;
}

// 0x80134840 - Unused?
void func_ovl28_80134840(void)
{
	return;
}

// 0x80134848
sb32 mnPlayers1PTrainingSelectFighter(GObj *gobj, s32 player, s32 unused, s32 select_button)
{
	MNPlayersSlotTraining *pslot;

	if (sMNPlayers1PTrainingSlots[player].cursor_status != nMNPlayersCursorStatusGrab)
	{
		return FALSE;
	}
	if (sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingSlots[player].held_player].fkind != nFTKindNull)
	{
		mnPlayers1PTrainingSelectFighterPuck(player, select_button);
		sMNPlayers1PTrainingSlots[player].recall_end_tic = sMNPlayers1PTrainingTotalTimeTics + 30;
		
		return TRUE;
	}
	else func_800269C0_275C0(nSYAudioFGMMenuDenied);

	return FALSE;
}

// 0x801348F0
void mnPlayers1PTrainingUpdateCursorGrabPriorities(s32 player, s32 puck)
{
	u32 priorities[/* */] = { 6, 4, 2, 0 };
	s32 i, order;

	gcMoveGObjDL(sMNPlayers1PTrainingSlots[player].cursor, 32, priorities[ARRAY_COUNT(priorities) - 1]);
	gcMoveGObjDL(sMNPlayers1PTrainingSlots[puck].puck, 32, priorities[ARRAY_COUNT(priorities) - 1] + 1);

	for (i = 0, order = ARRAY_COUNT(priorities) - 1; i < ARRAY_COUNT(priorities); i++, order--)
	{
		if (i != player)
		{
			if (sMNPlayers1PTrainingSlots[i].cursor != NULL)
			{
				gcMoveGObjDL(sMNPlayers1PTrainingSlots[i].cursor, 32, priorities[order]);
			}
			if (sMNPlayers1PTrainingSlots[i].held_player != -1)
			{
				gcMoveGObjDL(sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingSlots[i].held_player].puck, 32, priorities[order] + 1);
			}
		}
	}
}

// 0x80134A4C
void mnPlayers1PTrainingUpdateCursorPlacementPriorities(s32 player, s32 puck)
{
	s32 held_priorities[/* */] = { 3, 2, 1, 0 };
	s32	unheld_priorities[/* */] = { 6, 4, 2, 0 };
	s32 unused;
	s32 unheld_id;
	sb32 is_held[GMCOMMON_PLAYERS_MAX];
	s32 i;

	for (i = 0; i < (ARRAY_COUNT(sMNPlayers1PTrainingSlots) + ARRAY_COUNT(is_held)) / 2; i++)
	{
		if (sMNPlayers1PTrainingSlots[i].held_player == -1)
		{
			is_held[i] = FALSE;
		}
		else is_held[i] = TRUE;
	}
	for (i = 0, unheld_id = ARRAY_COUNT(unheld_priorities) - 1; i < (ARRAY_COUNT(is_held) + ARRAY_COUNT(unheld_priorities)) / 2; i++)
	{
		if ((i != player) && (is_held[i] != FALSE))
		{
			if (sMNPlayers1PTrainingSlots[i].cursor != NULL)
			{
				gcMoveGObjDL(sMNPlayers1PTrainingSlots[i].cursor, 32, unheld_priorities[unheld_id]);
			}
			gcMoveGObjDL(sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingSlots[i].held_player].puck, 32, unheld_priorities[unheld_id] + 1);
			unheld_id--;
		}
	}
	if (player != GMCOMMON_PLAYERS_MAX)
	{
		gcMoveGObjDL(sMNPlayers1PTrainingSlots[player].cursor, 32, unheld_priorities[unheld_id]);
	}
	gcMoveGObjDL(sMNPlayers1PTrainingSlots[puck].puck, 33, unheld_priorities[unheld_id] + 1);

	unheld_id--;

	for (i = 0; i < (ARRAY_COUNT(is_held) + ARRAY_COUNT(unheld_priorities)) / 2; i++)
	{
		if ((i != player) && (is_held[i] == FALSE))
		{
			if (sMNPlayers1PTrainingSlots[i].cursor != NULL)
			{
				gcMoveGObjDL(sMNPlayers1PTrainingSlots[i].cursor, 32, unheld_priorities[unheld_id]);
			}
			unheld_id--;
		}
	}
}

// 0x80134C64
void mnPlayers1PTrainingSetCursorPuckOffset(s32 player)
{
	sMNPlayers1PTrainingSlots[player].cursor_pickup_x =
	SObjGetStruct(sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingSlots[player].held_player].puck)->pos.x - 11.0F;

	sMNPlayers1PTrainingSlots[player].cursor_pickup_y =
	SObjGetStruct(sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingSlots[player].held_player].puck)->pos.y - -14.0F;
}

// 0x80134CC8
void mnPlayers1PTrainingSetCursorGrab(s32 player, s32 held_player)
{
	sMNPlayers1PTrainingSlots[held_player].holder_player = player;
	sMNPlayers1PTrainingSlots[held_player].is_selected = FALSE;

	sMNPlayers1PTrainingSlots[player].cursor_status = nMNPlayersCursorStatusGrab;
	sMNPlayers1PTrainingSlots[player].held_player = held_player;

	sMNPlayers1PTrainingSlots[held_player].is_fighter_selected = FALSE;

	mnPlayers1PTrainingUpdateFighter(held_player);
	mnPlayers1PTrainingUpdateCursorGrabPriorities(player, held_player);
	mnPlayers1PTrainingSetCursorPuckOffset(player);
	mnPlayers1PTrainingUpdateCursor(sMNPlayers1PTrainingSlots[player].cursor, player, sMNPlayers1PTrainingSlots[player].cursor_status);

	sMNPlayers1PTrainingSlots[player].is_cursor_adjusting = TRUE;

	func_800269C0_275C0(nSYAudioFGMSamusDash);

	mnPlayers1PTrainingDestroyHandicapLevel(held_player);
	mnPlayers1PTrainingDestroyPortraitFlash(held_player);
	mnPlayers1PTrainingUpdateNameAndEmblem(held_player);
}

// 0x80134D8C
sb32 mnPlayers1PTrainingCheckCursorPuckGrab(GObj *gobj, s32 player)
{
	s32 i;

	if ((sMNPlayers1PTrainingTotalTimeTics < sMNPlayers1PTrainingSlots[player].recall_end_tic) || (sMNPlayers1PTrainingSlots[player].is_recalling != FALSE))
	{
		return FALSE;
	}
	else if (sMNPlayers1PTrainingSlots[player].cursor_status != nMNPlayersCursorStatusHover)
	{
		return FALSE;
	}
	else for (i = ARRAY_COUNT(sMNPlayers1PTrainingSlots) - 1; i >= 0; i--)
	{
		if (player == i)
		{
			if
			(
				(sMNPlayers1PTrainingSlots[i].holder_player == GMCOMMON_PLAYERS_MAX) &&
				(sMNPlayers1PTrainingSlots[i].pkind != nFTPlayerKindNot) &&
				(mnPlayers1PTrainingCheckPuckInRange(gobj, player, i) != FALSE)
			)
			{
				mnPlayers1PTrainingSetCursorGrab(player, i);

				return TRUE;
			}
		}
		else if
		(
			(sMNPlayers1PTrainingSlots[i].holder_player == GMCOMMON_PLAYERS_MAX) &&
			(sMNPlayers1PTrainingSlots[i].pkind == nFTPlayerKindCom) &&
			(mnPlayers1PTrainingCheckPuckInRange(gobj, player, i) != FALSE)
		)
		{
			mnPlayers1PTrainingSetCursorGrab(player, i);

			return TRUE;
		}
	}
	return FALSE;
}

// 0x80134EE8
s32 mnPlayers1PTrainingGetPuckFighterKind(s32 player)
{
	SObj *sobj = SObjGetStruct(sMNPlayers1PTrainingSlots[player].puck);
	s32 pos_x = (s32) sobj->pos.x + 13;
	s32 pos_y = (s32) sobj->pos.y + 12;
	s32 fkind;
	sb32 is_in_range = ((pos_y > 35) && (pos_y < 79)) ? TRUE : FALSE;

	if (is_in_range != FALSE)
	{
		is_in_range = ((pos_x > 24) && (pos_x < 295)) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			fkind = mnPlayers1PTrainingGetFighterKind((pos_x - 25) / 45);

			if ((mnPlayers1PTrainingCheckFighterCrossed(fkind) != FALSE) || (mnPlayers1PTrainingCheckFighterLocked(fkind) != FALSE))
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
			fkind = mnPlayers1PTrainingGetFighterKind(((pos_x - 25) / 45) + 6);

			if ((mnPlayers1PTrainingCheckFighterCrossed(fkind) != FALSE) || (mnPlayers1PTrainingCheckFighterLocked(fkind) != FALSE))
			{
				return nFTKindNull;
			}
			else return fkind;
		}
	}
	return nFTKindNull;
}

// 0x80135074
void mnPlayers1PTrainingAdjustCursor(GObj *gobj, s32 player)
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

	if (sMNPlayers1PTrainingSlots[player].is_cursor_adjusting != FALSE)
	{
		delta = (sMNPlayers1PTrainingSlots[player].cursor_pickup_x - SObjGetStruct(sMNPlayers1PTrainingSlots[player].cursor)->pos.x) / 5.0F;
		is_in_range = ((delta >= -1.0F) && (delta <= 1.0F)) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			SObjGetStruct(sMNPlayers1PTrainingSlots[player].cursor)->pos.x = sMNPlayers1PTrainingSlots[player].cursor_pickup_x;
		}
		else SObjGetStruct(sMNPlayers1PTrainingSlots[player].cursor)->pos.x += delta;

		delta = (sMNPlayers1PTrainingSlots[player].cursor_pickup_y - SObjGetStruct(sMNPlayers1PTrainingSlots[player].cursor)->pos.y) / 5.0F;
		is_in_range = ((delta >= -1.0F) && (delta <= 1.0F)) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			SObjGetStruct(sMNPlayers1PTrainingSlots[player].cursor)->pos.y = sMNPlayers1PTrainingSlots[player].cursor_pickup_y;
		}
		else SObjGetStruct(sMNPlayers1PTrainingSlots[player].cursor)->pos.y += delta;

		if
		(
			(SObjGetStruct(sMNPlayers1PTrainingSlots[player].cursor)->pos.x == sMNPlayers1PTrainingSlots[player].cursor_pickup_x) &&
			(SObjGetStruct(sMNPlayers1PTrainingSlots[player].cursor)->pos.y == sMNPlayers1PTrainingSlots[player].cursor_pickup_y)
		)
		{
			sMNPlayers1PTrainingSlots[player].is_cursor_adjusting = FALSE;
		}
		SObjGetStruct(gobj)->next->pos.x = SObjGetStruct(gobj)->pos.x + pos[sMNPlayers1PTrainingSlots[player].cursor_status].x;
		SObjGetStruct(gobj)->next->pos.y = SObjGetStruct(gobj)->pos.y + pos[sMNPlayers1PTrainingSlots[player].cursor_status].y;
	}
	else if (sMNPlayers1PTrainingSlots[player].is_recalling == FALSE)
	{
		is_in_range =
		(
			(gSYControllerDevices[sMNPlayers1PTrainingManPlayer].stick_range.x < -8) ||
			(gSYControllerDevices[sMNPlayers1PTrainingManPlayer].stick_range.x > 8)
		) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			delta = (gSYControllerDevices[sMNPlayers1PTrainingManPlayer].stick_range.x / 20.0F) + SObjGetStruct(gobj)->pos.x;
			is_in_range = ((delta >= 0.0F) && (delta <= 280.0F)) ? TRUE : FALSE;

			if (is_in_range != FALSE)
			{
				SObjGetStruct(gobj)->pos.x = delta;
				SObjGetStruct(gobj)->next->pos.x = SObjGetStruct(gobj)->pos.x + pos[sMNPlayers1PTrainingSlots[player].cursor_status].x;
			}
		}
		is_in_range =
		(
			(gSYControllerDevices[sMNPlayers1PTrainingManPlayer].stick_range.y < -8) ||
			(gSYControllerDevices[sMNPlayers1PTrainingManPlayer].stick_range.y > 8)
		) ? TRUE : FALSE;

		if (is_in_range != FALSE)
		{
			delta = (gSYControllerDevices[sMNPlayers1PTrainingManPlayer].stick_range.y / -20.0F) + SObjGetStruct(gobj)->pos.y;
			is_in_range = ((delta >= 10.0F) && (delta <= 205.0F)) ? TRUE : FALSE;

			if (is_in_range != FALSE)
			{
				SObjGetStruct(gobj)->pos.y = delta;
				SObjGetStruct(gobj)->next->pos.y = SObjGetStruct(gobj)->pos.y + pos[sMNPlayers1PTrainingSlots[player].cursor_status].y;
			}
		}
	}
}

// 0x80135430
void mnPlayers1PTrainingUpdateCursorNoRecall(GObj *gobj, s32 player)
{
	s32 i;

	if ((SObjGetStruct(gobj)->pos.y > 124.0F) || (SObjGetStruct(gobj)->pos.y < 38.0F))
	{
		if (sMNPlayers1PTrainingSlots[player].cursor_status != nMNPlayersCursorStatusPointer)
		{
			mnPlayers1PTrainingUpdateCursor(gobj, player, nMNPlayersCursorStatusPointer);
			sMNPlayers1PTrainingSlots[player].cursor_status = nMNPlayersCursorStatusPointer;
		}
	}
	else if (sMNPlayers1PTrainingSlots[player].held_player == -1)
	{
		if (sMNPlayers1PTrainingSlots[player].cursor_status != nMNPlayersCursorStatusHover)
		{
			mnPlayers1PTrainingUpdateCursor(gobj, player, nMNPlayersCursorStatusHover);
			sMNPlayers1PTrainingSlots[player].cursor_status = nMNPlayersCursorStatusHover;
		}
	}
	else if (sMNPlayers1PTrainingSlots[player].cursor_status != nMNPlayersCursorStatusGrab)
	{
		mnPlayers1PTrainingUpdateCursor(gobj, player, nMNPlayersCursorStatusGrab);
		sMNPlayers1PTrainingSlots[player].cursor_status = nMNPlayersCursorStatusGrab;
	}
	if ((sMNPlayers1PTrainingSlots[player].cursor_status == nMNPlayersCursorStatusPointer) && (sMNPlayers1PTrainingSlots[player].is_selected != FALSE))
	{
		for (i = 0; i < ARRAY_COUNT(sMNPlayers1PTrainingSlots); i++)
		{
			if ((sMNPlayers1PTrainingSlots[i].is_selected == TRUE) && (mnPlayers1PTrainingCheckPuckInRange(gobj, player, i) != FALSE))
			{
				mnPlayers1PTrainingUpdateCursor(gobj, player, nMNPlayersCursorStatusHover);
				sMNPlayers1PTrainingSlots[player].cursor_status = nMNPlayersCursorStatusHover;
				
				break;
			}
		}
	}
}

// 0x801355E0
void mnPlayers1PTrainingUpdateCostume(s32 player, s32 select_button)
{
	s32 costume = ftParamGetCostumeCommonID(sMNPlayers1PTrainingSlots[player].fkind, select_button);

	if (mnPlayers1PTrainingCheckCostumeUsed(sMNPlayers1PTrainingSlots[player].fkind, player, costume) != FALSE)
	{
		func_800269C0_275C0(nSYAudioFGMMenuDenied);
	}
	else
	{
		ftParamInitAllParts(sMNPlayers1PTrainingSlots[player].player, costume, 0);
		sMNPlayers1PTrainingSlots[player].costume = costume;

		func_800269C0_275C0(nSYAudioFGMMenuScroll2);
	}
}

// 0x80135674
sb32 mnPlayers1PTrainingCheckManFighterSelected(s32 player)
{
	if
	(
		(sMNPlayers1PTrainingSlots[player].is_selected) &&
		(sMNPlayers1PTrainingSlots[player].held_player == -1) &&
		(sMNPlayers1PTrainingSlots[player].pkind == nFTPlayerKindMan)
	)
	{
		return TRUE;
	}
	else return FALSE;
}

// 0x801356D0
void mnPlayers1PTrainingRecallPuck(s32 player)
{
	sMNPlayers1PTrainingSlots[player].is_fighter_selected = FALSE;
	sMNPlayers1PTrainingSlots[player].is_selected = FALSE;
	sMNPlayers1PTrainingSlots[player].is_recalling = TRUE;
	sMNPlayers1PTrainingSlots[player].recall_tics = 0;
	sMNPlayers1PTrainingSlots[player].recall_start_x = SObjGetStruct(sMNPlayers1PTrainingSlots[player].puck)->pos.x;
	sMNPlayers1PTrainingSlots[player].recall_start_y = SObjGetStruct(sMNPlayers1PTrainingSlots[player].puck)->pos.y;

	sMNPlayers1PTrainingSlots[player].recall_end_x = SObjGetStruct(sMNPlayers1PTrainingSlots[player].cursor)->pos.x + 20.0F;

	if (sMNPlayers1PTrainingSlots[player].recall_end_x > 280.0F)
	{
		sMNPlayers1PTrainingSlots[player].recall_end_x = 280.0F;
	}
	sMNPlayers1PTrainingSlots[player].recall_end_y = SObjGetStruct(sMNPlayers1PTrainingSlots[player].cursor)->pos.y + -15.0F;

	if (sMNPlayers1PTrainingSlots[player].recall_end_y < 10.0F)
	{
		sMNPlayers1PTrainingSlots[player].recall_end_y = 10.0F;
	}
	if (sMNPlayers1PTrainingSlots[player].recall_end_y < sMNPlayers1PTrainingSlots[player].recall_start_y)
	{
		sMNPlayers1PTrainingSlots[player].recall_mid_y = sMNPlayers1PTrainingSlots[player].recall_end_y - 20.0F;
	}
	else sMNPlayers1PTrainingSlots[player].recall_mid_y = sMNPlayers1PTrainingSlots[player].recall_start_y - 20.0F;
}

// 0x801357CC
void mnPlayers1PTrainingBackTo1PMode(void)
{
	gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
	gSCManagerSceneData.scene_curr = nSCKind1PMode;

	mnPlayers1PTrainingSetSceneData();
	syAudioStopBGMAll();
	func_800266A0_272A0();
	syTaskmanSetLoadScene();
}

// 0x80135818
void mnPlayers1PTrainingDetectBack(s32 player)
{
	if ((sMNPlayers1PTrainingTotalTimeTics >= 10) && (gSYControllerDevices[player].button_tap & B_BUTTON))
	{
		mnPlayers1PTrainingBackTo1PMode();
	}
}

// 0x8013586C
sb32 mnPlayers1PTrainingCheckBackInRange(GObj *gobj)
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

// 0x80135934
void mnPlayers1PTrainingCursorProcUpdate(GObj *gobj)
{
	s32 unused[5];
	s32 player = gobj->user_data.s;

	mnPlayers1PTrainingAdjustCursor(gobj, player);

	if
	(
		(gSYControllerDevices[sMNPlayers1PTrainingManPlayer].button_tap & A_BUTTON) &&
		(mnPlayers1PTrainingSelectFighter(gobj, player, sMNPlayers1PTrainingSlots[player].held_player, nMNPlayersSelectButtonA) == FALSE) &&
		(mnPlayers1PTrainingCheckCursorPuckGrab(gobj, player) == FALSE) &&
		(mnPlayers1PTrainingCheckBackInRange(gobj) != FALSE)
	)
	{
		mnPlayers1PTrainingBackTo1PMode();
		func_800269C0_275C0(nSYAudioFGMMenuScroll2);
	}
	if (sMNPlayers1PTrainingDefaultCostumeOnly == FALSE)
	{
		if
		(
			(gSYControllerDevices[sMNPlayers1PTrainingManPlayer].button_tap & U_CBUTTONS) &&
			(mnPlayers1PTrainingSelectFighter(gobj, player, sMNPlayers1PTrainingSlots[player].held_player, 0) == FALSE) &&
			(sMNPlayers1PTrainingSlots[player].is_fighter_selected != FALSE)
		)
		{
			mnPlayers1PTrainingUpdateCostume(player, 0);
		}
		if
		(
			(gSYControllerDevices[sMNPlayers1PTrainingManPlayer].button_tap & R_CBUTTONS) &&
			(mnPlayers1PTrainingSelectFighter(gobj, player, sMNPlayers1PTrainingSlots[player].held_player, 1) == FALSE) &&
			(sMNPlayers1PTrainingSlots[player].is_fighter_selected != FALSE)
		)
		{
			mnPlayers1PTrainingUpdateCostume(player, 1);
		}
		if
		(
			(gSYControllerDevices[sMNPlayers1PTrainingManPlayer].button_tap & D_CBUTTONS) &&
			(mnPlayers1PTrainingSelectFighter(gobj, player, sMNPlayers1PTrainingSlots[player].held_player, 2) == FALSE) &&
			(sMNPlayers1PTrainingSlots[player].is_fighter_selected != FALSE)
		)
		{
			mnPlayers1PTrainingUpdateCostume(player, 2);
		}
		if
		(
			(gSYControllerDevices[sMNPlayers1PTrainingManPlayer].button_tap & L_CBUTTONS) &&
			(mnPlayers1PTrainingSelectFighter(gobj, player, sMNPlayers1PTrainingSlots[player].held_player, 3) == FALSE) &&
			(sMNPlayers1PTrainingSlots[player].is_fighter_selected != FALSE)
		)
		{
			mnPlayers1PTrainingUpdateCostume(player, 3);
		}
	}

	if ((gSYControllerDevices[sMNPlayers1PTrainingManPlayer].button_tap & B_BUTTON) && (mnPlayers1PTrainingCheckManFighterSelected(player) != FALSE))
	{
		mnPlayers1PTrainingRecallPuck(player);
	}
	if (sMNPlayers1PTrainingSlots[player].is_recalling == FALSE)
	{
		mnPlayers1PTrainingDetectBack(player);
	}
	if (sMNPlayers1PTrainingSlots[player].is_recalling == FALSE)
	{
		mnPlayers1PTrainingUpdateCursorNoRecall(gobj, player);
	}
}

// 0x80135C18
void mnPlayers1PTrainingUpdatePuck(GObj *gobj, s32 puck)
{
	SObj *sobj;
	f32 pos_x, pos_y;
	intptr_t offsets[/* */] =
	{
		llMNPlayersCommon1PPuckSprite,
		llMNPlayersCommon2PPuckSprite,
		llMNPlayersCommon3PPuckSprite,
		llMNPlayersCommon4PPuckSprite,
		llMNPlayersCommonCPPuckSprite
	};

	pos_x = SObjGetStruct(gobj)->pos.x;
	pos_y = SObjGetStruct(gobj)->pos.y;

	gcRemoveSObjAll(gobj);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], offsets[puck]));
	sobj->pos.x = pos_x;
	sobj->pos.y = pos_y;
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
}

// 0x80135CCC
void mnPlayers1PTrainingCenterPuckInPortrait(GObj *gobj, s32 fkind)
{
	s32 portrait = mnPlayers1PTrainingGetPortrait(fkind);

	if (portrait >= 6)
	{
		SObjGetStruct(gobj)->pos.x = (portrait * 45) - (6 * 45) + 36;
		SObjGetStruct(gobj)->pos.y = 89.0F;
	}
	else
	{
		SObjGetStruct(gobj)->pos.x = (portrait * 45) + 36;
		SObjGetStruct(gobj)->pos.y = 46.0F;
	}
}

// 0x80135D7C - Unused?
void func_ovl28_80135D7C(void)
{
	return;
}

// 0x80135D84
void mnPlayers1PTrainingMovePuck(s32 player)
{
	SObjGetStruct(sMNPlayers1PTrainingSlots[player].puck)->pos.x += sMNPlayers1PTrainingSlots[player].puck_vel_x;
	SObjGetStruct(sMNPlayers1PTrainingSlots[player].puck)->pos.y += sMNPlayers1PTrainingSlots[player].puck_vel_y;
}

// 0x80135DD8
void mnPlayers1PTrainingPuckProcUpdate(GObj *gobj)
{
	s32 fkind;
	s32 player = gobj->user_data.s;

	if (sMNPlayers1PTrainingTotalTimeTics < 30)
	{
		gobj->flags = GOBJ_FLAG_HIDDEN;
	}
	else if
	(
		(sMNPlayers1PTrainingSlots[player].pkind == nFTPlayerKindCom) ||
		(
			(sMNPlayers1PTrainingSlots[player].pkind == nFTPlayerKindMan) &&
			(
				(sMNPlayers1PTrainingSlots[player].cursor_status != nMNPlayersCursorStatusPointer) ||
				(sMNPlayers1PTrainingSlots[player].is_selected == TRUE) ||
				(sMNPlayers1PTrainingSlots[player].is_recalling == TRUE)
			)
		)
	)
	{
		gobj->flags = GOBJ_FLAG_NONE;
	}
	else gobj->flags = GOBJ_FLAG_HIDDEN;
	
	if ((sMNPlayers1PTrainingSlots[player].pkind) != nFTPlayerKindNot)
	{
		if ((sMNPlayers1PTrainingSlots[player].is_selected == FALSE) && (sMNPlayers1PTrainingSlots[player].holder_player != GMCOMMON_PLAYERS_MAX))
		{
			if (sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingSlots[player].holder_player].is_cursor_adjusting == FALSE)
			{
				SObjGetStruct(gobj)->pos.x = SObjGetStruct(sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingSlots[player].holder_player].cursor)->pos.x + 11.0F;
				SObjGetStruct(gobj)->pos.y = SObjGetStruct(sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingSlots[player].holder_player].cursor)->pos.y + -14.0F;
			}
		}
		else mnPlayers1PTrainingMovePuck(player);

		fkind = mnPlayers1PTrainingGetPuckFighterKind(player);

		if
		(
			(sMNPlayers1PTrainingSlots[player].pkind == nFTPlayerKindCom) &&
			(fkind != sMNPlayers1PTrainingSlots[player].fkind) &&
			(fkind == nFTKindNull)
		)
		{
			if (sMNPlayers1PTrainingSlots[player].holder_player != GMCOMMON_PLAYERS_MAX)
			{
				mnPlayers1PTrainingSelectFighterPuck(sMNPlayers1PTrainingSlots[player].holder_player, 4);
			}
		}
		if ((sMNPlayers1PTrainingSlots[player].is_selected == FALSE) && (fkind != sMNPlayers1PTrainingSlots[player].fkind))
		{
			sMNPlayers1PTrainingSlots[player].fkind = fkind;

			mnPlayers1PTrainingUpdateFighter(player);
			mnPlayers1PTrainingUpdateNameAndEmblem(player);
		}
	}
}

// 0x80135FE8
void mnPlayers1PTrainingMakeCursorCamera(void)
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

// 0x80136088
void mnPlayers1PTrainingMakePuckCamera(void)
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
			25,
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

// 0x80136128
void mnPlayers1PTrainingMakeHandicapLevelCamera(void)
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
			43,
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

// 0x801361C8
void mnPlayers1PTrainingMakeReadyCamera(void)
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
			COBJ_MASK_DLLINK(38),
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

// 0x80136268
void mnPlayers1PTrainingMakeCursor(s32 player)
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

	sMNPlayers1PTrainingSlots[player].cursor = gobj = lbCommonMakeSpriteGObj
	(
		0,
		NULL,
		19,
		GOBJ_PRIORITY_DEFAULT,
		lbCommonDrawSObjAttr,
		32,
		priorities[player],
		~0,
		lbRelocGetFileData
		(
			Sprite*,
			sMNPlayers1PTrainingFiles[0],
			llMNPlayersCommonCursorHandGrabSprite
		),
		nGCProcessKindFunc,
		mnPlayers1PTrainingCursorProcUpdate,
		2
	);
	gobj->user_data.s = player;

	SObjGetStruct(gobj)->pos.x = 70.0F;
	SObjGetStruct(gobj)->pos.y = 170.0F;
	SObjGetStruct(gobj)->sprite.attr &= ~SP_FASTCOPY;
	SObjGetStruct(gobj)->sprite.attr |= SP_TRANSPARENT;

	mnPlayers1PTrainingUpdateCursor(gobj, player, nMNPlayersCursorStatusPointer);
}

// 0x801363C8
void mnPlayers1PTrainingPuckProcDisplay(GObj *gobj)
{
	/* Six RDP commands -- prim white, env the glow,
	 * (TEXEL0 - PRIM) * ENV + PRIM and G_RM_AA_XLU_SURF -- in the port's
	 * spelling (src/dc/lbcommon.h nLBCommonCombineTexPrimEnv), as the VS
	 * select's own mnPlayersVSPuckProcDisplay writes them. */
	lbCommonSpriteSetPrimColor(0xFF, 0xFF, 0xFF, 0xFF);
	lbCommonSpriteSetEnvColor
	(
		sMNPlayers1PTrainingPuckGlowColor,
		sMNPlayers1PTrainingPuckGlowColor,
		sMNPlayers1PTrainingPuckGlowColor,
		sMNPlayers1PTrainingPuckGlowColor
	);
	lbCommonSpriteSetCombine(nLBCommonCombineTexPrimEnv);

	lbCommonDrawSObjNoAttr(gobj);
}

// 0x801364C0
void mnPlayers1PTrainingMakePuck(s32 player)
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

	s32 dropped_priorities[/* */] = { 3, 2, 1, 0 };
	s32 held_priorities[/* */] = { 6, 4, 2, 0 };

	sMNPlayers1PTrainingSlots[player].puck = gobj = lbCommonMakeSpriteGObj
	(
		0,
		NULL,
		20,
		GOBJ_PRIORITY_DEFAULT,
		mnPlayers1PTrainingPuckProcDisplay,
		33,
		dropped_priorities[player],
		~0,
		lbRelocGetFileData
		(
			Sprite*,
			sMNPlayers1PTrainingFiles[0],
			offsets[player]
		),
		nGCProcessKindFunc,
		mnPlayers1PTrainingPuckProcUpdate,
		1
	);
	gobj->user_data.s = player;

	if (sMNPlayers1PTrainingSlots[player].pkind == nFTPlayerKindCom)
	{
		mnPlayers1PTrainingUpdatePuck(gobj, GMCOMMON_PLAYERS_MAX);
	}
	if ((sMNPlayers1PTrainingSlots[player].pkind == nFTPlayerKindMan) && (sMNPlayers1PTrainingSlots[player].held_player != -1))
	{
		gcMoveGObjDL(sMNPlayers1PTrainingSlots[player].puck, 32, held_priorities[player] + 1);
	}
	if (sMNPlayers1PTrainingSlots[player].fkind == nFTKindNull)
	{
		SObjGetStruct(gobj)->pos.x = 51.0F;
		SObjGetStruct(gobj)->pos.y = 161.0F;
	}
	else mnPlayers1PTrainingCenterPuckInPortrait(gobj, sMNPlayers1PTrainingSlots[player].fkind);

	SObjGetStruct(gobj)->sprite.attr &= ~SP_FASTCOPY;
	SObjGetStruct(gobj)->sprite.attr |= SP_TRANSPARENT;
}

// 0x801366D0
f32 mnPlayers1PTrainingGetPuckDistance(s32 this_player, s32 other_player)
{
	return sqrtf
	(
		SQUARE(SObjGetStruct(sMNPlayers1PTrainingSlots[other_player].puck)->pos.x - SObjGetStruct(sMNPlayers1PTrainingSlots[this_player].puck)->pos.x) +
		SQUARE(SObjGetStruct(sMNPlayers1PTrainingSlots[other_player].puck)->pos.y - SObjGetStruct(sMNPlayers1PTrainingSlots[this_player].puck)->pos.y)
	);
}

// 0x80136748
void mnPlayers1PTrainingPuckAdjustOverlap(s32 this_player, s32 other_player, f32 unused)
{
	s32 unused2;

	if (SObjGetStruct(sMNPlayers1PTrainingSlots[this_player].puck)->pos.x == SObjGetStruct(sMNPlayers1PTrainingSlots[other_player].puck)->pos.x)
	{
		sMNPlayers1PTrainingSlots[this_player].puck_vel_x += syUtilsRandIntRange(2) - 1;
	}
	else
	{
		sMNPlayers1PTrainingSlots[this_player].puck_vel_x +=
		(
			-1.0F * (SObjGetStruct(sMNPlayers1PTrainingSlots[other_player].puck)->pos.x - SObjGetStruct(sMNPlayers1PTrainingSlots[this_player].puck)->pos.x)
		) / 10.0F;
	}
	if (SObjGetStruct(sMNPlayers1PTrainingSlots[this_player].puck)->pos.y == SObjGetStruct(sMNPlayers1PTrainingSlots[other_player].puck)->pos.y)
	{
		sMNPlayers1PTrainingSlots[this_player].puck_vel_y += syUtilsRandIntRange(2) - 1;
	}
	else
	{
		sMNPlayers1PTrainingSlots[this_player].puck_vel_y +=
		(
			-1.0F * (SObjGetStruct(sMNPlayers1PTrainingSlots[other_player].puck)->pos.y - SObjGetStruct(sMNPlayers1PTrainingSlots[this_player].puck)->pos.y)
		) / 10.0F;
	}
}

// 0x80136888
void mnPlayers1PTrainingPuckAdjustPortraitEdge(s32 player)
{
	s32 portrait = mnPlayers1PTrainingGetPortrait(sMNPlayers1PTrainingSlots[player].fkind);
	f32 portrait_edge_x = ((portrait >= 6) ? portrait - 6 : portrait) * 45 + 25;
	f32 portrait_edge_y = ((portrait >= 6) ? 1 : 0) * 43 + 36;
	f32 new_pos_x = SObjGetStruct(sMNPlayers1PTrainingSlots[player].puck)->pos.x + sMNPlayers1PTrainingSlots[player].puck_vel_x + 13.0F;
	f32 new_pos_y = SObjGetStruct(sMNPlayers1PTrainingSlots[player].puck)->pos.y + sMNPlayers1PTrainingSlots[player].puck_vel_y + 12.0F;

	if (new_pos_x < (portrait_edge_x + 5.0F))
	{
		sMNPlayers1PTrainingSlots[player].puck_vel_x = ((portrait_edge_x + 5.0F) - new_pos_x) / 10.0F;
	}
	if (((portrait_edge_x + 45.0F) - 5.0F) < new_pos_x)
	{
		sMNPlayers1PTrainingSlots[player].puck_vel_x = ((new_pos_x - ((portrait_edge_x + 45.0F) - 5.0F)) * -1.0F) / 10.0F;
	}
	if (new_pos_y < (portrait_edge_y + 5.0F))
	{
		sMNPlayers1PTrainingSlots[player].puck_vel_y = ((portrait_edge_y + 5.0F) - new_pos_y) / 10.0F;
	}
	if (((portrait_edge_y + 43.0F) - 5.0F) < new_pos_y)
	{
		sMNPlayers1PTrainingSlots[player].puck_vel_y = ((new_pos_y - ((portrait_edge_y + 43.0F) - 5.0F)) * -1.0F) / 10.0F;
	}
}

// 0x80136A44
void mnPlayers1PTrainingPuckAdjustPlaced(s32 player)
{
	s32 other_player = (player == sMNPlayers1PTrainingManPlayer) ? sMNPlayers1PTrainingComPlayer : sMNPlayers1PTrainingManPlayer;
	f32 dists[GMCOMMON_PLAYERS_MAX];

	if (player != other_player)
	{
		if (sMNPlayers1PTrainingSlots[player].is_selected != FALSE)
		{
			dists[other_player] = mnPlayers1PTrainingGetPuckDistance(player, other_player);
		}
	}
	else dists[other_player] = -1.0F;

	sMNPlayers1PTrainingSlots[player].puck_vel_x = 0.0F;
	sMNPlayers1PTrainingSlots[player].puck_vel_y = 0.0F;

	if
	(
		(IS_BETWEEN(dists[other_player], 0.0F, 15.0F)) &&
		(sMNPlayers1PTrainingSlots[player].fkind == sMNPlayers1PTrainingSlots[other_player].fkind) &&
		(sMNPlayers1PTrainingSlots[player].fkind != nFTKindNull) &&
		(sMNPlayers1PTrainingSlots[other_player].is_selected == TRUE)
	)
	{
		mnPlayers1PTrainingPuckAdjustOverlap(player, other_player, (15.0F - dists[other_player]) / 15.0F);
	}
	mnPlayers1PTrainingPuckAdjustPortraitEdge(player);
}

// 0x80136BE0
void mnPlayers1PTrainingPuckAdjustRecall(s32 player)
{
	f32 vel_y, vel_x;

	sMNPlayers1PTrainingSlots[player].recall_tics++;

	if (sMNPlayers1PTrainingSlots[player].recall_tics < 11)
	{
		vel_x = (sMNPlayers1PTrainingSlots[player].recall_end_x - sMNPlayers1PTrainingSlots[player].recall_start_x) / 10.0F;

		if (sMNPlayers1PTrainingSlots[player].recall_tics < 6)
		{
			vel_y = (sMNPlayers1PTrainingSlots[player].recall_mid_y - sMNPlayers1PTrainingSlots[player].recall_start_y) / 5.0F;
		}	
		else vel_y = (sMNPlayers1PTrainingSlots[player].recall_end_y - sMNPlayers1PTrainingSlots[player].recall_mid_y) / 5.0F;

		sMNPlayers1PTrainingSlots[player].puck_vel_x = vel_x;
		sMNPlayers1PTrainingSlots[player].puck_vel_y = vel_y;
	}
	else if (sMNPlayers1PTrainingSlots[player].recall_tics == 11)
	{
		mnPlayers1PTrainingSetCursorGrab(player, player);

		sMNPlayers1PTrainingSlots[player].puck_vel_x = 0.0F;
		sMNPlayers1PTrainingSlots[player].puck_vel_y = 0.0F;
	}
	if (sMNPlayers1PTrainingSlots[player].recall_tics == 30)
	{
		sMNPlayers1PTrainingSlots[player].is_recalling = FALSE;
	}
}

// 0x80136CCC
void mnPlayers1PTrainingPuckAdjustProcUpdate(GObj *gobj)
{
	if (sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].is_recalling != FALSE)
	{
		mnPlayers1PTrainingPuckAdjustRecall(sMNPlayers1PTrainingManPlayer);
	}
	if (sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].is_selected != FALSE)
	{
		mnPlayers1PTrainingPuckAdjustPlaced(sMNPlayers1PTrainingManPlayer);
	}
	if (sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].is_recalling != FALSE)
	{
		mnPlayers1PTrainingPuckAdjustRecall(sMNPlayers1PTrainingComPlayer);
	}
	if (sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].is_selected != FALSE)
	{
		mnPlayers1PTrainingPuckAdjustPlaced(sMNPlayers1PTrainingComPlayer);
	}
}

// 0x80136DD8
void mnPlayers1PTrainingMakePuckAdjust(void)
{
	gcAddGObjProcess(gcMakeGObjSPAfter(0, NULL, 26, GOBJ_PRIORITY_DEFAULT), mnPlayers1PTrainingPuckAdjustProcUpdate, nGCProcessKindFunc, 1);
}

// 0x80136E1C
void mnPlayers1PTrainingPuckGlowProcUpdate(GObj *gobj)
{
	if (sMNPlayers1PTrainingIsPuckGlowIncreasing == FALSE)
	{
		sMNPlayers1PTrainingPuckGlowColor += 0x09;

		if (sMNPlayers1PTrainingPuckGlowColor > 0xFF)
		{
			sMNPlayers1PTrainingPuckGlowColor = 0xFF;
			sMNPlayers1PTrainingIsPuckGlowIncreasing = TRUE;
		}
	}
	if (sMNPlayers1PTrainingIsPuckGlowIncreasing == TRUE)
	{
		sMNPlayers1PTrainingPuckGlowColor -= 0x09;

		if (sMNPlayers1PTrainingPuckGlowColor < 0x80)
		{
			sMNPlayers1PTrainingPuckGlowColor = 0x80;
			sMNPlayers1PTrainingIsPuckGlowIncreasing = FALSE;
		}
	}
}

// 0x80136E94
void mnPlayers1PTrainingMakePuckGlow(void)
{
	gcAddGObjProcess(gcMakeGObjSPAfter(0, NULL, 26, GOBJ_PRIORITY_DEFAULT), mnPlayers1PTrainingPuckGlowProcUpdate, nGCProcessKindFunc, 1);
}

// 0x80136ED8
void mnPlayers1PTrainingCostumeSyncProcUpdate(GObj *gobj)
{
	s32 i;
	s32 costume;

	for (i = 0; i < ARRAY_COUNT(sMNPlayers1PTrainingSlots); i++)
	{
		if ((sMNPlayers1PTrainingSlots[i].fkind != nFTKindNull) && (mnPlayers1PTrainingGetFighterKindCount(sMNPlayers1PTrainingSlots[i].fkind) == FALSE))
		{
			costume = ftParamGetCostumeCommonID(sMNPlayers1PTrainingSlots[i].fkind, 0);

			if ((costume != sMNPlayers1PTrainingSlots[i].costume) && (sMNPlayers1PTrainingSlots[i].is_fighter_selected == FALSE))
			{
				ftParamInitAllParts(sMNPlayers1PTrainingSlots[i].player, costume, 0);
				sMNPlayers1PTrainingSlots[i].costume = costume;
			}
		}
	}
}

// 0x80136F84
void mnPlayers1PTrainingMakeCostumeSync(void)
{
	gcAddGObjProcess(gcMakeGObjSPAfter(0, NULL, 31, GOBJ_PRIORITY_DEFAULT), mnPlayers1PTrainingCostumeSyncProcUpdate, nGCProcessKindFunc, 1);
}

// 0x80136FC8
void mnPlayers1PTrainingSpotlightProcUpdate(GObj *gobj)
{
	s32 player = gobj->user_data.s;
	f32 sizes[/* */] =
	{
		1.5F, 1.5F, 2.0F, 1.5F, 1.5F, 1.5F,
		1.5F, 1.5F, 1.5F, 1.5F, 1.5F, 1.5F
	};

	if ((sMNPlayers1PTrainingSlots[player].is_fighter_selected == FALSE) && (sMNPlayers1PTrainingSlots[player].fkind != nFTKindNull))
	{
		gobj->flags = (gobj->flags == GOBJ_FLAG_HIDDEN) ? GOBJ_FLAG_NONE : GOBJ_FLAG_HIDDEN;

		DObjGetStruct(gobj)->scale.vec.f.x = sizes[sMNPlayers1PTrainingSlots[player].fkind];
		DObjGetStruct(gobj)->scale.vec.f.y = sizes[sMNPlayers1PTrainingSlots[player].fkind];
		DObjGetStruct(gobj)->scale.vec.f.y = sizes[sMNPlayers1PTrainingSlots[player].fkind];
	}
	else gobj->flags = GOBJ_FLAG_HIDDEN;
}

// 0x801370BC
void mnPlayers1PTrainingMakeSpotlight(void)
{
	s32 i;

#ifndef FT_HOSTTEST
	{
		int pal_bank = 0;

		if (fighter_load_scene(&sMNPlayers1PTrainingSpotlightPack,
		                 MNPLAYERS1PTRAINING_MODEL_SPOTLIGHT, &pal_bank) != 0)
		{
			return;
		}
	}
#endif
	for (i = 0; i < ARRAY_COUNT(sMNPlayers1PTrainingSlots); i++)
	{
		GObj *gobj = gcMakeGObjSPAfter(0, NULL, 21, GOBJ_PRIORITY_DEFAULT);

		/* DIVERGES as the VS select's: gcSetupCommonDObjs over relocData
		 * 22's DObjDesc, gcAddMObjAll over its MObjSubs and
		 * gcDrawDObjTreeDLLinksForGObj become the pack's own tree, its
		 * materials and the port's display proc. */
#ifndef FT_HOSTTEST
		fighter_clone(&sMNPlayers1PTrainingSpotlightModels[i],
		              &sMNPlayers1PTrainingSpotlightPack,
		              syTaskmanMalloc(sizeof(float[4]) *
		                              sMNPlayers1PTrainingSpotlightPack.hd->vert_count,
		                              0x8));
		dc_model_add_dobjs(gobj, NULL, &sMNPlayers1PTrainingSpotlightModels[i], NULL);
		gcAddGObjDisplay(gobj, dc_model_proc_display_layered, 9,
		                 GOBJ_PRIORITY_DEFAULT, ~0);
#else
		/* the host cross-test links the scene and not the renderer, so
		 * there is no pack to build a tree from and the placement below
		 * still needs one DObj to land on */
		gcAddDObjRpyR(gobj, NULL);
#endif
		gobj->user_data.s = i;

#ifndef FT_HOSTTEST
		dc_model_add_mobjs(gobj, &sMNPlayers1PTrainingSpotlightModels[i], 0.0F);
#endif
		gcAddGObjProcess(gobj, mnPlayers1PTrainingSpotlightProcUpdate, nGCProcessKindFunc, 1);
		gcPlayAnimAll(gobj);

		if (i == sMNPlayers1PTrainingManPlayer)
		{
			DObjGetStruct(gobj)->translate.vec.f.x = -830.0F;
			DObjGetStruct(gobj)->translate.vec.f.y = -870.0F;
			DObjGetStruct(gobj)->translate.vec.f.z = 0.0F;
		}
		else
		{
			DObjGetStruct(gobj)->translate.vec.f.x = 830.0F;
			DObjGetStruct(gobj)->translate.vec.f.y = -870.0F;
			DObjGetStruct(gobj)->translate.vec.f.z = 0.0F;
		}
	}
}

// 0x80137254 - bruh
void mnPlayers1PTrainingReadyProcDisplay(GObj *gobj)
{
	lbCommonDrawSObjAttr(gobj);
}

// 0x80137274
sb32 mnPlayers1PTrainingCheckReady(void)
{
	if
	(
		(sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].is_fighter_selected != FALSE) &&
		(sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].is_fighter_selected != FALSE)
	)
	{
		return TRUE;
	}
	else return FALSE;
}

// 0x801372D4
void mnPlayers1PTrainingReadyProcUpdate(GObj *gobj)
{
	if (mnPlayers1PTrainingCheckReady() != FALSE)
	{
		sMNPlayers1PTrainingReadyBlinkWait++;

		if (sMNPlayers1PTrainingReadyBlinkWait == 40)
		{
			sMNPlayers1PTrainingReadyBlinkWait = 0;
		}
		gobj->flags = (sMNPlayers1PTrainingReadyBlinkWait < 30) ? GOBJ_FLAG_NONE : GOBJ_FLAG_HIDDEN;
	}
	else
	{
		gobj->flags = GOBJ_FLAG_HIDDEN;
		sMNPlayers1PTrainingReadyBlinkWait = 0;
	}
}

// 0x80137354
void mnPlayers1PTrainingMakeReady(void)
{
	GObj *gobj;
	SObj *sobj;

	gobj = gcMakeGObjSPAfter(0, NULL, 32, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, mnPlayers1PTrainingReadyProcDisplay, 38, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, mnPlayers1PTrainingReadyProcUpdate, nGCProcessKindFunc, 1);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], llMNPlayersCommonReadyBannerSprite));
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

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], llMNPlayersCommonReadyToFightTextSprite));
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
	gcAddGObjDisplay(gobj, mnPlayers1PTrainingReadyProcDisplay, 28, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, mnPlayers1PTrainingReadyProcUpdate, nGCProcessKindFunc, 1);

#if defined(REGION_US)
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], llMNPlayersCommonPressTextSprite));
#else
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], llMNPlayersCommonPushTextSprite));
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

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], llMNPlayersCommonStartTextSprite));
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
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNPlayers1PTrainingFiles[0], llMNPlayersCommonButtonTextSprite));
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;
	sobj->sprite.red = 0xD6;
	sobj->sprite.green = 0xDD;
	sobj->sprite.blue = 0xC6;
	sobj->pos.x = 171.0F;
	sobj->pos.y = 219.0F;
#endif
}

// 0x801375D0 - Unused?
void func_ovl28_801375D0(void)
{
	return;
}

// 0x801375D8
void mnPlayers1PTrainingSetSceneData(void)
{
	gSCManagerSceneData.training_man_fkind = sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].fkind;
	gSCManagerSceneData.training_man_costume = sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].costume;

	gSCManagerSceneData.training_com_fkind = sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].fkind;
	gSCManagerSceneData.training_com_costume = sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].costume;
}

// 0x80137638
void mnPlayers1PTrainingPauseSlotProcesses(void)
{
	if (sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].cursor != NULL)
	{
		gcPauseGObjProcess(sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].cursor->gobjproc_head);
	}
	if (sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].puck != NULL)
	{
		gcPauseGObjProcess(sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].puck->gobjproc_head);
	}
	if (sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].puck != NULL)
	{
		gcPauseGObjProcess(sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].puck->gobjproc_head);
	}
}

// 0x80137700
void mnPlayers1PTrainingFuncRun(GObj *gobj)
{
	sMNPlayers1PTrainingTotalTimeTics++;

	if (sMNPlayers1PTrainingTotalTimeTics == sMNPlayers1PTrainingReturnTic)
	{
		gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
		gSCManagerSceneData.scene_curr = nSCKindTitle;

		mnPlayers1PTrainingSetSceneData();
		syTaskmanSetLoadScene();
	}
	else
	{
		if (scSubsysControllerCheckNoInputAll() == FALSE)
		{
			sMNPlayers1PTrainingReturnTic = sMNPlayers1PTrainingTotalTimeTics + I_MIN_TO_TICS(5);
		}
		if (sMNPlayers1PTrainingIsStart != FALSE)
		{
			sMNPlayers1PTrainingStartProceedWait--;

			if (sMNPlayers1PTrainingStartProceedWait == 0)
			{
				gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
				gSCManagerSceneData.scene_curr = nSCKindMaps;

				mnPlayers1PTrainingSetSceneData();
				syTaskmanSetLoadScene();
			}
		}
		else if ((scSubsysControllerGetPlayerTapButtons(START_BUTTON) != FALSE) && (sMNPlayers1PTrainingTotalTimeTics > 60))
		{
			if (mnPlayers1PTrainingCheckReady() != FALSE)
			{
				func_800269C0_275C0(nSYAudioVoicePublicCheer);

				sMNPlayers1PTrainingStartProceedWait = 30;
				sMNPlayers1PTrainingIsStart = TRUE;

				mnPlayers1PTrainingPauseSlotProcesses();
			}
			else func_800269C0_275C0(nSYAudioFGMMenuDenied);
		}
	}
}

// 0x8013784C
void mnPlayers1PTrainingInitPlayer(s32 player)
{
	sMNPlayers1PTrainingSlots[player].team_color_button = NULL;
	sMNPlayers1PTrainingSlots[player].handicap_cpu_level = NULL;
	sMNPlayers1PTrainingSlots[player].arrows = NULL;
	sMNPlayers1PTrainingSlots[player].handicap_cpu_level_value = NULL;
	sMNPlayers1PTrainingSlots[player].flash = NULL;
	sMNPlayers1PTrainingSlots[player].p_sfx = NULL;
	sMNPlayers1PTrainingSlots[player].sfx_id = 0;
	sMNPlayers1PTrainingSlots[player].player = NULL;

	if (player == sMNPlayers1PTrainingManPlayer)
	{
		sMNPlayers1PTrainingSlots[player].pkind = nFTPlayerKindMan;
		sMNPlayers1PTrainingSlots[player].holder_player = GMCOMMON_PLAYERS_MAX;
		sMNPlayers1PTrainingSlots[player].held_player = -1;
		sMNPlayers1PTrainingSlots[player].fkind = gSCManagerSceneData.training_man_fkind;
		sMNPlayers1PTrainingSlots[player].costume = gSCManagerSceneData.training_man_costume;
		sMNPlayers1PTrainingSlots[player].is_fighter_selected = TRUE;
		sMNPlayers1PTrainingSlots[player].is_selected = TRUE;
		sMNPlayers1PTrainingSlots[player].is_recalling = FALSE;

	}
	else
	{
		sMNPlayers1PTrainingSlots[player].holder_player = GMCOMMON_PLAYERS_MAX;
		sMNPlayers1PTrainingSlots[player].held_player = -1;
		sMNPlayers1PTrainingSlots[player].is_fighter_selected = TRUE;
		sMNPlayers1PTrainingSlots[player].is_selected = TRUE;
		sMNPlayers1PTrainingSlots[player].is_recalling = FALSE;
	}
}

// 0x80137900
void mnPlayers1PTrainingResetPlayer(s32 player)
{
	sMNPlayers1PTrainingSlots[player].team_color_button = NULL;
	sMNPlayers1PTrainingSlots[player].handicap_cpu_level = NULL;
	sMNPlayers1PTrainingSlots[player].arrows = NULL;
	sMNPlayers1PTrainingSlots[player].handicap_cpu_level_value = NULL;
	sMNPlayers1PTrainingSlots[player].flash = NULL;
	sMNPlayers1PTrainingSlots[player].player = NULL;
	sMNPlayers1PTrainingSlots[player].p_sfx = NULL;
	sMNPlayers1PTrainingSlots[player].sfx_id = 0;
	sMNPlayers1PTrainingSlots[player].is_selected = FALSE;
	sMNPlayers1PTrainingSlots[player].fkind = nFTKindNull;
	sMNPlayers1PTrainingSlots[player].is_recalling = FALSE;
	sMNPlayers1PTrainingSlots[player].pkind = nFTPlayerKindMan;
	sMNPlayers1PTrainingSlots[player].holder_player = player;
	sMNPlayers1PTrainingSlots[player].held_player = player;
}

// 0x80137960
void mnPlayers1PTrainingResetPlayerNot(s32 player)
{
	sMNPlayers1PTrainingSlots[player].team_color_button = NULL;
	sMNPlayers1PTrainingSlots[player].handicap_cpu_level = NULL;
	sMNPlayers1PTrainingSlots[player].arrows = NULL;
	sMNPlayers1PTrainingSlots[player].handicap_cpu_level_value = NULL;
	sMNPlayers1PTrainingSlots[player].flash = NULL;
	sMNPlayers1PTrainingSlots[player].player = NULL;
	sMNPlayers1PTrainingSlots[player].p_sfx = NULL;
	sMNPlayers1PTrainingSlots[player].sfx_id = 0;
	sMNPlayers1PTrainingSlots[player].is_selected = FALSE;
	sMNPlayers1PTrainingSlots[player].fkind = nFTKindNull;
	sMNPlayers1PTrainingSlots[player].is_recalling = FALSE;
	sMNPlayers1PTrainingSlots[player].pkind = nFTPlayerKindNot;
	sMNPlayers1PTrainingSlots[player].holder_player = GMCOMMON_PLAYERS_MAX;
	sMNPlayers1PTrainingSlots[player].held_player = -1;
}

// 0x801379CC
void mnPlayers1PTrainingInitVars(void)
{
	s32 i;
	s32 fkind, costume;

	sMNPlayers1PTrainingTotalTimeTics = 0;
	sMNPlayers1PTrainingIsStart = FALSE;
	sMNPlayers1PTrainingReturnTic = sMNPlayers1PTrainingTotalTimeTics + I_MIN_TO_TICS(5);
	sMNPlayers1PTrainingManPlayer = gSCManagerSceneData.player;
	sMNPlayers1PTrainingComPlayer = (sMNPlayers1PTrainingManPlayer == 0) ? 1 : 0;
	sMNPlayers1PTrainingFighterMask = gSCManagerBackupData.fighter_mask;

	if (gSCManagerSceneData.training_man_fkind == nFTKindNull)
	{
		mnPlayers1PTrainingResetPlayer(sMNPlayers1PTrainingManPlayer);
	}
	else mnPlayers1PTrainingInitPlayer(sMNPlayers1PTrainingManPlayer);

	sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingManPlayer].recall_end_tic = FALSE;

	fkind = gSCManagerSceneData.training_com_fkind;

	if (fkind == nFTKindNull)
	{
		do
		{
			do
			{
				fkind = syUtilsRandTimeUCharRange(nFTKindPlayableEnd + 1);
			}
			while (mnPlayers1PTrainingCheckFighterCrossed(fkind) != FALSE);
		}
		while (mnPlayers1PTrainingCheckFighterLocked(fkind) != FALSE);

		if (mnPlayers1PTrainingCheckCostumeUsed(fkind, sMNPlayers1PTrainingComPlayer, ftParamGetCostumeCommonID(fkind, 0)) != FALSE)
		{
			costume = ftParamGetCostumeCommonID(fkind, 1);
		}
		else costume = ftParamGetCostumeCommonID(fkind, 0);
	}
	else costume = gSCManagerSceneData.training_com_costume;

	sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].fkind = fkind;
	sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].costume = costume;
	sMNPlayers1PTrainingSlots[sMNPlayers1PTrainingComPlayer].pkind = nFTPlayerKindCom;
	mnPlayers1PTrainingInitPlayer(sMNPlayers1PTrainingComPlayer);

	for (i = 0; i < ARRAY_COUNT(sMNPlayers1PTrainingSlots); i++)
	{
		if ((i != sMNPlayers1PTrainingManPlayer) && (i != sMNPlayers1PTrainingComPlayer))
		{
			mnPlayers1PTrainingResetPlayerNot(i);
		}
	}
}

// 0x80137BB0
void mnPlayers1PTrainingInitSlot(s32 player)
{
	s32 fkind;

	if (player == sMNPlayers1PTrainingManPlayer)
	{
		mnPlayers1PTrainingMakeCursor(player);
	}
	else sMNPlayers1PTrainingSlots[player].cursor = NULL;

	mnPlayers1PTrainingMakePuck(player);
	mnPlayers1PTrainingMakeGate(player);

	if (sMNPlayers1PTrainingSlots[player].is_selected != FALSE)
	{
		fkind = sMNPlayers1PTrainingSlots[player].fkind;

		if (fkind != nFTKindNull)
		{
			mnPlayers1PTrainingMakeFighter(sMNPlayers1PTrainingSlots[player].player, player, fkind, sMNPlayers1PTrainingSlots[player].costume);
		}
	}
}

// 0x80137C74
void mnPlayers1PTrainingInitSlotAll(void)
{
	mnPlayers1PTrainingInitSlot(sMNPlayers1PTrainingManPlayer);
	mnPlayers1PTrainingInitSlot(sMNPlayers1PTrainingComPlayer);
	mnPlayers1PTrainingSetGateLUTAll();
}

/* mnplayers1ptraining.c:3155-3166: lbRelocInitSetup and
 * lbRelocLoadFilesListed over dMNPlayers1PTrainingFileIDs; here, the
 * banks. The eighth file is the spotlight model and has no bank, so its
 * slot stays NULL and mnPlayers1PTrainingMakeSpotlight reads the model
 * the VS select's way. */
void mnPlayers1PTrainingLoadFiles(void)
{
	static const char *const paths[ARRAY_COUNT(dMNPlayers1PTrainingFileIDs)] =
	{
		MNPLAYERS1PTRAINING_BANK_COMMON,
		MNPLAYERS1PTRAINING_BANK_1PMODE,
		MNPLAYERS1PTRAINING_BANK_MNCOMMON,
		MNPLAYERS1PTRAINING_BANK_EMBLEMS,
		MNPLAYERS1PTRAINING_BANK_SELECT,
		MNPLAYERS1PTRAINING_BANK_GAMEMODES,
		MNPLAYERS1PTRAINING_BANK_PORTRAITS,
		NULL
	};
	s32 i;

	for (i = 0; i < ARRAY_COUNT(dMNPlayers1PTrainingFileIDs); i++)
	{
		sMNPlayers1PTrainingFiles[i] = NULL;

		if (paths[i] == NULL)
		{
			continue;
		}
		if (sprite_bank_load(&sMNPlayers1PTrainingBanks[i], paths[i]) < 0)
		{
			syDebugPrintf("mnPlayers1PTraining: no bank for file %d (%s)\n",
			              (int)dMNPlayers1PTrainingFileIDs[i], paths[i]);
			continue;
		}
		sMNPlayers1PTrainingFiles[i] = &sMNPlayers1PTrainingBanks[i];
	}
}

// 0x80137CAC
void mnPlayers1PTrainingFuncStart(void)
{
	s32 unused1[2];
	s32 i, j;

	mnPlayers1PTrainingLoadFiles();
	gcMakeGObjSPAfter(nGCCommonKindPlayerSelect, mnPlayers1PTrainingFuncRun, 15, GOBJ_PRIORITY_DEFAULT);
	efParticleInitAll();
	/* DIVERGES: efManagerInitEffects is the port's
	 * efManagerLoadEffectBank (src/dc/efmanager.h), as in the battle
	 * and on the results screen. */
	efManagerLoadEffectBank();
	mnPlayers1PTrainingInitVars();
	ftManagerAllocFighter(FTDATA_FLAG_SUBMOTION, 4);
	/* the port's: the SubMotion statuses alone, as the flag says --
	 * tier 0 of each fighter's .anm (ftcommon.h ftManagerSetAnmTier) */
	ftManagerSetAnmTier(FIGHTER_ANM_TIER_MENU);

	for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
	{
		ftManagerSetupFilesAllKind(i);
	}
	for (i = 0; i < ARRAY_COUNT(sMNPlayers1PTrainingSlots); i++)
	{
		sMNPlayers1PTrainingSlots[i].figatree_heap = NULL;
	}
	mnPlayers1PTrainingMakePortraitCamera();
	mnPlayers1PTrainingMakeCursorCamera();
	mnPlayers1PTrainingMakePuckCamera();
	mnPlayers1PTrainingMakePlayerKindCamera();
	mnPlayers1PTrainingMakeGateCamera();
	mnPlayers1PTrainingMakePlayerKindSelectCamera();
	mnPlayers1PTrainingMakeFighterCamera();
	mnPlayers1PTrainingMakeTeamSelectCamera();
	mnPlayers1PTrainingMakeHandicapLevelCamera();
	mnPlayers1PTrainingMakePortraitWallpaperCamera();
	mnPlayers1PTrainingMakePortraitFlashCamera();
	mnPlayers1PTrainingMakeReadyCamera();
	mnPlayers1PTrainingMakeWallpaper();
	mnPlayers1PTrainingMakePortraitAll();
	mnPlayers1PTrainingInitSlotAll();
	mnPlayers1PTrainingMakeLabels();
	mnPlayers1PTrainingMakePuckAdjust();
	mnPlayers1PTrainingMakePuckGlow();
	mnPlayers1PTrainingMakeCostumeSync();
	mnPlayers1PTrainingMakeSpotlight();
	mnPlayers1PTrainingMakeReady();
	scSubsysFighterSetLightParams(45.0F, 45.0F, 0xFF, 0xFF, 0xFF, 0xFF);

	if (gSCManagerSceneData.scene_prev != nSCKindMaps)
	{
		syAudioPlayBGM(0, nSYAudioBGMBattleSelect);
	}
	func_800266A0_272A0();
	func_800269C0_275C0(nSYAudioVoiceAnnounceTrainingMode);
}

// 0x80138448
SYTaskmanSetup dMNPlayers1PTrainingTaskmanSetup =
{
    // Task Manager Buffer Setup
    {
        0,                          	// ???
        gcRunAll,              			// Update function
        scManagerFuncDraw,        		// Frame draw function
        NULL,                       	// Allocatable memory pool start
        0,                          	// Allocatable memory pool size
        1,                          	// ???
        2,                          	// Number of contexts?
        0, 0, 0, 0,                 	// the four DL buffer sizes
        0,                          	// Graphics Heap Size
        2,                          	// ???
        0,                          	// RDP Output Buffer Size
        NULL,                       	// Pre-render function
        syControllerFuncRead,       	// Controller I/O function
    },

    0,                              	// Number of GObjThreads
    sizeof(u64) * 64,              		// Thread stack size
    0,                              	// Number of thread stacks
    0,                              	// ???
    0,                              	// Number of GObjProcesses
    0,                              	// Number of GObjs
    sizeof(GObj),                   	// GObj size
    0,                              	// Number of XObjs
    NULL,                           	// Matrix function list
    NULL,                           	// DObjVec eject function
    0,                              	// Number of AObjs
    0,                              	// Number of MObjs
    0,                              	// Number of DObjs
    sizeof(DObj),                   	// DObj size
    0,                              	// Number of SObjs
    sizeof(SObj),                   	// SObj size
    0,                              	// Number of CObjs
    sizeof(CObj),                 		// CObj size
    
    mnPlayers1PTrainingFuncStart      	// Task start function
};

// 0x80137F00
void mnPlayers1PTrainingStartScene(void)
{
	scManagerFuncUpdate(&dMNPlayers1PTrainingTaskmanSetup);
}

/* The bzero arm of syDmaLoadOverlay for overlay 28, this file:
 * sc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnPlayers1PTrainingOverlayLoad(void)
{
	fighter_release(&sMNPlayers1PTrainingSpotlightPack);

	OVERLAY_CLEAR(sMNPlayers1PTrainingPad0x80138550);
	OVERLAY_CLEAR(sMNPlayers1PTrainingSlots);
	OVERLAY_CLEAR(sMNPlayers1PTrainingPad0x80138838);
	OVERLAY_CLEAR(sMNPlayers1PTrainingStartProceedWait);
	OVERLAY_CLEAR(sMNPlayers1PTrainingIsStart);
	OVERLAY_CLEAR(sMNPlayers1PTrainingDefaultCostumeOnly);
	OVERLAY_CLEAR(sMNPlayers1PTrainingPad0x8013886C);
	OVERLAY_CLEAR(sMNPlayers1PTrainingGameModeGObj);
	OVERLAY_CLEAR(sMNPlayers1PTrainingPuckGlowColor);
	OVERLAY_CLEAR(sMNPlayers1PTrainingIsPuckGlowIncreasing);
	OVERLAY_CLEAR(sMNPlayers1PTrainingFighterMask);
	OVERLAY_CLEAR(sMNPlayers1PTrainingPad0x80138880);
	OVERLAY_CLEAR(sMNPlayers1PTrainingReadyBlinkWait);
	OVERLAY_CLEAR(sMNPlayers1PTrainingPad0x80138888);
	OVERLAY_CLEAR(sMNPlayers1PTrainingTotalTimeTics);
	OVERLAY_CLEAR(sMNPlayers1PTrainingReturnTic);
	OVERLAY_CLEAR(sMNPlayers1PTrainingManPlayer);
	OVERLAY_CLEAR(sMNPlayers1PTrainingComPlayer);
	OVERLAY_CLEAR(sMNPlayers1PTrainingFiles);
	OVERLAY_CLEAR(sMNPlayers1PTrainingBanks);
	OVERLAY_CLEAR(sMNPlayers1PTrainingSpotlightPack);
	OVERLAY_CLEAR(sMNPlayers1PTrainingSpotlightModels);
}
