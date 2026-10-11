/* hosttest/common.c -- part of hosttest_ft.c: the scaffolding every test stands on: the scripted animation, the
 * verbatim decomp reference bodies, the hand-built stage, the mock
 * model, the stand-ins for what the battle scene reaches for, the
 * CHECK macros, and spawn()/frame() -- the fighter under test.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* ---- the animation, as the test scripts it ---------------------------
 *
 * Everything else about the fighter is real: it is a GObj with a DObj
 * tree, ftMainSetStatus hangs a figatree on each joint through
 * gcAddDObjAnimJoint, and sys/objanim.c evaluates the tracks. The one
 * piece replaced is ft/ftanim.c's parser, which reads the animation
 * *data* -- and this test has no ROM to read it from. So it links
 * without ft/ftanim.c (see the Makefile) and stands in for
 * ftAnimParseDObjFigatree here, keeping the clock exactly as the real
 * one keeps it (ft/ftanim.c:65-125) and standing in for the script with
 * two knobs: how long the animation runs, and what it writes into
 * TransN.
 *
 * That the real parser agrees with the game is not this test's job --
 * tools/check/figatree_check.py runs it against all 1775 of the ROM's
 * animations and demands bit-exact agreement. */
static float mock_anim_len = 1e9f;      /* frames before the End command */
static float mock_transn[3];            /* what the animation writes ... */
static float mock_transn_rot_z;         /* ... into the TransN joint */

/* The fighter under test. It comes out of the manager's
 * pool (ftManagerMakeFighter), so the tests reach it through a pointer;
 * `fp.` is spelled as it always was, through the macro defined below
 * the reference section (whose verbatim bodies take a `fp` of their
 * own). */
static FTStruct *fpp;

#ifdef SSB_NO_DRAW      /* the BAKER build links the real ft/ftanim.c */
void ftAnimParseDObjFigatree(DObj *root_dobj)
{
    if (root_dobj->anim_wait == AOBJ_ANIM_NULL)
    {
        return;
    }
    /* ft/ftanim.c:74-88 */
    if (root_dobj->anim_wait == AOBJ_ANIM_CHANGED)
    {
        root_dobj->anim_wait = -root_dobj->anim_frame;
    }
    else
    {
        root_dobj->anim_wait -= root_dobj->anim_speed;
        root_dobj->anim_frame += root_dobj->anim_speed;
        root_dobj->parent_gobj->anim_frame = root_dobj->anim_frame;
    }
    /* the End command (ft/ftanim.c:104-123): the leftover wait becomes
     * the GObj's anim_frame, which is how a status knows to move on */
    if (root_dobj->anim_frame >= mock_anim_len)
    {
        root_dobj->anim_frame = 0.0F;
        root_dobj->parent_gobj->anim_frame = 0.0F;
        root_dobj->anim_wait = AOBJ_ANIM_END;

        return;
    }
    root_dobj->anim_wait = 1.0F;        /* one frame's worth of Block */

    if (root_dobj == ftGetStruct(root_dobj->parent_gobj)->joints[nFTPartsJointTransN])
    {
        root_dobj->translate.vec.f.x = mock_transn[0];
        root_dobj->translate.vec.f.y = mock_transn[1];
        root_dobj->translate.vec.f.z = mock_transn[2];
        root_dobj->rotate.vec.f.z = mock_transn_rot_z;
    }
}
#endif

/* ---- reference: verbatim decomp bodies ------------------------------ */

typedef struct
{
    struct { struct { float x, y; } vel_ground, vel_air; } physics;
    struct { struct { struct { int x, y; } stick_range; } pl; } input;
} RefFT;

/* ftphysics.c 0x800D8978, verbatim */
static void ref_SetGroundVelFriction(RefFT *fp, float friction)
{
    if (fp->physics.vel_ground.x < 0.0F)
    {
        fp->physics.vel_ground.x += friction;

        if (fp->physics.vel_ground.x > 0.0F)
        {
            fp->physics.vel_ground.x = 0.0F;
        }
    }
    else
    {
        fp->physics.vel_ground.x -= friction;

        if (fp->physics.vel_ground.x < 0.0F)
        {
            fp->physics.vel_ground.x = 0.0F;
        }
    }
}

/* ftphysics.c 0x800D8A70, verbatim */
static void ref_SetGroundVelAbsStickRange(RefFT *fp, float vel,
                                          float friction)
{
    float v = abs(fp->input.pl.stick_range.x) * vel;

    if (fp->physics.vel_ground.x < v)
    {
        fp->physics.vel_ground.x = v;
    }
    else
    {
        fp->physics.vel_ground.x -= friction;

        if (fp->physics.vel_ground.x < v)
        {
            fp->physics.vel_ground.x = v;
        }
    }
}

/* ftphysics.c 0x800D8D68, verbatim */
static void ref_ApplyGravityClampTVel(RefFT *fp, float gravity, float tvel)
{
    fp->physics.vel_air.y -= gravity;

    if (fp->physics.vel_air.y < -tvel)
    {
        fp->physics.vel_air.y = -tvel;
    }
}

/* ftphysics.c 0x800D8EDC, verbatim */
static int ref_CheckClampAirVelXDec(RefFT *fp, float clamp)
{
    if (fabsf(fp->physics.vel_air.x) > clamp)
    {
        fp->physics.vel_air.x += ((fp->physics.vel_air.x >= 0.0F) ? -1.0F
                                                                  : 1.0F);
        if (fabsf(fp->physics.vel_air.x) < clamp)
        {
            if (fp->physics.vel_air.x >= 0.0F)
            {
                fp->physics.vel_air.x = clamp;
            }
            else fp->physics.vel_air.x = -clamp;
        }
        return 1;
    }
    else return 0;
}

/* ftphysics.c 0x800D8FC8, verbatim */
static void ref_ClampAirVelXStickRange(RefFT *fp, int stick_range_min,
                                       float vel, float clamp)
{
    if (abs(fp->input.pl.stick_range.x) >= stick_range_min)
    {
        fp->physics.vel_air.x += (fp->input.pl.stick_range.x * vel);

        if (fp->physics.vel_air.x < -clamp)
        {
            fp->physics.vel_air.x = -clamp;
        }
        else if (fp->physics.vel_air.x > clamp)
        {
            fp->physics.vel_air.x = clamp;
        }
    }
}

/* ftphysics.c 0x800D9074, verbatim (attr->air_friction inlined) */
static void ref_ApplyAirVelXFriction(RefFT *fp, float air_friction)
{
    if (fp->physics.vel_air.x < 0.0F)
    {
        fp->physics.vel_air.x += air_friction;

        if (fp->physics.vel_air.x >= 0.0F)
        {
            fp->physics.vel_air.x = 0.0F;
        }
    }
    else
    {
        fp->physics.vel_air.x -= air_friction;

        if (fp->physics.vel_air.x <= 0.0F)
        {
            fp->physics.vel_air.x = 0.0F;
        }
    }
}

/* ftcommonjump.c 0x8013F6A0, verbatim -- s32 pointees included: the game
 * truncates the jump force to whole units */
static void ref_JumpGetJumpForceButton(int stick_range_x, int *jump_vel_x,
                                       int *jump_vel_y, int is_shorthop)
{
    float sqrt_vel_x;
    float vel_y;
    float vel_x;

    vel_x = abs(stick_range_x);

    sqrt_vel_x = sqrtf(1.0F - SQUARE(vel_x / F_CONTROLLER_RANGE_MAX));

    if (is_shorthop == 0)
    {
        vel_y = (17.0F * sqrt_vel_x) + 63.0F;

        if ((SQUARE(vel_x) + SQUARE(vel_y)) > SQUARE(F_CONTROLLER_RANGE_MAX))
        {
            vel_y = sqrtf(SQUARE(F_CONTROLLER_RANGE_MAX) - SQUARE(vel_x));
        }
        if (vel_y < 63.0F)
        {
            vel_y = 63.0F;
        }
    }
    else
    {
        vel_y = (9.0F * sqrt_vel_x) + 36.0F;

        if ((SQUARE(vel_x) + SQUARE(vel_y)) > SQUARE(F_CONTROLLER_RANGE_MAX))
        {
            vel_y = sqrtf(SQUARE(F_CONTROLLER_RANGE_MAX) - SQUARE(vel_x));
        }
        if (vel_y < 36.0F)
        {
            vel_y = 36.0F;
        }
    }
    if (vel_y > 77.0F)
    {
        vel_y = 77.0F;
    }
    *jump_vel_x = (stick_range_x >= 0) ? vel_x : -vel_x;

    *jump_vel_y = vel_y;
}


#define fp (*fpp)
/* the fighter's position: the TopN joint's translate, as in the game */
#define fp_pos (DObjGetStruct(mock_gobj)->translate.vec.f)

/* ---- the port's private physics, reached through scenario state ----- */

/* Mario's FTAttributes (ft/ftdata.c dMarioMain_attr, and the ROM values
 * the pack exporter asserts against it). The collision diamond and the
 * ledge-grab box are src/relocData/203_MarioMain.c:274-275. */
/* FTAnimDesc.is_use_transn_joint (ft/fttypes.h:55) as the ROM's motion
 * table spells it: bit 30. ft/ftdef.h:28-29 name 0x80000000 TRANSN and
 * 0x40000000 XROTN, but the bit-field the code tests puts
 * is_use_transn_joint at bit 30, and the cliff and roll animations carry
 * 0x40000000 in ftdata.c; the port follows the code, and docker/patches/
 * 0004 makes the bit-field read that bit on this ABI too. */
#define MOCK_ANIMFLAG_TRANSN 0x40000000

/* the same values as the game's FTAttributes, filled the way the
 * manager fills a fighter's (ftManagerSetupAttributes) */
static FTAttributes kMarioAttr;

static const FPackAttr kMario = {
    /* the hit half: a hurtbox on each of the mock's first
     * two entries (joints[4] and [5], the hip and the torso; Mario's own
     * sit on joints the three-entry mock does not have), his hit-detect
     * range, the jab and
     * tilt is_have bits (31, 30, 28, 27, 26) plus the Neutral-B's (17),
     * the Up-B's (15), the Down-B's (13), the grab's (11), the five aerials'
     * (22, 21, 20, 19, 18) and the three smashes'
     * (25, 24, 23) and the dash attack's (29).
     * The ROM's Mario has all twenty-two; this word is the subset
     * the port can actually reach, so that a cascade check firing here
     * means the move exists rather than that the attribute was
     * optimistic. Only two bits are still clear, and each for a reason:
     * every is_have_specialair* (16, 14, 12) is a per-fighter move the
     * tests turn on by hand for the fighter under test, and voice (10)
     * is the FGM half. */
    .is_metallic = 0,
    .is_have = 0xFFFEA800u,
    .hit_detect_range = { 900.0f, 450.0f, 900.0f },
    .animlock = { 0, 0 },
    /* the joint tables: every one of the mock's
     * three entries instantiated, and the three hidden parts every
     * fighter has -- XRotN under TopN, TransN under TopN, YRotN under
     * XRotN, the rows as Mario's (relocData/203_MarioMain.c). The slot
     * tables are in the blob below. */
    .setup_parts = { 0xE0000000u, 0u },
    .hiddenpart_count = 3,
    .hiddenparts = { { 2, 0, 1, 3 }, { 1, 0, 0, 3 }, { 3, 2, 1, 0 } },
    .damage_coll_descs = {
        { 4, 1, 1, { 0.0f, 0.0f, 0.0f }, { 100.0f, 100.0f, 100.0f } },
        { 5, 0, 0, { 0.0f, 0.0f, 0.0f }, { 60.0f, 60.0f, 60.0f } },
        { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },
        { -1 },
    },
    .size = 1.12f,
    .walkslow_anim_length = 90.0f,
    .walkmiddle_anim_length = 60.0f,
    .walkfast_anim_length = 40.0f,
    .rebound_anim_length = 16.0f,
    .walk_speed_mul = 0.3f,
    .traction = 1.5f,
    .dash_speed = 54.0f,
    .dash_decel = 2.8f,
    .run_speed = 44.0f,
    .kneebend_anim_length = 3.0f,
    .jump_vel_x = 0.35f,
    .jump_height_mul = 0.7f,
    .jump_height_base = 26.0f,
    .jumpaerial_vel_x = 0.35f,
    .jumpaerial_height = 0.9f,
    .air_accel = 0.025f,
    .air_speed_max_x = 30.0f,
    .air_friction = 0.2f,
    .gravity = 2.4f,
    .tvel_base = 44.0f,
    .tvel_fast = 70.0f,
    /* relocData/203_MarioMain.c:267-273 */
    .shield_size = 260.0f,
    .jostle_width = 112.5f,
    .jostle_x = 0.0f,
    .cam_offset_y = 250.0f,
    .closeup_camera_zoom = 1600.0f,
    .camera_zoom = 1.0f,
    .camera_zoom_base = 500.0f,
    .weight = 1.0f,
    .attack1_followup_frames = 24.0f,
    .dash_to_run = 14.0f,
    .map_coll_top = 320.0f,
    .map_coll_center = 190.0f,
    .map_coll_bottom = 0.0f,
    .map_coll_width = 150.0f,
    .cliffcatch_coll_x = 400.0f,
    .cliffcatch_coll_y = 360.0f,
    .jumps_max = 2,
    /* src/relocData/203_MarioMain.c:331-332: cliff_status_ga plus the
     * unused_0x2CC word the game reads for EscapeSlow. Only the ledge
     * attack is airborne. */
    .cliff_status_ga = { nMPKineticsGround, nMPKineticsAir,
                         nMPKineticsGround, nMPKineticsGround,
                         nMPKineticsGround, nMPKineticsGround },
    /* fttypes.h:969: Mario's own thrown_status table,
     * copied verbatim from relocData/203_MarioMain.c's real ROM data --
     * every one of the 27 rows (one per nFTKind*) is the same regardless
     * of the caught fighter's own fkind: an -1/ThrownCommon forward
     * throw, a MarioBStart/MarioB back throw. Left all-zero (a NULL
     * catch_gobj target and a bogus status 0) would leave the field
     * unfilled -- which is exactly the gap that let a real completed grab dereference
     * garbage and abort the target the first time one played out on a
     * disc probe. */
#define MARIO_THROWN_ROW { { -1, nFTCommonStatusThrownCommon }, \
                           { nFTCommonStatusThrownMarioBStart, \
                             nFTCommonStatusThrownMarioB } }
    .thrown_status = {
        MARIO_THROWN_ROW, MARIO_THROWN_ROW, MARIO_THROWN_ROW,
        MARIO_THROWN_ROW, MARIO_THROWN_ROW, MARIO_THROWN_ROW,
        MARIO_THROWN_ROW, MARIO_THROWN_ROW, MARIO_THROWN_ROW,
        MARIO_THROWN_ROW, MARIO_THROWN_ROW, MARIO_THROWN_ROW,
        MARIO_THROWN_ROW, MARIO_THROWN_ROW, MARIO_THROWN_ROW,
        MARIO_THROWN_ROW, MARIO_THROWN_ROW, MARIO_THROWN_ROW,
        MARIO_THROWN_ROW, MARIO_THROWN_ROW, MARIO_THROWN_ROW,
        MARIO_THROWN_ROW, MARIO_THROWN_ROW, MARIO_THROWN_ROW,
        MARIO_THROWN_ROW, MARIO_THROWN_ROW, MARIO_THROWN_ROW,
    },
#undef MARIO_THROWN_ROW
    /* fttypes.h:917-920 and :968/:970, the item half,
     * Mario's own numbers from relocData/203_MarioMain.c:280-283 and
     * :348/:350.
     *
     * The two hand joints are the exception: Mario's are 28 and 17, and
     * the mock's tree is three entries deep, so a motion script asking
     * for joint -2 (ftParamGetJointID, "wherever the light item is")
     * would index a NULL slot. They stay TopN here, the value every
     * other mock joint field takes, and the tests that actually put an
     * item in a hand set the joint they want (the pattern
     * test_donkey_throw_wait already uses for joint_itemheavy_id). That
     * the pack really carries 28 and 17 is checked by
     * test_pack_attr_item_half below, which runs the copy on a
     * hand-built FPackAttr, and by the exporter's own per-fighter
     * cross-check against relocData. */
    .item_pickup = { 105.0f, 0.0f, 378.0f, 200.0f,
                     75.0f, 0.0f, 150.0f, 150.0f },
    .itemthrow_vel_scale = 100,
    .itemthrow_damage_scale = 100,
    .heavyget_sfx = nSYAudioVoiceMarioHeavyGet,
    .dead_fgm_ids = { nSYAudioVoiceMarioDead, nSYAudioFGMMarioDeadSlam },
    .deadup_sfx = nSYAudioVoiceMarioDeadUp,
    .damage_sfx = nSYAudioVoiceMarioDamage,
    .smash_sfx = { nSYAudioVoiceMarioSmash1, nSYAudioVoiceMarioSmash2,
                   nSYAudioVoiceMarioSmash3 },
    .joint_itemheavy_id = nFTPartsJointTopN,
    .joint_itemlight_id = nFTPartsJointTopN,
};

/* motion table: every row points at the pack's one animation, so a
 * status change always attaches something. The ledge rows carry the
 * TransN root-motion flag, as ft/ftdata.c:213 does for CliffCatch. */
static FPackMotion mock_motions[256];

/* ---- a stage in MPGeometryData's own layout (mp/mptypes.h:16-80) ----
 *
 * Two ordering rules the game's own data obeys and its collision walk
 * relies on:
 *
 *  - Line ids tile 0..N-1 in the order func_ovl2_800FB04C
 *    (mp/mpcollision.c:3436-3492) walks them: group by group, and
 *    within a group floor, ceiling, right wall, left wall.
 *  - Within the floor group the lines run highest first. The segment
 *    sweep (mpCollisionCheckFloorLineCollisionSame,
 *    mp/mpcollision.c: `goto l_break`) abandons the whole group at the
 *    first line whose top (coll_pos_next) is below the swept span, so a
 *    low line listed early hides every line after it.
 *
 * The ceiling, the walls and the slope sit off to the side of the
 * jumping area so the airborne tests are not fighting them.
 *
 *  ceil (-1800,900)--(-1000,900)
 *                          platform (-500,300)---(500,300) PASS
 *  rwall  ......................................................
 *  at      deck  (-2000,0)-------------------------(2000,0) CLIFF
 *  x=-3000                             slope (3000,0)--(6000,1500)
 *  y=-4000..1200                          lwall at x=9000, y=0..1200
 */
static const MPVertexData kVpos[] = {
    /* floors, highest first: slope, platform, deck */
    /*  0 */ { {  3000,     0 }, 0 },
    /*  1 */ { {  6000,  1500 }, 0 },
    /*  2 */ { {  -500,   300 }, MAP_VERTEX_COLL_PASS },
    /*  3 */ { {   500,   300 }, MAP_VERTEX_COLL_PASS },
    /*  4 */ { { -2000,     0 }, MAP_VERTEX_COLL_CLIFF },
    /*  5 */ { {  2000,     0 }, MAP_VERTEX_COLL_CLIFF },
    /*  6 */ { { -1800,   900 }, 0 },
    /*  7 */ { { -1000,   900 }, 0 },
    /*  8 */ { { -3000, -4000 }, 0 },
    /*  9 */ { { -3000,  1200 }, 0 },
    /* 10 */ { {  9000,     0 }, 0 },
    /* 11 */ { {  9000,  1200 }, 0 },
};

static const u16 kVertexIDs[] = {
    0, 1,       /* line 0: slope */
    2, 3,       /* line 1: pass-through platform */
    4, 5,       /* line 2: deck */
    6, 7,       /* line 3: ceiling */
    8, 9,       /* line 4: right wall (blocks leftward travel) */
    10, 11,     /* line 5: left wall (blocks rightward travel) */
};

static const MPVertexLinks kLinks[] = {
    { 0, 2 }, { 2, 2 }, { 4, 2 }, { 6, 2 }, { 8, 2 }, { 10, 2 },
};

/* one yakumono group: 3 floors, 1 ceiling, 1 rwall, 1 lwall */
static const MPLineInfo kLineInfo[] = {
    { 0, { { 0, 3 }, { 3, 1 }, { 4, 1 }, { 5, 1 } } },
};

static const MPMapObjData kMapObjs[] = {
    { nMPMapObjKindBattlePlayer1, {    0, 1000 } },
    /* player 2's start, for the scene driver at the bottom of this file:
     * scVSBattleStartBattle asks mpCollisionGetPlayerMapObjPosition for
     * every playing slot, and that function leaves pos untouched when it
     * finds no map object of that kind */
    { nMPMapObjKindBattlePlayer2, {  800, 1000 } },
    /* players 3 and 4, for the three-fighter test at the bottom of this
     * file: the same, spread further along the deck */
    { nMPMapObjKindBattlePlayer3, { 1600, 1000 } },
    { nMPMapObjKindBattlePlayer4, { 2400, 1000 } },
    { nMPMapObjKindRebirth,       {    0, 2500 } },
    /* Hyrule's tornado's own spawn point: what
     * grHyruleTwisterInitVars counts and grHyruleTwisterUpdateWait picks
     * from. One is enough -- the decomp's own count check needs at least
     * one and rejects more than ten, and a second would only make which
     * one a run gets random. On the deck, so the funnel's own floor
     * projection finds LINE_DECK below it. */
    { nMPMapObjKindTwister,       { -600, 1000 } },
    /* mvOpeningMarioMakeMotionWindow's own spawn point: the
     * decomp-verbatim mpCollisionGetMapObjCountKind(nMPMapObjKindMoviePlayer1)
     * guard loops forever if this count isn't exactly one, for any scene
     * that binds through the mock (stage_bind_collision, below) rather
     * than a scene-specific override the way grInishieMakeScale's own
     * test binds one. Position matches Castle's real ROM data
     * (nGRKindCastle, map_file 259) -- confirmed via
     * tools/export/ssb_stageexport.py's own read_ground/read_collision against
     * the source ROM -- though any position on the mock ground would do
     * for this test, since mvOpeningMarioMakeMotionWindow only uses it
     * to place the walking fighter. */
    { nMPMapObjKindMoviePlayer1,  {   -3,  648 } },
    /* mvOpeningJungleMakeFighters' two spawn points, on
     * the same guard: exactly one of each. Positions are arbitrary on
     * the mock ground. */
    { nMPMapObjKindMoviePlayer2,  { -300,  648 } },
    { nMPMapObjKindMoviePlayer3,  { -900,  648 } },
};

#define LINE_SLOPE 0
#define LINE_PLAT  1
#define LINE_DECK  2

static MPGeometryData mock_geo;

/* mp/mptypes.h:177 MPGroundData, the scalars the camera reads: bounds
 * wide enough that every scenario's fighter is inside them, a level
 * light (light_angle.z pitches the camera, gmcamera.c:496) */
static MPGroundData mock_ground;

/* ---- the model: a pack with no data in it ---------------------------
 *
 * dc_model_add_dobjs builds one DObj per masked-in FPackJoint, so the
 * test needs a skeleton but not a single vertex. Three DObjDesc entries
 * as Mario's first three sit (relocData/204_MarioModel.c): the hip at
 * (0, 150, 0), a torso, a limb off it -- joints[4], [5] and [6] under
 * the game's base -- with the TopN above them built by
 * ftManagerMakeFighter and the three hidden parts (kMario.hiddenparts)
 * made by the status setter when a motion asks.
 *
 * The animation bank has one entry whose script is never read -- the
 * parser above stands in for the reader -- but it has to be *there*, so
 * that ftMainSetStatus attaches it and every joint's anim_wait leaves
 * AOBJ_ANIM_NULL: the slot table names word 0 for every slot. */
#define MOCK_JOINTS 3
#define MOCK_SLOTS 8

static FPackJoint mock_joints[MOCK_JOINTS] = {
    { -1, { 0.0f, 150.0f, 0.0f }, { 0, 0, 0 }, { 1.0f, 1.0f, 1.0f } },
    {  0, { 0, 0, 0 }, { 0, 0, 0 }, { 1.0f, 1.0f, 1.0f } },
    {  1, { 0, 0, 0 }, { 0, 0, 0 }, { 1.0f, 1.0f, 1.0f } },
};

/* The pack's MainMotion words (fighter.h FPackHeader.off_script), as the
 * tests script them with the game's own command macros (ft/ftdef.h) --
 * the same words the exporter reads out of relocData/202_MarioMainMotion.c.
 * Only commands without a pointer word: a Subroutine or Goto carries an
 * address, which is 8 bytes on this host and 4 in the game's word
 * stream, so those are the target's to run (Mario's Wait script has
 * both, every frame, on H6). */
enum
{
    MOCK_SCRIPT_TURN,           /* dMarioMainMotion_Turn, verbatim */
    MOCK_SCRIPT_ATTACK,         /* a hitbox, refreshed, resized, cleared */
    MOCK_SCRIPT_LOOP,           /* a counted loop over a flag */
    MOCK_SCRIPT_ASYNC,          /* the two wait kinds, then pause */
    MOCK_SCRIPT_NUM
};
#define MOCK_SCRIPT_WORDS 16

static struct
{
    int32_t entries[MOCK_JOINTS];
    uint16_t words[2];
    uint32_t scripts[MOCK_SCRIPT_NUM][MOCK_SCRIPT_WORDS];
    FPackAnimSlots slotdir[1];
    int32_t slots[MOCK_SLOTS];
} mock_blob;

static const uint32_t kMockScripts[MOCK_SCRIPT_NUM][MOCK_SCRIPT_WORDS] = {
    [MOCK_SCRIPT_TURN] = {
        ftMotionCommandSetSlopeContour(3),
        ftMotionCommandWaitAsync(6),
        ftMotionCommandSetFlag1(1),
        ftMotionCommandEnd(),
    },
    [MOCK_SCRIPT_ATTACK] = {
        /* frame 2: hitbox 1, group 0, joint 6 (the mock's limb), 14%,
         * no rebound, element 0; size 200 at (30, 40, 50); angle 361,
         * kbs 100, kbw 100, hits ground and air; shield 3, sound level 2
         * kind 1, base knockback 40 */
        ftMotionCommandWait(2),
        ftMotionCommandMakeAttackCollS1(1, 0, 6, 14, 0, 0),
        ftMotionCommandMakeAttackCollS2(200, 30),
        ftMotionCommandMakeAttackCollS3(40, 50),
        ftMotionCommandMakeAttackCollS4(361, 100, 100, 3),
        ftMotionCommandMakeAttackCollS5(3, 2, 1, 40),
        ftMotionCommandWait(3),
        ftMotionCommandSetAttackCollSize(1, 120),
        ftMotionCommandSetAttackCollDamage(1, 9),
        ftMotionCommandWait(2),
        ftMotionCommandClearAttackCollAll(),
        ftMotionCommandEnd(),
    },
    [MOCK_SCRIPT_LOOP] = {
        ftMotionCommandLoopBegin(3),
        ftMotionCommandSetFlag2(1),
        ftMotionCommandWait(2),
        ftMotionCommandSetFlag2(0),
        ftMotionCommandWait(2),
        ftMotionCommandLoopEnd(),
        ftMotionCommandSetFlag3(7),
        ftMotionCommandEnd(),
    },
    [MOCK_SCRIPT_ASYNC] = {
        ftMotionCommandWait(4),
        ftMotionCommandSetFlag0(1),
        ftMotionCommandWaitAsync(6),
        ftMotionCommandSetFlag0(2),
        ftMotionCommandPauseScript(),
        ftMotionCommandSetFlag0(3),
        ftMotionCommandEnd(),
    },
};

static FPackAnim mock_anims[1];
static FPackHeader mock_hd;
static Fighter mock_model;

/* ---- the shield pose -------------------------------------
 *
 * What the loader builds out of a pack's FPackShieldPose (fighter.h),
 * made by hand for the mock: FTAttributes.dobj_lookup, a DObjDesc row per
 * DObj of the tree walk from XRotN -- XRotN, the three entries, YRotN at
 * (0, 195, 30) as Mario's is, and the zero-scale sentinel -- and
 * shield_anim_joints[8], a dispatch table per 45-degree sector of the
 * stick over the same walk. Each sector's table drives the hip's lean
 * (rotate y) and YRotN's tilt (rotate z) with the simplest script the
 * game's parser has a case for: SetValBlock over 45 frames, a linear
 * ramp from wherever the track was to the sector's target
 * (sys/objanim.c gcParseDObjAnimJoint, gcGetAObjValue). The game's
 * scripts are cubic and drive more tracks; the arithmetic the guard does
 * on top of them -- the sector, the frame, the lean -- is the same. */
#define MOCK_SHIELD_JOINTS 5            /* XRotN, hip, torso, limb, YRotN */
#define MOCK_SHIELD_FRAMES 45

static FPackDObjDesc mock_shield_lookup[MOCK_SHIELD_JOINTS + 1] = {
    {  0, NULL, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
    {  1, NULL, { 0.0f, 150.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
    {  2, NULL, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
    {  3, NULL, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
    {  1, NULL, { 0.0f, 195.0f, 30.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
    { 18, NULL, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
};
static const float kMockShieldHipY[8] =
    { 0.10f, 0.25f, 0.40f, 0.55f, 0.70f, 0.85f, 1.00f, 1.15f };
static const float kMockShieldYRotZ[8] =
    { 0.30f, 0.32f, 0.34f, 0.36f, 0.38f, 0.40f, 0.42f, 0.44f };
static AObjEvent32 mock_shield_hip[8][3];
static AObjEvent32 mock_shield_yrotn[8][3];
static void *mock_shield_tables[8][MOCK_SHIELD_JOINTS];

static void mock_shield_setup(void)
{
    int k;

    for (k = 0; k < 8; k++)
    {
        mock_shield_hip[k][0].u = aobjEvent32SetValBlock(AOBJ_FLAG_ROTY, MOCK_SHIELD_FRAMES);
        mock_shield_hip[k][1].f = kMockShieldHipY[k];
        mock_shield_hip[k][2].u = aobjEvent32End();
        mock_shield_yrotn[k][0].u = aobjEvent32SetValBlock(AOBJ_FLAG_ROTZ, MOCK_SHIELD_FRAMES);
        mock_shield_yrotn[k][1].f = kMockShieldYRotZ[k];
        mock_shield_yrotn[k][2].u = aobjEvent32End();
        mock_shield_tables[k][0] = NULL;                    /* XRotN */
        mock_shield_tables[k][1] = mock_shield_hip[k];      /* the hip */
        mock_shield_tables[k][2] = NULL;
        mock_shield_tables[k][3] = NULL;
        mock_shield_tables[k][4] = mock_shield_yrotn[k];    /* YRotN, last */
        mock_model.shield_joints[k] = mock_shield_tables[k];
    }
    mock_model.shield_lookup = mock_shield_lookup;
}
static GObj *mock_gobj;

/* src/dc/fighter.c is in this build for its loader
 * (FT_HOSTTEST leaves its PVR half, its pose player and its draw out):
 * the manager loads the real packs out of romdisk/ through the real
 * fighter_load, and clones one per fighter through the real
 * fighter_clone, which on the host is the struct copy with its own clip
 * buffer -- all the tree builder reads. The Mario mock above stays for
 * the tests that want an animation of their own making. */

/* fighter.c's draw half defines the viewport the camera pass publishes
 * (fighter.h); that half is not in this build */
#ifdef SSB_NO_DRAW
DCViewport gDCViewport = { 320.0f, 240.0f, 320.0f, 240.0f };
#endif

/* the controller the fighter's FTDesc points at; frame() fills it */
static SYController mock_in;

/* the second fighter, for the hit tests: player 1, its own pad (never
 * pressed), made by spawn_second and destroyed with the next spawn */
static GObj *mock_gobj2;
static FTStruct *fpp2;
static SYController mock_in2;
#define fp2 (*fpp2)

/* sc/scmanager.c:42: the battle state the manager and the stats write
 * into. One human Mario in slot 0. */
static SCBattleState mock_battle;

/* ---- what the battle scene reaches for that this build has not -------
 *
 * src/dc/scvsbattle.c is in the link, so the scene
 * driver at the bottom of this file can run a whole match. Four of the
 * symbols it names belong to files this build leaves out: src/dc/stage.c
 * and src/dc/input.c are both PVR and maple code. The stage the scene
 * would build is load_stage's, already bound; the pads the scene would
 * read are the four below, and the test writes them the way a pad
 * would. */
SYController gSYControllerDevices[MAXCONTROLLERS];
SYController gSYControllerMain;
u32 gSYControllerConnectedNum;
/* one pad, on port 0: what the menus' pad reads walk (src/dc/input.c) */
s8 gSYControllerDeviceStatuses[MAXCONTROLLERS] = { 0, -1, -1, -1 };

/* Set while the results screen is running: it leaves on a START press
 * from any pad, and its own 370-tic wait decides when one counts. Tapped
 * every other tic so there is always an edge. */
static int gSceneFeedStart;

/* a scene test's own per-tic probe, run from the controller read --
 * which is the one place inside a running scene the test can reach,
 * since syTaskmanStartTask does not return until the scene is over */
static void (*gSceneTicHook)(void);

void syControllerFuncRead(void)
{
    gSYControllerDevices[0].button_tap =
        (gSceneFeedStart && (dSYTaskmanUpdateCount & 1)) ? START_BUTTON : 0;
    gSYControllerMain = gSYControllerDevices[0];
    if (gSceneTicHook != NULL)
    {
        gSceneTicHook();
    }
}

GObj *grCommonSetupInitAll(void)
{
    return NULL;
}

/* src/dc/stage.c is the PVR loader and is not in this build, so the two
 * things the scenes reach it through are here. gGRStages is the table of
 * stages the port has packs for (stage.h): the stage select previews
 * from it and the battle binds out of it, and the stage-select test
 * fills two slots with mock stages below. */
Stage *gGRStages[GR_STAGES_MAX];
Stage *gStageBound;

/* gr/grcommonsetup.c:0x801313F0's own, which on the target is
 * src/dc/stage.c's -- that file is the PVR loader and is not in this
 * build, so the per-stage ground variables the one ported gr/ stage file
 * keeps its state in (Hyrule's tornado) live here instead.
 * grOverlayLoad below clears it, as the target's does. */
GRStruct gGRCommonStruct;

void stage_bind(Stage *st)
{
    gStageBound = st;
}

/* grStageAcquire and grStageRelease over that table. The
 * real pair reads a .stg off the medium into a malloc'd Stage and frees
 * it when the last holder lets go; these hand back whatever the test put
 * in gGRStages and only keep the count, because what a test can check
 * here is not the bytes -- there are none -- but whether every acquire a
 * scene makes is matched by a release. gStageHolds is that answer: zero
 * between scenes, and never negative.
 *
 * The mock does not empty gGRStages on the last release, deliberately.
 * The table's entries are the test's own static Stages; a NULL here
 * would mean "the test never installed one", which is a different thing
 * from "nobody is holding it" and is what the no-pack arm checks. */
int gStageHolds;
int gStageAcquires;
int gStageReleaseUnderflows;
static int sStageRefs[GR_STAGES_MAX];

const char *grStageFileName(s32 gkind)
{
    if (gkind < 0 || gkind >= GR_STAGES_MAX || gGRStages[gkind] == NULL)
    {
        return NULL;
    }
    return "mock.stg";
}

Stage *grStageAcquire(s32 gkind)
{
    if (gkind < 0 || gkind >= GR_STAGES_MAX || gGRStages[gkind] == NULL)
    {
        return NULL;
    }
    sStageRefs[gkind]++;
    gStageHolds++;
    gStageAcquires++;
    return gGRStages[gkind];
}

void grStageRelease(s32 gkind)
{
    if (gkind < 0 || gkind >= GR_STAGES_MAX || sStageRefs[gkind] <= 0)
    {
        gStageReleaseUnderflows++;
        return;
    }
    sStageRefs[gkind]--;
    gStageHolds--;
}

/* src/dc/stage.c's, which walks 32-bit scripts: nothing to attach here */
sb32 stage_add_layer_anims(Stage *st, GObj *gobj)
{
    (void)st;
    (void)gobj;
    return FALSE;
}

void stage_release(Stage *st)
{
    (void)st;   /* the test's stages are statics; nothing to give back */
}

/* src/dc/stage.c's, verbatim: sc1pgameboss.c looks its trees up by name
 * (linked since its two defeat hooks went live in sc1pgame.c). */
int stage_boss_pack(Stage *st, const char *name)
{
    int i;

    for (i = 0; i < st->boss_model_count; i++)
    {
        if (strcmp(st->boss_names[i], name) == 0)
        {
            return i;
        }
    }
    return -1;
}

static void stage_holds_reset(void)
{
    memset(sStageRefs, 0, sizeof(sStageRefs));
    gStageHolds = 0;
    gStageAcquires = 0;
    gStageReleaseUnderflows = 0;
}

/* stage.c's overlay-2 arm, over the stand-in above. gGRStages is not
 * cleared here for the reason src/dc/stage.c gives for not clearing it
 * there: the map files are the ROM's on the N64 and the romdisk's here,
 * loaded once, and overlay 2 holds the code that reads them, not the
 * files. gStageBound is sStage under another name -- the stage the
 * scene is playing on -- and that is the overlay's. */
void grOverlayLoad(void)
{
    OVERLAY_CLEAR(gStageBound);
    OVERLAY_CLEAR(gGRCommonStruct);
}

/* stage.c's, less the PVR header: what the training scene handed over,
 * for test_training_wallpaper to read back. */
static const DCSpriteTex *gHostStageWallpaperOverride;
static int gHostStageWallpaperOverrideCalls;

void stage_set_wallpaper_override(const DCSpriteTex *tex)
{
    gHostStageWallpaperOverride = tex;
    gHostStageWallpaperOverrideCalls++;
}

/* stage.c's, over a Stage the test made: the fields are the same fill
 * and nothing here touches the PVR. What the test asserts about it is
 * that the stage select draws it where the game draws its wallpaper --
 * the pixels are the target's business. */
Sprite *stage_wallpaper_sprite(Stage *st)
{
    if (st == NULL || st->wp_txr == NULL)
    {
        return NULL;
    }
    if (st->wp_sprite.bitmap == NULL)
    {
        st->wp_tex.txr = st->wp_txr;
        st->wp_tex.texw = st->wp_texw;
        st->wp_tex.texh = st->wp_texh;
        st->wp_tex.imgw = st->wp_w;
        st->wp_tex.imgh = st->wp_h;
        st->wp_tex.fmt = nDCSpriteTexFmtARGB1555;

        st->wp_bitmap.width = (s16)st->wp_w;
        st->wp_bitmap.width_img = (s16)st->wp_w;
        st->wp_bitmap.buf = &st->wp_tex;
        st->wp_bitmap.actualHeight = (s16)st->wp_h;

        st->wp_sprite.width = (s16)st->wp_w;
        st->wp_sprite.height = (s16)st->wp_h;
        st->wp_sprite.scalex = 1.0F;
        st->wp_sprite.scaley = 1.0F;
        st->wp_sprite.attr = SP_TRANSPARENT;
        st->wp_sprite.red = st->wp_sprite.green = st->wp_sprite.blue =
            st->wp_sprite.alpha = 0xFF;
        st->wp_sprite.istep = 1;
        st->wp_sprite.nbitmaps = 1;
        st->wp_sprite.bmheight = (s16)st->wp_h;
        st->wp_sprite.bmHreal = (s16)st->wp_h;
        st->wp_sprite.bmfmt = G_IM_FMT_RGBA;
        st->wp_sprite.bmsiz = G_IM_SIZ_16b;
        st->wp_sprite.bitmap = &st->wp_bitmap;
    }
    return &st->wp_sprite;
}

/* stage.c's, over the mock geometry: the scene calls this from inside
 * itself (scvsbattle.c:259), after syTaskmanStartTask has emptied the
 * heap the tables come out of, so the tables have to be rebuilt here
 * exactly as they are on the target. load_stage builds the same tables
 * for the tests that run without a scene around them.
 *
 * The ground pointer is bound here as well, which the target does one
 * stage earlier in scene setup: stage_bind sets it out of the Stage the scene picked
 * (scvsbattle.c:397, after the overlay reload). This build has no Stage
 * object for the scene to pick, so the stand-in for
 * mpCollisionInitGroundData binds the mock ground instead -- and it has
 * to be inside the scene, because gMPCollisionGroundData is one of
 * mpcollision.c's statics and overlay 2's reload has just zeroed the
 * lot of them. That is the mechanism working, not a workaround for it:
 * without it nothing would clear a decomp file's statics at all. */
void stage_bind_collision(void)
{
    mpCollisionLoadGeometry(&mock_geo);
    gMPCollisionGroundData = &mock_ground;
}

/* src/dc/bgm.c is the streaming player and is not in this build. The
 * stop has to move gHostBGMPlaying below, because scVSBattleStartScene
 * waits on it between two battles and a test that left it
 * playing would hang the next one. */
sb32 gHostBGMPlaying;

void syAudioStopBGMAll(void)
{
    gHostBGMPlaying = FALSE;
}

/* the match clock's last five seconds fade the music down
 * (ifCommonTimerFuncRun). The last value asked for is kept so the test
 * can read it. */
u32 gHostBGMVolume;

void syAudioSetBGMVolume(s32 sngplayer, u32 vol)
{
    (void)sngplayer;
    gHostBGMVolume = vol;
}

/* mnsoundtest.c's START_BUTTON fade-out is the only caller (bgm.c's real
 * version ramps over `time` ticks); the test only cares where the volume
 * ends up, so this just forwards to the instant setter above. */
void syAudioSetBGMVolumeFade(s32 sngplayer, u32 vol, u32 time)
{
    (void)time;
    syAudioSetBGMVolume(sngplayer, vol);
}

/* what the results screen's audio thread waits on (scvsresults.c
 * mnVSResultsAudioThreadUpdate): the win jingle starting and then
 * ending. The test drives it; syAudioStopBGMAll above clears it. */
s32 syAudioCheckBGMPlaying(s32 sngplayer)
{
    (void)sngplayer;
    return gHostBGMPlaying;
}

void grWallpaperMakeDecideKind(void)
{
}

/* src/dc/stage.c's, and src/dc/stage.c is not in this build. The real
 * one sweeps the wallpaper link; nothing here makes wallpaper GObjs. */
void grWallpaperResumeProcessAll(void)
{
}

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } } while (0)

#define CHECK_EQF(a, b) do { \
    float _a = (a), _b = (b); \
    if (_a != _b) { \
        printf("FAIL %s:%d: %s = %g, want %s = %g\n", __FILE__, __LINE__, \
               #a, _a, #b, _b); \
        failures++; \
    } } while (0)

#define CHECK_NEAR(a, b, eps) do { \
    float _a = (a), _b = (b); \
    if (fabsf(_a - _b) > (eps)) { \
        printf("FAIL %s:%d: %s = %g, want %s = %g (+/-%g)\n", __FILE__, \
               __LINE__, #a, _a, #b, _b, (float)(eps)); \
        failures++; \
    } } while (0)

#define CHECK_STATUS(want) do { \
    if (fp.status_id != (want)) { \
        printf("FAIL %s:%d: status %s, want %s\n", __FILE__, __LINE__, \
               ftMainStatusName(fp.status_id), ftMainStatusName(want)); \
        failures++; \
    } } while (0)

/* sys/taskman.c:1227 syTaskmanStartTask's pools, sized for one fighter
 * on a five-joint skeleton: 1 GObj, 6 DObjs, 18 XObjs. */
static SYTaskmanSetup mock_taskman_setup =
{
    { 0, NULL, NULL, NULL, 0, 1, 2, 0, 0, 0, 0, 0, 2, 0, NULL, NULL },
    0, sizeof(u64) * 192, 0, 0,
    8,                                  /* GObjProcesses */
    8,  sizeof(GObj),
    64,                                 /* XObjs */
    NULL, NULL,
    64,                                 /* AObjs */
    4,                                  /* MObjs */
    16, sizeof(DObj),
    2,  sizeof(SObj),
    1,  sizeof(CObj),
    NULL                                /* func_start: the tests spawn */
};

static void load_stage(void)
{
    int i;

    /* The scene's general heap, which the collision tables and, since
     * the fighter manager's pools come out of (taskman.h).
     * The target sizes this once at boot; here it is remade per load so
     * each test starts from an empty region. The manager's four Fighter
     * instances are 17.7K each on the host (see main.c on why), and the
     * scene driver at the bottom of this file allocates a second set of
     * four on top of these when it starts a real battle out of the same
     * region -- 256K was enough until then, 512K covers both. The
     * effect bank the battle scene loads on the target
     * is 326K on top of that, and the target's heap is 768K for it
     * (main.c); this build does not load it, for the reason in
     * scVSBattleLoadEffectBank.
     *
     * load_stage() itself is only called at a handful of scene-test
     * boundaries further down this file, not once per unit test -- most
     * of main()'s hundreds of test_*() calls, items/weapons among them,
     * share the single region main() makes and never reset it, so usage
     * accumulates across the whole run rather than per test.
     * is the first it/ file with its own companion weapon (itfflower.c's
     * wpManagerMakeWeapon calls), and adding it tipped the shared region
     * over 512K by the time test_entry_sequence's own sprite banks
     * needed their share -- sys/malloc.c's syMallocSet spins forever on
     * overflow (`while (TRUE);`, the game's own halt-and-catch-fire),
     * which reads as a hang, not a crash. 2M keeps the same headroom
     * ratio the 256K->512K jump did, with room for more roster items and
     * weapons. */
    if (syTaskmanMakeGeneralHeap(2 * 1024 * 1024) < 0)
        abort();

    for (i = 0; i < (int)(sizeof(mock_motions) / sizeof(mock_motions[0]));
         i++)
    {
        mock_motions[i].anim = 0;
        mock_motions[i].flags = 0;
    }
    /* ft/ftdata.c:213-228: every ledge motion runs TransN as root
     * motion, CliffCatch through CliffEscapeSlow2 */
    for (i = nFTCommonMotionCliffCatch; i <= nFTCommonMotionCliffEscapeSlow2;
         i++)
        mock_motions[i].flags = MOCK_ANIMFLAG_TRANSN;
    /* ft/ftdata.c:277-278: the two roll motions, RollF and RollB, carry
     * the same bit -- the roll's distance is its animation's TransN
     * (the status rows' physics proc is ftPhysicsApplyGroundVelTransN,
     * which reads that joint and would find none without this) */
    mock_motions[nFTCommonMotionEscapeF].flags = MOCK_ANIMFLAG_TRANSN;
    mock_motions[nFTCommonMotionEscapeB].flags = MOCK_ANIMFLAG_TRANSN;
    /* ft/ftdata.c:154 and 308: of the dash and run motions TurnRun and
     * AttackDash carry it, and both status rows' physics proc is the one
     * that reads the joint */
    mock_motions[nFTCommonMotionTurnRun].flags = MOCK_ANIMFLAG_TRANSN;
    mock_motions[nFTCommonMotionAttackDash].flags = MOCK_ANIMFLAG_TRANSN;
    /* ft/ftdata.c: Mario's two Appear motions are TransN root motion
     * too, and AnimJoint scripts. The ANIMJOINT bit is left off here on
     * purpose: it would route the joints through the real
     * gcParseDObjAnimJoint (compiled into this test), which would read
     * the mock's empty words, and the mock parser above stands in for
     * whichever parser the motion names. */
    mock_motions[nFTMarioMotionAppearR].flags = MOCK_ANIMFLAG_TRANSN;
    mock_motions[nFTMarioMotionAppearL].flags = MOCK_ANIMFLAG_TRANSN;
    /* ft/ftdata.c:341-342: the Super Jump Punch, both halves, are TransN
     * root motion too -- ftMarioSpecialHiProcPhysics's grounded half is
     * ftPhysicsApplyGroundVelTransN and its airborne half (through
     * ftPhysicsApplyAirVelTransNAll) is ftPhysicsGetAirVelTransN, the
     * same joint read the roll and ledge motions above need (the status is
     * reachable through the ground cascade rather than only by direct
     * call). */
    mock_motions[nFTMarioMotionSpecialHi].flags = MOCK_ANIMFLAG_TRANSN;
    mock_motions[nFTMarioMotionSpecialAirHi].flags = MOCK_ANIMFLAG_TRANSN;
    /* ft/ftdata.c:5963 (index 205, nFTPurinMotionSpecialN): Pound's
     * grounded half is TransN root motion too -- its status row's
     * physics proc is ftPhysicsApplyGroundVelTransN, the same read the
     * Super Jump Punch above needs. The literal there
     * names the bit FTANIM_FLAG_XROTN_JOINT, but it is bit 0x40000000,
     * which fttypes.h's FTAnimDesc bitfield calls is_use_transn_joint --
     * the same ftdef.h/fttypes.h cross-file naming swap seen elsewhere,
     * not a different bit. The airborne half (SpecialAirN, index 206) is
     * FTANIM_FLAG_NONE and its own proc never reads the joint, so it
     * needs no entry here. */
    mock_motions[nFTPurinMotionSpecialN].flags = MOCK_ANIMFLAG_TRANSN;
    /* ft/ftdata.c:2381-2382 (dFTSamusMotionDescs): Screw Attack, BOTH
     * halves -- ScrewAttackGround and ScrewAttackAir alike carry
     * FTANIM_FLAG_XROTN_JOINT, unlike Pound above where only the ground
     * half did. ftSamusSpecialHiProcPhysics's air branch calls
     * ftPhysicsApplyAirVelTransNYZ (ftPhysicsGetAirVelTransN under it),
     * the same joint read, so both entries are needed here. */
    mock_motions[nFTSamusMotionSpecialHi].flags = MOCK_ANIMFLAG_TRANSN;
    mock_motions[nFTSamusMotionSpecialAirHi].flags = MOCK_ANIMFLAG_TRANSN;
    /* ft/ftdata.c:4358-4359 (dFTCaptainMotionDescs): Falcon Punch, only the
     * ground half -- FalconPunchGround carries FTANIM_FLAG_XROTN_JOINT the
     * same as Pound's ground half above, while FalconPunchAir is
     * FTANIM_FLAG_NONE. ftCaptainSpecialNProcPhysics's ground branch calls
     * ftPhysicsApplyGroundVelTransN; the air branch (ftCaptainSpecialAirN-
     * ProcPhysics) calls ftPhysicsApplyAirVelFriction/DriftFastFall, which
     * read no joint, so only the ground entry is needed here (the same
     * one-half shape Pound's rows have, not Screw Attack's). */
    mock_motions[nFTCaptainMotionSpecialN].flags = MOCK_ANIMFLAG_TRANSN;
    /* ft/ftdata.c:4365,4367,4368 (dFTCaptainMotionDescs, first row 4155):
     * Falcon Dive, its Throw and its air version all carry
     * FTANIM_FLAG_XROTN_JOINT and all three run ftPhysicsApplyAirVelTransN-
     * All (Hi/AirHi through ftCaptainSpecialHiProcPhysics); the Catch
     * (4366, CatchingEnemyWhileDiving) carries 0x10000000 and no joint
     * read, so it gets no entry. */
    mock_motions[nFTCaptainMotionSpecialHi].flags = MOCK_ANIMFLAG_TRANSN;
    mock_motions[nFTCaptainMotionSpecialHiThrow].flags = MOCK_ANIMFLAG_TRANSN;
    mock_motions[nFTCaptainMotionSpecialAirHi].flags = MOCK_ANIMFLAG_TRANSN;
    /* ft/ftdata.c:3858,3860 (dFTYoshiMotionDescs): Yoshi Bomb, BOTH Start
     * halves -- GroundPoundGroundStart and GroundPoundAir alike carry
     * FTANIM_FLAG_XROTN_JOINT, the same both-halves shape Screw Attack has,
     * and their shared ProcPhysics is ftPhysicsApplyAirVelTransNAll, the
     * joint read itself. GroundPoundLanding (3859) is FTANIM_FLAG_NONE and
     * the Loop row has no motion at all. */
    mock_motions[nFTYoshiMotionSpecialLwStart].flags = MOCK_ANIMFLAG_TRANSN;
    mock_motions[nFTYoshiMotionSpecialAirLwStart].flags = MOCK_ANIMFLAG_TRANSN;
    /* ft/ftdata.c:1080-1087 (indices 203, 205, 207, 209, 210): the five
     * aerial halves of Fire Fox carry FTANIM_FLAG_XROTN_JOINT, the five
     * grounded ones FTANIM_FLAG_NONE. Only the Bound row's physics proc
     * (ftPhysicsApplyAirVelTransNYZ) actually reads the joint; the rest
     * are flagged here to match ftdata. */
    mock_motions[nFTFoxMotionSpecialAirHiStart].flags = MOCK_ANIMFLAG_TRANSN;
    mock_motions[nFTFoxMotionSpecialAirHiHold].flags = MOCK_ANIMFLAG_TRANSN;
    mock_motions[nFTFoxMotionSpecialAirHi].flags = MOCK_ANIMFLAG_TRANSN;
    mock_motions[nFTFoxMotionSpecialAirHiEnd].flags = MOCK_ANIMFLAG_TRANSN;
    mock_motions[nFTFoxMotionSpecialAirHiBound].flags = MOCK_ANIMFLAG_TRANSN;
    /* ft/ftdata.c:137-138: ShieldOn and ShieldOff link XRotN (bit 31, row
     * 0) and YRotN (bit 29, row 2) -- the two the shield pose animates */
    mock_motions[nFTCommonMotionGuardOn].flags = 0x80000000u | 0x20000000u;
    mock_motions[nFTCommonMotionGuardOff].flags = 0x80000000u | 0x20000000u;
    /* the scripts: Turn's is Mario's own; the others hang off motions
     * the tests reach and Mario would not script that way (the words
     * themselves go into the blob below, after it is cleared) */
    for (i = 0; i < (int)(sizeof(mock_motions) / sizeof(mock_motions[0]));
         i++)
        mock_motions[i].script = 0;
    mock_motions[nFTCommonMotionTurn].script =
        offsetof(typeof(mock_blob), scripts[MOCK_SCRIPT_TURN]);
    mock_motions[nFTCommonMotionSquatWait].script =
        offsetof(typeof(mock_blob), scripts[MOCK_SCRIPT_ATTACK]);
    mock_motions[nFTCommonMotionAttack11].script =
        offsetof(typeof(mock_blob), scripts[MOCK_SCRIPT_ATTACK]);
    mock_motions[nFTCommonMotionSquat].script =
        offsetof(typeof(mock_blob), scripts[MOCK_SCRIPT_LOOP]);
    mock_motions[nFTCommonMotionFall].script =
        offsetof(typeof(mock_blob), scripts[MOCK_SCRIPT_ASYNC]);

    mock_geo.yakumono_count = sizeof(kLineInfo) / sizeof(kLineInfo[0]);
    mock_geo.line_info = (MPLineInfo *)kLineInfo;
    mock_geo.vertex_links = (void *)kLinks;
    mock_geo.vertex_id = (void *)kVertexIDs;
    mock_geo.vertex_data = (void *)kVpos;
    mock_geo.mapobj_count = sizeof(kMapObjs) / sizeof(kMapObjs[0]);
    mock_geo.mapobjs = (void *)kMapObjs;

    /* The pools first and the geometry after, which is the order a scene
     * has: syTaskmanStartTask cuts the pools out of a freshly emptied
     * heap and the ground is loaded inside the scene's func_start
     * (scvsbattle.c:155). It used to be the other way round here,
     * because mpCollisionLoadGeometry reset the heap itself; it does not
     * any more, and the reset the pool setup does would take the
     * geometry with it. */
    syTaskmanSetupPools(&mock_taskman_setup);

    mpCollisionLoadGeometry(&mock_geo);

    memset(&mock_ground, 0, sizeof(mock_ground));
    mock_ground.camera_bound_top = 4000;
    mock_ground.camera_bound_bottom = -2000;
    mock_ground.camera_bound_right = 4000;
    mock_ground.camera_bound_left = -4000;
    mock_ground.map_bound_top = 5000;
    mock_ground.map_bound_bottom = -3000;
    mock_ground.map_bound_right = 5000;
    mock_ground.map_bound_left = -5000;
    /* relocData/265_GRHyruleMap.c:39-45 emblem_colors, which the HUD
     * tints each player's emblem with */
    mock_ground.emblem_colors[0].r = 0xFF; mock_ground.emblem_colors[0].g = 0xA0; mock_ground.emblem_colors[0].b = 0xA0;
    mock_ground.emblem_colors[1].r = 0xA0; mock_ground.emblem_colors[1].g = 0xA0; mock_ground.emblem_colors[1].b = 0xFF;
    mock_ground.emblem_colors[2].r = 0xF0; mock_ground.emblem_colors[2].g = 0xF0; mock_ground.emblem_colors[2].b = 0x64;
    mock_ground.emblem_colors[3].r = 0xAA; mock_ground.emblem_colors[3].g = 0xFF; mock_ground.emblem_colors[3].b = 0xAA;
    gMPCollisionGroundData = &mock_ground;


    /* the pack: a skeleton and one animation slot (see MOCK_JOINTS) */
    memset(&mock_blob, 0, sizeof(mock_blob));
    memcpy(mock_blob.scripts, kMockScripts, sizeof(mock_blob.scripts));
    mock_hd.joint_count = MOCK_JOINTS;
    mock_hd.anim_count = 1;
    /* the name is what ftManagerSetupFilesAllKind derives the kind's
     * sprite bank from (romdisk/ftmario.spr, the real one the game's
     * build makes), so the HUD's stock rows and emblems have sprites here */
    memcpy(mock_hd.name, "Mario", 6);
    mock_anims[0].off_words = offsetof(typeof(mock_blob), words);
    mock_anims[0].nwords = sizeof(mock_blob.words) / sizeof(uint16_t);
    mock_anims[0].off_entries = offsetof(typeof(mock_blob), entries);
    /* the raw slot table the attach deals down the tree: word 0 for
     * every slot, so every DObj the walk reaches -- the three entries
     * and whichever hidden parts the motion linked -- gets a script */
    mock_blob.slotdir[0].off = offsetof(typeof(mock_blob), slots);
    mock_blob.slotdir[0].count = MOCK_SLOTS;

    memset(&mock_model, 0, sizeof(mock_model));
    mock_model.blob = &mock_blob;
    mock_model.hd = &mock_hd;
    mock_model.joints = mock_joints;
    mock_model.anims = mock_anims;
    mock_model.slots = mock_blob.slotdir;
    mock_model.motions = mock_motions;
    mock_model.motion_count = sizeof(mock_motions) / sizeof(mock_motions[0]);
    mock_shield_setup();

    /* scvsbattle.c:161: the FTStruct pool, from the same heap; and the
     * battle the fighters are made into */
    memset(&mock_battle, 0, sizeof(mock_battle));
    gSCManagerBattleState = &mock_battle;
    /* scvsbattle.c's HUD setup: the stock snap and score popup a KO
     * makes read these rows */
    ifCommonPlayerDamageSetDigitPositions();
    mock_battle.players[0].pkind = nFTPlayerKindMan;
    mock_battle.players[0].fkind = nFTKindMario;
    mock_battle.players[1].pkind = nFTPlayerKindNot;
    mock_battle.players[2].pkind = nFTPlayerKindNot;
    mock_battle.players[3].pkind = nFTPlayerKindNot;
    mock_battle.stocks = 3;
    /* mn/mnvsmode/mnvsoptions.c:1204,1226, the VS defaults */
    mock_battle.damage_ratio = 100;
    mock_battle.players[0].handicap = mock_battle.players[1].handicap = 5;
    /* the pack the pools are sized from: the mock, which has no
     * vertices, so the clip pool is empty */
    gFTManagerModels[nFTKindMario] = &mock_model;
    ftManagerAllocFighter(FTDATA_FLAG_MAINMOTION, GMCOMMON_PLAYERS_MAX);
    mock_gobj = NULL;
}

/* the attributes the next spawn uses; a test swaps in a variant to move
 * one FTAttributes field (the cliff_status_ga tests) */
static const FPackAttr *mock_attr = &kMario;

/* One fighter, made the way scVSBattleStartBattle makes one
 * (scvsbattle.c:170-202 into ftManagerMakeFighter), except that the
 * spawn skips the entry -- the tests want Wait or Fall at once -- and
 * releases the controller, which the GO! would. `spawn_entry` below is
 * the entry itself. */
static void spawn_desc(float x, float y, int lr, int is_skip_entry)
{
    FTDesc desc = dFTManagerDefaultFighterDesc;

    mock_model.attr = mock_attr;
    mock_anim_len = 1e9f;
    mock_transn[0] = mock_transn[1] = mock_transn[2] = 0.0f;
    mock_transn_rot_z = 0.0f;

    /* one fighter at a time: the previous spawn's GObj goes back to the
     * pool with its whole DObj tree (objman.c:1780 gcEjectGObj) and its
     * FTStruct to the manager's */
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
    desc.pos.x = x;
    desc.pos.y = y;
    desc.pos.z = 0.0f;
    desc.lr = lr;
    desc.team = 0;
    desc.player = 0;
    desc.detail = nFTPartsDetailHigh;
    desc.shade = 1;
    desc.stock_count = mock_battle.stocks;
    desc.damage = 0;
    desc.handicap = mock_battle.players[0].handicap;
    desc.pkind = nFTPlayerKindMan;
    desc.controller = &mock_in;
    desc.is_skip_entry = is_skip_entry;
    desc.figatree_heap = ftManagerAllocFigatreeHeapKind(nFTKindMario);
    memset(&mock_in, 0, sizeof(mock_in));

    mock_gobj = ftManagerMakeFighter(&desc);
    ftParamInitPlayerBattleStats(0, mock_gobj);
    fpp = ftGetStruct(mock_gobj);
}

/* The FTStruct pool hands a slot back to the next spawn, and the game's
 * ftManagerInitFighter does not clear the knockback vectors -- in a real
 * match a fighter is never destroyed and remade, so nothing there ever
 * carries. Between tests it does, and a leftover launch would fly the
 * new fighter off the stage; a fresh spawn is what these tests mean. */
static void ft_clear_knockback(FTStruct *f)
{
    f->physics.vel_damage_air.x = f->physics.vel_damage_air.y =
        f->physics.vel_damage_air.z = 0.0f;
    f->physics.vel_damage_ground = 0.0f;
}

static void spawn(float x, float y)
{
    spawn_desc(x, y, 1, TRUE);
    ftParamUnlockPlayerControl(mock_gobj);
    ft_clear_knockback(fpp);
}

/* the second fighter, player 1, facing `lr`, standing where put */
static void spawn_second(float x, float y, int lr)
{
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = nFTKindMario;
    desc.pos.x = x;
    desc.pos.y = y;
    desc.pos.z = 0.0f;
    desc.lr = lr;
    desc.team = 1;
    desc.player = 1;
    desc.detail = nFTPartsDetailHigh;
    desc.shade = 1;
    desc.stock_count = mock_battle.stocks;
    desc.damage = 0;
    desc.handicap = mock_battle.players[1].handicap;
    desc.pkind = nFTPlayerKindMan;
    desc.controller = &mock_in2;
    desc.is_skip_entry = TRUE;
    desc.figatree_heap = ftManagerAllocFigatreeHeapKind(nFTKindMario);
    memset(&mock_in2, 0, sizeof(mock_in2));

    mock_battle.players[1].pkind = nFTPlayerKindMan;
    mock_battle.players[1].fkind = nFTKindMario;
    mock_gobj2 = ftManagerMakeFighter(&desc);
    ftParamInitPlayerBattleStats(1, mock_gobj2);
    fpp2 = ftGetStruct(mock_gobj2);
    ftParamUnlockPlayerControl(mock_gobj2);
    ft_clear_knockback(fpp2);
}

/* One game frame for the fighters: the controller as the port's input
 * layer would have latched it, then the six GObjProcesses in the order
 * gcRunAll runs them -- every fighter's priority-5 process, then every
 * fighter's 4, down to 0 (sys/objman.c gcRunAll walks the process
 * queue by priority, not by GObj). */
static void frame(int sx, int sy, unsigned tap, unsigned hold,
                  unsigned release)
{
    static void (*const procs[])(GObj *) = {
        ftMainProcUpdateInterrupt, ftMainProcPhysicsMapDefault,
        ftMainProcPhysicsMapCapture, ftMainProcSearchCatch,
        ftMainProcSearchHitAll, ftMainProcParams,
    };
    unsigned i;

    mock_in.stick_range.x = sx;
    mock_in.stick_range.y = sy;
    mock_in.button_tap = tap;
    mock_in.button_hold = hold;
    mock_in.button_release = release;
    mpCollisionAdvanceUpdateTic(NULL);
    for (i = 0; i < sizeof(procs) / sizeof(procs[0]); i++)
    {
        procs[i](mock_gobj);
        if (mock_gobj2 != NULL)
            procs[i](mock_gobj2);
    }
}

static void idle_frames(int n)
{
    while (n--)
        frame(0, 0, 0, 0, 0);
}

/* run until `pred` or `limit` frames pass; returns frames used */
static int run_until(int sx, int sy, int limit, int (*pred)(void))
{
    int i;

    for (i = 0; i < limit; i++)
    {
        if (pred())
            return i;
        frame(sx, sy, 0, 0, 0);
    }
    return limit;
}
