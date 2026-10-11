/* hosttest/weapons.c -- part of hosttest_ft.c: the weapon machinery (wpmain/wpprocess/wpdisplay/wpmap), the
 * weapon packs and tables, and each fighter's projectile.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* src/dc/wpmain.c: the weapon's per-frame WPStruct helpers, verified
 * against the decomp math with a stack WPStruct (no GObj needed for
 * these -- the vel/facing wrappers that take a weapon_gobj are thin
 * wpGetStruct()+DObjGetStruct() shells over the same arithmetic and get
 * exercised once a weapon actually spawns in a disc run). */
static void test_wp_main(void)
{
    WPStruct twp;
    FTStruct tfp;
    s32 i;

    /* wpMainDecLifeCheckExpire: counts down, TRUE only on the tic it hits 0 */
    twp.lifetime = 3;
    CHECK(wpMainDecLifeCheckExpire(&twp) == FALSE); /* 3 -> 2 */
    CHECK(twp.lifetime == 2);
    CHECK(wpMainDecLifeCheckExpire(&twp) == FALSE); /* 2 -> 1 */
    CHECK(wpMainDecLifeCheckExpire(&twp) == TRUE);  /* 1 -> 0 */
    CHECK(twp.lifetime == 0);

    /* wpMainGetStaledDamage: (s32)(damage*stale + 0.999) -- the 0.999
     * rounds a fractional product up, an exact one stays put */
    twp.attack_coll.damage = 12; twp.attack_coll.stale = 1.0F;
    CHECK(wpMainGetStaledDamage(&twp) == 12);       /* 12.999 -> 12 */
    twp.attack_coll.stale = 0.5F;
    CHECK(wpMainGetStaledDamage(&twp) == 6);        /* 6.999 -> 6 */
    twp.attack_coll.damage = 10; twp.attack_coll.stale = 0.95F;
    CHECK(wpMainGetStaledDamage(&twp) == 10);       /* 9.5+0.999=10.499 -> 10 */
    twp.attack_coll.damage = 0; twp.attack_coll.stale = 1.0F;
    CHECK(wpMainGetStaledDamage(&twp) == 0);        /* 0.999 -> 0 */

    /* wpMainApplyGravityClampTVel: -= gravity in y, then clamp the 2D
     * air speed to terminal velocity. Below tv it is untouched... */
    twp.physics.vel_air.x = 0.0F; twp.physics.vel_air.y = 0.0F;
    twp.physics.vel_air.z = 5.0F;                   /* z is not touched */
    wpMainApplyGravityClampTVel(&twp, 1.0F, 3.0F);
    CHECK_EQF(twp.physics.vel_air.y, -1.0F);        /* mag 1 <= 3, no clamp */
    wpMainApplyGravityClampTVel(&twp, 1.0F, 3.0F);
    CHECK_EQF(twp.physics.vel_air.y, -2.0F);        /* mag 2 <= 3 */
    wpMainApplyGravityClampTVel(&twp, 1.0F, 3.0F);
    CHECK_EQF(twp.physics.vel_air.y, -3.0F);        /* mag 3, not > 3 */
    wpMainApplyGravityClampTVel(&twp, 1.0F, 3.0F);
    CHECK_NEAR(twp.physics.vel_air.y, -3.0F, 1e-5F);/* mag 4 > 3 -> clamped back */
    CHECK_EQF(twp.physics.vel_air.z, 5.0F);         /* z untouched throughout */
    /* ...and a diagonal clamp normalises the x/y pair to tv */
    twp.physics.vel_air.x = 6.0F; twp.physics.vel_air.y = 4.0F;
    wpMainApplyGravityClampTVel(&twp, 12.0F, 5.0F); /* y: 4-12=-8; (6,-8) mag 10 */
    CHECK_NEAR(twp.physics.vel_air.x, 3.0F, 1e-4F); /* (6,-8)/10*5 = (3,-4) */
    CHECK_NEAR(twp.physics.vel_air.y, -4.0F, 1e-4F);

    /* wpMainReflectorSetLR: flips vel_air.x only when it opposes the
     * reflecting fighter's facing (x*lr < 0) */
    tfp.lr = 1;
    twp.physics.vel_air.x = -5.0F;
    wpMainReflectorSetLR(&twp, &tfp);               /* -5*1 < 0 -> flip */
    CHECK_EQF(twp.physics.vel_air.x, 5.0F);
    wpMainReflectorSetLR(&twp, &tfp);               /* 5*1 >= 0 -> keep */
    CHECK_EQF(twp.physics.vel_air.x, 5.0F);
    tfp.lr = -1;
    wpMainReflectorSetLR(&twp, &tfp);               /* 5*-1 < 0 -> flip */
    CHECK_EQF(twp.physics.vel_air.x, -5.0F);

    /* wpMainClearAttackRecord: every one of the 4 records reset -- gobj
     * NULL, all interact flags clear, rehit timer 0, group_id 7 */
    for (i = 0; i < ARRAY_COUNT(twp.attack_coll.attack_records); i++)
    {
        twp.attack_coll.attack_records[i].victim_gobj = (GObj *)0x1;
        twp.attack_coll.attack_records[i].victim_flags.is_interact_hurt = TRUE;
        twp.attack_coll.attack_records[i].victim_flags.timer_rehit = 9;
        twp.attack_coll.attack_records[i].victim_flags.group_id = 0;
    }
    wpMainClearAttackRecord(&twp);
    for (i = 0; i < ARRAY_COUNT(twp.attack_coll.attack_records); i++)
    {
        CHECK(twp.attack_coll.attack_records[i].victim_gobj == NULL);
        CHECK(twp.attack_coll.attack_records[i].victim_flags.is_interact_hurt == FALSE);
        CHECK(twp.attack_coll.attack_records[i].victim_flags.is_interact_shield == FALSE);
        CHECK(twp.attack_coll.attack_records[i].victim_flags.is_interact_reflect == FALSE);
        CHECK(twp.attack_coll.attack_records[i].victim_flags.is_interact_absorb == FALSE);
        CHECK(twp.attack_coll.attack_records[i].victim_flags.timer_rehit == 0);
        CHECK(twp.attack_coll.attack_records[i].victim_flags.group_id == 7);
    }
}

/* wpProcess -- the weapon's per-frame procs. The three GObj procs
 * (ProcWeaponMain / ProcSearchHitWeapon / ProcHitCollisions) need a live
 * scene (the gGCCommonLinks weapon chain, gMPCollisionGroundData, the
 * per-weapon proc hooks MakeWeapon wires) that no weapon exists to build
 * until MakeWeapon lands; this drives the pure record/position helpers
 * under them against hand-computed decomp values, on stack GObj/WPStruct/
 * DObj the same way the decomp reaches them through wpGetStruct/
 * DObjGetStruct (obj->user_data.p, obj->obj). */
static void test_wp_process(void)
{
    WPStruct twp;
    DObj tdobj;
    GObj tg;
    Vec3f off;
    WPAttackColl *ac;
    GObj victimA, victimB;
    s32 i;

    /* wpProcessUpdateHitOffsets: scale x/y, rotate about Z, add translate.
     * scale (2,3), rotate.z 0 (identity), translate (10,20,30) on (5,7,9):
     * (5*2, 7*3, 9) = (10,21,9) -> +translate = (20,41,39) */
    tdobj.scale.vec.f.x = 2.0F; tdobj.scale.vec.f.y = 3.0F; tdobj.scale.vec.f.z = 1.0F;
    tdobj.rotate.vec.f.z = 0.0F;
    tdobj.translate.vec.f.x = 10.0F; tdobj.translate.vec.f.y = 20.0F; tdobj.translate.vec.f.z = 30.0F;
    off.x = 5.0F; off.y = 7.0F; off.z = 9.0F;
    wpProcessUpdateHitOffsets(&tdobj, &off);
    CHECK_NEAR(off.x, 20.0F, 1e-4F);
    CHECK_NEAR(off.y, 41.0F, 1e-4F);
    CHECK_NEAR(off.z, 39.0F, 1e-4F);
    /* a 90-deg Z rotation sends (1,0) -> (0,1), scale 1, translate 0 */
    tdobj.scale.vec.f.x = tdobj.scale.vec.f.y = tdobj.scale.vec.f.z = 1.0F;
    tdobj.rotate.vec.f.z = 3.14159265F / 2.0F;
    tdobj.translate.vec.f.x = tdobj.translate.vec.f.y = tdobj.translate.vec.f.z = 0.0F;
    off.x = 1.0F; off.y = 0.0F; off.z = 0.0F;
    wpProcessUpdateHitOffsets(&tdobj, &off);
    CHECK_NEAR(off.x, 0.0F, 1e-4F);
    CHECK_NEAR(off.y, 1.0F, 1e-4F);
    CHECK_NEAR(off.z, 0.0F, 1e-4F);

    /* wpProcessUpdateAttackRecords: takes a GObj -> wpGetStruct(user_data.p).
     * When attack_state is Off nothing runs; otherwise a live record with a
     * positive rehit timer counts down, and clears itself on reaching 0. */
    tg.user_data.p = &twp;
    ac = &twp.attack_coll;
    for (i = 0; i < ARRAY_COUNT(ac->attack_records); i++)
    {
        ac->attack_records[i].victim_gobj = NULL;
        ac->attack_records[i].victim_flags.timer_rehit = 0;
    }
    ac->attack_records[0].victim_gobj = &victimA;
    ac->attack_records[0].victim_flags.timer_rehit = 2;
    ac->attack_records[0].victim_flags.group_id = 3;
    ac->attack_state = nGMAttackStateOff;               /* Off -> untouched */
    wpProcessUpdateAttackRecords(&tg);
    CHECK(ac->attack_records[0].victim_flags.timer_rehit == 2);
    CHECK(ac->attack_records[0].victim_gobj == &victimA);
    ac->attack_state = nGMAttackStateNew;               /* on -> count down */
    wpProcessUpdateAttackRecords(&tg);                  /* 2 -> 1, still held */
    CHECK(ac->attack_records[0].victim_flags.timer_rehit == 1);
    CHECK(ac->attack_records[0].victim_gobj == &victimA);
    wpProcessUpdateAttackRecords(&tg);                  /* 1 -> 0, cleared */
    CHECK(ac->attack_records[0].victim_gobj == NULL);
    CHECK(ac->attack_records[0].victim_flags.is_interact_hurt == FALSE);
    CHECK(ac->attack_records[0].victim_flags.is_interact_shield == FALSE);
    CHECK(ac->attack_records[0].victim_flags.is_interact_reflect == FALSE);
    CHECK(ac->attack_records[0].victim_flags.is_interact_absorb == FALSE);
    CHECK(ac->attack_records[0].victim_flags.group_id == 7);

    /* wpProcessSetHitInteractStats: a fresh victim takes the first empty
     * slot; a known victim updates its slot in place (no new slot). */
    for (i = 0; i < ARRAY_COUNT(ac->attack_records); i++)
    {
        ac->attack_records[i].victim_gobj = NULL;
        ac->attack_records[i].victim_flags.is_interact_hurt = FALSE;
        ac->attack_records[i].victim_flags.is_interact_shield = FALSE;
        ac->attack_records[i].victim_flags.timer_rehit = 0;
        ac->attack_records[i].victim_flags.group_id = 7;
    }
    wpProcessSetHitInteractStats(ac, &victimA, nGMHitTypeDamage, 0);
    CHECK(ac->attack_records[0].victim_gobj == &victimA);   /* first empty slot */
    CHECK(ac->attack_records[0].victim_flags.is_interact_hurt == TRUE);
    /* a second, different victim goes to the next slot */
    wpProcessSetHitInteractStats(ac, &victimB, nGMHitTypeAttack, 5);
    CHECK(ac->attack_records[1].victim_gobj == &victimB);
    CHECK(ac->attack_records[1].victim_flags.group_id == 5);
    /* the known victimA updates in place -- shield flag joins hurt, no new slot */
    wpProcessSetHitInteractStats(ac, &victimA, nGMHitTypeShieldRehit, 0);
    CHECK(ac->attack_records[0].victim_gobj == &victimA);
    CHECK(ac->attack_records[0].victim_flags.is_interact_hurt == TRUE);
    CHECK(ac->attack_records[0].victim_flags.is_interact_shield == TRUE);
    CHECK(ac->attack_records[0].victim_flags.timer_rehit == WEAPON_REHIT_TIME_DEFAULT);
    CHECK(ac->attack_records[2].victim_gobj == NULL);       /* slot 2 still free */
}

/* wpdisplay: the display-mode dispatch in wpDisplayMain, and PK Thunder's
 * colour tables + RGBA packing. The four renderers and the coloured draw
 * need a live weapon model (wpManagerMakeWeapon; PK Thunder
 * comes with Ness), so what is exercised here is wpDisplayMain's branch
 * logic on a stack WPStruct -- driven with a counting fake proc_display --
 * and the two colour words wpDisplayPKThunderSetColors records on the host
 * arm. */
extern SYColorRGB dWPDisplayPKThunderPrimColors[];
extern SYColorRGB dWPDisplayPKThunderEnvColors[];
extern u32 gWPDisplayPKThunderLastPrim;
extern u32 gWPDisplayPKThunderLastEnv;

static int sWpDisplayProcCount;
static GObj *sWpDisplayProcLast;
static void wp_display_fake_proc(GObj *g)
{
    sWpDisplayProcCount++;
    sWpDisplayProcLast = g;
}

static void test_wp_display(void)
{
    WPStruct twp;
    GObj tg;

    memset(&twp, 0, sizeof twp);
    tg.user_data.p = &twp;

    /* Colour tables verbatim (0x80188E10 / 0x80188E1C). */
    CHECK(dWPDisplayPKThunderPrimColors[0].r == 0x5E && dWPDisplayPKThunderPrimColors[0].g == 0xA3 && dWPDisplayPKThunderPrimColors[0].b == 0xFF);
    CHECK(dWPDisplayPKThunderPrimColors[3].r == 0xB3 && dWPDisplayPKThunderPrimColors[3].g == 0xF1 && dWPDisplayPKThunderPrimColors[3].b == 0xFF);
    CHECK(dWPDisplayPKThunderEnvColors[0].r == 0x3A && dWPDisplayPKThunderEnvColors[0].g == 0x00 && dWPDisplayPKThunderEnvColors[0].b == 0x83);
    CHECK(dWPDisplayPKThunderEnvColors[3].r == 0xA7 && dWPDisplayPKThunderEnvColors[3].g == 0x74 && dWPDisplayPKThunderEnvColors[3].b == 0xF8);

    /* Master mode always draws -- proc runs once -- whatever the hitbox. */
    twp.display_mode = nDBDisplayModeMaster;
    twp.attack_coll.attack_state = nGMAttackStateNew;
    sWpDisplayProcCount = 0;
    sWpDisplayProcLast = NULL;
    wpDisplayMain(&tg, wp_display_fake_proc);
    CHECK(sWpDisplayProcCount == 1);
    CHECK(sWpDisplayProcLast == &tg);

    /* A develop hit mode with a live hitbox diverts to the collision
     * visualiser -- proc does NOT run. */
    twp.display_mode = nDBDisplayModeHitCollisionFill;
    twp.attack_coll.attack_state = nGMAttackStateNew;
    sWpDisplayProcCount = 0;
    wpDisplayMain(&tg, wp_display_fake_proc);
    CHECK(sWpDisplayProcCount == 0);

    /* ...but the same mode with the hitbox off takes the "|| attack_state
     * == Off" arm and draws normally. */
    twp.attack_coll.attack_state = nGMAttackStateOff;
    sWpDisplayProcCount = 0;
    wpDisplayMain(&tg, wp_display_fake_proc);
    CHECK(sWpDisplayProcCount == 1);

    /* MapCollision mode draws (proc once), then the diamond visualiser. */
    twp.display_mode = nDBDisplayModeMapCollision;
    twp.attack_coll.attack_state = nGMAttackStateNew;
    sWpDisplayProcCount = 0;
    wpDisplayMain(&tg, wp_display_fake_proc);
    CHECK(sWpDisplayProcCount == 1);

    /* PK Thunder: trail_id indexes both tables and packs RGBA (alpha FF);
     * Master mode so it draws, the host arm records the two words. */
    twp.display_mode = nDBDisplayModeMaster;
    twp.attack_coll.attack_state = nGMAttackStateNew;
    twp.weapon_vars.pkthunder_trail.trail_id = 2;
    wpDisplayPKThunderProcDisplay(&tg);
    CHECK(gWPDisplayPKThunderLastPrim == 0xC2D9FFFFU);  /* prim[2] C2 D9 FF, a FF */
    CHECK(gWPDisplayPKThunderLastEnv == 0x8633D9FFU);   /* env[2]  86 33 D9, a FF */
}

/* wpmap.c: the wall-bounce math a launched weapon runs. The mpProcess*
 * collision procs (wpMapProcAll etc.) need a live stage map, so they are
 * exercised in-game once MakeWeapon wires them; here we drive the pure
 * WPStruct math -- lbCommonSim2D's sign, wpMapCheckAllRebound's reflect/
 * scale/position, and the ground/air setters -- against hand values. */
static void test_wp_map(void)
{
    WPStruct twp;
    DObj td;
    GObj tg;
    Vec3f pos;
    sb32 r;

    /* lbCommonSim2D: cosine similarity a.b / (|a|+|b|). {3,4}.{1,0} =
     * 3 / (5 + 1) = 0.5; a launch heading into a +x wall normal is < 0. */
    {
        Vec3f a = {3.0F, 4.0F, 0.0F}, n = {1.0F, 0.0F, 0.0F};
        CHECK(fabsf(lbCommonSim2D(&a, &n) - 0.5F) < 1e-5F);
        a.x = -4.0F; a.y = 0.0F;
        CHECK(lbCommonSim2D(&a, &n) < 0.0F);          /* -4 / 5 = -0.8 */
    }

    memset(&twp, 0, sizeof twp);
    memset(&td, 0, sizeof td);
    tg.obj = &td;
    tg.user_data.p = &twp;
    td.translate.vec.f.x = 10.0F;
    td.translate.vec.f.y = 20.0F;
    td.translate.vec.f.z = 30.0F;
    twp.coll_data.map_coll.width = 2.0F;
    twp.coll_data.map_coll.center = 1.0F;

    /* A left wall newly touched this frame (mask_prev clear -> mask_curr
     * set), its normal facing +x, the launch heading into it (vel.x < 0):
     * reflect -4 across {1,0} -> +4, scale by 0.8 -> 3.2, and report the
     * contact at (x+width, y+center, z). */
    twp.coll_data.mask_prev = 0;
    twp.coll_data.mask_curr = MAP_FLAG_LWALL;
    twp.coll_data.lwall_angle.x = 1.0F;
    twp.coll_data.lwall_angle.y = 0.0F;
    twp.physics.vel_air.x = -4.0F;
    twp.physics.vel_air.y = 0.0F;
    r = wpMapCheckAllRebound(&tg, MAP_FLAG_MAIN_MASK, 0.8F, &pos);
    CHECK(r == TRUE);
    CHECK(fabsf(twp.physics.vel_air.x - 3.2F) < 1e-4F);
    CHECK(fabsf(twp.physics.vel_air.y - 0.0F) < 1e-4F);
    CHECK(fabsf(pos.x - 12.0F) < 1e-4F);   /* 10 + width 2 */
    CHECK(fabsf(pos.y - 21.0F) < 1e-4F);   /* 20 + center 1 */
    CHECK(fabsf(pos.z - 30.0F) < 1e-4F);

    /* Same wall, but already touched last frame (mask_prev == mask_curr):
     * not "newly" collided, so no reflect and FALSE. */
    twp.coll_data.mask_prev = MAP_FLAG_LWALL;
    twp.coll_data.mask_curr = MAP_FLAG_LWALL;
    twp.physics.vel_air.x = -4.0F;
    r = wpMapCheckAllRebound(&tg, MAP_FLAG_MAIN_MASK, 0.8F, NULL);
    CHECK(r == FALSE);
    CHECK(fabsf(twp.physics.vel_air.x + 4.0F) < 1e-4F);  /* untouched */

    /* Newly touched but moving away from the wall (sim >= 0): no bounce. */
    twp.coll_data.mask_prev = 0;
    twp.coll_data.mask_curr = MAP_FLAG_LWALL;
    twp.physics.vel_air.x = 4.0F;                        /* +x, away from +x normal */
    r = wpMapCheckAllRebound(&tg, MAP_FLAG_MAIN_MASK, 0.8F, NULL);
    CHECK(r == FALSE);
    CHECK(fabsf(twp.physics.vel_air.x - 4.0F) < 1e-4F);

    /* Ground/air setters: ground velocity = air x-velocity * facing. */
    twp.lr = 1;
    twp.physics.vel_air.x = 3.2F;
    wpMapSetGround(&twp);
    CHECK(twp.ga == nMPKineticsGround);
    CHECK(fabsf(twp.physics.vel_ground - 3.2F) < 1e-4F);
    twp.lr = -1;
    wpMapSetGround(&twp);
    CHECK(fabsf(twp.physics.vel_ground + 3.2F) < 1e-4F); /* 3.2 * -1 */
    wpMapSetAir(&twp);
    CHECK(twp.ga == nMPKineticsAir);
}

/* src/dc/wpmanager.c wpManagerMakeWeapon: the whole spawn but the model
 * (which DIVERGES to a pack table empty until a fighter's weapon is
 * exported; the host arm builds a stub DObj so every field-fill and proc
 * wiring is checked here). The WPAttributes come through lbRelocGetFileData
 * over a caller-owned buffer, exactly as they come out of the fighter's
 * data file in game. Each parent arm (ground/fighter/weapon/item) is
 * driven with a stack parent whose struct MakeWeapon copies from. */
/* The two DObjDesc-tree weapon packs: the first
 * weapon models in this port that are trees rather than one display list
 * bound to one DObj, and the first weapon packs with an animation.
 *
 * Its WPDesc sets WEAPON_FLAG_DOBJDESC, so wpManagerMakeWeapon takes
 * gcSetupCustomDObjs' branch; the pack it needs is the shape a fighter's
 * is, not the flat quads of the other weapons. The tumble is an AnimJoint the
 * pack carries, not anything wplinkboomerang.c does. */
static void test_tree_weapon_packs(void)
{
    Fighter pack;
    int pal_bank = 0;

    memset(&pack, 0, sizeof(pack));
    CHECK(fighter_load(&pack, "wplinkboomerang.mdl", &pal_bank) == 0);

    if (pack.hd == NULL)
    {
        return;
    }
    /* three joints out of the DObjDesc tree, where every flat weapon has
     * exactly one */
    CHECK(pack.hd->joint_count == 3);
    CHECK(pack.hd->vert_count == 12);
    CHECK(pack.hd->tri_count == 8);
    CHECK(pack.hd->batch_count == 2);
    CHECK(pack.hd->tex_count == 1);
    CHECK(pack.hd->pal_count == 0);
    /* and the one thing no other weapon pack has: an animation */
    CHECK(pack.hd->anim_count == 1);
    CHECK(pack.anims != NULL);
    /* the two batches are two different materials, which is the other
     * thing a tree buys over a flat quad: joint 1 carries the boomerang
     * itself, a textured pair of triangles, and joint 2 under it carries
     * six untextured ones -- FPackBatch.tex -1, the Blaster's no-texel
     * path -- which is its motion trail. Both are shaded and unlit. */
    CHECK(pack.batches[0].tex >= 0);
    CHECK(pack.batches[0].shaded == 2);
    CHECK(pack.batches[1].tex == -1);
    CHECK(pack.batches[1].shaded == 2);
    CHECK(fabsf(pack.hd->radius - 204.4f) < 0.5f);

    fighter_release(&pack);

    /* ---- Kirby's Final Cutter blade: the same tree path
     * with the other half of the flags word set. Its geometry hangs off
     * DObjDLLink lists rather than each node's own dl pointer, which is
     * how the game picks what to draw; both of its nodes carry exactly
     * one link, so the pack holds nothing the game would not draw. Its
     * texture is 16x8, the smallest any weapon here has. ---- */
    pal_bank = 0;
    memset(&pack, 0, sizeof(pack));
    CHECK(fighter_load(&pack, "wpkirbycutter.mdl", &pal_bank) == 0);

    if (pack.hd == NULL)
    {
        return;
    }
    CHECK(pack.hd->joint_count == 2);
    CHECK(pack.hd->vert_count == 7);
    CHECK(pack.hd->tri_count == 3);
    CHECK(pack.hd->batch_count == 2);
    CHECK(pack.hd->tex_count == 1);
    CHECK(pack.hd->anim_count == 1);
    /* one untextured triangle and two textured ones, the boomerang's
     * shape again -- the blade and the sweep behind it */
    CHECK(pack.batches[0].tex == -1);
    CHECK(pack.batches[1].tex >= 0);
    CHECK(fabsf(pack.hd->radius - 351.5f) < 0.5f);

    fighter_release(&pack);

    /* ---- Pikachu's ground Thunder Jolt: the same tree
     * with links again, and the first weapon pack whose joints carry
     * MObjs -- six leaves, one sprite array each -- with a MatAnimJoint
     * to step through them. Its six arrays overlap in the file, and one
     * of the six steps 0 -> 2 without ever setting 1, where the shared
     * pool holds a genuine NULL; the packer fills that never-selected
     * frame with frame 0, so every MObj still has a contiguous run of
     * three for the runtime to index. ---- */
    pal_bank = 0;
    memset(&pack, 0, sizeof(pack));
    CHECK(fighter_load(&pack, "wppikachugjolt.mdl", &pal_bank) == 0);

    if (pack.hd == NULL)
    {
        return;
    }
    CHECK(pack.hd->joint_count == 8);
    CHECK(pack.hd->vert_count == 18);
    CHECK(pack.hd->tri_count == 6);
    CHECK(pack.hd->batch_count == 6);
    CHECK(pack.hd->anim_count == 1);
    CHECK(pack.mobjs != NULL);

    if (pack.mobjs != NULL)
    {
        int k;

        CHECK(pack.mobjs->mobj_count == 6);
        /* eighteen texture entries: six runs of three, each MObj's own
         * contiguous frames */
        CHECK(pack.hd->tex_count == 18);

        for (k = 0; k < (int)pack.mobjs->mobj_count; k++)
        {
            const FPackMObjSub *sub = &pack.mobj_subs[k];

            CHECK(sub->tex_count == 3);
            CHECK(sub->tex_first == k * 3);
        }
        /* every batch names an MObj -- the thing that made this weapon
         * different from the two trees before it */
        for (k = 0; k < (int)pack.hd->batch_count; k++)
        {
            CHECK(pack.mobj_batch[k] >= 0);
            CHECK(pack.mobj_batch[k] < (int)pack.mobjs->mobj_count);
        }
    }
    CHECK(fabsf(pack.hd->radius - 754.4f) < 0.5f);

    fighter_release(&pack);

    /* ---- Pikachu's Thunder, the last weapon model in the
     * game this port could not pack. Its WPDesc flags are 0x02, the one
     * shape where WPAttributes.data is a DObjDLLink array rather than a
     * display list -- gcAddDObjForGObj puts it in the same union field
     * gcDrawDObjDLLinksForGObj walks. Four sprite frames on one MObj and
     * NO MatAnimJoint: the weapon's own code picks among them. The head
     * and the trail share this one pack. ---- */
    pal_bank = 0;
    memset(&pack, 0, sizeof(pack));
    CHECK(fighter_load(&pack, "wppikachuthunder.mdl", &pal_bank) == 0);

    if (pack.hd == NULL)
    {
        return;
    }
    CHECK(pack.hd->joint_count == 1);
    CHECK(pack.hd->vert_count == 4);
    CHECK(pack.hd->tri_count == 2);
    CHECK(pack.hd->batch_count == 1);
    /* four frames of one picture, and no animation to step them */
    CHECK(pack.hd->tex_count == 4);
    CHECK(pack.hd->anim_count == 0);
    CHECK(pack.mobjs != NULL);

    if (pack.mobjs != NULL)
    {
        CHECK(pack.mobjs->mobj_count == 1);
        CHECK(pack.mobj_subs[0].tex_first == 0);
        CHECK(pack.mobj_subs[0].tex_count == 4);
    }
    CHECK(fabsf(pack.hd->radius - 540.0f) < 0.5f);

    fighter_release(&pack);
}


/* The entry vehicles: Mario's warp pipe, Donkey
 * Kong's barrel and Samus's arrival point -- the first models this port
 * takes out of a FIGHTER's own Special2 file rather than an effect bank
 * or a weapon's.
 *
 * The arm of ftCommonAppearSetStatus that makes it needs a model bake
 * of its own for each vehicle. This covers that bake plus the arm, so a
 * Mario, a Luigi or a giant Mario rises out of a pipe. */
static void test_entry_vehicles(void)
{
    Fighter pack;
    int pal_bank = 0;
    int i, lit;

    memset(&pack, 0, sizeof(pack));
    CHECK(fighter_load(&pack, "efdokan.mdl", &pal_bank) == 0);

    if (pack.hd == NULL)
    {
        return;
    }
    /* three joints, forty-four triangles, two pictures and one animation:
     * a solid object, not the billboards every weapon pack holds */
    CHECK(pack.hd->joint_count == 3);
    /* 47, not 45: six of the pipe's vertices are rewritten by
     * gsSPModifyVertex, whose coordinates the RSP keeps unscaled while
     * the loaded ones carry gsSPTexture's 0xFFFF (0.99998) -- two of them
     * no longer land on a loaded vertex's bits (ssb_assets._modify_vertex) */
    CHECK(pack.hd->vert_count == 47);
    CHECK(pack.hd->tri_count == 44);
    CHECK(pack.hd->batch_count == 3);
    CHECK(pack.hd->tex_count == 2);
    CHECK(pack.hd->pal_count == 0);
    CHECK(pack.hd->anim_count == 1);
    CHECK(pack.anims != NULL);
    /* and LIT -- shaded 1 is N.L over vertex normals, which no weapon
     * model in this port uses and every fighter's does */
    for (i = 0; i < (int)pack.hd->batch_count; i++)
    {
        CHECK(pack.batches[i].shaded == 1);
        CHECK(pack.batches[i].tex >= 0);
    }
    CHECK(fabsf(pack.hd->radius - 247.5f) < 0.5f);

    fighter_release(&pack);

    /* ---- Donkey Kong's barrel: the pipe's shape again,
     * and its maker is the pipe's without even the file switch. ---- */
    pal_bank = 0;
    memset(&pack, 0, sizeof(pack));
    CHECK(fighter_load(&pack, "eftaru.mdl", &pal_bank) == 0);

    if (pack.hd == NULL)
    {
        return;
    }
    CHECK(pack.hd->joint_count == 2);
    CHECK(pack.hd->tri_count == 26);
    CHECK(pack.hd->batch_count == 2);
    CHECK(pack.hd->tex_count == 2);
    CHECK(pack.hd->anim_count == 1);
    for (i = 0; i < (int)pack.hd->batch_count; i++)
    {
        CHECK(pack.batches[i].shaded == 1);
    }
    CHECK(fabsf(pack.hd->radius - 310.5f) < 0.5f);
    fighter_release(&pack);

    /* ---- Samus's arrival point, two firsts at once: the
     * first model in this port with MORE THAN ONE DObjDLLink on a node
     * (the game draws both, one into each DL head), and the first MIXED
     * vehicle -- two lit batches and one flat quad for the glow, where
     * the pipe and the barrel are lit throughout. ---- */
    pal_bank = 0;
    memset(&pack, 0, sizeof(pack));
    CHECK(fighter_load(&pack, "efpoint.mdl", &pal_bank) == 0);

    if (pack.hd == NULL)
    {
        return;
    }
    CHECK(pack.hd->joint_count == 2);
    CHECK(pack.hd->vert_count == 60);
    CHECK(pack.hd->tri_count == 50);
    CHECK(pack.hd->batch_count == 3);
    CHECK(pack.hd->tex_count == 1);
    CHECK(pack.hd->anim_count == 1);

    lit = 0;
    for (i = 0; i < (int)pack.hd->batch_count; i++)
    {
        lit += (pack.batches[i].shaded == 1);
    }
    CHECK(lit == 2);
    CHECK(fabsf(pack.hd->radius - 210.1f) < 0.5f);
    fighter_release(&pack);

    /* ---- Kirby's entry star: the first effect pack in
     * this port with TWO animations. The star sweeps in from the side he
     * faces, and the game picks by writing a block offset into the shared
     * descriptor; the port picks by index instead, R first. It is also a
     * flat billboard with no lit batch at all -- the pipe and the barrel
     * are lit throughout and Samus's point is mixed, so there is no rule
     * about a vehicle's lighting. ---- */
    pal_bank = 0;
    memset(&pack, 0, sizeof(pack));
    CHECK(fighter_load(&pack, "efstar.mdl", &pal_bank) == 0);

    if (pack.hd == NULL)
    {
        return;
    }
    CHECK(pack.hd->joint_count == 2);
    CHECK(pack.hd->tri_count == 2);
    CHECK(pack.hd->batch_count == 1);
    CHECK(pack.hd->tex_count == 1);
    CHECK(pack.hd->anim_count == 2);
    CHECK(pack.anims != NULL);
    /* no lit batch at all */
    CHECK(pack.batches[0].shaded != 1);
    /* the two sweeps are different animations, not one baked twice --
     * two blocks at two offsets is not enough to say that, so the words
     * themselves are compared */
    CHECK(pack.anims[0].off_words != pack.anims[1].off_words);
    {
        const uint8_t *w0 = (const uint8_t *)pack.blob + pack.anims[0].off_words;
        const uint8_t *w1 = (const uint8_t *)pack.blob + pack.anims[1].off_words;
        uint32_t n = (pack.anims[0].nwords < pack.anims[1].nwords)
                         ? pack.anims[0].nwords : pack.anims[1].nwords;

        CHECK(n > 0);
        CHECK(memcmp(w0, w1, n * 4) != 0);
    }
    CHECK(fabsf(pack.hd->radius - 419.3f) < 0.5f);
    fighter_release(&pack);

    /* ---- Link's entry wave and the beam that comes down with it, the only
     * entrance in the game that makes two effects.
     * They are the first vehicles here with an MObjSub and a
     * MatAnimJoint -- and their MObjs carry NO sprite array, so what the
     * material animation moves is a colour or a texture offset rather
     * than a picture. That is why each MObj's texture run is one long
     * where the ground Thunder Jolt's six are three. ---- */
    {
        static const char *const kLink[2] = { "efwave.mdl", "efbeam.mdl" };
        static const float kRad[2] = { 457.4f, 1665.3f };
        int w;

        for (w = 0; w < 2; w++)
        {
            pal_bank = 0;
            memset(&pack, 0, sizeof(pack));
            CHECK(fighter_load(&pack, kLink[w], &pal_bank) == 0);

            if (pack.hd == NULL)
            {
                continue;
            }
            CHECK(pack.hd->joint_count == 2);
            CHECK(pack.hd->vert_count == 18);
            CHECK(pack.hd->tri_count == 16);
            CHECK(pack.hd->batch_count == 1);
            CHECK(pack.hd->tex_count == 1);
            CHECK(pack.hd->anim_count == 1);
            CHECK(pack.mobjs != NULL);

            if (pack.mobjs != NULL)
            {
                CHECK(pack.mobjs->mobj_count == 1);
                /* one picture, not a flipbook */
                CHECK(pack.mobj_subs[0].tex_count == 1);
                /* and a material animation over it, which is the whole
                 * reason the MObj is there */
                CHECK(pack.mobjs->words > 0);
            }
            CHECK(fabsf(pack.hd->radius - kRad[w]) < 0.5f);
            fighter_release(&pack);
        }
    }

    /* ---- Yoshi's entry egg, the seventh vehicle and two
     * more firsts.
     *
     * Its EFDesc's flags are 0x1 with no 0x4, the only vehicle without
     * it, and efmanager.c:1992-2004 reads that bit to decide what
     * o_dobjsetup POINTS AT: with 0x4 a DObjDesc tree, without it a bare
     * display list. So the generated header's name
     * llYoshiSpecial2EntryEggDObjDesc says the opposite of what the field
     * is -- llLinkSpecial3BoomerangDLDisplayList in reverse --
     * and the packer builds by hand the one-node tree the game builds by
     * hand. One joint is how that shows here.
     *
     * And its MObj steps a SPRITE ARRAY where Link's two step a colour:
     * two pictures of the egg, baked contiguously as a run its FPackMObjs
     * indexes, which is why tex_count is 2 with one batch drawing it. ---- */
    pal_bank = 0;
    memset(&pack, 0, sizeof(pack));
    CHECK(fighter_load(&pack, "efentryegg.mdl", &pal_bank) == 0);

    if (pack.hd == NULL)
    {
        return;
    }
    /* the flat display list: one node under the root, not a tree */
    CHECK(pack.hd->joint_count == 1);
    CHECK(pack.hd->vert_count == 4);
    CHECK(pack.hd->tri_count == 2);
    CHECK(pack.hd->batch_count == 1);
    CHECK(pack.hd->anim_count == 1);
    /* the flipbook: two frames for one batch */
    CHECK(pack.hd->tex_count == 2);
    CHECK(pack.mobjs != NULL);

    if (pack.mobjs != NULL)
    {
        CHECK(pack.mobjs->mobj_count == 1);
        CHECK(pack.mobj_subs[0].tex_count == 2);
        CHECK(pack.mobj_subs[0].tex_first == 0);
        /* the batch is repointed at its run's frame 0 */
        CHECK(pack.batches[0].tex == pack.mobj_subs[0].tex_first);
        CHECK(pack.mobjs->words > 0);
    }
    CHECK(fabsf(pack.hd->radius - 127.3f) < 0.5f);
    fighter_release(&pack);

    /* ---- Fox's Arwing, the eighth vehicle and by a
     * distance the largest: twelve joints where no vehicle before it
     * passed three, twenty-seven batches and twenty-one textures.
     *
     * Two things are new. Its two AnimJoint arrays live in Fox's
     * Special2 and its tree in his Special3 -- the first vehicle whose
     * animation is not in the file its geometry is in, which the maker
     * says out loud (lbCommonSetupTreeDObjs reads gFTDataFoxSpecial3 and
     * lbCommonAddDObjAnimJointAll reads gFTDataFoxSpecial2). And it is
     * the first pack in this exporter to need bake_retry: a tile whose
     * clamp mode is set but whose hull samples V 120 of an extent of 32
     * has to be re-baked with that tile forced to a clamped extent, the
     * way the fighters' own models have been.
     *
     * The two arrays are the pack's two animations, R first, and they
     * really are different: four of the twelve joints are driven in each
     * and the scripts differ word for word. ---- */
    pal_bank = 0;
    memset(&pack, 0, sizeof(pack));
    CHECK(fighter_load(&pack, "efarwing.mdl", &pal_bank) == 0);

    if (pack.hd == NULL)
    {
        return;
    }
    CHECK(pack.hd->joint_count == 12);
    CHECK(pack.hd->vert_count == 230);
    CHECK(pack.hd->tri_count == 116);
    CHECK(pack.hd->batch_count == 27);
    CHECK(pack.hd->tex_count == 21);
    CHECK(pack.hd->anim_count == 2);
    CHECK(pack.anims != NULL);
    /* Mixed, like Samus's point and unlike the pipe and the barrel:
     * twenty-one of the twenty-seven batches are shaded and six are not.
     * The packer's own line reports every batch lit, which is a
     * different bit -- G_LIGHTING in the geometry mode rather than
     * shaded 1, N.L over vertex normals -- and the two are worth not
     * confusing. */
    lit = 0;
    for (i = 0; i < (int)pack.hd->batch_count; i++)
    {
        lit += (pack.batches[i].shaded == 1);
    }
    CHECK(lit == 21);
    /* the two sweeps are two animations and not one baked twice */
    CHECK(pack.anims[0].off_words != pack.anims[1].off_words);
    {
        const uint8_t *w0 = (const uint8_t *)pack.blob + pack.anims[0].off_words;
        const uint8_t *w1 = (const uint8_t *)pack.blob + pack.anims[1].off_words;
        uint32_t n = (pack.anims[0].nwords < pack.anims[1].nwords)
                         ? pack.anims[0].nwords : pack.anims[1].nwords;

        CHECK(n > 0);
        CHECK(memcmp(w0, w1, n * 4) != 0);
    }
    /* and each drives four of the twelve joints -- 0, 1, 6 and 10, the
     * last of which is the node the maker hangs its billboard on */
    {
        int a, drawn[2];

        for (a = 0; a < 2; a++)
        {
            const int32_t *ent =
                (const int32_t *)((const uint8_t *)pack.blob
                                  + pack.anims[a].off_entries);

            drawn[a] = 0;
            for (i = 0; i < (int)pack.hd->joint_count; i++)
            {
                drawn[a] += (ent[i] >= 0);
            }
            CHECK(ent[0] >= 0);
            CHECK(ent[1] >= 0);
            CHECK(ent[6] >= 0);
            CHECK(ent[10] >= 0);
        }
        CHECK(drawn[0] == 4);
        CHECK(drawn[1] == 4);
    }
    CHECK(fabsf(pack.hd->radius - 1215.7f) < 0.5f);
    fighter_release(&pack);

    /* ---- Captain Falcon's Blue Falcon, the ninth and
     * last vehicle with an arm of its own.
     *
     * One animation, not two: the car turns around by a rotate on the
     * root rather than by a second sweep. What is new is what is IN that
     * animation. The game applies the AnimJoint array at 0x6200 over the
     * tree and then overwrites eight of its entries by hand -- four
     * passes of two siblings, the odd one taking the wheel script at
     * 0x6518 and the even one the script at 0x6598. gcAddDObjAnimJoint
     * replaces rather than appends and the maker writes last, so those
     * eight writes are what the game plays, and the pack bakes them into
     * the array (EFENTRY_ANIMPATCH). The array on its own drives three
     * joints; the baked one drives eleven of the twelve.
     *
     * That is the mirror image of the Arwing above, whose maker wrote
     * FIRST and was overwritten. ---- */
    pal_bank = 0;
    memset(&pack, 0, sizeof(pack));
    CHECK(fighter_load(&pack, "efcar.mdl", &pal_bank) == 0);

    if (pack.hd == NULL)
    {
        return;
    }
    CHECK(pack.hd->joint_count == 12);
    CHECK(pack.hd->vert_count == 404);
    CHECK(pack.hd->tri_count == 160);
    CHECK(pack.hd->batch_count == 22);
    CHECK(pack.hd->tex_count == 13);
    CHECK(pack.hd->anim_count == 1);
    CHECK(pack.anims != NULL);
    {
        const int32_t *ent =
            (const int32_t *)((const uint8_t *)pack.blob
                              + pack.anims[0].off_entries);
        int drawn = 0;

        for (i = 0; i < (int)pack.hd->joint_count; i++)
        {
            drawn += (ent[i] >= 0);
        }
        /* eleven of twelve: the array's three plus the maker's eight */
        CHECK(drawn == 11);
        CHECK(ent[0] < 0);
        /* and the eight the maker wrote alternate between exactly two
         * scripts -- the wheel pair -- which is the claim the patch
         * makes and the thing a wrong node index would break */
        CHECK(ent[3] == ent[5]);
        CHECK(ent[5] == ent[7]);
        CHECK(ent[7] == ent[9]);
        CHECK(ent[4] == ent[6]);
        CHECK(ent[6] == ent[8]);
        CHECK(ent[8] == ent[10]);
        CHECK(ent[3] != ent[4]);
        /* the three the array itself drives are none of those two */
        CHECK(ent[1] != ent[3] && ent[1] != ent[4]);
        CHECK(ent[2] != ent[3] && ent[2] != ent[4]);
        CHECK(ent[11] != ent[3] && ent[11] != ent[4]);
    }
    CHECK(fabsf(pack.hd->radius - 1119.1f) < 0.5f);
    fighter_release(&pack);

    /* ---- Yoshi's Egg Lay egg. Not a vehicle -- it is
     * here because it is the same shape as one, a DObjDesc tree in a
     * fighter's own file with AnimJoints over it and no MObj -- and it
     * closes the older of the two stubs.
     *
     * THREE animations, a first for this port: the throw the descriptor
     * names, and the wait and the break that efManagerYoshiEggLaySetAnim
     * picks between. The game picks by writing a block pointer out of
     * dEFManagerYoshiEggLayAnimJoints; all three are baked side by side
     * here and the port's copy of that table holds pack indexes, throw
     * first because index 0 is the one efManagerAddModel applies.
     *
     * It is tiny on purpose: one quad, drawn at the size the maker
     * writes into it out of dFTCommonYoshiEggDamageCollDescs, which is
     * per fighter kind. A radius of 3.4 is the unit egg. ---- */
    pal_bank = 0;
    memset(&pack, 0, sizeof(pack));
    CHECK(fighter_load(&pack, "efegglay.mdl", &pal_bank) == 0);

    if (pack.hd == NULL)
    {
        return;
    }
    CHECK(pack.hd->joint_count == 2);
    CHECK(pack.hd->vert_count == 4);
    CHECK(pack.hd->tri_count == 2);
    CHECK(pack.hd->batch_count == 1);
    CHECK(pack.hd->tex_count == 1);
    CHECK(pack.hd->anim_count == 3);
    CHECK(pack.anims != NULL);
    CHECK(fabsf(pack.hd->radius - 3.4f) < 0.2f);
    {
        /* the three are three, not one baked three times */
        int a, b2;

        for (a = 0; a < 3; a++)
        {
            for (b2 = a + 1; b2 < 3; b2++)
            {
                const uint8_t *w0 = (const uint8_t *)pack.blob
                                    + pack.anims[a].off_words;
                const uint8_t *w1 = (const uint8_t *)pack.blob
                                    + pack.anims[b2].off_words;
                uint32_t n = (pack.anims[a].nwords < pack.anims[b2].nwords)
                                 ? pack.anims[a].nwords
                                 : pack.anims[b2].nwords;

                CHECK(n > 0);
                CHECK(memcmp(w0, w1, n * 4) != 0);
            }
            /* and each drives both joints, which is what makes the
             * egg's two halves move together */
            {
                const int32_t *ent =
                    (const int32_t *)((const uint8_t *)pack.blob
                                      + pack.anims[a].off_entries);

                CHECK(ent[0] >= 0);
                CHECK(ent[1] >= 0);
            }
        }
    }
    /* the port's own table names the wait and the break, in that order,
     * and neither is the throw the descriptor applies */
    {
        extern s32 dEFManagerYoshiEggLayAnimJoints[];

        CHECK(dEFManagerYoshiEggLayAnimJoints[0] == 1);
        CHECK(dEFManagerYoshiEggLayAnimJoints[1] == 2);
    }
    fighter_release(&pack);

    /* ---- Kirby's Final Cutter, four packs out of the one
     * file -- relocData 348, the same Special2 his entry star came from
     * -- and the last four effect stubs in this port that are not it/'s.
     *
     * The four differ in exactly the two ways a bake cares about. The
     * TRAIL is the only one with DL links. The BLADE (the descriptor
     * calls it Draw) is the only one whose o_anim_joint is zero and so
     * the only pack here with NO animation at all. And the two ARCS are
     * the only vehicle-shaped packs in this port with NO TEXTURE:
     * untextured triangles, which is what the boomerang's motion trail
     * is too, so the shape is not new even though it is rare. ---- */
    {
        static const char *const kCut[4] = { "efcutup.mdl", "efcutdown.mdl",
                                             "efcutdraw.mdl",
                                             "efcuttrail.mdl" };
        static const u32 kJoints[4] = { 5, 6, 2, 3 };
        static const u32 kTris[4]   = { 24, 24, 2, 3 };
        static const u32 kBatches[4] = { 3, 4, 1, 2 };
        static const u32 kTexs[4]   = { 0, 0, 1, 1 };
        static const u32 kAnims[4]  = { 1, 1, 0, 1 };
        static const u32 kTextured[4] = { 0, 0, 1, 1 };
        static const float kRad[4] = { 443.7f, 408.0f, 278.4f, 366.3f };
        int c;

        for (c = 0; c < 4; c++)
        {
            pal_bank = 0;
            memset(&pack, 0, sizeof(pack));
            CHECK(fighter_load(&pack, kCut[c], &pal_bank) == 0);

            if (pack.hd == NULL)
            {
                continue;
            }
            CHECK(pack.hd->joint_count == kJoints[c]);
            CHECK(pack.hd->tri_count == kTris[c]);
            CHECK(pack.hd->batch_count == kBatches[c]);
            CHECK(pack.hd->tex_count == kTexs[c]);
            CHECK(pack.hd->anim_count == kAnims[c]);
            CHECK(pack.hd->pal_count == 0);
            CHECK(fabsf(pack.hd->radius - kRad[c]) < 0.5f);

            /* How many batches name a texture, pinned per pack rather
             * than by a rule: the two arcs name none at all, the blade
             * is one textured quad, and the TRAIL is mixed -- one
             * textured batch and one untextured, which is the
             * boomerang's shape and the reason a rule
             * here would have been wrong for the third time this
             * milestone. */
            {
                int textured = 0;

                for (i = 0; i < (int)pack.hd->batch_count; i++)
                {
                    if (pack.batches[i].tex >= 0)
                    {
                        CHECK(pack.batches[i].tex < (int)pack.hd->tex_count);
                        textured++;
                    }
                    /* none of the four is lit: they are all flat */
                    CHECK(pack.batches[i].shaded != 1);
                }
                CHECK(textured == (int)kTextured[c]);
            }
            /* the blade has no animation and the other three have one
             * that drives something */
            if (kAnims[c] == 0)
            {
                CHECK(pack.anims == NULL || pack.hd->anim_count == 0);
            }
            else
            {
                const int32_t *ent;
                int drawn = 0;

                CHECK(pack.anims != NULL);
                ent = (const int32_t *)((const uint8_t *)pack.blob
                                        + pack.anims[0].off_entries);
                for (i = 0; i < (int)pack.hd->joint_count; i++)
                {
                    drawn += (ent[i] >= 0);
                }
                CHECK(drawn > 0);
                CHECK(pack.anims[0].nwords > 0);
            }
            fighter_release(&pack);
        }
    }
}


/* src/dc/wpmanager.c, the weapon pool's round trip.
 *
 * found that scvsbattle.c never called wpManagerAllocWeapons,
 * so sWPManagerStructsAllocFree was NULL in every battle and
 * wpManagerGetNextStructAlloc -- the FIRST line of wpManagerMakeWeapon --
 * returned NULL for every projectile the port had ever compiled. The call
 * is in now, at both of the scene's sites.
 *
 * That makes a whole path live that had never run, and this is the half
 * of it the host can hold still: spawn until the pool is empty, check
 * that running out is a NULL and not a crash -- which is exactly the
 * state the missing line made permanent, so it is worth being able to
 * tell the two apart -- and check that ejecting every weapon hands every
 * struct back. The other half, that a weapon really lives and expires on
 * the SH-4, is -DDB_WPLIFE_PROBE. */
static void test_weapon_pool_roundtrip(void)
{
    extern WPStruct *sWPManagerStructsAllocFree;
    WPAttributes attr;
    void *attr_base;
    WPDesc desc;
    Vec3f spawn = { 0.0F, 0.0F, 0.0F };
    GObj *made[WEAPON_ALLOC_MAX + 4];
    WPStruct *w;
    int free0, free1, n, nulls, i;

    wpManagerAllocWeapons();

    memset(&attr, 0, sizeof(attr));
    attr_base = &attr;
    memset(&desc, 0, sizeof(desc));
    desc.flags = 0;
    desc.kind = 0x321;
    desc.p_weapon = &attr_base;
    desc.o_attributes = 0;

    free0 = 0;
    for (w = sWPManagerStructsAllocFree; w != NULL; w = w->next)
    {
        free0++;
    }
    CHECK(free0 == WEAPON_ALLOC_MAX);

    /* spawn past the end of the pool */
    n = nulls = 0;
    for (i = 0; i < (int)(sizeof(made) / sizeof(made[0])); i++)
    {
        made[i] = wpManagerMakeWeapon(NULL, &desc, &spawn,
                                      WEAPON_FLAG_PARENT_GROUND);
        if (made[i] != NULL)
        {
            n++;
        }
        else
        {
            nulls++;
        }
    }
    /* the pool is the cap, and past it the game says no rather than
     * writing through a NULL -- the decomp's own answer, and the one
     * every spawn in this port gives */
    CHECK(n == WEAPON_ALLOC_MAX);
    CHECK(nulls == 4);
    CHECK(sWPManagerStructsAllocFree == NULL);

    /* ---- A TRAP worth pinning: gcEjectGObj is NOT how a weapon dies.
     * It takes the GObj away and leaves the WPStruct out of the pool
     * forever. wp/wpmain.c's wpMainDestroyWeapon is the pair -- stop the
     * sound, hand the struct back, THEN eject -- and it is what every
     * expiry path in the game calls. Ejecting one weapon the wrong way
     * here shows the difference. ---- */
    gcEjectGObj(made[0]);
    made[0] = NULL;
    free1 = 0;
    for (w = sWPManagerStructsAllocFree; w != NULL; w = w->next)
    {
        free1++;
    }
    CHECK(free1 == 0);                  /* the struct did NOT come back */

    /* and every one destroyed properly does */
    for (i = 1; i < (int)(sizeof(made) / sizeof(made[0])); i++)
    {
        if (made[i] != NULL)
        {
            wpMainDestroyWeapon(made[i]);
        }
    }
    free1 = 0;
    for (w = sWPManagerStructsAllocFree; w != NULL; w = w->next)
    {
        free1++;
    }
    CHECK(free1 == free0 - 1);          /* all but the one leaked above */

    /* the pool is usable again after the round trip: one more spawn */
    made[0] = wpManagerMakeWeapon(NULL, &desc, &spawn,
                                  WEAPON_FLAG_PARENT_GROUND);
    CHECK(made[0] != NULL);
    if (made[0] != NULL)
    {
        wpMainDestroyWeapon(made[0]);
    }
}


static void test_wp_make(void)
{
    WPAttributes attr;
    void *attr_base;
    WPDesc desc;
    Vec3f spawn = { 10.0F, 20.0F, 30.0F };
    GObj *wg;
    WPStruct *wp;
    GObj parent;                        /* a stack parent; only its user_data.p is read */

    sb32 (*const P_UPD)(GObj*) = (sb32 (*)(GObj*))(uintptr_t)0x1001;
    sb32 (*const P_MAP)(GObj*) = (sb32 (*)(GObj*))(uintptr_t)0x1002;
    sb32 (*const P_HIT)(GObj*) = (sb32 (*)(GObj*))(uintptr_t)0x1003;
    sb32 (*const P_SHD)(GObj*) = (sb32 (*)(GObj*))(uintptr_t)0x1004;
    sb32 (*const P_HOP)(GObj*) = (sb32 (*)(GObj*))(uintptr_t)0x1005;
    sb32 (*const P_SET)(GObj*) = (sb32 (*)(GObj*))(uintptr_t)0x1006;
    sb32 (*const P_REF)(GObj*) = (sb32 (*)(GObj*))(uintptr_t)0x1007;
    sb32 (*const P_ABS)(GObj*) = (sb32 (*)(GObj*))(uintptr_t)0x1008;

    /* fresh pool from the live scene heap (test_wp_pool drained it) */
    wpManagerAllocWeapons();

    memset(&attr, 0, sizeof(attr));
    attr.damage = 15;
    attr.element = 3;
    attr.size = 8;                      /* wp->attack_coll.size = size * 0.5 = 4 */
    attr.angle = 100;
    attr.knockback_scale = 50;
    attr.knockback_weight = 30;
    attr.knockback_base = 40;
    attr.shield_damage = 5;
    attr.sfx = 200;
    attr.priority = 2;
    attr.attack_count = 1;
    attr.can_setoff = 1;
    attr.can_rehit_item = 1;
    attr.can_rehit_fighter = 0;
    attr.can_hop = 1;
    attr.can_reflect = 1;
    attr.can_absorb = 0;
    attr.can_shield = 1;
    attr.attack_offsets[0].x = 1; attr.attack_offsets[0].y = 2; attr.attack_offsets[0].z = 3;
    attr.attack_offsets[1].x = 4; attr.attack_offsets[1].y = 5; attr.attack_offsets[1].z = 6;
    attr.map_coll_top = 7;
    attr.map_coll_center = 8;
    attr.map_coll_bottom = 9;
    attr.map_coll_width = 10;
    attr_base = &attr;

    memset(&desc, 0, sizeof(desc));
    desc.flags = 0;                     /* else branch -> wpDisplayDLHead1 */
    desc.kind = 0x123;
    desc.p_weapon = &attr_base;         /* lbRelocGetFileData: (*p_weapon)+o = &attr */
    desc.o_attributes = 0;
    desc.proc_update = P_UPD; desc.proc_map = P_MAP; desc.proc_hit = P_HIT;
    desc.proc_shield = P_SHD; desc.proc_hop = P_HOP; desc.proc_setoff = P_SET;
    desc.proc_reflector = P_REF; desc.proc_absorb = P_ABS;

    /* ---- GROUND arm: no parent, deterministic defaults ---- */
    wg = wpManagerMakeWeapon(NULL, &desc, &spawn, WEAPON_FLAG_PARENT_GROUND);
    CHECK(wg != NULL);
    wp = wpGetStruct(wg);
    CHECK(wp->weapon_gobj == wg);
    CHECK(wp->kind == 0x123);
    CHECK(wp->owner_gobj == NULL);
    CHECK(wp->team == WEAPON_TEAM_DEFAULT);
    CHECK(wp->player == WEAPON_PORT_DEFAULT);
    CHECK(wp->handicap == WEAPON_HANDICAP_DEFAULT);
    CHECK(wp->player_num == 0);
    CHECK(wp->lr == +1);
    CHECK(wp->display_mode == sWPManagerDisplayMode);
    CHECK(wp->attack_coll.motion_attack_id == nFTMotionAttackIDNone);
    CHECK(fabsf(wp->attack_coll.stale - WEAPON_STALE_DEFAULT) < 1e-4F);
    /* the attribute-fed fields (same for every arm) */
    /* MakeWeapon sets attack_state = New, then wpProcessUpdateHitPositions
     * (wired at the tail, attack_count == 1) advances it New -> Transfer --
     * so seeing Transfer proves both the set and that the tail ran. */
    CHECK(wp->attack_coll.attack_state == nGMAttackStateTransfer);
    CHECK(wp->attack_coll.damage == 15);
    CHECK(wp->attack_coll.element == 3);
    CHECK(fabsf(wp->attack_coll.size - 4.0F) < 1e-4F);
    CHECK(wp->attack_coll.angle == 100);
    CHECK(wp->attack_coll.knockback_scale == 50);
    CHECK(wp->attack_coll.knockback_weight == 30);
    CHECK(wp->attack_coll.knockback_base == 40);
    CHECK(wp->attack_coll.shield_damage == 5);
    CHECK(wp->attack_coll.fgm_id == 200);
    CHECK(wp->attack_coll.priority == 2);
    CHECK(wp->attack_coll.attack_count == 1);
    CHECK(wp->attack_coll.can_setoff == 1);
    CHECK(wp->attack_coll.can_rehit_item == 1);
    CHECK(wp->attack_coll.can_rehit_fighter == 0);
    CHECK(wp->attack_coll.can_rehit_shield == 0);
    CHECK(wp->attack_coll.can_hop == 1);
    CHECK(wp->attack_coll.can_reflect == 1);
    CHECK(wp->attack_coll.can_absorb == 0);
    CHECK(wp->attack_coll.can_not_heal == 0);
    CHECK(wp->attack_coll.can_shield == 1);
    CHECK(wp->attack_coll.interact_mask == GMHITCOLLISION_FLAG_ALL);
    CHECK(wp->attack_coll.offsets[0].x == 1.0F && wp->attack_coll.offsets[0].z == 3.0F);
    CHECK(wp->attack_coll.offsets[1].y == 5.0F);
    /* reset block */
    CHECK(wp->hit_normal_damage == 0 && wp->hit_shield_damage == 0);
    CHECK(wp->reflect_gobj == NULL && wp->absorb_gobj == NULL);
    CHECK(wp->group_id == 0);
    CHECK(wp->p_sfx == NULL && wp->sfx_id == 0);
    CHECK(wp->ga == nMPKineticsAir);
    /* collision data */
    CHECK(wp->coll_data.p_translate == &DObjGetStruct(wg)->translate.vec.f);
    CHECK(wp->coll_data.p_lr == &wp->lr);
    CHECK(wp->coll_data.map_coll.top == 7 && wp->coll_data.map_coll.width == 10);
    CHECK(wp->coll_data.p_map_coll == &wp->coll_data.map_coll);
    CHECK(wp->coll_data.floor_line_id == -1 && wp->coll_data.rwall_line_id == -1);
    CHECK(wp->coll_data.mask_curr == 0);
    /* proc wiring straight off the descriptor; proc_dead cleared */
    CHECK(wp->proc_update == P_UPD && wp->proc_map == P_MAP);
    CHECK(wp->proc_hit == P_HIT && wp->proc_shield == P_SHD);
    CHECK(wp->proc_hop == P_HOP && wp->proc_setoff == P_SET);
    CHECK(wp->proc_reflector == P_REF && wp->proc_absorb == P_ABS);
    CHECK(wp->proc_dead == NULL);
    /* spawn position written into the DObj and mirrored to pos_prev */
    CHECK(DObjGetStruct(wg)->translate.vec.f.x == 10.0F);
    CHECK(DObjGetStruct(wg)->translate.vec.f.z == 30.0F);
    CHECK(wp->coll_data.pos_prev.y == 20.0F);
    gcEjectGObj(wg);                    /* do not leave a weapon with fake procs live */

    /* ---- FIGHTER arm: copies the owner FTStruct ---- */
    {
        static FTStruct tfp;
        memset(&tfp, 0, sizeof(tfp));
        tfp.team = 2; tfp.player = 0; tfp.handicap = 4; tfp.player_num = 1; tfp.lr = -1;
        tfp.display_mode = 1;
        tfp.motion_attack_id = nFTMotionAttackIDNone;
        tfp.motion_count = 7;
        tfp.stat_count = 9;
        parent.user_data.p = &tfp;

        wg = wpManagerMakeWeapon(&parent, &desc, &spawn, WEAPON_FLAG_PARENT_FIGHTER);
        CHECK(wg != NULL);
        wp = wpGetStruct(wg);
        CHECK(wp->owner_gobj == &parent);
        CHECK(wp->team == 2 && wp->player == 0 && wp->handicap == 4);
        CHECK(wp->player_num == 1 && wp->lr == -1);
        CHECK(wp->display_mode == 1);
        CHECK(wp->attack_coll.motion_attack_id == nFTMotionAttackIDNone);
        CHECK(wp->attack_coll.motion_count == 7);
        CHECK(wp->attack_coll.stat_count == 9);
        /* stale routed through ftParamGetStale with the fighter's args */
        CHECK(fabsf(wp->attack_coll.stale - ftParamGetStale(0, nFTMotionAttackIDNone, 7)) < 1e-4F);
        wpMainDestroyWeapon(wg);
    }

    /* ---- WEAPON arm: copies the owner WPStruct (no param calls) ---- */
    {
        static WPStruct owner;
        static GObj owner_owner;
        memset(&owner, 0, sizeof(owner));
        owner.owner_gobj = &owner_owner;
        owner.team = 3; owner.player = 1; owner.handicap = 5; owner.player_num = 2; owner.lr = -1;
        owner.display_mode = 2;
        owner.attack_coll.stale = 0.75F;
        owner.attack_coll.motion_attack_id = 5;
        owner.attack_coll.motion_count = 11;
        owner.attack_coll.stat_count = 13;
        parent.user_data.p = &owner;

        wg = wpManagerMakeWeapon(&parent, &desc, &spawn, WEAPON_FLAG_PARENT_WEAPON);
        CHECK(wg != NULL);
        wp = wpGetStruct(wg);
        CHECK(wp->owner_gobj == &owner_owner);   /* not the parent, but the parent's owner */
        CHECK(wp->team == 3 && wp->player == 1 && wp->handicap == 5);
        CHECK(wp->player_num == 2 && wp->lr == -1 && wp->display_mode == 2);
        CHECK(fabsf(wp->attack_coll.stale - 0.75F) < 1e-4F);
        CHECK(wp->attack_coll.motion_attack_id == 5 && wp->attack_coll.motion_count == 11);
        CHECK(wp->attack_coll.stat_count == 13);
        wpMainDestroyWeapon(wg);
    }

    /* ---- ITEM arm: copies the owner ITStruct (the arm that needed the
     * it/item.h types even though item.c is not compiled) ---- */
    {
        static ITStruct tip;
        static GObj item_owner;
        memset(&tip, 0, sizeof(tip));
        tip.owner_gobj = &item_owner;
        tip.team = 1; tip.player = 2; tip.handicap = 6; tip.player_num = 3; tip.lr = +1;
        tip.display_mode = 3;
        tip.attack_coll.stale = 0.5F;
        tip.attack_coll.motion_attack_id = 4;
        tip.attack_coll.motion_count = 15;
        tip.attack_coll.stat_count = 17;
        parent.user_data.p = &tip;

        wg = wpManagerMakeWeapon(&parent, &desc, &spawn, WEAPON_FLAG_PARENT_ITEM);
        CHECK(wg != NULL);
        wp = wpGetStruct(wg);
        CHECK(wp->owner_gobj == &item_owner);
        CHECK(wp->team == 1 && wp->player == 2 && wp->handicap == 6);
        CHECK(wp->player_num == 3 && wp->lr == +1 && wp->display_mode == 3);
        CHECK(fabsf(wp->attack_coll.stale - 0.5F) < 1e-4F);
        CHECK(wp->attack_coll.motion_attack_id == 4 && wp->attack_coll.motion_count == 15);
        CHECK(wp->attack_coll.stat_count == 17);
        wpMainDestroyWeapon(wg);
    }
}

/* Mario's fireball, the weapon (wp/wpmario/wpmariofireball.c,
 * compiled straight from the decomp). wpMarioFireballMakeWeapon is the first
 * real caller of wpManagerMakeWeapon: it reads the per-fighter
 * table dWPMarioFireballWeaponAttributes for the fireball's own numbers --
 * lifetime, base speed, launch angle, gravity, spin -- and lays down the
 * launch velocity from them. On the host wpManagerAddModel builds a stub
 * DObj so the make succeeds and every one of those writes is checked
 * against the real table; on target the empty model table refuses first
 * (see the disc probe). The WPStruct's own collision fields still arrive
 * through the reloc read, redirected here onto a stack buffer the way
 * test_wp_make does, so the fighter's loaded file need not exist. */
extern wpMarioFireballAttributes dWPMarioFireballWeaponAttributes[];
extern void *gFTMarioFileSpecial1;
extern void *gFTDataLuigiSpecial1;
extern int   llMarioSpecial1FireballWeaponAttributes;
extern int   llLuigiSpecial1FireballWeaponAttributes;
extern GObj* wpMarioFireballMakeWeapon(GObj *fighter_gobj, Vec3f *pos, s32 index);
extern sb32  wpMarioFireballProcUpdate(GObj *weapon_gobj);

static void test_wp_mario_fireball(void)
{
    static WPAttributes attr;           /* the reloc read's target (coll fields) */
    static FTStruct tfp;                /* the owner fighter */
    Vec3f owner_pos = { 100.0F, 50.0F, 0.0F };
    Vec3f spawn = { 12.0F, 34.0F, 0.0F };
    GObj *wg;
    WPStruct *wp;
    DObj *dobj;

    wpManagerAllocWeapons();            /* fresh pool */

    memset(&attr, 0, sizeof(attr));
    attr.size = 8;
    attr.attack_count = 1;

    memset(&tfp, 0, sizeof(tfp));
    tfp.player = 0;
    tfp.lr = +1;
    tfp.motion_attack_id = nFTMotionAttackIDNone;
    tfp.coll_data.p_translate = &owner_pos;
    tfp.coll_data.floor_line_id = -1;
    tfp.coll_data.ceil_line_id  = -1;
    tfp.coll_data.lwall_line_id = -1;
    tfp.coll_data.rwall_line_id = -1;

    /* ---- Mario (index 0): -5 deg, base speed 50, gravity 1.2 ---- */
    {
        wpMarioFireballAttributes *a = &dWPMarioFireballWeaponAttributes[0];
        static GObj parent;
        f32 angle;
        memset(&parent, 0, sizeof(parent));
        parent.user_data.p = &tfp;
        tfp.ga = nMPKineticsGround;         /* -> angle_ground */
        angle = a->angle_ground;

        /* redirect the reloc read: (*p_weapon) + offset lands on &attr */
        gFTMarioFileSpecial1 =
            (void*)((char*)&attr - (intptr_t)&llMarioSpecial1FireballWeaponAttributes);

        wg = wpMarioFireballMakeWeapon(&parent, &spawn, 0);
        CHECK(wg != NULL);
        wp = wpGetStruct(wg);
        dobj = DObjGetStruct(wg);

        CHECK(wp->weapon_vars.fireball.index == 0);
        CHECK(wp->lifetime == a->lifetime);                       /* 140 */
        CHECK(fabsf(wp->physics.vel_air.z) < 1e-5F);
        CHECK(fabsf(wp->physics.vel_air.x - (a->vel_base * cosf(angle) * (f32)tfp.lr)) < 1e-3F);
        CHECK(fabsf(wp->physics.vel_air.y - (a->vel_base * sinf(angle))) < 1e-3F);
        CHECK(fabsf(dobj->mobj->palette_id - a->anim_frame) < 1e-5F);   /* 0.0 */

        /* one ProcUpdate tic: gravity onto vel_air.y, spin onto rotate.x,
         * both read from the same table; lifetime ticks down. */
        wp->lifetime = 100;
        wp->physics.vel_air.y = 0.0F;
        dobj->rotate.vec.f.x = 0.0F;
        CHECK(wpMarioFireballProcUpdate(wg) == FALSE);
        CHECK(wp->lifetime == 99);
        CHECK(fabsf(wp->physics.vel_air.y - (-a->gravity)) < 1e-3F);    /* -1.2 */
        CHECK(fabsf(dobj->rotate.vec.f.x - a->rotate_speed) < 1e-4F);   /* DTOR(20) */
        wpMainDestroyWeapon(wg);
    }

    /* ---- Luigi (index 1): 0 deg, base speed 36, no gravity ---- */
    {
        wpMarioFireballAttributes *a = &dWPMarioFireballWeaponAttributes[1];
        static GObj parent;
        memset(&parent, 0, sizeof(parent));
        parent.user_data.p = &tfp;
        tfp.ga = nMPKineticsAir;            /* -> angle_air (also 0 for Luigi) */

        gFTDataLuigiSpecial1 =
            (void*)((char*)&attr - (intptr_t)&llLuigiSpecial1FireballWeaponAttributes);

        wg = wpMarioFireballMakeWeapon(&parent, &spawn, 1);
        CHECK(wg != NULL);
        wp = wpGetStruct(wg);
        dobj = DObjGetStruct(wg);

        CHECK(wp->weapon_vars.fireball.index == 1);
        CHECK(wp->lifetime == a->lifetime);
        CHECK(fabsf(wp->physics.vel_air.x - (a->vel_base * (f32)tfp.lr)) < 1e-3F);  /* cos0 */
        CHECK(fabsf(wp->physics.vel_air.y) < 1e-3F);                                /* sin0 */
        CHECK(fabsf(dobj->mobj->palette_id - a->anim_frame) < 1e-5F);              /* 1.0 */
        wpMainDestroyWeapon(wg);
    }
}

/* Fox's Blaster shot (wp/wpfox/wpfoxblaster.c, compiled
 * straight from the decomp -- the port's second directly-compiled weapon
 * file after wpmariofireball.c). Its WPDesc carries no WEAPON_FLAG_DOBJDESC
 * and a NULL p_mobjsubs (the real DL it names, dFoxSpecial4_ReflectorDL_
 * DisplayList, is bound whole rather than built from MObjSubs) -- on
 * target that reaches wpManagerAddModel's baked-pack branch exactly as
 * Mario's fireball does (DIVERGES there, already); on host it is the same
 * bare-DObj-plus-stub-MObj stub test_wp_mario_fireball exercises, so no
 * romdisk asset is needed yet. Drive MakeWeapon's launch velocity/facing,
 * one ProcUpdate tic's scale ramp (with its clamp), and ProcHop's velocity
 * reflection off a shield -- the three functions with actual arithmetic in
 * them; ProcMap/ProcHit/ProcReflector are thin wrappers around
 * wpMapTestAllCheckCollEnd/wpMainReflectorSetLR and the effect call ported
 * alongside (efManagerFoxBlasterGlowMakeEffect, src/dc/efmanager.c),
 * un-exercised here the same way test_wp_mario_fireball leaves its own
 * spark effects un-exercised. ftfoxspecialn.c (the caller) is still
 * unported: nothing reaches wpFoxBlasterMakeWeapon on target yet either,
 * the same "code first, table later" shape ftpurinspecialn.c and
 * ftmariospecialn.c stood as before their own steps. The ProcHop rotation
 * (syVectorRotateAbout3D, a full arbitrary-axis 3D rotation) is not one to
 * eyeball -- the expected vector below came out of a Python replica of the
 * decomp's own arithmetic (sys/vector.c:169), not a hand guess. */
extern void *gFTDataFoxSpecial1;
extern int   llFoxSpecial1BlasterWeaponAttributes;
extern GObj* wpFoxBlasterMakeWeapon(GObj *fighter_gobj, Vec3f *pos);
extern sb32  wpFoxBlasterProcUpdate(GObj *weapon_gobj);
extern sb32  wpFoxBlasterProcHop(GObj *weapon_gobj);

static void test_wp_fox_blaster(void)
{
    static WPAttributes attr;
    static FTStruct tfp;
    static GObj parent;
    Vec3f spawn = { 12.0F, 34.0F, 0.0F };
    Vec3f owner_pos = { 100.0F, 50.0F, 0.0F };
    GObj *wg;
    WPStruct *wp;
    DObj *dobj;

    wpManagerAllocWeapons();

    memset(&attr, 0, sizeof(attr));
    attr.size = 8;
    attr.attack_count = 1;

    memset(&tfp, 0, sizeof(tfp));
    tfp.player = 0;
    tfp.lr = -1;
    tfp.motion_attack_id = nFTMotionAttackIDNone;
    tfp.coll_data.p_translate = &owner_pos;
    tfp.coll_data.floor_line_id = -1;
    tfp.coll_data.ceil_line_id  = -1;
    tfp.coll_data.lwall_line_id = -1;
    tfp.coll_data.rwall_line_id = -1;

    memset(&parent, 0, sizeof(parent));
    parent.user_data.p = &tfp;

    gFTDataFoxSpecial1 =
        (void*)((char*)&attr - (intptr_t)&llFoxSpecial1BlasterWeaponAttributes);

    wg = wpFoxBlasterMakeWeapon(&parent, &spawn);
    CHECK(wg != NULL);
    wp = wpGetStruct(wg);
    dobj = DObjGetStruct(wg);

    /* facing left: vel_air.x = lr * WPBLASTER_VEL_X, no vertical component */
    CHECK(fabsf(wp->physics.vel_air.x - ((f32)tfp.lr * WPBLASTER_VEL_X)) < 1e-3F);
    CHECK(fabsf(wp->physics.vel_air.y) < 1e-5F);
    CHECK(fabsf(dobj->rotate.vec.f.z -
                syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x)) < 1e-4F);

    /* ProcUpdate: the shot's scale ramps up each tic, clamped at the top */
    dobj->scale.vec.f.x = 0.0F;
    CHECK(wpFoxBlasterProcUpdate(wg) == FALSE);
    CHECK(fabsf(dobj->scale.vec.f.x - WPBLASTER_ADD_SCALE_X) < 1e-4F);

    dobj->scale.vec.f.x = WPBLASTER_CLAMP_SCALE_X - 1.0F;
    CHECK(wpFoxBlasterProcUpdate(wg) == FALSE);
    CHECK(fabsf(dobj->scale.vec.f.x - WPBLASTER_CLAMP_SCALE_X) < 1e-4F);

    /* ProcHop: reflect vel_air about shield_collide_dir by shield_collide_
     * angle*2, re-face, and reset the scale ramp so the shot regrows.
     * (100,0,0) rotated pi radians about the y axis (0,1,0) -> (-100,0,0). */
    wp->physics.vel_air.x = 100.0F;
    wp->physics.vel_air.y = 0.0F;
    wp->physics.vel_air.z = 0.0F;
    wp->shield_collide_dir.x = 0.0F;
    wp->shield_collide_dir.y = 1.0F;
    wp->shield_collide_dir.z = 0.0F;
    wp->shield_collide_angle = 3.14159265F / 2.0F;   /* *2 inside ProcHop = pi */
    dobj->scale.vec.f.x = 1.0F;
    CHECK(wpFoxBlasterProcHop(wg) == FALSE);
    CHECK(fabsf(wp->physics.vel_air.x - (-100.0F)) < 1e-2F);
    CHECK(fabsf(wp->physics.vel_air.y) < 1e-2F);
    CHECK(fabsf(dobj->rotate.vec.f.z -
                syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x)) < 1e-4F);
    CHECK(fabsf(dobj->scale.vec.f.x - 1.0F) < 1e-5F);

    wpMainDestroyWeapon(wg);
}

/* The fighters' weapon tables off the real bake (src/dc/wpattrs.c):
 * romdisk/wpattrs.bin loaded, the eleven gFTData* bases bound, and the
 * arithmetic wpManagerMakeWeapon does -- base + &ll<...>WeaponAttributes,
 * the symbol an absolute out of hosttest_wpattrs.ld -- landing on the
 * ROM's own numbers with NO fake table anywhere. Fox's Blaster is the
 * one-record file; Yoshi's is the two-record one (the egg at 0xC, the
 * Star at 0x40 -- spread apart here by the host's wider WPAttributes,
 * which is what --record-size 72 in the Makefile and the sizeof check
 * below are about), and the Star is the roster's one negative
 * shield_damage. Then the whole spawn, to see the hitbox arrive in the
 * WPStruct and the shot NOT take the model-less arm. */
extern void *gFTDataYoshiMain;
extern int   llYoshiMainStarWeaponAttributes;
extern int   llYoshiMainEggThrowWeaponAttributes;
extern void *gFTDataBossMainMotion;
extern int   llBossMainMotionBulletNormalWeaponAttributes;
extern int   llBossMainMotionBulletHardWeaponAttributes;

static void test_wp_attrs(void)
{
    static FTStruct tfp;
    static GObj parent;
    Vec3f spawn = { 12.0F, 34.0F, 0.0F };
    Vec3f owner_pos = { 100.0F, 50.0F, 0.0F };
    WPAttributes *attr;
    GObj *wg;
    WPStruct *wp;

    CHECK(sizeof(WPAttributes) == 72);

    CHECK(wpAttrsLoad() == 0);
    wpAttrsBind();
    CHECK(gFTDataFoxSpecial1 != NULL);
    CHECK(gFTDataYoshiMain != NULL);
    CHECK(wpAttrsFileBlob(210) == gFTDataFoxSpecial1);
    CHECK(wpAttrsFileBlob(247) == gFTDataYoshiMain);
    /* every file the blob carries is bound -- Master Hand's MainMotion
     * (249) shipped for a whole milestone with no row, and his bullets
     * read their attributes from address 0x774 */
    CHECK(wpAttrsUnboundFiles() == 0);
    CHECK(gFTDataBossMainMotion != NULL);
    CHECK(wpAttrsFileBlob(249) == gFTDataBossMainMotion);
    attr = (WPAttributes *)((char *)gFTDataBossMainMotion +
                            (intptr_t)&llBossMainMotionBulletNormalWeaponAttributes);
    CHECK(attr->data != NULL && attr->damage != 0 && attr->size != 0);
    attr = (WPAttributes *)((char *)gFTDataBossMainMotion +
                            (intptr_t)&llBossMainMotionBulletHardWeaponAttributes);
    CHECK(attr->data != NULL && attr->damage != 0 && attr->size != 0);

    attr = (WPAttributes *)((char *)gFTDataFoxSpecial1 +
                            (intptr_t)&llFoxSpecial1BlasterWeaponAttributes);
    CHECK(attr->data != NULL);              /* it has a model: not model-less */
    CHECK(attr->p_mobjsubs == NULL);
    CHECK(attr->map_coll_top == 10 && attr->map_coll_center == 0 &&
          attr->map_coll_bottom == -10 && attr->map_coll_width == 10);
    CHECK(attr->size == 40);
    CHECK(attr->angle == 10);
    CHECK(attr->knockback_scale == 100);
    CHECK(attr->damage == 6);
    CHECK(attr->element == 0);
    CHECK(attr->knockback_weight == 1);
    CHECK(attr->shield_damage == 1);
    CHECK(attr->attack_count == 1);
    CHECK(attr->can_setoff == 0);
    CHECK(attr->sfx == 2);
    CHECK(attr->priority == 1);
    CHECK(attr->can_rehit_item == 0 && attr->can_rehit_fighter == 0);
    CHECK(attr->can_hop == 1 && attr->can_reflect == 1 &&
          attr->can_absorb == 1 && attr->can_shield == 1);
    CHECK(attr->knockback_base == 0);

    attr = (WPAttributes *)((char *)gFTDataYoshiMain +
                            (intptr_t)&llYoshiMainEggThrowWeaponAttributes);
    CHECK(attr->size == 200);
    CHECK(attr->damage == 14);
    CHECK(attr->shield_damage == 6);
    CHECK(attr->sfx == 31);
    CHECK(attr->knockback_base == 50);
    CHECK(attr->map_coll_top == 150 && attr->map_coll_bottom == -150);

    attr = (WPAttributes *)((char *)gFTDataYoshiMain +
                            (intptr_t)&llYoshiMainStarWeaponAttributes);
    CHECK(attr->size == 160);
    CHECK(attr->damage == 4);
    CHECK(attr->knockback_weight == 30);
    CHECK(attr->shield_damage == -3);
    CHECK(attr->sfx == 34);
    CHECK(attr->can_rehit_item == 1);
    CHECK(attr->map_coll_top == 100 && attr->map_coll_center == 0 &&
          attr->map_coll_bottom == -100 && attr->map_coll_width == 96);

    /* the spawn, on the bound base and nothing else */
    wpManagerAllocWeapons();

    memset(&tfp, 0, sizeof(tfp));
    tfp.player = 0;
    tfp.lr = 1;
    tfp.motion_attack_id = nFTMotionAttackIDNone;
    tfp.coll_data.p_translate = &owner_pos;
    tfp.coll_data.floor_line_id = -1;
    tfp.coll_data.ceil_line_id  = -1;
    tfp.coll_data.lwall_line_id = -1;
    tfp.coll_data.rwall_line_id = -1;

    memset(&parent, 0, sizeof(parent));
    parent.user_data.p = &tfp;

    wg = wpFoxBlasterMakeWeapon(&parent, &spawn);
    CHECK(wg != NULL);
    wp = wpGetStruct(wg);
    CHECK(wp->attack_coll.damage == 6);
    CHECK(fabsf(wp->attack_coll.size - 20.0F) < 1e-4F);   /* size * 0.5 */
    CHECK(wp->attack_coll.attack_count == 1);
    CHECK(wp->attack_coll.knockback_scale == 100);
    CHECK(wp->coll_data.map_coll.top == 10 && wp->coll_data.map_coll.bottom == -10);
    wpMainDestroyWeapon(wg);
}

/* wp/wpyoshi/wpyoshistar.c, compiled unmodified from the decomp
 * -- the two stars Yoshi Bomb's landing throws, one each way. The same
 * bare-DObj host shape test_wp_fox_blaster stands on, and the same "no
 * romdisk row yet" state on target (wpManagerMakeWeapon returns NULL
 * there until a sWPManagerModels row exists, and wpYoshiStarMakeStars
 * ignores both returns anyway, verbatim). Drive MakeWeapon's spawn offset,
 * launch angle and facing for both directions, the lifetime-driven scale
 * ramp with its clamp, one ProcUpdate tic (scale, spin, the radial
 * velocity decay by WPYOSHISTAR_VEL_CLAMP -- and that below the clamp it
 * stops dead rather than reversing), expiry on the last tic, ProcHop's
 * reflection and re-facing, and ProcReflector's lifetime/scale reset and
 * lr flip. ProcHit/ProcShield are the SparkleWhite effect call plus a
 * hit_normal_damage read, un-exercised here the same way the blaster's
 * effect leaves are. */
extern void *gFTDataYoshiMain;
extern int   llYoshiMainStarWeaponAttributes;
extern GObj *gGCCommonLinks[];
extern GObj* wpYoshiStarMakeStars(GObj *fighter_gobj, Vec3f *pos);
extern f32   wpYoshiStarGetScale(WPStruct *wp);
extern sb32  wpYoshiStarProcUpdate(GObj *weapon_gobj);
extern sb32  wpYoshiStarProcHop(GObj *weapon_gobj);
extern sb32  wpYoshiStarProcReflector(GObj *weapon_gobj);

static void test_wp_yoshi_star(void)
{
    static WPAttributes attr;
    static FTStruct tfp;
    static GObj parent;
    Vec3f spawn = { 12.0F, 34.0F, 0.0F };
    Vec3f owner_pos = { 100.0F, 50.0F, 0.0F };
    GObj *wg, *wg2;
    WPStruct *wp;
    DObj *dobj;
    f32 want_x, want_y, speed;

    wpManagerAllocWeapons();

    memset(&attr, 0, sizeof(attr));
    attr.size = 8;
    attr.attack_count = 1;

    memset(&tfp, 0, sizeof(tfp));
    tfp.player = 0;
    tfp.lr = +1;
    tfp.motion_attack_id = nFTMotionAttackIDNone;
    tfp.coll_data.p_translate = &owner_pos;
    tfp.coll_data.floor_line_id = -1;
    tfp.coll_data.ceil_line_id  = -1;
    tfp.coll_data.lwall_line_id = -1;
    tfp.coll_data.rwall_line_id = -1;

    memset(&parent, 0, sizeof(parent));
    parent.user_data.p = &tfp;

    gFTDataYoshiMain =
        (void*)((char*)&attr - (intptr_t)&llYoshiMainStarWeaponAttributes);

    /* Through the game's own entry, not wpYoshiStarMakeWeapon directly:
     * that one ends in the decomp's `return;` with no value unless the
     * decomp's AVOID_UB option is on (it is not, anywhere in this port),
     * so its return is unspecified -- and never read, on N64 or here,
     * since wpYoshiStarMakeStars drops both. The two stars come off the
     * weapon link instead, told apart by facing; the link is empty
     * between tests (test_wp_make checks that outright). */
    CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);
    CHECK(wpYoshiStarMakeStars(&parent, &spawn) == NULL);
    wg = wg2 = NULL;
    {
        GObj *g;
        int n = 0;

        for (g = gGCCommonLinks[nGCCommonLinkIDWeapon]; g != NULL; g = g->link_next)
        {
            n++;
            if (wpGetStruct(g)->lr == +1) wg = g;
            if (wpGetStruct(g)->lr == -1) wg2 = g;
        }
        CHECK(n == 2);
    }
    CHECK(wg != NULL);
    CHECK(wg2 != NULL);
    if (wg == NULL || wg2 == NULL)
    {
        return;
    }
    wp = wpGetStruct(wg);
    dobj = DObjGetStruct(wg);

    /* the right-going star (the owner faces +1): spawned WPYOSHISTAR_OFF_X
     * to the right and OFF_Y up of the given point, launched at 30
     * degrees, 16 tics to live */
    CHECK(wp->lifetime == WPYOSHISTAR_LIFETIME);
    CHECK(fabsf(dobj->translate.vec.f.x - (spawn.x + WPYOSHISTAR_OFF_X)) < 1e-3F);
    CHECK(fabsf(dobj->translate.vec.f.y - (spawn.y + WPYOSHISTAR_OFF_Y)) < 1e-3F);
    want_x = __cosf(WPYOSHISTAR_ANGLE) * WPYOSHISTAR_VEL;
    want_y = __sinf(WPYOSHISTAR_ANGLE) * WPYOSHISTAR_VEL;
    CHECK(fabsf(wp->physics.vel_air.x - want_x) < 1e-3F);
    CHECK(fabsf(wp->physics.vel_air.y - want_y) < 1e-3F);

    /* the left-going one mirrors x only */
    CHECK(wpGetStruct(wg2)->lifetime == WPYOSHISTAR_LIFETIME);
    CHECK(fabsf(DObjGetStruct(wg2)->translate.vec.f.x - (spawn.x - WPYOSHISTAR_OFF_X)) < 1e-3F);
    CHECK(fabsf(DObjGetStruct(wg2)->translate.vec.f.y - (spawn.y + WPYOSHISTAR_OFF_Y)) < 1e-3F);
    CHECK(fabsf(wpGetStruct(wg2)->physics.vel_air.x + want_x) < 1e-3F);
    CHECK(fabsf(wpGetStruct(wg2)->physics.vel_air.y - want_y) < 1e-3F);
    wpMainDestroyWeapon(wg2);

    /* the scale ramp: lifetime*0.175 + 0.3, clamped at 1 -- 16 tics in
     * reads 3.1 -> 1.0; at 4 tics left it is 1.0 exactly, at 2 it is 0.65 */
    CHECK(fabsf(wpYoshiStarGetScale(wp) - 1.0F) < 1e-5F);
    wp->lifetime = 2;
    CHECK(fabsf(wpYoshiStarGetScale(wp) - 0.65F) < 1e-5F);
    wp->lifetime = WPYOSHISTAR_LIFETIME;

    /* one ProcUpdate tic: lifetime down one, scale from the ramp, spin
     * by ROTATE_SPEED*lr, and the speed shrinks by VEL_CLAMP along its
     * own direction (30 -> 28.2 at the same 30 degrees) */
    dobj->rotate.vec.f.z = 0.0F;
    CHECK(wpYoshiStarProcUpdate(wg) == FALSE);
    CHECK(wp->lifetime == WPYOSHISTAR_LIFETIME - 1);
    CHECK(fabsf(dobj->scale.vec.f.x - 1.0F) < 1e-5F);
    CHECK(fabsf(dobj->scale.vec.f.y - 1.0F) < 1e-5F);
    CHECK(fabsf(dobj->rotate.vec.f.z - WPYOSHISTAR_ROTATE_SPEED) < 1e-5F);
    speed = WPYOSHISTAR_VEL - WPYOSHISTAR_VEL_CLAMP;
    CHECK(fabsf(wp->physics.vel_air.x - __cosf(WPYOSHISTAR_ANGLE) * speed) < 1e-3F);
    CHECK(fabsf(wp->physics.vel_air.y - __sinf(WPYOSHISTAR_ANGLE) * speed) < 1e-3F);

    /* below the clamp the velocity zeroes rather than reversing */
    wp->physics.vel_air.x = 1.0F;
    wp->physics.vel_air.y = 0.5F;
    CHECK(wpYoshiStarProcUpdate(wg) == FALSE);
    CHECK(fabsf(wp->physics.vel_air.x) < 1e-5F);
    CHECK(fabsf(wp->physics.vel_air.y) < 1e-5F);

    /* and it expires on the tic its lifetime reaches zero -- the dust
     * effect it makes there returns NULL on host, harmlessly */
    wp->lifetime = 1;
    CHECK(wpYoshiStarProcUpdate(wg) == TRUE);
    wp->lifetime = WPYOSHISTAR_LIFETIME;

    /* ProcHop: reflect vel_air about shield_collide_dir by shield_collide_
     * angle*2 and re-face from the new x. (100,0,0) about y by pi ->
     * (-100,0,0), the same case the blaster's test uses. */
    wp->physics.vel_air.x = 100.0F;
    wp->physics.vel_air.y = 0.0F;
    wp->physics.vel_air.z = 0.0F;
    wp->shield_collide_dir.x = 0.0F;
    wp->shield_collide_dir.y = 1.0F;
    wp->shield_collide_dir.z = 0.0F;
    wp->shield_collide_angle = 3.14159265F / 2.0F;
    CHECK(wpYoshiStarProcHop(wg) == FALSE);
    CHECK(fabsf(wp->physics.vel_air.x - (-100.0F)) < 1e-2F);
    CHECK(fabsf(wp->physics.vel_air.y) < 1e-2F);
    CHECK(wp->lr == -1);
    CHECK(fabsf(dobj->rotate.vec.f.z -
                syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x)) < 1e-4F);

    /* ProcReflector: a fresh lifetime, full scale, the velocity turned to
     * the reflecting owner's facing by wpMainReflectorSetLR (owner faces
     * +1, star moving -x -> x flipped to +100), rotate.z from the new
     * velocity, and the star's own lr flipped (-1 after ProcHop -> +1) */
    wp->owner_gobj = &parent;
    wp->lifetime = 3;
    dobj->scale.vec.f.x = dobj->scale.vec.f.y = 0.4F;
    CHECK(wpYoshiStarProcReflector(wg) == FALSE);
    CHECK(wp->lifetime == WPYOSHISTAR_LIFETIME);
    CHECK(fabsf(dobj->scale.vec.f.x - 1.0F) < 1e-5F);
    CHECK(fabsf(dobj->scale.vec.f.y - 1.0F) < 1e-5F);
    CHECK(fabsf(wp->physics.vel_air.x - 100.0F) < 1e-2F);
    CHECK(fabsf(dobj->rotate.vec.f.z -
                syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x)) < 1e-4F);
    CHECK(wp->lr == +1);

    wpMainDestroyWeapon(wg);
    CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);
}

/* wp/wpsamus/wpsamuschargeshot.c, compiled unmodified from
 * the decomp, driven through the two callers in ftsamusspecialn.c that
 * make it: the Loop's held shot (ftSamusSpecialNLoopSetStatus, parented,
 * no projectile flag, grown a level every FTSAMUS_CHARGE_INT tics by
 * ftSamusSpecialNLoopProcUpdate) and the End's release (ftSamusSpecialN-
 * EndProcUpdate on flag0: the held shot is fired in place -- is_full_
 * charge set, wpSamusChargeShotProcUpdate's next tic launches it with the
 * level's velocity/damage/size out of dWPSamusChargeShotWeaponAttributes
 * -- and the recoil pushes Samus back by the level). Same bare-DObj host
 * shape as the blaster's and the stars' tests, with gFTDataSamusSpecial1
 * (ftsamus.c's real global) put under a WPAttributes the same way; the
 * shot is unmodeled on target the same way the Bomb is. The charge SFX
 * loop (ftParamPlayLoopSFX) and the shoot SFX (func_800269C0_275C0) are
 * the port's audio stubs on host. */
extern void *gFTDataSamusSpecial1;
extern int   llSamusSpecial1ChargeShotWeaponAttributes;
extern wpSamusChargeShotAttributes dWPSamusChargeShotWeaponAttributes[];
extern sb32  wpSamusChargeShotProcUpdate(GObj *weapon_gobj);
extern sb32  wpSamusChargeShotProcHop(GObj *weapon_gobj);

static void test_wp_samus_charge_shot(void)
{
    static WPAttributes attr;
    GObj *wg;
    WPStruct *wp;
    DObj *dobj;
    int tic;

    wpManagerAllocWeapons();
    memset(&attr, 0, sizeof(attr));
    attr.size = 8;
    attr.attack_count = 1;
    gFTDataSamusSpecial1 =
        (void*)((char*)&attr - (intptr_t)&llSamusSpecial1ChargeShotWeaponAttributes);

    spawn(0.0f, 0.0f);
    fp.fkind = nFTKindSamus;
    fp.attr->is_have_specialn = TRUE;
    fp.passive_vars.samus.charge_level = 0;
    /* the shot spawns off joint 16 -- the 3-joint mock has TopN there */
    {
        DObj *saved_j16 = fp.joints[FTSAMUS_CHARGE_JOINT];
        fp.joints[FTSAMUS_CHARGE_JOINT] = fp.joints[nFTPartsJointTopN];

        CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);
        ftSamusSpecialNStartSetStatus(mock_gobj);
        CHECK_STATUS(nFTSamusStatusSpecialNStart);

        /* the Loop makes the held shot: parented to Samus, its hitbox off,
         * scaled by level 0's sprite size, the owner link set both ways */
        ftSamusSpecialNLoopSetStatus(mock_gobj);
        CHECK_STATUS(nFTSamusStatusSpecialNLoop);
        CHECK(fp.status_vars.samus.specialn.charge_int == FTSAMUS_CHARGE_INT);
        wg = fp.status_vars.samus.specialn.charge_gobj;
        CHECK(wg != NULL);
        if (wg == NULL)
        {
            fp.joints[FTSAMUS_CHARGE_JOINT] = saved_j16;
            return;
        }
        wp = wpGetStruct(wg);
        dobj = DObjGetStruct(wg);
        CHECK(wp->weapon_vars.charge_shot.is_release == FALSE);
        CHECK(wp->weapon_vars.charge_shot.charge_size == 0);
        CHECK(wp->weapon_vars.charge_shot.is_full_charge == FALSE);
        CHECK(wp->weapon_vars.charge_shot.owner_gobj == mock_gobj);
        CHECK(wp->attack_coll.attack_state == nGMAttackStateOff);
        CHECK(fabsf(dobj->scale.vec.f.x -
                    dWPSamusChargeShotWeaponAttributes[0].gfx_size / WPCHARGESHOT_GFX_SIZE_DIV) < 1e-4f);
        {
            /* the spawn point is FTSAMUS_CHARGE_OFF_X along the joint's own
             * x, taken through its world transform (facing included), so
             * compare with the game's own helper rather than the raw offset */
            Vec3f want;
            ftSamusSpecialNGetChargeShotPosition(&fp, &want);
            CHECK(fabsf(dobj->translate.vec.f.x - want.x) < 1e-3f);
            CHECK(fabsf(dobj->translate.vec.f.y - want.y) < 1e-3f);
        }

        /* FTSAMUS_CHARGE_INT tics of Loop raise the level by one and pass
         * it to the shot; the next tic of the shot's own ProcUpdate
         * rescales it and spins it, unreleased */
        for (tic = 0; tic < FTSAMUS_CHARGE_INT; tic++)
        {
            ftSamusSpecialNLoopProcUpdate(mock_gobj);
        }
        CHECK(fp.passive_vars.samus.charge_level == 1);
        CHECK(wp->weapon_vars.charge_shot.charge_size == 1);
        CHECK(fp.status_vars.samus.specialn.charge_int == FTSAMUS_CHARGE_INT);
        dobj->rotate.vec.f.z = 0.0f;
        CHECK(wpSamusChargeShotProcUpdate(wg) == FALSE);
        CHECK(wp->weapon_vars.charge_shot.is_release == FALSE);
        CHECK(fabsf(dobj->scale.vec.f.x -
                    dWPSamusChargeShotWeaponAttributes[1].gfx_size / WPCHARGESHOT_GFX_SIZE_DIV) < 1e-4f);
        CHECK(fabsf(dobj->rotate.vec.f.z - (-WPCHARGESHOT_ROTATE_SPEED * wp->lr)) < 1e-5f);

        /* the release from the ground: End's flag0 tic moves the shot to
         * the hand, marks it full, runs its map collision once, unlinks
         * it from Samus, and recoils her by (level + 1) */
        ftSamusSpecialNEndSetStatus(mock_gobj);
        CHECK_STATUS(nFTSamusStatusSpecialNEnd);
        fp.motion_vars.flags.flag0 = TRUE;
        mock_gobj->anim_frame = 5.0f;
        ftSamusSpecialNEndProcUpdate(mock_gobj);
        CHECK(fp.motion_vars.flags.flag0 == FALSE);
        CHECK(fp.status_vars.samus.specialn.charge_gobj == NULL);
        CHECK(wp->weapon_vars.charge_shot.owner_gobj == NULL);
        CHECK(wp->weapon_vars.charge_shot.is_full_charge == TRUE);
        CHECK(wp->weapon_vars.charge_shot.charge_size == 1);
        CHECK(fabsf(fp.physics.vel_ground.x -
                    -((FTSAMUS_CHARGE_RECOIL_MUL * 2.0f) + FTSAMUS_CHARGE_RECOIL_BASE)) < 1e-4f);
        CHECK(fp.passive_vars.samus.charge_level == 0);
        CHECK(fp.proc_damage == NULL);

        /* the shot's next tic launches it at level 1: velocity, damage,
         * hitbox size and map collision box out of the table, hitbox on */
        CHECK(wpSamusChargeShotProcUpdate(wg) == FALSE);
        CHECK(wp->weapon_vars.charge_shot.is_release == TRUE);
        CHECK(fabsf(wp->physics.vel_air.x - dWPSamusChargeShotWeaponAttributes[1].vel_x * wp->lr) < 1e-4f);
        CHECK(wp->attack_coll.damage == dWPSamusChargeShotWeaponAttributes[1].damage);
        CHECK(fabsf(wp->attack_coll.size - dWPSamusChargeShotWeaponAttributes[1].attack_size * 0.5f) < 1e-4f);
        CHECK(fabsf(wp->coll_data.map_coll.width - dWPSamusChargeShotWeaponAttributes[1].coll_size * 0.5f) < 1e-4f);
        /* New, then the same tic's wpProcessUpdateHitPositions moves it on
         * to Transfer with the first position taken */
        CHECK(wp->attack_coll.attack_state == nGMAttackStateTransfer);
        CHECK(wp->attack_coll.priority == dWPSamusChargeShotWeaponAttributes[1].priority);

        /* ProcHop reflects the velocity about the shield normal and
         * re-faces the shot from its new x: (v,0,0) about y by pi */
        wp->physics.vel_air.x = 100.0f;
        wp->physics.vel_air.y = 0.0f;
        wp->physics.vel_air.z = 0.0f;
        wp->shield_collide_dir.x = 0.0f;
        wp->shield_collide_dir.y = 1.0f;
        wp->shield_collide_dir.z = 0.0f;
        wp->shield_collide_angle = 3.14159265f / 2.0f;
        CHECK(wpSamusChargeShotProcHop(wg) == FALSE);
        CHECK(fabsf(wp->physics.vel_air.x - (-100.0f)) < 1e-2f);
        CHECK(wp->lr == -1);

        wpMainDestroyWeapon(wg);
        CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);

        /* a released shot made on the spot (no held one) launches at once */
        {
            Vec3f pos = { 0.0f, 0.0f, 0.0f };
            GObj *wg2;

            fp.passive_vars.samus.charge_level = FTSAMUS_CHARGE_MAX;
            wg2 = wpSamusChargeShotMakeWeapon(mock_gobj, &pos, FTSAMUS_CHARGE_MAX, TRUE);
            CHECK(wg2 != NULL);
            if (wg2 != NULL)
            {
                CHECK(wpGetStruct(wg2)->weapon_vars.charge_shot.is_release == TRUE);
                CHECK(wpGetStruct(wg2)->attack_coll.damage == dWPSamusChargeShotWeaponAttributes[FTSAMUS_CHARGE_MAX].damage);
                CHECK(wpGetStruct(wg2)->proc_dead == wpSamusChargeShotProcDead);
                wpMainDestroyWeapon(wg2);
            }
        }

        /* and a full charge in the Loop: the tic that reaches CHARGE_MAX
         * drops the held shot and cancels out to Wait, keeping the charge */
        fp.passive_vars.samus.charge_level = FTSAMUS_CHARGE_MAX - 1;
        ftSamusSpecialNLoopSetStatus(mock_gobj);
        CHECK(fp.status_vars.samus.specialn.charge_gobj != NULL);
        for (tic = 0; tic < FTSAMUS_CHARGE_INT; tic++)
        {
            ftSamusSpecialNLoopProcUpdate(mock_gobj);
        }
        CHECK(fp.passive_vars.samus.charge_level == FTSAMUS_CHARGE_MAX);
        CHECK(fp.status_vars.samus.specialn.charge_gobj == NULL);
        CHECK_STATUS(nFTCommonStatusWait);
        CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);

        fp.joints[FTSAMUS_CHARGE_JOINT] = saved_j16;
    }
    fp.passive_vars.samus.charge_level = 0;
    fp.fkind = nFTKindMario;
}

/* Link's boomerang (wp/wplink/wplinkboomerang.c, compiled
 * unmodified). The same bare-DObj host shape as the blaster's, the
 * stars' and the Charge Shot's tests, with gFTDataLinkSpecial1
 * (ft/ftchar/ftlink/ftlink.c's real global)
 * put under a WPAttributes the same way; it is unmodeled on target as
 * they are, so wpManagerMakeWeapon refuses there and every caller in
 * ftlinkspecialn.c already handles the NULL the decomp's way.
 *
 * What cannot be driven here is wpLinkBoomerangSetReturnVars, which
 * hides the boomerang's inner mesh with
 * DObjGetStruct(g)->child->child->flags: the host's bare DObj has no
 * children, and building two fake ones would test the fake, not the
 * game. Everything that reaches it -- ProcHit, ProcShield, ProcSetOff
 * and the slowed-to-10 arm of ProcUpdate -- is left with it, the same
 * way the blaster's effect leaves are. The RETURN flag it would set is
 * set by hand below to reach CheckOwnerCatch, which is the one place the
 * weapon calls BACK into a fighter's file. */
extern void *gFTDataLinkSpecial1;
extern int   llLinkSpecial1BoomerangWeaponAttributes;

static void test_wp_link_boomerang(void)
{
    static WPAttributes attr;
    Vec3f pos = { 10.0f, 20.0f, 0.0f };
    Vec3f vel;
    GObj *wg;
    WPStruct *wp;
    DObj *dobj;
    f32 a;
    /* wpLinkBoomerangFlags lives in wplinkboomerang.c, not a header, so
     * the two bits this test needs are spelled out here: bit 0 RETURN,
     * bit 2 FORWARD (wp/wplink/wplinkboomerang.c:41-62). */
    const u8 flag_return  = 1 << 0;
    const u8 flag_forward = 1 << 2;

    wpManagerAllocWeapons();
    memset(&attr, 0, sizeof(attr));
    attr.size = 8;
    attr.attack_count = 1;
    gFTDataLinkSpecial1 =
        (void*)((char*)&attr - (intptr_t)&llLinkSpecial1BoomerangWeaponAttributes);

    /* ---- the throw angle, which is pure arithmetic on the stick and is
     * the same function for the tilt and the smash throw ---- */
    memset(&vel, 0, sizeof(vel));
    fp.input.pl.stick_range.x = 0;
    fp.input.pl.stick_range.y = 0;
    a = wpLinkBoomerangGetAngleSetVel(&vel, &fp, +1, WPBOOMERANG_VEL_TILT);
    CHECK_EQF(a, 0.0f);
    CHECK_EQF(vel.x, WPBOOMERANG_VEL_TILT);
    CHECK_NEAR(vel.y, 0.0f, 1e-4f);

    /* facing left mirrors the velocity and reports the angle as its
     * reflection about the vertical, so the returned angle is a compass
     * bearing while the velocity is already signed */
    a = wpLinkBoomerangGetAngleSetVel(&vel, &fp, -1, WPBOOMERANG_VEL_TILT);
    CHECK_NEAR(a, F_CST_DTOR32(180.0F), 1e-5f);
    CHECK_EQF(vel.x, -WPBOOMERANG_VEL_TILT);

    /* the stick angles it, clamped to 30 degrees either way, and only
     * past WPBOOMERANG_ANGLE_STICK_THRESHOLD */
    fp.input.pl.stick_range.y = WPBOOMERANG_ANGLE_STICK_THRESHOLD;
    a = wpLinkBoomerangGetAngleSetVel(&vel, &fp, +1, WPBOOMERANG_VEL_TILT);
    CHECK_EQF(a, 0.0f);                     /* not past it: no angle */
    fp.input.pl.stick_range.y = 127;
    a = wpLinkBoomerangGetAngleSetVel(&vel, &fp, +1, WPBOOMERANG_VEL_SMASH);
    CHECK_NEAR(a, F_CST_DTOR32(30.0F), 1e-5f);
    CHECK_NEAR(vel.x, cosf(F_CST_DTOR32(30.0F)) * WPBOOMERANG_VEL_SMASH, 1e-2f);
    CHECK_NEAR(vel.y, sinf(F_CST_DTOR32(30.0F)) * WPBOOMERANG_VEL_SMASH, 1e-2f);
    fp.input.pl.stick_range.y = -127;
    a = wpLinkBoomerangGetAngleSetVel(&vel, &fp, +1, WPBOOMERANG_VEL_SMASH);
    CHECK_NEAR(a, F_CST_DTOR32(360.0F) - F_CST_DTOR32(30.0F), 1e-5f);
    fp.input.pl.stick_range.y = 0;

    /* ---- the angle helpers ---- */
    a = F_CST_DTOR32(370.0F);
    wpLinkBoomerangClampAngle360(&a);
    CHECK_NEAR(a, F_CST_DTOR32(10.0F), 1e-4f);
    a = F_CST_DTOR32(-370.0F);
    wpLinkBoomerangClampAngle360(&a);
    CHECK_NEAR(a, F_CST_DTOR32(-10.0F), 1e-4f);

    /* the forward clamp keeps a bounced boomerang out of the four
     * near-vertical wedges, one per quadrant */
    a = F_CST_DTOR32(45.0F);
    wpLinkBoomerangClampAngleForward(&a);
    CHECK_NEAR(a, F_CST_DTOR32(30.0F), 1e-5f);
    a = F_CST_DTOR32(120.0F);
    wpLinkBoomerangClampAngleForward(&a);
    CHECK_NEAR(a, F_CST_DTOR32(150.0F), 1e-5f);
    a = F_CST_DTOR32(240.0F);
    wpLinkBoomerangClampAngleForward(&a);
    CHECK_NEAR(a, F_CLC_DTOR32(210.0F), 1e-5f);
    a = F_CST_DTOR32(300.0F);
    wpLinkBoomerangClampAngleForward(&a);
    CHECK_NEAR(a, F_CLC_DTOR32(330.0F), 1e-5f);
    a = F_CST_DTOR32(10.0F);                /* already forward: untouched */
    wpLinkBoomerangClampAngleForward(&a);
    CHECK_NEAR(a, F_CST_DTOR32(10.0F), 1e-5f);

    /* ---- the throw ---- */
    spawn(0.0f, 0.0f);
    mock_anim_len = 1e9f;
    fp.fkind = nFTKindLink;
    fp.lr = +1;
    fp.ga = nMPKineticsGround;
    fp.passive_vars.link.boomerang_gobj = NULL;
    fp.status_vars.link.specialn.is_smash = FALSE;

    CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);
    wg = wpLinkBoomerangMakeWeapon(mock_gobj, &pos);
    CHECK(wg != NULL);
    if (wg == NULL)
    {
        fp.fkind = nFTKindMario;
        return;
    }
    wp = wpGetStruct(wg);
    dobj = DObjGetStruct(wg);

    /* spawned 150 ahead and 290 above the joint it was given */
    CHECK_EQF(dobj->translate.vec.f.x, pos.x + WPBOOMERANG_OFF_X);
    CHECK_EQF(dobj->translate.vec.f.y, pos.y + WPBOOMERANG_OFF_Y);
    CHECK(wp->lr == +1);
    CHECK(wp->lifetime == WPBOOMERANG_LIFETIME_TILT);
    CHECK_EQF(wp->physics.vel_air.x, WPBOOMERANG_VEL_TILT);
    CHECK_EQF(wp->weapon_vars.boomerang.default_angle, 0.0f);
    CHECK(wp->weapon_vars.boomerang.flags == flag_forward);
    CHECK(wp->weapon_vars.boomerang.homing_delay == 130);
    CHECK(wp->weapon_vars.boomerang.adjust_angle_delay == 0);
    CHECK(wp->weapon_vars.boomerang.parent_gobj == mock_gobj);
    CHECK(wp->proc_setoff == wpLinkBoomerangProcSetOff);
    CHECK(wp->proc_dead == wpLinkBoomerangProcDead);
    CHECK(wp->is_camera_follow == TRUE);
    CHECK(wp->is_hitlag_victim == TRUE);

    /* ---- one tic of flight: the homing delay ticks down (which is what
     * keeps CheckOffCamera off the camera for the first 130), and the
     * forward arm sheds 1.4 of speed and re-derives the velocity from
     * the angle ---- */
    CHECK(wpLinkBoomerangProcUpdate(wg) == FALSE);
    CHECK(wp->weapon_vars.boomerang.homing_delay == 129);
    CHECK_NEAR(wp->physics.vel_air.x, WPBOOMERANG_VEL_TILT - 1.4f, 1e-3f);

    /* the two speed clamps the flight rides between */
    wp->physics.vel_air.x = 60.0f;
    wp->physics.vel_air.y = 80.0f;              /* 3-4-5: exactly 100 */
    CHECK_NEAR(wpLinkBoomerangAddVelSqrt(wp, 1.0f), 90.0f, 1e-4f);
    wp->physics.vel_air.x = 6.0f;
    wp->physics.vel_air.y = 8.0f;               /* exactly 10 */
    CHECK_NEAR(wpLinkBoomerangSubVelSqrt(wp, 1.4f), 10.0f, 1e-4f);
    CHECK_NEAR(wpLinkBoomerangAddVelSqrt(wp, 5.0f), 15.0f, 1e-4f);

    /* UpdateVelLR re-derives x/y from the angle and re-faces the weapon
     * off the quadrant -- the left half of the compass is lr -1 */
    wp->weapon_vars.boomerang.default_angle = 0.0f;
    wpLinkBoomerangUpdateVelLR(wp, 50.0f);
    CHECK_NEAR(wp->physics.vel_air.x, 50.0f, 1e-3f);
    CHECK(wp->lr == +1);
    wp->weapon_vars.boomerang.default_angle = F_CST_DTOR32(180.0F);
    wpLinkBoomerangUpdateVelLR(wp, 50.0f);
    CHECK_NEAR(wp->physics.vel_air.x, -50.0f, 1e-3f);
    CHECK(wp->lr == -1);

    /* with no parent there is no homing and no distance */
    wp->weapon_vars.boomerang.parent_gobj = NULL;
    CHECK_EQF(wpLinkBoomerangGetDistUpdateAngle(wg), 0.0f);
    wp->weapon_vars.boomerang.parent_gobj = mock_gobj;

    /* ProcHop turns it by twice the shield's angle, either way */
    wp->weapon_vars.boomerang.default_angle = F_CST_DTOR32(90.0F);
    wp->shield_collide_dir.z = 1.0f;
    wp->shield_collide_angle = F_CST_DTOR32(20.0F);
    CHECK(wpLinkBoomerangProcHop(wg) == FALSE);
    CHECK_NEAR(wp->weapon_vars.boomerang.default_angle, F_CST_DTOR32(130.0F), 1e-4f);
    wp->shield_collide_dir.z = -1.0f;
    CHECK(wpLinkBoomerangProcHop(wg) == FALSE);
    CHECK_NEAR(wp->weapon_vars.boomerang.default_angle, F_CST_DTOR32(90.0F), 1e-4f);

    /* ---- the catch: the one place the weapon calls back into a
     * fighter's file, and the branch that tells Link from Kirby. Only a
     * RETURNING boomerang inside 180 units is caught, and only from an
     * interruptible throw. ---- */
    wp->weapon_vars.boomerang.flags = flag_return;
    fp.passive_vars.link.boomerang_gobj = wg;
    fp.is_special_interrupt = FALSE;
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    wpLinkBoomerangCheckOwnerCatch(wg, 200.0f);         /* too far */
    CHECK_STATUS(nFTCommonStatusWait);
    CHECK(fp.passive_vars.link.boomerang_gobj == wg);

    wpLinkBoomerangCheckOwnerCatch(wg, 100.0f);         /* close, but not
                                                         * interruptible */
    CHECK_STATUS(nFTCommonStatusWait);
    /* the boomerang is still cleared and destroyed either way: the catch
     * animation is what the interrupt flag gates, not the pickup */
    CHECK(fp.passive_vars.link.boomerang_gobj == NULL);
    CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);

    /* again, this time interruptible: Link takes it back into SpecialNGet */
    fp.status_vars.link.specialn.is_smash = FALSE;
    wg = wpLinkBoomerangMakeWeapon(mock_gobj, &pos);
    CHECK(wg != NULL);
    if (wg == NULL)
    {
        fp.fkind = nFTKindMario;
        return;
    }
    wp = wpGetStruct(wg);
    wp->weapon_vars.boomerang.flags = flag_return;
    fp.passive_vars.link.boomerang_gobj = wg;
    fp.is_special_interrupt = TRUE;
    fp.ga = nMPKineticsGround;
    wpLinkBoomerangCheckOwnerCatch(wg, 100.0f);
    CHECK_STATUS(nFTLinkStatusSpecialNGet);
    CHECK(fp.passive_vars.link.boomerang_gobj == NULL);
    CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);

    /* ---- and the Kirby branch: the SAME weapon file picks the other
     * owner field and the other file's setter off fp->fkind ---- */
    fp.fkind = nFTKindKirby;
    fp.passive_vars.kirby.copylink_boomerang_gobj = NULL;
    fp.status_vars.kirby.copylink_specialn.is_smash = TRUE;
    wg = wpLinkBoomerangMakeWeapon(mock_gobj, &pos);
    CHECK(wg != NULL);
    if (wg == NULL)
    {
        fp.fkind = nFTKindMario;
        return;
    }
    wp = wpGetStruct(wg);
    /* a smash throw is faster and lives longer -- read through Kirby's
     * copy of the status var, which is the union alias the weapon file
     * inherits by writing fp->status_vars.link.specialn.is_smash */
    CHECK(wp->lifetime == WPBOOMERANG_LIFETIME_SMASH);
    CHECK_EQF(wp->physics.vel_air.x, WPBOOMERANG_VEL_SMASH);

    wp->weapon_vars.boomerang.flags = flag_return;
    fp.passive_vars.kirby.copylink_boomerang_gobj = wg;
    /* ftMainSetStatus clears is_special_interrupt, so the flag goes on
     * after the reset to Wait, not before it */
    ftMainSetStatus(mock_gobj, nFTCommonStatusWait, 0.0f, 1.0f, FTSTATUS_PRESERVE_NONE);
    fp.is_special_interrupt = TRUE;
    fp.ga = nMPKineticsAir;
    wpLinkBoomerangCheckOwnerCatch(wg, 100.0f);
    CHECK_STATUS(nFTKirbyStatusCopyLinkSpecialAirNReturn);
    CHECK(fp.passive_vars.kirby.copylink_boomerang_gobj == NULL);
    CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);

    /* ---- a KO takes a boomerang still in flight off the
     * field (ftCommonDeadResetCommonVars -> ftManagerDestroyFighter-
     * Weapons), Link's and Kirby's copy alike ---- */
    fp.fkind = nFTKindLink;
    fp.passive_vars.link.boomerang_gobj = NULL;
    wg = wpLinkBoomerangMakeWeapon(mock_gobj, &pos);
    CHECK(wg != NULL);
    fp.passive_vars.link.boomerang_gobj = wg;
    ftManagerDestroyFighterWeapons(mock_gobj);
    CHECK(fp.passive_vars.link.boomerang_gobj == NULL);
    CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);
    fp.fkind = nFTKindKirby;
    fp.passive_vars.kirby.copylink_boomerang_gobj = NULL;
    wg = wpLinkBoomerangMakeWeapon(mock_gobj, &pos);
    CHECK(wg != NULL);
    fp.passive_vars.kirby.copylink_boomerang_gobj = wg;
    ftManagerDestroyFighterWeapons(mock_gobj);
    CHECK(fp.passive_vars.kirby.copylink_boomerang_gobj == NULL);
    CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);

    fp.ga = nMPKineticsGround;
    fp.is_special_interrupt = FALSE;
    fp.fkind = nFTKindMario;
}

/* Mario's Neutral-B accessory proc (ft/ftchar/ftmario/
 * ftmariospecialn.c, compiled straight from the decomp -- the fireball's
 * caller). ftMarioSpecialNProcAccessory runs every frame while Special-N
 * is set; gated on motion flag0 (the SpawnFireball animation event), it
 * reads the spawn joint's world position and hands it to
 * wpMarioFireballMakeWeapon with the item index its fkind selects
 * (Mario 0, Luigi 1). Drive it on the mock Mario: the gate, the flag0
 * clear, the index, and that the fireball lands where the joint is. */
extern GObj *gGCCommonLinks[];
extern MPGeometryData *gMPCollisionGeometry;

static void test_ft_mario_specialn_accessory(void)
{
    static WPAttributes attr;
    u16 saved_lines, saved_mapobjs;
    DObj *saved_j16;
    Vec3f jp;
    GObj *wg;

    spawn(0.0f, 0.0f);                       /* the mock Mario, joints built */
    CHECK(fp.fkind == nFTKindMario);

    /* the accessory reads joints[FTMARIO_FIREBALL_SPAWN_JOINT]; the mock's
     * tree stops at the limb (joint 6), so point 16 at it -- the proc only
     * asks for a world position, not a particular bone. Save the old slot
     * and put it back before returning: leaving 16 aliased to 6 makes the
     * fighter teardown walk the shared DObj twice and corrupts the heap the
     * next spawn draws from. */
    saved_j16 = fp.joints[FTMARIO_FIREBALL_SPAWN_JOINT];
    fp.joints[FTMARIO_FIREBALL_SPAWN_JOINT] = fp.joints[6];

    /* the collision attributes wpManagerMakeWeapon reads, redirected onto a
     * local the same way test_wp_mario_fireball does */
    memset(&attr, 0, sizeof(attr));
    attr.size = 8;
    attr.attack_count = 1;
    gFTMarioFileSpecial1 =
        (void*)((char*)&attr - (intptr_t)&llMarioSpecial1FireballWeaponAttributes);

    /* wpMarioFireballMakeWeapon spawns COLLPROJECT|PARENT_FIGHTER, which
     * projects the fresh weapon down onto the stage floor. The weapon's
     * transform is still identity-less this early, so inverting it against
     * the mock floor divides by a zero determinant (gcSetInvMatrix spins
     * forever). test_wp_mario_fireball dodges this by having no stage; here
     * a mock stage is loaded, so empty its geometry (lines and mapobjs)
     * across the spawn -- the projection then finds no floor and leaves the
     * fireball at the joint. The joint read walks the fighter's own
     * matrices, not this geometry, so it is unaffected. Real stages project
     * cleanly (the disc probe is where the grounded fireball's floor snap
     * is proven). */
    saved_lines = gMPCollisionGeometry->yakumono_count;
    saved_mapobjs = gMPCollisionGeometry->mapobj_count;
    gMPCollisionGeometry->yakumono_count = 0;
    gMPCollisionGeometry->mapobj_count = 0;

    wpManagerAllocWeapons();                 /* fresh pool + empty weapon link */
    CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);

    /* the joint's world position, computed the same way the proc will */
    jp.x = jp.y = jp.z = 0.0f;
    gmCollisionGetFighterPartsWorldPosition(fp.joints[6], &jp);

    /* gate closed: flag0 == 0 spawns nothing */
    fp.motion_vars.flags.flag0 = FALSE;
    ftMarioSpecialNProcAccessory(mock_gobj);
    CHECK(gGCCommonLinks[nGCCommonLinkIDWeapon] == NULL);

    /* gate open: one fireball, index 0, at the joint, flag0 consumed */
    fp.motion_vars.flags.flag0 = TRUE;
    ftMarioSpecialNProcAccessory(mock_gobj);
    CHECK(fp.motion_vars.flags.flag0 == FALSE);
    wg = gGCCommonLinks[nGCCommonLinkIDWeapon];
    CHECK(wg != NULL);
    if (wg != NULL)
    {
        Vec3f *t = &DObjGetStruct(wg)->translate.vec.f;
        CHECK(wpGetStruct(wg)->weapon_vars.fireball.index == 0);
        CHECK_NEAR(t->x, jp.x, 1e-3f);
        CHECK_NEAR(t->y, jp.y, 1e-3f);
        CHECK_NEAR(t->z, jp.z, 1e-3f);
        wpMainDestroyWeapon(wg);             /* proper teardown: returns the pool struct */
    }

    /* Luigi's fkind selects index 1 (the same procs serve both -- Luigi is
     * a Mario clone), the gate opens once more */
    fp.fkind = nFTKindLuigi;
    gFTDataLuigiSpecial1 =
        (void*)((char*)&attr - (intptr_t)&llLuigiSpecial1FireballWeaponAttributes);
    fp.motion_vars.flags.flag0 = TRUE;
    ftMarioSpecialNProcAccessory(mock_gobj);
    wg = gGCCommonLinks[nGCCommonLinkIDWeapon];
    CHECK(wg != NULL);
    if (wg != NULL)
    {
        CHECK(wpGetStruct(wg)->weapon_vars.fireball.index == 1);
        wpMainDestroyWeapon(wg);
    }

    gMPCollisionGeometry->yakumono_count = saved_lines;
    gMPCollisionGeometry->mapobj_count = saved_mapobjs;
    fp.joints[FTMARIO_FIREBALL_SPAWN_JOINT] = saved_j16;  /* undo the alias */
    fp.fkind = nFTKindMario;                 /* as the next test expects */
}

/* The flip is the Turn script's: WaitAsync(6) then SetFlag1(1), so the
 * facing changes on the frame the animation reaches 6, not before.
 * */
static void test_turn(void)
{
    spawn(0.0f, 0.0f);
    CHECK(fp.lr == 1);
    frame(-80, 0, 0, 0, 0);
    CHECK_STATUS(nFTCommonStatusTurn);
    CHECK(fp.slope_contour == 3);           /* SetSlopeContour(3), frame 0 */
    idle_frames(3);
    CHECK(fp.lr == 1);                      /* not yet */
    idle_frames(2);
    CHECK(fp.lr == -1);
    CHECK(fp.motion_vars.flags.flag1 == 0); /* consumed by the flip */
    CHECK(fp.status_vars.common.turn.is_disable_sa_interrupts);
}
