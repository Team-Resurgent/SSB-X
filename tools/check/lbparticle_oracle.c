/* Host driver for the particle bytecode interpreter, built twice by
 * tools/check/lbparticle_check.py -- once over src/dc/lbparticle.c and once
 * over ssb-decomp-re/src/lb/lbparticle.c -- so the port's copy of that
 * file can be run against the file it was copied from. Same driver, same
 * bank, same seed, same frames; the two traces are compared byte for
 * byte.
 *
 * What runs underneath is the game's: the object system out of
 * ssb-decomp-re/src/sys with the port's src/dc/taskman.c beneath it for
 * the scene heap, the same arrangement tools/check/objanim_oracle.c uses.
 * efParticleInitAll's three allocations are made here at the game's own
 * counts (ef/efparticle.c:30-33) and the two GObj procs the allocators
 * return are called in the order the process list would call them --
 * structs first, generators second, which is the order ef/efparticle.c
 * creates them in.
 *
 * The bank is the port's exported efcommon.scb and .txb (see
 * tools/export/ssb_particleexport.py), installed straight into
 * sLBParticleScriptBanks rather than through lbParticleSetupBankID:
 *
 *   - the scripts need no work at all. LBScript's 0x30-byte prefix holds
 *     nothing wider than four bytes, so the exported file's layout is the
 *     host's, and each script is a pointer into the file image.
 *   - the textures do. LBTexture ends in `void *data[1]`, which is eight
 *     bytes here and four in the file, so a header is copied out into a
 *     host-shaped one. Its data[] is left null: the interpreter reads a
 *     texture's `flags` and nothing else (lbParticleMakeChildScriptID),
 *     and what reads data[] is the renderer, which is not in this file.
 *
 * That is also why lbParticleSetupBankID is not used: its walk turns the
 * file's 32-bit offsets into pointers in place, which cannot be done in a
 * 64-bit process. The target is where that walk is checked
 * (src/dc/scvsbattle.c scVSBattleLoadEffectBank); this checks what runs
 * once it has.
 *
 * Usage: lbparticle_oracle <efcommon.scb> <efcommon.txb> <frames> <seed>
 * Writes one fixed record per live particle per frame on stdout, native
 * byte order; this never runs on the Dreamcast.
 *
 * Also: lbparticle_oracle --readfloat <efcommon.scb>
 * Calls lbParticleReadFloatBigEnd at every offset of the script bank and
 * writes what it returned, four native bytes each, so Python can hold the
 * whole bank to its own `>f`. That leg is the one thing the trace compare
 * cannot see: both of these builds run on a little-endian machine, so a
 * reader that reverses its bytes reverses them the same way twice and the
 * traces agree on a number neither the N64 nor the Dreamcast would have
 * computed. See tools/check/lbparticle_check.py.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ssb_types.h>
#include <sys/obj.h>
#include <sys/debug.h>
#include <sys/utils.h>
#include <lb/library.h>

#include "lbptrace.h"
#include "taskman.h"

/* sys/utils.c:143. The decomp's sys/utils.h declares the seed *pointer*
 * setter and not this one; the driver wants the seed itself, so that
 * every script starts where the last one did. */
extern void syUtilsSetRandomSeed(s32 seed);

/* lb/lbparticle.c's file-scope state. The game declares all of it
 * without `static` and the port's copy keeps that, so a driver can
 * install a bank and walk the live list without adding an accessor to
 * the file under test. */
extern s32 sLBParticleScriptBanksNum[];
extern s32 sLBParticleTextureBanksNum[];
extern LBScript **sLBParticleScriptBanks[];
extern LBTexture **sLBParticleTextureBanks[];
extern LBParticle *sLBParticleStructsAllocLinks[];
extern u16 gLBParticleStructsUsedNum;
extern u16 gLBParticleGeneratorsUsedNum;
extern u16 gLBParticleTransformsUsedNum;

#define BANK_ID 0

/* Both builds are linked with -Wl,--wrap=__sinf -Wl,--wrap=__cosf, so
 * every call the interpreter makes to the game's trig comes through
 * here first and gets counted.
 *
 * This was the one place the port did not compute what the N64
 * computed. src/dc/sysshim.c stood libultra's __sinf and __cosf in with
 * the C library's, and the C library is glibc here and newlib on the
 * Dreamcast -- so a script that called neither was the same on both
 * machines by construction and a script that called either was not, and
 * this count is what told them apart. __sinf and __cosf are libultra's own
 * now (ssb_trigexport.py). The count
 * stays, because it is what says the closure held. The counts go to
 * stderr; stdout is the trace and is compared byte for byte. */
extern f32 __real___sinf(f32 x);
extern f32 __real___cosf(f32 x);

static s32 sTrigCalls;

f32 __wrap___sinf(f32 x) { sTrigCalls++; return __real___sinf(x); }
f32 __wrap___cosf(f32 x) { sTrigCalls++; return __real___cosf(x); }

/* src/dc/fighter.c:969's, restated the way tools/check/objanim_oracle.c
 * restates it: src/dc/objdisplay.c is linked for the object system's
 * sake and reaches for the renderer's viewport, and linking fighter.c
 * for one struct would drag the PVR in behind it. Nothing here draws. */
typedef struct DCViewport { float cx, cy, hw, hh; } DCViewport;
DCViewport gDCViewport = { 320.0f, 240.0f, 320.0f, 240.0f };

#ifdef ORACLE_DECOMP
/* Built over the decomp's file, which carries lbParticleDrawTextures with
 * its real F3DEX2 macros. In --draw mode this is the buffer they write
 * into and what the check decodes; in every other mode nothing calls the
 * renderer and this array only has to resolve. The port has no such array
 * at all (src/dc/ifcommon.c:53 says why). */
Gfx *gSYTaskmanDLHeads[4];
static Gfx sDLBuf[64 * 1024];
#else
#include "lbpdraw.h"
#endif


static void *xmalloc(size_t n)
{
    void *p = calloc(1, n);

    if (p == NULL)
    {
        fprintf(stderr, "lbparticle_oracle: out of memory\n");
        exit(1);
    }
    return p;
}

static u8 *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    u8 *buf;
    long n;

    if (f == NULL)
    {
        fprintf(stderr, "lbparticle_oracle: %s: cannot open\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = xmalloc((size_t)n);

    if (fread(buf, 1, (size_t)n, f) != (size_t)n)
    {
        fprintf(stderr, "lbparticle_oracle: %s: short read\n", path);
        exit(1);
    }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

static u32 le32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void install_bank(const char *scb_path, const char *txb_path)
{
    size_t scb_len, txb_len;
    u8 *scb = slurp(scb_path, &scb_len);
    u8 *txb = slurp(txb_path, &txb_len);
    s32 n, i;

    n = (s32)le32(scb);
    sLBParticleScriptBanksNum[BANK_ID] = n;
    sLBParticleScriptBanks[BANK_ID] = xmalloc((size_t)n * sizeof(LBScript*));

    for (i = 0; i < n; i++)
    {
        sLBParticleScriptBanks[BANK_ID][i] = (LBScript*)(scb + le32(scb + 4 + i * 4));
    }
    n = (s32)le32(txb);
    sLBParticleTextureBanksNum[BANK_ID] = n;
    sLBParticleTextureBanks[BANK_ID] = xmalloc((size_t)n * sizeof(LBTexture*));

    for (i = 0; i < n; i++)
    {
        const u8 *src = txb + le32(txb + 4 + i * 4);
        u32 count = le32(src);
        LBTexture *tx = xmalloc(sizeof(LBTexture) + (size_t)(count * 2 + 1) * sizeof(void*));

        u32 j, data_num;

        tx->count = count;
        tx->fmt = (s32)le32(src + 4);
        tx->siz = (s32)le32(src + 8);
        tx->width = (s32)le32(src + 12);
        tx->height = (s32)le32(src + 16);
        tx->flags = le32(src + 20);

        /* The images, and then the palettes if this is a CI series --
         * one of them if the series shares it, one an image otherwise.
         * lb/lbparticle.c:226-240 lbParticleSetupBankID is the count and
         * the rule; here the file's offsets become pointers into the
         * file image instead of into the reloc block, which is the same
         * arithmetic against a different base.
         *
         * The interpreter needs none of this -- it reads a texture's
         * flags and nothing else -- but the renderer needs all of it,
         * and --draw runs the renderer. */
        data_num = count;

        if (tx->fmt == G_IM_FMT_CI)
        {
            data_num += (tx->flags & 1) ? 1 : count;
        }
        for (j = 0; j < data_num; j++)
        {
            tx->data[j] = (void *)(txb + le32(src + 24 + j * 4));
        }
        sLBParticleTextureBanks[BANK_ID][i] = tx;
    }
}

/* Every offset of the script bank through the reader under test. The
 * cursor it hands back has to be the one it was given plus four -- the
 * bytecode's whole grammar rests on that -- and the float it wrote goes
 * out for tools/check/lbparticle_check.py to compare against
 * struct.unpack(">f"). The header words at the front of the file are in the
 * sweep too: they are not floats, but the reader does not know that and the
 * question is only whether it reads four bytes big-endian. */
static int read_float_scan(const char *scb_path)
{
    size_t len;
    u8 *scb = slurp(scb_path, &len);
    size_t off;

    for (off = 0; off + 4 <= len; off++)
    {
        f32 f;
        u8 *end = lbParticleReadFloatBigEnd(scb + off, &f);

        if (end != scb + off + 4)
        {
            fprintf(stderr, "lbparticle_oracle: reader moved %ld bytes at %lu\n",
                    (long)(end - (scb + off)), (unsigned long)off);
            return 1;
        }
        fwrite(&f, sizeof(f), 1, stdout);
    }
    return 0;
}


/* ---- --draw ---------------------------------------------------------
 *
 * The renderer, run over a scene both builds can make: one camera, one
 * effect GObj, and whatever particles the bank's scripts have alive on
 * the frame. The port's copy of lbParticleDrawTextures ends in
 * src/dc/lbpdraw.h's model of the RDP and the decomp's ends in
 * libultra's own macros, so the two builds cannot write the same bytes
 * and do not try: this one writes a record per rectangle, that one
 * writes the display list it built, and tools/check/lbparticle_check.py
 * decodes the second into the first.
 *
 * That is the only leg that can say anything about the model. The
 * function above it is the decomp's to the character (the text leg), so
 * there is nothing left to check about the arithmetic; what is left is
 * whether eighteen redefined macros read their arguments the way
 * libultra reads them, and this is a display list from one and a
 * struct from the other, field by field.
 *
 * The camera is the driver's own -- the battle camera's viewport and a
 * plain perspective through a look-at -- because any camera does, so
 * long as it is the same camera on both sides. */
static GObj sDrawCameraGObj;
static GObj sDrawEffectGObj;
static CObj sDrawCObj;
static XObj sDrawXObjs[2];

static void draw_scene_make(void)
{
    sDrawXObjs[0].kind = nGCMatrixKindPerspFastF;
    sDrawXObjs[1].kind = 6;             /* LookAt into PROJECTION */

    sDrawCObj.xobjs_num = 2;
    sDrawCObj.xobjs[0] = &sDrawXObjs[0];
    sDrawCObj.xobjs[1] = &sDrawXObjs[1];

    /* sys/rdp.c:68-79 syRdpSetViewport(10, 10, 310, 230) -- the battle
     * camera's 300x220 inside the ten-pixel frame -- written out rather
     * than called, so that sys/rdp.c and the GBI behind it stay out of
     * this link. */
    sDrawCObj.viewport.vp.vscale[0] = (310 - 160) * 4;
    sDrawCObj.viewport.vp.vtrans[0] = 160 * 4;
    sDrawCObj.viewport.vp.vscale[1] = (230 - 120) * 4;
    sDrawCObj.viewport.vp.vtrans[1] = 120 * 4;
    sDrawCObj.viewport.vp.vscale[2] = G_MAXZ / 2;
    sDrawCObj.viewport.vp.vtrans[2] = G_MAXZ / 2;

    sDrawCObj.projection.persp.fovy = 40.0F;
    sDrawCObj.projection.persp.aspect = 300.0F / 220.0F;
    sDrawCObj.projection.persp.near = 100.0F;
    sDrawCObj.projection.persp.far = 30000.0F;
    sDrawCObj.projection.persp.scale = 1.0F;

    sDrawCObj.vec.eye.x = 0.0F;
    sDrawCObj.vec.eye.y = 300.0F;
    sDrawCObj.vec.eye.z = 1500.0F;
    sDrawCObj.vec.at.x = 0.0F;
    sDrawCObj.vec.at.y = 300.0F;
    sDrawCObj.vec.at.z = 0.0F;
    sDrawCObj.vec.up.x = 0.0F;
    sDrawCObj.vec.up.y = 1.0F;
    sDrawCObj.vec.up.z = 0.0F;

    sDrawCameraGObj.obj = &sDrawCObj;
    gGCCurrentCamera = &sDrawCameraGObj;

    /* ef/efdisplay.c gives each of its four GObjs a subset of the
     * sixteen particle link lists; the driver wants every particle it
     * made, so it takes all sixteen. */
    sDrawEffectGObj.camera_mask = ~(u64)0;
}

/* One call's output. The port writes records; the decomp writes the
 * display list it built, words as they lie, for Python to decode. Both
 * are preceded by a count so the check can pair call with call. */
static void draw_emit(void)
{
#ifdef ORACLE_DECOMP
    u32 n;

    gSYTaskmanDLHeads[0] = sDLBuf;

    lbParticleDrawTextures(&sDrawEffectGObj);

    n = (u32)(gSYTaskmanDLHeads[0] - sDLBuf);

    if (n > ARRAY_COUNT(sDLBuf))
    {
        fprintf(stderr, "lbparticle_oracle: display list overran\n");
        exit(1);
    }
    fwrite(&n, sizeof(n), 1, stdout);
    fwrite(sDLBuf, sizeof(sDLBuf[0]), n, stdout);
#else
    u32 n;
    s32 i;

    gLBPDrawLogCount = 0;
    gLBPDrawLogging = TRUE;

    lbParticleDrawTextures(&sDrawEffectGObj);

    n = (u32)gLBPDrawLogCount;
    fwrite(&n, sizeof(n), 1, stdout);

    for (i = 0; i < gLBPDrawLogCount; i++)
    {
        const LBPDrawRect *r = &gLBPDrawLog[i];
        f32 fs[4];
        s32 is[18];
        u8 bs[12];

        fs[0] = r->xl; fs[1] = r->yl; fs[2] = r->xh; fs[3] = r->yh;

        is[0] = r->s;       is[1] = r->t;
        is[2] = r->dsdx;    is[3] = r->dtdy;
        is[4] = r->z;
        is[5] = (s32)(u32)(uintptr_t)r->image;
        is[6] = (s32)(u32)(uintptr_t)r->palette;
        is[7] = r->fmt;     is[8] = r->siz;
        is[9] = r->width;   is[10] = r->height;
        is[11] = r->cms;    is[12] = r->cmt;
        is[13] = r->masks;  is[14] = r->maskt;
        is[15] = r->combine; is[16] = r->ac; is[17] = r->tlut;

        memcpy(bs + 0, r->prim, 4);
        memcpy(bs + 4, r->env, 4);
        bs[8] = r->blend_alpha;
        bs[9] = r->dither_c;
        bs[10] = r->dither_a;
        bs[11] = r->zcmp;

        fwrite(fs, sizeof(fs), 1, stdout);
        fwrite(is, sizeof(is), 1, stdout);
        fwrite(bs, sizeof(bs), 1, stdout);
    }
#endif
}

static void oracle_scene_start(void) {}
static void oracle_scene_update(void) {}
static void oracle_scene_draw(void) {}

static SYTaskmanSetup sOracleSetup =
{
    {
        0, oracle_scene_update, oracle_scene_draw,
        NULL, 0,                        /* keep the heap made below */
        1, 2, 0, 0, 0, 0, 0, 2, 0,      /* N64 display-list plumbing */
        NULL, NULL                      /* lights, controller */
    },

    0, 0, 0, 0,                         /* GObjThreads: none */
    8,                                  /* GObjProcesses */
    8, sizeof(GObj),
    1,                                  /* XObjs: nothing is drawn */
    NULL, NULL,
    1,                                  /* AObjs */
    1,                                  /* MObjs */
    1, sizeof(DObj),
    1, sizeof(SObj),
    1, sizeof(CObj),

    oracle_scene_start
};

int main(int argc, char **argv)
{
    GObj *structs_gobj, *generators_gobj;
    s32 frames, seed, script_id, f;
    s32 draw = 0;

    if (argc == 3 && strcmp(argv[1], "--readfloat") == 0)
    {
        return read_float_scan(argv[2]);
    }
    if (argc == 6 && strcmp(argv[1], "--draw") == 0)
    {
        draw = 1;
        argv++;
        argc--;
    }
    if (argc != 5)
    {
        fprintf(stderr, "usage: %s [--draw] <efcommon.scb> <efcommon.txb>"
                " <frames> <seed>\n", argv[0]);
        return 1;
    }
    frames = atoi(argv[3]);
    seed = atoi(argv[4]);

    if (syTaskmanMakeGeneralHeap(4 * 1024 * 1024) < 0)
    {
        return 1;
    }
    /* The pools and nothing else: syTaskmanStartTask ends in the frame
     * loop and does not return, and this driver runs its own tics. Same
     * split tools/check/objanim_oracle.c uses. */
    syTaskmanSetupPools(&sOracleSetup);

    install_bank(argv[1], argv[2]);

    if (draw)
    {
        draw_scene_make();
    }

    /* ef/efparticle.c:30-33, at the game's counts. */
    structs_gobj = lbParticleAllocStructs(112);
    generators_gobj = lbParticleAllocGenerators(24);
    lbParticleAllocTransforms(80, sizeof(LBTransform));

    if (structs_gobj == NULL || generators_gobj == NULL)
    {
        fprintf(stderr, "lbparticle_oracle: allocators failed\n");
        return 1;
    }
    for (script_id = 0; script_id < sLBParticleScriptBanksNum[BANK_ID]; script_id++)
    {
        /* The same start every time: a known seed, and a particle placed
         * and pushed by the driver rather than by whatever made it in the
         * game, so the only thing that moves is the script. */
        syUtilsSetRandomSeed(seed);

        if (lbParticleMakePosVel(BANK_ID, script_id,
                                 10.0F, 20.0F, 30.0F,
                                 1.0F, 2.0F, 3.0F) == NULL)
        {
            fprintf(stderr, "lbparticle_oracle: script %d would not make\n",
                    script_id);
            return 1;
        }
        for (f = 0; f < frames; f++)
        {
            LBParticle *pc;
            s32 index = 0;

            lbParticleStructFuncRun(structs_gobj);
            lbParticleGeneratorFuncRun(generators_gobj);

            if (draw)
            {
                draw_emit();
                continue;
            }
            for (pc = sLBParticleStructsAllocLinks[0]; pc != NULL; pc = pc->next)
            {
                LBPTraceRec r;

                lbpTraceRec(&r, pc, script_id, f, index++,
                            gLBParticleStructsUsedNum);
                fwrite(&r, sizeof(r), 1, stdout);
            }
        }
        fprintf(stderr, "trig %d %d\n", script_id, sTrigCalls);
        sTrigCalls = 0;

        lbParticleEjectStructAll();
        lbParticleEjectGeneratorAll();

        if (gLBParticleStructsUsedNum != 0 || gLBParticleGeneratorsUsedNum != 0 ||
            gLBParticleTransformsUsedNum != 0)
        {
            fprintf(stderr, "lbparticle_oracle: script %d leaked %u structs, "
                    "%u generators, %u transforms\n", script_id,
                    gLBParticleStructsUsedNum, gLBParticleGeneratorsUsedNum,
                    gLBParticleTransformsUsedNum);
            return 1;
        }
    }
    return 0;
}
