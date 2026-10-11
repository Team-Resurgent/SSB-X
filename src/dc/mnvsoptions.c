/* mnvsoptions.c -- see mnvsoptions.h. Every function is
 * mn/mnvsmode/mnvsoptions.c's by name and body, REGION_US arms; the
 * line numbers are the decomp's. */
#include "mnvsoptions.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"

#include <sys/debug.h>
#include <sys/controller.h>
#include <sys/rdp.h>
#include <sc/scdef.h>
#include <sc/scsubsys/scsubsys.h>
#include <gm/gmsound.h>
#include <ft/ftcommon.h>
#include <lb/lbdef.h>
#include <mn/mndef.h>
#include <PR/os.h>

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/sysshim.c defines it over the FGM engine */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mnvsmode.c does the same). Each `&llXxxSprite` below
 * is written `llXxxSprite`, the number, with the label's name kept as
 * the macro's. The values are relocData files 0 (MNCommon) and 7
 * (MNVSOptions) as tools/export/ssb_spriteexport.py --list reads them off the
 * ROM. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* the decomp's share of unused locals: the digit widths table, the
 * `unused` slots IDO's stack layout needed, and the subtitle offset
 * array the US arm never reads */
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
/* and mnVSOptionsFuncRun's stick_range, which the decomp assigns inside
 * the short-circuit of `(button ...) || (stick_range = ..., ...)`: the
 * arm that reads it runs only where the button check was false, which
 * is exactly where the assignment happened */
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

#define llMNCommonAutoTextJapSprite      0x01968
#define llMNCommonOnTextSprite           0x0B818
#define llMNCommonOffTextSprite          0x0B958
#define llMNCommonSlashSprite            0x0BA28
#define llMNCommonDigit0Sprite           0x0D310
#define llMNCommonDigit1Sprite           0x0D3E0
#define llMNCommonDigit2Sprite           0x0D4B0
#define llMNCommonDigit3Sprite           0x0D580
#define llMNCommonDigit4Sprite           0x0D650
#define llMNCommonDigit5Sprite           0x0D720
#define llMNCommonDigit6Sprite           0x0D7F0
#define llMNCommonDigit7Sprite           0x0D8C0
#define llMNCommonDigit8Sprite           0x0D990
#define llMNCommonDigit9Sprite           0x0DA60
#define llMNCommonPercentageSprite       0x0DB30
#define llMNCommonAutoTextSprite         0x0DF48
#define llMNCommonSmashBrosCollageSprite 0x18000

#define llMNVSOptionsVSOptionsTextSprite   0x2668
#define llMNVSOptionsBubbleSprite          0x33D8
#define llMNVSOptionsHandicapTextSprite    0x3690
#define llMNVSOptionsTeamAttackTextSprite  0x3968
#define llMNVSOptionsStageSelectTextSprite 0x3CF8
#define llMNVSOptionsItemSwitchTextSprite  0x3FC8
#define llMNVSOptionsDamageTextSprite      0x4228
#define llMNVSOptionsConsoleIconDarkSprite 0x5F60

#define MNVSOPTIONS_BANK_COMMON     "mncommon.spr"
#define MNVSOPTIONS_BANK_VSOPTIONS  "mnvsoptions.spr"

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mnvsoptions.c:39 (0x801346C0): { &llMNCommonFileID,
 * &llMNVSOptionsFileID }, the two link labels' values written out. */
u32 dMNVSOptionsFileIDs[/* */] = { 0, 7 };

/* mnvsoptions.c:42-49 dMNVSOptionsLights1 and dMNVSOptionsDisplayList,
 * the lighting the pre-render function would set for 3D this scene
 * never draws, are not here: see dMNVSOptionsTaskmanSetup. */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x801348C0
GObj *sMNVSOptionsOptionGObjs[nMNVSOptionsOptionEnumCount];

// 0x801348E0
s32 sMNVSOptionsOption;

// 0x801348E4
s32 sMNVSOptionsHandicapStatus;

// 0x801348E8
s32 sMNVSOptionsTeamAttackStatus;

// 0x801348EC
s32 sMNVSOptionsStageSelectStatus;

// 0x801348F0
s32 sMNVSOptionsDamage;

// 0x801348F4
GObj *sMNVSOptionsSubtitlesGObj;

/* 0x801348F8. DIVERGES in its type only: the decomp declares this s32
 * and stores a GObj* in it, which is the same 32 bits on the N64 and is
 * not on a 64-bit host, where the host test would eject a truncated
 * pointer. It is the pointer everywhere it is used. */
GObj *sMNVSOptionsDamageGObj;

// 0x801348FC
s32 sMNVSOptionsFirstAvailableOption;

// 0x80134900
s32 sMNVSOptionsLastAvailableOption;

// 0x80134904
sb32 sMNVSOptionsIsHaveItemSwitch;

// 0x80134908
s32 sMNVSOptionsOptionChangeWait;

// 0x8013490C
s32 sMNVSOptionsTotalTimeTics;

// 0x80134910
s32 sMNVSOptionsReturnTic;

/* 0x80134918 sMNVSOptionsStatusBuffer, the reloc loader's 24 file
 * nodes, is not here: see mnVSOptionsLoadFiles. */

// 0x801349D8
void *sMNVSOptionsFiles[ARRAY_COUNT(dMNVSOptionsFileIDs)];

/* the banks behind sMNVSOptionsFiles */
static SpriteBank sMNVSOptionsBanks[ARRAY_COUNT(dMNVSOptionsFileIDs)];

/* mnvsoptions.c:18-30, the five macros that bind the option-change wait
 * this scene keeps to mn/mncommon's input helpers. */
#define mnVSOptionsCheckGetOptionButtonInput(is_button, mask) \
mnCommonCheckGetOptionButtonInput(sMNVSOptionsOptionChangeWait, is_button, mask)

#define mnVSOptionsCheckGetOptionStickInputUD(stick_range, min, b) \
mnCommonCheckGetOptionStickInputUD(sMNVSOptionsOptionChangeWait, stick_range, min, b)

#define mnVSOptionsCheckGetOptionStickInputLR(stick_range, min, b) \
mnCommonCheckGetOptionStickInputLR(sMNVSOptionsOptionChangeWait, stick_range, min, b)

#define mnVSOptionsSetOptionChangeWaitP(is_button, stick_range, div) \
mnCommonSetOptionChangeWaitP(sMNVSOptionsOptionChangeWait, is_button, stick_range, div)

#define mnVSOptionsSetOptionChangeWaitN(is_button, stick_range, div) \
mnCommonSetOptionChangeWaitN(sMNVSOptionsOptionChangeWait, is_button, stick_range, div)

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mnvsoptions.c:119-122 0x80131B00 mnVSOptionsFuncLights, the
 * G_LIGHTING geometry mode and one light for 3D this scene never draws,
 * is not here: see dMNVSOptionsTaskmanSetup. */

// 0x80131B24
sb32 mnVSOptionsCheckHaveItemSwitch(void)
{
    if (gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_ITEMSWITCH)
    {
        return TRUE;
    }
    else return FALSE;
}

// 0x80131B4C
s32 mnVSOptionsGetPowerOf(s32 base, s32 exp)
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

// 0x80131BEC
void mnVSOptionsSetDamageDigitSpriteColors(SObj *sobj, u32 *colors)
{
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = colors[0];
    sobj->sprite.green = colors[1];
    sobj->sprite.blue = colors[2];
}

// 0x80131C1C
s32 mnVSOptionsGetDamageDigitCount(s32 damage, s32 digit_count_max)
{
    s32 digit_count_curr = digit_count_max;

    while (digit_count_curr > 0)
    {
        s32 digit = (mnVSOptionsGetPowerOf(10, digit_count_curr - 1) != 0) ? damage / mnVSOptionsGetPowerOf(10, digit_count_curr - 1) : 0;

        if (digit != 0)
        {
            return digit_count_curr;
        }
        else digit_count_curr--;
    }
    return 0;
}

// 0x80131CC8
void mnVSOptionsMakeDamageDigitSObjs(GObj *gobj, s32 damage, f32 pos_x, f32 pos_y, u32 *colors, s32 digit_count_max, sb32 is_fixed_digit_count)
{
    // 0x80134708
    u32 digit_offsets[/* */] =
    {
        llMNCommonDigit0Sprite,
        llMNCommonDigit1Sprite,
        llMNCommonDigit2Sprite,
        llMNCommonDigit3Sprite,
        llMNCommonDigit4Sprite,
        llMNCommonDigit5Sprite,
        llMNCommonDigit6Sprite,
        llMNCommonDigit7Sprite,
        llMNCommonDigit8Sprite,
        llMNCommonDigit9Sprite
    };

    // 0x80134730 - unused
    f32 digit_widths[/* */] =
    {
        10.0F,
         6.0F,
         9.0F,
         9.0F,
        10.0F,
         9.0F,
         9.0F,
        10.0F,
         9.0F,
        10.0F
    };

    s32 i;
    s32 digit;
    f32 calc_x;
    SObj *sobj;

    calc_x = pos_x;

    if (damage < 0)
    {
        damage = 0;
    }
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[0], digit_offsets[damage % 10]));
    mnVSOptionsSetDamageDigitSpriteColors(sobj, colors);

    calc_x -= 11.0F;

    sobj->pos.x = calc_x;
    sobj->pos.y = pos_y;

    for (i = 1; i < ((is_fixed_digit_count != FALSE) ? digit_count_max : mnVSOptionsGetDamageDigitCount(damage, digit_count_max)); i++)
    {
        digit = (mnVSOptionsGetPowerOf(10, i) != 0) ? damage / mnVSOptionsGetPowerOf(10, i) : 0;

        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[0], digit_offsets[digit % 10]));

        mnVSOptionsSetDamageDigitSpriteColors(sobj, colors);

        calc_x -= 11.0F;

        sobj->pos.x = calc_x;
        sobj->pos.y = pos_y;
    }
}

// 0x80131F74
void mnVSOptionsSetOptionSpriteColors(GObj *gobj, s32 status)
{
    // 0x80134758
    SYColorRGBPair notcolors = { { 0x00, 0x00, 0x00 }, { 0x82, 0x82, 0xAA } };

    // 0x80134760
    SYColorRGBPair hicolors = { { 0xFA, 0x8C, 0x00 }, { 0xF4, 0xC8, 0x0A } };

    SYColorRGBPair *colors;
    SObj *sobj = SObjGetStruct(gobj);

    colors = (status != nMNOptionTabStatusNot) ? &hicolors : &notcolors;

    sobj->envcolor.r = colors->prim.r;
    sobj->envcolor.g = colors->prim.g;
    sobj->envcolor.b = colors->prim.b;

    sobj->sprite.red = colors->env.r;
    sobj->sprite.green = colors->env.g;
    sobj->sprite.blue = colors->env.b;
}

// 0x80131FFC
void mnVSOptionsSetToggleSpriteColors(GObj *gobj, s32 status)
{
    SObj *sobj = SObjGetStruct(gobj)->next->next;

    if (status != nMNOptionTabStatusOff)
    {
        sobj->sprite.red = 0xFF;
        sobj->sprite.green = 0x00;
        sobj->sprite.blue = 0x28;

        sobj->next->sprite.red = 0x32;
        sobj->next->sprite.green = 0x32;
        sobj->next->sprite.blue = 0x32;
    }
    else
    {
        sobj->sprite.red = 0x32;
        sobj->sprite.green = 0x32;
        sobj->sprite.blue = 0x32;

        sobj->next->sprite.red = 0xFF;
        sobj->next->sprite.green = 0x00;
        sobj->next->sprite.blue = 0x28;
    }
}

// 0x80132078
void mnVSOptionsMakeOnOffToggle(GObj *gobj, f32 pos_x, f32 pos_y)
{
    SObj *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[0], llMNCommonOnTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x;
    sobj->pos.y = pos_y;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[0], llMNCommonOffTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x + 25.0F + 7.0F;
    sobj->pos.y = pos_y;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[0], llMNCommonSlashSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x + 25.0F;
    sobj->pos.y = pos_y;

    sobj->sprite.red = 0x32;
    sobj->sprite.green = 0x32;
    sobj->sprite.blue = 0x32;
}

// 0x80132184
void mnVSOptionsSetHandicapSpriteColors(GObj *gobj, s32 setting)
{
    SObj *sobj = SObjGetStruct(gobj)->next->next;

    switch (setting)
    {
    case nSCBattleHandicapOn:
        sobj->sprite.red = 0xFF;
        sobj->sprite.green = 0x00;
        sobj->sprite.blue = 0x28;

        sobj->next->sprite.red = 0x32;
        sobj->next->sprite.green = 0x32;
        sobj->next->sprite.blue = 0x32;

        sobj->next->next->sprite.red = 0x32;
        sobj->next->next->sprite.green = 0x32;
        sobj->next->next->sprite.blue = 0x32;
        break;

    case nSCBattleHandicapAuto:
        sobj->sprite.red = 0x32;
        sobj->sprite.green = 0x32;
        sobj->sprite.blue = 0x32;

        sobj->next->sprite.red = 0xFF;
        sobj->next->sprite.green = 0x00;
        sobj->next->sprite.blue = 0x28;

        sobj->next->next->sprite.red = 0x32;
        sobj->next->next->sprite.green = 0x32;
        sobj->next->next->sprite.blue = 0x32;
        break;

    case nSCBattleHandicapOff:
        sobj->sprite.red = 0x32;
        sobj->sprite.green = 0x32;
        sobj->sprite.blue = 0x32;

        sobj->next->sprite.red = 0x32;
        sobj->next->sprite.green = 0x32;
        sobj->next->sprite.blue = 0x32;

        sobj->next->next->sprite.red = 0xFF;
        sobj->next->next->sprite.green = 0x00;
        sobj->next->next->sprite.blue = 0x28;
        break;
    }
}

// 0x801322B8
void mnVSOptionsMakeHandicapToggle(GObj *gobj, f32 pos_x, f32 pos_y)
{
    SObj *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[0], llMNCommonOnTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x;
    sobj->pos.y = pos_y;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[0], llMNCommonAutoTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x + 25.0F + 5.0F;
    sobj->pos.y = pos_y + 1.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[0], llMNCommonOffTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x + 25.0F + 5.0F + 30.0F + 6.0F;
    sobj->pos.y = pos_y;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[0], llMNCommonSlashSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x + 25.0F;
    sobj->pos.y = pos_y;

    sobj->sprite.red = 0x32;
    sobj->sprite.green = 0x32;
    sobj->sprite.blue = 0x32;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[0], llMNCommonSlashSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x + 25.0F + 5.0F + 30.0F;
    sobj->pos.y = pos_y;

    sobj->sprite.red = 0x32;
    sobj->sprite.green = 0x32;
    sobj->sprite.blue = 0x32;
}

// 0x80132478
void mnVSOptionsMakeDamageDigits(void)
{
    GObj *gobj;
    s32 unused;

    // 0x80134768
    u32 colors[/* */] =
    {
        0xFF, 0x00, 0x28,
        0x00, 0x00, 0x00
    };

    s32 pos_y = (sMNVSOptionsIsHaveItemSwitch != FALSE) ? 151 : 164;

    sMNVSOptionsDamageGObj = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);
    mnVSOptionsMakeDamageDigitSObjs(gobj, sMNVSOptionsDamage, 220.0F, pos_y, colors, 3, FALSE);
}

// 0x80132564
void mnVSOptionsMakeDamageOption(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 pos_y;

    pos_y = (sMNVSOptionsIsHaveItemSwitch != FALSE) ? 148 : 161;

    sMNVSOptionsOptionGObjs[nMNVSOptionsOptionDamage] = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[1], llMNVSOptionsBubbleSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 90.0F;
    sobj->pos.y = pos_y;

    mnVSOptionsSetOptionSpriteColors(gobj, sMNVSOptionsOption == nMNVSOptionsOptionDamage);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[1], llMNVSOptionsDamageTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 116.0F;
    sobj->pos.y = pos_y + 1;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[0], llMNCommonPercentageSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 226.0F;
    sobj->pos.y = pos_y + 3;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;
}

// 0x801326F0
void mnVSOptionsMakeItemSwitchOption(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 pos_y = 176;

    sMNVSOptionsOptionGObjs[nMNVSOptionsOptionItemSwitch] = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[1], llMNVSOptionsBubbleSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 82.0F;
    sobj->pos.y = pos_y + 1;

    mnVSOptionsSetOptionSpriteColors(gobj, sMNVSOptionsOption == nMNVSOptionsOptionItemSwitch);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[1], llMNVSOptionsItemSwitchTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 128.0F;
    sobj->pos.y = pos_y + 3;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;
}

// 0x80132804
void mnVSOptionsMakeStageSelectOption(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 pos_y;

    pos_y = (sMNVSOptionsIsHaveItemSwitch != FALSE) ? 119 : 129;

    sMNVSOptionsOptionGObjs[nMNVSOptionsOptionStageSelect] = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[1], llMNVSOptionsBubbleSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 98.0F;
    sobj->pos.y = pos_y;

    mnVSOptionsSetOptionSpriteColors(gobj, sMNVSOptionsOption == nMNVSOptionsOptionStageSelect);
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[1], llMNVSOptionsStageSelectTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 104.0F;
    sobj->pos.y = pos_y + 1;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    mnVSOptionsMakeOnOffToggle(gobj, 208.0F, pos_y + 1);
    mnVSOptionsSetToggleSpriteColors(gobj, sMNVSOptionsStageSelectStatus);
}

// 0x80132968
void mnVSOptionsMakeTeamAttackOption(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 pos_y;

    pos_y = (sMNVSOptionsIsHaveItemSwitch != FALSE) ? 90 : 97;

    sMNVSOptionsOptionGObjs[nMNVSOptionsOptionTeamAttack] = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[1], llMNVSOptionsBubbleSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 106.0F;
    sobj->pos.y = pos_y;

    mnVSOptionsSetOptionSpriteColors(gobj, sMNVSOptionsOption == nMNVSOptionsOptionTeamAttack);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[1], llMNVSOptionsTeamAttackTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 116.0F;
    sobj->pos.y = pos_y + 2;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    mnVSOptionsMakeOnOffToggle(gobj, 212.0F, pos_y + 1);
    mnVSOptionsSetToggleSpriteColors(gobj, sMNVSOptionsTeamAttackStatus);
}

// 0x80132AC8
void mnVSOptionsMakeHandicapOption(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 pos_y;

    pos_y = (sMNVSOptionsIsHaveItemSwitch != FALSE) ? 61 : 65;

    sMNVSOptionsOptionGObjs[nMNVSOptionsOptionHandicap] = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[1], llMNVSOptionsBubbleSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 114.0F;
    sobj->pos.y = pos_y;

    mnVSOptionsSetOptionSpriteColors(gobj, sMNVSOptionsOption == nMNVSOptionsOptionHandicap);
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[1], llMNVSOptionsHandicapTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 121.0F;
    sobj->pos.y = pos_y + 2;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    mnVSOptionsMakeHandicapToggle(gobj, 191.0F, pos_y + 1);
    mnVSOptionsSetHandicapSpriteColors(gobj, sMNVSOptionsHandicapStatus);
}

/* mnvsoptions.c:680-695 0x80132C24. DIVERGES in one place: the nine RDP
 * commands -- a G_CC_PRIMITIVE fill rectangle at prim colour
 * (0x80, 0x80, 0x80, 0xFF) under G_RM_AA_XLU_SURF, and the render mode
 * and cycle type put back after it -- are one fill quad at the pass's
 * next depth (src/dc/lbcommon.c lbCommonSpriteFillRect), the grey bar
 * the VS OPTIONS label then draws over. 1-cycle mode, so the
 * lower-right stays exclusive as the quad wants it. */
void mnVSOptionsLabelProcDisplay(GObj *gobj)
{
    lbCommonSpriteFillRect(79, 34, 310, 39, 0x80, 0x80, 0x80, 0xFF);

    lbCommonClearExternSpriteParams();
    lbCommonDrawSObjAttr(gobj);
}

// 0x80132D68
void mnVSOptionsMakeLabel(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, mnVSOptionsLabelProcDisplay, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[1], llMNVSOptionsVSOptionsTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->envcolor.r = 0x00;
    sobj->envcolor.g = 0x00;
    sobj->envcolor.b = 0x00;

    sobj->sprite.red = 0xF2;
    sobj->sprite.green = 0xC7;
    sobj->sprite.blue = 0x0D;

    sobj->pos.x = 84.0F;
    sobj->pos.y = 24.0F;
}

// 0x80132E24
void mnVSOptionsSetSubtitleSpriteColors(SObj *sobj)
{
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->envcolor.r = 0x00;
    sobj->envcolor.g = 0x00;
    sobj->envcolor.b = 0x00;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0xFF;
    sobj->sprite.blue = 0xFF;
}

/* mnvsoptions.c:738-866 0x80132E58. The whole body is REGION_JP -- the
 * US build makes the GObj and nothing else, and the cursor moves eject
 * and remake it every time. It stays as the decomp has it. */
void mnVSOptionsMakeSubtitle(void)
{
    GObj *gobj;
    SObj *sobj;

    // 0x80134780
    u32 sp1C[/* */] =
    {
        llMNCommonOffTextSprite,
        llMNCommonOnTextSprite,
        llMNCommonAutoTextJapSprite
    };

    sMNVSOptionsSubtitlesGObj = gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);
}

// 0x80132EAC
void mnVSOptionsMakeWallpaper(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 0, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[0], llMNCommonSmashBrosCollageSprite));

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;
}

/* mnvsoptions.c:883-995 0x80132F2C. DIVERGES in one place, as the
 * label's: the fill rectangle under the option the cursor is on is one
 * quad. This one is emitted in G_CYC_FILL, where the RDP takes the
 * lower-right corner as inclusive -- which is why sys/objdisplay.c
 * writes `lrx--, lry--` before its own fill rects, and why these
 * rectangles are one pixel tall with uly == lry -- so the quad, whose
 * corner is exclusive, is drawn one further right and down. */
void mnVSOptionsUnderlineProcDisplay(GObj *gobj)
{
    // 0x8013478C
    SYRectangle handicap_rect[/* */] =
    {
        { 255, 77, 283, 77 },
        { 190, 77, 216, 77 },
        { 219, 77, 251, 77 }
    };

    // 0x801347BC
    SYRectangle team_attack_rect[/* */] =
    {
        { 245, 106, 272, 106 },
        { 213, 106, 239, 106 }
    };

    // 0x801347DC
    SYRectangle stage_select_rect[/* */] =
    {
        { 241, 135, 269, 135 },
        { 208, 135, 234, 135 }
    };

    s32 off_y;
    SYRectangle *r;

    switch (sMNVSOptionsOption)
    {
    case nMNVSOptionsOptionHandicap:
        off_y = (sMNVSOptionsIsHaveItemSwitch != FALSE) ? 0 : 4;
        r = &handicap_rect[sMNVSOptionsHandicapStatus];
        lbCommonSpriteFillRect(r->ulx, r->uly + off_y, r->lrx + 1, r->lry + off_y + 1,
                               0xFF, 0x00, 0x28, 0xFF);
        break;

    case nMNVSOptionsOptionTeamAttack:
        off_y = (sMNVSOptionsIsHaveItemSwitch != FALSE) ? 0 : 7;
        r = &team_attack_rect[sMNVSOptionsTeamAttackStatus];
        lbCommonSpriteFillRect(r->ulx, r->uly + off_y, r->lrx + 1, r->lry + off_y + 1,
                               0xFF, 0x00, 0x28, 0xFF);
        break;

    case nMNVSOptionsOptionStageSelect:
        off_y = (sMNVSOptionsIsHaveItemSwitch != FALSE) ? 0 : 10;
        r = &stage_select_rect[sMNVSOptionsStageSelectStatus];
        lbCommonSpriteFillRect(r->ulx, r->uly + off_y, r->lrx + 1, r->lry + off_y + 1,
                               0xFF, 0x00, 0x28, 0xFF);
        break;
    }
    lbCommonClearExternSpriteParams();
}

// 0x80133300
void mnVSOptionsMakeUnderline(void)
{
    gcAddGObjDisplay(gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT), mnVSOptionsUnderlineProcDisplay, 1, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mnvsoptions.c:1003-1016 0x8013334C. DIVERGES as the label's: the dark
 * red wash over the whole screen is one fill quad, blended by its own
 * alpha as G_RM_AA_XLU_SURF blends it. */
void mnVSOptionsTintProcDisplay(GObj *gobj)
{
    lbCommonSpriteFillRect(10, 10, 310, 230, 0x0D, 0x00, 0x00, 0x99);

    lbCommonClearExternSpriteParams();
}

// 0x80133464
void mnVSOptionsMakeTint(void)
{
    gcAddGObjDisplay(gcMakeGObjSPAfter(0, NULL, 5, GOBJ_PRIORITY_DEFAULT), mnVSOptionsTintProcDisplay, 3, GOBJ_PRIORITY_DEFAULT, ~0);
}

// 0x801334B0
void mnVSOptionsMakeDecal(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 6, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 4, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSOptionsFiles[1], llMNVSOptionsConsoleIconDarkSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x4A;
    sobj->sprite.green = 0x2A;
    sobj->sprite.blue = 0x23;

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;
}

// 0x80133558
void mnVSOptionsMakeTintCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            1,
            NULL,
            1,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            70,
            COBJ_MASK_DLLINK(3),
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

// 0x801335F8
void mnVSOptionsMakeOptionCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            1,
            NULL,
            1,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            50,
            COBJ_MASK_DLLINK(2),
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

// 0x80133698
void mnVSOptionsMakeLabelUnderlineCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            1,
            NULL,
            1,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            40,
            COBJ_MASK_DLLINK(1),
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

// 0x80133738
void mnVSOptionsMakeWallpaperCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            1,
            NULL,
            1,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            80,
            COBJ_MASK_DLLINK(0),
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

// 0x801337D8
void mnVSOptionsMakeDecalCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            1,
            NULL,
            1,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            60,
            COBJ_MASK_DLLINK(4),
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

// 0x80133878
void mnVSOptionsInitVars(void)
{
    sMNVSOptionsOption = (gSCManagerSceneData.scene_prev == nSCKindVSItemSwitch) ? nMNVSOptionsOptionItemSwitch : nMNVSOptionsOptionHandicap;

    sMNVSOptionsHandicapStatus = gSCManagerTransferBattleState.handicap;
    sMNVSOptionsTeamAttackStatus = gSCManagerTransferBattleState.is_team_attack;
    sMNVSOptionsStageSelectStatus = gSCManagerTransferBattleState.is_stage_select;
    sMNVSOptionsDamage = gSCManagerTransferBattleState.damage_ratio;

    sMNVSOptionsFirstAvailableOption = nMNVSOptionsOptionHandicap;

    if (mnVSOptionsCheckHaveItemSwitch() != FALSE)
    {
        sMNVSOptionsLastAvailableOption = nMNVSOptionsOptionItemSwitch;
        sMNVSOptionsIsHaveItemSwitch = TRUE;
    }
    else
    {
        sMNVSOptionsLastAvailableOption = nMNVSOptionsOptionDamage;
        sMNVSOptionsIsHaveItemSwitch = FALSE;
    }
    sMNVSOptionsOptionChangeWait = 0;

    sMNVSOptionsTotalTimeTics = 0;
    sMNVSOptionsReturnTic = sMNVSOptionsTotalTimeTics + I_MIN_TO_TICS(5);
}

// 0x8013394C
void mnVSOptionsSetAllSettings(void)
{
    gSCManagerTransferBattleState.handicap = sMNVSOptionsHandicapStatus;
    gSCManagerTransferBattleState.is_team_attack = sMNVSOptionsTeamAttackStatus;
    gSCManagerTransferBattleState.is_stage_select = sMNVSOptionsStageSelectStatus;
    gSCManagerTransferBattleState.damage_ratio = sMNVSOptionsDamage;

    if (gSCManagerTransferBattleState.handicap == nSCBattleHandicapOff)
    {
        s32 i;

        for (i = 0; i < ARRAY_COUNT(gSCManagerTransferBattleState.players); i++)
        {
            gSCManagerTransferBattleState.players[i].handicap = FTCOMMON_HANDICAP_DEFAULT;
        }
    }
}

// 0x801339C4
void mnVSOptionsSetHandicapSettings(void)
{
    s32 i;

    if (sMNVSOptionsHandicapStatus == nSCBattleHandicapAuto)
    {
        for (i = 0; i < ARRAY_COUNT(gSCManagerTransferBattleState.players); i++)
        {
            gSCManagerTransferBattleState.players[i].handicap = 5;
        }
    }
    else for (i = 0; i < ARRAY_COUNT(gSCManagerTransferBattleState.players); i++)
    {
        gSCManagerTransferBattleState.players[i].handicap = FTCOMMON_HANDICAP_DEFAULT;
    }
}

// 0x80133A40
void mnVSOptionsFuncRun(GObj *gobj)
{
    s32 unused;

    // 0x801347FC
    GObj **option_gobjss[/* */] =
    {
        &sMNVSOptionsOptionGObjs[nMNVSOptionsOptionHandicap],
        &sMNVSOptionsOptionGObjs[nMNVSOptionsOptionTeamAttack],
        &sMNVSOptionsOptionGObjs[nMNVSOptionsOptionStageSelect],
        &sMNVSOptionsOptionGObjs[nMNVSOptionsOptionDamage],
        &sMNVSOptionsOptionGObjs[nMNVSOptionsOptionItemSwitch]
    };

    s32 stick_range;
    sb32 is_button;

    sMNVSOptionsTotalTimeTics++;

    if (sMNVSOptionsTotalTimeTics >= 10)
    {
        if (sMNVSOptionsTotalTimeTics == sMNVSOptionsReturnTic)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            mnVSOptionsSetAllSettings();
            syTaskmanSetLoadScene();
            return;
        }
        if (scSubsysControllerCheckNoInputAll() == FALSE)
        {
            sMNVSOptionsReturnTic = sMNVSOptionsTotalTimeTics + I_MIN_TO_TICS(5);
        }
        if (sMNVSOptionsOptionChangeWait != 0)
        {
            sMNVSOptionsOptionChangeWait--;
        }
        if
        (
            (scSubsysControllerGetPlayerStickInRangeLR(-20, 20) != FALSE)                                         &&
            (scSubsysControllerGetPlayerStickInRangeUD(-20, 20) != FALSE)                                         &&
            (scSubsysControllerGetPlayerHoldButtons(U_JPAD | R_JPAD | R_TRIG | U_CBUTTONS | R_CBUTTONS) == FALSE) &&
            (scSubsysControllerGetPlayerHoldButtons(D_JPAD | L_JPAD | L_TRIG | D_CBUTTONS | L_CBUTTONS) == FALSE)
        )
        {
            sMNVSOptionsOptionChangeWait = 0;
        }
        if ((scSubsysControllerGetPlayerTapButtons(A_BUTTON | START_BUTTON) != FALSE) && (sMNVSOptionsOption == nMNVSOptionsOptionItemSwitch))
        {
            func_800269C0_275C0(nSYAudioFGMMenuSelect);

            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindVSItemSwitch;

            mnVSOptionsSetAllSettings();
            syTaskmanSetLoadScene();
        }
        if (scSubsysControllerGetPlayerTapButtons(B_BUTTON) != FALSE)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindVSMode;

            mnVSOptionsSetAllSettings();
            syTaskmanSetLoadScene();
        }
        if
        (
            mnVSOptionsCheckGetOptionButtonInput(is_button, U_JPAD | U_CBUTTONS) ||
            mnVSOptionsCheckGetOptionStickInputUD(stick_range, 20, 1)
        )
        {
            func_800269C0_275C0(nSYAudioFGMMenuScroll2);

            mnVSOptionsSetOptionChangeWaitP(is_button, stick_range, 7);

            mnVSOptionsSetOptionSpriteColors(*option_gobjss[sMNVSOptionsOption], nMNOptionTabStatusNot);

            if (sMNVSOptionsOption == sMNVSOptionsFirstAvailableOption)
            {
                sMNVSOptionsOption = sMNVSOptionsLastAvailableOption;
            }
            else sMNVSOptionsOption--;

            mnVSOptionsSetOptionSpriteColors(*option_gobjss[sMNVSOptionsOption], nMNOptionTabStatusHighlight);

            if (sMNVSOptionsOption == sMNVSOptionsFirstAvailableOption)
            {
                sMNVSOptionsOptionChangeWait += 8;
            }
            gcEjectGObj(sMNVSOptionsSubtitlesGObj);
            mnVSOptionsMakeSubtitle();
        }
        if
        (
            mnVSOptionsCheckGetOptionButtonInput(is_button, D_JPAD | D_CBUTTONS) ||
            mnVSOptionsCheckGetOptionStickInputUD(stick_range, -20, 0)
        )
        {
            func_800269C0_275C0(nSYAudioFGMMenuScroll2);

            mnVSOptionsSetOptionChangeWaitN(is_button, stick_range, 7);

            mnVSOptionsSetOptionSpriteColors(*option_gobjss[sMNVSOptionsOption], nMNOptionTabStatusNot);

            if (sMNVSOptionsOption == sMNVSOptionsLastAvailableOption)
            {
                sMNVSOptionsOption = sMNVSOptionsFirstAvailableOption;
            }
            else sMNVSOptionsOption++;

            mnVSOptionsSetOptionSpriteColors(*option_gobjss[sMNVSOptionsOption], nMNOptionTabStatusHighlight);

            if (sMNVSOptionsOption == sMNVSOptionsLastAvailableOption)
            {
                sMNVSOptionsOptionChangeWait += 8;
            }
            gcEjectGObj(sMNVSOptionsSubtitlesGObj);
            mnVSOptionsMakeSubtitle();
        }
        if
        (
            mnVSOptionsCheckGetOptionButtonInput(is_button, L_JPAD | L_TRIG | L_CBUTTONS) ||
            mnVSOptionsCheckGetOptionStickInputLR(stick_range, -20, 0)
        )
        {
            switch (sMNVSOptionsOption)
            {
            case nMNVSOptionsOptionHandicap:
                if (sMNVSOptionsHandicapStatus != nSCBattleHandicapOn)
                {
                    sMNVSOptionsHandicapStatus = (sMNVSOptionsHandicapStatus == nSCBattleHandicapOff) ? nSCBattleHandicapAuto : nSCBattleHandicapOn;

                    func_800269C0_275C0(nSYAudioFGMMenuScroll1);
                    gcEjectGObj(sMNVSOptionsSubtitlesGObj);
                    mnVSOptionsMakeSubtitle();
                    mnVSOptionsSetHandicapSettings();
                }
                mnVSOptionsSetHandicapSpriteColors(sMNVSOptionsOptionGObjs[nMNVSOptionsOptionHandicap], sMNVSOptionsHandicapStatus);
                mnVSOptionsSetOptionChangeWaitN(is_button, stick_range, 7);
                break;

            case nMNVSOptionsOptionTeamAttack:
                if (sMNVSOptionsTeamAttackStatus == nMNOptionTabStatusOff)
                {
                    sMNVSOptionsTeamAttackStatus = nMNOptionTabStatusOn;

                    func_800269C0_275C0(nSYAudioFGMMenuScroll1);
                    gcEjectGObj(sMNVSOptionsSubtitlesGObj);
                    mnVSOptionsMakeSubtitle();
                }
                mnVSOptionsSetToggleSpriteColors(sMNVSOptionsOptionGObjs[nMNVSOptionsOptionTeamAttack], sMNVSOptionsTeamAttackStatus);
                mnVSOptionsSetOptionChangeWaitN(is_button, stick_range, 7);
                break;

            case nMNVSOptionsOptionStageSelect:
                if (sMNVSOptionsStageSelectStatus == nMNOptionTabStatusOff)
                {
                    sMNVSOptionsStageSelectStatus = nMNOptionTabStatusOn;

                    func_800269C0_275C0(nSYAudioFGMMenuScroll1);
                    gcEjectGObj(sMNVSOptionsSubtitlesGObj);
                    mnVSOptionsMakeSubtitle();
                }
                mnVSOptionsSetToggleSpriteColors(sMNVSOptionsOptionGObjs[nMNVSOptionsOptionStageSelect], sMNVSOptionsStageSelectStatus);
                mnVSOptionsSetOptionChangeWaitN(is_button, stick_range, 7);
                break;

            case nMNVSOptionsOptionDamage:
                func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                if (sMNVSOptionsDamage == 50)
                {
                    sMNVSOptionsDamage = 200;
                }
                else sMNVSOptionsDamage--;

                mnVSOptionsSetOptionChangeWaitN(is_button, stick_range, 14);

                if (sMNVSOptionsDamage == 50)
                {
                    sMNVSOptionsOptionChangeWait += 8;
                }
                gcEjectGObj(sMNVSOptionsDamageGObj);
                mnVSOptionsMakeDamageDigits();
                break;
            }
        }
        if
        (
            mnVSOptionsCheckGetOptionButtonInput(is_button, R_JPAD | R_TRIG | R_CBUTTONS) ||
            mnVSOptionsCheckGetOptionStickInputLR(stick_range, 20, 1)
        )
        {
            switch (sMNVSOptionsOption)
            {
            case nMNVSOptionsOptionHandicap:
                if (sMNVSOptionsHandicapStatus != nSCBattleHandicapOff)
                {
                    sMNVSOptionsHandicapStatus = (sMNVSOptionsHandicapStatus == nSCBattleHandicapOn) ? nSCBattleHandicapAuto : nSCBattleHandicapOff;

                    func_800269C0_275C0(nSYAudioFGMMenuScroll1);
                    gcEjectGObj(sMNVSOptionsSubtitlesGObj);
                    mnVSOptionsMakeSubtitle();
                    mnVSOptionsSetHandicapSettings();
                }
                mnVSOptionsSetHandicapSpriteColors(sMNVSOptionsOptionGObjs[nMNVSOptionsOptionHandicap], sMNVSOptionsHandicapStatus);
                mnVSOptionsSetOptionChangeWaitP(is_button, stick_range, 7);
                break;

            case nMNVSOptionsOptionTeamAttack:
                if (sMNVSOptionsTeamAttackStatus == nMNOptionTabStatusOn)
                {
                    sMNVSOptionsTeamAttackStatus = nMNOptionTabStatusOff;

                    func_800269C0_275C0(nSYAudioFGMMenuScroll1);
                    gcEjectGObj(sMNVSOptionsSubtitlesGObj);
                    mnVSOptionsMakeSubtitle();
                }
                mnVSOptionsSetToggleSpriteColors(sMNVSOptionsOptionGObjs[nMNVSOptionsOptionTeamAttack], sMNVSOptionsTeamAttackStatus);
                mnVSOptionsSetOptionChangeWaitP(is_button, stick_range, 7);
                break;

            case nMNVSOptionsOptionStageSelect:
                if (sMNVSOptionsStageSelectStatus == nMNOptionTabStatusOn)
                {
                    sMNVSOptionsStageSelectStatus = nMNOptionTabStatusOff;

                    func_800269C0_275C0(nSYAudioFGMMenuScroll1);
                    gcEjectGObj(sMNVSOptionsSubtitlesGObj);
                    mnVSOptionsMakeSubtitle();
                }
                mnVSOptionsSetToggleSpriteColors(sMNVSOptionsOptionGObjs[nMNVSOptionsOptionStageSelect], sMNVSOptionsStageSelectStatus);
                mnVSOptionsSetOptionChangeWaitP(is_button, stick_range, 7);
                break;

            case nMNVSOptionsOptionDamage:
                if (sMNVSOptionsDamage == 200)
                {
                    sMNVSOptionsDamage = 50;
                }
                else sMNVSOptionsDamage++;

                mnVSOptionsSetOptionChangeWaitP(is_button, stick_range, 14);

                if (sMNVSOptionsDamage == 200)
                {
                    sMNVSOptionsOptionChangeWait += 8;
                }
                func_800269C0_275C0(nSYAudioFGMMenuScroll1);
                gcEjectGObj(sMNVSOptionsDamageGObj);
                mnVSOptionsMakeDamageDigits();
                break;
            }
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON) != FALSE)
        {
            switch (sMNVSOptionsOption)
            {
            case nMNVSOptionsOptionHandicap:
                func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                if (sMNVSOptionsHandicapStatus == nSCBattleHandicapOff)
                {
                    sMNVSOptionsHandicapStatus = nSCBattleHandicapOn;
                }
                else if (sMNVSOptionsHandicapStatus == nSCBattleHandicapAuto)
                {
                    sMNVSOptionsHandicapStatus = nSCBattleHandicapOff;
                }
                else sMNVSOptionsHandicapStatus = nSCBattleHandicapAuto;

                mnVSOptionsSetHandicapSpriteColors(sMNVSOptionsOptionGObjs[nMNVSOptionsOptionHandicap], sMNVSOptionsHandicapStatus);
                gcEjectGObj(sMNVSOptionsSubtitlesGObj);
                mnVSOptionsMakeSubtitle();
                mnVSOptionsSetHandicapSettings();
                break;

            case nMNVSOptionsOptionTeamAttack:
                func_800269C0_275C0(nSYAudioFGMMenuScroll1);
                sMNVSOptionsTeamAttackStatus = (sMNVSOptionsTeamAttackStatus == nMNOptionTabStatusOn) ? nMNOptionTabStatusOff : nMNOptionTabStatusOn;

                mnVSOptionsSetToggleSpriteColors(sMNVSOptionsOptionGObjs[nMNVSOptionsOptionTeamAttack], sMNVSOptionsTeamAttackStatus);
                gcEjectGObj(sMNVSOptionsSubtitlesGObj);
                mnVSOptionsMakeSubtitle();
                break;

            case nMNVSOptionsOptionStageSelect:
                func_800269C0_275C0(nSYAudioFGMMenuScroll1);
                sMNVSOptionsStageSelectStatus = (sMNVSOptionsStageSelectStatus == nMNOptionTabStatusOn) ? nMNOptionTabStatusOff : nMNOptionTabStatusOn;

                mnVSOptionsSetToggleSpriteColors(sMNVSOptionsOptionGObjs[nMNVSOptionsOptionStageSelect], sMNVSOptionsStageSelectStatus);
                gcEjectGObj(sMNVSOptionsSubtitlesGObj);
                mnVSOptionsMakeSubtitle();
                break;
            }
        }
    }
}

/* mnvsoptions.c:1537-1549: lbRelocInitSetup and lbRelocLoadFilesListed
 * over dMNVSOptionsFileIDs; here, the banks. A bank that is not there
 * leaves its file NULL and sprite_bank_get says so per sprite. */
void mnVSOptionsLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMNVSOptionsFileIDs)] =
    {
        MNVSOPTIONS_BANK_COMMON,
        MNVSOPTIONS_BANK_VSOPTIONS
    };
    s32 i;

    for (i = 0; i < ARRAY_COUNT(dMNVSOptionsFileIDs); i++)
    {
        if (sprite_bank_load(&sMNVSOptionsBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mnVSOptions: no bank for file %d (%s)\n",
                          (int)dMNVSOptionsFileIDs[i], paths[i]);
            sMNVSOptionsFiles[i] = NULL;
            continue;
        }
        sMNVSOptionsFiles[i] = &sMNVSOptionsBanks[i];
    }
}

/* mnvsoptions.c:1537-1578 0x80134504, in the game's order. DIVERGES:
 * the clear camera (gcMakeDefaultCameraGObj on link 0, DL priority 100,
 * a black fill) is the frame clear, which the PVR does itself -- the
 * same cut as the VS mode's. */
void mnVSOptionsFuncStart(void)
{
    mnVSOptionsLoadFiles();

    gcMakeGObjSPAfter(0, mnVSOptionsFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

    mnVSOptionsInitVars();
    mnVSOptionsMakeWallpaperCamera();
    mnVSOptionsMakeLabelUnderlineCamera();
    mnVSOptionsMakeOptionCamera();
    mnVSOptionsMakeTintCamera();
    mnVSOptionsMakeDecalCamera();
    mnVSOptionsMakeWallpaper();
    mnVSOptionsMakeTint();
    mnVSOptionsMakeDecal();
    mnVSOptionsMakeLabel();
    mnVSOptionsMakeHandicapOption();
    mnVSOptionsMakeTeamAttackOption();
    mnVSOptionsMakeStageSelectOption();
    mnVSOptionsMakeDamageOption();
    mnVSOptionsMakeDamageDigits();

    if (sMNVSOptionsIsHaveItemSwitch != FALSE)
    {
        mnVSOptionsMakeItemSwitchOption();
    }
    mnVSOptionsMakeUnderline();
    mnVSOptionsMakeSubtitle();
}

/* mnvsoptions.c:1580 dMNVSOptionsVideoSetup is the N64's video mode:
 * see mnVSOptionsStartScene. */

/* mnvsoptions.c:1583-1625 (0x8013482C). The pool counts are the game's,
 * all zero: a menu scene takes its objects straight from the scene
 * heap. DIVERGES as the VS mode's: the arena is the port's region, the
 * draw is the scene manager's, and mnVSOptionsFuncLights is dropped. */
SYTaskmanSetup dMNVSOptionsTaskmanSetup =
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

    mnVSOptionsFuncStart                // Task start function
};

/* The bzero arm of syDmaLoadOverlay for overlay 20, this
 * file: sc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnVSOptionsOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNVSOptionsOptionGObjs);
    OVERLAY_CLEAR(sMNVSOptionsOption);
    OVERLAY_CLEAR(sMNVSOptionsHandicapStatus);
    OVERLAY_CLEAR(sMNVSOptionsTeamAttackStatus);
    OVERLAY_CLEAR(sMNVSOptionsStageSelectStatus);
    OVERLAY_CLEAR(sMNVSOptionsDamage);
    OVERLAY_CLEAR(sMNVSOptionsSubtitlesGObj);
    OVERLAY_CLEAR(sMNVSOptionsDamageGObj);
    OVERLAY_CLEAR(sMNVSOptionsFirstAvailableOption);
    OVERLAY_CLEAR(sMNVSOptionsLastAvailableOption);
    OVERLAY_CLEAR(sMNVSOptionsIsHaveItemSwitch);
    OVERLAY_CLEAR(sMNVSOptionsOptionChangeWait);
    OVERLAY_CLEAR(sMNVSOptionsTotalTimeTics);
    OVERLAY_CLEAR(sMNVSOptionsReturnTic);
    OVERLAY_CLEAR(sMNVSOptionsFiles);
    OVERLAY_CLEAR(sMNVSOptionsBanks);
}

/* mnvsoptions.c:1628-1635 0x80134668. DIVERGES as the VS mode's:
 * syVideoInit and the zbuffer are the N64's video mode, set once at
 * boot here, and the arena_size line is the link map. What is left is
 * the last line. */
void mnVSOptionsStartScene(void)
{
    syTaskmanStartTask(&dMNVSOptionsTaskmanSetup);
}
