/* hosttest/physics.c -- part of hosttest_ft.c: layers 1-3: the physics fuzz against verbatim ft/ftphysics.c, the
 * scenarios on Mario's numbers, and the collision walk (floors,
 * edges, platforms, slopes, walls, ledges).
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

static int is_grounded(void) { return fp.ga == nMPKineticsGround; }

static void fuzz_physics(void)
{
    unsigned seed = 12345;
    int i;

    for (i = 0; i < 200000; i++)
    {
        RefFT ref;
        float friction, vel, clamp, gravity, tvel;
        int decision_ref;

        seed = seed * 1664525u + 1013904223u;
        memset(&ref, 0, sizeof(ref));
        ref.physics.vel_ground.x = ((seed >> 8) % 2000 - 1000) / 10.0f;
        ref.physics.vel_air.x = ((seed >> 12) % 2000 - 1000) / 12.0f;
        ref.physics.vel_air.y = ((seed >> 4) % 2000 - 1000) / 9.0f;
        ref.input.pl.stick_range.x = (int)((seed >> 16) % 161) - 80;
        ref.input.pl.stick_range.y = (int)((seed >> 20) % 161) - 80;
        friction = (seed % 40) / 10.0f;
        vel = ((seed >> 5) % 100) / 100.0f;
        clamp = 10.0f + (seed >> 7) % 40;
        gravity = ((seed >> 9) % 50) / 10.0f;
        tvel = 20.0f + (seed >> 11) % 60;

        /* the port's equivalents live behind static linkage; drive them
         * through a scratch FTStruct built to match and the exposed
         * test hooks below */
        {
            FTStruct p;

            memset(&p, 0, sizeof(p));
            p.attr = &kMarioAttr;
            p.physics.vel_ground.x = ref.physics.vel_ground.x;
            p.physics.vel_air.x = ref.physics.vel_air.x;
            p.physics.vel_air.y = ref.physics.vel_air.y;
            p.input.pl.stick_range.x = ref.input.pl.stick_range.x;
            p.input.pl.stick_range.y = ref.input.pl.stick_range.y;

            ftTestSetGroundVelFriction(&p, friction);
            ref_SetGroundVelFriction(&ref, friction);
            CHECK_EQF(p.physics.vel_ground.x, ref.physics.vel_ground.x);

            ftTestSetGroundVelAbsStickRange(&p, vel, friction);
            ref_SetGroundVelAbsStickRange(&ref, vel, friction);
            CHECK_EQF(p.physics.vel_ground.x, ref.physics.vel_ground.x);

            ftTestApplyGravityClampTVel(&p, gravity, tvel);
            ref_ApplyGravityClampTVel(&ref, gravity, tvel);
            CHECK_EQF(p.physics.vel_air.y, ref.physics.vel_air.y);

            decision_ref = ref_CheckClampAirVelXDec(&ref, clamp);
            CHECK(ftTestCheckClampAirVelXDec(&p, clamp) == decision_ref);
            CHECK_EQF(p.physics.vel_air.x, ref.physics.vel_air.x);

            ftTestClampAirVelXStickRange(&p, 8, vel / 10.0f, clamp);
            ref_ClampAirVelXStickRange(&ref, 8, vel / 10.0f, clamp);
            CHECK_EQF(p.physics.vel_air.x, ref.physics.vel_air.x);

            ftTestApplyAirVelXFriction(&p);
            ref_ApplyAirVelXFriction(&ref, kMario.air_friction);
            CHECK_EQF(p.physics.vel_air.x, ref.physics.vel_air.x);
        }

        if (failures > 20)
        {
            printf("too many failures, aborting fuzz\n");
            return;
        }
    }

    /* jump force button curve, every stick value, both hop kinds */
    {
        int sx, hop;

        for (hop = 0; hop <= 1; hop++)
            for (sx = -80; sx <= 80; sx++)
            {
                int rx, ry, px, py;

                ref_JumpGetJumpForceButton(sx, &rx, &ry, hop);
                ftTestJumpGetJumpForceButton(sx, &px, &py, hop);
                CHECK_EQF(px, rx);
                CHECK_EQF(py, ry);
            }
    }
}


/* ---- layer 2: scenarios on Mario's numbers -------------------------- */

static void test_walk_tiers(void)
{
    int i;

    spawn(0.0f, 0.0f);
    CHECK_STATUS(nFTCommonStatusWait);

    frame(20, 0, 0, 0, 0);      /* below the middle threshold */
    CHECK_STATUS(nFTCommonStatusWalkSlow);
    frame(40, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWalkMiddle);
    frame(70, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWalkFast);

    /* fast walk at full stick: vel_ground.x climbs to 80*0.3 = 24 */
    for (i = 0; i < 40; i++)
        frame(80, 0, 0, 0, 0);
    CHECK_EQF(fp.physics.vel_ground.x, 80 * 0.3f);
    CHECK(fp_pos.x > 0.0f);

    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);
}

static void test_button_jump_long_and_short(void)
{
    int want_x, want_y;

    /* long hop: hold the button through the 3-frame squat */
    spawn(0.0f, 0.0f);
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusKneeBend);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusJumpF);
    ref_JumpGetJumpForceButton(0, &want_x, &want_y, 0);
    CHECK_EQF(fp.physics.vel_air.y,
              want_y * kMario.jump_height_mul + kMario.jump_height_base -
              kMario.gravity);      /* one air frame already applied */
    CHECK(fp.ga == nMPKineticsAir);

    /* short hop: release inside the window */
    spawn(0.0f, 0.0f);
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, 0, FT_JUMP_BUTTONS_TEST);    /* released frame 2 */
    frame(0, 0, 0, 0, 0);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusJumpF);
    ref_JumpGetJumpForceButton(0, &want_x, &want_y, 1);
    CHECK_EQF(fp.physics.vel_air.y,
              want_y * kMario.jump_height_mul + kMario.jump_height_base -
              kMario.gravity);
}

static void test_stick_jump_and_aerial(void)
{
    spawn(0.0f, 0.0f);
    frame(0, 80, 0, 0, 0);      /* fresh full-up stick */
    CHECK_STATUS(nFTCommonStatusKneeBend);
    frame(0, 80, 0, 0, 0);
    frame(0, 80, 0, 0, 0);
    frame(0, 80, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusJumpF);
    CHECK_EQF(fp.physics.vel_air.y,
              80.0f * kMario.jump_height_mul + kMario.jump_height_base -
              kMario.gravity);
    CHECK(fp.jumps_used == 1);  /* mpCommonSetFighterAir spends it */

    /* aerial jump: tap the button mid-air; only one extra jump */
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusJumpAerialF);
    CHECK(fp.jumps_used == 2);
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusJumpAerialF);   /* no change */
    CHECK(fp.jumps_used == 2);
}

static void test_land_light_and_heavy(void)
{
    int i;

    /* normal fall -> light landing, back onto the platform it left */
    spawn(0.0f, 300.0f);
    CHECK(fp.coll_data.floor_line_id == LINE_PLAT);
    frame(0, 80, 0, 0, 0);
    idle_frames(3);             /* through the 3-frame jump squat */
    CHECK(fp.ga == nMPKineticsAir);
    run_until(0, 0, 120, is_grounded);
    CHECK_STATUS(nFTCommonStatusLandingLight);
    CHECK_EQF(fp_pos.y, 300.0f);
    CHECK(fp.coll_data.floor_line_id == LINE_PLAT);

    /* fast-fall -> heavy landing */
    spawn(0.0f, 300.0f);
    frame(0, 80, 0, 0, 0);
    idle_frames(3);
    for (i = 0; i < 60 && fp.physics.vel_air.y >= 0.0f; i++)
        idle_frames(1);         /* wait for the peak */
    frame(0, -80, 0, 0, 0);     /* fresh down tap: fast fall */
    CHECK(fp.is_fastfall == 1);
    run_until(0, 0, 120, is_grounded);
    CHECK_STATUS(nFTCommonStatusLandingHeavy);
}

/* ---- layer 3: the collision walk ------------------------------------ */

/* Standing puts the diamond's bottom corner on the floor, and the walk
 * reports the line and its flags. */
static void test_spawn_settles_on_floor(void)
{
    Vec3f start;

    mpCollisionGetPlayerMapObjPosition(nMPMapObjKindBattlePlayer1, &start);
    CHECK_EQF(start.x, 0.0f);
    CHECK_EQF(start.y, 1000.0f);

    /* ft/ftmanager.c:554 only stands the fighter up when the floor is
     * within 300 units; 1000 above the platform is a fall */
    spawn(start.x, start.y);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK_STATUS(nFTCommonStatusFall);

    /* ...and 100 above it is a stand, snapped down onto the line */
    spawn(0.0f, 400.0f);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK_EQF(fp_pos.y, 300.0f);
    CHECK(fp.coll_data.floor_line_id == LINE_PLAT);
    CHECK(fp.coll_data.floor_flags & MAP_VERTEX_COLL_PASS);

    /* the deck below is solid and un-passable */
    spawn(-1500.0f, 100.0f);
    CHECK_EQF(fp_pos.y, 0.0f);
    CHECK(fp.coll_data.floor_line_id == LINE_DECK);
    CHECK(!(fp.coll_data.floor_flags & MAP_VERTEX_COLL_PASS));
}

/* Walking off the deck's edge teeters (Ottotto) rather than falling --
 * the behaviour the vertical-ray shim could not express. */
static void test_walkoff_teeters(void)
{
    int i;

    /* mp/mpcommon.c:58 only takes the teeter when the stick is under
     * 60 toward the edge, so this is a middle-speed walk (over
     * FTCOMMON_WALKMIDDLE_STICK_RANGE_MIN, under the cliff gate) */
    spawn(1500.0f, 0.0f);
    CHECK_STATUS(nFTCommonStatusWait);

    for (i = 0; i < 400 && fp.status_id != nFTCommonStatusOttotto; i++)
        frame(50, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusOttotto);
    CHECK(fp.ga == nMPKineticsGround);
    /* mp/mpcommon.c:86 parks the fighter 40 units in from the vertex */
    CHECK_EQF(fp_pos.x, 2000.0f - 40.0f);
    CHECK(fp.coll_data.mask_stat & MAP_FLAG_FLOOREDGE);

    /* the teeter ends in OttottoWait (ftcommonottotto.c:38-41) */
    mock_anim_len = 4.0f;
    idle_frames(6);
    CHECK_STATUS(nFTCommonStatusOttottoWait);

    /* turning away is the only way off the ledge inland: the walk
     * interrupt at ftcommonottotto.c:50 is gated on the stick pointing
     * the way the fighter faces, which is at the edge. Once turned and
     * walking, ftCommonOttottoProcMap's distance test (line 82) hands
     * the fighter back to Wait. The turn animation has to outlast the
     * script's flip frame (WaitAsync(6)), as Mario's does. */
    mock_anim_len = 12.0f;
    for (i = 0; i < 240 && !(fp.status_id == nFTCommonStatusWait ||
                             fp.status_id == nFTCommonStatusWalkSlow ||
                             fp.status_id == nFTCommonStatusWalkMiddle ||
                             fp.status_id == nFTCommonStatusWalkFast); i++)
        frame(-80, 0, 0, 0, 0);
    CHECK(fp.lr == -1);
    CHECK(fp.ga == nMPKineticsGround);
    idle_frames(0);
    for (i = 0; i < 60; i++)
        frame(-80, 0, 0, 0, 0);
    CHECK(fp_pos.x <
          2000.0f - 40.0f - FTCOMMON_OTTOTTO_WALK_DIST_X_MIN);
    mock_anim_len = 1e9f;
}

/* Running off the edge -- no teeter, straight to Fall -- is what the
 * game does when the fighter is moving fast enough that
 * mpCommonCheckSetFighterCliffEdge's stick test fails. Here: stick held
 * hard away from the ledge past the edge in Turn, whose map proc is
 * mpCommonSetFighterFallOnGroundBreak (no CLIFFEDGE flag). */
static void test_walkoff_without_cliffedge_falls(void)
{
    int i;

    spawn(1900.0f, 0.0f);
    for (i = 0; i < 200 && fp.ga == nMPKineticsGround; i++)
        frame(80, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(fp.jumps_used >= 1);
}

/* Down-tap on a pass-through platform squats, then drops through it and
 * only it: the deck below still catches the fighter. */
static void test_dropthrough(void)
{
    int i;

    spawn(0.0f, 400.0f);
    CHECK(fp.coll_data.floor_line_id == LINE_PLAT);

    frame(0, -80, 0, 0, 0);     /* fresh hard down: squat, pass armed */
    CHECK_STATUS(nFTCommonStatusSquat);
    for (i = 0; i < 8 && fp.status_id == nFTCommonStatusSquat; i++)
        frame(0, -80, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusPass);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(fp.coll_data.ignore_line_id == LINE_PLAT);

    run_until(0, 0, 120, is_grounded);
    CHECK_EQF(fp_pos.y, 0.0f);
    CHECK(fp.coll_data.floor_line_id == LINE_DECK);
}

/* Jumping from below passes up through the platform and lands on it. */
static void test_jump_through_platform(void)
{
    spawn(0.0f, 0.0f);
    CHECK(fp.coll_data.floor_line_id == LINE_DECK);

    frame(0, 80, 0, 0, 0);
    idle_frames(3);
    CHECK(fp.ga == nMPKineticsAir);
    run_until(0, 0, 200, is_grounded);
    CHECK_EQF(fp_pos.y, 300.0f);
    CHECK(fp.coll_data.floor_line_id == LINE_PLAT);
}

/* A sloped floor tilts the ground-to-air velocity transfer
 * (ft/ftphysics.c:8-9), so walking uphill gains height. The slope rises
 * 500 over 1000, so floor_angle is the unit normal of that line. */
static void test_slope(void)
{
    float nx, ny, mag;
    int i;

    spawn(3200.0f, 350.0f);
    CHECK(fp.coll_data.floor_line_id == LINE_SLOPE);
    CHECK_EQF(fp_pos.y, 100.0f);    /* y = (x-3000)/2 */

    /* mp/mpcollision.c:613-645 mpCollisionGetFCAngle for (3000,0)-(4000,500):
     * py = -(dy/dx) = -0.5, angle = (py, 1) / sqrt(py^2+1) normalised */
    mag = sqrtf(0.5f * 0.5f + 1.0f);
    nx = -0.5f / mag;
    ny = 1.0f / mag;
    CHECK_NEAR(fp.coll_data.floor_angle.x, nx, 1e-5f);
    CHECK_NEAR(fp.coll_data.floor_angle.y, ny, 1e-5f);

    /* walking uphill: y climbs as x does, staying on the line */
    for (i = 0; i < 40; i++)
        frame(80, 0, 0, 0, 0);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK(fp_pos.x > 3200.0f);
    CHECK_NEAR(fp_pos.y, (fp_pos.x - 3000.0f) * 0.5f, 0.01f);
    /* the transfer puts the motion along the surface, not flat */
    CHECK(fp.physics.vel_air.y > 0.0f);
    CHECK_NEAR(fp.physics.vel_air.x, fp.lr * ny * fp.physics.vel_ground.x,
               1e-4f);
}

/* A hard ceiling stops a fast rise: mpCommonProcFighterCliffFloorCeil
 * raises MAP_FLAG_CEILHEAVY above 30/frame and the fighter bonks. */
static void test_ceiling_bonk(void)
{
    int i;

    /* under the ceiling span, on the deck */
    spawn(-1400.0f, 0.0f);
    frame(0, 80, 0, 0, 0);
    idle_frames(3);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(fp.physics.vel_air.y >= 30.0f);

    for (i = 0; i < 40 && fp.status_id != nFTCommonStatusStopCeil; i++)
        idle_frames(1);
    CHECK_STATUS(nFTCommonStatusStopCeil);
    CHECK_EQF(fp.physics.vel_air.y, 0.0f);
    /* the diamond's top corner is at the ceiling line */
    CHECK_NEAR(fp_pos.y + kMario.map_coll_top, 900.0f, 0.01f);
}

/* A right wall blocks leftward travel and does not stop the fall. */
static void test_wall_blocks(void)
{
    int i;

    spawn(-2700.0f, 900.0f);
    CHECK(fp.ga == nMPKineticsAir);
    for (i = 0; i < 60; i++)
        frame(-80, 0, 0, 0, 0);
    /* the diamond's left corner cannot pass x = -3000 */
    CHECK(fp_pos.x - kMario.map_coll_width >= -3000.0f - 0.01f);
}

/* Falling past the deck's left ledge while facing it grabs the ledge,
 * then the hang, then either climb (stick toward the stage) or drop
 * (stick away). */
static void spawn_at_ledge(void)
{
    /* just left of the deck's left vertex, facing right (into the
     * stage): mpProcessCheckTestLCliffCollision needs lr == +1 */
    spawn(-2100.0f, 700.0f);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(fp.lr == 1);
    /* the hang pose comes from the animation's TransN translate */
    mock_transn[0] = 0.0f;
    mock_transn[1] = -200.0f;
    mock_transn[2] = -100.0f;
}

static int is_cliff_catch(void)
{
    return fp.status_id == nFTCommonStatusCliffCatch;
}

/* hang, then let the catch animation end into CliffWait */
static void reach_cliff_wait(void)
{
    spawn_at_ledge();
    CHECK(run_until(0, 0, 60, is_cliff_catch) < 60);
    CHECK_STATUS(nFTCommonStatusCliffCatch);
    mock_anim_len = 3.0f;
    idle_frames(4);
    CHECK_STATUS(nFTCommonStatusCliffWait);
    CHECK(fp.proc_damage == ftCommonCliffCommonProcDamage);  /* G13 */
    CHECK(fp.capture_immune_mask == FTCATCHKIND_MASK_TARUCANN);
    mock_anim_len = 1e9f;
}

static int is_cliff_catch2(void)
{
    return fp2.status_id == nFTCommonStatusCliffCatch;
}

/* mp/mpcommon.c:553-569: a ledge another fighter already
 * holds from the same side is refused, so the second falls past it; with
 * the ledge let go, the same fall catches it. */
static void test_ledge_shared(void)
{
    reach_cliff_wait();
    CHECK(fp.is_cliff_hold);
    spawn_second(-2100.0f, 700.0f, +1);
    CHECK(fp2.ga == nMPKineticsAir);
    CHECK(run_until(0, 0, 60, is_cliff_catch2) == 60);
    CHECK(!fp2.is_cliff_hold);
    CHECK(DObjGetStruct(mock_gobj2)->translate.vec.f.y < 0.0f);
    CHECK_STATUS(nFTCommonStatusCliffWait);

    reach_cliff_wait();
    fp.is_cliff_hold = FALSE;
    spawn_second(-2100.0f, 700.0f, +1);
    CHECK(run_until(0, 0, 60, is_cliff_catch2) < 60);
    CHECK(fp2.is_cliff_hold);
}

static void test_ledge_grab_and_climb(void)
{
    float edge_x = -2000.0f, edge_y = 0.0f;
    int n;

    spawn_at_ledge();
    n = run_until(0, 0, 60, is_cliff_catch);
    CHECK(n < 60);
    CHECK_STATUS(nFTCommonStatusCliffCatch);
    CHECK(fp.is_cliff_hold);
    CHECK(fp.coll_data.cliff_id == LINE_DECK);
    CHECK(fp.coll_data.floor_line_id == -1);
    /* a hit on the ledge first steps the fighter back
     * beside it, and a hanging fighter cannot be taken by a barrel
     * cannon */
    CHECK(fp.proc_damage == ftCommonCliffCommonProcDamage);
    CHECK(fp.capture_immune_mask == FTCATCHKIND_MASK_TARUCANN);
    CHECK_EQF(fp.physics.vel_air.x, 0.0f);
    CHECK_EQF(fp.physics.vel_air.y, 0.0f);
    /* ftcommoncliffcatchwait.c:27-30: TransN z into x, y into y, both
     * scaled by the model size */
    CHECK_NEAR(fp_pos.x,
               edge_x + fp.joints[nFTPartsJointTransN]->translate.vec.f.z *
               fp.lr * kMario.size, 1e-3f);
    CHECK_NEAR(fp_pos.y,
               edge_y + fp.joints[nFTPartsJointTransN]->translate.vec.f.y *
               kMario.size, 1e-3f);
    /* the ledge motions run TransN as root motion: the hidden part is
     * made for the status (ftMainUpdateHiddenPartID) and unlinked behind
     * the attach (ft/ftmain.c:4710-4721) -- a childless last sibling of
     * the hip under TopN, whose translate reaches nothing on the tree
     * and is read back as root motion instead */
    CHECK(fp.anim_desc.flags.is_use_transn_joint);
    CHECK(fp.joints[nFTPartsJointTransN] != NULL);
    CHECK(ftGetParts(fp.joints[nFTPartsJointTransN])->joint_id == nFTPartsJointTransN);
    CHECK(fp.joints[nFTPartsJointTransN]->child == NULL);
    CHECK(fp.joints[nFTPartsJointTransN]->parent == fp.joints[nFTPartsJointTopN]);
    CHECK(fp.joints[nFTPartsJointTopN]->child == fp.joints[4]);
    CHECK(fp.joints[4]->sib_next == fp.joints[nFTPartsJointTransN]);
    CHECK(fp.joints[4]->parent == fp.joints[nFTPartsJointTopN]);
    {
        Vec3f v;

        v.x = v.y = v.z = 0.0f;
        gmCollisionGetFighterPartsWorldPosition(fp.joints[6], &v);
        CHECK_NEAR(v.y, fp_pos.y + 168.0f, 1e-3f);
    }

    /* the catch animation ends in the hang */
    mock_anim_len = 3.0f;
    idle_frames(4);
    CHECK_STATUS(nFTCommonStatusCliffWait);
    mock_anim_len = 1e9f;

    /* the first frame of neutral stick only arms the interrupt
     * (ftcommoncliffclimb.c:115) */
    CHECK(fp.status_vars.common.cliffwait.is_allow_interrupt);

    /* stick toward the stage starts the wind-up
     * (ftcommoncliffclimb.c:57-77), still hanging */
    mock_anim_len = 2.0f;
    frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusCliffQuick);
    CHECK(fp.status_vars.common.cliffmotion.status_id ==
          nFTCommonCliffKindClimbQuick);
    CHECK(fp.status_vars.common.cliffmotion.cliff_id == LINE_DECK);
    CHECK(fp.is_cliff_hold);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK_NEAR(fp_pos.x,
               edge_x + mock_transn[2] * fp.lr * kMario.size, 1e-3f);
    CHECK_NEAR(fp_pos.y,
               edge_y + mock_transn[1] * kMario.size, 1e-3f);

    /* it ends in the first half of the climb, which is still the hang's
     * physics -- the fighter is pinned to the vertex the whole pull-up */
    idle_frames(1);
    CHECK_STATUS(nFTCommonStatusCliffClimbQuick1);
    CHECK(fp.proc_damage == ftCommonCliffCommonProcDamage);  /* G13 */
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(fp.is_cliff_hold);

    /* the pull-up brings the root back to the vertex, so the hand-off
     * into the second half carries no velocity */
    mock_transn[1] = mock_transn[2] = 0.0f;
    idle_frames(2);
    CHECK_STATUS(nFTCommonStatusCliffClimbQuick2);
    /* cliff_status_ga says a climb-up is grounded, and
     * ftCommonCliffCommon2UpdateCollData stands the fighter 5 units in
     * from the vertex on the cliff's own line */
    CHECK(fp.ga == nMPKineticsGround);
    CHECK(fp.is_jostle_ignore);
    CHECK(!fp.is_cliff_hold);
    CHECK(fp.coll_data.floor_line_id == LINE_DECK);
    CHECK_NEAR(fp_pos.x, edge_x + 5.0f, 1e-3f);
    CHECK_EQF(fp_pos.y, edge_y);
    CHECK_EQF(fp.physics.vel_ground.x, 0.0f);

    /* and the climb ends standing on the deck */
    idle_frames(2);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK(fp.coll_data.floor_line_id == LINE_DECK);
    mock_anim_len = 1e9f;
}

/* Reach CliffClimbQuick2 with the root motion at rest, so a test can
 * drive it from a known state. */
static void reach_climb2(void)
{
    reach_cliff_wait();
    mock_anim_len = 2.0f;
    frame(80, 0, 0, 0, 0);              /* CliffQuick */
    idle_frames(1);                     /* CliffClimbQuick1 */
    mock_transn[1] = mock_transn[2] = 0.0f;
    idle_frames(2);                     /* CliffClimbQuick2, standing */
    CHECK_STATUS(nFTCommonStatusCliffClimbQuick2);
    mock_anim_len = 1e9f;               /* hold it while the test drives */
}

/* ftcommoncliffclimb.c:206-244 ftCommonCliffCommon2ProcPhysics, grounded
 * arm: the animation moves the fighter, not the stick. Cross-checked
 * against verbatim transcriptions of ftPhysicsApplyGroundVelTransN
 * (ft/ftphysics.c:172-173) and ftPhysicsSetGroundVelTransferAir
 * (ft/ftphysics.c:7-8). */
static void test_ledge_climb_root_motion(void)
{
    int i;

    reach_climb2();

    for (i = 1; i <= 4; i++)
    {
        float prev_z = mock_transn[2];
        float prev_x = mock_transn[0];
        float x0 = fp_pos.x;
        float angle_x = fp.coll_data.floor_angle.x;
        float angle_y = fp.coll_data.floor_angle.y;
        float want_ground_x, want_ground_z, want_air_x, want_air_y;

        mock_transn[2] = 12.0f * i;     /* the animation walking inward */
        mock_transn[0] = 3.0f * i;

        want_ground_x = (mock_transn[2] - prev_z) * kMario.size;
        want_ground_z = (mock_transn[0] - prev_x) * -fp.lr * kMario.size;
        want_air_x = fp.lr * angle_y * want_ground_x;
        want_air_y = fp.lr * -angle_x * want_ground_x;

        idle_frames(1);

        CHECK_STATUS(nFTCommonStatusCliffClimbQuick2);
        CHECK_NEAR(fp.physics.vel_ground.x, want_ground_x, 1e-3f);
        CHECK_NEAR(fp.physics.vel_ground.z, want_ground_z, 1e-3f);
        CHECK_NEAR(fp.physics.vel_air.x, want_air_x, 1e-3f);
        CHECK_NEAR(fp.physics.vel_air.y, want_air_y, 1e-3f);
        CHECK_NEAR(fp_pos.x, x0 + want_air_x, 1e-3f);
    }
    mock_anim_len = 1e9f;
}

/* ftcommoncliffescape.c: Z from the hang rolls up onto the stage. Same
 * two-part shape as the climb, but its ProcMap is the edge variant
 * (ftcommoncliffclimb.c:261-272) so the roll may leave the platform. */
static void test_ledge_roll(void)
{
    float edge_x = -2000.0f;

    reach_cliff_wait();
    mock_anim_len = 2.0f;

    frame(0, 0, N64_Z, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusCliffQuick);
    CHECK(fp.status_vars.common.cliffmotion.status_id ==
          nFTCommonCliffKindEscapeQuick);
    CHECK(fp.is_cliff_hold);

    idle_frames(1);
    CHECK_STATUS(nFTCommonStatusCliffEscapeQuick1);
    CHECK(fp.ga == nMPKineticsAir);

    mock_transn[1] = mock_transn[2] = 0.0f;
    idle_frames(2);
    CHECK_STATUS(nFTCommonStatusCliffEscapeQuick2);
    /* cliff_status_ga[EscapeQuick] is grounded for Mario */
    CHECK(fp.ga == nMPKineticsGround);
    CHECK(fp.coll_data.floor_line_id == LINE_DECK);
    CHECK_NEAR(fp_pos.x, edge_x + 5.0f, 1e-3f);

    idle_frames(2);
    CHECK_STATUS(nFTCommonStatusWait);
    mock_anim_len = 1e9f;
}

/* ft/ftcommon/ftcommoncliffattack.c, compiled unmodified
 * A or B from the hang is the ledge attack, the third release
 * beside the climb and the roll. Its check leads the ledge cascade
 * (ftcommoncliffcatchwait.c:89-96), so it wins over the roll's Z on a
 * frame that presses both -- which is what the second half of this test
 * asserts. The two halves are the same shape as every other release:
 * the wind-up hangs, the second half stands on the deck. */
static void test_ledge_attack(void)
{
    float edge_x = -2000.0f;

    reach_cliff_wait();
    mock_anim_len = 2.0f;

    frame(0, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusCliffQuick);
    CHECK(fp.status_vars.common.cliffmotion.status_id ==
          nFTCommonCliffKindAttackQuick);
    CHECK(fp.status_vars.common.cliffmotion.cliff_id == LINE_DECK);
    CHECK(fp.is_cliff_hold);

    /* ftcommoncliffattack.c:26-39: the wind-up ends in Quick1, which
     * still hangs and installs the ledge's own damage proc */
    idle_frames(1);
    CHECK_STATUS(nFTCommonStatusCliffAttackQuick1);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(fp.is_cliff_hold);
    CHECK(fp.proc_damage == ftCommonCliffCommonProcDamage);
    /* the row carries the attack ids the jab's row carries, which is
     * what lets the motion script's hitboxes stale and refresh */
    CHECK(fp.motion_attack_id == nFTMotionAttackIDCliffAttackQuick);
    CHECK(fp.stat_flags.attack_id == nFTStatusAttackIDCliffAttackQuick);

    /* ftcommoncliffattack.c:63-68: the second half places the fighter 5
     * units in from the vertex, as every other release's does. It is the
     * one release Mario's cliff_status_ga leaves airborne there
     * (203_MarioMain.c:331, index 1 of five), so it follows the cliff
     * line rather than integrating -- the arm test_ledge_climb_airborne
     * has to fake an attribute to reach. */
    mock_transn[1] = mock_transn[2] = 0.0f;
    idle_frames(2);
    CHECK_STATUS(nFTCommonStatusCliffAttackQuick2);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(!fp.is_cliff_hold);
    CHECK_NEAR(fp_pos.x, edge_x + 5.0f, 1e-3f);

    /* and it lands when the animation takes the root through the floor */
    mock_anim_len = 1e9f;
    mock_transn[1] = -30.0f;
    idle_frames(1);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK(fp.coll_data.floor_line_id == LINE_DECK);

    /* B is the same input, and the attack's check runs ahead of the
     * roll's: press Z and B together and the attack is what comes out */
    reach_cliff_wait();
    mock_anim_len = 2.0f;
    frame(0, 0, N64_B | N64_Z, N64_B | N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusCliffQuick);
    CHECK(fp.status_vars.common.cliffmotion.status_id ==
          nFTCommonCliffKindAttackQuick);
    mock_anim_len = 1e9f;

    /* above 100% it is the slow pair, through the same cliffmotion slot
     * (ftcommoncliffclimb.c:65-71 picks the tier, not the release) */
    reach_cliff_wait();
    fp.percent_damage = FTCOMMON_CLIFF_DAMAGE_HIGH;
    mock_anim_len = 2.0f;
    frame(0, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusCliffSlow);
    CHECK(fp.status_vars.common.cliffmotion.status_id ==
          nFTCommonCliffKindAttackSlow);
    idle_frames(1);
    CHECK_STATUS(nFTCommonStatusCliffAttackSlow1);
    CHECK(fp.motion_attack_id == nFTMotionAttackIDCliffAttackSlow);
    mock_transn[1] = mock_transn[2] = 0.0f;
    idle_frames(2);
    CHECK_STATUS(nFTCommonStatusCliffAttackSlow2);
    /* the slow pair reads index 4, which the ROM leaves FALSE: grounded,
     * unlike its quick twin */
    CHECK(fp.ga == nMPKineticsGround);
    CHECK(fp.coll_data.floor_line_id == LINE_DECK);
    idle_frames(2);
    CHECK_STATUS(nFTCommonStatusWait);
    mock_anim_len = 1e9f;
    fp.percent_damage = 0.0f;
}
