/* mnmaps.c -- see mnmaps.h. Every function is mn/mnmaps/mnmaps.c's by
 * name and body, REGION_US arms; the line numbers are the decomp's. */
#include "mnmaps.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"
#include "stage.h"
#include "objmodel.h"

#include <sys/debug.h>
#include <sys/controller.h>
#include <sys/rdp.h>
#include <sys/utils.h>
#include <sys/objanim.h>          /* gcPlayAnimAll */
#include <sc/scdef.h>
#include <sc/scsubsys/scsubsys.h>
#include <gm/gmsound.h>
#include <gr/grdef.h>
#include <lb/lbcommon.h>
#include <mn/mndef.h>
#include <PR/os.h>

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/sysshim.c defines it over the FGM engine */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mnvsmode.c does the same). Each `&llXxxSprite` below
 * is written `llXxxSprite`, the number, with the label's name kept as
 * the macro's. The values are relocData files 20 (FTEmblemSprites), 21
 * (MNSelectCommon), 30 (MNMaps) and 33 (MNCommonFonts) as
 * tools/export/ssb_spriteexport.py --list reads them off the ROM. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* mnMapsGetSlot runs off the end of its switch without a return, as the
 * decomp's does; the decomp builds with -Wno-return-type */
#pragma GCC diagnostic ignored "-Wreturn-type"
/* and its share of unused locals (the `unused` arrays the compiler's
 * stack layout needed, dMNMapsSubtitles* on the US arm) */
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
/* and mnMapsFuncRun's stick_input, which the decomp assigns inside the
 * short-circuit of `(button_input != 0) || (stick_input = ..., ...)`:
 * the arm that reads it runs only where button_input is zero, which is
 * exactly where the assignment happened, and the compiler cannot see it */
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

#define llMNSelectCommonStoneBackgroundSprite 0x0440

#define llMNMapsPeachsCastleTextSprite     0x01F8
#define llMNMapsSectorZTextSprite          0x0438
#define llMNMapsCongoJungleTextSprite      0x0678
#define llMNMapsPlanetZebesTextSprite      0x08B8
#define llMNMapsHyruleCastleTextSprite     0x0B10
#define llMNMapsYoshisIslandTextSprite     0x0D58
#define llMNMapsSaffronCityTextSprite      0x0F98
#define llMNMapsMushroomKingdomTextSprite  0x11D8
#define llMNMapsDreamLandTextSprite        0x1418
#define llMNMapsCursorSprite               0x1AB8
#define llMNMapsQuestionMarkSprite         0x1DD8
#define llMNMapsStageSelectTextSprite      0x26A0
#define llMNMapsWoodenCircleSprite         0x3840
#define llMNMapsPlateRightSprite           0x3C68
#define llMNMapsPlateMiddleSprite          0x3D68
#define llMNMapsPlateLeftSprite            0x3FA8
#define llMNMapsPeachsCastleSprite         0x4D88
#define llMNMapsSectorZSprite              0x5B68
#define llMNMapsCongoJungleSprite          0x6948
#define llMNMapsPlanetZebesSprite          0x7728
#define llMNMapsHyruleCastleSprite         0x8508
#define llMNMapsYoshisIslandSprite         0x92E8
#define llMNMapsSaffronCitySprite          0xA0C8
#define llMNMapsMushroomKingdomSprite      0xAEA8
#define llMNMapsDreamLandSprite            0xBC88
#define llMNMapsTilesSprite                0xC728
#define llMNMapsRandomSmallSprite          0xCB10
#define llMNMapsRandomBigSprite            0xDE30

#define llFTEmblemSpritesMarioSprite       0x0618
#define llFTEmblemSpritesDonkeySprite      0x0C78
#define llFTEmblemSpritesMetroidSprite     0x12D8
#define llFTEmblemSpritesFoxSprite         0x1938
#define llFTEmblemSpritesKirbySprite       0x1F98
#define llFTEmblemSpritesZeldaSprite       0x25F8
#define llFTEmblemSpritesYoshiSprite       0x2C58
#define llFTEmblemSpritesPMonstersSprite   0x3918

#define llMNCommonFontsLetterASprite       0x0040
#define llMNCommonFontsLetterBSprite       0x00D0
#define llMNCommonFontsLetterCSprite       0x0160
#define llMNCommonFontsLetterDSprite       0x01F0
#define llMNCommonFontsLetterESprite       0x0280
#define llMNCommonFontsLetterFSprite       0x0310
#define llMNCommonFontsLetterGSprite       0x03A0
#define llMNCommonFontsLetterHSprite       0x0430
#define llMNCommonFontsLetterISprite       0x04C0
#define llMNCommonFontsLetterJSprite       0x0550
#define llMNCommonFontsLetterKSprite       0x05E0
#define llMNCommonFontsLetterLSprite       0x0670
#define llMNCommonFontsLetterMSprite       0x0700
#define llMNCommonFontsLetterNSprite       0x0790
#define llMNCommonFontsLetterOSprite       0x0820
#define llMNCommonFontsLetterPSprite       0x08B0
#define llMNCommonFontsLetterQSprite       0x0940
#define llMNCommonFontsLetterRSprite       0x09D0
#define llMNCommonFontsLetterSSprite       0x0A60
#define llMNCommonFontsLetterTSprite       0x0AF0
#define llMNCommonFontsLetterUSprite       0x0B80
#define llMNCommonFontsLetterVSprite       0x0C10
#define llMNCommonFontsLetterWSprite       0x0CA0
#define llMNCommonFontsLetterXSprite       0x0D30
#define llMNCommonFontsLetterYSprite       0x0DC0
#define llMNCommonFontsLetterZSprite       0x0E50
#define llMNCommonFontsSymbolApostropheSprite 0x0ED0
#define llMNCommonFontsSymbolPercentSprite    0x0F60
#define llMNCommonFontsSymbolPeriodSprite     0x0FD0

/* The banks the four files became (src/dc/mnmodeselect.c on where they
 * live). The emblems' and the stone wall's are the character select's,
 * already exported. */
#define MNMAPS_BANK_EMBLEMS "ftemblems.spr"
#define MNMAPS_BANK_SELECT  "mnselect.spr"
#define MNMAPS_BANK_MAPS    "mnmaps.spr"
#define MNMAPS_BANK_FONTS   "mnfonts.spr"

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mnmaps.c:19-25 (0x801344D0): the five link labels' values written out.
 * 26 is GRWallpaperTrainingBlack, which only training mode's preview
 * reads; the port has no such scene, so mnMapsLoadFiles leaves it NULL
 * rather than spend VRAM on a 300x220 image nothing draws. */
u32 dMNMapsFileIDs[/* */] = { 20, 21, 30, 33, 26 };

/* mnmaps.c:28-39 dMNMapsFileInfos, the nine map files and the offset of
 * MPGroundData in each, and mnmaps.c:42-47 dMNMapsWallpaperOffsets and
 * mnmaps.c:50-55/58 the three training-mode wallpapers, are not here:
 * gGRStages (src/dc/stage.h) is the port's table of what a stage is,
 * and mnMapsLoadMapFile reads it. */

/* mnmaps.c:60-68 dMNMapsLights1 and dMNMapsDisplayList, the lighting the
 * pre-render function sets for the preview model, are not here: see
 * dMNMapsTaskmanSetup. The port's equivalent is dc_model_set_light,
 * which mnMapsFuncStart calls with the same direction. */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x80134BD0
s32 sMNMapsPad0x80134BD0[2];

// 0x80134BD8
s32 sMNMapsCursorSlot;

// 0x80134BDC
GObj *sMNMapsCursorGObj;

// 0x80134BE0
GObj *sMNMapsNameLogoGObj;

// 0x80134BE4
GObj *sMNMapsHeap0WallpaperGObj;

// 0x80134BE8
GObj *sMNMapsHeap1WallpaperGObj;

// 0x80134BF0
GObj *sMNMapsHeap0LayerGObjs[4];

// 0x80134C00
GObj *sMNMapsHeap1LayerGObjs[4];

/* mnmaps.c:106 sMNMapsGroundInfo, MPGroundData* on the N64: what
 * mnMapsLoadMapFile leaves behind for mnMapsMakePreview to build from.
 * The port's stage is a Stage (src/dc/stage.h), which carries the same
 * scalars and the layers as one baked pack. */
Stage *sMNMapsGroundInfo;

/* The port's two model heaps. The game takes two heaps the size of its
 * largest map file (mnMapsAllocModelHeaps) and force-loads a map into
 * whichever one is not on screen, so that the preview being built and
 * the preview being shown never share storage. The port's stage comes
 * out of malloc rather than a heap, so what a "heap" holds here is the
 * kind whose stage that slot has a reference on, or MNMAPS_HEAP_EMPTY.
 * Everything else about the double buffer, including which slot is
 * which, is sMNMapsHeapID and the game's.
 *
 * -1 rather than a kind: gr/grdef.h has no "no stage" value, because a
 * heap on the N64 is empty when nothing has been loaded into it and the
 * game never asks. */
#define MNMAPS_HEAP_EMPTY (-1)
static s32 sMNMapsHeapStages[2];

// 0x80134C14
CObj *sMNMapsPreviewCObj;

// 0x80134C18
sb32 sMNMapsIsTrainingMode;

// 0x80134C1C - // flag indicating which bonus features are available
u8 sMNMapsUnlockedMask;

// 0x80134C20
s32 sMNMapsHeapID;

// 0x80134C24 - Frames elapsed on SSS
s32 sMNMapsTotalTimeTics;

// 0x80134C28 - Frames until cursor can be moved again
s32 sMNMapsScrollWait;

// 0x80134C2C - Frames to wait until exiting Stage Select
s32 sMNMapsReturnTic;

/* mnmaps.c:127-130 sMNMapsStatusBuffer and sMNMapsForceStatusBuffer, the
 * reloc loader's thirty-entry file tables, are not here: mnMapsLoadFiles. */

// 0x80134E10
void *sMNMapsFiles[ARRAY_COUNT(dMNMapsFileIDs)];

/* the banks behind sMNMapsFiles */
static SpriteBank sMNMapsBanks[ARRAY_COUNT(dMNMapsFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mnmaps.c:141-157 0x80131B00. DIVERGES: the game measures the largest
 * of the nine map files and takes two heaps that size. The port's stage
 * is a malloc'd Stage of whatever size the pack is, so there is nothing
 * to measure and nothing to reserve; what the two heaps carried -- which
 * preview is which -- is sMNMapsHeapID and the two GObj arrays, and
 * those are the game's and stay. This marks both slots empty, which is
 * the state a freshly taken pair of heaps is in. */
void mnMapsAllocModelHeaps(void)
{
    sMNMapsHeapStages[0] = MNMAPS_HEAP_EMPTY;
    sMNMapsHeapStages[1] = MNMAPS_HEAP_EMPTY;
}

/* The port's own: the stage select's stages, given back with the scene.
 * The game has no counterpart because its heaps go with the scene heap
 * -- mnMapsStartScene is where this is called from, for the same reason
 * ftManagerReleaseFilesAll is called where it is. */
void mnMapsReleaseMapFiles(void)
{
    s32 i;

    for (i = 0; i < 2; i++)
    {
        if (sMNMapsHeapStages[i] != MNMAPS_HEAP_EMPTY)
        {
            grStageRelease(sMNMapsHeapStages[i]);
            sMNMapsHeapStages[i] = MNMAPS_HEAP_EMPTY;
        }
    }
    sMNMapsGroundInfo = NULL;
}

/* mnmaps.c:160-163 mnMapsFuncLights 0x80131B88, a gSPDisplayList of the
 * lighting above, is not here: dMNMapsTaskmanSetup. */

// 0x80131BAC
sb32 mnMapsCheckLocked(s32 gkind)
{
    if (gkind == nGRKindInishie)
    {
        if (sMNMapsUnlockedMask & LBBACKUP_UNLOCK_MASK_INISHIE)
        {
            return FALSE;
        }
        else return TRUE;
    }
    else return FALSE;
}

// 0x80131BE4
s32 mnMapsGetCharacterID(const char c)
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

// 0x80131C5C
f32 mnMapsGetCharacterSpacing(const char *str, s32 c)
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

// 0x80131D80 - Unused?
void mnMapsMakeString(GObj *gobj, const char *str, f32 x, f32 y, u32 *color)
{
    intptr_t chars[/* */] =
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

    for (i = 0; str[i] != 0; i++)
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
            sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[3], chars[mnMapsGetCharacterID(str[i])]));
            sobj->pos.x = start_x;

            start_x += sobj->sprite.width + mnMapsGetCharacterSpacing(str, i);

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

// 0x80131FA4
void mnMapsMakeWallpaper(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 0, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[1], llMNSelectCommonStoneBackgroundSprite));

    sobj->cms = G_TX_WRAP;
    sobj->cmt = G_TX_WRAP;

    sobj->masks = 6;
    sobj->maskt = 5;

    sobj->lrs = 300;
    sobj->lrt = 220;

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;
}

// 0x80132048
void mnMapsMakePlaque(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 8, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 6, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[2], llMNMapsWoodenCircleSprite));
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 189.0F;
    sobj->pos.y = 124.0F;
}

/* mnmaps.c:222-243 0x801320E0. The thirteen RDP commands are the port's
 * two fill quads (src/dc/lbcommon.h): a blue-grey rule under STAGE
 * SELECT and a black shadow under the name plate, both blended by their
 * alpha inside this sprite pass, so both take the pass's depth like
 * every rectangle around them (the mode select's fill found that). The
 * cycle type, the combine and the two render modes are what the port's
 * fill quad already is; the pipe syncs have nothing to sync. */
void mnMapsLabelsProcDisplay(GObj *gobj)
{
    lbCommonSpriteFillRect(160, 128, 320, 134, 0x57, 0x60, 0x88, 0xFF);
    lbCommonSpriteFillRect(194, 189, 268, 193, 0x00, 0x00, 0x00, 0x33);

    lbCommonClearExternSpriteParams();
    lbCommonDrawSObjAttr(gobj);
}

// 0x80132288
void mnMapsMakeLabels(void)
{
    GObj *gobj;
    SObj *sobj;
    s32 x;

    gobj = gcMakeGObjSPAfter(0, NULL, 6, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, mnMapsLabelsProcDisplay, 4, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[2], llMNMapsStageSelectTextSprite));
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->envcolor.r = 0x00;
    sobj->envcolor.g = 0x00;
    sobj->envcolor.b = 0x00;

    sobj->sprite.red = 0xAF;
    sobj->sprite.green = 0xB1;
    sobj->sprite.blue = 0xCC;

    sobj->pos.x = 172.0F;
    sobj->pos.y = 122.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[2], llMNMapsPlateLeftSprite));
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 174.0F;
    sobj->pos.y = 191.0F;

    for (x = 186; x < 262; x += 4)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[2], llMNMapsPlateMiddleSprite));
        sobj->pos.x = x;
        sobj->pos.y = 191.0F;
    }
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[2], llMNMapsPlateRightSprite));
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 262.0F;
    sobj->pos.y = 191.0F;
}

// 0x80132430
s32 mnMapsGetGroundKind(s32 slot)
{
    s32 gkinds[/* */] =
    {
        nGRKindCastle, nGRKindJungle, nGRKindHyrule, nGRKindZebes, nGRKindInishie,
        nGRKindYoster, nGRKindPupupu, nGRKindSector, nGRKindYamabuki, 0xDE
    };

    if (slot == 9)
    {
        return 0xDE;
    }
    return gkinds[slot];
}

// 0x80132498
s32 mnMapsGetSlot(s32 gkind)
{
    switch (gkind)
    {
    case nGRKindCastle:
        return 0;

    case nGRKindJungle:
        return 1;

    case nGRKindHyrule:
        return 2;

    case nGRKindZebes:
        return 3;

    case nGRKindInishie:
        return 4;

    case nGRKindYoster:
        return 5;

    case nGRKindPupupu:
        return 6;

    case nGRKindSector:
        return 7;

    case nGRKindYamabuki:
        return 8;

    case 0xDE:
        return 9;
    }
}

// 0x80132528
void mnMapsMakeIcons(void)
{
    GObj *gobj;
    SObj *sobj;

    intptr_t offsets[/* */] =
    {
        llMNMapsPeachsCastleSprite,     llMNMapsSectorZSprite,
        llMNMapsCongoJungleSprite,      llMNMapsPlanetZebesSprite,
        llMNMapsHyruleCastleSprite,     llMNMapsYoshisIslandSprite,
        llMNMapsDreamLandSprite,        llMNMapsSaffronCitySprite,
        llMNMapsMushroomKingdomSprite,  llMNMapsRandomSmallSprite
    };
    s32 x;
    s32 i;

    gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; i < ARRAY_COUNT(offsets); i++)
    {
        if (mnMapsCheckLocked(mnMapsGetGroundKind(i)) == FALSE)
        {
            x = i * 50;

            if (i == 9)
            {
                sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[2], llMNMapsRandomSmallSprite));
            }
            else sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[2], offsets[mnMapsGetGroundKind(i)]));

            if (i < 5)
            {
                sobj->pos.y = 30.0F;
                sobj->pos.x = x + 30;
            }
            else
            {
                sobj->pos.y = 68.0F;
                sobj->pos.x = x - 220;
            }
        }
    }
}

// 0x801326DC
void mnMapsSetNamePosition(SObj *sobj, s32 gkind)
{
    Vec2f positions[/* */] =
    {
        { 195.0F, 196.0F },
        { 202.0F, 196.0F },
        { 190.0F, 196.0F },
        { 195.0F, 196.0F },
        { 198.0F, 196.0F },
        { 190.0F, 196.0F },
        { 195.0F, 196.0F },
        { 190.0F, 196.0F },
        { 190.0F, 196.0F }
    };
#if defined(REGION_US)
    sobj->pos.x = 183.0F;
    sobj->pos.y = 196.0F;
#else
    sobj->pos.x = positions[gkind].x;
    sobj->pos.y = positions[gkind].y;
#endif
}

// 0x80132738
void mnMapsMakeName(GObj *gobj, s32 gkind)
{
    SObj* sobj;
    intptr_t offsets[/* */] =
    {
        llMNMapsPeachsCastleTextSprite,
        llMNMapsSectorZTextSprite,
        llMNMapsCongoJungleTextSprite,
        llMNMapsPlanetZebesTextSprite,
        llMNMapsHyruleCastleTextSprite,
        llMNMapsYoshisIslandTextSprite,
        llMNMapsDreamLandTextSprite,
        llMNMapsSaffronCityTextSprite,
        llMNMapsMushroomKingdomTextSprite
    };

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[2], offsets[gkind]));
    mnMapsSetNamePosition(sobj, gkind);

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;
}

#if defined(REGION_US)
// 0x80134700
char *dMNMapsSubtitles[/* */] =
{
    "IN THE SKY OF",
    "SECTOR Z",
    "CONGO JUNGLE",
    "PLANET ZEBES",
    "CASTLE OF HYRULE",
    "YOSHI'S ISLAND",
    "PUPUPU LAND",
    "YAMABUKI CITY",
    "CLASSIC MUSHROOM"
};
char *dMNMapsSubtitles2[/* */] =
{
    "CASTLE PEACH",
    "ABOARD A GREAT FOX",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "KINGDOM"
};

// 0x80134748
Vec2f dMNMapsSubtitlePositions[/* */] =
{
    { 192.0F, 167.0F },
    { 214.0F, 167.0F },
    { 202.0F, 169.0F },
    { 202.0F, 169.0F },
    { 193.0F, 169.0F },
    { 198.0F, 169.0F },
    { 205.0F, 169.0F },
    { 199.0F, 169.0F },
    { 191.0F, 167.0F }
};
Vec2f dMNMapsSubtitlePositions2[/* */] =
{
    { 209.0F, 174.0F },
    { 188.0F, 174.0F },
    {   0.0F,   0.0F },
    {   0.0F,   0.0F },
    {   0.0F,   0.0F },
    {   0.0F,   0.0F },
    {   0.0F,   0.0F },
    { 203.0F, 174.0F },
    { 213.0F, 174.0 }
};

// 0x801347D8
u32 dMNMapsSubtitleColors[/* */] = { 255, 255, 255 };

// 0x801327E0 - Unused?
void mnMapsSubtitleHasExtraLine(void)
{
    return;
}

// 0x801327E8 - Unused?
void mnMapsMakeSubtitle(void)
{
    return;
}
#else
// 0x801327E0 - Unused?
sb32 mnMapsSubtitleHasExtraLine(s32 gkind)
{
    switch (gkind)
    {
        case nGRKindCastle:
        case nGRKindSector:
        case nGRKindInishie:
            return TRUE;
        default:
            return FALSE;
    }
}

// 0x801327E8 - Unused?
void mnMapsMakeSubtitle(GObj *gobj, s32 gkind) {
    char *dMNMapsSubtitles[/* */] =
    {
        "IN THE SKY OF",
        "SECTOR Z",
        "CONGO JUNGLE",
        "PLANET ZEBES",
        "CASTLE OF HYRULE",
        "YOSHI'S ISLAND",
        "PUPUPU LAND",
        "YAMABUKI CITY",
        "CLASSIC MUSHROOM"
    };
    char *dMNMapsSubtitles2[/* */] =
    {
        "CASTLE PEACH",
        "ABOARD A GREAT FOX",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "KINGDOM"
    };

    Vec2f dMNMapsSubtitlePositions[/* */] =
    {
        { 192.0F, 167.0F },
        { 214.0F, 167.0F },
        { 202.0F, 169.0F },
        { 202.0F, 169.0F },
        { 193.0F, 169.0F },
        { 198.0F, 169.0F },
        { 205.0F, 169.0F },
        { 199.0F, 169.0F },
        { 191.0F, 167.0F }
    };
    Vec2f dMNMapsSubtitlePositions2[/* */] =
    {
        { 209.0F, 174.0F },
        { 188.0F, 174.0F },
        {   0.0F,   0.0F },
        {   0.0F,   0.0F },
        {   0.0F,   0.0F },
        {   0.0F,   0.0F },
        {   0.0F,   0.0F },
        { 203.0F, 174.0F },
        { 213.0F, 174.0 }
    };

    u32 dMNMapsSubtitleColors[/* */] = { 255, 255, 255 };

    mnMapsMakeString(gobj, dMNMapsSubtitles[gkind], dMNMapsSubtitlePositions[gkind].x, dMNMapsSubtitlePositions[gkind].y, dMNMapsSubtitleColors);

    if (mnMapsSubtitleHasExtraLine(gkind) != FALSE)
    {
        mnMapsMakeString(gobj, dMNMapsSubtitles2[gkind], dMNMapsSubtitlePositions2[gkind].x, dMNMapsSubtitlePositions2[gkind].y, dMNMapsSubtitleColors);
    }
}
#endif

// 0x801327F0
void mnMapsSetLogoPosition(GObj *gobj, s32 gkind)
{
    Vec2f positions[/* */] =
    {
        { 3.0F, 19.0F },
        { 3.0F, 19.0F },
        { 3.0F, 20.0F },
        { 2.0F, 20.0F },
        { 3.0F, 17.0F },
        {-1.0F, 19.0F },
        { 1.0F, 20.0F },
        { 1.0F, 20.0F },
        { 3.0F, 19.0F },
        {34.0F, 20.0F }
    };

    if (gkind == 0xDE)
    {
        SObjGetStruct(gobj)->pos.x = 223.0F;
        SObjGetStruct(gobj)->pos.y = 144.0F;
    }
    else
    {
        SObjGetStruct(gobj)->pos.x = positions[gkind].x + 189.0F;
        SObjGetStruct(gobj)->pos.y = positions[gkind].y + 124.0F;
    }
}

// 0x801328A8
void mnMapsMakeEmblem(GObj *gobj, s32 gkind)
{
    SObj *sobj;

    intptr_t offsets[/* */] =
    {
        llFTEmblemSpritesMarioSprite,   llFTEmblemSpritesFoxSprite,
        llFTEmblemSpritesDonkeySprite,  llFTEmblemSpritesMetroidSprite,
        llFTEmblemSpritesZeldaSprite,   llFTEmblemSpritesYoshiSprite,
        llFTEmblemSpritesKirbySprite,   llFTEmblemSpritesPMonstersSprite,
        llFTEmblemSpritesMarioSprite
    };

    if (gkind == 0xDE)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[2], llMNMapsQuestionMarkSprite));

        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->sprite.red = 0x5C;
        sobj->sprite.green = 0x22;
        sobj->sprite.blue = 0x00;
    }
    else
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[0], offsets[gkind]));
        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;

        sobj->sprite.red = 0x5C;
        sobj->sprite.green = 0x22;
        sobj->sprite.blue = 0x00;
    }
    mnMapsSetLogoPosition(gobj, gkind);
}

// 0x801329AC
void mnMapsMakeNameAndEmblem(s32 slot)
{
    GObj *gobj;

    if (sMNMapsNameLogoGObj != NULL)
    {
        gcEjectGObj(sMNMapsNameLogoGObj);
    }
    gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    sMNMapsNameLogoGObj = gobj;
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);
    mnMapsMakeEmblem(sMNMapsNameLogoGObj, mnMapsGetGroundKind(slot));

    if (slot != 9)
    {
        mnMapsMakeName(sMNMapsNameLogoGObj, mnMapsGetGroundKind(slot));
#if defined(REGION_JP)
        mnMapsMakeSubtitle(sMNMapsNameLogoGObj, mnMapsGetGroundKind(slot));
#endif
    }
}

// 0x80132A58
void mnMapsSetCursorPosition(GObj *gobj, s32 slot)
{
    if (slot < 5)
    {
        SObjGetStruct(gobj)->pos.x = (slot * 50) + 23;
        SObjGetStruct(gobj)->pos.y = 23.0F;
    }
    else
    {
        SObjGetStruct(gobj)->pos.x = (slot * 50) - 250 + 23;
        SObjGetStruct(gobj)->pos.y = 61.0F;
    }
}

// 0x80132ADC
void mnMapsMakeCursor(void)
{
    GObj *gobj;
    SObj *sobj;

    sMNMapsCursorGObj = gobj = gcMakeGObjSPAfter(0, NULL, 7, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 5, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[2], llMNMapsCursorSprite));
    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x00;

    mnMapsSetCursorPosition(gobj, sMNMapsCursorSlot);
}

/* mnmaps.c:585-598 0x80132B84. The game force-loads the map file into
 * the heap the caller names and points sMNMapsGroundInfo at the
 * MPGroundData inside it; this reads the same file off the same medium
 * (src/dc/stage.h) into the slot that is not on screen, which is the
 * heap the game's caller names -- mnMapsMakePreview is the only one, and
 * it hands its own the same way. So `heap` keeps the parameter's place
 * and the slot is read from sMNMapsHeapID instead.
 *
 * DIVERGES in one place, defensively: all nine stages have packs, so a
 * stage whose pack will not load leaves NULL, which mnMapsMakePreview
 * draws as an empty window.
 *
 * The slot's previous stage is released after the new one is taken, not
 * before, because they can be the same kind -- a cursor moved off a
 * stage and back -- and releasing first would free a stage only to read
 * it again. */
void mnMapsLoadMapFile(s32 gkind, void *heap)
{
    s32 slot = (sMNMapsHeapID == 0) ? 1 : 0;
    s32 was = sMNMapsHeapStages[slot];

    (void)heap;

    sMNMapsGroundInfo = grStageAcquire(gkind);
    sMNMapsHeapStages[slot] = (sMNMapsGroundInfo != NULL) ? gkind : MNMAPS_HEAP_EMPTY;

    if (was != MNMAPS_HEAP_EMPTY)
    {
        grStageRelease(was);
    }
}

/* mnmaps.c:601-618 0x80132BC8. As mnMapsLabelsProcDisplay: the eleven
 * RDP commands are one fill quad, the black pane the preview sits on. */
void mnMapsPreviewWallpaperProcDisplay(GObj *gobj)
{
    lbCommonSpriteFillRect(43, 130, 152, 211, 0x00, 0x00, 0x00, 0x73);

    lbCommonClearExternSpriteParams();
    lbCommonDrawSObjAttr(gobj);
}

/* mnmaps.c:50-59 dMNMapsTrainingModeFileInfos' file column and
 * dMNMapsTrainingModeWallpaperIDs, verbatim but for the link labels
 * written out (26-28 are GRWallpaperTrainingBlack, -Yellow, -Blue). The
 * table's other column, the fog colour, has no reader in this file. */
static const char *const dMNMapsTrainingModeBanks[/* */] =
{
    "grwptrainingblack.spr", "grwptrainingyellow.spr", "grwptrainingblue.spr"
};
s32 dMNMapsTrainingModeWallpaperIDs[/* */] = { 2, 0, 0, 0, 2, 1, 2, 2, 2, 0 };

/* The one file the game forces into file 26's heap slot per preview
 * (lbRelocGetForceExternHeapFile): here a bank, reloaded per preview. */
static SpriteBank sMNMapsTrainingWallpaperBank;

/* mnmaps.c:621-682 0x80132D2C. DIVERGES twice, both in the else arm:
 * the training background is a bank of its own (the three files are
 * relocData 26-28, each one sprite at 0x20718, llGRWallpaperTraining-
 * BlueSprite for all three as the game has it), and the stage's own
 * background comes off the pack rather than out of MPGroundData
 * (stage_wallpaper_sprite, src/dc/stage.h). A stage the port has no
 * pack for draws the tiles and nothing on them. */
GObj* mnMapsMakePreviewWallpaper(s32 gkind)
{
    GObj *gobj;
    SObj *sobj;
    s32 x;

    gobj = gcMakeGObjSPAfter(0, NULL, 9, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, mnMapsPreviewWallpaperProcDisplay, 7, GOBJ_PRIORITY_DEFAULT, ~0);

    // draw patterned bg
    for (x = 43; x < 155; x += 16)
    {
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[2], llMNMapsTilesSprite));
        sobj->pos.x = x;
        sobj->pos.y = 130.0F;

        continue;
    }
    // Check if Random
    if (gkind == 0xDE)
    {
        // If Random, use Random image
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMapsFiles[2], llMNMapsRandomBigSprite));
        sobj->pos.x = 40.0F;
        sobj->pos.y = 127.0F;
    }
    else
    {
        Sprite *wallpaper;

        // If not Random, check if Training Mode
        if (sMNMapsIsTrainingMode == TRUE)
        {
            // If Training Mode, use Smash logo bg
            wallpaper = NULL;

            if (sprite_bank_load(&sMNMapsTrainingWallpaperBank, dMNMapsTrainingModeBanks[dMNMapsTrainingModeWallpaperIDs[gkind]]) == 0)
            {
                wallpaper = sprite_bank_get(&sMNMapsTrainingWallpaperBank, 0x20718);
            }
        }
        else wallpaper = stage_wallpaper_sprite(sMNMapsGroundInfo); // Use stage bg

        if (wallpaper == NULL)
        {
            return gobj;
        }
        sobj = lbCommonMakeSObjForGObj(gobj, wallpaper); // Use stage bg

        sobj->sprite.attr &= ~SP_FASTCOPY;

        sobj->sprite.scalex = 0.37F;
        sobj->sprite.scaley = 0.37F;

        sobj->pos.x = 40.0F;
        sobj->pos.y = 127.0F;
    }
    return gobj;
}

/* mnmaps.c:685-692 0x80132EF0 and mnmaps.c:695-706 0x80132F70. Both set
 * the z-buffer and a render mode and then walk the tree; the port's
 * display proc for a model built from a pack does the same walk with
 * the PVR's own depth test, and the secondary layer's second DL head is
 * the translucent list the same proc submits to on the second pass
 * (src/dc/objmodel.c). What is left of either is that call -- the
 * layered one, because the preview stands between the sprite pass that
 * draws its window and the passes that draw the icons and the plaque
 * over it, and the PVR's opaque list is under every sprite. */
void mnMapsModelPriProcDisplay(GObj *gobj)
{
    dc_model_proc_display_layered(gobj);
}

void mnMapsModelSecProcDisplay(GObj *gobj)
{
    dc_model_proc_display_layered(gobj);
}

/* mnmaps.c:709-742 0x8013303C. DIVERGES: the game builds one GObj per
 * MPGroundDesc -- a DObj tree from its DObjDesc, its MObjs, its joint
 * and material animations -- and hangs it on the primary or the
 * secondary display proc as MPGroundData.layer_mask says.
 * tools/export/ssb_stageexport.py bakes all four layers into one pack
 * (src/dc/stage.h), so there is one GObj, its tree is the pack's joints
 * (src/dc/objmodel.c), and the layer_mask choice is already in the
 * batches' PVR lists. The animations are the game's: attached
 * (stage_add_layer_anims, the pack's gcAddAnimAll) and played ONCE, with
 * no process, so the preview stands still at frame 0 of every layer's
 * animation just as the game's does. The scale is the game's, per
 * stage, set after the play as the game sets it. */
GObj* mnMapsMakeLayer(s32 gkind, Stage *stage, s32 id)
{
    GObj *gobj;
    f32 scales[/* */] =
    {
        0.5F, 0.2F, 0.6F,
        0.5F, 0.3F, 0.6F,
        0.5F, 0.4F, 0.2F
    };

    if (stage == NULL)
    {
        return NULL;
    }
    gobj = gcMakeGObjSPAfter(0, NULL, 5, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, mnMapsModelPriProcDisplay, 3, GOBJ_PRIORITY_DEFAULT, ~0);

    /* the pack's joints hang straight off the GObj, as the battle
     * stage's do (grCommonSetupInitAll, src/dc/stage.c), so each of the
     * merged layers' roots is a sibling there. The game calls this
     * function once per layer and puts scales[gkind] on that layer's
     * root DObj, replacing whatever its DObjDesc carried; the port has
     * one GObj for the four, so the walk below puts it on every root --
     * which is the same thing, and is checked: all four of Hyrule's
     * layer roots carry an identity scale in the ROM. */
    if (dc_model_add_dobjs(gobj, NULL, &stage->model, NULL) < 0)
    {
        gcEjectGObj(gobj);
        return NULL;
    }
    if (stage_add_layer_anims(stage, gobj) != FALSE)
    {
        gcPlayAnimAll(gobj);
    }
    {
        DObj *root;

        for (root = DObjGetStruct(gobj); root != NULL; root = root->sib_next)
        {
            root->scale.vec.f.x = scales[gkind];
            root->scale.vec.f.y = scales[gkind];
            root->scale.vec.f.z = scales[gkind];
        }
    }

    return gobj;
}

/* mnmaps.c:745-777 0x801331AC. One GObj for the four layers, per
 * mnMapsMakeLayer, then the game's two fix-ups: Saffron's layer-3
 * grandchild and Yoshi's Island's 15th and 17th layer-0 DObjs are
 * hidden, pieces the preview leaves out.
 *
 * DIVERGES only in how a layer's root is found. The game has a GObj per
 * layer; the port's merged tree bakes the layers in order, so layer 0's
 * root is the GObj's first root, and Saffron's layer 3 -- one root, the
 * last layer baked -- is its last. tools/export/ssb_stageexport.py refuses to
 * write either stage if that stops being true. */
void mnMapsMakeModel(s32 gkind, s32 heap_id)
{
    DObj *root_dobj, *next_dobj;
    GObj **gobjs = (heap_id == 0) ? sMNMapsHeap1LayerGObjs : sMNMapsHeap0LayerGObjs;
    s32 i;

    gobjs[0] = mnMapsMakeLayer(gkind, sMNMapsGroundInfo, 0);

    if (gobjs[0] == NULL)
    {
        return;
    }
    if (gkind == nGRKindYamabuki)
    {
        for (root_dobj = DObjGetStruct(gobjs[0]); root_dobj->sib_next != NULL; root_dobj = root_dobj->sib_next);

        DObjGetChild(DObjGetChild(root_dobj))->flags = DOBJ_FLAG_HIDDEN;
    }
    if (gkind == nGRKindYoster)
    {
        for
        (
            next_dobj = root_dobj = DObjGetStruct(gobjs[0]), i = 1;
            next_dobj != NULL;
            next_dobj = lbCommonGetTreeDObjNextFromRoot(next_dobj, root_dobj), i++
        )
        {
            if ((i == 0xF) || (i == 0x11))
            {
                next_dobj->flags = DOBJ_FLAG_HIDDEN;
            }
        }
    }
}

// 0x801332DC
void mnMapsDestroyPreview(s32 heap_id)
{
    s32 i;

    if (heap_id == 0)
    {
        if (sMNMapsHeap0WallpaperGObj != NULL)
        {
            gcEjectGObj(sMNMapsHeap0WallpaperGObj);
            sMNMapsHeap0WallpaperGObj = NULL;
        }
        for (i = 0; i < ARRAY_COUNT(sMNMapsHeap0LayerGObjs); i++)
        {
            if (sMNMapsHeap0LayerGObjs[i] != NULL)
            {
                gcEjectGObj(sMNMapsHeap0LayerGObjs[i]);
                sMNMapsHeap0LayerGObjs[i] = NULL;
            }
        }
    }
    else
    {
        if (sMNMapsHeap1WallpaperGObj != NULL)
        {
            gcEjectGObj(sMNMapsHeap1WallpaperGObj);
            sMNMapsHeap1WallpaperGObj = NULL;
        }
        for (i = 0; i < ARRAY_COUNT(sMNMapsHeap1LayerGObjs); i++)
        {
            if (sMNMapsHeap1LayerGObjs[i] != NULL)
            {
                gcEjectGObj(sMNMapsHeap1LayerGObjs[i]);
                sMNMapsHeap1LayerGObjs[i] = NULL;
            }
        }
    }
}

// 0x801333B4
void mnMapsMakePreview(s32 gkind)
{
    if (gkind != 0xDE)
    {
        /* the game hands mnMapsLoadMapFile whichever of its two model
         * heaps is not the one on screen; the port's stages are already
         * loaded, so both arms are the same call */
        mnMapsLoadMapFile(gkind, NULL);
    }
    if (sMNMapsHeapID == 0)
    {
        sMNMapsHeap1WallpaperGObj = mnMapsMakePreviewWallpaper(gkind);
    }
    else sMNMapsHeap0WallpaperGObj = mnMapsMakePreviewWallpaper(gkind);

    if (gkind != 0xDE)
    {
        mnMapsMakeModel(gkind, sMNMapsHeapID);
        mnMapsSetPreviewCameraPosition(sMNMapsPreviewCObj, gkind);
    }
    mnMapsDestroyPreview(sMNMapsHeapID);

    sMNMapsHeapID = (sMNMapsHeapID == 0) ? 1 : 0;
}

// 0x801334AC
void mnMapsMakeWallpaperCamera(void)
{
    GObj *gobj = gcMakeCameraGObj
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
    );
    CObj *cobj = CObjGetStruct(gobj);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

// 0x8013354C
void mnMapsMakePlaqueCamera(void)
{
    GObj *gobj = gcMakeCameraGObj
    (
        1,
        NULL,
        1,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSprite,
        40,
        COBJ_MASK_DLLINK(6),
        ~0,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(gobj);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

// 0x801335EC
void mnMapsMakePreviewWallpaperCamera(void)
{
    GObj *gobj = gcMakeCameraGObj
    (
        1,
        NULL,
        1,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSprite,
        70,
        COBJ_MASK_DLLINK(7),
        ~0,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(gobj);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

// 0x8013368C
void mnMapsMakeLabelsViewport(void)
{
    GObj *gobj = gcMakeCameraGObj
    (
        1,
        NULL,
        1,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSprite,
        30,
        COBJ_MASK_DLLINK(4),
        ~0,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(gobj);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

// 0x8013372C
void mnMapsMakeIconsCamera(void)
{
    GObj *gobj = gcMakeCameraGObj
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
    );
    CObj *cobj = CObjGetStruct(gobj);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

// 0x801337CC
void mnMapsMakeNameAndEmblemCamera(void)
{
    GObj *gobj = gcMakeCameraGObj
    (
        1,
        NULL,
        1,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSprite,
        20,
        COBJ_MASK_DLLINK(2),
        ~0,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(gobj);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

// 0x8013386C
void mnMapsMakeCursorCamera(void)
{
    GObj *gobj = gcMakeCameraGObj
    (
        1,
        NULL,
        1,
        GOBJ_PRIORITY_DEFAULT,
        lbCommonDrawSprite,
        50,
        COBJ_MASK_DLLINK(5),
        ~0,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(gobj);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

// 0x8013390C
void mnMapsSetPreviewCameraPosition(CObj *cobj, s32 gkind)
{
    Vec3f positions[/* */] =
    {
        { 1700.0F, 1800.0F, 0.0F },
        { 1600.0F, 1600.0F, 0.0F },
        { 1600.0F, 1600.0F, 0.0F },
        { 1600.0F, 1600.0F, 0.0F },
        { 1600.0F, 1500.0F, 0.0F },
        { 1600.0F, 1600.0F, 0.0F },
        { 1600.0F, 1500.0F, 0.0F },
        { 1600.0F, 1600.0F, 0.0F },
        { 1200.0F, 1600.0F, 0.0F },
    };

    if (gkind == 0xDE)
    {
        gkind = nGRKindCastle;
    }
    cobj->vec.eye.x = -3000.0F;
    cobj->vec.eye.y = 3000.0F;
    cobj->vec.eye.z = 9000.0F;
    cobj->vec.up.x = 0.0F;
    cobj->vec.up.y = 1.0F;
    cobj->vec.up.z = 0.0F;
    cobj->vec.at.x = positions[gkind].x;
    cobj->vec.at.y = positions[gkind].y;
    cobj->vec.at.z = positions[gkind].z;
}

// 0x801339C4
void mnMapsPreviewCameraThreadUpdate(GObj *gobj)
{
    CObj* cobj = CObjGetStruct(gobj);
    f32 y = cobj->vec.at.y;
    f32 deg = 0.0F;

    while (TRUE)
    {
        cobj->vec.at.y = __sinf(F_CLC_DTOR32(deg)) * 40.0F + y;

        deg = (deg + 2.0F > 360.0F) ? deg + 2.0F - 360.0F : deg + 2.0F;

        gcSleepCurrentGObjThread(1);
    }
}

// 0x80133A88
void mnMapsMakePreviewCamera(void)
{
    s32 unused;
    GObj *gobj = gcMakeCameraGObj
    (
        1,
        NULL,
        1,
        GOBJ_PRIORITY_DEFAULT,
        func_80017DBC,
        65,
        COBJ_MASK_DLLINK(3),
        ~0,
        TRUE,
        nGCProcessKindThread,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(gobj);

    sMNMapsPreviewCObj = cobj;

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->projection.persp.far = 16384.0F;

    mnMapsSetPreviewCameraPosition(cobj, mnMapsGetGroundKind(sMNMapsCursorSlot));

    gcAddGObjProcess(gobj, mnMapsPreviewCameraThreadUpdate, nGCProcessKindThread, 1);
}

// 0x80133B78
void mnMapsSaveSceneData(void)
{
    s32 unused[/* */] =
    {
        nGRKindPupupu,  nGRKindZebes,   nGRKindCastle,
        nGRKindInishie, nGRKindJungle,  nGRKindSector,
        nGRKindYoster,  nGRKindYamabuki,nGRKindHyrule
    };
    s32 gkind;

    if (sMNMapsCursorSlot == 9)
    {
        do
        {
            gkind = syUtilsRandTimeUCharRange(9);
        }
        while ((mnMapsCheckLocked(gkind) != FALSE) || (gkind == gSCManagerSceneData.gkind));

        gSCManagerSceneData.gkind = gkind;
    }
    else gSCManagerSceneData.gkind = mnMapsGetGroundKind(sMNMapsCursorSlot);

    if (sMNMapsIsTrainingMode == FALSE)
    {
        gSCManagerSceneData.maps_vsmode_gkind = mnMapsGetGroundKind(sMNMapsCursorSlot);
    }
    if (sMNMapsIsTrainingMode == TRUE)
    {
        gSCManagerSceneData.maps_training_gkind = mnMapsGetGroundKind(sMNMapsCursorSlot);
    }
}

// 0x80133C6C
void mnMapsInitVars(void)
{
    s32 i;

    sMNMapsNameLogoGObj = NULL;
    sMNMapsHeap0WallpaperGObj = NULL;
    sMNMapsHeap1WallpaperGObj = NULL;

    for (i = 0; i < ARRAY_COUNT(sMNMapsHeap0LayerGObjs); i++)
    {
        sMNMapsHeap0LayerGObjs[i] = NULL;
        sMNMapsHeap1LayerGObjs[i] = NULL;
    }
    switch (gSCManagerSceneData.scene_prev)
    {
    case nSCKindPlayers1PTraining:
        sMNMapsIsTrainingMode = TRUE;
        sMNMapsCursorSlot = mnMapsGetSlot(gSCManagerSceneData.maps_training_gkind);
        break;

    case nSCKindPlayersVS:
        sMNMapsIsTrainingMode = FALSE;
        sMNMapsCursorSlot = mnMapsGetSlot(gSCManagerSceneData.maps_vsmode_gkind);
        break;
    }
    sMNMapsUnlockedMask = gSCManagerBackupData.unlock_mask;
    sMNMapsHeapID = 1;
    sMNMapsTotalTimeTics = 0;
    sMNMapsReturnTic = sMNMapsTotalTimeTics + I_MIN_TO_TICS(5);
}

// 0x80133D60
void mnMapsSaveSceneData2(void)
{
    mnMapsSaveSceneData();
}

// 0x80133D80
void mnMapsFuncRun(GObj *gobj)
{
    s32 unused;
    s32 stick_input;
    s32 button_input;

    sMNMapsTotalTimeTics++;

    if (sMNMapsTotalTimeTics >= 10)
    {
        if (sMNMapsTotalTimeTics == sMNMapsReturnTic)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            mnMapsSaveSceneData2();
            syTaskmanSetLoadScene();
            return;
        }
        if (scSubsysControllerCheckNoInputAll() == FALSE)
        {
            sMNMapsReturnTic = sMNMapsTotalTimeTics + I_MIN_TO_TICS(5);
        }
        if (sMNMapsScrollWait != 0)
        {
            sMNMapsScrollWait--;
        }
        if
        (
            (scSubsysControllerGetPlayerStickInRangeLR(-20, 20)) &&
            (scSubsysControllerGetPlayerStickInRangeUD(-20, 20)) &&
            (scSubsysControllerGetPlayerHoldButtons(U_JPAD | R_JPAD | R_TRIG | U_CBUTTONS | R_CBUTTONS) == FALSE) &&
            (scSubsysControllerGetPlayerHoldButtons(D_JPAD | L_JPAD | L_TRIG | D_CBUTTONS | L_CBUTTONS) == FALSE)
        )
        {
            sMNMapsScrollWait = 0;
        }

        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | START_BUTTON))
        {
            mnMapsSaveSceneData2();
            func_800269C0_275C0(nSYAudioFGMStageSelect);

            if (sMNMapsIsTrainingMode == TRUE)
            {
                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKind1PTrainingMode;
            }
            else
            {
                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKindVSBattle;
            }
            syTaskmanSetLoadScene();
        }
        if (scSubsysControllerGetPlayerTapButtons(B_BUTTON))
        {
            mnMapsSaveSceneData2();

            if (sMNMapsIsTrainingMode == TRUE)
            {
                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKindPlayers1PTraining;
            }
            else
            {
                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKindPlayersVS;
            }
            syTaskmanSetLoadScene();
        }
        if (sMNMapsScrollWait == 0)
        {
            button_input = scSubsysControllerGetPlayerHoldButtons(U_JPAD | U_CBUTTONS);

            if ((button_input != 0) || (stick_input = scSubsysControllerGetPlayerStickUD(20, 1), (stick_input != 0)))
            {
                if ((sMNMapsCursorSlot >= 5) && (mnMapsCheckLocked(mnMapsGetGroundKind(sMNMapsCursorSlot - 5)) == FALSE))
                {
                    func_800269C0_275C0(nSYAudioFGMMenuScroll2);

                    sMNMapsCursorSlot -= 5;

                    mnMapsMakeNameAndEmblem(sMNMapsCursorSlot);
                    mnMapsSetCursorPosition(sMNMapsCursorGObj, sMNMapsCursorSlot);
                    mnMapsMakePreview(mnMapsGetGroundKind(sMNMapsCursorSlot));
                }
                if (button_input != 0)
                {
                    sMNMapsScrollWait = 12;
                }
                else sMNMapsScrollWait = (160 - stick_input) / 7;

                return;
            }
            button_input = scSubsysControllerGetPlayerHoldButtons(D_JPAD | D_CBUTTONS);

            if ((button_input != 0) || (stick_input = scSubsysControllerGetPlayerStickUD(-20, 0), (stick_input != 0)))
            {
                if ((sMNMapsCursorSlot < 5) && (mnMapsCheckLocked(mnMapsGetGroundKind(sMNMapsCursorSlot + 5)) == FALSE))
                {
                    func_800269C0_275C0(nSYAudioFGMMenuScroll2);

                    sMNMapsCursorSlot += 5;

                    mnMapsMakeNameAndEmblem(sMNMapsCursorSlot);
                    mnMapsSetCursorPosition(sMNMapsCursorGObj, sMNMapsCursorSlot);
                    mnMapsMakePreview(mnMapsGetGroundKind(sMNMapsCursorSlot));
                }
                if (button_input != 0)
                {
                    sMNMapsScrollWait = 12;
                }
                else sMNMapsScrollWait = (stick_input + 160) / 7;

                return;
            }
            button_input = scSubsysControllerGetPlayerHoldButtons(L_JPAD | L_TRIG | L_CBUTTONS);

            if ((button_input != 0) || (stick_input = scSubsysControllerGetPlayerStickLR(-20, 0), (stick_input)))
            {
                switch (sMNMapsCursorSlot)
                {
                case 0:
                    sMNMapsCursorSlot = (mnMapsCheckLocked(mnMapsGetGroundKind(4))) ? 3 : 4;
                    break;

                case 5:
                    sMNMapsCursorSlot = 9;
                    break;

                default:
                    sMNMapsCursorSlot--;
                }
                func_800269C0_275C0(nSYAudioFGMMenuScroll2);
                mnMapsMakeNameAndEmblem(sMNMapsCursorSlot);
                mnMapsSetCursorPosition(sMNMapsCursorGObj, sMNMapsCursorSlot);
                mnMapsMakePreview(mnMapsGetGroundKind(sMNMapsCursorSlot));

                if (button_input != 0)
                {
                    sMNMapsScrollWait = 12;
                }
                else sMNMapsScrollWait = (stick_input + 160) / 7;

                return;
            }
            button_input = scSubsysControllerGetPlayerHoldButtons(R_JPAD | R_TRIG | R_CBUTTONS);

            if ((button_input != 0) || (stick_input = scSubsysControllerGetPlayerStickLR(20, 1), (stick_input)))
            {
                switch (sMNMapsCursorSlot)
                {
                case 3:
                    sMNMapsCursorSlot = (mnMapsCheckLocked(mnMapsGetGroundKind(4))) ? 0 : 4;
                    break;

                case 4:
                    sMNMapsCursorSlot = 0;
                    break;

                case 9:
                    sMNMapsCursorSlot = 5;
                    break;

                default:
                    sMNMapsCursorSlot++;
                }
                func_800269C0_275C0(nSYAudioFGMMenuScroll2);
                mnMapsMakeNameAndEmblem(sMNMapsCursorSlot);
                mnMapsSetCursorPosition(sMNMapsCursorGObj, sMNMapsCursorSlot);
                mnMapsMakePreview(mnMapsGetGroundKind(sMNMapsCursorSlot));

                if (button_input != 0)
                {
                    sMNMapsScrollWait = 12;
                }
                else sMNMapsScrollWait = (160 - stick_input) / 7;
            }
        }
    }
}

/* mnmaps.c:1618-1626 mnMapsFuncStart's LBRelocSetup and
 * lbRelocLoadFilesListed: the port's files are sprite banks, one per
 * relocData file, loaded from the romdisk (src/dc/sprite.h). File 26 is
 * training mode's preview background and is not exported: mnmaps.h. */
void mnMapsLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMNMapsFileIDs)] =
    {
        MNMAPS_BANK_EMBLEMS,
        MNMAPS_BANK_SELECT,
        MNMAPS_BANK_MAPS,
        MNMAPS_BANK_FONTS,
        NULL
    };
    s32 i;

    for (i = 0; i < ARRAY_COUNT(dMNMapsFileIDs); i++)
    {
        if (paths[i] == NULL)
        {
            sMNMapsFiles[i] = NULL;
            continue;
        }
        if (sprite_bank_load(&sMNMapsBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mnMaps: no bank for file %d (%s)\n",
                          (int)dMNMapsFileIDs[i], paths[i]);
            sMNMapsFiles[i] = NULL;
            continue;
        }
        sMNMapsFiles[i] = &sMNMapsBanks[i];
    }
}

/* mnmaps.c:1616-1660 0x80134304, in the game's order. DIVERGES: the
 * clear camera (gcMakeDefaultCameraGObj on link 0, DL priority 100, a
 * black fill with the z-buffer) is the frame clear, which the PVR does
 * itself -- the same cut as the mode select's. The lighting the scene's
 * pre-render function would set for the preview model is
 * dc_model_set_light, with dMNMapsLights1's direction: the game's
 * gdSPDefLights1 puts its light at (0x14, 0x14, 0x14) in the Light's
 * signed bytes, which the RSP reads on a 0x7F == 1.0 scale -- so the
 * light is (20, 20, 20) / 127, a short vector 0.27 long, and every
 * face of the preview is lit mostly by the ambient. */
void mnMapsFuncStart(void)
{
    mnMapsLoadFiles();
    mnMapsAllocModelHeaps();

    gcMakeGObjSPAfter(0, mnMapsFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
    mnMapsInitVars();
    dc_model_set_light(20.0F / 127.0F, 20.0F / 127.0F, 20.0F / 127.0F);
    mnMapsMakeWallpaperCamera();
    mnMapsMakeLabelsViewport();
    mnMapsMakeIconsCamera();
    mnMapsMakeNameAndEmblemCamera();
    mnMapsMakeCursorCamera();
    mnMapsMakePreviewCamera();
    mnMapsMakePlaqueCamera();
    mnMapsMakePreviewWallpaperCamera();
    mnMapsMakeWallpaper();
    mnMapsMakePlaque();
    mnMapsMakeLabels();
    mnMapsMakeIcons();
    mnMapsMakeNameAndEmblem(sMNMapsCursorSlot);
    mnMapsMakeCursor();
    mnMapsMakePreview(mnMapsGetGroundKind(sMNMapsCursorSlot));
}

/* mnmaps.c:1663 dMNMapsVideoSetup is not here: mnMapsStartScene. */

/* mnmaps.c:1666-1708 (0x80134928). The pool counts are the game's, all
 * zero: a menu scene takes its objects straight from the scene heap,
 * and the preview camera's thread its stack the same way (sys/objman.c
 * gcGetGObjThread). DIVERGES as the VS mode's: the arena is the port's
 * region, the draw is the scene manager's, and mnMapsFuncLights is
 * dropped. */
SYTaskmanSetup dMNMapsTaskmanSetup =
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
    sizeof(u64) * 64,                   // Thread stack size
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

    mnMapsFuncStart                     // Task start function
};

/* The bzero arm of syDmaLoadOverlay for overlay 30, this
 * file: sc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnMapsOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNMapsPad0x80134BD0);
    OVERLAY_CLEAR(sMNMapsCursorSlot);
    OVERLAY_CLEAR(sMNMapsCursorGObj);
    OVERLAY_CLEAR(sMNMapsTrainingWallpaperBank);
    OVERLAY_CLEAR(sMNMapsNameLogoGObj);
    OVERLAY_CLEAR(sMNMapsHeap0WallpaperGObj);
    OVERLAY_CLEAR(sMNMapsHeap1WallpaperGObj);
    OVERLAY_CLEAR(sMNMapsHeap0LayerGObjs);
    OVERLAY_CLEAR(sMNMapsHeap1LayerGObjs);
    OVERLAY_CLEAR(sMNMapsGroundInfo);
    OVERLAY_CLEAR(sMNMapsHeapStages);
    OVERLAY_CLEAR(sMNMapsPreviewCObj);
    OVERLAY_CLEAR(sMNMapsIsTrainingMode);
    OVERLAY_CLEAR(sMNMapsUnlockedMask);
    OVERLAY_CLEAR(sMNMapsHeapID);
    OVERLAY_CLEAR(sMNMapsTotalTimeTics);
    OVERLAY_CLEAR(sMNMapsScrollWait);
    OVERLAY_CLEAR(sMNMapsReturnTic);
    OVERLAY_CLEAR(sMNMapsFiles);
    OVERLAY_CLEAR(sMNMapsBanks);

    /* Zero is nGRKindCastle, not "empty". The clear above is what the
     * overlay load owes every static; this is what the value has to be,
     * and it is the same call mnMapsFuncStart makes. */
    mnMapsAllocModelHeaps();
}

/* mnmaps.c:1711-1719 0x8013446C. DIVERGES as the VS mode's: syVideoInit
 * and the zbuffer are the N64's video mode, set once at boot here, and
 * the arena_size line is the link map. What is left is the last line. */
void mnMapsStartScene(void)
{
    syTaskmanStartTask(&dMNMapsTaskmanSetup);

    /* The two previews' stages, given back with the scene: the task does
     * not return until the scene is over, so this is the scene's end.
     * The game's equivalent is its two model heaps going with the scene
     * heap.  4a. */
    mnMapsReleaseMapFiles();
}
