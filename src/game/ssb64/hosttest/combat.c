/* hosttest/combat.c -- part of hosttest_ft.c: fighter against fighter: hit detection, guard, rolls, grabs, the
 * attack statuses, weapon and item hits, and picking items up.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* ---- hit detection --------------------------------------- */

/* gm/gmcollision.c over the FTParts every joint carries: a point in a
 * joint's space comes out where the DObj chain -- TopN's scale, the
 * hip's bind translate -- puts it, and moves with the fighter. Since
 * the chain is the game's: entry k at joints[4 + k], no
 * TransN in Wait, and a TransN a ledge motion links reaching nothing
 * (it is unlinked behind the attach, ft/ftmain.c:4710-4721). */
static void test_parts_world_position(void)
{
    Vec3f v;

    spawn(0.0f, 0.0f);
    CHECK(ftGetParts(fp.joints[nFTPartsJointTopN]) != NULL);
    CHECK(ftGetParts(fp.joints[6]) != NULL);
    CHECK(ftGetParts(fp.joints[6])->joint_id == 6);
    /* Wait links no hidden part: slots 1 to 3 are empty */
    CHECK(fp.joints[nFTPartsJointTransN] == NULL);
    CHECK(fp.joints[nFTPartsJointXRotN] == NULL);
    CHECK(fp.joints[nFTPartsJointYRotN] == NULL);
    CHECK(fp.joints[nFTPartsJointTopN]->child == fp.joints[4]);
    /* the origin of joint 6: the hip's bind (0, 150, 0) through TopN's
     * scale (attr->size 1.12), the torso and the limb at zero */
    v.x = v.y = v.z = 0.0f;
    gmCollisionGetFighterPartsWorldPosition(fp.joints[6], &v);
    CHECK_NEAR(v.x, fp_pos.x, 1e-3f);
    CHECK_NEAR(v.y, fp_pos.y + 168.0f, 1e-3f);
    CHECK_NEAR(v.z, 0.0f, 1e-3f);
    /* an offset in that space: TopN scales it by attr->size and turns
     * it a quarter turn about y -- a fighter facing right (lr +1) has
     * rotate.y = pi/2, so the joint's +z is the world's +x */
    v.x = 30.0f; v.y = 40.0f; v.z = 50.0f;
    gmCollisionGetFighterPartsWorldPosition(fp.joints[6], &v);
    CHECK_NEAR(v.x, fp_pos.x + 56.0f, 1e-3f);
    CHECK_NEAR(v.y, fp_pos.y + 168.0f + 44.8f, 1e-3f);
    CHECK_NEAR(v.z, -33.6f, 1e-3f);
    /* the cache goes stale as the fighter moves: walk a frame, ask again */
    frame(80, 0, 0, 0, 0);
    frame(80, 0, 0, 0, 0);
    CHECK(fp_pos.x > 0.0f);
    v.x = v.y = v.z = 0.0f;
    gmCollisionGetFighterPartsWorldPosition(fp.joints[6], &v);
    CHECK_NEAR(v.x, fp_pos.x, 1e-3f);
    /* facing left is the other quarter turn: +z becomes -x */
    spawn_desc(0.0f, 0.0f, -1, TRUE);
    ftParamUnlockPlayerControl(mock_gobj);
    v.x = 30.0f; v.y = 40.0f; v.z = 50.0f;
    gmCollisionGetFighterPartsWorldPosition(fp.joints[6], &v);
    CHECK_NEAR(v.x, fp_pos.x - 56.0f, 1e-2f);
    CHECK_NEAR(v.z, 33.6f, 1e-2f);
}

static int is_wait(void) { return fp.status_id == nFTCommonStatusWait; }

/* The guard's arithmetic on a stick (sx, sy) for a fighter facing lr
 * (ftcommonguard1.c ftCommonGuardUpdateShieldAngle), and what the mock's
 * linear script then makes of the track that last targeted `*prev`
 * (gcParseDObjAnimJoint SetValBlock: base = the old target, rate =
 * (target - base) / 45, value = base + frame * rate; then
 * ftCommonGuardGetJointTransform's pull toward the DObjDesc's zero by
 * the lean). Returns the rotation the joint should show; `*prev` becomes
 * the sector's target, as the AObj's does. */
static float shield_expect(float *prev, const float *targets, int sx, int sy,
                           int lr, int *sector)
{
    float angle = atan2f((float)sy, (float)(sx * lr));
    float deg, f, range, val;
    int i;

    if (angle < 0.0f)
        angle += 2.0f * (float)M_PI;
    deg = angle * 180.0f / (float)M_PI;
    if (deg > 359.0f)
        deg = 359.0f;
    i = (int)(deg / 45.0f);
    f = deg - (float)i * 45.0f;
    range = sqrtf((float)(sx * sx + sy * sy)) / 80.0f;
    if (range > 1.0f)
        range = 1.0f;
    val = *prev + f * (targets[i] - *prev) / (float)MOCK_SHIELD_FRAMES;
    *prev = targets[i];
    *sector = i;
    return val * range;
}

static int is_guard(void) { return fp.status_id == nFTCommonStatusGuard; }

/* ft/ftcommon/ftcommonguard1.c and guard2.c: Z in Wait is
 * GuardOn, whose ShieldOn animation links XRotN and YRotN; when it ends,
 * Guard holds the pose the shield-pose tables make for the stick's sector
 * and lean, YRotN scaled to the shield; letting go is GuardOff and then
 * Wait; the shield decays a point every sixteen frames; a jump out of it
 * is GuardKneeBend; a hit on it is shield stun. */
/* The shield bubble: the effect pool is built by the
 * effect tests' own reset, further down; the two colour words are what
 * src/dc/efmanager.c's host arm records where the target would set them
 * on the pack; gGCScaleX is sys/objdisplay.c's, which matrix kind 0x4F
 * writes. */
static void ef_pool_reset(void);
extern s32 sEFManagerStructsFreeNum;
extern u32 gEFManagerShieldLastPrim;
extern u32 gEFManagerShieldLastEnv;
extern f32 gGCScaleX;

/* One guarding fighter's bubble, checked where the guard has just put it
 * up: the effect efManagerShieldMakeEffect made on the effect link, its
 * struct's fighter and player, its root DObj's user_data on YRotN and
 * its matrix kinds -- 0x4F on the root, 0x2C on the child under it --
 * and what 0x4F computes: YRotN's world matrix, whose translation is the
 * joint's world position (gmCollisionGetFighterPartsWorldPosition agrees)
 * and whose first row's length is the joint's world scale, left in
 * gGCScaleX for the billboard. Then the colours the display proc sets:
 * PRIM white and ENV the player's, both at 0xC0. */
static void check_bubble(GObj *fighter_gobj, FTStruct *f, u32 want_env)
{
    GObj *bubble = f->status_vars.common.guard.effect_gobj;
    DObj *yrotn = f->joints[nFTPartsJointYRotN];
    EFStruct *ep;
    DObj *root;
    float m[16];
    Vec3f at;

    CHECK(bubble != NULL);
    CHECK(f->is_effect_attach);
    if (bubble == NULL || yrotn == NULL)
    {
        return;
    }
    ep = efGetStruct(bubble);
    root = DObjGetStruct(bubble);
    CHECK(bubble->link_id == nGCCommonLinkIDEffect);
    CHECK(ep != NULL);
    CHECK(root != NULL);
    if (ep == NULL || root == NULL)
    {
        return;
    }
    CHECK(ep->fighter_gobj == fighter_gobj);
    CHECK(ep->effect_vars.shield.player == f->player);
    CHECK(!ep->effect_vars.shield.is_damage_shield);
    CHECK(ep->proc_update == efManagerShieldProcUpdate);
    CHECK(root->user_data.p == yrotn);
    CHECK(root->xobjs_num == 3);
    CHECK(root->xobjs[0]->kind == 0x4F);
    CHECK(root->xobjs[1]->kind == nGCMatrixKindNull);
    CHECK(root->xobjs[2]->kind == nGCMatrixKindNull);
    CHECK(root->child != NULL);
    if (root->child != NULL)
    {
        CHECK(root->child->xobjs[0]->kind == nGCMatrixKindRecalcRotRpyRSca);
        CHECK(root->child->xobjs[1]->kind == nGCMatrixKindNull);
    }
    /* the joint's own origin walked out to the world: the game's helper
     * transforms the vector it is given, so it starts at zero (a missing
     * initialiser would read whatever the stack
     * held, which was zero until the guard test walked its stick over) */
    at.x = at.y = at.z = 0.0f;
    gmCollisionGetFighterPartsWorldPosition(yrotn, &at);
    gGCScaleX = 1.0f;
    CHECK(gcDObjLocalMatrixF(m, root, 0x4F) == TRUE);
    CHECK_NEAR(m[3], at.x, 1e-2f);
    CHECK_NEAR(m[7], at.y, 1e-2f);
    CHECK_NEAR(m[11], at.z, 1e-2f);
    CHECK_NEAR(gGCScaleX, sqrtf(m[0] * m[0] + m[4] * m[4] + m[8] * m[8]), 1e-4f);
    CHECK_NEAR(gGCScaleX, yrotn->scale.vec.f.x * DObjGetStruct(fighter_gobj)->scale.vec.f.x, 1e-2f);
    CHECK(m[12] == 0.0f && m[13] == 0.0f && m[14] == 0.0f && m[15] == 1.0f);

    efManagerShieldProcDisplay(bubble);
    CHECK(gEFManagerShieldLastPrim == 0xFFFFFFC0);
    CHECK(gEFManagerShieldLastEnv == want_env);
}

/* efmanager.c:1362,1375: neither desc is in ef/efmanager.h -- named
 * here, ahead of the rest of the fifty-three EFDescs' own block below,
 * because the egg escape test needs both to check its desc reuses the
 * shield's model. */
extern EFDesc dEFManagerYoshiShieldEffectDesc;
extern EFDesc dEFManagerYoshiEggEscapeEffectDesc;
extern EFDesc dEFManagerFireSparkEffectDesc;

static void test_guard(void)
{
    float prev_hip, prev_yrotn, want;
    int sector, i;
    DObj *hip, *yrotn;

    ef_pool_reset();
    spawn(0.0f, 0.0f);
    mock_anim_len = 3.0f;
    CHECK(fp.shield_health == 55);
    CHECK_EQF(fp.attr->shield_size, 260.0f);
    CHECK(fp.attr->dobj_lookup == (DObjDesc *)mock_shield_lookup);
    CHECK(fp.attr->shield_anim_joints[3] == (AObjEvent32 **)mock_shield_tables[3]);

    /* Z held, stick a little up: GuardOn, sector 0 at frame ~21.8. The
     * tilt has to be an old one, not a fresh one: the
     * shield's cascade has the roll in it, and a stick past 56 that
     * crossed 20 inside the last four tics is a roll, not a lean
     * (ftCommonEscapeGetStatus), and past 56 inside three it is a dash.
     * Walking the stick over under both thresholds first
     * is what the game asks of a player who wants the shield this far
     * over, and it is what the rest of this test needs. */
    frame(30, 0, 0, 0, 0);
    frame(30, 0, 0, 0, 0);
    frame(30, 0, 0, 0, 0);
    frame(30, 0, 0, 0, 0);
    CHECK(fp.tap_stick_x >= FTCOMMON_ESCAPE_BUFFER_TICS_MAX);
    frame(80, 32, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuardOn);
    CHECK(fp.motion_id == nFTCommonMotionGuardOn);
    CHECK(fp.is_shield);
    CHECK(fp.joints[nFTPartsJointXRotN] != NULL);
    CHECK(fp.joints[nFTPartsJointYRotN] != NULL);
    /* the bubble, player 1's red */
    check_bubble(mock_gobj, &fp, 0xFF0000C0);
    CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM - 1);
    CHECK(fp.joints[nFTPartsJointTransN] == NULL);
    yrotn = fp.joints[nFTPartsJointYRotN];
    hip = fp.joints[nFTPartsJointCommonStart];
    CHECK(yrotn->parent == fp.joints[nFTPartsJointXRotN]);
    CHECK(fp.joints[nFTPartsJointXRotN]->parent == fp.joints[nFTPartsJointTopN]);
    CHECK(ftGetParts(yrotn) != NULL);
    CHECK_NEAR(yrotn->scale.vec.f.x, (0.65f + 0.35f) * 260.0f / 30.0f, 1e-4f);
    CHECK_NEAR(yrotn->scale.vec.f.z, yrotn->scale.vec.f.x, 1e-6f);
    CHECK(fp.status_vars.common.guard.angle_i == 0);
    CHECK_NEAR(fp.status_vars.common.guard.angle_f, 21.8f, 0.3f);
    CHECK_EQF(fp.status_vars.common.guard.shield_rotate_range, 1.0f);
    CHECK(fp.status_vars.common.guard.release_lag == FTCOMMON_GUARD_RELEASE_LAG);
    CHECK(fp.status_vars.common.guard.shield_decay_wait == FTCOMMON_GUARD_DECAY_INT);
    CHECK(!fp.status_vars.common.guard.is_release);
    CHECK(fp.status_vars.common.guard.slide_tics == 0);
    /* GuardOn animates YRotN alone, from the sector table's last script;
     * the hip waits for Guard */
    prev_yrotn = 0.0f;
    want = shield_expect(&prev_yrotn, kMockShieldYRotZ, 80, 32, 1, &sector);
    CHECK(sector == 0);
    CHECK_NEAR(yrotn->rotate.vec.f.z, want, 3e-3f);
    CHECK(yrotn->anim_wait == AOBJ_ANIM_NULL);
    CHECK_EQF(hip->rotate.vec.f.y, 0.0f);

    /* the animation ends: Guard, the pose held every frame */
    for (i = 0; i < 6 && fp.status_id == nFTCommonStatusGuardOn; i++)
    {
        frame(80, 32, 0, N64_Z, 0);
        want = shield_expect(&prev_yrotn, kMockShieldYRotZ, 80, 32, 1, &sector);
        if (fp.status_id == nFTCommonStatusGuardOn)
            CHECK_NEAR(yrotn->rotate.vec.f.z, want, 3e-3f);
    }
    CHECK_STATUS(nFTCommonStatusGuard);
    CHECK(fp.motion_id == -1);
    CHECK(fp.anim_desc.flags.is_anim_joint);
    CHECK(fp.is_shield);
    CHECK(fp.joints[nFTPartsJointXRotN] != NULL);
    CHECK(fp.joints[nFTPartsJointYRotN] == yrotn);
    prev_hip = 0.0f;
    want = shield_expect(&prev_hip, kMockShieldHipY, 80, 32, 1, &sector);
    CHECK_NEAR(hip->rotate.vec.f.y, want, 3e-3f);
    CHECK_EQF(hip->translate.vec.f.y, 150.0f);
    CHECK(hip->anim_wait == AOBJ_ANIM_NULL);
    CHECK_NEAR(yrotn->rotate.vec.f.z, want = shield_expect(&prev_yrotn, kMockShieldYRotZ, 80, 32, 1, &sector), 3e-3f);
    /* YRotN's translation is pulled toward its row's (0, 195, 30) by
     * 1 - lean; at full lean the script's, which is the reset's zero */
    CHECK_NEAR(yrotn->translate.vec.f.y, 0.0f, 1e-3f);

    /* another sector, a shorter stick: table 1 from where the track was,
     * at 90% lean */
    frame(40, 60, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuard);
    want = shield_expect(&prev_hip, kMockShieldHipY, 40, 60, 1, &sector);
    CHECK(sector == 1);
    CHECK(fp.status_vars.common.guard.angle_i == 1);
    CHECK_NEAR(fp.status_vars.common.guard.shield_rotate_range, sqrtf(40.0f * 40.0f + 60.0f * 60.0f) / 80.0f, 1e-4f);
    CHECK_NEAR(hip->rotate.vec.f.y, want, 3e-3f);
    CHECK_NEAR(yrotn->translate.vec.f.y, 195.0f * (1.0f - fp.status_vars.common.guard.shield_rotate_range), 1e-2f);
    frame(0, 0, 0, N64_Z, 0);
    CHECK(fp.status_vars.common.guard.angle_i == 0);
    CHECK_EQF(fp.status_vars.common.guard.shield_rotate_range, 0.0f);
    CHECK_EQF(hip->rotate.vec.f.y, 0.0f);
    CHECK_NEAR(yrotn->translate.vec.f.y, 195.0f, 1e-3f);
    CHECK_NEAR(yrotn->translate.vec.f.z, 30.0f, 1e-3f);

    /* the shield decays a point every sixteen frames it is up, and the
     * sphere with it */
    CHECK(fp.shield_health == 55);
    for (i = 0; i < 16 && fp.shield_health == 55; i++)
        frame(0, 0, 0, N64_Z, 0);
    CHECK(fp.shield_health == 54);
    CHECK(fp.status_vars.common.guard.shield_decay_wait == FTCOMMON_GUARD_DECAY_INT);
    CHECK_NEAR(yrotn->scale.vec.f.x, ((0.65f * (54.0f / 55.0f)) + 0.35f) * 260.0f / 30.0f, 1e-4f);

    /* let go: GuardOff. The release lag counts down from the moment the
     * shield goes up (ftCommonGuardUpdateShieldVars decrements it every
     * frame), so after eight frames of Guard the shield drops the frame Z
     * is released; the ShieldOff animation still links YRotN, and Wait
     * ejects it */
    CHECK(fp.status_vars.common.guard.release_lag == 0);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusGuardOff);
    CHECK(fp.motion_id == nFTCommonMotionGuardOff);
    CHECK(fp.status_vars.common.guard.is_release);
    CHECK(!fp.is_shield);
    CHECK(fp.joints[nFTPartsJointYRotN] != NULL);
    /* GuardOff preserves effects through ftMainSetStatus, but the bubble
     * goes with the shield: ftCommonGuardOffProcUpdate runs
     * ftParamProcStopEffect the frame the release lag ends, which here
     * is this one, and the struct is back on the pool */
    CHECK(!fp.is_effect_attach);
    CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
    run_until(0, 0, 8, is_wait);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(!fp.is_shield);
    CHECK(fp.joints[nFTPartsJointXRotN] == NULL);
    CHECK(fp.joints[nFTPartsJointYRotN] == NULL);
    CHECK(fp.shield_health == 54);
    CHECK(!fp.is_effect_attach);

    /* facing left, the stick's angle is mirrored: right is sector 4 */
    spawn_desc(0.0f, 0.0f, -1, TRUE);
    ftParamUnlockPlayerControl(mock_gobj);
    mock_anim_len = 3.0f;
    frame(80, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuardOn);
    CHECK(fp.status_vars.common.guard.angle_i == 4);
    CHECK_NEAR(fp.status_vars.common.guard.angle_f, 0.0f, 0.3f);
    run_until(0, 0, 8, is_wait);        /* Z let go: GuardOn ends into GuardOff into Wait */
    CHECK_STATUS(nFTCommonStatusWait);

    /* let go within the lag: the shield stays up through GuardOff */
    spawn(0.0f, 0.0f);
    mock_anim_len = 3.0f;
    frame(0, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuardOn);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusGuardOn);
    CHECK(fp.status_vars.common.guard.is_release);
    CHECK(fp.is_shield);
    frame(0, 0, 0, 0, 0);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusGuardOff);
    CHECK(fp.is_shield);
    CHECK(fp.status_vars.common.guard.release_lag == FTCOMMON_GUARD_RELEASE_LAG - 3);
    /* and the bubble is up as long as the shield is */
    CHECK(fp.is_effect_attach);
    CHECK(fp.status_vars.common.guard.effect_gobj != NULL);
    run_until(0, 0, 8, is_wait);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(!fp.is_shield);
    CHECK(!fp.is_effect_attach);

    /* a jump out of the shield: GuardKneeBend, KneeBend's own procs */
    spawn(0.0f, 0.0f);
    mock_anim_len = 3.0f;
    frame(0, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuardOn);
    for (i = 0; i < 8 && !is_guard(); i++)
        frame(0, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuard);
    frame(0, 80, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuardKneeBend);
    CHECK(fp.motion_id == nFTCommonMotionGuardKneeBend);
    CHECK(fp.is_special_interrupt);
    CHECK(fp.status_vars.common.kneebend.input_source == FTCOMMON_KNEEBEND_INPUT_TYPE_STICK);
    CHECK(!fp.is_shield);
    CHECK(fp.joints[nFTPartsJointYRotN] == NULL);

    /* shield stun: a jab on a held shield is GuardSetOff for the hit's
     * damage * 1.62 + 4 frames (ftmain.c ftMainUpdateShieldStatFighter:
     * shield_damage is the hit's damage, 14 here; the shield loses that
     * plus the hitbox's own shield damage, 3), sliding at twice that
     * many units a frame, toward the attacker when facing them */
    spawn(0.0f, 0.0f);
    spawn_second(100.0f, 0.0f, -1);
    mock_anim_len = 3.0f;
    mock_in2.button_hold = N64_Z;
    frame(0, 0, 0, 0, 0);
    CHECK(fp2.status_id == nFTCommonStatusGuardOn);
    for (i = 0; i < 6 && fp2.status_id != nFTCommonStatusGuard; i++)
        frame(0, 0, 0, 0, 0);
    CHECK(fp2.status_id == nFTCommonStatusGuard);
    CHECK(fp2.is_shield);
    CHECK(fp2.joints[nFTPartsJointYRotN] != NULL);
    mock_anim_len = 12.0f;
    frame(0, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttack11);
    for (i = 0; i < 6 && fp2.status_id == nFTCommonStatusGuard; i++)
        frame(0, 0, 0, N64_A, 0);
    CHECK(fp2.status_id == nFTCommonStatusGuardSetOff);
    CHECK(fp2.percent_damage == 0);
    CHECK(fp2.shield_health == 55 - (14 + 3));
    /* the blocked hit flags the bubble for one frame of colour row 4 --
     * the setter raises is_damage_shield, the display proc reads it, and
     * the effect's next update clears it (efManagerShieldProcUpdate) */
    {
        GObj *bubble = fp2.status_vars.common.guard.effect_gobj;

        CHECK(bubble != NULL);
        if (bubble != NULL)
        {
            EFStruct *ep = efGetStruct(bubble);

            CHECK(ep->effect_vars.shield.is_damage_shield);
            efManagerShieldProcDisplay(bubble);
            CHECK(gEFManagerShieldLastPrim == 0xFFFFFFC0);
            CHECK(gEFManagerShieldLastEnv == 0xC0C0C0C0);
            efManagerShieldProcUpdate(bubble);
            CHECK(!ep->effect_vars.shield.is_damage_shield);
            efManagerShieldProcDisplay(bubble);
            CHECK(gEFManagerShieldLastEnv == 0x00FF00C0);   /* player 2's green */
        }
    }
    CHECK(fp2.shield_lr == -1);
    CHECK(fp2.is_shield);
    CHECK(fp2.status_vars.common.guard.is_setoff);
    CHECK_NEAR(fp2.status_vars.common.guard.setoff_frames,
               14.0f * FTCOMMON_GUARD_SETOFF_MUL + FTCOMMON_GUARD_SETOFF_ADD, 1e-3f);
    CHECK_NEAR(fp2.physics.vel_ground.x,
               -2.0f * fp2.status_vars.common.guard.setoff_frames, 1e-3f);
    /* the stun runs out with Z still held: back to Guard */
    for (i = 0; i < 40 && fp2.status_id == nFTCommonStatusGuardSetOff; i++)
        frame(0, 0, 0, 0, 0);
    CHECK(fp2.status_id == nFTCommonStatusGuard);
    mock_in2.button_hold = 0;
    mock_anim_len = 1e9f;

    /* Yoshi's egg (efmanager.c:4172 efManagerYoshiShieldMakeEffect), made
     * on the Mario stand-in -- the maker reads nothing of the fighter but
     * his player, his YRotN and his flag: one DObj at scale 1.5 in x and
     * y whose kinds are 0x50 (YRotN's world position) and 0x2C (the
     * billboard), and a display proc that darkens ENV as the shield
     * weakens: 1 - health / 55 of (0xAE, 0xD6, 0xD6), alpha 0 */
    spawn(0.0f, 0.0f);          /* takes the stun case's fighters and their bubble down first */
    ef_pool_reset();
    {
        GObj *egg = efManagerYoshiShieldMakeEffect(mock_gobj);

        CHECK(egg != NULL);
        CHECK(fp.is_effect_attach);
        if (egg != NULL)
        {
            DObj *root = DObjGetStruct(egg);

            CHECK(efGetStruct(egg)->fighter_gobj == mock_gobj);
            CHECK(efGetStruct(egg)->effect_vars.shield.player == 0);
            CHECK(root != NULL);
            if (root != NULL)
            {
                CHECK(root->xobjs[0]->kind == 0x50);
                CHECK(root->xobjs[1]->kind == nGCMatrixKindRecalcRotRpyRSca);
                CHECK(root->xobjs[2]->kind == nGCMatrixKindNull);
                CHECK_EQF(root->scale.vec.f.x, 1.5f);
                CHECK_EQF(root->scale.vec.f.y, 1.5f);
                CHECK_EQF(root->scale.vec.f.z, 1.0f);
                CHECK(root->user_data.p == fp.joints[nFTPartsJointYRotN]);
            }
            CHECK(fp.shield_health == 55);
            efManagerYoshiShieldProcDisplay(egg);
            CHECK(gEFManagerShieldLastEnv == 0x00000000);
            fp.shield_health = 0;
            efManagerYoshiShieldProcDisplay(egg);
            CHECK(gEFManagerShieldLastEnv == 0xAED6D600);
            fp.shield_health = 22;
            efManagerYoshiShieldProcDisplay(egg);
            CHECK(gEFManagerShieldLastEnv == ((u32)(u8)(0xAE * (1.0f - 22.0f / 55.0f)) << 24 |
                                             (u32)(u8)(0xD6 * (1.0f - 22.0f / 55.0f)) << 16 |
                                             (u32)(u8)(0xD6 * (1.0f - 22.0f / 55.0f)) << 8));
            fp.shield_health = 55;
            /* and it goes the way the guard's goes */
            ftParamProcStopEffect(mock_gobj);
            CHECK(!fp.is_effect_attach);
            CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
        }
    }

    /* efmanager.c:5426 efManagerYoshiEggEscapeMakeEffect:
     * the same fighter hatching back out of the egg above, but this
     * maker draws off dEFManagerYoshiEggEscapeEffectDesc, a second desc
     * over the shield's own model (o_dobjsetup and proc_display equal,
     * checked below) -- one DObj at scale 1.5, kinds {0x50, 0x4A, 0x2E},
     * hung on joints[5] (the decomp's own literal, not YRotN), plus
     * ftParamHideModelPartAll and is_effect_attach. */
    ef_pool_reset();
    {
        GObj *egg = efManagerYoshiEggEscapeMakeEffect(mock_gobj);

        CHECK(dEFManagerYoshiEggEscapeEffectDesc.flags == EFFECT_FLAG_USERDATA);
        CHECK(dEFManagerYoshiEggEscapeEffectDesc.proc_display ==
              efManagerYoshiShieldProcDisplay);
        CHECK(dEFManagerYoshiEggEscapeEffectDesc.o_dobjsetup ==
              dEFManagerYoshiShieldEffectDesc.o_dobjsetup);
        CHECK(egg != NULL);
        CHECK(fp.is_effect_attach);
        CHECK(fp.is_modelpart_modify);
        if (egg != NULL)
        {
            DObj *root = DObjGetStruct(egg);

            CHECK(efGetStruct(egg)->fighter_gobj == mock_gobj);
            CHECK(root != NULL);
            if (root != NULL)
            {
                CHECK(root->xobjs[0]->kind == 0x50);
                CHECK(root->xobjs[1]->kind == 0x4A);
                CHECK(root->xobjs[2]->kind == 0x2E);
                CHECK_EQF(root->scale.vec.f.x, 1.5f);
                CHECK_EQF(root->scale.vec.f.y, 1.5f);
                CHECK(root->user_data.p == fp.joints[5]);

                /* 0x4A writes no matrix: it turns rotate.z
                 * to the joint's rotate.x, signed by which way the joint's
                 * x axis leans in z, for the 0x2E after it to spin by */
                if (fp.joints[5] != NULL)
                {
                    f32 want;

                    fp.joints[5]->rotate.vec.f.x = 0.625f;
                    root->rotate.vec.f.z = 99.0f;
                    gcPrepDObjMatrix(NULL, root);
                    gcInitDLs();
                    func_ovl2_800EDBA4(fp.joints[5]);
                    want = (ftGetParts(fp.joints[5])->mtx_translate[0][2] > 0.0F) ? 0.625f : -0.625f;
                    CHECK_EQF(root->rotate.vec.f.z, want);
                }
            }
            ftParamProcStopEffect(mock_gobj);
            CHECK(!fp.is_effect_attach);
            CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
        }
    }

    /* efmanager.c:3998 efManagerFireSparkMakeEffect, the
     * second of the two real DIVERGES arms. Flags carry 0x4 and not
     * 0x1 -- the dead explosion's own branch -- so efManagerMakeEffect-
     * NoForce already walks dEFManagerFireSparkEffectDesc's real
     * DObjDesc tree onto the effect's own DObj before this maker runs:
     * root gets transform_types1 {0x50, 0x49, 0x12} and its one child
     * transform_types2 {0x45, Null, 0}. The decomp's own second walk,
     * lbCommonSetDObjTransformsForTreeDObjs over dobj->child, is
     * dropped -- there is no relocData to re-read it from and the pack
     * already carries the child's baked (0, -90, 0). */
    ef_pool_reset();
    {
        GObj *spark = efManagerFireSparkMakeEffect(mock_gobj);

        CHECK(dEFManagerFireSparkEffectDesc.flags ==
              (0x4 | EFFECT_FLAG_USERDATA));
        CHECK(dEFManagerFireSparkEffectDesc.proc_display ==
              lbCommonDObjScaleXProcDisplay);
        CHECK(spark != NULL);
        CHECK(fp.is_effect_attach);

        if (spark != NULL)
        {
            DObj *root = DObjGetStruct(spark);
            EFStruct *ep = efGetStruct(spark);

            CHECK(spark->dl_link_id == 15);
            CHECK(efGetStruct(spark)->fighter_gobj == mock_gobj);
            CHECK(root != NULL);
            CHECK(ep != NULL);

            if (root != NULL)
            {
                CHECK(root->xobjs[0]->kind == 0x50);
                CHECK(root->xobjs[1]->kind == 0x49);
                CHECK(root->xobjs[2]->kind == 0x12);
                CHECK_EQF(root->translate.vec.f.y, 160.0F);
                CHECK(root->user_data.p == fp.joints[16]);
                CHECK(root->child != NULL);

                /* 0x49: its local y is the attached
                 * joint's own x axis, normalised, and the other two turn
                 * to the camera about it -- an orthonormal basis with no
                 * translation. The mock fighter has no joint 16, so the
                 * check hangs the spark off the first joint it does have. */
                {
                    static GObj cam_gobj;
                    static CObj cam_cobj;
                    DObj *joint = NULL;
                    void *saved = root->user_data.p;
                    float m[16];
                    const Mtx44f *jm;
                    f32 len = 0.0f;
                    s32 ji;

                    for (ji = nFTPartsJointTopN; (ji < FTPARTS_JOINT_NUM_MAX) && (joint == NULL); ji++)
                    {
                        joint = fp.joints[ji];
                    }
                    CHECK(joint != NULL);
                    if (joint != NULL)
                    {
                        cam_gobj.obj = &cam_cobj;
                        cam_cobj.vec.eye.x = 300.0f;
                        cam_cobj.vec.eye.y = 800.0f;
                        cam_cobj.vec.eye.z = 5000.0f;
                        cam_cobj.vec.at.x = cam_cobj.vec.at.y = cam_cobj.vec.at.z = 0.0f;
                        root->user_data.p = joint;
                        gGCCurrentCamera = &cam_gobj;
                        CHECK(gcDObjLocalMatrixF(m, root, 0x49) == TRUE);
                        gGCCurrentCamera = NULL;
                        root->user_data.p = saved;

                        jm = &ftGetParts(joint)->mtx_translate;
                        len = sqrtf(SQUARE((*jm)[0][0]) + SQUARE((*jm)[0][1]) + SQUARE((*jm)[0][2]));
                        CHECK(len > 0.0f);
                        if (len > 0.0f)
                        {
                            CHECK_NEAR(m[1], (*jm)[0][0] / len, 1e-4f);
                            CHECK_NEAR(m[5], (*jm)[0][1] / len, 1e-4f);
                            CHECK_NEAR(m[9], (*jm)[0][2] / len, 1e-4f);
                        }
                        CHECK_NEAR(m[0] * m[1] + m[4] * m[5] + m[8] * m[9], 0.0f, 1e-4f);
                        CHECK_NEAR(m[2] * m[1] + m[6] * m[5] + m[10] * m[9], 0.0f, 1e-4f);
                        CHECK_NEAR(m[0] * m[0] + m[4] * m[4] + m[8] * m[8], 1.0f, 1e-3f);
                        CHECK(m[3] == 0.0f && m[7] == 0.0f && m[11] == 0.0f && m[15] == 1.0f);
                    }
                }

                if (root->child != NULL)
                {
                    CHECK(root->child->xobjs[0]->kind == 0x45);
                    CHECK(root->child->xobjs[1]->kind == nGCMatrixKindNull);
                    CHECK(root->child->xobjs[2]->kind == 0x00);
                }
            }
            ftParamProcStopEffect(mock_gobj);
            CHECK(!fp.is_effect_attach);
            CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
        }
    }
}

static int is_escape(void)
{
    return (fp.status_id == nFTCommonStatusEscapeF) ||
           (fp.status_id == nFTCommonStatusEscapeB);
}

/* ft/ftcommon/ftcommonescape.c: the roll. The stick tapped
 * past 56 while the shield is up is EscapeF or EscapeB by its sign
 * against the facing; the roll drops the shield and its bubble, ignores
 * jostle, carries the item-throw buffer the other two ways in do not,
 * flips the fighter on the animation's flag1 event, and ends in Wait
 * with both velocity vectors zeroed -- or, for Yoshi with Z still held,
 * back in his shield. */
static void test_roll(void)
{
    int i;

    ef_pool_reset();
    spawn(0.0f, 0.0f);
    mock_anim_len = 3.0f;
    frame(0, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuardOn);
    for (i = 0; i < 8 && !is_guard(); i++)
        frame(0, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuard);
    CHECK(fp.is_shield);
    CHECK(fp.is_effect_attach);
    CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM - 1);

    /* facing right, the stick tapped right: forward. The shield goes --
     * ftCommonEscapeSetStatus sets the status with PRESERVE_NONE, which
     * is what takes the bubble down and ejects the shield's joints */
    mock_anim_len = 20.0f;
    frame(80, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusEscapeF);
    CHECK(fp.motion_id == nFTCommonMotionEscapeF);
    CHECK(fp.status_vars.common.escape.itemthrow_buffer_tics == 5);
    CHECK(fp.is_jostle_ignore);
    CHECK(!fp.is_shield);
    CHECK(!fp.is_effect_attach);
    CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
    CHECK(fp.joints[nFTPartsJointYRotN] == NULL);
    CHECK(fp.joints[nFTPartsJointXRotN] == NULL);
    /* ftCommonEscapeProcStatus ran on the way in and zeroed the flag the
     * animation's about-face event writes */
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK(fp.proc_status == NULL);
    CHECK(fp.lr == 1);

    /* the about-face: the script's event (src/dc/ftmain.c:505) writes
     * flag1, and the roll's next update turns it into a flipped facing */
    fp.motion_vars.flags.flag1 = 1;
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusEscapeF);
    CHECK(fp.lr == -1);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    fp.lr = 1;

    /* nothing interrupts it: the roll's ProcInterrupt is the item throw
     * and no item is held. Z, the stick, A: it rolls on */
    frame(80, 0, N64_A | N64_Z, N64_A | N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusEscapeF);

    /* the animation ends: both velocity vectors to zero, then Wait */
    fp.physics.vel_air.y = 9.0f;
    fp.physics.vel_ground.x = 12.0f;
    i = run_until(0, 0, 30, is_wait);
    CHECK(i < 30);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK_EQF(fp.physics.vel_air.y, 0.0f);
    CHECK_EQF(fp.physics.vel_ground.x, 0.0f);

    /* the stick against the facing: backward */
    spawn(0.0f, 0.0f);
    mock_anim_len = 3.0f;
    frame(0, 0, 0, N64_Z, 0);
    for (i = 0; i < 8 && !is_guard(); i++)
        frame(0, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuard);
    mock_anim_len = 20.0f;
    frame(-80, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusEscapeB);
    CHECK(fp.motion_id == nFTCommonMotionEscapeB);
    CHECK(fp.status_vars.common.escape.itemthrow_buffer_tics == 5);

    /* a shield held with the stick already over does not roll: the tap
     * has to be inside four tics (ftCommonEscapeGetStatus reads
     * tap_stick_x, which the input pass resets to 1 the frame the stick
     * crosses 20 and counts up from there) */
    spawn(0.0f, 0.0f);
    mock_anim_len = 3.0f;
    for (i = 0; i < 5; i++)
        frame(30, 0, 0, 0, 0);      /* under the dash's threshold too */
    CHECK(fp.tap_stick_x >= FTCOMMON_ESCAPE_BUFFER_TICS_MAX);
    frame(80, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuardOn);
    for (i = 0; i < 8 && !is_guard(); i++)
        frame(80, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuard);
    CHECK(fp.is_shield);

    /* ftCommonEscapeGetStatus itself, over the two numbers it reads:
     * 56 of stick and a tap inside 4 tics, forward when the stick and
     * the facing agree */
    fp.input.pl.stick_range.x = 55;
    fp.tap_stick_x = 1;
    fp.lr = 1;
    CHECK(ftCommonEscapeGetStatus(&fp) == -1);
    fp.input.pl.stick_range.x = 56;
    CHECK(ftCommonEscapeGetStatus(&fp) == nFTCommonStatusEscapeF);
    fp.input.pl.stick_range.x = -56;
    CHECK(ftCommonEscapeGetStatus(&fp) == nFTCommonStatusEscapeB);
    fp.lr = -1;
    CHECK(ftCommonEscapeGetStatus(&fp) == nFTCommonStatusEscapeF);
    fp.input.pl.stick_range.x = 56;
    CHECK(ftCommonEscapeGetStatus(&fp) == nFTCommonStatusEscapeB);
    fp.tap_stick_x = FTCOMMON_ESCAPE_BUFFER_TICS_MAX;
    CHECK(ftCommonEscapeGetStatus(&fp) == -1);
    fp.lr = 1;
    fp.tap_stick_x = 1;
    fp.input.pl.stick_range.x = 0;
    CHECK(ftCommonEscapeGetStatus(&fp) == -1);

    /* the dash's way in (no ported caller yet): Z alone, forward
     * whatever the stick says, and no item-throw buffer */
    spawn(0.0f, 0.0f);
    mock_anim_len = 20.0f;
    fp.input.pl.button_tap = fp.input.button_mask_z;
    fp.input.pl.stick_range.x = -80;
    CHECK(ftCommonEscapeCheckInterruptDash(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusEscapeF);
    CHECK(fp.status_vars.common.escape.itemthrow_buffer_tics == 0);
    fp.input.pl.button_tap = 0;
    /* and the charged neutral-B's, which reads the stick like the
     * shield's does */
    spawn(0.0f, 0.0f);
    mock_anim_len = 20.0f;
    fp.input.pl.stick_range.x = 80;
    fp.tap_stick_x = 1;
    CHECK(ftCommonEscapeCheckInterruptSpecialNDonkey(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusEscapeF);
    CHECK(fp.status_vars.common.escape.itemthrow_buffer_tics == 0);

    /* Yoshi's roll goes back into his shield if Z is still held and the
     * shield is not spent (ftCommonGuardCheckInterruptEscape), because
     * his shield is the egg -- a status of its own, not a pose. The
     * stand-in wears his kind for it; the egg is the effect the shield
     * maker makes for Yoshi. */
    ef_pool_reset();
    spawn(0.0f, 0.0f);
    mock_anim_len = 3.0f;
    fp.fkind = nFTKindYoshi;
    frame(0, 0, 0, N64_Z, 0);
    for (i = 0; i < 8 && !is_guard(); i++)
        frame(0, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuard);
    mock_anim_len = 6.0f;
    frame(80, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusEscapeF);
    CHECK(!fp.is_shield);
    for (i = 0; i < 12 && is_escape(); i++)
        frame(0, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuard);
    CHECK(fp.is_shield);
    CHECK(fp.is_effect_attach);
    CHECK(fp.status_vars.common.guard.release_lag == FTCOMMON_GUARD_RELEASE_LAG);
    CHECK(fp.status_vars.common.guard.effect_gobj != NULL);
    /* and it is the egg, not the bubble: the egg's root carries matrix
     * kind 0x50 where the bubble's carries 0x4F */
    if (fp.status_vars.common.guard.effect_gobj != NULL)
    {
        CHECK(DObjGetStruct(fp.status_vars.common.guard.effect_gobj)->xobjs[0]->kind == 0x50);
    }
    /* with Z let go it is Wait, egg or no egg */
    fp.fkind = nFTKindYoshi;
    mock_anim_len = 6.0f;
    frame(-80, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusEscapeB);
    i = run_until(0, 0, 20, is_wait);
    CHECK(i < 20);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(!fp.is_shield);
    CHECK(!fp.is_effect_attach);
    CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
    mock_anim_len = 1e9f;
}

static int is_run(void) { return fp.status_id == nFTCommonStatusRun; }
static int is_dash(void) { return fp.status_id == nFTCommonStatusDash; }

/* ft/ftcommon/ftcommondash.c, ftcommonrun.c, ftcommonrunbrake.c and
 * ftcommonturnrun.c: the dash, the run it becomes, the
 * brake that ends it and the turn-run that reverses it, on Mario's own
 * attributes -- dash_speed 54, dash_decel 2.8, run_speed 44,
 * dash_to_run 14, traction 1.5. */
static void test_dash_run(void)
{
    int i;

    /* a stick tapped past 56 out of Wait is a dash, and it spends the
     * tap so the next frame cannot dash again off it */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusDash);
    CHECK(fp.motion_id == nFTCommonMotionDash);
    CHECK_EQF(fp.physics.vel_ground.x, kMario.dash_speed);
    CHECK(fp.tap_stick_x == FTINPUT_STICKBUFFER_TICS_MAX);
    CHECK(fp.motion_vars.flags.flag1 == 1);
    CHECK(fp.lr == 1);

    /* the dash runs flat for seven frames and then brakes at dash_decel
     * (ftCommonDashProcPhysics) */
    while (mock_gobj->anim_frame < FTCOMMON_DASH_DECELERATE_BEGIN)
    {
        CHECK_EQF(fp.physics.vel_ground.x, kMario.dash_speed);
        frame(80, 0, 0, 0, 0);
    }
    CHECK_NEAR(fp.physics.vel_ground.x, kMario.dash_speed - kMario.dash_decel, 1e-3f);
    frame(80, 0, 0, 0, 0);
    CHECK_NEAR(fp.physics.vel_ground.x, kMario.dash_speed - 2.0f * kMario.dash_decel, 1e-3f);

    /* held forward, the dash becomes a run on the frame it reaches
     * dash_to_run, and the run is run_speed flat */
    {
        float last = 0.0f;

        i = 0;
        while (!is_run() && i++ < 30)
        {
            last = mock_gobj->anim_frame;
            frame(80, 0, 0, 0, 0);
        }
        CHECK_STATUS(nFTCommonStatusRun);
        CHECK(fp.motion_id == nFTCommonMotionRun);
        /* the frame the dash was on when it became a run: the last one
         * under dash_to_run, since the check's window is one frame of
         * animation speed wide */
        CHECK_NEAR(last, kMario.dash_to_run - 1.0f, 0.01f);
    }
    CHECK_EQF(fp.physics.vel_ground.x, kMario.run_speed);
    frame(80, 0, 0, 0, 0);
    CHECK_EQF(fp.physics.vel_ground.x, kMario.run_speed);

    /* the stick let go: RunBrake, whose flag says it may still be
     * turned out of, braking at traction * 1.25 */
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusRunBrake);
    CHECK(fp.motion_vars.flags.flag1 == 1);
    CHECK_NEAR(fp.physics.vel_ground.x, kMario.run_speed - kMario.traction * 1.25f, 1e-3f);
    /* and it ends in Wait */
    mock_anim_len = 3.0f;
    i = run_until(0, 0, 12, is_wait);
    CHECK(i < 12);
    CHECK_STATUS(nFTCommonStatusWait);
    mock_anim_len = 1e9f;

    /* the stick pushed back out of a run is TurnRun: the flip is the
     * animation's flag1 event, as the roll's is, and it takes the run's
     * velocity with it */
    spawn(0.0f, 0.0f);
    frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusDash);
    i = 0;
    while (!is_run() && i++ < 30)
        frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusRun);
    frame(-80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusTurnRun);
    CHECK(fp.motion_id == nFTCommonMotionTurnRun);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK(fp.motion_vars.flags.flag2 == 0);
    CHECK(fp.lr == 1);
    fp.motion_vars.flags.flag1 = 1;
    frame(-80, 0, 0, 0, 0);
    CHECK(fp.lr == -1);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    /* when its animation ends it is a run again, the other way */
    mock_anim_len = 3.0f;
    i = 0;
    while (!is_run() && i++ < 12)
        frame(-80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusRun);
    CHECK(fp.lr == -1);
    CHECK_EQF(fp.physics.vel_ground.x, kMario.run_speed);
    mock_anim_len = 1e9f;

    /* the brake can be turned out of while its flag is up and it is
     * inside four frames (ftCommonRunBrakeProcInterrupt) */
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusRunBrake);
    frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusTurnRun);

    /* and a turn-run can be braked out of once its second flag is up,
     * which is where the USA ROM clamps the slide it used to keep
     * (ftcommonrunbrake.c:119-125) */
    fp.motion_vars.flags.flag2 = 1;
    fp.physics.vel_ground.x = 100.0f;
    fp.input.pl.stick_range.x = 0;
    CHECK(ftCommonRunBrakeCheckInterruptTurnRun(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusRunBrake);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK_EQF(fp.physics.vel_ground.x, kMario.run_speed);

    /* a run jumps off a lower stick than a stand does: 44 against 53
     * (ftCommonKneeBendGetInputTypeRun) */
    spawn(0.0f, 0.0f);
    frame(0, 50, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);          /* 50 < 53, no jump */
    frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusDash);
    i = 0;
    while (!is_run() && i++ < 30)
        frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusRun);
    frame(80, 50, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusKneeBend);
    CHECK(fp.status_vars.common.kneebend.input_source == FTCOMMON_KNEEBEND_INPUT_TYPE_STICK);

    /* the roll out of a dash: the way in ftcommonescape.c, inside the
     * dash's first three frames */
    spawn(0.0f, 0.0f);
    mock_anim_len = 20.0f;
    frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusDash);
    frame(80, 0, N64_Z, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusEscapeF);
    CHECK(fp.status_vars.common.escape.itemthrow_buffer_tics == 0);

    /* the shield out of a dash slides for what is left of the twenty
     * frames the second window covers */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusDash);
    for (i = 0; i < 5; i++)
        frame(80, 0, 0, 0, 0);
    CHECK(is_dash());
    {
        float at = mock_gobj->anim_frame;

        /* the proc itself, so the frame the slide is measured from is
         * the one read here rather than the next animation step's */
        fp.input.pl.button_hold |= fp.input.button_mask_z;
        ftCommonDashProcInterrupt(mock_gobj);
        CHECK_STATUS(nFTCommonStatusGuardOn);
        CHECK(at > 0.0f && at <= 20.0f);
        CHECK(fp.status_vars.common.guard.slide_tics == (s32)(20.0f - at));
    }

    /* the dash out of a turn (ftcommonturn.c:81-92, the block a walk-out
     * must not stand in for): the stick held
     * against the facing turns, and the frame the turn flips, the same
     * stick dashes the new way */
    spawn(0.0f, 0.0f);
    frame(-80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusTurn);
    CHECK(fp.status_vars.common.turn.lr_dash == -1);    /* SetStatusInvertLR */
    CHECK(fp.status_vars.common.turn.lr_turn == -1);
    i = 0;
    while (!is_dash() && i++ < 12)
        frame(-80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusDash);
    CHECK(fp.lr == -1);
    CHECK(fp.motion_vars.flags.flag1 == 0);
    CHECK_EQF(fp.physics.vel_ground.x, kMario.dash_speed);

    /* the dash is not reachable off a stick that was already over: the
     * tap has to be inside three tics (FTCOMMON_DASH_BUFFER_TICS_MAX) */
    spawn(0.0f, 0.0f);
    for (i = 0; i < 5; i++)
        frame(30, 0, 0, 0, 0);
    CHECK(fp.status_id == nFTCommonStatusWalkSlow ||
          fp.status_id == nFTCommonStatusWalkMiddle);
    CHECK(fp.tap_stick_x >= FTCOMMON_DASH_BUFFER_TICS_MAX);
    frame(80, 0, 0, 0, 0);
    CHECK(fp.status_id == nFTCommonStatusWalkFast);
    mock_anim_len = 1e9f;
}

/* ft/ftcommon/ftcommonappeal.c: the L-tap taunt, status
 * 189. It sits between GuardOn and KneeBend in every ground cascade, so
 * an L-tap out of Wait, a walk or a squat is the taunt; it ends in Wait
 * when the animation runs out (ftAnimEndSetWait); and on the animation's
 * flag1 event ftCommonAppealProcInterrupt cancels it -- into the grab
 * if Z and A are held, into a shield if only Z is. The input read edge-detects a tap from
 * the button-hold transition (src/dc/ftcommon.c:7397), so L is fed as a
 * fresh hold. */
static void test_appeal(void)
{
    int i;

    /* from Wait: L tapped is the taunt, and the flag the animation's
     * event writes is cleared going in */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, 0, 0, N64_L, 0);
    CHECK_STATUS(nFTCommonStatusAppeal);
    CHECK(fp.motion_id == nFTCommonMotionAppeal);
    CHECK(fp.motion_vars.flags.flag1 == 0);

    /* it ends in Wait when the animation runs out */
    mock_anim_len = 4.0f;
    i = run_until(0, 0, 20, is_wait);
    CHECK(i < 20);
    CHECK_STATUS(nFTCommonStatusWait);

    /* the flag1 cancel: the taunt up, the event fired, Z held -> a shield */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(0, 0, 0, 0, 0);
    frame(0, 0, 0, N64_L, 0);
    CHECK_STATUS(nFTCommonStatusAppeal);
    fp.motion_vars.flags.flag1 = 1;
    frame(0, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuardOn);

    /* the same event with nothing held does not cancel: the taunt runs on */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(0, 0, 0, 0, 0);
    frame(0, 0, 0, N64_L, 0);
    CHECK_STATUS(nFTCommonStatusAppeal);
    fp.motion_vars.flags.flag1 = 1;
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusAppeal);

    /* the cascade reach: L out of a walk and out of a squat is the taunt */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(40, 0, 0, 0, 0);
    CHECK(fp.status_id == nFTCommonStatusWalkSlow ||
          fp.status_id == nFTCommonStatusWalkMiddle);
    frame(40, 0, 0, N64_L, 0);
    CHECK_STATUS(nFTCommonStatusAppeal);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(0, -80, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusSquat);
    frame(0, -80, 0, N64_L, 0);
    CHECK_STATUS(nFTCommonStatusAppeal);
    mock_anim_len = 1e9f;
}
static int is_furafura(void) { return fp.status_id == nFTCommonStatusFuraFura; }
static int is_shieldbreakdown(void)
{
    return fp.status_id == nFTCommonStatusShieldBreakDownU ||
           fp.status_id == nFTCommonStatusShieldBreakDownD;
}

/* the shield break: guarding until the shield decays to
 * nothing launches the five-status chain Fly -> Fall -> Down -> Stand ->
 * FuraFura and back to Wait (ftcommonshieldbreak{fly,fall,down,stand}.c,
 * ftcommonfurafura.c), plus its two shared helpers. */
static void test_shield_break(void)
{
    int i, idle_used, mash_used;

    /* --- ShieldBreakFly's own properties, from the entry the guard calls
     * at shield_health == 0 (ftCommonShieldBreakFlyCommonSetStatus). Check
     * before any physics tic so the launch velocity is the attribute's. --- */
    spawn(0.0f, 300.0f);
    mock_anim_len = 1e9f;
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);
    fp.attr->shield_break_vel_y = 3.0f;
    ftCommonShieldBreakFlyCommonSetStatus(mock_gobj);
    CHECK_STATUS(nFTCommonStatusShieldBreakFly);
    CHECK(fp.ga == nMPKineticsAir);
    CHECK_EQF(fp.physics.vel_air.x, 0.0f);
    CHECK_EQF(fp.physics.vel_air.y, 3.0f);

    /* --- the 1P game's Shield Breaker (ftcommonshieldbreakfly.c:58-64):
     * set only when the 1P player's hit broke someone else's shield, not
     * when it ran down on its own (shield_damage 0) --- */
    {
        s32 saved_type = mock_battle.game_type;
        u8 saved_player = gSCManagerSceneData.player;

        mock_battle.game_type = nSCBattleGameType1PGame;
        gSCManagerSceneData.player = (fp.player + 1) % 4;
        fp.shield_player = gSCManagerSceneData.player;
        gSC1PGameBonusShieldBreaker = FALSE;
        fp.shield_damage = 0;
        ftCommonShieldBreakFlyCommonSetStatus(mock_gobj);
        CHECK(gSC1PGameBonusShieldBreaker == FALSE);
        fp.shield_damage = 10;
        ftCommonShieldBreakFlyCommonSetStatus(mock_gobj);
        CHECK(gSC1PGameBonusShieldBreaker == TRUE);
        mock_battle.game_type = saved_type;
        gSCManagerSceneData.player = saved_player;
        gSC1PGameBonusShieldBreaker = FALSE;
    }

    /* --- the whole chain, driven the way a match drives it: the shield up,
     * its health walked down to the last point, and one more decay tick
     * breaks it inside ftCommonGuardOnProcUpdate. On the platform, so the
     * pop-up has ground to come back down to. --- */
    spawn(0.0f, 300.0f);
    mock_anim_len = 1e9f;
    /* under the roll and dash thresholds first (as test_guard does) */
    frame(30, 0, 0, 0, 0);
    frame(30, 0, 0, 0, 0);
    frame(30, 0, 0, 0, 0);
    frame(30, 0, 0, 0, 0);
    frame(0, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuardOn);
    fp.attr->shield_break_vel_y = 3.0f;
    fp.shield_health = 1;
    fp.status_vars.common.guard.shield_decay_wait = 1;
    frame(0, 0, 0, N64_Z, 0);   /* decay 1 -> 0, break same frame */
    CHECK_STATUS(nFTCommonStatusShieldBreakFly);
    CHECK(fp.ga == nMPKineticsAir);
    /* shield_health is not asserted here: ftMainUpdateShieldStatFighter
     * snaps any <= 0 value back to 30 the same frame (ftmain.c), so the
     * break never leaves it at zero -- the dizzy's restore is checked at
     * the direct FuraFuraSetStatus below. */

    /* it pops up, comes back down, and lands into a ground down status */
    i = run_until(0, 0, 120, is_shieldbreakdown);
    CHECK(i < 120);
    CHECK(is_shieldbreakdown());
    CHECK(fp.ga == nMPKineticsGround);

    /* Down (anim end) -> Stand (anim end) -> FuraFura, the dizzy */
    mock_anim_len = 4.0f;
    i = run_until(0, 0, 60, is_furafura);
    CHECK(i < 60);
    CHECK_STATUS(nFTCommonStatusFuraFura);
    CHECK(fp.shield_health >= 30);      /* the dizzy has restored it */
    CHECK(fp.breakout_wait > 0);

    /* the counter reaches zero -> Wait */
    fp.breakout_wait = 1;
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    /* --- ftCommonDownBounceCheckUpOrDown: the hip's x rotation says back
     * (down, 1) or face (up, 0). pi/2 lands in the down half-turn, 3pi/2
     * in the up. --- */
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    fp.joints[4]->rotate.vec.f.x = 1.57079633f;   /* pi/2, on its back */
    CHECK(ftCommonDownBounceCheckUpOrDown(mock_gobj) == 1);
    fp.joints[4]->rotate.vec.f.x = 4.71238898f;   /* 3pi/2, face up */
    CHECK(ftCommonDownBounceCheckUpOrDown(mock_gobj) == 0);

    /* --- FuraFura's breakout: mashing A leaves the dizzy sooner than
     * waiting it out. Two fresh dizzies from the same damage, one idled to
     * Wait, one mashed (a fresh A edge every other frame) until it leaves
     * FuraFura -- the exit target Wait is checked above; here only that
     * mashing is faster, since a held A jabs the frame Wait begins. --- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(0, 0, 0, 0, 0);
    fp.percent_damage = 380;            /* a short window: 400-380+90 = 110 */
    ftCommonFuraFuraSetStatus(mock_gobj);
    CHECK_STATUS(nFTCommonStatusFuraFura);
    CHECK(fp.shield_health == 30);      /* the dizzy sets it to 30 */
    CHECK_NEAR((float)fp.breakout_wait, 110.0f, 0.5f);
    idle_used = run_until(0, 0, 400, is_wait);
    CHECK(idle_used < 400);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(0, 0, 0, 0, 0);
    fp.percent_damage = 380;
    ftCommonFuraFuraSetStatus(mock_gobj);
    CHECK_STATUS(nFTCommonStatusFuraFura);
    for (i = 0; i < 400 && is_furafura(); i++)
        frame(0, 0, 0, (i & 1) ? N64_A : 0, 0);
    mash_used = i;
    CHECK(mash_used < idle_used);

    mock_anim_len = 1e9f;
}

static int is_catch(void) { return fp.status_id == nFTCommonStatusCatch; }

/* fp.attr->thrown_status (the per-caught-kind thrown-status table: Mario's
 * forward throw is the one-shot ThrownCommon, his back throw the two-stage
 * MarioBStart -> MarioB) is real production data now, not mocked here --
 * kMario's own .thrown_status flows through the real
 * ftManagerSetupAttributes into kMarioAttr at suite startup, the same path
 * a real fighter gets. Patching a private array in by hand right where the
 * throw needs it would hide a gap in that wiring -- exactly what let such a
 * gap stand undetected until a real disc probe
 * completed an actual grab and crashed on it.
 *
 * The throw hit descriptor is a separate thing and genuinely is mocked:
 * fp.throw_desc comes from a figatree motion event (ftMotionEventSetThrow2,
 * ftmain.c) the mock motion table doesn't carry, so the test still supplies
 * one by hand (a Damage status, 8%, sent at 45deg). */
static FTThrowHitDesc mock_throw_desc = { nFTCommonStatusDamageN2, 8, 45, 80, 60, 30, nGMHitElementNormal };

/* the hold the grab-break checks below start from: fp holds fp2 (CatchWait
 * and CaptureWait, cross-linked), both on the ground and undamaged, with
 * mock_throw_descs as the catcher's throw hits. */
static FTThrowHitDesc mock_throw_descs[2] =
{
    { nFTCommonStatusDamageN2, 8, 45, 80, 60, 30, nGMHitElementNormal },
    { nFTCommonStatusDamageN2, 5, 45, 80, 60, 30, nGMHitElementNormal },
};

static void grab_hold(void)
{
    int i;

    spawn(0.0f, 0.0f);
    spawn_second(30.0f, 0.0f, +1);
    fp.attr->joint_itemheavy_id = nFTPartsJointTopN;
    fp.throw_desc = mock_throw_descs;
    mock_anim_len = 4.0f;
    frame(0, 0, N64_A, N64_Z | N64_A, 0);
    fp.search_gobj = mock_gobj2;
    ftCommonCatchPullProcCatch(mock_gobj);
    ftCommonCapturePulledProcCapture(mock_gobj2, mock_gobj);
    for (i = 0; i < 30 && fp.status_id == nFTCommonStatusCatchPull; i++)
        frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusCatchWait);
    CHECK(fp2.status_id == nFTCommonStatusCaptureWait);
    CHECK(fp.catch_gobj == mock_gobj2);
    CHECK(fp2.capture_gobj == mock_gobj);
    fp.percent_damage = fp2.percent_damage = 0;
}

/* the jostle (ft/ftmain.c:1512-1569): two grounded fighters
 * on one floor line closer than their jostle widths are pushed apart,
 * 6.75 a frame in x and 3 in z, and a fighter out of range drifts back to
 * z 0. A fighter being held is not pushed against. The mock's
 * jostle_width is 112.5 and its jostle_x 0. */
static void test_jostle(void)
{
    int i;
    f32 gap;

    /* overlapping: each is pushed away from the other, in x and in z */
    spawn(0.0f, 0.0f);
    spawn_second(30.0f, 0.0f, -1);
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(fp.ga == nMPKineticsGround && fp2.ga == nMPKineticsGround);
    CHECK(fp.physics.vel_jostle_x == -6.75f);
    CHECK(fp2.physics.vel_jostle_x == 6.75f);
    CHECK(fp.physics.vel_jostle_z == 3.0f);
    CHECK(fp2.physics.vel_jostle_z == -3.0f);
    for (i = 0; i < 20; i++)
        frame(0, 0, 0, 0, 0);
    gap = DObjGetStruct(mock_gobj2)->translate.vec.f.x - DObjGetStruct(mock_gobj)->translate.vec.f.x;
    CHECK(gap > 30.0f + 100.0f);
    CHECK(DObjGetStruct(mock_gobj)->translate.vec.f.z > 0.0f);
    CHECK(DObjGetStruct(mock_gobj2)->translate.vec.f.z < 0.0f);

    /* apart past the widths: no push, and z is pulled back to 0 */
    for (i = 0; i < 60; i++)
        frame(0, 0, 0, 0, 0);
    CHECK(fp.physics.vel_jostle_x == 0.0f);
    CHECK(DObjGetStruct(mock_gobj)->translate.vec.f.z == 0.0f);
    CHECK(DObjGetStruct(mock_gobj2)->translate.vec.f.z == 0.0f);

    /* on the same spot: pushed opposite ways, not both the same way */
    spawn(0.0f, 0.0f);
    spawn_second(0.0f, 0.0f, +1);
    frame(0, 0, 0, 0, 0);
    CHECK(fp.physics.vel_jostle_x != 0.0f);
    CHECK(fp.physics.vel_jostle_x == -fp2.physics.vel_jostle_x);
    CHECK(fp.physics.vel_jostle_z == -fp2.physics.vel_jostle_z);

    /* jostle ignored: no push given, though the other is still pushed */
    spawn(0.0f, 0.0f);
    spawn_second(30.0f, 0.0f, -1);
    fp.is_jostle_ignore = TRUE;
    frame(0, 0, 0, 0, 0);
    CHECK(fp.physics.vel_jostle_x == 0.0f);
    CHECK(fp2.physics.vel_jostle_x == 6.75f);

    /* holding a fighter: the catcher is not pushed off what it holds */
    grab_hold();
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusCatchWait);
    CHECK(fp.physics.vel_jostle_x == 0.0f);
    CHECK(fp.physics.vel_jostle_z == 0.0f);
}

/* a hit queued the way ftMainProcParams queues one */
static void grab_hit(FTStruct *hit_fp, s32 damage, f32 knockback)
{
    hit_fp->damage_queue = damage;
    hit_fp->damage_knockback = knockback;
    hit_fp->damage_angle = 45;
    hit_fp->damage_lr = -1;
    hit_fp->damage_index = 1;
    hit_fp->damage_element = nGMHitElementNormal;
    hit_fp->damage_player_num = 2;
}

static int grab_let_go(FTStruct *f)
{
    return (f->status_id == nFTCommonStatusWait) || (f->status_id == nFTCommonStatusFall);
}

/* the grab: Z held with A tapped reaches out on the Catch
 * status (ftcommoncatch1.c). Step 11 armed the connect -- the catch search
 * (ftMainProcSearchCatch, run in frame()) fires proc_catch/proc_capture on
 * a find -- so the catcher pulls the caught fighter into CatchWait and holds
 * it in CaptureWait. With nobody in range the reach still whiffs to Wait.
 * Step 12 is the throw that ends the hold. Step 13 adds the dash and run
 * grabs (the same reach out of a dash or a run, via CheckInterruptDashRun). */
/* test_grab's physics census: counts each fighter's proc_physics calls,
 * then runs the status's own */
static void (*sGrabPhysics[2])(GObj *);
static int sGrabPhysicsRuns[2];

static void grab_count_physics(GObj *fighter_gobj)
{
    int k = (fighter_gobj == mock_gobj) ? 0 : 1;

    sGrabPhysicsRuns[k]++;
    sGrabPhysics[k](fighter_gobj);
}

static void test_grab(void)
{
    int i;

    /* --- the standing grab, no victim: Z held, A tapped, out of Wait --- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 6.0f;
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);
    frame(0, 0, N64_A, N64_Z | N64_A, 0);
    CHECK_STATUS(nFTCommonStatusCatch);
    /* the connect is armed: is_catchstatus is set, but no one is in
     * range, so the search finds nobody and the reach whiffs to Wait */
    CHECK(fp.is_catchstatus == TRUE);
    CHECK(fp.catch_gobj == NULL);
    CHECK(run_until(0, 0, 30, is_wait) < 30);
    CHECK_STATUS(nFTCommonStatusWait);

    /* --- priority: A alone from Wait is the jab (the grab needs Z held);
     * Z+A is the grab, because the catch check sits ahead of the attacks
     * in the ground cascade --- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 6.0f;
    frame(0, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttack11);
    run_until(0, 0, 20, is_wait);
    frame(0, 0, N64_A, N64_Z | N64_A, 0);
    CHECK_STATUS(nFTCommonStatusCatch);
    run_until(0, 0, 30, is_wait);

    /* --- the shield-grab: A tapped while the shield is up --- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 6.0f;
    frame(0, 0, 0, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusGuardOn);
    frame(0, 0, N64_A, N64_Z | N64_A, 0);
    CHECK_STATUS(nFTCommonStatusCatch);
    run_until(0, 0, 30, is_wait);

    /* --- the jab-cancel grab: Z within the jab's first two frames --- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 6.0f;
    frame(0, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttack11);
    frame(0, 0, N64_Z, N64_Z, 0);
    CHECK_STATUS(nFTCommonStatusCatch);
    run_until(0, 0, 30, is_wait);

    /* --- the connect: a fighter in the grab box is caught and
     * held. The mock's Catch motion raises no catch box, so drive the two
     * procs the search fires (ftmain.c ftMainProcSearchCatch: proc_catch on
     * the catcher, proc_capture on the caught) directly, then let frame()
     * run the pull out into the hold. --- */
    spawn(0.0f, 0.0f);
    spawn_second(30.0f, 0.0f, +1);
    /* the caught body is placed at the catcher's item-heavy joint; point it
     * at a joint the 3-joint mock skeleton actually has */
    fp.attr->joint_itemheavy_id = nFTPartsJointTopN;
    mock_anim_len = 4.0f;
    frame(0, 0, N64_A, N64_Z | N64_A, 0);
    CHECK_STATUS(nFTCommonStatusCatch);
    CHECK(fp.is_catchstatus == TRUE);
    CHECK(fp.proc_catch == ftCommonCatchPullProcCatch);
    CHECK(fp.proc_capture == ftCommonCapturePulledProcCapture);
    CHECK(fp.catch_mask == FTCATCHKIND_MASK_COMMON);

    fp.search_gobj = mock_gobj2;                 /* what the search records */
    {
        /* the pull makes the grab swirl, efcatchswirl.mdl's
         * tree, at the catcher's heavy-item joint */
        GObj *before[64];
        GObj *effect_gobj;
        Vec3f want = { 0.0F, 0.0F, 0.0F };
        s32 nbefore = 0, made = 0, k;

        ef_pool_reset();
        for (effect_gobj = gGCCommonLinks[nGCCommonLinkIDEffect];
             (effect_gobj != NULL) && (nbefore < ARRAY_COUNT(before));
             effect_gobj = effect_gobj->link_next)
        {
            before[nbefore++] = effect_gobj;
        }
        ftCommonCatchPullProcCatch(mock_gobj);   /* proc_catch */
        gmCollisionGetFighterPartsWorldPosition(fp.joints[fp.attr->joint_itemheavy_id], &want);

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
                CHECK(dobj != NULL);
                if (dobj != NULL)
                {
                    CHECK(dobj->child != NULL);
                    CHECK_EQF(dobj->translate.vec.f.x, want.x);
                    CHECK_EQF(dobj->translate.vec.f.y, want.y);
                    CHECK_EQF(dobj->translate.vec.f.z, want.z);
                }
                gcEjectGObj(effect_gobj);
            }
            effect_gobj = next;
        }
        CHECK(made == 1);
        sEFManagerStructsAllocFree = NULL;
        sEFManagerStructsFreeNum = 0;
    }
    ftCommonCapturePulledProcCapture(mock_gobj2, mock_gobj); /* proc_capture */

    CHECK_STATUS(nFTCommonStatusCatchPull);
    CHECK(fp.catch_gobj == mock_gobj2);
    CHECK(fp.is_catch_or_capture == FALSE);
    CHECK(fp.capture_immune_mask == FTCATCHKIND_MASK_ALL);
    CHECK(fp2.status_id == nFTCommonStatusCapturePulled);
    CHECK(fp2.capture_gobj == mock_gobj);
    CHECK(fp2.lr == -fp.lr);
    CHECK(fp2.capture_immune_mask == FTCATCHKIND_MASK_ALL);

    /* the pull runs out (mock_anim_len): catcher -> CatchWait, throw timer
     * armed; caught -> CaptureWait, still cross-linked */
    for (i = 0; i < 30 && fp.status_id == nFTCommonStatusCatchPull; i++)
        frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusCatchWait);
    CHECK(fp.status_vars.common.catchwait.throw_wait > 0);
    CHECK(fp2.status_id == nFTCommonStatusCaptureWait);
    CHECK(fp.catch_gobj == mock_gobj2);
    CHECK(fp2.capture_gobj == mock_gobj);

    /* ftmain.c:1918-1936: in the hold each of the pair runs its physics
     * once a frame, the catcher from the priority-4 process and the
     * caught one from the priority-3 process, after the catcher moves */
    sGrabPhysics[0] = fp.proc_physics;
    sGrabPhysics[1] = fp2.proc_physics;
    fp.proc_physics = fp2.proc_physics = grab_count_physics;
    sGrabPhysicsRuns[0] = sGrabPhysicsRuns[1] = 0;
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusCatchWait);
    CHECK(sGrabPhysicsRuns[0] == 1);
    CHECK(sGrabPhysicsRuns[1] == 1);
    fp.proc_physics = sGrabPhysics[0];
    fp2.proc_physics = sGrabPhysics[1];

    /* the throw. fp.attr->thrown_status is real production data
     * (kMarioAttr, wired at suite startup); only
     * throw_desc still needs mocking (see the comment above
     * mock_throw_desc). Both fighters are Mario, so the caught kind indexes
     * the Mario row. */
    fp.throw_desc = &mock_throw_desc;

    /* the forward throw: the hold's timer runs out, so CatchWait's interrupt
     * fires a forward throw. The catcher -> ThrowF, the caught -> ThrownCommon,
     * airborne with its immune mask up. */
    fp.status_vars.common.catchwait.throw_wait = 0;
    CHECK(ftCommonThrowCheckInterruptCatchWait(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusThrowF);
    CHECK(fp.capture_immune_mask == FTCATCHKIND_MASK_ALL);
    CHECK(fp.catch_gobj == mock_gobj2);          /* still linked until release */
    CHECK(fp2.status_id == nFTCommonStatusThrownCommon);
    CHECK(fp2.ga == nMPKineticsAir);
    CHECK(fp2.jumps_used == 1);
    CHECK(fp2.capture_gobj == mock_gobj);

    /* the release frame: a figatree event sets flag2, and ThrowProcUpdate
     * hands the caught fighter its knockback and lets go. flag2 == 2 keeps
     * the throw's lr (a forward throw launches away from the catcher). */
    fp2.percent_damage = 0.0f;
    fp.motion_vars.flags.flag2 = 2;
    ftCommonThrowProcUpdate(mock_gobj);
    CHECK(fp.catch_gobj == NULL);
    CHECK(fp.capture_immune_mask == FTCATCHKIND_MASK_NONE);
    CHECK(fp2.capture_gobj == NULL);
    CHECK(fp2.percent_damage == mock_throw_desc.damage);   /* 8% dealt */
    /* the release armed proc_status = ftCommonThrownProcStatus; the launch's
     * ftMainSetStatus runs it once (re-arming the throw-source params) and
     * clears it, so it reads back NULL but its effect (throw_fkind recorded)
     * is visible. */
    CHECK(fp2.proc_status == NULL);
    CHECK(fp2.throw_fkind == nFTKindMario);
    CHECK(fp2.status_id != nFTCommonStatusThrownCommon);   /* launched out of the hold */
    CHECK(fp2.is_hitstun);

    /* the back throw: re-establish the hold, then interrupt with the stick
     * held back and the throw timer still running -- CheckInterrupt reads a
     * back throw. The catcher -> ThrowB, the caught -> the two-stage back
     * throw (MarioBStart now, MarioB queued for its anim end). */
    spawn(0.0f, 0.0f);
    spawn_second(30.0f, 0.0f, +1);
    fp.attr->joint_itemheavy_id = nFTPartsJointTopN;
    fp.throw_desc = &mock_throw_desc;
    mock_anim_len = 4.0f;
    frame(0, 0, N64_A, N64_Z | N64_A, 0);
    fp.search_gobj = mock_gobj2;
    ftCommonCatchPullProcCatch(mock_gobj);
    ftCommonCapturePulledProcCapture(mock_gobj2, mock_gobj);
    for (i = 0; i < 30 && fp.status_id == nFTCommonStatusCatchPull; i++)
        frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusCatchWait);
    /* the stick shoved back this frame (prev in the deadzone, now past the
     * back threshold; lr is +1, so -x is back), the throw timer not yet out */
    fp.status_vars.common.catchwait.throw_wait = 5;
    fp.input.pl.stick_prev.x = 0;
    fp.input.pl.stick_range.x = -80;
    CHECK(ftCommonThrowCheckInterruptCatchWait(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusThrowB);
    CHECK(fp2.status_id == nFTCommonStatusThrownMarioBStart);
    CHECK(fp2.status_vars.common.thrown.status_id == nFTCommonStatusThrownMarioB);

    /* --- the grab breaks. The five release functions were
     * empty stubs until then, so a grab outlived a KO or a hit on either
     * fighter, both still pointing at each other. --- */

    /* the held fighter is KO'd (ftcommondead.c): both let go, into Wait or
     * Fall */
    grab_hold();
    ftCommonThrownDecideDeadResult(mock_gobj2);
    CHECK(fp.catch_gobj == NULL);
    CHECK(fp2.capture_gobj == NULL);
    CHECK(grab_let_go(&fp));
    CHECK(grab_let_go(&fp2));

    /* the catcher is KO'd: the same, from the other side */
    grab_hold();
    ftCommonThrownDecideDeadResult(mock_gobj);
    CHECK(fp.catch_gobj == NULL);
    CHECK(fp2.capture_gobj == NULL);
    CHECK(grab_let_go(&fp));
    CHECK(grab_let_go(&fp2));

    /* the catcher is hit and the held fighter is not: the catcher flinches
     * and the held fighter is launched by the throw's second hit, taking
     * its damage (SetStatusDamageRelease) */
    grab_hold();
    grab_hit(&fp, 30, 64.0f);
    ftCommonDamageUpdateMain(mock_gobj);
    CHECK(fp.catch_gobj == NULL);
    CHECK(fp2.capture_gobj == NULL);
    CHECK(fp.is_hitstun);
    CHECK(fp2.is_hitstun);
    CHECK(fp2.percent_damage > 0);
    CHECK(fp2.percent_damage <= mock_throw_descs[1].damage);

    /* both are hit, and the held fighter's hit is too light to break its
     * hold: it still takes the second hit's damage (UpdateDamageStats) */
    grab_hold();
    grab_hit(&fp, 30, 64.0f);
    grab_hit(&fp2, 1, 64.0f);
    ftCommonDamageUpdateMain(mock_gobj);
    CHECK(fp.catch_gobj == NULL);
    CHECK(fp2.capture_gobj == NULL);
    CHECK(fp2.percent_damage > 0);
    CHECK(fp2.percent_damage <= mock_throw_descs[1].damage);

    /* the held fighter is hit out and the catcher is not: the catcher is
     * pushed back, and takes no damage (SetStatusNoDamageRelease) */
    grab_hold();
    grab_hit(&fp2, FTCOMMON_DAMAGE_CATCH_RELEASE_THRESHOLD, 64.0f);
    ftCommonDamageUpdateMain(mock_gobj2);
    CHECK(fp.catch_gobj == NULL);
    CHECK(fp2.capture_gobj == NULL);
    CHECK(fp2.is_hitstun);
    CHECK(fp.percent_damage == 0);
    CHECK(fp.status_id != nFTCommonStatusCatchWait);

    grab_hit(&fp, 0, 0.0f);
    grab_hit(&fp2, 0, 0.0f);

    /* --- the dash grab: Z held with A tapped out of a dash
     * reaches ftCommonCatchCheckInterruptDashRun in the dash's second
     * frame window. The first window (flag1 set, anim <= 5) carries the
     * standing arm; advancing past frame 5 puts us in the second, where
     * the DashRun arm is the one the grab comes through. --- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusDash);
    for (i = 0; i < 15 && mock_gobj->anim_frame <= 5.0f; i++)
        frame(0, 0, 0, 0, 0);
    CHECK(is_dash());
    CHECK(mock_gobj->anim_frame > 5.0f);         /* the second window */
    frame(0, 0, N64_A, N64_Z | N64_A, 0);
    CHECK_STATUS(nFTCommonStatusCatch);
    CHECK(fp.is_catchstatus == TRUE);

    /* --- the run grab: the same DashRun arm, first in ftCommonRunProcInterrupt --- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusDash);
    i = 0;
    while (!is_run() && i++ < 30)
        frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusRun);
    frame(80, 0, N64_A, N64_Z | N64_A, 0);
    CHECK_STATUS(nFTCommonStatusCatch);
    CHECK(fp.is_catchstatus == TRUE);

    /* Samus's grab makes the grapple beam glow,
     * efgrapplebeam.mdl's tree, hung off her joint 23 by kind 0x4F. The
     * mock has no joint 23, so TopN stands in for the length of the call. */
    {
        GObj *before[64];
        GObj *effect_gobj;
        DObj *saved_joint = fp.joints[23];
        s32 nbefore = 0, made = 0, k;

        fp.joints[23] = fp.joints[nFTPartsJointTopN];
        fp.fkind = nFTKindSamus;
        fp.is_effect_attach = FALSE;
        ef_pool_reset();
        for (effect_gobj = gGCCommonLinks[nGCCommonLinkIDEffect];
             (effect_gobj != NULL) && (nbefore < ARRAY_COUNT(before));
             effect_gobj = effect_gobj->link_next)
        {
            before[nbefore++] = effect_gobj;
        }
        ftCommonCatchSetStatus(mock_gobj);
        CHECK_STATUS(nFTCommonStatusCatch);
        CHECK(fp.is_effect_attach == TRUE);

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
                CHECK((efGetStruct(effect_gobj) != NULL) &&
                      (efGetStruct(effect_gobj)->fighter_gobj == mock_gobj));
                CHECK(dobj != NULL);
                if (dobj != NULL)
                {
                    CHECK(dobj->child != NULL);
                    CHECK(dobj->xobjs[0]->kind == 0x4F);
                    CHECK(dobj->user_data.p == fp.joints[nFTPartsJointTopN]);
                }
                gcEjectGObj(effect_gobj);
            }
            effect_gobj = next;
        }
        CHECK(made == 1);
        sEFManagerStructsAllocFree = NULL;
        sEFManagerStructsFreeNum = 0;
        fp.is_effect_attach = FALSE;
        fp.fkind = nFTKindMario;
        fp.joints[23] = saved_joint;
    }

    (void)is_catch;
    mock_anim_len = 1e9f;
    fp.attr->thrown_status = NULL;
    fp.throw_desc = NULL;
}

/* the jab and the tilts out of Wait, and where each goes when its
 * animation ends (ftcommonattack1.c, attacks3.c, attackhi3.c, attacklw3.c) */
static void test_attack_statuses(void)
{
    spawn(0.0f, 0.0f);
    mock_anim_len = 6.0f;
    frame(0, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttack11);
    CHECK(fp.attack1_status_id == nFTCommonStatusAttack11);
    CHECK_EQF(fp.attack1_followup_frames, 24.0f);
    CHECK(fp.stat_flags.attack_id == nFTStatusAttackIDAttack11);
    CHECK(fp.motion_attack_id == nFTMotionAttackIDAttack11);
    /* the jab's script (MOCK_SCRIPT_ATTACK) raises its hitbox on frame 2 */
    frame(0, 0, 0, N64_A, 0);
    frame(0, 0, 0, 0, 0);
    CHECK(fp.attack_colls[1].attack_state != nGMAttackStateOff);
    CHECK(fp.is_attack_active);
    /* and Wait when the animation ends, hitboxes cleared with the status */
    run_until(0, 0, 20, is_wait);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(fp.attack_colls[1].attack_state == nGMAttackStateOff);

    /* A with the stick forward: the forward tilt, level (stick angle 0
     * picks AttackS3 of the five).
     *
     * Every stick below is 40, not 80, because the smashes sit
     * in the cascade ahead of the tilts. A
     * flicked stick and A is a smash, and 80 on a fresh tap is exactly
     * that input -- test_smash_attacks below is where the 80s went. 40
     * clears the tilts' threshold (20) and misses the smashes' (56 for
     * the forward, 53 for the up and down), so these reach the tilts on
     * any tap age, which is what this test is about. */
    frame(40, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackS3);
    run_until(0, 0, 20, is_wait);
    CHECK_STATUS(nFTCommonStatusWait);
    /* forward and up: the high-angled one */
    frame(40, 30, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackS3Hi);
    run_until(0, 0, 20, is_wait);
    /* backward: the tilt needs the stick toward the facing (lr +1) */
    frame(-40, 0, N64_A, N64_A, 0);
    CHECK(fp.status_id != nFTCommonStatusAttackS3);
    spawn(0.0f, 0.0f);
    mock_anim_len = 6.0f;
    /* straight up: the up tilt, centre of its three */
    frame(0, 40, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackHi3);
    run_until(0, 0, 20, is_wait);
    /* up and forward: the forward-leaning one (angle < 77 degrees) */
    frame(30, 40, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackHi3F);
    run_until(0, 0, 20, is_wait);
    /* down: the down tilt, which ends in SquatWait, not Wait */
    frame(0, -40, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackLw3);
    CHECK(fp.motion_attack_id == nFTMotionAttackIDAttackLw3);
    run_until(0, -80, 20, is_squatwait);
    CHECK_STATUS(nFTCommonStatusSquatWait);
}

extern sb32 ftCommonAttackS4CheckInterruptCommon(GObj *fighter_gobj);
extern sb32 ftCommonAttackS4CheckInterruptDash(GObj *fighter_gobj);
extern sb32 ftCommonAttackS4CheckInterruptTurn(GObj *fighter_gobj);
extern sb32 ftCommonAttackHi4CheckInterruptCommon(GObj *fighter_gobj);
extern sb32 ftCommonAttackHi4CheckInterruptKneeBend(GObj *fighter_gobj);
extern sb32 ftCommonAttackLw4CheckInterruptCommon(GObj *fighter_gobj);
extern sb32 ftCommonAttackLw4CheckInterruptSquat(GObj *fighter_gobj);
extern void ftCommonAttackS4ProcUpdate(GObj *fighter_gobj);

/* the three smashes (ftcommonattacks4.c, ftcommonattackhi4.c,
 * ftcommonattacklw4.c): their seven status rows, the six
 * checks, and the four cascade slots that are not the plain one -- the
 * dash's, the turn's, the jumpsquat's and the squat's.
 *
 * What separates a smash from the tilt with the same input is the stick,
 * twice over: how far it is over (56 sideways, 53 up or down, against
 * the tilts' 20) and how recently it got there (tap_stick_x/y inside
 * three or four tics). test_attack_statuses above is the other side of
 * this coin -- it drives the stick to 40, under every smash threshold,
 * to reach the tilts on any tap age. */
static void test_smash_attacks(void)
{
    int i;

    /* the three attribute bits in the mock's is_have */
    spawn(0.0f, 0.0f);
    CHECK(fp.attr->is_have_attacks4);
    CHECK(fp.attr->is_have_attackhi4);
    CHECK(fp.attr->is_have_attacklw4);

    /* ---- the seven rows, 202-208, the last holes in
     * dFTCommonActionStatusDescs ---- */

    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackS4Hi, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionAttackS4Hi);
    CHECK(fp.proc_update == ftCommonAttackS4ProcUpdate);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundFrictionOrTransN);
    CHECK(fp.proc_map == mpCommonSetFighterFallOnEdgeBreak);
    CHECK(fp.motion_attack_id == nFTMotionAttackIDAttackS4);
    CHECK(fp.stat_flags.attack_id == nFTStatusAttackIDAttackS4);
    /* the seven are the only rows in the table whose sflags say smash */
    CHECK(fp.stat_flags.is_smash_attack);
    CHECK(fp.stat_flags.ga == nMPKineticsGround);

    /* the other four forward smashes differ in motion and nothing else */
    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackS4HiS, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionAttackS4HiS);
    CHECK(fp.proc_update == ftCommonAttackS4ProcUpdate);
    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackS4, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionAttackS4);
    CHECK(fp.proc_update == ftCommonAttackS4ProcUpdate);
    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackS4LwS, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionAttackS4LwS);
    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackS4Lw, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionAttackS4Lw);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundFrictionOrTransN);

    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackHi4, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionAttackHi4);
    CHECK(fp.proc_update == ftAnimEndSetWait);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundFrictionOrTransN);
    CHECK(fp.stat_flags.attack_id == nFTStatusAttackIDAttackHi4);
    CHECK(fp.stat_flags.is_smash_attack);

    /* the down smash is the one row of the seven with a physics column
     * of its own: plain ground friction, not the TransN one */
    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackLw4, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionAttackLw4);
    CHECK(fp.proc_update == ftAnimEndSetWait);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == mpCommonSetFighterFallOnEdgeBreak);
    CHECK(fp.stat_flags.attack_id == nFTStatusAttackIDAttackLw4);
    CHECK(fp.stat_flags.is_smash_attack);

    /* and the tilt next door is not a smash */
    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackLw3, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(!fp.stat_flags.is_smash_attack);

    /* ---- the union aliases the three files lean on (ft/ftcommon.h):
     * ftCommonAttack4StatusVars.lr is the word ftCommonTurnStatusVars
     * calls lr_turn, which is how the turn's forward smash measures the
     * stick against the way the turn is going. Nothing ever writes
     * attack4.lr; the turn does. is_goto_attacklw4 is a word no code in
     * the game writes at all. ---- */
    CHECK((void *)&fp.status_vars.common.attack4.lr ==
          (void *)&fp.status_vars.common.turn.lr_turn);
    CHECK((void *)&fp.status_vars.common.attack4.is_goto_attacklw4 !=
          (void *)&fp.status_vars.common.turn.lr_turn);

    /* ---- the forward smash out of Wait, and its five angles.
     * ftCommonAttackS4SetStatus picks between a five-way, a three-way
     * and a one-way spread by asking whether the pack carries the
     * angled motions. Every one of the mock's 256 motions carries anim
     * 0, so the five-way branch is the one a frame-driven test reaches
     * here; the three-way and the one-way are driven below by hiding
     * the motions the branch asks for. ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 6.0f;
    frame(80, 0, N64_A, N64_A, 0);          /* 0 degrees */
    CHECK_STATUS(nFTCommonStatusAttackS4);
    CHECK(fp.stat_flags.is_smash_attack);
    run_until(0, 0, 20, is_wait);
    frame(80, 60, N64_A, N64_A, 0);         /* 36.9, past 21 */
    CHECK_STATUS(nFTCommonStatusAttackS4Hi);
    run_until(0, 0, 20, is_wait);
    frame(80, 20, N64_A, N64_A, 0);         /* 14.0, past 7 */
    CHECK_STATUS(nFTCommonStatusAttackS4HiS);
    run_until(0, 0, 20, is_wait);
    frame(80, -20, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackS4LwS);
    run_until(0, 0, 20, is_wait);
    frame(80, -60, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackS4Lw);
    run_until(0, 0, 20, is_wait);
    /* both edges of the 21-degree band */
    frame(80, 31, N64_A, N64_A, 0);         /* 21.2 */
    CHECK_STATUS(nFTCommonStatusAttackS4Hi);
    run_until(0, 0, 20, is_wait);
    frame(80, 29, N64_A, N64_A, 0);         /* 19.9 */
    CHECK_STATUS(nFTCommonStatusAttackS4HiS);
    run_until(0, 0, 20, is_wait);
    /* and of the 7-degree one */
    frame(80, 11, N64_A, N64_A, 0);         /* 7.8 */
    CHECK_STATUS(nFTCommonStatusAttackS4HiS);
    run_until(0, 0, 20, is_wait);
    frame(80, 9, N64_A, N64_A, 0);          /* 6.4 */
    CHECK_STATUS(nFTCommonStatusAttackS4);
    run_until(0, 0, 20, is_wait);

    /* the three-way spread: hide the motion the five-way branch asks
     * for and the same 14-degree stick is a level smash, not a middle
     * high-angled one. Its band is 17 degrees, not 21. */
    mock_motions[nFTCommonMotionAttackS4HiS].anim = -1;
    frame(80, 20, N64_A, N64_A, 0);         /* 14.0, inside 17 */
    CHECK_STATUS(nFTCommonStatusAttackS4);
    run_until(0, 0, 20, is_wait);
    frame(80, 31, N64_A, N64_A, 0);         /* 21.2, past 17 */
    CHECK_STATUS(nFTCommonStatusAttackS4Hi);
    run_until(0, 0, 20, is_wait);
    frame(80, -31, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackS4Lw);
    run_until(0, 0, 20, is_wait);
    /* the one-way: no angled motion at all, every stick is the level one */
    mock_motions[nFTCommonMotionAttackS4Hi].anim = -1;
    frame(80, 60, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackS4);
    run_until(0, 0, 20, is_wait);
    mock_motions[nFTCommonMotionAttackS4HiS].anim = 0;
    mock_motions[nFTCommonMotionAttackS4Hi].anim = 0;

    /* the stick threshold: 55 is a tilt, 56 is a smash */
    frame(55, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackS3);
    run_until(0, 0, 20, is_wait);
    frame(56, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackS4);
    run_until(0, 0, 20, is_wait);

    /* backward is a forward smash too -- the common check takes the
     * stick's magnitude and ftParamSetStickLR turns the fighter */
    CHECK(fp.lr == 1);
    frame(-80, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackS4);
    CHECK(fp.lr == -1);

    /* the tap window: three tics, and it is the only thing between this
     * input and the forward tilt */
    spawn(0.0f, 0.0f);
    fp.input.pl.stick_range.x = 80;
    fp.input.pl.stick_range.y = 0;
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.tap_stick_x = FTCOMMON_ATTACKS4_BUFFER_TICS_MAX;
    CHECK(ftCommonAttackS4CheckInterruptCommon(mock_gobj) == FALSE);
    fp.tap_stick_x = FTCOMMON_ATTACKS4_BUFFER_TICS_MAX - 1;
    CHECK(ftCommonAttackS4CheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusAttackS4);
    /* and no A press, no smash */
    spawn(0.0f, 0.0f);
    fp.input.pl.stick_range.x = 80;
    fp.tap_stick_x = 0;
    fp.input.pl.button_tap = 0;
    CHECK(ftCommonAttackS4CheckInterruptCommon(mock_gobj) == FALSE);

    /* the attribute gate, all three */
    spawn(0.0f, 0.0f);
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.tap_stick_x = fp.tap_stick_y = 0;
    fp.input.pl.stick_range.x = 80;
    fp.attr->is_have_attacks4 = FALSE;
    CHECK(ftCommonAttackS4CheckInterruptCommon(mock_gobj) == FALSE);
    fp.attr->is_have_attacks4 = TRUE;
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = 80;
    fp.attr->is_have_attackhi4 = FALSE;
    CHECK(ftCommonAttackHi4CheckInterruptCommon(mock_gobj) == FALSE);
    fp.attr->is_have_attackhi4 = TRUE;
    CHECK(ftCommonAttackHi4CheckInterruptCommon(mock_gobj) == TRUE);
    spawn(0.0f, 0.0f);
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.tap_stick_y = 0;
    fp.input.pl.stick_range.y = -80;
    fp.attr->is_have_attacklw4 = FALSE;
    CHECK(ftCommonAttackLw4CheckInterruptCommon(mock_gobj) == FALSE);
    fp.attr->is_have_attacklw4 = TRUE;
    CHECK(ftCommonAttackLw4CheckInterruptCommon(mock_gobj) == TRUE);

    /* ---- the up smash and the down smash out of Wait: one status
     * each, no angles, and a four-tic window instead of three ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 6.0f;
    frame(0, 80, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackHi4);
    run_until(0, 0, 20, is_wait);
    frame(0, 52, N64_A, N64_A, 0);          /* one short of 53 */
    CHECK_STATUS(nFTCommonStatusAttackHi3);
    run_until(0, 0, 20, is_wait);
    frame(0, 53, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackHi4);
    run_until(0, 0, 20, is_wait);
    frame(0, -80, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackLw4);
    run_until(0, 0, 20, is_wait);
    frame(0, -52, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackLw3);
    run_until(0, -52, 20, is_squatwait);
    spawn(0.0f, 0.0f);
    mock_anim_len = 6.0f;
    frame(0, -53, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackLw4);

    /* their tap window is four, not three */
    spawn(0.0f, 0.0f);
    fp.input.pl.stick_range.y = 80;
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.tap_stick_y = FTCOMMON_ATTACKHI4_BUFFER_TICS_MAX;
    CHECK(ftCommonAttackHi4CheckInterruptCommon(mock_gobj) == FALSE);
    fp.tap_stick_y = FTCOMMON_ATTACKHI4_BUFFER_TICS_MAX - 1;
    CHECK(ftCommonAttackHi4CheckInterruptCommon(mock_gobj) == TRUE);
    spawn(0.0f, 0.0f);
    fp.input.pl.stick_range.y = -80;
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.tap_stick_y = FTCOMMON_ATTACKLW4_BUFFER_TICS_MAX;
    CHECK(ftCommonAttackLw4CheckInterruptCommon(mock_gobj) == FALSE);
    fp.tap_stick_y = FTCOMMON_ATTACKLW4_BUFFER_TICS_MAX - 1;
    CHECK(ftCommonAttackLw4CheckInterruptCommon(mock_gobj) == TRUE);

    /* ---- the dash's slot (ftcommondash.c:33): no tap window at all,
     * and the stick measured against the facing rather than by
     * magnitude -- a dash already has the stick over, so the tap is
     * long stale by the time A is pressed ---- */
    spawn(0.0f, 0.0f);
    fp.lr = 1;
    fp.input.pl.stick_range.x = 80;
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.tap_stick_x = 200;
    CHECK(ftCommonAttackS4CheckInterruptCommon(mock_gobj) == FALSE);
    CHECK(ftCommonAttackS4CheckInterruptDash(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusAttackS4);
    /* the stick behind the fighter is not a dash smash */
    spawn(0.0f, 0.0f);
    fp.lr = 1;
    fp.input.pl.stick_range.x = -80;
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.tap_stick_x = 200;
    CHECK(ftCommonAttackS4CheckInterruptDash(mock_gobj) == FALSE);

    /* ---- the turn's slot (ftcommonturn.c:49-60): the grab first, then
     * a forward smash measured against lr_turn for the first six frames
     * of the turn and against the facing after ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(-80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusTurn);
    CHECK(fp.status_vars.common.turn.lr_turn == -1);
    CHECK(fp.status_vars.common.attack4.lr == -1);      /* the alias */
    CHECK(fp.status_vars.common.turn.attacks4_buffer == 0);
    /* the stick the way the turn is going, inside the window: the
     * turn's own check takes it and turns the fighter with it */
    frame(-80, 0, N64_A, N64_A, 0);
    CHECK(fp.status_vars.common.turn.attacks4_buffer == 1);
    CHECK_STATUS(nFTCommonStatusAttackS4);
    CHECK(fp.lr == -1);
    /* the stick the other way, inside the window: nothing. The turn is
     * the one cascade with no ftCommonAttackS4CheckInterruptCommon in
     * its run of seven, so a stick that fails the turn's own check has
     * no second chance at a forward smash. */
    spawn(0.0f, 0.0f);
    frame(-80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusTurn);
    frame(80, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusTurn);
    /* the buffer counts every frame of the turn: one for the frame the
     * smash was refused on, and one for each frame since */
    for (i = 0; i < 4; i++)
        frame(0, 0, 0, 0, 0);
    CHECK(fp.status_vars.common.turn.attacks4_buffer == 5);
    /* and the grab, the slot ahead of all of it */
    spawn(0.0f, 0.0f);
    mock_anim_len = 6.0f;
    frame(-80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusTurn);
    frame(0, 0, N64_A, N64_Z | N64_A, 0);
    CHECK_STATUS(nFTCommonStatusCatch);

    /* ---- the jumpsquat's slot (ftcommonkneebend.c:37-45): the up-B
     * first, then the up smash, and only if neither fires does the
     * stick go on raising the jump's force. No tap window here either
     * -- the stick is already up, which is why the fighter is in
     * jumpsquat at all. ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusKneeBend);
    frame(0, 80, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackHi4);
    /* with the stick stale: still the up smash */
    spawn(0.0f, 0.0f);
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusKneeBend);
    fp.input.pl.stick_range.y = 80;
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.tap_stick_y = 200;
    CHECK(ftCommonAttackHi4CheckInterruptCommon(mock_gobj) == FALSE);
    CHECK(ftCommonAttackHi4CheckInterruptKneeBend(mock_gobj) == TRUE);
    /* the up-B out of jumpsquat, the arm ahead of it */
    spawn(0.0f, 0.0f);
    frame(0, 0, FT_JUMP_BUTTONS_TEST, FT_JUMP_BUTTONS_TEST, 0);
    CHECK_STATUS(nFTCommonStatusKneeBend);
    frame(0, 80, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialHi);

    /* ---- the squat's slot (ftcommonsquat.c:16 and 36): the down smash
     * there is gated by is_goto_attacklw4 instead of by the tap window,
     * which is the whole of what makes the squat spelling of the run of
     * seven different from the common one ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 3.0f;
    frame(0, -80, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusSquat);
    run_until(0, -80, 40, is_squatwait);
    CHECK_STATUS(nFTCommonStatusSquatWait);
    mock_anim_len = 6.0f;
    /* One frame more than this needed before, which put the
     * decomp's own ftCommonSquatSetStatusNoPass in place of a hand-copy
     * that had left out its ftMainPlayAnimEventsAll -- so the squat now
     * reaches SquatWait one frame sooner and the tap window is one tic
     * shorter when it gets here. The claim is unchanged: the stick has
     * been down long enough that the common check would refuse it. */
    frame(0, -80, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusSquatWait);
    CHECK(fp.tap_stick_y >= FTCOMMON_ATTACKLW4_BUFFER_TICS_MAX);
    fp.status_vars.common.attack4.is_goto_attacklw4 = FALSE;
    frame(0, -80, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackLw3);     /* the tilt below it */
    spawn(0.0f, 0.0f);
    mock_anim_len = 3.0f;
    frame(0, -80, 0, 0, 0);
    run_until(0, -80, 40, is_squatwait);
    mock_anim_len = 6.0f;
    fp.status_vars.common.attack4.is_goto_attacklw4 = TRUE;
    frame(0, -80, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackLw4);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
}

extern sb32 ftCommonAttackDashCheckInterruptCommon(GObj *fighter_gobj);

/* the dash attack (ftcommonattackdash.c), status 192, and
 * with it the last two holes in the dash's and the run's cascades: the
 * neutral-B, which both cascades must call.
 *
 * The check is the simplest in the file -- A alone, no stick test at all,
 * because the fighter is already running and only the dash and the run
 * call it. What the row is worth watching for is its physics column:
 * ftPhysicsApplyGroundVelTransN, so the lunge's distance comes out of the
 * animation's TransN joint rather than a velocity, which is why the mock
 * has to give nFTCommonMotionAttackDash the TransN flag ft/ftdata.c gives
 * it. */
static void test_attack_dash(void)
{
    int i;

    spawn(0.0f, 0.0f);
    CHECK(fp.attr->is_have_attackdash);

    /* ---- the row ---- */
    ftMainSetStatus(mock_gobj, nFTCommonStatusAttackDash, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionAttackDash);
    CHECK(fp.proc_update == ftAnimEndSetWait);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelTransN);
    CHECK(fp.proc_map == mpCommonSetFighterFallOnEdgeBreak);
    CHECK(fp.motion_attack_id == nFTMotionAttackIDAttackDash);
    CHECK(fp.stat_flags.attack_id == nFTStatusAttackIDAttackDash);
    /* it is an attack but not a smash -- only 202-208 carry that bit */
    CHECK(!fp.stat_flags.is_smash_attack);
    CHECK(fp.stat_flags.ga == nMPKineticsGround);

    /* ---- the check: A alone, whatever the stick is doing ---- */
    spawn(0.0f, 0.0f);
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = 0;
    fp.input.pl.button_tap = fp.input.button_mask_a;
    CHECK(ftCommonAttackDashCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusAttackDash);
    spawn(0.0f, 0.0f);
    fp.input.pl.button_tap = 0;
    CHECK(ftCommonAttackDashCheckInterruptCommon(mock_gobj) == FALSE);
    fp.input.pl.button_tap = fp.input.button_mask_a;
    fp.attr->is_have_attackdash = FALSE;
    CHECK(ftCommonAttackDashCheckInterruptCommon(mock_gobj) == FALSE);
    fp.attr->is_have_attackdash = TRUE;

    /* ---- out of a dash (ftcommondash.c:55, the SECOND window). The
     * first window, frames 0-5 with the script's flag1 up, has the
     * forward smash instead and no dash attack at all -- so A with a
     * neutral stick does nothing there, which is checked first. ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusDash);
    frame(0, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusDash);
    /* past frame 5, with the stick let go so the dash does not become a
     * run, the second window takes it */
    for (i = 0; i < 6; i++)
        frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusDash);
    CHECK(mock_gobj->anim_frame > 5.0f);
    frame(0, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackDash);

    /* ---- out of a run (ftcommonrun.c:16) ---- */
    spawn(0.0f, 0.0f);
    frame(80, 0, 0, 0, 0);
    i = 0;
    while (!is_run() && i++ < 30)
        frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusRun);
    frame(80, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttackDash);

    /* ---- the neutral-B, the other hole in those cascades. It sits ahead
     * of the grab in both cascades, so B out of a dash or a run is
     * Mario's fireball and not a reach. ---- */
    spawn(0.0f, 0.0f);
    frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusDash);
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialN);
    /* and in the second window too, where it is the first thing tried */
    spawn(0.0f, 0.0f);
    frame(80, 0, 0, 0, 0);
    for (i = 0; i < 6; i++)
        frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusDash);
    frame(0, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialN);

    spawn(0.0f, 0.0f);
    frame(80, 0, 0, 0, 0);
    i = 0;
    while (!is_run() && i++ < 30)
        frame(80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusRun);
    frame(80, 0, N64_B, N64_B, 0);
    CHECK_STATUS(nFTMarioStatusSpecialN);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
}

/* the four singles: statuses 14, 58, 59 and 165, the rows
 * that must not be left zeroed and belong to no unported family.
 * 58 is the live one: every ported up-B ends in it. */
static void test_singles_rows(void)
{
    FTAttributes *attr;
    float y0;
    int idle_used, mash_used, i;

    /* ---- 14 WalkEnd: all-NULL in the game's table too, and nothing in
     * the game sets it. The row carries a motion id and nothing else. ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    frame(0, 0, 0, 0, 0);
    ftMainSetStatus(mock_gobj, nFTCommonStatusWalkEnd, 0.0f, 1.0f,
                    FTSTATUS_PRESERVE_NONE);
    CHECK(fp.motion_id == nFTCommonMotionWalkEnd);
    CHECK(fp.proc_update == NULL);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == NULL);
    CHECK(fp.proc_map == NULL);
    CHECK(fp.stat_flags.ga == nMPKineticsGround);
    CHECK(!fp.stat_flags.is_smash_attack);
    CHECK(fp.motion_attack_id == nFTMotionAttackIDNone);
    /* a status with no procs at all still steps without doing anything */
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWalkEnd);

    /* ---- 58 FallSpecial: the helpless fall. Its four procs come from
     * ft/ftcommon/ftcommonfallspecial.c, compiled unmodified -- so without
     * this row the six up-Bs would drop their fighter into a status that
     * runs nothing at all. ---- */
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    attr = fp.attr;
    ftCommonFallSpecialSetStatus(mock_gobj, 0.5f, FALSE, TRUE, FALSE,
                                 20.0f, FALSE);
    CHECK_STATUS(nFTCommonStatusFallSpecial);
    CHECK(fp.motion_id == nFTCommonMotionFallSpecial);
    CHECK(fp.proc_update == NULL);
    CHECK(fp.proc_interrupt == ftCommonFallSpecialProcInterrupt);
    CHECK(fp.proc_physics == ftCommonFallSpecialProcPhysics);
    CHECK(fp.proc_map == ftCommonFallSpecialProcMap);
    /* the kinetics column is Ground in the game's row even though the
     * status is airborne -- the setter moves the fighter itself */
    CHECK(fp.stat_flags.ga == nMPKineticsGround);
    CHECK(fp.ga == nMPKineticsAir);
    /* the setter's own state */
    CHECK_NEAR(fp.status_vars.common.fallspecial.drift,
               attr->air_speed_max_x * 0.5f, 1e-3f);
    CHECK(fp.status_vars.common.fallspecial.is_allow_pass == TRUE);
    CHECK(fp.status_vars.common.fallspecial.is_fall_accelerate == TRUE);
    CHECK(fp.status_vars.common.fallspecial.is_goto_landing == FALSE);
    CHECK_NEAR(fp.status_vars.common.fallspecial.landing_lag, 20.0f, 1e-3f);
    CHECK(fp.status_vars.common.fallspecial.is_allow_interrupt == FALSE);
    CHECK(fp.jumps_used == attr->jumps_max);   /* no jump out of helpless */
    CHECK(fp.is_special_interrupt == TRUE);

    /* the row's physics runs: accelerating, gravity pulls with no clamp
     * short of terminal, and the fall keeps getting faster */
    fp.physics.vel_air.y = 0.0f;
    ftCommonFallSpecialProcPhysics(mock_gobj);
    y0 = fp.physics.vel_air.y;
    CHECK(y0 < 0.0f);
    ftCommonFallSpecialProcPhysics(mock_gobj);
    CHECK(fp.physics.vel_air.y < y0);

    /* not accelerating: the same gravity, clamped at the fast terminal */
    fp.status_vars.common.fallspecial.is_fall_accelerate = FALSE;
    fp.physics.vel_air.y = -attr->tvel_fast * 4.0f;
    ftCommonFallSpecialProcPhysics(mock_gobj);
    CHECK_NEAR(fp.physics.vel_air.y, -attr->tvel_fast, 1e-3f);

    /* ftCommonFallSpecialProcPass, the pass-through predicate the map proc
     * hands to mpCommonCheckFighterPassCliff: TRUE (do not pass) unless all
     * three of allow-pass, a pass-flagged floor and a stick held below
     * FTCOMMON_FALLSPECIAL_PASS_STICK_RANGE_MIN hold. */
    fp.status_vars.common.fallspecial.is_allow_pass = TRUE;
    fp.coll_data.floor_flags = MAP_VERTEX_COLL_PASS;
    fp.input.pl.stick_range.y = FTCOMMON_FALLSPECIAL_PASS_STICK_RANGE_MIN - 1;
    CHECK(ftCommonFallSpecialProcPass(mock_gobj) == FALSE);   /* passes */
    fp.input.pl.stick_range.y = FTCOMMON_FALLSPECIAL_PASS_STICK_RANGE_MIN;
    CHECK(ftCommonFallSpecialProcPass(mock_gobj) != FALSE);   /* the edge */
    fp.input.pl.stick_range.y = FTCOMMON_FALLSPECIAL_PASS_STICK_RANGE_MIN - 1;
    fp.coll_data.floor_flags = 0;
    CHECK(ftCommonFallSpecialProcPass(mock_gobj) != FALSE);
    fp.coll_data.floor_flags = MAP_VERTEX_COLL_PASS;
    fp.status_vars.common.fallspecial.is_allow_pass = FALSE;
    CHECK(ftCommonFallSpecialProcPass(mock_gobj) != FALSE);
    fp.input.pl.stick_range.y = 0;
    fp.coll_data.floor_flags = 0;

    /* the row's interrupt is the double jump's, which helpless refuses:
     * jumps_used is already at the max, so pressing jump changes nothing */
    fp.status_vars.common.fallspecial.is_allow_pass = TRUE;
    ftCommonFallSpecialProcInterrupt(mock_gobj);
    CHECK_STATUS(nFTCommonStatusFallSpecial);

    /* ---- 59 LandingFallSpecial: that fall's landing. Its setter needs a
     * non-empty row on the far side. ---- */
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    ftCommonLandingFallSpecialSetStatus(mock_gobj, FALSE, 1.0f);
    CHECK_STATUS(nFTCommonStatusLandingFallSpecial);
    CHECK(fp.motion_id == nFTCommonMotionLandingFallSpecial);
    CHECK(fp.proc_update == ftAnimEndSetWait);
    CHECK(fp.proc_interrupt == ftCommonLandingProcInterrupt);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == mpCommonProcFighterOnCliffEdge);
    CHECK(fp.stat_flags.ga == nMPKineticsGround);
    /* and it ends the way every landing does, into Wait */
    mock_anim_len = 4.0f;
    CHECK(run_until(0, 0, 30, is_wait) < 30);
    mock_anim_len = 1e9f;

    /* ---- 165 FuraSleep: the sleep. Same shape as FuraFura, a longer
     * floor (75 tics against the dizzy's 60) and no shield_health. ---- */
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    fp.percent_damage = 100;
    ftCommonFuraSleepSetStatus(mock_gobj);
    CHECK_STATUS(nFTCommonStatusFuraSleep);
    CHECK(fp.motion_id == nFTCommonMotionFuraSleep);
    CHECK(fp.proc_update == ftCommonFuraSleepProcUpdate);
    CHECK(fp.proc_interrupt == NULL);
    CHECK(fp.proc_physics == ftPhysicsApplyGroundVelFriction);
    CHECK(fp.proc_map == mpCommonSetFighterFallOnGroundBreak);
    /* 300 - 100 + 75 */
    CHECK(fp.breakout_wait == FTCOMMON_FURASLEEP_BREAKOUT_WAIT_DEFAULT - 100 +
                              FTCOMMON_FURASLEEP_BREAKOUT_WAIT_MIN);

    /* past the default the window clamps at the floor, never below it */
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    fp.percent_damage = FTCOMMON_FURASLEEP_BREAKOUT_WAIT_DEFAULT + 50;
    ftCommonFuraSleepSetStatus(mock_gobj);
    CHECK(fp.breakout_wait == FTCOMMON_FURASLEEP_BREAKOUT_WAIT_MIN);

    /* the counter reaches zero -> Wait */
    fp.breakout_wait = 1;
    frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);

    /* and mashing leaves it sooner than waiting it out, as the dizzy does */
    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    fp.percent_damage = 260;                  /* 300-260+75 = 115 */
    ftCommonFuraSleepSetStatus(mock_gobj);
    CHECK(fp.breakout_wait == 115);
    idle_used = run_until(0, 0, 400, is_wait);
    CHECK(idle_used < 400);

    spawn(0.0f, 0.0f);
    frame(0, 0, 0, 0, 0);
    fp.percent_damage = 260;
    ftCommonFuraSleepSetStatus(mock_gobj);
    for (i = 0; i < 400 && fp.status_id == nFTCommonStatusFuraSleep; i++)
        frame(0, 0, 0, (i & 1) ? N64_A : 0, 0);
    mash_used = i;
    CHECK(mash_used < idle_used);

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
}

/* A fighter's projectile lands on another fighter through the game's
 * own search -- ft/ftmain.c ftMainSearchHitWeapon and the ftMainUpdate*
 * StatWeapon family. Guards against Fox's Blaster drawing (the
 * weapon-table bind) but never connecting.
 * Fox's shot is made off the bound table (test_wp_attrs) at the victim's
 * own hurtbox, its hit position placed the way wpProcessProcWeaponMain
 * does it every tic, and one frame of the fighters' processes does the
 * rest: the victim's ProcSearchHitAll walks the weapon link, the stat
 * update logs the hit as nFTHitLogObjectWeapon, and ProcParams pays it
 * out. The numbers are the table's: 6 damage, angle 10, no element. */
static void test_weapon_hits_fighter(void)
{
    Vec3f pos;
    GObj *wg;
    WPStruct *wp;

    CHECK(wpAttrsLoad() == 0);
    wpAttrsBind();
    wpManagerAllocWeapons();

    spawn(0.0f, 0.0f);
    spawn_second(100.0f, 0.0f, -1);
    mock_anim_len = 1e9f;
    idle_frames(1);
    CHECK(fp2.status_id == nFTCommonStatusWait);
    CHECK(fp2.percent_damage == 0);

    /* the shot, at the victim's first hurtbox: the mock's hip
     * (mock_joints[0]) sits 150 above the TopN and carries the box, 50
     * wide, so the 20-unit shot there is well inside it */
    pos.x = DObjGetStruct(mock_gobj2)->translate.vec.f.x + fp2.damage_colls[0].offset.x;
    pos.y = DObjGetStruct(mock_gobj2)->translate.vec.f.y + 150.0f + fp2.damage_colls[0].offset.y;
    pos.z = 0.0f;
    wg = wpFoxBlasterMakeWeapon(mock_gobj, &pos);
    CHECK(wg != NULL);
    wp = wpGetStruct(wg);
    CHECK(wp->owner_gobj == mock_gobj);
    CHECK(wp->player == 0);
    CHECK(wp->attack_coll.damage == 6);
    /* MakeWeapon's own tail call already placed the hit (New -> Transfer,
     * pos_curr at the shot), the way MakeItem's does */
    CHECK(wp->attack_coll.attack_state == nGMAttackStateTransfer);
    CHECK(wp->attack_coll.interact_mask & GMHITCOLLISION_FLAG_FIGHTER);

    /* one more tic of the weapon's own process: Transfer -> Interpolate,
     * pos_prev stamped, the shot still where it was made */
    wpProcessUpdateHitPositions(wg);
    CHECK(wp->attack_coll.attack_state == nGMAttackStateInterpolate);

    frame(0, 0, 0, 0, 0);
    CHECK(fp2.percent_damage == 6);
    CHECK(fp2.damage_object_class == nFTHitLogObjectWeapon);
    CHECK(fp2.damage_object_kind == wp->kind);
    CHECK(fp2.damage_player == 0);
    CHECK(fp2.damage_angle == 10);
    CHECK(fp2.damage_element == 0);
    /* the shot flies right (the owner faces +1), so the victim is hit
     * from the left: ftmain.c's weapon arm reads vel_air.x, not positions */
    CHECK(wp->physics.vel_air.x > 5.0f);
    CHECK(fp2.damage_lr == -1);
    CHECK(fp2.damage_joint_id == fp2.damage_colls[0].joint_id);
    CHECK(fp2.is_hitstun);
    /* the weapon's own bookkeeping: hit_normal_damage (the Blaster does
     * not rehit) and the record that keeps it from landing twice */
    CHECK(wp->hit_normal_damage == 6);
    CHECK(wp->attack_coll.attack_records[0].victim_gobj == mock_gobj2);
    CHECK(wp->attack_coll.attack_records[0].victim_flags.is_interact_hurt);
    /* the battle stats, credited to the shot's owner */
    CHECK(mock_battle.players[0].total_damage_given == 6);
    CHECK(mock_battle.players[1].total_damage_all == 6);
    CHECK(mock_battle.players[1].total_damage_players[0] == 6);
    /* the owner's own percent is untouched: the search skips owner_gobj */
    CHECK(fp.percent_damage == 0);

    /* the record holds: the same shot on the same victim does not land
     * again (the victim was launched, so put the shot back on it first) */
    ft_clear_knockback(fpp2);
    DObjGetStruct(wg)->translate.vec.f.x = DObjGetStruct(mock_gobj2)->translate.vec.f.x + fp2.damage_colls[0].offset.x;
    DObjGetStruct(wg)->translate.vec.f.y = DObjGetStruct(mock_gobj2)->translate.vec.f.y + 150.0f + fp2.damage_colls[0].offset.y;
    wpProcessUpdateHitPositions(wg);
    frame(0, 0, 0, 0, 0);
    CHECK(fp2.percent_damage == 6);

    wpMainDestroyWeapon(wg);
    CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);
}

/* The item half of the same search -- ftMainSearchHitItem and the
 * ftMainUpdate*StatItem family. Two hand-made items (the pattern of
 * test_it_manager_make_item: a model-less attribute record behind a
 * one-entry file), both with a live hitbox on the fighter's hurtbox: a
 * thrown-type one that damages (logged as nFTHitLogObjectItem, launched
 * on the table's angle and element), and a Star -- nITTypeTouch, kind
 * nITKindStar -- whose touch is the invincibility, the star music and
 * no damage at all, with its hitbox switched off by the touch. */
static sb32 test_it_hit_proc_update(GObj *g) { (void)g; return FALSE; }
static sb32 test_it_hit_proc_map(GObj *g) { (void)g; return FALSE; }

static void test_item_hits_fighter(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    ITStruct *save_free = gITManagerStructsAllocFree;
    ITAttributes attr;
    ITDesc desc;
    void *file_ptr;
    Vec3f pos;
    Vec3f vel = { 0.0F, 0.0F, 0.0F };
    GObj *item_gobj;
    ITStruct *ip;
    s32 saved_type;
    u8 saved_player;
    int i;

    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
        pool[i].next = &pool[i + 1];
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    memset(&attr, 0, sizeof(attr));
    attr.data = NULL;
    attr.type = nITTypeThrow;
    attr.weight = 1;
    attr.is_give_hitlag = TRUE;
    attr.damage = 12;
    attr.element = nGMHitElementFire;
    attr.size = 40.0F;                  /* halved at MakeItem: 20 */
    attr.angle = 45;
    attr.knockback_scale = 100; attr.knockback_weight = 50; attr.knockback_base = 20;
    attr.attack_count = 1;
    attr.hitstatus = nGMHitStatusNormal;
    attr.map_coll_top = 10; attr.map_coll_center = 0; attr.map_coll_bottom = -10; attr.map_coll_width = 15;

    file_ptr = &attr;
    memset(&desc, 0, sizeof(desc));
    desc.kind = nITKindBombHei;
    desc.p_file = &file_ptr;
    desc.o_attributes = 0;
    desc.attack_state = nGMAttackStateNew;
    desc.proc_update = test_it_hit_proc_update;
    desc.proc_map = test_it_hit_proc_map;

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    idle_frames(1);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(fp.percent_damage == 0);

    /* ---- the damaging item, on the fighter's first hurtbox (the mock's
     * hip, 150 above the TopN, the box 50 wide -- test_weapon_hits_
     * fighter's placement) ---- */
    pos.x = DObjGetStruct(mock_gobj)->translate.vec.f.x + fp.damage_colls[0].offset.x;
    pos.y = DObjGetStruct(mock_gobj)->translate.vec.f.y + 150.0f + fp.damage_colls[0].offset.y;
    pos.z = 0.0f;
    item_gobj = itManagerMakeItem(NULL, &desc, &pos, &vel, 0);
    CHECK(item_gobj != NULL);
    ip = itGetStruct(item_gobj);
    CHECK(ip->owner_gobj == NULL);
    CHECK(ip->type == nITTypeThrow);
    CHECK(ip->attack_coll.attack_state == nGMAttackStateTransfer); /* MakeItem's own tail call placed it */
    CHECK(ip->attack_coll.interact_mask & GMHITCOLLISION_FLAG_FIGHTER);

    frame(0, 0, 0, 0, 0);
    CHECK(fp.percent_damage == 12);
    CHECK(fp.damage_object_class == nFTHitLogObjectItem);
    CHECK(fp.damage_object_kind == nITKindBombHei);
    CHECK(fp.damage_angle == 45);
    CHECK(fp.damage_element == nGMHitElementFire);
    /* a still item: the side is read off the positions, item to the
     * fighter's right or on it -> hit from the right */
    CHECK(fp.damage_lr == ((DObjGetStruct(mock_gobj)->translate.vec.f.x <
                            DObjGetStruct(item_gobj)->translate.vec.f.x) ? +1 : -1));
    CHECK(ip->hit_lr == -fp.damage_lr);
    CHECK(fp.is_hitstun);
    CHECK(ip->hit_normal_damage == 12);
    CHECK(ip->attack_coll.attack_records[0].victim_gobj == mock_gobj);
    CHECK(ip->attack_coll.attack_records[0].victim_flags.is_interact_hurt);
    /* an ownerless item's damage is credited to nobody (WEAPON_PORT_
     * DEFAULT's item twin): the victim's own stats still move */
    CHECK(mock_battle.players[0].total_damage_all == 12);

    itMainDestroyItem(item_gobj);
    CHECK(gGCCommonLinks[nGCCommonLinkIDItem] == NULL);

    /* ---- the Star: a touch, not a hit ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    idle_frames(1);
    CHECK(fp.star_hitstatus == nGMHitStatusNormal);
    CHECK(fp.percent_damage == 0);
    gMPCollisionBGMCurrent = gMPCollisionBGMDefault = 0;
    /* in the 1P game the human's Star is counted (ftmain.c:2524-2529), and
     * one taken rules out the No Item bonus */
    saved_type = mock_battle.game_type;
    saved_player = gSCManagerSceneData.player;
    mock_battle.game_type = nSCBattleGameType1PGame;
    gSCManagerSceneData.player = fp.player;
    gSC1PGameBonusStarCount = 0;

    attr.type = nITTypeTouch;
    attr.damage = 0;
    attr.element = nGMHitElementNormal;
    desc.kind = nITKindStar;
    pos.x = DObjGetStruct(mock_gobj)->translate.vec.f.x + fp.damage_colls[0].offset.x;
    pos.y = DObjGetStruct(mock_gobj)->translate.vec.f.y + 150.0f + fp.damage_colls[0].offset.y;
    pos.z = 0.0f;
    item_gobj = itManagerMakeItem(NULL, &desc, &pos, &vel, 0);
    CHECK(item_gobj != NULL);
    ip = itGetStruct(item_gobj);
    CHECK(ip->type == nITTypeTouch);

    frame(0, 0, 0, 0, 0);
    CHECK(fp.percent_damage == 0);
    CHECK(fp.star_hitstatus == nGMHitStatusInvincible);
    /* ITSTAR_INVINCIBLE_TIME, less the tic ProcParams took off it in the
     * same frame if it did */
    CHECK(fp.star_invincible_tics >= ITSTAR_INVINCIBLE_TIME - 1 &&
          fp.star_invincible_tics <= ITSTAR_INVINCIBLE_TIME);
    CHECK(ip->attack_coll.attack_state == nGMAttackStateOff);
    CHECK(ip->hit_normal_damage == 1);
    CHECK(gMPCollisionBGMCurrent == nSYAudioBGMStar);
    CHECK(!fp.is_hitstun);
    CHECK(gSC1PGameBonusStarCount == 1);
    mock_battle.game_type = saved_type;
    gSCManagerSceneData.player = saved_player;

    itMainDestroyItem(item_gobj);
    gITManagerStructsAllocFree = save_free;
    ftParamTryUpdateItemMusic();
}

/* ---- item use: a fighter picking an item up, carrying it and
 * throwing it (ft/ftcommon/ftcommonget.c and ftcommonitemthrow.c, both
 * compiled unmodified, driven here through src/dc/itmain.c's
 * itMainSetFighterHold / itMainSetFighterThrow).
 *
 * Every one of these builds its item the way test_item_hits_fighter
 * above does -- a private ITStruct pool swapped into
 * gITManagerStructsAllocFree and a stack ITAttributes behind a
 * one-entry fake "file" -- because the suite has no item pack. Two
 * things the game does for itself have to be done by hand as a
 * consequence: is_allow_pickup (the game sets it when the item lands,
 * itMainSetGroundAllowPickup) and coll_data.floor_line_id (the mock map
 * gives the item none, and ftCommonGetFindItem will not pick up across
 * floor lines). --------------------------------------------------- */
static sb32 test_it_get_proc_update(GObj *g) { (void)g; return FALSE; }
static sb32 test_it_get_proc_map(GObj *g) { (void)g; return FALSE; }

/* one pickup-able item at (x, y) on the fighter's own floor line */
static GObj *test_item_make_pickup(ITAttributes *attr, ITDesc *desc,
                                   void **file_ptr, float x, float y)
{
    Vec3f pos, vel = { 0.0F, 0.0F, 0.0F };
    GObj *item_gobj;
    ITStruct *ip;

    *file_ptr = attr;
    desc->p_file = file_ptr;
    desc->o_attributes = 0;
    desc->attack_state = nGMAttackStateNew;
    desc->proc_update = test_it_get_proc_update;
    desc->proc_map = test_it_get_proc_map;

    pos.x = x; pos.y = y; pos.z = 0.0f;
    item_gobj = itManagerMakeItem(NULL, desc, &pos, &vel, 0);
    if (item_gobj == NULL)
        return NULL;
    ip = itGetStruct(item_gobj);
    ip->is_allow_pickup = TRUE;
    ip->coll_data.floor_line_id = fp.coll_data.floor_line_id;
    return item_gobj;
}

/* the attribute record every test below starts from: a light, throwable
 * item with no map collision box of its own, so the pickup rectangle is
 * the fighter's attributes alone */
static void test_item_attr_light(ITAttributes *attr)
{
    memset(attr, 0, sizeof(*attr));
    attr->data = NULL;
    attr->type = nITTypeThrow;
    attr->weight = nITWeightLight;
    attr->vel_scale = 100;              /* F_PCT_TO_DEC -> 1.0 */
    attr->size = 40.0F;
    attr->hitstatus = nGMHitStatusNormal;
}

/* 1. ftCommonGetFindItem: the rectangle, the floor line, the mask and
 * the allow flag, against Mario's own numbers out of the pack --
 * pickup_offset_light {105, 0} with pickup_range_light {378, 200}, so
 * facing right the window runs from x = -273 to x = 483. */
static void test_item_pickup_find(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    ITStruct *save_free = gITManagerStructsAllocFree;
    ITAttributes attr;
    ITDesc desc;
    void *file_ptr;
    GObj *near_gobj, *far_gobj;
    int i;

    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
        pool[i].next = &pool[i + 1];
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    idle_frames(1);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(fp.lr == +1);
    CHECK(fp.attr->item_pickup.pickup_offset_light.x == 105.0f);
    CHECK(fp.attr->item_pickup.pickup_range_light.x == 378.0f);

    memset(&desc, 0, sizeof(desc));
    desc.kind = nITKindSword;
    test_item_attr_light(&attr);

    /* inside the window, on the fighter's floor line */
    near_gobj = test_item_make_pickup(&attr, &desc, &file_ptr, 200.0f, 0.0f);
    CHECK(near_gobj != NULL);
    CHECK(itGetStruct(near_gobj)->weight == nITWeightLight);
    CHECK(ftCommonGetFindItem(mock_gobj, FTCOMMON_GET_MASK_LIGHT) == near_gobj);
    /* the mask is the weight: a light item is invisible to a heavy-only
     * search, which is what HeavyGet's own ProcUpdate runs */
    CHECK(ftCommonGetFindItem(mock_gobj, FTCOMMON_GET_MASK_HEAVY) == NULL);
    CHECK(ftCommonGetFindItem(mock_gobj,
              FTCOMMON_GET_MASK_LIGHT | FTCOMMON_GET_MASK_HEAVY) == near_gobj);

    /* past the right edge (105 + 378 = 483): out */
    DObjGetStruct(near_gobj)->translate.vec.f.x = 600.0f;
    CHECK(ftCommonGetFindItem(mock_gobj, FTCOMMON_GET_MASK_LIGHT) == NULL);
    /* past the left edge (105 - 378 = -273): out. The window is offset
     * by the facing, so it reaches much further forward than back. */
    DObjGetStruct(near_gobj)->translate.vec.f.x = -400.0f;
    CHECK(ftCommonGetFindItem(mock_gobj, FTCOMMON_GET_MASK_LIGHT) == NULL);
    /* too high (the range's y is 200 above and below the offset) */
    DObjGetStruct(near_gobj)->translate.vec.f.x = 200.0f;
    DObjGetStruct(near_gobj)->translate.vec.f.y = 400.0f;
    CHECK(ftCommonGetFindItem(mock_gobj, FTCOMMON_GET_MASK_LIGHT) == NULL);
    DObjGetStruct(near_gobj)->translate.vec.f.y = 0.0f;
    CHECK(ftCommonGetFindItem(mock_gobj, FTCOMMON_GET_MASK_LIGHT) == near_gobj);

    /* another floor line: in the rectangle and still not picked up --
     * this is what keeps a fighter from reaching through a platform */
    itGetStruct(near_gobj)->coll_data.floor_line_id =
        fp.coll_data.floor_line_id + 1;
    CHECK(ftCommonGetFindItem(mock_gobj, FTCOMMON_GET_MASK_LIGHT) == NULL);
    itGetStruct(near_gobj)->coll_data.floor_line_id = fp.coll_data.floor_line_id;

    /* an item still in the air, or inside its post-throw wait, has
     * is_allow_pickup clear */
    itGetStruct(near_gobj)->is_allow_pickup = FALSE;
    CHECK(ftCommonGetFindItem(mock_gobj, FTCOMMON_GET_MASK_LIGHT) == NULL);
    itGetStruct(near_gobj)->is_allow_pickup = TRUE;

    /* two in the window: the nearest to the window's own centre wins,
     * measured on x alone */
    far_gobj = test_item_make_pickup(&attr, &desc, &file_ptr, 450.0f, 0.0f);
    CHECK(far_gobj != NULL);
    CHECK(ftCommonGetFindItem(mock_gobj, FTCOMMON_GET_MASK_LIGHT) == near_gobj);
    DObjGetStruct(near_gobj)->translate.vec.f.x = 470.0f;
    CHECK(ftCommonGetFindItem(mock_gobj, FTCOMMON_GET_MASK_LIGHT) == far_gobj);

    itMainDestroyItem(near_gobj);
    itMainDestroyItem(far_gobj);
    gITManagerStructsAllocFree = save_free;
    printf("item pickup: the search window is the fighter's own "
           "item_pickup rectangle, offset by the facing, one floor line "
           "only, masked by weight, and the nearest wins\n");
}

/* 2. the pickup itself: the attack cascades' ftCommonGetCheckInterrupt
 * Common finds the item and sets LightGet, the animation's flag1 frame
 * runs ftCommonGetProcUpdate into itMainSetFighterHold, and the status
 * ends into Wait with the item in the hand. */
static void test_item_pickup_hold(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    ITStruct *save_free = gITManagerStructsAllocFree;
    ITAttributes attr;
    ITDesc desc;
    void *file_ptr;
    GObj *item_gobj;
    ITStruct *ip;
    DObj *item_dobj;
    int i;

    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
        pool[i].next = &pool[i + 1];
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    spawn(0.0f, 0.0f);
    mock_anim_len = 6.0f;
    idle_frames(1);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(fp.item_gobj == NULL);

    memset(&desc, 0, sizeof(desc));
    desc.kind = nITKindSword;
    test_item_attr_light(&attr);
    item_gobj = test_item_make_pickup(&attr, &desc, &file_ptr, 200.0f, 0.0f);
    CHECK(item_gobj != NULL);
    ip = itGetStruct(item_gobj);
    /* the item's own root, before the hold splices a parent above it --
     * DObjGetStruct(item_gobj) is item_gobj->obj, which the splice moves */
    item_dobj = DObjGetStruct(item_gobj);

    /* the check every ground attack setter runs before it starts an
     * attack: an item in reach turns the A press into a pickup */
    CHECK(ftCommonGetCheckInterruptCommon(mock_gobj) == TRUE);
    CHECK_STATUS(nFTCommonStatusLightGet);
    CHECK(fp.proc_damage == ftCommonLightGetProcDamage);
    CHECK(fp.item_gobj == NULL);            /* not yet -- the flag frame does it */

    /* the figatree event the pickup animation carries */
    fp.motion_vars.flags.flag1 = 1;
    ftCommonGetProcUpdate(mock_gobj);

    CHECK(fp.item_gobj == item_gobj);
    CHECK(ip->owner_gobj == mock_gobj);
    CHECK(ip->is_hold == TRUE);
    CHECK(ip->is_allow_pickup == FALSE);
    CHECK(ip->team == fp.team);
    CHECK(ip->player == fp.player);
    CHECK(ip->handicap == fp.handicap);
    CHECK(ip->pickup_wait == ITEM_PICKUP_WAIT_DEFAULT);
    /* the item's own DObj now hangs under a new parent joint that
     * itMainSetFighterHold spliced in, and that joint points at the
     * fighter's light-item hand -- this is what matrix kind 0x52 reads
     * every frame to draw the item in the hand */
    CHECK(item_gobj->obj != NULL);
    CHECK(DObjGetStruct(item_gobj) != item_dobj);
    CHECK(DObjGetStruct(item_gobj)->child == item_dobj);
    CHECK(item_dobj->parent == DObjGetStruct(item_gobj));
    CHECK(DObjGetStruct(item_gobj)->user_data.p ==
          fp.joints[fp.attr->joint_itemlight_id]);

    /* the pickup's own animation runs out and the fighter stands up
     * again, still holding it */
    for (i = 0; i < 30 && fp.status_id == nFTCommonStatusLightGet; i++)
        frame(0, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(fp.item_gobj == item_gobj);

    /* and a second pickup is refused while a hand is full */
    CHECK(ftCommonGetCheckInterruptCommon(mock_gobj) == FALSE);

    itMainDestroyItem(item_gobj);
    CHECK(gGCCommonLinks[nGCCommonLinkIDItem] == NULL);
    gITManagerStructsAllocFree = save_free;
    printf("item pickup: an item in reach turns the attack into LightGet, "
           "the flag frame hands it to itMainSetFighterHold, and it hangs "
           "off a spliced joint pointed at the fighter's hand\n");
}

/* 3. the throw: ftCommonItemThrowProcUpdate reads the throw's row out of
 * dFTCommonDataItemThrowDescs, scales it by the fighter's own
 * itemthrow_vel_scale (a pack field) and hands the item
 * to itMainSetFighterThrow. */
static void test_item_throw(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    ITStruct *save_free = gITManagerStructsAllocFree;
    ITAttributes attr;
    ITDesc desc;
    void *file_ptr;
    GObj *item_gobj;
    ITStruct *ip;
    const FTItemThrow *row;
    float base, want_x, want_y;
    int i;

    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
        pool[i].next = &pool[i + 1];
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    spawn(0.0f, 0.0f);
    mock_anim_len = 6.0f;
    idle_frames(1);

    memset(&desc, 0, sizeof(desc));
    desc.kind = nITKindSword;
    test_item_attr_light(&attr);
    item_gobj = test_item_make_pickup(&attr, &desc, &file_ptr, 200.0f, 0.0f);
    CHECK(item_gobj != NULL);
    ip = itGetStruct(item_gobj);
    CHECK(ip->vel_scale == 1.0f);           /* attr.vel_scale 100% */

    itMainSetFighterHold(item_gobj, mock_gobj);
    CHECK(fp.item_gobj == item_gobj);

    /* the forward throw, as ftCommonAttack1CheckInterruptCommon's item
     * arm would set it */
    ftCommonItemThrowSetStatus(mock_gobj, nFTCommonStatusLightThrowF);
    CHECK_STATUS(nFTCommonStatusLightThrowF);
    /* InitStatusVars' defaults: angle 361 means "use the table's" */
    CHECK(fp.status_vars.common.itemthrow.throw_angle == 361);
    CHECK(fp.status_vars.common.itemthrow.throw_vel == 1.0f);

    /* the release frame the throw animation carries */
    fp.motion_vars.item_throw.is_throw_item = TRUE;
    ftCommonItemThrowProcUpdate(mock_gobj);

    row = &dFTCommonDataItemThrowDescs[nFTCommonStatusLightThrowF -
                                       nFTCommonStatusLightThrowStart];
    base = 0.01f * (row->vel_scale * 1.0f * fp.attr->itemthrow_vel_scale);
    want_x = __cosf(F_CLC_DTOR32(row->angle)) * base * fp.lr * ip->vel_scale;
    want_y = __sinf(F_CLC_DTOR32(row->angle)) * base * ip->vel_scale;

    CHECK(fp.item_gobj == NULL);
    CHECK(ip->is_hold == FALSE);
    CHECK(ip->is_thrown == TRUE);
    CHECK(ip->times_thrown == 1);
    CHECK(ip->owner_gobj == mock_gobj);      /* still credited to the thrower */
    CHECK(fabsf(ip->physics.vel_air.x - want_x) < 0.01f);
    CHECK(fabsf(ip->physics.vel_air.y - want_y) < 0.01f);
    CHECK(ip->physics.vel_air.x > 0.0f);     /* facing right */

    itMainDestroyItem(item_gobj);
    gITManagerStructsAllocFree = save_free;
    printf("item throw: LightThrowF releases on the flag frame at the "
           "table's angle and velocity, scaled by the fighter's "
           "itemthrow_vel_scale and the item's own vel_scale\n");
}

/* 4. the three items a pickup eats rather than holds: the Maxim Tomato
 * and the Heart Container heal and vanish, the Hammer arms hammer_tics
 * and takes over the music. All three run through
 * ftCommonLightGetProcDamage, which LightGet installs as proc_damage. */
static void test_item_consume(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    ITStruct *save_free = gITManagerStructsAllocFree;
    ITAttributes attr;
    ITDesc desc;
    void *file_ptr;
    GObj *item_gobj;
    int i;

    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
        pool[i].next = &pool[i + 1];
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    memset(&desc, 0, sizeof(desc));
    test_item_attr_light(&attr);
    attr.type = nITTypeConsume;

    /* --- the Maxim Tomato --- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    idle_frames(1);
    fp.percent_damage = 50.0f;
    desc.kind = nITKindTomato;
    item_gobj = test_item_make_pickup(&attr, &desc, &file_ptr, 200.0f, 0.0f);
    CHECK(item_gobj != NULL);
    itMainSetFighterHold(item_gobj, mock_gobj);
    CHECK(fp.item_gobj == item_gobj);

    ftCommonLightGetProcDamage(mock_gobj);
    CHECK(fp.damage_heal == ITTOMATO_DAMAGE_HEAL);
    CHECK(fp.item_gobj == NULL);            /* eaten: itMainDestroyItem */
    CHECK(gGCCommonLinks[nGCCommonLinkIDItem] == NULL);

    /* --- the Heart Container: the same path, the bigger heal --- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    idle_frames(1);
    fp.percent_damage = 150.0f;
    desc.kind = nITKindHeart;
    item_gobj = test_item_make_pickup(&attr, &desc, &file_ptr, 200.0f, 0.0f);
    CHECK(item_gobj != NULL);
    itMainSetFighterHold(item_gobj, mock_gobj);

    ftCommonLightGetProcDamage(mock_gobj);
    CHECK(fp.damage_heal == ITHEART_DAMAGE_HEAL);
    CHECK(fp.item_gobj == NULL);

    /* --- the Hammer: not destroyed, held for ITHAMMER_TIME with its own
     * music over the stage's --- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    idle_frames(1);
    gMPCollisionBGMCurrent = gMPCollisionBGMDefault = 0;
    desc.kind = nITKindHammer;
    item_gobj = test_item_make_pickup(&attr, &desc, &file_ptr, 200.0f, 0.0f);
    CHECK(item_gobj != NULL);
    itMainSetFighterHold(item_gobj, mock_gobj);

    CHECK(fp.hammer_tics == 0);
    ftCommonLightGetProcDamage(mock_gobj);
    CHECK(fp.hammer_tics == ITHAMMER_TIME);
    CHECK(fp.item_gobj == item_gobj);        /* kept, unlike the food */
    CHECK(gMPCollisionBGMCurrent == nSYAudioBGMHammer);
    /* and the Hammer's own states are reachable: every kneebend, walk,
     * turn, fall and aerial check asks this first, and it reads the
     * item's KIND, so it is true from the moment the hand closes --
     * hammer_tics is the timer, not the gate */
    CHECK(ftHammerCheckHoldHammer(mock_gobj) == TRUE);
    /* and the Wait setter asks it too, so a holder who
     * stops stands in HammerWait with the Hammer's moveset */
    ftCommonWaitSetStatus(mock_gobj);
    CHECK_STATUS(nFTCommonStatusHammerWait);

    fp.hammer_tics = 0;
    itMainDestroyItem(item_gobj);
    gITManagerStructsAllocFree = save_free;
    ftParamTryUpdateItemMusic();

    /* without one, Wait is the plain one, special-
     * interruptible (Link's boomerang catch), with the idle player tag
     * armed; the ledge hang arms it too */
    spawn(0.0f, 0.0f);
    fp.is_special_interrupt = FALSE;
    fp.playertag_wait = 0;
    ftCommonWaitSetStatus(mock_gobj);
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(fp.is_special_interrupt == TRUE);
    CHECK(fp.playertag_wait == 120);
    fp.playertag_wait = 0;
    ftCommonCliffWaitSetStatus(mock_gobj);
    CHECK(fp.playertag_wait == 120);
    spawn(0.0f, 0.0f);
    printf("item consume: the Tomato and the Heart heal and vanish, the "
           "Hammer stays in the hand for %d tics with its own music\n",
           ITHAMMER_TIME);
}

/* A jab lands: the victim's percent, both fighters' hitlag, the attack
 * record that keeps the hitbox from landing twice, the battle stats. */
static void test_hit_lands(void)
{
    FTAttackColl *ac;
    int i;

    spawn(0.0f, 0.0f);
    spawn_second(100.0f, 0.0f, -1);
    mock_anim_len = 12.0f;
    CHECK(fp2.status_id == nFTCommonStatusWait);
    CHECK(fp2.damage_colls[0].joint == fp2.joints[4]);
    CHECK_EQF(fp2.damage_colls[0].size.x, 50.0f);   /* halved at spawn */
    CHECK(fp2.damage_colls[2].hitstatus == nGMHitStatusNone);

    frame(0, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttack11);
    CHECK(fp2.percent_damage == 0);
    /* the script's Wait(2) runs out on the next frame: the hitbox is
     * raised, placed, searched and paid out in that one frame (priority
     * 5 script, 4 position, 1 search, 0 params) */
    for (i = 0; i < 4 && fp2.percent_damage == 0; i++)
        frame(0, 0, 0, 0, 0);
    CHECK(i == 1);
    ac = &fp.attack_colls[1];
    CHECK(ac->attack_state == nGMAttackStateTransfer);
    CHECK(fp2.percent_damage == 14);
    CHECK(fp2.damage_player == 0);
    CHECK(fp2.damage_lr == -1);         /* attacker to its left */
    CHECK(fp2.damage_joint_id == 4);
    CHECK(fp2.damage_index == 1);       /* the hurtbox's placement */
    CHECK(fp2.damage_angle == 361);
    CHECK(fp2.damage_element == 0);
    /* hitlag: (14 / 3 + 5) = 9 tics on both, the knockback held */
    CHECK(fp2.hitlag_tics == 9);
    CHECK(fp.hitlag_tics == 9);
    CHECK(fp2.is_knockback_paused);
    CHECK(fp2.shuffle_tics == (s32)(9 * FTCOMMON_DAMAGE_SHUFFLE_MUL));  /* 11 */
    /* ftdisplaymain.c:1205-1216: the victim is drawn jogged
     * by this tic's shuffle offset, in world space ahead of the camera's
     * view (objmodel.c dc_model_view_for); the attacker, in hitlag but
     * not shuffling, is drawn under the camera's own view */
    {
        extern Vec2f dFTDisplayMainShufflePositions[/* */][4];
        static const float view[16] = {
            0.0f, 0.0f, -1.0f, 10.0f,
            0.0f, 1.0f,  0.0f, -20.0f,
            1.0f, 0.0f,  0.0f, -3000.0f,
            0.0f, 0.0f,  0.0f, 1.0f
        };
        float save_view[16], save_proj[16];
        const Vec2f *jog = &dFTDisplayMainShufflePositions[fp2.is_shuffle_electric][fp2.shuffle_frame_index];
        const float *got;
        int r, c;

        memcpy(save_view, gcGetViewF(), sizeof(save_view));
        memcpy(save_proj, gcGetProjF(), sizeof(save_proj));
        gcSetCameraMatrixF(view, save_proj);

        CHECK(fp.shuffle_tics == 0);
        CHECK(dc_model_view_for(mock_gobj) == gcGetViewF());
        CHECK((jog->x != 0.0f) || (jog->y != 0.0f));
        got = dc_model_view_for(mock_gobj2);
        CHECK(got != gcGetViewF());
        for (r = 0; r < 4; r++)
        {
            for (c = 0; c < 3; c++)
            {
                CHECK_EQF(got[r * 4 + c], view[r * 4 + c]);
            }
            CHECK_NEAR(got[r * 4 + 3], view[r * 4 + 0] * jog->x +
                       view[r * 4 + 1] * jog->y + view[r * 4 + 3], 1e-3f);
        }
        gcSetCameraMatrixF(save_view, save_proj);
    }
    /* the numbers ftMainProcParams cleared after paying out */
    CHECK(fp2.damage_queue == 0);
    CHECK(fp.attack_damage == 0);
    CHECK_EQF(fp2.damage_knockback, 0.0f);
    /* the record: this victim, hurt, by hitbox 1 */
    CHECK(ac->attack_records[0].victim_gobj == mock_gobj2);
    CHECK(ac->attack_records[0].victim_flags.is_interact_hurt);
    /* the battle stats (ft/ftparam.c:1744) */
    CHECK(mock_battle.players[0].total_damage_given == 14);
    CHECK(mock_battle.players[1].total_damage_all == 14);
    CHECK(mock_battle.players[1].stock_damage_all == 14);
    CHECK(mock_battle.players[1].total_damage_players[0] == 14);
    /* launched: a real Damage status, not Wait, and marked
     * hitstunned; which one depends on the knockback this hit computed,
     * not asserted here */
    CHECK(fp2.status_id != nFTCommonStatusWait);
    CHECK(fp2.is_hitstun);
    /* the same hitbox does not land again while it lasts, resize or
     * no resize (the script re-damages it on frame 5) */
    for (i = 0; i < 12; i++)
        frame(0, 0, 0, 0, 0);
    CHECK(fp2.percent_damage == 14);
    /* the hitlag ran out and the jab went on to end */
    CHECK(fp.hitlag_tics == 0);
    CHECK(fp2.hitlag_tics == 0);
    /* a second jab, a fresh hitbox: it lands again -- once the jab's
     * follow-up window (attack1_followup_frames, 24) has closed, so A
     * starts Attack11 again rather than Attack12; and staled by the
     * queue the first one went into (ft/ftparam.c:1608): 14 x 0.75,
     * rounded the game's way, is 11 */
    run_until(0, 0, 40, is_wait);
    CHECK_STATUS(nFTCommonStatusWait);
    idle_frames(30);
    CHECK_EQF(fp.attack1_followup_frames, 0.0f);
    /* the first hit's knockback sent fp2 off; put it back in
     * range the way a fresh dummy placement would, through the port's
     * own safe-teleport (ftMainRespawn: position, velocities, and the
     * collision line reacquired via ftManagerProjectFighterFloor) rather
     * than poking translate directly, which left coll_data.floor_line_id
     * stale and crashed mpcollision.c's line lookup. This beat tests the
     * stale queue on a second landed hit, not where the first one sent
     * the victim. */
    ftMainRespawn(mock_gobj2, 100.0f, 0.0f);
    frame(0, 0, N64_A, N64_A, 0);
    CHECK_STATUS(nFTCommonStatusAttack11);
    for (i = 0; i < 3; i++)
        frame(0, 0, 0, 0, 0);
    CHECK(fp.attack_colls[1].damage == 11);
    CHECK(fp2.percent_damage == 25);
    CHECK(mock_battle.players[0].total_damage_given == 25);
}

/* No hit: out of the hit-detect range; the hurtbox intangible; and the
 * victim behind the attacker, where the hitbox is not. */
static void test_hit_misses(void)
{
    int i;

    /* out of range: the range box (900 wide) plus the hitbox's radius */
    spawn(0.0f, 0.0f);
    spawn_second(1200.0f, 0.0f, -1);
    mock_anim_len = 12.0f;
    frame(0, 0, N64_A, N64_A, 0);
    for (i = 0; i < 4; i++)
        frame(0, 0, 0, 0, 0);
    CHECK(fp2.percent_damage == 0);
    CHECK(fp.hitlag_tics == 0);
    CHECK(fp.attack_colls[1].attack_records[0].victim_gobj == NULL);

    /* intangible: in range, in the box, and passed over */
    spawn(0.0f, 0.0f);
    spawn_second(100.0f, 0.0f, -1);
    mock_anim_len = 12.0f;
    fp2.hitstatus = nGMHitStatusIntangible;
    frame(0, 0, N64_A, N64_A, 0);
    for (i = 0; i < 4; i++)
        frame(0, 0, 0, 0, 0);
    CHECK(fp2.percent_damage == 0);
    CHECK(fp.attack_colls[1].attack_records[0].victim_gobj == NULL);

    /* behind: the hitbox is 33 units ahead of the attacker and 100
     * across, the hurtbox 50 across; at -250 they do not meet */
    spawn(0.0f, 0.0f);
    spawn_second(-250.0f, 0.0f, 1);
    mock_anim_len = 12.0f;
    frame(0, 0, N64_A, N64_A, 0);
    for (i = 0; i < 4; i++)
        frame(0, 0, 0, 0, 0);
    CHECK(fp2.percent_damage == 0);
    /* while the same jab from the other side lands */
    spawn_desc(0.0f, 0.0f, -1, TRUE);
    ftParamUnlockPlayerControl(mock_gobj);
    spawn_second(-100.0f, 0.0f, 1);
    mock_anim_len = 12.0f;
    frame(0, 0, N64_A, N64_A, 0);
    for (i = 0; i < 4; i++)
        frame(0, 0, 0, 0, 0);
    CHECK(fp2.percent_damage == 14);
    /* damage_lr's exact sign here (a
     * CHECK(fp2.damage_lr == 1) tracked the stub-era search code's
     * output at frame+4, not a verified knockback direction, so it was
     * never checked against real physics. What matters, that the mirrored jab
     * connects for the right damage, is the CHECK above. */
}

/* The hit sound and the FGM shim: nothing to assert on the host but
 * that the path runs; the frame above already went through
 * ftMainPlayHitSFX. What can be checked is the stale-move queue the
 * hit fed (ft/ftparam.c:1639): the second jab in a row is staled. */
static void test_hit_stales(void)
{
    int i;

    spawn(0.0f, 0.0f);
    spawn_second(100.0f, 0.0f, -1);
    mock_anim_len = 12.0f;
    frame(0, 0, N64_A, N64_A, 0);
    for (i = 0; i < 3; i++)
        frame(0, 0, 0, 0, 0);
    CHECK(fp2.percent_damage == 14);
    CHECK(mock_battle.players[0].stale_info[0].attack_id == nFTMotionAttackIDAttack11);
    CHECK(mock_battle.players[0].stale_id == 1);
    /* the same jab instance is not staled against itself ... */
    CHECK_EQF(ftParamGetStale(0, nFTMotionAttackIDAttack11, fp.motion_count), 1.0f);
    /* ... the next one is, by the queue's first factor */
    CHECK_EQF(ftParamGetStale(0, nFTMotionAttackIDAttack11, fp.motion_count + 1), 0.75f);
}
