/* dcmemcard.c -- see dcmemcard.h. The port's own scene: nothing here is
 * the decomp's but the calls it makes and the shapes it borrows, which
 * say where they come from. */
#include "dcmemcard.h"
#include "scport.h"
#include "dcstrings.h"
#include "dctext.h"
#include "vmusave.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"

#include <stdio.h>
#include <string.h>

#include <sys/debug.h>
#include <sys/controller.h>
#include <sys/rdp.h>
#include <sc/scsubsys/scsubsys.h>
#include <gm/gmsound.h>
#include <mn/mndef.h>
#include <macros.h>

#ifndef FT_HOSTTEST
#include <arch/timer.h>
#endif

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/mnbackupclear.c's own copy of this declaration. */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* The Backup Clear screen's header file (relocData 78, src/dc/
 * mnbackupclear.c's [2]) holds the "OPTION" plate every child of Options
 * wears in its corner; this page is one more child. */
#define DCMEMCARD_BANK_HEADER "mnbackupclearheader.spr"
#define llMNBackupClearHeaderOptionSprite 0x00b40

/* mnbackupclear.c's own wait macros, over this scene's wait */
#define dcMemCardCheckGetOptionButtonInput(is_button, mask) \
    mnCommonCheckGetOptionButtonInput(sDCMemCardChangeWait, is_button, mask)
#define dcMemCardCheckGetOptionStickInputUD(stick_range, min, b) \
    mnCommonCheckGetOptionStickInputUD(sDCMemCardChangeWait, stick_range, min, b)
#define dcMemCardSetOptionChangeWaitP(is_button, stick_range, div) \
    mnCommonSetOptionChangeWaitP(sDCMemCardChangeWait, is_button, stick_range, div)
#define dcMemCardSetOptionChangeWaitN(is_button, stick_range, div) \
    mnCommonSetOptionChangeWaitN(sDCMemCardChangeWait, is_button, stick_range, div)

/* Where things go, in the game's pixels (320x240): the title where Backup
 * Clear's sits (mnbackupclear.c's 133, 22), the rows where its tabs start
 * (y 54) and at its step (27) while six fit, closer for seven or eight;
 * a row's name on the left and its two lines of detail beside it. A name
 * is about 122 wide and the longest detail ("Saved 2026/09/24 12:00")
 * about 112, so the detail ends inside the title-safe 288. */
#define DCMEMCARD_TITLE_X  133
#define DCMEMCARD_TITLE_Y  22
#define DCMEMCARD_ROW_Y    54
#define DCMEMCARD_ROW_STEP 27
#define DCMEMCARD_ROW_TIGHT 20
#define DCMEMCARD_NAME_X   32
#define DCMEMCARD_INFO_X   174
#define DCMEMCARD_BACK_X   296
#define DCMEMCARD_BACK_Y   210
#define DCMEMCARD_EMPTY_Y  100
/* the boot check: its message, and its answers' first row */
#define DCMEMCARD_BOOT_Y     96
#define DCMEMCARD_BOOT_ANS_Y 140
/* tics the "creating" message is drawn before the write blocks the loop */
#define DCMEMCARD_BOOT_DRAW_TICS 4

/* Colours: the title Backup Clear's (0xF2C70D), the names its tabs'
 * (mnBackupClearUpdateOptionTabColors: 0xFFA800 on the cursor, 0x7D4507
 * off it), the details the port's notice lavender. */
#define DCMEMCARD_RGB_TITLE 0xF2C70D
#define DCMEMCARD_RGB_HI    0xFFA800
#define DCMEMCARD_RGB_NOT   0x7D4507
#define DCMEMCARD_RGB_INFO  0xB7BCEC

/* Above every sprite the scene draws (lbcommon.h's LB_SPRITE_Z_BASE band
 * starts at 1.0), below the port's notice (vmunotice.h). */
#define DCMEMCARD_Z 100.0F

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

VMUCardInfo sDCMemCardCards[VMUCARD_MAX];
s32 sDCMemCardCount;
s32 sDCMemCardCursor;
s32 sDCMemCardInUse;

static s32 sDCMemCardChangeWait;
static SpriteBank sDCMemCardHeaderBank;
static sb32 sDCMemCardHeaderLoaded;

/* The boot check (dcMemCardBootBegin): asked for before the first scene,
 * seen by FuncStart, which then runs the scene as the check and not as the
 * page. The scene the game was about to start is kept to go back to. */
enum
{
    nDCMemCardBootAsk,      /* the question, waiting for an answer */
    nDCMemCardBootWrite,    /* the file is being made */
    nDCMemCardBootFailed    /* it could not be */
};

static sb32 sDCMemCardBootPending;
static sb32 sDCMemCardBoot;
static s32 sDCMemCardBootReturnCurr;
static s32 sDCMemCardBootReturnPrev;
static s32 sDCMemCardBootStage;
static s32 sDCMemCardBootSel;
static s32 sDCMemCardBootWait;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

void dcMemCardRowText(s32 row, char *name, char *save, char *room)
{
    const VMUCardInfo *c = &sDCMemCardCards[row];

    snprintf(name, DCMEMCARD_LINE, gDCStrings[nDCStrCardName],
             'A' + c->port, c->unit);

    switch (c->state)
    {
    case nVMUCardValid:
        snprintf(save, DCMEMCARD_LINE, gDCStrings[nDCStrCardSaved],
                 c->year, c->month, c->day, c->hour, c->min);
        break;

    case nVMUCardCorrupt:
        snprintf(save, DCMEMCARD_LINE, "%s", gDCStrings[nDCStrCardDamaged]);
        break;

    default:
        snprintf(save, DCMEMCARD_LINE, "%s", gDCStrings[nDCStrCardNoSave]);
        break;
    }

    if (c->free_blocks < 0)
    {
        room[0] = '\0';
    }
    else
    {
        snprintf(room, DCMEMCARD_LINE,
                 gDCStrings[(row == sDCMemCardInUse) ? nDCStrCardFreeInUse
                                                     : nDCStrCardFree],
                 c->free_blocks);
    }
}

static s32 dcMemCardRowY(s32 row)
{
    return DCMEMCARD_ROW_Y +
           row * ((sDCMemCardCount <= 6) ? DCMEMCARD_ROW_STEP : DCMEMCARD_ROW_TIGHT);
}

/* What the boot check says and offers. `lines` are the two lines of the
 * message, `answers` the strings the player picks from; returns how many
 * answers there are. The VMU is the one the store is backed by, or A1 when
 * there is none (the message that needs no name does not use it). */
static s32 dcMemCardBootText(char lines[2][DCMEMCARD_LINE],
                             const char *answers[2])
{
    int port = 0, unit = 1;
    s32 state = sy_sram_boot_state();

    (void)sy_sram_card(&port, &unit);

    if (sDCMemCardBootStage == nDCMemCardBootWrite)
    {
        snprintf(lines[0], DCMEMCARD_LINE, "%s", gDCStrings[nDCStrCreating]);
        snprintf(lines[1], DCMEMCARD_LINE, "%s", gDCStrings[nDCStrCreatingWarn]);
        return 0;
    }
    if (sDCMemCardBootStage == nDCMemCardBootFailed)
    {
        snprintf(lines[0], DCMEMCARD_LINE, "%s", gDCStrings[nDCStrFailed]);
        snprintf(lines[1], DCMEMCARD_LINE, gDCStrings[nDCStrFailed2],
                 'A' + port, unit);
        answers[0] = gDCStrings[nDCStrActWithout];
        return 1;
    }
    switch (state)
    {
    case nSYSramBootNoSave:
        snprintf(lines[0], DCMEMCARD_LINE, gDCStrings[nDCStrNoSave],
                 'A' + port, unit);
        snprintf(lines[1], DCMEMCARD_LINE, gDCStrings[nDCStrNoSave2],
                 sy_sram_boot_need_blocks());
        answers[0] = gDCStrings[nDCStrActCreate];
        answers[1] = gDCStrings[nDCStrActWithout];
        return 2;

    case nSYSramBootDamaged:
        snprintf(lines[0], DCMEMCARD_LINE, gDCStrings[nDCStrDamaged],
                 'A' + port, unit);
        snprintf(lines[1], DCMEMCARD_LINE, "%s", gDCStrings[nDCStrDamaged2]);
        answers[0] = gDCStrings[nDCStrActOverwrite];
        answers[1] = gDCStrings[nDCStrActWithout];
        return 2;

    case nSYSramBootNoSpace:
        snprintf(lines[0], DCMEMCARD_LINE, gDCStrings[nDCStrNoSpace],
                 'A' + port, unit);
        snprintf(lines[1], DCMEMCARD_LINE, gDCStrings[nDCStrNoSpace2],
                 sy_sram_boot_need_blocks(), sy_sram_boot_free_blocks());
        answers[0] = gDCStrings[nDCStrActWithout];
        return 1;

    default:
        snprintf(lines[0], DCMEMCARD_LINE, "%s", gDCStrings[nDCStrNoCard]);
        snprintf(lines[1], DCMEMCARD_LINE, "%s", gDCStrings[nDCStrNoCard2]);
        answers[0] = gDCStrings[nDCStrContinue];
        return 1;
    }
}

/* The check's page: the title, the two lines, the answers under them with
 * the cursor's colour on the one the A button takes. Centred, in the
 * game's pixels scaled by the screen's, like the page's empty state. */
static void dcMemCardBootDisplay(void)
{
    f32 k = gDCScreenScale;
    char lines[2][DCMEMCARD_LINE];
    const char *answers[2];
    s32 n = dcMemCardBootText(lines, answers);
    s32 i;

    dctext_draw(gDCStrings[nDCStrTitle],
                ((320 * k) - (dctext_width(gDCStrings[nDCStrTitle]) * k)) / 2,
                DCMEMCARD_TITLE_Y * k, k, DCMEMCARD_Z,
                0xFF000000 | DCMEMCARD_RGB_TITLE);
    for (i = 0; i < 2; i++)
    {
        dctext_draw(lines[i], ((320 * k) - dctext_width(lines[i])) / 2,
                    ((DCMEMCARD_BOOT_Y * k) + (i * DCTEXT_LINE)), 1.0F,
                    DCMEMCARD_Z, 0xFF000000 | DCMEMCARD_RGB_INFO);
    }
    for (i = 0; i < n; i++)
    {
        dctext_draw(answers[i],
                    ((320 * k) - (dctext_width(answers[i]) * k)) / 2,
                    (DCMEMCARD_BOOT_ANS_Y + (i * DCMEMCARD_ROW_STEP)) * k, k,
                    DCMEMCARD_Z,
                    0xFF000000 | ((i == sDCMemCardBootSel) ? DCMEMCARD_RGB_HI
                                                           : DCMEMCARD_RGB_NOT));
    }
}

/* The page's words, in framebuffer pixels over the scene's sprites: the
 * overlay's way of drawing (dctext_draw), from a display proc on the
 * scene's own camera, which calls it on the translucent pass. Positions
 * are the game's pixels scaled by the screen's; the title and the names
 * are a game pixel a font pixel, the details a framebuffer pixel one, the
 * size the credits are. */
static void dcMemCardProcDisplay(GObj *gobj)
{
    f32 k = gDCScreenScale;
    char name[DCMEMCARD_LINE], save[DCMEMCARD_LINE], room[DCMEMCARD_LINE];
    s32 i;

    (void)gobj;

    if (sDCMemCardBoot != FALSE)
    {
        dcMemCardBootDisplay();
        return;
    }
    dctext_draw(gDCStrings[nDCStrTitle], DCMEMCARD_TITLE_X * k,
                DCMEMCARD_TITLE_Y * k, k, DCMEMCARD_Z,
                0xFF000000 | DCMEMCARD_RGB_TITLE);

    if (sDCMemCardCount == 0)
    {
        s32 j;

        for (j = 0; j < 2; j++)
        {
            const char *s = gDCStrings[(j == 0) ? nDCStrNoCard : nDCStrNoCard2];

            dctext_draw(s, ((320 * k) - dctext_width(s)) / 2,
                        (DCMEMCARD_EMPTY_Y * k) + (j * DCTEXT_LINE),
                        1.0F, DCMEMCARD_Z, 0xFF000000 | DCMEMCARD_RGB_INFO);
        }
    }
    for (i = 0; i < sDCMemCardCount; i++)
    {
        f32 y = dcMemCardRowY(i) * k;

        dcMemCardRowText(i, name, save, room);
        dctext_draw(name, DCMEMCARD_NAME_X * k, y, k, DCMEMCARD_Z,
                    0xFF000000 | ((i == sDCMemCardCursor) ? DCMEMCARD_RGB_HI
                                                          : DCMEMCARD_RGB_NOT));
        dctext_draw(save, DCMEMCARD_INFO_X * k, y, 1.0F, DCMEMCARD_Z,
                    0xFF000000 | DCMEMCARD_RGB_INFO);
        dctext_draw(room, DCMEMCARD_INFO_X * k, y + DCTEXT_LINE, 1.0F,
                    DCMEMCARD_Z, 0xFF000000 | DCMEMCARD_RGB_INFO);
    }
    dctext_draw(gDCStrings[nDCStrBack],
                (DCMEMCARD_BACK_X * k) - dctext_width(gDCStrings[nDCStrBack]),
                DCMEMCARD_BACK_Y * k, 1.0F, DCMEMCARD_Z,
                0xFF000000 | DCMEMCARD_RGB_INFO);
}

/* mnbackupclear.c:219-254 mnBackupClearMakeHeaderSObjs's first half: the
 * "OPTION" plate, in its colour and place. The second half is Backup
 * Clear's own title sprite; this page's is words (dcMemCardProcDisplay). */
static void dcMemCardMakeHeader(void)
{
    GObj *gobj;
    SObj *sobj;
    Sprite *sprite;

    if (sDCMemCardHeaderLoaded == FALSE)
    {
        return;
    }
    sprite = sprite_bank_get(&sDCMemCardHeaderBank, llMNBackupClearHeaderOptionSprite);
    if (sprite == NULL)
    {
        return;
    }
    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 0, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, sprite);

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0x5F;
    sobj->sprite.green = 0x58;
    sobj->sprite.blue = 0x46;

    sobj->pos.x = 24.0F;
    sobj->pos.y = 17.0F;
}

/* the words' GObj, on the camera's second link */
static void dcMemCardMakeText(void)
{
    GObj *gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, dcMemCardProcDisplay, 1, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mnbackupclear.c:486-510 mnBackupClearMakeMainCamera, the same camera. */
static void dcMemCardMakeCamera(void)
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

/* Every card, once. The writer finishes first, so the page shows what
 * the cards hold rather than what they held before a write it would
 * otherwise read across. */
static void dcMemCardScan(void)
{
#ifndef FT_HOSTTEST
    uint64_t t0 = timer_ms_gettime64();
#endif
    int port, unit;
    s32 i;

    sy_sram_quiesce();
    sDCMemCardCount = vmucard_scan(sDCMemCardCards, VMUCARD_MAX);
    sDCMemCardInUse = -1;
    if (sy_sram_card(&port, &unit) != FALSE)
    {
        for (i = 0; i < sDCMemCardCount; i++)
        {
            if ((sDCMemCardCards[i].port == port) &&
                (sDCMemCardCards[i].unit == unit))
            {
                sDCMemCardInUse = i;
            }
        }
    }
    for (i = 0; i < sDCMemCardCount; i++)
    {
        const VMUCardInfo *c = &sDCMemCardCards[i];

        syDebugPrintf("dcmemcard: %c%d %s%s%s, %d blocks free%s\n",
                      'a' + c->port, c->unit,
                      (c->state == nVMUCardValid) ? "valid"
                      : (c->state == nVMUCardCorrupt) ? "damaged" : "no save",
                      (c->why != NULL) ? " -- " : "",
                      (c->why != NULL) ? c->why : "",
                      c->free_blocks, (i == sDCMemCardInUse) ? ", in use" : "");
    }
#ifndef FT_HOSTTEST
    syDebugPrintf("dcmemcard: %d card%s read in %u ms\n", (int)sDCMemCardCount,
                  (sDCMemCardCount == 1) ? "" : "s",
                  (unsigned)(timer_ms_gettime64() - t0));
#else
    syDebugPrintf("dcmemcard: %d card%s read\n", (int)sDCMemCardCount,
                  (sDCMemCardCount == 1) ? "" : "s");
#endif
}

/* The wait between two cursor steps counts down, and a neutral stick and
 * no direction held clears it (mnbackupclear.c:566-590). */
static void dcMemCardWaitTick(void)
{
    if (sDCMemCardChangeWait != 0)
    {
        sDCMemCardChangeWait--;
    }
    if
    (
        (scSubsysControllerGetPlayerStickInRangeLR(-20, 20) != FALSE)         &&
        (scSubsysControllerGetPlayerStickInRangeUD(-20, 20) != FALSE)         &&
        (scSubsysControllerGetPlayerHoldButtons(U_JPAD | U_CBUTTONS) == FALSE)&&
        (scSubsysControllerGetPlayerHoldButtons(D_JPAD | D_CBUTTONS) == FALSE)
    )
    {
        sDCMemCardChangeWait = 0;
    }
}

/* U/D and the stick over `count` rows with the menu's sound and wait,
 * wrapping at either end. */
static void dcMemCardMoveCursor(s32 *cursor, s32 count)
{
    s32 stick_range;
    sb32 is_button;

    if (count < 2)
    {
        return;
    }
    if
    (
        dcMemCardCheckGetOptionButtonInput(is_button, U_JPAD | U_CBUTTONS) ||
        dcMemCardCheckGetOptionStickInputUD(stick_range, 20, 1)
    )
    {
        func_800269C0_275C0(nSYAudioFGMMenuScroll2);

        dcMemCardSetOptionChangeWaitP(is_button, stick_range, 7);

        *cursor = (*cursor == 0) ? (count - 1) : (*cursor - 1);
    }
    else if
    (
        dcMemCardCheckGetOptionButtonInput(is_button, D_JPAD | D_CBUTTONS) ||
        dcMemCardCheckGetOptionStickInputUD(stick_range, -20, 0)
    )
    {
        func_800269C0_275C0(nSYAudioFGMMenuScroll2);

        dcMemCardSetOptionChangeWaitN(is_button, stick_range, 7);

        *cursor = (*cursor == (count - 1)) ? 0 : (*cursor + 1);
    }
}

/* mnbackupclear.c:566-663 mnBackupClearUpdateOptionMainMenu's shape: B
 * back to Options as that screen goes, U/D and the stick over the rows
 * with its sound and its wait. A does nothing: the page has no
 * actions yet. */
static void dcMemCardFuncRunPage(void)
{
    dcMemCardWaitTick();

    if (scSubsysControllerGetPlayerTapButtons(B_BUTTON) != FALSE)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindOption;

        syTaskmanSetLoadScene();
        return;
    }
    dcMemCardMoveCursor(&sDCMemCardCursor, sDCMemCardCount);
}

/* The boot check is over: on to the scene the game was starting, as if
 * this had never run. */
static void dcMemCardBootLeave(void)
{
    gSCManagerSceneData.scene_curr = sDCMemCardBootReturnCurr;
    gSCManagerSceneData.scene_prev = sDCMemCardBootReturnPrev;

    syTaskmanSetLoadScene();
}

/* The check's turn each tic. The question takes U/D and A (B is the safe
 * answer, the last one, "continue without saving"); an answer that makes
 * the file draws the "creating" message for a few tics first, because the
 * write blocks the frame loop until the VMU has it. */
static void dcMemCardFuncRunBoot(void)
{
    char lines[2][DCMEMCARD_LINE];
    const char *answers[2];
    s32 n;

    if (sDCMemCardBootStage == nDCMemCardBootWrite)
    {
        if (--sDCMemCardBootWait > 0)
        {
            return;
        }
        sy_sram_boot_create();

        if (sy_sram_failed() != FALSE)
        {
            sy_sram_clear_failure();
            sy_sram_boot_decline();
            sDCMemCardBootStage = nDCMemCardBootFailed;
            sDCMemCardBootSel = 0;
            return;
        }
        dcMemCardBootLeave();
        return;
    }
    dcMemCardWaitTick();

    n = dcMemCardBootText(lines, answers);
    dcMemCardMoveCursor(&sDCMemCardBootSel, n);

    if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | START_BUTTON) != FALSE)
    {
        /* the first answer of a prompt that can make the file makes it */
        if ((sDCMemCardBootStage == nDCMemCardBootAsk) && (n == 2) &&
            (sDCMemCardBootSel == 0))
        {
            sDCMemCardBootStage = nDCMemCardBootWrite;
            sDCMemCardBootWait = DCMEMCARD_BOOT_DRAW_TICS;
            return;
        }
        sy_sram_boot_decline();
        dcMemCardBootLeave();
    }
    else if ((scSubsysControllerGetPlayerTapButtons(B_BUTTON) != FALSE) && (n == 2))
    {
        sy_sram_boot_decline();
        dcMemCardBootLeave();
    }
}

static void dcMemCardFuncRun(GObj *gobj)
{
    (void)gobj;

    if (sDCMemCardBoot != FALSE)
    {
        dcMemCardFuncRunBoot();
    }
    else dcMemCardFuncRunPage();
}

/* Every static set, here rather than in an xxxOverlayLoad: this scene
 * has no overlay (dcmemcard.h). The cursor starts on the card in use. */
static void dcMemCardInitVars(void)
{
    memset(sDCMemCardCards, 0, sizeof(sDCMemCardCards));
    sDCMemCardCount = 0;
    sDCMemCardCursor = 0;
    sDCMemCardInUse = -1;
    sDCMemCardChangeWait = 0;
    sDCMemCardBoot = FALSE;
    memset(&sDCMemCardHeaderBank, 0, sizeof(sDCMemCardHeaderBank));
    sDCMemCardHeaderLoaded = FALSE;
}

void dcMemCardBootBegin(void)
{
    s32 state = sy_sram_boot_state();

    sDCMemCardBootPending = FALSE;

    if (state == nSYSramBootSaved)
    {
        return;
    }
#if defined(DB_MEMCARD_AUTO)
    /* -DDB_MEMCARD_AUTO=1 answers yes where a file can be made, 2 no
     * (src/dc/db.h): a probe with nobody at the pad */
    if ((DB_MEMCARD_AUTO == 1) &&
        ((state == nSYSramBootNoSave) || (state == nSYSramBootDamaged)))
    {
        sy_sram_boot_create();
    }
    else if (state != nSYSramBootNoCard)
    {
        sy_sram_boot_decline();
    }
    return;
#elif defined(DB_BOOT_SCENE) && !defined(DB_MEMCARD_PROBE)
    /* a probe that starts somewhere else is not a player at a console:
     * the game's default write lands in the store as it always did */
    return;
#else
    sy_sram_hold();
    sDCMemCardBootReturnCurr = gSCManagerSceneData.scene_curr;
    sDCMemCardBootReturnPrev = gSCManagerSceneData.scene_prev;
    sDCMemCardBootPending = TRUE;

    gSCManagerSceneData.scene_curr = nSCKindDCMemCard;
#endif
}

void dcMemCardFuncStart(void)
{
    sb32 boot = sDCMemCardBootPending;

    sDCMemCardBootPending = FALSE;
    dcMemCardInitVars();

    if (boot != FALSE)
    {
        /* the check: no cards to list, no plate; just the question */
        sDCMemCardBoot = TRUE;
        sDCMemCardBootStage = nDCMemCardBootAsk;
        sDCMemCardBootSel = 0;
        sDCMemCardBootWait = 0;
        gcMakeGObjSPAfter(0, dcMemCardFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
        dcMemCardMakeCamera();
        dcMemCardMakeText();
        return;
    }

    if (sprite_bank_load(&sDCMemCardHeaderBank, DCMEMCARD_BANK_HEADER) < 0)
    {
        syDebugPrintf("dcmemcard: no bank %s\n", DCMEMCARD_BANK_HEADER);
    }
    else sDCMemCardHeaderLoaded = TRUE;

    dcMemCardScan();
    if (sDCMemCardInUse >= 0)
    {
        sDCMemCardCursor = sDCMemCardInUse;
    }
    gcMakeGObjSPAfter(0, dcMemCardFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

    dcMemCardMakeCamera();
    dcMemCardMakeHeader();
    dcMemCardMakeText();
}

/* src/dc/mnbackupclear.c's dMNBackupClearTaskmanSetup, the same menu
 * scene's pools and functions: all zero pool counts, the scene heap for
 * everything, the scene manager's draw. */
SYTaskmanSetup dDCMemCardTaskmanSetup =
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

    dcMemCardFuncStart                  // Task start function
};

void dcMemCardStartScene(void)
{
    syTaskmanStartTask(&dDCMemCardTaskmanSetup);
}
