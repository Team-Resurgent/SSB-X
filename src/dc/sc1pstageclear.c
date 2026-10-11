/* sc1pstageclear.c -- see sc1pstageclear.h. Every function is
 * sc/sc1pmode/sc1pstageclear.c's by name and body unless marked
 * DIVERGES; the line numbers are the decomp's. */
#include "sc1pstageclear.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"
#include "ftcommon.h"
#include "bgm.h"
#include "fighter.h"
#include "objmodel.h"

#include <sys/debug.h>
#include <sys/controller.h>
#include <sys/rdp.h>
#include <sys/utils.h>
#include <sys/objdisplay.h>
#include <sc/scdef.h>
#include <sc/scsubsys/scsubsys.h>
#include <sc/sc1pmode/sc1pgame.h>
#include <gm/gmsound.h>
#include <lb/lbdef.h>
#include <PR/os.h>
#include <string.h>

#include "taskman.h"              /* syTaskmanGetPhoto */
#include <config.h>               /* GS_SCREEN_WIDTH_DEFAULT */

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/sysshim.c defines it over the FGM engine */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* sc/sc1pmode/sc1pstageclear.h's own declarations, of the two functions
 * this file calls before it defines them. The decomp has that header;
 * the port's sc1pstageclear.h is the scene's, not the module's. */
sb32 sc1PStageClearCheckNoTimer(void);
void sc1PStageClearMakeScoreSObjs(void);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset, as src/dc/sc1pintro.c and every other ported scene do it.
 * Each `&llXxxSprite` is written `llXxxSprite`, the number. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* the decomp's share of unused locals */
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

/* The six banks, in dSC1PStageClearFileIDs order. The last four are
 * already on the disc: the three digit sets are the battle HUD's
 * (Makefile HUD_BANKS) and the two icons are this scene's own. */
#define SC1PSTAGECLEAR_BANK_TEXT     "sc1pstageclear1.spr"
#define SC1PSTAGECLEAR_BANK_SCORE    "sc1pstageclear2.spr"
#define SC1PSTAGECLEAR_BANK_DAMAGE   "ifdamage.spr"
#define SC1PSTAGECLEAR_BANK_TIMER    "iftimer.spr"
#define SC1PSTAGECLEAR_BANK_DIGITS   "ifdigits.spr"
#define SC1PSTAGECLEAR_BANK_OBJECTS  "sc1pstageclear3.spr"

/* The sprite offsets, from src/dc/decomp/reloc_data.us.h -- the values
 * the decomp's own ll<Name>Sprite link labels carry. The fifty-seven
 * bonus names in dSC1PStageClearBonusData are most of them. */
#define llGRWallpaperTrainingBlackSprite                     0x20718
#define llIFCommonDigits0Sprite                              0x68
#define llIFCommonDigits1Sprite                              0x118
#define llIFCommonDigits2Sprite                              0x1c8
#define llIFCommonDigits3Sprite                              0x278
#define llIFCommonDigits4Sprite                              0x328
#define llIFCommonDigits5Sprite                              0x3d8
#define llIFCommonDigits6Sprite                              0x488
#define llIFCommonDigits7Sprite                              0x538
#define llIFCommonDigits8Sprite                              0x5e8
#define llIFCommonDigits9Sprite                              0x698
#define llIFCommonDigitsColonSprite                          0x8d8
#define llIFCommonDigitsCrossSprite                          0x828
#define llIFCommonDigitsDashSprite                           0x710
#define llIFCommonPlayerDamageDigit0Sprite                   0x148
#define llIFCommonPlayerDamageDigit1Sprite                   0x2d8
#define llIFCommonPlayerDamageDigit2Sprite                   0x500
#define llIFCommonPlayerDamageDigit3Sprite                   0x698
#define llIFCommonPlayerDamageDigit4Sprite                   0x8c0
#define llIFCommonPlayerDamageDigit5Sprite                   0xa58
#define llIFCommonPlayerDamageDigit6Sprite                   0xc80
#define llIFCommonPlayerDamageDigit7Sprite                   0xe18
#define llIFCommonPlayerDamageDigit8Sprite                   0x1040
#define llIFCommonPlayerDamageDigit9Sprite                   0x1270
#define llIFCommonTimerSymbolCrossSprite                     0x1018
#define llSC1PStageClear1AcidClearTextSprite                 0x118a8
#define llSC1PStageClear1AerialTextSprite                    0x123e8
#define llSC1PStageClear1AllVariationsTextSprite             0xe428
#define llSC1PStageClear1ArwingClearTextSprite               0x11e48
#define llSC1PStageClear1BonusBorderSprite                   0xa4b8
#define llSC1PStageClear1BonusPageArrowSprite                0xb6a8
#define llSC1PStageClear1BonusTextSprite                     0xd340
#define llSC1PStageClear1BoobyTrapTextSprite                 0x11128
#define llSC1PStageClear1BrosCalamityTextSprite              0x12f28
#define llSC1PStageClear1BumperClearTextSprite               0x11a88
#define llSC1PStageClear1CheapShotTextSprite                 0xd528
#define llSC1PStageClear1ClearTextSprite                     0x1d58
#define llSC1PStageClear1ColonTextSprite                     0x2120
#define llSC1PStageClear1CometMysticTextSprite               0x116c8
#define llSC1PStageClear1CounterAttackTextSprite             0x12028
#define llSC1PStageClear1DamageTextSprite                    0x2b48
#define llSC1PStageClear1DKDefenderTextSprite                0x13108
#define llSC1PStageClear1DKPerfectTextSprite                 0x132e8
#define llSC1PStageClear1DoubleKOTextSprite                  0xe7e8
#define llSC1PStageClear1EasyClearTextSprite                 0x141e8
#define llSC1PStageClear1FighterStanceTextSprite             0x11308
#define llSC1PStageClear1FullPowerTextSprite                 0xfc88
#define llSC1PStageClear1GameTextSprite                      0x1338
#define llSC1PStageClear1GiantImpactTextSprite               0xeba8
#define llSC1PStageClear1GoodFriendTextSprite                0x134c8
#define llSC1PStageClear1HardClearTextSprite                 0x145a8
#define llSC1PStageClear1HawkTextSprite                      0xde88
#define llSC1PStageClear1HeartThrobTextSprite                0x10408
#define llSC1PStageClear1HeavyDamageTextSprite               0xe248
#define llSC1PStageClear1ItemStrikeTextSprite                0xe608
#define llSC1PStageClear1ItemThrowTextSprite                 0xef68
#define llSC1PStageClear1JackpotTextSprite                   0x12988
#define llSC1PStageClear1JudoWarriorTextSprite               0xdca8
#define llSC1PStageClear1KirbyRanksTextSprite                0x12d48
#define llSC1PStageClear1LastChanceTextSprite                0xf328
#define llSC1PStageClear1LastSecondTextSprite                0x125c8
#define llSC1PStageClear1Lucky3TextSprite                    0x127a8
#define llSC1PStageClear1MeteorSmashTextSprite               0x12208
#define llSC1PStageClear1MewCatcherTextSprite                0xfe68
#define llSC1PStageClear1MysticTextSprite                    0x114e8
#define llSC1PStageClear1NoDamageClearTextSprite             0x13a68
#define llSC1PStageClear1NoDamageTextSprite                  0xfaa8
#define llSC1PStageClear1NoItemTextSprite                    0xd8e8
#define llSC1PStageClear1NoMissClearTextSprite               0x13888
#define llSC1PStageClear1NoMissTextSprite                    0xf8c8
#define llSC1PStageClear1NormalClearTextSprite               0x143c8
#define llSC1PStageClear1PacifistTextSprite                  0xf508
#define llSC1PStageClear1PerfectTextSprite                   0xf6e8
#define llSC1PStageClear1PokemonFinishTextSprite             0x10f48
#define llSC1PStageClear1ResultTextSprite                    0xaf98
#define llSC1PStageClear1ShieldBreakerTextSprite             0xdac8
#define llSC1PStageClear1ShooterTextSprite                   0xe068
#define llSC1PStageClear1SingleMoveTextSprite                0x10d68
#define llSC1PStageClear1SmashlessTextSprite                 0x109a8
#define llSC1PStageClear1SmashManiaTextSprite                0x107c8
#define llSC1PStageClear1SpecialBonusTextSprite              0x2060
#define llSC1PStageClear1SpecialMoveTextSprite               0x10b88
#define llSC1PStageClear1SpeedDemonTextSprite                0x13e28
#define llSC1PStageClear1SpeedKingTextSprite                 0x13c48
#define llSC1PStageClear1SpeedsterTextSprite                 0xed88
#define llSC1PStageClear1StageTextSprite                     0x9d8
#define llSC1PStageClear1StarClearTextSprite                 0x10048
#define llSC1PStageClear1StarFinishTextSprite                0xd708
#define llSC1PStageClear1TargetTextSprite                    0xb4f8
#define llSC1PStageClear1TextShadowSprite                    0xd1c8
#define llSC1PStageClear1ThrowDownTextSprite                 0x105e8
#define llSC1PStageClear1TimerDamageDigit0Sprite             0xb808
#define llSC1PStageClear1TimerDamageDigit1Sprite             0xb968
#define llSC1PStageClear1TimerDamageDigit2Sprite             0xbac8
#define llSC1PStageClear1TimerDamageDigit3Sprite             0xbc28
#define llSC1PStageClear1TimerDamageDigit4Sprite             0xbd88
#define llSC1PStageClear1TimerDamageDigit5Sprite             0xbee8
#define llSC1PStageClear1TimerDamageDigit6Sprite             0xc048
#define llSC1PStageClear1TimerDamageDigit7Sprite             0xc1a8
#define llSC1PStageClear1TimerDamageDigit8Sprite             0xc308
#define llSC1PStageClear1TimerDamageDigit9Sprite             0xc468
#define llSC1PStageClear1TimerTextSprite                     0x25e8
#define llSC1PStageClear1TornadoClearTextSprite              0x11c68
#define llSC1PStageClear1TricksterTextSprite                 0xe9c8
#define llSC1PStageClear1TripleKOTextSprite                  0xf148
#define llSC1PStageClear1TrueFriendTextSprite                0x136a8
#define llSC1PStageClear1VegetarianTextSprite                0x10228
#define llSC1PStageClear1VeryEasyClearTextSprite             0x14008
#define llSC1PStageClear1VeryHardClearTextSprite             0x14788
#define llSC1PStageClear1YoshiRainbowTextSprite              0x12b68
#define llSC1PStageClear2ScoreTextSprite                     0x408
#define llSC1PStageClear3PlatformSprite                      0xc0
#define llSC1PStageClear3TargetSprite                        0x1d0

/* ---- the pools --------------------------------------------------------
 *
 * The decomp's dGM1PStageClearTaskmanSetup carries zero for every count
 * (the real numbers are in the ROM's data, not in the decomp), so these
 * are the port's. The screen is sprites and nothing else: no fighter,
 * no stage, no model, so XObjs, AObjs, MObjs and DObjs are only what
 * the two cameras and the GObj machinery want.
 *
 * SObjs is the number that matters, and the worst case is the bonus
 * table: ten bonus rows at once, each up to thirteen SObjs (a name, a
 * colon and up to six digits, and the No Miss row adds a cross and two
 * more digits), plus the eight-digit score, the header text, the timer
 * and damage rows and the ten bonus-objective icons. 256 covers that
 * with room; each is 100-odd bytes out of a 1280 KB scene heap. */
#define SC1PSTAGECLEAR_GOBJS       64
#define SC1PSTAGECLEAR_GOBJPROCS   64
#define SC1PSTAGECLEAR_XOBJS       32
#define SC1PSTAGECLEAR_AOBJS       32
#define SC1PSTAGECLEAR_MOBJS       16
#define SC1PSTAGECLEAR_DOBJS       16
#define SC1PSTAGECLEAR_SOBJS      256
#define SC1PSTAGECLEAR_COBJS        8

// 0x80134EE0
/* The six files, as src/dc/decomp/reloc_data.us.h reads them off the
 * ROM: 80 SC1PStageClear1 (the score table's own text and the
 * fifty-seven bonus names), 81 SC1PStageClear2 (the word SCORE), 164
 * IFCommonPlayerDamage, 165 IFCommonTimer and 36 IFCommonDigits (the
 * three digit sets the HUD already ships), and 151 SC1PStageClear3
 * (the target and platform icons the two bonus stages count with).
 *
 * CUT: the seventh, 26 GRWallpaperTrainingBlack. It is not a picture
 * this scene shows -- sc1PStageClearCopyFramebufToWallpaper overwrites
 * every one of its 300x220 pixels with the frame the match left behind
 * before it is ever drawn -- so the port's own texture stands where the
 * game's loaded file did, and nothing of file 26 is read. That is also
 * why src/dc/mnmaps.c does not load it. */
u32 dSC1PStageClearFileIDs[/* */] = { 80, 81, 164, 165, 36, 151 };

/* one SpriteBank per file */
static SpriteBank sSC1PStageClearBanks[ARRAY_COUNT(dSC1PStageClearFileIDs)];

/* The wallpaper: the previous scene's last frame, rendered into a
 * texture by the frame loop (taskman.h syTaskmanGetPhoto), in place
 * of file 26's bitmap, and the Sprite laid over it so lb/lbcommon.c's
 * drawing runs on it unchanged. src/dc/stage.c stage_wallpaper_sprite
 * builds the same three records over a stage's wallpaper texture, and
 * says why the fields are what they are. The photo is the whole
 * 640x480 frame; TEXW/TEXH are the powers of two its UVs count in. */
#define SC1PSTAGECLEAR_WP_TEXW  1024
#define SC1PSTAGECLEAR_WP_TEXH  512

static DCSpriteTex sSC1PStageClearWallpaperTex;
static Bitmap      sSC1PStageClearWallpaperBitmap;
static Sprite      sSC1PStageClearWallpaperSprite;

// 0x80134EFC
SC1PStageClearScore dSC1PStageClearBonusData[/* */] =
{
	// Cheap Shot
	{ llSC1PStageClear1CheapShotTextSprite, -99 },

	// Star Finish
#if defined(REGION_US)
	{ llSC1PStageClear1StarFinishTextSprite, 10000 },
#else
	{ llSC1PStageClear1StarFinishTextSprite, 2000 },
#endif

	// No Item
#if defined(REGION_US)
	{ llSC1PStageClear1NoItemTextSprite, 1000 },
#else
	{ llSC1PStageClear1NoItemTextSprite, 5000 },
#endif

	// Shield Breaker
#if defined(REGION_US)
	{ llSC1PStageClear1ShieldBreakerTextSprite, 8000 },
#else
	{ llSC1PStageClear1ShieldBreakerTextSprite, 5000 },
#endif

	// Judo Warrior
#if defined(REGION_US)
	{ llSC1PStageClear1JudoWarriorTextSprite, 5000 },
#else
	{ llSC1PStageClear1JudoWarriorTextSprite, 4000 },
#endif

	// Hawk
#if defined(REGION_US)
	{ llSC1PStageClear1HawkTextSprite, 18000 },
#else
	{ llSC1PStageClear1HawkTextSprite, 10000 },
#endif

	// Shooter
#if defined(REGION_US)
	{ llSC1PStageClear1ShooterTextSprite, 12000 },
#else
	{ llSC1PStageClear1ShooterTextSprite, 5000 },
#endif

	// Heavy Damage
#if defined(REGION_US)
	{ llSC1PStageClear1HeavyDamageTextSprite, 28000 },
#else
	{ llSC1PStageClear1HeavyDamageTextSprite, 10000 },
#endif

	// All Variations
#if defined(REGION_US)
	{ llSC1PStageClear1AllVariationsTextSprite, 30000 },
#else
	{ llSC1PStageClear1AllVariationsTextSprite, 15000 },
#endif

	// Item Strike
#if defined(REGION_US)
	{ llSC1PStageClear1ItemStrikeTextSprite, 20000 },
#else
	{ llSC1PStageClear1ItemStrikeTextSprite, 10000 },
#endif

	// Double KO
#if defined(REGION_US)
	{ llSC1PStageClear1DoubleKOTextSprite, 0 },
#else
	{ llSC1PStageClear1DoubleKOTextSprite, 6000 },
#endif

	// Trickster
#if defined(REGION_US)
	{ llSC1PStageClear1TricksterTextSprite, 11000 },
#else
	{ llSC1PStageClear1TricksterTextSprite, 8000 },
#endif

	// Giant Impact
#if defined(REGION_US)
	{ llSC1PStageClear1GiantImpactTextSprite, 0 },
#else
	{ llSC1PStageClear1GiantImpactTextSprite, 7000 },
#endif

	// Speedster
#if defined(REGION_US)
	{ llSC1PStageClear1SpeedsterTextSprite, 10000 },
#else
	{ llSC1PStageClear1SpeedsterTextSprite, 8000 },
#endif

	// Item Throw
#if defined(REGION_US)
	{ llSC1PStageClear1ItemThrowTextSprite, 16000 },
#else
	{ llSC1PStageClear1ItemThrowTextSprite, 10000 },
#endif

	// Triple KO
#if defined(REGION_US)
	{ llSC1PStageClear1TripleKOTextSprite, 0 },
#else
	{ llSC1PStageClear1TripleKOTextSprite, 15000 },
#endif

	// Last Chance
#if defined(REGION_US)
	{ llSC1PStageClear1LastChanceTextSprite, 0 },
#else
	{ llSC1PStageClear1LastChanceTextSprite, 15000 },
#endif

	// Pacifist
#if defined(REGION_US)
	{ llSC1PStageClear1PacifistTextSprite, 60000 },
#else
	{ llSC1PStageClear1PacifistTextSprite, 30000 },
#endif

	// Perfect
#if defined(REGION_US)
	{ llSC1PStageClear1PerfectTextSprite, 30000 },
#else
	{ llSC1PStageClear1PerfectTextSprite, 10000 },
#endif

	// No Miss
#if defined(REGION_US)
	{ llSC1PStageClear1NoMissTextSprite, 5000 },
#else
	{ llSC1PStageClear1NoMissTextSprite, 1500 },
#endif

	// No Damage
#if defined(REGION_US)
	{ llSC1PStageClear1NoDamageTextSprite, 15000 },
#else
	{ llSC1PStageClear1NoDamageTextSprite, 10000 },
#endif

	// Full Power
	{ llSC1PStageClear1FullPowerTextSprite, 5000 },

	// Final Stage Clear
#if defined(REGION_US)
	{ llSC1PStageClear1VeryEasyClearTextSprite, 70000 },
#else
	{ llSC1PStageClear1VeryEasyClearTextSprite, 40000 },
#endif

	// No Miss Clear
#if defined(REGION_US)
	{ llSC1PStageClear1NoMissClearTextSprite, 70000 },
#else
	{ llSC1PStageClear1NoMissClearTextSprite, 40000 },
#endif

	// No Damage Clear
#if defined(REGION_US)
	{ llSC1PStageClear1NoDamageClearTextSprite, 400000 },
#else
	{ llSC1PStageClear1NoDamageClearTextSprite, 300000 },
#endif

	// Speed King
#if defined(REGION_US)
	{ llSC1PStageClear1SpeedKingTextSprite, 40000 },
#else
	{ llSC1PStageClear1SpeedKingTextSprite, 20000 },
#endif

	// Speed Demon
#if defined(REGION_US)
	{ llSC1PStageClear1SpeedDemonTextSprite, 80000 },
#else
	{ llSC1PStageClear1SpeedDemonTextSprite, 60000 },
#endif

	// Mew Catcher
#if defined(REGION_US)
	{ llSC1PStageClear1MewCatcherTextSprite, 15000 },
#else
	{ llSC1PStageClear1MewCatcherTextSprite, 8000 },
#endif

	// Star Clear
#if defined(REGION_US)
	{ llSC1PStageClear1StarClearTextSprite, 12000 },
#else
	{ llSC1PStageClear1StarClearTextSprite, 8000 },
#endif

	// Vegetarian
#if defined(REGION_US)
	{ llSC1PStageClear1VegetarianTextSprite, 9000 },
#else
	{ llSC1PStageClear1VegetarianTextSprite, 5000 },
#endif

	// Heart Throb
#if defined(REGION_US)
	{ llSC1PStageClear1HeartThrobTextSprite, 17000 },
#else
	{ llSC1PStageClear1HeartThrobTextSprite, 8000 },
#endif

	// Throw Down
	{ llSC1PStageClear1ThrowDownTextSprite, 2000 },

	// Smash Mania
#if defined(REGION_US)
	{ llSC1PStageClear1SmashManiaTextSprite, 3500 },
#else
	{ llSC1PStageClear1SmashManiaTextSprite, 3000 },
#endif

	// Smashless
#if defined(REGION_US)
	{ llSC1PStageClear1SmashlessTextSprite, 5000 },
#else
	{ llSC1PStageClear1SmashlessTextSprite, 3000 },
#endif

	// Special Move
#if defined(REGION_US)
	{ llSC1PStageClear1SpecialMoveTextSprite, 5000 },
#else
	{ llSC1PStageClear1SpecialMoveTextSprite, 5000 },
#endif

	// Single Move
	{ llSC1PStageClear1SingleMoveTextSprite, 8000 },

	// Pokemon Finish
#if defined(REGION_US)
	{ llSC1PStageClear1PokemonFinishTextSprite, 11000 },
#else
	{ llSC1PStageClear1PokemonFinishTextSprite, 8000 },
#endif

	// Booby Trap
#if defined(REGION_US)
	{ llSC1PStageClear1BoobyTrapTextSprite, 12000 },
#else
	{ llSC1PStageClear1BoobyTrapTextSprite, 8000 },
#endif

	// Fighter Stance
	{ llSC1PStageClear1FighterStanceTextSprite, 100 },

	// Mystic
#if defined(REGION_US)
	{ llSC1PStageClear1MysticTextSprite, 7000 },
#else
	{ llSC1PStageClear1MysticTextSprite, 6000 },
#endif

	// Comet Mystic
#if defined(REGION_US)
	{ llSC1PStageClear1CometMysticTextSprite, 10000 },
#else
	{ llSC1PStageClear1CometMysticTextSprite, 7000 },
#endif

	// Acid Clear
#if defined(REGION_US)
	{ llSC1PStageClear1AcidClearTextSprite, 1500 },
#else
	{ llSC1PStageClear1AcidClearTextSprite, 1000 },
#endif

	// Bumper Clear
#if defined(REGION_US)
	{ llSC1PStageClear1BumperClearTextSprite, 10000 },
#else
	{ llSC1PStageClear1BumperClearTextSprite, 3000 },
#endif

	// Tornado Clear
	{ llSC1PStageClear1TornadoClearTextSprite, 3000 },

	// ARWING Clear
#if defined(REGION_US)
	{ llSC1PStageClear1ArwingClearTextSprite, 4000 },
#else
	{ llSC1PStageClear1ArwingClearTextSprite, 3000 },
#endif

	// Counter Attack
#if defined(REGION_US)
	{ llSC1PStageClear1CounterAttackTextSprite, 0 },
#else
	{ llSC1PStageClear1CounterAttackTextSprite, 5000 },
#endif

	// Meteor Smash
#if defined(REGION_US)
	{ llSC1PStageClear1MeteorSmashTextSprite, 0 },
#else
	{ llSC1PStageClear1MeteorSmashTextSprite, 6000 },
#endif

	// Aerial
#if defined(REGION_US)
	{ llSC1PStageClear1AerialTextSprite, 0 },
#else
	{ llSC1PStageClear1AerialTextSprite, 20000 },
#endif

	// Last Second
#if defined(REGION_US)
	{ llSC1PStageClear1LastSecondTextSprite, 8000 },
#else
	{ llSC1PStageClear1LastSecondTextSprite, 10000 },
#endif

	// Lucky 3
#if defined(REGION_US)
	{ llSC1PStageClear1Lucky3TextSprite, 9990 },
#else
	{ llSC1PStageClear1Lucky3TextSprite, 8000 },
#endif

	// Jackpot
#if defined(REGION_US)
	{ llSC1PStageClear1JackpotTextSprite, 3330 },
#else
	{ llSC1PStageClear1JackpotTextSprite, 5000 },
#endif

	// Yoshi Rainbow
#if defined(REGION_US)
	{ llSC1PStageClear1YoshiRainbowTextSprite, 50000 },
#else
	{ llSC1PStageClear1YoshiRainbowTextSprite, 15000 },
#endif

	// Kirby Ranks
#if defined(REGION_US)
	{ llSC1PStageClear1KirbyRanksTextSprite, 25000 },
#else
	{ llSC1PStageClear1KirbyRanksTextSprite, 12000 },
#endif

	// Bros. Calamity
#if defined(REGION_US)
	{ llSC1PStageClear1BrosCalamityTextSprite, 25000 },
#else
	{ llSC1PStageClear1BrosCalamityTextSprite, 12000 },
#endif

	// DK Defender
#if defined(REGION_US)
	{ llSC1PStageClear1DKDefenderTextSprite, 10000 },
#else
	{ llSC1PStageClear1DKDefenderTextSprite, 7000 },
#endif

	// DK Perfect
	{ llSC1PStageClear1DKPerfectTextSprite, 50000 },

	// Good Friend
#if defined(REGION_US)
	{ llSC1PStageClear1GoodFriendTextSprite, 8000 },
#else
	{ llSC1PStageClear1GoodFriendTextSprite, 5000 },
#endif

	// True Friend
#if defined(REGION_US)
	{ llSC1PStageClear1TrueFriendTextSprite, 25000 }
#else
	{ llSC1PStageClear1TrueFriendTextSprite, 30000 },
#endif
};

/* CUT: dSC1PStageClearLights11 and dSC1PStageClearLights12
 * (0x801350D0, 0x801350E8), the two lights the pre-render function
 * would have set -- dGM1PStageClearTaskmanSetup says why, and neither
 * is read by anything else. The scene draws no model at all. */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x801352C0
u32 sSC1PStageClearPad0x801352C0[3];

// 0x801352CC
u32 sSC1PStageClearTotalTimeTics;

// 0x801352D0 - Set once, never used
u32 sSC1PStageClearUnused0x801352D0;

// 0x801352D4 - What kind of stage has been cleared
s32 sSC1PStageClearKind;

// 0x801352D8 - Whether player earned any special bonuses
sb32 sSC1PStageClearIsHaveBonusStats;

// 0x801352DC
s32 sSC1PStageClearSecondsRemain;

// 0x801352E0
s32 sSC1PStageClearDamageDealt;

// 0x801352E4
s32 sSC1PStageClearDifficulty;

// 0x801352E8
s32 sSC1PStageClearScoreTotal;

// 0x801352EC - Stage that the player just cleared / failed
s32 sSC1PStageClear1PGameStage;

// 0x801352F0
s32 sSC1PStageClearPad0x801352F0[2];

// 0x801352F8 - GObj of "Timer" and "Damage" scores
GObj *sSC1PStageClearTimerTextGObj;

// 0x801352FC
GObj *sSC1PStageClearTimerMultiplierGObj;

// 0x80135300
GObj *sSC1PStageClearDamageTextGObj;

// 0x80135304
GObj *sSC1PStageClearDamageMultiplierGObj;

// 0x80135308
s32 sSC1PStageClearPad0x80135308[2];

// 0x80135310 - GObj of "SCORE" text
GObj *sSC1PStageClearScoreTextGObj;

// 0x80135314 - GObj of "- BONUS -" text
GObj *sSC1PStageClearBonusTextGObj;

// 0x80135318
GObj *sSC1PStageClearTargetGObj;

// 0x8013531C
s32 sSC1PStageClearPad0x8013531C;

// 0x80135320
u32 sSC1PStageClearBonusFlags[3];

// 0x8013532C
s32 sSC1PStageClearBonusID;

// 0x80135330
s32 sSC1PStageClearBonusNum;

// 0x80135334
s32 sSC1PStageClearIsSetCommonAdvanceTic;

// 0x80135338
sb32 sSC1PStageClearIsAdvance;

// 0x8013533C
s32 sSC1PStageClearIsAllowProceedNext;

// 0x80135340
u32 sSC1PStageClearCommonAdvanceTic;

// 0x80135344
u32 sSC1PStageClearBonusShowNextTic;

// 0x80135348
u32 sSC1PStageClearBonusAdvanceTic;

// 0x80135350
GObj *sSC1PStageClearBonusStatGObjs[10];

// 0x80135378 - GObjs of target or platform sprites?
GObj *sSC1PStageClearBonusObjectiveGObjs[10];

// 0x801353A0
u32 sSC1PStageClearBaseIntervalTic;

// 0x801353A4
s32 sSC1PStageClearBonusObjectivesCleared;

// 0x801353A8
s32 sSC1PStageClearTimerTextTic;

// 0x801353AC
s32 sSC1PStageClearTimerDigitTic;

// 0x801353B0
s32 sSC1PStageClearTimerMultiplierTic;

// 0x801353B4
s32 sSC1PStageClearTimerEjectTic;

// 0x801353B8
s32 sSC1PStageClearDamageTextTic;

// 0x801353BC
s32 sSC1PStageClearDamageDigitTic;

// 0x801353C0
s32 sSC1PStageClearDamageMultiplierTic;

// 0x801353C4
s32 sSC1PStageClearDamageEjectTic;

// 0x801353C8
s32 sSC1PStageClearPad0x801353C8[2];

/* CUT: sSC1PStageClearStatusBuffer and sSC1PStageClearForceStatusBuffer,
 * the reloc loader's bookkeeping -- sc1PStageClearLoadFiles replaces the
 * loader they belong to. */

// 0x80135588
void *sSC1PStageClearFiles[ARRAY_COUNT(dSC1PStageClearFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* CUT: sc1PStageClearFuncLights (0x80131B00), the pre-render function --
 * dGM1PStageClearTaskmanSetup carries NULL where the decomp names it,
 * as every other ported scene's does. Here it is a cut with nothing
 * behind it: the screen is sprites from top to bottom, so there is no
 * lit model for ftDisplayLightsDrawReflect to light. */

// 0x80131B58
s32 sc1PStageClearGetPowerOf(s32 base, s32 exp)
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
void sc1PStageClearSetDigitSpriteColors(SObj *sobj, s32 digit_kind, SYColorRGBPair *colors_base)
{
	// 0x80135100
	SYColorRGBPair colors_all[/* */] =
	{
		// Damage / Timer digits
		{
			{ 0x00, 0x00, 0x00 },
			{ 0xC8, 0xCB, 0xD3 }
		},

		// Bonus stat digits
		{
			{ 0x00, 0x00, 0x00 },
			{ 0xFF, 0xFF, 0xFF }
		},

		// Score digits
		{
			{ 0x00, 0x00, 0x00 },
			{ 0xFF, 0xEC, 0x00 }
		}
	};
	SYColorRGBPair *colors_id = &colors_all[digit_kind];

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	if (colors_base == NULL)
	{
		sobj->envcolor.r = colors_id->prim.r;
		sobj->envcolor.g = colors_id->prim.g;
		sobj->envcolor.b = colors_id->prim.b;
		sobj->sprite.red = colors_id->env.r;
		sobj->sprite.green = colors_id->env.g;
		sobj->sprite.blue = colors_id->env.b;
	}
	else
	{
		sobj->envcolor.r = colors_base->prim.r;
		sobj->envcolor.g = colors_base->prim.g;
		sobj->envcolor.b = colors_base->prim.b;
		sobj->sprite.red = colors_base->env.r;
		sobj->sprite.green = colors_base->env.g;
		sobj->sprite.blue = colors_base->env.b;
	}
}

// 0x80131CC4
s32 sc1PStageClearGetScoreDigitCount(s32 points, s32 digit_count_max)
{
	s32 digit_count_curr = digit_count_max;

	while (digit_count_curr > 0)
	{
		s32 digit = (sc1PStageClearGetPowerOf(10, digit_count_curr - 1) != 0) ? points / sc1PStageClearGetPowerOf(10, digit_count_curr - 1) : 0;

		if (digit != 0)
		{
			return digit_count_curr;
		}
		else digit_count_curr--;
	}
	return 0;
}

// 0x80131D70
Sprite* sc1PStageClearGetScoreDigitSprite(s32 digit_kind, s32 digit)
{
	// 0x80135114
	s32 file_array_ids[/* */] = { 0, 4, 2 };

	// 0x80135120
	intptr_t offsets[/* */][3] =
	{
		{ llSC1PStageClear1TimerDamageDigit0Sprite, llIFCommonDigits0Sprite, llIFCommonPlayerDamageDigit0Sprite },
		{ llSC1PStageClear1TimerDamageDigit1Sprite, llIFCommonDigits1Sprite, llIFCommonPlayerDamageDigit1Sprite },
		{ llSC1PStageClear1TimerDamageDigit2Sprite, llIFCommonDigits2Sprite, llIFCommonPlayerDamageDigit2Sprite },
		{ llSC1PStageClear1TimerDamageDigit3Sprite, llIFCommonDigits3Sprite, llIFCommonPlayerDamageDigit3Sprite },
		{ llSC1PStageClear1TimerDamageDigit4Sprite, llIFCommonDigits4Sprite, llIFCommonPlayerDamageDigit4Sprite },
		{ llSC1PStageClear1TimerDamageDigit5Sprite, llIFCommonDigits5Sprite, llIFCommonPlayerDamageDigit5Sprite },
		{ llSC1PStageClear1TimerDamageDigit6Sprite, llIFCommonDigits6Sprite, llIFCommonPlayerDamageDigit6Sprite },
		{ llSC1PStageClear1TimerDamageDigit7Sprite, llIFCommonDigits7Sprite, llIFCommonPlayerDamageDigit7Sprite },
		{ llSC1PStageClear1TimerDamageDigit8Sprite, llIFCommonDigits8Sprite, llIFCommonPlayerDamageDigit8Sprite },
		{ llSC1PStageClear1TimerDamageDigit9Sprite, llIFCommonDigits9Sprite, llIFCommonPlayerDamageDigit9Sprite }
	};

	return lbRelocGetFileData
	(
		Sprite*,
		sSC1PStageClearFiles[file_array_ids[digit_kind]],
		offsets[digit][digit_kind]
	);
}

// 0x80131E10
void sc1PStageClearMakeScoreDigits
(
	GObj *gobj,
	s32 points,
	f32 x,
	f32 y,
	SYColorRGBPair *colors,
	s32 offset_x,
	s32 digit_kind,
	s32 sub,
	s32 digit_count_max,
	sb32 is_fixed_digit_count
)
{
	SObj *sobj;
	f32 calc_x;
	s32 i;
	sb32 is_negative;
	s32 digit;

	is_negative = FALSE;

	if (points < 0)
	{
		if ((digit_kind == 2) || (digit_kind == 0))
		{
			points = 0;
		}
		else
		{
			points = -points;
			is_negative = TRUE;
		}
	}
	sobj = lbCommonMakeSObjForGObj(gobj, sc1PStageClearGetScoreDigitSprite(digit_kind, points % 10));

	sc1PStageClearSetDigitSpriteColors(sobj, digit_kind, colors);

	calc_x = (sub != 0) ? x - sub : x - (sobj->sprite.width + offset_x);

	sobj->pos.x = calc_x;
	sobj->pos.y = y;

	for (i = 1; i < ((is_fixed_digit_count != FALSE) ? digit_count_max : sc1PStageClearGetScoreDigitCount(points, digit_count_max)); i++)
	{
		digit = (sc1PStageClearGetPowerOf(10, i) != 0) ? points / sc1PStageClearGetPowerOf(10, i) : 0;

		sobj = lbCommonMakeSObjForGObj(gobj, sc1PStageClearGetScoreDigitSprite(digit_kind, digit % 10));

		sc1PStageClearSetDigitSpriteColors(sobj, digit_kind, colors);

		calc_x = (sub != 0) ? calc_x - sub : calc_x - (sobj->sprite.width + offset_x);

		sobj->pos.x = calc_x;
		sobj->pos.y = y;
	}
	if (is_negative != FALSE)
	{
		if (digit_kind == 1)
		{
			sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[4], llIFCommonDigitsDashSprite));
		}
		sc1PStageClearSetDigitSpriteColors(sobj, digit_kind, colors);

		calc_x = (sub != 0) ? calc_x - sub : calc_x - (sobj->sprite.width + offset_x);

		sobj->pos.x = calc_x;
		sobj->pos.y = y + 3.0F;
	}
}

// 0x801320E0
void sc1PStageClearTextProcDisplay(GObj *gobj)
{
	gDPPipeSync(gSYTaskmanDLHeads[0]++);
	lbCommonDrawSObjAttr(gobj);
	gDPPipeSync(gSYTaskmanDLHeads[0]++);
}

// 0x8013213C
void sc1PStageClearMakeTextSObjs(void)
{
	GObj *gobj;
	SObj *sobj;

	gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(gobj, sc1PStageClearTextProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1TextShadowSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->sprite.red = 0x00;
	sobj->sprite.green = 0x00;
	sobj->sprite.blue = 0x00;

	sobj->pos.x = 33.0F;
	sobj->pos.y = 23.0F;

	if (sSC1PStageClearKind == nSC1PStageClearKindResult)
	{
		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1ResultTextSprite));
		sobj->sprite.attr &= ~SP_FASTCOPY;
		sobj->sprite.attr |= SP_TRANSPARENT;

		sobj->envcolor.r = 0xFF;
		sobj->envcolor.g = 0x00;
		sobj->envcolor.b = 0x00;

		sobj->sprite.red = 0xFF;
		sobj->sprite.green = 0xC8;
		sobj->sprite.blue = 0x00;

		sobj->pos.x = 104.0F;
		sobj->pos.y = 24.0F;
	}
	else
	{
		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1ClearTextSprite));

		sobj->sprite.attr &= ~SP_FASTCOPY;
		sobj->sprite.attr |= SP_TRANSPARENT;

		sobj->envcolor.r = 0xFF;
		sobj->envcolor.g = 0x00;
		sobj->envcolor.b = 0x00;

		sobj->sprite.red = 0xFF;
		sobj->sprite.green = 0xC8;
		sobj->sprite.blue = 0x00;

		sobj->pos.x = 166.0F;
		sobj->pos.y = 24.0F;

		if (sSC1PStageClearKind == nSC1PStageClearKindStage)
		{
			sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1StageTextSprite));
		}
		else sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1GameTextSprite));

		sobj->sprite.attr &= ~SP_FASTCOPY;
		sobj->sprite.attr |= SP_TRANSPARENT;

		sobj->envcolor.r = 0xFF;
		sobj->envcolor.g = 0x00;
		sobj->envcolor.b = 0x00;

		sobj->sprite.red = 0xFF;
		sobj->sprite.green = 0xC8;
		sobj->sprite.blue = 0x00;

		sobj->pos.x = 53.0F;
		sobj->pos.y = 24.0F;
	}
	sSC1PStageClearBonusTextGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(gobj, sc1PStageClearTextProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1BonusTextSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->sprite.red = 0xFF;
	sobj->sprite.green = 0x28;
	sobj->sprite.blue = 0x0A;

	sobj->pos.x = 121.0F;
	sobj->pos.y = 67.0F;
}

// 0x801323F8
void sc1PStageClearMakeScoreSObjs(void)
{
	GObj *gobj;
	SObj *sobj;

	sSC1PStageClearScoreTextGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(gobj, sc1PStageClearTextProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[1], llSC1PStageClear2ScoreTextSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->envcolor.r = 0xFF;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;

	sobj->sprite.red = 0xFF;
	sobj->sprite.green = 0xC8;
	sobj->sprite.blue = 0x00;

	sobj->pos.x = 90.0F;
	sobj->pos.y = 200.0F;

	sc1PStageClearMakeScoreDigits(gobj, sSC1PStageClearScoreTotal, 295.0F, 197.0F, NULL, 0, 2, 16, 8, TRUE);
}

// 0x801324FC
void sc1PStageClearMakeTimerTextSObjs(f32 y)
{
	GObj *gobj;
	SObj *sobj;

	sSC1PStageClearTimerTextGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(gobj, sc1PStageClearTextProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1TimerTextSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->pos.x = 42.0F;
	sobj->pos.y = y;

	sobj->envcolor.r = 0x00;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;

	sobj->sprite.red = 0xB7;
	sobj->sprite.green = 0xE4;
	sobj->sprite.blue = 0xFF;

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1ColonTextSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->pos.x = 118.0F;
	sobj->pos.y = y + 1.0F;

	sobj->envcolor.r = 0x00;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;

	sobj->sprite.red = 0xB7;
	sobj->sprite.green = 0xE4;
	sobj->sprite.blue = 0xFF;

	func_800269C0_275C0(nSYAudioFGMStageClearScoreDisplay);
}

// 0x8013263C
void sc1PStageClearMakeTimerDigits(f32 y)
{
	GObj *gobj;
	SObj *sobj;
	f32 x;
	s32 multiplier;
	s32 unused;

	sSC1PStageClearTimerMultiplierGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(gobj, sc1PStageClearTextProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[3], llIFCommonTimerSymbolCrossSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->pos.x = 181.0F;
	sobj->pos.y = y + 2.0F;

	sobj->envcolor.r = 0x00;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;

	sobj->sprite.red = 0xFF;
	sobj->sprite.green = 0xFF;
	sobj->sprite.blue = 0xFF;

	switch (sSC1PStageClear1PGameStage)
	{
	case nSC1PGameStageBonus1:
	case nSC1PGameStageBonus2:
		x = 246.0F;
		multiplier = 200;
		break;

	case nSC1PGameStageBonus3:
		x = 246.0F;
		multiplier = 500;
		break;

	default:
#if defined(REGION_US)
		x = 233.0F;
		multiplier = 50;
#else
		x = 236.0F;
		multiplier = 100;
#endif
	}
	sc1PStageClearMakeScoreDigits(gobj, multiplier, x, y - 1.0F, NULL, 1, 0, 0, 4, FALSE);
	sc1PStageClearMakeScoreDigits(gobj, sSC1PStageClearSecondsRemain, 171.0F, y - 1.0F, NULL, 1, 0, 0, 3, FALSE);
}

// 0x801327D4
s32 sc1PStageClearGetAppendTotalTimeScore(f32 y)
{
	GObj *gobj;
	s32 unused;
	s32 time_score_total;
	s32 multiplier;

	sSC1PStageClearTimerMultiplierGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(gobj, sc1PStageClearTextProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);

	switch (sSC1PStageClear1PGameStage)
	{
	case nSC1PGameStageBonus1:
	case nSC1PGameStageBonus2:
		multiplier = 200;
		break;

	case nSC1PGameStageBonus3:
		multiplier = 500;
		break;

	default:
#if defined(REGION_US)
		multiplier = 50;
#else
		multiplier = 100;
#endif
	}
	time_score_total = sSC1PStageClearSecondsRemain * multiplier;

	sc1PStageClearMakeScoreDigits(gobj, time_score_total, 200.0F, y - 1.0F, NULL, 1, 0, 0, 5, FALSE);
	func_800269C0_275C0(nSYAudioFGMStageClearScoreRegister);

	return time_score_total;
}

// 0x801328CC
void sc1PStageClearMakeDamageTextSObjs(f32 y)
{
	GObj *gobj;
	SObj *sobj;
	s32 unused;

	sSC1PStageClearDamageTextGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
	gcAddGObjDisplay(gobj, sc1PStageClearTextProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1DamageTextSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->pos.x = 42.0F;
	sobj->pos.y = (s32)y;

	sobj->envcolor.r = 0x00;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;

	sobj->sprite.red = 0xB7;
	sobj->sprite.green = 0xE4;
	sobj->sprite.blue = 0xFF;

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1ColonTextSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->pos.x = 118.0F;
	sobj->pos.y = (s32)y + 2;

	sobj->envcolor.r = 0x00;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;

	sobj->sprite.red = 0xB7;
	sobj->sprite.green = 0xE4;
	sobj->sprite.blue = 0xFF;

	func_800269C0_275C0(nSYAudioFGMStageClearScoreDisplay);
}

// 0x80132A20
void sc1PStageClearMakeDamageDigits(f32 y)
{
	GObj *gobj;
	SObj *sobj;
	s32 x;
	s32 unused;

	x = (sSC1PStageClearDamageDealt > 1000) ? 184 : 171;

	sSC1PStageClearDamageMultiplierGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(gobj, sc1PStageClearTextProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);

	sc1PStageClearMakeScoreDigits(gobj, sSC1PStageClearDamageDealt, x, (s32)y - 1, NULL, 1, 0, 0, 4, FALSE);
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[3], llIFCommonTimerSymbolCrossSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->pos.x = x + 10;
	sobj->pos.y = (s32)y + 2;

	sobj->envcolor.r = 0x00;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;

	sobj->sprite.red = 0xFF;
	sobj->sprite.green = 0xFF;
	sobj->sprite.blue = 0xFF;

	sc1PStageClearMakeScoreDigits(gobj, 10, x + 55, (s32)y - 1, NULL, 1, 0, 0, 2, TRUE);
}

// 0x80132BB4
s32 sc1PStageClearGetAppendTotalDamageScore(f32 y)
{
	GObj *gobj;
	s32 unused;
	s32 damage_score_total;

	sSC1PStageClearDamageMultiplierGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(gobj, sc1PStageClearTextProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
	damage_score_total = sSC1PStageClearDamageDealt * 10;

	sc1PStageClearMakeScoreDigits(gobj, damage_score_total, 200.0F, (s32)y - 1, NULL, 1, 0, 0, 5, FALSE);
	func_800269C0_275C0(nSYAudioFGMStageClearScoreRegister);

	return damage_score_total;
}

// 0x80132C80
void sc1PStageClearMakeTargetTextSObjs(void)
{
	GObj *gobj;
	SObj *sobj;
	s32 y1 = 94, y2 = 96;

	sSC1PStageClearTargetGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(gobj, sc1PStageClearTextProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1TargetTextSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->pos.x = 42.0F;
	sobj->pos.y = y1;

	sobj->envcolor.r = 0x00;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;

	sobj->sprite.red = 0xB7;
	sobj->sprite.green = 0xE4;
	sobj->sprite.blue = 0xFF;

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1ColonTextSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->pos.x = 118.0F;
	sobj->pos.y = y2;

	sobj->envcolor.r = 0x00;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;

	sobj->sprite.red = 0xB7;
	sobj->sprite.green = 0xE4;
	sobj->sprite.blue = 0xFF;

	func_800269C0_275C0(nSYAudioFGMStageClearScoreDisplay);
}

// 0x80132DC0
void func_ovl56_80132DC0(GObj *gobj)
{
	gobj->flags = (gobj->user_data.u < sSC1PStageClearTotalTimeTics) ? GOBJ_FLAG_NONE : GOBJ_FLAG_HIDDEN;

	if (gobj->user_data.u == sSC1PStageClearTotalTimeTics)
	{
		func_800269C0_275C0(nSYAudioFGMStageClearScoreRegister);
		gcEjectGObj(sSC1PStageClearScoreTextGObj);
		sSC1PStageClearScoreTotal += 1000;
		sc1PStageClearMakeScoreSObjs();
	}
}

// 0x80132E40
void func_ovl56_80132E40(f32 x, f32 y, s32 objective_num)
{
	GObj *gobj;
	SObj *sobj;

	sSC1PStageClearBonusObjectiveGObjs[objective_num] = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(gobj, sc1PStageClearTextProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, func_ovl56_80132DC0, nGCProcessKindFunc, 1);

	gobj->user_data.u = (objective_num * 10) + sSC1PStageClearTotalTimeTics;

	switch (sSC1PStageClear1PGameStage)
	{
	case nSC1PGameStageBonus1:
		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[5], llSC1PStageClear3TargetSprite));
		break;

	case nSC1PGameStageBonus2:
		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[5], llSC1PStageClear3PlatformSprite));
		break;

#ifdef AVOID_UB
	/* The same warning sc1PIntroMakeBonusTasks carries, and the decomp
	 * writes the guard there and not here: sobj is uninitialized in the
	 * default case and read below it. Only sc1PStageClearUpdateResultScore
	 * calls this, and only for a bonus stage, so the default arm is
	 * unreachable in the game -- but the port builds with -DAVOID_UB and
	 * a stage the bonus scenes have not filled in yet would otherwise
	 * write through a stack slot. */
	default:
		sobj = NULL;
		break;
#endif
	}
#ifdef AVOID_UB
	if (sobj == NULL)
	{
		return;
	}
#endif
	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->pos.x = x;
	sobj->pos.y = y;
}

// 0x80132F78
void func_ovl56_80132F78(void)
{
	s32 i = 0;

	if (sSC1PStageClearBonusObjectivesCleared > 0)
	{
		s32 x = 130;

		do
		{
			func_ovl56_80132E40(x, 93.0F, i);
			i++, x += 16;
		}
		while (i < sSC1PStageClearBonusObjectivesCleared);
	}
}

// 0x80132FF8
SC1PStageClearStats* sc1PStageClearSetupBonusStats(SC1PStageClearStats *bonus_setup, s32 bonus_id)
{
	SC1PStageClearStats bonus;

	if (bonus_id < 32)
	{
		bonus.bonus_array_id = 0;
		bonus.bonus_id = bonus_id;
	}
	else if (bonus_id < 64)
	{
		bonus.bonus_array_id = 1;
		bonus.bonus_id = bonus_id - 32;
	}
	else
	{
		bonus.bonus_array_id = 2;
		bonus.bonus_id = bonus_id - 64;
	}
	*bonus_setup = bonus;

	return bonus_setup;
}

// 0x8013305C
sb32 sc1PStageClearCheckHaveBonusStats(void)
{
	s32 unused[3];
	SC1PStageClearStats bonus;
	s32 i = 0; while (TRUE) // WARNING: Newline memes
	{
		sc1PStageClearSetupBonusStats(&bonus, i);

		i++;

		if (sSC1PStageClearBonusFlags[bonus.bonus_array_id] & (1 << bonus.bonus_id))
		{
			return TRUE;
		}
		if (i == NBITS(sSC1PStageClearBonusFlags))
		{
			return FALSE;
		}
	}
}

// 0x801330F0
sb32 sc1PStageClearCheckGameClearBonus(s32 bonus_id)
{
	switch (bonus_id)
	{
	case nSC1PGameBonusStageClear:
	case nSC1PGameBonusNoMissClear:
	case nSC1PGameBonusNoDamageClear:
	case nSC1PGameBonusSpeedKing:
	case nSC1PGameBonusSpeedDemon:
		return TRUE;

	default:
		return FALSE;
	}
}

// 0x80133128
s32 sc1PStageClearGetNoMissMultiplier(s32 stage)
{
	// 0x80135198 - "No Miss" bonus multiplier
	s32 nomiss[/* */] = { 1, 2, 3, 0, 4, 5, 6, 0, 7, 8, 9, 0, 10, 11 };

	return nomiss[stage];
}

// 0x80133188
void sc1PStageClearCommonProcUpdate(GObj *gobj)
{
	if (gobj->user_data.u == sSC1PStageClearTotalTimeTics)
	{
		func_800269C0_275C0(nSYAudioFGMStageClearScoreDisplay);
	}
	gobj->flags = (gobj->user_data.u < sSC1PStageClearTotalTimeTics) ? GOBJ_FLAG_NONE : GOBJ_FLAG_HIDDEN;
}

// 0x801331EC
s32 sc1PStageClearGetAppendBonusStatPoints(s32 bonus_id, s32 bonus_num, f32 x, f32 y)
{
	GObj *gobj;
	SObj *sobj;
	s32 points;

	// 0x801351D0
	intptr_t offsets[/* */] =
	{
		llSC1PStageClear1VeryEasyClearTextSprite,
		llSC1PStageClear1EasyClearTextSprite,
		llSC1PStageClear1NormalClearTextSprite,
		llSC1PStageClear1HardClearTextSprite,
		llSC1PStageClear1VeryHardClearTextSprite
	};

	// 0x801351E4
	SYColorRGBPair colors = { { 0x00, 0x00, 0x00 }, { 0xFF, 0xFF, 0x00 } };

	sSC1PStageClearBonusStatGObjs[bonus_num] = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(gobj, sc1PStageClearTextProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, sc1PStageClearCommonProcUpdate, nGCProcessKindFunc, 1);

	gobj->user_data.u = (bonus_num * 10) + sSC1PStageClearTotalTimeTics;

	if (bonus_id == nSC1PGameBonusStageClear)
	{
		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], offsets[sSC1PStageClearDifficulty]));
	}
	else sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], dSC1PStageClearBonusData[bonus_id].offset));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->pos.x = x;
	sobj->pos.y = y;

	if (sc1PStageClearCheckGameClearBonus(bonus_id) != FALSE)
	{
		sobj->sprite.red = 0xFF;
		sobj->sprite.green = 0x00;
		sobj->sprite.blue = 0x00;
	}
	else
	{
		sobj->sprite.red = 0xFF;
		sobj->sprite.green = 0xFF;
		sobj->sprite.blue = 0x00;
	}
	if (bonus_id == nSC1PGameBonusNoMiss)
	{
		sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[4], llIFCommonDigitsCrossSprite));

		sobj->sprite.attr &= ~SP_FASTCOPY;
		sobj->sprite.attr |= SP_TRANSPARENT;

		sobj->pos.x = x + 40.0F;
		sobj->pos.y = y - 1.0F;

		sobj->sprite.red = 0xFF;
		sobj->sprite.green = 0xFF;
		sobj->sprite.blue = 0x00;

		sc1PStageClearMakeScoreDigits(gobj, sc1PStageClearGetNoMissMultiplier(sSC1PStageClear1PGameStage), (x + 40.0F) + 26.0F, y - 1.0F, &colors, 0, 1, 0, 2, FALSE);
	}
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[4], llIFCommonDigitsColonSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->pos.x = 183.0F;
	sobj->pos.y = y;

	sobj->sprite.red = 0xFF;
	sobj->sprite.green = 0xFF;
	sobj->sprite.blue = 0x00;

	switch (bonus_id)
	{
	case nSC1PGameBonusNoMiss:
		points = dSC1PStageClearBonusData[bonus_id].points * sc1PStageClearGetNoMissMultiplier(sSC1PStageClear1PGameStage);
		break;

	case nSC1PGameBonusStageClear:
		points = dSC1PStageClearBonusData[bonus_id].points * (sSC1PStageClearDifficulty + 1);
		break;

	default:
		points = dSC1PStageClearBonusData[bonus_id].points;
	}
	sc1PStageClearMakeScoreDigits(gobj, points, 241.0F, y - 1.0F, NULL, 0, 1, 0, 6, FALSE);

	return points;
}

// 0x801335A0
void sc1PStageClearMakeBonusPageArrow(void)
{
	GObj *gobj;
	SObj *sobj;

	sSC1PStageClearBonusStatGObjs[9] = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(gobj, sc1PStageClearTextProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
	gcAddGObjProcess(gobj, sc1PStageClearCommonProcUpdate, nGCProcessKindFunc, 1);

	gobj->user_data.u = sSC1PStageClearTotalTimeTics + 90;

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1BonusPageArrowSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->pos.x = 249.0F;
	sobj->pos.y = 176.0F;
}

// 0x80133668
sb32 sc1PStageClearCheckHaveBonusStatID(s32 bonus_id)
{
	SC1PStageClearStats bonus;

	while (bonus_id < NBITS(sSC1PStageClearBonusFlags))
	{
		sc1PStageClearSetupBonusStats(&bonus, bonus_id);

		bonus_id++;

		if (sSC1PStageClearBonusFlags[bonus.bonus_array_id] & (1 << bonus.bonus_id))
		{
			return TRUE;
		}
	}
	return FALSE;
}

// 0x801336F8
s32 sc1PStageClearGetUpdateBonusStatPointsAll(void)
{
	s32 unused[2];
	s32 i;
	s32 points;
	SC1PStageClearStats bonus;

	points = 0;

	for (i = 0; i < ARRAY_COUNT(sSC1PStageClearBonusStatGObjs); i++)
	{
		sSC1PStageClearBonusStatGObjs[i] = NULL;
	}
	sSC1PStageClearIsSetCommonAdvanceTic = 0;
	sSC1PStageClearBonusNum = 0;

	while (TRUE)
	{
		if (sSC1PStageClearBonusID == NBITS(sSC1PStageClearBonusFlags))
		{
			sSC1PStageClearIsAdvance = TRUE;
			return points;
		}
		if (sSC1PStageClearBonusNum == (ARRAY_COUNT(sSC1PStageClearBonusStatGObjs) - 1))
		{
			if (sc1PStageClearCheckHaveBonusStatID(sSC1PStageClearBonusID) == FALSE)
			{
				sSC1PStageClearIsAdvance = TRUE;
				return points;
			}
			sc1PStageClearMakeBonusPageArrow();
			return points;
		}
		sc1PStageClearSetupBonusStats(&bonus, sSC1PStageClearBonusID);

		if (sSC1PStageClearBonusFlags[bonus.bonus_array_id] & (1 << bonus.bonus_id))
		{
			points += sc1PStageClearGetAppendBonusStatPoints(sSC1PStageClearBonusID, sSC1PStageClearBonusNum, 80.0F, (sSC1PStageClearBonusNum * 11) + 86);
			sSC1PStageClearBonusNum++;
		}
		sSC1PStageClearBonusID++;
	}
}

// 0x801338A0
void sc1PStageClearMakeBonusTable(void)
{
	GObj *gobj;
	SObj *sobj;

	gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(gobj, sc1PStageClearTextProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1BonusBorderSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->envcolor.r = 0x00;
	sobj->envcolor.g = 0x00;
	sobj->envcolor.b = 0x00;

	sobj->sprite.red = 0xFA;
	sobj->sprite.green = 0xE2;
	sobj->sprite.blue = 0xB5;

	sobj->pos.x = 52.0F;
	sobj->pos.y = 62.0F;

	sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PStageClearFiles[0], llSC1PStageClear1SpecialBonusTextSprite));

	sobj->sprite.attr &= ~SP_FASTCOPY;
	sobj->sprite.attr |= SP_TRANSPARENT;

	sobj->envcolor.r = 0xFF;
	sobj->envcolor.g = 0xFF;
	sobj->envcolor.b = 0x00;

	sobj->sprite.red = 0xFF;
	sobj->sprite.green = 0xFF;
	sobj->sprite.blue = 0xFF;

	sobj->pos.x = 91.0F;
	sobj->pos.y = 72.0F;
}

// 0x801339C0
void sc1PStageClearWallpaperProcDisplay(GObj *gobj)
{
	gDPPipeSync(gSYTaskmanDLHeads[0]++);
	gDPSetCycleType(gSYTaskmanDLHeads[0]++, G_CYC_1CYCLE);
	gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_OPA_SURF, G_RM_OPA_SURF2);

	/* gDPSetPrimColor 0x80 grey and G_CC_MODULATEI_PRIM, which dim the
	 * match's last frame to half, are the port's two lbCommonSprite*
	 * calls (lbcommon.h): the GBI lines write the scratch heads and
	 * nothing reads them. */
	lbCommonSpriteSetPrimColor(0x80, 0x80, 0x80, 0xFF);
	lbCommonSpriteSetCombine(nLBCommonCombineTexPrim);

	lbCommonDrawSObjNoAttr(gobj);

	gDPPipeSync(gSYTaskmanDLHeads[0]++);
	gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
}

/* sc1pstageclear.c:1556-1579 0x80133AC0.
 *
 * DIVERGES in one argument: the Sprite is the port's own
 * (sc1PStageClearCopyFramebufToWallpaper built it over the texture it
 * grabbed) where the game's is lbRelocGetFileData(Sprite*,
 * sSC1PStageClearFiles[6], llGRWallpaperTrainingBlackSprite). Both are
 * the same picture by the time this runs -- the game overwrites file
 * 26's pixels with the framebuffer and so does the port -- and the
 * dSC1PStageClearFileIDs note above says why the port keeps no file
 * there. The rest, the GObj, the display link and the two positions,
 * is the decomp's. A grab that failed leaves the Sprite NULL and the
 * scene draws no wallpaper, which is the shape every other missing
 * bank takes here. */
void sc1PStageClearMakeWallpaper(void)
{
	GObj *gobj;
	SObj *sobj;

	if (sSC1PStageClearWallpaperSprite.bitmap == NULL)
	{
		return;
	}
	gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);

	gcAddGObjDisplay(gobj, sc1PStageClearWallpaperProcDisplay, 27, GOBJ_PRIORITY_DEFAULT, ~0);
	sobj = lbCommonMakeSObjForGObj(gobj, &sSC1PStageClearWallpaperSprite);

	sobj->sprite.attr &= ~SP_FASTCOPY;

	/* DIVERGES: 0, not 10. The game's copy skipped the frame's ten-pixel
	 * border and drew the 300x220 rest at (10, 10); the photo is the
	 * whole frame, so it is drawn from the corner and the camera's
	 * 10..310 x 10..230 scissor leaves out the same border. */
	sobj->pos.x = 0.0F;
	sobj->pos.y = 0.0F;
}

// 0x80133B48
void sc1PStageClearMakeTextCamera(void)
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

// 0x80133BE8
void sc1PStageClearMakeWallpaperCamera(void)
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
			90,
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

// 0x80133C88
void sc1PStageClearInitVars(void)
{
	s32 i;

	sSC1PStageClearTotalTimeTics = 0;
	sSC1PStageClearUnused0x801352D0 = 0;

	sSC1PStageClear1PGameStage = gSCManagerSceneData.spgame_stage;
	sSC1PStageClearDifficulty = gSCManagerBackupData.spgame_difficulty;

	switch (sSC1PStageClear1PGameStage)
	{
	default:
		sSC1PStageClearKind = nSC1PStageClearKindStage;
		break;

	case nSC1PGameStageBonus1:
	case nSC1PGameStageBonus2:
	case nSC1PGameStageBonus3:
		sSC1PStageClearKind = nSC1PStageClearKindResult;
		sSC1PStageClearBonusObjectivesCleared = gSCManagerSceneData.bonus_tasks_complete;
		break;

	case nSC1PGameStageBoss:
		sSC1PStageClearKind = nSC1PStageClearKindGame;
		break;
	}
	sSC1PStageClearSecondsRemain = gSCManagerSceneData.spgame_time_remain;
	sSC1PStageClearDamageDealt = gSCManager1PGameBattleState.players[gSCManagerSceneData.player].total_damage_given;
	sSC1PStageClearScoreTotal = gSCManagerSceneData.spgame_score;

	sSC1PStageClearBonusFlags[0] = gSCManagerSceneData.bonus_get_mask[0];
	sSC1PStageClearBonusFlags[1] = gSCManagerSceneData.bonus_get_mask[1];
	sSC1PStageClearBonusFlags[2] = gSCManagerSceneData.bonus_get_mask[2];

	sSC1PStageClearIsHaveBonusStats = sc1PStageClearCheckHaveBonusStats();

	sSC1PStageClearBonusID = 0;
	sSC1PStageClearBaseIntervalTic = 0;

	for (i = 0; i < ARRAY_COUNT(sSC1PStageClearBonusStatGObjs); i++)
	{
		sSC1PStageClearBonusStatGObjs[i] = NULL;
	}
	for (i = 0; i < ARRAY_COUNT(sSC1PStageClearBonusObjectiveGObjs); i++)
	{
		sSC1PStageClearBonusObjectiveGObjs[i] = NULL;
	}
	sSC1PStageClearTimerTextGObj = NULL;
	sSC1PStageClearTimerMultiplierGObj = NULL;
	sSC1PStageClearTargetGObj = NULL;

	sSC1PStageClearIsSetCommonAdvanceTic = 0;
	sSC1PStageClearIsAdvance = FALSE;
	sSC1PStageClearIsAllowProceedNext = 0;

	if ((sSC1PStageClearKind == nSC1PStageClearKindStage) || (sSC1PStageClearKind == nSC1PStageClearKindGame))
	{
		if (sc1PStageClearCheckNoTimer() != FALSE)
		{
			sSC1PStageClearDamageTextTic = 10;
			sSC1PStageClearDamageDigitTic = 20;
			sSC1PStageClearDamageMultiplierTic = 40;
			sSC1PStageClearDamageEjectTic = 60;
		}
		else
		{
			sSC1PStageClearTimerTextTic = 10;
			sSC1PStageClearTimerDigitTic = 20;
			sSC1PStageClearTimerMultiplierTic = 60;
			sSC1PStageClearTimerEjectTic = 80;
			sSC1PStageClearDamageTextTic = 30;
			sSC1PStageClearDamageDigitTic = 40;
			sSC1PStageClearDamageMultiplierTic = 100;
			sSC1PStageClearDamageEjectTic = 120;
		}
	}
}

// 0x80133EEC
void sc1PStageClearUpdateTotal1PGameScore(void)
{
	gSCManagerSceneData.spgame_score = sSC1PStageClearScoreTotal;
}

// 0x80133F00
void sc1PStageClearUpdateBonusScore(void)
{
	sc1PStageClearMakeScoreSObjs();
	func_800269C0_275C0(nSYAudioFGMScoreDisplayBonus);
}

// 0x80133F28
sb32 sc1PStageClearCheckNoTimer(void)
{
	if (gSCManagerSceneData.spgame_time_limit == SCBATTLE_TIMELIMIT_INFINITE)
	{
		return TRUE;
	}
	else return FALSE;
}

// 0x80133F50
void sc1PStageUpdateBonusStatAll(void)
{
	s32 i;

	if (sSC1PStageClearCommonAdvanceTic == sSC1PStageClearTotalTimeTics)
	{
		for (i = 0; i < ARRAY_COUNT(sSC1PStageClearBonusStatGObjs); i++)
		{
			if (sSC1PStageClearBonusStatGObjs[i] != NULL)
			{
				gcEjectGObj(sSC1PStageClearBonusStatGObjs[i]);
			}
		}
		sSC1PStageClearScoreTotal += sc1PStageClearGetUpdateBonusStatPointsAll();

		if (sSC1PStageClearScoreTotal < 0)
		{
			sSC1PStageClearScoreTotal = 0;
		}
		sSC1PStageClearBonusShowNextTic = (sSC1PStageClearBonusNum * 10) + sSC1PStageClearTotalTimeTics + 20;
	}
	else if (sSC1PStageClearBonusShowNextTic == sSC1PStageClearTotalTimeTics)
	{
		gcEjectGObj(sSC1PStageClearScoreTextGObj);
		sc1PStageClearUpdateBonusScore();

		sSC1PStageClearIsSetCommonAdvanceTic = TRUE;

		if (sSC1PStageClearIsAdvance != FALSE)
		{
			sSC1PStageClearBonusAdvanceTic = sSC1PStageClearTotalTimeTics + 20;
		}
	}
	else if (sSC1PStageClearBonusAdvanceTic == sSC1PStageClearTotalTimeTics)
	{
		sSC1PStageClearIsAllowProceedNext = TRUE;
	}
}

// 0x8013407C
void sc1PStageClearUpdateGameClearScore(void)
{
	s32 unused;
	f32 y;

	if (sc1PStageClearCheckNoTimer() == FALSE)
	{
		if (sSC1PStageClearTotalTimeTics == sSC1PStageClearTimerTextTic)
		{
			sc1PStageClearMakeTimerTextSObjs(94.0F);
		}
		if (sSC1PStageClearTotalTimeTics == sSC1PStageClearTimerDigitTic)
		{
			sc1PStageClearMakeTimerDigits(94.0F);
		}
		if (sSC1PStageClearTotalTimeTics == sSC1PStageClearTimerMultiplierTic)
		{
			gcEjectGObj(sSC1PStageClearTimerMultiplierGObj);
			sSC1PStageClearScoreTotal += sc1PStageClearGetAppendTotalTimeScore(94.0F);
		}
		if (sSC1PStageClearTotalTimeTics == sSC1PStageClearTimerEjectTic)
		{
			gcEjectGObj(sSC1PStageClearScoreTextGObj);
			sc1PStageClearUpdateBonusScore();
		}
	}
	y = (sc1PStageClearCheckNoTimer() == FALSE) ? 126.0F : 94.0F;

	if (sSC1PStageClearTotalTimeTics == sSC1PStageClearDamageTextTic)
	{
		sc1PStageClearMakeDamageTextSObjs(y);
	}
	if (sSC1PStageClearTotalTimeTics == sSC1PStageClearDamageDigitTic)
	{
		sc1PStageClearMakeDamageDigits(y);
	}
	if (sSC1PStageClearTotalTimeTics == sSC1PStageClearDamageMultiplierTic)
	{
		gcEjectGObj(sSC1PStageClearDamageMultiplierGObj);
		sSC1PStageClearScoreTotal += sc1PStageClearGetAppendTotalDamageScore(y);
	}
	if (sSC1PStageClearTotalTimeTics == sSC1PStageClearDamageEjectTic)
	{
		gcEjectGObj(sSC1PStageClearScoreTextGObj);
		sc1PStageClearUpdateBonusScore();

		if (sSC1PStageClearIsHaveBonusStats != FALSE)
		{
			sSC1PStageClearCommonAdvanceTic =
			sSC1PStageClearBonusShowNextTic =
			sSC1PStageClearBonusAdvanceTic 	= sSC1PStageClearTotalTimeTics + 10;
		}
		else sSC1PStageClearIsAllowProceedNext = TRUE;
	}
	if
	(
		(sSC1PStageClearIsHaveBonusStats != FALSE) 	&&
		(sSC1PStageClearBonusTextGObj != NULL) 		&&
		(sSC1PStageClearCommonAdvanceTic == sSC1PStageClearTotalTimeTics)
	)
	{
		gcEjectGObj(sSC1PStageClearBonusTextGObj);
		sSC1PStageClearBonusTextGObj = NULL;

		if (sSC1PStageClearTimerTextGObj != NULL)
		{
			gcEjectGObj(sSC1PStageClearTimerTextGObj);
			sSC1PStageClearTimerTextGObj = NULL;

			gcEjectGObj(sSC1PStageClearTimerMultiplierGObj);
			sSC1PStageClearTimerMultiplierGObj = NULL;
		}
		gcEjectGObj(sSC1PStageClearDamageTextGObj);
		sSC1PStageClearDamageTextGObj = NULL;

		gcEjectGObj(sSC1PStageClearDamageMultiplierGObj);
		sSC1PStageClearDamageMultiplierGObj = NULL;

		sc1PStageClearMakeBonusTable();
	}
	if (sSC1PStageClearIsHaveBonusStats != FALSE)
	{
		sc1PStageUpdateBonusStatAll();
	}
}

// 0x80134340 - ??? Exactly the same as the function above ???
void sc1PStageClearUpdateStageClearScore(void)
{
	s32 unused;
	f32 y;

	if (sc1PStageClearCheckNoTimer() == FALSE)
	{
		if (sSC1PStageClearTotalTimeTics == sSC1PStageClearTimerTextTic)
			sc1PStageClearMakeTimerTextSObjs(94.0F);

		if (sSC1PStageClearTotalTimeTics == sSC1PStageClearTimerDigitTic)
			sc1PStageClearMakeTimerDigits(94.0F);

		if (sSC1PStageClearTotalTimeTics == sSC1PStageClearTimerMultiplierTic)
		{
			gcEjectGObj(sSC1PStageClearTimerMultiplierGObj);
			sSC1PStageClearScoreTotal += sc1PStageClearGetAppendTotalTimeScore(94.0F);
		}
		if (sSC1PStageClearTotalTimeTics == sSC1PStageClearTimerEjectTic)
		{
			gcEjectGObj(sSC1PStageClearScoreTextGObj);
			sc1PStageClearUpdateBonusScore();
		}
	}
	y = (sc1PStageClearCheckNoTimer() == FALSE) ? 126.0F : 94.0F;

	if (sSC1PStageClearTotalTimeTics == sSC1PStageClearDamageTextTic)
	{
		sc1PStageClearMakeDamageTextSObjs(y);
	}
	if (sSC1PStageClearTotalTimeTics == sSC1PStageClearDamageDigitTic)
	{
		sc1PStageClearMakeDamageDigits(y);
	}
	if (sSC1PStageClearTotalTimeTics == sSC1PStageClearDamageMultiplierTic)
	{
		gcEjectGObj(sSC1PStageClearDamageMultiplierGObj);
		sSC1PStageClearScoreTotal += sc1PStageClearGetAppendTotalDamageScore(y);
	}
	if (sSC1PStageClearTotalTimeTics == sSC1PStageClearDamageEjectTic)
	{
		gcEjectGObj(sSC1PStageClearScoreTextGObj);
		sc1PStageClearUpdateBonusScore();

		if (sSC1PStageClearIsHaveBonusStats != FALSE)
		{
			sSC1PStageClearCommonAdvanceTic =
			sSC1PStageClearBonusShowNextTic =
			sSC1PStageClearBonusAdvanceTic 	= sSC1PStageClearTotalTimeTics + 10;
		}
		else sSC1PStageClearIsAllowProceedNext = TRUE;
	}
	if
	(
		(sSC1PStageClearIsHaveBonusStats != FALSE)	&&
		(sSC1PStageClearBonusTextGObj != NULL) 		&& 
		(sSC1PStageClearCommonAdvanceTic == sSC1PStageClearTotalTimeTics)
	)
	{
		gcEjectGObj(sSC1PStageClearBonusTextGObj);
		sSC1PStageClearBonusTextGObj = NULL;

		if (sSC1PStageClearTimerTextGObj != NULL)
		{
			gcEjectGObj(sSC1PStageClearTimerTextGObj);
			sSC1PStageClearTimerTextGObj = NULL;

			gcEjectGObj(sSC1PStageClearTimerMultiplierGObj);
			sSC1PStageClearTimerMultiplierGObj = NULL;
		}
		gcEjectGObj(sSC1PStageClearDamageTextGObj);
		sSC1PStageClearDamageTextGObj = NULL;

		gcEjectGObj(sSC1PStageClearDamageMultiplierGObj);
		sSC1PStageClearDamageMultiplierGObj = NULL;

		sc1PStageClearMakeBonusTable();
	}
	if (sSC1PStageClearIsHaveBonusStats != FALSE)
	{
		sc1PStageUpdateBonusStatAll();
	}
}

// 0x80134604
void sc1PStageClearUpdateResultScore(void)
{
	s32 i;

	if (sSC1PStageClear1PGameStage != nSC1PGameStageBonus3)
	{
		if (sSC1PStageClearTotalTimeTics == 10)
		{
			sc1PStageClearMakeTargetTextSObjs();
		}
		else if (sSC1PStageClearTotalTimeTics == 20)
		{
			func_ovl56_80132F78();
			sSC1PStageClearBaseIntervalTic = (sSC1PStageClearBonusObjectivesCleared * 10) + sSC1PStageClearTotalTimeTics;
		}
	}
	else if (sSC1PStageClearTotalTimeTics == 10)
	{
		sSC1PStageClearBaseIntervalTic = sSC1PStageClearTotalTimeTics;
	}
	if (sSC1PStageClearBaseIntervalTic != 0)
	{
		if ((sc1PStageClearCheckNoTimer() != FALSE) && (sSC1PStageClear1PGameStage != nSC1PGameStageBonus3))
		{
			if (sSC1PStageClearBaseIntervalTic == sSC1PStageClearTotalTimeTics)
			{
				if (sSC1PStageClearIsHaveBonusStats != FALSE)
				{
					sSC1PStageClearCommonAdvanceTic =
					sSC1PStageClearBonusShowNextTic =
					sSC1PStageClearBonusAdvanceTic  = sSC1PStageClearTotalTimeTics + 10;
				}
				else sSC1PStageClearIsAllowProceedNext = TRUE;
			}
		}
		else if (sSC1PStageClearTotalTimeTics == (sSC1PStageClearBaseIntervalTic + 10))
		{
			if (sSC1PStageClear1PGameStage == nSC1PGameStageBonus3)
			{
				sc1PStageClearMakeTimerTextSObjs(94.0F);
			}
			else if (sSC1PStageClearBonusObjectivesCleared == ARRAY_COUNT(sSC1PStageClearBonusObjectiveGObjs))
			{
				sc1PStageClearMakeTimerTextSObjs(126.0F);
			}		
		}
		else if (sSC1PStageClearTotalTimeTics == (sSC1PStageClearBaseIntervalTic + 30))
		{
			if (sSC1PStageClear1PGameStage == nSC1PGameStageBonus3)
			{
				sc1PStageClearMakeTimerDigits(94.0F);
			}
			else if (sSC1PStageClearBonusObjectivesCleared == 10)
			{
				sc1PStageClearMakeTimerDigits(126.0F);
			}
		}
		else if (sSC1PStageClearTotalTimeTics == (sSC1PStageClearBaseIntervalTic + 50))
		{
			if (sSC1PStageClear1PGameStage == nSC1PGameStageBonus3)
			{
				gcEjectGObj(sSC1PStageClearTimerMultiplierGObj);
				sSC1PStageClearScoreTotal += sc1PStageClearGetAppendTotalTimeScore(94.0F);

			}
			else if (sSC1PStageClearBonusObjectivesCleared == ARRAY_COUNT(sSC1PStageClearBonusObjectiveGObjs))
			{
				gcEjectGObj(sSC1PStageClearTimerMultiplierGObj);
				sSC1PStageClearScoreTotal += sc1PStageClearGetAppendTotalTimeScore(126.0F);
			}
		}
		else if (sSC1PStageClearTotalTimeTics == (sSC1PStageClearBaseIntervalTic + 70))
		{
			if
			(
				(sSC1PStageClearBonusObjectivesCleared == ARRAY_COUNT(sSC1PStageClearBonusObjectiveGObjs)) || 
				(sSC1PStageClear1PGameStage == nSC1PGameStageBonus3)
			)
			{
				gcEjectGObj(sSC1PStageClearScoreTextGObj);
				sc1PStageClearUpdateBonusScore();
			}
			if (sSC1PStageClearIsHaveBonusStats != FALSE)
			{
				sSC1PStageClearCommonAdvanceTic =
				sSC1PStageClearBonusShowNextTic =
				sSC1PStageClearBonusAdvanceTic  = sSC1PStageClearTotalTimeTics + 10;
			}
			else sSC1PStageClearIsAllowProceedNext = TRUE;
		}
	}
	if
	(
		(sSC1PStageClearIsHaveBonusStats != FALSE) 	&&
		(sSC1PStageClearBonusTextGObj != NULL) 		&&
		(sSC1PStageClearCommonAdvanceTic == sSC1PStageClearTotalTimeTics)
	)
	{
		gcEjectGObj(sSC1PStageClearBonusTextGObj);
		sSC1PStageClearBonusTextGObj = NULL;

		if (sSC1PStageClearTargetGObj != NULL)
		{
			gcEjectGObj(sSC1PStageClearTargetGObj);
			sSC1PStageClearTargetGObj = NULL;

			for (i = 0; i < ARRAY_COUNT(sSC1PStageClearBonusObjectiveGObjs); i++)
			{
				if (sSC1PStageClearBonusObjectiveGObjs[i] != NULL)
				{
					gcEjectGObj(sSC1PStageClearBonusObjectiveGObjs[i]);
					sSC1PStageClearBonusObjectiveGObjs[i] = NULL;
				}
			}
		}
		if (sSC1PStageClearTimerTextGObj != NULL)
		{
			gcEjectGObj(sSC1PStageClearTimerTextGObj);
			sSC1PStageClearTimerTextGObj = NULL;

			gcEjectGObj(sSC1PStageClearTimerMultiplierGObj);
			sSC1PStageClearTimerMultiplierGObj = NULL;
		}
		sc1PStageClearMakeBonusTable();
	}
	if (sSC1PStageClearIsHaveBonusStats != FALSE)
	{
		sc1PStageUpdateBonusStatAll();
	}
}

// 0x801349F0
void sc1PStageClearFuncRun(GObj *gobj)
{
	sSC1PStageClearTotalTimeTics++;

	if (sSC1PStageClearTotalTimeTics >= 10)
	{
		if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
		{
			if (sSC1PStageClearIsAllowProceedNext != FALSE)
			{
				gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
				gSCManagerSceneData.scene_curr = nSCKindTitle;

				sc1PStageClearUpdateTotal1PGameScore();
				
				syTaskmanSetLoadScene();
			}
			else if ((sSC1PStageClearIsSetCommonAdvanceTic != FALSE) && (sSC1PStageClearIsAdvance == FALSE))
			{
				sSC1PStageClearCommonAdvanceTic = sSC1PStageClearTotalTimeTics;
			}
		}
		switch (sSC1PStageClearKind)
		{
		case nSC1PStageClearKindGame:
			sc1PStageClearUpdateGameClearScore();
			break;

		case nSC1PStageClearKindStage:
			sc1PStageClearUpdateStageClearScore();
			break;

		case nSC1PStageClearKindResult:
			sc1PStageClearUpdateResultScore();
			break;
		}
	}
}

/* sc1pstageclear.c:2118-2162 0x80134AF4, the same job on different
 * pixels.
 *
 * DIVERGES, and it is the one real piece of porting in this file. The
 * game reads 150 u32 a row straight out of gSYSchedulerCurrentFramebuffer
 * -- the 320x240 RGBA5551 frame the match left behind, less the ten-pixel
 * border -- and writes them into file 26's bitmap, swapping the two
 * halves of each u32 on odd rows for the N64's interlaced field order
 * and stepping the destination on every sixth row for the sprite's own
 * padding.
 *
 * Here nothing is copied. The frame loop rendered the match's last
 * frame into a texture (taskman.h syTaskmanGetPhoto, asked for by
 * src/dc/sc1pmanager.c before each stage), and the Sprite is laid over
 * that texture as it stands: 640x480 texels at half scale, so it covers
 * the game's 320x240 one texel to one framebuffer pixel. It is read back
 * through a render-to-texture photo, not vram_s.
 *
 * Called from FuncStart, as the game calls it. No photo leaves the
 * Sprite without a bitmap, and sc1PStageClearMakeWallpaper then draws
 * nothing -- which is also what a direct boot into this scene gets. */
void sc1PStageClearCopyFramebufToWallpaper(void)
{
	s32 w, h;
	void *photo = syTaskmanGetPhoto(&w, &h);

	sSC1PStageClearWallpaperSprite.bitmap = NULL;

	if (photo == NULL)
	{
		syDebugPrintf("sc1PStageClear: no exit photo for the wallpaper\n");
		return;
	}
	sSC1PStageClearWallpaperTex.txr = photo;
	sSC1PStageClearWallpaperTex.texw = SC1PSTAGECLEAR_WP_TEXW;
	sSC1PStageClearWallpaperTex.texh = SC1PSTAGECLEAR_WP_TEXH;
	sSC1PStageClearWallpaperTex.imgw = w;
	sSC1PStageClearWallpaperTex.imgh = h;
	sSC1PStageClearWallpaperTex.fmt = nDCSpriteTexFmtRGB565Stride;

	sSC1PStageClearWallpaperBitmap.width = w;
	sSC1PStageClearWallpaperBitmap.width_img = w;
	sSC1PStageClearWallpaperBitmap.s = 0;
	sSC1PStageClearWallpaperBitmap.t = 0;
	sSC1PStageClearWallpaperBitmap.buf = &sSC1PStageClearWallpaperTex;
	sSC1PStageClearWallpaperBitmap.actualHeight = h;
	sSC1PStageClearWallpaperBitmap.LUToffset = 0;

	sSC1PStageClearWallpaperSprite.width = w;
	sSC1PStageClearWallpaperSprite.height = h;
	sSC1PStageClearWallpaperSprite.scalex = (f32)GS_SCREEN_WIDTH_DEFAULT / w;
	sSC1PStageClearWallpaperSprite.scaley = (f32)GS_SCREEN_HEIGHT_DEFAULT / h;
	sSC1PStageClearWallpaperSprite.attr = SP_TRANSPARENT;
	sSC1PStageClearWallpaperSprite.red = 0xFF;
	sSC1PStageClearWallpaperSprite.green = 0xFF;
	sSC1PStageClearWallpaperSprite.blue = 0xFF;
	sSC1PStageClearWallpaperSprite.alpha = 0xFF;
	sSC1PStageClearWallpaperSprite.istep = 1;
	sSC1PStageClearWallpaperSprite.nbitmaps = 1;
	sSC1PStageClearWallpaperSprite.bmheight = h;
	sSC1PStageClearWallpaperSprite.bmHreal = h;
	sSC1PStageClearWallpaperSprite.bmfmt = G_IM_FMT_RGBA;
	sSC1PStageClearWallpaperSprite.bmsiz = G_IM_SIZ_16b;
	sSC1PStageClearWallpaperSprite.bitmap = &sSC1PStageClearWallpaperBitmap;
}

/* DIVERGES. sc1pstageclear.c:2168-2180 is the reloc loader's setup and
 * its load of the seven files; the port's are banks, one per entry of
 * dSC1PStageClearFileIDs, and the seventh is not loaded at all (that
 * table says why). Split out of FuncStart under the name the other
 * ported sc1pmode/ scenes use (src/dc/sc1pintro.c sc1PIntroLoadFiles).
 *
 * A bank that will not load leaves its slot NULL, which is what the
 * game's own load-failure path leaves: the sprites that came out of it
 * are then absent rather than fatal, and the loader has already named
 * the missing file. */
void sc1PStageClearLoadFiles(void)
{
	static const char *const paths[ARRAY_COUNT(dSC1PStageClearFileIDs)] =
	{
		SC1PSTAGECLEAR_BANK_TEXT,
		SC1PSTAGECLEAR_BANK_SCORE,
		SC1PSTAGECLEAR_BANK_DAMAGE,
		SC1PSTAGECLEAR_BANK_TIMER,
		SC1PSTAGECLEAR_BANK_DIGITS,
		SC1PSTAGECLEAR_BANK_OBJECTS
	};
	s32 i;

	for (i = 0; i < ARRAY_COUNT(dSC1PStageClearFileIDs); i++)
	{
		sSC1PStageClearFiles[i] = NULL;

		if (sprite_bank_load(&sSC1PStageClearBanks[i], paths[i]) < 0)
		{
			syDebugPrintf("sc1PStageClear: no bank for file %d (%s)\n",
			              (int)dSC1PStageClearFileIDs[i], paths[i]);
			continue;
		}
		sSC1PStageClearFiles[i] = &sSC1PStageClearBanks[i];
	}
}

// 0x80134CC4
void sc1PStageClearFuncStart(void)
{
	sc1PStageClearLoadFiles();
	gcMakeGObjSPAfter(0, sc1PStageClearFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
	sc1PStageClearCopyFramebufToWallpaper();
	/* CUT: gcMakeDefaultCameraGObj. Unlike most of this port's copies of
	 * that cut, the flag word here is 0 -- no COBJ_FLAG_FILLCOLOR -- so
	 * the camera was never painting anything to begin with; it is the
	 * N64's frame and z setup, which the PVR does itself. */
	sc1PStageClearInitVars();
	sc1PStageClearMakeTextCamera();
	sc1PStageClearMakeWallpaperCamera();
	sc1PStageClearMakeWallpaper();
	sc1PStageClearMakeTextSObjs();
	sc1PStageClearMakeScoreSObjs();

	switch (sSC1PStageClear1PGameStage)
	{
	case nSC1PGameStageBoss:
		syAudioPlayBGM(0, nSYAudioBGM1PGameClear);
		break;

	case nSC1PGameStageBonus1:
	case nSC1PGameStageBonus2:
		if (gSCManagerSceneData.bonus_tasks_complete == 10)
		{
			syAudioPlayBGM(0, nSYAudioBGM1PBonusStageClear);
		}
		else syAudioPlayBGM(0, nSYAudioBGM1PBonusStageFailure);
		break;

	case nSC1PGameStageBonus3:
		if (gSCManagerSceneData.spgame_time_remain != 0)
		{
			syAudioPlayBGM(0, nSYAudioBGM1PBonusStageClear);
		}
		else syAudioPlayBGM(0, nSYAudioBGM1PBonusStageFailure);
		break;

	default:
		syAudioPlayBGM(0, nSYAudioBGM1PStageClear);
		break;
	}
}

/* CUT: dGM1PStageClearVideoSetup (0x801351EC), the N64's video mode --
 * sc1PStageClearStartScene says why. */

// 0x80135208
SYTaskmanSetup dGM1PStageClearTaskmanSetup =
{
    // Task Manager Buffer Setup
    {
        0,                          // ???
        gcRunAll,              		// Update function
        scManagerFuncDraw,          // Frame draw function
        /* the decomp names &ovl56_BSS_END here; a NULL arena_start keeps
         * the region syTaskmanMakeGeneralHeap already made, which is
         * what every ported scene's setup carries (src/dc/taskman.c) */
        NULL,                       // Allocatable memory pool start
        0,                          // Allocatable memory pool size
        1,                          // ???
        2,                          // Number of contexts?
        0, 0, 0, 0,                 // the four DL buffer sizes
        0,                          // Graphics Heap Size
        2,                          // ???
        0,                          // RDP Output Buffer Size
        /* CUT: sc1PStageClearFuncLights -- see its own note above. */
        NULL,                       // Pre-render function
        syControllerFuncRead,       // Controller I/O function
    },

    0,                              // Number of GObjThreads
    sizeof(u64) * 192,              // Thread stack size
    0,                              // Number of thread stacks
    0,                              // ???
    SC1PSTAGECLEAR_GOBJPROCS,
    SC1PSTAGECLEAR_GOBJS,   sizeof(GObj),
    SC1PSTAGECLEAR_XOBJS,
    /* the decomp names dLBCommonFuncMatrixList here; the port has no
     * such table (the matrix kinds are a switch in objdisplay.c), the
     * same NULL every other ported scene's setup carries */
    NULL,                           // Matrix function list
    NULL,                           // DObjVec eject function
    SC1PSTAGECLEAR_AOBJS,
    SC1PSTAGECLEAR_MOBJS,
    SC1PSTAGECLEAR_DOBJS,   sizeof(DObj),
    SC1PSTAGECLEAR_SOBJS,   sizeof(SObj),
    SC1PSTAGECLEAR_COBJS,   sizeof(CObj),

    sc1PStageClearFuncStart         // Task start function
};

// 0x80134E84
void sc1PStageClearStartScene(void)
{
	/* CUT: syVideoInit and the z-buffer it is handed. The port's video
	 * mode is the PVR's, set once at boot; and the arena line below it
	 * reads a link map the ELF does not have --
	 * dGM1PStageClearTaskmanSetup carries NULL for arena_start instead.
	 * Both cuts are src/dc/scvsbattle.c's, for its reasons. */
	scManagerFuncUpdate(&dGM1PStageClearTaskmanSetup);
}

/* The port's own bzero arm for dSCManagerOverlays[56] (src/dc/overlay.c).
 * The banks' records are syTaskmanMalloc'd out of the scene heap and go
 * with it, so the SpriteBank statics have to be cleared or the next
 * visit thinks they are still loaded. The wallpaper's three records go
 * with them; its texture is the frame loop's exit photo, which the frame
 * loop frees (taskman.h). */
void sc1PStageClearOverlayLoad(void)
{
	OVERLAY_CLEAR(sSC1PStageClearBanks);
	OVERLAY_CLEAR(sSC1PStageClearFiles);
	OVERLAY_CLEAR(sSC1PStageClearWallpaperTex);
	OVERLAY_CLEAR(sSC1PStageClearWallpaperBitmap);
	OVERLAY_CLEAR(sSC1PStageClearWallpaperSprite);
	OVERLAY_CLEAR(sSC1PStageClearTotalTimeTics);
	OVERLAY_CLEAR(sSC1PStageClearUnused0x801352D0);
	OVERLAY_CLEAR(sSC1PStageClearKind);
	OVERLAY_CLEAR(sSC1PStageClearIsHaveBonusStats);
	OVERLAY_CLEAR(sSC1PStageClearSecondsRemain);
	OVERLAY_CLEAR(sSC1PStageClearDamageDealt);
	OVERLAY_CLEAR(sSC1PStageClearDifficulty);
	OVERLAY_CLEAR(sSC1PStageClearScoreTotal);
	OVERLAY_CLEAR(sSC1PStageClear1PGameStage);
	OVERLAY_CLEAR(sSC1PStageClearTimerTextGObj);
	OVERLAY_CLEAR(sSC1PStageClearTimerMultiplierGObj);
	OVERLAY_CLEAR(sSC1PStageClearDamageTextGObj);
	OVERLAY_CLEAR(sSC1PStageClearDamageMultiplierGObj);
	OVERLAY_CLEAR(sSC1PStageClearScoreTextGObj);
	OVERLAY_CLEAR(sSC1PStageClearBonusTextGObj);
	OVERLAY_CLEAR(sSC1PStageClearTargetGObj);
	OVERLAY_CLEAR(sSC1PStageClearBonusFlags);
	OVERLAY_CLEAR(sSC1PStageClearBonusID);
	OVERLAY_CLEAR(sSC1PStageClearBonusNum);
	OVERLAY_CLEAR(sSC1PStageClearIsSetCommonAdvanceTic);
	OVERLAY_CLEAR(sSC1PStageClearIsAdvance);
	OVERLAY_CLEAR(sSC1PStageClearIsAllowProceedNext);
	OVERLAY_CLEAR(sSC1PStageClearCommonAdvanceTic);
	OVERLAY_CLEAR(sSC1PStageClearBonusShowNextTic);
	OVERLAY_CLEAR(sSC1PStageClearBonusAdvanceTic);
	OVERLAY_CLEAR(sSC1PStageClearBonusStatGObjs);
	OVERLAY_CLEAR(sSC1PStageClearBonusObjectiveGObjs);
	OVERLAY_CLEAR(sSC1PStageClearBaseIntervalTic);
	OVERLAY_CLEAR(sSC1PStageClearBonusObjectivesCleared);
	OVERLAY_CLEAR(sSC1PStageClearTimerTextTic);
	OVERLAY_CLEAR(sSC1PStageClearTimerDigitTic);
	OVERLAY_CLEAR(sSC1PStageClearTimerMultiplierTic);
	OVERLAY_CLEAR(sSC1PStageClearTimerEjectTic);
	OVERLAY_CLEAR(sSC1PStageClearDamageTextTic);
	OVERLAY_CLEAR(sSC1PStageClearDamageDigitTic);
	OVERLAY_CLEAR(sSC1PStageClearDamageMultiplierTic);
	OVERLAY_CLEAR(sSC1PStageClearDamageEjectTic);
	OVERLAY_CLEAR(sSC1PStageClearPad0x801352C0);
	OVERLAY_CLEAR(sSC1PStageClearPad0x801352F0);
	OVERLAY_CLEAR(sSC1PStageClearPad0x80135308);
	OVERLAY_CLEAR(sSC1PStageClearPad0x8013531C);
	OVERLAY_CLEAR(sSC1PStageClearPad0x801353C8);
}
