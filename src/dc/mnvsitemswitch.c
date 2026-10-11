/* mnvsitemswitch.c -- see mnvsitemswitch.h. Every function is
 * mn/mnvsmode/mnvsitemswitch.c's by name and body, REGION_US arms; the
 * line numbers are the decomp's. */
#include "mnvsitemswitch.h"
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
#include <it/itdef.h>
#include <mn/mndef.h>
#include <PR/os.h>
#include <macros.h>

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/sysshim.c defines it over the FGM engine */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mnvsoptions.c does the same). Each `&llXxxSprite`
 * below is written `llXxxSprite`, the number, with the label's name
 * kept as the macro's. The values are relocData file 8
 * (MNVSItemSwitch) as tools/export/ssb_spriteexport.py --list reads them off
 * the ROM. The file's other twenty-three sprites are the JP subtitle's
 * and are not exported (mnItemSwitchMakeSubtitle). */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* the decomp's share of unused locals: mnVSItemSwitchFuncRun's
 * `unused`, and the two the US arm of mnItemSwitchMakeSubtitle leaves
 * behind with its body */
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
/* and mnVSItemSwitchFuncRun's stick_range, which the decomp assigns
 * inside the short-circuit of `(button ...) || (stick_range = ..., ...)`:
 * the arm that reads it runs only where the button check was false,
 * which is exactly where the assignment happened (the same shape as
 * src/dc/mnvsoptions.c's) */
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

#define llMNVSItemSwitchLabelVSOptionsSprite    0x009A8
#define llMNVSItemSwitchLabelItemSwitchSprite   0x00B20
#define llMNVSItemSwitchAppearanceNoneSprite    0x00CE8
#define llMNVSItemSwitchAppearanceVeryLowSprite 0x00EA8
#define llMNVSItemSwitchAppearanceLowSprite     0x00F98
#define llMNVSItemSwitchAppearanceMiddleSprite  0x010D0
#define llMNVSItemSwitchAppearanceHighSprite    0x011E8
#define llMNVSItemSwitchAppearanceVeryHighSprite 0x013A8
#define llMNVSItemSwitchToggleOnSprite          0x01488
#define llMNVSItemSwitchToggleOffSprite         0x01568
#define llMNVSItemSwitchToggleSlashSprite       0x01608
#define llMNVSItemSwitchDecalButtonSprite       0x03430
#define llMNVSItemSwitchItemListSprite          0x05E60
#define llMNVSItemSwitchCursorSprite            0x063A8

#define MNVSITEMSWITCH_BANK "mnitemswitch.spr"

// // // // // // // // // // // //
//                               //
//             MACROS            //
//                               //
// // // // // // // // // // // //

/* mnvsitemswitch.c:16-29, verbatim: mn/mndef.h's option-input macros
 * with this scene's change-wait bound in. */
#define mnVSItemSwitchCheckGetOptionButtonInput(is_button, mask) \
mnCommonCheckGetOptionButtonInput(sMNVSItemSwitchOptionChangeWait, is_button, mask)

#define mnVSItemSwitchCheckGetOptionStickInputUD(stick_range, min, b) \
mnCommonCheckGetOptionStickInputUD(sMNVSItemSwitchOptionChangeWait, stick_range, min, b)

#define mnVSItemSwitchCheckGetOptionStickInputLR(stick_range, min, b) \
mnCommonCheckGetOptionStickInputLR(sMNVSItemSwitchOptionChangeWait, stick_range, min, b)

#define mnVSItemSwitchSetOptionChangeWaitP(is_button, stick_range, div) \
mnCommonSetOptionChangeWaitP(sMNVSItemSwitchOptionChangeWait, is_button, stick_range, div)

#define mnVSItemSwitchSetOptionChangeWaitN(is_button, stick_range, div) \
mnCommonSetOptionChangeWaitN(sMNVSItemSwitchOptionChangeWait, is_button, stick_range, div)

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mnvsitemswitch.c:37-55 (0x80133210). Sixteen rows: [0] is the
 * appearance rate, which is not an item and whose entry is a zero the
 * loops start past; [1..15] are the utility items in the order they are
 * listed on screen. The red shell has no row of its own -- the green
 * shell's carries it (mnVSItemSwitchSetItemSettings). */
s32 dMNVSItemSwitchTogglesItemKinds[/* */] =
{
    0,
    nITKindSword,
    nITKindBat,
    nITKindHammer,
    nITKindHarisen,
    nITKindMSBomb,
    nITKindBombHei,
    nITKindNBumper,
    nITKindGShell,
    nITKindMBall,
    nITKindLGun,
    nITKindFFlower,
    nITKindStarRod,
    nITKindTomato,
    nITKindHeart,
    nITKindStar
};

/* mnvsitemswitch.c:58 (0x80133250): { &llMNVSItemSwitchFileID }, the
 * link label's value written out. */
u32 dMNVSItemSwitchFileIDs[/* */] = { 8 };

/* mnvsitemswitch.c:61-69 dMNVSItemSwitchLights1 and
 * dMNVSItemSwitchDisplayList, the lighting the pre-render function
 * would set for the 3D this scene never draws. Dropped with
 * mnVSItemSwitchFuncLights (dMNVSItemSwitchTaskmanSetup says why). */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* mnvsitemswitch.c:80 */
s32 sMNVSItemSwitchOptionSelectID;

/* mnvsitemswitch.c:83. The subtitle's GObj: on the US build it carries
 * nothing (mnItemSwitchMakeSubtitle), and is ejected and remade every
 * time the cursor moves all the same. */
static GObj *sMNVSItemSwitchUnkGObj;

/* mnvsitemswitch.c:85, 88 */
GObj *sMNVSItemSwitchOptionGObjs[(nITKindUtilityEnd - nITKindUtilityStart) + 1];
sb32 sMNVSItemSwitchOptionStatuses[(nITKindUtilityEnd - nITKindUtilityStart) + 1];

/* mnvsitemswitch.c:91, 94, 97, 100 */
static GObj *sMNVSItemSwitchCursorGObj;
static s32 sMNVSItemSwitchOptionChangeWait;
static s32 sMNVSItemSwitchTotalTimeTics;
static s32 sMNVSItemSwitchReturnTic;

/* mnvsitemswitch.c:103 sMNVSItemSwitchStatusBuffer[24], the reloc
 * loader's per-file status records. Dropped with lbRelocInitSetup
 * (mnVSItemSwitchLoadFiles). */

/* mnvsitemswitch.c:106 */
void *sMNVSItemSwitchFiles[ARRAY_COUNT(dMNVSItemSwitchFileIDs)];

/* the bank behind sMNVSItemSwitchFiles */
static SpriteBank sMNVSItemSwitchBanks[ARRAY_COUNT(dMNVSItemSwitchFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mnvsitemswitch.c:113-116 mnVSItemSwitchFuncLights: two GBI commands
 * setting a single light for 3D this scene has none of. Dropped, as
 * every other menu scene's is (src/dc/mnvsmode.c). */

/* mnvsitemswitch.c:119-142 0x80131B24, verbatim: an ON/OFF pair where
 * the live half is red and the dead half grey. The SObj's `next` is the
 * OFF sprite, because mnVSItemSwitchMakeToggle makes them in that
 * order. */
void mnVSItemSwitchSetToggleSpriteColors(GObj *gobj, s32 status)
{
    SObj *sobj = SObjGetStruct(gobj);

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

/* mnvsitemswitch.c:145-174 0x80131B98, verbatim: ON, OFF and the slash
 * between them, three SObjs on the row's one GObj. */
void mnVSItemSwitchMakeToggle(GObj *gobj, f32 pos_x, f32 pos_y)
{
    SObj *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSItemSwitchFiles[0], llMNVSItemSwitchToggleOnSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x;
    sobj->pos.y = pos_y;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSItemSwitchFiles[0], llMNVSItemSwitchToggleOffSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x + 21.0F + 5.0F;
    sobj->pos.y = pos_y;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSItemSwitchFiles[0], llMNVSItemSwitchToggleSlashSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x + 21.0F;
    sobj->pos.y = pos_y;

    sobj->sprite.red = 0x32;
    sobj->sprite.green = 0x32;
    sobj->sprite.blue = 0x32;
}

/* mnvsitemswitch.c:177-191 0x80131CA4. DIVERGES in one place, and it is
 * the same place as the VS options screen's own label
 * (src/dc/mnvsoptions.c mnVSOptionsLabelProcDisplay): the nine RDP
 * commands are one fill quad at the pass's next depth, the grey bar the
 * VS OPTIONS label then draws over. 1-cycle mode, so the lower-right
 * stays exclusive as lbCommonSpriteFillRect wants it. */
void mnVSItemSwitchLabelsProcDisplay(GObj *gobj)
{
    lbCommonSpriteFillRect(79, 34, 310, 39, 0x80, 0x80, 0x80, 0xFF);

    lbCommonClearExternSpriteParams();
    lbCommonDrawSObjAttr(gobj);
}

/* mnvsitemswitch.c:194-230 0x80131DE8, verbatim: VS OPTIONS in gold on
 * the bar, ITEM SWITCH in white beside it. */
void mnVSItemSwitchMakeLabels(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, mnVSItemSwitchLabelsProcDisplay, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSItemSwitchFiles[0], llMNVSItemSwitchLabelVSOptionsSprite));

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

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSItemSwitchFiles[0], llMNVSItemSwitchLabelItemSwitchSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0xFF;
    sobj->sprite.blue = 0xFF;

    sobj->pos.x = 222.0F;
    sobj->pos.y = 30.0F;
}

/* mnvsitemswitch.c:233-245 mnItemSwitchSetSubtitleSpriteColors: the
 * subtitle's three sprites' colours, and the subtitle is JP's. Dropped
 * with the body that calls it. */

/* mnvsitemswitch.c:248-345 0x80131F30. On the JP build this is a framed
 * caption under the list -- the item's name in Japanese, and for the
 * rate row the rate's name as well -- out of twenty-three sprites this
 * port does not export. On the US build the whole body is under
 * `#if defined(REGION_JP)` and what is left is the two lines here: a
 * GObj with nothing on it, kept in sMNVSItemSwitchUnkGObj and ejected
 * and remade on every cursor move. Ported as it stands, because that
 * GObj is real -- it is made, ejected and remade the same number of
 * times as the JP build's, and the eject is the thing overlay reloads
 * exist to keep honest (src/dc/overlay.h). */
void mnItemSwitchMakeSubtitle(s32 option_id)
{
    GObj *gobj;

    (void)option_id;

    sMNVSItemSwitchUnkGObj = gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);

    (void)gobj;
}

/* mnvsitemswitch.c:348-366 0x80131FDC, verbatim: the button legend in
 * the corner, in the brown the whole VS chain uses for it. */
void mnVSItemSwitchMakeDecal(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 6, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 4, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSItemSwitchFiles[0], llMNVSItemSwitchDecalButtonSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x48;
    sobj->sprite.green = 0x2A;
    sobj->sprite.blue = 0x23;

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;
}

/* mnvsitemswitch.c:369-387 0x80132084, verbatim: the sixteen row names,
 * one sprite. */
void mnVSItemSwitchMakeItemList(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSItemSwitchFiles[0], llMNVSItemSwitchItemListSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0xFF;
    sobj->sprite.blue = 0xFF;

    sobj->pos.x = 125.0F;
    sobj->pos.y = 48.0F;
}

/* mnvsitemswitch.c:390-401 0x8013212C, verbatim: the rate's row sits
 * four pixels above where the arithmetic would put it. */
void mnVSItemSwitchSetCursorPosition(GObj *gobj, s32 option_id)
{
    SObj *sobj = SObjGetStruct(gobj);

    sobj->pos.x = 115.0F;

    if (option_id == 0)
    {
        sobj->pos.y = 47.0F;
    }
    else sobj->pos.y = (option_id * 10) + 51;
}

/* mnvsitemswitch.c:404-421 0x80132178, verbatim. */
void mnVSItemSwitchMakeCursor(s32 off_y)
{
    GObj *gobj;
    SObj *sobj;

    sMNVSItemSwitchCursorGObj = gobj = gcMakeGObjSPAfter(0, NULL, 5, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 3, GOBJ_PRIORITY_DEFAULT, ~0);
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSItemSwitchFiles[0], llMNVSItemSwitchCursorSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0xDE;
    sobj->sprite.blue = 0x00;

    mnVSItemSwitchSetCursorPosition(gobj, off_y);
}

/* mnvsitemswitch.c:424-464 0x80132224, verbatim: the rate is a word,
 * one sprite per step, and each step's word is a different width, so
 * the table is six left edges that right-align them. */
void mnVSItemSwitchMakeAppearance(s32 rate)
{
    GObj *gobj;
    SObj *sobj;

    // 0x801332F0
    s32 pos_x[/* */] =
    {
        242,
        240,
        254,
        244,
        252,
        238
    };

    // 0x80133308
    intptr_t rate_offsets[/* */] =
    {
        llMNVSItemSwitchAppearanceNoneSprite,
        llMNVSItemSwitchAppearanceVeryLowSprite,
        llMNVSItemSwitchAppearanceLowSprite,
        llMNVSItemSwitchAppearanceMiddleSprite,
        llMNVSItemSwitchAppearanceHighSprite,
        llMNVSItemSwitchAppearanceVeryHighSprite
    };

    sMNVSItemSwitchOptionGObjs[0] = gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNVSItemSwitchFiles[0], rate_offsets[rate]));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x[rate];
    sobj->pos.y = 49.0F;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;
}

/* mnvsitemswitch.c:467-481 0x80132368, verbatim. The loop bound is the
 * decomp's: two arrays of the same length averaged, which is that
 * length -- IDO's way of writing ARRAY_COUNT here, kept as it stands. */
void mnVSItemSwitchInitToggles(void)
{
    s32 i;
    GObj *gobj;

    mnVSItemSwitchMakeAppearance(sMNVSItemSwitchOptionStatuses[0]);

    for (i = 1; i < (s32)(ARRAY_COUNT(sMNVSItemSwitchOptionGObjs) + ARRAY_COUNT(sMNVSItemSwitchOptionStatuses)) / 2; i++)
    {
        sMNVSItemSwitchOptionGObjs[i] = gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);

        gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
        mnVSItemSwitchMakeToggle(gobj, 244.0F, i * 10 + 54);
        mnVSItemSwitchSetToggleSpriteColors(gobj, sMNVSItemSwitchOptionStatuses[i]);
    }
}

/* mnvsitemswitch.c:484-505 0x80132468, verbatim. */
void mnVSItemSwitchMakeCursorCamera(void)
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
            30,
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

/* mnvsitemswitch.c:508-529 0x80132508, verbatim: the list, the labels,
 * the toggles and the rate all draw on this one. */
void mnVSItemSwitchMakeLabelsCamera(void)
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

/* mnvsitemswitch.c:532-536 func_ovl21_801325A8, an empty function
 * nothing calls. Not ported; nothing to port. */

/* mnvsitemswitch.c:539-560 0x801325B0, verbatim. */
void mnVSItemSwitchMakeDecalCamera(void)
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

/* mnvsitemswitch.c:563-577 0x80132650, verbatim: the battle state's
 * mask read out into fifteen row states. */
void mnVSItemSwitchGetItemSettings(void)
{
    s32 i;

    sMNVSItemSwitchOptionStatuses[0] = gSCManagerTransferBattleState.item_appearance_rate;

    for (i = 1; i < (s32)(ARRAY_COUNT(dMNVSItemSwitchTogglesItemKinds) + ARRAY_COUNT(sMNVSItemSwitchOptionStatuses)) / 2; i++)
    {
        if ((1 << dMNVSItemSwitchTogglesItemKinds[i]) & gSCManagerTransferBattleState.item_toggles)
        {
            sMNVSItemSwitchOptionStatuses[i] = nMNOptionTabStatusOn;
        }
        else sMNVSItemSwitchOptionStatuses[i] = nMNOptionTabStatusOff;
    }
}

/* mnvsitemswitch.c:580-603 0x801327B8, verbatim: and back again, with
 * the green shell's row carrying the red shell's bit either way. */
void mnVSItemSwitchSetItemSettings(void)
{
    s32 i;

    gSCManagerTransferBattleState.item_appearance_rate = sMNVSItemSwitchOptionStatuses[0];

    for (i = 1; i < (s32)(ARRAY_COUNT(dMNVSItemSwitchTogglesItemKinds) + ARRAY_COUNT(sMNVSItemSwitchOptionStatuses)) / 2; i++)
    {
        if (sMNVSItemSwitchOptionStatuses[i] != nMNOptionTabStatusOff)
        {
            gSCManagerTransferBattleState.item_toggles |= (1 << dMNVSItemSwitchTogglesItemKinds[i]);

            if (dMNVSItemSwitchTogglesItemKinds[i] == nITKindGShell)
            {
                gSCManagerTransferBattleState.item_toggles |= (1 << nITKindRShell);
            }
        }
        else
        {
            gSCManagerTransferBattleState.item_toggles &= ~(1 << dMNVSItemSwitchTogglesItemKinds[i]);

            if (dMNVSItemSwitchTogglesItemKinds[i] == nITKindGShell)
            {
                gSCManagerTransferBattleState.item_toggles &= ~(1 << nITKindRShell);
            }
        }
    }
}

/* mnvsitemswitch.c:606-615 0x80132948, verbatim. sMNVSItemSwitchReturnTic
 * is the five idle minutes the rest of the VS chain has and this scene
 * sets and never reads -- the decomp's, kept. */
void mnVSItemSwitchInitVars(void)
{
    sMNVSItemSwitchOptionSelectID = 0;

    mnVSItemSwitchGetItemSettings();

    sMNVSItemSwitchOptionChangeWait = 0;
    sMNVSItemSwitchTotalTimeTics = 0;

    sMNVSItemSwitchReturnTic = sMNVSItemSwitchTotalTimeTics + I_MIN_TO_TICS(5);
}

/* mnvsitemswitch.c:618-630 0x80132988, verbatim. Note the bound: this
 * one really is ARRAY_COUNT, so it reads all fifteen toggle rows. */
sb32 mnVSItemSwitchCheckAllTogglesOff(void)
{
    s32 i;

    for (i = 1; i < (s32)ARRAY_COUNT(sMNVSItemSwitchOptionStatuses); i++)
    {
        if (sMNVSItemSwitchOptionStatuses[i] != nMNOptionTabStatusOff)
        {
            return FALSE;
        }
    }
    return TRUE;
}

/* mnvsitemswitch.c:633-645 0x80132A44, verbatim, and the one piece of
 * this file that is not a sprite: everything off means the mask is
 * zero, so nothing spawns at all; anything on adds the four containers
 * -- egg, capsule, barrel, crate -- which have no row of their own and
 * are what the other items come out of. */
void mnVSItemSwitchSetItemToggles(void)
{
    if (mnVSItemSwitchCheckAllTogglesOff() != FALSE)
    {
        gSCManagerTransferBattleState.item_toggles = 0;
    }
    else
    {
        mnVSItemSwitchSetItemSettings();

        gSCManagerTransferBattleState.item_toggles |= ((1 << nITKindEgg) | (1 << nITKindCapsule) | (1 << nITKindTaru) | (1 << nITKindBox));
    }
}

/* mnvsitemswitch.c:648-657 0x80132A94, verbatim: the rate's row is a
 * sprite that changes, so it is remade; a toggle row only recolours. */
void mnVSItemSwitchUpdateOption(s32 option_id, s32 rate)
{
    if (option_id == 0)
    {
        gcEjectGObj(sMNVSItemSwitchOptionGObjs[option_id]);
        mnVSItemSwitchMakeAppearance(rate);
    }
    else mnVSItemSwitchSetToggleSpriteColors(sMNVSItemSwitchOptionGObjs[option_id], rate);
}

/* mnvsitemswitch.c:660-838 0x80132AF0, verbatim. Ten deaf tics, then:
 * B saves and goes back to the VS options; up and down walk the sixteen
 * rows and wrap, with eight extra tics of change-wait spent landing on
 * either end so the wrap is not accidental; left and right set a toggle
 * off and on rather than flipping it, and step the rate through its six
 * values; A flips a toggle and steps the rate. */
void mnVSItemSwitchFuncRun(GObj *gobj)
{
    s32 unused;
    s32 stick_range;
    sb32 is_button;

    (void)gobj;

    sMNVSItemSwitchTotalTimeTics++;

    if (sMNVSItemSwitchTotalTimeTics >= 10)
    {
        if (sMNVSItemSwitchOptionChangeWait != 0)
        {
            sMNVSItemSwitchOptionChangeWait--;
        }
        if
        (
            (scSubsysControllerGetPlayerStickInRangeLR(-20, 20) != FALSE)                                         &&
            (scSubsysControllerGetPlayerStickInRangeUD(-20, 20) != FALSE)                                         &&
            (scSubsysControllerGetPlayerHoldButtons(U_JPAD | R_JPAD | R_TRIG | U_CBUTTONS | R_CBUTTONS) == FALSE) &&
            (scSubsysControllerGetPlayerHoldButtons(D_JPAD | L_JPAD | L_TRIG | D_CBUTTONS | L_CBUTTONS) == FALSE)
        )
        {
            sMNVSItemSwitchOptionChangeWait = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(B_BUTTON) != FALSE)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindVSOptions;

            mnVSItemSwitchSetItemToggles();
            syTaskmanSetLoadScene();
        }
        if
        (
            mnVSItemSwitchCheckGetOptionButtonInput(is_button, U_JPAD | U_CBUTTONS) ||
            mnVSItemSwitchCheckGetOptionStickInputUD(stick_range, 20, 1)
        )
        {
            func_800269C0_275C0(nSYAudioFGMMenuScroll2);

            mnVSItemSwitchSetOptionChangeWaitP(is_button, stick_range, 7);

            if (sMNVSItemSwitchOptionSelectID == 0)
            {
                sMNVSItemSwitchOptionSelectID = (ARRAY_COUNT(sMNVSItemSwitchOptionGObjs) - 1);
            }
            else sMNVSItemSwitchOptionSelectID--;

            if (sMNVSItemSwitchOptionSelectID == 0)
            {
                sMNVSItemSwitchOptionChangeWait += 8;
            }
            mnVSItemSwitchSetCursorPosition(sMNVSItemSwitchCursorGObj, sMNVSItemSwitchOptionSelectID);

            gcEjectGObj(sMNVSItemSwitchUnkGObj);
            mnItemSwitchMakeSubtitle(sMNVSItemSwitchOptionSelectID);
        }
        if
        (
            mnVSItemSwitchCheckGetOptionButtonInput(is_button, D_JPAD | D_CBUTTONS) ||
            mnVSItemSwitchCheckGetOptionStickInputUD(stick_range, -20, 0)
        )
        {
            func_800269C0_275C0(nSYAudioFGMMenuScroll2);

            mnVSItemSwitchSetOptionChangeWaitN(is_button, stick_range, 7);

            if (sMNVSItemSwitchOptionSelectID == (ARRAY_COUNT(sMNVSItemSwitchOptionGObjs) - 1))
            {
                sMNVSItemSwitchOptionSelectID = 0;
            }
            else sMNVSItemSwitchOptionSelectID++;

            if (sMNVSItemSwitchOptionSelectID == (ARRAY_COUNT(sMNVSItemSwitchOptionGObjs) - 1))
            {
                sMNVSItemSwitchOptionChangeWait += 8;
            }
            mnVSItemSwitchSetCursorPosition(sMNVSItemSwitchCursorGObj, sMNVSItemSwitchOptionSelectID);

            gcEjectGObj(sMNVSItemSwitchUnkGObj);
            mnItemSwitchMakeSubtitle(sMNVSItemSwitchOptionSelectID);
        }
        if
        (
            mnVSItemSwitchCheckGetOptionButtonInput(is_button, L_JPAD | L_TRIG | L_CBUTTONS) ||
            mnVSItemSwitchCheckGetOptionStickInputLR(stick_range, -20, 0)
        )
        {
            if (sMNVSItemSwitchOptionSelectID == 0)
            {
                func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                if (sMNVSItemSwitchOptionStatuses[0] > nSCBattleItemSwitchNone)
                {
                    sMNVSItemSwitchOptionStatuses[0]--;
                }
                else sMNVSItemSwitchOptionStatuses[0] = nSCBattleItemSwitchVeryHigh;

                mnVSItemSwitchUpdateOption(sMNVSItemSwitchOptionSelectID, sMNVSItemSwitchOptionStatuses[0]);

                gcEjectGObj(sMNVSItemSwitchUnkGObj);
                mnItemSwitchMakeSubtitle(sMNVSItemSwitchOptionSelectID);
            }
            else if (sMNVSItemSwitchOptionStatuses[sMNVSItemSwitchOptionSelectID] == nMNOptionTabStatusOff)
            {
                func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                sMNVSItemSwitchOptionStatuses[sMNVSItemSwitchOptionSelectID] = nMNOptionTabStatusOn;

                mnVSItemSwitchUpdateOption(sMNVSItemSwitchOptionSelectID, sMNVSItemSwitchOptionStatuses[sMNVSItemSwitchOptionSelectID]);
            }
            mnVSItemSwitchSetOptionChangeWaitN(is_button, stick_range, 7);
        }
        if
        (
            mnVSItemSwitchCheckGetOptionButtonInput(is_button, R_JPAD | R_TRIG | R_CBUTTONS) ||
            mnVSItemSwitchCheckGetOptionStickInputLR(stick_range, 20, 1)
        )
        {
            if (sMNVSItemSwitchOptionSelectID == 0)
            {
                func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                if (sMNVSItemSwitchOptionStatuses[0] < nSCBattleItemSwitchVeryHigh)
                {
                    sMNVSItemSwitchOptionStatuses[0]++;
                }
                else sMNVSItemSwitchOptionStatuses[0] = nSCBattleItemSwitchNone;

                mnVSItemSwitchUpdateOption(sMNVSItemSwitchOptionSelectID, sMNVSItemSwitchOptionStatuses[0]);

                gcEjectGObj(sMNVSItemSwitchUnkGObj);
                mnItemSwitchMakeSubtitle(sMNVSItemSwitchOptionSelectID);
            }
            else if (sMNVSItemSwitchOptionStatuses[sMNVSItemSwitchOptionSelectID] == nMNOptionTabStatusOn)
            {
                func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                sMNVSItemSwitchOptionStatuses[sMNVSItemSwitchOptionSelectID] = nMNOptionTabStatusOff;

                mnVSItemSwitchUpdateOption(sMNVSItemSwitchOptionSelectID, sMNVSItemSwitchOptionStatuses[sMNVSItemSwitchOptionSelectID]);
            }
            mnVSItemSwitchSetOptionChangeWaitP(is_button, stick_range, 7);
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON) != FALSE)
        {
            if (sMNVSItemSwitchOptionSelectID == 0)
            {
                func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                if (sMNVSItemSwitchOptionStatuses[0] < nSCBattleItemSwitchVeryHigh)
                {
                    sMNVSItemSwitchOptionStatuses[0]++;
                }
                else sMNVSItemSwitchOptionStatuses[0] = nSCBattleItemSwitchNone;

                mnVSItemSwitchUpdateOption(sMNVSItemSwitchOptionSelectID, sMNVSItemSwitchOptionStatuses[0]);

                gcEjectGObj(sMNVSItemSwitchUnkGObj);
                mnItemSwitchMakeSubtitle(sMNVSItemSwitchOptionSelectID);
            }
            else
            {
                func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                if (sMNVSItemSwitchOptionStatuses[sMNVSItemSwitchOptionSelectID] == nMNOptionTabStatusOff)
                {
                    sMNVSItemSwitchOptionStatuses[sMNVSItemSwitchOptionSelectID] = nMNOptionTabStatusOn;
                }
                else sMNVSItemSwitchOptionStatuses[sMNVSItemSwitchOptionSelectID] = nMNOptionTabStatusOff;

                mnVSItemSwitchUpdateOption(sMNVSItemSwitchOptionSelectID, sMNVSItemSwitchOptionStatuses[sMNVSItemSwitchOptionSelectID]);
            }
        }
    }
}

/* mnvsitemswitch.c:841-852 lbRelocInitSetup and lbRelocLoadFilesListed,
 * as every other menu scene has them (src/dc/mnvsmode.c): one sprite
 * bank stands in for the one relocData file. */
static void mnVSItemSwitchLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMNVSItemSwitchFileIDs)] =
    {
        MNVSITEMSWITCH_BANK
    };
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(dMNVSItemSwitchFileIDs); i++)
    {
        if (sprite_bank_load(&sMNVSItemSwitchBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mnVSItemSwitch: no bank for file %d (%s)\n",
                          (int)dMNVSItemSwitchFileIDs[i], paths[i]);
            sMNVSItemSwitchFiles[i] = NULL;
            continue;
        }
        sMNVSItemSwitchFiles[i] = &sMNVSItemSwitchBanks[i];
    }
}

/* mnvsitemswitch.c:841-863 0x80133090, in the game's order.
 *
 * DIVERGES: the LBRelocSetup block is mnVSItemSwitchLoadFiles above,
 * and gcMakeDefaultCameraGObj -- the black clear camera, which this
 * scene asks for with COBJ_FLAG_FILLCOLOR | COBJ_FLAG_ZBUFFER -- is
 * dropped because the PVR clears its own framebuffer
 * (src/dc/taskman.c). */
void mnVSItemSwitchFuncStart(void)
{
    mnVSItemSwitchLoadFiles();

    gcMakeGObjSPAfter(0, mnVSItemSwitchFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

    mnVSItemSwitchInitVars();
    mnVSItemSwitchMakeCursorCamera();
    mnVSItemSwitchMakeLabelsCamera();
    mnVSItemSwitchMakeDecalCamera();
    mnVSItemSwitchMakeLabels();
    mnVSItemSwitchMakeDecal();
    mnVSItemSwitchMakeItemList();
    mnVSItemSwitchMakeCursor(sMNVSItemSwitchOptionSelectID);
    mnVSItemSwitchInitToggles();

    mnItemSwitchMakeSubtitle(sMNVSItemSwitchOptionSelectID);
}

/* mnvsitemswitch.c:866 dMNVSItemSwitchVideoSetup is the N64's video
 * mode: see mnVSItemSwitchStartScene. */

/* mnvsitemswitch.c:869-909 (0x8013333C). The pool counts are the
 * game's; the eight GObjThreads are this scene's own and every other
 * menu scene's are zero. DIVERGES as the rest: the arena is the port's
 * region, the draw is the scene manager's, and mnVSItemSwitchFuncLights
 * is dropped. */
SYTaskmanSetup dMNVSItemSwitchTaskmanSetup =
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

    8,                                  // Number of GObjThreads
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
    sizeof(CObj),                       // CObj size

    mnVSItemSwitchFuncStart             // Task start function
};

/* The bzero arm of syDmaLoadOverlay for overlay 21, this file:
 * src/dc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnVSItemSwitchOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNVSItemSwitchOptionSelectID);
    OVERLAY_CLEAR(sMNVSItemSwitchUnkGObj);
    OVERLAY_CLEAR(sMNVSItemSwitchOptionGObjs);
    OVERLAY_CLEAR(sMNVSItemSwitchOptionStatuses);
    OVERLAY_CLEAR(sMNVSItemSwitchCursorGObj);
    OVERLAY_CLEAR(sMNVSItemSwitchOptionChangeWait);
    OVERLAY_CLEAR(sMNVSItemSwitchTotalTimeTics);
    OVERLAY_CLEAR(sMNVSItemSwitchReturnTic);
    OVERLAY_CLEAR(sMNVSItemSwitchFiles);
    OVERLAY_CLEAR(sMNVSItemSwitchBanks);
}

/* mnvsitemswitch.c:912-918 0x801331B0. DIVERGES as every other menu
 * scene's: syVideoInit and the zbuffer are the N64's video mode, set
 * once at boot here, and the arena_size line is the link map. What is
 * left is the last line. */
void mnVSItemSwitchStartScene(void)
{
    syTaskmanStartTask(&dMNVSItemSwitchTaskmanSetup);
}
