/* hosttest/memcard.c -- part of hosttest_ft.c: the memory card's scene
 * (src/dc/dcmemcard.h).
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* ---- the MEMORY CARD page ----------------------------------------------
 *
 * The port's own scene, run the way test_backupclear_menu runs Backup
 * Clear -- the real task loop, a tic hook for the pad -- over the host's
 * fake cards (src/dc/vmucard.c). What is proved, with none, one and three
 * cards in the machine: the scene reads every card when it starts and
 * says of each what the scan did; it knows which one the store is
 * backed by and starts its cursor there; the cursor walks and wraps; B
 * goes back to Options as Backup Clear does; every line it draws is
 * made of glyphs the font has; and the "SAVING..." box stays down while
 * it runs, since the page speaks for the card itself.
 */
static s32 gMemCardPressTic[3];
static s32 gMemCardCursorSeen[3];
static int gMemCardNoticeChecked;

static void memcard_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    if (tic == 2)
    {
        /* the notice would be up (a save was published a moment ago on
         * the host's clock), and it is not, on this page */
        if (sy_sram_notice_visible() != 0)
        {
            CHECK(vmunotice_visible() == 0);
            gMemCardNoticeChecked = 1;
        }
    }
    if (tic == gMemCardPressTic[0])
    {
        gMemCardCursorSeen[0] = sDCMemCardCursor;
        pad->button_hold = D_JPAD;
    }
    if (tic == gMemCardPressTic[1])
    {
        gMemCardCursorSeen[1] = sDCMemCardCursor;
        pad->button_hold = U_JPAD;
    }
    if (tic == gMemCardPressTic[2])
    {
        gMemCardCursorSeen[2] = sDCMemCardCursor;
        pad->button_tap = B_BUTTON;
    }
}

/* the scene, from Options, until its B */
static void memcard_run(s32 down, s32 up, s32 back)
{
    gMemCardPressTic[0] = down;
    gMemCardPressTic[1] = up;
    gMemCardPressTic[2] = back;
    gMemCardCursorSeen[0] = gMemCardCursorSeen[1] = gMemCardCursorSeen[2] = -1;

    sprite_bank_release_all();
    gSCManagerSceneData.scene_prev = nSCKindOption;
    gSCManagerSceneData.scene_curr = nSCKindDCMemCard;

    gSceneTicHook = memcard_tic;
    dcMemCardStartScene();
    gSceneTicHook = NULL;

    CHECK(gSCManagerSceneData.scene_prev == nSCKindDCMemCard);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindOption);
}

/* every line a row draws, and that each is made of the font's glyphs */
static void memcard_rows(char name[][DCMEMCARD_LINE],
                         char save[][DCMEMCARD_LINE],
                         char room[][DCMEMCARD_LINE])
{
    s32 i;

    for (i = 0; i < sDCMemCardCount; i++)
    {
        const char *lines[3];
        s32 j;

        dcMemCardRowText(i, name[i], save[i], room[i]);
        lines[0] = name[i];
        lines[1] = save[i];
        lines[2] = room[i];
        for (j = 0; j < 3; j++)
        {
            const char *s;

            for (s = lines[j]; *s != '\0'; s++)
            {
                CHECK(dctext_index(*s) != DCTEXT_NO_GLYPH);
            }
        }
    }
}


/* ---- the boot check (dcMemCardBootBegin) -------------------------------
 *
 * The same scene, run in front of the first scene the way
 * scManagerRunScene runs it: what the store found decides whether it asks
 * anything; the question holds the store's writes until it is answered;
 * yes puts the file on the VMU before the scene it interrupted starts, no
 * leaves every VMU alone; and the interrupted scene is the one that
 * starts, whichever was answered.
 */
static s32 gBootDownTic, gBootPressTic;
static u16 gBootButton;
static uint32_t gBootWrites0;   /* writes made before the run */

/* writes the run itself has made, landed or not */
static uint32_t memcard_boot_writes(void)
{
    return sy_sram_host_writes() - gBootWrites0;
}

static void memcard_boot_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    if (tic == gBootDownTic)
    {
        pad->button_hold = D_JPAD;
    }
    /* again every dozen tics: the prompt that follows a failed write
     * needs a second press */
    if ((tic >= gBootPressTic) && (((tic - gBootPressTic) % 12) == 0))
    {
        pad->button_tap = gBootButton;
    }
}

/* scManagerInitData over the machine as it is, then the check, answered
 * by `button` at tic `press` (the cursor taken down a row at `down`, -1
 * for never).
 * Returns whether the check came up. */
static int memcard_boot_run(s32 down, s32 press, u16 button)
{
    int asked;

    gBootWrites0 = sy_sram_host_writes();
    scManagerInitData();
    sprite_bank_release_all();
    gSCManagerSceneData.scene_curr = nSCKindStartup;
    gSCManagerSceneData.scene_prev = nSCKindTitle;

    dcMemCardBootBegin();
    asked = (gSCManagerSceneData.scene_curr == nSCKindDCMemCard);
    if (asked != FALSE)
    {
        /* the store's changes are held: the game's defaults sit in RAM */
        sy_sram_flush();
        CHECK(memcard_boot_writes() == 0);

        gBootDownTic = down;
        gBootPressTic = press;
        gBootButton = button;
        gSceneTicHook = memcard_boot_tic;
        dcMemCardStartScene();
        gSceneTicHook = NULL;
    }
    /* the scene it interrupted, as it was */
    CHECK(gSCManagerSceneData.scene_curr == nSCKindStartup);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindTitle);
    return asked;
}

static void test_dcmemcard_boot(void)
{
    static u8 file[3036];
    int port, unit;

    /* 1. A VMU with a save the game takes: nothing is asked. */
    vmucard_host_reset();
    vmucard_host_insert(0, 1, 200);
    sy_sram_init();
    gSCManagerBackupData = dSCManagerDefaultBackupData;
    gSCManagerBackupData.boot = 5;
    lbBackupWrite();
    syDmaReadSram(0, file, sizeof(file));
    vmucard_host_put(0, 1, file, sizeof(file), 0);
    CHECK(memcard_boot_run(-1, 0, 0) == FALSE);
    CHECK(sy_sram_boot_state() == nSYSramBootSaved);
    CHECK(sy_sram_card(&port, &unit) != FALSE);

    /* 2. No VMU: it says so, A goes on, and nothing can be saved. */
    vmucard_host_reset();
    CHECK(memcard_boot_run(-1, 3, A_BUTTON) != FALSE);
    CHECK(sy_sram_boot_state() == nSYSramBootNoCard);
    CHECK(sy_sram_card(&port, &unit) == FALSE);

    /* 3. A blank VMU, yes: the file is on it when the scene is over, and
     *    the store is still backed by it. */
    vmucard_host_reset();
    vmucard_host_insert(0, 1, 200);
    CHECK(memcard_boot_run(-1, 3, A_BUTTON) != FALSE);
    CHECK(sy_sram_boot_state() == nSYSramBootNoSave);
    CHECK(memcard_boot_writes() == 1);
    CHECK(vmucard_host_get(0, 1, file, sizeof(file)) >= 0);
    CHECK(vmucard_payload_ok(file, sizeof(file)) != FALSE);
    CHECK(sy_sram_card(&port, &unit) != FALSE && port == 0 && unit == 1);

    /* 4. The same, no (the second answer): no file, and the game's later
     *    saves go nowhere either. */
    vmucard_host_reset();
    vmucard_host_insert(0, 1, 200);
    CHECK(memcard_boot_run(3, 12, A_BUTTON) != FALSE);
    CHECK(vmucard_host_get(0, 1, file, sizeof(file)) < 0);
    CHECK(sy_sram_card(&port, &unit) == FALSE);
    lbBackupWrite();
    sy_sram_flush();
    CHECK(vmucard_host_get(0, 1, file, sizeof(file)) < 0);

    /* 5. B is no, whichever row the cursor is on. */
    vmucard_host_reset();
    vmucard_host_insert(0, 1, 200);
    CHECK(memcard_boot_run(-1, 3, B_BUTTON) != FALSE);
    CHECK(vmucard_host_get(0, 1, file, sizeof(file)) < 0);
    CHECK(sy_sram_card(&port, &unit) == FALSE);

    /* 6. A damaged file is asked about, not overwritten unasked: no leaves
     *    it as it was; yes replaces it with one the game takes. */
    vmucard_host_reset();
    vmucard_host_insert(0, 1, 200);
    vmucard_host_put(0, 1, file, sizeof(file), 1);
    CHECK(memcard_boot_run(-1, 3, B_BUTTON) != FALSE);
    CHECK(sy_sram_boot_state() == nSYSramBootDamaged);
    CHECK(memcard_boot_writes() == 0);
    vmucard_host_reset();
    vmucard_host_insert(0, 1, 200);
    vmucard_host_put(0, 1, file, sizeof(file), 1);
    CHECK(memcard_boot_run(-1, 3, A_BUTTON) != FALSE);
    CHECK(vmucard_host_get(0, 1, file, sizeof(file)) >= 0);
    CHECK(vmucard_payload_ok(file, sizeof(file)) != FALSE);

    /* 7. No room: the need and the free are said, the only answer goes on,
     *    and no write is tried. */
    vmucard_host_reset();
    vmucard_host_insert(0, 1, 3);
    CHECK(memcard_boot_run(-1, 3, A_BUTTON) != FALSE);
    CHECK(sy_sram_boot_state() == nSYSramBootNoSpace);
    CHECK(sy_sram_boot_free_blocks() == 3);
    CHECK(sy_sram_boot_need_blocks() > 3);
    CHECK(memcard_boot_writes() == 0);
    CHECK(sy_sram_card(&port, &unit) == FALSE);

    /* 8. A write that fails is said (the second prompt), and A goes on
     *    with saving off. */
    vmucard_host_reset();
    vmucard_host_insert(0, 1, 200);
    vmucard_host_fail_writes(0, 1, 1);
    CHECK(memcard_boot_run(-1, 3, A_BUTTON) != FALSE);
    CHECK(sy_sram_card(&port, &unit) == FALSE);
    CHECK(sy_sram_failed() == 0);
}

static void test_dcmemcard(void)
{
    static u8 good[3036];
    char name[VMUCARD_MAX][DCMEMCARD_LINE];
    char save[VMUCARD_MAX][DCMEMCARD_LINE];
    char room[VMUCARD_MAX][DCMEMCARD_LINE];
    int blocks;

    /* a save the game would keep, written its own way (test_save_cards) */
    vmucard_host_reset();
    sy_sram_init();
    gSCManagerBackupData = dSCManagerDefaultBackupData;
    gSCManagerBackupData.boot = 5;
    lbBackupWrite();
    syDmaReadSram(0, good, sizeof(good));
    blocks = vmucard_file_blocks(sizeof(good));

    /* 1. No card: the page says so, and B still goes home. */
    vmucard_host_reset();
    sy_sram_init();
    memcard_run(0, 0, 3);
    CHECK(sDCMemCardCount == 0);
    CHECK(sDCMemCardInUse == -1);
    CHECK(sDCMemCardCursor == 0);

    /* 2. One card with the save on it, the one the store writes to. The
     *    notice would be up from a save the moment before. */
    vmucard_host_reset();
    vmucard_host_insert(0, 1, 200);
    vmucard_host_put(0, 1, good, sizeof(good), 0);
    sy_sram_init();
    sy_sram_host_set_clock(1000);
    sy_sram_save_now();
    gMemCardNoticeChecked = 0;
    memcard_run(0, 0, 3);
    CHECK(gMemCardNoticeChecked == 1);
    CHECK(sDCMemCardCount == 1);
    CHECK(sDCMemCardInUse == 0);
    CHECK(sDCMemCardCards[0].state == nVMUCardValid);
    memcard_rows(name, save, room);
    CHECK(strcmp(name[0], "VMU A1") == 0);
    CHECK(strncmp(save[0], "Saved 1999/09/09 ", 17) == 0);
    {
        char want[DCMEMCARD_LINE];

        snprintf(want, sizeof(want), "%d blocks free - in use",
                 sDCMemCardCards[0].free_blocks);
        CHECK(strcmp(room[0], want) == 0);
    }
    /* the fake's free count is what the test gave it, file or not */
    CHECK(sDCMemCardCards[0].free_blocks == 200);
    CHECK(sDCMemCardCards[0].file_blocks == blocks);

    /* 3. Three: an empty A1, a damaged A2, the save on B1. The store is
     *    on B1, and so is the cursor; down wraps to A1, up comes back. */
    vmucard_host_reset();
    vmucard_host_insert(0, 1, 200);
    vmucard_host_insert(0, 2, 100);
    vmucard_host_insert(1, 1, 200);
    vmucard_host_put(0, 2, good, sizeof(good), 1);
    vmucard_host_put(1, 1, good, sizeof(good), 0);
    sy_sram_init();
    memcard_run(3, 5, 7);
    CHECK(sDCMemCardCount == 3);
    CHECK(sDCMemCardInUse == 2);
    CHECK(gMemCardCursorSeen[0] == 2);
    CHECK(gMemCardCursorSeen[1] == 0);
    CHECK(gMemCardCursorSeen[2] == 2);
    memcard_rows(name, save, room);
    CHECK(strcmp(name[0], "VMU A1") == 0);
    CHECK(strcmp(name[1], "VMU A2") == 0);
    CHECK(strcmp(name[2], "VMU B1") == 0);
    CHECK(strcmp(save[0], "No save data") == 0);
    CHECK(strcmp(save[1], "Damaged save data") == 0);
    CHECK(strncmp(save[2], "Saved ", 6) == 0);
    CHECK(strcmp(room[0], "200 blocks free") == 0);
    CHECK(strstr(room[2], "in use") != NULL);

    vmucard_host_reset();
    sy_sram_host_set_clock(0);
    scManagerInitData();

    test_dcmemcard_boot();
    vmucard_host_reset();
    sy_sram_host_set_clock(0);
    scManagerInitData();
}
