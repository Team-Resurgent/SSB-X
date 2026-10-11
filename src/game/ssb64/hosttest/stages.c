/* hosttest/stages.c -- part of hosttest_ft.c: the stages: particle banks built on the host, the map file, every
 * stage's gr/ logic, the bonus stages, and the collision
 * line-existence contract.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

extern void ftCommonTwisterSetStatus(GObj *fighter_gobj, GObj *hazard_gobj);
extern void ftCommonTwisterProcUpdate(GObj *fighter_gobj);
extern void ftCommonTwisterProcPhysics(GObj *fighter_gobj);

/* The four arrays lbParticleSetupBankID writes and the interpreter
 * reads. They are file-scope in src/dc/lbparticle.c and named here for
 * the one thing that can install a bank on this machine -- see
 * host_load_particle_bank below. */
extern LBScript **sLBParticleScriptBanks[];
extern LBTexture **sLBParticleTextureBanks[];
extern s32 sLBParticleScriptBanksNum[];
extern s32 sLBParticleTextureBanksNum[];

/* grhyrule.c's own enum, mirrored so the checks below read as names
 * rather than numbers. The file's copy is not in a header because
 * nothing outside it indexes one. */
enum
{
    kTwisterSleep, kTwisterWait, kTwisterSummon, kTwisterMove,
    kTwisterTurn, kTwisterStop, kTwisterSubside
};

/* --- a particle bank, built at this machine's own width -------------
 *
 * The game's banks are ROM images of 32-bit structures: LBScriptDesc and
 * LBTextureDesc hold `T *[]` over what the file holds as a `u32[]` of
 * offsets into itself, and lbParticleSetupBankID walks a freshly loaded
 * bank pointerizing them in place. On x86-64 a pointer is eight bytes
 * where that walk assumes four, so it reads and writes at twice the
 * stride and off the end of the block: no bank can be loaded here at
 * all. src/dc/efmanager.c's efManagerLoadEffectBank has the long version
 * of why, and each of the port's three bank loads is `#ifdef
 * FT_HOSTTEST`ed around it for the same reason: the common bank's
 * (src/dc/efmanager.c, the wrapper scvsbattle.c calls), the per-fighter
 * ones' (src/dc/ftmanager.c), and Hyrule's own (src/dc/grhyrule.c,
 * below).
 *
 * So this builds one by hand instead, at the width this machine actually
 * has. Both files are read whole and their own tables decoded as the
 * u32s they are. A script's record can be pointed at where it lies in
 * the scb buffer, because LBScript is pointer-free for its first 0x30
 * bytes -- `bytecode[]` is the only pointer and it is last. A texture
 * needs a copy: LBTexture is pointer-free for its first 0x18 bytes and
 * then holds `void *data[]`, so the header is copied out and data[]
 * rebuilt pointing into the txb buffer.
 *
 * Everything downstream then runs exactly the code it runs on the
 * target: lbParticleMakeScriptID, the generator links, and the bytecode
 * interpreter itself, over the real scripts of the real bank. What is
 * not real is only where the two buffers came from -- the extracted
 * files, through the same asset_read_whole the rest of the suite uses.
 *
 * The two buffers are deliberately not freed: the script records point
 * into the scb one for as long as the bank is installed. Bank slot
 * `bank_id` is overwritten; a caller that cares saves the four globals
 * around it (test_gr_hyrule does). */
static void host_load_particle_bank(s32 bank_id, const char *scb_name,
                                    const char *txb_name)
{
    long scb_size, txb_size;
    u8 *scb = asset_read_whole(scb_name, &scb_size);
    u8 *txb = asset_read_whole(txb_name, &txb_size);
    LBScript **scripts;
    LBTexture **textures;
    u32 nscripts, ntextures, off;
    u32 i, j;

    CHECK(scb != NULL);
    CHECK(txb != NULL);

    if (scb == NULL || txb == NULL)
    {
        return;
    }

    memcpy(&nscripts, scb, sizeof(nscripts));
    CHECK(nscripts > 0);

    scripts = malloc(nscripts * sizeof(*scripts));
    CHECK(scripts != NULL);

    for (i = 0; i < nscripts; i++)
    {
        memcpy(&off, scb + 4 + i * 4, sizeof(off));
        /* inside the block, and past the offset array it is in */
        CHECK(off >= 4 + nscripts * 4);
        CHECK(off + 0x30 <= (u32)scb_size);
        scripts[i] = (LBScript *)(scb + off);
    }

    memcpy(&ntextures, txb, sizeof(ntextures));
    CHECK(ntextures > 0);

    textures = malloc(ntextures * sizeof(*textures));
    CHECK(textures != NULL);

    for (i = 0; i < ntextures; i++)
    {
        u32 count, fmt, flags, ndata;
        LBTexture *tx;

        memcpy(&off, txb + 4 + i * 4, sizeof(off));
        CHECK(off >= 4 + ntextures * 4);
        CHECK(off + 0x18 <= (u32)txb_size);

        memcpy(&count, txb + off, sizeof(count));
        memcpy(&fmt, txb + off + 4, sizeof(fmt));
        memcpy(&flags, txb + off + 0x14, sizeof(flags));

        /* lbParticleSetupBankID's own rule for how many words follow the
         * header: one per image, plus a palette each -- or a single
         * shared palette after them when flags bit 0 says so. */
        ndata = count;

        if (fmt == G_IM_FMT_CI)
        {
            ndata = (flags & 1) ? count + 1 : count * 2;
        }
        tx = malloc(0x18 + ndata * sizeof(tx->data[0]));
        CHECK(tx != NULL);

        memcpy(tx, txb + off, 0x18);

        for (j = 0; j < ndata; j++)
        {
            u32 doff;

            memcpy(&doff, txb + off + 0x18 + j * 4, sizeof(doff));
            tx->data[j] = txb + doff;
        }
        textures[i] = tx;
    }

    sLBParticleScriptBanks[bank_id] = scripts;
    sLBParticleScriptBanksNum[bank_id] = (s32)nscripts;
    sLBParticleTextureBanks[bank_id] = textures;
    sLBParticleTextureBanksNum[bank_id] = (s32)ntextures;
}

/* ---- the stage's map file (MPK1) ------------------------
 *
 * src/dc/stage.c is the PVR loader and is not in this build, so the three
 * calls the port's grjungle.c makes into it are here, over a Stage the
 * test fills with host_load_map_file below. That helper reads the MPK1
 * block of the stage's own .stg the way the target's stage_load_map does
 * -- same fields, same fixups, same item weights -- so what the test
 * drives through grJungleMakeTaruCann is the bytes the exporter wrote,
 * not a fixture that looks like them.
 *
 * The one thing that is not the target's: dc_model_add_dobjs is called
 * over gStageBound's own map_model instead of sStage's, which is the same
 * pack either way. */

static void *sHostMapBlob;
static uint32_t sHostMapWords[2048];
static AObjEvent32 *sHostMapAnims[STAGE_MAP_ANIMS_MAX];
static const char *sHostMapAnimNames[STAGE_MAP_ANIMS_MAX];
/* which joint of its tree each script goes on: the same field
 * src/dc/stage.c's Stage carries, and the reason Dream Land's eyes
 * animate at all -- see stage.h's own note on it. */
static int sHostMapAnimJoints[STAGE_MAP_ANIMS_MAX];
static int sHostMapAnimCount;
static uint8_t sHostMapWeights[STAGE_ITEM_WEIGHTS_COUNT];
static int sHostMapHasWeights;
/* The per-DObj array stage_map_anim_array fills, shared by every test
 * that checks one. File scope rather than a local: it is 256 bytes of
 * pointers, and the stage tests are large frames already. */
static AObjEvent32 *sHostAnimArray[STAGE_MAP_JOINTS_MAX];
static s32 sHostMapAttack[STAGE_ATTACK_COLL_S32S];
static int sHostMapAttackCount;

/* stage_load_map, over a file read whole: returns 0, or -1 with the
 * reason. The MPK1 block's own layout is stage.c's -- see the exporter. */
static int host_load_map_file(Stage *st, const char *name)
{
    long size;
    u8 *blob = asset_read_whole(name, &size);
    const u8 *p;
    u32 pack_size, anim_count, fixup_count, weight_count, attack_count;
    u32 graft_size, object_count;
    u32 off_pack, off_anims, off_fixups, off_weights, off_attack, off_graft;
    u32 off_objects;
    u32 i, at, words = 0;
    uint32_t *dst;

    CHECK(blob != NULL);

    if (blob == NULL)
    {
        return -1;
    }
    p = blob;
    for (i = 0; i + 4 <= (u32)size; i++)
    {
        if (memcmp(p + i, "MPK1", 4) == 0)
        {
            break;
        }
    }
    CHECK(i + 4 <= (u32)size);

    p += i;
    memcpy(&pack_size, p + 4, 4);
    memcpy(&anim_count, p + 8, 4);
    memcpy(&fixup_count, p + 12, 4);
    memcpy(&weight_count, p + 16, 4);
    memcpy(&attack_count, p + 20, 4);
    memcpy(&graft_size, p + 24, 4);
    memcpy(&object_count, p + 28, 4);
    memcpy(&off_pack, p + 32, 4);
    memcpy(&off_anims, p + 36, 4);
    memcpy(&off_fixups, p + 40, 4);
    memcpy(&off_weights, p + 44, 4);
    memcpy(&off_attack, p + 48, 4);
    memcpy(&off_graft, p + 52, 4);
    memcpy(&off_objects, p + 56, 4);

    CHECK(anim_count <= STAGE_MAP_ANIMS_MAX);
    CHECK(attack_count <= STAGE_ATTACK_COLL_S32S);
    CHECK(weight_count <= STAGE_ITEM_WEIGHTS_COUNT);
    CHECK(object_count <= STAGE_MAP_OBJECTS_MAX);
    /* A stage with objects has packs to name them with. PEACH'S CASTLE
     * HAS NEITHER: its `map_nodes` is an AnimJoint ROOT rather than a
     * tree, so its MPK1 block carries one script and zero
     * objects -- which is the only shape where pack_size is legitimately
     * 0, and the reason this is a CHECK and not a constant. */
    if (object_count != 0)
    {
        CHECK(pack_size != 0);
    }
    else
    {
        CHECK(pack_size == 0);
    }

    dst = sHostMapWords;
    at = off_anims;
    for (i = 0; i < anim_count; i++)
    {
        u32 n, k;

        memcpy(&n, p + at, 4);
        CHECK(words + n <= ARRAY_COUNT(sHostMapWords));
        memcpy(&sHostMapAnimJoints[i], p + at + 4, 4);
        sHostMapAnimNames[i] = (const char *)(p + at + 8);
        sHostMapAnims[i] = (AObjEvent32 *)dst;
        for (k = 0; k < n; k++)
        {
            memcpy(&dst[k], p + at + 40 + k * 4, 4);
        }
        dst += n;
        words += n;
        at += 40 + n * 4;
    }
    for (i = 0; i < fixup_count; i++)
    {
        u32 a, w, t;

        memcpy(&a, p + off_fixups + i * 12, 4);
        memcpy(&w, p + off_fixups + i * 12 + 4, 4);
        memcpy(&t, p + off_fixups + i * 12 + 8, 4);
        CHECK(a < anim_count && t < anim_count);
        ((uint32_t *)sHostMapAnims[a])[w] = (uint32_t)(uintptr_t)
            sHostMapAnims[t];
    }
    sHostMapAnimCount = (int)anim_count;
    if (weight_count != 0)
    {
        memcpy(sHostMapWeights, p + off_weights, weight_count);
        sHostMapHasWeights = (int)weight_count;
    }
    if (attack_count != 0)
    {
        /* the same seven s32s, and the Stage's own copy of them: the
         * target's stage_load_map puts them in map_owned, which this
         * build's Stage does not have, so the test's arena stands in */
        CHECK(attack_count == STAGE_ATTACK_COLL_S32S);
        memcpy(sHostMapAttack, p + off_attack, sizeof(sHostMapAttack));
        sHostMapAttackCount = (int)attack_count;
        st->attack_coll = (GRAttackColl *)sHostMapAttack;
    }
    /* One baked pack per object, the objects table naming where each sits
     * inside the pack area -- the same read src/dc/stage.c's
     * stage_load_map makes. */
    for (i = 0; i < object_count; i++)
    {
        u32 oo, os;

        memcpy(&oo, p + off_objects + i * 8, 4);
        memcpy(&os, p + off_objects + i * 8 + 4, 4);
        CHECK(os != 0 && oo + os <= pack_size);
        CHECK(fighter_init(&st->map_models[i],
                           (uint8_t *)p + off_pack + oo, (long)os,
                           &st->map_pal_banks[i]) == 0);
    }
    st->map_object_count = (int)object_count;
    if (graft_size != 0)
    {
        CHECK(fighter_init(&st->graft_model, (uint8_t *)p + off_graft,
                           graft_size, &st->graft_pal_bank) == 0);
    }
    /* The blob stays: fighter_init keeps the pack's sections as pointers
     * into it -- the same contract src/dc/stage.c's own Stage has -- and
     * so do the anim names, which lie in the MPK1 block. sHostMapBlob is
     * the test's to give back at teardown. */
    sHostMapBlob = blob;

    return 0;
}

/* src/dc/stage.c's stage_load_placements: the optional BTG1/BMP1 block a
 * bonus course carries (stage.h), which host_load_map_file above knows
 * nothing about.
 *
 * It reads the file itself rather than working off sHostMapBlob, and RACE
 * TO THE FINISH IS WHY: bonus3.stg has no MPK1 block at all -- its
 * MPGroundData names no map_nodes, so the exporter writes none -- so
 * host_load_map_file cannot be the thing that opens it. The blob is freed
 * here too, which the MPK1 read cannot do: a placement is copied by value
 * and a script is copied into `*owned`, so nothing is left pointing into
 * the file.
 *
 * It is written out here for the reason every other host stand-in in this
 * file is -- src/dc/stage.c is not compiled into this build -- and it is
 * the same parser, block for block: 32-byte header, placements at 16
 * bytes each, anims at `4 + 32 + n*4`, then the fixup pass that turns a
 * script's loop word into a pointer. Returns the number of placements, or
 * -1 if the file carries no such block. */
static int host_load_placements(Stage *st, const char *name,
                                const char *magic)
{
    long size = 0;
    u8 *blob = asset_read_whole(name, &size);
    const uint8_t *p = blob;
    uint32_t placement_count, anim_count, fixup_count;
    uint32_t off_pos, off_anims, off_fixups, block_size;
    uint32_t i, at, words = 0;
    Vec3f *pos;
    int8_t *anim;
    uint8_t *count, *anim_count_out;
    AObjEvent32 **anims;
    void **owned;
    uint32_t *w;

    CHECK(blob != NULL);

    if (blob == NULL)
    {
        return -1;
    }
    for (i = 0; (long)i + 4 <= size; i++)
    {
        if (memcmp(p + i, magic, 4) == 0)
        {
            break;
        }
    }
    if ((long)i + 32 > size)
    {
        free(blob);
        return -1;
    }
    p += i;

    if (strcmp(magic, "BMP1") == 0)
    {
        pos = st->bumpers;
        anim = st->bumper_anim;
        count = &st->bumper_count;
        anim_count_out = &st->bumper_anim_count;
        anims = st->bumper_anims;
        owned = &st->bumper_owned;
    }
    else
    {
        pos = st->targets;
        anim = st->target_anim;
        count = &st->target_count;
        anim_count_out = &st->target_anim_count;
        anims = st->target_anims;
        owned = &st->target_owned;
    }

    memcpy(&placement_count, p + 4, 4);
    memcpy(&anim_count, p + 8, 4);
    memcpy(&fixup_count, p + 12, 4);
    memcpy(&off_pos, p + 16, 4);
    memcpy(&off_anims, p + 20, 4);
    memcpy(&off_fixups, p + 24, 4);
    memcpy(&block_size, p + 28, 4);

    CHECK(placement_count <= STAGE_BUMPERS_MAX);
    CHECK(anim_count <= STAGE_BUMPER_ANIMS_MAX);
    CHECK((long)(p - blob) + (long)block_size <= size);

    at = off_anims;
    for (i = 0; i < anim_count; i++)
    {
        uint32_t n;

        memcpy(&n, p + at, 4);
        at += 36 + n * 4;
        words += n;
    }
    CHECK((long)(p - blob) + (long)at <= size);

    *owned = malloc(words * 4 + 4);
    CHECK(*owned != NULL);
    if (*owned == NULL)
    {
        free(blob);
        return -1;
    }
    w = (uint32_t *)*owned;
    at = off_anims;
    for (i = 0; i < anim_count; i++)
    {
        uint32_t n, k;

        memcpy(&n, p + at, 4);
        anims[i] = (AObjEvent32 *)w;
        for (k = 0; k < n; k++)
        {
            memcpy(&w[k], p + at + 36 + k * 4, 4);
        }
        w += n;
        at += 36 + n * 4;
    }
    for (i = 0; i < fixup_count; i++)
    {
        uint32_t a, fw, t;

        memcpy(&a, p + off_fixups + i * 12, 4);
        memcpy(&fw, p + off_fixups + i * 12 + 4, 4);
        memcpy(&t, p + off_fixups + i * 12 + 8, 4);
        CHECK(a < anim_count && t < anim_count);
        ((uint32_t *)anims[a])[fw] = (uint32_t)(uintptr_t)anims[t];
    }
    for (i = 0; i < placement_count; i++)
    {
        const uint8_t *t = p + off_pos + i * 16;
        int32_t a;

        memcpy(&pos[i].x, t, 4);
        memcpy(&pos[i].y, t + 4, 4);
        memcpy(&pos[i].z, t + 8, 4);
        memcpy(&a, t + 12, 4);
        CHECK(a < (int32_t)anim_count);
        anim[i] = (int8_t)a;
    }
    *count = (uint8_t)placement_count;
    *anim_count_out = (uint8_t)anim_count;

    free(blob);
    return (int)placement_count;
}

/* src/dc/stage.c's, over gStageBound: the same tree, the same display and
 * the same anim process the target builds. */
DObj *stage_bind_map_model(GObj *gobj, s32 dl_link)
{
    return stage_bind_map_object(gobj, 0, dl_link);
}

DObj *stage_bind_map_object(GObj *gobj, s32 index, s32 dl_link)
{
    if (gStageBound == NULL || index < 0 ||
        index >= gStageBound->map_object_count ||
        gStageBound->map_models[index].hd == NULL)
    {
        return NULL;
    }
    if (dc_model_add_dobjs(gobj, NULL, &gStageBound->map_models[index],
                           gStageBound->map_joints[index]) < 0)
    {
        return NULL;
    }
    gcAddGObjDisplay(gobj, gcDrawDObjTreeForGObj, dl_link,
                     GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 5);

    return DObjGetStruct(gobj);
}

/* src/dc/stage.c's, over the arena host_load_map_file filled. */
void stage_map_anim_array(const char *name, AObjEvent32 **out)
{
    int i;

    for (i = 0; i < STAGE_MAP_JOINTS_MAX; i++)
    {
        out[i] = NULL;
    }
    for (i = 0; i < sHostMapAnimCount; i++)
    {
        int j;

        if (strcmp(sHostMapAnimNames[i], name) != 0)
        {
            continue;
        }
        j = sHostMapAnimJoints[i];

        if (j >= 0 && j < STAGE_MAP_JOINTS_MAX)
        {
            out[j] = sHostMapAnims[i];
        }
    }
}

int stage_map_anim_joint(const char *name)
{
    int i;

    for (i = 0; i < sHostMapAnimCount; i++)
    {
        if (strcmp(sHostMapAnimNames[i], name) == 0)
        {
            return sHostMapAnimJoints[i];
        }
    }
    return -1;
}

AObjEvent32 *stage_map_anim(const char *name)
{
    int i;

    for (i = 0; i < sHostMapAnimCount; i++)
    {
        if (strcmp(sHostMapAnimNames[i], name) == 0)
        {
            return sHostMapAnims[i];
        }
    }
    return NULL;
}

/* src/dc/stage.c's own, over gStageBound: the graft, the same way
 * stage_bind_map_model is. The MObjs are built (so dobj->mobj exists,
 * which grYosterUpdateCloudSolid's first gate dereferences) and then
 * their scripts are DETACHED: the host cannot walk an AObjEvent32 script
 * at all -- eight bytes to the ROM's four -- and the gcPlayAnimAll
 * process this build registers would spin on one. The test drives the
 * gate itself instead, by writing dobj->mobj->anim_wait. */
DObj *stage_bind_map_cloud(GObj *gobj, DObj *parent, int alt)
{
    MObj *mobj;

    if (gStageBound == NULL || gStageBound->graft_model.hd == NULL)
    {
        return NULL;
    }
    if (dc_model_add_dobjs(gobj, parent, &gStageBound->graft_model,
                           gStageBound->graft_joints) < 0)
    {
        return NULL;
    }
    if (dc_model_add_mobjs_dobj(gStageBound->graft_joints[0],
                                &gStageBound->graft_model, 0.0F,
                                alt, 0) > 0)
    {
        for (mobj = gStageBound->graft_joints[0]->mobj; mobj != NULL;
             mobj = mobj->next)
        {
            mobj->matanim_joint.event32 = NULL;
            mobj->anim_wait = AOBJ_ANIM_NULL;
        }
    }
    return gStageBound->graft_joints[0];
}

void stage_cloud_set_anim(DObj *dobj, int alt)
{
    (void)dobj;
    (void)alt;
}

/* src/dc/stage.c's, likewise a no-op here: the host build detaches every
 * MObj's script at bind time (AObjEvent32 is eight bytes here), so there
 * is nothing to re-point. */
void stage_map_set_anim(DObj *dobj, int index, int alt)
{
    (void)dobj;
    (void)index;
    (void)alt;
}

/* See stage.h: src/dc/stage.c's own sStage, which is gStageBound here. */
Stage *stage_bound(void)
{
    return gStageBound;
}

/* src/dc/stage.c's, written out rather than stubbed: it is a strcmp over
 * a name table and nothing in it needs the PVR, so the host can ask the
 * same question sc1pbonusstage.c's platform maker asks. */
int stage_platform_pack(Stage *st, const char *name)
{
    int i;

    for (i = 0; i < st->platform_model_count; i++)
    {
        if (strcmp(st->platform_names[i], name) == 0)
        {
            return i;
        }
    }
    return -1;
}

DObj *stage_map_joint(int obj, int index)
{
    if (gStageBound == NULL || obj < 0 || obj >= STAGE_MAP_OBJECTS_MAX ||
        index < 0 || index >= STAGE_MAP_JOINTS_MAX)
    {
        return NULL;
    }
    return gStageBound->map_joints[obj][index];
}

/* Hyrule's tornado, both halves.
 *
 * The producer (src/dc/grhyrule.c) is driven through its whole state
 * machine over the mock stage's own deck, and the consumer
 * (src/dc/ftcommontwister.c) is driven for real -- a spawned Mario is
 * put inside the funnel's box and the game's own per-frame hazard search
 * (ftMainProcSearchHitAll -> ftMainSearchHitHazard) catches him, because
 * the SetStatus that had been a no-op stub is a real one now.
 *
 * The funnel's own GObj is made by grHyruleMakeGround, which is what
 * src/dc/stage.c calls when the bound stage is nGRKindHyrule. It is
 * ejected at the end and the mock's kMapObjs carries one
 * nMPMapObjKindTwister entry, which is what grHyruleTwisterInitVars
 * counts -- without one it halts on the decomp's own "Twister positions
 * are error!" loop, so the map object is the test's, not the port's.
 *
 * The funnel's particle bank is the real GRHyrule one, built into bank 0
 * by host_load_particle_bank above: that is the whole point of the
 * fixture, and grHyruleTwisterInitVars' own load is `#ifdef FT_HOSTTEST`
 * -ed out for the reason stated there. So the funnel is really made, its
 * transform is really added, and its generator really runs. */
static void test_gr_hyrule(void)
{
    GObj *twister_gobj;
    GObj *ground_gobj;
    Vec3f pos;
    Vec3f funnel_pos;
    s32 kind;
    s32 saved_status = mock_battle.game_status;
    LBScript **saved_scripts;
    LBTexture **saved_textures;
    s32 saved_nscripts;
    s32 saved_ntextures;
    f32 release_wait;

    /* the mock's deck, which the twister's own spawn position is over */
    enum { kDeckLine = LINE_DECK };

    /* The particle pools a scene allocates once (scvsbattle.c:275, the
     * game's own efParticleInitAll), which nothing else in this suite
     * needs and so nothing else has started. Without it the struct,
     * generator and transform free lists are empty and the funnel's
     * effect unwinds to NULL on its first allocation. */
    efParticleInitAll();

    saved_scripts = sLBParticleScriptBanks[0];
    saved_textures = sLBParticleTextureBanks[0];
    saved_nscripts = sLBParticleScriptBanksNum[0];
    saved_ntextures = sLBParticleTextureBanksNum[0];

    host_load_particle_bank(0, "grhyrule.scb", "grhyrule.txb");

    mock_battle.game_status = nSCBattleGameStatusGo;

    /* ---- grHyruleMakeGround: the stage's own ground GObj, its positions
     * read off the map file, the status at Sleep and the particle bank
     * loaded. ---- */
    ground_gobj = grHyruleMakeGround();
    CHECK(ground_gobj != NULL);
    CHECK(gGRCommonStruct.hyrule.twister_pos_count == 1);
    CHECK(gGRCommonStruct.hyrule.twister_pos_ids != NULL);
    CHECK(gGRCommonStruct.hyrule.twister_pos_ids[0] == 5);
    CHECK(gGRCommonStruct.hyrule.twister_status == kTwisterSleep);
    /* zero because this build does not load one -- see the `#ifdef
     * FT_HOSTTEST` arm in grHyruleTwisterInitVars -- and the bank the
     * fixture installed above is the one in slot 0. The target's own
     * value comes from the disc probe. */
    CHECK(gGRCommonStruct.hyrule.particle_bank_id == 0);

    /* ---- the Sleep arm only ever looks at the match's status: while the
     * match is still waiting to start the funnel stays asleep, and the
     * first tic it is not, Sleep hands over to Wait with the countdown
     * 1600..2800. ---- */
    mock_battle.game_status = nSCBattleGameStatusWait;
    grHyruleTwisterProcUpdate(ground_gobj);
    CHECK(gGRCommonStruct.hyrule.twister_status == kTwisterSleep);

    mock_battle.game_status = nSCBattleGameStatusGo;
    grHyruleTwisterProcUpdate(ground_gobj);
    CHECK(gGRCommonStruct.hyrule.twister_status == kTwisterWait);
    CHECK(gGRCommonStruct.hyrule.twister_wait >= 1600);
    CHECK(gGRCommonStruct.hyrule.twister_wait <= 2800);

    /* ---- Wait: the countdown's last tic spawns the funnel at the map
     * object's position, which projects down onto the deck. It lands in
     * Summon, with the funnel's own GObj and its particle transform, and
     * the two travel bounds taken from the deck's edges. ---- */
    gGRCommonStruct.hyrule.twister_wait = 1;
    grHyruleTwisterProcUpdate(ground_gobj);
    CHECK(gGRCommonStruct.hyrule.twister_status == kTwisterSummon);
    CHECK(gGRCommonStruct.hyrule.twister_wait == 80);
    CHECK(gGRCommonStruct.hyrule.twister_line_id == kDeckLine);
    CHECK(gGRCommonStruct.hyrule.twister_gobj != NULL);
    CHECK(gGRCommonStruct.hyrule.twister_xf != NULL);
    /* the deck runs -2000..2000, and an edge with a wall under it is
     * pulled in 300 -- so either value, and either way inside that. */
    CHECK(gGRCommonStruct.hyrule.twister_leftedge_x >= -2300.0F);
    CHECK(gGRCommonStruct.hyrule.twister_leftedge_x <= -1700.0F);
    CHECK(gGRCommonStruct.hyrule.twister_rightedge_x >= 1700.0F);
    CHECK(gGRCommonStruct.hyrule.twister_rightedge_x <= 2300.0F);

    twister_gobj = gGRCommonStruct.hyrule.twister_gobj;

    /* the funnel sits where the map object put it, on the deck */
    funnel_pos = DObjGetStruct(twister_gobj)->translate.vec.f;
    CHECK(funnel_pos.x > -700.0F);
    CHECK(funnel_pos.x < -500.0F);
    CHECK(funnel_pos.y > -1.0F);
    CHECK(funnel_pos.y < 1.0F);

    /* ---- grHyruleTwisterGetLR: which side of the funnel the fighters
     * standing on its own floor line are. Nothing has been registered
     * with the fighter system as a ground obstacle yet -- that is
     * UpdateSummon's last tic, below -- so every spawn in this section is
     * a test of the funnel's own arithmetic and not of the catch. ---- */

    /* a fighter on the platform above is not on the funnel's line, which
     * is the line the direction pick is measured along: nothing to pick
     * from */
    spawn(0.0F, 300.0F);
    CHECK(fpp->coll_data.floor_line_id == LINE_PLAT);
    CHECK(grHyruleTwisterGetLR() == 0);

    /* ---- grHyruleTwisterCheckGetDamageKind: the box, on its own. A
     * fighter standing in the funnel answers TRUE; one 350 units to its
     * right does not; and neither is caught, because the funnel is not a
     * ground obstacle yet. ---- */
    spawn(-600.0F, 0.0F);
    idle_frames(2);
    CHECK(fpp->ga == nMPKineticsGround);
    CHECK(fpp->coll_data.floor_line_id == kDeckLine);

    kind = -1;
    CHECK(grHyruleTwisterCheckGetDamageKind(twister_gobj, mock_gobj, &kind) == TRUE);
    CHECK(kind == nGMHitEnvironmentTwister);
    CHECK(fpp->status_id != nFTCommonStatusTwister);

    kind = -1;
    spawn(-250.0F, 0.0F);
    idle_frames(2);
    CHECK(grHyruleTwisterCheckGetDamageKind(twister_gobj, mock_gobj, &kind) == FALSE);
    CHECK(fpp->status_id != nFTCommonStatusTwister);

    /* ---- and the direction: a fighter level with the funnel is on its
     * left, one out to its right is on its right. ---- */
    spawn(-600.0F, 0.0F);
    idle_frames(2);
    CHECK(grHyruleTwisterGetLR() == -1);         /* level with it: the left */

    spawn(100.0F, 0.0F);
    idle_frames(2);
    CHECK(grHyruleTwisterGetLR() == +1);         /* out on its right */

    /* ---- Summon: the countdown's last tic commits to a direction (10
     * units a tic), draws its speed window, and registers the funnel with
     * the fighter system as a ground obstacle -- which is the first tic
     * anything can be caught. ---- */
    gGRCommonStruct.hyrule.twister_wait = 1;
    grHyruleTwisterProcUpdate(ground_gobj);
    CHECK(gGRCommonStruct.hyrule.twister_status == kTwisterMove);
    CHECK(gGRCommonStruct.hyrule.twister_wait >= 520);
    CHECK(gGRCommonStruct.hyrule.twister_wait <= 1120);
    CHECK((gGRCommonStruct.hyrule.twister_vel == 10.0F) ||
          (gGRCommonStruct.hyrule.twister_vel == -10.0F));
    CHECK(gGRCommonStruct.hyrule.twister_speed_wait >= 180);
    CHECK(gGRCommonStruct.hyrule.twister_speed_wait <= 300);

    /* ---- and the catch itself, through the game's own search: a Mario
     * spawned inside the box is put into nFTCommonStatusTwister by
     * ftMainSearchHitHazard -> ftMainSetHitHazard -> SetStatus, within
     * the next two frames, with the funnel remembered and full capture
     * immunity granted. This is the half that was a no-op stub. ---- */
    spawn(-600.0F, 0.0F);
    CHECK(fpp->status_id != nFTCommonStatusTwister);

    idle_frames(2);
    CHECK(fpp->status_id == nFTCommonStatusTwister);
    CHECK(fpp->status_vars.common.twister.tornado_gobj == twister_gobj);
    CHECK(fpp->capture_immune_mask == FTCATCHKIND_MASK_ALL);

    /* ---- ftCommonTwisterProcPhysics: the spiral. One call in, the
     * fighter has been given a velocity toward the point the funnel's own
     * angle says, capped at 50 units a tic. ---- */
    ftCommonTwisterProcPhysics(mock_gobj);
    CHECK(fpp->physics.vel_air.x < 60.0F);
    CHECK(fpp->physics.vel_air.x > -60.0F);
    CHECK(fpp->physics.vel_air.y < 60.0F);
    CHECK(fpp->physics.vel_air.y > -60.0F);

    /* ---- ProcUpdate: sixty tics of riding and then the throw out, which
     * costs 14 damage and sets the sixty-tick cooldown that keeps the
     * same funnel from catching the same fighter again.
     *
     * The two frames above are a live world, so how much of the sixty has
     * already run is read rather than assumed -- the countdown is the
     * status's own release_wait, and the loop below is exactly what is
     * left of it, one short of the throw. ---- */
    CHECK(fpp->percent_damage == 0);

    release_wait = fpp->status_vars.common.twister.release_wait;
    CHECK(release_wait < FTCOMMON_TORNADO_RELEASE_WAIT);

    for (kind = 0; kind < (s32)(FTCOMMON_TORNADO_RELEASE_WAIT - release_wait) - 1; kind++)
    {
        ftCommonTwisterProcUpdate(mock_gobj);
    }
    CHECK(fpp->status_id == nFTCommonStatusTwister);     /* one tic short */

    ftCommonTwisterProcUpdate(mock_gobj);                /* the sixtieth */
    CHECK(fpp->status_id != nFTCommonStatusTwister);
    CHECK(fpp->percent_damage == 14);
    CHECK(fpp->twister_wait == FTCOMMON_TORNADO_PICKUP_WAIT);

    /* ---- grHyruleTwisterCheckGetPosition: where the funnel is, while it
     * is moving. ---- */
    gGRCommonStruct.hyrule.twister_status = kTwisterMove;
    funnel_pos.x = -1234.0F;
    funnel_pos.y = 567.0F;
    funnel_pos.z = 0.0F;
    DObjGetStruct(twister_gobj)->translate.vec.f = funnel_pos;
    pos.x = pos.y = pos.z = 0.0F;
    CHECK(grHyruleTwisterCheckGetPosition(&pos) == TRUE);
    CHECK(pos.x == -1234.0F);
    CHECK(pos.y == 567.0F);

    gGRCommonStruct.hyrule.twister_status = kTwisterTurn;
    CHECK(grHyruleTwisterCheckGetPosition(&pos) == TRUE);

    gGRCommonStruct.hyrule.twister_status = kTwisterSleep;
    pos.x = 7.0F;
    CHECK(grHyruleTwisterCheckGetPosition(&pos) == FALSE);
    CHECK(pos.x == 7.0F);                        /* untouched when it answers no */

    /* ---- the lifetime countdown Move and Turn share: it only reaches
     * Stop on the tic its own counter does. ---- */
    gGRCommonStruct.hyrule.twister_wait = 2;
    CHECK(grHyruleTwisterDecLifetimeCheckStop() == FALSE);
    CHECK(grHyruleTwisterDecLifetimeCheckStop() == TRUE);
    CHECK(gGRCommonStruct.hyrule.twister_status == kTwisterStop);

    /* ---- Stop: the funnel will not leave while a fighter is still
     * caught in it. This one is not (it was thrown out above), so the
     * next tic takes it to Subside, unregisters the obstacle, puffs and
     * ejects its own GObj. ---- */
    grHyruleTwisterProcUpdate(ground_gobj);
    CHECK(gGRCommonStruct.hyrule.twister_status == kTwisterSubside);
    CHECK(gGRCommonStruct.hyrule.twister_wait == 32);

    /* ---- Subside: thirty-two tics of puff, then back to Wait with a
     * fresh countdown. ---- */
    for (kind = 0; kind < 32; kind++)
    {
        grHyruleTwisterProcUpdate(ground_gobj);
    }
    CHECK(gGRCommonStruct.hyrule.twister_status == kTwisterWait);
    CHECK(gGRCommonStruct.hyrule.twister_wait >= 1600);
    CHECK(gGRCommonStruct.hyrule.twister_wait <= 2800);

    /* teardown: the funnel's own GObj is gone (UpdateStop ejected it),
     * and the ground GObj is this test's to give back. gGRCommonStruct
     * is a real global with a scene-heap pointer in it, so it is cleared
     * the way the overlay reload does. The match status is put back
     * because this test moved it and the mock is shared. */
    ftMainClearGroundObstacle(twister_gobj);
    gcEjectGObj(ground_gobj);
    memset(&gGRCommonStruct, 0, sizeof(gGRCommonStruct));
    mock_battle.game_status = saved_status;

    /* bank 0 goes back to what it was -- another stage test will install
     * its own there -- and the effect's generator is gone with the funnel
     * the GObj was ejected out from under (UpdateStop's own puff, then
     * UpdateSubside's eject). */
    sLBParticleScriptBanks[0] = saved_scripts;
    sLBParticleTextureBanks[0] = saved_textures;
    sLBParticleScriptBanksNum[0] = saved_nscripts;
    sLBParticleTextureBanksNum[0] = saved_ntextures;
}

/* Kongo Jungle's barrel cannon, both halves.
 *
 * The producer (src/dc/grjungle.c) is driven through its whole two-state
 * machine, and this is the first stage test whose subject is a MODEL:
 * the cannon's tree, its three AnimJoint scripts and the position every
 * read in the file goes through all come out of the stage's own .stg,
 * read by host_load_map_file above -- so grJungleMakeTaruCann builds a
 * real DObj tree, plays a real script through sys/objanim.c, and
 * grJungleTaruCannGetPosition answers with a real number the script put
 * there rather than a number the test did.
 *
 * The consumer (src/dc/ftcommontaru.c) is driven for real the same way
 * the twister's is: a spawned Mario is put inside the cannon's box and
 * the game's own per-frame hazard search (ftMainProcSearchHitAll ->
 * ftMainSearchHitHazard) catches him, because the SetStatus that had
 * been a no-op stub is a real one now -- the third time this port has
 * found that shape. */
static Stage sJungleStage;

/* grjungle.c's own private enum, the same way test_gr_hyrule names the
 * twister's states: the decomp keeps it in the .c file, so there is
 * nothing to include and nothing to collide with. */
enum { kTaruCannMove = 0, kTaruCannRotate = 1 };

/* grjungle.c's initialized data, declared where the shim header's own
 * is not on this file's include path. */
extern DObjTransformTypes dGRJungleTaruCannTransformKinds[];

static void test_gr_jungle(void)
{
    GObj *cannon_gobj;
    Stage *saved_bound;
    s32 kind;
    s32 i;
    s32 saved_status = mock_battle.game_status;
    f32 release_wait;
    f32 rotate_step;

    /* the stage's own map file, read and baked the way the target reads
     * it. gStageBound is what src/dc/stage.c's sStage is called here. */
    saved_bound = gStageBound;
    gStageBound = &sJungleStage;
    memset(&sJungleStage, 0, sizeof(sJungleStage));
    CHECK(host_load_map_file(&sJungleStage, "jungle.stg") == 0);

    /* the exporter's own numbers, checked where they arrive: the map
     * object is two joints and one quad, and the map file names three
     * AnimJoint scripts beside it (gr/grcommon/grjungle.c plays
     * TaruCannDefault, TaruCannFill and TaruCannShoot). */
    CHECK(sJungleStage.map_models[0].hd != NULL);
    CHECK(sJungleStage.map_models[0].hd->joint_count == 2);
    CHECK(sHostMapAnimCount == 3);
    CHECK(stage_map_anim("TaruCannDefault") != NULL);
    CHECK(stage_map_anim("TaruCannFill") != NULL);
    CHECK(stage_map_anim("TaruCannShoot") != NULL);
    CHECK(stage_map_anim("TaruCannNonesuch") == NULL);

    /* the map file's item weights, MPGroundData.item_weights: the
     * decomp's own dGRJungleMap_item_weights, which the randomizer reads
     * to decide what drops. The first three are Box, Taru and Capsule. */
    CHECK(sHostMapHasWeights == STAGE_ITEM_WEIGHTS_COUNT);
    CHECK(sHostMapWeights[0] == 0x50);
    CHECK(sHostMapWeights[1] == 0x78);
    CHECK(sHostMapWeights[2] == 0x32);

    /* ---- grJungleMakeGround: the cannon GObj, with the map file's
     * tree on it, printed at the map file's own joint order. ---- */
    mock_battle.game_status = nSCBattleGameStatusGo;
    CHECK(grJungleMakeGround() == NULL);     /* this stage has no ground GObj */
    cannon_gobj = gGRCommonStruct.jungle.tarucann_gobj;
    CHECK(cannon_gobj != NULL);

    /* two joints, the root on the GObj and the quad under it -- the
     * shape grJungleTaruCannAddAnimOffset's own `->child` needs */
    CHECK(DObjGetStruct(cannon_gobj) != NULL);
    CHECK(DObjGetStruct(cannon_gobj)->child != NULL);
    CHECK(stage_map_joint(0, 0) == DObjGetStruct(cannon_gobj));
    CHECK(stage_map_joint(0, 1) == DObjGetStruct(cannon_gobj)->child);

    /* and the default animation has already run: the script's own first
     * values, x = 3540 and y = -1597.5, are on the ROOT -- which is the
     * whole point of putting it there (grjungle.c's header). */

    /* The default script really is the one the exporter wrote and it
     * really is on the root, which is what the two position reads below
     * and ftCommonTaruCannProcPhysics all go through. The script's own
     * numbers are not asserted here: gcParseDObjAnimJoint and the cubic
     * in gcPlayDObjAnimJoint are the engine's, driven for real, and what
     * they evaluate to on the first tic is the engine's business, not
     * this test's. What is this port's business is that the tree, the
     * joint order and the script all arrived. */
    /* The scripts are the ROM's own words, byte for byte, and the one
     * relocated pointer in each -- SetAnim's, the loop back to the
     * script's first command -- was written in at load. This is the half
     * of the export the host can check: the interpreter that consumes
     * them cannot run here at all (see grJungleMakeTaruCann's own note
     * on AObjEvent32 being eight bytes on x86-64). */
    CHECK(sHostMapAnimCount == 3);
    CHECK((((uint32_t *)sHostMapAnims[0])[0]) == 0x12080000);  /* SetVal0Rate TRAX */
    CHECK((((uint32_t *)sHostMapAnims[0])[1]) == 0x455D4000);  /* 3540.0f */
    CHECK((((uint32_t *)sHostMapAnims[0])[2]) == 0x14300000);  /* SetValAfterBlock TRAY|TRAZ */
    CHECK((((uint32_t *)sHostMapAnims[0])[5]) == 0x100800F9);  /* SetVal0RateBlock TRAX, 249 */
    CHECK((((uint32_t *)sHostMapAnims[0])[9]) == 0x1C000000);  /* SetAnim */
    CHECK((((uint32_t *)sHostMapAnims[0])[10]) == (uint32_t)(uintptr_t)sHostMapAnims[0]);
    /* and that is the END of it: SetAnim is unconditional, so the script
     * is eleven words and the next one starts on the twelfth. This used
     * to read word 11 and find an End, because the exporter's walk
     * carried on past the loop and swallowed whatever the ROM had put
     * behind it -- a zero, here, but a neighbour's data on two of the
     * Board the Platforms courses, which is why the walk stops where
     * the runtime stops. */
    CHECK((uint32_t *)sHostMapAnims[1] == ((uint32_t *)sHostMapAnims[0]) + 11);
    CHECK(stage_map_anim("TaruCannFill") == sHostMapAnims[1]);
    CHECK(stage_map_anim("TaruCannShoot") == sHostMapAnims[2]);

    /* ---- Move: the wait between rolls is 180..360, and its last tic
     * picks a direction and a fixed 90 tics to spend on it. ---- */
    CHECK(gGRCommonStruct.jungle.tarucann_status == kTaruCannMove);
    CHECK(gGRCommonStruct.jungle.tarucann_wait >= 180);
    CHECK(gGRCommonStruct.jungle.tarucann_wait <= 360);
    CHECK(gGRCommonStruct.jungle.tarucann_rotate_step == F_CST_DTOR32(0.0F));

    gGRCommonStruct.jungle.tarucann_wait = 2;
    grJungleTaruCannProcUpdate(cannon_gobj);
    CHECK(gGRCommonStruct.jungle.tarucann_status == kTaruCannMove);
    CHECK(gGRCommonStruct.jungle.tarucann_wait == 1);

    grJungleTaruCannProcUpdate(cannon_gobj);
    CHECK(gGRCommonStruct.jungle.tarucann_status == kTaruCannRotate);
    CHECK(gGRCommonStruct.jungle.tarucann_wait == 90);
    rotate_step = gGRCommonStruct.jungle.tarucann_rotate_step;
    CHECK((rotate_step == 0.07F) || (rotate_step == -0.07F));

    /* ---- Rotate: 89 steps of the roll and then the zeroing tic, which
     * is the tic the status goes back to Move. The zeroing is on the
     * wait == 0 arm, not the step arm, so the accumulated rotation is
     * NOT what the last step made it. ---- */
    DObjGetStruct(cannon_gobj)->rotate.vec.f.z = 0.0F;

    for (i = 0; i < 89; i++)
    {
        grJungleTaruCannProcUpdate(cannon_gobj);
        CHECK(gGRCommonStruct.jungle.tarucann_status == kTaruCannRotate);
    }
    /* either direction: the step's own sign is the coin toss UpdateMove
     * made, and 89 of them is a little over a full turn */
    CHECK(fabsf(DObjGetStruct(cannon_gobj)->rotate.vec.f.z) > 6.2F);
    CHECK(fabsf(DObjGetStruct(cannon_gobj)->rotate.vec.f.z) < 6.3F);
    CHECK(grJungleTaruCannGetRotate() == DObjGetStruct(cannon_gobj)->rotate.vec.f.z);

    grJungleTaruCannProcUpdate(cannon_gobj);
    CHECK(gGRCommonStruct.jungle.tarucann_status == kTaruCannMove);
    CHECK(gGRCommonStruct.jungle.tarucann_wait >= 180);
    CHECK(gGRCommonStruct.jungle.tarucann_wait <= 360);
    CHECK(DObjGetStruct(cannon_gobj)->rotate.vec.f.z == F_CST_DTOR32(0.0F));

    /* ---- grJungleTaruCannCheckGetDamageKind: the box, on its own. The
     * cannon's own position is where the script put it, so the test
     * moves it onto the deck rather than hunting for a fighter that can
     * reach it -- what the function reads is the DObj, either way. ---- */
    DObjGetStruct(cannon_gobj)->translate.vec.f.x = 0.0F;
    DObjGetStruct(cannon_gobj)->translate.vec.f.y = 0.0F;

    /* The check is a function of the two DObjs and the fighter's own
     * three fields, so the test PLACES the fighter rather than letting
     * the world settle him: the cannon is registered as a ground
     * obstacle by grJungleMakeTaruCann itself, so a fighter standing in
     * its box is caught by the game's own search on the very next tic
     * (the twister test finds the same thing one tic earlier).
     * That is what the section after this one drives on purpose. */
    spawn(0.0F, 0.0F);
    fpp->status_id = nFTCommonStatusWait;
    fpp->tarucann_wait = 0;
    fpp->capture_immune_mask = 0;

    DObjGetStruct(cannon_gobj)->translate.vec.f.x = 0.0F;
    DObjGetStruct(cannon_gobj)->translate.vec.f.y = 0.0F;
    DObjGetStruct(mock_gobj)->translate.vec.f.x = 0.0F;
    DObjGetStruct(mock_gobj)->translate.vec.f.y = 0.0F;

    kind = -1;
    CHECK(grJungleTaruCannCheckGetDamageKind(cannon_gobj, mock_gobj, &kind) == TRUE);
    CHECK(kind == nGMHitEnvironmentTaruCann);
    CHECK(fpp->status_id != nFTCommonStatusTaruCann);

    /* 279 units to its right is inside the square; 300 is outside it,
     * and so is 300 straight up. */
    DObjGetStruct(mock_gobj)->translate.vec.f.x = 279.0F;
    kind = -1;
    CHECK(grJungleTaruCannCheckGetDamageKind(cannon_gobj, mock_gobj, &kind) == TRUE);
    CHECK(kind == nGMHitEnvironmentTaruCann);

    DObjGetStruct(mock_gobj)->translate.vec.f.x = 300.0F;
    CHECK(grJungleTaruCannCheckGetDamageKind(cannon_gobj, mock_gobj, &kind) == FALSE);

    DObjGetStruct(mock_gobj)->translate.vec.f.x = 0.0F;
    DObjGetStruct(mock_gobj)->translate.vec.f.y = 300.0F;
    CHECK(grJungleTaruCannCheckGetDamageKind(cannon_gobj, mock_gobj, &kind) == FALSE);

    DObjGetStruct(mock_gobj)->translate.vec.f.y = 0.0F;

    /* ---- only one fighter per cannon. The scan walks the fighter
     * link, skips the fighter being tested, and compares the cannon each
     * other one REMEMBERS -- so a second fighter in THIS cannon blocks
     * the catch, and the same fighter in a different cannon does not.
     * The subject has to be outside nFTCommonStatusTaruCann for the
     * check to get that far, which is why this is the second fighter's
     * status and not the first's. ---- */
    spawn_second(0.0F, 0.0F, +1);

    fp2.status_id = nFTCommonStatusTaruCann;
    fp2.status_vars.common.tarucann.tarucann_gobj = cannon_gobj;
    CHECK(grJungleTaruCannCheckGetDamageKind(cannon_gobj, mock_gobj, &kind) == FALSE);

    fp2.status_vars.common.tarucann.tarucann_gobj = (GObj *)&sJungleStage;
    CHECK(grJungleTaruCannCheckGetDamageKind(cannon_gobj, mock_gobj, &kind) == TRUE);

    fp2.status_id = nFTCommonStatusWait;

    /* ---- and the immunity mask and cooldown, the other two gates ---- */
    fpp->capture_immune_mask = FTCATCHKIND_MASK_TARUCANN;
    CHECK(grJungleTaruCannCheckGetDamageKind(cannon_gobj, mock_gobj, &kind) == FALSE);

    fpp->capture_immune_mask = 0;
    fpp->tarucann_wait = FTCOMMON_TARUCANN_PICKUP_WAIT;
    CHECK(grJungleTaruCannCheckGetDamageKind(cannon_gobj, mock_gobj, &kind) == FALSE);

    /* ---- the catch itself, through the game's own search: a Mario
     * spawned inside the box is put into nFTCommonStatusTaruCann by
     * ftMainSearchHitHazard -> ftMainSetHitHazard -> SetStatus, within
     * the next two frames, with the cannon remembered, full capture
     * immunity and is_invisible. ---- */
    fpp->tarucann_wait = 0;
    spawn(0.0F, 0.0F);
    DObjGetStruct(mock_gobj)->translate.vec.f.x = 0.0F;
    DObjGetStruct(mock_gobj)->translate.vec.f.y = 0.0F;
    CHECK(fpp->status_id != nFTCommonStatusTaruCann);

    idle_frames(2);
    CHECK(fpp->status_id == nFTCommonStatusTaruCann);
    CHECK(fpp->status_vars.common.tarucann.tarucann_gobj == cannon_gobj);
    CHECK(fpp->capture_immune_mask == FTCATCHKIND_MASK_ALL);
    CHECK(fpp->is_invisible == TRUE);
    CHECK(fpp->status_vars.common.tarucann.shoot_wait == 0);
    CHECK(fpp->tarucann_wait == 0);      /* SetStatus does not set it */

    /* ---- ftCommonTaruCannProcPhysics: the whole of the status's
     * motion, one line, to wherever the cannon's root is. ---- */
    DObjGetStruct(cannon_gobj)->translate.vec.f.x = -777.0F;
    DObjGetStruct(cannon_gobj)->translate.vec.f.y = 555.0F;
    ftCommonTaruCannProcPhysics(mock_gobj);
    CHECK(DObjGetStruct(mock_gobj)->translate.vec.f.x == -777.0F);
    CHECK(DObjGetStruct(mock_gobj)->translate.vec.f.y == 555.0F);

    /* ---- ProcUpdate: FTCOMMON_TARUCANN_RELEASE_WAIT tics inside, then
     * the shoot animation is asked for and the shoot countdown starts.
     * The two frames above are a live world, so how much of the first
     * countdown has already run is read rather than assumed -- the same
     * rule the twister test follows. ---- */
    release_wait = fpp->status_vars.common.tarucann.release_wait;
    CHECK(release_wait < FTCOMMON_TARUCANN_RELEASE_WAIT);

    for (kind = 0; kind < (s32)(FTCOMMON_TARUCANN_RELEASE_WAIT - release_wait); kind++)
    {
        ftCommonTaruCannProcUpdate(mock_gobj);
    }
    CHECK(fpp->status_id == nFTCommonStatusTaruCann);
    CHECK(fpp->status_vars.common.tarucann.shoot_wait == FTCOMMON_TARUCANN_SHOOT_WAIT);

    /* ---- and the shoot countdown's own end throws the fighter out. The
     * cannon's damage is 0 and its base knockback 180, so this is the
     * ground-hazard curve with a zero damage add; what the test checks is
     * the status it lands in and the sixteen-tic cooldown. ---- */
    for (kind = 0; kind < FTCOMMON_TARUCANN_SHOOT_WAIT - 1; kind++)
    {
        ftCommonTaruCannProcUpdate(mock_gobj);
    }
    CHECK(fpp->status_id == nFTCommonStatusTaruCann);   /* one tic short */

    ftCommonTaruCannProcUpdate(mock_gobj);
    CHECK(fpp->status_id != nFTCommonStatusTaruCann);
    CHECK(fpp->tarucann_wait == FTCOMMON_TARUCANN_PICKUP_WAIT);

    /* ---- grJungleTaruCannAddAnimShoot / AddAnimFill are no-ops in
     * THIS build -- both go through AddAnimOffset, which is guarded for
     * the reason grJungleMakeTaruCann states (the host cannot walk an
     * AObjEvent32 script). They are still called, so that the guard
     * itself is exercised and a build that lost it would be caught by
     * the interpreter spinning rather than by nothing at all. ---- */
    grJungleTaruCannAddAnimShoot(cannon_gobj);
    grJungleTaruCannAddAnimFill(cannon_gobj);
    grJungleTaruCannAddAnimOffset(cannon_gobj, "TaruCannNonesuch");
    CHECK(DObjGetStruct(cannon_gobj)->child->anim_joint.event32 == NULL);

    /* ---- dGRJungleTaruCannTransformKinds, the file's own initialized
     * data (dead in this port; see grjungle.c's header) ---- */
    /* DObjTransformTypes is three u8s (tk1, tk2, tk3) and the decomp's
     * first entry is `{ 0x28, nGCMatrixKindRotRpyR, 0x00 }` -- 0x28 in
     * tk1 and RotRpyR in tk2, which grModelSetupGroundDObjs turns into
     * two XObjs. The second entry is the translate+rotate+scale kind in
     * tk1 and none in tk2. */
    CHECK(dGRJungleTaruCannTransformKinds[0].tk1 == 0x28);
    CHECK(dGRJungleTaruCannTransformKinds[0].tk2 == nGCMatrixKindRotRpyR);
    CHECK(dGRJungleTaruCannTransformKinds[0].tk3 == 0x00);
    CHECK(dGRJungleTaruCannTransformKinds[1].tk1 == nGCMatrixKindTraRotRpyRSca);
    CHECK(dGRJungleTaruCannTransformKinds[1].tk2 == nGCMatrixKindNull);

    /* teardown: the cannon is registered with the fighter system and its
     * GObj is this test's to give back, and gGRCommonStruct is a real
     * global that the overlay reload clears. */
    ftMainClearGroundObstacle(cannon_gobj);
    gcEjectGObj(cannon_gobj);
    memset(&gGRCommonStruct, 0, sizeof(gGRCommonStruct));
    fighter_release(&sJungleStage.map_models[0]);
    free(sHostMapBlob);
    sHostMapBlob = NULL;
    mock_battle.game_status = saved_status;
    gStageBound = saved_bound;
}

/* Planet Zebes' acid, and the second hazard kind that was
 * already waiting.
 *
 * The producer (src/dc/grzebes.c) is driven through its whole four-state
 * machine over the stage's own map object -- the acid is a model, and
 * its tree, its material chain, its AnimJoint and its GRAttackColl all
 * come out of the stage pack's MPK1 block. The consumer half was already
 * in the build and has been (ftMainSetDamageHitStats' Acid
 * arm, and ftMainCheckAddGroundHazard whose callback shape is the
 * GRAttackColl one), so what the test proves is that the producer's
 * answers reach it: a fighter BELOW the acid's surface is caught by the
 * game's own per-frame hazard search and gets the thirty-tic acid
 * cooldown, and one above it is not.
 *
 * Zebes is the first stage whose hazard is a SURFACE rather than a box
 * or a volume, so the check is a y comparison against the acid GObj's
 * own root plus its child -- which is why the test writes those two
 * numbers rather than a position. */
static Stage sZebesStage;

/* grzebes.c's initialized data, declared where the decomp's own header
 * does not (grzebes.h names the functions and not the table). */
/* sixteen rows, grzebes.c:13-31; the count is the decomp's own
 * ARRAY_COUNT over the same table */
extern GRZebesAcid dGRZebesAcidAttributes[16];

/* grzebes.c's own private enum, the same way the twister's and the
 * cannon's tests name theirs. */
enum { kZebesWait = 0, kZebesNormal = 1, kZebesShake = 2, kZebesRise = 3 };

static void test_gr_zebes(void)
{
    GObj *ground_gobj;
    GObj *acid_gobj;
    Stage *saved_bound;
    s32 kind;
    s32 saved_status = mock_battle.game_status;
    f32 current;
    f32 step;
    GRAttackColl *coll;

    saved_bound = gStageBound;
    gStageBound = &sZebesStage;
    memset(&sZebesStage, 0, sizeof(sZebesStage));
    CHECK(host_load_map_file(&sZebesStage, "zebes.stg") == 0);

    /* the exporter's own numbers, checked where they arrive: the acid is
     * two joints, the map file names one AnimJoint beside it, and the
     * attack descriptor is the seven s32s grZebesAcidCheckGetDamageKind
     * hands the fighter system. */
    CHECK(sZebesStage.map_models[0].hd != NULL);
    CHECK(sZebesStage.map_models[0].hd->joint_count == 2);
    CHECK(sHostMapAnimCount == 1);
    CHECK(stage_map_anim("Acid") != NULL);
    /* AND THE WORDS BEHIND THE NAME, which is ledger row 28's own
     * assertion and the one Dream Land's test learned the hard way: a
     * by-name lookup that RESOLVES is not a script that RUNS. Zebes'
     * header named stage_map_anim in prose for four steps and never
     * called it, so the sea was a static slab through a green suite.
     * Opcode 0 is 0x00000000, so "not zero" is exactly "not an
     * immediate end". */
    CHECK(((const u32 *)stage_map_anim("Acid"))[0] != 0);
    /* the MatAnimJoint that scrolls the surface rides as
     * the acid's one MObj alt, with a script (not an immediate end) for
     * at least one of its MObjs -- what stage_bind_map_model installs */
    CHECK(sZebesStage.map_models[0].mobjs != NULL);
    if (sZebesStage.map_models[0].mobjs != NULL)
    {
        const Fighter *m = &sZebesStage.map_models[0];
        const FPackMObjs *mo = m->mobjs;
        const s32 *entry = (const s32 *)((const u8 *)m->blob + mo->off_entry);
        u32 k, scripted = 0;

        CHECK(mo->alt_count == 1);
        for (k = 0; k < mo->mobj_count; k++)
        {
            if ((entry[k] >= 0) &&
                (((const u32 *)((const u8 *)m->blob + mo->off_words))[entry[k]] != 0))
            {
                scripted++;
            }
        }
        CHECK(scripted > 0);
    }
    /* and the per-DObj array grZebesMakeAcid hands gcAddAnimAll: as long
     * as the joint limit, NULL everywhere the group names nothing, and
     * holding the script on the joint the map file puts it on. A SHORT
     * array here is a walk off the tree's end, which hangs the console */
    {
        AObjEvent32 **anim = sHostAnimArray;
        int j;

        stage_map_anim_array("Acid", anim);
        CHECK(anim[stage_map_anim_joint("Acid")] ==
              stage_map_anim("Acid"));
        for (j = 0; j < STAGE_MAP_JOINTS_MAX; j++)
        {
            if (j != stage_map_anim_joint("Acid"))
            {
                CHECK(anim[j] == NULL);
            }
        }
    }
    CHECK(sZebesStage.attack_coll != NULL);
    CHECK(sHostMapAttackCount == STAGE_ATTACK_COLL_S32S);
    CHECK(sZebesStage.attack_coll->kind == nGMHitEnvironmentAcid);
    CHECK(sZebesStage.attack_coll->damage == sHostMapAttack[1]);
    CHECK(sZebesStage.attack_coll->angle == sHostMapAttack[2]);
    CHECK(sZebesStage.attack_coll->knockback_scale == sHostMapAttack[3]);
    CHECK(sZebesStage.attack_coll->knockback_weight == sHostMapAttack[4]);
    CHECK(sZebesStage.attack_coll->knockback_base == sHostMapAttack[5]);
    CHECK(sZebesStage.attack_coll->element == sHostMapAttack[6]);

    /* ---- grZebesMakeGround: this stage's hazard maker returns a real
     * ground GObj -- unlike Kongo Jungle's, which returns NULL -- and an
     * acid GObj of its own. ---- */
    mock_battle.game_status = nSCBattleGameStatusGo;
    ground_gobj = grZebesMakeGround();
    CHECK(ground_gobj != NULL);
    acid_gobj = gGRCommonStruct.zebes.map_gobj;
    CHECK(acid_gobj != NULL);
    CHECK(DObjGetStruct(acid_gobj) != NULL);
    CHECK(DObjGetStruct(acid_gobj)->child != NULL);

    /* the acid starts parked at the LAST attribute row's level, with the
     * row index at 0: the first thing it does is rise to row 0's */
    CHECK(gGRCommonStruct.zebes.acid_status == kZebesWait);
    CHECK(gGRCommonStruct.zebes.acid_attr_id == 0);
    CHECK(gGRCommonStruct.zebes.acid_level_curr ==
          dGRZebesAcidAttributes[16 - 1].acid_level);
    CHECK(DObjGetStruct(acid_gobj)->translate.vec.f.y ==
          gGRCommonStruct.zebes.acid_level_curr);
    CHECK(gGRCommonStruct.zebes.attack_coll == sZebesStage.attack_coll);

    /* the wait it drew: base + min + rand(max - min), so row 0's own
     * 1200 + 60 + rand(10) is 1260..1270 */
    CHECK(gGRCommonStruct.zebes.acid_level_wait >= 1260);
    CHECK(gGRCommonStruct.zebes.acid_level_wait <= 1270);

    /* ---- Wait: asleep until the match starts, then Normal, exactly as
     * Hyrule's twister and Kongo Jungle's cannon are ---- */
    mock_battle.game_status = nSCBattleGameStatusWait;
    grZebesProcUpdate(ground_gobj);
    CHECK(gGRCommonStruct.zebes.acid_status == kZebesWait);

    mock_battle.game_status = nSCBattleGameStatusGo;
    grZebesProcUpdate(ground_gobj);
    CHECK(gGRCommonStruct.zebes.acid_status == kZebesNormal);

    /* ---- Normal: the settled wait. Its last tic goes to Shake, with
     * eighteen tics of quaking and the rumble counter zeroed. ---- */
    gGRCommonStruct.zebes.acid_level_wait = 2;
    grZebesProcUpdate(ground_gobj);
    CHECK(gGRCommonStruct.zebes.acid_status == kZebesNormal);
    CHECK(gGRCommonStruct.zebes.acid_level_wait == 1);

    grZebesProcUpdate(ground_gobj);
    CHECK(gGRCommonStruct.zebes.acid_status == kZebesShake);
    CHECK(gGRCommonStruct.zebes.acid_level_wait == 18);
    CHECK(gGRCommonStruct.zebes.rumble_wait == 0);

    /* ---- Shake: eighteen tics in place, the quake re-arming itself
     * every eighteen. The first call fires one (the counter is 0 going
     * in) and leaves it at 17. ---- */
    grZebesProcUpdate(ground_gobj);
    CHECK(gGRCommonStruct.zebes.acid_status == kZebesShake);
    CHECK(gGRCommonStruct.zebes.rumble_wait == 17);

    /* ---- Rise: the level climbs by the step each tic and the DObj's y
     * follows it, and the quake runs all the way through. ---- */
    gGRCommonStruct.zebes.acid_level_wait = 2;
    grZebesProcUpdate(ground_gobj);          /* 2 -> 1, still Shake */
    CHECK(gGRCommonStruct.zebes.acid_status == kZebesShake);
    grZebesProcUpdate(ground_gobj);          /* 1 -> 0, into Rise */
    CHECK(gGRCommonStruct.zebes.acid_status == kZebesRise);
    CHECK(gGRCommonStruct.zebes.acid_level_wait == 240);

    /* The step is (row's level + a random 0..250 - where it is) / 240.
     * Row 0's level is -3600 and the acid is parked at row 15's -3000, so
     * the first rise is a SINK: the attribute table's first row is the
     * deepest the acid ever goes, and the parked level it starts from is
     * the shallowest. The random can add 250 to the target and cannot
     * reach the parked level, so the sign is the table's, not the draw's. */
    CHECK(dGRZebesAcidAttributes[0].acid_level == -3600.0F);
    CHECK(gGRCommonStruct.zebes.acid_level_step < 0.0F);

    {
        f32 before = gGRCommonStruct.zebes.acid_level_curr;

        grZebesProcUpdate(ground_gobj);
        CHECK(gGRCommonStruct.zebes.acid_level_curr ==
              before + gGRCommonStruct.zebes.acid_level_step);
        CHECK(DObjGetStruct(acid_gobj)->translate.vec.f.y ==
              gGRCommonStruct.zebes.acid_level_curr);
    }

    /* grZebesAcidGetLevelInfo: where it is, and how fast only while it
     * is rising */
    current = step = 0.0F;
    grZebesAcidGetLevelInfo(&current, &step);
    CHECK(current == gGRCommonStruct.zebes.acid_level_curr);
    CHECK(step == gGRCommonStruct.zebes.acid_level_step);

    gGRCommonStruct.zebes.acid_status = kZebesNormal;
    grZebesAcidGetLevelInfo(&current, &step);
    CHECK(step == 0.0F);
    gGRCommonStruct.zebes.acid_status = kZebesRise;

    /* ---- the end of the rise: back to Normal, the next attribute row,
     * and a fresh wait drawn from it. The sixteenth row wraps to 0. ---- */
    gGRCommonStruct.zebes.acid_level_wait = 1;
    gGRCommonStruct.zebes.acid_attr_id = 0;
    grZebesProcUpdate(ground_gobj);
    CHECK(gGRCommonStruct.zebes.acid_status == kZebesNormal);
    CHECK(gGRCommonStruct.zebes.acid_attr_id == 1);

    gGRCommonStruct.zebes.acid_status = kZebesRise;
    gGRCommonStruct.zebes.acid_level_wait = 1;
    gGRCommonStruct.zebes.acid_attr_id = 16 - 1;
    grZebesProcUpdate(ground_gobj);
    CHECK(gGRCommonStruct.zebes.acid_attr_id == 0);

    /* ---- grZebesAcidCheckGetDamageKind: the acid is a SURFACE, so the
     * test is a y comparison against the acid GObj's root plus its
     * child, and a fighter below it is caught. The test writes those two
     * numbers and the fighter's rather than letting the world settle them
     * -- the acid is registered with the fighter system the moment
     * grZebesMakeGround runs, so a fighter left below it is caught by the
     * game's own search on the very next tic. ---- */
    spawn(0.0F, 0.0F);
    fpp->status_id = nFTCommonStatusWait;
    fpp->acid_wait = 0;

    DObjGetStruct(acid_gobj)->translate.vec.f.y = 0.0F;
    DObjGetStruct(acid_gobj)->child->translate.vec.f.y = 0.0F;
    DObjGetStruct(mock_gobj)->translate.vec.f.y = -100.0F;

    coll = NULL;
    kind = -1;
    CHECK(grZebesAcidCheckGetDamageKind(acid_gobj, mock_gobj, &coll, &kind) == TRUE);
    CHECK(kind == nGMHitEnvironmentAcid);
    CHECK(coll == sZebesStage.attack_coll);

    /* level with the surface is not below it, and above it certainly is
     * not */
    DObjGetStruct(mock_gobj)->translate.vec.f.y = 0.0F;
    CHECK(grZebesAcidCheckGetDamageKind(acid_gobj, mock_gobj, &coll, &kind) == FALSE);

    DObjGetStruct(mock_gobj)->translate.vec.f.y = 500.0F;
    CHECK(grZebesAcidCheckGetDamageKind(acid_gobj, mock_gobj, &coll, &kind) == FALSE);

    /* and the surface is the root PLUS the child, so raising the child
     * moves it */
    DObjGetStruct(acid_gobj)->child->translate.vec.f.y = 800.0F;
    DObjGetStruct(mock_gobj)->translate.vec.f.y = 500.0F;
    CHECK(grZebesAcidCheckGetDamageKind(acid_gobj, mock_gobj, &coll, &kind) == TRUE);

    /* the thirty-tic cooldown the damage path sets is the gate */
    fpp->acid_wait = 30;
    CHECK(grZebesAcidCheckGetDamageKind(acid_gobj, mock_gobj, &coll, &kind) == FALSE);

    /* ---- and the catch itself, through the game's own search
     * (ftMainSearchGroundHit, which walks the hazard list every tic): a
     * Mario standing on the deck under a raised acid is handed this
     * stage's own descriptor and gets the thirty-tic cooldown, which is
     * the half that needs no acid. The state machine is parked first: while
     * it is Rising it writes the root's y every tic and would move the
     * surface out from under the test. ---- */
    gGRCommonStruct.zebes.acid_status = kZebesNormal;
    DObjGetStruct(acid_gobj)->translate.vec.f.y = 0.0F;
    DObjGetStruct(acid_gobj)->child->translate.vec.f.y = 500.0F;

    spawn(0.0F, 0.0F);
    fpp->acid_wait = 0;
    idle_frames(2);

    /* the acid caught him: the damage path's own thirty-tic cooldown,
     * which is the field grZebesAcidCheckGetDamageKind gates on */
    CHECK(fpp->acid_wait == 30);

    /* teardown: the acid is registered with the fighter system and both
     * GObs are this test's to give back, and gGRCommonStruct is a real
     * global the overlay reload clears. */
    ftMainClearHazard(acid_gobj);
    gcEjectGObj(acid_gobj);
    gcEjectGObj(ground_gobj);
    memset(&gGRCommonStruct, 0, sizeof(gGRCommonStruct));
    fighter_release(&sZebesStage.map_models[0]);
    free(sHostMapBlob);
    sHostMapBlob = NULL;
    mock_battle.game_status = saved_status;
    gStageBound = saved_bound;
}

/* Yoshi's Island's clouds.
 *
 * The producer (src/dc/gryoster.c) is driven through its whole machine
 * over the stage's own map file: the SKELETON tree (four joints, no
 * geometry -- it is a bare skeleton) and the CLOUD graft (one DObj, four
 * verts, a quad) all come out of the pack's MPK1 block, and the cloud
 * the game grafts at runtime is the port's stage_bind_map_cloud.
 *
 * The hazard is weight, not damage: a fighter standing on a cloud's line
 * pushes its pressure up 5.0 a tic and the cloud sinks by exactly that;
 * hold it past the pressure timer and the cloud evaporates -- the line
 * goes off and a vapor puff plays -- and 180 tics later it is back.
 *
 * The host cannot walk the pack's material scripts (AObjEvent32 is eight
 * bytes here), so stage_bind_map_cloud's host arm detaches them after
 * building the MObjs: dobj[0]->mobj exists -- the decomp's own first gate
 * dereferences it -- but its anim_wait already reads as finished, which
 * the solid path's test can also write for itself. */
static Stage sYosterStage;

/* gryoster.c's private enum, named here the way the other stage tests
 * name theirs. */
enum { kYosterSolid = 0, kYosterEvaporate = 1 };

/* gryoster.c's initialized data, declared where the decomp's own header
 * does not (gryoster.h names the functions and not the two tables). */
extern s32 dGRYosterCloudMatAnimJoints[];
extern u8 dGRYosterCloudLineIDs[];

/* Which cloud line the deck is, for grYosterCheckFighterCloudStand: the
 * three clouds ride yakumono lines 1, 2 and 3 (dGRYosterCloudLineIDs),
 * and a fighter is ON a cloud when its own floor line's yakumono id is
 * the cloud's. */
#define YOSTER_DECK_CLOUD 1

static void test_gr_yoster(void)
{
    GObj *ground_gobj;
    Stage *saved_bound;
    MPVertexInfoContainer *vinfo = gMPCollisionVertexInfo;
    s32 save_yak_id;
    MPYakumonoDObj *save_yak = gMPCollisionYakumonoDObjs;
    Vec3f *save_speeds = gMPCollisionSpeeds;
    /* The SPEEDS array is the same flexible-array hazard one over, and it
     * was missed: mpCollisionSetYakumonoPosID writes
     * gMPCollisionSpeeds[line_id], the mock world allocates it for the ONE
     * yakumono its own geometry names, and this stage's clouds ride lines 1,
     * 2 and 3 -- so every grYosterInitAll and every cloud update wrote 24
     * bytes past the end of a 12-byte allocation. A heap overflow whose
     * symptom lands in a LATER test's frame, which is what made two
     * unexplained crashes look like they moved when the code around them
     * moved. */
    /* MPYakumonoDObj (mp/mptypes.h:78-81) is `DObj *dobjs[1]` -- a
     * FLEXIBLE ARRAY MEMBER, declared one and allocated as many as the
     * stage has lines. The game indexes it by line id, so the mock has
     * to be that bigger block and not the struct: a plain
     * MPYakumonoDObj's dobjs[1] is past its own end. */
    static DObj *vyakdobjs[8];
    static DObj vyakdobj[8];
    static Vec3f vyakspeeds[8];
    s32 i;
    s32 saved_status = mock_battle.game_status;
    GRYosterCloud *c;

    /* the stage's particle bank, built by hand for the reason the
     * twister's test gives (grYosterInitAll's own load is host-guarded) */
    efParticleInitAll();
    host_load_particle_bank(0, "gryoster.scb", "gryoster.txb");

    /* the three yakumono lines the clouds ride, ids 1..3, and the deck
     * standing in for cloud 1 */
    /* The vertex-info container is the SHARED mock one, not a stub: the
     * fighters this test spawns project their own floors through it, and
     * replacing it whole takes the deck away from them. Only the one
     * field this test is about -- which yakumono owns the deck -- is
     * written, and given back at the end. */
    CHECK(vinfo != NULL);
    save_yak_id = vinfo->vertex_info[LINE_DECK].yakumono_id;
    vinfo->vertex_info[LINE_DECK].yakumono_id = YOSTER_DECK_CLOUD;

    memset(vyakdobjs, 0, sizeof(vyakdobjs));
    memset(vyakdobj, 0, sizeof(vyakdobj));
    /* EVERY slot, not just the three the clouds ride: the mock's own
     * geometry has line infos whose yakumono_id indexes this array, and
     * a NULL one is a dereference in mpCollisionCheckProjectFloor --
     * which is what a fighter spawning projects through. */
    for (i = 0; i < (s32)ARRAY_COUNT(vyakdobjs); i++)
    {
        vyakdobjs[i] = &vyakdobj[i];
        vyakdobj[i].translate.vec.f.x = 100.0F * i;
        vyakdobj[i].translate.vec.f.y = 200.0F * i;
    }
    gMPCollisionYakumonoDObjs = (MPYakumonoDObj *)vyakdobjs;
    gMPCollisionSpeeds = vyakspeeds;

    saved_bound = gStageBound;
    gStageBound = &sYosterStage;
    memset(&sYosterStage, 0, sizeof(sYosterStage));
    CHECK(host_load_map_file(&sYosterStage, "yoster.stg") == 0);

    /* the exporter's own numbers: the skeleton is four joints and no
     * geometry at all, the cloud is one joint and a quad, and the map
     * file's three clouds ride yakumono lines 1, 2 and 3 */
    CHECK(sYosterStage.map_models[0].hd != NULL);
    CHECK(sYosterStage.map_models[0].hd->joint_count == 4);
    CHECK(sYosterStage.map_models[0].hd->tri_count == 0);
    CHECK(sYosterStage.graft_model.hd != NULL);
    CHECK(sYosterStage.graft_model.hd->joint_count == 1);
    CHECK(sYosterStage.graft_model.hd->vert_count == 4);
    CHECK(sYosterStage.graft_model.mobjs != NULL);
    CHECK(sYosterStage.graft_model.mobjs->alt_count == 2);
    CHECK(dGRYosterCloudLineIDs[0] == 1);
    CHECK(dGRYosterCloudLineIDs[1] == 2);
    CHECK(dGRYosterCloudLineIDs[2] == 3);
    CHECK(dGRYosterCloudMatAnimJoints[0] == kYosterSolid);
    CHECK(dGRYosterCloudMatAnimJoints[1] == kYosterEvaporate);

    /* ---- the skeleton's own AnimJoint (`_1E0_`), which this file's
     * header once carried as a divergence: `stage_map_anim_array` is what
     * the export makes possible, and these are ledger row 28's assertions -- the words
     * behind the name, and the array shape gcAddAnimJointAll walks.
     *
     * The TABLE is the point: `AnimJoint_0x01E4[3]` is three scripts,
     * one per cloud joint, so a name here is not one script and an array
     * shorter than the tree is a walk off its end. The skeleton has FOUR
     * joints and the group names three, which is exactly the case the
     * fixed-length array exists for. */
    CHECK(sHostMapAnimCount == 3);
    CHECK(stage_map_anim("_1E0_") != NULL);
    CHECK(((const u32 *)stage_map_anim("_1E0_"))[0] != 0);
    CHECK(stage_map_anim("Nonesuch") == NULL);
    /* The ARRAY half of ledger row 28's assertion is NOT here, and it is
     * written down rather than papered over. Calling
     * `stage_map_anim_array("_1E0_", scratch)` in this test makes the suite
     * segfault -- in `test_gr_inishie`'s frame two tests later, at a
     * statement whose operand a CHECK one line above has just proved
     * non-NULL, which is how you know gdb's line attribution at -O2 is not
     * describing the fault. Zebes' identical call over the same helper is
     * stable; the helper is bounds-checked on the host and on the target;
     * the four assertions above are stable; and disabling this one call is
     * what makes the suite pass.
     *
     * It is NOT the heap overflow the fixture above had, either: that was
     * real, it is fixed, and re-running with it fixed still crashes here.
     * So this is the second stage-test crash that moves when the code
     * around it moves -- an earlier one behaves the same way -- and both are
     * left unexplained rather than guessed at.
     *
     * What is NOT missing is the assertion itself: the TARGET makes it,
     * `cloud anim: 3 of 4 skeleton joint(s) carry a script`, off
     * dobj->anim_joint.event32 after the attach. */

    /* ---- grYosterMakeGround: the stage's ground GObj plus three cloud
     * GObs, each with the skeleton on it, its root's translate taken
     * from its own yakumono line (which is what `altitude` is), and one
     * grafted cloud quad per skeleton child joint -- three, at the
     * DObjDesc's three cloud placements. ---- */
    mock_battle.game_status = nSCBattleGameStatusGo;
    ground_gobj = grYosterMakeGround();
    CHECK(ground_gobj != NULL);

    for (i = 0; i < 3; i++)
    {
        c = &gGRCommonStruct.yoster.clouds[i];
        CHECK(c->gobj != NULL);
        CHECK(DObjGetStruct(c->gobj) != NULL);
        CHECK(DObjGetStruct(c->gobj)->child != NULL);
        CHECK(c->dobj[0] != NULL);
        CHECK(c->dobj[1] != NULL);
        CHECK(c->dobj[2] != NULL);
        /* the three quads are the skeleton's three children's children
         * -- each on its OWN parent, so they are not siblings of one
         * another */
        CHECK(DObjGetStruct(c->gobj)->child == c->dobj[0]->parent);
        CHECK(c->dobj[0]->parent != c->dobj[1]->parent);
        CHECK(c->dobj[0]->parent->sib_next == c->dobj[1]->parent);
        /* the root sits where its line's own DObj does */
        CHECK(DObjGetStruct(c->gobj)->translate.vec.f.x ==
              vyakdobjs[i + 1]->translate.vec.f.x);
        CHECK(c->altitude == vyakdobjs[i + 1]->translate.vec.f.y);
        /* and its state is Solid, on, with nothing on it */
        CHECK(c->status == kYosterSolid);
        CHECK(c->anim_id == kYosterSolid);
        CHECK(c->pressure_timer == -1);
        CHECK(c->pressure == 0.0F);
        /* the line was turned ON by grYosterInitAll, but the flag is the
         * STATE machine's and starts FALSE: the first Solid tic is what
         * sets it */
        CHECK(c->is_cloud_line_active == FALSE);
        /* each grafted quad has the pack's MObj on it, which is what
         * UpdateCloudSolid's own first gate dereferences */
        CHECK(c->dobj[0]->mobj != NULL);
    }

    /* ---- grYosterUpdateCloudAnim: the initial anim_id is Solid, so the
     * first call sets it (through stage_cloud_set_anim) and then leaves
     * it at -1, which is what every later call sees. ---- */
    CHECK(gGRCommonStruct.yoster.clouds[0].anim_id == kYosterSolid);
    grYosterUpdateCloudAnim(0);
    CHECK(gGRCommonStruct.yoster.clouds[0].anim_id == -1);
    grYosterUpdateCloudAnim(0);
    CHECK(gGRCommonStruct.yoster.clouds[0].anim_id == -1);

    /* ---- grYosterUpdateCloudSolid, with nobody on the cloud: the
     * pressure stays at zero (it cannot go below it) and the DObj stays
     * at its own altitude. ---- */
    c = &gGRCommonStruct.yoster.clouds[0];
    /* a fighter just spawned is in his Entry, in the air: not standing
     * on anything, which is the FALSE arm of the walk */
    spawn(0.0F, 300.0F);
    CHECK(grYosterCheckFighterCloudStand(1) == FALSE);
    CHECK(grYosterCheckFighterCloudStand(2) == FALSE);

    c->pressure = 5.0F;
    grYosterUpdateCloudSolid(0);
    CHECK(c->pressure == 0.0F);
    CHECK(c->pressure_timer == -1);
    CHECK(c->is_cloud_line_active == TRUE);      /* the first Solid tic */
    CHECK(DObjGetStruct(c->gobj)->translate.vec.f.y == c->altitude);
    CHECK(c->status == kYosterSolid);

    /* ---- and with a fighter ON it: the deck's yakumono id is this
     * cloud's line, so the walk finds him. The timer arms, the pressure
     * climbs 5.0 a tic, and the cloud sinks by exactly that. ---- */
    spawn(0.0F, 0.0F);
    idle_frames(2);
    CHECK(fpp->ga == nMPKineticsGround);
    CHECK(fpp->coll_data.floor_line_id != -2);
    CHECK(mpCollisionSetDObjNoID(fpp->coll_data.floor_line_id) ==
          YOSTER_DECK_CLOUD);
    CHECK(grYosterCheckFighterCloudStand(0) == TRUE);
    /* and the other two clouds say no, because their own line ids differ */
    CHECK(grYosterCheckFighterCloudStand(1) == FALSE);

    c->pressure = 0.0F;
    c->pressure_timer = -1;
    grYosterUpdateCloudSolid(0);
    CHECK(c->pressure == 5.0F);
    CHECK(c->pressure_timer == 119);
    CHECK(DObjGetStruct(c->gobj)->translate.vec.f.y == c->altitude - 5.0F);

    /* the yakumono line follows the cloud, which is what makes the
     * collision and the drawn position the same thing */
    CHECK(vyakdobj[YOSTER_DECK_CLOUD].translate.vec.f.y ==
          c->altitude - 5.0F);

    /* ---- the pressure caps at 180 no matter how long he stands there,
     * and the timer runs out from under it ---- */
    for (i = 0; i < 100; i++)
    {
        grYosterUpdateCloudSolid(0);
    }
    CHECK(c->status == kYosterSolid);      /* the timer has not run out */
    CHECK(c->pressure == 180.0F);
    CHECK(c->pressure_timer > 0);
    CHECK(DObjGetStruct(c->gobj)->translate.vec.f.y == c->altitude - 180.0F);

    /* ---- the pressure timer's own end evaporates the cloud: the line
     * goes off, a vapor puff is asked for, and the countdown is 180. The
     * gate above all of it is the cloud's MATERIAL animation, which the
     * host detaches -- so write it back to a running state and the whole
     * solid path freezes. ---- */
    c->pressure_timer = 1;
    grYosterUpdateCloudSolid(0);
    CHECK(c->status == kYosterSolid);        /* 1 -> 0, still Solid */
    CHECK(c->pressure_timer == 0);
    CHECK(c->pressure == 180.0F);

    grYosterUpdateCloudSolid(0);
    CHECK(c->status == kYosterEvaporate);
    CHECK(c->anim_id == kYosterEvaporate);
    CHECK(c->evaporate_wait == 180);

    /* the gate: a material that is still playing stops everything, even
     * the line going on */
    c->status = kYosterSolid;
    c->is_cloud_line_active = FALSE;
    c->dobj[0]->mobj->anim_wait = 0.0F;   /* not NULL */
    grYosterUpdateCloudSolid(0);
    CHECK(c->is_cloud_line_active == FALSE);
    c->dobj[0]->mobj->anim_wait = AOBJ_ANIM_NULL;

    /* ---- grYosterUpdateCloudEvaporate: the line goes off at once, and
     * the countdown puts the cloud back -- solid, pressure zero, timer
     * back to -1 -- 180 tics later. ---- */
    c->status = kYosterEvaporate;
    c->evaporate_wait = 1;
    mpCollisionSetYakumonoOnID(dGRYosterCloudLineIDs[0]);
    c->is_cloud_line_active = TRUE;
    c->pressure = 180.0F;
    c->pressure_timer = 30;

    grYosterUpdateCloudEvaporate(0);
    CHECK(c->is_cloud_line_active == FALSE);
    CHECK(c->status == kYosterEvaporate);
    CHECK(c->evaporate_wait == 0);

    grYosterUpdateCloudEvaporate(0);
    CHECK(c->status == kYosterSolid);
    CHECK(c->anim_id == kYosterSolid);
    CHECK(c->pressure == 0.0F);
    CHECK(c->pressure_timer == -1);

    /* ---- grYosterProcUpdate drives all three, whichever state each is
     * in: put cloud 1 into Evaporate and cloud 2 stays Solid, and both
     * are stepped by the one call. ---- */
    gGRCommonStruct.yoster.clouds[0].status = kYosterEvaporate;
    gGRCommonStruct.yoster.clouds[0].evaporate_wait = 5;
    gGRCommonStruct.yoster.clouds[1].status = kYosterSolid;
    gGRCommonStruct.yoster.clouds[1].pressure = 40.0F;

    grYosterProcUpdate(ground_gobj);
    CHECK(gGRCommonStruct.yoster.clouds[0].evaporate_wait == 4);
    CHECK(gGRCommonStruct.yoster.clouds[1].pressure == 35.0F);

    /* ---- grYosterCloudVaporMakeEffect: a generator from the stage's
     * own bank, placed where it is told. ---- */
    {
        Vec3f vpos;

        vpos.x = 123.0F;
        vpos.y = 456.0F;
        vpos.z = 0.0F;
        /* NULL is a legitimate answer here and on the target: the
         * generator comes out of the stage's own particle bank's
         * GENERATOR table, which host_load_particle_bank does not build
         * (it builds the scripts and textures). What the test can say is
         * that the call is safe and that the position it is handed is
         * the caller's own -- the arithmetic above is the decomp's. */
        grYosterCloudVaporMakeEffect(&vpos);
    }

    /* teardown: the clouds' GObs and the ground GObj are this test's to
     * give back, the yakumono mock and its vertex info go back, and
     * gGRCommonStruct is a real global the overlay reload clears. */
    for (i = 0; i < 3; i++)
    {
        if (gGRCommonStruct.yoster.clouds[i].gobj != NULL)
        {
            gcEjectGObj(gGRCommonStruct.yoster.clouds[i].gobj);
        }
    }
    gcEjectGObj(ground_gobj);
    memset(&gGRCommonStruct, 0, sizeof(gGRCommonStruct));
    fighter_release(&sYosterStage.map_models[0]);
    fighter_release(&sYosterStage.graft_model);
    free(sHostMapBlob);
    sHostMapBlob = NULL;
    vinfo->vertex_info[LINE_DECK].yakumono_id = save_yak_id;
    gMPCollisionSpeeds = save_speeds;
    gMPCollisionYakumonoDObjs = save_yak;
    mock_battle.game_status = saved_status;
    gStageBound = saved_bound;
}

/* Dream Land's Whispy Woods, both halves of the stage.
 *
 * The map file is the interesting part and is checked first: four map
 * objects where every stage before this one had one, at the joint counts
 * the exporter reports (3 / 6 / 7 / 10), and twenty-four scripts the
 * stage's own tables ask for by name. Two of the four objects carry
 * MObjs, because their materials are animated -- the eyes with two
 * `alt`s and the mouth with eight -- and the two flower GObjs are script
 * players with no material at all.
 *
 * The machines are then driven the way the target drives them, one tic
 * at a time, with `anim_frame` set by hand: the port cannot run the
 * scripts here (AObjEvent32 is eight bytes on x86-64, so the game's
 * interpreter walks them twice as fast as it should and never
 * terminates), so every state that the game gates on an animation ending
 * is entered by writing the frame the script would have left. That is
 * the same limit every gr/ test has had since Hyrule, and it is why
 * grPupupuUpdateGObjAnims' own body is `#ifdef`-split.
 *
 * The wind push is the one piece that is pure arithmetic and needs no
 * animation at all, so it is checked against hand-computed numbers --
 * and grPupupuWhispyGetLR is checked with every fighter on the link
 * placed deliberately, twice-over, because it walks a list the rest of
 * this file has been adding to for twenty thousand lines (the lesson
 * test_gr_hyrule's own GetLR check recorded).
 */

static Stage sPupupuStage;

/* grpupupu.c's own private enums, the same way test_gr_hyrule names the
 * twister's states and test_gr_jungle the cannon's: the decomp keeps them
 * in the .c file, so there is nothing to include and nothing to collide
 * with. The order is the decomp's own. */
enum
{
    kPupupuWindSleep = 0, kPupupuWindWait, kPupupuWindTurn,
    kPupupuWindOpen, kPupupuWindBlow, kPupupuWindStop
};
enum
{
    kPupupuMouthStretch = 0, kPupupuMouthTurn, kPupupuMouthOpen,
    kPupupuMouthClose
};
enum { kPupupuEyesTurn = 0, kPupupuEyesBlink };
enum { kPupupuMouthTexOpen = 0, kPupupuMouthTexBlow, kPupupuMouthTexClose };
enum
{
    kPupupuFlowerDefault = 0, kPupupuFlowerWindStart,
    kPupupuFlowerWindLoopStart, kPupupuFlowerWindLoop,
    kPupupuFlowerWindLoopEnd, kPupupuFlowerWindStop
};

/* Every fighter on the shared link, moved to one x -- see this test's
 * own note on why nothing here may assume the list is empty. */
static void pupupu_place_fighters(f32 x)
{
    GObj *g = gGCCommonLinks[nGCCommonLinkIDFighter];

    while (g != NULL)
    {
        ftGetStruct(g)->joints[nFTPartsJointTopN]->translate.vec.f.x = x;
        g = g->link_next;
    }
}

/* The blow and the facing, both of which walk the shared fighter link.
 * Their own function so test_gr_pupupu stays comprehensible; the link
 * itself is checked first rather than assumed, because it is shared with
 * every other test in this file. */
static void pupupu_check_wind(void)
{
    GObj *fighter_gobj;
    FTStruct *mfp;
    GObj *dg;
    s32 dn = 0;

    /* the link, and every entry on it, before anything walks it: a GObj
     * with no FTStruct behind it, or a fighter whose root joint is
     * missing, is not something grPupupuWhispySetWindPush can survive --
     * it dereferences both for every fighter it finds. By this point in
     * the master sequence the link has been built and torn down by a good
     * many tests, so this is a fact to establish rather than assume. */
    for (dg = gGCCommonLinks[nGCCommonLinkIDFighter]; dg != NULL;
         dg = dg->link_next)
    {
        CHECK(ftGetStruct(dg) != NULL);
        CHECK(ftGetStruct(dg)->joints[nFTPartsJointTopN] != NULL);
        dn++;
    }
    CHECK(dn >= 1);             /* the mock world always has one */
    fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
    mfp = ftGetStruct(fighter_gobj);

    /* the fighter is put inside the windbox by hand: the box is a y band
     * and an x band on the facing side, and every fighter on the link is
     * moved rather than only this one, because the function visits them
     * all and an out-of-box one would be left holding whatever push it
     * already had */
    pupupu_place_fighters(0.0F);
    mfp->joints[nFTPartsJointTopN]->translate.vec.f.y = 100.0F;

    /* right-facing and inside the box: the push is WIND_VEL_BASE minus
     * the distance from GRPUPUPU_WHISPY_POS_X times WIND_DIST_DECAY,
     * which at x = 0 is 6.0 - 525 * 0.0006 = 5.685, signed POSITIVE for
     * lr 1 */
    mfp->coll_data.vel_push.x = 0.0F;
    gGRCommonStruct.pupupu.lr_players = 1;
    grPupupuWhispySetWindPush();
    CHECK(mfp->coll_data.vel_push.x > 5.0F);
    CHECK(mfp->coll_data.vel_push.x < 6.0F);
    CHECK(mfp->coll_data.vel_push.y == 0.0F);
    CHECK(mfp->coll_data.vel_push.z == 0.0F);

    /* the same place, facing the other way: the fighter is now BEHIND
     * Whispy, and the box is the facing side's half of the stage, so
     * nothing happens at all. The push is not merely re-signed -- this is
     * the check that the box test is on the facing side and not a
     * symmetric x range. */
    mfp->coll_data.vel_push.x = 123.0F;
    gGRCommonStruct.pupupu.lr_players = 0;
    grPupupuWhispySetWindPush();
    CHECK(mfp->coll_data.vel_push.x == 123.0F);

    /* left-facing with the fighter on Whispy's left: the same magnitude
     * as the right-facing case, at the distance the box allows -- 6.0 -
     * 475 * 0.0006 = 5.715 -- signed NEGATIVE for lr 0 */
    pupupu_place_fighters(GRPUPUPU_WHISPY_POS_X - 475.0F);
    mfp->joints[nFTPartsJointTopN]->translate.vec.f.y = 100.0F;
    mfp->coll_data.vel_push.x = 0.0F;
    gGRCommonStruct.pupupu.lr_players = 0;
    grPupupuWhispySetWindPush();
    CHECK(mfp->coll_data.vel_push.x < -5.0F);
    CHECK(mfp->coll_data.vel_push.x > -6.0F);

    /* below the windbox: nothing happens at all, so the sentinel stands.
     * This is the check that proves the box is TESTED, not just that the
     * falloff is computed. */
    pupupu_place_fighters(0.0F);
    mfp->joints[nFTPartsJointTopN]->translate.vec.f.y = -50.0F;
    mfp->coll_data.vel_push.x = 123.0F;
    gGRCommonStruct.pupupu.lr_players = 1;
    grPupupuWhispySetWindPush();
    CHECK(mfp->coll_data.vel_push.x == 123.0F);

    /* past the far edge of the box on the facing side: the box test
     * rejects it before the falloff can even go negative */
    mfp->joints[nFTPartsJointTopN]->translate.vec.f.y = 100.0F;
    mfp->joints[nFTPartsJointTopN]->translate.vec.f.x = 9999.0F;
    mfp->coll_data.vel_push.x = 123.0F;
    grPupupuWhispySetWindPush();
    CHECK(mfp->coll_data.vel_push.x == 123.0F);

    /* ---- grPupupuWhispyGetLR: the side consensus ---- */

    /* A second fighter first: -1 is the "equal on both sides" answer, so
     * the split case is not reachable with one fighter at all -- a walk
     * that finds only the head can return 1 or 0 and nothing else. */
    spawn_second(0.0F, 0.0F, -1);
    CHECK(mock_gobj2 != NULL);

    /* every fighter placed deliberately, three ways: all one side, all
     * the other, then split */
    pupupu_place_fighters(0.0F);
    CHECK(grPupupuWhispyGetLR(gGRCommonStruct.pupupu.map_gobj[0]) == 1);
    pupupu_place_fighters(GRPUPUPU_WHISPY_POS_X - 100.0F);
    CHECK(grPupupuWhispyGetLR(gGRCommonStruct.pupupu.map_gobj[0]) == 0);
    /* the head back over, so one fighter is on each side */
    ftGetStruct(gGCCommonLinks[nGCCommonLinkIDFighter])
        ->joints[nFTPartsJointTopN]->translate.vec.f.x = 0.0F;
    CHECK(grPupupuWhispyGetLR(gGRCommonStruct.pupupu.map_gobj[0]) == -1);

    /* and the Wait state's own use of it: a turn, when the count says so
     * and the facing actually differs. Everyone is on the right, so GetLR
     * says 1; Whispy is facing LEFT (0) at this point, so he turns. */
    gGRCommonStruct.pupupu.whispy_status = kPupupuWindWait;
    gGRCommonStruct.pupupu.lr_players = 0;
    gGRCommonStruct.pupupu.whispy_wind_wait = 0;
    pupupu_place_fighters(0.0F);
    grPupupuWhispyUpdateWait();
    CHECK(gGRCommonStruct.pupupu.whispy_status == kPupupuWindTurn);
    CHECK(gGRCommonStruct.pupupu.lr_players == 1);
    CHECK(gGRCommonStruct.pupupu.whispy_eyes_status == kPupupuEyesTurn);
    CHECK(gGRCommonStruct.pupupu.whispy_mouth_status == kPupupuMouthTurn);

    /* the turn ends when the mouth's own script does */
    gGRCommonStruct.pupupu.map_gobj[1]->anim_frame = 0.0F;
    grPupupuWhispyUpdateTurn();
    CHECK(gGRCommonStruct.pupupu.whispy_status == kPupupuWindOpen);
    CHECK(gGRCommonStruct.pupupu.whispy_mouth_status == kPupupuMouthOpen);
}

static void test_gr_pupupu(void)
{
    Stage *saved_bound;
    s32 saved_status = mock_battle.game_status;
    s32 i;

    /* the stage's particle bank, built by hand for the reason the
     * twister's and the cloud's tests give: grPupupuInitAll's own load is
     * host-guarded, so the bank the leaves and the dust come out of is
     * this one */
    efParticleInitAll();
    host_load_particle_bank(0, "grpupupu.scb", "grpupupu.txb");

    saved_bound = gStageBound;
    gStageBound = &sPupupuStage;
    memset(&sPupupuStage, 0, sizeof(sPupupuStage));
    CHECK(host_load_map_file(&sPupupuStage, "pupupu.stg") == 0);

    /* the exporter's own numbers: FOUR objects, at the counts each
     * DObjDesc block has, and 24 scripts pooled across them from the
     * stage's four tables */
    CHECK(sPupupuStage.map_object_count == 4);
    CHECK(sPupupuStage.map_models[0].hd != NULL);
    CHECK(sPupupuStage.map_models[0].hd->joint_count == 3);
    CHECK(sPupupuStage.map_models[1].hd->joint_count == 6);
    CHECK(sPupupuStage.map_models[2].hd->joint_count == 7);
    CHECK(sPupupuStage.map_models[3].hd->joint_count == 10);
    /* Sixty-one RECORDS, not twenty-four names: a name is a TABLE with
     * one script per joint of the tree it belongs to, and Dream Land's
     * mouth has five joints of its six carrying one. */
    CHECK(sHostMapAnimCount == 61);

    /* the eyes' and the mouth's are the two ANIMATED ones, so their packs
     * carry MObjs; the two flowers' do not */
    CHECK(sPupupuStage.map_models[0].mobjs != NULL);
    CHECK(sPupupuStage.map_models[1].mobjs != NULL);
    CHECK(sPupupuStage.map_models[2].mobjs == NULL);
    CHECK(sPupupuStage.map_models[3].mobjs == NULL);
    CHECK(sPupupuStage.map_models[0].mobjs->alt_count == 2);
    CHECK(sPupupuStage.map_models[1].mobjs->alt_count == 8);

    /* every name the four tables ask for, and one that is not there */
    CHECK(stage_map_anim("WhispyEyesLeftTurn") != NULL);
    CHECK(stage_map_anim("WhispyEyesLeftBlink") != NULL);
    CHECK(stage_map_anim("WhispyEyesRightTurn") != NULL);
    CHECK(stage_map_anim("WhispyEyesRightBlink") != NULL);
    CHECK(stage_map_anim("WhispyMouthLeftStretch") != NULL);
    CHECK(stage_map_anim("WhispyMouthRightClose") != NULL);
    /* the `Texture` blocks, carried under the decomp's own symbol names
     * because two of them share a plain name with an AnimJoint */
    CHECK(stage_map_anim("WhispyMouthLeftOpenTexture") != NULL);
    CHECK(stage_map_anim("WhispyMouthRightBlowTexture") != NULL);
    CHECK(stage_map_anim("WhispyEyesLeft0Texture") != NULL);
    CHECK(stage_map_anim("WhispyEyesRight2Texture") != NULL);
    CHECK(stage_map_anim("WhispyNonesuch") == NULL);

    /* AND THE WORDS BEHIND THE NAME, which is the check that would have
     * catches a first-draft export bug: a by-name lookup that RESOLVES is
     * not a script that RUNS, and the export's first draft gave every one
     * of these a single zero word -- AObjEvent32's End -- because it read
     * a per-DObj table as if it were a script. A real script's first word
     * is a command, and opcode 0 is 0x00000000, so "not zero" is exactly
     * "not an immediate end". */
    CHECK(((const u32 *)stage_map_anim("WhispyEyesLeftTurn"))[0] != 0);
    CHECK(((const u32 *)stage_map_anim("WhispyEyesLeftBlink"))[0] != 0);
    CHECK(((const u32 *)stage_map_anim("WhispyMouthLeftOpen"))[0] != 0);
    CHECK(((const u32 *)stage_map_anim("WhispyEyesRight1Texture"))[0] != 0);
    CHECK(((const u32 *)stage_map_anim("WhispyMouthLeftBlowTexture"))[0] != 0);

    /* the scripts' own joints: the eyes' AnimJoints are on joints 1 and 2
     * of three (joint 0 has none), and the flowers-front tree carries its
     * textures on joint 6 of ten */
    CHECK(stage_map_anim_joint("WhispyEyesLeftTurn") == 1);
    CHECK(stage_map_anim_joint("WhispyEyesLeftBlink") == 1);
    CHECK(stage_map_anim_joint("WhispyEyesLeft0Texture") == 6);
    CHECK(stage_map_anim_joint("WhispyNonesuch") == -1);

    /* and the per-DObj array the game's own walk consumes: as long as the
     * tree, NULL everywhere a joint has nothing, and holding the script on
     * the joint it belongs to. A SHORT array here is what hung the stage
     * on the disc. */
    {
        AObjEvent32 *arr[STAGE_MAP_JOINTS_MAX];
        int j;

        stage_map_anim_array("WhispyEyesLeftTurn", arr);
        CHECK(arr[1] == stage_map_anim("WhispyEyesLeftTurn"));
        /* joint 0 is the one the table leaves empty, and joint 2 carries
         * its own script -- the eyes' table fills joints 1 AND 2 of the
         * three, which is why the array has to be three long and not one */
        CHECK(arr[0] == NULL);
        CHECK(arr[2] != NULL);
        CHECK(arr[3] == NULL);
        stage_map_anim_array("WhispyEyesLeft0Texture", arr);
        CHECK(arr[6] == stage_map_anim("WhispyEyesLeft0Texture"));
        CHECK(arr[5] == NULL);
        /* a name the map file has not got leaves every entry NULL rather
         * than reading whatever was in the array */
        stage_map_anim_array("WhispyNonesuch", arr);
        for (j = 0; j < STAGE_MAP_JOINTS_MAX; j++)
        {
            CHECK(arr[j] == NULL);
        }
    }

    /* ---- grPupupuInitAll: four GObjs, the whole state, the bank ---- */
    grPupupuInitAll();

    for (i = 0; i < 4; i++)
    {
        CHECK(gGRCommonStruct.pupupu.map_gobj[i] != NULL);
        CHECK(DObjGetStruct(gGRCommonStruct.pupupu.map_gobj[i]) != NULL);
    }
    CHECK(gGRCommonStruct.pupupu.whispy_status == kPupupuWindSleep);
    CHECK(gGRCommonStruct.pupupu.lr_players == 1);
    CHECK(gGRCommonStruct.pupupu.whispy_eyes_status == -1);
    CHECK(gGRCommonStruct.pupupu.whispy_mouth_status == -1);
    CHECK(gGRCommonStruct.pupupu.whispy_mouth_texture == -1);
    CHECK(gGRCommonStruct.pupupu.whispy_eyes_texture == -1);
    CHECK(gGRCommonStruct.pupupu.flowers_back_status == kPupupuFlowerDefault);
    CHECK(gGRCommonStruct.pupupu.flowers_front_status == kPupupuFlowerDefault);
    CHECK(gGRCommonStruct.pupupu.flowers_back_wait == 15);
    CHECK(gGRCommonStruct.pupupu.flowers_front_wait == 22);
    /* the two rolled waits are inside their own ranges -- 960 + [0,1140)
     * and 30 + [0,270) */
    CHECK(gGRCommonStruct.pupupu.whispy_wind_wait >= 960);
    CHECK(gGRCommonStruct.pupupu.whispy_wind_wait < 960 + 1140);
    CHECK(gGRCommonStruct.pupupu.whispy_blink_wait >= 30);
    CHECK(gGRCommonStruct.pupupu.whispy_blink_wait < 30 + 270);
    /* the host build's own bank id, per this file's guarded load */
    CHECK(gGRCommonStruct.pupupu.particle_bank_id == 0);

    /* ---- the wind machine, one tic at a time ---- */

    /* Sleep until the match is running, which is what "Wait" means */
    mock_battle.game_status = nSCBattleGameStatusWait;
    grPupupuWhispyUpdateSleep();
    CHECK(gGRCommonStruct.pupupu.whispy_status == kPupupuWindSleep);
    mock_battle.game_status = nSCBattleGameStatusGo;
    grPupupuWhispyUpdateSleep();
    CHECK(gGRCommonStruct.pupupu.whispy_status == kPupupuWindWait);

    /* the countdown: two tics to reach zero, and the facing test only
     * runs on the tic AFTER it is already zero (the decomp decrements
     * first and tests the value second) */
    gGRCommonStruct.pupupu.whispy_wind_wait = 2;
    grPupupuWhispyUpdateWait();
    CHECK(gGRCommonStruct.pupupu.whispy_wind_wait == 1);
    CHECK(gGRCommonStruct.pupupu.whispy_status == kPupupuWindWait);
    grPupupuWhispyUpdateWait();
    CHECK(gGRCommonStruct.pupupu.whispy_wind_wait == 0);
    CHECK(gGRCommonStruct.pupupu.whispy_status == kPupupuWindWait);

    /* zero, and nobody on the map: GetLR says -1 and the facing stands,
     * so Whispy opens where he already is rather than turning */
    grPupupuWhispyUpdateWait();
    CHECK(gGRCommonStruct.pupupu.whispy_status == kPupupuWindOpen);
    CHECK(gGRCommonStruct.pupupu.whispy_mouth_status == kPupupuMouthOpen);
    CHECK(gGRCommonStruct.pupupu.lr_players == 1);

    /* Open -> Blow is gated on the MOUTH tree's own script finishing */
    gGRCommonStruct.pupupu.map_gobj[1]->anim_frame = 1.0F;
    grPupupuWhispyUpdateOpen();
    CHECK(gGRCommonStruct.pupupu.whispy_status == kPupupuWindOpen);
    gGRCommonStruct.pupupu.map_gobj[1]->anim_frame = 0.0F;
    grPupupuWhispyUpdateOpen();
    CHECK(gGRCommonStruct.pupupu.whispy_status == kPupupuWindBlow);
    CHECK(gGRCommonStruct.pupupu.flowers_back_status ==
          kPupupuFlowerWindStart);
    CHECK(gGRCommonStruct.pupupu.flowers_front_status ==
          kPupupuFlowerWindStart);
    CHECK(gGRCommonStruct.pupupu.whispy_wind_duration >= 240);
    CHECK(gGRCommonStruct.pupupu.whispy_wind_duration < 240 + 80);
    CHECK(gGRCommonStruct.pupupu.rumble_wait == 0);

    /* Blow's countdown runs the wind rumble on its own tail, arming the
     * wait AFTER the effect so the first quake is on the first tic */
    gGRCommonStruct.pupupu.whispy_wind_duration = 3;
    grPupupuWhispyUpdateBlow();
    CHECK(gGRCommonStruct.pupupu.whispy_wind_duration == 2);
    CHECK(gGRCommonStruct.pupupu.rumble_wait ==
          GRPUPUPU_WHISPY_WIND_RUMBLE_WAIT - 1);
    CHECK(gGRCommonStruct.pupupu.whispy_status == kPupupuWindBlow);

    /* and running out closes the mouth and hands both flower machines to
     * their end state */
    gGRCommonStruct.pupupu.whispy_wind_duration = 1;
    grPupupuWhispyUpdateBlow();
    CHECK(gGRCommonStruct.pupupu.whispy_wind_duration == 0);
    CHECK(gGRCommonStruct.pupupu.whispy_status == kPupupuWindStop);
    CHECK(gGRCommonStruct.pupupu.whispy_mouth_status == kPupupuMouthClose);
    CHECK(gGRCommonStruct.pupupu.flowers_back_status ==
          kPupupuFlowerWindLoopEnd);
    CHECK(gGRCommonStruct.pupupu.flowers_front_status ==
          kPupupuFlowerWindLoopEnd);

    /* Stop re-arms the wait, gated the same way on the mouth's script */
    gGRCommonStruct.pupupu.map_gobj[1]->anim_frame = 1.0F;
    grPupupuWhispyUpdateStop();
    CHECK(gGRCommonStruct.pupupu.whispy_status == kPupupuWindStop);
    gGRCommonStruct.pupupu.map_gobj[1]->anim_frame = 0.0F;
    grPupupuWhispyUpdateStop();
    CHECK(gGRCommonStruct.pupupu.whispy_status == kPupupuWindWait);
    CHECK(gGRCommonStruct.pupupu.whispy_wind_wait >= 960);

    /* ---- the blink, and its two arms ---- */
    gGRCommonStruct.pupupu.whispy_eyes_status = -1;
    /* the mouth's own status is whatever the Open test above left it at,
     * and this section is about what the BLINK does to it, so it is
     * cleared to the idle value first */
    gGRCommonStruct.pupupu.whispy_mouth_status = -1;
    gGRCommonStruct.pupupu.map_gobj[0]->anim_frame = 0.0F;
    gGRCommonStruct.pupupu.whispy_status = kPupupuWindWait;
    gGRCommonStruct.pupupu.map_gobj[1]->anim_frame = 1.0F;  /* mouth busy */
    gGRCommonStruct.pupupu.whispy_blink_wait = 1;
    grPupupuWhispyUpdateBlink();
    CHECK(gGRCommonStruct.pupupu.whispy_eyes_status == kPupupuEyesBlink);
    /* the timer hit exactly 0, so it is NOT re-rolled -- that is what the
     * -10 arm below is for */
    CHECK(gGRCommonStruct.pupupu.whispy_blink_wait == 0);
    /* and the mouth was busy, so it was not stretched */
    CHECK(gGRCommonStruct.pupupu.whispy_mouth_status == -1);

    /* the same, with the mouth idle: the blink ALSO stretches it */
    gGRCommonStruct.pupupu.whispy_eyes_status = -1;
    gGRCommonStruct.pupupu.map_gobj[0]->anim_frame = 0.0F;
    gGRCommonStruct.pupupu.map_gobj[1]->anim_frame = 0.0F;
    gGRCommonStruct.pupupu.whispy_blink_wait = 1;
    grPupupuWhispyUpdateBlink();
    CHECK(gGRCommonStruct.pupupu.whispy_mouth_status ==
          kPupupuMouthStretch);

    /* the -10 arm: a timer parked at 0 reaching -10 blinks again, and
     * this one IS re-rolled */
    gGRCommonStruct.pupupu.whispy_eyes_status = -1;
    gGRCommonStruct.pupupu.map_gobj[0]->anim_frame = 0.0F;
    gGRCommonStruct.pupupu.whispy_blink_wait = -9;
    grPupupuWhispyUpdateBlink();
    CHECK(gGRCommonStruct.pupupu.whispy_eyes_status == kPupupuEyesBlink);
    CHECK(gGRCommonStruct.pupupu.whispy_blink_wait >= 30);

    /* a busy eyes script blocks the whole function, even mid-countdown */
    gGRCommonStruct.pupupu.whispy_eyes_status = kPupupuEyesTurn;
    gGRCommonStruct.pupupu.map_gobj[0]->anim_frame = 0.0F;
    gGRCommonStruct.pupupu.whispy_blink_wait = 1;
    grPupupuWhispyUpdateBlink();
    CHECK(gGRCommonStruct.pupupu.whispy_blink_wait == 1);
    gGRCommonStruct.pupupu.whispy_eyes_status = -1;

    /* ---- the two flower machines, and the texture swaps ---- */

    /* the BACK flowers drive the MOUTH's texture: Open on a timer, then
     * Blow when the loop-start script ends, then Close when the blow
     * ends. WindLoop and WindStop have no arm at all. */
    gGRCommonStruct.pupupu.flowers_back_status = kPupupuFlowerWindStart;
    gGRCommonStruct.pupupu.flowers_back_wait = 1;
    grPupupuFlowersBackUpdateAll();
    CHECK(gGRCommonStruct.pupupu.whispy_mouth_texture == kPupupuMouthTexOpen);
    CHECK(gGRCommonStruct.pupupu.flowers_back_status ==
          kPupupuFlowerWindLoopStart);
    gGRCommonStruct.pupupu.map_gobj[2]->anim_frame = 1.0F;
    grPupupuFlowersBackUpdateAll();
    CHECK(gGRCommonStruct.pupupu.whispy_mouth_texture == kPupupuMouthTexOpen);
    gGRCommonStruct.pupupu.map_gobj[2]->anim_frame = 0.0F;
    grPupupuFlowersBackUpdateAll();
    CHECK(gGRCommonStruct.pupupu.whispy_mouth_texture == kPupupuMouthTexBlow);
    CHECK(gGRCommonStruct.pupupu.flowers_back_status == kPupupuFlowerWindLoop);
    CHECK(gGRCommonStruct.pupupu.flowers_back_wait == 15);

    /* the loop itself does nothing, so the end is what closes the mouth */
    grPupupuFlowersBackUpdateAll();
    CHECK(gGRCommonStruct.pupupu.whispy_mouth_texture == kPupupuMouthTexBlow);
    gGRCommonStruct.pupupu.flowers_back_status = kPupupuFlowerWindLoopEnd;
    gGRCommonStruct.pupupu.flowers_back_wait = 1;
    grPupupuFlowersBackUpdateAll();
    CHECK(gGRCommonStruct.pupupu.whispy_mouth_texture == kPupupuMouthTexClose);
    CHECK(gGRCommonStruct.pupupu.flowers_back_status == kPupupuFlowerWindStop);
    CHECK(gGRCommonStruct.pupupu.flowers_back_wait == 15);

    /* the FRONT flowers drive the EYES' texture, and this is the machine
     * the wind is actually gated on */
    gGRCommonStruct.pupupu.flowers_front_status = kPupupuFlowerWindStart;
    gGRCommonStruct.pupupu.flowers_front_wait = 1;
    grPupupuFlowersFrontUpdateAll();
    CHECK(gGRCommonStruct.pupupu.whispy_eyes_texture == 0);
    CHECK(gGRCommonStruct.pupupu.flowers_front_status ==
          kPupupuFlowerWindLoopStart);
    gGRCommonStruct.pupupu.map_gobj[3]->anim_frame = 0.0F;
    grPupupuFlowersFrontUpdateAll();
    CHECK(gGRCommonStruct.pupupu.whispy_eyes_texture == 1);
    CHECK(gGRCommonStruct.pupupu.flowers_front_status ==
          kPupupuFlowerWindLoop);
    CHECK(gGRCommonStruct.pupupu.flowers_front_wait == 22);

    /* ---- grPupupuWhispySetWindPush and grPupupuWhispyGetLR: the blow
     * itself, and which way it faces (their own function, below) ---- */
    pupupu_check_wind();

    /* ---- teardown ---- */
    for (i = 0; i < 4; i++)
    {
        gcEjectGObj(gGRCommonStruct.pupupu.map_gobj[i]);
    }
    memset(&gGRCommonStruct, 0, sizeof(gGRCommonStruct));
    for (i = 0; i < 4; i++)
    {
        if (sPupupuStage.map_models[i].hd != NULL)
        {
            fighter_release(&sPupupuStage.map_models[i]);
        }
    }
    free(sHostMapBlob);
    sHostMapBlob = NULL;
    mock_battle.game_status = saved_status;
    gStageBound = saved_bound;
}

/* ---- Mushroom Kingdom's scales -------------------------
 *
 * src/dc/grinishie.c is the port's sixth stage hazard and the first whose
 * map object is a DISPLAY LIST rather than a DObjDesc tree --
 * `MPGroundData.map_nodes` names `MapHead`, and grinishie.c:372 gives
 * each platform an empty DObj carrying it. So this test is where the
 * exporter's third object shape and the loader's existing MPK1 reader
 * meet: object 0 is one joint and object 1 is the scale's five-joint
 * tree, and the file's own `map_dobjs[]` reads (the two chain lengths)
 * come out of the second.
 *
 * The mock world has none of what the stage's own code reads --
 * nMPMapObjKindScaleL/R are not in kMapObjs, and the two collision
 * lines the platforms hang on are yakumono ids 1 and 2, which the mock
 * geometry's single zero-id line group does not allocate. The test
 * binds its own geometry for the length of the call and gives the mock's
 * back afterwards.
 */
static Stage sInishieStage;

/* the file's own private enum, the same way test_gr_hyrule names the
 * twister's states. */
enum
{
    kInishieScaleWait = 0,
    kInishieScaleFall = 1,
    kInishieScaleSleep = 2,
    kInishieScaleRetract = 3
};

/* gr/grvars.h's GRCommonGroundVarsInishie says two scale entries, and
 * dGRInishieScaleLineGroups says which yakumono id each hangs on. */
#define INISHIE_LINES 3

static const MPLineInfo sInishieLineInfo[] = {
    /* the mock's own group, unchanged */
    { 0, { { 0, 3 }, { 3, 1 }, { 4, 1 }, { 5, 1 } } },
    /* and one empty group per platform line, so the yakumono array
     * mpCollisionLoadGeometry sizes from the highest id is three long */
    { 1, { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 2, { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
};

static MPMapObjData sInishieMapObjs[2];
static MPGeometryData sInishieGeo;
static MPGeometryData *sInishieSavedGeo;
static void *sInishieSavedMapObjs;
static MPYakumonoDObj *sInishieSavedYakumono;
static Vec3f *sInishieSavedSpeeds;

/* bind a ground the stage's own code can find its two platforms on */
static void inishie_bind_scale_ground(void)
{
    sInishieSavedGeo = gMPCollisionGeometry;
    sInishieSavedMapObjs = gMPCollisionMapObjs;
    sInishieSavedYakumono = gMPCollisionYakumonoDObjs;
    sInishieSavedSpeeds = gMPCollisionSpeeds;

    /* the two platforms' own spawn points, at the heights the test then
     * checks against. Both are in s16 range, which is what MPMapObjData
     * carries. */
    sInishieMapObjs[0].mapobj_kind = nMPMapObjKindScaleL;
    sInishieMapObjs[0].pos.x = -600;
    sInishieMapObjs[0].pos.y = 1000;

    sInishieMapObjs[1].mapobj_kind = nMPMapObjKindScaleR;
    sInishieMapObjs[1].pos.x = 600;
    sInishieMapObjs[1].pos.y = 1000;

    /* everything else is the mock's, so the line and vertex tables this
     * stage does not use still come out the way they always did */
    sInishieGeo = mock_geo;
    sInishieGeo.yakumono_count =
        (u16)(sizeof(sInishieLineInfo) / sizeof(sInishieLineInfo[0]));
    sInishieGeo.line_info = (MPLineInfo *)sInishieLineInfo;
    sInishieGeo.mapobj_count = 2;
    sInishieGeo.mapobjs = (void *)sInishieMapObjs;

    mpCollisionLoadGeometry(&sInishieGeo);
}

static void inishie_restore_scale_ground(void)
{
    gMPCollisionGeometry = sInishieSavedGeo;
    gMPCollisionMapObjs = sInishieSavedMapObjs;
    gMPCollisionYakumonoDObjs = sInishieSavedYakumono;
    gMPCollisionSpeeds = sInishieSavedSpeeds;
}

/* put the one fighter on `side`'s line, weighted and grounded, so
 * grInishieScaleGetPressure counts it. `line_id` is a FLOOR LINE id, and
 * mpCollisionSetDObjNoID reads that line's own yakumono id out of the
 * vertex info -- which is why the mock's deck is line 1 and the two
 * platform lines are the yakumono ids the fixture just sized. */
static void inishie_place_fighter(s32 line_id, s32 grounded)
{
    FTStruct *mfp = ftGetStruct(gGCCommonLinks[nGCCommonLinkIDFighter]);

    if (grounded != 0)
    {
        mfp->ga = nMPKineticsGround;
        mfp->coll_data.floor_line_id = line_id;
    }
    else
    {
        mfp->ga = nMPKineticsAir;
        mfp->coll_data.floor_line_id = -2;
    }
}

static void test_gr_inishie(void)
{
    Stage *saved_bound;
    GObj *fighter_gobj;
    DObj *l_dobj;
    DObj *r_dobj;
    s32 saved_status = mock_battle.game_status;
    s32 i;

    saved_bound = gStageBound;
    gStageBound = &sInishieStage;
    memset(&sInishieStage, 0, sizeof(sInishieStage));
    CHECK(host_load_map_file(&sInishieStage, "inishie.stg") == 0);

    /* the fall fires the stage's own sparkle effect at both platforms.
     * The bank is not loaded -- grInishie.c has no
     * efParticleGetLoadBankID call in the decomp either -- and
     * lbParticleMakeScriptID is what says so by returning NULL. */
    efParticleInitAll();

    /* the exporter's own numbers: TWO objects, and this is the pair that
     * needed the third shape -- object 0 is the platform, one DObj
     * carrying `MapHead`, and object 1 is the scale's own DObjDesc tree
     * of five. Both orders matter: map_nodes names the platform. */
    CHECK(sInishieStage.map_object_count == 2);
    CHECK(sInishieStage.map_models[0].hd != NULL);
    CHECK(sInishieStage.map_models[0].hd->joint_count == 1);
    CHECK(sInishieStage.map_models[1].hd != NULL);
    CHECK(sInishieStage.map_models[1].hd->joint_count == 5);
    /* neither object has a material: the map group lists no MObjSub, and
     * the platform's GObj in the decomp never gets one either */
    CHECK(sInishieStage.map_models[0].mobjs == NULL);
    CHECK(sInishieStage.map_models[1].mobjs == NULL);

    /* the one script the file plays, and -- ledger row 28's own
     * assertion -- that a by-name lookup which RESOLVES is not a script
     * that RUNS. Opcode 0 is 0x00000000, so "not zero" is exactly "not
     * an immediate end". */
    CHECK(stage_map_anim("ScaleRetract") != NULL);
    CHECK(((const u32 *)stage_map_anim("ScaleRetract"))[0] != 0);
    CHECK(stage_map_anim("ScaleNonesuch") == NULL);

    /* ---- grInishieMakeScale ---- */
    inishie_bind_scale_ground();
    /* mpCollisionLoadGeometry allocates the yakumono arrays out of the
     * scene heap, and the one this test binds has THREE slots where the
     * mock world's has one. If the allocator cannot serve them the three
     * pointers come back NULL and the store that fills dobjs[] is a
     * store through NULL, which is a segfault somewhere else entirely --
     * so it is named here instead. */
    CHECK(gMPCollisionYakumonoDObjs != NULL);
    CHECK(gMPCollisionSpeeds != NULL);
    CHECK(gMPCollisionVertexInfo != NULL);
    grInishieMakeScale();

    l_dobj = gGRCommonStruct.inishie.scale[0].platform_dobj;
    r_dobj = gGRCommonStruct.inishie.scale[1].platform_dobj;
    CHECK(l_dobj != NULL);
    CHECK(r_dobj != NULL);
    /* two SEPARATE DObjs, from two binds of the one pack object */
    CHECK(l_dobj != r_dobj);

    /* the platforms sit on their own spawn points and remember the
     * height as their base */
    CHECK(l_dobj->translate.vec.f.x == -600.0F);
    CHECK(l_dobj->translate.vec.f.y == 1000.0F);
    CHECK(r_dobj->translate.vec.f.x == 600.0F);
    CHECK(r_dobj->translate.vec.f.y == 1000.0F);
    CHECK(gGRCommonStruct.inishie.scale[0].platform_base_y == 1000.0F);
    CHECK(gGRCommonStruct.inishie.scale[1].platform_base_y == 1000.0F);

    /* the two chains come off the SCALE TREE's own joints, which is the
     * reason object 1 has to be a real five-joint tree: the lengths are
     * joint 0's y plus joint 3's for the left and joint 0's plus joint
     * 1's for the right, read by index out of the pack. */
    CHECK(gGRCommonStruct.inishie.scale[0].string_dobj !=
          gGRCommonStruct.inishie.scale[1].string_dobj);
    CHECK(gGRCommonStruct.inishie.scale[0].string_length ==
          stage_map_joint(1, 0)->translate.vec.f.y +
          stage_map_joint(1, 3)->translate.vec.f.y);
    CHECK(gGRCommonStruct.inishie.scale[1].string_length ==
          stage_map_joint(1, 0)->translate.vec.f.y +
          stage_map_joint(1, 1)->translate.vec.f.y);
    /* and the lengths are not zero, so the tail's subtraction below is a
     * real move rather than a no-op that would pass whatever it did */
    CHECK(gGRCommonStruct.inishie.scale[0].string_length != 0.0F);
    CHECK(gGRCommonStruct.inishie.scale[1].string_length != 0.0F);

    /* both lines are ON and the machine is level and waiting */
    CHECK(gMPCollisionYakumonoDObjs->dobjs[1]->user_data.s ==
          nMPYakumonoStatusOn);
    CHECK(gMPCollisionYakumonoDObjs->dobjs[2]->user_data.s ==
          nMPYakumonoStatusOn);
    CHECK(gGRCommonStruct.inishie.splat_status == kInishieScaleWait);
    CHECK(gGRCommonStruct.inishie.splat_alt == 0.0F);
    CHECK(gGRCommonStruct.inishie.splat_accelerate == 0.0F);
    for (i = 0; i < 4; i++)
    {
        CHECK(gGRCommonStruct.inishie.players_tt[i] == 0);
        CHECK(gGRCommonStruct.inishie.players_ga[i] == 0);
    }

    /* ---- grInishieScaleGetPressure ---- */
    /* The fighter this section drives comes from the tests before it --
     * the mock world always has one and pupupu_check_wind asserts it --
     * so the precondition is named here rather than assumed. A GObj on
     * the link whose payload is gone is a live object with no FTStruct
     * behind it, which is what every `mfp->` below would deref. */
    fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
    CHECK(fighter_gobj != NULL);
    CHECK(ftGetStruct(fighter_gobj) != NULL);
    /* the two CHECKs above are the whole of what this test needs from
     * the fighter: the rest goes through inishie_place_fighter, which
     * fetches its own. There is no local to keep, and there must not be
     * one named `fp` -- see the note on the macro at the top of this
     * file's `#define fp (*fpp)`. */

    /* nobody grounded anywhere: no pressure on either side */
    inishie_place_fighter(-2, 0);
    CHECK(grInishieScaleGetPressure(1) == 0.0F);
    CHECK(grInishieScaleGetPressure(2) == 0.0F);

    /* the fighter grounded on the DECK (line 1, whose vertex info names
     * yakumono id 0) presses NEITHER of the two scale lines -- which is
     * the check that the line id is resolved through
     * mpCollisionSetDObjNoID rather than compared as itself */
    inishie_place_fighter(1, 1);
    CHECK(grInishieScaleGetPressure(1) == 0.0F);
    CHECK(grInishieScaleGetPressure(2) == 0.0F);

    /* ---- the state machine, one tic at a time ---- */

    /* an empty scale with weight on neither side centres itself: the
     * alt is pushed back to zero eight at a time and the acceleration is
     * cleared outright */
    gGRCommonStruct.inishie.splat_alt = -20.0F;
    gGRCommonStruct.inishie.splat_accelerate = 5.0F;
    grInishieScaleUpdateWait();
    CHECK(gGRCommonStruct.inishie.splat_alt == -12.0F);
    CHECK(gGRCommonStruct.inishie.splat_accelerate == 0.0F);

    /* the platforms follow the alt in OPPOSITE directions off their own
     * bases, and the two chains take up the slack -- the whole point of
     * the stage's furniture */
    CHECK(l_dobj->translate.vec.f.y == 1000.0F - 12.0F);
    CHECK(r_dobj->translate.vec.f.y == 1000.0F + 12.0F);
    CHECK(gGRCommonStruct.inishie.scale[0].string_dobj->translate.vec.f.y ==
          l_dobj->translate.vec.f.y -
          gGRCommonStruct.inishie.scale[0].string_length);
    CHECK(gGRCommonStruct.inishie.scale[1].string_dobj->translate.vec.f.y ==
          r_dobj->translate.vec.f.y -
          gGRCommonStruct.inishie.scale[1].string_length);
    CHECK(gGRCommonStruct.inishie.splat_status == kInishieScaleWait);

    /* and the machine's tail writes the platforms' own positions into
     * the two collision lines EVERY tic -- which is what makes a falling
     * platform a falling floor and not a falling picture */
    grInishieScaleProcUpdate(NULL);
    CHECK(gMPCollisionYakumonoDObjs->dobjs[1]->translate.vec.f.y ==
          l_dobj->translate.vec.f.y);
    CHECK(gMPCollisionYakumonoDObjs->dobjs[2]->translate.vec.f.y ==
          r_dobj->translate.vec.f.y);
    CHECK(gMPCollisionYakumonoDObjs->dobjs[1]->translate.vec.f.x == -600.0F);
    CHECK(gMPCollisionYakumonoDObjs->dobjs[2]->translate.vec.f.x == 600.0F);

    /* past the limit: the pair FALLS, the alt is clamped to the limit
     * with the sign it had, and the acceleration is cleared. The alt is
     * handed over the limit directly rather than driven there by
     * weight: reaching it through the pressure arithmetic is the
     * falloff's business, and the mock world has no line whose yakumono
     * id is either platform's. It has to be far enough over that the
     * centring step's own minus-8 does not pull it back under. */
    gGRCommonStruct.inishie.splat_alt = 1200.0F;
    gGRCommonStruct.inishie.splat_accelerate = 50.0F;
    grInishieScaleUpdateWait();
    CHECK(gGRCommonStruct.inishie.splat_status == kInishieScaleFall);
    CHECK(gGRCommonStruct.inishie.splat_alt == 1100.0F);
    CHECK(gGRCommonStruct.inishie.splat_accelerate == 0.0F);
    /* the platforms moved with it, still in opposite directions */
    CHECK(l_dobj->translate.vec.f.y == 1000.0F + 1100.0F);
    CHECK(r_dobj->translate.vec.f.y == 1000.0F - 1100.0F);

    /* the other sign: the alt is clamped NEGATIVE */
    gGRCommonStruct.inishie.splat_alt = -1200.0F;
    gGRCommonStruct.inishie.splat_status = kInishieScaleWait;
    grInishieScaleUpdateWait();
    CHECK(gGRCommonStruct.inishie.splat_status == kInishieScaleFall);
    CHECK(gGRCommonStruct.inishie.splat_alt == -1100.0F);

    /* the fall: the platforms drop together, accelerating three a tic to
     * a terminal seventy, until BOTH are under the map floor less a
     * thousand. The mock ground's map_bound_bottom is -3000, so the
     * deadzone is -4000. */
    gGRCommonStruct.inishie.splat_alt = 1100.0F;
    gGRCommonStruct.inishie.splat_accelerate = 0.0F;
    /* the two are levelled first, so "both moved by the same amount" is
     * a statement about the fall rather than about where the tilt test
     * above left them */
    l_dobj->translate.vec.f.y = 1000.0F;
    r_dobj->translate.vec.f.y = 1000.0F;
    grInishieScaleUpdateFall();
    CHECK(gGRCommonStruct.inishie.splat_accelerate == 3.0F);
    /* both moved by the SAME amount here -- a falling pair does not tilt,
     * which is the difference between this state and Wait's */
    CHECK(l_dobj->translate.vec.f.y == 997.0F);
    CHECK(r_dobj->translate.vec.f.y == 997.0F);
    /* not yet under the deadzone: still falling */
    CHECK(gGRCommonStruct.inishie.splat_status == kInishieScaleFall);

    /* put both platforms just above it and fall one more tic */
    l_dobj->translate.vec.f.y = -3999.0F;
    r_dobj->translate.vec.f.y = -3999.0F;
    gGRCommonStruct.inishie.splat_accelerate = 0.0F;
    grInishieScaleUpdateFall();
    CHECK(gGRCommonStruct.inishie.splat_status == kInishieScaleSleep);
    CHECK(gGRCommonStruct.inishie.splat_accelerate == 0.0F);
    /* both lines go OFF, which is what stops the platforms being floors */
    CHECK(gMPCollisionYakumonoDObjs->dobjs[1]->user_data.s ==
          nMPYakumonoStatusOff);
    CHECK(gMPCollisionYakumonoDObjs->dobjs[2]->user_data.s ==
          nMPYakumonoStatusOff);
    /* and the sleep is the decomp's own 180 tics */
    CHECK(gGRCommonStruct.inishie.splat_wait == 180);

    /* the sleep counts down, and at zero hands the pair to the retract
     * state. The script itself is host-guarded (see the file's header),
     * so what is checked here is the transition, not the attach. */
    grInishieScaleUpdateStep();
    CHECK(gGRCommonStruct.inishie.splat_wait == 179);
    CHECK(gGRCommonStruct.inishie.splat_status == kInishieScaleSleep);
    gGRCommonStruct.inishie.splat_wait = 1;
    grInishieScaleUpdateStep();
    CHECK(gGRCommonStruct.inishie.splat_wait == 0);
    CHECK(gGRCommonStruct.inishie.splat_status == kInishieScaleRetract);

    /* the rise walks the alt back ten a tic and, when it crosses, stops
     * the platforms' scripts by hand and puts both lines back ON */
    gGRCommonStruct.inishie.splat_alt = 1100.0F;
    gGRCommonStruct.inishie.splat_status = kInishieScaleRetract;
    grInishieScaleUpdateRetract();
    CHECK(gGRCommonStruct.inishie.splat_alt == 1090.0F);
    CHECK(gGRCommonStruct.inishie.splat_status == kInishieScaleRetract);
    CHECK(l_dobj->translate.vec.f.y == 1000.0F + 1090.0F);
    CHECK(r_dobj->translate.vec.f.y == 1000.0F - 1090.0F);
    /* the two chain DObjs follow the platforms here too */
    CHECK(gGRCommonStruct.inishie.scale[0].string_dobj->translate.vec.f.y ==
          l_dobj->translate.vec.f.y -
          gGRCommonStruct.inishie.scale[0].string_length);

    gGRCommonStruct.inishie.splat_alt = 5.0F;
    l_dobj->anim_wait = AOBJ_ANIM_CHANGED;
    r_dobj->anim_wait = AOBJ_ANIM_CHANGED;
    grInishieScaleUpdateRetract();
    CHECK(gGRCommonStruct.inishie.splat_alt == 0.0F);
    CHECK(gGRCommonStruct.inishie.splat_status == kInishieScaleWait);
    CHECK(l_dobj->anim_wait == AOBJ_ANIM_NULL);
    CHECK(r_dobj->anim_wait == AOBJ_ANIM_NULL);
    CHECK(gMPCollisionYakumonoDObjs->dobjs[1]->user_data.s ==
          nMPYakumonoStatusOn);
    CHECK(gMPCollisionYakumonoDObjs->dobjs[2]->user_data.s ==
          nMPYakumonoStatusOn);
    CHECK(l_dobj->translate.vec.f.y == 1000.0F);
    CHECK(r_dobj->translate.vec.f.y == 1000.0F);

    /* ---- grInishieScaleGetPlatformInfo, which the decomp itself marks
     * unused and which is ported for that reason alone ---- */
    {
        f32 alt;
        f32 accel;

        gGRCommonStruct.inishie.splat_alt = -300.0F;
        gGRCommonStruct.inishie.splat_accelerate = -7.0F;
        grInishieScaleGetPlatformInfo(&alt, &accel);
        CHECK(alt == 1100.0F - 300.0F);
        CHECK(accel == 7.0F);
    }

    /* ---- teardown ---- */
    memset(&gGRCommonStruct, 0, sizeof(gGRCommonStruct));
    for (i = 0; i < 2; i++)
    {
        if (sInishieStage.map_models[i].hd != NULL)
        {
            fighter_release(&sInishieStage.map_models[i]);
        }
    }
    free(sHostMapBlob);
    sHostMapBlob = NULL;
    /* ---- the `?` block ---------------------------------
     *
     * NOT grInishieMakePowerBlock itself: its first real act is
     * mpCollisionGetMapObjCountKind(nMPMapObjKindPowerBlock), and a
     * stage whose map names none loops forever printing -- the DECOMP'S
     * own error arm (grinishie.c:520-527), kept verbatim. The mock world
     * has no map objects at all, so this drives the machine's pieces and
     * leaves that call to the target, where the map file is the real one.
     *
     * The `?` block's state is the STAGE's; the item has one status of
     * its own and test_it_powerblock above drives that. */
    {
        GObj pblock, tfighter;
        FTStruct tfp;
        ITStruct tip;
        GRAttackColl *coll = NULL;
        s32 kind = -1;

        memset(&pblock, 0, sizeof(pblock));
        memset(&tfighter, 0, sizeof(tfighter));
        memset(&tfp, 0, sizeof(tfp));
        memset(&tip, 0, sizeof(tip));
        tfighter.user_data.p = &tfp;
        pblock.user_data.p = &tip;

        /* grInishiePowerBlockSetWait: 1800 tics -- thirty seconds -- and
         * the Make state */
        gGRCommonStruct.inishie.pblock_status = kInishiePowerBlockDamage;
        grInishiePowerBlockSetWait();
        CHECK(gGRCommonStruct.inishie.pblock_status == kInishiePowerBlockMake);
        CHECK(gGRCommonStruct.inishie.pblock_appear_wait == 1800);

        /* grInishiePowerBlockUpdateWait, through the machine's own
         * dispatch: nothing at all while the battle is in its pre-match
         * hold, and the first block armed the moment it leaves */
        gGRCommonStruct.inishie.pblock_status = kInishiePowerBlockWait;
        gGRCommonStruct.inishie.pblock_appear_wait = 0;
        /* the mock battle's own status IS Wait -- a match that has not
         * started -- so the arm that does nothing is the one it is
         * already in, and the arm that arms the block needs the other
         * value put there by hand */
        saved_status = mock_battle.game_status;
        CHECK(saved_status == nSCBattleGameStatusWait);
        grInishiePowerBlockProcUpdate(NULL);
        CHECK(gGRCommonStruct.inishie.pblock_status == kInishiePowerBlockWait);
        CHECK(gGRCommonStruct.inishie.pblock_appear_wait == 0);
        mock_battle.game_status = nSCBattleGameStatusGo;
        grInishiePowerBlockProcUpdate(NULL);
        CHECK(gGRCommonStruct.inishie.pblock_status == kInishiePowerBlockMake);
        CHECK(gGRCommonStruct.inishie.pblock_appear_wait == 1800);

        /* grInishiePowerBlockSetDamage: the ITEM calls this when it is
         * hit. The hazard registration is ftMainCheckAddGroundHazard,
         * whose table is ftmain.c's own static -- what is observable
         * here is the two-tick countdown and the state. */
        gGRCommonStruct.inishie.pblock_gobj = &pblock;
        grInishiePowerBlockSetDamage();
        CHECK(gGRCommonStruct.inishie.pblock_status == kInishiePowerBlockDamage);
        CHECK(gGRCommonStruct.inishie.pblock_appear_wait == 2);

        /* grInishiePowerBlockUpdateDamage: two tics later the hazard
         * comes off -- ftMainClearHazard over the same table, which is a
         * no-op for a GObj that was never in it */
        grInishiePowerBlockProcUpdate(NULL);
        CHECK(gGRCommonStruct.inishie.pblock_appear_wait == 1);
        CHECK(gGRCommonStruct.inishie.pblock_status == kInishiePowerBlockDamage);
        grInishiePowerBlockProcUpdate(NULL);
        CHECK(gGRCommonStruct.inishie.pblock_appear_wait == 0);

        /* grInishiePowerBlockCheckGetDamageKind: a GROUNDED fighter that
         * is not the one the block last damaged gets the hit, with the
         * STAGE's own attack descriptor -- the one the port's pack
         * carries as Stage.attack_coll. The consumer of
         * nGMHitEnvironmentPowerBlock has been in ftmain.c
         * with nothing to fire it; this is what fires it. */
        gGRCommonStruct.inishie.attack_coll = sInishieStage.attack_coll;
        CHECK(gGRCommonStruct.inishie.attack_coll != NULL);
        tfp.ga = nMPKineticsGround;
        tip.damage_gobj = NULL;

        CHECK(grInishiePowerBlockCheckGetDamageKind(&pblock, &tfighter,
                                                    &coll, &kind) == TRUE);
        CHECK(coll == gGRCommonStruct.inishie.attack_coll);
        CHECK(kind == nGMHitEnvironmentPowerBlock);

        /* the SAME fighter twice: the block will not hit the one it just
         * hit -- that is what damage_gobj is for */
        tip.damage_gobj = &tfighter;
        coll = NULL;
        kind = -1;
        CHECK(grInishiePowerBlockCheckGetDamageKind(&pblock, &tfighter,
                                                    &coll, &kind) == FALSE);
        CHECK(coll == NULL);
        CHECK(kind == -1);

        /* and a fighter who is NOT grounded is not touched at all */
        tip.damage_gobj = NULL;
        tfp.ga = nMPKineticsAir;
        CHECK(grInishiePowerBlockCheckGetDamageKind(&pblock, &tfighter,
                                                    &coll, &kind) == FALSE);
    }

    /* ---- the Piranha Plants' stage side ----------------
     *
     * NOT grInishieMakePakkun itself: it makes two real items through
     * itManagerMakeItemSetupCommon at two map object positions, and the
     * mock world has no map objects. What is here is
     * grInishiePakkunSetWaitFighter, which walks the stage's own
     * pakkun_gobj array and raises the flag each plant's state machine
     * reads next tic -- the pipe-entry path, whose CALLER is
     * ft/ftcommon/ftcommondokan.c. */
    {
        GObj plants[2];
        ITStruct pits[2];
        s32 n;

        memset(plants, 0, sizeof(plants));
        memset(pits, 0, sizeof(pits));

        for (n = 0; n < 2; n++)
        {
            plants[n].user_data.p = &pits[n];
            gGRCommonStruct.inishie.pakkun_gobj[n] = &plants[n];
        }
        grInishiePakkunSetWaitFighter();
        CHECK(pits[0].item_vars.pakkun.is_wait_fighter == TRUE);
        CHECK(pits[1].item_vars.pakkun.is_wait_fighter == TRUE);

        /* and a plant that failed to make is a NULL slot, which the
         * function passes straight through */
        gGRCommonStruct.inishie.pakkun_gobj[1] = NULL;
        pits[0].item_vars.pakkun.is_wait_fighter = FALSE;
        grInishiePakkunSetWaitFighter();
        CHECK(pits[0].item_vars.pakkun.is_wait_fighter == TRUE);
        CHECK(gGRCommonStruct.inishie.pakkun_gobj[1] == NULL);
    }

    /* the stage's own three GObojs -- the scale tree and the two
     * platforms. grInishieMakeScale keeps no handles to any of them
     * and neither does the decomp: they are found on the ground
     * link, and every earlier stage test ejects its own, so the list
     * is this stage's alone here. Leaving them alive is not inert --
     * the next test walks the object system and finds display procs
     * whose pack has just been released. */
    while (gGCCommonLinks[nGCCommonLinkIDGround] != NULL)
    {
        gcEjectGObj(gGCCommonLinks[nGCCommonLinkIDGround]);
    }
    inishie_restore_scale_ground();
    mock_battle.game_status = saved_status;
    gStageBound = saved_bound;
}

/* ---- Peach's Castle -------------------------------------
 *
 * The one stage of the nine whose `map_nodes` is not a tree. It is
 * `dStageCastleFile3_AnimJointRoot` -- an `AObjEvent32 *` table with a
 * single entry -- and the game feeds the TABLE to gcAddAnimJointAll on a
 * ground GObj whose tree is one empty DObj. So there is nothing to bake
 * and nothing to name but the script, and this test is where the port's
 * whole understanding of that shape is pinned: zero objects, one anim,
 * and the words the anim actually holds.
 */
static Stage sYamabukiStage;
static Stage sSectorStage;

/* ---- Saffron City's Gate -------------------------------
 *
 * The eighth stage hazard, and the first whose ground GObj is built from
 * a MAP OBJECT rather than by hand: the building is object 0 of the
 * pack, its door is two per-joint AnimJoint tables, and the whole
 * machine is three states.
 *
 * The world it needs is a FOUR-slot one: the gate's own collision line
 * is yakumono 3 (`grYamabukiInitGroundVars` turns it on, and
 * grYamabukiGateSetPositionNear/Far and GateUpdateOpen all read
 * gMPCollisionYakumonoDObjs->dobjs[3]), and the mock world has one. So
 * this binds a geometry of its own, the same shape
 * inishie_bind_scale_ground uses.
 */
static const MPLineInfo sYamabukiLineInfo[] = {
    /* the mock's own group, unchanged */
    { 0, { { 0, 3 }, { 3, 1 }, { 4, 1 }, { 5, 1 } } },
    /* one empty group per gate line, so the yakumono array is four long */
    { 1, { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 2, { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
    { 3, { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
};

static MPMapObjData sYamabukiMapObjs[1];
static MPGeometryData sYamabukiGeo;
static MPGeometryData *sYamabukiSavedGeo;
static MPMapObjContainer *sYamabukiSavedMapObjs;
static MPYakumonoDObj *sYamabukiSavedYakumono;
static Vec3f *sYamabukiSavedSpeeds;

static void yamabuki_bind_gate_ground(void)
{
    sYamabukiSavedGeo = gMPCollisionGeometry;
    sYamabukiSavedMapObjs = gMPCollisionMapObjs;
    sYamabukiSavedYakumono = gMPCollisionYakumonoDObjs;
    sYamabukiSavedSpeeds = gMPCollisionSpeeds;

    /* the gate's own spawn point: the monster comes out of
     * nMPMapObjKindMonster, and the roofline is the yakumono line */
    sYamabukiMapObjs[0].mapobj_kind = nMPMapObjKindMonster;
    sYamabukiMapObjs[0].pos.x = 1280;
    sYamabukiMapObjs[0].pos.y = 1500;

    sYamabukiGeo = mock_geo;
    sYamabukiGeo.yakumono_count =
        (u16)(sizeof(sYamabukiLineInfo) / sizeof(sYamabukiLineInfo[0]));
    sYamabukiGeo.line_info = (MPLineInfo *)sYamabukiLineInfo;
    sYamabukiGeo.mapobj_count = 1;
    sYamabukiGeo.mapobjs = (void *)sYamabukiMapObjs;

    mpCollisionLoadGeometry(&sYamabukiGeo);
}

static void yamabuki_restore_gate_ground(void)
{
    gMPCollisionGeometry = sYamabukiSavedGeo;
    gMPCollisionMapObjs = sYamabukiSavedMapObjs;
    gMPCollisionYakumonoDObjs = sYamabukiSavedYakumono;
    gMPCollisionSpeeds = sYamabukiSavedSpeeds;
}

extern u16 dGRYamabukiMonsterMapObjKinds[5];
extern s32 dGRYamabukiMonsterAttackKind;

static void test_gr_yamabuki(void)
{
    Stage *saved_bound;
    s32 saved_status = mock_battle.game_status;

    saved_bound = gStageBound;
    gStageBound = &sYamabukiStage;
    memset(&sYamabukiStage, 0, sizeof(sYamabukiStage));
    CHECK(host_load_map_file(&sYamabukiStage, "yamabuki.stg") == 0);

    /* ONE object: the building. Five joints, and the pack carries its
     * DL-links as batches like any other map object. */
    CHECK(sYamabukiStage.map_object_count == 1);
    CHECK(sYamabukiStage.map_models[0].hd != NULL);
    CHECK(sYamabukiStage.map_models[0].hd->joint_count == 5);

    /* TEN scripts: the Gate's two tables, four records each (one per
     * joint that carries a script), and the two Gate monsters' appear
     * scripts -- which the map group names nowhere, so the stage entry
     * gives each an offset AND THE FILE it is in (the `(kind, name,
     * (file, offset))` form of stage_anims). Chansey's and Porygon's are
     * both 159's, the monsters' model file, NOT 160's -- which is where
     * the Gate's tree comes from and where a bare offset used to be read,
     * straight out of a display list. */
    CHECK(sHostMapAnimCount == 13);
    CHECK(stage_map_anim("GLuckyAppear") != NULL);
    CHECK(stage_map_anim("PorygonAppear") != NULL);
    CHECK(stage_map_anim("MarumineAppear") != NULL);
    CHECK(stage_map_anim("HitokageAppear") != NULL);
    CHECK(stage_map_anim("FushigibanaAppear") != NULL);
    CHECK(stage_map_anim("GateOpen") != NULL);
    CHECK(stage_map_anim("GateClose") != NULL);
    CHECK(stage_map_anim("GateNonesuch") == NULL);
    /* a by-name lookup that RESOLVES is not a script that RUNS */
    CHECK(((const u32 *)stage_map_anim("GateOpen"))[0] != 0);
    CHECK(((const u32 *)stage_map_anim("GLuckyAppear"))[0] != 0);
    CHECK(((const u32 *)stage_map_anim("PorygonAppear"))[0] != 0);
    /* ... AND a non-zero first word is not proof of the right FILE, which
     * is what this pins. Both monsters' scripts open with an AObjEvent32
     * `SetAnim`, and these two words are the ROM's own at 159:0x03F8 and
     * 159:0x0F38. The bytes at 160:0x03F8 -- where a bare offset used to
     * be read -- are a DISPLAY LIST, and its first word is 0x05000204,
     * which is non-zero too. */
    CHECK(((const u32 *)stage_map_anim("GLuckyAppear"))[0] == 0x08080000);
    CHECK(((const u32 *)stage_map_anim("PorygonAppear"))[0] == 0x08180000);
    /* ... and Marumine's is a SetValRate, not a SetAnim -- a different
     * opcode in the same position, which is exactly why the word is
     * pinned rather than assumed: 159:0x0828 is this script, and 159's
     * own neighbors are the only other words it could plausibly be. */
    CHECK(((const u32 *)stage_map_anim("MarumineAppear"))[0] == 0x0C180000);
    /* Hitokage's opens with the same opcode Marumine's does, so the two
     * scripts are told apart by their ARGUMENT: 0x0828's first float is
     * -20.37 and 0x1A28's is -88.34 (C2B0AF56). */
    CHECK(((const u32 *)stage_map_anim("HitokageAppear"))[0] == 0x0C180000);
    CHECK(((const u32 *)stage_map_anim("HitokageAppear"))[2] == 0xC2B0AF56);
    /* ... and the joint each is on. Both are entry 1 of a two-slot
     * `anim_joints` array whose entry 0 is NULL (`mobjlink_0x03F0` and
     * `mobjlink_0x0F30` of reloc file 159), so a script landed on joint 0
     * would be a wrong animation rather than none. */
    CHECK(stage_map_anim_joint("GLuckyAppear") == 1);
    CHECK(stage_map_anim_joint("PorygonAppear") == 1);
    CHECK(stage_map_anim_joint("MarumineAppear") == 1);
    CHECK(stage_map_anim_joint("HitokageAppear") == 1);
    CHECK(stage_map_anim_joint("FushigibanaAppear") == 1);

    /* and the per-joint array grYamabukiGateAddAnimOffset hands to
     * gcAddAnimJointAll: the scripts land on the joints the map file
     * puts them on, and nowhere else */
    {
        AObjEvent32 *arr[STAGE_MAP_JOINTS_MAX];
        int j;

        stage_map_anim_array("GateOpen", arr);
        for (j = 0; j < STAGE_MAP_JOINTS_MAX; j++)
        {
            if (arr[j] != NULL)
            {
                CHECK(stage_map_anim_joint("GateOpen") >= 0);
                break;
            }
        }
        CHECK(j < STAGE_MAP_JOINTS_MAX);   /* at least one joint carries one */
        stage_map_anim_array("GLuckyAppear", arr);
        CHECK(arr[1] == stage_map_anim("GLuckyAppear"));
        CHECK(arr[0] == NULL);
    }

    /* the world the gate's own arithmetic needs */
    yamabuki_bind_gate_ground();
    CHECK(gMPCollisionYakumonoDObjs != NULL);
    CHECK(gMPCollisionYakumonoDObjs->dobjs[3] != NULL);
    gMPCollisionYakumonoDObjs->dobjs[3]->translate.vec.f.y = 1200.0F;

    /* ---- the three states ----------------------------------------- */

    /* Sleep: nothing at all until the match leaves its pre-match hold,
     * and then Wait with a 1000-to-2000-tick monster timer */
    gGRCommonStruct.yamabuki.gate_status = kYamabukiGateSleep;
    gGRCommonStruct.yamabuki.monster_wait = 0;
    mock_battle.game_status = nSCBattleGameStatusWait;
    grYamabukiGateProcUpdate(NULL);
    CHECK(gGRCommonStruct.yamabuki.gate_status == kYamabukiGateSleep);
    CHECK(gGRCommonStruct.yamabuki.monster_wait == 0);

    mock_battle.game_status = nSCBattleGameStatusGo;
    grYamabukiGateProcUpdate(NULL);
    CHECK(gGRCommonStruct.yamabuki.gate_status == kYamabukiGateWait);
    CHECK(gGRCommonStruct.yamabuki.monster_wait >= 1000);
    CHECK(gGRCommonStruct.yamabuki.monster_wait <= 2000);

    /* the door's two ends, and BOTH read the roofline's own y -- only x
     * is the gate's */
    grYamabukiGateSetPositionNear();
    CHECK_NEAR(gGRCommonStruct.yamabuki.gate_pos.x, 960.0F, 1e-4F);
    CHECK_NEAR(gGRCommonStruct.yamabuki.gate_pos.y, 1200.0F, 1e-4F);
    grYamabukiGateSetPositionFar();
    CHECK_NEAR(gGRCommonStruct.yamabuki.gate_pos.x, 1600.0F, 1e-4F);
    CHECK_NEAR(gGRCommonStruct.yamabuki.gate_pos.y, 1200.0F, 1e-4F);

    /* grYamabukiGateUpdateYakumonoPos: THE GATE IS A FLOOR. The line at
     * yakumono 3 is written with the gate's own position, which is what
     * makes the roof standable and the closing door a moving one. */
    gGRCommonStruct.yamabuki.gate_pos.x = 1234.0F;
    grYamabukiGateUpdateYakumonoPos();
    CHECK_NEAR(gMPCollisionYakumonoDObjs->dobjs[3]->translate.vec.f.x,
               1234.0F, 1e-4F);
    CHECK_NEAR(gMPCollisionYakumonoDObjs->dobjs[3]->translate.vec.f.y,
               1200.0F, 1e-4F);

    /* grYamabukiGateCheckPlayersNear: a grounded fighter standing on
     * nMPMaterialDetect is what opens it. The material is the stage's own
     * geometry, so the test says so with the flags byte. */
    {
        GObj tfighter;
        FTStruct tfp;
        GObj *saved_link = gGCCommonLinks[nGCCommonLinkIDFighter];

        memset(&tfighter, 0, sizeof(tfighter));
        memset(&tfp, 0, sizeof(tfp));
        tfighter.user_data.p = &tfp;
        gGCCommonLinks[nGCCommonLinkIDFighter] = &tfighter;

        tfp.ga = nMPKineticsGround;
        tfp.coll_data.floor_flags = nMPMaterialDetect;
        CHECK(grYamabukiGateCheckPlayersNear() == TRUE);

        /* the right material in the AIR is nobody standing anywhere */
        tfp.ga = nMPKineticsAir;
        CHECK(grYamabukiGateCheckPlayersNear() == FALSE);

        /* and so is the wrong material on the ground */
        tfp.ga = nMPKineticsGround;
        tfp.coll_data.floor_flags = (nMPMaterialDetect + 1) & MAP_VERTEX_MAT_MASK;
        CHECK(grYamabukiGateCheckPlayersNear() == FALSE);

        gGCCommonLinks[nGCCommonLinkIDFighter] = saved_link;
    }

    /* grYamabukiGateSetClosedWait: closed, and both countdowns re-armed */
    gGRCommonStruct.yamabuki.gate_status = kYamabukiGateOpen;
    gGRCommonStruct.yamabuki.gate_wait = 0;
    grYamabukiGateSetClosedWait();
    CHECK(gGRCommonStruct.yamabuki.gate_status == kYamabukiGateWait);
    CHECK(gGRCommonStruct.yamabuki.gate_wait == 1000);
    CHECK(gGRCommonStruct.yamabuki.monster_wait >= 1000);
    CHECK_NEAR(gGRCommonStruct.yamabuki.gate_pos.x, 960.0F, 1e-4F);

    /* grYamabukiGateUpdateOpen: with no monster the gate closes; with
     * one, the door's edge rides against it -- the monster's own x minus
     * its map collision WIDTH -- and clamps at the near end, which is
     * where it stops tracking altogether */
    gGRCommonStruct.yamabuki.monster_gobj = NULL;
    grYamabukiGateUpdateOpen();
    CHECK(gGRCommonStruct.yamabuki.gate_status == kYamabukiGateWait);

    {
        GObj monster;
        ITStruct mip;
        DObj mdobj;

        memset(&monster, 0, sizeof(monster));
        memset(&mip, 0, sizeof(mip));
        memset(&mdobj, 0, sizeof(mdobj));
        monster.user_data.p = &mip;
        monster.obj = &mdobj;
        mip.coll_data.map_coll.width = 200.0F;

        gGRCommonStruct.yamabuki.gate_status = kYamabukiGateOpen;
        gGRCommonStruct.yamabuki.gate_noentry = FALSE;
        gGRCommonStruct.yamabuki.monster_gobj = &monster;

        /* out in the middle: the door follows */
        mdobj.translate.vec.f.x = 1400.0F;
        grYamabukiGateUpdateOpen();
        CHECK_NEAR(gGRCommonStruct.yamabuki.gate_pos.x, 1200.0F, 1e-3F);
        CHECK(gGRCommonStruct.yamabuki.gate_noentry == FALSE);

        /* past the far end: clamped, but still tracking */
        mdobj.translate.vec.f.x = 3000.0F;
        grYamabukiGateUpdateOpen();
        CHECK_NEAR(gGRCommonStruct.yamabuki.gate_pos.x, 1600.0F, 1e-3F);
        CHECK(gGRCommonStruct.yamabuki.gate_noentry == FALSE);

        /* and past the NEAR end: clamped once and then never tracked
         * again -- the flag is what stops the door grinding into the
         * building */
        mdobj.translate.vec.f.x = 500.0F;
        grYamabukiGateUpdateOpen();
        CHECK_NEAR(gGRCommonStruct.yamabuki.gate_pos.x, 960.0F, 1e-3F);
        CHECK(gGRCommonStruct.yamabuki.gate_noentry == TRUE);

        mdobj.translate.vec.f.x = 1400.0F;
        grYamabukiGateUpdateOpen();
        CHECK_NEAR(gGRCommonStruct.yamabuki.gate_pos.x, 960.0F, 1e-3F);

        gGRCommonStruct.yamabuki.monster_gobj = NULL;
    }

    /* grYamabukiGateClearMonsterGObj: the ITEM's own way of saying it is
     * gone (test_it_glucky drives the damage proc that calls it) */
    gGRCommonStruct.yamabuki.monster_gobj = (GObj *)&sYamabukiStage;
    grYamabukiGateClearMonsterGObj();
    CHECK(gGRCommonStruct.yamabuki.monster_gobj == NULL);

    /* the file's own initialized data */
    CHECK(ARRAY_COUNT(dGRYamabukiMonsterMapObjKinds) == 5);
    CHECK(dGRYamabukiMonsterMapObjKinds[0] == nMPMapObjKindMonster);
    CHECK(dGRYamabukiMonsterAttackKind == GRYAMABUKI_MONSTER_WEAPON_MAX);

    /* NOT driven: grYamabukiGateMakeMonster and
     * grYamabukiGateUpdateWait's two paths into it. Both make a REAL
     * item through itManagerMakeItemSetupCommon, and the kind is drawn
     * at random from five -- four of which are not ported yet. The
     * monster kind's own item, Chansey, is driven in test_it_glucky. */

    free(sHostMapBlob);
    sHostMapBlob = NULL;
    yamabuki_restore_gate_ground();
    mock_battle.game_status = saved_status;
    gStageBound = saved_bound;
}

/* ---- Sector Z's pack ------------------------------------
 *
 * The stage with NO map object, and the first whose scripts point at
 * something that is not a script. Its Arwing is Fox's Special3's tree,
 * built by grSectorInitAll through the force-status buffer rather than out
 * of `map_nodes`, so the pack carries no object at all -- just the 39
 * scripts (the eight flight patterns' thirty, five pilots, two out of
 * Fox's file and two more the decomp files under Fox's name but reads out
 * of this stage's) and the fourteen `SYInterpDesc`s they `SetInterp` to
 * (each with its three float arrays).
 *
 * WHAT THIS CAN AND CANNOT SEE. The scripts and the descriptors are the
 * host loader's own business, so it can check that they all arrived, that
 * the names are unique, and -- the part that matters -- that a
 * DESCRIPTOR'S POINTER WORDS WERE WRITTEN by the fixup pass, which is the
 * whole mechanism. What it cannot check is that the
 * runtime reads them as Catrom splines: that needs the AObj interpreter,
 * which the host has no 4-byte `AObjEvent32` for (every script attach is
 * `#ifdef FT_HOSTTEST`-guarded for the same reason). The interpreter is
 * the decomp's own `interp.o`, unmodified, so what is at risk is the
 * POINTER, not the maths.
 */
/* The sizes are the decomp's own (grsector.c:47-137) -- the file declares
 * them with an empty bound, so the count is a check the test has to state, not one
 * it can inherit. And `grSectorArwingDecideZNear` is declared BY HAND
 * because the decomp's grsector.h does not list it: grsector.c:413 defines
 * it, the header simply omits it, and an implicit `int` declaration of a
 * void function is the trap this file's other by-hand externs exist for. */
extern u8 dGRSectorArwingLaserCounts[8];
extern s16 dGRSectorArwingMapPositionsX[3];
extern u8 dGRSectorArwingPilotIDs[56];
extern u8 dGRSectorArwingPilotWaitTimers[6][2];
extern void grSectorArwingDecideZNear(void);

static void test_gr_sector(void)
{
    Stage *saved_bound = gStageBound;

    gStageBound = &sSectorStage;
    memset(&sSectorStage, 0, sizeof(sSectorStage));
    CHECK(host_load_map_file(&sSectorStage, "sector.stg") == 0);

    /* no object: the Arwing is not this stage's */
    CHECK(sSectorStage.map_object_count == 0);

    /* 30 flight-pattern scripts + 5 pilot animations + 2 out of Fox's
     * Special3 + 2 more the decomp names for Fox but reads here
     * + 14 descriptors x (3 arrays + itself) */
    CHECK(sHostMapAnimCount == 95);

    /* the patterns, by the names the stage table gives them */
    CHECK(stage_map_anim("Arwing0Path") != NULL);
    CHECK(stage_map_anim("Arwing0Alt7") != NULL);
    CHECK(stage_map_anim("Arwing0Alt11") != NULL);
    CHECK(stage_map_anim("Arwing7Path") != NULL);
    CHECK(stage_map_anim("Arwing7Alt11") != NULL);
    /* Arwing1 and Arwing4 have no 0x2C script -- the name must NOT resolve,
     * which is what says the two NULL fields were dropped rather than
     * exported as something */
    CHECK(stage_map_anim("Arwing1Alt11") == NULL);
    CHECK(stage_map_anim("Arwing4Alt11") == NULL);

    /* a pattern script's FIRST WORD is a SetInterp -- 0x1A040000, opcode
     * 0x0D -- and word 1 is the descriptor it names. That word is the one
     * the fixup pass fills, so it must be non-zero after the load, and it
     * must NOT be the ROM's own value (which was an offset). */
    {
        const u32 *w = (const u32 *)stage_map_anim("Arwing0Path");

        CHECK(w != NULL);
        CHECK(w[0] == 0x1A040000);
        CHECK((w[0] >> 25 & 0x7F) == 0x0D);
        CHECK(w[1] != 0);
    }

    /* ... and the descriptor it names is in the pack, with its own three
     * pointers WRITTEN. Interp153_1004 is one of them: kind 3 (Catrom) and
     * 8 points, which is the ROM's own first word (0x03000008), so the
     * record is the descriptor's bytes and not another script's. */
    {
        const u32 *d = (const u32 *)stage_map_anim("Interp153_1004");

        CHECK(d != NULL);
        CHECK(d[0] == 0x03000008);
        CHECK(d[2] != 0);       /* +0x08 points    -- the fixup's own word */
        CHECK(d[3] != 0);       /* +0x0C length    -- a float, never a fixup */
        CHECK(d[4] != 0);       /* +0x10 keyframes */
        CHECK(d[5] != 0);       /* +0x14 quartics  */

        /* the three arrays are entries of their own, and the descriptor
         * points AT them: their first words are the ROM's floats, which is
         * how a reader tells a control-point array from a script */
        CHECK(stage_map_anim("Interp153_1004Points") != NULL);
        CHECK(stage_map_anim("Interp153_1004Keyframes") != NULL);
        CHECK(stage_map_anim("Interp153_1004Quartics") != NULL);
    }

    /* ... and the five PILOT animations, which are the other table --
     * `dGRSectorArwingAnimJoints`, indexed by the pilot the Arwing is
     * flying. Pilot 0 means "no pilot" and its table entry is the map head
     * itself, so there is no `ArwingPilot0` to resolve. */
    CHECK(stage_map_anim("ArwingPilot1") != NULL);
    CHECK(stage_map_anim("ArwingPilot2") != NULL);
    CHECK(stage_map_anim("ArwingPilot3") != NULL);
    CHECK(stage_map_anim("ArwingPilot4") != NULL);
    CHECK(stage_map_anim("ArwingPilot5") != NULL);
    CHECK(stage_map_anim("ArwingPilot0") == NULL);

    /* ... and the two scripts out of FOX's Special3 that the Arwing's own
     * transitions play, which are in a THIRD file (161) -- the same
     * `(file, offset)` form makes that no different from the other two. */
    CHECK(stage_map_anim("FoxSpecial3_2E74") != NULL);
    CHECK(stage_map_anim("FoxSpecial3_2EB4") != NULL);

    /* ... and the two the decomp FILES under Fox's name but reads out of
     * this stage's own file (grsector.c's divergence 3): the Arwing's
     * weapon parts come out of them, and the tree gives joints 2, 3, 4
     * and 5 a scale of 1e-5, so without them the guns and wings never
     * appear. Their FIRST WORDS are what says these are the right scripts
     * and not two others of a similar length: the gun one opens with
     * SetValAfterBlock (0x15C00000, opcode 0x0A) and the wing one with
     * SetVal0RateBlock (0x11C00000, opcode 0x08) -- file 153's own bytes
     * at 0x1B34 and 0x1B84, which is NOT where the symbol names put them
     * (file 161 at 0x1B34 is a display list). Their first three payload
     * words are the scales the animation starts from. */
    {
        const u32 *gun = (const u32 *)stage_map_anim("ArwingGunScale");
        const u32 *wing = (const u32 *)stage_map_anim("ArwingWingScale");

        CHECK(gun != NULL);
        CHECK(wing != NULL);
        CHECK(gun[0] == 0x15C00000);
        CHECK((gun[0] >> 25 & 0x7F) == 0x0A);
        CHECK(gun[1] == 0x3F800000);    /* 1.0F -- the part at full size */
        CHECK(wing[0] == 0x11C00000);
        CHECK((wing[0] >> 25 & 0x7F) == 0x08);
        CHECK(wing[1] == 0x3727C5AC);   /* 1e-5F -- the tree's own scale */
    }

    /* and every descriptor the export found is there -- fourteen of them,
     * which is the nine a script names directly plus the five named from
     * inside another descriptor's arrays */
    CHECK(stage_map_anim("Interp153_00E8") != NULL);
    CHECK(stage_map_anim("Interp153_1AF4") != NULL);
    CHECK(stage_map_anim("Interp153_14C8") != NULL);

    /* ---- and the ARWING'S OWN MATHS, which is the half of this file a
     * pack test cannot reach. Everything below is driven with two DObjs
     * standing in for the joints the stage builds: joint 0 is the flight
     * PATH (the aircraft's position), joint 1 the pilot's (whose z
     * rotation is the airframe's own yaw). */
    {
        DObj path, pilot;
        Vec3f rot, dir, v3, v4;
        DObj *saved0 = gGRCommonStruct.sector.map_dobjs[0];
        DObj *saved1 = gGRCommonStruct.sector.map_dobjs[1];

        memset(&path, 0, sizeof(path));
        memset(&pilot, 0, sizeof(pilot));
        gGRCommonStruct.sector.map_dobjs[0] = &path;
        gGRCommonStruct.sector.map_dobjs[1] = &pilot;

        /* grSectorArwingDecideZNear: whether the Arwing is close enough to
         * the deck for its collision line to be live. The threshold is 200
         * units of z, and it is STRICTLY inside it -- -200 is not near. */
        path.translate.vec.f.z = 199.0F;
        grSectorArwingDecideZNear();
        CHECK(gGRCommonStruct.sector.is_arwing_z_near == TRUE);

        path.translate.vec.f.z = -200.0F;
        grSectorArwingDecideZNear();
        CHECK(gGRCommonStruct.sector.is_arwing_z_near == FALSE);

        /* the per-pilot hooks, which take the line away and give it back
         * at particular tics of THAT pilot's flight: pilot 1 at 0 and 88,
         * pilot 4 at 0 and 178. Every other tic leaves it alone -- which
         * is what the 5 and the 177 below are for. */
        gGRCommonStruct.sector.is_arwing_line_active = TRUE;
        gGRCommonStruct.sector.arwing_state_timer = 5;
        func_ovl2_80106C88();
        CHECK(gGRCommonStruct.sector.is_arwing_line_active == TRUE);

        gGRCommonStruct.sector.arwing_state_timer = 0;
        func_ovl2_80106C88();
        CHECK(gGRCommonStruct.sector.is_arwing_line_active == FALSE);

        gGRCommonStruct.sector.arwing_state_timer = 88;
        func_ovl2_80106C88();
        CHECK(gGRCommonStruct.sector.is_arwing_line_active == TRUE);

        gGRCommonStruct.sector.arwing_state_timer = 177;
        func_ovl2_80106CC4();
        CHECK(gGRCommonStruct.sector.is_arwing_line_active == TRUE);

        gGRCommonStruct.sector.arwing_state_timer = 178;
        func_ovl2_80106CC4();
        CHECK(gGRCommonStruct.sector.is_arwing_line_active == TRUE);

        gGRCommonStruct.sector.arwing_state_timer = 0;
        func_ovl2_80106CC4();
        CHECK(gGRCommonStruct.sector.is_arwing_line_active == FALSE);

        /* func_ovl2_8010719C: a velocity into the Euler angles a laser is
         * DRAWN with. The Arwing is flat -- joint 1's z rotation 0, so the
         * perpendicular it crosses against is 90 degrees round, (0,1,0) --
         * the velocity is straight along -x, which is the wing lasers' own
         * velocity, and the answer is a yaw of -90 degrees with no pitch
         * and no roll. That is the laser pointing the way it travels,
         * which is the whole of what this function is for. */
        pilot.rotate.vec.f.z = 0.0F;

        v3.x = -1.0F;
        v3.y = 0.0F;
        v3.z = 0.0F;

        func_ovl2_8010719C(&v3, &rot);
        CHECK_NEAR(rot.y, F_CST_DTOR32(-90.0F), 1e-5F);
        CHECK_NEAR(rot.x, 0.0F, 1e-5F);
        CHECK_NEAR(rot.z, 0.0F, 1e-5F);

        /* ... and func_ovl2_801070A4's three arms, one of which the test
         * above just reached through: the two DEGENERATE ones -- a
         * direction lying on the z axis, where atan2(y, x) has nothing to
         * work from and the 90-degree yaw stands in instead -- and the
         * general one. */
        v3.x = 0.0F;
        v3.y = 0.0F;
        v3.z = -1.0F;
        v4.x = 1.0F;
        v4.y = 1.0F;
        v4.z = 0.0F;
        dir.x = dir.y = dir.z = 0.0F;
        rot.x = rot.y = rot.z = 1234.0F;

        func_ovl2_801070A4(&rot, &dir, &v3, &v4);
        CHECK_NEAR(rot.y, F_CST_DTOR32(90.0F), 1e-5F);
        CHECK_NEAR(rot.x, syUtilsArcTan2(1.0F, 1.0F), 1e-5F);
        CHECK(rot.z == 0.0F);

        v3.z = 1.0F;
        func_ovl2_801070A4(&rot, &dir, &v3, &v4);
        CHECK_NEAR(rot.y, F_CST_DTOR32(-90.0F), 1e-5F);
        CHECK_NEAR(rot.x, syUtilsArcTan2(-1.0F, 1.0F), 1e-5F);

        v3.x = 1.0F;
        v3.y = 0.0F;
        v3.z = 0.5F;
        v4.z = 0.25F;
        dir.z = 0.5F;

        func_ovl2_801070A4(&rot, &dir, &v3, &v4);
        CHECK_NEAR(rot.y, syUtilsArcSin(-0.5F), 1e-5F);
        CHECK_NEAR(rot.x, syUtilsArcTan2(0.25F, 0.5F), 1e-5F);
        CHECK_NEAR(rot.z, syUtilsArcTan2(0.0F, 1.0F), 1e-5F);

        gGRCommonStruct.sector.map_dobjs[0] = saved0;
        gGRCommonStruct.sector.map_dobjs[1] = saved1;
    }

    /* ... and the file's own initialized tables, which are the DRAW: the
     * laser count per flight pattern (2 is the fixed wing pair, 4 two
     * aimed bolts, 0 a pattern that never shoots), the three x positions
     * an Arwing can be aimed at, and the pilot table -- 56 ids in six
     * rows, each row a (start, span) pair the NEXT pilot is drawn from,
     * and each row starting where the one before it ended. */
    CHECK(ARRAY_COUNT(dGRSectorArwingLaserCounts) == 8);
    CHECK(dGRSectorArwingLaserCounts[0] == 2);
    CHECK(dGRSectorArwingLaserCounts[4] == 2);
    CHECK(dGRSectorArwingLaserCounts[5] == 0);
    CHECK(ARRAY_COUNT(dGRSectorArwingMapPositionsX) == 3);
    CHECK(dGRSectorArwingMapPositionsX[0] == -3000);
    CHECK(dGRSectorArwingMapPositionsX[1] == 0);
    CHECK(dGRSectorArwingMapPositionsX[2] == 9000);
    CHECK(ARRAY_COUNT(dGRSectorArwingPilotIDs) == 56);
    CHECK(ARRAY_COUNT(dGRSectorArwingPilotWaitTimers) == 6);
    CHECK(dGRSectorArwingPilotWaitTimers[0][0] == 0);
    CHECK(dGRSectorArwingPilotWaitTimers[0][1] == 7);
    CHECK(dGRSectorArwingPilotWaitTimers[1][0] == 7);
    CHECK(dGRSectorArwingPilotWaitTimers[5][0] == 45);
    CHECK((dGRSectorArwingPilotWaitTimers[5][0] +
           dGRSectorArwingPilotWaitTimers[5][1]) == 55);

    sHostMapBlob = NULL;
    gStageBound = saved_bound;
}

static Stage sCastleStage;

static void test_gr_castle(void)
{
    Stage *saved_bound = gStageBound;
    GObj ground, bumper;
    DObj ground_dobj, bumper_dobj;
    GObj *saved_bumper = gGRCommonStruct.castle.bumper_gobj;
    Vec3f saved_pos = gGRCommonStruct.castle.bumper_pos;

    gStageBound = &sCastleStage;
    memset(&sCastleStage, 0, sizeof(sCastleStage));
    CHECK(host_load_map_file(&sCastleStage, "castle.stg") == 0);

    /* NO OBJECTS AT ALL, and that is the shape: the pack's MPK1 block is
     * written with a zero-length pack area, which no other stage's is */
    CHECK(sCastleStage.map_object_count == 0);
    CHECK(sCastleStage.map_models[0].hd == NULL);
    /* ... and its item weights came through anyway, because the block
     * still exists -- the block is what map_nodes asked for, not the
     * object */
    CHECK(sHostMapHasWeights == 20);

    /* ONE script, on joint 0, under the name the decomp's own symbol
     * gives it */
    CHECK(sHostMapAnimCount == 1);
    CHECK(strcmp(sHostMapAnimNames[0], "AnimJointRoot") == 0);
    CHECK(sHostMapAnimJoints[0] == 0);
    CHECK(stage_map_anim("AnimJointRoot") != NULL);
    CHECK(stage_map_anim_joint("AnimJointRoot") == 0);
    CHECK(stage_map_anim("ScaleRetract") == NULL);   /* another stage's */

    /* AND THE WORDS. A name that RESOLVES is not a script that RUNS, so
     * this checks the sweep itself against the decomp's own source
     * (relocData/156_StageCastleFile3.c): five SetValBlock ramps on
     * AOBJ_FLAG_TRAX -- 0 at frame 0, -1050 at 599, +1050 at 1200, 0 at
     * 600 and 0 at 2400 -- then a SetAnim back to the top, which the
     * loader has already rewritten from the script's file offset to its
     * index in this pack. The MAGIC here is the decomp's assembler's,
     * written out: `aobjEvent32SetValBlock(flags, frame)` is
     * 0x06000000 | flags | (frame << 8). */
    {
        const u32 *w = (const u32 *)stage_map_anim("AnimJointRoot");

        CHECK(w[0] == 0x06080000);      /* SetValBlock TRAX, frame 0 */
        CHECK(w[2] == 0x06080257);      /* TRAX, frame 599 */
        CHECK(w[4] == 0x060804B0);      /* TRAX, frame 1200 */
        CHECK(w[6] == 0x06080258);      /* TRAX, frame 600 */
        CHECK(w[8] == 0x06080960);      /* TRAX, frame 2400 */
        /* the ramps' own values, as the decomp's own float words:
         * 0.0F, -1050.0F, +1050.0F, 0.0F, 0.0F -- the sweep left, back
         * right, and home */
        CHECK(w[1] == 0x00000000);
        CHECK(w[3] == 0xC4834000);
        CHECK(w[5] == 0x44834000);
        CHECK(w[7] == 0x00000000);
        CHECK(w[9] == 0x00000000);
        /* and the loop, which is the pack's one FIXUP: the decomp's
         * word 11 is a relocated pointer to the script's own first word,
         * and stage_load_map writes the target's INDEX there -- 0, this
         * pack's only script. The opcode word in front of it keeps the
         * decomp's own 0x1C000000.
         *
         * The WORD ITSELF is not asserted, and that is the host build's
         * doing: the fixup store is a `uint32_t` (both hex_load_map_file
         * and stage_load_map write `(uint32_t)(uintptr_t)`), which is
         * lossless for a 32-bit target and truncating for this 64-bit
         * one. The target's SetAnim lands on the script; the host's
         * lands on the script's low half. Every stage test in this file
         * has lived with that since Zebes -- this is the note that says
         * so, rather than a fourth silent one. */
        CHECK(w[10] == 0x1C000000);
    }

    /* grCastleBumperProcUpdate: the whole of the stage's motion, and it
     * is pure -- the Bumper's x becomes the carriage's swept x PLUS the
     * position the map object tagged. */
    memset(&ground, 0, sizeof(ground));
    memset(&ground_dobj, 0, sizeof(ground_dobj));
    memset(&bumper, 0, sizeof(bumper));
    memset(&bumper_dobj, 0, sizeof(bumper_dobj));
    ground.obj = &ground_dobj;
    bumper.obj = &bumper_dobj;

    gGRCommonStruct.castle.bumper_pos.x = 1600.0F;
    gGRCommonStruct.castle.bumper_pos.y = 300.0F;
    gGRCommonStruct.castle.bumper_pos.z = -200.0F;
    gGRCommonStruct.castle.bumper_gobj = &bumper;

    /* the sweep's two ends, handed over directly: the script is what
     * moves the carriage on the target, and the host cannot walk it */
    ground_dobj.translate.vec.f.x = -1050.0F;
    grCastleBumperProcUpdate(&ground);
    CHECK_NEAR(bumper_dobj.translate.vec.f.x, 1600.0F - 1050.0F, 1e-3F);
    /* y and z are NOT the carriage's: the sweep is TraX only, and this
     * is what says the updater writes one axis */
    CHECK_NEAR(bumper_dobj.translate.vec.f.y, 0.0F, 1e-4F);
    CHECK_NEAR(bumper_dobj.translate.vec.f.z, 0.0F, 1e-4F);

    ground_dobj.translate.vec.f.x = 1050.0F;
    grCastleBumperProcUpdate(&ground);
    CHECK_NEAR(bumper_dobj.translate.vec.f.x, 1600.0F + 1050.0F, 1e-3F);

    /* the decomp's own NULL guard, which is load-bearing: the item can be
     * gone while this process keeps running */
    gGRCommonStruct.castle.bumper_gobj = NULL;
    bumper_dobj.translate.vec.f.x = 42.0F;
    grCastleBumperProcUpdate(&ground);
    CHECK_NEAR(bumper_dobj.translate.vec.f.x, 42.0F, 1e-4F);

    /* NOT driven here, and each for its own reason: grCastleMakeGround
     * makes a REAL item through itManagerMakeItemSetupCommon (covered by
     * test_it_gbumper, which spawns one and checks the Castle branch) and
     * reads the map's own nMPMapObjKindBumper position, which needs the
     * collision tables bound to this stage -- and the anim attachment in
     * grCastleInitAll is host-guarded, because AObjEvent32 is eight bytes
     * here and the interpreter would never terminate. */

    /* the map's Bumper object is the one thing the stage's spawn position
     * comes from, and the mock world's geometry is not this stage's: the
     * host has no bound collision for Castle, so
     * mpCollisionGetMapObjIDsKind would answer about the mock deck. It is
     * checked on the target instead, where grCastleInitAll runs for real.
     * What the pack CAN say here is that the item weights came through
     * beside the anims -- the host reads them into its own array rather
     * than into the Stage, because the target's stage_load_map puts them
     * in map_owned and this build's Stage has none.
     *
     * `sHostMapWeights[0]` is the decomp's own first weight for this
     * stage (dGRCastleMap_item_weights, relocData/259_GRCastleMap.c). */
    CHECK(sHostMapHasWeights == 20);
    CHECK(sHostMapWeights[0] == 0x50);

    free(sHostMapBlob);
    sHostMapBlob = NULL;
    gGRCommonStruct.castle.bumper_gobj = saved_bumper;
    gGRCommonStruct.castle.bumper_pos = saved_pos;
    gStageBound = saved_bound;
}

/* ---- Race to the Finish --------------------------------
 *
 * src/dc/grbonus3.c is the tenth stage-logic file and the smallest, and
 * unlike the nine before it every one of its three pieces can be driven
 * here: the bumpers come out of the pack rather than off a reloc walk
 * (the file's second divergence), the barrel comes off one map object,
 * and the gate is a material test on one fighter.
 *
 * What the mock world has to be lent is the map: the barrel's position
 * is `nMPMapObjKind1PGameBonus3TaruBomb`, which kMapObjs does not carry,
 * and grBonus3TaruBombMakeActor LOOPS FOREVER on any count but one --
 * the decomp's own error arm. The fixture binds two objects so that
 * count is a real filter and not an accident of an empty table.
 */
static Stage sBonus3Stage;
static MPMapObjData sBonus3MapObjs[2];

/* named rather than left implicit: hosttest_ft.c does not include the
 * decomp's gr headers, and an undeclared function here is silently given
 * an `int` return and unchecked arguments */
extern void grBonus3MakeBumpers(void);
extern void grBonus3TaruBombProcUpdate(GObj *ground_gobj);
extern void grBonus3TaruBombMakeActor(void);
extern void grBonus3FinishProcUpdate(GObj *ground_gobj);
extern void grBonus3FinishMakeActor(void);
extern GObj *grBonus3MakeGround(void);
extern s32 gmCommonLoadFiles(void);

/* the barrel's own tagged position, and a second object of another kind
 * beside it so mpCollisionGetMapObjCountKind has something to reject */
#define BONUS3_BARREL_X (-1400)
#define BONUS3_BARREL_Y (2200)

static s32 bonus3_count_items(s32 kind)
{
    GObj *g;
    s32 n = 0;

    for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL; g = g->link_next)
    {
        if (itGetStruct(g)->kind == kind)
        {
            n++;
        }
    }
    return n;
}

/* THE ITEMS THIS TEST MADE, AND ONLY THOSE. The link is not empty when
 * the test starts -- an earlier one leaves two Crates on it, and the
 * scene tests further down expect them to still be there -- so the ones
 * standing at entry are written down and everything else is taken off.
 *
 * It restarts from the head each time round rather than following a
 * `next` captured before the eject: gcEjectGObj can take more than the
 * one GObj with it. The bound is the pool's size, so a link that will
 * not clear is a failure and not a hang. */
static GObj *sBonus3KeepItems[ITEM_ALLOC_MAX];
static s32 sBonus3KeepCount;

static void bonus3_note_items(void)
{
    GObj *g;

    sBonus3KeepCount = 0;
    for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL;
         g = g->link_next)
    {
        CHECK(sBonus3KeepCount < (s32)ARRAY_COUNT(sBonus3KeepItems));
        if (sBonus3KeepCount < (s32)ARRAY_COUNT(sBonus3KeepItems))
        {
            sBonus3KeepItems[sBonus3KeepCount++] = g;
        }
    }
}

static sb32 bonus3_is_kept(GObj *g)
{
    s32 i;

    for (i = 0; i < sBonus3KeepCount; i++)
    {
        if (sBonus3KeepItems[i] == g)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/* the same, for the GROUND link. Both of this file's makers put a
 * process on a ground GObj of their own and keep no handle to it -- the
 * scene ejects them on the way out, which a test has no way in -- and a
 * barrel maker left running would go on making barrels through every
 * test after this one. That is exactly what it did the first time this
 * test was written. */
static GObj *sBonus3KeepGrounds[16];
static s32 sBonus3KeepGroundCount;

static void bonus3_note_grounds(void)
{
    GObj *g;

    sBonus3KeepGroundCount = 0;
    for (g = gGCCommonLinks[nGCCommonLinkIDGround]; g != NULL;
         g = g->link_next)
    {
        CHECK(sBonus3KeepGroundCount <
              (s32)ARRAY_COUNT(sBonus3KeepGrounds));
        if (sBonus3KeepGroundCount < (s32)ARRAY_COUNT(sBonus3KeepGrounds))
        {
            sBonus3KeepGrounds[sBonus3KeepGroundCount++] = g;
        }
    }
}

static s32 bonus3_new_grounds(void)
{
    GObj *g;
    s32 n = 0;
    s32 i;

    for (g = gGCCommonLinks[nGCCommonLinkIDGround]; g != NULL;
         g = g->link_next)
    {
        sb32 kept = FALSE;

        for (i = 0; i < sBonus3KeepGroundCount; i++)
        {
            if (sBonus3KeepGrounds[i] == g)
            {
                kept = TRUE;
            }
        }
        if (!kept)
        {
            n++;
        }
    }
    return n;
}

static void bonus3_eject_grounds(void)
{
    s32 guard;

    for (guard = 0; guard < 64; guard++)
    {
        GObj *g = gGCCommonLinks[nGCCommonLinkIDGround];
        s32 i;

        while (g != NULL)
        {
            sb32 kept = FALSE;

            for (i = 0; i < sBonus3KeepGroundCount; i++)
            {
                if (sBonus3KeepGrounds[i] == g)
                {
                    kept = TRUE;
                }
            }
            if (!kept)
            {
                break;
            }
            g = g->link_next;
        }
        if (g == NULL)
        {
            return;
        }
        gcEjectGObj(g);
    }
    CHECK(FALSE);                   /* the link would not clear */
}

static void bonus3_eject_items(void)
{
    s32 guard;

    for (guard = 0; guard < ITEM_ALLOC_MAX * 4; guard++)
    {
        GObj *g = gGCCommonLinks[nGCCommonLinkIDItem];

        while ((g != NULL) && bonus3_is_kept(g))
        {
            g = g->link_next;
        }
        if (g == NULL)
        {
            return;
        }
        gcEjectGObj(g);
    }
    CHECK(FALSE);                   /* the link would not clear */
}

static void test_gr_bonus3(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    Stage *saved_bound = gStageBound;
    ITStruct *saved_free = gITManagerStructsAllocFree;
    MPGeometryData geo;
    MPGeometryData *saved_geo = gMPCollisionGeometry;
    void *saved_mapobjs = gMPCollisionMapObjs;
    MPYakumonoDObj *saved_yakumono = gMPCollisionYakumonoDObjs;
    Vec3f *saved_speeds = gMPCollisionSpeeds;
    s32 saved_gkind = gSCManagerBattleState->gkind;
    s32 saved_status = gSCManagerBattleState->game_status;
    s32 saved_player = gSCManagerSceneData.player;
    GObj *saved_slot = gSCManagerBattleState->players[0].fighter_gobj;
    sb32 loaded_here = FALSE;
    s32 i;

    if (itemPackLoaded() == 0)
    {
        CHECK(itemPackLoad("itcommon.itp") == 0);
        loaded_here = TRUE;
    }
    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
    {
        pool[i].next = &pool[i + 1];
    }
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    /* the items and grounds already standing, which are not this test's
     * to eject */
    bonus3_note_items();
    bonus3_note_grounds();

    gStageBound = &sBonus3Stage;
    memset(&sBonus3Stage, 0, sizeof(sBonus3Stage));
    /* NO MPK1 BLOCK, which is this stage and no other: its MPGroundData
     * names no map_nodes at all, so host_load_map_file has nothing to
     * open here and the BMP1 block is read on its own. */
    CHECK(host_load_placements(&sBonus3Stage, "bonus3.stg", "BMP1") == 4);

    /* ---- the BMP1 block, which IS grBonus3MakeBumpers' two tables ----
     *
     * Four placements and four scripts, one script per bumper. The
     * exporter drops the DObjDesc array's first entry exactly the way
     * this function's `anim_joint++, dobjdesc++` drops it, so four here
     * is four iterations of the decomp's loop -- and the positions below
     * are the decomp's own, read off relocData 162 (GRBonus3File3). */
    CHECK(sBonus3Stage.bumper_count == 4);
    CHECK(sBonus3Stage.bumper_anim_count == 4);
    CHECK_NEAR(sBonus3Stage.bumpers[0].x, 900.0F, 1e-3F);
    CHECK_NEAR(sBonus3Stage.bumpers[0].y, -2550.0F, 1e-3F);
    CHECK_NEAR(sBonus3Stage.bumpers[2].x, -1050.0F, 1e-3F);
    CHECK_NEAR(sBonus3Stage.bumpers[2].y, -2550.0F, 1e-3F);
    CHECK_NEAR(sBonus3Stage.bumpers[3].x, -2550.0F, 1e-3F);
    CHECK_NEAR(sBonus3Stage.bumpers[3].y, -3600.0F, 1e-3F);
    /* every bumper on this course moves, and each on a script of its own
     * -- which is why the still-bumper arm (`bumper_anim[i] < 0`) is not
     * exercised here: no placement on this stage takes it. The Board the
     * Platforms courses are where a still one lives. */
    for (i = 0; i < (s32)sBonus3Stage.bumper_count; i++)
    {
        CHECK(sBonus3Stage.bumper_anim[i] == (int8_t)i);
        CHECK(sBonus3Stage.bumper_anims[i] != NULL);
        /* a name that RESOLVES is not a script that RUNS: opcode 0 is
         * 0x00000000, so "not zero" is exactly "not an immediate end" */
        CHECK(((const u32 *)sBonus3Stage.bumper_anims[i])[0] != 0);
    }
    /* and the first one's own first two words, which say what kind of
     * script it is: 0x12100000 is the TraY SetInterp and 0xC51F6000 is
     * -2550.0F, the height bumper 0 stands at. A script that only
     * resolved could not carry its own placement's y. */
    CHECK(((const u32 *)sBonus3Stage.bumper_anims[0])[0] == 0x12100000);
    CHECK(((const u32 *)sBonus3Stage.bumper_anims[0])[1] == 0xC51F6000);

    /* ---- grBonus3MakeBumpers, driven for real ------------------------
     *
     * The four items are the stage's, not a fighter's: the gkind is NOT
     * Peach's Castle, so itGBumperMakeItem leaves the table's own
     * knockback alone (itgbumper.c:195, and test_it_gbumper pins both
     * arms of that branch). */
    gSCManagerBattleState->gkind = nGRKindBonus3;
    bonus3_eject_items();
    CHECK(bonus3_count_items(nITKindGBumper) == 0);

    grBonus3MakeBumpers();
    CHECK(bonus3_count_items(nITKindGBumper) == 4);
    {
        GObj *g;
        s32 seen = 0;

        for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL;
             g = g->link_next)
        {
            ITStruct *ip = itGetStruct(g);
            DObj *dobj;

            if (ip->kind != nITKindGBumper)
            {
                continue;
            }
            dobj = DObjGetStruct(g);
            /* the placement reached the item: bumper 0's own x, which is
             * the only one of the four at +900 */
            if (dobj->translate.vec.f.x > 800.0F)
            {
                CHECK_NEAR(dobj->translate.vec.f.y, -2550.0F, 1e-3F);
                seen++;
            }
            /* a stage item, however it was made */
            CHECK(ip->owner_gobj == NULL);
            CHECK(ip->attack_coll.interact_mask == GMHITCOLLISION_FLAG_FIGHTER);
        }
        CHECK(seen == 1);
    }
    bonus3_eject_items();

    /* a stage with NO bumpers makes none, and does not walk off the end
     * of the arrays looking for a terminator the port does not have */
    sBonus3Stage.bumper_count = 0;
    grBonus3MakeBumpers();
    CHECK(bonus3_count_items(nITKindGBumper) == 0);
    sBonus3Stage.bumper_count = 4;

    /* ---- grBonus3TaruBombMakeActor and the barrel's clock ------------ */
    sBonus3MapObjs[0].mapobj_kind = nMPMapObjKindRebirth;
    sBonus3MapObjs[0].pos.x = 0;
    sBonus3MapObjs[0].pos.y = 0;
    sBonus3MapObjs[1].mapobj_kind = nMPMapObjKind1PGameBonus3TaruBomb;
    sBonus3MapObjs[1].pos.x = BONUS3_BARREL_X;
    sBonus3MapObjs[1].pos.y = BONUS3_BARREL_Y;
    geo = mock_geo;
    geo.mapobj_count = 2;
    geo.mapobjs = (void *)sBonus3MapObjs;
    mpCollisionLoadGeometry(&geo);

    CHECK(mpCollisionGetMapObjCountKind(nMPMapObjKind1PGameBonus3TaruBomb)
          == 1);
    memset(&gGRCommonStruct.bonus3, 0, sizeof(gGRCommonStruct.bonus3));
    grBonus3TaruBombMakeActor();
    /* the maker's own ground GObj, which carries the process -- one, and
     * this test's to take away again */
    CHECK(bonus3_new_grounds() == 1);
    /* the position came off the map object and not off the pack */
    CHECK_NEAR(gGRCommonStruct.bonus3.tarubomb_make_pos.x,
               (f32)BONUS3_BARREL_X, 1e-3F);
    CHECK_NEAR(gGRCommonStruct.bonus3.tarubomb_make_pos.y,
               (f32)BONUS3_BARREL_Y, 1e-3F);
    CHECK_NEAR(gGRCommonStruct.bonus3.tarubomb_make_pos.z, 0.0F, 1e-4F);
    CHECK(gGRCommonStruct.bonus3.tarubomb_make_wait == 180);
    /* grBonus3InitHeaders is not ported, and this is what says so: both
     * bases stay NULL and nothing in the file reads them (the file's
     * first divergence) */
    CHECK(gGRCommonStruct.bonus3.map_head == NULL);
    CHECK(gGRCommonStruct.bonus3.item_head == NULL);

    /* grBonus3TaruBombProcUpdate: 180 tics of counting down and one
     * barrel on the tic the counter reaches zero. The decrement is at
     * the TAIL, so the make tic is the one that starts at 0 and the
     * period is 181 calls, not 180 -- which is the decomp's own shape
     * and the reason this counts them. */
    bonus3_eject_items();
    CHECK(bonus3_count_items(nITKindTaruBomb) == 0);
    for (i = 0; i < 180; i++)
    {
        grBonus3TaruBombProcUpdate(NULL);
    }
    CHECK(bonus3_count_items(nITKindTaruBomb) == 0);
    CHECK(gGRCommonStruct.bonus3.tarubomb_make_wait == 0);
    grBonus3TaruBombProcUpdate(NULL);
    CHECK(bonus3_count_items(nITKindTaruBomb) == 1);
    /* re-armed to 180 and then decremented in the same call */
    CHECK(gGRCommonStruct.bonus3.tarubomb_make_wait == 179);
    {
        GObj *g;

        for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL;
             g = g->link_next)
        {
            if (itGetStruct(g)->kind == nITKindTaruBomb)
            {
                DObj *dobj = DObjGetStruct(g);

                CHECK_NEAR(dobj->translate.vec.f.x, (f32)BONUS3_BARREL_X,
                           1e-3F);
                CHECK_NEAR(dobj->translate.vec.f.y, (f32)BONUS3_BARREL_Y,
                           1e-3F);
                /* falling from rest, which is the whole of the spawn */
                CHECK_NEAR(itGetStruct(g)->physics.vel_air.x, 0.0F, 1e-4F);
            }
        }
    }
    bonus3_eject_items();

    /* ---- grBonus3FinishProcUpdate, the win condition -----------------
     *
     * The banner it raises is a real one, out of the announcer's plain
     * alphabet (file 37), so the eight sprite banks have to be resident
     * the way scVSBattleStartScene leaves them. gmCommonLoadFiles is
     * what every scene calls; seven of the eight ship. */
    CHECK(gmCommonLoadFiles() == 7);
    {
        GObj tfighter;
        FTStruct tfp;

        memset(&tfighter, 0, sizeof(tfighter));
        memset(&tfp, 0, sizeof(tfp));
        tfighter.user_data.p = &tfp;

        gSCManagerSceneData.player = 0;
        gSCManagerBattleState->game_status = nSCBattleGameStatusGo;

        /* the port's own guard, and the only line of this function that
         * is not the decomp's: a slot with nobody in it is left alone
         * rather than dereferenced (DB_BOOT_STAGE can boot this stage
         * into a VS battle with an empty P1) */
        gSCManagerBattleState->players[0].fighter_gobj = NULL;
        grBonus3FinishProcUpdate(NULL);
        CHECK(gSCManagerBattleState->game_status == nSCBattleGameStatusGo);

        gSCManagerBattleState->players[0].fighter_gobj = &tfighter;

        /* AIRBORNE over the gate is not the gate: both halves of the
         * test have to hold */
        tfp.ga = nMPKineticsAir;
        tfp.coll_data.floor_flags = nMPMaterialDetect;
        grBonus3FinishProcUpdate(NULL);
        CHECK(gSCManagerBattleState->game_status == nSCBattleGameStatusGo);

        /* and grounded on any other material is not it either */
        tfp.ga = nMPKineticsGround;
        tfp.coll_data.floor_flags = nMPMaterialSpikes;
        grBonus3FinishProcUpdate(NULL);
        CHECK(gSCManagerBattleState->game_status == nSCBattleGameStatusGo);

        /* the material is read out of the LOW bits and the rest of the
         * word is somebody else's: MAP_VERTEX_MAT_MASK is what says so */
        tfp.coll_data.floor_flags = (u32)nMPMaterialDetect |
                                    ~(u32)MAP_VERTEX_MAT_MASK;
        grBonus3FinishProcUpdate(NULL);
        CHECK(gSCManagerBattleState->game_status == nSCBattleGameStatusEnd);

        /* what ifCommonAnnounceCompleteInitInterface left behind: the
         * End status above, and the banner itself -- nine letters of
         * COMPLETE! on the interface link, white on red. */
        {
            GObj *g;
            GObj *banner = NULL;

            for (g = gGCCommonLinks[nGCCommonLinkIDInterface]; g != NULL;
                 g = g->link_next)
            {
                SObj *sobj = (g->obj_kind == 2) ? SObjGetStruct(g) : NULL;

                if ((sobj != NULL) && (sobj->pos.x == 46.0F) &&
                    (sobj->pos.y == 101.0F))
                {
                    banner = g;
                    break;
                }
            }
            CHECK(banner != NULL);
            if (banner != NULL)
            {
                SObj *sobj = SObjGetStruct(banner);
                s32 n = 0;

                while (sobj != NULL)
                {
                    CHECK(sobj->sprite.attr ==
                          (SP_TEXSHUF | SP_TRANSPARENT));
                    CHECK(sobj->sprite.red == 0xFF);
                    CHECK(sobj->envcolor.r == 0xFF);
                    CHECK(sobj->envcolor.g == 0x00);
                    CHECK(sobj->envcolor.b == 0x00);
                    n++;
                    sobj = sobj->next;
                }
                CHECK(n == 9);
                gcEjectGObj(banner);
            }
        }
    }

    /* ---- grBonus3MakeGround, the three makers in the decomp's order --
     *
     * It returns NULL and means it: this is the one stage in the game
     * with no ground GObj of its own, because each actor makes its own.
     * Three GObjs come out of it -- the four bumpers are items, the
     * barrel maker is one and the gate is the other -- and both of the
     * latter carry a process this test then takes away, for the reason
     * bonus3_eject_grounds gives. */
    bonus3_eject_grounds();
    bonus3_eject_items();
    CHECK(bonus3_new_grounds() == 0);
    CHECK(grBonus3MakeGround() == NULL);
    CHECK(bonus3_count_items(nITKindGBumper) == 4);
    CHECK(bonus3_new_grounds() == 2);
    CHECK(gGRCommonStruct.bonus3.tarubomb_make_wait == 180);
    bonus3_eject_grounds();

    /* the mock world back the way it was: the geometry, the item pool,
     * the battle state and the stage binding */
    gMPCollisionGeometry = saved_geo;
    gMPCollisionMapObjs = saved_mapobjs;
    gMPCollisionYakumonoDObjs = saved_yakumono;
    gMPCollisionSpeeds = saved_speeds;
    gSCManagerBattleState->players[0].fighter_gobj = saved_slot;
    gSCManagerBattleState->game_status = saved_status;
    gSCManagerBattleState->gkind = saved_gkind;
    gSCManagerSceneData.player = saved_player;
    gITManagerStructsAllocFree = saved_free;
    if (loaded_here)
    {
        itemPackRelease();
    }
    bonus3_eject_items();
    free(sBonus3Stage.bumper_owned);
    sBonus3Stage.bumper_owned = NULL;
    gStageBound = saved_bound;

    printf("bonus3: 4 bumpers off the pack's BMP1 block, a barrel every "
           "180 tics at the map's own object, and a grounded fighter on "
           "nMPMaterialDetect raises COMPLETE!\n");
}

/* ---- the bonus stage -----------------------------------
 *
 * src/dc/sc1pbonusstage.c's Break the Targets half, driven the way
 * test_gr_bonus3 above drives Race to the Finish's: a course's BTG1
 * block loaded by hand, the targets made off it for real, and the task
 * row and the count walked down to the COMPLETE! banner.
 *
 * What is NOT driven here is the scene itself -- sc1PBonusStageFuncStart
 * runs inside syTaskmanStartTask and wants a fighter, a camera and a
 * bound collision world. The disc probe is what sees that; this is the
 * data path and the count, which is where a wrong pack read would show.
 */
static Stage sBonusStage;
/* a Board the Platforms course, for the bumper half */
static Stage sBonus2Stage;

/* named rather than left implicit, for test_gr_bonus3's reason: an
 * undeclared function here is silently given an `int` return */
extern void sc1PBonusStageInitVars(void);
extern void sc1PBonusStageSetupFiles(void);
extern void sc1PBonusStageMakeTargets(void);
extern void sc1PBonusStageMakeTargetSprites(void);
extern void sc1PBonusStageMakeTaskSprites(void);
extern void sc1PBonusStageUpdateTargetCount(void);
extern void sc1PBonusStageMakeBonus1Ground(void);
extern void sc1PBonusStageSetPlayerInterfacePositions(void);
/* and the Board the Platforms half */
extern void sc1PBonusStageMakePlatformSprites(void);
extern void sc1PBonusStageUpdatePlatformInterface(void);
extern s32 sc1PBonusStageGetPlatformKind(s32 line_id);
extern void sc1PBonusStageMakeBumpers(void);

/* and src/dc/ifcommon.c's end-of-match announcer, whose bonus arm is
 * covered here: the function, the two proc_sets it picks between, and
 * the slot it parks one in */
extern void ifCommonAnnounceEndMessage(void);
extern void ifCommonBattleInterfaceProcSet(void);
extern void ifCommon1PGameInterfaceProcSet(void);
extern void (*sIFCommonBattleInterfaceProcSet)(void);

static s32 bonus_row_sprites_of(GObj *gobj)
{
    SObj *sobj;
    s32 n = 0;

    if (gobj == NULL)
    {
        return -1;
    }
    for (sobj = SObjGetStruct(gobj); sobj != NULL; sobj = sobj->next)
    {
        n++;
    }
    return n;
}

static s32 bonus_row_sprites(void)
{
    return bonus_row_sprites_of(gGRCommonStruct.bonus1.interface_gobj);
}

/* every interface GObj gone, so the next banner made is the only one */
static void bonus_eject_interface(void)
{
    GObj *g;

    while ((g = gGCCommonLinks[nGCCommonLinkIDInterface]) != NULL)
    {
        gcEjectGObj(g);
    }
}

/* which announce banner is up, by the x its table's first letter sits
 * at: 22 is dIFCommonAnnounceGameSetSpriteData, 77 the Failure one.
 * -1.0 when nothing was made. */
static float bonus_banner_x(void)
{
    GObj *g;

    for (g = gGCCommonLinks[nGCCommonLinkIDInterface]; g != NULL;
         g = g->link_next)
    {
        if (g->obj_kind == 2)
        {
            return SObjGetStruct(g)->pos.x;
        }
    }
    return -1.0F;
}

static void test_sc1p_bonus_stage(void)
{
    static ITStruct pool[ITEM_ALLOC_MAX];
    Stage *saved_bound = gStageBound;
    ITStruct *saved_free = gITManagerStructsAllocFree;
    s32 saved_gkind = gSCManagerBattleState->gkind;
    s32 saved_status = gSCManagerBattleState->game_status;
    SCBattleState *saved_state = gSCManagerBattleState;
    SCCommonData saved_scene = gSCManagerSceneData;
    LBBackupData saved_backup = gSCManagerBackupData;
    sb32 loaded_here = FALSE;
    s32 i;

    if (itemPackLoaded() == 0)
    {
        CHECK(itemPackLoad("itcommon.itp") == 0);
        loaded_here = TRUE;
    }
    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
    {
        pool[i].next = &pool[i + 1];
    }
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;

    bonus3_note_items();

    /* ---- sc1PBonusStageInitVars, the ladder/practice split -----------
     *
     * The gkind is the FIGHTER's index added to the range start, which
     * is what makes the twelve courses one per fighter, and which range
     * it is added to is decided by which scene sent us. Both paths, and
     * both clocks. */
    gSCManagerBattleState = &gSCManagerVSBattleState;   /* anything but the scene's */
    gSCManagerSceneData.player = 1;

    gSCManagerSceneData.scene_prev = nSCKind1PBonus1Players;
    gSCManagerSceneData.bonus_fkind = nFTKindLink;
    gSCManagerSceneData.bonus_costume = 2;
    sc1PBonusStageInitVars();
    /* the scene took the pointer for itself, and it is NOT the VS one */
    CHECK(gSCManagerBattleState != &gSCManagerVSBattleState);
    CHECK(gSCManagerBattleState->gkind == nGRKindBonus1Start + nFTKindLink);
    CHECK(gSCManagerBattleState->game_type == nSCBattleGameTypeBonus);
    CHECK(gSCManagerBattleState->game_rules ==
          (SCBATTLE_GAMERULE_BONUS | SCBATTLE_GAMERULE_TIME));
    CHECK(gSCManagerBattleState->pl_count == 1);
    CHECK(gSCManagerBattleState->cp_count == 0);
    /* a practice run is untimed and counts UP */
    CHECK(gSCManagerBattleState->time_limit == SCBATTLE_TIMELIMIT_INFINITE);
    /* one human, in gSCManagerSceneData.player's own slot, and nobody
     * anywhere else */
    CHECK(gSCManagerBattleState->players[1].pkind == nFTPlayerKindMan);
    CHECK(gSCManagerBattleState->players[1].fkind == nFTKindLink);
    CHECK(gSCManagerBattleState->players[1].costume == 2);
    CHECK(gSCManagerBattleState->players[0].pkind == nFTPlayerKindNot);
    CHECK(gSCManagerBattleState->players[2].pkind == nFTPlayerKindNot);
    CHECK(gSCManagerBattleState->players[3].pkind == nFTPlayerKindNot);

    /* and from the LADDER: the run's own fighter and costume, and two
     * minutes on the clock unless the run itself is untimed */
    gSCManagerSceneData.scene_prev = nSCKind1PGame;
    gSCManagerSceneData.spgame_stage = nSC1PGameStageBonus1;
    gSCManagerSceneData.fkind = nFTKindKirby;
    gSCManagerSceneData.costume = 1;
    gSCManagerSceneData.spgame_time_limit = 5;
    sc1PBonusStageInitVars();
    CHECK(gSCManagerBattleState->gkind == nGRKindBonus1Start + nFTKindKirby);
    CHECK(gSCManagerBattleState->time_limit == 2);
    CHECK(gSCManagerBattleState->players[1].fkind == nFTKindKirby);
    CHECK(gSCManagerBattleState->players[1].costume == 1);

    gSCManagerSceneData.spgame_time_limit = SCBATTLE_TIMELIMIT_INFINITE;
    sc1PBonusStageInitVars();
    CHECK(gSCManagerBattleState->time_limit == SCBATTLE_TIMELIMIT_INFINITE);

    /* the HUD row's own positions, which this scene moves to the bottom
     * of the screen because the task row is along the top */
    sc1PBonusStageSetPlayerInterfacePositions();
    CHECK(gIFCommonPlayerInterface.player_pos_y == 210);
    CHECK(gIFCommonPlayerInterface.player_pos_x[0] == 55);
    CHECK(gIFCommonPlayerInterface.player_pos_x[3] == 55);

    /* ---- the BTG1 block, which IS sc1PBonusStageMakeTargets' two
     * tables ------------------------------------------------------------
     *
     * Mario's course: ten targets, of which exactly ONE moves, on a
     * script the exporter named `Targets3`. The count is the game's own
     * SCBATTLE_BONUSGAME_TASK_MAX and the decomp hangs the console on
     * any other, so ten here is not a convenience -- it is the contract
     * the exporter refuses to break. */
    gStageBound = &sBonusStage;
    memset(&sBonusStage, 0, sizeof(sBonusStage));
    CHECK(host_load_placements(&sBonusStage, "bonus1mario.stg", "BTG1")
          == 10);
    CHECK(sBonusStage.target_count == SCBATTLE_BONUSGAME_TASK_MAX);
    CHECK(sBonusStage.target_anim_count == 1);
    CHECK_NEAR(sBonusStage.targets[0].x, -1350.0F, 1e-3F);
    CHECK_NEAR(sBonusStage.targets[0].y, -2250.0F, 1e-3F);
    CHECK_NEAR(sBonusStage.targets[3].x, 4950.0F, 1e-3F);
    CHECK_NEAR(sBonusStage.targets[3].y, -1800.0F, 1e-3F);
    /* nine still ones and target 2 on script 0 -- the still arm
     * (`target_anim[i] < 0`) is the common case here, where Race to the
     * Finish's four bumpers never take it */
    for (i = 0; i < (s32)sBonusStage.target_count; i++)
    {
        CHECK(sBonusStage.target_anim[i] == ((i == 2) ? 0 : -1));
    }
    CHECK(sBonusStage.target_anims[0] != NULL);
    /* a name that RESOLVES is not a script that RUNS: opcode 0 is
     * 0x00000000, so "not zero" is exactly "not an immediate end" */
    CHECK(((const u32 *)sBonusStage.target_anims[0])[0] != 0);

    /* ---- sc1PBonusStageMakeTargets, driven for real ------------------ */
    bonus3_eject_items();
    CHECK(bonus3_count_items(nITKindTarget) == 0);
    gGRCommonStruct.bonus1.target_count = 0;
    gGRCommonStruct.bonus1.interface_gobj = NULL;

    sc1PBonusStageMakeTargets();
    CHECK(bonus3_count_items(nITKindTarget) == SCBATTLE_BONUSGAME_TASK_MAX);
    CHECK(gGRCommonStruct.bonus1.target_count == SCBATTLE_BONUSGAME_TASK_MAX);
    {
        GObj *g;
        s32 seen = 0;

        for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL;
             g = g->link_next)
        {
            ITStruct *ip = itGetStruct(g);
            DObj *dobj;

            if (ip->kind != nITKindTarget)
            {
                continue;
            }
            dobj = DObjGetStruct(g);
            /* target 3's own x, the only one of the ten past +4000 */
            if (dobj->translate.vec.f.x > 4000.0F)
            {
                CHECK_NEAR(dobj->translate.vec.f.y, -1800.0F, 1e-3F);
                seen++;
            }
            /* a stage item, and itTargetMakeItem's own two writes: a
             * GROUND item on no floor line, which is what pins it in
             * mid-air exactly where the course put it */
            CHECK(ip->owner_gobj == NULL);
            CHECK(ip->ga == nMPKineticsGround);
            CHECK(ip->coll_data.floor_line_id == -1);
        }
        CHECK(seen == 1);
    }

    /* ---- the task row, and walking it down to COMPLETE! --------------
     *
     * The row is one sprite per target still standing, and
     * sc1PBonusStageUpdateTargetInterface takes the LAST one off each
     * time -- it walks `target_count` links down after the decrement, so
     * the walk stops one short of where it would have. The banner it
     * ends on is a real one out of the announcer's alphabet, so the
     * sprite banks have to be resident the way every scene leaves them:
     * sc1PBonusStageSetupFiles is what loads both those and this
     * scene's own task-row bank. */
    sc1PBonusStageSetupFiles();
    sc1PBonusStageMakeTargetSprites();
    CHECK(gGRCommonStruct.bonus1.interface_gobj != NULL);
    CHECK(bonus_row_sprites() == SCBATTLE_BONUSGAME_TASK_MAX);

    /* a practice run whose record is not already perfect says COMPLETE!,
     * not NEW RECORD -- which is the only thing the backup is read for */
    gSCManagerSceneData.scene_prev = nSCKind1PBonus1Players;
    gSCManagerSceneData.bonus_fkind = nFTKindMario;
    gSCManagerBackupData.spgame_records[nFTKindMario].bonus1_task_count = 0;
    gSCManagerBattleState->game_status = nSCBattleGameStatusGo;

    for (i = 0; i < SCBATTLE_BONUSGAME_TASK_MAX - 1; i++)
    {
        sc1PBonusStageUpdateTargetCount();
        CHECK(gGRCommonStruct.bonus1.target_count ==
              SCBATTLE_BONUSGAME_TASK_MAX - 1 - i);
        CHECK(bonus_row_sprites() == SCBATTLE_BONUSGAME_TASK_MAX - 1 - i);
        /* nothing announced until the last one goes */
        CHECK(gSCManagerBattleState->game_status == nSCBattleGameStatusGo);
    }
    sc1PBonusStageUpdateTargetCount();
    CHECK(gGRCommonStruct.bonus1.target_count == 0);
    CHECK(gSCManagerBattleState->game_status == nSCBattleGameStatusEnd);

    /* the banner itself: nine letters of COMPLETE!, white on red, at the
     * position dIFCommonAnnounceCompleteSpriteData's first row names */
    {
        GObj *g;
        GObj *banner = NULL;

        for (g = gGCCommonLinks[nGCCommonLinkIDInterface]; g != NULL;
             g = g->link_next)
        {
            SObj *sobj = (g->obj_kind == 2) ? SObjGetStruct(g) : NULL;

            if ((sobj != NULL) && (sobj->pos.x == 46.0F) &&
                (sobj->pos.y == 101.0F))
            {
                banner = g;
                break;
            }
        }
        CHECK(banner != NULL);
        if (banner != NULL)
        {
            gcEjectGObj(banner);
        }
    }
    if (gGRCommonStruct.bonus1.interface_gobj != NULL)
    {
        gcEjectGObj(gGRCommonStruct.bonus1.interface_gobj);
        gGRCommonStruct.bonus1.interface_gobj = NULL;
    }
    bonus3_eject_items();

    /* ---- the FAILURE banner (src/dc/ifcommon.c) --
     *
     * Its guard is the whole of it: a clock that runs out on the tic the
     * tenth target breaks must not paint FAILURE over COMPLETE!, and
     * nSCBattleGameStatusEnd is what says the completion arm got there
     * first. */
    gSCManagerBattleState->game_status = nSCBattleGameStatusEnd;
    ifCommonAnnounceFailureInitInterface();
    {
        GObj *g;
        GObj *banner = NULL;

        for (g = gGCCommonLinks[nGCCommonLinkIDInterface]; g != NULL;
             g = g->link_next)
        {
            SObj *sobj = (g->obj_kind == 2) ? SObjGetStruct(g) : NULL;

            if ((sobj != NULL) && (sobj->pos.x == 77.0F) &&
                (sobj->pos.y == 101.0F))
            {
                banner = g;
            }
        }
        CHECK(banner == NULL);
    }
    gSCManagerBattleState->game_status = nSCBattleGameStatusGo;
    ifCommonAnnounceFailureInitInterface();
    {
        GObj *g;
        GObj *banner = NULL;

        for (g = gGCCommonLinks[nGCCommonLinkIDInterface]; g != NULL;
             g = g->link_next)
        {
            SObj *sobj = (g->obj_kind == 2) ? SObjGetStruct(g) : NULL;

            if ((sobj != NULL) && (sobj->pos.x == 77.0F) &&
                (sobj->pos.y == 101.0F))
            {
                banner = g;
                break;
            }
        }
        CHECK(banner != NULL);
        if (banner != NULL)
        {
            SObj *sobj = SObjGetStruct(banner);
            s32 n = 0;

            while (sobj != NULL)
            {
                /* white letters on a BLUE shadow, where COMPLETE!'s is
                 * red -- the one thing that tells the two tables apart
                 * at a glance */
                CHECK(sobj->envcolor.r == 0x00);
                CHECK(sobj->envcolor.g == 0x00);
                CHECK(sobj->envcolor.b == 0xFF);
                n++;
                sobj = sobj->next;
            }
            CHECK(n == 7);              /* F A I L U R E */
            gcEjectGObj(banner);
        }
    }

    /* ---- ifCommonAnnounceEndMessage's three arms
     *
     * What a fighter's death announces, and it is not the same thing in
     * the three modes that can reach it. A bonus stage gets FAILURE,
     * because falling off one IS the failure; a 1P match with stocks
     * left gets GAME SET over the 1P proc_set, which pulls the camera
     * out over 45 tics; everything else gets GAME SET over the VS one,
     * which snaps back in 3. The proc_set is the tell -- it is what
     * ifCommonBattleSetInterface parks for the freeze to run later. */
    {
        s32 saved_type = gSCManagerBattleState->game_type;
        s32 saved_stock = gSCManagerBattleState->players[0].stock_count;
        s32 saved_pl = gSCManagerSceneData.player;

        gSCManagerSceneData.player = 0;

        bonus_eject_interface();
        gSCManagerBattleState->gkind = nGRKindBonus1Start;
        gSCManagerBattleState->game_type = nSCBattleGameTypeBonus;
        ifCommonAnnounceEndMessage();
        CHECK(sIFCommonBattleInterfaceProcSet == ifCommonBattleInterfaceProcSet);
        CHECK(bonus_banner_x() == 77.0F);   /* FAILURE */

        bonus_eject_interface();
        gSCManagerBattleState->gkind = nGRKindHyrule;
        gSCManagerBattleState->game_type = nSCBattleGameType1PGame;
        gSCManagerBattleState->players[0].stock_count = 2;
        ifCommonAnnounceEndMessage();
        CHECK(sIFCommonBattleInterfaceProcSet == ifCommon1PGameInterfaceProcSet);
        CHECK(bonus_banner_x() == 22.0F);   /* GAME SET */

        /* out of stocks: the 1P game takes the VS arm, which is why the
         * port got away with only that one for as long as it did */
        bonus_eject_interface();
        gSCManagerBattleState->players[0].stock_count = -1;
        ifCommonAnnounceEndMessage();
        CHECK(sIFCommonBattleInterfaceProcSet == ifCommonBattleInterfaceProcSet);
        CHECK(bonus_banner_x() == 22.0F);

        bonus_eject_interface();
        gSCManagerBattleState->players[0].stock_count = saved_stock;
        gSCManagerBattleState->game_type = saved_type;
        gSCManagerSceneData.player = saved_pl;
    }

    /* ---- the Bonus2 arm of the task row ---------------
     *
     * The same dispatch the scene makes on gkind, from the other side:
     * a platform course draws the PLATFORM row, into bonus2's own GObj,
     * and leaves bonus1's alone. The row is `platform_count` long, which
     * is the count the collision walk found -- here set by hand, since
     * the walk itself is driven further down. */
    gSCManagerBattleState->gkind = nGRKindBonus2Start;
    gGRCommonStruct.bonus1.interface_gobj = NULL;
    gGRCommonStruct.bonus2.interface_gobj = NULL;
    gGRCommonStruct.bonus2.platform_count = 4;
    sc1PBonusStageMakeTaskSprites();
    CHECK(gGRCommonStruct.bonus1.interface_gobj == NULL);
    CHECK(gGRCommonStruct.bonus2.interface_gobj != NULL);
    CHECK(bonus_row_sprites_of(gGRCommonStruct.bonus2.interface_gobj) == 4);

    /* and one platform boarded takes one sprite off the end, the target
     * row's rule: the count goes down FIRST, which is what makes the
     * walk stop one short of where it would have
     * (sc1PBonusStageUpdatePlatformCount's order) */
    gGRCommonStruct.bonus2.platform_count--;
    sc1PBonusStageUpdatePlatformInterface();
    CHECK(bonus_row_sprites_of(gGRCommonStruct.bonus2.interface_gobj) == 3);
    gcEjectGObj(gGRCommonStruct.bonus2.interface_gobj);
    gGRCommonStruct.bonus2.interface_gobj = NULL;
    gGRCommonStruct.bonus2.platform_count = 0;

    /* ---- the six platform packs, by the names the scene asks for -----
     *
     * dSC1PBonusStagePlatformDescs and its Boarded twin are tables of
     * reloc labels the port cannot have, so src/dc/sc1pbonusstage.c asks
     * romdisk/bonus2plat.pak for a tree BY NAME. A typo there is a
     * platform that silently does not appear, so the six names are
     * pinned here against the exporter's own list
     * (tools/export/ssb_stageexport.py BONUS2_PLATFORMS) -- and in the order
     * the descs had them, because the size index doubles as the
     * user_data status the boarding swap reads back. */
    {
        static const char *const names[] =
        {
            "PlatformSmall", "PlatformMedium", "PlatformLarge",
            "BoardedPlatformSmall", "BoardedPlatformMedium",
            "BoardedPlatformLarge"
        };
        s32 n;

        memset(sBonusStage.platform_names, 0,
               sizeof(sBonusStage.platform_names));
        sBonusStage.platform_model_count = (int)ARRAY_COUNT(names);

        for (n = 0; n < (s32)ARRAY_COUNT(names); n++)
        {
            strcpy(sBonusStage.platform_names[n], names[n]);
        }
        for (n = 0; n < (s32)ARRAY_COUNT(names); n++)
        {
            CHECK(stage_platform_pack(&sBonusStage, names[n]) == n);
        }
        CHECK(stage_platform_pack(&sBonusStage, "PlatformHuge") == -1);
        sBonusStage.platform_model_count = 0;
    }

    /* ---- sc1PBonusStageMakeBumpers, off a real course's BMP1 block ---
     *
     * Fox's is the fullest of the five that have any -- seven of the
     * twelve courses have none at all, which is the decomp's
     * `{ 0x0, 0x0 }` row and this port's bumper_count 0. The count is
     * the check: the exporter drops the DObjDesc array's first entry
     * exactly where the decomp's `dobjdesc++, anim_joints++` drops it,
     * so what comes out here is the game's own iteration count. */
    {
        Stage *saved_stage = gStageBound;
        s32 made;

        memset(&sBonus2Stage, 0, sizeof(sBonus2Stage));
        CHECK(host_load_placements(&sBonus2Stage, "bonus2fox.stg", "BMP1") == 9);
        CHECK(sBonus2Stage.bumper_count == 9);
        /* exactly one bumper per course stands still, which is the
         * `bumper_anim[i] < 0` arm the target course never takes */
        made = 0;
        for (i = 0; i < (s32)sBonus2Stage.bumper_count; i++)
        {
            if (sBonus2Stage.bumper_anim[i] < 0)
            {
                made++;
            }
        }
        CHECK(made == 1);

        gStageBound = &sBonus2Stage;
        bonus3_eject_items();
        for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
        {
            pool[i].next = &pool[i + 1];
        }
        pool[ITEM_ALLOC_MAX - 1].next = NULL;
        gITManagerStructsAllocFree = pool;

        gSCManagerBattleState->gkind = nGRKindBonus2Start + nFTKindFox;
        CHECK(bonus3_count_items(nITKindGBumper) == 0);
        sc1PBonusStageMakeBumpers();
        CHECK(bonus3_count_items(nITKindGBumper) == 9);
        bonus3_eject_items();

        free(sBonus2Stage.bumper_owned);
        sBonus2Stage.bumper_owned = NULL;
        gStageBound = saved_stage;
    }

    /* and the Bonus1 arm, through the ground maker the stage chain
     * calls: ten items again, off the same block */
    gSCManagerBattleState->gkind = nGRKindBonus1Start;
    bonus3_eject_items();
    /* the pool re-linked, because ITEM_ALLOC_MAX is SIXTEEN and this
     * test wants ten twice: ejecting a GObj takes the item off the link
     * but the port's free list is itManager's own and the mock world
     * does not hand structs back to it. Safe here and only here --
     * every item this test made is off the link by now, and the ones it
     * was told to keep came out of the pool the caller owns. */
    for (i = 0; i < ITEM_ALLOC_MAX - 1; i++)
    {
        pool[i].next = &pool[i + 1];
    }
    pool[ITEM_ALLOC_MAX - 1].next = NULL;
    gITManagerStructsAllocFree = pool;
    sc1PBonusStageMakeBonus1Ground();
    CHECK(bonus3_count_items(nITKindTarget) == SCBATTLE_BONUSGAME_TASK_MAX);
    sc1PBonusStageMakeTaskSprites();
    CHECK(bonus_row_sprites() == SCBATTLE_BONUSGAME_TASK_MAX);
    if (gGRCommonStruct.bonus1.interface_gobj != NULL)
    {
        gcEjectGObj(gGRCommonStruct.bonus1.interface_gobj);
        gGRCommonStruct.bonus1.interface_gobj = NULL;
    }
    bonus3_eject_items();

    /* the mock world back the way it was */
    gSCManagerBackupData = saved_backup;
    gSCManagerSceneData = saved_scene;
    gSCManagerBattleState = saved_state;
    gSCManagerBattleState->game_status = saved_status;
    gSCManagerBattleState->gkind = saved_gkind;
    gITManagerStructsAllocFree = saved_free;
    if (loaded_here)
    {
        itemPackRelease();
    }
    free(sBonusStage.target_owned);
    sBonusStage.target_owned = NULL;
    gStageBound = saved_bound;
    memset(&gGRCommonStruct.bonus1, 0, sizeof(gGRCommonStruct.bonus1));

    printf("1pbonus: ten targets off Mario's BTG1 block, one of them on a "
           "script, the task row counted down to COMPLETE!, FAILURE "
           "refused once the course is already won, and a death "
           "announcing FAILURE on a course where a 1P match announces "
           "GAME SET over its own 45-tic proc_set; and the platform "
           "half's own row, its six pack names and Fox's nine bumpers "
           "(one of them still)\n");
}

/* ---- the collision line-existence contract, which is what killed the
 * boot battle ---------------------------------
 *
 * `mpCollisionGetVertexCountLineID` and its siblings spin in
 * `while (TRUE)` -- and the port's `scManagerRunPrintGObjStatus` aborts
 * -- when the line they are handed belongs to a yakumono the stage has
 * turned OFF (mp/mpcollision.c:3077). `mpCollisionCheckExistLineID` is
 * the question that answers whether that is safe, and it is what the
 * game's own callers ask first.
 *
 * The guest's crash was `mpGetNbVertex() no collision` with no other
 * collision message before it, which names its own caller by
 * elimination: ftshadow.c reaches the count through
 * `mpCollisionGetFloorEdgeL/R` and mpprocess.c through
 * `mpCollisionGetLRCommonLWall/RWall`, and BOTH of those carry the same
 * guard with a message of their own (`mpGetLREdge() no collision`,
 * `mpGetLRCommon() no collision`) -- so either would have printed first.
 * The only caller left is src/dc/db.c's `draw_collision`, which asks
 * `mpCollisionGetVertexFlagsLineID` first and THAT function has no status
 * guard. The overlay asks every line of every kind on every frame, so it
 * was the one place in the port where a line that had gone away could
 * take the console with it.
 *
 * This test pins the contract the fix turns on: existence is exactly
 * "the yakumono is not off", the flags query is unguarded (the reason
 * the overlay got as far as it did), and the count query is not reached
 * once existence is asked. */
static void test_collision_line_existence(void)
{
    /* the mock world's deck: line 1, on yakumono 0, which is what
     * mpCollisionLoadGeometry leaves everything at */
    DObj *yak = gMPCollisionYakumonoDObjs->dobjs[0];
    s16 saved = yak->user_data.s;

    CHECK(gMPCollisionLinesNum > 1);
    CHECK(gMPCollisionVertexInfo->vertex_info[1].yakumono_id == 0);

    CHECK(mpCollisionSetDObjNoID(1) == 0);
    CHECK(mpCollisionCheckExistLineID(1) == TRUE);

    /* and the flags query the overlay asks FIRST still answers -- it has
     * no status guard, which is exactly why the overlay walked into the
     * count query instead of being stopped one call earlier */
    (void)mpCollisionGetVertexFlagsLineID(1);

    /* off, and the line is gone as far as every guard is concerned */
    yak->user_data.s = nMPYakumonoStatusOff;
    CHECK(mpCollisionCheckExistLineID(1) == FALSE);

    /* -2 is the "no collision" sentinel and answers FALSE without
     * touching any table; -1 is not a question, it is a bug, and the
     * function spins on it -- so it is not asked here. */
    CHECK(mpCollisionCheckExistLineID(-2) == FALSE);

    /* a line the geometry does not have is not asked about at all: the
     * overlay's ids come out of gMPCollisionLineGroups, and every one of
     * those is in range by construction */
    yak->user_data.s = nMPYakumonoStatusOn;
    CHECK(mpCollisionCheckExistLineID(1) == TRUE);

    yak->user_data.s = saved;
}
