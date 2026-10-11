/* hosttest/presentation.c -- part of hosttest_ft.c: the battle camera and wallpaper, the auto-demo, How to Play, the
 * ending, the Room opening, camera anims and the staff roll.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* ---- layer 5: the battle camera -------------------------------------
 *
 * gm/gmcamera.c is verbatim, so this is a scenario check on its
 * integration: the camera GObj is made as scVSBattleStartBattle makes
 * it, runs its process per frame, and settles on the fighter. */
static void test_battle_camera(void)
{
    CObj *cobj;
    int i;

    spawn(-1500.0f, 100.0f);            /* standing on the deck at y=0 */
    /* the mock stage's camera bounds (kCamBounds... see stage tables):
     * generous, so the fighter's interest is inside them */
    gmCameraSetViewportDimensions(10, 10, 310, 230);
    gmCameraMakeBattleCamera();
    CHECK(gGMCameraGObj != NULL);
    cobj = CObjGetStruct(gGMCameraGObj);
    CHECK(gGMCameraStruct.status_curr == nGMCameraStatusDefault);
    CHECK_EQF(gGMCameraStruct.viewport_width, 300.0f);
    CHECK_EQF(cobj->projection.persp.aspect, 300.0f / 220.0f);
    /* gmcamera.c:1168-1176: the eye starts 10000 out, at (0, 300) */
    CHECK_EQF(cobj->vec.at.y, 300.0f);
    CHECK_NEAR(cobj->vec.eye.z, 10000.0f, 1.0f);

    for (i = 0; i < 300; i++)
    {
        frame(0, 0, 0, 0, 0);
        gmCameraRunFuncCamera(gGMCameraGObj);
    }
    /* one standing fighter, facing right: gmCameraUpdateInterests frames
     * it 1000 zoom-units ahead and 700 behind, so the box's centre --
     * the look-at -- sits 150 units ahead of it. The zoom is the
     * one-player 1.5 times camera_zoom (1.0), times 0.75 once Wait has
     * held 120 tics: 1.125, so 150 * 1.125 = 168.75 ahead. The lift
     * (gmCameraGetTargetAtY) is zero this close, so y is cam_offset_y.
     * The distance eases toward the clamp's 2500 floor, and the yaw
     * (func_ovl2_8010C3C0, -at.x / 133 degrees, capped at 17.5) swings
     * the eye toward the stage's centre. */
    CHECK_NEAR(cobj->vec.at.x, -1500.0f + 168.75f, 0.5f);
    CHECK_NEAR(cobj->vec.at.y, 250.0f, 0.5f);
    CHECK_NEAR(gGMCameraStruct.target_dist, 2500.0f, 1.0f);
    CHECK(cobj->vec.eye.z > 2000.0f && cobj->vec.eye.z < 2500.0f);
    CHECK(cobj->vec.eye.x > cobj->vec.at.x);
    CHECK_NEAR(cobj->vec.eye.x - cobj->vec.at.x,
               2500.0f * lbCommonSin(F_CLC_DTOR32(10.0f)), 15.0f);
    CHECK_EQF(cobj->projection.persp.fovy, 38.0f);

    /* gGMCameraMatrix, which gmCameraRunFuncCamera has just
     * rebuilt from this CObj, through ft/ftparam.c's projection -- what
     * the player tags position themselves with. The look-at point is
     * the viewport's centre by construction, and any other point is
     * checked against a pinhole camera worked from the CObj's vectors
     * alone: depth along the look direction, offsets along the derived
     * up and right, times the half-viewport in pixels (150 x 110) and
     * the perspective's 1 / tan(fovy / 2), the x over the aspect. */
    {
        Vec3f p;
        f32 x, y;
        f32 look[3], up[3], right[3], v[3], len, d, f;
        f32 px, py;

        func_ovl2_800EB924(cobj, gGMCameraMatrix, &cobj->vec.at, &x, &y);
        CHECK_NEAR(x, 0.0f, 0.001f);
        CHECK_NEAR(y, 0.0f, 0.001f);

        look[0] = cobj->vec.at.x - cobj->vec.eye.x;
        look[1] = cobj->vec.at.y - cobj->vec.eye.y;
        look[2] = cobj->vec.at.z - cobj->vec.eye.z;
        len = sqrtf(look[0] * look[0] + look[1] * look[1] + look[2] * look[2]);
        look[0] /= len; look[1] /= len; look[2] /= len;
        /* right = up x look with the game's negated look, i.e. look x up here */
        right[0] = look[1] * 0.0f - look[2] * 1.0f;
        right[1] = look[2] * 0.0f - look[0] * 0.0f;
        right[2] = look[0] * 1.0f - look[1] * 0.0f;
        len = sqrtf(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
        right[0] /= len; right[1] /= len; right[2] /= len;
        up[0] = right[1] * look[2] - right[2] * look[1];
        up[1] = right[2] * look[0] - right[0] * look[2];
        up[2] = right[0] * look[1] - right[1] * look[0];

        p.x = cobj->vec.at.x + 400.0f;
        p.y = cobj->vec.at.y + 250.0f;
        p.z = cobj->vec.at.z - 300.0f;
        v[0] = p.x - cobj->vec.eye.x;
        v[1] = p.y - cobj->vec.eye.y;
        v[2] = p.z - cobj->vec.eye.z;
        d = v[0] * look[0] + v[1] * look[1] + v[2] * look[2];
        f = 1.0f / tanf(F_CLC_DTOR32(38.0f * 0.5f));
        px = 150.0f * ((v[0] * right[0] + v[1] * right[1] + v[2] * right[2]) / d) * f / (300.0f / 220.0f);
        py = 110.0f * ((v[0] * up[0] + v[1] * up[1] + v[2] * up[2]) / d) * f;
        func_ovl2_800EB924(cobj, gGMCameraMatrix, &p, &x, &y);
        CHECK(px > 10.0f && py > 5.0f);
        CHECK_NEAR(x, px, 0.05f);
        CHECK_NEAR(y, py, 0.05f);
    }

    /* facing left biases the framing 1000 ahead, 700 behind: a second
     * fighter on the right pulls the box wider and the distance out */
    gcEjectGObj(gGMCameraGObj);
    gGMCameraGObj = NULL;
}

/* the battle camera's stage cameras, KO follow and weapon
 * framing. Planet Zebes and Mushroom Kingdom pick their own cameras; a
 * fighter KO'd off the top is framed at the top of the camera bounds,
 * scaled across by camera_bound_top / map_bound_top; and a weapon that
 * asks the camera to follow it (Yoshi's egg, the boomerang, PK Thunder)
 * widens the box. */
extern void gmCameraZebesFuncCamera(GObj *camera_gobj);
extern void gmCameraInishieFuncCamera(GObj *camera_gobj);
static void test_battle_camera_v03(void)
{
    u8 saved_gkind = mock_battle.gkind;
    Vec3f vec, want_vec;
    f32 hz, vt, want_hz, want_vt;
    f32 star_x;

    spawn(-900.0f, 100.0f);
    gmCameraSetViewportDimensions(10, 10, 310, 230);

    mock_battle.gkind = nGRKindZebes;
    gmCameraMakeBattleCamera();
    CHECK(gGMCameraStruct.status_curr == nGMCameraStatusZebes);
    CHECK(gGMCameraStruct.status_default == nGMCameraStatusZebes);
    CHECK(gGMCameraStruct.func_camera == gmCameraZebesFuncCamera);
    gcEjectGObj(gGMCameraGObj);

    mock_battle.gkind = nGRKindInishie;
    gmCameraMakeBattleCamera();
    CHECK(gGMCameraStruct.status_curr == nGMCameraStatusInishie);
    CHECK(gGMCameraStruct.func_camera == gmCameraInishieFuncCamera);
    gmCameraRunFuncCamera(gGMCameraGObj);
    gcEjectGObj(gGMCameraGObj);

    mock_battle.gkind = saved_gkind;
    gmCameraMakeBattleCamera();
    CHECK(gGMCameraStruct.status_curr == nGMCameraStatusDefault);
    gcEjectGObj(gGMCameraGObj);
    gGMCameraGObj = NULL;

    /* ---- the star KO: the interest of a DeadUp fighter is its dead.pos
     * put on the top bound, which is the interest a live fighter
     * standing exactly there would have ---- */
    star_x = (4000.0f * -900.0f) / 5000.0f;
    DObjGetStruct(mock_gobj)->translate.vec.f.x = star_x;
    DObjGetStruct(mock_gobj)->translate.vec.f.y = 4000.0f - fp.attr->cam_offset_y;
    gmCameraUpdateInterests(&want_vec, &want_hz, &want_vt);

    DObjGetStruct(mock_gobj)->translate.vec.f.x = 0.0f;
    DObjGetStruct(mock_gobj)->translate.vec.f.y = 300.0f;
    fp.status_vars.common.dead.pos.x = -900.0f;
    fp.status_vars.common.dead.pos.y = 9000.0f;
    fp.status_vars.common.dead.pos.z = 0.0f;
    fp.camera_mode = nFTCameraModeDeadUp;
    gmCameraUpdateInterests(&vec, &hz, &vt);
    CHECK_NEAR(vec.x, want_vec.x, 0.01f);
    CHECK_NEAR(vec.y, want_vec.y, 0.01f);
    CHECK_NEAR(hz, want_hz, 0.01f);
    CHECK_NEAR(vt, want_vt, 0.01f);
    fp.camera_mode = nFTCameraModeDefault;

    /* ---- a following weapon 800 ahead widens the box to its x + 1000;
     * one that does not follow leaves it alone ---- */
    {
        static WPStruct wp_follow;
        GObj *weapon_gobj;
        DObj *dobj;

        DObjGetStruct(mock_gobj)->translate.vec.f.x = -900.0f;
        DObjGetStruct(mock_gobj)->translate.vec.f.y = 300.0f;
        gmCameraUpdateInterests(&want_vec, &want_hz, &want_vt);

        weapon_gobj = gcMakeGObjSPAfter(nGCCommonKindWeapon, NULL, nGCCommonLinkIDWeapon, GOBJ_PRIORITY_DEFAULT);
        CHECK(weapon_gobj != NULL);
        if (weapon_gobj != NULL)
        {
            memset(&wp_follow, 0, sizeof(wp_follow));
            weapon_gobj->user_data.p = &wp_follow;
            dobj = gcAddDObjForGObj(weapon_gobj, NULL);
            dobj->translate.vec.f.x = -900.0f + 800.0f;
            dobj->translate.vec.f.y = 300.0f;
            dobj->translate.vec.f.z = 0.0f;

            gmCameraUpdateInterests(&vec, &hz, &vt);
            CHECK_NEAR(hz, want_hz, 0.01f);

            wp_follow.is_camera_follow = TRUE;
            gmCameraUpdateInterests(&vec, &hz, &vt);
            CHECK(hz > want_hz + 1.0f);
            /* the right edge: the weapon's x + 1000 */
            CHECK_NEAR(vec.x + hz, -900.0f + 800.0f + 1000.0f, 0.01f);

            gcEjectGObj(weapon_gobj);
        }
    }
    spawn(0.0f, 0.0f);
}

/* the wallpaper's placement per stage kind
 * (src/dc/grwallpaper.h): Yoshi's Island holds still, Sector Z zooms
 * about the centre without panning, and the rest pan with the view. */
static void test_wallpaper_placement(void)
{
    f32 px, py, scale;

    /* Yoshi's Island: fixed, whatever the camera does */
    stage_wallpaper_place(nGRKindYoster, 900.0f, -300.0f, 2000.0f, &px, &py, &scale);
    CHECK_EQF(px, 10.0f);
    CHECK_EQF(py, 10.0f);
    CHECK_EQF(scale, 1.0f);

    /* Sector Z, 2000 out: 20000 / 12000 = 5/3, centred */
    stage_wallpaper_place(nGRKindSector, 0.0f, 0.0f, 2000.0f, &px, &py, &scale);
    CHECK_NEAR(scale, 20000.0f / 12000.0f, 1e-4f);
    CHECK_NEAR(px, 10.0f - 300.0f * ((20000.0f / 12000.0f) - 1.0f) * 0.5f, 1e-3f);
    CHECK_NEAR(py, 10.0f - 220.0f * ((20000.0f / 12000.0f) - 1.0f) * 0.5f, 1e-3f);
    /* ...and an angled view moves nothing but the zoom */
    stage_wallpaper_place(nGRKindSector, 1200.0f, 0.0f, 1600.0f, &px, &py, &scale);
    CHECK_NEAR(scale, 20000.0f / 12000.0f, 1e-4f);
    CHECK_NEAR(px, 10.0f - 300.0f * ((20000.0f / 12000.0f) - 1.0f) * 0.5f, 1e-3f);
    /* far out, the 1.004 floor */
    stage_wallpaper_place(nGRKindSector, 0.0f, 0.0f, 30000.0f, &px, &py, &scale);
    CHECK_NEAR(scale, 1.004f, 1e-5f);

    /* the common pan: straight on at 2000 is 20000 / 10000 = 2, centred
     * (-140, -100); turned right, it pans with the view */
    stage_wallpaper_place(nGRKindHyrule, 0.0f, 0.0f, 2000.0f, &px, &py, &scale);
    CHECK_NEAR(scale, 2.0f, 1e-5f);
    CHECK_NEAR(px, -140.0f, 1e-3f);
    CHECK_NEAR(py, -100.0f, 1e-3f);
    stage_wallpaper_place(nGRKindHyrule, 400.0f, 0.0f, 2000.0f, &px, &py, &scale);
    CHECK(px > -140.0f);
}

/* ---- the auto-demo scene (src/dc/scautodemo.c) ---------
 *
 * Unlike scvsbattle.c's own tests above, scautodemo.c drives a whole
 * running match with camera-focus cycling over a real-time span (the
 * shortest of its six focus-table entries is 60 tics, the longest 400)
 * and a stage rotation meant to run for a disc-probe's whole duration --
 * not something a host test should try to play out to completion the
 * way test_battle_camera plays out 300 tics of one camera. What this
 * checks instead, proportionately: the pure helper functions in
 * isolation (popcount, the fighter shuffle), scAutoDemoInitDemo's own
 * state (every cycled stage, every player Com, the level and stock
 * fields) without any GObj at all, and the real focus-table state
 * machine driven tic-for-tic through all six of its entries -- the
 * same zoom/level/detail effects a target run would show, just without
 * waiting out 1,481 real frames on hardware to see them. */
static void test_autodemo(void)
{
    GObj *p2_gobj, *p3_gobj, *interface_gobj;
    FTDesc desc;
    SYController mock_in3, mock_in4;
    s32 i;

    /* scAutoDemoGetFighterKindsNum: popcount of a 16-bit mask */
    CHECK(scAutoDemoGetFighterKindsNum(0) == 0);
    CHECK(scAutoDemoGetFighterKindsNum(0xFFFF) == 16);
    CHECK(scAutoDemoGetFighterKindsNum(0x0B) == 3); /* 0b1011 */

    /* scAutoDemoGetShuffledFighterKind: the `random`-th set bit of
     * this_mask that prev_mask does not already have */
    CHECK(scAutoDemoGetShuffledFighterKind(0x0F, 0x00, 0) == 0);
    CHECK(scAutoDemoGetShuffledFighterKind(0x0F, 0x00, 3) == 3);
    CHECK(scAutoDemoGetShuffledFighterKind(0x0F, 0x05, 0) == 1); /* bit 0 excluded */
    CHECK(scAutoDemoGetShuffledFighterKind(0x0F, 0x05, 1) == 3); /* bits 0 and 2 excluded */

    /* scAutoDemoCheckStopFocusPlayer: only the three dead statuses cut
     * a focus short */
    spawn(-1500.0f, 100.0f);
    fpp->status_id = nFTCommonStatusWait;
    CHECK(scAutoDemoCheckStopFocusPlayer(fpp) == FALSE);
    fpp->status_id = nFTCommonStatusDeadDown;
    CHECK(scAutoDemoCheckStopFocusPlayer(fpp) != FALSE);
    fpp->status_id = nFTCommonStatusDeadLeftRight;
    CHECK(scAutoDemoCheckStopFocusPlayer(fpp) != FALSE);
    fpp->status_id = nFTCommonStatusDeadUpStar;
    CHECK(scAutoDemoCheckStopFocusPlayer(fpp) != FALSE);
    fpp->status_id = nFTCommonStatusWait;

    /* scAutoDemoInitDemo: every one of the eight cycled stages in
     * order, wrapping back to the first; every player Com at level 9;
     * gSCManagerSceneData.demo_fkind is {0, 0} on a plain boot (nothing
     * in this host build wires the title screen's own picker in) --
     * so players 0
     * and 1 both come back Mario. scAutoDemoInitDemo points
     * gSCManagerBattleState at its own static, not mock_battle, so this
     * section restores it before the focus-cycle section below. */
    memset(&gSCManagerSceneData, 0, sizeof(gSCManagerSceneData));
    for (i = 0; i < (s32)ARRAY_COUNT(dSCAutoDemoGroundOrder); i++)
    {
        scAutoDemoInitDemo();
        CHECK(gSCManagerBattleState->gkind == dSCAutoDemoGroundOrder[i]);
        CHECK(gSCManagerBattleState->game_type == nSCBattleGameTypeDemo);
        CHECK(gSCManagerBattleState->players[0].pkind == nFTPlayerKindCom);
        CHECK(gSCManagerBattleState->players[3].pkind == nFTPlayerKindCom);
        CHECK(gSCManagerBattleState->players[0].level == 9);
        CHECK(gSCManagerBattleState->players[0].fkind == nFTKindMario);
        CHECK(gSCManagerBattleState->players[1].fkind == nFTKindMario);
    }
    /* one full lap of the eight-entry order wraps the cycle index */
    CHECK(gSCManagerSceneData.demo_gkind_order == 0);

    /* ---- the focus-table state machine, driven tic-for-tic -------- */
    gSCManagerBattleState = &mock_battle;

    /* players 0 and 1: the real fixtures the camera actually zooms to.
     * players 2 and 3: scAutoDemoResetFocusPlayerAll (the "P2 focus"
     * entry's own func_change) touches all four unconditionally, so
     * two bare fighters stand in -- the same fixture recipe
     * spawn_second uses, just for the two slots that recipe does not
     * cover. */
    spawn(-1500.0f, 100.0f);
    spawn_second(1500.0f, 100.0f, -1);

    desc = dFTManagerDefaultFighterDesc;
    desc.fkind = nFTKindMario;
    desc.pos.x = 0.0f; desc.pos.y = 100.0f; desc.pos.z = 0.0f;
    desc.lr = 1;
    desc.team = 2;
    desc.player = 2;
    desc.detail = nFTPartsDetailLow;
    desc.shade = 1;
    desc.stock_count = mock_battle.stocks;
    desc.damage = 0;
    desc.pkind = nFTPlayerKindCom;
    desc.controller = &mock_in3;
    desc.is_skip_entry = TRUE;
    desc.figatree_heap = ftManagerAllocFigatreeHeapKind(nFTKindMario);
    memset(&mock_in3, 0, sizeof(mock_in3));
    mock_battle.players[2].pkind = nFTPlayerKindCom;
    mock_battle.players[2].fkind = nFTKindMario;
    p2_gobj = ftManagerMakeFighter(&desc);
    ftParamInitPlayerBattleStats(2, p2_gobj);

    desc.player = 3;
    desc.team = 3;
    desc.controller = &mock_in4;
    memset(&mock_in4, 0, sizeof(mock_in4));
    mock_battle.players[3].pkind = nFTPlayerKindCom;
    mock_battle.players[3].fkind = nFTKindMario;
    p3_gobj = ftManagerMakeFighter(&desc);
    ftParamInitPlayerBattleStats(3, p3_gobj);

    interface_gobj = scAutoDemoMakeFocusInterface();
    /* the interface's own "Nothing?" -> "Pre-focus" fall-through (both
     * inside scAutoDemoMakeFocusInterface) has already run
     * scAutoDemoMakeFade once, unconditionally -- the demo always
     * fades in from black before the camera ever moves. */

    /* "Pre-focus" holds 340 tics, then "Change to P1 focus"'s own
     * func_change (scAutoDemoSetFocusPlayer1) fires on the 340th. */
    for (i = 0; i < 340; i++)
    {
        scAutoDemoUpdateFocus();
    }
    CHECK(gGMCameraStruct.status_curr == nGMCameraStatusPlayerZoom);
    CHECK(gGMCameraStruct.pzoom_fighter_gobj == mock_gobj);
    CHECK(fpp->detail_base == nFTPartsDetailHigh);
    CHECK(fpp2->level == 1);
    CHECK(ftGetStruct(p2_gobj)->level == 1);
    CHECK(ftGetStruct(p3_gobj)->level == 1);
    CHECK(gIFCommonPlayerInterface.is_magnify_display == FALSE);

    /* "P1 focus" holds 340 more, then "Player 2 focus"'s own
     * func_change (scAutoDemoSetFocusPlayer2) fires. */
    for (i = 0; i < 340; i++)
    {
        scAutoDemoUpdateFocus();
    }
    CHECK(gGMCameraStruct.status_curr == nGMCameraStatusPlayerZoom);
    CHECK(gGMCameraStruct.pzoom_fighter_gobj == mock_gobj2);
    CHECK(fpp2->detail_base == nFTPartsDetailHigh);
    CHECK(fpp->level == 1);
    CHECK(ftGetStruct(p2_gobj)->level == 1);
    CHECK(ftGetStruct(p3_gobj)->level == 1);

    /* Entering "P2 focus" costs one more full lap of the wait a state
     * spends before ITS OWN change fires -- scAutoDemoChangeFocus reads
     * sSCAutoDemoFunc->focus_end_wait (the entry ptr is STILL on) before
     * advancing it, so the number of ticks a state occupies is the
     * PRECEDING entry's own wait field, not its own; "P1 focus"'s own
     * table wait (340, already spent above) is what "P2 focus" occupies
     * for -- see the file's own header comment on dSCAutoDemoFuncList
     * before changing these counts. scAutoDemoResetFocusPlayerAll fires
     * here, after 340 more, not "P2 focus"'s own listed 400 (that 400
     * is what the FOLLOWING state, "End focus", occupies instead). */
    for (i = 0; i < 340; i++)
    {
        scAutoDemoUpdateFocus();
    }
    CHECK(gGMCameraStruct.status_curr == nGMCameraStatusDefault);
    CHECK(fpp->level == 9);
    CHECK(fpp2->level == 9);
    CHECK(ftGetStruct(p2_gobj)->level == 9);
    CHECK(ftGetStruct(p3_gobj)->level == 9);
    CHECK(fpp2->detail_base == nFTPartsDetailLow);

    /* "End focus" occupies "P2 focus"'s own listed 400, then
     * scAutoDemoSetMagnifyDisplayOn fires. */
    for (i = 0; i < 400; i++)
    {
        scAutoDemoUpdateFocus();
    }
    CHECK(gIFCommonPlayerInterface.is_magnify_display != FALSE);

    /* "End demo" occupies "End focus"'s own listed 60, then
     * scAutoDemoExit fires: back to the title's own successor scene,
     * and the manager told to load it. */
    gSCManagerSceneData.scene_curr = nSCKindAutoDemo;
    for (i = 0; i < 60; i++)
    {
        scAutoDemoUpdateFocus();
    }
    CHECK(gSCManagerSceneData.scene_curr == nSCKindStartup);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindAutoDemo);

    /* scAutoDemoExit really called syTaskmanSetLoadScene above (the same
     * call ifCommonBattleSetUpdateInterface makes, test_stock_match_ends
     * below), and nothing on the target ever needs to undo it -- the
     * next scene's own syTaskmanRunTask resets sSYTaskmanStatus as its
     * first line before it is ever read again. This process never calls
     * syTaskmanRunTask between tests, so left alone the flag stays set
     * to nSYTaskmanStatusLoadScene for good and every later
     * is_scene_over()/syTaskmanCheckBreakLoop() call -- including
     * test_stock_match_ends's own -- reads TRUE before that test has
     * done anything. src/dc/taskman.c/.h's syTaskmanResetBreakLoop is
     * that missing reset, host-test-only like syTaskmanRunFrame and
     * syTaskmanSetupPools beside it. */
    syTaskmanResetBreakLoop();

    /* The interface GObj scAutoDemoMakeFocusInterface made: on the
     * target it lives for the rest of the scene and is swept with the
     * whole heap at the next scene change; this host test process never
     * changes scenes, so it must be ejected by hand -- left alive, its
     * own scAutoDemoFuncRun keeps firing on every later test's gcRunAll,
     * walking sSCAutoDemoFunc past the end of dSCAutoDemoFuncList (it
     * is parked one past "End demo" above) into whatever the process
     * heap holds next, which crashed test_stock_match_ends's own
     * battle_frame() the first time this test ran. */
    gcEjectGObj(interface_gobj);

    /* scAutoDemoMakeFade's own transition actor (nGCCommonLinkIDTransition,
     * lbfade.c): also never driven a tic by this test (nothing here
     * calls gcRunAll), so its own lbFadeProcUpdate would otherwise sit
     * on the link forever, ready to run its bounded countdown against
     * lbfade.c's global fade state the moment some later test's
     * gcRunAll reaches it -- harmless on its own (it is bounded and
     * self-ejects), but no later test should see a GObj this one did
     * not make. */
    while (gGCCommonLinks[nGCCommonLinkIDTransition] != NULL)
    {
        gcEjectGObj(gGCCommonLinks[nGCCommonLinkIDTransition]);
    }

    ftManagerDestroyFighter(p2_gobj);
    ftManagerDestroyFighter(p3_gobj);

    /* Restore mock_battle's baseline (test_setup's own: players 1-3
     * nFTPlayerKindNot) -- this test is the only one that puts players
     * 2/3 in play, and every test after it shares this one fixture. */
    mock_battle.players[2].pkind = nFTPlayerKindNot;
    mock_battle.players[2].fighter_gobj = NULL;
    mock_battle.players[3].pkind = nFTPlayerKindNot;
    mock_battle.players[3].fighter_gobj = NULL;
}

/* ftparam.c's two 1P-game bonus writers (ftParamUpdate1PGameAttackStats
 * and the counter half of ftParamUpdate1PGameDamageStats), stubs until
 * the 1P game existed. The attack side counts the attack the 1P player
 * is LEAVING, once, and only in a 1P game; the defend side counts a hit
 * the 1P player dealt, once per damage_stat_count. */
static void test_1pgame_bonus_stats(void)
{
    SCBattleState *saved = gSCManagerBattleState;
    static SCBattleState saved_mock;         /* later tests read its players */
    static FTStruct bonus_fp;                     /* too big for the stack */
    s32 saved_player = gSCManagerSceneData.player;
    GMStatFlags from, to;
    s32 from_id = nFTStatusAttackIDAttackStart + 1;
    s32 to_id = nFTStatusAttackIDAttackStart + 2;

    saved_mock = mock_battle;
    memset(&mock_battle, 0, sizeof(mock_battle));
    gSCManagerBattleState = &mock_battle;
    gSCManagerSceneData.player = 0;
    memset(gSC1PGameBonusAttackIDCount, 0, sizeof(u32) * nFTStatusAttackIDEnumCount);
    memset(gSC1PGameBonusAttackIsSmashCount, 0, sizeof(u32) * 2);
    memset(gSC1PGameBonusAttackGroundAirCount, 0, sizeof(u32) * 2);
    memset(gSC1PGameBonusAttackIsProjectileCount, 0, sizeof(u32) * 2);
    memset(gSC1PGameBonusDefendIDCount, 0, sizeof(u32) * nFTStatusAttackIDEnumCount);
    memset(gSC1PGameBonusDefendIsSmashCount, 0, sizeof(u32) * 2);

    memset(&bonus_fp, 0, sizeof(bonus_fp));
    bonus_fp.pkind = nFTPlayerKindMan;
    bonus_fp.player = 0;
    from.halfword = 0;
    from.attack_id = from_id;
    from.is_smash_attack = TRUE;
    to.halfword = 0;
    to.attack_id = to_id;
    bonus_fp.stat_flags = from;

    /* A VS game counts nothing. */
    mock_battle.game_type = nSCBattleGameTypeRoyal;
    ftParamUpdate1PGameAttackStats(&bonus_fp, to.halfword);
    CHECK(gSC1PGameBonusAttackIDCount[from_id] == 0);

    /* The 1P game counts the attack being left, by all four keys. */
    mock_battle.game_type = nSCBattleGameType1PGame;
    ftParamUpdate1PGameAttackStats(&bonus_fp, to.halfword);
    CHECK(gSC1PGameBonusAttackIDCount[from_id] == 1);
    CHECK(gSC1PGameBonusAttackIDCount[to_id] == 0);
    CHECK(gSC1PGameBonusAttackIsSmashCount[1] == 1);
    CHECK(gSC1PGameBonusAttackGroundAirCount[0] == 1);
    CHECK(gSC1PGameBonusAttackIsProjectileCount[0] == 1);

    /* The same attack again is not a change; another player's is not
     * the 1P player's. */
    ftParamUpdate1PGameAttackStats(&bonus_fp, from.halfword);
    bonus_fp.player = 1;
    ftParamUpdate1PGameAttackStats(&bonus_fp, to.halfword);
    CHECK(gSC1PGameBonusAttackIDCount[from_id] == 1);

    /* Defend side: a hit by the 1P player counts once per stat count. */
    bonus_fp.player = 1;
    ftParamUpdate1PGameDamageStats(&bonus_fp, 0, 0, 0, from.halfword, 7);
    ftParamUpdate1PGameDamageStats(&bonus_fp, 0, 0, 0, from.halfword, 7);
    CHECK(gSC1PGameBonusDefendIDCount[from_id] == 1);
    CHECK(gSC1PGameBonusDefendIsSmashCount[1] == 1);
    /* ...and a hit by someone else does not. */
    ftParamUpdate1PGameDamageStats(&bonus_fp, 2, 0, 0, from.halfword, 8);
    CHECK(gSC1PGameBonusDefendIDCount[from_id] == 1);

    mock_battle = saved_mock;
    gSCManagerBattleState = saved;
    gSCManagerSceneData.player = saved_player;
}

/* src/dc/scexplain.c: the How to Play demo. Unlike
 * test_autodemo above, this does not drive scExplainFuncStart -- that
 * setup chain (grStageAcquire, scExplainLoadExplainFiles's own
 * sprite_bank_load, scExplainSetPhaseSObjs's own SObj allocations) reads
 * romdisk assets no host test in this file loads for any scene (compare
 * test_autodemo, which never calls scAutoDemoFuncStart either -- only
 * the smaller, asset-free pieces underneath it). What is tested here:
 * scExplainUpdateArgsSObj's pure struct logic, scExplainDetectExit's
 * pure global-state transition, and -- the one that matters most --
 * ftKeyProcessKeyEvents (src/dc/ftcommon.c) walking
 * dSCExplainKeyEvent0, a real 1258-halfword canned
 * script, rather than a synthetic host-test-only one. */
static void test_explain(void)
{
    SObj sobj;
    SCExplainArgs args;
    s32 i;
    s32 have_key;

    /* ---- scExplainUpdateArgsSObj: scexplain.c:568-578, pure struct
     * logic, no static state touched -- sprite_status == 1 shows the
     * sprite and repositions it (offset +10, +160, the window's own
     * screen origin); anything else hides it, leaving position alone. */
    memset(&sobj, 0, sizeof(sobj));
    sobj.sprite.attr = SP_HIDDEN;
    args.sprite_status = 1;
    args.sprite_pos_x = 50;
    args.sprite_pos_y = 20;
    scExplainUpdateArgsSObj(&args, &sobj);
    CHECK(!(sobj.sprite.attr & SP_HIDDEN));
    CHECK(sobj.pos.x == 60.0f);
    CHECK(sobj.pos.y == 180.0f);

    sobj.pos.x = 60.0f;
    sobj.pos.y = 180.0f;
    args.sprite_status = 0;
    scExplainUpdateArgsSObj(&args, &sobj);
    CHECK(sobj.sprite.attr & SP_HIDDEN);
    /* position untouched when hidden */
    CHECK(sobj.pos.x == 60.0f);
    CHECK(sobj.pos.y == 180.0f);

    /* ---- scExplainDetectExit: scexplain.c:581-599, any pad's A/B/START
     * tap ends the demo early, straight back to the title -- the same
     * global-state shape scAutoDemoExit itself uses. */
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));
    gSCManagerSceneData.scene_curr = nSCKindExplain;
    gSCManagerSceneData.scene_prev = nSCKindTitle;
    scExplainDetectExit();
    CHECK(gSCManagerSceneData.scene_curr == nSCKindExplain); /* no tap yet */

    gSYControllerDevices[1].button_tap = B_BUTTON;
    scExplainDetectExit();
    CHECK(gSCManagerSceneData.scene_curr == nSCKindTitle);
    CHECK(gSCManagerSceneData.scene_prev == nSCKindExplain);
    syTaskmanResetBreakLoop(); /* scExplainDetectExit really called
                                * syTaskmanSetLoadScene -- see
                                * test_autodemo's own note on why this
                                * must be undone by hand between tests. */
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    /* ---- ftKeyProcessKeyEvents x dSCExplainKeyEvent0: the primitive's
     * first real exercise. Hand-traced from
     * the script's own first two entries --
     *   FTKEY_EVENT_STICK(0, 0, 0), FTKEY_EVENT_STICK(2, 0, 130), ...
     * -- ftParamSetKey leaves input_wait at 0, so the very first
     * ftKeyProcessKeyEvents call walks event 0 (t=0, immediately
     * superseded) AND event 1 (t=130) in the same call, landing on
     * stick_range (2, 0) with input_wait armed to 130. */
    spawn(-1500.0f, 100.0f);
    ftParamSetKey(mock_gobj, (FTKeyEvent *)dSCExplainKeyEvent0);
    CHECK(ftParamCheckHaveKey(mock_gobj) != FALSE);

    ftKeyProcessKeyEvents(mock_gobj);
    CHECK(fpp->key.input_wait == 130);
    CHECK(fpp->input.cp.stick_range.x == 2);
    CHECK(fpp->input.cp.stick_range.y == 0);

    /* 129 more calls only decrement input_wait -- the script does not
     * advance again until it reaches 0, and the call that finally
     * decrements it to 0 processes event 2 in that same call (the while
     * loop below input_wait-- never returns leaving input_wait at a
     * readable 0 -- it keeps consuming zero-wait events until it lands
     * on one with a positive wait of its own). */
    for (i = 0; i < 129; i++)
    {
        ftKeyProcessKeyEvents(mock_gobj);
    }
    CHECK(fpp->input.cp.stick_range.x == 2); /* still event 1's value */

    /* The 130th call decrements input_wait's last count to 0 and, in
     * that same call, reads event 2, FTKEY_EVENT_STICK(-32, 0, 1). */
    ftKeyProcessKeyEvents(mock_gobj);
    CHECK(fpp->key.input_wait == 1);
    CHECK(fpp->input.cp.stick_range.x == -32);
    CHECK(fpp->input.cp.stick_range.y == 0);

    /* Run the whole real script to its end (FTKEY_EVENT_END(), which
     * sets key.script back to NULL) and confirm ftKeyProcessKeyEvents
     * walks all 1258 halfwords -- roughly 600 STICK/BUTTON events, each
     * held for its own real tic count -- without ever running past the
     * script's own end or looping forever. A bounded cap (60000 tics,
     * about 15 real minutes at 60Hz -- the whole scripted routine is
     * nowhere near that long) catches a runaway walk as a test failure
     * instead of a hang. */
    have_key = ftParamCheckHaveKey(mock_gobj);
    for (i = 0; have_key != FALSE && i < 60000; i++)
    {
        ftKeyProcessKeyEvents(mock_gobj);
        have_key = ftParamCheckHaveKey(mock_gobj);
    }
    CHECK(have_key == FALSE);
    CHECK(fpp->key.script == NULL);

    /* The demo's two random streams (scexplain.c:45-49): the update
     * draws from the first and leaves the second alone, and the
     * overlay's reload puts both back to 1, since the generator writes
     * through the pointer and the N64 got them back from ROM. */
    dSCExplainRandomSeed1 = 0x00000001;
    dSCExplainRandomSeed2 = 0x00000001;
    syUtilsSetRandomSeedPtr(&dSCExplainRandomSeed1);
    (void)syUtilsRandUShort();
    syUtilsSetRandomSeedPtr(NULL);
    CHECK(dSCExplainRandomSeed1 != 0x00000001);
    CHECK(dSCExplainRandomSeed2 == 0x00000001);
    dSCExplainRandomSeed2 = 12345;
    scExplainOverlayLoad();
    CHECK(dSCExplainRandomSeed1 == 0x00000001);
    CHECK(dSCExplainRandomSeed2 == 0x00000001);
}

/* src/dc/mvending.c: the ending diorama. mvEndingFuncRun is
 * one tic counter (the whole scene, its own comment says); this drives
 * it real tic-for-tic from a clean mvEndingInitVars, past the light
 * cue at 340 (mvEndingMakeRoomLight -- a plain GObj + display proc, no
 * asset dependency, so safe to exercise for real), and stops there,
 * short of the 540 cue -- ftManagerDestroyFighter(sMVEndingFighterGObj)
 * needs a real posed fighter this test never makes (mvEndingFuncStart's
 * own setup chain reads no romdisk assets by itself, but
 * ftManagerSetupFilesAllKind/mvEndingMakeFighter do), the same
 * proportionate stop test_autodemo's own header note already explains
 * for its camera cycle. What this confirms: the counter runs 339 tics
 * without touching gSCManagerSceneData.scene_curr/scene_prev (the
 * hand-off does not fire early) and the 340th tic's own real side
 * effect does not crash. */
static void test_ending(void)
{
    s32 i;
    s32 scene_curr_before = gSCManagerSceneData.scene_curr;
    s32 scene_prev_before = gSCManagerSceneData.scene_prev;

    mvEndingInitVars();
    mvEndingInitVars();          /* idempotent */

    /* Stop at tic 339, one short of tic 340's own mvEndingMakeRoomLight
     * call: that call makes a real GObj (sMVEndingRoomLightGObj) with no
     * public accessor to eject it again from outside mvending.c, and
     * mvEndingFuncRun only ejects it itself at tic 540 -- past the point
     * this test can reach without also driving ftManagerDestroyFighter
     * against the fighter this test never made. Matches
     * test_autodemo's own "don't drive further than the test needs"
     * precedent (m19-presentation.md's leaked-GObj lesson): a GObj left
     * in gGCCommonLinks[] by a test that returns without ejecting it
     * corrupts a later, unrelated test's own use of that same slot. */
    for (i = 0; i < 339; i++)
    {
        mvEndingFuncRun(NULL);
        CHECK(gSCManagerSceneData.scene_curr == scene_curr_before);
        CHECK(gSCManagerSceneData.scene_prev == scene_prev_before);
    }

    /* The two fades are fill quads now, not raw gDPFillRectangle into
     * the scratch heads: nothing in the opaque or punch-through pass,
     * and one full-frame fill each in the translucent one -- black for
     * the fade-in, white for the light. */
    {
        static const int lists[] = { PVR_LIST_OP_POLY, PVR_LIST_PT_POLY };
        const LBCommonSpriteQuad *q;

        for (i = 0; i < 2; i++)
        {
            gcSetDrawList(lists[i]);
            gLBCommonSpriteQuadLogCount = 0;
            mvEndingRoomFadeInProcDisplay(NULL);
            mvEndingRoomLightProcDisplay(NULL);
            CHECK(gLBCommonSpriteQuadLogCount == 0);
        }
        gcSetDrawList(PVR_LIST_TR_POLY);
        gLBCommonSpriteQuadLogCount = 0;
        mvEndingRoomFadeInProcDisplay(NULL);
        mvEndingRoomLightProcDisplay(NULL);
        CHECK(gLBCommonSpriteQuadLogCount == 2);

        for (i = 0; i < 2; i++)
        {
            q = &gLBCommonSpriteQuadLog[i];
            CHECK(q->rect.copy == -1);
            CHECK_EQF(q->x0, 20.0f);
            CHECK_EQF(q->y0, 20.0f);
            CHECK_EQF(q->x1, 620.0f);
            CHECK_EQF(q->y1, 460.0f);
        }
        CHECK((gLBCommonSpriteQuadLog[0].argb & 0xFFFFFF) == 0x000000);
        CHECK((gLBCommonSpriteQuadLog[1].argb & 0xFFFFFF) == 0xFFFFFF);
        gLBCommonSpriteQuadLogCount = 0;
    }
}

/* src/dc/mvopeningroom.c: the openings' first scene. Four
 * things it can own on the host, none of them needing a romdisk asset:
 *
 *  - mvOpeningRoomInitVars's two random draws. The dropped kind is
 *    drawn in a retry loop (`while (fkind = ..., fkind == pulled);`)
 *    that reads the wall-clock-seeded syUtilsRandTimeUCharRange, so the
 *    only thing worth asserting is the invariant the loop exists for:
 *    both kinds are from the eight originals and they always differ.
 *    Run enough times that a loop that never retried would show.
 *  - mvOpeningRoomFuncRun tic-for-tic to 279, one short of the tic-280
 *    arm: that arm calls mvOpeningRoomMakePulledFighter, which needs a
 *    real loaded pack this test never sets up -- the same proportionate
 *    stop test_ending/test_autodemo already take. What it confirms is
 *    that nothing before 280 touches scene_curr/scene_prev, i.e. the
 *    skip-to-title branch does not fire on an idle pad and the tic-22s
 *    hand-off does not fire early.
 *  - mvOpeningRoomSetSpotlightPosition against a bare stack GObj/DObj:
 *    a pure table read, and the one piece of this scene's cut spotlight
 *    that is worth keeping (mvopeningroom.c says why it is ported at
 *    all). Checks the *30 scale-up on translate and the raw scale, for
 *    every one of the twelve rows including the four all-zero ones.
 *  - the two eject helpers against this scene's statics while they are
 *    all still NULL, which is what they are for the whole run given the
 *    room props are cut. This comment used to say gcEjectGObj(NULL) was
 *    a safe no-op the port relied on; it is not one. objman.c:1780-1786
 *    reads NULL as "eject me" and gcRunGObj ejects the RUNNING GObj the
 *    moment func_run returns, which inside a run walk kills the scene's
 *    own driver silently. Both helpers go through gcEjectGObjIfMade
 *    (src/dc/objpvr.h) now, here and in mvEndingEjectRoomGObjs, and
 *    calling them with every static NULL is the case that proves it. */
static void test_openingroom(void)
{
    static const s32 originals[] =
    {
        nFTKindMario, nFTKindFox, nFTKindDonkey, nFTKindSamus,
        nFTKindLink, nFTKindYoshi, nFTKindKirby, nFTKindPikachu
    };
    s32 i, j, k;
    s32 scene_curr_before, scene_prev_before;
    GObj gobj;
    DObj dobj;

    for (i = 0; i < 64; i++)
    {
        s32 pulled, dropped, pulled_ok = 0, dropped_ok = 0;

        mvOpeningRoomInitVars();

        pulled = mvOpeningRoomGetPulledFighterKind();
        dropped = mvOpeningRoomGetDroppedFighterKind();

        for (j = 0; j < (s32)ARRAY_COUNT(originals); j++)
        {
            if (originals[j] == pulled)
            {
                pulled_ok = 1;
            }
            if (originals[j] == dropped)
            {
                dropped_ok = 1;
            }
        }
        CHECK(pulled_ok);
        CHECK(dropped_ok);
    }

    /* An idle pad: scSubsysControllerGetPlayerTapButtons walks every
     * device, and a tap left behind by an earlier test would send this
     * scene to the title on its own tic 10. */
    memset(&gSYControllerDevices, 0, sizeof(gSYControllerDevices));

    mvOpeningRoomInitVars();

    scene_curr_before = gSCManagerSceneData.scene_curr;
    scene_prev_before = gSCManagerSceneData.scene_prev;

    for (i = 0; i < 279; i++)
    {
        mvOpeningRoomFuncRun(NULL);
        CHECK(gSCManagerSceneData.scene_curr == scene_curr_before);
        CHECK(gSCManagerSceneData.scene_prev == scene_prev_before);
    }

    memset(&gobj, 0, sizeof(gobj));
    memset(&dobj, 0, sizeof(dobj));
    gobj.obj = &dobj;

    for (k = 0; k < 12; k++)
    {
        mvOpeningRoomSetSpotlightPosition(&gobj, k);

        /* Every row's y is either 74.904 or 0, and translate is the
         * table times 30 -- the one arithmetic this function does. */
        CHECK(dobj.translate.vec.f.y == 74.904F * 30.0F ||
              dobj.translate.vec.f.y == 0.0F);
        /* Uniform in x/z, never scaled in y except when the whole row
         * is zero (the four 1P-only kinds). */
        CHECK(dobj.scale.vec.f.x == dobj.scale.vec.f.z);
        CHECK(dobj.scale.vec.f.y == 1.0F || dobj.scale.vec.f.y == 0.0F);
        CHECK((dobj.scale.vec.f.y == 0.0F) ==
              (dobj.translate.vec.f.y == 0.0F));
    }

    mvOpeningRoomEjectRoomGObjs();
    mvOpeningRoomEjectCameraGObjs();
}

/* src/dc/camanim.c + tools/export/ssb_camanimexport.py: the camera
 * animation banks.
 *
 * WHAT THIS HAS TO CATCH, and why it is not simply "run the parser".
 *
 * The port cannot run gcParseCObjCamAnimJoint here: `union AObjEvent32`
 * carries a `void *p`, so it is EIGHT bytes on x86-64 and four on the
 * SH-4, and the decomp's `AObjAnimAdvance(script)` is `script++` -- on
 * this host it would step two words at a time and never reach an End
 * (src/dc/gryamabuki.c guards its own gcAddAnimJointAll call for exactly
 * that). So the checks below walk the bank's words as u32 with the same
 * command decode the target's parser uses.
 *
 * That decode is `opcode = w >> 25` on BOTH, which is worth stating
 * because it looks wrong: sys/objtypes.h declares the command bitfield
 * in the reverse order (payload, flags, opcode) when the target is
 * little-endian, so the field that a big-endian compiler put in the top
 * seven bits stays in the top seven bits on sh-elf and on x86 alike.
 * The exporter therefore ships the ROM's word values unchanged, which is
 * what every other AnimJoint exporter in this tree already does.
 *
 * The failure this exists for is the one from
 * [[stage-layer-animation-gap]] and src/dc/stage.c:230-257: a script
 * whose Jump/SetAnim/SetInterp pointer word was carried over as the
 * absolute N64 address it is in the ROM. That does NOT crash. The
 * parser follows it, never finds an End, and spins inside the frame --
 * a black screen with no error. camanim_walk below is what makes it
 * loud: a pointer word that is not the address of a word in this bank
 * is a failure, not a jump. */
#define CAM_AJ_END          0
#define CAM_AJ_JUMP         1
#define CAM_AJ_WAIT         2
#define CAM_AJ_SETVALRATE_B 5
#define CAM_AJ_SETVALRATE   6
#define CAM_AJ_ADDLENGTH   12
#define CAM_AJ_SETINTERP   13
#define CAM_AJ_SETANIM     14
#define CAM_AJ_SETFLAGS    15
#define CAM_AJ_FUNCANIM    16
#define CAM_AJ_FUNCANIM_TR 17

/* The ten camera tracks, nGCAnimTrackEyeX..nGCAnimTrackFovY
 * (sys/objdef.h:248-259). gcParseCObjCamAnimJoint's flag loop runs
 * exactly that many times and then stops advancing the script, so a bit
 * above bit 9 would desynchronise its own pc. */
#define CAM_TRACKS 10

/* A pointer word, after camanim_bank_load has relocated it, is the
 * address of a word in this bank -- truncated to 32 bits, which is what
 * the port stores everywhere (src/dc/fighter.c does the same for a
 * fighter pack's AnimJoint animations). Find which word it names; -1
 * when it names none, which is the failure above. */
static int camanim_word_at(const u32 *words, u32 n, u32 value)
{
    u32 i;

    for (i = 0; i < n; i++)
    {
        if ((u32)(uintptr_t)(words + i) == value)
        {
            return (int)i;
        }
    }
    return -1;
}

/* Walk one script from word `start`. Returns the index of its End word,
 * or -1 with a printed reason. Never loops: a word visited twice is a
 * failure, which is the spin this test is for. */
static int camanim_walk(const u32 *words, u32 n, u32 start, u8 *seen)
{
    u32 pc = start;

    memset(seen, 0, n);
    for (;;)
    {
        u32 w, op, flags, nvals;

        if (pc >= n)
        {
            printf("camanim refused: walk left the bank at word %u of %u\n",
                   pc, n);
            return -1;
        }
        if (seen[pc])
        {
            printf("camanim refused: walk revisits word %u -- the parser "
                   "would spin here\n", pc);
            return -1;
        }
        seen[pc] = 1;
        w = words[pc];
        op = (w >> 25) & 0x7F;
        flags = (w >> 15) & 0x3FF;

        if (op == CAM_AJ_END)
        {
            return (int)pc;
        }
        if (op == CAM_AJ_JUMP || op == CAM_AJ_SETANIM ||
            op == CAM_AJ_SETINTERP)
        {
            int t;

            if (pc + 1 >= n)
            {
                printf("camanim refused: pointer word %u is past the bank\n",
                       pc + 1);
                return -1;
            }
            t = camanim_word_at(words, n, words[pc + 1]);
            if (t < 0)
            {
                printf("camanim refused: word %u holds 0x%08X, which is no "
                       "word of this bank -- an unrelocated pointer\n",
                       pc + 1, (unsigned)words[pc + 1]);
                return -1;
            }
            seen[pc + 1] = 1;
            /* SetInterp's pointer is a curve descriptor, not a branch:
             * the script goes on past it (objanim.c:2763). */
            pc = (op == CAM_AJ_SETINTERP) ? pc + 2 : (u32)t;
            continue;
        }
        pc++;
        if (op == CAM_AJ_WAIT || op == CAM_AJ_ADDLENGTH ||
            op == CAM_AJ_SETFLAGS || op == CAM_AJ_FUNCANIM)
        {
            continue;
        }
        if (flags >> CAM_TRACKS)
        {
            printf("camanim refused: command %u sets flag bit %d, past the "
                   "%d camera tracks\n", pc - 1, 31 - __builtin_clz(flags),
                   CAM_TRACKS);
            return -1;
        }
        if (op == CAM_AJ_FUNCANIM_TR)
        {
            nvals = (u32)__builtin_popcount(flags & 0x3FF);
        }
        else if (op >= 3 && op <= 12)
        {
            nvals = (u32)__builtin_popcount(flags) *
                    ((op == CAM_AJ_SETVALRATE_B || op == CAM_AJ_SETVALRATE)
                     ? 2u : 1u);
        }
        else
        {
            printf("camanim refused: opcode %u at word %u is not an "
                   "AObjEvent32Kind\n", op, pc - 1);
            return -1;
        }
        pc += nvals;
    }
}

/* One bank's every script walked end to end. */
static int camanim_check_bank(CamAnimBank *bank, const char *what)
{
    u8 *seen;
    u32 i;
    int bad = 0;

    seen = malloc(bank->nwords ? bank->nwords : 1);
    if (seen == NULL)
    {
        return 1;
    }
    for (i = 0; i < bank->count; i++)
    {
        u32 first = (u32)(bank->entries[i].words - bank->words);
        int end = camanim_walk(bank->words, bank->nwords, first, seen);

        if (end < 0)
        {
            printf("  (in %s, script %s)\n", what, bank->entries[i].name);
            bad++;
            continue;
        }
        /* The exporter cuts each script at the End it walks to, so the
         * End must be the entry's LAST word -- not merely somewhere
         * inside it. A shorter walk means the exporter and the parser
         * disagree about the language. */
        if ((u32)end != first + bank->entries[i].nwords - 1)
        {
            printf("camanim mismatch: %s/%s ends at word %d, its last word is "
                   "%u\n", what, bank->entries[i].name, end,
                   first + bank->entries[i].nwords - 1);
            bad++;
        }
    }
    free(seen);
    return bad;
}

/* Build a one-script .cam by hand, for the cases the ROM has no example
 * of. `reloc_value` is what the pointer word carries before the load:
 * a word index (what the exporter writes) or, for the regression, the
 * absolute N64 address the ROM has. */
static void camanim_write(const char *path, const u32 *words, u32 nwords,
                          const u32 *relocs, u32 nreloc, const char *magic,
                          long truncate_to)
{
    u8 buf[1024];
    u32 off_dir = 32, off_words = 32 + 40;
    u32 off_reloc = off_words + 4 * nwords;
    u32 hdr[6];
    u32 dir[2];
    long len = (long)(off_reloc + 4 * nreloc);
    /* NOT `fp`: this file carries `#define fp (*fpp)` for the fighter
     * under test (above), so a local of that name silently becomes a
     * write through an uninitialised pointer. */
    FILE *out;

    memset(buf, 0, sizeof(buf));
    memcpy(buf, magic, 8);
    hdr[0] = 1; hdr[1] = nwords; hdr[2] = nreloc;
    hdr[3] = off_dir; hdr[4] = off_words; hdr[5] = off_reloc;
    memcpy(buf + 8, hdr, sizeof(hdr));
    memcpy(buf + off_dir, "Only", 5);
    dir[0] = 0; dir[1] = nwords;
    memcpy(buf + off_dir + CAMANIM_NAME_LEN, dir, sizeof(dir));
    memcpy(buf + off_words, words, 4 * nwords);
    if (nreloc != 0)
    {
        memcpy(buf + off_reloc, relocs, 4 * nreloc);
    }
    if (truncate_to > 0 && truncate_to < len)
    {
        len = truncate_to;
    }
    out = fopen(path, "wb");
    CHECK(out != NULL);
    if (out != NULL)
    {
        fwrite(buf, 1, (size_t)len, out);
        fclose(out);
    }
}

static void test_camanim(void)
{
    /* The three banks the build ships, against what
     * `ssb_camanimexport.py --list` prints for them. The word counts are
     * the walker's own answer on the ROM, so a bank rebuilt from a
     * different ROM revision, or an exporter that stopped at the wrong
     * word, shows up here and not as a camera that films the floor. */
    static const struct { const char *name; u32 nwords; } kRoom[4] =
    {
        { "Scene1", 249 }, { "Scene2", 17 },
        { "Scene3", 27 },  { "Scene4", 53 }
    };
    /* MVOpeningCommon (file 65) is the one file all eight per-fighter
     * opening scenes take their camera out of; none of those scenes is
     * ported, so this bank is checked here and read nowhere else yet. */
    static const char *const kCommon[8] =
    {
        "Mario", "Donkey", "Samus", "Fox", "Link", "Yoshi", "Pikachu",
        "Kirby"
    };
    CamAnimBank bank;
    u32 words[8], relocs[2];
    u32 i;

    memset(&bank, 0, sizeof(bank));
    CHECK(camanim_bank_load(&bank, "mvopeningroom.cam") == 0);
    CHECK(bank.count == 4);
    CHECK(bank.nwords == 249 + 17 + 27 + 53);
    for (i = 0; i < 4 && i < bank.count; i++)
    {
        CHECK(strcmp(bank.entries[i].name, kRoom[i].name) == 0);
        CHECK(bank.entries[i].nwords == kRoom[i].nwords);
        CHECK(camanim_get(&bank, kRoom[i].name) ==
              (AObjEvent32 *)bank.entries[i].words);
    }
    CHECK(camanim_check_bank(&bank, "mvopeningroom.cam") == 0);
    /* A name the bank does not have is a build error, and answers NULL
     * rather than something wrong -- which gcAddCObjCamAnimJoint takes
     * (the camera just holds still). */
    CHECK(camanim_get(&bank, "Scene5") == NULL);

    memset(&bank, 0, sizeof(bank));
    CHECK(camanim_bank_load(&bank, "mvopeningcommon.cam") == 0);
    CHECK(bank.count == 8);
    for (i = 0; i < 8 && i < bank.count; i++)
    {
        CHECK(strcmp(bank.entries[i].name, kCommon[i]) == 0);
        CHECK(bank.entries[i].nwords == 10);
    }
    CHECK(camanim_check_bank(&bank, "mvopeningcommon.cam") == 0);

    memset(&bank, 0, sizeof(bank));
    CHECK(camanim_bank_load(&bank, "mvending.cam") == 0);
    CHECK(bank.count == 1);
    CHECK(bank.entries[0].nwords == 316);
    CHECK(strcmp(bank.entries[0].name, "Operator") == 0);
    CHECK(camanim_check_bank(&bank, "mvending.cam") == 0);

    /* ---- the relocation, which no shipped bank exercises -------------
     *
     * None of the thirteen scripts in the ROM has a pointer word at all,
     * so the three checks below are the only thing that holds the
     * machinery honest. Script: Jump -> word 2, which is End. */
    words[0] = (u32)CAM_AJ_JUMP << 25;
    words[1] = 2;                       /* the exporter writes an INDEX */
    words[2] = (u32)CAM_AJ_END << 25;
    relocs[0] = 1;

    camanim_write("./hosttest_camanim.cam", words, 3, relocs, 1,
                  "SSBCAM1", 0);
    memset(&bank, 0, sizeof(bank));
    CHECK(camanim_bank_load(&bank, "./hosttest_camanim.cam") == 0);
    CHECK(bank.nwords == 3);
    /* The index became this bank's own address, not the index and not
     * anything else. */
    CHECK(bank.words[1] == (u32)(uintptr_t)(bank.words + 2));
    CHECK(bank.words[1] != 2);
    CHECK(camanim_check_bank(&bank, "synthetic jump") == 0);

    /* THE REGRESSION. The same script with the pointer word left as the
     * absolute N64 address the ROM carries and nothing listed to
     * relocate -- which is what a memcpy out of the relocData file
     * produces. The load cannot tell (there is no relocation to check),
     * and on the target the parser would follow 0x800F1234, never reach
     * an End and spin: a black screen, no crash, no log line. The walk
     * is what refuses it. */
    words[1] = 0x800F1234u;
    camanim_write("./hosttest_camanim.cam", words, 3, relocs, 0,
                  "SSBCAM1", 0);
    memset(&bank, 0, sizeof(bank));
    CHECK(camanim_bank_load(&bank, "./hosttest_camanim.cam") == 0);
    CHECK(bank.words[1] == 0x800F1234u);
    {
        u8 seen[3];

        CHECK(camanim_walk(bank.words, bank.nwords, 0, seen) == -1);
    }

    /* A relocation whose target is outside the bank is refused at load,
     * because the pointer it would write is that same silent hang. */
    words[1] = 99;
    camanim_write("./hosttest_camanim.cam", words, 3, relocs, 1,
                  "SSBCAM1", 0);
    memset(&bank, 0, sizeof(bank));
    CHECK(camanim_bank_load(&bank, "./hosttest_camanim.cam") == -1);
    CHECK(!bank.is_loaded);
    CHECK(camanim_get(&bank, "Only") == NULL);

    /* And the two ways a bank can be the wrong file altogether. */
    words[1] = 2;
    camanim_write("./hosttest_camanim.cam", words, 3, relocs, 1,
                  "SSBSPR1", 0);
    memset(&bank, 0, sizeof(bank));
    CHECK(camanim_bank_load(&bank, "./hosttest_camanim.cam") == -1);

    camanim_write("./hosttest_camanim.cam", words, 3, relocs, 1,
                  "SSBCAM1", 80);
    memset(&bank, 0, sizeof(bank));
    CHECK(camanim_bank_load(&bank, "./hosttest_camanim.cam") == -1);

    memset(&bank, 0, sizeof(bank));
    CHECK(camanim_bank_load(&bank, "./no-such-file.cam") == -1);
    CHECK(camanim_get(&bank, "Only") == NULL);

    remove("./hosttest_camanim.cam");
}

/* src/dc/scstaffroll.c: the staff roll. The pure hit-test
 * math and the pure position clamp, driven directly with known input;
 * SCStaffrollNameUpdateAlloc/SetPrevAlloc's own free-list, on a bare
 * stack GObj (no scene setup at all -- both only ever touch
 * gobj->user_data.p and the allocator's own free list); and
 * scStaffrollTryHideUnlocks against the real gSCManagerBackupData,
 * confirming directly that it is read-only
 * (scstaffroll.h's header note) -- unlock_mask comes back bit-for-bit
 * whatever it was set to, whichever names it question-marks in the
 * compiled-in credit text along the way. */
static void test_staffroll_curve(void);

static void test_staffroll(void)
{
    Mtx44f identity =
    {
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
    };
    Vec3f origin;
    f32 width, height;
    GObj gobj;
    SCStaffrollName *cn, *reused;
    u8 mask_before;

    /* scStaffrollGetLockOnPositionX/Y: a plain clamp, checked at both
     * edges and in range. */
    CHECK(scStaffrollGetLockOnPositionX(-100) == 20);
    CHECK(scStaffrollGetLockOnPositionX(300) == 300);
    CHECK(scStaffrollGetLockOnPositionX(9999) == 620);
    CHECK(scStaffrollGetLockOnPositionY(-100) == 20);
    CHECK(scStaffrollGetLockOnPositionY(300) == 300);
    CHECK(scStaffrollGetLockOnPositionY(9999) == 460);

    /* func_ovl59_80131BB0: an identity matrix puts the origin dead
     * center of the 640x480 screen -- width = height = 0. */
    origin.x = origin.y = origin.z = 0.0f;
    func_ovl59_80131BB0(identity, &origin, &width, &height);
    CHECK_EQF(width, 0.0f);
    CHECK_EQF(height, 0.0f);

    /* SCStaffrollNameUpdateAlloc: a fresh alloc zeroes the bookkeeping
     * fields and stashes itself on the GObj; SetPrevAlloc returns it to
     * the free list, and the next UpdateAlloc reuses that same pointer
     * rather than allocating again. */
    memset(&gobj, 0, sizeof(gobj));
    cn = SCStaffrollNameUpdateAlloc(&gobj);
    CHECK(cn != NULL);
    CHECK(gobj.user_data.p == cn);
    CHECK_EQF(cn->offset_x, 0.0f);
    CHECK_EQF(cn->unkgmcreditsstruct0x10, 0.0f);
    CHECK_EQF(cn->interpolation, 0.0f);
    CHECK(cn->status == 0);
    SCStaffrollNameSetPrevAlloc(cn);
    reused = SCStaffrollNameUpdateAlloc(&gobj);
    CHECK(reused == cn);

    /* scStaffrollTryHideUnlocks: read-only against the real save data,
     * whichever way the mask points. */
    mask_before = gSCManagerBackupData.unlock_mask;
    gSCManagerBackupData.unlock_mask = 0;
    scStaffrollTryHideUnlocks();
    CHECK(gSCManagerBackupData.unlock_mask == 0);
    gSCManagerBackupData.unlock_mask = 0xFF;
    scStaffrollTryHideUnlocks();
    CHECK(gSCManagerBackupData.unlock_mask == 0xFF);
    gSCManagerBackupData.unlock_mask = mask_before;

    /* dSCStaffrollTextBoxSpriteInfo, the popup's font: every glyph is a
     * sprite of its own in file 195. Until 2026-09-24 the table read the
     * file's interleaved AUpper/ALower/BUpper... run as capitals then
     * small letters, and the small letters, digits, colon and period
     * landed on each other's sprites -- the garbled popup. The table
     * has 74 entries, one sprite each, none shared. */
    {
        s32 n = (s32)(sizeof(dSCStaffrollTextBoxSpriteInfo) / sizeof(dSCStaffrollTextBoxSpriteInfo[0]));
        s32 i, j, shared = 0;

        CHECK(n == 74);
        for (i = 0; i < n; i++)
        {
            for (j = i + 1; j < n; j++)
            {
                if (dSCStaffrollTextBoxSpriteInfo[i].offset == dSCStaffrollTextBoxSpriteInfo[j].offset)
                {
                    shared++;
                }
            }
        }
        CHECK(shared == 0);
        /* a and z, the digit 1 and the period, by the file's own offsets */
        CHECK(dSCStaffrollTextBoxSpriteInfo[26].offset == 0x03310);
        CHECK(dSCStaffrollTextBoxSpriteInfo[51].offset == 0x05ab0);
        CHECK(dSCStaffrollTextBoxSpriteInfo[61].offset == 0x05de8);
        CHECK(dSCStaffrollTextBoxSpriteInfo[63].offset == 0x05c90);
    }

    test_staffroll_curve();
}

/* src/dc/scstaffroll.c: the curve and the lean the credit
 * roll rides, compiled in out of relocData/195_SCStaffroll.c rather
 * than exported (scstaffroll.h (3)). What the original cut was not the
 * glyphs alone -- it was this, and without it every line sat at the
 * origin whether or not it had letters. It is transcribed numbers, so
 * what it needs is exactly what a host test gives: the WORDS held
 * against the decomp's own, and the CURVE driven end to end.
 *
 * The two script words are the decomp's own aobjEvent32SetValBlock
 * expansions, spelled out: opcode SetValBlock in bits 25..31, the ROTZ
 * track bit in 15..24, the frame in 0..14. Asserting the whole word
 * also pins the port's build of the decomp's macros, which is the one
 * thing here that is not a literal.
 *
 * The curve is checked for what scStaffrollJobAndNameThreadUpdate
 * depends on and nothing more. NOT for passing through its endpoints:
 * six keyframes over eight control points is a quadratic spline
 * (syInterpQuadSpline, three points a segment), which interpolates
 * neither end -- measured here, it starts and ends well inside them.
 * What it must do is stay inside the control points' own convex hull
 * (which is the whole guarantee such a spline gives), carry a line a
 * long way over its 0 -> 1, recede monotonically in z and climb in y.
 * That last pair is the motion itself: away from the camera and up the
 * screen. */
static void test_staffroll_curve(void)
{
    Vec3f p0, pm, p1;
    u32 w;

    /* the script: two ramps and an End, and nothing else */
    CHECK(dSCStaffrollNameAnimJointScript[4] == aobjEvent32End());

    w = dSCStaffrollNameAnimJointScript[0];
    CHECK(((w >> 25) & 0x7F) == (u32)nGCAnimEvent32SetValBlock);
    CHECK(((w >> 15) & 0x3FF) == (u32)AOBJ_FLAG_ROTZ);
    CHECK((w & 0x7FFF) == 0);

    w = dSCStaffrollNameAnimJointScript[2];
    CHECK(((w >> 25) & 0x7F) == (u32)nGCAnimEvent32SetValBlock);
    CHECK(((w >> 15) & 0x3FF) == (u32)AOBJ_FLAG_ROTZ);
    CHECK((w & 0x7FFF) == 99);

    /* the two lean values, as the ROM's own f32 words */
    CHECK(dSCStaffrollNameAnimJointScript[1] == 0xBE427301U);
    CHECK(dSCStaffrollNameAnimJointScript[3] == 0x3ED67750U);

    /* the descriptor points at this file's own three arrays */
    CHECK(dSCStaffrollNameInterpolationDesc[0].kind == nSYInterpKindBezier);
    CHECK(dSCStaffrollNameInterpolationDesc[0].points_num == 6);
    CHECK(dSCStaffrollNameInterpolationDesc[0].points == dSCStaffrollNamePoints);
    CHECK(dSCStaffrollNameInterpolationDesc[0].keyframes == dSCStaffrollNameKeyframes);
    CHECK(dSCStaffrollNameInterpolationDesc[0].quartics == dSCStaffrollNameQuartics);

    /* and it runs, endpoint to endpoint, receding all the way */
    syInterpCubic(&p0, dSCStaffrollNameInterpolationDesc, 0.0f);
    syInterpCubic(&pm, dSCStaffrollNameInterpolationDesc, 0.5f);
    syInterpCubic(&p1, dSCStaffrollNameInterpolationDesc, 1.0f);

    /* inside the control points' own bounding box, all three */
    {
        Vec3f lo = dSCStaffrollNamePoints[0];
        Vec3f hi = dSCStaffrollNamePoints[0];
        const Vec3f *s[3];
        s32 i, k;

        for (i = 1; i < 8; i++)
        {
            lo.x = (dSCStaffrollNamePoints[i].x < lo.x) ? dSCStaffrollNamePoints[i].x : lo.x;
            lo.y = (dSCStaffrollNamePoints[i].y < lo.y) ? dSCStaffrollNamePoints[i].y : lo.y;
            lo.z = (dSCStaffrollNamePoints[i].z < lo.z) ? dSCStaffrollNamePoints[i].z : lo.z;
            hi.x = (dSCStaffrollNamePoints[i].x > hi.x) ? dSCStaffrollNamePoints[i].x : hi.x;
            hi.y = (dSCStaffrollNamePoints[i].y > hi.y) ? dSCStaffrollNamePoints[i].y : hi.y;
            hi.z = (dSCStaffrollNamePoints[i].z > hi.z) ? dSCStaffrollNamePoints[i].z : hi.z;
        }
        s[0] = &p0; s[1] = &pm; s[2] = &p1;

        for (k = 0; k < 3; k++)
        {
            CHECK(s[k]->x >= lo.x && s[k]->x <= hi.x);
            CHECK(s[k]->y >= lo.y && s[k]->y <= hi.y);
            CHECK(s[k]->z >= lo.z && s[k]->z <= hi.z);
        }
    }
    /* and it travels a long way doing it */
    CHECK(fabsf(p1.z - p0.z) > 500.0f);

    CHECK(p0.z > pm.z);
    CHECK(pm.z > p1.z);
    /* a line also climbs the screen as it goes */
    CHECK(p1.y > p0.y);
}

/* src/dc/scautodemo.c: the twelve name-plate sprite keys,
 * in nFTKind order. scAutoDemoInitSObjs itself needs a scene heap, a
 * GObj pool and a loaded bank, none of which this build has -- but the
 * table it indexes is transcribed literals out of
 * tools/export/ssb_spriteexport.py --file 12 --list, and that is what can go
 * wrong silently. Ordered, distinct, and each one where the tool put
 * it. */
static void test_autodemo_names(void)
{
    static const u32 want[12] =
    {
        0x00138, 0x00258, 0x00378, 0x004f8, 0x00618, 0x00738,
        0x00858, 0x00a38, 0x00bb8, 0x00d38, 0x00f78, 0x01098
    };
    s32 i;

    for (i = 0; i < 12; i++)
    {
        CHECK(dSCAutoDemoFighterNameSpriteOffsets[i] == want[i]);

        if (i != 0)
        {
            CHECK(dSCAutoDemoFighterNameSpriteOffsets[i] >
                  dSCAutoDemoFighterNameSpriteOffsets[i - 1]);
        }
    }
    /* Mario is nFTKindMario, and the table is indexed by fkind */
    CHECK(dSCAutoDemoFighterNameSpriteOffsets[nFTKindMario] == 0x00138);
    CHECK(dSCAutoDemoFighterNameSpriteOffsets[nFTKindNess] == 0x01098);
}

static void test_respawn(void)
{
    Vec3f rebirth;

    spawn(0.0f, 0.0f);
    mpCollisionGetPlayerMapObjPosition(nMPMapObjKindRebirth, &rebirth);
    ftMainRespawn(mock_gobj, rebirth.x, rebirth.y);
    CHECK_EQF(fp_pos.x, 0.0f);
    CHECK_EQF(fp_pos.y, 2500.0f);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK_EQF(fp.physics.vel_air.y, 0.0f);

    run_until(0, 0, 200, is_grounded);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK_EQF(fp_pos.y, 300.0f);
}
