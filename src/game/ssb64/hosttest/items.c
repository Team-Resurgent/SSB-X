/* hosttest/items.c -- part of hosttest_ft.c: the weapon and item pools, then every item one by one, and
 * gmrumble (the rumble events items and hits raise).
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* wp/wpmanager.c: the WPStruct pool every projectile is drawn from.
 * The pool half of the weapon manager stands on its own -- a free list
 * over WEAPON_ALLOC_MAX structs and a never-zero group counter -- so it
 * is verified through its own API against wp/wpmanager.c's own logic. */
extern WPStruct *sWPManagerStructsAllocFree;
extern s32 sWPManagerDisplayMode;
extern u32 sWPManagerGroupID;

static void test_wp_pool(void)
{
    WPStruct *got[WEAPON_ALLOC_MAX];
    WPStruct *extra;
    WPStruct *freed;
    int i, j;

    /* The scene heap is already live from the tests above (the last one
     * spawned fighters), which is the state scVSBattle has when it calls
     * wpManagerAllocWeapons (scvsbattle.c:232) -- so this does not reset
     * the scene (a load_stage() here re-cuts the object pools and would
     * strand the camera/links the tests below inherit); it just draws
     * the 27KB pool out of the same heap syTaskmanMalloc hands the game. */
    wpManagerAllocWeapons();

    /* AllocWeapons seeds the counter at 1 and the display mode at Master */
    CHECK(sWPManagerStructsAllocFree != NULL);
    CHECK(sWPManagerGroupID == 1);
    CHECK(sWPManagerDisplayMode == nDBDisplayModeMaster);

    /* exactly WEAPON_ALLOC_MAX structs come out, each 8-aligned and
     * distinct; the pool is then exhausted and hands back NULL */
    for (i = 0; i < WEAPON_ALLOC_MAX; i++)
    {
        got[i] = wpManagerGetNextStructAlloc();
        CHECK(got[i] != NULL);
        CHECK(((uintptr_t)got[i] & 0x7) == 0);
    }
    for (i = 0; i < WEAPON_ALLOC_MAX; i++)
        for (j = i + 1; j < WEAPON_ALLOC_MAX; j++)
            CHECK(got[i] != got[j]);

    extra = wpManagerGetNextStructAlloc();
    CHECK(extra == NULL);

    /* SetPrev pushes one back; the next Get returns that same one (LIFO) */
    wpManagerSetPrevStructAlloc(got[WEAPON_ALLOC_MAX - 1]);
    freed = wpManagerGetNextStructAlloc();
    CHECK(freed == got[WEAPON_ALLOC_MAX - 1]);
    CHECK(wpManagerGetNextStructAlloc() == NULL);

    /* GetGroupID hands out a rising id and advances the counter */
    sWPManagerGroupID = 1;
    CHECK(wpManagerGetGroupID() == 1);
    CHECK(wpManagerGetGroupID() == 2);
    CHECK(sWPManagerGroupID == 3);

    /* the wrap guard: 0xFFFFFFFF is a legal id to return, but the
     * counter is never left at 0 (0 is the "no group" sentinel) */
    sWPManagerGroupID = 0xFFFFFFFFu;
    CHECK(wpManagerGetGroupID() == 0xFFFFFFFFu);
    CHECK(sWPManagerGroupID == 1);
    CHECK(wpManagerGetGroupID() == 1);
    CHECK(sWPManagerGroupID == 2);
}

/* src/dc/itmanager.c: the ITStruct free list and the two
 * asset-free pieces of it/itmanager.c's own bookkeeping. This builds the
 * pool by hand, the same shape itManagerInitItems' own loop uses, rather
 * than calling it: the pool here is a fixture the test allocates and
 * frees repeatedly, where itManagerInitItems' is the scene's, allocated
 * once. test_it_init_items below is the one that calls the real thing. */
static void test_it_pool(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    ITStruct *got[ITEM_ALLOC_MAX];
    ITStruct *extra;
    ITStruct *freed;
    ITStruct *save_free = gITManagerStructsAllocFree;
    MPGroundData *save_ground = gMPCollisionGroundData;
    MPGroundData synth_ground;
    MPItemWeights synth_weights;
    s32 save_appearance_rate = gSCManagerBattleState->item_appearance_rate;
    u32 save_toggles = gSCManagerBattleState->item_toggles;
    int i, j;

    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
        pool[i].next = &pool[i + 1];
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    /* exactly ITEM_ALLOC_MAX structs come out, each distinct, then NULL */
    for (i = 0; i < ITEM_ALLOC_MAX; i++)
    {
        got[i] = itManagerGetNextStructAlloc();
        CHECK(got[i] != NULL);
    }
    for (i = 0; i < ITEM_ALLOC_MAX; i++)
        for (j = i + 1; j < ITEM_ALLOC_MAX; j++)
            CHECK(got[i] != got[j]);

    extra = itManagerGetNextStructAlloc();
    CHECK(extra == NULL);
    CHECK(itManagerGetCurrentAlloc() == NULL);

    /* SetPrev pushes one back; the next Get returns that same one (LIFO) */
    itManagerSetPrevStructAlloc(got[ITEM_ALLOC_MAX - 1]);
    CHECK(itManagerGetCurrentAlloc() == got[ITEM_ALLOC_MAX - 1]);
    freed = itManagerGetNextStructAlloc();
    CHECK(freed == got[ITEM_ALLOC_MAX - 1]);
    CHECK(itManagerGetNextStructAlloc() == NULL);

    gITManagerStructsAllocFree = save_free;

    /* itManagerInitMonsterVars: 44 - 32 Pokémon, curr/prev both "none" */
    gITManagerMonsterData.monster_curr = gITManagerMonsterData.monster_prev = 5;
    gITManagerMonsterData.monsters_num = 0;
    itManagerInitMonsterVars();
    CHECK(gITManagerMonsterData.monster_curr == U8_MAX);
    CHECK(gITManagerMonsterData.monster_prev == U8_MAX);
    CHECK(gITManagerMonsterData.monsters_num == (nITKindMBallMonsterEnd - nITKindMBallMonsterStart));

    /* itManagerSetupContainerDrops: on the port today gMPCollisionGroundData
     * ->item_weights is always NULL (no stage exports one yet, src/dc/
     * stage.c's own note), so the real function always takes its final
     * `else` and zeroes weights_sum -- prove that first. */
    gITManagerRandomWeights.weights_sum = 0xBEEF;
    gSCManagerBattleState->item_appearance_rate = nSCBattleItemSwitchLow;
    gSCManagerBattleState->item_toggles = ~0u;
    CHECK(gMPCollisionGroundData->item_weights == NULL);
    itManagerSetupContainerDrops();
    CHECK(gITManagerRandomWeights.weights_sum == 0);

    /* Then drive the real weighted branch through a hand-installed
     * MPGroundData/MPItemWeights, the same "restore after" shape
     * used for dEFGroundDatas[nGRKindCastle] -- proving the arithmetic
     * itself once a stage's weights do exist. nITKindUtilityStart..End is
     * sixteen slots (Tomato..Poké Ball); touch the first six: two toggled
     * with weights 10/20, one toggled-on with weight 0 (counted in
     * weights_sum, not in valids_num), one toggled-off with a weight
     * (ignored), one more toggled with weight 30, one untoggled and zero. */
    memset(&synth_weights, 0, sizeof(synth_weights));
    synth_weights.values[nITKindUtilityStart + 0] = 10;
    synth_weights.values[nITKindUtilityStart + 1] = 20;
    synth_weights.values[nITKindUtilityStart + 2] = 0;  /* toggled on, weight 0 */
    synth_weights.values[nITKindUtilityStart + 3] = 99; /* toggled off */
    synth_weights.values[nITKindUtilityStart + 4] = 30;
    synth_weights.values[nITKindUtilityStart + 5] = 0;  /* toggled off, weight 0 */

    synth_ground = *gMPCollisionGroundData;
    synth_ground.item_weights = &synth_weights;
    gMPCollisionGroundData = &synth_ground;

    gSCManagerBattleState->item_appearance_rate = nSCBattleItemSwitchLow;
    /* item_toggles is a bitmask over the ABSOLUTE nITKind values (the
     * function reads `item_toggles >> nITKindUtilityStart` once and then
     * `>>= 1` per loop step, so bit i's net shift by the time index i is
     * tested is exactly i) -- not over the 0-based offset into values[]. */
    gSCManagerBattleState->item_toggles =
        (1u << (nITKindUtilityStart + 0)) | (1u << (nITKindUtilityStart + 1)) |
        (1u << (nITKindUtilityStart + 2)) | (1u << (nITKindUtilityStart + 4));

    itManagerSetupContainerDrops();

    /* any_weights = 10+20+0+30 = 60 (every toggled slot, zero-weight ones
     * included); valid (toggled AND nonzero) = the 10/20/30 rows, three of
     * them -> valids_num = 3+1 = 4 (the ++ before the malloc sizes the
     * array for the Poké-Ball trailer row too); weights_sum = 60 + ceil(6) */
    CHECK(gITManagerRandomWeights.weights_sum == 66);
    CHECK(gITManagerRandomWeights.valids_num == 4);
    CHECK(gITManagerRandomWeights.kinds[0] == nITKindUtilityStart + 0);
    CHECK(gITManagerRandomWeights.blocks[0] == 0);
    CHECK(gITManagerRandomWeights.kinds[1] == nITKindUtilityStart + 1);
    CHECK(gITManagerRandomWeights.blocks[1] == 10);
    CHECK(gITManagerRandomWeights.kinds[2] == nITKindUtilityStart + 4);
    CHECK(gITManagerRandomWeights.blocks[2] == 30);
    CHECK(gITManagerRandomWeights.kinds[3] == nITKindMBallMonsterStart);
    CHECK(gITManagerRandomWeights.blocks[3] == 60);

    gMPCollisionGroundData = save_ground;
    gSCManagerBattleState->item_appearance_rate = save_appearance_rate;
    gSCManagerBattleState->item_toggles = save_toggles;
}

/* Not declared in any decomp header -- itmanager.c's own decomp source
 * never externs these either, since their only reader/writer besides
 * itManagerMakeItem's neighbours all live in that same file. */
extern u16 dITManagerAppearanceRatesMin[];
extern u16 dITManagerAppearanceRatesMax[];
extern ITAppearActor gITManagerAppearActor;

/* ---- the two reloc files, faked at their own offsets ----------------
 *
 * Several item functions reach their data through `itGetPData(ip, A, B)`,
 * which is `attr->data - A + B` (it/item.h:41) over offsets the decomp
 * names by SYMBOL (src/dc/itemoffsets.h). A test drives those by pointing
 * `attr->data` at a buffer plus the block's own offset, so both terms land
 * inside the buffer.
 *
 * The buffers must be REAL and ZEROED, not a bare address like
 * `(void*)0x10000`: what comes back is a script the caller then runs, and
 * a zeroed word is opcode 0 (`nGCAnimEvent32End`) by construction -- the
 * point itgShell.c's own header makes -- so a joint pointed into one of
 * these ends immediately. An address that is not mapped does not.
 *
 * Sized past the largest offset any test names (ITCommonObject's
 * `NBumperWaitDisplayList` at 0x7af8); the real files are 3,392 and 79,584
 * bytes, and nothing here indexes past what it names. */
/* The events arrays `itGetAttackEvent(it_desc, off)` hands back -- that
 * macro is `*it_desc.p_file + off` (it/item.h:45) and every ITDesc's
 * `p_file` is `&gITManagerCommonData`, so a test points that global at the
 * faked file MINUS the block's offset and reads the block through this. */
#define host_attack_events(off) ((ITAttackEvent *)(sHostItemData + (off)))

static u8 sHostItemData[0x2000];        /* ITCommonData, relocData 0xFB */
static u8 sHostItemModels[0x8000];      /* ITCommonObject, relocData 0x56 */

static sb32 test_it_make_item_proc_update(GObj *g) { (void)g; return TRUE; }
static sb32 test_it_make_item_proc_map(GObj *g) { (void)g; return TRUE; }

/* src/dc/itmanager.c + lbcommon.c: the keystone. Unlike
 * itManagerInitItems, itManagerMakeItem takes its ITDesc as a parameter
 * -- lbRelocGetFileData is plain pointer arithmetic ((type)(file+offset)),
 * so a stack ITAttributes reached through a fake one-element "file" table
 * is a real, honest input, not a mock. gcMakeGObjSPAfter/AddDObjForGObj/
 * AddGObjDisplay/AddGObjProcess are all sys/objman.c, compiled unmodified
 * -- the same real GObj/DObj pool machinery the UI tests already drive
 * (gcMakeGObjSPAfter(nGCCommonKindInterface, ...) throughout this file). */
static void test_it_manager_make_item(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    ITStruct *save_free = gITManagerStructsAllocFree;
    ITAttributes attr;
    ITDesc desc;
    void *file_ptr;
    Vec3f pos = { 10.0F, 20.0F, 30.0F };
    Vec3f vel = { 1.0F, 2.0F, 3.0F };
    GObj *item_gobj;
    ITStruct *ip;
    int i;

    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
        pool[i].next = &pool[i + 1];
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    /* attr->data == NULL is the port's honest current state -- no item
     * pack is exported here, so MakeItem's asset
     * branch always takes the plain `gcAddDObjForGObj(item_gobj, NULL)`
     * else-arm, same shape SetupContainerDrops/MakeAppearActor already
     * have with their own still-empty inputs. */
    memset(&attr, 0, sizeof(attr));
    attr.data = NULL;
    attr.is_display_colanim = FALSE;
    attr.is_display_xlu = FALSE;
    attr.type = 3;
    attr.weight = 1; /* ub32:1 -- Heavy = 0, Light = 1 */
    attr.is_give_hitlag = TRUE;
    attr.drop_sfx = 11; attr.throw_sfx = 22; attr.smash_sfx = 33;
    attr.vel_scale = 50; /* F_PCT_TO_DEC(50) */
    attr.damage = 12;
    attr.element = nGMHitElementFire;
    attr.attack_offset0_x = 1; attr.attack_offset0_y = 2; attr.attack_offset0_z = 3;
    attr.attack_offset1_x = 4; attr.attack_offset1_y = 5; attr.attack_offset1_z = 6;
    attr.size = 20.0F;
    attr.angle = 45;
    attr.knockback_scale = 100; attr.knockback_weight = 50; attr.knockback_base = 20;
    attr.can_setoff = TRUE;
    attr.shield_damage = 8;
    attr.hit_sfx = 99;
    attr.priority = 2;
    attr.can_rehit_item = TRUE;
    attr.can_rehit_fighter = FALSE;
    attr.can_hop = TRUE;
    attr.can_reflect = FALSE;
    attr.can_shield = TRUE;
    attr.attack_count = 1;
    attr.hitstatus = nGMHitStatusNormal;
    attr.damage_coll_offset.x = 1.0F; attr.damage_coll_offset.y = 2.0F; attr.damage_coll_offset.z = 3.0F;
    attr.damage_coll_size.x = 10.0F; attr.damage_coll_size.y = 20.0F; attr.damage_coll_size.z = 30.0F;
    attr.map_coll_top = 10; attr.map_coll_center = 0; attr.map_coll_bottom = -10; attr.map_coll_width = 15;

    file_ptr = &attr;
    memset(&desc, 0, sizeof(desc));
    desc.kind = 7;
    desc.p_file = &file_ptr;
    desc.o_attributes = 0;
    desc.attack_state = nGMAttackStateNew;
    desc.proc_update = test_it_make_item_proc_update;
    desc.proc_map = test_it_make_item_proc_map;

    item_gobj = itManagerMakeItem(NULL, &desc, &pos, &vel, 0);
    CHECK(item_gobj != NULL);

    ip = itGetStruct(item_gobj);
    CHECK(ip->item_gobj == item_gobj);
    CHECK(ip->owner_gobj == NULL);
    CHECK(ip->kind == 7);
    CHECK(ip->type == 3);
    CHECK_EQF(ip->physics.vel_air.x, 1.0F);
    CHECK_EQF(ip->physics.vel_air.y, 2.0F);
    CHECK_EQF(ip->physics.vel_air.z, 3.0F);
    CHECK_EQF(ip->physics.vel_ground, 0.0F);
    CHECK(ip->attr == &attr);
    CHECK(ip->is_allow_pickup == FALSE);
    CHECK(ip->is_hold == FALSE);
    CHECK(ip->pickup_wait == ITEM_PICKUP_WAIT_DEFAULT);
    CHECK(ip->weight == 1);
    CHECK(ip->is_hitlag_victim == TRUE);
    CHECK(ip->drop_sfx == 11); CHECK(ip->throw_sfx == 22); CHECK(ip->smash_sfx == 33);
    CHECK_NEAR(ip->vel_scale, F_PCT_TO_DEC(50), 1e-6F);
    CHECK(ip->is_thrown == FALSE);

    /* MakeItem's own tail calls itProcessUpdateAttackPositions, which
     * advances a fresh New state straight to Transfer (its own New arm)
     * before MakeItem returns -- so the state observed here is one step
     * past what item_desc->attack_state supplied. */
    CHECK(ip->attack_coll.attack_state == nGMAttackStateTransfer);
    CHECK(ip->attack_coll.damage == 12);
    CHECK_EQF(ip->attack_coll.throw_mul, 1.0F);
    CHECK(ip->attack_coll.element == nGMHitElementFire);
    CHECK_EQF(ip->attack_coll.offsets[0].x, 1.0F);
    CHECK_EQF(ip->attack_coll.offsets[1].z, 6.0F);
    CHECK_EQF(ip->attack_coll.size, 10.0F); /* attr.size * 0.5F */
    CHECK(ip->attack_coll.angle == 45);
    CHECK(ip->attack_coll.can_setoff == TRUE);
    CHECK(ip->attack_coll.shield_damage == 8);
    CHECK(ip->attack_coll.fgm_id == 99);
    CHECK(ip->attack_coll.priority == 2);
    CHECK(ip->attack_coll.can_rehit_shield == FALSE);
    CHECK(ip->attack_coll.attack_count == 1);
    CHECK(ip->attack_coll.interact_mask == GMHITCOLLISION_FLAG_ALL);
    /* itMainClearAttackRecord already proven at test_it_main; just check
     * it actually ran (slot 0's group_id, cleared to 7). */
    CHECK(ip->attack_coll.attack_records[0].victim_flags.group_id == 7);

    CHECK(ip->damage_coll.hitstatus == nGMHitStatusNormal);
    CHECK_EQF(ip->damage_coll.offset.x, 1.0F);
    CHECK_EQF(ip->damage_coll.size.x, 5.0F); /* attr.damage_coll_size.x * 0.5F */
    CHECK(ip->damage_coll.interact_mask == GMHITCOLLISION_FLAG_ALL);

    CHECK(ip->coll_data.p_translate == &DObjGetStruct(item_gobj)->translate.vec.f);
    CHECK(ip->coll_data.p_lr == &ip->lr);
    CHECK(ip->coll_data.map_coll.top == 10);
    CHECK(ip->coll_data.map_coll.bottom == -10);
    CHECK(ip->coll_data.p_map_coll == &ip->coll_data.map_coll);
    CHECK(ip->coll_data.ignore_line_id == -1);

    CHECK(ip->proc_update == test_it_make_item_proc_update);
    CHECK(ip->proc_map == test_it_make_item_proc_map);
    CHECK(ip->proc_dead == NULL);

    CHECK_EQF(DObjGetStruct(item_gobj)->translate.vec.f.x, 10.0F);
    CHECK_EQF(DObjGetStruct(item_gobj)->translate.vec.f.y, 20.0F);
    CHECK_EQF(ip->coll_data.pos_prev.x, 10.0F);

    CHECK(ip->ga == nMPKineticsAir);

    /* pool exhaustion: no free ITStruct -> NULL back, no GObj built */
    gITManagerStructsAllocFree = NULL;
    CHECK(itManagerMakeItem(NULL, &desc, &pos, &vel, 0) == NULL);

    gITManagerStructsAllocFree = save_free;
}

/* it/itmanager.c:486-494 itManagerSetItemSpawnWait -- touches only
 * ITAppearActor.spawn_wait and the two appearance-rate tables (no
 * roster, unlike its caller itManagerAppearActorProcUpdate). */
static void test_it_manager_set_spawn_wait(void)
{
    u32 save_rate = gSCManagerBattleState->item_appearance_rate;

    /* rate index 0: both tables are I_SEC_TO_TICS(0) == 0, so the random
     * range is [0, 0) and spawn_wait always comes out exactly 0. */
    gSCManagerBattleState->item_appearance_rate = nSCBattleItemSwitchNone;
    gITManagerAppearActor.spawn_wait = 0xBEEF;
    itManagerSetItemSpawnWait();
    CHECK(gITManagerAppearActor.spawn_wait == 0);

    /* rate index 1 ("Low"? whatever nSCBattleItemSwitchNone+1 names):
     * spawn_wait must land in [min, max]. */
    gSCManagerBattleState->item_appearance_rate = nSCBattleItemSwitchNone + 1;
    itManagerSetItemSpawnWait();
    CHECK(gITManagerAppearActor.spawn_wait >= dITManagerAppearanceRatesMin[1]);
    CHECK(gITManagerAppearActor.spawn_wait <= dITManagerAppearanceRatesMax[1]);

    gSCManagerBattleState->item_appearance_rate = save_rate;
}

static int sItMapProcCount;
static void test_it_map_fake_proc(GObj *g) { (void)g; sItMapProcCount++; }
/* void, not sb32: itMapCheckDestroyDropped's fourth parameter is
 * `void (*proc_status)(GObj*)` (src/dc/itmap.c:283), and passing a function
 * of a different type through a pointer is undefined behaviour even when
 * the result is discarded -- which is why the compiler warns. */
static void test_it_map_fake_status(GObj *g) { (void)g; sItMapProcCount++; }

/* src/dc/itmap.c: itMapCheckCollideAllRebound/SetGroundRebound/SetGround/
 * SetAir are pure math over ITStruct/DObj -- driven the same way
 * test_wp_map drives wpMapCheckAllRebound/wpMapSetGround, with the same
 * numbers for the rebound case as a cross-check. The mpProcessUpdateMain-
 * driven functions (TestAllCollisionFlag and everything built on it) need
 * a live stage's collision lines to ever report a real hit, which no
 * stage exports yet (the same position wpProcess's own ProcSearchHit-
 * Weapon/ProcWeaponMain are in) -- so those are driven with a safe,
 * honest "nothing collided this frame" MPCollData (p_translate/p_map_coll
 * real, floor_line_id -1, every mask/diff zero) and checked for the
 * correct "no collision" composition and, where a proc_map/proc_status
 * callback is involved, that it is correctly NOT called. */
static void test_it_map(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    Vec3f pos;
    sb32 r;

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;

    /* itMapCheckCollideAllRebound: same numbers test_wp_map already
     * proved for wpMapCheckAllRebound -- a left wall newly touched this
     * frame (mask_prev clear -> mask_curr set), normal +x, launch
     * heading into it (vel.x < 0): reflect -4 across {1,0} -> +4, scale
     * by 0.8 -> 3.2, contact reported at (x+width, y+center, z). */
    tdobj.translate.vec.f.x = 10.0F;
    tdobj.translate.vec.f.y = 20.0F;
    tdobj.translate.vec.f.z = 30.0F;
    tip.coll_data.map_coll.width = 2.0F;
    tip.coll_data.map_coll.center = 1.0F;

    tip.coll_data.mask_prev = 0;
    tip.coll_data.mask_curr = MAP_FLAG_LWALL;
    tip.coll_data.lwall_angle.x = 1.0F;
    tip.coll_data.lwall_angle.y = 0.0F;
    tip.physics.vel_air.x = -4.0F;
    tip.physics.vel_air.y = 0.0F;
    r = itMapCheckCollideAllRebound(&titem, MAP_FLAG_MAIN_MASK, 0.8F, &pos);
    CHECK(r == TRUE);
    CHECK_NEAR(tip.physics.vel_air.x, 3.2F, 1e-4F);
    CHECK_NEAR(tip.physics.vel_air.y, 0.0F, 1e-4F);
    CHECK_NEAR(pos.x, 12.0F, 1e-4F);
    CHECK_NEAR(pos.y, 21.0F, 1e-4F);
    CHECK_NEAR(pos.z, 30.0F, 1e-4F);

    /* already touched last frame -> not "newly" collided, no reflect */
    tip.coll_data.mask_prev = MAP_FLAG_LWALL;
    tip.coll_data.mask_curr = MAP_FLAG_LWALL;
    tip.physics.vel_air.x = -4.0F;
    r = itMapCheckCollideAllRebound(&titem, MAP_FLAG_MAIN_MASK, 0.8F, NULL);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, -4.0F, 1e-4F);

    /* newly touched but moving away from the wall (sim >= 0): no bounce */
    tip.coll_data.mask_prev = 0;
    tip.coll_data.mask_curr = MAP_FLAG_LWALL;
    tip.physics.vel_air.x = 4.0F;
    r = itMapCheckCollideAllRebound(&titem, MAP_FLAG_MAIN_MASK, 0.8F, NULL);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 4.0F, 1e-4F);

    /* itMapSetGroundRebound: vel (3,4), floor_angle (0,1) (unit normal,
     * pointing straight up), ground_rebound 0.5 -> (0.75, 0.25), worked
     * by hand and confirmed in Python before writing this in. */
    {
        Vec3f vel = { 3.0F, 4.0F, 0.0F };
        Vec3f floor_angle = { 0.0F, 1.0F, 0.0F };
        itMapSetGroundRebound(&vel, &floor_angle, 0.5F);
        CHECK_NEAR(vel.x, 0.75F, 1e-4F);
        CHECK_NEAR(vel.y, 0.25F, 1e-4F);
    }

    /* itMapSetGround/SetAir: ground velocity = air x-velocity * facing,
     * same numbers as test_wp_map's wpMapSetGround/SetAir. */
    tip.lr = 1;
    tip.physics.vel_air.x = 3.2F;
    itMapSetGround(&tip);
    CHECK(tip.ga == nMPKineticsGround);
    CHECK_NEAR(tip.physics.vel_ground, 3.2F, 1e-4F);
    tip.lr = -1;
    itMapSetGround(&tip);
    CHECK_NEAR(tip.physics.vel_ground, -3.2F, 1e-4F);
    itMapSetAir(&tip);
    CHECK(tip.ga == nMPKineticsAir);

    /* The mpProcessUpdateMain-driven family: a safe "nothing collided"
     * MPCollData (real p_translate/p_map_coll, no line IDs, zero diff/
     * masks) -- every one of these must come back FALSE/no-callback.
     * mpCollisionCheckExistLineID treats -1 as "should never happen"
     * (an infinite debug-print loop, mp/mpcollision.c:4093-4102) and -2
     * as its real "no line here" sentinel -- every *_line_id field below
     * has to be -2, not -1 or the field's own zero default, or the very
     * first mpProcess* call aborts.
     *
     * mpProcessCheckTestFloorCollisionAdjNew (and its wall/ceil twins)
     * pick between two internal variants by comparing coll_data->update_
     * tic against the live gMPCollisionUpdateTic -- "Diff" the first
     * time a coll_data is touched this tic, "Same" (which trusts fields
     * a prior call in the SAME tic already wrote) on every call after.
     * mpProcessUpdateMain sets update_tic to gMPCollisionUpdateTic on
     * every call, so reusing one coll_data across several of these test
     * calls without resetting it puts every call after the first onto
     * the "Same" path with stale cached fields -- not a port bug, real
     * decomp behaviour this test has no live scene/tic counter to drive
     * correctly. Reset fresh before each call instead of trying to
     * predict that caching by hand. */
    {
        int i;
        for (i = 0; i < 10; i++)
        {
            memset(&tip.coll_data, 0, sizeof(tip.coll_data));
            tip.coll_data.p_translate = &tdobj.translate.vec.f;
            tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
            tip.coll_data.pos_prev = tdobj.translate.vec.f;
            tip.coll_data.floor_line_id = -2;
            tip.coll_data.ceil_line_id = -2;
            tip.coll_data.lwall_line_id = -2;
            tip.coll_data.rwall_line_id = -2;
            tip.coll_data.ewall_line_id = -2;
            tip.coll_data.ignore_line_id = -2;

            switch (i)
            {
            case 0: CHECK(itMapTestAllCollisionFlag(&titem, MAP_FLAG_MAIN_MASK) == FALSE); break;
            case 1: CHECK(itMapTestAllCheckCollEnd(&titem) == FALSE); break;
            case 2: CHECK(itMapTestLRWallCheckFloor(&titem) == FALSE); break;

            case 3:
                sItMapProcCount = 0;
                CHECK(itMapCheckLRWallProcNoFloor(&titem, test_it_map_fake_proc) == FALSE);
                CHECK(sItMapProcCount == 1); /* no floor -> proc_map DOES run here */
                break;

            case 4:
                sItMapProcCount = 0;
                CHECK(itMapCheckMapProcAll(&titem, test_it_map_fake_proc) == FALSE);
                CHECK(sItMapProcCount == 0); /* no collision at all -> proc never runs */
                break;

            case 5:
                sItMapProcCount = 0;
                CHECK(func_ovl3_80173E9C(&titem, test_it_map_fake_proc) == FALSE);
                CHECK(sItMapProcCount == 0);
                break;

            case 6:
                sItMapProcCount = 0;
                CHECK(itMapCheckMapReboundProcNoFloor(&titem, 0.8F, test_it_map_fake_proc) == FALSE);
                /* proc_map only runs when TestAllCollisionFlag(FLOOR) is
                 * TRUE -- unlike LRWallProcNoFloor's inverted `== FALSE`
                 * guard above, this one gates on collision actually
                 * happening, so it never fires here. */
                CHECK(sItMapProcCount == 0);
                break;

            case 7:
                sItMapProcCount = 0;
                CHECK(itMapCheckLanding(&titem, 0.8F, 0.5F, test_it_map_fake_proc) == FALSE);
                CHECK(sItMapProcCount == 0); /* no floor landing -> proc_map never runs */
                break;

            case 8:
                sItMapProcCount = 0;
                CHECK(itMapCheckMapReboundProcAll(&titem, 0.8F, 0.5F, test_it_map_fake_proc) == FALSE);
                CHECK(sItMapProcCount == 0);
                break;

            case 9:
                CHECK(itMapCheckDestroyLanding(&titem, 0.8F) == FALSE);
                break;
            }
        }
    }

    memset(&tip.coll_data, 0, sizeof(tip.coll_data));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    sItMapProcCount = 0;
    tip.times_landed = 0;
    CHECK(itMapCheckDestroyDropped(&titem, 0.8F, 0.5F, test_it_map_fake_status) == FALSE);
    CHECK(sItMapProcCount == 0); /* never lands -> times_landed never advances */
    CHECK(tip.times_landed == 0);
}

/* Not declared in any decomp header -- it/itcommon/itstar.c's own decomp
 * source never externs these either, both being used only within that
 * same file there. */
extern ITDesc dITStarItemDesc;

/* src/dc/itstar.c: the first of the 21-item roster. proc_update/
 * proc_hit are pure ITStruct math already proven piece by piece (test_it_main's
 * gravity clamp and spin, test_it_manager_make_item's itMainClearAttackRecord/
 * itProcessUpdateAttackPositions composition) -- driven here together as
 * Star Man's own sequence. proc_map is the "nothing collided" composition
 * test_it_map's mpProcessUpdateMain family established (floor/ceil/lwall/
 * rwall/ewall line IDs at -2, the real "no line" sentinel; no stage exports
 * collision lines yet, so that is the only honest branch to drive). Make-
 * Item drives itManagerMakeItem for real with Star Man's own procs. */
static void test_it_star(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    sb32 r;

    /* dITStarItemDesc: static fields, verbatim against it/itcommon/itstar.c. */
    CHECK(dITStarItemDesc.kind == nITKindStar);
    CHECK(dITStarItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITStarItemDesc.o_attributes == (intptr_t)0x148);  /* reloc_data.us.h: llITCommonDataStarItemAttributes */
    CHECK(dITStarItemDesc.transform_types.tk1 == nGCMatrixKindTraRotRpyR);
    CHECK(dITStarItemDesc.transform_types.tk2 == nGCMatrixKindNull);
    CHECK(dITStarItemDesc.transform_types.tk3 == 0);
    CHECK(dITStarItemDesc.attack_state == nGMAttackStateOff);
    CHECK(dITStarItemDesc.proc_update == itStarCommonProcUpdate);
    CHECK(dITStarItemDesc.proc_map == itStarCommonProcMap);
    CHECK(dITStarItemDesc.proc_hit == itStarCommonProcHit);
    CHECK(dITStarItemDesc.proc_shield == NULL);
    CHECK(dITStarItemDesc.proc_hop == NULL);
    CHECK(dITStarItemDesc.proc_setoff == NULL);
    CHECK(dITStarItemDesc.proc_reflector == NULL);
    CHECK(dITStarItemDesc.proc_damage == NULL);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;

    /* itStarCommonProcHit: always TRUE. */
    CHECK(itStarCommonProcHit(&titem) == TRUE);

    /* itStarCommonProcUpdate: gravity clamp every call; the multi-
     * countdown reaches zero on the SECOND call here and refreshes the
     * attack collision (itMainRefreshAttackColl, in src/dc/itmain.c)
     * -- itMainClearAttackRecord resets the record (group_id
     * back to 7), attack_state goes to New, and itProcessUpdateAttackPositions'
     * own New arm advances it straight to Transfer and stamps pos_curr in
     * the same call, the same composition test_it_manager_make_item found
     * for itManagerMakeItem's own tail call. spin runs every call. */
    tip.attack_coll.attack_count = 1;
    tip.attack_coll.attack_state = nGMAttackStateOff;
    tip.attack_coll.offsets[0].x = 1.0F;
    tip.attack_coll.offsets[0].y = 2.0F;
    tip.attack_coll.offsets[0].z = 3.0F;
    tip.multi = 2;
    tip.spin_step = 5.0F;
    tdobj.translate.vec.f.x = 10.0F;
    tdobj.translate.vec.f.y = 20.0F;
    tdobj.translate.vec.f.z = 30.0F;

    r = itStarCommonProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITSTAR_GRAVITY, 1e-4F);
    CHECK(tip.multi == 1);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff); /* not yet refreshed */
    CHECK_NEAR(tdobj.rotate.vec.f.z, 5.0F, 1e-4F);

    r = itStarCommonProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -2.0F * ITSTAR_GRAVITY, 1e-4F);
    CHECK(tip.multi == 0);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateTransfer);
    CHECK(tip.attack_coll.attack_records[0].victim_flags.group_id == 7);
    CHECK_NEAR(tip.attack_coll.attack_pos[0].pos_curr.x, 11.0F, 1e-4F);
    CHECK_NEAR(tip.attack_coll.attack_pos[0].pos_curr.y, 22.0F, 1e-4F);
    CHECK_NEAR(tip.attack_coll.attack_pos[0].pos_curr.z, 33.0F, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 10.0F, 1e-4F);

    /* itStarCommonProcMap: the honest "nothing collided" composition (see
     * the file comment above). */
    memset(&tip.coll_data, 0, sizeof(tip.coll_data));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    tip.physics.vel_air.y = 7.0F;
    tip.lr = 1;
    r = itStarCommonProcMap(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, 7.0F, 1e-4F); /* untouched: no floor hit */
    CHECK(tip.lr == 1); /* untouched: no rebound, SetSpinVelLR never runs */

    /* itStarMakeItem: drives itManagerMakeItem for real. gITManagerCommon-
     * Data needs a real base: dITStarItemDesc.o_attributes is llITCommon-
     * DataStarItemAttributes, a real offset into ITCommonData
     * (src/dc/itemoffsets.h) -- so the override below computes the
     * base backward from a local ITAttributes, making the file+offset
     * arithmetic land on it exactly -- the same "hand-installed override" shape
     * stage_bind_collision already uses for gMPCollisionGroundData. */
    {
        static ITStruct pool[ITEM_ALLOC_MAX];
        ITStruct *save_free = gITManagerStructsAllocFree;
        void *save_common = gITManagerCommonData;
        static GObj cam_gobj;
        static CObj cam_cobj;
        GObj *save_camera = gGMCameraGObj;
        ITAttributes attr;
        Vec3f pos = { -100.0F, 5.0F, 0.0F };
        Vec3f vel = { 999.0F, 999.0F, 999.0F }; /* itStarMakeItem computes its own */
        GObj *item_gobj;
        ITStruct *ip;
        DObj *dobj;
        int i;

        for (i = 0; i < ITEM_ALLOC_MAX - 1; i++) pool[i].next = &pool[i + 1];
        pool[ITEM_ALLOC_MAX - 1].next = NULL;
        gITManagerStructsAllocFree = pool;

        memset(&attr, 0, sizeof(attr));
        attr.data = NULL; /* the attribute base is this test's own, see below */

        gITManagerCommonData = (void*)((uintptr_t)&attr - (uintptr_t)0x148);  /* reloc_data.us.h: llITCommonDataStarItemAttributes */

        memset(&cam_gobj, 0, sizeof(cam_gobj));
        memset(&cam_cobj, 0, sizeof(cam_cobj));
        cam_cobj.vec.at.x = 0.0F; /* camera looking at x=0 */
        cam_gobj.obj = &cam_cobj;
        gGMCameraGObj = &cam_gobj;

        /* pos.x (-100) < camera at.x (0) -> facing +x */
        item_gobj = itStarMakeItem(NULL, &pos, &vel, 0);
        CHECK(item_gobj != NULL);

        if (item_gobj != NULL)
        {
            ip = itGetStruct(item_gobj);
            dobj = DObjGetStruct(item_gobj);

            CHECK(ip->kind == nITKindStar);
            CHECK(ip->proc_update == itStarCommonProcUpdate);
            CHECK(ip->proc_map == itStarCommonProcMap);
            CHECK(ip->proc_hit == itStarCommonProcHit);
            CHECK_NEAR(ip->physics.vel_air.x, ITSTAR_VEL_X, 1e-4F);
            CHECK_NEAR(ip->physics.vel_air.y, ITSTAR_BOUNCE_Y, 1e-4F);
            CHECK_NEAR(ip->physics.vel_air.z, 0.0F, 1e-4F);
            CHECK(ip->attack_coll.interact_mask == GMHITCOLLISION_FLAG_FIGHTER);
            CHECK(ip->multi == ITSTAR_INTERACT_DELAY);
            CHECK(ip->is_unused_item_bool == TRUE);
            CHECK_NEAR(dobj->rotate.vec.f.z, 0.0F, 1e-4F);
            CHECK_NEAR(dobj->translate.vec.f.x, -100.0F, 1e-4F);
            CHECK_NEAR(dobj->translate.vec.f.y, 5.0F, 1e-4F);
        }

        /* facing the other way: pos.x (100) >= camera at.x (0) -> -x */
        pos.x = 100.0F;
        item_gobj = itStarMakeItem(NULL, &pos, &vel, 0);
        CHECK(item_gobj != NULL);
        if (item_gobj != NULL)
        {
            ip = itGetStruct(item_gobj);
            CHECK_NEAR(ip->physics.vel_air.x, -ITSTAR_VEL_X, 1e-4F);
        }

        gGMCameraGObj = save_camera;
        gITManagerCommonData = save_common;
        gITManagerStructsAllocFree = save_free;
    }
}

/* Not declared in any decomp header -- it/itcommon/ittomato.c's own decomp
 * source never externs these either, all three being used only within
 * that same file there. */
extern ITDesc dITTomatoItemDesc;
extern ITStatusDesc dITTomatoStatusDescs[];

enum
{
    nITTomatoStatusWait,
    nITTomatoStatusFall,
    nITTomatoStatusDropped
};

/* src/dc/ittomato.c: the roster's second entry. All seven of
 * the file's functions are in the port -- MakeItem's
 * tail's needs are all exported; this test
 * covers the six that are pure ITStruct math, and test_it_make_item_arrow
 * is the one that drives a MakeItem end to end. The Wait/Fall/
 * Dropped SetStatus trio is pure ITStruct math (itMainSetGroundAllow-
 * Pickup in itmain.c, and itMainSetStatus)
 * driven directly; the Map functions are the mpProcessUpdateMain-driven
 * family's honest "nothing collided" composition (test_it_map's own
 * reset discipline) -- except itTomatoWaitProcMap, whose own callback
 * (itMapCheckLRWallProcNoFloor's inverted guard) actually FIRES on "no
 * floor," giving a real, observable Wait->Fall transition rather than a
 * no-op. */
static void test_it_tomato(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    sb32 r;

    CHECK(dITTomatoItemDesc.kind == nITKindTomato);
    CHECK(dITTomatoItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITTomatoItemDesc.o_attributes == (intptr_t)0xb8);  /* reloc_data.us.h: llITCommonDataTomatoItemAttributes */
    CHECK(dITTomatoItemDesc.transform_types.tk1 == nGCMatrixKindTraRotRpyR);
    CHECK(dITTomatoItemDesc.transform_types.tk2 == nGCMatrixKindNull);
    CHECK(dITTomatoItemDesc.attack_state == nGMAttackStateOff);
    CHECK(dITTomatoItemDesc.proc_update == itTomatoFallProcUpdate);
    CHECK(dITTomatoItemDesc.proc_map == itTomatoFallProcMap);
    CHECK(dITTomatoItemDesc.proc_hit == NULL);

    CHECK(dITTomatoStatusDescs[nITTomatoStatusWait].proc_update == NULL);
    CHECK(dITTomatoStatusDescs[nITTomatoStatusWait].proc_map == itTomatoWaitProcMap);
    CHECK(dITTomatoStatusDescs[nITTomatoStatusFall].proc_update == itTomatoFallProcUpdate);
    CHECK(dITTomatoStatusDescs[nITTomatoStatusFall].proc_map == itTomatoFallProcMap);
    CHECK(dITTomatoStatusDescs[nITTomatoStatusDropped].proc_update == itTomatoFallProcUpdate);
    CHECK(dITTomatoStatusDescs[nITTomatoStatusDropped].proc_map == itTomatoDroppedProcMap);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;

    /* itTomatoFallProcUpdate: gravity + spin, same shape as itstar.c's
     * itStarCommonProcUpdate (test_it_star), no multi-countdown here. */
    tip.spin_step = 3.0F;
    r = itTomatoFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITTOMATO_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 3.0F, 1e-4F);

    /* itTomatoWaitSetStatus: itMainSetGroundAllowPickup's own fields
     * (is_allow_pickup/times_landed/attack_state/vel_air/ga), then the
     * dispatch to Wait's own procs. */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 5.0F; tip.physics.vel_air.y = 5.0F; tip.physics.vel_air.z = 5.0F;
    tip.times_landed = 3;
    tip.attack_coll.attack_state = nGMAttackStateNew;
    itTomatoWaitSetStatus(&titem);
    CHECK(tip.is_allow_pickup == TRUE);
    CHECK(tip.times_landed == 0);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK_NEAR(tip.physics.vel_air.y, 0.0F, 1e-4F);
    CHECK(tip.ga == nMPKineticsGround);
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == itTomatoWaitProcMap);

    /* itTomatoFallSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.is_allow_pickup = TRUE;
    itTomatoFallSetStatus(&titem);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_update == itTomatoFallProcUpdate);
    CHECK(tip.proc_map == itTomatoFallProcMap);

    /* itTomatoDroppedSetStatus: dead code in the decomp itself (no
     * ITStatusDesc row and no Map function ever transitions to it), kept
     * verbatim per the file header, still checked. */
    memset(&tip, 0, sizeof(tip));
    itTomatoDroppedSetStatus(&titem);
    CHECK(tip.proc_update == itTomatoFallProcUpdate);
    CHECK(tip.proc_map == itTomatoDroppedProcMap);

    /* itTomatoWaitProcMap: honest "no floor" reset -- itMapCheckLRWallProc-
     * NoFloor's guard is `== FALSE` (inverted), so the callback (Fall-
     * SetStatus) DOES run here, a real Wait->Fall transition. */
    memset(&tip, 0, sizeof(tip));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    tip.is_allow_pickup = TRUE;
    r = itTomatoWaitProcMap(&titem);
    CHECK(r == FALSE);
    CHECK(tip.is_allow_pickup == FALSE); /* itTomatoFallSetStatus ran */
    CHECK(tip.proc_map == itTomatoFallProcMap);

    /* itTomatoFallProcMap/DroppedProcMap: itMapCheckDestroyDropped's own
     * honest "nothing collided" composition (test_it_map already proved
     * this exact function) -- proc_status only fires once times_landed
     * reaches ITEM_LANDING_NUM_MAX on a REAL floor hit, so a no-collision
     * frame leaves it untouched. Reset fresh per call. */
    {
        int i;
        for (i = 0; i < 2; i++)
        {
            memset(&tip.coll_data, 0, sizeof(tip.coll_data));
            tip.coll_data.p_translate = &tdobj.translate.vec.f;
            tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
            tip.coll_data.pos_prev = tdobj.translate.vec.f;
            tip.coll_data.floor_line_id = -2;
            tip.coll_data.ceil_line_id = -2;
            tip.coll_data.lwall_line_id = -2;
            tip.coll_data.rwall_line_id = -2;
            tip.coll_data.ewall_line_id = -2;
            tip.coll_data.ignore_line_id = -2;
            tip.times_landed = 0;
            r = (i == 0) ? itTomatoFallProcMap(&titem) : itTomatoDroppedProcMap(&titem);
            CHECK(r == FALSE);
            CHECK(tip.times_landed == 0);
        }
    }
}

/* Not declared in any decomp header -- it/itcommon/itheart.c's own decomp
 * source never externs these either, all three being used only within
 * that same file there. */
extern ITDesc dITHeartItemDesc;
extern ITStatusDesc dITHeartStatusDescs[];

enum
{
    nITHeartStatusWait,
    nITHeartStatusFall,
    nITHeartStatusDropped
};

/* src/dc/itheart.c: the roster's third entry, nearly
 * line-for-line identical to ittomato.c -- same test shape,
 * mirrored with Heart's own constants/kind. MakeItem is in the port
 * along with every other roster item's, driven by
 * test_it_make_item_arrow rather than from here. */
static void test_it_heart(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    sb32 r;

    CHECK(dITHeartItemDesc.kind == nITKindHeart);
    CHECK(dITHeartItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITHeartItemDesc.o_attributes == (intptr_t)0x100);  /* reloc_data.us.h: llITCommonDataHeartItemAttributes */
    CHECK(dITHeartItemDesc.transform_types.tk1 == nGCMatrixKindTraRotRpyR);
    CHECK(dITHeartItemDesc.transform_types.tk2 == nGCMatrixKindNull);
    CHECK(dITHeartItemDesc.attack_state == nGMAttackStateOff);
    CHECK(dITHeartItemDesc.proc_update == itHeartFallProcUpdate);
    CHECK(dITHeartItemDesc.proc_map == itHeartFallProcMap);
    CHECK(dITHeartItemDesc.proc_hit == NULL);

    CHECK(dITHeartStatusDescs[nITHeartStatusWait].proc_update == NULL);
    CHECK(dITHeartStatusDescs[nITHeartStatusWait].proc_map == itHeartWaitProcMap);
    CHECK(dITHeartStatusDescs[nITHeartStatusFall].proc_update == itHeartFallProcUpdate);
    CHECK(dITHeartStatusDescs[nITHeartStatusFall].proc_map == itHeartFallProcMap);
    CHECK(dITHeartStatusDescs[nITHeartStatusDropped].proc_update == itHeartFallProcUpdate);
    CHECK(dITHeartStatusDescs[nITHeartStatusDropped].proc_map == itHeartDroppedProcMap);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;

    /* itHeartFallProcUpdate: gravity + spin. */
    tip.spin_step = 2.0F;
    r = itHeartFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITHEART_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 2.0F, 1e-4F);

    /* itHeartWaitSetStatus: itMainSetGroundAllowPickup's own fields. */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 5.0F; tip.physics.vel_air.y = 5.0F; tip.physics.vel_air.z = 5.0F;
    tip.times_landed = 3;
    tip.attack_coll.attack_state = nGMAttackStateNew;
    itHeartWaitSetStatus(&titem);
    CHECK(tip.is_allow_pickup == TRUE);
    CHECK(tip.times_landed == 0);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK(tip.ga == nMPKineticsGround);
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == itHeartWaitProcMap);

    /* itHeartFallSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.is_allow_pickup = TRUE;
    itHeartFallSetStatus(&titem);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_update == itHeartFallProcUpdate);
    CHECK(tip.proc_map == itHeartFallProcMap);

    /* itHeartDroppedSetStatus: dead code in the decomp itself, kept
     * verbatim per the file header, still checked. */
    memset(&tip, 0, sizeof(tip));
    itHeartDroppedSetStatus(&titem);
    CHECK(tip.proc_update == itHeartFallProcUpdate);
    CHECK(tip.proc_map == itHeartDroppedProcMap);

    /* itHeartWaitProcMap: honest "no floor" reset -- the inverted guard
     * makes the callback (FallSetStatus) actually fire. */
    memset(&tip, 0, sizeof(tip));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    tip.is_allow_pickup = TRUE;
    r = itHeartWaitProcMap(&titem);
    CHECK(r == FALSE);
    CHECK(tip.is_allow_pickup == FALSE); /* itHeartFallSetStatus ran */
    CHECK(tip.proc_map == itHeartFallProcMap);

    /* itHeartFallProcMap/DroppedProcMap: honest "nothing collided"
     * composition, reset fresh per call. */
    {
        int i;
        for (i = 0; i < 2; i++)
        {
            memset(&tip.coll_data, 0, sizeof(tip.coll_data));
            tip.coll_data.p_translate = &tdobj.translate.vec.f;
            tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
            tip.coll_data.pos_prev = tdobj.translate.vec.f;
            tip.coll_data.floor_line_id = -2;
            tip.coll_data.ceil_line_id = -2;
            tip.coll_data.lwall_line_id = -2;
            tip.coll_data.rwall_line_id = -2;
            tip.coll_data.ewall_line_id = -2;
            tip.coll_data.ignore_line_id = -2;
            tip.times_landed = 0;
            r = (i == 0) ? itHeartFallProcMap(&titem) : itHeartDroppedProcMap(&titem);
            CHECK(r == FALSE);
            CHECK(tip.times_landed == 0);
        }
    }
}

/* Not declared in any decomp header -- it/itcommon/itsword.c's own decomp
 * source never externs these either, all three being used only within
 * that same file there. */
extern ITDesc dITSwordItemDesc;
extern ITStatusDesc dITSwordStatusDescs[];

enum
{
    nITSwordStatusWait,
    nITSwordStatusFall,
    nITSwordStatusHold,
    nITSwordStatusThrown,
    nITSwordStatusDropped
};

/* src/dc/itsword.c: the roster's fourth entry, the first
 * held/thrown item -- a five-state machine (Wait/Fall as for the
 * other roster items, plus Hold/Thrown/Dropped). MakeItem is in the port
 * along with every other roster item's, driven by
 * test_it_make_item_arrow rather than from here.
 * itSwordThrownSetStatus/DroppedSetStatus dereference DObjGetStruct(...)
 * ->child unconditionally -- dead code in the current build (see the
 * file header) but still tested here with a real stack child DObj so
 * the dereference itself is safe. */
static void test_it_sword(void)
{
    GObj titem, towner;
    ITStruct tip;
    ITAttributes tattr;
    DObj tdobj, tchild;
    FTStruct tfp;
    sb32 r;

    CHECK(dITSwordItemDesc.kind == nITKindSword);
    CHECK(dITSwordItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITSwordItemDesc.o_attributes == (intptr_t)0x190);  /* reloc_data.us.h: llITCommonDataSwordItemAttributes */
    CHECK(dITSwordItemDesc.transform_types.tk1 == nGCMatrixKindTraRotRpyRSca);
    CHECK(dITSwordItemDesc.transform_types.tk2 == nGCMatrixKindNull);
    CHECK(dITSwordItemDesc.attack_state == nGMAttackStateOff);
    CHECK(dITSwordItemDesc.proc_update == itSwordFallProcUpdate);
    CHECK(dITSwordItemDesc.proc_map == itSwordFallProcMap);
    CHECK(dITSwordItemDesc.proc_hit == NULL);

    CHECK(dITSwordStatusDescs[nITSwordStatusWait].proc_map == itSwordWaitProcMap);
    CHECK(dITSwordStatusDescs[nITSwordStatusFall].proc_update == itSwordFallProcUpdate);
    CHECK(dITSwordStatusDescs[nITSwordStatusFall].proc_map == itSwordFallProcMap);
    CHECK(dITSwordStatusDescs[nITSwordStatusHold].proc_update == NULL);
    CHECK(dITSwordStatusDescs[nITSwordStatusHold].proc_map == NULL);
    CHECK(dITSwordStatusDescs[nITSwordStatusThrown].proc_update == itSwordFallProcUpdate);
    CHECK(dITSwordStatusDescs[nITSwordStatusThrown].proc_map == itSwordThrownProcMap);
    CHECK(dITSwordStatusDescs[nITSwordStatusThrown].proc_hit == itSwordThrownProcHit);
    CHECK(dITSwordStatusDescs[nITSwordStatusThrown].proc_shield == itSwordThrownProcHit);
    CHECK(dITSwordStatusDescs[nITSwordStatusThrown].proc_hop == itMainCommonProcHop);
    CHECK(dITSwordStatusDescs[nITSwordStatusThrown].proc_setoff == itSwordThrownProcHit);
    CHECK(dITSwordStatusDescs[nITSwordStatusThrown].proc_reflector == itMainCommonProcReflector);
    CHECK(dITSwordStatusDescs[nITSwordStatusDropped].proc_map == itSwordDroppedProcMap);
    CHECK(dITSwordStatusDescs[nITSwordStatusDropped].proc_hit == itSwordThrownProcHit);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tchild, 0, sizeof(tchild));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tdobj.child = &tchild;
    /* itSwordFallProcUpdate: gravity + spin. */
    tip.spin_step = 4.0F;
    r = itSwordFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITSWORD_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 4.0F, 1e-4F);
    /* itSwordWaitSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 5.0F; tip.physics.vel_air.y = 5.0F; tip.physics.vel_air.z = 5.0F;
    tip.times_landed = 3;
    tip.attack_coll.attack_state = nGMAttackStateNew;
    itSwordWaitSetStatus(&titem);
    CHECK(tip.is_allow_pickup == TRUE);
    CHECK(tip.times_landed == 0);
    CHECK(tip.ga == nMPKineticsGround);
    CHECK(tip.proc_map == itSwordWaitProcMap);
    /* itSwordFallSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.is_allow_pickup = TRUE;
    itSwordFallSetStatus(&titem);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_update == itSwordFallProcUpdate);
    CHECK(tip.proc_map == itSwordFallProcMap);
    /* itSwordHoldSetStatus: rotate.y reset, both procs go NULL (equipped,
     * the fighter's own carry animation drives it, not this item). */
    tdobj.rotate.vec.f.y = 1.5F;
    itSwordHoldSetStatus(&titem);
    CHECK_NEAR(tdobj.rotate.vec.f.y, F_CST_DTOR32(0.0F), 1e-6F);
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == NULL);
    /* itSwordThrownProcHit: rebound math, worked by hand and confirmed
     * in Python before writing this in (10 * -0.06 = -0.6; 20 * -0.3 +
     * 25 = 19). */
    tip.attack_coll.attack_state = nGMAttackStateNew;
    tip.physics.vel_air.x = 10.0F;
    tip.physics.vel_air.y = 20.0F;
    r = itSwordThrownProcHit(&titem);
    CHECK(r == FALSE);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK_NEAR(tip.physics.vel_air.x, -0.6F, 1e-4F);
    CHECK_NEAR(tip.physics.vel_air.y, 19.0F, 1e-4F);
    /* itSwordThrownSetStatus/DroppedSetStatus: dead code today (see the
     * file header) but still driven here with a real child DObj so the
     * unconditional ->child dereference is safe. */
    tchild.rotate.vec.f.y = 0.0F;
    itSwordThrownSetStatus(&titem);
    CHECK(tip.proc_map == itSwordThrownProcMap);
    CHECK_NEAR(tchild.rotate.vec.f.y, F_CST_DTOR32(90.0F), 1e-6F);

    tchild.rotate.vec.f.y = 0.0F;
    itSwordDroppedSetStatus(&titem);
    CHECK(tip.proc_map == itSwordDroppedProcMap);
    CHECK_NEAR(tchild.rotate.vec.f.y, F_CST_DTOR32(90.0F), 1e-6F);
    /* itMainCommonProcHop/Reflector, reached through this item's own
     * status table -- proven directly here since this is their first
     * it/ caller (they were wp/'s common proc bodies before).
     * itMainCommonProcHop's own tail calls itMainSetSpinVelLR ->
     * itMainSetCommonSpin, which reads ip->attr->spin_speed -- needs a
     * real (zeroed is fine) ITAttributes, not the NULL a bare memset
     * leaves it at. */
    memset(&tip, 0, sizeof(tip));
    memset(&tattr, 0, sizeof(tattr));
    memset(&towner, 0, sizeof(towner));
    memset(&tfp, 0, sizeof(tfp));
    tip.attr = &tattr;
    towner.user_data.p = &tfp;
    tip.owner_gobj = &towner;
    tip.shield_collide_dir.x = 0.0F;
    tip.shield_collide_dir.y = 1.0F;
    tip.shield_collide_dir.z = 0.0F;
    tip.shield_collide_angle = 0.0F; /* zero rotation -> vel_air unchanged, spin still runs */
    tip.physics.vel_air.x = 3.0F;
    tip.physics.vel_air.y = 0.0F;
    tip.physics.vel_air.z = 0.0F;
    r = itMainCommonProcHop(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 3.0F, 1e-4F);
    CHECK(tip.lr == 1); /* itMainSetSpinVelLR: vel_air.x >= 0 -> +1 */

    tfp.lr = 1;
    tip.physics.vel_air.x = -3.0F;
    r = itMainCommonProcReflector(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 3.0F, 1e-4F); /* (-3 * 1) < 0 -> flipped */
    tip.physics.vel_air.x = 3.0F;
    r = itMainCommonProcReflector(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 3.0F, 1e-4F); /* (3 * 1) >= 0 -> unchanged */
    /* itSwordWaitProcMap: honest "no floor" reset -- the inverted guard
     * makes the callback (FallSetStatus) actually fire. */
    memset(&tip, 0, sizeof(tip));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    tip.is_allow_pickup = TRUE;
    r = itSwordWaitProcMap(&titem);
    CHECK(r == FALSE);
    CHECK(tip.is_allow_pickup == FALSE); /* itSwordFallSetStatus ran */
    CHECK(tip.proc_map == itSwordFallProcMap);
    /* itSwordFallProcMap/ThrownProcMap/DroppedProcMap: honest "nothing
     * collided" composition, reset fresh per call. */
    {
        int i;
        for (i = 0; i < 3; i++)
        {
            memset(&tip.coll_data, 0, sizeof(tip.coll_data));
            tip.coll_data.p_translate = &tdobj.translate.vec.f;
            tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
            tip.coll_data.pos_prev = tdobj.translate.vec.f;
            tip.coll_data.floor_line_id = -2;
            tip.coll_data.ceil_line_id = -2;
            tip.coll_data.lwall_line_id = -2;
            tip.coll_data.rwall_line_id = -2;
            tip.coll_data.ewall_line_id = -2;
            tip.coll_data.ignore_line_id = -2;
            tip.times_landed = 0;
            switch (i)
            {
            case 0: r = itSwordFallProcMap(&titem); break;
            case 1: r = itSwordThrownProcMap(&titem); break;
            case 2: r = itSwordDroppedProcMap(&titem); break;
            }
            CHECK(r == FALSE);
            CHECK(tip.times_landed == 0);
        }
    }
}

/* Not declared in any decomp header -- it/itcommon/itbat.c's own decomp
 * source never externs these either, all three being used only within
 * that same file there. */
extern ITDesc dITBatItemDesc;
extern ITStatusDesc dITBatStatusDescs[];

enum
{
    nITBatStatusWait,
    nITBatStatusFall,
    nITBatStatusHold,
    nITBatStatusThrown,
    nITBatStatusDropped
};

/* src/dc/itbat.c: the roster's fifth entry, Sword's
 * closest twin -- same five-state shape, MakeItem left out for the
 * same reason. One real difference: the decomp keeps itBatThrownProcUpdate
 * as its own function (identical body to itBatFallProcUpdate) rather than
 * reusing FallProcUpdate directly the way itsword.c's Thrown row did --
 * tested as its own call, same numbers. */
static void test_it_bat(void)
{
    GObj titem, towner;
    ITStruct tip;
    ITAttributes tattr;
    DObj tdobj, tchild;
    FTStruct tfp;
    sb32 r;

    CHECK(dITBatItemDesc.kind == nITKindBat);
    CHECK(dITBatItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITBatItemDesc.o_attributes == (intptr_t)0x1d8);  /* reloc_data.us.h: llITCommonDataBatItemAttributes */
    CHECK(dITBatItemDesc.transform_types.tk1 == nGCMatrixKindTraRotRpyR);
    CHECK(dITBatItemDesc.transform_types.tk2 == nGCMatrixKindNull);
    CHECK(dITBatItemDesc.attack_state == nGMAttackStateOff);
    CHECK(dITBatItemDesc.proc_update == itBatFallProcUpdate);
    CHECK(dITBatItemDesc.proc_map == itBatFallProcMap);
    CHECK(dITBatItemDesc.proc_hit == NULL);

    CHECK(dITBatStatusDescs[nITBatStatusWait].proc_map == itBatWaitProcMap);
    CHECK(dITBatStatusDescs[nITBatStatusFall].proc_update == itBatFallProcUpdate);
    CHECK(dITBatStatusDescs[nITBatStatusFall].proc_map == itBatFallProcMap);
    CHECK(dITBatStatusDescs[nITBatStatusHold].proc_update == NULL);
    CHECK(dITBatStatusDescs[nITBatStatusHold].proc_map == NULL);
    CHECK(dITBatStatusDescs[nITBatStatusThrown].proc_update == itBatThrownProcUpdate);
    CHECK(dITBatStatusDescs[nITBatStatusThrown].proc_map == itBatThrownProcMap);
    CHECK(dITBatStatusDescs[nITBatStatusThrown].proc_hit == itBatThrownProcHit);
    CHECK(dITBatStatusDescs[nITBatStatusThrown].proc_shield == itBatThrownProcHit);
    CHECK(dITBatStatusDescs[nITBatStatusThrown].proc_hop == itMainCommonProcHop);
    CHECK(dITBatStatusDescs[nITBatStatusThrown].proc_setoff == itBatThrownProcHit);
    CHECK(dITBatStatusDescs[nITBatStatusThrown].proc_reflector == itMainCommonProcReflector);
    CHECK(dITBatStatusDescs[nITBatStatusDropped].proc_update == itBatFallProcUpdate);
    CHECK(dITBatStatusDescs[nITBatStatusDropped].proc_map == itBatDroppedProcMap);
    CHECK(dITBatStatusDescs[nITBatStatusDropped].proc_hit == itBatThrownProcHit);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tchild, 0, sizeof(tchild));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tdobj.child = &tchild;
    /* itBatFallProcUpdate: gravity + spin. */
    tip.spin_step = 4.0F;
    r = itBatFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITBAT_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 4.0F, 1e-4F);
    /* itBatThrownProcUpdate: same body, its own function -- proven
     * separately since the decomp keeps it separate. */
    memset(&tip, 0, sizeof(tip));
    tip.spin_step = 4.0F;
    r = itBatThrownProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITBAT_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 8.0F, 1e-4F); /* accumulated across both calls */
    /* itBatWaitSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 5.0F; tip.physics.vel_air.y = 5.0F; tip.physics.vel_air.z = 5.0F;
    tip.times_landed = 3;
    tip.attack_coll.attack_state = nGMAttackStateNew;
    itBatWaitSetStatus(&titem);
    CHECK(tip.is_allow_pickup == TRUE);
    CHECK(tip.times_landed == 0);
    CHECK(tip.ga == nMPKineticsGround);
    CHECK(tip.proc_map == itBatWaitProcMap);
    /* itBatFallSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.is_allow_pickup = TRUE;
    itBatFallSetStatus(&titem);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_update == itBatFallProcUpdate);
    CHECK(tip.proc_map == itBatFallProcMap);
    /* itBatHoldSetStatus: rotate.y reset, both procs go NULL. */
    tdobj.rotate.vec.f.y = 1.5F;
    itBatHoldSetStatus(&titem);
    CHECK_NEAR(tdobj.rotate.vec.f.y, F_CST_DTOR32(0.0F), 1e-6F);
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == NULL);
    /* itBatThrownProcHit: same rebound math as Sword's own (Python-verified
     * 10*-0.06=-0.6; 20*-0.3+25=19). */
    tip.attack_coll.attack_state = nGMAttackStateNew;
    tip.physics.vel_air.x = 10.0F;
    tip.physics.vel_air.y = 20.0F;
    r = itBatThrownProcHit(&titem);
    CHECK(r == FALSE);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK_NEAR(tip.physics.vel_air.x, -0.6F, 1e-4F);
    CHECK_NEAR(tip.physics.vel_air.y, 19.0F, 1e-4F);
    /* itBatThrownSetStatus/DroppedSetStatus: dead code today, driven here
     * against a real child DObj so the unconditional ->child dereference
     * is safe. */
    tchild.rotate.vec.f.y = 0.0F;
    itBatThrownSetStatus(&titem);
    CHECK(tip.proc_map == itBatThrownProcMap);
    CHECK_NEAR(tchild.rotate.vec.f.y, F_CST_DTOR32(90.0F), 1e-6F);

    tchild.rotate.vec.f.y = 0.0F;
    itBatDroppedSetStatus(&titem);
    CHECK(tip.proc_map == itBatDroppedProcMap);
    CHECK_NEAR(tchild.rotate.vec.f.y, F_CST_DTOR32(90.0F), 1e-6F);
    /* itMainCommonProcHop/Reflector, same shape test_it_sword already
     * proved -- ip->attr must point at a real ITAttributes. */
    memset(&tip, 0, sizeof(tip));
    memset(&tattr, 0, sizeof(tattr));
    memset(&towner, 0, sizeof(towner));
    memset(&tfp, 0, sizeof(tfp));
    tip.attr = &tattr;
    towner.user_data.p = &tfp;
    tip.owner_gobj = &towner;
    tip.shield_collide_dir.x = 0.0F;
    tip.shield_collide_dir.y = 1.0F;
    tip.shield_collide_dir.z = 0.0F;
    tip.shield_collide_angle = 0.0F;
    tip.physics.vel_air.x = 3.0F;
    tip.physics.vel_air.y = 0.0F;
    tip.physics.vel_air.z = 0.0F;
    r = itMainCommonProcHop(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 3.0F, 1e-4F);
    CHECK(tip.lr == 1);

    tfp.lr = 1;
    tip.physics.vel_air.x = -3.0F;
    r = itMainCommonProcReflector(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 3.0F, 1e-4F);
    tip.physics.vel_air.x = 3.0F;
    r = itMainCommonProcReflector(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 3.0F, 1e-4F);
    /* itBatWaitProcMap: honest "no floor" reset -- the inverted guard
     * makes the callback (FallSetStatus) actually fire. */
    memset(&tip, 0, sizeof(tip));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    tip.is_allow_pickup = TRUE;
    r = itBatWaitProcMap(&titem);
    CHECK(r == FALSE);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.proc_map == itBatFallProcMap);
    /* itBatFallProcMap/ThrownProcMap/DroppedProcMap: honest "nothing
     * collided" composition, reset fresh per call. */
    {
        int i;
        for (i = 0; i < 3; i++)
        {
            memset(&tip.coll_data, 0, sizeof(tip.coll_data));
            tip.coll_data.p_translate = &tdobj.translate.vec.f;
            tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
            tip.coll_data.pos_prev = tdobj.translate.vec.f;
            tip.coll_data.floor_line_id = -2;
            tip.coll_data.ceil_line_id = -2;
            tip.coll_data.lwall_line_id = -2;
            tip.coll_data.rwall_line_id = -2;
            tip.coll_data.ewall_line_id = -2;
            tip.coll_data.ignore_line_id = -2;
            tip.times_landed = 0;
            switch (i)
            {
            case 0: r = itBatFallProcMap(&titem); break;
            case 1: r = itBatThrownProcMap(&titem); break;
            case 2: r = itBatDroppedProcMap(&titem); break;
            }
            CHECK(r == FALSE);
            CHECK(tip.times_landed == 0);
        }
    }
}

/* Not declared in any decomp header -- it/itcommon/ithammer.c's own decomp
 * source never externs these either, all three being used only within
 * that same file there. */
extern ITDesc dITHammerItemDesc;
extern ITStatusDesc dITHammerStatusDescs[];

enum
{
    nITHammerStatusWait,
    nITHammerStatusFall,
    nITHammerStatusHold,
    nITHammerStatusThrown,
    nITHammerStatusDropped
};

/* src/dc/ithammer.c: the roster's sixth entry -- close to
 * Sword/Bat's five-state shape but with no proc_hop/proc_reflector on
 * its Thrown/Dropped rows (both NULL), and two extra calls its
 * SetStatus functions make that neither Sword nor Bat's own do:
 * ftParamTryUpdateItemMusic (a real, already-ported no-op DIVERGES) and,
 * for Dropped, itMainClearColAnim first. itHammerCommonSetColAnim is
 * dead code in the decomp itself (no row names it) but still tested
 * here, proven the same "run it in parallel against the ftparam.c
 * function directly" way test_it_main already proved
 * itMainCheckSetColAnimID's own pass-through shape. */
static void test_it_hammer(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj, tchild;
    GMColAnim ca1, ca2;
    sb32 r;

    CHECK(dITHammerItemDesc.kind == nITKindHammer);
    CHECK(dITHammerItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITHammerItemDesc.o_attributes == (intptr_t)0x374);  /* reloc_data.us.h: llITCommonDataHammerItemAttributes */
    CHECK(dITHammerItemDesc.transform_types.tk1 == nGCMatrixKindTraRotRpyR);
    CHECK(dITHammerItemDesc.transform_types.tk2 == nGCMatrixKindNull);
    CHECK(dITHammerItemDesc.attack_state == nGMAttackStateOff);
    CHECK(dITHammerItemDesc.proc_update == itHammerFallProcUpdate);
    CHECK(dITHammerItemDesc.proc_map == itHammerFallProcMap);
    CHECK(dITHammerItemDesc.proc_hit == NULL);

    CHECK(dITHammerStatusDescs[nITHammerStatusWait].proc_map == itHammerWaitProcMap);
    CHECK(dITHammerStatusDescs[nITHammerStatusFall].proc_update == itHammerFallProcUpdate);
    CHECK(dITHammerStatusDescs[nITHammerStatusFall].proc_map == itHammerFallProcMap);
    CHECK(dITHammerStatusDescs[nITHammerStatusHold].proc_update == NULL);
    CHECK(dITHammerStatusDescs[nITHammerStatusHold].proc_map == NULL);
    CHECK(dITHammerStatusDescs[nITHammerStatusThrown].proc_update == itHammerThrownProcUpdate);
    CHECK(dITHammerStatusDescs[nITHammerStatusThrown].proc_map == itHammerThrownProcMap);
    CHECK(dITHammerStatusDescs[nITHammerStatusThrown].proc_hit == itHammerCommonProcHit);
    CHECK(dITHammerStatusDescs[nITHammerStatusThrown].proc_shield == itHammerCommonProcHit);
    CHECK(dITHammerStatusDescs[nITHammerStatusThrown].proc_hop == NULL);
    CHECK(dITHammerStatusDescs[nITHammerStatusThrown].proc_setoff == itHammerCommonProcHit);
    CHECK(dITHammerStatusDescs[nITHammerStatusThrown].proc_reflector == NULL);
    CHECK(dITHammerStatusDescs[nITHammerStatusDropped].proc_update == itHammerFallProcUpdate);
    CHECK(dITHammerStatusDescs[nITHammerStatusDropped].proc_map == itHammerDroppedProcMap);
    CHECK(dITHammerStatusDescs[nITHammerStatusDropped].proc_hit == itHammerCommonProcHit);
    CHECK(dITHammerStatusDescs[nITHammerStatusDropped].proc_hop == NULL);
    CHECK(dITHammerStatusDescs[nITHammerStatusDropped].proc_reflector == NULL);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tchild, 0, sizeof(tchild));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tdobj.child = &tchild;
    /* itHammerFallProcUpdate: gravity + spin. */
    tip.spin_step = 4.0F;
    r = itHammerFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITHAMMER_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 4.0F, 1e-4F);
    /* itHammerThrownProcUpdate: same body, its own function. */
    memset(&tip, 0, sizeof(tip));
    tip.spin_step = 4.0F;
    r = itHammerThrownProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITHAMMER_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 8.0F, 1e-4F);
    /* itHammerWaitSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 5.0F; tip.physics.vel_air.y = 5.0F; tip.physics.vel_air.z = 5.0F;
    tip.times_landed = 3;
    tip.attack_coll.attack_state = nGMAttackStateNew;
    itHammerWaitSetStatus(&titem);
    CHECK(tip.is_allow_pickup == TRUE);
    CHECK(tip.times_landed == 0);
    CHECK(tip.ga == nMPKineticsGround);
    CHECK(tip.proc_map == itHammerWaitProcMap);
    /* itHammerFallSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.is_allow_pickup = TRUE;
    itHammerFallSetStatus(&titem);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_update == itHammerFallProcUpdate);
    CHECK(tip.proc_map == itHammerFallProcMap);
    /* itHammerHoldSetStatus: rotate.y reset, both procs go NULL. */
    tdobj.rotate.vec.f.y = 1.5F;
    itHammerHoldSetStatus(&titem);
    CHECK_NEAR(tdobj.rotate.vec.f.y, F_CST_DTOR32(0.0F), 1e-6F);
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == NULL);
    /* itHammerCommonProcHit: same rebound math as Sword/Bat's own. */
    tip.attack_coll.attack_state = nGMAttackStateNew;
    tip.physics.vel_air.x = 10.0F;
    tip.physics.vel_air.y = 20.0F;
    r = itHammerCommonProcHit(&titem);
    CHECK(r == FALSE);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK_NEAR(tip.physics.vel_air.x, -0.6F, 1e-4F);
    CHECK_NEAR(tip.physics.vel_air.y, 19.0F, 1e-4F);
    /* itHammerThrownSetStatus/DroppedSetStatus: dead code today, driven
     * against a real child DObj so the unconditional ->child dereference
     * is safe; ftParamTryUpdateItemMusic is a real already-ported no-op,
     * called here for real, not skipped. */
    tchild.rotate.vec.f.y = 0.0F;
    itHammerThrownSetStatus(&titem);
    CHECK(tip.proc_map == itHammerThrownProcMap);
    CHECK_NEAR(tchild.rotate.vec.f.y, F_CST_DTOR32(90.0F), 1e-6F);

    tchild.rotate.vec.f.y = 0.0F;
    itHammerDroppedSetStatus(&titem);
    CHECK(tip.proc_map == itHammerDroppedProcMap);
    CHECK_NEAR(tchild.rotate.vec.f.y, F_CST_DTOR32(90.0F), 1e-6F);
    /* itHammerCommonSetColAnim: dead code in the decomp itself, proven
     * the same parallel-comparison way test_it_main already proved
     * itMainCheckSetColAnimID's own pass-through shape. */
    memset(&ca1, 0, sizeof(ca1));
    memset(&ca2, 0, sizeof(ca2));
    tip.colanim = ca1;
    itHammerCommonSetColAnim(&titem);
    ftParamCheckSetColAnimID(&ca2, nGMColAnimItemHammerEnd, 0);
    CHECK(memcmp(&tip.colanim, &ca2, sizeof(GMColAnim)) == 0);
    /* itHammerWaitProcMap: honest "no floor" reset -- the inverted guard
     * makes the callback (FallSetStatus) actually fire. */
    memset(&tip, 0, sizeof(tip));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    tip.is_allow_pickup = TRUE;
    r = itHammerWaitProcMap(&titem);
    CHECK(r == FALSE);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.proc_map == itHammerFallProcMap);
    /* itHammerFallProcMap/ThrownProcMap/DroppedProcMap: honest "nothing
     * collided" composition, reset fresh per call. */
    {
        int i;
        for (i = 0; i < 3; i++)
        {
            memset(&tip.coll_data, 0, sizeof(tip.coll_data));
            tip.coll_data.p_translate = &tdobj.translate.vec.f;
            tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
            tip.coll_data.pos_prev = tdobj.translate.vec.f;
            tip.coll_data.floor_line_id = -2;
            tip.coll_data.ceil_line_id = -2;
            tip.coll_data.lwall_line_id = -2;
            tip.coll_data.rwall_line_id = -2;
            tip.coll_data.ewall_line_id = -2;
            tip.coll_data.ignore_line_id = -2;
            tip.times_landed = 0;
            switch (i)
            {
            case 0: r = itHammerFallProcMap(&titem); break;
            case 1: r = itHammerThrownProcMap(&titem); break;
            case 2: r = itHammerDroppedProcMap(&titem); break;
            }
            CHECK(r == FALSE);
            CHECK(tip.times_landed == 0);
        }
    }
}

/* Not declared in any decomp header -- it/itcommon/itharisen.c's own decomp
 * source never externs these either, all three being used only within
 * that same file there. */
extern ITDesc dITHarisenItemDesc;
extern ITStatusDesc dITHarisenStatusDescs[];

enum
{
    nITHarisenStatusWait,
    nITHarisenStatusFall,
    nITHarisenStatusHold,
    nITHarisenStatusThrown,
    nITHarisenStatusDropped
};

/* src/dc/itharisen.c: the roster's seventh entry -- close
 * to Sword's own five-state shape (Thrown/Dropped wired to
 * itMainCommonProcHop/ProcReflector, unlike Hammer's). The one new
 * behaviour: itHarisenHoldSetStatus adds a scale-only XObj via
 * gcAddXObjForDObjFixed (sys/objman.c, already in the build) on top of
 * the usual rotate.y reset -- driven here for real (this test runs
 * after load_stage(), so the XObj pool is already up, the same
 * precondition test_it_star's own MakeItem sub-test already relies on).
 * func_ovl3_80175408 is left out (see the file header) so it has no
 * test here. */
static void test_it_harisen(void)
{
    GObj titem, towner;
    ITStruct tip;
    ITAttributes tattr;
    DObj tdobj, tchild;
    FTStruct tfp;
    sb32 r;

    CHECK(dITHarisenItemDesc.kind == nITKindHarisen);
    CHECK(dITHarisenItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITHarisenItemDesc.o_attributes == (intptr_t)0x220);  /* reloc_data.us.h: llITCommonDataHarisenItemAttributes */
    CHECK(dITHarisenItemDesc.transform_types.tk1 == nGCMatrixKindTraRotRpyRSca);
    CHECK(dITHarisenItemDesc.transform_types.tk2 == nGCMatrixKindNull);
    CHECK(dITHarisenItemDesc.attack_state == nGMAttackStateOff);
    CHECK(dITHarisenItemDesc.proc_update == itHarisenFallProcUpdate);
    CHECK(dITHarisenItemDesc.proc_map == itHarisenFallProcMap);
    CHECK(dITHarisenItemDesc.proc_hit == NULL);

    CHECK(dITHarisenStatusDescs[nITHarisenStatusWait].proc_map == itHarisenWaitProcMap);
    CHECK(dITHarisenStatusDescs[nITHarisenStatusFall].proc_update == itHarisenFallProcUpdate);
    CHECK(dITHarisenStatusDescs[nITHarisenStatusFall].proc_map == itHarisenFallProcMap);
    CHECK(dITHarisenStatusDescs[nITHarisenStatusHold].proc_update == NULL);
    CHECK(dITHarisenStatusDescs[nITHarisenStatusHold].proc_map == NULL);
    CHECK(dITHarisenStatusDescs[nITHarisenStatusThrown].proc_update == itHarisenThrownProcUpdate);
    CHECK(dITHarisenStatusDescs[nITHarisenStatusThrown].proc_map == itHarisenThrownProcMap);
    CHECK(dITHarisenStatusDescs[nITHarisenStatusThrown].proc_hit == itHarisenCommonProcHit);
    CHECK(dITHarisenStatusDescs[nITHarisenStatusThrown].proc_shield == itHarisenCommonProcHit);
    CHECK(dITHarisenStatusDescs[nITHarisenStatusThrown].proc_hop == itMainCommonProcHop);
    CHECK(dITHarisenStatusDescs[nITHarisenStatusThrown].proc_setoff == itHarisenCommonProcHit);
    CHECK(dITHarisenStatusDescs[nITHarisenStatusThrown].proc_reflector == itMainCommonProcReflector);
    CHECK(dITHarisenStatusDescs[nITHarisenStatusDropped].proc_update == itHarisenFallProcUpdate);
    CHECK(dITHarisenStatusDescs[nITHarisenStatusDropped].proc_map == itHarisenDroppedProcMap);
    CHECK(dITHarisenStatusDescs[nITHarisenStatusDropped].proc_hit == itHarisenCommonProcHit);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tchild, 0, sizeof(tchild));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tdobj.child = &tchild;
    /* itHarisenCommonSetScale */
    itHarisenCommonSetScale(&titem, 2.5F);
    CHECK_NEAR(tdobj.scale.vec.f.x, 2.5F, 1e-6F);
    CHECK_NEAR(tdobj.scale.vec.f.y, 2.5F, 1e-6F);
    CHECK_NEAR(tdobj.scale.vec.f.z, 2.5F, 1e-6F);
    /* itHarisenFallProcUpdate: gravity + spin. */
    tip.spin_step = 4.0F;
    r = itHarisenFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITHARISEN_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 4.0F, 1e-4F);
    /* itHarisenThrownProcUpdate: same body, its own function. */
    memset(&tip, 0, sizeof(tip));
    tip.spin_step = 4.0F;
    r = itHarisenThrownProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITHARISEN_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 8.0F, 1e-4F);
    /* itHarisenWaitSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 5.0F; tip.physics.vel_air.y = 5.0F; tip.physics.vel_air.z = 5.0F;
    tip.times_landed = 3;
    tip.attack_coll.attack_state = nGMAttackStateNew;
    itHarisenWaitSetStatus(&titem);
    CHECK(tip.is_allow_pickup == TRUE);
    CHECK(tip.times_landed == 0);
    CHECK(tip.ga == nMPKineticsGround);
    CHECK(tip.proc_map == itHarisenWaitProcMap);
    /* itHarisenFallSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.is_allow_pickup = TRUE;
    itHarisenFallSetStatus(&titem);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_update == itHarisenFallProcUpdate);
    CHECK(tip.proc_map == itHarisenFallProcMap);
    /* itHarisenHoldSetStatus: adds a scale-only XObj (the pool is up,
     * this test runs after load_stage()), rotate.y reset, both procs go
     * NULL. */
    tdobj.xobjs_num = 0;
    tdobj.rotate.vec.f.y = 1.5F;
    itHarisenHoldSetStatus(&titem);
    CHECK_NEAR(tdobj.rotate.vec.f.y, 0.0F, 1e-6F);
    CHECK(tdobj.xobjs_num == 1);
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == NULL);
    /* itHarisenCommonProcHit: same rebound math as every prior thrown
     * item's own. */
    tip.attack_coll.attack_state = nGMAttackStateNew;
    tip.physics.vel_air.x = 10.0F;
    tip.physics.vel_air.y = 20.0F;
    r = itHarisenCommonProcHit(&titem);
    CHECK(r == FALSE);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK_NEAR(tip.physics.vel_air.x, -0.6F, 1e-4F);
    CHECK_NEAR(tip.physics.vel_air.y, 19.0F, 1e-4F);
    /* itHarisenThrownSetStatus/DroppedSetStatus: dead code today, driven
     * against a real child DObj so the unconditional ->child dereference
     * is safe. -90 degrees, not Sword/Bat/Hammer's +90. */
    tchild.rotate.vec.f.y = 0.0F;
    itHarisenThrownSetStatus(&titem);
    CHECK(tip.proc_map == itHarisenThrownProcMap);
    CHECK_NEAR(tchild.rotate.vec.f.y, F_CST_DTOR32(-90.0F), 1e-6F);

    tchild.rotate.vec.f.y = 0.0F;
    itHarisenDroppedSetStatus(&titem);
    CHECK(tip.proc_map == itHarisenDroppedProcMap);
    CHECK_NEAR(tchild.rotate.vec.f.y, F_CST_DTOR32(-90.0F), 1e-6F);
    /* itMainCommonProcHop/Reflector, same shape test_it_sword first
     * proved -- ip->attr must point at a real ITAttributes. */
    memset(&tip, 0, sizeof(tip));
    memset(&tattr, 0, sizeof(tattr));
    memset(&towner, 0, sizeof(towner));
    memset(&tfp, 0, sizeof(tfp));
    tip.attr = &tattr;
    towner.user_data.p = &tfp;
    tip.owner_gobj = &towner;
    tip.shield_collide_dir.x = 0.0F;
    tip.shield_collide_dir.y = 1.0F;
    tip.shield_collide_dir.z = 0.0F;
    tip.shield_collide_angle = 0.0F;
    tip.physics.vel_air.x = 3.0F;
    tip.physics.vel_air.y = 0.0F;
    tip.physics.vel_air.z = 0.0F;
    r = itMainCommonProcHop(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 3.0F, 1e-4F);
    CHECK(tip.lr == 1);

    tfp.lr = 1;
    tip.physics.vel_air.x = -3.0F;
    r = itMainCommonProcReflector(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 3.0F, 1e-4F);
    tip.physics.vel_air.x = 3.0F;
    r = itMainCommonProcReflector(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 3.0F, 1e-4F);
    /* itHarisenWaitProcMap: honest "no floor" reset -- the inverted guard
     * makes the callback (FallSetStatus) actually fire. */
    memset(&tip, 0, sizeof(tip));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    tip.is_allow_pickup = TRUE;
    r = itHarisenWaitProcMap(&titem);
    CHECK(r == FALSE);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.proc_map == itHarisenFallProcMap);
    /* itHarisenFallProcMap/ThrownProcMap/DroppedProcMap: honest "nothing
     * collided" composition, reset fresh per call. */
    {
        int i;
        for (i = 0; i < 3; i++)
        {
            memset(&tip.coll_data, 0, sizeof(tip.coll_data));
            tip.coll_data.p_translate = &tdobj.translate.vec.f;
            tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
            tip.coll_data.pos_prev = tdobj.translate.vec.f;
            tip.coll_data.floor_line_id = -2;
            tip.coll_data.ceil_line_id = -2;
            tip.coll_data.lwall_line_id = -2;
            tip.coll_data.rwall_line_id = -2;
            tip.coll_data.ewall_line_id = -2;
            tip.coll_data.ignore_line_id = -2;
            tip.times_landed = 0;
            switch (i)
            {
            case 0: r = itHarisenFallProcMap(&titem); break;
            case 1: r = itHarisenThrownProcMap(&titem); break;
            case 2: r = itHarisenDroppedProcMap(&titem); break;
            }
            CHECK(r == FALSE);
            CHECK(tip.times_landed == 0);
        }
    }
}

/* Not declared in any decomp header -- it/itcommon/itfflower.c's own decomp
 * source never externs these either, all being used only within that same
 * file there. */
extern ITDesc dITFFlowerItemDesc;
extern ITStatusDesc dITFFlowerStatusDescs[];
extern WPDesc dITFFlowerWeaponFlameWeaponDesc;

enum
{
    nITFFlowerStatusWait,
    nITFFlowerStatusFall,
    nITFFlowerStatusHold,
    nITFFlowerStatusThrown,
    nITFFlowerStatusDropped
};

/* src/dc/itfflower.c: the roster's eighth entry and the
 * first with a companion weapon -- the flame `itFFlowerWeaponFlameMakeWeapon`
 * spawns is driven for real here (wpManagerAddModel's host arm always
 * succeeds, the same way every wp/ host test already relies on), and
 * itFFlowerShootFlame's own angle-table read is proven with the same
 * backward-computed gITManagerCommonData override test_it_star's own
 * MakeItem sub-test uses. */
static void test_it_fflower(void)
{
    GObj titem, towner, tweapon;
    ITStruct tip;
    FTStruct tfp, tfp_owner;
    WPStruct twp;
    DObj tdobj, tchild;
    sb32 r;
    void *save_common = gITManagerCommonData;

    CHECK(dITFFlowerItemDesc.kind == nITKindFFlower);
    CHECK(dITFFlowerItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITFFlowerItemDesc.o_attributes == (intptr_t)0x2e4);  /* reloc_data.us.h: llITCommonDataFFlowerItemAttributes */
    CHECK(dITFFlowerItemDesc.transform_types.tk1 == nGCMatrixKindTraRotRpyR);
    CHECK(dITFFlowerItemDesc.attack_state == nGMAttackStateOff);
    CHECK(dITFFlowerItemDesc.proc_update == itFFlowerFallProcUpdate);
    CHECK(dITFFlowerItemDesc.proc_map == itFFlowerFallProcMap);

    CHECK(dITFFlowerStatusDescs[nITFFlowerStatusWait].proc_map == itFFlowerWaitProcMap);
    CHECK(dITFFlowerStatusDescs[nITFFlowerStatusFall].proc_update == itFFlowerFallProcUpdate);
    CHECK(dITFFlowerStatusDescs[nITFFlowerStatusHold].proc_update == NULL);
    CHECK(dITFFlowerStatusDescs[nITFFlowerStatusHold].proc_map == NULL);
    /* note: unlike every prior five-state item, Thrown/Dropped BOTH reuse
     * itFFlowerFallProcUpdate directly for proc_update -- no separate
     * ThrownProcUpdate function exists in this file. */
    CHECK(dITFFlowerStatusDescs[nITFFlowerStatusThrown].proc_update == itFFlowerFallProcUpdate);
    CHECK(dITFFlowerStatusDescs[nITFFlowerStatusThrown].proc_map == itFFlowerThrownProcMap);
    CHECK(dITFFlowerStatusDescs[nITFFlowerStatusThrown].proc_hit == itFFlowerCommonProcHit);
    CHECK(dITFFlowerStatusDescs[nITFFlowerStatusThrown].proc_hop == itMainCommonProcHop);
    CHECK(dITFFlowerStatusDescs[nITFFlowerStatusThrown].proc_reflector == itMainCommonProcReflector);
    CHECK(dITFFlowerStatusDescs[nITFFlowerStatusDropped].proc_update == itFFlowerFallProcUpdate);
    CHECK(dITFFlowerStatusDescs[nITFFlowerStatusDropped].proc_map == itFFlowerDroppedProcMap);

    CHECK(dITFFlowerWeaponFlameWeaponDesc.kind == nWPKindFFlowerFlame);
    CHECK(dITFFlowerWeaponFlameWeaponDesc.p_weapon == (void**)&gITManagerCommonData);
    CHECK(dITFFlowerWeaponFlameWeaponDesc.o_attributes == (intptr_t)0x32c);  /* reloc_data.us.h: llITCommonDataFFlowerFlameWeaponAttributes */
    CHECK(dITFFlowerWeaponFlameWeaponDesc.proc_update == itFFlowerWeaponFlameProcUpdate);
    CHECK(dITFFlowerWeaponFlameWeaponDesc.proc_map == itFFlowerWeaponFlameProcMap);
    CHECK(dITFFlowerWeaponFlameWeaponDesc.proc_hit == itFFlowerWeaponFlameProcHit);
    CHECK(dITFFlowerWeaponFlameWeaponDesc.proc_hop == NULL);
    CHECK(dITFFlowerWeaponFlameWeaponDesc.proc_reflector == itFFlowerWeaponFlameProcReflector);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tchild, 0, sizeof(tchild));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tdobj.child = &tchild;
    /* itFFlowerFallProcUpdate: gravity + spin. */
    tip.spin_step = 4.0F;
    r = itFFlowerFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITFFLOWER_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 4.0F, 1e-4F);
    /* itFFlowerWaitSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.times_landed = 3;
    itFFlowerWaitSetStatus(&titem);
    CHECK(tip.is_allow_pickup == TRUE);
    CHECK(tip.times_landed == 0);
    CHECK(tip.ga == nMPKineticsGround);
    CHECK(tip.proc_map == itFFlowerWaitProcMap);
    /* itFFlowerFallSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.is_allow_pickup = TRUE;
    itFFlowerFallSetStatus(&titem);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_update == itFFlowerFallProcUpdate);
    CHECK(tip.proc_map == itFFlowerFallProcMap);
    /* itFFlowerHoldSetStatus: no rotate reset this time -- just dispatch. */
    itFFlowerHoldSetStatus(&titem);
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == NULL);
    /* itFFlowerCommonProcHit: same rebound math as every prior thrown
     * item's own. */
    tip.attack_coll.attack_state = nGMAttackStateNew;
    tip.physics.vel_air.x = 10.0F;
    tip.physics.vel_air.y = 20.0F;
    r = itFFlowerCommonProcHit(&titem);
    CHECK(r == FALSE);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK_NEAR(tip.physics.vel_air.x, -0.6F, 1e-4F);
    CHECK_NEAR(tip.physics.vel_air.y, 19.0F, 1e-4F);
    /* itFFlowerThrownSetStatus */
    itFFlowerThrownSetStatus(&titem);
    CHECK(tip.proc_map == itFFlowerThrownProcMap);
    /* itFFlowerDroppedSetStatus */
    itFFlowerDroppedSetStatus(&titem);
    CHECK(tip.proc_map == itFFlowerDroppedProcMap);
    /* itFFlowerThrownProcMap/DroppedProcMap: multi == 0 takes the
     * DestroyLanding branch, multi != 0 the honest DestroyDropped
     * composition -- both reset fresh per call. */
    {
        int i;
        for (i = 0; i < 4; i++)
        {
            memset(&tip.coll_data, 0, sizeof(tip.coll_data));
            tip.coll_data.p_translate = &tdobj.translate.vec.f;
            tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
            tip.coll_data.pos_prev = tdobj.translate.vec.f;
            tip.coll_data.floor_line_id = -2;
            tip.coll_data.ceil_line_id = -2;
            tip.coll_data.lwall_line_id = -2;
            tip.coll_data.rwall_line_id = -2;
            tip.coll_data.ewall_line_id = -2;
            tip.coll_data.ignore_line_id = -2;
            tip.times_landed = 0;
            tip.multi = (i < 2) ? 0 : 5;
            switch (i)
            {
            case 0: r = itFFlowerThrownProcMap(&titem); break;
            case 1: r = itFFlowerDroppedProcMap(&titem); break;
            case 2: r = itFFlowerThrownProcMap(&titem); break;
            case 3: r = itFFlowerDroppedProcMap(&titem); break;
            }
            CHECK(r == FALSE);
            CHECK(tip.times_landed == 0);
        }
    }

    /* The flame weapon, driven for real -- wpManagerAddModel's host arm
     * always succeeds (see src/dc/wpmanager.c), so this proves the real
     * field-fill and proc wiring, not just static table shape. */
    wpManagerAllocWeapons();
    {
        Vec3f spawn = { 10.0F, 20.0F, 0.0F };
        Vec3f vel = { 5.0F, -2.0F, 0.0F };
        GObj *weapon_gobj;
        WPAttributes tattr_wp;

        memset(&tfp_owner, 0, sizeof(tfp_owner));
        tfp_owner.lr = 1;
        /* WEAPON_FLAG_COLLPROJECT + WEAPON_FLAG_PARENT_FIGHTER together
         * make wpManagerMakeWeapon's own tail call
         * mpCommonRunWeaponCollisionDefault(weapon_gobj,
         * ftGetStruct(parent_gobj)->coll_data.p_translate, ...) -- a
         * NULL p_translate (the zeroed FTStruct's own default) is
         * dereferenced immediately inside mpCommonCopyCollDataStats
         * (`this_coll_data->pos_prev = *pos;`). Needs a real Vec3f, the
         * same "set p_translate before feeding a coll_data anywhere"
         * rule every itMap-adjacent test in this port already follows. */
        tfp_owner.coll_data.p_translate = &tdobj.translate.vec.f;
        towner.user_data.p = &tfp_owner;

        /* dITFFlowerWeaponFlameWeaponDesc.p_weapon is &gITManagerCommonData,
         * fixed at static-init time -- wpManagerMakeWeapon's own
         * lbRelocGetFileData(WPAttributes*, *wp_desc->p_weapon, ...) reads
         * a real WPAttributes through it (attr->map_coll_width and
         * friends, well past the offset's own four bytes), so this needs
         * the same backward-computed override
         * itFFlowerShootFlame's own block below uses, not a bare
         * gITManagerCommonData left at whatever a prior test's own
         * override last set it to. */
        memset(&tattr_wp, 0, sizeof(tattr_wp));
        gITManagerCommonData = (void*)((uintptr_t)&tattr_wp - (uintptr_t)0x32c);  /* reloc_data.us.h: llITCommonDataFFlowerFlameWeaponAttributes */

        weapon_gobj = itFFlowerWeaponFlameMakeWeapon(&towner, &spawn, &vel);
        CHECK(weapon_gobj != NULL);
        if (weapon_gobj != NULL)
        {
            WPStruct *wp = wpGetStruct(weapon_gobj);

            CHECK_NEAR(wp->physics.vel_air.x, 5.0F * tfp_owner.lr, 1e-4F);
            CHECK_NEAR(wp->physics.vel_air.y, -2.0F, 1e-4F);
            CHECK(wp->lifetime == ITFFLOWER_AMMO_LIFETIME);

            wpMainDestroyWeapon(weapon_gobj);
        }
        gITManagerCommonData = save_common;
    }
    /* itFFlowerWeaponFlameProcUpdate: decrements lifetime, expires at 0. */
    memset(&twp, 0, sizeof(twp));
    tweapon.user_data.p = &twp;
    twp.lifetime = 2;
    r = itFFlowerWeaponFlameProcUpdate(&tweapon);
    CHECK(r == FALSE);
    CHECK(twp.lifetime == 1);
    r = itFFlowerWeaponFlameProcUpdate(&tweapon);
    CHECK(r == TRUE);
    CHECK(twp.lifetime == 0);
    /* itFFlowerWeaponFlameProcMap: honest "nothing collided" composition,
     * same reset-per-call shape every itMap test in this port uses. */
    memset(&twp, 0, sizeof(twp));
    tweapon.obj = &tdobj;
    twp.coll_data.p_translate = &tdobj.translate.vec.f;
    twp.coll_data.p_map_coll = &twp.coll_data.map_coll;
    twp.coll_data.pos_prev = tdobj.translate.vec.f;
    twp.coll_data.floor_line_id = -2;
    twp.coll_data.ceil_line_id = -2;
    twp.coll_data.lwall_line_id = -2;
    twp.coll_data.rwall_line_id = -2;
    twp.coll_data.ewall_line_id = -2;
    twp.coll_data.ignore_line_id = -2;
    r = itFFlowerWeaponFlameProcMap(&tweapon);
    CHECK(r == FALSE);
    /* itFFlowerWeaponFlameProcHit: no crash, always refuses further hits
     * this frame (return FALSE) -- the audio/effect calls have no
     * observable state this test can check. */
    r = itFFlowerWeaponFlameProcHit(&tweapon);
    CHECK(r == FALSE);
    /* itFFlowerWeaponFlameProcReflector: lifetime reset, lr flip through
     * wpMainReflectorSetLR, two particle spawns (no crash is the check). */
    memset(&twp, 0, sizeof(twp));
    memset(&tfp, 0, sizeof(tfp));
    tfp.lr = 1;
    twp.owner_gobj = &towner;
    towner.user_data.p = &tfp;
    twp.lifetime = 0;
    twp.physics.vel_air.x = -3.0F;
    r = itFFlowerWeaponFlameProcReflector(&tweapon);
    CHECK(r == FALSE);
    CHECK(twp.lifetime == ITFFLOWER_AMMO_LIFETIME);

    /* itFFlowerShootFlame: dead code today (see the file header), driven
     * anyway with the same backward-computed gITManagerCommonData
     * override test_it_star's own MakeItem sub-test uses,
     * so the angle-table read resolves to a real local buffer. */
    {
        f32 angles[2] = { 0.0F, F_CST_DTOR32(90.0F) };
        ITStruct tip2;
        GObj tfighter_gobj;

        memset(&tip2, 0, sizeof(tip2));
        memset(&tfighter_gobj, 0, sizeof(tfighter_gobj));
        memset(&tfp, 0, sizeof(tfp));
        tip2.multi = 60;
        tfp.item_gobj = &titem;
        titem.user_data.p = &tip2;
        tfighter_gobj.user_data.p = &tfp;

        gITManagerCommonData = (void*)((uintptr_t)&angles - (uintptr_t)0x360);  /* reloc_data.us.h: llITCommonDataFFlowerFlameAngles */

        {
            Vec3f pos = { 0.0F, 0.0F, 0.0F };

            /* itFFlowerShootFlame's own tail calls
             * itFFlowerWeaponFlameMakeWeapon, whose COLLPROJECT|PARENT_
             * FIGHTER spawn dereferences the owner's own coll_data.
             * p_translate the same way the direct MakeWeapon block above
             * does -- the same rule applies here too. */
            tfp.coll_data.p_translate = &pos;

            itFFlowerShootFlame(&tfighter_gobj, &pos, 0, 3);
            CHECK(tip2.multi == 57);

            /* the flame weapon it just spawned is real and stays linked
             * on nGCCommonLinkIDWeapon -- every later test that assumes
             * an empty weapon link at its own start (test_wp_yoshi_star,
             * test_ft_mario_specialn_accessory, ...) fails otherwise, the
             * same "proper teardown returns the pool struct" rule every
             * other MakeWeapon call in this file already follows. */
            {
                extern GObj *gGCCommonLinks[];
                if (gGCCommonLinks[nGCCommonLinkIDWeapon] != NULL)
                {
                    wpMainDestroyWeapon(gGCCommonLinks[nGCCommonLinkIDWeapon]);
                }
            }
        }

        gITManagerCommonData = save_common;
    }
}

/* Not declared in any decomp header -- it/itcommon/itlgun.c's own decomp
 * source never externs these either, all being used only within that
 * same file there. */
extern ITDesc dITLGunItemDesc;
extern ITStatusDesc dITLGunStatusDescs[];
extern WPDesc dITLGunAmmoWeaponDesc;

enum
{
    nITLGunStatusWait,
    nITLGunStatusFall,
    nITLGunStatusHold,
    nITLGunStatusThrown,
    nITLGunStatusDropped
};

/* src/dc/itlgun.c: the roster's ninth entry and Fire
 * Flower's closest twin -- the ammo `itLGunWeaponAmmoMakeWeapon` spawns
 * is driven for real here the same way, and its shield-bounce ProcHop/
 * ProcReflector (which Fire Flower's own flame weapon never wired) get
 * their own real numbers, cross-checked against test_wp_fox_blaster/
 * test_wp_yoshi_star's own identical (100,0,0)-about-y-by-pi case. */
static void test_it_lgun(void)
{
    GObj titem, towner, tweapon;
    ITStruct tip;
    FTStruct tfp, tfp_owner;
    WPStruct twp;
    DObj tdobj, tchild;
    sb32 r;
    void *save_common = gITManagerCommonData;

    CHECK(dITLGunItemDesc.kind == nITKindLGun);
    CHECK(dITLGunItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITLGunItemDesc.o_attributes == (intptr_t)0x268);  /* reloc_data.us.h: llITCommonDataLGunItemAttributes */
    CHECK(dITLGunItemDesc.transform_types.tk1 == nGCMatrixKindTraRotRpyR);
    CHECK(dITLGunItemDesc.attack_state == nGMAttackStateOff);
    CHECK(dITLGunItemDesc.proc_update == itLGunFallProcUpdate);
    CHECK(dITLGunItemDesc.proc_map == itLGunFallProcMap);

    CHECK(dITLGunStatusDescs[nITLGunStatusWait].proc_map == itLGunWaitProcMap);
    CHECK(dITLGunStatusDescs[nITLGunStatusFall].proc_update == itLGunFallProcUpdate);
    CHECK(dITLGunStatusDescs[nITLGunStatusHold].proc_update == NULL);
    CHECK(dITLGunStatusDescs[nITLGunStatusHold].proc_map == NULL);
    CHECK(dITLGunStatusDescs[nITLGunStatusThrown].proc_update == itLGunFallProcUpdate);
    CHECK(dITLGunStatusDescs[nITLGunStatusThrown].proc_map == itLGunThrownProcMap);
    CHECK(dITLGunStatusDescs[nITLGunStatusThrown].proc_hit == itLGunCommonProcHit);
    CHECK(dITLGunStatusDescs[nITLGunStatusThrown].proc_hop == itMainCommonProcHop);
    CHECK(dITLGunStatusDescs[nITLGunStatusThrown].proc_reflector == itMainCommonProcReflector);
    CHECK(dITLGunStatusDescs[nITLGunStatusDropped].proc_update == itLGunFallProcUpdate);
    CHECK(dITLGunStatusDescs[nITLGunStatusDropped].proc_map == itLGunDroppedProcMap);

    CHECK(dITLGunAmmoWeaponDesc.kind == nWPKindLGunAmmo);
    CHECK(dITLGunAmmoWeaponDesc.p_weapon == (void**)&gITManagerCommonData);
    CHECK(dITLGunAmmoWeaponDesc.o_attributes == (intptr_t)0x2b0);  /* reloc_data.us.h: llITCommonDataLGunAmmoWeaponAttributes */
    CHECK(dITLGunAmmoWeaponDesc.proc_update == itLGunWeaponAmmoProcUpdate);
    CHECK(dITLGunAmmoWeaponDesc.proc_map == itLGunWeaponAmmoProcMap);
    CHECK(dITLGunAmmoWeaponDesc.proc_hit == itLGunWeaponAmmoProcHit);
    CHECK(dITLGunAmmoWeaponDesc.proc_shield == itLGunWeaponAmmoProcHit);
    /* unlike Fire Flower's own flame WPDesc (proc_hop NULL), the ammo
     * wires a real shield-bounce ProcHop. */
    CHECK(dITLGunAmmoWeaponDesc.proc_hop == itLGunWeaponAmmoProcHop);
    CHECK(dITLGunAmmoWeaponDesc.proc_setoff == itLGunWeaponAmmoProcHit);
    CHECK(dITLGunAmmoWeaponDesc.proc_reflector == itLGunWeaponAmmoProcReflector);
    CHECK(dITLGunAmmoWeaponDesc.proc_absorb == itLGunWeaponAmmoProcHit);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tchild, 0, sizeof(tchild));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tdobj.child = &tchild;
    /* itLGunFallProcUpdate: gravity + spin. */
    tip.spin_step = 4.0F;
    r = itLGunFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITLGUN_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 4.0F, 1e-4F);
    /* itLGunWaitSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.times_landed = 3;
    itLGunWaitSetStatus(&titem);
    CHECK(tip.is_allow_pickup == TRUE);
    CHECK(tip.times_landed == 0);
    CHECK(tip.ga == nMPKineticsGround);
    CHECK(tip.proc_map == itLGunWaitProcMap);
    /* itLGunFallSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.is_allow_pickup = TRUE;
    itLGunFallSetStatus(&titem);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_update == itLGunFallProcUpdate);
    CHECK(tip.proc_map == itLGunFallProcMap);
    /* itLGunHoldSetStatus: no rotate reset this time -- just dispatch. */
    itLGunHoldSetStatus(&titem);
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == NULL);
    /* itLGunCommonProcHit: same rebound math as every prior thrown
     * item's own. */
    tip.attack_coll.attack_state = nGMAttackStateNew;
    tip.physics.vel_air.x = 10.0F;
    tip.physics.vel_air.y = 20.0F;
    r = itLGunCommonProcHit(&titem);
    CHECK(r == FALSE);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK_NEAR(tip.physics.vel_air.x, -0.6F, 1e-4F);
    CHECK_NEAR(tip.physics.vel_air.y, 19.0F, 1e-4F);
    /* itLGunThrownSetStatus/DroppedSetStatus both read
     * ftGetStruct(ip->owner_gobj)->lr BEFORE the dead ->child dereference
     * (unlike Fire Flower's own pair, which never touches lr at all) --
     * ip->owner_gobj needs a real fighter GObj first. */
    memset(&tfp_owner, 0, sizeof(tfp_owner));
    tfp_owner.lr = -1;
    towner.user_data.p = &tfp_owner;
    tip.owner_gobj = &towner;
    /* itLGunThrownSetStatus */
    itLGunThrownSetStatus(&titem);
    CHECK(tip.proc_map == itLGunThrownProcMap);
    CHECK_NEAR(tchild.rotate.vec.f.y, F_CST_DTOR32(-90.0F), 1e-4F);
    /* itLGunDroppedSetStatus */
    itLGunDroppedSetStatus(&titem);
    CHECK(tip.proc_map == itLGunDroppedProcMap);
    CHECK_NEAR(tchild.rotate.vec.f.y, F_CST_DTOR32(-90.0F), 1e-4F);
    /* itLGunThrownProcMap/DroppedProcMap: multi == 0 takes the
     * DestroyLanding branch, multi != 0 the honest DestroyDropped
     * composition -- both reset fresh per call. */
    {
        int i;
        for (i = 0; i < 4; i++)
        {
            memset(&tip.coll_data, 0, sizeof(tip.coll_data));
            tip.coll_data.p_translate = &tdobj.translate.vec.f;
            tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
            tip.coll_data.pos_prev = tdobj.translate.vec.f;
            tip.coll_data.floor_line_id = -2;
            tip.coll_data.ceil_line_id = -2;
            tip.coll_data.lwall_line_id = -2;
            tip.coll_data.rwall_line_id = -2;
            tip.coll_data.ewall_line_id = -2;
            tip.coll_data.ignore_line_id = -2;
            tip.times_landed = 0;
            tip.multi = (i < 2) ? 0 : 5;
            switch (i)
            {
            case 0: r = itLGunThrownProcMap(&titem); break;
            case 1: r = itLGunDroppedProcMap(&titem); break;
            case 2: r = itLGunThrownProcMap(&titem); break;
            case 3: r = itLGunDroppedProcMap(&titem); break;
            }
            CHECK(r == FALSE);
            CHECK(tip.times_landed == 0);
        }
    }

    /* The ammo weapon, driven for real -- wpManagerAddModel's host arm
     * always succeeds, so this proves the real field-fill and proc
     * wiring, not just static table shape. */
    wpManagerAllocWeapons();
    {
        Vec3f spawn = { 10.0F, 20.0F, 0.0F };
        GObj *weapon_gobj;
        WPAttributes tattr_wp;

        memset(&tfp_owner, 0, sizeof(tfp_owner));
        tfp_owner.lr = -1;
        /* WEAPON_FLAG_COLLPROJECT + WEAPON_FLAG_PARENT_FIGHTER together
         * make wpManagerMakeWeapon's own tail dereference the owner's
         * own coll_data.p_translate -- the same rule the
         * itfflower.c test follows. */
        tfp_owner.coll_data.p_translate = &tdobj.translate.vec.f;
        towner.user_data.p = &tfp_owner;

        memset(&tattr_wp, 0, sizeof(tattr_wp));
        gITManagerCommonData = (void*)((uintptr_t)&tattr_wp - (uintptr_t)0x2b0);  /* reloc_data.us.h: llITCommonDataLGunAmmoWeaponAttributes */

        weapon_gobj = itLGunWeaponAmmoMakeWeapon(&towner, &spawn);
        CHECK(weapon_gobj != NULL);
        if (weapon_gobj != NULL)
        {
            WPStruct *wp = wpGetStruct(weapon_gobj);

            CHECK_NEAR(wp->physics.vel_air.x, (f32)tfp_owner.lr * ITLGUN_AMMO_VEL_X, 1e-3F);
            CHECK_NEAR(DObjGetStruct(weapon_gobj)->rotate.vec.f.z,
                       syUtilsArcTan2(wp->physics.vel_air.y, wp->physics.vel_air.x), 1e-4F);

            wpMainDestroyWeapon(weapon_gobj);
        }
        gITManagerCommonData = save_common;
    }
    /* itLGunWeaponAmmoProcUpdate: scale ramps up by ITLGUN_AMMO_STEP_
     * SCALE_X each tic, clamped at ITLGUN_AMMO_CLAMP_SCALE_X. */
    memset(&twp, 0, sizeof(twp));
    tweapon.user_data.p = &twp;
    tweapon.obj = &tdobj;
    memset(&tdobj, 0, sizeof(tdobj));
    tdobj.scale.vec.f.x = 0.0F;
    r = itLGunWeaponAmmoProcUpdate(&tweapon);
    CHECK(r == FALSE);
    CHECK_NEAR(tdobj.scale.vec.f.x, ITLGUN_AMMO_STEP_SCALE_X, 1e-4F);
    tdobj.scale.vec.f.x = ITLGUN_AMMO_CLAMP_SCALE_X - 1.0F;
    r = itLGunWeaponAmmoProcUpdate(&tweapon);
    CHECK(r == FALSE);
    CHECK_NEAR(tdobj.scale.vec.f.x, ITLGUN_AMMO_CLAMP_SCALE_X, 1e-4F);
    /* itLGunWeaponAmmoProcMap: honest "nothing collided" composition,
     * same reset-per-call shape every itMap test in this port uses. */
    memset(&twp, 0, sizeof(twp));
    memset(&tdobj, 0, sizeof(tdobj));
    tweapon.obj = &tdobj;
    twp.coll_data.p_translate = &tdobj.translate.vec.f;
    twp.coll_data.p_map_coll = &twp.coll_data.map_coll;
    twp.coll_data.pos_prev = tdobj.translate.vec.f;
    twp.coll_data.floor_line_id = -2;
    twp.coll_data.ceil_line_id = -2;
    twp.coll_data.lwall_line_id = -2;
    twp.coll_data.rwall_line_id = -2;
    twp.coll_data.ewall_line_id = -2;
    twp.coll_data.ignore_line_id = -2;
    r = itLGunWeaponAmmoProcMap(&tweapon);
    CHECK(r == FALSE);
    /* itLGunWeaponAmmoProcHit: no crash, always signals a hit (TRUE) --
     * the effect call has no observable state this test can check. */
    r = itLGunWeaponAmmoProcHit(&tweapon);
    CHECK(r == TRUE);
    /* itLGunWeaponAmmoProcHop: reflect vel_air about shield_collide_dir
     * by shield_collide_angle*2, re-face from the new x, reset scale.
     * (100,0,0) about y by pi -> (-100,0,0), the same case test_wp_
     * fox_blaster/test_wp_yoshi_star's own ProcHop tests use. */
    memset(&twp, 0, sizeof(twp));
    twp.physics.vel_air.x = 100.0F;
    twp.physics.vel_air.y = 0.0F;
    twp.physics.vel_air.z = 0.0F;
    twp.shield_collide_dir.x = 0.0F;
    twp.shield_collide_dir.y = 1.0F;
    twp.shield_collide_dir.z = 0.0F;
    twp.shield_collide_angle = 3.14159265F / 2.0F;
    memset(&tdobj, 0, sizeof(tdobj));
    tdobj.scale.vec.f.x = 0.4F;
    tweapon.obj = &tdobj;
    r = itLGunWeaponAmmoProcHop(&tweapon);
    CHECK(r == FALSE);
    CHECK_NEAR(twp.physics.vel_air.x, -100.0F, 1e-2F);
    CHECK_NEAR(twp.physics.vel_air.y, 0.0F, 1e-2F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, syUtilsArcTan2(twp.physics.vel_air.y, twp.physics.vel_air.x), 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.x, 1.0F, 1e-5F);
    /* itLGunWeaponAmmoProcReflector: wpMainReflectorSetLR flips vel_air.x
     * only when it opposes the owner's own facing (fp.lr) -- no lr flip
     * of its own, unlike a fighter-special weapon's own reflector; scale
     * and rotate.z still reset the same way ProcHop's own tail does. */
    memset(&twp, 0, sizeof(twp));
    memset(&tfp, 0, sizeof(tfp));
    tfp.lr = 1;
    twp.owner_gobj = &towner;
    towner.user_data.p = &tfp;
    twp.physics.vel_air.x = -100.0F;
    twp.physics.vel_air.y = 0.0F;
    memset(&tdobj, 0, sizeof(tdobj));
    tdobj.scale.vec.f.x = 0.4F;
    tweapon.obj = &tdobj;
    r = itLGunWeaponAmmoProcReflector(&tweapon);
    CHECK(r == FALSE);
    CHECK_NEAR(twp.physics.vel_air.x, 100.0F, 1e-2F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, syUtilsArcTan2(twp.physics.vel_air.y, twp.physics.vel_air.x), 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.x, 1.0F, 1e-5F);

    /* itLGunMakeAmmo: dead code today (see the file header), driven
     * anyway -- decrements the caller's own ip->multi and spawns a real
     * ammo weapon the same way the direct MakeWeapon block above does. */
    {
        WPAttributes tattr_wp;
        ITStruct tip2;
        GObj tfighter_gobj;
        Vec3f pos = { 0.0F, 0.0F, 0.0F };

        memset(&tip2, 0, sizeof(tip2));
        memset(&tfighter_gobj, 0, sizeof(tfighter_gobj));
        memset(&tfp, 0, sizeof(tfp));
        tip2.multi = 5;
        tfp.item_gobj = &titem;
        titem.user_data.p = &tip2;
        tfp.coll_data.p_translate = &pos;
        tfighter_gobj.user_data.p = &tfp;

        memset(&tattr_wp, 0, sizeof(tattr_wp));
        gITManagerCommonData = (void*)((uintptr_t)&tattr_wp - (uintptr_t)0x2b0);  /* reloc_data.us.h: llITCommonDataLGunAmmoWeaponAttributes */

        itLGunMakeAmmo(&tfighter_gobj, &pos);
        CHECK(tip2.multi == 4);

        /* the ammo it just spawned is real and stays linked on
         * nGCCommonLinkIDWeapon -- the same leaked-weapon rule
         * for itFFlowerShootFlame's own dead-code block. */
        {
            extern GObj *gGCCommonLinks[];
            if (gGCCommonLinks[nGCCommonLinkIDWeapon] != NULL)
            {
                wpMainDestroyWeapon(gGCCommonLinks[nGCCommonLinkIDWeapon]);
            }
        }

        gITManagerCommonData = save_common;
    }
}

/* Not declared in any decomp header -- it/itcommon/itstarrod.c's own
 * decomp source never externs these either, all being used only within
 * that same file there. */
extern ITDesc dITStarRodItemDesc;
extern ITStatusDesc dITStarRodStatusDescs[];
extern WPDesc dITStarRodWeaponStarWeaponDesc;

enum
{
    nITStarRodStatusWait,
    nITStarRodStatusFall,
    nITStarRodStatusHold,
    nITStarRodStatusThrown,
    nITStarRodStatusDropped
};

/* src/dc/itstarrod.c: the roster's tenth entry and a third
 * companion-weapon item in the Fire Flower/Ray Gun family -- driven for
 * real through both the tilt and smash MakeWeapon paths (the reloc-
 * offset swap between llITCommonDataStarRodWeaponAttributes and
 * llITCommonDataStarRodSmashWeaponAttributes is this file's own new
 * wrinkle, proven by checking BOTH constants land, not just one). */
static void test_it_starrod(void)
{
    GObj titem, towner, tweapon;
    ITStruct tip;
    FTStruct tfp, tfp_owner;
    WPStruct twp;
    DObj tdobj, tchild;
    sb32 r;
    void *save_common = gITManagerCommonData;

    CHECK(dITStarRodItemDesc.kind == nITKindStarRod);
    CHECK(dITStarRodItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITStarRodItemDesc.o_attributes == (intptr_t)0x48c);  /* reloc_data.us.h: llITCommonDataStarRodItemAttributes */
    CHECK(dITStarRodItemDesc.transform_types.tk1 == nGCMatrixKindTraRotRpyR);
    CHECK(dITStarRodItemDesc.attack_state == nGMAttackStateOff);
    CHECK(dITStarRodItemDesc.proc_update == itStarRodFallProcUpdate);
    CHECK(dITStarRodItemDesc.proc_map == itStarRodFallProcMap);

    CHECK(dITStarRodStatusDescs[nITStarRodStatusWait].proc_map == itStarRodWaitProcMap);
    CHECK(dITStarRodStatusDescs[nITStarRodStatusFall].proc_update == itStarRodFallProcUpdate);
    CHECK(dITStarRodStatusDescs[nITStarRodStatusHold].proc_update == NULL);
    CHECK(dITStarRodStatusDescs[nITStarRodStatusHold].proc_map == NULL);
    CHECK(dITStarRodStatusDescs[nITStarRodStatusThrown].proc_update == itStarRodThrownProcUpdate);
    CHECK(dITStarRodStatusDescs[nITStarRodStatusThrown].proc_map == itStarRodThrownProcMap);
    CHECK(dITStarRodStatusDescs[nITStarRodStatusThrown].proc_hit == itStarRodThrownProcHit);
    CHECK(dITStarRodStatusDescs[nITStarRodStatusThrown].proc_hop == itMainCommonProcHop);
    CHECK(dITStarRodStatusDescs[nITStarRodStatusThrown].proc_reflector == itMainCommonProcReflector);
    CHECK(dITStarRodStatusDescs[nITStarRodStatusDropped].proc_update == itStarRodFallProcUpdate);
    CHECK(dITStarRodStatusDescs[nITStarRodStatusDropped].proc_map == itStarRodDroppedProcMap);

    CHECK(dITStarRodWeaponStarWeaponDesc.kind == nWPKindStarRodStar);
    CHECK(dITStarRodWeaponStarWeaponDesc.p_weapon == (void**)&gITManagerCommonData);
    CHECK(dITStarRodWeaponStarWeaponDesc.proc_update == itStarRodWeaponStarProcUpdate);
    CHECK(dITStarRodWeaponStarWeaponDesc.proc_map == itStarRodWeaponStarProcMap);
    CHECK(dITStarRodWeaponStarWeaponDesc.proc_hit == itStarRodWeaponStarProcHit);
    CHECK(dITStarRodWeaponStarWeaponDesc.proc_shield == itStarRodWeaponStarProcHit);
    CHECK(dITStarRodWeaponStarWeaponDesc.proc_hop == itStarRodWeaponStarProcHop);
    CHECK(dITStarRodWeaponStarWeaponDesc.proc_setoff == itStarRodWeaponStarProcHit);
    CHECK(dITStarRodWeaponStarWeaponDesc.proc_reflector == itStarRodWeaponStarProcReflector);
    CHECK(dITStarRodWeaponStarWeaponDesc.proc_absorb == itStarRodWeaponStarProcHit);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tchild, 0, sizeof(tchild));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tdobj.child = &tchild;
    /* itStarRodFallProcUpdate: gravity + spin. */
    tip.spin_step = 4.0F;
    r = itStarRodFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITSTARROD_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 4.0F, 1e-4F);
    /* itStarRodWaitSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.times_landed = 3;
    itStarRodWaitSetStatus(&titem);
    CHECK(tip.is_allow_pickup == TRUE);
    CHECK(tip.times_landed == 0);
    CHECK(tip.ga == nMPKineticsGround);
    CHECK(tip.proc_map == itStarRodWaitProcMap);
    /* itStarRodFallSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.is_allow_pickup = TRUE;
    itStarRodFallSetStatus(&titem);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_update == itStarRodFallProcUpdate);
    CHECK(tip.proc_map == itStarRodFallProcMap);
    /* itStarRodHoldSetStatus: rotate reset to 0, then dispatch. */
    tdobj.rotate.vec.f.y = 123.0F;
    itStarRodHoldSetStatus(&titem);
    CHECK_EQF(tdobj.rotate.vec.f.y, F_CST_DTOR32(0.0F));
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == NULL);
    /* itStarRodThrownProcHit: same rebound math as every prior thrown
     * item's own. */
    tip.attack_coll.attack_state = nGMAttackStateNew;
    tip.physics.vel_air.x = 10.0F;
    tip.physics.vel_air.y = 20.0F;
    r = itStarRodThrownProcHit(&titem);
    CHECK(r == FALSE);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK_NEAR(tip.physics.vel_air.x, -0.6F, 1e-4F);
    CHECK_NEAR(tip.physics.vel_air.y, 19.0F, 1e-4F);
    /* itStarRodThrownSetStatus/DroppedSetStatus: unlike Ray Gun's own
     * pair, neither reads the owner's lr -- always +90 degrees
     * on the dead ->child dereference. */
    itStarRodThrownSetStatus(&titem);
    CHECK(tip.proc_map == itStarRodThrownProcMap);
    CHECK_NEAR(tchild.rotate.vec.f.y, F_CST_DTOR32(90.0F), 1e-4F);
    itStarRodDroppedSetStatus(&titem);
    CHECK(tip.proc_map == itStarRodDroppedProcMap);
    CHECK_NEAR(tchild.rotate.vec.f.y, F_CST_DTOR32(90.0F), 1e-4F);
    /* itStarRodThrownProcMap/DroppedProcMap: multi == 0 takes the
     * DestroyLanding branch, multi != 0 the honest DestroyDropped
     * composition -- both reset fresh per call, same shape Ray Gun's own
     * ThrownProcMap/DroppedProcMap tests use. */
    {
        int i;
        for (i = 0; i < 4; i++)
        {
            memset(&tip.coll_data, 0, sizeof(tip.coll_data));
            tip.coll_data.p_translate = &tdobj.translate.vec.f;
            tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
            tip.coll_data.pos_prev = tdobj.translate.vec.f;
            tip.coll_data.floor_line_id = -2;
            tip.coll_data.ceil_line_id = -2;
            tip.coll_data.lwall_line_id = -2;
            tip.coll_data.rwall_line_id = -2;
            tip.coll_data.ewall_line_id = -2;
            tip.coll_data.ignore_line_id = -2;
            tip.times_landed = 0;
            tip.multi = (i < 2) ? 0 : 5;
            switch (i)
            {
            case 0: r = itStarRodThrownProcMap(&titem); break;
            case 1: r = itStarRodDroppedProcMap(&titem); break;
            case 2: r = itStarRodThrownProcMap(&titem); break;
            case 3: r = itStarRodDroppedProcMap(&titem); break;
            }
            CHECK(r == FALSE);
            CHECK(tip.times_landed == 0);
        }
    }

    /* The ammo star, driven for real through BOTH is_smash paths --
     * wpManagerAddModel's host arm always succeeds, so this proves the
     * real field-fill and proc wiring, and that the reloc-offset swap
     * (o_attributes -> the smash stand-in) actually lands. */
    wpManagerAllocWeapons();
    {
        Vec3f spawn = { 10.0F, 20.0F, 0.0F };
        GObj *weapon_gobj;
        WPAttributes tattr_wp, tattr_wp_smash;

        memset(&tfp_owner, 0, sizeof(tfp_owner));
        tfp_owner.lr = -1;
        /* WEAPON_FLAG_COLLPROJECT + WEAPON_FLAG_PARENT_FIGHTER together
         * make wpManagerMakeWeapon's own tail dereference the owner's
         * own coll_data.p_translate -- the same rule the
         * Fire Flower and Ray Gun companion-weapon tests follow. */
        tfp_owner.coll_data.p_translate = &tdobj.translate.vec.f;
        towner.user_data.p = &tfp_owner;

        memset(&tattr_wp, 0, sizeof(tattr_wp));
        gITManagerCommonData = (void*)((uintptr_t)&tattr_wp - (uintptr_t)0x4d4);  /* reloc_data.us.h: llITCommonDataStarRodWeaponAttributes */

        weapon_gobj = itStarRodWeaponStarMakeWeapon(&towner, &spawn, FALSE);
        CHECK(weapon_gobj != NULL);
        CHECK(dITStarRodWeaponStarWeaponDesc.o_attributes == (intptr_t)0x4d4);  /* reloc_data.us.h: llITCommonDataStarRodWeaponAttributes */
        if (weapon_gobj != NULL)
        {
            WPStruct *wp = wpGetStruct(weapon_gobj);

            CHECK_NEAR(wp->physics.vel_air.x, ITSTARROD_AMMO_TILTVEL_X * (f32)wp->lr, 1e-3F);
            CHECK_NEAR(wp->weapon_vars.star.lifetime, ITSTARROD_AMMO_TILT_LIFETIME, 1e-3F);

            wpMainDestroyWeapon(weapon_gobj);
        }

        memset(&tattr_wp_smash, 0, sizeof(tattr_wp_smash));
        gITManagerCommonData = (void*)((uintptr_t)&tattr_wp_smash - (uintptr_t)0x508);  /* reloc_data.us.h: llITCommonDataStarRodSmashWeaponAttributes */

        weapon_gobj = itStarRodWeaponStarMakeWeapon(&towner, &spawn, TRUE);
        CHECK(weapon_gobj != NULL);
        CHECK(dITStarRodWeaponStarWeaponDesc.o_attributes == (intptr_t)0x508);  /* reloc_data.us.h: llITCommonDataStarRodSmashWeaponAttributes */
        if (weapon_gobj != NULL)
        {
            WPStruct *wp = wpGetStruct(weapon_gobj);

            CHECK_NEAR(wp->physics.vel_air.x, ITSTARROD_AMMO_SMASH_VEL_X * (f32)wp->lr, 1e-3F);
            CHECK_NEAR(wp->weapon_vars.star.lifetime, ITSTARROD_AMMO_SMASH_LIFETIME, 1e-3F);

            wpMainDestroyWeapon(weapon_gobj);
        }
        gITManagerCommonData = save_common;
    }
    /* itStarRodWeaponStarProcUpdate: lifetime == 0 hides the DObj and
     * spawns a sparkle effect, signalling destroy (TRUE); lifetime > 0
     * decrements, spins rotate.z by -0.2*lr, and (lifetime odd) spawns a
     * spark at a randomized y -- checked only for "no crash, correct
     * decrement/spin" since the random y has no fixed expected value. */
    memset(&twp, 0, sizeof(twp));
    memset(&tdobj, 0, sizeof(tdobj));
    tweapon.user_data.p = &twp;
    tweapon.obj = &tdobj;
    twp.weapon_vars.star.lifetime = 0;
    r = itStarRodWeaponStarProcUpdate(&tweapon);
    CHECK(r == TRUE);
    CHECK(tdobj.flags == DOBJ_FLAG_HIDDEN);

    memset(&twp, 0, sizeof(twp));
    memset(&tdobj, 0, sizeof(tdobj));
    twp.weapon_vars.star.lifetime = 4;
    twp.lr = 1;
    tdobj.rotate.vec.f.z = 0.0F;
    r = itStarRodWeaponStarProcUpdate(&tweapon);
    CHECK(r == FALSE);
    CHECK(twp.weapon_vars.star.lifetime == 3);
    CHECK_NEAR(tdobj.rotate.vec.f.z, -0.2F, 1e-4F);
    /* itStarRodWeaponStarProcMap: honest "nothing collided" composition,
     * same reset-per-call shape every itMap test in this port uses. */
    memset(&twp, 0, sizeof(twp));
    memset(&tdobj, 0, sizeof(tdobj));
    tweapon.obj = &tdobj;
    twp.coll_data.p_translate = &tdobj.translate.vec.f;
    twp.coll_data.p_map_coll = &twp.coll_data.map_coll;
    twp.coll_data.pos_prev = tdobj.translate.vec.f;
    twp.coll_data.floor_line_id = -2;
    twp.coll_data.ceil_line_id = -2;
    twp.coll_data.lwall_line_id = -2;
    twp.coll_data.rwall_line_id = -2;
    twp.coll_data.ewall_line_id = -2;
    twp.coll_data.ignore_line_id = -2;
    r = itStarRodWeaponStarProcMap(&tweapon);
    CHECK(r == FALSE);
    /* itStarRodWeaponStarProcHit: no crash, always signals a hit (TRUE). */
    r = itStarRodWeaponStarProcHit(&tweapon);
    CHECK(r == TRUE);
    /* itStarRodWeaponStarProcHop: reflect vel_air about shield_collide_dir
     * by shield_collide_angle*2, re-face from the new x, reset scale,
     * then set lr from the new vel_air.x's sign -- the extra lr-set step
     * Ray Gun's own ammo ProcHop does NOT have. (100,0,0) about y by pi
     * -> (-100,0,0), the same case test_wp_fox_blaster/test_wp_yoshi_
     * star/test_it_lgun's own ProcHop tests use. */
    memset(&twp, 0, sizeof(twp));
    twp.physics.vel_air.x = 100.0F;
    twp.physics.vel_air.y = 0.0F;
    twp.physics.vel_air.z = 0.0F;
    twp.shield_collide_dir.x = 0.0F;
    twp.shield_collide_dir.y = 1.0F;
    twp.shield_collide_dir.z = 0.0F;
    twp.shield_collide_angle = 3.14159265F / 2.0F;
    memset(&tdobj, 0, sizeof(tdobj));
    tdobj.scale.vec.f.x = 0.4F;
    tweapon.obj = &tdobj;
    r = itStarRodWeaponStarProcHop(&tweapon);
    CHECK(r == FALSE);
    CHECK_NEAR(twp.physics.vel_air.x, -100.0F, 1e-2F);
    CHECK_NEAR(twp.physics.vel_air.y, 0.0F, 1e-2F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, syUtilsArcTan2(twp.physics.vel_air.y, twp.physics.vel_air.x), 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.x, 1.0F, 1e-5F);
    CHECK(twp.lr == -1);
    /* itStarRodWeaponStarProcReflector: wpMainReflectorSetLR flips
     * vel_air.x only when it opposes the owner's own facing (fp.lr), then
     * this function ALSO flips wp->lr itself -- unlike Ray Gun's own
     * reflector, which leaves lr alone. */
    memset(&twp, 0, sizeof(twp));
    memset(&tfp, 0, sizeof(tfp));
    tfp.lr = 1;
    twp.owner_gobj = &towner;
    towner.user_data.p = &tfp;
    twp.physics.vel_air.x = -100.0F;
    twp.physics.vel_air.y = 0.0F;
    twp.lr = -1;
    memset(&tdobj, 0, sizeof(tdobj));
    tdobj.scale.vec.f.x = 0.4F;
    tweapon.obj = &tdobj;
    r = itStarRodWeaponStarProcReflector(&tweapon);
    CHECK(r == FALSE);
    CHECK_NEAR(twp.physics.vel_air.x, 100.0F, 1e-2F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, syUtilsArcTan2(twp.physics.vel_air.y, twp.physics.vel_air.x), 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.x, 1.0F, 1e-5F);
    CHECK(twp.lr == 1);

    /* itStarRodMakeStar: dead code today (see the file header), driven
     * anyway -- decrements the caller's own ip->multi (read through
     * ftGetStruct(fighter_gobj)->item_gobj, NOT a parameter) and spawns a
     * real ammo star the same way the direct MakeWeapon block above
     * does. */
    {
        WPAttributes tattr_wp;
        ITStruct tip2;
        GObj tfighter_gobj;
        Vec3f pos = { 0.0F, 0.0F, 0.0F };

        memset(&tip2, 0, sizeof(tip2));
        memset(&tfighter_gobj, 0, sizeof(tfighter_gobj));
        memset(&tfp, 0, sizeof(tfp));
        tip2.multi = 5;
        tfp.item_gobj = &titem;
        titem.user_data.p = &tip2;
        tfp.coll_data.p_translate = &pos;
        tfighter_gobj.user_data.p = &tfp;

        memset(&tattr_wp, 0, sizeof(tattr_wp));
        gITManagerCommonData = (void*)((uintptr_t)&tattr_wp - (uintptr_t)0x4d4);  /* reloc_data.us.h: llITCommonDataStarRodWeaponAttributes */

        itStarRodMakeStar(&tfighter_gobj, &pos, FALSE);
        CHECK(tip2.multi == 4);

        /* the ammo it just spawned is real and stays linked on
         * nGCCommonLinkIDWeapon -- the same leaked-weapon rule steps
         * 14-15 found for their own dead-code MakeAmmo/ShootFlame
         * blocks. */
        {
            extern GObj *gGCCommonLinks[];
            if (gGCCommonLinks[nGCCommonLinkIDWeapon] != NULL)
            {
                wpMainDestroyWeapon(gGCCommonLinks[nGCCommonLinkIDWeapon]);
            }
        }

        gITManagerCommonData = save_common;
    }
}

extern ITDesc dITGShellItemDesc;
extern ITStatusDesc dITGShellStatusDescs[];

enum
{
    nITGShellStatusWait, nITGShellStatusFall, nITGShellStatusHold,
    nITGShellStatusThrown, nITGShellStatusDropped, nITGShellStatusSpin,
    nITGShellStatusSpinAir
};

static void test_it_gshell(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    MObj tmobj;
    ITAttributes tattr;
    sb32 r;

    CHECK(dITGShellItemDesc.kind == nITKindGShell);
    CHECK(dITGShellItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITGShellItemDesc.o_attributes == (intptr_t)0x53c);  /* reloc_data.us.h: llITCommonDataGShellItemAttributes */
    CHECK(dITGShellItemDesc.proc_update == itGShellFallProcUpdate);
    CHECK(dITGShellItemDesc.proc_map == itGShellFallProcMap);

    CHECK(dITGShellStatusDescs[nITGShellStatusWait].proc_map == itGShellWaitProcMap);
    CHECK(dITGShellStatusDescs[nITGShellStatusWait].proc_damage == itGShellCommonProcDamage);
    CHECK(dITGShellStatusDescs[nITGShellStatusHold].proc_update == NULL);
    CHECK(dITGShellStatusDescs[nITGShellStatusHold].proc_map == NULL);
    CHECK(dITGShellStatusDescs[nITGShellStatusThrown].proc_hit == itGShellCommonProcHit);
    CHECK(dITGShellStatusDescs[nITGShellStatusThrown].proc_shield == itGShellCommonProcShield);
    CHECK(dITGShellStatusDescs[nITGShellStatusThrown].proc_setoff == itGShellCommonProcShield);
    CHECK(dITGShellStatusDescs[nITGShellStatusThrown].proc_hop == itMainCommonProcHop);
    CHECK(dITGShellStatusDescs[nITGShellStatusThrown].proc_reflector == itMainCommonProcReflector);
    CHECK(dITGShellStatusDescs[nITGShellStatusDropped].proc_map == itGShellThrownProcMap);
    /* Ground Spin/Air Spin: proc_shield reuses proc_hit (unlike Thrown/
     * Dropped's own shared proc_shield/proc_setoff pair), and proc_hop/
     * proc_setoff are both NULL -- the spin can't be hopped or set off,
     * only hit, reflected, or damaged further. */
    CHECK(dITGShellStatusDescs[nITGShellStatusSpin].proc_update == itGShellSpinProcUpdate);
    CHECK(dITGShellStatusDescs[nITGShellStatusSpin].proc_map == itGShellSpinProcMap);
    CHECK(dITGShellStatusDescs[nITGShellStatusSpin].proc_hit == itGShellCommonProcHit);
    CHECK(dITGShellStatusDescs[nITGShellStatusSpin].proc_shield == itGShellCommonProcHit);
    CHECK(dITGShellStatusDescs[nITGShellStatusSpin].proc_hop == NULL);
    CHECK(dITGShellStatusDescs[nITGShellStatusSpin].proc_setoff == NULL);
    CHECK(dITGShellStatusDescs[nITGShellStatusSpin].proc_damage == itGShellSpinProcDamage);
    CHECK(dITGShellStatusDescs[nITGShellStatusSpinAir].proc_update == itGShellFallProcUpdate);
    CHECK(dITGShellStatusDescs[nITGShellStatusSpinAir].proc_map == itGShellThrownProcMap);
    CHECK(dITGShellStatusDescs[nITGShellStatusSpinAir].proc_hit == itGShellCommonProcHit);
    CHECK(dITGShellStatusDescs[nITGShellStatusSpinAir].proc_damage == itGShellSpinProcDamage);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.mobj = &tmobj;
    titem.user_data.p = &tip;
    titem.obj = &tdobj;

    /* itGShellFallProcUpdate: gravity only -- unlike StarRod's own pair,
     * this one has no itVisualsUpdateSpin call. */
    r = itGShellFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITGSHELL_GRAVITY, 1e-4F);

    /* itGShellFallProcMap: health == 0 takes DestroyLanding, health != 0
     * the honest DestroyDropped composition -- both reset fresh, same
     * shape every itMap-adjacent test in this port uses. */
    {
        int i;
        for (i = 0; i < 2; i++)
        {
            memset(&tip.coll_data, 0, sizeof(tip.coll_data));
            tip.coll_data.p_translate = &tdobj.translate.vec.f;
            tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
            tip.coll_data.pos_prev = tdobj.translate.vec.f;
            tip.coll_data.floor_line_id = -2;
            tip.coll_data.ceil_line_id = -2;
            tip.coll_data.lwall_line_id = -2;
            tip.coll_data.rwall_line_id = -2;
            tip.coll_data.ewall_line_id = -2;
            tip.coll_data.ignore_line_id = -2;
            tip.times_landed = 0;
            tip.item_vars.shell.health = (i == 0) ? 0 : 2;
            r = itGShellFallProcMap(&titem);
            CHECK(r == FALSE);
            CHECK(tip.times_landed == 0);
        }
    }
    /* itGShellWaitProcMap: honest "no floor collided" composition. */
    memset(&tip.coll_data, 0, sizeof(tip.coll_data));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    r = itGShellWaitProcMap(&titem);
    CHECK(r == FALSE);

    /* itGShellWaitInitVars/WaitSetStatus: three branches -- slow (Wait),
     * fast + is_damage (Spin), fast + no damage (Wait, same fields as
     * the slow branch minus the explicit is_damage reset since it was
     * already FALSE). Only the first two are driven through SetStatus
     * directly; the Spin branch needs the anim scaffold below, so it is
     * proven separately by itGShellSpinSetStatus's own block. */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 5.0F;
    tip.item_vars.shell.is_damage = TRUE;
    itGShellWaitSetStatus(&titem);
    CHECK(tip.item_vars.shell.is_damage == FALSE);
    CHECK(tip.is_damage_all == TRUE);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK(tip.proc_map == itGShellWaitProcMap);

    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 50.0F;
    tip.item_vars.shell.is_damage = FALSE;
    itGShellWaitSetStatus(&titem);
    CHECK(tip.is_damage_all == TRUE);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
    CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK(tip.proc_map == itGShellWaitProcMap);

    /* itGShellFallSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.is_allow_pickup = TRUE;
    itGShellFallSetStatus(&titem);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_update == itGShellFallProcUpdate);
    CHECK(tip.proc_map == itGShellFallProcMap);

    /* itGShellCommonProcDamage: big damage (ground -> Spin, air ->
     * SpinAir) both need the anim scaffold via SpinSetStatus, proven
     * below; small damage (-> Wait/Fall by ga) is proven here. */
    memset(&tip, 0, sizeof(tip));
    tip.damage_queue = 1.0F;
    tip.damage_lr = 1;
    tip.ga = nMPKineticsGround;
    r = itGShellCommonProcDamage(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK(tip.proc_map == itGShellWaitProcMap);

    memset(&tip, 0, sizeof(tip));
    tip.damage_queue = 1.0F;
    tip.damage_lr = 1;
    tip.ga = nMPKineticsAir;
    r = itGShellCommonProcDamage(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK(tip.proc_map == itGShellFallProcMap);

    /* itGShellHoldSetStatus: rotate.y reset to a plain 0.0F -- unlike
     * StarRod's own Hold transition, no F_CST_DTOR32 wrapper in the
     * decomp text here. */
    memset(&tip, 0, sizeof(tip));
    tdobj.rotate.vec.f.y = 123.0F;
    itGShellHoldSetStatus(&titem);
    CHECK_EQF(tdobj.rotate.vec.f.y, 0.0F);
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == NULL);

    /* itGShellThrownProcUpdate/ProcMap, ThrownSetStatus/DroppedSetStatus:
     * both SetStatus functions set health=1/is_damage=TRUE, same as each
     * other. */
    memset(&tip, 0, sizeof(tip));
    r = itGShellThrownProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITGSHELL_GRAVITY, 1e-4F);

    memset(&tip.coll_data, 0, sizeof(tip.coll_data));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    r = itGShellThrownProcMap(&titem);
    CHECK(r == FALSE);

    memset(&tip, 0, sizeof(tip));
    itGShellThrownSetStatus(&titem);
    CHECK(tip.item_vars.shell.health == 1);
    CHECK(tip.item_vars.shell.is_damage == TRUE);
    CHECK(tip.proc_map == itGShellThrownProcMap);
    itGShellDroppedSetStatus(&titem);
    CHECK(tip.item_vars.shell.health == 1);
    CHECK(tip.item_vars.shell.is_damage == TRUE);
    CHECK(tip.proc_map == itGShellThrownProcMap);

    /* itGShellSpinProcUpdate: damage_all_delay is a u8, so `== -1` in the
     * decomp's own comparison promotes the field to int (255) against
     * the int literal -1 -- 255 != -1, so the just-set-to-255 value
     * falls straight through to the decrement below in the SAME call,
     * landing at 254, not 255. Caught by the host test failing on a
     * first-draft `(u8)-1` expectation; verified by reading the C
     * integer-promotion rule, not by guessing from the decomp's own
     * "wraparound" look. The lifetime countdown -> TRUE at 0. */
    memset(&tip, 0, sizeof(tip));
    tip.item_vars.shell.dust_effect_int = 1;
    tip.item_vars.shell.damage_all_delay = 0;
    tip.lifetime = 3;
    r = itGShellSpinProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK(tip.is_damage_all == TRUE);
    CHECK(tip.item_vars.shell.damage_all_delay == (u8)-2);
    CHECK(tip.lifetime == 2);

    memset(&tip, 0, sizeof(tip));
    tip.item_vars.shell.dust_effect_int = 1;
    tip.item_vars.shell.damage_all_delay = 5;
    tip.lifetime = 0;
    r = itGShellSpinProcUpdate(&titem);
    CHECK(r == TRUE);
    CHECK(tip.item_vars.shell.damage_all_delay == 4);

    /* itGShellSpinProcMap: itMapCheckLRWallProcNoFloor's own honest "no
     * floor" composition, then itMapCheckCollideAllRebound's own honest
     * "already touched last frame" no-op -- same reset-per-call shape
     * as the other itMap tests, composed the same way test_it_star's own
     * ProcMap test already proved for a single itMap call. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    tdobj.mobj = &tmobj;
    titem.obj = &tdobj;
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    tip.coll_data.mask_prev = MAP_FLAG_LWALL;
    tip.coll_data.mask_curr = MAP_FLAG_LWALL;
    tip.physics.vel_air.x = -4.0F;
    r = itGShellSpinProcMap(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, -4.0F, 1e-4F);

    /* itGShellCommonProcHit: health randomized in [0, HEALTH_MAX), a
     * fixed rebound vel.y, and a random-scaled reflected vel.x -- only
     * the deterministic parts are checked. itGShellCommonClearAnim's own
     * unconditional `DObjGetStruct(item_gobj)->mobj->matanim_joint...`
     * dereference (also reached from itGShellWaitInitVars) is why every
     * tdobj reset in this test keeps a real tmobj wired, not just the
     * Spin scaffold below -- caught as a genuine NULL-deref segfault
     * writing this test, the same "wire every field a function reaches"
     * lesson already seen for `.obj` vs `.user_data.p`. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    tdobj.mobj = &tmobj;
    titem.obj = &tdobj;
    tip.physics.vel_air.x = -10.0F;
    r = itGShellCommonProcHit(&titem);
    CHECK(r == FALSE);
    /* ProcHit sets hitstatus to Normal itself, but its own tail call
     * (itGShellFallSetStatus) sets it to None right after -- the same
     * "a keystone's own trailing call is part of its observable
     * contract" lesson for itManagerMakeItem's tail.
     * Caught by the host test on a first-draft Normal expectation. */
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone);
    CHECK(tip.item_vars.shell.health < ITGSHELL_HEALTH_MAX);
    CHECK_NEAR(tip.physics.vel_air.y, ITGSHELL_REBOUND_VEL_Y, 1e-4F);
    CHECK(tip.proc_update == itGShellFallProcUpdate);
    CHECK(tip.proc_map == itGShellFallProcMap);

    /* itGShellSpinProcDamage: same shape as CommonProcDamage but adds
     * (not sets) to vel_air.x, and its small-damage branch takes it to
     * Fall/Wait by ga, same as CommonProcDamage's own. */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 1.0F;
    tip.damage_queue = 1.0F;
    tip.damage_lr = 1;
    tip.ga = nMPKineticsGround;
    r = itGShellSpinProcDamage(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK(tip.proc_map == itGShellWaitProcMap);

    /* itGShellCommonProcShield: VelSetRebound, same as every prior
     * companion item's own proc_shield. */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 5.0F;
    r = itGShellCommonProcShield(&titem);
    CHECK(r == FALSE);

    /* The Spin/SpinAir machinery, driven for real: itGShellSpinAddAnim's
     * itGetPData arithmetic (a THIRD kind of reloc-offset arithmetic
     * this port hadn't used yet, see the file header) resolves to this
     * file's own llITCommonDataShellAnimJoint/MatAnimJoint stand-ins --
     * BSS-zeroed ints that, read back as an AObjEvent32 word, decode to
     * opcode 0 (nGCAnimEvent32End) by construction, so the real
     * gcAddDObjAnimJoint -> gcPlayAnimAll -> gcParseDObjAnimJoint tail
     * this function ends with runs to a genuine, safe completion rather
     * than needing a stub. DOBJ_PARENT_NULL is `(DObj*)1`, NOT a plain
     * zeroed pointer -- gcPlayAnimAll's own tree-walk termination check
     * (`dobj->parent == DOBJ_PARENT_NULL`) needs it set explicitly, or
     * it dereferences a NULL `dobj->parent` instead. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    memset(&tattr, 0, sizeof(tattr));
    tdobj.parent = DOBJ_PARENT_NULL;
    tdobj.parent_gobj = &titem;
    tdobj.mobj = &tmobj;
    tattr.data = (void*)(sHostItemModels + llITCommonDataShellDataStart);
    tip.attr = &tattr;
    titem.user_data.p = &tip;
    titem.obj = &tdobj;

    itGShellSpinAddAnim(&titem);
    CHECK(tdobj.anim_joint.event32 == (AObjEvent32*)(sHostItemModels + llITCommonDataShellAnimJoint));
    CHECK(tmobj.matanim_joint.event32 == (AObjEvent32*)(sHostItemModels + llITCommonDataShellMatAnimJoint));

    /* itGShellSpinInitVars/SpinSetStatus: clamps vel_air.x, sets lr from
     * its sign, resets vel_air.y, seeds the dust/damage_all_delay
     * timers, calls SpinAddAnim (proven above) and
     * itMainRefreshAttackColl, then dispatches to Spin. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.parent = DOBJ_PARENT_NULL;
    tdobj.parent_gobj = &titem;
    tdobj.mobj = &tmobj;
    tip.attr = &tattr;
    tip.physics.vel_air.x = 999.0F;
    itGShellSpinSetStatus(&titem);
    CHECK_NEAR(tip.physics.vel_air.x, ITGSHELL_CLAMP_VEL_X, 1e-4F);
    CHECK(tip.lr == 1);
    CHECK_NEAR(tip.physics.vel_air.y, 0.0F, 1e-4F);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.pickup_wait == ITEM_PICKUP_WAIT_DEFAULT);
    CHECK(tip.item_vars.shell.dust_effect_int == ITGSHELL_EFFECT_SPAWN_INT);
    CHECK(tip.item_vars.shell.damage_all_delay == ITGSHELL_DAMAGE_ALL_WAIT);
    CHECK(tip.is_damage_all == FALSE);
    CHECK(tip.proc_update == itGShellSpinProcUpdate);
    CHECK(tip.proc_map == itGShellSpinProcMap);

    /* itGShellSpinAirInitVars/SpinAirSetStatus: same clamp/lr shape,
     * minus the dust/anim/FGM setup Spin's own ground variant has. */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = -999.0F;
    itGShellSpinAirSetStatus(&titem);
    CHECK_NEAR(tip.physics.vel_air.x, -ITGSHELL_CLAMP_VEL_X, 1e-4F);
    CHECK(tip.lr == -1);
    CHECK(tip.is_damage_all == FALSE);
    CHECK(tip.proc_update == itGShellFallProcUpdate);
    CHECK(tip.proc_map == itGShellThrownProcMap);

    /* itGShellWaitInitVars's own Spin branch (fast + is_damage), and
     * itGShellCommonProcDamage's own big-damage/ground branch -- both
     * dispatch through itGShellSpinSetStatus, now provable end to end
     * with the anim scaffold above. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.parent = DOBJ_PARENT_NULL;
    tdobj.parent_gobj = &titem;
    tdobj.mobj = &tmobj;
    tip.attr = &tattr;
    tip.physics.vel_air.x = 50.0F;
    tip.item_vars.shell.is_damage = TRUE;
    itGShellWaitSetStatus(&titem);
    CHECK(tip.attack_coll.attack_state != nGMAttackStateOff);
    CHECK(tip.proc_update == itGShellSpinProcUpdate);
    CHECK(tip.proc_map == itGShellSpinProcMap);
}

extern ITDesc dITRShellItemDesc;
extern ITStatusDesc dITRShellStatusDescs[];

enum
{
    nITRShellStatusWait, nITRShellStatusFall, nITRShellStatusHold,
    nITRShellStatusThrown, nITRShellStatusDropped, nITRShellStatusSpin,
    nITRShellStatusSpinAir
};

static void test_it_rshell(void)
{
    GObj titem, tfighter1, tfighter2, towner;
    ITStruct tip;
    DObj tdobj, tdobj_f1, tdobj_f2, tdobj_owner;
    MObj tmobj;
    ITAttributes tattr;
    sb32 r;

    CHECK(dITRShellItemDesc.kind == nITKindRShell);
    CHECK(dITRShellItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITRShellItemDesc.o_attributes == (intptr_t)0x584);  /* reloc_data.us.h: llITCommonDataRShellItemAttributes */
    CHECK(dITRShellItemDesc.proc_update == itRShellFallProcUpdate);
    CHECK(dITRShellItemDesc.proc_map == itRShellFallProcMap);
    /* Unlike Green Shell's own ITDesc, proc_damage is wired at the top
     * level here too, not just per status row. */
    CHECK(dITRShellItemDesc.proc_damage == itRShellCommonProcDamage);

    CHECK(dITRShellStatusDescs[nITRShellStatusWait].proc_map == itRShellWaitProcMap);
    CHECK(dITRShellStatusDescs[nITRShellStatusWait].proc_damage == itRShellCommonProcDamage);
    CHECK(dITRShellStatusDescs[nITRShellStatusHold].proc_update == NULL);
    CHECK(dITRShellStatusDescs[nITRShellStatusThrown].proc_hit == itRShellCommonProcHit);
    CHECK(dITRShellStatusDescs[nITRShellStatusThrown].proc_shield == itRShellCommonProcShield);
    CHECK(dITRShellStatusDescs[nITRShellStatusThrown].proc_setoff == itRShellCommonProcShield);
    CHECK(dITRShellStatusDescs[nITRShellStatusThrown].proc_hop == itMainCommonProcHop);
    /* Red Shell's own dedicated reflector, not the shared
     * itMainCommonProcReflector Green Shell's identical rows use. */
    CHECK(dITRShellStatusDescs[nITRShellStatusThrown].proc_reflector == itRShellCommonProcReflector);
    CHECK(dITRShellStatusDescs[nITRShellStatusSpin].proc_update == itRShellSpinProcUpdate);
    CHECK(dITRShellStatusDescs[nITRShellStatusSpin].proc_map == itRShellSpinProcMap);
    CHECK(dITRShellStatusDescs[nITRShellStatusSpin].proc_hit == itRShellCommonProcHit);
    CHECK(dITRShellStatusDescs[nITRShellStatusSpin].proc_shield == itRShellCommonProcHit);
    CHECK(dITRShellStatusDescs[nITRShellStatusSpin].proc_hop == NULL);
    CHECK(dITRShellStatusDescs[nITRShellStatusSpin].proc_reflector == itRShellCommonProcReflector);
    CHECK(dITRShellStatusDescs[nITRShellStatusSpin].proc_damage == itRShellSpinProcDamage);
    CHECK(dITRShellStatusDescs[nITRShellStatusSpinAir].proc_update == itRShellFallProcUpdate);
    CHECK(dITRShellStatusDescs[nITRShellStatusSpinAir].proc_map == itRShellThrownProcMap);
    CHECK(dITRShellStatusDescs[nITRShellStatusSpinAir].proc_damage == itRShellCommonProcDamage);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.mobj = &tmobj;
    titem.user_data.p = &tip;
    titem.obj = &tdobj;

    /* itRShellFallProcUpdate: gravity, plus damage_all_delay's own u8
     * promotion trap Green Shell's SpinProcUpdate already found -- a
     * value just set to -1 (255) falls through to the same call's own
     * decrement, landing at 254. */
    tip.item_vars.shell.damage_all_delay = 0;
    r = itRShellFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITRSHELL_GRAVITY, 1e-4F);
    CHECK(tip.item_vars.shell.damage_all_delay == (u8)-2);
    CHECK(tip.owner_gobj == NULL);

    /* itRShellWaitProcMap/FallProcMap: honest no-collision compositions,
     * same shape as every prior itMap-adjacent test in this port. */
    memset(&tip.coll_data, 0, sizeof(tip.coll_data));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    r = itRShellWaitProcMap(&titem);
    CHECK(r == FALSE);

    {
        int i;
        for (i = 0; i < 2; i++)
        {
            memset(&tip.coll_data, 0, sizeof(tip.coll_data));
            tip.coll_data.p_translate = &tdobj.translate.vec.f;
            tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
            tip.coll_data.pos_prev = tdobj.translate.vec.f;
            tip.coll_data.floor_line_id = -2;
            tip.coll_data.ceil_line_id = -2;
            tip.coll_data.lwall_line_id = -2;
            tip.coll_data.rwall_line_id = -2;
            tip.coll_data.ewall_line_id = -2;
            tip.coll_data.ignore_line_id = -2;
            tip.times_landed = 0;
            tip.item_vars.shell.health = (i == 0) ? 0 : 2;
            r = itRShellFallProcMap(&titem);
            CHECK(r == FALSE);
            CHECK(tip.times_landed == 0);
        }
    }

    /* itRShellCommonSetStatusWaitOrSpin/ProcStatusWaitOrSpin: the slow
     * branch (Wait) -- the fast+is_damage (Spin) branch is proven below
     * with the anim scaffold. */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 5.0F;
    itRShellCommonProcStatusWaitOrSpin(&titem);
    CHECK(tip.item_vars.shell.is_damage == FALSE);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK(tip.proc_map == itRShellWaitProcMap);

    /* itRShellFallSetStatus */
    memset(&tip, 0, sizeof(tip));
    tip.is_allow_pickup = TRUE;
    itRShellFallSetStatus(&titem);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_update == itRShellFallProcUpdate);
    CHECK(tip.proc_map == itRShellFallProcMap);

    /* itRShellCommonProcDamage: small damage clears attack_state to Off
     * (unlike Green Shell's own CommonProcDamage, which leaves it
     * untouched on its small-damage branch); big damage needs the anim
     * scaffold, proven below. 0.5*ITRSHELL_DAMAGE_MUL_NORMAL(10) = 5.0,
     * under ITRSHELL_STOP_VEL_X(8) -- Green Shell's own analogous test
     * used damage_queue=1.0, but that file's DAMAGE_MUL_NORMAL(8) and
     * STOP_VEL_X(12) don't carry over; recomputed for THIS file's own
     * constants rather than assumed. */
    memset(&tip, 0, sizeof(tip));
    tip.damage_queue = 0.5F;
    tip.damage_lr = 1;
    r = itRShellCommonProcDamage(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);

    /* itRShellHoldSetStatus */
    memset(&tip, 0, sizeof(tip));
    tdobj.rotate.vec.f.y = 123.0F;
    itRShellHoldSetStatus(&titem);
    CHECK_EQF(tdobj.rotate.vec.f.y, F_CST_DTOR32(0.0F));
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == NULL);

    /* itRShellThrownSetStatus/DroppedSetStatus: both seed health=1,
     * is_damage=TRUE, a fresh damage_all_delay, and times_thrown=0. */
    memset(&tip, 0, sizeof(tip));
    itRShellThrownSetStatus(&titem);
    CHECK(tip.item_vars.shell.health == 1);
    CHECK(tip.item_vars.shell.is_damage == TRUE);
    CHECK(tip.item_vars.shell.damage_all_delay == ITRSHELL_DAMAGE_ALL_WAIT);
    CHECK(tip.times_thrown == 0);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_map == itRShellThrownProcMap);
    itRShellDroppedSetStatus(&titem);
    CHECK(tip.item_vars.shell.health == 1);
    CHECK(tip.proc_map == itRShellThrownProcMap);

    /* itRShellThrownProcMap: a real landing (not the honest no-collision
     * case) -- itMapCheckLanding's own TRUE branch sets lr from the
     * current vel_air.x's sign and a fixed post-landing vel_air.x. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    tdobj.mobj = &tmobj;
    titem.obj = &tdobj;
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.pos_prev.y = -100.0F;
    tdobj.translate.vec.f.y = 0.0F;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    tip.physics.vel_air.x = 5.0F;
    r = itRShellThrownProcMap(&titem);
    CHECK(r == FALSE);

    /* itRShellSpinEdgeInvertVelLR: a pure function, no stage needed --
     * flips vel_air.x/item_vars.shell.vel_x, sets lr from the `lr`
     * parameter's own 0/1 convention (0 = left = -1, nonzero = right =
     * +1), not from any sign test. */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 7.0F;
    tip.item_vars.shell.vel_x = 3.0F;
    itRShellSpinEdgeInvertVelLR(&titem, 1);
    CHECK_NEAR(tip.physics.vel_air.x, -7.0F, 1e-4F);
    CHECK_NEAR(tip.item_vars.shell.vel_x, -3.0F, 1e-4F);
    CHECK(tip.lr == 1);
    itRShellSpinEdgeInvertVelLR(&titem, 0);
    CHECK_NEAR(tip.physics.vel_air.x, 7.0F, 1e-4F);
    CHECK(tip.lr == -1);

    /* itRShellSpinCheckCollisionEdge: honest "no floor line" composition
     * -- no stage exports collision data yet (the same limitation every
     * itMap-adjacent test in this port has), so only
     * the mpCollisionCheckExistLineID(-2) -> FALSE early-out is provable
     * here; the real edge-bounce arithmetic is proven directly above via
     * itRShellSpinEdgeInvertVelLR itself. */
    memset(&tip, 0, sizeof(tip));
    memset(&tattr, 0, sizeof(tattr));
    tip.attr = &tattr;
    tip.coll_data.floor_line_id = -2;
    tip.lr = -1;
    itRShellSpinCheckCollisionEdge(&titem);
    CHECK(tip.lr == -1);

    /* itRShellSpinProcMap: honest "already touched last frame" no-op,
     * same composed shape Green Shell's own SpinProcMap test uses. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    tdobj.mobj = &tmobj;
    titem.obj = &tdobj;
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    tip.coll_data.mask_prev = MAP_FLAG_LWALL;
    tip.coll_data.mask_curr = MAP_FLAG_LWALL;
    tip.physics.vel_air.x = -4.0F;
    r = itRShellSpinProcMap(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, -4.0F, 1e-4F);

    /* itRShellCommonProcHit: the interact counter reaching 0 destroys
     * the shell outright (TRUE), independent of health -- the genuinely
     * new mechanic beyond Green Shell's own health-only ProcHit. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    tdobj.mobj = &tmobj;
    titem.obj = &tdobj;
    tip.item_vars.shell.interact = 1;
    r = itRShellCommonProcHit(&titem);
    CHECK(r == TRUE);

    /* Unlike Green Shell's own ProcHit (whose tail is FallSetStatus, no
     * anim work), a surviving hit here always ends in itRShellSpinSetStatus
     * -- needs the same anim scaffold the Spin machinery block below
     * uses (DOBJ_PARENT_NULL, a real ITAttributes, etc.), set up early
     * here rather than only later. Caught as a genuine NULL-deref
     * segfault writing this test, the same class of oversight as Green
     * Shell's own DObj.mobj gap -- a function's own tail
     * transition can reach further scaffold than the function's own
     * name suggests. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    memset(&tattr, 0, sizeof(tattr));
    tdobj.parent = DOBJ_PARENT_NULL;
    tdobj.parent_gobj = &titem;
    tdobj.mobj = &tmobj;
    tattr.data = (void*)(sHostItemModels + llITCommonDataShellDataStart);
    tip.attr = &tattr;
    titem.obj = &tdobj;
    tip.item_vars.shell.interact = 2;
    tip.hit_lr = 1;
    tip.physics.vel_air.x = 5.0F;
    r = itRShellCommonProcHit(&titem);
    CHECK(r == FALSE);
    /* ProcHit decrements interact to 1 itself, but its own tail call
     * (itRShellSpinSetStatus -> SpinInitVars, is_setup_vars starting
     * FALSE on this fresh tip) resets interact to ITRSHELL_INTERACT_MAX
     * right after -- the same "a keystone's own trailing call is part
     * of its observable contract" lesson for
     * itManagerMakeItem's tail, and again for Green
     * Shell's own ProcHit/hitstatus. A first-draft check here expected
     * the bare post-decrement value (1), not the post-tail-call one. */
    CHECK(tip.item_vars.shell.interact == ITRSHELL_INTERACT_MAX);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
    CHECK(tip.item_vars.shell.health < ITRSHELL_HEALTH_MAX);
    CHECK_NEAR(tip.physics.vel_air.x, ((-5.0F) + (ITRSHELL_RECOIL_VEL_X * 1)) * ITRSHELL_RECOIL_MUL_X, 1e-3F);

    /* itRShellSpinProcDamage: interact-to-zero destroys outright (TRUE);
     * otherwise the same big/small damage split CommonProcDamage has,
     * but ADDING to vel_air.x rather than setting it, and the big-damage
     * branch always goes back through Spin (never SpinAir by ga). */
    memset(&tip, 0, sizeof(tip));
    tip.item_vars.shell.interact = 1;
    r = itRShellSpinProcDamage(&titem);
    CHECK(r == TRUE);

    memset(&tip, 0, sizeof(tip));
    tip.item_vars.shell.interact = 2;
    tip.physics.vel_air.x = 1.0F;
    tip.damage_queue = 1.0F;
    tip.damage_lr = 1;
    r = itRShellSpinProcDamage(&titem);
    CHECK(r == FALSE);
    CHECK(tip.item_vars.shell.interact == 1);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);

    /* itRShellCommonProcShield: VelSetRebound, same as every prior
     * companion item's own proc_shield. */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 5.0F;
    r = itRShellCommonProcShield(&titem);
    CHECK(r == FALSE);

    /* itRShellCommonProcReflector: interact-to-zero destroys outright
     * (TRUE); otherwise biases lr/vel_air.x toward whichever side of the
     * reflecting fighter (owner_gobj) the shell is on, then adds a fixed
     * kick. Item left of the fighter -> lr = -1; vel_air.x >= 0 flips
     * negative. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tdobj_owner, 0, sizeof(tdobj_owner));
    titem.obj = &tdobj;
    towner.obj = &tdobj_owner;
    tip.owner_gobj = &towner;
    tdobj.translate.vec.f.x = -10.0F;
    tdobj_owner.translate.vec.f.x = 0.0F;
    tip.item_vars.shell.interact = 2;
    tip.physics.vel_air.x = 3.0F;
    tip.item_vars.shell.vel_x = 3.0F;
    r = itRShellCommonProcReflector(&titem);
    CHECK(r == FALSE);
    CHECK(tip.lr == -1);
    CHECK_NEAR(tip.physics.vel_air.x, -3.0F + (ITRSHELL_ADD_VEL_X * -1), 1e-3F);
    CHECK_NEAR(tip.item_vars.shell.vel_x, -3.0F, 1e-4F);
    CHECK(tip.owner_gobj == NULL);

    /* Unconditionally dereferences ip->owner_gobj (via DObjGetStruct, at
     * the same declaration as item_dobj) BEFORE the interact==0 check --
     * unlike itRShellCommonProcHit/SpinProcDamage, whose own interact
     * decrement+check comes first with nothing dereferenced yet. A bare
     * memset here leaves owner_gobj NULL and segfaults; owner_gobj must
     * stay wired even for the "destroys immediately" case. Caught as a
     * genuine NULL-deref bisected with per-statement fprintf/fflush
     * markers across the whole function after two narrower marker
     * passes gave misleading zero-output results -- worth remembering:
     * bisecting a SUBRANGE of a function with no markers before it can
     * mask where the crash really is; mark the whole function on the
     * first pass, not just the suspect block. */
    memset(&tip, 0, sizeof(tip));
    tip.owner_gobj = &towner;
    tip.item_vars.shell.interact = 1;
    r = itRShellCommonProcReflector(&titem);
    CHECK(r == TRUE);

    /* The Spin/SpinAir machinery, driven for real, same anim scaffold
     * Green Shell's own test uses: DOBJ_PARENT_NULL is
     * `(DObj*)1`, not a plain zeroed pointer, or gcPlayAnimAll's own
     * tree-walk termination dereferences a NULL dobj->parent. Also
     * covers itRShellSpinAddAnim's own itGetPData arithmetic against the
     * SAME three symbols Green Shell's own stand-ins already prove. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    memset(&tattr, 0, sizeof(tattr));
    tdobj.parent = DOBJ_PARENT_NULL;
    tdobj.parent_gobj = &titem;
    tdobj.mobj = &tmobj;
    tattr.data = (void*)(sHostItemModels + llITCommonDataShellDataStart);
    tip.attr = &tattr;
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tip.physics.vel_air.x = 999.0F;
    itRShellSpinSetStatus(&titem);
    CHECK_NEAR(tip.physics.vel_air.x, ITRSHELL_CLAMP_VEL_X, 1e-4F);
    CHECK(tip.lr == 1);
    CHECK_NEAR(tip.physics.vel_air.y, 0.0F, 1e-4F);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.pickup_wait == ITEM_PICKUP_WAIT_DEFAULT);
    CHECK(tip.item_vars.shell.is_setup_vars == TRUE);
    CHECK(tip.lifetime == ITRSHELL_LIFETIME);
    CHECK(tip.item_vars.shell.interact == ITRSHELL_INTERACT_MAX);
    CHECK(tip.item_vars.shell.dust_effect_int == ITRSHELL_EFFECT_SPAWN_INT);
    CHECK(tip.owner_gobj == NULL);
    CHECK(tip.ga == nMPKineticsGround);
    CHECK(tip.proc_update == itRShellSpinProcUpdate);
    CHECK(tip.proc_map == itRShellSpinProcMap);
    CHECK(tdobj.anim_joint.event32 == (AObjEvent32*)(sHostItemModels + llITCommonDataShellAnimJoint));
    CHECK(tmobj.matanim_joint.event32 == (AObjEvent32*)(sHostItemModels + llITCommonDataShellMatAnimJoint));

    /* is_setup_vars gates lifetime/interact from being reset on a SECOND
     * Spin entry (e.g. re-hit while already spinning) -- proven by
     * driving SpinSetStatus again without resetting is_setup_vars. */
    tip.lifetime = 111;
    tip.item_vars.shell.interact = 5;
    itRShellSpinSetStatus(&titem);
    CHECK(tip.lifetime == 111);
    CHECK(tip.item_vars.shell.interact == 5);

    /* itRShellSpinAirInitVars/SpinAirSetStatus: same clamp/lr shape,
     * clamped to ITRSHELL_CLAMP_AIR_X (not CLAMP_VEL_X), no anim/FGM
     * setup. */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = -999.0F;
    itRShellSpinAirSetStatus(&titem);
    CHECK_NEAR(tip.physics.vel_air.x, -ITRSHELL_CLAMP_AIR_X, 1e-4F);
    CHECK(tip.lr == -1);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_update == itRShellFallProcUpdate);
    CHECK(tip.proc_map == itRShellThrownProcMap);

    /* itRShellCommonSetStatusWaitOrSpin's own Spin branch (fast +
     * is_damage), now provable end to end with the anim scaffold. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.parent = DOBJ_PARENT_NULL;
    tdobj.parent_gobj = &titem;
    tdobj.mobj = &tmobj;
    tip.attr = &tattr;
    tip.physics.vel_air.x = 50.0F;
    tip.item_vars.shell.is_damage = TRUE;
    itRShellCommonProcStatusWaitOrSpin(&titem);
    CHECK(tip.attack_coll.attack_state != nGMAttackStateOff);
    CHECK(tip.proc_update == itRShellSpinProcUpdate);
    CHECK(tip.proc_map == itRShellSpinProcMap);

    /* itRShellSpinUpdateFollowPlayer/SpinSearchFollowPlayer: driven for
     * real over a two-fighter gGCCommonLinks chain -- the nearer fighter
     * (by squared XY distance) is the one the shell nudges toward,
     * confirmed via the sign of the resulting vel_air.x/lr, not just
     * "no crash". */
    {
        GObj *save_fighter = gGCCommonLinks[nGCCommonLinkIDFighter];

        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        memset(&tdobj_f1, 0, sizeof(tdobj_f1));
        memset(&tdobj_f2, 0, sizeof(tdobj_f2));
        titem.obj = &tdobj;
        tfighter1.obj = &tdobj_f1;
        tfighter2.obj = &tdobj_f2;
        tdobj.translate.vec.f.x = 0.0F;
        tdobj_f1.translate.vec.f.x = 100.0F;
        tdobj_f2.translate.vec.f.x = 10.0F;
        tfighter1.link_next = &tfighter2;
        tfighter2.link_next = NULL;
        gGCCommonLinks[nGCCommonLinkIDFighter] = &tfighter1;

        tip.ga = nMPKineticsGround;
        tip.physics.vel_air.x = 0.0F;
        tip.attack_coll.attack_state = nGMAttackStateOff;
        itRShellSpinSearchFollowPlayer(&titem);
        /* nearest is tfighter2 at x=10, to the right -> nudges +x */
        CHECK(tip.item_vars.shell.vel_x > 0.0F);
        CHECK(tip.physics.vel_air.x > 0.0F);
        CHECK(tip.lr == 1);
        CHECK(tip.attack_coll.attack_state != nGMAttackStateOff);

        gGCCommonLinks[nGCCommonLinkIDFighter] = save_fighter;
    }
}

extern ITDesc dITNBumperItemDesc;
extern ITStatusDesc dITNBumperStatusDescs[];

enum
{
    nITNBumperStatusWait, nITNBumperStatusFall, nITNBumperStatusHold,
    nITNBumperStatusThrown, nITNBumperStatusDropped, nITNBumperStatusAttached,
    nITNBumperStatusHitAir, nITNBumperStatusGDisappear
};

static void test_it_nbumper(void)
{
    GObj titem, towner;
    ITStruct tip;
    DObj tdobj, tdobj_owner;
    MObj tmobj;
    ITAttributes tattr;
    sb32 r;

    CHECK(dITNBumperItemDesc.kind == nITKindNBumper);
    CHECK(dITNBumperItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITNBumperItemDesc.o_attributes == (intptr_t)0x69c);  /* reloc_data.us.h: llITCommonDataNBumperItemAttributes */
    CHECK(dITNBumperItemDesc.proc_update == itNBumperFallProcUpdate);
    CHECK(dITNBumperItemDesc.proc_map == itNBumperFallProcMap);
    CHECK(dITNBumperItemDesc.proc_hit == NULL);
    CHECK(dITNBumperItemDesc.proc_damage == NULL);

    CHECK(dITNBumperStatusDescs[nITNBumperStatusWait].proc_map == itNBumperWaitProcMap);
    CHECK(dITNBumperStatusDescs[nITNBumperStatusFall].proc_update == itNBumperFallProcUpdate);
    CHECK(dITNBumperStatusDescs[nITNBumperStatusHold].proc_update == NULL);
    CHECK(dITNBumperStatusDescs[nITNBumperStatusThrown].proc_hit == itNBumperThrownProcHit);
    CHECK(dITNBumperStatusDescs[nITNBumperStatusThrown].proc_shield == itNBumperThrownProcShield);
    CHECK(dITNBumperStatusDescs[nITNBumperStatusThrown].proc_hop == itMainCommonProcHop);
    CHECK(dITNBumperStatusDescs[nITNBumperStatusThrown].proc_reflector == itNBumperThrownProcReflector);
    CHECK(dITNBumperStatusDescs[nITNBumperStatusDropped].proc_map == itNBumperThrownProcMap);
    /* the ONLY status whose proc_update differs from proc_map's own
     * "Thrown" family -- Attached is its own self-contained state. */
    CHECK(dITNBumperStatusDescs[nITNBumperStatusAttached].proc_update == itNBumperAttachedProcUpdate);
    CHECK(dITNBumperStatusDescs[nITNBumperStatusAttached].proc_map == itNBumperAttachedProcMap);
    CHECK(dITNBumperStatusDescs[nITNBumperStatusAttached].proc_hit == itNBumperAttachedProcHit);
    CHECK(dITNBumperStatusDescs[nITNBumperStatusAttached].proc_reflector == itNBumperAttachedProcReflector);
    /* HitAir reuses Thrown's own proc_map/proc_hit/proc_shield/proc_reflector,
     * only proc_update is its own. */
    CHECK(dITNBumperStatusDescs[nITNBumperStatusHitAir].proc_update == itNBumperHitAirProcUpdate);
    CHECK(dITNBumperStatusDescs[nITNBumperStatusHitAir].proc_map == itNBumperThrownProcMap);
    CHECK(dITNBumperStatusDescs[nITNBumperStatusHitAir].proc_hit == itNBumperThrownProcHit);
    CHECK(dITNBumperStatusDescs[nITNBumperStatusGDisappear].proc_update == itNBumperGDisappearProcUpdate);
    CHECK(dITNBumperStatusDescs[nITNBumperStatusGDisappear].proc_map == NULL);

    /* itNBumperFallProcUpdate: gravity, the multi-driven shrink-scale
     * animation (2.0 - (10 - multi)*0.1, decrementing multi each frame),
     * and the SAME u16-width `!= -1` integer-promotion trap
     * seen for Shell's u8 damage_all_delay -- a u16 set to -1 (0xFFFF)
     * promotes to int 65535, never equal to the int literal -1, so it
     * falls straight through to this same call's own decrement, landing
     * at (u16)-2, not (u16)-1. Applied directly here rather than
     * rediscovered as a surprise. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.mobj = &tmobj;
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tip.multi = 5;
    tip.item_vars.bumper.damage_all_delay = 0;
    tip.owner_gobj = &titem; /* nonzero, so itMainClearOwnerStats's own NULL-out is provable */
    r = itNBumperFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITBUMPER_GRAVITY_NORMAL, 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.x, 2.0F - (10 - 5) * 0.1F, 1e-4F);
    CHECK(tip.multi == 4);
    CHECK(tip.item_vars.bumper.damage_all_delay == (u16)-2);
    CHECK(tip.owner_gobj == NULL);

    /* multi == 0: the else branch resets scale to 1.0 instead. */
    memset(&tip, 0, sizeof(tip));
    tdobj.scale.vec.f.x = tdobj.scale.vec.f.y = tdobj.scale.vec.f.z = 9.0F;
    tip.item_vars.bumper.damage_all_delay = 1;
    r = itNBumperFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tdobj.scale.vec.f.x, 1.0F, 1e-4F);
    CHECK(tip.item_vars.bumper.damage_all_delay == 0);

    /* itNBumperWaitProcMap/FallProcMap: honest no-collision compositions,
     * same shape as every prior itMap-adjacent test in this port. */
    memset(&tip, 0, sizeof(tip));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    r = itNBumperWaitProcMap(&titem);
    CHECK(r == FALSE);
    memset(&tip.coll_data, 0, sizeof(tip.coll_data));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    r = itNBumperFallProcMap(&titem);
    CHECK(r == FALSE);

    /* itNBumperThrownProcHit: a fixed airborne-rebound kick keyed off
     * hit_lr, a three-frame "hit" palette, and a tail transition
     * (itNBumperHitAirSetStatus) into HitAir. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.mobj = &tmobj;
    titem.obj = &tdobj;
    tip.hit_lr = 1;
    r = itNBumperThrownProcHit(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tdobj.scale.vec.f.x, 2.0F, 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.y, 2.0F, 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.z, 2.0F, 1e-4F);
    CHECK(tip.item_vars.bumper.hit_anim_length == ITBUMPER_HIT_ANIM_LENGTH);
    CHECK_NEAR(tmobj.palette_id, 1.0F, 1e-4F);
    CHECK_NEAR(tip.physics.vel_air.x, ITBUMPER_REBOUND_AIR_X * 1, 1e-3F);
    CHECK_NEAR(tip.physics.vel_air.y, ITBUMPER_REBOUND_AIR_Y, 1e-3F);
    CHECK(tip.multi == ITBUMPER_HIT_SCALE);
    /* the tail call's own effect: HitAir's own damage_all_delay reset,
     * and the status transition itself. */
    CHECK(tip.item_vars.bumper.damage_all_delay == ITBUMPER_DAMAGE_ALL_WAIT);
    CHECK(tip.proc_update == itNBumperHitAirProcUpdate);

    /* itNBumperWaitSetStatus/FallSetStatus/HoldSetStatus. Wait's own
     * ITStatusDesc row leaves proc_update NULL (only Fall/Thrown/
     * Dropped/Attached/HitAir update every frame; Wait only reacts to
     * itNBumperWaitProcMap's own wall check). */
    memset(&tip, 0, sizeof(tip));
    itNBumperWaitSetStatus(&titem);
    CHECK(tip.is_allow_pickup == TRUE);
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == itNBumperWaitProcMap);

    memset(&tip, 0, sizeof(tip));
    tip.is_allow_pickup = TRUE;
    itNBumperFallSetStatus(&titem);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_update == itNBumperFallProcUpdate);
    CHECK(tip.proc_map == itNBumperFallProcMap);

    memset(&tip, 0, sizeof(tip));
    itNBumperHoldSetStatus(&titem);
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == NULL);

    /* itNBumperThrownProcUpdate: same gravity/damage-trap/spin shape as
     * FallProcUpdate, but with NO multi-driven scale step at all -- the
     * scale animation is Fall/Attached/HitAir's own thing, not Thrown's. */
    memset(&tip, 0, sizeof(tip));
    tip.item_vars.bumper.damage_all_delay = 0;
    r = itNBumperThrownProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITBUMPER_GRAVITY_NORMAL, 1e-4F);
    CHECK(tip.item_vars.bumper.damage_all_delay == (u16)-2);

    /* itNBumperThrownProcMap: honest no-collision composition. */
    memset(&tip, 0, sizeof(tip));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    r = itNBumperThrownProcMap(&titem);
    CHECK(r == FALSE);

    /* itNBumperThrownProcShield */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 5.0F;
    tip.owner_gobj = &titem;
    r = itNBumperThrownProcShield(&titem);
    CHECK(r == FALSE);
    CHECK(tip.owner_gobj == NULL);

    /* itNBumperThrownProcReflector: flips vel_air.x only when it's
     * currently moving the SAME way the reflecting fighter faces
     * (vel_air.x * fp->lr < 0). */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj_owner, 0, sizeof(tdobj_owner));
    towner.obj = &tdobj_owner;
    tip.owner_gobj = &towner;
    towner.user_data.p = NULL;
    {
        FTStruct tfp_owner;
        memset(&tfp_owner, 0, sizeof(tfp_owner));
        towner.user_data.p = &tfp_owner;
        tfp_owner.lr = 1;
        tip.physics.vel_air.x = -5.0F;
        r = itNBumperThrownProcReflector(&titem);
        CHECK(r == FALSE);
        CHECK_NEAR(tip.physics.vel_air.x, 5.0F, 1e-4F);
        CHECK(tip.owner_gobj == NULL);
    }

    /* itNBumperThrownSetStatus/DroppedSetStatus: both seed a fresh
     * damage_all_delay and the fixed collision half-heights. */
    memset(&tip, 0, sizeof(tip));
    itNBumperThrownSetStatus(&titem);
    CHECK(tip.item_vars.bumper.damage_all_delay == ITBUMPER_DAMAGE_ALL_WAIT);
    CHECK_NEAR(tip.coll_data.map_coll.top, ITBUMPER_COLL_SIZE, 1e-4F);
    CHECK_NEAR(tip.coll_data.map_coll.bottom, -ITBUMPER_COLL_SIZE, 1e-4F);
    CHECK(tip.proc_map == itNBumperThrownProcMap);
    itNBumperDroppedSetStatus(&titem);
    CHECK(tip.item_vars.bumper.damage_all_delay == ITBUMPER_DAMAGE_ALL_WAIT);
    CHECK(tip.proc_map == itNBumperThrownProcMap);

    /* itNBumperAttachedSetModelPitch: orients the model's own Z rotate to
     * the floor normal's arctangent, and remembers the floor line it
     * pinned to. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.obj = &tdobj;
    tip.coll_data.floor_angle.x = 0.0F;
    tip.coll_data.floor_angle.y = 1.0F;
    tip.coll_data.floor_line_id = 7;
    itNBumperAttachedSetModelPitch(&titem);
    CHECK(tip.attach_line_id == 7);
    CHECK_NEAR(tdobj.rotate.vec.f.z, syUtilsArcTan2(1.0F, 0.0F) - F_CLC_DTOR32(90.0F), 1e-4F);

    /* itNBumperAttachedInitVars: the fourth kind of reloc-offset use this
     * port has needed -- not an inert o_attributes block, not a one-word
     * AObjEvent32 script, but a DL swap (assigned, never dereferenced
     * here) AND a full MObjSub struct copy through gcAddMObjForDObj,
     * which IS dereferenced. Both land inside the faked model file: the
     * MObjSub comes back at sHostItemModels + llITCommonDataNBumperWait-
     * MObjSub, which is zeroed and 0x7a38 into a 0x8000 buffer, so the
     * struct copy reads a real zeroed MObjSub rather than running off an
     * object sized for an `int`. (that offset is a real
     * number, not a stand-in object; the buffer is what keeps the
     * copy safe now -- see the note by sHostItemModels.) */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    memset(&tattr, 0, sizeof(tattr));
    tdobj.mobj = &tmobj;
    tattr.data = (void*)(sHostItemModels + llITCommonDataNBumperDataStart);
    tip.attr = &tattr;
    titem.obj = &tdobj;
    tip.physics.vel_air.x = tip.physics.vel_air.y = tip.physics.vel_air.z = 9.0F;
    tip.owner_gobj = &titem;
    itNBumperAttachedInitVars(&titem);
    CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK(tdobj.dl == (Gfx*)(sHostItemModels + llITCommonDataNBumperWaitDisplayList));
    CHECK(tdobj.mobj != NULL);
    CHECK_NEAR(tdobj.scale.vec.f.x, 1.0F, 1e-4F);
    CHECK_NEAR(tip.coll_data.map_coll.top, ITBUMPER_COLL_SIZE, 1e-4F);
    CHECK(tip.attach_line_id == tip.coll_data.floor_line_id);
    CHECK(tip.is_attach_surface == TRUE);
    CHECK(tip.lifetime == ITBUMPER_LIFETIME);
    CHECK(tip.owner_gobj == NULL);

    /* itNBumperAttachedProcHit: same fixed-kick shape as ThrownProcHit,
     * but scales only X/Z (not Y), and does NOT tail into a status
     * transition of its own. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.mobj = &tmobj;
    titem.obj = &tdobj;
    tdobj.scale.vec.f.y = 42.0F;
    tip.hit_lr = 1;
    r = itNBumperAttachedProcHit(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tdobj.scale.vec.f.x, 2.0F, 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.z, 2.0F, 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.y, 42.0F, 1e-4F); /* untouched */
    CHECK(tip.item_vars.bumper.hit_anim_length == ITBUMPER_HIT_ANIM_LENGTH);
    CHECK_NEAR(tmobj.palette_id, 1.0F, 1e-4F);
    CHECK(tip.lr == -1);
    CHECK_NEAR(tip.physics.vel_air.x, 1 * ITBUMPER_REBOUND_VEL_X, 1e-3F);
    CHECK(tip.multi == ITBUMPER_HIT_SCALE);

    /* itNBumperAttachedProcUpdate: the palette-fade-out edge (hit_anim_length
     * already 0, palette still 1 -> clears it this frame instead of
     * decrementing further), the honest "no floor line" edge-bounce
     * no-op (mpCollisionCheckExistLineID(-2) -> FALSE, same limitation
     * Red Shell's own SpinCheckCollisionEdge test has), the
     * multi-below-STOPVEL_WAIT zeroing, and a lifetime that runs out
     * tailing into itNBumperGDisappearSetStatus. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    memset(&tattr, 0, sizeof(tattr));
    tdobj.mobj = &tmobj;
    titem.obj = &tdobj;
    tip.attr = &tattr;
    tip.item_vars.bumper.hit_anim_length = 0;
    tmobj.palette_id = 1.0F;
    tip.coll_data.floor_line_id = -2;
    tip.lr = 1;
    tip.multi = 2; /* < ITBUMPER_STOPVEL_WAIT(4) */
    tip.physics.vel_air.x = 50.0F;
    tip.lifetime = 5;
    r = itNBumperAttachedProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tmobj.palette_id, 0.0F, 1e-4F);
    CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.x, 2.0F - (10 - 2) * 0.1F, 1e-4F);
    CHECK(tip.multi == 1);
    CHECK(tip.lifetime == 4);

    /* lifetime reaching 0 tails into GDisappearSetStatus, decremented
     * AFTER the tail call (per the decomp's own line order) -- so the
     * observed lifetime is GDisappear's own reset value minus one, not
     * DESPAWN_TIMER itself, the same "keystone's own trailing call is
     * part of its observable contract" lesson. */
    memset(&tip, 0, sizeof(tip));
    tip.coll_data.floor_line_id = -2;
    tip.lr = 1;
    tip.multi = 0;
    tip.lifetime = 0;
    r = itNBumperAttachedProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK(tip.proc_update == itNBumperGDisappearProcUpdate);
    CHECK(tip.lifetime == ITBUMPER_DESPAWN_TIMER - 1);

    /* itNBumperAttachedProcMap: honest "no collision at all" composition
     * -- itMapCheckLRWallProcNoFloor's own no-wall-hit path, same
     * limitation every itMap-adjacent test in this port has. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.mobj = &tmobj;
    titem.obj = &tdobj;
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    tip.attach_line_id = -2;
    r = itNBumperAttachedProcMap(&titem);
    CHECK(r == FALSE);

    /* itNBumperAttachedProcReflector: same fixed-kick shape as
     * ThrownProcReflector, but keyed off the fighter's OWN lr (not a
     * velocity-direction test), and always flips both lr and vel_air.x
     * toward the fighter's facing. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    memset(&tdobj_owner, 0, sizeof(tdobj_owner));
    tdobj.mobj = &tmobj;
    titem.obj = &tdobj;
    towner.obj = &tdobj_owner;
    tip.owner_gobj = &towner;
    {
        FTStruct tfp_owner;
        memset(&tfp_owner, 0, sizeof(tfp_owner));
        towner.user_data.p = &tfp_owner;
        tfp_owner.lr = -1;
        r = itNBumperAttachedProcReflector(&titem);
        CHECK(r == FALSE);
        CHECK_NEAR(tdobj.scale.vec.f.x, 2.0F, 1e-4F);
        CHECK_NEAR(tdobj.scale.vec.f.z, 2.0F, 1e-4F);
        CHECK(tip.item_vars.bumper.hit_anim_length == 3);
        CHECK_NEAR(tmobj.palette_id, 1.0F, 1e-4F);
        CHECK_NEAR(tip.physics.vel_air.x, -(-1) * ITBUMPER_REBOUND_VEL_X, 1e-3F);
        CHECK(tip.lr == -1);
        CHECK(tip.multi == ITBUMPER_HIT_SCALE);
        CHECK(tip.owner_gobj == NULL);
    }

    /* itNBumperAttachedSetStatus: AttachedInitVars's own effects plus the
     * status transition, driven together. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    memset(&tattr, 0, sizeof(tattr));
    tdobj.mobj = &tmobj;
    tattr.data = (void*)(sHostItemModels + llITCommonDataNBumperDataStart);
    tip.attr = &tattr;
    titem.obj = &tdobj;
    itNBumperAttachedSetStatus(&titem);
    CHECK(tip.is_attach_surface == TRUE);
    CHECK(tip.lifetime == ITBUMPER_LIFETIME);
    CHECK(tip.proc_update == itNBumperAttachedProcUpdate);
    CHECK(tip.proc_map == itNBumperAttachedProcMap);

    /* itNBumperHitAirProcUpdate: same palette-fade/multi-scale/damage-trap
     * shape as AttachedProcUpdate/FallProcUpdate, but with the SHARPER
     * gravity constant (ITBUMPER_GRAVITY_HIT, not _NORMAL). */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.mobj = &tmobj;
    titem.obj = &tdobj;
    tip.item_vars.bumper.hit_anim_length = 5;
    tmobj.palette_id = 1.0F;
    tip.multi = 3;
    tip.item_vars.bumper.damage_all_delay = 0;
    r = itNBumperHitAirProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK(tip.item_vars.bumper.hit_anim_length == 4); /* decremented, not cleared -- still nonzero */
    CHECK_NEAR(tip.physics.vel_air.y, -ITBUMPER_GRAVITY_HIT, 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.x, 2.0F - (10 - 3) * 0.1F, 1e-4F);
    CHECK(tip.multi == 2);
    CHECK(tip.item_vars.bumper.damage_all_delay == (u16)-2);

    /* itNBumperHitAirSetStatus */
    memset(&tip, 0, sizeof(tip));
    itNBumperHitAirSetStatus(&titem);
    CHECK(tip.item_vars.bumper.damage_all_delay == ITBUMPER_DAMAGE_ALL_WAIT);
    CHECK(tip.proc_update == itNBumperHitAirProcUpdate);
    CHECK(tip.proc_map == itNBumperThrownProcMap);

    /* itNBumperGDisappearProcUpdate: lifetime reaching 0 destroys the
     * item outright (TRUE); an odd lifetime toggles DOBJ_FLAG_HIDDEN this
     * frame, an even one leaves it alone -- both sides checked. */
    memset(&tip, 0, sizeof(tip));
    tip.lifetime = 0;
    r = itNBumperGDisappearProcUpdate(&titem);
    CHECK(r == TRUE);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.obj = &tdobj;
    tip.lifetime = 3;
    r = itNBumperGDisappearProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK(tdobj.flags == DOBJ_FLAG_HIDDEN);
    CHECK(tip.lifetime == 2);

    memset(&tip, 0, sizeof(tip));
    tdobj.flags = DOBJ_FLAG_NONE;
    titem.obj = &tdobj;
    tip.lifetime = 4;
    r = itNBumperGDisappearProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK(tdobj.flags == DOBJ_FLAG_NONE); /* even lifetime -- untouched this frame */
    CHECK(tip.lifetime == 3);

    /* itNBumperGDisappearSetStatus: the item's own permanent "dying"
     * reset -- palette/scale/flags/attack_state/velocity all cleared,
     * lifetime seeded to DESPAWN_TIMER. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.mobj = &tmobj;
    titem.obj = &tdobj;
    tmobj.palette_id = 1.0F;
    tdobj.scale.vec.f.x = tdobj.scale.vec.f.y = tdobj.scale.vec.f.z = 5.0F;
    tdobj.flags = DOBJ_FLAG_HIDDEN;
    tip.attack_coll.attack_state = nGMAttackStateNew;
    tip.physics.vel_air.x = tip.physics.vel_air.y = tip.physics.vel_air.z = 9.0F;
    itNBumperGDisappearSetStatus(&titem);
    CHECK_NEAR(tmobj.palette_id, 0.0F, 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.x, 1.0F, 1e-4F);
    CHECK(tip.lifetime == ITBUMPER_DESPAWN_TIMER);
    CHECK(tdobj.flags == DOBJ_FLAG_NONE);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK(tip.proc_update == itNBumperGDisappearProcUpdate);
}

/* GBumper, the first of the ten "stage items"
 * (dITManagerProcMakeList entries 22 to 31) to have a body, and the one
 * of the ten whose assets the port already shipped -- its
 * `ItemAttributes` is ITCommonData 0xcf0 and its model is ITCommonObject
 * 0x7648, both long since inside romdisk/itcommon.itp. It has no status
 * table at all: the desc's own proc_update/proc_hit ARE the whole of it,
 * which is what makes it 116 lines against the Normal Bumper's 600. */
extern ITDesc dITGBumperItemDesc;

static void test_it_gbumper(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    MObj tmobj;
    sb32 r;

    CHECK(dITGBumperItemDesc.kind == nITKindGBumper);
    CHECK(dITGBumperItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITGBumperItemDesc.o_attributes == (intptr_t)0xcf0);  /* reloc_data.us.h: llITCommonDataGBumperItemAttributes */
    CHECK(dITGBumperItemDesc.transform_types.tk1 == nGCMatrixKindTraRotRpyRSca);
    CHECK(dITGBumperItemDesc.transform_types.tk2 == nGCMatrixKindNull);
    CHECK(dITGBumperItemDesc.transform_types.tk3 == 0);
    /* nGMAttackStateNEW, not Off: a Bumper is not a thrown object, and
     * the state is what it starts in -- Star Man and every carried item
     * the switch can pick is `nGMAttackStateOff` */
    CHECK(dITGBumperItemDesc.attack_state == nGMAttackStateNew);
    CHECK(dITGBumperItemDesc.proc_update == itGBumperCommonProcUpdate);
    CHECK(dITGBumperItemDesc.proc_map == NULL);
    CHECK(dITGBumperItemDesc.proc_hit == itGBumperCommonProcHit);
    CHECK(dITGBumperItemDesc.proc_shield == NULL);
    CHECK(dITGBumperItemDesc.proc_hop == NULL);
    CHECK(dITGBumperItemDesc.proc_setoff == NULL);
    CHECK(dITGBumperItemDesc.proc_reflector == NULL);
    CHECK(dITGBumperItemDesc.proc_damage == NULL);

    /* itGBumperCommonProcHit: the hit itself -- hard 2.0 scale, the
     * second palette, a three-frame timer, and the ten-step spring-back
     * the update walks down. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.mobj = &tmobj;
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tdobj.scale.vec.f.z = 7.0F;
    r = itGBumperCommonProcHit(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tdobj.scale.vec.f.x, 2.0F, 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.y, 2.0F, 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.z, 7.0F, 1e-4F);   /* untouched: ProcHit writes x,y only */
    CHECK(tip.item_vars.bumper.hit_anim_length == ITBUMPER_HIT_ANIM_LENGTH);
    CHECK_NEAR(tmobj.palette_id, 1.0F, 1e-4F);
    CHECK(tip.multi == ITBUMPER_HIT_SCALE);

    /* itGBumperCommonProcUpdate, the palette arm: hit_anim_length still
     * running -> the ELSE branch decrements it and the palette stays.
     *
     * multi is set by hand for each call rather than inherited from the
     * ProcHit above: BOTH arms of this function run on every call, so
     * every ProcUpdate is one step of the spring-back and one palette
     * decision together, and the arithmetic below is only readable if
     * each call states its own input. */
    tip.item_vars.bumper.hit_anim_length = 3;
    tip.multi = ITBUMPER_HIT_SCALE;
    tmobj.palette_id = 1.0F;
    r = itGBumperCommonProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK(tip.item_vars.bumper.hit_anim_length == 2);
    CHECK_NEAR(tmobj.palette_id, 1.0F, 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.x, 2.0F - (10 - ITBUMPER_HIT_SCALE) * 0.1F, 1e-4F);
    CHECK(tip.multi == ITBUMPER_HIT_SCALE - 1);

    /* the timer running out is what clears the palette -- and the same
     * call takes the next step of the shrink. The z axis is never
     * written by this file: a Bumper squashes in the picture plane and
     * not in depth. */
    tip.item_vars.bumper.hit_anim_length = 0;
    tip.multi = 9;
    tmobj.palette_id = 1.0F;
    tdobj.scale.vec.f.z = 7.0F;
    r = itGBumperCommonProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tmobj.palette_id, 0.0F, 1e-4F);      /* timer ran out -> first palette */
    CHECK(tip.item_vars.bumper.hit_anim_length == 0);
    CHECK_NEAR(tdobj.scale.vec.f.x, 1.9F, 1e-4F);
    CHECK(tip.multi == 8);

    /* and the last step of it: multi 1 -> 1.1, multi 0 -> exactly 1.0. */
    tip.multi = 1;
    r = itGBumperCommonProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tdobj.scale.vec.f.x, 1.1F, 1e-4F);
    CHECK(tip.multi == 0);
    r = itGBumperCommonProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tdobj.scale.vec.f.x, 1.0F, 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.y, 1.0F, 1e-4F);
    CHECK_NEAR(tdobj.scale.vec.f.z, 7.0F, 1e-4F);

    /* A Bumper that was never hit takes the else arm every frame --
     * hit_anim_length is decremented from zero rather than tested
     * cleanly. That is the decomp's own if/else, and `hit_anim_length`
     * is a u16 (it/itvars.h:548), so the decrement WRAPS: 0 becomes
     * 65535, not -1. It is only ever read for equality with zero, so
     * nothing downstream can see it -- but the field's width is the
     * whole reason the decomp's own `!= -1` shapes elsewhere in it/
     * (Shell's u8 and this file's own u16 damage_all_delay) behave the
     * way they do, and this is the same trap in its simplest form. */
    tip.item_vars.bumper.hit_anim_length = 0;
    tip.multi = 0;
    tmobj.palette_id = 0.0F;
    r = itGBumperCommonProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK(tip.item_vars.bumper.hit_anim_length == (u16)-1);
    CHECK(tip.item_vars.bumper.hit_anim_length == 0xFFFF);

    /* itGBumperMakeItem, driven for real.
     * attr.data must be non-NULL for the model path to run, because that
     * is the branch that calls itemModelAddToGObj and therefore the one
     * that gives the DObj its MObj; the palette write below would land
     * past address zero without it. The value is never read -- only the
     * pack row (0xcf0) picks the model -- so a dummy suffices. */
    {
        static ITStruct pool[ITEM_ALLOC_MAX];
        ITStruct *save_free = gITManagerStructsAllocFree;
        void *save_common = gITManagerCommonData;
        ITAttributes attr;
        Vec3f pos = { -100.0F, 5.0F, 0.0F };
        Vec3f vel = { 999.0F, 999.0F, 999.0F };
        GObj *item_gobj;
        ITStruct *ip;
        DObj *dobj;
        int i;
        s32 save_gkind = gSCManagerBattleState->gkind;
        u32 save_weight;
        s32 save_angle;

        for (i = 0; i < ITEM_ALLOC_MAX - 1; i++) pool[i].next = &pool[i + 1];
        pool[ITEM_ALLOC_MAX - 1].next = NULL;
        gITManagerStructsAllocFree = pool;

        memset(&attr, 0, sizeof(attr));
        attr.data = (void*)1;   /* non-NULL opens the model path, see above */

        gITManagerCommonData = (void*)((uintptr_t)&attr -
                                       (uintptr_t)0xcf0);  /* reloc_data.us.h: llITCommonDataGBumperItemAttributes */

        /* Peach's Castle's own call, gr/grcommon/grcastle.c:57. */
        gSCManagerBattleState->gkind = nGRKindCastle;
        item_gobj = itGBumperMakeItem(NULL, &pos, &vel, ITEM_FLAG_PARENT_GROUND);
        CHECK(item_gobj != NULL);

        if (item_gobj != NULL)
        {
            ip = itGetStruct(item_gobj);
            dobj = DObjGetStruct(item_gobj);

            CHECK(ip->kind == nITKindGBumper);
            CHECK(ip->proc_update == itGBumperCommonProcUpdate);
            CHECK(ip->proc_map == NULL);
            CHECK(ip->proc_hit == itGBumperCommonProcHit);
            CHECK(ip->multi == 0);
            /* itMainClearOwnerStats, the first thing the tail calls */
            CHECK(ip->is_damage_all == TRUE);
            CHECK(ip->owner_gobj == NULL);
            CHECK(ip->team == ITEM_TEAM_DEFAULT);
            /* a stage item: fighters only, and the shield may re-hit it */
            CHECK(ip->attack_coll.interact_mask == GMHITCOLLISION_FLAG_FIGHTER);
            CHECK(ip->attack_coll.can_rehit_shield == TRUE);
            CHECK_NEAR(ip->physics.vel_air.x, 0.0F, 1e-4F);
            CHECK_NEAR(ip->physics.vel_air.y, 0.0F, 1e-4F);
            CHECK_NEAR(ip->physics.vel_air.z, 0.0F, 1e-4F);
            /* THE MObj: the child of the pre-eject tree carries it, and
             * the eject makes that child the root, so this is the DObj
             * the palette write above lands on. */
            CHECK(dobj->mobj != NULL);
            CHECK_NEAR(dobj->mobj->palette_id, 0.0F, 1e-4F);
            /* the Castle pair, and both numbers can ONLY have come from
             * this branch: the fake attr is zeroed, so itManagerMakeItem
             * copied 0/0 out of it and the tail overwrote them. */
            CHECK(ip->attack_coll.knockback_weight == ITBUMPER_CASTLE_KNOCKBACK);
            CHECK(ip->attack_coll.angle == ITBUMPER_CASTLE_ANGLE);

            save_weight = ip->attack_coll.knockback_weight;
            save_angle = ip->attack_coll.angle;

            gcEjectGObj(item_gobj);

            /* the same make on a stage that is NOT Peach's Castle leaves
             * the table's own (zeroed) values alone -- gr/grbonus/
             * grbonus3.c:32 makes the same kind, weaker. */
            gSCManagerBattleState->gkind = nGRKindZebes;
            item_gobj = itGBumperMakeItem(NULL, &pos, &vel, ITEM_FLAG_PARENT_GROUND);
            CHECK(item_gobj != NULL);

            if (item_gobj != NULL)
            {
                ip = itGetStruct(item_gobj);
                CHECK(ip->attack_coll.knockback_weight == 0);
                CHECK(ip->attack_coll.angle == 0);
                CHECK(save_weight == ITBUMPER_CASTLE_KNOCKBACK);
                CHECK(save_angle == ITBUMPER_CASTLE_ANGLE);
                gcEjectGObj(item_gobj);
            }
        }

        gSCManagerBattleState->gkind = save_gkind;
        gITManagerCommonData = save_common;
        gITManagerStructsAllocFree = save_free;
    }
}

/* ---- the `?` block --------------------------------------
 *
 * The first of the ten STAGE items to reach a stage, and the first item
 * whose attributes are not in ITCommonData: PowerBlock's table is
 * Mushroom Kingdom's own (`ItemAttributes PowerBlock`, 0xD8 of reloc
 * file 155), which is why the pack carries it under that same offset and
 * why the desc's `p_file` diverges to the item pack.
 */
extern ITDesc dITPowerBlockItemDesc;
extern ITStatusDesc dITPowerBlockStatusDescs[];

/* the item's own status enum, which the decomp keeps inside
 * it/itground/itpowerblock.c and this file names for the same reason
 * every other item test does: the table's row is the assertion. */
enum
{
    nITPowerBlockStatusWait
};

/* and the stage's, from gr/grcommon/grinishie.c:46-52 -- see
 * test_gr_inishie below, whose `?` block machine these are the states of */
enum
{
    kInishiePowerBlockWait,
    kInishiePowerBlockMake,
    kInishiePowerBlockSleep,
    kInishiePowerBlockDamage
};

static void test_it_powerblock(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    sb32 r;

    CHECK(dITPowerBlockItemDesc.kind == nITKindPowerBlock);
    CHECK(dITPowerBlockItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITPowerBlockItemDesc.o_attributes == (intptr_t)0xD8);
    /* THE DECOMP'S OWN 0x44, which is a bare number rather than a
     * nGCMatrixKind. Kept as written -- see the item file's header. */
    CHECK(dITPowerBlockItemDesc.transform_types.tk1 == 0x44);
    CHECK(dITPowerBlockItemDesc.transform_types.tk2 == nGCMatrixKindNull);
    CHECK(dITPowerBlockItemDesc.attack_state == nGMAttackStateOff);
    CHECK(dITPowerBlockItemDesc.proc_update == itPowerBlockCommonProcUpdate);
    CHECK(dITPowerBlockItemDesc.proc_map == NULL);
    CHECK(dITPowerBlockItemDesc.proc_hit == NULL);
    /* the item has ONE status, and the block's damaged animation is not
     * it: `proc_damage` is where the hit lands */
    CHECK(dITPowerBlockItemDesc.proc_damage == NULL);
    CHECK(dITPowerBlockStatusDescs[nITPowerBlockStatusWait].proc_damage ==
          itPowerBlockWaitProcDamage);
    CHECK(dITPowerBlockStatusDescs[nITPowerBlockStatusWait].proc_update ==
          NULL);

    /* itPowerBlockCommonProcUpdate: the block is not ready until its
     * idle animation has run out -- anim_wait AOBJ_ANIM_NULL is what
     * "run out" is (sys/objtypes.h:41) */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tdobj.anim_wait = 0.0F;
    tip.damage_coll.hitstatus = nGMHitStatusNone;
    r = itPowerBlockCommonProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone);   /* untouched */

    tdobj.anim_wait = AOBJ_ANIM_NULL;
    r = itPowerBlockCommonProcUpdate(&titem);
    CHECK(r == FALSE);
    /* itPowerBlockWaitSetStatus ran: the status table's row is now the
     * item's, and a hit is allowed */
    CHECK(tip.proc_damage == itPowerBlockWaitProcDamage);
    CHECK(tip.proc_update == NULL);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
    CHECK(tip.is_thrown == FALSE);

    /* itPowerBlockNDamageProcUpdate: the item destroys itself (TRUE)
     * only once the damage animation has played out, and the STAGE is
     * told first -- which is what arms the next block */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.obj = &tdobj;
    tdobj.anim_wait = 1.0F;
    gGRCommonStruct.inishie.pblock_status = kInishiePowerBlockDamage;
    gGRCommonStruct.inishie.pblock_appear_wait = 2;
    r = itPowerBlockNDamageProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK(gGRCommonStruct.inishie.pblock_appear_wait == 2);  /* untouched */

    tdobj.anim_wait = AOBJ_ANIM_NULL;
    r = itPowerBlockNDamageProcUpdate(&titem);
    CHECK(r == TRUE);
    CHECK(gGRCommonStruct.inishie.pblock_status ==
          kInishiePowerBlockMake);
    CHECK(gGRCommonStruct.inishie.pblock_appear_wait == 1800);

    /* and the model: the port's own bake, keyed by the same 0xD8 the
     * desc carries. Two joints -- the block and the NULL link the
     * DObjDesc's third entry names -- and no MObj, because PowerBlock's
     * table names no MObjSub. */
    {
        GObj gobj;

        memset(&gobj, 0, sizeof(gobj));
        CHECK(itemModelAddToGObj(&gobj, 0xD8) == 2);
        CHECK(DObjGetStruct(&gobj) != NULL);
        CHECK(DObjGetStruct(&gobj)->child != NULL);
        CHECK(DObjGetStruct(&gobj)->child->mobj == NULL);
    }
}

/* ---- the Piranha Plant ----------------------------------
 *
 * Mushroom Kingdom's other hazard, and the second item whose attributes
 * live in the stage's map file (0x120). It is the first that carries an
 * MObj -- `p_mobjsubs` is mobjlink_0x0A68, two MObjSubs -- which is what
 * `dobj->mobj` in three of its functions is, and the first that hangs a
 * MatAnimJoint on that MObj to swap its palette.
 */
extern ITDesc dITPakkunItemDesc;
extern ITStatusDesc dITPakkunStatusDescs[];

enum
{
    kITPakkunWait,
    kITPakkunAppear,
    kITPakkunDamaged
};

static void test_it_pakkun(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    XObj txobj;
    sb32 r;

    CHECK(dITPakkunItemDesc.kind == nITKindPakkun);
    CHECK(dITPakkunItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITPakkunItemDesc.o_attributes == (intptr_t)0x120);
    CHECK(dITPakkunItemDesc.transform_types.tk1 == nGCMatrixKindTra);
    CHECK(dITPakkunItemDesc.transform_types.tk2 == 0x30);
    CHECK(dITPakkunItemDesc.attack_state == nGMAttackStateOff);
    CHECK(dITPakkunItemDesc.proc_update == itPakkunWaitProcUpdate);
    CHECK(dITPakkunItemDesc.proc_map == NULL);
    CHECK(dITPakkunItemDesc.proc_damage == NULL);

    CHECK(dITPakkunStatusDescs[kITPakkunWait].proc_update ==
          itPakkunWaitProcUpdate);
    CHECK(dITPakkunStatusDescs[kITPakkunWait].proc_damage == NULL);
    CHECK(dITPakkunStatusDescs[kITPakkunAppear].proc_update ==
          itPakkunAppearProcUpdate);
    CHECK(dITPakkunStatusDescs[kITPakkunAppear].proc_damage ==
          itPakkunAppearProcDamage);
    CHECK(dITPakkunStatusDescs[kITPakkunDamaged].proc_update ==
          itPakkunDamagedProcUpdate);
    CHECK(dITPakkunStatusDescs[kITPakkunDamaged].proc_damage == NULL);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tip.item_vars.pakkun.pos.y = 512.0F;

    /* itPakkunWaitInitVars: the plant is down the pipe, unhittable, and
     * its height is its own spawn's y */
    tdobj.translate.vec.f.y = 9999.0F;
    itPakkunWaitInitVars(&titem);
    CHECK(tip.multi == ITPAKKUN_APPEAR_WAIT);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK_NEAR(tdobj.translate.vec.f.y, 512.0F, 1e-4F);
    CHECK(tip.proc_update == itPakkunWaitProcUpdate);   /* the Wait row */
    CHECK(tip.proc_dead == NULL);

    /* itPakkunWaitProcUpdate: the countdown, and on zero the plant comes
     * up -- by its own spawn height, which is what the appear animation
     * and the growing hurtbox are both measured against. NO FIGHTERS are
     * linked on the host, so itPakkunCommonCheckNoFighter says yes. */
    tip.multi = 1;
    r = itPakkunWaitProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tdobj.translate.vec.f.y, 512.0F + 512.0F, 1e-4F);
    CHECK(tip.proc_update == itPakkunAppearProcUpdate);   /* the Appear row */
    CHECK(tip.proc_damage == itPakkunAppearProcDamage);

    /* and the stage's own flag re-arms the countdown instead */
    tip.multi = 1;
    tip.item_vars.pakkun.is_wait_fighter = TRUE;
    tdobj.translate.vec.f.y = 512.0F;
    r = itPakkunWaitProcUpdate(&titem);
    CHECK(r == FALSE);
    /* ITPAKKUN_APPEAR_WAIT MINUS ONE: the flag re-arms `multi` and the
     * SAME call then decrements it, exactly as the decomp's two
     * statements do -- so a plant the stage has just interrupted waits
     * one tic less than a fresh one */
    CHECK(tip.multi == ITPAKKUN_APPEAR_WAIT - 1);
    CHECK(tip.item_vars.pakkun.is_wait_fighter == FALSE);
    CHECK_NEAR(tdobj.translate.vec.f.y, 512.0F, 1e-4F);   /* not raised */

    /* itPakkunAppearUpdateDamageColl: below the clamp the plant has no
     * hurtbox at all; above it, the box is proportional to how far out
     * it has come and its offset follows, so the hittable part IS the
     * part of the plant that is out of the pipe */
    tip.damage_coll.hitstatus = nGMHitStatusNormal;
    tdobj.translate.vec.f.y = 512.0F;   /* pos_y 0 -> off_y is the offset */
    itPakkunAppearUpdateDamageColl(&titem);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);

    tip.damage_coll.hitstatus = nGMHitStatusNormal;
    tdobj.translate.vec.f.y = 512.0F + 512.0F;   /* pos_y 512 */
    itPakkunAppearUpdateDamageColl(&titem);
    {
        f32 pos_y = 512.0F;
        f32 off_y = pos_y + ITPAKKUN_APPEAR_OFF_Y;

        CHECK(tip.damage_coll.size.y ==
              (off_y - ITPAKKUN_CLAMP_OFF_Y) * ITPAKKUN_HURT_SIZE_MUL_Y);
        CHECK(tip.damage_coll.offset.y ==
              (tip.damage_coll.size.y + ITPAKKUN_CLAMP_OFF_Y) - pos_y);
    }

    /* itPakkunAppearProcDamage: a hit over the threshold knocks the
     * plant out -- upside down, along the damage angle, with the
     * damaged palette and the Damaged status (which is the only place
     * proc_dead is set). */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&txobj, 0, sizeof(txobj));
    tdobj.xobjs[1] = &txobj;
    titem.obj = &tdobj;
    tip.damage_knockback = ITPAKKUN_NDAMAGE_KNOCKBACK_MIN - 1;
    r = itPakkunAppearProcDamage(&titem);
    CHECK(r == FALSE);
    CHECK(tip.proc_update == NULL);   /* untouched: under the threshold */
    CHECK(txobj.kind == 0);

    tip.damage_knockback = ITPAKKUN_NDAMAGE_KNOCKBACK_MIN;
    tip.damage_lr = 1;
    tip.ga = nMPKineticsAir;
    r = itPakkunAppearProcDamage(&titem);
    CHECK(r == FALSE);
    CHECK(txobj.kind == 0x46);      /* the decomp's own XObj kind */
    CHECK_NEAR(tdobj.rotate.vec.f.z, F_CST_DTOR32(180.0F), 1e-3F);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK(tip.proc_update == itPakkunDamagedProcUpdate);
    CHECK(tip.proc_dead == itPakkunDamagedProcDead);
    CHECK(tdobj.anim_wait == AOBJ_ANIM_NULL);
    /* the knockback is along the angle the FIGHTER system gives, and the
     * plant's own facing flips only the x -- `-ip->damage_lr`. Checked
     * by running the same hit twice, once each way: the y is the same
     * number and the x is its negative. */
    {
        f32 x_lr1 = tip.physics.vel_air.x, y_lr1 = tip.physics.vel_air.y;

        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        memset(&txobj, 0, sizeof(txobj));
        tdobj.xobjs[1] = &txobj;
        titem.obj = &tdobj;
        tip.damage_knockback = ITPAKKUN_NDAMAGE_KNOCKBACK_MIN;
        tip.damage_lr = -1;
        tip.ga = nMPKineticsAir;
        r = itPakkunAppearProcDamage(&titem);
        CHECK(r == FALSE);
        CHECK_NEAR(tip.physics.vel_air.x, -x_lr1, 1e-3F);
        CHECK_NEAR(tip.physics.vel_air.y, y_lr1, 1e-3F);
        /* and the pair is a UNIT vector scaled by the knockback -- true
         * for whatever angle ftCommonDamageGetKnockbackAngle returns,
         * which for this input (damage_angle 0, in the air) is 0, so the
         * y really is zero and the x carries the whole hit */
        CHECK_NEAR(tip.physics.vel_air.x * tip.physics.vel_air.x +
                   tip.physics.vel_air.y * tip.physics.vel_air.y,
                   tip.damage_knockback * tip.damage_knockback, 1e-2F);
    }

    /* itPakkunDamagedProcUpdate: gravity, and nothing else */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.y = 0.0F;
    r = itPakkunDamagedProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITPAKKUN_GRAVITY, 1e-4F);

    /* itPakkunDamagedProcDead: the engine's proc_dead. A plant is not
     * destroyed, it is recycled -- home, upright, still, back to Wait. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    {
        static MObj tmobj;
        memset(&tmobj, 0, sizeof(tmobj));
        tdobj.mobj = &tmobj;
        tdobj.rotate.vec.f.z = 3.0F;
        titem.obj = &tdobj;
        tip.item_vars.pakkun.pos.x = 100.0F;
        tip.item_vars.pakkun.pos.y = 200.0F;
        tip.item_vars.pakkun.pos.z = 300.0F;
        tip.item_vars.pakkun.is_wait_fighter = TRUE;
        tip.physics.vel_air.x = tip.physics.vel_air.y =
            tip.physics.vel_air.z = 9.0F;

        r = itPakkunDamagedProcDead(&titem);
        CHECK(r == FALSE);
        CHECK_NEAR(tdobj.translate.vec.f.x, 100.0F, 1e-4F);
        CHECK_NEAR(tdobj.translate.vec.f.y, 200.0F, 1e-4F);
        CHECK_NEAR(tdobj.translate.vec.f.z, 300.0F, 1e-4F);
        CHECK(tip.multi == ITPAKKUN_REBIRTH_WAIT);
        CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
        CHECK_NEAR(tdobj.rotate.vec.f.z, 0.0F, 1e-4F);
        CHECK(tmobj.anim_wait == AOBJ_ANIM_NULL);
        CHECK(tip.proc_update == itPakkunWaitProcUpdate);
        CHECK(tip.proc_dead == NULL);
        CHECK(tip.item_vars.pakkun.is_wait_fighter == FALSE);
    }

    /* itPakkunCommonCheckNoFighter with nobody at all: TRUE. The link
     * is emptied first -- earlier tests in this file leave fighters on
     * it -- and a NULL GObj is the decomp's own "no plant" arm. */
    {
        GObj *save_link = gGCCommonLinks[nGCCommonLinkIDFighter];

        gGCCommonLinks[nGCCommonLinkIDFighter] = NULL;
        CHECK(itPakkunCommonCheckNoFighter(NULL) == TRUE);
        CHECK(itPakkunCommonCheckNoFighter(&titem) == TRUE);
        gGCCommonLinks[nGCCommonLinkIDFighter] = save_link;
    }

    /* and with a fighter standing in the box, FALSE -- the box is around
     * the plant's SPAWN, measured against the fighter's TopN joint */
    {
        GObj tfighter;
        FTStruct tfp;
        GObj *saved_link = gGCCommonLinks[nGCCommonLinkIDFighter];

        static DObj tjoints[FTPARTS_JOINT_NUM_MAX];
        int tj;

        memset(&tfighter, 0, sizeof(tfighter));
        memset(&tfp, 0, sizeof(tfp));
        memset(tjoints, 0, sizeof(tjoints));
        tfighter.user_data.p = &tfp;
        /* `joints` is an ARRAY of POINTERS inside FTStruct, so the test
         * gives every slot one of its own rather than pointing the array
         * elsewhere */
        for (tj = 0; tj < FTPARTS_JOINT_NUM_MAX; tj++)
        {
            tfp.joints[tj] = &tjoints[tj];
        }
        tip.item_vars.pakkun.pos.x = 0.0F;
        tip.item_vars.pakkun.pos.y = 0.0F;
        tfp.joints[nFTPartsJointTopN]->translate.vec.f.x =
            ITPAKKUN_DETECT_SIZE_WIDTH / 2.0F;
        tfp.joints[nFTPartsJointTopN]->translate.vec.f.y =
            (ITPAKKUN_DETECT_SIZE_BOTTOM + ITPAKKUN_DETECT_SIZE_TOP) / 2.0F;
        gGCCommonLinks[nGCCommonLinkIDFighter] = &tfighter;
        CHECK(itPakkunCommonCheckNoFighter(&titem) == FALSE);

        /* one unit outside the box horizontally: nobody there */
        tfp.joints[nFTPartsJointTopN]->translate.vec.f.x =
            ITPAKKUN_DETECT_SIZE_WIDTH + 1.0F;
        CHECK(itPakkunCommonCheckNoFighter(&titem) == TRUE);

        gGCCommonLinks[nGCCommonLinkIDFighter] = saved_link;
    }

    /* the model: two joints. The DObjDesc has three entries, but the
     * third is the NULL link every item's tree ends on and the bake
     * drops it -- the same count PowerBlock's three-entry tree gives.
     * What is DIFFERENT is the MObj: Pakkun's table names an MObjSub
     * chain (mobjlink_0x0A68) where PowerBlock's names none, and
     * `dobj->mobj` is what three of this item's functions write
     * through. */
    {
        GObj gobj;

        memset(&gobj, 0, sizeof(gobj));
        CHECK(itemModelAddToGObj(&gobj, 0x120) == 2);
        CHECK(DObjGetStruct(&gobj) != NULL);
        CHECK(DObjGetStruct(&gobj)->child != NULL);
        CHECK(DObjGetStruct(&gobj)->child->mobj != NULL);
    }
}

/* ---- Saffron City's Chansey -----------------------------
 *
 * The first of the Gate's five, and the first stage item that is a FULL
 * monster: it walks, it lays EGGS (real items, through the roster entry
 * the port has), and the ITEM SWITCH decides whether
 * it lays any.
 */
extern ITDesc dITGLuckyItemDesc;
extern ITStatusDesc dITGLuckyStatusDescs[];

enum
{
    kITGLuckyDamaged
};

/* gr/grcommon/gryamabuki.c:32-37, the Gate's three states -- the other
 * half of this item, which see test_gr_yamabuki */
enum
{
    kYamabukiGateSleep,
    kYamabukiGateWait,
    kYamabukiGateOpen
};

static void test_it_glucky(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    sb32 r;
    u32 saved_toggles = mock_battle.item_toggles;
    s32 saved_rate = mock_battle.item_appearance_rate;

    CHECK(dITGLuckyItemDesc.kind == nITKindGLucky);
    CHECK(dITGLuckyItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITGLuckyItemDesc.o_attributes == (intptr_t)0xBC);
    CHECK(dITGLuckyItemDesc.transform_types.tk1 == nGCMatrixKindTraRotRpyR);
    /* nGMAttackStateNew -- a monster's hitbox is live from the moment it
     * appears, unlike the stage furniture's */
    CHECK(dITGLuckyItemDesc.attack_state == nGMAttackStateNew);
    CHECK(dITGLuckyItemDesc.proc_update == itGLuckyCommonProcUpdate);
    CHECK(dITGLuckyItemDesc.proc_hit == itGLuckyCommonProcHit);
    CHECK(dITGLuckyItemDesc.proc_damage == itGLuckyCommonProcDamage);
    CHECK(dITGLuckyItemDesc.proc_map == NULL);

    /* ONE status, the knocked-out one */
    CHECK(dITGLuckyStatusDescs[kITGLuckyDamaged].proc_update ==
          itGLuckyDamagedProcUpdate);
    CHECK(dITGLuckyStatusDescs[kITGLuckyDamaged].proc_damage == NULL);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tip.item_vars.glucky.pos.x = 3.0F;
    tip.item_vars.glucky.pos.y = -2.0F;

    /* itGLuckyCommonProcUpdate: the WALK is the spawn vector added every
     * tic, and outside the egg interval nothing else happens. anim_wait
     * is left running (not AOBJ_ANIM_NULL), so the item lives. */
    tdobj.anim_frame = 0.0F;
    tdobj.anim_wait = 1.0F;
    r = itGLuckyCommonProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tdobj.translate.vec.f.x, 3.0F, 1e-4F);
    CHECK_NEAR(tdobj.translate.vec.f.y, -2.0F, 1e-4F);

    /* the EGG INTERVAL with the switch OFF: the count still comes down
     * and the ten-tick delay still starts -- a Chansey in a match with
     * Eggs off walks out empty, it does not stop walking */
    tip.item_vars.glucky.egg_spawn_count = 6;
    tip.multi = 0;
    tdobj.anim_frame = ITGLUCKY_EGG_SPAWN_BEGIN;
    mock_battle.item_toggles = 0;
    mock_battle.item_appearance_rate = nSCBattleItemSwitchLow;
    r = itGLuckyCommonProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK(tip.item_vars.glucky.egg_spawn_count == 5);
    CHECK(tip.multi == 9);      /* set to 10, then decremented in the same call */
    CHECK_NEAR(tdobj.translate.vec.f.x, 6.0F, 1e-4F);

    /* and a second tic inside the interval does NOT lay -- the multi
     * delay is what spaces them */
    tip.item_vars.glucky.egg_spawn_count = 5;
    r = itGLuckyCommonProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK(tip.item_vars.glucky.egg_spawn_count == 5);

    /* the END of the animation is what closes the gate and destroys the
     * item (TRUE) -- and it is the STAGE's own function that is called */
    tdobj.anim_wait = AOBJ_ANIM_NULL;
    tdobj.anim_frame = 0.0F;
    gGRCommonStruct.yamabuki.gate_status = 2;   /* Open */
    gGRCommonStruct.yamabuki.gate_wait = 0;
    r = itGLuckyCommonProcUpdate(&titem);
    CHECK(r == TRUE);
    CHECK(gGRCommonStruct.yamabuki.gate_status == kYamabukiGateWait);
    CHECK(gGRCommonStruct.yamabuki.gate_wait == 1000);

    /* itGLuckyCommonProcHit: the hitbox goes off, nothing else */
    memset(&tip, 0, sizeof(tip));
    tip.attack_coll.attack_state = nGMAttackStateNew;
    r = itGLuckyCommonProcHit(&titem);
    CHECK(r == FALSE);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);

    /* itGLuckyDamagedProcUpdate: gravity and a spin whose DIRECTION is
     * the item's facing */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.obj = &tdobj;
    tip.lr = 1;
    r = itGLuckyDamagedProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITGLUCKY_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, -ITGLUCKY_HIT_ROTATE_Z, 1e-3F);
    tip.lr = -1;
    r = itGLuckyDamagedProcUpdate(&titem);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 0.0F, 1e-3F);

    /* itGLuckyDamagedProcDead is one line: TRUE. A Chansey does NOT
     * recycle the way a Piranha Plant does. */
    CHECK(itGLuckyDamagedProcDead(&titem) == TRUE);

    /* itGLuckyCommonProcDamage: under the threshold nothing happens;
     * over it the gate is told it has no monster, and the item is
     * thrown and enters its damaged status (which is the only place
     * proc_dead is set) */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.obj = &tdobj;
    gGRCommonStruct.yamabuki.monster_gobj = &titem;
    tip.damage_knockback = ITGLUCKY_NDAMAGE_KNOCKBACK_MIN - 1.0F;
    r = itGLuckyCommonProcDamage(&titem);
    CHECK(r == FALSE);
    CHECK(gGRCommonStruct.yamabuki.monster_gobj == &titem);

    tip.damage_knockback = ITGLUCKY_NDAMAGE_KNOCKBACK_MIN;
    tip.damage_lr = 1;
    tip.ga = nMPKineticsAir;
    tip.attack_coll.attack_state = nGMAttackStateNew;
    r = itGLuckyCommonProcDamage(&titem);
    CHECK(r == FALSE);
    CHECK(gGRCommonStruct.yamabuki.monster_gobj == NULL);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone);
    CHECK(tdobj.anim_wait == AOBJ_ANIM_NULL);
    CHECK(tip.proc_update == itGLuckyDamagedProcUpdate);
    CHECK(tip.proc_dead == itGLuckyDamagedProcDead);
    CHECK_NEAR(tip.physics.vel_air.x * tip.physics.vel_air.x +
               tip.physics.vel_air.y * tip.physics.vel_air.y,
               tip.damage_knockback * tip.damage_knockback, 1e-2F);

    /* the model: two joints -- the DObjDesc has three and the NULL link
     * is dropped -- and NO MObj, because GLucky's table names no
     * MObjSub chain */
    {
        GObj gobj;

        memset(&gobj, 0, sizeof(gobj));
        CHECK(itemModelAddToGObj(&gobj, 0xBC) == 2);
        CHECK(DObjGetStruct(&gobj) != NULL);
        CHECK(DObjGetStruct(&gobj)->mobj == NULL);
    }

    mock_battle.item_toggles = saved_toggles;
    mock_battle.item_appearance_rate = saved_rate;
    gGRCommonStruct.yamabuki.monster_gobj = NULL;
}

extern ITDesc dITPorygonItemDesc;

/* The effect pool's own counter, which test_it_porygon reads to see the
 * shake-stop dust actually allocated. ef_pool_reset is defined further
 * down and already forward-declared for the tests between here and
 * there; this is the same declaration, one function earlier. */
extern s32 sEFManagerStructsFreeNum;
static void ef_pool_reset(void);

static void test_it_porygon(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    ITItemEvent ev;
    sb32 r;
    /* This test runs BEFORE test_item_pack in main's list, and that one
     * asserts the pack is NOT loaded when it starts -- so if this is what
     * loads it, this is what gives it back. */
    sb32 loaded_here = 0;

    if (itemPackLoaded() == 0)
    {
        CHECK(itemPackLoad("itcommon.itp") == 0);
        loaded_here = 1;
    }

    CHECK(dITPorygonItemDesc.kind == nITKindPorygon);
    CHECK(dITPorygonItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITPorygonItemDesc.o_attributes == (intptr_t)0x16C);
    CHECK(dITPorygonItemDesc.transform_types.tk1 == nGCMatrixKindTraRotRpyR);
    /* nGMAttackStateNew -- like Chansey's, a Gate monster's hitbox is live
     * from the moment it appears */
    CHECK(dITPorygonItemDesc.attack_state == nGMAttackStateNew);
    CHECK(dITPorygonItemDesc.proc_update == itPorygonCommonProcUpdate);
    CHECK(dITPorygonItemDesc.proc_hit == NULL);
    CHECK(dITPorygonItemDesc.proc_damage == NULL);
    CHECK(dITPorygonItemDesc.proc_map == NULL);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;

    /* ---- the EVENT MACHINE ----------------------------------------
     *
     * The table is the pack's, read through the same accessor the item
     * uses; these are the decomp's numbers (dGRYamabukiMap_Porygon_
     * HitParties, 264_GRYamabukiMap.c:228) read back for the comparison,
     * so a wrong key or a wrong section offset fails HERE rather than as
     * a wrong hitbox in a match. */
    CHECK(itemPackLoaded() == 1);
    CHECK(itemPackMonsterEvent((intptr_t)0x16C, 0, &ev) == 0);
    CHECK(ev.timer == 0);

    /* The item is spawned with multi 0 and event_id 0, so the FIRST tic
     * already matches event 0's timer of 0: its numbers are installed and
     * event_id goes to 1. This is the decomp's own shape -- the `if` runs
     * before the `multi++`, so an event whose timer is 0 fires on the tic
     * multi is 0. */
    tip.multi = 0;
    tip.event_id = 0;
    tip.item_vars.porygon.offset.x = 1.5F;
    tip.item_vars.porygon.offset.y = -0.5F;
    itPorygonCommonUpdateMonsterEvent(&titem);
    CHECK(tip.event_id == 1);
    CHECK(tip.multi == 1);
    CHECK(tip.attack_coll.angle == 40);
    CHECK(tip.attack_coll.damage == 18);
    CHECK_NEAR(tip.attack_coll.size, 300.0F, 1e-4F);
    CHECK_NEAR(tip.attack_coll.knockback_scale, 40.0F, 1e-4F);
    CHECK(tip.attack_coll.knockback_weight == 0);
    CHECK(tip.attack_coll.knockback_base == 70);
    CHECK(tip.attack_coll.element == 0);
    CHECK(tip.attack_coll.can_setoff == 0);
    CHECK(tip.attack_coll.shield_damage == 0);
    /* the event's own sound, by name (264_GRYamabukiMap.c:228); the
     * exporter wrote every FGM name as 0 until the G05 sweep */
    CHECK(tip.attack_coll.fgm_id == nSYAudioFGMPunchL);

    /* and until `multi` reaches the NEXT event's timer (8) nothing more
     * is installed -- the hitbox sits at event 0's numbers for seven more
     * tics, which is what the walk looks like */
    tip.multi = 7;
    itPorygonCommonUpdateMonsterEvent(&titem);
    CHECK(tip.event_id == 1);
    CHECK(tip.multi == 8);
    CHECK(tip.attack_coll.damage == 18);

    /* the eighth tic: event 1's numbers, and event_id goes to 2 -- which
     * the decomp's own wrap takes straight back to 1, so the pair
     * alternates for the rest of the item's life */
    itPorygonCommonUpdateMonsterEvent(&titem);
    CHECK(tip.event_id == 1);
    CHECK(tip.multi == 9);
    CHECK(tip.attack_coll.damage == 8);
    CHECK(tip.attack_coll.knockback_scale == 70);
    CHECK(tip.attack_coll.knockback_base == 40);
    CHECK_NEAR(tip.attack_coll.size, 300.0F, 1e-4F);

    /* ---- the SHAKE-STOP DUST, at multi == ITPORYGON_SHAKE_STOP_WAIT ---
     *
     * `efManagerDustLightMakeEffect` at the item's own x/z with y ZEROED
     * -- the floor it stands on, not its raised height -- and this is the
     * one effect the whole item makes.
     *
     * WHAT THIS CAN AND CANNOT SEE. The bank is forced to -1, the "no
     * particle data" net every other effect test in this file uses, and
     * that makes lbParticleMakeScriptID answer NULL for the dust's
     * script: efManagerDustLightMakeEffect allocates an EFStruct, fails
     * to find the particle, and hands the struct straight back through
     * efManagerDestroyParticleGObj. So the pool's own count is BALANCED
     * across the call, and that balance is the assertion -- it is the
     * observable fact of the tic having reached the branch at all with
     * the effect machinery intact, and it is exactly the net-zero a bank
     * of nought scripts produces. The bank-loaded path (a real dust
     * particle, a transform, a lifetime) is what the twister scaffolding
     * further down exercises. */
    {
        s32 saved_bank = gEFManagerParticleBankID;
        s32 free_before;

        ef_pool_reset();
        gEFManagerParticleBankID = -1;

        memset(&tdobj, 0, sizeof(tdobj));
        tdobj.translate.vec.f.x = 10.0F;
        tdobj.translate.vec.f.y = 500.0F;
        tdobj.translate.vec.f.z = -20.0F;
        titem.obj = &tdobj;

        free_before = sEFManagerStructsFreeNum;
        tip.multi = ITPORYGON_SHAKE_STOP_WAIT - 1;
        itPorygonCommonUpdateMonsterEvent(&titem);
        CHECK(tip.multi == ITPORYGON_SHAKE_STOP_WAIT);
        CHECK(sEFManagerStructsFreeNum == free_before);

        gEFManagerParticleBankID = saved_bank;
    }

    /* ---- the walk, and the end of the animation --------------------
     *
     * ProcUpdate adds the SPAWN VECTOR every tic (the appear script's own
     * TraX is the shape of the walk and this is the speed), and the end
     * of the animation closes the gate and destroys the item. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.obj = &tdobj;
    tip.item_vars.porygon.offset.x = 2.0F;
    tip.item_vars.porygon.offset.y = 0.25F;
    tip.multi = 100;      /* past both events, so no hitbox churn */
    tip.event_id = 1;
    tdobj.anim_wait = 1.0F;
    r = itPorygonCommonProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tdobj.translate.vec.f.x, 2.0F, 1e-4F);
    CHECK_NEAR(tdobj.translate.vec.f.y, 0.25F, 1e-4F);

    tdobj.anim_wait = AOBJ_ANIM_NULL;
    gGRCommonStruct.yamabuki.gate_status = 2;   /* Open */
    gGRCommonStruct.yamabuki.gate_wait = 0;
    r = itPorygonCommonProcUpdate(&titem);
    CHECK(r == TRUE);
    CHECK(gGRCommonStruct.yamabuki.gate_status == kYamabukiGateWait);
    CHECK(gGRCommonStruct.yamabuki.gate_wait == 1000);

    /* ---- the model -------------------------------------------------
     *
     * Porygon's DObjDesc is at file 159 + 0x0EA0, three entries -- a root
     * with no display list, the body, and the link-18 terminator the bake
     * drops -- and its table names NO MObjSub chain (p_mobjsubs is NULL
     * where Chansey's names one), so the baked tree is two joints with no
     * MObj on either. */
    {
        GObj gobj;

        memset(&gobj, 0, sizeof(gobj));
        CHECK(itemModelAddToGObj(&gobj, 0x16C) == 2);
        CHECK(DObjGetStruct(&gobj) != NULL);
        CHECK(DObjGetStruct(&gobj)->mobj == NULL);
    }

    gGRCommonStruct.yamabuki.monster_gobj = NULL;

    if (loaded_here)
    {
        itemPackRelease();
    }
}

extern ITDesc dITMarumineItemDesc;
extern ITStatusDesc dITMarumineStatusDescs[];

/* The Electrode: the second of Saffron City's five, and the first item
 * here whose EXPLOSION reads a scripted hitbox table -- the ATTACK EVENTS
 * section, one size down from Porygon's monster events and
 * read through its own accessor. */
static void test_it_marumine(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    sb32 r;
    sb32 loaded_here = 0;

    if (itemPackLoaded() == 0)
    {
        CHECK(itemPackLoad("itcommon.itp") == 0);
        loaded_here = 1;
    }

    CHECK(dITMarumineItemDesc.kind == nITKindMarumine);
    CHECK(dITMarumineItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITMarumineItemDesc.o_attributes == (intptr_t)0x104);
    /* nGCMatrixKindTra -- an Electrode ROLLS rather than turns, and the
     * roll is in its appear script's own RotZ */
    CHECK(dITMarumineItemDesc.transform_types.tk1 == nGCMatrixKindTra);
    /* nGMAttackStateOff: its hitbox is dark until it explodes */
    CHECK(dITMarumineItemDesc.attack_state == nGMAttackStateOff);
    CHECK(dITMarumineItemDesc.proc_update == itMarumineCommonProcUpdate);
    CHECK(dITMarumineItemDesc.proc_hit == NULL);
    CHECK(dITMarumineItemDesc.proc_damage == NULL);
    CHECK(dITMarumineItemDesc.proc_map == NULL);

    /* ONE status, the exploding one */
    CHECK(dITMarumineStatusDescs[0].proc_update ==
          itMarumineExplodeProcUpdate);
    CHECK(dITMarumineStatusDescs[0].proc_damage == NULL);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tip.item_vars.marumine.offset.x = 4.0F;
    tip.item_vars.marumine.offset.y = -1.0F;

    /* itMarumineCommonProcUpdate: the ROLL is the spawn vector added
     * every tic, and while the appear script is still running nothing
     * else happens at all -- anim_wait is left running, so FALSE. */
    tdobj.anim_wait = 1.0F;
    r = itMarumineCommonProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tdobj.translate.vec.f.x, 4.0F, 1e-4F);
    CHECK_NEAR(tdobj.translate.vec.f.y, -1.0F, 1e-4F);
    CHECK_NEAR(tip.item_vars.marumine.offset.x, 4.0F, 1e-4F);

    /* ---- the ATTACK EVENT MACHINE, driven on its own ----------------
     *
     * The table is the pack's, read through the same accessor the item
     * uses; these are the decomp's numbers (dGRYamabukiMap_Marumine_
     * AttackEvents, 264_GRYamabukiMap.c:179). Entering the state sets
     * multi and event_id to 0 and applies event 0 IN THE SAME CALL, so
     * the first hitbox is installed on tic 0 rather than one tic later. */
    CHECK(itemPackLoaded() == 1);

    memset(&tip, 0, sizeof(tip));
    itMarumineExplodeSetStatus(&titem);
    CHECK(tip.multi == 0);
    CHECK(tip.event_id == 1);
    CHECK_NEAR(tip.attack_coll.throw_mul, 1.0F, 1e-4F);
    CHECK(tip.attack_coll.angle == 361);
    CHECK(tip.attack_coll.damage == 30);
    CHECK_NEAR(tip.attack_coll.size, 700.0F, 1e-4F);
    CHECK(tip.attack_coll.can_reflect == FALSE);
    CHECK(tip.attack_coll.can_shield == FALSE);
    CHECK(tip.attack_coll.element == nGMHitElementFire);
    CHECK(tip.attack_coll.can_setoff == FALSE);
    /* and the status really was set */
    CHECK(tip.proc_update == itMarumineExplodeProcUpdate);

    /* tic 1 does NOT match event 1's timer of 2, so nothing is installed;
     * tic 2 is the next size down */
    tip.multi = 1;
    itMarumineExplodeUpdateAttackEvent(&titem);
    CHECK(tip.event_id == 1);
    CHECK(tip.attack_coll.damage == 30);

    tip.multi = 2;
    itMarumineExplodeUpdateAttackEvent(&titem);
    CHECK(tip.event_id == 2);
    CHECK(tip.attack_coll.damage == 30);
    CHECK_NEAR(tip.attack_coll.size, 350.0F, 1e-4F);

    /* ... and the WRAP: event 3 increments to 4 and straight back to 3,
     * so the smallest hitbox is the one that re-fires rather than the
     * sequence restarting at the widest. */
    tip.multi = 4;
    itMarumineExplodeUpdateAttackEvent(&titem);
    CHECK(tip.event_id == 3);
    CHECK(tip.attack_coll.damage == 20);
    CHECK_NEAR(tip.attack_coll.size, 300.0F, 1e-4F);

    /* tic 6 with event 3 armed: event 3's own timer, so it applies and
     * the index goes 3 -> 4 -> 3 -- the LAST event re-fires rather than
     * the sequence restarting at the widest. A second tic 6 changes
     * nothing, because multi has not moved. */
    tip.multi = 6;
    itMarumineExplodeUpdateAttackEvent(&titem);
    CHECK(tip.event_id == 3);          /* 3 -> 4 -> 3 */
    CHECK(tip.attack_coll.damage == 10);
    CHECK_NEAR(tip.attack_coll.size, 200.0F, 1e-4F);

    tip.multi = 6;
    itMarumineExplodeUpdateAttackEvent(&titem);
    CHECK(tip.event_id == 3);
    CHECK(tip.attack_coll.damage == 10);

    /* ---- itMarumineExplodeProcUpdate: six tics, then the gate -------
     *
     * `multi` is incremented AFTER the event machine, and the item is
     * destroyed (TRUE) on the tic it reaches ITMARUMINE_EXPLODE_LIFETIME
     * -- and it is the STAGE's own function that is called. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.obj = &tdobj;
    tip.multi = 0;
    tip.event_id = 0;
    gGRCommonStruct.yamabuki.gate_status = 2;   /* Open */
    gGRCommonStruct.yamabuki.gate_wait = 0;
    r = itMarumineExplodeProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK(tip.multi == 1);

    tip.multi = ITMARUMINE_EXPLODE_LIFETIME - 1;
    r = itMarumineExplodeProcUpdate(&titem);
    CHECK(r == TRUE);
    CHECK(tip.multi == ITMARUMINE_EXPLODE_LIFETIME);
    CHECK(gGRCommonStruct.yamabuki.gate_status == kYamabukiGateWait);
    CHECK(gGRCommonStruct.yamabuki.gate_wait == 1000);

    /* ---- itMarumineExplodeMakeEffectGotoSetStatus -------------------
     *
     * The tail the anim-end runs: the hurtbox goes INTANGIBLE first, then
     * the sparkle (scaled), the quake, the hide, and the status change --
     * which applies event 0 in the same call. The effect makers are run
     * under the "no particle bank" net the other effect tests use, so the
     * sparkle's own XF is not there to scale; what this pins is the
     * ORDER, which is what the item depends on. */
    {
        s32 saved_bank = gEFManagerParticleBankID;

        ef_pool_reset();
        gEFManagerParticleBankID = -1;

        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        titem.obj = &tdobj;
        tip.damage_coll.hitstatus = nGMHitStatusNormal;
        tip.attack_coll.damage = 999;

        itMarumineExplodeMakeEffectGotoSetStatus(&titem);
        CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone);
        CHECK(tdobj.flags == DOBJ_FLAG_HIDDEN);
        CHECK(tip.attack_coll.fgm_id == nSYAudioFGMExplodeL);
        CHECK(tip.event_id == 1);              /* event 0 applied */
        CHECK(tip.attack_coll.damage == 30);
        CHECK(tip.proc_update == itMarumineExplodeProcUpdate);

        gEFManagerParticleBankID = saved_bank;
    }

    /* ---- the model -------------------------------------------------
     *
     * Marumine's DObjDesc is at file 159 + 0x0790, three entries -- a
     * root, the ball, and the link-18 terminator the bake drops -- and
     * its table names NO MObjSub chain. It is the same SHAPE as
     * Porygon's, which is the point of checking it: two items keyed
     * 0x104 and 0x16C must not resolve to one another's tree. */
    {
        GObj gobj;

        memset(&gobj, 0, sizeof(gobj));
        CHECK(itemModelAddToGObj(&gobj, 0x104) == 2);
        CHECK(DObjGetStruct(&gobj) != NULL);
        CHECK(DObjGetStruct(&gobj)->mobj == NULL);
    }

    gGRCommonStruct.yamabuki.monster_gobj = NULL;

    if (loaded_here)
    {
        itemPackRelease();
    }
}

extern ITDesc dITFushigibanaItemDesc;
extern WPDesc dITFushigibanaWeaponRazorWeaponDesc;

/* The Venusaur: the LAST of Saffron City's five, and the one that fires a
 * weapon with a real MODEL -- where Charmander's flame is particles, the
 * razor is a baked DObjDesc tree, which is why it needed both the weapon
 * table's own row and the pack's data marker. */
static void test_it_fushigibana(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    ITItemEvent ev;
    sb32 loaded_here = 0;

    if (itemPackLoaded() == 0)
    {
        CHECK(itemPackLoad("itcommon.itp") == 0);
        loaded_here = 1;
    }

    CHECK(dITFushigibanaItemDesc.kind == nITKindFushigibana);
    CHECK(dITFushigibanaItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITFushigibanaItemDesc.o_attributes == (intptr_t)0x278);
    CHECK(dITFushigibanaItemDesc.transform_types.tk1 ==
          nGCMatrixKindTraRotRpyR);
    CHECK(dITFushigibanaItemDesc.attack_state == nGMAttackStateNew);
    CHECK(dITFushigibanaItemDesc.proc_update ==
          itFushigibanaCommonProcUpdate);
    CHECK(dITFushigibanaItemDesc.proc_hit == NULL);
    CHECK(dITFushigibanaItemDesc.proc_damage == NULL);

    /* THE WEAPON DESC: a DObJDesc TREE whose display lists are DL links
     * (render flags 0x03), and Proc Hit pressed into service as Shield,
     * Set-Off AND Absorb. */
    CHECK(dITFushigibanaWeaponRazorWeaponDesc.kind ==
          nWPKindFushigibanaRazor);
    CHECK(dITFushigibanaWeaponRazorWeaponDesc.p_weapon ==
          &gITManagerCommonData);
    CHECK(dITFushigibanaWeaponRazorWeaponDesc.o_attributes ==
          (intptr_t)0x308);
    CHECK(dITFushigibanaWeaponRazorWeaponDesc.flags == 0x03);
    CHECK(dITFushigibanaWeaponRazorWeaponDesc.transform_types.tk1 ==
          nGCMatrixKindTraRotRpyRSca);
    CHECK(dITFushigibanaWeaponRazorWeaponDesc.proc_update ==
          itFushigibanaWeaponRazorProcUpdate);
    CHECK(dITFushigibanaWeaponRazorWeaponDesc.proc_map == NULL);
    CHECK(dITFushigibanaWeaponRazorWeaponDesc.proc_hit ==
          itFushigibanaWeaponRazorProcHit);
    CHECK(dITFushigibanaWeaponRazorWeaponDesc.proc_shield ==
          itFushigibanaWeaponRazorProcHit);
    CHECK(dITFushigibanaWeaponRazorWeaponDesc.proc_setoff ==
          itFushigibanaWeaponRazorProcHit);
    CHECK(dITFushigibanaWeaponRazorWeaponDesc.proc_absorb ==
          itFushigibanaWeaponRazorProcHit);
    CHECK(dITFushigibanaWeaponRazorWeaponDesc.proc_hop ==
          itFushigibanaWeaponRazorProcHop);
    CHECK(dITFushigibanaWeaponRazorWeaponDesc.proc_reflector ==
          itFushigibanaWeaponRazorProcReflector);

    /* and the razor's attributes are in the pack, with a data pointer --
     * which is what takes wpManagerAddModel's PACK arm rather than the
     * bare-DObj one the flame takes. */
    {
        WPAttributes *rz = itemPackWeaponAttr(
            *dITFushigibanaWeaponRazorWeaponDesc.p_weapon,
            dITFushigibanaWeaponRazorWeaponDesc.o_attributes);

        CHECK(rz != NULL);
        CHECK(wpManagerIsModelLess(rz) == FALSE);
    }

    /* ---- itFushigibanaCommonUpdateMonsterEvent ----------------------
     *
     * Porygon's machine: the first event's timer is 0, so it fires on the
     * tic multi is 0 and the second is armed. The decomp's own table
     * (dGRYamabukiMap_Fushigibana_HitParties, 264_GRYamabukiMap.c:346):
     * { 0, 40, 20, 300, 100, 90, 0, 0, 0, 0 } and
     * { 8, 361, 8, 300, 70, 0, 30, 0, 0, 0 } -- and note the SECOND one
     * does not have a 0 knockback_base, unlike Porygon's pair. */
    CHECK(itemPackLoaded() == 1);
    CHECK(itemPackMonsterEvent((intptr_t)0x278, 0, &ev) == 0);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tip.item_vars.fushigibana.offset.x = 3.0F;
    tip.item_vars.fushigibana.offset.y = -0.5F;

    tip.multi = 0;
    tip.event_id = 0;
    itFushigibanaCommonUpdateMonsterEvent(&titem);
    CHECK(tip.event_id == 1);
    CHECK(tip.multi == 1);
    CHECK(tip.attack_coll.angle == 40);
    CHECK(tip.attack_coll.damage == 20);
    CHECK_NEAR(tip.attack_coll.size, 300.0F, 1e-4F);
    CHECK_NEAR(tip.attack_coll.knockback_scale, 100.0F, 1e-4F);
    CHECK(tip.attack_coll.knockback_weight == 90);
    CHECK(tip.attack_coll.knockback_base == 0);

    tip.multi = 7;
    itFushigibanaCommonUpdateMonsterEvent(&titem);
    CHECK(tip.multi == 8);

    tip.multi = 8;
    itFushigibanaCommonUpdateMonsterEvent(&titem);
    CHECK(tip.event_id == 1);          /* 1 -> 2 -> 1: the pair alternates */
    CHECK(tip.attack_coll.damage == 8);
    CHECK(tip.attack_coll.angle == 361);
    CHECK(tip.attack_coll.knockback_base == 30);

    /* ---- itFushigibanaCommonProcUpdate ------------------------------
     *
     * The walk, the event machine, and the RAZOR's spawn timer -- which
     * unlike Charmander's counts down on the tic it is seeded. The
     * window is 40..120 of the anim, and the razor maker is avoided here
     * by keeping the timer above zero. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    {
        static MObj tmobj;
        memset(&tmobj, 0, sizeof(tmobj));
        tdobj.mobj = &tmobj;
    }
    titem.obj = &tdobj;
    tip.item_vars.fushigibana.offset.x = 3.0F;
    tip.item_vars.fushigibana.offset.y = -0.5F;
    tip.item_vars.fushigibana.flags = GRYAMABUKI_MONSTER_WEAPON_WAIT;
    tip.item_vars.fushigibana.razor_spawn_wait = 5;
    tdobj.anim_wait = 1.0F;
    tdobj.anim_frame = ITFUSHIGIBANA_RAZOR_SPAWN_BEGIN - 1.0F;

    CHECK(itFushigibanaCommonProcUpdate(&titem) == FALSE);
    CHECK(tdobj.mobj->texture_id_curr == 0);
    CHECK(tip.item_vars.fushigibana.razor_spawn_wait == 5);
    CHECK_NEAR(tdobj.translate.vec.f.x, 3.0F, 1e-4F);
    CHECK_NEAR(tdobj.translate.vec.f.y, -0.5F, 1e-4F);

    /* inside the window: mouth open, timer down one -- and the EVENT
     * machine ran too, which is why multi moved */
    tip.multi = 3;
    tdobj.anim_frame = ITFUSHIGIBANA_RAZOR_SPAWN_BEGIN;
    CHECK(itFushigibanaCommonProcUpdate(&titem) == FALSE);
    CHECK(tdobj.mobj->texture_id_curr == 1);
    CHECK(tip.item_vars.fushigibana.razor_spawn_wait == 4);
    CHECK(tip.multi == 4);

    /* the end of the animation closes the gate */
    tdobj.anim_wait = AOBJ_ANIM_NULL;
    tdobj.anim_frame = 0.0F;
    gGRCommonStruct.yamabuki.gate_status = 2;   /* Open */
    gGRCommonStruct.yamabuki.gate_wait = 0;
    CHECK(itFushigibanaCommonProcUpdate(&titem) == TRUE);
    CHECK(gGRCommonStruct.yamabuki.gate_status == kYamabukiGateWait);
    CHECK(gGRCommonStruct.yamabuki.gate_wait == 1000);

    /* ---- the RAZOR's own procs -------------------------------------- */

    /* Proc Update: the razor accelerates along x in its own facing every
     * tic -- `ADD_VEL_X * lr`, so the sign follows the razor's. */
    {
        static WPStruct twp;

        memset(&twp, 0, sizeof(twp));
        titem.user_data.p = &twp;
        twp.lr = -1;
        twp.physics.vel_air.x = ITFUSHIGIBANA_RAZOR_VEL_X;
        twp.lifetime = ITFUSHIGIBANA_RAZOR_LIFETIME;

        CHECK(itFushigibanaWeaponRazorProcUpdate(&titem) == FALSE);
        CHECK_NEAR(twp.physics.vel_air.x,
                   ITFUSHIGIBANA_RAZOR_VEL_X + ITFUSHIGIBANA_RAZOR_ADD_VEL_X * -1,
                   1e-4F);

        /* and a lr of +1 accelerates the other way */
        twp.lr = +1;
        CHECK(itFushigibanaWeaponRazorProcUpdate(&titem) == FALSE);
        CHECK_NEAR(twp.physics.vel_air.x,
                   ITFUSHIGIBANA_RAZOR_VEL_X + ITFUSHIGIBANA_RAZOR_ADD_VEL_X * -1
                   + ITFUSHIGIBANA_RAZOR_ADD_VEL_X * 1, 1e-4F);

        /* Proc Hit consumes it (TRUE), unlike the flame */
        CHECK(itFushigibanaWeaponRazorProcHit(&titem) == TRUE);
    }

    /* ---- the model -------------------------------------------------
     *
     * Fushigibana's DObjDesc is file 159 + 0x2340 and its `p_mobjsubs` is
     * mobjlink_0x2180, so like Hitokage's it carries an MObj -- which is
     * what `dobj->mobj->texture_id_curr` (its mouth) is written through. */
    {
        GObj gobj;

        memset(&gobj, 0, sizeof(gobj));
        CHECK(itemModelAddToGObj(&gobj, 0x278) == 2);
        CHECK(DObjGetStruct(&gobj) != NULL);
        CHECK(DObjGetStruct(&gobj)->child != NULL);
        CHECK(DObjGetStruct(&gobj)->child->mobj != NULL);
    }

    gGRCommonStruct.yamabuki.monster_gobj = NULL;

    if (loaded_here)
    {
        itemPackRelease();
    }
}

extern ITDesc dITTaruBombItemDesc;
extern ITStatusDesc dITTaruBombStatusDescs[];

/* Race to the Finish's barrel bomb (src/dc/ittarubomb.c).
 * The eighth stage item, and the first one that is not Saffron City's:
 * its table is in a BONUS stage's map file, which is the first map file
 * the item exporter met with no REGION_JP split in it at all.
 *
 * The map procs are left alone here for the reason the other stage
 * items' are -- itMapTestLRWallCheckFloor and
 * itMapCheckCollideAllRebound walk a real collision file this test has
 * none of. What IS on this side of the wire is everything the barrel
 * decides for itself: the attributes it carries, the box it squares up,
 * the burst it walks, and the roll, which is a pure function of the
 * floor's angle and needs no map at all. */
static void test_it_tarubomb(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    sb32 r;
    sb32 loaded_here = 0;

    if (itemPackLoaded() == 0)
    {
        CHECK(itemPackLoad("itcommon.itp") == 0);
        loaded_here = 1;
    }

    CHECK(dITTaruBombItemDesc.kind == nITKindTaruBomb);
    CHECK(dITTaruBombItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITTaruBombItemDesc.o_attributes == (intptr_t)0xA8);
    CHECK(dITTaruBombItemDesc.transform_types.tk1 ==
          nGCMatrixKindTraRotRpyR);
    /* nGMAttackStateNew: unlike the Electrode's, a barrel's hitbox is
     * live from the moment it is made -- it can be set off by one */
    CHECK(dITTaruBombItemDesc.attack_state == nGMAttackStateNew);
    CHECK(dITTaruBombItemDesc.proc_update == itTaruBombFallProcUpdate);
    CHECK(dITTaruBombItemDesc.proc_map == itTaruBombFallProcMap);
    /* ONE function is hit, shield, set-off AND reflector: whatever
     * touches a barrel, the barrel goes off */
    CHECK(dITTaruBombItemDesc.proc_hit == itTaruBombCommonProcHit);
    CHECK(dITTaruBombItemDesc.proc_shield == itTaruBombCommonProcHit);
    CHECK(dITTaruBombItemDesc.proc_setoff == itTaruBombCommonProcHit);
    CHECK(dITTaruBombItemDesc.proc_reflector == itTaruBombCommonProcHit);
    CHECK(dITTaruBombItemDesc.proc_hop == NULL);
    CHECK(dITTaruBombItemDesc.proc_damage == itTaruBombCommonProcDamage);

    /* the three states, and the middle one is deaf: once the barrel is
     * exploding, nothing can touch it */
    CHECK(dITTaruBombStatusDescs[0].proc_update == itTaruBombFallProcUpdate);
    CHECK(dITTaruBombStatusDescs[0].proc_map == itTaruBombFallProcMap);
    CHECK(dITTaruBombStatusDescs[1].proc_update ==
          itTaruBombExplodeProcUpdate);
    CHECK(dITTaruBombStatusDescs[1].proc_map == NULL);
    CHECK(dITTaruBombStatusDescs[1].proc_hit == NULL);
    CHECK(dITTaruBombStatusDescs[1].proc_damage == NULL);
    CHECK(dITTaruBombStatusDescs[2].proc_update == itTaruBombRollProcUpdate);
    CHECK(dITTaruBombStatusDescs[2].proc_map == itTaruBombRollProcMap);

    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&titem, 0, sizeof(titem));
    titem.user_data.p = &tip;
    titem.obj = &tdobj;

    /* ---- the collision box, squared up ------------------------------
     *
     * itTaruBombCommonSetMapCollisionBox lays the barrel on its side and
     * throws away the top and bottom the attributes gave (236/-236),
     * using the WIDTH for both -- so a barrel's map box is as tall as it
     * is wide however the table was filled in. */
    tip.coll_data.map_coll.top = 236;
    tip.coll_data.map_coll.bottom = -236;
    tip.coll_data.map_coll.width = 221;
    itTaruBombCommonSetMapCollisionBox(&titem);
    CHECK(tip.coll_data.map_coll.top == 221);
    CHECK(tip.coll_data.map_coll.bottom == -221);
    CHECK_NEAR(tdobj.rotate.vec.f.x, 1.57079632F, 1e-4F);

    /* ---- falling ----------------------------------------------------
     *
     * gravity, clamped at ITTARUBOMB_TVEL, and the spin the barrel left
     * the ground with is carried through the air unchanged. */
    tip.physics.vel_air.y = 0.0F;
    tip.item_vars.tarubomb.roll_rotate_step = 0.25F;
    tdobj.rotate.vec.f.z = 0.0F;
    r = itTaruBombFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -4.0F, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 0.25F, 1e-4F);
    CHECK_NEAR(tip.item_vars.tarubomb.roll_rotate_step, 0.25F, 1e-4F);

    /* ---- damage -----------------------------------------------------
     *
     * ITTARUBOMB_HEALTH_MAX is 10, five less than a Crate's, and BELOW
     * it nothing happens at all -- no effect machinery is reached, which
     * is why this half needs no pool. */
    tip.percent_damage = 9;
    tip.proc_update = itTaruBombFallProcUpdate;
    tdobj.flags = 0;
    r = itTaruBombCommonProcDamage(&titem);
    CHECK(r == FALSE);
    /* it did not go off: still falling, still visible */
    CHECK(tip.proc_update == itTaruBombFallProcUpdate);
    CHECK(tdobj.flags == 0);

    /* ---- the roll ---------------------------------------------------
     *
     * itTaruBombRollProcUpdate is a pure function of the floor's normal:
     * how far that normal is from straight up is what pushes the barrel
     * along, so a FLAT floor pushes it nowhere. */
    tip.physics.vel_air.x = 0.0F;
    tip.physics.vel_air.y = 0.0F;
    tip.coll_data.floor_angle.x = 0.0F;
    tip.coll_data.floor_angle.y = 1.0F;
    tdobj.rotate.vec.f.z = 0.0F;
    r = itTaruBombRollProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK(tip.lr == +1);                      /* 0.0F >= 0.0F */
    CHECK_NEAR(tdobj.rotate.vec.f.z, 0.0F, 1e-4F);

    /* ... and a 45-degree slope leaning left rolls it left, at
     * (PI/4) * ITTARUBOMB_MUL_VEL_X = 1.0996 per tic. The spin is the
     * SPEED, signed against the direction of travel, so a barrel running
     * left turns the positive way: 1.0996 * ITTARUBOMB_ROLL_ROTATE_MUL. */
    tip.physics.vel_air.x = 0.0F;
    tip.physics.vel_air.y = 0.0F;
    tip.coll_data.floor_angle.x = -0.70710678F;
    tip.coll_data.floor_angle.y = 0.70710678F;
    tdobj.rotate.vec.f.z = 0.0F;
    r = itTaruBombRollProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.x, -1.09955740F, 1e-4F);
    CHECK(tip.lr == -1);
    CHECK_NEAR(tip.item_vars.tarubomb.roll_rotate_step, 0.00494800F, 1e-6F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 0.00494800F, 1e-6F);

    /* the mirror slope rolls it the other way, and the spin flips sign */
    tip.physics.vel_air.x = 0.0F;
    tip.physics.vel_air.y = 0.0F;
    tip.coll_data.floor_angle.x = +0.70710678F;
    tip.coll_data.floor_angle.y = +0.70710678F;
    (void)itTaruBombRollProcUpdate(&titem);
    CHECK_NEAR(tip.physics.vel_air.x, +1.09955740F, 1e-4F);
    CHECK(tip.lr == +1);
    CHECK_NEAR(tip.item_vars.tarubomb.roll_rotate_step, -0.00494800F, 1e-6F);

    /* ---- the BURST, driven on its own -------------------------------
     *
     * The pack's own table (the decomp's dGRBonus3Map_TaruBomb_Attack
     * Events, 295_GRBonus3Map.c:109), read through the same accessor the
     * item uses. Entering the state sets multi and event_id to 0 and
     * applies event 0 IN THE SAME CALL, so the widest hitbox is up on
     * tic 0 rather than one tic later -- the same shape the Electrode's
     * has.
     *
     * This half needs no effect pool either: itTaruBombExplodeSetStatus
     * is the arm BELOW itTaruBombExplodeMakeEffectGotoSetStatus, so the
     * sparkle and the quake are not reached. */
    memset(&tip, 0, sizeof(tip));
    itTaruBombExplodeSetStatus(&titem);
    CHECK(tip.multi == 0);
    CHECK(tip.event_id == 1);
    CHECK(tip.attack_coll.fgm_id == nSYAudioFGMExplodeL);
    CHECK(tip.attack_coll.can_rehit_item == TRUE);
    CHECK(tip.attack_coll.can_reflect == FALSE);
    CHECK(tip.attack_coll.can_setoff == FALSE);
    CHECK(tip.attack_coll.element == nGMHitElementFire);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone);
    CHECK_NEAR(tip.attack_coll.throw_mul, 1.0F, 1e-4F);
    CHECK(tip.attack_coll.angle == 361);
    CHECK(tip.attack_coll.damage == 16);
    CHECK_NEAR(tip.attack_coll.size, 350.0F, 1e-4F);
    CHECK(tip.proc_update == itTaruBombExplodeProcUpdate);

    /* tic 1 is nobody's timer, so the hitbox stands; tic 2 is the next
     * size down. The item lives while multi < ITTARUBOMB_EXPLODE_LIFETIME
     * and is destroyed (TRUE) on the tic that reaches it. */
    r = itTaruBombExplodeProcUpdate(&titem);          /* multi 0 -> 1 */
    CHECK(r == FALSE);
    CHECK(tip.event_id == 1);
    CHECK(tip.attack_coll.damage == 16);

    r = itTaruBombExplodeProcUpdate(&titem);          /* multi 1 -> 2 */
    CHECK(r == FALSE);
    CHECK(tip.event_id == 2);
    CHECK(tip.attack_coll.damage == 11);
    CHECK_NEAR(tip.attack_coll.size, 250.0F, 1e-4F);

    r = itTaruBombExplodeProcUpdate(&titem);          /* multi 2 -> 3 */
    CHECK(r == FALSE);
    CHECK(tip.event_id == 2);
    CHECK(tip.attack_coll.damage == 11);

    r = itTaruBombExplodeProcUpdate(&titem);          /* multi 3 -> 4 */
    CHECK(r == FALSE);
    CHECK(tip.event_id == 3);
    CHECK(tip.attack_coll.damage == 8);
    CHECK_NEAR(tip.attack_coll.size, 150.0F, 1e-4F);

    r = itTaruBombExplodeProcUpdate(&titem);          /* multi 4 -> 5 */
    CHECK(r == FALSE);
    CHECK(tip.event_id == 3);

    /* the sixth tic destroys the item BEFORE the last event would have
     * applied -- multi reaches ITTARUBOMB_EXPLODE_LIFETIME and the proc
     * returns without stepping the burst, so event 3 (1% at size 0) is
     * only ever reached by a barrel that somehow lives longer. */
    r = itTaruBombExplodeProcUpdate(&titem);          /* multi 5 -> 6 */
    CHECK(r == TRUE);
    CHECK(tip.event_id == 3);
    CHECK(tip.attack_coll.damage == 8);

    /* ... and the wrap is real: given a seventh tic, event 3's own timer
     * of 6 applies and the index goes 3 -> 4 -> 3, so the STOPPED hitbox
     * is the one that stays rather than the sequence restarting wide. */
    tip.multi = 5;
    r = itTaruBombExplodeProcUpdate(&titem);
    CHECK(r == TRUE);
    tip.multi = 6;
    tip.event_id = 3;
    (void)itTaruBombExplodeProcUpdate(&titem);        /* multi 6 -> 7 */
    CHECK(tip.event_id == 3);

    /* ---- the explosion's own entry ----------------------------------
     *
     * itTaruBombExplodeMakeEffectGotoSetStatus stops the barrel dead,
     * hides its model and turns its hitbox off before the sparkle is
     * made -- and the sparkle is the reason this one is inside the
     * "no particle bank" net the other effect tests use. */
    {
        s32 saved_bank = gEFManagerParticleBankID;

        ef_pool_reset();
        gEFManagerParticleBankID = -1;

        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        tip.physics.vel_air.x = 12.0F;
        tip.physics.vel_air.y = -30.0F;
        tip.physics.vel_air.z = 1.0F;
        tip.damage_coll.hitstatus = nGMHitStatusNormal;
        tip.attack_coll.damage = 999;

        itTaruBombExplodeMakeEffectGotoSetStatus(&titem);

        CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
        CHECK_NEAR(tip.physics.vel_air.y, 0.0F, 1e-4F);
        CHECK_NEAR(tip.physics.vel_air.z, 0.0F, 1e-4F);
        CHECK(tdobj.flags == DOBJ_FLAG_HIDDEN);
        CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone);
        CHECK(tip.attack_coll.fgm_id == nSYAudioFGMExplodeL);
        CHECK(tip.event_id == 1);              /* event 0 applied */
        CHECK(tip.attack_coll.damage == 16);
        CHECK(tip.proc_update == itTaruBombExplodeProcUpdate);

        gEFManagerParticleBankID = saved_bank;
    }

    if (loaded_here)
    {
        itemPackRelease();
    }
}

extern ITDesc dITHitokageItemDesc;
extern ITStatusDesc dITHitokageStatusDescs[];
extern WPDesc dITHitokageWeaponFlameWeaponDesc;
/* the Gate's own global (src/dc/gryamabuki.c), declared beside the other
 * one at test_gr_yamabuki further down */
extern s32 dGRYamabukiMonsterAttackKind;

/* The Charmander: the third of Saffron City's five, and the first item
 * whose own body SPITS A WEAPON -- through wpManagerMakeWeapon with a
 * WPDesc whose attributes are the pack's, which is what the WEAPON
 * ATTRIBUTES section exists for. */
static void test_it_hitokage(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    s32 saved_kind;
    sb32 loaded_here = 0;

    if (itemPackLoaded() == 0)
    {
        CHECK(itemPackLoad("itcommon.itp") == 0);
        loaded_here = 1;
    }

    CHECK(dITHitokageItemDesc.kind == nITKindHitokage);
    CHECK(dITHitokageItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITHitokageItemDesc.o_attributes == (intptr_t)0x1FC);
    CHECK(dITHitokageItemDesc.transform_types.tk1 == nGCMatrixKindTraRotRpyR);
    CHECK(dITHitokageItemDesc.attack_state == nGMAttackStateNew);
    CHECK(dITHitokageItemDesc.proc_update == itHitokageCommonProcUpdate);
    CHECK(dITHitokageItemDesc.proc_damage == itHitokageCommonProcDamage);
    CHECK(dITHitokageItemDesc.proc_hit == NULL);

    /* ONE status, the knocked-out one */
    CHECK(dITHitokageStatusDescs[0].proc_update ==
          itHitokageDamagedProcUpdate);
    CHECK(dITHitokageStatusDescs[0].proc_damage == NULL);

    /* THE WEAPON DESC, and the two things about it worth pinning: its
     * attributes are the pack's (0x244, the flame's table), and Proc Hit
     * is ALSO Proc Shield and Proc Set-Off -- a flame that is blocked or
     * clanged with behaves exactly as one that hits. */
    CHECK(dITHitokageWeaponFlameWeaponDesc.kind == nWPKindHitokageFlame);
    CHECK(dITHitokageWeaponFlameWeaponDesc.p_weapon == &gITManagerCommonData);
    CHECK(dITHitokageWeaponFlameWeaponDesc.o_attributes == (intptr_t)0x244);
    CHECK(dITHitokageWeaponFlameWeaponDesc.transform_types.tk1 ==
          nGCMatrixKindTraRotRpyRSca);
    CHECK(dITHitokageWeaponFlameWeaponDesc.proc_update ==
          itHitokageWeaponFlameProcUpdate);
    CHECK(dITHitokageWeaponFlameWeaponDesc.proc_map ==
          itHitokageWeaponFlameProcMap);
    CHECK(dITHitokageWeaponFlameWeaponDesc.proc_hit ==
          itHitokageWeaponFlameProcHit);
    CHECK(dITHitokageWeaponFlameWeaponDesc.proc_shield ==
          itHitokageWeaponFlameProcHit);
    CHECK(dITHitokageWeaponFlameWeaponDesc.proc_setoff ==
          itHitokageWeaponFlameProcHit);
    CHECK(dITHitokageWeaponFlameWeaponDesc.proc_hop == NULL);
    CHECK(dITHitokageWeaponFlameWeaponDesc.proc_reflector ==
          itHitokageWeaponFlameProcReflector);
    CHECK(dITHitokageWeaponFlameWeaponDesc.proc_absorb == NULL);

    /* ... and the ATTRIBUTES the desc names really are in the pack, which
     * is the one thing that makes this weapon makeable at all: without
     * them wpManagerMakeWeapon would read the byte overlay at
     * ITCommonData + 0x244, which is another item's table. */
    CHECK(itemPackWeaponAttr(*dITHitokageWeaponFlameWeaponDesc.p_weapon,
                             dITHitokageWeaponFlameWeaponDesc.o_attributes)
          != NULL);

    /* ---- itHitokageCommonProcUpdate: the MOUTH, and the spawn timer ---
     *
     * `texture_id_curr` is 1 exactly while the flame may be spat, and 0
     * otherwise -- and it is written BEFORE the spawn check, so the mouth
     * is open on the frame the flame is due. `_INSTANT` fires from frame
     * 0; `_WAIT` only inside 40..120; `_NONE` never. The weapon maker is
     * avoided in the first four cases by keeping flame_spawn_wait above 0
     * (the counter decrements and nothing is made). */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    {
        static MObj tmobj;
        memset(&tmobj, 0, sizeof(tmobj));
        tdobj.mobj = &tmobj;
    }
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tip.item_vars.hitokage.offset.x = 5.0F;
    tip.item_vars.hitokage.offset.y = -2.0F;
    tip.item_vars.hitokage.flame_spawn_wait = 5;
    tdobj.anim_wait = 1.0F;

    /* OUTSIDE the window with WAIT: mouth shut, and the spawn timer does
     * not move at all -- it is decremented only inside the window, which
     * is why a Charmander whose window has passed never fires again */
    tip.item_vars.hitokage.flags = GRYAMABUKI_MONSTER_WEAPON_WAIT;
    tdobj.anim_frame = ITHITOKAGE_FLAME_SPAWN_BEGIN - 1.0F;
    CHECK(itHitokageCommonProcUpdate(&titem) == FALSE);
    CHECK(tdobj.mobj->texture_id_curr == 0);
    CHECK(tip.item_vars.hitokage.flame_spawn_wait == 5);
    CHECK_NEAR(tdobj.translate.vec.f.x, 5.0F, 1e-4F);
    CHECK_NEAR(tdobj.translate.vec.f.y, -2.0F, 1e-4F);

    /* inside the window: mouth OPEN, and the timer runs down rather than
     * firing, because it has not reached zero */
    tdobj.anim_frame = ITHITOKAGE_FLAME_SPAWN_BEGIN;
    CHECK(itHitokageCommonProcUpdate(&titem) == FALSE);
    CHECK(tdobj.mobj->texture_id_curr == 1);
    CHECK(tip.item_vars.hitokage.flame_spawn_wait == 4);

    /* past the END of the window the mouth shuts again, and the timer
     * stops counting -- the window is checked first */
    tdobj.anim_frame = ITHITOKAGE_FLAME_SPAWN_END + 1.0F;
    CHECK(itHitokageCommonProcUpdate(&titem) == FALSE);
    CHECK(tdobj.mobj->texture_id_curr == 0);
    CHECK(tip.item_vars.hitokage.flame_spawn_wait == 4);

    /* the end of the animation closes the gate and destroys the item */
    tip.item_vars.hitokage.flags = GRYAMABUKI_MONSTER_WEAPON_WAIT;
    tdobj.anim_wait = AOBJ_ANIM_NULL;
    gGRCommonStruct.yamabuki.gate_status = 2;   /* Open */
    gGRCommonStruct.yamabuki.gate_wait = 0;
    CHECK(itHitokageCommonProcUpdate(&titem) == TRUE);
    CHECK(gGRCommonStruct.yamabuki.gate_status == kYamabukiGateWait);
    CHECK(gGRCommonStruct.yamabuki.gate_wait == 1000);

    /* ---- the DRAW: the step-forward rule, on its own cases ----------
     *
     * `dGRYamabukiMonsterAttackKind` is the Gate's memory of what the last
     * monster did. A draw that EQUALS it, or that it is CONTAINED BY (the
     * `&` test: `_ALL` contains `_WAIT` and `_INSTANT`), is stepped
     * forward one and wrapped -- which is what stops two Charmanders in a
     * row breathing alike.
     *
     * IT IS A HEURISTIC, NOT AN INVARIANT, and the cases below are what
     * say so rather than a restatement of the formula: ONE step is not
     * always enough. A draw of 2 (`_INSTANT`) against a previous of 3
     * (`_ALL`) steps to exactly 3, which IS the previous mode. The port
     * keeps that, because the game does. What is always true is the part
     * the code checks first: a draw equal to the previous is stepped.
     * (The values are the `GRYAMABUKI_MONSTER_WEAPON_*` macros --
     * NONE 0, WAIT 1, INSTANT 2, ALL 3 -- and the arithmetic is the
     * model's own expression, applied here rather than sampled.) */
#define ITHITOKAGE_DRAW_STEP(drawn, prev)                                 \
    ((((drawn) == (prev)) || ((drawn) & (prev)))                          \
     ? (((drawn) + 1) % GRYAMABUKI_MONSTER_WEAPON_MAX) : (drawn))

    CHECK(ITHITOKAGE_DRAW_STEP(0, 0) == 1);   /* equal: stepped */
    CHECK(ITHITOKAGE_DRAW_STEP(1, 1) == 2);
    CHECK(ITHITOKAGE_DRAW_STEP(3, 3) == 0);   /* wrapped */
    CHECK(ITHITOKAGE_DRAW_STEP(0, 1) == 0);   /* NONE is in nothing: left */
    CHECK(ITHITOKAGE_DRAW_STEP(2, 1) == 2);
    CHECK(ITHITOKAGE_DRAW_STEP(3, 1) == 0);   /* ALL contains WAIT: stepped */
    CHECK(ITHITOKAGE_DRAW_STEP(2, 3) == 3);   /* one step, onto the previous */
    CHECK(ITHITOKAGE_DRAW_STEP(0, 3) == 0);

#undef ITHITOKAGE_DRAW_STEP

    /* and the global really is the Gate's -- writing it here is what
     * test_gr_yamabuki's own check of the Gate's init reads back */
    saved_kind = dGRYamabukiMonsterAttackKind;
    dGRYamabukiMonsterAttackKind = GRYAMABUKI_MONSTER_WEAPON_MAX;
    CHECK(dGRYamabukiMonsterAttackKind == GRYAMABUKI_MONSTER_WEAPON_MAX);
    dGRYamabukiMonsterAttackKind = saved_kind;

    /* ---- the model -------------------------------------------------
     *
     * Hitokage's DObjDesc is file 159 + 0x1990, three entries, and its
     * `p_mobjsubs` is mobjlink_0x17D0 -- a per-joint array whose entry 1
     * is the MObjSub chain. SO THIS ONE HAS AN MObj, which is what
     * itHitokageCommonProcUpdate writes its mouth through. */
    {
        GObj gobj;

        memset(&gobj, 0, sizeof(gobj));
        CHECK(itemModelAddToGObj(&gobj, 0x1FC) == 2);
        CHECK(DObjGetStruct(&gobj) != NULL);
        CHECK(DObjGetStruct(&gobj)->child != NULL);
        CHECK(DObjGetStruct(&gobj)->child->mobj != NULL);
    }

    /* ---- itHitokageDamagedProcUpdate/ProcDead ------------------------
     *
     * The same pair GLucky has: gravity and a spin whose direction is the
     * item's facing, and a DEFINITE destruction (TRUE) rather than the
     * Piranha Plant's recycle. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.obj = &tdobj;
    tip.lr = 1;
    CHECK(itHitokageDamagedProcUpdate(&titem) == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITHITOKAGE_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, -ITHITOKAGE_HIT_ROTATE_Z, 1e-3F);
    tip.lr = -1;
    CHECK(itHitokageDamagedProcUpdate(&titem) == FALSE);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 0.0F, 1e-3F);
    CHECK(itHitokageDamagedProcDead(&titem) == TRUE);

    /* ---- itHitokageCommonProcDamage: over the threshold the gate is told
     * it has no monster, and the item is thrown and enters its status.
     * 100 is a high bar for an item -- this Charmander takes a real hit to
     * knock over. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.obj = &tdobj;
    gGRCommonStruct.yamabuki.monster_gobj = &titem;
    tip.damage_knockback = ITHITOKAGE_NDAMAGE_KNOCKBACK_MIN - 1.0F;
    CHECK(itHitokageCommonProcDamage(&titem) == FALSE);
    CHECK(gGRCommonStruct.yamabuki.monster_gobj == &titem);

    tip.damage_knockback = ITHITOKAGE_NDAMAGE_KNOCKBACK_MIN;
    tip.damage_lr = 1;
    tip.ga = nMPKineticsAir;
    tip.attack_coll.attack_state = nGMAttackStateNew;
    CHECK(itHitokageCommonProcDamage(&titem) == FALSE);
    CHECK(gGRCommonStruct.yamabuki.monster_gobj == NULL);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone);
    CHECK(tdobj.anim_wait == AOBJ_ANIM_NULL);
    CHECK(tip.proc_update == itHitokageDamagedProcUpdate);
    CHECK(tip.proc_dead == itHitokageDamagedProcDead);

    gGRCommonStruct.yamabuki.monster_gobj = NULL;

    if (loaded_here)
    {
        itemPackRelease();
    }
}

extern ITDesc dITMSBombItemDesc;
extern ITStatusDesc dITMSBombStatusDescs[];

/* forward decl: the real one (identical signature) is defined further
 * down, beside the effect-manager tests this file's own explosion
 * scaffolding reuses. */
static void ef_pool_reset(void);

enum
{
    nITMSBombStatusWait, nITMSBombStatusFall, nITMSBombStatusHold,
    nITMSBombStatusThrown, nITMSBombStatusDropped, nITMSBombStatusAttached,
    nITMSBombStatusDetached, nITMSBombStatusExplode
};

static void test_it_msbomb(void)
{
    GObj titem, towner;
    ITStruct tip;
    DObj tdobj, tdobj_owner, tchild1, tchild2;
    MObj tmobj;
    ITAttributes tattr;
    sb32 r;

    CHECK(dITMSBombItemDesc.kind == nITKindMSBomb);
    CHECK(dITMSBombItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITMSBombItemDesc.o_attributes == (intptr_t)0x3bc);  /* reloc_data.us.h: llITCommonDataMSBombItemAttributes */
    CHECK(dITMSBombItemDesc.proc_update == itMSBombFallProcUpdate);
    CHECK(dITMSBombItemDesc.proc_map == itMSBombFallProcMap);
    CHECK(dITMSBombItemDesc.proc_hit == NULL);
    CHECK(dITMSBombItemDesc.proc_damage == NULL);

    CHECK(dITMSBombStatusDescs[nITMSBombStatusWait].proc_map == itMSBombWaitProcMap);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusFall].proc_update == itMSBombFallProcUpdate);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusHold].proc_update == NULL);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusThrown].proc_hit == itMSBombCommonProcHit);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusThrown].proc_shield == itMSBombCommonProcHit);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusThrown].proc_setoff == itMSBombCommonProcHit);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusThrown].proc_hop == itMainCommonProcHop);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusThrown].proc_reflector == itMainCommonProcReflector);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusDropped].proc_map == itMSBombDroppedProcMap);
    /* Attached/Detached: the two-phase surface-cling shape unique to this
     * item -- Bumper only ever had one attached state. Both
     * share proc_damage (itMSBombCommonProcDamage), but only Attached has
     * proc_map's own line-still-exists check wired to itMSBombAttachedProcMap;
     * Detached reuses Dropped's own proc_map. */
    CHECK(dITMSBombStatusDescs[nITMSBombStatusAttached].proc_update == itMSBombAttachedProcUpdate);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusAttached].proc_map == itMSBombAttachedProcMap);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusAttached].proc_damage == itMSBombCommonProcDamage);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusDetached].proc_update == itMSBombDetachedProcUpdate);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusDetached].proc_map == itMSBombDroppedProcMap);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusDetached].proc_damage == itMSBombCommonProcDamage);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusExplode].proc_update == itMSBombExplodeProcUpdate);
    CHECK(dITMSBombStatusDescs[nITMSBombStatusExplode].proc_map == NULL);

    /* itMSBombFallProcUpdate: gravity, spin, and the two-child DObj
     * propagation this file's own shape needs (dobj->child->sib_next's
     * own Z rotate is kept in lockstep with the root's -- the second
     * child model, not the first, unlike anything Bumper's single-child
     * shape needed). */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    memset(&tchild1, 0, sizeof(tchild1));
    memset(&tchild2, 0, sizeof(tchild2));
    tdobj.mobj = &tmobj;
    tdobj.child = &tchild1;
    tchild1.sib_next = &tchild2;
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tdobj.rotate.vec.f.z = 2.0F;
    tip.spin_step = 5.0F;
    r = itMSBombFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITMSBOMB_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 7.0F, 1e-4F);
    CHECK_NEAR(tchild2.rotate.vec.f.z, 7.0F, 1e-4F);
    CHECK_NEAR(tchild1.rotate.vec.f.z, 0.0F, 1e-4F); /* untouched -- only the SECOND child follows */

    /* itMSBombWaitProcMap/FallProcMap: honest no-collision compositions,
     * same shape as every prior itMap-adjacent test in this port. */
    memset(&tip, 0, sizeof(tip));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    r = itMSBombWaitProcMap(&titem);
    CHECK(r == FALSE);
    memset(&tip.coll_data, 0, sizeof(tip.coll_data));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    r = itMSBombFallProcMap(&titem);
    CHECK(r == FALSE);

    /* itMSBombWaitSetStatus/FallSetStatus/HoldSetStatus. */
    memset(&tip, 0, sizeof(tip));
    itMSBombWaitSetStatus(&titem);
    CHECK(tip.is_allow_pickup == TRUE);
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == itMSBombWaitProcMap);

    memset(&tip, 0, sizeof(tip));
    tip.is_allow_pickup = TRUE;
    itMSBombFallSetStatus(&titem);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.proc_update == itMSBombFallProcUpdate);
    CHECK(tip.proc_map == itMSBombFallProcMap);

    memset(&tip, 0, sizeof(tip));
    itMSBombHoldSetStatus(&titem);
    CHECK(tip.proc_update == NULL);
    CHECK(tip.proc_map == NULL);

    /* itMSBombThrownProcUpdate: same gravity/spin/two-child shape as
     * FallProcUpdate. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.mobj = &tmobj;
    tdobj.child = &tchild1;
    tchild1.sib_next = &tchild2;
    titem.obj = &tdobj;
    tdobj.rotate.vec.f.z = 0.0F;
    tip.spin_step = 3.0F;
    r = itMSBombThrownProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITMSBOMB_GRAVITY, 1e-4F);
    CHECK_NEAR(tchild2.rotate.vec.f.z, 3.0F, 1e-4F);

    /* itMSBombThrownProcMap/DroppedProcMap: both compose
     * itMapCheckMapProcAll -- honest no-collision, same limitation every
     * itMap-adjacent test in this port has. */
    memset(&tip, 0, sizeof(tip));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    r = itMSBombThrownProcMap(&titem);
    CHECK(r == FALSE);
    memset(&tip.coll_data, 0, sizeof(tip.coll_data));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    r = itMSBombDroppedProcMap(&titem);
    CHECK(r == FALSE);

    /* itMSBombCommonProcHit: VelSetRebound, same as every prior
     * companion item's own proc_hit/proc_shield/proc_setoff row (all
     * three status rows above wire the SAME function to all three). */
    memset(&tip, 0, sizeof(tip));
    tip.physics.vel_air.x = 5.0F;
    r = itMSBombCommonProcHit(&titem);
    CHECK(r == FALSE);

    /* itMSBombThrownSetStatus/DroppedSetStatus: both seed the fixed
     * collision half-heights. */
    memset(&tip, 0, sizeof(tip));
    itMSBombThrownSetStatus(&titem);
    CHECK_NEAR(tip.coll_data.map_coll.top, ITMSBOMB_COLL_SIZE, 1e-4F);
    CHECK_NEAR(tip.coll_data.map_coll.bottom, -ITMSBOMB_COLL_SIZE, 1e-4F);
    CHECK(tip.proc_map == itMSBombThrownProcMap);
    itMSBombDroppedSetStatus(&titem);
    CHECK_NEAR(tip.coll_data.map_coll.top, ITMSBOMB_COLL_SIZE, 1e-4F);
    CHECK(tip.proc_map == itMSBombDroppedProcMap);

    /* itMSBombAttachedUpdateSurface: orients the model's own Z rotate to
     * whichever of ceil/floor/lwall/rwall the item is touching (picked
     * out of coll_data->mask_curr) -- unlike Bumper's own
     * AttachedSetModelPitch, which only ever assumes floor. Floor and a
     * side wall both proven, not just floor. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.obj = &tdobj;
    tip.coll_data.mask_curr = MAP_FLAG_FLOOR;
    tip.coll_data.floor_angle.x = 0.0F;
    tip.coll_data.floor_angle.y = 1.0F;
    tip.coll_data.floor_line_id = 7;
    itMSBombAttachedUpdateSurface(&titem);
    CHECK(tip.attach_line_id == 7);
    CHECK_NEAR(tdobj.rotate.vec.f.z, syUtilsArcTan2(1.0F, 0.0F) - F_CST_DTOR32(90.0F), 1e-4F);

    memset(&tip, 0, sizeof(tip));
    tip.coll_data.mask_curr = MAP_FLAG_LWALL;
    tip.coll_data.lwall_angle.x = 1.0F;
    tip.coll_data.lwall_angle.y = 0.0F;
    tip.coll_data.lwall_line_id = 9;
    itMSBombAttachedUpdateSurface(&titem);
    CHECK(tip.attach_line_id == 9);
    CHECK_NEAR(tdobj.rotate.vec.f.z, syUtilsArcTan2(0.0F, 1.0F) - F_CST_DTOR32(90.0F), 1e-4F);

    /* itMSBombAttachedInitVars: the fixed collision box, the two-child
     * flags flip (child shown, child->sib_next hidden -- the OPPOSITE
     * sense from itMSBombMakeItem's own initial setup, which is left
     * out), the surface pin, and -- new beyond anything Bumper's own
     * AttachedInitVars needed -- a rumble call through
     * gSCManagerBattleState->players[ip->player].fighter_gobj when a
     * real player owns it. Driven twice: player == GMCOMMON_PLAYERS_MAX
     * (skips the rumble branch outright) and player == 0 with a scratch
     * fighter wired on gSCManagerBattleState (exercises the branch;
     * pkind left as nFTPlayerKindNot so ftParamMakeRumble's own inner
     * gmRumbleSetPlayerRumbleParams call, needing more scaffolding than
     * this test wires, is not reached -- ftParamMakeRumble's own guard,
     * not a shortcut taken in the port). Also proves the u8 `player`
     * field's own `!= -1` check is EFFECTIVELY DEAD CODE: a u8 promotes
     * to an int in [0,255] under `!=`, which can never equal the int
     * literal -1, so ONLY the `!= GMCOMMON_PLAYERS_MAX` half of the
     * guard ever excludes anything -- worth recording since it looks
     * load-bearing on a first read and is not. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.mobj = &tmobj;
    tdobj.child = &tchild1;
    tchild1.sib_next = &tchild2;
    titem.obj = &tdobj;
    tip.coll_data.mask_curr = MAP_FLAG_FLOOR;
    tip.coll_data.floor_angle.x = 0.0F;
    tip.coll_data.floor_angle.y = 1.0F;
    tip.physics.vel_air.x = tip.physics.vel_air.y = tip.physics.vel_air.z = 9.0F;
    tip.player = (u8)-1; /* 255 -- proves the dead-code half of the guard */
    tip.owner_gobj = &titem;
    itMSBombAttachedInitVars(&titem);
    CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
    CHECK(tchild1.flags == DOBJ_FLAG_NONE);
    CHECK(tchild2.flags == DOBJ_FLAG_HIDDEN);
    CHECK_NEAR(tip.coll_data.map_coll.top, ITMSBOMB_COLL_SIZE, 1e-4F);
    CHECK(tip.is_attach_surface == TRUE);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK(tip.owner_gobj == NULL);

    {
        FTStruct tfp_owner;
        SCPlayerData *slot = &gSCManagerBattleState->players[0];
        GObj *save_fighter_gobj = slot->fighter_gobj;

        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        memset(&tmobj, 0, sizeof(tmobj));
        memset(&tfp_owner, 0, sizeof(tfp_owner));
        tdobj.mobj = &tmobj;
        tdobj.child = &tchild1;
        tchild1.sib_next = &tchild2;
        titem.obj = &tdobj;
        towner.obj = &tdobj_owner;
        memset(&tdobj_owner, 0, sizeof(tdobj_owner));
        towner.user_data.p = &tfp_owner;
        tfp_owner.pkind = nFTPlayerKindNot; /* skips ftParamMakeRumble's own inner call */
        slot->fighter_gobj = &towner;
        tip.coll_data.mask_curr = MAP_FLAG_FLOOR;
        tip.player = 0;
        itMSBombAttachedInitVars(&titem);
        CHECK(tip.is_attach_surface == TRUE);

        slot->fighter_gobj = save_fighter_gobj;
    }

    /* itMSBombExplodeMakeEffect/InitStatusVars: driven with the SAME
     * "no bank loaded" safety net the effect-manager test
     * uses (gEFManagerParticleBankID = -1 makes every
     * particle-based maker return NULL through lbParticleMakeScriptID's
     * own guard) plus a scratch camera GObj for efManagerQuakeMakeEffect's
     * own GObj-based path (the same shape the quake-arithmetic test uses)
     * and gITManagerCommonData pinned to NULL for the tail's own
     * itMSBombExplodeSetStatus -> ...UpdateAttackEvent (this file's own
     * fourth reloc-offset kind, proven directly below too). */
    {
        static GObj cam_gobj;
        static CObj cam_cobj;
        GObj *prev_cam = gGMCameraGObj;
        s32 saved_bank = gEFManagerParticleBankID;
        void *save_common = gITManagerCommonData;

        memset(&cam_gobj, 0, sizeof(cam_gobj));
        memset(&cam_cobj, 0, sizeof(cam_cobj));
        cam_gobj.obj = &cam_cobj;
        gGMCameraGObj = &cam_gobj;
        ef_pool_reset();
        gEFManagerParticleBankID = -1;
        gITManagerCommonData = sHostItemData;
        memset(host_attack_events(llITCommonDataMSBombAttackEvents), 0, sizeof(ITAttackEvent) * 4);

        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        memset(&tmobj, 0, sizeof(tmobj));
        memset(&tattr, 0, sizeof(tattr));
        tdobj.mobj = &tmobj;
        titem.obj = &tdobj;
        tip.attr = &tattr;
        tip.coll_data.mask_curr = MAP_FLAG_FLOOR;
        tattr.map_coll_bottom = -20;
        tip.lr = 1;
        itMSBombExplodeInitStatusVars(&titem, TRUE);
        CHECK(tdobj.flags == DOBJ_FLAG_HIDDEN);
        CHECK(tip.proc_update == itMSBombExplodeProcUpdate);
        CHECK(tip.multi == 0);
        CHECK(tip.event_id == 1); /* ev[0].timer == 0 == tip.multi(0) -- UpdateAttackEvent's own tail fires immediately */

        /* itMSBombCommonProcDamage: the FGM plus the SAME InitStatusVars
         * tail, called with is_make_effect FALSE this time (the dust
         * puff is skipped regardless of mask_curr). */
        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        memset(&tmobj, 0, sizeof(tmobj));
        tdobj.mobj = &tmobj;
        titem.obj = &tdobj;
        tip.attr = &tattr;
        r = itMSBombCommonProcDamage(&titem);
        CHECK(r == FALSE);
        CHECK(tip.proc_update == itMSBombExplodeProcUpdate);

        gITManagerCommonData = save_common;
        gEFManagerParticleBankID = saved_bank;
        gGMCameraGObj = prev_cam;
    }

    /* itMSBombExplodeUpdateAttackEvent: the fourth kind of reloc-offset
     * arithmetic this port has needed (itGetAttackEvent) -- proven
     * directly, not just through its callers above. That macro is
     * `*it_desc.p_file + off`, and dITMSBombItemDesc's p_file is
     * `&gITManagerCommonData`, so pointing this global at the faked
     * ITCommonData makes the read land on the events written below. The
     * global is saved and restored, the same shape itstar/itfflower use
     * for it. */
    {
        void *save_common = gITManagerCommonData;
        gITManagerCommonData = sHostItemData;

        memset(&tip, 0, sizeof(tip));
        memset(host_attack_events(llITCommonDataMSBombAttackEvents), 0, sizeof(ITAttackEvent) * 4);
        host_attack_events(llITCommonDataMSBombAttackEvents)[0].timer = 5;
        host_attack_events(llITCommonDataMSBombAttackEvents)[0].angle = 100;
        host_attack_events(llITCommonDataMSBombAttackEvents)[0].damage = 12;
        host_attack_events(llITCommonDataMSBombAttackEvents)[0].size = 30;
        tip.multi = 5;
        tip.event_id = 0;
        itMSBombExplodeUpdateAttackEvent(&titem);
        CHECK(tip.attack_coll.angle == 100);
        CHECK(tip.attack_coll.damage == 12);
        CHECK(tip.attack_coll.size == 30);
        CHECK(tip.attack_coll.can_rehit_item == TRUE);
        CHECK(tip.attack_coll.can_hop == FALSE);
        CHECK(tip.attack_coll.can_reflect == FALSE);
        CHECK(tip.attack_coll.can_setoff == FALSE);
        CHECK(tip.attack_coll.element == nGMHitElementFire);
        CHECK(tip.event_id == 1);

        /* a non-matching multi/timer this frame is a no-op. */
        tip.event_id = 1;
        host_attack_events(llITCommonDataMSBombAttackEvents)[1].timer = 99;
        tip.multi = 0;
        itMSBombExplodeUpdateAttackEvent(&titem);
        CHECK(tip.event_id == 1);

        /* event_id wraparound: reaching 4 clamps back to 3, matching the
         * 4-bit field's own cap and the array's own [4] sizing. */
        tip.event_id = 3;
        tip.multi = 0;
        host_attack_events(llITCommonDataMSBombAttackEvents)[3].timer = 0;
        itMSBombExplodeUpdateAttackEvent(&titem);
        CHECK(tip.event_id == 3);

        gITManagerCommonData = save_common;
    }

    /* itMSBombAttachedProcMap: unlike every itMap-adjacent honest
     * "no-collision" composition elsewhere in this port, this call goes
     * STRAIGHT into mpCollisionCheckExistLineID(ip->attach_line_id) with
     * no outer itMapCheck* gate ahead of it (Bumper's own analogous test
     * never actually proved this branch either -- its own
     * outer itMapCheckLRWallProcNoFloor gate returns FALSE first on an
     * all-(-2) coll_data, short-circuiting before ever reaching its own
     * attach_line_id check). And `attach_line_id` is a u16 field
     * (it/ittypes.h) -- the usual int-literal -2 sentinel this port's
     * itMap-adjacent tests rely on can NEVER be expressed through it (a
     * u16 promotes to an int in [0,65535], never -1 or -2), so
     * mpCollisionCheckExistLineID falls through to its real array-index
     * branch every time this function is called, not its debug
     * sentinels. Proven here with a real, minimal one-entry
     * MPVertexInfoContainer/MPYakumonoDObj pair instead -- the honest
     * way, not a landmine of an out-of-bounds read through a truncated
     * sentinel (caught before committing: a first draft set
     * attach_line_id=-2 directly, which u16-truncates to 65534 and reads
     * whatever gMPCollisionVertexInfo happened to still be from an
     * earlier test, silently passing or failing depending on leftover
     * global state rather than on this function's own logic). */
    {
        static MPVertexInfoContainer vinfo;
        static MPYakumonoDObj yak;
        static DObj yakdobj;
        MPVertexInfoContainer *save_vinfo = gMPCollisionVertexInfo;
        MPYakumonoDObj *save_yak = gMPCollisionYakumonoDObjs;

        memset(&vinfo, 0, sizeof(vinfo));
        memset(&yak, 0, sizeof(yak));
        memset(&yakdobj, 0, sizeof(yakdobj));
        vinfo.vertex_info[0].yakumono_id = 0;
        yak.dobjs[0] = &yakdobj;
        yakdobj.user_data.s = nMPYakumonoStatusOff; /* >= Off -> line no longer exists */
        gMPCollisionVertexInfo = &vinfo;
        gMPCollisionYakumonoDObjs = &yak;

        memset(&tip, 0, sizeof(tip));
        tip.attach_line_id = 0;
        r = itMSBombAttachedProcMap(&titem);
        CHECK(r == FALSE);
        CHECK(tip.is_attach_surface == FALSE);
        CHECK(tip.proc_update == itMSBombDetachedProcUpdate);

        gMPCollisionVertexInfo = save_vinfo;
        gMPCollisionYakumonoDObjs = save_yak;
    }

    /* itMSBombDetachedInitVars/SetStatus. */
    memset(&tip, 0, sizeof(tip));
    tip.damage_coll.hitstatus = nGMHitStatusNone;
    tip.attack_coll.attack_state = nGMAttackStateNew;
    tip.owner_gobj = &titem;
    itMSBombDetachedInitVars(&titem);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
    CHECK(tip.attack_coll.attack_state == nGMAttackStateOff);
    CHECK(tip.owner_gobj == NULL);

    memset(&tip, 0, sizeof(tip));
    itMSBombDetachedSetStatus(&titem);
    CHECK(tip.proc_update == itMSBombDetachedProcUpdate);
    CHECK(tip.proc_map == itMSBombDroppedProcMap);

    /* itMSBombAttachedProcUpdate/DetachedProcUpdate: the SAME
     * gGCCommonLinks[nGCCommonLinkIDFighter] proximity-walk shape
     * Bumper's own AttachedProcUpdate uses -- but driving a
     * detonation instead of a bounce. The arming clock (ip->multi vs
     * ITMSBOMB_DETECT_FIGHTER_DELAY) is proven on its own first (below
     * the delay, the fighter list is never even walked -- a NULL link
     * would segfault if it were); then a real two-fighter walk, only the
     * near one (inside ITMSBOMB_DETECT_FIGHTER_RADIUS, 400^2=160000
     * units) detonating. */
    memset(&tip, 0, sizeof(tip));
    tip.multi = 0;
    {
        GObj *save_fighter = gGCCommonLinks[nGCCommonLinkIDFighter];
        gGCCommonLinks[nGCCommonLinkIDFighter] = NULL; /* would crash if walked this frame */
        r = itMSBombAttachedProcUpdate(&titem);
        CHECK(r == FALSE);
        CHECK(tip.multi == 1);
        gGCCommonLinks[nGCCommonLinkIDFighter] = save_fighter;
    }

    {
        GObj *save_fighter = gGCCommonLinks[nGCCommonLinkIDFighter];
        GObj tfighter1, tfighter2;
        DObj tdobj_f1, tdobj_f2;
        FTStruct tfp1, tfp2;
        FTAttributes tfattr;
        static GObj cam_gobj2;
        static CObj cam_cobj2;
        GObj *prev_cam = gGMCameraGObj;
        s32 saved_bank = gEFManagerParticleBankID;
        void *save_common = gITManagerCommonData;

        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        memset(&tmobj, 0, sizeof(tmobj));
        memset(&tdobj_f1, 0, sizeof(tdobj_f1));
        memset(&tdobj_f2, 0, sizeof(tdobj_f2));
        memset(&tfp1, 0, sizeof(tfp1));
        memset(&tfp2, 0, sizeof(tfp2));
        memset(&tfattr, 0, sizeof(tfattr));
        memset(&cam_gobj2, 0, sizeof(cam_gobj2));
        memset(&cam_cobj2, 0, sizeof(cam_cobj2));
        tdobj.mobj = &tmobj;
        titem.obj = &tdobj;
        tdobj.translate.vec.f.x = 0.0F;
        tfp1.attr = &tfattr;
        tfp2.attr = &tfattr;
        tfighter1.obj = &tdobj_f1;
        tfighter1.user_data.p = &tfp1;
        tfighter2.obj = &tdobj_f2;
        tfighter2.user_data.p = &tfp2;
        tdobj_f1.translate.vec.f.x = 1000.0F; /* far: outside the radius */
        tdobj_f2.translate.vec.f.x = 100.0F;  /* near: 100^2=10000 < 160000, inside */
        tfighter1.link_next = &tfighter2;
        tfighter2.link_next = NULL;
        gGCCommonLinks[nGCCommonLinkIDFighter] = &tfighter1;
        tip.multi = ITMSBOMB_DETECT_FIGHTER_DELAY;
        tip.coll_data.mask_curr = 0; /* skip the floor-dust branch inside MakeEffect */

        cam_gobj2.obj = &cam_cobj2;
        gGMCameraGObj = &cam_gobj2;
        ef_pool_reset();
        gEFManagerParticleBankID = -1;
        gITManagerCommonData = sHostItemData;
        memset(host_attack_events(llITCommonDataMSBombAttackEvents), 0, sizeof(ITAttackEvent) * 4);

        r = itMSBombAttachedProcUpdate(&titem);
        CHECK(r == FALSE);
        CHECK(tip.proc_update == itMSBombExplodeProcUpdate); /* the near fighter detonated it */

        /* itMSBombDetachedProcUpdate: the identical walk, but gravity
         * applies too (this state is reached mid-air, not pinned). */
        memset(&tip, 0, sizeof(tip));
        tip.multi = ITMSBOMB_DETECT_FIGHTER_DELAY;
        tip.coll_data.mask_curr = 0;
        r = itMSBombDetachedProcUpdate(&titem);
        CHECK(r == FALSE);
        CHECK_NEAR(tip.physics.vel_air.y, -ITMSBOMB_GRAVITY, 1e-4F);
        CHECK(tip.proc_update == itMSBombExplodeProcUpdate);

        gITManagerCommonData = save_common;
        gEFManagerParticleBankID = saved_bank;
        gGMCameraGObj = prev_cam;
        gGCCommonLinks[nGCCommonLinkIDFighter] = save_fighter;
    }

    /* itMSBombExplodeInitVars/SetStatus/ProcUpdate: the timed hitbox
     * sequence's own bookends -- InitVars seeds multi/event_id/attack
     * defaults and fires UpdateAttackEvent's own first entry (proven
     * above); ProcUpdate advances multi every frame and destroys the
     * item outright (TRUE) once it reaches ITMSBOMB_EXPLODE_LIFETIME. */
    {
        void *save_common = gITManagerCommonData;
        gITManagerCommonData = sHostItemData;
        memset(host_attack_events(llITCommonDataMSBombAttackEvents), 0, sizeof(ITAttackEvent) * 4);

        memset(&tip, 0, sizeof(tip));
        tip.multi = 3;
        tip.event_id = 2;
        itMSBombExplodeInitVars(&titem);
        CHECK(tip.multi == 0);
        CHECK(tip.attack_coll.throw_mul == ITEM_THROW_DEFAULT);
        CHECK(tip.attack_coll.fgm_id == nSYAudioFGMExplodeL);
        CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone);

        memset(&tip, 0, sizeof(tip));
        tip.multi = ITMSBOMB_EXPLODE_LIFETIME - 1;
        r = itMSBombExplodeProcUpdate(&titem);
        CHECK(r == TRUE);
        CHECK(tip.multi == ITMSBOMB_EXPLODE_LIFETIME);

        memset(&tip, 0, sizeof(tip));
        tip.multi = 0;
        r = itMSBombExplodeProcUpdate(&titem);
        CHECK(r == FALSE);
        CHECK(tip.multi == 1);

        itMSBombExplodeSetStatus(&titem);
        CHECK(tip.proc_update == itMSBombExplodeProcUpdate);

        gITManagerCommonData = save_common;
    }
}

extern ITDesc dITBombHeiItemDesc;
extern ITStatusDesc dITBombHeiStatusDescs[];
extern intptr_t dITBombHeiDisplayListOffsets[];

enum
{
    nITBombHeiStatusWait, nITBombHeiStatusFall, nITBombHeiStatusHold,
    nITBombHeiStatusThrown, nITBombHeiStatusDropped, nITBombHeiStatusWalk,
    nITBombHeiStatusExplodeMap, nITBombHeiStatusExplode, nITBombHeiStatusExplodeWait
};

static void test_it_bombhei(void)
{
    GObj titem;
    ITStruct tip;
    DObj tdobj;
    MObj tmobj;
    ITAttributes tattr;
    sb32 r;

    CHECK(dITBombHeiItemDesc.kind == nITKindBombHei);
    CHECK(dITBombHeiItemDesc.p_file == &gITManagerCommonData);
    CHECK(dITBombHeiItemDesc.o_attributes == (intptr_t)0x424);  /* reloc_data.us.h: llITCommonDataBombHeiItemAttributes */
    CHECK(dITBombHeiItemDesc.proc_update == itBombHeiFallProcUpdate);
    CHECK(dITBombHeiItemDesc.proc_map == itBombHeiFallProcMap);
    CHECK(dITBombHeiItemDesc.proc_hit == NULL);

    /* the "unused?" DL-offsets table -- ported as genuine initialized
     * data even though nothing in this file (or the decomp) reads it. */
    CHECK(dITBombHeiDisplayListOffsets[0] == (intptr_t)0x3310);  /* reloc_data.us.h: llITCommonDataBombHeiWalkRightDisplayList */
    CHECK(dITBombHeiDisplayListOffsets[1] == (intptr_t)0x34c0);  /* reloc_data.us.h: llITCommonDataBombHeiWalkLeftDisplayList */

    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusWait].proc_update == itBombHeiWaitProcUpdate);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusWait].proc_hit == itBombHeiCommonProcHit);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusWait].proc_damage == itBombHeiCommonProcHit);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusHold].proc_update == NULL);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusThrown].proc_hop == itMainCommonProcHop);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusThrown].proc_reflector == itMainCommonProcReflector);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusDropped].proc_map == itBombHeiDroppedProcMap);
    /* Walk's own row wires the EXPLODE common-hit family, not the plain
     * one every other status uses -- a hit while walking detonates
     * through the dust-effect path, not a bare rebound. */
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusWalk].proc_update == itBombHeiWalkProcUpdate);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusWalk].proc_hit == itBombHeiExplodeCommonProcHit);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusWalk].proc_hop == NULL);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusExplodeMap].proc_update == itBombHeiExplodeMapProcUpdate);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusExplodeMap].proc_map == NULL);
    /* ExplodeMap wires the SAME function to update/hit/shield/reflector/
     * damage -- any interaction at all detonates it identically. */
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusExplodeMap].proc_hit == itBombHeiExplodeMapProcUpdate);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusExplodeMap].proc_reflector == itBombHeiExplodeMapProcUpdate);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusExplode].proc_update == itBombHeiExplodeProcUpdate);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusExplode].proc_map == NULL);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusExplodeWait].proc_update == itBombHeiExplodeWaitProcUpdate);
    CHECK(dITBombHeiStatusDescs[nITBombHeiStatusExplodeWait].proc_map == itBombHeiExplodeWaitProcMap);

    /* itBombHeiFallProcUpdate/ThrownProcUpdate: gravity + spin, the same
     * plain shape every prior falling item in this port has had. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tmobj, 0, sizeof(tmobj));
    tdobj.mobj = &tmobj;
    titem.user_data.p = &tip;
    titem.obj = &tdobj;
    tip.spin_step = 3.0F;
    r = itBombHeiFallProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITBOMBHEI_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 3.0F, 1e-4F);

    memset(&tip, 0, sizeof(tip));
    tip.spin_step = -2.0F;
    r = itBombHeiThrownProcUpdate(&titem);
    CHECK(r == FALSE);
    CHECK_NEAR(tip.physics.vel_air.y, -ITBOMBHEI_GRAVITY, 1e-4F);
    CHECK_NEAR(tdobj.rotate.vec.f.z, 1.0F, 1e-4F);

    /* itBombHeiWalkGetLR: sums a +1/-1 per fighter (which side of the
     * item it's on) into a single consensus direction -- driven over a
     * real two-fighter gGCCommonLinks chain, both fighters to the
     * item's RIGHT (so both contribute -1, consensus -2, which
     * itBombHeiWaitProcUpdate's own `lr < 0` branch reads as "walk
     * toward them", i.e. rightward). */
    {
        GObj *save_fighter = gGCCommonLinks[nGCCommonLinkIDFighter];
        GObj tfighter1, tfighter2;
        DObj tdobj_f1, tdobj_f2;

        memset(&tdobj_f1, 0, sizeof(tdobj_f1));
        memset(&tdobj_f2, 0, sizeof(tdobj_f2));
        tfighter1.obj = &tdobj_f1;
        tfighter2.obj = &tdobj_f2;
        tdobj.translate.vec.f.x = 0.0F;
        tdobj_f1.translate.vec.f.x = 100.0F;
        tdobj_f2.translate.vec.f.x = 200.0F;
        tfighter1.link_next = &tfighter2;
        tfighter2.link_next = NULL;
        gGCCommonLinks[nGCCommonLinkIDFighter] = &tfighter1;

        CHECK(itBombHeiWalkGetLR(&titem) == -2);

        gGCCommonLinks[nGCCommonLinkIDFighter] = save_fighter;
    }

    /* itBombHeiWaitProcMap/FallProcMap: honest no-collision compositions,
     * same shape as every prior itMap-adjacent test in this port. */
    memset(&tip, 0, sizeof(tip));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    r = itBombHeiWaitProcMap(&titem);
    CHECK(r == FALSE);
    memset(&tip.coll_data, 0, sizeof(tip.coll_data));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    r = itBombHeiFallProcMap(&titem);
    CHECK(r == FALSE);

    /* itBombHeiWaitSetStatus/FallSetStatus/HoldSetStatus/ThrownSetStatus/
     * DroppedSetStatus: field bookends, same shape every prior status
     * setter in this port has had. */
    memset(&tip, 0, sizeof(tip));
    itBombHeiWaitSetStatus(&titem);
    CHECK(tip.is_allow_pickup == TRUE);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
    CHECK(tip.proc_update == itBombHeiWaitProcUpdate);

    memset(&tip, 0, sizeof(tip));
    tip.is_allow_pickup = TRUE;
    itBombHeiFallSetStatus(&titem);
    CHECK(tip.is_allow_pickup == FALSE);
    CHECK(tip.ga == nMPKineticsAir);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
    CHECK(tip.proc_update == itBombHeiFallProcUpdate);

    memset(&tip, 0, sizeof(tip));
    itBombHeiHoldSetStatus(&titem);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone);
    CHECK(tip.proc_update == NULL);

    memset(&tip, 0, sizeof(tip));
    itBombHeiThrownSetStatus(&titem);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
    CHECK(tip.proc_map == itBombHeiThrownProcMap);

    memset(&tip, 0, sizeof(tip));
    itBombHeiDroppedSetStatus(&titem);
    CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
    CHECK(tip.proc_map == itBombHeiDroppedProcMap);

    /* itBombHeiThrownProcMap/DroppedProcMap: honest no-collision
     * compositions -- unlike Bumper's own thrown path, ANY landing at
     * all (not just a floor bounce) detonates rather than settling. */
    memset(&tip, 0, sizeof(tip));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    r = itBombHeiThrownProcMap(&titem);
    CHECK(r == FALSE);
    memset(&tip.coll_data, 0, sizeof(tip.coll_data));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    r = itBombHeiDroppedProcMap(&titem);
    CHECK(r == FALSE);

    /* itBombHeiCommonSetWalkLR: the direct DL swap and velocity kick --
     * driven directly, not just through its own callers, to prove both
     * branches (including the display-list swap the decomp's own
     * WaitProcUpdate asymmetrically skips on its OWN right-walk branch,
     * documented in the header comment above). tattr.data is pinned so
     * itGetPData's own subtraction cancels to exactly the DL stand-ins'
     * own addresses, the same technique every prior itGetPData-driven
     * test in this port uses. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    memset(&tattr, 0, sizeof(tattr));
    tattr.data = (void*)(sHostItemModels + llITCommonDataBombHeiDataStart);
    tip.attr = &tattr;
    titem.obj = &tdobj;
    itBombHeiCommonSetWalkLR(&titem, 1);
    CHECK(tip.lr == 1);
    CHECK_NEAR(tip.physics.vel_air.x, ITBOMBHEI_WALK_VEL_X, 1e-4F);
    CHECK(tdobj.dl == (Gfx*)(sHostItemModels + llITCommonDataBombHeiWalkRightDisplayList));

    itBombHeiCommonSetWalkLR(&titem, 0);
    CHECK(tip.lr == -1);
    CHECK_NEAR(tip.physics.vel_air.x, -ITBOMBHEI_WALK_VEL_X, 1e-4F);
    CHECK(tdobj.dl == (Gfx*)(sHostItemModels + llITCommonDataBombHeiWalkLeftDisplayList));

    /* itBombHeiWalkProcMap: the wall-touch branches, each routing back
     * through CommonSetWalkLR to reverse direction -- honest "no floor
     * line" composition for the outer itMapCheckLRWallProcNoFloor call
     * (same limitation every itMap-adjacent test in this port has had
     * has), but mask_curr driven directly for the two wall
     * checks that follow it unconditionally. */
    memset(&tip, 0, sizeof(tip));
    memset(&tdobj, 0, sizeof(tdobj));
    titem.obj = &tdobj;
    tip.attr = &tattr;
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    tip.coll_data.mask_curr = MAP_FLAG_LWALL;
    r = itBombHeiWalkProcMap(&titem);
    CHECK(r == FALSE);
    CHECK(tip.lr == -1); /* LWALL -> CommonSetWalkLR(0) -> lr=-1 */

    memset(&tip.coll_data, 0, sizeof(tip.coll_data));
    tip.coll_data.p_translate = &tdobj.translate.vec.f;
    tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
    tip.coll_data.pos_prev = tdobj.translate.vec.f;
    tip.coll_data.floor_line_id = -2;
    tip.coll_data.ceil_line_id = -2;
    tip.coll_data.lwall_line_id = -2;
    tip.coll_data.rwall_line_id = -2;
    tip.coll_data.ewall_line_id = -2;
    tip.coll_data.ignore_line_id = -2;
    tip.coll_data.mask_curr = MAP_FLAG_RWALL;
    r = itBombHeiWalkProcMap(&titem);
    CHECK(r == FALSE);
    CHECK(tip.lr == 1); /* RWALL -> CommonSetWalkLR(1) -> lr=+1 */

    /* Everything from here on funnels into itBombHeiCommonSetExplode's
     * own ef/-effect tail -- driven with the SAME "no bank loaded"
     * safety net the effect-manager test uses
     * (gEFManagerParticleBankID = -1) plus a scratch camera GObj for
     * efManagerQuakeMakeEffect's own GObj-based path, the identical
     * scaffold MSBomb's own explosion tests use. */
    {
        static GObj cam_gobj;
        static CObj cam_cobj;
        GObj *prev_cam = gGMCameraGObj;
        s32 saved_bank = gEFManagerParticleBankID;
        void *save_common = gITManagerCommonData;

        memset(&cam_gobj, 0, sizeof(cam_gobj));
        memset(&cam_cobj, 0, sizeof(cam_cobj));
        cam_gobj.obj = &cam_cobj;
        gGMCameraGObj = &cam_gobj;
        ef_pool_reset();
        gEFManagerParticleBankID = -1;
        gITManagerCommonData = sHostItemData;
        memset(host_attack_events(llITCommonDataBombHeiAttackEvents), 0, sizeof(ITAttackEvent) * 4);

        /* itBombHeiCommonCheckMakeDustEffect: floor-rest and override
         * branches, both safe under the "no bank" net. */
        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        memset(&tattr, 0, sizeof(tattr));
        titem.obj = &tdobj;
        tip.attr = &tattr;
        tip.coll_data.mask_curr = MAP_FLAG_FLOOR;
        tattr.map_coll_bottom = -20;
        tip.lr = 1;
        itBombHeiCommonCheckMakeDustEffect(&titem, FALSE);
        memset(&tip, 0, sizeof(tip));
        tip.attr = &tattr;
        tip.coll_data.mask_curr = 0;
        itBombHeiCommonCheckMakeDustEffect(&titem, TRUE); /* override forces it even off a floor */

        /* itBombHeiCommonSetHitStatusNormal/None: direct field flips. */
        memset(&tip, 0, sizeof(tip));
        itBombHeiCommonSetHitStatusNormal(&titem);
        CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
        itBombHeiCommonSetHitStatusNone(&titem);
        CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone);

        /* itBombHeiWalkUpdateEffect: the smoke-puff timer -- reaching 0
         * makes the effect and reseeds the delay, THEN decrements
         * regardless (so a fresh seed of ITBOMBHEI_SMOKE_WAIT comes back
         * as SMOKE_WAIT-1 the same frame it was set, not SMOKE_WAIT
         * itself). */
        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        titem.obj = &tdobj;
        tip.item_vars.bombhei.smoke_delay = 0;
        itBombHeiWalkUpdateEffect(&titem);
        CHECK(tip.item_vars.bombhei.smoke_delay == ITBOMBHEI_SMOKE_WAIT - 1);

        tip.item_vars.bombhei.smoke_delay = 5;
        itBombHeiWalkUpdateEffect(&titem);
        CHECK(tip.item_vars.bombhei.smoke_delay == 4);

        /* itBombHeiCommonSetExplode/CommonClearVelSetExplode/CommonProcHit/
         * ExplodeCommonProcHit/ExplodeMapProcUpdate: all funnel into the
         * same explosion tail (hide the model, refresh the attack coll,
         * clear owner stats, and land in Explode). */
        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        titem.obj = &tdobj;
        tip.physics.vel_air.x = 5.0F;
        tip.owner_gobj = &titem;
        r = itBombHeiCommonProcHit(&titem);
        CHECK(r == FALSE);
        CHECK_NEAR(tip.physics.vel_air.x, 0.0F, 1e-4F);
        CHECK(tdobj.flags == DOBJ_FLAG_HIDDEN);
        CHECK(tip.attack_coll.fgm_id == nSYAudioFGMExplodeL);
        CHECK(tip.owner_gobj == NULL);
        CHECK(tip.proc_update == itBombHeiExplodeProcUpdate);

        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        memset(&tattr, 0, sizeof(tattr));
        titem.obj = &tdobj;
        tip.attr = &tattr;
        tip.coll_data.mask_curr = 0;
        r = itBombHeiExplodeCommonProcHit(&titem);
        CHECK(r == FALSE);
        CHECK(tip.proc_update == itBombHeiExplodeProcUpdate);

        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        memset(&tattr, 0, sizeof(tattr));
        titem.obj = &tdobj;
        tip.attr = &tattr;
        tip.coll_data.mask_curr = 0;
        r = itBombHeiExplodeMapProcUpdate(&titem);
        CHECK(r == FALSE);
        CHECK(tip.proc_update == itBombHeiExplodeProcUpdate);
        CHECK(tip.damage_coll.hitstatus == nGMHitStatusNone); /* itBombHeiExplodeMapSetStatus's own tail is skipped -- this call lands straight in Explode */

        /* itBombHeiExplodeMapSetStatus: the field bookend on its own. */
        memset(&tip, 0, sizeof(tip));
        itBombHeiExplodeMapSetStatus(&titem);
        CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
        CHECK(tip.proc_update == itBombHeiExplodeMapProcUpdate);

        /* itBombHeiExplodeWaitProcUpdate/ProcMap/InitVars/SetStatus: the
         * stall state before detonating on its own timer.
         * ITBOMBHEI_EXPLODE_WAIT is a FLOAT (90.0F) compared against
         * ip->multi's own u16 -- an exact int/float comparison for a
         * value this small, not a promotion trap, confirmed directly
         * rather than assumed. itMainCheckSetColAnimID is the same
         * thin ftParamCheckSetColAnimID pass-through Hammer's own test
         * already proves -- no extra scaffold needed. */
        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        memset(&tmobj, 0, sizeof(tmobj));
        memset(&tattr, 0, sizeof(tattr));
        tdobj.mobj = &tmobj;
        titem.obj = &tdobj;
        tip.attr = &tattr; /* CheckMakeDustEffect's own override=TRUE path dereferences attr unconditionally */
        tmobj.matanim_joint.event32 = (AObjEvent32*)(sHostItemModels + llITCommonDataBombHeiWalkMatAnimJoint);
        tip.multi = ITBOMBHEI_EXPLODE_WAIT - 1;
        r = itBombHeiExplodeWaitProcUpdate(&titem);
        CHECK(r == FALSE);
        CHECK_NEAR(tip.multi, ITBOMBHEI_EXPLODE_WAIT, 1e-4F);

        tip.multi = (u16)ITBOMBHEI_EXPLODE_WAIT;
        r = itBombHeiExplodeWaitProcUpdate(&titem);
        CHECK(r == FALSE);
        CHECK(tdobj.flags == DOBJ_FLAG_HIDDEN); /* the timer fired -- detonated this frame */

        memset(&tip, 0, sizeof(tip));
        tip.coll_data.p_translate = &tdobj.translate.vec.f;
        tip.coll_data.p_map_coll = &tip.coll_data.map_coll;
        tip.coll_data.pos_prev = tdobj.translate.vec.f;
        tip.coll_data.floor_line_id = -2;
        tip.coll_data.ceil_line_id = -2;
        tip.coll_data.lwall_line_id = -2;
        tip.coll_data.rwall_line_id = -2;
        tip.coll_data.ewall_line_id = -2;
        tip.coll_data.ignore_line_id = -2;
        r = itBombHeiExplodeWaitProcMap(&titem);
        CHECK(r == FALSE);

        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        memset(&tmobj, 0, sizeof(tmobj));
        tdobj.mobj = &tmobj;
        titem.obj = &tdobj;
        tmobj.matanim_joint.event32 = (AObjEvent32*)(sHostItemModels + llITCommonDataBombHeiWalkMatAnimJoint);
        itBombHeiExplodeWaitInitVars(&titem);
        CHECK(tip.multi == 0);
        CHECK(tmobj.matanim_joint.event32 == NULL);

        memset(&tip, 0, sizeof(tip));
        itBombHeiExplodeWaitSetStatus(&titem);
        CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
        CHECK(tip.proc_update == itBombHeiExplodeWaitProcUpdate);

        /* itBombHeiCommonUpdateAttackEvent: the same `*p_file + off`
         * read MSBomb's own test proves, over the faked ITCommonData --
         * see that one for the shape. */
        memset(&tip, 0, sizeof(tip));
        memset(host_attack_events(llITCommonDataBombHeiAttackEvents), 0, sizeof(ITAttackEvent) * 4);
        host_attack_events(llITCommonDataBombHeiAttackEvents)[0].timer = 7;
        host_attack_events(llITCommonDataBombHeiAttackEvents)[0].angle = 200;
        host_attack_events(llITCommonDataBombHeiAttackEvents)[0].damage = 15;
        host_attack_events(llITCommonDataBombHeiAttackEvents)[0].size = 40;
        tip.multi = 7;
        tip.event_id = 0;
        itBombHeiCommonUpdateAttackEvent(&titem);
        CHECK(tip.attack_coll.angle == 200);
        CHECK(tip.attack_coll.damage == 15);
        CHECK(tip.attack_coll.size == 40);
        CHECK(tip.attack_coll.element == nGMHitElementFire);
        CHECK(tip.event_id == 1);

        tip.event_id = 3;
        tip.multi = 0;
        host_attack_events(llITCommonDataBombHeiAttackEvents)[3].timer = 0;
        itBombHeiCommonUpdateAttackEvent(&titem);
        CHECK(tip.event_id == 3); /* wraps from 4 back to 3 */

        /* itBombHeiExplodeInitVars/SetStatus/ProcUpdate: the timed
         * hitbox sequence's own bookends. */
        memset(&tip, 0, sizeof(tip));
        tip.multi = 3;
        tip.event_id = 2;
        itBombHeiExplodeInitVars(&titem);
        CHECK(tip.multi == 0);
        CHECK(tip.attack_coll.throw_mul == ITEM_THROW_DEFAULT);

        memset(&tip, 0, sizeof(tip));
        tip.multi = ITBOMBHEI_EXPLODE_LIFETIME - 1;
        r = itBombHeiExplodeProcUpdate(&titem);
        CHECK(r == TRUE);
        CHECK(tip.multi == ITBOMBHEI_EXPLODE_LIFETIME);

        memset(&tip, 0, sizeof(tip));
        tip.multi = 0;
        r = itBombHeiExplodeProcUpdate(&titem);
        CHECK(r == FALSE);
        CHECK(tip.multi == 1);

        itBombHeiExplodeSetStatus(&titem);
        CHECK(tip.proc_update == itBombHeiExplodeProcUpdate);

        /* itBombHeiWalkInitVars/WalkSetStatus: the walk state's own
         * setup -- the anim/MObj scaffold Green/Red Shell's own Spin
         * machinery established (DOBJ_PARENT_NULL, a real ITAttributes
         * with `.data` pinned so itGetPData's own subtraction cancels to
         * exactly the mat-anim joint's own offset in the faked model
         * file), plus
         * itMainRefreshAttackColl (a pure ITStruct/DObj touch, no extra
         * scaffold beyond what's already wired) and the FGM call
         * (already proven safe everywhere else in this port). */
        memset(&tip, 0, sizeof(tip));
        memset(&tdobj, 0, sizeof(tdobj));
        memset(&tmobj, 0, sizeof(tmobj));
        memset(&tattr, 0, sizeof(tattr));
        tdobj.parent = DOBJ_PARENT_NULL;
        tdobj.parent_gobj = &titem;
        tdobj.mobj = &tmobj;
        tattr.data = (void*)(sHostItemModels + llITCommonDataBombHeiDataStart);
        tip.attr = &tattr;
        titem.obj = &tdobj;
        tip.coll_data.floor_line_id = -2;
        tip.lr = -1;
        itBombHeiWalkSetStatus(&titem);
        CHECK(tip.damage_coll.hitstatus == nGMHitStatusNormal);
        CHECK(tip.is_allow_pickup == FALSE);
        CHECK(tip.multi == 0);
        CHECK(tip.item_vars.bombhei.smoke_delay == ITBOMBHEI_SMOKE_WAIT);
        CHECK(tmobj.matanim_joint.event32 == (AObjEvent32*)(sHostItemModels + llITCommonDataBombHeiWalkMatAnimJoint));
        CHECK(tip.owner_gobj == NULL);
        CHECK(tip.proc_update == itBombHeiWalkProcUpdate);

        /* itBombHeiWaitProcUpdate: below the threshold just increments;
         * at the threshold it derives lr from WalkGetLR (no fighters on
         * the link this time -> 0 -> the RNG tie-break, uncontrollable
         * here, so only the multi-increment side is proven this way --
         * the lr<0/lr>=0 branches themselves are already proven directly
         * above via CommonSetWalkLR) and tails into WalkSetStatus, whose
         * own effects are proven above too. */
        {
            GObj *save_fighter2 = gGCCommonLinks[nGCCommonLinkIDFighter];

            memset(&tip, 0, sizeof(tip));
            tip.attr = &tattr; /* tail (WalkSetStatus -> WalkInitVars) dereferences attr->data unconditionally */
            tip.multi = ITBOMBHEI_WALK_WAIT - 1;
            gGCCommonLinks[nGCCommonLinkIDFighter] = NULL;
            r = itBombHeiWaitProcUpdate(&titem);
            CHECK(r == FALSE);
            CHECK(tip.multi == ITBOMBHEI_WALK_WAIT);
            CHECK(tip.proc_update == NULL); /* not yet -- one frame early, the if-block never ran */

            r = itBombHeiWaitProcUpdate(&titem);
            CHECK(r == FALSE);
            CHECK(tip.proc_update == itBombHeiWalkProcUpdate); /* threshold reached -- walking now */

            gGCCommonLinks[nGCCommonLinkIDFighter] = save_fighter2;
        }

        gITManagerCommonData = save_common;
        gEFManagerParticleBankID = saved_bank;
        gGMCameraGObj = prev_cam;
    }
}

extern GMRumbleEventDefault dGMRumbleEvent0[];
extern GMRumbleEventDefault dGMRumbleEvent1[];
extern GMRumbleEventDefault dGMRumbleEvent4[];
extern GMRumbleEventDefault *dGMRumbleEventList[11];
extern u8 dGMRumblePriorities[11];
extern GMRumbleScript sGMRumbleScripts[];
extern GMRumbleLink sGMRumbleLinks[];
extern GMRumblePlayer sGMRumblePlayers[];
extern void gmRumbleSetPlayerRumbleParams(s32 player, s32 rumble_id, s32 rumble_timer);
extern void gmRumbleStopRumbleID(s32 player, s32 rumble_id);
extern void gmRumbleMakeActor(void);
extern void gmRumbleInitPlayers(void);
extern void gmRumbleResumeProcessAll(void);
extern void gmRumbleActorProcUpdate(GObj *rumble_gobj);
extern void func_ovl2_801155C4(s32 player);
extern s32 gSYControllerRumbleStartCount[GMCOMMON_PLAYERS_MAX];
extern s32 gSYControllerRumbleStopCount[GMCOMMON_PLAYERS_MAX];

/* src/dc/gmrumble.c: gm/gmrumble.c, whole -- the Rumble Pak
 * scripted-event engine (fifteen functions) plus this file's own
 * syControllerStart/Stop/InitRumble (DIVERGES: the DC's purupuru Jump
 * Pack stands in for the N64's rumble pak; there is no maple bus on the
 * host, so those three just count calls into gSYControllerRumbleStart/
 * StopCount -- the same "mirror the target effect with a counter" shape
 * ColAnim env-colour capture used).
 *
 * gmRumbleMakeActor runs exactly ONCE for this whole file's run, in
 * main() right after load_stage() -- NOT here. ftParamMakeRumble
 * (ft/ftparam.c, already ported, already called from all over the
 * fighter damage/death/capture/thrown/item-shoot code this file
 * exercises) reaches gmRumbleSetPlayerRumbleParams for real now that
 * the old stub is gone, and that dereferences
 * sGMRumblePlayers[player].rlink unconditionally the first time ANY
 * test fires a hit -- so the link-chain setup has to exist before the
 * very first such test runs, not just before this one. By the time
 * THIS test runs, other tests have very likely already driven real
 * events through players 0-3, so nothing here assumes a pristine
 * just-made chain -- every player touched below is reset first via
 * func_ovl2_801155C4 (Init+Stop the motor, clear every queued event),
 * the same reset primitive gmRumbleInitPlayers itself is built from.
 *
 * gmRumbleInitPlayers/ResumeProcessAll's own direct callers in
 * scvsbattle.c/ifcommon.c/mntitle.c/scvsresults.c are wired,
 * and test_rumble_safety_nets below
 * covers their contract -- see that test's own header.
 * ftParamMakeRumble's call sites are wired too (see gmrumble.c's own header and ftparam.c).
 *
 * Not tested here, an honest limitation and not a gap: gmRumbleCheckSetEventID's
 * own "extend a running event's timer" branch and
 * gmRumbleGetEventPriorityRelink's slot-eviction pointer surgery under
 * a FULL three-slot queue both need a second/third concurrently-live
 * event on the same player, which the coverage below (one event live
 * at a time) never produces. */
static void test_gm_rumble(void)
{
    GObj *rumble_gobj;
    GMRumbleLink *rlink;
    s32 player, i;

    /* ---- static data: a spot check against the decomp's own tables ---- */
    CHECK(ARRAY_COUNT(dGMRumbleEventList) == 11);
    CHECK(dGMRumbleEvent0[0].opcode == nGMRumbleEventStartRumble);
    CHECK(dGMRumbleEvent0[0].param == 8000);
    CHECK(dGMRumbleEvent0[1].opcode == nGMRumbleEventEnd);
    CHECK(dGMRumbleEvent1[0].opcode == nGMRumbleEventLoopBegin);
    CHECK(dGMRumbleEvent1[0].param == 8000);
    CHECK(dGMRumbleEvent1[1].opcode == nGMRumbleEventStartRumble);
    CHECK(dGMRumbleEvent1[1].param == 2);
    CHECK(dGMRumbleEvent1[2].opcode == nGMRumbleEventStopRumble);
    CHECK(dGMRumbleEvent1[2].param == 2);
    CHECK(dGMRumbleEvent1[3].opcode == nGMRumbleEventLoopEnd);
    CHECK(dGMRumbleEvent1[4].opcode == nGMRumbleEventEnd);
    CHECK(dGMRumblePriorities[0] == 10);
    CHECK(dGMRumblePriorities[1] == 9);
    CHECK(dGMRumblePriorities[10] == 3);

    /* the actor GObj is on link 13 (shared with fighter shadows/
     * transitions/wallpaper) but is NOT necessarily its head by now --
     * every later GObj added to that link becomes the new head, so
     * find it by nGCCommonKindRumble, the same walk gmRumbleResumeProcessAll
     * itself does below. */
    rumble_gobj = gGCCommonLinks[nGCCommonLinkIDRumble];
    while ((rumble_gobj != NULL) && (rumble_gobj->id != nGCCommonKindRumble))
    {
        rumble_gobj = rumble_gobj->link_next;
    }
    CHECK(rumble_gobj != NULL);

    /* force every player idle first -- other tests before this one may
     * already have driven real rumble events through any of them. */
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        func_ovl2_801155C4(player);
    }

    /* ---- each player's own three-slot link chain (GMRUMBLE_ARRAY_COLS)
     * -- linear next, wrapped prev (the head's own rprev is the tail,
     * matching gmRumbleAddLinkAfter's own reinsert-at-tail arithmetic).
     * This shape is an invariant of the chain's own topology, preserved
     * by every one of AddLinkAfter/GetEventPriorityRelink's pointer
     * surgeries -- true regardless of which physical slot is currently
     * "head" -- so it still holds here even though gmRumbleMakeActor
     * itself ran once, in main(), long before this test. ---- */
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        CHECK(sGMRumblePlayers[player].is_active == FALSE);
        CHECK(sGMRumblePlayers[player].rlink != NULL);
        CHECK(sGMRumblePlayers[player].rlink->p_script->p_event == NULL);

        rlink = sGMRumblePlayers[player].rlink;
        for (i = 1; i < GMRUMBLE_ARRAY_COLS; i++)
        {
            CHECK(rlink->rnext != NULL);
            CHECK(rlink->rnext->rprev == rlink);
            CHECK(rlink->rnext->p_script->p_event == NULL);
            rlink = rlink->rnext;
        }
        CHECK(rlink->rnext == NULL);                            /* the tail: linear, not circular */
        CHECK(sGMRumblePlayers[player].rlink->rprev == rlink);  /* head's own rprev wraps to the tail */
    }

    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        gSYControllerRumbleStartCount[i] = 0;
        gSYControllerRumbleStopCount[i] = 0;
    }

    /* ---- gmRumbleSetPlayerRumbleParams: wiring a fresh event into the
     * idle head slot -- gmRumbleCheckSetEventID finds nothing playing,
     * gmRumbleGetEventPriorityRelink's own `p_event == NULL` branch
     * returns the head's script at once, no pointer surgery needed. ---- */
    gmRumbleSetPlayerRumbleParams(0, 0, 0);

    rlink = sGMRumblePlayers[0].rlink;
    CHECK(rlink->p_script->p_event == dGMRumbleEvent0);
    CHECK(rlink->p_script->rumble_id == 0);
    CHECK(rlink->p_script->rumble_timer == -1);   /* rumble_timer == 0 in -> "run forever" sentinel */
    CHECK(rlink->p_script->is_rumble_active == FALSE);
    CHECK(rlink->p_script->rumble_status == 0);

    /* ---- gmRumbleActorProcUpdate: hand-verified against dGMRumbleEvent0
     * = { StartRumble 8000, End 0 } -- one tic drains the interpreter
     * through the zero-duration opcode to the first nonzero
     * rumble_status, firing exactly one Start on the way, then holds
     * (Event0 never stops itself). ---- */
    gmRumbleActorProcUpdate(rumble_gobj);
    CHECK(gSYControllerRumbleStartCount[0] == 1);
    CHECK(gSYControllerRumbleStopCount[0] == 0);
    CHECK(sGMRumblePlayers[0].is_active == TRUE);
    CHECK(rlink->p_script->rumble_status == 7999);

    for (i = 0; i < 5; i++)
    {
        gmRumbleActorProcUpdate(rumble_gobj);
    }
    CHECK(gSYControllerRumbleStartCount[0] == 1);   /* still just the one Start -- no re-trigger */
    CHECK(rlink->p_script->rumble_status == 7994);
    CHECK(sGMRumblePlayers[0].is_active == TRUE);

    /* ---- gmRumbleStopRumbleID: ends it early by rumble_id, freeing the
     * slot (p_event -> NULL, moved to the tail by gmRumbleAddLinkAfter)
     * and, through gmRumbleGetMotorUpdateStatus's own "just went idle"
     * branch, the one Stop that turns the motor back off. ---- */
    gmRumbleStopRumbleID(0, 0);
    CHECK(gSYControllerRumbleStopCount[0] == 1);
    CHECK(sGMRumblePlayers[0].is_active == FALSE);
    CHECK(sGMRumblePlayers[0].rlink->p_script->p_event == NULL);

    /* a second StopRumbleID on an already-idle player is the decomp's
     * own no-op guard (`if (rlink->p_script->p_event != NULL)`) */
    gmRumbleStopRumbleID(0, 0);
    CHECK(gSYControllerRumbleStopCount[0] == 1);

    /* ---- gmRumbleInitPlayers: unconditional Init+Stop for all four,
     * regardless of whether anything was playing ---- */
    gmRumbleInitPlayers();
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        CHECK(gSYControllerRumbleStopCount[player] >= 1);
    }

    /* ---- gmRumbleResumeProcessAll: walks link 13 filtering by
     * nGCCommonKindRumble -- just needs to not crash on a process that
     * was never paused. ---- */
    gmRumbleResumeProcessAll();

    /* ---- func_ovl2_801155C4: the decomp's own "Unused?" function,
     * ported per this port's byte-for-byte doctrine -- Init+Stop the
     * motor and clear every one of this player's own queued events,
     * whether or not the motor was actually on. Re-zeroed here: the
     * gmRumbleInitPlayers call above already touched player 1's own
     * StopCount as a side effect (it stops all four unconditionally),
     * and this section's own assertions should not depend on that. ---- */
    gSYControllerRumbleStartCount[1] = 0;
    gSYControllerRumbleStopCount[1] = 0;
    gmRumbleSetPlayerRumbleParams(1, 4, 0);
    CHECK(sGMRumblePlayers[1].rlink->p_script->p_event != NULL);
    gmRumbleActorProcUpdate(rumble_gobj);
    CHECK(gSYControllerRumbleStartCount[1] == 1);
    CHECK(sGMRumblePlayers[1].is_active == TRUE);

    func_ovl2_801155C4(1);
    CHECK(gSYControllerRumbleStopCount[1] == 1);
    CHECK(sGMRumblePlayers[1].is_active == FALSE);
    rlink = sGMRumblePlayers[1].rlink;
    while (rlink != NULL)
    {
        CHECK(rlink->p_script->p_event == NULL);
        rlink = rlink->rnext;
    }

    /* NOT torn down: the actor GObj and sGMRumblePlayers[] are set up
     * once in main() for this whole file's run (see this function's own
     * header comment) -- ejecting it here would break every later test
     * that fires a real hit through ftParamMakeRumble the same way the
     * missing setup itself did before this test added the reset above. */
}

extern void func_ovl31_80137898(GObj *gobj);
extern void func_ovl31_80137938(void);
extern void func_ovl31_8013797C(void);
extern void gcPauseGObjProcessAll(GObj *gobj);

/* The three direct callers outside the fighters that are wired:
 * gmRumbleInitPlayers (the pause, the reset
 * and the GAME SET/TIME UP banner's own freeze, in ifcommon.c and
 * scvsbattle.c), gmRumbleResumeProcessAll (that banner letting go) and
 * the results screen's own pair (scvsresults.c).
 *
 * Only what each one DOES is checked here; that the call sites call them
 * is a thing the target run shows, not the host, which never reaches
 * those scenes -- there is no pause menu to press and no results screen
 * to arrive at. What is checked is the contract the wiring leans on:
 * InitPlayers and 8013797C stop all four pads whether or not anything is
 * buzzing, ResumeProcessAll actually un-freezes a live event, and
 * 80137938's thread alternates the winner's motor for exactly the two
 * seconds the game says and then ejects itself.
 *
 * Runs right after test_gm_rumble in main(), so the results statics are
 * still zero and mnVSResultsGetWinPlayer's free-for-all arm returns 0 --
 * but it is asked rather than assumed, and the thread section is skipped
 * with a note if the state says otherwise. */
static void test_rumble_safety_nets(void)
{
    GObj *rumble_gobj;
    GObj *link_before;
    GMRumbleScript *p_script;
    s32 start_before[GMCOMMON_PLAYERS_MAX];
    s32 stop_before[GMCOMMON_PLAYERS_MAX];
    s32 i, tic, winner;

    /* the actor, found the same way test_gm_rumble finds it: link 13 is
     * shared and the rumble GObj is not necessarily its head by now */
    rumble_gobj = gGCCommonLinks[nGCCommonLinkIDRumble];
    while ((rumble_gobj != NULL) && (rumble_gobj->id != nGCCommonKindRumble))
    {
        rumble_gobj = rumble_gobj->link_next;
    }
    CHECK(rumble_gobj != NULL);

    /* ---- gmRumbleInitPlayers: Init then Stop, all four, whether or not
     * anything was playing (it does not look at is_active). ---- */
    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        gSYControllerRumbleStartCount[i] = 0;
        gSYControllerRumbleStopCount[i] = 0;
    }
    gmRumbleInitPlayers();
    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        /* no Start at all: syControllerInitRumble is the port's own
         * no-op (KOS registers the purupuru driver at boot, src/dc/
         * gmrumble.c), so the only call that reaches the pads is the
         * Stop behind it. */
        CHECK(gSYControllerRumbleStartCount[i] == 0);
        CHECK(gSYControllerRumbleStopCount[i] == 1);
    }

    /* ---- func_ovl31_8013797C: the results screen's own copy of that
     * loop, over GMCOMMON_PLAYERS_MAX rather than the pad array. ---- */
    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        gSYControllerRumbleStartCount[i] = 0;
        gSYControllerRumbleStopCount[i] = 0;
    }
    func_ovl31_8013797C();
    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        CHECK(gSYControllerRumbleStartCount[i] == 0);
        CHECK(gSYControllerRumbleStopCount[i] == 1);
    }

    /* ---- gmRumbleResumeProcessAll: the GAME SET banner's freeze takes
     * the rumble actor down with everything else, and this is what lets
     * it back in. Freeze it the way ifCommonBattleInterfacePauseGObj
     * does (gcPauseGObjProcessAll, no GOBJ_FLAG_NORUN -- the resume is a
     * process resume and nothing else), then watch a live event stop
     * dead and start again. ---- */
    func_ovl2_801155C4(0);
    gmRumbleSetPlayerRumbleParams(0, 0, 0);

    gcRunAll();
    p_script = sGMRumblePlayers[0].rlink->p_script;
    CHECK(p_script->p_event != NULL);
    CHECK(p_script->rumble_status == 7999);   /* dGMRumbleEvent0's StartRumble 8000, one tic in */

    gcPauseGObjProcessAll(rumble_gobj);
    for (i = 0; i < 3; i++)
    {
        gcRunAll();
    }
    CHECK(p_script->rumble_status == 7999);   /* frozen: not one tic of it moved */

    gmRumbleResumeProcessAll();
    gcRunAll();
    CHECK(p_script->rumble_status == 7998);   /* and it is running again */

    gmRumbleStopRumbleID(0, 0);
    func_ovl2_801155C4(0);

    /* ---- func_ovl31_80137938 -> func_ovl31_80137898: the winner's pad,
     * every other tic, for two seconds (120 tics at 60Hz), then the
     * thread ejects its own GObj. ---- */
    winner = mnVSResultsGetWinPlayer();

    if ((winner < 0) || (winner >= GMCOMMON_PLAYERS_MAX))
    {
        printf("rumble safety nets: mnVSResultsGetWinPlayer returned %d, "
               "the results thread not driven\n", (int)winner);
        return;
    }

    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        start_before[i] = gSYControllerRumbleStartCount[i];
        stop_before[i]  = gSYControllerRumbleStopCount[i];
    }
    /* what the thread's GObj goes on; the count is compared at the end
     * rather than the pointer, since gcMakeGObjSPAfter inserts after an
     * existing head rather than in front of it */
    link_before = gGCCommonLinks[nGCCommonLinkIDPlayerSelect];

    func_ovl31_80137938();

    /* Twenty tics first, then on to the eject. The Starts are exact: odd
     * tics 1..19 give 10, and the whole two seconds is odd tics 1..119,
     * 60, with no tic 121 -- the thread stops counting where the game
     * says it does. The first wake is tic 1 because the tic a process is
     * added on is tic 0 of gcRunAll's own turn, as test_gobj_threads
     * shows.
     *
     * The Stops are a lower bound and not an equality, and it is worth
     * writing down why rather than pretending otherwise: the pad
     * counters are global, and a gcRunAll window is a live world. The
     * thread owes 10 over the first window (even tics 2..20) and 60 over
     * the second (even tics 2..118 plus its own tic-120 Stop, which
     * takes the place of that tic's `else` arm -- gcEjectGObj(NULL) took
     * the GObj down at the end of the run the sleep never came back
     * from). Measured over 130 tics this test sees 61, one more than the
     * thread owes, and the extra is not the thread's: over a window that
     * long the scene's own fighter code reaches these same counters --
     * ftmain.c's motion-event MakeRumble/StopRumble cases, ftParamMakeRumble
     * under every hit -- and one such Stop landed on that pad. Nothing
     * else Starts a motor in this window, so the Starts stay exact. This
     * is the same "the world is running too" hazard test_gm_rumble
     * already hit from the other side, and it is why that test re-zeroes
     * before each of its own sections. */
    for (tic = 0; tic < 20; tic++)
    {
        gcRunAll();
    }
    CHECK(gSYControllerRumbleStartCount[winner] - start_before[winner] == 10);
    CHECK(gSYControllerRumbleStopCount[winner] - stop_before[winner] >= 10);

    for (; tic < 130; tic++)
    {
        gcRunAll();
    }
    CHECK(gSYControllerRumbleStartCount[winner] - start_before[winner] == 60);
    CHECK(gSYControllerRumbleStopCount[winner] - stop_before[winner] >= 60);

    /* and nobody else's pad was touched */
    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        if (i != winner)
        {
            CHECK(gSYControllerRumbleStartCount[i] == start_before[i]);
            CHECK(gSYControllerRumbleStopCount[i] == stop_before[i]);
        }
    }
    /* the thread ejected itself at tic 120 and its link reads as before */
    CHECK(gGCCommonLinks[nGCCommonLinkIDPlayerSelect] == link_before);
}
