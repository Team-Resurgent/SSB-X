/* hosttest/match.c -- part of hosttest_ft.c: the match: KOs and rebirth, the scene end to end, the clock, sudden
 * death, the pause menu, the off-screen arrows and magnifying
 * glasses, and three fighters at once.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* ---- the KO ----------------------------------------------
 * ft/ftcommon/ftcommondead.c and ftcommonrebirth.c, through
 * ftMainProcPhysicsMap's blast-line check. This is the test that
 * exercises the real blast-line path rather than a teleport. */

static int is_dead_down(void) { return fp.status_id == nFTCommonStatusDeadDown; }
static int is_dead_left(void) { return fp.status_id == nFTCommonStatusDeadLeftRight; }
static int is_rebirth_down(void) { return fp.status_id == nFTCommonStatusRebirthDown; }
static int is_falling(void) { return fp.status_id == nFTCommonStatusFall; }

static void test_ko_and_rebirth(void)
{
    float y_at_rebirth, y_lowered;

    spawn(0.0f, 0.0f);
    /* the mock's animations run forever unless a length is set, and
     * RebirthStand ends on its animation (ftAnimEndCheckSetStatus) */
    mock_anim_len = 12.0f;
    mock_battle.game_rules |= SCBATTLE_GAMERULE_STOCK;
    mock_battle.players[0].stock_count = 3;
    mock_battle.players[0].falls = 0;
    mock_battle.players[0].score = 0;
    mock_battle.players[0].total_selfdestructs = 0;
    fp.stock_count = 3;
    CHECK(fp.damage_player == -1);      /* nobody has hit them */

    /* off the side of the stage, just above the bottom blast line, and
     * falling: the check runs on the settled position every frame */
    ftMainRespawn(mock_gobj, -4500.0f, -2800.0f);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(run_until(0, 0, 60, is_dead_down) < 60);

    /* ftCommonDeadInitStatusVars and ftCommonDeadUpdateScore: invisible
     * and ghosted -- which is what keeps the next frame past the line
     * from KO-ing them a second time -- one fall on the record, a
     * self-destruct because damage_player is nobody, and one stock gone
     * from both the fighter and the battle state */
    CHECK(fp.is_invisible && fp.is_ghost && fp.is_menu_ignore);
    CHECK(fp.is_playertag_hide && fp.is_shadow_hide);
    CHECK(mock_battle.players[0].falls == 1);
    CHECK(mock_battle.players[0].total_selfdestructs == 1);
    CHECK(mock_battle.players[0].score == 0);
    CHECK(fp.stock_count == 2);
    CHECK(mock_battle.players[0].stock_count == 2);
    /* the check runs at priority 4, below the status's own proc_update
     * at 5, so the wait has not been decremented yet this frame */
    CHECK(fp.status_vars.common.dead.wait == FTCOMMON_DEAD_WAIT);
    CHECK_EQF(fp.physics.vel_air.y, 0.0f);

    /* the dead wait runs out into the rebirth platform, at the stage's
     * Rebirth mapobj (0, 2500), dropped in from map_bound_top */
    CHECK(run_until(0, 0, FTCOMMON_DEAD_WAIT + 4, is_rebirth_down)
          < FTCOMMON_DEAD_WAIT + 4);
    CHECK(mock_battle.players[0].falls == 1);   /* paid once, not twice */
    CHECK(fp.percent_damage == 0);              /* ftManagerInitFighter */
    CHECK(fp.status_vars.common.rebirth.halo_number == 0);
    CHECK_EQF(fp.status_vars.common.rebirth.halo_offset.x, 0.0f);
    CHECK_EQF(fp.status_vars.common.rebirth.halo_offset.y, 2500.0f);
    /* the platform is a floor the line table does not have: -2 is the
     * game's own id for it, and every proc_map on the three Rebirth
     * rows is the descent below, never the collision walk */
    CHECK(fp.coll_data.floor_line_id == -2);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK(fp.is_rebirth && fp.is_ghost);
    y_at_rebirth = fp_pos.y;
    CHECK_NEAR(y_at_rebirth, 5000.0f, 1.0f);

    /* ftCommonRebirthCommonProcMap: y is a quadratic in the remaining
     * halo_lower_wait, so 90 tics take it from map_bound_top down to
     * the halo, and it is strictly falling on the way */
    idle_frames(30);
    y_lowered = fp_pos.y;
    CHECK(y_lowered < y_at_rebirth && y_lowered > 2500.0f);
    idle_frames(FTCOMMON_REBIRTH_HALO_LOWER_WAIT - 30);
    CHECK_NEAR(fp_pos.y, 2500.0f, 1.0f);
    CHECK(fp.status_vars.common.rebirth.halo_lower_wait == 0);

    /* FTCOMMON_REBIRTH_HALO_STAND_WAIT after the platform appeared the
     * fighter stands up on it; it is still a ghost until it steps off */
    CHECK(fp.status_id == nFTCommonStatusRebirthStand ||
          fp.status_id == nFTCommonStatusRebirthWait);
    CHECK(fp.is_ghost);
    CHECK(fp.invincible_tics == 0);

    /* and at FTCOMMON_REBIRTH_HALO_DESPAWN_WAIT it drops off, with the
     * game's own invincibility timer running */
    CHECK(run_until(0, 0, FTCOMMON_REBIRTH_HALO_DESPAWN_WAIT, is_falling)
          < FTCOMMON_REBIRTH_HALO_DESPAWN_WAIT);
    CHECK(fp.invincible_tics == FTCOMMON_REBIRTH_INVINCIBLE_FRAMES);
    CHECK(!fp.is_ghost && !fp.is_rebirth);
    CHECK(fp.ga == nMPKineticsAir);

    /* the last stock: ftCommonDeadCheckRebirth sends a fighter whose
     * stock_count has gone to -1 to Sleep instead of the platform, and
     * Sleep is where it stays (the stock steal it can leave by is a
     * team battle's, and this is not one) */
    fp.stock_count = 0;
    mock_battle.players[0].stock_count = 0;
    ftMainRespawn(mock_gobj, -4500.0f, -2800.0f);
    CHECK(run_until(0, 0, 60, is_dead_down) < 60);
    CHECK(fp.stock_count == -1);
    CHECK(mock_battle.players[0].stock_count == -1);
    CHECK(mock_battle.players[0].falls == 2);
    idle_frames(FTCOMMON_DEAD_WAIT + 2);
    CHECK(fp.status_id == nFTCommonStatusSleep);
    CHECK(fp.is_invisible && fp.is_ghost);
    idle_frames(120);
    CHECK(fp.status_id == nFTCommonStatusSleep);

    mock_battle.game_rules &= ~SCBATTLE_GAMERULE_STOCK;
}

/* ftcommondead.c's 1P-game arms. A 1P enemy dies on the stage's
 * team box (map_bound_team_*), well inside the blast lines, and its KO
 * is counted by the 1P rules -- the stock and the enemy tally -- not the
 * STOCK ones. Before these arms were restored a team rung's enemies
 * could not die off the box and the rung ended on the clock. */
extern u8 sSC1PGameEnemyStocksRemaining;
extern s32 sSC1PGameBonusStatPlayerKOsNum;

static void test_ko_1pgame_team_box(void)
{
    s32 saved_rules = mock_battle.game_rules;
    s32 saved_type = mock_battle.game_type;
    s32 saved_scene_player = gSCManagerSceneData.player;
    MPGroundData saved_ground = mock_ground;

    spawn(0.0f, 0.0f);
    mock_anim_len = 12.0f;
    mock_battle.game_rules = SCBATTLE_GAMERULE_1PGAME | SCBATTLE_GAMERULE_TIME;
    mock_battle.game_type = nSCBattleGameType1PGame;
    mock_battle.players[0].is_spgame_enemy = TRUE;
    mock_battle.players[0].stock_count = 0;
    mock_battle.players[0].falls = 0;
    fp.stock_count = 0;
    gSCManagerSceneData.player = 1;          /* the human is someone else */
    sSC1PGameEnemyStocksRemaining = 3;
    sSC1PGameBonusStatPlayerKOsNum = 0;

    /* a team box inside the mock stage's blast lines (x -5000..5000),
     * as every real one is (Yoshi's Island: 5500 in, 7500 out) */
    mock_ground.map_bound_team_top = 4500;
    mock_ground.map_bound_team_bottom = -2500;
    mock_ground.map_bound_team_right = 3500;
    mock_ground.map_bound_team_left = -3500;

    /* the box is only an enemy's: the same spot does not kill a
     * fighter that is not one */
    mock_battle.players[0].is_spgame_enemy = FALSE;
    ftMainRespawn(mock_gobj, -3700.0f, 1000.0f);
    idle_frames(2);
    CHECK(fp.status_id != nFTCommonStatusDeadLeftRight);
    CHECK(mock_battle.players[0].falls == 0);

    mock_battle.players[0].is_spgame_enemy = TRUE;
    ftMainRespawn(mock_gobj, -3700.0f, 1000.0f);
    CHECK(run_until(0, 0, 4, is_dead_left) < 4);
    CHECK(fp.stock_count == -1);
    CHECK(mock_battle.players[0].stock_count == -1);
    CHECK(mock_battle.players[0].falls == 1);
    CHECK(sSC1PGameEnemyStocksRemaining == 2);
    CHECK(sSC1PGameBonusStatPlayerKOsNum == 1);

    mock_battle.players[0].is_spgame_enemy = FALSE;
    mock_battle.game_rules = saved_rules;
    mock_battle.game_type = saved_type;
    gSCManagerSceneData.player = saved_scene_player;
    mock_ground = saved_ground;
    sSC1PGameEnemyStocksRemaining = 0;
    sSC1PGameBonusStatPlayerKOsNum = 0;
}

/* the same KO, but with a killer: a landed jab sets the victim's
 * damage_player, and ftCommonDeadUpdateScore pays the score to them
 * rather than counting a self-destruct. */
static void test_ko_credits_the_attacker(void)
{
    spawn(0.0f, 0.0f);
    spawn_second(100.0f, 0.0f, -1);
    mock_anim_len = 12.0f;
    mock_battle.game_rules |= SCBATTLE_GAMERULE_STOCK;
    mock_battle.players[0].score = 0;
    mock_battle.players[0].total_kos_players[1] = 0;
    mock_battle.players[1].falls = 0;
    mock_battle.players[1].total_selfdestructs = 0;
    mock_battle.players[1].stock_count = 3;
    fpp2->stock_count = 3;

    /* a jab connects: ftMainProcParams' damage payout is what writes
     * damage_player, and it outlives the hitstun */
    frame(0, 0, N64_A, N64_A, 0);
    idle_frames(1);
    CHECK(fpp2->percent_damage > 0);
    CHECK(fpp2->damage_player == 0);

    ftMainRespawn(mock_gobj2, -4500.0f, -2800.0f);
    {
        int i;
        for (i = 0; i < 60 &&
             fpp2->status_id != nFTCommonStatusDeadDown; i++)
            frame(0, 0, 0, 0, 0);
        CHECK(i < 60);
    }
    CHECK(mock_battle.players[1].falls == 1);
    CHECK(mock_battle.players[1].total_selfdestructs == 0);
    CHECK(mock_battle.players[0].score == 1);
    CHECK(mock_battle.players[0].total_kos_players[1] == 1);
    CHECK(mock_battle.players[1].stock_count == 2);
    CHECK(fpp2->stock_count == 2);

    mock_battle.game_rules &= ~SCBATTLE_GAMERULE_STOCK;
}

/* The battle's own per-frame update, which is what the target runs:
 * scvsbattle.c:75 scVSBattleFuncUpdate is one line, and that line is
 * ifCommonBattleUpdateInterfaceAll -- the dispatcher on game_status. Its
 * arms all run the object system once, so the fighters' six processes
 * still run every tic; what changes is that the end-state arms run them
 * over a frozen world and count the GAME SET banner down.
 *
 * frame() above stays what the other tests use: it drives the six
 * processes directly, which is the same thing while the status is Go and
 * is a smaller thing to debug. mpCollisionAdvanceUpdateTic is the ground
 * GObj's process in the game, and this test has no ground GObj, so it
 * is called here as frame() calls it. */
static void battle_frame(void)
{
    memset(&mock_in, 0, sizeof(mock_in));
    memset(&mock_in2, 0, sizeof(mock_in2));
    mpCollisionAdvanceUpdateTic(NULL);
    ifCommonBattleUpdateInterfaceAll();
}

/* run battle_frame until `pred` or `limit` tics pass; tics used */
static int battle_until(int limit, int (*pred)(void))
{
    int i;

    for (i = 0; i < limit; i++)
    {
        if (pred())
            return i;
        battle_frame();
    }
    return limit;
}

static int is_status_set(void)
{
    return mock_battle.game_status == nSCBattleGameStatusSet;
}

static int is_scene_over(void)
{
    return syTaskmanCheckBreakLoop() != FALSE;
}

/* if/ifcommon.c:431, the placement counter ifCommonBattleInitPlacement
 * sets and ifCommonBattleUpdateScoreStocks walks down */
extern s32 sIFCommonBattlePlace;

/* A stock match, played to the end through the game's own frame update:
 * two players, one stock each, one of them knocked out. That KO is the
 * whole of the end condition -- ftCommonDeadUpdateScore takes the last
 * stock and calls ifCommonBattleUpdateScoreStocks, which finds nobody
 * left on that team, writes its placement, and finds the placement
 * counter at zero, which is GAME SET. What follows is a fixed timeline
 * that this test pins tic by tic, because it is the timeline a match
 * ends on and nothing else measures it. */
/* The effects the earlier tests made and left: the grab swirl comes off
 * every grab test's pull. Their packs' AnimJoint scripts are
 * 32-bit words, which the host's 8-byte AObjEvent32 cannot parse, so once
 * gcRunAll runs their processes they read garbage. Tests that only call
 * procs by hand never run them; the battle-loop tests below do, so they
 * start from an effect link with no model effect on it. */
static void eject_model_effects(void)
{
    GObj *effect_gobj = gGCCommonLinks[nGCCommonLinkIDEffect];

    while (effect_gobj != NULL)
    {
        GObj *next = effect_gobj->link_next;

        if ((DObjGetStruct(effect_gobj) != NULL) &&
            (DObjGetStruct(effect_gobj)->child != NULL))
        {
            gcEjectGObj(effect_gobj);
        }
        effect_gobj = next;
    }
}

static void test_stock_match_ends(void)
{
    int tics;

    eject_model_effects();

    /* the GAME SET freeze wakes the two particle GObjs.
     * scvsbattle.c's efParticleInitAll makes them once; this suite calls
     * it from several tests, and every call after the first finds the
     * GObjs already made and leaves the two globals NULL -- so look the
     * GObjs up by the ids lb/lbparticle.c gives them. */
    if (gcFindGObjByID(~0x5) == NULL)
    {
        efParticleInitAll();
    }
    gEFParticleStructsGObj = gcFindGObjByID(~0x5);
    gEFParticleGeneratorsGObj = gcFindGObjByID(~0x6);
    CHECK(gEFParticleStructsGObj != NULL);
    CHECK(gEFParticleGeneratorsGObj != NULL);
    spawn(0.0f, 0.0f);
    spawn_second(1000.0f, 0.0f, -1);
    mock_anim_len = 12.0f;
    mock_battle.game_rules |= SCBATTLE_GAMERULE_STOCK;
    mock_battle.is_team_battle = FALSE;
    mock_battle.players[0].place = mock_battle.players[1].place = 0;
    /* the last stock each: stock_count is what is left after this life,
     * so 0 is "on their last" and the next KO takes it to -1 */
    mock_battle.players[0].stock_count = 0;
    mock_battle.players[1].stock_count = 0;
    fpp->stock_count = 0;
    fpp2->stock_count = 0;

    /* ft/ftparam.c:2659 ftParamInitGame, as the scene calls it: two
     * players, so two teams, so the first team out places second and the
     * winner is the one never given a placement at all */
    ftParamInitGame();
    CHECK(sIFCommonBattlePlace == 1);

    mock_battle.game_status = nSCBattleGameStatusGo;

    /* player 2 over the bottom blast line on its last stock */
    ftMainRespawn(mock_gobj2, -4500.0f, -2800.0f);
    tics = 0;
    while (tics < 60 && fpp2->status_id != nFTCommonStatusDeadDown)
    {
        battle_frame();
        tics++;
    }
    CHECK(tics < 60);

    /* the KO paid the last stock, and paying it ended the match: the
     * loser has its placement, the winner still has none, and the
     * counter that decided it is spent */
    CHECK(fpp2->stock_count == -1);
    CHECK(mock_battle.players[1].stock_count == -1);
    CHECK(mock_battle.players[1].place == 1);
    CHECK(mock_battle.players[0].place == 0);
    CHECK(sIFCommonBattlePlace == 0);
    CHECK(mock_battle.game_status == nSCBattleGameStatusEnd);
    CHECK(is_scene_over() == 0);

    /* ifCommonBattleEndUpdateInterface is one tic long: it freezes the
     * world and hands the wait straight to the BossDefeat arm through
     * the dispatcher's own fallthrough. So End is never seen twice, and
     * after that tic the survivor's GObj is not being run. */
    battle_frame();
    CHECK(mock_battle.game_status == nSCBattleGameStatusBossDefeat);
    CHECK(mock_gobj->flags & GOBJ_FLAG_NORUN);
    CHECK(mock_gobj2->flags & GOBJ_FLAG_NORUN);

    /* the GAME SET banner's 90 tics (ifCommonBattleSetInterface's
     * restore_wait), one already spent on the tic above */
    tics = battle_until(120, is_status_set);
    CHECK(tics == 90);
    CHECK(is_scene_over() == 0);

    /* through the banner the particles run only on links 2 and 3, the
     * HUD's (ifCommonBattleInterfaceProcUpdate) */
    CHECK((gEFParticleStructsGObj->flags & GOBJ_FLAG_NORUN) == 0);
    CHECK(gEFParticleStructsGObj->flags & (0x10000 << 0));
    CHECK(gEFParticleStructsGObj->flags & (0x10000 << 1));
    CHECK((gEFParticleStructsGObj->flags & (0x10000 << 2)) == 0);
    CHECK((gEFParticleStructsGObj->flags & (0x10000 << 3)) == 0);
    CHECK(gEFParticleGeneratorsGObj->flags == gEFParticleStructsGObj->flags);

    /* and ifCommonBattleSetUpdateInterface's last three, after which the
     * scene says it is finished the way every scene does */
    tics = battle_until(10, is_scene_over);
    CHECK(tics == 4);
    CHECK(mock_battle.game_status == nSCBattleGameStatusSet);

    /* the interface GObjs the timeline hid -- the GAME SET banner among
     * them -- are hidden, not ejected (ifCommonBattleInterfaceProcSet) */
    {
        GObj *g = gGCCommonLinks[nGCCommonLinkIDInterface];
        int seen = 0;

        while (g != NULL)
        {
            CHECK(g->flags == GOBJ_FLAG_HIDDEN);
            seen++;
            g = g->link_next;
        }
        CHECK(seen > 0);
    }

    efParticleGObjClearSkipAll();
    mock_battle.game_rules &= ~SCBATTLE_GAMERULE_STOCK;
    mock_battle.game_status = nSCBattleGameStatusWait;
}

/* ---- the scene, end to end ------------------------------------------
 *
 * Everything above drives pieces of a battle. This drives the battle:
 * sc/sccommon/scvsbattle.c ported (src/dc/scvsbattle.c), started by the
 * scene manager's own loop (src/dc/scmanager.c), with the frame loop
 * inside it (syTaskmanRunTask). Nothing here calls a fighter process, a
 * collision tic or the HUD dispatcher -- the scene does, in the game's
 * order, and the test only watches.
 *
 * It runs last because syTaskmanStartTask re-cuts the object pools: no
 * GObj any earlier test made survives it.
 */
static s32 gSceneGoTic = -1;
static s32 gSceneDropTic;
static int gSceneDropped;
/* A match that never ends would hang the test, and the scene's loop is
 * the game's -- there is no outer counter to bound it. So the watcher
 * ends the scene itself if the timeline has not, and the test says so. */
#define SCENE_TIC_LIMIT 900
static int gSceneGaveUp;

/* the HUD's rows, found on the interface link by their
 * shape -- how many SObjs hang off the GObj and which player it says
 * it is for. sobj_count and sobj_at walk a GObj's chain. */
static int sobj_count(GObj *gobj)
{
    int n = 0;
    SObj *sobj = (gobj->obj_kind == 2) ? SObjGetStruct(gobj) : NULL;

    while (sobj != NULL)
    {
        n++;
        sobj = sobj->next;
    }
    return n;
}

static SObj *sobj_at(GObj *gobj, int i)
{
    SObj *sobj = SObjGetStruct(gobj);

    while (i-- > 0 && sobj != NULL)
    {
        sobj = sobj->next;
    }
    return sobj;
}

static GObj *hud_find(int nsobjs, s32 player)
{
    GObj *gobj = gGCCommonLinks[nGCCommonLinkIDInterface];

    while (gobj != NULL)
    {
        if (sobj_count(gobj) == nsobjs && (player < 0 || gobj->user_data.s == player))
        {
            return gobj;
        }
        gobj = gobj->link_next;
    }
    return NULL;
}

static int gSceneHudSeen[3];
static int gSceneFadeSeen[3];

/* Link 13 is shared: sys/objdef.h:92-98 gives the same id to the movie,
 * the rumble actor, the wallpaper, the fighter parts, the shadows, the
 * transition -- and put ft/ftpublic.c in the link,
 * the crowd. So "is the fade still up" is a question about a GObj of
 * that kind, not about the list being empty; before the crowd existed
 * the two happened to be the same test. */
/* The same walk on any link. The transition wipe's two GObjs are both
 * on link 0 with ids of their own (mnvsresults.c:3363-3364). */
static GObj *link_find(s32 link, u32 id)
{
    GObj *g;

    for (g = gGCCommonLinks[link]; g != NULL; g = g->link_next)
    {
        if (g->id == id)
        {
            return g;
        }
    }
    return NULL;
}

static GObj *link13_find(s32 kind)
{
    GObj *g;

    for (g = gGCCommonLinks[nGCCommonLinkIDTransition]; g != NULL;
         g = g->link_next)
    {
        if ((s32) g->id == kind)
        {
            return g;
        }
    }
    return NULL;
}

extern u16 gHostFGMStartedLog[];
extern int gHostFGMStartedCount;
extern u16 dIFCommonAnnounceDefeatedVoiceIDs[];

/* free every voice in the double's pool, so nothing below is refused a
 * slot, and start the log again */
static void crowd_reset(void)
{
    func_800266A0_272A0();
    gHostFGMStartedCount = 0;
}

/* was this voice asked for since the last reset? A whole match plays a
 * good many sounds, so the announcer's are looked for rather than
 * counted. */
static int crowd_said(u16 id)
{
    int i;
    int n = (gHostFGMStartedCount < 64) ? gHostFGMStartedCount : 64;

    for (i = 0; i < n; i++)
    {
        if (gHostFGMStartedLog[i] == id)
        {
            return 1;
        }
    }
    return 0;
}

/* the fade in from black the battle starts with
 * (scvsbattle.c:224, src/dc/lbfade.c), read off lb/lbfade.c's own
 * variables. Per tic the process walks the alpha step up and the length
 * down, so while the step is short of the top the two sum to the
 * length it was given plus two -- whichever of the process and this
 * watcher runs first in a tic -- and two tics after the top the GObj is
 * gone. The quad it draws is the one fill in the frame. */
static void scene_check_fade(s32 tic)
{
    if (tic == 3)
    {
        CHECK(link13_find(nGCCommonKindTransition) != NULL);
        CHECK(sLBFadeAlphaMax == 12);
        CHECK(sLBFadeColor.r == 0 && sLBFadeColor.a == 0);
        CHECK(sLBFadeAlphaCurrent < sLBFadeAlphaMax);
        CHECK(sLBFadeAlphaCurrent + sLBFadeLength == 14);
        gLBCommonSpriteQuadLogCount = 0;
        gSceneFadeSeen[0] = 1;
    }
    if (tic == 4)
    {
        int i, fills = 0;
        s32 cur = sLBFadeAlphaCurrent;
        s32 a0 = 0xFF - (s32)(((f32)cur / 12.0F) * 255.0F);
        s32 a1 = 0xFF - (s32)(((f32)(cur - 1) / 12.0F) * 255.0F);

        CHECK(sLBFadeAlphaCurrent + sLBFadeLength == 14);
        for (i = 0; i < gLBCommonSpriteQuadLogCount && i < LB_SPRITE_QUAD_LOG_MAX; i++)
        {
            const LBCommonSpriteQuad *q = &gLBCommonSpriteQuadLog[i];
            s32 a;

            if (q->rect.copy != -1)
            {
                continue;
            }
            fills++;
            CHECK_EQF(q->x0, 20.0f);
            CHECK_EQF(q->y0, 20.0f);
            CHECK_EQF(q->x1, 620.0f);
            CHECK_EQF(q->y1, 460.0f);
            /* past every sprite's depth: the quad takes the frame's
             * next step (lbCommonSpriteNextDepth) */
            CHECK(i == 0 || q->z > gLBCommonSpriteQuadLog[i - 1].z);
            CHECK((q->argb & 0xFFFFFF) == 0);
            a = (s32)(q->argb >> 24);
            CHECK(a == a0 || a == a1);
            /* the last quad of the frame: the fade draws after every
             * camera */
            CHECK(i == gLBCommonSpriteQuadLogCount - 1);
        }
        CHECK(fills == 1);
        gSceneFadeSeen[1] = 1;
    }
    if (tic == 20)
    {
        CHECK(link13_find(nGCCommonKindTransition) == NULL);
        CHECK(sLBFadeAlphaCurrent == 12 && sLBFadeLength == 0);
        gSceneFadeSeen[2] = 1;
    }
}

/* The HUD two tics after the GO!, once the display procs have laid it
 * out at least once with the digits showing. Every number below is
 * worked from if/ifcommon.c's text and its tables, with the sprite
 * sizes read off the SObj (they are the ROM's, out of the banks the
 * game's build makes). */
static void scene_check_hud_at_go(void)
{
    GObj *damage = hud_find(5, 0);
    GObj *stocks = hud_find(6, 0);
    GObj *tag = hud_find(1, 0);
    GObj *go = hud_find(3, -1);
    SObj *sobj;
    IFDCharacter *ch;

    CHECK(damage != NULL && stocks != NULL && tag != NULL && go != NULL);
    if (damage == NULL || stocks == NULL || tag == NULL || go == NULL)
    {
        return;
    }
    /* ifCommonPlayerDamageInitInterface: the emblem, 27x25 (Mario's
     * FTEmblem), centred on (55, 210) and nudged (3, -3), in the
     * stage's colour for player 1 */
    sobj = sobj_at(damage, 0);
    CHECK(sobj->sprite.width == 27 && sobj->sprite.height == 25);
    CHECK(sobj->pos.x == (s32)(55 - 13.5f + 3) && sobj->pos.y == (s32)(210 - 12.5f - 3));
    CHECK(sobj->sprite.red == 0xFF && sobj->sprite.green == 0xA0 && sobj->sprite.blue == 0xA0);
    CHECK(sobj->sprite.attr == (SP_TEXSHUF | SP_TRANSPARENT));
    /* ifCommonPlayerDamageUpdateDigits at 0%: two characters, "0" and
     * "%", 14 and 17 wide in the digit-width table, laid right to left
     * from the row's centre. The game's quirk, kept: the row starts at
     * scale 1.04, and the one update that lays the digits out finds the
     * centre at 55 + 31 * 1.04 / 2 = 71.12 before it drops the scale to
     * 1.0 and spaces them at that -- so the "%" is centred at 71.12 -
     * 8.5 = 62.62 and the "0" at 71.12 - 17 - 7 = 47.12, both on y 210,
     * the other two hidden, and every later update returns early
     * because nothing changed. Then ifCommonPlayerDamageProcDisplay
     * gives each SObj its digit's real Sprite and puts its top-left at
     * the centre less half its size, truncated, in player 1's colour at
     * full brightness. */
    sobj = sobj_at(damage, 1);
    ch = sobj->user_data.p;
    CHECK(ch->image_id == 0xA);
    CHECK_NEAR(ch->pos.x, 62.62f, 0.001f);
    CHECK_EQF(ch->pos.y, 210.0f);
    CHECK(!(sobj->sprite.attr & SP_HIDDEN));
    CHECK(sobj->sprite.width > 0 && sobj->sprite.scalex == 1.0f);
    /* the first digit's position is not truncated (ifcommon.c:816-817
     * assign the floats); every later one's is (:856-857) */
    CHECK_NEAR(sobj->pos.x, ch->pos.x - sobj->sprite.width * 0.5f, 0.001f);
    CHECK_NEAR(sobj->pos.y, 210.0f - sobj->sprite.height * 0.5f, 0.001f);
    CHECK(sobj->sprite.red == 0xFF && sobj->sprite.green == 0xF0 && sobj->sprite.blue == 0xF0);
    sobj = sobj_at(damage, 2);
    ch = sobj->user_data.p;
    CHECK(ch->image_id == 0);
    CHECK_NEAR(ch->pos.x, 47.12f, 0.001f);
    CHECK(!(sobj->sprite.attr & SP_HIDDEN));
    CHECK(sobj->pos.x == (s32)(ch->pos.x - sobj->sprite.width * 0.5f));
    CHECK(sobj_at(damage, 3)->sprite.attr & SP_HIDDEN);
    CHECK(sobj_at(damage, 4)->sprite.attr & SP_HIDDEN);

    /* ifCommonPlayerStockMultiProcDisplay with stock_count 0: one icon,
     * Mario's 8x10 Stock under costume 0's palette -- the first of the
     * bank's five -- at (55 - 24 - 4, 210 - 5 - 20); the other five
     * hidden */
    sobj = sobj_at(stocks, 0);
    CHECK(sobj->sprite.width == 8 && sobj->sprite.height == 10);
    CHECK(!(sobj->sprite.attr & SP_HIDDEN));
    CHECK_EQF(sobj->pos.x, 27.0f);
    CHECK_EQF(sobj->pos.y, 185.0f);
    CHECK(sobj->sprite.LUT != NULL);
    {
        FTStruct *p1 = ftGetStruct(gSCManagerBattleState->players[0].fighter_gobj);

        CHECK(p1->attr->sprites != NULL && p1->attr->sprites->stock_luts != NULL);
        CHECK(sobj->sprite.LUT == p1->attr->sprites->stock_luts[0]);
        CHECK(p1->attr->sprites->stock_luts[1] != p1->attr->sprites->stock_luts[0]);
    }
    CHECK(sobj_at(stocks, 1)->sprite.attr & SP_HIDDEN);
    CHECK(sobj_at(stocks, 5)->sprite.attr & SP_HIDDEN);

    /* ifCommonPlayerTagMakeInterface: the 1P tag in colour 0 */
    sobj = sobj_at(tag, 0);
    CHECK(sobj->sprite.width > 0);
    CHECK(sobj->sprite.attr == (SP_TEXSHUF | SP_TRANSPARENT));
    CHECK(sobj->sprite.red == 0xED && sobj->sprite.green == 0x36 && sobj->sprite.blue == 0x36);
    CHECK(sobj->envcolor.r == 0 && sobj->envcolor.g == 0);

    /* ifCommonAnnounceGoMakeInterface: G, O and ! at their table
     * positions, cloud sprites */
    sobj = sobj_at(go, 0);
    CHECK_EQF(sobj->pos.x, 82.0f);
    CHECK_EQF(sobj->pos.y, 93.0f);
    CHECK(sobj->sprite.attr == (SP_CLOUD | SP_TEXSHUF));
    CHECK(sobj->sprite.width > 0);
    sobj = sobj_at(go, 2);
    CHECK_EQF(sobj->pos.x, 214.0f);
    gSceneHudSeen[0] = 1;
}

static void scene_watch_run(GObj *gobj)
{
    (void)gobj;

    scene_check_fade((s32)dSYTaskmanUpdateCount);
    if (gSceneGoTic < 0 &&
        gSCManagerBattleState->game_status == nSCBattleGameStatusGo)
    {
        gSceneGoTic = (s32)dSYTaskmanUpdateCount;
        gSceneDropTic = gSceneGoTic + 30;
    }
    if (gSceneGoTic >= 0 && (s32)dSYTaskmanUpdateCount == gSceneGoTic + 2)
    {
        scene_check_hud_at_go();
    }
    /* sixty-one tics after the GO! the banner's thread has ejected it */
    if (gSceneGoTic >= 0 && (s32)dSYTaskmanUpdateCount == gSceneGoTic + 63)
    {
        CHECK(hud_find(3, -1) == NULL);
        gSceneHudSeen[1] = 1;
    }
    /* once player 2 has fallen and placed, GAME SET: seven letters at
     * their table positions, on the link the freeze keeps running */
    if (!gSceneHudSeen[2] && gSCManagerBattleState->game_status == nSCBattleGameStatusBossDefeat)
    {
        GObj *set = hud_find(7, -1);

        CHECK(set != NULL);
        if (set != NULL)
        {
            CHECK_EQF(sobj_at(set, 0)->pos.x, 22.0f);
            CHECK_EQF(sobj_at(set, 0)->pos.y, 95.0f);
            CHECK_EQF(sobj_at(set, 6)->pos.x, 262.0f);
            CHECK(sobj_at(set, 0)->sprite.attr == (SP_TEXSHUF | SP_TRANSPARENT));
            CHECK(!(set->flags & GOBJ_FLAG_NORUN));
        }
        gSceneHudSeen[2] = 1;
    }
    /* The one thing the test has to do from inside the match: put player
     * 2 over the bottom blast line on its last stock, because two idle
     * fighters never finish one. A pad would do this; there is no CPU
     * player here. */
    if (!gSceneDropped && gSceneGoTic >= 0 &&
        (s32)dSYTaskmanUpdateCount == gSceneDropTic)
    {
        ftMainRespawn(gSCManagerBattleState->players[1].fighter_gobj,
                      -4500.0f, -2800.0f);
        gSceneDropped = 1;
    }
    if (dSYTaskmanUpdateCount > SCENE_TIC_LIMIT)
    {
        gSceneGaveUp = 1;
        syTaskmanSetLoadScene();
    }
}

/* gSCVSBattleFuncDebug: the hook scVSBattleStartBattle calls once every
 * GObj a battle needs exists. On the interface link, which is one of the
 * two the end-state timeline keeps running after it freezes the world. */
static void scene_watch_start(void)
{
    /* scvsbattle.c:167 ftPublicMakeActor ran with the rest of them: the
     * crowd is a GObj like any other, on the link it shares with the
     * transition and the shadows. */
    CHECK(link13_find(nGCCommonKindPublic) != NULL);

    gcMakeGObjSPAfter(nGCCommonKindInterface, scene_watch_run,
                      nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);
}

static void test_scene_runs_a_match(void)
{
    int player;

    /* what the scene manager does between scenes (src/dc/scmanager.c):
     * the banks go, so the next scene's setup loads its own rather than
     * keeping pointers into a heap that has been handed out again */
    sprite_bank_release_all();
    load_stage();
    /* what spawn_desc does for the tests above, since the scene makes
     * its own fighters: the pack's attributes, and an animation that
     * ends -- the entry's Appear has to finish for a fighter to reach
     * Wait */
    mock_model.attr = mock_attr;
    mock_anim_len = 12.0f;

    /* mn/mnvsmode.c fills this in and hands it to the scene: two human
     * Marios, one stock each -- stock_count is what is left after the
     * current life, so 0 means the next KO ends it. */
    memset(&gSCManagerTransferBattleState, 0,
           sizeof(gSCManagerTransferBattleState));
    gSCManagerTransferBattleState.game_rules = SCBATTLE_GAMERULE_STOCK;
    gSCManagerTransferBattleState.stocks = 0;
    gSCManagerTransferBattleState.pl_count = 2;
    gSCManagerTransferBattleState.damage_ratio = 100;
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        SCPlayerData *pd = &gSCManagerTransferBattleState.players[player];

        pd->pkind = (player < 2) ? nFTPlayerKindMan : nFTPlayerKindNot;
        pd->fkind = nFTKindMario;
        pd->team = pd->player = player;
        pd->tag = player;
        pd->color = player;
        pd->shade = 1;
        pd->handicap = 5;
        pd->stock_count = gSCManagerTransferBattleState.stocks;
    }

    gSceneGoTic = -1;
    gSceneDropped = 0;
    gSceneGaveUp = 0;
    memset(gSceneHudSeen, 0, sizeof(gSceneHudSeen));
    memset(gSceneFadeSeen, 0, sizeof(gSceneFadeSeen));
    gSCVSBattleFuncDebug = scene_watch_start;

    /* The scene entry the scene manager would call. It is called
     * directly and not through scManagerRunScene, because the manager's
     * loop does not stop: the results screen sends the port back to the
     * battle and the two would trade forever. The manager's switch is two
     * arms, and the target run is what exercises the loop; what the
     * tests want is each scene, driven once and asserted. */
    gSCManagerSceneData.scene_curr = nSCKindVSBattle;
    crowd_reset();
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_FIGHTING);
    syDmaLoadOverlay(OVERLAY_VSBATTLE);
    scVSBattleStartScene();

    /* The battle scene did its two jobs: it ran a match, and it named
     * what follows. */
    CHECK(gSCManagerSceneData.scene_prev == nSCKindVSBattle);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSResults);
    CHECK(gSCManagerBattleState == &gSCManagerTransferBattleState);

    /* ifcommon.c's entry sequence: the entry-all thread wakes at tic 90,
     * the countdown runs five seconds, GO! unlocks the controllers --
     * 390 tics after the scene's first, which is tic 0. The watcher sees
     * it on the tic after that, because the countdown is a GObj thread
     * and threads run after the link walk this GObj's func_run is in;
     * 391 is also the frame the target's log names it on. */
    CHECK(gSceneGoTic == 391);
    CHECK(gSceneDropped);
    CHECK(!gSceneGaveUp);
    /* the HUD was seen at the GO!, GO! gone sixty tics on, and GAME SET
     * at the end */
    CHECK(gSceneHudSeen[0] && gSceneHudSeen[1] && gSceneHudSeen[2]);
    /* and the fade in from black ran its fourteen tics */
    CHECK(gSceneFadeSeen[0] && gSceneFadeSeen[1] && gSceneFadeSeen[2]);

    /* the end-state timeline ran to its last tic: the loser placed, the
     * winner never given a placement, the status left at Set */
    CHECK(gSCManagerTransferBattleState.players[1].place == 1);
    CHECK(gSCManagerTransferBattleState.players[0].place == 0);
    CHECK(gSCManagerTransferBattleState.players[1].stock_count == -1);
    CHECK(gSCManagerTransferBattleState.players[0].stock_count == 0);
    CHECK(gSCManagerTransferBattleState.game_status ==
          nSCBattleGameStatusSet);


    /* and the frame loop left when the scene said so, not before: the
     * drop, the 90-tic banner and its three-tic tail all happened. */
    CHECK((s32)dSYTaskmanUpdateCount > gSceneDropTic + 90);

    gSCVSBattleFuncDebug = NULL;
}

/* The scene after it. Runs on the battle test's leftovers on purpose:
 * gSCManagerTransferBattleState is what the battle wrote, and the
 * rankings are the only thing in the game that reads it. */
static void test_results_ranks_the_match(void)
{
    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSResults);

    /* where the screen goes on the way out depends on
     * whether its unlock check found anything, so pin that here -- the
     * item switch already unlocked, nothing queued. test_results_screen
     * is the one that pins it the other way. */
    gSCManagerBackupData.unlock_mask |= LBBACKUP_UNLOCK_MASK_ITEMSWITCH;
    {
        s32 u;

        for (u = 0; u < nLBBackupUnlockEnumCount; u++)
        {
            gSCManagerSceneData.unlock_messages[u] = nLBBackupUnlockEnumCount;
        }
    }

    gSceneFeedStart = 1;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_VSRESULTS);
    mnVSResultsStartScene();
    gSceneFeedStart = 0;

    /* mnvsresults.c:2711 mnVSResultsInitRankings over a stock match: the
     * places are the ones the battle handed out, the KOs and falls are
     * the battle's own counters, and the winner is whoever placed
     * first. Player 2 fell four times to its own blast line and player 1
     * never scored, so nobody has a KO and the tally is all falls. */
    CHECK(mnVSResultsGetWinPlayer() == 0);
    CHECK(mnVSResultsGetPlace(0) == 0);
    CHECK(mnVSResultsGetPlace(1) == 1);
    CHECK(mnVSResultsGetKOs(0) == gSCManagerTransferBattleState.players[0].score);
    CHECK(mnVSResultsGetTKO(1) == gSCManagerTransferBattleState.players[1].falls);
    CHECK(mnVSResultsGetFighterKind(0) == nFTKindMario);

    /* mnvsresults.c:266 mnVSResultsCheckExit: START does nothing until
     * sMNVSResultsTotalTimeTics has reached 370, and the counter is
     * bumped at the top of the tic, so the earliest tic that can leave is
     * index 369 -- and the scene is 370 tics long. The stub above has a
     * tap edge waiting on every odd tic, so it leaves on the first one it
     * is allowed to. */
    CHECK((s32)dSYTaskmanUpdateCount == 370);

    /* and it names what follows: the VS character select, nothing having
     * unlocked -- which is what makes scManagerRunScene's loop a loop.
     * It is the title scene. */
    CHECK(gSCManagerSceneData.scene_prev == nSCKindVSResults);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindPlayersVS);
    CHECK(gSCManagerSceneData.unlock_messages[0] == nLBBackupUnlockEnumCount);
}

/* ---- the match clock ---------------------------------- */

/* if/ifcommon.c's clock, which runs in the battle scene. The test runs the same scene as
 * test_scene_runs_a_match with the TIME rule instead of the stock rule,
 * and nobody is dropped over a blast line: what ends this match is the
 * clock running out. */
extern u32 gHostBGMVolume;
extern u32 sIFCommonTimerIsStarted;
extern u32 sIFCommonTimerLimit;
extern u8 sIFCommonTimerDigitsInterface[4];
extern ub8 sIFCommonIsAnnouncedSecond[5];

static s32 gTimeGoTic;
static s32 gTimeSlamTic;
static int gTimeSeen[4];
static int gTimeSlammed;

/* The clock's GObj on the interface link. Five SObjs is also a damage
 * row's shape -- an emblem and four digits -- so the four rows the HUD
 * keeps a pointer to are ruled out by name. */
extern IFPlayerDamage sIFCommonPlayerDamageInterface[GMCOMMON_PLAYERS_MAX];

static GObj *clock_find(void)
{
    GObj *gobj = gGCCommonLinks[nGCCommonLinkIDInterface];

    while (gobj != NULL)
    {
        if (sobj_count(gobj) == 5)
        {
            int i;
            int is_damage = 0;

            for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
            {
                if (sIFCommonPlayerDamageInterface[i].interface_gobj == gobj)
                {
                    is_damage = 1;
                }
            }
            if (!is_damage)
            {
                return gobj;
            }
        }
        gobj = gobj->link_next;
    }
    return NULL;
}

static void time_watch_run(GObj *gobj)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;

    (void)gobj;

    if (gTimeGoTic < 0 &&
        gSCManagerBattleState->game_status == nSCBattleGameStatusGo)
    {
        gTimeGoTic = tic;
        gTimeSlamTic = tic + 4;
    }
    /* [0] before GO: the clock is on screen, reading its full limit,
     * and not running. ifCommonTimerProcDisplay has cut the digits by
     * now -- the host build runs the scene's draw like the target does
     * (src/dc/taskman.c syTaskmanCommonTaskDraw) -- so a minute reads
     * 0 1 : 0 0. */
    if (gTimeGoTic < 0 && tic == 60)
    {
        GObj *clk = clock_find();

        CHECK(clk != NULL);
        CHECK(sIFCommonTimerIsStarted == FALSE);
        CHECK(sIFCommonTimerLimit == I_MIN_TO_TICS(1));
        CHECK(gSCManagerBattleState->time_remain == I_MIN_TO_TICS(1));
        CHECK(gSCManagerBattleState->time_passed == 0);
        CHECK(sIFCommonTimerDigitsInterface[0] == 0);
        CHECK(sIFCommonTimerDigitsInterface[1] == 1);
        CHECK(sIFCommonTimerDigitsInterface[2] == 0);
        CHECK(sIFCommonTimerDigitsInterface[3] == 0);
        if (clk != NULL)
        {
            /* the colon, the one SObj ifCommonTimerMakeDigits places
             * itself: 260 - w/2, 30 - h/2 */
            CHECK_EQF(sobj_at(clk, 4)->pos.x,
                      (f32)(s32)(260.0f - sobj_at(clk, 4)->sprite.width * 0.5f));
            /* and the first digit, which the display proc placed */
            CHECK_EQF(sobj_at(clk, 0)->pos.x,
                      (f32)(s32)(232.0f - sobj_at(clk, 0)->sprite.width * 0.5f));
        }
        gTimeSeen[0] = 1;
    }
    /* [1] two tics after GO the clock has moved by exactly those two.
     * The runner is on link 10 and this watcher on link 11, so gcRunAll
     * has already run the clock this tic. */
    if (gTimeGoTic >= 0 && tic == gTimeGoTic + 2)
    {
        CHECK(sIFCommonTimerIsStarted != FALSE);
        CHECK(gSCManagerBattleState->time_passed == 2);
        CHECK(gSCManagerBattleState->time_remain == I_MIN_TO_TICS(1) - 2);
        gTimeSeen[1] = 1;
    }
    /* Five seconds of clock is the interesting part of a minute of it,
     * and this is the test's one intervention: the rest of the limit is
     * skipped so the run fits in SCENE_TIC_LIMIT. Everything after this
     * is the game's own arithmetic on the game's own path. */
    if (!gTimeSlammed && gTimeGoTic >= 0 && tic == gTimeSlamTic)
    {
        gSCManagerBattleState->time_remain = I_SEC_TO_TICS(5);
        /* and the test's second intervention, from: give
         * player 1 a KO, so the two do not finish level. Since sudden
         * death landed, a TIME match that ends tied runs a second
         * battle -- which is test_sudden_death's, below. This test is
         * the clock's, and wants the one battle it has always run. */
        gSCManagerBattleState->players[0].score = 1;
        gTimeSlammed = 1;
    }
    /* [2] the announcer counts the last five down, one per second, and
     * the music comes down with them: at four seconds left the volume is
     * (240/300)*20480 + 10240. */
    if (gTimeSlammed && tic == gTimeSlamTic + I_SEC_TO_TICS(1) - 1)
    {
        CHECK(gSCManagerBattleState->time_remain == I_SEC_TO_TICS(4) + 1);
        CHECK(sIFCommonIsAnnouncedSecond[4] != FALSE);
        CHECK(sIFCommonIsAnnouncedSecond[3] == FALSE);
        CHECK(crowd_said(nSYAudioVoiceAnnounceFive));
        CHECK(gHostBGMVolume ==
              (u32)(((f32)(I_SEC_TO_TICS(4) + 1) / F_SEC_TO_TICS(5)) * 20480.0f + 10240.0f));
        gTimeSeen[2] = 1;
    }
    /* [3] TIME UP, six letters at their table positions -- checked on the
     * tic the clock reaches zero and not later, because
     * ifCommonBonusInterfaceProcUpdate freezes the interface link along
     * with everything else (the GAME SET path resumes it; this one does
     * not) and this watcher stops running with it. The clock is on link
     * 10 and the watcher on link 11, so gcRunAll has already run it. */
    if (!gTimeSeen[3] &&
        gSCManagerBattleState->game_status == nSCBattleGameStatusEnd)
    {
        GObj *up = hud_find(6, -1);

        CHECK(gSCManagerBattleState->time_remain == 0);
        CHECK(up != NULL);
        if (up != NULL)
        {
            CHECK_EQF(sobj_at(up, 0)->pos.x, 45.0f);
            CHECK_EQF(sobj_at(up, 0)->pos.y, 95.0f);
            CHECK_EQF(sobj_at(up, 5)->pos.x, 238.0f);
            CHECK(sobj_at(up, 0)->sprite.attr == (SP_TEXSHUF | SP_TRANSPARENT));
        }
        gTimeSeen[3] = 1;
    }
    if (dSYTaskmanUpdateCount > SCENE_TIC_LIMIT)
    {
        gSceneGaveUp = 1;
        syTaskmanSetLoadScene();
    }
}

static void time_watch_start(void)
{
    gcMakeGObjSPAfter(nGCCommonKindInterface, time_watch_run,
                      nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);
}

static void test_time_match(void)
{
    int player;

    sprite_bank_release_all();
    load_stage();
    mock_model.attr = mock_attr;
    mock_anim_len = 12.0f;

    memset(&gSCManagerTransferBattleState, 0,
           sizeof(gSCManagerTransferBattleState));
    gSCManagerTransferBattleState.game_rules = SCBATTLE_GAMERULE_TIME;
    gSCManagerTransferBattleState.time_limit = 1;
    gSCManagerTransferBattleState.stocks = 0;
    gSCManagerTransferBattleState.pl_count = 2;
    gSCManagerTransferBattleState.damage_ratio = 100;
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        SCPlayerData *pd = &gSCManagerTransferBattleState.players[player];

        pd->pkind = (player < 2) ? nFTPlayerKindMan : nFTPlayerKindNot;
        pd->fkind = nFTKindMario;
        pd->team = pd->player = player;
        pd->tag = player;
        pd->color = player;
        pd->shade = 1;
        pd->handicap = 5;
        pd->stock_count = gSCManagerTransferBattleState.stocks;
        /* mnplayersvs.c:4665: a TIME match draws one stock icon, not a
         * row of them */
        pd->is_single_stockicon = TRUE;
    }

    gTimeGoTic = -1;
    gTimeSlammed = 0;
    gSceneGaveUp = 0;
    memset(gTimeSeen, 0, sizeof(gTimeSeen));
    gSCVSBattleFuncDebug = time_watch_start;

    gSCManagerSceneData.scene_curr = nSCKindVSBattle;
    crowd_reset();
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_FIGHTING);
    syDmaLoadOverlay(OVERLAY_VSBATTLE);
    scVSBattleStartScene();

    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSResults);
    CHECK(!gSceneGaveUp);
    CHECK(gTimeGoTic == 391);
    CHECK(gTimeSeen[0] && gTimeSeen[1] && gTimeSeen[2] && gTimeSeen[3]);

    /* the clock stopped where it ran out and the match ended there: the
     * runner ejects itself on that tic, so time_passed is the four tics
     * before the slam plus the five seconds after it, and no more. */
    CHECK(gSCManagerBattleState->time_remain == 0);
    CHECK(gSCManagerBattleState->time_passed ==
          (u32)(4 + I_SEC_TO_TICS(5)));
    CHECK(gSCManagerBattleState->game_status == nSCBattleGameStatusSet);

    /* every one of the last five seconds was announced, and TIME UP with
     * them -- the sfx id ifCommonBattleSetInterface queues */
    CHECK(crowd_said(nSYAudioVoiceAnnounceFive));
    CHECK(crowd_said(nSYAudioVoiceAnnounceFour));
    CHECK(crowd_said(nSYAudioVoiceAnnounceThree));
    CHECK(crowd_said(nSYAudioVoiceAnnounceTwo));
    CHECK(crowd_said(nSYAudioVoiceAnnounceOne));
    CHECK(crowd_said(nSYAudioVoiceAnnounceTimeUp));

    /* nobody fell, so nobody was placed: a TIME match's places are the
     * results screen's to work out, not the battle's */
    CHECK(gSCManagerTransferBattleState.players[0].place == 0);
    CHECK(gSCManagerTransferBattleState.players[1].place == 0);

    /* The whole run. GO at 391, four tics, five seconds of clock: the
     * clock reaches zero on tic 695. Then the end timeline, which counts
     * a wait down to zero and acts on the tic after -- ninety-one for the
     * banner, four for the Set state -- and dSYTaskmanUpdateCount is one
     * past the last tic run, because syTaskmanRunFrame bumps it after
     * the update that ended the scene. */
    CHECK((s32)dSYTaskmanUpdateCount == 391 + 4 + I_SEC_TO_TICS(5) + 96);

    /* and the scene stopped at one battle: scVSBattleSetScoreCheckSuddenDeath
     * saw a clear winner and returned FALSE, so the state the results
     * screen gets is still the transfer state */
    CHECK(gSCManagerSceneData.is_suddendeath == FALSE);
    CHECK(gSCManagerBattleState == &gSCManagerTransferBattleState);

    gSCVSBattleFuncDebug = NULL;
}

/* ---- sudden death ------------------------------------
 *
 * The same TIME match as above, left tied. scVSBattleStartScene then
 * runs the battle scene a second time inside itself, on the state
 * scVSBattleSetScoreCheckSuddenDeath builds -- so this watcher is
 * installed twice, once by each setup's gSCVSBattleFuncDebug, and
 * gSCManagerSceneData.is_suddendeath is what tells the two apart.
 */
static s32 gSDGoTic[2];
static size_t gSDHeap[2];
static s32 gSDDropTic;
static int gSDSeen[4];
static int gSDSlammed;
static int gSDDropped;

static void sd_watch_run(GObj *gobj)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    s32 which = (gSCManagerSceneData.is_suddendeath != FALSE) ? 1 : 0;

    (void)gobj;

    if (gSDGoTic[which] < 0 &&
        gSCManagerBattleState->game_status == nSCBattleGameStatusGo)
    {
        gSDGoTic[which] = tic;

        if (which == 1)
        {
            gSDDropTic = tic + 30;
        }
    }
    /* The first battle, cut short the way test_time_match cuts it and
     * left level: neither player scores, so both finish on nought and
     * the check below finds a tie. */
    if (which == 0 && !gSDSlammed && gSDGoTic[0] >= 0 && tic == gSDGoTic[0] + 4)
    {
        gSCManagerBattleState->time_remain = I_SEC_TO_TICS(1);
        gSDSlammed = 1;
    }
    /* Both battles' scene-heap high-water mark, once each setup is done.
     * They have to match: the two setups allocate the same things, and
     * the one way for the second to come out short is a sprite bank the
     * heap reset threw away without unloading -- which is what sudden
     * death found in syTaskmanResetGeneralHeap. */
    if (tic == 2)
    {
        gSDHeap[which] = syTaskmanGeneralHeapUsed();
    }
    /* [0] the second battle's first tics. The state under it is the one
     * the check built, not the one the menus filled: the tied players
     * only, the rule swapped to STOCK and the score row off. */
    if (which == 1 && !gSDSeen[0] && tic == 2)
    {
        CHECK(gSCManagerBattleState == &gSCManagerVSBattleState);
        CHECK(gSCManagerBattleState->game_rules == SCBATTLE_GAMERULE_STOCK);
        CHECK(gSCManagerBattleState->is_show_score == FALSE);
        CHECK(gSCManagerBattleState->pl_count == 2);
        CHECK(gSCManagerBattleState->cp_count == 0);
        CHECK(gSCManagerBattleState->players[0].pkind == nFTPlayerKindMan);
        CHECK(gSCManagerBattleState->players[1].pkind == nFTPlayerKindMan);
        CHECK(gSCManagerBattleState->players[2].pkind == nFTPlayerKindNot);

        /* scvsbattle.c:466-468: no stock left and 300% on the clock, so
         * the first hit that lands settles it. And is_skip_entry, which
         * ft/ftmanager.c:889 reads as "stand there" rather than the
         * warp-in -- so the fighters are already in a common status and
         * not in the entry's Appear. */
        {
            FTStruct *p1 = ftGetStruct(gSCManagerBattleState->players[0].fighter_gobj);
            FTStruct *p2 = ftGetStruct(gSCManagerBattleState->players[1].fighter_gobj);

            CHECK(p1 != NULL && p2 != NULL);
            if (p1 != NULL && p2 != NULL)
            {
                CHECK(p1->percent_damage == 300);
                CHECK(p2->percent_damage == 300);
                CHECK(p1->stock_count == 0);
                CHECK(p2->stock_count == 0);
                CHECK(p1->status_id != nFTCommonStatusEntry);
                CHECK(p2->status_id != nFTCommonStatusEntry);
            }
        }
        gSDSeen[0] = 1;
    }
    /* [1] the banner. Twelve letters out of the announcer's plain
     * alphabet (file 37, bank slot 7) at the table's two rows, painted
     * white on black by ifCommonAnnounceSetColors -- every other banner
     * in the port takes its colour from the sprite. */
    if (which == 1 && !gSDSeen[1] && tic == 2)
    {
        GObj *sd = hud_find(12, -1);

        CHECK(sd != NULL);
        if (sd != NULL)
        {
            CHECK_EQF(sobj_at(sd, 0)->pos.x, 74.0f);
            CHECK_EQF(sobj_at(sd, 0)->pos.y, 67.0f);
            CHECK_EQF(sobj_at(sd, 6)->pos.x, 83.0f);
            CHECK_EQF(sobj_at(sd, 6)->pos.y, 113.0f);
            CHECK_EQF(sobj_at(sd, 11)->pos.x, 227.0f);
            CHECK(sobj_at(sd, 0)->sprite.attr == (SP_TEXSHUF | SP_TRANSPARENT));
            CHECK(sobj_at(sd, 0)->sprite.red == 0xFF);
            CHECK(sobj_at(sd, 0)->sprite.green == 0xFF);
            CHECK(sobj_at(sd, 0)->sprite.blue == 0xFF);
            CHECK(sobj_at(sd, 11)->envcolor.r == 0x00);
            CHECK(sobj_at(sd, 11)->envcolor.g == 0x00);
            CHECK(sobj_at(sd, 11)->envcolor.b == 0x00);
            /* the two letters the table repeats are the same sprite */
            CHECK(sobj_at(sd, 2)->sprite.width == sobj_at(sd, 3)->sprite.width);
        }
        CHECK(crowd_said(nSYAudioVoiceAnnounceSuddenDeath));
        gSDSeen[1] = 1;
    }
    /* [2] the GO!. There is no countdown in a sudden death -- the
     * thread sleeps the entry sequence's ninety tics and goes straight
     * to it -- so the banner and the traffic light never meet. */
    if (which == 1 && !gSDSeen[2] && gSDGoTic[1] >= 0 && tic == gSDGoTic[1] + 2)
    {
        CHECK(hud_find(3, -1) != NULL);     /* GO!'s three letters */
        CHECK(hud_find(11, -1) == NULL);    /* the countdown's eleven */
        /* and SUDDEN DEATH is gone with the wait it belonged to: the
         * thread's gcEjectGObj(NULL) takes the GObj it is running on,
         * banner and all, the same way the entry-all thread's does */
        CHECK(hud_find(12, -1) == NULL);
        gSDSeen[2] = 1;
    }
    /* the one intervention the second battle needs, the same one
     * test_scene_runs_a_match makes: two idle fighters never finish */
    if (which == 1 && !gSDDropped && gSDGoTic[1] >= 0 && tic == gSDDropTic)
    {
        ftMainRespawn(gSCManagerBattleState->players[1].fighter_gobj,
                      -4500.0f, -2800.0f);
        gSDDropped = 1;
    }
    /* [3] GAME SET, not TIME UP: the rule is STOCK by now */
    if (which == 1 && !gSDSeen[3] &&
        gSCManagerBattleState->game_status == nSCBattleGameStatusBossDefeat)
    {
        CHECK(hud_find(7, -1) != NULL);
        gSDSeen[3] = 1;
    }
    if (dSYTaskmanUpdateCount > SCENE_TIC_LIMIT)
    {
        gSceneGaveUp = 1;
        syTaskmanSetLoadScene();
    }
}

static void sd_watch_start(void)
{
    gcMakeGObjSPAfter(nGCCommonKindInterface, sd_watch_run,
                      nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);
}

static void test_sudden_death(void)
{
    int player;

    sprite_bank_release_all();
    load_stage();
    mock_model.attr = mock_attr;
    mock_anim_len = 12.0f;

    memset(&gSCManagerTransferBattleState, 0,
           sizeof(gSCManagerTransferBattleState));
    gSCManagerTransferBattleState.game_rules = SCBATTLE_GAMERULE_TIME;
    gSCManagerTransferBattleState.time_limit = 1;
    gSCManagerTransferBattleState.stocks = 0;
    gSCManagerTransferBattleState.pl_count = 2;
    gSCManagerTransferBattleState.damage_ratio = 100;
    gSCManagerTransferBattleState.is_show_score = TRUE;
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        SCPlayerData *pd = &gSCManagerTransferBattleState.players[player];

        pd->pkind = (player < 2) ? nFTPlayerKindMan : nFTPlayerKindNot;
        pd->fkind = nFTKindMario;
        pd->team = pd->player = player;
        pd->tag = player;
        pd->color = player;
        pd->shade = 1;
        pd->handicap = 5;
        pd->stock_count = gSCManagerTransferBattleState.stocks;
        pd->is_single_stockicon = TRUE;
    }

    gSDGoTic[0] = gSDGoTic[1] = -1;
    gSDSlammed = gSDDropped = 0;
    gSceneGaveUp = 0;
    memset(gSDSeen, 0, sizeof(gSDSeen));
    gSCVSBattleFuncDebug = sd_watch_start;

    gSCManagerSceneData.is_suddendeath = FALSE;
    gSCManagerSceneData.is_reset = FALSE;
    gSCManagerSceneData.scene_curr = nSCKindVSBattle;
    crowd_reset();
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_FIGHTING);
    syDmaLoadOverlay(OVERLAY_VSBATTLE);
    scVSBattleStartScene();

    CHECK(!gSceneGaveUp);
    CHECK(gSDSeen[0] && gSDSeen[1] && gSDSeen[2] && gSDSeen[3]);

    /* the first battle ran the whole entry sequence, the second none of
     * it: ifCommonSuddenDeathThread's ninety tics and then GO!, seen on
     * the tic after, against the entry sequence's 90 + five seconds */
    CHECK(gSDGoTic[0] == 391);
    CHECK(gSDGoTic[1] == 91);

    /* and the second battle set up out of the same heap as the first:
     * every bank it needs was loaded again, because emptying the heap
     * unloaded them */
    CHECK(gSDHeap[0] != 0);
    CHECK(gSDHeap[1] == gSDHeap[0]);

    /* the check ran, said yes, and left the flag the results screen
     * reads set -- and the state pointer moved with it */
    CHECK(gSCManagerSceneData.is_suddendeath != FALSE);
    CHECK(gSCManagerBattleState == &gSCManagerVSBattleState);

    /* the second battle's result, on the second battle's state: the one
     * who fell placed second, the survivor was never placed. The
     * transfer state still holds the tied TIME match that got them
     * here. */
    CHECK(gSCManagerVSBattleState.players[1].place == 1);
    CHECK(gSCManagerVSBattleState.players[0].place == 0);
    CHECK(gSCManagerVSBattleState.players[1].stock_count == -1);
    CHECK(gSCManagerTransferBattleState.players[0].score == 0);
    CHECK(gSCManagerTransferBattleState.players[1].score == 0);

    CHECK(gSCManagerSceneData.scene_prev == nSCKindVSBattle);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSResults);

    /* and the whole thing, tic for tic: the second battle is 91 to the
     * GO!, 30 to the drop, and the end timeline after it. */
    CHECK((s32)dSYTaskmanUpdateCount > gSDDropTic + 90);

    gSCVSBattleFuncDebug = NULL;
}

/* And the screen after it, on the sudden death's leftovers -- the state
 * pointer still at gSCManagerVSBattleState and is_suddendeath still set,
 * which is the one combination mnVSResultsSetRoyalPlace's second
 * condition exists for: with the flag on, two players who scored the
 * same still take different places if the battle gave them different
 * ones. Without it a sudden death would be reported as the draw that
 * started it. */
static void test_sudden_death_rankings(void)
{
    CHECK(gSCManagerSceneData.is_suddendeath != FALSE);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSResults);

    {
        s32 u;

        for (u = 0; u < nLBBackupUnlockEnumCount; u++)
        {
            gSCManagerSceneData.unlock_messages[u] = nLBBackupUnlockEnumCount;
        }
    }

    gSceneFeedStart = 1;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_VSRESULTS);
    mnVSResultsStartScene();
    gSceneFeedStart = 0;

    /* nobody scored in the tied TIME match that got here, so both come
     * out of mnVSResultsGetPointsDirect on nought -- and it is the
     * places the sudden death handed out that separate them. */
    CHECK(mnVSResultsGetPlace(0) == 0);
    CHECK(mnVSResultsGetPlace(1) == 1);
    CHECK(mnVSResultsGetWinPlayer() == 0);
    CHECK(mnVSResultsGetKOs(0) == 0);
    CHECK(mnVSResultsGetKOs(1) == 0);

    /* still a TIME match's screen: the rule the results reads is the
     * transfer state's, which sudden death does not touch */
    CHECK((s32)dSYTaskmanUpdateCount == 410);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindPlayersVS);

    /* Put the scene data back the way every other test expects to find
     * it. Nothing in the game clears is_suddendeath but
     * scVSBattleStartBattle, and the tests that follow do not run one. */
    gSCManagerSceneData.is_suddendeath = FALSE;
    gSCManagerBattleState = &gSCManagerTransferBattleState;
}

/* mnvsresults.c:2688 mnVSResultsSetPlaceTime, the arm the results screen
 * refused to take until there was a clock. The scores are put in by hand
 * because two idle Marios score nothing: what is under test is the sort,
 * and the sort's input is the battle state's score and falls. */
static void test_time_rankings(void)
{
    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSResults);

    gSCManagerTransferBattleState.players[0].score = 1;
    gSCManagerTransferBattleState.players[0].falls = 2;
    gSCManagerTransferBattleState.players[1].score = 3;
    gSCManagerTransferBattleState.players[1].falls = 1;

    gSCManagerBackupData.unlock_mask |= LBBACKUP_UNLOCK_MASK_ITEMSWITCH;
    {
        s32 u;

        for (u = 0; u < nLBBackupUnlockEnumCount; u++)
        {
            gSCManagerSceneData.unlock_messages[u] = nLBBackupUnlockEnumCount;
        }
    }

    gSceneFeedStart = 1;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_VSRESULTS);
    mnVSResultsStartScene();
    gSceneFeedStart = 0;

    /* points are KOs minus falls: player 2 has 2, player 1 has -1. The
     * sort is highest first and places count from 0, so player 2 wins a
     * match it did not survive any better than the other. */
    CHECK(mnVSResultsGetPlace(1) == 0);
    CHECK(mnVSResultsGetPlace(0) == 1);
    CHECK(mnVSResultsGetWinPlayer() == 1);
    CHECK(mnVSResultsGetKOs(1) == 3);
    CHECK(mnVSResultsGetTKO(0) == 2);

    /* mnvsresults.c:2820: a TIME match's screen holds 410 tics before
     * START will leave it, not the stock match's 370 -- there is more of
     * it to read. This is the first test to take that arm. */
    CHECK((s32)dSYTaskmanUpdateCount == 410);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindPlayersVS);
}

/* the Auto handicap. On the time match's leftovers
 * (player 2 on 2 points, player 1 on -1), both players human: the results
 * screen moves the best human's handicap down one and the worst's up one,
 * and only when the rule is Auto. Then the clamp arms directly: at 1 and 9
 * nothing moves, a best already at 1 gives the worst both steps, and a
 * worst already at 9 takes both off the best. */
extern void mnVSResultsSetAutoHandicaps(s32 best, s32 worst);

static void test_auto_handicap(void)
{
    SCBattleState *bs = &gSCManagerTransferBattleState;
    u8 pkind_bak[2] = { bs->players[0].pkind, bs->players[1].pkind };
    u8 handicap_rule_bak = bs->handicap;
    u8 handicap_bak[2] = { bs->players[0].handicap, bs->players[1].handicap };
    s32 u;

    CHECK(gSCManagerSceneData.scene_curr == nSCKindPlayersVS);
    gSCManagerSceneData.scene_curr = nSCKindVSResults;

    bs->players[0].pkind = nFTPlayerKindMan;
    bs->players[1].pkind = nFTPlayerKindMan;
    bs->players[0].score = 1;
    bs->players[0].falls = 2;
    bs->players[1].score = 3;
    bs->players[1].falls = 1;
    bs->players[0].handicap = 5;
    bs->players[1].handicap = 5;
    bs->handicap = nSCBattleHandicapAuto;

    gSCManagerBackupData.unlock_mask |= LBBACKUP_UNLOCK_MASK_ITEMSWITCH;
    for (u = 0; u < nLBBackupUnlockEnumCount; u++)
    {
        gSCManagerSceneData.unlock_messages[u] = nLBBackupUnlockEnumCount;
    }
    gSceneFeedStart = 1;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_VSRESULTS);
    mnVSResultsStartScene();
    gSceneFeedStart = 0;

    CHECK(mnVSResultsGetWinPlayer() == 1);
    CHECK(bs->players[1].handicap == 4);
    CHECK(bs->players[0].handicap == 6);

    /* the clamp arms, best = player 2 and worst = player 1 */
    bs->players[1].handicap = 1;
    bs->players[0].handicap = 9;
    mnVSResultsSetAutoHandicaps(1, 0);
    CHECK(bs->players[1].handicap == 1 && bs->players[0].handicap == 9);

    bs->players[0].handicap = 7;
    mnVSResultsSetAutoHandicaps(1, 0);
    CHECK(bs->players[1].handicap == 1 && bs->players[0].handicap == 9);

    bs->players[1].handicap = 3;
    mnVSResultsSetAutoHandicaps(1, 0);
    CHECK(bs->players[1].handicap == 1 && bs->players[0].handicap == 9);

    bs->handicap = handicap_rule_bak;
    bs->players[0].handicap = handicap_bak[0];
    bs->players[1].handicap = handicap_bak[1];
    bs->players[0].pkind = pkind_bak[0];
    bs->players[1].pkind = pkind_bak[1];
}

/* ---- the pause menu -----------------------------------
 *
 * One stock match, paused twice. The first pause is unpaused; the second
 * is reset out of. What makes this testable from the host is that the
 * pause is not a flag anything reads -- it is the tic on which
 * ifCommonBattleUpdateInterfaceAll does not call gcRunAll -- so a GObj
 * that counts its own runs measures the freeze exactly.
 *
 * The probe cannot be that GObj, for the same reason: a paused tic never
 * runs it. It is gSceneTicHook, which syControllerFuncRead calls every
 * tic before the scene update, pause or no pause. That is also where the
 * pad has to be written, since the update reads it in the same tic.
 */
extern u8 sIFCommonBattlePausePlayer;
extern u8 sIFCommonBattlePauseKindInterface;

static int gPauseGoTic;
static int gPauseRunTics;       /* tics on which the object system ran */
static int gPausePausedAt;      /* the tic the first pause began */
static int gPauseResetAt;       /* the tic the reset combo was held */
static int gPauseSeen[6];
static f32 gPausePanX;

static void pause_watch_run(GObj *gobj)
{
    (void)gobj;

    gPauseRunTics++;

    if (gPauseGoTic < 0 &&
        gSCManagerBattleState->game_status == nSCBattleGameStatusGo)
    {
        gPauseGoTic = (s32)dSYTaskmanUpdateCount;
    }
}

static void pause_watch_start(void)
{
    gcMakeGObjSPAfter(nGCCommonKindInterface, pause_watch_run,
                      nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);
}

/* the pause menu's two GObjs: the frame, which carries no SObjs, and the
 * sprites, which carry thirteen -- the player number and twelve decals */
static GObj *pause_find(int nsobjs)
{
    GObj *gobj = gGCCommonLinks[nGCCommonLinkIDPauseMenu];

    while (gobj != NULL)
    {
        if (sobj_count(gobj) == nsobjs)
        {
            return gobj;
        }
        gobj = gobj->link_next;
    }
    return NULL;
}

/* ifcommon.c's bonus-stage pause arms for the bonus
 * stages. L on the pause menu retries a course, and only one played
 * from its own menu: the task ends with the status still Pause, which
 * is what sc1PBonusStageStartScene reads to run the course again. A
 * course reached from a 1P run has no retry. */
/* Master Hand's defeat ends in ifCommonBattleEndSetBossDefeat: the
 * last background row's colour fade calls it, and it has to leave the
 * game in BossDefeat with the restore wait at zero, or
 * ifCommonBattleBossDefeatUpdateInterface never runs the proc_set that
 * reaches Set (the disc showed the chain end in 1pstageclear). */
static void test_boss_defeat_end(void)
{
    extern u16 sIFCommonBattlePauseCameraRestoreWait;
    extern u16 sSC1PGameBossDefeatSoundTerminateTemp;
    SCBattleState *saved_state = gSCManagerBattleState;
    SCBattleState state = gSCManager1PGameBattleState;
    u16 saved_wait = sIFCommonBattlePauseCameraRestoreWait;
    unsigned count = fgm_get_count();

    gSCManagerBattleState = &state;
    state.game_status = nSCBattleGameStatusEnd;
    sIFCommonBattlePauseCameraRestoreWait = U16_MAX;

    /* the defeat's start, sc1PGameBossDefeatInterfaceProcUpdate's last
     * two lines: no new sound until the end below puts the count back */
    sSC1PGameBossDefeatSoundTerminateTemp = count;
    fgm_set_count(0);
    CHECK(func_800269C0_275C0(nSYAudioFGMGamePause) == NULL);

    ifCommonBattleEndSetBossDefeat();
    CHECK(state.game_status == nSCBattleGameStatusBossDefeat);
    CHECK(sIFCommonBattlePauseCameraRestoreWait == 0);
    CHECK(fgm_get_count() == count);

    sIFCommonBattlePauseCameraRestoreWait = saved_wait;
    gSCManagerBattleState = saved_state;
}

/* sc1PTrainingModeLoadWallpaper: each stage's row of
 * dSC1PTrainingModeWallpaperIDs picks one of relocData 26-28, and the
 * one 300x220 sprite in that bank is what the stage draws instead of its
 * own. Yoshi's Story is the yellow row, Sector Z the black, Peach's
 * Castle the blue. */
void sc1PTrainingModeLoadWallpaper(void);

static void test_training_wallpaper(void)
{
    SCBattleState *saved_state = gSCManagerBattleState;
    SCBattleState state = gSCManagerVSBattleState;
    static const s32 rows[] = { nGRKindYoster, nGRKindSector, nGRKindCastle };
    u32 i;

    gSCManagerBattleState = &state;

    for (i = 0; i < ARRAY_COUNT(rows); i++)
    {
        state.gkind = rows[i];
        gHostStageWallpaperOverride = NULL;
        gHostStageWallpaperOverrideCalls = 0;

        sc1PTrainingModeLoadWallpaper();

        CHECK(gHostStageWallpaperOverrideCalls == 1);
        CHECK(gHostStageWallpaperOverride != NULL);
        if (gHostStageWallpaperOverride != NULL)
        {
            CHECK(gHostStageWallpaperOverride->imgw == 300);
            CHECK(gHostStageWallpaperOverride->imgh == 220);
            CHECK(gHostStageWallpaperOverride->fmt == nDCSpriteTexFmtARGB1555);
        }
    }
    gSCManagerBattleState = saved_state;
}

/* The opening Room's tic-1040 star wipe (src/dc/mvopeningroom.c):
 * romdisk/mvroomwipe.bin, the triangles tools/export/ssb_roomwipeexport.py
 * bakes out of relocData 63, read by the scene's own loader.
 *
 *  - the counts and colours the ROM holds: 62 inner-star triangles in
 *    white and 128 outer-star ones in the red the ring shows;
 *  - the wipe's whole premise: at the AnimJoint's final scale 1.0 the
 *    INNER star, seen by the transition camera (eye z 1000, fovy
 *    39.56, CObj default aspect 4/3), covers every corner of the
 *    viewport, so the tic-1080 eject hands over a fully revealed scene;
 *    and at the first scale 0.05 the OUTER star sits wholly inside it,
 *    so the wipe opens as a small star in the middle of the frozen
 *    frame. Either failing would mean a wrong file, offset or camera. */
static s32 wipe_point_in_tris(const f32 *c, s32 count, f32 px, f32 py)
{
    s32 t;

    for (t = 0; t < count; t++)
    {
        const f32 *a = c + (t * 3 + 0) * 3, *b = c + (t * 3 + 1) * 3,
                  *d = c + (t * 3 + 2) * 3;
        f32 d1 = (px - b[0]) * (a[1] - b[1]) - (a[0] - b[0]) * (py - b[1]);
        f32 d2 = (px - d[0]) * (b[1] - d[1]) - (b[0] - d[0]) * (py - d[1]);
        f32 d3 = (px - a[0]) * (d[1] - a[1]) - (d[0] - a[0]) * (py - a[1]);
        s32 neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        s32 pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

        if (!(neg && pos))
        {
            return 1;
        }
    }
    return 0;
}

/* src/dc/assetroot.c's models.bnd (tools/export/ssb_bundle.py): a bundle the
 * test writes itself, in the tool's layout, mounted and read back through
 * asset_open -- entries not a multiple of 32 long, two files open at once
 * over the one shared handle, a seek, a read past the end, a name the
 * bundle does not have falling through to the loose tree, and a file that
 * is not a bundle refused. NOT YET TESTED ON REAL HARDWARE (2026-09-22):
 * this test is the whole of the verification so far. */
static u8 bundle_byte(int k, long i) { return (u8)(k * 37 + i * 7); }

static void test_asset_bundle(void)
{
    static const char *const names[3] = { "a.mdl", "b.mdl", "c.mdl" };
    static const long sizes[3] = { 40, 64, 1 };
    const char *path = "hosttest_bundle.bnd";
    u8 file[512];
    u8 buf[128];
    long off = 160;     /* 16 + 3 * 40 = 136, rounded up to 32 */
    long offs[3];
    AssetFile a, b;
    FILE *fh;   /* not fp: the decomp headers make that a macro */
    int k;
    long i;

    memset(file, 0, sizeof(file));
    memcpy(file, "BND2", 4);
    file[4] = 3;
    file[8] = 16;
    file[12] = (u8)off;
    for (k = 0; k < 3; k++)
    {
        u8 *e = file + 16 + 40 * k;

        offs[k] = off;
        strcpy((char *)e, names[k]);
        e[32] = (u8)off;
        e[33] = (u8)(off >> 8);
        e[36] = (u8)sizes[k];
        for (i = 0; i < sizes[k]; i++)
            file[off + i] = bundle_byte(k, i);
        off = (off + sizes[k] + 31) & ~31L;
    }
    fh = fopen(path, "wb");
    CHECK(fh != NULL);
    if (fh == NULL)
        return;
    fwrite(file, 1, (size_t)off, fh);
    fclose(fh);

    CHECK(asset_bundle_mount(path) == 0);
    CHECK(asset_bundle_count() == 3);

    /* whole, and clamped to the entry */
    if (asset_open(&a, "a.mdl") != 0)
    {
        CHECK(!"a.mdl did not open out of the bundle");
        asset_bundle_unmount();
        remove(path);
        return;
    }
    CHECK(a.bundled && a.base == offs[0] && asset_size(&a) == 40);
    CHECK(asset_mmap(&a) == NULL);
    CHECK(asset_read(&a, buf, sizeof(buf)) == 40);
    for (i = 0; i < 40; i++)
        CHECK(buf[i] == bundle_byte(0, i));
    CHECK(asset_read(&a, buf, 8) == 0);

    /* interleaved over the one handle, and a seek */
    CHECK(asset_seek(&a, 5) == 0);
    CHECK(asset_open(&b, "b.mdl") == 0);
    CHECK(asset_read(&b, buf, 10) == 10 && buf[9] == bundle_byte(1, 9));
    CHECK(asset_read(&a, buf, 10) == 10 && buf[0] == bundle_byte(0, 5) &&
          buf[9] == bundle_byte(0, 14));
    CHECK(asset_read(&b, buf, 54) == 54 && buf[0] == bundle_byte(1, 10) &&
          buf[53] == bundle_byte(1, 63));
    CHECK(asset_seek(&b, 65) < 0);
    asset_close(&b);
    asset_close(&a);

    /* a one-byte file, through the whole-file reader */
    {
        long n = 0;
        u8 *w = asset_read_whole("c.mdl", &n);

        CHECK(w != NULL && n == 1 && w[0] == bundle_byte(2, 0));
        free(w);
    }

    /* not in the bundle: the loose tree, as before */
    CHECK(asset_open(&a, "mario.pack") == 0);
    CHECK(!a.bundled && asset_size(&a) > 0);
    asset_close(&a);
    CHECK(asset_open(&a, "romdisk/a.mdl") < 0);

    /* unmounted, the bundle's names are gone */
    asset_bundle_unmount();
    CHECK(asset_bundle_count() == 0);
    CHECK(asset_open(&a, "a.mdl") < 0);

    /* something that is not a bundle mounts nothing */
    CHECK(asset_bundle_mount("romdisk/mario.pack") < 0);
    CHECK(asset_bundle_count() == 0);
    CHECK(asset_bundle_mount("romdisk/no-such.bnd") < 0);
    remove(path);
}

/* src/dc/assetroot.c's hold list: a held name is read whole on its
 * first open and served from RAM after that -- the same bytes, a seek,
 * a read past the end, no mapping -- and a list that drops the name
 * frees the copy while one it keeps carries it over. */
static void test_asset_hold(void)
{
    static const char *const held[2] = { "mario.pack", "fox.pack" };
    static const char *const fox_only[1] = { "fox.pack" };
    AssetFile a, b;
    u8 buf[64], want[64];
    const u8 *copy;
    long size;
    int i;

    asset_hold_set(held, 2);
    CHECK(asset_hold_bytes() == 0);

    /* the loose file's first 64 bytes, for comparison */
    CHECK(asset_open(&b, "romdisk/mario.pack") == 0);
    CHECK(b.held == NULL && asset_read(&b, want, 64) == 64);
    asset_close(&b);

    CHECK(asset_open(&a, "mario.pack") == 0);
    CHECK(a.held != NULL && asset_mmap(&a) == NULL);
    size = asset_size(&a);
    CHECK(asset_hold_bytes() == size);
    CHECK(asset_read(&a, buf, 64) == 64 && memcmp(buf, want, 64) == 0);
    CHECK(asset_seek(&a, 10) == 0 && asset_read(&a, buf, 4) == 4 &&
          memcmp(buf, want + 10, 4) == 0);
    CHECK(asset_seek(&a, size - 3) == 0 && asset_read(&a, buf, 64) == 3);
    CHECK(asset_seek(&a, size + 1) < 0);
    copy = a.held;
    asset_close(&a);

    /* a second open is the same copy */
    CHECK(asset_open(&a, "mario.pack") == 0 && a.held == copy);
    asset_close(&a);

    /* the same list again keeps it; a list without it frees it */
    asset_hold_set(held, 2);
    CHECK(asset_open(&a, "mario.pack") == 0 && a.held == copy);
    asset_close(&a);
    CHECK(asset_open(&a, "fox.pack") == 0 && a.held != NULL);
    asset_close(&a);
    asset_hold_set(fox_only, 1);
    CHECK(asset_open(&a, "mario.pack") == 0 && a.held == NULL);
    asset_close(&a);
    CHECK(asset_hold_bytes() > 0);

    /* a path is never held, and an empty list frees everything */
    for (i = 0; i < 2; i++)
    {
        asset_hold_set(held, 2);
        CHECK(asset_open(&a, "romdisk/fox.pack") == 0 && a.held == NULL);
        asset_close(&a);
    }
    asset_hold_set(NULL, 0);
    CHECK(asset_hold_bytes() == 0);
    CHECK(asset_open(&a, "fox.pack") == 0 && a.held == NULL);
    asset_close(&a);
}

#include "scprefetch.h"        /* dSCPrefetchOpening */

/* assetroot.h's reading ahead. Off the SH-4 asset_prefetch_go reads
 * the queue on the spot, so every state the loader leaves is here in
 * order: queued, read, served, handed over, freed. */
static void test_asset_prefetch(void)
{
    static const char *const both[2] = { "mario.pack", "fox.pack" };
    static const char *const fox[1] = { "fox.pack" };
    AssetFile a;
    u8 mario64[64], fox64[64], buf[64];
    long mario_size, fox_size;
    void *p;

    CHECK(asset_open(&a, "romdisk/mario.pack") == 0);
    mario_size = asset_size(&a);
    CHECK(asset_read(&a, mario64, 64) == 64);
    asset_close(&a);
    CHECK(asset_open(&a, "romdisk/fox.pack") == 0);
    fox_size = asset_size(&a);
    CHECK(asset_read(&a, fox64, 64) == 64);
    asset_close(&a);

    /* two scenes queued; nothing is read before the first one's go, and
     * a name it opens meanwhile is read off the medium and dequeued */
    asset_prefetch_clear();
    asset_prefetch_add(1, both, 2);
    asset_prefetch_add(2, fox, 1);
    CHECK(asset_prefetch_enter(1));
    CHECK(asset_prefetch_bytes() == 0);
    CHECK(asset_open(&a, "fox.pack") == 0 && a.held == NULL && a.ahead == 0);
    asset_close(&a);
    asset_prefetch_go();
    CHECK(asset_prefetch_bytes() == mario_size + fox_size);

    /* scene 1's copy, handed over whole */
    CHECK(asset_open(&a, "mario.pack") == 0 && a.ahead != 0);
    p = asset_read_all(&a);
    CHECK(p != NULL && memcmp(p, mario64, 64) == 0 && a.ahead == 0);
    asset_close(&a);
    free(p);
    CHECK(asset_prefetch_bytes() == fox_size);

    /* scene 2's copy is not scene 1's to open */
    CHECK(asset_open(&a, "fox.pack") == 0 && a.ahead == 0);
    asset_close(&a);

    /* in scene 2 it is served, read like any file, freed at close; the
     * next open goes to the medium */
    CHECK(asset_prefetch_enter(2));
    CHECK(asset_open(&a, "fox.pack") == 0 && a.ahead != 0);
    CHECK(asset_read(&a, buf, 64) == 64 && memcmp(buf, fox64, 64) == 0);
    CHECK(asset_seek(&a, fox_size - 3) == 0 && asset_read(&a, buf, 64) == 3);
    asset_close(&a);
    CHECK(asset_prefetch_bytes() == 0);
    CHECK(asset_open(&a, "fox.pack") == 0 && a.ahead == 0);
    asset_close(&a);

    /* a scene not queued changes nothing */
    CHECK(!asset_prefetch_enter(7));

    /* a scene passed gives back what it never opened */
    asset_prefetch_clear();
    asset_prefetch_add(4, both, 2);
    asset_prefetch_add(5, fox, 1);
    CHECK(asset_prefetch_enter(4));
    asset_prefetch_go();
    CHECK(asset_prefetch_bytes() == mario_size + 2 * fox_size);
    CHECK(asset_prefetch_enter(5));
    CHECK(asset_prefetch_bytes() == fox_size);

    /* the hold list takes the loader's copy instead of reading again */
    asset_hold_set(fox, 1);
    CHECK(asset_open(&a, "fox.pack") == 0 && a.held != NULL && a.ahead == 0);
    asset_close(&a);
    CHECK(asset_prefetch_bytes() == 0 && asset_hold_bytes() == fox_size);
    asset_hold_set(NULL, 0);

    /* clearing under an open file leaves its copy until the close */
    asset_prefetch_clear();
    asset_prefetch_add(6, fox, 1);
    CHECK(asset_prefetch_enter(6));
    asset_prefetch_go();
    CHECK(asset_open(&a, "fox.pack") == 0 && a.ahead != 0);
    asset_prefetch_clear();
    CHECK(asset_read(&a, buf, 64) == 64 && memcmp(buf, fox64, 64) == 0);
    asset_close(&a);
    CHECK(asset_prefetch_bytes() == 0);
    CHECK(asset_open(&a, "fox.pack") == 0 && a.ahead == 0);
    asset_close(&a);

    /* and every name the opening movie queues is a file, so a list gone
     * stale by a rename fails here rather than as a quiet miss */
    {
        s32 row, k;

        for (row = 0; row < (s32)ARRAY_COUNT(dSCPrefetchOpening); row++)
        {
            for (k = 0; k < dSCPrefetchOpening[row].count; k++)
            {
                const char *name = dSCPrefetchOpening[row].files[k];
                char disc[64];

                /* a few models exist only on the disc, in its bundle */
                snprintf(disc, sizeof(disc), "disc/%s", name);
                if (asset_open(&a, name) != 0 && asset_open(&a, disc) != 0)
                {
                    printf("prefetch: %s is not a file\n", name);
                    CHECK(0);
                    continue;
                }
                asset_close(&a);
            }
        }
    }
}

static void test_room_wipe(void)
{
    const f32 *overlay, *outline;
    s32 n_overlay, n_outline, i, k;
    u32 argb;
    f32 hh = 1000.0F * tanf(39.56115341F * 0.5F * 3.14159265F / 180.0F);
    f32 hw = hh * (4.0F / 3.0F);

    CHECK(mvOpeningRoomLoadWipe());

    overlay = mvOpeningRoomGetWipeTris(FALSE, &n_overlay, &argb);
    CHECK(overlay != NULL);
    CHECK(n_overlay == 62);
    CHECK(argb == 0xFFFFFFFF);

    outline = mvOpeningRoomGetWipeTris(TRUE, &n_outline, &argb);
    CHECK(outline != NULL);
    CHECK(n_outline == 128);
    CHECK(argb == 0xFFFF0000);

    if (overlay == NULL || outline == NULL)
    {
        return;
    }
    for (k = 0; k < 4; k++)
    {
        CHECK(wipe_point_in_tris(overlay, n_overlay, (k & 1) ? hw : -hw, (k & 2) ? hh : -hh));
    }
    for (i = 0; i < n_outline * 3; i++)
    {
        CHECK(fabsf(outline[i * 3 + 0] * 0.05F) < hw);
        CHECK(fabsf(outline[i * 3 + 1] * 0.05F) < hh);
    }
}

/* gcMtxModLookAt (syMatrixModLookAtF): matrix kinds 8-11/14-17's view,
 * where up.x is a roll. Roll 0 is the plain look-at off the fixed axis;
 * a roll keeps the line of sight, keeps the basis orthonormal, and
 * turns the up row by exactly that angle. */
static void test_mod_lookat(void)
{
    float plain[16], rolled[16];
    const f32 roll = 0.15F;
    f32 dot;
    s32 r, c;

    gcMtxLookAt(plain, 300.0F, 500.0F, 1700.0F, 0.0F, 100.0F, 0.0F, 0.0F, 1.0F, 0.0F);
    gcMtxModLookAt(rolled, 300.0F, 500.0F, 1700.0F, 0.0F, 100.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F);
    for (r = 0; r < 16; r++)
    {
        CHECK(fabsf(plain[r] - rolled[r]) < 1e-4F);
    }
    gcMtxModLookAt(rolled, 300.0F, 500.0F, 1700.0F, 0.0F, 100.0F, 0.0F, roll, 0.0F, 1.0F, 0.0F);

    /* the look row is the line of sight either way */
    for (c = 0; c < 3; c++)
    {
        CHECK(fabsf(plain[8 + c] - rolled[8 + c]) < 1e-5F);
    }
    /* orthonormal rows */
    for (r = 0; r < 3; r++)
    {
        for (c = 0; c < 3; c++)
        {
            dot = rolled[r * 4 + 0] * rolled[c * 4 + 0] + rolled[r * 4 + 1] * rolled[c * 4 + 1] + rolled[r * 4 + 2] * rolled[c * 4 + 2];
            CHECK(fabsf(dot - ((r == c) ? 1.0F : 0.0F)) < 1e-5F);
        }
    }
    /* up turned by the roll, not by atan(up.x) toward world X */
    dot = plain[4] * rolled[4] + plain[5] * rolled[5] + plain[6] * rolled[6];
    CHECK(fabsf(dot - cosf(roll)) < 1e-5F);
}

static void test_bonus_pause_retry(void)
{
    SCCommonData saved_scene = gSCManagerSceneData;
    u8 saved_player = sIFCommonBattlePausePlayer;
    u8 saved_kind = sIFCommonBattlePauseKindInterface;
    u16 saved_tap = gSYControllerDevices[0].button_tap;

    syTaskmanResetBreakLoop();
    sIFCommonBattlePausePlayer = 0;
    sIFCommonBattlePauseKindInterface = nIFPauseKindPlayerNA; /* no camera */
    gSCManagerSceneData.scene_curr = nSCKind1PBonusStage;

    /* from a 1P run: L does nothing */
    gSCManagerSceneData.scene_prev = nSCKind1PGame;
    gSYControllerDevices[0].button_tap = L_TRIG;
    ifCommonBattlePauseUpdateInterface();
    CHECK(syTaskmanCheckBreakLoop() == FALSE);

    /* from the Break the Targets menu: L ends the task */
    gSCManagerSceneData.scene_prev = nSCKind1PBonus1Players;
    ifCommonBattlePauseUpdateInterface();
    CHECK(syTaskmanCheckBreakLoop() != FALSE);
    syTaskmanResetBreakLoop();

    /* and outside a bonus stage L is nothing at all */
    gSCManagerSceneData.scene_curr = nSCKindVSBattle;
    gSCManagerSceneData.scene_prev = nSCKindVSMode;
    ifCommonBattlePauseUpdateInterface();
    CHECK(syTaskmanCheckBreakLoop() == FALSE);

    gSYControllerDevices[0].button_tap = saved_tap;
    sIFCommonBattlePauseKindInterface = saved_kind;
    sIFCommonBattlePausePlayer = saved_player;
    gSCManagerSceneData = saved_scene;
}

static int pause_link_count(void)
{
    GObj *gobj = gGCCommonLinks[nGCCommonLinkIDPauseMenu];
    int n = 0;

    while (gobj != NULL)
    {
        n++;
        gobj = gobj->link_next;
    }
    return n;
}

/* is every GObj on the HUD's link hidden? ifCommonBattlePauseInitInterface
 * sets the flags whole rather than or-ing, so the test is equality. */
static int hud_all_hidden(u32 flags)
{
    GObj *gobj = gGCCommonLinks[nGCCommonLinkIDInterface];

    while (gobj != NULL)
    {
        if (gobj->flags != flags)
        {
            return 0;
        }
        gobj = gobj->link_next;
    }
    return 1;
}

static void pause_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    SYController *pad = &gSYControllerDevices[0];
    int go = gPauseGoTic;

    pad->button_tap = 0;
    pad->button_hold = 0;
    pad->stick_range.x = 0;
    pad->stick_range.y = 0;

    if (go < 0)
    {
        return;
    }

    /* --- the first pause ------------------------------------------- */

    /* ten tics into the match, START on pad 0 */
    if (tic == go + 10)
    {
        pad->button_tap = START_BUTTON;
        gPausePausedAt = tic;
        gPauseSeen[0] = gPauseRunTics;   /* borrowed as the run count */
        return;
    }
    /* the tic after: the pause the previous tic's update built. Two
     * GObjs on the pause link, thirteen sprites on the second of them,
     * the HUD hidden, the chime rung, and the world not run -- the
     * update that made the menu is the last one that called gcRunAll. */
    if (tic == gPausePausedAt + 1)
    {
        GObj *sprites = pause_find(13);

        CHECK(gSCManagerBattleState->game_status == nSCBattleGameStatusPause);
        CHECK(pause_link_count() == 2);
        CHECK(pause_find(0) != NULL);
        CHECK(sprites != NULL);
        CHECK(sIFCommonBattlePausePlayer == 0);
        /* both fighters stand where the entry put them, well inside the
         * camera's reach, so it is the full menu with the close-up */
        CHECK(sIFCommonBattlePauseKindInterface == nIFPauseKindDefault);
        CHECK(gIFCommonPlayerInterface.is_magnify_display == FALSE);
        CHECK(hud_all_hidden(GOBJ_FLAG_HIDDEN));
        CHECK(crowd_said(nSYAudioFGMGamePause));
        CHECK(gHostBGMVolume == 0x3C00);
        if (sprites != NULL)
        {
            /* the player number, then PAUSE, then the four buttons --
             * ifCommonBattlePauseMakeSObjsAll's order, at the decal
             * table's positions */
            SObj *sobj = sobj_at(sprites, 0);

            CHECK_EQF(sobj->pos.x, 213.0f);
            CHECK_EQF(sobj->pos.y, 191.0f);
            CHECK(sobj->sprite.attr == (SP_TEXSHUF | SP_TRANSPARENT));
            CHECK(sobj->sprite.width > 0);

            sobj = sobj_at(sprites, 1);
            CHECK_EQF(sobj->pos.x, 232.0f);
            CHECK_EQF(sobj->pos.y, 191.0f);

            /* the A button, blue on darker blue */
            sobj = sobj_at(sprites, 2);
            CHECK_EQF(sobj->pos.x, 99.0f);
            CHECK(sobj->sprite.red == 0x00 && sobj->sprite.green == 0x95 &&
                  sobj->sprite.blue == 0xFF);
            CHECK(sobj->envcolor.r == 0x00 && sobj->envcolor.g == 0x05 &&
                  sobj->envcolor.b == 0xC7);

            /* the three pluses are one sprite drawn three times */
            CHECK(sobj_at(sprites, 6)->sprite.width ==
                  sobj_at(sprites, 7)->sprite.width);
            CHECK_EQF(sobj_at(sprites, 7)->pos.x, 136.0f);

            /* the last of the twelve is the control stick, which only
             * the default menu draws */
            CHECK_EQF(sobj_at(sprites, 12)->pos.x, 31.0f);
            CHECK_EQF(sobj_at(sprites, 12)->pos.y, 29.0f);
        }
        /* the pausing tic itself did not run the object system either:
         * ifCommonBattleGoUpdateInterface returns on the START it finds,
         * and gcRunAll is the line after the loop it returns out of */
        CHECK(gPauseRunTics == gPauseSeen[0]);
        gPauseSeen[1] = 1;
        gPausePanX = gGMCameraPauseCameraEyeX;
        return;
    }
    /* forty paused tics with the stick held right, at 0.000333 radians
     * per unit per tic. Nothing else in the scene moves through any of
     * them. */
    if (tic > gPausePausedAt + 1 && tic <= gPausePausedAt + 41)
    {
        pad->stick_range.x = 80;

        /* ten tics in, before the clamp: the pan is exactly its
         * arithmetic, ten times eighty times the rate */
        if (tic == gPausePausedAt + 12)
        {
            CHECK_EQF(gGMCameraPauseCameraEyeX,
                      gPausePanX + 10 * (80 * 0.000333F));
        }
        return;
    }
    if (tic == gPausePausedAt + 42)
    {
        /* forty tics of it is 1.0656 radians, past fifty degrees, so the
         * camera is sitting on the clamp */
        CHECK(gGMCameraPauseCameraEyeX > gPausePanX);
        CHECK_EQF(gGMCameraPauseCameraEyeX, F_CLC_DTOR32(50.0F));
        /* and in forty-one paused tics the world never ran once */
        CHECK(gPauseRunTics == gPauseSeen[0]);
        gPauseSeen[2] = 1;

        /* START again: unpause */
        pad->button_tap = START_BUTTON;
        return;
    }
    /* the twenty tics of the camera easing back. The status is Unpause
     * and the menu is still up -- the eject is on the last of them. */
    if (tic == gPausePausedAt + 44)
    {
        CHECK(gSCManagerBattleState->game_status == nSCBattleGameStatusUnpause);
        CHECK(pause_link_count() == 2);
        CHECK(gGMCameraPauseCameraEyeX < F_CLC_DTOR32(50.0F));
        CHECK(gPauseRunTics == gPauseSeen[0]);
        return;
    }
    /* the twenty-first tic runs the restore's tail: menu ejected, HUD
     * back, status Go, the camera's angles put back exactly, and the
     * object system run again on that same tic -- the first tic the
     * world has moved since the pause. */
    if (tic == gPausePausedAt + 64)
    {
        CHECK(gSCManagerBattleState->game_status == nSCBattleGameStatusGo);
        CHECK(pause_link_count() == 0);
        CHECK(hud_all_hidden(0));
        CHECK(gIFCommonPlayerInterface.is_magnify_display != FALSE);
        CHECK_EQF(gGMCameraPauseCameraEyeX, gPausePanX);
        CHECK(gHostBGMVolume == 0x7800);
        CHECK(gPauseRunTics == gPauseSeen[0] + 1);
        gPauseSeen[3] = 1;
        return;
    }
    /* the match is running again, tic for tic */
    if (tic == gPausePausedAt + 74)
    {
        CHECK(gPauseRunTics == gPauseSeen[0] + 11);
        gPauseSeen[4] = 1;
    }

    /* --- the second pause, and the reset --------------------------- */

    if (tic == gPausePausedAt + 80)
    {
        pad->button_tap = START_BUTTON;
        return;
    }
    if (tic == gPausePausedAt + 82)
    {
        CHECK(gSCManagerBattleState->game_status == nSCBattleGameStatusPause);
        CHECK(gSCManagerSceneData.is_reset == FALSE);
        /* A+B+R+Z. The game reads the combo out of button_hold but only
         * looks at it on a tic with a tap, so both are set. */
        pad->button_tap = A_BUTTON;
        pad->button_hold = A_BUTTON | B_BUTTON | R_TRIG | Z_TRIG;
        gPauseResetAt = tic;
        return;
    }
    if (gPauseResetAt > 0 && tic == gPauseResetAt + 1)
    {
        /* is_reset set, the match ended into the three-tic Set state,
         * and the menu hidden rather than ejected -- the reset does not
         * unwind the pause, it ends the match under it */
        CHECK(gSCManagerSceneData.is_reset != FALSE);
        CHECK(gSCManagerBattleState->game_status == nSCBattleGameStatusSet);
        CHECK(pause_link_count() == 2);
        CHECK(hud_all_hidden(GOBJ_FLAG_HIDDEN));
        gPauseSeen[5] = 1;
    }
    if (dSYTaskmanUpdateCount > SCENE_TIC_LIMIT)
    {
        gSceneGaveUp = 1;
        syTaskmanSetLoadScene();
    }
}

static void test_pause_menu(void)
{
    int player;

    sprite_bank_release_all();
    load_stage();
    mock_model.attr = mock_attr;
    mock_anim_len = 12.0f;

    memset(&gSCManagerTransferBattleState, 0,
           sizeof(gSCManagerTransferBattleState));
    gSCManagerTransferBattleState.game_rules = SCBATTLE_GAMERULE_STOCK;
    gSCManagerTransferBattleState.stocks = 2;
    gSCManagerTransferBattleState.pl_count = 2;
    gSCManagerTransferBattleState.damage_ratio = 100;
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        SCPlayerData *pd = &gSCManagerTransferBattleState.players[player];

        pd->pkind = (player < 2) ? nFTPlayerKindMan : nFTPlayerKindNot;
        pd->fkind = nFTKindMario;
        pd->team = pd->player = player;
        pd->tag = player;
        pd->color = player;
        pd->shade = 1;
        pd->handicap = 5;
        pd->stock_count = gSCManagerTransferBattleState.stocks;
    }

    gPauseGoTic = -1;
    gPauseRunTics = 0;
    gPausePausedAt = -1;
    gPauseResetAt = -1;
    gSceneGaveUp = 0;
    memset(gPauseSeen, 0, sizeof(gPauseSeen));
    gSCVSBattleFuncDebug = pause_watch_start;
    gSceneTicHook = pause_tic;

    gSCManagerSceneData.is_reset = FALSE;
    gSCManagerSceneData.scene_curr = nSCKindVSBattle;
    crowd_reset();
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_FIGHTING);
    syDmaLoadOverlay(OVERLAY_VSBATTLE);
    scVSBattleStartScene();

    gSceneTicHook = NULL;
    gSCVSBattleFuncDebug = NULL;

    CHECK(!gSceneGaveUp);
    CHECK(gPauseGoTic == 391);
    CHECK(gPauseSeen[1] && gPauseSeen[2] && gPauseSeen[3] && gPauseSeen[4] &&
          gPauseSeen[5]);

    /* The scene ended on the reset, not on a KO: nobody was placed and
     * nobody lost a stock. */
    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSResults);
    CHECK(gSCManagerTransferBattleState.players[0].place == 0);
    CHECK(gSCManagerTransferBattleState.players[1].place == 0);
    CHECK(gSCManagerTransferBattleState.players[0].stock_count == 2);
    CHECK(gSCManagerTransferBattleState.players[1].stock_count == 2);

    /* and not into sudden death either: scVSBattleStartScene's tie check
     * is guarded on is_reset, so a reset out of a level
     * match is a no contest and not a tiebreak. */
    CHECK(gSCManagerSceneData.is_suddendeath == FALSE);
    CHECK(gSCManagerBattleState == &gSCManagerTransferBattleState);

    /* The whole run: GO at 391, the pause at GO+10, and the match ends
     * four tics after the reset -- ifCommonBattleInterfaceProcSet's
     * three-tic wait plus the tic the loop leaves on. */
    CHECK((s32)dSYTaskmanUpdateCount == gPauseResetAt + 5);
}

/* The results screen the reset leads to. is_reset is still set, so this
 * is the arm scvsresults.c has carried with nothing to
 * set it: NO CONTEST, which collapses the screen's three build tics to
 * one and holds 200 tics instead of a stock match's 370. */
static void test_pause_reset_no_contest(void)
{
    s32 u;

    CHECK(gSCManagerSceneData.scene_curr == nSCKindVSResults);
    CHECK(gSCManagerSceneData.is_reset != FALSE);

    sprite_bank_release_all();
    gSCManagerBackupData.unlock_mask |= LBBACKUP_UNLOCK_MASK_ITEMSWITCH;
    for (u = 0; u < nLBBackupUnlockEnumCount; u++)
    {
        gSCManagerSceneData.unlock_messages[u] = nLBBackupUnlockEnumCount;
    }

    gSceneFeedStart = 1;
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_SUBSYS);
    syDmaLoadOverlay(OVERLAY_VSRESULTS);
    mnVSResultsStartScene();
    gSceneFeedStart = 0;

    CHECK((s32)dSYTaskmanUpdateCount == 200);
    CHECK(gSCManagerSceneData.scene_curr == nSCKindPlayersVS);

    /* put it back for the tests after this one */
    gSCManagerSceneData.is_reset = FALSE;
}

/* ---- the off-screen arrows ----------------------------
 *
 * Three functions and a camera, and the interesting thing about all four
 * is that the host link can drive them: the arrows are decided in the
 * update (ifCommonPlayerArrowsFuncRun, a process on the interface link)
 * off two bits the *display* wrote (FTStruct.is_magnify_show and
 * .magnify_pos, src/dc/ftdisplaymain.c), so the half that has no
 * renderer under it is exactly the half that is worth checking.
 *
 * The one thing the test has to supply is the frame's draw. On the
 * target a frame lowers gIFCommonPlayerInterface.arrows_flags in
 * gmCameraDefaultProcDisplay and every fighter's display proc raises its
 * own again a moment later; there is no draw in this link, so
 * arrows_draw_pass below is those two lines in that order.
 */
/* src/dc/ifcommon.c:467 -- the arrows' FGM cadence counter */
extern u8 sIFCommonPlayerMagnifySoundWait;

static s32 gArrowGoTic;
static s32 gArrowSeen[10];
static FTStruct *arrow_fp(s32 player)
{
    return ftGetStruct(gSCManagerBattleState->players[player].fighter_gobj);
}

/* Put a fighter's TopN joint somewhere. That joint is the whole of what
 * ftDisplayMainProcDisplay reads to decide the question, so writing it
 * is the shortest honest way to ask. */
static void arrow_put(s32 player, f32 x, f32 y)
{
    DObj *topn = arrow_fp(player)->joints[nFTPartsJointTopN];

    topn->translate.vec.f.x = x;
    topn->translate.vec.f.y = y;
    topn->translate.vec.f.z = 0.0F;
}

static void arrows_draw_pass(void)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    gGCCurrentCamera = gGMCameraGObj;
    gIFCommonPlayerInterface.magnify_mode = 0;
    gIFCommonPlayerInterface.arrows_flags = 0;

    while (fighter_gobj != NULL)
    {
        ftDisplayMainProcDisplay(fighter_gobj);
        fighter_gobj = fighter_gobj->link_next;
    }
    gGCCurrentCamera = NULL;
}

/* how many nSYAudioFGMMagnify the run has played so far */
static s32 arrow_fgm_count(void)
{
    s32 n = 0;
    s32 i;

    for (i = 0; i < gHostFGMStartedCount && i < 64; i++)
    {
        if (gHostFGMStartedLog[i] == nSYAudioFGMMagnify)
        {
            n++;
        }
    }
    return n;
}

static GObj *arrow_camera(void)
{
    GObj *gobj = gGCCommonLinks[nGCCommonLinkIDCamera];

    while (gobj != NULL)
    {
        if (gobj->id == nGCCommonKindPlayerArrowsCamera)
        {
            return gobj;
        }
        gobj = gobj->link_next;
    }
    return NULL;
}

/* the two arrow GObjs, told apart by their display proc */
static GObj *arrow_gobj(void (*proc_display)(GObj*))
{
    GObj *gobj = gGCCommonLinks[nGCCommonLinkIDInterface];

    while (gobj != NULL)
    {
        if (gobj->proc_display == proc_display)
        {
            return gobj;
        }
        gobj = gobj->link_next;
    }
    return NULL;
}

/* The GO tic, off a GObj rather than off the hook: nSCBattleGameStatusGo
 * is zero and gSCManagerTransferBattleState starts zeroed, so a hook that
 * ran from tic 0 would call the first tic of the scene GO. This one is
 * made at the end of scVSBattleStartBattle, by which time the status is
 * Wait. */
static void arrows_watch_run(GObj *gobj)
{
    (void)gobj;

    if (gArrowGoTic < 0 &&
        gSCManagerBattleState->game_status == nSCBattleGameStatusGo)
    {
        gArrowGoTic = (s32)dSYTaskmanUpdateCount;
    }
}

static void arrows_watch_start(void)
{
    gcMakeGObjSPAfter(nGCCommonKindInterface, arrows_watch_run,
                      nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);
}

static void arrows_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    s32 go = gArrowGoTic;

    if (go < 0)
    {
        return;
    }

    /* --- what scVSBattleStartBattle built ---------------------------- */
    if (tic == go + 10)
    {
        GObj *left = arrow_gobj(ifCommonPlayerArrowsLeftProcDisplay);
        GObj *right = arrow_gobj(ifCommonPlayerArrowsRightProcDisplay);
        GObj *cam = arrow_camera();
        CObj *cobj;

        CHECK(left != NULL && right != NULL && cam != NULL);
        CHECK(left->dl_link_id == 8 && right->dl_link_id == 8);

        /* ifcommon.c:1789-1802: -134 and +134, the right one turned
         * half a circle so its chevrons march the other way */
        CHECK_EQF(DObjGetStruct(left)->translate.vec.f.x, -134.0f);
        CHECK_EQF(DObjGetStruct(left)->translate.vec.f.y, 0.0f);
        CHECK_EQF(DObjGetStruct(right)->translate.vec.f.x, 134.0f);
        CHECK_EQF(DObjGetStruct(right)->translate.vec.f.z, 0.0f);
        CHECK_EQF(DObjGetStruct(right)->rotate.vec.f.z, F_CST_DTOR32(180.0F));

        /* gmcamera.c:1342-1346: an ortho box the size of the battle
         * viewport, with the eye a thousand units out on +Z. 134 is
         * inside its 150, which is what puts the arrows at the edge of
         * the screen rather than off it. */
        cobj = CObjGetStruct(cam);
        CHECK(cam->camera_mask == COBJ_MASK_DLLINK(8));
        CHECK_EQF(cobj->projection.ortho.l, -150.0f);
        CHECK_EQF(cobj->projection.ortho.r, 150.0f);
        CHECK_EQF(cobj->projection.ortho.b, -110.0f);
        CHECK_EQF(cobj->projection.ortho.t, 110.0f);
        CHECK_EQF(cobj->projection.ortho.n, 100.0f);
        CHECK_EQF(cobj->projection.ortho.f, 12800.0f);
        CHECK_EQF(cobj->vec.eye.z, 1000.0f);
        CHECK_EQF(cobj->vec.at.z, 0.0f);
        CHECK_EQF(cobj->vec.up.y, 1.0f);

        /* ifcommon.c:1808-1818, the whole truth table: sideways wins
         * only when it is the larger of the two, and nothing at all is
         * raised for a fighter that went out the top or the bottom. */
        {
            static const struct { f32 x, y; u8 want; } kFlags[] = {
                {  200.0F,   10.0F, IFCOMMON_PLAYERARROWS_MASK_RIGHT },
                { -200.0F,   10.0F, IFCOMMON_PLAYERARROWS_MASK_LEFT  },
                {  200.0F, -300.0F, 0 },
                { -200.0F,  300.0F, 0 },
                {   10.0F,   10.0F, 0 },
                {    0.0F,    0.0F, 0 },
                {  -10.0F,    9.0F, IFCOMMON_PLAYERARROWS_MASK_LEFT  },
            };
            size_t k;

            for (k = 0; k < ARRAY_COUNT(kFlags); k++)
            {
                gIFCommonPlayerInterface.arrows_flags = 0;
                ifCommonPlayerArrowsUpdateFlags(kFlags[k].x, kFlags[k].y);
                CHECK(gIFCommonPlayerInterface.arrows_flags == kFlags[k].want);
            }
            /* and it only ever ors, so two fighters out opposite sides
             * light both */
            gIFCommonPlayerInterface.arrows_flags = 0;
            ifCommonPlayerArrowsUpdateFlags(200.0F, 0.0F);
            ifCommonPlayerArrowsUpdateFlags(-200.0F, 0.0F);
            CHECK(gIFCommonPlayerInterface.arrows_flags ==
                  (IFCOMMON_PLAYERARROWS_MASK_LEFT |
                   IFCOMMON_PLAYERARROWS_MASK_RIGHT));
            gIFCommonPlayerInterface.arrows_flags = 0;
        }
        gArrowSeen[0] = 1;
        return;
    }

    /* --- the display block: who has left the viewport ---------------- */
    if (tic == go + 12)
    {
        FTStruct *p1 = arrow_fp(0);

        /* well off the right of a shot that is following two fighters
         * near the origin */
        arrow_put(0, 4000.0F, 0.0F);
        arrows_draw_pass();
        CHECK(p1->is_magnify_show != FALSE);
        CHECK(p1->magnify_pos.x > 0.0F);
        CHECK(gIFCommonPlayerInterface.magnify_mode == 1);
        CHECK(gIFCommonPlayerInterface.arrows_flags ==
              IFCOMMON_PLAYERARROWS_MASK_RIGHT);

        arrow_put(0, -4000.0F, 0.0F);
        arrows_draw_pass();
        CHECK(p1->magnify_pos.x < 0.0F);
        CHECK(gIFCommonPlayerInterface.arrows_flags ==
              IFCOMMON_PLAYERARROWS_MASK_LEFT);

        /* straight up is off screen too -- the glass gets it, the
         * arrows do not */
        arrow_put(0, 0.0F, 6000.0F);
        arrows_draw_pass();
        CHECK(p1->is_magnify_show != FALSE);
        CHECK(gIFCommonPlayerInterface.arrows_flags == 0);
        CHECK(gIFCommonPlayerInterface.magnify_mode == 1);

        /* and back where it started, in bounds: nothing */
        arrow_put(0, 0.0F, 0.0F);
        arrows_draw_pass();
        CHECK(p1->is_magnify_show == FALSE);
        CHECK(gIFCommonPlayerInterface.arrows_flags == 0);
        CHECK(gIFCommonPlayerInterface.magnify_mode == 0);

        gArrowSeen[1] = 1;
        return;
    }

    /* --- the run function's three-state machine, and the sound -------
     *
     * Driven by hand rather than over tics, and for a reason worth
     * writing down: the host link runs the *draw* as well as the update
     * (src/dc/taskman.c syTaskmanCommonTaskDraw), so the frame after
     * this one would put is_magnify_show back to where the fighter
     * really is and the flag would not survive to be read. What is
     * under test is ifCommonPlayerArrowsFuncRun, and these are the
     * calls the object system makes to it, one after another. */
    if (tic == go + 20)
    {
        FTStruct *p1 = arrow_fp(0);
        FTStruct *p2 = arrow_fp(1);
        s32 base;
        s32 i;

        CHECK(gIFCommonPlayerInterface.is_magnify_display != FALSE);
        CHECK(gIFCommonPlayerInterface.arrows_left_status == 0);
        CHECK(gIFCommonPlayerInterface.arrows_right_status == 0);

        /* the started log holds 64 and a match has played more than
         * that by now */
        gHostFGMStartedCount = 0;
        base = 0;
        sIFCommonPlayerMagnifySoundWait = 0;

        p1->is_magnify_show = TRUE;
        p1->magnify_pos.x = -100.0F;
        p1->magnify_pos.y = 10.0F;

        /* the tic it comes up: status 1, which is the frame the
         * animation is installed on, and the FGM with its thirty-tic
         * wait started and already counted down one */
        ifCommonPlayerArrowsFuncRun(NULL);
        CHECK(gIFCommonPlayerInterface.arrows_left_status == 1);
        CHECK(gIFCommonPlayerInterface.arrows_right_status == 0);
        CHECK(arrow_fgm_count() == base + 1);
        CHECK(sIFCommonPlayerMagnifySoundWait == 29);

        ifCommonPlayerArrowsFuncRun(NULL);
        CHECK(gIFCommonPlayerInterface.arrows_left_status == 2);
        CHECK(arrow_fgm_count() == base + 1);
        CHECK(sIFCommonPlayerMagnifySoundWait == 28);
        gArrowSeen[2] = 1;

        /* twenty-eight more take the wait to zero and no further */
        for (i = 0; i < 28; i++)
        {
            ifCommonPlayerArrowsFuncRun(NULL);
        }
        CHECK(sIFCommonPlayerMagnifySoundWait == 0);
        CHECK(arrow_fgm_count() == base + 1);

        /* the thirty-first is the second sound */
        ifCommonPlayerArrowsFuncRun(NULL);
        CHECK(arrow_fgm_count() == base + 2);
        CHECK(sIFCommonPlayerMagnifySoundWait == 29);
        gArrowSeen[3] = 1;

        /* is_magnify_ignore is 1P mode's exemption: the fighter is off
         * screen and does not count. The sound stops with it. */
        p1->is_magnify_ignore = TRUE;
        ifCommonPlayerArrowsFuncRun(NULL);
        CHECK(gIFCommonPlayerInterface.arrows_left_status == 0);
        CHECK(sIFCommonPlayerMagnifySoundWait == 0);
        p1->is_magnify_ignore = FALSE;

        /* is_rebirth, the respawn platform, is the other one */
        p1->is_rebirth = TRUE;
        ifCommonPlayerArrowsFuncRun(NULL);
        CHECK(gIFCommonPlayerInterface.arrows_left_status == 0);
        p1->is_rebirth = FALSE;

        /* both fighters out, one each way */
        p2->is_magnify_show = TRUE;
        p2->magnify_pos.x = 120.0F;
        p2->magnify_pos.y = -30.0F;

        ifCommonPlayerArrowsFuncRun(NULL);
        CHECK(gIFCommonPlayerInterface.arrows_left_status == 1);
        CHECK(gIFCommonPlayerInterface.arrows_right_status == 1);
        ifCommonPlayerArrowsFuncRun(NULL);
        CHECK(gIFCommonPlayerInterface.arrows_left_status == 2);
        CHECK(gIFCommonPlayerInterface.arrows_right_status == 2);
        gArrowSeen[4] = 1;

        /* out the top is off screen for the glass and not for the
         * arrows: |x| > |y| is the whole test either side of it */
        p1->magnify_pos.x = 10.0F;
        p1->magnify_pos.y = 500.0F;
        p2->is_magnify_show = FALSE;
        ifCommonPlayerArrowsFuncRun(NULL);
        CHECK(gIFCommonPlayerInterface.arrows_left_status == 0);
        CHECK(gIFCommonPlayerInterface.arrows_right_status == 0);
        CHECK(sIFCommonPlayerMagnifySoundWait == 0);

        /* and the statuses are not the flags: arrows_flags is the
         * draw's own answer, lowered by gmCameraDefaultProcDisplay every
         * frame, and nothing above touched it */
        CHECK(gIFCommonPlayerInterface.arrows_flags == 0);

        p1->is_magnify_show = FALSE;
        gArrowSeen[5] = 1;
        syTaskmanSetLoadScene();
        return;
    }
    if (dSYTaskmanUpdateCount > SCENE_TIC_LIMIT)
    {
        gSceneGaveUp = 1;
        syTaskmanSetLoadScene();
    }
}

static void test_offscreen_arrows(void)
{
    int player;
    size_t k;

    sprite_bank_release_all();
    load_stage();
    mock_model.attr = mock_attr;
    mock_anim_len = 12.0f;

    memset(&gSCManagerTransferBattleState, 0,
           sizeof(gSCManagerTransferBattleState));
    gSCManagerTransferBattleState.game_rules = SCBATTLE_GAMERULE_STOCK;
    gSCManagerTransferBattleState.stocks = 2;
    gSCManagerTransferBattleState.pl_count = 2;
    gSCManagerTransferBattleState.damage_ratio = 100;
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        SCPlayerData *pd = &gSCManagerTransferBattleState.players[player];

        pd->pkind = (player < 2) ? nFTPlayerKindMan : nFTPlayerKindNot;
        pd->fkind = nFTKindMario;
        pd->team = pd->player = player;
        pd->tag = player;
        pd->color = player;
        pd->shade = 1;
        pd->handicap = 5;
        pd->stock_count = gSCManagerTransferBattleState.stocks;
    }

    gArrowGoTic = -1;
    gSceneGaveUp = 0;
    memset(gArrowSeen, 0, sizeof(gArrowSeen));
    gSCVSBattleFuncDebug = arrows_watch_start;
    gSceneTicHook = arrows_tic;

    gSCManagerSceneData.is_reset = FALSE;
    gSCManagerSceneData.scene_curr = nSCKindVSBattle;
    crowd_reset();
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_FIGHTING);
    syDmaLoadOverlay(OVERLAY_VSBATTLE);
    scVSBattleStartScene();

    gSceneTicHook = NULL;
    gSCVSBattleFuncDebug = NULL;

    CHECK(!gSceneGaveUp);
    CHECK(gArrowGoTic == 391);
    for (k = 0; k < 6; k++)
    {
        CHECK(gArrowSeen[k]);
    }
    /* the scene ended on the hook's own syTaskmanSetLoadScene, so
     * nobody was placed and nobody lost a stock */
    CHECK(gSCManagerTransferBattleState.players[0].stock_count == 2);
    CHECK(gSCManagerTransferBattleState.players[1].stock_count == 2);
}

/* ---- the magnifying glasses -----------------------------
 *
 * Three pieces, and the host link reaches two of them whole: the
 * geometry of the glass -- where it goes, how big, where its handle
 * points -- is all arithmetic in if/ifcommon.c and gm/gmcamera.c, and
 * the mask that rounds the fighter off is sixteen planes in
 * src/dc/clip.c, pure C. What the host cannot see is the pixels.
 *
 * The camera's pass is driven the way the arrows test drives the main
 * camera's: set gGCCurrentCamera and call the proc, which captures the
 * fighters' own display procs -- the magnify arms of
 * src/dc/ftdisplaymain.c run for real, down to the DObj placement of
 * the handle, and only the draw under them is a stub.
 */
/* src/dc/ifcommon.c: the glasses' own */
extern IFPlayerMagnify sIFCommonPlayerMagnifyInterface[GMCOMMON_PLAYERS_MAX];

static s32 gMagnifySeen[4];

static GObj *magnify_camera(void)
{
    GObj *gobj = gGCCommonLinks[nGCCommonLinkIDCamera];

    while (gobj != NULL)
    {
        if (gobj->id == nGCCommonKindPlayerMagnifyCamera)
        {
            return gobj;
        }
        gobj = gobj->link_next;
    }
    return NULL;
}

static void clip_put(ClipVtx *v, float x, float y)
{
    v->cx = x;
    v->cy = y;
    v->cz = 0.0f;
    v->cw = 1.0f;
    v->u = x;
    v->v = y;
    v->argb = 0xFFFFFFFF;
}

/* src/dc/clip.c on its own: the sixteen planes, and the near clip
 * moved there unchanged */
static void test_clip_planes(void)
{
    ClipVtx poly[CLIP_MAX_VERTS];
    ClipVtx scratch[CLIP_MAX_VERTS];
    ClipVtx tri[3];
    ClipVtx four[4];
    int n, i;

    /* a small triangle at the centre comes back as it went */
    clip_put(&poly[0], 0.1f, 0.1f);
    clip_put(&poly[1], -0.2f, 0.1f);
    clip_put(&poly[2], 0.0f, -0.2f);
    n = clip_planes(poly, 3, CLIP_MAGNIFY_PLANES, CLIP_MAGNIFY_NPLANES,
                    CLIP_MAGNIFY_INRADIUS, scratch);
    CHECK(n == 3);
    CHECK_EQF(poly[1].cx, -0.2f);
    CHECK_EQF(poly[2].cy, -0.2f);
    CHECK_EQF(poly[0].u, 0.1f);

    /* a triangle around the whole glass comes back as the glass: every
     * vertex on the ring between the inradius and the circumradius of
     * the 16-gon, which is inside the mask's soft edge (clip.h) */
    clip_put(&poly[0], 3.0f, 0.0f);
    clip_put(&poly[1], -1.5f, 2.598f);
    clip_put(&poly[2], -1.5f, -2.598f);
    n = clip_planes(poly, 3, CLIP_MAGNIFY_PLANES, CLIP_MAGNIFY_NPLANES,
                    CLIP_MAGNIFY_INRADIUS, scratch);
    CHECK(n >= 16 && n <= CLIP_MAX_VERTS);
    for (i = 0; i < n; i++)
    {
        float r = sqrtf(poly[i].cx * poly[i].cx + poly[i].cy * poly[i].cy);

        CHECK(r >= CLIP_MAGNIFY_INRADIUS - 1e-4f && r <= 1.0f + 1e-4f);
        CHECK_EQF(poly[i].cw, 1.0f);
    }

    /* a triangle off to one side, outside one plane whole: nothing */
    clip_put(&poly[0], 2.0f, 2.0f);
    clip_put(&poly[1], 3.0f, 2.0f);
    clip_put(&poly[2], 2.0f, 3.0f);
    n = clip_planes(poly, 3, CLIP_MAGNIFY_PLANES, CLIP_MAGNIFY_NPLANES,
                    CLIP_MAGNIFY_INRADIUS, scratch);
    CHECK(n == 0);

    /* the same with the fast path off: the planes alone say the same */
    clip_put(&poly[0], 0.1f, 0.1f);
    clip_put(&poly[1], -0.2f, 0.1f);
    clip_put(&poly[2], 0.0f, -0.2f);
    n = clip_planes(poly, 3, CLIP_MAGNIFY_PLANES, CLIP_MAGNIFY_NPLANES,
                    0.0f, scratch);
    CHECK(n == 3);
    CHECK_EQF(poly[1].cx, -0.2f);

    /* one edge crossing: three in, one plane cuts one corner off */
    clip_put(&poly[0], 0.0f, 0.0f);
    clip_put(&poly[1], 0.5f, 0.0f);
    clip_put(&poly[2], 0.0f, 1.5f);
    n = clip_planes(poly, 3, CLIP_MAGNIFY_PLANES, CLIP_MAGNIFY_NPLANES,
                    CLIP_MAGNIFY_INRADIUS, scratch);
    CHECK(n >= 4);
    for (i = 0; i < n; i++)
    {
        CHECK(poly[i].cy <= 1.0f + 1e-4f);
    }

    /* the near clip: a vertex behind the eye splits its edges, and the
     * clip position is the plane cz + cw = 0 */
    clip_put(&tri[0], 0.0f, 0.0f);
    clip_put(&tri[1], 1.0f, 0.0f);
    clip_put(&tri[2], 0.0f, 1.0f);
    tri[2].cz = -3.0f;
    n = clip_near(tri, four);
    CHECK(n == 4);
    for (i = 0; i < n; i++)
    {
        CHECK(four[i].cz + four[i].cw >= -1e-5f);
    }
    clip_put(&tri[0], 0.0f, 0.0f);
    tri[0].cz = tri[1].cz = tri[2].cz = -5.0f;
    CHECK(clip_near(tri, four) == 0);
}

static void magnify_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    s32 go = gArrowGoTic;

    if (go < 0)
    {
        return;
    }

    /* --- what scVSBattleStartBattle built ---------------------------- */
    if (tic == go + 10)
    {
        GObj *cam = magnify_camera();
        GObj *gobj;
        CObj *cobj;
        s32 n = 0;

        CHECK(cam != NULL);
        cobj = CObjGetStruct(cam);

        /* gmcamera.c:1377-1404: DL link 9, the fighters', and the two
         * XObjs of the game's own kinds over the object system's
         * default ortho box and eye (objman.c:89-92) */
        CHECK(cam->camera_mask == COBJ_MASK_DLLINK(9));
        CHECK(cobj->flags & COBJ_FLAG_DLBUFFERS);
        CHECK(cobj->xobjs_num == 2);
        CHECK(cobj->xobjs[0] != NULL && cobj->xobjs[0]->kind == 0x4D);
        CHECK(cobj->xobjs[1] != NULL && cobj->xobjs[1]->kind == 0x4E);
        CHECK_EQF(cobj->projection.ortho.l, -160.0f);
        CHECK_EQF(cobj->projection.ortho.r, 160.0f);
        CHECK_EQF(cobj->projection.ortho.b, -120.0f);
        CHECK_EQF(cobj->projection.ortho.t, 120.0f);
        CHECK_EQF(cobj->vec.eye.z, 1500.0f);
        CHECK_EQF(cobj->vec.up.y, 1.0f);

        /* ifcommon.c:1612-1629: one glass per fighter on link 12, in
         * the player's colour; and the display switch is up, because
         * the GO! raised it */
        for (gobj = gGCCommonLinks[nGCCommonLinkIDMagnify]; gobj != NULL;
             gobj = gobj->link_next)
        {
            n++;
        }
        CHECK(n == 2);
        CHECK(sIFCommonPlayerMagnifyInterface[0].interface_gobj != NULL);
        CHECK(sIFCommonPlayerMagnifyInterface[1].interface_gobj != NULL);
        CHECK(sIFCommonPlayerMagnifyInterface[0].interface_gobj->link_id == nGCCommonLinkIDMagnify);
        CHECK(sIFCommonPlayerMagnifyInterface[0].color_id == 0);
        CHECK(sIFCommonPlayerMagnifyInterface[1].color_id == 1);
        CHECK(gIFCommonPlayerInterface.is_magnify_display != FALSE);
        CHECK(gIFCommonPlayerInterface.magnify_mode == 0);

        /* ifcommon.c:1312-1394, ifCommonPlayerMagnifyGetPosition over
         * the battle's 300x220 at scale 1: an inset box of left -125,
         * right 125, up 90, down -90, and the glass on the ray from
         * the centre toward the fighter where it meets that box */
        {
            static const struct { f32 px, py, x, y; } kPos[] = {
                {     0.0F,  500.0F,    0.0F,  90.0F },
                {     0.0F, -500.0F,    0.0F, -90.0F },
                {   500.0F,    0.0F,  125.0F,   0.0F },
                {  1000.0F, 1000.0F,   90.0F,  90.0F },
                { -4000.0F,  100.0F, -125.0F,   3.125F },
                {   200.0F,  150.0F,  120.0F,  90.0F },
                {  -200.0F, -150.0F, -120.0F, -90.0F },
            };
            size_t k;

            gIFCommonPlayerInterface.magnify_scale = 1.0F;
            for (k = 0; k < ARRAY_COUNT(kPos); k++)
            {
                Vec2f pos;

                ifCommonPlayerMagnifyGetPosition(kPos[k].px, kPos[k].py, &pos);
                CHECK_EQF(pos.x, kPos[k].x);
                CHECK_EQF(pos.y, kPos[k].y);
            }
        }
        gMagnifySeen[0] = 1;
        return;
    }

    /* --- the camera's pass ------------------------------------------- */
    if (tic == go + 12)
    {
        FTStruct *p1 = arrow_fp(0);
        GObj *cam = magnify_camera();
        CObj *main_cobj = CObjGetStruct(gGMCameraGObj);
        IFPlayerMagnify *ifmag = &sIFCommonPlayerMagnifyInterface[0];
        DObj *handle = DObjGetStruct(ifmag->interface_gobj);
        Vec3f eye_saved;
        f32 dist_x, dist_y, dist_z, dist, want, scale;
        s32 half;

        /* the main camera's pass: P1 well off the right, the glass
         * asked for */
        arrow_put(0, 4000.0F, 0.0F);
        arrows_draw_pass();
        CHECK(p1->is_magnify_show != FALSE);
        CHECK(gIFCommonPlayerInterface.magnify_mode == 1);

        /* the magnify camera's, as gcDrawAll runs it */
        gGCCurrentCamera = cam;
        gmCameraPlayerMagnifyProcDisplay(cam);
        gGCCurrentCamera = NULL;
        CHECK(gIFCommonPlayerInterface.magnify_mode == 2);

        /* gmcamera.c:1292-1338 by hand: 600 units above the look-at,
         * seen from the battle camera's distance under its field of
         * view, in half-viewport heights, over eighteen */
        dist_x = main_cobj->vec.eye.x - main_cobj->vec.at.x;
        dist_y = main_cobj->vec.eye.y - main_cobj->vec.at.y;
        dist_z = main_cobj->vec.eye.z - main_cobj->vec.at.z;
        dist = sqrtf(dist_x * dist_x + dist_y * dist_y + dist_z * dist_z);
        want = (600.0F / (dist * tanf(F_CLC_DTOR32(main_cobj->projection.persp.fovy * 0.5F))))
               * (f32)(gGMCameraStruct.viewport_height / 2) / 18.0F;
        scale = gIFCommonPlayerInterface.magnify_scale;
        CHECK(scale > 0.0F && scale <= 3.0F);
        if (want > 3.0F)
        {
            want = 3.0F;
        }
        CHECK(fabsf(scale - want) < 1e-3F * want);

        /* the glass is on the right edge, inset by its own size, and
         * the fighter is drawn through an 18*scale square centred on it
         * (ifcommon.c:1538-1543, syRdpSetViewport's 10.2 units) */
        CHECK_EQF(ifmag->pos.x, 150.0F - 20.0F * scale - 5.0F);
        half = (s32)(9.0F * scale * 4.0F);
        CHECK(abs((s32)ifmag->viewport.vp.vscale[0] - half) <= 1);
        CHECK(abs((s32)ifmag->viewport.vp.vscale[1] - half) <= 1);
        CHECK(abs((s32)ifmag->viewport.vp.vtrans[0] -
                  (s32)((ifmag->pos.x + gGMCameraStruct.viewport_center_x) * 4.0F)) <= 1);
        CHECK(abs((s32)ifmag->viewport.vp.vtrans[1] -
                  (s32)((gGMCameraStruct.viewport_center_y - ifmag->pos.y) * 4.0F)) <= 1);

        /* ifcommon.c:1589-1595, the handle: at the glass, turned toward
         * the fighter, half the glass's scale */
        CHECK_EQF(handle->translate.vec.f.x, ifmag->pos.x);
        CHECK_EQF(handle->translate.vec.f.y, ifmag->pos.y);
        CHECK_EQF(handle->rotate.vec.f.z,
                  syUtilsArcTan2(p1->magnify_pos.y, p1->magnify_pos.x) - F_CST_DTOR32(90.0F));
        CHECK_EQF(handle->scale.vec.f.x, scale * 0.5F);
        CHECK_EQF(handle->scale.vec.f.y, scale * 0.5F);
        gMagnifySeen[1] = 1;

        /* the formula uncapped: the battle camera pulled back to where
         * 900 units are under three glasses tall -- 6000 out, under a
         * 38 degree lens, is 1.78 -- and the same pass again */
        eye_saved = main_cobj->vec.eye;
        main_cobj->vec.eye.x = main_cobj->vec.at.x;
        main_cobj->vec.eye.y = main_cobj->vec.at.y;
        main_cobj->vec.eye.z = main_cobj->vec.at.z + 6000.0F;
        want = (600.0F / (6000.0F * tanf(F_CLC_DTOR32(main_cobj->projection.persp.fovy * 0.5F))))
               * (f32)(gGMCameraStruct.viewport_height / 2) / 18.0F;
        CHECK(want > 1.7F && want < 1.9F);
        gIFCommonPlayerInterface.magnify_mode = 1;
        gGCCurrentCamera = cam;
        gmCameraPlayerMagnifyProcDisplay(cam);
        gGCCurrentCamera = NULL;
        scale = gIFCommonPlayerInterface.magnify_scale;
        CHECK(fabsf(scale - want) < 1e-3F * want);
        /* and the handle and the square followed it */
        CHECK_EQF(handle->scale.vec.f.x, scale * 0.5F);
        half = (s32)(9.0F * scale * 4.0F);
        CHECK(abs((s32)ifmag->viewport.vp.vscale[0] - half) <= 1);

        /* the cap: pull the battle camera in and the glass stops
         * growing at three */
        main_cobj->vec.eye.x = main_cobj->vec.at.x;
        main_cobj->vec.eye.y = main_cobj->vec.at.y;
        main_cobj->vec.eye.z = main_cobj->vec.at.z + 200.0F;
        gIFCommonPlayerInterface.magnify_mode = 1;
        gGCCurrentCamera = cam;
        gmCameraPlayerMagnifyProcDisplay(cam);
        gGCCurrentCamera = NULL;
        CHECK_EQF(gIFCommonPlayerInterface.magnify_scale, 3.0F);
        CHECK_EQF(handle->scale.vec.f.x, 1.5F);
        main_cobj->vec.eye = eye_saved;
        gMagnifySeen[2] = 1;

        /* home again: the main camera lowers the mode and the magnify
         * camera's pass does nothing at all */
        arrow_put(0, 0.0F, 0.0F);
        arrows_draw_pass();
        CHECK(gIFCommonPlayerInterface.magnify_mode == 0);
        handle->translate.vec.f.x = -999.0F;
        gGCCurrentCamera = cam;
        gmCameraPlayerMagnifyProcDisplay(cam);
        gGCCurrentCamera = NULL;
        CHECK_EQF(handle->translate.vec.f.x, -999.0F);
        gMagnifySeen[3] = 1;

        syTaskmanSetLoadScene();
        return;
    }
    if (dSYTaskmanUpdateCount > SCENE_TIC_LIMIT)
    {
        gSceneGaveUp = 1;
        syTaskmanSetLoadScene();
    }
}

static void test_magnify_glass(void)
{
    int player;
    size_t k;

    test_clip_planes();

    sprite_bank_release_all();
    load_stage();
    mock_model.attr = mock_attr;
    mock_anim_len = 12.0f;

    memset(&gSCManagerTransferBattleState, 0,
           sizeof(gSCManagerTransferBattleState));
    gSCManagerTransferBattleState.game_rules = SCBATTLE_GAMERULE_STOCK;
    gSCManagerTransferBattleState.stocks = 2;
    gSCManagerTransferBattleState.pl_count = 2;
    gSCManagerTransferBattleState.damage_ratio = 100;
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        SCPlayerData *pd = &gSCManagerTransferBattleState.players[player];

        pd->pkind = (player < 2) ? nFTPlayerKindMan : nFTPlayerKindNot;
        pd->fkind = nFTKindMario;
        pd->team = pd->player = player;
        pd->tag = player;
        pd->color = player;
        pd->shade = 1;
        pd->handicap = 5;
        pd->stock_count = gSCManagerTransferBattleState.stocks;
    }

    gArrowGoTic = -1;
    gSceneGaveUp = 0;
    memset(gMagnifySeen, 0, sizeof(gMagnifySeen));
    gSCVSBattleFuncDebug = arrows_watch_start;
    gSceneTicHook = magnify_tic;

    gSCManagerSceneData.is_reset = FALSE;
    gSCManagerSceneData.scene_curr = nSCKindVSBattle;
    crowd_reset();
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_FIGHTING);
    syDmaLoadOverlay(OVERLAY_VSBATTLE);
    scVSBattleStartScene();

    gSceneTicHook = NULL;
    gSCVSBattleFuncDebug = NULL;

    CHECK(!gSceneGaveUp);
    CHECK(gArrowGoTic == 391);
    for (k = 0; k < 4; k++)
    {
        CHECK(gMagnifySeen[k]);
    }
}

/* ---- three fighters, and the jab between two of them -----
 *
 * With three or more fighters in a battle the
 * machine rebooted the frame one of them attacked. This test
 * pins the cause. The battle scene the target runs, with three players instead of
 * two: one driven through a jab's motion script at a second standing in
 * its reach, and a third standing clear of both.
 *
 * What it asserts is the shape of the hit pass with three fighters on
 * the link, which is the only thing that changes between the two-player
 * battle every other scene test runs and this one: every raised hitbox
 * on every fighter hangs off a joint (a raised hitbox with no joint
 * is the failure to look for), the victim takes the damage the two-player test measures, and
 * the bystander -- who is in the same pass, walked by the same link --
 * takes none and appears in no attack record.
 */
#define THREE_PLAYERS 3

static int gThreeGoTic;
static int gThreeSeen[5];
static int gThreeGaveUp;

static FTStruct *three_fp(int player)
{
    GObj *gobj = gSCManagerBattleState->players[player].fighter_gobj;

    return (gobj != NULL) ? ftGetStruct(gobj) : NULL;
}

/* Every hitbox that is up, on every fighter, has a joint to be placed
 * on. ftMainProcPhysicsMap walks exactly these to move them with the
 * animation, and the port's own NULL guard there (ledger row 10) is the
 * only reason a hitbox without one does not walk a null pointer. */
static void three_check_attack_colls(void)
{
    int player, i;

    for (player = 0; player < THREE_PLAYERS; player++)
    {
        FTStruct *mfp = three_fp(player);

        CHECK(mfp != NULL);

        if (mfp == NULL)
        {
            continue;
        }
        for (i = 0; i < (int)ARRAY_COUNT(mfp->attack_colls); i++)
        {
            FTAttackColl *ac = &mfp->attack_colls[i];

            if (ac->attack_state == nGMAttackStateOff)
            {
                continue;
            }
            CHECK(ac->joint != NULL);
        }
    }
}

static void three_tic(void)
{
    s32 tic = (s32)dSYTaskmanUpdateCount;
    FTStruct *p1, *p2, *p3;

    if (gThreeGoTic < 0)
    {
        if (gSCManagerBattleState->game_status == nSCBattleGameStatusGo)
        {
            gThreeGoTic = tic;
        }
        if (tic > SCENE_TIC_LIMIT)
        {
            gThreeGaveUp = 1;
            syTaskmanSetLoadScene();
        }
        return;
    }
    p1 = three_fp(0);
    p2 = three_fp(1);
    p3 = three_fp(2);

    if (p1 == NULL || p2 == NULL || p3 == NULL)
    {
        CHECK(0);
        syTaskmanSetLoadScene();
        return;
    }
    switch (tic - gThreeGoTic)
    {
    case 1:
        /* three of them, standing, each one its own player: the state
         * every other scene test has two of */
        CHECK(p1->status_id == nFTCommonStatusWait);
        CHECK(p2->status_id == nFTCommonStatusWait);
        CHECK(p3->status_id == nFTCommonStatusWait);
        CHECK(p1->player == 0 && p2->player == 1 && p3->player == 2);
        {
            GObj *gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
            int n = 0;

            while (gobj != NULL)
            {
                n++;
                gobj = gobj->link_next;
            }
            CHECK(n == THREE_PLAYERS);
        }
        gThreeSeen[0] = 1;
        break;

    case 2:
        /* the victim into the attacker's reach, the bystander well out
         * of it -- both through the port's own safe teleport, which
         * reacquires the collision line (test_hit_lands on why) */
        ftMainRespawn(p2->fighter_gobj,
                      DObjGetStruct(p1->fighter_gobj)->translate.vec.f.x +
                          100.0F * p1->lr,
                      DObjGetStruct(p1->fighter_gobj)->translate.vec.f.y);
        ftMainRespawn(p3->fighter_gobj,
                      DObjGetStruct(p1->fighter_gobj)->translate.vec.f.x +
                          1200.0F * p1->lr,
                      DObjGetStruct(p1->fighter_gobj)->translate.vec.f.y);
        break;

    case 4:
        /* the pad, as the input layer would have latched it: A tapped
         * and held, which is what starts Attack11 out of Wait */
        gSYControllerDevices[0].button_tap = A_BUTTON;
        gSYControllerDevices[0].button_hold = A_BUTTON;
        break;

    case 5:
        gSYControllerDevices[0].button_tap = 0;
        gSYControllerDevices[0].button_hold = 0;
        CHECK(p1->status_id == nFTCommonStatusAttack11);
        CHECK(p2->percent_damage == 0);
        gThreeSeen[1] = 1;
        break;

    default:
        break;
    }
    if (tic - gThreeGoTic >= 5)
    {
        three_check_attack_colls();
    }
    /* the jab's hitbox is raised, placed, searched and paid out inside
     * the four tics after the status change (test_hit_lands) */
    if (!gThreeSeen[2] && tic - gThreeGoTic > 5 && p2->percent_damage != 0)
    {
        CHECK(p2->percent_damage == 14);
        CHECK(p2->damage_player == 0);
        CHECK(p1->attack_colls[1].attack_records[0].victim_gobj ==
              p2->fighter_gobj);
        /* the third fighter was in the pass and was not hit by it: no
         * damage, no record, and no hitstun */
        CHECK(p3->percent_damage == 0);
        CHECK(!p3->is_hitstun);
        CHECK(p1->attack_colls[1].attack_records[0].victim_gobj !=
              p3->fighter_gobj);
        gThreeSeen[2] = 1;
    }
    if (tic - gThreeGoTic > 60)
    {
        /* the jab ran, landed, and ended with all three still in the
         * scene it ran in */
        CHECK(gThreeSeen[2]);
        CHECK(p1->status_id == nFTCommonStatusWait);
        gThreeSeen[3] = 1;
        syTaskmanSetLoadScene();
    }
}

static void test_three_fighters_jab(void)
{
    int player;
    size_t k;

    sprite_bank_release_all();
    load_stage();
    mock_model.attr = mock_attr;
    mock_anim_len = 12.0f;

    memset(&gSCManagerTransferBattleState, 0,
           sizeof(gSCManagerTransferBattleState));
    gSCManagerTransferBattleState.game_rules = SCBATTLE_GAMERULE_STOCK;
    gSCManagerTransferBattleState.stocks = 2;
    gSCManagerTransferBattleState.pl_count = THREE_PLAYERS;
    gSCManagerTransferBattleState.damage_ratio = 100;
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        SCPlayerData *pd = &gSCManagerTransferBattleState.players[player];

        pd->pkind = (player < THREE_PLAYERS) ? nFTPlayerKindMan
                                             : nFTPlayerKindNot;
        pd->fkind = nFTKindMario;
        pd->team = pd->player = player;
        pd->tag = player;
        pd->color = player;
        pd->shade = 1;
        pd->handicap = 5;
        pd->stock_count = gSCManagerTransferBattleState.stocks;
    }

    gThreeGoTic = -1;
    gThreeGaveUp = 0;
    memset(gThreeSeen, 0, sizeof(gThreeSeen));
    gSceneTicHook = three_tic;

    gSCManagerSceneData.is_reset = FALSE;
    gSCManagerSceneData.scene_curr = nSCKindVSBattle;
    crowd_reset();
    syDmaLoadOverlay(OVERLAY_BATTLE);
    syDmaLoadOverlay(OVERLAY_FIGHTING);
    syDmaLoadOverlay(OVERLAY_VSBATTLE);
    scVSBattleStartScene();

    gSceneTicHook = NULL;

    CHECK(!gThreeGaveUp);
    CHECK(gThreeGoTic == 391);
    for (k = 0; k < 4; k++)
    {
        CHECK(gThreeSeen[k]);
    }
}
