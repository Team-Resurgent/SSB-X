/* hosttest/effects.c -- part of hosttest_ft.c: the particle banks, the matrix kinds, colour lerps, the screen flash,
 * confetti and the effect manager.
 *
 * Not compiled on its own: hosttest_ft.c includes every part in
 * order, so this file sees everything the parts before it define. */

/* ---- the particle banks ---------------------------------
 *
 * A bank is two ROM byte ranges cut out at build time and byteswapped
 * into a little-endian machine's order (tools/export/ssb_particleexport.py).
 * That those bytes are right is tools/check/particle_check.py's leg: it
 * compiles all eighteen of the decomp's own src/particles sources for
 * the host and memcmps them against the exported files, so nothing here
 * restates what a bank contains.
 *
 * What is left for this suite is the port's side of the read --
 * src/dc/dma.c, which is the whole of what replaced the cartridge. The
 * game asks for a ROM address range and nothing else, so everything that
 * can go wrong is in the lookup: the wrong file for an address, the
 * right file at the wrong offset, a read walked off the end of a segment
 * into what is a different file here and was the next bytes of the same
 * ROM there.
 *
 * What is deliberately NOT here is lbParticleSetupBankID's walk, and the
 * reason is in scVSBattleLoadEffectBank: the bank's arrays are the ROM's
 * 32-bit pointers and this host's are 64-bit, so the walk cannot run in
 * this build at all. The target run is where it is checked, and the
 * `particles:` line is what it prints. The last block below is the part
 * of that the host *can* say: the shape the walk will find.
 */
/* gr/grcommon/gryoster.h's own four lGRYosterParticle* signs are the
 * real header's now -- the hand declaration that stood here was
 * uintptr_t where the header says intptr_t, and the include above is
 * what found it. */

/* The .txp reader (src/dc/lbpartex.c) against the .txb it is a
 * conversion of. The texels are tools/check/particletex_check.py's -- it holds
 * every one of them to the decomp's own extractParticleTextures.py -- so
 * what is left for a C test is the thing that Python cannot see: that
 * the loader this port links walks the file the exporter wrote and comes
 * out with the bank's own shape. The host build has no PVR, so no VRAM
 * is taken and lbpTexBytes counts what would have been. */
static void test_particle_texpack(void)
{
    long size;
    u8 *txb;
    u32 n, i, off, frames_total = 0, want_bytes = 0;
    int frames = -1;

    txb = asset_read_whole("efcommon.txb", &size);
    CHECK(txb != NULL);
    if (txb == NULL)
        return;

    CHECK(lbpTexLoadBank(0, "efcommon") == 0);

    n = *(u32 *)txb;
    CHECK(n == 47);

    for (i = 0; i < n; i++)
    {
        u32 count, fmt, siz, w, h, f;

        off = ((u32 *)txb)[1 + i];
        count = *(u32 *)(txb + off);
        fmt = *(u32 *)(txb + off + 4);
        siz = *(u32 *)(txb + off + 8);
        w = *(u32 *)(txb + off + 12);
        h = *(u32 *)(txb + off + 16);
        (void)fmt;
        (void)siz;

        /* one frame per frame, none beyond, and each one the .txb's own
         * size with the PVR's minimum and power of two applied */
        CHECK(lbpTexGet(0, (int)i, (int)count) == NULL);

        for (f = 0; f < count; f++)
        {
            const LBPTex *t = lbpTexGet(0, (int)i, (int)f);

            CHECK(t != NULL);
            if (t == NULL)
                continue;
            CHECK(t->src_w == w && t->src_h == h);
            CHECK(t->w >= 8 && t->h >= 8);
            CHECK(t->w >= t->src_w && t->h >= t->src_h);
            CHECK((t->w & (t->w - 1)) == 0 && (t->h & (t->h - 1)) == 0);
            want_bytes += t->w * t->h * 2;
            frames_total++;
        }
    }
    CHECK(frames_total == 202);
    CHECK(lbpTexBytes(&frames) == want_bytes);

    /* Each texture keeps the format the exporter chose for it, and 33 of
     * the 47 need ARGB4444's graded alpha -- 15 among them, the smoke
     * ftCommonDamageUpdateDustEffect trails behind a launched fighter
     * (script 0x57). The host stores the file's 1 or 2 where the target
     * stores KOS's (2 << 27); a field too narrow for the latter once
     * turned every one of the 33 into ARGB1555, and lbpartex.c now
     * refuses to compile that. */
    {
        u32 argb4444 = 0;

        for (i = 0; i < n; i++)
        {
            const LBPTex *t = lbpTexGet(0, (int)i, 0);

            if (t != NULL && t->fmt == 2)
                argb4444++;
        }
        CHECK(argb4444 == 33);
        CHECK(lbpTexGet(0, 15, 0) != NULL && lbpTexGet(0, 15, 0)->fmt == 2);
    }
    CHECK(frames == (int)frames_total);

    /* A bank that is not there is not there, and asking twice for the one
     * that is costs nothing -- ef/efparticle.c's cache asks twice. */
    CHECK(lbpTexGet(1, 0, 0) == NULL);
    CHECK(lbpTexLoadBank(0, "efcommon") == 0);
    CHECK(lbpTexBytes(NULL) == want_bytes);
    /* And a load that cannot happen leaves the bank that is there. */
    CHECK(lbpTexLoadBank(0, "no_such_bank") != 0);
    CHECK(lbpTexGet(0, 0, 0) != NULL);
    CHECK(lbpTexBytes(NULL) == want_bytes);

    free(txb);
    lbpTexFreeAll();

    /* ---- The two per-fighter banks ----
     *
     * Kirby's is particles_unk0 and Yoshi's is particles_unk2, and both
     * must be loaded: gFTDataKirbyParticleBankID and
     * gFTDataYoshiParticleBankID would otherwise be bss-zero, and bank 0 is efcommon's,
     * so the two makers that read them were stubbed rather than called
     * -- an unloaded id does not fail, it resolves to the common bank
     * and draws the wrong effect.
     *
     * What the host can say is the shape, on its own side of the
     * loader: each bank's .txp reads, each carries its own frames, and
     * loading one does not disturb the other. Whether the SCRIPT half
     * walks is the target's to say (the pointer arrays are 32-bit and
     * this build is not -- see efManagerLoadEffectBank), and
     * -DDB_BANK_PROBE says it: each fighter's particle comes back
     * carrying its own bank id. ---- */
    {
        static const char *const kBanks[2] = { "particles_unk0",
                                               "particles_unk2" };
        /* unk0 is three textures over five frames, unk2 two over
         * three -- next to efcommon's forty-seven over two hundred and
         * two, which is why a fighter's bank costs almost nothing */
        static const u32 kTextures[2] = { 3, 2 };
        static const u32 kFrames[2] = { 5, 3 };
        u32 b, total[2];

        for (b = 0; b < 2; b++)
        {
            u8 *tb = asset_read_whole(
                (b == 0) ? "particles_unk0.txb" : "particles_unk2.txb",
                &size);

            CHECK(tb != NULL);
            if (tb == NULL)
                continue;
            CHECK(*(u32 *)tb == kTextures[b]);

            /* loaded into two different slots, so the second cannot be
             * the first answering again */
            CHECK(lbpTexLoadBank((int)b + 1, kBanks[b]) == 0);
            total[b] = 0;
            for (i = 0; i < kTextures[b]; i++)
            {
                u32 count = *(u32 *)(tb + ((u32 *)tb)[1 + i]);
                u32 f;

                CHECK(lbpTexGet((int)b + 1, (int)i, (int)count) == NULL);
                for (f = 0; f < count; f++)
                {
                    const LBPTex *t = lbpTexGet((int)b + 1, (int)i, (int)f);

                    CHECK(t != NULL);
                    if (t == NULL)
                        continue;
                    CHECK(t->w >= 8 && t->h >= 8);
                    CHECK(t->w >= t->src_w && t->h >= t->src_h);
                    total[b]++;
                }
            }
            CHECK(total[b] == kFrames[b]);
            free(tb);
        }
        /* both still there, and each still its own */
        CHECK(lbpTexGet(1, 0, 0) != NULL);
        CHECK(lbpTexGet(2, 0, 0) != NULL);
        CHECK(lbpTexGet(1, 0, 0) != lbpTexGet(2, 0, 0));
        lbpTexFreeAll();
    }
}


/* src/dc/lbpdraw.c, the RDP lb/lbparticle.c's renderer draws through.
 *
 * The renderer itself is the decomp's line for line and every macro
 * argument it passes is checked against the display list the decomp's
 * own build emits (tools/check/lbparticle_check.py's draw leg). What that
 * cannot reach is the arithmetic the port added underneath: turning
 * gDPSetPrimDepth's screen Z back into the 1/w the PVR sorts by. There
 * is no second implementation of that anywhere -- it is the port's
 * answer to two encodings of one depth -- so it is checked here, against
 * the projection it claims to invert.
 */
/* ef/efmanager.c's spine, on the host: the EFStruct pool and what an
 * effect does with a struct it cannot use.
 *
 * These are the file's own variables, at the game's own names --
 * efmanager.c:1717,1720 declares both without `static` and the port's
 * copy keeps that -- so the pool can be built here rather than through
 * efManagerInitEffects, which would call efDisplayInitAll and try to
 * read the common effect bank off the medium. This build cannot load
 * that bank (see scVSBattleLoadEffectBank), which is exactly why the
 * unwind below is the path worth testing here. */
extern EFStruct *sEFManagerStructsAllocFree;
extern s32 sEFManagerStructsFreeNum;

/* efmanager.c:1648. ef/efmanager.h declares none of the fifty-three
 * EFDescs -- the game reaches each one only from the maker beside it --
 * so the one the port carries is named here to test the model path
 * without going through ftCommonRebirthDownSetStatus. */
extern EFDesc dEFManagerRebirthHaloEffectDesc;
extern EFDesc dEFManagerDamageFlyOrbsEffectDesc;
extern EFDesc dEFManagerDamageSpawnOrbsEffectDesc;
extern EFDesc dEFManagerDamageSlashEffectDesc;
extern EFDesc dEFManagerDamageFlySparksEffectDesc;
extern EFDesc dEFManagerDamageSpawnSparksEffectDesc;
extern EFDesc dEFManagerStarRodSparkEffectDesc;
extern EFDesc dEFManagerDamageFlyMDustEffectDesc;
extern EFDesc dEFManagerDamageSpawnMDustEffectDesc;
extern EFDesc dEFManagerDeadExplodeEffectDesc;
extern EFDesc dEFManagerShockSmallEffectDesc;
/* Neither table is declared in ef/efmanager.h -- both are file-scope
 * data the game's own procs index and nothing else names -- so the test
 * names them here to say the two really are two (efmanager.c:14-18). */
extern f32 dEFManagerDamageSpawnSparksAngles[];
extern f32 dEFManagerDamageSpawnMDustAngles[];
/* The dead explosion's own file-scope data, for the same reason
 * (efmanager.c:57-75, 879-889). */
extern u8 dEFManagerDeadExplodeEnvColorSiblingR[];
extern u8 dEFManagerDeadExplodeEnvColorSiblingG[];
extern u8 dEFManagerDeadExplodeEnvColorSiblingB[];
extern u8 dEFManagerDeadExplodeEnvColorChildR[];
extern u8 dEFManagerDeadExplodeEnvColorChildG[];
extern u8 dEFManagerDeadExplodeEnvColorChildB[];
extern f32 dEFManagerDeadExplodeRotateD[];
extern intptr_t dEFManagerDeadExplodeMatAnimJoints[];

static EFStruct ef_pool[EFFECT_ALLOC_NUM];

/* The flying orb a spawner's update just made. It goes on the effect
 * link like every other effect GObj and gcMakeGObjSPAfter decides where
 * in the list, so this asks by what it is rather than by where: the only
 * GObj on the link whose update proc is the flyer's. */
static GObj *ef_find_fly_orb(void)
{
    GObj *gobj = gGCCommonLinks[nGCCommonLinkIDEffect];

    while (gobj != NULL)
    {
        EFStruct *ep = efGetStruct(gobj);

        if (ep != NULL && ep->proc_update == efManagerDamageFlyOrbsProcUpdate)
        {
            return gobj;
        }
        gobj = gobj->link_next;
    }
    return NULL;
}

/* The same, for the spark spawner's flyer -- and for the metal dust's,
 * because dEFManagerDamageFlyMDustEffectDesc names the very same
 * proc_update (efmanager.c:340). Each test below ejects every flyer it
 * inspects before making the next, so there is never more than one. */
static GObj *ef_find_fly_spark(void)
{
    GObj *gobj = gGCCommonLinks[nGCCommonLinkIDEffect];

    while (gobj != NULL)
    {
        EFStruct *ep = efGetStruct(gobj);

        if (ep != NULL && ep->proc_update == efManagerDamageFlySparksProcUpdate)
        {
            return gobj;
        }
        gobj = gobj->link_next;
    }
    return NULL;
}

/* efmanager.c:1740-1749, without the malloc. */
static void ef_pool_reset(void)
{
    s32 i;

    memset(ef_pool, 0xA5, sizeof(ef_pool));

    for (i = 0; i < (EFFECT_ALLOC_NUM - 1); i++)
    {
        ef_pool[i].next = &ef_pool[i + 1];
    }
    ef_pool[EFFECT_ALLOC_NUM - 1].next = NULL;

    sEFManagerStructsAllocFree = &ef_pool[0];
    sEFManagerStructsFreeNum = EFFECT_ALLOC_NUM;
}

/* ---- matrix kinds 0x44 and 0x45, against lb/lbcommon.c ---------------
 *
 * The two hand-built DObjs an effect with flags 0x1 gets carry whatever
 * matrix kinds its EFDesc's transform triples name, and the damage
 * sparks' child asks for 0x45 while the metal dust's asks for 0x44
 * (ef/efmanager.c:279 and 339). Both are >= 66, which on the N64 means
 * they are not gcPrepDObjMatrix's own switch at all: sGCMatrixFuncList
 * dispatches them, indexed by kind - 66, and a VS battle installs
 * dLBCommonFuncMatrixList there (sc/scvsbattle.c:54). Entry 2 of it is
 * func_ovl0_800CA024 and entry 3 is lbCommonRotScaFuncMatrix.
 *
 * The port has no such list -- src/dc/objdisplay.c dispatches the few
 * kinds an effect uses beside the ordinary ones -- so what has to be
 * checked is that its two cases compute what those two functions
 * compute. Both decomp functions write packed RSP fixed point straight
 * into an Mtx, so they are copied here verbatim and their answers
 * converted back to float; the port's are read out of
 * gcDObjLocalMatrixF. The port is column-vector and the RSP is
 * row-vector, so the comparison is against the transpose, which is the
 * half of this that a reading of the code alone would get wrong.
 */
extern u16 gSYSinTable[0x800];

/* lb/lbcommon.c:1639 func_ovl0_800CA024, verbatim but for the unused
 * `dls` argument and the name. */
static void lbc_800CA024(Mtx *mtx, DObj *dobj)
{
    s32 e1, e2;

    e1 = FTOFIX32(dobj->scale.vec.f.x);
    e2 = 0;

    mtx->m[0][0] = (e1 & 0xffff0000) | ((e2 >> 16) & 0xffff);
    mtx->m[2][0] = ((e1 << 16) & 0xffff0000) | (e2 & 0xffff);

    mtx->m[0][1] = 0;
    mtx->m[2][1] = 0;

    e1 = 0;
    e2 = FTOFIX32(dobj->scale.vec.f.y);

    mtx->m[0][2] = (e1 & 0xffff0000) | ((e2 >> 16) & 0xffff);
    mtx->m[2][2] = ((e1 << 16) & 0xffff0000) | (e2 & 0xffff);

    mtx->m[0][3] = 0;
    mtx->m[2][3] = 0;

    mtx->m[1][0] = 0;
    mtx->m[3][0] = 0;

    e1 = FTOFIX32(dobj->scale.vec.f.z);
    e2 = 0;

    mtx->m[1][1] = (e1 & 0xffff0000) | ((e2 >> 16) & 0xffff);
    mtx->m[3][1] = ((e1 << 16) & 0xffff0000) | (e2 & 0xffff);

    e1 = FTOFIX32(dobj->translate.vec.f.x);
    e2 = FTOFIX32(dobj->translate.vec.f.y);

    mtx->m[1][2] = (e1 & 0xffff0000) | ((e2 >> 16) & 0xffff);
    mtx->m[3][2] = ((e1 << 16) & 0xffff0000) | (e2 & 0xffff);

    e1 = FTOFIX32(dobj->translate.vec.f.z);
    e2 = 0x10000;

    mtx->m[1][3] = (e1 & 0xffff0000) | ((e2 >> 16) & 0xffff);
    mtx->m[3][3] = ((e1 << 16) & 0xffff0000) | (e2 & 0xffff);
}

/* lb/lbcommon.c:618 lbCommonMatrixRotSca, verbatim. */
static void lbc_MatrixRotSca(Mtx *mtx, f32 rotx, f32 roty, f32 rotz,
                             f32 scax, f32 scay, f32 scaz)
{
    u32 e1, e2;
    s32 sinx, cosx;
    s32 siny, cosy;
    s32 sinz, cosz;
    s32 scay_l, scax_l, scaz_l;
    u16 idx, idy, idz;

    idx = ((s32) (rotx * 651.8986206F)) & 0xFFF;
    sinx = gSYSinTable[idx & 0x7FF];
    if (idx & 0x800) { sinx = -sinx; }
    idx += 0x400;
    cosx = gSYSinTable[idx & 0x7FF];
    if (idx & 0x800) { cosx = -cosx; }

    idy = ((s32) (roty * 651.8986206F)) & 0xFFF;
    siny = gSYSinTable[idy & 0x7FF];
    if (idy & 0x800) { siny = -siny; }
    idy += 0x400;
    cosy = gSYSinTable[idy & 0x7FF];
    if (idy & 0x800) { cosy = -cosy; }

    idz = ((s32) (rotz * 651.8986206F)) & 0xFFF;
    sinz = gSYSinTable[idz & 0x7FF];
    if (idz & 0x800) { sinz = -sinz; }
    idz += 0x400;
    cosz = gSYSinTable[idz & 0x7FF];
    if (idz & 0x800) { cosz = -cosz; }

    scax_l = (scax * 256.0F);
    scay_l = (scay * 256.0F);
    scaz_l = (scaz * 256.0F);

    e1 = ((((cosy * cosz) >> 14) * scax_l) >> 8);
    e2 = ((((cosy * sinz) >> 14) * scax_l) >> 8);
    mtx->m[0][0] = COMBINE_INTEGRAL(e1, e2);
    mtx->m[2][0] = COMBINE_FRACTIONAL(e1, e2);

    e1 = ((-siny * scax_l) >> 7);
    mtx->m[0][1] = COMBINE_INTEGRAL(e1, 0);
    mtx->m[2][1] = COMBINE_FRACTIONAL(e1, 0);

    e1 = (((((((sinx * siny) >> 15) * cosz) >> 14) - ((cosx * sinz) >> 14)) * scay_l) >> 8);
    e2 = (((((((sinx * siny) >> 15) * sinz) >> 14) + ((cosx * cosz) >> 14)) * scay_l) >> 8);
    mtx->m[0][2] = COMBINE_INTEGRAL(e1, e2);
    mtx->m[2][2] = COMBINE_FRACTIONAL(e1, e2);

    e1 = ((((sinx * cosy) >> 14) * scay_l) >> 8);
    mtx->m[0][3] = COMBINE_INTEGRAL(e1, 0);
    mtx->m[2][3] = COMBINE_FRACTIONAL(e1, 0);

    e1 = (((((((cosx * siny) >> 15) * cosz) >> 14) + ((sinx * sinz) >> 14)) * scaz_l) >> 8);
    e2 = (((((((cosx * siny) >> 15) * sinz) >> 14) - ((sinx * cosz) >> 14)) * scaz_l) >> 8);
    mtx->m[1][0] = COMBINE_INTEGRAL(e1, e2);
    mtx->m[3][0] = COMBINE_FRACTIONAL(e1, e2);

    e1 = ((((cosx * cosy) >> 14) * scaz_l) >> 8);
    mtx->m[1][1] = COMBINE_INTEGRAL(e1, 0);
    mtx->m[3][1] = COMBINE_FRACTIONAL(e1, 0);

    mtx->m[1][2] = COMBINE_INTEGRAL(0, 0);
    mtx->m[3][2] = COMBINE_FRACTIONAL(0, 0);
    mtx->m[1][3] = COMBINE_INTEGRAL(0, 0x10000);
    mtx->m[3][3] = COMBINE_FRACTIONAL(0, 0);
}

/* guMtxL2F for one element: an Mtx is sixteen words, the first eight
 * holding the sixteen integer halves and the last eight the fractional
 * ones, two elements to a word in the RSP's row order.
 *
 * `Mtx_t` is `long[4][4]` (PR/gbi.h:1164) and this test builds for the
 * host, where a long is eight bytes and not the N64's four -- so the
 * words are read as long and cut back to the 32 bits the RSP would have
 * seen. Getting that wrong is silent: every element but one comes back
 * zero, which is how it was found. */
static float lbc_elem(const Mtx *mtx, int idx)
{
    const long *w = (const long *)mtx;
    u32 hi = (u32)w[idx >> 1];
    u32 lo = (u32)w[8 + (idx >> 1)];
    s32 v;

    if (idx & 1)
    {
        v = (s32)((hi << 16) | (lo & 0xFFFF));
    }
    else
    {
        v = (s32)((hi & 0xFFFF0000) | ((lo >> 16) & 0xFFFF));
    }
    return (float)v / 65536.0F;
}

/* lb/lbcommon.c:491-607 lbCommonMatrixTraRotScaInv, verbatim but for the
 * name: the matrix kind 0x4B draws a fighter joint with while its motion
 * sets is_use_animlocks. */
static void lbc_MatrixTraRotScaInv(Mtx *mtx, f32 trax, f32 tray, f32 traz,
                                   f32 rotx, f32 roty, f32 rotz,
                                   f32 scax_inv, f32 scay_inv, f32 scaz_inv,
                                   f32 scax, f32 scay, f32 scaz)
{
    u32 e1, e2;
    s32 sinx, cosx;
    s32 siny, cosy;
    s32 sinz, cosz;
    s32 scay_l, scax_l, scaz_l;
    s32 scax_inv_l, scay_inv_l, scaz_inv_l;
    u16 idx, idy, idz;

    idx = ((s32) (rotx * 651.8986206F)) & 0xFFF;
    sinx = gSYSinTable[idx & 0x7FF];
    if (idx & 0x800) { sinx = -sinx; }
    idx += 0x400;
    cosx = gSYSinTable[idx & 0x7FF];
    if (idx & 0x800) { cosx = -cosx; }

    idy = ((s32) (roty * 651.8986206F)) & 0xFFF;
    siny = gSYSinTable[idy & 0x7FF];
    if (idy & 0x800) { siny = -siny; }
    idy += 0x400;
    cosy = gSYSinTable[idy & 0x7FF];
    if (idy & 0x800) { cosy = -cosy; }

    idz = ((s32) (rotz * 651.8986206F)) & 0xFFF;
    sinz = gSYSinTable[idz & 0x7FF];
    if (idz & 0x800) { sinz = -sinz; }
    idz += 0x400;
    cosz = gSYSinTable[idz & 0x7FF];
    if (idz & 0x800) { cosz = -cosz; }

    scax_l = (scax * 256.0F);
    scay_l = (scay * 256.0F);
    scaz_l = (scaz * 256.0F);

    scax_inv_l = ((1.0F / scax_inv) * 256.0F);
    scay_inv_l = ((1.0F / scay_inv) * 256.0F);
    scaz_inv_l = ((1.0F / scaz_inv) * 256.0F);

    e1 = (((((cosy * cosz) >> 14) * scax_l) >> 8) * scax_inv_l) >> 8;
    e2 = (((((cosy * sinz) >> 14) * scax_l) >> 8) * scay_inv_l) >> 8;
    mtx->m[0][0] = COMBINE_INTEGRAL(e1, e2);
    mtx->m[2][0] = COMBINE_FRACTIONAL(e1, e2);

    e1 = (((-siny * scax_l) >> 7) * scaz_inv_l) >> 8;
    mtx->m[0][1] = COMBINE_INTEGRAL(e1, 0);
    mtx->m[2][1] = COMBINE_FRACTIONAL(e1, 0);

    e1 = ((((((((sinx * siny) >> 15) * cosz) >> 14) - ((cosx * sinz) >> 14)) * scay_l) >> 8) * scax_inv_l) >> 8;
    e2 = ((((((((sinx * siny) >> 15) * sinz) >> 14) + ((cosx * cosz) >> 14)) * scay_l) >> 8) * scay_inv_l) >> 8;
    mtx->m[0][2] = COMBINE_INTEGRAL(e1, e2);
    mtx->m[2][2] = COMBINE_FRACTIONAL(e1, e2);

    e1 = (((((sinx * cosy) >> 14) * scay_l) >> 8) * scaz_inv_l) >> 8;
    mtx->m[0][3] = COMBINE_INTEGRAL(e1, 0);
    mtx->m[2][3] = COMBINE_FRACTIONAL(e1, 0);

    e1 = ((((((((cosx * siny) >> 15) * cosz) >> 14) + ((sinx * sinz) >> 14)) * scaz_l) >> 8) * scax_inv_l) >> 8;
    e2 = ((((((((cosx * siny) >> 15) * sinz) >> 14) - ((sinx * cosz) >> 14)) * scaz_l) >> 8) * scay_inv_l) >> 8;
    mtx->m[1][0] = COMBINE_INTEGRAL(e1, e2);
    mtx->m[3][0] = COMBINE_FRACTIONAL(e1, e2);

    e1 = (((((cosx * cosy) >> 14) * scaz_l) >> 8) * scaz_inv_l) >> 8;
    mtx->m[1][1] = COMBINE_INTEGRAL(e1, 0);
    mtx->m[3][1] = COMBINE_FRACTIONAL(e1, 0);

    e1 = (s32) (trax * 65536.0F);
    e2 = (s32) (tray * 65536.0F);
    mtx->m[1][2] = COMBINE_INTEGRAL(e1, e2);
    mtx->m[3][2] = COMBINE_FRACTIONAL(e1, e2);

    e1 = (s32) (traz * 65536.0F);
    mtx->m[1][3] = COMBINE_INTEGRAL(e1, 0x10000);
    mtx->m[3][3] = COMBINE_FRACTIONAL(e1, 0);
}

extern Vec3f gLBCommonScale;

static void test_matrix_kinds(void)
{
    static const struct { f32 t[3], r[3], s[3]; } kCase[] = {
        { {   0.0F,   0.0F,   0.0F }, { 0.0F, 0.0F, 0.0F },
          {   1.0F,   1.0F,   1.0F } },
        { {  10.0F, -20.0F,  30.0F }, { 0.3F, -0.7F, 1.9F },
          {   2.0F,   0.5F,   1.5F } },
        { { -400.0F, 700.0F, -3.5F }, { -2.4F, 0.9F, 0.1F },
          {   0.25F,  3.0F,   1.0F } },
    };
    u32 c;

    for (c = 0; c < ARRAY_COUNT(kCase); c++)
    {
        static DObj d;
        Mtx want;
        float got[16];
        int i, j;

        memset(&d, 0, sizeof(d));
        d.translate.vec.f.x = kCase[c].t[0];
        d.translate.vec.f.y = kCase[c].t[1];
        d.translate.vec.f.z = kCase[c].t[2];
        d.rotate.vec.f.x = kCase[c].r[0];
        d.rotate.vec.f.y = kCase[c].r[1];
        d.rotate.vec.f.z = kCase[c].r[2];
        d.scale.vec.f.x = kCase[c].s[0];
        d.scale.vec.f.y = kCase[c].s[1];
        d.scale.vec.f.z = kCase[c].s[2];

        /* 0x44: the metal dust's child. Exact but for the
         * decomp's truncation to 1/65536. */
        memset(&want, 0, sizeof(want));
        lbc_800CA024(&want, &d);
        CHECK(gcDObjLocalMatrixF(got, &d, 0x44) == TRUE);

        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 4; j++)
            {
                CHECK_NEAR(got[i * 4 + j], lbc_elem(&want, j * 4 + i),
                           1.0F / 16384.0F);
            }
        }

        /* 0x45: the damage sparks' child. The decomp does
         * this one in fixed point off a 2048-entry sine table and throws
         * bits away at four shifts, so the tolerance is the table's and
         * not the float's. */
        memset(&want, 0, sizeof(want));
        lbc_MatrixRotSca(&want, kCase[c].r[0], kCase[c].r[1], kCase[c].r[2],
                         kCase[c].s[0], kCase[c].s[1], kCase[c].s[2]);
        CHECK(gcDObjLocalMatrixF(got, &d, 0x45) == TRUE);

        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 4; j++)
            {
                CHECK_NEAR(got[i * 4 + j], lbc_elem(&want, j * 4 + i), 0.01F);
            }
        }

        /* 0x4B, a fighter joint, three
         * ways: plainly, where it is TraRotRpyRSca -- the RotScaInv with
         * no parent scale to divide out -- then under is_use_animlocks
         * with a parent scale to divide out, which must also leave this
         * joint's product as the running scale; and locked, where it is
         * whatever gm/gmcollision.c left in unk_dobjtrans_0x10. */
        {
            static FTParts parts;
            sb32 saved_flag = fp.is_use_animlocks;
            Vec3f parent = { 2.0F, 0.5F, 1.25F };
            f32 tol;

            memset(&parts, 0, sizeof(parts));
            d.parent_gobj = mock_gobj;
            d.user_data.p = &parts;

            fp.is_use_animlocks = FALSE;
            gLBCommonScale = parent;
            memset(&want, 0, sizeof(want));
            lbc_MatrixTraRotScaInv(&want, kCase[c].t[0], kCase[c].t[1], kCase[c].t[2],
                                   kCase[c].r[0], kCase[c].r[1], kCase[c].r[2],
                                   1.0F, 1.0F, 1.0F,
                                   kCase[c].s[0], kCase[c].s[1], kCase[c].s[2]);
            CHECK(gcDObjLocalMatrixF(got, &d, 0x4B) == TRUE);
            CHECK(gLBCommonScale.x == parent.x && gLBCommonScale.z == parent.z);

            for (i = 0; i < 4; i++)
            {
                for (j = 0; j < 4; j++)
                {
                    CHECK_NEAR(got[i * 4 + j], lbc_elem(&want, j * 4 + i), 0.01F);
                }
            }

            fp.is_use_animlocks = TRUE;
            gLBCommonScale = parent;
            memset(&want, 0, sizeof(want));
            lbc_MatrixTraRotScaInv(&want, kCase[c].t[0], kCase[c].t[1], kCase[c].t[2],
                                   kCase[c].r[0], kCase[c].r[1], kCase[c].r[2],
                                   parent.x, parent.y, parent.z,
                                   kCase[c].s[0] * parent.x,
                                   kCase[c].s[1] * parent.y,
                                   kCase[c].s[2] * parent.z);
            CHECK(gcDObjLocalMatrixF(got, &d, 0x4B) == TRUE);
            CHECK_NEAR(gLBCommonScale.x, kCase[c].s[0] * parent.x, 1e-6F);
            CHECK_NEAR(gLBCommonScale.y, kCase[c].s[1] * parent.y, 1e-6F);
            CHECK_NEAR(gLBCommonScale.z, kCase[c].s[2] * parent.z, 1e-6F);

            /* the fixed point keeps 8 bits of each scale and of each
             * inverse, and truncates twice, so its error is a fraction of
             * the element: 0.6% at worst over these cases */
            for (i = 0; i < 4; i++)
            {
                for (j = 0; j < 4; j++)
                {
                    tol = 0.01F + 0.01F * fabsf(lbc_elem(&want, j * 4 + i));
                    CHECK_NEAR(got[i * 4 + j], lbc_elem(&want, j * 4 + i), tol);
                }
            }

            fp.is_use_animlocks = FALSE;
            parts.transform_update_mode = 3;
            for (i = 0; i < 4; i++)
            {
                for (j = 0; j < 4; j++)
                {
                    parts.unk_dobjtrans_0x10[i][j] = (f32)(i * 4 + j + 1) * ((j == 3) ? 0.0F : 1.0F);
                }
            }
            parts.unk_dobjtrans_0x10[3][3] = 1.0F;
            CHECK(gcDObjLocalMatrixF(got, &d, 0x4B) == TRUE);
            for (i = 0; i < 4; i++)
            {
                for (j = 0; j < 3; j++)
                {
                    CHECK(got[j * 4 + i] == parts.unk_dobjtrans_0x10[i][j]);
                }
            }
            CHECK(got[12] == 0.0F && got[13] == 0.0F && got[14] == 0.0F && got[15] == 1.0F);

            fp.is_use_animlocks = saved_flag;
            gLBCommonScale.x = gLBCommonScale.y = gLBCommonScale.z = 1.0F;
            d.parent_gobj = NULL;
            d.user_data.p = NULL;
        }
    }

    /* 44 (0x2C), the RSP billboard the shield bubble and Yoshi's egg
     * are drawn with. objdisplay.c:822-856 rewrites the
     * MVP's first three rows as the projection times
     * diag(sx * gGCScaleX, sy * gGCScaleX_before, sx * gGCScaleX) and
     * keeps its translation, so the property to hold the port's world
     * matrix to is: view * m has exactly that diagonal for its rotation
     * block and view * at for its translation, for any view. */
    {
        static const struct { f32 eye[3], at[3], up[3]; } kView[] = {
            { { 0.0F, 0.0F, 1000.0F }, { 0.0F, 0.0F, 0.0F }, { 0.0F, 1.0F, 0.0F } },
            { { 400.0F, 300.0F, 1800.0F }, { 100.0F, 200.0F, 0.0F }, { 0.0F, 1.0F, 0.0F } },
            { { -900.0F, 2500.0F, -600.0F }, { 50.0F, -20.0F, 30.0F }, { 0.2F, 0.9F, 0.1F } },
        };
        u32 v;

        for (v = 0; v < ARRAY_COUNT(kView); v++)
        {
            float view[16], m[16], vm[16];
            Vec3f sc = { 2.0F, 0.5F, 3.0F };
            float at[3] = { 100.0F, -30.0F, 250.0F };
            f32 scale_x = 1.5F;
            int i, j, k;

            gcMtxLookAt(view, kView[v].eye[0], kView[v].eye[1], kView[v].eye[2],
                        kView[v].at[0], kView[v].at[1], kView[v].at[2],
                        kView[v].up[0], kView[v].up[1], kView[v].up[2]);
            gcMtxRecalcRotRpyRSca(m, at, view, &sc, &scale_x);
            CHECK_NEAR(scale_x, 3.0F, 1e-6F);          /* 1.5 * sx, kept */

            for (i = 0; i < 4; i++)
            {
                for (j = 0; j < 4; j++)
                {
                    vm[i * 4 + j] = 0.0F;
                    for (k = 0; k < 4; k++)
                    {
                        vm[i * 4 + j] += view[i * 4 + k] * m[k * 4 + j];
                    }
                }
            }
            for (i = 0; i < 3; i++)
            {
                float want_t = view[i * 4 + 3];

                for (j = 0; j < 3; j++)
                {
                    float want = (i != j) ? 0.0F : (i == 1) ? 0.5F * 1.5F : 3.0F;

                    CHECK_NEAR(vm[i * 4 + j], want, 1e-4F);
                    want_t += view[i * 4 + j] * at[j];
                }
                CHECK_NEAR(vm[i * 4 + 3], want_t, 1e-3F);
            }
            CHECK(m[12] == 0.0F && m[13] == 0.0F && m[14] == 0.0F && m[15] == 1.0F);
        }
    }

    /* 0x46, the same billboard with an in-plane spin and no scale
     * (lb/lbcommon.c:1702 func_ovl0_800CA194). It rewrites the MVP's
     * first three rows as row0 = P[0]*cos + P[1]*sin, row1 = -P[0]*sin +
     * P[1]*cos, row2 = P[2], and keeps the translation -- which, by the
     * same reading as kind 44 above, is the property `view * m` has to
     * have: Rz(theta) for its rotation block and view * at for its
     * translation, for any view. gGCScaleX must come back untouched,
     * since this kind neither reads nor folds it.
     *
     * The Motion-Sensor Bomb, the Master Ball, Saffron City's Onix and
     * Meowth, Marumine and two of efmanager.c's effects ask for it. */
    {
        static const struct { f32 eye[3], at[3], up[3]; } kView[] = {
            { { 0.0F, 0.0F, 1000.0F }, { 0.0F, 0.0F, 0.0F }, { 0.0F, 1.0F, 0.0F } },
            { { 400.0F, 300.0F, 1800.0F }, { 100.0F, 200.0F, 0.0F }, { 0.0F, 1.0F, 0.0F } },
            { { -900.0F, 2500.0F, -600.0F }, { 50.0F, -20.0F, 30.0F }, { 0.2F, 0.9F, 0.1F } },
        };
        static const f32 kRot[] = { 0.0F, 0.7F, -1.9F, 3.0F };
        u32 v, r;

        for (v = 0; v < ARRAY_COUNT(kView); v++)
        {
            for (r = 0; r < ARRAY_COUNT(kRot); r++)
            {
                float view[16], m[16], vm[16];
                float at[3] = { 100.0F, -30.0F, 250.0F };
                f32 c = cosf(kRot[r]), s = sinf(kRot[r]);
                int i, j, k;

                gcMtxLookAt(view, kView[v].eye[0], kView[v].eye[1],
                            kView[v].eye[2], kView[v].at[0], kView[v].at[1],
                            kView[v].at[2], kView[v].up[0], kView[v].up[1],
                            kView[v].up[2]);
                gcMtxRecalcRotZ(m, at, view, kRot[r]);

                for (i = 0; i < 4; i++)
                {
                    for (j = 0; j < 4; j++)
                    {
                        vm[i * 4 + j] = 0.0F;
                        for (k = 0; k < 4; k++)
                        {
                            vm[i * 4 + j] += view[i * 4 + k] * m[k * 4 + j];
                        }
                    }
                }
                for (i = 0; i < 3; i++)
                {
                    float want_t = view[i * 4 + 3];
                    float want_row[3];

                    want_row[0] = (i == 0) ? c : (i == 1) ? s : 0.0F;
                    want_row[1] = (i == 0) ? -s : (i == 1) ? c : 0.0F;
                    want_row[2] = (i == 2) ? 1.0F : 0.0F;

                    for (j = 0; j < 3; j++)
                    {
                        CHECK_NEAR(vm[i * 4 + j], want_row[j], 1e-4F);
                        want_t += view[i * 4 + j] * at[j];
                    }
                    CHECK_NEAR(vm[i * 4 + 3], want_t, 1e-3F);
                }
                CHECK(m[12] == 0.0F && m[13] == 0.0F && m[14] == 0.0F &&
                      m[15] == 1.0F);
            }
        }
    }

    /* 46 and 45, 0x47 and 0x48: the same billboard with
     * the decomp's rows turned and scaled. Each rewrites MVP row j as
     * sum_k R[j][k] * P[k], so `view * m` must come out as R's transpose
     * beside view * at, with gGCScaleX folded as kind 44 folds it for 45,
     * 46 and 0x48 and left alone for 0x47. R is written out here from
     * sys/objdisplay.c:858-944 and lb/lbcommon.c:1760-1911 directly. */
    {
        static const struct { f32 eye[3], at[3], up[3]; } kView2[] = {
            { { 0.0F, 0.0F, 1000.0F }, { 0.0F, 0.0F, 0.0F }, { 0.0F, 1.0F, 0.0F } },
            { { -900.0F, 2500.0F, -600.0F }, { 50.0F, -20.0F, 30.0F }, { 0.2F, 0.9F, 0.1F } },
        };
        static const f32 kAng[][2] = { { 0.0F, 0.0F }, { 0.7F, -1.2F }, { -2.5F, 2.9F } };
        const Vec3f sc = { 1.5F, 0.5F, 9.0F };
        u32 v, a, kind;

        for (v = 0; v < ARRAY_COUNT(kView2); v++)
        {
            for (a = 0; a < ARRAY_COUNT(kAng); a++)
            {
                for (kind = 0; kind < 3; kind++)
                {
                    float view[16], m[16], vm[16];
                    float at[3] = { -40.0F, 75.0F, 12.0F };
                    f32 rx = kAng[a][0], ry = kAng[a][1];
                    f32 scale_x = 2.0F, sx = 1.0F, sy = 1.0F;
                    f32 r[3][3];
                    int i2, j2, k2;

                    gcMtxLookAt(view, kView2[v].eye[0], kView2[v].eye[1],
                                kView2[v].eye[2], kView2[v].at[0],
                                kView2[v].at[1], kView2[v].at[2],
                                kView2[v].up[0], kView2[v].up[1],
                                kView2[v].up[2]);
                    if (kind == 0)
                    {
                        /* 46, by rx as its spin */
                        f32 c = cosf(rx), s = sinf(rx);

                        gcMtxRecalcRotSca(m, at, view, rx, &sc, &scale_x);
                        sx = 2.0F * sc.x;
                        sy = 2.0F * sc.y;
                        r[0][0] = sx * c;  r[0][1] = sy * s; r[0][2] = 0.0F;
                        r[1][0] = -sx * s; r[1][1] = sy * c; r[1][2] = 0.0F;
                        r[2][0] = 0.0F;    r[2][1] = 0.0F;   r[2][2] = sx;
                        CHECK_NEAR(scale_x, 3.0F, 1e-5F);
                    }
                    else
                    {
                        f32 sinx = sinf(rx), cosx = cosf(rx);
                        f32 siny = sinf(ry), cosy = cosf(ry);

                        if (kind == 1)
                        {
                            gcMtxRecalcRotRpy(m, at, view, rx, ry, NULL, NULL);
                        }
                        else
                        {
                            gcMtxRecalcRotRpy(m, at, view, rx, ry, &sc, &scale_x);
                            sx = 2.0F * sc.x;
                            sy = 2.0F * sc.y;
                        }
                        r[0][0] = cosy * sx;        r[0][1] = 0.0F;         r[0][2] = -siny * sx;
                        r[1][0] = sinx * siny * sy; r[1][1] = cosx * sy;    r[1][2] = sinx * cosy * sy;
                        r[2][0] = cosx * siny * sx; r[2][1] = -sinx * sx;   r[2][2] = cosx * cosy * sx;
                        CHECK_NEAR(scale_x, (kind == 1) ? 2.0F : 3.0F, 1e-5F);
                    }
                    for (i2 = 0; i2 < 4; i2++)
                    {
                        for (j2 = 0; j2 < 4; j2++)
                        {
                            vm[i2 * 4 + j2] = 0.0F;
                            for (k2 = 0; k2 < 4; k2++)
                            {
                                vm[i2 * 4 + j2] += view[i2 * 4 + k2] * m[k2 * 4 + j2];
                            }
                        }
                    }
                    for (i2 = 0; i2 < 3; i2++)
                    {
                        float want_t = view[i2 * 4 + 3];

                        for (j2 = 0; j2 < 3; j2++)
                        {
                            CHECK_NEAR(vm[i2 * 4 + j2], r[j2][i2], 1e-4F);
                            want_t += view[i2 * 4 + j2] * at[j2];
                        }
                        CHECK_NEAR(vm[i2 * 4 + 3], want_t, 1e-3F);
                    }
                    CHECK(m[12] == 0.0F && m[13] == 0.0F && m[14] == 0.0F &&
                          m[15] == 1.0F);
                }
            }
        }
    }

    /* 48, Yoshi's Island's clouds. Its
     * rows are sGCMatrixMod1F's, which the camera pass computes under
     * modes 2 and 3 (the battle camera's) as syMatrixLookAtF from
     * (0, eye.y, d) to (0, at.y, 0), d the eye's horizontal distance.
     * Written out from sys/matrix.c:119-168 with up (0, 1, 0), its rows
     * are (1, 0, 0), (0, d, eye.y - at.y) / len and (0, at.y - eye.y, d)
     * / len, scaled as 0x48 scales them; `view * m` must hold their
     * transpose beside view * at. A mode that leaves Mod1F alone (1, the
     * arrows camera) keeps the last one, and a camera straight above its
     * target collapses the cloud to nothing. */
    {
        static const struct { f32 eye[3], at[3]; } kCam[] = {
            { { 0.0F, 0.0F, 1000.0F }, { 0.0F, 0.0F, 0.0F } },
            { { 400.0F, 900.0F, 1800.0F }, { 100.0F, 200.0F, 0.0F } },
            { { -900.0F, -700.0F, -600.0F }, { 50.0F, -20.0F, 30.0F } },
        };
        const Vec3f sc = { 1.5F, 0.5F, 9.0F };
        XObj look_xobj;
        CObj cam;
        u32 v;

        memset(&look_xobj, 0, sizeof(look_xobj));
        memset(&cam, 0, sizeof(cam));
        look_xobj.kind = nGCMatrixKindLookAt;
        cam.xobjs[0] = &look_xobj;
        cam.xobjs_num = 1;
        gcSetCameraMatrixMode(3);

        for (v = 0; v <= ARRAY_COUNT(kCam); v++)
        {
            float view[16], m[16], vm[16];
            float at[3] = { -40.0F, 75.0F, 12.0F };
            f32 scale_x = 2.0F, sx = 3.0F, sy = 1.0F;
            f32 r[3][3];
            f32 d, dy, len;
            int i, j, k;
            u32 c = (v < ARRAY_COUNT(kCam)) ? v : 1;

            cam.vec.eye.x = kCam[c].eye[0];
            cam.vec.eye.y = kCam[c].eye[1];
            cam.vec.eye.z = kCam[c].eye[2];
            cam.vec.at.x = kCam[c].at[0];
            cam.vec.at.y = kCam[c].at[1];
            cam.vec.at.z = kCam[c].at[2];
            cam.vec.up.x = 0.0F;
            cam.vec.up.y = 1.0F;
            cam.vec.up.z = 0.0F;

            if (v == ARRAY_COUNT(kCam))
            {
                /* the arrows camera's mode, over a different camera: the
                 * last pass's Mod1F (kCam[2]'s) stays */
                gcSetCameraMatrixMode(1);
                c = 2;
            }
            gcPrepCameraMatrix(NULL, &cam);
            gcSetCameraMatrixMode(3);

            memcpy(view, gcGetViewF(), sizeof(view));
            gcMtxRecalcMod1(m, at, view, &sc, &scale_x);
            CHECK_NEAR(scale_x, 3.0F, 1e-5F);

            d = sqrtf(SQUARE(kCam[c].at[2] - kCam[c].eye[2]) +
                      SQUARE(kCam[c].at[0] - kCam[c].eye[0]));
            dy = kCam[c].eye[1] - kCam[c].at[1];
            len = sqrtf(SQUARE(d) + SQUARE(dy));
            r[0][0] = sx;   r[0][1] = 0.0F;             r[0][2] = 0.0F;
            r[1][0] = 0.0F; r[1][1] = sy * d / len;     r[1][2] = sy * dy / len;
            r[2][0] = 0.0F; r[2][1] = -sx * dy / len;   r[2][2] = sx * d / len;

            for (i = 0; i < 4; i++)
            {
                for (j = 0; j < 4; j++)
                {
                    vm[i * 4 + j] = 0.0F;
                    for (k = 0; k < 4; k++)
                    {
                        vm[i * 4 + j] += view[i * 4 + k] * m[k * 4 + j];
                    }
                }
            }
            for (i = 0; i < 3; i++)
            {
                float want_t = view[i * 4 + 3];

                for (j = 0; j < 3; j++)
                {
                    CHECK_NEAR(vm[i * 4 + j], r[j][i], 1e-4F);
                    want_t += view[i * 4 + j] * at[j];
                }
                CHECK_NEAR(vm[i * 4 + 3], want_t, 1e-3F);
            }
        }

        /* straight above: no horizontal distance, no cloud */
        {
            float view[16], m[16];
            float at[3] = { 0.0F, 0.0F, 0.0F };
            f32 scale_x = 1.0F;
            int i, j;

            cam.vec.eye.x = 5.0F;
            cam.vec.eye.y = 1000.0F;
            cam.vec.eye.z = 7.0F;
            cam.vec.at.x = 5.0F;
            cam.vec.at.y = 0.0F;
            cam.vec.at.z = 7.0F;
            cam.vec.up.x = 0.0F;
            cam.vec.up.y = 0.0F;
            cam.vec.up.z = 1.0F;
            gcPrepCameraMatrix(NULL, &cam);
            memcpy(view, gcGetViewF(), sizeof(view));
            gcMtxRecalcMod1(m, at, view, &sc, &scale_x);

            for (i = 0; i < 3; i++)
            {
                for (j = 0; j < 3; j++)
                {
                    CHECK(m[i * 4 + j] == 0.0F);
                }
            }
        }
        gcSetCameraMatrixMode(0);
    }
}

/* efManagerDeadExplodeMakeEffect, the burst a fighter leaves
 * when he is knocked off the screen and the last efManager* the port had
 * stubbed.
 *
 * Everything about it that is not another quad is here. It is the only
 * maker in the file that reaches past the first child -- the third
 * sibling's MObj takes the player's colour and the first one's takes the
 * darker inner colour -- and the only one that swaps the shared EFDesc's
 * MatAnimJoint before making the effect, which is one line of the
 * decomp's that the port keeps *and* one line beside it that the port
 * adds (the alternate index; see the maker's DIVERGES). It also takes no
 * EFStruct at all, because its flags name SPECIALLINK and 0x4 and not
 * USERDATA -- so a pool that lost one here would be losing it to an
 * effect that never asked.
 *
 * There is no pack in this build, so the tree is efManagerAddModel's
 * FT_HOSTTEST stand-in: a stand, three children, one MObj each. What
 * that stand-in exists for is exactly this test.
 *
 * The colours are not written down here. They are read out of the
 * decomp's own six tables, which tools/check/efmanager_check.py holds the
 * port's copy of to the character, so this checks that the right table
 * reaches the right MObj rather than re-typing twelve bytes.
 */
static void test_dead_explode(void)
{
    static const u32 kType[] = { 0, 1, 3 };   /* ftcommondead.c's three */
    s32 player;
    u32 t;

    ef_pool_reset();

    for (t = 0; t < ARRAY_COUNT(kType); t++)
    {
        for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
        {
            Vec3f pos = { 111.0F, 222.0F, 333.0F };
            GObj *gobj = efManagerDeadExplodeMakeEffect(&pos, player,
                                                        kType[t]);
            DObj *root, *first, *mid, *last;

            CHECK(gobj != NULL);
            if (gobj == NULL)
            {
                continue;
            }
            /* EFFECT_FLAG_SPECIALLINK, and no USERDATA: the special link
             * and not a struct (efmanager.c:850, 1946-1963). */
            CHECK(gobj->link_id == nGCCommonLinkIDSpecialEffect);
            CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
            CHECK(gobj->user_data.p == NULL);

            /* efmanager.c:4808, the line the port keeps: the shared
             * descriptor really is mutated, player by player. */
            CHECK(dEFManagerDeadExplodeEffectDesc.o_matanim_joint ==
                  dEFManagerDeadExplodeMatAnimJoints[player]);

            root = DObjGetStruct(gobj);
            CHECK(root != NULL);
            if (root == NULL)
            {
                gcEjectGObj(gobj);
                continue;
            }
            /* efmanager.c:4816-4821: the burst goes where the fighter
             * died, turned by which blast line took him. */
            CHECK_EQF(root->translate.vec.f.x, pos.x);
            CHECK_EQF(root->translate.vec.f.y, pos.y);
            CHECK_EQF(root->translate.vec.f.z, pos.z);
            CHECK_NEAR(root->rotate.vec.f.z,
                       F_CLC_DTOR32(dEFManagerDeadExplodeRotateD[kType[t]]),
                       1.0e-6F);

            /* transform_types1 and transform_types2 are both the fused
             * nGCMatrixKindTraRotRpyRSca, which the port spells out over
             * the three XObjs dc_model_add_dobjs already installed
             * (efManagerSetTransformTypes). */
            CHECK(root->xobjs[0]->kind == nGCMatrixKindTra);
            CHECK(root->xobjs[1]->kind == nGCMatrixKindRotRpyR);
            CHECK(root->xobjs[2]->kind == nGCMatrixKindSca);

            first = root->child;
            CHECK(first != NULL);
            mid = (first != NULL) ? first->sib_next : NULL;
            CHECK(mid != NULL);
            last = (mid != NULL) ? mid->sib_next : NULL;
            CHECK(last != NULL);
            if (last == NULL)
            {
                gcEjectGObj(gobj);
                continue;
            }
            CHECK(last->sib_next == NULL);
            CHECK(first->xobjs[0]->kind == nGCMatrixKindTra);
            CHECK(first->xobjs[1]->kind == nGCMatrixKindRotRpyR);
            CHECK(first->xobjs[2]->kind == nGCMatrixKindSca);

            /* efmanager.c:4825-4835. child gets the Child triple and
             * child->sib_next->sib_next the Sibling triple, and the one
             * in the middle is left with whatever its display list set
             * -- which is why getting the two the wrong way round, or
             * reaching one sibling instead of two, would draw the right
             * colours in the wrong places. */
            CHECK(first->mobj != NULL);
            CHECK(mid->mobj != NULL);
            CHECK(last->mobj != NULL);
            if (first->mobj == NULL || mid->mobj == NULL ||
                last->mobj == NULL)
            {
                gcEjectGObj(gobj);
                continue;
            }
            CHECK(first->mobj->sub.envcolor.s.r ==
                  dEFManagerDeadExplodeEnvColorChildR[player]);
            CHECK(first->mobj->sub.envcolor.s.g ==
                  dEFManagerDeadExplodeEnvColorChildG[player]);
            CHECK(first->mobj->sub.envcolor.s.b ==
                  dEFManagerDeadExplodeEnvColorChildB[player]);
            CHECK((first->mobj->sub.flags & MOBJ_FLAG_ENVCOLOR) != 0);

            CHECK(last->mobj->sub.envcolor.s.r ==
                  dEFManagerDeadExplodeEnvColorSiblingR[player]);
            CHECK(last->mobj->sub.envcolor.s.g ==
                  dEFManagerDeadExplodeEnvColorSiblingG[player]);
            CHECK(last->mobj->sub.envcolor.s.b ==
                  dEFManagerDeadExplodeEnvColorSiblingB[player]);
            CHECK((last->mobj->sub.flags & MOBJ_FLAG_ENVCOLOR) != 0);

            CHECK((mid->mobj->sub.flags & MOBJ_FLAG_ENVCOLOR) == 0);

            gcEjectGObj(gobj);
        }
    }
    /* And the pool is where it started: this maker takes no struct, so
     * twelve explosions have to leave all thirty-eight free. */
    CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
}

/* ---- the screen flash and the effects camera -----------
 *
 * if/ifscreenflash.c is a colour animation drawn as one quad, and the
 * interpreter it runs on -- ft/ftmain.c:959-1200 ftMainUpdateColAnim, in
 * the link -- is not otherwise exercised by a script on the port.
 * So this replays each of gm/gmcolscripts.c's five ScreenFlash scripts
 * tic by tic through the port's interpreter and holds every tic's colour
 * to a table worked from the decomp's text: SetColor1 writes the colour
 * and zeroes the steps, BlendColor1(n, c) divides (c - current) by n in
 * C's integer arithmetic (toward zero, so (0 - 108) / 30 is -3 and
 * (0 - 70) / 6 is -11), Wait(n) arms the timer, the colour steps once a
 * tic while is_use_color1 is up, and End returns TRUE when no other
 * script runs, on which ifScreenFlashProcUpdate resets. The DeadExplode
 * script clears the colour itself before its End; the four hit scripts
 * do not, and it is the reset that lowers is_use_color1. The scripts
 * are the decomp's own words (hosttest_gmcolscripts.c) and the tables
 * are what the decomp's rules make of them; the port's interpreter is
 * what is on trial.
 *
 * The rest is the shape: the interface GObj on DL link 22 behind the
 * is_allow_screenflash option, the priority rule (ftParamCheckSetColAnimID
 * refuses a lower priority and takes an equal one), a length that runs
 * out, the quad ifScreenFlashProcDisplay logs -- in the translucent pass
 * only, the game's 10..310 x 10..230 at four sub-pixels, the script's
 * colour with its alpha scaled by the interface's -- the hit thresholds
 * that ask for one, the two cameras with their links and priorities, and
 * the order the frame walks them in.
 */
extern GMColAnim sIFScreenFlashColAnim;
extern u8 sIFScreenFlashAlpha;

static const struct
{
    s32 id;
    u8 r, g, b;
    s32 tics;               /* tics before the End: rows in `a` */
    u8 a[36];
} kFlashScript[] =
{
    { nGMColAnimScreenFlashDeadExplode, 0xFF, 0xFF, 0xFF, 36,
      { 18, 36, 54, 72, 90, 108, 105, 102, 99, 96, 93, 90, 87, 84, 81,
        78, 75, 72, 69, 66, 63, 60, 57, 54, 51, 48, 45, 42, 39, 36, 33,
        30, 27, 24, 21, 18 } },
    { nGMColAnimScreenFlashDamageNormal, 0xFF, 0xFF, 0xFF, 6,
      { 59, 48, 37, 26, 15, 4 } },
    { nGMColAnimScreenFlashDamageFire, 0xFF, 0x8C, 0x78, 8,
      { 70, 60, 50, 40, 30, 20, 10, 0 } },
    { nGMColAnimScreenFlashDamageElectric, 0x8C, 0x8C, 0xFF, 8,
      { 70, 60, 50, 40, 30, 20, 10, 0 } },
    { nGMColAnimScreenFlashDamageIce, 0x00, 0x80, 0xFF, 6,
      { 117, 94, 71, 48, 25, 2 } },
};

static s32 flash_link_count(s32 link)
{
    GObj *g = gGCCommonLinks[link];
    s32 n = 0;

    while (g != NULL)
    {
        n++;
        g = g->link_next;
    }
    return n;
}

static GObj *flash_find_display(s32 link, void (*proc)(GObj*))
{
    GObj *g = gGCCommonLinks[link];

    while (g != NULL && g->proc_display != proc)
    {
        g = g->link_next;
    }
    return g;
}

/* the camera list: DL link 64, walked by gcDrawAll from its head */
static GObj *flash_camera_head(void)
{
    return gGCCommonDLLinks[ARRAY_COUNT(gGCCommonDLLinks) - 1];
}

static GObj *flash_find_camera(void (*proc)(GObj*))
{
    GObj *g = flash_camera_head();

    while (g != NULL && g->proc_display != proc)
    {
        g = g->dl_link_next;
    }
    return g;
}

static s32 gFlashProcCalls;
static GObj *gFlashProcLast;

static void flash_count_proc(GObj *gobj)
{
    gFlashProcCalls++;
    gFlashProcLast = gobj;
}

/* gcPlayMObjMatAnim's linear colour track. The decomp's
 * lerp (objanim.c:1351-1388) is byte arithmetic on a big-endian word and
 * src/dc/objanim.c stands in front of it; the host is little-endian as
 * the SH4 is, so before the fix this read 60 00 80 00 where the N64 has
 * 50 60 70 80 -- the opening Room's white spotlight, drawn magenta. A
 * pack is the WORD 0xRRGGBBAA everywhere in the port (src/dc/objmodel.c,
 * src/dc/fighter.c material_of). */
static void matanim_color_bits(f32 *slot, u32 bits)
{
    memcpy(slot, &bits, sizeof(bits));
}

static void test_matanim_color_lerp(void)
{
    static const u8 tracks[] =
    {
        nGCAnimTrackPrimColor, nGCAnimTrackEnvColor, nGCAnimTrackBlendColor,
        nGCAnimTrackLight1Color, nGCAnimTrackLight2Color
    };
    MObj mobj;
    AObj aobj;
    u32 k;

    for (k = 0; k < ARRAY_COUNT(tracks); k++)
    {
        SYColorPack *got;

        memset(&mobj, 0, sizeof(mobj));
        memset(&aobj, 0, sizeof(aobj));
        mobj.aobj = &aobj;
        mobj.anim_wait = 8.0F;
        mobj.anim_speed = 1.0F;
        aobj.track = tracks[k];
        aobj.kind = nGCAnimKindLinear;
        aobj.length_invert = 1.0F / 8.0F;
        aobj.length = -1.0F;
        matanim_color_bits(&aobj.value_base, 0x10203040);
        matanim_color_bits(&aobj.value_target, 0x90A0B0C0);

        got = (k == 0) ? &mobj.sub.primcolor :
              (k == 1) ? &mobj.sub.envcolor :
              (k == 2) ? &mobj.sub.blendcolor :
              (k == 3) ? &mobj.sub.light1color : &mobj.sub.light2color;

        /* the first frame: length 0, the base colour exactly */
        gcPlayMObjMatAnim(&mobj);
        CHECK(got->pack == 0x10203040);
        /* half way along, every channel half way */
        aobj.length = 3.0F;
        gcPlayMObjMatAnim(&mobj);
        CHECK(got->pack == 0x50607080);
        /* past the end the fraction clamps and the target holds */
        aobj.length = 20.0F;
        gcPlayMObjMatAnim(&mobj);
        CHECK(got->pack == 0x90A0B0C0);

        /* the step kind was always a word copy and must stay one */
        aobj.kind = nGCAnimKindStep;
        aobj.length_invert = 4.0F;
        aobj.length = 0.0F;
        gcPlayMObjMatAnim(&mobj);
        CHECK(got->pack == 0x10203040);
        aobj.length = 5.0F;
        gcPlayMObjMatAnim(&mobj);
        CHECK(got->pack == 0x90A0B0C0);
    }
    /* a script that is not running moves nothing */
    memset(&mobj, 0, sizeof(mobj));
    mobj.aobj = &aobj;
    mobj.anim_wait = AOBJ_ANIM_NULL;
    mobj.sub.primcolor.pack = 0x01020304;
    aobj.track = nGCAnimTrackPrimColor;
    aobj.kind = nGCAnimKindLinear;
    gcPlayMObjMatAnim(&mobj);
    CHECK(mobj.sub.primcolor.pack == 0x01020304);
}

static void test_screen_flash(void)
{
    GObj *gobj, *cam, *ecam, *on25, *on24, *on22, *g;
    const LBCommonSpriteQuad *q;
    s32 n, k, t;
    Vp want;
    u32 prev;
    sb32 order_ok, seen_flash;

    /* ---- the option gate: off, nothing is made; the state is reset
     * either way ---- */
    sIFScreenFlashColAnim.colanim_id = nGMColAnimScreenFlashDamageIce;
    gSCManagerBackupData.is_allow_screenflash = FALSE;
    n = flash_link_count(nGCCommonLinkIDInterface);
    ifScreenFlashMakeInterface(0x80);
    CHECK(flash_link_count(nGCCommonLinkIDInterface) == n);
    CHECK(sIFScreenFlashAlpha == 0x80);
    CHECK(sIFScreenFlashColAnim.colanim_id == 0);
    CHECK(sIFScreenFlashColAnim.cs[0].p_script == NULL);

    /* on, as scManagerInitData's defaults have it (test_backup): one
     * interface GObj on DL link 22 with a process of kind Func */
    gSCManagerBackupData.is_allow_screenflash = TRUE;
    ifScreenFlashMakeInterface(0xFF);
    CHECK(flash_link_count(nGCCommonLinkIDInterface) == n + 1);
    gobj = flash_find_display(nGCCommonLinkIDInterface, ifScreenFlashProcDisplay);
    CHECK(gobj != NULL);
    if (gobj == NULL)
    {
        return;
    }
    CHECK(gobj->dl_link_id == 22);
    CHECK(gobj->camera_tag == ~0u);
    CHECK(gobj->gobjproc_head != NULL);
    CHECK(gobj->gobjproc_head != NULL &&
          gobj->gobjproc_head->kind == nGCProcessKindFunc);
    CHECK(sIFScreenFlashAlpha == 0xFF);

    /* ---- the five scripts, tic by tic ---- */
    for (k = 0; k < (s32)ARRAY_COUNT(kFlashScript); k++)
    {
        ifScreenFlashSetColAnimID(kFlashScript[k].id, 0);
        CHECK(sIFScreenFlashColAnim.colanim_id == kFlashScript[k].id);
        CHECK(sIFScreenFlashColAnim.is_use_color1 == 0);
        CHECK(sIFScreenFlashColAnim.cs[0].p_script != NULL);
        CHECK(sIFScreenFlashColAnim.cs[1].p_script == NULL);

        for (t = 0; t < kFlashScript[k].tics; t++)
        {
            ifScreenFlashProcUpdate(gobj);
            CHECK(sIFScreenFlashColAnim.colanim_id == kFlashScript[k].id);
            CHECK(sIFScreenFlashColAnim.is_use_color1 == 1);
            CHECK(sIFScreenFlashColAnim.color1.r == kFlashScript[k].r);
            CHECK(sIFScreenFlashColAnim.color1.g == kFlashScript[k].g);
            CHECK(sIFScreenFlashColAnim.color1.b == kFlashScript[k].b);
            if (sIFScreenFlashColAnim.color1.a != kFlashScript[k].a[t])
            {
                printf("  screen flash script %d tic %d: alpha %d, want %d\n",
                       (int)kFlashScript[k].id, (int)t + 1,
                       (int)sIFScreenFlashColAnim.color1.a,
                       (int)kFlashScript[k].a[t]);
                fflush(stdout);
                CHECK(sIFScreenFlashColAnim.color1.a == kFlashScript[k].a[t]);
            }
        }
        /* the tic after the table is the End: over, and reset */
        ifScreenFlashProcUpdate(gobj);
        CHECK(sIFScreenFlashColAnim.colanim_id == 0);
        CHECK(sIFScreenFlashColAnim.is_use_color1 == 0);
        CHECK(sIFScreenFlashColAnim.cs[0].p_script == NULL);
        /* and one more is a no-op over a NULL script, not a fault */
        ifScreenFlashProcUpdate(gobj);
        CHECK(sIFScreenFlashColAnim.colanim_id == 0);
    }

    /* ---- the priority rule: 60 over 60 restarts, 1 over 60 is refused
     * and leaves the running one alone ---- */
    ifScreenFlashSetColAnimID(nGMColAnimScreenFlashDeadExplode, 0);
    for (t = 0; t < 3; t++)
    {
        ifScreenFlashProcUpdate(gobj);
    }
    CHECK(sIFScreenFlashColAnim.color1.a == 54);
    CHECK(ftParamCheckSetColAnimID(&sIFScreenFlashColAnim,
                                   nGMColAnimFighterComPlayer, 0) == FALSE);
    CHECK(sIFScreenFlashColAnim.colanim_id == nGMColAnimScreenFlashDeadExplode);
    CHECK(sIFScreenFlashColAnim.color1.a == 54);
    ifScreenFlashProcUpdate(gobj);
    CHECK(sIFScreenFlashColAnim.color1.a == 72);
    ifScreenFlashSetColAnimID(nGMColAnimScreenFlashDamageIce, 0);
    CHECK(sIFScreenFlashColAnim.colanim_id == nGMColAnimScreenFlashDamageIce);
    CHECK(sIFScreenFlashColAnim.is_use_color1 == 0);
    ifScreenFlashProcUpdate(gobj);
    CHECK(sIFScreenFlashColAnim.color1.a == 117);
    CHECK(sIFScreenFlashColAnim.color1.r == 0x00);
    CHECK(sIFScreenFlashColAnim.color1.b == 0xFF);

    /* ---- a length: the game passes 0 from every caller, and the
     * decomp's rule for a non-zero one is that it ends the animation
     * when it runs out, before the script's own End ---- */
    ifScreenFlashSetColAnimID(nGMColAnimScreenFlashDeadExplode, 4);
    CHECK(sIFScreenFlashColAnim.length == 4);
    for (t = 0; t < 3; t++)
    {
        ifScreenFlashProcUpdate(gobj);
        CHECK(sIFScreenFlashColAnim.colanim_id == nGMColAnimScreenFlashDeadExplode);
    }
    ifScreenFlashProcUpdate(gobj);
    CHECK(sIFScreenFlashColAnim.colanim_id == 0);
    CHECK(sIFScreenFlashColAnim.length == 0);

    /* ---- the quad ---- */
    ifScreenFlashSetColAnimID(nGMColAnimScreenFlashDamageFire, 0);
    ifScreenFlashProcUpdate(gobj);
    CHECK(sIFScreenFlashColAnim.color1.a == 70);

    gcSetDrawList(PVR_LIST_OP_POLY);
    gLBCommonSpriteQuadLogCount = 0;
    ifScreenFlashProcDisplay(gobj);
    CHECK(gLBCommonSpriteQuadLogCount == 0);
    gcSetDrawList(PVR_LIST_PT_POLY);
    ifScreenFlashProcDisplay(gobj);
    CHECK(gLBCommonSpriteQuadLogCount == 0);

    gcSetDrawList(PVR_LIST_TR_POLY);
    ifScreenFlashProcDisplay(gobj);
    CHECK(gLBCommonSpriteQuadLogCount == 1);
    q = &gLBCommonSpriteQuadLog[0];
    CHECK(q->rect.rxh == 10 * 4 && q->rect.ryh == 10 * 4);
    CHECK(q->rect.rxl == 310 * 4 && q->rect.ryl == 230 * 4);
    CHECK(q->rect.copy == -1);
    CHECK(q->argb == ((70u << 24) | 0xFF8C78u));

    /* the interface's alpha scales the script's: 70 * 0x80 / 0xFF is 35 */
    sIFScreenFlashAlpha = 0x80;
    gLBCommonSpriteQuadLogCount = 0;
    ifScreenFlashProcDisplay(gobj);
    CHECK(gLBCommonSpriteQuadLogCount == 1);
    CHECK(gLBCommonSpriteQuadLog[0].argb == ((35u << 24) | 0xFF8C78u));
    sIFScreenFlashAlpha = 0xFF;

    /* no colour, no quad */
    ftParamResetColAnim(&sIFScreenFlashColAnim);
    gLBCommonSpriteQuadLogCount = 0;
    ifScreenFlashProcDisplay(gobj);
    CHECK(gLBCommonSpriteQuadLogCount == 0);

    /* ---- the hit that asks for one: ftcommondamage.c:384-417, the
     * knockback strictly past VERYHIGH, and the element's script ---- */
    ftCommonDamageCheckMakeScreenFlash(FTCOMMON_DAMAGE_KNOCKBACK_VERYHIGH,
                                       nGMHitElementFire);
    CHECK(sIFScreenFlashColAnim.colanim_id == 0);
    ftCommonDamageCheckMakeScreenFlash(FTCOMMON_DAMAGE_KNOCKBACK_VERYHIGH + 0.5F,
                                       nGMHitElementFire);
    CHECK(sIFScreenFlashColAnim.colanim_id == nGMColAnimScreenFlashDamageFire);
    ftCommonDamageCheckMakeScreenFlash(FTCOMMON_DAMAGE_KNOCKBACK_VERYHIGH + 0.5F,
                                       nGMHitElementElectric);
    CHECK(sIFScreenFlashColAnim.colanim_id == nGMColAnimScreenFlashDamageElectric);
    ftCommonDamageCheckMakeScreenFlash(FTCOMMON_DAMAGE_KNOCKBACK_VERYHIGH + 0.5F,
                                       nGMHitElementFreezing);
    CHECK(sIFScreenFlashColAnim.colanim_id == nGMColAnimScreenFlashDamageIce);
    ftCommonDamageCheckMakeScreenFlash(FTCOMMON_DAMAGE_KNOCKBACK_VERYHIGH + 0.5F,
                                       nGMHitElementNormal);
    CHECK(sIFScreenFlashColAnim.colanim_id == nGMColAnimScreenFlashDamageNormal);
    ftCommonDamageCheckMakeScreenFlash(FTCOMMON_DAMAGE_KNOCKBACK_VERYHIGH + 0.5F,
                                       nGMHitElementSlash);
    CHECK(sIFScreenFlashColAnim.colanim_id == nGMColAnimScreenFlashDamageNormal);
    ftParamResetColAnim(&sIFScreenFlashColAnim);

    /* ---- the cameras ---- */
    gmCameraSetViewportDimensions(10, 10, 310, 230);
    gIFCommonPlayerInterface.magnify_mode = 0;

    gmCameraScreenFlashMakeCamera();
    cam = flash_find_camera(gmCameraScreenFlashProcDisplay);
    CHECK(cam != NULL);
    if (cam != NULL)
    {
        CHECK(cam->dl_link_priority == 20);
        CHECK(cam->camera_mask == COBJ_MASK_DLLINK(22));
        CHECK(cam->camera_tag == ~0u);
        CHECK(CObjGetStruct(cam)->flags & COBJ_FLAG_DLBUFFERS);
        CHECK(!(CObjGetStruct(cam)->flags & COBJ_FLAG_IDENTIFIER));
    }

    ecam = gmCameraMakeEffectCamera();
    CHECK(ecam != NULL);
    CHECK(ecam == flash_find_camera(gmCameraEffectProcDisplay));
    if (ecam != NULL)
    {
        CHECK(ecam->dl_link_priority == 15);
        CHECK(ecam->camera_mask == COBJ_MASK_DLLINK(25));
        CHECK(CObjGetStruct(ecam)->flags & COBJ_FLAG_DLBUFFERS);
        /* the battle's viewport, by the same call the battle camera makes
         * (sys/rdp.c syRdpSetViewport writes the first three of each) */
        memset(&want, 0, sizeof(want));
        syRdpSetViewport(&want, 10.0F, 10.0F, 310.0F, 230.0F);
        for (t = 0; t < 3; t++)
        {
            if (CObjGetStruct(ecam)->viewport.vp.vscale[t] != want.vp.vscale[t] ||
                CObjGetStruct(ecam)->viewport.vp.vtrans[t] != want.vp.vtrans[t])
            {
                printf("  effect camera viewport[%d]: scale %d trans %d, want %d %d (dims %d %d %d %d)\n",
                       (int)t, (int)CObjGetStruct(ecam)->viewport.vp.vscale[t],
                       (int)CObjGetStruct(ecam)->viewport.vp.vtrans[t],
                       (int)want.vp.vscale[t], (int)want.vp.vtrans[t],
                       (int)gGMCameraStruct.viewport_ulx, (int)gGMCameraStruct.viewport_uly,
                       (int)gGMCameraStruct.viewport_lrx, (int)gGMCameraStruct.viewport_lry);
                fflush(stdout);
            }
            CHECK(CObjGetStruct(ecam)->viewport.vp.vscale[t] == want.vp.vscale[t]);
            CHECK(CObjGetStruct(ecam)->viewport.vp.vtrans[t] == want.vp.vtrans[t]);
        }
    }

    /* the walk: gcDrawAll takes the camera list from its head, and the
     * list is kept highest priority first with equals in the order they
     * were made -- so the flash's camera comes before the effects' */
    prev = ~0u;
    order_ok = TRUE;
    seen_flash = FALSE;
    for (g = flash_camera_head(); g != NULL; g = g->dl_link_next)
    {
        if (g->dl_link_priority > prev)
        {
            order_ok = FALSE;
        }
        prev = g->dl_link_priority;
        if (g == cam)
        {
            seen_flash = TRUE;
        }
        if (g == ecam)
        {
            CHECK(seen_flash);
        }
    }
    CHECK(order_ok);

    /* what each captures: a counting display proc on DL links 22, 24
     * and 25, and each camera's own proc run in the translucent pass */
    on22 = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(on22, flash_count_proc, 22, GOBJ_PRIORITY_DEFAULT, ~0);
    on24 = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(on24, flash_count_proc, 24, GOBJ_PRIORITY_DEFAULT, ~0);
    on25 = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(on25, flash_count_proc, 25, GOBJ_PRIORITY_DEFAULT, ~0);

    gcSetDrawList(PVR_LIST_TR_POLY);
    if (ecam != NULL)
    {
        gFlashProcCalls = 0;
        gFlashProcLast = NULL;
        gGCCurrentCamera = ecam;
        gmCameraEffectProcDisplay(ecam);
        CHECK(gFlashProcCalls == 1);
        CHECK(gFlashProcLast == on25);
    }
    if (cam != NULL)
    {
        gFlashProcCalls = 0;
        gFlashProcLast = NULL;
        gGCCurrentCamera = cam;
        gmCameraScreenFlashProcDisplay(cam);
        CHECK(gFlashProcCalls == 1);
        CHECK(gFlashProcLast == on22);
    }
    gGCCurrentCamera = NULL;

    gcEjectGObj(on22);
    gcEjectGObj(on24);
    gcEjectGObj(on25);
    if (ecam != NULL)
    {
        gcEjectGObj(ecam);
    }
    if (cam != NULL)
    {
        gcEjectGObj(cam);
    }
    gcEjectGObj(gobj);
    CHECK(flash_link_count(nGCCommonLinkIDInterface) == n);
}

/* ---- the results screen's confetti ----------------------
 *
 * Two lines of the results screen -- efParticleInitAll and
 * efManagerInitEffects -- and one camera were what stood between
 * mnVSResultsMakeConfetti and the port. The scene tests above run the
 * whole screen with them in; what this adds is the camera's shape (the
 * podium's, over DL links 18, 15 and 10 -- ef/efdisplay.c's display
 * GObjs -- and 9, at priority 50, the emblem camera's view) and that the
 * confetti with no bank behind it, which is this host, makes nothing and
 * takes nothing: the maker is a particle maker and never touches the
 * EFStruct pool.
 */
static void test_results_confetti(void)
{
    GObj *cam;
    Vec3f pos = { 0.0F, 1000.0F, -400.0F };
    Vp want;
    s32 saved_bank = gEFManagerParticleBankID;
    s32 t;

    mnVSResultsMakeFighterCamera();
    for (cam = flash_camera_head(); cam != NULL; cam = cam->dl_link_next)
    {
        if (cam->camera_mask == (COBJ_MASK_DLLINK(18) | COBJ_MASK_DLLINK(15) |
                                 COBJ_MASK_DLLINK(10) | COBJ_MASK_DLLINK(9)))
        {
            break;
        }
    }
    CHECK(cam != NULL);
    if (cam != NULL)
    {
        CObj *cobj = CObjGetStruct(cam);

        CHECK(cam->dl_link_priority == 50);
        CHECK(cam->camera_tag == ~0u);
        CHECK(cam->proc_display == func_80017DBC);
        CHECK(cobj->vec.eye.x == 0.0F && cobj->vec.eye.y == 0.0F &&
              cobj->vec.eye.z == 1800.0F);
        CHECK(cobj->vec.at.x == 0.0F && cobj->vec.at.y == 0.0F &&
              cobj->vec.at.z == 0.0F);
        CHECK(cobj->vec.up.y == 1.0F);
        memset(&want, 0, sizeof(want));
        syRdpSetViewport(&want, 10.0F, 10.0F, 310.0F, 230.0F);
        for (t = 0; t < 3; t++)
        {
            CHECK(cobj->viewport.vp.vscale[t] == want.vp.vscale[t]);
            CHECK(cobj->viewport.vp.vtrans[t] == want.vp.vtrans[t]);
        }
    }

    ef_pool_reset();
    gEFManagerParticleBankID = -1;
    CHECK(efManagerConfettiMakeEffect(&pos, FALSE) == NULL);
    CHECK(efManagerConfettiMakeEffect(&pos, TRUE) == NULL);
    mnVSResultsMakeConfetti();
    CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
    gEFManagerParticleBankID = saved_bank;

    if (cam != NULL)
    {
        gcEjectGObj(cam);
    }
}

static void test_effect_manager(void)
{
    static LBTransform xf;
    static DObj dobj;
    EFStruct *ep;
    EFStruct vars;
    GObj *gobj;
    s32 saved_bank = gEFManagerParticleBankID;
    Vec3f pos = { 10.0F, 20.0F, 30.0F };
    s32 i;

    ef_pool_reset();


    /* efmanager.c:1762 efManagerGetNextStructAlloc: the free list head,
     * with the three fields every maker reads before it writes them
     * cleared. The pool was filled with 0xA5 above, so a field this
     * forgot would come back as garbage rather than as zero. */
    ep = efManagerGetEffectForce();
    CHECK(ep == &ef_pool[0]);

    if (ep != NULL)
    {
        CHECK(ep->fighter_gobj == NULL);
        CHECK(ep->xf == NULL);
        CHECK(ep->is_pause_effect == FALSE);
    }
    CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM - 1);

    /* efmanager.c:1800 efManagerSetPrevStructAlloc pushes it back on the
     * head, so the next take is the same struct. */
    efManagerSetPrevStructAlloc(ep);
    CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
    CHECK(efManagerGetEffectForce() == ep);

    /* The reserve, efmanager.c:1766: an effect that does not force gives
     * up while five structs are still free, so the ones that do force --
     * the KO explosion, the shield -- still have one to take. Thirty-
     * three no-force takes leave five, and five is not less than five,
     * so the thirty-fourth is allowed and the thirty-fifth is not. */
    ef_pool_reset();

    for (i = 0; i < (EFFECT_ALLOC_NUM - 5); i++)
    {
        CHECK(efManagerGetEffectNoForce() != NULL);
    }
    CHECK(sEFManagerStructsFreeNum == 5);
    CHECK(efManagerGetEffectNoForce() != NULL);
    CHECK(sEFManagerStructsFreeNum == 4);
    CHECK(efManagerGetEffectNoForce() == NULL);
    CHECK(sEFManagerStructsFreeNum == 4);

    for (i = 0; i < 4; i++)
    {
        CHECK(efManagerGetEffectForce() != NULL);
    }
    CHECK(sEFManagerStructsFreeNum == 0);
    CHECK(efManagerGetEffectForce() == NULL);

    /* And the unwind. With no bank loaded lbParticleMakeScriptID returns
     * NULL (lbparticle.c:404, script_id >= 0 scripts), which is the one
     * failure every maker in the file has to survive: it has already
     * taken a struct and made a GObj by then, and efManagerDestroy-
     * ParticleGObj is what gives both back. A leak here is an effect
     * that works for the first thirty-eight hits of a match and never
     * again, which is not something a frame rate or a screenshot would
     * ever show. Seven makers, twice the pool's depth each. */
    ef_pool_reset();
    gEFManagerParticleBankID = -1;

    for (i = 0; i < (EFFECT_ALLOC_NUM * 2); i++)
    {
        CHECK(efManagerSetOffMakeEffect(&pos, 10) == NULL);
        CHECK(efManagerDamageFireMakeEffect(&pos, 10) == NULL);
        CHECK(efManagerDamageElectricMakeEffect(&pos, 10) == NULL);
        CHECK(efManagerDamageCoinMakeEffect(&pos) == NULL);
        CHECK(efManagerDamageNormalLightMakeEffect(&pos, i & 3, 10, FALSE)
              == NULL);
        efManagerDamageNormalHeavyMakeEffect(&pos, i & 3, 10);
        CHECK(efManagerDustExpandSmallMakeEffect(&pos, 2.0F) == NULL);
        CHECK(efManagerConfettiMakeEffect(&pos, FALSE) == NULL);
        CHECK(efManagerConfettiMakeEffect(&pos, TRUE) == NULL);
        CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
    }
    /* The star-KO twinkle takes no struct at all -- a particle and a
     * transform and nothing else -- so it has none to leak, and this is
     * what says so. */
    CHECK(efManagerSparkleWhiteDeadMakeEffect(&pos, 5.0F) == NULL);
    CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);

    gEFManagerParticleBankID = saved_bank;

    /* efmanager.c:2103 efManagerDefaultProcUpdate, which is the whole of
     * what an effect GObj does per frame: it moves its particle's
     * transform by the velocity its maker rolled. Everything the seven
     * above compute -- the angle, the speed, the scale from the damage
     * -- ends here. */
    gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect,
                             GOBJ_PRIORITY_DEFAULT);
    CHECK(gobj != NULL);

    if (gobj != NULL)
    {
        memset(&vars, 0, sizeof(vars));
        memset(&xf, 0, sizeof(xf));

        gobj->user_data.p = &vars;
        vars.effect_vars.common.xf = &xf;
        vars.effect_vars.common.vel.x = 3.0F;
        vars.effect_vars.common.vel.y = -2.0F;
        xf.translate.x = 100.0F;
        xf.translate.y = 50.0F;
        xf.translate.z = 7.0F;

        efManagerDefaultProcUpdate(gobj);
        efManagerDefaultProcUpdate(gobj);

        CHECK_EQF(xf.translate.x, 106.0F);
        CHECK_EQF(xf.translate.y, 46.0F);
        CHECK_EQF(xf.translate.z, 7.0F);    /* Z is never touched */

        /* efmanager.c:1856,1881, the two Z sorters: an effect whose DObj
         * has gone a thousand units past the camera moves to DL link 2
         * and back to 20, which is how the game draws an effect in front
         * of or behind the stage. Nothing ported calls these yet -- the
         * EFDescs that name them are the model path -- but they come
         * with the spine and they are one comparison each. */
        memset(&dobj, 0, sizeof(dobj));
        dobj.parent_gobj = gobj;

        /* On a DL link first. gcMoveGObjDL, which is all the sorters
         * do, calls gcRemoveGObjFromDLLinkedList without checking
         * whether the GObj is on a link at all (objman.c:1996), and
         * gcInitGObjCommon's "no link" sentinel is dl_link_id =
         * ARRAY_COUNT(gGCCommonDLLinks) -- one past the end of both link
         * arrays. So sorting a GObj that has no display writes off the
         * end of gGCCommonDLLinks, and what is next to it is the GObj
         * pool's own ceiling. The game never does that: every effect
         * with a display proc goes through gcAddGObjDisplay in
         * efManagerMakeEffect before anything sorts it, and this is that
         * line. */
        gcAddGObjDisplay(gobj, NULL, 20, GOBJ_PRIORITY_DEFAULT, ~0);

        dobj.translate.vec.f.z = -2000.0F;
        efManagerSortZNeg(&dobj);
        CHECK(gobj->dl_link_id == 2);
        dobj.translate.vec.f.z = -999.0F;
        efManagerSortZNeg(&dobj);
        CHECK(gobj->dl_link_id == 20);

        dobj.translate.vec.f.z = 2000.0F;
        efManagerSortZPos(&dobj);
        CHECK(gobj->dl_link_id == 2);
        dobj.translate.vec.f.z = 999.0F;
        efManagerSortZPos(&dobj);
        CHECK(gobj->dl_link_id == 20);

        gcEjectGObj(gobj);
    }
    /* efmanager.c:1928 efManagerMakeEffect, the model path.
     * This build has no pack -- the host cross-test links the scene and
     * not the renderer -- so efManagerAddModel takes its FT_HOSTTEST arm
     * and stands up the two DObjs the halo's maker reaches for. What is
     * checked here is everything around the model: that the descriptor's
     * flags took a struct, that its DL link and both procs went onto the
     * GObj, that the root's three XObj kinds are the ones
     * transform_types1 names -- 0x50 and two Nulls, which is what makes
     * the halo follow a joint rather than sit at the origin -- and that
     * the struct comes back when the effect is ejected the way
     * ft/ftparam.c ejects it. */
    ef_pool_reset();
    {
        GObj *halo = efManagerMakeEffectForce(&dEFManagerRebirthHaloEffectDesc);

        CHECK(halo != NULL);
        CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM - 1);

        if (halo != NULL)
        {
            DObj *root = DObjGetStruct(halo);

            CHECK(halo->link_id == nGCCommonLinkIDEffect);
            CHECK(efGetStruct(halo) != NULL);
            CHECK(efGetStruct(halo)->proc_update == gcPlayAnimAll);
            CHECK(root != NULL);

            if (root != NULL)
            {
                CHECK(root->xobjs[0]->kind == 0x50);
                CHECK(root->xobjs[1]->kind == nGCMatrixKindNull);
                CHECK(root->xobjs[2]->kind == nGCMatrixKindNull);
                CHECK(root->child != NULL);

                /* And everything under it takes transform_types2, which
                 * for the halo is the fused nGCMatrixKindTraRotRpyRSca
                 * -- one XObj on the N64 and the port's own three spelt
                 * out (efManagerSetTransformTypes). */
                if (root->child != NULL)
                {
                    CHECK(root->child->xobjs[0]->kind == nGCMatrixKindTra);
                    CHECK(root->child->xobjs[1]->kind
                          == nGCMatrixKindRotRpyR);
                    CHECK(root->child->xobjs[2]->kind == nGCMatrixKindSca);
                }
            }
            /* ft/ftparam.c:1379 ftParamStopEffect, which is what takes
             * the halo away: the struct goes back on the free list and
             * the GObj is ejected. */
            efManagerSetPrevStructAlloc(efGetStruct(halo));
            gcEjectGObj(halo);
            CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
        }
    }
    /* And a descriptor the port has no pack for is refused whole -- no
     * GObj left on the link and no struct kept. Every EFDesc but the
     * halo's is in that state today, so this is the common case. */
    {
        EFDesc unknown = dEFManagerRebirthHaloEffectDesc;

        ef_pool_reset();
        CHECK(efManagerMakeEffectForce(&unknown) == NULL);
        CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
    }
    /* efmanager.c:3250 efManagerDamageSpawnOrbsMakeEffect and the pair
     * of procs above it. Two effects, and only one of them
     * has a model: a hit makes a *spawner*, whose EFDesc has no display
     * proc at all -- so efManagerMakeEffect returns before it ever looks
     * for a pack -- and whose update makes a flying orb every fourth
     * tic. This is where the two are told apart.
     *
     * The randomness is the game's own syUtilsRandFloat, so what is
     * checked is the ranges efmanager.c:3227-3238 writes, not particular
     * numbers: scale 3 to 5, speed 120 to 220, angle 60 to 120 degrees
     * (which is why an orb always goes upwards). */
    ef_pool_reset();
    {
        GObj *spawner =
            efManagerMakeEffectNoForce(&dEFManagerDamageSpawnOrbsEffectDesc);
        Vec3f at = { 11.0F, 22.0F, 33.0F };

        CHECK(spawner != NULL);

        if (spawner != NULL)
        {
            EFStruct *sp = efGetStruct(spawner);

            /* No display proc, so no tree: efmanager.c:1969 returns the
             * GObj as it stands. */
            CHECK(DObjGetStruct(spawner) == NULL);
            CHECK(sp != NULL);

            sp->effect_vars.damage_spawn_orbs.pos = at;
            sp->effect_vars.damage_spawn_orbs.lifetime = 8;

            efManagerDamageSpawnOrbsProcUpdate(spawner);

            /* 8 % 4 == 0, so that tic made one. */
            CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM - 2);
            CHECK(sp->effect_vars.damage_spawn_orbs.lifetime == 7);

            /* Both that orb and the spawner have to go again: this
             * suite runs on one GObj pool. */
            {
                GObj *made = ef_find_fly_orb();

                CHECK(made != NULL);

                if (made != NULL)
                {
                    efManagerSetPrevStructAlloc(efGetStruct(made));
                    gcEjectGObj(made);
                }
            }
            efManagerSetPrevStructAlloc(sp);
            gcEjectGObj(spawner);
        }
    }
    {
        GObj *orb =
            efManagerMakeEffectNoForce(&dEFManagerDamageFlyOrbsEffectDesc);

        CHECK(orb != NULL);

        if (orb != NULL)
        {
            DObj *root = DObjGetStruct(orb);
            EFStruct *op = efGetStruct(orb);

            CHECK(orb->dl_link_id == 15);
            CHECK(op != NULL);
            CHECK(op->proc_update == efManagerDamageFlyOrbsProcUpdate);
            CHECK(root != NULL);

            if (root != NULL)
            {
                /* transform_types1 is {0x28, Sca, Null}: matrix kind 40,
                 * the camera-facing billboard, and then the scale the
                 * spawner rolled. The child takes transform_types2,
                 * {Sca, Null, Null} -- the animation's own shrink and
                 * nothing else. Neither triple is fused, so both go into
                 * the slots as they stand. */
                CHECK(root->xobjs[0]->kind == 40);
                CHECK(root->xobjs[1]->kind == nGCMatrixKindSca);
                CHECK(root->xobjs[2]->kind == nGCMatrixKindNull);
                CHECK(root->child != NULL);

                if (root->child != NULL)
                {
                    CHECK(root->child->xobjs[0]->kind == nGCMatrixKindSca);
                    CHECK(root->child->xobjs[1]->kind == nGCMatrixKindNull);
                    CHECK(root->child->xobjs[2]->kind == nGCMatrixKindNull);
                }
                /* efmanager.c:3183-3204, the flyer's whole motion. */
                root->translate.vec.f.x = 100.0F;
                root->translate.vec.f.y = 200.0F;
                root->translate.vec.f.z = 300.0F;
                op->effect_vars.damage_fly_orbs.vel.x = 7.0F;
                op->effect_vars.damage_fly_orbs.vel.y = 40.0F;
                op->effect_vars.damage_fly_orbs.lifetime = 2;

                efManagerDamageFlyOrbsProcUpdate(orb);

                CHECK_EQF(root->translate.vec.f.x, 107.0F);
                CHECK_EQF(root->translate.vec.f.y, 240.0F);
                CHECK_EQF(root->translate.vec.f.z, 300.0F);
                CHECK_EQF(op->effect_vars.damage_fly_orbs.vel.y, 30.0F);
                CHECK(op->effect_vars.damage_fly_orbs.lifetime == 1);

                /* Two more tics take the lifetime below zero, which is
                 * where efmanager.c:3193 gives the struct back and
                 * ejects the GObj itself. */
                efManagerDamageFlyOrbsProcUpdate(orb);
                efManagerDamageFlyOrbsProcUpdate(orb);
                CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
            }
        }
    }
    /* The spawner rolls each orb inside efmanager.c:3227-3238's ranges,
     * and the one thing about them that is not obvious is that the angle
     * is 90 degrees plus or minus 30, so an orb never goes downwards.
     * Forty rolls, because one is no evidence. */
    ef_pool_reset();
    {
        s32 rolls;

        for (rolls = 0; rolls < 40; rolls++)
        {
            GObj *spawner = efManagerMakeEffectForce(
                &dEFManagerDamageSpawnOrbsEffectDesc);
            EFStruct *sp = (spawner != NULL) ? efGetStruct(spawner) : NULL;

            if (sp == NULL)
            {
                CHECK(FALSE);
                break;
            }
            sp->effect_vars.damage_spawn_orbs.pos = pos;
            sp->effect_vars.damage_spawn_orbs.lifetime = 4;
            efManagerDamageSpawnOrbsProcUpdate(spawner);

            {
                GObj *orb = ef_find_fly_orb();
                EFStruct *op;
                DObj *root;
                f32 speed;

                CHECK(orb != NULL);

                if (orb == NULL)
                {
                    efManagerSetPrevStructAlloc(efGetStruct(spawner));
                    gcEjectGObj(spawner);
                    break;
                }
                op = efGetStruct(orb);
                root = DObjGetStruct(orb);
                CHECK(op != NULL && root != NULL);

                if (op == NULL || root == NULL)
                {
                    break;
                }
                CHECK_EQF(root->translate.vec.f.x, pos.x);
                CHECK_EQF(root->translate.vec.f.y, pos.y);
                CHECK_EQF(root->translate.vec.f.z, pos.z);
                CHECK(root->scale.vec.f.x >= 3.0F &&
                      root->scale.vec.f.x < 5.0F);
                CHECK_EQF(root->scale.vec.f.y, root->scale.vec.f.x);

                speed = sqrtf(SQUARE(op->effect_vars.damage_fly_orbs.vel.x) +
                              SQUARE(op->effect_vars.damage_fly_orbs.vel.y));
                CHECK(speed >= 119.9F && speed < 220.1F);
                /* 60 to 120 degrees: the Y is always up and the X never
                 * bigger than the Y. */
                CHECK(op->effect_vars.damage_fly_orbs.vel.y > 0.0F);
                CHECK(op->effect_vars.damage_fly_orbs.vel.y >=
                      ABSF(op->effect_vars.damage_fly_orbs.vel.x) - 0.001F);
                CHECK(op->effect_vars.damage_fly_orbs.lifetime >= 12 &&
                      op->effect_vars.damage_fly_orbs.lifetime < 16);

                efManagerSetPrevStructAlloc(op);
                gcEjectGObj(orb);
            }
            efManagerSetPrevStructAlloc(efGetStruct(spawner));
            gcEjectGObj(spawner);
        }
        CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
    }
    /* And the matrix the billboard kind builds (src/dc/objdisplay.c
     * gcMtxBillboard, sys/objdisplay.c:169 func_80010918). It is what
     * turns the orbs' one flat quad to face the camera, and it is the
     * only piece of this test that is arithmetic rather than plumbing.
     *
     * With the eye straight out along +Z -- where the battle camera is
     * -- the basis is the identity: the quad as authored already faces
     * the camera. The last column is the orb's own position, because
     * kind 40 carries the translate and kind 39 does not. */
    {
        float m[16];
        Vec3f at = { 10.0F, 20.0F, 30.0F };
        Vec3f eye = { 10.0F, 20.0F, 1030.0F };

        gcMtxBillboard(m, &at, &eye, TRUE);

        CHECK_NEAR(m[0], 1.0F, 0.0001F);
        CHECK_NEAR(m[5], 1.0F, 0.0001F);
        CHECK_NEAR(m[10], 1.0F, 0.0001F);
        CHECK_NEAR(m[1], 0.0F, 0.0001F);
        CHECK_NEAR(m[4], 0.0F, 0.0001F);
        CHECK_EQF(m[3], 10.0F);
        CHECK_EQF(m[7], 20.0F);
        CHECK_EQF(m[11], 30.0F);
        CHECK_EQF(m[12], 0.0F);
        CHECK_EQF(m[15], 1.0F);

        /* Kind 39 is the same basis with no translate in it. */
        gcMtxBillboard(m, &at, &eye, FALSE);
        CHECK_EQF(m[3], 0.0F);
        CHECK_EQF(m[7], 0.0F);
        CHECK_EQF(m[11], 0.0F);

        /* The camera directly above: `res` is zero, there is no answer,
         * and the decomp leaves the identity rather than dividing by it
         * (objdisplay.c:209-213). */
        eye.x = 10.0F;
        eye.y = 1020.0F;
        eye.z = 30.0F;
        gcMtxBillboard(m, &at, &eye, TRUE);
        CHECK_EQF(m[0], 1.0F);
        CHECK_EQF(m[5], 1.0F);
        CHECK_EQF(m[10], 1.0F);
        CHECK_EQF(m[1], 0.0F);
        CHECK_EQF(m[9], 0.0F);

        /* The camera off to +X: the quad's own +X now points along world
         * -Z and its -Z along +X, which is the turn that keeps a flat
         * sprite side-on to nobody. */
        eye.x = 1010.0F;
        eye.y = 20.0F;
        eye.z = 30.0F;
        gcMtxBillboard(m, &at, &eye, TRUE);
        CHECK_NEAR(m[8], -1.0F, 0.0001F);   /* the new Z's x */
        CHECK_NEAR(m[2], 1.0F, 0.0001F);    /* the new X's z */
        CHECK_NEAR(m[5], 1.0F, 0.0001F);
    }
    /* efmanager.c:2486 efManagerDamageSlashMakeEffect.
     *
     * The first descriptor the port reads that names all four of an
     * EFDesc's offsets, and the first with a *material*: its two MObjs
     * each carry a sprite array their MatAnimJoints step through. None
     * of that is here -- this build has no pack and no renderer -- so
     * what the host can check is the rest of it, which is the half that
     * would be silently wrong.
     *
     * The slash's flags are 0x4 | 0x1 and *not* EFFECT_FLAG_USERDATA, so
     * it takes no EFStruct at all: efManagerFuncRun finds a NULL one and
     * gives it efManagerNoStructProcUpdate, and its whole lifetime is its
     * animation. The proc_update its descriptor names is never called.
     * That is easy to get wrong by copying the orbs. */
    ef_pool_reset();
    {
        static const struct { s32 size; f32 scale; } want[] = {
            { 0, 0.6F }, { 4, 0.92F }, { 5, 1.0F }, { 10, 1.9F },
        };
        s32 k;

        CHECK(dEFManagerDamageSlashEffectDesc.flags == (0x4 | 0x1));
        CHECK(!(dEFManagerDamageSlashEffectDesc.flags &
                EFFECT_FLAG_USERDATA));
        CHECK(dEFManagerDamageSlashEffectDesc.dl_link == 18);
        /* The decomp's own value: this test never calls
         * efManagerInitEffects (see the file header above), so the port's
         * runtime fixup (src/dc/efmanager.c's efManagerFixupModelProcDisplays)
         * never runs here and the field stays gcDrawDObjTreeDLLinksForGObj. */
        CHECK(dEFManagerDamageSlashEffectDesc.proc_display ==
              gcDrawDObjTreeDLLinksForGObj);

        for (k = 0; k < (s32)ARRAY_COUNT(want); k++)
        {
            GObj *slash = efManagerDamageSlashMakeEffect(&pos, want[k].size,
                                                         0.75F);
            DObj *root = (slash != NULL) ? DObjGetStruct(slash) : NULL;

            CHECK(slash != NULL);
            CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
            if (slash == NULL || root == NULL)
            {
                CHECK(FALSE);
                break;
            }
            CHECK(slash->link_id == nGCCommonLinkIDEffect);
            CHECK(efGetStruct(slash) == NULL);
            CHECK(slash->dl_link_id == 18);

            /* transform_types1 is {0x28, 0x45, 0}: the camera-facing
             * billboard over a second kind, which is what keeps the
             * streak square to the camera wherever the hit was. */
            CHECK(root->xobjs[0]->kind == 0x28);
            CHECK(root->xobjs[1]->kind == 0x45);
            CHECK(root->xobjs[2]->kind == nGCMatrixKindNull);

            /* and transform_types2 under it, the fused
             * nGCMatrixKindTraRotRpyRSca spelt out */
            CHECK(root->child != NULL);
            if (root->child != NULL)
            {
                CHECK(root->child->xobjs[0]->kind == nGCMatrixKindTra);
                CHECK(root->child->xobjs[1]->kind == nGCMatrixKindRotRpyR);
                CHECK(root->child->xobjs[2]->kind == nGCMatrixKindSca);
            }
            /* What the maker itself writes: the hit's position, the
             * hit's angle about Z, and a scale that is 1 at size 5 and
             * bends the other way below it -- 0.08 per point down,
             * 0.18 per point up. */
            CHECK_EQF(root->translate.vec.f.x, pos.x);
            CHECK_EQF(root->translate.vec.f.y, pos.y);
            CHECK_EQF(root->translate.vec.f.z, pos.z);
            CHECK_EQF(root->rotate.vec.f.z, 0.75F);
            CHECK_NEAR(root->scale.vec.f.x, want[k].scale, 0.0001F);
            CHECK_NEAR(root->scale.vec.f.y, want[k].scale, 0.0001F);
            /* Z is left alone: the slash is flat and stays flat. */
            CHECK_EQF(root->scale.vec.f.z, 1.0F);

            gcEjectGObj(slash);
        }
        CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
    }
    /* efmanager.c:3493 efManagerDamageSpawnSparksMakeEffect and the two
     * procs above it. A pair like the orbs': the spawner
     * has no display proc and so no tree, and its update makes a flyer on
     * three of its eight tics. What is different, and what is checked
     * here, is that the three angles are not random --
     * dEFManagerDamageSpawnSparksAngles read *backwards* as the lifetime
     * counts down -- so every number below is exact.
     *
     * -(lifetime / 4) + 2 with lifetime 8, 4 and 0 is index 0, 1 and 2,
     * which is +18, 0 and -18 degrees. `lr` mirrors the X.
     */
    ef_pool_reset();
    {
        static const f32 want_deg[] = { 18.0F, 0.0F, -18.0F };
        GObj *spawner = efManagerDamageSpawnSparksMakeEffect(&pos, -1);
        s32 made = 0;
        s32 tic;

        CHECK(dEFManagerDamageSpawnSparksEffectDesc.flags ==
              EFFECT_FLAG_USERDATA);
        CHECK(dEFManagerDamageSpawnSparksEffectDesc.proc_display == NULL);
        CHECK(dEFManagerDamageSpawnSparksEffectDesc.o_dobjsetup == 0);
        CHECK(dEFManagerDamageSpawnSparksEffectDesc.o_mobjsub == 0);
        CHECK(spawner != NULL);

        if (spawner != NULL)
        {
            EFStruct *sp = efGetStruct(spawner);

            /* No display proc, so no tree (efmanager.c:1969). */
            CHECK(DObjGetStruct(spawner) == NULL);
            CHECK(sp != NULL);
            CHECK(sp->effect_vars.damage_spawn_sparks.lifetime == 8);
            CHECK(sp->effect_vars.damage_spawn_sparks.lr == -1);
            CHECK_EQF(sp->effect_vars.damage_spawn_sparks.pos.x, pos.x);
            CHECK_EQF(sp->effect_vars.damage_spawn_sparks.pos.y, pos.y);
            CHECK_EQF(sp->effect_vars.damage_spawn_sparks.pos.z, pos.z);

            /* Nine tics: eight of lifetime and the one that ejects it. */
            for (tic = 0; tic < 9 && sp != NULL; tic++)
            {
                s32 before = sEFManagerStructsFreeNum;
                s32 life = sp->effect_vars.damage_spawn_sparks.lifetime;
                GObj *fly;

                if (life < 0)
                {
                    break;
                }
                efManagerDamageSpawnSparksProcUpdate(spawner);

                if ((life % 4) != 0)
                {
                    CHECK(sEFManagerStructsFreeNum == before);
                    continue;
                }
                /* One struct out for the flyer -- except on the last
                 * tic, where the spawner's own lifetime goes negative
                 * in the same call and it gives its struct back before
                 * returning (efmanager.c:3484-3488). */
                CHECK(sEFManagerStructsFreeNum ==
                      before - ((life == 0) ? 0 : 1));

                fly = ef_find_fly_spark();
                CHECK(fly != NULL);

                if (fly != NULL)
                {
                    DObj *root = DObjGetStruct(fly);
                    EFStruct *xp = efGetStruct(fly);
                    f32 a = F_CLC_DTOR32(want_deg[made]);

                    CHECK(fly->dl_link_id == 15);
                    CHECK(root != NULL);
                    CHECK(xp != NULL);

                    if (root != NULL)
                    {
                        /* transform_types1 {0x28, RotRpyR, 0}: the
                         * camera-facing billboard again, and under it
                         * transform_types2 {0x45, Null, 0}. */
                        CHECK(root->xobjs[0]->kind == 0x28);
                        CHECK(root->xobjs[1]->kind == nGCMatrixKindRotRpyR);
                        CHECK(root->xobjs[2]->kind == nGCMatrixKindNull);
                        CHECK(root->child != NULL);

                        if (root->child != NULL)
                        {
                            CHECK(root->child->xobjs[0]->kind == 0x45);
                            CHECK(root->child->xobjs[1]->kind ==
                                  nGCMatrixKindNull);
                            CHECK(root->child->xobjs[2]->kind ==
                                  nGCMatrixKindNull);
                        }
                        CHECK_EQF(root->translate.vec.f.x, pos.x);
                        CHECK_EQF(root->translate.vec.f.y, pos.y);
                        CHECK_EQF(root->translate.vec.f.z, pos.z);
                        /* the one random thing about it */
                        CHECK(root->rotate.vec.f.z >= 0.0F);
                        CHECK(root->rotate.vec.f.z <
                              F_CLC_DTOR32(360.0F) + 0.001F);
                    }
                    if (xp != NULL)
                    {
                        /* 50 units a tic at the table's angle, mirrored
                         * by lr, and a per-tic bend of 0.004 of it back
                         * toward the middle for 250 tics. */
                        CHECK_NEAR(xp->effect_vars.damage_fly_sparks.vel.x,
                                   cosf(a) * 50.0F * -1.0F, 0.001F);
                        CHECK_NEAR(xp->effect_vars.damage_fly_sparks.vel.y,
                                   sinf(a) * 50.0F, 0.001F);
                        CHECK_NEAR(xp->effect_vars.damage_fly_sparks.add.x,
                                   -xp->effect_vars.damage_fly_sparks.vel.x
                                   * 0.004F, 0.000001F);
                        CHECK_NEAR(xp->effect_vars.damage_fly_sparks.add.y,
                                   -xp->effect_vars.damage_fly_sparks.vel.y
                                   * 0.004F, 0.000001F);
                        CHECK(xp->effect_vars.damage_fly_sparks.add_timer ==
                              250);

                        /* And one tic of the flyer's own update. There is
                         * no pack in this build, so gcAddAnimAll gave it
                         * nothing and anim_frame would eject it on the
                         * spot; write the frame the animation would have
                         * left and drive the other arm. */
                        if (root != NULL)
                        {
                            Vec3f v = xp->effect_vars.damage_fly_sparks.vel;
                            Vec3f d = xp->effect_vars.damage_fly_sparks.add;

                            fly->anim_frame = 10.0F;
                            efManagerDamageFlySparksProcUpdate(fly);

                            CHECK_NEAR(root->translate.vec.f.x, pos.x + v.x,
                                       0.001F);
                            CHECK_NEAR(root->translate.vec.f.y, pos.y + v.y,
                                       0.001F);
                            CHECK_EQF(root->translate.vec.f.z, pos.z);
                            CHECK(xp->effect_vars.damage_fly_sparks.add_timer
                                  == 249);
                            CHECK_NEAR(
                                xp->effect_vars.damage_fly_sparks.vel.x,
                                v.x + d.x, 0.001F);
                        }
                    }
                    efManagerSetPrevStructAlloc(efGetStruct(fly));
                    gcEjectGObj(fly);
                    made++;
                }
            }
            /* Three, and the spawner ejected itself on the ninth tic. */
            CHECK(made == 3);
            CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
        }
    }
    /* efmanager.c:3368 efManagerStarRodSparkMakeEffect and the proc above
     * it. Not a spawner/flyer pair -- one call, one GObj,
     * returned directly -- and dEFManagerStarRodSparkEffectDesc reuses
     * the damage sparks' exact four blocks (o_dobjsetup and o_mobjsub
     * are the same pointers, checked below), so this is the maker's own
     * writes and the update's decay, not the geometry again.
     *
     * transform_types1 is {0x28, 0x45, 0} here, not the flyer's {0x28,
     * RotRpyR, 0} -- a different literal in the decomp's own source,
     * not a typo this port repeats differently. */
    ef_pool_reset();
    {
        GObj *spark = efManagerStarRodSparkMakeEffect(&pos, 1);

        CHECK(dEFManagerStarRodSparkEffectDesc.flags ==
              (EFFECT_FLAG_USERDATA | 0x1));
        CHECK(dEFManagerStarRodSparkEffectDesc.proc_display ==
              lbCommonDObjScaleXProcDisplay);
        CHECK(dEFManagerStarRodSparkEffectDesc.o_dobjsetup ==
              dEFManagerDamageFlySparksEffectDesc.o_dobjsetup);
        CHECK(dEFManagerStarRodSparkEffectDesc.o_mobjsub ==
              dEFManagerDamageFlySparksEffectDesc.o_mobjsub);
        CHECK(spark != NULL);

        if (spark != NULL)
        {
            DObj *root = DObjGetStruct(spark);
            EFStruct *ep = efGetStruct(spark);

            CHECK(spark->dl_link_id == 15);
            CHECK(root != NULL);
            CHECK(ep != NULL);

            if (root != NULL)
            {
                CHECK(root->xobjs[0]->kind == 0x28);
                CHECK(root->xobjs[1]->kind == 0x45);
                CHECK(root->xobjs[2]->kind == nGCMatrixKindNull);
                CHECK(root->child != NULL);

                if (root->child != NULL)
                {
                    CHECK(root->child->xobjs[0]->kind == 0x45);
                    CHECK(root->child->xobjs[1]->kind == nGCMatrixKindNull);
                    CHECK(root->child->xobjs[2]->kind == nGCMatrixKindNull);
                }
                CHECK_EQF(root->translate.vec.f.x, pos.x);
                CHECK_EQF(root->translate.vec.f.y, pos.y);
                CHECK_EQF(root->translate.vec.f.z, pos.z);
                CHECK(root->rotate.vec.f.z >= 0.0F);
                CHECK(root->rotate.vec.f.z < F_CLC_DTOR32(360.0F) + 0.001F);
                CHECK_EQF(root->scale.vec.f.x, EFCOMMON_STARRODSPARK_SCALE);
                CHECK_EQF(root->scale.vec.f.y, EFCOMMON_STARRODSPARK_SCALE);
            }
            if (ep != NULL)
            {
                /* lr=1: vel = +25, add = -0.4, a 62-tic decay. */
                CHECK_EQF(ep->effect_vars.star_rod_spark.vel.x, 25.0F);
                CHECK_EQF(ep->effect_vars.star_rod_spark.add.x, -0.4F);
                CHECK(ep->effect_vars.star_rod_spark.add_timer == 62);

                /* One tic of the update. No pack in this build, so
                 * anim_frame starts 0 and the eject branch would fire
                 * on the spot; write the frame the animation would
                 * have left, as the flyer's test above does, and drive
                 * the decay arm instead.
                 *
                 * Unlike the flyer above, this update bends vel *before*
                 * adding it to translate (efmanager.c:3363-3367), so the
                 * position moves by the new velocity, not the old one. */
                if (root != NULL)
                {
                    f32 vel = ep->effect_vars.star_rod_spark.vel.x + -0.4F;

                    spark->anim_frame = 10.0F;
                    efManagerStarRodSparkProcUpdate(spark);

                    CHECK_NEAR(root->translate.vec.f.x, pos.x + vel,
                               0.001F);
                    CHECK(ep->effect_vars.star_rod_spark.add_timer == 61);
                    CHECK_NEAR(ep->effect_vars.star_rod_spark.vel.x, vel,
                               0.001F);
                }
                efManagerSetPrevStructAlloc(ep);
            }
            gcEjectGObj(spark);
        }
        CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
    }

    /* efmanager.c:2772 efManagerShockSmallMakeEffect, the
     * first of the two DIVERGES arms that turned out to need a real
     * export: EFCommonEffects2's own picture, efshock.mdl, no reuse.
     * Flags lack 0x1, so this is one DObj, not a stand and a child --
     * gcAddDObjForGObj gets the raw display list directly and
     * transform_types1 {0x28, 0x45, Null} lands on the DObj itself. The
     * maker jitters position, scale and rotation with syUtilsRandFloat,
     * so the test brackets what the decomp's own constants promise
     * rather than a value the RNG could change. */
    ef_pool_reset();
    {
        Vec3f shock_pos = { 100.0F, 200.0F, 300.0F };
        GObj *shock = efManagerShockSmallMakeEffect(&shock_pos);

        CHECK(dEFManagerShockSmallEffectDesc.flags == EFFECT_FLAG_USERDATA);
        CHECK(dEFManagerShockSmallEffectDesc.proc_display ==
              lbCommonDObjScaleXProcDisplay);
        CHECK(shock != NULL);

        if (shock != NULL)
        {
            DObj *root = DObjGetStruct(shock);
            EFStruct *ep = efGetStruct(shock);

            CHECK(shock->dl_link_id == 18);
            CHECK(root != NULL);
            CHECK(ep != NULL);

            if (root != NULL)
            {
                CHECK(root->xobjs[0]->kind == 0x28);
                CHECK(root->xobjs[1]->kind == 0x45);
                CHECK(root->xobjs[2]->kind == nGCMatrixKindNull);

                /* pos += rand()*300 + (-150): a delta in [-150, 150). */
                CHECK(root->translate.vec.f.x >= 100.0F - 150.0F &&
                      root->translate.vec.f.x < 100.0F + 150.0F);
                CHECK(root->translate.vec.f.y >= 200.0F - 150.0F &&
                      root->translate.vec.f.y < 200.0F + 150.0F);
                CHECK_EQF(root->translate.vec.f.z, 300.0F);

                /* rand()*0.5 + 0.75: a scale in [0.75, 1.25). */
                CHECK(root->scale.vec.f.x >= 0.75F &&
                      root->scale.vec.f.x < 1.25F);
                CHECK_EQF(root->scale.vec.f.x, root->scale.vec.f.y);

                CHECK(root->rotate.vec.f.z >= 0.0F);
                CHECK(root->rotate.vec.f.z < F_CLC_DTOR32(360.0F) + 0.001F);
            }
            if (ep != NULL)
            {
                /* the DIVERGES: __cosf/__sinf are computed and dropped
                 * on the floor by the shipped (DAIRANTOU_OPT0) build,
                 * so both survive as a flat 0. */
                CHECK_EQF(ep->effect_vars.common.vel.x, 0.0F);
                CHECK_EQF(ep->effect_vars.common.vel.y, 0.0F);

                efManagerSetPrevStructAlloc(ep);
            }
            gcEjectGObj(shock);
        }
        CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
    }
    /* efmanager.c:3574 efManagerDamageSpawnMDustMakeEffect and the proc
     * above it. The sparks again out of blocks of its own,
     * so the numbers are the sparks' numbers -- eight tics, three flyers,
     * +18/0/-18 degrees, 50 units a tic, 0.004 of it bent back for 250
     * tics -- and the point of running them a second time is that they
     * come out of a *different* table and a different EFStruct arm.
     *
     * What is actually different is in the descriptors, and it is checked
     * first: this flyer's proc_display is gcDrawDObjTreeDLLinksForGObj
     * rather than lbCommonDObjScaleXProcDisplay, which is what makes its
     * o_dobjsetup a DObjDLLink array rather than a display list (see
     * tools/check/mdust_check.py), and its transform_types2.tk1 is 0x44 rather
     * than 0x45.
     */
    ef_pool_reset();
    {
        static const f32 want_deg[] = { 18.0F, 0.0F, -18.0F };
        GObj *spawner;
        s32 made = 0;
        s32 tic;

        /* The two tables really are two tables, and they really do hold
         * the same three angles (efmanager.c:14-18). */
        /* the casts are the point: this compares the two TABLES'
         * addresses, not their contents, which is why the compiler
         * warns about comparing two arrays until it is told so. */
        CHECK((const void *)dEFManagerDamageSpawnMDustAngles !=
              (const void *)dEFManagerDamageSpawnSparksAngles);
        for (tic = 0; tic < 3; tic++)
        {
            CHECK_EQF(dEFManagerDamageSpawnMDustAngles[tic], want_deg[tic]);
        }
        CHECK(dEFManagerDamageFlyMDustEffectDesc.flags ==
              (EFFECT_FLAG_USERDATA | 0x1));
        CHECK(dEFManagerDamageFlyMDustEffectDesc.dl_link == 15);
        /* The decomp's own value: this test never calls
         * efManagerInitEffects (see the file header above), so the port's
         * runtime fixup (src/dc/efmanager.c's efManagerFixupModelProcDisplays)
         * never runs here and the field stays gcDrawDObjTreeDLLinksForGObj. */
        CHECK(dEFManagerDamageFlyMDustEffectDesc.proc_display ==
              gcDrawDObjTreeDLLinksForGObj);
        CHECK(dEFManagerDamageFlySparksEffectDesc.proc_display ==
              lbCommonDObjScaleXProcDisplay);
        CHECK(dEFManagerDamageFlyMDustEffectDesc.proc_update ==
              efManagerDamageFlySparksProcUpdate);
        CHECK(dEFManagerDamageFlyMDustEffectDesc.transform_types2.tk1 == 0x44);
        CHECK(dEFManagerDamageSpawnMDustEffectDesc.flags ==
              EFFECT_FLAG_USERDATA);
        CHECK(dEFManagerDamageSpawnMDustEffectDesc.proc_display == NULL);
        CHECK(dEFManagerDamageSpawnMDustEffectDesc.o_dobjsetup == 0);
        CHECK(dEFManagerDamageSpawnMDustEffectDesc.o_mobjsub == 0);

        spawner = efManagerDamageSpawnMDustMakeEffect(&pos, 1);
        CHECK(spawner != NULL);

        if (spawner != NULL)
        {
            EFStruct *sp = efGetStruct(spawner);

            CHECK(DObjGetStruct(spawner) == NULL);
            CHECK(sp != NULL);
            CHECK(sp->effect_vars.damage_spawn_mdust.lifetime == 8);
            CHECK(sp->effect_vars.damage_spawn_mdust.lr == 1);
            CHECK_EQF(sp->effect_vars.damage_spawn_mdust.pos.x, pos.x);
            CHECK_EQF(sp->effect_vars.damage_spawn_mdust.pos.y, pos.y);
            CHECK_EQF(sp->effect_vars.damage_spawn_mdust.pos.z, pos.z);

            for (tic = 0; tic < 9 && sp != NULL; tic++)
            {
                s32 before = sEFManagerStructsFreeNum;
                s32 life = sp->effect_vars.damage_spawn_mdust.lifetime;
                GObj *fly;

                if (life < 0)
                {
                    break;
                }
                efManagerDamageSpawnMDustProcUpdate(spawner);

                if ((life % 4) != 0)
                {
                    CHECK(sEFManagerStructsFreeNum == before);
                    continue;
                }
                /* As the sparks': net zero on the last tic, where the
                 * spawner gives its own struct back in the same call
                 * (efmanager.c:3565-3569). */
                CHECK(sEFManagerStructsFreeNum ==
                      before - ((life == 0) ? 0 : 1));

                fly = ef_find_fly_spark();
                CHECK(fly != NULL);

                if (fly != NULL)
                {
                    DObj *root = DObjGetStruct(fly);
                    EFStruct *xp = efGetStruct(fly);
                    f32 a = F_CLC_DTOR32(want_deg[made]);

                    CHECK(fly->dl_link_id == 15);
                    CHECK(root != NULL);
                    CHECK(xp != NULL);

                    if (root != NULL)
                    {
                        CHECK(root->xobjs[0]->kind == 0x28);
                        CHECK(root->xobjs[1]->kind == nGCMatrixKindRotRpyR);
                        CHECK(root->xobjs[2]->kind == nGCMatrixKindNull);
                        CHECK(root->child != NULL);

                        if (root->child != NULL)
                        {
                            /* 0x44, and this is the whole of what the
                             * two flyers' trees differ by. */
                            CHECK(root->child->xobjs[0]->kind == 0x44);
                            CHECK(root->child->xobjs[1]->kind ==
                                  nGCMatrixKindNull);
                            CHECK(root->child->xobjs[2]->kind ==
                                  nGCMatrixKindNull);
                        }
                        CHECK_EQF(root->translate.vec.f.x, pos.x);
                        CHECK_EQF(root->translate.vec.f.y, pos.y);
                        CHECK_EQF(root->translate.vec.f.z, pos.z);
                        CHECK(root->rotate.vec.f.z >= 0.0F);
                        CHECK(root->rotate.vec.f.z <
                              F_CLC_DTOR32(360.0F) + 0.001F);
                    }
                    if (xp != NULL)
                    {
                        /* lr is +1 here where the sparks' test used -1,
                         * so the X comes out the other way. */
                        CHECK_NEAR(xp->effect_vars.damage_fly_mdust.vel.x,
                                   cosf(a) * 50.0F, 0.001F);
                        CHECK_NEAR(xp->effect_vars.damage_fly_mdust.vel.y,
                                   sinf(a) * 50.0F, 0.001F);
                        CHECK_NEAR(xp->effect_vars.damage_fly_mdust.add.x,
                                   -xp->effect_vars.damage_fly_mdust.vel.x
                                   * 0.004F, 0.000001F);
                        CHECK_NEAR(xp->effect_vars.damage_fly_mdust.add.y,
                                   -xp->effect_vars.damage_fly_mdust.vel.y
                                   * 0.004F, 0.000001F);
                        CHECK(xp->effect_vars.damage_fly_mdust.add_timer ==
                              250);

                        /* The shared flyer proc reads the sparks' arm of
                         * the union, which is the same offsets; one tic
                         * of it moves this flyer too. */
                        if (root != NULL)
                        {
                            Vec3f v = xp->effect_vars.damage_fly_mdust.vel;

                            fly->anim_frame = 10.0F;
                            efManagerDamageFlySparksProcUpdate(fly);

                            CHECK_NEAR(root->translate.vec.f.x, pos.x + v.x,
                                       0.001F);
                            CHECK_NEAR(root->translate.vec.f.y, pos.y + v.y,
                                       0.001F);
                            CHECK(xp->effect_vars.damage_fly_mdust.add_timer
                                  == 249);
                        }
                    }
                    efManagerSetPrevStructAlloc(efGetStruct(fly));
                    gcEjectGObj(fly);
                    made++;
                }
            }
            CHECK(made == 3);
            CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
        }
    }
    /* efmanager.c:3812 efManagerQuakeMakeEffect, the one
     * effect in the file with no geometry at all: one DObj, an AnimJoint
     * script driving its translate, and an update that hands that
     * translate to the camera every frame. This build has no pack, so
     * efQuakeAnimJoint returns a table of one NULL and the GObj comes up
     * with no animation -- which is what lets the test write anim_frame
     * itself and drive the proc's two arms by hand.
     *
     * The arithmetic is efmanager.c:3777-3796 and it is worth checking
     * because two things about it read wrongly: the shake's X comes from
     * the DObj's *Z*, and the scale is the battle camera's eye-to-target
     * distance over 6500, so a camera further out than that shakes
     * further in world units to move the same distance on screen. */
    {
        static GObj cam_gobj;
        static CObj cam_cobj;
        GObj *prev_cam = gGMCameraGObj;
        GObj *quake;

        ef_pool_reset();
        memset(&cam_gobj, 0, sizeof(cam_gobj));
        memset(&cam_cobj, 0, sizeof(cam_cobj));
        cam_gobj.obj = &cam_cobj;
        gGMCameraGObj = &cam_gobj;

        quake = efManagerQuakeMakeEffect(2);
        CHECK(quake != NULL);
        CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM - 1);

        if (quake != NULL)
        {
            DObj *d = DObjGetStruct(quake);

            CHECK(d != NULL);
            /* efmanager.c:3859: the update process's priority is the
             * magnitude the other way up, so a bigger quake runs first. */
            CHECK(efGetStruct(quake)->effect_vars.quake.priority == 1);

            if (d != NULL)
            {
                CHECK(d->xobjs[0]->kind == nGCMatrixKindTra);

                d->translate.vec.f.y = 3.0F;
                d->translate.vec.f.z = 5.0F;

                /* Camera 1300 units out: under 6500, so no scaling. */
                cam_cobj.vec.eye.x = 0.0F;
                cam_cobj.vec.eye.y = 0.0F;
                cam_cobj.vec.eye.z = 1300.0F;
                quake->anim_frame = 10.0F;
                efManagerQuakeProcUpdate(quake);
                CHECK_EQF(gGMCameraStruct.vel_at.x, 5.0F);
                CHECK_EQF(gGMCameraStruct.vel_at.y, 3.0F);
                CHECK_EQF(gGMCameraStruct.vel_at.z, 0.0F);

                /* And 13000 out: twice 6500, so twice the shake. */
                cam_cobj.vec.eye.z = 13000.0F;
                quake->anim_frame = 10.0F;
                efManagerQuakeProcUpdate(quake);
                CHECK_NEAR(gGMCameraStruct.vel_at.x, 10.0F, 0.001F);
                CHECK_NEAR(gGMCameraStruct.vel_at.y, 6.0F, 0.001F);

                /* The animation running out is what ends it: the struct
                 * goes back and the GObj is ejected, without anything
                 * else having to know. */
                quake->anim_frame = 0.0F;
                efManagerQuakeProcUpdate(quake);
                CHECK(sEFManagerStructsFreeNum == EFFECT_ALLOC_NUM);
            }
        }
        gGMCameraGObj = prev_cam;
    }
    sEFManagerStructsAllocFree = NULL;
    sEFManagerStructsFreeNum = 0;
}

static void test_particle_draw(void)
{
    static GObj camera_gobj;
    static CObj cobj;
    static XObj xobjs[2];
    GObj *prev_camera = gGCCurrentCamera;
    const f32 near = 100.0F, far = 30000.0F;
    const f32 dists[] = { 200.0F, 500.0F, 1500.0F, 5000.0F, 25000.0F };
    u32 i;

    memset(&cobj, 0, sizeof(cobj));
    memset(&camera_gobj, 0, sizeof(camera_gobj));

    xobjs[0].kind = nGCMatrixKindPerspFastF;
    xobjs[1].kind = 6;

    cobj.xobjs_num = 2;
    cobj.xobjs[0] = &xobjs[0];
    cobj.xobjs[1] = &xobjs[1];
    cobj.projection.persp.near = near;
    cobj.projection.persp.far = far;
    cobj.viewport.vp.vscale[2] = G_MAXZ / 2;
    cobj.viewport.vp.vtrans[2] = G_MAXZ / 2;

    camera_gobj.obj = &cobj;
    gGCCurrentCamera = &camera_gobj;

    gLBPDrawLogging = TRUE;
    gLBPDrawLogCount = 0;

    /* A depth the RDP would have been given, for a point `d` in front of
     * the camera: sys/matrix.c:575-591's third row, then the viewport,
     * then gDPSetPrimDepth's times 32. What comes back has to be 1/d,
     * which is what src/dc/fighter.c gives the PVR for a vertex at the
     * same place. */
    for (i = 0; i < ARRAY_COUNT(dists); i++)
    {
        f32 d = dists[i];
        f32 a = (near + far) / (near - far);
        f32 b = (2.0F * near * far) / (near - far);
        f32 ndc = -a + b / d;
        f32 screen = ndc * (f32)(G_MAXZ / 2) + (f32)(G_MAXZ / 2);

        gLBPDrawLogCount = 0;
        lbpDrawSetRenderMode(G_RM_AA_ZB_XLU_SURF);
        lbpDrawSetPrimDepth((s32)(screen * 32.0F));
        lbpDrawRect(0.0F, 0.0F, 64.0F, 64.0F, 0, 0, 1024, 1024);

        CHECK(gLBPDrawLogCount == 1);
        CHECK(gLBPDrawLog[0].zcmp == TRUE);
        CHECK_NEAR(gLBPDrawLog[0].depth, 1.0F / d, (1.0F / d) * 0.01F);
    }
    /* Without a Z compare the RDP drew over whatever was there, and the
     * PVR sorts its translucent list by depth -- so the quads take the
     * frame's own order, above every 1/w a fighter can write and below
     * the HUD's sprites at LB_SPRITE_Z_BASE. */
    gLBPDrawLogCount = 0;
    lbpDrawSetRenderMode(G_RM_XLU_SURF);

    for (i = 0; i < 4; i++)
    {
        lbpDrawRect(0.0F, 0.0F, 64.0F, 64.0F, 0, 0, 1024, 1024);
    }
    CHECK(gLBPDrawLogCount == 4);
    CHECK(gLBPDrawLog[0].zcmp == FALSE);
    CHECK(gLBPDrawLog[0].depth > 1.0F / near);
    CHECK(gLBPDrawLog[3].depth < LB_SPRITE_Z_BASE);

    for (i = 1; i < 4; i++)
    {
        CHECK(gLBPDrawLog[i].depth > gLBPDrawLog[i - 1].depth);
    }
    /* And the state the macros drive: what a setter said is what the
     * next rectangle carries, which is the RDP's own rule and the reason
     * the renderer emits a texture load only when the texture changes. */
    gLBPDrawLogCount = 0;
    lbpDrawSetPrimColor(1, 2, 3, 4);
    lbpDrawSetEnvColor(5, 6, 7, 8);
    lbpDrawSetBlendColor(0, 0, 0, 9);
    lbpDrawSetAlphaCompare(G_AC_THRESHOLD);
    lbpDrawSetTextureLUT(G_TT_RGBA16);
    lbpDrawLoadTLUT((const void *)0x1234);
    lbpDrawLoadTexture((const void *)0x5678, G_IM_FMT_CI, G_IM_SIZ_4b,
                       32, 16, G_TX_MIRROR, G_TX_CLAMP, 5, 0);
    lbpDrawSetCombineLERP(G_CCMUX_PRIMITIVE, G_CCMUX_ENVIRONMENT,
                          G_CCMUX_TEXEL0, G_CCMUX_ENVIRONMENT);
    lbpDrawRect(-8.0F, -4.0F, 40.0F, 24.0F, 64, 32, 2048, 4096);

    CHECK(gLBPDrawLogCount == 1);

    if (gLBPDrawLogCount == 1)
    {
        const LBPDrawRect *r = &gLBPDrawLog[0];

        CHECK_EQF(r->xl, -8.0F);
        CHECK_EQF(r->yl, -4.0F);
        CHECK_EQF(r->xh, 40.0F);
        CHECK_EQF(r->yh, 24.0F);
        CHECK(r->s == 64 && r->t == 32);
        CHECK(r->dsdx == 2048 && r->dtdy == 4096);
        CHECK(r->prim[0] == 1 && r->prim[1] == 2 && r->prim[2] == 3 &&
              r->prim[3] == 4);
        CHECK(r->env[0] == 5 && r->env[1] == 6 && r->env[2] == 7 &&
              r->env[3] == 8);
        CHECK(r->blend_alpha == 9);
        CHECK(r->ac == G_AC_THRESHOLD);
        CHECK(r->tlut == G_TT_RGBA16);
        CHECK(r->palette == (const void *)0x1234);
        CHECK(r->image == (const void *)0x5678);
        CHECK(r->fmt == G_IM_FMT_CI && r->siz == G_IM_SIZ_4b);
        CHECK(r->width == 32 && r->height == 16);
        CHECK(r->cms == G_TX_MIRROR && r->cmt == G_TX_CLAMP);
        CHECK(r->masks == 5 && r->maskt == 0);
        CHECK(r->combine == nLBPDrawCombineEnvLerp);
    }
    /* The three combiners, told apart by the RGB cycle's first source --
     * which is all lb/lbparticle.c:2043-2069 needs to be told apart by,
     * and is what src/dc/lbpdraw.c reads. */
    lbpDrawSetCombine(nLBPDrawCombineModulateIAPrim);
    gLBPDrawLogCount = 0;
    lbpDrawRect(0.0F, 0.0F, 1.0F, 1.0F, 0, 0, 1024, 1024);
    CHECK(gLBPDrawLog[0].combine == nLBPDrawCombineModulateIAPrim);

    lbpDrawSetCombineLERP(G_CCMUX_NOISE, 0, G_CCMUX_TEXEL0, 0);
    gLBPDrawLogCount = 0;
    lbpDrawRect(0.0F, 0.0F, 1.0F, 1.0F, 0, 0, 1024, 1024);
    CHECK(gLBPDrawLog[0].combine == nLBPDrawCombineNoise);

    gLBPDrawLogging = FALSE;
    gLBPDrawLogCount = 0;
    gGCCurrentCamera = prev_camera;
}

static void test_particle_bank(void)
{
    long size;
    u8 *file, buf[512];
    u32 reads, bytes, reads2, bytes2, n, i, off;

    file = asset_read_whole("efcommon.scb", &size);
    CHECK(file != NULL && size == 0x2AA0);
    if (file == NULL)
        return;

    /* --- src/dc/dma.c: an address is a file and an offset ----------- */
    reads = sy_dma_reads(&bytes);

    /* the head of a segment */
    memset(buf, 0xAB, sizeof(buf));
    syDmaReadRom((uintptr_t)&lEFCommonParticleScriptBankLo, buf, sizeof(buf));
    CHECK(memcmp(buf, file, sizeof(buf)) == 0);

    /* an offset into one: the game reads a whole segment, but the
     * arithmetic is the same one a partial read would use, and only a
     * partial read can tell whether it is there at all */
    memset(buf, 0xAB, sizeof(buf));
    syDmaReadRom((uintptr_t)&lEFCommonParticleScriptBankLo + 0x800, buf,
                 sizeof(buf));
    CHECK(memcmp(buf, file + 0x800, sizeof(buf)) == 0);

    /* the tail, exactly */
    memset(buf, 0xAB, sizeof(buf));
    syDmaReadRom((uintptr_t)&lEFCommonParticleScriptBankHi - sizeof(buf), buf,
                 sizeof(buf));
    CHECK(memcmp(buf, file + size - sizeof(buf), sizeof(buf)) == 0);

    reads2 = sy_dma_reads(&bytes2);
    CHECK(reads2 - reads == 3);
    CHECK(bytes2 - bytes == 3 * sizeof(buf));

    /* Yoshi's Island's bank is a different file, and the address is the
     * only thing that says so (gr/grcommon/gryoster.h:9-12). */
    {
        long ysize;
        u8 *yfile = asset_read_whole("gryoster.txb", &ysize);

        CHECK(yfile != NULL && ysize == 0x230);
        if (yfile != NULL)
        {
            memset(buf, 0xAB, sizeof(buf));
            syDmaReadRom((uintptr_t)&lGRYosterParticleTextureBankLo, buf,
                         sizeof(buf));
            CHECK(memcmp(buf, yfile, sizeof(buf)) == 0);
            free(yfile);
        }
    }

    /* --- and what it refuses ---------------------------------------- */
    reads = sy_dma_reads(&bytes);

    /* an address in no segment: the destination is zeroed rather than
     * left holding the last scene's heap */
    memset(buf, 0xAB, sizeof(buf));
    syDmaReadRom(0x00100000, buf, sizeof(buf));
    for (i = 0; i < sizeof(buf); i++)
        CHECK(buf[i] == 0);

    /* a read that would run out of its segment. On the N64 that is the
     * next bytes of the same ROM; here it is a different file, so it
     * cannot be served and must not be half-served. */
    memset(buf, 0xAB, sizeof(buf));
    syDmaReadRom((uintptr_t)&lEFCommonParticleScriptBankHi - 4, buf,
                 sizeof(buf));
    for (i = 0; i < sizeof(buf); i++)
        CHECK(buf[i] == 0);

    /* neither counted as a read */
    sy_dma_reads(&bytes2);
    CHECK(bytes2 == bytes);
    CHECK(sy_dma_reads(NULL) == reads);

    /* --- the shape lbParticleSetupBankID will find ------------------ */
    memcpy(&n, file, sizeof(n));
    CHECK(n == 119);                    /* efcommon_scb.c's script_0..118 */
    for (i = 0; i < n; i++)
    {
        memcpy(&off, file + 4 + i * 4, sizeof(off));
        /* every offset is inside the block and past the array it is in,
         * which is what makes base + off a pointer into the bank */
        CHECK(off >= 4 + n * 4 && off + 0x30 <= (u32)size);
    }
    free(file);

    file = asset_read_whole("efcommon.txb", &size);
    CHECK(file != NULL && size == 0x4CEA0);
    if (file == NULL)
        return;
    memcpy(&n, file, sizeof(n));
    CHECK(n == 47);                     /* efcommon_txb.c's texture_0..46 */
    for (i = 0; i < n; i++)
    {
        memcpy(&off, file + 4 + i * 4, sizeof(off));
        CHECK(off >= 4 + n * 4 && off + 0x18 <= (u32)size);
    }
    /* texture 25 is the paletted one: 15 I4 frames with a palette each,
     * so its data[] is 30 words and not 15 (efcommon_txb.c:1119-1148).
     * That rule is where tools/export/ssb_particleexport.py was wrong first
     * time -- and the bank still loaded, with the fifteen palettes'
     * offsets left big-endian. */
    {
        u32 hdr[6];

        memcpy(&off, file + 4 + 25 * 4, sizeof(off));
        memcpy(hdr, file + off, sizeof(hdr));
        CHECK(hdr[0] == 15);            /* count */
        CHECK(hdr[1] == G_IM_FMT_CI);   /* fmt */
        CHECK((hdr[5] & 1) == 0);       /* a palette each, not one shared */
        /* the first image lands right after 30 data words */
        memcpy(&off, file + off + 0x18, sizeof(off));
        {
            u32 tx_off;

            memcpy(&tx_off, file + 4 + 25 * 4, sizeof(tx_off));
            CHECK(off == tx_off + 0x18 + 30 * 4);
        }
    }
    free(file);
}
