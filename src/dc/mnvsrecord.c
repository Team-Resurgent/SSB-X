/* mnvsrecord.c -- see mnvsrecord.h. Every function is mn/mndata/mnvsrecord.c's
 * by name and body, REGION_US arms; the line numbers are the decomp's. */
#include "mnvsrecord.h"
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

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/sysshim.c defines it over the FGM engine (src/dc/mndata.c does
 * the same). */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mndata.c does the same). Each `&llXxxSprite` below is
 * written `llXxxSprite`, the number, with the label's name kept as the
 * macro's. The values are relocData files 0x1f (MNVSRecordMain, this
 * screen's own) and 0x20 (MNDataCommon, the DATA menu's shared arrows
 * and header) as tools/export/ssb_spriteexport.py --list reads them off the
 * ROM; files 0x13 (MNPlayersPortraits) and 0x21 (MNCommonFonts) reuse
 * the banks the character select and the stage select already put on
 * the disc (mnportraits.spr, mnfonts.spr -- src/game/ssb64/Makefile). */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

#define llMNVSRecordMainQuestionSprite           0x00070
#define llMNVSRecordMainLabelTotalSprite         0x00258
#define llMNVSRecordMainDigit0Sprite             0x002f0
#define llMNVSRecordMainDigit1Sprite             0x00390
#define llMNVSRecordMainDigit2Sprite             0x00430
#define llMNVSRecordMainDigit3Sprite             0x004d0
#define llMNVSRecordMainDigit4Sprite             0x00570
#define llMNVSRecordMainDigit5Sprite             0x00610
#define llMNVSRecordMainDigit6Sprite             0x006b0
#define llMNVSRecordMainDigit7Sprite             0x00750
#define llMNVSRecordMainDigit8Sprite             0x007f0
#define llMNVSRecordMainDigit9Sprite             0x00890
#define llMNVSRecordMainSymbolPointSprite        0x00910
#define llMNVSRecordMainLabelWinPercentSprite    0x00a08
#define llMNVSRecordMainLabelKOsSprite           0x00af8
#define llMNVSRecordMainLabelTKOSprite           0x00be8
#define llMNVSRecordMainLabelSDPercentSprite     0x00cd8
#define llMNVSRecordMainLabelTimeSprite          0x00e10
#define llMNVSRecordMainLabelUsePercentSprite    0x00f08
#define llMNVSRecordMainLabelAvgSprite           0x01008
#define llMNVSRecordMainLabelKOdSprite           0x01140
#define llMNVSRecordMainSymbolSlashSprite        0x011d0
/* mnVSRecordSubtitleProcUpdate's own offsets[1] and [2]: unnamed by the
 * sprite exporter (the Ranking and Individual subtitle bitmaps), so the
 * decomp's raw hex is kept, the way it is written there. */
#define llMNVSRecordMainBattleScoreSprite        0x015d0
#define llMNVSRecordMainDownArrowsSprite         0x01668
#define llMNVSRecordMainSideArrowsSprite         0x017a8
#define llMNVSRecordMainMarioIconBWSprite        0x01918
#define llMNVSRecordMainFoxIconBWSprite          0x01a98
#define llMNVSRecordMainDonkeyIconBWSprite       0x01ca8
#define llMNVSRecordMainSamusIconBWSprite        0x01e88
#define llMNVSRecordMainLuigiIconBWSprite        0x02008
#define llMNVSRecordMainYoshiIconBWSprite        0x02178
#define llMNVSRecordMainLinkIconBWSprite         0x02370
#define llMNVSRecordMainCaptainIconBWSprite      0x02540
#define llMNVSRecordMainNessIconBWSprite         0x02698
#define llMNVSRecordMainPurinIconBWSprite        0x027c8
#define llMNVSRecordMainKirbyIconBWSprite        0x02930
#define llMNVSRecordMainPikachuIconBWSprite      0x02b30
#define llMNVSRecordMainMarioIconColorSprite     0x02d18
#define llMNVSRecordMainFoxIconColorSprite       0x02ef8
#define llMNVSRecordMainDonkeyIconColorSprite    0x03198
#define llMNVSRecordMainSamusIconColorSprite     0x03438
#define llMNVSRecordMainLuigiIconColorSprite     0x03618
#define llMNVSRecordMainYoshiIconColorSprite     0x037f8
#define llMNVSRecordMainLinkIconColorSprite      0x03a38
#define llMNVSRecordMainCaptainIconColorSprite   0x03cd8
#define llMNVSRecordMainNessIconColorSprite      0x03eb8
#define llMNVSRecordMainPurinIconColorSprite     0x04098
#define llMNVSRecordMainKirbyIconColorSprite     0x04308
#define llMNVSRecordMainPikachuIconColorSprite   0x045a8
#define llMNVSRecordMainPortraitWallpaperSprite  0x04d30
#define llMNVSRecordMainLabelSprite              0x05428
#define llMNVSRecordMainSymbolColonSprite        0x054c0

#define llMNDataCommonDataHeaderSprite           0x00b40
#define llMNDataCommonArrowLSprite               0x00be0
#define llMNDataCommonArrowRSprite               0x00c80

#define llMNPlayersPortraitsMarioSprite          0x04728
#define llMNPlayersPortraitsLuigiSprite          0x06978
#define llMNPlayersPortraitsDonkeySprite         0x08bc8
#define llMNPlayersPortraitsSamusSprite          0x0ae18
#define llMNPlayersPortraitsFoxSprite            0x0d068
#define llMNPlayersPortraitsKirbySprite          0x0f2b8
#define llMNPlayersPortraitsLinkSprite           0x11508
#define llMNPlayersPortraitsYoshiSprite          0x13758
#define llMNPlayersPortraitsPikachuSprite        0x159a8
#define llMNPlayersPortraitsNessSprite           0x17bf8
#define llMNPlayersPortraitsCaptainSprite        0x19e48
#define llMNPlayersPortraitsPurinSprite          0x1c098

#define llMNCommonFontsLetterASprite             0x00040
#define llMNCommonFontsLetterBSprite             0x000d0
#define llMNCommonFontsLetterCSprite             0x00160
#define llMNCommonFontsLetterDSprite             0x001f0
#define llMNCommonFontsLetterESprite             0x00280
#define llMNCommonFontsLetterFSprite             0x00310
#define llMNCommonFontsLetterGSprite             0x003a0
#define llMNCommonFontsLetterHSprite             0x00430
#define llMNCommonFontsLetterISprite             0x004c0
#define llMNCommonFontsLetterJSprite             0x00550
#define llMNCommonFontsLetterKSprite             0x005e0
#define llMNCommonFontsLetterLSprite             0x00670
#define llMNCommonFontsLetterMSprite             0x00700
#define llMNCommonFontsLetterNSprite             0x00790
#define llMNCommonFontsLetterOSprite             0x00820
#define llMNCommonFontsLetterPSprite             0x008b0
#define llMNCommonFontsLetterQSprite             0x00940
#define llMNCommonFontsLetterRSprite             0x009d0
#define llMNCommonFontsLetterSSprite             0x00a60
#define llMNCommonFontsLetterTSprite             0x00af0
#define llMNCommonFontsLetterUSprite             0x00b80
#define llMNCommonFontsLetterVSprite             0x00c10
#define llMNCommonFontsLetterWSprite             0x00ca0
#define llMNCommonFontsLetterXSprite             0x00d30
#define llMNCommonFontsLetterYSprite             0x00dc0
#define llMNCommonFontsLetterZSprite             0x00e50
#define llMNCommonFontsSymbolApostropheSprite    0x00ed0
#define llMNCommonFontsSymbolPercentSprite       0x00f60
#define llMNCommonFontsSymbolPeriodSprite        0x00fd0

/* The four files' banks. The last two are whole-file exports the
 * character select and the stage select already put on the disc; this
 * screen just opens them again (tools/check/disc_order_check.py's comment
 * says why that is cheaper than a second copy). */
#define MNVSRECORD_BANK_MAIN       "mnvsrecord.spr"
#define MNVSRECORD_BANK_DATACOMMON "mndatacommon.spr"
#define MNVSRECORD_BANK_PORTRAITS  "mnportraits.spr"
#define MNVSRECORD_BANK_FONTS      "mnfonts.spr"

// // // // // // // // // // // //
//                               //
//       MACROS                  //
//                               //
// // // // // // // // // // // //

#define mnVSRecordCheckGetOptionButtonInput(is_button, mask) \
mnCommonCheckGetOptionButtonInput(sMNVSRecordChangeWait, is_button, mask)

#define mnVSRecordCheckGetOptionStickInputUD(stick_range, min, b) \
mnCommonCheckGetOptionStickInputUD(sMNVSRecordChangeWait, stick_range, min, b)

#define mnVSRecordCheckGetOptionStickInputLR(stick_range, min, b) \
mnCommonCheckGetOptionStickInputLR(sMNVSRecordChangeWait, stick_range, min, b)

#define mnVSRecordSetOptionChangeWaitP(is_button, stick_range, div) \
mnCommonSetOptionChangeWaitP(sMNVSRecordChangeWait, is_button, stick_range, div)

#define mnVSRecordSetOptionChangeWaitN(is_button, stick_range, div) \
mnCommonSetOptionChangeWaitN(sMNVSRecordChangeWait, is_button, stick_range, div)

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mnvsrecord.c:42 (0x80136630), verbatim: the Ranking table's seven
 * column widths, Win%/KOs/TKO/SD%/Time/Use%/Avg. */
s32 dMNVSRecordRankingColumnWidths[/* */] = { 33, 33, 33, 33, 46, 35, 34 };

/* mnvsrecord.c:39-46 (0x8013664C): { &llMNVSRecordMainFileID,
 * &llMNDataCommonFileID, &llMNPlayersPortraitsFileID,
 * &llMNCommonFontsFileID }, the four link labels' values written out
 * (src/dc/decomp/reloc_data.us.h: 0x1f, 0x20, 0x13, 0x21). Used only for
 * ARRAY_COUNT and the load-failure message; mnVSRecordLoadFiles below
 * is what actually reaches the disc. */
u32 dMNVSRecordFileIDs[/* */] = { 0x1f, 0x20, 0x13, 0x21 };

/* mnvsrecord.c:48-57 dMNVSRecordLights1 and dMNVSRecordDisplayList, the
 * lighting the pre-render function would set for the 3D this scene
 * never draws. Dropped with mnVSRecordFuncLights, as every other menu
 * scene's is (src/dc/mndata.c). */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* mnvsrecord.c:66 sMNVSRecordPad0x80136C10[2], two words of the overlay
 * nothing names. Dropped: the port has no link map to keep a hole in
 * (src/dc/mndata.c drops its own the same way). */

/* mnvsrecord.c:69 */
s32 sMNVSRecordStatsKind;

/* mnvsrecord.c:72 */
sb32 sMNVSRecordIsChangeSubtitle;

/* mnvsrecord.c:75-78 */
GObj *sMNVSRecordTableHeadersGObj;
GObj *sMNVSRecordTableValuesGObj;

/* mnvsrecord.c:81-87, the three tables' sort orders (fighter kind by
 * screen column/row). */
s32 sMNVSRecordBattleScoreFighterKinds[(nFTKindPlayableEnd - nFTKindPlayableStart) + 1];
s32 sMNVSRecordRankingFighterKindOrder[(nFTKindPlayableEnd - nFTKindPlayableStart) + 1];
s32 sMNVSRecordIndivFighterKinds[(nFTKindPlayableEnd - nFTKindPlayableStart) + 1];

/* mnvsrecord.c:90 */
s32 sMNVSRecordCurrentIndex;

/* mnvsrecord.c:93 */
u16 sMNVSRecordFighterMask;

/* mnvsrecord.c:96 */
s32 sMNVSRecordFirstColumn;

/* mnvsrecord.c:99 */
s32 sMNVSRecordChangeWait;

/* mnvsrecord.c:102 sMNVSRecordStatusBuffer[24], the reloc loader's
 * per-file status records. Dropped with lbRelocInitSetup
 * (mnVSRecordLoadFiles, as src/dc/mndata.c drops its own). */

/* mnvsrecord.c:105 */
void *sMNVSRecordFiles[ARRAY_COUNT(dMNVSRecordFileIDs)];

/* the banks behind sMNVSRecordFiles */
static SpriteBank sMNVSRecordBanks[ARRAY_COUNT(dMNVSRecordFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mnvsrecord.c:113-117 mnVSRecordFuncLights: the scene's pre-render
 * function, two GBI commands setting a single light for 3D this scene
 * has none of. Dropped, as every other menu scene's is
 * (src/dc/mndata.c). */

/* mnvsrecord.c:119-138 0x80131B24, verbatim. */
s32 mnVSRecordGetFighterKindByIndex(s32 index)
{
    s32 fkinds[/* */] =
    {
        nFTKindMario,
        nFTKindDonkey,
        nFTKindLink,
        nFTKindSamus,
        nFTKindYoshi,
        nFTKindKirby,
        nFTKindFox,
        nFTKindPikachu,
        nFTKindLuigi,
        nFTKindCaptain,
        nFTKindNess,
        nFTKindPurin
    };
    return fkinds[index];
}

/* mnvsrecord.c:140-154 0x80131B74, verbatim. */
s32 mnVSRecordGetKOs(s32 fkind)
{
    s32 i;
    s32 total_kos = 0;

    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        if (mnVSRecordCheckHaveFighterKind(i) != FALSE)
        {
            total_kos += gSCManagerBackupData.vs_records[fkind].ko_count[i];
        }
    }
    return total_kos;
}

/* mnvsrecord.c:156-174 0x80131C0C, verbatim. */
s32 mnVSRecordGetTKO(s32 fkind)
{
    s32 i;
    s32 total_tkos = 0;

    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        if (mnVSRecordCheckHaveFighterKind(i))
        {
            total_tkos += gSCManagerBackupData.vs_records[i].ko_count[fkind];
        }
    }
    if ((gSCManagerBackupData.vs_records[fkind].selfdestructs + total_tkos) > 9999)
    {
        return 9999;
    }
    else return gSCManagerBackupData.vs_records[fkind].selfdestructs + total_tkos;
}

/* mnvsrecord.c:176-190 0x80131CD4, verbatim. */
s32 mnVSRecordGetTotalTKO(void)
{
    s32 i;
    s32 total_tkos = 0;

    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        if (mnVSRecordCheckHaveFighterKind(i))
        {
            total_tkos += mnVSRecordGetTKO(i);
        }
    }
    return total_tkos;
}

/* mnvsrecord.c:192-199 0x80131D38, verbatim. */
f32 mnVSRecordGetWinPercent(s32 fkind)
{
    f32 kos = mnVSRecordGetKOs(fkind);
    f32 tko = mnVSRecordGetTotalTKO();

    return ((tko != 0.0F) ? kos / tko : 0.0F) * 100.0F;
}

/* mnvsrecord.c:201-219 0x80131DA0, verbatim. */
s32 mnVSRecordGetPowerOf(s32 base, s32 exp)
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

/* mnvsrecord.c:221-234 0x80131E40, verbatim. */
void mnVSRecordSetSpriteColors(SObj *sobj, u32 *colors)
{
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->envcolor.r = colors[0];
    sobj->envcolor.g = colors[1];
    sobj->envcolor.b = colors[2];

    sobj->sprite.red = colors[3];
    sobj->sprite.green = colors[4];
    sobj->sprite.blue = colors[5];
}

/* mnvsrecord.c:236-252 0x80131E88, verbatim. */
s32 mnVSRecordGetDigitCount(s32 number, s32 digit_count_max)
{
    s32 digit_count_curr = digit_count_max;

    while (digit_count_curr > 0)
    {
        s32 digit = (mnVSRecordGetPowerOf(10, digit_count_curr - 1) != 0) ? number / mnVSRecordGetPowerOf(10, digit_count_curr - 1) : 0;

        if (digit != 0)
        {
            return digit_count_curr;
        }
        else digit_count_curr--;
    }
    return 0;
}

/* mnvsrecord.c:254-338 0x80131F34, verbatim. */
void mnVSRecordMakeDigits(GObj *gobj, s32 number, f32 x, f32 y, u32 *colors, sb32 is_show_tenths, sb32 is_wide, s32 digit_count_max, sb32 is_fixed_digit_count)
{
    intptr_t offsets[/* */] =
    {
        llMNVSRecordMainDigit0Sprite, llMNVSRecordMainDigit1Sprite,
        llMNVSRecordMainDigit2Sprite, llMNVSRecordMainDigit3Sprite,
        llMNVSRecordMainDigit4Sprite, llMNVSRecordMainDigit5Sprite,
        llMNVSRecordMainDigit6Sprite, llMNVSRecordMainDigit7Sprite,
        llMNVSRecordMainDigit8Sprite, llMNVSRecordMainDigit9Sprite
    };
    SObj *sobj;
    f32 calc_x = x;
    s32 i;
    s32 digit;

    if (number < 0)
    {
        number = 0;
    }
    if ((is_show_tenths != FALSE) && (number == 1000))
    {
        is_show_tenths = FALSE;
        number = number / 10;
    }

    if (is_show_tenths != FALSE)
    {
        s32 decimal = number % 10;
        number = number / 10;

        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], offsets[decimal]));
        mnVSRecordSetSpriteColors(sobj, colors);

        if (is_wide != FALSE)
        {
            calc_x -= 5.0F;
        }
        else calc_x -= 4.0F;

        sobj->pos.x = calc_x;
        sobj->pos.y = y;

        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], llMNVSRecordMainSymbolPointSprite));
        mnVSRecordSetSpriteColors(sobj, colors);

        if (is_wide != FALSE)
        {
            calc_x -= 3.0F;
        }
        else calc_x -= 2.0F;

        sobj->pos.x = calc_x;
        sobj->pos.y = y + 4.0F;
    }

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], offsets[number % 10]));
    mnVSRecordSetSpriteColors(sobj, colors);

    if (is_wide != FALSE)
    {
        calc_x -= 5.0F;
    }
    else calc_x -= 4.0F;

    sobj->pos.x = calc_x;
    sobj->pos.y = y;

    for (i = 1; i < ((is_fixed_digit_count != FALSE) ? digit_count_max : mnVSRecordGetDigitCount(number, digit_count_max)); i++)
    {
        digit = (mnVSRecordGetPowerOf(10, i) != 0) ? number / mnVSRecordGetPowerOf(10, i) : 0;

        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], offsets[digit % 10]));
        mnVSRecordSetSpriteColors(sobj, colors);

        if (is_wide != FALSE)
        {
            calc_x -= 5.0F;
        }
        else calc_x -= 4.0F;

        sobj->pos.x = calc_x;
        sobj->pos.y = y;
    }
}

/* mnvsrecord.c:340-364 0x8013232C, verbatim. */
s32 mnVSRecordGetCharacterID(const char c)
{
    switch (c)
    {
    case '\'':
        return 0x1A;

    case '%':
        return 0x1B;

    case '.':
        return 0x1C;

    case ' ':
        return 0x1D;

    default:
        if ((c < 'A') || (c > 'Z'))
        {
            return 0x1D;
        }
        else return c - 'A';
    }
}

/* mnvsrecord.c:366-431 0x801323A4, verbatim. */
f32 mnVSRecordGetCharacterSpacing(const char *str, s32 c)
{
    switch (str[c])
    {
    case 'A':
        switch (str[c + 1])
        {
        case 'F':
        case 'P':
        case 'T':
        case 'V':
        case 'Y':
            return 0.0F;

        default:
            return 1.0F;
        }
        break;

    case 'F':
    case 'P':
    case 'V':
    case 'Y':
        switch(str[c + 1])
        {
        case 'A':
        case 'T':
            return 0.0F;

        default:
            return 1.0F;
        }
        break;

    case 'Q':
    case 'T':
        switch(str[c + 1])
        {
        case '\'':
        case '.':
            return 1.0F;

        default:
            return 0.0F;
        }
        break;

    case '\'':
        return 1.0F;

    case '.':
        return 1.0F;

    default:
        switch(str[c + 1])
        {
        case 'T':
            return 0.0F;

        default:
            return 1.0F;
        }
        break;
    }
}

/* mnvsrecord.c:433-499 0x801324C8, verbatim. */
void mnVSRecordMakeString(GObj *gobj, const char *str, f32 x, f32 y, u32 *color)
{
    intptr_t offsets[/* */] =
    {
        llMNCommonFontsLetterASprite, llMNCommonFontsLetterBSprite,
        llMNCommonFontsLetterCSprite, llMNCommonFontsLetterDSprite,
        llMNCommonFontsLetterESprite, llMNCommonFontsLetterFSprite,
        llMNCommonFontsLetterGSprite, llMNCommonFontsLetterHSprite,
        llMNCommonFontsLetterISprite, llMNCommonFontsLetterJSprite,
        llMNCommonFontsLetterKSprite, llMNCommonFontsLetterLSprite,
        llMNCommonFontsLetterMSprite, llMNCommonFontsLetterNSprite,
        llMNCommonFontsLetterOSprite, llMNCommonFontsLetterPSprite,
        llMNCommonFontsLetterQSprite, llMNCommonFontsLetterRSprite,
        llMNCommonFontsLetterSSprite, llMNCommonFontsLetterTSprite,
        llMNCommonFontsLetterUSprite, llMNCommonFontsLetterVSprite,
        llMNCommonFontsLetterWSprite, llMNCommonFontsLetterXSprite,
        llMNCommonFontsLetterYSprite, llMNCommonFontsLetterZSprite,

        llMNCommonFontsSymbolApostropheSprite,
        llMNCommonFontsSymbolPercentSprite,
        llMNCommonFontsSymbolPeriodSprite
    };
    SObj *sobj;
    f32 start_x = x;
    s32 i;

    for (i = 0; str[i] != '\0'; i++)
    {
        if (((((str[i] >= '0') && (str[i] <= '9')) ? TRUE : FALSE)) || (str[i] == ' '))
        {
            if (str[i] == ' ')
            {
                start_x += 4.0F;
            }
            else start_x += str[i] - '0';
        }
        else
        {
            sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[3], offsets[mnVSRecordGetCharacterID(str[i])]));
            sobj->pos.x = start_x;

            start_x += sobj->sprite.width + mnVSRecordGetCharacterSpacing(str, i);

            switch (str[i])
            {
            case '\'':
                sobj->pos.y = y - 1.0F;
                break;

            case '.':
                sobj->pos.y = y + 4.0F;
                break;

            default:
                sobj->pos.y = y;
                break;
            }
            sobj->sprite.attr &= ~SP_FASTCOPY;
            sobj->sprite.attr |= SP_TRANSPARENT;

            sobj->sprite.red = color[0];
            sobj->sprite.green = color[1];
            sobj->sprite.blue = color[2];
        }
    }
}

/* mnvsrecord.c:501-521 0x801326EC, verbatim. */
sb32 mnVSRecordCheckHaveFighterKind(s32 fkind)
{
    switch (fkind)
    {
    case nFTKindNess:
        return (sMNVSRecordFighterMask & LBBACKUP_MASK_FIGHTER(nFTKindNess)) ? TRUE : FALSE;

    case nFTKindPurin:
        return (sMNVSRecordFighterMask & LBBACKUP_MASK_FIGHTER(nFTKindPurin)) ? TRUE : FALSE;

    case nFTKindCaptain:
        return (sMNVSRecordFighterMask & LBBACKUP_MASK_FIGHTER(nFTKindCaptain)) ? TRUE : FALSE;

    case nFTKindLuigi:
        return (sMNVSRecordFighterMask & LBBACKUP_MASK_FIGHTER(nFTKindLuigi)) ? TRUE : FALSE;

    default:
        return TRUE;
    }
}

/* mnvsrecord.c:523-559 0x801327B8, verbatim. */
void mnVSRecordMakeLabels(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[1], llMNDataCommonDataHeaderSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x5F;
    sobj->sprite.green = 0x58;
    sobj->sprite.blue = 0x46;

    sobj->pos.x = 24.0F;
    sobj->pos.y = 17.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], llMNVSRecordMainLabelSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->envcolor.r = 0x00;
    sobj->envcolor.g = 0x00;
    sobj->envcolor.b = 0x00;

    sobj->sprite.red = 0xF2;
    sobj->sprite.green = 0xC7;
    sobj->sprite.blue = 0x0D;

    sobj->pos.x = 99.0F;
    sobj->pos.y = 23.0F;
}

/* mnvsrecord.c:561-589 0x801328D4, verbatim. */
void mnVSRecordSubtitleProcUpdate(GObj *gobj)
{
    SObj *sobj;
    intptr_t offsets[/* */] =
    {
        llMNVSRecordMainBattleScoreSprite,
        0x1458,
        0x1318,
        0x0
    };

    if (sMNVSRecordIsChangeSubtitle != FALSE)
    {
        gcRemoveSObjAll(gobj);

        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], offsets[sMNVSRecordStatsKind]));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->sprite.red = 0x00;
        sobj->sprite.green = 0x00;
        sobj->sprite.blue = 0x00;

        sobj->pos.x = 222.0F;
        sobj->pos.y = 28.0F;
    }
}

/* mnvsrecord.c:591-612 0x80132994, verbatim. */
void mnVSRecordMakeSubtitle(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mnVSRecordSubtitleProcUpdate, nGCProcessKindFunc, 1);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], llMNVSRecordMainBattleScoreSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 222.0F;
    sobj->pos.y = 28.0F;
}

/* mnvsrecord.c:614-618 0x80132A50, verbatim. */
void mnVSRecordPortraitArrowsProcUpdate(GObj *gobj)
{
    gobj->flags = (sMNVSRecordStatsKind == nMNVSRecordKindIndiv) ? GOBJ_FLAG_NONE : GOBJ_FLAG_HIDDEN;
}

/* mnvsrecord.c:620-653 0x80132A7C, verbatim. */
void mnVSRecordMakePortraitStatsArrows(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mnVSRecordPortraitArrowsProcUpdate, nGCProcessKindFunc, 1);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[1], llMNDataCommonArrowLSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xE3;
    sobj->sprite.green = 0x7D;
    sobj->sprite.blue = 0x0C;

    sobj->pos.x = 40.0F;
    sobj->pos.y = 78.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[1], llMNDataCommonArrowRSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xE3;
    sobj->sprite.green = 0x7D;
    sobj->sprite.blue = 0x0C;

    sobj->pos.x = 105.0F;
    sobj->pos.y = 78.0F;
}

/* mnvsrecord.c:655-664 0x80132BA4, verbatim. */
void mnVSRecordResortArrowsProcUpdate(GObj *gobj)
{
    gobj->flags =
    (
        (sMNVSRecordStatsKind == nMNVSRecordKindBattleScore) ||
        (sMNVSRecordStatsKind == nMNVSRecordKindRanking)
    )
    ? GOBJ_FLAG_NONE : GOBJ_FLAG_HIDDEN;
}

/* mnvsrecord.c:666-687 0x80132BD4, verbatim. */
void mnVSRecordMakeResortArrows(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mnVSRecordResortArrowsProcUpdate, nGCProcessKindFunc, 1);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], llMNVSRecordMainDownArrowsSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 281.0F;
    sobj->pos.y = 39.0F;

    sobj->sprite.red = 0xE3;
    sobj->sprite.green = 0x7D;
    sobj->sprite.blue = 0xC;
}

/* mnvsrecord.c:689-716 0x80132C9C, verbatim. */
void mnVSRecordColumnArrowsProcUpdate(GObj *gobj)
{
    gobj->flags = (sMNVSRecordStatsKind == nMNVSRecordKindRanking) ? GOBJ_FLAG_NONE : GOBJ_FLAG_HIDDEN;
}

void mnVSRecordMakeColumnArrows(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, mnVSRecordColumnArrowsProcUpdate, nGCProcessKindFunc, 1);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], llMNVSRecordMainSideArrowsSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xE3;
    sobj->sprite.green = 0x7D;
    sobj->sprite.blue = 0x0C;

    sobj->pos.x = 25.0F;
    sobj->pos.y = 47.0F;
}

/* mnvsrecord.c:718-737 0x80132D90. DIVERGES: mnvsrecord.h says why every
 * gDPFillRectangle line below is lbCommonSpriteFillRect instead, at the
 * grid's grey (0x62, 0x62, 0x6A) opaque (src/dc/mndata.c's
 * mnDataLabelsProcDisplay is the same shape). N64 fill rectangles are
 * lower-right *inclusive*; the port's corner is exclusive, so every
 * coordinate the decomp shares between two edges (a 1px line's far end)
 * gets +1 here and nowhere else changes. */
void mnVSRecordDrawBattleScoreGrid(void)
{
    s32 i;

    for (i = 0; i < 14; i++)
    {
        lbCommonSpriteFillRect(24, 48 + i * 13, 296, 49 + i * 13, 0x62, 0x62, 0x6A, 0xFF);
    }
    lbCommonSpriteFillRect(24, 48, 25, 218, 0x62, 0x62, 0x6A, 0xFF);

    for (i = 0; i < 14; i++)
    {
        s32 x = (i == 13) ? (i * 18 + 61) : (i * 18 + 48);

        lbCommonSpriteFillRect(x, 48, x + 1, 218, 0x62, 0x62, 0x6A, 0xFF);
    }
}

/* mnvsrecord.c:739-773 0x80132EE4. DIVERGES: as mnVSRecordDrawBattleScoreGrid. */
void mnVSRecordDrawRankingGrid(s32 first_column)
{
    s32 x;
    s32 ids[ARRAY_COUNT(dMNVSRecordRankingColumnWidths)];
    s32 i;

    for (i = 0; i < ARRAY_COUNT(ids); i++)
    {
        if (first_column >= ARRAY_COUNT(dMNVSRecordRankingColumnWidths))
        {
            first_column -= ARRAY_COUNT(dMNVSRecordRankingColumnWidths);
        }
        ids[i] = first_column++;
    }
    lbCommonSpriteFillRect(24, 46, 296, 47, 0x62, 0x62, 0x6A, 0xFF);

    for (i = 0; i < 13; i++)
    {
        lbCommonSpriteFillRect(24, 61 + i * 13, 296, 62 + i * 13, 0x62, 0x62, 0x6A, 0xFF);
    }
    lbCommonSpriteFillRect(24, 46, 25, 218, 0x62, 0x62, 0x6A, 0xFF);

    x = 48;

    for(i = 0; i < ARRAY_COUNT(ids) + 1; i++)
    {
        lbCommonSpriteFillRect(x, 46, x + 1, 218, 0x62, 0x62, 0x6A, 0xFF);

        if (i < ARRAY_COUNT(ids))
        {
            x += dMNVSRecordRankingColumnWidths[ids[i]];
        }
    }
}

/* mnvsrecord.c:775-792 0x801330FC. DIVERGES: as mnVSRecordDrawBattleScoreGrid. */
void mnVSRecordDrawIndivGrid(void)
{
    s32 i;

    lbCommonSpriteFillRect(26, 144, 294, 145, 0x62, 0x62, 0x6A, 0xFF);

    for (i = 0; i < 5; i++)
    {
        lbCommonSpriteFillRect(26, 157 + i * 12, 294, 158 + i * 12, 0x62, 0x62, 0x6A, 0xFF);
    }
    lbCommonSpriteFillRect(26, 144, 27, 206, 0x62, 0x62, 0x6A, 0xFF);

    for (i = 0; i < 13; i++)
    {
        lbCommonSpriteFillRect(65 + i * 19, 144, 66 + i * 19, 206, 0x62, 0x62, 0x6A, 0xFF);
    }
}

/* mnvsrecord.c:794-823 0x8013328C. DIVERGES: the decomp wraps its three
 * grid functions in gDPPipeSync/SetCycleType(G_CYC_FILL)/SetRenderMode(
 * NOOP)/SetFillColor(syVideoGetFillColor(...)) ... SetRenderMode(
 * G_RM_AA_ZB_OPA_SURF)/SetCycleType(G_CYC_1CYCLE); the port's fill
 * rectangles carry their own header per call and need none of it --
 * syVideoGetFillColor's cut is the same one mnvsrecord.h describes. */
void mnVSRecordTableGridProcDisplay(GObj *gobj)
{
    (void)gobj;

    switch (sMNVSRecordStatsKind)
    {
    case nMNVSRecordKindBattleScore:
        mnVSRecordDrawBattleScoreGrid();
        break;

    case nMNVSRecordKindRanking:
        mnVSRecordDrawRankingGrid(sMNVSRecordFirstColumn);
        break;

    case nMNVSRecordKindIndiv:
        mnVSRecordDrawIndivGrid();
        break;
    }
    lbCommonClearExternSpriteParams();
}

/* mnvsrecord.c:825-830 0x801333EC, verbatim. */
void mnVSRecordMakeStatsGrid(void)
{
    GObj *gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, mnVSRecordTableGridProcDisplay, 3, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mnvsrecord.c:832-875 0x80133438, verbatim: only BattleScore and Indiv
 * columns carry an icon of their own (Ranking's rows do, through
 * mnVSRecordSetRowIconPosition below), so the switch has no Ranking
 * arm. Every caller is mnVSRecordMakeColumnIcons, which the same two
 * screens call, so the gap is unreachable; kept without a default arm,
 * as the decomp has it. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
void mnVSRecordSetIconPositionForColumn(SObj *sobj, s32 column)
{
    f32 x;
    f32 y;
    s32 fkind;
    s32 col_width;
    Vec2f offsets[/* */] =
    {
        { 1.0F, -5.0F }, { 1.0F, -6.0F },
        { 0.0F, -6.0F }, { 0.0F, -4.0F },
        { 1.0F, -6.0F }, { 0.0F, -5.0F },
        { 1.0F, -5.0F }, { 0.0F, -3.0F },
        { 0.0F,  1.0F }, { 0.0F, -5.0F },
        { 0.0F, -1.0F }, { 0.0F, -2.0F }
    };

    switch (sMNVSRecordStatsKind)
    {
    case nMNVSRecordKindBattleScore:
        col_width = 18;
        x = 49.0F;
        y = 49.0F;
        fkind = sMNVSRecordBattleScoreFighterKinds[column];
        break;

    case nMNVSRecordKindIndiv:
        col_width = 19;
        x = 66.0F;
        y = 145.0F;
        fkind = sMNVSRecordIndivFighterKinds[column];
        break;
    }
    if (mnVSRecordCheckHaveFighterKind(fkind))
    {
        sobj->pos.x = x + (col_width * column) + offsets[fkind].x;
        sobj->pos.y = y + offsets[fkind].y;
    }
    else
    {
        sobj->pos.x = x + (col_width * column) + 5.0F;
        sobj->pos.y = y;
    }
}
#pragma GCC diagnostic pop

/* mnvsrecord.c:877-892 0x801335A0, verbatim. */
SObj* mnVSRecordMakeLockedIcon(GObj *gobj)
{
    SObj *sobj;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], llMNVSRecordMainQuestionSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x8A;
    sobj->sprite.green = 0x88;
    sobj->sprite.blue = 0x92;

    return sobj;
}

/* mnvsrecord.c:894-933 0x801335FC, verbatim: fkinds is set for
 * BattleScore or Indiv only, the two screens with column icons
 * (mnVSRecordSetIconPositionForColumn above says why the gap is
 * unreachable). */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
void mnVSRecordMakeColumnIcons(GObj *gobj)
{
    intptr_t offsets[/* */] =
    {
        llMNVSRecordMainMarioIconBWSprite,  llMNVSRecordMainFoxIconBWSprite,
        llMNVSRecordMainDonkeyIconBWSprite, llMNVSRecordMainSamusIconBWSprite,
        llMNVSRecordMainLuigiIconBWSprite,  llMNVSRecordMainLinkIconBWSprite,
        llMNVSRecordMainYoshiIconBWSprite,  llMNVSRecordMainCaptainIconBWSprite,
        llMNVSRecordMainKirbyIconBWSprite,  llMNVSRecordMainPikachuIconBWSprite,
        llMNVSRecordMainPurinIconBWSprite,  llMNVSRecordMainNessIconBWSprite
    };
    s32 i;
    SObj *sobj;
    s32 *fkinds;

    switch (sMNVSRecordStatsKind)
    {
    case nMNVSRecordKindBattleScore:
        fkinds = sMNVSRecordBattleScoreFighterKinds;
        break;

    case nMNVSRecordKindIndiv:
        fkinds = sMNVSRecordIndivFighterKinds;
        break;
    }
    for (i = 0; i < ARRAY_COUNT(offsets); i++)
    {
        if (mnVSRecordCheckHaveFighterKind(fkinds[i]) != FALSE)
        {
            sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], offsets[fkinds[i]]));

            sobj->sprite.attr &= ~SP_FASTCOPY;
            sobj->sprite.attr |= SP_TRANSPARENT;
        }
        else sobj = mnVSRecordMakeLockedIcon(gobj);

        mnVSRecordSetIconPositionForColumn(sobj, i);
    }
}
#pragma GCC diagnostic pop

/* mnvsrecord.c:935-971 0x80133740, verbatim: fkind is set for
 * BattleScore or Ranking only, the two screens with row icons (Indiv
 * uses one portrait, not a row). Every caller is
 * mnVSRecordMakeRowIcons, which the same two screens call, so the gap
 * is unreachable. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
void mnVSRecordSetRowIconPosition(SObj *sobj, s32 row)
{
    f32 x = 25.0F;
    f32 y = 62.0F;
    s32 fkind;
    Vec2f offsets[/* */] =
    {
        { 5.0F, 0.0F }, { 5.0F, 0.0F },
        { 0.0F, 0.0F }, { 0.0F, 0.0F },
        { 3.0F, 0.0F }, { 5.0F, 0.0F },
        { 5.0F, 0.0F }, { 1.0F, 0.0F },
        { 0.0F, 1.0F }, { 0.0F, 0.0F },
        { 4.0F, 0.0F }, { 3.0F, 0.0F }
    };

    switch (sMNVSRecordStatsKind)
    {
    case nMNVSRecordKindBattleScore:
        fkind = sMNVSRecordBattleScoreFighterKinds[row];
        break;

    case nMNVSRecordKindRanking:
        fkind = sMNVSRecordRankingFighterKindOrder[row];
        break;
    }
    if (mnVSRecordCheckHaveFighterKind(fkind) != FALSE)
    {
        sobj->pos.x = x + offsets[fkind].x;
        sobj->pos.y = y + (row * 13) + offsets[fkind].y;
    }
    else
    {
        sobj->pos.x = x + 8.0F;
        sobj->pos.y = y + (row * 13);
    }
}
#pragma GCC diagnostic pop

/* mnvsrecord.c:973-1013 0x8013388C, verbatim: fkinds is set for
 * BattleScore or Ranking only (mnVSRecordSetRowIconPosition above says
 * why the gap is unreachable). */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
void mnVSRecordMakeRowIcons(GObj *gobj)
{
    intptr_t offsets[/* */] =
    {
        llMNVSRecordMainMarioIconColorSprite,  llMNVSRecordMainFoxIconColorSprite,
        llMNVSRecordMainDonkeyIconColorSprite, llMNVSRecordMainSamusIconColorSprite,
        llMNVSRecordMainLuigiIconColorSprite,  llMNVSRecordMainLinkIconColorSprite,
        llMNVSRecordMainYoshiIconColorSprite,  llMNVSRecordMainCaptainIconColorSprite,
        llMNVSRecordMainKirbyIconColorSprite,  llMNVSRecordMainPikachuIconColorSprite,
        llMNVSRecordMainPurinIconColorSprite,  llMNVSRecordMainNessIconColorSprite
    };
    s32 i;
    SObj *sobj;
    s32 *fkinds;

    switch (sMNVSRecordStatsKind)
    {
    case nMNVSRecordKindBattleScore:
        fkinds = sMNVSRecordBattleScoreFighterKinds;
        break;

    case nMNVSRecordKindRanking:
        fkinds = sMNVSRecordRankingFighterKindOrder;
        break;
    }
    for (i = 0; i < ARRAY_COUNT(offsets); i++)
    {
        if (mnVSRecordCheckHaveFighterKind(fkinds[i]) != FALSE)
        {
            sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], offsets[fkinds[i]]));

            sobj->sprite.attr &= ~SP_FASTCOPY;
            sobj->sprite.attr |= SP_TRANSPARENT;
        }
        else sobj = mnVSRecordMakeLockedIcon(gobj);

        mnVSRecordSetRowIconPosition(sobj, i);
    }
}
#pragma GCC diagnostic pop

/* mnvsrecord.c:1015-1065 0x801339D0, verbatim (bar the dropped
 * `s32 unused[2];` local the decomp declares and never reads). */
s32 mnVSRecordGetRanking(s32 fkind)
{
    s32 fkinds[(nFTKindPlayableEnd - nFTKindPlayableStart) + 1];
    s32 rank[(nFTKindPlayableEnd - nFTKindPlayableStart) + 1];
    s32 current_order;
    s32 i;
    f64 stats[(nFTKindPlayableEnd - nFTKindPlayableStart) + 1];

    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        fkinds[i] = mnVSRecordGetFighterKindByIndex(i);
    }
    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        stats[i] = mnVSRecordGetWinPercent(i);
    }
    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        s32 j;

        for (j = i + 1; j <= nFTKindPlayableEnd; j++)
        {
            if
            (
                (mnVSRecordCheckHaveFighterKind(fkinds[i]) == FALSE) ||
                (stats[fkinds[i]] < stats[fkinds[j]]) &&
                (mnVSRecordCheckHaveFighterKind(fkinds[j]) != FALSE)
            )
            {
                s32 prev = fkinds[i];

                fkinds[i] = fkinds[j];
                fkinds[j] = prev;
            }
        }
    }
    current_order = 1;

    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        rank[fkinds[i]] = current_order;

        if ((i != nFTKindPlayableEnd) && (stats[fkinds[i]] != stats[fkinds[i + 1]]))
        {
            current_order = i + 2;
        }
    }
    return rank[fkind];
}

/* mnvsrecord.c:1067-1124 0x80133C60, verbatim. */
void mnVSRecordMakePortraitStats(GObj *gobj, s32 fkind)
{
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
    u32 string_colors[/* */] = { 0x8A, 0x88, 0x92 };
    u32 digit_colors[/* */] = { 0x00, 0x00, 0x00, 0x8A, 0x88, 0x92 };

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], llMNVSRecordMainPortraitWallpaperSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 52.0F;
    sobj->pos.y = 55.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[2], offsets[fkind]));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 57.0F;
    sobj->pos.y = 60.0F;

    mnVSRecordMakeString(gobj, "RANKING", 150, 60, string_colors);
    mnVSRecordMakeDigits(gobj, 12, 265, 58, digit_colors, FALSE, TRUE, 2, FALSE);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], llMNVSRecordMainSymbolSlashSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 251.0F;
    sobj->pos.y = 59.0F;

    sobj->sprite.red = string_colors[0];
    sobj->sprite.green = string_colors[1];
    sobj->sprite.blue = string_colors[2];

    mnVSRecordMakeDigits(gobj, mnVSRecordGetRanking(fkind), 250, 58, digit_colors, FALSE, TRUE, 2, FALSE);

    mnVSRecordMakeString(gobj, "USED %", 150, 68, string_colors);
    mnVSRecordMakeDigits(gobj, mnVSRecordGetUsePercent(fkind) * 10, 265, 66, digit_colors, TRUE, TRUE, 3, FALSE);

    mnVSRecordMakeString(gobj, "ATTACK 3TOTAL", 149, 78, string_colors);
    mnVSRecordMakeDigits(gobj, gSCManagerBackupData.vs_records[fkind].damage_given, 265, 76, digit_colors, FALSE, TRUE, 6, FALSE);

    mnVSRecordMakeString(gobj, "DAMAGE TOTAL", 150, 86, string_colors);
    mnVSRecordMakeDigits(gobj, gSCManagerBackupData.vs_records[fkind].damage_taken, 265, 84, digit_colors, FALSE, TRUE, 6, FALSE);
}

/* mnvsrecord.c:1126-1235 0x80133FE8, verbatim. */
void mnVSRecordSortData(s32 stats_kind)
{
    s32 fkinds[(nFTKindPlayableEnd - nFTKindPlayableStart) + 1];
    f64 stats[(nFTKindPlayableEnd - nFTKindPlayableStart) + 1];
    s32 i;

    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        fkinds[i] = mnVSRecordGetFighterKindByIndex(i);
    }
    switch (stats_kind)
    {
    case nMNVSRecordKindBattleScore:
        for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
        {
            stats[i] = mnVSRecordGetKOs(i);
        }
        break;

        case nMNVSRecordKindRanking:
            for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
            {
                switch (sMNVSRecordFirstColumn)
                {
                case nMNVSRecordRankingKindWinPercent:
                    stats[i] = mnVSRecordGetWinPercent(i);
                    break;

                case nMNVSRecordRankingKindKOs:
                    stats[i] = mnVSRecordGetKOs(i);
                    break;

                case nMNVSRecordRankingKindTKO:
                    stats[i] = mnVSRecordGetTKO(i);
                    break;

                case nMNVSRecordRankingKindSDPercent:
                    stats[i] = mnVSRecordGetSDPercent(i);
                    break;

                case nMNVSRecordRankingKindTime:
                    stats[i] = gSCManagerBackupData.vs_records[i].time_used;
                    break;

                case nMNVSRecordRankingKindUsePercent:
                    stats[i] = mnVSRecordGetUsePercent(i);
                    break;

                case nMNVSRecordRankingKindAvg:
                    stats[i] = mnVSRecordGetAvg(i);
                    break;
                }
            }
            break;

    case nMNVSRecordKindIndiv:
        for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
        {
            stats[i] = mnVSRecordGetWinPercentAgainst(sMNVSRecordRankingFighterKindOrder[sMNVSRecordCurrentIndex], i);
        }
        break;
    }
    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        s32 j;

        for (j = i + 1; j <= nFTKindPlayableEnd; j++)
        {
            if
            (
                (mnVSRecordCheckHaveFighterKind(fkinds[i]) == FALSE) ||
                (stats[fkinds[i]] < stats[fkinds[j]]) &&
                (mnVSRecordCheckHaveFighterKind(fkinds[j]) != FALSE)
            )
            {
                s32 prev = fkinds[i];

                fkinds[i] = fkinds[j];
                fkinds[j] = prev;
            }
        }
    }
    switch (stats_kind)
    {
    case nMNVSRecordKindBattleScore:
        for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
        {
            sMNVSRecordBattleScoreFighterKinds[i] = fkinds[i];
        }
        break;

    case nMNVSRecordKindRanking:
        for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
        {
            sMNVSRecordRankingFighterKindOrder[i] = fkinds[i];
        }
        break;

    case nMNVSRecordKindIndiv:
        for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
        {
            sMNVSRecordIndivFighterKinds[i] = fkinds[i];
        }
        break;

    default:
        break;
    }
}

/* mnvsrecord.c:1237-1277 0x801343E0, verbatim. */
GObj* mnVSRecordMakeBattleScoreTableValues(void)
{
    f32 x, y;
    u32 colors[/* */] = { 0x00, 0x00, 0x00, 0xE5, 0xD1, 0x99 };
    GObj *gobj;
    s32 i, j;

    gobj = gcMakeGObjSPAfter(0, NULL, 6, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 5, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        x = 66.0F;
        y = (i * 13);

        if (mnVSRecordCheckHaveFighterKind(sMNVSRecordBattleScoreFighterKinds[i]))
        {
            for (j = nFTKindPlayableStart; j <= nFTKindPlayableEnd; j++)
            {
                if (mnVSRecordCheckHaveFighterKind(sMNVSRecordBattleScoreFighterKinds[j]))
                {
                    mnVSRecordMakeDigits
                    (
                        gobj,
                        gSCManagerBackupData.vs_records[sMNVSRecordBattleScoreFighterKinds[i]].ko_count[sMNVSRecordBattleScoreFighterKinds[j]],
                        x + (j * 18),
                        y + 65.0F,
                        colors,
                        FALSE,
                        FALSE,
                        4,
                        FALSE
                    );
                }
            }
            mnVSRecordMakeDigits(gobj, mnVSRecordGetKOs(sMNVSRecordBattleScoreFighterKinds[i]), x + 216.0F + 10.0F, y + 65.0F, colors, FALSE, FALSE, 6, FALSE);
        }
    }
    return gobj;
}

/* mnvsrecord.c:1279-1308 0x80134610, the REGION_US arm. */
GObj* mnVSRecordMakeBattleScoreTableHeaders(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 5, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 4, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], llMNVSRecordMainLabelTotalSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x8A;
    sobj->sprite.green = 0x88;
    sobj->sprite.blue = 0x92;

    sobj->pos.x = 264.0F;
    sobj->pos.y = 50.0F;

    mnVSRecordMakeRowIcons(gobj);
    mnVSRecordMakeColumnIcons(gobj);

    return gobj;
}

/* mnvsrecord.c:1310-1327 0x801346D8. DIVERGES: as
 * mnVSRecordDrawBattleScoreGrid -- one lbCommonSpriteFillRect at the
 * primcolor the decomp sets, (0x27, 0x00, 0xFF) opaque. */
void mnVSRecordRankingHighlightProcDisplay(GObj *gobj)
{
    (void)gobj;

    if (sMNVSRecordStatsKind == nMNVSRecordKindRanking)
    {
        lbCommonSpriteFillRect(24, 62 + (sMNVSRecordCurrentIndex * 13), 296, 75 + (sMNVSRecordCurrentIndex * 13), 0x27, 0x00, 0xFF, 0xFF);
        lbCommonClearExternSpriteParams();
    }
}

/* mnvsrecord.c:1329-1333 0x80134868, verbatim. */
void mnVSRecordMakeRankingHighlight(void)
{
    gcAddGObjDisplay(gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT), mnVSRecordRankingHighlightProcDisplay, 2, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mnvsrecord.c:1335-1343 0x801348B4, verbatim. */
f32 mnVSRecordGetAvg(s32 fkind)
{
    if (gSCManagerBackupData.vs_records[fkind].games_played != 0)
    {
        return (f32) gSCManagerBackupData.vs_records[fkind].player_count_tally / gSCManagerBackupData.vs_records[fkind].games_played;
    }
    else return 0.0F;
}

/* mnvsrecord.c:1345-1356 0x80134934, verbatim. */
s32 mnVSRecordGetGamesPlayedSum(void)
{
    s32 i;
    s32 total = 0;

    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        total += gSCManagerBackupData.vs_records[i].games_played;
    }
    return total;
}

/* mnvsrecord.c:1358-1370 0x80134978, verbatim. */
f32 mnVSRecordGetUsePercent(s32 fkind)
{
    f32 use_percent;

    if (mnVSRecordGetGamesPlayedSum() != 0.0F)
    {
        use_percent = gSCManagerBackupData.vs_records[fkind].games_played / (f32) mnVSRecordGetGamesPlayedSum();
    }
    else use_percent = 0.0F;

    return use_percent * 100.0F;
}

/* mnvsrecord.c:1372-1386 0x80134A1C, verbatim. */
f32 mnVSRecordGetSDPercent(s32 fkind)
{
    f32 selfdestruct_percent;
    f32 total_tkos = mnVSRecordGetTKO(fkind);
    f32 selfdestructs = gSCManagerBackupData.vs_records[fkind].selfdestructs;

    if (total_tkos != 0.0F)
    {
        selfdestruct_percent = selfdestructs / total_tkos;
    }
    else selfdestruct_percent = 0.0F;

    return selfdestruct_percent * 100.0F;
}

/* mnvsrecord.c:1388-1552 0x80134AA8, verbatim. */
GObj* mnVSRecordMakeRankingTableValues(s32 column)
{
    s32 col_widths[/* */] = { 27, 30, 30, 23, 35, 27, 39 };
    GObj *gobj;
    u32 colors[/* */] = { 0x00, 0x00, 0x00, 0xE5, 0xD1, 0x99 };
    s32 i;
    s32 j;
    s32 column_order[ARRAY_COUNT(col_widths)];
    s32 x;
    f32 y;

    for (i = 0; i < (ARRAY_COUNT(col_widths) + ARRAY_COUNT(column_order)) / 2; i++)
    {
        if (column >= ARRAY_COUNT(col_widths))
        {
            column -= ARRAY_COUNT(col_widths);
        }
        column_order[i] = column++;
    }

    gobj = gcMakeGObjSPAfter(0, NULL, 6, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 5, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        if (mnVSRecordCheckHaveFighterKind(sMNVSRecordRankingFighterKindOrder[i]))
        {
            x = 48;
            y = (i * 13) + 65.0F;

            for (j = 0; j < (ARRAY_COUNT(col_widths) + ARRAY_COUNT(column_order)) / 2; j++)
            {
                SObj *sobj;

                switch (column_order[j])
                {
                case nMNVSRecordRankingKindWinPercent:
                    mnVSRecordMakeDigits
                    (
                        gobj, mnVSRecordGetWinPercent(sMNVSRecordRankingFighterKindOrder[i]) * 10.0F,
                        col_widths[column_order[j]] + x,
                        y,
                        colors,
                        TRUE,
                        FALSE,
                        3,
                        FALSE
                    );
                    break;

                case nMNVSRecordRankingKindKOs:
                    mnVSRecordMakeDigits
                    (
                        gobj,
                        mnVSRecordGetKOs(sMNVSRecordRankingFighterKindOrder[i]),
                        col_widths[column_order[j]] + x,
                        y,
                        colors,
                        FALSE,
                        FALSE,
                        6,
                        FALSE
                    );
                    break;

                case nMNVSRecordRankingKindTKO:
                    mnVSRecordMakeDigits
                    (
                        gobj,
                        mnVSRecordGetTKO(sMNVSRecordRankingFighterKindOrder[i]),
                        col_widths[column_order[j]] + x,
                        y,
                        colors,
                        FALSE,
                        FALSE,
                        6,
                        FALSE
                    );
                    break;

                case nMNVSRecordRankingKindSDPercent:
                    mnVSRecordMakeDigits
                    (
                        gobj,
                        mnVSRecordGetSDPercent(sMNVSRecordRankingFighterKindOrder[i]) * 10.0F,
                        col_widths[column_order[j]] + x,
                        y,
                        colors,
                        TRUE,
                        FALSE,
                        3,
                        FALSE
                    );
                    break;

                case nMNVSRecordRankingKindTime:
                    mnVSRecordMakeDigits
                    (
                        gobj,
                        (gSCManagerBackupData.vs_records[sMNVSRecordRankingFighterKindOrder[i]].time_used % 3600) / 60,
                        col_widths[column_order[j]] + x,
                        y,
                        colors,
                        FALSE,
                        FALSE,
                        2,
                        TRUE
                    );
                    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], llMNVSRecordMainSymbolColonSprite));
                    mnVSRecordSetSpriteColors(sobj, colors);

                    sobj->pos.x = col_widths[column_order[j]] + x - 11;
                    sobj->pos.y = y;

                    mnVSRecordMakeDigits
                    (
                        gobj,
                        gSCManagerBackupData.vs_records[sMNVSRecordRankingFighterKindOrder[i]].time_used / 3600,
                        col_widths[column_order[j]] + x - 13,
                        y,
                        colors,
                        FALSE,
                        FALSE,
                        3,
                        FALSE
                    );
                    break;

                case nMNVSRecordRankingKindUsePercent:
                    mnVSRecordMakeDigits
                    (
                        gobj,
                        mnVSRecordGetUsePercent(sMNVSRecordRankingFighterKindOrder[i]) * 10.0F,
                        col_widths[column_order[j]] + x,
                        y,
                        colors,
                        TRUE,
                        FALSE,
                        3,
                        FALSE
                    );
                    break;

                case nMNVSRecordRankingKindAvg:
                    mnVSRecordMakeDigits
                    (
                        gobj,
                        mnVSRecordGetAvg(sMNVSRecordRankingFighterKindOrder[i]) * 10.0F,
                        col_widths[column_order[j]] + x - 15,
                        y,
                        colors,
                        TRUE,
                        FALSE,
                        1,
                        FALSE
                    );
                    break;
                }
                x += dMNVSRecordRankingColumnWidths[column_order[j]];
            }
        }
    }
    return gobj;
}

/* mnvsrecord.c:1554-1612 0x80135108, the REGION_US arm. */
GObj* mnVSRecordMakeRankingTableHeaders(s32 column)
{
    GObj *gobj;
    SObj *sobj;
    intptr_t offsets[/* */] =
    {
        llMNVSRecordMainLabelWinPercentSprite,
        llMNVSRecordMainLabelKOsSprite,
        llMNVSRecordMainLabelTKOSprite,
        llMNVSRecordMainLabelSDPercentSprite,
        llMNVSRecordMainLabelTimeSprite,
        llMNVSRecordMainLabelUsePercentSprite,
        llMNVSRecordMainLabelAvgSprite
    };
    s32 x_padding[/* */] = { 2, 2, 2, 4, 4, 3, 1 };
    s32 column_order[(ARRAY_COUNT(offsets) + ARRAY_COUNT(x_padding)) / 2];
    s32 i;
    s32 x;

    for (i = 0; i < ARRAY_COUNT(column_order); i++)
    {
        if (column >= ARRAY_COUNT(column_order))
        {
            column -= ARRAY_COUNT(column_order);
        }
        column_order[i] = column++;
    }
    gobj = gcMakeGObjSPAfter(0, NULL, 5, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 4, GOBJ_PRIORITY_DEFAULT, ~0);

    x = 48;

    for (i = 0; i < ARRAY_COUNT(column_order); i++)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], offsets[column_order[i]]));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = x_padding[column_order[i]] + x;
        sobj->pos.y = 49.0F;

        sobj->sprite.red = 0x8A;
        sobj->sprite.green = 0x88;
        sobj->sprite.blue = 0x92;

        x += dMNVSRecordRankingColumnWidths[column_order[i]];
    }
    mnVSRecordMakeRowIcons(gobj);

    return gobj;
}

/* mnvsrecord.c:1614-1628 0x8013531C, verbatim. */
f32 mnVSRecordGetWinPercentAgainst(s32 this_fkind, s32 against_fkind)
{
    f32 kos_for = gSCManagerBackupData.vs_records[this_fkind].ko_count[against_fkind];
    f32 total_kos = kos_for + gSCManagerBackupData.vs_records[against_fkind].ko_count[this_fkind];
    f32 ko_percent;

    if (total_kos != 0.0F)
    {
        ko_percent = kos_for / total_kos;
    }
    else ko_percent = 0.0F;

    return ko_percent * 100.0F;
}

/* mnvsrecord.c:1630-1639 0x801353F4, verbatim. */
f32 mnVSRecordGetAvgAgainst(s32 this_fkind, s32 against_fkind)
{
    if (gSCManagerBackupData.vs_records[this_fkind].played_against[against_fkind] != 0)
    {
        return (f32) gSCManagerBackupData.vs_records[this_fkind].player_count_tallies[against_fkind] /
                           gSCManagerBackupData.vs_records[this_fkind].played_against[against_fkind];
    }
    else return 0.0F;
}

/* mnvsrecord.c:1641-1645 0x8013547C, verbatim: an empty function nothing
 * in the ROM calls (src/dc/mndata.c's func_ovl61_80131D54 is the same
 * shape). */
void func_ovl32_8013547C(void)
{
    return;
}

/* mnvsrecord.c:1647-1716 0x80135484, verbatim (bar the dropped
 * `s32 unused[2];` local). */
GObj* mnVSRecordMakeIndivTableValues(void)
{
    GObj *gobj;
    f32 y[/* */] = { 160.0F, 172.0F, 184.0F, 196.0F };
    u32 colors[/* */] = { 0x00, 0x00, 0x00, 0xE5, 0xD1, 0x99 };
    s32 i;
    f32 x;

    gobj = gcMakeGObjSPAfter(0, NULL, 6, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 5, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        if (mnVSRecordCheckHaveFighterKind(sMNVSRecordRankingFighterKindOrder[i]))
        {
            x = (i * 19) + 84.0F;
            mnVSRecordMakeDigits
            (
                gobj,
                mnVSRecordGetWinPercentAgainst(sMNVSRecordRankingFighterKindOrder[sMNVSRecordCurrentIndex], sMNVSRecordIndivFighterKinds[i]) * 10.0F,
                x,
                y[0],
                colors,
                TRUE,
                FALSE,
                3,
                FALSE
            );
            mnVSRecordMakeDigits
            (
                gobj,
                gSCManagerBackupData.vs_records[sMNVSRecordRankingFighterKindOrder[sMNVSRecordCurrentIndex]].ko_count[sMNVSRecordIndivFighterKinds[i]],
                x,
                y[1],
                colors,
                FALSE,
                FALSE,
                4,
                FALSE
            );
            mnVSRecordMakeDigits
            (
                gobj,
                gSCManagerBackupData.vs_records[sMNVSRecordIndivFighterKinds[i]].ko_count[sMNVSRecordRankingFighterKindOrder[sMNVSRecordCurrentIndex]],
                x,
                y[2],
                colors,
                FALSE,
                FALSE,
                4,
                FALSE
            );
            mnVSRecordMakeDigits
            (
                gobj,
                mnVSRecordGetAvgAgainst(sMNVSRecordRankingFighterKindOrder[sMNVSRecordCurrentIndex], sMNVSRecordIndivFighterKinds[i]) * 10.0F,
                x,
                y[3],
                colors,
                TRUE,
                FALSE,
                3,
                FALSE
            );
        }
    }
    return gobj;
}

/* mnvsrecord.c:1718-1768 0x80135784, the REGION_US arm. */
GObj* mnVSRecordMakeIndivPortraitAll(void)
{
    GObj *gobj;
    SObj *sobj;
    intptr_t offsets[/* */] =
    {
        llMNVSRecordMainLabelWinPercentSprite,
        llMNVSRecordMainLabelKOsSprite,
        llMNVSRecordMainLabelKOdSprite,
        llMNVSRecordMainLabelAvgSprite
    };
    Vec2f positions[/* */] =
    {
        { 29.0F, 159.0F },
        { 28.0F, 171.0F },
        { 25.0F, 183.0F },
        { 26.0F, 195.0F }
    };
    s32 i;

    gobj = gcMakeGObjSPAfter(0, NULL, 5, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 4, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; i < (ARRAY_COUNT(offsets) + ARRAY_COUNT(positions)) / 2; i++)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSRecordFiles[0], offsets[i]));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = positions[i].x;
        sobj->pos.y = positions[i].y;

        sobj->sprite.red = 0x8A;
        sobj->sprite.green = 0x88;
        sobj->sprite.blue = 0x92;
    }

    mnVSRecordMakeColumnIcons(gobj);
    mnVSRecordMakePortraitStats(gobj, sMNVSRecordRankingFighterKindOrder[sMNVSRecordCurrentIndex]);

    return gobj;
}

/* mnvsrecord.c:1770-1796 0x80135934, verbatim. */
void mnVSRecordMakeStats(s32 stats_kind)
{
    switch (stats_kind)
    {
    case nMNVSRecordKindBattleScore:
        mnVSRecordSortData(stats_kind);

        sMNVSRecordTableHeadersGObj = mnVSRecordMakeBattleScoreTableHeaders();
        sMNVSRecordTableValuesGObj = mnVSRecordMakeBattleScoreTableValues();
        break;

    case nMNVSRecordKindRanking:
        mnVSRecordSortData(stats_kind);

        sMNVSRecordTableHeadersGObj = mnVSRecordMakeRankingTableHeaders(sMNVSRecordFirstColumn);
        sMNVSRecordTableValuesGObj = mnVSRecordMakeRankingTableValues(sMNVSRecordFirstColumn);
        break;

    case nMNVSRecordKindIndiv:
        mnVSRecordSortData(stats_kind);

        sMNVSRecordTableHeadersGObj = mnVSRecordMakeIndivPortraitAll();
        sMNVSRecordTableValuesGObj = mnVSRecordMakeIndivTableValues();
        break;
    }
}

/* mnvsrecord.c:1798-1921 0x801359EC-0x80135C6C, verbatim: five sprite
 * cameras over the same viewport, one per display link, drawn back to
 * front by their DL priorities -- the table values (link 5) at 40, the
 * table headers (4) at 20, the grid (3) at 60, the ranking highlight (2)
 * at 70 and the labels (1) at 80 (src/dc/mndata.c's four cameras are the
 * same shape). */
void mnVSRecordMakeTableValuesCamera(void)
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
            COBJ_MASK_DLLINK(5),
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

void mnVSRecordMakeTableHeadersCamera(void)
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
            20,
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

void mnVSRecordMakeTableGridCamera(void)
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

void mnVSRecordMakeRankingHighlightCamera(void)
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

void mnVSRecordMakeLabelsCamera(void)
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

/* mnvsrecord.c:1923-1932 0x80135D0C, verbatim. */
void mnVSRecordInitVars(void)
{
    sMNVSRecordStatsKind = nMNVSRecordKindStart;
    sMNVSRecordIsChangeSubtitle = FALSE;
    sMNVSRecordCurrentIndex = 0;
    sMNVSRecordChangeWait = 0;
    sMNVSRecordFighterMask = gSCManagerBackupData.fighter_mask;
    sMNVSRecordFirstColumn = nMNVSRecordRankingKindStart;
}

/* mnvsrecord.c:1934-1946 0x80135D48, verbatim. */
void mnVSRecordRedrawStats(s32 stats_kind)
{
    if (sMNVSRecordTableHeadersGObj != NULL)
    {
        gcEjectGObj(sMNVSRecordTableHeadersGObj);
    }
    if (sMNVSRecordTableValuesGObj != NULL)
    {
        gcEjectGObj(sMNVSRecordTableValuesGObj);
    }
    mnVSRecordMakeStats(stats_kind);
}

/* mnvsrecord.c:1948-2165 0x80135D98, verbatim (bar the dropped
 * `s32 unused;` local): B on Battle Score leaves for the DATA menu; B on
 * either other tab steps back one. Up/down on Ranking resorts by the
 * fighter under the cursor; left/right on Ranking scrolls its seven
 * columns and left/right on Individual walks the fighter roster. */
void mnVSRecordFuncRun(GObj *gobj)
{
    s32 stick_range;
    s32 is_button;

    (void)gobj;

    if (sMNVSRecordChangeWait != 0)
    {
        sMNVSRecordChangeWait--;
    }
    if
    (
        (sMNVSRecordStatsKind == nMNVSRecordKindIndiv) &&
        (scSubsysControllerGetPlayerStickInRangeLR(-20, 20)) &&
        (scSubsysControllerGetPlayerStickInRangeUD(-20, 20)) &&
        (scSubsysControllerGetPlayerHoldButtons(R_JPAD | U_JPAD | R_TRIG | R_CBUTTONS | U_CBUTTONS) == FALSE) &&
        (scSubsysControllerGetPlayerHoldButtons(L_JPAD | D_JPAD | L_TRIG | L_CBUTTONS | D_CBUTTONS) == FALSE)
    )
    {
        sMNVSRecordChangeWait = 0;
    }
    if (sMNVSRecordIsChangeSubtitle)
    {
        sMNVSRecordIsChangeSubtitle = FALSE;
    }
    if (scSubsysControllerGetPlayerTapButtons(B_BUTTON))
    {
        if (sMNVSRecordStatsKind == nMNVSRecordKindBattleScore)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindData;

            syTaskmanSetLoadScene();
        }
        else
        {
            func_800269C0_275C0(nSYAudioFGMBurnS);

            sMNVSRecordStatsKind--;
            sMNVSRecordIsChangeSubtitle = TRUE;

            mnVSRecordRedrawStats(sMNVSRecordStatsKind);
        }
    }
    if
    (
        (
            (scSubsysControllerGetPlayerTapButtons(A_BUTTON)) ||
            (scSubsysControllerGetPlayerTapButtons(START_BUTTON))
        )
        &&
        (sMNVSRecordStatsKind < nMNVSRecordKindEnd)
    )
    {
        func_800269C0_275C0(nSYAudioFGMBurnS);

        sMNVSRecordStatsKind++;
        sMNVSRecordIsChangeSubtitle = TRUE;

        mnVSRecordRedrawStats(sMNVSRecordStatsKind);
    }
    if (sMNVSRecordStatsKind == nMNVSRecordKindRanking)
    {
        if
        (
            mnVSRecordCheckGetOptionButtonInput(is_button, U_JPAD | U_CBUTTONS) ||
            mnVSRecordCheckGetOptionStickInputUD(stick_range, 20, 1)
        )
        {
            func_800269C0_275C0(nSYAudioFGMFoxFoot);

            if (sMNVSRecordCurrentIndex == nFTKindPlayableStart)
            {
                sMNVSRecordCurrentIndex = nFTKindPlayableEnd;
            }
            else sMNVSRecordCurrentIndex--;

            while (mnVSRecordCheckHaveFighterKind(sMNVSRecordRankingFighterKindOrder[sMNVSRecordCurrentIndex]) == FALSE)
            {
                if (sMNVSRecordCurrentIndex == nFTKindPlayableStart)
                {
                    sMNVSRecordCurrentIndex = nFTKindPlayableEnd;
                }
                else sMNVSRecordCurrentIndex--;
            }

            mnVSRecordSetOptionChangeWaitP(is_button, stick_range, 7);
        }
        if
        (
            mnVSRecordCheckGetOptionButtonInput(is_button, D_JPAD | D_CBUTTONS) ||
            mnVSRecordCheckGetOptionStickInputUD(stick_range, -20, 0)
        )
        {
            func_800269C0_275C0(nSYAudioFGMFoxFoot);

            if (sMNVSRecordCurrentIndex == nFTKindPlayableEnd)
            {
                sMNVSRecordCurrentIndex = nFTKindPlayableStart;
            }
            else sMNVSRecordCurrentIndex++;

            while (mnVSRecordCheckHaveFighterKind(sMNVSRecordRankingFighterKindOrder[sMNVSRecordCurrentIndex]) == FALSE)
            {
                if (sMNVSRecordCurrentIndex == nFTKindPlayableEnd)
                {
                    sMNVSRecordCurrentIndex = nFTKindPlayableStart;
                }
                else sMNVSRecordCurrentIndex++;
            }
            mnVSRecordSetOptionChangeWaitN(is_button, stick_range, 7);
        }
        if
        (
            mnVSRecordCheckGetOptionButtonInput(is_button, R_JPAD | R_TRIG | R_CBUTTONS) ||
            mnVSRecordCheckGetOptionStickInputLR(stick_range, 20, 1)
        )
        {
            func_800269C0_275C0(nSYAudioFGMFoxFoot);

            if (sMNVSRecordFirstColumn == nMNVSRecordRankingKindStart)
            {
                sMNVSRecordFirstColumn = nMNVSRecordRankingKindEnd;
            }
            else sMNVSRecordFirstColumn--;

            sMNVSRecordIsChangeSubtitle = TRUE;

            mnVSRecordRedrawStats(sMNVSRecordStatsKind);

            mnVSRecordSetOptionChangeWaitP(is_button, stick_range, 7);
        }
        if
        (
            mnVSRecordCheckGetOptionButtonInput(is_button, L_JPAD | L_TRIG | L_CBUTTONS) ||
            mnVSRecordCheckGetOptionStickInputLR(stick_range, -20, 0)
        )
        {
            func_800269C0_275C0(nSYAudioFGMFoxFoot);

            if (sMNVSRecordFirstColumn == nMNVSRecordRankingKindEnd)
            {
                sMNVSRecordFirstColumn = nMNVSRecordRankingKindStart;
            }
            else sMNVSRecordFirstColumn++;

            sMNVSRecordIsChangeSubtitle = TRUE;

            mnVSRecordRedrawStats(sMNVSRecordStatsKind);

            mnVSRecordSetOptionChangeWaitN(is_button, stick_range, 7);
        }
    }
    if (sMNVSRecordStatsKind == nMNVSRecordKindIndiv)
    {
        if
        (
            mnVSRecordCheckGetOptionButtonInput(is_button, R_JPAD | R_TRIG | R_CBUTTONS) ||
            mnVSRecordCheckGetOptionStickInputLR(stick_range, 20, 1)
        )
        {
            func_800269C0_275C0(nSYAudioFGMFoxFoot);

            if (sMNVSRecordCurrentIndex == nFTKindPlayableEnd)
            {
                sMNVSRecordCurrentIndex = nFTKindPlayableStart;
            }
            else sMNVSRecordCurrentIndex++;

            while (mnVSRecordCheckHaveFighterKind(sMNVSRecordRankingFighterKindOrder[sMNVSRecordCurrentIndex]) == FALSE)
            {
                if (sMNVSRecordCurrentIndex == nFTKindPlayableEnd)
                {
                    sMNVSRecordCurrentIndex = nFTKindPlayableStart;
                }
                else sMNVSRecordCurrentIndex++;
            }
            mnVSRecordRedrawStats(sMNVSRecordStatsKind);

            if (is_button)
            {
                sMNVSRecordChangeWait = 12;
            }
            else sMNVSRecordChangeWait = mnCommonGetOptionChangeWaitP(20, 7);
        }
        if
        (
            mnVSRecordCheckGetOptionButtonInput(is_button, L_JPAD | L_TRIG | L_CBUTTONS) ||
            mnVSRecordCheckGetOptionStickInputLR(stick_range, -20, 0)
        )
        {
            func_800269C0_275C0(nSYAudioFGMFoxFoot);

            if (sMNVSRecordCurrentIndex == nFTKindPlayableStart)
            {
                sMNVSRecordCurrentIndex = nFTKindPlayableEnd;
            }
            else sMNVSRecordCurrentIndex--;

            while (mnVSRecordCheckHaveFighterKind(sMNVSRecordRankingFighterKindOrder[sMNVSRecordCurrentIndex]) == FALSE)
            {
                if (sMNVSRecordCurrentIndex == nFTKindPlayableStart)
                {
                    sMNVSRecordCurrentIndex = nFTKindPlayableEnd;
                }
                else sMNVSRecordCurrentIndex--;
            }
            mnVSRecordRedrawStats(sMNVSRecordStatsKind);

            if (is_button != FALSE)
            {
                sMNVSRecordChangeWait = 12;
            }
            else sMNVSRecordChangeWait = mnCommonGetOptionChangeWaitN(-20, 7);
        }
    }
}

/* mnvsrecord.c:774-782 lbRelocInitSetup and :783 lbRelocLoadFilesListed,
 * as every other menu scene has them (src/dc/mndata.c): four sprite
 * banks stand in for the four relocData files. The last two are opened
 * again out of banks the character select and the stage select already
 * put on the disc (mnvsrecord.h says why). */
static void mnVSRecordLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMNVSRecordFileIDs)] =
    {
        MNVSRECORD_BANK_MAIN,
        MNVSRECORD_BANK_DATACOMMON,
        MNVSRECORD_BANK_PORTRAITS,
        MNVSRECORD_BANK_FONTS
    };
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(dMNVSRecordFileIDs); i++)
    {
        if (sprite_bank_load(&sMNVSRecordBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mnVSRecord: no bank for file %d (%s)\n",
                          (int)dMNVSRecordFileIDs[i], paths[i]);
            sMNVSRecordFiles[i] = NULL;
            continue;
        }
        sMNVSRecordFiles[i] = &sMNVSRecordBanks[i];
    }
}

/* mnvsrecord.c:2167-2202 0x80136488. DIVERGES: the LBRelocSetup block is
 * mnVSRecordLoadFiles above, and gcMakeDefaultCameraGObj -- the black
 * clear camera on link 0 at DL priority 100 -- is the frame clear, which
 * the PVR does itself (the same cut as src/dc/mndata.c's). */
void mnVSRecordFuncStart(void)
{
    mnVSRecordLoadFiles();

    gcMakeGObjSPAfter(0, mnVSRecordFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

    mnVSRecordInitVars();
    mnVSRecordMakeLabelsCamera();
    mnVSRecordMakeRankingHighlightCamera();
    mnVSRecordMakeTableGridCamera();
    mnVSRecordMakeTableHeadersCamera();
    mnVSRecordMakeTableValuesCamera();
    mnVSRecordMakeLabels();
    mnVSRecordMakeSubtitle();
    mnVSRecordMakePortraitStatsArrows();
    mnVSRecordMakeResortArrows();
    mnVSRecordMakeColumnArrows();
    mnVSRecordMakeStatsGrid();
    mnVSRecordMakeStats(sMNVSRecordStatsKind);
    mnVSRecordMakeRankingHighlight();

    syAudioPlayBGM(0, nSYAudioBGMData);
}

/* mnvsrecord.c:2204 dMNVSRecordVideoSetup is the N64's video mode: see
 * mnVSRecordStartScene. */

/* mnvsrecord.c:2207-2250 (0x801369E8). The pool counts are the game's,
 * all zero: a menu scene takes its objects straight from the scene
 * heap. DIVERGES as every other menu scene's: the arena is the port's
 * region, the draw is the scene manager's, and mnVSRecordFuncLights is
 * dropped. */
SYTaskmanSetup dMNVSRecordTaskmanSetup =
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

    mnVSRecordFuncStart                 // Task start function
};

/* The bzero arm of syDmaLoadOverlay for overlay 32, this file:
 * src/dc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnVSRecordOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNVSRecordStatsKind);
    OVERLAY_CLEAR(sMNVSRecordIsChangeSubtitle);
    OVERLAY_CLEAR(sMNVSRecordTableHeadersGObj);
    OVERLAY_CLEAR(sMNVSRecordTableValuesGObj);
    OVERLAY_CLEAR(sMNVSRecordBattleScoreFighterKinds);
    OVERLAY_CLEAR(sMNVSRecordRankingFighterKindOrder);
    OVERLAY_CLEAR(sMNVSRecordIndivFighterKinds);
    OVERLAY_CLEAR(sMNVSRecordCurrentIndex);
    OVERLAY_CLEAR(sMNVSRecordFighterMask);
    OVERLAY_CLEAR(sMNVSRecordFirstColumn);
    OVERLAY_CLEAR(sMNVSRecordChangeWait);
    OVERLAY_CLEAR(sMNVSRecordFiles);
    OVERLAY_CLEAR(sMNVSRecordBanks);
}

/* mnvsrecord.c:2252-2260 0x801365D0. DIVERGES: syVideoInit, the zbuffer
 * and the arena_size line are the N64's video mode and its link map, set
 * once at boot here as in every other scene. */
void mnVSRecordStartScene(void)
{
    syTaskmanStartTask(&dMNVSRecordTaskmanSetup);
}
