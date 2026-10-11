/* hosttest/openings.c -- part of hosttest_ft.c: the startup logo and the opening movie's scenes.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* ---- N64 LOGO / STARTUP -----------------------------------
 *
 * mn/mncommon/mnstartup.c is the US build's default boot scene
 * (src/dc/scmanagerdata.c:385-393) and the only route into the openings
 * chain outside mv/mvopening/ itself (src/dc/mnstartup.h's header note).
 * Two things make it worth a real test rather than the short
 * "starts, draws, does not wander off" shape test_nocontroller_menu
 * takes:
 *
 *  1. It is the first scene in this port whose whole behaviour lives in
 *     a GObjThread. mnStartupLogoThreadUpdate is a sleep-driven state
 *     machine -- 16 tics of parabola, 24 sitting still, a fade, 13 more,
 *     then the hand-off flag -- and dMNStartupTaskmanSetup declares ZERO
 *     GObjThreads while FuncStart adds one. That is fine on both targets
 *     (sys/objman.c:117-120 and :162-185 malloc a thread and its stack
 *     out of the scene heap when the free list is empty) but it is
 *     exactly the kind of "fine in theory" that deserves a run: this
 *     test drives the thread through every one of its four phases.
 *  2. It has two exits and they race. The skip branch (A/B/START once
 *     sMNStartupSkipAllowWait has counted 8 tics down to 0) and the
 *     hand-off branch (sMNStartupIsProceedOpening, set by the thread)
 *     are the if and else-if of one function, so a wrong tic count in
 *     either would show as the wrong scene, silently.
 *
 * The tic numbers below are the decomp's arithmetic counted out, and
 * each is asserted at the tic before as well as at the tic itself -- an
 * off-by-one in either direction fails. The scene is driven the way
 * test_nocontroller_menu drives its own: syTaskmanSetupPools then
 * func_start, never StartScene (that would block in the frame loop), and
 * syTaskmanResetBreakLoop afterwards because both exits really do call
 * syTaskmanSetLoadScene (test_autodemo's own note says why that matters
 * for whatever test runs next). */
static SObj *startup_find(s32 link, u32 offset)
{
    return scene_sprite_find(gMNStartupFiles, link, 0, offset);
}

static void startup_begin(void)
{
    sprite_bank_release_all();

    /* An idle pad. scSubsysControllerGetPlayerTapButtons walks every
     * device, so a tap left behind by an earlier test would take the
     * skip branch out of this scene on its own tic 8 -- the same trap
     * test_openingroom's own memset guards against. */
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindStartup;

    syTaskmanSetupPools(&dMNStartupTaskmanSetup);
    mnStartupFuncStart();
}

static void test_startup_scene(void)
{
    SObj *sobj;
    f32 y_prev;
    int i;

    /* ---- the hand-off exit: let the thread run to the end ---- */
    startup_begin();

    CHECK(gMNStartupFiles[0] != NULL);
    CHECK(sMNStartupSkipAllowWait == 8);
    CHECK(sMNStartupIsProceedOpening == FALSE);

    /* 0x073C0 is llN64LogoSprite; the link is gcMakeGObjSPAfter's own
     * third argument in mnStartupFuncStart, not gcAddGObjDisplay's DL
     * link (the m18-front-end recipe's trap on the two link kinds). */
    sobj = startup_find(nGCCommonLinkIDWallpaper, 0x073C0);
    CHECK(sobj != NULL);
    CHECK(sobj->pos.x == 96.0F);
    CHECK(sobj->pos.y == 220.0F);

    /* Phase 1, tics 1-16: the parabola. y starts back at 220 on tic 1
     * (step 16 gives 65 + 155) and falls every tic without ever
     * overshooting 65 -- the decomp writes the drop as a subtraction of
     * a negative, so a sign slip anywhere in it would send the logo up
     * and off the screen instead. */
    y_prev = sobj->pos.y;
    for (i = 1; i <= 16; i++)
    {
        gcRunAll();
        gcDrawAll();

        CHECK(sobj->pos.y <= y_prev);
        CHECK(sobj->pos.y >= 65.0F);
        y_prev = sobj->pos.y;
    }
    /* step 1 on tic 16: 65 + (38.75/64), just short of the resting
     * place, which the tic after sets exactly. */
    CHECK(sobj->pos.y > 65.0F);

    /* Phase 2, tic 17: the loop ends and y is assigned outright. */
    gcRunAll();
    gcDrawAll();
    CHECK(sobj->pos.y == 65.0F);

    /* Phases 2-4, tics 18-53: 24 tics sitting still, the fade, then 13
     * more. Nothing moves and nothing hands off. sMNStartupSkipAllowWait
     * has long since reached 0 (tic 8), so the ONLY reason scene_curr is
     * still nSCKindStartup here is that the pad is idle -- which is what
     * makes the skip case below a real second case and not a rerun. */
    for (i = 18; i <= 53; i++)
    {
        gcRunAll();
        gcDrawAll();

        CHECK(sobj->pos.y == 65.0F);
        CHECK(sMNStartupIsProceedOpening == FALSE);
        CHECK(gSCManagerSceneData.scene_curr == nSCKindStartup);
    }
    CHECK(sMNStartupSkipAllowWait == 0);

    /* Tic 54: the thread sets the flag. The actor GObj was made before
     * the wallpaper GObj and so runs first within the tic, which is why
     * the scene does not change until the tic after. */
    gcRunAll();
    gcDrawAll();
    CHECK(sMNStartupIsProceedOpening != FALSE);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindStartup);

    /* Tic 55: mnStartupActorFuncRun notices, and this is the line the
     * whole file exists for. */
    gcRunAll();
    gcDrawAll();
    CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningRoom);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindStartup);
    CHECK(syTaskmanCheckBreakLoop() != FALSE);

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: START, once the 8-tic guard has expired ---- */
    startup_begin();

    /* Held down from the start: the guard is what stops this, not an
     * absent press. Seven tics in, the scene must still be its own. */
    for (i = 1; i <= 7; i++)
    {
        gSYControllerDevices[0].button_tap = START_BUTTON;

        gcRunAll();
        gcDrawAll();

        CHECK(gSCManagerSceneData.scene_curr == nSCKindStartup);
    }
    CHECK(sMNStartupSkipAllowWait == 1);

    gSYControllerDevices[0].button_tap = START_BUTTON;

    gcRunAll();
    gcDrawAll();

    CHECK(sMNStartupSkipAllowWait == 0);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindStartup);
    /* and it left WITHOUT waiting for the thread */
    CHECK(sMNStartupIsProceedOpening == FALSE);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    /* the overlay reload is what makes the next scene's layout its own */
    mnStartupOverlayLoad();
    CHECK(gMNStartupFiles[0] == NULL);
    CHECK(sMNStartupSkipAllowWait == 0);
    CHECK(sMNStartupIsProceedOpening == FALSE);
}

static SObj *openingportraits_find(s32 link, int file, u32 offset)
{
    return scene_sprite_find(sMVOpeningPortraitsFiles, link, file, offset);
}

static Gfx *sOpeningPortraitsDL0Base;

static void openingportraits_begin(void)
{
    sprite_bank_release_all();

    /* An idle pad -- the same trap test_openingroom/test_startup_scene's
     * own memsets guard against: scSubsysControllerGetPlayerTapButtons
     * walks every device, and a tap left behind by an earlier test would
     * take the skip branch on this scene's own tic 10. */
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningPortraits;

    syTaskmanSetupPools(&dMVOpeningPortraitsTaskmanSetup);
    mvOpeningPortraitsFuncStart();

    /* This scene's cover writes raw gDPFillRectangle commands straight
     * into gSYTaskmanDLHeads[0] (mvOpeningPortraitsCoverProcDisplay), the
     * same wpmanager.c scratch every weapon/effect display proc shares.
     * On target that head is put back to its row's start before each of
     * the frame's three PVR passes by wpManagerResetDLHeads, installed as
     * syTaskmanSetDrawPassHook's own hook -- but only inside
     * syTaskmanCommonTaskDraw's pass loop (src/dc/taskman.c), which this
     * test's own direct gcRunAll()/gcDrawAll() driving (the same style
     * test_startup_scene already uses) never goes through. Left alone,
     * 150-plus tics of an unreset head walks off its 512-byte row and
     * into whatever host test happens to run next -- caught the first
     * time this test was written as a "DLBuffer over flow" print out of
     * an unrelated later test. wpManagerInitDLHeads() (src/dc/wpmanager.c,
     * fifth pass) both installs the real hook, for every
     * scene, so it is idempotent to call again here, and resets the head
     * immediately; openingportraits_draw below repeats that reset by hand
     * every tic, standing in for the pass loop this test does not run. */
    wpManagerInitDLHeads();
    sOpeningPortraitsDL0Base = gSYTaskmanDLHeads[0];
}

static void openingportraits_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningPortraitsDL0Base;
}

/* src/dc/mvopeningportraits.c: the openings' second scene,
 * tier 1 of the eighteen still unported -- pure sprite content, no
 * fighters, no relocData scene-graph gap.
 *
 * WHAT THIS HAS TO CATCH.
 *
 *  1. The set swap at tic 75. mvOpeningPortraitsFuncRun ejects set 1's
 *     GObj (link 17) and mvOpeningPortraitsMakeSet2 immediately remakes
 *     a GObj at the SAME link with set 2's four sprites -- so "set 1 is
 *     gone" and "set 2 is up" have to be checked as two separate,
 *     equally necessary facts, not inferred from one another. Getting
 *     mvOpeningPortraitsMakeSet1's own gcEjectGObjIfMade wrong (a plain
 *     gcEjectGObj on a GObj* that is provably never NULL here, same
 *     standing preference room/mvending/staffroll's own ejects already
 *     take) would eject the CURRENT scene's own driver instead if
 *     sMVOpeningPortraitsGObj had somehow gone stale -- this test is
 *     what would show that as "scene stops advancing after tic 75", the
 *     same silent failure mode [[m19-presentation]] already documents
 *     for a self-ejected driver.
 *  2. The cover sprite's own row table (mvOpeningPortraitsCoverProcUpdate)
 *     is NOT tic-ascending in the decomp's own source order (15, 45, 30,
 *     60, then 105, 135, 90, 120) -- transcribed by hand from a source
 *     that reads visually out of order is exactly the kind of place a
 *     copy-paste slip hides, so every one of the eight case tics is
 *     checked against its own y value, not against "the next row in
 *     sequence".
 *  3. The skip exit has NO countdown, unlike src/dc/mnstartup.c's own
 *     8-tic sMNStartupSkipAllowWait -- the guard here is the same
 *     `total_time_tics >= 10` the row/eject logic already sits behind,
 *     so a tap held from tic 1 does nothing until tic 10 and then acts
 *     immediately, one tic, not a further wait.
 *
 * The scene is driven the way test_startup_scene's own note explains:
 * syTaskmanSetupPools then FuncStart, never StartScene, and
 * syTaskmanResetBreakLoop after each exit since both really do call
 * syTaskmanSetLoadScene. */
static void test_openingportraits_scene(void)
{
    SObj *samus, *mario, *fox, *pikachu, *cover;
    SObj *link_, *kirby, *donkey, *yoshi;
    int i;

    /* ---- the hand-off exit: run the full 150 tics ---- */
    openingportraits_begin();

    CHECK(sMVOpeningPortraitsFiles[0] != NULL);
    CHECK(sMVOpeningPortraitsFiles[1] != NULL);

    /* set 1, link 17, file 0 -- the four portraits mvOpeningPortraitsMakeSet1
     * puts up, at their four row anchors. */
    samus   = openingportraits_find(17, 0, 0x9960);
    mario   = openingportraits_find(17, 0, 0x13310);
    fox     = openingportraits_find(17, 0, 0x1ccc0);
    pikachu = openingportraits_find(17, 0, 0x26670);
    CHECK(samus != NULL && mario != NULL && fox != NULL && pikachu != NULL);
    CHECK_EQF(samus->pos.x, 10.0F);   CHECK_EQF(samus->pos.y, 10.0F);
    CHECK_EQF(mario->pos.x, 10.0F);   CHECK_EQF(mario->pos.y, 65.0F);
    CHECK_EQF(fox->pos.x, 10.0F);     CHECK_EQF(fox->pos.y, 120.0F);
    CHECK_EQF(pikachu->pos.x, 10.0F); CHECK_EQF(pikachu->pos.y, 175.0F);
    CHECK((samus->sprite.attr & SP_FASTCOPY) == 0);

    /* set 2 is not up yet */
    CHECK(openingportraits_find(17, 1, 0x9960) == NULL);

    /* the cover, link 18, file 0, sliding in from the right */
    cover = openingportraits_find(18, 0, 0x2b2d0);
    CHECK(cover != NULL);
    CHECK_EQF(cover->pos.x, 656.0F);
    CHECK_EQF(cover->pos.y, 10.0F);
    CHECK((cover->sprite.attr & SP_TRANSPARENT) != 0);
    CHECK((cover->sprite.attr & SP_FASTCOPY) == 0);
    CHECK(cover->sprite.red == 0x00 && cover->sprite.green == 0x00 &&
          cover->sprite.blue == 0x00);

    for (i = 1; i <= 150; i++)
    {
        gcRunAll();
        openingportraits_draw();

        /* the row table, exactly as mvOpeningPortraitsCoverProcUpdate's
         * own switch orders it -- see this test's own note 2 above */
        switch (i)
        {
        case 15:  CHECK_EQF(cover->pos.x, -656.0F); CHECK_EQF(cover->pos.y, 10.0F);  break;
        case 30:  CHECK_EQF(cover->pos.x, -656.0F); CHECK_EQF(cover->pos.y, 120.0F); break;
        case 45:  CHECK_EQF(cover->pos.x, -656.0F); CHECK_EQF(cover->pos.y, 65.0F);  break;
        case 60:  CHECK_EQF(cover->pos.x, -656.0F); CHECK_EQF(cover->pos.y, 175.0F); break;
        case 90:  CHECK_EQF(cover->pos.x, 656.0F);  CHECK_EQF(cover->pos.y, 120.0F); break;
        case 105: CHECK_EQF(cover->pos.x, 656.0F);  CHECK_EQF(cover->pos.y, 10.0F);  break;
        case 120: CHECK_EQF(cover->pos.x, 656.0F);  CHECK_EQF(cover->pos.y, 175.0F); break;
        case 135: CHECK_EQF(cover->pos.x, 656.0F);  CHECK_EQF(cover->pos.y, 65.0F);  break;
        default: break;
        }

        if (i < 75)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningPortraits);
        }
        else if (i == 75)
        {
            /* set 1's own GObj is gone, and set 2 is up at the SAME four
             * anchors -- both checked, per this test's own note 1 */
            CHECK(openingportraits_find(17, 0, 0x9960) == NULL);

            link_ = openingportraits_find(17, 1, 0x9960);
            kirby = openingportraits_find(17, 1, 0x13310);
            donkey = openingportraits_find(17, 1, 0x1ccc0);
            yoshi = openingportraits_find(17, 1, 0x26670);
            CHECK(link_ != NULL && kirby != NULL && donkey != NULL && yoshi != NULL);
            CHECK_EQF(link_->pos.x, 10.0F);  CHECK_EQF(link_->pos.y, 10.0F);
            CHECK_EQF(kirby->pos.x, 10.0F);  CHECK_EQF(kirby->pos.y, 65.0F);
            CHECK_EQF(donkey->pos.x, 10.0F); CHECK_EQF(donkey->pos.y, 120.0F);
            CHECK_EQF(yoshi->pos.x, 10.0F);  CHECK_EQF(yoshi->pos.y, 175.0F);
        }
        else if (i < 150)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningPortraits);
        }
        else
        {
            /* tic 150: the hand-off. Mario is not ported, so on a real
             * probe scManagerRunScene's default arm bounces straight
             * back here -- the same reading room's own arm documents one
             * scene up the chain (src/dc/scmanager.c). */
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningMario);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningPortraits);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: held from tic 1, first takes effect at tic 10,
     * one tic, not a countdown (this test's own note 3) ---- */
    openingportraits_begin();

    for (i = 1; i <= 9; i++)
    {
        gSYControllerDevices[0].button_tap = START_BUTTON;

        gcRunAll();
        openingportraits_draw();

        CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningPortraits);
    }

    gSYControllerDevices[0].button_tap = START_BUTTON;

    gcRunAll();
    openingportraits_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningPortraits);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    /* the overlay reload is what makes the next scene's layout its own */
    mvOpeningPortraitsOverlayLoad();
    CHECK(sMVOpeningPortraitsFiles[0] == NULL);
    CHECK(sMVOpeningPortraitsFiles[1] == NULL);
}

static SObj *openingmario_find(s32 link, u32 offset)
{
    Sprite *sp = sprite_bank_get(sMVOpeningMarioNamesFileHead, offset);
    GObj *gobj;

    for (gobj = gGCCommonLinks[link]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        for (; sobj != NULL; sobj = sobj->next)
        {
            if (sp != NULL && sobj->sprite.bitmap == sp->bitmap)
            {
                return sobj;
            }
        }
    }
    return NULL;
}

static Gfx *sOpeningMarioDL0Base;

static void openingmario_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningMario;

    syTaskmanSetupPools(&dMVOpeningMarioTaskmanSetup);
    mvOpeningMarioFuncStart();

    /* The same head reset mvopeningportraits.c's own host test takes
     * (that test's note above has the mechanism). The colour panel no
     * longer writes a head -- it is a fill quad, checked below -- but
     * the stage's own procs still do, so the reset stays. */
    wpManagerInitDLHeads();
    sOpeningMarioDL0Base = gSYTaskmanDLHeads[0];
}

static void openingmario_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningMarioDL0Base;
}

/* The colour panel under the posed Mario, from tic 15: one fill quad
 * (rect.copy == -1) over 10,10-110,230 in A0,AA,FF, in the front sprite
 * band, drawn in the translucent pass only. It was a raw
 * gDPFillRectangle into the write-only scratch heads until the layering
 * fix, which is what this catches. */
static void openingmario_check_panel(void)
{
    int i, found = 0;

    gcSetDrawList(PVR_LIST_OP_POLY);
    gLBCommonSpriteQuadLogCount = 0;
    openingmario_draw();
    for (i = 0; i < gLBCommonSpriteQuadLogCount && i < LB_SPRITE_QUAD_LOG_MAX; i++)
    {
        CHECK(gLBCommonSpriteQuadLog[i].rect.copy != -1);
    }

    gcSetDrawList(PVR_LIST_TR_POLY);
    gLBCommonSpriteQuadLogCount = 0;
    openingmario_draw();
    for (i = 0; i < gLBCommonSpriteQuadLogCount && i < LB_SPRITE_QUAD_LOG_MAX; i++)
    {
        const LBCommonSpriteQuad *q = &gLBCommonSpriteQuadLog[i];

        if (q->rect.copy != -1)
        {
            continue;
        }
        found++;
        CHECK(q->rect.rxh == 10 * 4 && q->rect.ryh == 10 * 4);
        CHECK(q->rect.rxl == 110 * 4 && q->rect.ryl == 230 * 4);
        CHECK(q->argb == 0xFFA0AAFFu);
        CHECK(q->z >= LB_SPRITE_Z_BASE);
    }
    CHECK(found == 1);
    gcSetDrawList(PVR_LIST_OP_POLY);
}

/* src/dc/mvopeningmario.c: the openings' third scene, and
 * the first of the eight per-fighter scenes -- a real VS stage (Castle)
 * and a real scripted fighter walking toward the camera under a canned
 * FTKeyEvent script, alongside a separately posed Mario in a colored box
 * on the left third of the screen.
 *
 * WHAT THIS HAS TO CATCH.
 *
 *  1. The name card: five letters (M, A, R, I, O) at their own x
 *     positions, spelling the word left to right -- the source's own
 *     five-entry pos_x/offsets arrays make an off-by-one in either one
 *     easy to get subtly wrong (a shuffled letter, not a crash).
 *  2. The tic-15 hand-off: the name GObj is ejected
 *     (gcEjectGObjIfMade) and the motion camera comes up as
 *     gGMCameraGObj with GMCAMERA_BATTLE_DLLINK_MASK -- this port's own
 *     substitution for the decomp's gmCameraMakeMovieCamera
 *     (mvopeningmario.h's Substitution 3), not something the decomp's
 *     own source checks since the mask is port-only. Getting the
 *     ordering wrong (grWallpaperMakeDecideKind called before the
 *     camera exists, the decomp's own literal order) would either crash
 *     on a NULL gGMCameraGObj or silently hang the wallpaper off
 *     whatever camera a PRIOR test left behind -- this is what would
 *     catch either.
 *  3. The tic-60 hand-off to nSCKindOpeningDonkey, and the skip exit,
 *     which -- unlike mvopeningportraits.c's own 10-tic guard -- has NO
 *     guard at all: mvOpeningMarioFuncRun's button check runs every tic
 *     unconditionally, so a tap on tic 1 exits immediately, not after a
 *     wait.
 *
 * The scene is driven the way test_startup_scene's own note explains:
 * syTaskmanSetupPools then FuncStart, never StartScene, and
 * syTaskmanResetBreakLoop after each exit since both really do call
 * syTaskmanSetLoadScene. */
static void test_openingmario_scene(void)
{
    SObj *m, *a, *r, *i_, *o;
    int i;

    /* ---- the hand-off exit: run the full 60 tics ---- */
    openingmario_begin();

    CHECK(sMVOpeningMarioNamesFileHead != NULL);
    CHECK(gSCManagerBattleState->gkind == nGRKindCastle);
    CHECK(gSCManagerBattleState->players[0].fkind == nFTKindMario);
    CHECK(gSCManagerBattleState->players[0].pkind == nFTPlayerKindKey);

    /* the name, link 17, spelling M-A-R-I-O left to right (note 1) */
    m  = openingmario_find(17, 0x03980);
    a  = openingmario_find(17, 0x005E0);
    r  = openingmario_find(17, 0x05418);
    i_ = openingmario_find(17, 0x026B8);
    o  = openingmario_find(17, 0x044B0);
    CHECK(m != NULL && a != NULL && r != NULL && i_ != NULL && o != NULL);
    CHECK_EQF(m->pos.x, 80.0F);   CHECK_EQF(m->pos.y, 100.0F);
    CHECK_EQF(a->pos.x, 120.0F);  CHECK_EQF(a->pos.y, 100.0F);
    CHECK_EQF(r->pos.x, 160.0F);  CHECK_EQF(r->pos.y, 100.0F);
    CHECK_EQF(i_->pos.x, 190.0F); CHECK_EQF(i_->pos.y, 100.0F);
    CHECK_EQF(o->pos.x, 205.0F);  CHECK_EQF(o->pos.y, 100.0F);
    CHECK((m->sprite.attr & SP_TRANSPARENT) != 0);
    CHECK((m->sprite.attr & SP_FASTCOPY) == 0);

    for (i = 1; i <= 60; i++)
    {
        gcRunAll();
        openingmario_draw();

        if (i < 15)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningMario);
            CHECK(openingmario_find(17, 0x03980) != NULL);
            if (i == 5)
            {
                /* the frame's border must leave the name card alone: the
                 * only 3D cameras yet are the posed column's (10-110),
                 * and the card runs to 245. The name camera's own
                 * viewport is what claims the rest. */
                const DCViewport *vp;

                gcResetViewportUnion();
                gcSetDrawList(PVR_LIST_OP_POLY);
                openingmario_draw();
                vp = gcGetViewportUnion();
                CHECK_EQF(vp->cx - vp->hw, 20.0F);
                CHECK_EQF(vp->cx + vp->hw, 620.0F);
                CHECK_EQF(vp->cy - vp->hh, 20.0F);
                CHECK_EQF(vp->cy + vp->hh, 460.0F);
            }
        }
        else if (i == 15)
        {
            /* the name card is gone, and the motion camera is up as the
             * real battle camera (note 2) */
            CHECK(openingmario_find(17, 0x03980) == NULL);
            CHECK(gGMCameraGObj != NULL);
            CHECK(gGMCameraGObj->camera_mask == GMCAMERA_BATTLE_DLLINK_MASK);
        }
        else if (i < 60)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningMario);
            if (i == 20)
            {
                openingmario_check_panel();
            }
        }
        else
        {
            /* tic 60: the hand-off. Donkey is not ported, so on a real
             * probe scManagerRunScene's default arm bounces straight
             * back here -- the same reading every scene above it in the
             * chain already documents. */
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningDonkey);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningMario);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: no countdown at all (note 3) -- a tap on tic 1
     * exits on tic 1 ---- */
    openingmario_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;

    gcRunAll();
    openingmario_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningMario);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    /* the overlay reload is what makes the next scene's layout its own */
    mvOpeningMarioOverlayLoad();
    CHECK(sMVOpeningMarioNamesFileHead == NULL);
}

static SObj *openingdonkey_find(s32 link, u32 offset)
{
    Sprite *sp = sprite_bank_get(sMVOpeningDonkeyNamesFileHead, offset);
    GObj *gobj;

    for (gobj = gGCCommonLinks[link]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        for (; sobj != NULL; sobj = sobj->next)
        {
            if (sp != NULL && sobj->sprite.bitmap == sp->bitmap)
            {
                return sobj;
            }
        }
    }
    return NULL;
}

static Gfx *sOpeningDonkeyDL0Base;

static void openingdonkey_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningDonkey;

    syTaskmanSetupPools(&dMVOpeningDonkeyTaskmanSetup);
    mvOpeningDonkeyFuncStart();

    /* mvOpeningDonkeyPosedWallpaperProcDisplay (installed from tic 15)
     * writes raw gDPFillRectangle straight into gSYTaskmanDLHeads[0], the
     * same DL-head-overflow trap test_openingmario_scene's own note
     * explains; the same fix applies here. */
    wpManagerInitDLHeads();
    sOpeningDonkeyDL0Base = gSYTaskmanDLHeads[0];
}

static void openingdonkey_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningDonkeyDL0Base;
}

/* src/dc/mvopeningdonkey.c: the openings' fourth scene, and
 * the second of the eight per-fighter scenes -- reuses
 * test_openingmario_scene's own three things to catch (the name card,
 * the tic-15 hand-off, the tic-60 hand-off and the skip exit), with
 * this scene's own content: a two-letter "DK" name card (not "MARIO"),
 * a real Jungle stage (not Castle), and a hand-off to
 * nSCKindOpeningLink (not nSCKindOpeningDonkey) -- checked directly
 * against the decomp, not assumed from the scene-kind enum's own
 * declaration order (mvopeningdonkey.h's header note). */
static void test_openingdonkey_scene(void)
{
    SObj *d, *k;
    int i;

    /* ---- the hand-off exit: run the full 60 tics ---- */
    openingdonkey_begin();

    CHECK(sMVOpeningDonkeyNamesFileHead != NULL);
    CHECK(gSCManagerBattleState->gkind == nGRKindJungle);
    CHECK(gSCManagerBattleState->players[0].fkind == nFTKindDonkey);
    CHECK(gSCManagerBattleState->players[0].pkind == nFTPlayerKindKey);

    /* the name, link 17, spelling D-K left to right */
    d = openingdonkey_find(17, 0x01268);
    k = openingdonkey_find(17, 0x02F98);
    CHECK(d != NULL && k != NULL);
    CHECK_EQF(d->pos.x, 120.0F); CHECK_EQF(d->pos.y, 100.0F);
    CHECK_EQF(k->pos.x, 160.0F); CHECK_EQF(k->pos.y, 100.0F);
    CHECK((d->sprite.attr & SP_TRANSPARENT) != 0);
    CHECK((d->sprite.attr & SP_FASTCOPY) == 0);

    for (i = 1; i <= 60; i++)
    {
        gcRunAll();
        openingdonkey_draw();

        if (i < 15)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningDonkey);
            CHECK(openingdonkey_find(17, 0x01268) != NULL);
        }
        else if (i == 15)
        {
            /* the name card is gone, and the motion camera is up as the
             * real battle camera */
            CHECK(openingdonkey_find(17, 0x01268) == NULL);
            CHECK(gGMCameraGObj != NULL);
            CHECK(gGMCameraGObj->camera_mask == GMCAMERA_BATTLE_DLLINK_MASK);
        }
        else if (i < 60)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningDonkey);
        }
        else
        {
            /* tic 60: the hand-off to Link, not to Donkey itself --
             * Link is ported (test_openinglink_scene
             * below) and picks up the chain from here. */
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningLink);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningDonkey);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: no countdown at all -- a tap on tic 1 exits
     * on tic 1 ---- */
    openingdonkey_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;

    gcRunAll();
    openingdonkey_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningDonkey);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    /* the overlay reload is what makes the next scene's layout its own */
    mvOpeningDonkeyOverlayLoad();
    CHECK(sMVOpeningDonkeyNamesFileHead == NULL);
}

static SObj *openinglink_find(s32 link, u32 offset)
{
    Sprite *sp = sprite_bank_get(sMVOpeningLinkNamesFileHead, offset);
    GObj *gobj;

    for (gobj = gGCCommonLinks[link]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        for (; sobj != NULL; sobj = sobj->next)
        {
            if (sp != NULL && sobj->sprite.bitmap == sp->bitmap)
            {
                return sobj;
            }
        }
    }
    return NULL;
}

static Gfx *sOpeningLinkDL0Base;

static void openinglink_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningLink;

    syTaskmanSetupPools(&dMVOpeningLinkTaskmanSetup);
    mvOpeningLinkFuncStart();

    /* mvOpeningLinkPosedWallpaperProcDisplay (installed from tic 15)
     * writes raw gDPFillRectangle straight into gSYTaskmanDLHeads[0], the
     * same DL-head-overflow trap test_openingmario_scene's own note
     * explains; the same fix applies here. */
    wpManagerInitDLHeads();
    sOpeningLinkDL0Base = gSYTaskmanDLHeads[0];
}

static void openinglink_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningLinkDL0Base;
}

/* src/dc/mvopeninglink.c: the openings' fifth scene, and
 * the third of the eight per-fighter scenes -- reuses
 * test_openingmario_scene's/test_openingdonkey_scene's own three
 * things to catch (the name card, the tic-15 hand-off, the tic-60
 * hand-off and the skip exit), with this scene's own content: a
 * four-letter "LINK" name card, a real Hyrule stage (not Castle or
 * Jungle), and a hand-off to nSCKindOpeningSamus (not back to
 * nSCKindOpeningDonkey or on to nSCKindOpeningFox) -- checked directly
 * against the decomp, not assumed from the scene-kind enum's own
 * declaration order (mvopeninglink.h's header note). */
static void test_openinglink_scene(void)
{
    SObj *l, *i_letter, *n, *k;
    int i;

    /* ---- the hand-off exit: run the full 60 tics ---- */
    openinglink_begin();

    CHECK(sMVOpeningLinkNamesFileHead != NULL);
    CHECK(gSCManagerBattleState->gkind == nGRKindHyrule);
    CHECK(gSCManagerBattleState->players[0].fkind == nFTKindLink);
    CHECK(gSCManagerBattleState->players[0].pkind == nFTPlayerKindKey);

    /* the name, link 17, spelling L-I-N-K left to right */
    l = openinglink_find(17, 0x03358);
    i_letter = openinglink_find(17, 0x026B8);
    n = openinglink_find(17, 0x03E88);
    k = openinglink_find(17, 0x02F98);
    CHECK(l != NULL && i_letter != NULL && n != NULL && k != NULL);
    CHECK_EQF(l->pos.x, 100.0F); CHECK_EQF(l->pos.y, 100.0F);
    CHECK_EQF(i_letter->pos.x, 130.0F); CHECK_EQF(i_letter->pos.y, 100.0F);
    CHECK_EQF(n->pos.x, 145.0F); CHECK_EQF(n->pos.y, 100.0F);
    CHECK_EQF(k->pos.x, 180.0F); CHECK_EQF(k->pos.y, 100.0F);
    CHECK((l->sprite.attr & SP_TRANSPARENT) != 0);
    CHECK((l->sprite.attr & SP_FASTCOPY) == 0);

    for (i = 1; i <= 60; i++)
    {
        gcRunAll();
        openinglink_draw();

        if (i < 15)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningLink);
            CHECK(openinglink_find(17, 0x03358) != NULL);
        }
        else if (i == 15)
        {
            /* the name card is gone, and the motion camera is up as the
             * real battle camera */
            CHECK(openinglink_find(17, 0x03358) == NULL);
            CHECK(gGMCameraGObj != NULL);
            CHECK(gGMCameraGObj->camera_mask == GMCAMERA_BATTLE_DLLINK_MASK);
        }
        else if (i < 60)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningLink);
        }
        else
        {
            /* tic 60: the hand-off to Samus, not back to Donkey or on to
             * Fox -- Samus is ported (see
             * test_openingsamus_scene below) and picks up the chain from
             * here. */
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningSamus);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningLink);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: no countdown at all -- a tap on tic 1 exits
     * on tic 1 ---- */
    openinglink_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;

    gcRunAll();
    openinglink_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningLink);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    /* the overlay reload is what makes the next scene's layout its own */
    mvOpeningLinkOverlayLoad();
    CHECK(sMVOpeningLinkNamesFileHead == NULL);
}

/* SAMUS reads the S sprite twice (its first and last letter), so unlike
 * every sibling scene's own find helper (one offset, one match) this
 * one takes a skip count: skip=0 is the first "S" the GObj's own SObj
 * chain holds, skip=1 is the second. */
static SObj *openingsamus_find_skip(s32 link, u32 offset, s32 skip)
{
    Sprite *sp = sprite_bank_get(sMVOpeningSamusNamesFileHead, offset);
    GObj *gobj;

    for (gobj = gGCCommonLinks[link]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        for (; sobj != NULL; sobj = sobj->next)
        {
            if (sp != NULL && sobj->sprite.bitmap == sp->bitmap)
            {
                if (skip == 0)
                {
                    return sobj;
                }
                skip--;
            }
        }
    }
    return NULL;
}

static SObj *openingsamus_find(s32 link, u32 offset)
{
    return openingsamus_find_skip(link, offset, 0);
}

static Gfx *sOpeningSamusDL0Base;

static void openingsamus_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningSamus;

    syTaskmanSetupPools(&dMVOpeningSamusTaskmanSetup);
    mvOpeningSamusFuncStart();

    /* mvOpeningSamusPosedWallpaperProcDisplay (installed from tic 15)
     * writes raw gDPFillRectangle straight into gSYTaskmanDLHeads[0], the
     * same DL-head-overflow trap test_openingmario_scene's own note
     * explains; the same fix applies here. */
    wpManagerInitDLHeads();
    sOpeningSamusDL0Base = gSYTaskmanDLHeads[0];
}

static void openingsamus_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningSamusDL0Base;
}

/* src/dc/mvopeningsamus.c: the openings' sixth scene, and
 * the fourth of the eight per-fighter scenes -- reuses
 * test_openingmario_scene's own three things to catch (the name card,
 * the tic-15 hand-off, the tic-60 hand-off and the skip exit), with
 * this scene's own content: a five-letter "SAMUS" name card, a real
 * Zebes stage (not Castle, Jungle or Hyrule), and a hand-off to
 * nSCKindOpeningYoshi (not back to nSCKindOpeningLink or on to
 * nSCKindOpeningFox) -- checked directly against the decomp, not
 * assumed from the scene-kind enum's own declaration order
 * (mvopeningsamus.h's header note). */
static void test_openingsamus_scene(void)
{
    SObj *s1, *a, *m, *u, *s2;
    int i;

    /* ---- the hand-off exit: run the full 60 tics ---- */
    openingsamus_begin();

    CHECK(sMVOpeningSamusNamesFileHead != NULL);
    CHECK(gSCManagerBattleState->gkind == nGRKindZebes);
    CHECK(gSCManagerBattleState->players[0].fkind == nFTKindSamus);
    CHECK(gSCManagerBattleState->players[0].pkind == nFTPlayerKindKey);

    /* the name, link 17, spelling S-A-M-U-S left to right */
    s1 = openingsamus_find(17, 0x057F0);
    a = openingsamus_find(17, 0x005E0);
    m = openingsamus_find(17, 0x03980);
    u = openingsamus_find(17, 0x060D8);
    s2 = openingsamus_find_skip(17, 0x057F0, 1);
    CHECK(s1 != NULL && a != NULL && m != NULL && u != NULL && s2 != NULL);
    CHECK_EQF(s1->pos.x, 80.0F); CHECK_EQF(s1->pos.y, 100.0F);
    CHECK_EQF(a->pos.x, 110.0F); CHECK_EQF(a->pos.y, 100.0F);
    CHECK_EQF(m->pos.x, 150.0F); CHECK_EQF(m->pos.y, 100.0F);
    CHECK_EQF(u->pos.x, 190.0F); CHECK_EQF(u->pos.y, 100.0F);
    CHECK_EQF(s2->pos.x, 220.0F); CHECK_EQF(s2->pos.y, 100.0F);
    CHECK((s1->sprite.attr & SP_TRANSPARENT) != 0);
    CHECK((s1->sprite.attr & SP_FASTCOPY) == 0);

    for (i = 1; i <= 60; i++)
    {
        gcRunAll();
        openingsamus_draw();

        if (i < 15)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningSamus);
            CHECK(openingsamus_find(17, 0x057F0) != NULL);
        }
        else if (i == 15)
        {
            /* the name card is gone, and the motion camera is up as the
             * real battle camera */
            CHECK(openingsamus_find(17, 0x057F0) == NULL);
            CHECK(gGMCameraGObj != NULL);
            CHECK(gGMCameraGObj->camera_mask == GMCAMERA_BATTLE_DLLINK_MASK);
        }
        else if (i < 60)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningSamus);
        }
        else
        {
            /* tic 60: the hand-off to Yoshi, not back to Link or on to
             * Fox -- Yoshi is ported (test_openingyoshi_scene
             * below) and picks up the chain from here. */
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningYoshi);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningSamus);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: no countdown at all -- a tap on tic 1 exits
     * on tic 1 ---- */
    openingsamus_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;

    gcRunAll();
    openingsamus_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningSamus);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    /* the overlay reload is what makes the next scene's layout its own */
    mvOpeningSamusOverlayLoad();
    CHECK(sMVOpeningSamusNamesFileHead == NULL);
}

/* YOSHI, unlike SAMUS, spells no repeated letter -- the plain
 * first-match find helper every scene before Samus already uses is
 * enough here. */
static SObj *openingyoshi_find(s32 link, u32 offset)
{
    Sprite *sp = sprite_bank_get(sMVOpeningYoshiNamesFileHead, offset);
    GObj *gobj;

    for (gobj = gGCCommonLinks[link]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        for (; sobj != NULL; sobj = sobj->next)
        {
            if (sp != NULL && sobj->sprite.bitmap == sp->bitmap)
            {
                return sobj;
            }
        }
    }
    return NULL;
}

static Gfx *sOpeningYoshiDL0Base;

static void openingyoshi_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningYoshi;

    syTaskmanSetupPools(&dMVOpeningYoshiTaskmanSetup);
    mvOpeningYoshiFuncStart();

    /* mvOpeningYoshiPosedWallpaperProcDisplay (installed from tic 15)
     * writes raw gDPFillRectangle straight into gSYTaskmanDLHeads[0], the
     * same DL-head-overflow trap test_openingmario_scene's own note
     * explains; the same fix applies here. */
    wpManagerInitDLHeads();
    sOpeningYoshiDL0Base = gSYTaskmanDLHeads[0];
}

static void openingyoshi_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningYoshiDL0Base;
}

/* src/dc/mvopeningyoshi.c: the openings' seventh scene, and
 * the fifth of the eight per-fighter scenes -- reuses
 * test_openingmario_scene's own three things to catch (the name card,
 * the tic-15 hand-off, the tic-60 hand-off and the skip exit), with
 * this scene's own content: a five-letter "YOSHI" name card (no
 * repeated letter, unlike Samus's own), a real Yoster's Island stage
 * (not Zebes, Castle, Jungle or Hyrule), and a hand-off to
 * nSCKindOpeningKirby (not back to nSCKindOpeningSamus or on to
 * nSCKindOpeningPikachu) -- checked directly against the decomp, not
 * assumed from the scene-kind enum's own declaration order
 * (mvopeningyoshi.h's header note). */
static void test_openingyoshi_scene(void)
{
    SObj *y, *o, *s, *h, *i2;
    int i;

    /* ---- the hand-off exit: run the full 60 tics ---- */
    openingyoshi_begin();

    CHECK(sMVOpeningYoshiNamesFileHead != NULL);
    CHECK(gSCManagerBattleState->gkind == nGRKindYoster);
    CHECK(gSCManagerBattleState->players[0].fkind == nFTKindYoshi);
    CHECK(gSCManagerBattleState->players[0].pkind == nFTPlayerKindKey);

    /* the name, link 17, spelling Y-O-S-H-I left to right */
    y = openingyoshi_find(17, 0x07608);
    o = openingyoshi_find(17, 0x044B0);
    s = openingyoshi_find(17, 0x057F0);
    h = openingyoshi_find(17, 0x02408);
    i2 = openingyoshi_find(17, 0x026B8);
    CHECK(y != NULL && o != NULL && s != NULL && h != NULL && i2 != NULL);
    CHECK_EQF(y->pos.x, 80.0F);  CHECK_EQF(y->pos.y, 100.0F);
    CHECK_EQF(o->pos.x, 110.0F); CHECK_EQF(o->pos.y, 100.0F);
    CHECK_EQF(s->pos.x, 145.0F); CHECK_EQF(s->pos.y, 100.0F);
    CHECK_EQF(h->pos.x, 175.0F); CHECK_EQF(h->pos.y, 100.0F);
    CHECK_EQF(i2->pos.x, 208.0F); CHECK_EQF(i2->pos.y, 100.0F);
    CHECK((y->sprite.attr & SP_TRANSPARENT) != 0);
    CHECK((y->sprite.attr & SP_FASTCOPY) == 0);

    for (i = 1; i <= 60; i++)
    {
        gcRunAll();
        openingyoshi_draw();

        if (i < 15)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningYoshi);
            CHECK(openingyoshi_find(17, 0x07608) != NULL);
        }
        else if (i == 15)
        {
            /* the name card is gone, and the motion camera is up as the
             * real battle camera */
            CHECK(openingyoshi_find(17, 0x07608) == NULL);
            CHECK(gGMCameraGObj != NULL);
            CHECK(gGMCameraGObj->camera_mask == GMCAMERA_BATTLE_DLLINK_MASK);
        }
        else if (i < 60)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningYoshi);
        }
        else
        {
            /* tic 60: the hand-off to Kirby, not back to Samus or on to
             * Pikachu -- Kirby is ported (see
             * test_openingkirby_scene below). */
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningKirby);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningYoshi);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: no countdown at all -- a tap on tic 1 exits
     * on tic 1 ---- */
    openingyoshi_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;

    gcRunAll();
    openingyoshi_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningYoshi);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    /* the overlay reload is what makes the next scene's layout its own */
    mvOpeningYoshiOverlayLoad();
    CHECK(sMVOpeningYoshiNamesFileHead == NULL);
}

/* KIRBY spells no repeated letter -- the plain first-match find helper
 * is enough here. */
static SObj *openingkirby_find(s32 link, u32 offset)
{
    Sprite *sp = sprite_bank_get(sMVOpeningKirbyNamesFileHead, offset);
    GObj *gobj;

    for (gobj = gGCCommonLinks[link]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        for (; sobj != NULL; sobj = sobj->next)
        {
            if (sp != NULL && sobj->sprite.bitmap == sp->bitmap)
            {
                return sobj;
            }
        }
    }
    return NULL;
}

static Gfx *sOpeningKirbyDL0Base;

static void openingkirby_begin(void)
{
    sprite_bank_release_all();
    /* the real path binds Kirby's copy table in ftCommonOverlayLoad (the
     * fighting overlay load scManagerRunScene makes); Kirby is the first
     * opening scene whose fighter reads it at spawn */
    ftKirbyMainMotionBindOffsets();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningKirby;

    syTaskmanSetupPools(&dMVOpeningKirbyTaskmanSetup);
    mvOpeningKirbyFuncStart();

    /* mvOpeningKirbyPosedWallpaperProcDisplay (installed from tic 15)
     * writes raw gDPFillRectangle straight into gSYTaskmanDLHeads[0], the
     * same DL-head-overflow trap test_openingmario_scene's own note
     * explains; the same fix applies here. */
    wpManagerInitDLHeads();
    sOpeningKirbyDL0Base = gSYTaskmanDLHeads[0];
}

static void openingkirby_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningKirbyDL0Base;
}

/* src/dc/mvopeningkirby.c: the openings' eighth scene and
 * the sixth per-fighter one. Same three things to catch as
 * test_openingmario_scene, with this scene's own content: "KIRBY" (no
 * repeated letter), a real Dream Land stage, and a hand-off to
 * nSCKindOpeningFox (mvopeningkirby.h's header note). */
static void test_openingkirby_scene(void)
{
    SObj *k, *i2, *r, *b, *y;
    int i;

    /* ---- the hand-off exit: run the full 60 tics ---- */
    openingkirby_begin();

    CHECK(sMVOpeningKirbyNamesFileHead != NULL);
    CHECK(gSCManagerBattleState->gkind == nGRKindPupupu);
    CHECK(gSCManagerBattleState->players[0].fkind == nFTKindKirby);
    CHECK(gSCManagerBattleState->players[0].pkind == nFTPlayerKindKey);

    /* the name, link 17, spelling K-I-R-B-Y left to right */
    k = openingkirby_find(17, 0x02F98);
    i2 = openingkirby_find(17, 0x026B8);
    r = openingkirby_find(17, 0x05418);
    b = openingkirby_find(17, 0x009A8);
    y = openingkirby_find(17, 0x07608);
    CHECK(k != NULL && i2 != NULL && r != NULL && b != NULL && y != NULL);
    CHECK_EQF(k->pos.x, 90.0F);  CHECK_EQF(k->pos.y, 100.0F);
    CHECK_EQF(i2->pos.x, 125.0F); CHECK_EQF(i2->pos.y, 100.0F);
    CHECK_EQF(r->pos.x, 140.0F); CHECK_EQF(r->pos.y, 100.0F);
    CHECK_EQF(b->pos.x, 170.0F); CHECK_EQF(b->pos.y, 100.0F);
    CHECK_EQF(y->pos.x, 200.0F); CHECK_EQF(y->pos.y, 100.0F);
    CHECK((k->sprite.attr & SP_TRANSPARENT) != 0);
    CHECK((k->sprite.attr & SP_FASTCOPY) == 0);

    for (i = 1; i <= 60; i++)
    {
        gcRunAll();
        openingkirby_draw();

        if (i < 15)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningKirby);
            CHECK(openingkirby_find(17, 0x02F98) != NULL);
        }
        else if (i == 15)
        {
            /* the name card is gone, and the motion camera is up as the
             * real battle camera */
            CHECK(openingkirby_find(17, 0x02F98) == NULL);
            CHECK(gGMCameraGObj != NULL);
            CHECK(gGMCameraGObj->camera_mask == GMCAMERA_BATTLE_DLLINK_MASK);
        }
        else if (i < 60)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningKirby);
        }
        else
        {
            /* tic 60: the hand-off to Fox, not ported, so on a real probe
             * scManagerRunScene's default arm bounces straight back
             * here. */
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningFox);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningKirby);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: no countdown at all -- a tap on tic 1 exits
     * on tic 1 ---- */
    openingkirby_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;

    gcRunAll();
    openingkirby_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningKirby);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    /* the overlay reload is what makes the next scene's layout its own */
    mvOpeningKirbyOverlayLoad();
    CHECK(sMVOpeningKirbyNamesFileHead == NULL);
}

/* FOX spells no repeated letter -- the plain first-match find helper
 * is enough here. */
static SObj *openingfox_find(s32 link, u32 offset)
{
    Sprite *sp = sprite_bank_get(sMVOpeningFoxNamesFileHead, offset);
    GObj *gobj;

    for (gobj = gGCCommonLinks[link]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        for (; sobj != NULL; sobj = sobj->next)
        {
            if (sp != NULL && sobj->sprite.bitmap == sp->bitmap)
            {
                return sobj;
            }
        }
    }
    return NULL;
}

static Gfx *sOpeningFoxDL0Base;

static void openingfox_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningFox;

    syTaskmanSetupPools(&dMVOpeningFoxTaskmanSetup);
    mvOpeningFoxFuncStart();

    /* mvOpeningFoxPosedWallpaperProcDisplay (installed from tic 15)
     * writes raw gDPFillRectangle straight into gSYTaskmanDLHeads[0], the
     * same DL-head-overflow trap test_openingmario_scene's own note
     * explains; the same fix applies here. */
    wpManagerInitDLHeads();
    sOpeningFoxDL0Base = gSYTaskmanDLHeads[0];
}

static void openingfox_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningFoxDL0Base;
}

/* src/dc/mvopeningfox.c: the openings' ninth scene and the
 * seventh per-fighter one. Same three things to catch as
 * test_openingmario_scene: "FOX", a real Sector Z stage, and a hand-off
 * to nSCKindOpeningPikachu (mvopeningfox.h's header note). */
static void test_openingfox_scene(void)
{
    SObj *f, *o, *x;
    int i;

    /* ---- the hand-off exit: run the full 60 tics ---- */
    openingfox_begin();

    CHECK(sMVOpeningFoxNamesFileHead != NULL);
    CHECK(gSCManagerBattleState->gkind == nGRKindSector);
    CHECK(gSCManagerBattleState->players[0].fkind == nFTKindFox);
    CHECK(gSCManagerBattleState->players[0].pkind == nFTPlayerKindKey);

    /* the name, link 17, spelling F-O-X left to right */
    f = openingfox_find(17, 0x01A00);
    o = openingfox_find(17, 0x044B0);
    x = openingfox_find(17, 0x07108);
    CHECK(f != NULL && o != NULL && x != NULL);
    CHECK_EQF(f->pos.x, 110.0F); CHECK_EQF(f->pos.y, 100.0F);
    CHECK_EQF(o->pos.x, 140.0F); CHECK_EQF(o->pos.y, 100.0F);
    CHECK_EQF(x->pos.x, 185.0F); CHECK_EQF(x->pos.y, 100.0F);
    CHECK((f->sprite.attr & SP_TRANSPARENT) != 0);
    CHECK((f->sprite.attr & SP_FASTCOPY) == 0);

    for (i = 1; i <= 60; i++)
    {
        gcRunAll();
        openingfox_draw();

        if (i < 15)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningFox);
            CHECK(openingfox_find(17, 0x01A00) != NULL);
        }
        else if (i == 15)
        {
            /* the name card is gone, and the motion camera is up as the
             * real battle camera */
            CHECK(openingfox_find(17, 0x01A00) == NULL);
            CHECK(gGMCameraGObj != NULL);
            CHECK(gGMCameraGObj->camera_mask == GMCAMERA_BATTLE_DLLINK_MASK);
        }
        else if (i < 60)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningFox);
        }
        else
        {
            /* tic 60: the hand-off to Pikachu, not ported, so on a real probe
             * scManagerRunScene's default arm bounces straight back
             * here. */
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningPikachu);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningFox);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: no countdown at all -- a tap on tic 1 exits
     * on tic 1 ---- */
    openingfox_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;

    gcRunAll();
    openingfox_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningFox);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    /* the overlay reload is what makes the next scene's layout its own */
    mvOpeningFoxOverlayLoad();
    CHECK(sMVOpeningFoxNamesFileHead == NULL);
}

/* PIKACHU spells no repeated letter -- the plain first-match find helper
 * is enough here. */
static SObj *openingpikachu_find(s32 link, u32 offset)
{
    Sprite *sp = sprite_bank_get(sMVOpeningPikachuNamesFileHead, offset);
    GObj *gobj;

    for (gobj = gGCCommonLinks[link]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        for (; sobj != NULL; sobj = sobj->next)
        {
            if (sp != NULL && sobj->sprite.bitmap == sp->bitmap)
            {
                return sobj;
            }
        }
    }
    return NULL;
}

static Gfx *sOpeningPikachuDL0Base;

static void openingpikachu_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningPikachu;

    syTaskmanSetupPools(&dMVOpeningPikachuTaskmanSetup);
    mvOpeningPikachuFuncStart();

    /* mvOpeningPikachuPosedWallpaperProcDisplay (installed from tic 15)
     * writes raw gDPFillRectangle straight into gSYTaskmanDLHeads[0], the
     * same DL-head-overflow trap test_openingmario_scene's own note
     * explains; the same fix applies here. */
    wpManagerInitDLHeads();
    sOpeningPikachuDL0Base = gSYTaskmanDLHeads[0];
}

static void openingpikachu_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningPikachuDL0Base;
}

/* src/dc/mvopeningpikachu.c: the openings' tenth scene and
 * the eighth (last) per-fighter one. Same three things to catch as
 * test_openingmario_scene: "PIKACHU", a real Saffron stage, and a
 * hand-off to nSCKindOpeningRun (mvopeningpikachu.h's header note). */
static void test_openingpikachu_scene(void)
{
    SObj *p, *i2, *k, *a, *c, *h, *u;
    int i;

    /* ---- the hand-off exit: run the full 60 tics ---- */
    openingpikachu_begin();

    CHECK(sMVOpeningPikachuNamesFileHead != NULL);
    CHECK(gSCManagerBattleState->gkind == nGRKindYamabuki);
    CHECK(gSCManagerBattleState->players[0].fkind == nFTKindPikachu);
    CHECK(gSCManagerBattleState->players[0].pkind == nFTPlayerKindKey);

    /* the name, link 17, spelling P-I-K-A-C-H-U left to right */
    p = openingpikachu_find(17, 0x04890);
    i2 = openingpikachu_find(17, 0x026B8);
    k = openingpikachu_find(17, 0x02F98);
    a = openingpikachu_find(17, 0x005E0);
    c = openingpikachu_find(17, 0x00D80);
    h = openingpikachu_find(17, 0x02408);
    u = openingpikachu_find(17, 0x060D8);
    CHECK(p && i2 && k && a && c && h && u);
    CHECK_EQF(p->pos.x, 65.0F);   CHECK_EQF(p->pos.y, 100.0F);
    CHECK_EQF(i2->pos.x, 95.0F);  CHECK_EQF(i2->pos.y, 100.0F);
    CHECK_EQF(k->pos.x, 110.0F);  CHECK_EQF(k->pos.y, 100.0F);
    CHECK_EQF(a->pos.x, 140.0F);  CHECK_EQF(a->pos.y, 100.0F);
    CHECK_EQF(c->pos.x, 175.0F);  CHECK_EQF(c->pos.y, 100.0F);
    CHECK_EQF(h->pos.x, 205.0F);  CHECK_EQF(h->pos.y, 100.0F);
    CHECK_EQF(u->pos.x, 235.0F);  CHECK_EQF(u->pos.y, 100.0F);
    CHECK((p->sprite.attr & SP_TRANSPARENT) != 0);
    CHECK((p->sprite.attr & SP_FASTCOPY) == 0);

    for (i = 1; i <= 60; i++)
    {
        gcRunAll();
        openingpikachu_draw();

        if (i < 15)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningPikachu);
            CHECK(openingpikachu_find(17, 0x04890) != NULL);
        }
        else if (i == 15)
        {
            /* the name card is gone, and the motion camera is up as the
             * real battle camera */
            CHECK(openingpikachu_find(17, 0x04890) == NULL);
            CHECK(gGMCameraGObj != NULL);
            CHECK(gGMCameraGObj->camera_mask == GMCAMERA_BATTLE_DLLINK_MASK);
        }
        else if (i < 60)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningPikachu);
        }
        else
        {
            /* tic 60: the hand-off to Run, not ported, so on a real probe
             * scManagerRunScene's default arm bounces straight back
             * here. */
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningRun);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningPikachu);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: no countdown at all -- a tap on tic 1 exits
     * on tic 1 ---- */
    openingpikachu_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;

    gcRunAll();
    openingpikachu_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningPikachu);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    /* the overlay reload is what makes the next scene's layout its own */
    mvOpeningPikachuOverlayLoad();
    CHECK(sMVOpeningPikachuNamesFileHead == NULL);
}

/* The wallpaper's two SObjs: the left one is first on the link-17 GObj
 * whose SObj chain carries the wallpaper bitmap. */
static SObj *openingrun_wallpaper(void)
{
    Sprite *sp = sprite_bank_get(sMVOpeningRunWallpaperFileHead, 0x58A0);
    GObj *gobj;

    for (gobj = gGCCommonLinks[17]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        if (sobj != NULL && sp != NULL && sobj->sprite.bitmap == sp->bitmap)
        {
            return sobj;
        }
    }
    return NULL;
}

static Gfx *sOpeningRunDL0Base;

static void openingrun_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningRun;

    syTaskmanSetupPools(&dMVOpeningRunTaskmanSetup);
    mvOpeningRunFuncStart();

    wpManagerInitDLHeads();
    sOpeningRunDL0Base = gSYTaskmanDLHeads[0];
}

static void openingrun_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningRunDL0Base;
}

/* src/dc/mvopeningrun.c: the openings' eleventh scene, and
 * the first that is not a per-fighter one: eight fighters alive at once,
 * each flown by a proxy DObj, over a scrolling two-sprite wallpaper. It
 * hands off at tic 220 (not 60) to nSCKindOpeningCliff, and the skip only
 * counts from tic 10 (mvopeningrun.h's header note). */
static void test_openingrun_scene(void)
{
    SObj *wall;
    GObj *gobj;
    int i, proxies;

    /* ---- the hand-off exit: run the full 220 tics ---- */
    openingrun_begin();

    CHECK(sMVOpeningRunWallpaperFileHead != NULL);
    wall = openingrun_wallpaper();
    CHECK(wall != NULL && wall->next != NULL);
    CHECK_EQF(wall->pos.x, -320.0F);
    CHECK_EQF(wall->next->pos.x, 0.0F);
    CHECK_EQF(wall->sprite.scalex, 2.0F);

    /* eight proxies: a link-17 DObj GObj carrying its fighter */
    proxies = 0;
    for (gobj = gGCCommonLinks[17]; gobj != NULL; gobj = gobj->link_next)
    {
        if (gobj->obj_kind == 1 && gobj->user_data.p != NULL)
        {
            proxies++;
        }
    }
    CHECK(proxies == 8);

    for (i = 1; i <= 220; i++)
    {
        gcRunAll();
        openingrun_draw();

        if (i == 1)
        {
            /* the wallpaper scrolled 30 and the right sprite trails it */
            CHECK_EQF(wall->pos.x, -290.0F);
            CHECK_EQF(wall->next->pos.x, 30.0F);
        }
        if (i < 220)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningRun);
        }
        else
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningCliff);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningRun);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: taps count only from tic 10 ---- */
    openingrun_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;
    for (i = 1; i <= 9; i++)
    {
        gcRunAll();
        openingrun_draw();
        CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningRun);
    }
    gcRunAll();
    openingrun_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningRun);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    mvOpeningRunOverlayLoad();
    CHECK(sMVOpeningRunWallpaperFileHead == NULL);
}

/* The Cliff wallpaper's left SObj: first on the link-18 GObj. */
static SObj *openingcliff_wallpaper(void)
{
    Sprite *sp = sprite_bank_get(sMVOpeningCliffWallpaperFileHead, 0xB500);
    GObj *gobj;

    for (gobj = gGCCommonLinks[18]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        if (sobj != NULL && sp != NULL && sobj->sprite.bitmap == sp->bitmap)
        {
            return sobj;
        }
    }
    return NULL;
}

static Gfx *sOpeningCliffDL0Base;

static void openingcliff_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningCliff;

    syTaskmanSetupPools(&dMVOpeningCliffTaskmanSetup);
    mvOpeningCliffFuncStart();

    wpManagerInitDLHeads();
    sOpeningCliffDL0Base = gSYTaskmanDLHeads[0];
}

static void openingcliff_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningCliffDL0Base;
}

/* src/dc/mvopeningcliff.c: Link on a cliff, the hills and
 * the ocarina (packs the host cannot build: guarded), a two-sprite
 * wallpaper whose scroll speed steps down over the scene. Hands off at
 * tic 160 to nSCKindOpeningYamabuki; the skip counts from tic 10. */
static void test_openingcliff_scene(void)
{
    SObj *wall;
    int i;

    openingcliff_begin();

    CHECK(sMVOpeningCliffWallpaperFileHead != NULL);
    wall = openingcliff_wallpaper();
    CHECK(wall != NULL && wall->next != NULL);
    CHECK_EQF(wall->pos.x, 0.0F);
    CHECK_EQF(wall->next->pos.x, 320.0F);
    CHECK_EQF(wall->sprite.scalex, 2.0F);

    for (i = 1; i <= 160; i++)
    {
        gcRunAll();
        openingcliff_draw();

        if (i == 2)
        {
            int j, quads = 0;

            /* moving left, the right sprite trailing by one screen */
            CHECK(wall->pos.x < 0.0F);
            CHECK_EQF(wall->next->pos.x, wall->pos.x + 320.0F);

            /* the wallpaper camera is a backdrop camera
             * (lbCommonDrawSpriteBackdrop): every quad it draws takes a
             * depth in the backdrop band, under every 3D pixel, not the
             * front band every other sprite takes -- the layering fix */
            gcSetDrawList(PVR_LIST_TR_POLY);
            gLBCommonSpriteQuadLogCount = 0;
            openingcliff_draw();
            for (j = 0; j < gLBCommonSpriteQuadLogCount && j < LB_SPRITE_QUAD_LOG_MAX; j++)
            {
                const LBCommonSpriteQuad *q = &gLBCommonSpriteQuadLog[j];

                CHECK(q->rect.copy != -1);
                CHECK(q->z >= LB_SPRITE_Z_BACKDROP_BASE);
                CHECK(q->z < LB_SPRITE_Z_BASE);
                CHECK(j == 0 || q->z > gLBCommonSpriteQuadLog[j - 1].z);
                quads++;
            }
            CHECK(quads >= 2);
            gcSetDrawList(PVR_LIST_OP_POLY);
        }
        if (i < 160)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningCliff);
        }
        else
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningYamabuki);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningCliff);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: taps count only from tic 10 ---- */
    openingcliff_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;
    for (i = 1; i <= 9; i++)
    {
        gcRunAll();
        openingcliff_draw();
        CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningCliff);
    }
    gcRunAll();
    openingcliff_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningCliff);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    mvOpeningCliffOverlayLoad();
    CHECK(sMVOpeningCliffWallpaperFileHead == NULL);
}

static Gfx *sOpeningYamabukiDL0Base;

static void openingyamabuki_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningYamabuki;

    syTaskmanSetupPools(&dMVOpeningYamabukiTaskmanSetup);
    mvOpeningYamabukiFuncStart();

    wpManagerInitDLHeads();
    sOpeningYamabukiDL0Base = gSYTaskmanDLHeads[0];
}

static void openingyamabuki_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningYamabukiDL0Base;
}

/* src/dc/mvopeningyamabuki.c: Pikachu before a wallpaper, the
 * legs, their shadow and a Poke Ball (packs the host cannot build:
 * guarded). Hands off at tic 160 to nSCKindOpeningJungle; the skip counts
 * from tic 10. */
static void test_openingyamabuki_scene(void)
{
    Sprite *sp;
    GObj *gobj;
    SObj *wall = NULL;
    int i;

    openingyamabuki_begin();

    CHECK(sMVOpeningYamabukiWallpaperFileHead != NULL);
    sp = sprite_bank_get(sMVOpeningYamabukiWallpaperFileHead, 0x3EE58);
    CHECK(sp != NULL);
    for (gobj = gGCCommonLinks[20]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        if (sobj != NULL && sp != NULL && sobj->sprite.bitmap == sp->bitmap)
        {
            wall = sobj;
        }
    }
    CHECK(wall != NULL);
    if (wall != NULL)
    {
        CHECK_EQF(wall->pos.x, 0.0F);
        CHECK_EQF(wall->pos.y, 0.0F);
    }

    for (i = 1; i <= 160; i++)
    {
        gcRunAll();
        openingyamabuki_draw();

        if (i < 160)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningYamabuki);
        }
        else
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningJungle);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningYamabuki);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: taps count only from tic 10 ---- */
    openingyamabuki_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;
    for (i = 1; i <= 9; i++)
    {
        gcRunAll();
        openingyamabuki_draw();
        CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningYamabuki);
    }
    gcRunAll();
    openingyamabuki_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningYamabuki);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    mvOpeningYamabukiOverlayLoad();
    CHECK(sMVOpeningYamabukiWallpaperFileHead == NULL);
}

static Gfx *sOpeningJungleDL0Base;

static void openingjungle_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningJungle;

    syTaskmanSetupPools(&dMVOpeningJungleTaskmanSetup);
    mvOpeningJungleFuncStart();

    wpManagerInitDLHeads();
    sOpeningJungleDL0Base = gSYTaskmanDLHeads[0];
}

static void openingjungle_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningJungleDL0Base;
}

/* src/dc/mvopeningjungle.c: two key-driven fighters (Donkey
 * Kong and Samus) on Jungle Japes under a scripted stage camera. Hands
 * off at tic 320 to nSCKindOpeningYoster; a tap skips at any tic. */
static void test_openingjungle_scene(void)
{
    int i, fighters;
    GObj *gobj;

    openingjungle_begin();

    /* Donkey Kong and Samus: two fighter GObjs */
    fighters = 0;
    for (gobj = gGCCommonLinks[nGCCommonLinkIDFighter]; gobj != NULL; gobj = gobj->link_next)
    {
        fighters++;
    }
    CHECK(fighters == 2);

    for (i = 1; i <= 320; i++)
    {
        gcRunAll();
        openingjungle_draw();

        if (i < 320)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningJungle);
        }
        else
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningYoster);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningJungle);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: a tap counts from the first tic ---- */
    openingjungle_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;
    gcRunAll();
    openingjungle_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningJungle);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    mvOpeningJungleOverlayLoad();
}

static Gfx *sOpeningYosterDL0Base;

static void openingyoster_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningYoster;

    syTaskmanSetupPools(&dMVOpeningYosterTaskmanSetup);
    mvOpeningYosterFuncStart();

    wpManagerInitDLHeads();
    sOpeningYosterDL0Base = gSYTaskmanDLHeads[0];
}

static void openingyoster_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningYosterDL0Base;
}

/* src/dc/mvopeningyoster.c: four Yoshis in four costumes, the
 * nest and the ground (packs the host cannot build: guarded) before a
 * wallpaper. Hands off at tic 160 to nSCKindOpeningSector; the skip
 * counts from tic 10. */
static void test_openingyoster_scene(void)
{
    Sprite *sp;
    GObj *gobj;
    SObj *wall = NULL;
    int i, fighters;

    openingyoster_begin();

    CHECK(sMVOpeningYosterWallpaperFileHead != NULL);
    sp = sprite_bank_get(sMVOpeningYosterWallpaperFileHead, 0x26C88);
    CHECK(sp != NULL);
    for (gobj = gGCCommonLinks[20]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        if (sobj != NULL && sp != NULL && sobj->sprite.bitmap == sp->bitmap)
        {
            wall = sobj;
        }
    }
    CHECK(wall != NULL);
    if (wall != NULL)
    {
        CHECK_EQF(wall->pos.x, 10.0F);
        CHECK_EQF(wall->pos.y, 10.0F);
    }

    /* four Yoshis */
    fighters = 0;
    for (gobj = gGCCommonLinks[nGCCommonLinkIDFighter]; gobj != NULL; gobj = gobj->link_next)
    {
        fighters++;
    }
    CHECK(fighters == 4);

    for (i = 1; i <= 160; i++)
    {
        gcRunAll();
        openingyoster_draw();

        if (i < 160)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningYoster);
        }
        else
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningSector);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningYoster);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: taps count only from tic 10 ---- */
    openingyoster_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;
    for (i = 1; i <= 9; i++)
    {
        gcRunAll();
        openingyoster_draw();
        CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningYoster);
    }
    gcRunAll();
    openingyoster_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningYoster);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    mvOpeningYosterOverlayLoad();
    CHECK(sMVOpeningYosterWallpaperFileHead == NULL);
}

static Gfx *sOpeningSectorDL0Base;

static void openingsector_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningSector;

    syTaskmanSetupPools(&mvOpeningSectorTaskmanSetup);
    mvOpeningSectorFuncStart();

    /* the cockpit's display proc writes raw gDP commands into the DL
     * head, the same overflow trap test_openingmario_scene explains */
    wpManagerInitDLHeads();
    sOpeningSectorDL0Base = gSYTaskmanDLHeads[0];
}

static void openingsector_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningSectorDL0Base;
}

/* src/dc/mvopeningsector.c: no fighter -- a four-sprite
 * starfield that scrolls (6 px a tic to the right, then slowing), the
 * Great Fox and three Arwings (packs the host cannot build: guarded),
 * and a cockpit sprite that comes in at tic 120 and grows to full size.
 * Hands off at tic 160 to nSCKindOpeningStandoff; the skip counts from
 * tic 10. */
static void test_openingsector_scene(void)
{
    Sprite *sp;
    GObj *gobj;
    SObj *wall = NULL;
    int i;

    openingsector_begin();

    CHECK(sMVOpeningSectorWallpaperFileHead != NULL);
    CHECK(sMVOpeningSectorCockpitFileHead != NULL);
    sp = sprite_bank_get(sMVOpeningSectorWallpaperFileHead, 0x26C88);
    CHECK(sp != NULL);
    for (gobj = gGCCommonLinks[20]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        if (sobj != NULL && sp != NULL && sobj->sprite.bitmap == sp->bitmap)
        {
            wall = sobj;
        }
    }
    CHECK(wall != NULL);
    if (wall != NULL)
    {
        /* four tiles: this one and three following it */
        CHECK(wall->next != NULL && wall->next->next != NULL && wall->next->next->next != NULL);
        CHECK_EQF(wall->pos.x, 10.0F);
    }

    for (i = 1; i <= 160; i++)
    {
        gcRunAll();
        openingsector_draw();

        if (i == 2 && wall != NULL)
        {
            /* the first tic's speed is 6 to the right, wrapped back by
             * one tile once past 10 */
            CHECK(wall->pos.x != 10.0F);
            CHECK_EQF(wall->next->pos.x, wall->pos.x + 300.0F);
        }
        if (i == 119)
        {
            CHECK(gGCCommonLinks[21] == NULL);
        }
        if (i == 121)
        {
            /* the cockpit's GObj is made at tic 120, at a quarter size */
            CHECK(gGCCommonLinks[21] != NULL);
        }
        if (i < 160)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningSector);
        }
        else
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningStandoff);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningSector);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: taps count only from tic 10 ---- */
    openingsector_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;
    for (i = 1; i <= 9; i++)
    {
        gcRunAll();
        openingsector_draw();
        CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningSector);
    }
    gcRunAll();
    openingsector_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningSector);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    mvOpeningSectorOverlayLoad();
    CHECK(sMVOpeningSectorWallpaperFileHead == NULL);
}

static Gfx *sOpeningStandoffDL0Base;
static Gfx *sOpeningStandoffDL1Base;

static void openingstandoff_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningStandoff;

    syTaskmanSetupPools(&dMVOpeningStandoffTaskmanSetup);
    mvOpeningStandoffFuncStart();

    /* the flash's display proc writes raw gDP commands into DL head 1,
     * the same overflow trap test_openingmario_scene explains */
    wpManagerInitDLHeads();
    sOpeningStandoffDL0Base = gSYTaskmanDLHeads[0];
    sOpeningStandoffDL1Base = gSYTaskmanDLHeads[1];
}

static void openingstandoff_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningStandoffDL0Base;
    gSYTaskmanDLHeads[1] = sOpeningStandoffDL1Base;
}

/* src/dc/mvopeningstandoff.c: Mario and Kirby on a ground
 * (packs the host cannot build: guarded) before a two-tile wallpaper that
 * scrolls, zooms to 4x between tics 90-105 and 180-195 and then scrolls
 * up from tic 301. Hands off at tic 320 to nSCKindOpeningClash; the skip
 * counts from tic 10. */
static void test_openingstandoff_scene(void)
{
    Sprite *sp;
    GObj *gobj;
    SObj *wall = NULL;
    int i, fighters;

    openingstandoff_begin();

    CHECK(sMVOpeningStandoffWallpaperFileHead != NULL);
    sp = sprite_bank_get(sMVOpeningStandoffWallpaperFileHead, 0xB500);
    CHECK(sp != NULL);
    for (gobj = gGCCommonLinks[19]; gobj != NULL; gobj = gobj->link_next)
    {
        SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

        if (sobj != NULL && sp != NULL && sobj->sprite.bitmap == sp->bitmap)
        {
            wall = sobj;
        }
    }
    CHECK(wall != NULL);
    if (wall != NULL)
    {
        CHECK(wall->next != NULL);
        CHECK_EQF(wall->pos.x, 0.0F);
    }

    /* Mario and Kirby */
    fighters = 0;
    for (gobj = gGCCommonLinks[nGCCommonLinkIDFighter]; gobj != NULL; gobj = gobj->link_next)
    {
        fighters++;
    }
    CHECK(fighters == 2);

    for (i = 1; i <= 320; i++)
    {
        gcRunAll();
        openingstandoff_draw();

        if (wall != NULL && wall->next != NULL)
        {
            if (i == 2)
            {
                /* the first tic sets the speed to 2 */
                CHECK_EQF(wall->pos.x, 4.0F);
                CHECK_EQF(wall->next->pos.x, wall->pos.x - 320.0F);
                CHECK_EQF(wall->sprite.scalex, 2.0F);
            }
            if (i == 100)
            {
                /* the zoom window: 4x, the second tile 640 back, up 240 */
                CHECK_EQF(wall->sprite.scalex, 4.0F);
                CHECK_EQF(wall->pos.y, -240.0F);
                CHECK_EQF(wall->next->pos.x, wall->pos.x - 640.0F);
            }
            if (i == 110)
            {
                CHECK_EQF(wall->sprite.scalex, 2.0F);
                CHECK_EQF(wall->pos.y, 0.0F);
            }
            if (i == 310)
            {
                /* from tic 301 it scrolls up, not across */
                CHECK(wall->pos.y > 0.0F);
            }
        }
        if (i < 320)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningStandoff);
        }
        else
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningClash);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningStandoff);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: taps count only from tic 10 ---- */
    openingstandoff_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;
    for (i = 1; i <= 9; i++)
    {
        gcRunAll();
        openingstandoff_draw();
        CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningStandoff);
    }
    gcRunAll();
    openingstandoff_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningStandoff);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    mvOpeningStandoffOverlayLoad();
    CHECK(sMVOpeningStandoffWallpaperFileHead == NULL);
}

static Gfx *sOpeningClashDL0Base;

static void openingclash_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningClash;

    syTaskmanSetupPools(&dMVOpeningClashTaskmanSetup);
    mvOpeningClashFuncStart();

    /* the void flash writes raw gDP commands into the DL head, the same
     * overflow trap test_openingmario_scene explains */
    wpManagerInitDLHeads();
    sOpeningClashDL0Base = gSYTaskmanDLHeads[0];
}

static void openingclash_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[0] = sOpeningClashDL0Base;
}

/* src/dc/mvopeningclash.c: the eight fighters in the clash
 * status and four wallpaper quadrants (packs the host cannot build:
 * guarded); the white void GObj appears at tic 144 and it hands off at
 * tic 160 to nSCKindOpeningNewcomers; the skip counts from tic 10. */
static void test_openingclash_scene(void)
{
    GObj *gobj;
    int i, fighters, quadrants;

    openingclash_begin();

    fighters = 0;
    for (gobj = gGCCommonLinks[nGCCommonLinkIDFighter]; gobj != NULL; gobj = gobj->link_next)
    {
        fighters++;
    }
    CHECK(fighters == 8);

    quadrants = 0;
    for (gobj = gGCCommonLinks[20]; gobj != NULL; gobj = gobj->link_next)
    {
        quadrants++;
    }
    CHECK(quadrants == 4);
    CHECK(gGCCommonLinks[18] == NULL);

    for (i = 1; i <= 160; i++)
    {
        gcRunAll();
        openingclash_draw();

        if (i == 143)
        {
            CHECK(gGCCommonLinks[18] == NULL);
        }
        if (i == 145)
        {
            CHECK(gGCCommonLinks[18] != NULL);
        }
        if (i < 160)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningClash);
        }
        else
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningNewcomers);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningClash);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: taps count only from tic 10 ---- */
    openingclash_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;
    for (i = 1; i <= 9; i++)
    {
        gcRunAll();
        openingclash_draw();
        CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningClash);
    }
    gcRunAll();
    openingclash_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningClash);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    mvOpeningClashOverlayLoad();
}

static Gfx *sOpeningNewcomersDL1Base;

static void openingnewcomers_begin(void)
{
    sprite_bank_release_all();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    gSCManagerSceneData.scene_prev =
    gSCManagerSceneData.scene_curr = nSCKindOpeningNewcomers;

    syTaskmanSetupPools(&dMVOpeningNewcomersTaskmanSetup);
    mvOpeningNewcomersFuncStart();

    /* the fade's display proc writes raw gDP commands into DL head 1,
     * the same overflow trap test_openingmario_scene explains */
    wpManagerInitDLHeads();
    sOpeningNewcomersDL1Base = gSYTaskmanDLHeads[1];
}

static void openingnewcomers_draw(void)
{
    gcDrawAll();
    gSYTaskmanDLHeads[1] = sOpeningNewcomersDL1Base;
}

/* src/dc/mvopeningnewcomers.c: four silhouettes (packs the
 * host cannot build: guarded) on link 19 and no fighter; the black fade's
 * GObj appears at tic 30 and the scene ends at tic 40 on the title
 * screen; the skip counts from tic 10. Which of the four are locked comes
 * from the backup's fighter mask, checked directly. */
static void test_openingnewcomers_scene(void)
{
    GObj *gobj;
    int i, n;

    /* a fresh save has unlocked none of the four; every one of them is
     * unlocked in turn by its own bit and by nothing else */
    gSCManagerBackupData.fighter_mask = 0;
    openingnewcomers_begin();
    CHECK(mvOpeningNewcomersCheckLocked(nFTKindPurin) != FALSE);
    CHECK(mvOpeningNewcomersCheckLocked(nFTKindCaptain) != FALSE);
    CHECK(mvOpeningNewcomersCheckLocked(nFTKindLuigi) != FALSE);
    CHECK(mvOpeningNewcomersCheckLocked(nFTKindNess) != FALSE);
    CHECK(mvOpeningNewcomersCheckLocked(nFTKindMario) == FALSE);
    syTaskmanResetBreakLoop();

    gSCManagerBackupData.fighter_mask = LBBACKUP_MASK_FIGHTER(nFTKindNess) |
                                        LBBACKUP_MASK_FIGHTER(nFTKindLuigi);
    openingnewcomers_begin();
    CHECK(mvOpeningNewcomersCheckLocked(nFTKindPurin) != FALSE);
    CHECK(mvOpeningNewcomersCheckLocked(nFTKindCaptain) != FALSE);
    CHECK(mvOpeningNewcomersCheckLocked(nFTKindLuigi) == FALSE);
    CHECK(mvOpeningNewcomersCheckLocked(nFTKindNess) == FALSE);

    n = 0;
    for (gobj = gGCCommonLinks[19]; gobj != NULL; gobj = gobj->link_next)
    {
        n++;
    }
    CHECK(n == 4);
    CHECK(gGCCommonLinks[18] == NULL);

    for (i = 1; i <= 40; i++)
    {
        gcRunAll();
        openingnewcomers_draw();

        if (i == 29)
        {
            CHECK(gGCCommonLinks[18] == NULL);
        }
        if (i == 31)
        {
            CHECK(gGCCommonLinks[18] != NULL);
        }
        if (i < 40)
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningNewcomers);
        }
        else
        {
            CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
            CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningNewcomers);
            CHECK(syTaskmanCheckBreakLoop() != FALSE);
        }
    }

    syTaskmanResetBreakLoop();

    /* ---- the skip exit: taps count only from tic 10 ---- */
    openingnewcomers_begin();

    gSYControllerDevices[0].button_tap = START_BUTTON;
    for (i = 1; i <= 9; i++)
    {
        gcRunAll();
        openingnewcomers_draw();
        CHECK(gSCManagerSceneData.scene_curr == nSCKindOpeningNewcomers);
    }
    gcRunAll();
    openingnewcomers_draw();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindOpeningNewcomers);

    syTaskmanResetBreakLoop();
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    mvOpeningNewcomersOverlayLoad();
}
