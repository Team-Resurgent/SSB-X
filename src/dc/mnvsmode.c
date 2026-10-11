/* mnvsmode.c -- see mnvsmode.h. Every function is mn/mnvsmode/mnvsmode.c's
 * by name and body, REGION_US arms; the line numbers are the decomp's. */
#include "mnvsmode.h"
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
#include <mn/mndef.h>
#include <PR/os.h>

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/sysshim.c defines it over the FGM engine */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* ft/ftparam.h:2642,2648 (src/dc/ftparam.c); the header itself wants
 * the effect types behind it, which the port has no header for */
s32 ftParamGetCostumeCommonID(s32 fkind, s32 color);
s32 ftParamGetCostumeTeamID(s32 fkind, s32 color);

// // // // // // // // // // // //
//                               //
//       EXTERNAL VARIABLES      //
//                               //
// // // // // // // // // // // //

extern ub8 gSYMainImemOK;

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mnmodeselect.c does the same). Each `&llXxxSprite`
 * below is written `llXxxSprite`, the number, with the label's name
 * kept as the macro's. The values are relocData files 0 (MNCommon) and
 * 6 (MNVSMode) as tools/export/ssb_spriteexport.py --list reads them off the
 * ROM. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* mnVSModeGetShade and mnVSModeGetCostume run off the end of their
 * loops without a return, as the decomp's do (IDO left the last i in
 * v0); the decomp builds with -Wno-return-type and the text stays as
 * it is */
#pragma GCC diagnostic ignored "-Wreturn-type"
/* and the decomp's share of unused locals (the digit widths table,
 * `unused`), which the decomp's own build turns off too */
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
/* and mnVSModeUpdateButton's `colors`, set on every status the callers
 * pass and on none the switch's default arm would see */
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

#define llMNCommonOptionTabLeftSprite    0x001E8
#define llMNCommonOptionTabMiddleSprite  0x00330
#define llMNCommonOptionTabRightSprite   0x00568
#define llMNCommonDecalPaperSprite       0x02A30
#define llMNCommonSmashLogoSprite        0x031F8
#define llMNCommonGameModeTextSprite     0x0D240
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
#define llMNCommonInfinitySprite         0x0DC48
#define llMNCommonArrowRSprite           0x0DD90
#define llMNCommonArrowLSprite           0x0DE30
#define llMNCommonSmashBrosCollageSprite 0x18000

#define llMNVSModeVSStartTextSprite      0x24C8
#define llMNVSModeRulePeriodTextSprite   0x2748
#define llMNVSModeTimeTextSprite         0x28E0
#define llMNVSModeStockTextSprite        0x2A80
#define llMNVSModeTeamTextSprite         0x2C20
#define llMNVSModeTimePeriodTextSprite   0x2EC8
#define llMNVSModeMinTextSprite          0x2FC8
#define llMNVSModeStockPeriodTextSprite  0x3248
#define llMNVSModeVSOptionsTextSprite    0x3828
#define llMNVSModeConsoleIconDarkSprite  0x5EB0
#define llMNVSModeVSTextSprite           0x6118

/* The banks the two files became (src/dc/mnmodeselect.c on where they
 * live). The common bank is the mode select's,
 * exported with this scene's sprites in it as well. */
#define MNVSMODE_BANK_COMMON "mncommon.spr"
#define MNVSMODE_BANK_VSMODE "mnvsmode.spr"

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mnvsmode.c:26 (0x801347B0): { &llMNCommonFileID, &llMNVSModeFileID },
 * the two link labels' values written out. */
u32 dMNVSModeFileIDs[/* */] = { 0, 6 };

/* mnvsmode.c:29-37 dMNVSModeLights1 and dMNVSModeDisplayList, the
 * lighting the pre-render function would set for 3D this scene never
 * draws, are not here: see dMNVSModeTaskmanSetup. */

// 0x80134930
GObj* sMNVSModeButtonGObjVSStart;

// 0x80134934
GObj* sMNVSModeButtonGObjRule;

// 0x80134938
GObj* sMNVSModeButtonGObjTimeStock;

// 0x8013493C
GObj* sMNVSModeButtonGObjVSOptions;

// 0x80134940 - Padding?
s32 sMNVSModePad0x80134940[2];

// 0x80134948
s32 sMNVSModeCursorIndex;

// 0x8013494C
s32 sMNVSModeRule;

// 0x80134950
s32 sMNVSModeTime;

// 0x80134954
s32 sMNVSModeStock;

// 0x80134958
GObj* sMNVSModeRuleValueGObj;

// 0x8013495C
GObj* sMNVSModeTimeStockValueGObj;

// 0x80134960
GObj* sMNVSModeUnusedGObj;

// 0x80134964
GObj* sMNVSModeRuleArrowsGObj;

// 0x80134968
GObj* sMNVSModeTimeStockArrowsGObj;

// 0x8013496C
s32 sMNVSModeTimeStockArrowBlinkTimer;

// 0x80134970
s32 sMNVSModeRuleArrowBlinkTimer;

// 0x80134974
s32 sMNVSModeExitInterrupt;

// 0x80134978
s32 sMNVSModeInputDirection;

// 0x8013497C
s32 sMNVSModeChangeWait;

// 0x80134980
s32 sMNVSModeTotalTimeTics;

// 0x80134984
s32 sMNVSModeReturnTic;


/* mnvsmode.c:106 sMNVSModeStatusBuffer, the reloc status buffer, is
 * not here: mnVSModeLoadFiles. */

// 0x80134A48
void *sMNVSModeFiles[ARRAY_COUNT(dMNVSModeFileIDs)];

/* the banks behind sMNVSModeFiles */
static SpriteBank sMNVSModeBanks[ARRAY_COUNT(dMNVSModeFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mnvsmode.c:121-124 mnVSModeFuncLights 0x80131B00, a gSPDisplayList of
 * the lighting above, is not here: dMNVSModeTaskmanSetup. */

// 0x80131B24
s32 mnVSModePow(s32 num, s32 pow)
{
    if (pow == 0) 
    {
        return 1;
    }
    else
    {
        s32 result = num, i = pow;

        if (pow >= 2)
        {
            do
            {
                result *= num;
            }
            while (--i != 1);
        }
        return result;
    }
}

// 0x80131BC4
void mnVSModeSetTextureColors(SObj* sobj, u32 *colors)
{
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;
    sobj->sprite.red = colors[0];
    sobj->sprite.green = colors[1];
    sobj->sprite.blue = colors[2];
}

// 0x80131BF4
s32 mnVSModeGetNumberOfDigits(s32 num, s32 maxDigits)
{
    s32 numDigits;

    for (numDigits = maxDigits; numDigits > 0; numDigits--)
    {
        if (mnVSModePow(10, numDigits - 1) != 0 ? num / mnVSModePow(10, numDigits - 1) : 0 != 0) return numDigits;
    }

    return 0;
}

// 0x80131CA0
void mnVSModeMakeNumber(GObj* number_gobj, s32 num, f32 x, f32 y, u32 *colors, s32 maxDigits, sb32 pad)
{
    // 0x801347F8
    intptr_t number_offsets[/* */] =
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

    // 0x80134820
    Vec2f floats[/* */] =
    {
        { 10.0F,  6.0F }, 
        {  9.0F,  9.0F },
        { 10.0F,  9.0F }, 
        {  9.0F, 10.0F },
        {  9.0F, 10.0F }
    };

    SObj* number_sobj;
    f32 left_x = x;
    s32 place;
    s32 numDigits;
    s32 digit;

    if (num < 0) num = 0;

    number_sobj = lbCommonMakeSObjForGObj(number_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[0], number_offsets[num % 10]));
    mnVSModeSetTextureColors(number_sobj, colors);
    left_x -= 11.0F;
    number_sobj->pos.x = left_x;
    number_sobj->pos.y = y;

    for
    (
        place = 1, numDigits = (pad != FALSE) ? maxDigits : mnVSModeGetNumberOfDigits(num, maxDigits);
        place < numDigits;
        place++, numDigits = (pad != FALSE) ? maxDigits : mnVSModeGetNumberOfDigits(num, maxDigits)
    )
    {
        digit = (mnVSModePow(10, place) != 0) ? num / mnVSModePow(10, place) : 0;

        number_sobj = lbCommonMakeSObjForGObj(number_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[0], number_offsets[digit % 10]));
        mnVSModeSetTextureColors(number_sobj, colors);
        left_x -= 11.0F;
        number_sobj->pos.x = left_x;
        number_sobj->pos.y = y;
    }
}

// 0x80131F4C
void mnVSModeUpdateButton(GObj* button_gobj, s32 button_status)
{
    // 0x80134848
    SYColorRGBPair selcolors = { { 0x00, 0x00, 0x00 }, { 0xFF, 0xFF, 0xFF } };

    // 0x80134850
    SYColorRGBPair hicolors = { { 0x82, 0x00, 0x28 }, { 0xFF, 0x00, 0x28 } };

    // 0x80134858
    SYColorRGBPair notcolors = { { 0x00, 0x00, 0x00 }, { 0x82, 0x82, 0xAA } };

    SYColorRGBPair *colors;
    s32 i;
    SObj* button_sobj;

    switch (button_status)
    {
    case nMNOptionTabStatusHighlight:
        colors = &hicolors;
        break;
    
    case nMNOptionTabStatusNot:
        colors = &notcolors;
        break;

    case nMNOptionTabStatusSelected:
        colors = &selcolors;
        break;

    default:
        break;
    }

    button_sobj = SObjGetStruct(button_gobj);

    for (i = 0; i < 3; i++)
    {
        button_sobj->envcolor.r = colors->prim.r;
        button_sobj->envcolor.g = colors->prim.g;
        button_sobj->envcolor.b = colors->prim.b;
        
        button_sobj->sprite.red = colors->env.r;
        button_sobj->sprite.green = colors->env.g;
        button_sobj->sprite.blue = colors->env.b;

        button_sobj = button_sobj->next;
    }
}

// 0x80132024
void mnVSModeMakeButton(GObj* button_gobj, f32 x, f32 y, s32 arg3)
{
    SObj* button_sobj;

    button_sobj = lbCommonMakeSObjForGObj(button_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[0], llMNCommonOptionTabLeftSprite));
    button_sobj->sprite.attr &= ~SP_FASTCOPY;
    button_sobj->sprite.attr |= SP_TRANSPARENT;
    button_sobj->pos.x = x;
    button_sobj->pos.y = y;

    button_sobj = lbCommonMakeSObjForGObj(button_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[0], llMNCommonOptionTabMiddleSprite));
    button_sobj->sprite.attr &= ~SP_FASTCOPY;
    button_sobj->sprite.attr |= SP_TRANSPARENT;
    button_sobj->pos.x = x + 16.0F;
    button_sobj->pos.y = y;
    button_sobj->cms = 0;
    button_sobj->cmt = 0;
    button_sobj->masks = 4;
    button_sobj->maskt = 0;
    button_sobj->lrs = arg3 * 8;
    button_sobj->lrt = 0x1D;

    button_sobj = lbCommonMakeSObjForGObj(button_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[0], llMNCommonOptionTabRightSprite));
    button_sobj->sprite.attr &= ~SP_FASTCOPY;
    button_sobj->sprite.attr |= SP_TRANSPARENT;
    button_sobj->pos.x = x + 16.0F + (arg3 * 8);
    button_sobj->pos.y = y;
}

// 0x80132154
void mnVSModeMakeVSStartButton()
{
    GObj* button_gobj;
    SObj* button_sobj;

    sMNVSModeButtonGObjVSStart = button_gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(button_gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);
    mnVSModeMakeButton(button_gobj, 120.0F, 31.0F, 17);

    mnVSModeUpdateButton(button_gobj, (sMNVSModeCursorIndex == 0) ? nMNOptionTabStatusHighlight : nMNOptionTabStatusNot);

    button_sobj = lbCommonMakeSObjForGObj(button_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeVSStartTextSprite));
    button_sobj->sprite.attr &= ~SP_FASTCOPY;
    button_sobj->sprite.attr |= SP_TRANSPARENT;
    button_sobj->sprite.red = 0x00;
    button_sobj->sprite.green = 0x00;
    button_sobj->sprite.blue = 0x00;
    button_sobj->pos.x = 153.0F;
    button_sobj->pos.y = 36.0F;
}

// 0x80132238
void mnVSModeMakeRuleValue()
{
    GObj* rule_value_gobj;
    SObj* rule_value_sobj;

    // 0x80134860
    SYColorRGB color = { 0xFF, 0xFF, 0xFF };

    sMNVSModeRuleValueGObj = rule_value_gobj = gcMakeGObjSPAfter(0, NULL, 5, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(rule_value_gobj, lbCommonDrawSObjAttr, 3, GOBJ_PRIORITY_DEFAULT, ~0);

    switch (sMNVSModeRule)
    {
        case nMNVSModeRuleStock:
            rule_value_sobj = lbCommonMakeSObjForGObj(rule_value_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeStockTextSprite));
            rule_value_sobj->sprite.attr &= ~SP_FASTCOPY;
            rule_value_sobj->sprite.attr |= SP_TRANSPARENT;
            rule_value_sobj->pos.x = 183.0F;
            rule_value_sobj->pos.y = 78.0F;
            rule_value_sobj->sprite.red = color.r;
            rule_value_sobj->sprite.green = color.g;
            rule_value_sobj->sprite.blue = color.b;
            return;
        case nMNVSModeRuleTime:
            rule_value_sobj = lbCommonMakeSObjForGObj(rule_value_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeTimeTextSprite));
            rule_value_sobj->sprite.attr &= ~SP_FASTCOPY;
            rule_value_sobj->sprite.attr |= SP_TRANSPARENT;
            rule_value_sobj->pos.x = 187.0F;
            rule_value_sobj->pos.y = 78.0F;
            rule_value_sobj->sprite.red = color.r;
            rule_value_sobj->sprite.green = color.g;
            rule_value_sobj->sprite.blue = color.b;
            return;
        case nMNVSModeRuleStockTeam:
            rule_value_sobj = lbCommonMakeSObjForGObj(rule_value_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeStockTextSprite));
            rule_value_sobj->sprite.attr &= ~SP_FASTCOPY;
            rule_value_sobj->sprite.attr |= SP_TRANSPARENT;
            rule_value_sobj->pos.x = 165.0F;
            rule_value_sobj->pos.y = 78.0F;
            rule_value_sobj->sprite.red = color.r;
            rule_value_sobj->sprite.green = color.g;
            rule_value_sobj->sprite.blue = color.b;

            rule_value_sobj = lbCommonMakeSObjForGObj(rule_value_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeTeamTextSprite));
            rule_value_sobj->sprite.attr &= ~SP_FASTCOPY;
            rule_value_sobj->sprite.attr |= SP_TRANSPARENT;
            rule_value_sobj->pos.x = 212.0F;
            rule_value_sobj->pos.y = 78.0F;
            rule_value_sobj->sprite.red = color.r;
            rule_value_sobj->sprite.green = color.g;
            rule_value_sobj->sprite.blue = color.b;
            return;
        case nMNVSModeRuleTimeTeam:
            rule_value_sobj = lbCommonMakeSObjForGObj(rule_value_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeTimeTextSprite));
            rule_value_sobj->sprite.attr &= ~SP_FASTCOPY;
            rule_value_sobj->sprite.attr |= SP_TRANSPARENT;
            rule_value_sobj->pos.x = 168.0F;
            rule_value_sobj->pos.y = 78.0F;
            rule_value_sobj->sprite.red = color.r;
            rule_value_sobj->sprite.green = color.g;
            rule_value_sobj->sprite.blue = color.b;

            rule_value_sobj = lbCommonMakeSObjForGObj(rule_value_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeTeamTextSprite));
            rule_value_sobj->sprite.attr &= ~SP_FASTCOPY;
            rule_value_sobj->sprite.attr |= SP_TRANSPARENT;
            rule_value_sobj->pos.x = 212.0F;
            rule_value_sobj->pos.y = 78.0F;
            rule_value_sobj->sprite.red = color.r;
            rule_value_sobj->sprite.green = color.g;
            rule_value_sobj->sprite.blue = color.b;
            return;
    }
}

// 0x80132524
SObj* mnVSModeGetArrowSObj(GObj* arrow_gobj, s32 direction)
{
    SObj* next_arrow_sobj;
    SObj* first_arrow_sobj;

    first_arrow_sobj = SObjGetStruct(arrow_gobj);

    if (first_arrow_sobj != NULL)
    {
        if (direction == first_arrow_sobj->user_data.s)
        {
            return first_arrow_sobj;
        }
        next_arrow_sobj = first_arrow_sobj->next;

        if ((next_arrow_sobj != NULL) && (direction == next_arrow_sobj->user_data.s))
        {
            return next_arrow_sobj;
        }
    }
    return NULL;
}

// 0x80132570
void mnVSModeMakeLeftArrow(GObj* arrow_gobj, f32 x, f32 y)
{
    SObj* arrow_sobj;

    arrow_sobj = lbCommonMakeSObjForGObj(arrow_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[0], llMNCommonArrowLSprite));
    arrow_sobj->user_data.s = 0;
    arrow_sobj->sprite.attr &= ~SP_FASTCOPY;
    arrow_sobj->sprite.attr |= SP_TRANSPARENT;
    arrow_sobj->pos.x = x;
    arrow_sobj->pos.y = y;
    arrow_sobj->sprite.red = 0xFF;
    arrow_sobj->sprite.green = 0xAE;
    arrow_sobj->sprite.blue = 0x00;
}

// 0x801325E4
void mnVSModeMakeRightArrow(GObj* arrow_gobj, f32 x, f32 y)
{
    SObj* arrow_sobj;

    arrow_sobj = lbCommonMakeSObjForGObj(arrow_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[0], llMNCommonArrowRSprite));
    arrow_sobj->user_data.s = 1;
    arrow_sobj->sprite.attr &= ~SP_FASTCOPY;
    arrow_sobj->sprite.attr |= SP_TRANSPARENT;
    arrow_sobj->pos.x = x;
    arrow_sobj->pos.y = y;
    arrow_sobj->sprite.red = 0xFF;
    arrow_sobj->sprite.green = 0xAE;
    arrow_sobj->sprite.blue = 0x00;
}

// 0x8013265C
void mnVSModeAnimateRuleArrows(GObj* rule_arrows_gobj)
{
    SObj* arrow_sobj;

    while (TRUE)
    {
        if (sMNVSModeCursorIndex == 1)
        {
            sMNVSModeRuleArrowBlinkTimer--;

            if (sMNVSModeRuleArrowBlinkTimer == 0)
            {
                if (rule_arrows_gobj->flags == GOBJ_FLAG_HIDDEN)
                {
                    rule_arrows_gobj->flags = GOBJ_FLAG_NONE;
                }
                else rule_arrows_gobj->flags = GOBJ_FLAG_HIDDEN;

                sMNVSModeRuleArrowBlinkTimer = 30;
            }

            if (sMNVSModeRule == nMNVSModeRuleTime)
            {
                arrow_sobj = mnVSModeGetArrowSObj(rule_arrows_gobj, 0);

                if (arrow_sobj != NULL)
                {
                    gcEjectSObj(arrow_sobj);
                }
            }
            else if (mnVSModeGetArrowSObj(rule_arrows_gobj, 0) == NULL)
            {
                mnVSModeMakeLeftArrow(rule_arrows_gobj, 165.0F, 70.0F);
            }

            if (sMNVSModeRule == nMNVSModeRuleStockTeam)
            {
                arrow_sobj = mnVSModeGetArrowSObj(rule_arrows_gobj, 1);

                if (arrow_sobj != NULL)
                {
                    gcEjectSObj(arrow_sobj);
                }
            }
            else if (mnVSModeGetArrowSObj(rule_arrows_gobj, 1) == NULL)
            {
                mnVSModeMakeRightArrow(rule_arrows_gobj, 250.0F, 70.0F);
            }
        }
        else rule_arrows_gobj->flags = GOBJ_FLAG_HIDDEN;

        gcSleepCurrentGObjThread(1);
    }
}

// 0X80132818
void mnVSModeMakeRuleArrows()
{
    GObj* rule_arrows_gobj;

    if (sMNVSModeRuleArrowsGObj != NULL)
    {
        gcEjectGObj(sMNVSModeRuleArrowsGObj);

        sMNVSModeRuleArrowsGObj = NULL;
    }
    sMNVSModeRuleArrowsGObj = rule_arrows_gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(rule_arrows_gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(rule_arrows_gobj, mnVSModeAnimateRuleArrows, 0, 1);
}

// 0x801328A8
void mnVSModeAnimateTimeStockArrows(GObj* time_stock_arrows_gobj)
{
    while (TRUE)
    {
        if (sMNVSModeCursorIndex == 2)
        {
            sMNVSModeTimeStockArrowBlinkTimer--;

            if (sMNVSModeTimeStockArrowBlinkTimer == 0)
            {
                if (time_stock_arrows_gobj->flags == GOBJ_FLAG_HIDDEN)
                {
                    time_stock_arrows_gobj->flags = GOBJ_FLAG_NONE;
                }
                else time_stock_arrows_gobj->flags = GOBJ_FLAG_HIDDEN;

                sMNVSModeTimeStockArrowBlinkTimer = 30;
            }
        }
        else time_stock_arrows_gobj->flags = GOBJ_FLAG_HIDDEN;

        gcSleepCurrentGObjThread(1);
    }
}

// 0x80132964
void mnVSModeMakeTimeStockArrows()
{
    GObj* time_stock_arrows_gobj;

    if (sMNVSModeTimeStockArrowsGObj != NULL)
    {
        gcEjectGObj(sMNVSModeTimeStockArrowsGObj);

        sMNVSModeTimeStockArrowsGObj = NULL;
    }

    sMNVSModeTimeStockArrowsGObj = time_stock_arrows_gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(time_stock_arrows_gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(time_stock_arrows_gobj, mnVSModeAnimateTimeStockArrows, nGCProcessKindThread, 1);

    if (mnVSModeIsTime() != FALSE)
    {
        mnVSModeMakeLeftArrow(time_stock_arrows_gobj, 155.0F, 109.0F);
        mnVSModeMakeRightArrow(time_stock_arrows_gobj, 230.0F, 109.0F);
    }
    else
    {
        mnVSModeMakeLeftArrow(time_stock_arrows_gobj, 165.0F, 109.0F);
        mnVSModeMakeRightArrow(time_stock_arrows_gobj, 230.0F, 109.0F);
    }
}

// 0x80132A4C
void mnVSModeMakeRuleButton()
{
    GObj* rule_button_gobj;
    SObj* button_sobj;

    sMNVSModeButtonGObjRule = rule_button_gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(rule_button_gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);
    mnVSModeMakeButton(rule_button_gobj, 97.0F, 70.0F, 17);

    mnVSModeUpdateButton(rule_button_gobj, (sMNVSModeCursorIndex == nMNVSModeOptionRule) ? nMNOptionTabStatusHighlight : nMNOptionTabStatusNot);

    button_sobj = lbCommonMakeSObjForGObj(rule_button_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeRulePeriodTextSprite));
    button_sobj->sprite.attr &= ~SP_FASTCOPY;
    button_sobj->sprite.attr |= SP_TRANSPARENT;
    button_sobj->sprite.red = 0x00;
    button_sobj->sprite.green = 0x00;
    button_sobj->sprite.blue = 0x00;
    button_sobj->pos.x = 108.0F;
    button_sobj->pos.y = 75.0F;

    mnVSModeMakeRuleArrows();
}

// 0x80132B38
sb32 mnVSModeIsTime()
{
    if ((sMNVSModeRule == nMNVSModeRuleTime) || (sMNVSModeRule == nMNVSModeRuleTimeTeam))
    {
        return TRUE;
    }
    else return FALSE;
}

// 0x80132B68
s32 mnVSModeGetTimeStockValue()
{
    if (mnVSModeIsTime() != FALSE)
    {
        return sMNVSModeTime;
    }
    else return sMNVSModeStock + 1;
}

// 0x80132BA0
void mnVSModeMakeTimeStockValue()
{
    GObj* time_stock_value_gobj;
    SObj* time_stock_value_sobj;
    s32 value;
    s32 x;
    s32 unused;

    // 0x80134864
    u32 colors[/* */] = { 0x000000FF, 0x000000FF, 0x000000FF };

    sMNVSModeTimeStockValueGObj = time_stock_value_gobj = gcMakeGObjSPAfter(0, NULL, 5, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(time_stock_value_gobj, lbCommonDrawSObjAttr, 3, GOBJ_PRIORITY_DEFAULT, ~0);

    value = mnVSModeGetTimeStockValue();

    if (value == SCBATTLE_TIMELIMIT_INFINITE)
    {
        time_stock_value_sobj = lbCommonMakeSObjForGObj(time_stock_value_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[0], llMNCommonInfinitySprite));
        time_stock_value_sobj->sprite.attr &= ~SP_FASTCOPY;
        time_stock_value_sobj->sprite.attr |= SP_TRANSPARENT;
        time_stock_value_sobj->pos.x = 162.0F;
        time_stock_value_sobj->pos.y = 118.0F;
        time_stock_value_sobj->sprite.red = colors[0];
        time_stock_value_sobj->sprite.green = colors[1];
        time_stock_value_sobj->sprite.blue = colors[2];
    }
    else
    {
        if (mnVSModeIsTime() != FALSE)
        {
            x = (value < 10) ? 185 : 190;
        }
        else
        {
            x = (value < 10) ? 210 : 215;
        }
        mnVSModeMakeNumber(time_stock_value_gobj, value, (f32)x, 116.0F, colors, 2, 0);
    }
}

// 0x80132D04
void mnVSModeMakeTimeStockButton()
{
    GObj* time_stock_button_gobj;
    SObj* time_stock_button_sobj;

    sMNVSModeButtonGObjTimeStock = time_stock_button_gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(time_stock_button_gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);
    mnVSModeMakeButton(time_stock_button_gobj, 74.0F, 109.0F, 17);

    mnVSModeUpdateButton(time_stock_button_gobj, (sMNVSModeCursorIndex == nMNVSModeOptionTimeStock) ? nMNOptionTabStatusHighlight : nMNOptionTabStatusNot);

    if ((sMNVSModeRule == nMNVSModeRuleTime) || (sMNVSModeRule == nMNVSModeRuleTimeTeam))
    {
        time_stock_button_sobj = lbCommonMakeSObjForGObj(time_stock_button_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeTimePeriodTextSprite));
        time_stock_button_sobj->sprite.attr &= ~SP_FASTCOPY;
        time_stock_button_sobj->sprite.attr |= SP_TRANSPARENT;
        time_stock_button_sobj->sprite.red = 0x00;
        time_stock_button_sobj->sprite.green = 0x00;
        time_stock_button_sobj->sprite.blue = 0x00;
        time_stock_button_sobj->pos.x = 97.0F;
        time_stock_button_sobj->pos.y = 113.0F;

        time_stock_button_sobj = lbCommonMakeSObjForGObj(time_stock_button_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeMinTextSprite));
        time_stock_button_sobj->sprite.attr &= ~SP_FASTCOPY;
        time_stock_button_sobj->sprite.attr |= SP_TRANSPARENT;
        time_stock_button_sobj->sprite.red = 0x00;
        time_stock_button_sobj->sprite.green = 0x00;
        time_stock_button_sobj->sprite.blue = 0x00;
        time_stock_button_sobj->pos.x = 197.0F;
        time_stock_button_sobj->pos.y = 120.0F;
    }
    else
    {
        time_stock_button_sobj = lbCommonMakeSObjForGObj(time_stock_button_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeStockPeriodTextSprite));
        time_stock_button_sobj->sprite.attr &= ~SP_FASTCOPY;
        time_stock_button_sobj->sprite.attr |= SP_TRANSPARENT;
        time_stock_button_sobj->sprite.red = 0x00;
        time_stock_button_sobj->sprite.green = 0x00;
        time_stock_button_sobj->sprite.blue = 0x00;
        time_stock_button_sobj->pos.x = 106.0F;
        time_stock_button_sobj->pos.y = 114.0F;
    }

    mnVSModeMakeTimeStockArrows();
}

// 0x80132EBC
void mnVSModeMakeVSOptionsButton(void)
{
    GObj* button_gobj;
    SObj* button_sobj;

    sMNVSModeButtonGObjVSOptions = button_gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(button_gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);
    mnVSModeMakeButton(button_gobj, 51.0F, 148.0F, 17);
    mnVSModeUpdateButton(button_gobj, (sMNVSModeCursorIndex == nMNVSModeOptionOptions) ? nMNOptionTabStatusHighlight : nMNOptionTabStatusNot);

    button_sobj = lbCommonMakeSObjForGObj(button_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeVSOptionsTextSprite));
    button_sobj->sprite.attr &= ~SP_FASTCOPY;
    button_sobj->sprite.attr |= SP_TRANSPARENT;
    button_sobj->sprite.red = 0x00;
    button_sobj->sprite.green = 0x00;
    button_sobj->sprite.blue = 0x00;
#if defined(REGION_US)
    button_sobj->pos.x = 71.0F;
#else
    button_sobj->pos.x = 98.0F;
#endif
    button_sobj->pos.y = 151.0F;
}

// 0x80132FA4 - Unused?
void mnVSModeSetSubtitleSpriteColors(SObj* sobj)
{
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;
    sobj->envcolor.r = 0;
    sobj->envcolor.g = 0;
    sobj->envcolor.b = 0;
    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0xFF;
    sobj->sprite.blue = 0xFF;
}

// 0x80132FD8
void mnVSModeMakeSubtitle(void)
{
    GObj *gobj;
    SObj *sobj;

    sMNVSModeUnusedGObj = gobj = gcMakeGObjSPAfter(0, NULL, 5, GOBJ_PRIORITY_DEFAULT);

#if defined(REGION_JP)
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 3, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[0], llMNCommonFrameSprite));

    sobj->pos.x = 93.0F;
    sobj->pos.y = 189.0F;
    
    mnVSModeSetSubtitleSpriteColors(sobj);
    
    switch (sMNVSModeCursorIndex)
    {
        case nMNVSModeOptionStart:
            sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeVSStartTextJapSprite));
            
            sobj->pos.x = 121.0F;
            sobj->pos.y = 195.0F;
            
            mnVSModeSetSubtitleSpriteColors(sobj);
            return;

        case nMNVSModeOptionRule:
            switch (sMNVSModeRule)
            {
                case nMNVSModeRuleTime:
                    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeTimeBasedTextJapSprite));
                    
                    sobj->pos.x = 120.0F;
                    sobj->pos.y = 195.0F;
                    
                    mnVSModeSetSubtitleSpriteColors(sobj);

                    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeBattleTextJapSprite));
                    
                    sobj->pos.x = 166.0F;
                    sobj->pos.y = 195.0F;
                    
                    mnVSModeSetSubtitleSpriteColors(sobj);
                    break;
                case nMNVSModeRuleStock:
                    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeStockTextJapSprite));
                    
                    sobj->pos.x = 115.0F;
                    sobj->pos.y = 195.0F;
                    
                    mnVSModeSetSubtitleSpriteColors(sobj);

                    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeBattleTextJapSprite));
                    
                    sobj->pos.x = 172.0F;
                    sobj->pos.y = 195.0F;
                    
                    mnVSModeSetSubtitleSpriteColors(sobj);
                    break;
                case nMNVSModeRuleTimeTeam:
                    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeTimeBasedTextJapSprite));
                    
                    sobj->pos.x = 102.0F;
                    sobj->pos.y = 195.0F;
                    
                    mnVSModeSetSubtitleSpriteColors(sobj);

                    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeTeamTextJapSprite));
                    
                    sobj->pos.x = 148.0F;
                    sobj->pos.y = 195.0F;
                    
                    mnVSModeSetSubtitleSpriteColors(sobj);

                    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeBattleTextJapSprite));
                    
                    sobj->pos.x = 182.0F;
                    sobj->pos.y = 195.0F;
                    
                    mnVSModeSetSubtitleSpriteColors(sobj);
                    break;
                case nMNVSModeRuleStockTeam:
                    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeStockTextJapSprite));
                    
                    sobj->pos.x = 98.0F;
                    sobj->pos.y = 195.0F;
                    
                    mnVSModeSetSubtitleSpriteColors(sobj);

                    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeTeamTextJapSprite));
                    
                    sobj->pos.x = 154.0F;
                    sobj->pos.y = 195.0F;
                    
                    mnVSModeSetSubtitleSpriteColors(sobj);

                    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeBattleTextJapSprite));
                    
                    sobj->pos.x = 188.0F;
                    sobj->pos.y = 195.0F;
                    
                    mnVSModeSetSubtitleSpriteColors(sobj);
                    break;
            }

            return;

        case nMNVSModeOptionTimeStock:
            switch (sMNVSModeRule)
            {
                case nMNVSModeRuleStock:
                case nMNVSModeRuleStockTeam:
                    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeRemaningPlayersTextJapSprite));
                    
                    sobj->pos.x = 126.0F;
                    sobj->pos.y = 195.0F;
                    
                    mnVSModeSetSubtitleSpriteColors(sobj);
                    break;
                case nMNVSModeRuleTime:
                case nMNVSModeRuleTimeTeam:
                    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeTimeLimitTextJapSprite));
                    
                    sobj->pos.x = 132.0F;
                    sobj->pos.y = 195.0F;
                    
                    mnVSModeSetSubtitleSpriteColors(sobj);
                    break;
            }
            return;

        case nMNVSModeOptionOptions:
            sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeMatchOptionsTextJapSprite));
            
            sobj->pos.x = 111.0F;
            sobj->pos.y = 195.0F;
            
            mnVSModeSetSubtitleSpriteColors(sobj);
            return;
    }
#endif
}


/* mnvsmode.c:910-925 0x80133008. DIVERGES in one place: the nine RDP
 * commands -- a G_CC_PRIMITIVE fill rectangle at prim colour
 * (0xA0, 0x78, 0x14, 0xE6) under G_RM_AA_XLU_SURF, and the render mode
 * and cycle type put back after it -- are one fill quad at the pass's
 * next depth (src/dc/lbcommon.c lbCommonSpriteFillRect), the
 * paper-coloured rectangle the menu name's sprites then draw over. */
void mnVSModeRenderMenuName(GObj* menu_name_gobj)
{
    lbCommonSpriteFillRect(225, 143, 310, 230, 0xA0, 0x78, 0x14, 0xE6);

    lbCommonClearExternSpriteParams();
    lbCommonDrawSObjAttr(menu_name_gobj);
}

// 0x8013314C
void mnVSModeMakeMenuName()
{
    GObj* menu_name_gobj;
    SObj* menu_name_sobj;

    menu_name_gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(menu_name_gobj, mnVSModeRenderMenuName, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    menu_name_sobj = lbCommonMakeSObjForGObj(menu_name_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[0], llMNCommonSmashLogoSprite));
    menu_name_sobj->sprite.attr &= ~SP_FASTCOPY;
    menu_name_sobj->sprite.attr |= SP_TRANSPARENT;
    menu_name_sobj->sprite.red = 0x00;
    menu_name_sobj->sprite.green = 0x00;
    menu_name_sobj->sprite.blue = 0x00;
    menu_name_sobj->pos.x = 235.0F;
    menu_name_sobj->pos.y = 158.0F;

    menu_name_sobj = lbCommonMakeSObjForGObj(menu_name_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeVSTextSprite));
    menu_name_sobj->sprite.attr &= ~SP_FASTCOPY;
    menu_name_sobj->sprite.attr |= SP_TRANSPARENT;
    menu_name_sobj->sprite.red = 0x00;
    menu_name_sobj->sprite.green = 0x00;
    menu_name_sobj->sprite.blue = 0x00;
    menu_name_sobj->pos.x = 158.0F;
    menu_name_sobj->pos.y = 192.0F;

    menu_name_sobj = lbCommonMakeSObjForGObj(menu_name_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[0], llMNCommonGameModeTextSprite));
    menu_name_sobj->sprite.attr &= ~SP_FASTCOPY;
    menu_name_sobj->sprite.attr |= SP_TRANSPARENT;
    menu_name_sobj->sprite.red = 0x00;
    menu_name_sobj->sprite.green = 0x00;
    menu_name_sobj->sprite.blue = 0x00;
    menu_name_sobj->pos.x = 189.0F;
    menu_name_sobj->pos.y = 87.0F;
}

// 0x80133298
void mnVSModeMakeBackground(void)
{
    GObj* bg_gobj;
    SObj* bg_sobj;

    bg_gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(bg_gobj, lbCommonDrawSObjAttr, 0, GOBJ_PRIORITY_DEFAULT, ~0);

    bg_sobj = lbCommonMakeSObjForGObj(bg_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[0], llMNCommonSmashBrosCollageSprite));
    bg_sobj->pos.x = 10.0F;
    bg_sobj->pos.y = 10.0F;

    bg_sobj = lbCommonMakeSObjForGObj(bg_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[0], llMNCommonDecalPaperSprite));
    bg_sobj->sprite.attr &= ~SP_FASTCOPY;
    bg_sobj->sprite.attr |= SP_TRANSPARENT;
    bg_sobj->sprite.red = 0xA0;
    bg_sobj->sprite.green = 0x78;
    bg_sobj->sprite.blue = 0x14;
    bg_sobj->pos.x = 140.0F;
    bg_sobj->pos.y = 143.0F;

    bg_sobj = lbCommonMakeSObjForGObj(bg_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[0], llMNCommonDecalPaperSprite));
    bg_sobj->sprite.attr &= ~SP_FASTCOPY;
    bg_sobj->sprite.attr |= SP_TRANSPARENT;
    bg_sobj->sprite.red = 0xA0;
    bg_sobj->sprite.green = 0x78;
    bg_sobj->sprite.blue = 0x14;
    bg_sobj->pos.x = 225.0F;
    bg_sobj->pos.y = 56.0F;

    bg_sobj = lbCommonMakeSObjForGObj(bg_gobj, lbRelocGetFileData(Sprite*, sMNVSModeFiles[1], llMNVSModeConsoleIconDarkSprite));
    bg_sobj->sprite.attr &= ~SP_FASTCOPY;
    bg_sobj->sprite.attr |= SP_TRANSPARENT;
    bg_sobj->sprite.red = 0x99;
    bg_sobj->sprite.green = 0x99;
    bg_sobj->sprite.blue = 0x99;
    bg_sobj->pos.x = 10.0F;
    bg_sobj->pos.y = 10.0F;
}

// 0x8013342C
void mnVSModeMakeButtonValuegSYRdpViewport(void)
{
    GObj *camera_gobj = gcMakeCameraGObj
    (
        1,
        NULL,
        1,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSprite,
        0x14,
        COBJ_MASK_DLLINK(3),
        -1,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(camera_gobj);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

// 0x801334CC
void mnVSModeMakeButtonViewport(void)
{
    GObj *camera_gobj = gcMakeCameraGObj
    (
        1,
        NULL,
        1,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSprite,
        40,
        COBJ_MASK_DLLINK(2),
        -1,
        0,
        1,
        0,
        1,
        0
    );
    CObj *cobj = CObjGetStruct(camera_gobj);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

// 0x8013356C
void mnVSModeMakeMenuNameViewport(void)
{
    GObj *camera_gobj = gcMakeCameraGObj
    (
        1,
        NULL,
        1,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSprite,
        60,
        COBJ_MASK_DLLINK(1),
        -1,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(camera_gobj);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

// 0x8013360C
void mnVSModeMakeBackgroundViewport()
{
    GObj *camera_gobj = gcMakeCameraGObj(
        1,
        NULL,
        1,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSprite,
        80,
        COBJ_MASK_DLLINK(0),
        -1,
        0,
        1,
        0,
        1,
        0
    );
    CObj *cobj = CObjGetStruct(camera_gobj);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

// 0x801336AC
void mnVSModeFuncStartVars()
{
    if (gSCManagerSceneData.scene_prev == nSCKindVSOptions)
    {
        sMNVSModeCursorIndex = nMNVSModeOptionOptions;
    }
    else sMNVSModeCursorIndex = nMNVSModeOptionStart;

    sMNVSModeChangeWait = 0;

    switch (gSCManagerTransferBattleState.is_team_battle)
    {
        case FALSE:
            if (gSCManagerTransferBattleState.game_rules == SCBATTLE_GAMERULE_TIME)
            {
                sMNVSModeRule = nMNVSModeRuleTime;
            }
            else sMNVSModeRule = nMNVSModeRuleStock;

            break;
        case TRUE:
            if (gSCManagerTransferBattleState.game_rules == SCBATTLE_GAMERULE_TIME)
            {
                sMNVSModeRule = nMNVSModeRuleTimeTeam;
            }
            else sMNVSModeRule = nMNVSModeRuleStockTeam;

            break;
    }

    sMNVSModeTime = gSCManagerTransferBattleState.time_limit;
    sMNVSModeStock = gSCManagerTransferBattleState.stocks;
    sMNVSModeTimeStockArrowsGObj = 0;
    sMNVSModeRuleArrowsGObj = 0;
    sMNVSModeInputDirection = nMNVSModeInputDirectionNone;
    sMNVSModeTotalTimeTics = 0;
    sMNVSModeExitInterrupt = 0;
    sMNVSModeReturnTic = sMNVSModeTotalTimeTics + I_MIN_TO_TICS(5);
    sMNVSModeTimeStockArrowBlinkTimer = 0;
    sMNVSModeRuleArrowBlinkTimer = 0;
}

// 0x801337B8
void mnVSModeSaveSettings()
{
    gSCManagerTransferBattleState.time_limit = sMNVSModeTime;
    gSCManagerTransferBattleState.stocks = sMNVSModeStock;

    switch (sMNVSModeRule)
    {
        case nMNVSModeRuleStock:
            gSCManagerTransferBattleState.is_team_battle = FALSE;
            gSCManagerTransferBattleState.game_rules = SCBATTLE_GAMERULE_STOCK;
            break;
        case nMNVSModeRuleTime:
            gSCManagerTransferBattleState.is_team_battle = FALSE;
            gSCManagerTransferBattleState.game_rules = SCBATTLE_GAMERULE_TIME;
            break;
        case nMNVSModeRuleStockTeam:
            gSCManagerTransferBattleState.is_team_battle = TRUE;
            gSCManagerTransferBattleState.game_rules = SCBATTLE_GAMERULE_STOCK;
            break;
        case nMNVSModeRuleTimeTeam:
            gSCManagerTransferBattleState.is_team_battle = TRUE;
            gSCManagerTransferBattleState.game_rules = SCBATTLE_GAMERULE_TIME;
            break;
    }
}

// 0x80133850
s32 mnVSModeGetShade(s32 player)
{
    sb32 is_same_costume[GMCOMMON_PLAYERS_MAX];
    s32 i;

    if ((sMNVSModeRule == nMNVSModeRuleTime) || (sMNVSModeRule == nMNVSModeRuleStock))
    {
        return 0;
    }
    for (i = 0; i < ARRAY_COUNT(is_same_costume); i++)
    {
        is_same_costume[i] = FALSE;
    }
    for (i = 0; i < ARRAY_COUNT(is_same_costume); i++)
    {
        if
        (
            (player != i) &&
            (gSCManagerTransferBattleState.players[player].fkind == gSCManagerTransferBattleState.players[i].fkind) &&
            (gSCManagerTransferBattleState.players[player].team == gSCManagerTransferBattleState.players[i].team)
        )
        {
            is_same_costume[gSCManagerTransferBattleState.players[i].shade] = TRUE;
        }
    }
    for (i = 0; i < ARRAY_COUNT(is_same_costume); i++)
    {
        if (is_same_costume[i] == FALSE)
        {
            return i;
        }
    }
}

// 0x8013394C
s32 mnVSModeGetCostume(s32 fkind, s32 arg1)
{
    s32 i;
    s32 j;
    s32 unused[2];
    sb32 is_same_costume[GMCOMMON_PLAYERS_MAX];

    for (i = 0; i < ARRAY_COUNT(is_same_costume); i++)
    {
        is_same_costume[i] = FALSE;
    }
    for (i = 0; i < ARRAY_COUNT(is_same_costume); i++)
    {
        if (i != arg1)
        {
            if (fkind == gSCManagerTransferBattleState.players[i].fkind)
            {
                for (j = 0; j < ARRAY_COUNT(is_same_costume); j++)
                {
                    if (ftParamGetCostumeCommonID(fkind, j) == gSCManagerTransferBattleState.players[i].costume)
                    {
                        is_same_costume[j] = TRUE;
                    }
                }
            }
        }
    }
    for (i = 0; i < ARRAY_COUNT(is_same_costume); i++)
    {
        if (is_same_costume[i] == FALSE)
        {
            return i;
        }
    }
}

// 0x80133A8C
void mnVSModeSetCostumesAndShades(void)
{
    s32 i;

    switch (sMNVSModeRule)
    {
        case nMNVSModeRuleTime:
        case nMNVSModeRuleStock:
            for (i = 0; i < ARRAY_COUNT(gSCManagerTransferBattleState.players); i++)
            {
                if (gSCManagerTransferBattleState.players[i].fkind != nFTKindNull)
                {
                    gSCManagerTransferBattleState.players[i].costume = ftParamGetCostumeCommonID(gSCManagerTransferBattleState.players[i].fkind, mnVSModeGetCostume(gSCManagerTransferBattleState.players[i].fkind, i));
                    gSCManagerTransferBattleState.players[i].shade = mnVSModeGetShade(i);
                }
            }
            break;
        case nMNVSModeRuleTimeTeam:
        case nMNVSModeRuleStockTeam:
            for (i = 0; i < ARRAY_COUNT(gSCManagerTransferBattleState.players); i++)
            {
                if (gSCManagerTransferBattleState.players[i].fkind != nFTKindNull)
                {
                    gSCManagerTransferBattleState.players[i].costume = ftParamGetCostumeTeamID(gSCManagerTransferBattleState.players[i].fkind, gSCManagerTransferBattleState.players[i].team);
                    gSCManagerTransferBattleState.players[i].shade = mnVSModeGetShade(i);
                }
            }
            break;
    }
}

// 0x80133B8C
void mnVSModeMain(GObj *gobj)
{
    s32 unused;

    // 0x80134870
    GObj** buttons[/* */] = { &sMNVSModeButtonGObjVSStart, &sMNVSModeButtonGObjRule, &sMNVSModeButtonGObjTimeStock, &sMNVSModeButtonGObjVSOptions };
    
    s32 stick_range;
    s32 is_button;

    sMNVSModeTotalTimeTics++;

    if (sMNVSModeTotalTimeTics >= 10)
    {
        if (sMNVSModeTotalTimeTics == sMNVSModeReturnTic)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            mnVSModeSaveSettings();
            syTaskmanSetLoadScene();

            return;
        }
        if (scSubsysControllerCheckNoInputAll() == FALSE)
        {
            sMNVSModeReturnTic = sMNVSModeTotalTimeTics + I_MIN_TO_TICS(5);
        }
        if (sMNVSModeExitInterrupt != 0)
        {
            syTaskmanSetLoadScene();
        }
        if (sMNVSModeChangeWait != 0)
        {
            sMNVSModeChangeWait--;
        }
        if
        (
            (scSubsysControllerGetPlayerStickInRangeLR(-20, 20) != FALSE) &&
            (scSubsysControllerGetPlayerStickInRangeUD(-20, 20) != FALSE) &&
            (scSubsysControllerGetPlayerHoldButtons(U_JPAD | R_JPAD | R_TRIG | U_CBUTTONS | R_CBUTTONS) == FALSE) &&
            (scSubsysControllerGetPlayerHoldButtons(D_JPAD | L_JPAD | L_TRIG | D_CBUTTONS | L_CBUTTONS) == FALSE)
        )
        {
            sMNVSModeChangeWait = 0;
            sMNVSModeInputDirection = nMNVSModeInputDirectionNone;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | START_BUTTON) != FALSE)
        {
            switch (sMNVSModeCursorIndex)
            {
                case nMNVSModeOptionStart:
                    func_800269C0_275C0(nSYAudioFGMMenuSelect);
                    mnVSModeUpdateButton(sMNVSModeButtonGObjVSStart, nMNOptionTabStatusSelected);
                    mnVSModeSaveSettings();

                    sMNVSModeExitInterrupt = TRUE;

                    gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                    gSCManagerSceneData.scene_curr = nSCKindPlayersVS;

                    return;
                case nMNVSModeOptionOptions:
                    func_800269C0_275C0(nSYAudioFGMMenuSelect);
                    mnVSModeUpdateButton(sMNVSModeButtonGObjVSOptions, nMNOptionTabStatusSelected);
                    mnVSModeSaveSettings();

                    sMNVSModeExitInterrupt = TRUE;

                    gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                    gSCManagerSceneData.scene_curr = nSCKindVSOptions;

                    return;
            }
        }

        if (scSubsysControllerGetPlayerTapButtons(B_BUTTON) != FALSE)
        {
            mnVSModeSaveSettings();

            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindModeSelect;

            syTaskmanSetLoadScene();
        }
        if
        (
            mnCommonCheckGetOptionButtonInput(sMNVSModeChangeWait, is_button, U_JPAD | U_CBUTTONS) ||
            mnCommonCheckGetOptionStickInputUD(sMNVSModeChangeWait, stick_range, 20, 1)
        )
        {
            func_800269C0_275C0(nSYAudioFGMMenuScroll2);

            mnCommonSetOptionChangeWaitP(sMNVSModeChangeWait, is_button, stick_range, 7);

            if (sMNVSModeCursorIndex == nMNVSModeOptionRule)
            {
                sMNVSModeChangeWait += 8;
            }
            mnVSModeUpdateButton(*buttons[sMNVSModeCursorIndex], nMNOptionTabStatusNot);

            if (sMNVSModeCursorIndex == nMNVSModeOptionStart)
            {
                sMNVSModeCursorIndex = nMNVSModeOptionOptions;
            }
            else sMNVSModeCursorIndex--;

            mnVSModeUpdateButton(*buttons[sMNVSModeCursorIndex], nMNOptionTabStatusHighlight);
            gcEjectGObj(sMNVSModeUnusedGObj);
            mnVSModeMakeSubtitle();

            sMNVSModeInputDirection = nMNVSModeInputDirectionUp;

            if (sMNVSModeCursorIndex == nMNVSModeOptionRule)
            {
                sMNVSModeRuleArrowsGObj->flags = GOBJ_FLAG_NONE;
                sMNVSModeRuleArrowBlinkTimer = 30;
            }

            if (sMNVSModeCursorIndex == nMNVSModeOptionTimeStock)
            {
                sMNVSModeTimeStockArrowsGObj->flags = GOBJ_FLAG_NONE;
                sMNVSModeTimeStockArrowBlinkTimer = 30;
            }
        }

        if
        (
            mnCommonCheckGetOptionButtonInput(sMNVSModeChangeWait, is_button, D_JPAD | D_CBUTTONS) ||
            mnCommonCheckGetOptionStickInputUD(sMNVSModeChangeWait, stick_range, -20, 0)
        )
        {
            func_800269C0_275C0(nSYAudioFGMMenuScroll2);

            mnCommonSetOptionChangeWaitN(sMNVSModeChangeWait, is_button, stick_range, 7);

            if (sMNVSModeCursorIndex == nMNVSModeOptionTimeStock)
            {
                sMNVSModeChangeWait += 8;
            }

            mnVSModeUpdateButton(*buttons[sMNVSModeCursorIndex], nMNOptionTabStatusNot);

            if (sMNVSModeCursorIndex == nMNVSModeOptionOptions)
            {
                sMNVSModeCursorIndex = nMNVSModeOptionStart;
            }
            else sMNVSModeCursorIndex++;

            mnVSModeUpdateButton(*buttons[sMNVSModeCursorIndex], nMNOptionTabStatusHighlight);
            gcEjectGObj(sMNVSModeUnusedGObj);
            mnVSModeMakeSubtitle();

            sMNVSModeInputDirection = nMNVSModeInputDirectionDown;

            if (sMNVSModeCursorIndex == 1)
            {
                sMNVSModeRuleArrowsGObj->flags = GOBJ_FLAG_NONE;
                sMNVSModeRuleArrowBlinkTimer = 30;
            }

            if (sMNVSModeCursorIndex == 2)
            {
                sMNVSModeTimeStockArrowsGObj->flags = GOBJ_FLAG_NONE;
                sMNVSModeTimeStockArrowBlinkTimer = 30;
            }
        }

        switch (sMNVSModeCursorIndex)
        {
            case nMNVSModeOptionRule:
                if
                (
                    mnCommonCheckGetOptionButtonInput(sMNVSModeChangeWait, is_button, L_JPAD | L_TRIG | L_CBUTTONS) ||
                    mnCommonCheckGetOptionStickInputLR(sMNVSModeChangeWait, stick_range, -20, 0)
                )
                {
                    if (sMNVSModeRule > nMNVSModeRuleTime)
                    {
                        func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                        sMNVSModeRule--;

                        if (sMNVSModeRule == nMNVSModeRuleStock)
                        {
                            mnVSModeSetCostumesAndShades();
                        }

                        gcEjectGObj(sMNVSModeButtonGObjTimeStock);
                        mnVSModeMakeTimeStockButton();
                        gcEjectGObj(sMNVSModeRuleValueGObj);
                        mnVSModeMakeRuleValue();
                        gcEjectGObj(sMNVSModeTimeStockValueGObj);
                        mnVSModeMakeTimeStockValue();

                        mnCommonSetOptionChangeWaitN(sMNVSModeChangeWait, is_button, stick_range, 7);

                        gcEjectGObj(sMNVSModeUnusedGObj);
                        mnVSModeMakeSubtitle();

                        sMNVSModeInputDirection = nMNVSModeInputDirectionLeft;
                    }
                }

                if (
                    mnCommonCheckGetOptionButtonInput(sMNVSModeChangeWait, is_button, R_JPAD | R_TRIG | R_CBUTTONS) ||
                    mnCommonCheckGetOptionStickInputLR(sMNVSModeChangeWait, stick_range, 20, 1)
                )
                {
                    if (sMNVSModeRule < nMNVSModeRuleStockTeam)
                    {
                        func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                        sMNVSModeRule++;

                        if (sMNVSModeRule == nMNVSModeRuleTimeTeam)
                        {
                            mnVSModeSetCostumesAndShades();
                        }

                        gcEjectGObj(sMNVSModeButtonGObjTimeStock);
                        mnVSModeMakeTimeStockButton();
                        gcEjectGObj(sMNVSModeRuleValueGObj);
                        mnVSModeMakeRuleValue();
                        gcEjectGObj(sMNVSModeTimeStockValueGObj);
                        mnVSModeMakeTimeStockValue();

                        mnCommonSetOptionChangeWaitP(sMNVSModeChangeWait, is_button, stick_range, 7);

                        gcEjectGObj(sMNVSModeUnusedGObj);
                        mnVSModeMakeSubtitle();

                        sMNVSModeInputDirection = nMNVSModeInputDirectionRight;

                        return;
                    }
                }
                break;

            case nMNVSModeOptionTimeStock:
                if (
                    mnCommonCheckGetOptionButtonInput(sMNVSModeChangeWait, is_button, L_JPAD | L_TRIG | L_CBUTTONS) ||
                    mnCommonCheckGetOptionStickInputLR(sMNVSModeChangeWait, stick_range, -20, 0)
                )
                {
                    func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                    mnCommonSetOptionChangeWaitN(sMNVSModeChangeWait, is_button, stick_range, 14);

                    if (sMNVSModeInputDirection != nMNVSModeInputDirectionLeft)
                    {
                        sMNVSModeChangeWait *= 2;
                    }

                    if (mnVSModeIsTime() != FALSE)
                    {
                        if (sMNVSModeTime == 1)
                        {
                            sMNVSModeTime = SCBATTLE_TIMELIMIT_INFINITE;
                        }
                        else sMNVSModeTime--;

                        if (sMNVSModeTime == 1)
                        {
                            sMNVSModeChangeWait += 8;
                        }
                    }
                    else
                    {
                        if (sMNVSModeStock == 0)
                        {
                            sMNVSModeStock = 98;
                        }
                        else sMNVSModeStock--;

                        if (sMNVSModeStock == 0)
                        {
                            sMNVSModeChangeWait += 8;
                        }
                    }
                    gcEjectGObj(sMNVSModeTimeStockValueGObj);
                    mnVSModeMakeTimeStockValue();

                    sMNVSModeInputDirection = nMNVSModeInputDirectionLeft;
                }
                if
                (
                    mnCommonCheckGetOptionButtonInput(sMNVSModeChangeWait, is_button, R_JPAD | R_TRIG | R_CBUTTONS) ||
                    mnCommonCheckGetOptionStickInputLR(sMNVSModeChangeWait, stick_range, 20, 1)
                )
                {
                    func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                    mnCommonSetOptionChangeWaitP(sMNVSModeChangeWait, is_button, stick_range, 14);

                    if (sMNVSModeInputDirection != nMNVSModeInputDirectionRight)
                    {
                        sMNVSModeChangeWait *= 2;
                    }

                    if (mnVSModeIsTime() != FALSE)
                    {
                        if (sMNVSModeTime == 100)
                        {
                            sMNVSModeTime = 1;
                        }
                        else sMNVSModeTime += 1;

                        if (sMNVSModeTime == SCBATTLE_TIMELIMIT_INFINITE)
                        {
                            sMNVSModeChangeWait += 8;
                        }
                    }
                    else
                    {
                        if (sMNVSModeStock == 98)
                        {
                            sMNVSModeStock = 0;
                        }
                        else sMNVSModeStock++;

                        if (sMNVSModeStock == 98)
                        {
                            sMNVSModeChangeWait += 8;
                        }
                    }
                    gcEjectGObj(sMNVSModeTimeStockValueGObj);
                    mnVSModeMakeTimeStockValue();

                    sMNVSModeInputDirection = nMNVSModeInputDirectionRight;
                }
                break;
        }
    }
}


/* mnvsmode.c:1610-1620 and 1625: lbRelocInitSetup and
 * lbRelocLoadFilesListed over dMNVSModeFileIDs; here, the banks. A bank
 * that is not there leaves its file NULL and sprite_bank_get says so
 * per sprite. */
void mnVSModeLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMNVSModeFileIDs)] =
    {
        MNVSMODE_BANK_COMMON,
        MNVSMODE_BANK_VSMODE
    };
    s32 i;

    for (i = 0; i < ARRAY_COUNT(dMNVSModeFileIDs); i++)
    {
        if (sprite_bank_load(&sMNVSModeBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mnVSMode: no bank for file %d (%s)\n",
                          (int)dMNVSModeFileIDs[i], paths[i]);
            sMNVSModeFiles[i] = NULL;
            continue;
        }
        sMNVSModeFiles[i] = &sMNVSModeBanks[i];
    }
}

/* mnvsmode.c:1607-1649 0x801345C4, in the game's order. DIVERGES: the
 * clear camera (gcMakeDefaultCameraGObj on link 0, DL priority 100, a
 * black fill with the z-buffer) is the frame clear, which the PVR does
 * itself -- the same cut as the mode select's. The IMEM check is the
 * game's: gSYMainImemOK is what sys/main.c read off the RSP at boot,
 * and src/dc/sysshim.c says what the port answers. */
void mnVSModeFuncStart(void)
{
    mnVSModeLoadFiles();

    if (!(gSCManagerBackupData.error_flags & LBBACKUP_ERROR_RANDOMKNOCKBACK) && (gSCManagerBackupData.boot > 21) && (gSYMainImemOK == FALSE))
    {
        gSCManagerBackupData.error_flags |= LBBACKUP_ERROR_RANDOMKNOCKBACK;
    }
    gcMakeGObjSPAfter(0, mnVSModeMain, 0, GOBJ_PRIORITY_DEFAULT);

    mnVSModeFuncStartVars();
    mnVSModeMakeBackgroundViewport();
    mnVSModeMakeMenuNameViewport();
    mnVSModeMakeButtonViewport();
    mnVSModeMakeButtonValuegSYRdpViewport();
    mnVSModeMakeBackground();
    mnVSModeMakeMenuName();
    mnVSModeMakeVSStartButton();
    mnVSModeMakeRuleButton();
    mnVSModeMakeRuleValue();
    mnVSModeMakeTimeStockButton();
    mnVSModeMakeTimeStockValue();
    mnVSModeMakeVSOptionsButton();
    mnVSModeMakeSubtitle();

    if (gSCManagerSceneData.scene_prev == nSCKindPlayersVS)
    {
        syAudioPlayBGM(0, nSYAudioBGMModeSelect);
    }
}

/* mnvsmode.c:1652 dMNVSModeVideoSetup is the N64's video mode: see
 * mnVSModeStartScene. */

/* mnvsmode.c:1655-1697 (0x8013489C). The pool counts are the game's, all
 * zero: a menu scene takes its objects straight from the scene heap, and
 * the two arrow threads their stacks the same way (sys/objman.c
 * gcGetGObjThread). DIVERGES as the mode select's: the arena is the
 * port's region, the draw is the scene manager's, and mnVSModeFuncLights
 * is dropped. */
SYTaskmanSetup dMNVSModeTaskmanSetup =
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

    mnVSModeFuncStart                   // Task start function
};

/* The bzero arm of syDmaLoadOverlay for overlay 19, this
 * file: sc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnVSModeOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNVSModeButtonGObjVSStart);
    OVERLAY_CLEAR(sMNVSModeButtonGObjRule);
    OVERLAY_CLEAR(sMNVSModeButtonGObjTimeStock);
    OVERLAY_CLEAR(sMNVSModeButtonGObjVSOptions);
    OVERLAY_CLEAR(sMNVSModePad0x80134940);
    OVERLAY_CLEAR(sMNVSModeCursorIndex);
    OVERLAY_CLEAR(sMNVSModeRule);
    OVERLAY_CLEAR(sMNVSModeTime);
    OVERLAY_CLEAR(sMNVSModeStock);
    OVERLAY_CLEAR(sMNVSModeRuleValueGObj);
    OVERLAY_CLEAR(sMNVSModeTimeStockValueGObj);
    OVERLAY_CLEAR(sMNVSModeUnusedGObj);
    OVERLAY_CLEAR(sMNVSModeRuleArrowsGObj);
    OVERLAY_CLEAR(sMNVSModeTimeStockArrowsGObj);
    OVERLAY_CLEAR(sMNVSModeTimeStockArrowBlinkTimer);
    OVERLAY_CLEAR(sMNVSModeRuleArrowBlinkTimer);
    OVERLAY_CLEAR(sMNVSModeExitInterrupt);
    OVERLAY_CLEAR(sMNVSModeInputDirection);
    OVERLAY_CLEAR(sMNVSModeChangeWait);
    OVERLAY_CLEAR(sMNVSModeTotalTimeTics);
    OVERLAY_CLEAR(sMNVSModeReturnTic);
    OVERLAY_CLEAR(sMNVSModeFiles);
    OVERLAY_CLEAR(sMNVSModeBanks);
}

/* mnvsmode.c:1700-1707 0x80134758. DIVERGES as the mode select's:
 * syVideoInit and the zbuffer are the N64's video mode, set once at boot
 * here, and the arena_size line is the link map. What is left is the
 * last line. */
void mnVSModeStartScene(void)
{
    syTaskmanStartTask(&dMNVSModeTaskmanSetup);
}
