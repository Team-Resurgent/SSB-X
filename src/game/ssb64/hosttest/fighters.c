/* hosttest/fighters.c -- part of hosttest_ft.c: the fighters' own moves: specials per fighter and their demuxes,
 * aerials, the roster's per-fighter tables, and the damage statuses.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* ft/ftchar/ftmario/ftmariospeciallw.c, compiled unmodified
 * the Tornado, and with it the first Down-B the port can enter.
 * Three things are under test and only one of them is Mario's -- the
 * cascade check (a hand port of ftcommonspeciallw.c:48-58, because the
 * decomp's file names all twelve fighters' setters and only five are
 * in), the per-fighter status-table dispatch, and the move itself. */
static void test_downb_tornado(void)
{
    spawn(0.0f, 0.0f);
    CHECK_STATUS(nFTCommonStatusWait);

    /* B with a neutral stick is not the Tornado: ftcommonspeciallw.c:53
     * wants the stick below FTCOMMON_SPECIALLW_STICK_RANGE_MIN as well.
     * The neutral-B demux sits ahead of the down-special in
     * the Wait cascade and takes the tap first -- a neutral B is the
     * fireball, not the Tornado. Re-spawn back to Wait afterwards. */
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialN);
    spawn(0.0f, 0.0f);
    CHECK_STATUS(nFTCommonStatusWait);

    /* The setter on its own first (ftmariospeciallw.c:145-160): even
     * from the ground it puts the fighter in the *air* status, airborne,
     * falling at 7 -- and then writes Ground into stat_flags.ga, which
     * is what the hitbox tables read. Both, verbatim. */
    ftMarioSpecialLwSetStatus(mock_gobj);
    CHECK_STATUS(nFTMarioStatusSpecialAirLw);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(fp.stat_flags.ga == nMPKineticsGround);
    CHECK_EQF(fp.physics.vel_air.y, -7.0f);
    CHECK(fp.motion_id == nFTMarioMotionSpecialAirLw);
    CHECK(fp.motion_attack_id == nFTMotionAttackIDSpecialLw);
    /* ftmariospeciallw.c:136-142 ftMarioSpecialLwInitStatusVars */
    CHECK(fp.status_vars.mario.speciallw.dust_effect_int == 5);
    CHECK_EQF(fp.status_vars.mario.speciallw.friction, 0.0f);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK(fp.motion_vars.flags.flag2 == 0);

    /* and now through the cascade. Standing on the deck the whole air
     * half lasts less than a frame: ftMarioSpecialAirLwProcMap is
     * mpCommonProcFighterLanding, so the same frame that entered the
     * move lands it in the grounded half at the same animation frame. */
    spawn(0.0f, 0.0f);
    frame(0, -80, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialLw);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK(fp.coll_data.floor_line_id == LINE_DECK);
    CHECK(fp.motion_id == nFTMarioMotionSpecialLw);
    CHECK(fp.proc_physics == ftMarioSpecialLwProcPhysics);
    CHECK(fp.proc_map == ftMarioSpecialLwProcMap);

    /* ftmariospeciallw.c:46-60 ftMarioSpecialLwUpdateFriction: the drag
     * only runs once the motion script raises flag1, and then it grows
     * by 2 a frame and is subtracted from the clamp */
    CHECK_EQF(ftMarioSpecialLwUpdateFriction(&fp, 100.0f), 100.0f);
    fp.motion_vars.flags.flag1 = 1;
    CHECK_EQF(ftMarioSpecialLwUpdateFriction(&fp, 100.0f), 98.0f);
    CHECK_EQF(ftMarioSpecialLwUpdateFriction(&fp, 100.0f), 96.0f);
    CHECK_EQF(fp.status_vars.mario.speciallw.friction, -4.0f);
    /* and it never lets the clamp go negative */
    fp.status_vars.mario.speciallw.friction = -1000.0f;
    CHECK_EQF(ftMarioSpecialLwUpdateFriction(&fp, 100.0f), 0.0f);
    fp.motion_vars.flags.flag1 = 0;

    /* ftmariospeciallw.c:70-76: while the script holds flag3 up, another
     * B lifts the Tornado off the ground and back into the air half */
    fp.motion_vars.flags.flag3 = 1;
    fp.status_vars.mario.speciallw.friction = 0.0f;
    frame(0, 0, 0, 0, N64_B);           /* let go of the entry press */
    CHECK_STATUS(nFTMarioStatusSpecialLw);
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialAirLw);
    CHECK(fp.ga == nMPKineticsAir);
    /* ftMarioSpecialAirLwSetDisableRise clears flag3 on the way through,
     * so the lift is once per script cue */
    CHECK(fp.motion_vars.flags.flag3 == 0);

    /* the air half spends the rise once (ftmariospeciallw.c:34-42) */
    CHECK(!fp.passive_vars.mario.is_expend_tornado);
    fp.motion_vars.flags.flag2 = 1;
    idle_frames(1);
    CHECK(fp.passive_vars.mario.is_expend_tornado);
    CHECK(fp.motion_vars.flags.flag2 == 0);
}

/* The Down-B check's DIVERGES arm and the per-fighter table dispatch.
 * dFTCommonSpecialLwStatusList has a setter for five of the twenty-seven
 * FTKinds and NULL for the rest, where the game has all twenty-seven;
 * dFTMainSpecialStatusDescs has four tables and NULL for the rest, where
 * the game has twelve. Neither hole may do anything but nothing. */
extern FTSpecialColl dFoxMainMotion_LwReflectorFTSpecialColl;
extern void ftFoxReflectorBindOffsets(void);

/* Falcon Punch's flame, Falcon Kick's trail and Sing's notes
 * had makers and no packs, so each returned NULL and the fighter's own
 * update never raised is_effect_attach. Each is made here through the
 * game's own caller, hung off the joint the decomp names, and taken back
 * by the game's own stop. The mock has none of joints 16, 23 or TopN's
 * siblings, so TopN stands in for the length of each call. */
extern s32 sEFManagerStructsFreeNum;
static void ef_pool_reset(void);
extern void ftCaptainSpecialNUpdateEffect(GObj *fighter_gobj);
extern void ftCaptainSpecialLwUpdateEffect(GObj *fighter_gobj);

static void check_v18_effect(void (*proc_update)(GObj*), s32 joint_id)
{
    GObj *effect_gobj = gGCCommonLinks[nGCCommonLinkIDEffect];
    s32 found = 0;

    for (; effect_gobj != NULL; effect_gobj = effect_gobj->link_next)
    {
        EFStruct *ep = efGetStruct(effect_gobj);

        if ((ep != NULL) && (ep->fighter_gobj == mock_gobj))
        {
            found++;
            CHECK(ep->proc_update == proc_update);
            CHECK(DObjGetStruct(effect_gobj)->user_data.p == fp.joints[joint_id]);
        }
    }
    CHECK(found == 1);
    CHECK(fp.is_effect_attach == TRUE);
    CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM - 1);

    ftParamProcStopEffect(mock_gobj);
    CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
    fp.is_effect_attach = FALSE;
}

/* The game's scaling matrix kinds fold the joint's scale.x into
 * gGCScaleX (sys/objdisplay.c:565-638), and a billboard kind below reads
 * it back as its size, since it keeps nothing of the stack but the
 * position. So a kind-44 child of a joint scaled by 2 draws twice its own
 * scale: `view * m` must carry diag(2 sx, 2 sy, 2 sx). */
static float sTestFoldMtx[16];
static int sTestFoldSubmits;

static void test_fold_submit(DCDisplay *disp, const float *mtx)
{
    (void)disp;
    memcpy(sTestFoldMtx, mtx, sizeof(sTestFoldMtx));
    sTestFoldSubmits++;
}

static void test_billboard_scale_fold(void)
{
    static DCDisplay disp;
    GObj *gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);
    DObj *root, *child;
    float view[16], vm[16];
    int i, j, k;

    CHECK(gobj != NULL);
    if (gobj == NULL)
    {
        return;
    }
    memset(&disp, 0, sizeof(disp));
    disp.proc_submit = test_fold_submit;

    root = gcAddDObjForGObj(gobj, NULL);
    gcAddXObjForDObjFixed(root, nGCMatrixKindTraRotRpyRSca, 0);
    root->translate.vec.f.x = 10.0F;
    root->translate.vec.f.y = 20.0F;
    root->translate.vec.f.z = 30.0F;
    root->scale.vec.f.x = root->scale.vec.f.y = root->scale.vec.f.z = 2.0F;

    child = gcAddChildForDObj(root, NULL);
    gcAddXObjForDObjFixed(child, nGCMatrixKindRecalcRotRpyRSca, 0);
    child->scale.vec.f.x = 1.5F;
    child->scale.vec.f.y = 0.5F;
    child->scale.vec.f.z = 1.5F;
    child->dv = &disp;

    /* a real view: the walk's billboard reads the camera pass's */
    {
        float look[16], proj[16];

        gcMtxLookAt(look, 400.0F, 300.0F, 1800.0F, 100.0F, 200.0F, 0.0F, 0.0F, 1.0F, 0.0F);
        memset(proj, 0, sizeof(proj));
        proj[0] = proj[5] = proj[10] = proj[15] = 1.0F;
        gcSetCameraMatrixF(look, proj);
    }
    sTestFoldSubmits = 0;
    gcDrawDObjTreeForGObj(gobj);
    CHECK(sTestFoldSubmits == 1);
    CHECK_EQF(gGCScaleX, 1.0F);

    memcpy(view, gcGetViewF(), sizeof(view));
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            vm[i * 4 + j] = 0.0F;
            for (k = 0; k < 4; k++)
            {
                vm[i * 4 + j] += view[i * 4 + k] * sTestFoldMtx[k * 4 + j];
            }
        }
    }
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            float want = (i != j) ? 0.0F : (i == 1) ? 1.0F : 3.0F;

            CHECK_NEAR(vm[i * 4 + j], want, 1e-4F);
        }
    }
    CHECK_NEAR(sTestFoldMtx[3], 10.0F, 1e-4F);
    CHECK_NEAR(sTestFoldMtx[7], 20.0F, 1e-4F);
    CHECK_NEAR(sTestFoldMtx[11], 30.0F, 1e-4F);

    child->dv = NULL;
    gcEjectGObj(gobj);
}

static void test_v18_effect_makers(void)
{
    DObj *saved16, *saved23;

    ef_pool_reset();
    spawn(0.0f, 0.0f);
    saved16 = fp.joints[16];
    saved23 = fp.joints[23];
    fp.joints[16] = fp.joints[23] = fp.joints[nFTPartsJointTopN];

    fp.fkind = nFTKindCaptain;
    fp.is_effect_attach = FALSE;
    fp.motion_vars.flags.flag0 = 1;
    ftCaptainSpecialNUpdateEffect(mock_gobj);
    CHECK(fp.motion_vars.flags.flag0 == 0);
    check_v18_effect(efManagerNoEjectProcUpdate, 16);

    fp.motion_vars.flags.flag2 = 1;
    ftCaptainSpecialLwUpdateEffect(mock_gobj);
    CHECK(fp.motion_vars.flags.flag2 == 0);
    check_v18_effect(efManagerNoEjectProcUpdate, 23);

    fp.fkind = nFTKindPurin;
    CHECK(efManagerPurinSingMakeEffect(mock_gobj) != NULL);
    fp.is_effect_attach = TRUE;
    check_v18_effect(efManagerHaveStructProcUpdate, nFTPartsJointTopN);

    fp.joints[16] = saved16;
    fp.joints[23] = saved23;
    fp.fkind = nFTKindMario;
    spawn(0.0f, 0.0f);
}

static void test_downb_dispatch(void)
{
    /* Link's slot was the last NULL here; the decomp's own ftcommonspeciallw.c is compiled and
     * B-with-down pulls out his Bomb rather than falling through to the
     * crouch */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindLink;
    frame(0, -80, N64_B, N64_B, 0);
    CHECK_STATUS(nFTLinkStatusSpecialLw);
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindMario;

    /* the attribute gates it too: clear the bit and the same input is
     * the crouch again, for a fighter who does have the file */
    spawn(0.0f, 0.0f);
    fp.attr->is_have_speciallw = FALSE;
    frame(0, -80, N64_B, N64_B, 0);
    CHECK_STATUS(nFTCommonStatusSquat);
    fp.attr->is_have_speciallw = TRUE;

    /* the three other fighters' tables, by dispatch: set the kind and
     * ask for one of that kind's own special statuses. Each row is
     * ft<F>status.h's, so what this checks is that the id lands in the
     * right table -- the Mario model underneath plays the mock animation
     * either way. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindDonkey;
    ftMainSetStatus(mock_gobj, nFTDonkeyStatusSpecialLwLoop, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTDonkeyMotionSpecialLwLoop);
    CHECK(fp.proc_update == ftDonkeySpecialLwLoopProcUpdate);
    CHECK(fp.proc_interrupt == ftDonkeySpecialLwLoopProcInterrupt);

    fp.fkind = nFTKindPurin;
    ftMainSetStatus(mock_gobj, nFTPurinStatusSpecialAirLw, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTPurinMotionSpecialLw);
    CHECK(fp.proc_map == ftPurinSpecialAirLwProcMap);

    fp.fkind = nFTKindKirby;
    ftMainSetStatus(mock_gobj, nFTKirbyStatusSpecialLwHold, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTKirbyMotionSpecialLwHold);
    CHECK(fp.proc_update == ftKirbySpecialLwHoldProcUpdate);
    CHECK(fp.proc_physics == ftKirbySpecialLwHoldProcPhysics);

    /* the nametag kinds share their base fighter's table, as the game's
     * dFTMainSpecialStatusDescs does */
    fp.fkind = nFTKindNKirby;
    ftMainSetStatus(mock_gobj, nFTKirbyStatusSpecialLwEnd, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTKirbyMotionSpecialLwEnd);
    CHECK(fp.proc_map == mpCommonProcFighterWaitOrLanding);

    /* Mario's own table now carries the fireball. Before, the
     * SpecialN/SpecialAirN rows were holes and dispatch installed NULL
     * callbacks; now the id lands on ftmariospecialn.c's procs. The two
     * halves share ProcUpdate and differ in kinetics/friction/ProcMap. */
    fp.fkind = nFTKindMario;
    ftMainSetStatus(mock_gobj, nFTMarioStatusSpecialN, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTMarioMotionSpecialN);
    CHECK(fp.proc_update == ftMarioSpecialNProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == ftMarioSpecialNProcMap);

    ftMainSetStatus(mock_gobj, nFTMarioStatusSpecialAirN, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTMarioMotionSpecialAirN);
    CHECK(fp.proc_update == ftMarioSpecialNProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelDrift);
    CHECK(fp.proc_map == ftMarioSpecialAirNProcMap);

    /* the Up-B rows now go live too -- ftmariospecialhi.c was
     * promoted to host+target so its procs are named here. Both halves carry
     * all four callbacks (the fireball's ProcInterrupt was NULL) and differ
     * only in kinetics; ProcMap is ftMarioSpecialHiProcMap for both. */
    ftMainSetStatus(mock_gobj, nFTMarioStatusSpecialHi, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTMarioMotionSpecialHi);
    CHECK(fp.proc_update == ftMarioSpecialHiProcUpdate);
    CHECK(fp.proc_interrupt == ftMarioSpecialHiProcInterrupt);
    CHECK(fp.proc_physics == ftMarioSpecialHiProcPhysics);
    CHECK(fp.proc_map == ftMarioSpecialHiProcMap);

    ftMainSetStatus(mock_gobj, nFTMarioStatusSpecialAirHi, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTMarioMotionSpecialAirHi);
    CHECK(fp.proc_update == ftMarioSpecialHiProcUpdate);
    CHECK(fp.proc_interrupt == ftMarioSpecialHiProcInterrupt);
    CHECK(fp.proc_physics == ftMarioSpecialHiProcPhysics);
    CHECK(fp.proc_map == ftMarioSpecialHiProcMap);

    /* Purin's own table now carries Pound -- ftpurinspecialn.c's
     * nine procs are compiled unmodified, waiting only on
     * these two rows. Ground and air differ in ProcPhysics (TransN root
     * motion on the ground half, ftPurinSpecialAirNProcPhysics in the air)
     * and in ProcMap. */
    fp.fkind = nFTKindPurin;
    ftMainSetStatus(mock_gobj, nFTPurinStatusSpecialN, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTPurinMotionSpecialN);
    CHECK(fp.proc_update == ftAnimEndSetWait);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelTransN);
    CHECK(fp.proc_map == ftPurinSpecialNProcMap);

    ftMainSetStatus(mock_gobj, nFTPurinStatusSpecialAirN, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTPurinMotionSpecialAirN);
    CHECK(fp.proc_update == ftAnimEndSetFall);
    CHECK(fp.proc_physics == ftPurinSpecialAirNProcPhysics);
    CHECK(fp.proc_map == ftPurinSpecialAirNProcMap);

    /* Fox's own table now carries the Blaster -- both rows
     * share ftFoxSpecialNProcUpdate/ProcInterrupt, differing only in
     * kinetics/ProcPhysics/ProcMap the way Mario's fireball rows do. */
    fp.fkind = nFTKindFox;
    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialN, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialN);
    CHECK(fp.proc_update == ftFoxSpecialNProcUpdate);
    CHECK(fp.proc_interrupt == ftFoxSpecialNProcInterrupt);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == mpCommonSetFighterFallOnEdgeBreak);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialAirN, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialAirN);
    CHECK(fp.proc_update == ftFoxSpecialNProcUpdate);
    CHECK(fp.proc_interrupt == ftFoxSpecialNProcInterrupt);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelDrift);
    CHECK(fp.proc_map == mpCommonProcFighterWaitOrLanding);

    /* Purin's own table now carries Sing too -- her third and
     * last special-move file. Both rows share ftPurinSpecialHiProcUpdate
     * and differ only in kinetics/ProcPhysics/ProcMap, the same shape
     * Mario's fireball and Fox's Blaster rows have. */
    fp.fkind = nFTKindPurin;
    ftMainSetStatus(mock_gobj, nFTPurinStatusSpecialHi, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTPurinMotionSpecialHi);
    CHECK(fp.proc_update == ftPurinSpecialHiProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == ftPurinSpecialHiProcMap);

    ftMainSetStatus(mock_gobj, nFTPurinStatusSpecialAirHi, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTPurinMotionSpecialHi);
    CHECK(fp.proc_update == ftPurinSpecialHiProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelFriction);
    CHECK(fp.proc_map == ftPurinSpecialAirHiProcMap);

    /* Donkey's own table now carries Spinning Kong -- his
     * second special-move file after Down-B's. Unlike every SpecialHi row
     * above, both halves have their own ProcPhysics and ProcMap (no shared
     * ftPhysics or mpCommon leaf), since Spinning Kong's acceleration and
     * floor/cliff handling are Donkey's own, verbatim from ftdonkeyspecialhi.c. */
    fp.fkind = nFTKindDonkey;
    ftMainSetStatus(mock_gobj, nFTDonkeyStatusSpecialHi, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTDonkeyMotionSpecialHi);
    CHECK(fp.proc_update == ftDonkeySpecialHiProcUpdate);
    CHECK(fp.proc_physics == ftDonkeySpecialHiProcPhysics);
    CHECK(fp.proc_map == ftDonkeySpecialHiProcMap);

    ftMainSetStatus(mock_gobj, nFTDonkeyStatusSpecialAirHi, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTDonkeyMotionSpecialAirHi);
    CHECK(fp.proc_update == ftDonkeySpecialAirHiProcUpdate);
    CHECK(fp.proc_physics == ftDonkeySpecialAirHiProcPhysics);
    CHECK(fp.proc_map == ftDonkeySpecialAirHiProcMap);

    /* Samus's own table, her FIRST special-move table
     * (dFTMainSpecialStatusDescs[nFTKindSamus] was FT_SPECIAL_NONE
     * before it). Both halves share ftSamusSpecialHiProcUpdate AND
     * ftSamusSpecialHiProcMap -- unlike Donkey's pair, only ProcPhysics
     * differs between ground and air. */
    fp.fkind = nFTKindSamus;
    ftMainSetStatus(mock_gobj, nFTSamusStatusSpecialHi, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTSamusMotionSpecialHi);
    CHECK(fp.proc_update == ftSamusSpecialHiProcUpdate);
    CHECK(fp.proc_physics == ftSamusSpecialHiProcPhysics);
    CHECK(fp.proc_map == ftSamusSpecialHiProcMap);

    ftMainSetStatus(mock_gobj, nFTSamusStatusSpecialAirHi, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTSamusMotionSpecialAirHi);
    CHECK(fp.proc_update == ftSamusSpecialHiProcUpdate);
    CHECK(fp.proc_physics == ftSamusSpecialAirHiProcPhysics);
    CHECK(fp.proc_map == ftSamusSpecialHiProcMap);

    /* Captain's own table, his FIRST special-move table
     * (dFTMainSpecialStatusDescs[nFTKindCaptain] was FT_SPECIAL_NONE
     * before it). Both halves share ProcInterrupt (NULL) but not
     * ProcUpdate -- ftAnimEndSetWait on the ground, ftAnimEndSetFall in
     * the air, the same common leaves Purin's Pound rows share -- and
     * differ in kinetics/ProcPhysics/ProcMap. */
    fp.fkind = nFTKindCaptain;
    ftMainSetStatus(mock_gobj, nFTCaptainStatusSpecialN, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCaptainMotionSpecialN);
    CHECK(fp.proc_update == ftAnimEndSetWait);
    CHECK(fp.proc_physics == ftCaptainSpecialNProcPhysics);
    CHECK(fp.proc_map == ftCaptainSpecialNProcMap);

    ftMainSetStatus(mock_gobj, nFTCaptainStatusSpecialAirN, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCaptainMotionSpecialAirN);
    CHECK(fp.proc_update == ftAnimEndSetFall);
    CHECK(fp.proc_physics == ftCaptainSpecialAirNProcPhysics);
    CHECK(fp.proc_map == ftCaptainSpecialAirNProcMap);

    /* Samus's Down-B rows join her table (the Up-B's are already there) --
     * Bomb. Unlike every row above, "is projectile" is TRUE
     * here (the fighter itself is marked a projectile attacker while the
     * bomb is live), and both halves have their own ProcUpdate as well as
     * their own ProcPhysics/ProcMap -- no shared common leaf anywhere in
     * this pair, the same fully-separate shape Donkey's SpecialHi pair
     * has (unlike Samus's own SpecialHi rows, which share ProcUpdate). */
    fp.fkind = nFTKindSamus;
    ftMainSetStatus(mock_gobj, nFTSamusStatusSpecialLw, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTSamusMotionSpecialLw);
    CHECK(fp.proc_update == ftSamusSpecialLwProcUpdate);
    CHECK(fp.proc_physics == ftSamusSpecialLwProcPhysics);
    CHECK(fp.proc_map == ftSamusSpecialLwProcMap);

    ftMainSetStatus(mock_gobj, nFTSamusStatusSpecialAirLw, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTSamusMotionSpecialAirLw);
    CHECK(fp.proc_update == ftSamusSpecialAirLwProcUpdate);
    CHECK(fp.proc_physics == ftSamusSpecialAirLwProcPhysics);
    CHECK(fp.proc_map == ftSamusSpecialAirLwProcMap);

    /* Captain's Down-B rows, Falcon Kick -- five statuses, not
     * two. SpecialLw and SpecialLwAir share both ProcUpdate and ProcPhysics
     * (only ProcMap differs, the ground-vs-ran-off-an-edge split);
     * SpecialLwLanding, SpecialAirLw and SpecialLwBound each have their own
     * ProcPhysics/ProcMap pair, the same fully-separate shape Samus's Bomb
     * pair has. */
    fp.fkind = nFTKindCaptain;
    ftMainSetStatus(mock_gobj, nFTCaptainStatusSpecialLw, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCaptainMotionSpecialLw);
    CHECK(fp.proc_update == ftCaptainSpecialLwProcUpdate);
    CHECK(fp.proc_physics == ftCaptainSpecialLwProcPhysics);
    CHECK(fp.proc_map == ftCaptainSpecialLwProcMap);

    ftMainSetStatus(mock_gobj, nFTCaptainStatusSpecialLwAir, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCaptainMotionSpecialLwAir);
    CHECK(fp.proc_update == ftCaptainSpecialLwProcUpdate);
    CHECK(fp.proc_physics == ftCaptainSpecialLwProcPhysics);
    CHECK(fp.proc_map == ftCaptainSpecialLwAirProcMap);

    ftMainSetStatus(mock_gobj, nFTCaptainStatusSpecialLwLanding, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCaptainMotionSpecialLwLanding);
    CHECK(fp.proc_update == ftAnimEndSetWait);
    CHECK(fp.proc_physics == ftCaptainSpecialLwLandingProcPhysics);
    CHECK(fp.proc_map == mpCommonSetFighterFallOnEdgeBreak);

    ftMainSetStatus(mock_gobj, nFTCaptainStatusSpecialAirLw, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCaptainMotionSpecialAirLw);
    CHECK(fp.proc_update == ftAnimEndSetFall);
    CHECK(fp.proc_physics == ftCaptainSpecialAirLwProcPhysics);
    CHECK(fp.proc_map == ftCaptainSpecialAirLwProcMap);

    ftMainSetStatus(mock_gobj, nFTCaptainStatusSpecialLwBound, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCaptainMotionSpecialLwBound);
    CHECK(fp.proc_update == ftAnimEndSetFall);
    CHECK(fp.proc_physics == ftCaptainSpecialLwBoundProcPhysics);
    CHECK(fp.proc_map == mpCommonProcFighterWaitOrLanding);

    /* the Down-B demux (ftCommonSpecialLwCheckInterruptCommon) has been
     * wired into every ground cascade since before this table existed --
     * a real B-tap with the stick held down now reaches Samus's Bomb with
     * no direct call, the same shape Falcon Punch's Neutral-B reaches. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindSamus;
    fp.attr->is_have_speciallw = TRUE;
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, -80, N64_B, N64_B, 0);
    CHECK_STATUS(nFTSamusStatusSpecialLw);

    /* and Captain's own row, the same shape -- a real B+down from a
     * standing Captain reaches Falcon Kick with no direct call either. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindCaptain;
    fp.attr->is_have_speciallw = TRUE;
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, -80, N64_B, N64_B, 0);
    CHECK_STATUS(nFTCaptainStatusSpecialLw);
    CHECK(fp.proc_physics == ftCaptainSpecialLwProcPhysics);

    /* Yoshi's own table, his FIRST special-move table
     * (dFTMainSpecialStatusDescs[nFTKindYoshi] was FT_SPECIAL_NONE
     * before it) -- Yoshi Bomb, four rows. The two Start rows share every
     * proc column and differ only in kinetics; Landing has its own
     * ProcUpdate (the one that throws the stars) over two common leaves;
     * and the Loop row is the first status this port's tables carry with
     * no motion at all -- Script ID -1, which ftMainSetStatus leaves
     * alone (fp.motion_id reads back as -1, the anim untouched), and a
     * NULL ProcUpdate. */
    fp.fkind = nFTKindYoshi;
    ftMainSetStatus(mock_gobj, nFTYoshiStatusSpecialLwStart, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTYoshiMotionSpecialLwStart);
    CHECK(fp.proc_update == ftYoshiSpecialLwStartProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelTransNAll);
    CHECK(fp.proc_map == ftYoshiSpecialLwStartProcMap);

    ftMainSetStatus(mock_gobj, nFTYoshiStatusSpecialLwLanding, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTYoshiMotionSpecialLwLanding);
    CHECK(fp.proc_update == ftYoshiSpecialLwLandingProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == mpCommonSetFighterFallOnGroundBreak);

    ftMainSetStatus(mock_gobj, nFTYoshiStatusSpecialAirLwStart, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTYoshiMotionSpecialAirLwStart);
    CHECK(fp.proc_update == ftYoshiSpecialLwStartProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelTransNAll);
    CHECK(fp.proc_map == ftYoshiSpecialLwStartProcMap);

    ftMainSetStatus(mock_gobj, nFTYoshiStatusSpecialAirLwLoop,
                    mock_gobj->anim_frame, 0.0f, FTSTATUS_PRESERVE_HIT);
    CHECK(fp.motion_id == -1);
    CHECK(fp.proc_update == NULL);
    CHECK(fp.proc_physics == ftYoshiSpecialAirLwLoopProcPhysics);
    CHECK(fp.proc_map == ftYoshiSpecialAirLwLoopProcMap);

    /* and the setter itself: ftYoshiSpecialAirLwLoopSetStatus caps the
     * carried-over |vel_x| at 30, hands its own vel_y through the status
     * change, and then forces vel_y to AT LEAST 150 down -- a floor on the
     * plunge speed, not a ceiling: -200 stays -200, -40 becomes -150
     * (ftyoshispeciallw.c:130-149, FTYOSHI_YOSHIBOMB_VEL_Y_CLAMP's own
     * comment calls it the constant downward velocity) */
    fp.physics.vel_air.x = -45.0f;
    fp.physics.vel_air.y = -200.0f;
    ftYoshiSpecialAirLwLoopSetStatus(mock_gobj);
    CHECK_STATUS(nFTYoshiStatusSpecialAirLwLoop);
    CHECK(fabsf(fp.physics.vel_air.x - (-30.0f)) < 1e-5f);
    CHECK(fabsf(fp.physics.vel_air.y - (-200.0f)) < 1e-5f);
    fp.physics.vel_air.x = 12.0f;
    fp.physics.vel_air.y = -40.0f;
    ftYoshiSpecialAirLwLoopSetStatus(mock_gobj);
    CHECK(fabsf(fp.physics.vel_air.x - 12.0f) < 1e-5f);
    CHECK(fabsf(fp.physics.vel_air.y - (-150.0f)) < 1e-5f);

    /* and Yoshi's own demux row -- a real B+down from a standing Yoshi
     * reaches the hop with no direct call. ftYoshiSpecialLwStartSetStatus
     * takes him airborne on the spot (mpCommonSetFighterAir) and spends
     * every jump. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindYoshi;
    fp.attr->is_have_speciallw = TRUE;
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, -80, N64_B, N64_B, 0);
    CHECK_STATUS(nFTYoshiStatusSpecialLwStart);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(fp.jumps_used == fp.attr->jumps_max);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelTransNAll);

    /* Fox's Down-B rows, the Reflector -- ten statuses.
     * The ground five share ProcPhysics/ProcMap, the air five share
     * ftFoxSpecialAirLwCommonProcPhysics/mpCommonProcFighterWaitOrLanding;
     * Start/Hit/End/Turn each share ProcUpdate across ground and air, the
     * Turn rows play the Loop motion, and only the Loop pair carries a
     * ProcInterrupt. */
    fp.fkind = nFTKindFox;
    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialLwStart, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialLwStart);
    CHECK(fp.proc_update == ftFoxSpecialLwStartProcUpdate);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == mpCommonSetFighterFallOnEdgeBreak);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialLwHit, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialLwHit);
    CHECK(fp.proc_update == ftFoxSpecialLwHitProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialLwEnd, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialLwEnd);
    CHECK(fp.proc_update == ftFoxSpecialLwEndProcUpdate);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialLwLoop, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialLwLoop);
    CHECK(fp.proc_update == ftFoxSpecialLwLoopProcUpdate);
    CHECK(fp.proc_interrupt == ftFoxSpecialLwLoopProcInterrupt);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialLwTurn, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialLwLoop);
    CHECK(fp.proc_update == ftFoxSpecialLwTurnProcUpdate);
    CHECK(fp.proc_interrupt == NULL);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialAirLwStart, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialAirLwStart);
    CHECK(fp.proc_update == ftFoxSpecialLwStartProcUpdate);
    CHECK(fp.proc_physics == ftFoxSpecialAirLwCommonProcPhysics);
    CHECK(fp.proc_map == mpCommonProcFighterWaitOrLanding);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialAirLwHit, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialAirLwHit);
    CHECK(fp.proc_update == ftFoxSpecialLwHitProcUpdate);
    CHECK(fp.proc_physics == ftFoxSpecialAirLwCommonProcPhysics);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialAirLwEnd, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialAirLwEnd);
    CHECK(fp.proc_update == ftFoxSpecialLwEndProcUpdate);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialAirLwLoop, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialAirLwLoop);
    CHECK(fp.proc_update == ftFoxSpecialLwLoopProcUpdate);
    CHECK(fp.proc_interrupt == ftFoxSpecialLwLoopProcInterrupt);
    CHECK(fp.proc_map == mpCommonProcFighterWaitOrLanding);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialAirLwTurn, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialAirLwLoop);
    CHECK(fp.proc_update == ftFoxSpecialLwTurnProcUpdate);
    CHECK(fp.proc_physics == ftFoxSpecialAirLwCommonProcPhysics);

    /* the hurt sphere: bound out of the port's own copy of Fox's motion
     * file bytes (ftFoxReflectorBindOffsets, run at every overlay load on
     * target), so the unmodified lbRelocGetFileData-shaped read in
     * ftFoxSpecialLwStartInitStatusVars lands on it. A real B+down from a
     * standing Fox reaches Start with the sphere in fp->special_coll, the
     * release lag armed, the hexagon made and hung off
     * TopN, and the per-status vars zeroed. */
    ftFoxReflectorBindOffsets();
    ef_pool_reset();
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;
    fp.attr->is_have_speciallw = TRUE;
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, -80, N64_B, N64_B, 0);
    CHECK_STATUS(nFTFoxStatusSpecialLwStart);
    CHECK(fp.special_coll == &dFoxMainMotion_LwReflectorFTSpecialColl);
    CHECK(fp.special_coll->kind == nFTSpecialCollKindFoxReflector);
    CHECK(fp.special_coll->joint_id == 4);
    CHECK(fabsf(fp.special_coll->size.x - 350.0f) < 1e-5f);
    CHECK(fp.status_vars.fox.speciallw.release_lag == FTFOX_REFLECTOR_RELEASE_LAG);
    CHECK(fp.status_vars.fox.speciallw.is_release == FALSE);
    CHECK(fp.status_vars.fox.speciallw.gravity_delay == FTFOX_REFLECTOR_GRAVITY_DELAY);
    CHECK(fp.status_vars.fox.speciallw.effect_gobj != NULL);
    CHECK(fp.is_effect_attach == TRUE);
    CHECK(fp.motion_vars.flags.flag2 == 4);
    {
        GObj *effect_gobj = fp.status_vars.fox.speciallw.effect_gobj;
        EFStruct *ep = efGetStruct(effect_gobj);

        CHECK(ep->fighter_gobj == mock_gobj);
        CHECK(ep->proc_update == efManagerFoxReflectorProcUpdate);
        CHECK(DObjGetStruct(effect_gobj)->user_data.p == fp.joints[nFTPartsJointTopN]);
        CHECK(ep->effect_vars.reflector.index == 0);
        CHECK(ep->effect_vars.reflector.status == 4);

        /* a status written by the fighter's UpdateEffect is taken on the
         * next tic, by index into the pack's four animations, and spent */
        ep->effect_vars.reflector.status = 2;
        efManagerFoxReflectorProcUpdate(effect_gobj);
        CHECK(ep->effect_vars.reflector.index == 2);
        CHECK(ep->effect_vars.reflector.status == 4);
    }

    /* Start's anim end hands over to Loop, which raises is_reflect; with
     * B held the lag counts down and nothing else happens; on the tic B
     * is seen released after the lag is spent, Loop ends into End */
    mock_gobj->anim_frame = 0.0f;
    fp.input.pl.button_hold = fp.input.button_mask_b;
    ftFoxSpecialLwStartProcUpdate(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialLwLoop);
    CHECK(fp.is_reflect == TRUE);
    {
        int tic;

        for (tic = 0; tic < FTFOX_REFLECTOR_RELEASE_LAG; tic++)
        {
            ftFoxSpecialLwLoopProcUpdate(mock_gobj);
        }
    }
    CHECK_STATUS(nFTFoxStatusSpecialLwLoop);
    CHECK(fp.status_vars.fox.speciallw.release_lag == 0);
    CHECK(fp.status_vars.fox.speciallw.is_release == FALSE);
    fp.input.pl.button_hold = 0;
    ftFoxSpecialLwLoopProcUpdate(mock_gobj);
    CHECK(fp.status_vars.fox.speciallw.is_release == TRUE);
    CHECK_STATUS(nFTFoxStatusSpecialLwEnd);
    mock_gobj->anim_frame = 0.0f;
    ftFoxSpecialLwEndProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusWait);

    /* the turn: four tics of Turn, the facing flipped once turn_tics is
     * within FTFOX_REFLECTOR_TURN_FRAMES (the init tic itself), back to
     * Loop when they run out (the lag is still up) */
    ftFoxSpecialLwStartSetStatus(mock_gobj);
    ftFoxSpecialLwLoopSetStatus(mock_gobj);
    CHECK(fp.lr == +1);
    ftFoxSpecialLwTurnSetStatus(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialLwTurn);
    CHECK(fp.lr == -1);
    CHECK(fp.motion_vars.flags.flag1 == 1);
    CHECK(fp.status_vars.fox.speciallw.turn_tics == FTFOX_REFLECTOR_TURN_FRAMES - 1);
    ftFoxSpecialLwTurnProcUpdate(mock_gobj);
    ftFoxSpecialLwTurnProcUpdate(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialLwTurn);
    ftFoxSpecialLwTurnProcUpdate(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialLwLoop);
    CHECK(fp.lr == -1);

    /* the hit (ft/ftmain.c's reflect dispatch calls this on a reflected
     * projectile): Fox faces the way the reflection said, into Hit, the
     * reflect flag up again; Hit's anim end decides Loop (lag up) */
    fp.reflect_lr = +1;
    ftFoxSpecialLwHitSetStatus(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialLwHit);
    CHECK(fp.lr == +1);
    CHECK(fp.is_reflect == TRUE);
    mock_gobj->anim_frame = 0.0f;
    ftFoxSpecialLwHitProcUpdate(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialLwLoop);

    /* and the air version's setter kills the vertical speed and halves
     * the horizontal, with the same init */
    fp.physics.vel_air.x = 40.0f;
    fp.physics.vel_air.y = -30.0f;
    ftFoxSpecialAirLwStartSetStatus(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialAirLwStart);
    CHECK(fabsf(fp.physics.vel_air.x - 20.0f) < 1e-5f);
    CHECK(fabsf(fp.physics.vel_air.y) < 1e-5f);
    CHECK(fp.special_coll == &dFoxMainMotion_LwReflectorFTSpecialColl);

    /* a projectile that beats the reflector's resist
     * shatters it (ft/ftmain.c's reflect dispatch, reflect_damage set):
     * the shards at the sphere and Fox into ShieldBreakFly. The mock has
     * no joint 4, so TopN stands in for the length of the call. */
    {
        DObj *saved_joint = fp.joints[fp.special_coll->joint_id];

        if (saved_joint == NULL)
        {
            fp.joints[fp.special_coll->joint_id] = fp.joints[nFTPartsJointTopN];
        }
        fp.reflect_lr = -1;
        ftCommonShieldBreakFlyReflectorSetStatus(mock_gobj);
        CHECK_STATUS(nFTCommonStatusShieldBreakFly);
        fp.joints[fp.special_coll->joint_id] = saved_joint;
    }
    fp.special_coll = NULL;
    fp.is_reflect = FALSE;
    fp.fkind = nFTKindMario;                 /* as the next test expects */
    {
        /* every hexagon the setters above made is already gone: each
         * status change's ftParamProcStopEffect took it back */
        GObj *effect_gobj = gGCCommonLinks[nGCCommonLinkIDEffect];
        s32 ejected = 0;

        while (effect_gobj != NULL)
        {
            GObj *next = effect_gobj->link_next;
            EFStruct *ep = efGetStruct(effect_gobj);

            if ((ep != NULL) && (ep->proc_update == efManagerFoxReflectorProcUpdate))
            {
                efManagerSetPrevStructAlloc(ep);
                gcEjectGObj(effect_gobj);
                ejected++;
            }
            effect_gobj = next;
        }
        CHECK(ejected == 0);
        CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
    }
    spawn(0.0f, 0.0f);
}

/* the fighter's colour animations. The setter starts a
 * script by priority on fp->colanim; the electric one is per fighter
 * kind; the reset clears it and puts back what the fighter's state still
 * calls for; and a status change resets an interruptible one. The host
 * cannot run the scripts themselves (ftMainRunUpdateColAnim). */
extern GMColDesc dGMColScriptsDescs[];
extern s32 dFTParamSkeletonColAnimIDs[];
extern sb32 ftParamCheckSetSkeletonColAnimID(GObj *fighter_gobj, s32 damage_level);
extern void ftParamResetFighterColAnim(GObj *fighter_gobj);
extern void ftParamResetStatUpdateColAnim(GObj *fighter_gobj);

static void test_fighter_colanim(void)
{
    s32 id;

    spawn(0.0f, 0.0f);
    CHECK(fp.colanim.colanim_id == 0);

    CHECK(ftParamCheckSetFighterColAnimID(mock_gobj, nGMColAnimFighterDamageCommon, 0) == TRUE);
    CHECK(fp.colanim.colanim_id == nGMColAnimFighterDamageCommon);
    CHECK(fp.colanim.cs[0].p_script == dGMColScriptsDescs[nGMColAnimFighterDamageCommon].p_script);

    ftParamResetFighterColAnim(mock_gobj);
    CHECK(fp.colanim.colanim_id == 0);
    CHECK(fp.colanim.cs[0].p_script == NULL);

    /* the electric hit: Samus's own id, one per damage level */
    fp.fkind = nFTKindSamus;
    CHECK(ftParamCheckSetSkeletonColAnimID(mock_gobj, 1) == TRUE);
    CHECK(fp.colanim.colanim_id == dFTParamSkeletonColAnimIDs[nFTKindSamus] + 1);
    CHECK(dFTParamSkeletonColAnimIDs[nFTKindSamus] == 0x1C);
    fp.fkind = nFTKindMario;

    /* the reset puts back a standing one: the star */
    fp.star_invincible_tics = 100;
    ftParamResetStatUpdateColAnim(mock_gobj);
    CHECK(fp.colanim.colanim_id == nGMColAnimFighterStar);
    fp.star_invincible_tics = 0;
    ftParamResetStatUpdateColAnim(mock_gobj);
    CHECK(fp.colanim.colanim_id == 0);

    /* and DK's full charge */
    fp.fkind = nFTKindDonkey;
    fp.passive_vars.donkey.charge_level = FTDONKEY_GIANTPUNCH_CHARGE_MAX;
    ftParamResetStatUpdateColAnim(mock_gobj);
    CHECK(fp.colanim.colanim_id == nGMColAnimFighterCommonSpecialNCharge);
    fp.passive_vars.donkey.charge_level = 0;
    fp.fkind = nFTKindMario;
    ftParamResetFighterColAnim(mock_gobj);

    /* a status change resets an interruptible colanim and keeps one that
     * is not */
    for (id = 1; (id < nGMColAnimFighterStar) && !dGMColScriptsDescs[id].is_unlocked; id++)
    {
    }
    CHECK(id < nGMColAnimFighterStar);
    if (id < nGMColAnimFighterStar)
    {
        ftParamCheckSetFighterColAnimID(mock_gobj, id, 0);
        ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
        CHECK(fp.colanim.colanim_id == 0);
        ftParamCheckSetFighterColAnimID(mock_gobj, id, 0);
        ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_COLANIM);
        CHECK(fp.colanim.colanim_id == id);
    }
    ftParamResetFighterColAnim(mock_gobj);
    spawn(0.0f, 0.0f);
}

/* landing and walking timing. On the one frame a
 * landing opens to interrupts, a stick held down goes straight to
 * SquatWait; a frame later it crouches through Squat. A walk setter plays
 * the new walk's first frame of events at once, and a slow or middle walk
 * may be interrupted by a special on the frame it starts. */
extern void ftCommonLandingSetStatusParam(GObj *fighter_gobj, s32 status_id, sb32 is_allow_interrupt, f32 anim_speed);

static void test_landing_walk_timing(void)
{
    f32 frame_before;

    /* the landing's first open frame: straight to SquatWait */
    spawn(0.0f, 0.0f);
    ftCommonLandingSetStatusParam(mock_gobj, nFTCommonStatusLandingLight, TRUE, 1.0F);
    CHECK_STATUS(nFTCommonStatusLandingLight);
    mock_gobj->anim_frame = FTCOMMON_LANDING_INTERRUPT_BEGIN;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = -80;
    fp.input.pl.button_tap = fp.input.pl.button_hold = 0;
    fp.tap_stick_y = fp.hold_stick_y = FTINPUT_STICKBUFFER_TICS_MAX;
    ftCommonLandingProcInterrupt(mock_gobj);
    CHECK_STATUS(nFTCommonStatusSquatWait);

    /* any later frame: the crouch */
    spawn(0.0f, 0.0f);
    ftCommonLandingSetStatusParam(mock_gobj, nFTCommonStatusLandingLight, TRUE, 1.0F);
    mock_gobj->anim_frame = FTCOMMON_LANDING_INTERRUPT_BEGIN + 2.0F;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = -80;
    fp.input.pl.button_tap = fp.input.pl.button_hold = 0;
    fp.tap_stick_y = fp.hold_stick_y = FTINPUT_STICKBUFFER_TICS_MAX;
    ftCommonLandingProcInterrupt(mock_gobj);
    CHECK_STATUS(nFTCommonStatusSquat);

    /* a slow walk: its first frame played, specials open */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.input.pl.stick_range.x = 10;
    fp.input.pl.stick_range.y = 0;
    ftCommonWalkSetStatusParam(mock_gobj, 3.0F);
    CHECK_STATUS(nFTCommonStatusWalkSlow);
    CHECK(fp.is_special_interrupt == TRUE);
    frame_before = mock_gobj->anim_frame;
    CHECK(frame_before != 3.0F);

    /* a fast walk: played, but not open */
    spawn(0.0f, 0.0f);
    fp.input.pl.stick_range.x = 80;
    ftCommonWalkSetStatusParam(mock_gobj, 0.0F);
    CHECK_STATUS(nFTCommonStatusWalkFast);
    CHECK(fp.is_special_interrupt == FALSE);
    spawn(0.0f, 0.0f);
}

/* the forward smash's Pikachu and Ness arms. Ness's
 * setter binds the bat's reflector out of his MainMotion image and the
 * update holds is_reflect to flag1. Pikachu's setter clears the flags and
 * installs the effect pause/resume hooks, and each flag makes a Thunder
 * Shock (efManagerPikachuThunderShockMakeEffect) at his tail, stepping
 * gfx_id by 1 or 2 mod 3, with the fighter marked as carrying an effect.
 * The mock has no joint 11, so TopN stands in. */
extern void ftCommonAttackS4SetStatus(GObj *fighter_gobj);
extern void ftCommonAttackS4ProcUpdate(GObj *fighter_gobj);
extern void *gFTNessFileMainMotion;
extern EFStruct *sEFManagerStructsAllocFree;
extern s32 sEFManagerStructsFreeNum;
static void ef_pool_reset(void);

static void test_attacks4_pikachu_ness(void)
{
    DObj *saved_joint;
    GObj *before[32];
    GObj *effect_gobj;
    s32 nbefore = 0;
    s32 made = 0;
    s32 prev_gfx;
    s32 step;
    s32 k;

    /* ---- Ness: the bat reflects while flag1 is up ---- */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindNess;
    fp.motion_vars.flags.flag1 = 1;
    fp.special_coll = NULL;
    ftCommonAttackS4SetStatus(mock_gobj);
    CHECK((void *)fp.special_coll == (void *)((u8 *)gFTNessFileMainMotion + 0x1114));
    if (fp.special_coll != NULL)
    {
        CHECK(fp.special_coll->kind == 2);
        CHECK_EQF(fp.special_coll->size.x, 300.0F);
    }
    fp.motion_vars.flags.flag1 = 1;
    fp.is_reflect = FALSE;
    ftCommonAttackS4ProcUpdate(mock_gobj);
    CHECK(fp.is_reflect == TRUE);
    fp.motion_vars.flags.flag1 = 0;
    ftCommonAttackS4ProcUpdate(mock_gobj);
    CHECK(fp.is_reflect == FALSE);
    fp.special_coll = NULL;

    /* ---- Pikachu: a spark on each flag ---- */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindPikachu;
    fp.motion_vars.flags.flag1 = 1;
    fp.motion_vars.flags.flag2 = 1;
    ftCommonAttackS4SetStatus(mock_gobj);
    CHECK(fp.motion_vars.flags.flag1 == 0 && fp.motion_vars.flags.flag2 == 0);
    CHECK(fp.status_vars.common.attack4.gfx_id == 0);
    CHECK(fp.proc_lagstart == ftParamProcPauseEffect);
    CHECK(fp.proc_lagend == ftParamProcResumeEffect);

    /* the effect pool is test_effect_manager's; seed it for these four
     * and remember which effect GObjs were already there */
    ef_pool_reset();
    for (effect_gobj = gGCCommonLinks[nGCCommonLinkIDEffect];
         (effect_gobj != NULL) && (nbefore < ARRAY_COUNT(before));
         effect_gobj = effect_gobj->link_next)
    {
        before[nbefore++] = effect_gobj;
    }
    saved_joint = fp.joints[11];
    if (saved_joint == NULL)
    {
        fp.joints[11] = fp.joints[nFTPartsJointTopN];
    }
    for (step = 0; step < 4; step++)
    {
        prev_gfx = fp.status_vars.common.attack4.gfx_id;
        fp.is_effect_attach = FALSE;
        if (step & 1)
        {
            fp.motion_vars.flags.flag2 = 1;
        }
        else fp.motion_vars.flags.flag1 = 1;

        ftCommonAttackS4ProcUpdate(mock_gobj);
        CHECK(fp.motion_vars.flags.flag1 == 0 && fp.motion_vars.flags.flag2 == 0);
        CHECK(fp.status_vars.common.attack4.gfx_id >= 0 &&
              fp.status_vars.common.attack4.gfx_id < 3 &&
              fp.status_vars.common.attack4.gfx_id != prev_gfx);
        CHECK(fp.is_effect_attach == TRUE);
    }
    fp.joints[11] = saved_joint;

    /* four sparks, each on the Thunder Shock's desc and hung off TopN */
    effect_gobj = gGCCommonLinks[nGCCommonLinkIDEffect];
    while (effect_gobj != NULL)
    {
        GObj *next = effect_gobj->link_next;

        for (k = 0; (k < nbefore) && (before[k] != effect_gobj); k++)
        {
        }
        if (k == nbefore)
        {
            EFStruct *ep = efGetStruct(effect_gobj);

            made++;
            CHECK((ep != NULL) && (ep->fighter_gobj == mock_gobj));
            CHECK(DObjGetStruct(effect_gobj)->user_data.p == fp.joints[nFTPartsJointTopN]);
            gcEjectGObj(effect_gobj);
        }
        effect_gobj = next;
    }
    CHECK(made == 4);
    sEFManagerStructsAllocFree = NULL;
    sEFManagerStructsFreeNum = 0;
    fp.proc_lagstart = fp.proc_lagend = NULL;

    fp.fkind = nFTKindMario;                 /* as the next test expects */
    spawn(0.0f, 0.0f);
}

/* the Neutral-B command demux, ftcommonspecialn.c's
 * ftCommonSpecialNCheckInterruptCommon + dFTCommonSpecialNStatusList.
 * Driven directly (the port's Wait ProcInterrupt does not yet route the
 * neutral-B check into its cascade). B tapped
 * with the stick vertically neutral, on a fighter that has a Neutral-B,
 * must reach the fighter's SpecialN setter; for Mario that is the
 * fireball. */
static void test_specialn_demux(void)
{
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindMario;
    fp.attr->is_have_specialn = TRUE;

    /* B tap, stick neutral -> the interrupt fires and the id lands on
     * Mario's SpecialN row */
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = 0;
    CHECK(ftCommonSpecialNCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTMarioStatusSpecialN);
    CHECK(fp.motion_id == nFTMarioMotionSpecialN);
    CHECK(fp.proc_update == ftMarioSpecialNProcUpdate);

    /* no B tap -> no interrupt */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindMario;
    fp.attr->is_have_specialn = TRUE;
    fp.input.pl.button_tap = 0;
    fp.input.pl.stick_range.y = 0;
    CHECK(ftCommonSpecialNCheckInterruptCommon(mock_gobj) == FALSE);

    /* stick at the Up-B threshold (y == 40) is out of the neutral band,
     * so this is not a Neutral-B */
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialNCheckInterruptCommon(mock_gobj) == FALSE);

    /* and the Down-B threshold (y == -40) the same way */
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALLW_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialNCheckInterruptCommon(mock_gobj) == FALSE);

    /* the attribute gates it: a fighter with no Neutral-B does nothing */
    fp.input.pl.stick_range.y = 0;
    fp.attr->is_have_specialn = FALSE;
    CHECK(ftCommonSpecialNCheckInterruptCommon(mock_gobj) == FALSE);
    fp.attr->is_have_specialn = TRUE;

    /* Ness was the last NULL slot here until his B-press is
     * PK Fire now. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindNess;
    fp.attr->is_have_specialn = TRUE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.y = 0;
    CHECK(ftCommonSpecialNCheckInterruptCommon(mock_gobj) != FALSE);
    CHECK_STATUS(nFTNessStatusSpecialN);
    spawn(0.0f, 0.0f);

    /* the demux is now wired into the ground cascades, so a
     * real neutral B-tap reaches it with no direct call. From a standing
     * Wait it goes through ftCommonWaitProcInterrupt. */
    spawn(0.0f, 0.0f);
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialN);

    /* and from a Walk it goes through ftCommonWalkProcInterrupt: the same
     * leading arm sits in every ground cascade, not just Wait. */
    spawn(0.0f, 0.0f);
    frame(20, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWalkSlow);
    frame(20, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialN);

    /* Purin's row is live now too, and the demux dispatches
     * on fkind alone -- no wiring of its own was needed, since
     * ftCommonSpecialNCheckInterruptCommon already loops on fp->fkind and
     * is already wired into every ground cascade. A real
     * B-tap from a standing Purin reaches Pound with no direct call. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindPurin;
    fp.attr->is_have_specialn = TRUE;
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTPurinStatusSpecialN);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelTransN);

    /* Fox's own row, the same shape -- a real B-tap from a
     * standing Fox reaches the Blaster with no direct call either. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;
    fp.attr->is_have_specialn = TRUE;
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTFoxStatusSpecialN);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);

    /* Donkey's Giant Punch, his third and last special file
     * -- eight rows. The Start and Loop pairs share ProcInterrupt (the
     * B/A release read; Loop's also reads Z for the cancel and the
     * grounded roll-out), the End/Full pairs share every proc column and
     * are told apart by charge level in the setter, and all eight
     * motions are FTANIM_FLAG_NONE, so no mock-motion entry. */
    fp.fkind = nFTKindDonkey;
    ftMainSetStatus(mock_gobj, nFTDonkeyStatusSpecialNStart, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTDonkeyMotionSpecialNStart);
    CHECK(fp.proc_update == ftDonkeySpecialNStartProcUpdate);
    CHECK(fp.proc_interrupt == ftDonkeySpecialNStartProcInterrupt);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == ftDonkeySpecialNStartProcMap);

    ftMainSetStatus(mock_gobj, nFTDonkeyStatusSpecialAirNStart, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTDonkeyMotionSpecialAirNStart);
    CHECK(fp.proc_update == ftDonkeySpecialAirNStartProcUpdate);
    CHECK(fp.proc_interrupt == ftDonkeySpecialNStartProcInterrupt);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelFriction);
    CHECK(fp.proc_map == ftDonkeySpecialAirNStartProcMap);

    ftMainSetStatus(mock_gobj, nFTDonkeyStatusSpecialNLoop, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTDonkeyMotionSpecialNLoop);
    CHECK(fp.proc_update == ftDonkeySpecialNLoopProcUpdate);
    CHECK(fp.proc_interrupt == ftDonkeySpecialNLoopProcInterrupt);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == ftDonkeySpecialNLoopProcMap);

    ftMainSetStatus(mock_gobj, nFTDonkeyStatusSpecialAirNLoop, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTDonkeyMotionSpecialAirNLoop);
    CHECK(fp.proc_update == ftDonkeySpecialNLoopProcUpdate);
    CHECK(fp.proc_interrupt == ftDonkeySpecialNLoopProcInterrupt);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelFriction);
    CHECK(fp.proc_map == ftDonkeySpecialAirNLoopProcMap);

    ftMainSetStatus(mock_gobj, nFTDonkeyStatusSpecialNEnd, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTDonkeyMotionSpecialNEnd);
    CHECK(fp.proc_update == ftDonkeySpecialNEndProcUpdate);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == mpCommonSetFighterFallOnEdgeBreak);

    ftMainSetStatus(mock_gobj, nFTDonkeyStatusSpecialAirNEnd, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTDonkeyMotionSpecialAirNEnd);
    CHECK(fp.proc_update == ftDonkeySpecialNEndProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelFriction);
    CHECK(fp.proc_map == ftDonkeySpecialAirNEndProcMap);

    ftMainSetStatus(mock_gobj, nFTDonkeyStatusSpecialNFull, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTDonkeyMotionSpecialNFull);
    CHECK(fp.proc_update == ftDonkeySpecialNEndProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == mpCommonSetFighterFallOnEdgeBreak);

    ftMainSetStatus(mock_gobj, nFTDonkeyStatusSpecialAirNFull, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTDonkeyMotionSpecialAirNFull);
    CHECK(fp.proc_update == ftDonkeySpecialNEndProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelFriction);
    CHECK(fp.proc_map == ftDonkeySpecialAirNEndProcMap);

    /* the charge itself, ftdonkeyspecialn.c:88-138: a real B-tap from a
     * standing Donkey reaches Start with the charge at zero and the
     * damage hook set; Start's anim end hands over to Loop; each Loop
     * tic at the head of the animation (anim_frame in [0, anim_speed))
     * first arms is_charging, then counts one level a tic up to
     * FTDONKEY_GIANTPUNCH_CHARGE_MAX, where the anim speed doubles and
     * is_cancel goes up (with the full-charge colanim), and the
     * tic after that cancels out to Wait. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindDonkey;
    fp.attr->is_have_specialn = TRUE;
    fp.passive_vars.donkey.charge_level = 0;
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTDonkeyStatusSpecialNStart);
    CHECK(fp.proc_damage == ftDonkeySpecialNProcDamage);
    CHECK(fp.status_vars.donkey.specialn.is_release == FALSE);
    CHECK(fp.status_vars.donkey.specialn.is_charging == FALSE);
    CHECK(fp.status_vars.donkey.specialn.is_cancel == FALSE);

    ftDonkeySpecialNLoopSetStatus(mock_gobj);
    CHECK_STATUS(nFTDonkeyStatusSpecialNLoop);
    mock_gobj->anim_frame = 0.0f;
    DObjGetStruct(mock_gobj)->anim_speed = 1.0f;
    ftDonkeySpecialNLoopProcUpdate(mock_gobj);          /* arms the charge */
    CHECK(fp.status_vars.donkey.specialn.is_charging == TRUE);
    CHECK(fp.passive_vars.donkey.charge_level == 0);
    {
        int tic;

        for (tic = 0; tic < FTDONKEY_GIANTPUNCH_CHARGE_MAX - 1; tic++)
        {
            ftDonkeySpecialNLoopProcUpdate(mock_gobj);
        }
    }
    CHECK(fp.passive_vars.donkey.charge_level == FTDONKEY_GIANTPUNCH_CHARGE_MAX - 1);
    CHECK(fp.status_vars.donkey.specialn.is_cancel == FALSE);
    CHECK_STATUS(nFTDonkeyStatusSpecialNLoop);
    /* the last level: the same tic that reaches CHARGE_MAX doubles the
     * anim speed, raises is_cancel, and -- the if-chain runs on in the
     * same call -- cancels out to Wait at once, whose own status change
     * puts the anim speed back. The charge is kept for the next press. */
    ftDonkeySpecialNLoopProcUpdate(mock_gobj);
    CHECK(fp.passive_vars.donkey.charge_level == FTDONKEY_GIANTPUNCH_CHARGE_MAX);
    CHECK(fp.status_vars.donkey.specialn.is_cancel == TRUE);
    CHECK_STATUS(nFTCommonStatusWait);

    /* a fully charged Donkey's next B-tap releases at once (is_release
     * starts TRUE at full charge) and the End setter picks Full, sets the
     * ground velocity from the charge, moves the charge into the status
     * vars and zeroes the passive one */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindDonkey;
    fp.attr->is_have_specialn = TRUE;
    fp.passive_vars.donkey.charge_level = FTDONKEY_GIANTPUNCH_CHARGE_MAX;
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTDonkeyStatusSpecialNStart);
    CHECK(fp.status_vars.donkey.specialn.is_release == TRUE);
    ftDonkeySpecialNEndSetStatus(mock_gobj);
    CHECK_STATUS(nFTDonkeyStatusSpecialNFull);
    CHECK(fabsf(fp.physics.vel_ground.x -
                FTDONKEY_GIANTPUNCH_CHARGE_MAX * FTDONKEY_GIANTPUNCH_VEL_MUL) < 1e-4f);
    CHECK(fp.status_vars.donkey.specialn.charge_level == FTDONKEY_GIANTPUNCH_CHARGE_MAX);
    CHECK(fp.passive_vars.donkey.charge_level == 0);

    /* a half charge takes End, not Full */
    fp.passive_vars.donkey.charge_level = 4;
    ftDonkeySpecialNEndSetStatus(mock_gobj);
    CHECK_STATUS(nFTDonkeyStatusSpecialNEnd);
    CHECK(fabsf(fp.physics.vel_ground.x - 4 * FTDONKEY_GIANTPUNCH_VEL_MUL) < 1e-4f);
    CHECK(fp.status_vars.donkey.specialn.charge_level == 4);

    /* and in End (not Full, whose hitboxes carry their own numbers)
     * ftDonkeySpecialNEndProcUpdate adds charge*2 to every hitbox that
     * has just come out (attack_state New) and leaves the others alone */
    mock_gobj->anim_frame = 5.0f;
    fp.attack_colls[0].attack_state = nGMAttackStateNew;
    fp.attack_colls[0].damage = 10;
    fp.attack_colls[1].attack_state = nGMAttackStateInterpolate;
    fp.attack_colls[1].damage = 10;
    ftDonkeySpecialNEndProcUpdate(mock_gobj);
    CHECK(fp.attack_colls[0].damage == 10 + 4 * FTDONKEY_GIANTPUNCH_CHARGE_DAMAGE_MUL);
    CHECK(fp.attack_colls[1].damage == 10);

    /* a hit resets the stored charge through the damage hook */
    fp.passive_vars.donkey.charge_level = 7;
    ftDonkeySpecialNProcDamage(mock_gobj);
    CHECK(fp.passive_vars.donkey.charge_level == 0);

    /* Samus's Charge Shot, her third and last special file
     * -- five rows, every one "is projectile" like Bomb's. Start and End
     * each share ProcUpdate across ground and air; only the ground Start
     * and the Loop carry a ProcInterrupt. */
    fp.fkind = nFTKindSamus;
    ftMainSetStatus(mock_gobj, nFTSamusStatusSpecialNStart, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTSamusMotionSpecialNStart);
    CHECK(fp.proc_update == ftSamusSpecialNStartProcUpdate);
    CHECK(fp.proc_interrupt == ftSamusSpecialNStartProcInterrupt);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == ftSamusSpecialNStartProcMap);

    ftMainSetStatus(mock_gobj, nFTSamusStatusSpecialNLoop, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTSamusMotionSpecialNLoop);
    CHECK(fp.proc_update == ftSamusSpecialNLoopProcUpdate);
    CHECK(fp.proc_interrupt == ftSamusSpecialNLoopProcInterrupt);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == ftSamusSpecialNLoopProcMap);

    ftMainSetStatus(mock_gobj, nFTSamusStatusSpecialNEnd, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTSamusMotionSpecialNEnd);
    CHECK(fp.proc_update == ftSamusSpecialNEndProcUpdate);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == ftSamusSpecialNEndProcMap);

    ftMainSetStatus(mock_gobj, nFTSamusStatusSpecialAirNStart, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTSamusMotionSpecialAirNStart);
    CHECK(fp.proc_update == ftSamusSpecialNStartProcUpdate);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelDrift);
    CHECK(fp.proc_map == ftSamusSpecialAirNStartProcMap);

    ftMainSetStatus(mock_gobj, nFTSamusStatusSpecialAirNEnd, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTSamusMotionSpecialAirNEnd);
    CHECK(fp.proc_update == ftSamusSpecialNEndProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelFriction);
    CHECK(fp.proc_map == ftSamusSpecialAirNEndProcMap);

    /* a real B-tap from a standing Samus reaches Start with the anim
     * speed scaled by the stored charge (ftSamusSpecialNStartGetAnimSpeed:
     * 1.0 at zero charge, 0.84 at full), the damage hook set, no held
     * shot yet, and is_release FALSE unless the charge is already full */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindSamus;
    fp.attr->is_have_specialn = TRUE;
    fp.passive_vars.samus.charge_level = 0;
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTSamusStatusSpecialNStart);
    CHECK(fp.proc_damage == ftSamusSpecialNProcDamage);
    CHECK(fp.status_vars.samus.specialn.charge_gobj == NULL);
    CHECK(fp.status_vars.samus.specialn.is_release == FALSE);
    CHECK(fabsf(DObjGetStruct(mock_gobj)->anim_speed - 1.0f) < 1e-5f);

    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindSamus;
    fp.attr->is_have_specialn = TRUE;
    fp.passive_vars.samus.charge_level = FTSAMUS_CHARGE_MAX;
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTSamusStatusSpecialNStart);
    CHECK(fp.status_vars.samus.specialn.is_release == TRUE);
    CHECK(fabsf(DObjGetStruct(mock_gobj)->anim_speed - 0.84f) < 1e-4f);
    CHECK(fabsf(ftSamusSpecialNStartGetAnimSpeed(&fp) - 0.84f) < 1e-4f);

    /* the damage hook zeroes the charge and drops any held shot */
    fp.passive_vars.samus.charge_level = 3;
    ftSamusSpecialNProcDamage(mock_gobj);
    CHECK(fp.passive_vars.samus.charge_level == 0);
    fp.fkind = nFTKindMario;

    /* Captain's own row, his first live slot in this table --
     * a real B-tap from a standing Captain reaches Falcon Punch with no
     * direct call, same as Purin's and Fox's above. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindCaptain;
    fp.attr->is_have_specialn = TRUE;
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTCaptainStatusSpecialN);
    CHECK(fp.proc_physics == ftCaptainSpecialNProcPhysics);

    /* ---- Donkey Kong's CARGO THROW, statuses 235-245 ----
     *
     * His forward throw does not throw: it picks the caught fighter up
     * and carries him, and these eleven statuses are the whole of what
     * he can do while carrying -- stand, three walking speeds, turn,
     * jump, fall, land, take a hit, and the two that finally throw. The
     * array had been sized through SpecialLwEnd and
     * grew for the first time here.
     *
     * Eight decomp files compile unmodified, so what this checks is the
     * eleven rows and the one entry the game uses to reach them:
     * ftDonkeyThrowFWaitSetStatus, which ft/ftcommon/ftcommonthrow.c
     * calls at the end of a DK forward throw and which was a stub in
     * src/dc/ftcommon.c. ---- */
    {
        extern FTStatusDesc dFTDonkeySpecialStatusDescs[];
        extern void ftDonkeyThrowFWaitSetStatus(GObj *fighter_gobj);
        FTStatusDesc *r;
        int i;

        /* every one of the eleven is filled, carries the ThrowF attack
         * id, and has a physics and a map proc */
        for (i = nFTDonkeyStatusThrowFWait; i <= nFTDonkeyStatusThrowAirFF; i++)
        {
            r = &dFTDonkeySpecialStatusDescs[i - nFTCommonStatusSpecialStart];
            CHECK(r->sflags.attack_id == nFTStatusAttackIDThrowF);
            CHECK(r->proc_physics != NULL);
            CHECK(r->proc_map != NULL);
            /* the game marks every one of them Ground, including the two
             * that are plainly airborne -- see the note in ftcommon.c */
            CHECK(r->sflags.ga == nMPKineticsGround);
        }
        /* the three walking speeds share every proc column: what differs
         * between them is the motion, which is the whole point of having
         * three */
        for (i = nFTDonkeyStatusThrowFWalkSlow; i <= nFTDonkeyStatusThrowFWalkFast; i++)
        {
            r = &dFTDonkeySpecialStatusDescs[i - nFTCommonStatusSpecialStart];
            CHECK(r->proc_update == NULL);
            CHECK(r->proc_interrupt == ftDonkeyThrowFWalkProcInterrupt);
            CHECK(r->proc_physics == ftCommonWalkProcPhysics);
            CHECK(r->proc_map == ftDonkeyThrowFCommonProcMap);
        }
        CHECK(dFTDonkeySpecialStatusDescs[nFTDonkeyStatusThrowFWalkSlow - nFTCommonStatusSpecialStart].mflags.motion_id
              != dFTDonkeySpecialStatusDescs[nFTDonkeyStatusThrowFWalkFast - nFTCommonStatusSpecialStart].mflags.motion_id);
        /* the two airborne ones say so in their physics if not in their
         * ga, and they are the only two that do */
        r = &dFTDonkeySpecialStatusDescs[nFTDonkeyStatusThrowFFall - nFTCommonStatusSpecialStart];
        CHECK(r->proc_physics == ftPhysicsApplyAirVelDriftFastFall);
        r = &dFTDonkeySpecialStatusDescs[nFTDonkeyStatusThrowAirFF - nFTCommonStatusSpecialStart];
        CHECK(r->proc_physics == ftPhysicsApplyAirVelDrift);
        CHECK(r->proc_map == ftDonkeyThrowAirFFProcMap);

        /* and the way in, which the throw takes when its animation ends */
        spawn(0.0f, 0.0f);
        fp.fkind = nFTKindDonkey;
        ftDonkeyThrowFWaitSetStatus(mock_gobj);
        CHECK_STATUS(nFTDonkeyStatusThrowFWait);
        CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
        CHECK(fp.proc_map == ftDonkeyThrowFCommonProcMap);
        fp.fkind = nFTKindMario;
        spawn(0.0f, 0.0f);
    }

    /* ---- KIRBY'S FORWARD THROW, statuses 228-230 ----
     *
     * Kirby's forward throw is not the common one.
     * ftCommonThrowSetStatus branches on his fkind: he goes airborne and
     * takes nFTKirbyStatusThrowF rather than nFTCommonStatusThrowF,
     * because what he does is leap and slam. That branch has been in this
     * port, verbatim. It sets status 228, which must
     * not be an EMPTY ROW (motion 0, no procs: the throw would play the
     * wrong animation and never reach the fall or the landing).
     *
     * A caller wired to a row that does not exist is the failure shape,
     * and nothing but reading the table finds it.
     * So this checks the three rows and then the branch that reaches
     * them. ---- */
    {
        extern FTStatusDesc dFTKirbySpecialStatusDescs[];
        FTStatusDesc *r;
        int i;

        for (i = nFTKirbyStatusThrowF; i <= nFTKirbyStatusThrowFLanding; i++)
        {
            r = &dFTKirbySpecialStatusDescs[i - nFTCommonStatusSpecialStart];
            CHECK(r->mflags.motion_id != 0);
            CHECK(r->sflags.attack_id == nFTStatusAttackIDThrowF);
            CHECK(r->proc_interrupt == NULL);
            /* the game marks all three Ground and the first two plainly
             * are not: ThrowF's physics is the air transfer and
             * ThrowFFall has no physics proc at all */
            CHECK(r->sflags.ga == nMPKineticsGround);
            CHECK(r->proc_map != NULL);
        }
        r = &dFTKirbySpecialStatusDescs[nFTKirbyStatusThrowF - nFTCommonStatusSpecialStart];
        CHECK(r->proc_update == ftKirbyThrowFProcUpdate);
        CHECK(r->proc_physics == ftPhysicsApplyAirVelTransNAll);
        r = &dFTKirbySpecialStatusDescs[nFTKirbyStatusThrowFFall - nFTCommonStatusSpecialStart];
        CHECK(r->proc_update == NULL);
        CHECK(r->proc_physics == NULL);
        CHECK(r->proc_map == ftKirbyThrowFProcMap);
        r = &dFTKirbySpecialStatusDescs[nFTKirbyStatusThrowFLanding - nFTCommonStatusSpecialStart];
        CHECK(r->proc_update == ftCommonThrowProcUpdate);
        CHECK(r->proc_physics == ftKirbyThrowFLandingProcPhysics);
        CHECK(r->proc_map == ftKirbyThrowFLandingProcMap);

        /* the two setters, and what each does beside setting a status:
         * the fall clears the caught fighter's is_ignore_dead (which
         * ftCommonThrowSetStatus raised on both of them), and the
         * landing puts Kirby back on the ground first */
        spawn(0.0f, 0.0f);
        fp.fkind = nFTKindKirby;
        fp.catch_gobj = mock_gobj;      /* he clears his own, which is fine */
        fp.is_ignore_dead = TRUE;
        ftKirbyThrowFFallSetStatus(mock_gobj);
        CHECK_STATUS(nFTKirbyStatusThrowFFall);
        CHECK(fp.is_ignore_dead == FALSE);

        fp.ga = nMPKineticsAir;
        ftKirbyThrowFLandingSetStatus(mock_gobj);
        CHECK_STATUS(nFTKirbyStatusThrowFLanding);
        CHECK(fp.ga == nMPKineticsGround);
        CHECK(fp.proc_physics == ftKirbyThrowFLandingProcPhysics);

        /* and the landing's physics really does ask ga at runtime, which
         * is the one thing in this family that is not a table lookup:
         * grounded it runs the ground friction and airborne it runs the
         * air transfer, and the two touch different velocities. */
        {
            f32 g_air, a_air;

            fp.ga = nMPKineticsGround;
            fp.physics.vel_air.x = 100.0f;
            fp.physics.vel_ground.x = 100.0f;
            ftKirbyThrowFLandingProcPhysics(mock_gobj);
            g_air = fp.physics.vel_air.x;

            fp.ga = nMPKineticsAir;
            fp.physics.vel_air.x = 100.0f;
            fp.physics.vel_ground.x = 100.0f;
            ftKirbyThrowFLandingProcPhysics(mock_gobj);
            a_air = fp.physics.vel_air.x;

            CHECK(g_air != a_air);
        }

        fp.catch_gobj = NULL;
        fp.fkind = nFTKindMario;
        spawn(0.0f, 0.0f);
    }

    /* ---- THE RAPID JAB, three rows on each of five
     * fighters ----
     *
     * ft/ftcommon/ftcommonattack100.c compiles unmodified now, and with
     * it goes a hand-copy: ftCommonAttack100StartCheckInterruptCommon
     * was written out here because the rest of the file
     * was not in the build, and the decomp's own is what the jab calls.
     *
     * Five fighters have a rapid jab -- Fox, Link, Kirby, Purin and
     * Captain Falcon -- and their statuses are at DIFFERENT indices:
     * Kirby's, Fox's and Purin's start at SpecialStart itself and
     * Link's and Captain's are one further in, behind an Attack13. So
     * this walks each table by its own enum rather than by a number. ---- */
    {
        extern FTStatusDesc dFTKirbySpecialStatusDescs[];
        extern FTStatusDesc dFTFoxSpecialStatusDescs[];
        extern FTStatusDesc dFTPurinSpecialStatusDescs[];
        extern FTStatusDesc dFTCaptainSpecialStatusDescs[];
        extern FTStatusDesc dFTLinkSpecialStatusDescs[];
        struct { FTStatusDesc *tab; int start; void *phys; } k[5] = {
            { dFTKirbySpecialStatusDescs,   nFTKirbyStatusAttack100Start,
              (void *)ftPhysicsApplyGroundVelFriction },
            { dFTFoxSpecialStatusDescs,     nFTFoxStatusAttack100Start,
              (void *)ftPhysicsApplyGroundVelFriction },
            { dFTPurinSpecialStatusDescs,   nFTPurinStatusAttack100Start,
              (void *)ftPhysicsApplyGroundVelFriction },
            { dFTLinkSpecialStatusDescs,    nFTLinkStatusAttack100Start,
              (void *)ftPhysicsApplyGroundVelFriction },
            /* the one that is not the others': Captain walks forward
             * while he jabs */
            { dFTCaptainSpecialStatusDescs, nFTCaptainStatusAttack100Start,
              (void *)ftPhysicsApplyGroundVelTransN },
        };
        FTStatusDesc *r;
        int f, j;

        for (f = 0; f < 5; f++)
        {
            for (j = 0; j < 3; j++)
            {
                r = &k[f].tab[k[f].start + j - nFTCommonStatusSpecialStart];
                CHECK(r->mflags.motion_id != 0);
                CHECK(r->sflags.attack_id == nFTStatusAttackIDAttack100);
                CHECK(r->sflags.ga == nMPKineticsGround);
                CHECK((void *)r->proc_physics == k[f].phys);
                CHECK(r->proc_map == mpCommonSetFighterFallOnEdgeBreak);
            }
            /* the three differ in exactly the two columns they should */
            r = &k[f].tab[k[f].start - nFTCommonStatusSpecialStart];
            CHECK(r->proc_update == ftCommonAttack100StartProcUpdate);
            CHECK(r->proc_interrupt == NULL);
            r = &k[f].tab[k[f].start + 1 - nFTCommonStatusSpecialStart];
            CHECK(r->proc_update == ftCommonAttack100LoopProcUpdate);
            CHECK(r->proc_interrupt == ftCommonAttack100LoopProcInterrupt);
            r = &k[f].tab[k[f].start + 2 - nFTCommonStatusSpecialStart];
            CHECK(r->proc_update == ftAnimEndSetWait);
            CHECK(r->proc_interrupt == NULL);
        }

        /* and the way in, which is the decomp's own function now: enough
         * A-taps at the last jab of the chain, with the animation event
         * raised, and the fighter goes to his own Attack100Start. Fox
         * needs four and Captain six -- the counts are the switch's. */
        spawn(0.0f, 0.0f);
        fp.fkind = nFTKindFox;
        ftMainSetStatus(mock_gobj, nFTCommonStatusAttack12, 0.0f, 1.0f,
                        FTSTATUS_PRESERVE_NONE);
        fp.attack1_input_count = 3;
        fp.motion_vars.flags.flag1 = 1;
        fp.input.pl.button_tap = fp.input.button_mask_a;
        CHECK(ftCommonAttack100StartCheckInterruptCommon(mock_gobj) == TRUE);
        CHECK_STATUS(nFTFoxStatusAttack100Start);

        /* one tap short and it only arms the flag */
        spawn(0.0f, 0.0f);
        fp.fkind = nFTKindFox;
        ftMainSetStatus(mock_gobj, nFTCommonStatusAttack12, 0.0f, 1.0f,
                        FTSTATUS_PRESERVE_NONE);
        fp.attack1_input_count = 1;
        fp.motion_vars.flags.flag1 = 1;
        fp.is_goto_attack100 = FALSE;
        fp.input.pl.button_tap = fp.input.button_mask_a;
        CHECK(ftCommonAttack100StartCheckInterruptCommon(mock_gobj) == FALSE);
        CHECK_STATUS(nFTCommonStatusAttack12);

        /* and a fighter with no rapid jab is refused before the count is
         * even read -- Mario's third jab is a finisher */
        spawn(0.0f, 0.0f);
        fp.fkind = nFTKindMario;
        fp.attack1_input_count = 9;
        fp.motion_vars.flags.flag1 = 1;
        fp.input.pl.button_tap = fp.input.button_mask_a;
        CHECK(ftCommonAttack100StartCheckInterruptCommon(mock_gobj) == FALSE);
        CHECK(fp.attack1_input_count == 9);   /* untouched: the kind check is first */

        fp.input.pl.button_tap = 0;
        fp.attack1_input_count = 0;
        fp.motion_vars.flags.flag1 = 0;
        fp.is_goto_attack100 = FALSE;
        spawn(0.0f, 0.0f);
    }
}

/* the Up-B command demux, the twin of test_specialn_demux
 * above. ftCommonSpecialHiCheckInterruptCommon + dFTCommonSpecialHiStatusList,
 * wired into the same eight ground cascades (between the
 * SpecialN and SpecialLw arms), so this drives both the check function
 * directly and, at the end, the real cascades. */
static void test_specialhi_demux(void)
{
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindMario;
    fp.attr->is_have_specialhi = TRUE;

    /* B tap with the stick held up (y >= 40) -> the interrupt fires and the
     * id lands on Mario's SpecialHi row, the Super Jump Punch */
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialHiCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTMarioStatusSpecialHi);
    CHECK(fp.motion_id == nFTMarioMotionSpecialHi);
    CHECK(fp.proc_update == ftMarioSpecialHiProcUpdate);

    /* no B tap -> no interrupt */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindMario;
    fp.attr->is_have_specialhi = TRUE;
    fp.input.pl.button_tap = 0;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialHiCheckInterruptCommon(mock_gobj) == FALSE);

    /* stick one unit below the up threshold (y == 39) is out of the up band,
     * so this is a Neutral-B, not an Up-B */
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN - 1;
    CHECK(ftCommonSpecialHiCheckInterruptCommon(mock_gobj) == FALSE);

    /* the attribute gates it: a fighter with no Up-B does nothing even with
     * the stick up */
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;
    fp.attr->is_have_specialhi = FALSE;
    CHECK(ftCommonSpecialHiCheckInterruptCommon(mock_gobj) == FALSE);
    fp.attr->is_have_specialhi = TRUE;

    /* Ness was the last NULL slot here until his up-B is
     * PK Thunder now. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindNess;
    fp.attr->is_have_specialhi = TRUE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialHiCheckInterruptCommon(mock_gobj) != FALSE);
    CHECK_STATUS(nFTNessStatusSpecialHiStart);
    spawn(0.0f, 0.0f);

    /* NYoshi (20) is filled, unlike a hole.
     * The interesting part is what it does NOT do: unlike the Neutral-B
     * table, which routes NYoshi to Mario's fireball ("un bro momento"),
     * the Up-B table gives it Yoshi's own setter, so a polygon Yoshi
     * throws a real egg. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindNYoshi;
    fp.attr->is_have_specialhi = TRUE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialHiCheckInterruptCommon(mock_gobj) != FALSE);
    CHECK(fp.status_id == nFTYoshiStatusSpecialHi);

    /* the demux is now wired into the ground cascades, so a
     * real stick-up B-tap reaches it with no direct call. From a standing
     * Wait it goes through ftCommonWaitProcInterrupt, landing between the
     * SpecialN and SpecialLw arms. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindMario;
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, FTCOMMON_SPECIALHI_STICK_RANGE_MIN, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialHi);

    /* and from a Walk it goes through ftCommonWalkProcInterrupt: the same
     * arm sits in every ground cascade, not just Wait. */
    spawn(0.0f, 0.0f);
    frame(20, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWalkSlow);
    frame(20, FTCOMMON_SPECIALHI_STICK_RANGE_MIN, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialHi);

    /* and a neutral-stick B-tap from the same Wait still reaches the
     * fireball, not the Super Jump Punch: SpecialN's arm leads SpecialHi's
     * in the cascade, matching the game's own SpecialN -> SpecialHi ->
     * SpecialLw order. */
    spawn(0.0f, 0.0f);
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialN);

    /* Purin's own row, Sing -- the same shape as Fox's
     * Neutral-B row at the end of test_specialn_demux. A real stick-up
     * B-tap from a standing Purin reaches Sing with no direct call. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindPurin;
    fp.attr->is_have_specialhi = TRUE;
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, FTCOMMON_SPECIALHI_STICK_RANGE_MIN, N64_B, N64_B, 0);
    CHECK_STATUS(nFTPurinStatusSpecialHi);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);

    /* Donkey's own row, Spinning Kong -- the same shape, but
     * ProcPhysics is Donkey's own (ftDonkeySpecialHiProcPhysics), not a
     * shared ftPhysics* leaf. A real stick-up B-tap from a standing Donkey
     * reaches Spinning Kong with no direct call. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindDonkey;
    fp.attr->is_have_specialhi = TRUE;
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, FTCOMMON_SPECIALHI_STICK_RANGE_MIN, N64_B, N64_B, 0);
    CHECK_STATUS(nFTDonkeyStatusSpecialHi);
    CHECK(fp.proc_physics == ftDonkeySpecialHiProcPhysics);

    /* Samus's own row, Screw Attack -- her first special-move
     * table. Unlike Donkey's pair, ftSamusSpecialHiSetStatus lands directly
     * on nFTSamusStatusSpecialHi (227) with no same-tic transition; a real
     * stick-up B-tap from a standing Samus reaches it with no direct call. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindSamus;
    fp.attr->is_have_specialhi = TRUE;
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, FTCOMMON_SPECIALHI_STICK_RANGE_MIN, N64_B, N64_B, 0);
    CHECK_STATUS(nFTSamusStatusSpecialHi);
    CHECK(fp.proc_physics == ftSamusSpecialHiProcPhysics);

    /* Captain's Up-B rows, Falcon Dive -- his third and last
     * special file. Four rows: Hi and AirHi share every proc column and
     * differ only in kinetics; Catch has its own Update/Physics over
     * mpCommonUpdateFighterProjectFloor; Throw is three common leaves. */
    fp.fkind = nFTKindCaptain;
    ftMainSetStatus(mock_gobj, nFTCaptainStatusSpecialHi, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCaptainMotionSpecialHi);
    CHECK(fp.proc_update == ftCaptainSpecialHiProcUpdate);
    CHECK(fp.proc_interrupt == ftCaptainSpecialHiProcInterrupt);
    CHECK(fp.proc_physics == ftCaptainSpecialHiProcPhysics);
    CHECK(fp.proc_map == ftCaptainSpecialHiProcMap);

    ftMainSetStatus(mock_gobj, nFTCaptainStatusSpecialHiCatch, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCaptainMotionSpecialHiCatch);
    CHECK(fp.proc_update == ftCaptainSpecialHiCatchProcUpdate);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftCaptainSpecialHiCatchProcPhysics);
    CHECK(fp.proc_map == mpCommonUpdateFighterProjectFloor);

    ftMainSetStatus(mock_gobj, nFTCaptainStatusSpecialHiThrow, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCaptainMotionSpecialHiThrow);
    CHECK(fp.proc_update == ftAnimEndSetFall);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelTransNAll);
    CHECK(fp.proc_map == mpCommonProcFighterWaitOrLanding);

    ftMainSetStatus(mock_gobj, nFTCaptainStatusSpecialAirHi, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCaptainMotionSpecialAirHi);
    CHECK(fp.proc_update == ftCaptainSpecialHiProcUpdate);
    CHECK(fp.proc_interrupt == ftCaptainSpecialHiProcInterrupt);
    CHECK(fp.proc_physics == ftCaptainSpecialHiProcPhysics);
    CHECK(fp.proc_map == ftCaptainSpecialHiProcMap);

    /* and the caught side's common row, 179 */
    ftMainSetStatus(mock_gobj, nFTCommonStatusCaptureCaptain, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionCaptureCaptain);
    CHECK(fp.proc_update == NULL);
    CHECK(fp.proc_physics == ftCommonCaptureCaptainProcPhysics);
    CHECK(fp.proc_map == mpCommonUpdateFighterProjectFloor);

    /* a real stick-up B-tap from a standing Captain reaches Falcon Dive
     * with no direct call: airborne on the spot, every jump spent, the
     * catch params set (ftCaptainSpecialHiSetCatchParams) and the
     * per-status vars zeroed by ftCaptainSpecialHiProcStatus. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindCaptain;
    fp.attr->is_have_specialhi = TRUE;
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, FTCOMMON_SPECIALHI_STICK_RANGE_MIN, N64_B, N64_B, 0);
    CHECK_STATUS(nFTCaptainStatusSpecialHi);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(fp.jumps_used == fp.attr->jumps_max);
    CHECK(fp.catch_mask == FTCATCHKIND_MASK_CAPTAINSPECIALHI);
    CHECK(fp.proc_catch == ftCaptainSpecialHiProcCatch);
    CHECK(fp.proc_capture == ftCommonCaptureCaptainProcCapture);
    CHECK(fp.motion_vars.flags.flag2 == FTCAPTAIN_FALCONDIVE_UNK_TIMER);
    CHECK(fp.status_vars.captain.specialhi.flags == 0);
    fp.fkind = nFTKindMario;                 /* as the next test expects */
}

/* the AERIAL command demux, the third and last of the three
 * -- ftCommonSpecialAirCheckInterruptCommon over
 * dFTCommonSpecialAirN/Hi/LwStatusList (ft/ftcommon/ftcommonspecialair.c),
 * wired into ftCommonJumpProcInterrupt and ftCommonPassProcInterrupt, which
 * between them are the interrupt column of every aerial row the port has
 * (JumpF/B, JumpAerialF/B, Fall, FallAerial, Pass, and DamageFall through
 * its own wrapper). One function serves all three directions here, unlike
 * the ground pair, so this drives it directly first and then through the
 * real aerial cascades. NOTE the hammer arm (ftHammerCheckHoldHammer, which
 * swallows the press outright) cannot be driven: the port's hammer is a
 * FALSE stub, so it is always the "no hammer" branch. */
extern void (*dFTCommonSpecialAirNStatusList[])(GObj*);
extern void (*dFTCommonSpecialAirHiStatusList[])(GObj*);
extern void (*dFTCommonSpecialAirLwStatusList[])(GObj*);

static void test_specialair_demux(void)
{
    /* ---- the check function, driven directly ---- */

    /* B tap with a neutral stick, airborne -> Mario's SpecialAirN row */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindMario;
    fp.ga = nMPKineticsAir;
    fp.attr->is_have_specialairn = TRUE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = 0;
    CHECK(ftCommonSpecialAirCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTMarioStatusSpecialAirN);
    CHECK(fp.motion_id == nFTMarioMotionSpecialAirN);
    CHECK(fp.proc_update == ftMarioSpecialNProcUpdate);

    /* the same tap with the stick held up (y >= 40) -> SpecialAirHi */
    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.attr->is_have_specialairhi = TRUE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialAirCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTMarioStatusSpecialAirHi);
    CHECK(fp.motion_id == nFTMarioMotionSpecialAirHi);

    /* and with it held down (y <= -40) -> SpecialAirLw */
    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.attr->is_have_specialairlw = TRUE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALLW_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialAirCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTMarioStatusSpecialAirLw);
    CHECK(fp.motion_id == nFTMarioMotionSpecialAirLw);

    /* the bands are exclusive at their edges, the same way the two ground
     * checks are: one unit inside the neutral band on either side is a
     * Neutral-B, not an Up-B or a Down-B */
    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.attr->is_have_specialairn = TRUE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN - 1;
    CHECK(ftCommonSpecialAirCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTMarioStatusSpecialAirN);

    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.attr->is_have_specialairn = TRUE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALLW_STICK_RANGE_MIN + 1;
    CHECK(ftCommonSpecialAirCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTMarioStatusSpecialAirN);

    /* no B tap -> no interrupt in any direction */
    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.input.pl.button_tap = 0;
    fp.input.pl.stick_range.y = 0;
    CHECK(ftCommonSpecialAirCheckInterruptCommon(mock_gobj) == FALSE);
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialAirCheckInterruptCommon(mock_gobj) == FALSE);
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALLW_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialAirCheckInterruptCommon(mock_gobj) == FALSE);

    /* each direction has its own attribute, and each gates only its own:
     * with all three off nothing fires, and the up band does NOT fall
     * through to the neutral one when is_have_specialairhi is clear (the
     * decomp's else-if chain tests the stick first, the attribute second) */
    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.attr->is_have_specialairn = FALSE;
    fp.attr->is_have_specialairhi = FALSE;
    fp.attr->is_have_specialairlw = FALSE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.y = 0;
    CHECK(ftCommonSpecialAirCheckInterruptCommon(mock_gobj) == FALSE);
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialAirCheckInterruptCommon(mock_gobj) == FALSE);
    fp.attr->is_have_specialairn = TRUE;   /* neutral-B on, stick still up */
    CHECK(ftCommonSpecialAirCheckInterruptCommon(mock_gobj) == FALSE);
    CHECK_STATUS(nFTCommonStatusWait);

    /* the neutral band turns the fighter to the stick first, exactly as
     * the ground Neutral-B check does: a right-facing Mario tapping B with
     * the stick pushed left comes out facing left */
    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.lr = 1.0f;
    fp.attr->is_have_specialairn = TRUE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.x = -80;
    fp.input.pl.stick_range.y = 0;
    CHECK(ftCommonSpecialAirCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_EQF(fp.lr, -1.0f);

    /* Ness's three air specials, the last slots filled,
     * reached through the demux in each stick direction */
    {
        static const s32 kStickY[3] = {
            0, FTCOMMON_SPECIALHI_STICK_RANGE_MIN, FTCOMMON_SPECIALLW_STICK_RANGE_MIN
        };
        static const s32 kStatus[3] = {
            nFTNessStatusSpecialAirN, nFTNessStatusSpecialAirHiStart,
            nFTNessStatusSpecialAirLwStart
        };
        int k;

        for (k = 0; k < 3; k++)
        {
            spawn(0.0f, 0.0f);
            fp.fkind = nFTKindNess;
            fp.ga = nMPKineticsAir;
            fp.attr->is_have_specialairn = TRUE;
            fp.attr->is_have_specialairhi = TRUE;
            fp.attr->is_have_specialairlw = TRUE;
            fp.input.pl.button_tap = fp.input.button_mask_b;
            fp.input.pl.stick_range.x = 0;
            fp.input.pl.stick_range.y = kStickY[k];
            CHECK(ftCommonSpecialAirCheckInterruptCommon(mock_gobj) != FALSE);
            CHECK_STATUS(kStatus[k]);
        }
        spawn(0.0f, 0.0f);
    }

    /* Donkey's three Down-B slots are NULL in the GAME, not in the port:
     * the Hand Slap has no aerial form at all, and his attributes say so
     * (is_have_specialairlw 0 in DonkeyMain, NDonkeyMain and GDonkeyMain),
     * which is the only thing that keeps the demux from calling through
     * the NULL -- the decomp has no guard, and neither does the port. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindDonkey;
    fp.ga = nMPKineticsAir;
    fp.attr->is_have_specialairlw = FALSE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALLW_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialAirCheckInterruptCommon(mock_gobj) == FALSE);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(dFTCommonSpecialAirLwStatusList[nFTKindDonkey] == NULL);
    CHECK(dFTCommonSpecialAirLwStatusList[nFTKindNDonkey] == NULL);
    CHECK(dFTCommonSpecialAirLwStatusList[nFTKindGDonkey] == NULL);
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindDonkey] ==
          ftDonkeySpecialAirNStartSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindDonkey] ==
          ftDonkeySpecialAirHiSetStatus);

    /* the decomp's own slip, kept verbatim: slot 12 (Boss) of the Up-B
     * table holds ftMarioSpecialAir*N*SetStatus, not the Hi one. Every
     * other Mario-routed slot in that table is the Hi setter. */
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindBoss] ==
          ftMarioSpecialAirNSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindMMario] ==
          ftMarioSpecialAirHiSetStatus);
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindBoss] ==
          ftMarioSpecialAirNSetStatus);

    /* and the split the ground Up-B table also makes: NYoshi (20) is Mario
     * in the AirN table ("more bro momento") but Yoshi's OWN in the AirHi
     * one, and the AirLw table routes it to Yoshi's setter too. All four
     * of Yoshi's aerial slots are live, so this is the table asserting
     * the decomp's split rather than a hole. */
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindNYoshi] ==
          ftMarioSpecialAirNSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindNYoshi] ==
          ftYoshiSpecialAirHiSetStatus);
    CHECK(dFTCommonSpecialAirLwStatusList[nFTKindNYoshi] ==
          ftYoshiSpecialAirLwStartSetStatus);
    /* Yoshi's own four, all filled: the air Neutral-B slot is filled
     * with the setter. */
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindYoshi] ==
          ftYoshiSpecialAirNSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindYoshi] ==
          ftYoshiSpecialAirHiSetStatus);

    /* ---- the wiring: a real B tap in the air, no direct call ---- */

    /* a full jump puts Mario in JumpF, whose interrupt column is
     * ftCommonJumpProcInterrupt -- the SpecialAir check now leads it */
    spawn(0.0f, 0.0f);
    fp.attr->is_have_specialairn = TRUE;
    fp.attr->is_have_specialairhi = TRUE;
    fp.attr->is_have_specialairlw = TRUE;
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusJumpF);
    CHECK(fp.ga == nMPKineticsAir);
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialAirN);

    /* the same jump, but the stick up on the B tap -> the air Super Jump
     * Punch. (An aerial jump would also answer a stick-up frame; the
     * SpecialAir arm leads it, so the B tap wins.) */
    spawn(0.0f, 0.0f);
    fp.attr->is_have_specialairhi = TRUE;
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusJumpF);
    frame(0, FTCOMMON_SPECIALHI_STICK_RANGE_MIN, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialAirHi);

    /* and the stick down -> the air Tornado */
    spawn(0.0f, 0.0f);
    fp.attr->is_have_specialairlw = TRUE;
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusJumpF);
    frame(0, FTCOMMON_SPECIALLW_STICK_RANGE_MIN, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialAirLw);

    /* Fox's air Blaster, through the same cascade: the demux is the only
     * thing that reaches ftFoxSpecialAirNSetStatus, whose row is in
     * dFTFoxSpecialStatusDescs */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;
    fp.attr->is_have_specialairn = TRUE;
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusJumpF);
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTFoxStatusSpecialAirN);

    /* Fox's air Fire Fox -- the aerial half of the ground/air pair,
     * reached from the air */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;
    fp.attr->is_have_specialairhi = TRUE;
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusJumpF);
    frame(0, FTCOMMON_SPECIALHI_STICK_RANGE_MIN, N64_B, N64_B, 0);
    CHECK_STATUS(nFTFoxStatusSpecialAirHiStart);

    /* Kirby's air Stone. His AirLw rows have sat in
     * dFTKirbySpecialStatusDescs with nothing to reach them --
     * this demux is the first thing that does. */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindKirby;
    fp.attr->is_have_specialairlw = TRUE;
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusJumpF);
    frame(0, FTCOMMON_SPECIALLW_STICK_RANGE_MIN, N64_B, N64_B, 0);
    CHECK_STATUS(nFTKirbyStatusSpecialAirLwStart);

    /* Purin's air Sing, and Yoshi's air Yoshi Bomb: the last two of the
     * eight fighters with an aerial special */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindPurin;
    fp.attr->is_have_specialairhi = TRUE;
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusJumpF);
    frame(0, FTCOMMON_SPECIALHI_STICK_RANGE_MIN, N64_B, N64_B, 0);
    CHECK_STATUS(nFTPurinStatusSpecialAirHi);

    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindYoshi;
    fp.attr->is_have_specialairlw = TRUE;
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusJumpF);
    frame(0, FTCOMMON_SPECIALLW_STICK_RANGE_MIN, N64_B, N64_B, 0);
    CHECK_STATUS(nFTYoshiStatusSpecialAirLwStart);

    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindMario;                 /* as the next test expects */
}

/* the aerials -- ft/ftcommon/ftcommonattackair.c's four
 * functions in src/dc/ftcommon.c, the eleven rows they need in
 * dFTCommonActionStatusDescs (209-213 AttackAir, 214-219 LandingAir), and
 * the AttackAir check wired into ftCommonJumpProcInterrupt and
 * ftCommonPassProcInterrupt behind the SpecialAir one. With those
 * two arms real, both cascades are now the game's whole. */
static void test_attack_air(void)
{
    /* the five aerial attributes: the mock's is_have word (kMario above)
     * is the subset of the ROM's the port can reach, and it
     * includes the aerials' five bits. On target the fighter's
     * own ROM word is read whole, so this pins the mock, not the game. */
    spawn(0.0f, 0.0f);
    CHECK(fp.attr->is_have_attackairn);
    CHECK(fp.attr->is_have_attackairf);
    CHECK(fp.attr->is_have_attackairb);
    CHECK(fp.attr->is_have_attackairhi);
    CHECK(fp.attr->is_have_attackairlw);

    /* ---- the eleven rows ---- */

    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackAirN, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionAttackAirN);
    CHECK(fp.proc_update == ftAnimEndSetFall);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelDrift);
    CHECK(fp.proc_map == ftCommonAttackAirProcMap);

    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackAirF, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionAttackAirF);
    CHECK(fp.proc_update == ftAnimEndSetFall);
    CHECK(fp.proc_map == ftCommonAttackAirProcMap);

    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackAirB, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionAttackAirB);
    CHECK(fp.proc_update == ftAnimEndSetFall);

    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackAirHi, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionAttackAirHi);
    CHECK(fp.proc_update == ftAnimEndSetFall);

    /* the one aerial with an update column of its own: Link's rehit */
    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackAirLw, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionAttackAirLw);
    CHECK(fp.proc_update == ftCommonAttackAirLwProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelDrift);
    CHECK(fp.proc_map == ftCommonAttackAirProcMap);

    /* the six landings: one shape, and each keeps the attack id of the
     * aerial it lands */
    ftMainSetStatus(mock_gobj, nFTCommonStatusLandingAirN, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionLandingAirN);
    CHECK(fp.proc_update == ftAnimEndSetWait);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == mpCommonProcFighterOnCliffEdge);

    ftMainSetStatus(mock_gobj, nFTCommonStatusLandingAirF, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionLandingAirF);
    ftMainSetStatus(mock_gobj, nFTCommonStatusLandingAirB, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionLandingAirB);
    ftMainSetStatus(mock_gobj, nFTCommonStatusLandingAirHi, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionLandingAirHi);
    ftMainSetStatus(mock_gobj, nFTCommonStatusLandingAirLw, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionLandingAirLw);
    ftMainSetStatus(mock_gobj, nFTCommonStatusLandingAirNull, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionLandingAirNull);
    CHECK(fp.proc_update == ftAnimEndSetWait);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);

    /* ---- the check function, driven directly ---- */

    /* A tap, stick neutral (both axes inside 20) -> the neutral air. The
     * setter clears flag1 -- so a landing before the motion script sets
     * it costs no lag -- and pushes tics_since_last_z out of the L-cancel
     * window, so a Z tap from before the swing cannot smooth this one. */
    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.motion_vars.flags.flag1 = 7;
    fp.tics_since_last_z = 0;
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = 0;
    CHECK(ftCommonAttackAirCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusAttackAirN);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK(fp.tics_since_last_z == FTINPUT_ZTRIGLAST_TICS_MAX);

    /* one unit inside the neutral box on both axes is still the neutral
     * air; one unit outside on x is a directional one (angle 0 -> forward
     * against a right-facing Mario) */
    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.lr = 1.0f;
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.input.pl.stick_range.x = FTCOMMON_ATTACKAIR_DIRECTION_STICK_RANGE_MIN - 1;
    fp.input.pl.stick_range.y = FTCOMMON_ATTACKAIR_DIRECTION_STICK_RANGE_MIN - 1;
    CHECK(ftCommonAttackAirCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusAttackAirN);

    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.lr = 1.0f;
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.input.pl.stick_range.x = FTCOMMON_ATTACKAIR_DIRECTION_STICK_RANGE_MIN;
    fp.input.pl.stick_range.y = 0;
    CHECK(ftCommonAttackAirCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusAttackAirF);

    /* the stick straight up is atan2(80, 0) = 90 degrees, past the
     * 50-degree cut, so it is the up air whatever the facing */
    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = 80;
    CHECK(ftCommonAttackAirCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusAttackAirHi);

    /* and straight down is -90 -> the down air, the one that installs a
     * proc_hit and zeroes the rehit timer */
    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.status_vars.common.attackair.rehit_timer = 9;
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = -80;
    CHECK(ftCommonAttackAirCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusAttackAirLw);
    CHECK(fp.proc_hit == ftCommonAttackAirLwProcHit);
    CHECK(fp.status_vars.common.attackair.rehit_timer == 0);

    /* the angle is atan2(y, |x|), so the forward/back split is on the sign
     * of x against the facing and nothing else: the same stick is the
     * forward air facing right and the back air facing left */
    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.lr = 1.0f;
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.input.pl.stick_range.x = -80;
    fp.input.pl.stick_range.y = 0;
    CHECK(ftCommonAttackAirCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusAttackAirB);

    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.lr = -1.0f;
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.input.pl.stick_range.x = -80;
    fp.input.pl.stick_range.y = 0;
    CHECK(ftCommonAttackAirCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusAttackAirF);

    /* no A tap -> nothing; and each attribute gates only its own aerial */
    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.input.pl.button_tap = 0;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = 0;
    CHECK(ftCommonAttackAirCheckInterruptCommon(mock_gobj) == FALSE);

    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.attr->is_have_attackairn = FALSE;
    CHECK(ftCommonAttackAirCheckInterruptCommon(mock_gobj) == FALSE);
    CHECK_STATUS(nFTCommonStatusWait);
    fp.input.pl.stick_range.y = 80;         /* up still works */
    CHECK(ftCommonAttackAirCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusAttackAirHi);
    fp.attr->is_have_attackairn = TRUE;

    /* ---- Link's rehit, the one fkind-gated pair in the file ---- */

    /* on anyone but Link the hit callback does nothing at all */
    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    fp.physics.vel_air.y = -30.0f;
    fp.is_fastfall = TRUE;
    ftCommonAttackAirLwProcHit(mock_gobj);
    CHECK_EQF(fp.physics.vel_air.y, -30.0f);
    CHECK(fp.is_fastfall == TRUE);

    /* on Link it clears the hitboxes, kills the fast fall, bounces him
     * back up and arms the timer -- and, past the rewind frame, rewinds
     * the swing to it so the same attack can connect twice */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindLink;
    fp.ga = nMPKineticsAir;
    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackAirLw, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    mock_gobj->anim_frame = FTCOMMON_ATTACKAIRLW_LINK_REHIT_FRAME_BEGIN + 5.0f;
    fp.is_fastfall = TRUE;
    fp.physics.vel_air.y = -30.0f;
    ftCommonAttackAirLwProcHit(mock_gobj);
    CHECK(fp.is_fastfall == FALSE);
    CHECK_EQF(fp.physics.vel_air.y, FTCOMMON_ATTACKAIRLW_LINK_REHIT_BOUNCE_VEL_Y);
    CHECK_STATUS(nFTCommonStatusAttackAirLw);
    CHECK_EQF(mock_gobj->anim_frame, FTCOMMON_ATTACKAIRLW_LINK_REHIT_FRAME_BEGIN);
    CHECK(fp.status_vars.common.attackair.rehit_timer ==
          FTCOMMON_ATTACKAIRLW_LINK_REHIT_TIMER);

    /* and before that frame it arms the timer without rewinding */
    mock_gobj->anim_frame = 10.0f;
    fp.status_vars.common.attackair.rehit_timer = 0;
    ftCommonAttackAirLwProcHit(mock_gobj);
    CHECK_EQF(mock_gobj->anim_frame, 10.0f);
    CHECK(fp.status_vars.common.attackair.rehit_timer ==
          FTCOMMON_ATTACKAIRLW_LINK_REHIT_TIMER);

    /* the update column counts it down, one a tic */
    fp.status_vars.common.attackair.rehit_timer = 3;
    ftCommonAttackAirLwProcUpdate(mock_gobj);
    CHECK(fp.status_vars.common.attackair.rehit_timer == 2);
    ftCommonAttackAirLwProcUpdate(mock_gobj);
    ftCommonAttackAirLwProcUpdate(mock_gobj);
    CHECK(fp.status_vars.common.attackair.rehit_timer == 0);
    /* at zero it stops counting rather than wrapping */
    ftCommonAttackAirLwProcUpdate(mock_gobj);
    CHECK(fp.status_vars.common.attackair.rehit_timer == 0);
    fp.fkind = nFTKindMario;

    /* ---- the wiring: a real A tap in the air, no direct call ---- */

    /* a full jump, then A with the stick neutral -> the neutral air. The
     * aerial jump answers A too, and this proves the AttackAir arm leads
     * it: jumps_used does not move. */
    spawn(0.0f, 0.0f);
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusJumpF);
    CHECK(fp.jumps_used == 1);
    frame(0, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackAirN);
    CHECK(fp.jumps_used == 1);

    /* the same jump with the stick down on the A tap -> the down air */
    spawn(0.0f, 0.0f);
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, -80, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackAirLw);

    /* and a B tap on the same frame still reaches the special, not the
     * aerial: SpecialAir's arm leads AttackAir's, the game's order */
    spawn(0.0f, 0.0f);
    fp.attr->is_have_specialairn = TRUE;
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, N64_A | N64_B, N64_A | N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialAirN);

    /* ---- the landing, ftCommonAttackAirProcMap's three arms ---- */

    /* with no lag percentage in flag1 (which is where the setter leaves
     * it) the lag arm is skipped, and a fighter still descending faster
     * than FTCOMMON_ATTACKAIR_SKIPLANDING_VEL_Y_MAX takes the ordinary
     * landing rather than dropping straight into Wait */
    spawn(0.0f, 0.0f);
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackAirN);
    run_until(0, 0, 120, is_grounded);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK(fp.status_id == nFTCommonStatusLandingLight);

    /* and with a lag percentage set, and no recent Z, the landing is the
     * aerial's own -- LandingAirN here, since Mario's pack carries that
     * animation; a pack without one takes the generic LandingAirNull at
     * the lag's own speed instead, which is the other arm of the
     * has-an-animation test. */
    spawn(0.0f, 0.0f);
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackAirN);
    while (fp.ga != nMPKineticsGround)
    {
        fp.motion_vars.flags.flag1 = 50;         /* 50% lag */
        fp.tics_since_last_z = FTCOMMON_ATTACKAIR_SMOOTHLANDING_TICS_MAX + 1;
        frame(0, 0, 0, 0, 0);
    }
    CHECK(fp.status_id == nFTCommonStatusLandingAirN);

    /* the same landing with a Z tap inside the smooth-landing window
     * skips the lag arm entirely -- the L-cancel */
    spawn(0.0f, 0.0f);
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, 0, FT_JUMP_BUTTONS_TEST, 0);
    frame(0, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackAirN);
    while (fp.ga != nMPKineticsGround)
    {
        fp.motion_vars.flags.flag1 = 50;
        fp.tics_since_last_z = 0;                /* Z just now */
        frame(0, 0, 0, 0, 0);
    }
    CHECK(fp.status_id != nFTCommonStatusLandingAirN);
    CHECK(fp.status_id != nFTCommonStatusLandingAirNull);

    spawn(0.0f, 0.0f);                           /* as the next test expects */
}

/* Fire Fox, Fox's Up-B -- ft/ftchar/ftfox/ftfoxspecialhi.c's
 * thirty procs, compiled unmodified, over dFTFoxSpecialStatusDescs' nine
 * new rows (227-235). The move is a ground/air pair at every stage: Start
 * (the crouch, FTFOX_FIREFOX_GRAVITY_DELAY tics before gravity bites and
 * the entry velocity halved), Hold (the charge, FTFOX_FIREFOX_LAUNCH_DELAY
 * tics), the travel itself (FTFOX_FIREFOX_TRAVEL_TIME tics at
 * FTFOX_FIREFOX_VEL, decelerating from the second tic) and End -- plus the
 * aerial-only Bound, the recoil off a surface he meets head-on. The two
 * leaves the port gained for it get their own checks below: the graze test
 * lbCommonCheckAdjustSim2D (src/dc/lbcommon.c), and ftFoxSpecialHiProcPass,
 * the predicate mpCommonCheckFighterPass (src/dc/mpcommon.c) carries into
 * the floor walk. */
extern sb32 lbCommonCheckAdjustSim2D(Vec3f *a, Vec3f *b, f32 angle);

static void test_fox_firefox(void)
{
    Vec3f vel, surface;
    float mag;

    /* ---- the nine rows ---- */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialHiStart, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialHiStart);
    CHECK(fp.proc_update == ftFoxSpecialHiStartProcUpdate);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == ftFoxSpecialHiStartProcMap);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialAirHiStart, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialAirHiStart);
    CHECK(fp.proc_update == ftFoxSpecialAirHiStartProcUpdate);
    CHECK(fp.proc_physics == ftFoxSpecialAirHiStartProcPhysics);
    CHECK(fp.proc_map == ftFoxSpecialAirHiStartProcMap);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialHiHold, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialHiHold);
    CHECK(fp.proc_update == ftFoxSpecialHiHoldProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == ftFoxSpecialHiHoldProcMap);

    /* the air Hold borrows the air Start's physics proc -- the same
     * delayed gravity, the charge just keeps running */
    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialAirHiHold, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialAirHiHold);
    CHECK(fp.proc_update == ftFoxSpecialHiHoldProcUpdate);
    CHECK(fp.proc_physics == ftFoxSpecialAirHiStartProcPhysics);
    CHECK(fp.proc_map == ftFoxSpecialAirHiHoldProcMap);

    /* the travel pair share their update proc and differ only in physics */
    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialHi, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialHi);
    CHECK(fp.proc_update == ftFoxSpecialHiProcUpdate);
    CHECK(fp.proc_physics == ftFoxSpecialHiProcPhysics);
    CHECK(fp.proc_map == ftFoxSpecialHiProcMap);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialAirHi, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialAirHi);
    CHECK(fp.proc_update == ftFoxSpecialHiProcUpdate);
    CHECK(fp.proc_physics == ftFoxSpecialAirHiProcPhysics);
    CHECK(fp.proc_map == ftFoxSpecialAirHiProcMap);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialHiEnd, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialHiEnd);
    CHECK(fp.proc_update == ftAnimEndSetWait);
    CHECK(fp.proc_physics == ftFoxSpecialHiEndProcPhysics);
    CHECK(fp.proc_map == mpCommonSetFighterFallOnGroundBreak);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialAirHiEnd, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialAirHiEnd);
    CHECK(fp.proc_update == ftFoxSpecialAirHiEndProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelDrift);
    CHECK(fp.proc_map == ftFoxSpecialAirHiEndProcMap);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialAirHiBound, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTFoxMotionSpecialAirHiBound);
    CHECK(fp.proc_update == ftFoxSpecialAirHiBoundProcUpdate);
    CHECK(fp.proc_physics == ftFoxSpecialAirHiBoundProcPhysics);
    CHECK(fp.proc_map == ftFoxSpecialAirHiBoundProcMap);

    /* ---- the crouch: entry velocity halved, gravity held off ---- */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;
    fp.attr->is_have_specialhi = TRUE;
    fp.physics.vel_ground.x = 40.0f;
    ftFoxSpecialHiStartSetStatus(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialHiStart);
    CHECK_NEAR(fp.physics.vel_ground.x, 20.0f, 1e-4f);
    CHECK(fp.status_vars.fox.specialhi.gravity_delay == FTFOX_FIREFOX_GRAVITY_DELAY);

    /* its air twin kills the vertical velocity outright and halves the
     * horizontal one -- Fox stops dead where the crouch begins */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;
    fp.physics.vel_air.x = 30.0f;
    fp.physics.vel_air.y = -50.0f;
    ftFoxSpecialAirHiStartSetStatus(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialAirHiStart);
    CHECK_NEAR(fp.physics.vel_air.x, 15.0f, 1e-4f);
    CHECK_NEAR(fp.physics.vel_air.y, 0.0f, 1e-4f);
    CHECK(fp.status_vars.fox.specialhi.gravity_delay == FTFOX_FIREFOX_GRAVITY_DELAY);

    /* the gravity delay is a countdown, and only when it is spent does
     * gravity start pulling (ftFoxSpecialAirHiStartProcPhysics) */
    fp.physics.vel_air.y = 0.0f;
    fp.status_vars.fox.specialhi.gravity_delay = 2;
    ftFoxSpecialAirHiStartProcPhysics(mock_gobj);
    CHECK(fp.status_vars.fox.specialhi.gravity_delay == 1);
    CHECK_NEAR(fp.physics.vel_air.y, 0.0f, 1e-4f);
    ftFoxSpecialAirHiStartProcPhysics(mock_gobj);
    CHECK(fp.status_vars.fox.specialhi.gravity_delay == 0);
    CHECK_NEAR(fp.physics.vel_air.y, 0.0f, 1e-4f);
    ftFoxSpecialAirHiStartProcPhysics(mock_gobj);
    CHECK(fp.physics.vel_air.y < 0.0f);

    /* ---- the charge, then the launch straight up ---- */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;
    fp.attr->is_have_specialhi = TRUE;
    ftFoxSpecialHiHoldSetStatus(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialHiHold);
    CHECK(fp.status_vars.fox.specialhi.launch_delay == FTFOX_FIREFOX_LAUNCH_DELAY);

    /* one tic short of the launch nothing happens */
    fp.status_vars.fox.specialhi.launch_delay = 2;
    ftFoxSpecialHiHoldProcUpdate(mock_gobj);
    CHECK(fp.status_vars.fox.specialhi.launch_delay == 1);
    CHECK_STATUS(nFTFoxStatusSpecialHiHold);

    /* on the last tic, with the stick neutral, he goes airborne straight up:
     * angle 90 degrees, the whole FTFOX_FIREFOX_VEL in y, every jump spent */
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = 0;
    ftFoxSpecialHiHoldProcUpdate(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialAirHi);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK_NEAR(fp.status_vars.fox.specialhi.angle, 1.57079633f, 1e-5f);
    CHECK_NEAR(fp.physics.vel_air.x, 0.0f, 1e-3f);
    CHECK_NEAR(fp.physics.vel_air.y, FTFOX_FIREFOX_VEL, 1e-3f);
    CHECK(fp.status_vars.fox.specialhi.anim_frames == FTFOX_FIREFOX_TRAVEL_TIME);
    CHECK(fp.status_vars.fox.specialhi.decelerate_wait == 0);
    CHECK(fp.status_vars.fox.specialhi.pass_timer == 0);
    CHECK(fp.jumps_used == fp.attr->jumps_max);

    /* ---- the same launch with the stick held into the floor: he stays on
     * the ground and travels along it instead (ftFoxSpecialHiDecideSetStatus's
     * other arm -- the stick is past the threshold and more than 90 degrees
     * off the floor's normal) ---- */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;
    fp.attr->is_have_specialhi = TRUE;
    CHECK(!(fp.coll_data.floor_flags & MAP_VERTEX_COLL_PASS));
    ftFoxSpecialHiHoldSetStatus(mock_gobj);
    fp.status_vars.fox.specialhi.launch_delay = 1;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = -FTFOX_FIREFOX_ANGLE_STICK_THRESHOLD;
    ftFoxSpecialHiHoldProcUpdate(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialHi);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK_NEAR(fp.physics.vel_ground.x, FTFOX_FIREFOX_VEL, 1e-4f);
    CHECK_NEAR(fp.status_vars.fox.specialhi.angle, 0.0f, 1e-5f);   /* flat deck */
    CHECK(fp.status_vars.fox.specialhi.anim_frames == FTFOX_FIREFOX_TRAVEL_TIME);

    /* the ground travel decelerates from the second tic and hands the
     * velocity on to vel_air through the floor's angle (flat here, so
     * straight across) */
    CHECK(fp.status_vars.fox.specialhi.decelerate_wait == 0);
    ftFoxSpecialHiProcPhysics(mock_gobj);
    CHECK(fp.status_vars.fox.specialhi.decelerate_wait == 1);
    CHECK_NEAR(fp.physics.vel_ground.x, FTFOX_FIREFOX_VEL, 1e-4f);
    ftFoxSpecialHiProcPhysics(mock_gobj);
    CHECK(fp.status_vars.fox.specialhi.decelerate_wait == FTFOX_FIREFOX_DECELERATE_DELAY);
    CHECK_NEAR(fp.physics.vel_ground.x, FTFOX_FIREFOX_VEL - FTFOX_FIREFOX_DECELERATE_VEL, 1e-3f);
    CHECK_NEAR(fp.physics.vel_air.x, fp.lr * fp.physics.vel_ground.x, 1e-3f);

    /* and when the travel time runs out on the ground it ends there */
    fp.status_vars.fox.specialhi.anim_frames = 1;
    ftFoxSpecialHiProcUpdate(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialHiEnd);

    /* ---- the air travel: the deceleration comes off along the angle, and
     * the model's pitch tracks the velocity (joint 4, the hip) ---- */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;
    fp.lr = +1;
    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialAirHi, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.ga = nMPKineticsAir;    /* the row's kinetics; the setter took him up */
    fp.status_vars.fox.specialhi.angle = 0.0f;         /* straight ahead */
    fp.status_vars.fox.specialhi.anim_frames = FTFOX_FIREFOX_TRAVEL_TIME;
    fp.status_vars.fox.specialhi.decelerate_wait = FTFOX_FIREFOX_DECELERATE_DELAY - 1;
    fp.physics.vel_air.x = FTFOX_FIREFOX_VEL;
    fp.physics.vel_air.y = 0.0f;
    ftFoxSpecialAirHiProcPhysics(mock_gobj);
    CHECK_NEAR(fp.physics.vel_air.x, FTFOX_FIREFOX_VEL - FTFOX_FIREFOX_DECELERATE_VEL, 1e-3f);
    CHECK_NEAR(fp.physics.vel_air.y, 0.0f, 1e-3f);
    /* moving flat out along +x, so the pitch is atan2(x, y) - 90 == 0 */
    CHECK_NEAR(fp.joints[4]->rotate.vec.f.x, 0.0f, 1e-4f);

    /* the same travel run out in the air ends in the air */
    fp.status_vars.fox.specialhi.anim_frames = 1;
    ftFoxSpecialHiProcUpdate(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialAirHiEnd);

    /* whose own update drops him into the helpless fall when the animation
     * is spent */
    mock_gobj->anim_frame = 0.0f;
    ftFoxSpecialAirHiEndProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusFallSpecial);

    /* ---- the Bound: the recoil status clears flag1, and the motion
     * script raising it again drops him into the helpless fall ---- */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;
    fp.motion_vars.flags.flag1 = TRUE;
    ftFoxSpecialAirHiBoundSetStatus(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialAirHiBound);
    CHECK(fp.motion_vars.flags.flag1 == FALSE);

    fp.ga = nMPKineticsAir;
    mock_gobj->anim_frame = 5.0f;
    ftFoxSpecialAirHiBoundProcUpdate(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialAirHiBound);       /* flag still down */
    fp.motion_vars.flags.flag1 = TRUE;
    ftFoxSpecialAirHiBoundProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusFallSpecial);

    /* on the ground the same animation running out is an ordinary Wait */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;
    ftFoxSpecialAirHiBoundSetStatus(mock_gobj);
    fp.ga = nMPKineticsGround;
    mock_gobj->anim_frame = 0.0f;
    ftFoxSpecialAirHiBoundProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusWait);

    /* ---- the ground/air status swaps the map procs drive ---- */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;
    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialHiStart, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftFoxSpecialHiStartSwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialAirHiStart);
    CHECK(fp.ga == nMPKineticsAir);
    ftFoxSpecialAirHiStartSwitchStatusGround(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialHiStart);
    CHECK(fp.ga == nMPKineticsGround);

    ftMainSetStatus(mock_gobj, nFTFoxStatusSpecialHiHold, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftFoxSpecialHiHoldSwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialAirHiHold);
    ftFoxSpecialAirHiHoldSwitchStatusGround(mock_gobj);
    CHECK_STATUS(nFTFoxStatusSpecialHiHold);

    /* ---- ftFoxSpecialHiProcPass, the predicate mpCommonCheckFighterPass
     * hands the floor walk: on a solid floor he always stops (TRUE); the
     * drop-through platform he passes for fifteen tics and no longer ---- */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;
    fp.status_vars.fox.specialhi.pass_timer = 0;
    CHECK(!(fp.coll_data.floor_flags & MAP_VERTEX_COLL_PASS));
    CHECK(ftFoxSpecialHiProcPass(mock_gobj) == TRUE);
    fp.coll_data.floor_flags |= MAP_VERTEX_COLL_PASS;
    CHECK(ftFoxSpecialHiProcPass(mock_gobj) == FALSE);
    fp.status_vars.fox.specialhi.pass_timer = 14;
    CHECK(ftFoxSpecialHiProcPass(mock_gobj) == FALSE);
    fp.status_vars.fox.specialhi.pass_timer = 15;
    CHECK(ftFoxSpecialHiProcPass(mock_gobj) == TRUE);

    /* ---- lbCommonCheckAdjustSim2D, the graze test his aerial ProcMap runs
     * against every surface he touches ---- */

    /* head-on into a wall whose normal points back at him: no graze, the
     * caller goes on to bounce or end the move, and the velocity is
     * untouched */
    vel.x = 100.0f; vel.y = 0.0f; vel.z = 0.0f;
    surface.x = -1.0f; surface.y = 0.0f; surface.z = 0.0f;
    CHECK(lbCommonCheckAdjustSim2D(&vel, &surface, FTFOX_FIREFOX_BOUND_ANGLE) == FALSE);
    CHECK_NEAR(vel.x, 100.0f, 1e-4f);
    CHECK_NEAR(vel.y, 0.0f, 1e-4f);

    /* a shallow clip of a steep slope: within the 20-degree window, so the
     * velocity is rotated onto the surface -- same magnitude, now
     * perpendicular to the normal, and sliding up rather than into it */
    vel.x = 100.0f; vel.y = 0.0f; vel.z = 0.0f;
    surface.x = -0.2f; surface.y = 0.9797959f; surface.z = 0.0f;
    CHECK(lbCommonCheckAdjustSim2D(&vel, &surface, FTFOX_FIREFOX_BOUND_ANGLE) == TRUE);
    mag = sqrtf(vel.x * vel.x + vel.y * vel.y);
    CHECK_NEAR(mag, 100.0f, 1e-3f);
    CHECK_NEAR(vel.x * surface.x + vel.y * surface.y, 0.0f, 1e-3f);
    CHECK(vel.x > 0.0f && vel.y > 0.0f);

    /* and one moving with the surface, not against it, is no graze at all */
    vel.x = 100.0f; vel.y = 0.0f; vel.z = 0.0f;
    surface.x = 1.0f; surface.y = 0.0f; surface.z = 0.0f;
    CHECK(lbCommonCheckAdjustSim2D(&vel, &surface, FTFOX_FIREFOX_BOUND_ANGLE) == FALSE);
    CHECK_NEAR(vel.x, 100.0f, 1e-4f);

    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindMario;                 /* as the next test expects */
}

/* the grab half of Falcon Dive -- ftcaptainspecialhi.c's
 * ProcCatch/CatchProcPhysics/CatchProcUpdate/ThrowSetStatus on the catcher
 * and ft/ftcommon/ftcommoncapturecaptain.c (compiled unmodified) on the
 * caught: ProcCapture, ProcPhysics through UpdatePositions, and Release.
 * Driven the way test_grab's connect is (the mock's motions raise no
 * catch box, so the two procs the search fires run directly), with the
 * catcher's joint 29 -- which the 3-joint mock has not got -- aliased to
 * its TopN the way test_ft_mario_specialn_accessory aliases the fireball's
 * spawn joint. UpdatePositions' Vec2h read goes through the port's own
 * copy of dCaptainMainMotion_0x0000 under the bound file base, so the
 * numbers below are the decomp's Mario row (30, 70). */
extern void ftCommonCaptureCaptainBindOffsets(void);
extern void ftCommonCaptureCaptainUpdatePositions(GObj *fighter_gobj, GObj *capture_gobj, Vec3f *pos);
extern Vec2h dCaptainMainMotion_0x0000[];
extern int llCaptainMainMotionSpecialHiVec2h;
static FTThrowHitDesc mock_throw_desc;   /* test_grab's, defined below it */

static void test_captain_falcon_dive(void)
{
    DObj *saved_j29;
    Vec3f v;

    ftCommonCaptureCaptainBindOffsets();
    {
        Vec2h *offset_add = lbRelocGetFileData(Vec2h*, gFTDataCaptainMainMotion,
                                               &llCaptainMainMotionSpecialHiVec2h);
        CHECK(offset_add == dCaptainMainMotion_0x0000);
        CHECK(offset_add[nFTKindMario].x == 0x1E && offset_add[nFTKindMario].y == 0x46);
        CHECK(offset_add[nFTKindGDonkey].x == 0x64 && offset_add[nFTKindGDonkey].y == 0xFA);
    }

    spawn(0.0f, 0.0f);
    spawn_second(30.0f, 0.0f, +1);
    fp.fkind = nFTKindCaptain;
    fp2.fkind = nFTKindMario;
    saved_j29 = fp.joints[29];
    fp.joints[29] = fp.joints[nFTPartsJointTopN];

    ftCaptainSpecialHiSetStatus(mock_gobj);
    CHECK_STATUS(nFTCaptainStatusSpecialHi);
    CHECK(fp.lr == +1);

    /* the connect: catcher's proc_catch, then the caught's proc_capture,
     * as ftMainProcSearchCatch fires them. The caught body is on the
     * ground: on the CATCHER that means is_catch_or_capture TRUE and no
     * NOUPDATE (that flag, which shares its storage with
     * status_vars.captain.specialhi.flags bit 4, goes up only when the
     * caught body was airborne, and then the catcher stays put); on the
     * CAUGHT side the same ground check raises its own NOUPDATE, so the
     * body is not dragged. */
    fp.search_gobj = mock_gobj2;
    ftCaptainSpecialHiProcCatch(mock_gobj);
    ftCommonCaptureCaptainProcCapture(mock_gobj2, mock_gobj);

    CHECK_STATUS(nFTCaptainStatusSpecialHiCatch);
    CHECK(fp.catch_gobj == mock_gobj2);
    CHECK(fp.capture_immune_mask == FTCATCHKIND_MASK_ALL);
    CHECK(fp2.ga == nMPKineticsAir);        /* ProcCapture takes the caught body airborne */
    CHECK(fp.is_catch_or_capture == TRUE);
    CHECK(!(fp.status_vars.common.capturecaptain.capture_flag & FTCOMMON_CAPTURECAPTAIN_MASK_NOUPDATE));
    CHECK(fp2.status_id == nFTCommonStatusCaptureCaptain);
    CHECK(fp2.capture_gobj == mock_gobj);
    CHECK(fp2.lr == -fp.lr);
    CHECK(fp2.capture_immune_mask == FTCATCHKIND_MASK_ALL);
    CHECK(fp2.is_catch_or_capture == TRUE);
    CHECK(fp2.status_vars.common.capturecaptain.capture_flag & FTCOMMON_CAPTURECAPTAIN_MASK_NOUPDATE);

    /* UpdatePositions, the decomp's arithmetic: catcher's joint 29 minus
     * (caught TopN + the Mario row scaled by the catcher's facing) --
     * catcher at (0,0), caught at (30,0), lr +1 -> (0,0) - (60,70) */
    ftCommonCaptureCaptainUpdatePositions(mock_gobj, mock_gobj2, &v);
    CHECK(fabsf(v.x - (-60.0f)) < 1e-3f);
    CHECK(fabsf(v.y - (-70.0f)) < 1e-3f);
    CHECK(fabsf(v.z) < 1e-5f);

    /* the caught side's ProcPhysics leaves the body where it is under
     * NOUPDATE; with the flag down it drags the body to the catcher's
     * hand (translate += the vector, clamped to 180 a tic) */
    ftCommonCaptureCaptainProcPhysics(mock_gobj2);
    CHECK(fabsf(DObjGetStruct(mock_gobj2)->translate.vec.f.x - 30.0f) < 1e-3f);
    fp2.status_vars.common.capturecaptain.capture_flag &= ~FTCOMMON_CAPTURECAPTAIN_MASK_NOUPDATE;
    ftCommonCaptureCaptainProcPhysics(mock_gobj2);
    CHECK(fabsf(DObjGetStruct(mock_gobj2)->translate.vec.f.x - (30.0f - 60.0f)) < 1e-3f);
    CHECK(fabsf(DObjGetStruct(mock_gobj2)->translate.vec.f.y - (0.0f - 70.0f)) < 1e-3f);
    DObjGetStruct(mock_gobj2)->translate.vec.f.x = 30.0f;
    DObjGetStruct(mock_gobj2)->translate.vec.f.y = 0.0f;
    fp2.status_vars.common.capturecaptain.capture_flag |= FTCOMMON_CAPTURECAPTAIN_MASK_NOUPDATE;

    /* and the catcher's own CatchProcPhysics pulls the catcher toward the
     * body instead (translate -= the vector) unless bit 4 of its flags
     * says not to */
    ftCaptainSpecialHiCatchProcPhysics(mock_gobj);
    CHECK(fabsf(DObjGetStruct(mock_gobj)->translate.vec.f.x - 60.0f) < 1e-3f);
    CHECK(fabsf(DObjGetStruct(mock_gobj)->translate.vec.f.y - 70.0f) < 1e-3f);
    DObjGetStruct(mock_gobj)->translate.vec.f.x = 0.0f;
    DObjGetStruct(mock_gobj)->translate.vec.f.y = 0.0f;
    fp.status_vars.captain.specialhi.flags |= 4;
    ftCaptainSpecialHiCatchProcPhysics(mock_gobj);
    CHECK(fabsf(DObjGetStruct(mock_gobj)->translate.vec.f.x) < 1e-5f);

    /* the throw: the catch anim runs out, the caught body's THROW flag
     * goes up, the catcher enters Throw and lets go (catch_gobj NULL,
     * immune mask cleared); the caught side's next ProcPhysics sees the
     * flag and releases through ftCommonThrownReleaseThrownUpdateStats,
     * leaving CaptureCaptain */
    /* the release reads the catcher's throw_desc, which the game's catch
     * animation script sets (ftcommoncatch's throw command); the mock's
     * scripts set none, so hand it test_grab's descriptor */
    fp.throw_desc = &mock_throw_desc;
    mock_gobj->anim_frame = 0.0f;
    ftCaptainSpecialHiCatchProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCaptainStatusSpecialHiThrow);
    CHECK(fp.catch_gobj == NULL);
    CHECK(fp.capture_immune_mask == FTCATCHKIND_MASK_NONE);
    CHECK(fp2.status_vars.common.capturecaptain.capture_flag & FTCOMMON_CAPTURECAPTAIN_MASK_THROW);
    CHECK(fp2.status_id == nFTCommonStatusCaptureCaptain);
    ftCommonCaptureCaptainProcPhysics(mock_gobj2);
    CHECK(fp2.status_id != nFTCommonStatusCaptureCaptain);
    CHECK(fp2.capture_gobj == NULL);
    CHECK(fp2.capture_immune_mask == FTCATCHKIND_MASK_NONE);
    CHECK(fp2.percent_damage == mock_throw_desc.damage);

    fp.throw_desc = NULL;
    fp.joints[29] = saved_j29;
    fp.fkind = nFTKindMario;
}

/* Yoshi's Neutral-B (Egg Lay), both sides. ftyoshispecialn.c
 * and ftcommoncaptureyoshi.c are compiled unmodified, so what is under test
 * is the eight status rows, the demux slot, and the handoff between the two
 * files -- which runs through a three-value `stage` word and two motion
 * flags, and is the most indirect connect in the port. The egg's own effect
 * is stubbed to NULL (unmodeled, see src/dc/ftcommon.c), so the escape path
 * exercised here is the one that runs WITHOUT it, which is the path the
 * target takes too. */
extern void (*dFTCommonSpecialNStatusList[])(GObj*);

static void test_yoshi_egg_lay(void)
{
    ftCommonYoshiEggDesc *egg;
    float x0;

    /* ---- the demux slot, and the decomp quirk beside it ---- */
    CHECK(dFTCommonSpecialNStatusList[nFTKindYoshi] == ftYoshiSpecialNSetStatus);
    /* NYoshi (20) is Mario's fireball in the game's own table -- the decomp
     * marks it "un bro momento". Kept, not corrected. */
    CHECK(dFTCommonSpecialNStatusList[nFTKindNYoshi] == ftMarioSpecialNSetStatus);

    /* ---- the six rows, 228-233: two mirrored threes ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(0, 0, 0, 0, 0);
    fp.fkind = nFTKindYoshi;

    ftMainSetStatus(mock_gobj, nFTYoshiStatusSpecialN, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTYoshiMotionSpecialN);
    CHECK(fp.proc_update == ftAnimEndSetWait);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == ftYoshiSpecialNProcMap);
    CHECK(fp.stat_flags.ga == nMPKineticsGround);
    CHECK(fp.motion_attack_id == nFTMotionAttackIDSpecialN);
    CHECK(!fp.stat_flags.is_smash_attack);

    ftMainSetStatus(mock_gobj, nFTYoshiStatusSpecialNCatch, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTYoshiMotionSpecialNCatch);
    CHECK(fp.proc_update == ftYoshiSpecialNCatchProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == ftYoshiSpecialNCatchProcMap);

    ftMainSetStatus(mock_gobj, nFTYoshiStatusSpecialNRelease, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTYoshiMotionSpecialNRelease);
    CHECK(fp.proc_update == ftYoshiSpecialNReleaseProcUpdate);
    CHECK(fp.proc_map == ftYoshiSpecialNReleaseProcMap);

    /* the air three differ in kinetics, in the physics leaf, and in the
     * plain row's update -- Fall, not Wait.
     *
     * NOTE the Wait in between, and it is not tidying: ftMainSetStatus
     * reloads sflags only when the incoming row's status attack id is
     * None or differs from the one the fighter already carries
     * (ft/ftmain.c:4589). All six of these rows are nFTStatusAttackID-
     * SpecialN, so going straight from the ground row to the air one
     * leaves stat_flags.ga reading Ground -- the game's staling rule, not
     * a stale row. Wait's attack id is None, which always reloads. */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMainSetStatus(mock_gobj, nFTYoshiStatusSpecialAirN, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTYoshiMotionSpecialAirN);
    CHECK(fp.proc_update == ftAnimEndSetFall);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelFriction);
    CHECK(fp.proc_map == ftYoshiSpecialAirNProcMap);
    CHECK(fp.stat_flags.ga == nMPKineticsAir);

    ftMainSetStatus(mock_gobj, nFTYoshiStatusSpecialAirNCatch, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.proc_update == ftYoshiSpecialAirNCatchProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelFriction);
    ftMainSetStatus(mock_gobj, nFTYoshiStatusSpecialAirNRelease, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.proc_update == ftYoshiSpecialAirNReleaseProcUpdate);
    CHECK(fp.proc_map == ftYoshiSpecialAirNReleaseProcMap);

    /* ---- the two common rows, 177 and 178 ---- */
    ftMainSetStatus(mock_gobj, nFTCommonStatusCaptureYoshi, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    /* the swallow borrows the ordinary pulled grab's motion */
    CHECK(fp.motion_id == nFTCommonMotionCapturePulled);
    CHECK(fp.proc_update == NULL);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftCommonCaptureYoshiProcPhysics);
    CHECK(fp.proc_map == mpCommonUpdateFighterProjectFloor);

    ftMainSetStatus(mock_gobj, nFTCommonStatusYoshiEgg, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionYoshiEgg);
    CHECK(fp.proc_update == ftCommonYoshiEggProcUpdate);
    CHECK(fp.proc_interrupt == ftCommonYoshiEggProcInterrupt);
    CHECK(fp.proc_physics == ftCommonYoshiEggProcPhysics);
    CHECK(fp.proc_map == ftCommonYoshiEggProcMap);

    /* ---- the setter, and the catch params it arms ---- */
    spawn(0.0f, 0.0f);
    spawn_second(30.0f, 0.0f, +1);
    frame(0, 0, 0, 0, 0);
    fp.fkind = nFTKindYoshi;
    fp2.fkind = nFTKindMario;

    fp.motion_vars.flags.flag1 = fp.motion_vars.flags.flag2 = 1;
    ftYoshiSpecialNSetStatus(mock_gobj);
    CHECK_STATUS(nFTYoshiStatusSpecialN);
    /* proc_status is a ONE-SHOT: the setter arms it, ftMainSetStatus calls
     * it and clears it (ft/ftmain.c:4593-4598). So what is left to check
     * is its effect -- the ground hook clears flag1 and leaves flag2 --
     * and that the field is empty again. */
    CHECK(fp.proc_status == NULL);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK(fp.motion_vars.flags.flag2 == 1);
    CHECK(fp.catch_mask == FTCATCHKIND_MASK_YOSHISPECIALN);
    CHECK(fp.proc_catch == ftYoshiSpecialNCatchProcCatch);
    CHECK(fp.proc_capture == ftCommonCaptureYoshiProcCapture);
    /* the air setter arms the air ProcCatch instead, and its own hook
     * clears BOTH flags where the ground one clears only flag1 */
    fp.motion_vars.flags.flag1 = fp.motion_vars.flags.flag2 = 1;
    ftYoshiSpecialAirNSetStatus(mock_gobj);
    CHECK_STATUS(nFTYoshiStatusSpecialAirN);
    CHECK(fp.proc_status == NULL);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK(fp.motion_vars.flags.flag2 == 0);
    CHECK(fp.proc_catch == ftYoshiSpecialAirNCatchProcCatch);
    CHECK(fp.proc_capture == ftCommonCaptureYoshiProcCapture);

    /* ---- the connect, as ftMainProcSearchCatch fires it ---- */
    ftYoshiSpecialNSetStatus(mock_gobj);
    fp.lr = +1;
    fp.search_gobj = mock_gobj2;
    ftYoshiSpecialNCatchProcCatch(mock_gobj);
    CHECK_STATUS(nFTYoshiStatusSpecialNCatch);
    CHECK(fp.catch_gobj == mock_gobj2);
    CHECK(fp.capture_immune_mask == FTCATCHKIND_MASK_ALL);

    ftCommonCaptureYoshiProcCapture(mock_gobj2, mock_gobj);
    CHECK(fp2.status_id == nFTCommonStatusCaptureYoshi);
    CHECK(fp2.capture_gobj == mock_gobj);
    CHECK(fp2.lr == -fp.lr);
    CHECK(fp2.ga == nMPKineticsAir);      /* the swallow takes the body up */
    CHECK(fp2.capture_immune_mask == FTCATCHKIND_MASK_ALL);
    CHECK(fp2.is_catch_or_capture == FALSE);
    CHECK(fp2.status_vars.common.captureyoshi.stage == 0);

    /* ---- the handoff: three stages, driven by the CATCHER's two motion
     * flags and read by the CAUGHT fighter's physics proc. Stage 0 is the
     * swallow travelling; the catcher's flag2 moves it to 1; the caught
     * side's own physics proc turns 1 into 2 and hides the body; the
     * catcher's flag1 moves it to 3 and drops the catch; and the next
     * physics tic at stage 3 lays the egg. ---- */
    fp.motion_vars.flags.flag2 = 1;
    ftYoshiSpecialNCatchUpdateCaptureVars(&fp);
    CHECK(fp2.status_vars.common.captureyoshi.stage == 1);
    CHECK(fp.motion_vars.flags.flag2 == 0);   /* consumed */

    ftCommonCaptureYoshiProcPhysics(mock_gobj2);
    CHECK(fp2.status_vars.common.captureyoshi.stage == 2);
    CHECK(fp2.is_invisible == TRUE);
    CHECK(fp2.is_shadow_hide == TRUE);

    fp.motion_vars.flags.flag1 = 1;
    ftYoshiSpecialNCatchUpdateCaptureVars(&fp);
    CHECK(fp2.status_vars.common.captureyoshi.stage == 3);
    CHECK(fp.catch_gobj == NULL);
    CHECK(fp.capture_immune_mask == FTCATCHKIND_MASK_NONE);

    /* the egg, laid */
    x0 = DObjGetStruct(mock_gobj)->translate.vec.f.x;
    ftCommonCaptureYoshiProcPhysics(mock_gobj2);
    CHECK(fp2.status_id == nFTCommonStatusYoshiEgg);
    CHECK(fp2.motion_id == nFTCommonMotionYoshiEgg);
    CHECK(fp2.is_invisible == TRUE);
    CHECK(fp2.capture_gobj == NULL);
    CHECK(fp2.proc_trap == ftCommonYoshiEggProcTrap);
    CHECK(fp2.ga == nMPKineticsAir);
    CHECK_NEAR(fp2.damage_mul, FTCOMMON_YOSHIEGG_DAMAGE_MUL, 1e-4f);
    CHECK(fp2.breakout_wait == FTCOMMON_YOSHIEGG_BREAKOUT_INPUTS_MIN);
    CHECK(fp2.status_vars.common.captureyoshi.lr == fp.lr);
    CHECK(fp2.status_vars.common.captureyoshi.effect_gobj == NULL);
    CHECK(!fp2.status_vars.common.captureyoshi.is_damagefloor);
    /* laid behind Yoshi and above him, moving back and up */
    CHECK_NEAR(DObjGetStruct(mock_gobj2)->translate.vec.f.x,
               x0 - fp.lr * FTCOMMON_YOSHIEGG_LAY_OFF_X, 1e-3f);
    CHECK_NEAR(fp2.physics.vel_air.x, -fp.lr * FTCOMMON_YOSHIEGG_LAY_VEL_X, 1e-3f);
    CHECK_NEAR(fp2.physics.vel_air.y, FTCOMMON_YOSHIEGG_LAY_VEL_Y, 1e-3f);
    /* five points of damage, charged through Kirby's helper (the game's
     * own joke -- ftKirbySpecialNApplyCaptureDamage, ported for this) */
    CHECK(fp2.percent_damage == 5);

    /* the egg's damage collision is the per-victim row of the file's own
     * 27-entry table: Mario's is 157 up, 180 across */
    egg = &dFTCommonYoshiEggDamageCollDescs[nFTKindMario];
    CHECK_NEAR(egg->offset.y, 157.0f, 1e-3f);
    CHECK_NEAR(egg->size.x, 180.0f, 1e-3f);
    CHECK_NEAR(fp2.damage_colls[0].offset.y, egg->offset.y, 1e-3f);
    CHECK_NEAR(fp2.damage_colls[0].size.x, egg->size.x, 1e-3f);
    CHECK(fp2.damage_colls[0].joint_id == nFTPartsJointTopN);
    CHECK(fp2.damage_colls[0].is_grabbable == FALSE);
    CHECK(fp2.is_hitstatus_nodamage == TRUE);
    CHECK(fp2.is_damage_coll_modify == TRUE);

    /* ---- the escape, on the path the port actually takes: no effect
     * GObj, so the physics proc's own counter is what breaks the egg.
     * Stage one, mashing not required: captureyoshi.breakout_wait counts
     * down to zero, flag0 goes up and the counter reloads with the escape
     * wait; stage two, the update proc counts THAT down and the fighter
     * comes out into Fall. ---- */
    fp2.ga = nMPKineticsGround;
    fp2.motion_vars.flags.flag0 = 0;
    fp2.status_vars.common.captureyoshi.breakout_wait = 2;
    ftCommonYoshiEggProcPhysics(mock_gobj2);
    CHECK(fp2.motion_vars.flags.flag0 == 0);        /* 2 -> 1, not yet */
    ftCommonYoshiEggProcPhysics(mock_gobj2);
    CHECK(fp2.motion_vars.flags.flag0 == 0);        /* 1 -> 0, still not */
    ftCommonYoshiEggProcPhysics(mock_gobj2);
    CHECK(fp2.motion_vars.flags.flag0 == 1);
    CHECK(fp2.status_vars.common.captureyoshi.breakout_wait ==
          FTCOMMON_YOSHIEGG_ESCAPE_WAIT_DEFAULT);

    /* the update proc runs that second counter down and cracks the egg */
    fp2.status_vars.common.captureyoshi.breakout_wait = 1;
    ftCommonYoshiEggProcUpdate(mock_gobj2);
    CHECK(fp2.status_id == nFTCommonStatusYoshiEgg);   /* 1 -> 0, one more */
    ftCommonYoshiEggProcUpdate(mock_gobj2);
    CHECK(fp2.status_id == nFTCommonStatusFall);
    CHECK(fp2.ga == nMPKineticsAir);
    CHECK_NEAR(fp2.physics.vel_air.y, FTCOMMON_YOSHIEGG_ESCAPE_VEL_Y, 1e-3f);
    CHECK_NEAR(fp2.physics.vel_air.x, 0.0f, 1e-3f);

    /* ---- ProcTrap: damage shortens the wait, and acid ends it outright.
     * The acid arm is the one that sets is_damagefloor, which is the only
     * way out of the egg that does not need either counter. ---- */
    fp2.status_id = nFTCommonStatusYoshiEgg;
    fp2.motion_vars.flags.flag0 = 0;
    fp2.status_vars.common.captureyoshi.breakout_wait = 100;
    fp2.status_vars.common.captureyoshi.is_damagefloor = FALSE;
    fp2.damage_queue = 10;
    fp2.damage_object_class = nFTHitLogObjectFighter;
    ftCommonYoshiEggProcTrap(mock_gobj2);
    /* 100 - (2 * 10) / 0.5 == 60 */
    CHECK(fp2.status_vars.common.captureyoshi.breakout_wait == 60);
    CHECK(!fp2.status_vars.common.captureyoshi.is_damagefloor);
    CHECK(fp2.damage_kind == nFTHitLogObjectGround);

    fp2.damage_object_class = nFTHitLogObjectGround;
    fp2.damage_object_kind = nGMHitEnvironmentAcid;
    ftCommonYoshiEggProcTrap(mock_gobj2);
    CHECK(fp2.status_vars.common.captureyoshi.breakout_wait == 0);
    CHECK(fp2.status_vars.common.captureyoshi.is_damagefloor == TRUE);
    /* and is_damagefloor is the update proc's first branch: straight out */
    ftMainSetStatus(mock_gobj2, nFTCommonStatusYoshiEgg, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    fp2.status_vars.common.captureyoshi.is_damagefloor = TRUE;
    ftCommonYoshiEggProcUpdate(mock_gobj2);
    CHECK(fp2.status_id == nFTCommonStatusFall);

    /* ---- the release, back on the catcher ---- */
    ftYoshiSpecialNReleaseSetStatus(mock_gobj);
    CHECK_STATUS(nFTYoshiStatusSpecialNRelease);
    CHECK(fp.capture_immune_mask == FTCATCHKIND_MASK_ALL);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK(fp.motion_vars.flags.flag2 == 0);
    ftYoshiSpecialAirNReleaseSetStatus(mock_gobj);
    CHECK_STATUS(nFTYoshiStatusSpecialAirNRelease);

    /* and the Catch update, which is what reaches the release: it waits
     * for flag1 with a live catch, or for the animation to run out */
    ftYoshiSpecialNSetStatus(mock_gobj);
    ftYoshiSpecialNCatchProcCatch(mock_gobj);
    mock_gobj->anim_frame = 5.0f;
    fp.catch_gobj = mock_gobj2;
    fp.motion_vars.flags.flag1 = 0;
    ftYoshiSpecialNCatchProcUpdate(mock_gobj);
    CHECK_STATUS(nFTYoshiStatusSpecialNCatch);      /* nothing to go on */
    fp.motion_vars.flags.flag1 = 1;
    ftYoshiSpecialNCatchProcUpdate(mock_gobj);
    CHECK_STATUS(nFTYoshiStatusSpecialNRelease);
    /* the animation running out does it too, with no flag and no catch */
    ftYoshiSpecialNSetStatus(mock_gobj);
    ftYoshiSpecialNCatchProcCatch(mock_gobj);
    fp.catch_gobj = NULL;
    fp.motion_vars.flags.flag1 = 0;
    mock_gobj->anim_frame = 0.0f;
    ftYoshiSpecialNCatchProcUpdate(mock_gobj);
    CHECK_STATUS(nFTYoshiStatusSpecialNRelease);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindMario;
}

/* Yoshi's Up-B (Egg Throw), the last of his three specials.
 * ftyoshispecialhi.c and wp/wpyoshi/wpyoshieggthrow.c are compiled
 * unmodified, so what is under test is the two status rows, the four demux
 * slots, and the charge-and-throw state machine those procs drive. The egg
 * itself is unmodeled -- wpManagerMakeWeapon returns NULL -- so egg_gobj
 * stays NULL throughout, which is exactly the path the target takes; the
 * decomp guards every use of it. */
static void test_yoshi_egg_throw(void)
{
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);
    DObj *saved_yrotn;
    int i;

    /* ---- the four demux slots. The Up-B table is the one that does NOT
     * route NYoshi to Mario, unlike the Neutral-B pair. ---- */
    CHECK(dFTCommonSpecialHiStatusList[nFTKindYoshi] == ftYoshiSpecialHiSetStatus);
    CHECK(dFTCommonSpecialHiStatusList[nFTKindNYoshi] == ftYoshiSpecialHiSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindYoshi] == ftYoshiSpecialAirHiSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindNYoshi] == ftYoshiSpecialAirHiSetStatus);
    /* with these four Yoshi is table-complete: all twelve of his slots
     * across the six lists now name one of his own setters, except the two
     * the game itself routes to Mario */
    CHECK(dFTCommonSpecialNStatusList[nFTKindYoshi] == ftYoshiSpecialNSetStatus);
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindYoshi] == ftYoshiSpecialAirNSetStatus);
    /* the ground Down-B list is static in ftcommon.c, so only the aerial
     * one is reachable from here; the ground slot is covered elsewhere */
    CHECK(dFTCommonSpecialAirLwStatusList[nFTKindYoshi] == ftYoshiSpecialAirLwStartSetStatus);

    /* ---- the two rows, 222 and 223. They are the only pair of Yoshi's
     * that is is_projectile, and they share no proc at all. ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(0, 0, 0, 0, 0);
    fp.fkind = nFTKindYoshi;

    ftMainSetStatus(mock_gobj, nFTYoshiStatusSpecialHi, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTYoshiMotionSpecialHi);
    CHECK(fp.proc_update == ftYoshiSpecialHiProcUpdate);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftYoshiSpecialHiProcPhysics);
    CHECK(fp.proc_map == ftYoshiSpecialHiProcMap);
    CHECK(fp.stat_flags.ga == nMPKineticsGround);
    CHECK(fp.stat_flags.is_projectile == TRUE);
    CHECK(fp.stat_flags.attack_id == nFTStatusAttackIDSpecialHi);

    /* a None-id status between the two, or sflags do not reload -- both
     * rows carry nFTStatusAttackIDSpecialHi (ft/ftmain.c:4589) */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMainSetStatus(mock_gobj, nFTYoshiStatusSpecialAirHi, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTYoshiMotionSpecialAirHi);
    CHECK(fp.proc_update == ftYoshiSpecialAirHiProcUpdate);
    CHECK(fp.proc_physics == ftYoshiSpecialAirHiProcPhysics);
    CHECK(fp.proc_map == ftYoshiSpecialAirHiProcMap);
    CHECK(fp.stat_flags.ga == nMPKineticsAir);
    CHECK(fp.stat_flags.is_projectile == TRUE);

    /* ---- the setters, and the vars they clear ---- */
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    fp.fkind = nFTKindYoshi;
    fp.motion_vars.flags.flag1 = fp.motion_vars.flags.flag2 = 1;
    fp.status_vars.yoshi.specialhi.throw_force = 99;
    ftYoshiSpecialHiSetStatus(mock_gobj);
    CHECK(fp.status_id == nFTYoshiStatusSpecialHi);
    CHECK(fp.proc_damage == ftYoshiSpecialHiProcDamage);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK(fp.motion_vars.flags.flag2 == 0);
    CHECK(fp.status_vars.yoshi.specialhi.egg_gobj == NULL);
    CHECK(fp.status_vars.yoshi.specialhi.throw_force == 0);

    ftYoshiSpecialAirHiSetStatus(mock_gobj);
    CHECK(fp.status_id == nFTYoshiStatusSpecialAirHi);
    CHECK(fp.proc_damage == ftYoshiSpecialHiProcDamage);

    /* ---- the charge: B held counts throw_force up, one per tic, and
     * nothing else does. This is the whole of the move's timing. ---- */
    ftYoshiSpecialHiSetStatus(mock_gobj);
    fp.input.pl.button_hold = 0;
    for (i = 0; i < 5; i++)
        ftYoshiSpecialHiUpdateEggThrowForce(mock_gobj);
    CHECK(fp.status_vars.yoshi.specialhi.throw_force == 0);
    fp.input.pl.button_hold = fp.input.button_mask_b;
    for (i = 0; i < 5; i++)
        ftYoshiSpecialHiUpdateEggThrowForce(mock_gobj);
    CHECK(fp.status_vars.yoshi.specialhi.throw_force == 5);
    fp.input.pl.button_hold = 0;

    /* The egg hangs off YRotN (ft/ftchar/ftyoshi/ftyoshi.h:12), and
     * ftYoshiSpecialHiGetEggPosition reads that joint's world position
     * unconditionally. The mock's tree is three DObjs and YRotN is
     * deliberately NULL in it (the shield's own tests assert that), so
     * alias it to TopN for the beats below the way test_captain_falcon_dive
     * aliases joint 29. */
    saved_yrotn = fp.joints[nFTPartsJointYRotN];
    fp.joints[nFTPartsJointYRotN] = fp.joints[nFTPartsJointTopN];

    /* ---- the egg's two motion-script beats, flag2 == 1 then 2. Both are
     * consumed on read. The egg is unmodeled, so the make returns NULL and
     * the throw beat finds nothing to throw -- but it still raises flag1,
     * which is what the air map proc reads to catch a ledge. ---- */
    fp.motion_vars.flags.flag2 = 1;             /* the script says "make" */
    ftYoshiSpecialHiUpdateEggVars(mock_gobj);
    CHECK(fp.motion_vars.flags.flag2 == 0);     /* consumed */
    CHECK(fp.status_vars.yoshi.specialhi.egg_gobj == NULL);   /* unmodeled */
    CHECK(fp.motion_vars.flags.flag1 == 0);     /* not the throw beat */

    fp.motion_vars.flags.flag2 = 2;             /* the script says "throw" */
    ftYoshiSpecialHiUpdateEggVars(mock_gobj);
    CHECK(fp.motion_vars.flags.flag2 == 0);
    CHECK(fp.motion_vars.flags.flag1 == 1);     /* raised whether or not
                                                 * there was an egg */
    /* neither beat, nothing happens */
    fp.motion_vars.flags.flag1 = 0;
    ftYoshiSpecialHiUpdateEggVars(mock_gobj);
    CHECK(fp.motion_vars.flags.flag1 == 0);

    /* the vector update and the damage hook are both no-ops with no egg,
     * which is the point of checking them: the port takes this path every
     * frame of the move */
    ftYoshiSpecialHiUpdateEggVectors(&fp);
    ftYoshiSpecialHiProcDamage(mock_gobj);
    CHECK(fp.status_vars.yoshi.specialhi.egg_gobj == NULL);
    fp.joints[nFTPartsJointYRotN] = saved_yrotn;

    /* ---- the two updates end differently: the ground throw into Wait,
     * the air throw into Fall. That is Yoshi's recovery. ---- */
    ftYoshiSpecialHiSetStatus(mock_gobj);
    mock_gobj->anim_frame = 0.0f;
    ftYoshiSpecialHiProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusWait);

    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    fp.fkind = nFTKindYoshi;
    ftYoshiSpecialAirHiSetStatus(mock_gobj);
    fp.ga = nMPKineticsAir;
    mock_gobj->anim_frame = 0.0f;
    ftYoshiSpecialAirHiProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusFall);
    mock_anim_len = 1e9f;

    /* ---- the ground/air swap, both ways. Each re-arms proc_damage, which
     * ftMainSetStatus does not carry across. ---- */
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    fp.fkind = nFTKindYoshi;
    mock_anim_len = 1e9f;
    ftYoshiSpecialHiSetStatus(mock_gobj);
    fp.proc_damage = NULL;
    ftYoshiSpecialHiSwitchStatusAir(mock_gobj);
    CHECK(fp.status_id == nFTYoshiStatusSpecialAirHi);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(fp.proc_damage == ftYoshiSpecialHiProcDamage);

    fp.proc_damage = NULL;
    ftYoshiSpecialAirHiSwitchStatusGround(mock_gobj);
    CHECK(fp.status_id == nFTYoshiStatusSpecialHi);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK(fp.proc_damage == ftYoshiSpecialHiProcDamage);

    /* ---- the demux from the input side: a stick-up B-tap on a Yoshi
     * reaches his own setter through the ground cascade, with no direct
     * call. This is what the four slots above are for. ---- */
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    fp.fkind = nFTKindYoshi;
    fp.attr->is_have_specialhi = TRUE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialHiCheckInterruptCommon(mock_gobj) != FALSE);
    CHECK(fp.status_id == nFTYoshiStatusSpecialHi);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindMario;
}

/* Kirby's Up-B (Final Cutter). ftkirbyspecialhi.c and
 * wp/wpkirby/wpkirbycutter.c compile unmodified, so what is under test is
 * the four status rows, the four demux slots, and the effect switch the
 * update drives -- which is where the port and the game part company,
 * because all four cutter effects are stubbed to NULL and the blade is
 * unmodeled. */
static void test_kirby_final_cutter(void)
{
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);

    /* ---- the four demux slots ---- */
    CHECK(dFTCommonSpecialHiStatusList[nFTKindKirby] == ftKirbySpecialHiSetStatus);
    CHECK(dFTCommonSpecialHiStatusList[nFTKindNKirby] == ftKirbySpecialHiSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindKirby] == ftKirbySpecialAirHiSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindNKirby] == ftKirbySpecialAirHiSetStatus);
    /* his Neutral-B was the one hole in his table when this test was
     * written; it is filled now, and it does NOT go to a setter --
     * it goes to the copy demux, which picks by what Kirby swallowed.
     * See test_kirby_inhale below. */
    CHECK(dFTCommonSpecialNStatusList[nFTKindKirby] == ftKirbySpecialNSetStatusSelect);
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindKirby] == ftKirbySpecialAirNSetStatusSelect);

    /* ---- the four rows, 256-259. All four are is_projectile. ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(0, 0, 0, 0, 0);
    fp.fkind = nFTKindKirby;

    ftMainSetStatus(mock_gobj, nFTKirbyStatusSpecialHi, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTKirbyMotionSpecialHi);
    CHECK(fp.proc_update == ftKirbySpecialHiProcUpdate);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftKirbySpecialHiProcPhysics);
    CHECK(fp.proc_map == ftKirbySpecialHiProcMap);
    CHECK(fp.stat_flags.ga == nMPKineticsGround);
    CHECK(fp.stat_flags.is_projectile == TRUE);
    CHECK(fp.stat_flags.attack_id == nFTStatusAttackIDSpecialHi);

    /* a None-id status between each pair, or sflags do not reload: all four
     * rows carry nFTStatusAttackIDSpecialHi (ft/ftmain.c:4589) */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMainSetStatus(mock_gobj, nFTKirbyStatusSpecialHiLanding, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTKirbyMotionSpecialHiLanding);
    CHECK(fp.proc_update == ftKirbySpecialHiLandingProcUpdate);
    CHECK(fp.proc_physics == ftKirbySpecialHiLandingProcPhysics);
    CHECK(fp.proc_map == mpCommonSetFighterFallOnGroundBreak);
    CHECK(fp.stat_flags.ga == nMPKineticsGround);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMainSetStatus(mock_gobj, nFTKirbyStatusSpecialAirHi, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTKirbyMotionSpecialAirHi);
    /* the rise shares its update and map with the ground row -- it is the
     * same move whichever way it began -- and differs only in the physics */
    CHECK(fp.proc_update == ftKirbySpecialHiProcUpdate);
    CHECK(fp.proc_map == ftKirbySpecialHiProcMap);
    CHECK(fp.proc_physics == ftKirbySpecialAirHiProcPhysics);
    CHECK(fp.stat_flags.ga == nMPKineticsAir);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMainSetStatus(mock_gobj, nFTKirbyStatusSpecialAirHiFall, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTKirbyMotionSpecialAirHiFall);
    CHECK(fp.proc_update == NULL);            /* the only one with none */
    CHECK(fp.proc_physics == ftKirbySpecialAirHiFallProcPhysics);
    CHECK(fp.proc_map == ftKirbySpecialAirHiFallProcMap);
    CHECK(fp.stat_flags.ga == nMPKineticsAir);
    CHECK(fp.stat_flags.is_projectile == TRUE);

    /* ---- the setters. All four install the effect pause/resume pair; the
     * two that begin the move also arm a proc_status that zeroes the three
     * motion flags, and proc_status is a one-shot, so check its effect. ---- */
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    fp.fkind = nFTKindKirby;
    fp.motion_vars.flags.flag0 = fp.motion_vars.flags.flag1 =
        fp.motion_vars.flags.flag2 = 1;
    ftKirbySpecialHiSetStatus(mock_gobj);
    CHECK(fp.status_id == nFTKirbyStatusSpecialHi);
    CHECK(fp.proc_status == NULL);
    CHECK(fp.motion_vars.flags.flag0 == 0);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK(fp.motion_vars.flags.flag2 == 0);
    CHECK(fp.proc_lagstart == ftParamProcPauseEffect);
    CHECK(fp.proc_lagend == ftParamProcResumeEffect);

    fp.motion_vars.flags.flag1 = fp.motion_vars.flags.flag2 = 1;
    ftKirbySpecialAirHiSetStatus(mock_gobj);
    CHECK(fp.status_id == nFTKirbyStatusSpecialAirHi);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK(fp.motion_vars.flags.flag2 == 0);

    /* the landing setter does NOT clear the flags -- it has no proc_status
     * -- because it continues a move already in flight */
    fp.motion_vars.flags.flag2 = 3;
    ftKirbySpecialHiLandingSetStatus(mock_gobj);
    CHECK(fp.status_id == nFTKirbyStatusSpecialHiLanding);
    CHECK(fp.motion_vars.flags.flag2 == 3);
    CHECK(fp.proc_lagstart == ftParamProcPauseEffect);
    fp.motion_vars.flags.flag2 = 0;

    /* the fall setter preserves vel_air.y across the status change and
     * spends every jump -- there is no double jump out of a Final Cutter */
    fp.physics.vel_air.y = -12.5f;
    fp.jumps_used = 0;
    ftKirbySpecialAirHiFallSetStatus(mock_gobj);
    CHECK(fp.status_id == nFTKirbyStatusSpecialAirHiFall);
    CHECK_NEAR(fp.physics.vel_air.y, -12.5f, 1e-4f);
    CHECK(fp.jumps_used == fp.attr->jumps_max);

    /* ---- the effect switch, which is where the port diverges. flag2 case
     * 1 is the stop, and it clears only when an effect is attached; cases 2
     * to 5 are the four makers, all stubbed to NULL here, so the flag stays
     * raised and the same case is asked for again next frame. That is the
     * path the target takes too. ---- */
    ftKirbySpecialAirHiSetStatus(mock_gobj);
    fp.is_effect_attach = FALSE;
    fp.motion_vars.flags.flag2 = 1;
    ftKirbySpecialHiUpdateEffect(mock_gobj);
    CHECK(fp.motion_vars.flags.flag2 == 1);    /* nothing attached to stop */
    fp.is_effect_attach = TRUE;
    ftKirbySpecialHiUpdateEffect(mock_gobj);
    CHECK(fp.motion_vars.flags.flag2 == 0);    /* stopped, and cleared */

    for (fp.motion_vars.flags.flag2 = 2; fp.motion_vars.flags.flag2 <= 5;)
    {
        u8 asked = fp.motion_vars.flags.flag2;

        fp.is_effect_attach = FALSE;
        ftKirbySpecialHiUpdateEffect(mock_gobj);
        /* the maker said no, so the request stands and is_effect_attach
         * stays down -- both are what the game would have changed */
        CHECK(fp.motion_vars.flags.flag2 == asked);
        CHECK(fp.is_effect_attach == FALSE);
        fp.motion_vars.flags.flag2 = asked + 1;
    }
    fp.motion_vars.flags.flag2 = 0;

    /* flag1's own switch takes only 0 and 1 -- 1 is a second stop, and
     * anything above it reaches the game's assert-loop, which never
     * returns. Only the reachable half is exercised here. */
    fp.is_effect_attach = TRUE;
    fp.motion_vars.flags.flag1 = 1;
    ftKirbySpecialHiUpdateEffect(mock_gobj);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    fp.is_effect_attach = FALSE;

    /* ---- the landing, which is what asks for the blade. flag0 is the
     * motion script's one beat; it clears whether or not a weapon comes
     * back, so an unmodeled cutter costs the shockwave and nothing else.
     * The spawn joint is TopN (joint 0), which the mock has. ---- */
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    fp.fkind = nFTKindKirby;
    mock_anim_len = 1e9f;
    ftKirbySpecialHiLandingSetStatus(mock_gobj);
    fp.lr = +1;
    fp.motion_vars.flags.flag0 = 1;
    ftKirbySpecialHiLandingProcUpdate(mock_gobj);
    CHECK(fp.motion_vars.flags.flag0 == 0);
    CHECK_STATUS(nFTKirbyStatusSpecialHiLanding);
    /* and it ends into Wait when the animation runs out */
    mock_gobj->anim_frame = 0.0f;
    ftKirbySpecialHiLandingProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusWait);
}

/* Kirby's Neutral-B, the copy inhale. Three decomp files
 * compile unmodified (ftkirbyspecialn.c, ftcommoncapturekirby.c and the
 * knockback leaf ftcommoncapture.c), so what is under test is the
 * eighteen Kirby rows, the four common rows the swallowed fighter lands
 * in, and the thing this move has that no other special does: a SECOND
 * demux, keyed on what Kirby last ate. */
static void test_kirby_inhale(void)
{
    extern void (*dFTKirbySpecialNStatusList[])(GObj*);
    extern void (*dFTKirbySpecialAirNStatusList[])(GObj*);
    extern void ftKirbyMainMotionBindOffsets(void);
    extern int llKirbyMainMotionSpecialNFTKirbyCopy;
    extern int llKirbyMainMotionftKirbyAttack100Effect;
    int i;

    /* ---- Kirby's motion file, read the way the decomp reads it
     * (ftkirbyspecialn.c:113, ftcommonattack100.c:91): the copy table and
     * the rapid-jab sparks at their own offsets, not the .bss after an
     * `int` stand-in (src/dc/ftcommon.c ftKirbyMainMotionBindOffsets) ---- */
    ftKirbyMainMotionBindOffsets();
    {
        FTKirbyCopy *copy = lbRelocGetFileData(FTKirbyCopy*,
            gFTDataKirbyMainMotion, &llKirbyMainMotionSpecialNFTKirbyCopy);
        ftKirbyAttack100Effect *effect = (ftKirbyAttack100Effect*)
            ((uintptr_t)gFTDataKirbyMainMotion +
             (intptr_t)&llKirbyMainMotionftKirbyAttack100Effect);

        CHECK((intptr_t)&llKirbyMainMotionftKirbyAttack100Effect == 0x1220);
        CHECK(copy[nFTKindMario].copy_modelpart_id == 12);
        CHECK(copy[nFTKindNess].copy_id == nFTKindNess);
        CHECK(copy[nFTKindNess].copy_modelpart_id == 13);
        CHECK(copy[nFTKindDonkey].star_damage == 30);
        CHECK(copy[nFTKindGDonkey].star_damage == 50);
        CHECK_EQF(copy[nFTKindYoshi].effect_scale, 1.7F);
        CHECK_EQF(effect[0].offset.y, 200.0F);
        CHECK_EQF(effect[1].rotate, 19.0F);
        CHECK_EQF(effect[5].add, -30.0F);
    }

    /* ---- the outer demux sends Kirby to the SELECTOR, not to a setter ---- */
    CHECK(dFTCommonSpecialNStatusList[nFTKindKirby] == ftKirbySpecialNSetStatusSelect);
    CHECK(dFTCommonSpecialNStatusList[nFTKindNKirby] == ftKirbySpecialNSetStatusSelect);
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindKirby] == ftKirbySpecialAirNSetStatusSelect);
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindNKirby] == ftKirbySpecialAirNSetStatusSelect);

    /* ---- the inner demux. Kirby's own mouth and every polygon are live
     * (a polygon has no special to copy, so the decomp gives them the
     * inhale); Purin's is live because ftkirbycopypurinspecialn.c is
     * compiled; the other eight are still NULL. ---- */
    CHECK(dFTKirbySpecialNStatusList[nFTKindKirby] == ftKirbySpecialNStartSetStatus);
    CHECK(dFTKirbySpecialNStatusList[nFTKindBoss] == ftKirbySpecialNStartSetStatus);
    CHECK(dFTKirbySpecialAirNStatusList[nFTKindKirby] == ftKirbySpecialAirNStartSetStatus);
    CHECK(dFTKirbySpecialNStatusList[nFTKindPurin] == ftKirbyCopyPurinSpecialNSetStatus);
    CHECK(dFTKirbySpecialAirNStatusList[nFTKindPurin] == ftKirbyCopyPurinSpecialAirNSetStatus);
    for (i = nFTKindNMario; i <= nFTKindGDonkey; i++)
    {
        CHECK(dFTKirbySpecialNStatusList[i] == ftKirbySpecialNStartSetStatus);
        CHECK(dFTKirbySpecialAirNStatusList[i] == ftKirbySpecialAirNStartSetStatus);
    }
    /* the copy mouths are in test_kirby_copy below */
    CHECK(dFTKirbySpecialNStatusList[nFTKindPikachu] == ftKirbyCopyPikachuSpecialNSetStatus);
    CHECK(dFTKirbySpecialAirNStatusList[nFTKindPikachu] == ftKirbyCopyPikachuSpecialAirNSetStatus);
    CHECK(dFTKirbySpecialNStatusList[nFTKindNess] == ftKirbyCopyNessSpecialNSetStatus);
    CHECK(dFTKirbySpecialAirNStatusList[nFTKindNess] == ftKirbyCopyNessSpecialAirNSetStatus);

    /* ---- the selector reads passive copy_id and calls that mouth ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(0, 0, 0, 0, 0);
    fp.fkind = nFTKindKirby;

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copy_id = nFTKindNess;     /* the last mouth filled (G04) */
    ftKirbySpecialNSetStatusSelect(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyNessSpecialN);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copy_id = nFTKindKirby;    /* his own */
    ftKirbySpecialNSetStatusSelect(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusSpecialNStart);

    /* the start setter also arms the catch and resets the status vars:
     * copy_id back to Kirby (a fresh inhale has copied nothing yet) */
    CHECK(fp.is_catchstatus == TRUE);
    CHECK(fp.catch_mask == FTCATCHKIND_MASK_KIRBYSPECIALN);
    CHECK(fp.proc_catch == ftKirbySpecialNCatchProcCatch);
    CHECK(fp.proc_capture == ftCommonCaptureKirbyProcCapture);
    CHECK(fp.status_vars.kirby.specialn.copy_id == nFTKindKirby);
    CHECK(fp.status_vars.kirby.specialn.release_lag == FTKIRBY_VACUUM_RELEASE_LAG);
    CHECK(fp.motion_vars.flags.flag0 == 0);

    /* in the air it arms the air catch proc instead, and nothing else differs */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.ga = nMPKineticsAir;
    ftKirbySpecialAirNStartSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusSpecialAirNStart);
    CHECK(fp.proc_catch == ftKirbySpecialAirNCatchProcCatch);
    CHECK(fp.proc_capture == ftCommonCaptureKirbyProcCapture);
    fp.ga = nMPKineticsGround;

    /* ---- a walk of the rows that carry something worth pinning. The
     * ground and air nines are the same nine, so several share a proc.
     *
     * Note the Wait before the FIRST row as well as between the pairs:
     * the air start setter just above left stat_flags carrying
     * nFTStatusAttackIDSpecialN, and SpecialNStart carries it too, so
     * without a None-id status in between the flags keep the air row's
     * ga and the check below reads nMPKineticsAir. The procs reload
     * either way, which is what makes this one easy to miss. ---- */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMainSetStatus(mock_gobj, nFTKirbyStatusSpecialNStart, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTKirbyMotionSpecialNStart);
    CHECK(fp.proc_update == ftKirbySpecialNStartProcUpdate);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == ftKirbySpecialNStartProcMap);
    CHECK(fp.stat_flags.ga == nMPKineticsGround);
    CHECK(fp.stat_flags.attack_id == nFTStatusAttackIDSpecialN);

    /* every row in the family carries the same status attack id, so a
     * None-id status has to sit between each pair or sflags do not
     * reload (ft/ftmain.c:4589) */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMainSetStatus(mock_gobj, nFTKirbyStatusSpecialNLoop, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.proc_update == ftKirbySpecialNLoopProcUpdate);
    CHECK(fp.proc_interrupt == ftKirbySpecialNLoopProcInterrupt);

    /* the two Catch rows are the odd ones: motion -1, because the catch
     * keeps whatever the mouth was already playing, and the AIR one is
     * nMPKineticsGround -- a fighter being reeled in is not flying */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMainSetStatus(mock_gobj, nFTKirbyStatusSpecialNCatch, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.proc_update == ftKirbySpecialNCatchProcUpdate);
    CHECK(fp.proc_map == ftKirbySpecialNCatchProcMap);
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMainSetStatus(mock_gobj, nFTKirbyStatusSpecialAirNCatch, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.proc_update == ftKirbySpecialNCatchProcUpdate);   /* shared */
    CHECK(fp.proc_map == ftKirbySpecialAirNCatchProcMap);
    CHECK(fp.stat_flags.ga == nMPKineticsGround);              /* not Air */

    /* the two Throws are the spit, and they are the only rows in the
     * eighteen whose physics translates rather than rubs speed off */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMainSetStatus(mock_gobj, nFTKirbyStatusSpecialNThrow, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelTransN);
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMainSetStatus(mock_gobj, nFTKirbyStatusSpecialAirNThrow, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelTransNAll);
    CHECK(fp.stat_flags.ga == nMPKineticsAir);

    /* the two Ends leave the family the two ordinary ways */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMainSetStatus(mock_gobj, nFTKirbyStatusSpecialNEnd, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.proc_update == ftAnimEndSetWait);
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMainSetStatus(mock_gobj, nFTKirbyStatusSpecialAirNEnd, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.proc_update == ftAnimEndSetFall);

    /* ---- the four common rows the SWALLOWED fighter lands in ---- */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMainSetStatus(mock_gobj, nFTCommonStatusCaptureKirby, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionDamageFall);
    CHECK(fp.proc_update == NULL);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftCommonCaptureKirbyProcPhysics);
    CHECK(fp.proc_map == mpCommonUpdateFighterProjectFloor);   /* not a landing */

    ftMainSetStatus(mock_gobj, nFTCommonStatusCaptureWaitKirby, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.proc_interrupt == ftCommonCaptureWaitKirbyProcInterrupt);
    CHECK(fp.proc_map == ftCommonCaptureWaitKirbyProcMap);
    /* the only row in the whole common table with no physics at all: a
     * fighter inside Kirby's mouth is carried, he does not move himself */
    CHECK(fp.proc_physics == NULL);

    ftMainSetStatus(mock_gobj, nFTCommonStatusThrownKirbyStar, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.proc_update == ftCommonThrownKirbyStarProcUpdate);
    CHECK(fp.proc_physics == ftCommonThrownKirbyStarProcPhysics);
    CHECK(fp.proc_map == ftCommonThrownCommonStarProcMap);
    ftMainSetStatus(mock_gobj, nFTCommonStatusThrownCopyStar, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.proc_update == ftCommonThrownCopyStarProcUpdate);
    CHECK(fp.proc_physics == ftCommonThrownCopyStarProcPhysics);
    CHECK(fp.proc_map == ftCommonThrownCommonStarProcMap);     /* shared */

    /* ---- the inhale's wind, which is where the port diverges. The maker
     * is stubbed (Kirby's particle bank is not loaded), and the guard
     * clears flag0 and raises is_effect_attach only INSIDE the if -- so
     * with a NULL maker the request stands and is re-asked every frame,
     * exactly as the target does it. ---- */
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    fp.fkind = nFTKindKirby;
    mock_anim_len = 1e9f;
    fp.is_effect_attach = FALSE;
    fp.motion_vars.flags.flag0 = 1;
    ftKirbySpecialNLoopProcUpdate(mock_gobj);
    CHECK(fp.motion_vars.flags.flag0 == 1);
    CHECK(fp.is_effect_attach == FALSE);
    /* and the guard's other half really does gate on the attach flag */
    fp.is_effect_attach = TRUE;
    ftKirbySpecialNLoopProcUpdate(mock_gobj);
    CHECK(fp.motion_vars.flags.flag0 == 1);
    fp.is_effect_attach = FALSE;
    fp.motion_vars.flags.flag0 = 0;

    /* ---- the mouth opens into the hold when the animation runs out ---- */
    ftKirbySpecialNStartSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusSpecialNStart);
    mock_gobj->anim_frame = 0.0f;
    ftKirbySpecialNStartProcUpdate(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusSpecialNLoop);

    /* ---- the rise ends into the fall, which is the recovery's shape ---- */
    ftKirbySpecialAirHiSetStatus(mock_gobj);
    fp.ga = nMPKineticsAir;
    mock_gobj->anim_frame = 0.0f;
    ftKirbySpecialHiProcUpdate(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusSpecialAirHiFall);

    /* ---- the demux from the input side: a stick-up B-tap on a Kirby ---- */
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    fp.fkind = nFTKindKirby;
    fp.attr->is_have_specialhi = TRUE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialHiCheckInterruptCommon(mock_gobj) != FALSE);
    CHECK(fp.status_id == nFTKirbyStatusSpecialHi);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindMario;
}

/* Kirby's copy half. Six ftkirbycopy*specialn.c files
 * compile unmodified; what is under test is the twenty-nine rows they
 * need, the sixteen demux slots that reach them, and the four decomp
 * facts that are not transcription: Mario's file drives Luigi's statuses
 * too, Samus's start anim speed is her charge level, Donkey's end picks
 * Full over End at max charge, and a swallowed Yoshi gives Kirby YOSHI's
 * catch kind -- the common rows for the egg. */
static void test_kirby_copy(void)
{
    extern void (*dFTKirbySpecialNStatusList[])(GObj*);
    extern void (*dFTKirbySpecialAirNStatusList[])(GObj*);
    extern FTStatusDesc dFTKirbySpecialStatusDescs[];
    extern void ftKirbyMainMotionBindOffsets(void);
    extern int llKirbyMainMotionSpecialNFTKirbyCopy;
    FTStatusDesc *row;
    int i;

    /* ---- the eight filled mouths. Mario's setter serves
     * three of them: Luigi has no copy file of his own, and Metal Mario
     * shares Mario's exactly as he shares Mario's own specials. ---- */
    CHECK(dFTKirbySpecialNStatusList[nFTKindMario] == ftKirbyCopyMarioSpecialNSetStatus);
    CHECK(dFTKirbySpecialNStatusList[nFTKindLuigi] == ftKirbyCopyMarioSpecialNSetStatus);
    CHECK(dFTKirbySpecialNStatusList[nFTKindMMario] == ftKirbyCopyMarioSpecialNSetStatus);
    CHECK(dFTKirbySpecialNStatusList[nFTKindFox] == ftKirbyCopyFoxSpecialNSetStatus);
    CHECK(dFTKirbySpecialNStatusList[nFTKindDonkey] == ftKirbyCopyDonkeySpecialNStartSetStatus);
    CHECK(dFTKirbySpecialNStatusList[nFTKindSamus] == ftKirbyCopySamusSpecialNStartSetStatus);
    CHECK(dFTKirbySpecialNStatusList[nFTKindYoshi] == ftKirbyCopyYoshiSpecialNSetStatus);
    CHECK(dFTKirbySpecialNStatusList[nFTKindCaptain] == ftKirbyCopyCaptainSpecialNSetStatus);

    CHECK(dFTKirbySpecialAirNStatusList[nFTKindMario] == ftKirbyCopyMarioSpecialAirNSetStatus);
    CHECK(dFTKirbySpecialAirNStatusList[nFTKindLuigi] == ftKirbyCopyMarioSpecialAirNSetStatus);
    CHECK(dFTKirbySpecialAirNStatusList[nFTKindMMario] == ftKirbyCopyMarioSpecialAirNSetStatus);
    CHECK(dFTKirbySpecialAirNStatusList[nFTKindFox] == ftKirbyCopyFoxSpecialAirNSetStatus);
    CHECK(dFTKirbySpecialAirNStatusList[nFTKindDonkey] == ftKirbyCopyDonkeySpecialAirNStartSetStatus);
    CHECK(dFTKirbySpecialAirNStatusList[nFTKindSamus] == ftKirbyCopySamusSpecialAirNStartSetStatus);
    CHECK(dFTKirbySpecialAirNStatusList[nFTKindYoshi] == ftKirbyCopyYoshiSpecialAirNSetStatus);
    CHECK(dFTKirbySpecialAirNStatusList[nFTKindCaptain] == ftKirbyCopyCaptainSpecialAirNSetStatus);

    /* ---- the rows. Every status the six files can set has a physics and
     * a map proc; only Ness's two are still zero, and so is the AppearR/L
     * pair, which belongs to no special at all. Link's six (287-292)
     * are in the filled set and checked in test_link_boomerang below;
     * Pikachu's two (252-253) are checked in test_pikachu_jolt. ---- */
    for (i = nFTKirbyStatusCopyMarioSpecialN; i <= nFTKirbyStatusCopyDonkeySpecialAirNFull; i++)
    {
        row = &dFTKirbySpecialStatusDescs[i - nFTCommonStatusSpecialStart];
        CHECK(row->proc_physics != NULL);
        CHECK(row->proc_map != NULL);
    }
    for (i = nFTKirbyStatusCopyPurinSpecialN; i <= nFTKirbyStatusCopyYoshiSpecialAirNRelease; i++)
    {
        row = &dFTKirbySpecialStatusDescs[i - nFTCommonStatusSpecialStart];
        CHECK(row->proc_physics != NULL);
        CHECK(row->proc_map != NULL);
    }
    for (i = nFTKirbyStatusCopyNessSpecialN; i <= nFTKirbyStatusCopyNessSpecialAirN; i++)
    {
        row = &dFTKirbySpecialStatusDescs[i - nFTCommonStatusSpecialStart];
        CHECK(row->proc_physics != NULL);
        CHECK(row->proc_map != NULL);
    }

    /* Purin's two rows were PAST THE END of this array once:
     * ftkirbycopypurinspecialn.c is compiled, and
     * her setter is in slot 10, but the array stopped at
     * SpecialAirNCopy. Growing it to CopyYoshiSpecialAirNRelease is what
     * makes her mouth land in a row instead of past one. */
    row = &dFTKirbySpecialStatusDescs[nFTKirbyStatusCopyPurinSpecialN - nFTCommonStatusSpecialStart];
    CHECK(row->mflags.motion_id == nFTKirbyMotionCopyPurinSpecialN);
    CHECK(row->proc_map == ftKirbyCopyPurinSpecialNProcMap);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(0, 0, 0, 0, 0);
    fp.fkind = nFTKindKirby;

    /* ---- Mario's file, four statuses. FTKIRBY_COPYMARIO_FIREBALL_CHECK_
     * FTKIND reads passive copy_id inside the setter, so ONE function
     * reaches both Mario's rows and Luigi's. Wait is interposed before
     * each because these rows carry a status attack id and ftmain.c:4589
     * only reloads stat_flags when that id changes. ---- */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copy_id = nFTKindMario;
    ftKirbyCopyMarioSpecialNSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyMarioSpecialN);
    CHECK(fp.proc_accessory == ftKirbyCopyMarioSpecialNProcAccessory);
    CHECK(fp.motion_vars.flags.flag0 == 0);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copy_id = nFTKindLuigi;
    ftKirbyCopyMarioSpecialNSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyLuigiSpecialN);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copy_id = nFTKindMMario;
    ftKirbyCopyMarioSpecialAirNSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyMarioSpecialAirN);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copy_id = nFTKindNLuigi;
    ftKirbyCopyMarioSpecialAirNSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyLuigiSpecialAirN);

    /* the ground/air switch reads the same macro, and carries the frame
     * across so the fireball keeps its place in the animation */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copy_id = nFTKindLuigi;
    ftKirbyCopyMarioSpecialNSetStatus(mock_gobj);
    mock_gobj->anim_frame = 3.0f;
    ftKirbyCopyMarioSpecialNSwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyLuigiSpecialAirN);
    CHECK(fp.ga == nMPKineticsAir);
    ftKirbyCopyMarioSpecialAirNSwitchStatusGround(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyLuigiSpecialN);
    CHECK(fp.ga == nMPKineticsGround);

    /* ---- Fox's copy. The Blaster re-fires on a B tap, but only while
     * flag1 is up, and the interrupt picks the status by fp->ga. ---- */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftKirbyCopyFoxSpecialNSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyFoxSpecialN);
    CHECK(fp.motion_vars.flags.flag0 == 0);
    CHECK(fp.motion_vars.flags.flag1 == 0);

    fp.input.pl.button_tap = fp.input.button_mask_b;
    ftKirbyCopyFoxSpecialNProcInterrupt(mock_gobj);     /* flag1 still 0 */
    CHECK_STATUS(nFTKirbyStatusCopyFoxSpecialN);

    fp.motion_vars.flags.flag1 = 1;
    fp.ga = nMPKineticsAir;
    ftKirbyCopyFoxSpecialNProcInterrupt(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyFoxSpecialAirN);
    CHECK(fp.motion_vars.flags.flag1 == 0);             /* the setter reset it */
    fp.input.pl.button_tap = 0;
    fp.ga = nMPKineticsGround;

    /* the animation running out drops him back to Wait or Fall */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftKirbyCopyFoxSpecialNSetStatus(mock_gobj);
    mock_gobj->anim_frame = 0.0f;
    ftKirbyCopyFoxSpecialNProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusWait);

    /* ---- Samus's copy. The start animation plays SLOWER the more charge
     * is banked -- ftkirbycopysamusspecialn.c:326-333, 1.0 down to 0.84 at
     * FTKIRBY_COPYSAMUS_CHARGE_MAX -- and only a full bank starts released.
     * In the air it is released whatever the charge. ---- */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copysamus_charge_level = 0;
    ftKirbyCopySamusSpecialNStartSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopySamusSpecialNStart);
    CHECK_EQF(DObjGetStruct(mock_gobj)->anim_speed, 1.0f);
    CHECK(fp.status_vars.kirby.copysamus_specialn.is_release == FALSE);
    CHECK(fp.status_vars.kirby.copysamus_specialn.charge_gobj == NULL);
    CHECK(fp.proc_damage == ftKirbyCopySamusSpecialNProcDamage);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copysamus_charge_level = FTKIRBY_COPYSAMUS_CHARGE_MAX;
    ftKirbyCopySamusSpecialNStartSetStatus(mock_gobj);
    CHECK_EQF(DObjGetStruct(mock_gobj)->anim_speed, 0.84f);
    CHECK(fp.status_vars.kirby.copysamus_specialn.is_release == TRUE);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copysamus_charge_level = 0;
    ftKirbyCopySamusSpecialAirNStartSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopySamusSpecialAirNStart);
    CHECK(fp.status_vars.kirby.copysamus_specialn.is_release == TRUE);

    /* taking a hit mid-charge loses the bank and the held shot */
    fp.passive_vars.kirby.copysamus_charge_level = 4;
    ftKirbyCopySamusSpecialNProcDamage(mock_gobj);
    CHECK(fp.passive_vars.kirby.copysamus_charge_level == 0);
    CHECK(fp.status_vars.kirby.copysamus_specialn.charge_gobj == NULL);

    /* ---- Donkey's copy. The punch is banked in a PASSIVE var, so it
     * survives the status ending; the end setter picks Full over End at
     * max and cashes the bank into ground velocity. ---- */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copydonkey_charge_level = 0;
    ftKirbyCopyDonkeySpecialNStartSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyDonkeySpecialNStart);
    CHECK(fp.proc_damage == ftKirbyCopyDonkeySpecialNProcDamage);
    CHECK(fp.status_vars.kirby.copydonkey_specialn.is_release == FALSE);
    CHECK(fp.status_vars.kirby.copydonkey_specialn.is_charging == FALSE);
    CHECK(fp.status_vars.kirby.copydonkey_specialn.is_cancel == FALSE);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copydonkey_charge_level = FTKIRBY_COPYDONKEY_GIANTPUNCH_CHARGE_MAX;
    ftKirbyCopyDonkeySpecialNStartSetStatus(mock_gobj);
    CHECK(fp.status_vars.kirby.copydonkey_specialn.is_release == TRUE);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copydonkey_charge_level = FTKIRBY_COPYDONKEY_GIANTPUNCH_CHARGE_MAX;
    ftKirbyCopyDonkeySpecialNEndSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyDonkeySpecialNFull);
    CHECK_EQF(fp.physics.vel_ground.x,
              FTKIRBY_COPYDONKEY_GIANTPUNCH_CHARGE_MAX * FTKIRBY_COPYDONKEY_GIANTPUNCH_VEL_MUL);
    /* and the bank moves into the status var, leaving the passive empty */
    CHECK(fp.status_vars.kirby.copydonkey_specialn.charge_level == FTKIRBY_COPYDONKEY_GIANTPUNCH_CHARGE_MAX);
    CHECK(fp.passive_vars.kirby.copydonkey_charge_level == 0);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copydonkey_charge_level = 3;
    ftKirbyCopyDonkeySpecialNEndSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyDonkeySpecialNEnd);
    CHECK_EQF(fp.physics.vel_ground.x, 3.0f * FTKIRBY_COPYDONKEY_GIANTPUNCH_VEL_MUL);

    /* ---- Captain's copy: two statuses, and the switch keeps the frame ---- */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftKirbyCopyCaptainSpecialNSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyCaptainSpecialN);
    CHECK(fp.motion_vars.flags.flag0 == 0);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK(fp.motion_vars.flags.flag2 == 0);
    mock_gobj->anim_frame = 5.0f;
    ftKirbyCopyCaptainSpecialNSwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyCaptainSpecialAirN);
    CHECK(fp.ga == nMPKineticsAir);
    fp.ga = nMPKineticsGround;

    /* ---- Yoshi's copy. A swallowed Yoshi gives Kirby Yoshi's OWN catch
     * kind and Yoshi's OWN capture proc -- ftCommonCaptureYoshiProcCapture,
     * out of the file added for the egg. The copy reuses the
     * victim side whole; only the catch proc is Kirby's. ---- */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftKirbyCopyYoshiSpecialNSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyYoshiSpecialN);
    CHECK(fp.is_catchstatus == TRUE);
    CHECK(fp.catch_mask == FTCATCHKIND_MASK_YOSHISPECIALN);
    CHECK(fp.proc_catch == ftKirbyCopyYoshiSpecialNCatchProcCatch);
    CHECK(fp.proc_capture == ftCommonCaptureYoshiProcCapture);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.ga = nMPKineticsAir;
    ftKirbyCopyYoshiSpecialAirNSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyYoshiSpecialAirN);
    CHECK(fp.proc_catch == ftKirbyCopyYoshiSpecialAirNCatchProcCatch);
    CHECK(fp.proc_capture == ftCommonCaptureYoshiProcCapture);
    fp.ga = nMPKineticsGround;

    /* the catch status waits for either a caught fighter or the end of the
     * animation before it moves on (ftkirbycopyyoshispecialn.c:14) */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftKirbyCopyYoshiSpecialNCatchProcCatch(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyYoshiSpecialNCatch);

    /* ---- the outer demux still reaches all of this through the selector:
     * copy_id picks the mouth, and an unported one is still
     * a no-op ---- */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copy_id = nFTKindDonkey;
    ftKirbySpecialNSetStatusSelect(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyDonkeySpecialNStart);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copy_id = nFTKindNess;
    ftKirbySpecialNSetStatusSelect(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyNessSpecialN);

    /* ---- Kirby's two stars, efkirbystar.mdl's. The star
     * round a spat-out fighter hangs off its TopN (kind 0x50) and is
     * scaled to its kind; the lost copy's flies from Kirby's own position
     * and falls, spinning. Both work on the child of the bare root. ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    ftKirbyMainMotionBindOffsets();
    ef_pool_reset();
    {
        FTKirbyCopy *copy = lbRelocGetFileData(FTKirbyCopy*,
            gFTDataKirbyMainMotion, &llKirbyMainMotionSpecialNFTKirbyCopy);
        GObj *star = efManagerCaptureKirbyStarMakeEffect(mock_gobj);
        DObj *child;

        CHECK(star != NULL);
        child = (star != NULL) ? DObjGetStruct(star)->child : NULL;
        CHECK(child != NULL);
        if (child != NULL)
        {
            CHECK(efGetStruct(star)->fighter_gobj == mock_gobj);
            CHECK(child->user_data.p == fp.joints[nFTPartsJointTopN]);
            CHECK(child->xobjs[0]->kind == 0x50);
            CHECK(child->xobjs[1]->kind == 0x2E);
            CHECK_EQF(child->scale.vec.f.x, copy[fp.fkind].effect_scale);
            CHECK_EQF(child->scale.vec.f.z, 1.0F);
            gcEjectGObj(star);
        }

        fp.lr = -1;
        star = efManagerLoseKirbyStarMakeEffect(mock_gobj);
        CHECK(star != NULL);
        child = (star != NULL) ? DObjGetStruct(star)->child : NULL;
        CHECK(child != NULL);
        if (child != NULL)
        {
            EFStruct *ep = efGetStruct(star);
            f32 x = DObjGetStruct(mock_gobj)->translate.vec.f.x;

            CHECK_EQF(child->translate.vec.f.x, x);
            CHECK(ep->effect_vars.lose_kirby_star.lr == -1);
            CHECK(ep->effect_vars.lose_kirby_star.vel.x < 0.0F);
            efManagerLoseKirbyStarProcUpdate(star);
            CHECK(child->translate.vec.f.x < x);
            CHECK(child->rotate.vec.f.z > 0.0F);
            gcEjectGObj(star);
        }
        fp.lr = +1;
    }
    sEFManagerStructsAllocFree = NULL;
    sEFManagerStructsFreeNum = 0;

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindMario;
}

/* Link's boomerang, his FIRST special table, and Kirby's
 * copy of it. ft/ftchar/ftlink/ftlink.c, ftlinkspecialn.c,
 * wp/wplink/wplinkboomerang.c and ft/ftchar/ftkirby/ftkirbycopylink-
 * specialn.c all compile unmodified; what is under test here is the
 * fifteen-row table they need, the four demux slots that reach it, the
 * two Kirby slots and six Kirby rows, and one thing that is not a new
 * feature at all -- row 220. ftCommonAttack13SetStatus has named
 * nFTLinkStatusAttack13 since the jab was ported, so Link's third jab
 * already ran ftMainSetStatus into a fighter whose table did not exist.
 * The weapon itself is test_wp_link_boomerang, with the other wp tests. */
static void test_link_boomerang(void)
{
    extern void (*dFTCommonSpecialNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);
    extern void (*dFTKirbySpecialNStatusList[])(GObj*);
    extern void (*dFTKirbySpecialAirNStatusList[])(GObj*);
    extern FTStatusDesc dFTLinkSpecialStatusDescs[];
    extern FTStatusDesc dFTKirbySpecialStatusDescs[];
    FTStatusDesc *row;
    GObj fake_boomerang;
    int i;

    /* ---- the four demux slots, ground and air, base kind and nametag ---- */
    CHECK(dFTCommonSpecialNStatusList[nFTKindLink] == ftLinkSpecialNSetStatus);
    CHECK(dFTCommonSpecialNStatusList[nFTKindNLink] == ftLinkSpecialNSetStatus);
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindLink] == ftLinkSpecialAirNSetStatus);
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindNLink] == ftLinkSpecialAirNSetStatus);
    /* filled his Up-B, both ways and both nametags. His
     * Down-B is still a hole -- it throws a bomb, and a bomb is an item,
     * so it depends on the item code. */
    CHECK(dFTCommonSpecialHiStatusList[nFTKindLink] == ftLinkSpecialHiSetStatus);
    CHECK(dFTCommonSpecialHiStatusList[nFTKindNLink] == ftLinkSpecialHiSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindLink] == ftLinkSpecialAirHiSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindNLink] == ftLinkSpecialAirHiSetStatus);

    /* ---- the rows. 220 and 224-225 are common-proc rows; 221-223 are
     * the rapid jab, 226-228 the Spin Attack and
     * 229-234 the Neutral-B. His table has no holes left. ---- */
    row = &dFTLinkSpecialStatusDescs[nFTLinkStatusAttack13 - nFTCommonStatusSpecialStart];
    CHECK(row->mflags.motion_id == nFTLinkMotionAttack13);
    CHECK(row->proc_update == ftAnimEndSetWait);
    CHECK(row->proc_map == mpCommonSetFighterFallOnEdgeBreak);
    /* 221-223, the rapid jab, stopped being a hole --
     * the family is checked with the other four fighters' in
     * test_specialn_demux */
    for (i = nFTLinkStatusAttack100Start; i <= nFTLinkStatusAttack100End; i++)
    {
        row = &dFTLinkSpecialStatusDescs[i - nFTCommonStatusSpecialStart];
        CHECK(row->proc_physics == ftPhysicsApplyGroundVelFriction);
        CHECK(row->proc_map == mpCommonSetFighterFallOnEdgeBreak);
    }
    for (i = nFTLinkStatusAppearR; i <= nFTLinkStatusAppearL; i++)
    {
        row = &dFTLinkSpecialStatusDescs[i - nFTCommonStatusSpecialStart];
        CHECK(row->proc_update == ftCommonAppearProcUpdate);
        CHECK(row->proc_physics == ftCommonAppearProcPhysics);
    }
    /* the Spin Attack's three rows. All three carry the
     * SpecialHi attack id, the ground pair is Ground and the air one is
     * Air, and every one of them has a physics and a map proc -- the
     * middle row's physics is the common friction, which is the game's
     * own choice and not a gap. */
    for (i = nFTLinkStatusSpecialHi; i <= nFTLinkStatusSpecialAirHi; i++)
    {
        row = &dFTLinkSpecialStatusDescs[i - nFTCommonStatusSpecialStart];
        CHECK(row->sflags.attack_id == nFTStatusAttackIDSpecialHi);
        CHECK(row->proc_physics != NULL);
        CHECK(row->proc_map != NULL);
        CHECK(row->proc_interrupt == NULL);
        CHECK(row->sflags.ga ==
              ((i == nFTLinkStatusSpecialAirHi) ? nMPKineticsAir
                                                : nMPKineticsGround));
    }
    row = &dFTLinkSpecialStatusDescs[nFTLinkStatusSpecialHi - nFTCommonStatusSpecialStart];
    CHECK(row->mflags.motion_id == nFTLinkMotionSpecialHi);
    CHECK(row->proc_update == ftLinkSpecialHiProcUpdate);
    CHECK(row->proc_map == ftLinkSpecialHiProcMap);
    row = &dFTLinkSpecialStatusDescs[nFTLinkStatusSpecialHiEnd - nFTCommonStatusSpecialStart];
    CHECK(row->proc_update == ftLinkSpecialHiEndProcUpdate);
    CHECK(row->proc_physics == ftPhysicsApplyGroundVelFriction);
    row = &dFTLinkSpecialStatusDescs[nFTLinkStatusSpecialAirHi - nFTCommonStatusSpecialStart];
    CHECK(row->proc_update == ftLinkSpecialAirHiProcUpdate);
    CHECK(row->proc_physics == ftLinkSpecialAirHiProcPhysics);
    for (i = nFTLinkStatusSpecialN; i <= nFTLinkStatusSpecialAirNEmpty; i++)
    {
        row = &dFTLinkSpecialStatusDescs[i - nFTCommonStatusSpecialStart];
        CHECK(row->proc_physics != NULL);
        CHECK(row->proc_map != NULL);
        /* every one of the six is a projectile status: that flag is what
         * ftMainProcSearch reads to let a reflector turn the boomerang */
        CHECK(row->sflags.is_projectile == TRUE);
        CHECK(row->sflags.attack_id == nFTStatusAttackIDSpecialN);
    }
    /* the array stops here on purpose -- 235-236 are the bombs */
    CHECK(nFTLinkStatusSpecialAirNEmpty - nFTCommonStatusSpecialStart + 1 == 15);

    /* ---- row 220 was a live abort, not a hole: this is the path that
     * reached it. ftCommonAttack13SetStatus's fkind switch has named
     * nFTLinkStatusAttack13 since the jab, into FT_SPECIAL_NONE. ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindLink;
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftCommonAttack13SetStatus(mock_gobj);
    CHECK_STATUS(nFTLinkStatusAttack13);
    CHECK(fp.motion_id == nFTLinkMotionAttack13);
    CHECK(fp.attack1_status_id == nFTLinkStatusAttack13);

    /* ---- the setters. Empty-handed he throws; with one already out the
     * animation plays through with nothing spawned, and the throw is
     * interruptible so the catch can take it back. ---- */
    memset(&fake_boomerang, 0, sizeof(fake_boomerang));
    fp.passive_vars.link.boomerang_gobj = NULL;

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    /* proc_status is a ONE-SHOT (ft/ftmain.c:4593-4598): ftMainSetStatus
     * runs it and clears it inside the setter, so what can be checked is
     * its EFFECT -- here, that a hard stick held this frame came out of
     * the throw as a smash throw. */
    fp.input.pl.stick_range.x = FTLINK_BOOMERANG_SMASH_STICK_MIN;
    fp.hold_stick_x = 0;
    fp.status_vars.link.specialn.is_smash = FALSE;
    ftLinkSpecialNSetStatus(mock_gobj);
    CHECK_STATUS(nFTLinkStatusSpecialN);
    CHECK(fp.proc_status == NULL);
    CHECK(fp.status_vars.link.specialn.is_smash == TRUE);
    CHECK(fp.motion_id == nFTLinkMotionSpecialN);
    fp.input.pl.stick_range.x = 0;

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.is_special_interrupt = FALSE;
    fp.passive_vars.link.boomerang_gobj = &fake_boomerang;
    ftLinkSpecialNSetStatus(mock_gobj);
    CHECK_STATUS(nFTLinkStatusSpecialNEmpty);
    CHECK(fp.is_special_interrupt == TRUE);

    fp.ga = nMPKineticsAir;
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.link.boomerang_gobj = NULL;
    ftLinkSpecialAirNSetStatus(mock_gobj);
    CHECK_STATUS(nFTLinkStatusSpecialAirN);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.is_special_interrupt = FALSE;
    fp.passive_vars.link.boomerang_gobj = &fake_boomerang;
    ftLinkSpecialAirNSetStatus(mock_gobj);
    CHECK_STATUS(nFTLinkStatusSpecialAirNEmpty);
    CHECK(fp.is_special_interrupt == TRUE);

    /* ---- the catch. Only ga decides which of the two it is, and the
     * boomerang's own code calls this on its owner. ---- */
    fp.ga = nMPKineticsGround;
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftLinkSpecialNGetSetStatus(mock_gobj);
    CHECK_STATUS(nFTLinkStatusSpecialNGet);

    fp.ga = nMPKineticsAir;
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftLinkSpecialNGetSetStatus(mock_gobj);
    CHECK_STATUS(nFTLinkStatusSpecialAirNReturn);

    /* ---- the ground/air switch keeps the throw's own frame: both map
     * procs hand mpCommon the current anim_frame under
     * FTSTATUS_PRESERVE_MODELPART, so walking off a ledge mid-throw does
     * not restart the animation. ---- */
    fp.ga = nMPKineticsGround;
    fp.passive_vars.link.boomerang_gobj = NULL;
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftLinkSpecialNSetStatus(mock_gobj);
    mock_gobj->anim_frame = 7.0f;
    ftLinkSpecialNSwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTLinkStatusSpecialAirN);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK_EQF(mock_gobj->anim_frame, 7.0f);

    mock_gobj->anim_frame = 3.0f;
    ftLinkSpecialAirNSwitchStatusGround(mock_gobj);
    CHECK_STATUS(nFTLinkStatusSpecialN);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK_EQF(mock_gobj->anim_frame, 3.0f);

    /* the empty pair's switches also raise is_special_interrupt, because
     * an empty-handed Link is the one who can still catch */
    fp.is_special_interrupt = FALSE;
    mock_gobj->anim_frame = 2.0f;
    ftLinkSpecialNEmptySwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTLinkStatusSpecialAirNEmpty);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(fp.is_special_interrupt == TRUE);

    fp.is_special_interrupt = FALSE;
    ftLinkSpecialAirNEmptySwitchStatusGround(mock_gobj);
    CHECK_STATUS(nFTLinkStatusSpecialNEmpty);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK(fp.is_special_interrupt == TRUE);

    /* ---- the smash read. ftLinkSpecialNProcStatus is the status proc
     * every one of the four setters installs, and it runs once a frame:
     * a hard sideways stick held for fewer than FTLINK_BOOMERANG_SMASH_
     * BUFFER frames is a smash throw. It also clears flag0, which is the
     * animation event that spawns the boomerang -- so the spawn cannot
     * survive into the next frame unfired. ---- */
    fp.motion_vars.flags.flag0 = 1;
    fp.input.pl.stick_range.x = FTLINK_BOOMERANG_SMASH_STICK_MIN;
    fp.hold_stick_x = FTLINK_BOOMERANG_SMASH_BUFFER - 1;
    fp.stat_flags.is_smash_attack = FALSE;
    ftLinkSpecialNProcStatus(mock_gobj);
    CHECK(fp.motion_vars.flags.flag0 == 0);
    CHECK(fp.status_vars.link.specialn.is_smash == TRUE);
    CHECK(fp.stat_flags.is_smash_attack == TRUE);   /* REGION_US only */

    /* one frame too late is a tilt throw, and so is a soft stick */
    fp.hold_stick_x = FTLINK_BOOMERANG_SMASH_BUFFER;
    ftLinkSpecialNProcStatus(mock_gobj);
    CHECK(fp.status_vars.link.specialn.is_smash == FALSE);

    fp.hold_stick_x = 0;
    fp.input.pl.stick_range.x = FTLINK_BOOMERANG_SMASH_STICK_MIN - 1;
    ftLinkSpecialNProcStatus(mock_gobj);
    CHECK(fp.status_vars.link.specialn.is_smash == FALSE);

    /* the stick read is on the magnitude, so a hard LEFT stick is a smash
     * throw too, whichever way Link faces */
    fp.input.pl.stick_range.x = -FTLINK_BOOMERANG_SMASH_STICK_MIN;
    ftLinkSpecialNProcStatus(mock_gobj);
    CHECK(fp.status_vars.link.specialn.is_smash == TRUE);

    /* ---- Kirby's copy: the Link mouth and the six
     * rows behind it. The setter reads a DIFFERENT passive var --
     * copylink_boomerang_gobj, not link.boomerang_gobj -- which is the
     * whole reason the copy is its own file. ---- */
    CHECK(dFTKirbySpecialNStatusList[nFTKindLink] == ftKirbyCopyLinkSpecialNSetStatus);
    CHECK(dFTKirbySpecialAirNStatusList[nFTKindLink] == ftKirbyCopyLinkSpecialAirNSetStatus);
    for (i = nFTKirbyStatusCopyLinkSpecialN; i <= nFTKirbyStatusCopyLinkSpecialAirNEmpty; i++)
    {
        row = &dFTKirbySpecialStatusDescs[i - nFTCommonStatusSpecialStart];
        CHECK(row->proc_physics != NULL);
        CHECK(row->proc_map != NULL);
        CHECK(row->sflags.attack_id == nFTStatusAttackIDSpecialNCopyLink);
    }

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindKirby;
    fp.ga = nMPKineticsGround;
    fp.passive_vars.kirby.copylink_boomerang_gobj = NULL;

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copy_id = nFTKindLink;
    fp.input.pl.stick_range.x = FTKIRBY_COPYLINK_BOOMERANG_SMASH_STICK_MIN;
    fp.hold_stick_x = 0;
    fp.status_vars.kirby.copylink_specialn.is_smash = FALSE;
    ftKirbySpecialNSetStatusSelect(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyLinkSpecialN);
    /* the same one-shot, and Kirby's copy of it reads his own status var */
    CHECK(fp.status_vars.kirby.copylink_specialn.is_smash == TRUE);
    fp.input.pl.stick_range.x = 0;

    /* the same swallowed Link, with a boomerang already out */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copylink_boomerang_gobj = &fake_boomerang;
    fp.is_special_interrupt = FALSE;
    ftKirbySpecialNSetStatusSelect(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyLinkSpecialNEmpty);
    CHECK(fp.is_special_interrupt == TRUE);

    /* and the catch, which the boomerang calls on Kirby rather than on
     * Link because wpLinkBoomerangCheckOwnerCatch branches on fkind */
    fp.ga = nMPKineticsAir;
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftKirbyCopyLinkSpecialNGetSetStatus(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyLinkSpecialAirNReturn);

    fp.passive_vars.kirby.copylink_boomerang_gobj = NULL;
    fp.passive_vars.kirby.copy_id = nFTKindKirby;
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;

    /* ---- the Spin Attack ----
     *
     * Link's Up-B, and the last special in the game outside Ness's that
     * is not an item. Both decomp files compile AND link unmodified --
     * ft/ftchar/ftlink/ftlinkspecialhi.c and wp/wplink/wplinkspinattack.c
     * -- so the port adds three table rows, four demux slots
     * (checked above) and two packs.
     *
     * The two setters, driven the way the demux drives them: a B-tap
     * with the stick up goes to 226 on the ground and 228 in the air.
     * The ground one is what ftLinkSpecialHiEndProcMap sends to 227 when
     * the spin lands, which is why 227 needs no setter of its own. ---- */
    fp.fkind = nFTKindLink;
    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsGround;
    ftLinkSpecialHiSetStatus(mock_gobj);
    CHECK_STATUS(nFTLinkStatusSpecialHi);

    spawn(0.0f, 0.0f);
    fp.ga = nMPKineticsAir;
    ftLinkSpecialAirHiSetStatus(mock_gobj);
    CHECK_STATUS(nFTLinkStatusSpecialAirHi);

    /* and 227, which only the landing reaches */
    spawn(0.0f, 0.0f);
    ftLinkSpecialHiEndSetStatus(mock_gobj);
    CHECK_STATUS(nFTLinkStatusSpecialHiEnd);

    /* ---- the two packs. The weapon is the spinning hitbox and the
     * effect is the glow around him.
     *
     * The weapon is the thirteenth WPDesc this port compiles and the
     * first added after the weapon frontier closed. Its
     * flags are 0x03 -- a DObjDesc tree with DL links, the Final
     * Cutter's shape -- and it is the only weapon here whose attributes
     * live in a fighter's MAIN file rather than one of his Specials.
     * NINE MObjs over two joints, and not one of them steps a sprite
     * array: what their MatAnimJoint moves is a colour. That last fact
     * is what the packer had never met -- pack_wptree's per-frame
     * closure assumed every MObj had an array, because the two weapons
     * it was written for did. ---- */
    {
        Fighter wp_pack;
        int pal_bank = 0;
        int i;

        memset(&wp_pack, 0, sizeof(wp_pack));
        CHECK(fighter_load(&wp_pack, "wplinkspin.mdl", &pal_bank) == 0);

        if (wp_pack.hd != NULL)
        {
            CHECK(wp_pack.hd->joint_count == 2);
            CHECK(wp_pack.hd->tri_count == 18);
            CHECK(wp_pack.hd->batch_count == 9);
            /* nine batches, nine MObjs, nine textures: one per MObj,
             * because each names one picture and no two fold to the
             * same material */
            CHECK(wp_pack.hd->tex_count == 9);
            CHECK(wp_pack.hd->anim_count == 1);
            CHECK(wp_pack.mobjs != NULL);

            if (wp_pack.mobjs != NULL)
            {
                CHECK(wp_pack.mobjs->mobj_count == 9);
                /* every one of the nine is a single picture */
                for (i = 0; i < 9; i++)
                {
                    CHECK(wp_pack.mobj_subs[i].tex_count == 1);
                }
                CHECK(wp_pack.mobjs->words > 0);
            }
            fighter_release(&wp_pack);
        }
        memset(&wp_pack, 0, sizeof(wp_pack));
        pal_bank = 0;
        CHECK(fighter_load(&wp_pack, "efspin.mdl", &pal_bank) == 0);

        if (wp_pack.hd != NULL)
        {
            CHECK(wp_pack.hd->joint_count == 2);
            CHECK(wp_pack.hd->tri_count == 2);
            CHECK(wp_pack.hd->batch_count == 1);
            CHECK(wp_pack.hd->tex_count == 1);
            CHECK(wp_pack.hd->anim_count == 1);
            CHECK(wp_pack.mobjs != NULL);

            if (wp_pack.mobjs != NULL)
            {
                CHECK(wp_pack.mobjs->mobj_count == 1);
                CHECK(wp_pack.mobj_subs[0].tex_count == 1);
            }
            fighter_release(&wp_pack);
        }
    }
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindMario;
}

/* Luigi's special table. No new file compiles for this:
 * every proc in his nine rows is Mario's, out of the three files the
 * port has. What the table fixes is an abort.
 * Mario's setters do not branch on fkind -- ftMarioSpecialNSetStatus
 * sets the NUMBER 223 -- and five of Luigi's six demux slots have held
 * those setters, so every special Luigi had ran
 * ftMainSetStatus into FT_SPECIAL_NONE, where ftMainGetStatusDesc
 * returns NULL and ftMainSetStatus prints and calls abort(). Reaching
 * the end of this test is itself the check. */
static void test_luigi_table(void)
{
    extern void (*dFTCommonSpecialNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirLwStatusList[])(GObj*);
    extern FTStatusDesc dFTLuigiSpecialStatusDescs[];
    extern FTStatusDesc dFTMarioSpecialStatusDescs[];
    static const s32 kMotions[9] = {
        nFTLuigiMotionAttack13, nFTLuigiMotionAppearR, nFTLuigiMotionAppearL,
        nFTLuigiMotionSpecialN, nFTLuigiMotionSpecialAirN,
        nFTLuigiMotionSpecialHi, nFTLuigiMotionSpecialAirHi,
        nFTLuigiMotionSpecialLw, nFTLuigiMotionSpecialAirLw
    };
    FTStatusDesc *row;
    int i;

    /* ---- the five slots that were live into nothing, and the sixth,
     * which the port had already noticed and left NULL ---- */
    CHECK(dFTCommonSpecialNStatusList[nFTKindLuigi] == ftMarioSpecialNSetStatus);
    CHECK(dFTCommonSpecialNStatusList[nFTKindNLuigi] == ftMarioSpecialNSetStatus);
    CHECK(dFTCommonSpecialHiStatusList[nFTKindLuigi] == ftMarioSpecialHiSetStatus);
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindLuigi] == ftMarioSpecialAirNSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindLuigi] == ftMarioSpecialAirHiSetStatus);
    CHECK(dFTCommonSpecialAirLwStatusList[nFTKindLuigi] == ftMarioSpecialAirLwSetStatus);

    /* ---- nine rows, all filled, each naming the motion its own enum
     * does. The enums are Mario's laid out row for row, so those ids are
     * numerically Mario's too -- what differs is the pack behind them. ---- */
    for (i = 0; i < 9; i++)
    {
        row = &dFTLuigiSpecialStatusDescs[i];
        CHECK(row->proc_physics != NULL);
        CHECK(row->proc_map != NULL);
        CHECK(row->mflags.motion_id == kMotions[i]);
    }

    /* ---- and the fact that is worth writing down: his table and
     * Mario's are the SAME TABLE. Parse both out of the decomp, rename
     * one fighter to the other, and they agree line for line -- so the
     * port could have pointed dFTMainSpecialStatusDescs[nFTKindLuigi] at
     * Mario's array and been numerically right. It keeps two because the
     * game keeps two. ---- */
    CHECK(memcmp(dFTLuigiSpecialStatusDescs, dFTMarioSpecialStatusDescs,
                 9 * sizeof(FTStatusDesc)) == 0);

    /* ---- every one of the six, driven. Each of these lines
     * would otherwise abort(). ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindLuigi;
    fp.attr->is_have_specialn = TRUE;
    fp.attr->is_have_specialhi = TRUE;
    fp.attr->is_have_speciallw = TRUE;

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftCommonAttack13SetStatus(mock_gobj);
    CHECK_STATUS(nFTLuigiStatusAttack13);
    CHECK(fp.attack1_status_id == nFTLuigiStatusAttack13);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMarioSpecialNSetStatus(mock_gobj);
    CHECK_STATUS(nFTLuigiStatusSpecialN);
    CHECK(fp.proc_map == ftMarioSpecialNProcMap);
    /* the fireball's animation gate is armed by the setter, and it is the
     * accessory -- not the row -- that reads fkind to pick Luigi's
     * projectile out of dWPMarioFireballWeaponAttributes */
    CHECK(fp.proc_accessory == ftMarioSpecialNProcAccessory);
    CHECK(fp.motion_vars.flags.flag0 == FALSE);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMarioSpecialAirNSetStatus(mock_gobj);
    CHECK_STATUS(nFTLuigiStatusSpecialAirN);
    CHECK(fp.proc_physics == ftPhysicsApplyAirVelDrift);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMarioSpecialHiSetStatus(mock_gobj);
    CHECK_STATUS(nFTLuigiStatusSpecialHi);
    CHECK(fp.proc_interrupt == ftMarioSpecialHiProcInterrupt);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMarioSpecialAirHiSetStatus(mock_gobj);
    CHECK_STATUS(nFTLuigiStatusSpecialAirHi);

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftMarioSpecialAirLwSetStatus(mock_gobj);
    CHECK_STATUS(nFTLuigiStatusSpecialAirLw);

    /* the ground Tornado through the real check rather than the setter:
     * dFTCommonSpecialLwStatusList's Luigi slot is the last of the
     * six.
     *
     * It lands in the AIR row, 228, not 227: ftMarioSpecialLwSetStatus
     * calls mpCommonSetFighterAir and sets nFTMarioStatusSpecialAirLw
     * outright, then puts stat_flags.ga back to Ground by hand -- the
     * Tornado lifts him off the floor on frame one whichever way it was
     * started. Row 227 is reached only from above, when a spinning Luigi
     * touches back down (ftmariospeciallw.c:120). */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindLuigi;
    fp.ga = nMPKineticsGround;
    fp.attr->is_have_speciallw = TRUE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALLW_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialLwCheckInterruptCommon(mock_gobj) != FALSE);
    CHECK_STATUS(nFTLuigiStatusSpecialAirLw);
    CHECK(fp.ga == nMPKineticsAir);              /* really airborne */
    CHECK(fp.stat_flags.ga == nMPKineticsGround); /* but the row says ground */
    CHECK_EQF(fp.physics.vel_air.y, -7.0f);
    fp.input.pl.button_tap = 0;
    fp.input.pl.stick_range.y = 0;

    /* and the landing, which is the only way into row 227 */
    ftMarioSpecialAirLwSwitchStatusGround(mock_gobj);
    CHECK_STATUS(nFTLuigiStatusSpecialLw);
    CHECK(fp.proc_map == ftMarioSpecialLwProcMap);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.proc_accessory = NULL;
    fp.fkind = nFTKindMario;
}

/* Ness's table, and the LAST live abort of the kind steps
 * the earlier tables found. No new file compiles for the six rows
 * themselves; the C is four ftcommonentry.c procs of Ness's, hand-ported
 * with the rest of that file. His specials stay holes: ftnessspecialn.c
 * is 118 lines and would compile unmodified but for itNessPKFireMakeItem,
 * which needs the item code. */
static void test_ness_table(void)
{
    extern void (*dFTCommonSpecialNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirLwStatusList[])(GObj*);
    extern FTStatusDesc dFTNessSpecialStatusDescs[];
    extern sb32 ftCommonEntryHasAppear(s32 fkind);
    static const s32 kMotions[6] = {
        nFTNessMotionAttack13,
        nFTNessMotionAppearRStart, nFTNessMotionAppearLStart,
        nFTNessMotionAppearWait,
        nFTNessMotionAppearREnd, nFTNessMotionAppearLEnd
    };
    FTStatusDesc *row;
    int i;

    /* ---- his specials: every demux slot is his, for
     * Ness and NNess alike; test_ness_specials has the moves ---- */
    CHECK(dFTCommonSpecialNStatusList[nFTKindNess] == ftNessSpecialNSetStatus);
    CHECK(dFTCommonSpecialNStatusList[nFTKindNNess] == ftNessSpecialNSetStatus);
    CHECK(dFTCommonSpecialHiStatusList[nFTKindNess] == ftNessSpecialHiStartSetStatus);
    CHECK(dFTCommonSpecialHiStatusList[nFTKindNNess] == ftNessSpecialHiStartSetStatus);
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindNess] == ftNessSpecialAirNSetStatus);
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindNNess] == ftNessSpecialAirNSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindNess] == ftNessSpecialAirHiStartSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindNNess] == ftNessSpecialAirHiStartSetStatus);
    CHECK(dFTCommonSpecialAirLwStatusList[nFTKindNess] == ftNessSpecialAirLwStartSetStatus);
    CHECK(dFTCommonSpecialAirLwStatusList[nFTKindNNess] == ftNessSpecialAirLwStartSetStatus);

    /* ---- six rows, and the array stops there on purpose ---- */
    for (i = 0; i < 6; i++)
    {
        row = &dFTNessSpecialStatusDescs[i];
        CHECK(row->proc_update != NULL);
        CHECK(row->proc_physics != NULL);
        CHECK(row->proc_map != NULL);
        CHECK(row->mflags.motion_id == kMotions[i]);
    }
    CHECK(nFTNessStatusAppearLEnd - nFTCommonStatusSpecialStart + 1 == 6);
    /* the three of the five that need procs of Ness's own */
    CHECK(dFTNessSpecialStatusDescs[nFTNessStatusAppearRStart - nFTCommonStatusSpecialStart].proc_update
          == ftNessAppearStartProcUpdate);
    CHECK(dFTNessSpecialStatusDescs[nFTNessStatusAppearLStart - nFTCommonStatusSpecialStart].proc_update
          == ftNessAppearStartProcUpdate);
    CHECK(dFTNessSpecialStatusDescs[nFTNessStatusAppearWait - nFTCommonStatusSpecialStart].proc_update
          == ftNessAppearWaitProcUpdate);
    CHECK(dFTNessSpecialStatusDescs[nFTNessStatusAppearREnd - nFTCommonStatusSpecialStart].proc_update
          == ftCommonAppearProcUpdate);

    /* ---- the abort. Each of these three lines would otherwise end in scManagerRunPrintGObjStatus's abort(). ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindNess;

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftCommonAttack13SetStatus(mock_gobj);
    CHECK_STATUS(nFTNessStatusAttack13);
    CHECK(fp.motion_id == nFTNessMotionAttack13);
    CHECK(fp.attack1_status_id == nFTNessStatusAttack13);

    /* ---- the entrance, walked. Ness's is the roster's longest: five
     * statuses, because he arrives inside the PSI ring. Each advance is an
     * animation end, which is anim_frame reaching zero. ---- */
    ftMainSetStatus(mock_gobj, nFTNessStatusAppearRStart, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.status_vars.common.entry.lr = +1;
    fp.is_shadow_hide = TRUE;

    mock_gobj->anim_frame = 5.0f;            /* still ringing */
    ftNessAppearStartProcUpdate(mock_gobj);
    CHECK_STATUS(nFTNessStatusAppearRStart);

    mock_gobj->anim_frame = 0.0f;            /* the ring has closed */
    ftNessAppearStartProcUpdate(mock_gobj);
    CHECK_STATUS(nFTNessStatusAppearWait);
    CHECK(fp.is_shadow_hide == FALSE);       /* the shadow comes back here */

    fp.status_vars.common.entry.lr = +1;
    mock_gobj->anim_frame = 0.0f;
    ftNessAppearWaitProcUpdate(mock_gobj);
    CHECK_STATUS(nFTNessStatusAppearREnd);

    /* facing the other way ends in the other row -- entry.lr, not fp.lr,
     * because ftCommonAppearSetStatus zeroes fp.lr for the warp-in */
    ftMainSetStatus(mock_gobj, nFTNessStatusAppearWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.status_vars.common.entry.lr = -1;
    mock_gobj->anim_frame = 0.0f;
    ftNessAppearWaitProcUpdate(mock_gobj);
    CHECK_STATUS(nFTNessStatusAppearLEnd);

    /* ---- and the second fact. ftCommonAppearSetStatus takes the status id
     * from dFTCommonEntryAppearStatusIDs; a kind without a row there
     * stands on the spot at the countdown instead of warping in. Every
     * kind has a row: Ness, Link, Luigi, Pikachu and Master Hand all warp
     * in, and nobody stands on the spot. ---- */
    CHECK(ftCommonEntryHasAppear(nFTKindNess) != FALSE);
    CHECK(ftCommonEntryHasAppear(nFTKindLink) != FALSE);
    CHECK(ftCommonEntryHasAppear(nFTKindLuigi) != FALSE);
    CHECK(ftCommonEntryHasAppear(nFTKindMario) != FALSE);
    CHECK(ftCommonEntryHasAppear(nFTKindPikachu) != FALSE);
    CHECK(ftCommonEntryHasAppear(nFTKindBoss) != FALSE);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindMario;
}

/* ft/ftcommon/ftcommonentry.c, the roster's entrance.
 *
 * Two facts have to be true for a fighter to warp in rather than abort:
 * dFTCommonEntryAppearStatusIDs must name a status for his kind, and
 * that status must have a row in the table dFTMainSpecialStatusDescs
 * points his kind at. Steps 60-66 found the gap between those two facts
 * four separate times, so this test asks both questions for all 27 kinds
 * and both facings in a loop, rather than a kind at a time. */
static void test_roster_entrance(void)
{
    extern sb32 ftCommonEntryHasAppear(s32 fkind);
    extern sb32 ftCommonEntryAppearIsLive(s32 fkind, s32 entry_id);
    extern FTStatusDesc dFTCommonActionStatusDescs[];
    extern FTStatusDesc dFTFoxSpecialStatusDescs[];
    extern FTStatusDesc dFTDonkeySpecialStatusDescs[];
    extern FTStatusDesc dFTSamusSpecialStatusDescs[];
    extern FTStatusDesc dFTYoshiSpecialStatusDescs[];
    extern FTStatusDesc dFTCaptainSpecialStatusDescs[];
    extern FTStatusDesc dFTKirbySpecialStatusDescs[];
    extern FTStatusDesc dFTPurinSpecialStatusDescs[];
    FTStatusDesc *row;
    int i, live;

    /* ---- the census. ALL 27 kinds have both facings live.
     * Master Hand's is live only because his special table exists;
     * a filled dFTCommonEntryAppearStatusIDs row with no status table
     * under it makes HasAppear TRUE while IsLive stays FALSE, which is
     * why both are asked. Both facings of Master
     * Hand's row are the SAME status, nFTBossStatusAppear: a hand has no
     * left and right, and the loop asks each facing separately so that
     * would show. ---- */
    live = 0;
    for (i = 0; i < nFTKindEnumCount; i++)
    {
        CHECK(ftCommonEntryHasAppear(i) != FALSE);
        CHECK(ftCommonEntryAppearIsLive(i, 0) != FALSE);
        CHECK(ftCommonEntryAppearIsLive(i, 1) != FALSE);
        live++;
    }
    CHECK(live == nFTKindEnumCount);
    CHECK(ftCommonEntryAppearIsLive(-1, 0) == FALSE);
    CHECK(ftCommonEntryAppearIsLive(nFTKindEnumCount, 0) == FALSE);
    CHECK(ftCommonEntryAppearIsLive(nFTKindMario, 2) == FALSE);

    /* the twelve Polys are the common EntryNull, not a special status of
     * their own: below nFTCommonStatusSpecialStart, so IsLive above went
     * through dFTCommonActionStatusDescs to answer for them */
    CHECK(nFTCommonStatusEntryNull < nFTCommonStatusSpecialStart);
    CHECK(dFTCommonActionStatusDescs[nFTCommonStatusEntryNull -
              nFTCommonStatusActionStart].proc_update == ftCommonEntryNullProcUpdate);

    /* ---- the sixteen entrance rows. Every one of them is the
     * same shape in the decomp's own status headers: the common update
     * and physics procs and mpCommonUpdateFighterProjectFloor, with the
     * fighter's own motion id and no attack id. ---- */
#define ENTRY_ROW(tbl, status, motion, proc)                                  \
    do {                                                                      \
        row = &(tbl)[(status) - nFTCommonStatusSpecialStart];                 \
        CHECK(row->mflags.motion_id == (motion));                             \
        CHECK(row->mflags.attack_id == nFTMotionAttackIDNone);                \
        CHECK(row->sflags.attack_id == nFTStatusAttackIDNone);                \
        CHECK(row->sflags.ga == nMPKineticsGround);                           \
        CHECK(row->sflags.is_smash_attack == FALSE);                          \
        CHECK(row->sflags.is_projectile == FALSE);                            \
        CHECK(row->proc_update == (proc));                                    \
        CHECK(row->proc_interrupt == NULL);                                   \
        CHECK(row->proc_physics == ftCommonAppearProcPhysics);                \
        CHECK(row->proc_map == mpCommonUpdateFighterProjectFloor);            \
    } while (0)

    ENTRY_ROW(dFTFoxSpecialStatusDescs, nFTFoxStatusAppearR,
              nFTFoxMotionAppearR, ftCommonAppearProcUpdate);
    ENTRY_ROW(dFTFoxSpecialStatusDescs, nFTFoxStatusAppearL,
              nFTFoxMotionAppearL, ftCommonAppearProcUpdate);
    ENTRY_ROW(dFTDonkeySpecialStatusDescs, nFTDonkeyStatusAppearR,
              nFTDonkeyMotionAppearR, ftCommonAppearProcUpdate);
    ENTRY_ROW(dFTDonkeySpecialStatusDescs, nFTDonkeyStatusAppearL,
              nFTDonkeyMotionAppearL, ftCommonAppearProcUpdate);
    ENTRY_ROW(dFTSamusSpecialStatusDescs, nFTSamusStatusAppearR,
              nFTSamusMotionAppearR, ftCommonAppearProcUpdate);
    ENTRY_ROW(dFTSamusSpecialStatusDescs, nFTSamusStatusAppearL,
              nFTSamusMotionAppearL, ftCommonAppearProcUpdate);
    ENTRY_ROW(dFTYoshiSpecialStatusDescs, nFTYoshiStatusAppearR,
              nFTYoshiMotionAppearR, ftCommonAppearProcUpdate);
    ENTRY_ROW(dFTYoshiSpecialStatusDescs, nFTYoshiStatusAppearL,
              nFTYoshiMotionAppearL, ftCommonAppearProcUpdate);
    ENTRY_ROW(dFTKirbySpecialStatusDescs, nFTKirbyStatusAppearR,
              nFTKirbyMotionAppearR, ftCommonAppearProcUpdate);
    ENTRY_ROW(dFTKirbySpecialStatusDescs, nFTKirbyStatusAppearL,
              nFTKirbyMotionAppearL, ftCommonAppearProcUpdate);
    ENTRY_ROW(dFTPurinSpecialStatusDescs, nFTPurinStatusAppearR,
              nFTPurinMotionAppearR, ftCommonAppearProcUpdate);
    ENTRY_ROW(dFTPurinSpecialStatusDescs, nFTPurinStatusAppearL,
              nFTPurinMotionAppearL, ftCommonAppearProcUpdate);
    /* Captain's four: the Start halves are the only Appear rows in the
     * roster with a proc that is not the common one */
    ENTRY_ROW(dFTCaptainSpecialStatusDescs, nFTCaptainStatusAppearRStart,
              nFTCaptainMotionAppearRStart, ftCaptainAppearStartProcUpdate);
    ENTRY_ROW(dFTCaptainSpecialStatusDescs, nFTCaptainStatusAppearLStart,
              nFTCaptainMotionAppearLStart, ftCaptainAppearStartProcUpdate);
    ENTRY_ROW(dFTCaptainSpecialStatusDescs, nFTCaptainStatusAppearREnd,
              nFTCaptainMotionAppearREnd, ftCommonAppearProcUpdate);
    ENTRY_ROW(dFTCaptainSpecialStatusDescs, nFTCaptainStatusAppearLEnd,
              nFTCaptainMotionAppearLEnd, ftCommonAppearProcUpdate);
#undef ENTRY_ROW

    /* ---- the warp-in walked, on Samus. Samus
     * facing left: the setter stashes where she was spawned, zeroes her
     * facing into entry.lr, hides her, and the animation's end puts all
     * three back. ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindSamus;
    fp.lr = -1;
    fp.coll_data.floor_line_id = 7;
    fp_pos.x = 123.0f;
    fp_pos.y = 45.0f;
    fp_pos.z = 0.0f;

    ftCommonAppearSetStatus(mock_gobj);
    CHECK_STATUS(nFTSamusStatusAppearL);
    CHECK(fp.motion_id == nFTSamusMotionAppearL);
    CHECK(fp.lr == 0);                       /* the warp-in has no facing */
    CHECK(fp.status_vars.common.entry.lr == -1);
    CHECK(fp.status_vars.common.entry.floor_line_id == 7);
    CHECK(fp.status_vars.common.entry.entry_wait == FTCOMMON_ENTRY_WAIT);
    CHECK(fp.status_vars.common.entry.is_rotate == FALSE);
    CHECK(fabsf(fp.entry_pos.x - 123.0f) < 1e-3f);
    CHECK(fabsf(fp.entry_pos.y - 45.0f) < 1e-3f);
    CHECK(fp.is_ghost != FALSE);
    CHECK(fp.is_shadow_hide != FALSE);
    CHECK(fp.is_playertag_hide != FALSE);

    /* the model is somewhere else while the animation carries her in */
    fp_pos.x = -900.0f;
    fp_pos.y = -900.0f;
    fp.coll_data.floor_line_id = -1;

    mock_gobj->anim_frame = 5.0f;
    ftCommonAppearProcUpdate(mock_gobj);
    CHECK_STATUS(nFTSamusStatusAppearL);

    mock_gobj->anim_frame = 0.0f;
    ftCommonAppearProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(fp.lr == -1);                      /* and her facing comes back */
    CHECK(fp.coll_data.floor_line_id == 7);
    CHECK(fabsf(fp_pos.x - 123.0f) < 1e-3f);
    CHECK(fabsf(fp_pos.y - 45.0f) < 1e-3f);

    /* ---- Captain Falcon, the only other multi-status entrance, and the
     * only one that moves the fighter between display links: facing left
     * the Blue Falcon drives in from BEHIND the stage, so the setter puts
     * him on link 1 and his own update proc walks him back the moment the
     * car's animation has carried him in front of z = -1000. ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindCaptain;
    fp.lr = -1;
    fp_pos.x = fp_pos.y = fp_pos.z = 0.0f;

    ftCommonAppearSetStatus(mock_gobj);
    CHECK_STATUS(nFTCaptainStatusAppearLStart);
    CHECK(fp.status_vars.common.entry.is_rotate != FALSE);
    CHECK(fp.dl_link == 1);

    /* still behind the stage: he stays on link 1 */
    fp_pos.z = -2000.0f;
    mock_gobj->anim_frame = 5.0f;
    ftCaptainAppearStartProcUpdate(mock_gobj);
    CHECK(fp.dl_link == 1);
    CHECK_STATUS(nFTCaptainStatusAppearLStart);

    /* in front of it: back to the fighters' own link */
    fp_pos.z = 0.0f;
    ftCaptainAppearStartProcUpdate(mock_gobj);
    CHECK(fp.dl_link == FTDISPLAY_DLLINK_DEFAULT);

    /* and the animation's end is the Start->End step, by entry.lr */
    mock_gobj->anim_frame = 0.0f;
    ftCaptainAppearStartProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCaptainStatusAppearLEnd);
    CHECK(fp.is_shadow_hide == FALSE);

    ftMainSetStatus(mock_gobj, nFTCaptainStatusAppearRStart, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    fp.status_vars.common.entry.lr = +1;
    mock_gobj->anim_frame = 0.0f;
    ftCaptainAppearStartProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCaptainStatusAppearREnd);

    /* facing right he arrives in front, so neither the rotation nor the
     * link move happens */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindCaptain;
    fp.lr = +1;
    ftCommonAppearSetStatus(mock_gobj);
    CHECK_STATUS(nFTCaptainStatusAppearRStart);
    CHECK(fp.status_vars.common.entry.is_rotate == FALSE);
    CHECK(fp.dl_link == FTDISPLAY_DLLINK_DEFAULT);

    /* whatever link a status left the fighter on -- the
     * star KO's 19, the entrance's 1 -- the next status puts him back
     * on 9 (ft/ftmain.c:4544-4547), unless he is a menu's Demo fighter */
    ftParamMoveDLLink(mock_gobj, 19);
    CHECK(fp.dl_link == 19 && mock_gobj->dl_link_id == 19);
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.dl_link == FTDISPLAY_DLLINK_DEFAULT);
    CHECK(mock_gobj->dl_link_id == FTDISPLAY_DLLINK_DEFAULT);
    {
        s32 pkind = fp.pkind;

        fp.pkind = nFTPlayerKindDemo;
        ftParamMoveDLLink(mock_gobj, 19);
        ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f,
                        FTSTATUS_PRESERVE_NONE);
        CHECK(fp.dl_link == 19 && mock_gobj->dl_link_id == 19);
        ftParamMoveDLLink(mock_gobj, FTDISPLAY_DLLINK_DEFAULT);
        fp.pkind = pkind;
    }

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindMario;
}

/* dITCapsuleStatusDescs is the decomp's again: a Capsule
 * on the ground, falling or dropped breaks when it is hit, and a thrown
 * or dropped one bounces off a shield. Thirteen of these slots were NULL. */
extern ITStatusDesc dITCapsuleStatusDescs[];
extern sb32 itCapsuleCommonProcHit(GObj *item_gobj);
extern GObj *itCapsuleMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags);
extern void itCapsuleDroppedSetStatus(GObj *item_gobj);

static void test_capsule_status_table(void)
{
    const ITStatusDesc *d = dITCapsuleStatusDescs;
    int st;
    sb32 loaded_here = 0;
    GObj *cg;
    Vec3f pos = { 0.0F, 0.0F, 0.0F };
    Vec3f vel = { 0.0F, 0.0F, 0.0F };

    CHECK(d[0].proc_damage == itCapsuleCommonProcHit);
    CHECK(d[1].proc_damage == itCapsuleCommonProcHit);
    CHECK(d[0].proc_hit == NULL && d[1].proc_hit == NULL);
    CHECK(d[2].proc_damage == NULL && d[5].proc_damage == NULL);
    for (st = 3; st <= 4; st++)
    {
        CHECK(d[st].proc_hit == itCapsuleCommonProcHit);
        CHECK(d[st].proc_shield == itCapsuleCommonProcHit);
        CHECK(d[st].proc_hop == itMainCommonProcHop);
        CHECK(d[st].proc_setoff == itCapsuleCommonProcHit);
        CHECK(d[st].proc_reflector == itCapsuleCommonProcHit);
        CHECK(d[st].proc_damage == itCapsuleCommonProcHit);
    }

    /* ---- and a real one reads them: dropped, it takes the row's procs */
    if (itemPackLoaded() == 0)
    {
        CHECK(itemPackLoad("itcommon.itp") == 0);
        loaded_here = 1;
    }
    cg = itCapsuleMakeItem(NULL, &pos, &vel, 0);
    CHECK(cg != NULL);
    if (cg != NULL)
    {
        ITStruct *ip = itGetStruct(cg);

        itCapsuleDroppedSetStatus(cg);
        CHECK(ip->proc_damage == itCapsuleCommonProcHit);
        CHECK(ip->proc_shield == itCapsuleCommonProcHit);
        CHECK(ip->proc_hop == itMainCommonProcHop);
        itMainDestroyItem(cg);
    }
    if (loaded_here)
    {
        itemPackRelease();
    }
}

/* Mushroom Kingdom's warp pipe. ft/ftcommon/ftcommondokan.c
 * is compiled unmodified, with rows 62-65, and the DownStand and Guard
 * cascades fall through to it again (the test drives DownStand's: the
 * mock cannot hold a shield up without GuardOn's setup). The mock ground gets the three map
 * objects the pipe reads -- both mouths and the wall exit -- the way
 * test_gr_inishie's scale fixture binds its platforms. */
static MPMapObjData sDokanMapObjs[3];

static void test_dokan_pipe(void)
{
    MPGeometryData geo;
    MPGeometryData *saved_geo = gMPCollisionGeometry;
    void *saved_mapobjs = gMPCollisionMapObjs;
    MPYakumonoDObj *saved_yakumono = gMPCollisionYakumonoDObjs;
    Vec3f *saved_speeds = gMPCollisionSpeeds;
    GObj *saved_pakkun[2];
    s32 status_end;

    sDokanMapObjs[0].mapobj_kind = nMPMapObjKindDokanL;
    sDokanMapObjs[0].pos.x = 150;
    sDokanMapObjs[0].pos.y = 0;
    sDokanMapObjs[1].mapobj_kind = nMPMapObjKindDokanR;
    sDokanMapObjs[1].pos.x = 900;
    sDokanMapObjs[1].pos.y = 0;
    sDokanMapObjs[2].mapobj_kind = nMPMapObjKindDokanWall;
    sDokanMapObjs[2].pos.x = -900;
    sDokanMapObjs[2].pos.y = 0;
    geo = mock_geo;
    geo.mapobj_count = 3;
    geo.mapobjs = (void *)sDokanMapObjs;
    mpCollisionLoadGeometry(&geo);

    /* DokanStart tells Mushroom Kingdom's two Piranha Plants; this ground
     * has none, and gGRCommonStruct is a union an earlier stage test
     * left its own vars in */
    saved_pakkun[0] = gGRCommonStruct.inishie.pakkun_gobj[0];
    saved_pakkun[1] = gGRCommonStruct.inishie.pakkun_gobj[1];
    gGRCommonStruct.inishie.pakkun_gobj[0] = NULL;
    gGRCommonStruct.inishie.pakkun_gobj[1] = NULL;

    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindFox;          /* a fighter who turns to face the pipe */

    /* ---- not crouched hard enough, or on a plain floor: nothing ---- */
    fp.coll_data.floor_flags &= ~MAP_VERTEX_MAT_MASK;
    fp.input.pl.stick_range.y = FTCOMMON_DOKAN_STICK_RANGE_MIN;
    fp.tap_stick_y = 0;
    CHECK(ftCommonDokanStartCheckInterruptCommon(mock_gobj) == FALSE);
    fp.coll_data.floor_flags |= nMPMaterialDokanL;
    fp.input.pl.stick_range.y = FTCOMMON_DOKAN_STICK_RANGE_MIN + 1;
    CHECK(ftCommonDokanStartCheckInterruptCommon(mock_gobj) == FALSE);
    fp.input.pl.stick_range.y = FTCOMMON_DOKAN_STICK_RANGE_MIN;
    fp.tap_stick_y = FTCOMMON_DOKAN_BUFFER_TICS_MAX;
    CHECK(ftCommonDokanStartCheckInterruptCommon(mock_gobj) == FALSE);

    /* ---- too far from the mouth ---- */
    fp.tap_stick_y = 0;
    DObjGetStruct(mock_gobj)->translate.vec.f.x = 150.0f + FTCOMMON_DOKAN_DETECT_WIDTH + 1.0f;
    CHECK(ftCommonDokanStartCheckInterruptCommon(mock_gobj) == FALSE);

    /* ---- in reach, through the get-up's cascade (DownStand, flag1
     * set, no jump and no platform to drop through): in he goes ---- */
    DObjGetStruct(mock_gobj)->translate.vec.f.x = 0.0f;
    ftCommonDownStandSetStatus(mock_gobj);
    fp.motion_vars.flags.flag1 = 1;
    fp.coll_data.floor_flags = (fp.coll_data.floor_flags & ~MAP_VERTEX_MAT_MASK) | nMPMaterialDokanL;
    fp.input.pl.stick_range.y = FTCOMMON_DOKAN_STICK_RANGE_MIN;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.button_hold = 0;
    fp.input.pl.button_tap = 0;
    fp.tap_stick_y = 0;
    ftCommonDownStandProcInterrupt(mock_gobj);
    CHECK_STATUS(nFTCommonStatusDokanStart);
    CHECK(fp.status_vars.common.dokan.material == nMPMaterialDokanL);
    CHECK_EQF(fp.status_vars.common.dokan.pos_curr.x, 150.0F);
    CHECK(fp.is_jostle_ignore != FALSE);
    CHECK(fp.status_vars.common.dokan.turn_stop_wait == FTCOMMON_DOKAN_TURN_STOP_WAIT_DEFAULT - 1);

    /* the physics proc walks him onto the mouth, 25 a tic */
    ftCommonDokanStartProcPhysics(mock_gobj);
    CHECK_EQF(DObjGetStruct(mock_gobj)->translate.vec.f.x, 25.0F);
    DObjGetStruct(mock_gobj)->translate.vec.f.x = 140.0f;
    ftCommonDokanStartProcPhysics(mock_gobj);
    CHECK_EQF(DObjGetStruct(mock_gobj)->translate.vec.f.x, 150.0F);

    /* ---- down the pipe: invisible, bound for the other mouth or, a
     * quarter of the time, the wall ---- */
    ftCommonDokanWaitSetStatus(mock_gobj);
    CHECK_STATUS(nFTCommonStatusDokanWait);
    CHECK(fp.is_invisible != FALSE);
    CHECK(fp.ga == nMPKineticsAir);
    if (fp.status_vars.common.dokan.mapobj_kind == nMPMapObjKindDokanR)
    {
        CHECK_EQF(fp.status_vars.common.dokan.target_pos.x, 900.0F);
        status_end = nFTCommonStatusDokanEnd;
    }
    else
    {
        CHECK(fp.status_vars.common.dokan.mapobj_kind == nMPMapObjKindDokanWall);
        status_end = nFTCommonStatusDokanWalk;
    }
    fp.status_vars.common.dokan.pos_adjust_wait = FTCOMMON_DOKAN_POS_ADJUST_WAIT - 1;
    ftCommonDokanWaitProcUpdate(mock_gobj);
    CHECK_STATUS(status_end);
    CHECK_EQF(DObjGetStruct(mock_gobj)->translate.vec.f.x, fp.status_vars.common.dokan.target_pos.x);
    CHECK(fp.status_vars.common.dokan.playertag_wait == FTCOMMON_DOKAN_PLAYERTAG_WAIT);

    /* ---- and the forced wall exit, whichever the roll gave above ---- */
    fp.status_vars.common.dokan.mapobj_kind = nMPMapObjKindDokanWall;
    fp.status_vars.common.dokan.target_pos.x = -900.0F;
    fp.status_vars.common.dokan.target_pos.y = 0.0F;
    fp.status_vars.common.dokan.pos_adjust_wait = FTCOMMON_DOKAN_POS_ADJUST_WAIT - 1;
    ftCommonDokanWaitProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusDokanWalk);
    CHECK(fp.lr == +1);
    CHECK_EQF(DObjGetStruct(mock_gobj)->translate.vec.f.x, -900.0F);

    gGRCommonStruct.inishie.pakkun_gobj[0] = saved_pakkun[0];
    gGRCommonStruct.inishie.pakkun_gobj[1] = saved_pakkun[1];
    gMPCollisionGeometry = saved_geo;
    gMPCollisionMapObjs = saved_mapobjs;
    gMPCollisionYakumonoDObjs = saved_yakumono;
    gMPCollisionSpeeds = saved_speeds;
    fp.fkind = nFTKindMario;
    spawn(0.0f, 0.0f);
}

/* the cargo walk's animation lengths. The pack carries
 * throw_walk{slow,middle,fast}_anim_length (213_DonkeyMain.c:277-279,
 * checked against the ROM by tools/export/ssb_packexport.py) and
 * ftManagerSetupAttributes copies them, so ftDonkeyThrowFWalkProcInterrupt's
 * speed change scales the frame instead of dividing by zero. */
static void test_donkey_cargo_walk_lengths(void)
{
    static FTAttributes attr;
    static FTStruct cargo_fp;
    Fighter donkey;
    int pal_bank = 0;

    memset(&donkey, 0, sizeof(donkey));
    CHECK(fighter_load(&donkey, "donkey.pack", &pal_bank) == 0);
    CHECK(donkey.attr != NULL);
    if (donkey.attr != NULL)
    {
        CHECK_EQF(donkey.attr->throw_walkslow_anim_length, 50.0f);
        CHECK_EQF(donkey.attr->throw_walkmiddle_anim_length, 35.0f);
        CHECK_EQF(donkey.attr->throw_walkfast_anim_length, 20.0f);

        memset(&attr, 0, sizeof(attr));
        ftManagerSetupAttributes(&attr, donkey.attr);
        cargo_fp.attr = &attr;
        CHECK_EQF(ftDonkeyThrowFWalkGetWalkAnimLength(&cargo_fp, nFTDonkeyStatusThrowFWalkSlow), 50.0F);
        CHECK_EQF(ftDonkeyThrowFWalkGetWalkAnimLength(&cargo_fp, nFTDonkeyStatusThrowFWalkMiddle), 35.0F);
        CHECK_EQF(ftDonkeyThrowFWalkGetWalkAnimLength(&cargo_fp, nFTDonkeyStatusThrowFWalkFast), 20.0F);

        /* no aerial Hand Slap: the attribute is what keeps
         * ftCommonSpecialAirCheckInterruptCommon off his NULL slot */
        CHECK(attr.is_have_speciallw == 1 && attr.is_have_specialairlw == 0);
    }
    fighter_release(&donkey);
}

/* the fog a fighter is drawn under: the colour
 * animation's tint and the team shade, as ft/ftdisplaymain.c's three
 * fog functions pick them, from Mario's own shade_color and fog_color
 * carried through the pack. */
extern SYColorRGBA sFTDisplayMainFogColor;
extern u32 gFTDisplayMainLastFog;
static void test_fighter_fog(void)
{
    static FTAttributes attr;
    static FTStruct fog_fp;
    Fighter mario;
    int pal_bank = 0;

    memset(&mario, 0, sizeof(mario));
    CHECK(fighter_load(&mario, "mario.pack", &pal_bank) == 0);
    CHECK(mario.attr != NULL);
    if (mario.attr == NULL)
    {
        return;
    }
    /* relocData/203_MarioMain.c:285-286 */
    CHECK(mario.attr->shade_color[0] == 0xFF && mario.attr->shade_color[3] == 0x50);
    CHECK(mario.attr->shade_color[4] == 0x00 && mario.attr->shade_color[7] == 0x50);
    CHECK(mario.attr->fog_color[0] == 0xFF && mario.attr->fog_color[3] == 0x00);

    memset(&attr, 0, sizeof(attr));
    ftManagerSetupAttributes(&attr, mario.attr);
    CHECK(attr.shade_color[0].r == 0xFF && attr.shade_color[0].a == 0x50);
    CHECK(attr.shade_color[1].r == 0x00 && attr.shade_color[1].a == 0x50);
    CHECK(attr.fog_color.r == 0xFF);

    memset(&fog_fp, 0, sizeof(fog_fp));
    fog_fp.attr = &attr;

    /* no shade, no tint: no fog */
    gFTDisplayMainLastFog = 0xDEADBEEF;
    ftDisplayMainDecideFogColor(&fog_fp);
    CHECK(gFTDisplayMainLastFog == 0);

    /* no shade, a half red tint: the tint itself */
    fog_fp.colanim.color1.r = 0xFF;
    fog_fp.colanim.color1.a = 0x80;
    ftDisplayMainCalcFogColor(&fog_fp);
    ftDisplayMainSetFogColor(&fog_fp);
    CHECK(gFTDisplayMainLastFog == 0xFF000080);

    /* the second of a team's two Marios: shade 1, white at 0x50. Without
     * a tint the fog is the shade; with a transparent tint the fold comes
     * out at the same colour (ftmanager.c:709-711's pre-multiplied
     * shade_color over a shade_base of 0x50) */
    fog_fp.shade = 1;
    fog_fp.shade_color.r = fog_fp.shade_color.g = fog_fp.shade_color.b = (0xFF * 0x50) / 0xFF;
    ftDisplayMainDecideFogColor(&fog_fp);
    CHECK(gFTDisplayMainLastFog == 0xFFFFFF50);
    fog_fp.colanim.color1.r = 0x00;
    fog_fp.colanim.color1.a = 0x00;
    ftDisplayMainCalcFogColor(&fog_fp);
    CHECK(sFTDisplayMainFogColor.r == 0xFF && sFTDisplayMainFogColor.g == 0xFF);
    CHECK(sFTDisplayMainFogColor.b == 0xFF && sFTDisplayMainFogColor.a == 0x50);

    /* a fog_color.r under 0xFF scales the tint's alpha */
    fog_fp.shade = 0;
    attr.fog_color.r = 0x80;
    fog_fp.colanim.color1.r = 0xFF;
    fog_fp.colanim.color1.a = 0xFF;
    ftDisplayMainCalcFogColor(&fog_fp);
    CHECK(sFTDisplayMainFogColor.a == 0x80);

    fighter_release(&mario);
}

/* Samus's rolls: the one spline table a figatree names (SetTranslateInterp,
 * ft/ftanim.c:350) carried native -- the SYInterpDesc and its three arrays
 * rewritten for a 32-bit little-endian reader, the pointers as word
 * indices for fighter_init to relocate (a 64-bit host does not; its
 * SYInterpDesc is wider). Read as the u16 words the rest of the figatree
 * is, the keyframes never passed t and syInterpGetFracFrame walked
 * megabytes a frame -- 17 ms each, Samus's every roll. The table is
 * rebuilt host-wide here and played through the decomp's sys/interp.c. */
static void test_samus_roll_spline(void)
{
    Fighter samus;
    int pal_bank = 0;
    uint32_t i;
    int found = 0;

    memset(&samus, 0, sizeof(samus));
    CHECK(fighter_load(&samus, "samus.pack", &pal_bank) == 0);
    for (i = 0; (samus.blob != NULL) && (i < samus.hd->anim_count); i++)
    {
        const FPackAnim *a = &samus.anims[i];
        uint8_t *base = (uint8_t *)fighter_anim_words(&samus, a);
        const uint8_t *d = base + 0x118;
        const uint32_t *rl = (const uint32_t *)((uint8_t *)samus.blob + a->off_reloc);
        SYInterpDesc desc;
        uint32_t ptr[3];
        int16_t points_num;
        Vec3f out;
        f32 frac;

        if (strcmp(a->name, "FTSamusAnimRollF") != 0)
        {
            continue;
        }
        found = 1;
        CHECK(a->kind == FPACK_ANIM_FIGATREE);
        CHECK(a->nreloc == 3);
        if (a->nreloc != 3)
        {
            break;
        }
        CHECK(rl[0] == 0x118 / 2 + 4 && rl[1] == 0x118 / 2 + 8 && rl[2] == 0x118 / 2 + 10);

        memset(&desc, 0, sizeof(desc));
        desc.kind = d[0];
        memcpy(&points_num, d + 2, 2);
        desc.points_num = points_num;
        memcpy(&desc.unk04, d + 4, 4);
        memcpy(&ptr[0], d + 8, 4);
        memcpy(&desc.length, d + 12, 4);
        memcpy(&ptr[1], d + 16, 4);
        memcpy(&ptr[2], d + 20, 4);
        CHECK(desc.kind == nSYInterpKindBezier);
        CHECK(desc.points_num == 5);
        CHECK_NEAR(desc.length, 38.56186f, 1e-4f);
        CHECK(ptr[0] == 0x60 / 2 && ptr[1] == 0xB4 / 2 && ptr[2] == 0xC8 / 2);
        if (ptr[1] != 0xB4 / 2)
        {
            break;
        }
        desc.points = (Vec3f *)(base + ptr[0] * 2);
        desc.keyframes = (f32 *)(base + ptr[1] * 2);
        desc.quartics = (f32 *)(base + ptr[2] * 2);
        CHECK_EQF(desc.keyframes[0], 0.0F);
        CHECK_NEAR(desc.keyframes[1], 0.087792f, 1e-6f);
        CHECK_EQF(desc.keyframes[4], 1.0F);
        CHECK_NEAR(desc.points[0].x, 93.83934f, 1e-3f);

        frac = syInterpGetFracFrame(&desc, 0.5F);
        CHECK(frac > 0.0F && frac < 1.0F);
        /* the curve sys/interp.c makes of it: arc-length even, from the
         * second control point to 1087.7 forward, 143 units an eighth */
        syInterpCubic(&out, &desc, 0.0F);
        CHECK_NEAR(out.x, 0.672566f, 0.01f);
        CHECK_NEAR(out.z, -0.743516f, 0.01f);
        syInterpCubic(&out, &desc, 0.5F);
        CHECK_NEAR(out.x, -88.3005f, 0.05f);
        CHECK_NEAR(out.z, 521.146f, 0.05f);
        syInterpCubic(&out, &desc, 1.0F);
        CHECK_NEAR(out.x, -0.364328f, 0.05f);
        CHECK_NEAR(out.z, 1087.676f, 0.05f);
    }
    CHECK(found);
    fighter_release(&samus);
}

/* The .anm in tiers (fighter.h FPackAttr.anm_tier_end, ftcommon.h
 * ftManagerSetAnmTier): Mario read to tier 0, as a character select reads
 * him, holds the pose the select plays and not his walk. Binding the walk
 * through a clone -- which is how a fighter reaches its pack -- reads the
 * rest of the file into the pack, keeps the prefix for whatever still
 * plays from it, and lands on the words the whole file has. */
static void test_anm_tiers(void)
{
    Fighter full, part, clone;
    const FPackAnim *sel = NULL, *walk = NULL;
    int pal_bank = 0;
    void *prefix;
    uint32_t i;

    memset(&full, 0, sizeof(full));
    memset(&part, 0, sizeof(part));
    CHECK(fighter_load(&full, "mario.pack", &pal_bank) == 0);
    pal_bank = 0;
    CHECK(fighter_load_tier(&part, "mario.pack", &pal_bank, 0) == 0);
    if (full.hd == NULL || part.hd == NULL)
    {
        return;
    }
    for (i = 0; i < part.hd->anim_count; i++)
    {
        if (strcmp(part.anims[i].name, "FTMarioAnimSelected") == 0)
        {
            sel = &part.anims[i];
        }
        if (strcmp(part.anims[i].name, "FTMarioAnimWalk1") == 0)
        {
            walk = &part.anims[i];
        }
    }
    CHECK(sel != NULL && walk != NULL);
    if (sel == NULL || walk == NULL)
    {
        fighter_release(&part);
        fighter_release(&full);
        return;
    }
    CHECK(full.anm_size == sizeof(FPackAnm) + full.attr->anm_size);
    CHECK(part.anm_size == sizeof(FPackAnm) + part.attr->anm_tier_end[0]);
    CHECK(part.anm_size < full.anm_size);
    CHECK(fighter_anim_words(&part, sel) != NULL);
    CHECK(fighter_anim_words(&part, walk) == NULL);
    /* a figatree without splines has no pointer to relocate: its words
     * are the file's, wherever they were read to */
    CHECK(sel->kind == FPACK_ANIM_FIGATREE && sel->nreloc == 0);
    CHECK(memcmp(fighter_anim_words(&part, sel), fighter_anim_words(&full, sel),
                 fighter_anim_bytes(sel)) == 0);

    fighter_clone(&clone, &part, NULL);
    prefix = part.anm;
    CHECK(fighter_anim_words_need(&clone, walk) != NULL);
    CHECK(part.anm_retired == prefix && part.anm != prefix);
    CHECK(part.anm_size == full.anm_size);
    CHECK(clone.anm_retired == NULL && clone.anm_owned == 0);
    CHECK(walk->kind == FPACK_ANIM_FIGATREE && walk->nreloc == 0);
    CHECK(memcmp(fighter_anim_words(&clone, walk), fighter_anim_words(&full, walk),
                 fighter_anim_bytes(walk)) == 0);
    CHECK(fighter_anim_words(&clone, sel) != NULL);
    /* grown already: asking for tier 1 reads nothing */
    CHECK(fighter_anm_grow(&part, 1, 0) == 0 && part.anm_retired == prefix);

    fighter_release(&part);
    fighter_release(&full);
}

/* Luigi's translate_scales. The pack carries the table
 * (relocData/221_LuigiMain.c:194, checked against the ROM by
 * tools/export/ssb_packexport.py), ftManagerSetupAttributes points FTAttributes
 * at it, ftMainSetStatus turns it on per motion, and ftParamUpdateAnimKeys
 * (verbatim) plays the translation tracks through
 * lbCommonPlayTranslateScaledDObjAnim. */
static void test_luigi_translate_scales(void)
{
    static Vec3f scales[FTPARTS_JOINT_NUM_MAX];
    static FTAttributes attr;
    Fighter luigi, mario;
    int pal_bank = 0;
    DObj *hip;
    AObj *aobj;
    Vec3f *saved_scales;
    int i;

    /* ---- the packs ---- */
    memset(&luigi, 0, sizeof(luigi));
    memset(&mario, 0, sizeof(mario));
    CHECK(fighter_load(&luigi, "luigi.pack", &pal_bank) == 0);
    pal_bank = 0;
    CHECK(fighter_load(&mario, "mario.pack", &pal_bank) == 0);
    if ((luigi.attr != NULL) && (mario.attr != NULL))
    {
        CHECK(luigi.attr->is_have_translate_scales == 1);
        CHECK(mario.attr->is_have_translate_scales == 0);
        CHECK_NEAR(luigi.attr->translate_scales[3][1], 1.1379f, 1e-5f);
        CHECK_NEAR(luigi.attr->translate_scales[7][1], 1.1618f, 1e-5f);
        CHECK_NEAR(luigi.attr->translate_scales[11][1], 1.2488f, 1e-5f);
        CHECK_NEAR(luigi.attr->translate_scales[12][1], 2.886f, 1e-5f);
        CHECK_NEAR(luigi.attr->translate_scales[13][1], 1.1618f, 1e-5f);
        CHECK_EQF(luigi.attr->translate_scales[12][0], 1.0f);
        CHECK_EQF(luigi.attr->translate_scales[4][1], 1.0f);
        /* past the file's 29 rows, identity */
        CHECK_EQF(luigi.attr->translate_scales[30][1], 1.0f);

        ftManagerSetupAttributes(&attr, luigi.attr);
        CHECK(attr.translate_scales != NULL);
        if (attr.translate_scales != NULL)
        {
            CHECK_NEAR(attr.translate_scales[12].y, 2.886f, 1e-5f);
        }
        ftManagerSetupAttributes(&attr, mario.attr);
        CHECK(attr.translate_scales == NULL);
    }
    fighter_release(&luigi);
    fighter_release(&mario);

    /* ---- the play: a hip whose Y track reads 100 lands at 200 under a
     * x2 row, and at 100 on a motion that turns the scaling off ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    hip = fp.joints[nFTPartsJointCommonStart];
    CHECK(hip != NULL);
    if (hip == NULL)
    {
        return;
    }
    for (i = 0; i < FTPARTS_JOINT_NUM_MAX; i++)
    {
        scales[i].x = scales[i].y = scales[i].z = 1.0F;
    }
    scales[nFTPartsJointCommonStart].y = 2.0F;
    saved_scales = fp.attr->translate_scales;
    fp.attr->translate_scales = scales;

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.is_have_translate_scale == TRUE);

    aobj = gcAddAObjForDObj(hip, nGCAnimTrackTraY);
    aobj->kind = nGCAnimKindLinear;
    aobj->value_base = 100.0F;
    aobj->rate_base = 0.0F;
    aobj->length = 0.0F;
    hip->anim_wait = 1.0F;
    ftParamUpdateAnimKeys(mock_gobj);
    CHECK_EQF(hip->translate.vec.f.y, 200.0F);

    fp.is_have_translate_scale = FALSE;
    hip->anim_wait = 1.0F;
    ftParamUpdateAnimKeys(mock_gobj);
    CHECK_EQF(hip->translate.vec.f.y, 100.0F);

    /* a fighter without the table is never switched on */
    fp.attr->translate_scales = NULL;
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    CHECK(fp.is_have_translate_scale == FALSE);

    gcRemoveAObjFromDObj(hip);
    fp.attr->translate_scales = saved_scales;
    spawn(0.0f, 0.0f);
}

/* ftParamUpdateDamage's tail: a hit shakes a held item
 * loose when its damage beats rand(60), and a knockback-free hit never
 * does. A Capsule is a throwing item, so the empty-shooter roll never
 * fires for it. */
static void test_hit_knocks_item_loose(void)
{
    sb32 loaded_here = 0;
    GObj *cg;
    Vec3f pos = { 0.0F, 0.0F, 0.0F };
    Vec3f vel = { 0.0F, 0.0F, 0.0F };

    if (itemPackLoaded() == 0)
    {
        CHECK(itemPackLoad("itcommon.itp") == 0);
        loaded_here = 1;
    }
    spawn(0.0f, 0.0f);

    cg = itCapsuleMakeItem(NULL, &pos, &vel, 0);
    CHECK(cg != NULL);
    if (cg == NULL)
    {
        goto done;
    }
    itMainSetFighterHold(cg, mock_gobj);
    CHECK(fp.item_gobj == cg);

    /* no knockback: the tail's own guard keeps the item, even at 60 */
    fp.damage_knockback = 0.0F;
    fp.hitlag_tics = 0;
    ftParamUpdateDamage(fpp, 60);
    CHECK(fp.item_gobj == cg);
    CHECK(fp.percent_damage == 60);

    /* a zero-damage hit with knockback: rand(60) is never below 0 */
    fp.damage_knockback = 40.0F;
    ftParamUpdateDamage(fpp, 0);
    CHECK(fp.item_gobj == cg);

    /* 60 beats every rand(60) roll: it drops */
    ftParamUpdateDamage(fpp, 60);
    CHECK(fp.item_gobj == NULL);
    CHECK(itGetStruct(cg)->is_hold == FALSE);
    CHECK(fp.percent_damage == 120);

    itMainDestroyItem(cg);

done:
    spawn(0.0f, 0.0f);
    if (loaded_here)
    {
        itemPackRelease();
    }
}

/* the copy power a Kirby spawns with. ftManagerInitFighter's
 * Kirby and Link arms (ft/ftmanager.c:613-643) are back, and
 * dFTManagerDefaultFighterDesc is read off the ROM field by field: it
 * was a byte late, which made copy_kind 0 -- Mario -- for every VS
 * spawn and every rebirth. The taunt's lose-copy arm is back too. */
extern void ftKirbyMainMotionBindOffsets(void);
extern int llKirbyMainMotionSpecialNFTKirbyCopy;

static void test_kirby_spawn_copy(void)
{
    FTDesc desc;
    FTKirbyCopy *copy;

    /* ---- the ROM's default description ---- */
    CHECK(dFTManagerDefaultFighterDesc.lr == +1);
    CHECK(dFTManagerDefaultFighterDesc.team == 0);
    CHECK(dFTManagerDefaultFighterDesc.detail == nFTPartsDetailHigh);
    CHECK(dFTManagerDefaultFighterDesc.costume == 0);
    CHECK(dFTManagerDefaultFighterDesc.handicap == 9);
    CHECK(dFTManagerDefaultFighterDesc.level == 3);
    CHECK(dFTManagerDefaultFighterDesc.stock_count == 0);
    CHECK(dFTManagerDefaultFighterDesc.unk_rebirth_0x1D == 3);
    CHECK(dFTManagerDefaultFighterDesc.team_order == 0);
    CHECK(dFTManagerDefaultFighterDesc.copy_kind == nFTKindKirby);
    CHECK(dFTManagerDefaultFighterDesc.pkind == nFTPlayerKindDemo);

    if (gFTDataKirbyMainMotion == NULL)
    {
        ftKirbyMainMotionBindOffsets();
    }
    copy = lbRelocGetFileData(FTKirbyCopy*, gFTDataKirbyMainMotion, &llKirbyMainMotionSpecialNFTKirbyCopy);

    /* ---- a Kirby made or reborn from it is plain Kirby, and one that
     * swallowed Fox and was KO'd loses Fox ---- */
    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindKirby;
    fp.passive_vars.kirby.copy_id = nFTKindFox;
    fp.passive_vars.kirby.is_ignore_losecopy = TRUE;
    fp.modelpart_status[FTKIRBY_COPY_MODELPARTS_JOINT - nFTPartsJointCommonStart].modelpart_id_base = 99;
    desc = dFTManagerDefaultFighterDesc;
    desc.pos = DObjGetStruct(mock_gobj)->translate.vec.f;
    ftManagerInitFighter(mock_gobj, &desc);
    CHECK(fp.passive_vars.kirby.copy_id == nFTKindKirby);
    CHECK(fp.passive_vars.kirby.is_ignore_losecopy == FALSE);
    CHECK(fp.modelpart_status[FTKIRBY_COPY_MODELPARTS_JOINT - nFTPartsJointCommonStart].modelpart_id_base ==
          copy[nFTKindKirby].copy_modelpart_id);

    /* ---- a Kirby made with a power keeps it through a taunt ---- */
    desc.copy_kind = nFTKindSamus;
    ftManagerInitFighter(mock_gobj, &desc);
    CHECK(fp.passive_vars.kirby.copy_id == nFTKindSamus);
    CHECK(fp.passive_vars.kirby.is_ignore_losecopy == TRUE);
    CHECK(fp.modelpart_status[FTKIRBY_COPY_MODELPARTS_JOINT - nFTPartsJointCommonStart].modelpart_id_base ==
          copy[nFTKindSamus].copy_modelpart_id);
    ftCommonAppealSetStatus(mock_gobj);
    CHECK_STATUS(nFTCommonStatusAppeal);
    CHECK(fp.passive_vars.kirby.copy_id == nFTKindSamus);

    /* ---- one he swallowed, he drops ---- */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.kirby.copy_id = nFTKindFox;
    fp.passive_vars.kirby.is_ignore_losecopy = FALSE;
    ftCommonAppealSetStatus(mock_gobj);
    CHECK_STATUS(nFTCommonStatusAppeal);
    CHECK(fp.passive_vars.kirby.copy_id == nFTKindKirby);

    /* ---- a non-Kirby's passive vars are left alone ---- */
    fp.fkind = nFTKindMario;
    fp.passive_vars.kirby.copy_id = nFTKindFox;
    ftCommonAppealSetStatus(mock_gobj);
    CHECK(fp.passive_vars.kirby.copy_id == nFTKindFox);

    /* ---- Link's arm: the empty hand and no boomerang ---- */
    fp.fkind = nFTKindLink;
    fp.passive_vars.link.boomerang_gobj = mock_gobj;
    fp.modelpart_status[21 - nFTPartsJointCommonStart].modelpart_id_base = 5;
    fp.modelpart_status[19 - nFTPartsJointCommonStart].modelpart_id_base = 5;
    ftManagerInitFighter(mock_gobj, &desc);
    CHECK(fp.passive_vars.link.boomerang_gobj == NULL);
    CHECK(fp.modelpart_status[21 - nFTPartsJointCommonStart].modelpart_id_base == -1);
    CHECK(fp.modelpart_status[19 - nFTPartsJointCommonStart].modelpart_id_base == 0);

    fp.fkind = nFTKindMario;
    spawn(0.0f, 0.0f);
}

/* Link's Bomb: ft/ftchar/ftlink/ftlinkspeciallw.c and
 * it/itfighter/itlinkbomb.c, compiled unmodified. The three tables it
 * reads out of LinkMain have no initializer in the decomp, so they come
 * off the ROM: its ITAttributes (an item-pack record, key 0x40) and its
 * attack events and bloat scales (raw tables in LinkMain's wpattrs blob). */
extern ITDesc dItLinkBombItemDesc;
extern void *gFTDataLinkMain;
extern int llLinkMainBombAttackEvents;
extern int llLinkMainBombBloatScales;

static GObj *test_link_find_bomb(void)
{
    GObj *g;

    for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL; g = g->link_next)
    {
        if (itGetStruct(g)->kind == nITKindLinkBomb)
        {
            return g;
        }
    }
    return NULL;
}

static void test_link_bomb(void)
{
    ITAttributes *attr;
    ITAttackEvent *ev;
    f32 *scales;
    GObj *bg;
    ITStruct *ip;
    sb32 loaded_here = 0;
    int i;

    if (itemPackLoaded() == 0)
    {
        CHECK(itemPackLoad("itcommon.itp") == 0);
        loaded_here = 1;
    }
    CHECK(gFTDataLinkMain != NULL);
    if (gFTDataLinkMain == NULL)
    {
        goto done;
    }

    /* ---- the ROM's tables, through the decomp's own arithmetic ---- */
    CHECK(dItLinkBombItemDesc.o_attributes == (intptr_t)0x40);
    attr = itemPackAttr(gFTDataLinkMain, 0x40);
    CHECK(attr != NULL);
    if (attr == NULL)
    {
        goto done;
    }
    CHECK(attr->data != NULL);
    CHECK(attr->map_coll_top == 113 && attr->map_coll_bottom == -113);
    CHECK(attr->size == 220);
    CHECK(attr->angle == 80 && attr->damage == 2);
    CHECK(attr->knockback_base == 60);
    CHECK(attr->type == nITTypeThrow);
    CHECK(attr->vel_scale == 60);
    CHECK(attr->is_display_colanim && attr->is_give_hitlag);

    ev = itGetAttackEvent(dItLinkBombItemDesc, &llLinkMainBombAttackEvents);
    for (i = 0; i < 4; i++)
    {
        CHECK(ev[i].timer == i * 2);
        CHECK(ev[i].angle == 361);
        CHECK(ev[i].damage == 5);
    }
    CHECK(ev[0].size == 300 && ev[1].size == 230 && ev[2].size == 150 && ev[3].size == 0);
    scales = (f32*)((uintptr_t)gFTDataLinkMain + (intptr_t)&llLinkMainBombBloatScales);
    CHECK_NEAR(scales[0], 0.8f, 1e-5f);
    CHECK_NEAR(scales[1], 1.0f, 1e-5f);
    CHECK_NEAR(scales[5], 1.8f, 1e-5f);

    /* ---- Down-B pulls one out into his hand ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindLink;
    CHECK(test_link_find_bomb() == NULL);
    ftLinkSpecialLwSetStatus(mock_gobj);
    CHECK_STATUS(nFTLinkStatusSpecialLw);
    fp.motion_vars.flags.flag0 = 1;
    ftLinkSpecialLwProcUpdate(mock_gobj);
    CHECK(fp.motion_vars.flags.flag0 == 0);
    bg = test_link_find_bomb();
    CHECK(bg != NULL);
    if (bg == NULL)
    {
        goto done;
    }
    ip = itGetStruct(bg);
    CHECK(fp.item_gobj == bg);
    CHECK(ip->is_hold != FALSE);
    CHECK(ip->owner_gobj == mock_gobj);
    CHECK(ip->lifetime == ITLINKBOMB_LIFETIME);
    CHECK(DObjGetStruct(bg)->child != NULL);

    /* ---- held, it swells through the bloat scales ---- */
    ip->lifetime = ITLINKBOMB_BLOAT_BEGIN;
    CHECK(itLinkBombHoldProcUpdate(bg) == FALSE);
    CHECK(ip->item_vars.linkbomb.scale_id == 1);
    ip->item_vars.linkbomb.scale_int = 0;
    CHECK(itLinkBombHoldProcUpdate(bg) == FALSE);
    CHECK(DObjGetStruct(bg)->child != NULL &&
          DObjGetStruct(bg)->child->scale.vec.f.x == scales[1]);
    CHECK(ip->item_vars.linkbomb.scale_id == 2);

    /* ---- and at zero it goes off in his hand: the explosion's hitbox
     * grows and shrinks through the four attack events ---- */
    ip->lifetime = 0;
    CHECK(itLinkBombHoldProcUpdate(bg) == FALSE);
    CHECK(fp.item_gobj == NULL);
    CHECK(ip->attack_coll.damage == 5);
    CHECK(ip->attack_coll.angle == 361);
    CHECK(ip->attack_coll.size == 300.0f);
    CHECK(ip->attack_coll.element == nGMHitElementFire);
    CHECK(ip->attack_coll.fgm_id == nSYAudioFGMExplodeL);
    for (i = 0; i < 2; i++)
    {
        CHECK(itLinkBombExplodeProcUpdate(bg) == FALSE);
    }
    CHECK(itLinkBombExplodeProcUpdate(bg) == FALSE);
    CHECK(ip->attack_coll.size == 230.0f);
    for (i = 3; i < ITLINKBOMB_EXPLODE_LIFETIME - 1; i++)
    {
        CHECK(itLinkBombExplodeProcUpdate(bg) == FALSE);
    }
    CHECK(itLinkBombExplodeProcUpdate(bg) == TRUE);
    itMainDestroyItem(bg);
    CHECK(test_link_find_bomb() == NULL);

    /* ---- and a second bomb costs the scene heap nothing: the first
     * one's DObjs and MObjs are back in their pools, and the model's
     * joint payloads are shared (src/dc/objmodel.c). It used to cut a
     * fresh set out of a heap that only empties between scenes, for
     * every item, weapon and effect a match made. ---- */
    {
        size_t heap_used = syTaskmanGeneralHeapUsed();

        fp.motion_vars.flags.flag0 = 1;
        ftLinkSpecialLwProcUpdate(mock_gobj);
        bg = test_link_find_bomb();
        CHECK(bg != NULL);
        if (bg != NULL)
        {
            CHECK(DObjGetStruct(bg)->child != NULL);
            itMainDestroyItem(bg);
        }
        CHECK(syTaskmanGeneralHeapUsed() == heap_used);
    }

done:
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindMario;
    if (loaded_here)
    {
        itemPackRelease();
    }
}

/* Ness's specials, compiled unmodified: PK Fire
 * (ftnessspecialn.c, wpnesspkfire.c and the pillar it/itfighter/
 * itnesspkfire.c), PK Thunder (ftnessspecialhi.c, wpnesspkthunder.c) and
 * PSI Magnet (ftnessspeciallw.c), plus Kirby's copied PK Fire. The rows
 * are status_check.py's to compare; this walks what they do. */
static GObj *test_ness_find_pkfire(void)
{
    GObj *g;

    for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL; g = g->link_next)
    {
        if (itGetStruct(g)->kind == nITKindNessPKFire)
        {
            return g;
        }
    }
    return NULL;
}

static void test_ness_specials(void)
{
    extern FTStatusDesc dFTNessSpecialStatusDescs[];
    extern void *gFTNessFileSpecial1;
    extern void *gFTNessFileMainMotion;
    GObj *wg, *ig;
    WPStruct *wp;
    ITStruct *ip;
    ITAttributes *attr;
    DObj *saved_joint = NULL;
    DObj *saved_joint4 = NULL;
    sb32 is_joints_swapped = FALSE;
    sb32 loaded_here = 0;
    int i;

    for (i = nFTNessStatusSpecialN; i <= nFTNessStatusSpecialAirLwEnd; i++)
    {
        FTStatusDesc *row = &dFTNessSpecialStatusDescs[i - nFTCommonStatusSpecialStart];

        CHECK(row->proc_update != NULL);
        CHECK(row->proc_physics != NULL);
        CHECK(row->proc_map != NULL);
    }

    /* ---- the four packs the moves draw with, read back the way
     * efModelLoad and item_model_load read them. The bubble and the wave
     * each flip one MObj between two sprites; the trail end is a lone quad
     * with nothing to animate; the pillar is two drawing joints under a
     * stand and a root, with its AnimJoint table carried along. */
    {
        static const struct
        {
            const char *pack;
            u32 joints, tris, batches, texs, anims;
            s32 mobj_texs;              /* -1: no FPackMObjs section */
        } kPacks[] =
        {
            { "efpsimagnet.mdl",  2,  2, 1, 2, 1,  2 },
            { "efpkwave.mdl",     3,  2, 1, 2, 1,  2 },
            { "efpktrail.mdl",    1,  2, 1, 1, 0, -1 },
            { "itnesspkfire.mdl", 4, 10, 3, 2, 1, -1 },
        };
        u32 k;

        for (k = 0; k < ARRAY_COUNT(kPacks); k++)
        {
            Fighter pack;
            int pal_bank = 0;

            memset(&pack, 0, sizeof(pack));
            CHECK(fighter_load(&pack, kPacks[k].pack, &pal_bank) == 0);

            if (pack.hd == NULL)
            {
                continue;
            }
            CHECK(pack.hd->joint_count == kPacks[k].joints);
            CHECK(pack.hd->tri_count == kPacks[k].tris);
            CHECK(pack.hd->batch_count == kPacks[k].batches);
            CHECK(pack.hd->tex_count == kPacks[k].texs);
            CHECK(pack.hd->anim_count == kPacks[k].anims);
            CHECK((pack.mobjs != NULL) == (kPacks[k].mobj_texs >= 0));

            if ((pack.mobjs != NULL) && (kPacks[k].mobj_texs >= 0))
            {
                CHECK(pack.mobjs->mobj_count == 1);
                CHECK(pack.mobj_subs[0].tex_count == kPacks[k].mobj_texs);
            }
            fighter_release(&pack);
        }
    }
    if (itemPackLoaded() == 0)
    {
        CHECK(itemPackLoad("itcommon.itp") == 0);
        loaded_here = 1;
    }

    /* ---- PK Fire: the setter arms the accessory, and the motion's flag
     * throws the spark forward and slightly down ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindNess;
    fp.lr = +1;
    ftNessSpecialNSetStatus(mock_gobj);
    CHECK_STATUS(nFTNessStatusSpecialN);
    CHECK(fp.proc_accessory == ftNessSpecialNProcAccessory);
    CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);

    fp.motion_vars.flags.flag0 = TRUE;
    ftNessSpecialNProcAccessory(mock_gobj);
    CHECK(fp.motion_vars.flags.flag0 == FALSE);
    wg = gGCCommonLinks[nGCCommonLinkIDWeapon];
    CHECK(wg != NULL);
    if (wg == NULL)
    {
        goto done;
    }
    wp = wpGetStruct(wg);
    CHECK(wp->kind == nWPKindPKFire);
    CHECK(wp->owner_gobj == mock_gobj);
    CHECK(wp->lifetime == WPPKFIRE_LIFETIME);
    CHECK_NEAR(wp->physics.vel_air.x, __cosf(FTNESS_PKFIRE_SPARK_ANGLE_GROUND) * FTNESS_PKFIRE_SPARK_VEL_GROUND, 1e-3f);
    CHECK(wp->physics.vel_air.y < 0.0f);
    /* NessSpecial1's spark table, bound by wpattrs.c (US: 4 damage) */
    CHECK(wp->attack_coll.damage == 4);
    CHECK(wp->attack_coll.can_reflect && wp->attack_coll.can_absorb);

    /* ---- the spark connects: the pillar is an ITEM, its table found in
     * NessSpecial1 at 0x34 through itemPackAttr ---- */
    attr = itemPackAttr(gFTNessFileSpecial1, 0x34);
    CHECK(attr != NULL);
    CHECK(test_ness_find_pkfire() == NULL);
    CHECK(wpNessPKFireProcHit(wg) == TRUE);
    wpMainDestroyWeapon(wg);
    ig = test_ness_find_pkfire();
    CHECK(ig != NULL);
    if (ig == NULL)
    {
        goto done;
    }
    ip = itGetStruct(ig);
    CHECK(ip->kind == nITKindNessPKFire);
    CHECK(ip->attr == attr);
    CHECK(ip->owner_gobj == mock_gobj);
    CHECK(ip->is_allow_pickup == FALSE);
    CHECK(ip->lifetime == ITPKFIRE_LIFETIME);
    /* the US half of the table's #if: 3 damage, angle 70, base 4 */
    CHECK(ip->attack_coll.damage == 3);
    CHECK(ip->attack_coll.angle == 70);
    CHECK(ip->attack_coll.knockback_base == 4);
    CHECK(ip->attack_coll.attack_count == 2);
    /* the pillar's `data` is the stage items' marker, and its tree is
     * itnesspkfire.mdl: joints 0-3 of NessSpecial3's DObjDesc, and after
     * itManagerMakeItem's eject the root is joint 1 with the two drawing
     * joints under it. Its AnimJoint table rides in the pack, which the
     * host does not walk (itemModelAnimJoints), so nothing plays here. */
    CHECK(attr->data != NULL);
    CHECK(attr->anim_joints == NULL);
    CHECK(DObjGetStruct(ig) != NULL);
    if (DObjGetStruct(ig) != NULL)
    {
        CHECK(DObjGetStruct(ig)->parent == DOBJ_PARENT_NULL);
        CHECK(DObjGetStruct(ig)->child != NULL);
        CHECK((DObjGetStruct(ig)->child != NULL) &&
              (DObjGetStruct(ig)->child->sib_next != NULL));
        CHECK(DObjGetStruct(ig)->dv != NULL);
    }
    CHECK(itemModelAnimJoints(0x34) == NULL);

    /* its first update drops it into the fall, and every update shrinks
     * the pillar with its lifetime, hitbox and all */
    CHECK(ip->proc_update(ig) == FALSE);
    CHECK(ip->ga == nMPKineticsAir);
    CHECK(itNessPKFireFallProcUpdate(ig) == FALSE);
    CHECK(ip->lifetime == ITPKFIRE_LIFETIME - 1);
    CHECK_NEAR(DObjGetStruct(ig)->scale.vec.f.x, 1.0f, 1e-5f);
    /* hurt, it burns out ITPKFIRE_HURT_DAMAGE_MUL times the damage faster */
    ip->lifetime = 50;
    ip->damage_highest = 10;
    CHECK(itNessPKFireCommonProcDamage(ig) == FALSE);
    CHECK(ip->lifetime == 50 - (10 * ITPKFIRE_HURT_DAMAGE_MUL) - 1);
    /* spent, it is at half size and asks to be destroyed */
    ip->lifetime = 0;
    CHECK(itNessPKFireFallProcUpdate(ig) == TRUE);
    CHECK_NEAR(DObjGetStruct(ig)->scale.vec.f.x, 0.5f, 1e-5f);
    CHECK_NEAR(ip->attack_coll.size, attr->size * 0.5f * 0.5f, 1e-4f);
    itMainDestroyItem(ig);
    CHECK(test_ness_find_pkfire() == NULL);

    /* ---- Kirby's copy throws the same spark ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindKirby;
    fp.passive_vars.kirby.copy_id = nFTKindNess;
    ftKirbySpecialNSetStatusSelect(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyNessSpecialN);
    fp.motion_vars.flags.flag0 = TRUE;
    CHECK(fp.proc_accessory != NULL);
    if (fp.proc_accessory != NULL)
    {
        fp.proc_accessory(mock_gobj);
    }
    wg = gGCCommonLinks[nGCCommonLinkIDWeapon];
    CHECK(wg != NULL && wpGetStruct(wg)->kind == nWPKindPKFire);
    if (wg != NULL)
    {
        wpMainDestroyWeapon(wg);
    }

    /* ---- PSI Magnet: the hold turns absorb on over NessMainMotion's
     * sphere, and an absorb turns him to face it ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindNess;
    ftNessSpecialLwStartSetStatus(mock_gobj);
    CHECK_STATUS(nFTNessStatusSpecialLwStart);
    ftNessSpecialLwHoldSetStatus(mock_gobj);
    CHECK_STATUS(nFTNessStatusSpecialLwHold);
    CHECK(fp.is_absorb != FALSE);
    CHECK(gFTNessFileMainMotion != NULL);
    CHECK(fp.special_coll != NULL);
    if (fp.special_coll != NULL)
    {
        CHECK((char*)fp.special_coll == (char*)gFTNessFileMainMotion + 0x16D4);
        CHECK(fp.special_coll->kind == 1);
        CHECK_EQF(fp.special_coll->offset.x, 300.0f);
        CHECK_EQF(fp.special_coll->size.x, 430.0f);
    }
    /* ... and the hold's InitVars made the bubble: efpsimagnet.mdl's row,
     * hung on his TopN through kind 0x50 */
    CHECK(fp.is_effect_attach != FALSE);
    {
        GObj *eg;
        GObj *bubble = NULL;

        for (eg = gGCCommonLinks[nGCCommonLinkIDEffect]; eg != NULL; eg = eg->link_next)
        {
            if ((efGetStruct(eg) != NULL) && (efGetStruct(eg)->fighter_gobj == mock_gobj))
            {
                bubble = eg;
            }
        }
        CHECK(bubble != NULL);
        if ((bubble != NULL) && (DObjGetStruct(bubble) != NULL))
        {
            CHECK(DObjGetStruct(bubble)->user_data.p == fp.joints[nFTPartsJointTopN]);
            CHECK(DObjGetStruct(bubble)->xobjs[0]->kind == 0x50);
        }
    }
    fp.lr = +1;
    fp.absorb_lr = -1;
    ftNessSpecialLwProcAbsorb(mock_gobj);
    CHECK_STATUS(nFTNessStatusSpecialLwHit);
    CHECK(fp.lr == -1);
    ftParamProcStopEffect(mock_gobj);

    /* ---- PK Thunder: the head goes up from his hand; steered back into
     * him, it launches him (the Jibaku) ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindNess;
    /* the mock skeleton has three joints; the head spawns off joint 12,
     * and the Jibaku pitches joint 4 (ftNessSpecialHiUpdateModelPitch) */
    saved_joint = fp.joints[FTNESS_PKTHUNDER_SPAWN_JOINT];
    saved_joint4 = fp.joints[4];
    fp.joints[FTNESS_PKTHUNDER_SPAWN_JOINT] = fp.joints[nFTPartsJointTopN];
    fp.joints[4] = fp.joints[nFTPartsJointTopN];
    is_joints_swapped = TRUE;
    ftNessSpecialHiStartSetStatus(mock_gobj);
    CHECK_STATUS(nFTNessStatusSpecialHiStart);
    CHECK(fp.status_vars.ness.specialhi.pkjibaku_delay == FTNESS_PKJIBAKU_DELAY);
    ftNessSpecialHiHoldSetStatus(mock_gobj);
    CHECK_STATUS(nFTNessStatusSpecialHiHold);
    wg = fp.status_vars.ness.specialhi.pkthunder_gobj;
    if (wg == NULL)
    {
        ftNessSpecialHiMakePKThunder(mock_gobj);
        wg = fp.status_vars.ness.specialhi.pkthunder_gobj;
    }
    CHECK(wg != NULL);
    if (wg == NULL)
    {
        goto done;
    }
    wp = wpGetStruct(wg);
    CHECK(wp->kind == nWPKindPKThunderHead);
    CHECK(wp->lifetime == WPPKTHUNDER_LIFETIME);
    CHECK_EQF(wp->physics.vel_air.y, FTNESS_PKTHUNDER_SPAWN_VEL_Y);
    CHECK(wp->weapon_vars.pkthunder.parent_gobj == mock_gobj);
    /* the hold made the wave too, on joint 5 as the decomp's literal says */
    CHECK(fp.is_effect_attach != FALSE);
    ftParamProcStopEffect(mock_gobj);

    /* ---- the trail's last segment: efpktrail.mdl's row. Its maker runs
     * the ProcUpdate at once, which reads Ness's trail ring and not the
     * model -- (x, y) of the slot COUNT - 2 behind the write index, and
     * the heading from the slot before it, less 90 degrees ---- */
    {
        GObj *seg;
        s32 last = ARRAY_COUNT(wp->weapon_vars.pkthunder.trail_gobj) - 1;
        GObj *saved_seg = wp->weapon_vars.pkthunder.trail_gobj[last];

        fp.status_vars.ness.specialhi.pkthunder_gobj = wg;
        fp.passive_vars.ness.pkthunder_trail_id = 0;
        fp.passive_vars.ness.pkthunder_trail_x[1] = 100;
        fp.passive_vars.ness.pkthunder_trail_y[1] = 40;
        fp.passive_vars.ness.pkthunder_trail_x[2] = 120;
        fp.passive_vars.ness.pkthunder_trail_y[2] = 40;
        seg = efManagerNessPKThunderTrailMakeEffect(mock_gobj);
        CHECK(seg != NULL);
        if (seg != NULL)
        {
            EFStruct *ep = efGetStruct(seg);

            CHECK(wp->weapon_vars.pkthunder.trail_gobj[last] == seg);
            CHECK(ep->effect_vars.pkthunder.owner_gobj == mock_gobj);
            CHECK_EQF(DObjGetStruct(seg)->translate.vec.f.x, 120.0f);
            CHECK_EQF(DObjGetStruct(seg)->translate.vec.f.y, 40.0f);
            CHECK_NEAR(DObjGetStruct(seg)->rotate.vec.f.z, -F_CLC_DTOR32(90.0F), 1e-4f);
            ep->effect_vars.pkthunder.status |= nWPNessPKThunderStatusDestroy;
            efManagerNessPKThunderTrailProcUpdate(seg);
        }
        wp->weapon_vars.pkthunder.trail_gobj[last] = saved_seg;
    }

    /* not while the delay runs, then yes: 100 behind him at chest height */
    DObjGetStruct(wg)->translate.vec.f.x = DObjGetStruct(mock_gobj)->translate.vec.f.x - 100.0f;
    DObjGetStruct(wg)->translate.vec.f.y = DObjGetStruct(mock_gobj)->translate.vec.f.y + 150.0f;
    CHECK(ftNessSpecialHiCheckCollidePKThunder(mock_gobj) == FALSE);
    fp.status_vars.ness.specialhi.pkjibaku_delay = 1;
    fp.passive_vars.ness.is_thunder_destroy = FALSE;
    ftNessSpecialHiHoldProcUpdate(mock_gobj);
    CHECK(fp.status_id == nFTNessStatusSpecialHiJibaku ||
          fp.status_id == nFTNessStatusSpecialAirHiJibaku);
    CHECK(wp->weapon_vars.pkthunder.status == nWPNessPKThunderStatusCollide);
    CHECK(fp.lr == +1);
    CHECK(fp.proc_damage == NULL);
    CHECK(wpNessPKThunderHeadProcUpdate(wg) == TRUE);
    wpMainDestroyWeapon(wg);
    while (gGCCommonLinks[nGCCommonLinkIDWeapon] != NULL)
    {
        wpMainDestroyWeapon(gGCCommonLinks[nGCCommonLinkIDWeapon]);
    }

done:
    if (is_joints_swapped)
    {
        fp.joints[FTNESS_PKTHUNDER_SPAWN_JOINT] = saved_joint;
        fp.joints[4] = saved_joint4;
    }
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindMario;
    if (loaded_here)
    {
        itemPackRelease();
    }
}

/* ft/ftchar/ftpikachu/ftpikachuspecialn.c, wp/wppikachu/wppikachuthunderjolt.c
 * and ft/ftchar/ftkirby/ftkirbycopypikachuspecialn.c -- the Thunder Jolt,
 * Pikachu's Neutral-B and the LAST fighter in the game to
 * get a status table of his own.
 *
 * Two things land at once and only one of them is the move. The four
 * rows are 220-223, and 220-221 are the entrance -- which means
 * dFTCommonEntryAppearStatusIDs points at Pikachu, and every playable
 * kind warps in. An entry row with no table under it would make
 * ftCommonEntryHasAppear TRUE while ftCommonEntryAppearIsLive stayed
 * FALSE. Both answer TRUE below. */
static void test_pikachu_jolt(void)
{
    extern void (*dFTCommonSpecialNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirHiStatusList[])(GObj*);
    extern void (*dFTKirbySpecialNStatusList[])(GObj*);
    extern void (*dFTKirbySpecialAirNStatusList[])(GObj*);
    extern sb32 ftCommonEntryHasAppear(s32 fkind);
    extern sb32 ftCommonEntryAppearIsLive(s32 fkind, s32 entry_id);
    extern FTStatusDesc dFTPikachuSpecialStatusDescs[];
    extern FTStatusDesc dFTKirbySpecialStatusDescs[];
    FTStatusDesc *row;
    DObj *saved_joint;
    WPStruct twp;
    DObj tdobj;
    GObj tg;
    int i;

    /* ---- the six demux slots: the two Neutral-B lists for base kind and
     * nametag, and Kirby's two copy lists. His Up-B and Down-B stay NULL,
     * which is what keeps rows 224-237 unreachable rather than silently
     * wrong -- the same shape Link's table has. ---- */
    CHECK(dFTCommonSpecialNStatusList[nFTKindPikachu] == ftPikachuSpecialNSetStatus);
    CHECK(dFTCommonSpecialNStatusList[nFTKindNPikachu] == ftPikachuSpecialNSetStatus);
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindPikachu] == ftPikachuSpecialAirNSetStatus);
    CHECK(dFTCommonSpecialAirNStatusList[nFTKindNPikachu] == ftPikachuSpecialAirNSetStatus);
    /* his Up-B */
    CHECK(dFTCommonSpecialHiStatusList[nFTKindPikachu] == ftPikachuSpecialHiStartSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindPikachu] == ftPikachuSpecialAirHiStartSetStatus);
    CHECK(dFTKirbySpecialNStatusList[nFTKindPikachu] == ftKirbyCopyPikachuSpecialNSetStatus);
    CHECK(dFTKirbySpecialAirNStatusList[nFTKindPikachu] == ftKirbyCopyPikachuSpecialAirNSetStatus);

    /* ---- the four rows, field by field. They are the first four of
     * twelve; the Thunder rows are covered by test_pikachu_thunder. ---- */
    CHECK(nFTPikachuStatusSpecialAirN - nFTCommonStatusSpecialStart + 1 == 4);

    for (i = nFTPikachuStatusAppearR; i <= nFTPikachuStatusAppearL; i++)
    {
        row = &dFTPikachuSpecialStatusDescs[i - nFTCommonStatusSpecialStart];
        CHECK(row->mflags.motion_id ==
              (u32)(nFTPikachuMotionAppearR + (i - nFTPikachuStatusAppearR)));
        CHECK(row->mflags.attack_id == nFTMotionAttackIDNone);
        CHECK(row->proc_update == ftCommonAppearProcUpdate);
        CHECK(row->proc_interrupt == NULL);
        CHECK(row->proc_physics == ftCommonAppearProcPhysics);
        CHECK(row->proc_map == mpCommonUpdateFighterProjectFloor);
        CHECK(row->sflags.ga == nMPKineticsGround);
        CHECK(row->sflags.is_projectile == FALSE);
        CHECK(row->sflags.attack_id == nFTStatusAttackIDNone);
    }

    row = &dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialN - nFTCommonStatusSpecialStart];
    CHECK(row->mflags.motion_id == nFTPikachuMotionSpecialN);
    CHECK(row->mflags.attack_id == nFTMotionAttackIDSpecialN);
    CHECK(row->proc_update == ftAnimEndSetWait);
    CHECK(row->proc_interrupt == NULL);
    CHECK(row->proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(row->proc_map == ftPikachuSpecialNProcMap);
    CHECK(row->sflags.ga == nMPKineticsGround);
    CHECK(row->sflags.is_smash_attack == FALSE);
    /* both halves are projectile statuses -- that flag is what lets a
     * reflector turn the jolt, and wpPikachuThunderJoltAirProcReflector
     * below is what it turns */
    CHECK(row->sflags.is_projectile == TRUE);
    CHECK(row->sflags.attack_id == nFTStatusAttackIDSpecialN);

    row = &dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialAirN - nFTCommonStatusSpecialStart];
    CHECK(row->mflags.motion_id == nFTPikachuMotionSpecialAirN);
    CHECK(row->mflags.attack_id == nFTMotionAttackIDSpecialN);
    CHECK(row->proc_update == ftAnimEndSetFall);
    CHECK(row->proc_physics == ftPhysicsApplyAirVelFriction);
    CHECK(row->proc_map == ftPikachuSpecialAirNProcMap);
    CHECK(row->sflags.ga == nMPKineticsAir);
    CHECK(row->sflags.is_projectile == TRUE);
    CHECK(row->sflags.attack_id == nFTStatusAttackIDSpecialN);

    /* ---- the entrance. Both facts: the two entrance rows
     * exist, so Pikachu has a status to name. ---- */
    CHECK(ftCommonEntryHasAppear(nFTKindPikachu) != FALSE);
    CHECK(ftCommonEntryAppearIsLive(nFTKindPikachu, 0) != FALSE);
    CHECK(ftCommonEntryAppearIsLive(nFTKindPikachu, 1) != FALSE);
    /* NPikachu is a Poly: his row is the COMMON EntryNull, not these */
    CHECK(ftCommonEntryAppearIsLive(nFTKindNPikachu, 0) != FALSE);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindPikachu;
    fp.lr = +1;
    ftCommonAppearSetStatus(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusAppearR);
    CHECK(fp.motion_id == nFTPikachuMotionAppearR);
    CHECK(fp.status_vars.common.entry.lr == +1);
    CHECK(fp.is_ghost != FALSE);

    mock_gobj->anim_frame = 0.0f;
    ftCommonAppearProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(fp.lr == +1);

    /* the warp-in throws the Poke Ball in at the entry
     * point, and the ball moves to DL link 20 once it is past z 1000 and
     * back to 10 in front of it (efManagerMBallThrownProcUpdate) */
    {
        GObj *before[64];
        GObj *effect_gobj;
        s32 nbefore = 0, made = 0, k;

        ef_pool_reset();
        for (effect_gobj = gGCCommonLinks[nGCCommonLinkIDEffect];
             (effect_gobj != NULL) && (nbefore < ARRAY_COUNT(before));
             effect_gobj = effect_gobj->link_next)
        {
            before[nbefore++] = effect_gobj;
        }
        spawn(0.0f, 0.0f);
        mock_anim_len = 1e9f;
        fp.fkind = nFTKindPikachu;
        fp.lr = -1;
        ftCommonAppearSetStatus(mock_gobj);
        CHECK_STATUS(nFTPikachuStatusAppearL);

        effect_gobj = gGCCommonLinks[nGCCommonLinkIDEffect];
        while (effect_gobj != NULL)
        {
            GObj *next = effect_gobj->link_next;

            for (k = 0; (k < nbefore) && (before[k] != effect_gobj); k++)
            {
            }
            if (k == nbefore)
            {
                DObj *dobj = DObjGetStruct(effect_gobj);

                made++;
                CHECK(efGetStruct(effect_gobj) != NULL);
                CHECK(dobj != NULL);
                if ((dobj != NULL) && (dobj->child != NULL))
                {
                    CHECK_EQF(dobj->translate.vec.f.x, fp.entry_pos.x);
                    CHECK_EQF(dobj->translate.vec.f.y, fp.entry_pos.y);
                    /* paused, so the host's empty animation does not end
                     * and eject it under the check */
                    efGetStruct(effect_gobj)->is_pause_effect = TRUE;
                    dobj->child->translate.vec.f.z = 2000.0F;
                    efManagerMBallThrownProcUpdate(effect_gobj);
                    CHECK(effect_gobj->dl_link_id == 20);
                    dobj->child->translate.vec.f.z = 0.0F;
                    efManagerMBallThrownProcUpdate(effect_gobj);
                    CHECK(effect_gobj->dl_link_id == 10);
                }
                else CHECK(dobj != NULL && dobj->child != NULL);
                gcEjectGObj(effect_gobj);
            }
            effect_gobj = next;
        }
        CHECK(made == 1);
        sEFManagerStructsAllocFree = NULL;
        sEFManagerStructsFreeNum = 0;
    }

    /* ---- the setters. Both install the accessory proc, which is where
     * the whole move lives: the status rows do nothing but play the
     * animation and let it end. ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindPikachu;
    fp.ga = nMPKineticsGround;

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.proc_accessory = NULL;
    fp.motion_vars.flags.flag0 = 1;
    ftPikachuSpecialNSetStatus(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialN);
    CHECK(fp.motion_id == nFTPikachuMotionSpecialN);
    CHECK(fp.proc_accessory == ftPikachuSpecialNProcAccessory);
    CHECK(fp.motion_vars.flags.flag0 == 0);  /* InitStatusVars clears it */

    fp.ga = nMPKineticsAir;
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.proc_accessory = NULL;
    ftPikachuSpecialAirNSetStatus(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialAirN);
    CHECK(fp.proc_accessory == ftPikachuSpecialNProcAccessory);

    /* ---- the ground/air switches keep the throw's own frame, the same
     * shape Link's do: walking off a ledge mid-jolt does not restart the
     * animation, and the accessory proc is reinstalled on both sides so
     * the spawn event still fires after the switch. ---- */
    fp.ga = nMPKineticsAir;
    mock_gobj->anim_frame = 6.0f;
    fp.proc_accessory = NULL;
    ftPikachuSpecialAirNSwitchStatusGround(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialN);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK_EQF(mock_gobj->anim_frame, 6.0f);
    CHECK(fp.proc_accessory == ftPikachuSpecialNProcAccessory);

    mock_gobj->anim_frame = 4.0f;
    fp.proc_accessory = NULL;
    ftPikachuSpecialNSwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialAirN);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK_EQF(mock_gobj->anim_frame, 4.0f);
    CHECK(fp.proc_accessory == ftPikachuSpecialNProcAccessory);

    /* ---- the accessory proc. It is gated on motion_vars.flags.flag0,
     * the animation event, and clears it -- so the spawn cannot survive
     * into the next frame unfired. The weapon it asks for comes back
     * NULL (unmodeled: no sWPManagerModels row for either of the jolt's
     * two WPDescs), which is the decomp's own no-projectile path and
     * exactly what Samus's Bomb and Link's boomerang do here. ---- */
    saved_joint = fp.joints[FTPIKACHU_THUNDERJOLT_SPAWN_JOINT];
    fp.joints[FTPIKACHU_THUNDERJOLT_SPAWN_JOINT] = fp.joints[nFTPartsJointTopN];

    fp.motion_vars.flags.flag0 = 0;
    ftPikachuSpecialNProcAccessory(mock_gobj);   /* no flag: nothing at all */
    CHECK(fp.motion_vars.flags.flag0 == 0);

    fp.motion_vars.flags.flag0 = 1;
    ftPikachuSpecialNProcAccessory(mock_gobj);
    CHECK(fp.motion_vars.flags.flag0 == 0);

    fp.joints[FTPIKACHU_THUNDERJOLT_SPAWN_JOINT] = saved_joint;

    /* ---- the weapon. wp/wppikachu/wppikachuthunderjolt.c is the largest
     * weapon file the port has taken, and its callee sweep came back with
     * seventeen mpCollision edge and line queries reading as MISSING --
     * every one of them already in mp/mpcollision.c, compiled unmodified
     *. What can be driven without a stage is the arithmetic: the
     * airborne ball's hop and gravity, and the ground crawler's velocity,
     * which is the move's whole character. Stack GObj/WPStruct/DObj, the
     * way test_wp_process reaches them. ---- */
    memset(&twp, 0, sizeof(twp));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tg, 0, sizeof(tg));
    tg.user_data.p = &twp;
    tg.obj = &tdobj;

    /* the airborne ball has NO gravity (WPPIKACHUJOLT_GRAVITY is 0) --
     * it flies the straight -45 degree line Pikachu throws it on, and
     * only the terminal-velocity clamp touches it */
    CHECK_EQF(WPPIKACHUJOLT_GRAVITY, 0.0F);
    twp.lifetime = 10;
    twp.physics.vel_air.x = 3.0F;
    twp.physics.vel_air.y = -4.0F;              /* magnitude 5 */
    CHECK(wpPikachuThunderJoltAirProcUpdate(&tg) == FALSE);
    CHECK(twp.lifetime == 9);
    CHECK_NEAR(twp.physics.vel_air.x, 3.0F, 1e-4F);
    CHECK_NEAR(twp.physics.vel_air.y, -4.0F, 1e-4F);

    /* the shield hop: reflect the velocity about the shield's normal by
     * twice the collide angle (syVectorRotateAbout3D, sys/vector.c) */
    twp.physics.vel_air.x = 1.0F;
    twp.physics.vel_air.y = 0.0F;
    twp.physics.vel_air.z = 0.0F;
    twp.shield_collide_dir.x = 0.0F;
    twp.shield_collide_dir.y = 0.0F;
    twp.shield_collide_dir.z = 1.0F;            /* about +Z */
    twp.shield_collide_angle = 3.14159265F / 4.0F;   /* x2 = 90 degrees */
    CHECK(wpPikachuThunderJoltAirProcHop(&tg) == FALSE);
    CHECK_NEAR(twp.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK_NEAR(twp.physics.vel_air.y, 1.0F, 1e-4F);

    /* ---- the ground crawler's velocity. Its speed is constant
     * (WPPIKACHUJOLT_VEL) along the surface it is riding, and the sign
     * rules are what make it crawl UP one wall and DOWN the other rather
     * than fall off: on a FLOOR the x component takes the jolt's lr; on a
     * WALL lr 2 forces the y component positive and lr 3 forces it
     * negative. anim_frame is kept off WPPIKACHUJOLT_ANIM_PUSH_FRAME so
     * the spark and the animation restart stay out of it. ---- */
    tg.anim_frame = 0.0F;
    twp.lifetime = 10;
    tdobj.rotate.vec.f.z = 0.0F;                /* along +X, flat floor */
    twp.weapon_vars.thunder_jolt.line_type = nMPLineKindFloor;
    twp.lr = +1;
    CHECK(wpPikachuThunderJoltGroundProcUpdate(&tg) == FALSE);
    CHECK_NEAR(twp.physics.vel_air.x, WPPIKACHUJOLT_VEL, 1e-3F);
    CHECK_NEAR(twp.physics.vel_air.y, 0.0F, 1e-3F);

    twp.lifetime = 10;
    twp.lr = -1;
    CHECK(wpPikachuThunderJoltGroundProcUpdate(&tg) == FALSE);
    CHECK_NEAR(twp.physics.vel_air.x, -WPPIKACHUJOLT_VEL, 1e-3F);

    /* a wall: the rotation points the jolt DOWN (-90 degrees), and lr 2
     * flips the y component back up */
    tdobj.rotate.vec.f.z = -3.14159265F / 2.0F;
    twp.weapon_vars.thunder_jolt.line_type = nMPLineKindLWall;
    twp.lifetime = 10;
    twp.lr = 2;
    CHECK(wpPikachuThunderJoltGroundProcUpdate(&tg) == FALSE);
    CHECK(twp.physics.vel_air.y > 0.0F);
    CHECK_NEAR(twp.physics.vel_air.y, WPPIKACHUJOLT_VEL, 1e-3F);

    /* lr 3 on the same wall forces it down instead */
    twp.lifetime = 10;
    twp.lr = 3;
    tdobj.rotate.vec.f.z = 3.14159265F / 2.0F;  /* now pointing up */
    CHECK(wpPikachuThunderJoltGroundProcUpdate(&tg) == FALSE);
    CHECK(twp.physics.vel_air.y < 0.0F);
    CHECK_NEAR(twp.physics.vel_air.y, -WPPIKACHUJOLT_VEL, 1e-3F);

    /* the same two rules on the other wall -- the switch shares its arm */
    twp.weapon_vars.thunder_jolt.line_type = nMPLineKindRWall;
    twp.lifetime = 10;
    twp.lr = 2;
    CHECK(wpPikachuThunderJoltGroundProcUpdate(&tg) == FALSE);
    CHECK(twp.physics.vel_air.y > 0.0F);

    /* and the lifetime is the jolt's own, not the fighter's: it expires
     * on the tic it reaches zero, whatever it is riding */
    twp.lifetime = 1;
    CHECK(wpPikachuThunderJoltGroundProcUpdate(&tg) == TRUE);
    CHECK(twp.lifetime == 0);

    /* the spark the crawler throws off has a pack now, so
     * efManagerAddModel no longer refuses it: placed and turned as the
     * jolt is */
    {
        Vec3f at = { 120.0F, -40.0F, 0.0F };
        GObj *spark;

        ef_pool_reset();
        spark = efManagerPikachuThunderJoltMakeEffect(&at, 0.75F);
        CHECK(spark != NULL);
        if (spark != NULL)
        {
            CHECK_EQF(DObjGetStruct(spark)->translate.vec.f.x, 120.0F);
            CHECK_EQF(DObjGetStruct(spark)->translate.vec.f.y, -40.0F);
            CHECK_EQF(DObjGetStruct(spark)->rotate.vec.f.z, 0.75F);
            gcEjectGObj(spark);
        }
        /* and the Thunder's trail spark: half size, the lifetime it was
         * asked for, and the fourth picture only when asked for it */
        spark = efManagerPikachuThunderTrailMakeEffect(&at, 10, 3);
        CHECK(spark != NULL);
        if (spark != NULL)
        {
            CHECK_EQF(DObjGetStruct(spark)->scale.vec.f.x, 0.5F);
            CHECK(efGetStruct(spark)->effect_vars.thunder_trail.lifetime == 10);
            CHECK((DObjGetStruct(spark)->mobj != NULL) &&
                  (DObjGetStruct(spark)->mobj->texture_id_curr == 3));
            gcEjectGObj(spark);
        }
        spark = efManagerPikachuThunderTrailMakeEffect(&at, 6, 0);
        CHECK(spark != NULL);
        if (spark != NULL)
        {
            CHECK((DObjGetStruct(spark)->mobj != NULL) &&
                  (DObjGetStruct(spark)->mobj->texture_id_curr == 0));
            gcEjectGObj(spark);
        }
        sEFManagerStructsAllocFree = NULL;
        sEFManagerStructsFreeNum = 0;
    }

    /* the animation a turn re-adds (wpPikachuThunderJoltGroundAddAnim),
     * read the way that function reads it: every one of the tree's eight
     * joints must see a NULL AnimJoint and a NULL MatAnimJoint row. A
     * non-NULL entry here is .bss the parser walks as a script, which is
     * how a four-CPU Saffron City match hung (src/dc/wpattrs.c). */
    {
        /* as reloc_data.us.h declares them to the decomp's translation
         * unit */
        extern int llPikachuSpecial3ThunderJoltBAnimJoint;
        extern int llPikachuSpecial3ThunderJoltBMatAnimJoint;
        AObjEvent32 **anims = lbRelocGetFileData(AObjEvent32**,
            gFTDataPikachuSpecial3, &llPikachuSpecial3ThunderJoltBAnimJoint);
        AObjEvent32 ***mats = lbRelocGetFileData(AObjEvent32***,
            gFTDataPikachuSpecial3, &llPikachuSpecial3ThunderJoltBMatAnimJoint);
        int j;

        CHECK(gFTDataPikachuSpecial3 == NULL);
        for (j = 0; j < 8; j++)
        {
            CHECK(anims[j] == NULL);
            CHECK(mats[j] == NULL);
        }
    }

    /* the reflector resets the life, turns the jolt around if it was
     * coming AT the reflecting fighter (wpMainReflectorSetLR: only when
     * vel.x and the fighter's lr disagree), then faces the model and sets
     * the jolt's own lr from the velocity it ends up with */
    twp.owner_gobj = mock_gobj;
    fp.lr = +1;

    twp.physics.vel_air.x = -5.0F;              /* coming at a right-facer */
    twp.lifetime = 3;
    CHECK(wpPikachuThunderJoltGroundProcReflector(&tg) == FALSE);
    CHECK(twp.lifetime == WPPIKACHUJOLT_LIFETIME);
    CHECK_NEAR(twp.physics.vel_air.x, 5.0F, 1e-4F);   /* turned around */
    CHECK_NEAR(tdobj.rotate.vec.f.y, 3.14159265F, 1e-3F);
    CHECK(twp.lr == +1);

    /* already going the fighter's way: left alone, and the model faces
     * the other way */
    fp.lr = -1;
    twp.physics.vel_air.x = -5.0F;
    CHECK(wpPikachuThunderJoltGroundProcReflector(&tg) == FALSE);
    CHECK_NEAR(twp.physics.vel_air.x, -5.0F, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.y, 0.0F, 1e-3F);
    CHECK(twp.lr == -1);

    /* ---- Kirby's ninth copy mouth, and the two rows behind it. The
     * whole difference from Pikachu's own file is the spawn offset off
     * the joint and the two status ids; the jolt itself never asks who
     * threw it, which is why this half is a one-way dependency. ---- */
    for (i = nFTKirbyStatusCopyPikachuSpecialN; i <= nFTKirbyStatusCopyPikachuSpecialAirN; i++)
    {
        row = &dFTKirbySpecialStatusDescs[i - nFTCommonStatusSpecialStart];
        CHECK(row->proc_physics != NULL);
        CHECK(row->proc_map != NULL);
        CHECK(row->sflags.is_projectile == TRUE);
        CHECK(row->sflags.attack_id == nFTStatusAttackIDSpecialNCopyPikachu);
    }
    row = &dFTKirbySpecialStatusDescs[nFTKirbyStatusCopyPikachuSpecialN - nFTCommonStatusSpecialStart];
    CHECK(row->mflags.motion_id == nFTKirbyMotionCopyPikachuSpecialN);
    CHECK(row->proc_update == ftAnimEndSetWait);
    CHECK(row->proc_map == ftKirbyCopyPikachuSpecialNProcMap);
    row = &dFTKirbySpecialStatusDescs[nFTKirbyStatusCopyPikachuSpecialAirN - nFTCommonStatusSpecialStart];
    CHECK(row->mflags.motion_id == nFTKirbyMotionCopyPikachuSpecialAirN);
    CHECK(row->proc_update == ftAnimEndSetFall);
    CHECK(row->proc_map == ftKirbyCopyPikachuSpecialAirNProcMap);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindKirby;
    fp.ga = nMPKineticsGround;
    fp.passive_vars.kirby.copy_id = nFTKindPikachu;

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.proc_accessory = NULL;
    ftKirbySpecialNSetStatusSelect(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyPikachuSpecialN);
    CHECK(fp.proc_accessory == ftKirbyCopyPikachuSpecialNProcAccessory);

    fp.ga = nMPKineticsAir;
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.proc_accessory = NULL;
    ftKirbySpecialAirNSetStatusSelect(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyPikachuSpecialAirN);
    CHECK(fp.proc_accessory == ftKirbyCopyPikachuSpecialNProcAccessory);

    /* his switches carry the same frame across, and his accessory proc
     * is gated on the same animation event */
    mock_gobj->anim_frame = 5.0f;
    ftKirbyCopyPikachuSpecialAirNSwitchStatusGround(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyPikachuSpecialN);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK_EQF(mock_gobj->anim_frame, 5.0f);

    ftKirbyCopyPikachuSpecialNSwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTKirbyStatusCopyPikachuSpecialAirN);
    CHECK(fp.ga == nMPKineticsAir);

    fp.motion_vars.flags.flag0 = 1;
    ftKirbyCopyPikachuSpecialNProcAccessory(mock_gobj);
    CHECK(fp.motion_vars.flags.flag0 == 0);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindMario;
}

/* ft/ftchar/ftpikachu/ftpikachuspeciallw.c and wp/wppikachu/wppikachuthunder.c
 * -- the Thunder, Pikachu's Down-B.
 *
 * Four statuses twice over, ground and air, and the pairing is the shape
 * of the move: Start calls the bolt down, Loop waits for it, Hit is
 * Pikachu struck by his own lightning, End is the recovery. Every one of
 * the eight can switch to its opposite number mid-move, carrying the
 * animation's own frame across, which is why the file has eight ProcMaps
 * and eight switch functions rather than two.
 *
 * The bolt itself is TWO weapons: a head that is not an attack at all
 * (attack_state Off, a falling marker) and the trail segments it drops
 * behind itself, which are. Neither is modelled on this port, so
 * ftPikachuSpecialLwMakeThunder stores a NULL -- and that NULL is a path
 * the decomp's own code has: is_thunder_destroy. */
static void test_pikachu_thunder(void)
{
    extern void (*dFTCommonSpecialAirLwStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirHiStatusList[])(GObj*);
    extern FTStatusDesc dFTPikachuSpecialStatusDescs[];
    static const s32 kLwMotions[8] = {
        nFTPikachuMotionSpecialLwStart,    nFTPikachuMotionSpecialLwLoop,
        nFTPikachuMotionSpecialLwHit,      nFTPikachuMotionSpecialLwEnd,
        nFTPikachuMotionSpecialAirLwStart, nFTPikachuMotionSpecialAirLwLoop,
        nFTPikachuMotionSpecialAirLwHit,   nFTPikachuMotionSpecialAirLwEnd
    };
    FTStatusDesc *row;
    DObj *saved_joint;
    WPStruct twp, owner_wp;
    MObj tmobj;
    DObj tdobj;
    GObj tg;
    int i;

    /* ---- the two demux slots the Down-B needs, base kind and nametag.
     * His Up-B was still a hole when this test was written; it is filled
     * now, and test_pikachu_agility covers those rows. ---- */
    CHECK(dFTCommonSpecialAirLwStatusList[nFTKindPikachu] == ftPikachuSpecialAirLwStartSetStatus);
    CHECK(dFTCommonSpecialAirLwStatusList[nFTKindNPikachu] == ftPikachuSpecialAirLwStartSetStatus);

    /* the ground list is static in src/dc/ftcommon.c, so its slot is
     * driven rather than read: a B-tap with the stick held down */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindPikachu;
    fp.ga = nMPKineticsGround;
    fp.attr->is_have_speciallw = TRUE;
    fp.input.pl.button_tap = fp.input.button_mask_b;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = FTCOMMON_SPECIALLW_STICK_RANGE_MIN;
    CHECK(ftCommonSpecialLwCheckInterruptCommon(mock_gobj) != FALSE);
    CHECK_STATUS(nFTPikachuStatusSpecialLwStart);
    fp.input.pl.button_tap = 0;
    fp.input.pl.stick_range.y = 0;

    /* ---- the eight rows. The array holds twelve rows here; the
     * Agility rows bring it to eighteen. ---- */
    CHECK(nFTPikachuStatusSpecialAirLwEnd - nFTCommonStatusSpecialStart + 1 == 12);
    for (i = nFTPikachuStatusSpecialLwStart; i <= nFTPikachuStatusSpecialAirLwEnd; i++)
    {
        row = &dFTPikachuSpecialStatusDescs[i - nFTCommonStatusSpecialStart];
        CHECK(row->mflags.motion_id == (u32)kLwMotions[i - nFTPikachuStatusSpecialLwStart]);
        CHECK(row->proc_interrupt == NULL);
        CHECK(row->proc_physics != NULL);
        CHECK(row->proc_map != NULL);
        /* every one is a projectile status -- the bolt is a weapon */
        CHECK(row->sflags.is_projectile == TRUE);
        CHECK(row->sflags.is_smash_attack == FALSE);
        /* and every one carries the game's own WRONG attack id: the
         * decomp's table says "Scuffed attack IDs, SpecialHi and
         * SpecialLw are swapped", and this is the Down-B */
        CHECK(row->mflags.attack_id == nFTMotionAttackIDSpecialHi);
        CHECK(row->sflags.attack_id == nFTStatusAttackIDSpecialHi);
        CHECK(row->sflags.ga == ((i <= nFTPikachuStatusSpecialLwEnd)
                                 ? nMPKineticsGround : nMPKineticsAir));
    }
    /* only the airborne Hit needs physics of Pikachu's own: the bolt
     * knocks him downward under a gravity of its own */
    row = &dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialAirLwHit - nFTCommonStatusSpecialStart];
    CHECK(row->proc_physics == ftPikachuSpecialAirLwHitProcPhysics);
    CHECK(row->proc_update == ftPikachuSpecialAirLwHitProcUpdate);
    row = &dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialLwEnd - nFTCommonStatusSpecialStart];
    CHECK(row->proc_update == ftAnimEndSetWait);
    CHECK(row->proc_map == ftPikachuSpecialLwEndProcMap);

    /* the stack shells the bolt is driven on, the way test_wp_process
     * reaches a weapon: wpGetStruct is user_data.p and DObjGetStruct is
     * obj */
    memset(&twp, 0, sizeof(twp));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    memset(&tg, 0, sizeof(tg));
    tg.user_data.p = &twp;
    tg.obj = &tdobj;
    tdobj.mobj = &tmobj;

    /* ---- the move, walked on the mock. The bolt is unmodelled, so
     * ftPikachuSpecialLwMakeThunder stores NULL -- which is exactly the
     * decomp's own "the thunder is gone" path, and the walk below is the
     * one a real match takes when the bolt has already struck. ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindPikachu;
    fp.ga = nMPKineticsGround;
    saved_joint = fp.joints[FTPIKACHU_THUNDER_SPAWN_JOINT];
    fp.joints[FTPIKACHU_THUNDER_SPAWN_JOINT] = fp.joints[nFTPartsJointTopN];

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.pikachu.is_thunder_destroy = TRUE;
    fp.motion_vars.flags.flag0 = 1;
    fp.motion_vars.flags.flag1 = 1;
    ftPikachuSpecialLwStartSetStatus(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialLwStart);
    CHECK(fp.motion_id == nFTPikachuMotionSpecialLwStart);
    /* InitStatusVars clears both animation events and the destroyed flag */
    CHECK(fp.motion_vars.flags.flag0 == 0);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK(fp.passive_vars.pikachu.is_thunder_destroy == FALSE);

    /* Start's update calls the bolt down only on the animation event, and
     * ends into Loop. Loop's setter installs the damage hook, which is
     * what makes being hit mid-Thunder cancel the bolt. */
    fp.status_vars.pikachu.speciallw.thunder_gobj = NULL;
    fp.proc_damage = NULL;
    mock_gobj->anim_frame = 5.0f;
    ftPikachuSpecialLwStartProcUpdate(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialLwStart);

    mock_gobj->anim_frame = 0.0f;
    ftPikachuSpecialLwStartProcUpdate(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialLwLoop);
    CHECK(fp.motion_id == nFTPikachuMotionSpecialLwLoop);
    /* A DECOMP QUIRK, pinned rather than fixed. ftPikachuSpecialLwLoop-
     * UpdateThunder (ftpikachuspeciallw.c:273) assigns proc_damage, and
     * ftPikachuSpecialLwLoopSetStatus calls it BEFORE ftMainSetStatus --
     * which clears every proc hook including that one (ftmain.c:4809).
     * So the hook is installed and wiped in the same call, and
     * ftPikachuSpecialLwProcDamage is dead in the shipped game: being
     * hit mid-Thunder does not cancel the bolt through it. Its one other
     * caller-free path, the End setter's ftPikachuSpecialLwClearProc-
     * Damage, is a no-op for the same reason. Both files are compiled
     * unmodified, so the port inherits the quirk exactly. Compare his
     * Neutral-B, where the setters call InitStatusVars AFTER
     * ftMainSetStatus and the accessory proc really does survive. */
    CHECK(fp.proc_damage == NULL);

    /* ---- the collide check is where the unmodelled bolt shows. A NULL
     * thunder_gobj raises is_thunder_destroy and reports no hit, and the
     * Loop update reads that as "the bolt is spent" and recovers. ---- */
    fp.status_vars.pikachu.speciallw.thunder_gobj = NULL;
    fp.passive_vars.pikachu.is_thunder_destroy = FALSE;
    CHECK(ftPikachuSpecialLwCheckCollideThunder(mock_gobj) == FALSE);
    CHECK(fp.passive_vars.pikachu.is_thunder_destroy != FALSE);

    ftMainSetStatus(mock_gobj, nFTPikachuStatusSpecialLwLoop, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.pikachu.is_thunder_destroy = FALSE;
    fp.status_vars.pikachu.speciallw.thunder_gobj = NULL;
    ftPikachuSpecialLwLoopProcUpdate(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialLwEnd);

    /* the animation event ends the Loop too, with a live bolt that is
     * simply too far away to have struck */
    ftMainSetStatus(mock_gobj, nFTPikachuStatusSpecialLwLoop, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.passive_vars.pikachu.is_thunder_destroy = FALSE;
    fp.status_vars.pikachu.speciallw.thunder_gobj = &tg;
    tdobj.translate.vec.f.x = 10000.0f;
    tdobj.translate.vec.f.y = 10000.0f;
    twp.weapon_vars.thunder.thunder_state = nWPPikachuThunderStatusActive;
    fp.motion_vars.flags.flag1 = 1;
    ftPikachuSpecialLwLoopProcUpdate(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialLwEnd);
    CHECK(twp.weapon_vars.thunder.thunder_state == nWPPikachuThunderStatusActive);
    fp.motion_vars.flags.flag1 = 0;

    /* ---- the Hit pair. The airborne setter is the only one that touches
     * velocity: the bolt throws him upward as it passes through. ---- */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.proc_damage = ftPikachuSpecialLwProcDamage;
    fp.motion_vars.flags.flag1 = 1;
    ftPikachuSpecialLwHitSetStatus(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialLwHit);
    CHECK(fp.proc_damage == NULL);
    CHECK(fp.motion_vars.flags.flag1 == 0);

    fp.ga = nMPKineticsAir;
    fp.physics.vel_air.y = -99.0f;
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftPikachuSpecialAirLwHitSetStatus(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialAirLwHit);
    CHECK_EQF(fp.physics.vel_air.y, FTPIKACHU_THUNDER_HITVEL_Y);

    /* Hit ends on its own animation event, air side into the air End */
    fp.motion_vars.flags.flag1 = 1;
    ftPikachuSpecialAirLwHitProcUpdate(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialAirLwEnd);

    /* and the airborne End falls out rather than standing up */
    mock_gobj->anim_frame = 0.0f;
    ftPikachuSpecialAirLwEndProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusFall);

    /* ---- all four ground/air switches keep the animation's own frame:
     * walking off a ledge or landing mid-Thunder must not restart it.
     * That is eight functions in the decomp and the reason the file is
     * 444 lines for a four-status move. ---- */
    fp.ga = nMPKineticsGround;
    ftMainSetStatus(mock_gobj, nFTPikachuStatusSpecialLwStart, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    mock_gobj->anim_frame = 9.0f;
    ftPikachuSpecialLwStartSwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialAirLwStart);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK_EQF(mock_gobj->anim_frame, 9.0f);

    mock_gobj->anim_frame = 3.0f;
    ftPikachuSpecialAirLwStartSwitchStatusGround(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialLwStart);
    CHECK(fp.ga == nMPKineticsGround);
    CHECK_EQF(mock_gobj->anim_frame, 3.0f);

    ftMainSetStatus(mock_gobj, nFTPikachuStatusSpecialLwLoop, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    mock_gobj->anim_frame = 11.0f;
    ftPikachuSpecialLwLoopSwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialAirLwLoop);
    CHECK_EQF(mock_gobj->anim_frame, 11.0f);
    ftPikachuSpecialAirLwLoopSwitchStatusGround(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialLwLoop);

    ftMainSetStatus(mock_gobj, nFTPikachuStatusSpecialLwHit, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftPikachuSpecialLwHitSwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialAirLwHit);
    ftPikachuSpecialAirLwHitSwitchStatusGround(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialLwHit);

    ftMainSetStatus(mock_gobj, nFTPikachuStatusSpecialLwEnd, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftPikachuSpecialLwEndSwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialAirLwEnd);
    ftPikachuSpecialAirLwEndSwitchStatusGround(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialLwEnd);

    /* ---- the damage hook. Being hit while the bolt is out marks it
     * Destroy on the weapon itself, so the bolt stops rather than
     * carrying on without its owner. With no bolt it raises the flag
     * instead, which is the same thing said locally. ---- */
    fp.status_vars.pikachu.speciallw.thunder_gobj = &tg;
    fp.passive_vars.pikachu.is_thunder_destroy = FALSE;
    twp.weapon_vars.thunder.thunder_state = nWPPikachuThunderStatusActive;
    ftPikachuSpecialLwProcDamage(mock_gobj);
    CHECK(twp.weapon_vars.thunder.thunder_state == nWPPikachuThunderStatusDestroy);

    fp.status_vars.pikachu.speciallw.thunder_gobj = NULL;
    fp.passive_vars.pikachu.is_thunder_destroy = FALSE;
    ftPikachuSpecialLwProcDamage(mock_gobj);
    CHECK(fp.passive_vars.pikachu.is_thunder_destroy != FALSE);

    /* ---- the collide box, on a bolt that is really there. It is
     * FTPIKACHU_THUNDER_COLLIDE_X wide and COLLIDE_Y tall about a point
     * COLL_OFF_Y above the bolt, and a hit marks the bolt Collide --
     * which is how the head weapon learns to stop. ---- */
    fp.status_vars.pikachu.speciallw.thunder_gobj = &tg;
    fp.passive_vars.pikachu.is_thunder_destroy = FALSE;
    twp.weapon_vars.thunder.thunder_state = nWPPikachuThunderStatusActive;
    fp_pos.x = 0.0f;
    fp_pos.y = 0.0f;
    tdobj.translate.vec.f.x = 0.0f;
    tdobj.translate.vec.f.y = -FTPIKACHU_THUNDER_COLL_OFF_Y;   /* dead level */
    CHECK(ftPikachuSpecialLwCheckCollideThunder(mock_gobj) == TRUE);
    CHECK(twp.weapon_vars.thunder.thunder_state == nWPPikachuThunderStatusCollide);

    twp.weapon_vars.thunder.thunder_state = nWPPikachuThunderStatusActive;
    tdobj.translate.vec.f.x = FTPIKACHU_THUNDER_COLLIDE_X + 1.0f;   /* too far aside */
    CHECK(ftPikachuSpecialLwCheckCollideThunder(mock_gobj) == FALSE);

    tdobj.translate.vec.f.x = 0.0f;
    tdobj.translate.vec.f.y = -FTPIKACHU_THUNDER_COLL_OFF_Y - FTPIKACHU_THUNDER_COLLIDE_Y - 1.0f;
    CHECK(ftPikachuSpecialLwCheckCollideThunder(mock_gobj) == FALSE);
    CHECK(twp.weapon_vars.thunder.thunder_state == nWPPikachuThunderStatusActive);

    /* ---- the weapon, on stack shells. The head is not an attack: it
     * falls, and on the tic it lands or expires it tells its owner so
     * through wpPikachuThunderHeadSetDestroy, which checks the player
     * number so one Pikachu's bolt cannot cancel another's. ---- */
    memset(&owner_wp, 0, sizeof(owner_wp));
    twp.owner_gobj = mock_gobj;
    twp.player_num = fp.player_num;
    twp.weapon_vars.thunder.thunder_state = nWPPikachuThunderStatusActive;
    fp.passive_vars.pikachu.is_thunder_destroy = FALSE;
    wpPikachuThunderHeadSetDestroy(&tg, TRUE);
    CHECK(fp.passive_vars.pikachu.is_thunder_destroy != FALSE);

    /* another player's bolt says nothing to this fighter */
    twp.player_num = fp.player_num + 1;
    fp.passive_vars.pikachu.is_thunder_destroy = FALSE;
    wpPikachuThunderHeadSetDestroy(&tg, TRUE);
    CHECK(fp.passive_vars.pikachu.is_thunder_destroy == FALSE);

    /* and a bolt already marked Destroy says nothing either -- the guard
     * is on the state, not on the flag */
    twp.player_num = fp.player_num;
    twp.weapon_vars.thunder.thunder_state = nWPPikachuThunderStatusDestroy;
    wpPikachuThunderHeadSetDestroy(&tg, TRUE);
    CHECK(fp.passive_vars.pikachu.is_thunder_destroy == FALSE);

    /* the head's update: Collide ends it at once, and so does the
     * lifetime running out -- and the expiry is the one that tells the
     * owner. The trail effect it asks for on the way out is unmodelled
     * and comes back NULL, which nothing reads. */
    twp.weapon_vars.thunder.thunder_state = nWPPikachuThunderStatusCollide;
    twp.lifetime = 5;
    CHECK(wpPikachuThunderHeadProcUpdate(&tg) == TRUE);
    CHECK(twp.lifetime == 5);                 /* not even counted down */

    twp.weapon_vars.thunder.thunder_state = nWPPikachuThunderStatusActive;
    twp.lifetime = 2;
    fp.passive_vars.pikachu.is_thunder_destroy = FALSE;
    CHECK(wpPikachuThunderHeadProcUpdate(&tg) == FALSE);
    CHECK(twp.lifetime == 1);
    CHECK(fp.passive_vars.pikachu.is_thunder_destroy == FALSE);
    CHECK(wpPikachuThunderHeadProcUpdate(&tg) == TRUE);
    CHECK(twp.lifetime == 0);
    CHECK(fp.passive_vars.pikachu.is_thunder_destroy != FALSE);

    /* the trail segment's update: it counts down, and stops being an
     * attack WPPIKACHUTHUNDER_EXPIRE tics before it goes */
    twp.lifetime = WPPIKACHUTHUNDER_TRAIL_LIFETIME;
    CHECK(wpPikachuThunderTrailProcUpdate(&tg) == FALSE);
    CHECK(twp.lifetime == WPPIKACHUTHUNDER_TRAIL_LIFETIME - 1);
    twp.lifetime = WPPIKACHUTHUNDER_EXPIRE;
    CHECK(wpPikachuThunderTrailProcUpdate(&tg) == TRUE);   /* dips below */
    twp.lifetime = 1;
    CHECK(wpPikachuThunderTrailProcUpdate(&tg) == TRUE);   /* or runs out */
    CHECK(twp.lifetime == 0);

    fp.joints[FTPIKACHU_THUNDER_SPAWN_JOINT] = saved_joint;
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindMario;
}

/* ft/ftchar/ftpikachu/ftpikachuspecialhi.c -- the Agility, or Quick
 * Attack. His Up-B, six rows, and the LAST special move any
 * playable fighter was missing that does not depend on the item code.
 *
 * The cleanest ftchar file this port has taken: no weapon, no effect, no
 * reloc symbol, no global. Two teleports and a model squash, and every
 * leaf it needs was already compiled in.
 *
 * Three of the six rows carry an oddity of the game's own, all kept: the
 * two Start rows have motion id -1 and play no animation at all, row 235
 * (the AIRBORNE Start) is marked Ground and the decomp says so, and all
 * six carry attack id SpecialLw on an Up-B -- the same swap the Thunder's
 * eight carry the other way round. */
static void test_pikachu_agility(void)
{
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirHiStatusList[])(GObj*);
    extern FTStatusDesc dFTPikachuSpecialStatusDescs[];
    FTStatusDesc *row;
    float v0, v1;
    int i;

    /* ---- the four demux slots, base kind and nametag ---- */
    CHECK(dFTCommonSpecialHiStatusList[nFTKindPikachu] == ftPikachuSpecialHiStartSetStatus);
    CHECK(dFTCommonSpecialHiStatusList[nFTKindNPikachu] == ftPikachuSpecialHiStartSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindPikachu] == ftPikachuSpecialAirHiStartSetStatus);
    CHECK(dFTCommonSpecialAirHiStatusList[nFTKindNPikachu] == ftPikachuSpecialAirHiStartSetStatus);

    /* ---- the six rows. His table is now WHOLE: eighteen rows, 220 to
     * 237, nothing past the Agility in the game either. ---- */
    CHECK(nFTPikachuStatusSpecialAirHiEnd - nFTCommonStatusSpecialStart + 1 == 18);
    for (i = nFTPikachuStatusSpecialHiStart; i <= nFTPikachuStatusSpecialAirHiEnd; i++)
    {
        row = &dFTPikachuSpecialStatusDescs[i - nFTCommonStatusSpecialStart];
        CHECK(row->proc_update != NULL);
        CHECK(row->proc_interrupt == NULL);
        CHECK(row->proc_physics != NULL);
        CHECK(row->proc_map != NULL);
        /* the Quick Attack is a teleport, not a hitbox */
        CHECK(row->sflags.is_projectile == FALSE);
        /* and every one carries the game's own WRONG attack id -- Down-B
         * on an Up-B, the mirror of the Thunder's eight */
        CHECK(row->mflags.attack_id == nFTMotionAttackIDSpecialLw);
        CHECK(row->sflags.attack_id == nFTStatusAttackIDSpecialLw);
    }
    /* the two Start rows play NO animation: motion id -1, which fits
     * because FTMotionFlags.motion_id is a SIGNED ten-bit field */
    CHECK(dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialHiStart - nFTCommonStatusSpecialStart].mflags.motion_id == -1);
    CHECK(dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialAirHiStart - nFTCommonStatusSpecialStart].mflags.motion_id == -1);
    /* row 235 is the AIRBORNE Start and is marked Ground -- the decomp's
     * own comment says "not marked as airborne". Kept as the game has it;
     * its ProcPhysics applies gravity regardless. */
    CHECK(dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialAirHiStart - nFTCommonStatusSpecialStart].sflags.ga == nMPKineticsGround);
    CHECK(dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialAirHiStart - nFTCommonStatusSpecialStart].proc_physics == ftPikachuSpecialAirHiStartProcPhysics);
    CHECK(dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialAirHi - nFTCommonStatusSpecialStart].sflags.ga == nMPKineticsAir);
    CHECK(dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialHi - nFTCommonStatusSpecialStart].proc_physics == ftPikachuSpecialHiProcPhysics);

    /* ---- the Start. It freezes the animation and counts down on a
     * status var instead, which is what the -1 motion is for. ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindPikachu;
    fp.ga = nMPKineticsGround;

    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.physics.vel_air.x = 11.0f;
    fp.physics.vel_air.y = 12.0f;
    fp.physics.vel_ground.x = 13.0f;
    fp.motion_vars.flags.flag1 = 1;
    fp.status_vars.pikachu.specialhi.is_subsequent_zip = TRUE;
    fp.status_vars.pikachu.specialhi.pass_timer = 9;
    ftPikachuSpecialHiStartSetStatus(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialHiStart);
    CHECK(fp.status_vars.pikachu.specialhi.anim_frames == FTPIKACHU_QUICKATTACK_START_TIME);
    CHECK(fp.status_vars.pikachu.specialhi.is_subsequent_zip == FALSE);
    CHECK(fp.status_vars.pikachu.specialhi.pass_timer == 0);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK_EQF(fp.physics.vel_air.x, 0.0f);
    CHECK_EQF(fp.physics.vel_air.y, 0.0f);
    CHECK_EQF(fp.physics.vel_ground.x, 0.0f);
    /* intangible while he is winding up -- this is why the Quick Attack
     * beats an attack aimed at where he was */
    CHECK(fp.hitstatus == nGMHitStatusIntangible);

    /* nineteen ticks do nothing but count; the twentieth is the zip */
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = 0;
    for (i = 0; i < FTPIKACHU_QUICKATTACK_START_TIME - 1; i++)
    {
        ftPikachuSpecialHiStartProcUpdate(mock_gobj);
        CHECK_STATUS(nFTPikachuStatusSpecialHiStart);
    }
    CHECK(fp.status_vars.pikachu.specialhi.anim_frames == 1);

    /* ---- the zip. With the stick under FTPIKACHU_QUICKATTACK_STICK_RANGE_MIN
     * the grounded setter gives up on the ground and hands over to the
     * airborne one, which reads a neutral stick as straight up. ---- */
    ftPikachuSpecialHiStartProcUpdate(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialAirHi);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(fp.status_vars.pikachu.specialhi.anim_frames == FTPIKACHU_QUICKATTACK_ZIP_TIME);
    /* a neutral stick is read as full range straight up */
    CHECK(fp.status_vars.pikachu.specialhi.stick_range.x == 0);
    CHECK(fp.status_vars.pikachu.specialhi.stick_range.y == I_CONTROLLER_RANGE_MAX);
    CHECK_NEAR(fp.physics.vel_air.x, 0.0f, 1e-3f);
    CHECK_NEAR(fp.physics.vel_air.y,
               FTPIKACHU_QUICKATTACK_VEL_BASE * F_CONTROLLER_RANGE_MAX
               + FTPIKACHU_QUICKATTACK_VEL_ADD, 1e-2f);
    /* and it costs him every jump he had left */
    CHECK(fp.jumps_used == fp.attr->jumps_max);

    /* a hard stick sideways sends him along it instead, at the same
     * 3*range + 90 the neutral case takes at full range */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.lr = +1;
    fp.status_vars.pikachu.specialhi.is_subsequent_zip = FALSE;
    fp.input.pl.stick_range.x = I_CONTROLLER_RANGE_MAX;
    fp.input.pl.stick_range.y = 0;
    ftPikachuSpecialAirHiSetStatus(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialAirHi);
    v0 = FTPIKACHU_QUICKATTACK_VEL_BASE * F_CONTROLLER_RANGE_MAX
       + FTPIKACHU_QUICKATTACK_VEL_ADD;
    CHECK_NEAR(fp.physics.vel_air.x, v0, 1e-2f);
    CHECK_NEAR(fp.physics.vel_air.y, 0.0f, 1e-2f);
    CHECK(fp.status_vars.pikachu.specialhi.stick_range.x == I_CONTROLLER_RANGE_MAX);

    /* a SECOND zip is FTPIKACHU_QUICKATTACK_VEL_MUL slower */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.status_vars.pikachu.specialhi.is_subsequent_zip = TRUE;
    ftPikachuSpecialAirHiSetStatus(mock_gobj);
    CHECK_NEAR(fp.physics.vel_air.x, v0 * FTPIKACHU_QUICKATTACK_VEL_MUL, 1e-2f);
    fp.status_vars.pikachu.specialhi.is_subsequent_zip = FALSE;

    /* the stick is read against the FACING, so a left-facing Pikachu with
     * the stick right zips right all the same */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.lr = -1;
    fp.input.pl.stick_range.x = -I_CONTROLLER_RANGE_MAX;
    fp.input.pl.stick_range.y = 0;
    ftPikachuSpecialAirHiSetStatus(mock_gobj);
    v1 = fp.physics.vel_air.x;
    CHECK(v1 < 0.0f);
    CHECK_NEAR(-v1, v0, 1e-2f);

    /* ---- the second zip's gate. It needs a stick past the minimum, no
     * zip already spent, and an angle at least
     * FTPIKACHU_QUICKATTACK_ANGLE_DIFF_MIN away from the first. ---- */
    fp.status_vars.pikachu.specialhi.is_subsequent_zip = FALSE;
    fp.status_vars.pikachu.specialhi.stick_range.x = I_CONTROLLER_RANGE_MAX;
    fp.status_vars.pikachu.specialhi.stick_range.y = 0;

    fp.input.pl.stick_range.x = 1;      /* under the minimum */
    fp.input.pl.stick_range.y = 1;
    CHECK(ftPikachuSpecialHiCheckGotoSubZip(mock_gobj) == FALSE);

    fp.input.pl.stick_range.x = I_CONTROLLER_RANGE_MAX;   /* the same way */
    fp.input.pl.stick_range.y = 0;
    CHECK(ftPikachuSpecialHiCheckGotoSubZip(mock_gobj) == FALSE);

    fp.input.pl.stick_range.x = 0;      /* ninety degrees off */
    fp.input.pl.stick_range.y = I_CONTROLLER_RANGE_MAX;
    CHECK(ftPikachuSpecialHiCheckGotoSubZip(mock_gobj) != FALSE);

    fp.status_vars.pikachu.specialhi.is_subsequent_zip = TRUE;  /* already spent */
    CHECK(ftPikachuSpecialHiCheckGotoSubZip(mock_gobj) == FALSE);
    fp.status_vars.pikachu.specialhi.is_subsequent_zip = FALSE;

    /* ---- the End. It backs the zip velocity up and keeps a fifth of
     * it, which is the little slide out of the teleport. ---- */
    fp.ga = nMPKineticsAir;
    ftMainSetStatus(mock_gobj, nFTPikachuStatusSpecialAirHi, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.physics.vel_air.x = 100.0f;
    fp.physics.vel_air.y = 50.0f;
    ftPikachuSpecialAirHiEndSetStatus(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialAirHiEnd);
    CHECK_NEAR(fp.status_vars.pikachu.specialhi.vel_x_bak, 100.0f, 1e-3f);
    CHECK_NEAR(fp.status_vars.pikachu.specialhi.vel_y_bak, 50.0f, 1e-3f);
    CHECK_NEAR(fp.physics.vel_air.x, 100.0f * FTPIKACHU_QUICKATTACK_VEL_BAK_MUL, 1e-3f);
    CHECK_NEAR(fp.physics.vel_air.y, 50.0f * FTPIKACHU_QUICKATTACK_VEL_BAK_MUL, 1e-3f);

    fp.ga = nMPKineticsGround;
    ftMainSetStatus(mock_gobj, nFTPikachuStatusSpecialHi, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.physics.vel_ground.x = 200.0f;
    ftPikachuSpecialHiEndSetStatus(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialHiEnd);
    CHECK_NEAR(fp.physics.vel_ground.x, 200.0f * FTPIKACHU_QUICKATTACK_VEL_BAK_MUL, 1e-3f);

    /* the End's update is the second zip's one chance: flag1 == 1 is the
     * animation window, and a stick turned far enough inside it starts
     * the second teleport. Missing the window sets flag1 to 2, which
     * shuts it for good. */
    fp.status_vars.pikachu.specialhi.stick_range.x = I_CONTROLLER_RANGE_MAX;
    fp.status_vars.pikachu.specialhi.stick_range.y = 0;
    fp.input.pl.stick_range.x = 1;
    fp.input.pl.stick_range.y = 1;
    fp.motion_vars.flags.flag1 = 1;
    ftPikachuSpecialHiEndProcUpdate(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialHiEnd);
    CHECK(fp.motion_vars.flags.flag1 == 2);

    ftMainSetStatus(mock_gobj, nFTPikachuStatusSpecialHiEnd, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.status_vars.pikachu.specialhi.is_subsequent_zip = FALSE;
    fp.status_vars.pikachu.specialhi.stick_range.x = I_CONTROLLER_RANGE_MAX;
    fp.status_vars.pikachu.specialhi.stick_range.y = 0;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = I_CONTROLLER_RANGE_MAX;
    fp.motion_vars.flags.flag1 = 1;
    ftPikachuSpecialHiEndProcUpdate(mock_gobj);
    CHECK(fp.status_vars.pikachu.specialhi.is_subsequent_zip != FALSE);
    CHECK(fp.status_vars.pikachu.specialhi.anim_frames == FTPIKACHU_QUICKATTACK_ZIP_TIME);

    /* with the window shut, the animation's end is the way out -- Wait on
     * the ground, and the helpless fall in the air, which is what makes
     * the Quick Attack a recovery you can only use once */
    fp.ga = nMPKineticsGround;
    ftMainSetStatus(mock_gobj, nFTPikachuStatusSpecialHiEnd, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.motion_vars.flags.flag1 = 0;
    mock_gobj->anim_frame = 0.0f;
    ftPikachuSpecialHiEndProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusWait);

    fp.ga = nMPKineticsAir;
    ftMainSetStatus(mock_gobj, nFTPikachuStatusSpecialAirHiEnd, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.motion_vars.flags.flag1 = 0;
    mock_gobj->anim_frame = 0.0f;
    ftPikachuSpecialAirHiEndProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusFallSpecial);

    /* ---- the two ground/air switches. Unlike the Thunder's they pass
     * 0.0F as the animation rate, because the Start plays no animation at
     * all and the zip's is frozen. ---- */
    fp.ga = nMPKineticsGround;
    ftMainSetStatus(mock_gobj, nFTPikachuStatusSpecialHiStart, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftPikachuSpecialHiStartSwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialAirHiStart);
    CHECK(fp.ga == nMPKineticsAir);
    ftPikachuSpecialAirHiStartSwitchStatusGround(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialHiStart);
    CHECK(fp.ga == nMPKineticsGround);

    ftMainSetStatus(mock_gobj, nFTPikachuStatusSpecialHi, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.jumps_used = 0;
    ftPikachuSpecialHiSwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialAirHi);
    CHECK(fp.jumps_used == fp.attr->jumps_max);   /* the zip spends them */
    ftPikachuSpecialAirHiSwitchStatusGround(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialHi);

    ftMainSetStatus(mock_gobj, nFTPikachuStatusSpecialHiEnd, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    ftPikachuSpecialHiEndSwitchStatusAir(mock_gobj);
    CHECK_STATUS(nFTPikachuStatusSpecialAirHiEnd);
    ftPikachuSpecialAirHiEndSwitchStatusGround(mock_gobj);   /* unused in the game */
    CHECK_STATUS(nFTPikachuStatusSpecialHiEnd);

    /* ---- the model squash. The zip's physics proc turns joint 4 to
     * point along the velocity and stretches it along z, which is the
     * streak Pikachu becomes. ---- */
    fp.lr = +1;
    fp.physics.vel_air.x = 0.0f;
    fp.physics.vel_air.y = 100.0f;              /* straight up */
    ftPikachuSpecialHiUpdateModelPitchScale(mock_gobj);
    v0 = fp.joints[FTPIKACHU_QUICKATTACK_BASE_JOINT]->rotate.vec.f.x;
    CHECK_NEAR(v0, -3.14159265f / 2.0f, 1e-3f);   /* atan2(0,100) - 90 deg */
    CHECK_EQF(fp.joints[FTPIKACHU_QUICKATTACK_BASE_JOINT]->scale.vec.f.x, FTPIKACHU_QUICKATTACK_SCALE_X);
    CHECK_EQF(fp.joints[FTPIKACHU_QUICKATTACK_BASE_JOINT]->scale.vec.f.y, FTPIKACHU_QUICKATTACK_SCALE_Y);
    CHECK_EQF(fp.joints[FTPIKACHU_QUICKATTACK_BASE_JOINT]->scale.vec.f.z, FTPIKACHU_QUICKATTACK_SCALE_Z);

    /* a quarter turn of the velocity is a quarter turn of the model */
    fp.physics.vel_air.x = 100.0f;
    fp.physics.vel_air.y = 0.0f;
    ftPikachuSpecialHiUpdateModelPitchScale(mock_gobj);
    v1 = fp.joints[FTPIKACHU_QUICKATTACK_BASE_JOINT]->rotate.vec.f.x;
    CHECK_NEAR(fabsf(v1 - v0), 3.14159265f / 2.0f, 1e-3f);

    /* and the facing flips it: the streak points where he is going, not
     * where he is aimed */
    fp.lr = -1;
    ftPikachuSpecialHiUpdateModelPitchScale(mock_gobj);
    CHECK_NEAR(fp.joints[FTPIKACHU_QUICKATTACK_BASE_JOINT]->rotate.vec.f.x,
               -(v1 + 3.14159265f / 2.0f) - 3.14159265f / 2.0f, 1e-3f);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindMario;
}


/* AVOID_UB build-wide.
 *
 * The decomp guards twelve places where a function can fall off its end
 * or read an uninitialised value, and offers the definite answer behind
 * -DAVOID_UB. This port now sets it in DECOMP_DEFS, which is the one
 * variable both the target and the host build pick up, so the decomp
 * files compiled unmodified, the port's own C, this test and the disc
 * probe all take the same arm.
 *
 * Six of the twelve sites are in files this port compiles. Four of those
 * are reachable and are pinned below; the other two are defensive (an
 * arctan helper whose `type` is 0, 1 or 2 by construction, and
 * mnPlayersVSGetShade's tail, which its own early return and the four-
 * slot array between them make unreachable). The seventh site that
 * matters is not a return value at all -- see the note below.
 *
 * Without the option every check here reads whatever happened to be in
 * the return register, so a green run of this test is not proof on its
 * own; taking -DAVOID_UB back out is what proves it. */
static void test_avoid_ub(void)
{
    extern f32 gcGetAObjValue(AObj *aobj);
    extern f32 gcGetAObjRate(AObj *aobj);
    extern f32 gcGetDObjAxisTrack(DObj *dobj, s32 track);
    extern f32 gcGetDObjDescAxisTrack(DObjDesc *dobjdesc, s32 track);
    extern s32 mnPlayersVSGetShade(s32 player);
    extern GObj *wpYoshiStarMakeWeapon(GObj *fighter_gobj, Vec3f *pos, s32 lr);
    extern WPStruct *sWPManagerStructsAllocFree;
    AObj taobj;
    DObjDesc tdesc;
    DObj tdobj;
    Vec3f pos;
    DObj *saved_j16, *saved_j17;
    int free_before, free_after;
    WPStruct *w;

    /* ---- sys/objanim.c, four sites, all "the switch ran out of cases".
     * An AObj whose kind is None matches none of the three arms; the
     * option makes both readers answer 0 rather than fall through. ---- */
    memset(&taobj, 0, sizeof(taobj));
    taobj.kind = nGCAnimKindNone;
    taobj.value_base = 111.0f;
    taobj.value_target = 222.0f;
    taobj.rate_base = 333.0f;
    CHECK_EQF(gcGetAObjValue(&taobj), 0.0f);
    CHECK_EQF(gcGetAObjRate(&taobj), 0.0f);

    /* the three real kinds still answer as the game answers, which is
     * what says the option added an arm rather than replacing one */
    taobj.kind = nGCAnimKindStep;
    taobj.length_invert = 1.0f;
    taobj.length = 2.0f;
    CHECK_EQF(gcGetAObjValue(&taobj), 222.0f);   /* invert <= length */
    taobj.length_invert = 3.0f;
    CHECK_EQF(gcGetAObjValue(&taobj), 111.0f);
    CHECK_EQF(gcGetAObjRate(&taobj), 0.0f);
    taobj.kind = nGCAnimKindLinear;
    CHECK_EQF(gcGetAObjRate(&taobj), 333.0f);

    /* the same shape over a joint's tracks: nGCAnimTrackNone is below
     * every case in both readers */
    memset(&tdobj, 0, sizeof(tdobj));
    tdobj.rotate.vec.f.x = 1.0f;
    tdobj.translate.vec.f.y = 2.0f;
    tdobj.scale.vec.f.z = 3.0f;
    CHECK_EQF(gcGetDObjAxisTrack(&tdobj, nGCAnimTrackRotX), 1.0f);
    CHECK_EQF(gcGetDObjAxisTrack(&tdobj, nGCAnimTrackTraY), 2.0f);
    CHECK_EQF(gcGetDObjAxisTrack(&tdobj, nGCAnimTrackScaZ), 3.0f);
    CHECK_EQF(gcGetDObjAxisTrack(&tdobj, nGCAnimTrackNone), 0.0f);
    CHECK_EQF(gcGetDObjAxisTrack(&tdobj, nGCAnimTrackScaZ + 1), 0.0f);

    memset(&tdesc, 0, sizeof(tdesc));
    tdesc.rotate.x = 4.0f;
    tdesc.translate.y = 5.0f;
    tdesc.scale.z = 6.0f;
    CHECK_EQF(gcGetDObjDescAxisTrack(&tdesc, nGCAnimTrackRotX), 4.0f);
    CHECK_EQF(gcGetDObjDescAxisTrack(&tdesc, nGCAnimTrackTraY), 5.0f);
    CHECK_EQF(gcGetDObjDescAxisTrack(&tdesc, nGCAnimTrackScaZ), 6.0f);
    CHECK_EQF(gcGetDObjDescAxisTrack(&tdesc, nGCAnimTrackNone), 0.0f);

    /* ---- wp/wpyoshi/wpyoshistar.c: the one site that is a MISSING
     * RETURN VALUE rather than a missing case. Without the option
     * wpYoshiStarMakeWeapon ends in a bare `return;` from a GObj*
     * function; with it, it hands the star back. Its own caller
     * (wpYoshiStarMakeStars) ignores the result either way, which is why
     * the game never noticed -- the host test found it. ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindYoshi;
    pos.x = pos.y = pos.z = 0.0f;
    CHECK(wpYoshiStarMakeWeapon(mock_gobj, &pos, +1) == NULL);   /* unmodelled */

    /* ---- ft/ftchar/ftmario/ftmariospecialn.c and its Kirby copy: the
     * two sites that are an UNINITIALISED VALUE rather than a missing
     * return. Their fkind switch has no arm for a fighter who is neither
     * a Mario nor a Luigi, and without the option the default falls
     * through and spawns a fireball with an indeterminate item id. With
     * it the accessory proc returns and nothing is spawned.
     *
     * It is reachable: dFTCommonSpecialNStatusList gives MASTER HAND
     * Mario's setter (the decomp does too), so a Boss pressing B would
     * arrive here as nFTKindBoss. Nothing in this port spawns him, so it
     * was latent -- and it is now defined. ---- */
    saved_j16 = fp.joints[FTMARIO_FIREBALL_SPAWN_JOINT];
    saved_j17 = fp.joints[FTKIRBY_COPYMARIO_FIREBALL_SPAWN_JOINT];
    fp.joints[FTMARIO_FIREBALL_SPAWN_JOINT] = fp.joints[nFTPartsJointTopN];
    fp.joints[FTKIRBY_COPYMARIO_FIREBALL_SPAWN_JOINT] = fp.joints[nFTPartsJointTopN];

    free_before = 0;
    for (w = sWPManagerStructsAllocFree; w != NULL; w = w->next)
    {
        free_before++;
    }
    fp.fkind = nFTKindMario;
    fp.motion_vars.flags.flag0 = 1;
    ftMarioSpecialNProcAccessory(mock_gobj);
    CHECK(fp.motion_vars.flags.flag0 == 0);

    fp.fkind = nFTKindBoss;                      /* neither Mario nor Luigi */
    fp.motion_vars.flags.flag0 = 1;
    ftMarioSpecialNProcAccessory(mock_gobj);
    /* the flag is cleared either way: the decomp clears it at the top of
     * the block, BEFORE the switch, so the option's `return` cannot save
     * it. What the option changes is the line after the switch, and
     * neither arm is visible from here on host -- the fireball is
     * unmodelled and wpManagerMakeWeapon's host arm allocates nothing, so
     * the pool count does not move for a Mario either. What this pins is
     * that the arm runs at all without reading an uninitialised local:
     * the checks that carry the test are the four in sys/objanim.c
     * above, whose answers the option supplies outright. */
    CHECK(fp.motion_vars.flags.flag0 == 0);
    free_after = 0;
    for (w = sWPManagerStructsAllocFree; w != NULL; w = w->next)
    {
        free_after++;
    }
    CHECK(free_after == free_before);

    fp.fkind = nFTKindKirby;
    fp.passive_vars.kirby.copy_id = nFTKindBoss;
    fp.motion_vars.flags.flag0 = 1;
    ftKirbyCopyMarioSpecialNProcAccessory(mock_gobj);
    CHECK(fp.motion_vars.flags.flag0 == 0);

    fp.joints[FTMARIO_FIREBALL_SPAWN_JOINT] = saved_j16;
    fp.joints[FTKIRBY_COPYMARIO_FIREBALL_SPAWN_JOINT] = saved_j17;

    /* ---- src/dc/mnplayersvs.c: the port's own copy carries the guard
     * and now takes its arm. Defensive: the early return above it and the
     * four-slot array below make the tail unreachable, and this only
     * pins that the defined answer is 0. ---- */
    CHECK(mnPlayersVSGetShade(0) == 0);

    /* ---- The site that is not a return value at all:
     * sys/objscript.c gcParseGObjScript. Without the option it assigns
     * `gobj = gobj->gobjscripts` and then reads its loop bound at
     * `(uintptr_t)gobj + (offsetof(GObj, gobjscripts_num) -
     * offsetof(GObj, gobjscripts))` -- a FAKE MATCH that lands on the
     * right field only because of where the N64 compiler put them. On
     * the SH-4 that is a read into the script array. The decomp's own
     * comment says to use the guarded arm in real applications. Nothing
     * in this port calls it yet, so there is nothing to drive here; the
     * proof is that the file compiles the guarded arm, which
     * tools/check/reloc_check.py and the target link both see. ---- */

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindMario;
}


/* The weapon quads: Samus's Charge Shot and Pikachu's
 * airborne Thunder Jolt, projectiles modelled like Fox's Blaster.
 *
 * Five of the twelve WPDescs the port compiles carry flags 0x00 -- no
 * DObjDesc tree, no DL links -- which is wp/wpmanager.c's simplest path:
 * one display list bound to one DObj. Two of the five were already
 * packed (the fireball and the blaster); these are two more, and they
 * are simpler than either, because their WPAttributes.p_mobjsubs is NULL
 * and their CI4 tile carries its TLUT inline.
 *
 * sWPManagerModels is static in src/dc/wpmanager.c and
 * wpManagerAddModel's host arm stubs the tree unconditionally, so what
 * the host can check is the PACKS: that each one loads through the same
 * fighter_load the target's wpModelLoad calls, and that what comes back
 * is the quad the packer says it baked. */
static void test_weapon_quads(void)
{
    static const struct {
        const char *pack;
        const char *who;
        float radius;
        int shaded;             /* 2 = vertex colour, 0 = flat + ENV lerp */
    } kQuads[] = {
        { "wpsamuschargeshot.mdl", "Samus's Charge Shot",   21.2f,  2 },
        { "wppikachujolt.mdl",     "Pikachu's Thunder Jolt", 100.4f, 2 },
        /* found by the extern reloc walk. The stars are the
         * first weapon here whose batch lerps ENV and PRIM by the texel
         * instead of reading SHADE -- shaded 0, and the env colour baked
         * into the pack is their yellow. */
        { "wpyoshistar.mdl",       "Yoshi's stars",          329.5f, 0 },
        { "wpyoshieggthrow.mdl",   "Yoshi's thrown egg",     189.9f, 2 },
        /* The only one of the five that carries an MObj --
         * which is why its display list, which branches into segment 0xE,
         * could not bake until the extern walk could reach the MObjSub
         * table too. Its two palettes are two frames of that MObj, and
         * wpsamusbomb.c winds between them as the fuse burns. */
        { "wpsamusbomb.mdl",       "Samus's Bomb",           127.3f, 2 },
        /* the Ray Gun's shot, the fifth flags-0x00 weapon
         * the item model exporter had misread as a tree. Flat, green
         * ENV lerp, like the stars. */
        { "wplgunammo.mdl",        "The Ray Gun's shot",     53.2f,  0 },
    };
    Fighter pack;
    int pal_bank, i;

    for (i = 0; i < (int)(sizeof(kQuads) / sizeof(kQuads[0])); i++)
    {
        pal_bank = 0;
        memset(&pack, 0, sizeof(pack));
        CHECK(fighter_load(&pack, kQuads[i].pack, &pal_bank) == 0);
        if (pack.hd == NULL)
        {
            continue;                    /* the load failed; do not walk it */
        }
        /* one joint, one quad, one texture, and no animation at all --
         * the weapon's motion is the WPStruct's, not a script's */
        CHECK(pack.hd->joint_count == 1);
        CHECK(pack.hd->vert_count == 4);
        CHECK(pack.hd->tri_count == 2);
        CHECK(pack.hd->batch_count == 1);
        CHECK(pack.hd->anim_count == 0);
        /* four bake one picture straight into the batch; the Bomb keeps
         * an MObj with a frame per palette, so it has two textures and
         * the batch names the first of them */
        if (pack.mobjs != NULL)
        {
            CHECK(pack.hd->tex_count == 2);
            CHECK(pack.mobjs->mobj_count == 1);
        }
        else
        {
            CHECK(pack.hd->tex_count == 1);
        }
        CHECK(pack.hd->pal_count == 0);   /* the CI4 TLUT is folded in */
        /* the batch is textured: FPackBatch.tex is a real index rather
         * than the blaster's -1 */
        CHECK(pack.batches[0].tex >= 0);
        CHECK(pack.batches[0].tex < (int)pack.hd->tex_count);
        /* shaded 2 is "vertex colour", which is what a combine that reads
         * SHADE with lighting off bakes to -- the blaster's stage, and
         * the reason those need no light. shaded 0 is a flat material
         * colour, which is what the stars' ENV/PRIM lerp bakes to. */
        CHECK(pack.batches[0].shaded == kQuads[i].shaded);
        /* and the quad is square about the origin, which is what makes it
         * a billboard the weapon's own rotate turns */
        CHECK(fabsf(pack.hd->radius - kQuads[i].radius) < 0.5f);
        fighter_release(&pack);
    }
}


/* ft/ftcommon/ftcommonrebound.c, compiled unmodified.
 * ft/ftmain.c:2021 calls ftCommonReboundWaitSetStatus, which once
 * reached a do-nothing stub; the file and the two status
 * rows are what make the shield poke land. Every number below is the
 * decomp's arithmetic, not a transcription of it -- the functions under
 * test are the decomp's own translation units. */
static void test_shield_rebound(void)
{
    float rebound = 8.0f;

    spawn(0.0f, 0.0f);
    CHECK_STATUS(nFTCommonStatusWait);

    fp.attack_rebound = rebound;
    fp.hit_lr = fp.lr;                  /* hit from in front */
    ftCommonReboundWaitSetStatus(mock_gobj);

    CHECK_STATUS(nFTCommonStatusReboundWait);
    /* ftcommonrebound.c:36-40: the flinch is played back so that it
     * lasts exactly as long as the rebound does */
    CHECK_EQF(fp.status_vars.common.rebound.anim_speed,
              kMario.rebound_anim_length / rebound);
    CHECK_EQF(fp.status_vars.common.rebound.rebound_timer, rebound);
    /* hit from the front pushes backwards, at 2x the timer */
    CHECK_EQF(fp.physics.vel_ground.x, -2.0f * rebound);
    /* script id -1: the pose is held, so no motion is loaded */
    CHECK(fp.motion_id == -1);
    /* the row's proc_physics is the ground friction, so the push decays
     * rather than sliding forever */
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);

    /* ftcommonrebound.c:44-47: the wait's update is the handoff */
    idle_frames(1);
    CHECK_STATUS(nFTCommonStatusRebound);
    CHECK(fp.motion_id == nFTCommonMotionRebound);
    CHECK_EQF(fp.status_vars.common.rebound.rebound_timer, rebound);

    /* ftcommonrebound.c:10-20: the timer counts the flinch down and
     * Wait is what it ends in */
    idle_frames((int)rebound - 1);
    CHECK_STATUS(nFTCommonStatusRebound);
    idle_frames(1);
    CHECK_STATUS(nFTCommonStatusWait);

    /* hit from behind pushes the other way */
    spawn(0.0f, 0.0f);
    fp.attack_rebound = rebound;
    fp.hit_lr = -fp.lr;
    ftCommonReboundWaitSetStatus(mock_gobj);
    CHECK_EQF(fp.physics.vel_ground.x, +2.0f * rebound);
}

/* ftcommoncliffclimb.c:65-71: above FTCOMMON_CLIFF_DAMAGE_HIGH the same
 * three inputs pick the slow statuses instead. */
static void test_ledge_climb_slow(void)
{
    reach_cliff_wait();
    fp.percent_damage = FTCOMMON_CLIFF_DAMAGE_HIGH;
    mock_anim_len = 2.0f;

    frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusCliffSlow);
    CHECK(fp.status_vars.common.cliffmotion.status_id ==
          nFTCommonCliffKindClimbSlow);

    idle_frames(1);
    CHECK_STATUS(nFTCommonStatusCliffClimbSlow1);

    mock_transn[1] = mock_transn[2] = 0.0f;
    idle_frames(2);
    CHECK_STATUS(nFTCommonStatusCliffClimbSlow2);
    CHECK(fp.ga == nMPKineticsGround);

    idle_frames(2);
    CHECK_STATUS(nFTCommonStatusWait);
    mock_anim_len = 1e9f;
    fp.percent_damage = 0.0f;
}

/* fttypes.h:951 cliff_status_ga decides whether each cliff state is
 * grounded; Mario's ledge attack is the airborne one. Flip the climb to
 * airborne to drive ftCommonCliffCommon2ProcPhysics's other arm, which
 * does not integrate at all -- it follows the cliff line's own floor and
 * makes the velocity whatever gets there. */
static void test_ledge_climb_airborne(void)
{
    static FPackAttr air;
    float edge_x = -2000.0f, edge_y = 0.0f;

    air = kMario;
    air.cliff_status_ga[nFTCommonCliffKindClimbQuick] = nMPKineticsAir;
    mock_attr = &air;

    reach_cliff_wait();
    mock_anim_len = 2.0f;
    frame(80, 0, 0, 0, 0);
    idle_frames(1);
    CHECK_STATUS(nFTCommonStatusCliffClimbQuick1);
    mock_transn[1] = mock_transn[2] = 0.0f;
    idle_frames(2);
    CHECK_STATUS(nFTCommonStatusCliffClimbQuick2);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(!fp.is_jostle_ignore);
    /* still placed 5 units in from the vertex, but left in the air, and
     * held on the cliff's floor plus the raw TransN y
     * (ftcommoncliffclimb.c:187-189 -- unscaled, unlike the hang) */
    CHECK_NEAR(fp_pos.x, edge_x + 5.0f, 1e-3f);
    CHECK_NEAR(fp_pos.y, edge_y + mock_transn[1], 1e-3f);

    mock_anim_len = 1e9f;
    /* and it lands the moment the animation takes it through the floor
     * (ftcommoncliffclimb.c:253-256 mpCommonCheckFighterLanding) */
    mock_transn[1] = -30.0f;
    idle_frames(1);
    CHECK(fp.ga == nMPKineticsGround);

    mock_attr = &kMario;
}

static void test_ledge_drop(void)
{
    spawn_at_ledge();
    run_until(0, 0, 60, is_cliff_catch);
    CHECK_STATUS(nFTCommonStatusCliffCatch);

    mock_anim_len = 3.0f;
    idle_frames(4);
    CHECK_STATUS(nFTCommonStatusCliffWait);
    mock_anim_len = 1e9f;

    /* stick away from the stage drops off, and the regrab timer stops
     * the fighter catching the same ledge again immediately */
    frame(-80, 0, 0, 0, 0);
    CHECK(fp.status_id == nFTCommonStatusFall ||
          fp.status_id == nFTCommonStatusFallAerial);
    CHECK(!fp.is_cliff_hold);
    CHECK(fp.cliffcatch_wait > 0);

    idle_frames(10);
    CHECK(fp.status_id != nFTCommonStatusCliffCatch);
}

/* The hang times out after FTCOMMON_CLIFF_FALL_WAIT_DAMAGE_LOW frames
 * (ftcommoncliffcatchwait.c:105-112, 0 damage picks the long timer). Since
 * the timeout drops into the DamageFall tumble (the fighter
 * can tech it), not a plain Fall -- ftCommonCliffWaitCheckFall's real
 * ftCommonDamageFallSetStatusFromCliffWait. */
static void test_ledge_timeout(void)
{
    int i;

    spawn_at_ledge();
    run_until(0, 0, 60, is_cliff_catch);
    mock_anim_len = 3.0f;
    idle_frames(4);
    CHECK_STATUS(nFTCommonStatusCliffWait);
    mock_anim_len = 1e9f;

    for (i = 0; i < FTCOMMON_CLIFF_FALL_WAIT_DAMAGE_LOW + 8 &&
                fp.status_id == nFTCommonStatusCliffWait; i++)
        idle_frames(1);
    CHECK(i <= FTCOMMON_CLIFF_FALL_WAIT_DAMAGE_LOW);
    CHECK_STATUS(nFTCommonStatusDamageFall);
}

/* the Fly/tumble launch. A level-3 hit (knockback 64,
 * hitstun 34) that the clamp used to fold down to a level-2 stand-in now
 * really launches: DamageFlyN, then the tumble (DamageFall) once the
 * flinch and hitstun run out, then -- the knockdown floor being a separate
 * path -- a stand back into Wait when the tumble reaches the ground. */
static void test_tumble(void)
{
    spawn(0.0f, 0.0f);
    mock_anim_len = 4.0f;

    /* a launching hit dealt straight into the queued-damage fields the
     * game's own ftMainProcParams fills, routed through the ported entry
     * (ftCommonDamageGotoDamageStatus -> ftCommonDamageInitDamageVars). */
    fp.damage_queue = 30;
    fp.damage_knockback = 64.0f;    /* hitstun 34 -> damage_level 3 */
    fp.damage_angle = 45;           /* clear of the FlyTop window (70-110) */
    fp.damage_lr = -1;
    fp.damage_index = 1;            /* the N tier */
    fp.damage_element = nGMHitElementNormal;
    fp.damage_player_num = 1;
    ftCommonDamageGotoDamageStatus(mock_gobj);

    /* the un-clamp: the hit really launches now, into the Fly tier and
     * airborne, marked hitstun -- not the level-2 stand-in the clamp
     * used to leave. */
    CHECK_STATUS(nFTCommonStatusDamageFlyN);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK(fp.is_hitstun);

    /* the flinch hands to the tumble (ftCommonDamageAirCommonProcUpdate's
     * "anim ended and hitstun spent -> DamageFall"): DamageFall is a real
     * registered status, entered airborne with the fall physics. The
     * physics-timed handoff and the fall onto the floor run live on the
     * disc probe; here the two setters are driven directly. */
    ftCommonDamageFallSetStatusFromDamage(mock_gobj);
    CHECK_STATUS(nFTCommonStatusDamageFall);
    CHECK(fp.ga == nMPKineticsAir);

    /* the knockdown floor: the tumble's ground contact now
     * bounces the fighter DOWN (DownBounce), no longer straight to Wait.
     * Driven directly (the setter the ProcMap calls on floor contact); the
     * live floor landing is on the disc probe. The hip joint is unrotated
     * in the mock, so the fighter lands face-up (DownBounceU). */
    ftCommonDownBounceSetStatus(mock_gobj);
    CHECK_STATUS(nFTCommonStatusDownBounceU);
    CHECK(fp.ga == nMPKineticsGround);

    /* the bounce settles into the lie-down (DownWait): the auto-stand timer
     * is armed to its full length and incoming damage is halved. */
    ftCommonDownWaitSetStatus(mock_gobj);
    CHECK_STATUS(nFTCommonStatusDownWaitU);
    CHECK(fp.status_vars.common.downwait.stand_wait == FTCOMMON_DOWNWAIT_STAND_WAIT);
    CHECK(fp.damage_mul == 0.5f);

    /* the auto-stand timer running out gets the fighter up (DownStand):
     * drive it to its last tick and pump one ProcUpdate. */
    fp.status_vars.common.downwait.stand_wait = 1;
    ftCommonDownWaitProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusDownStandU);

    /* the get-up anim ending drops into Wait (ftAnimEndSetWait, the
     * DownStand Proc Update). Clear the queued-damage fields first, or the
     * frame chain's ftMainProcParams would re-launch off the stale queue. */
    fp.damage_queue = 0;
    fp.damage_knockback = 0.0f;
    mock_anim_len = 3.0f;
    idle_frames(6);
    CHECK_STATUS(nFTCommonStatusWait);
    mock_anim_len = 1e9f;

    /* the manual get-up also works: from a fresh lie-down, a hard stick-up
     * stands the fighter at once (ftCommonDownStandCheckInterruptCommon),
     * without waiting for the timer. */
    ftCommonDownWaitSetStatus(mock_gobj);
    CHECK_STATUS(nFTCommonStatusDownWaitU);
    fp.input.pl.stick_range.y = 80;
    CHECK(ftCommonDownStandCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusDownStandU);
    fp.input.pl.stick_range.y = 0;

    /* the ground tech. Entered from the damage side: a fighter
     * that pressed Z within FTCOMMON_PASSIVE_BUFFER_TICS_MAX (20) frames of
     * touching down techs instead of knocking down. Drive the two damage-side
     * checks directly (the same way ftCommonDamageAirCommonProcMap does),
     * grounded, with the damage queue cleared so the frame chain stays put. */
    fp.ga = nMPKineticsGround;
    fp.damage_queue = 0;
    fp.damage_knockback = 0.0f;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = 0;

    /* stick centred, inside the window -> neutral tech (Passive). */
    fp.tics_since_last_z = 5;
    CHECK(ftCommonPassiveCheckInterruptDamage(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusPassive);

    /* the Z buffer expired -> no tech (the launch would fall through to the
     * knockdown floor); status must not become a tech. */
    fp.tics_since_last_z = FTCOMMON_PASSIVE_BUFFER_TICS_MAX;
    CHECK(ftCommonPassiveCheckInterruptDamage(mock_gobj) == FALSE);
    CHECK(ftCommonPassiveStandCheckInterruptDamage(mock_gobj) == FALSE);

    /* stick pushed toward the facing (x*lr >= 0), inside the window -> the
     * forward roll (PassiveStandF); pushed away -> the back roll (B). */
    fp.tics_since_last_z = 5;
    fp.lr = 1;
    fp.input.pl.stick_range.x = FTCOMMON_PASSIVE_F_OR_B_RANGE;
    CHECK(ftCommonPassiveStandCheckInterruptDamage(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusPassiveStandF);

    fp.tics_since_last_z = 5;
    fp.input.pl.stick_range.x = -FTCOMMON_PASSIVE_F_OR_B_RANGE;
    CHECK(ftCommonPassiveStandCheckInterruptDamage(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusPassiveStandB);

    /* stick short of the roll threshold -> PassiveStand declines (a centred
     * tech would still take it via ftCommonPassiveCheckInterruptDamage). */
    fp.tics_since_last_z = 5;
    fp.input.pl.stick_range.x = FTCOMMON_PASSIVE_F_OR_B_RANGE - 1;
    CHECK(ftCommonPassiveStandCheckInterruptDamage(mock_gobj) == FALSE);
    fp.input.pl.stick_range.x = 0;
}

/* the wall bounce. A launched fighter whose map proc finds a
 * wall/ceiling beside it (ftCommonWallDamageCheckGoto) slams into it: the
 * stacked knockback is reflected off the surface normal, kept at 0.8, and
 * the fighter enters WallDamage (status 56) intangible, then its Proc
 * Update drops it back into DamageFall when the reflected hitstun ends.
 * The impact wave / quake effects and the disc-side floor physics run live
 * on the probe; here the branch, the reflection math and the handoff are
 * driven directly (the same way ftCommonDamageAirCommonProcMap does). */
static void test_wall_bounce(void)
{
    spawn(0.0f, 0.0f);
    mock_anim_len = 4.0f;

    /* no wall touched -> CheckGoto declines (the launch would keep falling). */
    fp.status_vars.common.damage.coll_mask_curr = 0;
    CHECK(ftCommonWallDamageCheckGoto(mock_gobj) == FALSE);

    /* set up a fighter stacked with knockback (30, 40) meeting a left wall
     * whose normal points into the stage (1, 0): reflecting (30,40) across
     * (1,0) gives (-30,40), scaled 0.8 -> (-24,32), magnitude 40. */
    fp.physics.vel_air.x = fp.physics.vel_air.y = fp.physics.vel_air.z = 0.0f;
    fp.physics.vel_damage_air.x = 30.0f;
    fp.physics.vel_damage_air.y = 40.0f;
    fp.physics.vel_damage_air.z = 0.0f;
    fp.coll_data.lwall_angle.x = 1.0f;
    fp.coll_data.lwall_angle.y = 0.0f;
    fp.coll_data.lwall_angle.z = 0.0f;
    fp.status_vars.common.damage.coll_mask_curr = MAP_FLAG_LWALL;
    fp.is_hitstun = TRUE;
    fp.intangible_tics = 0;

    CHECK(ftCommonWallDamageCheckGoto(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusWallDamage);

    /* the reflected, scaled knockback becomes the new damage velocity, the
     * live air velocity is zeroed, and the facing flips to the bounce. */
    CHECK_EQF(fp.physics.vel_damage_air.x, -24.0f);
    CHECK_EQF(fp.physics.vel_damage_air.y, 32.0f);
    CHECK_EQF(fp.physics.vel_air.x, 0.0f);
    CHECK_EQF(fp.physics.vel_air.y, 0.0f);
    CHECK(fp.lr == +1);                 /* vel_damage_air.x < 0 -> +1 */
    CHECK_EQF(fp.damage_knockback_stack, 40.0f);

    /* the slam sets hitstun from the reflected knockback, grants at least
     * the wall-damage intangibility window, and clears the public hitstun
     * flag (the fighter is committed to the bounce, not flinching). */
    CHECK(fp.status_vars.common.damage.hitstun_tics == (s32)ftParamGetHitStun(40.0f));
    CHECK(fp.status_vars.common.damage.hitstun_tics > 0);
    CHECK(fp.intangible_tics >= FTCOMMON_WALLDAMAGE_INTANGIBLE_TIMER);
    CHECK(fp.special_hitstatus == nGMHitStatusIntangible);
    CHECK(fp.is_hitstun == FALSE);

    /* the Proc Update decrements the reflected hitstun; on its last tic the
     * bounce hands back to the DamageFall tumble to finish the fall. */
    fp.status_vars.common.damage.hitstun_tics = 1;
    fp.damage_queue = 0;
    fp.damage_knockback = 0.0f;
    ftCommonWallDamageProcUpdate(mock_gobj);
    CHECK_STATUS(nFTCommonStatusDamageFall);

    /* a right wall and a ceiling take the other two arms of CheckGoto. */
    ftMainSetStatus(mock_gobj, nFTCommonStatusDamageFlyN, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.physics.vel_damage_air.x = 0.0f;
    fp.physics.vel_damage_air.y = 50.0f;
    fp.coll_data.rwall_angle.x = -1.0f;
    fp.coll_data.rwall_angle.y = 0.0f;
    fp.coll_data.rwall_angle.z = 0.0f;
    fp.status_vars.common.damage.coll_mask_curr = MAP_FLAG_RWALL;
    CHECK(ftCommonWallDamageCheckGoto(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusWallDamage);

    ftMainSetStatus(mock_gobj, nFTCommonStatusDamageFlyN, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.coll_data.ceil_angle.x = 0.0f;
    fp.coll_data.ceil_angle.y = -1.0f;
    fp.coll_data.ceil_angle.z = 0.0f;
    fp.status_vars.common.damage.coll_mask_curr = MAP_FLAG_CEIL;
    CHECK(ftCommonWallDamageCheckGoto(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusWallDamage);

    fp.status_vars.common.damage.coll_mask_curr = 0;
}
