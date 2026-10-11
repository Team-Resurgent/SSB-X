/* mn1pcontinue.c -- see mn1pcontinue.h. Every function is
 * mn/mn1pmode/mn1pcontinue.c's by name and body unless marked
 * DIVERGES; the line numbers are the decomp's. */
#include "mn1pcontinue.h"
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
#include <ef/efparticle.h>
#include <lb/lbdef.h>
#include <mn/mndef.h>
#include <PR/os.h>

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/sysshim.c defines it over the FGM engine */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset, as src/dc/sc1pchallenger.c and every other ported scene do
 * it. Each `&llXxxSprite` is written `llXxxSprite`, the number. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* the decomp's share of unused locals */
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
/* and mnPlayers1PGameContinueFuncRun's stick_range. src/dc/mnvsoptions.c
 * silences the same warning and its note says the read only happens
 * where the assignment did; that is NOT true here, and the difference is
 * the decomp's, not the port's. mn1pcontinue.c:1112-1121 reads the
 * variable after `(tap L ...) || (stick_range = ..., ... != 0)`, so a
 * tap of L or R short-circuits past the assignment and the cooldown is
 * then computed from whatever the stack slot held. Ported as written:
 * the game does this too, with the same stack. */
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

// // // // // // // // // // // //
//                               //
//            MACROS             //
//                               //
// // // // // // // // // // // //

#define mnPlayers1PGameContinueCheckGetOptionButtonInput(is_button, mask) \
mnCommonCheckGetOptionButtonInput(sMN1PContinueOptionChangeWait, is_button, mask)

#define mnPlayers1PGameContinueCheckGetOptionStickInputUD(stick_range, min, b) \
mnCommonCheckGetOptionStickInputUD(sMN1PContinueOptionChangeWait, stick_range, min, b)

#define mnPlayers1PGameContinueCheckGetOptionStickInputLR(stick_range, min, b) \
mnCommonCheckGetOptionStickInputLR(sMN1PContinueOptionChangeWait, stick_range, min, b)

#define mnPlayers1PGameContinueSetOptionChangeWaitP(stick_range) \
(sMN1PContinueOptionChangeWait = (160 - (stick_range)) / 5)

#define mnPlayers1PGameContinueSetOptionChangeWaitN(stick_range) \
(sMN1PContinueOptionChangeWait = ((stick_range) + 160) / 5)

/* The five files, as src/dc/decomp/reloc_data.us.h reads them off the
 * ROM. Only the first is new: 79 is this scene's own room, spotlight,
 * shadow, cursor and three words. The other four already ship -- 81 and
 * 80 for the score screen, 37 for the battle announcer's
 * letters, 164 for the damage digits -- and this scene borrows the word
 * SCORE, the letters of GAME OVER and the digits from them rather than
 * carrying its own. */
#define MN1PCONTINUE_BANK_MAIN        "mn1pcontinue.spr"
#define MN1PCONTINUE_BANK_SCORETEXT   "sc1pstageclear2.spr"
#define MN1PCONTINUE_BANK_ANNOUNCE    "ifannounce.spr"
#define MN1PCONTINUE_BANK_DIGITS      "ifdamage.spr"
#define MN1PCONTINUE_BANK_STAGECLEAR1 "sc1pstageclear1.spr"

/* The sprite offsets, from src/dc/decomp/reloc_data.us.h -- the values
 * the decomp's own ll<Name>Sprite link labels carry. */
#define llMN1PContinueContinueTextSprite    0x18f0
#define llMN1PContinueYesTextSprite         0x1e08
#define llMN1PContinueNoTextSprite          0x2318
#define llMN1PContinueCursorSprite          0x2df8
#define llMN1PContinueRoomSprite            0x1e3d8
#define llMN1PContinueSpotlightSprite       0x21900
#define llMN1PContinueShadowSprite          0x224f8

#define llSC1PStageClear2ScoreTextSprite    0x408

#define llIFCommonPlayerDamageDigit0Sprite  0x148
#define llIFCommonPlayerDamageDigit1Sprite  0x2d8
#define llIFCommonPlayerDamageDigit2Sprite  0x500
#define llIFCommonPlayerDamageDigit3Sprite  0x698
#define llIFCommonPlayerDamageDigit4Sprite  0x8c0
#define llIFCommonPlayerDamageDigit5Sprite  0xa58
#define llIFCommonPlayerDamageDigit6Sprite  0xc80
#define llIFCommonPlayerDamageDigit7Sprite  0xe18
#define llIFCommonPlayerDamageDigit8Sprite  0x1040
#define llIFCommonPlayerDamageDigit9Sprite  0x1270

#define llIFCommonAnnounceCommonLetterASprite 0x5e0
#define llIFCommonAnnounceCommonLetterESprite 0x1628
#define llIFCommonAnnounceCommonLetterGSprite 0x1f08
#define llIFCommonAnnounceCommonLetterMSprite 0x3980
#define llIFCommonAnnounceCommonLetterOSprite 0x44b0
#define llIFCommonAnnounceCommonLetterRSprite 0x5418
#define llIFCommonAnnounceCommonLetterVSprite 0x65d8

/* ---- the pools --------------------------------------------------------
 *
 * The decomp's dMN1PContinueTaskmanSetup carries zero for every count,
 * so these are the port's. The bill is one fighter and a little over
 * twenty sprites at the worst moment (eight GAME OVER letters, eight
 * score digits, the word SCORE, the room, the spotlight, the shadow),
 * on nine GObjs and seven cameras -- so these are
 * src/dc/sc1pchallenger.c's numbers, which cover one fighter, with the
 * SObjs and CObjs raised to cover what this scene really stands up. */
#define MN1PCONTINUE_GOBJS       32
#define MN1PCONTINUE_GOBJPROCS   32
#define MN1PCONTINUE_XOBJS      192
#define MN1PCONTINUE_AOBJS      384
#define MN1PCONTINUE_MOBJS       32
#define MN1PCONTINUE_DOBJS       64
#define MN1PCONTINUE_SOBJS       48
#define MN1PCONTINUE_COBJS       16

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

// 0x80134160
u32 dMN1PContinueFileIDs[/* */] = { 79, 81, 37, 164, 80 };

/* one SpriteBank per file */
static SpriteBank sMN1PContinueBanks[ARRAY_COUNT(dMN1PContinueFileIDs)];

/* CUT: dMN1PContinueLights11 and dMN1PContinueLights12, the two
 * Lights1 the pre-render list would have pushed --
 * dMN1PContinueTaskmanSetup says why. */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x801342F0
s32 sMN1PContinuePad0x801342F0[2];

// 0x801342F8
void *sMN1PContinueFigatreeHeap;

// 0x801342FC
s32 sMN1PContinueTotalTimeTics;

// 0x80134300
GObj *sMN1PContinueFighterGObj;

// 0x80134304
GObj *sMN1PContinueContinueGObj;

// 0x80134308
GObj *sMN1PContinueShadowGObj;

// 0x8013430C
GObj *sMN1PContinueSpotlightGObj;

// 0x80134310
GObj *sMN1PContinueCursorGObj;

// 0x80134314
GObj *sMN1PContinueOptionGObj;

// 0x80134318
GObj *sMN1PContinueRoomGObj;

// 0x8013431C
s32 sMN1PContinueRoomFadeInAlpha;

// 0x80134320
GObj *sMN1PContinueRoomFadeInGObj;

// 0x80134324
s32 sMN1PContinueSpotlightFadeAlpha;

// 0x80134328
GObj *sMN1PContinueSpotlightFadeGObj;

// 0x8013432C
s32 sMN1PContinueRoomFadeOutAlpha;

// 0x80134330
GObj *sMN1PContinueRoomFadeOutGObj;

// 0x80134334
GObj *sMN1PContinueGameOverGObj;

// 0x80134338
s32 sMN1PContinueOptionSelect;

// 0x8013433C
s32 sMN1PContinueStatus;

// 0x80134340
f32 sMN1PContinueGameOverFadeOutScale;

// 0x80134344
f32 sMN1PContinueGameOverColorStep;

// 0x80134348
FTDemoDesc sMN1PContinueFighterDemoDesc;

// 0x80134354 - ??? set but never used?
s32 sMN1PContinueUnknown0x80134354;

// 0x80134358
s32 sMN1PContinueOptionNoGameOverInputWait;

// 0x8013435C
s32 sMN1PContinueOptionYesRetryTic;

// 0x80134360
s32 sMN1PContinueIsSelectContinue;

// 0x80134364
s32 sMN1PContinueOptionNoGameOverAutoWait;

// 0x80134368
GObj *sMN1PContinueScoreGObj;

// 0x8013436C
s32 sMN1PContinueOptionChangeWait;

/* CUT: sMN1PContinueStatusBuffer and sMN1PContinueForceStatusBuffer,
 * the reloc loader's own bookkeeping -- mnPlayers1PGameContinueFuncStart
 * says why. */

// 0x80134528
void *sMN1PContinueFiles[ARRAY_COUNT(dMN1PContinueFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* CUT: mnPlayers1PGameContinueFuncLights (0x80131B00). The decomp's
 * pre-render function sets G_LIGHTING and calls
 * ftDisplayLightsDrawReflect, which is RDP state with no PVR
 * counterpart; what lights the fighter instead is
 * scSubsysFighterSetLightParams, which FuncStart calls in its own right
 * at the bottom of this file. src/dc/sc1pchallenger.c made the same cut
 * for the same reason. */

// 0x80131B58
s32 mnPlayers1PGameContinueGetPowerOf(s32 base, s32 exp)
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
void mnPlayers1PGameContinueScoreDigitInitSprite(SObj *sobj)
{
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->envcolor.r = 0x00;
    sobj->envcolor.g = 0x00;
    sobj->envcolor.b = 0x00;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0xEC;
    sobj->sprite.blue = 0x00;
}

// 0x80131C30
s32 mnPlayers1PGameContinueGetScoreDigitCount(s32 points, s32 digit_count_max)
{
    s32 digit_count_curr = digit_count_max;

    while (digit_count_curr > 0)
    {
        s32 digit = (mnPlayers1PGameContinueGetPowerOf(10, digit_count_curr - 1) != 0) ? points / mnPlayers1PGameContinueGetPowerOf(10, digit_count_curr - 1) : 0;

        if (digit != 0)
        {
            return digit_count_curr;
        }
        else digit_count_curr--;
    }
    return 0;
}

// 0x80131CDC
Sprite* mnPlayers1PGameContinueScoreDigitGetSprite(s32 digit)
{
    // 0x80134534
    intptr_t offsets[/* */] =
    {
        llIFCommonPlayerDamageDigit0Sprite,
        llIFCommonPlayerDamageDigit1Sprite,
        llIFCommonPlayerDamageDigit2Sprite,
        llIFCommonPlayerDamageDigit3Sprite,
        llIFCommonPlayerDamageDigit4Sprite,
        llIFCommonPlayerDamageDigit5Sprite,
        llIFCommonPlayerDamageDigit6Sprite,
        llIFCommonPlayerDamageDigit7Sprite,
        llIFCommonPlayerDamageDigit8Sprite,
        llIFCommonPlayerDamageDigit9Sprite
    };
    return lbRelocGetFileData(Sprite*, sMN1PContinueFiles[3], offsets[digit]);
}

// 0x80131D40
void mnPlayers1PGameContinueMakeScoreDigits
(
    GObj *gobj,
    s32 points,
    f32 x,
    f32 y,
    f32 offset_x,
    s32 sub,
    s32 digit_count,
    sb32 is_fixed_digit_count
)
{
    SObj *sobj;
    f32 calc_x;
    s32 digit;
    s32 i;

    if (points < 0)
    {
        points = 0;
    }
    sobj = lbCommonMakeSObjForGObj(gobj, mnPlayers1PGameContinueScoreDigitGetSprite(points % 10));
    mnPlayers1PGameContinueScoreDigitInitSprite(sobj);

    calc_x = (sub != 0) ? x - sub : x - (sobj->sprite.width + offset_x);

    sobj->pos.x = calc_x;
    sobj->pos.y = y;

    for (i = 1; i < ((is_fixed_digit_count != FALSE) ? digit_count : mnPlayers1PGameContinueGetScoreDigitCount(points, digit_count)); i++)
    {
        digit = (mnPlayers1PGameContinueGetPowerOf(10, i) != 0) ? points / mnPlayers1PGameContinueGetPowerOf(10, i) : 0;

        sobj = lbCommonMakeSObjForGObj(gobj, mnPlayers1PGameContinueScoreDigitGetSprite(digit % 10));
        mnPlayers1PGameContinueScoreDigitInitSprite(sobj);

        calc_x = (sub != 0) ? calc_x - sub : calc_x - (sobj->sprite.width + offset_x);

        sobj->pos.x = calc_x;
        sobj->pos.y = y;
    }
}

// 0x80131F98
void mnPlayers1PGameContinueMakeScoreDisplay(s32 points)
{
    GObj *gobj;
    SObj *sobj;

    sMN1PContinueScoreGObj = gobj = gcMakeGObjSPAfter(0, NULL, 20, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 28, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PContinueFiles[1], llSC1PStageClear2ScoreTextSprite));

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

    mnPlayers1PGameContinueMakeScoreDigits(gobj, points, 295.0F, 197.0F, 0.0F, 16, 8, TRUE);
}

// 0x80132094 - Unused?
void func_ovl55_80132094(void)
{
    return;
}

// 0x8013209C
void mnPlayers1PGameContinueSetFighterScale(GObj *gobj, s32 fkind)
{
    DObjGetStruct(gobj)->scale.vec.f.x = dSCSubsysFighterScales[fkind];
    DObjGetStruct(gobj)->scale.vec.f.y = dSCSubsysFighterScales[fkind];
    DObjGetStruct(gobj)->scale.vec.f.z = dSCSubsysFighterScales[fkind];
}

// 0x801320D4
void mnPlayers1PGameContinueMakeFighter(s32 fkind)
{
    GObj *fighter_gobj;
    FTDesc desc;

    desc = dFTManagerDefaultFighterDesc;

    desc.fkind = fkind;

    desc.pos.x = 90.0F;

    desc.costume = sMN1PContinueFighterDemoDesc.costume;
    desc.shade = sMN1PContinueFighterDemoDesc.shade;
    desc.figatree_heap = sMN1PContinueFigatreeHeap;

    desc.pos.y = 2070.0F;
    desc.pos.z = 0.0F;

    sMN1PContinueFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

    scSubsysFighterSetStatus(fighter_gobj, nFTDemoStatusFigureDropped);
    mnPlayers1PGameContinueSetFighterScale(fighter_gobj, sMN1PContinueFighterDemoDesc.fkind);
}

/* DIVERGES, the same substitution in all three fade procs below.
 * mn1pcontinue.c:353-373, 385-405 and 417-437 each push the same eight
 * gDP commands -- a pipe sync, 1CYCLE, a black gDPSetPrimColor at the
 * running alpha, G_CC_PRIMITIVE, G_RM_AA_XLU_SURF, one gDPFillRectangle
 * over (10,10)-(310,230), a second sync and the render mode put back --
 * and that is exactly lbCommonSpriteFillRect, which
 * src/dc/sc1pchallenger.c:346 and src/dc/mn1pmode.c:408 already use for
 * the same eight. 1CYCLE's lower-right corner is exclusive, which is
 * the corner lbCommonSpriteFillRect takes, so the numbers carry over
 * unchanged. The trailing render-mode restore has nothing to restore:
 * gSYTaskmanDLHeads is a scratch buffer nothing renders from
 * (src/dc/taskman.h). */

// 0x801321A8
void mnPlayers1PGameContinueRoomFadeOutProcDisplay(GObj *gobj)
{
    if (sMN1PContinueRoomFadeOutAlpha < 0xFF)
    {
        sMN1PContinueRoomFadeOutAlpha += 5;

        if (sMN1PContinueRoomFadeOutAlpha > 0xFF)
        {
            sMN1PContinueRoomFadeOutAlpha = 0xFF;
        }
    }
    lbCommonSpriteFillRect(10, 10, 310, 230,
                           0x00, 0x00, 0x00, sMN1PContinueRoomFadeOutAlpha);
}

// 0x801322DC
void mnPlayers1PGameContinueMakeRoomFadeOut(void)
{
    GObj *gobj;

    sMN1PContinueRoomFadeOutAlpha = 0x00;
    sMN1PContinueRoomFadeOutGObj = gobj = gcMakeGObjSPAfter(0, NULL, 23, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, mnPlayers1PGameContinueRoomFadeOutProcDisplay, 32, GOBJ_PRIORITY_DEFAULT, ~0);
}

// 0x80132338
void mnPlayers1PGameContinueRoomFadeInProcDisplay(GObj *gobj)
{
    if (sMN1PContinueRoomFadeInAlpha > 0x00)
    {
        sMN1PContinueRoomFadeInAlpha -= 0x05;

        if (sMN1PContinueRoomFadeInAlpha < 0x00)
        {
            sMN1PContinueRoomFadeInAlpha = 0x00;
        }
    }
    lbCommonSpriteFillRect(10, 10, 310, 230,
                           0x00, 0x00, 0x00, sMN1PContinueRoomFadeInAlpha);
}

// 0x80132460
void mnPlayers1PGameContinueMakeRoomFadeIn(void)
{
    GObj *gobj;

    sMN1PContinueRoomFadeInAlpha = 0xFF;
    sMN1PContinueRoomFadeInGObj = gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, mnPlayers1PGameContinueRoomFadeInProcDisplay, 26, GOBJ_PRIORITY_DEFAULT, ~0);
}

// 0x801324C0
void mnPlayers1PGameContinueSpotlightFadeProcDisplay(GObj *gobj)
{
    if (sMN1PContinueSpotlightFadeAlpha > 0x00)
    {
        sMN1PContinueSpotlightFadeAlpha -= 0x05;

        if (sMN1PContinueSpotlightFadeAlpha < 0x00)
        {
            sMN1PContinueSpotlightFadeAlpha = 0x00;
        }
    }
    lbCommonSpriteFillRect(10, 10, 310, 230,
                           0x00, 0x00, 0x00, sMN1PContinueSpotlightFadeAlpha);
}

// 0x801325E8
void mnPlayers1PGameContinueMakeSpotlightFade(void)
{
    GObj *gobj;

    sMN1PContinueSpotlightFadeAlpha = 0xFF;
    sMN1PContinueSpotlightFadeGObj = gobj = gcMakeGObjSPAfter(0, NULL, 22, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, mnPlayers1PGameContinueSpotlightFadeProcDisplay, 31, GOBJ_PRIORITY_DEFAULT, ~0);
}

// 0x80132648
void mnPlayers1PGameContinueMakeRoom(void)
{
    GObj *gobj;
    SObj *sobj;

    sMN1PContinueRoomGObj = gobj = gcMakeGObjSPAfter(0, NULL, 19, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 29, GOBJ_PRIORITY_DEFAULT, ~0);
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PContinueFiles[0], llMN1PContinueRoomSprite));

    sobj->pos.x = 30.0F;
    sobj->pos.y = 28.0F;
}

// 0x801326D4
void mnPlayers1PGameContinueMakeSpotlight(void)
{
    GObj *gobj;
    SObj *sobj;

    sMN1PContinueShadowGObj = gobj = gcMakeGObjSPAfter(0, NULL, 21, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 30, GOBJ_PRIORITY_DEFAULT, ~0);
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PContinueFiles[0], llMN1PContinueShadowSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xBE;
    sobj->sprite.green = 0xBE;
    sobj->sprite.blue = 0xFF;

    sobj->pos.x = 80.0F;
    sobj->pos.y = 156.0F;

    sMN1PContinueSpotlightGObj = gobj = gcMakeGObjSPAfter(0, NULL, 21, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 30, GOBJ_PRIORITY_DEFAULT, ~0);
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PContinueFiles[0], llMN1PContinueSpotlightSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xBE;
    sobj->sprite.green = 0xBE;
    sobj->sprite.blue = 0xFF;

    sobj->pos.x = 80.0F;
    sobj->pos.y = 28.0F;
}

// 0x80132824
void mnPlayers1PGameContinueMakeContinue(void)
{
    GObj *gobj;
    SObj *sobj;

    sMN1PContinueContinueGObj = gobj = gcMakeGObjSPAfter(0, NULL, 20, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 28, GOBJ_PRIORITY_DEFAULT, ~0);
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PContinueFiles[0], llMN1PContinueContinueTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->envcolor.r = 0x00;
    sobj->envcolor.g = 0x00;
    sobj->envcolor.b = 0x00;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0xFF;
    sobj->sprite.blue = 0xFF;

    sobj->pos.x = 64.0F;
    sobj->pos.y = 64.0F;
}

// 0x801328D8
void MN1PContinueOptionSetHighlightColors(GObj *gobj, s32 option)
{
    SObj *sobj;

    // 0x801341D0
    SYColorRGBPair not = { { 0x00, 0x00, 0x00 }, { 0xFF, 0x00, 0x00 } };

    // 0x801341D8
    SYColorRGBPair highlight = { { 0x00, 0x00, 0x00 }, { 0x4C, 0x47, 0x5F } };

    SYColorRGBPair *color;

    sobj = SObjGetStruct(gobj);

    color = (option == nMN1PContinueOptionYes) ? &not : &highlight;

    sobj->envcolor.r = color->prim.r;
    sobj->envcolor.g = color->prim.g;
    sobj->envcolor.b = color->prim.b;

    sobj->sprite.red = color->env.r;
    sobj->sprite.green = color->env.g;
    sobj->sprite.blue = color->env.b;

    sobj = SObjGetStruct(gobj)->next;

    color = (option == nMN1PContinueOptionNo) ? &not : &highlight;

    sobj->envcolor.r = color->prim.r;
    sobj->envcolor.g = color->prim.g;
    sobj->envcolor.b = color->prim.b;

    sobj->sprite.red = color->env.r;
    sobj->sprite.green = color->env.g;
    sobj->sprite.blue = color->env.b;
}

// 0x801329AC
void MN1PContinueOptionProcUpdate(GObj *gobj)
{
    MN1PContinueOptionSetHighlightColors(gobj, sMN1PContinueOptionSelect);
}

// 0x801329D0
void mnPlayers1PGameContinueMakeOptions(void)
{
    GObj *gobj;
    SObj *sobj;

    sMN1PContinueOptionGObj = gobj = gcMakeGObjSPAfter(0, NULL, 20, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 28, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, MN1PContinueOptionProcUpdate, nGCProcessKindFunc, 1);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PContinueFiles[0], llMN1PContinueYesTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 84.0F;
    sobj->pos.y = 129.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PContinueFiles[0], llMN1PContinueNoTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 189.0F;
    sobj->pos.y = 129.0F;

    MN1PContinueOptionSetHighlightColors(gobj, sMN1PContinueOptionSelect);
}

// 0x80132AE8
void mnPlayers1PGameContinueCursorSetPosition(GObj *gobj, s32 option)
{
    SObj *sobj = SObjGetStruct(gobj);

    if (option == nMN1PContinueOptionYes)
    {
        sobj->pos.x = 76.0F;
        sobj->pos.y = 120.0F;
    }
    else
    {
        sobj->pos.x = 177.0F;
        sobj->pos.y = 120.0F;
    }
}

// 0x80132B2C
void mnPlayers1PGameContinueCursorProcUpdate(GObj *gobj)
{
    mnPlayers1PGameContinueCursorSetPosition(gobj, sMN1PContinueOptionSelect);
}

// 0x80132B50
void mnPlayers1PGameContinueMakeCursor(void)
{
    GObj *gobj;
    SObj *sobj;

    sMN1PContinueCursorGObj = gobj = gcMakeGObjSPAfter(0, NULL, 20, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 28, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mnPlayers1PGameContinueCursorProcUpdate, nGCProcessKindFunc, 1);
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PContinueFiles[0], llMN1PContinueCursorSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->envcolor.r = 0x00;
    sobj->envcolor.g = 0x00;
    sobj->envcolor.b = 0x00;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    mnPlayers1PGameContinueCursorSetPosition(gobj, sMN1PContinueOptionSelect);
}

// 0x80132C1C
void mnPlayers1PGameContinueGameOverInitSprites(SObj *sobj)
{
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->envcolor.r = 0x1A;
    sobj->envcolor.g = 0x00;
    sobj->envcolor.b = 0xE6;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0xFF;
    sobj->sprite.blue = 0xFF;
}

// 0x80132C58
void mnPlayers1PGameContinueGameOverTextStepColors(GObj *gobj)
{
    SObj *sobj = SObjGetStruct(gobj);

    // 0x801341E0
    f32 values[/* */] = { 26.0F, 0.0F, 230.0F, 255.0F, 255.0F, 255.0F };

    if (sMN1PContinueGameOverColorStep < 1.0F)
    {
        sMN1PContinueGameOverColorStep += F_PCT_TO_DEC(1.0F);

        if (sMN1PContinueGameOverColorStep > 1.0F)
        {
            sMN1PContinueGameOverColorStep = 1.0F;
        }
        while (sobj != NULL)
        {
            sobj->envcolor.r = values[0] * sMN1PContinueGameOverColorStep;
            sobj->envcolor.g = values[1] * sMN1PContinueGameOverColorStep;
            sobj->envcolor.b = values[2] * sMN1PContinueGameOverColorStep;

            sobj->sprite.red = values[3] * sMN1PContinueGameOverColorStep;
            sobj->sprite.green = values[4] * sMN1PContinueGameOverColorStep;
            sobj->sprite.blue = values[5] * sMN1PContinueGameOverColorStep;

            sobj = sobj->next;
        }
    }
}

// 0x8013307C
void mnPlayers1PGameContinueMakeGameOverText(void)
{
    GObj *gobj;
    SObj *sobj;

    // 0x801341F8
    intptr_t letters[/* */] =
    {
        llIFCommonAnnounceCommonLetterGSprite,
        llIFCommonAnnounceCommonLetterASprite,
        llIFCommonAnnounceCommonLetterMSprite,
        llIFCommonAnnounceCommonLetterESprite,
        llIFCommonAnnounceCommonLetterOSprite,
        llIFCommonAnnounceCommonLetterVSprite,
        llIFCommonAnnounceCommonLetterESprite,
        llIFCommonAnnounceCommonLetterRSprite
    };

    // 0x80134218
    f32 positions_x[/* */] = { 30.0F, 60.0F, 95.0F, 133.0F, 166.0F, 200.0F, 230.0F, 254.0F };

    s32 i;

    sMN1PContinueGameOverColorStep = 0.0F;

    sMN1PContinueGameOverGObj = gobj = gcMakeGObjSPAfter(0, NULL, 20, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 28, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mnPlayers1PGameContinueGameOverTextStepColors, nGCProcessKindFunc, 1);

    for (i = 0; i < (ARRAY_COUNT(letters) + ARRAY_COUNT(positions_x)) / 2; i++)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PContinueFiles[2], letters[i]));

        sobj->pos.x = positions_x[i];
        sobj->pos.y = 50.0F;

        mnPlayers1PGameContinueGameOverInitSprites(sobj);
    }
}

// 0x80133210
void mnPlayers1PGameContinueGameOverProcUpdate(GObj *gobj)
{
    SObj *sobj = SObjGetStruct(sMN1PContinueRoomGObj);

    if (sMN1PContinueGameOverFadeOutScale > F_PCT_TO_DEC(1.0F))
    {
        sMN1PContinueGameOverFadeOutScale -= F_PCT_TO_DEC(1.0F);

        if (sMN1PContinueGameOverFadeOutScale < F_PCT_TO_DEC(1.0F))
        {
            sMN1PContinueGameOverFadeOutScale = F_PCT_TO_DEC(1.0F);
        }
        sobj->sprite.scalex = sMN1PContinueGameOverFadeOutScale;
        sobj->sprite.scaley = sMN1PContinueGameOverFadeOutScale;

        sobj->pos.x = 160.0F - ((260.0F * sMN1PContinueGameOverFadeOutScale) / 2);
        sobj->pos.y = 120.0F - ((184.0F * sMN1PContinueGameOverFadeOutScale) / 2);

        DObjGetStruct(sMN1PContinueFighterGObj)->translate.vec.f.y += 3.0F;

        DObjGetStruct(sMN1PContinueFighterGObj)->scale.vec.f.x = dSCSubsysFighterScales[sMN1PContinueFighterDemoDesc.fkind] * sMN1PContinueGameOverFadeOutScale;
        DObjGetStruct(sMN1PContinueFighterGObj)->scale.vec.f.y = dSCSubsysFighterScales[sMN1PContinueFighterDemoDesc.fkind] * sMN1PContinueGameOverFadeOutScale;
        DObjGetStruct(sMN1PContinueFighterGObj)->scale.vec.f.z = dSCSubsysFighterScales[sMN1PContinueFighterDemoDesc.fkind] * sMN1PContinueGameOverFadeOutScale;
    }
}

// 0x80133368
void mnPlayers1PGameContinueMakeGameOver(void)
{
    GObj *gobj;

    sMN1PContinueGameOverFadeOutScale = 1.0F;
    sMN1PContinueGameOverGObj = gobj = gcMakeGObjSPAfter(0, NULL, 20, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjProcess(gobj, mnPlayers1PGameContinueGameOverProcUpdate, nGCProcessKindFunc, 1);
}

// 0x801333C4
void mnPlayers1PGameContinueMakeRoomFadeInCamera(void)
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

    cobj->flags = COBJ_FLAG_DLBUFFERS;
}

// 0x80133474
void mnPlayers1PGameContinueMakeSpotlightFadeCamera(void)
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

    cobj->flags = COBJ_FLAG_DLBUFFERS;
}

// 0x80133524
void mnPlayers1PGameContinueMakeRoomFadeOutCamera(void)
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

    cobj->flags = COBJ_FLAG_DLBUFFERS;
}

// 0x801335D4
void mnPlayers1PGameContinueSetupCamera(CObj *cobj)
{
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->vec.eye.y = 1000.0F;
    cobj->vec.eye.z = 2000.0F;
    cobj->vec.at.y = 400.0F;

    cobj->vec.eye.x = 0.0F;
    cobj->vec.at.x = 0.0F;
    cobj->vec.at.z = 0.0F;
    cobj->vec.up.x = 0.0F;
    cobj->vec.up.z = 0.0F;

    cobj->vec.up.y = 1.0F;

    cobj->projection.persp.fovy = 30.0F;
    cobj->projection.persp.near = 100.0F;
    cobj->projection.persp.far = 15000.0F;

    cobj->flags = COBJ_FLAG_DLBUFFERS;
}

// 0x80133694
void mnPlayers1PGameContinueMakeMainCamera(void)
{
    // 0x08048600
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
            GOBJ_PRIORITY_DEFAULT,
            func_80017EC0,
            50,
            COBJ_MASK_DLLINK(27) | COBJ_MASK_DLLINK(18) |
            COBJ_MASK_DLLINK(15) | COBJ_MASK_DLLINK(10) |
            COBJ_MASK_DLLINK(9),
            -1,
            TRUE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    mnPlayers1PGameContinueSetupCamera(cobj);
}

// 0x80133718
void mnPlayers1PGameContinueMakeRoomCamera(void)
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

// 0x801337B8
void mnPlayers1PGameContinueMakeSpotlightCamera(void)
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

    cobj->flags = COBJ_FLAG_DLBUFFERS;
}

// 0x80133868
void mnPlayers1PGameContinueMakeTextCamera(void)
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

    cobj->flags = COBJ_FLAG_DLBUFFERS;
}

// 0x80133918
void mnPlayers1PGameContinueInitVars(void)
{
    sMN1PContinueTotalTimeTics = 0;

    sMN1PContinueFighterDemoDesc.fkind = gSCManager1PGameBattleState.players[gSCManagerSceneData.player].fkind;
    sMN1PContinueFighterDemoDesc.costume = gSCManager1PGameBattleState.players[gSCManagerSceneData.player].costume;
    sMN1PContinueFighterDemoDesc.shade   = gSCManager1PGameBattleState.players[gSCManagerSceneData.player].shade;

    sMN1PContinueOptionSelect = 0;
    sMN1PContinueStatus = 0;
    sMN1PContinueUnknown0x80134354 = 0;
    sMN1PContinueOptionNoGameOverAutoWait = -1;
}

// 0x80133990 - real
void mnPlayers1PGameContinueUnused0x80133990(void)
{
    return;
}

// 0x80133998
void mnPlayers1PGameContinueFuncRun(GObj *gobj)
{
    s32 unused;
    s32 stick_range;

    sMN1PContinueTotalTimeTics++;

    if (sMN1PContinueTotalTimeTics >= 10)
    {
        if (sMN1PContinueTotalTimeTics == sMN1PContinueOptionYesRetryTic)
        {
            // Why though?
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            sMN1PContinueIsSelectContinue = TRUE;

            syTaskmanSetLoadScene();
        }
        if (sMN1PContinueUnknown0x80134354 != 0)
        {
            sMN1PContinueUnknown0x80134354--;
        }
        if (sMN1PContinueOptionChangeWait != 0)
        {
            sMN1PContinueOptionChangeWait--;
        }
		if
        (
            (scSubsysControllerGetPlayerStickInRangeLR(-15, 15) != FALSE) &&
            (scSubsysControllerGetPlayerStickInRangeUD(-15, 15) != FALSE)
        )
		{
            sMN1PContinueOptionChangeWait = 0;
        }
        if ((sMN1PContinueTotalTimeTics == I_SEC_TO_TICS(40)) && (sMN1PContinueStatus == 0))
        {
            gcEjectGObj(sMN1PContinueShadowGObj);
            gcEjectGObj(sMN1PContinueSpotlightGObj);
            gcEjectGObj(sMN1PContinueContinueGObj);
            gcEjectGObj(sMN1PContinueOptionGObj);
            gcEjectGObj(sMN1PContinueCursorGObj);

            mnPlayers1PGameContinueMakeRoomFadeOut();
            mnPlayers1PGameContinueMakeGameOverText();
            mnPlayers1PGameContinueMakeGameOver();

            sMN1PContinueStatus = 2;
            sMN1PContinueOptionNoGameOverInputWait = sMN1PContinueTotalTimeTics + I_SEC_TO_TICS(1.5);
            sMN1PContinueOptionNoGameOverAutoWait = sMN1PContinueTotalTimeTics + I_SEC_TO_TICS(30);

			syAudioPlayBGM(0, nSYAudioBGM1PGameOver);
			func_800269C0_275C0(nSYAudioVoiceAnnounceGameOver);
		}
        if (sMN1PContinueStatus == 0)
        {
            if
            (
                (sMN1PContinueTotalTimeTics > I_SEC_TO_TICS(2.5)) &&
                (scSubsysControllerGetPlayerTapButtons(A_BUTTON | START_BUTTON) != FALSE)
            )
            {
                switch (sMN1PContinueOptionSelect)
                {
                case nMN1PContinueOptionYes:
                    gcEjectGObj(sMN1PContinueShadowGObj);
                    gcEjectGObj(sMN1PContinueSpotlightGObj);
                    gcEjectGObj(sMN1PContinueContinueGObj);
                    gcEjectGObj(sMN1PContinueOptionGObj);
                    gcEjectGObj(sMN1PContinueCursorGObj);

                    gSCManagerSceneData.spgame_score *= 0.5F;

                    gcEjectGObj(sMN1PContinueScoreGObj);
                    mnPlayers1PGameContinueMakeScoreDisplay(gSCManagerSceneData.spgame_score);
					scSubsysFighterSetStatus(sMN1PContinueFighterGObj, nFTDemoStatusFigureStand);

					sMN1PContinueStatus = 1;
                    sMN1PContinueOptionYesRetryTic = sMN1PContinueTotalTimeTics + I_SEC_TO_TICS(4);

					func_800269C0_275C0(nSYAudioFGM1PGameContinue);
					break;

                case nMN1PContinueOptionNo:
                    gcEjectGObj(sMN1PContinueShadowGObj);
                    gcEjectGObj(sMN1PContinueSpotlightGObj);
                    gcEjectGObj(sMN1PContinueContinueGObj);
                    gcEjectGObj(sMN1PContinueOptionGObj);
                    gcEjectGObj(sMN1PContinueCursorGObj);

                    mnPlayers1PGameContinueMakeRoomFadeOut();
                    mnPlayers1PGameContinueMakeGameOverText();
                    mnPlayers1PGameContinueMakeGameOver();

                    sMN1PContinueStatus = 2;
                    sMN1PContinueOptionNoGameOverInputWait = sMN1PContinueTotalTimeTics + I_SEC_TO_TICS(1.5);
                    sMN1PContinueOptionNoGameOverAutoWait = sMN1PContinueTotalTimeTics + I_SEC_TO_TICS(30);

                    syAudioPlayBGM(0, nSYAudioBGM1PGameOver);
                    func_800269C0_275C0(nSYAudioVoiceAnnounceGameOver);
                    break;
                }
            }
            if (sMN1PContinueTotalTimeTics > 120)
            {
                if
                (
                    (
                        (scSubsysControllerGetPlayerTapButtons(L_TRIG | L_JPAD | L_CBUTTONS) != FALSE) ||
                        mnPlayers1PGameContinueCheckGetOptionStickInputLR(stick_range, -15, FALSE)
                    )
                    &&
                    (sMN1PContinueOptionSelect == nMN1PContinueOptionNo)
                )
                {
                    func_800269C0_275C0(nSYAudioFGMMenuScroll1);
                    sMN1PContinueOptionSelect = nMN1PContinueOptionYes;
                    mnPlayers1PGameContinueSetOptionChangeWaitN(stick_range);
                }
                if
                (
                    (
                        (scSubsysControllerGetPlayerTapButtons(R_TRIG | R_JPAD | R_CBUTTONS) != FALSE) ||
                        mnPlayers1PGameContinueCheckGetOptionStickInputLR(stick_range, 15, TRUE)
                    )
                    &&
                    (sMN1PContinueOptionSelect == nMN1PContinueOptionYes)
                )
                {
                    func_800269C0_275C0(nSYAudioFGMMenuScroll1);
                    sMN1PContinueOptionSelect = nMN1PContinueOptionNo;
                    mnPlayers1PGameContinueSetOptionChangeWaitP(stick_range);
                }
            }
        }
        if
        (
            (sMN1PContinueStatus == 2)                                              &&
            (sMN1PContinueOptionNoGameOverInputWait < sMN1PContinueTotalTimeTics)   &&
            (scSubsysControllerGetPlayerTapButtons(A_BUTTON | START_BUTTON) != FALSE)
        )
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            mnPlayers1PGameContinueUnused0x80133990();

            sMN1PContinueIsSelectContinue = FALSE;
            syTaskmanSetLoadScene();
        }
        if (sMN1PContinueTotalTimeTics == sMN1PContinueOptionNoGameOverAutoWait)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            mnPlayers1PGameContinueUnused0x80133990();

            sMN1PContinueIsSelectContinue = FALSE;
            syTaskmanSetLoadScene();
        }
        if (sMN1PContinueTotalTimeTics == 40)
        {
            mnPlayers1PGameContinueMakeSpotlight();
            mnPlayers1PGameContinueMakeSpotlightFade();
        }
        if (sMN1PContinueTotalTimeTics == 60)
        {
            mnPlayers1PGameContinueMakeRoomFadeIn();
            mnPlayers1PGameContinueMakeRoom();
            mnPlayers1PGameContinueMakeContinue();
            func_800269C0_275C0(nSYAudioVoiceAnnounceContinue);
        }
        if (sMN1PContinueTotalTimeTics == 120)
        {
            mnPlayers1PGameContinueMakeOptions();
            mnPlayers1PGameContinueMakeCursor();
        }
    }
}

/* DIVERGES: the port's half of mnPlayers1PGameContinueFuncStart's first
 * two lines. lbRelocInitSetup is the ROM's reloc loader being pointed at
 * this scene's two status buffers, and lbRelocLoadFilesListed pulls the
 * five files in; the port's file is a sprite bank on the romdisk, so
 * what is left is opening the five and filing the handles where the
 * decomp files the pointers. Four of the five are other scenes' banks,
 * already on the disc for their own sake. */
void mnPlayers1PGameContinueLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMN1PContinueFileIDs)] =
    {
        MN1PCONTINUE_BANK_MAIN,
        MN1PCONTINUE_BANK_SCORETEXT,
        MN1PCONTINUE_BANK_ANNOUNCE,
        MN1PCONTINUE_BANK_DIGITS,
        MN1PCONTINUE_BANK_STAGECLEAR1
    };
    s32 i;

    for (i = 0; i < ARRAY_COUNT(dMN1PContinueFileIDs); i++)
    {
        sMN1PContinueFiles[i] = NULL;

        if (sprite_bank_load(&sMN1PContinueBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mn1PContinue: no bank for file %d (%s)\n",
                          (int)dMN1PContinueFileIDs[i], paths[i]);
            continue;
        }
        sMN1PContinueFiles[i] = &sMN1PContinueBanks[i];
    }
}

// 0x80133F58
void mnPlayers1PGameContinueFuncStart(void)
{
    mnPlayers1PGameContinueLoadFiles();

    gcMakeGObjSPAfter(0, mnPlayers1PGameContinueFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
    /* CUT: gcMakeDefaultCameraGObj, the black clear camera -- the PVR
     * clears its own framebuffer, and a COBJ_FLAG_FILLCOLOR camera whose
     * mask covers a 3D list paints over it
     * ([[fillcolor-camera-paints-over-3d]]; src/dc/mnstartup.c's note
     * spells the second half out). Here it would have covered the
     * fighter and all six sprite links. */
    efParticleInitAll();
    mnPlayers1PGameContinueInitVars();
    /* DIVERGES: efManagerInitEffects is the port's
     * efManagerLoadEffectBank (src/dc/efmanager.h), as in the battle and
     * on both of the ladder's cards. */
    efManagerLoadEffectBank();
    ftManagerAllocFighter(FTDATA_FLAG_SUBMOTION, 1);
    ftManagerSetupFilesAllKind(sMN1PContinueFighterDemoDesc.fkind);

    /* DIVERGES: gFTManagerFigatreeHeapSize is the ROM loader's, and the
     * port has no figatree heap -- a fighter's pack plays its animations
     * in place (src/dc/mnplayersvs.c says it first). The NULL is what
     * mnPlayers1PGameContinueMakeFighter then passes as
     * desc.figatree_heap. */
    sMN1PContinueFigatreeHeap = NULL;

    mnPlayers1PGameContinueMakeMainCamera();
    mnPlayers1PGameContinueMakeRoomFadeInCamera();
    mnPlayers1PGameContinueMakeSpotlightFadeCamera();
    mnPlayers1PGameContinueMakeRoomFadeOutCamera();
    mnPlayers1PGameContinueMakeRoomCamera();
    mnPlayers1PGameContinueMakeSpotlightCamera();
    mnPlayers1PGameContinueMakeTextCamera();
    mnPlayers1PGameContinueMakeFighter(sMN1PContinueFighterDemoDesc.fkind);
    mnPlayers1PGameContinueMakeScoreDisplay(gSCManagerSceneData.spgame_score);

    scSubsysFighterSetLightParams(45.0F, 45.0F, 0xFF, 0xFF, 0xFF, 0xFF);

    syAudioStopBGMAll();
    syAudioPlayBGM(0, nSYAudioBGM1PGameEndChoice);
}

/* CUT: dMN1PContinueVideoSetup (0x80134238), the N64's video mode --
 * mnPlayers1PGameContinueStartScene says why. */

// 0x80134254
SYTaskmanSetup dMN1PContinueTaskmanSetup =
{
    // Task Manager Buffer Setup
    {
        0,                          // ???
        gcRunAll,              		// Update function
        scManagerFuncDraw,          // Frame draw function
        /* the decomp names &ovl55_BSS_END here; a NULL arena_start keeps
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
        /* CUT: mnPlayers1PGameContinueFuncLights, this scene's
         * pre-render function -- the top of this file says why. */
        NULL,                       // Pre-render function
        syControllerFuncRead,       // Controller I/O function
    },

    0,                              // Number of GObjThreads
    sizeof(u64) * 192,              // Thread stack size
    0,                              // Number of thread stacks
    0,                              // ???
    MN1PCONTINUE_GOBJPROCS,
    MN1PCONTINUE_GOBJS,   sizeof(GObj),
    MN1PCONTINUE_XOBJS,
    /* the decomp names dLBCommonFuncMatrixList here; the port has no
     * such table (the matrix kinds are a switch in objdisplay.c), the
     * same NULL every other ported scene's setup carries */
    NULL,                           // Matrix function list
    NULL,                           // DObjVec eject function
    MN1PCONTINUE_AOBJS,
    MN1PCONTINUE_MOBJS,
    MN1PCONTINUE_DOBJS,   sizeof(DObj),
    MN1PCONTINUE_SOBJS,   sizeof(SObj),
    MN1PCONTINUE_COBJS,   sizeof(CObj),

    mnPlayers1PGameContinueFuncStart // Task start function
};

// 0x801340FC
void mnPlayers1PGameContinueStartScene(void)
{
    /* CUT: syVideoInit and the z-buffer it is handed. The port's video
     * mode is the PVR's, set once at boot; and the arena line below it
     * reads a link map the ELF does not have --
     * dMN1PContinueTaskmanSetup carries NULL for arena_start instead.
     * Both cuts are src/dc/scvsbattle.c's, for its reasons. */
    scManagerFuncUpdate(&dMN1PContinueTaskmanSetup);

    gSCManagerSceneData.is_continue = sMN1PContinueIsSelectContinue;
}

/* The port's own bzero arm for dSCManagerOverlays[55] (src/dc/overlay.c).
 * The banks' records are syTaskmanMalloc'd out of the scene heap and go
 * with it, so the SpriteBank statics have to be cleared or the next
 * visit thinks they are still loaded.
 *
 * sMN1PContinueIsSelectContinue is in this list because it is in the
 * overlay's noload segment in the ROM and the game bzeroes it there
 * too -- and because zero is the right answer for a scene that was
 * entered and did not finish: no continue. StartScene reads it back
 * out after scManagerFuncUpdate returns, which is inside the same
 * scene, so the clear never eats a real answer. */
void mnPlayers1PGameContinueOverlayLoad(void)
{
    OVERLAY_CLEAR(sMN1PContinueBanks);
    OVERLAY_CLEAR(sMN1PContinueFiles);
    OVERLAY_CLEAR(sMN1PContinuePad0x801342F0);
    OVERLAY_CLEAR(sMN1PContinueFigatreeHeap);
    OVERLAY_CLEAR(sMN1PContinueTotalTimeTics);
    OVERLAY_CLEAR(sMN1PContinueFighterGObj);
    OVERLAY_CLEAR(sMN1PContinueContinueGObj);
    OVERLAY_CLEAR(sMN1PContinueShadowGObj);
    OVERLAY_CLEAR(sMN1PContinueSpotlightGObj);
    OVERLAY_CLEAR(sMN1PContinueCursorGObj);
    OVERLAY_CLEAR(sMN1PContinueOptionGObj);
    OVERLAY_CLEAR(sMN1PContinueRoomGObj);
    OVERLAY_CLEAR(sMN1PContinueRoomFadeInAlpha);
    OVERLAY_CLEAR(sMN1PContinueRoomFadeInGObj);
    OVERLAY_CLEAR(sMN1PContinueSpotlightFadeAlpha);
    OVERLAY_CLEAR(sMN1PContinueSpotlightFadeGObj);
    OVERLAY_CLEAR(sMN1PContinueRoomFadeOutAlpha);
    OVERLAY_CLEAR(sMN1PContinueRoomFadeOutGObj);
    OVERLAY_CLEAR(sMN1PContinueGameOverGObj);
    OVERLAY_CLEAR(sMN1PContinueOptionSelect);
    OVERLAY_CLEAR(sMN1PContinueStatus);
    OVERLAY_CLEAR(sMN1PContinueGameOverFadeOutScale);
    OVERLAY_CLEAR(sMN1PContinueGameOverColorStep);
    OVERLAY_CLEAR(sMN1PContinueFighterDemoDesc);
    OVERLAY_CLEAR(sMN1PContinueUnknown0x80134354);
    OVERLAY_CLEAR(sMN1PContinueOptionNoGameOverInputWait);
    OVERLAY_CLEAR(sMN1PContinueOptionYesRetryTic);
    OVERLAY_CLEAR(sMN1PContinueIsSelectContinue);
    OVERLAY_CLEAR(sMN1PContinueOptionNoGameOverAutoWait);
    OVERLAY_CLEAR(sMN1PContinueScoreGObj);
    OVERLAY_CLEAR(sMN1PContinueOptionChangeWait);
}
