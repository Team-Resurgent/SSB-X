/* hosttest/scripts.c -- part of hosttest_ft.c: the motion-command scripts, GObj threads and the entry sequence.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* ---- the motion-command scripts ------------------------------------
 * ftMainParseMotionEvent and the three stepping loops are the decomp's
 * text (src/dc/ftmain.c); what these check is the port around them:
 * the pack's script words reach the interpreter, the bit-fields decode
 * the way the game's macros packed them (docker/patches/0005), and the
 * clock -- script_wait against anim_speed, the async wait against
 * anim_frame -- runs at the game's frames. */

/* a hitbox event fills FTAttackColl the way ft/ftmain.c:183-269 does */
static int is_squatwait(void) { return fp.status_id == nFTCommonStatusSquatWait; }

static void test_script_attack_coll(void)
{
    FTAttackColl *ac;

    spawn(0.0f, 0.0f);
    mock_anim_len = 3.0f;                   /* Squat ends, SquatWait begins */
    frame(0, -80, 0, 0, 0);                 /* Squat */
    CHECK_STATUS(nFTCommonStatusSquat);
    run_until(0, -80, 40, is_squatwait);
    CHECK_STATUS(nFTCommonStatusSquatWait);
    mock_anim_len = 1e9f;
    /* the script started with the status: Wait(2) then the hitbox */
    ac = &fp.attack_colls[1];
    CHECK(ac->attack_state == nGMAttackStateOff);
    frame(0, -80, 0, 0, 0);
    CHECK(ac->attack_state == nGMAttackStateOff);
    frame(0, -80, 0, 0, 0);
    /* the script leaves it New; the physics process gives it its world
     * position and moves it to Transfer the same frame (ft/ftmain.c:
     * 1862-1916), so that is what a frame ends with */
    CHECK(ac->attack_state == nGMAttackStateTransfer);
    CHECK(fp.is_attack_active);
    CHECK(ac->group_id == 0);
    CHECK(ac->joint_id == 6);
    CHECK(ac->joint == fp.joints[6]);
    CHECK(ac->damage == 14);
    CHECK(ac->can_rebound == 0);
    CHECK(ac->element == 0);
    CHECK_EQF(ac->size, 100.0f);            /* 200 * 0.5 */
    CHECK_EQF(ac->offset.x, 30.0f);
    CHECK_EQF(ac->offset.y, 40.0f);
    CHECK_EQF(ac->offset.z, 50.0f);
    CHECK(ac->angle == 361);
    CHECK(ac->knockback_scale == 100);
    CHECK(ac->knockback_weight == 100);
    CHECK(ac->is_hit_air == 1);
    CHECK(ac->is_hit_ground == 1);
    CHECK(ac->shield_damage == 3);
    CHECK(ac->fgm_level == 2);
    CHECK(ac->fgm_kind == 1);
    CHECK(ac->knockback_base == 40);
    CHECK(ac->is_scale_pos == 0);
    CHECK(ac->attack_records[0].victim_gobj == NULL);
    CHECK(ac->attack_records[3].victim_flags.group_id == 7);
    /* Wait(3): resize and re-damage on frame 5 (the stick stays down:
     * letting go would leave SquatWait, and the script with it) */
    frame(0, -80, 0, 0, 0);
    frame(0, -80, 0, 0, 0);
    frame(0, -80, 0, 0, 0);
    CHECK_EQF(ac->size, 60.0f);
    CHECK(ac->damage == 9);
    /* Wait(2): cleared on frame 7 */
    frame(0, -80, 0, 0, 0);
    frame(0, -80, 0, 0, 0);
    CHECK(ac->attack_state == nGMAttackStateOff);
    CHECK(!fp.is_attack_active);
}

/* LoopBegin/LoopEnd count through their body; the flag after the loop
 * lands when the third pass ends */
static void test_script_loop(void)
{
    int on = 0, off = 0, i;

    spawn(0.0f, 0.0f);
    frame(0, -80, 0, 0, 0);                 /* Squat: the loop script */
    CHECK_STATUS(nFTCommonStatusSquat);
    CHECK(fp.motion_vars.flags.flag2 == 1); /* first pass, frame 0 */
    CHECK(fp.motion_vars.flags.flag3 == 0);
    for (i = 0; i < 12; i++)
    {
        frame(0, -80, 0, 0, 0);
        if (fp.status_id != nFTCommonStatusSquat)
            break;
        if (fp.motion_vars.flags.flag2) on++; else off++;
    }
    CHECK(fp.motion_vars.flags.flag3 == 7); /* set as the third pass ends */
    /* 4 and 8, not 5 and 7. The script is the same --
     * flag3 still lands on 7 as the third pass ends -- and what moved is
     * WHEN pass zero runs: ftCommonSquatSetStatusNoPass is the decomp's
     * own now, and it calls ftMainPlayAnimEventsAll immediately after
     * ftMainSetStatus, which the port's hand-copy did not. So frame 0's
     * events fire at set time rather than on the first update, and the
     * twelve frames sampled below start one further into the loop. */
    CHECK(on == 4 && off == 8);             /* frames 1-12 of 3 passes of 2+2 */
}

/* ---- the six rows that carried stand-in callbacks ----
 *
 * dFTCommonActionStatusDescs had six rows running something other
 * than the game's proc, because the three files those
 * procs live in were not in the build:
 * ft/ftcommon/ftcommonjumpaerial.c, ftcommonsquat.c and ftcommonfall.c.
 * All three compile unmodified now and thirteen hand-copies of their
 * functions came out of src/dc/ftcommon.c.
 *
 * One of the six is worth knowing about: SquatWait's ProcUpdate was
 * NULL, and the game's own ftCommonSquatWaitProcUpdate is `return;` --
 * an empty function. That stand-in was never wrong. The other five
 * were. ---- */
static void test_standin_rows(void)
{
    extern FTStatusDesc dFTCommonActionStatusDescs[];
    FTStatusDesc *r;

    r = &dFTCommonActionStatusDescs[nFTCommonStatusJumpAerialF - nFTCommonStatusActionStart];
    CHECK(r->proc_update == ftCommonJumpAerialProcUpdate);
    CHECK(r->proc_interrupt == ftCommonJumpAerialProcInterrupt);
    r = &dFTCommonActionStatusDescs[nFTCommonStatusJumpAerialB - nFTCommonStatusActionStart];
    CHECK(r->proc_update == ftCommonJumpAerialProcUpdate);
    CHECK(r->proc_interrupt == ftCommonJumpAerialProcInterrupt);
    r = &dFTCommonActionStatusDescs[nFTCommonStatusFall - nFTCommonStatusActionStart];
    CHECK(r->proc_update == NULL);
    CHECK(r->proc_interrupt == ftCommonFallProcInterrupt);
    r = &dFTCommonActionStatusDescs[nFTCommonStatusFallAerial - nFTCommonStatusActionStart];
    CHECK(r->proc_update == NULL);
    CHECK(r->proc_interrupt == ftCommonFallProcInterrupt);
    r = &dFTCommonActionStatusDescs[nFTCommonStatusSquatWait - nFTCommonStatusActionStart];
    CHECK(r->proc_update == ftCommonSquatWaitProcUpdate);
    CHECK(r->proc_interrupt == ftCommonSquatWaitProcInterrupt);
    r = &dFTCommonActionStatusDescs[nFTCommonStatusSquatRv - nFTCommonStatusActionStart];
    CHECK(r->proc_update == ftAnimEndSetWait);
    CHECK(r->proc_interrupt == ftCommonSquatRvProcInterrupt);

    /* ---- the multi-jump, which is what JumpAerial's two rows are for.
     *
     * Kirby and Purin get five aerial jumps and everyone else one, and
     * the two velocity tables that say how much each is worth live in
     * ftcommonjumpaerial.c itself -- Kirby's falls 60/52/47/40 and
     * Purin's 60/40/20/0, so her fifth jump lifts her nothing at all.
     * They are the only per-fighter data in the file. ---- */
    {
        extern f32 dFTKirbyJumpAerialFVelocities[];
        extern f32 dFTPurinJumpAerialFVelocities[];

        CHECK(dFTKirbyJumpAerialFVelocities[0] == 60.0f);
        CHECK(dFTKirbyJumpAerialFVelocities[3] == 40.0f);
        CHECK(dFTPurinJumpAerialFVelocities[0] == 60.0f);
        CHECK(dFTPurinJumpAerialFVelocities[3] == 0.0f);
    }

    /* and the jump itself, driven: a fighter in the air with a jump
     * left takes JumpAerialF, and one with none does not move */
    spawn(0.0f, 400.0f);
    fp.fkind = nFTKindMario;
    ftMainSetStatus(mock_gobj, nFTCommonStatusFall, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    mpCommonSetFighterAir(&fp);
    fp.jumps_used = 1;
    fp.attr->jumps_max = 2;
    fp.input.pl.button_tap = FT_JUMP_BUTTONS_TEST;
    CHECK(ftCommonJumpAerialCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusJumpAerialF);
    CHECK(fp.jumps_used == 2);

    spawn(0.0f, 400.0f);
    ftMainSetStatus(mock_gobj, nFTCommonStatusFall, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    mpCommonSetFighterAir(&fp);
    fp.jumps_used = 2;
    fp.attr->jumps_max = 2;
    fp.input.pl.button_tap = FT_JUMP_BUTTONS_TEST;
    CHECK(ftCommonJumpAerialCheckInterruptCommon(mock_gobj) == FALSE);
    CHECK_STATUS(nFTCommonStatusFall);

    fp.input.pl.button_tap = 0;
    fp.jumps_used = 0;
    spawn(0.0f, 0.0f);
}

/* ---- the pass-through, and a divergence whose reason was
 * wrong ----
 *
 * ft/ftcommon/ftcommonpass.c compiles unmodified now and seven
 * hand-copies came out of src/dc/ftcommon.c with it. One of them carried
 * a DIVERGES that does not hold: ftCommonPassSetStatusParam latched
 * coll_data.floor_line_id BEFORE ftMainSetStatus, on the stated grounds
 * that "the port's ftMainSetStatus clears ignore_line_id". The game's
 * clears it too (ft/ftmain.c:4500) -- and the decomp assigns
 * ignore_line_id = floor_line_id AFTER that call, so the clear happens
 * first and the assignment stands. Nothing between the two touches
 * floor_line_id (mpCommonSetFighterAir sets ga, vel_air.z and
 * jumps_used, and nothing else), so the two orderings agree and the
 * latch was an unnecessary divergence resting on a misread.
 *
 * What this checks is the property both orderings were for: dropping
 * through a platform leaves ignore_line_id naming the line the fighter
 * was standing on, and only that one. ---- */
static void test_pass_through(void)
{
    spawn(0.0f, 0.0f);
    fp.coll_data.floor_line_id = 7;
    fp.coll_data.ignore_line_id = -1;
    fp.ga = nMPKineticsGround;

    ftCommonPassSetStatusParam(mock_gobj, nFTCommonStatusPass, 0.0f,
                               FTSTATUS_PRESERVE_NONE);

    CHECK_STATUS(nFTCommonStatusPass);
    CHECK(fp.ga == nMPKineticsAir);
    /* the line it was standing on, latched through a call that clears
     * this very field */
    CHECK(fp.coll_data.ignore_line_id == 7);
    CHECK(fp.physics.vel_air.y == 0.0f);
    CHECK(fp.tap_stick_y == FTINPUT_STICKBUFFER_TICS_MAX);

    /* and the input the drop needs: the stick down inside the tap
     * window, on a floor whose flags allow it */
    spawn(0.0f, 0.0f);
    fp.input.pl.stick_range.y = -80;
    fp.tap_stick_y = 0;
    fp.coll_data.floor_flags = MAP_VERTEX_COLL_PASS;
    CHECK(ftCommonPassCheckInputSuccess(&fp) == TRUE);
    /* the same stick on a floor that does not pass is refused */
    fp.coll_data.floor_flags = 0;
    CHECK(ftCommonPassCheckInputSuccess(&fp) == FALSE);
    /* and so is a stale tap */
    fp.coll_data.floor_flags = MAP_VERTEX_COLL_PASS;
    fp.tap_stick_y = FTCOMMON_PASS_BUFFER_TICS_MAX;
    CHECK(ftCommonPassCheckInputSuccess(&fp) == FALSE);

    fp.input.pl.stick_range.y = 0;
    fp.tap_stick_y = 0;
    fp.coll_data.floor_flags = 0;
    fp.coll_data.ignore_line_id = -1;
    spawn(0.0f, 0.0f);
}

/* SyncWait counts from the previous event, AsyncWait from the
 * animation's start, PauseScript holds until the animation ends */
static void test_script_waits(void)
{
    spawn(0.0f, 1500.0f);                   /* ~30 frames of air */
    CHECK_STATUS(nFTCommonStatusFall);
    CHECK(fp.motion_vars.flags.flag0 == 0);
    idle_frames(3);
    CHECK(fp.motion_vars.flags.flag0 == 0);
    idle_frames(1);                         /* frame 4: Wait(4) */
    CHECK(fp.motion_vars.flags.flag0 == 1);
    idle_frames(1);
    CHECK(fp.motion_vars.flags.flag0 == 1);
    idle_frames(1);                         /* frame 6: WaitAsync(6) */
    CHECK(fp.motion_vars.flags.flag0 == 2);
    idle_frames(10);                        /* paused: never 3 */
    CHECK(fp.motion_vars.flags.flag0 == 2);
    CHECK(fp.motion_scripts[0][0].p_script != NULL);
}

/* Respawning drops the fighter in from the rebirth point. */
/* The spawn as the battle scene does it: into Entry, invisible and
 * locked, then ftCommonAppearSetStatus (the entry focus's call) plays
 * the Appear as root motion around the spawn point, and the animation's
 * end lands the fighter back on it facing the way it spawned. */
static void test_entry_and_appear(void)
{
    Vec3f start;

    spawn_desc(0.0f, 400.0f, -1, FALSE);
    CHECK_STATUS(nFTCommonStatusEntry);
    CHECK(fp.is_invisible);
    CHECK(fp.is_ghost);
    CHECK(fp.is_control_disable);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK_EQF(fp_pos.y, 300.0f);
    CHECK(fp.coll_data.floor_line_id == LINE_PLAT);
    CHECK(fp.lr == -1);
    /* ftmanager.c:697: the stock count into the battle state */
    CHECK(mock_battle.players[0].stock_count == 3);
    CHECK(mock_battle.players[0].fighter_gobj == mock_gobj);

    /* locked: the stick moves nothing */
    frame(80, 0, 0, 0, 0);
    frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusEntry);
    CHECK_EQF(fp_pos.x, 0.0f);
    CHECK(fp.status_total_tics == 0);

    start = fp_pos;
    mock_anim_len = 6.0f;
    mock_transn[1] = 120.0f;
    ftCommonAppearSetStatus(mock_gobj);
    CHECK_STATUS(nFTMarioStatusAppearL);
    CHECK(fp.lr == 0);
    CHECK(fp.status_vars.common.entry.lr == -1);
    CHECK(fp.camera_mode == nFTCameraModeEntry);
    CHECK(fp.is_ghost);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK_EQF(fp.entry_pos.y, start.y);
    /* ftcommonentry.c:134-155: the position follows TransN's animated
     * translate, laid on the spawn point, with no velocity of its own */
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTMarioStatusAppearL);
    CHECK_EQF(fp_pos.y, start.y + 120.0f);
    CHECK_EQF(fp.physics.vel_air.y, 0.0f);
    CHECK_EQF(fp.joints[nFTPartsJointTopN]->translate.vec.f.y,
              start.y + 120.0f);

    /* ftcommonentry.c:113-131: the End command puts the fighter back
     * on the spawn point, facing as it spawned, on its floor */
    idle_frames(8);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK_EQF(fp_pos.y, start.y);
    CHECK(fp.lr == -1);
    CHECK(fp.coll_data.floor_line_id == LINE_PLAT);
    CHECK(!fp.is_ghost);
    /* ft/ftmain.c:4512-4515: a status keeps the entry camera until the
     * GO! sets the default one (ifCommonAnnounceGoSetStatus) */
    CHECK(fp.camera_mode == nFTCameraModeEntry);
    CHECK(fp.is_control_disable);

    /* the GO! (if/ifcommon.c:2005-2015): the stick reads again, and
     * facing left, a left stick tapped over is a dash (the game's
     * cascade dashes on a tap past 56) */
    ftParamUnlockPlayerControl(mock_gobj);
    fp.camera_mode = nFTCameraModeDefault;
    frame(-80, 0, 0, 0, 0);
    frame(-80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusDash);
    CHECK(fp.status_total_tics == 1);      /* reset by the Dash switch, then one tic */
}

/* ---- layer 4: GObj threads ------------------------------------------
 *
 * The object system runs a thread process as a coroutine: resumed once
 * per gcRunAll, suspended by gcSleepCurrentGObjThread, destroyed by main
 * after the thread ejects its own GObj (src/dc/sysshim.c). This is that
 * contract, then the game's entry sequence on it. */
static int thread_wakes[8];
static int thread_wake_count;
static int thread_tic;

static void thread_proc(GObj *gobj)
{
    int i;

    (void)gobj;
    for (i = 0; i < 3; i++)
    {
        if (thread_wake_count < 8)
            thread_wakes[thread_wake_count++] = thread_tic;
        gcSleepCurrentGObjThread(3);
    }
    if (thread_wake_count < 8)
        thread_wakes[thread_wake_count++] = thread_tic;
    gcEjectGObj(NULL);
    gcSleepCurrentGObjThread(1);
}

static void test_gobj_threads(void)
{
    GObj *gobj;
    int tic;

    thread_wake_count = 0;
    gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL,
                             nGCCommonLinkIDInterfaceActor,
                             GOBJ_PRIORITY_DEFAULT);
    gcAddGObjProcess(gobj, thread_proc, nGCProcessKindThread, 5);

    for (tic = 0; tic < 20; tic++)
    {
        thread_tic = tic;
        gcRunAll();
    }
    /* woken on tics 0, 3, 6 and 9: the sleep is counted in gcRunAll
     * turns, the first resume is the tic the process was added on */
    CHECK(thread_wake_count == 4);
    CHECK(thread_wakes[0] == 0);
    CHECK(thread_wakes[1] == 3);
    CHECK(thread_wakes[2] == 6);
    CHECK(thread_wakes[3] == 9);
    /* the self-eject took the GObj off its link */
    CHECK(gGCCommonLinks[nGCCommonLinkIDInterfaceActor] == NULL);
}

/* scvsbattle.c:216 ifCommonEntryAllMakeInterface, on a fighter made into
 * Entry: the entry-all thread wakes at 90 and starts the countdown and
 * the focus, both of which first run on that same tic (a process added
 * at the running priority is appended to its list and reached in the
 * same gcRunAll). The focus id is random: 0 starts the Appear at 112, 2
 * at 150, 1 at 195. The countdown's timer starts at its first run and
 * the GO! is its I_SEC_TO_TICS(5) case, rise included, so the
 * controller unlocks on tic 90+300 = 390. The threads are the game's;
 * the tics are counted here. */
static void test_entry_sequence(void)
{
    int tic, appear_tic = -1, wait_tic = -1, go_tic = -1;

    spawn_desc(0.0f, 400.0f, -1, FALSE);
    mock_anim_len = 30.0f;
    mock_battle.pl_count = 1;
    mock_battle.cp_count = 0;
    syUtilsSetRandomSeed(1);

    ifCommonBattleSetGameStatusWait();
    /* the countdown's traffic light and GO! are sprites out of the
     * battle's common files (src/dc/gmcommon.c); released at the end
     * as the scene manager would */
    scVSBattleSetupFiles();
    ifCommonEntryAllMakeInterface();
    CHECK(mock_battle.game_status == nSCBattleGameStatusWait);

    for (tic = 0; tic < 520; tic++)
    {
        /* a held stick until three tics past the GO!: nothing moves
         * before it, a walk starts on it (and would walk off the mock's
         * platform if held longer) */
        mock_in.stick_range.x = (go_tic < 0 || tic < go_tic + 3) ? -80 : 0;
        mpCollisionAdvanceUpdateTic(NULL);
        gcRunAll();
        if (appear_tic < 0 && fp.status_id == nFTMarioStatusAppearL)
            appear_tic = tic;
        if (appear_tic >= 0 && wait_tic < 0 &&
            fp.status_id == nFTCommonStatusWait)
            wait_tic = tic;
        if (go_tic < 0 && mock_battle.game_status == nSCBattleGameStatusGo)
            go_tic = tic;
        if (go_tic < 0)
            CHECK(fp.is_control_disable);
        if (go_tic < 0 && fp.status_id == nFTCommonStatusDash)
        {
            printf("FAIL %s:%d: moved before the GO! on tic %d\n",
                   __FILE__, __LINE__, tic);
            failures++;
            break;
        }
        /* the held stick is a dash from the frame control comes back
         * (a fast walk before it) */
        if (go_tic >= 0 && tic == go_tic + 2)
            CHECK_STATUS(nFTCommonStatusDash);
    }
    CHECK(appear_tic == 112 || appear_tic == 150 || appear_tic == 195);
    CHECK(wait_tic == appear_tic + 30);
    CHECK(go_tic == 390);
    CHECK(!fp.is_control_disable);
    CHECK(fp.camera_mode == nFTCameraModeDefault);
    CHECK(fp.status_total_tics > 0);
    /* every interface thread ejected itself: the focus by 90+22+22, the
     * countdown at 90+420 */
    CHECK(gGCCommonLinks[nGCCommonLinkIDInterfaceActor] == NULL);
    CHECK(gGCCommonLinks[nGCCommonLinkIDInterface] == NULL);
    sprite_bank_release_all();
    mock_in.stick_range.x = 0;
    mock_battle.pl_count = 0;
}
