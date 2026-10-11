/* mn1pmode.c -- see mn1pmode.h. Every function is
 * mn/mn1pmode/mn1pmode.c's by name and body, REGION_US arms; the
 * line numbers are the decomp's. */
#include "mn1pmode.h"
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
 * offset (src/dc/mnmodeselect.c does the same). Each `&llXxxSprite`
 * below is written `llXxxSprite`, the number, with the label's name
 * kept as the macro's. The values are relocData files 0 (MNCommon) and
 * 2 (MN1P) as src/dc/decomp/reloc_data.us.h reads them off the ROM. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* mn1PModeFuncRun's two stick conditions mix && into || without
 * brackets; the decomp builds with -Wno-parentheses and the text stays
 * as it is */
#pragma GCC diagnostic ignored "-Wparentheses"

#define llMNCommonOptionTabLeftSprite    0x1e8
#define llMNCommonOptionTabMiddleSprite  0x330
#define llMNCommonOptionTabRightSprite   0x568
#define llMNCommonDecalPaperSprite       0x2a30
#define llMNCommonSmashLogoSprite        0x31f8
#define llMNCommonGameModeTextSprite     0xd240
#define llMNCommonSmashBrosCollageSprite 0x18000

#define llMN1POptionTabSprite            0x1108
#define llMN1P1PGameTextSprite           0x2a28
#define llMN1PControllerIconDarkSprite   0x50f8
#define llMN1P1PTextSprite               0x5338
#define llMN1PTrainingModeTextSprite     0x5ac8
#define llMN1PBonus1PracticeTextSprite   0x5f28
#define llMN1PBonus2PracticeTextSprite   0x6388

/* The banks the two files became. */
#define MN1PMODE_BANK_COMMON "mncommon.spr"
#define MN1PMODE_BANK_1P     "mn1p.spr"

// // // // // // // // // // // //
//                               //
//             MACROS            //
//                               //
// // // // // // // // // // // //

#define mn1PModeCheckGetOptionButtonInput(is_button, mask) \
mnCommonCheckGetOptionButtonInput(sMN1PModeOptionChangeWait, is_button, mask)

#define mn1PModeCheckGetOptionStickInputUD(stick_range, min, b) \
mnCommonCheckGetOptionStickInputUD(sMN1PModeOptionChangeWait, stick_range, min, b)

#define mn1PModeCheckGetOptionStickInputLR(stick_range, min, b) \
mnCommonCheckGetOptionStickInputLR(sMN1PModeOptionChangeWait, stick_range, min, b)

#define mn1PModeSetOptionChangeWaitP(is_button, stick_range, div) \
mnCommonSetOptionChangeWaitP(sMN1PModeOptionChangeWait, is_button, stick_range, div)

#define mn1PModeSetOptionChangeWaitN(is_button, stick_range, div) \
mnCommonSetOptionChangeWaitN(sMN1PModeOptionChangeWait, is_button, stick_range, div)

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mn1pmode.c:39 (0x80133080): { &llMNCommonFileID, &llMN1PFileID },
 * the two link labels' values written out. */
u32 dMN1PModeFileIDs[/* */] = { 0, 2 };

/* mn1pmode.c:835-877 (0x8013310C). The pool counts are the game's, all
 * zero: a menu scene takes its objects straight from the scene heap.
 * DIVERGES as the mode select's (src/dc/mnmodeselect.c): the arena is
 * the port's region, the draw is the scene manager's, and
 * mn1PModeFuncLights -- a gSPDisplayList of two lighting commands, for
 * 3D this scene never draws -- is dropped. */
SYTaskmanSetup dMN1PModeTaskmanSetup =
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

    mn1PModeFuncStart                   // Task start function
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x801331A0
GObj *sMN1PModeOptionGObjs[nMN1PModeOptionEnumCount];

// 0x801331B8
s32 sMN1PModeOption;

// 0x801331BC
GObj *sMN1PModeSubtitleGObj;

// 0x801331C0
sb32 sMN1PModeIsProceedScene;

// 0x801331C4
s32 sMN1PModeOptionChangeWait;

// 0x801331C8
s32 sMN1PModeTotalTimeTics;

// 0x801331CC
s32 sMN1PModeReturnTic;

/* 0x801331D0 sMN1PModeStatusBuffer[24], the reloc status buffer, has no
 * port; 0x80133290 is the files' bases, here the two banks. */
static SpriteBank sMN1PModeBanks[ARRAY_COUNT(dMN1PModeFileIDs)];
void *sMN1PModeFiles[ARRAY_COUNT(dMN1PModeFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mn1pmode.c:101-156 0x80131B24. The three colour pairs are the
 * function's own stack constants. The count is 3 for the two full-size
 * tabs and 1 for the two short ones, which are one sprite each. */
void mn1PModeSetOptionSpriteColors(GObj *gobj, s32 status, s32 option_id)
{
    // 0x801330C8
    SYColorRGBPair selcolors = { { 0x00, 0x00, 0x00 }, { 0xFF, 0xFF, 0xFF } };

    // 0x801330D0
    SYColorRGBPair hicolors  = { { 0x82, 0x00, 0x28 }, { 0xFF, 0x00, 0x28 } };

    // 0x801330D8
    SYColorRGBPair notcolors = { { 0x00, 0x00, 0x00 }, { 0x82, 0x82, 0xAA } };

    SYColorRGBPair *colors;
    SObj *sobj;
    s32 count;
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
        break; // WARNING: Undefined behavior. This will assign sp 0x1C to colors which is uninitialized.
    }

    /*
     * Because Bonus 1 Pratice and Bonus 2 Practice use shorter, all-in-one option tab sprites,
     * they only require a single iteration as opposed to full-size, three-piece option tabs.
     */

    count = ((option_id == nMN1PModeOption1PGame) || (option_id == nMN1PModeOptionTrainingMode)) ? 3 : 1;

    sobj = SObjGetStruct(gobj);

    for (i = 0; i < count; i++)
    {
        sobj->envcolor.r = colors->prim.r;
        sobj->envcolor.g = colors->prim.g;
        sobj->envcolor.b = colors->prim.b;

        sobj->sprite.red = colors->env.r;
        sobj->sprite.green = colors->env.g;
        sobj->sprite.blue = colors->env.b;

        sobj = sobj->next;
    }
}

/* mn1pmode.c:159-193 0x80131D04, verbatim: the three-piece tab, whose
 * middle piece is tiled out to `lrs` eight-pixel steps. */
void mn1PModeMakeOptionTab(GObj *gobj, f32 pos_x, f32 pos_y, s32 lrs)
{
    SObj  *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[0], llMNCommonOptionTabLeftSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x;
    sobj->pos.y = pos_y;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[0], llMNCommonOptionTabMiddleSprite));

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

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[0], llMNCommonOptionTabRightSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x + 16.0F + (lrs * 8);
    sobj->pos.y = pos_y;
}

/* mn1pmode.c:196-218 0x80131E34, verbatim. */
void mn1PModeMake1PGame(void)
{
    GObj *gobj;
    SObj *sobj;

    sMN1PModeOptionGObjs[nMN1PModeOption1PGame] = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    mn1PModeMakeOptionTab(gobj, 124.0F, 42.0F, 16);
    mn1PModeSetOptionSpriteColors(gobj, sMN1PModeOption == nMN1PModeOption1PGame, nMN1PModeOption1PGame);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[1], llMN1P1PGameTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 161.0F;
    sobj->pos.y = 46.0F;
}

/* mn1pmode.c:221-243 0x80131F0C, verbatim. */
void mn1PModeMakeTrainingMode(void)
{
    GObj *gobj;
    SObj *sobj;

    sMN1PModeOptionGObjs[nMN1PModeOptionTrainingMode] = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    mn1PModeMakeOptionTab(gobj, 99.0F, 84.0F, 16);
    mn1PModeSetOptionSpriteColors(gobj, sMN1PModeOption == nMN1PModeOptionTrainingMode, nMN1PModeOptionTrainingMode);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[1], llMN1PTrainingModeTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 107.0F;
    sobj->pos.y = 87.0F;
}

/* mn1pmode.c:246-274 0x80131FE8, verbatim: the short all-in-one tab, so
 * the colours go on after the one sprite rather than before three. */
void mn1PModeMakeBonus1Practice(void)
{
    GObj *gobj;
    SObj *sobj;

    sMN1PModeOptionGObjs[nMN1PModeOptionBonus1Practice] = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[1], llMN1POptionTabSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 78.0F;
    sobj->pos.y = 126.0F;

    mn1PModeSetOptionSpriteColors(gobj, sMN1PModeOption == nMN1PModeOptionBonus1Practice, nMN1PModeOptionBonus1Practice);
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[1], llMN1PBonus1PracticeTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 97.0F;
    sobj->pos.y = 127.0F;
}

/* mn1pmode.c:277-305 0x801320F8, verbatim. */
void mn1PModeMakeBonus2Practice(void)
{
    GObj *gobj;
    SObj *sobj;

    sMN1PModeOptionGObjs[nMN1PModeOptionBonus2Practice] = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[1], llMN1POptionTabSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 67.0F;
    sobj->pos.y = 148.0F;

    mn1PModeSetOptionSpriteColors(gobj, sMN1PModeOption == nMN1PModeOptionBonus2Practice, nMN1PModeOptionBonus2Practice);
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[1], llMN1PBonus2PracticeTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 86.0F;
    sobj->pos.y = 149.0F;
}

/* mn1pmode.c:323-379 0x8013223C, the REGION_US arm -- which is the GObj
 * and nothing else. Everything the function does is under REGION_JP: a
 * frame and the chosen option's Japanese name, remade every time the
 * cursor moves, which is why mn1PModeFuncRun ejects and remakes this
 * GObj on each step. The US build has those four sprites in its file
 * (1PGameTextJap and its three neighbours) and never draws them, and
 * mn1PModeSetSubtitleSpriteColors goes with the arm. The same shape as
 * mnDataMakeMenuGObj (src/dc/mndata.c:345-380). */
void mn1PModeMakeSubtitle(void)
{
    sMN1PModeSubtitleGObj = gcMakeGObjSPAfter(0, NULL, 5, GOBJ_PRIORITY_DEFAULT);
}

/* mn1pmode.c:382-396 0x8013226C, the orange panel behind the logo and
 * the word GAME MODE: one gDPFillRectangle in G_CYC_1CYCLE under
 * G_CC_PRIMITIVE at 0xA0,0x78,0x14 and 0xE6 alpha, then the GObj's own
 * sprites over it. 1CYCLE's lower-right corner is exclusive, which is
 * the corner lbCommonSpriteFillRect takes, so the numbers are the
 * decomp's unchanged -- the same panel mnDataLabelsProcDisplay draws
 * (src/dc/mndata.c:383-397), to the pixel and the byte. */
void mn1PModeLabelsProcDisplay(GObj *gobj)
{
    lbCommonSpriteFillRect(225, 143, 310, 230, 0xA0, 0x78, 0x14, 0xE6);

    lbCommonClearExternSpriteParams();
    lbCommonDrawSObjAttr(gobj);
}

/* mn1pmode.c:399-442 0x801323B0, verbatim. */
void mn1PModeMakeLabels(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, mn1PModeLabelsProcDisplay, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[0], llMNCommonSmashLogoSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 235.0F;
    sobj->pos.y = 158.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[1], llMN1P1PTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 161.0F;
    sobj->pos.y = 194.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[0], llMNCommonGameModeTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 188.0F;
    sobj->pos.y = 88.0F;
}

/* mn1pmode.c:445-493 0x801324FC, verbatim: the collage wallpaper, two
 * paper decals tinted the panel's orange, and the dark controller icon
 * over them. */
void mn1PModeMakeDecals(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 0, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[0], llMNCommonSmashBrosCollageSprite));

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[0], llMNCommonDecalPaperSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xA0;
    sobj->sprite.green = 0x78;
    sobj->sprite.blue = 0x14;

    sobj->pos.x = 140.0F;
    sobj->pos.y = 143.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[0], llMNCommonDecalPaperSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xA0;
    sobj->sprite.green = 0x78;
    sobj->sprite.blue = 0x14;

    sobj->pos.x = 225.0F;
    sobj->pos.y = 56.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMN1PModeFiles[1], llMN1PControllerIconDarkSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x99;
    sobj->sprite.green = 0x99;
    sobj->sprite.blue = 0x99;

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;
}

/* mn1pmode.c:496-518 0x80132690, verbatim. The four cameras below are
 * one per DL link, drawn back to front: decals (0), labels (1), the
 * option tabs (2) and the subtitle (3). */
void mn1PModeMakeLink3Camera(void)
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

/* mn1pmode.c:521-543 0x80132730, verbatim. */
void mn1PModeMakeOptionsCamera(void)
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

/* mn1pmode.c:546-568 0x801327D0, verbatim. */
void mn1PModeMakeLabelsCamera(void)
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

/* mn1pmode.c:571-593 0x80132870, verbatim. */
void mn1PModeMakeDecalsCamera(void)
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

/* mn1pmode.c:596-622 0x80132910, verbatim: coming back from one of the
 * four selects puts the cursor back on the option that went there, and
 * anything else leaves it where the overlay's bzero left it (1P GAME,
 * which is 0). */
void mn1PModeInitVars(void)
{
    switch (gSCManagerSceneData.scene_prev)
    {
    case nSCKind1PGamePlayers:
        sMN1PModeOption = nMN1PModeOption1PGame;
        break;

    case nSCKindPlayers1PTraining:
        sMN1PModeOption = nMN1PModeOptionTrainingMode;
        break;

    case nSCKind1PBonus1Players:
        sMN1PModeOption = nMN1PModeOptionBonus1Practice;
        break;

    case nSCKind1PBonus2Players:
        sMN1PModeOption = nMN1PModeOptionBonus2Practice;
        break;
    }
    sMN1PModeOptionChangeWait = 0;
    sMN1PModeTotalTimeTics = 0;

    sMN1PModeIsProceedScene = FALSE;

    sMN1PModeReturnTic = sMN1PModeTotalTimeTics + I_MIN_TO_TICS(5);
}

/* mn1pmode.c:625-785 0x801329A8, verbatim. Nothing runs for the first
 * ten tics. After that: the five-minute idle return to the title, which
 * any input rearms; the deferred scene change (the tab is repainted
 * SELECTED on the press's own tic and the load happens on the next);
 * B to the mode select; and the stick or the jpad/C-buttons stepping
 * the cursor, which wraps and ejects and remakes the subtitle GObj. */
void mn1PModeFuncRun(GObj *gobj)
{
    s32 unused;

    // 0x801330E0
    GObj **option_gobjss[/* */] =
    {
        &sMN1PModeOptionGObjs[nMN1PModeOption1PGame],
        &sMN1PModeOptionGObjs[nMN1PModeOptionTrainingMode],
        &sMN1PModeOptionGObjs[nMN1PModeOptionBonus1Practice],
        &sMN1PModeOptionGObjs[nMN1PModeOptionBonus2Practice],
    };

    s32 stick_range;
    s32 player;
    sb32 is_button;

    sMN1PModeTotalTimeTics++;

    if (sMN1PModeTotalTimeTics >= 10)
    {
        if (sMN1PModeTotalTimeTics == sMN1PModeReturnTic)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            syTaskmanSetLoadScene();
            return;
        }
        if (scSubsysControllerCheckNoInputAll() == FALSE)
        {
            sMN1PModeReturnTic = sMN1PModeTotalTimeTics + I_MIN_TO_TICS(5);
        }
        if (sMN1PModeIsProceedScene != FALSE)
        {
            syTaskmanSetLoadScene();
        }
        if (sMN1PModeOptionChangeWait != 0)
        {
            sMN1PModeOptionChangeWait--;
        }
        if
        (
            (scSubsysControllerGetPlayerStickInRangeLR(-20, 20) != FALSE)          &&
            (scSubsysControllerGetPlayerStickInRangeUD(-20, 20) != FALSE)          &&
            (scSubsysControllerGetPlayerHoldButtons(U_JPAD | U_CBUTTONS) == FALSE) &&
            (scSubsysControllerGetPlayerHoldButtons(D_JPAD | D_CBUTTONS) == FALSE)
        )
        {
            sMN1PModeOptionChangeWait = 0;
        }
        player = scSubsysControllerGetPlayerTapButtons(A_BUTTON | START_BUTTON);

        if (player != FALSE)
        {
            switch (sMN1PModeOption)
            {
            case nMN1PModeOption1PGame:
                func_800269C0_275C0(nSYAudioFGMMenuSelect);
                mn1PModeSetOptionSpriteColors(sMN1PModeOptionGObjs[nMN1PModeOption1PGame], nMNOptionTabStatusSelected, nMN1PModeOption1PGame);

                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKind1PGamePlayers;
                gSCManagerSceneData.player = player - 1;

                sMN1PModeIsProceedScene = TRUE;
                return;

            case nMN1PModeOptionTrainingMode:
                func_800269C0_275C0(nSYAudioFGMMenuSelect);
                mn1PModeSetOptionSpriteColors(sMN1PModeOptionGObjs[nMN1PModeOptionTrainingMode], nMNOptionTabStatusSelected, nMN1PModeOptionTrainingMode);

                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKindPlayers1PTraining;
                gSCManagerSceneData.player = player - 1;

                sMN1PModeIsProceedScene = TRUE;
                return;

            case nMN1PModeOptionBonus1Practice:
                func_800269C0_275C0(nSYAudioFGMMenuSelect);
                mn1PModeSetOptionSpriteColors(sMN1PModeOptionGObjs[nMN1PModeOptionBonus1Practice], nMNOptionTabStatusSelected, nMN1PModeOptionBonus1Practice);

                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKind1PBonus1Players;
                gSCManagerSceneData.player = player - 1;

                sMN1PModeIsProceedScene = TRUE;
                return;

            case nMN1PModeOptionBonus2Practice:
                func_800269C0_275C0(nSYAudioFGMMenuSelect);
                mn1PModeSetOptionSpriteColors(sMN1PModeOptionGObjs[nMN1PModeOptionBonus2Practice], nMNOptionTabStatusSelected, nMN1PModeOptionBonus2Practice);

                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKind1PBonus2Players;
                gSCManagerSceneData.player = player - 1;

                sMN1PModeIsProceedScene = TRUE;
                return;
            }
        }
        if (scSubsysControllerGetPlayerTapButtons(B_BUTTON) != FALSE)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindModeSelect;

            syTaskmanSetLoadScene();
        }
        if
        (
            mn1PModeCheckGetOptionButtonInput(is_button, U_JPAD | U_CBUTTONS) ||
            mn1PModeCheckGetOptionStickInputUD(stick_range, 20, 1)
        )
        {
            func_800269C0_275C0(nSYAudioFGMMenuScroll2);

            mn1PModeSetOptionChangeWaitP(is_button, stick_range, 7);
            mn1PModeSetOptionSpriteColors(*option_gobjss[sMN1PModeOption], nMNOptionTabStatusNot, sMN1PModeOption);

            if (sMN1PModeOption == nMN1PModeOptionStart)
            {
                sMN1PModeOption = nMN1PModeOptionEnd;
            }
            else sMN1PModeOption--;

            if (sMN1PModeOption == nMN1PModeOptionStart)
            {
                sMN1PModeOptionChangeWait += 8;
            }
            mn1PModeSetOptionSpriteColors(*option_gobjss[sMN1PModeOption], nMNOptionTabStatusHighlight, sMN1PModeOption);
            gcEjectGObj(sMN1PModeSubtitleGObj);
            mn1PModeMakeSubtitle();
        }
        if
        (
            mn1PModeCheckGetOptionButtonInput(is_button, D_JPAD | D_CBUTTONS) ||
            mn1PModeCheckGetOptionStickInputUD(stick_range, -20, 0)
        )
        {
            func_800269C0_275C0(nSYAudioFGMMenuScroll2);

            mn1PModeSetOptionChangeWaitN(is_button, stick_range, 7);
            mn1PModeSetOptionSpriteColors(*option_gobjss[sMN1PModeOption], nMNOptionTabStatusNot, sMN1PModeOption);

            if (sMN1PModeOption == nMN1PModeOptionEnd)
            {
                sMN1PModeOption = nMN1PModeOptionStart;
            }
            else sMN1PModeOption++;

            if (sMN1PModeOption == nMN1PModeOptionEnd)
            {
                sMN1PModeOptionChangeWait += 8;
            }
            mn1PModeSetOptionSpriteColors(*option_gobjss[sMN1PModeOption], nMNOptionTabStatusHighlight, sMN1PModeOption);
            gcEjectGObj(sMN1PModeSubtitleGObj);
            mn1PModeMakeSubtitle();
        }
    }
}

/* mn1pmode.c:790-807: lbRelocInitSetup and lbRelocLoadFilesListed over
 * dMN1PModeFileIDs; here, the banks. A bank that is not there leaves
 * its file NULL and sprite_bank_get says so per sprite. */
void mn1PModeLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMN1PModeFileIDs)] =
    {
        MN1PMODE_BANK_COMMON,
        MN1PMODE_BANK_1P
    };
    s32 i;

    for (i = 0; i < ARRAY_COUNT(dMN1PModeFileIDs); i++)
    {
        if (sprite_bank_load(&sMN1PModeBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mn1PMode: no bank for file %d (%s)\n",
                          (int)dMN1PModeFileIDs[i], paths[i]);
            sMN1PModeFiles[i] = NULL;
            continue;
        }
        sMN1PModeFiles[i] = &sMN1PModeBanks[i];
    }
}

/* mn1pmode.c:788-829 0x80132E9C, in the game's order. Two DIVERGES:
 * the clear camera (gcMakeDefaultCameraGObj on link 0, DL priority 100,
 * a black fill) is the frame clear, which the PVR does itself -- the
 * same cut as the mode select's; and the decomp's own
 *
 *     if (!(error_flags & LBBACKUP_ERROR_HALFSTICKRANGE) &&
 *         (backup.boot > 42) && (gSYMainDmemOK == FALSE))
 *
 * is dropped with it. That arm latches "this console's stick only
 * reaches half range" from an RSP DMEM self-check the N64 boot runs
 * (gSYMainDmemOK, sys/main.c); there is no RSP here, nothing in the
 * port ever writes that global, and the flag it would set is read by
 * ftcommon.c alone. Leaving the check out leaves the flag as the save
 * data holds it, which is what a console that passes the check does. */
void mn1PModeFuncStart(void)
{
    mn1PModeLoadFiles();

    gcMakeGObjSPAfter(0, mn1PModeFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

    mn1PModeInitVars();
    mn1PModeMakeDecalsCamera();
    mn1PModeMakeLabelsCamera();
    mn1PModeMakeOptionsCamera();
    mn1PModeMakeLink3Camera();
    mn1PModeMakeDecals();
    mn1PModeMakeLabels();
    mn1PModeMake1PGame();
    mn1PModeMakeTrainingMode();
    mn1PModeMakeBonus1Practice();
    mn1PModeMakeBonus2Practice();
    mn1PModeMakeSubtitle();

    if (gSCManagerSceneData.scene_prev != nSCKindModeSelect)
    {
        syAudioPlayBGM(0, nSYAudioBGMModeSelect);
    }
}

/* The bzero arm of syDmaLoadOverlay for overlay 18, this file:
 * sc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mn1PModeOverlayLoad(void)
{
    OVERLAY_CLEAR(sMN1PModeOptionGObjs);
    OVERLAY_CLEAR(sMN1PModeOption);
    OVERLAY_CLEAR(sMN1PModeSubtitleGObj);
    OVERLAY_CLEAR(sMN1PModeIsProceedScene);
    OVERLAY_CLEAR(sMN1PModeOptionChangeWait);
    OVERLAY_CLEAR(sMN1PModeTotalTimeTics);
    OVERLAY_CLEAR(sMN1PModeReturnTic);
    OVERLAY_CLEAR(sMN1PModeBanks);
    OVERLAY_CLEAR(sMN1PModeFiles);
}

/* mn1pmode.c:880-887 0x80133020. DIVERGES as the mode select's:
 * syVideoInit and the zbuffer are the N64's video mode, set once at
 * boot here, and the arena_size line is the link map. What is left is
 * the last line. */
void mn1PModeStartScene(void)
{
    syTaskmanStartTask(&dMN1PModeTaskmanSetup);
}
