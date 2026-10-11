/* hosttest/roster.c -- part of hosttest_ft.c: every fighter through the engine: the files per scene, parts,
 * costumes, demo statuses, shadows, and all twelve standing.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* ---- the fighters' files, per scene -------------------------------------
 *
 * ft/ftmanager.c:352-362 ftManagerSetupFilesAllKind, which every scene
 * that stands a fighter up calls for each kind it wants -- the battle
 * for its players, the character select and the results screen for all
 * twelve -- and which the port answers with a pack and a sprite bank
 * per kind, both real here: the packs the target
 * ships, out of romdisk/, through the real loader. What has to hold: a
 * kind loads once and stays resident for the scene, a kind with no pack
 * is skipped, every playable kind has a pack with the joints and
 * triangles the exporter reported, a motion table whose rows point
 * inside it, and a bank with a stock icon and an emblem in it, and the
 * heap reset (which is a scene change, or sudden death) gives every
 * pack the manager loaded back and leaves one it did not load alone.
 *
 * The scripts are not run here: the host loader detaches them
 * (src/dc/fighter.c on why -- the decomp's event structs are laid out
 * for 4-byte pointers), so the table is checked as shipped, off the
 * file, and the spawn test below drives statuses whose logic needs no
 * script.
 */
static const char *const kKindNames[nFTKindEnumCount] =
{
    [nFTKindMario] = "Mario",     [nFTKindFox] = "Fox",
    [nFTKindDonkey] = "Donkey",   [nFTKindSamus] = "Samus",
    [nFTKindLuigi] = "Luigi",     [nFTKindLink] = "Link",
    [nFTKindYoshi] = "Yoshi",     [nFTKindCaptain] = "Captain",
    [nFTKindKirby] = "Kirby",     [nFTKindPikachu] = "Pikachu",
    [nFTKindPurin] = "Purin",     [nFTKindNess] = "Ness",
};

static const char *const sFTManagerKindNames[nFTKindEnumCount] =
{
    [nFTKindMario] = "mario",     [nFTKindFox] = "fox",
    [nFTKindDonkey] = "donkey",   [nFTKindSamus] = "samus",
    [nFTKindLuigi] = "luigi",     [nFTKindLink] = "link",
    [nFTKindYoshi] = "yoshi",     [nFTKindCaptain] = "captain",
    [nFTKindKirby] = "kirby",     [nFTKindPikachu] = "pikachu",
    [nFTKindPurin] = "purin",     [nFTKindNess] = "ness",
};

static void test_fighter_files_per_scene(void)
{
    s32 i;
    const Fighter *fox, *boss;

    sprite_bank_release_all();
    ftManagerReleaseFilesAll();
    load_stage();

    /* one kind: the pack and the bank, once */
    ftManagerSetupFilesAllKind(nFTKindFox);
    fox = gFTManagerModels[nFTKindFox];
    CHECK(fox != NULL);
    CHECK(fox != NULL && strcmp(fox->hd->name, "Fox") == 0);
    /* 356 of the model's own and 175 of its electric skeleton (V14) */
    CHECK(fox != NULL && fox->hd->joint_count == 27 && fox->hd->tri_count == 531);
    CHECK(fox != NULL && fox->hd->anim_count == 169);
    CHECK(fox != NULL && fox->attr != NULL && fox->motion_count == 219);
    /* the longest SubMotion table in the game: Fox's 24 rows carry the
     * nine opening statuses of his own scene as well as the common
     * fifteen */
    CHECK(fox != NULL && fox->submotion_count == 24);
    CHECK(ftManagerGetKindSprites(nFTKindFox) != NULL);
    CHECK(ftManagerGetKindSprites(nFTKindFox)->stock_sprite != NULL);
    CHECK(ftManagerGetKindSprites(nFTKindFox)->emblem != NULL);
    ftManagerSetupFilesAllKind(nFTKindFox);
    CHECK(gFTManagerModels[nFTKindFox] == fox);

    /* There is no longer a kind the port has no pack for. This once stood on
     * nFTKindNMario, then on
     * Master Hand, and the enum has nothing
     * else to move to -- so it asserts the opposite now.
     *
     * His is the smallest pack in the game and the only one whose
     * animations outweigh its geometry: 474 triangles over 25 joints
     * (18 of BossModel's own and the spliced item joints after them),
     * 33 of his 35 animation files -- FTBossAnimUnknown3 and Unknown5
     * are dead ROM the exporter drops -- plus the three
     * of Yoshi's his SubMotion table borrows, and not one
     * texture, because he is flat-shaded white.
     *
     * A pack and a sprite bank: 250_BossMain.c's FTSprites names a
     * stock icon, its one palette and the emblem ifcommon draws behind
     * his H.P readout, all in 345_MasterHandIcon.c. A pack is geometry
     * and animation only -- he has no status table and no motions yet. */
    ftManagerSetupFilesAllKind(nFTKindBoss);
    boss = gFTManagerModels[nFTKindBoss];
    CHECK(boss != NULL);
    CHECK(boss != NULL && strcmp(boss->hd->name, "Boss") == 0);
    CHECK(boss != NULL && boss->hd->joint_count == 25 &&
          boss->hd->tri_count == 474);
    CHECK(boss != NULL && boss->hd->anim_count == 36);
    /* dFTBossSubMotionDescs: 18 rows, three of them empty,
     * and the IntroR row -- the one the 1P stage card asks for -- poses
     * him in FTBossAnimPose1P. The last three name Yoshi's
     * Unknown8/9/10, which is why boss.pack carries 36 animations for
     * 33 files of his own. */
    CHECK(boss != NULL && boss->submotions != NULL &&
          boss->submotion_count == 18);
    if (boss != NULL && boss->submotions != NULL &&
        boss->submotion_count == 18)
    {
        int m = nFTDemoStatusIntroR - FTSTAT_OPENING2_START;

        CHECK(boss->submotions[m].anim >= 0 &&
              (u32)boss->submotions[m].anim < boss->hd->anim_count);
        CHECK(strcmp(boss->anims[boss->submotions[m].anim].name,
                     "FTBossAnimPose1P") == 0);
        /* rows 11..13 are the three the game leaves empty */
        CHECK(boss->submotions[11].anim < 0 &&
              boss->submotions[12].anim < 0 &&
              boss->submotions[13].anim < 0);
        /* his one scripted row is 16, the second of the
         * three in his own opening band (D_ovl1_80390D08), six words out
         * of scsubsysdataboss.c -- read from the file, since the host
         * load detaches the column. The card's IntroR row has no script:
         * a pose that neither speaks nor loops. */
        FILE *bfh = fopen("romdisk/boss.pack", "rb");

        CHECK(boss->submotions[m].script == 0);
        CHECK(bfh != NULL && boss->attr->off_submotion != 0);
        if (bfh != NULL)
        {
            FPackMotion brows[18];

            fseek(bfh, boss->attr->off_submotion, SEEK_SET);
            CHECK(fread(brows, sizeof(brows[0]), 18, bfh) == 18);
            CHECK(brows[16].script >= boss->hd->off_script &&
                  brows[16].script <
                  boss->hd->off_script + boss->hd->script_words * 4);
            CHECK(brows[m].script == 0);
            fclose(bfh);
        }
    }
    /* his bank, ftboss.spr: the stock icon and the emblem */
    CHECK(ftManagerGetKindSprites(nFTKindBoss) != NULL &&
          ftManagerGetKindSprites(nFTKindBoss)->stock_sprite != NULL &&
          ftManagerGetKindSprites(nFTKindBoss)->emblem != NULL);
    /* a Polygon's is the emblem alone -- FTSprites { NULL, NULL,
     * dMasterHandIcon_FTEmblem } in 207_NMarioMain.c -- so ifcommon
     * draws the emblem and makes no stock row for him */
    ftManagerSetupFilesAllKind(nFTKindNMario);
    CHECK(ftManagerGetKindSprites(nFTKindNMario) != NULL &&
          ftManagerGetKindSprites(nFTKindNMario)->stock_sprite == NULL &&
          ftManagerGetKindSprites(nFTKindNMario)->emblem != NULL);
    /* the empty shield pose an unshieldable fighter carries:
     * all-zero, and the guard's tables never built from it */
    CHECK(boss != NULL && boss->shield != NULL &&
          boss->shield->nwords == 0 && boss->shield->lookup_count == 0 &&
          boss->shield->table_count == 0);
    CHECK(boss != NULL && boss->shield_lookup == NULL &&
          boss->shield_joints[0] == NULL);
    ftManagerSetupFilesAllKind(nFTKindMario);
    CHECK(gFTManagerModels[nFTKindMario] == &mock_model);
    CHECK(ftManagerGetKindSprites(nFTKindMario) != NULL);

    /* the game's own loop, mnPlayersVSFuncStart's and mnVSResultsFuncStart's:
     * every playable kind, each with its bank */
    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        ftManagerSetupFilesAllKind(i);
    }
    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        const Fighter *pack = gFTManagerModels[i];

        CHECK(pack != NULL);
        if (i != nFTKindMario && pack != NULL)
        {
            u32 r;

            CHECK(strcmp(pack->hd->name, kKindNames[i]) == 0);
            CHECK(pack->hd->vert_count <= FTMANAGER_VCLIP_MAX);
            CHECK(pack->hd->pal_count == 0);
            CHECK(pack->attr != NULL && pack->motions != NULL);
            CHECK(pack->attr->jumps_max >= 1 && pack->attr->jumps_max <= 6);
            /* the demo statuses' table. Every playable kind
             * has one, it is long enough for the fifteen statuses of the
             * common band (nFTDemoStatusNull..IntroR), and every row
             * either names an animation in the bank or is empty. The
             * script column is read out of the FILE below, not here:
             * fighter_load detaches both tables' scripts on the host. */
            CHECK(pack->submotions != NULL &&
                  pack->submotion_count >=
                  (u32)(nFTDemoStatusIntroR - FTSTAT_OPENING2_START + 1));
            if (pack->submotions != NULL)
            {
                for (r = 0; r < pack->submotion_count; r++)
                {
                    CHECK(pack->submotions[r].anim == -1 ||
                          (u32)pack->submotions[r].anim < pack->hd->anim_count);
                    CHECK(pack->submotions[r].script == 0);
                }
                /* the selected pose the character select asks for
                 * (nFTDemoStatusWin1) is a real animation for everyone */
                CHECK(pack->submotions[nFTDemoStatusWin1 -
                                       FTSTAT_OPENING2_START].anim >= 0);
            }
            /* the motion table the game's ids index, as shipped (the host
             * loader detaches the scripts, src/dc/fighter.c): every row's
             * animation is in the bank or absent, and every row's script
             * is inside the pack's copy of the MainMotion file -- what
             * fighter_init relocates the pointers within */
            {
                char path[64];
                FILE *fh;
                FPackHeader hd;
                FPackMotion *rows;
                u32 scripted = 0;

                snprintf(path, sizeof(path), "romdisk/%s.pack", sFTManagerKindNames[i]);
                fh = fopen(path, "rb");
                CHECK(fh != NULL);
                if (fh != NULL)
                {
                    CHECK(fread(&hd, sizeof(hd), 1, fh) == 1);
                    CHECK(hd.motion_count == (u32)pack->motion_count);
                    rows = malloc(sizeof(*rows) * hd.motion_count);
                    fseek(fh, hd.off_motion, SEEK_SET);
                    CHECK(fread(rows, sizeof(*rows), hd.motion_count, fh) == hd.motion_count);
                    for (r = 0; r < hd.motion_count; r++)
                    {
                        CHECK(rows[r].anim == -1 || (u32)rows[r].anim < hd.anim_count);
                        CHECK(rows[r].script == 0 ||
                              (rows[r].script >= hd.off_script &&
                               rows[r].script < hd.off_script + hd.script_words * 4));
                        scripted += rows[r].script != 0;
                    }
                    /* row 0 is the Wait, with an animation and no script,
                     * for every fighter (ft/ftdata.c), and most rows are
                     * scripted */
                    CHECK(rows[0].anim >= 0 && rows[0].script == 0);
                    CHECK(scripted > hd.motion_count / 2);

                    /* The SubMotion rows' event scripts,
                     * read from the file for the same reason the
                     * MainMotion rows are: fighter_load zeroes both
                     * tables' script columns on the host. They live in
                     * the one script blob behind the MainMotion file, so
                     * a row's column is held to the same bound. Every
                     * playable kind has at least four -- the four win
                     * poses and the select screen's idle, which is the
                     * eye-blink loop. The table is found through
                     * FPackAttr, which the load does not touch. */
                    {
                        FPackMotion *srows;
                        u32 sposed = 0;

                        CHECK(pack->attr->off_submotion != 0 &&
                              pack->attr->submotion_count ==
                              pack->submotion_count);
                        srows = malloc(sizeof(*srows) *
                                       pack->attr->submotion_count);
                        fseek(fh, pack->attr->off_submotion, SEEK_SET);
                        CHECK(fread(srows, sizeof(*srows),
                                    pack->attr->submotion_count, fh) ==
                              pack->attr->submotion_count);
                        for (r = 0; r < pack->attr->submotion_count; r++)
                        {
                            CHECK(srows[r].anim == pack->submotions[r].anim);
                            CHECK(srows[r].script == 0 ||
                                  (srows[r].script >= hd.off_script &&
                                   srows[r].script <
                                   hd.off_script + hd.script_words * 4));
                            sposed += srows[r].script != 0;
                        }
                        CHECK(sposed >= 4);
                        /* Win1 speaks: its script is the one that plays
                         * the fighter's win voice */
                        CHECK(srows[nFTDemoStatusWin1 -
                                    FTSTAT_OPENING2_START].script != 0);
                        free(srows);
                    }

                    /* The hammer's swing: HammerWait's script calls into
                     * the common moveset (201_FTCommonMoveset.c), whose
                     * four scaled hitboxes are the hammer's only damage.
                     * The exporter used to leave that extern pointer
                     * NULL, so a held hammer hit nothing. The call's
                     * pointer word must be a relocated site, and what it
                     * reaches must make a hitbox. */
                    CHECK(hd.motion_count > nFTCommonMotionHammerWait);
                    if (hd.motion_count > nFTCommonMotionHammerWait)
                    {
                        u32 *w = malloc(hd.script_words * 4);
                        u32 *rl = malloc(hd.script_reloc_count * 4 + 4);
                        u32 at, k, q, tgt = 0;
                        sb32 is_call = FALSE, is_site = FALSE, is_hitbox = FALSE;

                        CHECK(rows[nFTCommonMotionHammerWait].script != 0);
                        fseek(fh, hd.off_script, SEEK_SET);
                        CHECK(fread(w, 4, hd.script_words, fh) == hd.script_words);
                        fseek(fh, hd.off_script_reloc, SEEK_SET);
                        CHECK(fread(rl, 4, hd.script_reloc_count, fh) == hd.script_reloc_count);
                        at = (rows[nFTCommonMotionHammerWait].script - hd.off_script) / 4;

                        for (k = at; (k < at + 8) && (k + 1 < hd.script_words); k++)
                        {
                            if ((w[k] >> 26) == nFTMotionEventSubroutine)
                            {
                                is_call = TRUE;
                                tgt = w[k + 1] / 4;
                                for (q = 0; q < hd.script_reloc_count; q++)
                                {
                                    is_site |= (rl[q] == k + 1);
                                }
                                break;
                            }
                        }
                        CHECK(is_call);
                        CHECK(is_site);
                        CHECK(tgt != 0 && tgt < hd.script_words);
                        for (q = tgt; is_site && (q < tgt + 16) && (q < hd.script_words); q++)
                        {
                            is_hitbox |= ((w[q] >> 26) == nFTMotionEventMakeAttackCollScaled);
                        }
                        CHECK(is_hitbox);

                        /* and every one of MainMotion's 23 extern sites --
                         * the Beam Sword's, Bat's, Fan's, Star Rod's and Fire
                         * Flower's swings and the thrown-damage scripts
                         * besides the Hammer's -- is a relocated site into
                         * the copy, which is the file's last 524 words */
                        {
                            u32 base = hd.script_words - (2096 / 4);
                            u32 into = 0;

                            for (q = 0; q < hd.script_reloc_count; q++)
                            {
                                into += (rl[q] < base) && (w[rl[q]] >= base * 4) &&
                                        (w[rl[q]] < hd.script_words * 4);
                            }
                            CHECK(hd.script_words > (2096 / 4));
                            CHECK(into == 23);
                        }
                        free(w);
                        free(rl);
                    }
                    free(rows);
                    fclose(fh);
                }
            }
        }
        CHECK(ftManagerGetKindSprites(i) != NULL);
        CHECK(ftManagerGetKindSprites(i)->stock_sprite != NULL);
        CHECK(ftManagerGetKindSprites(i)->stock_luts != NULL);
        CHECK(ftManagerGetKindSprites(i)->emblem != NULL);
    }

    /* the heap reset: what the manager loaded goes, what init installed
     * stays, and the banks go with theirs */
    syTaskmanResetGeneralHeap();
    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        CHECK((gFTManagerModels[i] != NULL) == (i == nFTKindMario));
        CHECK(ftManagerGetKindSprites(i) == NULL);
    }
    CHECK(gFTManagerModels[nFTKindMario] == &mock_model);

    /* and the next scene loads again */
    ftManagerSetupFilesAllKind(nFTKindPurin);
    CHECK(gFTManagerModels[nFTKindPurin] != NULL);
    CHECK(gFTManagerModels[nFTKindPurin] != NULL &&
          strcmp(gFTManagerModels[nFTKindPurin]->hd->name, "Purin") == 0);
    ftManagerReleaseFilesAll();
    CHECK(gFTManagerModels[nFTKindPurin] == NULL);
    sprite_bank_release_all();

    /* every playable has a pack, and so does Giant
     * Donkey Kong, the first kind outside the playable range to get one
     * -- the ladder's rung 6 deals nFTKindGDonkey and ftManagerMakeFighter
     * wants a pack for it. His is Donkey Kong's geometry and animations
     * under dGDonkeyMain's own attributes, so the range is no longer the
     * whole answer and this is the list the port actually ships.
     *
     * closed it: Master Hand's pack is the last one, so
     * every kind the game has now names one and this loop's list is the
     * whole enum. A pack is only geometry and animations, though -- he
     * still has no status table and no motions of his own.
     *
     * And every KIND has its warp-in row, Master
     * Hand's included, which comes from his special table. */
    for (i = 0; i < nFTKindEnumCount; i++)
    {
        sb32 want_pack = (i >= nFTKindPlayableStart && i <= nFTKindPlayableEnd) ||
                         i == nFTKindGDonkey || i == nFTKindMMario ||
                         i == nFTKindBoss ||
                         (i >= nFTKindNStart && i <= nFTKindNEnd);

        CHECK(ftManagerKindHasPack(i) == want_pack);
        CHECK(ftCommonEntryHasAppear(i) != FALSE);
    }
    /* load_stage zeroed the mock, and the scene tests after this one
     * spawn Mario without going through spawn_desc, which is where the
     * mock's attributes are normally put back */
    mock_model.attr = mock_attr;
}

/* ---- the packs kept across a scene change --------------
 *
 * The port's one deliberate deviation from the game's file lifetime:
 * ftManagerKeepFilesForScene marks the kinds the scene about to start
 * will ask for, and ftManagerReleaseFilesAll leaves those where they
 * are. The game re-DMAs them out of the cartridge (src/dc/ftmanager.c
 * quotes lb/lbreloc.c on why that is free there and is not here), so
 * there is no oracle for this one: what is checked is the property the
 * cache has to have -- a kept pack is the same pack, an unkept one is
 * gone, a mark is never stale, and the scene heap's share of a kind
 * (its attributes, its sprite bank) is dropped either way.
 */
static void keep_state_for(s32 a, s32 b)
{
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(gSCManagerTransferBattleState.players); i++)
    {
        gSCManagerTransferBattleState.players[i].pkind = nFTPlayerKindNot;
        gSCManagerTransferBattleState.players[i].fkind = nFTKindNull;
    }
    gSCManagerTransferBattleState.players[0].pkind = nFTPlayerKindMan;
    gSCManagerTransferBattleState.players[0].fkind = a;
    gSCManagerTransferBattleState.players[1].pkind = nFTPlayerKindCom;
    gSCManagerTransferBattleState.players[1].fkind = b;
}

static void test_fighter_files_kept(void)
{
    SCBattleState saved = gSCManagerTransferBattleState;
    const Fighter *fox, *purin;
    s32 i;

    sprite_bank_release_all();
    ftManagerKeepFilesForScene(nSCKindTitle);
    ftManagerReleaseFilesAll();
    load_stage();

    /* the character select: every playable kind resident at once */
    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        ftManagerSetupFilesAllKind(i);
    }
    fox = gFTManagerModels[nFTKindFox];
    purin = gFTManagerModels[nFTKindPurin];
    CHECK(fox != NULL && purin != NULL);

    /* select -> stage select, with two of the twelve in the match: the
     * two stay, the other ten go, and the stage select loads no pack of
     * its own -- which is the whole of the saving */
    keep_state_for(nFTKindFox, nFTKindPurin);
    ftManagerKeepFilesForScene(nSCKindMaps);
    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        CHECK(ftManagerKindIsKept(i) == (i == nFTKindFox || i == nFTKindPurin));
    }
    ftManagerReleaseFilesAll();
    sprite_bank_release_all();
    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        sb32 kept = (i == nFTKindFox || i == nFTKindPurin ||
                     i == nFTKindMario);      /* Mario's is the mock's */
        CHECK((gFTManagerModels[i] != NULL) == kept);
        /* the scene heap's share goes either way */
        CHECK(ftManagerGetKindSprites(i) == NULL);
    }
    CHECK(gFTManagerModels[nFTKindFox] == fox);
    CHECK(gFTManagerModels[nFTKindPurin] == purin);

    /* stage select -> battle: still marked, so still there, and the
     * battle's own ask finds the pack rather than reading it again */
    ftManagerKeepFilesForScene(nSCKindVSBattle);
    ftManagerReleaseFilesAll();
    CHECK(gFTManagerModels[nFTKindFox] == fox);
    ftManagerSetupFilesAllKind(nFTKindFox);
    ftManagerSetupFilesAllKind(nFTKindPurin);
    CHECK(gFTManagerModels[nFTKindFox] == fox);
    CHECK(gFTManagerModels[nFTKindPurin] == purin);
    CHECK(ftManagerGetKindSprites(nFTKindFox) != NULL);

    /* sudden death: a heap reset inside the scene, with no scene change
     * to recompute the marks. The banks and the attributes go with the
     * heap; the packs are the ones the scene change kept and stay. */
    syTaskmanResetGeneralHeap();
    CHECK(gFTManagerModels[nFTKindFox] == fox);
    CHECK(gFTManagerModels[nFTKindPurin] == purin);
    CHECK(ftManagerGetKindSprites(nFTKindFox) == NULL);
    ftManagerSetupFilesAllKind(nFTKindFox);
    CHECK(gFTManagerModels[nFTKindFox] == fox);

    /* battle -> results: the results screen loads all twelve, so what the
     * battle loaded stays -- the loaded
     * kinds and no others */
    ftManagerKeepFilesForScene(nSCKindVSResults);
    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        CHECK(ftManagerKindIsKept(i) == (gFTManagerModels[i] != NULL &&
                                         (i == nFTKindFox || i == nFTKindPurin)));
    }
    ftManagerReleaseFilesAll();
    CHECK(gFTManagerModels[nFTKindFox] == fox);
    CHECK(gFTManagerModels[nFTKindPurin] == purin);

    /* results -> title: nothing on that path keeps a pack, and a mark
     * cannot outlive the scene change that took it */
    ftManagerKeepFilesForScene(nSCKindTitle);
    for (i = 0; i < nFTKindEnumCount; i++)
    {
        CHECK(!ftManagerKindIsKept(i));
    }
    ftManagerReleaseFilesAll();
    sprite_bank_release_all();
    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        CHECK((gFTManagerModels[i] != NULL) == (i == nFTKindMario));
    }
    CHECK(gFTManagerModels[nFTKindMario] == &mock_model);

    /* a kind the battle state names but nothing loaded is marked and
     * costs nothing: the mark is on the kind, not on a pack */
    keep_state_for(nFTKindLink, nFTKindNess);
    ftManagerKeepFilesForScene(nSCKindVSBattle);
    CHECK(ftManagerKindIsKept(nFTKindLink) && ftManagerKindIsKept(nFTKindNess));
    ftManagerReleaseFilesAll();
    CHECK(gFTManagerModels[nFTKindLink] == NULL);
    ftManagerKeepFilesForScene(nSCKindTitle);

    /* the opening movie: what a fighter's own scene loaded stays through
     * the cuts after it, whatever the battle state says, and is gone on
     * the way into Newcomers (and into the title, on a skip) */
    keep_state_for(nFTKindLink, nFTKindNess);
    ftManagerSetupFilesAllKind(nFTKindFox);
    fox = gFTManagerModels[nFTKindFox];
    CHECK(fox != NULL);
    ftManagerKeepFilesForScene(nSCKindOpeningPikachu);
    CHECK(ftManagerKindIsKept(nFTKindFox));
    CHECK(!ftManagerKindIsKept(nFTKindLink) && !ftManagerKindIsKept(nFTKindNess));
    ftManagerReleaseFilesAll();
    sprite_bank_release_all();
    CHECK(gFTManagerModels[nFTKindFox] == fox);
    ftManagerKeepFilesForScene(nSCKindOpeningRun);
    ftManagerReleaseFilesAll();
    ftManagerSetupFilesAllKind(nFTKindFox);
    CHECK(gFTManagerModels[nFTKindFox] == fox);
    ftManagerKeepFilesForScene(nSCKindOpeningNewcomers);
    CHECK(!ftManagerKindIsKept(nFTKindFox));
    ftManagerReleaseFilesAll();
    sprite_bank_release_all();
    CHECK(gFTManagerModels[nFTKindFox] == NULL);
    /* the Room is entered from outside the movie and keeps nothing */
    ftManagerSetupFilesAllKind(nFTKindFox);
    ftManagerKeepFilesForScene(nSCKindOpeningRoom);
    CHECK(!ftManagerKindIsKept(nFTKindFox));
    ftManagerReleaseFilesAll();
    sprite_bank_release_all();
    CHECK(gFTManagerModels[nFTKindFox] == NULL);
    ftManagerKeepFilesForScene(nSCKindTitle);

    gSCManagerTransferBattleState = saved;
    mock_model.attr = mock_attr;
}

/* ---- every fighter, through the engine -------------------------------
 *
 * Each real pack, spawned the way scVSBattleStartBattle spawns a
 * fighter (spawn_desc, but of its own kind) and driven through the
 * decomp's own statuses with its own attributes, motion table and
 * figatrees: standing for 300 tics where put -- which is where Fox once died
 * on the target -- then a short hop and the landing,
 * then a jab, each a row of the pack's own table. The scripts are
 * detached on the host (src/dc/fighter.c), so what a script does --
 * hitboxes, sounds, effects -- is the target's to show; the statuses,
 * the physics and the animations are the engine's and are here.
 */
static int is_airborne(void) { return fp.ga == nMPKineticsAir; }

/* the costume spawn_kind deals (test_accessory) */
static s32 spawn_costume = 0;

static GObj *spawn_kind(s32 fkind, float x, float y)
{
    FTDesc desc = dFTManagerDefaultFighterDesc;

    if (mock_gobj != NULL)
    {
        ftManagerDestroyFighter(mock_gobj);
        mock_gobj = NULL;
    }
    desc.fkind = fkind;
    desc.pos.x = x;
    desc.pos.y = y;
    desc.pos.z = 0.0f;
    desc.lr = 1;
    desc.team = 0;
    desc.player = 0;
    desc.detail = nFTPartsDetailHigh;
    desc.shade = 1;
    desc.costume = spawn_costume;
    desc.stock_count = mock_battle.stocks;
    desc.damage = 0;
    desc.handicap = mock_battle.players[0].handicap;
    desc.pkind = nFTPlayerKindMan;
    desc.controller = &mock_in;
    desc.is_skip_entry = TRUE;
    desc.figatree_heap = ftManagerAllocFigatreeHeapKind(fkind);
    memset(&mock_in, 0, sizeof(mock_in));
    mock_gobj = ftManagerMakeFighter(&desc);
    ftParamInitPlayerBattleStats(0, mock_gobj);
    fpp = ftGetStruct(mock_gobj);
    ftParamUnlockPlayerControl(mock_gobj);
    ft_clear_knockback(fpp);
    return mock_gobj;
}


/* mp/mpcommon.c:299-405 mpCommonUpdateFighterSlopeContour,
 * every status's proc_slope. Each real pack stands on the mock slope
 * (rise 1 in 2) with both feet flagged the way a SetSlopeContour motion
 * event flags them (the host detaches the scripts, so the flags are set
 * here). 800DDF74 aims each foot's tip -- `rotate` along the lower leg
 * bone -- at its old height moved by the floor's rise under it,
 * clamped unk_0x320 below the root and no higher than unk_0x31C below
 * the foot joint; 800EBD08 then solves the two-bone leg for that point
 * in the foot joint's space. Where the point is within the two bones'
 * reach the tip must land on it; where it is not, the knee must be
 * straight (syUtilsArcCos clamps), never NaN. The feet stand apart in z,
 * so on a 2D slope most of the aim comes from the clamp, and across the
 * roster some foot has to actually move -- before the port proc_slope
 * was NULL and none did. Then the full-body flag tilts TopN to the floor
 * normal, and in the air nothing happens. */
static void slope_foot_tip(s32 joint_id, f32 rotate, Vec3f *out)
{
    out->x = rotate;
    out->y = out->z = 0.0F;
    gmCollisionGetFighterPartsWorldPosition(fpp->joints[joint_id]->child->child, out);
}

static void test_slope_contour(void)
{
    s32 k;
    s32 moved = 0, reached = 0, straight = 0;
    f32 err_max = 0.0F;

    for (k = nFTKindPlayableStart; k <= nFTKindPlayableEnd; k++)
    {
        FTAttributes *attr;
        s32 ids[2], f, i;
        f32 rotates[2], dists[2];
        Vec3f before[2], want[2], after;
        f32 root_y, root_x, tilt;

        ftManagerSetupFilesAllKind(k);
        spawn_kind(k, 4000.0f, 600.0f);
        for (i = 0; i < 4; i++)
        {
            frame(0, 0, 0, 0, 0);
        }
        attr = fpp->attr;
        CHECK(fp.ga == nMPKineticsGround);
        CHECK(fp.coll_data.floor_line_id == LINE_SLOPE);
        CHECK(fp.proc_slope == mpCommonUpdateFighterSlopeContour);

        ids[0] = attr->joint_rfoot_id;  rotates[0] = attr->joint_rfoot_rotate;
        ids[1] = attr->joint_lfoot_id;  rotates[1] = attr->joint_lfoot_rotate;
        CHECK(attr->unk_0x31C >= 50.0F && attr->unk_0x320 > 0.5F);
        if (k == nFTKindMario)
        {
            /* relocData/203_MarioMain.c:337-343 */
            CHECK(ids[0] == 23 && ids[1] == 18);
            CHECK_NEAR(rotates[0], 60.891F, 1e-3F);
            CHECK_NEAR(attr->unk_0x31C, 50.0F, 1e-6F);
            CHECK_NEAR(attr->unk_0x320, 0.5235988F, 1e-6F);
        }
        for (f = 0; f < 2; f++)
        {
            CHECK(ids[f] > nFTPartsJointTopN && ids[f] < (s32)FTPARTS_JOINT_NUM_MAX);
            CHECK(fpp->joints[ids[f]] != NULL &&
                  fpp->joints[ids[f]]->child != NULL &&
                  fpp->joints[ids[f]]->child->child != NULL);
            CHECK(rotates[f] > 0.0F);
            if (fpp->joints[ids[f]] == NULL || fpp->joints[ids[f]]->child == NULL ||
                fpp->joints[ids[f]]->child->child == NULL)
            {
                return;
            }
        }
        CHECK(ids[0] != ids[1]);

        /* where 800DDF74 aims each foot, from the pose as it stands */
        ftParamsUpdateFighterPartsTransformAll(fpp->joints[nFTPartsJointTopN]);
        root_x = DObjGetStruct(mock_gobj)->translate.vec.f.x;
        root_y = DObjGetStruct(mock_gobj)->translate.vec.f.y;
        for (f = 0; f < 2; f++)
        {
            Vec3f parent = { 0.0F, 0.0F, 0.0F };
            Vec3f local;
            f32 floor_y;

            slope_foot_tip(ids[f], rotates[f], &before[f]);
            floor_y = (before[f].x - 3000.0F) * 0.5F;
            want[f] = before[f];
            want[f].y += floor_y - root_y;
            if (want[f].y < root_y)
            {
                f32 lo = root_y - ABSF(root_x - want[f].x) * syUtilsTan(attr->unk_0x320);

                if (want[f].y < lo)
                {
                    want[f].y = lo;
                }
            }
            gmCollisionGetFighterPartsWorldPosition(fpp->joints[ids[f]], &parent);
            if (want[f].y > parent.y - attr->unk_0x31C)
            {
                want[f].y = parent.y - attr->unk_0x31C;
            }
            local = want[f];
            func_ovl2_800EE018(fpp->joints[ids[f]], &local);
            dists[f] = sqrtf(SQUARE(local.x) + SQUARE(local.y) + SQUARE(local.z));
        }
        fp.slope_contour = FTSLOPECONTOUR_FLAG_RFOOT | FTSLOPECONTOUR_FLAG_LFOOT;
        mpCommonUpdateFighterSlopeContour(mock_gobj);
        ftParamsUpdateFighterPartsTransformAll(fpp->joints[nFTPartsJointTopN]);
        for (f = 0; f < 2; f++)
        {
            DObj *shin = fpp->joints[ids[f]]->child->child;
            f32 thigh = shin->translate.vec.f.x;
            f32 err, knee = shin->rotate.vec.f.z;

            slope_foot_tip(ids[f], rotates[f], &after);
            CHECK(after.x == after.x && after.y == after.y && after.z == after.z);
            CHECK(knee == knee);
            err = sqrtf(SQUARE(after.x - want[f].x) + SQUARE(after.y - want[f].y) +
                        SQUARE(after.z - want[f].z));
            if (ABSF(after.y - before[f].y) > 2.0F)
            {
                moved++;
            }
            if ((dists[f] < thigh + rotates[f] - 1.0F) &&
                (dists[f] > ABSF(thigh - rotates[f]) + 1.0F))
            {
                /* within reach: on the point, to the solver's precision
                 * (its knee twist comes from the pose before the bend) */
                CHECK(err < 1.0F);
                if (err > err_max)
                {
                    err_max = err;
                }
                reached++;
            }
            else if (dists[f] >= thigh + rotates[f])
            {
                /* too far: the knee straight, the tip as low as it goes */
                CHECK(knee == 0.0F);
                straight++;
            }
        }

        /* the full-body flag: TopN pitched to the floor normal */
        fp.slope_contour = FTSLOPECONTOUR_FLAG_FULL;
        DObjGetStruct(mock_gobj)->rotate.vec.f.x = 0.0F;
        mpCommonUpdateFighterSlopeContour(mock_gobj);
        tilt = syUtilsArcTan2(fp.coll_data.floor_angle.x, fp.coll_data.floor_angle.y) * fp.lr;
        CHECK(ABSF(tilt) > 0.4F);
        CHECK_NEAR(DObjGetStruct(mock_gobj)->rotate.vec.f.x, tilt, 1e-6F);

        /* in the air it leaves the pose alone */
        fp.ga = nMPKineticsAir;
        DObjGetStruct(mock_gobj)->rotate.vec.f.x = 0.0F;
        mpCommonUpdateFighterSlopeContour(mock_gobj);
        CHECK(DObjGetStruct(mock_gobj)->rotate.vec.f.x == 0.0F);
        fp.ga = nMPKineticsGround;
        fp.slope_contour = 0;
        DObjGetStruct(mock_gobj)->rotate.vec.f.x = 0.0F;
    }
    CHECK(moved >= 4);
    CHECK(reached >= 4);
    printf("slope contour: 24 feet aimed, %d moved over 2 units, %d reached "
           "their point (worst miss %.2f), %d straightened out of reach\n",
           (int)moved, (int)reached, err_max, (int)straight);
}

/* ft/ftparam.c:1783-1796 ftParamGetEffectJointPosition, the cycler that
 * twenty of ftParamMakeEffect's forty-five arms open with. It steps
 * fp->effect_joint_array_id first and wraps at the end of the attribute
 * table's five, so a fighter on fire spreads the flames over those five
 * joints, one per call, in that order forever.
 *
 * landed it with the gate. What is checkable on the host is
 * exactly this much: the cycle, and that each call's position is the
 * world position of the joint the table names on that step. The effects
 * the arms then make are the target's -- the host declines a particle
 * script bank, whose pointer arrays are 32-bit. Every playable is walked
 * because the table is per-kind, and a joint id it names that the kind's
 * skeleton has not is a null dereference inside gm/gmcollision.c, which
 * is compiled unmodified and has no guard for it. */
static void test_effect_joint_cycle(void)
{
    const s32 num = (s32)ARRAY_COUNT(fpp->attr->effect_joint_ids);
    s32 k;

    sprite_bank_release_all();
    ftManagerReleaseFilesAll();
    load_stage();
    mock_gobj = NULL;
    mock_gobj2 = NULL;
    fpp2 = NULL;
    /* as the test before this one: Mario's slot survives
     * ftManagerReleaseFilesAll and would point into the heap load_stage
     * just remade */
    gFTManagerModels[nFTKindMario] = NULL;

    for (k = nFTKindPlayableStart; k <= nFTKindPlayableEnd; k++)
    {
        Vec3f pos, ref;
        s32 want, i;

        ftManagerSetupFilesAllKind(k);
        spawn_kind(k, 0.0f, 0.0f);

        /* Every joint the table names is one this skeleton has, and no
         * two of the five are the same bone -- spreading them is the
         * whole point of the cycler. Both hold on all twelve in the
         * ROM, and together they are what catches the table not being
         * carried at all: before the pack grew the field every entry
         * read 0, which is TopN, five times over. */
        for (i = 0; i < num; i++)
        {
            s32 id = fpp->attr->effect_joint_ids[i];
            s32 j;

            CHECK(id >= nFTPartsJointCommonStart &&
                  id < (s32)FTPARTS_JOINT_NUM_MAX);
            CHECK(fpp->joints[id] != NULL);

            for (j = 0; j < i; j++)
            {
                CHECK(fpp->attr->effect_joint_ids[j] != id);
            }
        }
        /* and the values themselves, against relocData/203_MarioMain.c:330
         * and 229_KirbyMain.c:  the head, the two forearms, the two shins */
        if (k == nFTKindMario)
        {
            CHECK(fpp->attr->effect_joint_ids[0] == 12 &&
                  fpp->attr->effect_joint_ids[1] == 15 &&
                  fpp->attr->effect_joint_ids[2] == 20 &&
                  fpp->attr->effect_joint_ids[3] == 25 &&
                  fpp->attr->effect_joint_ids[4] == 9);
        }
        if (k == nFTKindKirby)
        {
            CHECK(fpp->attr->effect_joint_ids[0] == 6 &&
                  fpp->attr->effect_joint_ids[1] == 16 &&
                  fpp->attr->effect_joint_ids[2] == 22 &&
                  fpp->attr->effect_joint_ids[3] == 27 &&
                  fpp->attr->effect_joint_ids[4] == 11);
        }

        fpp->effect_joint_array_id = 0;
        want = 0;

        /* twice round, so the wrap is under test and not just the step */
        for (i = 0; i < 2 * num; i++)
        {
            want++;

            if (want == num)
            {
                want = 0;
            }
            ftParamGetEffectJointPosition(fpp, &pos);

            CHECK((s32)fpp->effect_joint_array_id == want);

            ref.x = ref.y = ref.z = 0.0f;
            gmCollisionGetFighterPartsWorldPosition(
                fpp->joints[fpp->attr->effect_joint_ids[want]], &ref);

            CHECK(pos.x == ref.x && pos.y == ref.y && pos.z == ref.z);
        }
    }
    printf("effect joints: %d kinds cycle their %d effect joints in order "
           "and land on the joint's world position\n",
           (int)(nFTKindPlayableEnd - nFTKindPlayableStart + 1), (int)num);
}
/* the model parts. ftParamSetModelPartID and its
 * three neighbours put a part's payload on the joint where the game puts
 * the part's display list, and the walk records it as the joint's part;
 * the batches of any other part are then not drawn. Link's shield hand,
 * his sheath (a joint whose tree entry has no display list, so it starts
 * with none on) and his boomerang hand (baked out of another file), and
 * Kirby's fifteen heads. */
static s32 v06_part_tag(const Fighter *m, s32 joint, s32 id)
{
    const FPackPartJoint *pj = &m->part_joints[joint];

    return (id < pj->id_count) ? m->part_ids[pj->id_first + id].tag : -1;
}

static s32 v06_tag_batches(const Fighter *m, s32 tag, s32 joint)
{
    s32 b, n = 0;

    for (b = 0; b < (s32)m->hd->batch_count; b++)
    {
        if (m->batches[b].part == tag)
        {
            n++;
            CHECK(m->batches[b].joint == joint);
        }
    }
    return n;
}

/* ft/ftdisplaymain.c:847-947: the electric skeleton. Every
 * playable's pack carries FTAttributes.skeleton's tables as part tags
 * (FPackPartTag.skeleton) -- Kirby and Jigglypuff two ids, everyone else
 * one -- and the attribute's words: the joint word 0 names
 * (relocData/203_MarioMain.c:225 and friends) and non-NULL tables.
 * ftDisplayMainGetSkeletonID is ftDisplayMainDrawAll's choice, and with a
 * skeleton chosen the model draws that id's tags and nothing else. The
 * electric damage colour scripts that set the id jump through a
 * subroutine, which the host's copy cannot follow
 * (hosttest_gmcolscripts.c); the disc probe watches those. */
static void test_electric_skeleton(void)
{
    s32 k;

    sprite_bank_release_all();
    ftManagerReleaseFilesAll();
    load_stage();
    mock_gobj = NULL;
    mock_gobj2 = NULL;
    fpp2 = NULL;
    gFTManagerModels[nFTKindMario] = NULL;

    for (k = nFTKindPlayableStart; k <= nFTKindPlayableEnd; k++)
    {
        Fighter *m;
        s32 t, counts[3] = { 0, 0, 0 };
        s32 two = (k == nFTKindKirby) || (k == nFTKindPurin);
        s32 joint;
        void *dv;

        ftManagerSetupFilesAllKind(k);
        spawn_kind(k, 0.0f, 0.0f);
        m = dc_model_of(mock_gobj);
        CHECK(m != NULL && m->parts != NULL);
        CHECK(fp.attr->skeleton != NULL);
        if ((m == NULL) || (m->parts == NULL) || (fp.attr->skeleton == NULL))
        {
            continue;
        }
        joint = (s32)(intptr_t)fp.attr->skeleton[0];
        CHECK(fp.attr->skeleton[1] != NULL);
        CHECK((fp.attr->skeleton[2] != NULL) == two);
        if (k == nFTKindMario)
        {
            CHECK(joint == 12);
        }
        if (k == nFTKindLink)
        {
            CHECK(joint == 23);
        }
        if (k == nFTKindKirby)
        {
            CHECK(joint == 10);
        }
        for (t = 0; t < (s32)m->parts->tag_count; t++)
        {
            s32 sid = m->part_tags[t].skeleton;

            CHECK(sid <= 2);
            if (sid <= 2)
            {
                counts[sid]++;
            }
            /* a tag with vertices draws on its own joint; Samus's two
             * rows whose only list is a dls[0] that sets state are
             * empty */
            if ((sid != 0) && (m->part_tags[t].vert_count != 0))
            {
                CHECK(v06_tag_batches(m, t + 1, m->part_tags[t].joint) > 0);
            }
        }
        CHECK(counts[1] > 0);
        CHECK((counts[2] > 0) == two);

        /* ftDisplayMainDrawAll's choice */
        fp.colanim.skeleton_id = 0;
        CHECK(ftDisplayMainGetSkeletonID(fpp) == 0);
        fp.colanim.skeleton_id = 1;
        CHECK(ftDisplayMainGetSkeletonID(fpp) == 1);
        fp.colanim.skeleton_id = 2;
        CHECK(ftDisplayMainGetSkeletonID(fpp) == (two ? 2 : 0));
        fp.colanim.skeleton_id = 1;
        CHECK(fp.joints[joint] != NULL);
        if (fp.joints[joint] != NULL)
        {
            dv = fp.joints[joint]->dv;
            fp.joints[joint]->dv = NULL;
            CHECK(ftDisplayMainGetSkeletonID(fpp) == 0);
            fp.joints[joint]->dv = dv;
        }
        fp.colanim.skeleton_id = 0;

        /* what draws: skeleton 1's tags and nothing else, then the model */
        m->skeleton_id = 1;
        CHECK(!fighter_part_on(m, 0, 0));
        for (t = 0; t < (s32)m->parts->tag_count; t++)
        {
            CHECK(fighter_part_on(m, t + 1, m->part_tags[t].joint) ==
                  (m->part_tags[t].skeleton == 1));
        }
        m->skeleton_id = 0;
        CHECK(fighter_part_on(m, 0, 0));
        for (t = 0; t < (s32)m->parts->tag_count; t++)
        {
            if (m->part_tags[t].skeleton != 0)
            {
                CHECK(!fighter_part_on(m, t + 1, m->part_tags[t].joint));
            }
        }
    }

    printf("electric skeleton: twelve packs carry their skeleton tags, "
           "and a chosen skeleton draws only its own\n");
}

static void test_model_parts(void)
{
    Fighter *m;
    DCDisplay *disp;
    s32 i, t, n, tag;

    sprite_bank_release_all();
    ftManagerReleaseFilesAll();
    load_stage();
    mock_gobj = NULL;
    mock_gobj2 = NULL;
    fpp2 = NULL;
    gFTManagerModels[nFTKindMario] = NULL;

    /* Link */
    ftManagerSetupFilesAllKind(nFTKindLink);
    spawn_kind(nFTKindLink, 0.0f, 0.0f);
    m = dc_model_of(mock_gobj);
    /* fifteen model part tags, then seventeen of the skeleton (V14) */
    CHECK(m != NULL && m->parts != NULL && m->parts->tag_count == 32);
    if (m == NULL || m->parts == NULL)
    {
        return;
    }
    n = (s32)m->hd->joint_count;
    disp = m->disp;

    /* every tag is one vertex run, in order, on the joint it names */
    for (t = 0; t < (s32)m->parts->tag_count; t++)
    {
        CHECK(t == 0 || m->part_tags[t].vert_first >=
                        m->part_tags[t - 1].vert_first + m->part_tags[t - 1].vert_count);
        CHECK(v06_tag_batches(m, t + 1, m->part_tags[t].joint) > 0);
    }
    /* joints 20 and 21 (entries 16, 17) have no display list in the tree:
     * no payload, part -1 -- then ftManagerInitFighter's defaults, 21 -1
     * and 19 0, applied by the first status */
    CHECK(fpp->modelpart_status[16].modelpart_id_base == -1);
    CHECK(fpp->joints[20]->dv == NULL);
    CHECK(fpp->modelpart_status[21 - 4].modelpart_id_base == -1);
    CHECK(fpp->joints[21]->dv == NULL);
    CHECK(fpp->modelpart_status[19 - 4].modelpart_id_curr == 0);
    CHECK(fpp->joints[19]->dv == &disp[15]);
    CHECK(m->part_joints[15].tree == v06_part_tag(m, 15, 0));

    /* the boomerang hand, joint 11: part 1 is baked out of the boomerang's
     * file, and is what the walk then records */
    tag = v06_part_tag(m, 7, 1);
    CHECK(tag > 0 && tag != m->part_joints[7].tree);
    ftParamSetModelPartID(mock_gobj, 11, 1);
    CHECK(fpp->modelpart_status[7].modelpart_id_curr == 1);
    CHECK(fpp->joints[11]->dv == &disp[n + tag - 1]);
    CHECK(((DCDisplay *)fpp->joints[11]->dv)->joint == 7);
    CHECK(fpp->is_modelpart_modify == TRUE);
    gcDrawDObjTreeForGObj(mock_gobj);
    CHECK(m->part_cur[7] == tag);
    CHECK(m->part_cur[15] == m->part_joints[15].tree);

    /* the sheath on, then off */
    ftParamSetModelPartID(mock_gobj, 21, 0);
    CHECK(fpp->joints[21]->dv == &disp[n + v06_part_tag(m, 17, 0) - 1]);
    ftParamSetModelPartID(mock_gobj, 21, -1);
    CHECK(fpp->joints[21]->dv == NULL);

    /* hide all, then back to base */
    ftParamHideModelPartAll(mock_gobj);
    for (i = 4; i < FTPARTS_JOINT_NUM_MAX; i++)
    {
        CHECK(fpp->joints[i] == NULL || fpp->joints[i]->dv == NULL);
    }
    ftParamResetModelPartAll(mock_gobj);
    CHECK(fpp->is_modelpart_modify == FALSE);
    CHECK(fpp->joints[11]->dv == &disp[7]);
    CHECK(fpp->joints[19]->dv == &disp[15]);
    CHECK(fpp->joints[21]->dv == NULL);
    CHECK(fpp->joints[4]->dv == &disp[0]);
    gcDrawDObjTreeForGObj(mock_gobj);
    CHECK(m->part_cur[7] == m->part_joints[7].tree);

    /* Kirby's heads: fifteen ids, each its own tag on entry 2 but the
     * copies that share a display list */
    ftManagerSetupFilesAllKind(nFTKindKirby);
    spawn_kind(nFTKindKirby, 0.0f, 0.0f);
    m = dc_model_of(mock_gobj);
    CHECK(m != NULL && m->parts != NULL);
    if (m == NULL || m->parts == NULL)
    {
        return;
    }
    CHECK(m->part_joints[2].id_count == 15);
    for (i = 1; i < 15; i++)
    {
        tag = v06_part_tag(m, 2, i);
        CHECK(tag > 0 && tag != v06_part_tag(m, 2, 0));
        CHECK(v06_tag_batches(m, tag, 2) > 0);
    }
    tag = v06_part_tag(m, 2, 5);
    ftParamSetModelPartID(mock_gobj, FTKIRBY_COPY_MODELPARTS_JOINT, 5);
    gcDrawDObjTreeForGObj(mock_gobj);
    CHECK(m->part_cur[2] == tag);
    ftParamResetModelPartAll(mock_gobj);
    gcDrawDObjTreeForGObj(mock_gobj);
    CHECK(m->part_cur[2] == m->part_joints[2].tree);

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    printf("model parts: Link's 15 and Kirby's %u part tags swap onto their "
           "joints, hide and reset\n", (unsigned)m->parts->tag_count);
}

/* the costumes. Every pack carries each costume the
 * game can deal as a row of texture and colours per batch over costume
 * 0's mesh; ftManagerMakeFighter and ftParamInitAllParts pick the row. */
static void test_costumes(void)
{
    static const s32 counts[12] = { 5, 4, 5, 5, 4, 4, 6, 6, 5, 4, 4, 4 };
    Fighter *m;
    s32 k, c, b, differ;

    sprite_bank_release_all();
    ftManagerReleaseFilesAll();
    load_stage();
    mock_gobj = NULL;
    mock_gobj2 = NULL;
    fpp2 = NULL;
    gFTManagerModels[nFTKindMario] = NULL;

    for (k = nFTKindPlayableStart; k <= nFTKindPlayableEnd; k++)
    {
        const Fighter *pack;

        ftManagerSetupFilesAllKind(k);
        pack = gFTManagerModels[k];
        CHECK(pack != NULL && pack->costumes != NULL);
        if (pack == NULL || pack->costumes == NULL)
        {
            continue;
        }
        /* one past dFTParamCostumeIDs' largest id for the kind */
        CHECK((s32)pack->costumes->costume_count == counts[k - nFTKindPlayableStart]);
        /* costume 0's row is the batches' own */
        for (b = 0; b < (s32)pack->hd->batch_count; b++)
        {
            CHECK(pack->costume_mats[b].tex == pack->batches[b].tex);
            CHECK(pack->costume_mats[b].prim == pack->batches[b].prim);
            CHECK(pack->costume_mats[b].light1 == pack->batches[b].light1);
        }
        /* and every other costume changes the body -- but Pikachu's 2
         * and Jigglypuff's 1, which are costume 0 wearing the accessory
         * (the hat and the bow, FTAttributes.accesspart) */
        for (c = 1; c < (s32)pack->costumes->costume_count; c++)
        {
            s32 body = 0;

            differ = 0;
            for (b = 0; b < (s32)pack->hd->batch_count; b++)
            {
                const FPackCostumeMat *x = &pack->costume_mats[b];
                const FPackCostumeMat *y = &pack->costume_mats[c * pack->hd->batch_count + b];
                s32 d = (x->tex != y->tex) || (x->prim != y->prim) ||
                        (x->light1 != y->light1) || (x->light2 != y->light2);

                differ += d;
                body += d && ((pack->parts == NULL) || (pack->parts->accessory_tag == 0) ||
                              (pack->batches[b].part != pack->parts->accessory_tag));
            }
            /* Jigglypuff's first bow is its costume 0 texture, the only
             * costume to be the same model but for wearing it */
            CHECK((differ == 0) == ((k == nFTKindPurin) && (c == 1)));
            CHECK((body == 0) == (((k == nFTKindPikachu) && (c == 2)) ||
                                  ((k == nFTKindPurin) && (c == 1))));
        }
    }

    /* the spawn wears desc.costume, and the character select's change
     * reaches the model */
    ftManagerSetupFilesAllKind(nFTKindCaptain);
    spawn_kind(nFTKindCaptain, 0.0f, 0.0f);
    m = dc_model_of(mock_gobj);
    CHECK(m != NULL && m->costume == 0);
    ftParamInitAllParts(mock_gobj, 3, 1);
    CHECK(m != NULL && m->costume == 3 && fpp->costume == 3);
    ftParamInitAllParts(mock_gobj, 9, 1);
    CHECK(m != NULL && m->costume == 5 && fpp->costume == 9);
    ftParamInitAllParts(mock_gobj, 0, 1);
    CHECK(m != NULL && m->costume == 0);
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    printf("costumes: every kind carries its costumes, and the fighter wears "
           "the one it is dealt\n");
}

/* the demo statuses. scSubsysFighterSetStatus used to turn
 * every one of them into nFTCommonStatusWait; now ftMainSetStatus reads
 * scsubsysdata.c's FTOpeningDesc tables and the pack's SubMotion table,
 * and a posed fighter is posed.
 *
 * The three things worth holding: a common-band status reaches the
 * SubMotion row of the same number and NOT the MainMotion row of that
 * number (the two tables disagree, which is the whole point); the rows
 * that carry a proc install it and the rest install none; and Master
 * Hand's IntroR -- rung 13's stage card -- plays FTBossAnimPose1P. */
static void test_demo_status(void)
{
    const Fighter *m;
    s32 wait_motion;

    sprite_bank_release_all();
    ftManagerReleaseFilesAll();
    load_stage();
    mock_gobj = NULL;
    gFTManagerModels[nFTKindMario] = NULL;

    ftManagerSetupFilesAllKind(nFTKindMario);
    spawn_kind(nFTKindMario, 0.0f, 0.0f);
    m = dc_model_of(mock_gobj);
    CHECK(m != NULL && m->submotions != NULL);
    fpp->pkind = nFTPlayerKindDemo;

    /* Wait first, so the comparison below is against a real MainMotion
     * row rather than whatever the spawn left */
    scSubsysFighterSetStatus(mock_gobj, nFTCommonStatusWait);
    wait_motion = fpp->motion_id;
    CHECK(fpp->status_id == nFTCommonStatusWait);

    if (m != NULL && m->submotions != NULL)
    {
        s32 k;

        /* every status of the common band lands on its own SubMotion
         * row, and the status id is kept -- not rewritten to Wait */
        for (k = nFTDemoStatusNull; k <= nFTDemoStatusIntroR; k++)
        {
            scSubsysFighterSetStatus(mock_gobj, k);
            CHECK(fpp->status_id == k);
            CHECK(fpp->motion_id == k - FTSTAT_OPENING2_START);
        }
        /* Win1 is FTMarioAnimWin2 out of the SubMotion table; the
         * MainMotion row of the same number is something else entirely,
         * so reading the wrong table cannot pass this */
        scSubsysFighterSetStatus(mock_gobj, nFTDemoStatusWin1);
        k = m->submotions[fpp->motion_id].anim;
        CHECK(k >= 0 && (u32)k < m->hd->anim_count);
        CHECK(k >= 0 && strcmp(m->anims[k].name, "FTMarioAnimWin2") == 0);
        CHECK(m->motions[fpp->motion_id].anim != k);
        /* the row's event script: Win1's plays
         * nSYAudioVoiceMarioHereWe, so the pose speaks, and it is
         * installed by the same arm of ftMainSetStatus a MainMotion
         * row's is. What that arm does is what cannot be tested HERE:
         * fighter_load detaches both tables' scripts on the host,
         * because a script's pointer words are four bytes in the pack
         * and eight in this build. The shipped columns are checked
         * against the file in test_ftmanager; here the only thing to
         * say is that the detach reaches the new table too, which is
         * what keeps the interpreter off a truncated pointer. */
        CHECK(m->submotions[fpp->motion_id].script == 0);
        CHECK(fpp->motion_scripts[0][0].p_script == NULL);

        /* the proc column: FigureDropped and FigureStand carry
         * scSubsysFighterApplyVelTransN, the poses around them none */
        scSubsysFighterSetStatus(mock_gobj, nFTDemoStatusFigureDropped);
        CHECK(fpp->proc_update == scSubsysFighterApplyVelTransN);
        scSubsysFighterSetStatus(mock_gobj, nFTDemoStatusFigureStand);
        CHECK(fpp->proc_update == scSubsysFighterApplyVelTransN);
        /* FigurePulled's proc is mvOpeningFighterProcUpdate, the third
         * and last row of the common table to carry one, and the one
         * that reaches out of overlay 1 -- it holds the plucked trophy
         * against the opening room's Master Hand. */
        scSubsysFighterSetStatus(mock_gobj, nFTDemoStatusFigurePulled);
        CHECK(fpp->proc_update == mvOpeningFighterProcUpdate);
        CHECK(m->submotions[fpp->motion_id].anim >= 0);
        scSubsysFighterSetStatus(mock_gobj, nFTDemoStatusIntroL);
        CHECK(fpp->proc_update == NULL);

        /* Mario's own opening band: four rows, and the fifth status is
         * past his table -- ftMainGetOpeningDesc refuses it by the row's
         * own motion id rather than reading into Fox's table */
        scSubsysFighterSetStatus(mock_gobj, FTSTAT_OPENING1_START + 3);
        CHECK(fpp->motion_id == 18);
        CHECK(m->submotions[18].anim >= 0);

        /* a battle fighter still takes the status tables, and only the
         * battle arm sets proc_slope -- which is what tells the two
         * arms of ft/ftmain.c:4801-4822 apart from outside */
        fpp->proc_slope = NULL;
        fpp->pkind = nFTPlayerKindMan;
        scSubsysFighterSetStatus(mock_gobj, nFTCommonStatusWait);
        CHECK(fpp->motion_id == wait_motion);
        CHECK(fpp->proc_slope == mpCommonUpdateFighterSlopeContour);
        CHECK(fpp->proc_interrupt != NULL);
    }
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;

    /* Master Hand's stage-card pose (rung 13) */
    ftManagerSetupFilesAllKind(nFTKindBoss);
    spawn_kind(nFTKindBoss, 0.0f, 0.0f);
    m = dc_model_of(mock_gobj);
    fpp->pkind = nFTPlayerKindDemo;
    scSubsysFighterSetStatus(mock_gobj, nFTDemoStatusIntroR);
    CHECK(m != NULL && m->submotions != NULL &&
          fpp->motion_id == nFTDemoStatusIntroR - FTSTAT_OPENING2_START);
    if (m != NULL && m->submotions != NULL)
    {
        s32 k = m->submotions[fpp->motion_id].anim;

        CHECK(k >= 0 && (u32)k < m->hd->anim_count);
        CHECK(k >= 0 && strcmp(m->anims[k].name, "FTBossAnimPose1P") == 0);
    }
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    gFTManagerModels[nFTKindMario] = NULL;
    printf("demo status: the fifteen common poses and Mario's own four "
           "come out of the pack's SubMotion table, the drop and the "
           "stand carry scSubsysFighterApplyVelTransN, and Master Hand's "
           "1P stage card is FTBossAnimPose1P\n");
}

/* one ftDisplayMainProcDisplay of the spawned fighter, as the character
 * select's demo kind: no magnify camera in the way */
static void accessory_draw(void)
{
    s32 pkind = fpp->pkind;

    fpp->pkind = nFTPlayerKindDemo;
    ftDisplayMainProcDisplay(mock_gobj);
    fpp->pkind = pkind;
}

/* Pikachu's hat and Jigglypuff's bow. The accessory is one
 * more tag of the pack on the attribute's joint; a costume other than 0
 * makes the joint's parts GObj (ftManagerMakeFighter, ftParamInitAllParts)
 * and the display turns the tag on under it. */
static void test_accessory(void)
{
    static const s32 kinds[2] = { nFTKindPikachu, nFTKindPurin };
    Fighter *m;
    FTParts *parts;
    FTAccessPart *ap;
    s32 k, tag;

    sprite_bank_release_all();
    ftManagerReleaseFilesAll();
    load_stage();
    mock_gobj = NULL;
    mock_gobj2 = NULL;
    fpp2 = NULL;
    gFTManagerModels[nFTKindMario] = NULL;

    ftManagerSetupFilesAllKind(nFTKindMario);
    spawn_kind(nFTKindMario, 0.0f, 0.0f);
    CHECK(fpp->attr->accesspart == NULL);
    m = dc_model_of(mock_gobj);
    CHECK(m != NULL && (m->parts == NULL || m->parts->accessory_tag == 0));
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;

    for (k = 0; k < 2; k++)
    {
        ftManagerSetupFilesAllKind(kinds[k]);
        spawn_kind(kinds[k], 0.0f, 0.0f);
        m = dc_model_of(mock_gobj);
        ap = fpp->attr->accesspart;
        CHECK(m != NULL && m->parts != NULL && ap != NULL);
        if (m == NULL || m->parts == NULL || ap == NULL)
        {
            continue;
        }
        tag = (s32)m->parts->accessory_tag;
        CHECK(tag != 0 && tag <= (s32)m->parts->tag_count);
        CHECK(ap->dl == NULL && ap->mobjsubs == NULL);
        CHECK(fpp->joints[ap->joint_id] != NULL);
        CHECK(v06_tag_batches(m, tag, m->part_tags[tag - 1].joint) > 0);

        /* costume 0 wears none */
        parts = ftGetParts(fpp->joints[ap->joint_id]);
        CHECK(parts->gobj == NULL);
        accessory_draw();
        CHECK(!m->accessory_on);
        CHECK(!fighter_part_on(m, tag, m->part_tags[tag - 1].joint));

        ftParamInitAllParts(mock_gobj, 2, 0);
        CHECK(parts->gobj != NULL);
        accessory_draw();
        CHECK(m->accessory_on);
        CHECK(fighter_part_on(m, tag, m->part_tags[tag - 1].joint));

        /* hidden joints draw nothing, their accessory included */
        fpp->joints[ap->joint_id]->flags |= DOBJ_FLAG_HIDDEN;
        accessory_draw();
        CHECK(!m->accessory_on);
        fpp->joints[ap->joint_id]->flags &= ~DOBJ_FLAG_HIDDEN;

        /* a re-deal ejects the old GObj, and back to 0 leaves none */
        ftParamInitAllParts(mock_gobj, 1, 0);
        CHECK(parts->gobj != NULL);
        ftParamInitAllParts(mock_gobj, 0, 0);
        CHECK(parts->gobj == NULL);
        accessory_draw();
        CHECK(!m->accessory_on);

        ftManagerDestroyFighter(mock_gobj);
        mock_gobj = NULL;

        /* and a fighter dealt a costume other than 0 is made wearing it */
        spawn_costume = 3;
        spawn_kind(kinds[k], 0.0f, 0.0f);
        spawn_costume = 0;
        m = dc_model_of(mock_gobj);
        CHECK(m != NULL && m->costume == 3);
        CHECK(ftGetParts(fpp->joints[ap->joint_id])->gobj != NULL);
        accessory_draw();
        CHECK(m != NULL && m->accessory_on);
        ftManagerDestroyFighter(mock_gobj);
        mock_gobj = NULL;
    }
    printf("accessory: the hat and the bow are worn in every costume but 0\n");
}

/* Ness's entry silhouette (joint 12, part 1) is the one VS
 * part drawn out of the fog (FTPARTS_FLAG_NOFOG), and the display hands
 * each joint's flags to the model. */
static void test_part_fog(void)
{
    Fighter *m;
    s32 b, n;

    sprite_bank_release_all();
    ftManagerReleaseFilesAll();
    load_stage();
    mock_gobj = NULL;
    mock_gobj2 = NULL;
    fpp2 = NULL;
    gFTManagerModels[nFTKindMario] = NULL;

    ftManagerSetupFilesAllKind(nFTKindNess);
    spawn_kind(nFTKindNess, 0.0f, 0.0f);
    m = dc_model_of(mock_gobj);
    CHECK(m != NULL);
    if (m == NULL)
    {
        return;
    }
    accessory_draw();
    for (n = 0, b = 0; b < FIGHTER_MAX_JOINTS; b++)
    {
        n += (m->part_flags[b] & FIGHTER_PART_NOFOG) != 0;
    }
    CHECK(n == 0);

    ftParamSetModelPartID(mock_gobj, 12, 1);
    CHECK(ftGetParts(fpp->joints[12])->flags == FTPARTS_FLAG_NOFOG);
    accessory_draw();
    CHECK(m->part_flags[12 - nFTPartsJointCommonStart] == FTPARTS_FLAG_NOFOG);
    /* the part is drawn, on that joint */
    for (n = 0, b = 0; b < (s32)m->hd->batch_count; b++)
    {
        if (m->batches[b].part != 0 && m->batches[b].joint == 12 - nFTPartsJointCommonStart &&
            fighter_part_on(m, m->batches[b].part, m->batches[b].joint))
        {
            n++;
        }
    }
    CHECK(n > 0);

    ftParamSetModelPartID(mock_gobj, 12, 0);
    accessory_draw();
    CHECK(m->part_flags[12 - nFTPartsJointCommonStart] == 0);

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    printf("part fog: Ness's entry silhouette is drawn out of the fog\n");
}

/* Samus's grapple beam (joints 17-22, part 0) hangs MObjs
 * whose own material script flickers the segment through three tiles; the
 * pack carries them (FPackMObjs) and the part setters hang them. */
static void test_part_mobjs(void)
{
    Fighter *m;
    MObj *mobj;
    s32 b, n;

    sprite_bank_release_all();
    ftManagerReleaseFilesAll();
    load_stage();
    mock_gobj = NULL;
    mock_gobj2 = NULL;
    fpp2 = NULL;
    gFTManagerModels[nFTKindMario] = NULL;

    /* Mario's pack has none */
    ftManagerSetupFilesAllKind(nFTKindMario);
    spawn_kind(nFTKindMario, 0.0f, 0.0f);
    CHECK(dc_model_of(mock_gobj) != NULL && dc_model_of(mock_gobj)->mobjs == NULL);
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;

    ftManagerSetupFilesAllKind(nFTKindSamus);
    spawn_kind(nFTKindSamus, 0.0f, 0.0f);
    m = dc_model_of(mock_gobj);
    CHECK(m != NULL && m->mobjs != NULL);
    if (m == NULL || m->mobjs == NULL)
    {
        return;
    }
    CHECK(m->mobjs->mobj_count == 12);
    for (n = 0, b = 0; b < (s32)m->hd->batch_count; b++)
    {
        if (m->mobj_batch[b] >= 0)
        {
            n++;
            CHECK(m->batches[b].joint >= 13 && m->batches[b].joint <= 18);
            CHECK(m->mobj_subs[m->mobj_batch[b]].tex_count == 3);
        }
    }
    CHECK(n == 6);

    /* the beam is a hidden part the Catch animation links (hidden-part
     * row 4 roots joint 17), and off until the motion script puts it on */
    CHECK(fpp->joints[17] == NULL);
    ftMainUpdateHiddenPartID(fpp, 4);
    CHECK(fpp->joints[17] != NULL);
    if (fpp->joints[17] == NULL)
    {
        return;
    }
    /* the entry has no display list of its own, so no part is on */
    CHECK(fpp->modelpart_status[17 - nFTPartsJointCommonStart].modelpart_id_curr == -1);
    CHECK(fpp->joints[17]->dv == NULL);
    CHECK(fpp->joints[17]->mobj == NULL);
    ftParamSetModelPartID(mock_gobj, 17, 0);
    CHECK(fpp->joints[17]->dv != NULL);
    mobj = fpp->joints[17]->mobj;
    CHECK(mobj != NULL && mobj->next != NULL && mobj->next->next == NULL);
    CHECK(fpp->joints[18] == NULL || fpp->joints[18]->mobj == NULL);

    /* each on its own script, the pack's: the first holds tile 0 for 50
     * frames (SetValBlock) and then steps through 1 and 2. The script is
     * not run here: the host's AObjEvent32 holds a pointer and is 8
     * bytes, so gcParseMObjMatAnimJoint would step over the pack's 4-byte
     * words two at a time; the disc probe watches the tiles change. */
    if (mobj != NULL)
    {
        const u8 *words = (const u8 *)m->blob + m->mobjs->off_words;
        const s32 *entry = (const s32 *)((const u8 *)m->blob + m->mobjs->off_entry);
        s32 first = m->mobj_joint[13 * 2];

        CHECK(m->mobj_joint[13 * 2 + 1] == 2);
        CHECK(mobj->matanim_joint.event32 != NULL);
        CHECK(entry[first] >= 0 && entry[first + 1] >= 0);
        CHECK(((const u32 *)words)[entry[first] + 2] == 0x06008032);
        CHECK(mobj->texture_id_curr == 0);
    }

    ftParamSetModelPartID(mock_gobj, 17, -1);
    CHECK(fpp->joints[17]->mobj == NULL);
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    printf("part mobjs: Samus's grapple beam hangs its flickering MObjs\n");
}

/* V07: Link's sword trail. ftMainProcParams samples the sword joint a
 * frame at a time once a swing's motion script turns the trail on, and
 * ftDisplayMainDrawAfterImage strips between the samples. */
static void test_afterimage(void)
{
    s32 f;

    sprite_bank_release_all();
    ftManagerReleaseFilesAll();
    load_stage();
    mock_gobj = NULL;
    mock_gobj2 = NULL;
    fpp2 = NULL;
    gFTManagerModels[nFTKindMario] = NULL;

    ftManagerSetupFilesAllKind(nFTKindLink);
    spawn_kind(nFTKindLink, 0.0f, 0.0f);
    idle_frames(2);
    CHECK(fpp->afterimage.drawstatus == -1);
    CHECK(fpp->modelpart_status[11 - nFTPartsJointCommonStart].modelpart_id_curr == 0);

    /* a swing's script turns it on (SetAfterImage, ftmain.c's motion
     * events); each frame outside hitlag samples the sword, to three */
    fpp->afterimage.is_itemswing = FALSE;
    fpp->afterimage.drawstatus = 0;
    fpp->afterimage.desc_id = 0;
    for (f = 0; f < 4; f++)
    {
        ftMainProcParams(mock_gobj);
    }
    CHECK(fpp->afterimage.drawstatus == 3);
    CHECK(fpp->afterimage.desc_id == 1);
    for (f = 0; f < 3; f++)
    {
        f32 len = syVectorMag3D(&fpp->afterimage.desc[f].vec);

        CHECK(len > 0.5F);
    }
    /* Mario has no sword: nothing is sampled */
    fpp->fkind = nFTKindMario;
    fpp->afterimage.drawstatus = 0;
    ftMainProcParams(mock_gobj);
    CHECK(fpp->afterimage.drawstatus == 0);
    fpp->fkind = nFTKindLink;

    /* three samples, forced, a quarter turn apart */
    fpp->afterimage.is_itemswing = FALSE;
    fpp->afterimage.drawstatus = 3;
    fpp->afterimage.desc_id = 0;
    fpp->afterimage.desc[0].translate_x = 0;
    fpp->afterimage.desc[0].translate_y = 0;
    fpp->afterimage.desc[0].translate_z = 0;
    fpp->afterimage.desc[0].vec.x = 1.0F;
    fpp->afterimage.desc[0].vec.y = 0.0F;
    fpp->afterimage.desc[0].vec.z = 0.0F;
    fpp->afterimage.desc[1] = fpp->afterimage.desc[0];
    fpp->afterimage.desc[1].vec.x = 0.0F;
    fpp->afterimage.desc[1].vec.y = 1.0F;
    fpp->afterimage.desc[2] = fpp->afterimage.desc[0];
    fpp->afterimage.desc[2].vec.x = -1.0F;
    fpp->afterimage.desc[2].vec.y = 0.0F;
    ftDisplayMainDrawAfterImage(fpp);

    /* newest (desc 2) first, at 0xFE (255 / 2 * 2): its inner edge 50
     * out and cyan, its outer 250 out and white; three fill pairs between
     * each two samples (a quarter turn in thirty-degree steps), desc 1 at
     * vertex 8 and desc 0 last at alpha 0 */
    CHECK(gFTDisplayMainAfterImageVtxCount == 18);
    CHECK(gFTDisplayMainAfterImageTriCount == 16);
    CHECK(gFTDisplayMainAfterImageVtx[0].v.ob[0] == -50 && gFTDisplayMainAfterImageVtx[1].v.ob[0] == -250);
    CHECK(gFTDisplayMainAfterImageVtx[0].v.cn[0] == 0x00 && gFTDisplayMainAfterImageVtx[0].v.cn[1] == 0xFF &&
          gFTDisplayMainAfterImageVtx[0].v.cn[3] == 0xFE);
    CHECK(gFTDisplayMainAfterImageVtx[1].v.cn[0] == 0xFF && gFTDisplayMainAfterImageVtx[1].v.cn[3] == 0xFE);
    CHECK(gFTDisplayMainAfterImageVtx[8].v.ob[0] == 0 && gFTDisplayMainAfterImageVtx[8].v.ob[1] == 50);
    CHECK(gFTDisplayMainAfterImageVtx[17].v.ob[0] == 250 && gFTDisplayMainAfterImageVtx[17].v.cn[3] == 0);

    /* the Beam Sword's colours and reach */
    fpp->afterimage.is_itemswing = TRUE;
    ftDisplayMainDrawAfterImage(fpp);
    CHECK(gFTDisplayMainAfterImageVtx[0].v.ob[0] == -80 && gFTDisplayMainAfterImageVtx[1].v.ob[0] == -580);
    CHECK(gFTDisplayMainAfterImageVtx[0].v.cn[0] == 0xFF && gFTDisplayMainAfterImageVtx[0].v.cn[1] == 0x40);

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    printf("afterimage: Link's sword leaves its trail\n");
}





/* the texture parts -- the faces. The pack bakes a
 * tile per frame a motion script sets beside each batch the part's MObj
 * drew; ftParamSetTexturePartID and its two neighbours put the part on a
 * frame, and the part with no frames (Mario's second row, which no
 * script names) is left alone as the game's walk leaves it. */
static void test_texture_parts(void)
{
    Fighter *m;
    s32 b, e, n;

    sprite_bank_release_all();
    ftManagerReleaseFilesAll();
    load_stage();
    mock_gobj = NULL;
    mock_gobj2 = NULL;
    fpp2 = NULL;
    gFTManagerModels[nFTKindMario] = NULL;

    ftManagerSetupFilesAllKind(nFTKindMario);
    spawn_kind(nFTKindMario, 0.0f, 0.0f);
    m = dc_model_of(mock_gobj);
    CHECK(m != NULL && m->texparts != NULL);
    if (m == NULL || m->texparts == NULL)
    {
        return;
    }
    CHECK(fpp->attr->textureparts_container != NULL);
    CHECK(fpp->attr->textureparts_container->textureparts[0].joint_id == 12);
    CHECK(m->texparts->frame_count[0] == 4 && m->texparts->frame_count[1] == 0);

    /* one batch, the eyes: frame 0 is its own tile and the other three
     * are other tiles, costume by costume */
    n = 0;
    for (b = 0; b < (s32)m->hd->batch_count; b++)
    {
        const FPackTexPartBatch *tb = &m->texpart_batches[b];

        if (tb->part == 0xFF)
        {
            continue;
        }
        n++;
        CHECK(tb->part == 0 && m->batches[b].joint == 8);
        CHECK(m->texpart_frames[tb->frame_first] == m->batches[b].tex);
        for (e = 1; e < 4; e++)
        {
            CHECK(m->texpart_frames[tb->frame_first + e] >= 0);
            CHECK(m->texpart_frames[tb->frame_first + e] != m->batches[b].tex);
        }
    }
    CHECK(n == 1);

    ftParamSetTexturePartID(mock_gobj, 0, 2);
    CHECK(m->texpart_frame[0] == 2);
    CHECK(fpp->texturepart_status[0].texture_id_curr == 2);
    CHECK(fpp->is_texturepart_modify == TRUE);
    ftParamSetTexturePartID(mock_gobj, 1, 3);
    CHECK(m->texpart_frame[1] == 0);
    CHECK(fpp->texturepart_status[1].texture_id_curr == 0);
    ftParamResetTexturePartAll(mock_gobj);
    CHECK(m->texpart_frame[0] == 0);
    CHECK(fpp->texturepart_status[0].texture_id_curr == 0);
    CHECK(fpp->is_texturepart_modify == FALSE);

    /* a costume change re-deals the frame the status holds */
    fpp->texturepart_status[0].texture_id_curr = 3;
    m->texpart_frame[0] = 0;
    ftParamInitAllParts(mock_gobj, 1, 1);
    CHECK(m->texpart_frame[0] == 3 && m->costume == 1);
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;

    /* Donkey Kong has no container, and nothing happens */
    ftManagerSetupFilesAllKind(nFTKindDonkey);
    spawn_kind(nFTKindDonkey, 0.0f, 0.0f);
    CHECK(fpp->attr->textureparts_container == NULL);
    ftParamSetTexturePartID(mock_gobj, 0, 1);
    ftParamResetTexturePartAll(mock_gobj);
    CHECK(fpp->texturepart_status[0].texture_id_curr == 0);
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    printf("texture parts: Mario's eyes bake 4 frames and switch, reset and "
           "survive a costume change\n");
}

/* ft/ftshadow.c, all of it: the blob under a fighter. ftShadowProcDisplay
 * projects his x +- attr->shadow_size onto the floor line he is standing
 * over and builds the strip by hand, so what is checkable on the host is
 * the whole of the arithmetic -- the two edges, the altitude at each,
 * the texture coordinates, the strip's index order, and the clamp that
 * stops the shadow at a ledge. The PVR triangles it ends in are the
 * target's; src/dc/db.c's probe is what reads those.
 *
 * The altitude is checked against mpCollisionGetFCCommonFloor rather
 * than against ftShadowGetAltitude: a test that read the same line by
 * the same route would pass whatever the route returned. */
static void test_fighter_shadow(void)
{
    GObj *shadow_gobj;
    const FTShadowDraw *d;
    Vec3f edge_l, edge_r, probe;
    f32 size, x, dist;
    u32 flags;
    Vec3f angle;
    s32 line_id, k;

    sprite_bank_release_all();
    ftManagerReleaseFilesAll();
    load_stage();
    mock_gobj = NULL;
    mock_gobj2 = NULL;
    fpp2 = NULL;
    gFTManagerModels[nFTKindMario] = NULL;

    /* ---- the field, on all twelve ----
     *
     * fttypes.h:903, read out of each fighter's Main file at attribute
     * offset 0x7C (tools/export/ssb_packexport.py). Every fighter but Donkey
     * Kong is 200; his is 350. Without the pack field all
     * twelve would read 0 -- and a zero shadow_size divides in
     * ftShadowProcDisplay's two clamps. */
    for (k = nFTKindPlayableStart; k <= nFTKindPlayableEnd; k++)
    {
        ftManagerSetupFilesAllKind(k);
        spawn_kind(k, 0.0f, 0.0f);

        CHECK(fpp->attr->shadow_size == ((k == nFTKindDonkey) ? 350.0f
                                                              : 200.0f));
    }

    /* ---- the strip, on Mario, over the floor he is standing on ---- */
    ftManagerSetupFilesAllKind(nFTKindMario);
    spawn_kind(nFTKindMario, 0.0f, 0.0f);
    idle_frames(8);

    CHECK(fpp->ga == nMPKineticsGround);
    line_id = fpp->coll_data.floor_line_id;
    CHECK(line_id >= 0);

    /* The shadow ftManagerMakeFighter made for him, the newest GObj on
     * link 13 -- gcMakeGObjSPAfter puts it at the head. */
    shadow_gobj = gGCCommonLinks[nGCCommonLinkIDShadow];
    CHECK(shadow_gobj != NULL);
    CHECK(shadow_gobj->user_data.p != NULL);
    CHECK(((FTShadow *)shadow_gobj->user_data.p)->player == fpp->player);

    /* The proc draws in the translucent pass and in no other: the frame
     * runs the battle camera once per PVR list. */
    ftShadowResetFrame();
    gcSetDrawList(PVR_LIST_OP_POLY);
    ftShadowProcDisplay(shadow_gobj);
    CHECK(gFTShadowLogCount == 0);
    gcSetDrawList(PVR_LIST_PT_POLY);
    ftShadowProcDisplay(shadow_gobj);
    CHECK(gFTShadowLogCount == 0);

    gcSetDrawList(PVR_LIST_TR_POLY);
    ftShadowProcDisplay(shadow_gobj);
    CHECK(gFTShadowLogCount == 1);

    d = &gFTShadowLog[0];
    size = fpp->attr->shadow_size;
    x = DObjGetStruct(mock_gobj)->translate.vec.f.x;

    /* ftcommondata.c:350 dFTCommonDataShadowColorDefault, which is what
     * a free-for-all draws: black at 0xA0. */
    CHECK(d->prim[0] == 0x00 && d->prim[1] == 0x00 &&
          d->prim[2] == 0x00 && d->prim[3] == 0xA0);

    /* A floor line wider than the shadow and flat under it: four
     * vertices, two triangles, the strip's own index order. */
    mpCollisionGetFloorEdgeL(line_id, &edge_l);
    mpCollisionGetFloorEdgeR(line_id, &edge_r);
    CHECK(edge_l.x < x - size && edge_r.x > x + size);
    CHECK(d->vtx_num == 4);
    CHECK(d->tri_num == 2);
    CHECK(d->tri[0][0] == 1 && d->tri[0][1] == 0 && d->tri[0][2] == 3);
    CHECK(d->tri[1][0] == 0 && d->tri[1][1] == 2 && d->tri[1][2] == 3);

    /* The two edges, at the fighter's x either side of shadow_size, and
     * the quad's depth: the strip runs +200 to -200 in world z, which is
     * the S axis of the texture and 0 .. 1984 in the RDP's S10.5. */
    CHECK(d->vtx[0].n.ob[0] == (s16)(x - size));
    CHECK(d->vtx[1].n.ob[0] == d->vtx[0].n.ob[0]);
    CHECK(d->vtx[2].n.ob[0] == (s16)(x + size));
    CHECK(d->vtx[3].n.ob[0] == d->vtx[2].n.ob[0]);
    CHECK(d->vtx[0].n.ob[2] == 200 && d->vtx[1].n.ob[2] == -200);
    CHECK(d->vtx[2].n.ob[2] == 200 && d->vtx[3].n.ob[2] == -200);
    CHECK(d->vtx[0].n.tc[0] == 0 && d->vtx[1].n.tc[0] == 1984);
    CHECK(d->vtx[2].n.tc[0] == 0 && d->vtx[3].n.tc[0] == 1984);

    /* The T axis is the shadow's own width, so with neither edge clamped
     * it is the whole texture: 0 at the left edge, 1984 at the right,
     * which after gsSPTexture's half scale is one mirrored period. */
    CHECK(d->vtx[0].n.tc[1] == 0 && d->vtx[1].n.tc[1] == 0);
    CHECK(d->vtx[2].n.tc[1] == 1984 && d->vtx[3].n.tc[1] == 1984);

    /* The altitude, against the collision's own answer rather than
     * ftShadowGetAltitude's: mpCollisionGetFCCommonFloor walks the same
     * line by a different route (mpcollision.c, compiled unmodified) and
     * returns the distance from a point to the floor under it. The two
     * multiply and divide in a different order, so this allows the one
     * unit the s16 truncation can differ by. */
    for (k = 0; k < 2; k++)
    {
        probe.x = d->vtx[k * 2].n.ob[0];
        probe.y = DObjGetStruct(mock_gobj)->translate.vec.f.y + 1.0f;
        probe.z = 0.0f;
        dist = 0.0f;
        CHECK(mpCollisionGetFCCommonFloor(line_id, &probe, &dist, &flags,
                                          &angle) != FALSE);
        CHECK(fabsf((f32)d->vtx[k * 2].n.ob[1] - (probe.y + dist)) <= 1.0f);
        CHECK(d->vtx[k * 2 + 1].n.ob[1] == d->vtx[k * 2].n.ob[1]);
    }

    /* ---- the left clamp ----
     *
     * Standing within shadow_size of the floor line's left edge, the
     * strip stops at the edge and the texture starts partway in:
     * ((edge - centre) + size) * 992 / size, which is 992 at the centre
     * and 0 a full shadow_size to its left. */
    ftMainRespawn(mock_gobj, edge_l.x + 0.5f * size, edge_l.y + 40.0f);
    idle_frames(8);
    CHECK(fpp->ga == nMPKineticsGround);
    CHECK(fpp->coll_data.floor_line_id == line_id);

    ftShadowResetFrame();
    ftShadowProcDisplay(shadow_gobj);
    CHECK(gFTShadowLogCount == 1);

    d = &gFTShadowLog[0];
    x = DObjGetStruct(mock_gobj)->translate.vec.f.x;
    CHECK(d->vtx[0].n.ob[0] == (s16)edge_l.x);
    CHECK(d->vtx[2].n.ob[0] == (s16)(x + size));
    CHECK(d->vtx[0].n.tc[1] == (s16)((((edge_l.x - x) + size) * 992.0f) / size));
    CHECK(d->vtx[0].n.tc[1] > 0 && d->vtx[0].n.tc[1] < 992);
    CHECK(d->vtx[2].n.tc[1] == 1984);

    /* ---- and the two switches that hide it ---- */
    ftMainRespawn(mock_gobj, 0.0f, 0.0f);
    idle_frames(8);

    fpp->is_shadow_hide = TRUE;
    ftShadowResetFrame();
    ftShadowProcDisplay(shadow_gobj);
    CHECK(gFTShadowLogCount == 0);
    fpp->is_shadow_hide = FALSE;

    fpp->is_invisible = TRUE;
    ftShadowResetFrame();
    ftShadowProcDisplay(shadow_gobj);
    CHECK(gFTShadowLogCount == 0);
    fpp->is_invisible = FALSE;

    ftShadowResetFrame();
    ftShadowProcDisplay(shadow_gobj);
    CHECK(gFTShadowLogCount == 1);

    printf("shadow: 12 shadow_size, a %d-vertex strip on the floor line, "
           "its left edge clamped to the ledge, hidden by is_invisible "
           "and is_shadow_hide\n", (int)gFTShadowLog[0].vtx_num);
}

/* ef/efground.c, Part B (Phase 1): the background
 * ground actors' engine -- the Lakitu over Peach's Castle and its kin.
 * Every real stage's dEFGroundDatas slot (efground.h) is still NULL as
 * of Phase 1 except Castle's -- and Castle's only once its own Stage
 * loads via grStageAcquire, which this host build never does -- so
 * efGroundMakeAppearActor is a no-op everywhere this suite runs. This
 * test proves that first, then drives the engine with a table of its
 * own, so the functions the private sEFGroundActor gates
 * (efGroundMakeEffectID, EFGroundActorProcUpdate,
 * efGroundSetupRandomWeights, and -- Phase 1 -- dcGroundMakeEffect/
 * dcGroundSetupEffectDObjs) run for real rather than by inspection. */

static GObj *ground_find_by_z(s32 link, f32 z)
{
    GObj *g;

    for (g = gGCCommonLinks[link]; g != NULL; g = g->link_next)
    {
        if (g->id == nGCCommonKindEffect)
        {
            DObj *d = DObjGetStruct(g);

            if ((d != NULL) && (d->translate.vec.f.z == z))
            {
                return g;
            }
        }
    }
    return NULL;
}

static void test_ground_actors(void)
{
    static EFGroundParam param[1];
    static EFGroundDesc desc[1];
    static EFGroundActorAsset asset[1];
    EFGroundData saved_data;
    const FPackAttr *saved_attr;
    u8 saved_gkind;
    s32 saved_scene;
    GObj *actor;
    GObj *ground;
    DObj *root, *mid, *leaf;
    EFStruct *ep;
    s32 i;
    static const s32 kinds[] =
    {
        nGRKindCastle, nGRKindSector, nGRKindJungle, nGRKindZebes,
        nGRKindHyrule, nGRKindYoster, nGRKindPupupu, nGRKindYamabuki,
        nGRKindInishie,
    };

    /* the grab tests' swirls took structs out of whatever pool was live;
     * start from a full one, as the other effect tests do */
    eject_model_effects();
    ef_pool_reset();

    /* -- every real stage's slot is NULL today: efGroundMakeAppearActor
     * does nothing anywhere in the game the port can play yet. -- */
    saved_gkind = gSCManagerBattleState->gkind;
    saved_scene = gSCManagerSceneData.scene_curr;
    gSCManagerSceneData.scene_curr = nSCKindVSBattle;

    for (i = 0; i < (s32)(sizeof(kinds) / sizeof(kinds[0])); i++)
    {
        GObj *before = gGCCommonLinks[7];

        CHECK(dEFGroundDatas[kinds[i]].effect_params == NULL);
        gSCManagerBattleState->gkind = kinds[i];
        efGroundMakeAppearActor();
        CHECK(gGCCommonLinks[7] == before);
    }

    /* -- efGroundMakeEffectID / efGroundUpdatePhysics /
     * efGroundSetupRandomWeights / EFGroundActorProcUpdate, and (Part B,
     * Phase 1) dcGroundMakeEffect/dcGroundSetupEffectDObjs -- all
     * through the public gate, since Phase 1's rewrite made the last two
     * file-local (their old fixed, decomp signatures -- a raw DObjDesc
     * array to walk -- are ef/efground.h's, unused now; see efground.c's
     * own file header). A table of this test's own, its pack the shared
     * mock_model fixture every dc_model_add_dobjs test already uses --
     * dc_model_add_dobjs only reads the joint skeleton, so the mock
     * needs no vertex of its own either. -- */
    saved_attr = mock_model.attr;
    mock_model.attr = NULL;      /* build every joint, as a stage pack does */

    memset(&desc[0], 0, sizeof(desc[0]));
    desc[0].alt_high = 777.0F;
    desc[0].alt_low = 777.0F;
    desc[0].pos_z = 4242.0F;
    desc[0].scale = 1.5F;
    desc[0].effect_status = -1;
    desc[0].effect_desc.dl_link = 4;
    desc[0].effect_desc.proc_update = efGroundCommonProcUpdate;
    desc[0].effect_desc.proc_display = gcDrawDObjTreeForGObj;

    asset[0].pack = &mock_model;
    asset[0].anim_joint = NULL;
    asset[0].matanim_joint = NULL;
    /* pack joint 1 is a billboard joint (a DObjDesc id with 0xF000) */
    asset[0].billboard = 0x2;

    param[0].effect_id = 0;
    param[0].make_queue = 0;
    param[0].lr = +1;
    param[0].effect_weight = 1;

    saved_data = dEFGroundDatas[nGRKindCastle];
    dEFGroundDatas[nGRKindCastle].params_num = 1;
    dEFGroundDatas[nGRKindCastle].effect_params = param;
    dEFGroundDatas[nGRKindCastle].o_data = 0;
    dEFGroundDatas[nGRKindCastle].effect_descs = desc;
    efGroundSetActorAssets(nGRKindCastle, asset, 1);

    gSCManagerBattleState->gkind = nGRKindCastle;
    {
        GObj *before7 = gGCCommonLinks[7];

        CHECK(before7 == NULL);
        efGroundMakeAppearActor();
        CHECK(gGCCommonLinks[7] != before7);
        actor = gGCCommonLinks[7];
        CHECK(actor->id == nGCCommonKindEffect);
    }

    CHECK(ground_find_by_z(nGCCommonLinkIDEffect, 4242.0F) == NULL);

    /* efGroundMakeEffectID reads sEFGroundActor.lr, and only
     * EFGroundActorProcUpdate's own first tic sets that (from
     * param[0].lr) -- sEFGroundActor is private to efground.c, so
     * there is no way to prime it from here directly. Drive the real
     * timer instead: make_wait starts randomized 6000-15999
     * (efground.c), so enough direct ticks reach zero and the whole
     * chain -- EFGroundActorProcUpdate, efGroundSetupRandomWeights,
     * efGroundMakeEffectID, dcGroundMakeEffect, dcGroundSetupEffectDObjs,
     * efGroundUpdatePhysics -- runs for real. */
    for (i = 0; i < 20000; i++)
    {
        EFGroundActorProcUpdate(actor);
        if (ground_find_by_z(nGCCommonLinkIDEffect, 4242.0F) != NULL)
        {
            break;
        }
    }
    CHECK(i < 20000);

    ground = ground_find_by_z(nGCCommonLinkIDEffect, 4242.0F);
    CHECK(ground != NULL);

    root = NULL;
    mid = NULL;
    leaf = NULL;

    if (ground != NULL)
    {
        DObj *gd = DObjGetStruct(ground);

        CHECK(gd->translate.vec.f.y == 777.0F);
        CHECK(gd->scale.vec.f.x == 1.5F);
        CHECK(gd->scale.vec.f.y == 1.5F);
        CHECK(gd->scale.vec.f.z == 1.5F);
        /* lr == +1: rotate.y = 180 deg, x pinned to the left bound */
        CHECK(fabsf(gd->rotate.vec.f.y - F_CST_DTOR32(180.0F)) < 0.0001F);
        CHECK(gd->translate.vec.f.x == gMPCollisionGroundData->map_bound_left + 500.0F);

        /* dcGroundSetupEffectDObjs's tree, off mock_model's own bind
         * pose (mock_joints[0]/[1]) -- proves the pack-based rebuild
         * (Phase 1) hangs off the wrapper root exactly as the decomp's
         * own raw DObjDesc walk did. */
        root = gd;
        mid = root->child;
        CHECK(mid != NULL);
        if (mid != NULL)
        {
            CHECK(mid->translate.vec.f.x == 0.0F);
            CHECK(mid->translate.vec.f.y == 150.0F);
            CHECK(mid->translate.vec.f.z == 0.0F);
            leaf = mid->child;
            CHECK(leaf != NULL);
        }
        /* ef/efground.c:1328-1343: the billboard joint appends 0x48
         * (lr_bool clear), and lr == +1 turns it and every joint after it
         * 180 degrees -- joint 0, before it, keeps its bind yaw. */
        if ((mid != NULL) && (leaf != NULL) && (leaf->child != NULL))
        {
            CHECK(mid->xobjs[mid->xobjs_num - 1]->kind != 0x48);
            CHECK(mid->rotate.vec.f.y == 0.0F);
            CHECK(leaf->xobjs[leaf->xobjs_num - 1]->kind == 0x48);
            CHECK(fabsf(leaf->rotate.vec.f.y - F_CST_DTOR32(180.0F)) < 0.0001F);
            CHECK(leaf->child->xobjs[leaf->child->xobjs_num - 1]->kind != 0x48);
            CHECK(fabsf(leaf->child->rotate.vec.f.y - F_CST_DTOR32(180.0F)) < 0.0001F);
        }

        ep = efGetStruct(ground);
        CHECK(ep->effect_vars.ground_effect.lr == +1);
        /* lr param (+1, not +-3) clears lr_bool -- dcGroundMakeEffect's
         * ((lr != -3) && (lr != 3)) ? FALSE : TRUE line, unchanged from
         * the decomp's own efGroundMakeEffect. */
        CHECK(ep->effect_vars.ground_effect.lr_bool == FALSE);
    }

    /* -- efGroundCheckEffectInBounds / efGroundCommonProcUpdate /
     * efGroundUpdateStepPositions / efGroundSetStepPositions /
     * efGroundUpdateEffectYaw (efground.c:1145-1260, decomp-verbatim,
     * untouched by Phase 1), driven by hand on the actor spawned above
     * -- root/mid/leaf's own bind-pose values do not matter here, only
     * that they are real DObjs to overwrite before each check. -- */
    if (root != NULL && mid != NULL && leaf != NULL)
    {
        ep = efGetStruct(ground);
        ep->effect_vars.ground_effect.anim_joint = NULL;
        ep->effect_vars.ground_effect.matanim_joint = NULL;
        ep->effect_vars.ground_effect.effect_status = -1;
        ep->effect_vars.ground_effect.lr = -1;
        root->scale.vec.f.x = 2.0F;
        root->scale.vec.f.y = 3.0F;
        root->scale.vec.f.z = 1.0F;
        root->translate.vec.f.x = gMPCollisionGroundData->map_bound_left + 600.0F;
        root->translate.vec.f.y = 0.0F;
        mid->translate.vec.f.x = 10.0F;
        mid->translate.vec.f.y = 20.0F;
        mid->anim_wait = AOBJ_ANIM_NULL;

        CHECK(efGroundCheckEffectInBounds(ground) == TRUE);
        CHECK(root->translate.vec.f.x == gMPCollisionGroundData->map_bound_left + 600.0F);

        efGroundCommonProcUpdate(ground);
        /* lr == -1: root.x += mid.x * root.scale.x; root.y += mid.y * root.scale.y */
        CHECK(root->translate.vec.f.x == (gMPCollisionGroundData->map_bound_left + 600.0F + 20.0F));
        CHECK(root->translate.vec.f.y == 60.0F);
        CHECK(mid->translate.vec.f.x == 0.0F);
        CHECK(mid->translate.vec.f.y == 0.0F);

        /* effect_status == 0 is the one value that skips the branch --
         * everything else (including -1) runs it. */
        mid->translate.vec.f.x = 5.0F;
        ep->effect_vars.ground_effect.effect_status = 0;
        efGroundCommonProcUpdate(ground);
        CHECK(mid->translate.vec.f.x == 5.0F);
        ep->effect_vars.ground_effect.effect_status = -1;

        /* efGroundSetStepPositions: lr == -1 targets pos.x = -1000, and
         * scale_step is scale.x*10 / (1000 + root.x). */
        root->translate.vec.f.x = -200.0F;
        root->scale.vec.f.x = 4.0F;
        efGroundSetStepPositions(ground);
        CHECK(ep->effect_vars.ground_effect.pos.x == -1000.0F);
        CHECK(fabsf(ep->effect_vars.ground_effect.scale_step - (4.0F * 10.0F / 800.0F)) < 0.0001F);

        /* efGroundUpdateStepPositions: past pos.x (lr == -1, pos.x >=
         * root.x) zeroes scale_step before it is added; short of it, the
         * step still applies once. */
        ep->effect_vars.ground_effect.pos.x = -1000.0F;
        ep->effect_vars.ground_effect.scale_step = 0.5F;
        root->translate.vec.f.x = -999.0F; /* pos.x(-1000) < root.x(-999): still short */
        root->scale.vec.f.x = 1.0F;
        root->scale.vec.f.y = 1.0F;
        root->scale.vec.f.z = 1.0F;
        mid->anim_wait = AOBJ_ANIM_NULL;
        efGroundUpdateStepPositions(ground);
        CHECK(root->scale.vec.f.x == 1.5F);
        CHECK(root->scale.vec.f.y == 1.5F);
        CHECK(root->scale.vec.f.z == 1.5F);

        ep->effect_vars.ground_effect.pos.x = -1000.0F;
        ep->effect_vars.ground_effect.scale_step = 0.5F;
        root->translate.vec.f.x = -1000.0F; /* pos.x >= root.x now: over */
        mid->anim_wait = AOBJ_ANIM_NULL;
        efGroundUpdateStepPositions(ground);
        CHECK(ep->effect_vars.ground_effect.scale_step == 0.0F);
        CHECK(root->scale.vec.f.x == 1.5F); /* += 0, unchanged */

        /* efGroundUpdateEffectYaw: lr == +1 flips leaf's rotate.z after
         * the move CommonProcUpdate already does. */
        ep->effect_vars.ground_effect.lr = +1;
        leaf->rotate.vec.f.z = 9.0F;
        root->translate.vec.f.x = gMPCollisionGroundData->map_bound_right - 600.0F;
        mid->anim_wait = AOBJ_ANIM_NULL;
        efGroundUpdateEffectYaw(ground);
        CHECK(leaf->rotate.vec.f.z == -9.0F);

        /* efGroundCheckEffectInBounds's other half: crossing the bound
         * ejects the GObj. Last use of `root`/`mid`/`leaf`. */
        ep->effect_vars.ground_effect.lr = +1;
        root->translate.vec.f.x = gMPCollisionGroundData->map_bound_right - 400.0F;
        CHECK(efGroundCheckEffectInBounds(ground) == FALSE);
    }

    /* clean up every GObj this test made -- the two-pass loop runs this
     * test twice, and a leftover on link 7 would make the next pass's
     * `before7 == NULL` check above fail for a reason that has nothing
     * to do with efGroundMakeAppearActor. */
    for (;;)
    {
        GObj *g = ground_find_by_z(nGCCommonLinkIDEffect, 4242.0F);

        if (g == NULL)
        {
            break;
        }
        gcEjectGObj(g);
    }
    gcEjectGObj(actor);

    dEFGroundDatas[nGRKindCastle] = saved_data;
    efGroundSetActorAssets(nGRKindCastle, NULL, 0);
    mock_model.attr = saved_attr;
    gSCManagerBattleState->gkind = saved_gkind;
    gSCManagerSceneData.scene_curr = saved_scene;

    printf("ground: no stage's table exists yet but Castle's, so "
           "efGroundMakeAppearActor is a no-op everywhere else the port "
           "can play; given a table of its own, the timer path picks it, "
           "scales it %.1fx and sets it down at y %d pinned to the map "
           "bound, its tree built by dcGroundSetupEffectDObjs off a real "
           "baked pack (Part B, Phase 1) rather than a raw DObjDesc walk\n",
           (double)desc[0].scale, (int)desc[0].alt_high);
}

static void test_every_fighter_stands(void)
{
    s32 k;

    sprite_bank_release_all();
    ftManagerReleaseFilesAll();
    load_stage();
    /* the fighters of the test before this one lived in the heap
     * load_stage just remade; nothing to destroy */
    mock_gobj = NULL;
    mock_gobj2 = NULL;
    fpp2 = NULL;
    /* Mario's real pack too, not the mock init installs: the manager
     * loads a kind whose slot is empty, and gives back what it loaded */
    gFTManagerModels[nFTKindMario] = NULL;
    for (k = nFTKindPlayableStart; k <= nFTKindPlayableEnd; k++)
    {
        const Fighter *pack;
        float y0;
        int n;

        ftManagerSetupFilesAllKind(k);
        pack = gFTManagerModels[k];
        CHECK(pack != NULL);
        if (pack == NULL)
        {
            continue;
        }
        spawn_kind(k, 0.0f, 0.0f);
        y0 = DObjGetStruct(mock_gobj)->translate.vec.f.y;
        CHECK(fp.fkind == k);
        /* Every hurtbox
         * the kind's attributes declare is placed on a joint by
         * ftManagerMakeFighter, and gm/gmcollision.c -- compiled
         * unmodified -- reads that joint's world position for every
         * hitbox that comes near it. A hurtbox left live with no joint
         * is a null dereference inside a decomp function that has no
         * guard, because on the N64 every joint id an attribute names
         * always resolves. */
        {
            int c, live = 0, want = 0;

            /* what is live is exactly the descs whose joint the
             * skeleton has, in the order they are declared, with no
             * gap: the list is read to its first nGMHitStatusNone and
             * an orphan in the middle would take the ones after it */
            for (c = 0; c < (int)ARRAY_COUNT(fp.damage_colls); c++)
            {
                s32 id = fp.attr->damage_coll_descs[c].joint_id;

                if (id == -1)
                {
                    continue;
                }
                /* every id resolves under the game's
                 * base joints[4 + k] (tools/check/joint_check.py) */
                CHECK(id < (s32)ARRAY_COUNT(fp.joints) && fp.joints[id] != NULL);
                if (id >= (s32)ARRAY_COUNT(fp.joints) || fp.joints[id] == NULL)
                {
                    continue;
                }
                CHECK(fp.damage_colls[want].hitstatus != nGMHitStatusNone);
                CHECK(fp.damage_colls[want].joint == fp.joints[id]);
                CHECK(fp.damage_colls[want].joint_id == id);
                CHECK(fp.damage_colls[want].placement ==
                      fp.attr->damage_coll_descs[c].placement);
                want++;
            }
            for (c = 0; c < (int)ARRAY_COUNT(fp.damage_colls); c++)
            {
                if (fp.damage_colls[c].hitstatus == nGMHitStatusNone)
                {
                    break;
                }
                CHECK(fp.damage_colls[c].joint != NULL);
                live++;
            }
            CHECK(live == want);
            /* Kirby's and Jigglypuff's eighth hurtbox is the hat, on
             * joints 29 and 28 -- entries 25 and 24, the last their
             * masks instantiate -- which the port's old base put two
             * past the skeleton */
            if (k == nFTKindKirby || k == nFTKindPurin)
            {
                CHECK(live == 8);
            }
        }
        CHECK_STATUS(nFTCommonStatusWait);
        CHECK(fp.ga == nMPKineticsGround);
        idle_frames(300);
        if (fp.status_id != nFTCommonStatusWait)
        {
            printf("  %s: after 300 tics %s at (%.0f, %.0f) %s\n", kKindNames[k],
                   ftMainStatusName(fp.status_id),
                   DObjGetStruct(mock_gobj)->translate.vec.f.x,
                   DObjGetStruct(mock_gobj)->translate.vec.f.y,
                   fp.ga == nMPKineticsGround ? "ground" : "air");
        }
        CHECK_STATUS(nFTCommonStatusWait);
        CHECK(fp.ga == nMPKineticsGround);
        CHECK_EQF(DObjGetStruct(mock_gobj)->translate.vec.f.y, y0);
        CHECK_EQF(DObjGetStruct(mock_gobj)->translate.vec.f.x, 0.0f);

        /* a short hop: tap the jump button, through the squat */
        frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
        CHECK_STATUS(nFTCommonStatusKneeBend);
        n = run_until(0, 0, 12, is_airborne);
        CHECK(n < 12);
        CHECK_STATUS(nFTCommonStatusJumpF);
        n = run_until(0, 0, 120, is_grounded);
        CHECK(n < 120);
        n = run_until(0, 0, 40, is_wait);
        CHECK(n < 40);

        /* and a jab */
        frame(0, 0, N64_A, N64_A, 0);
        CHECK_STATUS(nFTCommonStatusAttack11);
        n = run_until(0, 0, 60, is_wait);
        CHECK(n < 60);
    }
    if (mock_gobj != NULL)
    {
        ftManagerDestroyFighter(mock_gobj);
        mock_gobj = NULL;
    }
    ftManagerReleaseFilesAll();
    sprite_bank_release_all();
    gFTManagerModels[nFTKindMario] = &mock_model;
    mock_model.attr = mock_attr;
}
