/* mnsoundtest.c -- see mnsoundtest.h. Every function is mn/mndata/mnsoundtest.c's
 * by name and body, REGION_US arms; the line numbers are the decomp's. */
#include "mnsoundtest.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"
#include "bgm.h"

#include <sys/debug.h>
#include <sys/controller.h>
#include <sys/rdp.h>
#include <sc/scdef.h>
#include <sc/scsubsys/scsubsys.h>
#include <gm/gmsound.h>
#include <lb/lbdef.h>
#include <mn/mndef.h>
#include <PR/os.h>
#include <macros.h>

/* n_env.c's start-a-voice-script and stop-every-FGM-at-once, which no
 * decomp header declares by these names; src/dc/syaudio.c defines both
 * over the FGM engine (src/dc/mndata.c and src/dc/mnvsrecord.c declare
 * the first the same way; src/dc/mntitle.c declares both). */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);
void func_800266A0_272A0(void);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mndata.c does the same). Each `&llXxxSprite` below is
 * written `llXxxSprite`, the number, with the label's name kept as the
 * macro's. The values are relocData file 0xc4 (MNSoundTest, this
 * screen's own) as tools/export/ssb_spriteexport.py --list reads them off the
 * ROM; files 0xc5 (IFCommonBattlePause), 0xa4 (IFCommonPlayerDamage) and
 * 0x0 (MNCommon) reuse banks the battle HUD and the menus already put on
 * the disc (ifpause.spr, ifdamage.spr, mncommon.spr --
 * src/game/ssb64/Makefile); file 0x20 (MNDataCommon) reuses the bank the
 * Records tab put there (src/dc/mnvsrecord.c). */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

#define llIFCommonBattlePauseDecalAButtonSprite  0x00958
#define llIFCommonBattlePauseDecalBButtonSprite  0x00a88

#define llIFCommonPlayerDamageDigit0Sprite       0x00148
#define llIFCommonPlayerDamageDigit1Sprite       0x002d8
#define llIFCommonPlayerDamageDigit2Sprite       0x00500
#define llIFCommonPlayerDamageDigit3Sprite       0x00698
#define llIFCommonPlayerDamageDigit4Sprite       0x008c0
#define llIFCommonPlayerDamageDigit5Sprite       0x00a58
#define llIFCommonPlayerDamageDigit6Sprite       0x00c80
#define llIFCommonPlayerDamageDigit7Sprite       0x00e18
#define llIFCommonPlayerDamageDigit8Sprite       0x01040
#define llIFCommonPlayerDamageDigit9Sprite       0x01270

#define llMNDataCommonDataHeaderSprite           0x00b40

#define llMNCommonArrowLSprite                   0x0de30
#define llMNCommonArrowRSprite                   0x0dd90

#define llMNSoundTestMusicTextSprite             0x00438
#define llMNSoundTestSoundTextSprite             0x009c0
#define llMNSoundTestVoiceTextSprite             0x00e48
#define llMNSoundTestCapsuleRightSprite          0x01138
#define llMNSoundTestColonExitTextSprite         0x01208
#define llMNSoundTestColonFadeOutTextSprite      0x01348
#define llMNSoundTestColonPlayTextSprite         0x01450
#define llMNSoundTestSoundTestTextSprite         0x01bb8
#define llMNSoundTestStartButtonSprite           0x01d50

/* The five files' banks. The last three are whole-file exports already
 * on the disc for other scenes; this screen just opens them again
 * (tools/check/disc_order_check.py's comment says why that is cheaper than a
 * second copy). */
#define MNSOUNDTEST_BANK_MAIN       "mnsoundtest.spr"
#define MNSOUNDTEST_BANK_DAMAGE     "ifdamage.spr"
#define MNSOUNDTEST_BANK_DATACOMMON "mndatacommon.spr"
#define MNSOUNDTEST_BANK_COMMON     "mncommon.spr"
#define MNSOUNDTEST_BANK_PAUSE      "ifpause.spr"

// // // // // // // // // // // //
//                               //
//             MACROS            //
//                               //
// // // // // // // // // // // //

#define mnSoundTestCheckGetOptionButtonInput(is_button, mask) \
mnCommonCheckGetOptionButtonInput(sMNSoundTestOptionChangeWait, is_button, mask)

#define mnSoundTestCheckGetOptionStickInputUD(stick_range, min, b) \
mnCommonCheckGetOptionStickInputUD(sMNSoundTestOptionChangeWait, stick_range, min, b)

#define mnSoundTestCheckGetOptionStickInputLR(stick_range, min, b) \
mnCommonCheckGetOptionStickInputLR(sMNSoundTestOptionChangeWait, stick_range, min, b)

#define mnSoundTestSetOptionChangeWaitP(is_button, stick_range, div) \
mnCommonSetOptionChangeWaitP(sMNSoundTestOptionChangeWait, is_button, stick_range, div)

#define mnSoundTestSetOptionChangeWaitN(is_button, stick_range, div) \
mnCommonSetOptionChangeWaitN(sMNSoundTestOptionChangeWait, is_button, stick_range, div)

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mnsoundtest.c:40-87 0x801339E0, verbatim: every music sequence ID the
 * game names, Music column order. */
u32 dMNSoundTestMusicIDs[/* */] =
{
    nSYAudioBGMOpening, nSYAudioBGMExplain, nSYAudioBGMData,
    nSYAudioBGMModeSelect, nSYAudioBGMCastle, nSYAudioBGMJungle,
    nSYAudioBGMHyrule, nSYAudioBGMZebes, nSYAudioBGMYoster,
    nSYAudioBGMPupupu, nSYAudioBGMSector, nSYAudioBGMYamabuki,
    nSYAudioBGMInishie, nSYAudioBGMInishieHurry, nSYAudioBGMWinMario,
    nSYAudioBGMWinDonkey, nSYAudioBGMWinZelda, nSYAudioBGMWinMetroid,
    nSYAudioBGMWinYoshi, nSYAudioBGMWinKirby, nSYAudioBGMWinFox,
    nSYAudioBGMWinPMonsters, nSYAudioBGMWinFZero, nSYAudioBGMWinMother,
    nSYAudioBGMResults, nSYAudioBGMHammer, nSYAudioBGMStar,
    nSYAudioBGMTrainingMode, nSYAudioBGM1PIntro, nSYAudioBGMBossStage,
    nSYAudioBGMBossEntry, nSYAudioBGMLast, nSYAudioBGM1PBonusStage,
    nSYAudioBGM1PStageClear, nSYAudioBGM1PGameClear,
    nSYAudioBGM1PBonusStageClear, nSYAudioBGM1PBonusStageFailure,
    nSYAudioBGMZako, nSYAudioBGMMetal, nSYAudioBGM1PChallenger,
    nSYAudioBGMMessage, nSYAudioBGMEnding, nSYAudioBGM1PGameEndChoice,
    nSYAudioBGM1PGameOver, nSYAudioBGMStaffroll
};

/* mnsoundtest.c:89-286 0x80133A94, verbatim: every FGM sound effect ID
 * the game names, Sound column order. */
u32 dMNSoundTestSoundIDs[/* */] =
{
    nSYAudioFGMOpeningSectorAmbient, nSYAudioFGMOpeningNewcomersClash,
    nSYAudioFGMPublicPrologue, nSYAudioFGMOpeningBatM,
    nSYAudioFGMAltitudeWarn, nSYAudioFGMDeadExplodeL,
    nSYAudioFGMDeadExplodeS, nSYAudioFGMKickL, nSYAudioFGMKickM,
    nSYAudioFGMKickS, nSYAudioFGMPunchL, nSYAudioFGMPunchM,
    nSYAudioFGMPunchS, nSYAudioFGMLightSwingL, nSYAudioFGMLightSwingM,
    nSYAudioFGMLightSwingS, nSYAudioFGMShockL, nSYAudioFGMShockS,
    nSYAudioFGMBurnL, nSYAudioFGMBurnS, nSYAudioFGMDonkeyLanding,
    nSYAudioFGMUnkGrind2, nSYAudioFGMKirbyPurinJump, nSYAudioFGMDonkeyFoot,
    nSYAudioFGMSamusFoot, nSYAudioFGMMMarioFoot, nSYAudioFGMNessDash,
    nSYAudioFGMGroundBrakeGrind, nSYAudioFGMGuardOn, nSYAudioFGMGuardOff,
    nSYAudioFGMShieldBreak, nSYAudioFGMDonkeyDeadSlam,
    nSYAudioFGMYoshiDownBounce, nSYAudioFGMCharacterUnkZip1,
    nSYAudioFGMHeavySwing1, nSYAudioFGMLightSwingLw1, nSYAudioFGMCatch,
    nSYAudioFGMDeadUpStar, nSYAudioFGMEscape, nSYAudioFGMMSBombAttach,
    nSYAudioFGMBombHeiFuse, nSYAudioFGMItemMapCollide,
    nSYAudioFGMBumperHit, nSYAudioFGMFireFlowerBurn, nSYAudioFGMItemGet,
    nSYAudioFGMHammerSwing, nSYAudioFGMHarisenHit, nSYAudioFGMBatHit,
    nSYAudioFGMStarMapCollide, nSYAudioFGMStarGet,
    nSYAudioFGMBombHeiWalkStart, nSYAudioFGMShellHit, nSYAudioFGMItemThrow,
    nSYAudioFGMItemSpawn1, nSYAudioFGMContainerSmash,
    nSYAudioFGMFireFlowerShoot, nSYAudioFGMLGunShoot, nSYAudioFGMLGunEmpty,
    nSYAudioFGMStarRodSwing4, nSYAudioFGMStarRodSwing1,
    nSYAudioFGMStarRodEmpty, nSYAudioFGMSwordSwing4, nSYAudioFGMSwordSwing1,
    nSYAudioFGMTaruBombHit, nSYAudioFGMTaruBombMap, nSYAudioFGMExplodeL,
    nSYAudioFGMFireShoot1, nSYAudioFGMShockML, nSYAudioFGMMarioAppealGrow,
    nSYAudioFGMMarioAppealShrink, nSYAudioFGMUnkDial1,
    nSYAudioFGMMarioSpecialN, nSYAudioFGMExplodeS,
    nSYAudioFGMMarioSpecialHiJump, nSYAudioFGMMarioSpecialHiCoin,
    nSYAudioFGMMarioUnkSwing1, nSYAudioFGMBossSlam, nSYAudioFGMBossUnk1,
    nSYAudioFGMBossUnk2, nSYAudioFGMDonkeyCharge,
    nSYAudioFGMLinkSpecialLwGet, nSYAudioFGMLinkSpecialNReturn,
    nSYAudioFGMLinkSpecialNShoot, nSYAudioFGMLinkSpecialNGet,
    nSYAudioFGMLinkSpecialHi, nSYAudioFGMLinkCatchHookshot,
    nSYAudioFGMLinkAppear, nSYAudioFGMBladeSwing4, nSYAudioFGMBladeSwing3,
    nSYAudioFGMBladeSwing1, nSYAudioFGMSlashL, nSYAudioFGMSlashM,
    nSYAudioFGMSlashS, nSYAudioFGMBladeDraw, nSYAudioFGMChargeShotAll,
    nSYAudioFGMUnkSmallPing1, nSYAudioFGMFoxBlaster, nSYAudioFGMSamusJump1,
    nSYAudioFGMSamusSpecialNShootL, nSYAudioFGMSamusSpecialNShootS,
    nSYAudioFGMSamusSpecialNCharge0, nSYAudioFGMSamusSpecialNCharge7,
    nSYAudioFGMSamusSpecialLw, nSYAudioFGMSamusCatchGrappleBeam,
    nSYAudioFGMSamusSpecialHi, nSYAudioFGMSamusUnkSwing,
    nSYAudioFGMSamusUnkCharge, nSYAudioFGMYoshiEggShatter1,
    nSYAudioFGMYoshiSpecialNTongue, nSYAudioFGMYoshiEggShatter3,
    nSYAudioFGMYoshiSpecialHiThrow, nSYAudioFGMYoshiEggLayShatter,
    nSYAudioFGMUnkMechanical4, nSYAudioFGMUnkLongWind,
    nSYAudioFGMKirbySpecialLwLanding, nSYAudioFGMKirbyAttackAirHi,
    nSYAudioFGMKirbySpecialNThrow, nSYAudioFGMKirbySpecialNCopyEat,
    nSYAudioFGMKirbySpecialNCopyThrow, nSYAudioFGMKirbySpecialNCopyUnk,
    nSYAudioFGMKirbyStarPing2, nSYAudioFGMKirbySpecialLwStart,
    nSYAudioFGMKirbySpecialNStart, nSYAudioFGMKirbySpecialNLoseCopy,
    nSYAudioFGMFoxSpecialN, nSYAudioFGMFoxSpecialHiStart,
    nSYAudioFGMFoxSpecialHiFly, nSYAudioFGMFoxSpecialLwHit,
    nSYAudioFGMFoxSpecialLwStart, nSYAudioFGMFoxAttackAirLw,
    nSYAudioFGMFoxAppearArwing, nSYAudioFGMUnkShoot1,
    nSYAudioFGMPikachuElectric1, nSYAudioFGMPikachuElectric2,
    nSYAudioFGMPikachuElectric5, nSYAudioFGMPikachuElectricLoop,
    nSYAudioFGMPikachuSpecialHiStart, nSYAudioFGMPikachuSpecialLwThunder,
    nSYAudioFGMCaptainAppearCar1, nSYAudioFGMCaptainAppearCar2,
    nSYAudioFGMCaptainSpecialHi, nSYAudioFGMCaptainSpecialNStart,
    nSYAudioFGMCaptainSpecialNPunch, nSYAudioFGMCharacterUnk1,
    nSYAudioFGMNessPKThunderLoop, nSYAudioFGMNessSpecialLwStart,
    nSYAudioFGMCharacterUnk3, nSYAudioFGMUnkSwoosh1, nSYAudioFGMUnkGate1,
    nSYAudioFGMBossBullet, nSYAudioFGMSectorArwingLaser,
    nSYAudioFGMSectorAmbient1, nSYAudioFGMOptionBackupClear,
    nSYAudioFGMMagnify, nSYAudioFGMBonusComplete, nSYAudioFGMPlayerHeal,
    nSYAudioFGMYosterCloudVapor, nSYAudioFGMStockSteal,
    nSYAudioFGMBonus2PlatformLanding, nSYAudioFGMGamePause,
    nSYAudioFGMInishiePowerBlock, nSYAudioFGMBonus1TargetBreak,
    nSYAudioFGMJungleTaruCannShoot, nSYAudioFGMHyruleTwisterAppear,
    nSYAudioFGMHyruleTwisterTrapped, nSYAudioFGMPupupuWhispyWind,
    nSYAudioFGMFloorDamageFire, nSYAudioFGMDogasSmog,
    nSYAudioFGMIwarkRockMake, nSYAudioFGMKabigonFall,
    nSYAudioFGMKabigonJump, nSYAudioFGMKamexHydro, nSYAudioFGMLizardonFlame,
    nSYAudioFGMMewFly, nSYAudioFGMNyarsCoin, nSYAudioFGMMBallOpen,
    nSYAudioFGMMonsterShoot, nSYAudioFGMTosakintoSplash,
    nSYAudioFGMUnkMechanical1, nSYAudioFGMTitlePressStart,
    nSYAudioFGMMenuSelect, nSYAudioFGMStageSelect,
    nSYAudioFGM1PGameContinue, nSYAudioFGMTrainingSel2,
    nSYAudioFGMMenuScroll1, nSYAudioFGMMenuScroll2, nSYAudioFGMMenuDenied,
    nSYAudioFGMPlayerSlotClose, nSYAudioFGMPlayerSlotWhoosh,
    nSYAudioFGMScoreDisplayBonus, nSYAudioFGMStageClearScoreRegister,
    nSYAudioFGMStageClearScoreDisplay, nSYAudioFGMDoorClose,
    nSYAudioFGMTrainingSel
};

/* mnsoundtest.c:288-585 0x80133D9C, verbatim: every voice line ID the
 * game names, Voice column order. */
u32 dMNSoundTestVoiceIDs[/* */] =
{
    // MARIO
    nSYAudioVoiceMarioSmash1, nSYAudioVoiceMarioSmash2,
    nSYAudioVoiceMarioSmash3, nSYAudioVoiceMarioSpecialLw,
    nSYAudioVoiceMarioDeadUp, nSYAudioVoiceMarioJump,
    nSYAudioVoiceMarioJumpAerial, nSYAudioVoiceMarioHeavyGet,
    nSYAudioVoiceMarioDead, nSYAudioVoiceMarioDamage,
    nSYAudioVoiceMarioHereWe,

    // DONKEY KONG
    nSYAudioVoiceDonkeyAppeal, nSYAudioVoiceDonkeySmash1,
    nSYAudioVoiceDonkeySmash2, nSYAudioVoiceDonkeySmash3,
    nSYAudioVoiceDonkeyDeadUp, nSYAudioVoiceDonkeyDamage,
    nSYAudioVoiceDonkeyDead1, nSYAudioVoiceDonkeyHeavyGet,
    nSYAudioVoiceDonkeyHeavyUnk, nSYAudioVoiceDonkeyDead2,

    // LINK
    nSYAudioVoiceLinkSmash1, nSYAudioVoiceLinkSmash2,
    nSYAudioVoiceLinkSmash3, nSYAudioVoiceLinkSpecialHi,
    nSYAudioVoiceLinkDeadUp, nSYAudioVoiceLinkDamage, nSYAudioVoiceLinkJump,
    nSYAudioVoiceLinkJumpAerial, nSYAudioVoiceLinkOttotto,
    nSYAudioVoiceLinkDead, nSYAudioVoiceLinkGrunt2,

    // YOSHI
    nSYAudioVoiceYoshiAppeal, nSYAudioVoiceYoshiSmash2,
    nSYAudioVoiceYoshiSmash3, nSYAudioVoiceYoshiCatch,
    nSYAudioVoiceYoshiDeadUp, nSYAudioVoiceYoshiDamage,
    nSYAudioVoiceYoshiJump, nSYAudioVoiceYoshiJumpAerial,
    nSYAudioVoiceYoshiFuraSleep, nSYAudioVoiceYoshiSpecialLwJump,
    nSYAudioVoiceYoshiSpecialLwFall, nSYAudioVoiceYoshiUnkGrunt2,
    nSYAudioVoiceYoshiThrow, nSYAudioVoiceYoshiUnkVocalize,

    // KIRBY
    nSYAudioVoiceKirbyAppeal, nSYAudioVoiceKirbySmash1,
    nSYAudioVoiceKirbySmash2, nSYAudioVoiceKirbySmash3,
    nSYAudioVoiceKirbyCopyLinkSpecialN, nSYAudioVoiceKirbyCopyPikachuSpecialN,
    nSYAudioVoiceKirbySpecialHi, nSYAudioVoiceKirbyCopyCaptainSpecialNFalcon,
    nSYAudioVoiceKirbyCopyCaptainSpecialNPunch,
    nSYAudioVoiceKirbyCopyDonkeySpecialN, nSYAudioVoiceKirbyCopyPurinSpecialN,
    nSYAudioVoiceKirbyDeadUp, nSYAudioVoiceKirbyFuraFura,
    nSYAudioVoiceKirbyDamage, nSYAudioVoiceKirbyHeavyGet,
    nSYAudioVoiceKirbyOttotto, nSYAudioVoiceKirbyCopyNessSpecialN,
    nSYAudioVoiceKirbyDead, nSYAudioVoiceKirbyFuraSleep,
    nSYAudioVoiceKirbySpecialLw,

    // FOX
    nSYAudioVoiceFoxDeadUp, nSYAudioVoiceFoxSpecialHi,
    nSYAudioVoiceFoxJumpAerial, nSYAudioVoiceFoxEscape,
    nSYAudioVoiceFoxSelected, nSYAudioVoiceFoxHeavyGet,
    nSYAudioVoiceFoxOttotto, nSYAudioVoiceFoxDead, nSYAudioVoiceFoxSmash1,
    nSYAudioVoiceFoxSmash2, nSYAudioVoiceFoxSmash3, nSYAudioVoiceFoxDamage,
    nSYAudioVoiceFoxFuraFura,

    // PIKACHU
    nSYAudioVoicePikachuAppeal, nSYAudioVoicePikachuSmash1,
    nSYAudioVoicePikachuSmash2, nSYAudioVoicePikachuSmash3,
    nSYAudioVoicePikachuSpecialN, nSYAudioVoicePikachuSpecialLw,
    nSYAudioVoicePikachuDeadUp, nSYAudioVoicePikachuDamage,
    nSYAudioVoicePikachuSpecialHi, nSYAudioVoicePikachuHeavyGet,
    nSYAudioVoicePikachuOttotto, nSYAudioVoicePikachuDead,
    nSYAudioVoicePikachuFuraSleep,

    // LUIGI
    nSYAudioVoiceLuigiSmash1, nSYAudioVoiceLuigiSmash2,
    nSYAudioVoiceLuigiSmash3, nSYAudioVoiceLuigiSpecialLw,
    nSYAudioVoiceLuigiDeadUp, nSYAudioVoiceLuigiFuraFura,
    nSYAudioVoiceLuigiDamage, nSYAudioVoiceLuigiJump,
    nSYAudioVoiceLuigiJumpAerial, nSYAudioVoiceLuigiHeavyGet,
    nSYAudioVoiceLuigiDead, nSYAudioVoiceLuigiHereWe,

    // CAPTAIN FALCON
    nSYAudioVoiceCaptainAppeal, nSYAudioVoiceCaptainSpecialHi,
    nSYAudioVoiceCaptainSmash1, nSYAudioVoiceCaptainSmash2,
    nSYAudioVoiceCaptainSmash3, nSYAudioVoiceCaptainSmash5,
    nSYAudioVoiceCaptainAttackS4, nSYAudioVoiceCaptainSpecialLw,
    nSYAudioVoiceCaptainSpecialNFalcon, nSYAudioVoiceCaptainSpecialNPunch,
    nSYAudioVoiceCaptainDeadUp, nSYAudioVoiceCaptainFuraFura,
    nSYAudioVoiceCaptainDamage, nSYAudioVoiceCaptainJumpAerial,
    nSYAudioVoiceCaptainHeavyGet, nSYAudioVoiceCaptainDead,
    nSYAudioVoiceCaptainFuraSleep, nSYAudioVoiceCaptainUnkQuick,

    // NESS
    nSYAudioVoiceNessAppeal, nSYAudioVoiceNessSmash1,
    nSYAudioVoiceNessSmash2, nSYAudioVoiceNessSmash3,
    nSYAudioVoiceNessUnkGrunt, nSYAudioVoiceNessDeadUp,
    nSYAudioVoiceNessFuraFura, nSYAudioVoiceNessDamage,
    nSYAudioVoiceNessHeavyGet, nSYAudioVoiceNessOttotto,
    nSYAudioVoiceNessSpecialN, nSYAudioVoiceNessSpecialHi,
    nSYAudioVoiceNessDead, nSYAudioVoiceNessFuraSleep,

    // JIGGLYPUFF
    nSYAudioVoicePurinAppeal, nSYAudioVoicePurinSmash1,
    nSYAudioVoicePurinSmash2, nSYAudioVoicePurinSmash3,
    nSYAudioVoicePurinSpecialN, nSYAudioVoicePurinDeadUp,
    nSYAudioVoicePurinFuraFura, nSYAudioVoicePurinDamage,
    nSYAudioVoicePurinUnkGrunt2, nSYAudioVoicePurinUnkGrunt3,
    nSYAudioVoicePurinUnkGrunt4, nSYAudioVoicePurinFuraSleep,
    nSYAudioVoicePurinSpecialLwSleep, nSYAudioVoicePurinSpecialLwWake,
    nSYAudioVoicePurinSpecialHi,

    // MASTER HAND
    nSYAudioVoiceBossAppear, nSYAudioVoiceBossDead,

    // ANNOUNCER
    nSYAudioVoiceAnnounceTitleWait, nSYAudioVoiceAnnounceMario,
    nSYAudioVoiceAnnounceDonkey, nSYAudioVoiceAnnounceSamus,
    nSYAudioVoiceAnnounceFox, nSYAudioVoiceAnnounceYoshi,
    nSYAudioVoiceAnnounceLink, nSYAudioVoiceAnnouncePikachu,
    nSYAudioVoiceAnnounceKirby, nSYAudioVoiceAnnounceLuigi,
    nSYAudioVoiceAnnounceCaptain, nSYAudioVoiceAnnounceNess,
    nSYAudioVoiceAnnouncePurin, nSYAudioVoiceAnnounceRedTeam,
    nSYAudioVoiceAnnounceBlueTeam, nSYAudioVoiceAnnounceGreenTeam,
    nSYAudioVoiceAnnounceFreeForAll, nSYAudioVoiceAnnounceTeamBattle,
    nSYAudioVoiceAnnounceSelectPlayer, nSYAudioVoiceAnnounceContinue,
    nSYAudioVoiceAnnounceGameOver, nSYAudioVoiceAnnounceGo,
    nSYAudioVoiceAnnounceFive, nSYAudioVoiceAnnounceFour,
    nSYAudioVoiceAnnounceThree, nSYAudioVoiceAnnounceTwo,
    nSYAudioVoiceAnnounceOne, nSYAudioVoiceAnnounceSuddenDeath,
    nSYAudioVoiceAnnounceTimeUp, nSYAudioVoiceAnnounceGameSet,
    nSYAudioVoiceAnnounceWinnerIs, nSYAudioVoiceAnnounceNoContest,
    nSYAudioVoiceAnnouncePlayer1, nSYAudioVoiceAnnouncePlayer2,
    nSYAudioVoiceAnnouncePlayer3, nSYAudioVoiceAnnouncePlayer4,
    nSYAudioVoiceAnnounceComputerPlayer, nSYAudioVoiceAnnounceVersus,
    nSYAudioVoiceAnnounceYoshiTeam, nSYAudioVoiceAnnounceKirbyTeam,
    nSYAudioVoiceAnnounceGDonkey, nSYAudioVoiceAnnounceMarioBros,
    nSYAudioVoiceAnnounceMMario, nSYAudioVoiceAnnounceZako,
    nSYAudioVoiceAnnounceBonusStage, nSYAudioVoiceAnnounceBreakTheTargets,
    nSYAudioVoiceAnnounceBoardThePlatforms, nSYAudioVoiceAnnounceComplete,
    nSYAudioVoiceAnnounceFailure, nSYAudioVoiceAnnounceNewRecord,
    nSYAudioVoiceAnnounceTrainingMode, nSYAudioVoiceAnnounceHowToPlay,

    // POKéBALL POKéMON
    nSYAudioVoiceMBallDogasAppear, nSYAudioVoiceMBallIwarkAppear,
    nSYAudioVoiceMBallKabigonFall, nSYAudioVoiceMBallKabigonAppear,
    nSYAudioVoiceMBallKamexAppear, nSYAudioVoiceMBallLuckyAppear,
    nSYAudioVoiceMBallMewAppear, nSYAudioVoiceMBallPippiAppear,
    nSYAudioVoiceMBallLizardonAppear, nSYAudioVoiceMBallSawamuraAppear,
    nSYAudioVoiceMBallSawamuraKick, nSYAudioVoiceMBallSpearAppear,
    nSYAudioVoiceMBallSpearSwarm, nSYAudioVoiceMBallStarmieAppear,
    nSYAudioVoiceMBallTosakintoAppear,

    // SAFFRON CITY POKéMON
    nSYAudioVoiceYamabukiFushigibana, nSYAudioVoiceYamabukiHitokage,
    nSYAudioVoiceYamabukiLucky,         // No Electrode?
    nSYAudioVoiceYamabukiPorygon,

    // AUDIENCE CHANTS
    nSYAudioVoicePublicDonkey, nSYAudioVoicePublicCaptain,
    nSYAudioVoicePublicFox, nSYAudioVoicePublicKirby,
    nSYAudioVoicePublicLink, nSYAudioVoicePublicLuigi,
    nSYAudioVoicePublicMario, nSYAudioVoicePublicNess,
    nSYAudioVoicePublicPikachu, nSYAudioVoicePublicPurin,
    nSYAudioVoicePublicSamus, nSYAudioVoicePublicYoshi,

    // AUDIENCE REACTIONS
    nSYAudioVoicePublicGaspL, nSYAudioVoicePublicGaspS,
    nSYAudioVoicePublicCheer, nSYAudioVoicePublicGaspClap,
    nSYAudioVoicePublicDamageL, nSYAudioVoicePublicDamageS,
    nSYAudioVoicePublicAbsorb, nSYAudioVoicePublicClapS
};

/* mnsoundtest.c:587-595 0x8013416C. Used only for ARRAY_COUNT and the
 * load-failure message; mnSoundTestLoadFiles below is what actually
 * reaches the disc (mnvsrecord.c's dMNVSRecordFileIDs does the same). */
u32 dMNSoundTestFileIDs[/* */] =
{
    0xc5,   /* &llIFCommonBattlePauseFileID */
    0xa4,   /* &llIFCommonPlayerDamageFileID */
    0x20,   /* &llMNDataCommonFileID */
    0x0,    /* &llMNCommonFileID */
    0xc4    /* &llMNSoundTestFileID */
};

/* mnsoundtest.c:597-603 0x80134180, verbatim. */
f32 dMNSoundTestArrowSpritePositions[/* */] =
{
    162.0F,  73.0F, 224.0F,
    181.0F, 121.0F, 243.0F,
    201.0F, 168.0F, 263.0F
};

/* mnsoundtest.c:605-618 0x801341A4, verbatim, renamed to this file's own
 * `ll...` offsets. */
intptr_t dMNSoundTestDigitSpriteOffsets[/* */] =
{
    llIFCommonPlayerDamageDigit0Sprite, llIFCommonPlayerDamageDigit1Sprite,
    llIFCommonPlayerDamageDigit2Sprite, llIFCommonPlayerDamageDigit3Sprite,
    llIFCommonPlayerDamageDigit4Sprite, llIFCommonPlayerDamageDigit5Sprite,
    llIFCommonPlayerDamageDigit6Sprite, llIFCommonPlayerDamageDigit7Sprite,
    llIFCommonPlayerDamageDigit8Sprite, llIFCommonPlayerDamageDigit9Sprite
};

/* mnsoundtest.c:620-621 0x801341CC, verbatim: only the first ten (digits
 * 0-9) are ever read (mnSoundTestUpdateNumberSprites indexes number %
 * 10); the last two are the decomp's own dead tail. */
s32 dMNSoundTestDigitSpriteWidths[/* */] = { 14, 9, 15, 14, 15, 13, 15, 14, 15, 15, 17, 20 };

/* mnsoundtest.c:624-632 dMNSoundTestLights1 and dMNSoundTestDisplayList,
 * the lighting the pre-render function would set for the 3D this scene
 * never draws. Dropped with mnSoundTestFuncLights, as every other menu
 * scene's is (src/dc/mndata.c, src/dc/mnvsrecord.c). */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* mnsoundtest.c:689 sMNSoundTestPad0x80134300[2] and :695
 * sMNSoundTestPad0x8013430C, the overlay's own link-map holes. Dropped:
 * the port has no link map to keep a hole in (src/dc/mnvsrecord.c drops
 * its own the same way). */

/* mnsoundtest.c:692 */
s32 sMNSoundTestOption;

/* mnsoundtest.c:698-704 */
s32 sMNSoundTestOptionColorR[nMNSoundTestOptionEnumCount];
s32 sMNSoundTestOptionColorG[nMNSoundTestOptionEnumCount];
s32 sMNSoundTestOptionColorB[nMNSoundTestOptionEnumCount];

/* mnsoundtest.c:707 */
s32 sMNSoundTestOptionChangeWait;

/* mnsoundtest.c:710 */
s32 sMNSoundTestDirectionInputKind;

/* mnsoundtest.c:713 sMNSoundTestPad0x80134344, another link-map hole.
 * Dropped, as above. */

/* mnsoundtest.c:716 */
s32 sMNSoundTestOptionSelectID[nMNSoundTestOptionEnumCount];

/* mnsoundtest.c:719 */
f32 sMNSoundTestSelectIDPositionsX[nMNSoundTestOptionEnumCount];

/* mnsoundtest.c:722 */
s32 sMNSoundTestFadeOutWait;

/* mnsoundtest.c:725 sMNSoundTestStatusBuffer[32], the reloc loader's
 * per-file status records. Dropped with lbRelocInitSetup
 * (mnSoundTestLoadFiles, as src/dc/mnvsrecord.c drops its own). */

/* mnsoundtest.c:728 */
void *sMNSoundTestFiles[ARRAY_COUNT(dMNSoundTestFileIDs)];

/* the banks behind sMNSoundTestFiles */
static SpriteBank sMNSoundTestBanks[ARRAY_COUNT(dMNSoundTestFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mnsoundtest.c:737-766 0x80131B00, verbatim. */
void mnSoundTestUpdateOptionColors(void)
{
    s32 i;

    for
    (
        i = 0;
        i <
        (
            ARRAY_COUNT(sMNSoundTestOptionColorR) +
            ARRAY_COUNT(sMNSoundTestOptionColorG) +
            ARRAY_COUNT(sMNSoundTestOptionColorB)
        ) / 3;
        i++
    )
    {
        if (i == sMNSoundTestOption)
        {
            sMNSoundTestOptionColorR[i] = 0xFF;
            sMNSoundTestOptionColorG[i] = 0xA8;
            sMNSoundTestOptionColorB[i] = 0x00;
        }
        else
        {
            sMNSoundTestOptionColorR[i] = 0x7D;
            sMNSoundTestOptionColorG[i] = 0x45;
            sMNSoundTestOptionColorB[i] = 0x07;
        }
    }
}

/* mnsoundtest.c:768-935 0x80131B80, verbatim. */
void mnSoundTestUpdateControllerInputs(void)
{
    s32 stick_range;
    sb32 is_button;

    if (sMNSoundTestOptionChangeWait != 0)
    {
        sMNSoundTestOptionChangeWait--;
    }
    if
    (
        (scSubsysControllerGetPlayerStickInRangeLR(-32, 32) != FALSE) &&
        (scSubsysControllerGetPlayerStickInRangeUD(-32, 32) != FALSE) &&
        (scSubsysControllerGetPlayerHoldButtons(U_JPAD | R_JPAD | R_TRIG | U_CBUTTONS | R_CBUTTONS) == FALSE) &&
        (scSubsysControllerGetPlayerHoldButtons(D_JPAD | L_JPAD | L_TRIG | D_CBUTTONS | L_CBUTTONS) == FALSE)
    )
    {
        sMNSoundTestOptionChangeWait = 0;
        sMNSoundTestDirectionInputKind = 0;
    }
    if
    (
        mnSoundTestCheckGetOptionButtonInput(is_button, U_JPAD | U_CBUTTONS) ||
        mnSoundTestCheckGetOptionStickInputUD(stick_range, 32, 1)
    )
    {
        func_800269C0_275C0(nSYAudioFGMMenuScroll2);

        mnSoundTestSetOptionChangeWaitP(is_button, stick_range, 8);

        sMNSoundTestOption--;

        if (sMNSoundTestOption < nMNSoundTestOptionStart)
        {
            sMNSoundTestOption = nMNSoundTestOptionEnd;
        }
        if (sMNSoundTestOption == nMNSoundTestOptionStart)
        {
            sMNSoundTestOptionChangeWait += 10;
        }
        sMNSoundTestDirectionInputKind = 3;
    }
    if
    (
        mnSoundTestCheckGetOptionButtonInput(is_button, D_JPAD | D_CBUTTONS) ||
        mnSoundTestCheckGetOptionStickInputUD(stick_range, -32, 0)
    )
    {
        func_800269C0_275C0(nSYAudioFGMMenuScroll2);

        mnSoundTestSetOptionChangeWaitN(is_button, stick_range, 8);

        sMNSoundTestOption++;

        if (sMNSoundTestOption > nMNSoundTestOptionEnd)
        {
            sMNSoundTestOption = nMNSoundTestOptionStart;
        }
        if (sMNSoundTestOption == nMNSoundTestOptionEnd)
        {
            sMNSoundTestOptionChangeWait += 10;
        }
        sMNSoundTestDirectionInputKind = 4;
    }
    if
    (
        mnSoundTestCheckGetOptionButtonInput(is_button, L_JPAD | L_TRIG | L_CBUTTONS) ||
        mnSoundTestCheckGetOptionStickInputLR(stick_range, -32, 0)
    )
    {
        mnSoundTestSetOptionChangeWaitN(is_button, stick_range, 16);

        sMNSoundTestOptionSelectID[sMNSoundTestOption]--;

        switch (sMNSoundTestOption)
        {
        case nMNSoundTestOptionMusic:
            if (sMNSoundTestOptionSelectID[sMNSoundTestOption] < 0)
            {
                sMNSoundTestOptionSelectID[sMNSoundTestOption] = (ARRAY_COUNT(dMNSoundTestMusicIDs) - 1);
            }
            if (sMNSoundTestOptionSelectID[sMNSoundTestOption] == 0)
            {
                sMNSoundTestOptionChangeWait += 20;
            }
            break;

        case nMNSoundTestOptionSound:
            if (sMNSoundTestOptionSelectID[sMNSoundTestOption] < 0)
            {
                sMNSoundTestOptionSelectID[sMNSoundTestOption] = (ARRAY_COUNT(dMNSoundTestSoundIDs) - 1);
            }
            if (sMNSoundTestOptionSelectID[sMNSoundTestOption] == 0)
            {
                sMNSoundTestOptionChangeWait += 20;
            }
            break;

        case nMNSoundTestOptionVoice:
            if (sMNSoundTestOptionSelectID[sMNSoundTestOption] < 0)
            {
                sMNSoundTestOptionSelectID[sMNSoundTestOption] = (ARRAY_COUNT(dMNSoundTestVoiceIDs) - 1);
            }
            if (sMNSoundTestOptionSelectID[sMNSoundTestOption] == 0)
            {
                sMNSoundTestOptionChangeWait += 20;
            }
            break;
        }
        if (sMNSoundTestDirectionInputKind != 1)
        {
            sMNSoundTestOptionChangeWait *= 2;
        }
        sMNSoundTestDirectionInputKind = 1;
    }
    if
    (
        mnSoundTestCheckGetOptionButtonInput(is_button, R_JPAD | R_TRIG | R_CBUTTONS) ||
        mnSoundTestCheckGetOptionStickInputLR(stick_range, 32, 1)
    )
    {
        mnSoundTestSetOptionChangeWaitP(is_button, stick_range, 16);

        sMNSoundTestOptionSelectID[sMNSoundTestOption]++;

        switch (sMNSoundTestOption)
        {
        case nMNSoundTestOptionMusic:
            if (sMNSoundTestOptionSelectID[sMNSoundTestOption] > (ARRAY_COUNT(dMNSoundTestMusicIDs) - 1))
            {
                sMNSoundTestOptionSelectID[sMNSoundTestOption] = 0;
            }
            if (sMNSoundTestOptionSelectID[sMNSoundTestOption] == (ARRAY_COUNT(dMNSoundTestMusicIDs) - 1))
            {
                sMNSoundTestOptionChangeWait += 20;
            }
            break;

        case nMNSoundTestOptionSound:
            if (sMNSoundTestOptionSelectID[sMNSoundTestOption] > (ARRAY_COUNT(dMNSoundTestSoundIDs) - 1))
            {
                sMNSoundTestOptionSelectID[sMNSoundTestOption] = 0;
            }
            if (sMNSoundTestOptionSelectID[sMNSoundTestOption] == (ARRAY_COUNT(dMNSoundTestSoundIDs) - 1))
            {
                sMNSoundTestOptionChangeWait += 20;
            }
            break;

        case nMNSoundTestOptionVoice:
            if (sMNSoundTestOptionSelectID[sMNSoundTestOption] > (ARRAY_COUNT(dMNSoundTestVoiceIDs) - 1))
            {
                sMNSoundTestOptionSelectID[sMNSoundTestOption] = 0;
            }
            if (sMNSoundTestOptionSelectID[sMNSoundTestOption] == (ARRAY_COUNT(dMNSoundTestVoiceIDs) - 1))
            {
                sMNSoundTestOptionChangeWait += 20;
            }
            break;
        }
        if (sMNSoundTestDirectionInputKind != 2)
        {
            sMNSoundTestOptionChangeWait *= 2;
        }
        sMNSoundTestDirectionInputKind = 2;
    }
}

/* mnsoundtest.c:937-989 0x801320B4, verbatim. */
void mnSoundTestUpdateFunctions(void)
{
    if (sMNSoundTestFadeOutWait != -1)
    {
        if (sMNSoundTestFadeOutWait != 0)
        {
            sMNSoundTestFadeOutWait--;
        }
        else
        {
            syAudioStopBGMAll();
            sMNSoundTestFadeOutWait = -1;
        }
    }
    else syAudioSetBGMVolume(0, 0x7000);

    if (scSubsysControllerGetPlayerTapButtons(A_BUTTON) != FALSE)
    {
        switch (sMNSoundTestOption)
        {
        case nMNSoundTestOptionMusic:
            if (sMNSoundTestFadeOutWait > 0)
            {
                sMNSoundTestFadeOutWait = -1;
            }
            syAudioStopBGMAll();
            syAudioPlayBGM(0, dMNSoundTestMusicIDs[sMNSoundTestOptionSelectID[nMNSoundTestOptionMusic]]);
            break;

        case nMNSoundTestOptionSound:
            func_800266A0_272A0();
            func_800269C0_275C0(dMNSoundTestSoundIDs[sMNSoundTestOptionSelectID[nMNSoundTestOptionSound]]);
            break;

        case nMNSoundTestOptionVoice:
            func_800266A0_272A0();
            func_800269C0_275C0(dMNSoundTestVoiceIDs[sMNSoundTestOptionSelectID[nMNSoundTestOptionVoice]]);
            break;
        }
    }
    else if (scSubsysControllerGetPlayerTapButtons(Z_TRIG) != FALSE)
    {
        syAudioStopBGMAll();
        func_800266A0_272A0();
    }
    else if (scSubsysControllerGetPlayerTapButtons(START_BUTTON) != FALSE)
    {
        syAudioSetBGMVolumeFade(0, 0, 120);
        sMNSoundTestFadeOutWait = 120;
        func_800266A0_272A0();
    }
}

/* mnsoundtest.c:991-1008 0x80132244, verbatim. */
void mnSoundTestFuncRun(GObj *gobj)
{
    (void)gobj;

    mnSoundTestUpdateOptionColors();

    if (scSubsysControllerGetPlayerTapButtons(B_BUTTON) != FALSE)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindData;

        syAudioStopBGMAll();
        func_800266A0_272A0();
        syAudioSetBGMVolume(0, 0x7000);
        syTaskmanSetLoadScene();
    }
    mnSoundTestUpdateControllerInputs();
    mnSoundTestUpdateFunctions();
}

/* mnsoundtest.c:1011-1026 lbRelocInitSetup and lbRelocLoadFilesListed, as
 * every other menu scene has them (src/dc/mndata.c, src/dc/mnvsrecord.c):
 * five sprite banks stand in for the five relocData files. The last
 * three are opened again out of banks the menus and the battle HUD
 * already put on the disc (mnsoundtest.h says why). */
static void mnSoundTestLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMNSoundTestFileIDs)] =
    {
        MNSOUNDTEST_BANK_PAUSE,
        MNSOUNDTEST_BANK_DAMAGE,
        MNSOUNDTEST_BANK_DATACOMMON,
        MNSOUNDTEST_BANK_COMMON,
        MNSOUNDTEST_BANK_MAIN
    };
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(dMNSoundTestFileIDs); i++)
    {
        if (sprite_bank_load(&sMNSoundTestBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mnSoundTest: no bank for file %d (%s)\n",
                          (int)dMNSoundTestFileIDs[i], paths[i]);
            sMNSoundTestFiles[i] = NULL;
            continue;
        }
        sMNSoundTestFiles[i] = &sMNSoundTestBanks[i];
    }
}

/* mnsoundtest.c:1028-1065 0x8013234C, verbatim. */
SObj *mnSoundTestMakeHeaderSObjs(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(1, NULL, 2, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[2], llMNDataCommonDataHeaderSprite));

    sobj->sprite.attr = SP_TRANSPARENT;

    sobj->sprite.red   = 0x5F;
    sobj->sprite.green = 0x58;
    sobj->sprite.blue  = 0x46;

    sobj->pos.x = 23.0F;
    sobj->pos.y = 17.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[4], llMNSoundTestSoundTestTextSprite));

    sobj->sprite.attr = SP_TRANSPARENT;

    sobj->sprite.red   = 0xF2;
    sobj->sprite.green = 0xC7;
    sobj->sprite.blue  = 0x0D;

    sobj->envcolor.r = 0x00;
    sobj->envcolor.g = 0x00;
    sobj->envcolor.b = 0x00;

    sobj->pos.x = 152.0F;
    sobj->pos.y = 23.0F;

    return sobj;
}

/* mnsoundtest.c:1067-1081 0x80132450, verbatim. */
void mnSoundTestOptionThreadUpdate(GObj *gobj)
{
    s32 color_id = gobj->user_data.s;
    SObj *sobj = SObjGetStruct(gobj);

    while (TRUE)
    {
        sobj->next->sprite.red   = sobj->sprite.red   = sMNSoundTestOptionColorR[color_id];
        sobj->next->sprite.green = sobj->sprite.green = sMNSoundTestOptionColorG[color_id];
        sobj->next->sprite.blue  = sobj->sprite.blue  = sMNSoundTestOptionColorB[color_id];

        gcSleepCurrentGObjThread(1);
    }
}

/* mnsoundtest.c:1083-1109 0x801324FC. DIVERGES: mnsoundtest.h says why
 * every gDPFillRectangle pair below is lbCommonSpriteFillRect instead,
 * at the selected option's own colour (src/dc/mnvsrecord.c's grid
 * functions are the same shape). N64 fill rectangles are lower-right
 * *inclusive*; the port's corner is exclusive, so both far coordinates
 * get +1 here and nowhere else changes. */
void mnSoundTestMusicProcDisplay(GObj *gobj)
{
    (void)gobj;

    lbCommonSpriteFillRect(10, 56, 113, 58,
                            sMNSoundTestOptionColorR[nMNSoundTestOptionMusic],
                            sMNSoundTestOptionColorG[nMNSoundTestOptionMusic],
                            sMNSoundTestOptionColorB[nMNSoundTestOptionMusic],
                            0xFF);
    lbCommonSpriteFillRect(10, 95, 113, 97,
                            sMNSoundTestOptionColorR[nMNSoundTestOptionMusic],
                            sMNSoundTestOptionColorG[nMNSoundTestOptionMusic],
                            sMNSoundTestOptionColorB[nMNSoundTestOptionMusic],
                            0xFF);
    lbCommonClearExternSpriteParams();
}

/* mnsoundtest.c:1111-1148 0x80132638, verbatim. */
SObj *mnSoundTestMakeMusicSObjs(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(1, NULL, 3, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    gobj->user_data.s = nMNSoundTestOptionMusic;

    gcAddGObjProcess(gobj, mnSoundTestOptionThreadUpdate, nGCProcessKindThread, 1);
    gcAddGObjDisplay
    (
        gcMakeGObjSPAfter(1, NULL, 3, GOBJ_PRIORITY_DEFAULT),
        mnSoundTestMusicProcDisplay,
        2,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[4], llMNSoundTestMusicTextSprite));
    sobj->sprite.attr = SP_TRANSPARENT;
    sobj->pos.x = 55.0F;
    sobj->pos.y = 61.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[4], llMNSoundTestCapsuleRightSprite));
    sobj->sprite.attr = SP_TRANSPARENT;
    sobj->pos.x = 112.0F;
    sobj->pos.y = 56.0F;

    return sobj;
}

/* mnsoundtest.c:1150-1176 0x80132758. DIVERGES: as mnSoundTestMusicProcDisplay. */
void mnSoundTestSoundProcDisplay(GObj *gobj)
{
    (void)gobj;

    lbCommonSpriteFillRect(10, 104, 133, 106,
                            sMNSoundTestOptionColorR[nMNSoundTestOptionSound],
                            sMNSoundTestOptionColorG[nMNSoundTestOptionSound],
                            sMNSoundTestOptionColorB[nMNSoundTestOptionSound],
                            0xFF);
    lbCommonSpriteFillRect(10, 143, 133, 145,
                            sMNSoundTestOptionColorR[nMNSoundTestOptionSound],
                            sMNSoundTestOptionColorG[nMNSoundTestOptionSound],
                            sMNSoundTestOptionColorB[nMNSoundTestOptionSound],
                            0xFF);
    lbCommonClearExternSpriteParams();
}

/* mnsoundtest.c:1178-1215 0x80132894, verbatim. */
SObj *mnSoundTestMakeSoundSObjs(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(1, NULL, 3, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mnSoundTestOptionThreadUpdate, nGCProcessKindThread, 1);

    gobj->user_data.s = nMNSoundTestOptionSound;

    gcAddGObjDisplay
    (
        gcMakeGObjSPAfter(1, NULL, 3, GOBJ_PRIORITY_DEFAULT),
        mnSoundTestSoundProcDisplay,
        2,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[4], llMNSoundTestSoundTextSprite));
    sobj->sprite.attr = SP_TRANSPARENT;
    sobj->pos.x = 64.0F;
    sobj->pos.y = 108.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[4], llMNSoundTestCapsuleRightSprite));
    sobj->sprite.attr = SP_TRANSPARENT;
    sobj->pos.x = 132.0F;
    sobj->pos.y = 104.0F;

    return sobj;
}

/* mnsoundtest.c:1217-1243 0x801329B8. DIVERGES: as mnSoundTestMusicProcDisplay. */
void mnSoundTestVoiceProcDisplay(GObj *gobj)
{
    (void)gobj;

    lbCommonSpriteFillRect(10, 152, 153, 154,
                            sMNSoundTestOptionColorR[nMNSoundTestOptionVoice],
                            sMNSoundTestOptionColorG[nMNSoundTestOptionVoice],
                            sMNSoundTestOptionColorB[nMNSoundTestOptionVoice],
                            0xFF);
    lbCommonSpriteFillRect(10, 191, 153, 193,
                            sMNSoundTestOptionColorR[nMNSoundTestOptionVoice],
                            sMNSoundTestOptionColorG[nMNSoundTestOptionVoice],
                            sMNSoundTestOptionColorB[nMNSoundTestOptionVoice],
                            0xFF);
    lbCommonClearExternSpriteParams();
}

/* mnsoundtest.c:1245-1282 0x80132AF4, verbatim. */
SObj *mnSoundTestMakeVoiceSObjs(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(1, NULL, 3, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mnSoundTestOptionThreadUpdate, nGCProcessKindThread, 1);

    gobj->user_data.s = nMNSoundTestOptionVoice;

    gcAddGObjDisplay
    (
        gcMakeGObjSPAfter(1, NULL, 3, GOBJ_PRIORITY_DEFAULT),
        mnSoundTestVoiceProcDisplay,
        2,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[4], llMNSoundTestVoiceTextSprite));
    sobj->sprite.attr = SP_TRANSPARENT;
    sobj->pos.x = 94.0F;
    sobj->pos.y = 156.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[4], llMNSoundTestCapsuleRightSprite));
    sobj->sprite.attr = SP_TRANSPARENT;
    sobj->pos.x = 152.0F;
    sobj->pos.y = 152.0F;

    return sobj;
}

/* mnsoundtest.c:1284-1303 0x80132C10, verbatim. */
SObj *mnSoundTestMakeAButtonSObj(GObj *gobj)
{
    SObj *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[0], llIFCommonBattlePauseDecalAButtonSprite));

    sobj->sprite.attr = SP_TRANSPARENT;

    sobj->sprite.red   = 0x6E;
    sobj->sprite.green = 0x77;
    sobj->sprite.blue  = 0x75;

    sobj->envcolor.r = 0x21;
    sobj->envcolor.g = 0x40;
    sobj->envcolor.b = 0x3A;

    sobj->pos.x = 55.0F;
    sobj->pos.y = 205.0F;

    return sobj;
}

/* mnsoundtest.c:1305-1324 0x80132C90, verbatim. */
SObj *mnSoundTestMakeBButtonSObj(GObj *gobj)
{
    SObj *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[0], llIFCommonBattlePauseDecalBButtonSprite));

    sobj->sprite.attr = SP_TRANSPARENT;

    sobj->sprite.red   = 0x6E;
    sobj->sprite.green = 0x77;
    sobj->sprite.blue  = 0x5D;

    sobj->envcolor.r = 0x29;
    sobj->envcolor.g = 0x37;
    sobj->envcolor.b = 0x16;

    sobj->pos.x = 218.0F;
    sobj->pos.y = 205.0F;

    return sobj;
}

/* mnsoundtest.c:1326-1345 0x80132D10, verbatim. */
SObj *mnSoundTestMakeStartButtonSObj(GObj *gobj)
{
    SObj *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[4], llMNSoundTestStartButtonSprite));

    sobj->sprite.attr = SP_TRANSPARENT;

    sobj->sprite.red   = 0x81;
    sobj->sprite.green = 0x6A;
    sobj->sprite.blue  = 0x62;

    sobj->envcolor.r = 0x3B;
    sobj->envcolor.g = 0x20;
    sobj->envcolor.b = 0x16;

    sobj->pos.x = 121.0F;
    sobj->pos.y = 205.0F;

    return sobj;
}

/* mnsoundtest.c:1347-1362 0x80132D90, verbatim. */
SObj *mnSoundTestMakeAFunctionSObj(GObj *gobj)
{
    SObj *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[4], llMNSoundTestColonPlayTextSprite));

    sobj->sprite.attr = SP_TRANSPARENT;

    sobj->sprite.red   = 0x73;
    sobj->sprite.green = 0x6B;
    sobj->sprite.blue  = 0x59;

    sobj->pos.x = 72.0F;
    sobj->pos.y = 208.0F;

    return sobj;
}

/* mnsoundtest.c:1364-1379 0x80132DF8, verbatim. */
SObj *mnSoundTestMakeStartFunctionSObj(GObj *gobj)
{
    SObj *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[4], llMNSoundTestColonFadeOutTextSprite));

    sobj->sprite.attr = SP_TRANSPARENT;

    sobj->sprite.red   = 0x73;
    sobj->sprite.green = 0x6B;
    sobj->sprite.blue  = 0x59;

    sobj->pos.x = 148.0F;
    sobj->pos.y = 208.0F;

    return sobj;
}

/* mnsoundtest.c:1381-1396 0x80132E60, verbatim. */
SObj *mnSoundTestMakeBFunctionSObj(GObj *gobj)
{
    SObj *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[4], llMNSoundTestColonExitTextSprite));

    sobj->sprite.attr = SP_TRANSPARENT;

    sobj->sprite.red   = 0x73;
    sobj->sprite.green = 0x6B;
    sobj->sprite.blue  = 0x59;

    sobj->pos.x = 235.0F;
    sobj->pos.y = 208.0F;

    return sobj;
}

/* mnsoundtest.c:1398-1410 0x80132EC8, verbatim. */
void mnSoundTestMakeButtonSObjs(void)
{
    GObj *gobj = gcMakeGObjSPAfter(1, NULL, 3, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    mnSoundTestMakeAButtonSObj(gobj);
    mnSoundTestMakeBButtonSObj(gobj);
    mnSoundTestMakeStartButtonSObj(gobj);
    mnSoundTestMakeAFunctionSObj(gobj);
    mnSoundTestMakeStartFunctionSObj(gobj);
    mnSoundTestMakeBFunctionSObj(gobj);
}

/* mnsoundtest.c:1412-1438 0x80132F50, verbatim. */
void mnSoundTestMakeNumberSObj(GObj *gobj)
{
    s32 i;

    for (i = 0; i < nMNSoundTestOptionEnumCount; i++)
    {
        SObj *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[1], dMNSoundTestDigitSpriteOffsets[0]));

        sobj->sprite.attr = SP_HIDDEN;

        switch (gobj->user_data.s)
        {
        case nMNSoundTestOptionMusic:
            sobj->pos.y = 67.0F;
            break;

        case nMNSoundTestOptionSound:
            sobj->pos.y = 115.0F;
            break;

        case nMNSoundTestOptionVoice:
            sobj->pos.y = 163.0F;
            break;
        }
    }
}

/* mnsoundtest.c:1440-1478 0x80133058, verbatim bar the dropped
 * `f32 unused[4];` local the decomp declares and never reads
 * (src/dc/mnvsrecord.c's mnVSRecordGetRanking drops its own dead local
 * the same way). */
void mnSoundTestUpdateNumberPositions(GObj *gobj, f32 width)
{
    f32 pos_x = 0.0F;
    s32 option = gobj->user_data.s;
    SObj *sobj = SObjGetStruct(gobj);
    SObj *rewind_sobj = sobj;

    while ((sobj != NULL) && (sobj->sprite.attr != SP_HIDDEN))
    {
        rewind_sobj = sobj;

        sobj = sobj->next;
    }
    sobj = rewind_sobj;

    while (sobj != NULL)
    {
        f32 uf = sobj->user_data.s;

        sobj->user_data.s = pos_x;

        pos_x += uf;

        sobj = sobj->prev;
    }
    sobj = SObjGetStruct(gobj);

    pos_x = sMNSoundTestSelectIDPositionsX[option] - (width * 0.5F);

    while ((sobj != NULL) && (sobj->sprite.attr != SP_HIDDEN))
    {
        sobj->pos.x = pos_x + sobj->user_data.s +
        ((option == nMNSoundTestOptionMusic) ? 171.0F : ((option == nMNSoundTestOptionSound) ? 190.0F : 210.0F));

        sobj = sobj->next;
    }
}

/* mnsoundtest.c:1480-1523 0x80133194, verbatim. */
void mnSoundTestUpdateNumberSprites(GObj *gobj)
{
    SObj *sobj = SObjGetStruct(gobj);
    f32 width = 0.0F;
    s32 option = gobj->user_data.s;
    s32 number = sMNSoundTestOptionSelectID[option] + 1;

    while (sobj != NULL)
    {
        sobj->sprite.attr = SP_HIDDEN;
        sobj = sobj->next;
    }
    sobj = SObjGetStruct(gobj);

    do
    {
        sobj->sprite = *lbRelocGetFileData(Sprite*, sMNSoundTestFiles[1], dMNSoundTestDigitSpriteOffsets[number % 10]);

        sobj->user_data.s = dMNSoundTestDigitSpriteWidths[number % 10];

        sobj->sprite.attr = SP_TRANSPARENT;

        sobj->sprite.red   = 0xFF;
        sobj->sprite.green = 0x00;
        sobj->sprite.blue  = 0x00;

        sobj->envcolor.r = 0x00;
        sobj->envcolor.g = 0x00;
        sobj->envcolor.b = 0x00;

        width += sobj->user_data.s;

        number *= 0.1F;

        if (number != 0)
        {
            sobj = sobj->next;
        }
    }
    while (number != 0);

    mnSoundTestUpdateNumberPositions(gobj, width);
}

/* mnsoundtest.c:1525-1543 0x80133304, verbatim. */
void mnSoundTestSelectIDThreadUpdate(GObj *gobj)
{
    s32 option = gobj->user_data.s;
    s32 number = -1;

    mnSoundTestMakeNumberSObj(gobj);

    while (TRUE)
    {
        if (number != sMNSoundTestOptionSelectID[option])
        {
            number = sMNSoundTestOptionSelectID[option];

            mnSoundTestUpdateNumberSprites(gobj);
        }
        gcSleepCurrentGObjThread(1);
    }
}

/* mnsoundtest.c:1545-1566 0x80133398, verbatim. */
void mnSoundTestMakeSelectIDGObjs(void)
{
    GObj *gobj = gcMakeGObjSPAfter(1, NULL, 5, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mnSoundTestSelectIDThreadUpdate, nGCProcessKindThread, 1);

    gobj->user_data.s = nMNSoundTestOptionMusic;

    gobj = gcMakeGObjSPAfter(1, NULL, 6, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mnSoundTestSelectIDThreadUpdate, nGCProcessKindThread, 1);

    gobj->user_data.s = nMNSoundTestOptionSound;

    gobj = gcMakeGObjSPAfter(1, NULL, 7, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mnSoundTestSelectIDThreadUpdate, nGCProcessKindThread, 1);

    gobj->user_data.s = nMNSoundTestOptionVoice;
}

/* mnsoundtest.c:1568-1603 0x801334BC, verbatim (the "// Really?" remark
 * is the decomp's own, kept as it is written there). */
void mnSoundTestArrowsThreadUpdate(GObj *gobj)
{
    SObj *sobj = SObjGetStruct(gobj);
    s32 arrow_toggle_wait = 30;
    s32 option = sMNSoundTestOption;
    s32 id;

    while (TRUE)
    {
        if (option != sMNSoundTestOption)
        {
            option = sMNSoundTestOption;

            arrow_toggle_wait = 30;

            gobj->flags = GOBJ_FLAG_NONE;
        }
        if (arrow_toggle_wait == 0)
        {
            arrow_toggle_wait = 30;

            gobj->flags ^= GOBJ_FLAG_HIDDEN;
        }
        arrow_toggle_wait--;

        id = sMNSoundTestOption * nMNSoundTestOptionEnumCount; // Really?

        sobj->pos.x = dMNSoundTestArrowSpritePositions[id + 0];
        sobj->pos.y = dMNSoundTestArrowSpritePositions[id + 1];
        sobj->next->pos.x = dMNSoundTestArrowSpritePositions[id + 2];
        sobj->next->pos.y = dMNSoundTestArrowSpritePositions[id + 1];

        gcSleepCurrentGObjThread(1);
    }
}

/* mnsoundtest.c:1605-1635 0x801335C8, verbatim. */
void mnSoundTestMakeArrowSObjs(void)
{
    GObj *gobj = gcMakeGObjSPAfter(1, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    SObj *sobj;

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mnSoundTestArrowsThreadUpdate, nGCProcessKindThread, 1);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[3], llMNCommonArrowLSprite));

    sobj->sprite.attr = SP_TRANSPARENT;

    sobj->pos.x = dMNSoundTestArrowSpritePositions[nMNSoundTestOptionStart + 0];
    sobj->pos.y = dMNSoundTestArrowSpritePositions[nMNSoundTestOptionStart + 1];

    sobj->sprite.red   = 0xFF;
    sobj->sprite.green = 0xC3;
    sobj->sprite.blue  = 0x26;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNSoundTestFiles[3], llMNCommonArrowRSprite));

    sobj->sprite.attr = SP_TRANSPARENT;

    sobj->pos.x = dMNSoundTestArrowSpritePositions[nMNSoundTestOptionStart + 2];
    sobj->pos.y = dMNSoundTestArrowSpritePositions[nMNSoundTestOptionStart + 3];

    sobj->sprite.red   = 0xFF;
    sobj->sprite.green = 0xC3;
    sobj->sprite.blue  = 0x26;
}

/* mnsoundtest.c:1637-1647 0x801336D8, verbatim. */
void mnSoundTestMakeAllSObjs(void)
{
    mnSoundTestMakeHeaderSObjs();
    mnSoundTestMakeMusicSObjs();
    mnSoundTestMakeSoundSObjs();
    mnSoundTestMakeVoiceSObjs();
    mnSoundTestMakeSelectIDGObjs();
    mnSoundTestMakeArrowSObjs();
    mnSoundTestMakeButtonSObjs();
}

/* mnsoundtest.c:1649-1694 0x80133728, verbatim: the sprite pass on
 * DL-link 1 and func_80017EC0's own pass on DL-link 2 (already ported,
 * src/dc/objdisplay.c -- src/dc/mnplayersvs.c calls it the same way). */
void mnSoundTestMakeCameras(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            2,
            NULL,
            4,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            30,
            COBJ_MASK_DLLINK(1),
            -1,
            0,
            1,
            0,
            1,
            0
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 630.0F, 470.0F);

    cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            2,
            NULL,
            4,
            GOBJ_PRIORITY_DEFAULT,
            func_80017EC0,
            50,
            COBJ_MASK_DLLINK(2),
            -1,
            0,
            1,
            0,
            1,
            0
        )
    );

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 630.0F, 470.0F);
}

/* mnsoundtest.c:1696-1714 0x80133858, verbatim. */
void mnSoundTestInitVars(void)
{
    sMNSoundTestOptionColorR[nMNSoundTestOptionMusic] = sMNSoundTestOptionColorR[nMNSoundTestOptionSound] = sMNSoundTestOptionColorR[nMNSoundTestOptionVoice] = 0x7D;
    sMNSoundTestOptionColorG[nMNSoundTestOptionMusic] = sMNSoundTestOptionColorG[nMNSoundTestOptionSound] = sMNSoundTestOptionColorG[nMNSoundTestOptionVoice] = 0x45;
    sMNSoundTestOptionColorB[nMNSoundTestOptionMusic] = sMNSoundTestOptionColorB[nMNSoundTestOptionSound] = sMNSoundTestOptionColorB[nMNSoundTestOptionVoice] = 0x07;

    sMNSoundTestOption = 0;
    sMNSoundTestOptionChangeWait = 0;
    sMNSoundTestDirectionInputKind = 0;

    sMNSoundTestOptionSelectID[nMNSoundTestOptionMusic] = sMNSoundTestOptionSelectID[nMNSoundTestOptionSound] = sMNSoundTestOptionSelectID[nMNSoundTestOptionVoice] = 0;

    sMNSoundTestSelectIDPositionsX[nMNSoundTestOptionMusic] = 26.5F;
    sMNSoundTestSelectIDPositionsX[nMNSoundTestOptionSound] = 26.5F;
    sMNSoundTestSelectIDPositionsX[nMNSoundTestOptionVoice] = 26.5F;

    sMNSoundTestFadeOutWait = -1;
}

/* mnsoundtest.c:1717-1725 0x801338F8. DIVERGES: the LBRelocSetup block is
 * mnSoundTestLoadFiles above, and gcMakeDefaultCameraGObj -- the black
 * clear camera on link 0 at DL priority 100 -- is the frame clear, which
 * the PVR does itself (the same cut as src/dc/mndata.c's and
 * src/dc/mnvsrecord.c's). */
void mnSoundTestFuncStart(void)
{
    gcMakeGObjSPAfter(0, mnSoundTestFuncRun, 1, GOBJ_PRIORITY_DEFAULT);

    mnSoundTestLoadFiles();
    mnSoundTestInitVars();
    mnSoundTestMakeAllSObjs();
    mnSoundTestMakeCameras();
}

/* mnsoundtest.c:1728-1731 mnSoundTestFuncLights: the scene's pre-render
 * function, one GBI command setting the scene's own single light for 3D
 * this scene has none of. Dropped, as every other menu scene's is
 * (src/dc/mndata.c, src/dc/mnvsrecord.c). */

/* mnsoundtest.c:635 dMNSoundTestVideoSetup is the N64's video mode: see
 * mnSoundTestStartScene. */

/* mnsoundtest.c:638-680 (0x8013425C). The pool counts are the game's, all
 * zero: a menu scene takes its objects straight from the scene heap.
 * DIVERGES as every other menu scene's: the arena is the port's region,
 * the draw is the scene manager's, and mnSoundTestFuncLights is dropped. */
SYTaskmanSetup dMNSoundTestTaskmanSetup =
{
    // Task Manager Buffer Setup
    {
        0,                              // ???
        gcRunAll,                       // Update function
        scManagerFuncDraw,              // Frame draw function
        NULL,                           // Allocatable memory pool start
        0,                              // Allocatable memory pool size
        1,                              // ???
        2,                              // Number of contexts?
        0, 0, 0, 0,                     // the four DL buffer sizes
        0,                              // Graphics Heap Size
        2,                              // ???
        0,                              // RDP Output Buffer Size
        NULL,                           // Pre-render function
        syControllerFuncRead,           // Controller I/O function
    },

    0,                                  // Number of GObjThreads
    sizeof(u64) * 192,                  // Thread stack size
    0,                                  // Number of thread stacks
    0,                                  // ???
    0,                                  // Number of GObjProcesses
    0,                                  // Number of GObjs
    sizeof(GObj),                       // GObj size
    0,                                  // Number of XObjs
    NULL,                               // Matrix function list
    NULL,                               // DObjVec eject function
    0,                                  // Number of AObjs
    0,                                  // Number of MObjs
    0,                                  // Number of DObjs
    sizeof(DObj),                       // DObj size
    0,                                  // Number of SObjs
    sizeof(SObj),                       // SObj size
    0,                                  // Number of CObjs
    sizeof(CObj),                       // Camera size

    mnSoundTestFuncStart                // Task start function
};

/* The bzero arm of syDmaLoadOverlay for overlay 62, this file:
 * src/dc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnSoundTestOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNSoundTestOption);
    OVERLAY_CLEAR(sMNSoundTestOptionColorR);
    OVERLAY_CLEAR(sMNSoundTestOptionColorG);
    OVERLAY_CLEAR(sMNSoundTestOptionColorB);
    OVERLAY_CLEAR(sMNSoundTestOptionChangeWait);
    OVERLAY_CLEAR(sMNSoundTestDirectionInputKind);
    OVERLAY_CLEAR(sMNSoundTestOptionSelectID);
    OVERLAY_CLEAR(sMNSoundTestSelectIDPositionsX);
    OVERLAY_CLEAR(sMNSoundTestFadeOutWait);
    OVERLAY_CLEAR(sMNSoundTestFiles);
    OVERLAY_CLEAR(sMNSoundTestBanks);
}

/* mnsoundtest.c:1734-1741 0x801369E8. DIVERGES: syVideoInit, the zbuffer
 * and the arena_size line are the N64's video mode and its link map, set
 * once at boot here as in every other scene. */
void mnSoundTestStartScene(void)
{
    syTaskmanStartTask(&dMNSoundTestTaskmanSetup);
}
