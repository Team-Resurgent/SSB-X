/* mnbackupclear.c -- see mnbackupclear.h. Every function is
 * mn/mnoption/mnbackupclear.c's by name and body, REGION_US arms; the
 * line numbers are the decomp's. */
#include "mnbackupclear.h"
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
#include <lb/lbdef.h>
#include <lb/lbbackup.h>
#include <mn/mndef.h>
#include <macros.h>

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/mnscreenadjust.c's own copy of this declaration. */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mnoption.c does the same). A palette (int*, the Yes/No
 * confirm sprite's colour) comes back through sprite_bank_lut instead
 * of this macro -- src/dc/mnplayersvs.c's own note on why. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* relocData file 77 (MNBackupClear), tools/export/ssb_spriteexport.py --file 77
 * --list against the ROM. */
#define llMNBackupClearHeaderBackupClearSprite  0x00b60
#define llMNBackupClearOptionNewcomersSprite    0x03a00
#define llMNBackupClearOption1PHighScoreSprite  0x04050
#define llMNBackupClearOptionVSRecordSprite     0x046a0
#define llMNBackupClearOptionPrizeSprite        0x05340
#define llMNBackupClearOptionAllDataClearSprite 0x05990
#define llMNBackupClearOptionCircleSprite       0x05db8
#define llMNBackupClearIsOkayTextSprite         0x063c8
#define llMNBackupClearAreYouSureTextSprite     0x069d8
#define llMNBackupClearOptionBonusStageTimeSprite 0x07020
#define llMNBackupClearOptionYesHighlightPalette  0x07500
#define llMNBackupClearOptionYesNotPalette        0x07528
#define llMNBackupClearOptionConfirmPalette       0x07550
#define llMNBackupClearOptionYesSprite            0x07580
#define llMNBackupClearOptionNoHighlightPalette   0x07a60
#define llMNBackupClearOptionNoNotPalette         0x07a88
#define llMNBackupClearOptionNoSprite             0x07ab8

/* relocData file 78 (MNBackupClearHeaderOption), its one sprite. */
#define llMNBackupClearHeaderOptionSprite 0x00b40

/* mnbackupclear.c:17-30's five macros, which name this file's wait
 * counter in the shared mnCommon* forms (mn/mndef.h), the same shape
 * mnoption.c's own five take. */
#define mnBackupClearCheckGetOptionButtonInput(is_button, mask) \
    mnCommonCheckGetOptionButtonInput(sMNBackupClearOptionChangeWait, is_button, mask)

#define mnBackupClearCheckGetOptionStickInputUD(stick_range, min, b) \
    mnCommonCheckGetOptionStickInputUD(sMNBackupClearOptionChangeWait, stick_range, min, b)

#define mnBackupClearCheckGetOptionStickInputLR(stick_range, min, b) \
    mnCommonCheckGetOptionStickInputLR(sMNBackupClearOptionChangeWait, stick_range, min, b)

#define mnBackupClearSetOptionChangeWaitP(is_button, stick_range, div) \
    mnCommonSetOptionChangeWaitP(sMNBackupClearOptionChangeWait, is_button, stick_range, div)

#define mnBackupClearSetOptionChangeWaitN(is_button, stick_range, div) \
    mnCommonSetOptionChangeWaitN(sMNBackupClearOptionChangeWait, is_button, stick_range, div)

/* The scene's three banks: [0] is mncommon.spr, the decomp's own file
 * list, unread on the US arm (mnbackupclear.h says why); [1] and [2]
 * are this screen's own two files. */
#define MNBACKUPCLEAR_BANK_COMMON "mncommon.spr"
#define MNBACKUPCLEAR_BANK        "mnbackupclear.spr"
#define MNBACKUPCLEAR_BANK_HEADER "mnbackupclearheader.spr"

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mnbackupclear.c:39-44, the three files' real ROM ids (mncommon.c's
 * own file is 0), the same literal-array shape mndata.c's
 * dMNDataFileIDs and mnoption.c's dMNOptionFileIDs take. */
u32 dMNBackupClearFileIDs[] = { 0, 77, 78 };

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* mnbackupclear.c:78-93, the six tabs' GObjs, in nMNBackupClearOption
 * order. */
GObj *sMNBackupClearOptionNewcomersGObj;
GObj *sMNBackupClearOption1PHighScoreGObj;
GObj *sMNBackupClearOptionBonusStageTimeGObj;
GObj *sMNBackupClearOptionVSRecordGObj;
GObj *sMNBackupClearOptionPrizeGObj;
GObj *sMNBackupClearOptionAllDataClearGObj;

/* mnbackupclear.c:96 sMNBackupPad0x801330B8[2], two words the link map
 * names but nothing reads. Dropped: the port has no link map to keep a
 * hole in (mnoption.c's own sMNOptionPad0x801337B0 note). */

/* mnbackupclear.c:99 */
s32 sMNBackupClearOption;

/* mnbackupclear.c:104 */
GObj *sMNBackupClearUnusedGObj;

/* mnbackupclear.c:108 */
GObj *sMNBackupClearOptionConfirmGObj;

/* mnbackupclear.c:111 - 0 = yes, 1 = no */
sb32 sMNBackupClearOptionConfirmYesOrNo;

/* mnbackupclear.c:114 */
s32 sMNBackupClearOptionMenuKind;

/* mnbackupclear.c:117 */
int *sMNBackupClearOptionConfirmLUTOrigin;

/* mnbackupclear.c:120 sMNBackupClearPad0x801330D8, one padding word.
 * Dropped the same way as sMNBackupPad0x801330B8 above. */

/* mnbackupclear.c:123, 126, 129, 132, 135 */
s32 sMNBackupClearOptionChangeWait;
s32 sMNBackupClearTotalTimeTics;
s32 sMNBackupClearUpdateWait;
s32 sMNBackupClearOptionConfirmAnimLength;
s32 sMNBackupClearReturnTic;

/* mnbackupclear.c:138 sMNBackupClearStatusBuffer[24]: the reloc
 * loader's per-file status records. Dropped with lbRelocInitSetup
 * (mnBackupClearLoadFiles). */

/* mnbackupclear.c:141 */
void *sMNBackupClearFiles[ARRAY_COUNT(dMNBackupClearFileIDs)];

/* the three banks behind sMNBackupClearFiles */
static SpriteBank sMNBackupClearBanks[ARRAY_COUNT(dMNBackupClearFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mnbackupclear.c:118-121 mnBackupClearFuncLights: the scene's
 * pre-render function, two GBI commands setting a single light for 3D
 * this scene has none of. Dropped, as every other menu scene's is
 * (src/dc/mnvsmode.c). */

/* mnbackupclear.c:153-216 0x80131B24, the REGION_US arm -- which is
 * just the tracking GObj, the same cut mnoption.c's mnOptionMakeMenuGObj
 * takes. Everything the decomp function does with `option` is under
 * REGION_JP: a second frame sprite behind the JP subtitle text
 * (mnbackupclear.h says why). */
void mnBackupClearMakeUnused(s32 option)
{
    (void)option;

    sMNBackupClearUnusedGObj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);
}

/* mnbackupclear.c:219-254 0x80131BC8, verbatim. */
void mnBackupClearMakeHeaderSObjs(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 0, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNBackupClearFiles[2], llMNBackupClearHeaderOptionSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x5F;
    sobj->sprite.green = 0x58;
    sobj->sprite.blue = 0x46;

    sobj->pos.x = 24.0F;
    sobj->pos.y = 17.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNBackupClearFiles[1], llMNBackupClearHeaderBackupClearSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->envcolor.r = 0x00;
    sobj->envcolor.g = 0x00;
    sobj->envcolor.b = 0x00;

    sobj->sprite.red = 0xF2;
    sobj->sprite.green = 0xC7;
    sobj->sprite.blue = 0x0D;

    sobj->pos.x = 133.0F;
    sobj->pos.y = 22.0F;
}

/* mnbackupclear.c:257-274 0x80131CE4, verbatim. */
void mnBackupClearUpdateOptionTabColors(GObj *gobj, s32 status)
{
    SYColorRGB notcolors = { 0x7D, 0x45, 0x07 };
    SYColorRGB hicolors = { 0xFF, 0xA8, 0x00 };
    SYColorRGB *colors;

    SObj *sobj = SObjGetStruct(gobj);

    colors = (status != nMNOptionTabStatusNot) ? &hicolors : &notcolors;

    sobj->sprite.red   = colors->r;
    sobj->sprite.green = colors->g;
    sobj->sprite.blue  = colors->b;
}

/* mnbackupclear.c:277-346 0x80131D44, the REGION_US arm of pos[]
 * (the JP one sits eight rows higher -- the header's JP subtitle takes
 * the room, mnoption.c's own mnOptionMakeOptionTabs has the same
 * split), otherwise verbatim. */
void mnBackupClearSetOptionSpriteColors(void)
{
    GObj *gobj;
    SObj *sobj;

    GObj **option_gobjs[] =
    {
        &sMNBackupClearOptionNewcomersGObj,
        &sMNBackupClearOption1PHighScoreGObj,
        &sMNBackupClearOptionBonusStageTimeGObj,
        &sMNBackupClearOptionVSRecordGObj,
        &sMNBackupClearOptionPrizeGObj,
        &sMNBackupClearOptionAllDataClearGObj
    };

    intptr_t offsets[] =
    {
        llMNBackupClearOptionNewcomersSprite,
        llMNBackupClearOption1PHighScoreSprite,
        llMNBackupClearOptionBonusStageTimeSprite,
        llMNBackupClearOptionVSRecordSprite,
        llMNBackupClearOptionPrizeSprite,
        llMNBackupClearOptionAllDataClearSprite
    };

    Vec2f pos[] =
    {
        { 95.0F,  54.0F },
        { 95.0F,  81.0F },
        { 95.0F, 108.0F },
        { 95.0F, 135.0F },
        { 95.0F, 162.0F },
        { 95.0F, 189.0F }
    };

    s32 i;

    for (i = 0; i < (s32)(ARRAY_COUNT(option_gobjs) + ARRAY_COUNT(offsets) + ARRAY_COUNT(pos)) / 3; i++)
    {
        *option_gobjs[i] = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);

        gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNBackupClearFiles[1], offsets[i]));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->pos.x = pos[i].x;
        sobj->pos.y = pos[i].y;

        if (i == sMNBackupClearOption)
        {
            mnBackupClearUpdateOptionTabColors(gobj, nMNOptionTabStatusHighlight);
        }
        else mnBackupClearUpdateOptionTabColors(gobj, nMNOptionTabStatusNot);
    }
}

/* mnbackupclear.c:349-357 0x80131F38, verbatim. */
void mnBackupClearEjectOptionGObjs(void)
{
    gcEjectGObj(sMNBackupClearOptionNewcomersGObj);
    gcEjectGObj(sMNBackupClearOption1PHighScoreGObj);
    gcEjectGObj(sMNBackupClearOptionVSRecordGObj);
    gcEjectGObj(sMNBackupClearOptionBonusStageTimeGObj);
    gcEjectGObj(sMNBackupClearOptionPrizeGObj);
    gcEjectGObj(sMNBackupClearOptionAllDataClearGObj);
}

/* mnbackupclear.c:359-376 0x80131F98, the RDP commands in the port's
 * spelling (src/dc/lbcommon.h): the confirm dialog's blue border, four
 * 1px lines. N64 fill rectangles are lower-right inclusive and the
 * port's corner is exclusive, so every rectangle's lower-right corner
 * gets +1 here and nowhere else changes (src/dc/mnvsrecord.c's own note
 * on the rule). lbCommonClearExternSpriteParams and lbCommonDrawSObjAttr
 * were already this decomp's own -- lb/lbcommon.c, unchanged. */
void mnBackupClearOptionConfirmProcDisplay(GObj *gobj)
{
    lbCommonSpriteFillRect(58, 64, 263, 65, 0x00, 0x00, 0xFF, 0xFF);
    lbCommonSpriteFillRect(58, 172, 263, 173, 0x00, 0x00, 0xFF, 0xFF);
    lbCommonSpriteFillRect(58, 64, 59, 173, 0x00, 0x00, 0xFF, 0xFF);
    lbCommonSpriteFillRect(262, 64, 263, 173, 0x00, 0x00, 0xFF, 0xFF);

    lbCommonClearExternSpriteParams();
    lbCommonDrawSObjAttr(gobj);
}

/* mnbackupclear.c:379-382 0x80132124, verbatim. */
void mnBackupClearEjectOptionConfirmGObj(void)
{
    gcEjectGObj(sMNBackupClearOptionConfirmGObj);
}

/* mnbackupclear.c:385-483 0x80132148 - 0 = yes, 1 = no. Verbatim, except
 * the four `sobj->sprite.LUT = lbRelocGetFileData(int*, ...)` palette
 * reads become sprite_bank_lut, the same swap mnplayersvs.c's own gate
 * card makes and for the same reason (this file's own note on
 * lbRelocGetFileData above). */
void mnBackupClearMakeOptionConfirm(sb32 confirm_kind, sb32 yes_or_no)
{
    GObj *gobj;
    SObj *sobj;

    sMNBackupClearOptionConfirmGObj = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, mnBackupClearOptionConfirmProcDisplay, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    if (yes_or_no == 0)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNBackupClearFiles[1], llMNBackupClearOptionYesSprite));
        sobj->sprite.LUT = sprite_bank_lut((SpriteBank *)sMNBackupClearFiles[1], llMNBackupClearOptionYesHighlightPalette);
    }
    else
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNBackupClearFiles[1], llMNBackupClearOptionYesSprite));
        sobj->sprite.LUT = sprite_bank_lut((SpriteBank *)sMNBackupClearFiles[1], llMNBackupClearOptionYesNotPalette);
    }
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 189.0F;
    sobj->pos.y = 106.0F;

    if (yes_or_no == 0)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNBackupClearFiles[1], llMNBackupClearOptionNoSprite));
        sobj->sprite.LUT = sprite_bank_lut((SpriteBank *)sMNBackupClearFiles[1], llMNBackupClearOptionNoNotPalette);
    }
    else
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNBackupClearFiles[1], llMNBackupClearOptionNoSprite));
        sobj->sprite.LUT = sprite_bank_lut((SpriteBank *)sMNBackupClearFiles[1], llMNBackupClearOptionNoHighlightPalette);
    }
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 83.0F;
    sobj->pos.y = 106.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNBackupClearFiles[1], llMNBackupClearOptionCircleSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    if (yes_or_no == 0)
    {
        sobj->pos.x = 193.0F;
        sobj->pos.y = 110.0F;
    }
    else
    {
        sobj->pos.x = 87.0F;
        sobj->pos.y = 110.0F;
    }
    sobj->sprite.red = 0xEF;
    sobj->sprite.green = 0x9D;
    sobj->sprite.blue = 0x00;

    if (confirm_kind == 1)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNBackupClearFiles[1], llMNBackupClearIsOkayTextSprite));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->sprite.red = 0xEF;
        sobj->sprite.green = 0x9D;
        sobj->sprite.blue = 0x00;

        sobj->pos.x = 59.0F;
        sobj->pos.y = 83.0F;
    }
    else
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNBackupClearFiles[1], llMNBackupClearAreYouSureTextSprite));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->sprite.red = 0xEF;
        sobj->sprite.green = 0x9D;
        sobj->sprite.blue = 0x00;

        sobj->pos.x = 59.0F;
        sobj->pos.y = 83.0F;
    }
}

/* mnbackupclear.c:486-510 0x80132430, verbatim. */
void mnBackupClearMakeMainCamera(void)
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
            COBJ_MASK_DLLINK(2) |
            COBJ_MASK_DLLINK(1) |
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

/* mnbackupclear.c:513-522 0x801324D0, verbatim. */
void mnBackupClearInitVars(void)
{
    sMNBackupClearOptionMenuKind = 0;
    sMNBackupClearOption = nMNBackupClearOptionStart;
    sMNBackupClearOptionChangeWait = 0;
    sMNBackupClearTotalTimeTics = 0;
    sMNBackupClearUpdateWait = 10;
    sMNBackupClearOptionConfirmAnimLength = 0;
    sMNBackupClearReturnTic = sMNBackupClearTotalTimeTics + I_MIN_TO_TICS(5);
}

/* mnbackupclear.c:525-557 0x8013251C, verbatim: the six
 * `lbBackupClear*` calls are lb/lbbackup.c's own, compiled unmodified
 * (mnbackupclear.h says why), and were sitting unused until this
 * screen's own switch became the first thing to call them. */
void mnBackupClearApplyOptionID(s32 option)
{
    switch (option)
    {
    case nMNBackupClearOptionNewcomers:
        lbBackupClearNewcomers();
        break;

    case nMNBackupClearOption1PHighScore:
        lbBackupClear1PHighScore();
        break;

    case nMNBackupClearOptionVSRecord:
        lbBackupClearVSRecord();
        break;

    case nMNBackupClearOptionBonusStageTime:
        lbBackupClearBonusStageTime();
        break;

    case nMNBackupClearOptionPrize:
        lbBackupClearPrize();
        break;

    case nMNBackupClearOptionAllDataClear:
        lbBackupClearAllData();
        lbBackupApplyOptions();
        break;
    }
    lbBackupCorrectErrors();
    lbBackupWrite();
    func_800269C0_275C0(nSYAudioFGMOptionBackupClear);
}

/* mnbackupclear.c:559-563 0x801325CC. The decomp's own "unused?" is
 * wrong: mnBackupClearUpdateOptionMainMenu's B-button arm below calls
 * it, so it is ported like everything else it sits beside -- an empty
 * body, same as the decomp's. func_ovl53_80132928 just after it in the
 * decomp really is unreached (no caller anywhere in the file or out of
 * it) and is not ported, mnbackupclear.h says so. */
void func_ovl53_801325CC(void)
{
    return;
}

/* mnbackupclear.c:566-663 0x801325D4, verbatim. */
void mnBackupClearUpdateOptionMainMenu(void)
{
    s32 stick_range;

    GObj **option_gobjs[] =
    {
        &sMNBackupClearOptionNewcomersGObj,
        &sMNBackupClearOption1PHighScoreGObj,
        &sMNBackupClearOptionBonusStageTimeGObj,
        &sMNBackupClearOptionVSRecordGObj,
        &sMNBackupClearOptionPrizeGObj,
        &sMNBackupClearOptionAllDataClearGObj
    };
    sb32 is_button;

    if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | START_BUTTON) != FALSE)
    {
        switch (sMNBackupClearOption)
        {
        case nMNBackupClearOptionNewcomers:
        case nMNBackupClearOption1PHighScore:
        case nMNBackupClearOptionBonusStageTime:
        case nMNBackupClearOptionVSRecord:
        case nMNBackupClearOptionPrize:
        case nMNBackupClearOptionAllDataClear:
            func_800269C0_275C0(nSYAudioFGMMenuSelect);
            mnBackupClearEjectOptionGObjs();

            sMNBackupClearOptionMenuKind = 1;
            sMNBackupClearOptionConfirmYesOrNo = 1;

            mnBackupClearMakeOptionConfirm(1, sMNBackupClearOptionConfirmYesOrNo);

            sMNBackupClearUpdateWait = 10;
            return;
        }
    }
    if (scSubsysControllerGetPlayerTapButtons(B_BUTTON) != FALSE)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindOption;

        func_ovl53_801325CC();
        syTaskmanSetLoadScene();
        return;
    }
    if
    (
        mnBackupClearCheckGetOptionButtonInput(is_button, U_JPAD | U_CBUTTONS) ||
        mnBackupClearCheckGetOptionStickInputUD(stick_range, 20, 1)
    )
    {
        func_800269C0_275C0(nSYAudioFGMMenuScroll2);

        mnBackupClearSetOptionChangeWaitP(is_button, stick_range, 7);
        mnBackupClearUpdateOptionTabColors(*option_gobjs[sMNBackupClearOption], nMNOptionTabStatusNot);

        if (sMNBackupClearOption == nMNBackupClearOptionStart)
        {
            sMNBackupClearOption = nMNBackupClearOptionEnd;
        }
        else sMNBackupClearOption--;

        if (sMNBackupClearOption == nMNBackupClearOptionStart)
        {
            sMNBackupClearOptionChangeWait += 8;
        }
        gcEjectGObj(sMNBackupClearUnusedGObj);
        mnBackupClearMakeUnused(sMNBackupClearOption);
        mnBackupClearUpdateOptionTabColors(*option_gobjs[sMNBackupClearOption], nMNOptionTabStatusHighlight);
    }
    else if
    (
        mnBackupClearCheckGetOptionButtonInput(is_button, D_JPAD | D_CBUTTONS) ||
        mnBackupClearCheckGetOptionStickInputUD(stick_range, -20, 0)
    )
    {
        func_800269C0_275C0(nSYAudioFGMMenuScroll2);

        mnBackupClearSetOptionChangeWaitN(is_button, stick_range, 7);
        mnBackupClearUpdateOptionTabColors(*option_gobjs[sMNBackupClearOption], nMNOptionTabStatusNot);

        if (sMNBackupClearOption == nMNBackupClearOptionEnd)
        {
            sMNBackupClearOption = nMNBackupClearOptionStart;
        }
        else sMNBackupClearOption++;

        if (sMNBackupClearOption == nMNBackupClearOptionEnd)
        {
            sMNBackupClearOptionChangeWait += 8;
        }
        gcEjectGObj(sMNBackupClearUnusedGObj);
        mnBackupClearMakeUnused(sMNBackupClearOption);
        mnBackupClearUpdateOptionTabColors(*option_gobjs[sMNBackupClearOption], nMNOptionTabStatusHighlight);
    }
}

/* mnbackupclear.c:672-760 0x80132930, verbatim. */
void mnBackupClearUpdateOptionConfirmMenu(sb32 confirm_kind)
{
    s32 stick_range;
    sb32 is_button;

    if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | START_BUTTON) != FALSE)
    {
        switch (sMNBackupClearOptionConfirmYesOrNo)
        {
        case 0:
            func_800269C0_275C0(nSYAudioFGMMenuSelect);
            if ((confirm_kind == 1) && (sMNBackupClearOption == nMNBackupClearOptionAllDataClear))
            {
                sMNBackupClearOptionMenuKind = 2;
                sMNBackupClearOptionConfirmYesOrNo = 1;

                mnBackupClearEjectOptionConfirmGObj();
                mnBackupClearMakeOptionConfirm(2, sMNBackupClearOptionConfirmYesOrNo);

                sMNBackupClearUpdateWait = 10;
            }
            else
            {
                sMNBackupClearOptionConfirmLUTOrigin = SObjGetStruct(sMNBackupClearOptionConfirmGObj)->sprite.LUT;
                SObjGetStruct(sMNBackupClearOptionConfirmGObj)->sprite.LUT =
                    sprite_bank_lut((SpriteBank *)sMNBackupClearFiles[1], llMNBackupClearOptionConfirmPalette);

                sMNBackupClearOptionMenuKind = 0;
                sMNBackupClearOptionConfirmAnimLength = 60;

                mnBackupClearApplyOptionID(sMNBackupClearOption);
            }
            return;

        case 1:
            sMNBackupClearOptionMenuKind = 0;
            mnBackupClearEjectOptionConfirmGObj();
            mnBackupClearSetOptionSpriteColors();
            sMNBackupClearUpdateWait = 10;
            return;
        }
    }
    if (scSubsysControllerGetPlayerTapButtons(B_BUTTON) != FALSE)
    {
        sMNBackupClearOptionMenuKind = 0;
        mnBackupClearEjectOptionConfirmGObj();
        mnBackupClearSetOptionSpriteColors();
        sMNBackupClearUpdateWait = 10;
    }
    else if
    (
        ((scSubsysControllerGetPlayerTapButtons(R_JPAD | R_CBUTTONS) != FALSE)) ||
        (mnBackupClearCheckGetOptionStickInputLR(stick_range, 20, 1))
    )
    {
        sMNBackupClearOptionChangeWait = mnCommonGetOptionChangeWaitN(stick_range, 7);

        if (sMNBackupClearOptionConfirmYesOrNo == 1)
        {
            func_800269C0_275C0(nSYAudioFGMMenuScroll2);

            sMNBackupClearOptionConfirmYesOrNo = 0;

            mnBackupClearEjectOptionConfirmGObj();
            mnBackupClearMakeOptionConfirm(confirm_kind, sMNBackupClearOptionConfirmYesOrNo);
        }
    }
    else if
    (
        (scSubsysControllerGetPlayerTapButtons(L_JPAD | L_CBUTTONS) != FALSE) ||
        (mnBackupClearCheckGetOptionStickInputLR(stick_range, -20, 0))
    )
    {
        if (sMNBackupClearOptionConfirmYesOrNo == 0)
        {
            func_800269C0_275C0(nSYAudioFGMMenuScroll2);

            sMNBackupClearOptionConfirmYesOrNo = 1;

            sMNBackupClearOptionChangeWait = mnCommonGetOptionChangeWaitN(stick_range, 7);

            mnBackupClearEjectOptionConfirmGObj();
            mnBackupClearMakeOptionConfirm(confirm_kind, sMNBackupClearOptionConfirmYesOrNo);
        }
    }
}

/* mnbackupclear.c:762-822 0x80132B9C, verbatim. */
void mnBackupClearFuncRun(GObj *gobj)
{
    (void)gobj;

    sMNBackupClearTotalTimeTics++;

    if (sMNBackupClearUpdateWait != 0)
    {
        sMNBackupClearUpdateWait--;
    }
    else if (sMNBackupClearOptionConfirmAnimLength != 0)
    {
        sMNBackupClearOptionConfirmAnimLength--;

        if ((sMNBackupClearOptionConfirmAnimLength % 10) == 0)
        {
            SObj *sobj = SObjGetStruct(sMNBackupClearOptionConfirmGObj);
            int *confirm_lut = sprite_bank_lut((SpriteBank *)sMNBackupClearFiles[1], llMNBackupClearOptionConfirmPalette);

            if (sobj->sprite.LUT == confirm_lut)
            {
                sobj->sprite.LUT = sMNBackupClearOptionConfirmLUTOrigin;
            }
            else sobj->sprite.LUT = confirm_lut;
        }
        if (sMNBackupClearOptionConfirmAnimLength == 0)
        {
            mnBackupClearEjectOptionConfirmGObj();
            mnBackupClearSetOptionSpriteColors();
        }
    }
    else
    {
        if (sMNBackupClearOptionChangeWait != 0)
        {
            sMNBackupClearOptionChangeWait--;
        }
        if
        (
            (scSubsysControllerGetPlayerStickInRangeLR(-20, 20) != FALSE)         &&
            (scSubsysControllerGetPlayerStickInRangeUD(-20, 20) != FALSE)         &&
            (scSubsysControllerGetPlayerHoldButtons(U_JPAD | U_CBUTTONS) == FALSE)&&
            (scSubsysControllerGetPlayerHoldButtons(D_JPAD | D_CBUTTONS) == FALSE)
        )
        {
            sMNBackupClearOptionChangeWait = 0;
        }
        switch (sMNBackupClearOptionMenuKind)
        {
        case 0:
            mnBackupClearUpdateOptionMainMenu();
            break;

        case 1:
            mnBackupClearUpdateOptionConfirmMenu(1);
            break;

        case 2:
            mnBackupClearUpdateOptionConfirmMenu(2);
            break;
        }
    }
}

/* mnbackupclear.c:419-435-shaped lbRelocInitSetup and
 * lbRelocLoadFilesListed, as every other menu scene has them
 * (src/dc/mndata.c): three sprite banks stand in for the scene's three
 * files, loaded out of the romdisk into the scene heap and VRAM.
 * [0] (mncommon.spr) is loaded and never read on the US arm -- kept so
 * sMNBackupClearFiles keeps the decomp's own three-file shape, the same
 * choice mnoption.c's two-file version made. */
static void mnBackupClearLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMNBackupClearFileIDs)] =
    {
        MNBACKUPCLEAR_BANK_COMMON,
        MNBACKUPCLEAR_BANK,
        MNBACKUPCLEAR_BANK_HEADER
    };
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(dMNBackupClearFileIDs); i++)
    {
        if (sprite_bank_load(&sMNBackupClearBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mnBackupClear: no bank for file %d (%s)\n",
                          (int)dMNBackupClearFileIDs[i], paths[i]);
            sMNBackupClearFiles[i] = NULL;
            continue;
        }
        sMNBackupClearFiles[i] = &sMNBackupClearBanks[i];
    }
}

/* mnbackupclear.c:824-847 0x80132D34, in the game's order.
 *
 * DIVERGES: the LBRelocSetup block is mnBackupClearLoadFiles above, and
 * gcMakeDefaultCameraGObj -- the black clear camera -- is the frame
 * clear, which the PVR does itself (the same cut as src/dc/mnoption.c's
 * and src/dc/mnscreenadjust.c's). */
void mnBackupClearFuncStart(void)
{
    mnBackupClearLoadFiles();

    gcMakeGObjSPAfter(0, mnBackupClearFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

    mnBackupClearInitVars();
    mnBackupClearMakeMainCamera();
    mnBackupClearMakeHeaderSObjs();
    mnBackupClearMakeUnused(sMNBackupClearOption);
    mnBackupClearSetOptionSpriteColors();
}

/* mnbackupclear.c:856-898. The pool counts are the game's, all zero: a
 * menu scene takes its objects straight from the scene heap. DIVERGES
 * as every other menu scene's: the arena is the port's region, the draw
 * function is the scene manager's (scManagerFuncDraw is gcDrawAll,
 * src/dc/mntitle.c's own note), and mnBackupClearFuncLights is dropped.
 * dMNBackupClearVideoSetup itself is not ported: see
 * mnBackupClearStartScene. */
SYTaskmanSetup dMNBackupClearTaskmanSetup =
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

    mnBackupClearFuncStart              // Task start function
};

/* The bzero arm of syDmaLoadOverlay for overlay 53, this file:
 * src/dc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnBackupClearOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNBackupClearOptionNewcomersGObj);
    OVERLAY_CLEAR(sMNBackupClearOption1PHighScoreGObj);
    OVERLAY_CLEAR(sMNBackupClearOptionBonusStageTimeGObj);
    OVERLAY_CLEAR(sMNBackupClearOptionVSRecordGObj);
    OVERLAY_CLEAR(sMNBackupClearOptionPrizeGObj);
    OVERLAY_CLEAR(sMNBackupClearOptionAllDataClearGObj);
    OVERLAY_CLEAR(sMNBackupClearOption);
    OVERLAY_CLEAR(sMNBackupClearUnusedGObj);
    OVERLAY_CLEAR(sMNBackupClearOptionConfirmGObj);
    OVERLAY_CLEAR(sMNBackupClearOptionConfirmYesOrNo);
    OVERLAY_CLEAR(sMNBackupClearOptionMenuKind);
    OVERLAY_CLEAR(sMNBackupClearOptionConfirmLUTOrigin);
    OVERLAY_CLEAR(sMNBackupClearOptionChangeWait);
    OVERLAY_CLEAR(sMNBackupClearTotalTimeTics);
    OVERLAY_CLEAR(sMNBackupClearUpdateWait);
    OVERLAY_CLEAR(sMNBackupClearOptionConfirmAnimLength);
    OVERLAY_CLEAR(sMNBackupClearReturnTic);
    OVERLAY_CLEAR(sMNBackupClearFiles);
    OVERLAY_CLEAR(sMNBackupClearBanks);
}

/* mnbackupclear.c:900-908 0x80132E28. DIVERGES: syVideoInit, the
 * zbuffer and the arena_size line are the N64's video mode and its link
 * map, set once at boot here as in every other scene. */
void mnBackupClearStartScene(void)
{
    syTaskmanStartTask(&dMNBackupClearTaskmanSetup);
}
