/* scmanagerdata.c -- sc/scmanager.c's three .data tables, verbatim.
 *
 * dSCManagerDefaultBackupData (scmanager.c:135), dSCManagerDefaultSceneData
 * (:487) and dSCManagerDefaultBattleState (:564): the save data, the scene
 * data and the battle state the game starts every one of gSCManagerBackupData,
 * gSCManagerSceneData, gSCManagerTransferBattleState and
 * gSCManagerVSBattleState from. src/dc/scmanager.c's scManagerInitData is
 * what installs them, and lb/lbbackup.c -- compiled unmodified out of the
 * decomp -- reads all three by name: it is the file that decides, on a save
 * whose checksum does not hold, that the defaults are what the player gets.
 *
 * They are here rather than in src/dc/scmanager.c so that this file can be
 * compiled on its own against nothing but headers, which is what
 * tools/check/backup_oracle.c does. That oracle and tools/check/backup_check.py
 * read the ROM's own bytes at dSCManagerDefaultBackupData and check what the
 * port compiled against them field by field, so a value mistyped here, or
 * a field the SH-4 lays out differently from the MIPS, fails the build.
 *
 * A 1P-game default is as much the game's as a VS one, so both are here:
 * dSCManagerDefaultSceneData carries the 1P port, ally kinds and time
 * limit, and dSCManagerDefaultBattleState the demo game type these globals
 * hold before any menu has run. The 1P game reads them, and
 * lbBackupCorrectErrors reads two of them (lbbackup.c:109, :119).
 */
#include <sc/scene.h>
#include <ft/ftdef.h>
#include <gr/grdef.h>
#include <macros.h>

// 0x800A3994
LBBackupData dSCManagerDefaultBackupData =
{ 
	// VS Records
	{
		// Mario VS Records
		{
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// KO count on each character
			0,										// Time used
			0,										// Damage dealt
			0,										// Damage taken
			0,										// ???
			0,										// Self-destructs
			0,										// Games played with this character present
			0,										// Avg. player count of this character in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// Avg. player count with other characters in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }	// Number of times played against other characters
		},

		// Fox VS Records
		{
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// KO count on each character
			0,										// Time used
			0,										// Damage dealt
			0,										// Damage taken
			0,										// ???
			0,										// Self-destructs
			0,										// Games played with this character present
			0,										// Avg. player count of this character in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// Avg. player count with other characters in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }	// Number of times played against other characters
		},

		// Donkey Kong VS Records
		{
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// KO count on each character
			0,										// Time used
			0,										// Damage dealt
			0,										// Damage taken
			0,										// ???
			0,										// Self-destructs
			0,										// Games played with this character present
			0,										// Avg. player count of this character in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// Avg. player count with other characters in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }	// Number of times played against other characters
		},

		// Samus VS Records
		{
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// KO count on each character
			0,										// Time used
			0,										// Damage dealt
			0,										// Damage taken
			0,										// ???
			0,										// Self-destructs
			0,										// Games played with this character present
			0,										// Avg. player count of this character in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// Avg. player count with other characters in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }	// Number of times played against other characters
		},

		// Luigi VS Records
		{
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// KO count on each character
			0,										// Time used
			0,										// Damage dealt
			0,										// Damage taken
			0,										// ???
			0,										// Self-destructs
			0,										// Games played with this character present
			0,										// Avg. player count of this character in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// Avg. player count with other characters in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }	// Number of times played against other characters
		},

		// Link VS Records
		{
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// KO count on each character
			0,										// Time used
			0,										// Damage dealt
			0,										// Damage taken
			0,										// ???
			0,										// Self-destructs
			0,										// Games played with this character present
			0,										// Avg. player count of this character in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// Avg. player count with other characters in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }	// Number of times played against other characters
		},

		// Yoshi VS Records
		{
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// KO count on each character
			0,										// Time used
			0,										// Damage dealt
			0,										// Damage taken
			0,										// ???
			0,										// Self-destructs
			0,										// Games played with this character present
			0,										// Avg. player count of this character in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// Avg. player count with other characters in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }	// Number of times played against other characters
		},

		// Captain Falcon VS Records
		{
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// KO count on each character
			0,										// Time used
			0,										// Damage dealt
			0,										// Damage taken
			0,										// ???
			0,										// Self-destructs
			0,										// Games played with this character present
			0,										// Avg. player count of this character in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// Avg. player count with other characters in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }	// Number of times played against other characters
		},

		// Kirby VS Records
		{
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// KO count on each character
			0,										// Time used
			0,										// Damage dealt
			0,										// Damage taken
			0,										// ???
			0,										// Self-destructs
			0,										// Games played with this character present
			0,										// Avg. player count of this character in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// Avg. player count with other characters in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }	// Number of times played against other characters
		},

		// Pikachu VS Records
		{
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// KO count on each character
			0,										// Time used
			0,										// Damage dealt
			0,										// Damage taken
			0,										// ???
			0,										// Self-destructs
			0,										// Games played with this character present
			0,										// Avg. player count of this character in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// Avg. player count with other characters in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }	// Number of times played against other characters
		},

		// Jigglypuff VS Records
		{
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// KO count on each character
			0,										// Time used
			0,										// Damage dealt
			0,										// Damage taken
			0,										// ???
			0,										// Self-destructs
			0,										// Games played with this character present
			0,										// Avg. player count of this character in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// Avg. player count with other characters in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }	// Number of times played against other characters
		},

		// Ness VS Records
		{
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// KO count on each character
			0,										// Time used
			0,										// Damage dealt
			0,										// Damage taken
			0,										// ???
			0,										// Self-destructs
			0,										// Games played with this character present
			0,										// Avg. player count of this character in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	// Avg. player count with other characters in-game
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }	// Number of times played against other characters
		}
	},

	TRUE,											// Are screen flashes allowed?
	1, 												// 0 = Mono, 1 = Stereo
	0,												// Vertical screen adjust offset
	0,												// Horizontal screen adjust offset
	nFTKindMario,									// Last character viewed on Character Data menu
	0,												// Mask of unlocked features
	0,												// Mask of available characters
	nSC1PGameDifficultyEasy,						// Last 1P Game difficulty setting
	2,												// Last 1P Game stocks setting

	// 1P Records
	{
		// Mario 1P Records
		{
			0,										// 1P Game High-Score
			0,										// 1P Game number of continues used
			0,										// 1P Game humber of bonuses earned
			0,										// 1P Game highest difficulty cleared
			I_MIN_TO_TICS(60),						// Break the Targets best time
			0,										// Break the Targets number of targets broken
			I_MIN_TO_TICS(60),						// Board the Platforms best time
			0,										// Board the Platforms number of platforms boarded
			FALSE									// Has this character cleared 1P Game?
		},

		// Fox 1P Records
		{
			0,										// 1P Game High-Score
			0,										// 1P Game number of continues used
			0,										// 1P Game humber of bonuses earned
			0,										// 1P Game highest difficulty cleared
			I_MIN_TO_TICS(60),						// Break the Targets best time
			0,										// Break the Targets number of targets broken
			I_MIN_TO_TICS(60),						// Board the Platforms best time
			0,										// Board the Platforms number of platforms boarded
			FALSE									// Has this character cleared 1P Game?
		},

		// Donkey Kong 1P Records
		{
			0,										// 1P Game High-Score
			0,										// 1P Game number of continues used
			0,										// 1P Game humber of bonuses earned
			0,										// 1P Game highest difficulty cleared
			I_MIN_TO_TICS(60),						// Break the Targets best time
			0,										// Break the Targets number of targets broken
			I_MIN_TO_TICS(60),						// Board the Platforms best time
			0,										// Board the Platforms number of platforms boarded
			FALSE									// Has this character cleared 1P Game?
		},

		// Samus 1P Records
		{
			0,										// 1P Game High-Score
			0,										// 1P Game number of continues used
			0,										// 1P Game humber of bonuses earned
			0,										// 1P Game highest difficulty cleared
			I_MIN_TO_TICS(60),						// Break the Targets best time
			0,										// Break the Targets number of targets broken
			I_MIN_TO_TICS(60),						// Board the Platforms best time
			0,										// Board the Platforms number of platforms boarded
			FALSE									// Has this character cleared 1P Game?
		},

		// Luigi 1P Records
		{
			0,										// 1P Game High-Score
			0,										// 1P Game number of continues used
			0,										// 1P Game humber of bonuses earned
			0,										// 1P Game highest difficulty cleared
			I_MIN_TO_TICS(60),						// Break the Targets best time
			0,										// Break the Targets number of targets broken
			I_MIN_TO_TICS(60),						// Board the Platforms best time
			0,										// Board the Platforms number of platforms boarded
			FALSE									// Has this character cleared 1P Game?
		},

		// Link 1P Records
		{
			0,										// 1P Game High-Score
			0,										// 1P Game number of continues used
			0,										// 1P Game humber of bonuses earned
			0,										// 1P Game highest difficulty cleared
			I_MIN_TO_TICS(60),						// Break the Targets best time
			0,										// Break the Targets number of targets broken
			I_MIN_TO_TICS(60),						// Board the Platforms best time
			0,										// Board the Platforms number of platforms boarded
			FALSE									// Has this character cleared 1P Game?
		},

		// Yoshi 1P Records
		{
			0,										// 1P Game High-Score
			0,										// 1P Game number of continues used
			0,										// 1P Game humber of bonuses earned
			0,										// 1P Game highest difficulty cleared
			I_MIN_TO_TICS(60),						// Break the Targets best time
			0,										// Break the Targets number of targets broken
			I_MIN_TO_TICS(60),						// Board the Platforms best time
			0,										// Board the Platforms number of platforms boarded
			FALSE									// Has this character cleared 1P Game?
		},

		// Captain Falcon 1P Records
		{
			0,										// 1P Game High-Score
			0,										// 1P Game number of continues used
			0,										// 1P Game humber of bonuses earned
			0,										// 1P Game highest difficulty cleared
			I_MIN_TO_TICS(60),						// Break the Targets best time
			0,										// Break the Targets number of targets broken
			I_MIN_TO_TICS(60),						// Board the Platforms best time
			0,										// Board the Platforms number of platforms boarded
			FALSE									// Has this character cleared 1P Game?
		},

		// Kirby 1P Records
		{
			0,										// 1P Game High-Score
			0,										// 1P Game number of continues used
			0,										// 1P Game humber of bonuses earned
			0,										// 1P Game highest difficulty cleared
			I_MIN_TO_TICS(60),						// Break the Targets best time
			0,										// Break the Targets number of targets broken
			I_MIN_TO_TICS(60),						// Board the Platforms best time
			0,										// Board the Platforms number of platforms boarded
			FALSE									// Has this character cleared 1P Game?
		},

		// Pikachu 1P Records
		{
			0,										// 1P Game High-Score
			0,										// 1P Game number of continues used
			0,										// 1P Game humber of bonuses earned
			0,										// 1P Game highest difficulty cleared
			I_MIN_TO_TICS(60),						// Break the Targets best time
			0,										// Break the Targets number of targets broken
			I_MIN_TO_TICS(60),						// Board the Platforms best time
			0,										// Board the Platforms number of platforms boarded
			FALSE									// Has this character cleared 1P Game?
		},

		// Jigglypuff 1P Records
		{
			0,										// 1P Game High-Score
			0,										// 1P Game number of continues used
			0,										// 1P Game humber of bonuses earned
			0,										// 1P Game highest difficulty cleared
			I_MIN_TO_TICS(60),						// Break the Targets best time
			0,										// Break the Targets number of targets broken
			I_MIN_TO_TICS(60),						// Board the Platforms best time
			0,										// Board the Platforms number of platforms boarded
			FALSE									// Has this character cleared 1P Game?
		},

		// Ness 1P Records
		{
			0,										// 1P Game High-Score
			0,										// 1P Game number of continues used
			0,										// 1P Game humber of bonuses earned
			0,										// 1P Game highest difficulty cleared
			I_MIN_TO_TICS(60),						// Break the Targets best time
			0,										// Break the Targets number of targets broken
			I_MIN_TO_TICS(60),						// Board the Platforms best time
			0,										// Board the Platforms number of platforms boarded
			FALSE									// Has this character cleared 1P Game?
		}
	},

	0,												// Mask of unique stages played in VS Mode (for Mushroom Kingdom)
	0,												// Number of games played in VS Mode to unlock Item Switch
	0,												// Total number of games played in VS Mode
	0,												// Anti-Piracy measures mask; this is where the "penalties" are stored
	0,												// Boot count
	0x29A,											// Signature?
	0												// Checksum of all previous save data struct members' values
};

// 0x800A3F80
SCCommonData dSCManagerDefaultSceneData =
{
#if defined(REGION_US)
	nSCKindStartup,										// Current scene
	nSCKindStartup,										// Previous scene
#else
	nSCKindOpeningRoom,									// Current scene
	nSCKindOpeningRoom,									// Previous scene
#endif
	// Queued unlock messages
	{
		nLBBackupUnlockEnumCount,
		nLBBackupUnlockEnumCount,
		nLBBackupUnlockEnumCount,
		nLBBackupUnlockEnumCount,
		nLBBackupUnlockEnumCount,
		nLBBackupUnlockEnumCount,
		nLBBackupUnlockEnumCount
	},

	nFTKindLuigi,									// Challanger approaching character
	0,												// Mask of previously demo'd characters
	nFTKindNull,									// First demo character?

	// Demo characters
	{
		nFTKindNull,
		nFTKindNull
	},

	nGRKindCastle,									// Stage selected

	FALSE,											// Is Sudden Death?
	FALSE,											// Has "continue" been selected?
	FALSE,											// Has A + B + Z + R been input?

	0,												// 1P Game player's port
	nFTKindNull,									// 1P Game player's character
	0,												// 1P Game player's costume
	5,												// 1P Game time limit (in seconds, 100 = infinite)
	0,												// 1P Game current stage
	
	// 1P Game ally characters
	{
		nFTKindMario,
		nFTKindMario
	},

	0,												// 1P Game time remaining (in seconds)
	0,												// 1P Game score
	0,												// 1P Game continues used
	0,												// 1P Game total bonuses acquired
	
	// Bonus masks
	{
		0,
		0,
		0
	},

	0,												// Bonus stage tasks completed
	nFTKindNull,									// Bonus stage character
	0,												// Bonus stage costume

	nFTKindNull,									// Training mode player's character
	0,												// Training mode player's costume
	nFTKindNull,									// Training mode player's character
	0,												// Training mode player's costume
	TRUE,											// Extend time until auto-demo starts?
	0,												// Auto-demo current stage order index
	nGRKindCastle,									// VS Mode stage selected
	nGRKindCastle,									// Training Mode stage selected
	0,												// Levels to subtract from challenger's CPU level
	FALSE											// Has the title screen animation been viewed?
};

// 0x800A3FC8
SCBattleState dSCManagerDefaultBattleState =
{
	nSCBattleGameTypeDemo,							// Game type
	nGRKindCastle,									// Stage
	FALSE,											// Is team battle?
	SCBATTLE_GAMERULE_TIME,							// Game rule
	0,												// Total players in-game
	0,												// Total CPUs in-game
	3,												// Time limit (in seconds)
	2,												// Stocks
	nSCBattleHandicapOff,							// Handicap setting
	FALSE,											// Is team attack enabled?
	TRUE,											// Is stage select enabled?
	100,											// Damage ratio
	~0,												// Item Switch mask
	TRUE,											// Reset players when first entering VS Mode character select screen?
	nSCBattleGameStatusWait,						// Status of current match
	0,												// Time remaining (in tics)
	0,												// Time passed (in tics)
	nSCBattleItemSwitchMiddle,						// Item appearance rate
	TRUE,											// Display score?
	0,												// Shadow colors based on team or use default black?

	// Player Data
	{
		// Player 1
		{
			3,										// CPU level
			9,										// Handicap
			nFTPlayerKindNot,						// Player type
			nFTKindNull,							// Character
			0,										// Team
			0,										// Port
			0,										// Costume
			0,										// Shade
			0,										// Color
			TRUE,									// Is permanent stock icon?
			0,										// Player tag
			0,										// Stock count
			FALSE,									// Is a "VS [Character] Team" member?
			0,										// Placement
			0,										// Falls
			0,										// Score

			// Total number of KOs scored on each player
			{
				0, 0, 0, 0
			},

			0,										// ???
			0,										// ???

			0,										// Total number of self-destructs
			0,										// Total damage dealt
			0,										// Total damage taken from all sources

			// Total damage taken from each player
			{
				0, 0, 0, 0
			},

			0,										// Total damage taken on current stock
			0,										// Combo damage taken from foes
			0,										// Combo hits landed by foes
			NULL,									// Pointer to fighter GObj
			0,										// Current position in stale moves queue
			
			// Stale moves info
			{
				{ 0, 0 },
				{ 0, 0 },
				{ 0, 0 },
				{ 0, 0 },
				{ 0, 0 }
			}
		},

		// Player 2
		{
			3,										// CPU level
			9,										// Handicap
			nFTPlayerKindNot,						// Player type
			nFTKindNull,							// Character
			0,										// Team
			0,										// Port
			0,										// Costume
			0,										// Shade
			0,										// Color
			TRUE,									// Permanent stock icon?
			1,										// Player tag
			0,										// Stock count
			FALSE,									// Is a "VS [Character] Team" member?
			0,										// Placement
			0,										// Falls
			0,										// Score

			// Total number of KOs scored on each player
			{
				0, 0, 0, 0
			},

			0,										// ???
			0,										// ???

			0,										// Total number of self-destructs
			0,										// Total damage dealt
			0,										// Total damage taken from all sources

			// Total damage taken from each player
			{
				0, 0, 0, 0
			},

			0,										// Total damage taken on current stock
			0,										// Combo damage taken from foes
			0,										// Combo hits landed by foes
			NULL,									// Pointer to fighter GObj
			0,										// Current position in stale moves queue
			
			// Stale moves info
			{
				{ 0, 0 },
				{ 0, 0 },
				{ 0, 0 },
				{ 0, 0 },
				{ 0, 0 }
			}
		},

		// Player 3
		{
			3,										// CPU level
			9,										// Handicap
			nFTPlayerKindNot,						// Player type
			nFTKindNull,							// Character
			1,										// Team
			0,										// Port
			0,										// Costume
			0,										// Shade
			0,										// Color
			TRUE,									// Permanent stock icon?
			2,										// Player tag
			0,										// Stock count
			FALSE,									// Is a "VS [Character] Team" member?
			0,										// Placement
			0,										// Falls
			0,										// Score

			// Total number of KOs scored on each player
			{
				0, 0, 0, 0
			},

			0,										// ???
			0,										// ???

			0,										// Total number of self-destructs
			0,										// Total damage dealt
			0,										// Total damage taken from all sources

			// Total damage taken from each player
			{
				0, 0, 0, 0
			},

			0,										// Total damage taken on current stock
			0,										// Combo damage taken from foes
			0,										// Combo hits landed by foes
			NULL,									// Pointer to fighter GObj
			0,										// Current position in stale moves queue
			
			// Stale moves info
			{
				{ 0, 0 },
				{ 0, 0 },
				{ 0, 0 },
				{ 0, 0 },
				{ 0, 0 }
			}
		},

		// Player 4
		{
			3,										// CPU level
			9,										// Handicap
			nFTPlayerKindNot,						// Player type
			nFTKindNull,							// Character
			1,										// Team
			0,										// Port
			0,										// Costume
			0,										// Shade
			0,										// Color
			TRUE,									// Permanent stock icon?
			3,										// Player tag
			0,										// Stock count
			FALSE,									// Is a "VS [Character] Team" member?
			0,										// Placement
			0,										// Falls
			0,										// Score

			// Total number of KOs scored on each player
			{
				0, 0, 0, 0
			},

			0,										// ???
			0,										// ???

			0,										// Total number of self-destructs
			0,										// Total damage dealt
			0,										// Total damage taken from all sources

			// Total damage taken from each player
			{
				0, 0, 0, 0
			},

			0,										// Total damage taken on current stock
			0,										// Combo damage taken from foes
			0,										// Combo hits landed by foes
			NULL,									// Pointer to fighter GObj
			0,										// Current position in stale moves queue
			
			// Stale moves info
			{
				{ 0, 0 },
				{ 0, 0 },
				{ 0, 0 },
				{ 0, 0 },
				{ 0, 0 }
			}
		}
	}
};
