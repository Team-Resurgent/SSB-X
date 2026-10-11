/* hosttest/computer.c -- part of hosttest_ft.c: the CPU player, src/dc/ftcomputer.c.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* the CPU dispatch skeleton (src/dc/ftcomputer.c), on the
 * same nFTPlayerKindCom a real VS CP slot spawns. Originally not a
 * moveset test at all -- it proved
 * only the drafted file's own dispatch plumbing: ftComputerSetupAll runs
 * at spawn (the ftManagerMakeFighter wiring, src/dc/ftmanager.c), the
 * dispatch cascade lands objective/behavior/trait where the decomp's
 * ProcessBehavior/ProcessTrait put them, and ftMainProcUpdateInterrupt's
 * Com case (src/dc/ftcommon.c) drives it every frame without fault. Since
 * landed the real ftComputerFollowObjectiveAttack, the second
 * half of this test now exercises real (if opponent-less) CPU behavior
 * too -- see the comment at the 180-frame loop below. */
static void test_computer_dispatch(void)
{
    FTDesc desc = dFTManagerDefaultFighterDesc;
    s32 i;

    mock_model.attr = mock_attr;
    mock_anim_len = 1e9f;
    mock_transn[0] = mock_transn[1] = mock_transn[2] = 0.0f;
    mock_transn_rot_z = 0.0f;

    if (mock_gobj != NULL)
    {
        ftManagerDestroyFighter(mock_gobj);
    }
    if (mock_gobj2 != NULL)
    {
        ftManagerDestroyFighter(mock_gobj2);
        mock_gobj2 = NULL;
        fpp2 = NULL;
    }
    desc.fkind = nFTKindMario;
    desc.pos.x = 0.0f;
    desc.pos.y = 0.0f;
    desc.pos.z = 0.0f;
    desc.lr = 1;
    desc.team = 0;
    desc.player = 0;
    desc.detail = nFTPartsDetailHigh;
    desc.shade = 1;
    desc.stock_count = mock_battle.stocks;
    desc.damage = 0;
    desc.handicap = mock_battle.players[0].handicap;
    desc.pkind = nFTPlayerKindCom;
    desc.level = 5;
    desc.controller = &mock_in;
    desc.is_skip_entry = TRUE;
    desc.figatree_heap = ftManagerAllocFigatreeHeapKind(nFTKindMario);
    memset(&mock_in, 0, sizeof(mock_in));

    mock_gobj = ftManagerMakeFighter(&desc);
    ftParamInitPlayerBattleStats(0, mock_gobj);
    fpp = ftGetStruct(mock_gobj);
    ftParamUnlockPlayerControl(mock_gobj);
    ft_clear_knockback(fpp);

    /* ftManagerMakeFighter's nFTPlayerKindCom arm (ftmanager.c ~1275, the
     * wiring) has already run ftComputerSetupAll by the time
     * this returns -- before any frame, dispatch is still parked at its
     * setup values (ftcomputer.c:7849-7852). */
    CHECK(fp.computer.proc_com == NULL);
    CHECK(fp.computer.trait == nFTComputerTraitDefault);
    CHECK(fp.computer.behavior == nFTComputerBehaviorDefault);
    /* level 5: 1440 - 5*240 = 240 (REGION_US), so trait's reroll is
     * still gated off on the first frame below -- see the frame-1 check. */
    CHECK(fp.computer.behavior_change_wait == 240);

    /* the spatial memory ftComputerSetupAll reads off the spawn point
     * and the mock stage's floor line (hosttest_ft.c's kVpos, a cliff
     * pair around x = +/-2000) */
    CHECK_EQF(fp.computer.origin_pos.x, 0.0f);
    CHECK_EQF(fp.computer.origin_pos.y, 0.0f);
    CHECK_EQF(fp.computer.target_pos.x, 0.0f);
    CHECK(fp.computer.floor_line_id == fp.coll_data.floor_line_id);
    CHECK(fp.computer.dash_predict > 0.0f);
    CHECK(fp.computer.cliff_left_pos.x < 0.0f);
    CHECK(fp.computer.cliff_right_pos.x > 0.0f);

    /* one frame: ftMainProcUpdateInterrupt's Com case runs
     * ftComputerProcessAll regardless of mock_in, so frame(0,0,0,0,0)
     * drives the AI the same way idle_frames does below. Trait's reroll
     * stays gated off (behavior_change_wait was 240 above), so behavior
     * holds at Default -> ProcDefault -> objective_base
     * Attack -> the now-real FollowObjectiveAttack, which
     * (no opponent on the stage) reaches func_ovl3_8013837C's temp_v0 == 0
     * arm and calls func_ovl3_8013877C -- whose own idle counter
     * (walk_stop_wait) only acts once it passes 30, so on this first tick
     * it just increments the counter and leaves the stick centered
     * the idle counter only increments. */
    frame(0, 0, 0, 0, 0);
    CHECK(fp.computer.behavior == nFTComputerBehaviorDefault);
    CHECK((void *)fp.computer.proc_com == (void *)ftComputerProcDefault);
    CHECK(fp.computer.objective == nFTComputerObjectiveAttack);
    CHECK(fp.computer.input_wait == 0);
    CHECK(fp.computer.p_command == NULL);
    CHECK_EQF(fp.input.pl.stick_range.x, 0.0f);
    CHECK_EQF(fp.input.pl.stick_range.y, 0.0f);
    CHECK_STATUS(nFTCommonStatusWait);

    /* "runs every frame without fault": three more seconds of dispatch
     * with no opponent on the stage still lands on objective Attack every
     * tic (level 5's behavior_change_wait, 240 at spawn, only reaches 0 --
     * and trait's reroll -- past frame 240), but is no longer frozen in
     * place: once func_ovl3_8013877C's own idle
     * counter passes 30, the real decomp
     * behavior is to pick a random point along the current floor line and
     * walk there -- the CPU's real idle-patrol wander.
     * What stays invariant regardless of the RNG draw: dispatch keeps
     * landing on Attack every tic, the fighter never leaves the mock
     * stage's own floor line (hosttest_ft.c's kVpos: the deck runs
     * -2000..2000, widened here for the walk's own turnaround slack), and
     * it never leaves the ground doing it. */
    for (i = 0; i < 180; i++)
    {
        frame(0, 0, 0, 0, 0);
        CHECK(fp.computer.behavior == nFTComputerBehaviorDefault);
        CHECK(fp.computer.objective == nFTComputerObjectiveAttack);
        CHECK(fp.ga == nMPKineticsGround);
        CHECK(DObjGetStruct(mock_gobj)->translate.vec.f.x >= -2200.0f);
        CHECK(DObjGetStruct(mock_gobj)->translate.vec.f.x <= 2200.0f);
    }
    CHECK_EQF(DObjGetStruct(mock_gobj)->translate.vec.f.y, 0.0f);

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* ftComputerFollowObjectiveWalk itself (ftcomputer.c, ported
 * for real above) -- called directly with com->target_pos forced, the
 * way the rest of the decision tail is unit-tested, since dispatch does
 * not reliably reach Walk on its own. Confirms the ground approach path: on flat ground with
 * nothing between the fighter and a point straight to one side at the
 * same height, the byte-code interpreter (ftComputerUpdateInputs,
 * verbatim) turns FollowObjectiveWalk's MoveAuto command
 * into a stick pushed toward the target with no vertical component
 * (dist_y is exactly zero at equal height). */
static void test_computer_follow_objective_walk(void)
{
    FTDesc desc = dFTManagerDefaultFighterDesc;

    mock_model.attr = mock_attr;
    mock_anim_len = 1e9f;
    mock_transn[0] = mock_transn[1] = mock_transn[2] = 0.0f;
    mock_transn_rot_z = 0.0f;

    if (mock_gobj != NULL)
    {
        ftManagerDestroyFighter(mock_gobj);
    }
    if (mock_gobj2 != NULL)
    {
        ftManagerDestroyFighter(mock_gobj2);
        mock_gobj2 = NULL;
        fpp2 = NULL;
    }
    desc.fkind = nFTKindMario;
    desc.pos.x = 0.0f;
    desc.pos.y = 0.0f;
    desc.pos.z = 0.0f;
    desc.lr = 1;
    desc.team = 0;
    desc.player = 0;
    desc.detail = nFTPartsDetailHigh;
    desc.shade = 1;
    desc.stock_count = mock_battle.stocks;
    desc.damage = 0;
    desc.handicap = mock_battle.players[0].handicap;
    desc.pkind = nFTPlayerKindCom;
    desc.level = 5;
    desc.controller = &mock_in;
    desc.is_skip_entry = TRUE;
    desc.figatree_heap = ftManagerAllocFigatreeHeapKind(nFTKindMario);
    memset(&mock_in, 0, sizeof(mock_in));

    mock_gobj = ftManagerMakeFighter(&desc);
    ftParamInitPlayerBattleStats(0, mock_gobj);
    fpp = ftGetStruct(mock_gobj);
    ftParamUnlockPlayerControl(mock_gobj);
    ft_clear_knockback(fpp);

    /* settle into Wait, the same one frame test_computer_dispatch uses,
     * so status_id/ga are the ordinary standing-still values the decomp
     * expects rather than whatever the spawn-frame transients left. */
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(fp.ga == nMPKineticsGround);

    /* a point 1000 units to the fighter's right, same height -- inside
     * the mock stage's flat floor, so none of FollowObjectiveWalk's
     * ceiling/floor-edge/wall redirects fire and it falls through to
     * the plain ground-approach tail. */
    fp.computer.target_pos.x = 1000.0f;
    fp.computer.target_pos.y = fp.joints[nFTPartsJointTopN]->translate.vec.f.y;

    ftComputerFollowObjectiveWalk(fpp);
    ftComputerUpdateInputs(fpp);

    CHECK(fp.input.cp.stick_range.x > 0);
    CHECK(fp.input.cp.stick_range.y == 0);

    /* the mirror case: a target to the left pushes the stick negative. */
    fp.computer.target_pos.x = -1000.0f;

    ftComputerFollowObjectiveWalk(fpp);
    ftComputerUpdateInputs(fpp);

    CHECK(fp.input.cp.stick_range.x < 0);
    CHECK(fp.input.cp.stick_range.y == 0);

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* ftcomputer.c's own script table -- not exposed through ft/ftcomputer.h
 * (it isn't in the decomp's either), so the test that checks a specific
 * script landed declares it the same way the file's other cross-TU
 * externs above do. */
extern u8 *dFTComputerPlayerInputScripts[];

/* ftComputerCheckTargetItemInRange and
 * ftComputerFollowObjectiveTrackItem (ftcomputer.c). The range check
 * reads fp->attr->item_pickup, Mario's real ROM values
 * (ssb-decomp-re/src/relocData/203_MarioMain.c:280: offset {105, 0},
 * range {378, 200} for a light item). The pack carries
 * them, so the test asserts them -- the pipeline's own regression
 * guard, since a lost field would degrade every fighter's pickup window
 * to a point without any other symptom. Follows
 * test_it_manager_make_item's own pattern (hosttest_ft.c) for a
 * standalone item GObj: a private ITStruct pool swapped into
 * gITManagerStructsAllocFree, a stack ITAttributes reached through a
 * one-element fake "file" table. */
static void test_computer_track_item(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    ITStruct *save_free = gITManagerStructsAllocFree;
    FTDesc desc = dFTManagerDefaultFighterDesc;
    ITAttributes it_attr;
    ITDesc it_desc;
    void *file_ptr;
    Vec3f it_pos = { 200.0f, 0.0f, 0.0f };
    Vec3f it_vel = { 0.0f, 0.0f, 0.0f };
    GObj *item_gobj;
    ITStruct *ip;
    int i;

    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
        pool[i].next = &pool[i + 1];
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    mock_model.attr = mock_attr;
    mock_anim_len = 1e9f;
    mock_transn[0] = mock_transn[1] = mock_transn[2] = 0.0f;
    mock_transn_rot_z = 0.0f;

    if (mock_gobj != NULL)
    {
        ftManagerDestroyFighter(mock_gobj);
    }
    if (mock_gobj2 != NULL)
    {
        ftManagerDestroyFighter(mock_gobj2);
        mock_gobj2 = NULL;
        fpp2 = NULL;
    }
    desc.fkind = nFTKindMario;
    desc.pos.x = 0.0f;
    desc.pos.y = 0.0f;
    desc.pos.z = 0.0f;
    desc.lr = 1;
    desc.team = 0;
    desc.player = 0;
    desc.detail = nFTPartsDetailHigh;
    desc.shade = 1;
    desc.stock_count = mock_battle.stocks;
    desc.damage = 0;
    desc.handicap = mock_battle.players[0].handicap;
    desc.pkind = nFTPlayerKindCom;
    desc.level = 5;
    desc.controller = &mock_in;
    desc.is_skip_entry = TRUE;
    desc.figatree_heap = ftManagerAllocFigatreeHeapKind(nFTKindMario);
    memset(&mock_in, 0, sizeof(mock_in));

    mock_gobj = ftManagerMakeFighter(&desc);
    ftParamInitPlayerBattleStats(0, mock_gobj);
    fpp = ftGetStruct(mock_gobj);
    ftParamUnlockPlayerControl(mock_gobj);
    ft_clear_knockback(fpp);

    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    /* The pack carries the real numbers, so this
     * asserts them instead of overriding them: Mario's own
     * relocData/203_MarioMain.c:280 row, through
     * tools/export/ssb_packexport.py -> FPackAttr -> ftManagerSetupAttributes.
     * If the pack pipeline ever loses the field again this fails here
     * rather than degrading the window to a point in silence. */
    CHECK(fp.attr->item_pickup.pickup_offset_light.x == 105.0f);
    CHECK(fp.attr->item_pickup.pickup_offset_light.y == 0.0f);
    CHECK(fp.attr->item_pickup.pickup_range_light.x == 378.0f);
    CHECK(fp.attr->item_pickup.pickup_range_light.y == 200.0f);
    CHECK(fp.attr->item_pickup.pickup_offset_heavy.x == 75.0f);
    CHECK(fp.attr->item_pickup.pickup_range_heavy.x == 150.0f);
    /* and the rest of the item half, same row (:281-283, :348, :350) */
    CHECK(fp.attr->itemthrow_vel_scale == 100);
    CHECK(fp.attr->itemthrow_damage_scale == 100);

    memset(&it_attr, 0, sizeof(it_attr));
    it_attr.data = NULL;
    it_attr.type = 3;
    it_attr.weight = 1; /* ub32:1 -- Heavy = 0, Light = 1 */
    it_attr.map_coll_top = 0.0f;
    it_attr.map_coll_center = 0.0f;
    it_attr.map_coll_bottom = 0.0f;
    it_attr.map_coll_width = 0.0f;

    file_ptr = &it_attr;
    memset(&it_desc, 0, sizeof(it_desc));
    it_desc.kind = 7;
    it_desc.p_file = &file_ptr;
    it_desc.attack_state = nGMAttackStateNew;
    it_desc.proc_update = test_it_make_item_proc_update;
    it_desc.proc_map = test_it_make_item_proc_map;

    /* 200 units right of spawn, same height: inside the {105, 0}/{378,
     * 200} window (-273 < 200 < 483). */
    item_gobj = itManagerMakeItem(NULL, &it_desc, &it_pos, &it_vel, 0);
    CHECK(item_gobj != NULL);
    ip = itGetStruct(item_gobj);
    CHECK(ip->weight == nITWeightLight);

    fp.computer.target_user = ip;
    CHECK(ftComputerCheckTargetItemInRange(fpp) != FALSE);

    /* in range, idle -> the grab script, not a fresh objective. */
    fp.status_id = nFTCommonStatusWait;
    ftComputerFollowObjectiveTrackItem(fpp);
    CHECK((void *)fp.computer.p_command ==
          (void *)dFTComputerPlayerInputScripts[nFTComputerInputStickNButtonBZReleaseAPress]);

    /* 1000 units right: outside the window (483 < 1000). */
    DObjGetStruct(item_gobj)->translate.vec.f.x = 1000.0f;
    CHECK(ftComputerCheckTargetItemInRange(fpp) == FALSE);

    /* out of range -> falls back to Walk, which is tested
     * on its own: a target to the left pushes the stick
     * negative, the same sign-check test_computer_follow_objective_walk
     * pins. */
    fp.computer.target_pos.x = -500.0f;
    fp.computer.target_pos.y = fp.joints[nFTPartsJointTopN]->translate.vec.f.y;

    ftComputerFollowObjectiveTrackItem(fpp);
    ftComputerUpdateInputs(fpp);

    CHECK(fp.input.cp.stick_range.x < 0);

    gITManagerStructsAllocFree = save_free;
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* ftComputerCheckSetEvadeTarget and
 * ftComputerFollowObjectiveEvade (ftcomputer.c). Both fighters are plain
 * spawn()/spawn_second() -- the function itself doesn't care whether
 * `this_fp` is Com or Man, only that a nearer opposite-team fighter
 * exists, so unlike the CPU-dispatch tests this one needs no
 * nFTPlayerKindCom at all. All three cases lean on the mock stage's real
 * deck geometry (hosttest_ft.c's kVpos: the floor runs -2000..2000) so
 * the fleeing math lands on exact numbers rather than fabricated ones:
 * evasion always aims 2500 units past the target, which straddles the
 * deck's 4000-unit width depending on which side the target is on. */
static void test_computer_evade_target(void)
{
    sb32 result;

    /* alone on the stage: the search loop finds no other-team fighter,
     * so the sentinel distance survives and the fighter is told to stay
     * put -- not a stub, the decomp's own "nothing to run from" case. */
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    result = ftComputerCheckSetEvadeTarget(fpp);
    CHECK(result != FALSE);
    CHECK_EQF(fp.computer.target_pos.x, 0.0f);
    CHECK(fp.computer.target_line_id == -1);

    /* an opponent at x=1000 (predict_x >= 500): fleeing left lands at
     * 1000 - 2500 = -1500, short of the deck's left edge (-2000), so the
     * plain path runs: objective becomes Evade and target_pos.x is
     * exactly -1500. */
    spawn_second(1000.0f, 0.0f, -1);
    frame(0, 0, 0, 0, 0);
    CHECK(fpp2->status_id >= nFTCommonStatusWait);

    result = ftComputerCheckSetEvadeTarget(fpp);
    CHECK(result != FALSE);
    CHECK((void *)fp.computer.target_user == (void *)fpp2);
    CHECK_EQF(fp.computer.target_pos.x, -1500.0f);
    CHECK(fp.computer.objective == nFTComputerObjectiveEvade);

    /* the mirror case, an opponent to the left: fleeing right lands at
     * -1000 + 2500 = 1500, also short of the right edge (2000). */
    DObjGetStruct(mock_gobj2)->translate.vec.f.x = -1000.0f;

    result = ftComputerCheckSetEvadeTarget(fpp);
    CHECK(result != FALSE);
    CHECK_EQF(fp.computer.target_pos.x, 1500.0f);

    /* an opponent at x=200 (predict_x < 500): fleeing left would land at
     * 200 - 2500 = -2300, PAST the left edge (-2000) -- the clamp fires,
     * flips to the opposite flee direction (200 + 2500 = 2700, never
     * itself checked against the far edge), fires an immediate turn and
     * returns FALSE rather than reaching FollowObjectiveWalk. */
    DObjGetStruct(mock_gobj2)->translate.vec.f.x = 200.0f;

    result = ftComputerCheckSetEvadeTarget(fpp);
    CHECK(result == FALSE);
    CHECK_EQF(fp.computer.target_pos.x, 2700.0f);
    CHECK((void *)fp.computer.p_command ==
          (void *)dFTComputerPlayerInputScripts[nFTComputerInputMoveAutoStickTiltHiReleaseZ]);

    /* FollowObjectiveEvade: the plain case (opponent at 1000, restored)
     * hands off to Walk -- the same
     * sign-check test_computer_follow_objective_walk already pins. */
    DObjGetStruct(mock_gobj2)->translate.vec.f.x = 1000.0f;

    ftComputerFollowObjectiveEvade(fpp);
    ftComputerUpdateInputs(fpp);
    CHECK(fp.input.cp.stick_range.x < 0);

    ftManagerDestroyFighter(mock_gobj2);
    mock_gobj2 = NULL;
    fpp2 = NULL;
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* func_ovl3_801361BC and ftComputerFollowObjectiveCounterAttack
 * (ftcomputer.c), both fully self-contained (no unported callee at all).
 * Uses the plain spawn() single-fighter helper and overrides fp.fkind
 * by hand afterward, the same way the file's other fkind-switch tests
 * already do (e.g. the specialhi/speciallw tests around line 2339). */
static void test_computer_counter_attack(void)
{
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    /* func_ovl3_801361BC, grounded, Mario (threshold 7): below it and
     * not Run/Dash -> marks is_shield_item_weapon rather than aiming
     * target_pos. */
    fp.computer.is_counterattack = FALSE;
    fp.computer.is_opponent_ra = FALSE;
    fp.computer.is_shield_item_weapon = FALSE;
    fp.computer.unk_ftcom_0x38 = 0;

    func_ovl3_801361BC(fpp);
    CHECK(fp.computer.is_shield_item_weapon != FALSE);
    CHECK(fp.computer.target_line_id == -1);

    /* airborne, already past the old target and falling: aims 1100
     * below the fighter's own current position rather than above it --
     * real field math, not a stub. */
    fp.computer.is_counterattack = FALSE;
    fp.computer.is_opponent_ra = FALSE;
    fp.ga = nMPKineticsAir;
    fp.computer.target_pos.y = 99999.0f; /* comfortably above damage_coll_size.y past the top joint */
    fp.physics.vel_air.y = -1.0f;

    func_ovl3_801361BC(fpp);
    CHECK_EQF(fp.computer.target_pos.y, fp.joints[nFTPartsJointTopN]->translate.vec.f.y - 1100.0f);

    fp.ga = nMPKineticsGround;

    /* FollowObjectiveCounterAttack: is_counterattack -> an immediate
     * grab-release, and the flag clears. */
    fp.computer.is_counterattack = TRUE;
    fp.computer.is_opponent_ra = FALSE;
    fp.computer.is_shield_item_weapon = FALSE;

    ftComputerFollowObjectiveCounterAttack(fpp);
    CHECK((void *)fp.computer.p_command ==
          (void *)dFTComputerPlayerInputScripts[nFTComputerInputButtonZ2]);
    CHECK(fp.computer.is_counterattack == FALSE);

    /* is_opponent_ra, Fox, outside the SpecialLw scope -> the
     * reflect/absorb-punish command. */
    fp.fkind = nFTKindFox;
    fp.status_id = nFTCommonStatusWait;
    fp.computer.is_opponent_ra = TRUE;

    ftComputerFollowObjectiveCounterAttack(fpp);
    CHECK((void *)fp.computer.p_command ==
          (void *)dFTComputerPlayerInputScripts[nFTComputerInputStickNXSmashLwButtonBReleaseBHold]);
    CHECK(fp.computer.is_opponent_ra == FALSE);
    fp.fkind = nFTKindMario;

    /* is_shield_item_weapon, outside the Guard scope -> hold shield. */
    fp.status_id = nFTCommonStatusWait;
    fp.computer.is_shield_item_weapon = TRUE;

    ftComputerFollowObjectiveCounterAttack(fpp);
    CHECK((void *)fp.computer.p_command ==
          (void *)dFTComputerPlayerInputScripts[nFTComputerInputStickNButtonZHold]);
    CHECK(fp.computer.is_shield_item_weapon == FALSE);

    /* none of the three flags set -> falls back to Walk. Grounded, at
     * or past Mario's own charge threshold (7), func_ovl3_801361BC's
     * own top-of-function call sets target_pos to (top.x, top.y+1100)
     * rather than raising is_shield_item_weapon (that's the
     * below-threshold arm the first check above already covered), so
     * this reaches Walk with the target directly overhead: same x as
     * the fighter, y above it, no floor line staked out
     * (target_line_id == -1, never the real floor_line_id) -- Walk's
     * own ground path answers with a jump-toward command, not a plain
     * horizontal MoveAuto. */
    fp.computer.is_counterattack = FALSE;
    fp.computer.is_opponent_ra = FALSE;
    fp.computer.is_shield_item_weapon = FALSE;
    fp.computer.unk_ftcom_0x38 = 100;

    ftComputerFollowObjectiveCounterAttack(fpp);
    CHECK(fp.computer.is_shield_item_weapon == FALSE);
    CHECK_EQF(fp.computer.target_pos.x, fp.joints[nFTPartsJointTopN]->translate.vec.f.x);
    CHECK_EQF(fp.computer.target_pos.y, fp.joints[nFTPartsJointTopN]->translate.vec.f.y + 1100.0f);
    CHECK((void *)fp.computer.p_command ==
          (void *)dFTComputerPlayerInputScripts[nFTComputerInputMoveAutoStickTiltHiReleaseZ]);

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* ftComputerCheckSetTargetEdgeRight/Left (ftcomputer.c), a
 * near-mirror pair with no caller yet -- func_ovl3_80134964, their one
 * decomp caller, is still unported and part of Recover's own chain, so
 * (like the neighbouring tests) this test calls both functions directly rather than
 * through dispatch. fp->attr is a shared per-fkind table row
 * (ftManagerKindAttributes), not a private copy, so the three fields
 * this test drives to keep the fall-arc math simple are saved and
 * restored around it rather than left mutated for later tests. */
static void test_computer_edge_target(void)
{
    f32 save_air_speed_max_x;
    f32 save_tvel_base;
    f32 save_gravity;
    sb32 result;

    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    /* airborne and still rising (vel_air.y >= 0): both functions skip
     * the whole floor-line search and aim just short of directly below
     * the fighter's own current position -- pure field arithmetic, no
     * geometry involved. */
    fp.physics.vel_air.y = 5.0f;

    result = ftComputerCheckSetTargetEdgeRight(fpp, FALSE);
    CHECK(result != FALSE);
    CHECK_EQF(fp.computer.target_pos.x, fp.joints[nFTPartsJointTopN]->translate.vec.f.x - 500.0f);
    CHECK_EQF(fp.computer.target_pos.y, fp.joints[nFTPartsJointTopN]->translate.vec.f.y);

    result = ftComputerCheckSetTargetEdgeLeft(fpp, FALSE);
    CHECK(result != FALSE);
    CHECK_EQF(fp.computer.target_pos.x, fp.joints[nFTPartsJointTopN]->translate.vec.f.x + 500.0f);
    CHECK_EQF(fp.computer.target_pos.y, fp.joints[nFTPartsJointTopN]->translate.vec.f.y);

    /* airborne and falling: drive air_speed_max_x/tvel_base/gravity to
     * values that put the fall-arc predictor in its simplest branch
     * (fall_predict <= 0, so edge_predict_y is a single linear step, not
     * the parabolic arms below it). is_find_edge_target = TRUE skips
     * the jump_predict gate so only the edge_offset guard is live.
     *
     * The mock's floor-line group (kVpos/kLineInfo above) has three
     * lines, iterated in a fixed order: the slope (3000,0)-(6000,1500),
     * the pass-through platform (-500,300)-(500,300), then the deck
     * (-2000,0)-(2000,0). The search returns on the FIRST candidate
     * whose own edge_dist_x sign matches and whose predicted landing
     * isn't a narrow near-miss just below that edge -- not the nearest
     * one -- so which real stage feature answers depends on iteration
     * order, not proximity. */
    save_air_speed_max_x = fp.attr->air_speed_max_x;
    save_tvel_base = fp.attr->tvel_base;
    save_gravity = fp.attr->gravity;

    fp.attr->air_speed_max_x = 200.0f;
    fp.attr->tvel_base = -100.0f;
    fp.attr->gravity = 1.0f;
    fp.ga = nMPKineticsAir;
    fp.physics.vel_air.y = -1.0f;

    /* just past the platform's own right edge (500,300): the slope's
     * right edge (6000) fails its distance check first (fp is short of
     * it), so the platform is the first real candidate, and a shallow
     * fall (predicted landing 499, still above the platform's own 300)
     * clears the near-miss guard -- accepted immediately. */
    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = 700.0f;
    fp.joints[nFTPartsJointTopN]->translate.vec.f.y = 500.0f;

    result = ftComputerCheckSetTargetEdgeRight(fpp, TRUE);
    CHECK(result != FALSE);
    CHECK_EQF(fp.computer.target_pos.x, 0.0f);
    CHECK_EQF(fp.computer.target_pos.y, 300.0f);

    /* the mirror position, off the platform's left edge (-500,300): but
     * the slope's own LEFT edge (3000,0) is line 0, checked before the
     * platform, and its loose leftward distance gate (fp.x < 3000) is
     * satisfied by essentially any position on or near the deck -- so
     * for Left, the slope wins first, not the platform. A real
     * consequence of iteration order, not a mirror of the Right case
     * above. */
    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = -700.0f;
    fp.joints[nFTPartsJointTopN]->translate.vec.f.y = 500.0f;

    result = ftComputerCheckSetTargetEdgeLeft(fpp, TRUE);
    CHECK(result != FALSE);
    CHECK_EQF(fp.computer.target_pos.x, 3500.0f);
    CHECK_EQF(fp.computer.target_pos.y, 0.0f);

    /* no candidate at all: past the slope's own left edge (x >= 3000)
     * for Left, every one of the three floor lines fails its distance
     * check outright. */
    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = 3500.0f;

    result = ftComputerCheckSetTargetEdgeLeft(fpp, TRUE);
    CHECK(result == FALSE);

    /* short of the platform's own right edge (x <= 500) for Right,
     * every line fails the same way. */
    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = 400.0f;

    result = ftComputerCheckSetTargetEdgeRight(fpp, TRUE);
    CHECK(result == FALSE);

    fp.attr->air_speed_max_x = save_air_speed_max_x;
    fp.attr->tvel_base = save_tvel_base;
    fp.attr->gravity = save_gravity;

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* func_ovl3_801346D4, func_ovl3_80134964,
 * ftComputerCheckTryCancelSpecialN and ftComputerFollowObjectiveRecover
 * (ftcomputer.c) -- the edge-target pair's own one caller, now that it
 * exists, plus Recover's SpecialN-cancel guard. */
static void test_computer_recover(void)
{
    f32 bound_right;
    sb32 result;

    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    /* ftComputerCheckTryCancelSpecialN: Donkey mid SpecialN charge -- an
     * immediate Z-tap (script index 0xC) cancels it, TRUE. */
    fp.fkind = nFTKindDonkey;
    fp.status_id = nFTDonkeyStatusSpecialNStart;

    result = ftComputerCheckTryCancelSpecialN(fpp);
    CHECK(result != FALSE);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[0xC]);

    /* same fkind, a status the switch doesn't list -> FALSE. */
    fp.status_id = nFTCommonStatusWait;

    result = ftComputerCheckTryCancelSpecialN(fpp);
    CHECK(result == FALSE);

    /* Kirby copying Donkey: the copy_id, not fkind, drives the switch. */
    fp.fkind = nFTKindKirby;
    fp.passive_vars.kirby.copy_id = nFTKindDonkey;
    fp.status_id = nFTKirbyStatusCopyDonkeySpecialAirNLoop;

    result = ftComputerCheckTryCancelSpecialN(fpp);
    CHECK(result != FALSE);

    /* Samus mid SpecialN charge -> the same cancel. */
    fp.fkind = nFTKindSamus;
    fp.status_id = nFTSamusStatusSpecialNLoop;

    result = ftComputerCheckTryCancelSpecialN(fpp);
    CHECK(result != FALSE);

    /* Mario has no charge to cancel at all -- FALSE regardless of
     * status_id (an arbitrary non-Mario id, to prove it's the fkind
     * switch and not the status that gates this, and not left over
     * default-zero either). */
    fp.fkind = nFTKindMario;
    fp.status_id = nFTDonkeyStatusSpecialNStart;

    result = ftComputerCheckTryCancelSpecialN(fpp);
    CHECK(result == FALSE);

    fp.status_id = nFTCommonStatusWait;

    /* func_ovl3_80134964: already within the camera's vertical bounds
     * (is_within_vertical_bounds = TRUE) hands straight off to
     * func_ovl3_801346D4, which aims back toward the stage from
     * whichever side of gMPCollisionBounds.current the fighter is past.
     * jumps_used == jumps_max and a huge overhead position collapse
     * both of func_ovl3_801346D4's own OR-guards to FALSE, so the y
     * result is deterministic (pos.y unchanged) regardless of
     * cliff_right_pos or the per-fkind range table this test doesn't
     * otherwise exercise. */
    bound_right = gMPCollisionBounds.current.right;

    fp.computer.is_within_vertical_bounds = TRUE;
    fp.jumps_used = fp.attr->jumps_max;
    fp.physics.vel_air.y = 0.0f;
    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = bound_right + 500.0f;
    fp.joints[nFTPartsJointTopN]->translate.vec.f.y = 99999.0f;

    func_ovl3_80134964(fpp);
    CHECK_EQF(fp.computer.target_pos.x, bound_right + 500.0f - 1100.0f);
    CHECK_EQF(fp.computer.target_pos.y, 99999.0f);

    /* FollowObjectiveRecover, end to end: the SpecialN cancel takes
     * priority over aiming at all -- target_pos is left exactly at a
     * sentinel, proving the early return, and the cancel script is the
     * one that lands. */
    fp.fkind = nFTKindDonkey;
    fp.status_id = nFTDonkeyStatusSpecialNStart;
    fp.computer.target_pos.x = -12345.0f;
    fp.computer.target_pos.y = -12345.0f;

    ftComputerFollowObjectiveRecover(fpp);
    CHECK_EQF(fp.computer.target_pos.x, -12345.0f);
    CHECK_EQF(fp.computer.target_pos.y, -12345.0f);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[0xC]);

    /* no charge to cancel: Recover aims (func_ovl3_80134964 again) and
     * hands off to Walk, which drives the stick toward target_pos.x --
     * the same sign-check test_computer_follow_objective_walk already
     * established for Walk itself. */
    fp.fkind = nFTKindMario;
    fp.status_id = nFTCommonStatusWait;
    fp.computer.is_within_vertical_bounds = TRUE;
    fp.jumps_used = fp.attr->jumps_max;
    fp.physics.vel_air.y = 0.0f;
    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = bound_right + 500.0f;
    fp.joints[nFTPartsJointTopN]->translate.vec.f.y = 99999.0f;

    ftComputerFollowObjectiveRecover(fpp);
    CHECK_EQF(fp.computer.target_pos.x, bound_right + 500.0f - 1100.0f);

    ftComputerUpdateInputs(fpp);
    CHECK(fp.input.cp.stick_range.x < 0); /* target is left of the fighter */

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* ftComputerCheckFindTarget, ftComputerCheckEvadeDistance,
 * ftComputerWaitGetTarget and func_ovl3_80132EC0 (ftcomputer.c) -- the
 * target-search building blocks nearly everything left in this file
 * still needs. */
static void test_computer_target_search(void)
{
    sb32 result;

    spawn(0.0f, 0.0f);
    spawn_second(1000.0f, 0.0f, -1);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    /* ftComputerCheckFindTarget: the only opposing fighter on screen,
     * so it becomes the target. */
    result = ftComputerCheckFindTarget(fpp);
    CHECK(result != FALSE);
    CHECK((void *)fp.computer.target_user == (void *)fpp2);
    CHECK_EQF(fp.computer.target_pos.x, 1000.0f);
    CHECK(fp.computer.target_dist > 0.0f);
    CHECK(fp.computer.ftcom_flags_0x4A_b1 != FALSE);

    /* same team: never a candidate at all, regardless of distance. */
    fp2.team = fp.team;

    result = ftComputerCheckFindTarget(fpp);
    CHECK(result == FALSE);
    CHECK(fp.computer.target_line_id == -1);
    CHECK_EQF(fp.computer.target_dist, F32_MAX);

    fp2.team = 1;

    /* ftComputerCheckEvadeDistance: a Star-invincible opponent within
     * 1500 units is worth fleeing outright. The symmetric Hammer branch
     * (same distance-threshold shape, a held item's own kind instead of
     * star_hitstatus) needs a real item GObj on fp2 -- the standalone
     * item pattern test_computer_track_item already builds -- and isn't
     * separately exercised here. */
    fp2.star_hitstatus = nGMHitStatusInvincible;

    result = ftComputerCheckEvadeDistance(fpp);
    CHECK(result != FALSE);

    fp2.star_hitstatus = nGMHitStatusNormal;

    result = ftComputerCheckEvadeDistance(fpp);
    CHECK(result == FALSE);

    /* ftComputerWaitGetTarget: picks the lowest-damage opponent (the
     * only one here) as a wait target, but -- a real decomp oddity, not
     * a port mistake -- always returns NULL itself; every real caller
     * reads com->target_gobj/target_damage_percent, not this return
     * value. */
    fp2.percent_damage = 42;
    fp.computer.target_gobj = NULL;

    CHECK(ftComputerWaitGetTarget(fpp) == NULL);
    CHECK((void *)fp.computer.target_gobj == (void *)mock_gobj2);
    CHECK(fp.computer.target_damage_percent == 42);
    CHECK(fp.computer.wiggle_wait >= 600.0f);

    /* func_ovl3_80132EC0: the decomp's own dead code, never called by
     * anything -- ported only so the census and the "not ported" list
     * both stay honest. Nothing to check but that it doesn't crash. */
    func_ovl3_80132EC0();

    ftManagerDestroyFighter(mock_gobj2);
    mock_gobj2 = NULL;
    fpp2 = NULL;
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* ftComputerCheckTryChargeSpecialN (ftcomputer.c) --
 * Donkey's/GDonkey's Giant Punch and Samus's charge shot (plus Kirby
 * copying either); Ness's own SpecialHi charge is a different
 * mechanism this function never touches. */
static void test_computer_charge_specialn(void)
{
    sb32 result;

    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    /* Donkey, not mid-SpecialN, under the charge cap -> holds the
     * charge (script index 0xB), TRUE. */
    fp.fkind = nFTKindDonkey;
    fp.status_id = nFTCommonStatusWait;
    fp.passive_vars.donkey.charge_level = 0;

    result = ftComputerCheckTryChargeSpecialN(fpp);
    CHECK(result != FALSE);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[0xB]);

    /* already at the charge cap -> FALSE, nothing left to hold. */
    fp.passive_vars.donkey.charge_level = FTDONKEY_GIANTPUNCH_CHARGE_MAX;

    result = ftComputerCheckTryChargeSpecialN(fpp);
    CHECK(result == FALSE);

    /* mid-SpecialN already (one of the disqualifying status IDs) ->
     * FALSE even with room left to charge. */
    fp.passive_vars.donkey.charge_level = 0;
    fp.status_id = nFTDonkeyStatusSpecialNStart;

    result = ftComputerCheckTryChargeSpecialN(fpp);
    CHECK(result == FALSE);

    /* GDonkey: a different status allowlist (idle/movement statuses),
     * same charge field. */
    fp.fkind = nFTKindGDonkey;
    fp.status_id = nFTCommonStatusWait;
    fp.passive_vars.donkey.charge_level = 0;

    result = ftComputerCheckTryChargeSpecialN(fpp);
    CHECK(result != FALSE);

    /* Samus: its own charge field and cap. */
    fp.fkind = nFTKindSamus;
    fp.status_id = nFTCommonStatusWait;
    fp.passive_vars.samus.charge_level = 0;

    result = ftComputerCheckTryChargeSpecialN(fpp);
    CHECK(result != FALSE);

    /* Kirby copying Donkey: the copy_id, not fkind, drives the switch,
     * and its own separate charge field. */
    fp.fkind = nFTKindKirby;
    fp.passive_vars.kirby.copy_id = nFTKindDonkey;
    fp.status_id = nFTCommonStatusWait;
    fp.passive_vars.kirby.copydonkey_charge_level = 0;

    result = ftComputerCheckTryChargeSpecialN(fpp);
    CHECK(result != FALSE);

    /* Mario has no charge mechanic at all -- FALSE regardless. */
    fp.fkind = nFTKindMario;
    fp.status_id = nFTCommonStatusWait;

    result = ftComputerCheckTryChargeSpecialN(fpp);
    CHECK(result == FALSE);

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* func_ovl3_8013877C (ftcomputer.c) -- the "nothing better
 * to do" idle patrol every one of Attack/Ally/Patrol/Rush/UnknownObj1
 * falls back to once func_ovl3_8013837C (still unported) and their own
 * target search come up empty. */
static void test_computer_idle_patrol(void)
{
    int i;

    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    fp.computer.walk_stop_wait = 0;

    /* the first 29 ticks: just holds the stick centered, counting up
     * (level 9's own "poke while waiting" frame, walk_stop_wait ==
     * (-level*2 + 18) == 0, is unreachable here since the increment
     * always runs before the check -- walk_stop_wait is never seen at
     * exactly 0 again once this loop starts). */
    for (i = 0; i < 29; i++)
    {
        func_ovl3_8013877C(fpp);
        CHECK(fp.computer.walk_stop_wait == i + 1);
        CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputStickN]);
    }

    /* the 30th tick: latches the fighter's own current floor line and
     * position as origin/edge, then -- since edge_pos still equals the
     * fighter's own position, within the 100-unit "pick a new spot"
     * threshold -- rolls a fresh random point along that same floor
     * line, clamped to its real edges (the mock deck, -2000..2000). */
    func_ovl3_8013877C(fpp);

    CHECK(fp.computer.floor_line_id == fp.coll_data.floor_line_id);
    CHECK(fp.computer.target_line_id == fp.computer.floor_line_id);
    CHECK(fp.computer.ftcom_flags_0x4A_b1 == FALSE);
    CHECK(fp.computer.target_pos.x >= -2000.0f);
    CHECK(fp.computer.target_pos.x <= 2000.0f);
    CHECK_EQF(fp.computer.target_pos.x, fp.computer.edge_pos.x);

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* func_ovl3_8013837C (ftcomputer.c) -- the shared "who am I
 * engaging" resolver. A real, verified oddity: ftComputerWaitGetTarget
 * always returns NULL itself, so this function's
 * own "already have a wait-target" arm can never actually take its
 * `return 1` -- every case here falls through to the
 * ftComputerCheckFindTarget-driven follow-timer below, whether or not
 * WaitGetTarget ran. What DOES differ is WaitGetTarget's side effect on
 * com->target_gobj -- skipped outright for GDonkey/YoshiTeam behavior,
 * which go straight to the follow-timer without calling it at all. */
static void test_computer_engage_target(void)
{
    s32 result;

    spawn(0.0f, 0.0f);
    spawn_second(1000.0f, 0.0f, -1);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    fp2.percent_damage = 10;

    /* fresh fighter_follow_since (0): always the "not yet due" reset,
     * regardless of a perfectly viable low-damage opponent sitting
     * right there -- proving target_fp from WaitGetTarget really is
     * NULL, not just untested. */
    fp.computer.fighter_follow_since = 0;
    fp.computer.target_gobj = NULL;

    result = func_ovl3_8013837C(fpp);
    CHECK(result == 0);
    CHECK(fp.computer.fighter_follow_since == 1);
    /* WaitGetTarget's own side effect DID run for this fkind. */
    CHECK((void *)fp.computer.target_gobj == (void *)mock_gobj2);

    /* GDonkey skips WaitGetTarget outright -- the same fresh-since reset
     * happens, but target_gobj is untouched (a sentinel survives). */
    fp.fkind = nFTKindGDonkey;
    fp.computer.fighter_follow_since = 0;
    fp.computer.target_gobj = (GObj *)0x1234;

    result = func_ovl3_8013837C(fpp);
    CHECK(result == 0);
    CHECK((void *)fp.computer.target_gobj == (void *)0x1234);

    fp.fkind = nFTKindMario;
    fp.computer.target_gobj = NULL;

    /* follow timer due, Mario (no charge/cancel of its own): the outer
     * `if` re-runs ftComputerCheckFindTarget on every call regardless
     * of fighter_follow_since (it's the left side of an OR), so
     * com->target_dist is always the real fp/fp2 distance by the time
     * the follow-timer body reads it, not whatever was last written by
     * hand -- fp2 at x=1000 makes that ~1000, under the 1200 cutoff,
     * so this falls all the way through to the default "engage" 1. */
    fp.computer.fighter_follow_since = 5;
    fp.computer.fighter_follow_wait = 5;
    fp.computer.fighter_follow_end = 100;

    result = func_ovl3_8013837C(fpp);
    CHECK(result == 1);
    CHECK(fp.computer.fighter_follow_since == 6);
    CHECK(fp.computer.target_dist < 1200.0f);

    /* follow timer due, Donkey mid-SpecialN charge, still the same
     * close (~1000) real distance -- the cancel fires instead, -1. */
    fp.fkind = nFTKindDonkey;
    fp.status_id = nFTDonkeyStatusSpecialNStart;
    fp.computer.fighter_follow_since = 5;
    fp.computer.fighter_follow_wait = 5;

    result = func_ovl3_8013837C(fpp);
    CHECK(result == -1);

    /* follow timer due, Donkey NOT yet charging, target moved out past
     * 1200 for real (so the recomputed target_dist actually clears the
     * cutoff this time) and grounded -- the charge-instead-of-engage
     * arm fires, also -1. */
    DObjGetStruct(mock_gobj2)->translate.vec.f.x = 1500.0f;
    fp.status_id = nFTCommonStatusWait;
    fp.passive_vars.donkey.charge_level = 0;
    fp.computer.fighter_follow_since = 5;
    fp.computer.fighter_follow_wait = 5;
    fp.ga = nMPKineticsGround;

    result = func_ovl3_8013837C(fpp);
    CHECK(result == -1);
    CHECK(fp.computer.target_dist >= 1200.0f);

    ftManagerDestroyFighter(mock_gobj2);
    mock_gobj2 = NULL;
    fpp2 = NULL;
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* func_ovl3_80138AA8 (ftcomputer.c) -- the opportunistic
 * smash-lunge at the current target. com->target_user is left NULL
 * throughout so the level-scaled Ness/Fox "back off" roll (gated on
 * `target_fp != NULL`) never fires, keeping every case here
 * deterministic without touching the RNG's outcome at all. */
static void test_computer_lunge_attack(void)
{
    sb32 result;

    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    fp.computer.target_user = NULL;

    /* the target is more than 400 units off in height -- the very first
     * gate, before fkind or distance matter at all. */
    fp.computer.target_pos.x = 0.0f;
    fp.computer.target_pos.y = fp.joints[nFTPartsJointTopN]->translate.vec.f.y + 1000.0f;

    result = func_ovl3_80138AA8(fpp, FALSE);
    CHECK(result == FALSE);

    /* in height range, but Yoshi isn't in the lunge allowlist at all --
     * falls out of the switch with nothing done. */
    fp.fkind = nFTKindYoshi;
    fp.computer.target_pos.y = fp.joints[nFTPartsJointTopN]->translate.vec.f.y;
    fp.computer.unk_ftcom_0x35 = 1;

    result = func_ovl3_80138AA8(fpp, FALSE);
    CHECK(result == FALSE);

    /* Mario, in range, already at the lunge-frequency cap
     * (unk_ftcom_0x35 >= 5 after its own increment) -- hands off to
     * Walk instead of lunging, TRUE either way. */
    fp.fkind = nFTKindMario;
    fp.computer.unk_ftcom_0x35 = 5;

    result = func_ovl3_80138AA8(fpp, FALSE);
    CHECK(result != FALSE);

    /* Mario, in range, under the cap, target_pos a short step to one
     * side with nothing between -- a clear lunge, the long-wait smash
     * script. */
    fp.computer.unk_ftcom_0x35 = 1;
    fp.computer.target_pos.x = fp.joints[nFTPartsJointTopN]->translate.vec.f.x - 10.0f;

    result = func_ovl3_80138AA8(fpp, FALSE);
    CHECK(result != FALSE);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputStickSmashAutoXButtonB]);

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* func_ovl3_80138EE4 (ftcomputer.c) -- should the CPU
 * dodge-roll away from its own target right now? DIVERGES: none -- the
 * decomp's own #if defined(REGION_US) Donkey-family allowlist IS this
 * port's region, kept as written. */
static void test_computer_escape_roll(void)
{
    sb32 result;

    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    fp.computer.ftcom_flags_0x4A_b1 = FALSE;

    /* GDonkey never rolls at all, gated before anything else runs. */
    fp.fkind = nFTKindGDonkey;

    result = func_ovl3_80138EE4(fpp);
    CHECK(result == FALSE);

    /* Mario, nothing close or threatening enough to roll from -- the
     * distance gate itself fails regardless of level or position. */
    fp.fkind = nFTKindMario;
    fp.computer.target_dist = 5000.0f;

    result = func_ovl3_80138EE4(fpp);
    CHECK(result == FALSE);

    /* Mario, close target, level 9 (>= the position-based clause's own
     * floor of 3) and to the left (lr=1, so (target.x - top.x) * lr <
     * 0) -- the roll fires without needing the sibling random clause to
     * cooperate, and lands short of the deck's own left edge (-2000),
     * so the edge guard doesn't block it either. */
    fp.level = 9;
    fp.lr = 1;
    fp.computer.target_dist = 100.0f;
    fp.computer.target_pos.x = fp.joints[nFTPartsJointTopN]->translate.vec.f.x - 500.0f;

    result = func_ovl3_80138EE4(fpp);
    CHECK(result != FALSE);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputEscapeL]);

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* ftComputerCheckDetectTarget (ftcomputer.c) -- the
 * attack-selection weighted pick, the last real prerequisite
 * Attack/Ally/Patrol/Rush need. Mario's own ground attack table
 * (dFTComputerAttacksMario, verbatim) has eleven
 * entries whose x/y windows overlap heavily by design -- several
 * attacks are meant to be simultaneously "in range" so the weighted
 * roll has something to choose between -- so rather than fight the RNG
 * for one exact winner, the in-range case below asserts the invariants
 * that must hold no matter which entry wins: the result is TRUE, the
 * command actually set is the one named by the input_kind this same
 * call wrote, and that input_kind is one of Mario's own eleven real
 * ground scripts, not garbage -- checked across enough repeated calls
 * to be a real statistical statement about the roll, not a lucky
 * single draw. */
static void test_computer_detect_target(void)
{
    sb32 result;
    int i;

    spawn(0.0f, 0.0f);
    spawn_second(200.0f, 0.0f, -1);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    fp.computer.target_user = fpp2;

    /* far enough away (well past even SpecialN's own 1200-unit reach,
     * plus any hurtbox margin) that nothing in the table can possibly
     * detect it -- attack_count stays 0, the plain FALSE path. */
    DObjGetStruct(mock_gobj2)->translate.vec.f.x = 50000.0f;

    result = ftComputerCheckDetectTarget(fpp, 0.0f);
    CHECK(result == FALSE);

    /* back in range of several ground attacks at once. */
    DObjGetStruct(mock_gobj2)->translate.vec.f.x = 200.0f;

    for (i = 0; i < 50; i++)
    {
        result = ftComputerCheckDetectTarget(fpp, 0.0f);
        CHECK(result != FALSE);
        CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[fp.computer.input_kind]);
        CHECK
        (
            (fp.computer.input_kind == nFTComputerInputStickNButtonA)             ||
            (fp.computer.input_kind == nFTComputerInputStickTiltAutoXButtonA)     ||
            (fp.computer.input_kind == nFTComputerInputStickSmashAutoXNYButtonA)  ||
            (fp.computer.input_kind == nFTComputerInputStickTiltHiButtonA)        ||
            (fp.computer.input_kind == nFTComputerInputStickSmashHiButtonA)       ||
            (fp.computer.input_kind == nFTComputerInputStickTiltLwButtonA)        ||
            (fp.computer.input_kind == nFTComputerInputStickSmashLwButtonA)       ||
            (fp.computer.input_kind == nFTComputerInputStickSmashAutoXButtonB)    ||
            (fp.computer.input_kind == nFTComputerInputStickSmashHiButtonB)       ||
            (fp.computer.input_kind == nFTComputerInputStickSmashLwButtonB)       ||
            (fp.computer.input_kind == nFTComputerInputStickNButtonZButtonA)
        );
    }

    ftManagerDestroyFighter(mock_gobj2);
    mock_gobj2 = NULL;
    fpp2 = NULL;
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* ftComputerFollowObjectiveUseItem (ftcomputer.c). Follows
 * test_computer_track_item's own pattern (hosttest_ft.c) for a standalone
 * item GObj, but pokes ITStruct's own type/kind/multi fields directly
 * between cases instead of building a fresh item per case -- the
 * function only ever reads them, so one real item GObj carries every
 * case. Covers: a plain single-use throwable (Bob-Omb, nITTypeThrow)
 * both under and at the accumulated-wait threshold; the Poké Ball
 * special case that always forces the long-wait throw regardless of the
 * counter; a spent, non-multi Shoot item (falls through to the same
 * throw logic, but increments item_throw_wait itself first, unlike the
 * Damage/Throw types above); and a multi-charge Shoot item (Ray Gun)
 * aiming at a real target found live via ftComputerCheckFindTarget --
 * the aligned release script, the level>=5 Ness/Fox insta-throw (which,
 * a real decomp quirk and not a port bug, returns before the function's
 * own item_throw_wait reset at the bottom -- the counter is left
 * untouched, not merely unreset-to-zero), and both Walk fallbacks
 * (Fire-Flower misalignment, and vertical distance alone regardless of
 * item kind). */
static void test_computer_use_item(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    ITStruct *save_free = gITManagerStructsAllocFree;
    ITAttributes it_attr;
    ITDesc it_desc;
    void *file_ptr;
    Vec3f it_pos = { 0.0f, 0.0f, 0.0f };
    Vec3f it_vel = { 0.0f, 0.0f, 0.0f };
    GObj *item_gobj;
    ITStruct *ip;
    int i;

    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
        pool[i].next = &pool[i + 1];
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    memset(&it_attr, 0, sizeof(it_attr));
    it_attr.data = NULL;
    it_attr.type = nITTypeThrow;
    it_attr.weight = 1;

    file_ptr = &it_attr;
    memset(&it_desc, 0, sizeof(it_desc));
    it_desc.kind = nITKindBombHei;
    it_desc.p_file = &file_ptr;
    it_desc.attack_state = nGMAttackStateNew;
    it_desc.proc_update = test_it_make_item_proc_update;
    it_desc.proc_map = test_it_make_item_proc_map;

    item_gobj = itManagerMakeItem(NULL, &it_desc, &it_pos, &it_vel, 0);
    CHECK(item_gobj != NULL);
    ip = itGetStruct(item_gobj);
    CHECK(ip->type == nITTypeThrow);

    fp.item_gobj = item_gobj;

    /* nITTypeThrow (Bob-Omb): this function only reads item_throw_wait
     * for Damage/Throw items, it never increments it itself -- that only
     * happens on the Shoot/non-multi fallthrough below. */
    fp.computer.item_throw_wait = 0;
    ftComputerFollowObjectiveUseItem(fpp);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputThrowItemImmediate]);
    CHECK(fp.computer.item_throw_wait == 0);

    fp.computer.item_throw_wait = 3;
    ftComputerFollowObjectiveUseItem(fpp);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputThrowItemWait]);
    CHECK(fp.computer.item_throw_wait == 0);

    /* Poké Ball: always the long-wait throw, regardless of the counter. */
    ip->kind = nITKindMBall;
    fp.computer.item_throw_wait = 0;
    ftComputerFollowObjectiveUseItem(fpp);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputThrowItemWait]);
    CHECK(fp.computer.item_throw_wait == 0);

    /* nITTypeShoot, multi == 0 (a spent single-shot gun): falls through
     * to the same throw logic, but increments item_throw_wait itself
     * first. */
    ip->kind = nITKindLGun;
    ip->type = nITTypeShoot;
    ip->multi = 0;

    fp.computer.item_throw_wait = 0;
    ftComputerFollowObjectiveUseItem(fpp);
    CHECK(fp.computer.item_throw_wait == 1);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputThrowItemImmediate]);

    ftComputerFollowObjectiveUseItem(fpp);
    ftComputerFollowObjectiveUseItem(fpp);
    CHECK(fp.computer.item_throw_wait == 0);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputThrowItemWait]);

    /* nITTypeShoot, multi != 0 (Ray Gun with ammo left): aims at a real
     * target via ftComputerCheckFindTarget instead of just throwing. */
    ip->multi = 5;
    spawn_second(200.0f, 0.0f, -1);

    fp.computer.item_throw_wait = 7;
    ftComputerFollowObjectiveUseItem(fpp);
    CHECK((void *)fp.computer.target_user == (void *)fpp2);
    CHECK((void *)fp.computer.p_command ==
          (void *)dFTComputerPlayerInputScripts[nFTComputerInputStickTiltAutoXButtonBZReleaseAPress]);
    CHECK(fp.computer.item_throw_wait == 0);

    /* level >= 5 against a Ness/Fox target: skip aiming and throw
     * immediately -- and, faithfully, skip the item_throw_wait reset
     * too, since that early return sits above it in the decomp. */
    fp.level = 5;
    fp2.fkind = nFTKindFox;
    fp.computer.item_throw_wait = 9;

    ftComputerFollowObjectiveUseItem(fpp);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputThrowItemImmediate]);
    CHECK(fp.computer.item_throw_wait == 9);

    fp.level = 1;
    fp2.fkind = nFTKindMario;

    /* Fire Flower, misaligned (target behind fp, which faces +x):
     * falls back to Walk instead of releasing. */
    ip->kind = nITKindFFlower;
    DObjGetStruct(mock_gobj2)->translate.vec.f.x = -200.0f;

    ftComputerFollowObjectiveUseItem(fpp);
    ftComputerUpdateInputs(fpp);
    CHECK(fp.input.cp.stick_range.x < 0);
    CHECK(fp.computer.item_throw_wait == 0);

    /* vertical distance alone (>= 400) falls back to Walk regardless of
     * item kind -- still Fire Flower here, on purpose. */
    DObjGetStruct(mock_gobj2)->translate.vec.f.x = 200.0f;
    DObjGetStruct(mock_gobj2)->translate.vec.f.y = 1000.0f;

    ftComputerFollowObjectiveUseItem(fpp);
    CHECK(fp.computer.item_throw_wait == 0);

    gITManagerStructsAllocFree = save_free;
    ftManagerDestroyFighter(mock_gobj2);
    mock_gobj2 = NULL;
    fpp2 = NULL;
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* ftComputerFollowObjectiveAttack, func_ovl3_801397F4,
 * ftComputerFollowObjectiveAlly and ftComputerFollowObjectivePatrol
 * (ftcomputer.c) -- Attack's own near-mirror cluster, all four sharing
 * func_ovl3_8013837C's outer three-way gate. Exercises that shared gate
 * once per outcome across all four (a live SpecialN charge is a
 * complete no-op past the gate save for func_ovl3_8013837C's own cancel
 * side effect; no opponent at all falls back to func_ovl3_8013877C's own
 * idle counter; a real engaged opponent reaches the cluster's own branch
 * logic), then two things that genuinely differ between them: a
 * successful ftComputerCheckDetectTarget always clears unk_ftcom_0x35
 * for all four, but only Attack and Patrol additionally clear
 * unk_ftcom_0x20 (func_ovl3_801397F4 and Ally don't touch it on this
 * path at all); and the shared func_ovl3_80138AA8 lunge short-circuit on
 * the out-of-range arm, forced deterministic the same way
 * test_computer_lunge_attack already established (unk_ftcom_0x35's own
 * >=5 frequency cap). Ally's own -0.5F detect bias is exercised at a
 * much closer range (50 units, against the others' 200) since a
 * negative bias is the one case this file hasn't already proven
 * reliable at 200 -- not separately probing Ally's unique 1% wander-roll
 * threshold below that, or Attack/Patrol's 5% one, since pinning an RNG
 * draw more precisely than that needs more than this test gives. */
static void test_computer_objective_engage(void)
{
    void (*const objectives[])(FTStruct *) =
    {
        ftComputerFollowObjectiveAttack,
        func_ovl3_801397F4,
        ftComputerFollowObjectiveAlly,
        ftComputerFollowObjectivePatrol,
    };
    int i;

    spawn(0.0f, 0.0f);
    spawn_second(200.0f, 0.0f, -1);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    /* a live SpecialN charge (temp_v0 == -1): nothing past
     * func_ovl3_8013837C's own gate runs. */
    fp.fkind = nFTKindDonkey;
    fp.status_id = nFTDonkeyStatusSpecialNStart;

    for (i = 0; i < 4; i++)
    {
        fp.computer.fighter_follow_since = 5;
        fp.computer.fighter_follow_wait = 5;
        fp.computer.unk_ftcom_0x35 = 77;

        objectives[i](fpp);

        CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[0xC]);
        CHECK(fp.computer.unk_ftcom_0x35 == 77);
    }

    fp.fkind = nFTKindMario;
    fp.status_id = nFTCommonStatusWait;

    /* no opponent worth engaging (temp_v0 == 0): a same-team fp2 is
     * never a candidate, the same trick test_computer_target_search
     * already uses -- keeps fp2 fully spawned and frame()-settled
     * instead of destroying and re-creating it (a fresh instance never
     * run through a real frame() left CheckDetectTarget/func_ovl3_80138AA8
     * unable to find it properly below, caught by this test's own first
     * draft failing there). Falls back to func_ovl3_8013877C for every
     * one of them -- its own idle counter increments unconditionally,
     * independent of its 30-tick action threshold. */
    fp2.team = fp.team;

    for (i = 0; i < 4; i++)
    {
        fp.computer.fighter_follow_since = 0;
        fp.computer.target_gobj = NULL;
        fp.computer.walk_stop_wait = 5;

        objectives[i](fpp);

        CHECK(fp.computer.walk_stop_wait == 6);
    }

    fp2.team = 1;

    /* a real, close opponent -- everything past this point is the
     * cluster's own in-range/out-of-range branch logic. Below level 3,
     * func_ovl3_80138EE4's own escape-roll chance (checked before
     * CheckDetectTarget on every one of these) can never fire regardless
     * of the RNG draw -- this test's first draft left the spawn default
     * (level 5) and got an occasional real dodge-roll instead of the
     * detect success it meant to force, since that chance is a genuine
     * ~17% per call at that level, not Donkey-family-gated the way
     * func_ovl3_80138AA8's own allowlist is. */
    fp.level = 3;

    fp.computer.fighter_follow_since = 5;
    fp.computer.fighter_follow_wait = 5;
    fp.computer.unk_ftcom_0x35 = 42;
    fp.computer.unk_ftcom_0x20 = 42;
    ftComputerFollowObjectiveAttack(fpp);
    CHECK(fp.computer.unk_ftcom_0x35 == 0);
    CHECK(fp.computer.unk_ftcom_0x20 == 0);

    fp.computer.fighter_follow_since = 5;
    fp.computer.fighter_follow_wait = 5;
    fp.computer.unk_ftcom_0x35 = 42;
    fp.computer.unk_ftcom_0x20 = 42;
    func_ovl3_801397F4(fpp);
    CHECK(fp.computer.unk_ftcom_0x35 == 0);
    CHECK(fp.computer.unk_ftcom_0x20 == 42);

    fp.computer.fighter_follow_since = 5;
    fp.computer.fighter_follow_wait = 5;
    fp.computer.unk_ftcom_0x35 = 42;
    fp.computer.unk_ftcom_0x20 = 42;
    ftComputerFollowObjectivePatrol(fpp);
    CHECK(fp.computer.unk_ftcom_0x35 == 0);
    CHECK(fp.computer.unk_ftcom_0x20 == 0);

    /* Ally's own -0.5F bias, at a much closer range for the same
     * reliability margin the others get at 200 units with 0.0F/2.0F. */
    DObjGetStruct(mock_gobj2)->translate.vec.f.x = 50.0f;

    fp.computer.fighter_follow_since = 5;
    fp.computer.fighter_follow_wait = 5;
    fp.computer.unk_ftcom_0x35 = 42;
    fp.computer.unk_ftcom_0x20 = 42;
    ftComputerFollowObjectiveAlly(fpp);
    CHECK(fp.computer.unk_ftcom_0x35 == 0);
    CHECK(fp.computer.unk_ftcom_0x20 == 42);

    /* a far (but still on-camera -- ftComputerCheckFindTarget's own
     * gMPCollisionBounds.current gate would otherwise lose the target
     * outright, a trap this test's first draft hit at 5000 units),
     * vertically-aligned opponent: past the follow-timer's own
     * (random*300)+1200 <= 1500 reach, with unk_ftcom_0x35 already at
     * its lunge-frequency cap so func_ovl3_80138AA8 deterministically
     * short-circuits to TRUE (the ">= 5, hand off to Walk" arm
     * test_computer_lunge_attack already exercises directly) -- shared
     * by all four. */
    DObjGetStruct(mock_gobj2)->translate.vec.f.x = 1600.0f;
    fp.level = 1;

    for (i = 0; i < 4; i++)
    {
        fp.computer.fighter_follow_since = 5;
        fp.computer.fighter_follow_wait = 5;
        fp.computer.unk_ftcom_0x35 = 5;
        fp.computer.walk_stop_wait = 99;

        objectives[i](fpp);

        CHECK(fp.computer.walk_stop_wait == 0);
    }

    ftManagerDestroyFighter(mock_gobj2);
    mock_gobj2 = NULL;
    fpp2 = NULL;
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* ftComputerFollowObjectiveRush (ftcomputer.c) --
 * structurally the odd one out of the objective cluster: it calls
 * ftComputerCheckFindTarget directly rather than going through
 * func_ovl3_8013837C's follow-timer gate (so it re-searches every tic,
 * with no charge/cancel short-circuit at all), never touches
 * unk_ftcom_0x35/unk_ftcom_0x20 anywhere in its own body, and its
 * out-of-range arm is the decomp's own `else if (TRUE)` dead
 * conditional -- kept as written, not simplified to a plain `else`. */
static void test_computer_objective_rush(void)
{
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    /* no opponent at all: falls back to func_ovl3_8013877C, the same
     * idle counter every other objective in the cluster shares. */
    fp.computer.walk_stop_wait = 5;
    ftComputerFollowObjectiveRush(fpp);
    CHECK(fp.computer.walk_stop_wait == 6);

    /* a real, close opponent, forced through
     * ftComputerCheckDetectTarget's own reliable-at-200-units success
     * path -- Rush never references unk_ftcom_0x35/0x20 at all, unlike
     * every other function in the cluster. */
    spawn_second(200.0f, 0.0f, -1);

    fp.computer.walk_stop_wait = 5;
    fp.computer.unk_ftcom_0x35 = 42;
    fp.computer.unk_ftcom_0x20 = 42;

    ftComputerFollowObjectiveRush(fpp);

    CHECK(fp.computer.walk_stop_wait == 0);
    CHECK(fp.computer.unk_ftcom_0x35 == 42);
    CHECK(fp.computer.unk_ftcom_0x20 == 42);

    /* a far (but still on-camera, per test_computer_objective_engage's
     * own note on ftComputerCheckFindTarget's gMPCollisionBounds gate)
     * opponent: the out-of-range arm always calls Walk, with no
     * func_ovl3_80138AA8 lunge short-circuit at all (unlike the rest of
     * the cluster). */
    DObjGetStruct(mock_gobj2)->translate.vec.f.x = 1600.0f;

    fp.computer.walk_stop_wait = 5;

    ftComputerFollowObjectiveRush(fpp);

    CHECK(fp.computer.walk_stop_wait == 0);

    ftManagerDestroyFighter(mock_gobj2);
    mock_gobj2 = NULL;
    fpp2 = NULL;
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* ftComputerGetObjectiveStatus (ftcomputer.c) -- the
 * status-override cascade every real Proc* function calls first (all
 * six). A representative sample of the
 * cascade's branches, not exhaustive: grJungleTaruCannGetPosition/
 * GetRotate's own branch is skipped entirely -- it dereferences a real
 * DObj through gGRCommonStruct.jungle.tarucann_gobj, which nothing in
 * this host test builds, and this test's own default setup keeps
 * status_id away from nFTCommonStatusTaruCann so that branch is simply
 * never reached rather than crashing. Genuinely probabilistic branches
 * (the escape-roll half of DownWaitD, the YoshiTeam/KirbyTeam/PolyTeam
 * skip chance's "does not skip" side, the DamageFall counter-attack
 * roll's "hits" side) are checked for the invariant that must hold
 * either way, not pinned to one outcome, the same standard the rest of
 * this file already holds RNG-gated branches to. */
static void test_computer_objective_status(void)
{
    GRStruct save_gr;
    s32 save_gkind = gSCManagerBattleState->gkind;
    s32 result;
    int i;

    save_gr = gGRCommonStruct;

    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    /* CliffWait: a low fall_wait leaves action_wait comfortably past
     * even level 9's own (near-zero) threshold, so the branch is
     * entered regardless of the 1% double-up roll; which of the three
     * scripts fires past that is a real 40/30/30 split this test treats
     * as an invariant across repeated calls, not one pinned outcome. */
    fp.level = 9;
    fp.status_id = nFTCommonStatusCliffWait;
    fp.status_vars.common.cliffwait.fall_wait = 0;

    for (i = 0; i < 30; i++)
    {
        result = ftComputerGetObjectiveStatus(mock_gobj);
        CHECK(result == 0);
        CHECK
        (
            ((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[0x28])                          ||
            ((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputButtonZ1])      ||
            ((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputMoveAutoStickTiltHiReleaseZ])
        );
    }

    /* DownWaitD, GDonkey (skips the escape-roll branch entirely,
     * deterministic): a low stand_wait leaves action_wait past level 9's
     * own threshold, and GDonkey's own exclusion means the only possible
     * result is the plain stand-up script. */
    fp.fkind = nFTKindGDonkey;
    fp.status_id = nFTCommonStatusDownWaitD;
    fp.status_vars.common.downwait.stand_wait = 0;

    result = ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(result == 0);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputStickNButtonBZReleaseAPress]);

    /* DownWaitD, Mario, a real close opponent: the escape-roll branch is
     * now live (fkind != GDonkey, target_dist < 800, level >= 4), a real
     * ~50/50 split between an early escape and the plain stand-up
     * script -- both are checked as the invariant, not one pinned. */
    fp.fkind = nFTKindMario;
    spawn_second(200.0f, 0.0f, -1);

    for (i = 0; i < 30; i++)
    {
        fp.status_id = nFTCommonStatusDownWaitD;
        fp.status_vars.common.downwait.stand_wait = 0;

        result = ftComputerGetObjectiveStatus(mock_gobj);
        CHECK(result == 0);
        CHECK
        (
            ((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputEscapeL])              ||
            ((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputEscapeR])              ||
            ((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputStickNButtonBZReleaseAPress])
        );
    }

    ftManagerDestroyFighter(mock_gobj2);
    mock_gobj2 = NULL;
    fpp2 = NULL;

    /* Ottotto: plain, deterministic. */
    fp.status_id = nFTCommonStatusOttottoWait;

    result = ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(result == 0);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputMoveAutoStickTiltHiReleaseZ]);

    /* CatchWait: which side of center picks Smash L vs R. */
    fp.status_id = nFTCommonStatusCatchWait;
    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = -100.0f;

    result = ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(result == 0);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputStickSmashL]);

    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = 100.0f;

    result = ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(result == 0);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputStickSmashR]);

    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = 0.0f;

    /* FuraFura: level 9's own threshold is 0, so target_find_wait's very
     * first post-increment value (1) already exceeds it -- one call is
     * enough. */
    fp.status_id = nFTCommonStatusFuraFura;
    fp.computer.target_find_wait = 0;

    result = ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(result == 0);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputWiggle]);

    /* leaving that status resets the counter back to 0. */
    fp.status_id = nFTCommonStatusWait;
    (void)ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(fp.computer.target_find_wait == 0);

    /* Kirby's own copy-ability eat/spit window. */
    fp.fkind = nFTKindKirby;
    fp.status_id = nFTKirbyStatusSpecialNCatch;

    result = ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(result == 0);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputStickN]);

    fp.status_id = nFTKirbyStatusSpecialNWait;

    result = ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(result == 0);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputStickSmashLwButtonB]);

    fp.fkind = nFTKindMario;
    fp.status_id = nFTCommonStatusWait;

    /* Ness's own PK Thunder self-aim, off-camera: func_ovl2_800F8FFC
     * reads FALSE far enough outside every registered floor line's own
     * x-span (this test's first draft used a huge Y instead, which
     * turned out to read TRUE -- func_ovl2_800F8FFC is a "is there a
     * floor roughly under here" projection, not a camera-bounds check,
     * so a huge Y over real stage X still finds one; a huge X finds
     * none), aiming target_pos by the fixed +/-200 offset formula
     * instead of a real search. */
    fp.fkind = nFTKindNess;
    fp.status_id = nFTNessStatusSpecialHiHold;
    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = 50000.0f;
    fp.joints[nFTPartsJointTopN]->translate.vec.f.y = 0.0f;

    result = ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(result == 0);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputNessSpecialHiAim]);
    CHECK_EQF(fp.computer.target_pos.x, 50200.0f);
    CHECK_EQF(fp.computer.target_pos.y, -100.0f);

    fp.fkind = nFTKindMario;
    fp.status_id = nFTCommonStatusWait;
    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = 0.0f;
    fp.joints[nFTPartsJointTopN]->translate.vec.f.y = 0.0f;

    /* the "stuck standing" escalation: stand_stop_wait increments once
     * per call while grounded and unmoved, and only the call where it
     * first exceeds 300 both sets is_stop_stand AND acts on it in that
     * same call (the is_stop_stand check sits right after the counter
     * logic, not on the next call) -- so exactly 300 calls read -1, and
     * the 301st reads 0. */
    fp.computer.stand_pos.x = fp.joints[nFTPartsJointTopN]->translate.vec.f.x;
    fp.computer.stand_pos.y = fp.joints[nFTPartsJointTopN]->translate.vec.f.y;
    fp.computer.stand_stop_wait = 0;
    fp.computer.is_stop_stand = FALSE;
    fp.computer.behavior = nFTComputerBehaviorDefault;

    for (i = 0; i < 300; i++)
    {
        result = ftComputerGetObjectiveStatus(mock_gobj);
        CHECK(result == -1);
    }
    CHECK(fp.computer.is_stop_stand == FALSE);

    result = ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(result == 0);
    CHECK(fp.computer.is_stop_stand != FALSE);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputMoveAutoStickTiltHiReleaseZ]);

    fp.computer.is_stop_stand = FALSE;
    fp.computer.stand_stop_wait = 0;

    /* the percent_damage skip chance: at level 1 the YoshiTeam roll's
     * own left-hand side is negative, so it is always <= a
     * [0,1)-ranged draw -- a guaranteed skip (-1), before any of the
     * later Recover/counter-attack logic below ever runs. */
    fp.percent_damage = 1;
    fp.computer.behavior = nFTComputerBehaviorYoshiTeam;
    fp.level = 1;

    result = ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(result == -1);

    fp.percent_damage = 0;
    fp.computer.behavior = nFTComputerBehaviorDefault;

    /* Hyrule's own collapsing-platform pull-to-Recover, off-camera: the
     * same far-off-X position used for Ness's own off-camera aim above
     * reads func_ovl2_800F8FFC FALSE here too, satisfying the branch's
     * first disjunct regardless of gkind/floor_line_id. */
    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = 50000.0f;
    fp.joints[nFTPartsJointTopN]->translate.vec.f.y = 0.0f;
    fp.computer.objective = nFTComputerObjectiveWalk;

    result = ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(result == 1);
    CHECK(fp.computer.objective == nFTComputerObjectiveRecover);

    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = 0.0f;
    fp.joints[nFTPartsJointTopN]->translate.vec.f.y = 0.0f;
    fp.computer.objective = nFTComputerObjectiveWalk;

    /* Zebes' own rising acid, on-camera: gGRCommonStruct.zebes's own
     * globals are plain BSS state grZebesAcidGetLevelInfo reads
     * directly, no stage load needed to drive them. acid_status is left
     * at its zeroed default (nGRZebesAcidStatusWait, private to
     * src/dc/grzebes.c, not worth exposing just for this) -- the
     * function's own step output is 0.0F either way when the status
     * isn't Rise, and this test's curr+500 threshold alone is already
     * satisfied regardless of step. */
    gSCManagerBattleState->gkind = nGRKindZebes;
    gGRCommonStruct.zebes.acid_level_curr = 0.0f;

    result = ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(result == 1);
    CHECK(fp.computer.objective == nFTComputerObjectiveRecover);

    gSCManagerBattleState->gkind = save_gkind;
    gGRCommonStruct = save_gr;
    fp.computer.objective = nFTComputerObjectiveWalk;

    /* the DamageFall counter-attack roll: the flag latches TRUE on the
     * very first qualifying call regardless of level, and the roll past
     * it (level >= 3) is a real chance this test checks both sides of --
     * "never fires below level 3" is the deterministic half, "can fire"
     * above it the statistical one. */
    fp.status_id = nFTCommonStatusDamageFall;
    fp.physics.vel_air.y = 0.0f;
    fp.coll_data.floor_dist = 0.0f;
    fp.computer.ftcom_flags_0x49_b3 = FALSE;
    fp.level = 1;

    result = ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(result == -1);
    CHECK(fp.computer.ftcom_flags_0x49_b3 != FALSE);

    fp.level = 9;

    for (i = 0; i < 30; i++)
    {
        fp.computer.ftcom_flags_0x49_b3 = FALSE;
        fp.computer.is_counterattack = FALSE;
        fp.computer.objective = nFTComputerObjectiveWalk;

        result = ftComputerGetObjectiveStatus(mock_gobj);
        CHECK((result == -1) || (result == 1));

        if (result == 1)
        {
            CHECK(fp.computer.objective == nFTComputerObjectiveCounterAttack);
            CHECK(fp.computer.is_counterattack != FALSE);
        }
    }

    fp.status_id = nFTCommonStatusWait;
    fp.computer.ftcom_flags_0x49_b3 = FALSE;

    /* the appeal-attempt taunt-wander timeout: a fresh attack landed
     * hard enough (attack_knockback > 160) arms a 60-tick window: past
     * 2500 units from the target while idling, it fires the taunt
     * script and clears the window immediately, rather than waiting out
     * the full 60. */
    fp.computer.attack_count = 0;
    fp.attack_count = 1;
    fp.attack_knockback = 200.0f;
    fp.computer.target_dist = 3000.0f;

    result = ftComputerGetObjectiveStatus(mock_gobj);
    CHECK(result == 0);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputStickNButtonL]);
    CHECK(fp.computer.appeal_attempt_frames == 0);

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* ftComputerSetFighterDamageDetectSize (ftcomputer.c) --
 * measures the fighter's own real, spawned hurtbox extents into
 * fp->damage_coll_size. Exact numbers depend on Mario's real hurtbox
 * geometry (not hand-derivable the way pure field arithmetic is), so
 * this checks the invariant that must hold regardless: both dimensions
 * come out positive and finite, proving the real geometry walk ran
 * (and, wired into ftmanager.c's own real call site, that
 * spawn() itself already produces one -- see the CHECK right after
 * spawn() below, before this test calls the function again by hand). */
static void test_computer_damage_detect_size(void)
{
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    /* wired into ftManagerMakeFighter for every Man/Com fighter
     * -- already real by the time spawn() returns, not just after
     * calling the function again below. */
    CHECK(fp.damage_coll_size.x > 0.0f);
    CHECK(fp.damage_coll_size.y > 0.0f);

    fp.damage_coll_size.x = -1.0f;
    fp.damage_coll_size.y = -1.0f;

    ftComputerSetFighterDamageDetectSize(mock_gobj);

    CHECK(fp.damage_coll_size.x > 0.0f);
    CHECK(fp.damage_coll_size.y > 0.0f);

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* ftComputerCheckFindItem (ftcomputer.c) -- the nearest
 * reachable item search ftComputerProcDefault's own TrackItem trigger
 * needs. Follows test_computer_track_item's own standalone-item-GObj
 * pattern. */
static void test_computer_find_item(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    ITStruct *save_free = gITManagerStructsAllocFree;
    ITAttributes it_attr;
    ITDesc it_desc;
    void *file_ptr;
    Vec3f it_pos = { 300.0f, 0.0f, 0.0f };
    Vec3f it_vel = { 0.0f, 0.0f, 0.0f };
    GObj *item_gobj;
    ITStruct *ip;
    sb32 result;
    int i;

    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
        pool[i].next = &pool[i + 1];
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    memset(&it_attr, 0, sizeof(it_attr));
    it_attr.data = NULL;
    it_attr.type = nITTypeThrow;
    it_attr.weight = 1;

    file_ptr = &it_attr;
    memset(&it_desc, 0, sizeof(it_desc));
    it_desc.kind = nITKindBombHei;
    it_desc.p_file = &file_ptr;
    it_desc.attack_state = nGMAttackStateNew;
    it_desc.proc_update = test_it_make_item_proc_update;
    it_desc.proc_map = test_it_make_item_proc_map;

    item_gobj = itManagerMakeItem(NULL, &it_desc, &it_pos, &it_vel, 0);
    CHECK(item_gobj != NULL);
    ip = itGetStruct(item_gobj);
    ip->is_allow_pickup = TRUE;

    /* already holding something: the search doesn't even run. */
    fp.item_gobj = item_gobj;

    result = ftComputerCheckFindItem(fpp);
    CHECK(result == FALSE);
    CHECK(fp.computer.target_line_id == -1);

    fp.item_gobj = NULL;

    /* empty-handed, a real pick-up-able item on camera and in bounds:
     * found. */
    result = ftComputerCheckFindItem(fpp);
    CHECK(result != FALSE);
    CHECK((void *)fp.computer.target_user == (void *)ip);
    CHECK_EQF(fp.computer.target_pos.x, 300.0f);
    CHECK(fp.computer.target_dist > 0.0f);

    /* not allowed to be picked up right now: never a candidate. */
    ip->is_allow_pickup = FALSE;

    result = ftComputerCheckFindItem(fpp);
    CHECK(result == FALSE);

    /* same-team item, team battle with no team-attack: never a
     * candidate either, the same shape ftComputerCheckFindTarget's own
     * team gate already uses (test_computer_target_search). */
    ip->is_allow_pickup = TRUE;
    ip->team = fp.team;
    gSCManagerBattleState->is_team_battle = TRUE;
    gSCManagerBattleState->is_team_attack = FALSE;

    result = ftComputerCheckFindItem(fpp);
    CHECK(result == FALSE);

    gSCManagerBattleState->is_team_battle = FALSE;

    /* this item outlives the test (the established pattern here never
     * destroys a standalone item GObj, only restores the free-list) --
     * leave it a non-candidate for whatever later test's own
     * CheckFindItem call comes next, the same way it started. */
    ip->is_allow_pickup = FALSE;

    gITManagerStructsAllocFree = save_free;
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* func_ovl3_80135B78 (ftcomputer.c) -- is a predicted
 * incoming attack worth reacting to right now, ftComputerProcDefault's
 * own CounterAttack trigger. Exercised through a standalone item's own
 * live attack_coll (the weapon path is the identical shape against
 * WPAttackColl/WPStruct instead and isn't separately exercised here,
 * the same call this file already makes for symmetric fighter-kind
 * branches elsewhere). Mario's own default fkind keeps is_opponent_ra
 * out of play for the base case; a second call with fkind forced to Fox
 * proves that flag's own latch. */
static void test_computer_predict_attack(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    ITStruct *save_free = gITManagerStructsAllocFree;
    ITAttributes it_attr;
    ITDesc it_desc;
    void *file_ptr;
    Vec3f it_pos = { 1000.0f, 0.0f, 0.0f };
    Vec3f it_vel = { 0.0f, 0.0f, 0.0f };
    GObj *item_gobj;
    ITStruct *ip;
    sb32 result;
    f32 top_y;
    s32 i;

    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
        pool[i].next = &pool[i + 1];
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    fp.damage_coll_size.x = 50.0f;
    fp.damage_coll_size.y = 50.0f;
    fp.level = 9;

    /* the real, spawned top-joint height, not a guessed constant --
     * the y-window below is centered on it so the check passes
     * regardless of Mario's own standing pose. */
    top_y = fp.joints[nFTPartsJointTopN]->translate.vec.f.y;

    memset(&it_attr, 0, sizeof(it_attr));
    it_attr.data = NULL;
    it_attr.type = nITTypeThrow;
    it_attr.weight = 1;

    file_ptr = &it_attr;
    memset(&it_desc, 0, sizeof(it_desc));
    it_desc.kind = nITKindBombHei;
    it_desc.p_file = &file_ptr;
    it_desc.attack_state = nGMAttackStateNew;
    it_desc.proc_update = test_it_make_item_proc_update;
    it_desc.proc_map = test_it_make_item_proc_map;

    item_gobj = itManagerMakeItem(NULL, &it_desc, &it_pos, &it_vel, 0);
    CHECK(item_gobj != NULL);
    ip = itGetStruct(item_gobj);

    ip->owner_gobj = NULL;
    ip->team = 1;
    ip->lr = -1;
    ip->physics.vel_air.x = -100.0f;
    ip->attack_coll.attack_state = nGMAttackStateInterpolate;
    ip->attack_coll.interact_mask = GMHITCOLLISION_FLAG_FIGHTER;
    ip->attack_coll.attack_count = 1;
    /* size=400 -> attack_size=200: still leaves predict_pos_x = 1000 -
     * (50+200) = 750 and predict_div_x = 750/100 = 7.5 (< 15), while
     * the y-window (top_y +/- 200, minus the fighter's own 50-unit
     * damage_coll_size on the low side) comfortably contains top_y
     * itself regardless of Mario's real standing height. */
    ip->attack_coll.size = 400.0f;
    ip->attack_coll.attack_pos[0].pos_curr.x = 1000.0f;
    ip->attack_coll.attack_pos[0].pos_curr.y = top_y;

    /* on a real closing course (item moving left toward the fighter at
     * x=0, well inside 15 tics), Mario's own default fkind never touches
     * is_opponent_ra. */
    fp.computer.is_opponent_ra = FALSE;

    result = func_ovl3_80135B78(fpp);
    CHECK(result != FALSE);
    CHECK(fp.computer.is_opponent_ra == FALSE);
    CHECK_EQF(fp.computer.target_pos.y, top_y);

    /* Fox (or Ness/NFox/NNess) latches is_opponent_ra on the same real
     * detection. */
    fp.fkind = nFTKindFox;
    fp.computer.is_opponent_ra = FALSE;

    result = func_ovl3_80135B78(fpp);
    CHECK(result != FALSE);
    CHECK(fp.computer.is_opponent_ra != FALSE);

    fp.fkind = nFTKindMario;

    /* moving away instead of closing: never a candidate. */
    ip->physics.vel_air.x = 100.0f;

    result = func_ovl3_80135B78(fpp);
    CHECK(result == FALSE);

    gITManagerStructsAllocFree = save_free;
    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

static void test_computer_proc_default(void)
{
    s32 result;

    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    /* ftComputerGetObjectiveStatus's own 0/1 short-circuits (already
     * tested above) pass straight back out of this function
     * without reaching any of its own logic below -- Ottotto is a
     * deterministic 0, the same far-off-X "off camera" position the
     * GetObjectiveStatus test used is a deterministic 1 (Recover). */
    fp.status_id = nFTCommonStatusOttottoWait;
    result = ftComputerProcDefault(mock_gobj);
    CHECK(result == 0);

    fp.status_id = nFTCommonStatusWait;
    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = 50000.0f;
    result = ftComputerProcDefault(mock_gobj);
    CHECK(result != FALSE);
    CHECK(fp.computer.objective == nFTComputerObjectiveRecover);
    fp.joints[nFTPartsJointTopN]->translate.vec.f.x = 0.0f;
    fp.computer.objective = nFTComputerObjectiveWalk;

    /* Fox/Ness's own SpecialLw charge-scope freeze: a status range this
     * function alone checks (GetObjectiveStatus doesn't gate on it). */
    fp.fkind = nFTKindFox;
    fp.status_id = nFTFoxStatusSpecialLwScopeStart;
    result = ftComputerProcDefault(mock_gobj);
    CHECK(result == FALSE);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[nFTComputerInputStickNButtonBRelease]);

    fp.fkind = nFTKindMario;
    fp.status_id = nFTCommonStatusWait;

    /* the shield-release check: any Guard-range status. Also this
     * function's own literal magic number (the decomp's own 0x24,
     * kept verbatim like the port's existing 0xB/0xC precedent) --
     * nFTComputerInputButtonZRelease by name, confirmed by counting
     * the enum, but never spelled that way in either the decomp or
     * the port. */
    fp.status_id = nFTCommonStatusGuardStart;
    result = ftComputerProcDefault(mock_gobj);
    CHECK(result == FALSE);
    CHECK((void *)fp.computer.p_command == (void *)dFTComputerPlayerInputScripts[0x24]);

    fp.status_id = nFTCommonStatusWait;

    /* a close real opponent (< 350) always wins Attack, ahead of
     * anything item-related below. */
    spawn_second(200.0f, 0.0f, -1);
    result = ftComputerProcDefault(mock_gobj);
    CHECK(result != FALSE);
    CHECK(fp.computer.objective == nFTComputerObjectiveAttack);
    ftManagerDestroyFighter(mock_gobj2);
    mock_gobj2 = NULL;
    fpp2 = NULL;

    /* holding a Damage/Shoot/Throw item routes to UseItem; anything
     * else (e.g. a Swing item) falls back to Attack. Both only apply
     * once fp->item_gobj is set -- the fighter actually holding
     * something -- independent of CheckFindItem's own ground search,
     * so this item is deliberately not_allow_pickup: it must never be
     * a candidate for that search, only reachable through the direct
     * item_gobj pointer this block drives by hand. */
    {
        static ITStruct pool[ITEM_ALLOC_MAX];
        ITStruct *save_free = gITManagerStructsAllocFree;
        ITAttributes it_attr;
        ITDesc it_desc;
        void *file_ptr;
        Vec3f it_pos = { 0.0f, 0.0f, 0.0f };
        Vec3f it_vel = { 0.0f, 0.0f, 0.0f };
        GObj *item_gobj;
        ITStruct *ip;
        s32 i;

        for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
            pool[i].next = &pool[i + 1];
        pool[ITEM_ALLOC_MAX - 1].next = NULL;
        gITManagerStructsAllocFree = pool;

        memset(&it_attr, 0, sizeof(it_attr));
        it_attr.data = NULL;
        it_attr.type = nITTypeThrow;
        it_attr.weight = 1;

        file_ptr = &it_attr;
        memset(&it_desc, 0, sizeof(it_desc));
        it_desc.kind = nITKindBombHei;
        it_desc.p_file = &file_ptr;
        it_desc.attack_state = nGMAttackStateNew;
        it_desc.proc_update = test_it_make_item_proc_update;
        it_desc.proc_map = test_it_make_item_proc_map;

        item_gobj = itManagerMakeItem(NULL, &it_desc, &it_pos, &it_vel, 0);
        CHECK(item_gobj != NULL);
        ip = itGetStruct(item_gobj);
        ip->is_allow_pickup = FALSE;

        fp.item_gobj = item_gobj;
        ip->type = nITTypeThrow;

        result = ftComputerProcDefault(mock_gobj);
        CHECK(result != FALSE);
        CHECK(fp.computer.objective == nFTComputerObjectiveUseItem);

        ip->type = nITTypeSwing;
        result = ftComputerProcDefault(mock_gobj);
        CHECK(result != FALSE);
        CHECK(fp.computer.objective == nFTComputerObjectiveAttack);

        fp.item_gobj = NULL;
        gITManagerStructsAllocFree = save_free;
    }

    /* nothing to react to at all (no opponent, no item): falls back to
     * objective_base. Confirmed empty-handed first (a real bug in
     * test_computer_find_item once left its
     * own item on the stage with is_allow_pickup still TRUE -- fixed
     * there rather than worked around here) so this is a real exercise
     * of the fallback branch, not an accident of whichever branch a
     * stray leftover item happened to hit. */
    fp.computer.item_track_wait = 0;
    fp.computer.objective_base = nFTComputerObjectiveAlly;
    CHECK(ftComputerCheckFindTarget(fpp) == FALSE);
    CHECK(ftComputerCheckFindItem(fpp) == FALSE);
    CHECK(fp.item_gobj == NULL);
    result = ftComputerProcDefault(mock_gobj);
    CHECK(result != FALSE);
    CHECK(fp.computer.objective == nFTComputerObjectiveAlly);
    fp.computer.objective_base = nFTComputerObjectiveAttack;

    /* the item-track wait counter and its real per-level threshold:
     * level 9 on a VS-type battle (game_type != 1PGame, this mock's own
     * zero-initialized default) makes track_wait = (-9*25)+225 = 0, so
     * item_track_wait's very first post-increment value (1) already
     * exceeds it -- one call against a real, reachable, pickable item
     * is enough, the same "measure the real threshold, don't guess a
     * call count" approach the idle-patrol test uses. Run
     * last: crossing this threshold makes every later call latch
     * TrackItem again as long as any pickable item remains in range. */
    {
        static ITStruct pool[ITEM_ALLOC_MAX];
        ITStruct *save_free = gITManagerStructsAllocFree;
        ITAttributes it_attr;
        ITDesc it_desc;
        void *file_ptr;
        Vec3f it_pos = { 1000.0f, 0.0f, 0.0f };
        Vec3f it_vel = { 0.0f, 0.0f, 0.0f };
        GObj *item_gobj;
        ITStruct *ip;
        s32 i;

        for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
            pool[i].next = &pool[i + 1];
        pool[ITEM_ALLOC_MAX - 1].next = NULL;
        gITManagerStructsAllocFree = pool;

        fp.level = 9;

        memset(&it_attr, 0, sizeof(it_attr));
        it_attr.data = NULL;
        it_attr.type = nITTypeThrow;
        it_attr.weight = 1;

        file_ptr = &it_attr;
        memset(&it_desc, 0, sizeof(it_desc));
        it_desc.kind = nITKindBombHei;
        it_desc.p_file = &file_ptr;
        it_desc.attack_state = nGMAttackStateNew;
        it_desc.proc_update = test_it_make_item_proc_update;
        it_desc.proc_map = test_it_make_item_proc_map;

        item_gobj = itManagerMakeItem(NULL, &it_desc, &it_pos, &it_vel, 0);
        CHECK(item_gobj != NULL);
        ip = itGetStruct(item_gobj);
        ip->is_allow_pickup = TRUE;

        result = ftComputerProcDefault(mock_gobj);
        CHECK(result != FALSE);
        CHECK(fp.computer.objective == nFTComputerObjectiveTrackItem);
        CHECK(fp.computer.item_track_wait == 1);

        gITManagerStructsAllocFree = save_free;
    }

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}

/* ftcomputer.c:6368-6532 ftComputerProcStand/Walk/Evade/Jump and
 * func_ovl3_80137E70, verbatim: the remaining five `proc_com` targets --
 * a near-mirror cluster, same as the FollowObjective* group.
 * Each is GetObjectiveStatus's own 0/1 short-circuit (already covered
 * above) followed by a few lines of its own:
 * Stand either idles or, waking from Rebirth, hands off to Walk; Walk
 * commits to a fresh wander point once it's within 100 units of the old
 * one, clamped to the floor line's own edges; Evade is a plain
 * objective latch; Jump alternates a straight-up target with a
 * countdown back to the ground; func_ovl3_80137E70 always retargets the
 * fighter's own origin. All four of the non-Evade functions share the
 * same "close enough -> Stand instead of Walk" tail. */
static void test_computer_proc_stand_walk_jump(void)
{
    Vec3f edge_left_pos;
    Vec3f edge_right_pos;
    s32 result;
    int i;

    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    fp.status_id = nFTCommonStatusOttottoWait;
    result = ftComputerProcStand(mock_gobj);
    CHECK(result == 0);
    result = ftComputerProcWalk(mock_gobj);
    CHECK(result == 0);
    result = ftComputerProcEvade(mock_gobj);
    CHECK(result == 0);
    result = ftComputerProcJump(mock_gobj);
    CHECK(result == 0);
    result = func_ovl3_80137E70(mock_gobj);
    CHECK(result == 0);
    fp.status_id = nFTCommonStatusWait;

    /* ProcStand: idles by default, hands off to Walk (toward its own
     * origin/floor line) when waking up from Rebirth. */
    result = ftComputerProcStand(mock_gobj);
    CHECK(result != FALSE);
    CHECK(fp.computer.objective == nFTComputerObjectiveStand);

    fp.status_id = nFTCommonStatusRebirthWait;
    result = ftComputerProcStand(mock_gobj);
    CHECK(result != FALSE);
    CHECK(fp.computer.objective == nFTComputerObjectiveWalk);
    CHECK_EQF(fp.computer.target_pos.x, fp.computer.origin_pos.x);
    CHECK_EQF(fp.computer.target_pos.y, fp.computer.origin_pos.y);
    CHECK(fp.computer.target_line_id == fp.computer.floor_line_id);
    fp.status_id = nFTCommonStatusWait;

    /* ProcEvade: a plain objective latch, nothing else. */
    result = ftComputerProcEvade(mock_gobj);
    CHECK(result != FALSE);
    CHECK(fp.computer.objective == nFTComputerObjectiveEvade);

    /* ProcWalk: already sitting on its own target (within 100 units) ->
     * commits to a fresh wander point along the floor line, clamped to
     * the mock deck's own real edges (hosttest_ft.c's kVpos: the floor
     * runs -2000..2000) rather than a value this test invents. */
    mpCollisionGetFloorEdgeL(fp.computer.floor_line_id, &edge_left_pos);
    mpCollisionGetFloorEdgeR(fp.computer.floor_line_id, &edge_right_pos);

    fp.computer.target_pos.x = fp.joints[nFTPartsJointTopN]->translate.vec.f.x;
    fp.computer.target_pos.y = fp.joints[nFTPartsJointTopN]->translate.vec.f.y;

    for (i = 0; i < 30; i++)
    {
        result = ftComputerProcWalk(mock_gobj);
        CHECK(result != FALSE);
        CHECK((fp.computer.objective == nFTComputerObjectiveWalk) || (fp.computer.objective == nFTComputerObjectiveStand));
        CHECK(fp.computer.target_pos.x >= edge_left_pos.x);
        CHECK(fp.computer.target_pos.x <= edge_right_pos.x);
        CHECK(fp.computer.target_line_id == fp.computer.floor_line_id);

        /* reset back to "already there" so the next iteration re-rolls
         * too, rather than letting one committed point settle Stand. */
        fp.computer.target_pos.x = fp.joints[nFTPartsJointTopN]->translate.vec.f.x;
        fp.computer.target_pos.y = fp.joints[nFTPartsJointTopN]->translate.vec.f.y;
    }

    /* ProcJump: no jump_wait yet -> commits to a straight-up target
     * 1100 units above origin, off the floor line entirely; once
     * jump_wait is running, it counts down and re-targets the ground
     * instead. */
    fp.computer.jump_wait = 0;
    result = ftComputerProcJump(mock_gobj);
    CHECK(result != FALSE);
    CHECK_EQF(fp.computer.target_pos.x, fp.computer.origin_pos.x);
    CHECK_EQF(fp.computer.target_pos.y, fp.computer.origin_pos.y + 1100.0f);
    CHECK(fp.computer.target_line_id == -1);
    CHECK(fp.computer.objective == nFTComputerObjectiveWalk);
    CHECK(fp.computer.jump_wait != 0);

    result = ftComputerProcJump(mock_gobj);
    CHECK(result != FALSE);
    CHECK_EQF(fp.computer.target_pos.y, fp.computer.origin_pos.y);
    CHECK(fp.computer.target_line_id == fp.computer.floor_line_id);

    /* func_ovl3_80137E70: always retargets the fighter's own origin
     * outright -- both axes, not just x like ProcWalk's own settle. */
    fp.computer.target_pos.x = 12345.0f;
    fp.computer.target_pos.y = 6789.0f;

    result = func_ovl3_80137E70(mock_gobj);
    CHECK(result != FALSE);
    CHECK_EQF(fp.computer.target_pos.x, fp.computer.origin_pos.x);
    CHECK_EQF(fp.computer.target_pos.y, fp.computer.origin_pos.y);
    CHECK(fp.computer.target_line_id == fp.computer.floor_line_id);
    CHECK((fp.computer.objective == nFTComputerObjectiveWalk) || (fp.computer.objective == nFTComputerObjectiveStand));

    ftManagerDestroyFighter(mock_gobj);
    mock_gobj = NULL;
    fpp = NULL;
}
