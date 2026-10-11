/* mnmodeselect.c -- see mnmodeselect.h. Every function is
 * mn/mncommon/mnmodeselect.c's by name and body, REGION_US arms; the
 * line numbers are the decomp's. */
#include "mnmodeselect.h"
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

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/ifcommon.c does the same). Each `&llXxxSprite` below is
 * written `llXxxSprite`, the number, with the label's name kept as the
 * macro's. The values are relocData files 0 (MNCommon) and 1 (MNMain)
 * as tools/export/ssb_spriteexport.py --list reads them off the ROM. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* mnModeSelectFuncRun's two stick conditions mix && into || without
 * brackets; the decomp builds with -Wno-parentheses and the text stays
 * as it is */
#pragma GCC diagnostic ignored "-Wparentheses"

#define llMNCommonSmashBrosCollageSprite 0x18000

#define llMNMainControllerIconSprite     0x1990
#define llMNMainConsoleIconSprite        0x2520
#define llMNMainDataIconSprite           0x30B0
#define llMNMainModeSelectTextSprite     0x40F0
#define llMNMainSettingsIconSprite       0x4C80
#define llMNMain1PModeTextSprite         0x5570
#define llMNMainVsModeTextSprite         0x57E0
#define llMNMainDataTextSprite           0x5980
#define llMNMainControllerIconDarkSprite 0x6048
#define llMNMainConsoleIconDarkSprite    0x6708
#define llMNMainDataIconDarkSprite       0x6DC8
#define llMNMainDecalBarEdgeSprite       0x72E8
#define llMNMainSmashLogoSprite          0x7AA8
#define llMNMainDecalBarMiddleSprite     0x7C38
#define llMNMainSettingsIconDarkSprite   0x82F8
#define llMNMainOptionTextSprite         0x84F8

/* The banks the two files became. The host test reads the same files
 * from the game's romdisk directory. */
#define MNMODESELECT_BANK_COMMON "mncommon.spr"
#define MNMODESELECT_BANK_MAIN   "mnmain.spr"

// // // // // // // // // // // //
//                               //
//             MACROS            //
//                               //
// // // // // // // // // // // //

#define mnModeSelectCheckGetOptionButtonInput(is_button, mask) \
mnCommonCheckGetOptionButtonInput(sMNModeSelectOptionChangeWait, is_button, mask)

#define mnModeSelectCheckGetOptionStickInputUD(stick_range, min, b) \
mnCommonCheckGetOptionStickInputUD(sMNModeSelectOptionChangeWait, stick_range, min, b)

#define mnModeSelectCheckGetOptionStickInputLR(stick_range, min, b) \
mnCommonCheckGetOptionStickInputLR(sMNModeSelectOptionChangeWait, stick_range, min, b)

#define mnModeSelectSetOptionChangeWaitP(is_button, stick_range, div) \
mnCommonSetOptionChangeWaitP(sMNModeSelectOptionChangeWait, is_button, stick_range, div)

#define mnModeSelectSetOptionChangeWaitN(is_button, stick_range, div) \
mnCommonSetOptionChangeWaitN(sMNModeSelectOptionChangeWait, is_button, stick_range, div)

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mnmodeselect.c:37 (0x80132B90): { &llMNCommonFileID, &llMNMainFileID },
 * the two link labels' values written out. */
u32 dMNModeSelectFileIDs[/* */] = { 0, 1 };

/* mnmodeselect.c:52-94 (0x80132BF4). The pool counts are the game's, all
 * zero: a menu scene takes its objects straight from the scene heap.
 * DIVERGES as the title's (src/dc/mntitle.c): the arena is the port's
 * region, the draw is the scene manager's, and mnModeSelectFuncLights --
 * a gSPDisplayList of two lighting commands, for 3D this scene never
 * draws -- is dropped. */
SYTaskmanSetup dMNModeSelectTaskmanSetup =
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

    mnModeSelectFuncStart               // Task start function
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x80132C88
static s32 sMNModeSelectOption;

// 0x80132C8C
static GObj *sMNModeSelectOption1PModeGObj;

// 0x80132C90
static GObj *sMNModeSelectOptionVSModeGObj;

// 0x80132C94
static GObj *sMNModeSelectOptionOptionGObj;

// 0x80132C98
static GObj *sMNModeSelectOptionDataGObj;

// 0x80132C9C
static s32 sMNModeSelectOptionChangeWait;

// 0x80132CA0
static s32 sMNModeSelectTotalTimeTics;

// 0x80132CA4
static s32 sMNModeSelectReturnTic;

/* 0x80132CA8 sMNModeSelectStatusBuffer[24], the reloc status buffer, has
 * no port; 0x80132D68 is the files' bases, here the two banks. */
static SpriteBank sMNModeSelectBanks[ARRAY_COUNT(dMNModeSelectFileIDs)];
void *sMNModeSelectFiles[ARRAY_COUNT(dMNModeSelectFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mnmodeselect.c:150-231 0x80131B24, REGION_US arms. */
void mnModeSelectMake1PMode(void)
{
    GObj *gobj;
    SObj *sobj;

    sMNModeSelectOption1PModeGObj = gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    if (sMNModeSelectOption == nMNModeSelectOption1PMode)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainControllerIconSprite));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->sprite.red = 0xFF;
        sobj->sprite.green = 0xFF;
        sobj->sprite.blue = 0xFF;

        sobj->envcolor.r = 0x00;
        sobj->envcolor.g = 0x00;
        sobj->envcolor.b = 0x00;

        sobj->pos.x = 169.0F;
        sobj->pos.y = 27.0F;
    }
    else
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainControllerIconDarkSprite));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->sprite.red = 0x96;
        sobj->sprite.green = 0x96;
        sobj->sprite.blue = 0x96;

        sobj->pos.x = 169.0F;
        sobj->pos.y = 27.0F;
    }
}

/* mnmodeselect.c:234-315 0x80131C44, REGION_US arms. */
void mnModeSelectMakeVSMode(void)
{
    GObj *gobj;
    SObj *sobj;

    sMNModeSelectOptionVSModeGObj = gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    if (sMNModeSelectOption == nMNModeSelectOptionVSMode)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainConsoleIconSprite));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->sprite.red = 0xFF;
        sobj->sprite.green = 0xFF;
        sobj->sprite.blue = 0xFF;

        sobj->envcolor.r = 0x00;
        sobj->envcolor.g = 0x00;
        sobj->envcolor.b = 0x00;

        sobj->pos.x = 128.0F;
        sobj->pos.y = 64.0F;
    }
    else
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainConsoleIconDarkSprite));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->sprite.red = 0x96;
        sobj->sprite.green = 0x96;
        sobj->sprite.blue = 0x96;

        sobj->pos.x = 128.0F;
        sobj->pos.y = 64.0F;
    }
}

/* mnmodeselect.c:318-399 0x80131D68, REGION_US arms. */
void mnModeSelectMakeOption(void)
{
    GObj *gobj;
    SObj *sobj;

    sMNModeSelectOptionOptionGObj = gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    if (sMNModeSelectOption == nMNModeSelectOptionOption)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainSettingsIconSprite));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->sprite.red = 0xFF;
        sobj->sprite.green = 0xFF;
        sobj->sprite.blue = 0xFF;

        sobj->envcolor.r = 0x00;
        sobj->envcolor.g = 0x00;
        sobj->envcolor.b = 0x00;

        sobj->pos.x = 87.0F;
        sobj->pos.y = 101.0F;
    }
    else
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainSettingsIconDarkSprite));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->sprite.red = 0x96;
        sobj->sprite.green = 0x96;
        sobj->sprite.blue = 0x96;

        sobj->pos.x = 87.0F;
        sobj->pos.y = 101.0F;
    }
}

/* mnmodeselect.c:402-483 0x80131E8C, REGION_US arms. */
void mnModeSelectMakeData(void)
{
    GObj *gobj;
    SObj *sobj;

    sMNModeSelectOptionDataGObj = gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    if (sMNModeSelectOption == nMNModeSelectOptionData)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainDataIconSprite));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->sprite.red = 0xFF;
        sobj->sprite.green = 0xFF;
        sobj->sprite.blue = 0xFF;

        sobj->envcolor.r = 0x00;
        sobj->envcolor.g = 0x00;
        sobj->envcolor.b = 0x00;

        sobj->pos.x = 46.0F;
        sobj->pos.y = 138.0F;
    }
    else
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainDataIconDarkSprite));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->sprite.red = 0x96;
        sobj->sprite.green = 0x96;
        sobj->sprite.blue = 0x96;

        sobj->pos.x = 46.0F;
        sobj->pos.y = 138.0F;
    }
}

/* mnmodeselect.c:486-538 0x80131FB0, verbatim. */
void mnModeSelectMakeLabels(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMain1PModeTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 224.0F;
    sobj->pos.y = 52.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainVsModeTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 183.0F;
    sobj->pos.y = 89.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainOptionTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 142.0F;
    sobj->pos.y = 126.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainDataTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 102.0F;
    sobj->pos.y = 163.0F;
}

/* mnmodeselect.c:541-618 0x80132168, verbatim. The bar's middle is the
 * game's one tiled sprite so far: a 16-pixel tile drawn 96 wide with
 * the s coordinate wrapping at 2^masks. */
void mnModeSelectMakeDecals(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 0, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[0], llMNCommonSmashBrosCollageSprite));

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainDecalBarMiddleSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x08;
    sobj->sprite.green = 0x33;
    sobj->sprite.blue = 0x65;

    sobj->cms = 0;
    sobj->cmt = 0;

    sobj->masks = 4;
    sobj->maskt = 0;

    sobj->lrs = 96;
    sobj->lrt = 38;

    sobj->pos.x = 0.0f;
    sobj->pos.y = 37.0f;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainDecalBarEdgeSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x08;
    sobj->sprite.green = 0x33;
    sobj->sprite.blue = 0x65;

    sobj->pos.x = 96.0F;
    sobj->pos.y = 37.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainModeSelectTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->envcolor.r = 0x00;
    sobj->envcolor.g = 0x00;
    sobj->envcolor.b = 0x00;

    sobj->sprite.red = 0x3C;
    sobj->sprite.green = 0x73;
    sobj->sprite.blue = 0xB4;

    sobj->pos.x = 28.0F;
    sobj->pos.y = 27.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNModeSelectFiles[1], llMNMainSmashLogoSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x08;
    sobj->sprite.green = 0x33;
    sobj->sprite.blue = 0x65;

    sobj->pos.x = 226.0F;
    sobj->pos.y = 137.0F;
}

/* mnmodeselect.c:621-642 0x80132398, verbatim. */
void mnModeSelectMakeLabelsCamera(void)
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

/* mnmodeselect.c:645-666 0x80132438, verbatim. */
void mnModeSelectMakeDecalsCamera(void)
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

/* mnmodeselect.c:669-675 0x801324D8, verbatim. */
void mnModeSelectMakeOptions(void)
{
    mnModeSelectMake1PMode();
    mnModeSelectMakeVSMode();
    mnModeSelectMakeOption();
    mnModeSelectMakeData();
}

/* mnmodeselect.c:678-684 0x80132510, verbatim. */
void mnModeSelectEjectOptions(void)
{
    gcEjectGObj(sMNModeSelectOption1PModeGObj);
    gcEjectGObj(sMNModeSelectOptionVSModeGObj);
    gcEjectGObj(sMNModeSelectOptionOptionGObj);
    gcEjectGObj(sMNModeSelectOptionDataGObj);
}

/* mnmodeselect.c:687-716 0x80132558, verbatim. Where the cursor starts:
 * on the option the previous scene was, so backing out of a mode lands
 * on it. */
void mnModeSelectInitVars(void)
{
    switch (gSCManagerSceneData.scene_prev)
    {
    default:
        sMNModeSelectOption = nMNModeSelectOption1PMode;
        break;

    case nSCKind1PMode:
        sMNModeSelectOption = nMNModeSelectOption1PMode;
        break;

    case nSCKindVSMode:
        sMNModeSelectOption = nMNModeSelectOptionVSMode;
        break;

    case nSCKindOption:
        sMNModeSelectOption = nMNModeSelectOptionOption;
        break;

    case nSCKindData:
        sMNModeSelectOption = nMNModeSelectOptionData;
        break;
    }
    sMNModeSelectOptionChangeWait = 0;

    sMNModeSelectTotalTimeTics = 0;
    sMNModeSelectReturnTic = sMNModeSelectTotalTimeTics + I_MIN_TO_TICS(5);
}

/* mnmodeselect.c:719-866 0x801325E8, verbatim. Ten tics of deafness on
 * the way in, then: five idle minutes or B go back to the title, A or
 * START take the option, and the stick (past 20, with a pause scaled to
 * how far it is pushed) or the pad and C buttons (a pause of 12) move
 * the choice, wrapping with 8 tics more at either end. */
void mnModeSelectFuncRun(GObj *gobj)
{
    s32 stick_range;
    sb32 is_button;

    (void)gobj;

    sMNModeSelectTotalTimeTics++;

    if (sMNModeSelectTotalTimeTics >= 10)
    {
        if (sMNModeSelectTotalTimeTics == sMNModeSelectReturnTic)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            syTaskmanSetLoadScene();
            return;
        }
        if (scSubsysControllerCheckNoInputAll() == FALSE)
        {
            sMNModeSelectReturnTic = sMNModeSelectTotalTimeTics + I_MIN_TO_TICS(5);
        }
        if (sMNModeSelectOptionChangeWait != 0)
        {
            sMNModeSelectOptionChangeWait--;
        }
        if
        (
            (scSubsysControllerGetPlayerStickInRangeLR(-20, 20) != FALSE)                                &&
            (scSubsysControllerGetPlayerStickInRangeUD(-20, 20) != FALSE)                                &&
            (scSubsysControllerGetPlayerHoldButtons(U_JPAD | R_JPAD | U_CBUTTONS | R_CBUTTONS) == FALSE) &&
            (scSubsysControllerGetPlayerHoldButtons(D_JPAD | L_JPAD | D_CBUTTONS | L_CBUTTONS) == FALSE)
        )
        {
            sMNModeSelectOptionChangeWait = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | START_BUTTON) != FALSE)
        {
            switch (sMNModeSelectOption)
            {
            case nMNModeSelectOption1PMode:
                func_800269C0_275C0(nSYAudioFGMMenuSelect);

                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKind1PMode;

                syTaskmanSetLoadScene();
                return;

            case nMNModeSelectOptionVSMode:
                func_800269C0_275C0(nSYAudioFGMMenuSelect);

                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKindVSMode;

                syTaskmanSetLoadScene();
                return;

            case nMNModeSelectOptionOption:
                func_800269C0_275C0(nSYAudioFGMMenuSelect);

                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKindOption;

                syTaskmanSetLoadScene();
                return;

            case nMNModeSelectOptionData:
                func_800269C0_275C0(nSYAudioFGMMenuSelect);

                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKindData;

                syTaskmanSetLoadScene();
                return;
            }
        }
        else
        {
            if (scSubsysControllerGetPlayerTapButtons(B_BUTTON) != FALSE)
            {
                syAudioStopBGMAll();

                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKindTitle;

                syTaskmanSetLoadScene();
            }
            if
            (
                (
                    (scSubsysControllerGetPlayerStickUD(20, 1) <= 0) ||
                    (scSubsysControllerGetPlayerStickLR(-20, 0) >= 0)
                )
                &&
                (
                    (scSubsysControllerGetPlayerStickUD(-20, 0) >= 0) ||
                    (scSubsysControllerGetPlayerStickLR(20, 1) <= 0)
                )
            )
            {
                if
                (
                    mnModeSelectCheckGetOptionButtonInput(is_button, U_JPAD | R_JPAD | U_CBUTTONS | R_CBUTTONS) ||
                    (sMNModeSelectOptionChangeWait == 0)
                    &&
                    (
                        ((stick_range) = scSubsysControllerGetPlayerStickUD(20, 1), stick_range != 0) ||
                        ((stick_range) = scSubsysControllerGetPlayerStickLR(20, 1), stick_range != 0)
                    )
                )
                {
                    func_800269C0_275C0(nSYAudioFGMMenuScroll2);

                    mnModeSelectSetOptionChangeWaitP(is_button, stick_range, 7);

                    if (sMNModeSelectOption == nMNModeSelectOptionStart)
                    {
                        sMNModeSelectOption = nMNModeSelectOptionEnd;
                    }
                    else sMNModeSelectOption--;

                    if (sMNModeSelectOption == nMNModeSelectOptionStart)
                    {
                        sMNModeSelectOptionChangeWait += 8;
                    }
                    mnModeSelectEjectOptions();
                    mnModeSelectMakeOptions();
                }
                if
                (
                    mnModeSelectCheckGetOptionButtonInput(is_button, D_JPAD | L_JPAD | D_CBUTTONS | L_CBUTTONS) ||
                    (sMNModeSelectOptionChangeWait == 0)
                    &&
                    (
                        ((stick_range) = scSubsysControllerGetPlayerStickUD(-20, 0), stick_range != 0) ||
                        ((stick_range) = scSubsysControllerGetPlayerStickLR(-20, 0), stick_range != 0)
                    )
                )
                {
                    func_800269C0_275C0(nSYAudioFGMMenuScroll2);

                    mnModeSelectSetOptionChangeWaitN(is_button, stick_range, 7);

                    if (sMNModeSelectOption == nMNModeSelectOptionEnd)
                    {
                        sMNModeSelectOption = nMNModeSelectOptionStart;
                    }
                    else sMNModeSelectOption++;

                    if (sMNModeSelectOption == nMNModeSelectOptionEnd)
                    {
                        sMNModeSelectOptionChangeWait += 8;
                    }
                    mnModeSelectEjectOptions();
                    mnModeSelectMakeOptions();
                }
            }
        }
    }
}

/* mnmodeselect.c:869-883: lbRelocInitSetup and lbRelocLoadFilesListed
 * over dMNModeSelectFileIDs; here, the banks. A bank that is not there
 * leaves its file NULL and sprite_bank_get says so per sprite. */
void mnModeSelectLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMNModeSelectFileIDs)] =
    {
        MNMODESELECT_BANK_COMMON,
        MNMODESELECT_BANK_MAIN
    };
    s32 i;

    for (i = 0; i < ARRAY_COUNT(dMNModeSelectFileIDs); i++)
    {
        if (sprite_bank_load(&sMNModeSelectBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mnModeSelect: no bank for file %d (%s)\n",
                          (int)dMNModeSelectFileIDs[i], paths[i]);
            sMNModeSelectFiles[i] = NULL;
            continue;
        }
        sMNModeSelectFiles[i] = &sMNModeSelectBanks[i];
    }
}

/* mnmodeselect.c:869-900 0x80132A0C, in the game's order. DIVERGES: the
 * clear camera (gcMakeDefaultCameraGObj on link 0, DL priority 100, a
 * black fill with the z-buffer) is the frame clear, which the PVR does
 * itself -- the same cut as the title's mnTitleMakeCameras. */
void mnModeSelectFuncStart(void)
{
    mnModeSelectLoadFiles();
    gcMakeGObjSPAfter(0, mnModeSelectFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

    mnModeSelectInitVars();
    mnModeSelectMakeDecalsCamera();
    mnModeSelectMakeLabelsCamera();
    mnModeSelectMakeDecals();
    mnModeSelectMakeOptions();
    mnModeSelectMakeLabels();

    if
    (
        (gSCManagerSceneData.scene_prev != nSCKind1PMode) &&
        (gSCManagerSceneData.scene_prev != nSCKindVSMode) &&
        (gSCManagerSceneData.scene_prev != nSCKindOption) &&
        (gSCManagerSceneData.scene_prev != nSCKindData)
    )
    {
        syAudioPlayBGM(0, nSYAudioBGMModeSelect);
    }
}

/* The bzero arm of syDmaLoadOverlay for overlay 17, this
 * file: sc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnModeSelectOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNModeSelectOption);
    OVERLAY_CLEAR(sMNModeSelectOption1PModeGObj);
    OVERLAY_CLEAR(sMNModeSelectOptionVSModeGObj);
    OVERLAY_CLEAR(sMNModeSelectOptionOptionGObj);
    OVERLAY_CLEAR(sMNModeSelectOptionDataGObj);
    OVERLAY_CLEAR(sMNModeSelectOptionChangeWait);
    OVERLAY_CLEAR(sMNModeSelectTotalTimeTics);
    OVERLAY_CLEAR(sMNModeSelectReturnTic);
    OVERLAY_CLEAR(sMNModeSelectBanks);
    OVERLAY_CLEAR(sMNModeSelectFiles);
}

/* mnmodeselect.c:903-910 0x80132B34. DIVERGES as the title's: syVideoInit
 * and the zbuffer are the N64's video mode, set once at boot here, and
 * the arena_size line is the link map. What is left is the last line. */
void mnModeSelectStartScene(void)
{
    syTaskmanStartTask(&dMNModeSelectTaskmanSetup);
}
