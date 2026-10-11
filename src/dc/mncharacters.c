/* mncharacters.c -- see mncharacters.h. Function-for-function port of
 * mn/mndata/mncharacters.c, the Character Data tab(third of
 * DATA's three tabs after mndata.c and mnvsrecord.c/mnsoundtest.c). Every
 * function is the decomp's by name and body on the REGION_US arms, with
 * the same cuts every ported menu scene makes:
 *
 *  - the lighting pre-render (mnCharactersFuncLights) is cut, the same
 *    as dSCVSBattleTaskmanSetup's and dMNPlayersVSTaskmanSetup's own:
 *    the port's lighting is the PVR back end's (src/dc/objdisplay.c),
 *    not a display-list injection, and ftDisplayLightsDrawReflect is
 *    not itself a ported function. This screen is not actually special
 *    among the ported menus for drawing a lit fighter -- mnplayersvs.c
 *    already draws up to four -- and its own pre-render note explains
 *    the cut in full.
 *  - the black-clear camera line in mnCharactersFuncStart is cut: the
 *    PVR clears its own frame.
 *  - syVideoInit, the zbuffer line and the arena_size line are cut:
 *    mnCharactersStartScene's own header note.
 *  - lbRelocInitSetup/lbRelocLoadFilesListed becomes mnCharactersLoadFiles,
 *    sprite_bank_load over three of this screen's four relocData files.
 *    The fourth (llFTEmblemModelsFileID, 35) is not a sprite bank -- see
 *    mnCharactersMakeEmblem below.
 *
 * DIVERGES (new to this step, beyond the standard five):
 *
 *  - The winner's-emblem model. mnCharactersMakeEmblem reads relocData 35
 *    at runtime on the N64 (a DObjDesc/MObjSub/AObjEvent32 triple per
 *    fighter kind); this port has no runtime display-list interpreter,
 *    so scvsresults.c already bakes the same file into ten per-kind
 *    .mdl packs at build time (tools/export/ssb_emblemexport.py,
 *    dMNVSResultsEmblemPacks). This file reuses those exact packs
 *    (dMNCharactersEmblemPacks below, the same ten names in the same
 *    fkind order) rather than exporting anything new, and keeps the
 *    decomp's own display proc (gcDrawDObjTreeForGObj) since this
 *    screen's emblem is not sandwiched between two sprite passes the way
 *    the results screen's is -- unlike mnVSResultsMakeEmblem it needs no
 *    layered proc.
 *  - The figatree heap. The decomp allocates
 *    syTaskmanMalloc(gFTManagerFigatreeHeapSize, 0x10) and hands it to
 *    ftManagerMakeFighter through FTDesc.figatree_heap; mnPlayersVSFuncStart
 *    (src/dc/mnplayersvs.c) already found the port does not need this --
 *    the pack's animations play in place, so ftManagerAllocFigatreeHeapKind
 *    never asks for the copy -- and sets every slot's figatree_heap to
 *    NULL instead of allocating. This file does the same: sMNCharactersFigatreeHeap
 *    is left NULL and nothing is malloc'd for it.
 *  - Character Data is the first live caller of "demo" status ids
 *    (FTSTATUS_CHARACTERS_DEMO, status_id + FTSTAT_CHARDATA_START);
 *    the two ranges above it
 *    (FTSTAT_OPENING1/2_START, mv/'s opening movies) have callers.
 *    src/dc/ftcommon.c's ftMainGetStatusDesc has
 *    the ft/ftmain.c:4550-4552 line that strips FTSTAT_CHARDATA_START
 *    back off before the normal special/action/null dispatch runs, so a
 *    demo fighter's status_id resolves to the same FTStatusDesc row a
 *    battle fighter's would. fp->status_id itself keeps the raw encoded
 *    value, exactly as ft/ftmain.c:4406 does it (before the
 *    subtraction) -- which is also why mnCharactersFighterProcUpdate's
 *    own `fp->status_id == nFTCommonStatusDamageE1` check (mncharacters.c:1921)
 *    compares against the *encoded* value in the game too, and is
 *    ported unchanged.
 */
#include "mncharacters.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"
#include "ftcommon.h"
#include "bgm.h"
#include "fighter.h"
#include "objmodel.h"
#include "efmanager.h"
#include "input.h"
#ifdef _arch_dreamcast
#include <dc/maple.h>
#endif

#include <sys/debug.h>
#include <sys/controller.h>
#include <sys/rdp.h>
#include <sys/utils.h>
#include <sys/objdisplay.h>
#include <sc/scdef.h>
#include <sc/scsubsys/scsubsys.h>
#include <gm/gmsound.h>
#include <lb/lbdef.h>
#include <ft/fighter.h>
#include <mn/mndef.h>
#include <mn/mntypes.h>
#include <PR/os.h>
#include <macros.h>

#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* mncharacters.c:14-28 the shared option-input macros, sMNCharactersChangeWait
 * as the wait cell every mn/mndef.h helper takes by name. */
#define mnCharactersCheckGetOptionButtonInput(is_button, mask) \
mnCommonCheckGetOptionButtonInput(sMNCharactersChangeWait, is_button, mask)

#define mnCharactersCheckGetOptionStickInputUD(stick_range, min, b) \
mnCommonCheckGetOptionStickInputUD(sMNCharactersChangeWait, stick_range, min, b)

#define mnCharactersCheckGetOptionStickInputLR(stick_range, min, b) \
mnCommonCheckGetOptionStickInputLR(sMNCharactersChangeWait, stick_range, min, b)

#define mnCharactersSetOptionChangeWaitP(is_button, stick_range, div) \
mnCommonSetOptionChangeWaitP(sMNCharactersChangeWait, is_button, stick_range, div)

#define mnCharactersSetOptionChangeWaitN(is_button, stick_range, div) \
mnCommonSetOptionChangeWaitN(sMNCharactersChangeWait, is_button, stick_range, div)

/* file 16 (llMNCharactersFileID),
 * tools/export/ssb_spriteexport.py --file 16 --list */
#define llMNCharactersLabelSprite                  0x00630
#define llMNCharactersNameTagDefaultSprite         0x01230
#define llMNCharactersNameTagTallSprite            0x028f0
#define llMNCharactersMarioNameSprite              0x02f98
#define llMNCharactersFoxNameSprite                0x033a0
#define llMNCharactersDonkeyNameSprite             0x04290
#define llMNCharactersSamusNameSprite              0x04910
#define llMNCharactersLuigiNameSprite              0x04f78
#define llMNCharactersLinkNameSprite               0x05398
#define llMNCharactersYoshiNameSprite              0x058f8
#define llMNCharactersCaptainNameSprite            0x06828
#define llMNCharactersKirbyNameSprite              0x06e48
#define llMNCharactersPikachuNameSprite            0x07628
#define llMNCharactersPurinNameSprite              0x082e0
#define llMNCharactersNessNameSprite               0x08828
#define llMNCharactersMarioStorySprite             0x0aca8
#define llMNCharactersFoxStorySprite                0x0d128
#define llMNCharactersDonkeyStorySprite             0x0f5a8
#define llMNCharactersSamusStorySprite              0x11a28
#define llMNCharactersLuigiStorySprite              0x13ea8
#define llMNCharactersLinkStorySprite               0x16328
#define llMNCharactersYoshiStorySprite              0x187a8
#define llMNCharactersCaptainStorySprite            0x1ac28
#define llMNCharactersKirbyStorySprite              0x1d0a8
#define llMNCharactersPikachuStorySprite            0x1f528
#define llMNCharactersPurinStorySprite              0x219a8
#define llMNCharactersNessStorySprite               0x23e28
#define llMNCharactersWorksWallpaperSprite          0x25058
#define llMNCharactersMarioWorksSprite              0x25ab8
#define llMNCharactersFoxWorksSprite                0x26518
#define llMNCharactersDonkeyWorksSprite             0x26f78
#define llMNCharactersSamusWorksSprite              0x279d8
#define llMNCharactersLuigiWorksSprite              0x28438
#define llMNCharactersLinkWorksSprite               0x28e98
#define llMNCharactersYoshiWorksSprite              0x298f8
#define llMNCharactersCaptainWorksSprite            0x2a358
#define llMNCharactersKirbyWorksSprite              0x2adb8
#define llMNCharactersPikachuWorksSprite            0x2b818
#define llMNCharactersPurinWorksSprite              0x2c278
#define llMNCharactersNessWorksSprite               0x2ccd8
#define llMNCharactersMotionSpecialHiInputSprite    0x2cda8
#define llMNCharactersMotionSpecialNInputSprite     0x2ce78
#define llMNCharactersMotionSpecialLwInputSprite    0x2cf48
#define llMNCharactersMarioSpecialHiNameSprite      0x2d088
#define llMNCharactersFoxSpecialHiNameSprite        0x2d1c8
#define llMNCharactersDonkeySpecialHiNameSprite     0x2d308
#define llMNCharactersSamusSpecialHiNameSprite      0x2d448
#define llMNCharactersLinkSpecialHiNameSprite       0x2d588
#define llMNCharactersYoshiSpecialHiNameSprite      0x2d6c8
#define llMNCharactersCaptainSpecialHiNameSprite    0x2d808
#define llMNCharactersKirbySpecialHiNameSprite      0x2d948
#define llMNCharactersPikachuSpecialHiNameSprite    0x2da88
#define llMNCharactersPurinSpecialHiNameSprite      0x2dbc8
#define llMNCharactersNessSpecialHiNameSprite       0x2dd08
#define llMNCharactersMarioSpecialNNameSprite       0x2de48
#define llMNCharactersFoxSpecialNNameSprite         0x2df88
#define llMNCharactersDonkeySpecialNNameSprite      0x2e0c8
#define llMNCharactersSamusSpecialNNameSprite       0x2e208
#define llMNCharactersLinkSpecialNNameSprite        0x2e348
#define llMNCharactersYoshiSpecialNNameSprite       0x2e488
#define llMNCharactersCaptainSpecialNNameSprite     0x2e5c8
#define llMNCharactersKirbySpecialNNameSprite       0x2e740
#define llMNCharactersPikachuSpecialNNameSprite     0x2e888
#define llMNCharactersPurinSpecialNNameSprite       0x2e9c8
#define llMNCharactersNessSpecialNNameSprite        0x2eb08
#define llMNCharactersMarioSpecialLwNameSprite      0x2ec48
#define llMNCharactersFoxSpecialLwNameSprite        0x2ed88
#define llMNCharactersDonkeySpecialLwNameSprite     0x2eec8
#define llMNCharactersSamusSpecialLwNameSprite      0x2f008
#define llMNCharactersLuigiSpecialLwNameSprite      0x2f148
#define llMNCharactersLinkSpecialLwNameSprite       0x2f288
#define llMNCharactersYoshiSpecialLwNameSprite      0x2f3c8
#define llMNCharactersCaptainSpecialLwNameSprite    0x2f508
#define llMNCharactersKirbySpecialLwNameSprite      0x2f648
#define llMNCharactersPikachuSpecialLwNameSprite    0x2f788
#define llMNCharactersPurinSpecialLwNameSprite      0x2f8c8
#define llMNCharactersNessSpecialLwNameSprite       0x2fa08
#define llMNCharactersStoryWallpaperSprite          0x30888

/* file 32 (llMNDataCommonFileID), already exported for mnvsrecord.c and
 * mnsoundtest.c (src/game/ssb64/Makefile romdisk/mndatacommon.spr) */
#define llMNDataCommonDataHeaderSprite 0x00b40
#define llMNDataCommonArrowLSprite     0x00be0
#define llMNDataCommonArrowRSprite     0x00c80

#define MNCHARACTERS_BANK_MAIN        "mncharacters.spr"
#define MNCHARACTERS_BANK_DATACOMMON  "mndatacommon.spr"
#define MNCHARACTERS_BANK_EMBLEMS     "ftemblems.spr"


// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

// 0x801340B0
MNCharactersSpecialMotion dMNCharactersSpecialMotionMario =
{
	// Mario Special Moves
	{
		// SpecialHi
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTMarioStatusSpecialHi),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialN
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTMarioStatusSpecialN),			666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialLw
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTMarioStatusSpecialLw),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		}
	}
};

// 0x801341D0
MNCharactersSpecialMotion dMNCharactersSpecialMotionFox =
{
	// Fox Special Moves
	{
		// SpecialHi
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTFoxStatusSpecialHiStart),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTFoxStatusSpecialHiHold),	 	 35, FTSTATUS_PRESERVE_COLANIM 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTFoxStatusSpecialHi),	 	 	 30, FTSTATUS_PRESERVE_COLANIM 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTFoxStatusSpecialHiEnd),		666, FTSTATUS_PRESERVE_COLANIM 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		},

		// SpecialN
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTFoxStatusSpecialN),			666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		},
	
		// SpecialLw
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTFoxStatusSpecialLwStart),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTFoxStatusSpecialLwLoop),	 	 60, FTSTATUS_PRESERVE_EFFECT 	|
																		 	 FTSTATUS_PRESERVE_COLANIM  },
			{ FTSTATUS_CHARACTERS_DEMO(nFTFoxStatusSpecialLwEnd),		666, FTSTATUS_PRESERVE_EFFECT 	|
																		 	 FTSTATUS_PRESERVE_COLANIM  },
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		}
	}
};

// 0x801342F0
MNCharactersSpecialMotion dMNCharactersSpecialMotionDonkey =
{
	// Donkey Kong Special Moves
	{	
		// SpecialHi
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTDonkeyStatusSpecialHi),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		},

		// SpecialN
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTDonkeyStatusSpecialNStart),	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTDonkeyStatusSpecialNLoop), 	666, FTSTATUS_PRESERVE_COLANIM 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTDonkeyStatusSpecialNEnd),	  	666, FTSTATUS_PRESERVE_COLANIM 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		},

		// SpecialLw
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTDonkeyStatusSpecialLwStart),	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTDonkeyStatusSpecialLwLoop), 	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTDonkeyStatusSpecialLwEnd),	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		}
	}
};

// 0x80134410
MNCharactersSpecialMotion dMNCharactersSpecialMotionSamus =
{
	// Samus Special Moves
	{
		// SpecialHi
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTSamusStatusSpecialHi),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		},

		// SpecialN
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTSamusStatusSpecialNStart),	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTSamusStatusSpecialNLoop), 	 60, FTSTATUS_PRESERVE_COLANIM 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTSamusStatusSpecialNEnd),	  	666, FTSTATUS_PRESERVE_COLANIM 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		},

		// SpecialLw
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTSamusStatusSpecialLw),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		}
	}
};

// 0x80134530
MNCharactersSpecialMotion dMNCharactersSpecialMotionLuigi =
{
	// Luigi Special Moves
	{
		// SpecialHi
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTLuigiStatusSpecialHi),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialN
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTLuigiStatusSpecialN),			666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialLw
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTLuigiStatusSpecialLw),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		}
	}
};

// 0x80134650
MNCharactersSpecialMotion dMNCharactersSpecialMotionLink =
{
	// Link Special Moves
	{
		// SpecialHi
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTLinkStatusSpecialHi),			666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTLinkStatusSpecialHiEnd),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialN
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTLinkStatusSpecialN),			666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialLw
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTLinkStatusSpecialLw),			666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		}
	}
};

// 0x80134770
MNCharactersSpecialMotion dMNCharactersSpecialMotionYoshi =
{
	// Yoshi Special Moves
	{
		// SpecialHi
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTYoshiStatusSpecialHi),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialN
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTYoshiStatusSpecialN),			666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialLw
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTYoshiStatusSpecialLwStart),	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTYoshiStatusSpecialAirLwLoop),	 12, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTYoshiStatusSpecialLwLanding),	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		}
	}
};

// 0x80134890
MNCharactersSpecialMotion dMNCharactersSpecialMotionCaptain =
{
	// Captain Falcon Special Moves
	{
		// SpecialHi
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTCaptainStatusSpecialHi),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialN
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTCaptainStatusSpecialN),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialLw
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTCaptainStatusSpecialLw),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		}
	}
};

// 0x801349B0
MNCharactersSpecialMotion dMNCharactersSpecialMotionKirby =
{
	// Kirby Special Moves
	{
		// SpecialHi
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTKirbyStatusSpecialHi),		666, FTSTATUS_PRESERVE_EFFECT 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTKirbyStatusSpecialAirHiFall),  12, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTKirbyStatusSpecialHiLanding), 666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialN
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTKirbyStatusSpecialNStart),	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTKirbyStatusSpecialNLoop), 	 40, FTSTATUS_PRESERVE_MODELPART|
																			 FTSTATUS_PRESERVE_EFFECT	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTKirbyStatusSpecialNEnd),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialLw
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTKirbyStatusSpecialLwStart),	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTKirbyStatusSpecialLwHold),	666, FTSTATUS_PRESERVE_MODELPART|
																			 FTSTATUS_PRESERVE_EFFECT	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTKirbyStatusSpecialLwEnd),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		}
	}
};

// 0x80134AD0
MNCharactersSpecialMotion dMNCharactersSpecialMotionPikachu =
{
	// Pikachu Special Moves
	{
		// SpecialHi
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTPikachuStatusSpecialHiEnd),	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialN
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTPikachuStatusSpecialN),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialLw
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTPikachuStatusSpecialLwStart),	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTPikachuStatusSpecialLwLoop),	 60, FTSTATUS_PRESERVE_COLANIM 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTPikachuStatusSpecialLwHit),	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTPikachuStatusSpecialLwEnd),	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			// Last row undefined -> all zeroes
		}
	}
};

// 0x80134BF0
MNCharactersSpecialMotion dMNCharactersSpecialMotionPurin =
{
	// Jigglypuff Special Moves
	{
		// SpecialHi
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTPurinStatusSpecialHi),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialN
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTPurinStatusSpecialN),			666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		},

		// SpecialLw
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTPurinStatusSpecialLw),		666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		}
	}
};

// 0x80134D10
MNCharactersSpecialMotion dMNCharactersSpecialMotionNess =
{
	// Ness Special Moves
	{
		// SpecialHi
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTNessStatusSpecialHiStart),	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTNessStatusSpecialHiHold),	 	120, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTNessStatusSpecialHiJibaku),	666, FTSTATUS_PRESERVE_NONE 	}, 	// Did HAL mix these up? THIS one should be 28 frames...
			{ FTSTATUS_CHARACTERS_DEMO(nFTNessStatusSpecialHiEnd),		 28, FTSTATUS_PRESERVE_NONE 	},	// ...not this (the animation lasts only 15 frames)!
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		},

		// SpecialN
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTNessStatusSpecialN),			666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		},
	
		// SpecialLw
		{
			{ FTSTATUS_CHARACTERS_DEMO(nFTNessStatusSpecialLwStart),	666, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_DEMO(nFTNessStatusSpecialLwHold),	 	 60, FTSTATUS_PRESERVE_NONE		},
			{ FTSTATUS_CHARACTERS_DEMO(nFTNessStatusSpecialLwHit),		666, FTSTATUS_PRESERVE_EFFECT 	|
																		 	 FTSTATUS_PRESERVE_COLANIM  },
			{ FTSTATUS_CHARACTERS_DEMO(nFTNessStatusSpecialLwEnd),	 	666, FTSTATUS_PRESERVE_NONE		},
			{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
			{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
		}
	}
};

// 0x80134E30
MNCharactersMotion dMNCharactersCommonMotionDescs[/* */][8] =
{
	// SpecialHi
	{
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// SpecialN
	{
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// SpecialLw
	{
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// WalkSlow
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWalkSlow),	 	 90, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// WalkMiddle
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWalkMiddle), 	 	 90, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// WalkFast
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWalkFast), 	 	 90, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 	 		 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Run
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusDash), 		 	 18, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusRun), 		 	 60, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusRunBrake),   		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// JumpF
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusJumpF), 	   		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusJumpAerialF),		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusFall),   	   		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusLandingLight),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// JumpB
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusJumpB), 	   		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusJumpAerialB),		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusFall),   	   		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusLandingLight),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// JumpAerialF
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusJumpAerialF),		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusLandingLight),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 					  			  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// JumpAerialB
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusJumpAerialB),		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusLandingLight),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 					  			  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Squat
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusSquat), 	   		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusSquatWait),		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusSquatRv),   		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Ottotto
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusOttotto),			666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusOttottoWait),		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 					  			  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// FuraFura
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusFuraFura),	 	 90, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Wait
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// DamageE1
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusDamageE1), 	 60, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// EscapeF
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusEscapeF), 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// EscapeB (oversight, this is actually EscapeF again)
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusEscapeF), 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Attack1
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack11), 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack12),		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},
	
	// AttackDash
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttackDash), 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// AttackS3
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttackS3), 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// AttackHi3
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttackHi3), 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// AttackLw3
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttackLw3), 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusSquatRv),   		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},
	
	// AttackS4
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttackS4), 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// AttackHi4
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttackHi4), 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// AttackLw4
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttackLw4), 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},
	
	// AttackAirN
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttackAirN), 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusFallAerial), 		 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusLandingLight),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// AttackAirF
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttackAirF), 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusFallAerial), 		 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusLandingLight),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// AttackAirB
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttackAirB), 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusFallAerial), 		 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusLandingLight),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},
	
	// AttackAirHi
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttackAirHi), 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusFallAerial), 		 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusLandingLight),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// AttackAirLw
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttackAirLw), 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusFallAerial), 		 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusLandingLight),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// ThrowF
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusThrowF), 			666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},
	
	// ThrowB
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusThrowB), 			666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Win1
	{
		{ nFTDemoStatusWin1, 										666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Win2
	{
		{ nFTDemoStatusWin2, 										666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Win3
	{
		{ nFTDemoStatusWin3, 										666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Win4
	{
		{ nFTDemoStatusWin4, 										666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Lose
	{
		{ nFTDemoStatusLose, 										666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Appeal
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAppeal), 			666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	}
};

// 0x80135CD0
MNCharactersMotion dMNCharactersKirbyJumpAerialMotionDescs[/* */] =
{
	{ FTSTATUS_CHARACTERS_DEMO(nFTKirbyStatusJumpAerialF1),		666, FTSTATUS_PRESERVE_NONE 	},
	{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
	{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
	{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
	{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
	{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
	{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
	{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
};

// 0x80135D30
MNCharactersMotion dMNCharactersKirbyFallMotionDesc = { FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusFall),	 30, FTSTATUS_PRESERVE_NONE 	};

// 0x80135D3C
MNCharactersMotion dMNCharactersPurinJumpAerialMotionDescs[/* */] =
{
	{ FTSTATUS_CHARACTERS_DEMO(nFTPurinStatusJumpAerialF1),		666, FTSTATUS_PRESERVE_NONE 	},
	{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
	{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
	{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
	{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
	{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
	{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
	{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
};

// 0x80135D9C
MNCharactersMotion dMNCharactersPurinFallMotionDesc = { FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusFall),	 30, FTSTATUS_PRESERVE_NONE 	};

// 0x80135DA8
MNCharactersMotion dMNCharactersAttack1MotionDescs[/* */][8] =
{
	// Mario
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack11),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack12),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTMarioStatusAttack13),	 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Fox
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack11),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack12),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTFoxStatusAttack100Start),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTFoxStatusAttack100Loop),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTFoxStatusAttack100End),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Donkey Kong
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack11),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack12),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		// Last row undefined -> all zeroes
	},

	// Samus
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack11),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack12),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Luigi
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack11),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack12),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTLuigiStatusAttack13),	 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Link
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack11),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack12),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTLinkStatusAttack100Start),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTLinkStatusAttack100Loop),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTLinkStatusAttack100End),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Yoshi
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack11),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack12),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Captain Falcon
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack11),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack12),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCaptainStatusAttack13),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCaptainStatusAttack100Start),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCaptainStatusAttack100Loop),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCaptainStatusAttack100End),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Kirby
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack11),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack12),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTKirbyStatusAttack100Start),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTKirbyStatusAttack100Loop),	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTKirbyStatusAttack100End),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Pikachu
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack11),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Jigglypuff
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack11),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack12),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},

	// Ness
	{
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack11),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusAttack12),	 	666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTNessStatusAttack13),	 		666, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusWait), 			 30, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	},
		{ FTSTATUS_CHARACTERS_NULL, 								  1, FTSTATUS_PRESERVE_NONE 	}
	},
};
/* mncharacters.c:1210-1216 dMNCharactersFileIDs, the port's own copy:
 * literal relocData numbers rather than &llXxxFileID symbols, the same
 * substitution every menu scene's file table makes. File 3
 * (llFTEmblemModelsFileID, 35) is not a sprite bank -- its slot in
 * sMNCharactersFiles stays NULL and mnCharactersMakeEmblem reads the
 * baked .mdl packs directly instead (see this file's DIVERGES note). */
u32 dMNCharactersFileIDs[] =
{
    16,   /* &llMNCharactersFileID */
    32,   /* &llMNDataCommonFileID */
    20,   /* &llFTEmblemSpritesFileID */
    35    /* &llFTEmblemModelsFileID */
};

/* // // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // // */

/* mncharacters.c:1238-1329, minus the pad fields (sMNCharactersPad0x801365F0,
 * sMNCharactersPad0x80136610) and dMNCharactersUnknown0x80136238, none of
 * which anything in the decomp file reads, and the status/force-status
 * buffers, which belong to the cut lbRelocInitSetup block. */
s32 sMNCharactersPage;
GObj *sMNCharactersEmblemGObj;
GObj *sMNCharactersNameGObj;
GObj *sMNCharactersStoryGObj;
GObj *sMNCharactersWorksGObj;
GObj *sMNCharactersFighterGObj;
s32 sMNCharactersMotionKind;
s32 sMNCharactersAnimFramesRemain;
sb32 sMNCharactersIsUseAnimFramesRemain;
sb32 sMNCharactersIsAutoRotate;
s32 sMNCharactersUnknown;
GObj *sMNCharactersMotionNameGObj;
GObj *sMNCharactersFighterCameraGObj;
f32 sMNCharactersHeldStickAngle;
union MNCharactersStickUnknown { f32 f; s32 s; } sMNCharactersHeldStickUnknown;
/* DIVERGES: always NULL -- see this file's header note on the figatree
 * heap, the same divergence mnPlayersVSFuncStart already made. */
void *sMNCharactersFigatreeHeap;
sb32 sMNCharactersIsDemo;
s32 sMNCharactersDemoFighterKind0;
s32 sMNCharactersDemoFighterKind1;
s32 sMNCharactersCurrentMotionTrack;
u16 sMNCharactersFighterMask;
s32 sMNCharactersCurrentAnimFrame;
s32 sMNCharactersRecentMotionKinds[3];
s32 sMNCharactersRecentMotionKindsID;
s32 sMNCharactersChangeWait;
s32 sMNCharactersTotalTimeTics;

void *sMNCharactersFiles[ARRAY_COUNT(dMNCharactersFileIDs)];
static SpriteBank sMNCharactersBanks[ARRAY_COUNT(dMNCharactersFileIDs)];

/* DIVERGES: the winner's-emblem model, exactly as scvsresults.c already
 * holds one -- src/dc/scvsresults.c's own sMNVSResultsEmblemModel and
 * dMNVSResultsEmblemPacks say why relocData 35 comes in as a baked pack
 * and not a sprite bank. Twelve kinds, ten emblems: Luigi wears Mario's
 * and Jigglypuff (Purin) wears Pikachu's (pmonsters.mdl), the same
 * duplication the decomp's three parallel tables have. */
static Fighter sMNCharactersEmblemModel;

static const char *const dMNCharactersEmblemPacks[] =
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

/* // // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // // */

/* mncharacters.c:1337-1342 mnCharactersFuncLights 0x80131B00 -- the
 * geometry mode and ftDisplayLightsDrawReflect's two lights at the
 * subsystem's angles -- is not here: dMNCharactersTaskmanSetup's
 * pre-render slot is NULL, the same cut dSCVSBattleTaskmanSetup and
 * dMNPlayersVSTaskmanSetup already make (the port's lighting is the PVR
 * back end's, src/dc/objdisplay.c; ftDisplayLightsDrawReflect itself is
 * not ported). */

/* mncharacters.c:1345-1363 0x80131B58, verbatim. */
s32 mnCharactersGetFighterKind(s32 page)
{
    s32 fkinds[] =
    {
        nFTKindMario,
        nFTKindLuigi,
        nFTKindDonkey,
        nFTKindLink,
        nFTKindSamus,
        nFTKindYoshi,
        nFTKindKirby,
        nFTKindFox,
        nFTKindPikachu,
        nFTKindPurin,
        nFTKindCaptain,
        nFTKindNess
    };
    return fkinds[page];
}

/* mncharacters.c:1366-1371 0x80131BA8, verbatim. */
s32 mnCharactersGetPage(s32 fkind)
{
    s32 pages[] = { 0, 7, 2, 4, 1, 3, 5, 10, 6, 8, 9, 11 };

    return pages[fkind];
}

/* mncharacters.c:1382-1431 0x80131C00, verbatim but for the &-drop on
 * every llMNCharactersXxxSprite (see this file's header: they are
 * plain offsets here, not addressable symbols). */
void mnCharactersMakeStory(s32 fkind)
{
    GObj *gobj;
    SObj *sobj;

    intptr_t offsets[] =
    {
        llMNCharactersMarioStorySprite,
        llMNCharactersFoxStorySprite,
        llMNCharactersDonkeyStorySprite,
        llMNCharactersSamusStorySprite,
        llMNCharactersLuigiStorySprite,
        llMNCharactersLinkStorySprite,
        llMNCharactersYoshiStorySprite,
        llMNCharactersCaptainStorySprite,
        llMNCharactersKirbyStorySprite,
        llMNCharactersPikachuStorySprite,
        llMNCharactersPurinStorySprite,
        llMNCharactersNessStorySprite,
    };

    sMNCharactersStoryGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 26, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCharactersFiles[0], llMNCharactersStoryWallpaperSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 126.0F;
    sobj->pos.y = 54.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCharactersFiles[0], offsets[fkind]));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0xFF;
    sobj->sprite.blue = 0xFF;

    sobj->pos.x = 126.0F;
    sobj->pos.y = 54.0F;
}

/* mncharacters.c:1434-1493 0x80131D44, verbatim but for the &-drop. */
void mnCharactersMakeDecals(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCharactersFiles[1], llMNDataCommonDataHeaderSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 23.0F;
    sobj->pos.y = 17.0F;

    sobj->sprite.red = 0x5F;
    sobj->sprite.green = 0x58;
    sobj->sprite.blue = 0x46;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCharactersFiles[0], llMNCharactersLabelSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 157.0F;
    sobj->pos.y = 23.0F;

    sobj->sprite.red = 0xF2;
    sobj->sprite.green = 0xC7;
    sobj->sprite.blue = 0x0D;

    sobj->envcolor.r = 0x00;
    sobj->envcolor.g = 0x00;
    sobj->envcolor.b = 0x00;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCharactersFiles[1], llMNDataCommonArrowLSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 257.0F;
    sobj->pos.y = 40.0F;

    sobj->sprite.red = 0xE3;
    sobj->sprite.green = 0x7D;
    sobj->sprite.blue = 0x0C;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCharactersFiles[1], llMNDataCommonArrowRSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 275.0F;
    sobj->pos.y = 40.0F;

    sobj->sprite.red = 0xE3;
    sobj->sprite.green = 0x7D;
    sobj->sprite.blue = 0x0C;
}

/* mncharacters.c:1496-1540 0x80131F28. DIVERGES: relocData 35 comes in
 * as a baked Fighter pack (dMNCharactersEmblemPacks, this file's header
 * note), not a runtime DObjDesc/MObjSub/AObjEvent32 triple, so this is
 * fighter_load + dc_model_add_dobjs/dc_model_add_mobjs in place of
 * gcSetupCommonDObjs/gcAddMObjAll/gcAddMatAnimJointAll reading
 * sMNCharactersFiles[3] -- exactly the substitution
 * src/dc/scvsresults.c's mnVSResultsMakeEmblem already makes for the
 * same file. The display proc stays the decomp's own
 * (gcDrawDObjTreeForGObj, already used on baked packs elsewhere --
 * src/dc/efmanager.c, src/dc/stage.c): this emblem is not sandwiched
 * between two sprite passes the way the results screen's is, so it
 * needs none of that proc's layering. */
void mnCharactersMakeEmblem(s32 fkind)
{
    GObj *gobj;

    if (fkind < 0 || fkind >= (s32)ARRAY_COUNT(dMNCharactersEmblemPacks))
    {
        syDebugPrintf("mnCharactersMakeEmblem: no emblem for kind %d\n", (int)fkind);
        return;
    }
#ifndef FT_HOSTTEST
    {
        int pal_bank = 0;

        if (fighter_load_scene(&sMNCharactersEmblemModel, dMNCharactersEmblemPacks[fkind], &pal_bank) != 0)
        {
            return;
        }
    }
#endif
    sMNCharactersEmblemGObj = gobj = gcMakeGObjSPAfter(0, NULL, 19, GOBJ_PRIORITY_DEFAULT);

#ifndef FT_HOSTTEST
    dc_model_add_dobjs(gobj, NULL, &sMNCharactersEmblemModel, NULL);
    /* DIVERGES: dc_model_proc_display for gcDrawDObjTreeForGObj -- the
     * bare tree walk submits nothing for a pack's DObjs, so the emblem
     * never drew (frame dump, 2026-09-22). The camera runs first (90),
     * so the emblem lies under every sprite, as on the N64. */
    gcAddGObjDisplay(gobj, dc_model_proc_display, 28, GOBJ_PRIORITY_DEFAULT, ~0);
    dc_model_add_mobjs(gobj, &sMNCharactersEmblemModel, 4.0F);
#else
    /* the host cross-test links the scene, not the renderer -- see
     * src/dc/scvsresults.c's mnVSResultsMakeEmblem, the same seam. */
    gcAddDObjRpyR(gobj, NULL);
#endif
    gcPlayAnimAll(gobj);

    DObjGetStruct(gobj)->translate.vec.f.x = -350.0F;
    DObjGetStruct(gobj)->translate.vec.f.y = 200.0F;
    DObjGetStruct(gobj)->translate.vec.f.z = 0.0F;
    DObjGetStruct(gobj)->scale.vec.f.x = 1.7F;
    DObjGetStruct(gobj)->scale.vec.f.y = 1.7F;
}

/* mncharacters.c:1543-1623 0x801320E4, verbatim but for the &-drop and
 * the REGION_US arm of the pos[] table (the JP one is one row
 * different, mncharacters.c:1560-1564). */
void mnCharactersMakeName(s32 fkind)
{
    GObj *gobj;
    SObj *sobj;

    Vec2f pos[] =
    {
        { 33.0F, 50.0F },
        { 46.0F, 51.0F },
        { 24.0F, 51.0F },
        { 24.0F, 51.0F },
        { 38.0F, 50.0F },
        { 44.0F, 49.0F },
        { 32.0F, 49.0F },
        { 24.0F, 48.0F },
        { 34.0F, 49.0F },
        { 23.0F, 50.0F },
        { 34.0F, 49.0F },
        { 42.0F, 52.0F }
    };
    intptr_t offsets[] =
    {
        llMNCharactersMarioNameSprite,  llMNCharactersFoxNameSprite,
        llMNCharactersDonkeyNameSprite, llMNCharactersSamusNameSprite,
        llMNCharactersLuigiNameSprite,  llMNCharactersLinkNameSprite,
        llMNCharactersYoshiNameSprite,  llMNCharactersCaptainNameSprite,
        llMNCharactersKirbyNameSprite,  llMNCharactersPikachuNameSprite,
        llMNCharactersPurinNameSprite,  llMNCharactersNessNameSprite
    };

    sMNCharactersNameGObj = gobj = gcMakeGObjSPAfter(0, NULL, 20, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 29, GOBJ_PRIORITY_DEFAULT, ~0);

    if ((fkind == nFTKindPurin) || (fkind == nFTKindCaptain))
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCharactersFiles[0], llMNCharactersNameTagTallSprite));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = 10.0F;
        sobj->pos.y = 44.0F;

        sobj->sprite.red = 0x7D;
        sobj->sprite.green = 0x45;
        sobj->sprite.blue = 0x07;
    }
    else
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCharactersFiles[0], llMNCharactersNameTagDefaultSprite));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = 10.0F;
        sobj->pos.y = 45.0F;

        sobj->sprite.red = 0x7D;
        sobj->sprite.green = 0x45;
        sobj->sprite.blue = 0x07;
    }
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCharactersFiles[0], offsets[fkind]));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos[fkind].x;
    sobj->pos.y = pos[fkind].y;

    sobj->sprite.red = 0x7D;
    sobj->sprite.green = 0x45;
    sobj->sprite.blue = 0x07;
}

/* mncharacters.c:1626-1645 0x801322F0, verbatim but for the &-drop. */
void mnCharactersMakeWorksWallpaper(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 21, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 30, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCharactersFiles[0], llMNCharactersWorksWallpaperSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 116.0F;
    sobj->pos.y = 173.0F;

    sobj->sprite.red = 0xCF;
    sobj->sprite.green = 0xCF;
    sobj->sprite.blue = 0xAE;
}

/* mncharacters.c:1648-1683 0x8013239C, verbatim but for the &-drop. */
void mnCharactersMakeWorks(s32 fkind)
{
    GObj *gobj;
    SObj *sobj;

    intptr_t offsets[] =
    {
        llMNCharactersMarioWorksSprite,
        llMNCharactersFoxWorksSprite,
        llMNCharactersDonkeyWorksSprite,
        llMNCharactersSamusWorksSprite,
        llMNCharactersLuigiWorksSprite,
        llMNCharactersLinkWorksSprite,
        llMNCharactersYoshiWorksSprite,
        llMNCharactersCaptainWorksSprite,
        llMNCharactersKirbyWorksSprite,
        llMNCharactersPikachuWorksSprite,
        llMNCharactersPurinWorksSprite,
        llMNCharactersNessWorksSprite,
    };

    sMNCharactersWorksGObj = gobj = gcMakeGObjSPAfter(0, NULL, 22, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 31, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCharactersFiles[0], offsets[fkind]));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 139.0F;
    sobj->pos.y = 180.0F;

    sobj->sprite.red = 0xBC;
    sobj->sprite.green = 0xBF;
    sobj->sprite.blue = 0xFF;
}

/* mncharacters.c:1686-1691 0x80132494, verbatim. */
void mnCharactersSetFighterScale(GObj *fighter_gobj, s32 fkind)
{
    DObjGetStruct(fighter_gobj)->scale.vec.f.x = dSCSubsysFighterScales[fkind];
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = dSCSubsysFighterScales[fkind];
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = dSCSubsysFighterScales[fkind];
}

/* mncharacters.c:1694-1699 0x801324CC, verbatim. */
void mnCharactersSetFighterPosition(GObj *fighter_gobj, s32 fkind)
{
    (void)fkind;
    DObjGetStruct(fighter_gobj)->translate.vec.f.x = 0.0F;
    DObjGetStruct(fighter_gobj)->translate.vec.f.y = -100.0F;
    DObjGetStruct(fighter_gobj)->translate.vec.f.z = 0.0F;
}

/* mncharacters.c:1708-1772 0x80132500, verbatim. */
MNCharactersMotion *mnCharactersGetMotion(MNCharactersMotion *motion, s32 fkind, s32 motion_kind, s32 unused, s32 track)
{
    MNCharactersSpecialMotion *special_motions[] =
    {
        &dMNCharactersSpecialMotionMario,  &dMNCharactersSpecialMotionFox,
        &dMNCharactersSpecialMotionDonkey, &dMNCharactersSpecialMotionSamus,
        &dMNCharactersSpecialMotionLuigi,  &dMNCharactersSpecialMotionLink,
        &dMNCharactersSpecialMotionYoshi,  &dMNCharactersSpecialMotionCaptain,
        &dMNCharactersSpecialMotionKirby,  &dMNCharactersSpecialMotionPikachu,
        &dMNCharactersSpecialMotionPurin,  &dMNCharactersSpecialMotionNess
    };

    (void)unused;

    if ((motion_kind == nMNCharactersMotionKindSpecialHi) || (motion_kind == nMNCharactersMotionKindSpecialN) || (motion_kind == nMNCharactersMotionKindSpecialLw))
    {
        *motion = special_motions[fkind]->motions[motion_kind][track];

        return motion;
    }
    else
    {
        if (fkind == nFTKindKirby)
        {
            if ((motion_kind == nMNCharactersMotionKindJumpAerialF) || (motion_kind == nMNCharactersMotionKindJumpAerialB))
            {
                *motion = dMNCharactersKirbyJumpAerialMotionDescs[track];

                return motion;
            }
            else if ((motion_kind >= nMNCharactersMotionKindAttackAirStart) && (motion_kind <= nMNCharactersMotionKindAttackAirEnd) && (track == 1))
            {
                *motion = dMNCharactersKirbyFallMotionDesc;

                return motion;
            }
        }
        if (fkind == nFTKindPurin)
        {
            if ((motion_kind == nMNCharactersMotionKindJumpAerialF) || (motion_kind == nMNCharactersMotionKindJumpAerialB))
            {
                *motion = dMNCharactersPurinJumpAerialMotionDescs[track];

                return motion;
            }
            else if ((motion_kind >= nMNCharactersMotionKindAttackAirStart) && (motion_kind <= nMNCharactersMotionKindAttackAirEnd) && (track == 1))
            {
                *motion = dMNCharactersPurinFallMotionDesc;

                return motion;
            }
        }
        if (motion_kind == nMNCharactersMotionKindAttack1)
        {
            *motion = dMNCharactersAttack1MotionDescs[fkind][track];

            return motion;
        }
        else
        {
            *motion = dMNCharactersCommonMotionDescs[motion_kind][track];

            return motion;
        }
    }
}

/* mncharacters.c:1775-1782 0x80132768, verbatim. */
sb32 mnCharactersCheckFighterAnimEnd(GObj *fighter_gobj)
{
    if (fighter_gobj->anim_frame == 0.0F)
    {
        return TRUE;
    }
    else return FALSE;
}

/* mncharacters.c:1785-1794 0x80132794, verbatim. */
void mnCharactersInitRecentMotionKinds(void)
{
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(sMNCharactersRecentMotionKinds); i++)
    {
        sMNCharactersRecentMotionKinds[i] = nMNCharactersMotionKindEnumCount;
    }
    sMNCharactersRecentMotionKindsID = 0;
}

/* mncharacters.c:1797-1809 0x801327C0, verbatim. */
sb32 mnCharactersCheckRecentMotionKind(s32 motion_kind)
{
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(sMNCharactersRecentMotionKinds); i++)
    {
        if (motion_kind == sMNCharactersRecentMotionKinds[i])
        {
            return TRUE;
        }
    }
    return FALSE;
}

#ifdef DB_CHARACTERS_TOUR
/* -DDB_CHARACTERS_TOUR=N (src/dc/db.h): N pages, each fighter playing
 * every motion kind once, in order, and the page turned after the last
 * -- what a -DDB_ANIM_USE trace of this screen needs to see every
 * animation the screen can play, where the game's random pick would
 * take hours to. Which N pages is the pads plugged in: one pad starts
 * at the first page, two at page N+1, three at 2N+1, so three runs
 * with 1, 2 and 3 pads tour twelve fighters at once. */
static s32 sMNCharactersTourKind;
static s32 sMNCharactersTourPages;
static sb32 sMNCharactersTourTurn;

static void mnCharactersTourInit(void)
{
    s32 pads = 0;
    s32 i;

    /* maple itself: a boot straight into this screen gets here before
     * the first poll, which is what sy_input_port_present answers from */
    for (i = 0; i < 4; i++)
    {
#ifdef _arch_dreamcast
        maple_device_t *dev = maple_enum_dev(i, 0);

        pads += (dev != NULL && (dev->info.functions & MAPLE_FUNC_CONTROLLER));
#else
        pads += (sy_input_port_present(i) != FALSE);
#endif
    }
    sMNCharactersFighterMask = ~0;
    sMNCharactersPage = nFTKindPlayableStart + ((pads > 0) ? pads - 1 : 0) * DB_CHARACTERS_TOUR;
    if (sMNCharactersPage > nFTKindPlayableEnd)
    {
        sMNCharactersPage = nFTKindPlayableStart;
    }
    sMNCharactersTourKind = 0;
    sMNCharactersTourPages = 0;
    sMNCharactersTourTurn = FALSE;
    syDebugPrintf("characters tour: %d pads, page %d, fighter %d\n", (int)pads,
                  (int)sMNCharactersPage, (int)mnCharactersGetFighterKind(sMNCharactersPage));
}

/* From the scene's own update, where changing the fighter is safe. */
static void mnCharactersTourUpdate(void)
{
    if (sMNCharactersTourTurn == FALSE)
    {
        return;
    }
    sMNCharactersTourTurn = FALSE;

    if (++sMNCharactersTourPages >= DB_CHARACTERS_TOUR)
    {
        syDebugPrintf("characters tour: done, %d pages\n", (int)sMNCharactersTourPages);
        return;
    }
    sMNCharactersPage = (sMNCharactersPage == nFTKindPlayableEnd) ? nFTKindPlayableStart : sMNCharactersPage + 1;
    sMNCharactersTourKind = 0;
    syDebugPrintf("characters tour: page %d, fighter %d\n", (int)sMNCharactersPage,
                  (int)mnCharactersGetFighterKind(sMNCharactersPage));
    mnCharactersChangeFighter(mnCharactersGetFighterKind(sMNCharactersPage));
    mnCharactersResetFighterCamera();
}
#endif

/* mncharacters.c:1812-1831 0x801327FC, verbatim. */
s32 mnCharactersRandMotionKind(void)
{
    s32 motion_kind;

#ifdef DB_CHARACTERS_TOUR
    if (sMNCharactersTourPages >= DB_CHARACTERS_TOUR)
    {
        return nMNCharactersMotionKindWait;
    }
    if (sMNCharactersTourKind >= nMNCharactersMotionKindEnumCount)
    {
        sMNCharactersTourTurn = TRUE;
        return nMNCharactersMotionKindWait;
    }
    return sMNCharactersTourKind++;
#endif

    do
    {
        motion_kind = syUtilsRandTimeUCharRange(nMNCharactersMotionKindEnumCount);
    }
    while (mnCharactersCheckRecentMotionKind(motion_kind) != FALSE);

    sMNCharactersRecentMotionKinds[sMNCharactersRecentMotionKindsID] = motion_kind;

    if (sMNCharactersRecentMotionKindsID >= ((s32)ARRAY_COUNT(sMNCharactersRecentMotionKinds) - 1))
    {
        sMNCharactersRecentMotionKindsID = 0;
    }
    else sMNCharactersRecentMotionKindsID++;

    return motion_kind;
}

/* mncharacters.c:1834-1875 0x8013286C, verbatim. */
MNCharactersMotion *mnCharactersSetMotion(MNCharactersMotion *motion, s32 motion_kind)
{
    MNCharactersMotion get_motion;

    mnCharactersGetMotion
    (
        &get_motion,
        mnCharactersGetFighterKind(sMNCharactersPage),
        motion_kind,
        sMNCharactersUnknown,
        sMNCharactersCurrentMotionTrack
    );
    if (get_motion.status_id == FTSTATUS_CHARACTERS_NULL)
    {
        sMNCharactersCurrentAnimFrame++;

        if (get_motion.anim_length == sMNCharactersCurrentAnimFrame)
        {
            sMNCharactersMotionKind = motion_kind = mnCharactersRandMotionKind();
            sMNCharactersCurrentAnimFrame = 0;
        }
        sMNCharactersCurrentMotionTrack = 0;

        mnCharactersGetMotion
        (
            &get_motion,
            mnCharactersGetFighterKind(sMNCharactersPage),
            motion_kind,
            sMNCharactersUnknown,
            sMNCharactersCurrentMotionTrack
        );
        *motion = get_motion;

        return motion;
    }
    else
    {
        *motion = get_motion;

        return motion;
    }
}

/* mncharacters.c:1878-1889 0x80132984, verbatim. */
MNCharactersMotion *mnCharactersAdvanceTrack(MNCharactersMotion *motion, s32 unused)
{
    MNCharactersMotion get_motion;

    (void)unused;

    sMNCharactersCurrentMotionTrack++;

    mnCharactersSetMotion(&get_motion, sMNCharactersMotionKind);

    *motion = get_motion;

    return motion;
}

/* mncharacters.c:1892-1985 0x801329E8, verbatim. */
void mnCharactersFighterProcUpdate(GObj *fighter_gobj)
{
    MNCharactersMotion new_motion;
    MNCharactersMotion next_motion;
    FTStruct *fp;

    fp = ftGetStruct(fighter_gobj);

    if (sMNCharactersIsUseAnimFramesRemain != FALSE)
    {
        if (sMNCharactersAnimFramesRemain != 0)
        {
            sMNCharactersAnimFramesRemain--;
        }
        else
        {
            mnCharactersAdvanceTrack
            (
                &new_motion,
                mnCharactersGetFighterKind(sMNCharactersPage)
            );
            mnCharactersGetMotion
            (
                &next_motion,
                mnCharactersGetFighterKind(sMNCharactersPage),
                sMNCharactersMotionKind,
                sMNCharactersUnknown,
                sMNCharactersCurrentMotionTrack + 1
            );
            if (fp->status_id == nFTCommonStatusDamageE1)
            {
                ftParamResetFighterColAnim(fighter_gobj);
            }
            ftMainSetStatus(fighter_gobj, new_motion.status_id, 0.0F, 1.0F, new_motion.flags);

            if (new_motion.status_id == FTSTATUS_CHARACTERS_DEMO(nFTCommonStatusDamageE1))
            {
                ftParamCheckSetSkeletonColAnimID(fighter_gobj, 3);
            }
            mnCharactersUpdateMotionName(sMNCharactersMotionNameGObj);

            if (new_motion.anim_length != 666)
            {
                if (next_motion.status_id == FTSTATUS_CHARACTERS_NULL)
                {
                    sMNCharactersAnimFramesRemain = new_motion.anim_length + 20;
                }
                else sMNCharactersAnimFramesRemain = new_motion.anim_length;

                sMNCharactersIsUseAnimFramesRemain = TRUE;
            }
            else
            {
                sMNCharactersAnimFramesRemain = 0;
                sMNCharactersIsUseAnimFramesRemain = FALSE;
            }
        }
    }
    else if (mnCharactersCheckFighterAnimEnd(fighter_gobj))
    {
        mnCharactersGetMotion
        (
            &new_motion,
            mnCharactersGetFighterKind(sMNCharactersPage),
            sMNCharactersMotionKind,
            sMNCharactersUnknown,
            sMNCharactersCurrentMotionTrack
        );
        mnCharactersGetMotion
        (
            &next_motion,
            mnCharactersGetFighterKind(sMNCharactersPage),
            sMNCharactersMotionKind,
            sMNCharactersUnknown,
            sMNCharactersCurrentMotionTrack + 1
        );
        if (next_motion.status_id == FTSTATUS_CHARACTERS_NULL)
        {
            sMNCharactersAnimFramesRemain = 20;
        }
        else sMNCharactersAnimFramesRemain = 0;

        sMNCharactersIsUseAnimFramesRemain = TRUE;
    }
    if (sMNCharactersIsAutoRotate != FALSE)
    {
        DObjGetStruct(fighter_gobj)->rotate.vec.f.y += F_CST_DTOR32(180.0F) / 360.0F;

        if (DObjGetStruct(fighter_gobj)->rotate.vec.f.y > F_CST_DTOR32(360.0F))
        {
            DObjGetStruct(fighter_gobj)->rotate.vec.f.y -= F_CST_DTOR32(360.0F);
        }
    }
}

/* mncharacters.c:1988-2034 0x80132C40. DIVERGES: sMNCharactersFigatreeHeap
 * is always NULL (this file's header note), where the decomp copies
 * sMNCharactersFigatreeHeap into desc.figatree_heap the same way either
 * side -- the field is just never allocated here. */
void mnCharactersMakeFighter(s32 fkind)
{
    GObj *fighter_gobj;
    FTStruct *fp;
    FTDesc desc = dFTManagerDefaultFighterDesc;
    MNCharactersMotion motion;

    desc.fkind = mnCharactersGetFighterKind(sMNCharactersPage);
    desc.costume = ftParamGetCostumeCommonID(mnCharactersGetFighterKind(sMNCharactersPage), 0);
    desc.figatree_heap = sMNCharactersFigatreeHeap;
    sMNCharactersFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

    gcAddGObjProcess(fighter_gobj, mnCharactersFighterProcUpdate, nGCProcessKindFunc, 1);

    fp = ftGetStruct(fighter_gobj);

    fp->is_muted = TRUE;

    mnCharactersSetFighterScale(fighter_gobj, fkind);
    mnCharactersSetFighterPosition(fighter_gobj, fkind);

    sMNCharactersUnknown = 1;

    mnCharactersInitRecentMotionKinds();

    sMNCharactersMotionKind = mnCharactersRandMotionKind();
    sMNCharactersCurrentMotionTrack = 0;
    sMNCharactersCurrentAnimFrame = 0;

    mnCharactersSetMotion(&motion, sMNCharactersMotionKind);

    ftMainSetStatus(fighter_gobj, motion.status_id, 0.0F, 1.0F, motion.flags);

    if (motion.anim_length != 666)
    {
        sMNCharactersIsUseAnimFramesRemain = TRUE;
        sMNCharactersAnimFramesRemain = motion.anim_length;
    }
    else
    {
        sMNCharactersIsUseAnimFramesRemain = FALSE;
        sMNCharactersAnimFramesRemain = 0;
    }
    mnCharactersUpdateMotionName(sMNCharactersMotionNameGObj);

    sMNCharactersIsAutoRotate = TRUE;
}

/* mncharacters.c:2037-2053 0x80132DD4, verbatim. */
s32 mnCharactersGetMotionKind(void)
{
    switch (sMNCharactersMotionKind)
    {
    case nMNCharactersMotionKindSpecialHi:
        return nMNCharactersMotionKindSpecialHi;

    case nMNCharactersMotionKindSpecialN:
        return nMNCharactersMotionKindSpecialN;

    case nMNCharactersMotionKindSpecialLw:
        return nMNCharactersMotionKindSpecialLw;

    default:
        return nMNCharactersMotionKindCommonStart;
    }
}

/* mncharacters.c:2056-2116 0x80132E20, verbatim but for the &-drop. */
void mnCharactersUpdateMotionName(GObj *gobj)
{
    SObj *sobj;

    intptr_t motion_names[][3] =
    {
        { llMNCharactersMarioSpecialHiNameSprite,   llMNCharactersMarioSpecialNNameSprite,   llMNCharactersMarioSpecialLwNameSprite   },
        { llMNCharactersFoxSpecialHiNameSprite,     llMNCharactersFoxSpecialNNameSprite,     llMNCharactersFoxSpecialLwNameSprite     },
        { llMNCharactersDonkeySpecialHiNameSprite,  llMNCharactersDonkeySpecialNNameSprite,  llMNCharactersDonkeySpecialLwNameSprite  },
        { llMNCharactersSamusSpecialHiNameSprite,   llMNCharactersSamusSpecialNNameSprite,   llMNCharactersSamusSpecialLwNameSprite   },
        { llMNCharactersMarioSpecialHiNameSprite,   llMNCharactersMarioSpecialNNameSprite,   llMNCharactersLuigiSpecialLwNameSprite   },
        { llMNCharactersLinkSpecialHiNameSprite,    llMNCharactersLinkSpecialNNameSprite,    llMNCharactersLinkSpecialLwNameSprite    },
        { llMNCharactersYoshiSpecialHiNameSprite,   llMNCharactersYoshiSpecialNNameSprite,   llMNCharactersYoshiSpecialLwNameSprite   },
        { llMNCharactersCaptainSpecialHiNameSprite, llMNCharactersCaptainSpecialNNameSprite, llMNCharactersCaptainSpecialLwNameSprite },
        { llMNCharactersKirbySpecialHiNameSprite,   llMNCharactersKirbySpecialNNameSprite,   llMNCharactersKirbySpecialLwNameSprite   },
        { llMNCharactersPikachuSpecialHiNameSprite, llMNCharactersPikachuSpecialNNameSprite, llMNCharactersPikachuSpecialLwNameSprite },
        { llMNCharactersPurinSpecialHiNameSprite,   llMNCharactersPurinSpecialNNameSprite,   llMNCharactersPurinSpecialLwNameSprite   },
        { llMNCharactersNessSpecialHiNameSprite,    llMNCharactersNessSpecialNNameSprite,    llMNCharactersNessSpecialLwNameSprite    }
    };

    intptr_t motion_inputs[] =
    {
        llMNCharactersMotionSpecialHiInputSprite,
        llMNCharactersMotionSpecialNInputSprite,
        llMNCharactersMotionSpecialLwInputSprite
    };

    s32 motion_kind = mnCharactersGetMotionKind();
    s32 fkind = mnCharactersGetFighterKind(sMNCharactersPage);

    gcRemoveSObjAll(gobj);

    if (motion_kind != nMNCharactersMotionKindCommonStart)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCharactersFiles[0], motion_inputs[motion_kind]));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = 24.0F;
        sobj->pos.y = 199.0F;

        sobj->sprite.red = 0xE3;
        sobj->sprite.green = 0x7D;
        sobj->sprite.blue = 0x0C;

        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNCharactersFiles[0], motion_names[fkind][motion_kind]));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = 24.0F;
        sobj->pos.y = 210.0F;

        sobj->sprite.red = 0xE3;
        sobj->sprite.green = 0x7D;
        sobj->sprite.blue = 0x0C;
    }
}

/* mncharacters.c:2119-2127 0x80132FA0, verbatim. */
void mnCharactersMakeMotionName(void)
{
    GObj *gobj;

    sMNCharactersMotionNameGObj = gobj = gcMakeGObjSPAfter(0, NULL, 22, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 31, GOBJ_PRIORITY_DEFAULT, ~0);

    mnCharactersUpdateMotionName(sMNCharactersMotionNameGObj);
}

/* mncharacters.c:2130-2152 0x80133000, verbatim. */
void mnCharactersMakeStoryCamera(void)
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

/* mncharacters.c:2155-2177 0x801330A0, verbatim. */
void mnCharactersMakeDecalsCamera(void)
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

/* mncharacters.c:2180-2212 0x80133140, verbatim. */
void mnCharactersMakeEmblemCamera(void)
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
            90,
            COBJ_MASK_DLLINK(28),
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

/* mncharacters.c:2215-2237 0x80133224, verbatim. */
void mnCharactersMakeNameCamera(void)
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

/* mncharacters.c:2240-2262 0x801332C4, verbatim. */
void mnCharactersMakeWorksWallpaperCamera(void)
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

/* mncharacters.c:2265-2287 0x80133364, verbatim. */
void mnCharactersMakeWorksCamera(void)
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

/* mncharacters.c:2290-2325 0x80133404, verbatim. */
void mnCharactersMakeFighterCamera(void)
{
    CObj *cobj;

    sMNCharactersFighterCameraGObj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017DBC,
        30,
        COBJ_MASK_DLLINK(18) | COBJ_MASK_DLLINK(15) |
        COBJ_MASK_DLLINK(10) | COBJ_MASK_DLLINK(9),
        ~0,
        TRUE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    cobj = CObjGetStruct(sMNCharactersFighterCameraGObj);
    cobj->flags = COBJ_FLAG_ZBUFFER;

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->vec.eye.x = 0.0F;
    cobj->vec.eye.y = 0.0F;
    cobj->vec.eye.z = 3000.0F;
    cobj->vec.at.x = 700.0F;
    cobj->vec.at.y = 370.0F;
    cobj->vec.at.z = 0.0F;
    cobj->vec.up.x = 0.0F;
    cobj->vec.up.y = 1.0F;
    cobj->vec.up.z = 0.0F;
}

/* mncharacters.c:2328-2345 0x80133510, verbatim. */
sb32 mnCharactersCheckHaveFighterKind(s32 fkind)
{
    if
    (
        (fkind == nFTKindLuigi)    ||
        (fkind == nFTKindCaptain)  ||
        (fkind == nFTKindPurin)    ||
        (fkind == nFTKindNess)
    )
    {
        if (sMNCharactersFighterMask & LBBACKUP_MASK_FIGHTER(fkind))
        {
            return TRUE;
        }
        else return FALSE;
    }
    else return TRUE;
}

/* mncharacters.c:2348-2380 0x80133568, verbatim. */
void mnCharactersInitVars(void)
{
    sMNCharactersTotalTimeTics = 0;

    sMNCharactersEmblemGObj = NULL;
    sMNCharactersNameGObj = NULL;
    sMNCharactersStoryGObj = NULL;
    sMNCharactersWorksGObj = NULL;
    sMNCharactersFighterGObj = NULL;
    sMNCharactersMotionNameGObj = NULL;

    sMNCharactersMotionKind = 0;
    sMNCharactersAnimFramesRemain = 0;
    sMNCharactersIsUseAnimFramesRemain = FALSE;
    sMNCharactersIsAutoRotate = TRUE;
    sMNCharactersUnknown = 1;
    sMNCharactersCurrentMotionTrack = 0;
    sMNCharactersFighterMask = gSCManagerBackupData.fighter_mask;

    if (gSCManagerSceneData.scene_prev == nSCKindData)
    {
        sMNCharactersPage = mnCharactersGetPage(gSCManagerBackupData.characters_fkind);
        sMNCharactersIsDemo = FALSE;
    }
    else
    {
        sMNCharactersIsDemo = TRUE;
        sMNCharactersDemoFighterKind0 = gSCManagerSceneData.demo_fkind[0];
        sMNCharactersDemoFighterKind1 = gSCManagerSceneData.demo_fkind[1];
        sMNCharactersPage = mnCharactersGetPage(sMNCharactersDemoFighterKind0);
    }
    mnCharactersInitRecentMotionKinds();
#ifdef DB_CHARACTERS_TOUR
    mnCharactersTourInit();
#endif
}

/* mncharacters.c:2383-2388 0x8013366C, verbatim. */
void mnCharactersBackupFighterKind(void)
{
    gSCManagerBackupData.characters_fkind = mnCharactersGetFighterKind(sMNCharactersPage);

    lbBackupWrite();
}

/* mncharacters.c:2391-2418 0x8013369C, verbatim. */
void mnCharactersChangeFighter(s32 fkind)
{
    if (sMNCharactersEmblemGObj != NULL)
    {
        gcEjectGObj(sMNCharactersEmblemGObj);
        mnCharactersMakeEmblem(fkind);
    }
    if (sMNCharactersNameGObj != NULL)
    {
        gcEjectGObj(sMNCharactersNameGObj);
        mnCharactersMakeName(fkind);
    }
    if (sMNCharactersStoryGObj != NULL)
    {
        gcEjectGObj(sMNCharactersStoryGObj);
        mnCharactersMakeStory(fkind);
    }
    if (sMNCharactersWorksGObj != NULL)
    {
        gcEjectGObj(sMNCharactersWorksGObj);
        mnCharactersMakeWorks(fkind);
    }
    if (sMNCharactersFighterGObj != NULL)
    {
        ftManagerDestroyFighter(sMNCharactersFighterGObj);
        mnCharactersMakeFighter(fkind);
    }
}

/* mncharacters.c:2421-2438 0x80133754, verbatim. */
void mnCharactersMoveFighterCamera(CObj *cobj, f32 angle, s32 unused)
{
    f32 theta;
    f32 radians;

    (void)unused;

    radians = F_CLC_DTOR32(angle);

    cobj->vec.eye.y = __sinf(radians) * 3000.0F;
    cobj->vec.eye.z = cosf(radians) * 3000.0F;

    theta = syUtilsArcTan2(370.0F, 0.0F) + radians;
    cobj->vec.at.y = __sinf(theta) * 370.0F;
    cobj->vec.at.z = cosf(theta) * 370.0F;

    theta = syUtilsArcTan2(1.0F, 0.0F) + radians;
    cobj->vec.up.y = __sinf(theta);
    cobj->vec.up.z = cosf(theta);
}

/* mncharacters.c:2441-2457 0x80133840, verbatim. */
void mnCharactersResetFighterCamera(void)
{
    CObj *cobj = CObjGetStruct(sMNCharactersFighterCameraGObj);

    cobj->vec.eye.x = 0.0F;
    cobj->vec.eye.y = 0.0F;
    cobj->vec.eye.z = 3000.0F;
    cobj->vec.at.x = 700.0F;
    cobj->vec.at.y = 370.0F;
    cobj->vec.at.z = 0.0F;
    cobj->vec.up.x = 0.0F;
    cobj->vec.up.y = 1.0F;
    cobj->vec.up.z = 0.0F;

    sMNCharactersHeldStickAngle = 0;
    sMNCharactersHeldStickUnknown.f = 0;
}

/* mncharacters.c:2460-2569 0x801338AC, verbatim. */
void mnCharactersUpdateScene(void)
{
    s32 players_z_num;
    s32 stick_range;
    sb32 is_button;

    if (scSubsysControllerGetPlayerTapButtons(B_BUTTON))
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindData;

        mnCharactersBackupFighterKind();
        syTaskmanSetLoadScene();
    }
    players_z_num = scSubsysControllerGetPlayerHoldButtons(Z_TRIG);

    if (players_z_num != 0)
    {
        players_z_num--;

        if (gSYControllerDevices[players_z_num].stick_range.x < -20)
        {
            DObjGetStruct(sMNCharactersFighterGObj)->rotate.vec.f.y -= F_CLC_DTOR32(gSYControllerDevices[players_z_num].stick_range.x / 60.0F);

            if (DObjGetStruct(sMNCharactersFighterGObj)->rotate.vec.f.y > F_CLC_DTOR32(360.0F))
            {
                DObjGetStruct(sMNCharactersFighterGObj)->rotate.vec.f.y -= F_CLC_DTOR32(360.0F);
            }
            sMNCharactersIsAutoRotate = FALSE;
        }
        if (gSYControllerDevices[players_z_num].stick_range.x > 20)
        {
            DObjGetStruct(sMNCharactersFighterGObj)->rotate.vec.f.y -= F_CLC_DTOR32(gSYControllerDevices[players_z_num].stick_range.x / 60.0F);

            if (DObjGetStruct(sMNCharactersFighterGObj)->rotate.vec.f.y > F_CLC_DTOR32(360.0F))
            {
                DObjGetStruct(sMNCharactersFighterGObj)->rotate.vec.f.y -= F_CLC_DTOR32(360.0F);
            }
            sMNCharactersIsAutoRotate = FALSE;
        }
        if ((gSYControllerDevices[players_z_num].stick_range.y > 20) && (sMNCharactersHeldStickAngle < 45.0F))
        {
            sMNCharactersHeldStickAngle += gSYControllerDevices[players_z_num].stick_range.y / 60.0F;

            mnCharactersMoveFighterCamera(CObjGetStruct(sMNCharactersFighterCameraGObj), sMNCharactersHeldStickAngle, sMNCharactersHeldStickUnknown.s);

            sMNCharactersIsAutoRotate = FALSE;
        }

        if ((gSYControllerDevices[players_z_num].stick_range.y < -20) && (sMNCharactersHeldStickAngle > -45.0F))
        {
            sMNCharactersHeldStickAngle += gSYControllerDevices[players_z_num].stick_range.y / 60.0F;

            mnCharactersMoveFighterCamera(CObjGetStruct(sMNCharactersFighterCameraGObj), sMNCharactersHeldStickAngle, sMNCharactersHeldStickUnknown.s);

            sMNCharactersIsAutoRotate = FALSE;
        }
    }
    else
    {
        if
        (
            mnCharactersCheckGetOptionButtonInput(is_button, L_JPAD | L_TRIG | L_CBUTTONS) ||
            mnCharactersCheckGetOptionStickInputLR(stick_range, -20, 0)
        )
        {
            do
            {
                if (sMNCharactersPage == nFTKindPlayableStart)
                {
                    sMNCharactersPage = nFTKindPlayableEnd;
                }
                else sMNCharactersPage--;
            }
            while (mnCharactersCheckHaveFighterKind(mnCharactersGetFighterKind(sMNCharactersPage)) == FALSE);

            mnCharactersChangeFighter(mnCharactersGetFighterKind(sMNCharactersPage));

            mnCharactersSetOptionChangeWaitN(is_button, stick_range, 7);

            mnCharactersResetFighterCamera();
        }
        if
        (
            mnCharactersCheckGetOptionButtonInput(is_button, R_JPAD | R_TRIG | R_CBUTTONS) ||
            mnCharactersCheckGetOptionStickInputLR(stick_range, 20, 1)
        )
        {
            do
            {
                if (sMNCharactersPage == nFTKindPlayableEnd)
                {
                    sMNCharactersPage = nFTKindPlayableStart;
                }
                else sMNCharactersPage++;
            }
            while (mnCharactersCheckHaveFighterKind(mnCharactersGetFighterKind(sMNCharactersPage)) == FALSE);

            mnCharactersChangeFighter(mnCharactersGetFighterKind(sMNCharactersPage));

            mnCharactersSetOptionChangeWaitP(is_button, stick_range, 7);

            mnCharactersResetFighterCamera();
        }
    }
}

/* mncharacters.c:2572-2594 0x80133CB8, verbatim. mnCharactersInitVars sets
 * sMNCharactersIsDemo TRUE when scene_prev is not nSCKindData, which is
 * how How to Play (src/dc/scexplain.c) and the title's attract loop
 * (src/dc/mntitle.c) reach this screen, so it runs, and its 600-tic
 * branch loads nSCKindAutoDemo (src/dc/scautodemo.c). */
void mnCharactersUpdateSceneDemo(void)
{
    if (scSubsysControllerGetPlayerTapButtons(START_BUTTON | A_BUTTON | B_BUTTON))
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindTitle;
        gSCManagerSceneData.is_extend_demo_wait = TRUE;

        syTaskmanSetLoadScene();
    }
    if (sMNCharactersTotalTimeTics == 300)
    {
        sMNCharactersPage = mnCharactersGetPage(sMNCharactersDemoFighterKind1);
        mnCharactersChangeFighter(mnCharactersGetFighterKind(sMNCharactersPage));
    }
    if (sMNCharactersTotalTimeTics == 600)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindAutoDemo;

        syTaskmanSetLoadScene();
    }
}

/* mncharacters.c:2597-2623 0x80133D68, verbatim. */
void mnCharactersFuncRun(GObj *gobj)
{
    (void)gobj;

    sMNCharactersTotalTimeTics++;

    if (sMNCharactersTotalTimeTics >= 10)
    {
        if (sMNCharactersChangeWait != 0)
        {
            sMNCharactersChangeWait--;
        }
        if
        (
            (scSubsysControllerGetPlayerStickInRangeLR(-20, 20) != FALSE)                 &&
            (scSubsysControllerGetPlayerStickInRangeUD(-20, 20) != FALSE)                 &&
            (scSubsysControllerGetPlayerHoldButtons(R_JPAD | R_TRIG | R_CBUTTONS) == FALSE) &&
            (scSubsysControllerGetPlayerHoldButtons(L_JPAD | L_TRIG | L_CBUTTONS) == FALSE)
        )
        {
            sMNCharactersChangeWait = 0;
        }
#ifdef DB_CHARACTERS_TOUR
        mnCharactersTourUpdate();
#endif
        if (sMNCharactersIsDemo == FALSE)
        {
            mnCharactersUpdateScene();
        }
        else mnCharactersUpdateSceneDemo();
    }
}

/* mncharacters.c:1011-1026-style LoadFiles (the port's own, replacing
 * lbRelocInitSetup/lbRelocLoadFilesListed): sprite_bank_load over three
 * of the four relocData files, in dMNCharactersFileIDs order. File 3
 * (the emblem models) has no bank -- mnCharactersMakeEmblem reads its
 * baked packs directly -- so its path is NULL and its slot in
 * sMNCharactersFiles is never set, the same as
 * src/dc/scvsresults.c's mnVSResultsLoadFiles handles its own file 35. */
static void mnCharactersLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMNCharactersFileIDs)] =
    {
        MNCHARACTERS_BANK_MAIN,
        MNCHARACTERS_BANK_DATACOMMON,
        MNCHARACTERS_BANK_EMBLEMS,
        NULL
    };
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(dMNCharactersFileIDs); i++)
    {
        if (paths[i] == NULL)
        {
            continue;
        }
        if (sprite_bank_load(&sMNCharactersBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mnCharacters: no bank for file %d (%s)\n",
                          (int)dMNCharactersFileIDs[i], paths[i]);
            continue;
        }
        sMNCharactersFiles[i] = &sMNCharactersBanks[i];
    }
}

/* mncharacters.c:2626-2677 0x80133E28. DIVERGES: lbRelocInitSetup/
 * lbRelocLoadFilesListed -> mnCharactersLoadFiles (above); the
 * black-clear camera line is cut, the PVR clears its own frame;
 * sMNCharactersFigatreeHeap is never malloc'd (this file's header note)
 * -- the decomp's syTaskmanMalloc(gFTManagerFigatreeHeapSize, 0x10) line
 * is gone with it; and efManagerInitEffects is efManagerLoadEffectBank,
 * the port's loader, exactly as mnVSResultsFuncStart's and
 * mnPlayersVSFuncStart's own calls are (src/dc/scvsresults.c,
 * src/dc/mnplayersvs.c) -- efManagerInitEffects itself walks a
 * relocData layout the port does not read this way. */
void mnCharactersFuncStart(void)
{
    s32 i;

    mnCharactersLoadFiles();
    gcMakeGObjSPAfter(0, mnCharactersFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
    efParticleInitAll();
    mnCharactersInitVars();
    efManagerLoadEffectBank();
    ftManagerAllocFighter(FTDATA_FLAG_MAINMOTION | FTDATA_FLAG_SUBMOTION, 1);
    /* the port's: the motions this screen tours, and no more -- tier 1
     * of each fighter's .anm (ftcommon.h ftManagerSetAnmTier) */
    ftManagerSetAnmTier(FIGHTER_ANM_TIER_CHARACTERS);

    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        ftManagerSetupFilesAllKind(i);
    }

    mnCharactersMakeStoryCamera();
    mnCharactersMakeDecalsCamera();
    mnCharactersMakeEmblemCamera();
    mnCharactersMakeNameCamera();
    mnCharactersMakeWorksWallpaperCamera();
    mnCharactersMakeWorksCamera();
    mnCharactersMakeFighterCamera();
    mnCharactersMakeDecals();
    mnCharactersMakeWorksWallpaper();
    mnCharactersMakeWorks(mnCharactersGetFighterKind(sMNCharactersPage));
    mnCharactersMakeStory(mnCharactersGetFighterKind(sMNCharactersPage));
    mnCharactersMakeEmblem(mnCharactersGetFighterKind(sMNCharactersPage));
    mnCharactersMakeName(mnCharactersGetFighterKind(sMNCharactersPage));
    mnCharactersMakeMotionName();
    mnCharactersMakeFighter(mnCharactersGetFighterKind(sMNCharactersPage));

    scSubsysFighterSetLightParams(45.0F, 10.0F, 0xFF, 0xFF, 0xFF, 0xFF);

    if (gSCManagerSceneData.scene_prev == nSCKindData)
    {
        syAudioPlayBGM(0, nSYAudioBGMData);
    }
}

/* mncharacters.c:2680 dMNCharactersVideoSetup is the N64's video mode:
 * see mnCharactersStartScene. */

/* mncharacters.c:2683-2725 0x80136518, pool counts included: the
 * decomp's own copy carries zero for every one of them, the same as
 * dMNPlayersVSTaskmanSetup's (src/dc/mnplayersvs.c) -- "the scene takes
 * its objects, and its threads' stacks, from the scene heap" rather
 * than a fixed pool, and that already carries a real fighter plus
 * cameras and sprites, so this screen's one fighter does too. DIVERGES
 * as the other menus': the pre-render function is NULL, not
 * mnCharactersFuncLights (that function's own header note says why);
 * the matrix function list is NULL too -- dLBCommonFuncMatrixList
 * is not itself a ported symbol (src/dc/scvsbattle.c's own
 * dSCVSBattleTaskmanSetup says why: it waits for the matrix stack,
 * sys/matrix.c); and the four DL buffer sizes, the graphics heap size
 * and the RDP output buffer size are all zero, not the decomp's
 * sizeof(Gfx)*3072/512, 0x8000 and 0xC000 -- there is no RSP and no RDP
 * here, the same cut dSCVSBattleTaskmanSetup's, dMNPlayersVSTaskmanSetup's
 * and dMNVSResultsTaskmanSetup's own literals already make, all three
 * of them scenes with real fighters on screen too. */
SYTaskmanSetup dMNCharactersTaskmanSetup =
{
    {
        0,
        gcRunAll,
        scManagerFuncDraw,
        NULL,
        0,
        1,
        2,
        0, 0, 0, 0,
        0,
        2,
        0,
        NULL,
        syControllerFuncRead,
    },

    0,
    sizeof(u64) * 192,
    0,
    0,
    0,
    0,
    sizeof(GObj),
    0,
    NULL,
    NULL,
    0,
    0,
    0,
    sizeof(DObj),
    0,
    sizeof(SObj),
    0,
    sizeof(CObj),

    mnCharactersFuncStart
};

/* mncharacters.c:2728-2735 0x80134050. DIVERGES as the other menus':
 * syVideoInit and the zbuffer are the N64's video mode, set once at boot,
 * and the arena_size line is the link map (&ovl1_VRAM - &ovl33_BSS_END,
 * addresses the port has none of). What is left is the last line. */
void mnCharactersStartScene(void)
{
	scManagerFuncUpdate(&dMNCharactersTaskmanSetup);
}

/* the port's own: every OVERLAY_CLEAR the reload has to cover, since
 * dSCManagerOverlays[33] is linked in rather than DMA'd -- see
 * src/dc/overlay.h. */
void mnCharactersOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNCharactersPage);
    OVERLAY_CLEAR(sMNCharactersEmblemGObj);
    OVERLAY_CLEAR(sMNCharactersNameGObj);
    OVERLAY_CLEAR(sMNCharactersStoryGObj);
    OVERLAY_CLEAR(sMNCharactersWorksGObj);
    OVERLAY_CLEAR(sMNCharactersFighterGObj);
    OVERLAY_CLEAR(sMNCharactersMotionKind);
    OVERLAY_CLEAR(sMNCharactersAnimFramesRemain);
    OVERLAY_CLEAR(sMNCharactersIsUseAnimFramesRemain);
    OVERLAY_CLEAR(sMNCharactersIsAutoRotate);
    OVERLAY_CLEAR(sMNCharactersUnknown);
    OVERLAY_CLEAR(sMNCharactersMotionNameGObj);
    OVERLAY_CLEAR(sMNCharactersFighterCameraGObj);
    OVERLAY_CLEAR(sMNCharactersHeldStickAngle);
    OVERLAY_CLEAR(sMNCharactersHeldStickUnknown);
    OVERLAY_CLEAR(sMNCharactersFigatreeHeap);
    OVERLAY_CLEAR(sMNCharactersIsDemo);
    OVERLAY_CLEAR(sMNCharactersDemoFighterKind0);
    OVERLAY_CLEAR(sMNCharactersDemoFighterKind1);
    OVERLAY_CLEAR(sMNCharactersCurrentMotionTrack);
    OVERLAY_CLEAR(sMNCharactersFighterMask);
    OVERLAY_CLEAR(sMNCharactersCurrentAnimFrame);
    OVERLAY_CLEAR(sMNCharactersRecentMotionKinds);
    OVERLAY_CLEAR(sMNCharactersRecentMotionKindsID);
    OVERLAY_CLEAR(sMNCharactersChangeWait);
    OVERLAY_CLEAR(sMNCharactersTotalTimeTics);
    OVERLAY_CLEAR(sMNCharactersFiles);
    OVERLAY_CLEAR(sMNCharactersBanks);
    OVERLAY_CLEAR(sMNCharactersEmblemModel);
}
