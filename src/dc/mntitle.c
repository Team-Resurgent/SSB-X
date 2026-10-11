/* mntitle.c -- see mntitle.h. Every function is mn/mncommon/mntitle.c's
 * by name and body, REGION_US arms; the line numbers are the decomp's. */
#include "mntitle.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"
#include "bgm.h"
#include "vmusave.h"            /* sy_sram_next_write_is_load */
#include "fighter.h"
#include "objmodel.h"
#include "objpvr.h"              /* gcGetDrawList, PVR_LIST_TR_POLY */
#include "lbpartex.h"            /* lbpTexLoadBank */

#include <sys/debug.h>
#include <sys/controller.h>
#include <sys/video.h>
#include <sys/rdp.h>
#include <sys/utils.h>
#include <sys/objanim.h>
#include <sc/scdef.h>
#include <gm/gmsound.h>
#include <PR/os.h>
#include <lb/lbbackup.h>
#include <lb/lbparticle.h>
#include <ef/efparticle.h>
#include <macros.h>              /* U8_MAX */

/* n_env.c's start-a-voice-script and its stop-them-all, which no decomp
 * header declares; src/dc/sysshim.c defines both over the FGM engine */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);
void func_800266A0_272A0(void);

/* mn/mncommon/mntitle.h:9-12: the logo fire's particle bank, the four
 * segment labels src/game/ssb64/particlebanks.ld places (the same
 * addresses the decomp's header comments). */
extern uintptr_t lMNTitleParticleScriptBankLo;  // 0x00B22C30
extern uintptr_t lMNTitleParticleScriptBankHi;  // 0x00B22D40
extern uintptr_t lMNTitleParticleTextureBankLo; // 0x00B22D40
extern uintptr_t lMNTitleParticleTextureBankHi; // 0x00B277B0

/* The bank the game's file 167 became (tools/export/ssb_spriteexport.py). The
 * game names its files by id through lbRelocLoadFilesListed; where the
 * disc keeps a scene's banks is the disc layout's business; the romdisk
 * is flat. The host test reads the same file from the
 * game's romdisk directory. */
#ifdef FT_HOSTTEST
#define MNTITLE_SPRITE_BANK "romdisk/mntitle.spr"
#define MNTITLE_FIRE_BANK "romdisk/mntitlefire.spr"
#else
#define MNTITLE_SPRITE_BANK "mntitle.spr"
#define MNTITLE_FIRE_BANK "mntitlefire.spr"
#endif

/* File 167's four bare animation trees, the labels', PRESS START's, the
 * animated logo's and the logo fire's (tools/export/ssb_transexport.py TREES):
 * a DObjDesc and its AnimJoint table each, as a pack. The slash is a
 * real model, tools/export/ssb_effectexport.py's titleslash row. */
#define MNTITLE_LABELS_TREE "mntitlelabels.tra"
#define MNTITLE_PRESS_START_TREE "mntitlepress.tra"
#define MNTITLE_LOGO_TREE "mntitlelogo.tra"
#define MNTITLE_FIRE_TREE "mntitlefiretree.tra"
#define MNTITLE_SLASH_MODEL "mntitleslash.mdl"

/* The particle bank the game's lMNTitleParticle* segment became
 * (tools/export/ssb_particleexport.py, PARTICLE_NAMES in the Makefile). */
#define MNTITLE_PARTICLE_BANK "mntitle"

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mntitle.c:20-26 (0x801341C0): the four sprites of the logo after the
 * opening movie -- the three black cutouts that ride the animated tree
 * and the full logo that fades over them -- as file 167's own offsets
 * (&llMNTitleLogoAnimCutoutSprite, StrikeV, StrikeH, Full; tools/
 * ssb_spriteexport.py --file 167 --list). */
intptr_t dMNTitleLogoAnimSprites[4] = { 0x08FC8, 0x097E8, 0x09B48, 0x0BBB0 };

/* mntitle.c:28-60 (0x801341F0): the thirty flame frames of file 168,
 * &llMNTitleFireAnimFrame1Sprite onward, written out as the file's own
 * offsets (tools/export/ssb_spriteexport.py --file 168 --list). */
#define MNTITLE_FIRE(n) (0x01018 + (n) * 0x1060)
intptr_t dMNTitleFireSpriteOffsets[30] =
{
	MNTITLE_FIRE(0),  MNTITLE_FIRE(1),  MNTITLE_FIRE(2),  MNTITLE_FIRE(3),  MNTITLE_FIRE(4),
	MNTITLE_FIRE(5),  MNTITLE_FIRE(6),  MNTITLE_FIRE(7),  MNTITLE_FIRE(8),  MNTITLE_FIRE(9),
	MNTITLE_FIRE(10), MNTITLE_FIRE(11), MNTITLE_FIRE(12), MNTITLE_FIRE(13), MNTITLE_FIRE(14),
	MNTITLE_FIRE(15), MNTITLE_FIRE(16), MNTITLE_FIRE(17), MNTITLE_FIRE(18), MNTITLE_FIRE(19),
	MNTITLE_FIRE(20), MNTITLE_FIRE(21), MNTITLE_FIRE(22), MNTITLE_FIRE(23), MNTITLE_FIRE(24),
	MNTITLE_FIRE(25), MNTITLE_FIRE(26), MNTITLE_FIRE(27), MNTITLE_FIRE(28), MNTITLE_FIRE(29)
};
#undef MNTITLE_FIRE

/* mntitle.c:64-93 (0x80134268), REGION_US. The offsets are the values
 * the link labels take in relocData file 167, from the decomp's
 * tools/relocFileDescriptions.us.txt -- the same numbers the game's
 * table holds. */
MNTitleSpriteDesc dMNTitleCommonSpriteDescs[/* */] =
{
	{ { 157,  94 }, 0x11988 },       // &llMNTitleCutoutSprite
	{ { 161,  88 }, 0x245C8 },       // &llMNTitleSmashSprite
	{ {  55,  96 }, 0x16728 },       // &llMNTitleSuperSprite
	{ { 268,  96 }, 0x25188 },       // &llMNTitleBrosSprite
	{ { 270, 132 }, 0x11AA8 },       // &llMNTitleTMUnkSprite
	{ { 160, 208 }, 0x15320 },       // &llMNTitleCopyrightSprite
	{ { 160,  15 }, 0x0C208 },       // &llMNTitleBorderUpperSprite
	{ { 162, 177 }, 0x15A48 },       // &llMNTitlePressStartSprite
	{ { 260,  60 }, 0x0BBB0 },       // &llMNTitleLogoAnimFullSprite
	{ { 277, 157 }, 0x0F398 }        // &llMNTitleTMSprite
};

// 0x80134318
u8 dMNTitleFireColorsR[/* */] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xE6, 0xFF, 0xFF };

// 0x80134320
u8 dMNTitleFireColorsG[/* */] = { 0xFF, 0xF0, 0xFF, 0xD1, 0xFF, 0xE2, 0xD2 };

// 0x80134328
u8 dMNTitleFireColorsB[/* */] = { 0xFF, 0x9B, 0x64, 0xD1, 0xE6, 0xB8, 0x94 };

/* mntitle.c:139-179 (0x8013438C). The pool counts are the game's, all
 * zero: a menu scene takes its objects straight from the scene heap
 * (objman.c gcGetGObjSetNextAlloc's fallback). DIVERGES: the arena is
 * the port's region (NULL here, see src/dc/taskman.c), the draw is the
 * scene manager's (scManagerFuncDraw is gcDrawAll), and
 * mnTitleFuncLights -- a gSPDisplayList of two lighting commands for
 * the 3D logo -- is dropped with the 3D logo. */
SYTaskmanSetup dMNTitleTaskmanSetup =
{
    // Task Manager Buffer Setup
    {
        0,                          // ???
        mnTitleFuncUpdate,          // Update function
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
    0,                              // Number of GObjProcesses
    0,                              // Number of GObjs
    sizeof(GObj),                   // GObj size
    0,                              // Number of XObjs
    NULL,                           // Matrix function list
    NULL,                           // DObjVec eject function
    0,                              // Number of AObjs
    0,                              // Number of MObjs
    0,                              // Number of DObjs
    sizeof(DObj),                   // DObj size
    0,                              // Number of SObjs
    sizeof(SObj),                   // SObj size
    0,                              // Number of CObjs
    sizeof(CObj),                   // CObj size

    mnTitleFuncStart                // Task start function
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* mntitlefiles.c:11, 26: the file ids and the loaded files, the title
 * file (167) and the fire animation (168), each a sprite bank here. */
static SpriteBank sMNTitleBank;
static SpriteBank sMNTitleFireBank;
void *sMNTitleFiles[2];

/* File 167's DObjDesc and AnimJoint data, which the game reads out of the
 * same file as the sprites and the port out of two packs. Loaded as the
 * scene starts and given back by mnTitleOverlayLoad on the next entry, as
 * src/dc/ftshadow.c gives back its model. */
static Fighter sMNTitleLabelsTree;
static sb32 sMNTitleLabelsTreeIsLoaded;
static Fighter sMNTitlePressStartTree;
static sb32 sMNTitlePressStartTreeIsLoaded;
static Fighter sMNTitleLogoTree;
static sb32 sMNTitleLogoTreeIsLoaded;
static Fighter sMNTitleFireTree;
static sb32 sMNTitleFireTreeIsLoaded;

/* The slash's pack and the one instance the scene draws of it, as
 * src/dc/mvopeningstandoff.c keeps its lightning. Target only: the host
 * links no renderer for a model that carries geometry. */
#ifndef FT_HOSTTEST
static Fighter sMNTitleSlashPack;
static Fighter sMNTitleSlashModel;
static sb32 sMNTitleSlashIsLoaded;
#endif

static void mnTitleSetupAnimTree(GObj *gobj, Fighter *tree, sb32 *is_loaded, const char *name);

// 0x80134444
static s32 sMNTitleParticleBankID;

// 0x80134448
static GObj *sMNTitleFireCameraGObj;

// 0x80134450
static s32 sMNTitleLayout;

// 0x80134454
static GObj *sMNTitleTransitionsGObj;

// 0x80134458
static GObj *sMNTitleMainGObj;

// 0x8013445C
static s32 sMNTitleTransitionTotalTimeTics;

// 0x80134460
static sb32 sMNTitleIsStartActorProcess;

// 0x80134464
static s32 sMNTitleFireAlpha;

// 0x80134468
static s32 sMNTitleFireAlphaUnused;

// 0x8013446C
static s32 sMNTitleLogoAlpha;

// 0x80134470
static sb32 sMNTitleIsProceedScene;

// 0x80134474
static s32 sMNTitleProceedSceneWait;

// 0x80134478
static s32 sMNTitleFireTimer;

// 0x8013447C
static f32 sMNTitleFireColorR;

// 0x80134480
static f32 sMNTitleFireColorG;

// 0x80134484
static f32 sMNTitleFireColorB;

// 0x80134488
static f32 sMNTitleFireColorDeltaR;

// 0x8013448C
static f32 sMNTitleFireColorDeltaG;

// 0x80134490
static f32 sMNTitleFireColorDeltaB;

// 0x80134494
static s32 sMNTitleFireColorID;

// 0x80134498
static u32 sMNTitleAllowProceedWait;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mntitle.c:260-272 0x80131B00, verbatim: how many bits of a fighter
 * mask are set. */
s32 mnTitleGetFighterKindsNum(u16 mask)
{
	s32 i, j;

	for (i = 0, j = 0; i < sizeof(u16) * 8; i++, mask = mask >> 1)
	{
		if (mask & 1)
		{
			j++;
		}
	}
	return j;
}

/* mntitle.c:275-293 0x80131B78, verbatim: the random-th fighter that is
 * in this_mask and not in prev_mask. */
s32 mnTitleGetShuffledFighterKind(u16 this_mask, u16 prev_mask, s32 random)
{
	s32 fkind = -1;

	random++;

	do
	{
		fkind++;

		if ((this_mask & LBBACKUP_MASK_FIGHTER(fkind)) && !(prev_mask & LBBACKUP_MASK_FIGHTER(fkind)))
		{
			random--;
		}
	}
	while (random != 0);

	return fkind;
}

/* mntitle.c:296-337 0x80131BC4, verbatim: the two fighters the next
 * auto-demo stars, drawn without replacement from the unlocked set until
 * every one has had a turn, then the set starts over. The decomp declares
 * it s32 and returns nothing; nothing reads a value, so it is void here. */
void mnTitleSetDemoFighterKinds(void)
{
	u16 unlocked_mask;
	s32 unlocked_count;
	s32 non_recently_demoed_count;

	unlocked_mask = gSCManagerBackupData.fighter_mask | LBBACKUP_CHARACTER_MASK_STARTER;

	if (~unlocked_mask & gSCManagerSceneData.demo_mask_prev)
	{
		gSCManagerSceneData.demo_mask_prev = 0;
	}
	unlocked_count = mnTitleGetFighterKindsNum(unlocked_mask);

	if (unlocked_count <= mnTitleGetFighterKindsNum(gSCManagerSceneData.demo_mask_prev))
	{
		gSCManagerSceneData.demo_mask_prev = 0;
	}
	unlocked_count = mnTitleGetFighterKindsNum(unlocked_mask);

	gSCManagerSceneData.demo_fkind[0] = mnTitleGetShuffledFighterKind(unlocked_mask, gSCManagerSceneData.demo_mask_prev, syUtilsRandIntRange(unlocked_count - mnTitleGetFighterKindsNum(gSCManagerSceneData.demo_mask_prev)));

	if (!(gSCManagerSceneData.demo_mask_prev))
	{
		gSCManagerSceneData.demo_first_fkind = gSCManagerSceneData.demo_fkind[0];
	}
	gSCManagerSceneData.demo_mask_prev |= LBBACKUP_MASK_FIGHTER(gSCManagerSceneData.demo_fkind[0]);

	unlocked_count = mnTitleGetFighterKindsNum(unlocked_mask);

	non_recently_demoed_count = unlocked_count - mnTitleGetFighterKindsNum(gSCManagerSceneData.demo_mask_prev);

	if (non_recently_demoed_count == 0)
	{
		gSCManagerSceneData.demo_fkind[1] = gSCManagerSceneData.demo_first_fkind;
	}
	else
	{
		gSCManagerSceneData.demo_fkind[1] = mnTitleGetShuffledFighterKind(unlocked_mask, gSCManagerSceneData.demo_mask_prev, syUtilsRandIntRange(non_recently_demoed_count));
		gSCManagerSceneData.demo_mask_prev |= LBBACKUP_MASK_FIGHTER(gSCManagerSceneData.demo_fkind[1]);
	}
}

/* mntitle.c:340-372 0x80131CF0, verbatim (XBOX: now including where the
 * stops are), both arms:
 * after the opening movie the layout starts at Opening with the clock at
 * zero and the fire colour black, and the flames, the labels and PRESS
 * START come in on the same clock as the logo animation. */
void mnTitleInitVars(void)
{
	s32 color_id;

	/* XBOX: the two stops back inside the else arm, where the decomp has
	 * them (mntitle.c:352-353). Straight after the movie the title does
	 * not stop anything: the opening's music plays on into the title's
	 * fanfare and the announcer's nSYAudioVoiceAnnounceTitleWait, started
	 * by mvOpeningNewcomersFuncStart, says "Super Smash Bros." over the
	 * logo. Stopping here cut both. src/dc/sndres.c keeps their samples
	 * resident and playing across the swap into the title. */
	if (gSCManagerSceneData.scene_prev == nSCKindOpeningNewcomers)
	{
		sMNTitleLayout = nMNTitleLayoutOpening;
		sMNTitleTransitionTotalTimeTics = 0;
		sMNTitleFireColorR = sMNTitleFireColorG = sMNTitleFireColorB = 0.0F;
	}
	else
	{
		syAudioStopBGMAll();
		func_800266A0_272A0();

		sMNTitleLayout = nMNTitleLayoutAnimate;
		sMNTitleTransitionTotalTimeTics = 169;

		color_id = syUtilsRandTimeUCharRange(7);
		sMNTitleFireColorID = color_id;
		sMNTitleFireColorR = dMNTitleFireColorsR[color_id];
		sMNTitleFireColorG = dMNTitleFireColorsG[color_id];
		sMNTitleFireColorB = dMNTitleFireColorsB[color_id];
	}
	sMNTitleFireTimer = 0;
	sMNTitleIsProceedScene = FALSE;
	sMNTitleProceedSceneWait = 3;
	sMNTitleIsStartActorProcess = FALSE;
	sMNTitleFireColorDeltaR = 0.0F;
	sMNTitleFireColorDeltaG = 0.0F;
	sMNTitleFireColorDeltaB = 0.0F;
}

/* mntitle.c:374-395 0x80131E68, verbatim. */
void mnTitleSetEndLogoPosition(void)
{
	GObj *smash_logo_gobj;
	SObj *smash_logo_sobj;

	smash_logo_gobj = gGCCommonLinks[10];

	if (gSCManagerSceneData.scene_prev == nSCKindOpeningNewcomers)
	{
		gcEndProcessAll(smash_logo_gobj);
	}
	smash_logo_sobj = SObjGetStruct(smash_logo_gobj);

	mnTitleSetPosition(NULL, smash_logo_sobj, nMNTitleSpriteKindLogo);

	smash_logo_sobj->user_data.s = 0xFF;
	sMNTitleLogoAlpha = 0x4C;

	smash_logo_sobj->sprite.scalex = 1.0F;
	smash_logo_sobj->sprite.scaley = 1.0F;
}

/* mntitle.c:397-450 0x80131EE4, AVOID_UB arms -- chosen by hand when
 * this was written, and also what the build says: -DAVOID_UB is in
 * DECOMP_DEFS, so the hand-choice and the define agree. */
void mnTitleSetEndLayout(void)
{
	s32 i;
	GObj *texture_gobj;
	GObj *gobj;
	SObj *sobj;

	gobj = gGCCommonLinks[6];

	texture_gobj = NULL;

	while (gobj != NULL)
	{
		if (gobj->id == 5)
		{
			mnTitleShowFire(gobj);
		}
		gobj = gobj->link_next;
	}
	gobj = gGCCommonLinks[8];

	while (gobj != NULL)
	{
		if (gobj->id == 8)
		{
			texture_gobj = gobj;
		}
		gobj = gobj->link_next;
	}

	if (texture_gobj != NULL)
	{
		gcEndProcessAll(texture_gobj);

		sobj = SObjGetStruct(texture_gobj);
		texture_gobj->flags = GOBJ_FLAG_NONE;

		for (i = 0; sobj != NULL; i++, sobj = sobj->next)
		{
			mnTitleSetPosition(NULL, sobj, i);
			mnTitleSetColors(sobj, i);

			sobj->sprite.scalex = sobj->sprite.scaley = 1.0F;
		}
	}
}

/* mntitle.c:452-487 0x80131F44, REGION_US arms, verbatim: the title's
 * idle demonstration. After 650 tics untouched (1190 when the movie has
 * just run) the title picks the next two demo fighters and leaves for
 * How to Play (src/dc/scexplain.c), which hands to the character
 * showcase (nSCKindCharacters) and on to the auto-demo battle; from
 * mode select or the auto-demo it goes back to the N64 logo
 * (nSCKindStartup) and the opening movie, which ends here again. The
 * JP arm's gcEjectGObj(gGCCommonLinks[13]) is the JP-only logo model,
 * never made in the US build. */
void mnTitleProceedDemoNext(void)
{
	u8 scene_prev = gSCManagerSceneData.scene_prev;

	gcMakeDefaultCameraGObj(2, GOBJ_PRIORITY_DEFAULT, 0, COBJ_FLAG_FILLCOLOR, GPACK_RGBA8888(0x00, 0x00, 0x00, 0xFF));

	mnTitleSetDemoFighterKinds();
	func_800266A0_272A0();

	gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;

	switch (scene_prev)
	{
	case nSCKindExplain:
		gSCManagerSceneData.scene_curr = nSCKindCharacters;
		syAudioPlayBGM(0, nSYAudioBGMExplain);
		break;

	case nSCKindModeSelect:
	case nSCKindAutoDemo:
		gSCManagerSceneData.scene_curr = nSCKindStartup;
		break;

	default:
		gSCManagerSceneData.scene_curr = nSCKindExplain;
		break;
	}
	gSCManagerSceneData.is_extend_demo_wait = TRUE;
	sMNTitleIsProceedScene = TRUE;
}

/* mntitle.c:490-504 0x80132090, verbatim: the black fill camera, drawn
 * last, covers the three frames before the scene changes
 * (src/dc/objdisplay.c gcPrepCameraViewport). The scene is the game's,
 * nSCKindModeSelect. */
void mnTitleProceedModeSelect(void)
{
	gcMakeDefaultCameraGObj(2, GOBJ_PRIORITY_DEFAULT, 0, COBJ_FLAG_FILLCOLOR, GPACK_RGBA8888(0x00, 0x00, 0x00, 0xFF));

	gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
	gSCManagerSceneData.scene_curr = nSCKindModeSelect;

	func_800266A0_272A0();
	func_800269C0_275C0(nSYAudioFGMTitlePressStart);

	sMNTitleIsProceedScene = TRUE;
}

/* mntitle.c:507-554 0x801320F0, verbatim. The else arm is a press during
 * the opening layout -- the logo still animating after the movie -- which
 * skips to the fire layout at once. */
void mnTitleFuncRun(GObj *gobj)
{
	s32 i;
	u16 buttons;

	(void)gobj;

	if (sMNTitleIsStartActorProcess == FALSE)
	{
		sMNTitleIsStartActorProcess++;
	}
	else if (sMNTitleIsProceedScene != FALSE)
	{
		sMNTitleProceedSceneWait--;

		if (sMNTitleProceedSceneWait == 0)
		{
			syTaskmanSetLoadScene();
		}
	}
	else for (i = 0; i < ARRAY_COUNT(gSYControllerDevices); i++)
	{
		buttons = gSYControllerDevices[i].button_tap;

		if (gSYControllerDevices[i].button_tap & (A_BUTTON | B_BUTTON | START_BUTTON))
		{
			if (sMNTitleLayout != 0)
			{
				if ((gSCManagerSceneData.is_title_anim_viewed) || (osResetType != 0))
				{
					if (!(buttons & B_BUTTON))
					{
						mnTitleProceedModeSelect();
						break;
					}
				}
			}
			else
			{
				sMNTitleTransitionTotalTimeTics = 169;
				sMNTitleLayout = nMNTitleLayoutAnimate;

				mnTitleTransitionFromFireLogo();
				syAudioStopBGMAll();
				func_800266A0_272A0();
				break;
			}
		}
	}
}

/* mntitle.c:557-568 0x80132210, verbatim: a new fire colour, eased to
 * from black over 80 tics. */
void mnTitleUpdateFireVars(void)
{
	s32 kind = syUtilsRandTimeUCharRange(7);

	sMNTitleFireColorID = kind;

	sMNTitleFireColorR = sMNTitleFireColorG = sMNTitleFireColorB = 0.0F;

	sMNTitleFireColorDeltaR = (dMNTitleFireColorsR[kind] - sMNTitleFireColorR) / 80.0F;
	sMNTitleFireColorDeltaG = (dMNTitleFireColorsG[kind] - sMNTitleFireColorG) / 80.0F;
	sMNTitleFireColorDeltaB = (dMNTitleFireColorsB[kind] - sMNTitleFireColorB) / 80.0F;
}

/* mntitle.c:571-598 0x80132320, verbatim: the logo animation gives way
 * to the fire layout -- the three cutouts hide, the logo fire's display
 * GObj (link 4) and the slash (link 14) go, the flames show. The two
 * ejects keep the game's own NULL guards; on the host neither is made. */
void mnTitleTransitionFromFireLogo(void)
{
	GObj *current_gobj;
	GObj *next_gobj;

	current_gobj = gGCCommonLinks[7];

	while (current_gobj != NULL)
	{
		next_gobj = current_gobj->link_next;

		if (current_gobj->id == 6)
		{
			current_gobj->flags = GOBJ_FLAG_HIDDEN;
		}
		current_gobj = next_gobj;
	}
	if (gGCCommonLinks[4] != NULL)
	{
		gcEjectGObj(gGCCommonLinks[4]);
	}
	if (gGCCommonLinks[14] != NULL)
	{
		gcEjectGObj(gGCCommonLinks[14]);
	}
	mnTitleShowGObjLinkID(6);
	mnTitleUpdateFireVars();
}

/* mntitle.c:601-611 0x801323AC, verbatim. */
void mnTitleShowGObjLinkID(s32 link_id)
{
	GObj *gobj = gGCCommonLinks[link_id];

	while (gobj != NULL)
	{
		gobj->flags = GOBJ_FLAG_NONE;

		gobj = gobj->link_next;
	}
}

/* mntitle.c:614-621 0x801323DC, verbatim. */
void mnTitleAdvanceLayout(void)
{
	if ((sMNTitleLayout == nMNTitleLayoutOpening) && (gSCManagerSceneData.scene_prev == nSCKindOpeningNewcomers))
	{
		gSCManagerSceneData.is_extend_demo_wait = FALSE;
	}
	sMNTitleLayout++;
}

/* mntitle.c:624-637 0x80132414, REGION_US, verbatim. */
void mnTitleSetAllowProceedWait(void)
{
	if (sMNTitleLayout == nMNTitleLayoutFinal)
	{
		sMNTitleAllowProceedWait = 280;
	}
	else sMNTitleAllowProceedWait = 364;
}

/* mntitle.c:640-726 0x80132448, REGION_US arms, verbatim step
 * 26: the title's clock, what appears when. Case 111 is the logo
 * animation giving way to the fire layout after the opening movie;
 * cases 650 and 1190 are the idle demonstration (mnTitleProceedDemoNext),
 * 650 tics untouched on an ordinary visit and 1190 when the movie has
 * just run (is_extend_demo_wait, which mnTitleAdvanceLayout clears at
 * 170 on that visit and mnTitleProceedDemoNext sets on the way out). */
void mnTitleTransitionsFuncRun(GObj *gobj)
{
	(void)gobj;

	sMNTitleTransitionTotalTimeTics++;

	if (sMNTitleTransitionTotalTimeTics == sMNTitleAllowProceedWait)
	{
		gSCManagerSceneData.is_title_anim_viewed = TRUE;
	}
	switch (sMNTitleTransitionTotalTimeTics)
	{
	case 111:
		mnTitleTransitionFromFireLogo();
		break;

	case 170:
		mnTitleSetEndLogoPosition();
		mnTitleShowGObjLinkID(8);

		mnTitleAdvanceLayout();
		mnTitleSetAllowProceedWait();
		break;

	case 220:
		mnTitleSetEndLayout();
		break;

	case 280:
		mnTitleShowGObjLinkID(9);
		break;

	case 35:
	case 65:
		func_800269C0_275C0(nSYAudioFGMOpeningBatM);
		break;

	case 200:
		if (sMNTitleLayout == nMNTitleLayoutAnimate)
		{
			func_800269C0_275C0(nSYAudioFGMPublicPrologue);
		}
		break;

	case 650:
		if (!(gSCManagerSceneData.is_extend_demo_wait))
		{
			mnTitleProceedDemoNext();
		}
		break;

	case 1190:
		if (gSCManagerSceneData.is_extend_demo_wait)
		{
			mnTitleProceedDemoNext();
		}
		break;
	}
}

/* mntitle.c:729-749 0x801325D4, verbatim: play the animation GObj one
 * tic and put each sprite where its DObj is, at its scale. */
void mnTitlePlayAnim(GObj *gobj)
{
	GObj *effect_gobj = gobj->user_data.p;
	SObj *sobj = SObjGetStruct(gobj);
	DObj *dobj = DObjGetStruct(effect_gobj);

	gcPlayAnimAll(effect_gobj);

	dobj = dobj->child;

	while (sobj != NULL)
	{
		sobj->sprite.scalex = dobj->scale.vec.f.x;
		sobj->sprite.scaley = dobj->scale.vec.f.y;
		sobj->pos.x = ((dobj->translate.vec.f.x + 160.0F) - (sobj->sprite.width * sobj->sprite.scalex * 0.5F));
		sobj->pos.y = ((120.0F - dobj->translate.vec.f.y) - (sobj->sprite.height * sobj->sprite.scaley * 0.5F));

		sobj = sobj->next;
		dobj = dobj->sib_next;
	}
}

/* mntitle.c:752-758 0x801326A4, verbatim. */
void mnTitlePressStartProcUpdate(GObj *gobj)
{
	if (gobj->flags != GOBJ_FLAG_HIDDEN)
	{
		mnTitlePlayAnim(gobj);
	}
}

/* mntitle.c:762-768 0x801326D4, REGION_US, verbatim. */
void mnTitleProcUpdate(GObj *gobj)
{
	if (gobj->flags != GOBJ_FLAG_HIDDEN)
	{
		mnTitlePlayAnim(gobj);
	}
}

/* mntitle.c:772-781 0x80132704, verbatim. */
void mnTitleUpdateLabelsPosition(GObj *gobj)
{
	SObj *sobj = SObjGetStruct(gobj);

	if ((sMNTitleLayout != nMNTitleLayoutOpening) || (gSCManagerSceneData.scene_prev != nSCKindOpeningNewcomers))
	{
		mnTitleSetPosition(NULL, sobj, nMNTitleSpriteKindFooter);
		mnTitleSetPosition(NULL, sobj->next, nMNTitleSpriteKindHeader);
	}
}

/* mntitle.c:784-797 0x80132764, verbatim. */
void mnTitleSetPosition(DObj *dobj, SObj *sobj, s32 kind)
{
	MNTitleSpriteDesc *desc;

	if (dobj != NULL)
	{
		desc = &dMNTitleCommonSpriteDescs[kind];
		dobj->translate.vec.f.x = desc->pos.x - 160.0F;
		dobj->translate.vec.f.y = -(desc->pos.y - 120.0F);
	}
	desc = &dMNTitleCommonSpriteDescs[kind];
	sobj->pos.x = (desc->pos.x - (sobj->sprite.width * 0.5F));
	sobj->pos.y = (desc->pos.y - (sobj->sprite.height * 0.5F));
}

/* mntitle.c:800-861 0x8013282C, REGION_US arms, verbatim. */
void mnTitleSetColors(SObj *sobj, s32 kind)
{
	if (kind < nMNTitleSpriteKindFooter)
	{
		if ((kind == nMNTitleSpriteKindDropShadow) || (kind == nMNTitleSpriteKindTM))
		{
			sobj->sprite.red = 0x00;
			sobj->sprite.green = 0x00;
			sobj->sprite.blue = 0x00;
		}
		else
		{
			sobj->sprite.red = 0xFF;
			sobj->sprite.green = 0xFE;
			sobj->sprite.blue = 0x2A;
			sobj->envcolor.r = 0x00;
			sobj->envcolor.g = 0x00;
			sobj->envcolor.b = 0x00;
		}
	}
	else switch (kind)
	{
	case nMNTitleSpriteKindFooter:
		sobj->sprite.red = 0xB7;
		sobj->sprite.green = 0xAE;
		sobj->sprite.blue = 0x7C;
		sobj->envcolor.r = 0x14;
		sobj->envcolor.g = 0x12;
		sobj->envcolor.b = 0x06;
		break;

	case nMNTitleSpriteKindHeader:
		sobj->sprite.red = 0x14;
		sobj->sprite.green = 0x12;
		sobj->sprite.blue = 0x06;
		break;

	case nMNTitleSpriteKindPressStart:
		sobj->sprite.red = 0xFF;
		sobj->sprite.green = 0xFF;
		sobj->sprite.blue = 0xFF;
		sobj->envcolor.r = 0x17;
		sobj->envcolor.g = 0x10;
		sobj->envcolor.b = 0xA4;
		break;

	case nMNTitleSpriteKindTM2:
		sobj->sprite.red = 0x15;
		sobj->sprite.green = 0x13;
		sobj->sprite.blue = 0x06;
		break;
	}
}

/* mntitle.c:864-881 0x80132940. The two flames, each drawn in the texel's
 * own colour with its alpha scaled by sMNTitleFireAlpha; the prim colour
 * and the combiner are the port's two lbCommonSprite* calls, as in
 * mnTitleLogoProcDisplay below. */
void mnTitleFireProcDisplay(GObj *fire_gobj)
{
	s32 i;
	SObj *fire_sobj = SObjGetStruct(fire_gobj);

	for (i = 0; i < 2; i++)
	{
		lbCommonPrepSObjAttr(NULL, fire_sobj);

		lbCommonSpriteSetPrimColor(0x00, 0x00, 0x00, sMNTitleFireAlpha);
		lbCommonSpriteSetCombine(nLBCommonCombineTexPrimAlpha);

		lbCommonPrepSObjDraw(NULL, fire_sobj);
		lbCommonClearExternSpriteParams();

		fire_sobj = fire_sobj->next;
	}
}

/* mntitle.c:884-895 0x80132A20, verbatim. */
void mnTitleFireFuncRun(GObj *gobj)
{
	if (gobj->flags != GOBJ_FLAG_HIDDEN)
	{
		sMNTitleFireAlpha += 0x0D;

		if (sMNTitleFireAlpha > 0xFF)
		{
			sMNTitleFireAlpha = 0xFF;
		}
	}
}

/* mntitle.c:898-903 0x80132A58, verbatim. */
void mnTitleShowFire(GObj *gobj)
{
	sMNTitleFireAlpha = 0xFF;

	gobj->flags = GOBJ_FLAG_NONE;
}

/* mntitle.c:906-922 0x80132A6C, verbatim but for the sprite lookup. */
void mnTitleUpdateFireSprite(SObj *sobj, sb32 is_next)
{
	Sprite *sprite = sprite_bank_get(sMNTitleFiles[1], dMNTitleFireSpriteOffsets[sobj->user_data.s]);

	sobj->sprite = *sprite;
	sobj->sprite.attr = SP_TRANSPARENT;

	sobj->sprite.scalex = (is_next != FALSE) ? 9.5F : 12.0F;
	sobj->sprite.scaley = (is_next != FALSE) ? 7.0F :  8.5F;

	sobj->user_data.s++;

	if (sobj->user_data.s >= 30)
	{
		sobj->user_data.s = 0;
	}
}

/* mntitle.c:925-931 0x80132B38, verbatim. */
void mnTitleFireProcUpdate(GObj *gobj)
{
	SObj *base_sobj = SObjGetStruct(gobj), *next_sobj = base_sobj->next;

	mnTitleUpdateFireSprite(base_sobj, FALSE);
	mnTitleUpdateFireSprite(next_sobj, TRUE);
}

/* mntitle.c:934-994 0x80132B70, verbatim but for the sprite lookup: two
 * flames, one frame apart, stretched over the screen behind the logo. */
void mnTitleMakeFire(void)
{
	s32 i;
	s32 target_texture;
	GObj *fire_gobj;
	SObj *fire_sobj;

	fire_gobj = gcMakeGObjSPAfter(5, mnTitleFireFuncRun, 6, GOBJ_PRIORITY_DEFAULT);

	if (fire_gobj != NULL)
	{
		gcAddGObjDisplay(fire_gobj, mnTitleFireProcDisplay, 0, GOBJ_PRIORITY_DEFAULT, ~0);
		gcAddGObjProcess(fire_gobj, mnTitleFireProcUpdate, nGCProcessKindFunc, 1);

		for (i = 0; i < 2; i++)
		{
			if (i != 0)
			{
				target_texture = 0;
			}
			else target_texture = 12;

			fire_sobj = lbCommonMakeSObjForGObj(fire_gobj, sprite_bank_get(sMNTitleFiles[1], dMNTitleFireSpriteOffsets[target_texture]));

			fire_sobj->sprite.attr = SP_TRANSPARENT;

			if (i != 0)
			{
				fire_sobj->pos.x = 8.0F;
			}
			else fire_sobj->pos.x = -32.0F;

			if (i != 0)
			{
				fire_sobj->pos.y = 8.0F;
			}
			else fire_sobj->pos.y = -16.0F;

			if (i != 0)
			{
				fire_sobj->sprite.scalex = 9.5F;
			}
			else fire_sobj->sprite.scalex = 12.0F;

			if (i != 0)
			{
				fire_sobj->sprite.scaley = 7.0F;
			}
			else fire_sobj->sprite.scaley = 8.5F;

			fire_sobj->user_data.s = target_texture;
		}
		sMNTitleFireAlpha = sMNTitleFireAlphaUnused = 0;

		fire_gobj->flags = GOBJ_FLAG_HIDDEN;

		if (gSCManagerSceneData.scene_prev != nSCKindOpeningNewcomers)
		{
			mnTitleShowFire(fire_gobj);
		}
	}
}

/* mntitle.c:998-1008 0x80132D5C, verbatim: the full logo follows the
 * animated tree's fourth child, at its scale. */
void mnTitleLogoProcUpdate(GObj *gobj)
{
	GObj *effect_gobj = gobj->user_data.p;
	SObj *logo_sobj = SObjGetStruct(gobj);
	DObj *logo_dobj = DObjGetStruct(effect_gobj)->child->sib_next->sib_next->sib_next;

	logo_sobj->sprite.scalex = logo_dobj->scale.vec.f.x;
	logo_sobj->sprite.scaley = logo_dobj->scale.vec.f.y;
	logo_sobj->pos.x = ((logo_dobj->translate.vec.f.x + 160.0F) - (logo_sobj->sprite.width * logo_sobj->sprite.scalex * 0.5F));
	logo_sobj->pos.y = ((120.0F - logo_dobj->translate.vec.f.y) - (logo_sobj->sprite.height * logo_sobj->sprite.scaley * 0.5F));
}

/* mntitle.c:1011-1027 0x80132DFC. The red silhouette behind the logo,
 * drawn at the alpha the title's clock sets (0x4C once the labels are
 * in). Its two GBI lines -- the prim colour with sMNTitleLogoAlpha for
 * alpha, and a combiner that multiplies the texel's alpha by it -- are
 * the port's two lbCommonSprite* calls (lbcommon.h). */
void mnTitleLogoProcDisplay(GObj *gobj)
{
	SObj *sobj = SObjGetStruct(gobj);

	if ((sobj->sprite.scalex < 0.0001F) || (sobj->sprite.scaley < 0.0001F))
	{
		return;
	}
	else
	{
		lbCommonPrepSObjAttr(NULL, sobj);

		lbCommonSpriteSetPrimColor(sobj->sprite.red, sobj->sprite.green, sobj->sprite.blue, sMNTitleLogoAlpha);
		lbCommonSpriteSetCombine(nLBCommonCombineIPrimAlpha);

		lbCommonDrawSObjNoAttr(gobj);
	}
}

/* mntitle.c:1031-1048 0x80132EDC, verbatim: the full logo fades from
 * opaque to the silhouette's 0x4C, four a tic, once it has a size. */
void mnTitleFadeOutLogoFuncRun(GObj *gobj)
{
	SObj *sobj = SObjGetStruct(gobj);

	if ((sobj->sprite.scalex < 0.0001F) || (sobj->sprite.scaley < 0.0001F))
	{
		return;
	}
	else
	{
		sMNTitleLogoAlpha -= 0x04;

		if (sMNTitleLogoAlpha <= 0x4C)
		{
			sMNTitleLogoAlpha = 0x4C;
		}
	}
}

/* mntitle.c:1051-1075 0x80132F3C, verbatim but for the sprite lookup. */
void mnTitleMakeLogoNoOpening(void)
{
	GObj *gobj = lbCommonMakeSpriteGObj
	(
		11,
		NULL,
		10,
		GOBJ_PRIORITY_DEFAULT,
		mnTitleLogoProcDisplay,
		0,
		GOBJ_PRIORITY_DEFAULT,
		-1,
		sprite_bank_get(sMNTitleFiles[0], dMNTitleCommonSpriteDescs[nMNTitleSpriteKindLogo].offset),
		nGCProcessKindFunc,
		NULL,
		1
	);
	SObj *sobj = SObjGetStruct(gobj);

	sobj->sprite.attr = SP_TRANSPARENT;

	sobj->sprite.red = 0xFF;
	sobj->sprite.green = 0x00;
	sobj->sprite.blue = 0x00;

	mnTitleSetPosition(NULL, sobj, nMNTitleSpriteKindLogo);
}

/* mntitle.c:1078-1159 0x80132FD8, verbatim but for the tree
 * (mnTitleSetupAnimTree over MNTITLE_LOGO_TREE, in gcSetupCommonDObjs +
 * gcAddAnimJointAll's stead) and the sprite lookups. The else arm is the
 * logo after the opening movie: an AnimJoint-driven tree of four joints
 * (id 7, link 7), three black cutouts riding the first three
 * (mnTitlePlayAnim, id 6, link 7) and the full logo on the fourth
 * (mnTitleLogoProcUpdate, link 10), fading from opaque to the
 * silhouette's alpha as it lands. */
void mnTitleMakeLogo(void)
{
	s32 i;
	GObj *animated_logo_gobj;
	GObj *fire_logo_gobj;
	GObj *logo_gobj;
	SObj *fire_logo_sobj;
	SObj *logo_sobj;
	DObj *fire_logo_dobj;

	if (gSCManagerSceneData.scene_prev != nSCKindOpeningNewcomers)
	{
		mnTitleMakeLogoNoOpening();
	}
	else
	{
		animated_logo_gobj = gcMakeGObjSPAfter(7, NULL, 7, GOBJ_PRIORITY_DEFAULT);
		mnTitleSetupAnimTree(animated_logo_gobj, &sMNTitleLogoTree, &sMNTitleLogoTreeIsLoaded, MNTITLE_LOGO_TREE);
		gcPlayAnimAll(animated_logo_gobj);

		fire_logo_gobj = gcMakeGObjSPAfter(6, NULL, 7, GOBJ_PRIORITY_DEFAULT);
		gcAddGObjDisplay(fire_logo_gobj, lbCommonDrawSObjAttr, 0, GOBJ_PRIORITY_DEFAULT, ~0);
		gcAddGObjProcess(fire_logo_gobj, mnTitlePlayAnim, nGCProcessKindFunc, 1);

		fire_logo_gobj->user_data.p = animated_logo_gobj;
		fire_logo_dobj = DObjGetStruct(animated_logo_gobj)->child;

		for (i = 0; i < ARRAY_COUNT(dMNTitleLogoAnimSprites) - 1; i++)
		{
			fire_logo_sobj = lbCommonMakeSObjForGObj(fire_logo_gobj, sprite_bank_get(sMNTitleFiles[0], dMNTitleLogoAnimSprites[i]));

			fire_logo_sobj->sprite.attr = SP_TRANSPARENT;

			fire_logo_sobj->pos.x = 0;
			fire_logo_sobj->pos.y = 0;

			fire_logo_sobj->sprite.red = 0x00;
			fire_logo_sobj->sprite.green = 0x00;
			fire_logo_sobj->sprite.blue = 0x00;

			fire_logo_dobj->translate.vec.f.x = 0.0F;
			fire_logo_dobj->translate.vec.f.y = 0.0F;

			fire_logo_dobj = fire_logo_dobj->sib_next;
		};

		logo_gobj = lbCommonMakeSpriteGObj
		(
			11,
			mnTitleFadeOutLogoFuncRun,
			10,
			GOBJ_PRIORITY_DEFAULT,
			mnTitleLogoProcDisplay,
			0,
			GOBJ_PRIORITY_DEFAULT,
			-1,
			sprite_bank_get(sMNTitleFiles[0], dMNTitleLogoAnimSprites[3]),
			nGCProcessKindFunc,
			mnTitleLogoProcUpdate,
			1
		);
		logo_sobj = SObjGetStruct(logo_gobj);

		logo_sobj->sprite.attr = SP_TRANSPARENT;

		logo_sobj->sprite.red = 0xFF;
		logo_sobj->sprite.green = 0x00;
		logo_sobj->sprite.blue = 0x00;

		fire_logo_dobj->translate.vec.f.x = 0.0F;
		fire_logo_dobj->translate.vec.f.y = 0.0F;

		logo_gobj->user_data.p = animated_logo_gobj;

		sMNTitleLogoAlpha = 0xFF;
	}
}

/* gcSetupCommonDObjs and gcAddAnimJointAll over one of file 167's
 * trees. DIVERGES: the game reads both out of the file it loaded; the
 * port loads the tree's pack (MNTITLE_*_TREE), builds
 * the DObjs with dc_model_add_dobjs, and hands gcAddAnimJointAll the
 * table the pack's animation carries, pointer per joint in tree order,
 * which is the one the game's file holds. The host build builds the
 * DObjs but adds no animation: `union AObjEvent32` is eight bytes on
 * x86-64, so its interpreter cannot read the words (src/dc/grcastle.c),
 * and the sprites then stand where mnTitleSetPosition put the DObjs. */
static void mnTitleSetupAnimTree(GObj *gobj, Fighter *tree, sb32 *is_loaded, const char *name)
{
	DObj *joints[FIGHTER_MAX_JOINTS];
	s32 njoints;

	if (*is_loaded == FALSE)
	{
		int pal_bank = 0;

		if (fighter_load_scene(tree, name, &pal_bank) != 0)
		{
			syDebugPrintf("mnTitle: no %s; the title cannot animate\n", name);
			return;
		}
		*is_loaded = TRUE;
	}
	njoints = dc_model_add_dobjs(gobj, NULL, tree, joints);

#ifndef FT_HOSTTEST
	if (njoints > 0)
	{
		AObjEvent32 *anim_joints[FIGHTER_MAX_JOINTS];
		const FPackAnim *anim = &tree->anims[0];
		const s32 *entries = (const s32 *)((const u8 *)tree->blob + anim->off_entries);
		s32 i;

		for (i = 0; i < njoints; i++)
		{
			anim_joints[i] = ((entries[i] >= 0) && ((u32)entries[i] < anim->nwords))
			               ? (AObjEvent32 *)((u8 *)tree->blob + anim->off_words) + entries[i]
			               : NULL;
		}
		gcAddAnimJointAll(gobj, anim_joints, 0.0F);
	}
#else
	(void)njoints;
#endif
}

/* mntitle.c:1181-1235 0x801332E4, REGION_US arms, verbatim but for the
 * tree (mnTitleSetupAnimTree) and the sprite lookup. The six logo sprites
 * on one GObj, flying in on the labels' animation from tic 170 until
 * mnTitleSetEndLayout pins them at 220, and the footer and header on
 * another; both hidden until tic 170. */
void mnTitleMakeLabels(void)
{
	s32 i;
	GObj *animation_gobj;
	GObj *gobj;
	SObj *texture_sobj;
	DObj *animation_dobj;

	animation_gobj = gcMakeGObjSPAfter(10, NULL, 8, GOBJ_PRIORITY_DEFAULT);
	mnTitleSetupAnimTree(animation_gobj, &sMNTitleLabelsTree, &sMNTitleLabelsTreeIsLoaded, MNTITLE_LABELS_TREE);
	gcPlayAnimAll(animation_gobj);

	gobj = gcMakeGObjSPAfter(8, NULL, 8, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, mnTitleProcUpdate, nGCProcessKindFunc, 1);

	gobj->user_data.p = animation_gobj;
	animation_dobj = DObjGetStruct(animation_gobj)->child;

	for (i = 0; i < nMNTitleSpriteKindFooter; i++)
	{
		texture_sobj = lbCommonMakeSObjForGObj(gobj, sprite_bank_get(sMNTitleFiles[0], dMNTitleCommonSpriteDescs[i].offset));
		texture_sobj->sprite.attr = SP_TRANSPARENT;

		mnTitleSetPosition(animation_dobj, texture_sobj, i);
		mnTitleSetColors(texture_sobj, i);

		animation_dobj = animation_dobj->sib_next;
	}
	gobj->flags = GOBJ_FLAG_HIDDEN;

	gobj = gcMakeGObjSPAfter(9, NULL, 8, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, mnTitleUpdateLabelsPosition, nGCProcessKindFunc, 1);

	gobj->user_data.p = animation_gobj;

	for (i = nMNTitleSpriteKindFooter; i < nMNTitleSpriteKindPressStart; i++)
	{
		texture_sobj = lbCommonMakeSObjForGObj(gobj, sprite_bank_get(sMNTitleFiles[0], dMNTitleCommonSpriteDescs[i].offset));
		texture_sobj->sprite.attr = SP_TRANSPARENT;

		mnTitleSetPosition(animation_dobj, texture_sobj, i);
		mnTitleSetColors(texture_sobj, i);

		animation_dobj = animation_dobj->sib_next;
	}
	gobj->flags = GOBJ_FLAG_HIDDEN;
}

/* mntitle.c:1238-1262 0x80133504, verbatim but for the tree and the
 * sprite lookup. PRESS START, hidden until tic 280 and pulsing on its
 * animation from then on. */
void mnTitleMakePressStart(void)
{
	GObj *press_start_anim_gobj;
	GObj *press_start_gobj;
	DObj *press_start_anim_dobj;
	SObj *press_start_sobj;

	press_start_anim_gobj = gcMakeGObjSPAfter(10, NULL, 8, GOBJ_PRIORITY_DEFAULT);
	mnTitleSetupAnimTree(press_start_anim_gobj, &sMNTitlePressStartTree, &sMNTitlePressStartTreeIsLoaded, MNTITLE_PRESS_START_TREE);
	gcPlayAnimAll(press_start_anim_gobj);

	press_start_anim_dobj = DObjGetStruct(press_start_anim_gobj)->child;

	press_start_gobj = gcMakeGObjSPAfter(8, NULL, 9, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(press_start_gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(press_start_gobj, mnTitlePressStartProcUpdate, nGCProcessKindFunc, 1);

	press_start_gobj->user_data.p = press_start_anim_gobj;

	press_start_sobj = lbCommonMakeSObjForGObj(press_start_gobj, sprite_bank_get(sMNTitleFiles[0], dMNTitleCommonSpriteDescs[nMNTitleSpriteKindPressStart].offset));
	press_start_sobj->sprite.attr = SP_TRANSPARENT;

	mnTitleSetPosition(press_start_anim_dobj, press_start_sobj, nMNTitleSpriteKindPressStart);
	mnTitleSetColors(press_start_sobj, nMNTitleSpriteKindPressStart);

	press_start_gobj->flags = GOBJ_FLAG_HIDDEN;
}

/* mntitle.c:1293-1315 0x8013366C, REGION_US arm. The slash that cuts
 * across the logo after the opening movie: a five-joint tree with an
 * MObj on each of its two drawn joints, stepping seven frames on the
 * MatAnimJoint while the AnimJoint sweeps it. DIVERGES the way
 * src/dc/mvopeningstandoff.c's lightning does: gcSetupCustomDObjsWithMObj
 * + gcAddAnimJointAll + gcAddMatAnimJointAll are the pack's tree
 * (dc_model_add_dobjs, dc_model_add_mobjs, the pack's own AnimJoint
 * table), and gcDrawDObjTreeDLLinksForGObj is dc_model_draw_tree_layered
 * -- its camera (DL link 2, priority 40) is ortho and walked after the
 * sprite camera, and the layered draw is what puts a model at the
 * frame's next sprite depth, over the logo the way the N64's later
 * camera drew it (src/dc/objmodel.h). Target only: the host links no
 * renderer for a model with geometry, so link 14 stays empty there and
 * mnTitleTransitionFromFireLogo's guard skips it. */
void mnTitleMakeSlash(void)
{
#ifndef FT_HOSTTEST
	GObj *gobj;

	if (gSCManagerSceneData.scene_prev == nSCKindOpeningNewcomers)
	{
		int pal_bank = 0;

		if (sMNTitleSlashIsLoaded == FALSE)
		{
			if (fighter_load_scene(&sMNTitleSlashPack, MNTITLE_SLASH_MODEL, &pal_bank) != 0)
			{
				syDebugPrintf("mnTitle: no %s; no slash\n", MNTITLE_SLASH_MODEL);
				return;
			}
			sMNTitleSlashIsLoaded = TRUE;
		}
		gobj = gcMakeGObjSPAfter(12, NULL, 14, GOBJ_PRIORITY_DEFAULT);
		gcAddGObjDisplay(gobj, dc_model_draw_tree_layered, 2, GOBJ_PRIORITY_DEFAULT, ~0);

		fighter_clone(&sMNTitleSlashModel, &sMNTitleSlashPack,
		              syTaskmanMalloc(sizeof(float[4]) * sMNTitleSlashPack.hd->vert_count, 0x8));
		dc_model_add_dobjs(gobj, NULL, &sMNTitleSlashModel, NULL);
		dc_model_add_mobjs(gobj, &sMNTitleSlashModel, 0.0F);
		{
			AObjEvent32 *anim_joints[FIGHTER_MAX_JOINTS];
			const FPackAnim *anim = &sMNTitleSlashPack.anims[0];
			const s32 *entries = (const s32 *)((const u8 *)sMNTitleSlashPack.blob + anim->off_entries);
			u32 j;

			for (j = 0; j < FIGHTER_MAX_JOINTS; j++)
			{
				anim_joints[j] = ((j < sMNTitleSlashPack.hd->joint_count) && (entries[j] >= 0) && ((u32)entries[j] < anim->nwords))
				               ? (AObjEvent32 *)((u8 *)sMNTitleSlashPack.blob + anim->off_words) + entries[j]
				               : NULL;
			}
			gcAddAnimJointAll(gobj, anim_joints, 0.0F);
		}
		gcPlayAnimAll(gobj);
		gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
	}
#endif
}

/* mntitle.c:1329-1383 0x80133770, verbatim: the fill colour behind the
 * flames, easing to one of seven colours over 80 tics every 260. */
void mnTitleFireCameraProcUpdate(GObj *gobj)
{
	CObj *cobj = CObjGetStruct(sMNTitleFireCameraGObj);

	(void)gobj;

	if (sMNTitleTransitionTotalTimeTics >= 40)
	{
		if (sMNTitleTransitionTotalTimeTics <= 110)
		{
			sMNTitleFireColorR += 4.0F;

			TAKE_MIN(sMNTitleFireColorR, 255);
		}
		else
		{
			if (sMNTitleFireTimer == 0)
			{
				s32 color_id;

				sMNTitleFireTimer = 260;

				color_id = syUtilsRandTimeUCharRange(7);

				if (color_id == sMNTitleFireColorID)
				{
					color_id++;

					if (color_id >= 7)
					{
						color_id = 0;
					}
				}
				sMNTitleFireColorID = color_id;

				sMNTitleFireColorDeltaR = (dMNTitleFireColorsR[color_id] - sMNTitleFireColorR) / 80.0F;
				sMNTitleFireColorDeltaG = (dMNTitleFireColorsG[color_id] - sMNTitleFireColorG) / 80.0F;
				sMNTitleFireColorDeltaB = (dMNTitleFireColorsB[color_id] - sMNTitleFireColorB) / 80.0F;
			}
			if (sMNTitleFireTimer >= 80)
			{
				sMNTitleFireColorR += sMNTitleFireColorDeltaR;
				sMNTitleFireColorG += sMNTitleFireColorDeltaG;
				sMNTitleFireColorB += sMNTitleFireColorDeltaB;
			}
			sMNTitleFireTimer--;
		}
		TAKE_MIN(sMNTitleFireColorR, 255.0F);
		TAKE_MIN(sMNTitleFireColorG, 255.0F);
		TAKE_MIN(sMNTitleFireColorB, 255.0F);

		TAKE_MAX(sMNTitleFireColorR, 0.0F);
		TAKE_MAX(sMNTitleFireColorG, 0.0F);
		TAKE_MAX(sMNTitleFireColorB, 0.0F);

		cobj->color = GPACK_RGBA8888((s32) sMNTitleFireColorR, (s32) sMNTitleFireColorG, (s32) sMNTitleFireColorB, 0xFF);
	}
}

/* mntitle.c:1386-1448 0x80133A94, AVOID_UB arm, verbatim step
 * 26: four cameras. The fire camera, a fill whose colour
 * mnTitleFireCameraProcUpdate eases (drawn first, DL link priority 100:
 * src/dc/objdisplay.c gcPrepCameraViewport); the sprite camera,
 * lbCommonDrawSprite over DL links 0 and 1 with the viewport that
 * becomes the 10-pixel scissor; the ortho camera the slash is drawn by
 * (DL link 2, priority 40, walked after the sprites); and the
 * perspective camera the logo's fire particles are drawn by (DL link 3,
 * priority 80, walked before them). The last two walk empty links on a
 * visit that does not come from the opening movie, as in the game. */
s32 mnTitleMakeCameras(void)
{
	GObj *camera_gobj;
	CObj *cobj;

	sMNTitleFireCameraGObj = gcMakeDefaultCameraGObj(2, GOBJ_PRIORITY_DEFAULT, 100, COBJ_FLAG_FILLCOLOR | COBJ_FLAG_ZBUFFER, GPACK_RGBA8888(0x00, 0x00, 0x00, 0xFF));
	gcAddGObjProcess(sMNTitleFireCameraGObj, mnTitleFireCameraProcUpdate, nGCProcessKindFunc, 1);

	camera_gobj = gcMakeCameraGObj
	(
		2,
		NULL,
		3,
		GOBJ_PRIORITY_DEFAULT,
		lbCommonDrawSprite,
		60,
		COBJ_MASK_DLLINK(1) | COBJ_MASK_DLLINK(0),
		~0,
		FALSE,
		nGCProcessKindFunc,
		NULL,
		1,
		FALSE
	);
	cobj = CObjGetStruct(camera_gobj);
	syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

	camera_gobj = gcMakeCameraGObj(3, NULL, 3, GOBJ_PRIORITY_DEFAULT, func_80017EC0, 40, COBJ_MASK_DLLINK(2), -1, FALSE, nGCProcessKindFunc, NULL, 1, FALSE);
	cobj = CObjGetStruct(camera_gobj);

	gcAddXObjForCamera(cobj, nGCMatrixKindOrtho, 0);
	gcAddXObjForCamera(cobj, nGCMatrixKindLookAt, 0);

	syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

	cobj->vec.at.x = cobj->vec.at.y = cobj->vec.at.z = 0.0F;

	cobj->vec.eye.x = cobj->vec.eye.y = 0.0F;
	cobj->vec.eye.z = 2000.0F;

	camera_gobj = gcMakeCameraGObj(3, NULL, 3, GOBJ_PRIORITY_DEFAULT, func_80017EC0, 80, COBJ_MASK_DLLINK(3), -1, TRUE, nGCProcessKindFunc, NULL, 1, FALSE);
	cobj = CObjGetStruct(camera_gobj);

	syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

	cobj->vec.at.x = cobj->vec.at.y = cobj->vec.at.z = 0.0F;

	cobj->vec.eye.x = cobj->vec.eye.y = 0.0F;
	cobj->vec.eye.z = 1000.0F;

	cobj->projection.persp.fovy = 30.0F;

	return 0;
}

/* mntitle.c:1450-1459 0x80133CFC. The logo fire's particles, drawn
 * between the render-mode lines src/dc/efdisplay.c's
 * efDisplayZPerspAAXLUProcDisplay writes; DIVERGES as it does,
 * translucent pass only. */
void mnTitleLogoFireProcDisplay(GObj *gobj)
{
	if (gcGetDrawList() != PVR_LIST_TR_POLY) return;

	gDPPipeSync(gSYTaskmanDLHeads[0]++);
	gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);

	lbParticleDrawTextures(gobj);

	gDPSetTexturePersp(gSYTaskmanDLHeads[0]++, G_TP_PERSP);
	gDPSetDepthSource(gSYTaskmanDLHeads[0]++, G_ZS_PIXEL);
	gDPPipeSync(gSYTaskmanDLHeads[0]++);
	gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
}

/* mntitle.c:1462-1470 0x80133DDC. The GObj the particles are drawn on
 * (id 15, link 4, DL link 3) and the scene's own particle bank.
 * DIVERGES as src/dc/gryoster.c does: efParticleGetLoadBankID walks the
 * bank the linker placed (src/game/ssb64/particlebanks.ld) and
 * lbpTexLoadBank binds its textures by name; on the host neither runs,
 * for the reason src/dc/ftmanager.c's ftManagerSetupParticleBankKind
 * gives, and the id stays zero. */
void mnTitleMakeLogoFire(void)
{
	GObj *gobj = gcMakeGObjSPAfter(15, NULL, 4, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, mnTitleLogoFireProcDisplay, 3, GOBJ_PRIORITY_DEFAULT, ~0);

	gobj->camera_mask = COBJ_MASK_DLLINK(0);

#ifndef FT_HOSTTEST
	sMNTitleParticleBankID = efParticleGetLoadBankID
	(
		(uintptr_t)&lMNTitleParticleScriptBankLo,
		(uintptr_t)&lMNTitleParticleScriptBankHi,
		(uintptr_t)&lMNTitleParticleTextureBankLo,
		(uintptr_t)&lMNTitleParticleTextureBankHi
	);

	if (lbpTexLoadBank(sMNTitleParticleBankID, MNTITLE_PARTICLE_BANK) != 0)
	{
		syDebugPrintf("mnTitle: %s.txp did not load; the logo fire is blank\n", MNTITLE_PARTICLE_BANK);
	}
	else
	{
		syDebugPrintf("particles: %s bank %d\n", MNTITLE_PARTICLE_BANK, (int)sMNTitleParticleBankID);
	}
#else
	sMNTitleParticleBankID = 0;
#endif
}

/* mntitle.c:1473-1495 0x80133E68, verbatim but for the tree
 * (mnTitleSetupAnimTree over MNTITLE_FIRE_TREE). After the opening
 * movie only: the fire tree (id 14, link 5), three limbs of two joints,
 * and the bank's script 0 hung on the second limb's leaf, whose
 * AnimJoint carries the generator up the logo. The generator is target
 * only, with the bank. */
void mnTitleMakeLogoFireParticles(void)
{
	GObj *logo_fire_effect_gobj;
	LBGenerator *gn;

	if (gSCManagerSceneData.scene_prev == nSCKindOpeningNewcomers)
	{
		logo_fire_effect_gobj = gcMakeGObjSPAfter(14, NULL, 5, GOBJ_PRIORITY_DEFAULT);

		mnTitleSetupAnimTree(logo_fire_effect_gobj, &sMNTitleFireTree, &sMNTitleFireTreeIsLoaded, MNTITLE_FIRE_TREE);
		gcPlayAnimAll(logo_fire_effect_gobj);
		gcAddGObjProcess(logo_fire_effect_gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);

#ifndef FT_HOSTTEST
		gn = lbParticleMakeGenerator(sMNTitleParticleBankID, 0);

		if (gn != NULL)
		{
			gn->dobj = DObjGetStruct(logo_fire_effect_gobj)->child->sib_next->child;
		}
#else
		(void)gn;
#endif
	}
}

/* mntitle.c:1498-1502 0x80133F3C, verbatim. */
void mnTitleMakeActors(void)
{
	sMNTitleMainGObj = gcMakeGObjSPAfter(0, mnTitleFuncRun, 1, GOBJ_PRIORITY_DEFAULT);
	sMNTitleTransitionsGObj = gcMakeGObjSPAfter(0, mnTitleTransitionsFuncRun, 15, GOBJ_PRIORITY_DEFAULT);
}

/* mntitlefiles.c:37-52 0x80134140. lbRelocInitSetup and
 * lbRelocLoadFilesListed over dMNTitleFileIDs; here, the two banks. */
void mnTitleLoadFiles(void)
{
	if (sprite_bank_load(&sMNTitleBank, MNTITLE_SPRITE_BANK) < 0)
	{
		syDebugPrintf("mnTitle: no sprite bank; the title is blank\n");
	}
	if (sprite_bank_load(&sMNTitleFireBank, MNTITLE_FIRE_BANK) < 0)
	{
		syDebugPrintf("mnTitle: no fire sprite bank; the flames are blank\n");
	}
	sMNTitleFiles[0] = &sMNTitleBank;
	sMNTitleFiles[1] = &sMNTitleFireBank;
}

/* mntitle.c:1505-1532 0x80133F90, in the game's order. Two things are not here: func_ovl10_80133634, which returns at
 * once in REGION_US, and the trailing `while (sySchedulerGetTicCount() <
 * 4215) continue;` after the opening movie -- cut outright
 * (src/dc/mvopeningportraits.h): this port's tic
 * counter only advances inside syTaskmanRunFrame, which runs after
 * func_start returns, so kept verbatim it hangs forever. The wait was
 * the movie's last sync to its music, and the port's openings dropped
 * theirs the same way.
 *
 * The four stop-rumble calls are the game's own loop over the pad
 * array: the title comes straight off a match
 * that may have left a pad buzzing, and syControllerStopRumble over the
 * purupuru (src/dc/gmrumble.c) is a state the port can now set rather
 * than a call it has to skip. */
void mnTitleFuncStart(void)
{
	s32 i;

	for (i = 0; i < ARRAY_COUNT(gSYControllerDevices); i++)
	{
		syControllerStopRumble(i);
	}
	mnTitleLoadFiles();
	mnTitleMakeActors();
	efParticleInitAll();
	mnTitleMakeLogoFire();
	mnTitleMakeCameras();
	mnTitleInitVars();
	mnTitleMakeFire();
	mnTitleMakeLogo();
	mnTitleMakeLabels();
	mnTitleMakePressStart();
	mnTitleMakeSlash();
	mnTitleMakeLogoFireParticles();
}

/* mntitle.c:1541-1544 0x80134098, verbatim. */
void mnTitleFuncUpdate(void)
{
	gcRunAll();
}

/* The bzero arm of syDmaLoadOverlay for overlay 10, this
 * file: sc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnTitleOverlayLoad(void)
{
    if (sMNTitleLabelsTreeIsLoaded)
    {
        fighter_release(&sMNTitleLabelsTree);
    }
    if (sMNTitlePressStartTreeIsLoaded)
    {
        fighter_release(&sMNTitlePressStartTree);
    }
    if (sMNTitleLogoTreeIsLoaded)
    {
        fighter_release(&sMNTitleLogoTree);
    }
    if (sMNTitleFireTreeIsLoaded)
    {
        fighter_release(&sMNTitleFireTree);
    }
#ifndef FT_HOSTTEST
    if (sMNTitleSlashIsLoaded)
    {
        fighter_release(&sMNTitleSlashPack);
    }
    OVERLAY_CLEAR(sMNTitleSlashPack);
    OVERLAY_CLEAR(sMNTitleSlashModel);
    OVERLAY_CLEAR(sMNTitleSlashIsLoaded);
#endif
    OVERLAY_CLEAR(sMNTitleBank);
    OVERLAY_CLEAR(sMNTitleFireBank);
    OVERLAY_CLEAR(sMNTitleLabelsTree);
    OVERLAY_CLEAR(sMNTitleLabelsTreeIsLoaded);
    OVERLAY_CLEAR(sMNTitlePressStartTree);
    OVERLAY_CLEAR(sMNTitlePressStartTreeIsLoaded);
    OVERLAY_CLEAR(sMNTitleLogoTree);
    OVERLAY_CLEAR(sMNTitleLogoTreeIsLoaded);
    OVERLAY_CLEAR(sMNTitleFireTree);
    OVERLAY_CLEAR(sMNTitleFireTreeIsLoaded);
    OVERLAY_CLEAR(sMNTitleParticleBankID);
    OVERLAY_CLEAR(sMNTitleFiles);
    OVERLAY_CLEAR(sMNTitleFireCameraGObj);
    OVERLAY_CLEAR(sMNTitleLayout);
    OVERLAY_CLEAR(sMNTitleTransitionsGObj);
    OVERLAY_CLEAR(sMNTitleMainGObj);
    OVERLAY_CLEAR(sMNTitleTransitionTotalTimeTics);
    OVERLAY_CLEAR(sMNTitleIsStartActorProcess);
    OVERLAY_CLEAR(sMNTitleFireAlpha);
    OVERLAY_CLEAR(sMNTitleFireAlphaUnused);
    OVERLAY_CLEAR(sMNTitleLogoAlpha);
    OVERLAY_CLEAR(sMNTitleIsProceedScene);
    OVERLAY_CLEAR(sMNTitleProceedSceneWait);
    OVERLAY_CLEAR(sMNTitleFireTimer);
    OVERLAY_CLEAR(sMNTitleFireColorR);
    OVERLAY_CLEAR(sMNTitleFireColorG);
    OVERLAY_CLEAR(sMNTitleFireColorB);
    OVERLAY_CLEAR(sMNTitleFireColorDeltaR);
    OVERLAY_CLEAR(sMNTitleFireColorDeltaG);
    OVERLAY_CLEAR(sMNTitleFireColorDeltaB);
    OVERLAY_CLEAR(sMNTitleFireColorID);
    OVERLAY_CLEAR(sMNTitleAllowProceedWait);
}

/* mntitle.c:1547-1559 0x801340B8.
 *
 * DIVERGES: syVideoInit and the zbuffer are the N64's video mode, set
 * once at boot here (src/dc/scvsresults.c says the same), and the
 * arena_size line is the link map. What is left is the boot counter and
 * the decomp's last line, both verbatim.
 *
 * The counter is bumped on the first title of a
 * session (is_title_anim_viewed is still FALSE) and it is what
 * mn/mnvsmode/mnvsmode.c mnVSModeFuncStart reads -- a save older than
 * 21 boots, on a machine whose RSP did not hold the game's own number,
 * turns knockback random for good. gSYMainImemOK is TRUE here
 * (src/dc/sysshim.c), so the port only ever takes the honest arm of
 * that test, but the counter under it is now real. */
void mnTitleStartScene(void)
{
	if ((!gSCManagerSceneData.is_title_anim_viewed) &&
	    (gSCManagerBackupData.boot <= U8_MAX))
	{
		gSCManagerBackupData.boot++;
		/* DIVERGES: the notice for this write says LOADING */
		sy_sram_next_write_is_load();
		lbBackupWrite();
	}
	syTaskmanStartTask(&dMNTitleTaskmanSetup);
}
