/* mnoption.c -- see mnoption.h. Every function is mn/mnoption/mnoption.c's
 * by name and body, REGION_US arms; the line numbers are the decomp's. */
#include "mnoption.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"
#include "bgm.h"

#include <sys/debug.h>
#include <sys/controller.h>
#include <sys/rdp.h>
#include <sys/audio.h>
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

/* mnoption.c:38, verbatim: the last-set sound quality, which
 * src/dc/syaudio.c already keeps (syAudioSetQuality). */
extern sb32 dSYAudioSoundQuality;

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mndata.c does the same). Each `&llXxxSprite` below is
 * written `llXxxSprite`, the number, with the label's name kept as the
 * macro's. The values are relocData files 0 (MNCommon) and 4 (MNOption)
 * as tools/export/ssb_spriteexport.py --list reads them off the ROM. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

#define llMNCommonOptionTabLeftSprite    0x001E8
#define llMNCommonOptionTabMiddleSprite  0x00330
#define llMNCommonOptionTabRightSprite   0x00568
#define llMNCommonSlashSprite            0x0BA28
#define llMNCommonDecalPaperSprite       0x02A30
#define llMNCommonSmashLogoSprite        0x031F8
#define llMNCommonSmashBrosCollageSprite 0x18000

#define llMNOptionStereoTextSprite       0x071F8
#define llMNOptionMonoTextSprite         0x073A8
#define llMNOptionSoundTextSprite        0x07628
#define llMNOptionScreenAdjustTextSprite 0x08138
#define llMNOptionBackupClearTextSprite  0x08780
#define llMNOptionOptionTextSprite       0x09288
#define llMNOptionSettingsIconDarkSprite 0x0B958

/* The banks the two files became. The common bank is the mode select's,
 * which already carries every sprite this screen takes from it -- the
 * tab's three pieces, the slash, the paper decal, the logo and the
 * collage. */
#define MNOPTION_BANK_COMMON "mncommon.spr"
#define MNOPTION_BANK_OPTION "mnoption.spr"

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mnoption.c:47 (0x80133620): { &llMNCommonFileID, &llMNOptionFileID },
 * the two link labels' values written out. */
u32 dMNOptionFileIDs[/* */] = { 0, 4 };

/* mnoption.c:50-58 dMNOptionLights1 and dMNOptionDisplayList, the
 * lighting the pre-render function would set for the 3D this scene never
 * draws. Dropped with mnOptionFuncLights (dMNOptionTaskmanSetup says
 * why). */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* mnoption.c:67-73 */
GObj *sMNOptionOptionSoundGObj;
GObj *sMNOptionOptionScreenAdjustGObj;
GObj *sMNOptionOptionBackupClearGObj;

/* mnoption.c:76 sMNOptionPad0x801337B0[2], two words of the overlay
 * between the GObjs and the option cursor that nothing names. Dropped:
 * the port has no link map to keep a hole in. */

/* mnoption.c:79 */
s32 sMNOptionOption;

/* mnoption.c:82 - 0 = mono, 1 = stereo */
sb32 sMNOptionSoundMonoOrStereo;

/* mnoption.c:85 */
sb32 sMNOptionIsScreenFlash;

/* mnoption.c:88 sMNOptionPad0x801337C4, one padding word. Dropped with
 * sMNOptionPad0x801337B0 above. */

/* mnoption.c:91 */
GObj *sMNOptionSoundOptionGObj;

/* mnoption.c:94 sMNOptionPad0x801337CC, one more padding word. Dropped
 * the same way. */

/* mnoption.c:97 */
GObj *sMNOptionMenuGObj;

/* mnoption.c:100, 103 D_ovl60_801337D4 and D_ovl60_801337D8: an unnamed
 * GObj and tic number, read only by the three functions below that
 * nothing calls (0x80132618, 0x8013275C's own caller 0x80132794, and
 * 0x801327CC). Kept for the same reason mndata.c keeps
 * func_ovl61_80131D54: the decomp's dead code, ported anyway rather than
 * silently dropped. */
GObj *D_ovl60_801337D4;
s32 D_ovl60_801337D8;

/* mnoption.c:106 sMNOptionIsProceedScene */
sb32 sMNOptionIsProceedScene;

/* mnoption.c:109-115 */
s32 sMNOptionOptionChangeWait;
s32 sMNOptionTotalTimeTics;
s32 sMNOptionReturnTic;

/* mnoption.c:117-118 sMNOptionPad0x801337EC, a padding word the decomp
 * itself comments out. Not ported. */

/* mnoption.c:121 sMNOptionStatusBuffer[24], the reloc loader's per-file
 * status records. Dropped with lbRelocInitSetup (mnOptionLoadFiles). */

/* mnoption.c:124 */
void *sMNOptionFiles[ARRAY_COUNT(dMNOptionFileIDs)];

/* the banks behind sMNOptionFiles */
static SpriteBank sMNOptionBanks[ARRAY_COUNT(dMNOptionFileIDs)];

/* mnoption.c:17-30's five macros, which name this file's wait counter in
 * the shared mnCommon* forms (mn/mndef.h). */
#define mnOptionCheckGetOptionButtonInput(is_button, mask) \
    mnCommonCheckGetOptionButtonInput(sMNOptionOptionChangeWait, is_button, mask)

#define mnOptionCheckGetOptionStickInputUD(stick_range, min, b) \
    mnCommonCheckGetOptionStickInputUD(sMNOptionOptionChangeWait, stick_range, min, b)

#define mnOptionCheckGetOptionStickInputLR(stick_range, min, b) \
    mnCommonCheckGetOptionStickInputLR(sMNOptionOptionChangeWait, stick_range, min, b)

#define mnOptionSetOptionChangeWaitP(is_button, stick_range, div) \
    mnCommonSetOptionChangeWaitP(sMNOptionOptionChangeWait, is_button, stick_range, div)

#define mnOptionSetOptionChangeWaitN(is_button, stick_range, div) \
    mnCommonSetOptionChangeWaitN(sMNOptionOptionChangeWait, is_button, stick_range, div)

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mnoption.c:133-136 mnOptionFuncLights: the scene's pre-render
 * function, two GBI commands setting a single light for 3D this scene
 * has none of. Dropped, as every other menu scene's is
 * (src/dc/mnvsmode.c). */

/* mnoption.c:138-185 0x80131B24, verbatim -- the same body
 * mnDataSetOptionSpriteColors carries in src/dc/mndata.c, including the
 * `default` arm that leaves `colors` uninitialized (the decomp's own
 * "Undefined behavior" comment). Every caller passes
 * nMNOptionTabStatusNot, Highlight or Selected, or the 0/1 of a
 * comparison against those first two, so the arm is unreachable. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
void mnOptionSetOptionSpriteColors(GObj *gobj, s32 status)
{
    // 0x80133668
    SYColorRGBPair selcolors = { { 0x00, 0x00, 0x00 }, { 0xFF, 0xFF, 0xFF } };

    // 0x80133670
    SYColorRGBPair hicolors  = { { 0x82, 0x00, 0x28 }, { 0xFF, 0x00, 0x28 } };

    // 0x80133678
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

/* mnoption.c:187-222 0x80131BFC, verbatim: the tab is three sprites, and
 * the middle one is one texel column tiled out to `lrs` * 8 pixels
 * (cms/cmt 0, masks 4), so a tab is as wide as its caller asks -- the
 * same idiom mnDataMakeOptionTab uses. */
void mnOptionMakeOptionTabs(GObj *gobj, f32 pos_x, f32 pos_y, s32 lrs)
{
    SObj *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[0], llMNCommonOptionTabLeftSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x;
    sobj->pos.y = pos_y;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[0], llMNCommonOptionTabMiddleSprite));

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

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[0], llMNCommonOptionTabRightSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = pos_x + 16.0F + (lrs * 8);
    sobj->pos.y = pos_y;
}

/* mnoption.c:224-249 0x80131D2C, verbatim: the two colours mono_or_stereo
 * picks between, one full-white the other 0x32 grey, over the STEREO and
 * MONO text sprites -- sobj is STEREO and sobj->next is MONO
 * (mnOptionMakeSoundToggle makes them in that order). */
void mnOptionSetSoundToggleSpriteColors(GObj *gobj, sb32 mono_or_stereo)
{
    SObj *sobj = SObjGetStruct(gobj);

    if (mono_or_stereo != 0)
    {
        sobj->sprite.red   = 0xFF;
        sobj->sprite.green = 0xFF;
        sobj->sprite.blue  = 0xFF;

        sobj->next->sprite.red   = 0x32;
        sobj->next->sprite.green = 0x32;
        sobj->next->sprite.blue  = 0x32;
    }
    else
    {
        sobj->sprite.red   = 0x32;
        sobj->sprite.green = 0x32;
        sobj->sprite.blue  = 0x32;

        sobj->next->sprite.red   = 0xFF;
        sobj->next->sprite.green = 0xFF;
        sobj->next->sprite.blue  = 0xFF;
    }
}

/* mnoption.c:251-289 0x80131D98, verbatim: STEREO, MONO and the slash
 * between them, each its own SObj on link 4 (mnOptionMakeLink4Camera),
 * ejected and remade whenever the toggle changes. */
void mnOptionMakeSoundToggle(void)
{
    GObj *gobj;
    SObj *sobj;

    sMNOptionSoundOptionGObj = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[1], llMNOptionStereoTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 179.0F;
    sobj->pos.y = 48.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[1], llMNOptionMonoTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 236.0F;
    sobj->pos.y = 48.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[0], llMNCommonSlashSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0x32;
    sobj->sprite.green = 0x32;
    sobj->sprite.blue  = 0x32;

    sobj->pos.x = 229.0F;
    sobj->pos.y = 48.0F;

    mnOptionSetSoundToggleSpriteColors(gobj, sMNOptionSoundMonoOrStereo);
}

/* mnoption.c:291-314 0x80131EF0, verbatim: the top tab, at the layout
 * mndata.c's DATA menu never needs to shrink for (Options always has
 * exactly three tabs). */
void mnOptionMakeSound(void)
{
    GObj *gobj;
    SObj *sobj;

    sMNOptionOptionSoundGObj = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    mnOptionMakeOptionTabs(gobj, 113.0F, 42.0F, 17);
    mnOptionSetOptionSpriteColors(gobj, sMNOptionOption == nMNOptionOptionSound);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[1], llMNOptionSoundTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue  = 0x00;

    sobj->pos.x = 116.0F;
    sobj->pos.y = 46.0F;
}

/* mnoption.c:316-341 0x80131FC4, verbatim. */
void mnOptionMakeScreenAdjust(void)
{
    GObj *gobj;
    SObj *sobj;

    sMNOptionOptionScreenAdjustGObj = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);

    mnOptionMakeOptionTabs(gobj, 91.0F, 89.0F, 17);

    mnOptionSetOptionSpriteColors(gobj, sMNOptionOption == nMNOptionOptionScreenAdjust);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[1], llMNOptionScreenAdjustTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue  = 0x00;

    sobj->pos.x = 103.0F;
    sobj->pos.y = 92.0F;
}

/* mnoption.c:343-366 0x8013209C, verbatim. */
void mnOptionMakeBackupClear(void)
{
    GObj *gobj;
    SObj *sobj;

    sMNOptionOptionBackupClearGObj = gobj = gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 2, GOBJ_PRIORITY_DEFAULT, ~0);
    mnOptionMakeOptionTabs(gobj, 69.0F, 136.0F, 17);
    mnOptionSetOptionSpriteColors(gobj, sMNOptionOption == nMNOptionOptionBackupClear);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[1], llMNOptionBackupClearTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue  = 0x00;

    sobj->pos.x = 86.0F;
    sobj->pos.y = 140.0F;
}

/* mnoption.c:368-381 0x80132174 - Unused?, verbatim, and unused in the US
 * build: its only caller is the JP arm of mnOptionMakeMenuGObj below,
 * the same shape mndata.c's mnDataSetSubtitleSpriteColors carries. */
void mnOptionSetSubtitleSpriteColors(SObj *sobj)
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

/* mnoption.c:383-457 0x801321A8, the REGION_US arm -- which is the GObj
 * and nothing else, the same cut mndata.c's mnDataMakeMenuGObj takes.
 * Everything the decomp function does is under REGION_JP: a frame and
 * the chosen option's Japanese description, remade every time the
 * cursor moves. */
void mnOptionMakeMenuGObj(void)
{
    sMNOptionMenuGObj = gcMakeGObjSPAfter(0, NULL, 5, GOBJ_PRIORITY_DEFAULT);
}

/* mnoption.c:459-474 0x80132248, the RDP commands in the port's spelling
 * (src/dc/lbcommon.h) -- the orange panel behind the logo and the word
 * OPTION, the same tint mndata.c's mnDataLabelsProcDisplay draws for
 * DATA. 1CYCLE's lower-right corner is exclusive, which is the corner
 * lbCommonSpriteFillRect takes, so the numbers are the decomp's
 * unchanged. */
void mnOptionLabelsProcDisplay(GObj *gobj)
{
    lbCommonSpriteFillRect(225, 143, 310, 230, 0xA0, 0x78, 0x14, 0xE6);

    lbCommonClearExternSpriteParams();
    lbCommonDrawSObjAttr(gobj);
}

/* mnoption.c:476-509 0x8013238C, verbatim. */
void mnOptionMakeLabels(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, mnOptionLabelsProcDisplay, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[0], llMNCommonSmashLogoSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue  = 0x00;

    sobj->pos.x = 235.0F;
    sobj->pos.y = 158.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[1], llMNOptionOptionTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0x00;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue  = 0x00;

    sobj->pos.x = 201.0F;
    sobj->pos.y = 120.0F;
}

/* mnoption.c:511-560 0x80132484, verbatim: the collage, two paper decals
 * in the same orange as the panel, and the dark settings icon over the
 * collage's own -- the same three-piece shape mndata.c's
 * mnDataMakeDecals draws for DATA. */
void mnOptionMakeDecals(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 0, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[0], llMNCommonSmashBrosCollageSprite));

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[0], llMNCommonDecalPaperSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0xA0;
    sobj->sprite.green = 0x78;
    sobj->sprite.blue  = 0x14;

    sobj->pos.x = 140.0F;
    sobj->pos.y = 143.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[0], llMNCommonDecalPaperSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0xA0;
    sobj->sprite.green = 0x78;
    sobj->sprite.blue  = 0x14;

    sobj->pos.x = 225.0F;
    sobj->pos.y = 56.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNOptionFiles[1], llMNOptionSettingsIconDarkSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0x99;
    sobj->sprite.green = 0x99;
    sobj->sprite.blue  = 0x99;

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;
}

/* mnoption.c:562-597 0x80132618, 0x8013275C and 0x80132794, all three
 * "Unused?" per the decomp's own comments and never called from any
 * reachable path -- unlike func_ovl61_80131D54 (mndata.c's one ported
 * dead function, an empty `return;`), the first of these draws a raw
 * GBI rectangle nothing else in this file needs translated, and the
 * other two only eject D_ovl60_801337D4, which no reachable code ever
 * sets to non-NULL. Omitted; D_ovl60_801337D4 itself is kept below
 * since mnOptionInitVars writes it. */

/* mnoption.c:599-603 0x801327CC - Unused?, verbatim, and never called. */
void func_ovl60_801327CC(void)
{
    return;
}

/* mnoption.c:605-643 0x801327D4, the RDP commands in the port's spelling.
 * The underline sits under whichever half of the toggle is picked
 * (rect[0] is STEREO's, rect[1] is MONO's), and only draws at all when
 * SOUND is the highlighted tab; the four rectangle corners are the
 * decomp's unchanged, its own G_CYC_FILL fill needing no correction
 * unlike mnOptionLabelsProcDisplay's G_CYC_1CYCLE one. The decomp's own
 * `unused` rectangle pair (0x801336CC) is dead data alongside it, and is
 * not ported. */
void mnOptionSoundUnderlineProcDisplay(GObj *gobj)
{
    // 0x801336AC
    SYRectangle rect[/* */] =
    {
        { 233,  64, 273,  64 },
        { 179,  64, 225,  64 }
    };

    (void)gobj;

    lbCommonClearExternSpriteParams();

    if (sMNOptionOption == nMNOptionOptionSound)
    {
        lbCommonSpriteFillRect
        (
            rect[sMNOptionSoundMonoOrStereo].ulx,
            rect[sMNOptionSoundMonoOrStereo].uly,
            rect[sMNOptionSoundMonoOrStereo].lrx,
            rect[sMNOptionSoundMonoOrStereo].lry,
            0xFF, 0xFF, 0xFF, 0xFF
        );
    }
}

/* mnoption.c:645-662 0x801329F4, verbatim. */
void mnOptionMakeSoundUnderline(void)
{
    gcAddGObjDisplay
    (
        gcMakeGObjSPAfter
        (
            0,
            NULL,
            5,
            GOBJ_PRIORITY_DEFAULT
        ),
        mnOptionSoundUnderlineProcDisplay,
        3,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
}

/* mnoption.c:664-786 0x80132A40, 0x80132AE0, 0x80132B80, 0x80132C20 and
 * 0x80132CC0, verbatim: five sprite cameras over the same viewport, one
 * per display link, drawn back to front by their DL priorities -- the
 * decals (link 0) at 80, the labels (1) at 60, the option tabs (2) at
 * 40, the sound underline (3) at 20 and the sound toggle text (4) at 10.
 * mndata.c's DATA menu has the same first four; this screen's own fifth
 * layer is the underline's own text, drawn last so it sits over the
 * underline. */
void mnOptionMakeSoundUnderlineCamera(void)
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

void mnOptionMakeLink4Camera(void)
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
            10,
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

void mnOptionMakeOptionsCamera(void)
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

void mnOptionMakeLabelsCamera(void)
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

void mnOptionMakeDecalsCamera(void)
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

/* mnoption.c:789-815 0x80132D60, verbatim: the cursor starts on the tab
 * the player came back from, the same accident-of-design mndata.c's
 * mnDataInitVars documents -- the scene manager's default arm sets
 * scene_prev to the screen that was asked for, so a bounce off either
 * unported child lands the cursor exactly where the game would put it. */
void mnOptionInitVars(void)
{
    switch (gSCManagerSceneData.scene_prev)
    {
    case nSCKindScreenAdjust:
        sMNOptionOption = nMNOptionOptionScreenAdjust;
        break;

    case nSCKindBackupClear:
        sMNOptionOption = nMNOptionOptionBackupClear;
        break;

    default:
        sMNOptionOption = nMNOptionOptionSound;
        break;
    }
    sMNOptionOptionChangeWait = 0;

    sMNOptionSoundMonoOrStereo = (dSYAudioSoundQuality == 1) ? 1 : 0;

    sMNOptionIsScreenFlash = gSCManagerBackupData.is_allow_screenflash;
    sMNOptionTotalTimeTics = 0;
    D_ovl60_801337D4 = NULL;
    sMNOptionIsProceedScene = FALSE;
    sMNOptionReturnTic = sMNOptionTotalTimeTics + I_MIN_TO_TICS(5);
}

/* mnoption.c:817-824 0x80132E10, verbatim. */
void mnOptionWriteBackup(void)
{
    gSCManagerBackupData.is_allow_screenflash = sMNOptionIsScreenFlash;
    gSCManagerBackupData.sound_mono_or_stereo = sMNOptionSoundMonoOrStereo;

    lbBackupWrite();
}

/* mnoption.c:826-1025 0x80132E4C, the REGION_US arms, verbatim.
 *
 * Ten tics of deaf input, then: five minutes of no input at all goes to
 * the title; A or START on SCREEN ADJUST or BACKUP CLEAR asks for that
 * screen and sets sMNOptionIsProceedScene so the load happens next tic
 * (unlike mndata.c's three children, this screen's own SOUND option has
 * no scene to ask for -- A on it toggles the sound quality instead, at
 * the bottom of this function); B goes back to the mode select; up or
 * down steps the cursor, wrapping at both ends; and while SOUND is
 * highlighted, L/R (or the stick) or A steps or flips the mono/stereo
 * toggle and calls syAudioSetQuality with the new value. */
void mnOptionFuncRun(GObj *gobj)
{
    GObj *select_gobj;
    sb32 stick_range;

    // 0x801336EC
    GObj **option_gobjs[/* */] = { &sMNOptionOptionSoundGObj, &sMNOptionOptionScreenAdjustGObj, &sMNOptionOptionBackupClearGObj };

    s32 is_button;

    (void)gobj;
    (void)select_gobj;

    sMNOptionTotalTimeTics++;

    if (sMNOptionTotalTimeTics >= 10)
    {
        if (sMNOptionTotalTimeTics == sMNOptionReturnTic)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            mnOptionWriteBackup();
            syTaskmanSetLoadScene();

            return;
        }
        if (scSubsysControllerCheckNoInputAll() == FALSE)
        {
            sMNOptionReturnTic = sMNOptionTotalTimeTics + I_MIN_TO_TICS(5);
        }
        if (sMNOptionIsProceedScene != FALSE)
        {
            syTaskmanSetLoadScene();
        }
        if (sMNOptionOptionChangeWait != 0)
        {
            sMNOptionOptionChangeWait--;
        }
        if
        (
            (scSubsysControllerGetPlayerStickInRangeLR(-20, 20) != FALSE)           &&
            (scSubsysControllerGetPlayerStickInRangeUD(-20, 20) != FALSE)           &&
            (scSubsysControllerGetPlayerHoldButtons(U_JPAD | U_CBUTTONS) == FALSE)  &&
            (scSubsysControllerGetPlayerHoldButtons(D_JPAD | D_CBUTTONS) == FALSE)
        )
        {
            sMNOptionOptionChangeWait = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | START_BUTTON) != FALSE)
        {
            switch (sMNOptionOption)
            {
            case nMNOptionOptionScreenAdjust:
                mnOptionWriteBackup();

                func_800269C0_275C0(nSYAudioFGMMenuSelect);

                mnOptionSetOptionSpriteColors(*option_gobjs[sMNOptionOption], nMNOptionTabStatusSelected);

                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKindScreenAdjust;

                sMNOptionIsProceedScene = TRUE;
                return;

            case nMNOptionOptionBackupClear:
                mnOptionWriteBackup();

                func_800269C0_275C0(nSYAudioFGMMenuSelect);

                mnOptionSetOptionSpriteColors(*option_gobjs[sMNOptionOption], nMNOptionTabStatusSelected);

                gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
                gSCManagerSceneData.scene_curr = nSCKindBackupClear;

                sMNOptionIsProceedScene = TRUE;
                return;
            }
        }
        if (scSubsysControllerGetPlayerTapButtons(B_BUTTON) != FALSE)
        {
            mnOptionWriteBackup();

            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindModeSelect;

            syTaskmanSetLoadScene();
        }
        if
        (
            mnOptionCheckGetOptionButtonInput(is_button, U_JPAD | U_CBUTTONS) ||
            mnOptionCheckGetOptionStickInputUD(stick_range, 20, 1)
        )
        {
            func_800269C0_275C0(nSYAudioFGMMenuScroll2);

            mnOptionSetOptionChangeWaitP(is_button, stick_range, 7);

            mnOptionSetOptionSpriteColors(*option_gobjs[sMNOptionOption], nMNOptionTabStatusNot);

            if (sMNOptionOption == nMNOptionOptionStart)
            {
                sMNOptionOption = nMNOptionOptionEnd;
            }
            else sMNOptionOption--;

            mnOptionSetOptionSpriteColors(*option_gobjs[sMNOptionOption], nMNOptionTabStatusHighlight);
            gcEjectGObj(sMNOptionMenuGObj);
            mnOptionMakeMenuGObj();
        }
        if
        (
            mnOptionCheckGetOptionButtonInput(is_button, D_JPAD | D_CBUTTONS) ||
            mnOptionCheckGetOptionStickInputUD(stick_range, -20, 0)
        )
        {
            func_800269C0_275C0(nSYAudioFGMMenuScroll2);

            mnOptionSetOptionChangeWaitN(is_button, stick_range, 7);

            if (sMNOptionOption == nMNOptionOptionScreenAdjust)
            {
                sMNOptionOptionChangeWait += 8;
            }
            mnOptionSetOptionSpriteColors(*option_gobjs[sMNOptionOption], nMNOptionTabStatusNot);

            if (sMNOptionOption == nMNOptionOptionEnd)
            {
                sMNOptionOption = nMNOptionOptionStart;
            }
            else sMNOptionOption++;

            mnOptionSetOptionSpriteColors(*option_gobjs[sMNOptionOption], nMNOptionTabStatusHighlight);
            gcEjectGObj(sMNOptionMenuGObj);
            mnOptionMakeMenuGObj();
        }
        if (sMNOptionOption == nMNOptionOptionSound)
        {
            if
            (
                (scSubsysControllerGetPlayerTapButtons(L_JPAD | L_TRIG | L_CBUTTONS) != FALSE) ||
                (mnOptionCheckGetOptionStickInputLR(stick_range, -20, 0))
            )
            {
                if (sMNOptionSoundMonoOrStereo == 0)
                {
                    func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                    sMNOptionSoundMonoOrStereo = 1;

                    gcEjectGObj(sMNOptionSoundOptionGObj);
                    mnOptionMakeSoundToggle();

                    sMNOptionOptionChangeWait = mnCommonGetOptionChangeWaitN(stick_range, 7);

                    gcEjectGObj(sMNOptionMenuGObj);
                    mnOptionMakeMenuGObj();
                    syAudioSetQuality(sMNOptionSoundMonoOrStereo);
                }
            }
            if
            (
                (scSubsysControllerGetPlayerTapButtons(R_JPAD | R_TRIG | R_CBUTTONS) != FALSE) ||
                (mnOptionCheckGetOptionStickInputLR(stick_range, 20, 1))
            )
            {
                if (sMNOptionSoundMonoOrStereo == 1)
                {
                    func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                    sMNOptionSoundMonoOrStereo = 0;

                    gcEjectGObj(sMNOptionSoundOptionGObj);
                    mnOptionMakeSoundToggle();

                    sMNOptionOptionChangeWait = mnCommonGetOptionChangeWaitP(stick_range, 7);

                    gcEjectGObj(sMNOptionMenuGObj);
                    mnOptionMakeMenuGObj();
                    syAudioSetQuality(sMNOptionSoundMonoOrStereo);
                }
            }
            if (scSubsysControllerGetPlayerTapButtons(A_BUTTON) != FALSE)
            {
                func_800269C0_275C0(nSYAudioFGMMenuScroll1);

                if (sMNOptionSoundMonoOrStereo == 1)
                {
                    sMNOptionSoundMonoOrStereo = 0;
                }
                else sMNOptionSoundMonoOrStereo = 1;

                gcEjectGObj(sMNOptionSoundOptionGObj);
                mnOptionMakeSoundToggle();
                gcEjectGObj(sMNOptionMenuGObj);
                mnOptionMakeMenuGObj();
                syAudioSetQuality(sMNOptionSoundMonoOrStereo);
            }
        }
    }
}

/* mnoption.c:1027-1042 lbRelocInitSetup and :1042 lbRelocLoadFilesListed,
 * as every other menu scene has them (src/dc/mndata.c): two sprite
 * banks stand in for the two relocData files, loaded out of the romdisk
 * into the scene heap and VRAM. */
static void mnOptionLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMNOptionFileIDs)] =
    {
        MNOPTION_BANK_COMMON,
        MNOPTION_BANK_OPTION
    };
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(dMNOptionFileIDs); i++)
    {
        if (sprite_bank_load(&sMNOptionBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mnOption: no bank for file %d (%s)\n",
                          (int)dMNOptionFileIDs[i], paths[i]);
            sMNOptionFiles[i] = NULL;
            continue;
        }
        sMNOptionFiles[i] = &sMNOptionBanks[i];
    }
}

/* mnoption.c:1027-1065 0x8013346C, in the game's order.
 *
 * DIVERGES: the LBRelocSetup block is mnOptionLoadFiles above, and
 * gcMakeDefaultCameraGObj -- the black clear camera on link 0 at DL
 * priority 100 -- is the frame clear, which the PVR does itself (the
 * same cut as src/dc/mndata.c's). */
void mnOptionFuncStart(void)
{
    mnOptionLoadFiles();

    gcMakeGObjSPAfter(0, mnOptionFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

    mnOptionInitVars();
    mnOptionMakeDecalsCamera();
    mnOptionMakeLabelsCamera();
    mnOptionMakeOptionsCamera();
    mnOptionMakeSoundUnderlineCamera();
    mnOptionMakeLink4Camera();
    mnOptionMakeDecals();
    mnOptionMakeLabels();
    mnOptionMakeSound();
    mnOptionMakeSoundToggle();
    mnOptionMakeScreenAdjust();
    mnOptionMakeBackupClear();
    mnOptionMakeSoundUnderline();
    mnOptionMakeMenuGObj();

    if (gSCManagerSceneData.scene_prev == nSCKindScreenAdjust)
    {
        syAudioPlayBGM(0, nSYAudioBGMModeSelect);
    }
}

/* mnoption.c:1067 dMNOptionVideoSetup is the N64's video mode: see
 * mnOptionStartScene. */

/* mnoption.c:1070-1113 (0x80133714). The pool counts are the game's, all
 * zero: a menu scene takes its objects straight from the scene heap.
 * DIVERGES as every other menu scene's: the arena is the port's region,
 * the draw is the scene manager's, and mnOptionFuncLights is dropped. */
SYTaskmanSetup dMNOptionTaskmanSetup =
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

    mnOptionFuncStart                   // Task start function
};

/* The bzero arm of syDmaLoadOverlay for overlay 60, this file:
 * src/dc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnOptionOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNOptionOptionSoundGObj);
    OVERLAY_CLEAR(sMNOptionOptionScreenAdjustGObj);
    OVERLAY_CLEAR(sMNOptionOptionBackupClearGObj);
    OVERLAY_CLEAR(sMNOptionOption);
    OVERLAY_CLEAR(sMNOptionSoundMonoOrStereo);
    OVERLAY_CLEAR(sMNOptionIsScreenFlash);
    OVERLAY_CLEAR(sMNOptionSoundOptionGObj);
    OVERLAY_CLEAR(sMNOptionMenuGObj);
    OVERLAY_CLEAR(D_ovl60_801337D4);
    OVERLAY_CLEAR(D_ovl60_801337D8);
    OVERLAY_CLEAR(sMNOptionIsProceedScene);
    OVERLAY_CLEAR(sMNOptionOptionChangeWait);
    OVERLAY_CLEAR(sMNOptionTotalTimeTics);
    OVERLAY_CLEAR(sMNOptionReturnTic);
    OVERLAY_CLEAR(sMNOptionFiles);
    OVERLAY_CLEAR(sMNOptionBanks);
}

/* mnoption.c:1115-1123 0x801335C0. DIVERGES: syVideoInit, the zbuffer and
 * the arena_size line are the N64's video mode and its link map, set
 * once at boot here as in every other scene. */
void mnOptionStartScene(void)
{
    syTaskmanStartTask(&dMNOptionTaskmanSetup);
}
