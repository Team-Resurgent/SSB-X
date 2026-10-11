/* mndata.c -- see mndata.h. Every function is mn/mndata/mndata.c's by
 * name and body, REGION_US arms; the line numbers are the decomp's. */
#include "mndata.h"
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
 * src/dc/sysshim.c defines it over the FGM engine */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mnvsmode.c does the same). Each `&llXxxSprite` below
 * is written `llXxxSprite`, the number, with the label's name kept as
 * the macro's. The values are relocData files 0 (MNCommon) and 5
 * (MNData) as tools/export/ssb_spriteexport.py --list reads them off the
 * ROM. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

#define llMNCommonOptionTabLeftSprite    0x001E8
#define llMNCommonOptionTabMiddleSprite  0x00330
#define llMNCommonOptionTabRightSprite   0x00568
#define llMNCommonDecalPaperSprite       0x02A30
#define llMNCommonSmashLogoSprite        0x031F8
#define llMNCommonSmashBrosCollageSprite 0x18000

#define llMNDataCharactersTextSprite     0x014E0
#define llMNDataVSRecordTextSprite       0x01900
#define llMNDataSoundTestTextSprite      0x01D20
#define llMNDataDataTextSprite           0x023A8
#define llMNDataDataIconDarkSprite       0x04A78

/* The banks the two files became. The common bank is the mode select's,
 * which already carries every sprite this screen takes from it -- the
 * tab's three pieces, the paper decal, the logo and the collage. */
#define MNDATA_BANK_COMMON "mncommon.spr"
#define MNDATA_BANK_DATA   "mndata.spr"

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mndata.c:42 (0x80132F20): { &llMNCommonFileID, &llMNDataFileID }, the
 * two link labels' values written out. */
u32 dMNDataFileIDs[/* */] = { 0, 5 };

/* mndata.c:45-53 dMNDataLights1 and dMNDataDisplayList, the lighting
 * the pre-render function would set for the 3D this scene never draws.
 * Dropped with mnDataFuncLights (dMNDataTaskmanSetup says why). */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* mndata.c:62-68 */
GObj *sMNDataOptionCharactersGObj;
GObj *sMNDataOptionVSRecordGObj;
GObj *sMNDataOptionSoundTestGObj;

/* mndata.c:71 sMNDataPad0x80133070[2], two words of the overlay between
 * the GObjs and the cursor that nothing names. Dropped: the port has no
 * link map to keep a hole in. */

/* mndata.c:74 */
s32 sMNDataOption;

/* mndata.c:77 */
static GObj *sMNDataMenuGObj;

/* mndata.c:80-84 */
s32 sMNDataFirstAvailableOption;
s32 sMNDataLastAvailableOption;

/* mndata.c:87 */
sb32 sMNDataIsHaveSoundTest;

/* mndata.c:90 */
static sb32 sMNDataIsProceedScene;

/* mndata.c:93-99 */
static s32 sMNDataOptionChangeWait;
static s32 sMNDataTotalTimeTics;
static s32 sMNDataReturnTic;

/* mndata.c:105 sMNDataStatusBuffer[24], the reloc loader's per-file
 * status records. Dropped with lbRelocInitSetup (mnDataLoadFiles). */

/* mndata.c:110 */
void *sMNDataFiles[ARRAY_COUNT(dMNDataFileIDs)];

/* the banks behind sMNDataFiles */
static SpriteBank sMNDataBanks[ARRAY_COUNT(dMNDataFileIDs)];

/* mndata.c:20-32's five macros, which name this file's wait counter in
 * the shared mnCommon* forms (mn/mndef.h). */
#define mnDataCheckGetOptionButtonInput(is_button, mask) \
    mnCommonCheckGetOptionButtonInput(sMNDataOptionChangeWait, is_button, mask)

#define mnDataCheckGetOptionStickInputUD(stick_range, min, b) \
    mnCommonCheckGetOptionStickInputUD(sMNDataOptionChangeWait, stick_range, min, b)

#define mnDataSetOptionChangeWaitP(is_button, stick_range, div) \
    mnCommonSetOptionChangeWaitP(sMNDataOptionChangeWait, is_button, stick_range, div)

#define mnDataSetOptionChangeWaitN(is_button, stick_range, div) \
    mnCommonSetOptionChangeWaitN(sMNDataOptionChangeWait, is_button, stick_range, div)

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mndata.c:115-118 mnDataFuncLights: the scene's pre-render function,
 * two GBI commands setting a single light for 3D this scene has none
 * of. Dropped, as every other menu scene's is (src/dc/mnvsmode.c). */

/* mndata.c:121-128 0x80131B24, verbatim. The bit is the one
 * src/dc/mnmessage.c sets when the unlock message for SOUND TEST is
 * shown, and lbBackupWrite puts on the memory card. */
sb32 mnDataCheckSoundTestUnlocked(void)
{
    if (gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_SOUNDTEST)
    {
        return TRUE;
    }
    else return FALSE;
}

/* mndata.c:131-177 0x80131B4C, verbatim -- including the `default`
 * arm, which leaves `colors` uninitialized and is why the decomp's
 * comment there says "Undefined behavior". Every caller passes
 * nMNOptionTabStatusNot, Highlight or Selected, or the 0/1 of a
 * comparison against those first two, so the arm is unreachable; it is
 * kept rather than given a value because a changed default would be a
 * divergence in a function that has none.
 *
 * The three SObjs walked here are the tab's left, middle and right
 * pieces, in the order mnDataMakeOptionTab hangs them. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
void mnDataSetOptionSpriteColors(GObj *gobj, s32 status)
{
    // 0x80132F68
    SYColorRGBPair selcolors = { { 0x00, 0x00, 0x00 }, { 0xFF, 0xFF, 0xFF } };

    // 0x80132F70
    SYColorRGBPair hicolors  = { { 0x82, 0x00, 0x28 }, { 0xFF, 0x00, 0x28 } };

    // 0x80132F78
    SYColorRGBPair notcolors = { { 0x00, 0x00, 0x00 }, { 0x82, 0x82, 0xAA } };

    SYColorRGBPair *colors;
    SObj *sobj;
    s32 i;

    switch (status)
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
    sobj = SObjGetStruct(gobj);

    for (i = 0; i < 3; i++)
    {
        sobj->envcolor.r = colors->prim.r;
        sobj->envcolor.g = colors->prim.g;
        sobj->envcolor.b = colors->prim.b;

        sobj->sprite.red   = colors->env.r;
        sobj->sprite.green = colors->env.g;
        sobj->sprite.blue  = colors->env.b;

        sobj = sobj->next;
    }
}
#pragma GCC diagnostic pop

/* mndata.c:180-215 0x80131C24, verbatim: the tab is three sprites, and
 * the middle one is one texel column tiled out to `lrs` * 8 pixels
 * (cms/cmt 0, masks 4), so a tab is as wide as its caller asks. */
void mnDataMakeOptionTab(GObj *gobj, f32 pos_x, f32 pos_y, s32 lrs)
{
    SObj *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNDataFiles[0], llMNCommonOptionTabLeftSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x;
    sobj->pos.y = pos_y;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNDataFiles[0], llMNCommonOptionTabMiddleSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x + 16.0F;
    sobj->pos.y = pos_y;

    sobj->cms = 0;
    sobj->cmt = 0;

    sobj->masks = 4;
    sobj->maskt = 0;

    sobj->lrs = lrs * 8;
    sobj->lrt = 29;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNDataFiles[0], llMNCommonOptionTabRightSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x + 16.0F + (lrs * 8);
    sobj->pos.y = pos_y;
}

/* mndata.c:217-220 0x80131D54, verbatim: an empty function nothing in
 * the ROM calls. */
void func_ovl61_80131D54(void)
{
    return;
}

/* mndata.c:223-260 0x80131D5C, verbatim. Two tabs or three is the whole
 * difference SOUND TEST makes to this screen, and it is made here and
 * in the two makers below: with it the stack starts higher (42 rather
 * than 57) and steps 47 pixels rather than 69. */
void mnDataMakeCharacters(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 pos_x;
    s32 pos_y;

    if (sMNDataIsHaveSoundTest != FALSE)
    {
        pos_x = 133;
        pos_y = 42;
    }
    else
    {
        pos_x = 113;
        pos_y = 57;
    }

    sMNDataOptionCharactersGObj = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    mnDataMakeOptionTab(gobj, pos_x, pos_y, 16);

    mnDataSetOptionSpriteColors(gobj, sMNDataOption == nMNDataOptionCharacters);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNDataFiles[1], llMNDataCharactersTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x + 26;
    sobj->pos.y = pos_y + 4;

    sobj->sprite.red   = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue  = 0x00;
}

/* mndata.c:263-300 0x80131E90, verbatim. */
void mnDataMakeVSRecord(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 pos_x;
    s32 pos_y;

    if (sMNDataIsHaveSoundTest != FALSE)
    {
        pos_x = 101;
        pos_y = 89;
    }
    else
    {
        pos_x = 81;
        pos_y = 126;
    }

    sMNDataOptionVSRecordGObj = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    mnDataMakeOptionTab(gobj, pos_x, pos_y, 16);

    mnDataSetOptionSpriteColors(gobj, sMNDataOption == nMNDataOptionVSRecord);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNDataFiles[1], llMNDataVSRecordTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x + 27;
    sobj->pos.y = pos_y + 4;

    sobj->sprite.red   = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue  = 0x00;
}

/* mndata.c:303-327 0x80131FC8, verbatim. Only ever called when the tab
 * exists, so its position is the three-tab layout's and has no arm. */
void mnDataMakeSoundTest(void)
{
    GObj *gobj;
    SObj *sobj;

    sMNDataOptionSoundTestGObj = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    mnDataMakeOptionTab(gobj, 69.0F, 136.0F, 16);

    mnDataSetOptionSpriteColors(gobj, sMNDataOption == nMNDataOptionSoundTest);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNDataFiles[1], llMNDataSoundTestTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue  = 0x00;

    sobj->pos.x = 95.0F;
    sobj->pos.y = 140.0F;
}

/* mndata.c:330-342 0x801320A0, verbatim, and unused in the US build:
 * its only caller is the JP arm of mnDataMakeMenuGObj below. */
void mnDataSetSubtitleSpriteColors(SObj *sobj)
{
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->envcolor.r = 0x00;
    sobj->envcolor.g = 0x00;
    sobj->envcolor.b = 0x00;

    sobj->sprite.red   = 0xFF;
    sobj->sprite.green = 0xFF;
    sobj->sprite.blue  = 0xFF;
}

/* mndata.c:345-380 0x801320D4, the REGION_US arm -- which is the GObj
 * and nothing else. Everything the function does is under REGION_JP: a
 * frame and the chosen option's Japanese description, remade every time
 * the cursor moves, which is why mnDataFuncRun ejects and remakes this
 * GObj on each step. The US build has three sprites for those
 * descriptions in its file and never draws them, and the two tables
 * that index them go with the arm. */
void mnDataMakeMenuGObj(void)
{
    sMNDataMenuGObj = gcMakeGObjSPAfter(0, NULL, 5, GOBJ_PRIORITY_DEFAULT);
}

/* mndata.c:383-397 0x80132164, the orange panel behind the logo and the
 * word DATA: one gDPFillRectangle in G_CYC_1CYCLE under G_CC_PRIMITIVE
 * at 0xA0,0x78,0x14 and 0xE6 alpha, then the GObj's own sprites over
 * it. 1CYCLE's lower-right corner is exclusive, which is the corner
 * lbCommonSpriteFillRect takes, so the numbers are the decomp's
 * unchanged (src/dc/mnmessage.c's tint is the same shape). */
void mnDataLabelsProcDisplay(GObj *gobj)
{
    lbCommonSpriteFillRect(225, 143, 310, 230, 0xA0, 0x78, 0x14, 0xE6);

    lbCommonClearExternSpriteParams();
    lbCommonDrawSObjAttr(gobj);
}

/* mndata.c:400-432 0x801322A8, verbatim. */
void mnDataMakeLabels(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, mnDataLabelsProcDisplay, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNDataFiles[0], llMNCommonSmashLogoSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 235.0F;
    sobj->pos.y = 158.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNDataFiles[1], llMNDataDataTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue  = 0x00;

    sobj->pos.x = 206.0F;
    sobj->pos.y = 131.0F;
}

/* mndata.c:435-483 0x801323A0, verbatim: the collage, two paper decals
 * in the same orange as the panel, and the dark DATA icon over the
 * collage's own. */
void mnDataMakeDecals(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 0, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNDataFiles[0], llMNCommonSmashBrosCollageSprite));

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNDataFiles[0], llMNCommonDecalPaperSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0xA0;
    sobj->sprite.green = 0x78;
    sobj->sprite.blue  = 0x14;

    sobj->pos.x = 140.0F;
    sobj->pos.y = 143.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNDataFiles[0], llMNCommonDecalPaperSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0xA0;
    sobj->sprite.green = 0x78;
    sobj->sprite.blue  = 0x14;

    sobj->pos.x = 225.0F;
    sobj->pos.y = 56.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNDataFiles[1], llMNDataDataIconDarkSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0x99;
    sobj->sprite.green = 0x99;
    sobj->sprite.blue  = 0x99;

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;
}

/* mndata.c:486-585 0x80132534, 0x801325D4, 0x80132674 and 0x80132714,
 * verbatim: four sprite cameras over the same viewport, one per display
 * link, drawn back to front by their DL priorities -- the decals (link
 * 0) at 80, the labels (1) at 60, the option tabs (2) at 40 and the
 * JP subtitle (3) at 20. */
void mnDataMakeLink3Camera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            1, NULL, 1, GOBJ_PRIORITY_DEFAULT, lbCommonDrawSprite, 20,
            COBJ_MASK_DLLINK(3), ~0, FALSE, nGCProcessKindFunc, NULL, 1, FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

void mnDataMakeOptionsCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            1, NULL, 1, GOBJ_PRIORITY_DEFAULT, lbCommonDrawSprite, 40,
            COBJ_MASK_DLLINK(2), ~0, FALSE, nGCProcessKindFunc, NULL, 1, FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

void mnDataMakeLabelsCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            1, NULL, 1, GOBJ_PRIORITY_DEFAULT, lbCommonDrawSprite, 60,
            COBJ_MASK_DLLINK(1), ~0, FALSE, nGCProcessKindFunc, NULL, 1, FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

void mnDataMakeDecalsCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            1, NULL, 1, GOBJ_PRIORITY_DEFAULT, lbCommonDrawSprite, 80,
            COBJ_MASK_DLLINK(0), ~0, FALSE, nGCProcessKindFunc, NULL, 1, FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mndata.c:586-618 0x801327B4, verbatim: the cursor starts on the tab
 * the player came back from, which is how the port's un-ported screens
 * behave correctly by accident -- the scene manager's default arm sets
 * scene_prev to the screen that was asked for (src/dc/scmanager.c), so
 * a bounce lands the cursor exactly where the game would put it. */
void mnDataInitVars(void)
{
    switch (gSCManagerSceneData.scene_prev)
    {
    case nSCKindVSRecord:
        sMNDataOption = nMNDataOptionVSRecord;
        break;

    case nSCKindSoundTest:
        sMNDataOption = nMNDataOptionSoundTest;
        break;

    default:
        sMNDataOption = nMNDataOptionCharacters;
        break;
    }
    sMNDataFirstAvailableOption = nMNDataOptionCharacters;

    if (mnDataCheckSoundTestUnlocked() != FALSE)
    {
        sMNDataLastAvailableOption = nMNDataOptionSoundTest;
        sMNDataIsHaveSoundTest = TRUE;
    }
    else
    {
        sMNDataLastAvailableOption = nMNDataOptionVSRecord;
        sMNDataIsHaveSoundTest = FALSE;
    }
    sMNDataOptionChangeWait = 0;
    sMNDataTotalTimeTics = 0;
    sMNDataIsProceedScene = FALSE;
    sMNDataReturnTic = sMNDataTotalTimeTics + I_MIN_TO_TICS(5);
}

/* mndata.c:621-771 0x80132874, the REGION_US arms, verbatim.
 *
 * Ten tics of deaf input, then: five minutes of no input at all goes to
 * the title; A or START enters the tab's screen; B goes back to the
 * mode select; and up or down steps the cursor, wrapping at both ends,
 * with a longer wait at the ends so a held stick does not race round. */
void mnDataFuncRun(GObj *gobj)
{
    s32 stick_range;

    // 0x8013F2A4
    GObj **option_gobjs[/* */] = { &sMNDataOptionCharactersGObj, &sMNDataOptionVSRecordGObj, &sMNDataOptionSoundTestGObj };

    sb32 is_button;

    (void)gobj;

    sMNDataTotalTimeTics++;

    if (sMNDataTotalTimeTics >= 10)
    {
        if (sMNDataTotalTimeTics == sMNDataReturnTic)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            syTaskmanSetLoadScene();

            return;
        }
        if (scSubsysControllerCheckNoInputAll() == FALSE)
        {
            sMNDataReturnTic = sMNDataTotalTimeTics + I_MIN_TO_TICS(5);
        }
        if (sMNDataIsProceedScene != FALSE)
        {
            syTaskmanSetLoadScene();
            return;
        }
        if (sMNDataOptionChangeWait != 0)
        {
            sMNDataOptionChangeWait--;
        }
        if
        (
            (scSubsysControllerGetPlayerStickInRangeLR(-20, 20) != FALSE) &&
            (scSubsysControllerGetPlayerStickInRangeUD(-20, 20) != FALSE) &&
            (scSubsysControllerGetPlayerHoldButtons(U_JPAD | U_CBUTTONS) == FALSE) &&
            (scSubsysControllerGetPlayerHoldButtons(D_JPAD | D_CBUTTONS) == FALSE)
        )
        {
            sMNDataOptionChangeWait = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | START_BUTTON) != FALSE)
        {
            switch (sMNDataOption)
            {
            case nMNDataOptionCharacters:
                func_800269C0_275C0(nSYAudioFGMMenuSelect);
                mnDataSetOptionSpriteColors(*option_gobjs[sMNDataOption], nMNOptionTabStatusSelected);
                syAudioStopBGMAll();

                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKindCharacters;
                sMNDataIsProceedScene = TRUE;
                return;

            case nMNDataOptionVSRecord:
                func_800269C0_275C0(nSYAudioFGMMenuSelect);
                mnDataSetOptionSpriteColors(*option_gobjs[sMNDataOption], nMNOptionTabStatusSelected);
                syAudioStopBGMAll();

                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKindVSRecord;
                sMNDataIsProceedScene = TRUE;
                return;

            case nMNDataOptionSoundTest:
                func_800269C0_275C0(nSYAudioFGMMenuSelect);
                mnDataSetOptionSpriteColors(*option_gobjs[sMNDataOption], nMNOptionTabStatusSelected);
                syAudioStopBGMAll();

                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKindSoundTest;
                sMNDataIsProceedScene = TRUE;
                return;
            }
        }
        if (scSubsysControllerGetPlayerTapButtons(B_BUTTON) != FALSE)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindModeSelect;

            syTaskmanSetLoadScene();
            return;
        }
        if
        (
            mnDataCheckGetOptionButtonInput(is_button, U_JPAD | U_CBUTTONS) ||
            mnDataCheckGetOptionStickInputUD(stick_range, 20, 1)
        )
        {
            func_800269C0_275C0(nSYAudioFGMMenuScroll2);

            mnDataSetOptionChangeWaitP(is_button, stick_range, 7);

            mnDataSetOptionSpriteColors(*option_gobjs[sMNDataOption], nMNOptionTabStatusNot);

            if (sMNDataOption == sMNDataFirstAvailableOption)
            {
                sMNDataOption = sMNDataLastAvailableOption;
            }
            else sMNDataOption--;

            mnDataSetOptionSpriteColors(*option_gobjs[sMNDataOption], nMNOptionTabStatusHighlight);

            if (sMNDataOption == sMNDataFirstAvailableOption)
            {
                sMNDataOptionChangeWait += 8;
            }
            gcEjectGObj(sMNDataMenuGObj);

            mnDataMakeMenuGObj();
        }
        if
        (
            mnDataCheckGetOptionButtonInput(is_button, D_JPAD | D_CBUTTONS) ||
            mnDataCheckGetOptionStickInputUD(stick_range, -20, 0)
        )
        {
            func_800269C0_275C0(nSYAudioFGMMenuScroll2);

            mnDataSetOptionChangeWaitN(is_button, stick_range, 7);

            mnDataSetOptionSpriteColors(*option_gobjs[sMNDataOption], nMNOptionTabStatusNot);

            if (sMNDataOption == sMNDataLastAvailableOption)
            {
                sMNDataOption = sMNDataFirstAvailableOption;
            }
            else sMNDataOption++;

            mnDataSetOptionSpriteColors(*option_gobjs[sMNDataOption], nMNOptionTabStatusHighlight);

            if (sMNDataOption == sMNDataLastAvailableOption)
            {
                sMNDataOptionChangeWait += 8;
            }
            gcEjectGObj(sMNDataMenuGObj);

            mnDataMakeMenuGObj();
        }
    }
}

/* mndata.c:774-782 lbRelocInitSetup and :783 lbRelocLoadFilesListed, as
 * every other menu scene has them (src/dc/mnvsmode.c): two sprite banks
 * stand in for the two relocData files, loaded out of the romdisk into
 * the scene heap and VRAM. */
static void mnDataLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMNDataFileIDs)] =
    {
        MNDATA_BANK_COMMON,
        MNDATA_BANK_DATA
    };
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(dMNDataFileIDs); i++)
    {
        if (sprite_bank_load(&sMNDataBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mnData: no bank for file %d (%s)\n",
                          (int)dMNDataFileIDs[i], paths[i]);
            sMNDataFiles[i] = NULL;
            continue;
        }
        sMNDataFiles[i] = &sMNDataBanks[i];
    }
}

/* mndata.c:774-866 0x80132D64, in the game's order.
 *
 * DIVERGES: the LBRelocSetup block is mnDataLoadFiles above, and
 * gcMakeDefaultCameraGObj -- the black clear camera on link 0 at DL
 * priority 100 -- is the frame clear, which the PVR does itself (the
 * same cut as src/dc/mnmodeselect.c's). */
void mnDataFuncStart(void)
{
    mnDataLoadFiles();

    gcMakeGObjSPAfter(0, mnDataFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

    mnDataInitVars();
    mnDataMakeDecalsCamera();
    mnDataMakeLabelsCamera();
    mnDataMakeOptionsCamera();
    mnDataMakeLink3Camera();
    mnDataMakeDecals();
    mnDataMakeLabels();
    mnDataMakeCharacters();
    mnDataMakeVSRecord();

    if (sMNDataIsHaveSoundTest != FALSE)
    {
        mnDataMakeSoundTest();
    }
    mnDataMakeMenuGObj();

    if
    (
        (gSCManagerSceneData.scene_prev == nSCKindVSRecord)  ||
        (gSCManagerSceneData.scene_prev == nSCKindCharacters)||
        (gSCManagerSceneData.scene_prev == nSCKindSoundTest)
    )
    {
        syAudioPlayBGM(0, nSYAudioBGMModeSelect);
    }
}

/* mndata.c:868 dMNDataVideoSetup is the N64's video mode: see
 * mnDataStartScene. */

/* mndata.c:871-912 (0x80132FCC). The pool counts are the game's, all
 * zero: a menu scene takes its objects straight from the scene heap.
 * DIVERGES as every other menu scene's: the arena is the port's region,
 * the draw is the scene manager's, and mnDataFuncLights is dropped. */
SYTaskmanSetup dMNDataTaskmanSetup =
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

    mnDataFuncStart                     // Task start function
};

/* The bzero arm of syDmaLoadOverlay for overlay 61, this file:
 * src/dc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnDataOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNDataOptionCharactersGObj);
    OVERLAY_CLEAR(sMNDataOptionVSRecordGObj);
    OVERLAY_CLEAR(sMNDataOptionSoundTestGObj);
    OVERLAY_CLEAR(sMNDataOption);
    OVERLAY_CLEAR(sMNDataMenuGObj);
    OVERLAY_CLEAR(sMNDataFirstAvailableOption);
    OVERLAY_CLEAR(sMNDataLastAvailableOption);
    OVERLAY_CLEAR(sMNDataIsHaveSoundTest);
    OVERLAY_CLEAR(sMNDataIsProceedScene);
    OVERLAY_CLEAR(sMNDataOptionChangeWait);
    OVERLAY_CLEAR(sMNDataTotalTimeTics);
    OVERLAY_CLEAR(sMNDataReturnTic);
    OVERLAY_CLEAR(sMNDataFiles);
    OVERLAY_CLEAR(sMNDataBanks);
}

/* mndata.c:868-874 0x80132EC0. DIVERGES: syVideoInit, the zbuffer and
 * the arena_size line are the N64's video mode and its link map, set
 * once at boot here as in every other scene. */
void mnDataStartScene(void)
{
    syTaskmanStartTask(&dMNDataTaskmanSetup);
}
