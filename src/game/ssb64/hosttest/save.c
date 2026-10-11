/* hosttest/save.c -- part of hosttest_ft.c: the save data and the memory card under it.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* ---- the save data ---------------------------------------
 *
 * lb/lbbackup.c is compiled unmodified out of the decomp, so what is
 * being tested here is not its arithmetic -- tools/check/backup_check.py holds
 * the defaults and the checksum against the ROM's own bytes -- but the
 * two things the port supplies under it: the store (src/dc/sysshim.c,
 * a byte array until the VMU) and the defaults (src/dc/scmanagerdata.c).
 * Between them they decide what the game boots from, and every one of
 * these assertions is a path lbBackupIsSramValid really takes on a
 * cartridge: never written, written and good, one copy gone, both gone.
 */
/* sys/audio.c:76 and sys/video.c:33-42, as mn/mnoption/mnoption.c:38
 * declares the first: sys/video.h names only the left and top offsets,
 * and sys/audio.h none of this at all, so the readers do it themselves.
 * src/dc/syaudio.c and src/dc/sysshim.c are what define them here. */
extern sb32 dSYAudioSoundQuality;
extern s16 gSYVideoOffsetLeft, gSYVideoOffsetRight;
extern s16 gSYVideoOffsetTop, gSYVideoOffsetBottom;

static void test_save_data(void)
{
    LBBackupData saved;
    u8 junk[64];
    s32 i;

    /* Nothing has been written yet, so the store is zeroes: the checksum
     * of all-zero bytes is the zero it reads back, but the signature is
     * not 666, so both copies are refused. That is a fresh cartridge. */
    CHECK(lbBackupIsSramValid() == FALSE);

    /* ...and what it left behind is the game's defaults, with the
     * checksum lbBackupWrite computed on the way out. */
    scManagerInitData();
    CHECK(gSCManagerBackupData.signature == 666);
    CHECK(gSCManagerBackupData.checksum ==
          lbBackupCreateChecksum(&gSCManagerBackupData));
    CHECK(lbBackupIsChecksumValid() != FALSE);
    CHECK(gSCManagerBackupData.is_allow_screenflash == TRUE);
    CHECK(gSCManagerBackupData.sound_mono_or_stereo == 1);
    CHECK(gSCManagerBackupData.characters_fkind == nFTKindMario);
    CHECK(gSCManagerBackupData.unlock_mask == 0);
    CHECK(gSCManagerBackupData.fighter_mask == 0);
    CHECK(gSCManagerBackupData.boot == 0);
    CHECK(gSCManagerBackupData.error_flags == 0);
    /* I_MIN_TO_TICS(60), for every character, on both bonus stages */
    for (i = 0; i < GMCOMMON_FIGHTERS_PLAYABLE_NUM; i++)
    {
        CHECK(gSCManagerBackupData.spgame_records[i].bonus1_time == 216000);
        CHECK(gSCManagerBackupData.spgame_records[i].bonus2_time == 216000);
    }
    /* the queue of unlock messages is seven "none", not seven zeroes --
     * zero is nLBBackupUnlockLuigi, and a memset left seven of him
     * queued (src/dc/db.c) */
    for (i = 0; i < nLBBackupUnlockEnumCount; i++)
    {
        CHECK(gSCManagerSceneData.unlock_messages[i] ==
              nLBBackupUnlockEnumCount);
    }
    /* lbBackupApplyOptions ran with them: stereo, and the picture where
     * the VI would have left it */
    CHECK(dSYAudioSoundQuality == 1);
    CHECK(gSYVideoOffsetLeft == 0 && gSYVideoOffsetRight == 0);
    CHECK(gSYVideoOffsetTop == 0 && gSYVideoOffsetBottom == 0);

    /* Written and good: the second read is a read, not a re-default.
     * The boot counter is the field that proves it -- change it, write,
     * lose the copy in RAM, and read it back. */
    gSCManagerBackupData.boot = 7;
    gSCManagerBackupData.fighter_mask = LBBACKUP_MASK_FIGHTER(nFTKindLuigi);
    lbBackupWrite();
    saved = gSCManagerBackupData;
    memset(&gSCManagerBackupData, 0, sizeof(gSCManagerBackupData));
    CHECK(lbBackupIsSramValid() != FALSE);
    CHECK(gSCManagerBackupData.boot == 7);
    CHECK(gSCManagerBackupData.fighter_mask ==
          LBBACKUP_MASK_FIGHTER(nFTKindLuigi));
    CHECK(memcmp(&gSCManagerBackupData, &saved, sizeof(saved)) == 0);

    /* One copy gone. lbBackupWrite lays two down -- at 0 and at
     * ALIGN(sizeof, 0x10) -- so wrecking the first has to leave the
     * second, and the read has to write the good one back over it. */
    memset(junk, 0xA5, sizeof(junk));
    syDmaWriteSram(junk, 0, sizeof(junk));
    CHECK(lbBackupIsSramValid() != FALSE);
    CHECK(gSCManagerBackupData.boot == 7);
    /* the repair: with the first copy good again, the same read passes
     * without ever reaching the second */
    memset(&gSCManagerBackupData, 0, sizeof(gSCManagerBackupData));
    CHECK(lbBackupIsSramValid() != FALSE);
    CHECK(gSCManagerBackupData.boot == 7);

    /* Both gone: back to the defaults, and written down so the next read
     * finds them. */
    syDmaWriteSram(junk, 0, sizeof(junk));
    syDmaWriteSram(junk, ALIGN(sizeof(LBBackupData), 0x10), sizeof(junk));
    CHECK(lbBackupIsSramValid() == FALSE);
    CHECK(gSCManagerBackupData.boot == 0);
    CHECK(gSCManagerBackupData.fighter_mask == 0);
    CHECK(lbBackupIsSramValid() != FALSE);

    /* lbBackupCorrectErrors, which is what a locked character costs a
     * saved pick. Nothing is unlocked, so Luigi is not a legal choice
     * and Mario is. */
    scManagerInitData();
    gSCManagerTransferBattleState.players[0].fkind = nFTKindLuigi;
    gSCManagerTransferBattleState.players[0].pkind = nFTPlayerKindCom;
    gSCManagerTransferBattleState.players[1].fkind = nFTKindMario;
    gSCManagerTransferBattleState.players[1].pkind = nFTPlayerKindCom;
    gSCManagerSceneData.maps_vsmode_gkind = nGRKindInishie;
    gSCManagerTransferBattleState.item_toggles = 0;
    gSCManagerTransferBattleState.item_appearance_rate = 0;
    lbBackupCorrectErrors();
    CHECK(gSCManagerTransferBattleState.players[0].fkind == nFTKindNull);
    CHECK(gSCManagerTransferBattleState.players[0].pkind == nFTPlayerKindMan);
    CHECK(gSCManagerTransferBattleState.players[1].fkind == nFTKindMario);
    CHECK(gSCManagerTransferBattleState.players[1].pkind == nFTPlayerKindCom);
    /* Mushroom Kingdom is behind the same gate, and the cursor falls
     * back to the stage dSCManagerDefaultSceneData names */
    CHECK(gSCManagerSceneData.maps_vsmode_gkind ==
          dSCManagerDefaultSceneData.maps_vsmode_gkind);
    /* and with the item switch locked, the item settings are the
     * default's rather than whatever was left in them */
    CHECK(gSCManagerTransferBattleState.item_toggles ==
          dSCManagerDefaultBattleState.item_toggles);
    CHECK(gSCManagerTransferBattleState.item_appearance_rate ==
          dSCManagerDefaultBattleState.item_appearance_rate);

    /* With the newcomers unlocked, the same pick stands. */
    gSCManagerBackupData.fighter_mask = LBBACKUP_CHARACTER_MASK_ALL;
    gSCManagerBackupData.unlock_mask = LBBACKUP_UNLOCK_MASK_ALL;
    gSCManagerTransferBattleState.players[0].fkind = nFTKindLuigi;
    gSCManagerTransferBattleState.players[0].pkind = nFTPlayerKindCom;
    gSCManagerSceneData.maps_vsmode_gkind = nGRKindInishie;
    gSCManagerTransferBattleState.item_toggles = 0;
    lbBackupCorrectErrors();
    CHECK(gSCManagerTransferBattleState.players[0].fkind == nFTKindLuigi);
    CHECK(gSCManagerTransferBattleState.players[0].pkind == nFTPlayerKindCom);
    CHECK(gSCManagerSceneData.maps_vsmode_gkind == nGRKindInishie);
    CHECK(gSCManagerTransferBattleState.item_toggles == 0);

    /* Leave the globals as a fresh boot, since every scene test below
     * fills in its own battle from here. */
    scManagerInitData();
}

/* ---- the memory card under the store ----------------------
 *
 * The test above is the game's half: lb/lbbackup.c over a store. This is
 * the port's half -- src/dc/vmusave.c -- and it is a deliberate
 * deviation from the N64 rather than a stand-in for it, so what it
 * asserts is the property rather than the original.
 * The console's card is a VMU and the host's is a plain file, and those
 * two are thirty-odd lines each at the bottom of that file; everything
 * above the split -- the store, the range check, the extent, the dirty
 * rule -- is the same code the Dreamcast runs, which is what makes this
 * worth running on the host at all.
 *
 * Four things are worth proving and one of them is subtle. A save must
 * outlive the session (2, 3); a machine with no card must behave exactly
 * as the port did before there was one (1); a write that cannot land
 * must be tried once and not once a frame (6). The subtle one is 5: the
 * bytes that go on the card are 3036 and not 1516, because
 * lbBackupWrite lays down *two* copies at two offsets, and the way to
 * prove the second one really went across is to destroy the first and
 * let the game's own fallback find it.
 */
static void test_save_card(void)
{
    LBBackupData saved;
    long n;
    FILE *f;

    /* lbBackupWrite's own two offsets added up (lb/lbbackup.c:39-40):
     * the second copy starts at ALIGN(sizeof, 0x10) and is sizeof
     * long. */
    n = (long)((((sizeof(LBBackupData) + 0xFu) & ~0xFu)) + sizeof(LBBackupData));
    CHECK(n == 3036);

    /* 1. No card. Every line the game runs is unchanged -- this is the
     *    port exactly as it was before the card -- and the flush that
     *    has nowhere to go still clears the mark. */
    remove(kSaveFile);
    sy_sram_set_host_path(NULL);
    sy_sram_init();
    CHECK(sy_sram_backing() == NULL);
    CHECK(sy_sram_is_dirty() == 0);
    scManagerInitData();
    CHECK(sy_sram_is_dirty() != 0);
    sy_sram_flush();
    CHECK(sy_sram_is_dirty() == 0);
    CHECK(save_file_size(kSaveFile) == -1);
    memset(&gSCManagerBackupData, 0, sizeof(gSCManagerBackupData));
    CHECK(lbBackupIsSramValid() != FALSE);

    /* 2. A blank card. Nothing in the port writes the first file: the
     *    boot does, because lbBackupIsSramValid refuses a store of
     *    zeroes and ends in lbBackupWrite. */
    sy_sram_set_host_path(kSaveFile);
    scManagerInitData();
    CHECK(sy_sram_backing() != NULL &&
          strcmp(sy_sram_backing(), kSaveFile) == 0);
    gSCManagerBackupData.characters_fkind = nFTKindFox;
    gSCManagerBackupData.vs_total_battles = 17;
    lbBackupWrite();
    CHECK(sy_sram_is_dirty() != 0);
    sy_sram_flush();
    CHECK(sy_sram_is_dirty() == 0);
    CHECK(save_file_size(kSaveFile) == n);

    /* 3. The power cycle. A session that starts on a written card reads
     *    it and does not re-default -- and does not write, either, which
     *    is what stops every boot costing a card write. */
    memset(&gSCManagerBackupData, 0, sizeof(gSCManagerBackupData));
    sy_sram_init();
    CHECK(sy_sram_is_dirty() == 0);
    CHECK(lbBackupIsSramValid() != FALSE);
    CHECK(sy_sram_is_dirty() == 0);
    CHECK(gSCManagerBackupData.characters_fkind == nFTKindFox);
    CHECK(gSCManagerBackupData.vs_total_battles == 17);
    saved = gSCManagerBackupData;

    /* 4. A flush with nothing to say writes nothing at all -- the file
     *    is gone and stays gone. */
    remove(kSaveFile);
    sy_sram_flush();
    CHECK(save_file_size(kSaveFile) == -1);
    lbBackupWrite();
    sy_sram_flush();
    CHECK(save_file_size(kSaveFile) == n);

    /* 5. The first copy destroyed. lbBackupIsSramValid falls through to
     *    the second one and repairs the first, which it can only do if
     *    the second one was on the card in the first place. */
    f = fopen(kSaveFile, "r+b");
    CHECK(f != NULL);
    {
        u8 junk[64];

        memset(junk, 0xA5, sizeof(junk));
        CHECK(fwrite(junk, 1, sizeof(junk), f) == sizeof(junk));
    }
    fclose(f);
    memset(&gSCManagerBackupData, 0, sizeof(gSCManagerBackupData));
    sy_sram_init();
    CHECK(lbBackupIsSramValid() != FALSE);
    CHECK(memcmp(&gSCManagerBackupData, &saved, sizeof(saved)) == 0);
    /* the repair is a write, so the card has something to be told */
    CHECK(sy_sram_is_dirty() != 0);
    sy_sram_flush();
    CHECK(save_file_size(kSaveFile) == n);
    memset(&gSCManagerBackupData, 0, sizeof(gSCManagerBackupData));
    sy_sram_init();
    CHECK(lbBackupIsSramValid() != FALSE);
    CHECK(memcmp(&gSCManagerBackupData, &saved, sizeof(saved)) == 0);

    /* 6. A card that cannot be written. One attempt per change, because
     *    the mark is cleared before the write and not after: a full or
     *    missing card must not put a failed write in front of every
     *    frame for the rest of the session. */
    sy_sram_set_host_path("no/such/directory/hosttest_save.bin");
    lbBackupWrite();
    CHECK(sy_sram_is_dirty() != 0);
    sy_sram_flush();
    CHECK(sy_sram_is_dirty() == 0);

    /* Leave the machine as the rest of the suite expects: no card, and a
     * fresh boot's globals. */
    sy_sram_set_host_path(NULL);
    remove(kSaveFile);
    scManagerInitData();
}

/* ---- the cards themselves ----------------------------------
 *
 * src/dc/vmucard.c's host fake: a row of cards in RAM, each with a claimed
 * free-block count and a save file or none. What is proved is the part of
 * the card code that is the same on the console -- which file a card's
 * bytes are judged to be, and which card the store is backed by -- and
 * the one rule a damaged file now follows: it is not loaded, so the game
 * reaches its defaults the way a fresh cartridge does, and the defaults
 * are what replace it.
 */
extern void syDmaReadSram(uintptr_t rom_src, void *ram_dst, size_t size);

static void test_save_cards(void)
{
    static u8 good[3036], bad[3036], back[3036];
    VMUCardInfo cards[VMUCARD_MAX], info;
    int blocks;

    /* A valid payload, the game's own way: its defaults with a boot count
     * nothing else has, written by lbBackupWrite and read out of the
     * store. */
    vmucard_host_reset();
    sy_sram_init();
    gSCManagerBackupData = dSCManagerDefaultBackupData;
    gSCManagerBackupData.boot = 77;
    lbBackupWrite();
    syDmaReadSram(0, good, sizeof(good));
    CHECK(vmucard_payload_ok(good, sizeof(good)) != 0);
    memset(bad, 0x5A, sizeof(bad));
    CHECK(vmucard_payload_ok(bad, sizeof(bad)) == 0);
    CHECK(vmucard_payload_ok(good, sizeof(good) - 1) == 0);

    /* the file: header, three icons, the eyecatch and the payload, 6748
     * bytes -- or the plain icon's eight when vmuart.bin is missing */
    blocks = vmucard_file_blocks(sizeof(good));
    CHECK(blocks == 14);

    /* 1. No card at all: nothing to scan, nothing backing the store. */
    CHECK(vmucard_scan(cards, VMUCARD_MAX) == 0);
    CHECK(sy_sram_backing() == NULL);

    /* 2. An empty card in A1 and the save on B1: the scan sees both, in
     *    port order, and the store is backed by the one with the save. */
    vmucard_host_insert(0, 1, 200);
    vmucard_host_insert(1, 1, 200);
    vmucard_host_put(1, 1, good, sizeof(good), 0);
    CHECK(vmucard_scan(cards, VMUCARD_MAX) == 2);
    CHECK(cards[0].port == 0 && cards[0].unit == 1);
    CHECK(cards[0].state == nVMUCardNoSave && cards[0].file_blocks == 0);
    CHECK(cards[1].port == 1 && cards[1].state == nVMUCardValid);
    CHECK(cards[1].file_blocks == blocks && cards[1].why == NULL);
    memset(&gSCManagerBackupData, 0, sizeof(gSCManagerBackupData));
    sy_sram_init();
    CHECK(sy_sram_backing() != NULL &&
          strcmp(sy_sram_backing(), "host:b1") == 0);
    CHECK(lbBackupIsSramValid() != FALSE);
    CHECK(gSCManagerBackupData.boot == 77);
    CHECK(sy_sram_is_dirty() == 0);

    /* 3. The first copy gone is not damage: the game repairs it. */
    memcpy(back, good, sizeof(back));
    memset(back, 0xA5, 64);
    vmucard_host_put(1, 1, back, sizeof(back), 0);
    CHECK(vmucard_read(1, 1, bad, sizeof(bad), &info) == nVMUCardValid);
    CHECK(memcmp(bad, back, sizeof(bad)) == 0);

    /* 4. Both copies gone, or the VMS CRC wrong: damaged, and nothing of
     *    it goes into the payload. */
    memset(back, 0xA5, sizeof(back));
    vmucard_host_put(1, 1, back, sizeof(back), 0);
    memset(bad, 0, sizeof(bad));
    CHECK(vmucard_read(1, 1, bad, sizeof(bad), &info) == nVMUCardCorrupt);
    CHECK(info.why != NULL && strcmp(info.why, "copies") == 0);
    CHECK(bad[0] == 0);
    vmucard_host_put(1, 1, good, sizeof(good), 1);
    CHECK(vmucard_read(1, 1, bad, sizeof(bad), &info) == nVMUCardCorrupt);
    CHECK(info.why != NULL && strcmp(info.why, "crc") == 0);
    CHECK(bad[0] == 0);

    /* 5. A damaged file and an empty card: the store is backed by the
     *    damaged one and holds zeroes, so the game installs its defaults
     *    and the first flush replaces the file with them. */
    memset(&gSCManagerBackupData, 0, sizeof(gSCManagerBackupData));
    sy_sram_init();
    CHECK(sy_sram_backing() != NULL &&
          strcmp(sy_sram_backing(), "host:b1") == 0);
    CHECK(lbBackupIsSramValid() == FALSE);
    CHECK(sy_sram_is_dirty() != 0);
    sy_sram_flush();
    CHECK(vmucard_read(1, 1, bad, sizeof(bad), &info) == nVMUCardValid);
    CHECK(info.free_blocks == 200 && info.file_blocks == blocks);
    CHECK(vmucard_host_get(1, 1, back, sizeof(back)) == 0);
    CHECK(vmucard_payload_ok(back, sizeof(back)) != 0);

    /* 6. A valid save beats a damaged one in an earlier slot. */
    vmucard_host_put(0, 1, good, sizeof(good), 1);
    vmucard_host_put(1, 1, good, sizeof(good), 0);
    sy_sram_init();
    CHECK(strcmp(sy_sram_backing(), "host:b1") == 0);

    /* 7. A card with too little room: the write fails once, the mark
     *    clears, and the card is left as it was. */
    vmucard_host_reset();
    vmucard_host_insert(2, 2, blocks - 1);
    sy_sram_init();
    CHECK(strcmp(sy_sram_backing(), "host:c2") == 0);
    CHECK(lbBackupIsSramValid() == FALSE);
    CHECK(sy_sram_is_dirty() != 0);
    sy_sram_flush();
    CHECK(sy_sram_is_dirty() == 0);
    CHECK(vmucard_host_get(2, 2, back, sizeof(back)) == -1);
    CHECK(vmucard_read(2, 2, back, sizeof(back), &info) == nVMUCardNoSave);
    CHECK(info.free_blocks == blocks - 1);

    /* 8. A card pulled after boot: the write finds nothing there. */
    vmucard_host_insert(2, 2, 200);
    sy_sram_init();
    lbBackupIsSramValid();
    vmucard_host_remove(2, 2);
    CHECK(vmucard_present(2, 2) == 0);
    sy_sram_flush();
    CHECK(sy_sram_is_dirty() == 0);

    vmucard_host_reset();
    scManagerInitData();
}

/* ---- the writer --------------------------------------------
 *
 * The pump hands the store to a writer thread and the game goes on. On
 * the host the writer runs inside the pump unless paused, which is how a
 * write "in flight" is held still long enough to pile more on top of it.
 * What is proved: shots coalesce, a hold keeps the change in RAM, a
 * failed write is not retried, "save now" writes without a change, and
 * the notice stays up for a second of the clock however fast the card.
 */
static u8 save_card_boot(int port, int unit)
{
    static u8 raw[3036];
    LBBackupData b;

    if (vmucard_host_get(port, unit, raw, sizeof(raw)) != 0)
    {
        return 0xFF;
    }
    memcpy(&b, raw, sizeof(b));

    return b.boot;
}

static void test_save_writer(void)
{
    u32 w;

    vmucard_host_reset();
    vmucard_host_insert(0, 1, 200);
    sy_sram_host_set_clock(0);
    sy_sram_init();
    gSCManagerBackupData = dSCManagerDefaultBackupData;

    /* 1. One change, one write, and the notice for a second from the
     *    shot -- the host's card takes no time at all. */
    CHECK(sy_sram_notice_visible() == 0);
    sy_sram_host_set_clock(5000);
    gSCManagerBackupData.boot = 1;
    lbBackupWrite();
    w = sy_sram_host_writes();
    sy_sram_pump();
    CHECK(sy_sram_is_dirty() == 0);
    CHECK(sy_sram_host_writes() == w + 1);
    CHECK(sy_sram_busy() == 0 && sy_sram_failed() == 0);
    CHECK(save_card_boot(0, 1) == 1);
    CHECK(sy_sram_notice_visible() != 0);
    sy_sram_host_set_clock(5999);
    CHECK(sy_sram_notice_visible() != 0);
    sy_sram_host_set_clock(6000);
    CHECK(sy_sram_notice_visible() == 0);

    /* 2. Two changes while the writer is busy: one write, the newer. The
     *    notice outlasts its second while there is a write to make. */
    sy_sram_host_pause_writer(1);
    gSCManagerBackupData.boot = 2;
    lbBackupWrite();
    sy_sram_pump();
    CHECK(sy_sram_busy() != 0);
    gSCManagerBackupData.boot = 3;
    lbBackupWrite();
    sy_sram_pump();
    sy_sram_host_set_clock(9000);
    CHECK(sy_sram_notice_visible() != 0);
    w = sy_sram_host_writes();
    sy_sram_quiesce();
    CHECK(sy_sram_host_writes() == w + 1);
    CHECK(save_card_boot(0, 1) == 3);
    CHECK(sy_sram_notice_visible() == 0);
    sy_sram_host_pause_writer(0);

    /* 3. A change after the last write landed is a write of its own. */
    w = sy_sram_host_writes();
    gSCManagerBackupData.boot = 4;
    lbBackupWrite();
    sy_sram_pump();
    gSCManagerBackupData.boot = 5;
    lbBackupWrite();
    sy_sram_pump();
    CHECK(sy_sram_host_writes() == w + 2);
    CHECK(save_card_boot(0, 1) == 5);

    /* 4. Held: the change stays in RAM, marked, until the release. */
    sy_sram_hold();
    gSCManagerBackupData.boot = 6;
    lbBackupWrite();
    w = sy_sram_host_writes();
    sy_sram_pump();
    CHECK(sy_sram_host_writes() == w);
    CHECK(sy_sram_is_dirty() != 0);
    CHECK(save_card_boot(0, 1) == 5);
    sy_sram_release();
    sy_sram_pump();
    CHECK(sy_sram_host_writes() == w + 1);
    CHECK(save_card_boot(0, 1) == 6);

    /* 5. A write that fails is said, marked, and not tried again; the
     *    next change is the next attempt, and one that lands clears the
     *    mark. */
    vmucard_host_fail_writes(0, 1, 1);
    gSCManagerBackupData.boot = 7;
    lbBackupWrite();
    w = sy_sram_host_writes();
    sy_sram_pump();
    CHECK(sy_sram_host_writes() == w + 1);
    CHECK(sy_sram_failed() != 0);
    sy_sram_pump();
    sy_sram_pump();
    CHECK(sy_sram_host_writes() == w + 1);
    CHECK(save_card_boot(0, 1) == 6);
    vmucard_host_fail_writes(0, 1, 0);
    gSCManagerBackupData.boot = 8;
    lbBackupWrite();
    sy_sram_pump();
    CHECK(sy_sram_failed() == 0);
    CHECK(save_card_boot(0, 1) == 8);
    vmucard_host_fail_writes(0, 1, 1);
    lbBackupWrite();
    sy_sram_pump();
    CHECK(sy_sram_failed() != 0);
    sy_sram_clear_failure();
    CHECK(sy_sram_failed() == 0);
    vmucard_host_fail_writes(0, 1, 0);

    /* 6. "Save now" writes what the store holds, changed or not. */
    w = sy_sram_host_writes();
    sy_sram_save_now();
    CHECK(sy_sram_host_writes() == w + 1);
    CHECK(save_card_boot(0, 1) == 8);

    vmucard_host_reset();
    sy_sram_host_set_clock(0);
    scManagerInitData();
}
