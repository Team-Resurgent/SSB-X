/* db -- the port's debug facility.
 *
 * The game has one of its own: src/db/ in the decomp, an overlay
 * (dbbattle.c, dbmenu.c, dbcube.c, dbfalls.c, dbmaps.c) carrying a
 * debug menu, a fighter picker and a collision viewer, installed at
 * boot and dropped from a shipping build.  This file is that place in
 * this port.  It is *not* a port of those files -- none of the game's
 * debug code is ported yet -- so nothing here carries a decomp name,
 * and everything here is marked as the port's own.
 *
 * What it holds: the collision overlay (-DDB_COLLISION_OVERLAY), the serial log,
 * the cliff warp (-DDB_CLIFF_WARP, and the only thing here bound to a
 * live button), two things a build you PLAY does not want and so two
 * things off by default -- one covers the picture, the other takes a
 * button the game plays with; what is left on is the log, which does
 * neither.  Then the scripted pad that walks the chain unattended, and
 * the
 * boot battle state a build that skips the menus wants. Keeping it
 * out of main.c makes main.c the game's boot: main calls db_install()
 * and nothing else here, and a shipping build drops both that call and this object from the link.
 *
 * The overlay draws collision lines -- green floors (cyan when
 * pass-through, white where a ledge can be grabbed), yellow ceilings,
 * red walls -- read back through the game's own line accessors, so what
 * is drawn is what collision sees.
 *
 * Everything here happens inside the game's frame loop, on a GObj like
 * everything else: scVSBattleStartBattle calls gSCVSBattleFuncDebug
 * once every GObj a battle needs exists and before the first tic, and
 * db_battle_start is what this installs there.
 */
#include <kos.h>
#include <malloc.h>
#include <dc/pvr.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "assetroot.h"
#include "db.h"
#include "perf.h"
#include "dcpvr.h"
#include "fgm.h"
#include "fighter.h"
#include "ftcommon.h"
#include "ftshadow.h"
#include "gmcamera.h"
#include "ifcommon.h"

#include <if/interface.h>       /* gIFCommonPlayerInterface, for the arrows */
#include <if/ifscreenflash.h>   /* the flash, for DB_EFFECT_LIVE 13 */
#include "input.h"
#include "lbcommon.h"           /* lbCommonGetTreeDObjNextFromRoot */
#include <gr/ground.h>          /* gGRCommonStruct, for DB_BONUS2_PROBE */
#include <gr/grvars.h>
#include "mpcommon.h"
#include "objmodel.h"
#include "objpvr.h"
#include "stage.h"
#include "taskman.h"
#include "dctext.h"
#include "vmunotice.h"
#include "stackguard.h"
#include "mtx.h"

#include "scmanager.h"
#include "scvsbattle.h"
#include "scvsresults.h"
#include "mntitle.h"
#include "mnmodeselect.h"
#include "mn1pmode.h"
#include "mnplayers1ptraining.h"
#include "sc1ptrainingmode.h"
#include "mnplayers1pgame.h"
#include "mnplayers1pbonus.h"
#include "sc1pchallenger.h"
#include "sc1pstageclear.h"
#include "sc1pintro.h"
#include "sc1pgame.h"
#include "sc1pbonusstage.h"
#include "mn1pcontinue.h"
#include "mnvsmode.h"
#include "mnvsoptions.h"
#include "mnplayersvs.h"
#include "mnmaps.h"
#include "mndata.h"
#include "mnvsrecord.h"
#include "mncharacters.h"
#include "mnsoundtest.h"
#include "mnoption.h"
#include "mnscreenadjust.h"
#include "mnbackupclear.h"
#include "mncongra.h"
#include "mnnocontroller.h"
#include "dcmemcard.h"
#include "scport.h"
#include "scautodemo.h"
#include "scexplain.h"
#include "mvending.h"
#include "mvopeningroom.h"
#include "mvopeningportraits.h"
#include "mvopeningmario.h"
#include "mvopeningdonkey.h"
#include "mvopeningsamus.h"
#include "mvopeninglink.h"
#include "mvopeningyoshi.h"
#include "mvopeningkirby.h"
#include "mvopeningfox.h"
#include "mvopeningpikachu.h"
#include "mvopeningrun.h"
#include "mvopeningcliff.h"
#include "mvopeningyamabuki.h"
#include "mvopeningjungle.h"
#include "mvopeningyoster.h"
#include "mvopeningsector.h"
#include "mvopeningstandoff.h"
#include "mvopeningclash.h"
#include "mvopeningnewcomers.h"
#include "mnstartup.h"
#include "scstaffroll.h"

#include <sys/obj.h>
#include <arch/rtc.h>
#include <gr/grdef.h>          /* nGRKindHyrule, the battle state's stage */
#include <sc/scdef.h>          /* nSCKindVSBattle */
#include <lb/library.h>
#include <ef/efparticle.h>
#include <ef/effect.h>       /* gEFManagerParticleBankID */
#include <wp/weapon.h>       /* WPStruct, for the -DDB_WP_POOL_PROBE probe */
#include <wp/wpmanager.h>
#include <it/item.h>         /* ITStruct, itGetStruct: -DDB_ITEM_LIVE_PROBE */
#include <it/itmanager.h>    /* itManagerMakeAppearActor */
#include "lbptrace.h"
#include "lbpdraw.h"

/* A player's fighter, or NULL when the slot has none in this scene.
 * fighter_gobj alone does not say that: a sudden death sets the untied
 * players' pkind to nFTPlayerKindNot and never spawns them, so their
 * slots keep the GObj pointer from the match before, into a pool the
 * scene change reset. The game's own code checks pkind first. The db
 * probes did not, and on the console one read a user_data of NULL and
 * faulted on the boot ROM's bytes behind it (hardware VS soak,
 * db_battle_run). Zeroed memory would let the same read go through
 * silently and log garbage. */
static GObj *db_fighter_gobj(int player)
{
    if (gSCManagerBattleState->players[player].pkind == nFTPlayerKindNot)
        return NULL;
    return gSCManagerBattleState->players[player].fighter_gobj;
}

/* ---- -DDB_PARTICLE_TRACE: the particle bank, run on the SH-4 --------
 *
 * src/dc/lbparticle.c is lb/lbparticle.c copied, and
 * tools/check/lbparticle_check.py runs the copy against the original on the
 * build machine, over the same bank, to the byte. What that cannot say
 * is whether the Dreamcast agrees: the same code over the same data, but
 * SH-4 floats, SH-4 alignment, and a bank that came off a disc and
 * through lbParticleSetupBankID's pointerize walk rather than being
 * handed over ready-made.
 *
 * So this replays the whole efcommon bank the way
 * tools/check/lbparticle_oracle.c replays it -- every script in turn, each
 * from the same seed at the same position with the same velocity for
 * the same number of frames, the same two procs in the same order, the
 * same record per live particle per frame (src/dc/lbptrace.h) -- and
 * prints one digest per script and one over all of them. The check tool
 * prints the same numbers on the host; the two are either equal or the
 * interpreter does not travel.
 *
 * Every script, not one, because the bank has state that spans them:
 * dLBParticleCurrentGeneratorID counts every particle ever made and goes
 * into the record, so a run that starts at script 63 numbers its
 * particles differently from one that reaches script 63 having made all
 * the others. Replaying the bank from the top is what makes the two
 * sequences the same sequence.
 *
 * It runs inside one call rather than across the scene's own frames, so
 * nothing about the process order can shift it: the live list is empty
 * before it and empty after, and the scene's own particle proc has
 * nothing to do either side.
 */
#if defined(DB_FB_DUMP) && !defined(FT_HOSTTEST)
/* -DDB_FB_DUMP=f1,f2,... (render frames since boot, ascending): the
 * framebuffer KOS is displaying, halved to 320x240 and written to serial
 * run-length encoded, for tools/check/fbdump.py to turn back into a PNG. What
 * it is for: a visual check when no screen capture is possible (a locked
 * session). The frame is the
 * one on screen after pvr_scene_finish, so it is the last complete one;
 * vram_s and vid_mode are the same pair lbtransition.c's photocopy
 * reads, RGB565.
 *
 * Format, one header and then 240 x 4 chunk lines of 80 pixels each:
 *   fbdump: begin <frame> <scene_curr> 320 240
 *   fbd <row> <chunk> <tokens>   tokens: hhhh one pixel, or
 *                                 rNNhhhh a run of NN (hex) pixels
 *   fbdump: end <frame>
 * Dumping takes a while on serial; the game stalls meanwhile, which is
 * fine for a still.
 *
 * -DDB_FB_DUMP_FULL: the framebuffer's own size instead (640 x 480 at the
 * port's usual scale of 2), the header saying so and each row 8 chunks of
 * 80 -- for measuring what a texture cut from the screen would cost at the
 * resolution it is drawn at. fbdump.py reads the size off the header.
 *
 * -DDB_FB_DUMP_BOX=x,y,w,h (with _FULL; w a multiple of 80): only that box of
 * the full-size frame, in framebuffer pixels. The header adds "box x y" and
 * the full size, and fbdump.py puts the box back on a black canvas of that
 * size. A dump is minutes of serial time, and a card's crowd is a quarter of
 * the screen. */
void db_fb_dump_frame(void)
{
    static const unsigned at[] = { DB_FB_DUMP };
    static unsigned frame, next;
    const uint16_t *fb;
    unsigned fb_w, fb_h, row, chunk, i, out_w, out_h;
    char line[80 * 7 + 32];

    frame++;
    if ((next >= sizeof(at) / sizeof(at[0])) || (frame != at[next]))
    {
        return;
    }
    next++;

    fb = (const uint16_t *)vram_s;
    fb_w = vid_mode->width;
    fb_h = vid_mode->height;

    if ((fb == NULL) || (fb_w < 320) || (fb_h < 240))
    {
        dbglog(DBG_WARNING, "fbdump: no framebuffer at frame %u\n", frame);
        return;
    }
#if defined(DB_FB_DUMP_FULL) && defined(DB_FB_DUMP_BOX)
    static const unsigned box[4] = { DB_FB_DUMP_BOX };

    out_w = box[2];
    out_h = box[3];
    dbglog(DBG_INFO, "fbdump: begin %u %d %u %u box %u %u\n", frame,
           (int)gSCManagerSceneData.scene_curr, fb_w, fb_h, box[0], box[1]);
#else
#ifdef DB_FB_DUMP_FULL
    out_w = fb_w;
    out_h = fb_h;
#else
    out_w = 320;
    out_h = 240;
#endif
    dbglog(DBG_INFO, "fbdump: begin %u %d %u %u\n", frame,
           (int)gSCManagerSceneData.scene_curr, out_w, out_h);
#endif

    for (row = 0; row < out_h; row++)
    {
#if defined(DB_FB_DUMP_FULL) && defined(DB_FB_DUMP_BOX)
        const uint16_t *src = fb + (size_t)(box[1] + row) * fb_w + box[0];
#else
        const uint16_t *src = fb + (size_t)((row * fb_h) / out_h) * fb_w;
#endif

        for (chunk = 0; chunk < out_w / 80; chunk++)
        {
            int n = snprintf(line, sizeof(line), "fbd %u %u ", row, chunk);

            for (i = chunk * 80; i < (chunk + 1) * 80; )
            {
#if defined(DB_FB_DUMP_FULL) && defined(DB_FB_DUMP_BOX)
#define DB_FB_DUMP_X(k) (k)
#else
#define DB_FB_DUMP_X(k) (((k) * fb_w) / out_w)
#endif
                uint16_t px = src[DB_FB_DUMP_X(i)];
                unsigned run = 1;

                while ((i + run < (chunk + 1) * 80) && (run < 255) &&
                       (src[DB_FB_DUMP_X(i + run)] == px))
                {
                    run++;
                }
                if (run > 2)
                {
                    n += snprintf(line + n, sizeof(line) - n, "r%02x%04x", run, px);
                }
                else
                {
                    unsigned k;

                    for (k = 0; k < run; k++)
                    {
                        n += snprintf(line + n, sizeof(line) - n, "%04x", px);
                    }
                }
                i += run;
            }
            dbglog(DBG_INFO, "%s\n", line);
        }
    }
    dbglog(DBG_INFO, "fbdump: end %u\n", frame);
}
#endif

#ifdef DB_PARTICLE_TRACE
#ifndef DB_PARTICLE_FRAMES
#define DB_PARTICLE_FRAMES 120
#endif
#ifndef DB_PARTICLE_SEED
#define DB_PARTICLE_SEED 1
#endif
/* -DDB_PARTICLE_FROM=<frame>: which eight frames of -DDB_PARTICLE_FIRST's
 * script print their particle's bits. The running digest prints for every
 * frame either way, so the first frame that disagrees with the host is one
 * run, and the field it disagrees on is the next. */
#ifndef DB_PARTICLE_FROM
#define DB_PARTICLE_FROM 0
#endif

/* lb/lbparticle.c's live list and bank sizes, at the game's own names. */
extern LBParticle *sLBParticleStructsAllocLinks[];
extern s32 sLBParticleScriptBanksNum[];
extern u16 gLBParticleStructsUsedNum;
extern void syUtilsSetRandomSeed(s32 seed);

static void db_particle_replay(void)
{
    s32 bank_id = gEFManagerParticleBankID;
    u32 whole = 0;
    s32 total = 0;
    s32 script_id;

    if (bank_id < 0 || gEFParticleStructsGObj == NULL)
    {
        dbglog(DBG_INFO, "db: particle trace -- no bank loaded\n");
        return;
    }
    for (script_id = 0; script_id < sLBParticleScriptBanksNum[bank_id]; script_id++)
    {
        u32 acc = 0;
        s32 rows = 0;
        s32 most = 0;
        s32 f;

        syUtilsSetRandomSeed(DB_PARTICLE_SEED);

        if (lbParticleMakePosVel(bank_id, script_id,
                                 10.0F, 20.0F, 30.0F,
                                 1.0F, 2.0F, 3.0F) == NULL)
        {
            dbglog(DBG_INFO, "db: particle script %d would not make\n",
                   (int)script_id);
            return;
        }
        for (f = 0; f < DB_PARTICLE_FRAMES; f++)
        {
            LBParticle *pc;
            s32 index = 0;

            lbParticleStructFuncRun(gEFParticleStructsGObj);
            lbParticleGeneratorFuncRun(gEFParticleGeneratorsGObj);

            for (pc = sLBParticleStructsAllocLinks[0]; pc != NULL; pc = pc->next)
            {
                LBPTraceRec r;

                lbpTraceRec(&r, pc, script_id, f, index++,
                            gLBParticleStructsUsedNum);
                acc = lbpTraceDigest(acc, &r);
                whole = lbpTraceDigest(whole, &r);
                rows++;
#if defined(DB_PARTICLE_FIRST) || defined(DB_PARTICLE_DUMP)
                /* -DDB_PARTICLE_FIRST=<id> prints eight frames of one
                 * script and -DDB_PARTICLE_DUMP every frame of all of
                 * them, in both cases every live particle and the whole
                 * trace record as its bytes, against what
                 * tools/check/lbparticle_check.py --script <id> --from <frame>
                 * and --dump print. Bytes rather than fields because a
                 * serial log's printf is not the place to find out what %f
                 * does to a single-precision vararg, and because a digest
                 * says which frame disagrees while these say which
                 * particle of it and which field. The whole dump is two
                 * megabytes down the serial line and about a minute of
                 * run, which is the price of not guessing. */
#ifdef DB_PARTICLE_DUMP
                if (1)
#else
                if (script_id == DB_PARTICLE_FIRST &&
                    f >= DB_PARTICLE_FROM && f < DB_PARTICLE_FROM + 8)
#endif
                {
                    static const char hex[] = "0123456789abcdef";
                    const u8 *b = (const u8 *)&r;
                    char line[2 * sizeof(r) + 1];
                    s32 k;

                    for (k = 0; k < (s32)sizeof(r); k++)
                    {
                        line[2 * k] = hex[b[k] >> 4];
                        line[2 * k + 1] = hex[b[k] & 0xF];
                    }
                    line[2 * sizeof(r)] = '\0';
                    dbglog(DBG_INFO, "db: particle s%d f%d i%d %s\n",
                           (int)script_id, (int)f, (int)index - 1, line);
                }
#endif
            }
#ifdef DB_PARTICLE_FIRST
            if (script_id == DB_PARTICLE_FIRST)
            {
                dbglog(DBG_INFO, "db: particle f%d %d live, digest 0x%08x\n",
                       (int)f, (int)index, (unsigned)acc);
            }
#endif
            if (index > most)
            {
                most = index;
            }
        }
        lbParticleEjectStructAll();
        lbParticleEjectGeneratorAll();
        total += rows;

        dbglog(DBG_INFO, "db: particle script %3d: %5d particle-frames, "
               "%2d at once, digest 0x%08x\n",
               (int)script_id, (int)rows, (int)most, (unsigned)acc);
    }
    dbglog(DBG_INFO, "db: particle trace -- %d scripts x %d frames from seed "
           "%d, %d particle-frames, digest 0x%08x\n",
           (int)sLBParticleScriptBanksNum[bank_id], (int)DB_PARTICLE_FRAMES,
           (int)DB_PARTICLE_SEED, (int)total, (unsigned)whole);
}
#endif /* DB_PARTICLE_TRACE */

/* ---- -DDB_PARTICLE_LIVE=<script id>: one particle, in the battle ----
 *
 * A way to put a particle on a screen without the game spawning one --
 * the bank's own script, made
 * at player one's feet every -DDB_PARTICLE_LIVE_PERIOD frames with
 * lb/lbparticle.c's own maker, and then left entirely alone. It looks
 * at the renderer alone.
 */
#ifdef DB_PARTICLE_LIVE
#ifndef DB_PARTICLE_LIVE_PERIOD
#define DB_PARTICLE_LIVE_PERIOD 20
#endif

static void db_particle_live(int frame)
{
    const FTStruct *fp;
    Vec3f pos;

    if (gEFManagerParticleBankID < 0 || frame < 2 ||
        (frame % DB_PARTICLE_LIVE_PERIOD) != 0)
    {
        return;
    }
    if (db_fighter_gobj(0) == NULL)
    {
        return;
    }
    fp = ftGetStruct(db_fighter_gobj(0));
    pos = DObjGetStruct(fp->fighter_gobj)->translate.vec.f;

    lbParticleMakePosVel(gEFManagerParticleBankID, DB_PARTICLE_LIVE,
                         pos.x, pos.y, pos.z, 0.0F, 0.0F, 0.0F);

    if ((frame % (DB_PARTICLE_LIVE_PERIOD * 15)) == 0)
    {
        dbglog(DBG_INFO, "db: particle script %d at (%.0f, %.0f, %.0f), "
               "%u live, %d rectangles and %d quads last frame\n",
               (int)DB_PARTICLE_LIVE, pos.x, pos.y, pos.z,
               gLBParticleStructsUsedNum, (int)lbpDrawRectCount(),
               (int)lbpDrawQuadCount());
    }
}
#endif /* DB_PARTICLE_LIVE */

/* ---- -DDB_EFFECT_LIVE=<maker>: one hit effect, in the battle ---------
 *
 * The eleven makers below are the game's own, called by ft/ftmain.c's stat updaters when an attack
 * connects. This is the same idea as DB_PARTICLE_LIVE above and for a
 * different reason: not that nothing spawns one -- something does now --
 * but that a hit landing on a target run is not something a scripted pad
 * can be relied on to arrange, and there is one thing about this path
 * that no screen can show.
 *
 * That thing is the pool. Every maker here takes an EFStruct out of the
 * 38 efManagerInitEffects made, and gives it back either when the
 * particle dies (efManagerDefaultProcDead) or straight away if the
 * particle could not be made. If the give-back were wrong the effects
 * would look perfect for a few seconds and then stop for the rest of the
 * match, which is exactly the failure a screenshot cannot see and this
 * line can: the free count has to come back up.
 */
#ifdef DB_EFFECT_LIVE
#ifndef DB_EFFECT_LIVE_PERIOD
#define DB_EFFECT_LIVE_PERIOD 20
#endif

/* ef/efmanager.c:1717,1720, at the game's own names -- neither is
 * static, in the decomp or in the copy. */
extern EFStruct *sEFManagerStructsAllocFree;
extern s32 sEFManagerStructsFreeNum;
/* if/ifscreenflash.c:11, the same way */
extern GMColAnim sIFScreenFlashColAnim;

static const char *db_effect_live_make(Vec3f *pos, int frame)
{
    s32 damage = 5 + (frame / DB_EFFECT_LIVE_PERIOD) % 20;

    switch (DB_EFFECT_LIVE)
    {
    case 1: efManagerSetOffMakeEffect(pos, damage);            return "SetOff";
    case 2: efManagerDamageFireMakeEffect(pos, damage);        return "DamageFire";
    case 3: efManagerDamageElectricMakeEffect(pos, damage);    return "DamageElectric";
    case 4: efManagerDamageCoinMakeEffect(pos);                return "DamageCoin";
    case 5: efManagerDamageNormalLightMakeEffect(pos, 0, damage, FALSE);
                                                               return "DamageNormalLight";
    case 6: efManagerDamageNormalHeavyMakeEffect(pos, 0, damage);
                                                               return "DamageNormalHeavy";
    case 7: efManagerSparkleWhiteDeadMakeEffect(pos, 5.0F);    return "SparkleWhiteDead";
    /* The first one here that is not a particle: a
     * spawner that lives a dozen tics and makes a billboarded quad every
     * fourth one. ft/ftmain.c:2740 makes one on a hit whose FGM level is
     * above weak, one time in four. */
    case 8: efManagerDamageSpawnOrbsMakeEffect(pos);            return "DamageSpawnOrbs";
    /* The streak, and the first effect in the port whose
     * picture moves -- two MObjs, each stepping a sprite array with its
     * MatAnimJoint. `damage` walks 5..24, which is the whole range of
     * the maker's scale, and the rotation walks with the frame so the
     * streak is seen at every angle rather than one.
     * ft/ftmain.c:1417 makes one on every hit that has a slash. */
    case 9: efManagerDamageSlashMakeEffect(pos, damage,
                 (f32)(frame / DB_EFFECT_LIVE_PERIOD) * 0.4F);  return "DamageSlash";
    /* The spawner, whose eight tics throw three flyers off
     * at +18, 0 and -18 degrees, each stepping a seven-frame sprite
     * array. `lr` alternates so both facings are seen.
     * ft/ftmain.c:1434 makes one on a metal-free heavy hit, one time in
     * four. */
    case 10: efManagerDamageSpawnSparksMakeEffect(pos,
                 ((frame / DB_EFFECT_LIVE_PERIOD) & 1) ? 1 : -1);
                                                               return "DamageSpawnSparks";
    /* The sparks again out of four blocks of its own, and
     * the Make rather than the Random one so every period puts one up
     * instead of one in four. ft/ftmain.c:1438 is the other arm of the
     * switch above 1434: the sparks when the fighter that was hit is not
     * metallic, this when it is. */
    case 11: efManagerDamageSpawnMDustMakeEffect(pos,
                 ((frame / DB_EFFECT_LIVE_PERIOD) & 1) ? 1 : -1);
                                                               return "DamageSpawnMDust";
    /* The shockwave, off relocData file 83's DL rather than
     * a particle -- an 18-vertex fan the AnimJoint spins open over eleven
     * frames as its prim alpha decays. `index` walks 0..2 (the X/Y/Z
     * colour rows -- red, green, blue) with each period, and the rotate
     * walks with the frame so the fan is seen at every angle.
     * ft/ftcommon/ftcommonwalldamage.c (next step) makes one on a
     * wall/ceiling/floor slam at SYVECTOR_AXIS_Z. */
    case 12: efManagerImpactWaveMakeEffect(pos,
                 (frame / DB_EFFECT_LIVE_PERIOD) % 3,
                 (f32)(frame / DB_EFFECT_LIVE_PERIOD) * 0.4F); return "ImpactWave";
    /* Not an effect at all: the screen flash, the wash
     * if/ifscreenflash.c draws over the arena on a KO and on a hit past
     * FTCOMMON_DAMAGE_KNOCKBACK_VERYHIGH. Its five scripts in turn, so
     * every colour is seen: the KO's white swell, then the four hit
     * elements. The state the previous period's script reached is
     * printed first, before this one replaces it. */
    case 13: dbglog(DBG_INFO, "db: screen flash %d: %s, alpha %d\n",
                    (int)sIFScreenFlashColAnim.colanim_id,
                    sIFScreenFlashColAnim.is_use_color1 ? "on" : "off",
                    (int)sIFScreenFlashColAnim.color1.a);
             ifScreenFlashSetColAnimID(nGMColAnimScreenFlashDeadExplode +
                 (frame / DB_EFFECT_LIVE_PERIOD) % 5, 0);      return "ScreenFlash";
    /* A reflector's shards, facing each way in turn. A
     * CPU match seldom breaks a reflector, so this is where the pack is
     * seen to load and draw. */
    case 15: efManagerReflectBreakMakeEffect(pos,
                 ((frame / DB_EFFECT_LIVE_PERIOD) & 1) ? 1 : -1);
                                                               return "ReflectBreak";
    /* The grab swirl. A CPU grab makes one too, but not on
     * a schedule a probe can count on. */
    case 16: efManagerCatchSwirlMakeEffect(pos);                  return "CatchSwirl";
    /* The dead explosion, the last of the model-path
     * effects the port owed and the only one whose colour comes from
     * two places at once -- the primitive out of one of four
     * MatAnimJoints and the environment out of the maker's own tables --
     * so both walk here: the player with each period and the blast line
     * every fourth. ft/ftcommon/ftcommondead.c makes one from each of
     * its three dead statuses. */
    default: efManagerDeadExplodeMakeEffect(pos,
                 (frame / DB_EFFECT_LIVE_PERIOD) % GMCOMMON_PLAYERS_MAX,
                 (u32)((frame / (DB_EFFECT_LIVE_PERIOD * 4)) % 2) * 2 + 1);
                                                               return "DeadExplode";
    }
}

static void db_effect_live(int frame)
{
    const FTStruct *fp;
    const char *name;
    Vec3f pos;

    if (gEFManagerParticleBankID < 0 || frame < 2 ||
        (frame % DB_EFFECT_LIVE_PERIOD) != 0)
    {
        return;
    }
    if (db_fighter_gobj(0) == NULL)
    {
        return;
    }
    fp = ftGetStruct(db_fighter_gobj(0));
    pos = DObjGetStruct(fp->fighter_gobj)->translate.vec.f;
    pos.y += 300.0F;    /* chest height, where a hit would land */

    name = db_effect_live_make(&pos, frame);

    if ((frame % (DB_EFFECT_LIVE_PERIOD * 15)) == 0)
    {
        dbglog(DBG_INFO, "db: effect %s at (%.0f, %.0f, %.0f), %u live, "
               "%d/%d effect structs free, %d rectangles and %d quads "
               "last frame\n",
               name, pos.x, pos.y, pos.z, gLBParticleStructsUsedNum,
               (int)sEFManagerStructsFreeNum, (int)EFFECT_ALLOC_NUM,
               (int)lbpDrawRectCount(), (int)lbpDrawQuadCount());
    }
}
#endif /* DB_EFFECT_LIVE */

/* ---- -DDB_QUAKE_TRACE=<lines>: the screen shake, in numbers ---------
 *
 * The quake draws nothing. It moves the camera, and it
 * moves it by a few units for about a second, which is the one thing a
 * screenshot cannot be held to: a KO pans and zooms the camera anyway,
 * so a pair of frames taken a fifth of a second apart cannot tell the
 * shake from the pan.
 *
 * What can tell them apart is the number itself.
 * efManagerQuakeProcUpdate is the only thing in the port that calls
 * gmCameraSetVelAt, so every non-zero gGMCameraStruct.vel_at is one
 * frame of one quake -- the AnimJoint script's own translate, scaled by
 * the battle camera's distance over 6500. This prints the first
 * DB_QUAKE_TRACE of them and then stops.
 */
#ifdef DB_QUAKE_TRACE
static void db_quake_trace(int frame)
{
    static int left = DB_QUAKE_TRACE;
    const Vec3f *v = &gGMCameraStruct.vel_at;

    if (left <= 0 || (v->x == 0.0F && v->y == 0.0F))
    {
        return;
    }
    dbglog(DBG_INFO, "db: quake frame %d, camera vel_at (%.2f, %.2f, %.2f)\n",
           frame, v->x, v->y, v->z);
    left--;
}
#endif /* DB_QUAKE_TRACE */

/* ---- -DDB_MARIO_SPECIALN_PROBE: Mario's Neutral-B on a real fighter ---
 *
 * Ports ft/ftchar/ftmario/ftmariospecialn.c unmodified. Its
 * ProcAccessory is the fireball's caller; the host test drives it on the
 * mock fighter but has to empty the stage first, because the fresh
 * weapon's COLLPROJECT inverts a still-identity-less transform against the
 * floor and divides by zero. This is the one thing the host cannot show:
 * the live projection on a real stage. Once, when a Mario player's parts
 * tree is built (the boot battle stands P2 as Mario), raise the
 * SpawnFireball flag the animation event would, run the accessory, and
 * read back the fireball it put on the weapon link -- its item index and
 * where it landed. It is left to live: wpProcess flies it as the match's
 * own projectile. The status table that would enter Special-N from a real
 * B-press is a later step; this reaches the proc directly. */
#ifdef DB_MARIO_SPECIALN_PROBE
static void db_mario_specialn_probe(int frame)
{
    extern void ftMarioSpecialNProcAccessory(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    GObj *wg;
    Vec3f jp;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);
        s32 k;

        if (g == NULL)
        {
            continue;
        }
        k = ftGetStruct(g)->fkind;
        if (k == nFTKindMario || k == nFTKindLuigi || k == nFTKindMMario ||
            k == nFTKindNMario || k == nFTKindNLuigi)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: specialn -- no Mario player in the battle\n");
        return;
    }

    jp.x = jp.y = jp.z = 0.0F;
    gmCollisionGetFighterPartsWorldPosition(
        fp->joints[FTMARIO_FIREBALL_SPAWN_JOINT], &jp);

    /* The boot scene reaches this without scVSBattle's weapon-pool setup,
     * so the pool can still be empty (as the step-26 model probe found);
     * cut it from the live scene heap if so, or wpMarioFireballMakeWeapon
     * has no free WPStruct and hands back NULL. A real match allocates it
     * on its own. Take one to test, then put it back or allocate. */
    {
        WPStruct *free_struct = wpManagerGetNextStructAlloc();

        if (free_struct == NULL)
        {
            wpManagerAllocWeapons();
        }
        else
        {
            wpManagerSetPrevStructAlloc(free_struct);
        }
    }

    fp->motion_vars.flags.flag0 = TRUE;      /* the SpawnFireball event */
    ftMarioSpecialNProcAccessory(fighter_gobj);

    wg = gGCCommonLinks[nGCCommonLinkIDWeapon];
    dbglog(DBG_INFO,
           "db: specialn -- flag0 %d (want 0) spawned %d index %d joint "
           "(%.0f,%.0f,%.0f) fireball (%.0f,%.0f,%.0f)\n",
           (int)fp->motion_vars.flags.flag0,
           (int)(wg != NULL),
           (wg != NULL) ? (int)wpGetStruct(wg)->weapon_vars.fireball.index : -1,
           jp.x, jp.y, jp.z,
           (wg != NULL) ? DObjGetStruct(wg)->translate.vec.f.x : 0.0F,
           (wg != NULL) ? DObjGetStruct(wg)->translate.vec.f.y : 0.0F,
           (wg != NULL) ? DObjGetStruct(wg)->translate.vec.f.z : 0.0F);
}
#endif /* DB_MARIO_SPECIALN_PROBE */

/* ---- -DDB_MARIO_FIREBALL_PALETTE_PROBE: the fireball's palette select --
 *
 * Teaches src/dc/objmodel.c's dc_joint_material to read a
 * PALETTE-flagged MObj's frame off palette_id instead of texture_id_curr,
 * and tools/export/ssb_effectexport.py to bake both of the fireball's palettes
 * (Mario's orange, Luigi's green) as the same tex_count-2 array the damage
 * slash's sprite-array frames already use. wpMarioFireballMakeWeapon's
 * `dobj->mobj->palette_id = anim_frame` write is verbatim; it
 * must land on a live MObj the renderer looks at, or every fireball
 * draws Mario's baked-in orange no matter which fkind threw it.
 *
 * Any player in the boot battle can carry it (the weapon's model is its
 * own pack, independent of the owner's) -- find one and call
 * wpMarioFireballMakeWeapon directly with index 0 then 1, as
 * DB_MARIO_FIREBALL_MODEL_PROBE already calls wpManagerMakeWeapon
 * directly (the fkind switch that would pick the index from a real
 * B-press is already host-tested, test_ft_mario_specialn_accessory; this
 * probe is only the renderer's half). Both fireballs are left flying:
 * that they come out two different colours, not one, is the on-screen
 * check. */
#ifdef DB_MARIO_FIREBALL_PALETTE_PROBE
static void db_mario_fireball_palette_probe(int frame)
{
    extern GObj* wpMarioFireballMakeWeapon(GObj *fighter_gobj, Vec3f *pos,
                                           s32 index);
    extern void *gFTMarioFileSpecial1;
    extern void *gFTDataLuigiSpecial1;
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    GObj *wg0, *wg1;
    Vec3f pos;
    float pal0 = -1.0F, pal1 = -1.0F;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: fireball-palette -- no player in the battle\n");
        return;
    }

    /* wpMarioFireballMakeWeapon reads the collision attributes off
     * whichever of gFTMarioFileSpecial1/gFTDataLuigiSpecial1 the index
     * names. Both are bound to their baked files by src/dc/wpattrs.c at
     * overlay load so only check they are. */
    if (gFTMarioFileSpecial1 == NULL || gFTDataLuigiSpecial1 == NULL)
    {
        dbglog(DBG_INFO, "db: fireball-palette -- a Special1 base is NULL\n");
        return;
    }

    {
        WPStruct *free_struct = wpManagerGetNextStructAlloc();

        if (free_struct == NULL)
        {
            wpManagerAllocWeapons();
        }
        else
        {
            wpManagerSetPrevStructAlloc(free_struct);
        }
    }

    pos.x = pos.y = pos.z = 0.0F;
    gmCollisionGetFighterPartsWorldPosition(
        fp->joints[FTMARIO_FIREBALL_SPAWN_JOINT], &pos);

    /* Called directly, as DB_MARIO_FIREBALL_MODEL_PROBE calls
     * wpManagerMakeWeapon directly: the status table that would reach
     * this from a real B-press is a later step, and the accessory's own
     * fkind switch is already host-tested (test_ft_mario_specialn_
     * accessory). This probe is only the renderer's half -- that the
     * MObj each index's weapon carries is the one the fkind switch
     * would have picked -- so it drives the index straight through. */
    wg0 = wpMarioFireballMakeWeapon(fighter_gobj, &pos, 0);   /* Mario, orange */
    if (wg0 != NULL)
    {
        pal0 = DObjGetStruct(wg0)->mobj->palette_id;
    }
    wg1 = wpMarioFireballMakeWeapon(fighter_gobj, &pos, 1);   /* Luigi, green */
    if (wg1 != NULL)
    {
        pal1 = DObjGetStruct(wg1)->mobj->palette_id;
    }

    dbglog(DBG_INFO,
           "db: fireball-palette -- spawned %d %d palette_id %.1f %.1f "
           "want 1 1 0.0 1.0\n",
           (int)(wg0 != NULL), (int)(wg1 != NULL),
           (double)pal0, (double)pal1);
}
#endif /* DB_MARIO_FIREBALL_PALETTE_PROBE */

/* ---- -DDB_FOX_BLASTER_PROBE: Fox's Blaster shot, the wp/ frontier's
 * second directly-compiled weapon file ----
 *
 * Ports wp/wpfox/wpfoxblaster.c (compiled straight from the
 * decomp, same as wpmariofireball.c) and efManagerFoxBlasterGlowMakeEffect
 * (src/dc/efmanager.c); the caller
 * is ft/ftchar/ftfox/ftfoxspecialn.c with the demux filled, and the
 * model (relocData 316's flat SHADE quad, romdisk/wpfoxblaster.mdl) has
 * a sWPManagerModels row. All three land here: this probe
 * spawns a real shot off the boot Fox (P1) directly through
 * wpFoxBlasterMakeWeapon and checks the whole wp/ chain ran -- pool take,
 * weapon GObj, DObj tree -- the same shape DB_MARIO_FIREBALL_MODEL_PROBE
 * checked for the fireball. Unlike the fireball, Fox's
 * WPAttributes carries p_mobjsubs=NULL (no MObj at all -- the DL's own
 * vertex colours are the whole material), so there is no MObj to check,
 * only the DObj. Ejected after each spawn so the pool struct comes back;
 * called every 30 frames on the boot Fox. */
#ifdef DB_FOX_BLASTER_PROBE
static void db_fox_blaster_probe(int frame)
{
    extern GObj* wpFoxBlasterMakeWeapon(GObj *fighter_gobj, Vec3f *pos);
    extern void wpMainDestroyWeapon(GObj *weapon_gobj);
    extern void *gFTDataFoxSpecial1;
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    WPStruct *free_struct;
    GObj *wg;
    DObj *root;
    Vec3f pos;
    int player;

    if (frame % 30 != 0)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindFox)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: fox-blaster -- no Fox player in the battle\n");
        return;
    }

    /* Off the real table: gFTDataFoxSpecial1 is bound to the baked file
     * by src/dc/wpattrs.c at overlay load. */
    if (gFTDataFoxSpecial1 == NULL)
    {
        dbglog(DBG_INFO, "db: fox-blaster -- gFTDataFoxSpecial1 is NULL\n");
        return;
    }

    pos.x = pos.y = pos.z = 0.0F;
    gmCollisionGetFighterPartsWorldPosition(fp->joints[FTFOX_BLASTER_HOLD_JOINT],
                                             &pos);

    free_struct = wpManagerGetNextStructAlloc();
    if (free_struct == NULL)
    {
        wpManagerAllocWeapons();
    }
    else
    {
        wpManagerSetPrevStructAlloc(free_struct);
    }

    wg = wpFoxBlasterMakeWeapon(fighter_gobj, &pos);
    root = (wg != NULL) ? DObjGetStruct(wg) : NULL;

    dbglog(DBG_INFO,
           "db: fox-blaster -- frame %d spawned %d dobj %d want 1 1\n",
           frame, (int)(wg != NULL), (int)(root != NULL));

    if (wg != NULL)
    {
        wpMainDestroyWeapon(wg);        /* returns the pool struct */
    }
}
#endif /* DB_FOX_BLASTER_PROBE */

/* ---- -DDB_MARIO_SPECIALHI_PROBE: Mario's Up-B procs on a real fighter ---
 *
 * Ports ft/ftchar/ftmario/ftmariospecialhi.c unmodified -- the
 * last of Mario's three special-move files, so the status table can be
 * assembled next. The whole file is dead-stripped until that table names its
 * procs, and the SetStatus entry points cannot be exercised yet: they route
 * through ftMainSetStatus into dFTMarioSpecialStatusDescs, which is still the
 * zero stub (populating it is the next step). What IS reachable now, on the
 * SH-4, table-free: ftMarioSpecialHiInitStatusVars (clears the two motion
 * flags, no callees) and ftMarioSpecialHiProcPhysics (reaches only ftPhysics*
 * air/gravity helpers, all already in the port). Once, on the boot Mario,
 * clear-then-check the flags, then run one frame of the air-drift physics
 * branch (is_air_bool + flag1) over a known vel_air and log before/after --
 * a real execution of the ported recovery physics on hardware. Everything is
 * saved and restored so the live fighter is untouched afterward. */
#ifdef DB_MARIO_SPECIALHI_PROBE
static void db_mario_specialhi_probe(int frame)
{
    extern void ftMarioSpecialHiInitStatusVars(GObj *fighter_gobj);
    extern void ftMarioSpecialHiProcPhysics(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    int player;
    Vec3f saved_vel;
    s32 saved_ga, saved_air_bool;
    u32 saved_flag1, saved_flag2;
    u32 init_flag1, init_flag2;

    if (frame != 60)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);
        s32 k;

        if (g == NULL)
        {
            continue;
        }
        k = ftGetStruct(g)->fkind;
        if (k == nFTKindMario || k == nFTKindLuigi || k == nFTKindMMario ||
            k == nFTKindNMario || k == nFTKindNLuigi)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: specialhi -- no Mario player in the battle\n");
        return;
    }

    /* save everything the two procs touch */
    saved_vel = fp->physics.vel_air;
    saved_ga = fp->ga;
    saved_air_bool = fp->status_vars.mario.specialhi.is_air_bool;
    saved_flag1 = fp->motion_vars.flags.flag1;
    saved_flag2 = fp->motion_vars.flags.flag2;

    /* (1) InitStatusVars: seed both flags nonzero, clear, read back */
    fp->motion_vars.flags.flag1 = 1;
    fp->motion_vars.flags.flag2 = 1;
    ftMarioSpecialHiInitStatusVars(fighter_gobj);
    init_flag1 = fp->motion_vars.flags.flag1;   /* capture before the physics
                                                 * branch overwrites flag1 */
    init_flag2 = fp->motion_vars.flags.flag2;

    /* (2) ProcPhysics falling branch: is_air_bool set, flag1 clear -> pure
     * gravity + air-X friction (ftPhysicsApplyGravityClampTVel then the
     * ClampAirVelXDecMax/Friction pair), no trans-N joint read, so the result
     * is fully predictable: gravity pulls vel_air.y down from +100, friction
     * shrinks vel_air.x from +100, and z is left untouched at 0. */
    fp->status_vars.mario.specialhi.is_air_bool = TRUE;
    fp->motion_vars.flags.flag1 = 0;
    fp->ga = nMPKineticsAir;
    fp->physics.vel_air.x = 100.0F;
    fp->physics.vel_air.y = 100.0F;
    fp->physics.vel_air.z = 0.0F;
    ftMarioSpecialHiProcPhysics(fighter_gobj);

    dbglog(DBG_INFO,
           "db: specialhi -- flags after init %u/%u (want 0/0) "
           "vel_air (%.2f,%.2f,%.2f) from (100,100,0): y fell %d x shrank %d "
           "z still 0 %d\n",
           (unsigned)init_flag1,
           (unsigned)init_flag2,
           fp->physics.vel_air.x, fp->physics.vel_air.y, fp->physics.vel_air.z,
           (int)(fp->physics.vel_air.y < 100.0F),
           (int)(fp->physics.vel_air.x < 100.0F),
           (int)(fp->physics.vel_air.z == 0.0F));

    /* restore the live fighter */
    fp->physics.vel_air = saved_vel;
    fp->ga = saved_ga;
    fp->status_vars.mario.specialhi.is_air_bool = saved_air_bool;
    fp->motion_vars.flags.flag1 = saved_flag1;
    fp->motion_vars.flags.flag2 = saved_flag2;
}
#endif /* DB_MARIO_SPECIALHI_PROBE */

/* ---- -DDB_FALLSPECIAL_PROBE: the recovery-fall physics on a real fighter --
 *
 * Ports ft/ftcommon/ftcommonfallspecial.c unmodified -- the first
 * of the ftcommon closure the Mario special-move status table pulls in. It is
 * the helpless-fall state the three specials' ProcMap route to. The whole file
 * is dead-stripped until that table names its procs; its SetStatus/ProcMap
 * reach unported ftcommon handoffs (jumpaerial's interrupt tree, cliffcatch,
 * landing, wait) and cannot run yet. What IS reachable now, table-free, is
 * ftCommonFallSpecialProcPhysics: it calls only ftPhysics* gravity/air-drift
 * helpers, all already in the port. Once, on the boot Mario, seed a known
 * vel_air and the fall-special status vars, run one frame of ProcPhysics in
 * air, and log before/after -- a real execution of the ported recovery-fall
 * maths on the SH-4. Gravity pulls vel_air.y down whichever branch runs
 * (fastfall, accelerate, or clamp-tvel), and z is left untouched. Everything
 * is saved and restored so the live fighter is unchanged afterward. */
#ifdef DB_FALLSPECIAL_PROBE
static void db_fallspecial_probe(int frame)
{
    extern void ftCommonFallSpecialProcPhysics(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    int player;
    Vec3f saved_vel;
    s32 saved_ga;
    u32 saved_fastfall;
    f32 saved_drift;
    s32 saved_fall_accel;

    if (frame != 60)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);
        s32 k;

        if (g == NULL)
        {
            continue;
        }
        k = ftGetStruct(g)->fkind;
        if (k == nFTKindMario || k == nFTKindLuigi || k == nFTKindMMario ||
            k == nFTKindNMario || k == nFTKindNLuigi)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: fallspecial -- no Mario player in the battle\n");
        return;
    }

    /* save everything ProcPhysics touches */
    saved_vel = fp->physics.vel_air;
    saved_ga = fp->ga;
    saved_fastfall = fp->is_fastfall;
    saved_drift = fp->status_vars.common.fallspecial.drift;
    saved_fall_accel = fp->status_vars.common.fallspecial.is_fall_accelerate;

    /* ProcPhysics in air: not fast-falling, not accelerating (so the
     * clamp-to-terminal gravity branch runs), zero drift so the air-X
     * friction path shrinks vel_air.x. Gravity always pulls vel_air.y down;
     * z is never touched. */
    fp->ga = nMPKineticsAir;
    fp->is_fastfall = FALSE;
    fp->status_vars.common.fallspecial.is_fall_accelerate = FALSE;
    fp->status_vars.common.fallspecial.drift = 0.0F;
    fp->physics.vel_air.x = 100.0F;
    fp->physics.vel_air.y = 100.0F;
    fp->physics.vel_air.z = 0.0F;
    ftCommonFallSpecialProcPhysics(fighter_gobj);

    dbglog(DBG_INFO,
           "db: fallspecial -- ProcPhysics vel_air (%.2f,%.2f,%.2f) from "
           "(100,100,0): y fell %d z still 0 %d\n",
           fp->physics.vel_air.x, fp->physics.vel_air.y, fp->physics.vel_air.z,
           (int)(fp->physics.vel_air.y < 100.0F),
           (int)(fp->physics.vel_air.z == 0.0F));

    /* restore the live fighter */
    fp->physics.vel_air = saved_vel;
    fp->ga = saved_ga;
    fp->is_fastfall = saved_fastfall;
    fp->status_vars.common.fallspecial.drift = saved_drift;
    fp->status_vars.common.fallspecial.is_fall_accelerate = saved_fall_accel;
}
#endif /* DB_FALLSPECIAL_PROBE */

/* ---- -DDB_PASSCLIFF_PROBE: the airborne pass/ledge walk on a real fighter --
 *
 * Finishes ftCommonFallSpecial's closure: the two callees its
 * ProcMap still lacked, mpCommonCheckFighterPassCliff (src/dc/mpcommon.c) and
 * ftCommonLandingFallSpecialSetStatus (src/dc/ftcommon.c). Porting PassCliff
 * meant restoring a ternary the port had DIVERGED away: mpCommonRun-
 * FighterSpecialCollisions again takes the pass-aware floor check
 * (mpProcessCheckTestFloorCollisionAdjNew, threading the per-status proc_map
 * through the sMPCommonProcPass file-static) whenever MAP_PROC_TYPE_PASS is
 * set, instead of always the NULL form. That branch had never run on the
 * SH-4. Here, once, on the boot Mario, call mpCommonCheckFighterPassCliff with
 * a proc_map that only tallies its calls and refuses every floor (returns
 * FALSE): the tally going up proves the floor check reached the pass proc
 * through the restored ternary -- the NULL form never calls a proc -- and the
 * FALSE return means no landing/cliff setter mutates the fighter. The walk
 * writes the fighter's position through coll_data->p_translate, so that and
 * the whole coll_data are saved and restored; the live fighter is unchanged. */
#ifdef DB_PASSCLIFF_PROBE
static int s_passcliff_proc_calls;

static sb32 db_passcliff_refuse(GObj *fighter_gobj)
{
    (void)fighter_gobj;
    s_passcliff_proc_calls++;
    return FALSE;
}

static void db_passcliff_probe(int frame)
{
    extern sb32 mpCommonCheckFighterPassCliff(GObj *fighter_gobj, sb32 (*proc_map)(GObj*));
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    int player;
    MPCollData saved_coll;
    Vec3f saved_translate;
    sb32 result;

    if (frame != 60)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);
        s32 k;

        if (g == NULL)
        {
            continue;
        }
        k = ftGetStruct(g)->fkind;
        if (k == nFTKindMario || k == nFTKindLuigi || k == nFTKindMMario ||
            k == nFTKindNMario || k == nFTKindNLuigi)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: passcliff -- no Mario player in the battle\n");
        return;
    }

    /* the walk moves the fighter through coll_data->p_translate; save it and
     * the whole coll_data so the live fighter is untouched afterward */
    saved_coll = fp->coll_data;
    saved_translate = *fp->coll_data.p_translate;

    /* At frame 60 Mario stands on a floor, so its live position is on one.
     * Seed a downward sweep straddling that position -- pos_prev 50 above,
     * a -100 pos_diff, no stage velocity -- so mpProcessUpdateMain sweeps
     * (+50 .. -50) across the floor line and the pass floor check reaches
     * the proc. (All of this is inside the saved/restored coll_data.) */
    fp->coll_data.pos_prev = saved_translate;
    fp->coll_data.pos_prev.y += 50.0F;
    fp->coll_data.pos_diff.x = 0.0F;
    fp->coll_data.pos_diff.y = -100.0F;
    fp->coll_data.pos_diff.z = 0.0F;
    fp->coll_data.vel_speed.x = fp->coll_data.vel_speed.y = fp->coll_data.vel_speed.z = 0.0F;
    fp->coll_data.vel_push.x = fp->coll_data.vel_push.y = fp->coll_data.vel_push.z = 0.0F;
    fp->coll_data.is_coll_end = FALSE;

    s_passcliff_proc_calls = 0;
    result = mpCommonCheckFighterPassCliff(fighter_gobj, db_passcliff_refuse);

    dbglog(DBG_INFO,
           "db: passcliff -- CheckFighterPassCliff returned %d, pass proc "
           "reached %d time(s): pass-branch ran %d\n",
           (int)result, s_passcliff_proc_calls,
           (int)(s_passcliff_proc_calls > 0));

    /* restore the live fighter */
    *fp->coll_data.p_translate = saved_translate;
    fp->coll_data = saved_coll;
}
#endif /* DB_PASSCLIFF_PROBE */

/* ---- -DDB_SPECIALN_TABLE_PROBE: Mario's fireball status rows on the SH-4 --
 *
 * Fills dFTMarioSpecialStatusDescs' SpecialN (223) and
 * SpecialAirN (224) rows, holes since the table was born. ftMainSetStatus
 * dispatches a special id through this table (per fighter, via
 * dFTMainSpecialStatusDescs[Mario]), so a hole meant a B-press reaching
 * SpecialN installed NULL callbacks and did nothing. The procs themselves
 * (ftmariospecialn.c, compiled unmodified) were already proven live on the
 * SH-4 by DB_MARIO_SPECIALN_PROBE, which runs the fireball's accessory; what
 * this step adds is the two rows, so what this checks is that they reached
 * RAM whole -- the four function pointers the linker resolved, and the
 * transcribed motion/kinetics/projectile flags. A pure read of a global;
 * nothing is mutated. */
#ifdef DB_SPECIALN_TABLE_PROBE
static void db_specialn_table_probe(int frame)
{
    extern FTStatusDesc dFTMarioSpecialStatusDescs[];
    extern void ftMarioSpecialNProcUpdate(GObj*);
    extern void ftMarioSpecialNProcMap(GObj*);
    extern void ftMarioSpecialAirNProcMap(GObj*);
    extern void ftPhysicsApplyGroundVelFriction(GObj*);
    extern void ftPhysicsApplyAirVelDrift(GObj*);
    FTStatusDesc *n   = &dFTMarioSpecialStatusDescs[nFTMarioStatusSpecialN - nFTCommonStatusSpecialStart];
    FTStatusDesc *air = &dFTMarioSpecialStatusDescs[nFTMarioStatusSpecialAirN - nFTCommonStatusSpecialStart];
    int n_ok, air_ok;

    if (frame != 60)
    {
        return;
    }

    n_ok = (n->proc_update == ftMarioSpecialNProcUpdate) &&
           (n->proc_physics == ftPhysicsApplyGroundVelFriction) &&
           (n->proc_map == ftMarioSpecialNProcMap) &&
           (n->proc_interrupt == NULL) &&
           (n->mflags.motion_id == nFTMarioMotionSpecialN) &&
           (n->sflags.ga == nMPKineticsGround) &&
           (n->sflags.is_projectile == TRUE);

    air_ok = (air->proc_update == ftMarioSpecialNProcUpdate) &&
             (air->proc_physics == ftPhysicsApplyAirVelDrift) &&
             (air->proc_map == ftMarioSpecialAirNProcMap) &&
             (air->proc_interrupt == NULL) &&
             (air->mflags.motion_id == nFTMarioMotionSpecialAirN) &&
             (air->sflags.ga == nMPKineticsAir) &&
             (air->sflags.is_projectile == TRUE);

    dbglog(DBG_INFO,
           "db: specialn-table -- SpecialN row ok %d (motion %d ga %d proj %d), "
           "SpecialAirN row ok %d (motion %d ga %d proj %d)\n",
           n_ok, (int)n->mflags.motion_id, (int)n->sflags.ga, (int)n->sflags.is_projectile,
           air_ok, (int)air->mflags.motion_id, (int)air->sflags.ga, (int)air->sflags.is_projectile);
}
#endif /* DB_SPECIALN_TABLE_PROBE */

/* ---- -DDB_SPECIALHI_TABLE_PROBE: Mario's Up-B status rows on the SH-4 -----
 *
 * Promotes ftmariospecialhi.c to host+target and filled the
 * SpecialHi (225) and SpecialAirHi (226) rows -- the last two holes in
 * dFTMarioSpecialStatusDescs, so Mario's four specials are now all
 * table-present. The Super Jump Punch is the first ported status with a
 * non-NULL ProcInterrupt, so this reads all four function pointers plus the
 * motion/kinetics/projectile flags, confirming the rows reached RAM whole
 * and the linker resolved the newly-named procs (they were dead-stripped
 * until this table named them). A pure read of a global; nothing mutated. */
#ifdef DB_SPECIALHI_TABLE_PROBE
static void db_specialhi_table_probe(int frame)
{
    extern FTStatusDesc dFTMarioSpecialStatusDescs[];
    extern void ftMarioSpecialHiProcUpdate(GObj*);
    extern void ftMarioSpecialHiProcInterrupt(GObj*);
    extern void ftMarioSpecialHiProcPhysics(GObj*);
    extern void ftMarioSpecialHiProcMap(GObj*);
    FTStatusDesc *hi  = &dFTMarioSpecialStatusDescs[nFTMarioStatusSpecialHi - nFTCommonStatusSpecialStart];
    FTStatusDesc *air = &dFTMarioSpecialStatusDescs[nFTMarioStatusSpecialAirHi - nFTCommonStatusSpecialStart];
    int hi_ok, air_ok;

    if (frame != 60)
    {
        return;
    }

    hi_ok = (hi->proc_update == ftMarioSpecialHiProcUpdate) &&
            (hi->proc_interrupt == ftMarioSpecialHiProcInterrupt) &&
            (hi->proc_physics == ftMarioSpecialHiProcPhysics) &&
            (hi->proc_map == ftMarioSpecialHiProcMap) &&
            (hi->mflags.motion_id == nFTMarioMotionSpecialHi) &&
            (hi->sflags.ga == nMPKineticsGround) &&
            (hi->sflags.is_projectile == FALSE);

    air_ok = (air->proc_update == ftMarioSpecialHiProcUpdate) &&
             (air->proc_interrupt == ftMarioSpecialHiProcInterrupt) &&
             (air->proc_physics == ftMarioSpecialHiProcPhysics) &&
             (air->proc_map == ftMarioSpecialHiProcMap) &&
             (air->mflags.motion_id == nFTMarioMotionSpecialAirHi) &&
             (air->sflags.ga == nMPKineticsAir) &&
             (air->sflags.is_projectile == FALSE);

    dbglog(DBG_INFO,
           "db: specialhi-table -- SpecialHi row ok %d (motion %d ga %d proj %d), "
           "SpecialAirHi row ok %d (motion %d ga %d proj %d)\n",
           hi_ok, (int)hi->mflags.motion_id, (int)hi->sflags.ga, (int)hi->sflags.is_projectile,
           air_ok, (int)air->mflags.motion_id, (int)air->sflags.ga, (int)air->sflags.is_projectile);
}
#endif /* DB_SPECIALHI_TABLE_PROBE */

/* ---- -DDB_SPECIALN_DEMUX_PROBE: the Neutral-B command demux on the SH-4 --
 *
 * Ports ftCommonSpecialNCheckInterruptCommon and a DIVERGING
 * dFTCommonSpecialNStatusList -- the 27-slot FTKind-indexed setter table
 * with only the ported entries live (all of them ftMarioSpecialNSetStatus:
 * Mario, Luigi, Boss, Metal/polygon Mario, NLuigi, NYoshi), every other
 * slot NULL. This reads the table's contents on the target: Mario's slot
 * (0) and Luigi's (4, the clone that shares Mario's setter) resolve to
 * ftMarioSpecialNSetStatus, Purin's (10) and Fox's (1)
 * resolve to their own setters, while an unported slot (Kirby, 8) is
 * NULL. Taking the check function's address forces the linker to keep it
 * (nothing else calls it). A pure
 * read of a global; nothing mutated. */
#ifdef DB_SPECIALN_DEMUX_PROBE
static void db_specialn_demux_probe(int frame)
{
    extern void (*dFTCommonSpecialNStatusList[])(GObj*);
    extern void ftMarioSpecialNSetStatus(GObj*);
    extern void ftPurinSpecialNSetStatus(GObj*);
    extern void ftFoxSpecialNSetStatus(GObj*);
    extern sb32 ftCommonSpecialNCheckInterruptCommon(GObj*);
    int mario_ok, luigi_ok, purin_ok, fox_ok, kirby_null, fn_present;

    if (frame != 60)
    {
        return;
    }

    mario_ok   = (dFTCommonSpecialNStatusList[nFTKindMario] == ftMarioSpecialNSetStatus);
    luigi_ok   = (dFTCommonSpecialNStatusList[nFTKindLuigi] == ftMarioSpecialNSetStatus);
    /* Purin's own setter, not a Mario clone's -- confirms the
     * table takes a second real fighter's row, not just more Mario slots. */
    purin_ok   = (dFTCommonSpecialNStatusList[nFTKindPurin] == ftPurinSpecialNSetStatus);
    /* Fox's own setter, the wp/ frontier's second weapon's
     * caller. */
    fox_ok     = (dFTCommonSpecialNStatusList[nFTKindFox]   == ftFoxSpecialNSetStatus);
    kirby_null = (dFTCommonSpecialNStatusList[nFTKindKirby] == NULL);
    fn_present = ((void*)ftCommonSpecialNCheckInterruptCommon != NULL);

    dbglog(DBG_INFO,
           "db: specialn-demux -- mario %d luigi %d purin %d fox %d "
           "kirby-null %d check-fn %d\n",
           mario_ok, luigi_ok, purin_ok, fox_ok, kirby_null, fn_present);
}
#endif /* DB_SPECIALN_DEMUX_PROBE */

/* ---- -DDB_PURIN_SPECIALN_PROBE: Purin's Pound on a real fighter ---------
 *
 * Fills dFTPurinSpecialStatusDescs' two Neutral-B rows and
 * dFTCommonSpecialNStatusList's Purin/NPurin slots -- ftpurinspecialn.c's
 * nine procs, compiled unmodified, have a table
 * to land in and a demux row to reach it from. No wiring of its own was
 * needed: ftCommonSpecialNCheckInterruptCommon already dispatches on
 * fp->fkind and was already threaded into every ground cascade, so Purin's slot going live is the whole change.
 *
 * Unlike the fireball's model (its own pack, independent of the owner),
 * Purin's status needs her OWN fighter pack bound -- forcing fp->fkind on
 * a live Fox or Mario would read Pound's motion row out of someone else's
 * data. -DDB_BOOT_P2_KIND=nFTKindPurin (above, in the boot roster) swaps
 * P2 from
 * Mario to a real Purin for this build, so this probe drives her genuine
 * streamed data, including the TransN joint read the ground half's
 * ftPhysicsApplyGroundVelTransN needs (the same joint the Super Jump
 * Punch reads) -- proving the real ROM asset carries the bit
 * the host's mock had to be told about by hand. */
#ifdef DB_PURIN_SPECIALN_PROBE
static void db_purin_specialn_probe(int frame)
{
    extern sb32 ftCommonSpecialNCheckInterruptCommon(GObj *fighter_gobj);
    extern FTStatusDesc dFTDonkeySpecialStatusDescs[];
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    sb32 interrupted;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindPurin)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: purin-specialn -- no Purin player in the "
                         "battle (build with "
                         "-DDB_BOOT_P2_KIND=nFTKindPurin)\n");
        return;
    }

    fp->attr->is_have_specialn = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = 0;

    interrupted = ftCommonSpecialNCheckInterruptCommon(fighter_gobj);

    dbglog(DBG_INFO,
           "db: purin-specialn -- interrupted %d status %d motion %d "
           "want 1 %d %d\n",
           (int)interrupted, (int)fp->status_id, (int)fp->motion_id,
           (int)nFTPurinStatusSpecialN, (int)nFTPurinMotionSpecialN);
}
#endif /* DB_PURIN_SPECIALN_PROBE */

/* ---- -DDB_FOX_SPECIALN_PROBE: the Blaster's real B-tap ------------------
 *
 * Fills dFTFoxSpecialStatusDescs' two Neutral-B rows and
 * dFTCommonSpecialNStatusList's Fox/NFox slots -- ftfoxspecialn.c's five
 * procs, ported this same step, finally have a table to land in and a
 * demux row to reach it from. No cascade wiring of its own was needed
 * (same as Purin's): ftCommonSpecialNCheckInterruptCommon already
 * dispatches on fp->fkind and was already threaded into every ground
 * cascade. Unlike Purin, Fox needs no boot-roster override
 * -- kBootKinds' P1 is Fox by default, so this drives the genuine boot
 * fighter directly. */
#ifdef DB_FOX_SPECIALN_PROBE
static void db_fox_specialn_probe(int frame)
{
    extern sb32 ftCommonSpecialNCheckInterruptCommon(GObj *fighter_gobj);
    extern FTStatusDesc dFTDonkeySpecialStatusDescs[];
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    sb32 interrupted;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindFox)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: fox-specialn -- no Fox player in the battle\n");
        return;
    }

    dbglog(DBG_INFO, "db: fox-specialn -- real attr->is_have_specialn %d "
                     "(before forcing it true below)\n",
           (int)fp->attr->is_have_specialn);

    fp->attr->is_have_specialn = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = 0;

    interrupted = ftCommonSpecialNCheckInterruptCommon(fighter_gobj);

    dbglog(DBG_INFO,
           "db: fox-specialn -- interrupted %d status %d motion %d "
           "want 1 %d %d\n",
           (int)interrupted, (int)fp->status_id, (int)fp->motion_id,
           (int)nFTFoxStatusSpecialN, (int)nFTFoxMotionSpecialN);
}
#endif /* DB_FOX_SPECIALN_PROBE */

/* ---- -DDB_FOX_STATUS_PROBE: what status Fox is REALLY sitting in ------
 *
 * Unlike db_fox_specialn_probe (which forces input at one frame),
 * this one never touches Fox's input or attributes -- it just samples
 * fp->status_id/motion_id/anim_frame periodically to see whether he
 * ever leaves whatever status ftCommonAppearSetStatus (or
 * ftCommonDeadCheckRebirth) puts him in at boot, for the "Fox stands
 * still, B does nothing, purple circle around him" bug report. */
#ifdef DB_FOX_STATUS_PROBE
static void db_fox_status_probe(int frame)
{
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    int player;

    if (frame % 30 != 0 || frame > 600)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindFox)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: fox-status -- no Fox player in the battle\n");
        return;
    }

    dbglog(DBG_INFO,
           "db: fox-status -- frame %d status %d motion %d anim_frame %f "
           "is_ghost %d is_rebirth %d AppearR %d AppearL %d RebirthDown %d "
           "RebirthStand %d RebirthWait %d\n",
           frame, (int)fp->status_id, (int)fp->motion_id,
           (double)fighter_gobj->anim_frame, (int)fp->is_ghost,
           (int)fp->is_rebirth, (int)nFTFoxStatusAppearR,
           (int)nFTFoxStatusAppearL, (int)nFTCommonStatusRebirthDown,
           (int)nFTCommonStatusRebirthStand, (int)nFTCommonStatusRebirthWait);
}
#endif /* DB_FOX_STATUS_PROBE */

/* ---- -DDB_FOX_BLASTER_LIVE_PROBE: the Blaster through the REAL status --
 *
 * db_fox_blaster_probe fakes gFTDataFoxSpecial1 and destroys the shot
 * the same frame; this one does neither. It puts Fox into his real
 * SpecialN status once he is out of the entry (frame 400) and then
 * watches what the motion script's flag0 spawns: whether a weapon GObj
 * appears at all, what WPAttributes it was built from (the reloc
 * stand-in resolves to zeros on target, see wpmanager.c), and how long
 * it lives. */
#ifdef DB_FOX_BLASTER_LIVE_PROBE
static void db_fox_blaster_live_probe(int frame)
{
    extern void ftFoxSpecialNSetStatus(GObj *fighter_gobj);
    extern void *gFTDataFoxSpecial1;
    extern int   llFoxSpecial1BlasterWeaponAttributes;
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    GObj *other_gobj = NULL;
    FTStruct *other_fp = NULL;
    GObj *wg;
    int player, nweapons = 0;

    if (frame < 399 || frame > 470)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindFox)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        return;
    }
    /* the target: whichever other fighter there is, stood 600 units in
     * front of Fox at his own height one frame before the shot, through
     * the port's safe teleport (ftMainRespawn reacquires the floor line),
     * so the shot's path crosses a hurtbox and the hit half of the probe
     * (ftMainSearchHitWeapon) has something to report */
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && g != fighter_gobj)
        {
            other_gobj = g;
            other_fp = ftGetStruct(g);
            break;
        }
    }
    if (frame == 399 && other_gobj != NULL)
    {
        extern void ftMainRespawn(GObj *fighter_gobj, float x, float y);
        DObj *fox = DObjGetStruct(fighter_gobj);

        ftMainRespawn(other_gobj, fox->translate.vec.f.x + fp->lr * 600.0F,
                      fox->translate.vec.f.y);
    }
    if (frame == 400)
    {
        dbglog(DBG_INFO,
               "db: fox-blaster-live -- gFTDataFoxSpecial1 %p ll-addr %p\n",
               gFTDataFoxSpecial1, (void*)&llFoxSpecial1BlasterWeaponAttributes);
        ftFoxSpecialNSetStatus(fighter_gobj);
    }
    for (wg = gGCCommonLinks[nGCCommonLinkIDWeapon]; wg != NULL; wg = wg->link_next)
    {
        WPStruct *wp = wpGetStruct(wg);
        DObj *root = DObjGetStruct(wg);

        nweapons++;
        dbglog(DBG_INFO,
               "db: fox-blaster-live -- frame %d weapon kind %d size %.1f dmg %d "
               "cnt %d mapcoll %.1f/%.1f/%.1f/%.1f pos %.0f,%.0f vel %.1f dobj %d "
               "rec0 %p hurt %d hitn %d\n",
               frame, (int)wp->kind, (double)wp->attack_coll.size,
               (int)wp->attack_coll.damage, (int)wp->attack_coll.attack_count,
               (double)wp->coll_data.map_coll.top, (double)wp->coll_data.map_coll.center,
               (double)wp->coll_data.map_coll.bottom, (double)wp->coll_data.map_coll.width,
               (double)(root ? root->translate.vec.f.x : 0.0F),
               (double)(root ? root->translate.vec.f.y : 0.0F),
               (double)wp->physics.vel_air.x, (int)(root != NULL),
               (void*)wp->attack_coll.attack_records[0].victim_gobj,
               (int)wp->attack_coll.attack_records[0].victim_flags.is_interact_hurt,
               (int)wp->hit_normal_damage);
    }
    dbglog(DBG_INFO,
           "db: fox-blaster-live -- frame %d status %d flag0 %d anim_frame %.0f "
           "weapons %d",
           frame, (int)fp->status_id, (int)fp->motion_vars.flags.flag0,
           (double)fighter_gobj->anim_frame, nweapons);
    if (other_fp != NULL)
    {
        DObj *o = DObjGetStruct(other_gobj);

        dbglog(DBG_INFO, " | other %p at %.0f,%.0f status %d percent %d hitlag %d "
               "dmg_class %d dmg_lr %d angle %d\n",
               (void*)other_gobj, (double)o->translate.vec.f.x,
               (double)o->translate.vec.f.y, (int)other_fp->status_id,
               (int)other_fp->percent_damage, (int)other_fp->hitlag_tics,
               (int)other_fp->damage_object_class, (int)other_fp->damage_lr,
               (int)other_fp->damage_angle);
    }
    else
    {
        dbglog(DBG_INFO, "\n");
    }
}
#endif /* DB_FOX_BLASTER_LIVE_PROBE */

/* ---- -DDB_ITEM_LIVE_PROBE: does a spawned item have a model? -----------
 *
 * For the "the red spawn arrow draws, the item under it never does"
 * report. Spawns through the game's own random spawner
 * (itManagerMakeAppearActor) three times, and every 30 frames walks
 * the item link and reports each item's kind, whether its DObj tree
 * was built from a pack (dc_model_of / root->dv), which of the four
 * itdisplay.c dispatchers it was given, and whether the live camera's
 * mask even covers the item DL link (11). */
#ifdef DB_ITEM_LIVE_PROBE
static void db_item_live_probe(int frame)
{
    extern void itDisplayOPAProcDisplay(GObj *item_gobj);
    extern void itDisplayXLUProcDisplay(GObj *item_gobj);
    extern void itDisplayColAnimOPAProcDisplay(GObj *item_gobj);
    extern void itDisplayColAnimXLUProcDisplay(GObj *item_gobj);
    GObj *g;
    int nitems = 0;

    if (frame == 400 || frame == 700 || frame == 1000)
    {
        g = itManagerMakeAppearActor();
        dbglog(DBG_INFO, "db: item-live -- frame %d MakeAppearActor -> %p\n",
               frame, (void*)g);
    }
    if (frame % 30 != 0 || frame > 1500)
    {
        return;
    }
    for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL; g = g->link_next)
    {
        ITStruct *ip = itGetStruct(g);
        DObj *root = DObjGetStruct(g);
        const char *disp =
            (g->proc_display == itDisplayOPAProcDisplay) ? "OPA" :
            (g->proc_display == itDisplayXLUProcDisplay) ? "XLU" :
            (g->proc_display == itDisplayColAnimOPAProcDisplay) ? "ColAnimOPA" :
            (g->proc_display == itDisplayColAnimXLUProcDisplay) ? "ColAnimXLU" : "?";

        nitems++;
        dbglog(DBG_INFO,
               "db: item-live -- frame %d item kind %d model %d dv %d disp %s "
               "dl_link %d hold %d dmode %d pos %.0f,%.0f arrow %d\n",
               frame, (int)ip->kind, (int)(dc_model_of(g) != NULL),
               (int)(root != NULL && root->dv != NULL), disp,
               (int)g->dl_link_id, (int)ip->is_hold, (int)ip->display_mode,
               (double)(root ? root->translate.vec.f.x : 0.0F),
               (double)(root ? root->translate.vec.f.y : 0.0F),
               (int)(ip->arrow_gobj != NULL));
    }
    dbglog(DBG_INFO, "db: item-live -- frame %d items %d cam_mask&(1<<11) %d\n",
           frame, nitems,
           (int)(gGCCurrentCamera != NULL &&
                 (gGCCurrentCamera->camera_mask & (1ULL << 11)) != 0));
}
#endif /* DB_ITEM_LIVE_PROBE */

/* ---- -DDB_ITEM_ALT_PROBE: the items that swap their display list ------
 *
 * Drops each of the five items whose code writes `dobj->dl` beside P1, two
 * seconds apart -- a Bob-omb (walks, so turns), a Bumper (attaches when it
 * lands), Blastoise, Hitmonlee and Onix (their attack poses) -- so the
 * switch in src/dc/itemmodel.c itemModelSetDisplayList runs on the target
 * without waiting for the item switch to deal them. itemmodel.c logs each
 * switch the first time it happens. Needs -DDB_BOOT_SCENE=nSCKindVSBattle. */
#ifdef DB_ITEM_ALT_PROBE
static void db_item_alt_probe(int frame)
{
    /* -DDB_ITEM_ALT_KIND0=nITKindDogas puts any one item first in the
     * queue (Koffing's smog can be looked at this way) */
#ifndef DB_ITEM_ALT_KIND0
#define DB_ITEM_ALT_KIND0 nITKindBombHei
#endif
    static const s32 kKinds[] = {
        DB_ITEM_ALT_KIND0, nITKindNBumper, nITKindKamex, nITKindSawamura,
        nITKindIwark
    };
    extern void itBombHeiCommonSetWalkLR(GObj *item_gobj, ub8 lr);
    static GObj *bombhei;
    GObj *fighter_gobj = db_fighter_gobj(0);
    s32 k = (frame - 300) / 120;
    Vec3f pos, vel;
    GObj *g;

    /* the Bob-omb turns only when the fight is on its other side, so turn
     * it by hand -- left, then back right -- through the game's own
     * setter, while it is still standing where it landed */
    if ((bombhei != NULL) && ((frame == 330) || (frame == 360)))
    {
        itBombHeiCommonSetWalkLR(bombhei, (frame == 330) ? 0 : 1);
        dbglog(DBG_INFO, "db: item-alt -- frame %d bombhei walk %s\n", frame,
               (frame == 330) ? "left" : "right");
    }

    if (frame == 300)
    {
        /* Kirby's motion file, read the decomp's way on the SH-4: the
         * absolute offsets and the bound base (src/dc/ftcommon.c
         * ftKirbyMainMotionBindOffsets). Ness is 11/13, the jab's second
         * spark turns 19. */
        extern int llKirbyMainMotionSpecialNFTKirbyCopy;
        extern int llKirbyMainMotionftKirbyAttack100Effect;
        FTKirbyCopy *copy = lbRelocGetFileData(FTKirbyCopy*,
            gFTDataKirbyMainMotion, &llKirbyMainMotionSpecialNFTKirbyCopy);
        ftKirbyAttack100Effect *effect = (ftKirbyAttack100Effect*)
            ((uintptr_t)gFTDataKirbyMainMotion +
             (intptr_t)&llKirbyMainMotionftKirbyAttack100Effect);

        dbglog(DBG_INFO, "db: kirby-tables -- offset 0x%x, ness copy %d part "
               "%d, jab spark 1 rotate %.0f\n",
               (unsigned)(intptr_t)&llKirbyMainMotionftKirbyAttack100Effect,
               (int)copy[nFTKindNess].copy_id,
               (int)copy[nFTKindNess].copy_modelpart_id,
               (double)effect[1].rotate);
    }
    if (frame < 300 || ((frame - 300) % 120) != 0 ||
        k >= (s32)ARRAY_COUNT(kKinds) || fighter_gobj == NULL)
    {
        return;
    }
    /* above Saffron City's main floor, clear of the fighters' fight */
    pos.x = 0.0F;
    pos.y = 800.0F;
    pos.z = 0.0F;
#ifdef DB_ITEM_ALT_AT_P1
    /* -DDB_ITEM_ALT_AT_P1: over player 1 instead, wherever the stage is --
     * the battle camera frames fighters and not items, so this is the way
     * to get one in the picture */
    pos = DObjGetStruct(fighter_gobj)->translate.vec.f;
    pos.y += 300.0F;
#endif
    vel.x = vel.y = vel.z = 0.0F;

    g = itManagerMakeItemSetupCommon(NULL, kKinds[k], &pos, &vel,
                                     ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_DEFAULT);
    if (kKinds[k] == nITKindBombHei)
    {
        bombhei = g;
    }
    dbglog(DBG_INFO, "db: item-alt -- frame %d kind %d made %p at %.0f,%.0f\n",
           frame, (int)kKinds[k], (void *)g, (double)pos.x, (double)pos.y);
}
#endif /* DB_ITEM_ALT_PROBE */

/* ---- -DDB_COMPLETE_BANNER: the cleared-bonus-stage banner ---------------
 *
 * COMPLETE! is spelled out of file 7's PLAIN alphabet and
 * coloured by hand, where TIME UP and GAME SET use file 1's pre-coloured
 * letters -- so nine sprite offsets had to be written down by hand, and a
 * wrong one is nine letters of garbage rather than a crash. The bonus
 * stage that would show it has no scene yet, so this raises it over a VS
 * battle at frame 400 for a look. Needs -DDB_BOOT_SCENE=nSCKindVSBattle.
 */
#ifdef DB_COMPLETE_BANNER
static void db_complete_banner(int frame)
{
    /* the BANNER only, not ifCommonAnnounceCompleteInitInterface: that
     * wrapper hands ifCommonBattleSetInterface a 90-tic restore_wait,
     * which is a 1.5-second window to photograph and not much use to a
     * probe. The maker's GObj carries no thread and no wait, so raising
     * it alone leaves COMPLETE! on screen for as long as the probe runs
     * with the match playing underneath. The wrapper is the same two
     * lines ifCommonAnnounceTimeUpInitInterface already is, and it is
     * the LETTERS that are new here -- nine sprite offsets written by
     * hand out of a bank no other banner in the port uses. */
    if (frame == 300)
    {
        dbglog(DBG_INFO, "db: complete-banner -- frame %d raising it\n",
               frame);
        ifCommonAnnounceCompleteMakeInterface();
    }
}
#endif /* DB_COMPLETE_BANNER */

/* ---- -DDB_TARUBOMB_PROBE: Race to the Finish's barrel, off its stage ---
 *
 * The barrel is made by grbonus3.c, and Race to the Finish
 * has no scene yet -- so the only way to see whether it loads a model,
 * falls, lands, rolls and explodes is to drop one into a battle. This
 * makes one every 180 frames (grBonus3TaruBombProcUpdate's own interval)
 * above P1's head and reports, every 30 frames, each barrel's status proc,
 * where it is, how fast, and what it is spinning at.
 *
 * What it is looking for: `model 1` (the tree came out of ittarubomb.mdl
 * rather than nothing), a status that goes Fall -> Roll on landing, a
 * `vel` that grows while the barrel is on a slope and a `spin` that grows
 * with it, and -- when a fighter hits one -- the switch to Explode, six
 * frames, and the item going away. Needs -DDB_BOOT_SCENE=nSCKindVSBattle.
 *
 * -DDB_TARUBOMB_NOMAKE turns the MAKE half off and leaves the report,
 * which is what Race to the Finish's own spawner wants:
 * grBonus3TaruBombProcUpdate is then the only thing putting barrels on
 * the field, so what the report shows is that stage's own clock and its
 * own map position rather than this probe's.
 */
#ifdef DB_TARUBOMB_PROBE
static void db_tarubomb_probe(int frame)
{
    GObj *g;
    Vec3f pos, vel;
    int nitems = 0;

#ifdef DB_TARUBOMB_NOMAKE
    if (0)
    {
#else
    if (frame >= 300 && frame <= 1200 && ((frame - 300) % 180) == 0)
    {
#endif
        /* over P1's HEAD, not a fixed point: a guessed (0, 800) is over
         * the floor on one stage and over the blast zone on the next,
         * and the whole question here is whether the barrel lands. */
        GObj *fighter_gobj = db_fighter_gobj(0);
        DObj *fdobj = (fighter_gobj != NULL) ? DObjGetStruct(fighter_gobj)
                                             : NULL;

        pos.x = (fdobj != NULL) ? fdobj->translate.vec.f.x : 0.0F;
        pos.y = ((fdobj != NULL) ? fdobj->translate.vec.f.y : 0.0F) + 600.0F;
        pos.z = 0.0F;
        vel.x = vel.y = vel.z = 0.0F;

        g = itManagerMakeItemSetupCommon(NULL, nITKindTaruBomb, &pos, &vel,
                                         ITEM_FLAG_COLLPROJECT | ITEM_FLAG_PARENT_DEFAULT);
        dbglog(DBG_INFO, "db: tarubomb -- frame %d made %p model %d at "
               "%.0f,%.0f\n", frame, (void *)g,
               (int)(g != NULL && dc_model_of(g) != NULL),
               (double)pos.x, (double)pos.y);
    }
    /* every THIRD frame, not every thirtieth: the whole explosion is
     * ITTARUBOMB_EXPLODE_LIFETIME (6) frames long, so a coarser sample
     * shows a barrel that was there and then was not and says nothing
     * about how it went. */
    if (frame % 3 != 0 || frame > 1500)
    {
        return;
    }
    for (g = gGCCommonLinks[nGCCommonLinkIDItem]; g != NULL; g = g->link_next)
    {
        ITStruct *ip = itGetStruct(g);
        DObj *root = DObjGetStruct(g);
        const char *st;

        if (ip->kind != nITKindTaruBomb)
        {
            continue;
        }
        st = (ip->proc_update == itTaruBombFallProcUpdate) ? "Fall" :
             (ip->proc_update == itTaruBombRollProcUpdate) ? "Roll" :
             (ip->proc_update == itTaruBombExplodeProcUpdate) ? "Explode" : "?";

        nitems++;
        dbglog(DBG_INFO,
               "db: tarubomb -- frame %d %s model %d pos %.0f,%.0f "
               "vel %.2f,%.2f spin %.4f rotz %.2f multi %d ev %d dmg %d "
               "size %.0f hidden %d ga %d box %d/%d/%d mask %x line %d\n",
               frame, st, (int)(dc_model_of(g) != NULL),
               (double)(root ? root->translate.vec.f.x : 0.0F),
               (double)(root ? root->translate.vec.f.y : 0.0F),
               (double)ip->physics.vel_air.x, (double)ip->physics.vel_air.y,
               (double)ip->item_vars.tarubomb.roll_rotate_step,
               (double)(root ? root->rotate.vec.f.z : 0.0F),
               (int)ip->multi, (int)ip->event_id,
               (int)ip->attack_coll.damage, (double)ip->attack_coll.size,
               (int)(root != NULL && (root->flags & DOBJ_FLAG_HIDDEN) != 0),
               (int)ip->ga, (int)ip->coll_data.map_coll.top,
               (int)ip->coll_data.map_coll.bottom,
               (int)ip->coll_data.map_coll.width,
               (unsigned)ip->coll_data.mask_curr,
               (int)ip->coll_data.floor_line_id);
    }
    if (nitems != 0 || (frame % 30) == 0)
    {
        dbglog(DBG_INFO, "db: tarubomb -- frame %d barrels %d\n", frame,
               nitems);
    }
}
#endif /* DB_TARUBOMB_PROBE */

/* ---- -DDB_BONUS2_PROBE: board a platform without playing --------------
 *
 * Board the Platforms' one real mechanic is a MODEL SWAP:
 * a fighter lands on a nMPMaterialDetect floor, the scene throws that
 * line's platform tree away and builds the "Boarded" twin of the same
 * size in its place, and the row loses a sprite. Nothing in a probe
 * walks a fighter onto a platform, so this does the landing's half
 * directly -- every 120 frames it finds the first floor line still
 * carrying a standing platform and calls the scene's own
 * sc1PBonusStageUpdatePlatformCount on it.
 *
 * What it is looking for: the count walking 10 -> 0, a `size` that stays
 * the size the standing tree had (the swap reads it back out of
 * user_data), a `joints` that is non-zero on the new tree (the Boarded
 * pack was found and built) and COMPLETE! at zero. A `joints 0` is the
 * pack lookup failing, which is the one thing the host cannot see.
 *
 * Needs -DDB_BOOT_SCENE=nSCKind1PBonusStage -DDB_BOOT_BONUS2. */
#ifdef DB_BONUS2_PROBE
static void db_bonus2_probe(int frame)
{
    s32 line_ids[100];
    s32 line_count;
    s32 i;

    if ((frame < 180) || ((frame % 120) != 0) ||
        (gMPCollisionYakumonoDObjs == NULL) ||
        (gGRCommonStruct.bonus2.platform_count == 0))
    {
        return;
    }
    line_count = mpCollisionGetLineCountType(nMPLineKindFloor);

    if (line_count > (s32)ARRAY_COUNT(line_ids))
    {
        line_count = (s32)ARRAY_COUNT(line_ids);
    }
    mpCollisionGetLineIDsTypeCount(nMPLineKindFloor, line_count, line_ids);

    if (frame == 240)
    {
        /* the census, on the first pass: which lines are platforms at
         * all, and what
         * each one's tree looks like. A line with `child 0` is the pack
         * bind failing and is the first thing to look at. */
        for (i = 0; i < line_count; i++)
        {
            DObj *d;
            s32 yid;

            if ((mpCollisionGetVertexFlagsLineID(line_ids[i]) & MAP_VERTEX_MAT_MASK) != nMPMaterialDetect)
            {
                continue;
            }
            yid = mpCollisionSetDObjNoID(line_ids[i]);
            d = gMPCollisionYakumonoDObjs->dobjs[yid];
            dbglog(DBG_INFO, "db: bonus2 -- line %d yak %d dobj %p child %p "
                   "status %d\n", (int)line_ids[i], (int)yid, (void *)d,
                   (void *)((d != NULL) ? d->child : NULL),
                   (int)((d != NULL && d->child != NULL)
                             ? d->child->user_data.s : -12345));
        }
    }
    for (i = 0; i < line_count; i++)
    {
        DObj *dobj;
        s32 id;

        if ((mpCollisionGetVertexFlagsLineID(line_ids[i]) & MAP_VERTEX_MAT_MASK) != nMPMaterialDetect)
        {
            continue;
        }
        id = mpCollisionSetDObjNoID(line_ids[i]);
        dobj = gMPCollisionYakumonoDObjs->dobjs[id];

        if ((dobj == NULL) || (dobj->child == NULL) ||
            (dobj->child->user_data.s == nMPYakumonoStatusNone))
        {
            continue;
        }
        {
            s32 size = dobj->child->user_data.s & ~0x8000;
            DObj *d;
            s32 joints = 0;

            sc1PBonusStageUpdatePlatformCount(dobj);

            for (d = dobj->child; d != NULL;
                 d = lbCommonGetTreeDObjNextFromRoot(d, dobj->child))
            {
                joints++;
            }
            dbglog(DBG_INFO, "db: bonus2 -- frame %d line %d size %d "
                   "boarded, joints %d, %d left\n", frame, (int)line_ids[i],
                   (int)size, (int)joints,
                   (int)gGRCommonStruct.bonus2.platform_count);
        }
        return;
    }
}
#endif /* DB_BONUS2_PROBE */

/* ---- -DDB_ITEM_PICKUP_PROBE: does a fighter pick an item up? -----------
 *
 * Pickup needs the whole fighter side (ftcommonget.c,
 * ftcommonitemthrow.c, ftcommonitemswing.c, ftcommonitemshoot.c,
 * ft/fthammer.c and the five ftcommonhammer*.c) was cut from the VS-era port and
 * restored with the items.
 *
 * This drops one item at P1's feet, waits for it to land and be marked
 * pickup-able, then taps A on his behalf and prints, every frame around
 * the press, the four things that have to move in order: the fighter's
 * status_id (Wait -> LightGet or HeavyGet), his item_gobj (NULL until
 * the animation's flag1 frame), the item's is_hold / is_allow_pickup,
 * and -- the part the new matrix kind is responsible for -- the hand
 * joint's world position against the item's own spliced parent joint,
 * which itMainSetFighterHold points at that joint and
 * gcDObjLocalMatrix's case 0x52 reads every frame.
 *
 * Two pieces of setup the probe does by hand, and neither is the thing
 * under test. P1 is a live CPU on a DB_BOOT_SCENE battle, so it walks,
 * dashes and attacks on its own: the probe re-snaps the item to the
 * front of wherever the fighter is standing and taps A every frame of a
 * forty-frame window, rather than betting one press on one position.
 * And it copies the fighter's own coll_data.floor_line_id onto the item
 * -- a dropped item lands on whatever line is under it, and
 * ftCommonGetFindItem will not reach across lines, which is correct and
 * not what this probe is trying to observe.
 *
 * -DDB_ITEM_PICKUP_KIND picks what to drop: 0 (the default) a Beam
 * Sword, the light swingable; 1 a Crate, the heavy one that goes to
 * HeavyGet -> LiftWait -> LiftTurn; 2 the Hammer, whose pickup is eaten
 * by ftCommonLightGetProcDamage into hammer_tics and its own six
 * statuses. Needs -DDB_BOOT_SCENE=nSCKindVSBattle like every other
 * boot-time probe. */
#ifdef DB_ITEM_PICKUP_PROBE
#ifndef DB_ITEM_PICKUP_KIND
#define DB_ITEM_PICKUP_KIND 0
#endif
#define DB_ITEM_PICKUP_SPAWN_FRAME 300
#define DB_ITEM_PICKUP_PRESS_FRAME 360

static GObj *sDBItemPickupGObj;

static void db_item_pickup_probe(int frame)
{
    extern GObj* itSwordMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags);
    extern GObj* itBoxMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags);
    extern GObj* itHammerMakeItem(GObj *parent_gobj, Vec3f *pos, Vec3f *vel, u32 flags);
    GObj *fighter_gobj;
    FTStruct *fp;
    ITStruct *ip;
    DObj *item_root;
    Vec3f pos, vel;
    s32 joint_id;

    if (frame < DB_ITEM_PICKUP_SPAWN_FRAME || frame > DB_ITEM_PICKUP_PRESS_FRAME + 90)
    {
        return;
    }
    fighter_gobj = db_fighter_gobj(0);

    if (fighter_gobj == NULL)
    {
        return;
    }
    fp = ftGetStruct(fighter_gobj);

    if (frame == DB_ITEM_PICKUP_SPAWN_FRAME)
    {
        pos = DObjGetStruct(fighter_gobj)->translate.vec.f;
        pos.x += 40.0F * fp->lr;
        pos.y += 200.0F;            /* let it fall the last little way */
        pos.z = 0.0F;
        vel.x = vel.y = vel.z = 0.0F;

        sDBItemPickupGObj =
#if DB_ITEM_PICKUP_KIND == 1
            itBoxMakeItem(NULL, &pos, &vel, 0);
#elif DB_ITEM_PICKUP_KIND == 2
            itHammerMakeItem(NULL, &pos, &vel, 0);
#else
            itSwordMakeItem(NULL, &pos, &vel, 0);
#endif
        dbglog(DBG_INFO,
               "db: item-pickup -- frame %d kind %d spawned %p at %.0f,%.0f "
               "p1 fkind %d at %.0f,%.0f lr %.0f hands light %d heavy %d "
               "window %.0f,%.0f +/- %.0f,%.0f\n",
               frame, (int)DB_ITEM_PICKUP_KIND, (void*)sDBItemPickupGObj,
               (double)pos.x, (double)pos.y, (int)fp->fkind,
               (double)DObjGetStruct(fighter_gobj)->translate.vec.f.x,
               (double)DObjGetStruct(fighter_gobj)->translate.vec.f.y,
               (double)fp->lr,
               (int)fp->attr->joint_itemlight_id,
               (int)fp->attr->joint_itemheavy_id,
               (double)fp->attr->item_pickup.pickup_offset_light.x,
               (double)fp->attr->item_pickup.pickup_offset_light.y,
               (double)fp->attr->item_pickup.pickup_range_light.x,
               (double)fp->attr->item_pickup.pickup_range_light.y);
        return;
    }
    if (sDBItemPickupGObj == NULL)
    {
        return;
    }
    ip = itGetStruct(sDBItemPickupGObj);

    /* the A tap, on the fighter's behalf: the ground cascade's attack
     * setters all ask ftCommonGetCheckInterruptCommon first, so this is
     * the game's own path into the pickup, not a direct call. The item
     * is put back in front of him first (see the header) -- a live CPU
     * does not stand still and a dropped item does not stay put. */
    if (frame >= DB_ITEM_PICKUP_PRESS_FRAME && fp->item_gobj == NULL)
    {
        DObj *root = DObjGetStruct(sDBItemPickupGObj);

        root->translate.vec.f.x =
            DObjGetStruct(fighter_gobj)->translate.vec.f.x + 60.0F * fp->lr;
        root->translate.vec.f.y =
            DObjGetStruct(fighter_gobj)->translate.vec.f.y;
        ip->coll_data.floor_line_id = fp->coll_data.floor_line_id;
        ip->is_allow_pickup = TRUE;

        fp->input.pl.button_tap |= fp->input.button_mask_a;
        fp->input.pl.button_hold |= fp->input.button_mask_a;
    }
    if (frame < DB_ITEM_PICKUP_PRESS_FRAME - 5)
    {
        return;
    }
    item_root = DObjGetStruct(sDBItemPickupGObj);
    joint_id = (ip->weight == nITWeightHeavy) ? fp->attr->joint_itemheavy_id
                                              : fp->attr->joint_itemlight_id;
    pos.x = pos.y = pos.z = 0.0F;
    gmCollisionGetFighterPartsWorldPosition(fp->joints[joint_id], &pos);

    dbglog(DBG_INFO,
           "db: item-pickup -- frame %d status %d item_gobj %d hold %d "
           "allow %d wt %d fl ft %d it %d hand %.0f,%.0f root %.0f,%.0f "
           "attached %d model %d rootdv %d hammer %d\n",
           frame, (int)fp->status_id, (int)(fp->item_gobj != NULL),
           (int)ip->is_hold, (int)ip->is_allow_pickup, (int)ip->weight,
           (int)fp->coll_data.floor_line_id, (int)ip->coll_data.floor_line_id,
           (double)pos.x, (double)pos.y,
           (double)item_root->translate.vec.f.x,
           (double)item_root->translate.vec.f.y,
           (int)(item_root->user_data.p == (void*)fp->joints[joint_id]),
           (int)(dc_model_of(sDBItemPickupGObj) != NULL),
           (int)(item_root->dv != NULL),
           (int)fp->hammer_tics);
}
#endif /* DB_ITEM_PICKUP_PROBE */

/* ---- -DDB_SPECIALHI_DEMUX_PROBE: the Up-B command demux on the SH-4 ------
 *
 * Ports ftCommonSpecialHiCheckInterruptCommon and a DIVERGING
 * dFTCommonSpecialHiStatusList, the twin of the Neutral-B demux above -- the
 * 27-slot FTKind-indexed setter table with only the ported entries live (all
 * of them ftMarioSpecialHiSetStatus: Mario, Luigi, Boss, Metal/polygon Mario,
 * NLuigi), every other slot NULL. This reads the table on the target: Mario's
 * slot (0) and Luigi's (4, the clone that shares Mario's setter) resolve to
 * ftMarioSpecialHiSetStatus, while an unported slot (Fox, 1) is NULL -- and so
 * is NYoshi (20), the one slot this table does NOT route to Mario (the decomp
 * gives it Yoshi's own, unported, setter). The check
 * function is wired into the same eight ground cascades SpecialN
 * reaches, so it is kept by those real callers now, not just by this probe
 * taking its address. A pure read of a global; nothing mutated. */
#ifdef DB_SPECIALHI_DEMUX_PROBE
static void db_specialhi_demux_probe(int frame)
{
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);
    extern void ftMarioSpecialHiSetStatus(GObj*);
    extern sb32 ftCommonSpecialHiCheckInterruptCommon(GObj*);
    int mario_ok, luigi_ok, fox_null, nyoshi_null, fn_present;

    if (frame != 60)
    {
        return;
    }

    mario_ok    = (dFTCommonSpecialHiStatusList[nFTKindMario]  == ftMarioSpecialHiSetStatus);
    luigi_ok    = (dFTCommonSpecialHiStatusList[nFTKindLuigi]  == ftMarioSpecialHiSetStatus);
    fox_null    = (dFTCommonSpecialHiStatusList[nFTKindFox]    == NULL);
    nyoshi_null = (dFTCommonSpecialHiStatusList[nFTKindNYoshi] == NULL);
    fn_present  = ((void*)ftCommonSpecialHiCheckInterruptCommon != NULL);

    dbglog(DBG_INFO,
           "db: specialhi-demux -- mario %d luigi %d fox-null %d nyoshi-null %d check-fn %d\n",
           mario_ok, luigi_ok, fox_null, nyoshi_null, fn_present);
}
#endif /* DB_SPECIALHI_DEMUX_PROBE */

/* ---- -DDB_PURIN_SPECIALHI_PROBE: Sing on a real fighter -----------------
 *
 * Fills dFTPurinSpecialStatusDescs' two Up-B rows and
 * dFTCommonSpecialHiStatusList's Purin/NPurin slots -- ftpurinspecialhi.c's
 * six procs, compiled unmodified, finish Purin's whole special-move set
 * (Neutral-B and Down-B were already live). No cascade wiring of its own
 * was needed: ftCommonSpecialHiCheckInterruptCommon already dispatches on
 * fp->fkind and was already threaded into every ground cascade.
 * Like Purin's Neutral-B probe above (not Fox's, which needs no boot
 * override), this needs a real Purin bound -- build with
 * -DDB_BOOT_P2_KIND=nFTKindPurin alongside. */
#ifdef DB_PURIN_SPECIALHI_PROBE
static void db_purin_specialhi_probe(int frame)
{
    extern sb32 ftCommonSpecialHiCheckInterruptCommon(GObj *fighter_gobj);
    extern sb32 ftCommonSpecialAirCheckInterruptCommon(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    sb32 interrupted;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindPurin)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: purin-specialhi -- no Purin player in the "
                         "battle (build with "
                         "-DDB_BOOT_P2_KIND=nFTKindPurin)\n");
        return;
    }

    fp->attr->is_have_specialhi = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;

    interrupted = ftCommonSpecialHiCheckInterruptCommon(fighter_gobj);

    dbglog(DBG_INFO,
           "db: purin-specialhi -- interrupted %d status %d motion %d "
           "want 1 %d %d\n",
           (int)interrupted, (int)fp->status_id, (int)fp->motion_id,
           (int)nFTPurinStatusSpecialHi, (int)nFTPurinMotionSpecialHi);
}
#endif /* DB_PURIN_SPECIALHI_PROBE */

/* ---- -DDB_DONKEY_SPECIALHI_PROBE: Spinning Kong on a real fighter -------
 *
 * Fills dFTDonkeySpecialStatusDescs' two Up-B rows and
 * dFTCommonSpecialHiStatusList's Donkey/NDonkey/GDonkey slots --
 * ftdonkeyspecialhi.c's eleven procs, compiled unmodified, finish Donkey's
 * second special-move file (Down-B's Hand Slap was already live). No
 * cascade wiring of its own was needed, the same as Purin's Sing above.
 * Needs a real Donkey bound the same way Purin's own probe does -- build
 * with -DDB_BOOT_P2_KIND=nFTKindDonkey alongside. */
#ifdef DB_DONKEY_SPECIALHI_PROBE
static void db_donkey_specialhi_probe(int frame)
{
    extern sb32 ftCommonSpecialHiCheckInterruptCommon(GObj *fighter_gobj);
    extern sb32 ftCommonSpecialAirCheckInterruptCommon(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    sb32 interrupted;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindDonkey)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: donkey-specialhi -- no Donkey player in the "
                         "battle (build with "
                         "-DDB_BOOT_P2_KIND=nFTKindDonkey)\n");
        return;
    }

    fp->attr->is_have_specialhi = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;

    interrupted = ftCommonSpecialHiCheckInterruptCommon(fighter_gobj);

    /* want SpecialAirHi (231), not SpecialHi (230): ftDonkeySpecialHiSetStatus
     * (ftdonkeyspecialhi.c:103-112) unconditionally calls ftMainSetStatus
     * with nFTDonkeyStatusSpecialAirHi even when entered from the ground --
     * only fp->stat_flags.ga is forced back to nMPKineticsGround after. The
     * ground-locked status (230) is reached later, only via
     * ftDonkeySpecialAirHiSwitchStatusGround off a floor landing during the
     * move, never as the demux's own entry point. */
    dbglog(DBG_INFO,
           "db: donkey-specialhi -- interrupted %d status %d motion %d "
           "want 1 %d %d\n",
           (int)interrupted, (int)fp->status_id, (int)fp->motion_id,
           (int)nFTDonkeyStatusSpecialAirHi, (int)nFTDonkeyMotionSpecialAirHi);
}
#endif /* DB_DONKEY_SPECIALHI_PROBE */

/* ---- -DDB_SAMUS_SPECIALHI_PROBE: Screw Attack on a real fighter --------
 *
 * Builds dFTSamusSpecialStatusDescs from scratch (Samus's FIRST
 * special-move table -- dFTMainSpecialStatusDescs[nFTKindSamus] was
 * FT_SPECIAL_NONE before this step) and filled dFTCommonSpecialHiStatusList's
 * Samus/NSamus slots -- ftsamusspecialhi.c's seven procs, compiled
 * unmodified, finish her first reachable special of any kind. No cascade
 * wiring of its own was needed, the same as Purin's and Donkey's above.
 * Needs a real Samus bound the same way -- build with
 * -DDB_BOOT_P2_KIND=nFTKindSamus alongside. Unlike Donkey's SetStatus
 * pair, ftSamusSpecialHiSetStatus lands directly on
 * nFTSamusStatusSpecialHi with no same-tic quirk, so "want" is a plain
 * readback here. */
#ifdef DB_SAMUS_SPECIALHI_PROBE
static void db_samus_specialhi_probe(int frame)
{
    extern sb32 ftCommonSpecialHiCheckInterruptCommon(GObj *fighter_gobj);
    extern sb32 ftCommonSpecialAirCheckInterruptCommon(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    sb32 interrupted;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindSamus)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: samus-specialhi -- no Samus player in the "
                         "battle (build with "
                         "-DDB_BOOT_P2_KIND=nFTKindSamus)\n");
        return;
    }

    fp->attr->is_have_specialhi = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;

    interrupted = ftCommonSpecialHiCheckInterruptCommon(fighter_gobj);

    dbglog(DBG_INFO,
           "db: samus-specialhi -- interrupted %d status %d motion %d "
           "want 1 %d %d\n",
           (int)interrupted, (int)fp->status_id, (int)fp->motion_id,
           (int)nFTSamusStatusSpecialHi, (int)nFTSamusMotionSpecialHi);
}
#endif /* DB_SAMUS_SPECIALHI_PROBE */

/* ---- -DDB_CAPTAIN_SPECIALN_PROBE: Falcon Punch on a real fighter ------
 *
 * Builds dFTCaptainSpecialStatusDescs from scratch (Captain's
 * FIRST special-move table -- dFTMainSpecialStatusDescs[nFTKindCaptain]
 * was FT_SPECIAL_NONE before this step) and filled the Neutral-B demux's
 * Captain/NCaptain slots -- ftcaptainspecialn.c's eleven procs, compiled
 * unmodified, finish his first reachable special of any kind. The demux
 * itself needed no wiring of its own (ftCommonSpecialNCheckInterruptCommon
 * has looped on fp->fkind and sat in every ground cascade since
 * long ago). Needs a real Captain bound the same way -- build with
 * -DDB_BOOT_P2_KIND=nFTKindCaptain alongside. ftCaptainSpecialNSetStatus
 * lands directly on nFTCaptainStatusSpecialN with no same-tic quirk, so
 * "want" is a plain readback here. */
#ifdef DB_CAPTAIN_SPECIALN_PROBE
static void db_captain_specialn_probe(int frame)
{
    extern sb32 ftCommonSpecialNCheckInterruptCommon(GObj *fighter_gobj);
    extern FTStatusDesc dFTDonkeySpecialStatusDescs[];
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    sb32 interrupted;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindCaptain)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: captain-specialn -- no Captain player in the "
                         "battle (build with "
                         "-DDB_BOOT_P2_KIND=nFTKindCaptain)\n");
        return;
    }

    fp->attr->is_have_specialn = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = 0;

    interrupted = ftCommonSpecialNCheckInterruptCommon(fighter_gobj);

    dbglog(DBG_INFO,
           "db: captain-specialn -- interrupted %d status %d motion %d "
           "want 1 %d %d\n",
           (int)interrupted, (int)fp->status_id, (int)fp->motion_id,
           (int)nFTCaptainStatusSpecialN, (int)nFTCaptainMotionSpecialN);
}
#endif /* DB_CAPTAIN_SPECIALN_PROBE */

/* ---- -DDB_SAMUS_SPECIALLW_PROBE: Bomb on a real fighter ---------------
 *
 * Extends dFTSamusSpecialStatusDescs (Up-B's own table)
 * with her Down-B rows and fills the Down-B demux's Samus/
 * NSamus slots -- ftsamusspeciallw.c's thirteen procs, compiled
 * unmodified, finish her second special. The demux itself needed no
 * wiring of its own (ftCommonSpecialLwCheckInterruptCommon has sat in
 * every ground cascade since long before this table existed). Needs a
 * real Samus bound the same way the Up-B probe does -- build with
 * -DDB_BOOT_P2_KIND=nFTKindSamus alongside; no new boot knob needed;
 * Samus's is already there. ftSamusSpecialLwSetStatus lands directly on
 * nFTSamusStatusSpecialLw with no same-tic quirk, so "want" is a plain
 * readback here -- the bomb itself is not thrown on this same tic
 * (flag0 gates ftSamusSpecialLwMakeBomb and starts FALSE), so this probe
 * only proves the demux/table path, not wpSamusBombMakeWeapon; that is a
 * disc-probe-direct-call the way Fox's Blaster is proved. */
#ifdef DB_SAMUS_SPECIALLW_PROBE
static void db_samus_speciallw_probe(int frame)
{
    extern sb32 ftCommonSpecialLwCheckInterruptCommon(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    sb32 interrupted;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindSamus)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: samus-speciallw -- no Samus player in the "
                         "battle (build with "
                         "-DDB_BOOT_P2_KIND=nFTKindSamus)\n");
        return;
    }

    fp->attr->is_have_speciallw = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = FTCOMMON_SPECIALLW_STICK_RANGE_MIN;

    interrupted = ftCommonSpecialLwCheckInterruptCommon(fighter_gobj);

    dbglog(DBG_INFO,
           "db: samus-speciallw -- interrupted %d status %d motion %d "
           "want 1 %d %d\n",
           (int)interrupted, (int)fp->status_id, (int)fp->motion_id,
           (int)nFTSamusStatusSpecialLw, (int)nFTSamusMotionSpecialLw);
}
#endif /* DB_SAMUS_SPECIALLW_PROBE */

/* ---- -DDB_CAPTAIN_SPECIALLW_PROBE: Falcon Kick on a real fighter ------
 *
 * Extends dFTCaptainSpecialStatusDescs (Falcon Punch's own
 * table) with the five Falcon Kick rows and fills the
 * Down-B demux's Captain/NCaptain slots -- ftcaptainspeciallw.c's nineteen
 * procs, compiled unmodified, finish his second special. The demux itself
 * needed no wiring of its own (ftCommonSpecialLwCheckInterruptCommon has
 * sat in every ground cascade since long before this table existed). Needs
 * a real Captain bound the same way the Falcon Punch probe does -- build with
 * -DDB_BOOT_P2_KIND=nFTKindCaptain alongside; no new boot knob needed;
 * Captain's is already there. ftCaptainSpecialLwSetStatus lands directly on
 * nFTCaptainStatusSpecialLw with no same-tic quirk, so "want" is a plain
 * readback here. */
#ifdef DB_CAPTAIN_SPECIALLW_PROBE
static void db_captain_speciallw_probe(int frame)
{
    extern sb32 ftCommonSpecialLwCheckInterruptCommon(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    sb32 interrupted;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindCaptain)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: captain-speciallw -- no Captain player in the "
                         "battle (build with "
                         "-DDB_BOOT_P2_KIND=nFTKindCaptain)\n");
        return;
    }

    fp->attr->is_have_speciallw = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = FTCOMMON_SPECIALLW_STICK_RANGE_MIN;

    interrupted = ftCommonSpecialLwCheckInterruptCommon(fighter_gobj);

    dbglog(DBG_INFO,
           "db: captain-speciallw -- interrupted %d status %d motion %d "
           "want 1 %d %d\n",
           (int)interrupted, (int)fp->status_id, (int)fp->motion_id,
           (int)nFTCaptainStatusSpecialLw, (int)nFTCaptainMotionSpecialLw);
}
#endif /* DB_CAPTAIN_SPECIALLW_PROBE */

/* ---- -DDB_YOSHI_SPECIALLW_PROBE: Yoshi Bomb on a real fighter ---------
 *
 * Gives Yoshi his FIRST special-move table
 * (dFTMainSpecialStatusDescs[nFTKindYoshi] was FT_SPECIAL_NONE before)
 * with the four Yoshi Bomb rows and filled the Down-B demux's Yoshi/
 * NYoshi slots -- ftyoshispeciallw.c's ten procs and wpyoshistar.c's
 * nine, compiled unmodified. The demux itself needed no wiring of its own
 * (ftCommonSpecialLwCheckInterruptCommon has sat in every ground cascade
 * since long before this table existed). Needs a real Yoshi bound --
 * build with -DDB_BOOT_P2_KIND=nFTKindYoshi, the same as the four before
 * it. ftYoshiSpecialLwStartSetStatus lands directly on
 * nFTYoshiStatusSpecialLwStart with no same-tic quirk, so "want" is a
 * plain readback here; it also takes him airborne on the spot, so the
 * kinetics column is logged too (want 1 = nMPKineticsAir). The stars
 * themselves are thrown on the landing tic, not this one, so this probe
 * proves the demux/table path only; wpYoshiStarMakeWeapon on target is
 * the host-tested code with no sWPManagerModels row yet, the same state
 * Samus's Bomb is in. */
#ifdef DB_YOSHI_SPECIALLW_PROBE
static void db_yoshi_speciallw_probe(int frame)
{
    extern sb32 ftCommonSpecialLwCheckInterruptCommon(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    sb32 interrupted;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindYoshi)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: yoshi-speciallw -- no Yoshi player in the "
                         "battle (build with "
                         "-DDB_BOOT_P2_KIND=nFTKindYoshi)\n");
        return;
    }

    fp->attr->is_have_speciallw = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = FTCOMMON_SPECIALLW_STICK_RANGE_MIN;

    interrupted = ftCommonSpecialLwCheckInterruptCommon(fighter_gobj);

    dbglog(DBG_INFO,
           "db: yoshi-speciallw -- interrupted %d status %d motion %d ga %d "
           "want 1 %d %d %d\n",
           (int)interrupted, (int)fp->status_id, (int)fp->motion_id,
           (int)fp->ga, (int)nFTYoshiStatusSpecialLwStart,
           (int)nFTYoshiMotionSpecialLwStart, (int)nMPKineticsAir);
}
#endif /* DB_YOSHI_SPECIALLW_PROBE */

/* ---- -DDB_DONKEY_SPECIALN_PROBE: Giant Punch on a real fighter ---------
 *
 * Fills dFTDonkeySpecialStatusDescs' eight Neutral-B rows and
 * dFTCommonSpecialNStatusList's Donkey/NDonkey/GDonkey slots --
 * ftdonkeyspecialn.c's twenty-four procs, compiled unmodified, finish
 * Donkey's whole special-move set. No cascade wiring of its own was
 * needed, the same as the other table-only probes. Needs a real Donkey
 * bound the same way the Up-B probe does -- build with
 * -DDB_BOOT_P2_KIND=nFTKindDonkey alongside; no new boot knob.
 * ftDonkeySpecialNStartSetStatus lands directly on
 * nFTDonkeyStatusSpecialNStart with no same-tic quirk (unlike Spinning
 * Kong's setter above), so "want" is a plain readback; the charge starts
 * at zero and is_release FALSE, logged too. */
#ifdef DB_DONKEY_SPECIALN_PROBE
static void db_donkey_specialn_probe(int frame)
{
    extern sb32 ftCommonSpecialNCheckInterruptCommon(GObj *fighter_gobj);
    extern FTStatusDesc dFTDonkeySpecialStatusDescs[];
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    sb32 interrupted;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindDonkey)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: donkey-specialn -- no Donkey player in the "
                         "battle (build with "
                         "-DDB_BOOT_P2_KIND=nFTKindDonkey)\n");
        return;
    }

    fp->attr->is_have_specialn = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = 0;

    interrupted = ftCommonSpecialNCheckInterruptCommon(fighter_gobj);

    dbglog(DBG_INFO,
           "db: donkey-specialn -- interrupted %d status %d motion %d "
           "charge %d release %d want 1 %d %d 0 0\n",
           (int)interrupted, (int)fp->status_id, (int)fp->motion_id,
           (int)fp->passive_vars.donkey.charge_level,
           (int)fp->status_vars.donkey.specialn.is_release,
           (int)nFTDonkeyStatusSpecialNStart, (int)nFTDonkeyMotionSpecialNStart);

    /* ---- the CARGO THROW ----
     *
     * Eleven statuses on a real Donkey, driven the way the game drives
     * them: ftDonkeyThrowFWaitSetStatus is what ftcommonthrow.c calls
     * when a DK forward throw's animation ends, and everything after it
     * is that status's own interrupt reading a pad.
     *
     * `carry` is the status the entry lands on, `walk` the one a hard
     * sideways stick reaches from it, `turn` the one the opposite
     * direction reaches, and `jump` what a Z-less A-tap... no: the
     * cargo carry's KneeBend is reached by the jump button, which is
     * what ftDonkeyThrowFWaitProcInterrupt reads. Each is the status_id
     * after one call, or -1 if the interrupt said no. */
    {
        extern void ftDonkeyThrowFWaitSetStatus(GObj *fighter_gobj);
            FTStatusDesc *r;
        int carry, walk, turn, filled, i;

        filled = 0;
        for (i = nFTDonkeyStatusThrowFWait; i <= nFTDonkeyStatusThrowAirFF; i++)
        {
            r = &dFTDonkeySpecialStatusDescs[i - nFTCommonStatusSpecialStart];

            if ((r->proc_physics != NULL) && (r->proc_map != NULL) &&
                (r->sflags.attack_id == nFTStatusAttackIDThrowF))
            {
                filled++;
            }
        }
        ftMainSetStatus(fighter_gobj, nFTCommonStatusWait, 0.0F, 1.0F,
                        FTSTATUS_PRESERVE_NONE);
        fp->input.pl.button_tap = 0;
        fp->input.pl.stick_range.x = 0;
        ftDonkeyThrowFWaitSetStatus(fighter_gobj);
        carry = (int)fp->status_id;

        /* a hard stick the way he faces walks him */
        fp->lr = +1;
        fp->input.pl.stick_range.x = 127;
        fp->motion_vars.flags.flag0 = 0;
        walk = (fp->proc_interrupt != NULL)
               ? (fp->proc_interrupt(fighter_gobj), (int)fp->status_id) : -1;

        /* and the other way turns him */
        ftMainSetStatus(fighter_gobj, nFTCommonStatusWait, 0.0F, 1.0F,
                        FTSTATUS_PRESERVE_NONE);
        ftDonkeyThrowFWaitSetStatus(fighter_gobj);
        fp->input.pl.stick_range.x = -127;
        turn = (fp->proc_interrupt != NULL)
               ? (fp->proc_interrupt(fighter_gobj), (int)fp->status_id) : -1;

        dbglog(DBG_INFO,
               "db: donkey-cargo -- filled %d carry %d walk %d turn %d "
               "want 11 %d %d (the fastest of %d..%d, for a stick at the "
               "rail) %d\n",
               filled, carry, walk, turn,
               (int)nFTDonkeyStatusThrowFWait,
               (int)nFTDonkeyStatusThrowFWalkFast,
               (int)nFTDonkeyStatusThrowFWalkSlow,
               (int)nFTDonkeyStatusThrowFWalkFast,
               (int)nFTDonkeyStatusThrowFTurn);

        ftMainSetStatus(fighter_gobj, nFTCommonStatusWait, 0.0F, 1.0F,
                        FTSTATUS_PRESERVE_NONE);
        fp->input.pl.stick_range.x = 0;
        fp->motion_vars.flags.flag0 = 0;
    }
}
#endif /* DB_DONKEY_SPECIALN_PROBE */

/* ---- -DDB_CAPTAIN_SPECIALHI_PROBE: Falcon Dive on a real fighter --------
 *
 * Extends dFTCaptainSpecialStatusDescs (Falcon Punch's and
 * Falcon Kick's since steps 45/47) with the four Falcon Dive rows, added
 * the caught side's common row (179, CaptureCaptain) and filled the Up-B
 * demux's Captain/NCaptain slots -- ftcaptainspecialhi.c's twelve procs
 * and ftcommoncapturecaptain.c's four, compiled unmodified, finish his
 * whole special-move set. Needs a real Captain bound -- build with
 * -DDB_BOOT_P2_KIND=nFTKindCaptain alongside; no new boot knob.
 * ftCaptainSpecialHiSetStatus takes him airborne on the spot
 * (mpCommonSetFighterAir) and lands directly on
 * nFTCaptainStatusSpecialHi with no same-tic quirk, so "want" is a plain
 * readback, with the kinetics column and the catch mask
 * (FTCATCHKIND_MASK_CAPTAINSPECIALHI, set by ftCaptainSpecialHiSetCatch-
 * Params) logged too. The grab itself needs a body in the way; that is
 * the host test's job. */
#ifdef DB_CAPTAIN_SPECIALHI_PROBE
static void db_captain_specialhi_probe(int frame)
{
    extern sb32 ftCommonSpecialHiCheckInterruptCommon(GObj *fighter_gobj);
    extern sb32 ftCommonSpecialAirCheckInterruptCommon(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    sb32 interrupted;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindCaptain)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: captain-specialhi -- no Captain player in the "
                         "battle (build with "
                         "-DDB_BOOT_P2_KIND=nFTKindCaptain)\n");
        return;
    }

    fp->attr->is_have_specialhi = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;

    interrupted = ftCommonSpecialHiCheckInterruptCommon(fighter_gobj);

    dbglog(DBG_INFO,
           "db: captain-specialhi -- interrupted %d status %d motion %d ga %d "
           "catchmask %d want 1 %d %d %d %d\n",
           (int)interrupted, (int)fp->status_id, (int)fp->motion_id,
           (int)fp->ga, (int)fp->catch_mask,
           (int)nFTCaptainStatusSpecialHi, (int)nFTCaptainMotionSpecialHi,
           (int)nMPKineticsAir, (int)FTCATCHKIND_MASK_CAPTAINSPECIALHI);
}
#endif /* DB_CAPTAIN_SPECIALHI_PROBE */

/* ---- -DDB_SAMUS_SPECIALN_PROBE: Charge Shot on a real fighter ----------
 *
 * Fills dFTSamusSpecialStatusDescs' five Neutral-B rows and
 * dFTCommonSpecialNStatusList's Samus/NSamus slots -- ftsamusspecialn.c's
 * twenty-two procs and wpsamuschargeshot.c, compiled unmodified, finish
 * her whole special-move set. Needs a real Samus bound -- build with
 * -DDB_BOOT_P2_KIND=nFTKindSamus alongside; no new boot knob.
 * ftSamusSpecialNStartSetStatus lands directly on
 * nFTSamusStatusSpecialNStart with no same-tic quirk, so "want" is a
 * plain readback; the charge starts at zero and
 * is_release FALSE (it is TRUE only at full charge), logged too. The
 * held shot is made on the Loop tic, not this one, and is unmodeled on
 * target anyway (no sWPManagerModels row). */
#ifdef DB_SAMUS_SPECIALN_PROBE
static void db_samus_specialn_probe(int frame)
{
    extern sb32 ftCommonSpecialNCheckInterruptCommon(GObj *fighter_gobj);
    extern FTStatusDesc dFTDonkeySpecialStatusDescs[];
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    sb32 interrupted;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindSamus)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: samus-specialn -- no Samus player in the "
                         "battle (build with "
                         "-DDB_BOOT_P2_KIND=nFTKindSamus)\n");
        return;
    }

    fp->attr->is_have_specialn = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = 0;

    interrupted = ftCommonSpecialNCheckInterruptCommon(fighter_gobj);

    dbglog(DBG_INFO,
           "db: samus-specialn -- interrupted %d status %d motion %d "
           "charge %d release %d want 1 %d %d 0 0\n",
           (int)interrupted, (int)fp->status_id, (int)fp->motion_id,
           (int)fp->passive_vars.samus.charge_level,
           (int)fp->status_vars.samus.specialn.is_release,
           (int)nFTSamusStatusSpecialNStart, (int)nFTSamusMotionSpecialNStart);
}
#endif /* DB_SAMUS_SPECIALN_PROBE */

/* ---- -DDB_FOX_SPECIALLW_PROBE: the Reflector on a real fighter ----------
 *
 * Fills dFTFoxSpecialStatusDescs' ten Down-B rows and
 * dFTCommonSpecialLwStatusList's Fox/NFox slots -- ftfoxspeciallw.c's
 * twenty-three procs, compiled unmodified, plus the reflector effect
 * (efmanager.c, whose maker returns NULL until the model exists) and the
 * hurt sphere bound out of the port's own copy of Fox's motion-file bytes
 * (ftcommon.c). P1 is Fox on every DB_BOOT_SCENE battle, so no boot knob.
 * ftFoxSpecialLwStartSetStatus lands directly on nFTFoxStatusSpecialLwStart
 * with no same-tic quirk, so "want" is a plain readback; the sphere's kind
 * (nFTSpecialCollKindFoxReflector, 0) and joint (4) read back through the
 * bound base prove the step-50 data pattern on target a second time. */
#ifdef DB_FOX_SPECIALLW_PROBE
static void db_fox_speciallw_probe(int frame)
{
    extern sb32 ftCommonSpecialLwCheckInterruptCommon(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    sb32 interrupted;
    int player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindFox)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: fox-speciallw -- no Fox player in the battle\n");
        return;
    }

    fp->attr->is_have_speciallw = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = FTCOMMON_SPECIALLW_STICK_RANGE_MIN;

    interrupted = ftCommonSpecialLwCheckInterruptCommon(fighter_gobj);

    dbglog(DBG_INFO,
           "db: fox-speciallw -- interrupted %d status %d motion %d "
           "collkind %d colljoint %d lag %d want 1 %d %d %d 4 %d\n",
           (int)interrupted, (int)fp->status_id, (int)fp->motion_id,
           fp->special_coll ? (int)fp->special_coll->kind : -1,
           fp->special_coll ? (int)fp->special_coll->joint_id : -1,
           (int)fp->status_vars.fox.speciallw.release_lag,
           (int)nFTFoxStatusSpecialLwStart, (int)nFTFoxMotionSpecialLwStart,
           (int)nFTSpecialCollKindFoxReflector, (int)FTFOX_REFLECTOR_RELEASE_LAG);
}
#endif /* DB_FOX_SPECIALLW_PROBE */

/* ---- -DDB_FOX_SPECIALHI_PROBE: Fire Fox on a real fighter --------------
 *
 * Fills dFTFoxSpecialStatusDescs' nine Up-B rows (227-235) and
 * dFTCommonSpecialHiStatusList's Fox/NFox slots -- ftfoxspecialhi.c's thirty
 * procs, compiled unmodified, plus two leaves the port had never needed
 * (lbCommonCheckAdjustSim2D, mpCommonCheckFighterPass). P1 is Fox on every
 * DB_BOOT_SCENE battle, so no boot knob. This walks the whole move on a real
 * fighter: the B-tap with the stick up lands on SpecialHiStart (halving his
 * ground velocity and arming the gravity delay), the charge is set and run
 * out, and the launch with a neutral stick takes him airborne straight up --
 * FTFOX_FIREFOX_VEL in y, the travel timer full, every jump spent. The last
 * two numbers are the model pitch ftFoxSpecialHiUpdateModelPitch writes
 * through ftParamsUpdateFighterPartsTransformAll on joint 4, which the
 * 3-joint host mock can only approximate: travelling straight up it is
 * atan2(0, 115)*lr - 90 degrees = -1.5708 rad, printed in milliradians. */
#ifdef DB_FOX_SPECIALHI_PROBE
static void db_fox_specialhi_probe(int frame)
{
    extern sb32 ftCommonSpecialHiCheckInterruptCommon(GObj *fighter_gobj);
    extern sb32 ftCommonSpecialAirCheckInterruptCommon(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    sb32 interrupted;
    int start_status, start_motion, gravity, hold, player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindFox)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: fox-specialhi -- no Fox player in the battle\n");
        return;
    }

    fp->attr->is_have_specialhi = TRUE;
    fp->physics.vel_ground.x = 40.0F;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;

    interrupted = ftCommonSpecialHiCheckInterruptCommon(fighter_gobj);

    start_status = (int)fp->status_id;
    start_motion = (int)fp->motion_id;
    gravity = (int)fp->status_vars.fox.specialhi.gravity_delay;

    /* the charge, then the last tic of it with the stick neutral */
    ftFoxSpecialHiHoldSetStatus(fighter_gobj);
    hold = (int)fp->status_vars.fox.specialhi.launch_delay;

    fp->status_vars.fox.specialhi.launch_delay = 1;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = 0;
    ftFoxSpecialHiHoldProcUpdate(fighter_gobj);

    dbglog(DBG_INFO,
           "db: fox-specialhi -- interrupted %d start %d motion %d velx_m %d "
           "gravity %d hold %d air %d status %d angle_mrad %d velx_m %d "
           "vely_m %d frames %d jumps %d pitch_mrad %d want 1 %d %d 20000 %d "
           "%d 1 %d 1570 0 115000 %d %d -1570\n",
           (int)interrupted, start_status, start_motion,
           (int)(fp->physics.vel_ground.x * 1000.0F), gravity, hold,
           (int)(fp->ga == nMPKineticsAir), (int)fp->status_id,
           (int)(fp->status_vars.fox.specialhi.angle * 1000.0F),
           (int)(fp->physics.vel_air.x * 1000.0F),
           (int)(fp->physics.vel_air.y * 1000.0F),
           (int)fp->status_vars.fox.specialhi.anim_frames,
           (int)fp->jumps_used,
           (int)(fp->joints[4]->rotate.vec.f.x * 1000.0F),
           (int)nFTFoxStatusSpecialHiStart, (int)nFTFoxMotionSpecialHiStart,
           (int)FTFOX_FIREFOX_GRAVITY_DELAY, (int)FTFOX_FIREFOX_LAUNCH_DELAY,
           (int)nFTFoxStatusSpecialAirHi, (int)FTFOX_FIREFOX_TRAVEL_TIME,
           (int)fp->attr->jumps_max);
}
#endif /* DB_FOX_SPECIALHI_PROBE */

/* ---- -DDB_SPECIALAIR_PROBE: the aerial command demux on a real fighter --
 *
 * Ports ft/ftcommon/ftcommonspecialair.c -- the three
 * FTKind-indexed tables (dFTCommonSpecialAirN/Hi/LwStatusList) and the one
 * check function that picks among them on the stick's vertical -- and wired
 * it into ftCommonJumpProcInterrupt and ftCommonPassProcInterrupt, which
 * between them are the interrupt column of every aerial row the port has.
 * P1 is Fox on every DB_BOOT_SCENE battle and Fox has all three directions
 * live, so no boot knob. Four passes, each from a fresh airborne Fall: the
 * check driven directly with the stick neutral, up and down, then the same
 * neutral press driven through the real cascade (ftCommonJumpProcInterrupt)
 * with no direct call -- the wiring. The three table facts after them are
 * the two kinds of NULL the tables carry: Link's is a port hole (unported
 * setter), Donkey's air Down-B is the GAME's own (the Hand Slap has no
 * aerial form), and slot 12 of the Up-B table is the decomp's own slip,
 * ftMarioSpecialAirNSetStatus sitting in the Hi column, kept verbatim. */
#ifdef DB_SPECIALAIR_PROBE
static void db_specialair_fall(GObj *fighter_gobj, FTStruct *fp, int stick_y)
{
    ftMainSetStatus(fighter_gobj, nFTCommonStatusFall, 0.0F, 1.0F,
                    FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsAir;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = stick_y;
}

static void db_specialair_probe(int frame)
{
    extern sb32 ftCommonSpecialAirCheckInterruptCommon(GObj *fighter_gobj);
    extern void (*dFTCommonSpecialAirNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirLwStatusList[])(GObj*);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    int got_n, got_hi, got_lw, ret_n, ret_hi, ret_lw, wired, player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindFox)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: specialair -- no Fox player in the battle\n");
        return;
    }

    fp->attr->is_have_specialairn = TRUE;
    fp->attr->is_have_specialairhi = TRUE;
    fp->attr->is_have_specialairlw = TRUE;

    db_specialair_fall(fighter_gobj, fp, 0);
    ret_n = (int)ftCommonSpecialAirCheckInterruptCommon(fighter_gobj);
    got_n = (int)fp->status_id;

    db_specialair_fall(fighter_gobj, fp, FTCOMMON_SPECIALHI_STICK_RANGE_MIN);
    ret_hi = (int)ftCommonSpecialAirCheckInterruptCommon(fighter_gobj);
    got_hi = (int)fp->status_id;

    db_specialair_fall(fighter_gobj, fp, FTCOMMON_SPECIALLW_STICK_RANGE_MIN);
    ret_lw = (int)ftCommonSpecialAirCheckInterruptCommon(fighter_gobj);
    got_lw = (int)fp->status_id;

    /* the wiring: the same neutral B-press, but through the cascade the
     * aerial rows actually carry, with no direct call to the check */
    db_specialair_fall(fighter_gobj, fp, 0);
    ftCommonJumpProcInterrupt(fighter_gobj);
    wired = (int)fp->status_id;

    dbglog(DBG_INFO,
           "db: specialair -- n %d %d hi %d %d lw %d %d wired %d "
           "linknull %d dknull %d bosscol %d want 1 %d 1 %d 1 %d %d 1 1 1\n",
           ret_n, got_n, ret_hi, got_hi, ret_lw, got_lw, wired,
           (int)(dFTCommonSpecialAirNStatusList[nFTKindLink] == NULL),
           (int)(dFTCommonSpecialAirLwStatusList[nFTKindDonkey] == NULL),
           (int)(dFTCommonSpecialAirHiStatusList[nFTKindBoss] ==
                 dFTCommonSpecialAirNStatusList[nFTKindBoss]),
           (int)nFTFoxStatusSpecialAirN,
           (int)nFTFoxStatusSpecialAirHiStart,
           (int)nFTFoxStatusSpecialAirLwStart,
           (int)nFTFoxStatusSpecialAirN);
}
#endif /* DB_SPECIALAIR_PROBE */

/* ---- -DDB_ATTACKAIR_PROBE: the aerials on a real fighter ---------------
 *
 * Ports ft/ftcommon/ftcommonattackair.c's four functions,
 * added the eleven rows they need (209-213 AttackAir, 214-219 LandingAir)
 * and wired ftCommonAttackAirCheckInterruptCommon into
 * ftCommonJumpProcInterrupt and ftCommonPassProcInterrupt behind the step-54
 * SpecialAir check. P1 is Fox on every DB_BOOT_SCENE battle. This drives the
 * check on him for all five directions from a fresh airborne Fall, then the
 * same neutral A-press through the real cascade with no direct call. The
 * last three numbers are the fighter's own is_have_attackair* bits, read
 * whole off his ROM attribute word -- unlike the host, whose mock carries
 * only the subset the port can reach. */
#ifdef DB_ATTACKAIR_PROBE
static int db_attackair_one(GObj *fighter_gobj, FTStruct *fp, int sx, int sy)
{
    ftMainSetStatus(fighter_gobj, nFTCommonStatusFall, 0.0F, 1.0F,
                    FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsAir;
    fp->lr = 1.0F;
    fp->input.pl.button_tap = fp->input.button_mask_a;
    fp->input.pl.stick_range.x = sx;
    fp->input.pl.stick_range.y = sy;

    if (ftCommonAttackAirCheckInterruptCommon(fighter_gobj) == FALSE)
    {
        return -1;
    }
    return (int)fp->status_id;
}

static void db_attackair_probe(int frame)
{
    extern sb32 ftCommonAttackAirCheckInterruptCommon(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    int got_n, got_f, got_b, got_hi, got_lw, wired, hit, player;

    if (frame != 60)   /* fighters and their joints are up well before this */
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindFox)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: attackair -- no Fox player in the battle\n");
        return;
    }

    got_n  = db_attackair_one(fighter_gobj, fp, 0, 0);
    got_f  = db_attackair_one(fighter_gobj, fp, 80, 0);
    got_b  = db_attackair_one(fighter_gobj, fp, -80, 0);
    got_hi = db_attackair_one(fighter_gobj, fp, 0, 80);
    got_lw = db_attackair_one(fighter_gobj, fp, 0, -80);
    /* the down air is the one that installs a proc_hit -- read it here,
     * before the next ftMainSetStatus clears it again */
    hit = (int)(fp->proc_hit == ftCommonAttackAirLwProcHit);

    /* the wiring: the same neutral A-press, but through the cascade the
     * aerial rows actually carry, with no direct call to the check */
    ftMainSetStatus(fighter_gobj, nFTCommonStatusFall, 0.0F, 1.0F,
                    FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsAir;
    fp->input.pl.button_tap = fp->input.button_mask_a;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = 0;
    ftCommonJumpProcInterrupt(fighter_gobj);
    wired = (int)fp->status_id;

    dbglog(DBG_INFO,
           "db: attackair -- n %d f %d b %d hi %d lw %d wired %d hit %d "
           "haven %d havelw %d want %d %d %d %d %d %d 1 1 1\n",
           got_n, got_f, got_b, got_hi, got_lw, wired,
           hit,
           (int)fp->attr->is_have_attackairn,
           (int)fp->attr->is_have_attackairlw,
           (int)nFTCommonStatusAttackAirN, (int)nFTCommonStatusAttackAirF,
           (int)nFTCommonStatusAttackAirB, (int)nFTCommonStatusAttackAirHi,
           (int)nFTCommonStatusAttackAirLw, (int)nFTCommonStatusAttackAirN);
}
#endif /* DB_ATTACKAIR_PROBE */

/* ---- -DDB_SMASH_PROBE: the smashes on a real fighter -------------------
 *
 * Ports ft/ftcommon/ftcommonattacks4.c and its two siblings,
 * filled the seven rows 202-208 they need, and put the run of three at the
 * head of the ground cascade's seven. P1 is Fox on every DB_BOOT_SCENE
 * battle. This drives the three checks on him directly, then the same
 * forward flick through ftCommonWaitProcInterrupt with no direct call.
 *
 * The one thing only the target can answer is the angle spread: the host
 * mock gives all 256 motions an animation, so ftCommonAttackS4SetStatus
 * always takes its five-way branch there. Here the pack is the real one,
 * so `a14` -- a 14-degree stick -- says AttackS4HiS if Fox's pack carries
 * the five-way motions and AttackS4 if it carries only three. Either
 * answer is right; what is being checked is that the branch is decided by
 * data and not by the port. The three is_have bits are read whole off his
 * ROM attribute word, unlike the host's curated mock. */
#ifdef DB_SMASH_PROBE
static int db_smash_one(GObj *fighter_gobj, FTStruct *fp, int sx, int sy,
                        sb32 (*check)(GObj *))
{
    ftMainSetStatus(fighter_gobj, nFTCommonStatusWait, 0.0F, 1.0F,
                    FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsGround;
    fp->lr = 1.0F;
    fp->tap_stick_x = fp->tap_stick_y = 0;
    fp->input.pl.button_tap = fp->input.button_mask_a;
    fp->input.pl.stick_range.x = sx;
    fp->input.pl.stick_range.y = sy;

    if (check(fighter_gobj) == FALSE)
    {
        return -1;
    }
    return (int)fp->status_id;
}

static void db_smash_probe(int frame)
{
    extern sb32 ftCommonAttackS4CheckInterruptCommon(GObj *fighter_gobj);
    extern sb32 ftCommonAttackS4CheckInterruptDash(GObj *fighter_gobj);
    extern sb32 ftCommonAttackHi4CheckInterruptCommon(GObj *fighter_gobj);
    extern sb32 ftCommonAttackLw4CheckInterruptCommon(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    int s4, a14, a37, alw, hi4, lw4, dash, wired, player;

    if (frame != 60)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindFox)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: smash -- no Fox player in the battle\n");
        return;
    }

    s4  = db_smash_one(fighter_gobj, fp, 80, 0, ftCommonAttackS4CheckInterruptCommon);
    a14 = db_smash_one(fighter_gobj, fp, 80, 20, ftCommonAttackS4CheckInterruptCommon);
    a37 = db_smash_one(fighter_gobj, fp, 80, 60, ftCommonAttackS4CheckInterruptCommon);
    alw = db_smash_one(fighter_gobj, fp, 80, -60, ftCommonAttackS4CheckInterruptCommon);
    hi4 = db_smash_one(fighter_gobj, fp, 0, 80, ftCommonAttackHi4CheckInterruptCommon);
    lw4 = db_smash_one(fighter_gobj, fp, 0, -80, ftCommonAttackLw4CheckInterruptCommon);
    /* the dash slot: the same stick with the tap long stale, which the
     * common check refuses and the dash one does not */
    dash = db_smash_one(fighter_gobj, fp, 80, 0, ftCommonAttackS4CheckInterruptDash);

    /* the wiring: the forward flick through the cascade the rows carry */
    ftMainSetStatus(fighter_gobj, nFTCommonStatusWait, 0.0F, 1.0F,
                    FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsGround;
    fp->tap_stick_x = 0;
    fp->input.pl.button_tap = fp->input.button_mask_a;
    fp->input.pl.stick_range.x = 80;
    fp->input.pl.stick_range.y = 0;
    ftCommonWaitProcInterrupt(fighter_gobj);
    wired = (int)fp->status_id;

    dbglog(DBG_INFO,
           "db: smash -- s4 %d a14 %d a37 %d alw %d hi4 %d lw4 %d dash %d "
           "wired %d have %d %d %d want %d [%d|%d] %d %d %d %d %d %d 1 1 1\n",
           s4, a14, a37, alw, hi4, lw4, dash, wired,
           (int)fp->attr->is_have_attacks4,
           (int)fp->attr->is_have_attackhi4,
           (int)fp->attr->is_have_attacklw4,
           (int)nFTCommonStatusAttackS4,
           (int)nFTCommonStatusAttackS4HiS, (int)nFTCommonStatusAttackS4,
           (int)nFTCommonStatusAttackS4Hi, (int)nFTCommonStatusAttackS4Lw,
           (int)nFTCommonStatusAttackHi4, (int)nFTCommonStatusAttackLw4,
           (int)nFTCommonStatusAttackS4, (int)nFTCommonStatusAttackS4);
}
#endif /* DB_SMASH_PROBE */

/* ---- -DDB_ATTACKDASH_PROBE: the dash attack on a real fighter ---------
 *
 * Ports ft/ftcommon/ftcommonattackdash.c, adds row 192 and
 * closes the dash's and the run's last two holes (the neutral-B, called from neither, and the dash attack itself). P1 is
 * Fox. The check takes A alone, so this drives it directly and then puts
 * the same press through ftCommonRunProcInterrupt with no direct call.
 * `transn` is the thing only the target can answer: the row's physics
 * proc reads the animation's TransN joint, so this reports whether Fox's
 * real pack marks the AttackDash motion as TransN root motion the way
 * ft/ftdata.c says it should. */
#ifdef DB_ATTACKDASH_PROBE
static void db_attackdash_probe(int frame)
{
    extern sb32 ftCommonAttackDashCheckInterruptCommon(GObj *fighter_gobj);
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    const Fighter *model;
    int got, none, wired, transn, player;

    if (frame != 60)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindFox)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: attackdash -- no Fox player in the battle\n");
        return;
    }

    ftMainSetStatus(fighter_gobj, nFTCommonStatusWait, 0.0F, 1.0F,
                    FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsGround;
    fp->input.pl.stick_range.x = fp->input.pl.stick_range.y = 0;
    fp->input.pl.button_tap = fp->input.button_mask_a;
    got = (ftCommonAttackDashCheckInterruptCommon(fighter_gobj) != FALSE)
          ? (int)fp->status_id : -1;

    /* no A, nothing -- the check has no other input */
    ftMainSetStatus(fighter_gobj, nFTCommonStatusWait, 0.0F, 1.0F,
                    FTSTATUS_PRESERVE_NONE);
    fp->input.pl.button_tap = 0;
    none = (int)ftCommonAttackDashCheckInterruptCommon(fighter_gobj);

    /* the wiring: the same press through the run's cascade */
    ftMainSetStatus(fighter_gobj, nFTCommonStatusRun, 0.0F, 1.0F,
                    FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsGround;
    fp->input.pl.button_tap = fp->input.button_mask_a;
    ftCommonRunProcInterrupt(fighter_gobj);
    wired = (int)fp->status_id;

    model = dc_model_of(fighter_gobj);
    transn = (nFTCommonMotionAttackDash < (s32)model->motion_count)
             ? (int)((model->motions[nFTCommonMotionAttackDash].flags >> 30) & 1)
             : -1;

    dbglog(DBG_INFO,
           "db: attackdash -- got %d none %d wired %d transn %d have %d "
           "want %d 0 %d 1 1\n",
           got, none, wired, transn, (int)fp->attr->is_have_attackdash,
           (int)nFTCommonStatusAttackDash, (int)nFTCommonStatusAttackDash);
}
#endif /* DB_ATTACKDASH_PROBE */

/* ---- -DDB_SINGLES_PROBE: the four singles on the SH-4 -----------------
 *
 * Fills dFTCommonActionStatusDescs rows 14 (WalkEnd), 58
 * (FallSpecial), 59 (LandingFallSpecial) and 165 (FuraSleep). The rows
 * themselves are a pure read; what only the target can answer is two
 * things.
 *
 * One: FallSpecial's three procs live in ftcommonfallspecial.o, an
 * UNMODIFIED decomp translation unit the target dead-strips with
 * --gc-sections. Until this row named them nothing in the target image
 * referenced ProcInterrupt/ProcPhysics/ProcMap at all, so this reports
 * that the linker kept them and that the pointers in RAM are theirs.
 *
 * Two: whether Fox's real pack has an animation for each of the four
 * motions. The host mock gives all 256 motions anim 0, so the host can
 * never fail that; ft/ftdata.c says these four are real motions and the
 * ROM is the only place that answers.
 *
 * Then it runs the helpless fall for real: the live fighter is put into
 * FallSpecial by the game's own setter and its row physics stepped
 * twice, which before this step ran nothing whatever (the six ported
 * up-Bs all end here). The fighter is returned to Wait afterwards. */
#ifdef DB_SINGLES_PROBE
static void db_singles_probe(int frame)
{
    extern FTStatusDesc dFTCommonActionStatusDescs[];
    extern void ftCommonFallSpecialProcInterrupt(GObj*);
    extern void ftCommonFallSpecialProcPhysics(GObj*);
    extern void ftCommonFallSpecialProcMap(GObj*);
    extern void ftCommonFuraSleepProcUpdate(GObj*);
    extern void ftAnimEndSetWait(GObj*);
    extern void ftCommonLandingProcInterrupt(GObj*);
    extern void ftPhysicsApplyGroundVelFriction(GObj*);
    extern void mpCommonProcFighterOnCliffEdge(GObj*);
    extern void mpCommonSetFighterFallOnGroundBreak(GObj*);
    static const s32 kSingles[4] = {
        nFTCommonStatusWalkEnd, nFTCommonStatusFallSpecial,
        nFTCommonStatusLandingFallSpecial, nFTCommonStatusFuraSleep
    };
    static const s32 kMotions[4] = {
        nFTCommonMotionWalkEnd, nFTCommonMotionFallSpecial,
        nFTCommonMotionLandingFallSpecial, nFTCommonMotionFuraSleep
    };
    GObj *fighter_gobj = NULL;
    FTStruct *fp = NULL;
    const Fighter *model;
    FTStatusDesc *walkend, *fall, *landing, *sleep;
    int rows, anims, player, i, fell, sleep_wait;
    f32 y0, y1;

    if (frame != 60)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL)
        {
            fighter_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: singles -- no player in the battle\n");
        return;
    }

    walkend = &dFTCommonActionStatusDescs[nFTCommonStatusWalkEnd - nFTCommonStatusActionStart];
    fall    = &dFTCommonActionStatusDescs[nFTCommonStatusFallSpecial - nFTCommonStatusActionStart];
    landing = &dFTCommonActionStatusDescs[nFTCommonStatusLandingFallSpecial - nFTCommonStatusActionStart];
    sleep   = &dFTCommonActionStatusDescs[nFTCommonStatusFuraSleep - nFTCommonStatusActionStart];

    rows = (walkend->mflags.motion_id == nFTCommonMotionWalkEnd) &&
           (walkend->proc_update == NULL) && (walkend->proc_interrupt == NULL) &&
           (walkend->proc_physics == NULL) && (walkend->proc_map == NULL) &&

           (fall->mflags.motion_id == nFTCommonMotionFallSpecial) &&
           (fall->proc_update == NULL) &&
           (fall->proc_interrupt == ftCommonFallSpecialProcInterrupt) &&
           (fall->proc_physics == ftCommonFallSpecialProcPhysics) &&
           (fall->proc_map == ftCommonFallSpecialProcMap) &&

           (landing->mflags.motion_id == nFTCommonMotionLandingFallSpecial) &&
           (landing->proc_update == ftAnimEndSetWait) &&
           (landing->proc_interrupt == ftCommonLandingProcInterrupt) &&
           (landing->proc_physics == ftPhysicsApplyGroundVelFriction) &&
           (landing->proc_map == mpCommonProcFighterOnCliffEdge) &&

           (sleep->mflags.motion_id == nFTCommonMotionFuraSleep) &&
           (sleep->proc_update == ftCommonFuraSleepProcUpdate) &&
           (sleep->proc_interrupt == NULL) &&
           (sleep->proc_physics == ftPhysicsApplyGroundVelFriction) &&
           (sleep->proc_map == mpCommonSetFighterFallOnGroundBreak);

    /* the four motions in the real pack, one bit each, low bit = WalkEnd */
    model = dc_model_of(fighter_gobj);
    anims = 0;
    for (i = 0; i < 4; i++)
    {
        if ((kMotions[i] < (s32)model->motion_count) &&
            (model->motions[kMotions[i]].anim >= 0))
        {
            anims |= (1 << i);
        }
    }
    (void)kSingles;

    /* the helpless fall, run for real on the live fighter */
    ftCommonFallSpecialSetStatus(fighter_gobj, 0.5F, FALSE, TRUE, FALSE, 20.0F, FALSE);
    fp->physics.vel_air.y = 0.0F;
    ftCommonFallSpecialProcPhysics(fighter_gobj);
    y0 = fp->physics.vel_air.y;
    ftCommonFallSpecialProcPhysics(fighter_gobj);
    y1 = fp->physics.vel_air.y;
    fell = ((y0 < 0.0F) && (y1 < y0));

    /* and the sleep's window, from the live fighter's own damage */
    ftCommonFuraSleepSetStatus(fighter_gobj);
    sleep_wait = (int)fp->breakout_wait;

    ftMainSetStatus(fighter_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsGround;
    fp->physics.vel_air.x = fp->physics.vel_air.y = 0.0F;

    dbglog(DBG_INFO,
           "db: singles -- rows %d anims 0x%X fell %d sleepwait %d "
           "want 1 0xF 1 %d\n",
           rows, anims, fell, sleep_wait,
           (int)(FTCOMMON_FURASLEEP_BREAKOUT_WAIT_DEFAULT - fp->percent_damage <= 0
                 ? FTCOMMON_FURASLEEP_BREAKOUT_WAIT_MIN
                 : FTCOMMON_FURASLEEP_BREAKOUT_WAIT_DEFAULT - fp->percent_damage +
                   FTCOMMON_FURASLEEP_BREAKOUT_WAIT_MIN));
}
#endif /* DB_SINGLES_PROBE */

/* ---- -DDB_YOSHI_EGGLAY_PROBE: Egg Lay end to end on the SH-4 ---------
 *
 * Compiles ft/ftchar/ftyoshi/ftyoshispecialn.c and
 * ft/ftcommon/ftcommoncaptureyoshi.c unmodified, filled Yoshi's six
 * SpecialN rows and common rows 177/178, and put ftYoshiSpecialNSetStatus
 * in the Neutral-B demux at slot 6. Needs a real Yoshi bound, so build
 * with -DDB_BOOT_P2_KIND=nFTKindYoshi (P1 stays Fox and is the victim).
 *
 * Three things only the target answers. `demux` runs the real B-press
 * through ftCommonSpecialNCheckInterruptCommon rather than calling the
 * setter, so the slot is proven from the input side. `anims` says whether
 * Yoshi's real pack has an animation for each of the four SpecialN
 * motions and Fox's has the egg's -- the host mock gives all 256 motions
 * anim 0 and can never fail that. And `egg` walks the whole handoff on
 * two live fighters with real packs, joints and collision data: Yoshi
 * swallows Fox, the three-stage counter runs, and Fox ends in status 178
 * with the damage collision taken from the file's own per-victim row
 * (Fox's is 155 up, 171 across). Both fighters are put back to Wait
 * afterwards. */
#ifdef DB_YOSHI_EGGLAY_PROBE
static void db_yoshi_egglay_probe(int frame)
{
    extern FTStatusDesc dFTYoshiSpecialStatusDescs[];
    extern FTStatusDesc dFTCommonActionStatusDescs[];
    extern sb32 ftCommonSpecialNCheckInterruptCommon(GObj *fighter_gobj);
    extern void (*dFTCommonSpecialNStatusList[])(GObj*);
    extern ftCommonYoshiEggDesc dFTCommonYoshiEggDamageCollDescs[];
    extern void efManagerYoshiEggLayProcUpdate(GObj *effect_gobj);
    static const s32 kMotions[4] = {
        nFTYoshiMotionSpecialN, nFTYoshiMotionSpecialNCatch,
        nFTYoshiMotionSpecialNRelease, nFTYoshiMotionSpecialAirN
    };
    GObj *yoshi_gobj = NULL, *victim_gobj = NULL;
    FTStruct *yfp = NULL, *vfp = NULL;
    const Fighter *model;
    FTStatusDesc *sn, *cap, *egg;
    ftCommonYoshiEggDesc *desc;
    int rows, anims, demux, stage, egg_ok, player, i;
    int fxok, fxjt, fxix, fxan;
    f32 fxsc;

    if (frame != 60)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g == NULL)
        {
            continue;
        }
        if (ftGetStruct(g)->fkind == nFTKindYoshi)
        {
            yoshi_gobj = g;
            yfp = ftGetStruct(g);
        }
        else if (victim_gobj == NULL)
        {
            victim_gobj = g;
            vfp = ftGetStruct(g);
        }
    }
    if (yfp == NULL || vfp == NULL)
    {
        dbglog(DBG_INFO, "db: yoshi-egglay -- need a Yoshi and one other "
                         "player (build with "
                         "-DDB_BOOT_P2_KIND=nFTKindYoshi)\n");
        return;
    }

    sn  = &dFTYoshiSpecialStatusDescs[nFTYoshiStatusSpecialN - nFTCommonStatusSpecialStart];
    cap = &dFTCommonActionStatusDescs[nFTCommonStatusCaptureYoshi - nFTCommonStatusActionStart];
    egg = &dFTCommonActionStatusDescs[nFTCommonStatusYoshiEgg - nFTCommonStatusActionStart];

    rows = (sn->mflags.motion_id == nFTYoshiMotionSpecialN) &&
           (sn->proc_map == ftYoshiSpecialNProcMap) &&
           (dFTYoshiSpecialStatusDescs[nFTYoshiStatusSpecialAirNRelease - nFTCommonStatusSpecialStart].proc_update
                == ftYoshiSpecialAirNReleaseProcUpdate) &&
           (cap->proc_physics == ftCommonCaptureYoshiProcPhysics) &&
           (cap->proc_update == NULL) &&
           (egg->proc_update == ftCommonYoshiEggProcUpdate) &&
           (egg->proc_map == ftCommonYoshiEggProcMap) &&
           (dFTCommonSpecialNStatusList[nFTKindYoshi] == ftYoshiSpecialNSetStatus);

    /* the four SpecialN motions in Yoshi's real pack, plus the egg's in
     * the victim's -- one bit each, low bit = SpecialN */
    model = dc_model_of(yoshi_gobj);
    anims = 0;
    for (i = 0; i < 4; i++)
    {
        if ((kMotions[i] < (s32)model->motion_count) &&
            (model->motions[kMotions[i]].anim >= 0))
        {
            anims |= (1 << i);
        }
    }
    model = dc_model_of(victim_gobj);
    if ((nFTCommonMotionYoshiEgg < (s32)model->motion_count) &&
        (model->motions[nFTCommonMotionYoshiEgg].anim >= 0))
    {
        anims |= (1 << 4);
    }

    /* the demux, from the input side: B with a neutral stick on the ground */
    ftMainSetStatus(yoshi_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    yfp->ga = nMPKineticsGround;
    yfp->attr->is_have_specialn = TRUE;
    yfp->input.pl.stick_range.x = yfp->input.pl.stick_range.y = 0;
    yfp->input.pl.button_tap = yfp->input.button_mask_b;
    demux = (ftCommonSpecialNCheckInterruptCommon(yoshi_gobj) != FALSE)
            ? (int)yfp->status_id : -1;

    /* the connect and the three stages, on two live fighters */
    yfp->lr = +1;
    yfp->search_gobj = victim_gobj;
    ftYoshiSpecialNCatchProcCatch(yoshi_gobj);
    ftCommonCaptureYoshiProcCapture(victim_gobj, yoshi_gobj);
    stage = (vfp->status_id == nFTCommonStatusCaptureYoshi) ? 1 : 0;

    yfp->motion_vars.flags.flag2 = 1;
    ftYoshiSpecialNCatchUpdateCaptureVars(yfp);
    ftCommonCaptureYoshiProcPhysics(victim_gobj);
    stage += (vfp->status_vars.common.captureyoshi.stage == 2) ? 1 : 0;

    yfp->motion_vars.flags.flag1 = 1;
    ftYoshiSpecialNCatchUpdateCaptureVars(yfp);
    ftCommonCaptureYoshiProcPhysics(victim_gobj);
    stage += (vfp->status_id == nFTCommonStatusYoshiEgg) ? 1 : 0;

    desc = &dFTCommonYoshiEggDamageCollDescs[vfp->fkind];
    egg_ok = (vfp->status_id == nFTCommonStatusYoshiEgg) &&
             (vfp->is_invisible == TRUE) &&
             (vfp->breakout_wait == FTCOMMON_YOSHIEGG_BREAKOUT_INPUTS_MIN) &&
             (vfp->damage_colls[0].size.x == desc->size.x) &&
             (vfp->damage_colls[0].offset.y == desc->offset.y) &&
             (vfp->is_damage_coll_modify == TRUE);

    /* The egg is drawn. A NULL efManagerYoshiEggLayMakeEffect would leave the caught
     * fighter -- which sets is_invisible itself -- simply vanished.
     *
     * `fx` is the effect being there at all, `fxjt` its pack's joints,
     * `fxsc` the size the maker wrote in (the kind's own effect_size,
     * not a default), and `fxix` the animation index: 2 on the way in,
     * and 1 after ftCommonCaptureYoshiProcPhysics has raised
     * force_index and the ProcUpdate has acted on it -- which is the
     * whole reason the pack carries three animations. */
    {
        GObj *fx = vfp->status_vars.common.captureyoshi.effect_gobj;
        EFStruct *ep;

        fxjt = fxix = fxan = -1;
        fxsc = 0;
        fxok = (fx != NULL);

        if (fx != NULL)
        {
            const Fighter *fm = dc_model_of(fx);

            fxjt = (fm != NULL && fm->hd != NULL)
                   ? (int)fm->hd->joint_count : -1;
            fxsc = DObjGetStruct(fx)->scale.vec.f.x;

            ep = efGetStruct(fx);
            fxix = (ep->effect_vars.yoshi_egg_lay.index == 2) ? 2 : -1;

            /* what the escape counter does when it runs out */
            ep->effect_vars.yoshi_egg_lay.force_index = 1;
            efManagerYoshiEggLayProcUpdate(fx);

            if (fxix == 2)
            {
                fxix = (int)ep->effect_vars.yoshi_egg_lay.index;
            }
            /* and WHICH animation that put on the tree's root. The index
             * above is the decomp's own state word and would read 1
             * whatever the port installed; this is the pack block the
             * root is actually playing, which is what the port's
             * dEFManagerYoshiEggLayAnimJoints decides. Pack animation 2
             * is the break. */
            if (fm != NULL && fm->hd != NULL && fm->anims != NULL &&
                fm->hd->anim_count == 3)
            {
                const u8 *p = (const u8 *)
                              DObjGetStruct(fx)->anim_joint.event32;
                const u8 *lo2 = (const u8 *)fm->blob
                                + fm->anims[2].off_words;
                const u8 *lo0 = (const u8 *)fm->blob
                                + fm->anims[0].off_words;

                /* inside the break's block and outside the throw's --
                 * the ranges are disjoint, so that is the animation */
                fxan = (p >= lo2) &&
                       (p < lo2 + fm->anims[2].nwords * 4) &&
                       !((p >= lo0) &&
                         (p < lo0 + fm->anims[0].nwords * 4));
            }
            gcEjectGObj(fx);
            vfp->status_vars.common.captureyoshi.effect_gobj = NULL;
            vfp->is_effect_attach = FALSE;
        }
    }
    dbglog(DBG_INFO,
           "db: yoshi-egglay -- rows %d anims 0x%X demux %d stage %d egg %d "
           "fx %d/%d scale %d/%d index %d anim %d (victim %d off %d "
           "size %d) want 1 0x1F %d 3 1 1 2 <the two scales equal> 1 1\n",
           rows, anims, demux, stage, egg_ok,
           fxok, fxjt, (int)(fxsc * 100.0F),
           (int)(desc->effect_size * 100.0F), fxix, fxan,
           (int)vfp->fkind, (int)desc->offset.y, (int)desc->size.x,
           (int)nFTYoshiStatusSpecialN);

    /* put both fighters back */
    ftMainSetStatus(victim_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    vfp->is_invisible = vfp->is_shadow_hide = FALSE;
    vfp->is_damage_coll_modify = vfp->is_hitstatus_nodamage = FALSE;
    vfp->capture_gobj = NULL;
    vfp->proc_trap = NULL;
    vfp->damage_mul = 1.0F;
    vfp->physics.vel_air.x = vfp->physics.vel_air.y = 0.0F;
    ftParamSetCaptureImmuneMask(vfp, FTCATCHKIND_MASK_NONE);
    ftMainSetStatus(yoshi_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    yfp->catch_gobj = yfp->search_gobj = NULL;
    yfp->input.pl.button_tap = 0;
    ftParamSetCaptureImmuneMask(yfp, FTCATCHKIND_MASK_NONE);
}
#endif /* DB_YOSHI_EGGLAY_PROBE */

/* ---- -DDB_YOSHI_EGGTHROW_PROBE: Egg Throw on the SH-4 -----------------
 *
 * Compiles ft/ftchar/ftyoshi/ftyoshispecialhi.c and
 * wp/wpyoshi/wpyoshieggthrow.c unmodified, filled Yoshi's rows 222/223
 * and put his setters in the four Up-B demux slots -- which finishes his
 * special table. Needs -DDB_BOOT_P2_KIND=nFTKindYoshi.
 *
 * `demux` is the whole point and is proven from the input side: a real
 * stick-up B-tap through ftCommonSpecialHiCheckInterruptCommon, not a
 * call to the setter. `anims` says Yoshi's real pack has both SpecialHi
 * motions (the host mock gives all 256 motions anim 0 and can never fail
 * that). `charge` runs the throw_force counter the way the move does,
 * and `beats` walks the motion script's two flag2 beats -- the make and
 * the throw -- on the live fighter, where the make is expected to come
 * back with NO egg because the weapon is unmodeled, and the throw beat is
 * expected to raise flag1 anyway. `noegg` is that expectation stated as a
 * check rather than assumed: if a later step gives the egg a
 * sWPManagerModels row, this probe is where it will first show up. */
#ifdef DB_YOSHI_EGGTHROW_PROBE
static void db_yoshi_eggthrow_probe(int frame)
{
    extern FTStatusDesc dFTYoshiSpecialStatusDescs[];
    extern sb32 ftCommonSpecialHiCheckInterruptCommon(GObj *fighter_gobj);
    extern sb32 ftCommonSpecialAirCheckInterruptCommon(GObj *fighter_gobj);
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirHiStatusList[])(GObj*);
    GObj *yoshi_gobj = NULL;
    FTStruct *fp = NULL;
    const Fighter *model;
    FTStatusDesc *hi, *air;
    int rows, anims, demux, charge, beats, noegg, player, i;

    if (frame != 60)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindYoshi)
        {
            yoshi_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: yoshi-eggthrow -- no Yoshi player in the "
                         "battle (build with "
                         "-DDB_BOOT_P2_KIND=nFTKindYoshi)\n");
        return;
    }

    hi  = &dFTYoshiSpecialStatusDescs[nFTYoshiStatusSpecialHi - nFTCommonStatusSpecialStart];
    air = &dFTYoshiSpecialStatusDescs[nFTYoshiStatusSpecialAirHi - nFTCommonStatusSpecialStart];

    rows = (hi->mflags.motion_id == nFTYoshiMotionSpecialHi) &&
           (hi->proc_update == ftYoshiSpecialHiProcUpdate) &&
           (hi->proc_physics == ftYoshiSpecialHiProcPhysics) &&
           (hi->proc_map == ftYoshiSpecialHiProcMap) &&
           (hi->sflags.is_projectile == TRUE) &&
           (air->mflags.motion_id == nFTYoshiMotionSpecialAirHi) &&
           (air->proc_update == ftYoshiSpecialAirHiProcUpdate) &&
           (air->proc_physics == ftYoshiSpecialAirHiProcPhysics) &&
           (air->proc_map == ftYoshiSpecialAirHiProcMap) &&
           (air->sflags.ga == nMPKineticsAir) &&
           (dFTCommonSpecialHiStatusList[nFTKindYoshi] == ftYoshiSpecialHiSetStatus) &&
           (dFTCommonSpecialAirHiStatusList[nFTKindYoshi] == ftYoshiSpecialAirHiSetStatus);

    model = dc_model_of(yoshi_gobj);
    anims = 0;
    if ((nFTYoshiMotionSpecialHi < (s32)model->motion_count) &&
        (model->motions[nFTYoshiMotionSpecialHi].anim >= 0))
    {
        anims |= 1;
    }
    if ((nFTYoshiMotionSpecialAirHi < (s32)model->motion_count) &&
        (model->motions[nFTYoshiMotionSpecialAirHi].anim >= 0))
    {
        anims |= 2;
    }

    /* the demux, from the input side */
    ftMainSetStatus(yoshi_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsGround;
    fp->attr->is_have_specialhi = TRUE;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    demux = (ftCommonSpecialHiCheckInterruptCommon(yoshi_gobj) != FALSE)
            ? (int)fp->status_id : -1;

    /* the charge: B held, one per tic */
    fp->input.pl.button_hold = fp->input.button_mask_b;
    for (i = 0; i < 7; i++)
    {
        ftYoshiSpecialHiUpdateEggThrowForce(yoshi_gobj);
    }
    charge = (int)fp->status_vars.yoshi.specialhi.throw_force;
    fp->input.pl.button_hold = 0;

    /* the motion script's two beats, on the live fighter and its real
     * joints -- the egg hangs off YRotN, which a real pack has */
    fp->motion_vars.flags.flag1 = 0;
    fp->motion_vars.flags.flag2 = 1;
    ftYoshiSpecialHiUpdateEggVars(yoshi_gobj);
    noegg = (fp->status_vars.yoshi.specialhi.egg_gobj == NULL);
    beats = (fp->motion_vars.flags.flag2 == 0) && (fp->motion_vars.flags.flag1 == 0);
    fp->motion_vars.flags.flag2 = 2;
    ftYoshiSpecialHiUpdateEggVars(yoshi_gobj);
    beats += (fp->motion_vars.flags.flag2 == 0) && (fp->motion_vars.flags.flag1 == 1);

    dbglog(DBG_INFO,
           "db: yoshi-eggthrow -- rows %d anims 0x%X demux %d charge %d "
           "beats %d noegg %d want 1 0x3 %d 7 2 1\n",
           rows, anims, demux, charge, beats, noegg,
           (int)nFTYoshiStatusSpecialHi);

    ftMainSetStatus(yoshi_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->input.pl.button_tap = 0;
    fp->input.pl.stick_range.y = 0;
    fp->motion_vars.flags.flag1 = fp->motion_vars.flags.flag2 = 0;
}
#endif /* DB_YOSHI_EGGTHROW_PROBE */

/* ---- -DDB_KIRBY_CUTTER_PROBE: the Final Cutter on the SH-4 ------------
 *
 * Compiles ft/ftchar/ftkirby/ftkirbyspecialhi.c and
 * wp/wpkirby/wpkirbycutter.c unmodified, filled Kirby's rows 256-259 and
 * put his setters in the four Up-B demux slots. P3 is Kirby in kBootKinds,
 * so this needs -DDB_BOOT_PLAYERS=4 rather than a knob of its own.
 *
 * `demux` proves the slot from the input side. `anims` says Kirby's real
 * pack has all four SpecialHi motions, which the host mock -- every motion
 * anim 0 -- can never fail. `effects` is the one that matters most here:
 * it runs the motion script's four effect beats (flag2 cases 2 to 5) on
 * the live fighter and checks each request STAYS raised, which is the
 * shape a stubbed maker leaves behind. If a later step bakes
 * gFTDataKirbySpecial2 into a pack, this is the line that changes, and it
 * should change to 0. `blade` is the same statement for the weapon: the
 * landing's beat clears flag0 and no cutter comes back. */
#ifdef DB_KIRBY_CUTTER_PROBE
static void db_kirby_cutter_probe(int frame)
{
    extern FTStatusDesc dFTKirbySpecialStatusDescs[];
    extern sb32 ftCommonSpecialHiCheckInterruptCommon(GObj *fighter_gobj);
    extern sb32 ftCommonSpecialAirCheckInterruptCommon(GObj *fighter_gobj);
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirHiStatusList[])(GObj*);
    static const s32 kMotions[4] = {
        nFTKirbyMotionSpecialHi, nFTKirbyMotionSpecialHiLanding,
        nFTKirbyMotionSpecialAirHi, nFTKirbyMotionSpecialAirHiFall
    };
    GObj *kirby_gobj = NULL;
    FTStruct *fp = NULL;
    const Fighter *model;
    FTStatusDesc *hi, *land, *air, *fall;
    int rows, anims, demux, effects, cutjoints, blade, player, i;

    if (frame != 60)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindKirby)
        {
            kirby_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: kirby-cutter -- no Kirby player in the battle "
                         "(build with -DDB_BOOT_PLAYERS=4)\n");
        return;
    }

    hi   = &dFTKirbySpecialStatusDescs[nFTKirbyStatusSpecialHi - nFTCommonStatusSpecialStart];
    land = &dFTKirbySpecialStatusDescs[nFTKirbyStatusSpecialHiLanding - nFTCommonStatusSpecialStart];
    air  = &dFTKirbySpecialStatusDescs[nFTKirbyStatusSpecialAirHi - nFTCommonStatusSpecialStart];
    fall = &dFTKirbySpecialStatusDescs[nFTKirbyStatusSpecialAirHiFall - nFTCommonStatusSpecialStart];

    rows = (hi->proc_update == ftKirbySpecialHiProcUpdate) &&
           (hi->proc_physics == ftKirbySpecialHiProcPhysics) &&
           (hi->proc_map == ftKirbySpecialHiProcMap) &&
           (hi->sflags.is_projectile == TRUE) &&
           (land->proc_update == ftKirbySpecialHiLandingProcUpdate) &&
           (land->proc_map == mpCommonSetFighterFallOnGroundBreak) &&
           (air->proc_update == ftKirbySpecialHiProcUpdate) &&
           (air->proc_physics == ftKirbySpecialAirHiProcPhysics) &&
           (air->sflags.ga == nMPKineticsAir) &&
           (fall->proc_update == NULL) &&
           (fall->proc_physics == ftKirbySpecialAirHiFallProcPhysics) &&
           (fall->proc_map == ftKirbySpecialAirHiFallProcMap) &&
           (dFTCommonSpecialHiStatusList[nFTKindKirby] == ftKirbySpecialHiSetStatus) &&
           (dFTCommonSpecialAirHiStatusList[nFTKindKirby] == ftKirbySpecialAirHiSetStatus);

    model = dc_model_of(kirby_gobj);
    anims = 0;
    for (i = 0; i < 4; i++)
    {
        if ((kMotions[i] < (s32)model->motion_count) &&
            (model->motions[kMotions[i]].anim >= 0))
        {
            anims |= (1 << i);
        }
    }

    /* the demux, from the input side */
    ftMainSetStatus(kirby_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsGround;
    fp->attr->is_have_specialhi = TRUE;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    demux = (ftCommonSpecialHiCheckInterruptCommon(kirby_gobj) != FALSE)
            ? (int)fp->status_id : -1;

    /* The four effect beats. This check is inverted.
     *
     * With a stub maker, ftKirbySpecialHiUpdateEffect clears motion_vars.flags.flag2 INSIDE
     * its `if (make(...) != NULL)` -- so a NULL maker left the request
     * standing and the switch asked again the next frame, for as long as
     * the status lasted. This probe pinned exactly that: flag2 unchanged
     * and is_effect_attach still FALSE.
     *
     * The four packs exist now, so each beat has to CONSUME its request
     * and raise is_effect_attach. Case 2 is the blade, 3 the up arc, 4
     * the down arc, 5 the trail (ftkirbyspecialhi.c:48-73), and `joints`
     * records what each one drew -- 2, 5, 6 and 3, which is the four
     * packs in that order and nothing else in this port. */
    effects = 0;
    cutjoints = 0;
    for (i = 2; i <= 5; i++)
    {
        GObj *g, *made = NULL;
        int n = 0, k;

        for (g = gGCCommonLinks[nGCCommonLinkIDEffect]; g != NULL;
             g = g->link_next)
        {
            n++;
        }
        fp->is_effect_attach = FALSE;
        fp->motion_vars.flags.flag2 = i;
        ftKirbySpecialHiUpdateEffect(kirby_gobj);

        if ((fp->motion_vars.flags.flag2 == 0) && (fp->is_effect_attach != FALSE))
        {
            effects |= (1 << (i - 2));
        }
        /* gcMakeGObjSPAfter appends, so the one just made is the tail */
        k = 0;
        for (g = gGCCommonLinks[nGCCommonLinkIDEffect]; g != NULL;
             g = g->link_next)
        {
            if (k++ == n)
            {
                made = g;
            }
        }
        if (made != NULL)
        {
            const Fighter *cm = dc_model_of(made);

            cutjoints |= ((cm != NULL && cm->hd != NULL)
                          ? (int)cm->hd->joint_count : 0) << ((i - 2) * 4);
            gcEjectGObj(made);
        }
    }
    fp->motion_vars.flags.flag2 = 0;
    fp->is_effect_attach = FALSE;

    /* the blade: the landing's one beat, on the live fighter's real joints */
    ftKirbySpecialHiLandingSetStatus(kirby_gobj);
    fp->lr = +1;
    fp->motion_vars.flags.flag0 = 1;
    ftKirbySpecialHiLandingProcUpdate(kirby_gobj);
    blade = (fp->motion_vars.flags.flag0 == 0);

    dbglog(DBG_INFO,
           "db: kirby-cutter -- rows %d anims 0x%X demux %d effects 0x%X "
           "joints 0x%X blade %d want 1 0xF %d 0xF 0x3652 1\n",
           rows, anims, demux, effects, cutjoints, blade,
           (int)nFTKirbyStatusSpecialHi);

    ftMainSetStatus(kirby_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->input.pl.button_tap = 0;
    fp->input.pl.stick_range.y = 0;
    fp->motion_vars.flags.flag0 = fp->motion_vars.flags.flag1 =
        fp->motion_vars.flags.flag2 = 0;
    fp->is_effect_attach = FALSE;
}
#endif /* DB_KIRBY_CUTTER_PROBE */

/* ---- -DDB_KIRBY_INHALE_PROBE: the copy inhale on the SH-4 --------------
 *
 * Compiles ft/ftchar/ftkirby/ftkirbyspecialn.c,
 * ft/ftcommon/ftcommoncapturekirby.c and ft/ftcommon/ftcommoncapture.c
 * unmodified, filled Kirby's eighteen rows 269-286 and the swallowed
 * fighter's four common rows 173-176, and wired the two-level demux.
 * P3 is Kirby in kBootKinds, so -DDB_BOOT_PLAYERS=4 and no new knob.
 *
 * `demux` walks BOTH levels from the input side, which is the whole point
 * of this probe -- the host can check the tables, but only the target can
 * show a real B-press arriving at SpecialNStart through the selector.
 * `copy` is the inner table's shape on the target: Kirby's own mouth and
 * the polygons live, the eight unported ones NULL, Purin's live.
 * `catchp` says the inhale armed the catch, which is what makes the mouth
 * able to hold a fighter at all. `wind` is the divergence: the inhale's
 * particle request should still be standing after the update, because
 * gFTDataKirbyParticleBankID is not loaded and the maker is stubbed. If
 * the per-fighter banks are ever loaded, this is the line that changes,
 * and it should change to 0. `rows` covers the two rows whose shape is
 * easiest to get wrong -- the air catch that is nMPKineticsGround, and
 * the swallowed fighter's CaptureWaitKirby with no physics at all. */
#ifdef DB_KIRBY_INHALE_PROBE
static void db_kirby_inhale_probe(int frame)
{
    extern FTStatusDesc dFTKirbySpecialStatusDescs[];
    extern FTStatusDesc dFTCommonActionStatusDescs[];
    extern sb32 ftCommonSpecialNCheckInterruptCommon(GObj *fighter_gobj);
    extern void (*dFTCommonSpecialNStatusList[])(GObj*);
    extern void (*dFTKirbySpecialNStatusList[])(GObj*);
    extern void (*dFTKirbySpecialAirNStatusList[])(GObj*);
    static const s32 kMotions[4] = {
        nFTKirbyMotionSpecialNStart, nFTKirbyMotionSpecialNLoop,
        nFTKirbyMotionSpecialNThrow, nFTKirbyMotionSpecialAirNStart
    };
    GObj *kirby_gobj = NULL;
    FTStruct *fp = NULL;
    const Fighter *model;
    FTStatusDesc *aircatch, *capwait, *thrown;
    int rows, anims, demux, copy, catchp, wind, player, i;

    if (frame != 60)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindKirby)
        {
            kirby_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: kirby-inhale -- no Kirby player in the battle "
                         "(build with -DDB_BOOT_PLAYERS=4)\n");
        return;
    }

    aircatch = &dFTKirbySpecialStatusDescs[nFTKirbyStatusSpecialAirNCatch - nFTCommonStatusSpecialStart];
    capwait  = &dFTCommonActionStatusDescs[nFTCommonStatusCaptureWaitKirby - nFTCommonStatusActionStart];
    thrown   = &dFTCommonActionStatusDescs[nFTCommonStatusThrownKirbyStar - nFTCommonStatusActionStart];

    rows = (aircatch->proc_update == ftKirbySpecialNCatchProcUpdate) &&
           (aircatch->proc_map == ftKirbySpecialAirNCatchProcMap) &&
           (aircatch->sflags.ga == nMPKineticsGround) &&
           (capwait->proc_physics == NULL) &&
           (capwait->proc_interrupt == ftCommonCaptureWaitKirbyProcInterrupt) &&
           (capwait->proc_map == ftCommonCaptureWaitKirbyProcMap) &&
           (thrown->proc_update == ftCommonThrownKirbyStarProcUpdate) &&
           (thrown->proc_map == ftCommonThrownCommonStarProcMap);

    copy = (dFTKirbySpecialNStatusList[nFTKindKirby] == ftKirbySpecialNStartSetStatus) &&
           (dFTKirbySpecialAirNStatusList[nFTKindKirby] == ftKirbySpecialAirNStartSetStatus) &&
           (dFTKirbySpecialNStatusList[nFTKindPurin] == ftKirbyCopyPurinSpecialNSetStatus) &&
           (dFTKirbySpecialNStatusList[nFTKindNMario] == ftKirbySpecialNStartSetStatus) &&
           (dFTKirbySpecialNStatusList[nFTKindGDonkey] == ftKirbySpecialNStartSetStatus) &&
           (dFTKirbySpecialNStatusList[nFTKindMario] == NULL) &&
           (dFTKirbySpecialNStatusList[nFTKindLuigi] == NULL) &&
           (dFTCommonSpecialNStatusList[nFTKindKirby] == ftKirbySpecialNSetStatusSelect);

    model = dc_model_of(kirby_gobj);
    anims = 0;
    for (i = 0; i < 4; i++)
    {
        if ((kMotions[i] < (s32)model->motion_count) &&
            (model->motions[kMotions[i]].anim >= 0))
        {
            anims |= (1 << i);
        }
    }

    /* both demux levels, from a real B-press */
    ftMainSetStatus(kirby_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsGround;
    fp->attr->is_have_specialn = TRUE;
    fp->passive_vars.kirby.copy_id = nFTKindKirby;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = 0;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    demux = (ftCommonSpecialNCheckInterruptCommon(kirby_gobj) != FALSE)
            ? (int)fp->status_id : -1;
    catchp = (fp->is_catchstatus != FALSE) &&
             (fp->proc_catch == ftKirbySpecialNCatchProcCatch) &&
             (fp->proc_capture == ftCommonCaptureKirbyProcCapture) &&
             (fp->status_vars.kirby.specialn.copy_id == nFTKindKirby);

    /* the inhale's wind: the request should still be standing */
    fp->is_effect_attach = FALSE;
    fp->motion_vars.flags.flag0 = 1;
    ftKirbySpecialNLoopProcUpdate(kirby_gobj);
    wind = (fp->motion_vars.flags.flag0 == 1) && (fp->is_effect_attach == FALSE);

    dbglog(DBG_INFO,
           "db: kirby-inhale -- rows %d copy %d anims 0x%X demux %d "
           "catch %d wind %d want 1 1 0xF %d 1 1\n",
           rows, copy, anims, demux, catchp, wind,
           (int)nFTKirbyStatusSpecialNStart);

    ftMainSetStatus(kirby_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->input.pl.button_tap = 0;
    fp->motion_vars.flags.flag0 = fp->motion_vars.flags.flag1 =
        fp->motion_vars.flags.flag2 = 0;
    fp->is_effect_attach = FALSE;
    fp->is_catchstatus = FALSE;
}
#endif /* DB_KIRBY_INHALE_PROBE */

/* ---- -DDB_KIRBY_COPY_PROBE: the copy half on the SH-4 ------------------
 *
 * Step 63 put six ftkirbycopy*specialn.c files in the build and the
 * twenty-nine rows they set. Four of those things the host cannot show.
 *
 * `anims` is the usual one: the host mock gives every motion anim = 0, so
 * only the target says whether the seven copy motions are really in
 * Kirby's motion file. `demux` is a real B-press walking BOTH levels --
 * the common list to ftKirbySpecialNSetStatusSelect, then the Kirby list
 * keyed on a swallowed Donkey -- and landing in CopyDonkeySpecialNStart.
 * `mario` is the one function that reaches four statuses: with a
 * swallowed Luigi the same setter must land in CopyLuigiSpecialN.
 * `speed` is Samus's charge read off the real DObj at max bank, 0.84 by
 * ftkirbycopysamusspecialn.c:326-333, which on host is only ever the mock
 * DObj's field. `purin` is the array-length fix: her rows must lie inside
 * dFTKirbySpecialStatusDescs, so this checks a real swallowed Purin lands in a real row. */
#ifdef DB_KIRBY_COPY_PROBE
static void db_kirby_copy_probe(int frame)
{
    extern FTStatusDesc dFTKirbySpecialStatusDescs[];
    extern sb32 ftCommonSpecialNCheckInterruptCommon(GObj *fighter_gobj);
    extern sb32 ftCommonAttack100StartCheckInterruptCommon(GObj *fighter_gobj);
    extern void (*dFTKirbySpecialNStatusList[])(GObj*);
    extern void (*dFTKirbySpecialAirNStatusList[])(GObj*);
    static const s32 kMotions[7] = {
        nFTKirbyMotionCopyMarioSpecialN, nFTKirbyMotionCopyFoxSpecialN,
        nFTKirbyMotionCopyDonkeySpecialNStart, nFTKirbyMotionCopySamusSpecialNStart,
        nFTKirbyMotionCopyCaptainSpecialN, nFTKirbyMotionCopyYoshiSpecialN,
        nFTKirbyMotionCopyPurinSpecialN
    };
    GObj *kirby_gobj = NULL;
    FTStruct *fp = NULL;
    const Fighter *model;
    FTStatusDesc *row;
    int rows, anims, demux, mario, purin, slots, speed, player, i;

    if (frame != 90)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindKirby)
        {
            kirby_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: kirby-copy -- no Kirby player in the battle "
                         "(build with -DDB_BOOT_PLAYERS=4)\n");
        return;
    }

    /* the eight mouths this step lights up, and the two still dark
     * (Link's is the third) */
    slots = (dFTKirbySpecialNStatusList[nFTKindMario] == ftKirbyCopyMarioSpecialNSetStatus) &&
            (dFTKirbySpecialNStatusList[nFTKindLuigi] == ftKirbyCopyMarioSpecialNSetStatus) &&
            (dFTKirbySpecialNStatusList[nFTKindMMario] == ftKirbyCopyMarioSpecialNSetStatus) &&
            (dFTKirbySpecialNStatusList[nFTKindFox] == ftKirbyCopyFoxSpecialNSetStatus) &&
            (dFTKirbySpecialNStatusList[nFTKindDonkey] == ftKirbyCopyDonkeySpecialNStartSetStatus) &&
            (dFTKirbySpecialNStatusList[nFTKindSamus] == ftKirbyCopySamusSpecialNStartSetStatus) &&
            (dFTKirbySpecialNStatusList[nFTKindYoshi] == ftKirbyCopyYoshiSpecialNSetStatus) &&
            (dFTKirbySpecialNStatusList[nFTKindCaptain] == ftKirbyCopyCaptainSpecialNSetStatus) &&
            (dFTKirbySpecialAirNStatusList[nFTKindDonkey] == ftKirbyCopyDonkeySpecialAirNStartSetStatus) &&
            /* This pair is checked against what is wired: Pikachu's setter
             * is filled, so the want is 1 (the Link
             * probe's `wp 0`), and the same lesson: a want is a claim
             * and goes stale like any other. Ness's is the one still
             * NULL, and his waits on it/. */
            (dFTKirbySpecialNStatusList[nFTKindPikachu] ==
             ftKirbyCopyPikachuSpecialNSetStatus) &&
            (dFTKirbySpecialNStatusList[nFTKindNess] == NULL);

    row = &dFTKirbySpecialStatusDescs[nFTKirbyStatusCopyYoshiSpecialNCatch - nFTCommonStatusSpecialStart];
    rows = (row->proc_update == ftKirbyCopyYoshiSpecialNCatchProcUpdate) &&
           (row->proc_map == ftKirbyCopyYoshiSpecialNCatchProcMap);
    row = &dFTKirbySpecialStatusDescs[nFTKirbyStatusCopyDonkeySpecialAirNFull - nFTCommonStatusSpecialStart];
    rows = rows && (row->sflags.ga == nMPKineticsAir) &&
           (row->proc_map == ftKirbyCopyDonkeySpecialAirNEndProcMap);

    model = dc_model_of(kirby_gobj);
    anims = 0;
    for (i = 0; i < 7; i++)
    {
        if ((kMotions[i] < (s32)model->motion_count) &&
            (model->motions[kMotions[i]].anim >= 0))
        {
            anims |= (1 << i);
        }
    }

    /* both demux levels, from a real B-press, on a swallowed Donkey */
    ftMainSetStatus(kirby_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsGround;
    fp->attr->is_have_specialn = TRUE;
    fp->passive_vars.kirby.copy_id = nFTKindDonkey;
    fp->passive_vars.kirby.copydonkey_charge_level = 0;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = 0;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    demux = (ftCommonSpecialNCheckInterruptCommon(kirby_gobj) != FALSE)
            ? (int)fp->status_id : -1;
    fp->input.pl.button_tap = 0;

    /* one setter, four statuses: a swallowed Luigi gets Luigi's row */
    ftMainSetStatus(kirby_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->passive_vars.kirby.copy_id = nFTKindLuigi;
    ftKirbyCopyMarioSpecialNSetStatus(kirby_gobj);
    mario = (int)fp->status_id;

    /* Samus's start plays slower the fuller the bank: 0.84 at max */
    ftMainSetStatus(kirby_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->passive_vars.kirby.copysamus_charge_level = FTKIRBY_COPYSAMUS_CHARGE_MAX;
    ftKirbyCopySamusSpecialNStartSetStatus(kirby_gobj);
    speed = (int)(DObjGetStruct(kirby_gobj)->anim_speed * 100.0F + 0.5F);
    ftKirbyCopySamusSpecialNDestroyChargeShot(fp);
    fp->passive_vars.kirby.copysamus_charge_level = 0;

    /* and Purin, whose rows were past the end of the array until now */
    ftMainSetStatus(kirby_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->passive_vars.kirby.copy_id = nFTKindPurin;
    ftKirbySpecialNSetStatusSelect(kirby_gobj);
    purin = (int)fp->status_id;

    dbglog(DBG_INFO,
           "db: kirby-copy -- slots %d rows %d anims 0x%X demux %d mario %d "
           "speed %d purin %d want 1 1 0x7F %d %d 84 %d\n",
           slots, rows, anims, demux, mario, speed, purin,
           (int)nFTKirbyStatusCopyDonkeySpecialNStart,
           (int)nFTKirbyStatusCopyLuigiSpecialN,
           (int)nFTKirbyStatusCopyPurinSpecialN);

    /* ---- KIRBY'S FORWARD THROW ----
     *
     * ftCommonThrowSetStatus branches on his fkind and sends him to
     * nFTKirbyStatusThrowF rather than the common one, because his
     * forward throw is a leap and a slam. That branch sets
     * a row that must not be empty.
     *
     * `throwf` is the three rows being filled, and `fall`/`land` are the
     * two setters read back -- the fall is what ends the leap and the
     * landing is what ends the slam. */
    {
        FTStatusDesc *r;
        int throwf = 0, fall, land, i;

        for (i = nFTKirbyStatusThrowF; i <= nFTKirbyStatusThrowFLanding; i++)
        {
            r = &dFTKirbySpecialStatusDescs[i - nFTCommonStatusSpecialStart];

            if ((r->mflags.motion_id != 0) && (r->proc_map != NULL) &&
                (r->sflags.attack_id == nFTStatusAttackIDThrowF))
            {
                throwf++;
            }
        }
        ftMainSetStatus(kirby_gobj, nFTCommonStatusWait, 0.0F, 1.0F,
                        FTSTATUS_PRESERVE_NONE);
        fp->catch_gobj = kirby_gobj;
        fp->is_ignore_dead = TRUE;
        ftKirbyThrowFFallSetStatus(kirby_gobj);
        fall = (fp->is_ignore_dead == FALSE) ? (int)fp->status_id : -1;

        fp->ga = nMPKineticsAir;
        ftKirbyThrowFLandingSetStatus(kirby_gobj);
        land = (fp->ga == nMPKineticsGround) ? (int)fp->status_id : -1;

        dbglog(DBG_INFO,
               "db: kirby-throwf -- rows %d fall %d land %d want 3 %d %d\n",
               throwf, fall, land,
               (int)nFTKirbyStatusThrowFFall,
               (int)nFTKirbyStatusThrowFLanding);

        /* ---- THE RAPID JAB ----
         *
         * Five fighters have one and Kirby is the only one that draws
         * anything: ftCommonAttack100LoopKirbyUpdateEffect throws a
         * spark per jab frame. This is the one of the five that is in
         * the battle, so it is the one the probe can drive.
         *
         * `jab100` is the status a fourth A-tap reaches from the last
         * jab of the chain with the animation event raised, and
         * `spark` is the effect the loop's own update makes -- a
         * two-joint pack out of relocData 348, which nothing else in
         * this port draws. */
        {
            extern GObj* efManagerKirbyVulcanJabMakeEffect(Vec3f *pos, s32 lr,
                                                           f32 rotate, f32 vel,
                                                           f32 add);
            Vec3f jpos;
            GObj *sp;
            int jab100, spark = -1;

            ftMainSetStatus(kirby_gobj, nFTCommonStatusAttack12, 0.0F, 1.0F,
                            FTSTATUS_PRESERVE_NONE);
            fp->attack1_input_count = 3;
            fp->motion_vars.flags.flag1 = 1;
            fp->input.pl.button_tap = fp->input.button_mask_a;
            jab100 = (ftCommonAttack100StartCheckInterruptCommon(kirby_gobj)
                      != FALSE) ? (int)fp->status_id : -1;
            fp->input.pl.button_tap = 0;
            fp->attack1_input_count = 0;
            fp->motion_vars.flags.flag1 = 0;

            jpos = DObjGetStruct(kirby_gobj)->translate.vec.f;
            sp = efManagerKirbyVulcanJabMakeEffect(&jpos, +1, 0.5F, 30.0F,
                                                   -2.0F);

            if (sp != NULL)
            {
                const Fighter *sm = dc_model_of(sp);

                spark = (sm != NULL && sm->hd != NULL)
                        ? (int)sm->hd->joint_count : -1;
                gcEjectGObj(sp);
            }
            dbglog(DBG_INFO,
                   "db: kirby-jab100 -- status %d spark %d want %d 2\n",
                   jab100, spark, (int)nFTKirbyStatusAttack100Start);
        }

        fp->catch_gobj = NULL;
        fp->is_ignore_dead = FALSE;
    }

    ftMainSetStatus(kirby_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->passive_vars.kirby.copy_id = nFTKindKirby;
    fp->proc_damage = NULL;
    fp->proc_accessory = NULL;
    fp->motion_vars.flags.flag0 = fp->motion_vars.flags.flag1 =
        fp->motion_vars.flags.flag2 = 0;
}
#endif /* DB_KIRBY_COPY_PROBE */

/* ---- -DDB_LINK_BOOMERANG_PROBE: Link's Neutral-B on the SH-4 ----------
 *
 * Compiles ft/ftchar/ftlink/ftlink.c, ftlinkspecialn.c,
 * wp/wplink/wplinkboomerang.c and ft/ftchar/ftkirby/ftkirbycopylink-
 * specialn.c unmodified, and built dFTLinkSpecialStatusDescs -- Link's
 * FIRST special table. Link is not in kBootKinds, so this needs
 * -DDB_BOOT_LINK_P2 (P1 stays Fox) and, for the `kirby` field,
 * -DDB_BOOT_PLAYERS=4.
 *
 * `jab` is the one that is not a new feature: ftCommonAttack13SetStatus
 * has named nFTLinkStatusAttack13 since the jab was ported, into a
 * fighter with no table, where ftMainGetStatusDesc returns NULL and
 * ftMainSetStatus aborts. This runs that path on the live Link and says
 * which status came back.
 *
 * `demux` proves the slot from the input side -- a real B-tap, through
 * ftCommonSpecialNCheckInterruptCommon, on the genuine boot fighter.
 * `anims` says Link's real pack has all seven motions the table names,
 * which the host mock -- every motion anim 0 -- can never fail. `wp` is
 * the unmodeled-weapon statement: wpManagerMakeWeapon has no
 * sWPManagerModels row for dWPLinkBoomerangWeaponDesc, so the throw's
 * own animation beat spawns nothing and the boomerang gobj stays NULL,
 * which the decomp's own code reads as an empty hand. If a later step
 * bakes the boomerang into a pack, this is the line that changes, and it
 * should change to 1. `kirby` is the copy: P3 swallowing Link reaches
 * status 287 through both demux levels. */
#ifdef DB_LINK_BOOMERANG_PROBE
static void db_link_boomerang_probe(int frame)
{
    extern FTStatusDesc dFTLinkSpecialStatusDescs[];
    extern FTStatusDesc dFTKirbySpecialStatusDescs[];
    extern sb32 ftCommonSpecialNCheckInterruptCommon(GObj *fighter_gobj);
    extern sb32 ftCommonAttack100StartCheckInterruptCommon(GObj *fighter_gobj);
    extern void (*dFTCommonSpecialNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirNStatusList[])(GObj*);
    extern void (*dFTKirbySpecialNStatusList[])(GObj*);
    static const s32 kMotions[7] = {
        nFTLinkMotionAttack13,
        nFTLinkMotionSpecialN, nFTLinkMotionSpecialNGet,
        nFTLinkMotionSpecialNEmpty, nFTLinkMotionSpecialAirN,
        nFTLinkMotionSpecialAirNReturn, nFTLinkMotionSpecialAirNEmpty
    };
    GObj *link_gobj = NULL;
    GObj *kirby_gobj = NULL;
    FTStruct *fp = NULL;
    const Fighter *model;
    FTStatusDesc *row;
    int rows, anims, demux, jab, slots, wp, kirby, player, i;
    int spinup, spinair, spinjt;

    if (frame != 90)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g == NULL)
        {
            continue;
        }
        if (ftGetStruct(g)->fkind == nFTKindLink)
        {
            link_gobj = g;
            fp = ftGetStruct(g);
        }
        if (ftGetStruct(g)->fkind == nFTKindKirby)
        {
            kirby_gobj = g;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: link-boomerang -- no Link player in the "
                         "battle (build with -DDB_BOOT_LINK_P2)\n");
        return;
    }

    slots = (dFTCommonSpecialNStatusList[nFTKindLink] == ftLinkSpecialNSetStatus) &&
            (dFTCommonSpecialNStatusList[nFTKindNLink] == ftLinkSpecialNSetStatus) &&
            (dFTCommonSpecialAirNStatusList[nFTKindLink] == ftLinkSpecialAirNSetStatus) &&
            (dFTCommonSpecialAirNStatusList[nFTKindNLink] == ftLinkSpecialAirNSetStatus) &&
            (dFTKirbySpecialNStatusList[nFTKindLink] == ftKirbyCopyLinkSpecialNSetStatus);

    /* the two holes stay holes, and the six live rows are live */
    row = &dFTLinkSpecialStatusDescs[nFTLinkStatusSpecialN - nFTCommonStatusSpecialStart];
    rows = (row->proc_update == ftLinkSpecialNProcUpdate) &&
           (row->proc_map == ftLinkSpecialNProcMap) &&
           (row->sflags.is_projectile == TRUE);
    row = &dFTLinkSpecialStatusDescs[nFTLinkStatusSpecialAirNReturn - nFTCommonStatusSpecialStart];
    rows = rows && (row->sflags.ga == nMPKineticsAir) &&
           (row->proc_map == mpCommonProcFighterWaitOrLanding);
    /* The Spin Attack's row is live */
    row = &dFTLinkSpecialStatusDescs[nFTLinkStatusSpecialHi - nFTCommonStatusSpecialStart];
    rows = rows && (row->proc_map == ftLinkSpecialHiProcMap) &&
           (row->proc_update == ftLinkSpecialHiProcUpdate);
    row = &dFTKirbySpecialStatusDescs[nFTKirbyStatusCopyLinkSpecialN - nFTCommonStatusSpecialStart];
    rows = rows && (row->proc_map == ftKirbyCopyLinkSpecialNProcMap);

    model = dc_model_of(link_gobj);
    anims = 0;
    for (i = 0; i < 7; i++)
    {
        if ((kMotions[i] < (s32)model->motion_count) &&
            (model->motions[kMotions[i]].anim >= 0))
        {
            anims |= (1 << i);
        }
    }

    /* the jab that used to abort */
    ftMainSetStatus(link_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftCommonAttack13SetStatus(link_gobj);
    jab = (int)fp->status_id;

    /* the throw, from a real B-tap */
    ftMainSetStatus(link_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsGround;
    fp->attr->is_have_specialn = TRUE;
    fp->passive_vars.link.boomerang_gobj = NULL;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = 0;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    demux = (ftCommonSpecialNCheckInterruptCommon(link_gobj) != FALSE)
            ? (int)fp->status_id : -1;
    fp->input.pl.button_tap = 0;

    /* the animation beat that spawns it: wplinkboomerang.mdl is baked and
     * wpManagerAllocWeapons is called, so a maker comes back 1. */
    fp->motion_vars.flags.flag0 = 1;
    ftLinkSpecialNMakeBoomerang(link_gobj);
    wp = (fp->passive_vars.link.boomerang_gobj != NULL);
    ftLinkSpecialNDestroyBoomerang(link_gobj);

    /* and Kirby's copy of it, through both demux levels */
    kirby = -1;

    if (kirby_gobj != NULL)
    {
        FTStruct *kp = ftGetStruct(kirby_gobj);

        ftMainSetStatus(kirby_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
        kp->passive_vars.kirby.copylink_boomerang_gobj = NULL;
        kp->passive_vars.kirby.copy_id = nFTKindLink;
        ftKirbySpecialNSetStatusSelect(kirby_gobj);
        kirby = (int)kp->status_id;

        ftMainSetStatus(kirby_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
        kp->passive_vars.kirby.copy_id = nFTKindKirby;
        kp->motion_vars.flags.flag0 = kp->motion_vars.flags.flag1 =
            kp->motion_vars.flags.flag2 = 0;
        kp->proc_damage = NULL;
        kp->proc_accessory = NULL;
    }

    /* ---- the Spin Attack ----
     *
     * Both demux levels from the input side -- a B-tap with the stick up
     * on the ground and in the air -- and then the animation beat that
     * spawns the weapon and the glow, which is one call:
     * ftLinkSpecialHiMakeWeapon makes the effect and then the weapon.
     *
     * `spinjt` is the two packs' joint counts, one nibble each: the
     * weapon's tree is two and so is the glow's, and a zero in either
     * nibble means the pack did not load. */
    ftMainSetStatus(link_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsGround;
    fp->attr->is_have_specialhi = TRUE;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = FTCOMMON_SPECIALHI_STICK_RANGE_MIN;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    spinup = (ftCommonSpecialHiCheckInterruptCommon(link_gobj) != FALSE)
             ? (int)fp->status_id : -1;

    /* the air one is a different demux, not the same one with ga
     * flipped: ftCommonSpecialHiCheckInterruptCommon always calls the
     * GROUND list, and the aerial cascade
     * (ft/ftcommon/ftcommonspecialair.c:152)
     * is what reaches dFTCommonSpecialAirHiStatusList. */
    ftMainSetStatus(link_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsAir;
    fp->attr->is_have_specialairhi = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    spinair = (ftCommonSpecialAirCheckInterruptCommon(link_gobj) != FALSE)
              ? (int)fp->status_id : -1;
    fp->input.pl.button_tap = 0;

    spinjt = 0;
    fp->status_vars.link.specialhi.spin_attack_gobj = NULL;
    fp->is_effect_attach = FALSE;
    fp->motion_vars.flags.flag0 = 1;
    ftLinkSpecialHiMakeWeapon(link_gobj, FALSE);

    if (fp->status_vars.link.specialhi.spin_attack_gobj != NULL)
    {
        const Fighter *wm =
            dc_model_of(fp->status_vars.link.specialhi.spin_attack_gobj);

        spinjt |= ((wm != NULL && wm->hd != NULL)
                   ? (int)wm->hd->joint_count : 0);
        wpMainDestroyWeapon(fp->status_vars.link.specialhi.spin_attack_gobj);
        fp->status_vars.link.specialhi.spin_attack_gobj = NULL;
    }
    if (fp->is_effect_attach != FALSE)
    {
        /* the glow is the effect link's tail, as the Cutter's are */
        GObj *g, *last = NULL;

        for (g = gGCCommonLinks[nGCCommonLinkIDEffect]; g != NULL;
             g = g->link_next)
        {
            last = g;
        }
        if (last != NULL)
        {
            const Fighter *em = dc_model_of(last);

            spinjt |= ((em != NULL && em->hd != NULL)
                       ? (int)em->hd->joint_count : 0) << 4;
            gcEjectGObj(last);
        }
        fp->is_effect_attach = FALSE;
    }
    fp->motion_vars.flags.flag0 = 0;
    fp->proc_lagstart = NULL;
    fp->proc_lagend = NULL;

    dbglog(DBG_INFO,
           "db: link-boomerang -- slots %d rows %d anims 0x%X jab %d "
           "demux %d wp %d kirby %d spin %d/%d joints 0x%X "
           "want 1 1 0x7F %d %d 1 %d(-1 with no Kirby in the battle) "
           "%d %d 0x22\n",
           slots, rows, anims, jab, demux, wp, kirby, spinup, spinair,
           spinjt,
           (int)nFTLinkStatusAttack13, (int)nFTLinkStatusSpecialN,
           (int)nFTKirbyStatusCopyLinkSpecialN,
           (int)nFTLinkStatusSpecialHi, (int)nFTLinkStatusSpecialAirHi);

    ftMainSetStatus(link_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->motion_vars.flags.flag0 = fp->motion_vars.flags.flag1 =
        fp->motion_vars.flags.flag2 = 0;
    fp->proc_damage = NULL;
    fp->proc_accessory = NULL;
}
#endif /* DB_LINK_BOOMERANG_PROBE */

/* ---- -DDB_LUIGI_TABLE_PROBE: Luigi's specials on the SH-4 -------------
 *
 * Builds dFTLuigiSpecialStatusDescs -- nine rows, no new file,
 * every proc Mario's. Luigi is not in kBootKinds, so this needs
 * -DDB_BOOT_LUIGI_P2 (P1 stays Fox).
 *
 * `jab`, `demux` and
 * `tornado` are the three ways into it that end in
 * scManagerRunPrintGObjStatus's abort() when unfilled: the third jab through
 * ftCommonAttack13SetStatus, a B-tap through the Neutral-B demux, and the
 * Down-B through the Down-B slot. `anims` is the target-only
 * one -- all nine motion ids the table names are really in LUIGI's pack,
 * which is the whole reason his table is not Mario's even though its rows
 * are identical (`same 1` says they are, byte for byte). `land` is the
 * one status neither setter sets: 227 is reached only when a spinning
 * Luigi touches back down. */
#ifdef DB_LUIGI_TABLE_PROBE
static void db_luigi_table_probe(int frame)
{
    extern FTStatusDesc dFTLuigiSpecialStatusDescs[];
    extern FTStatusDesc dFTMarioSpecialStatusDescs[];
    extern sb32 ftCommonSpecialNCheckInterruptCommon(GObj *fighter_gobj);
    extern sb32 ftCommonSpecialLwCheckInterruptCommon(GObj *fighter_gobj);
    extern void (*dFTCommonSpecialNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirLwStatusList[])(GObj*);
    static const s32 kMotions[9] = {
        nFTLuigiMotionAttack13, nFTLuigiMotionAppearR, nFTLuigiMotionAppearL,
        nFTLuigiMotionSpecialN, nFTLuigiMotionSpecialAirN,
        nFTLuigiMotionSpecialHi, nFTLuigiMotionSpecialAirHi,
        nFTLuigiMotionSpecialLw, nFTLuigiMotionSpecialAirLw
    };
    GObj *luigi_gobj = NULL;
    FTStruct *fp = NULL;
    const Fighter *model;
    int rows, anims, demux, jab, slots, same, tornado, land, player, i;

    if (frame != 90)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindLuigi)
        {
            luigi_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: luigi-table -- no Luigi player in the battle "
                         "(build with -DDB_BOOT_LUIGI_P2)\n");
        return;
    }

    /* all six */
    slots = (dFTCommonSpecialNStatusList[nFTKindLuigi] == ftMarioSpecialNSetStatus) &&
            (dFTCommonSpecialHiStatusList[nFTKindLuigi] == ftMarioSpecialHiSetStatus) &&
            (dFTCommonSpecialAirNStatusList[nFTKindLuigi] == ftMarioSpecialAirNSetStatus) &&
            (dFTCommonSpecialAirHiStatusList[nFTKindLuigi] == ftMarioSpecialAirHiSetStatus) &&
            (dFTCommonSpecialAirLwStatusList[nFTKindLuigi] == ftMarioSpecialAirLwSetStatus) &&
            (dFTCommonSpecialNStatusList[nFTKindNLuigi] == ftMarioSpecialNSetStatus);

    rows = 1;

    for (i = 0; i < 9; i++)
    {
        FTStatusDesc *row = &dFTLuigiSpecialStatusDescs[i];

        rows = rows && (row->proc_physics != NULL) && (row->proc_map != NULL) &&
               (row->mflags.motion_id == kMotions[i]);
    }
    same = (memcmp(dFTLuigiSpecialStatusDescs, dFTMarioSpecialStatusDescs,
                   9 * sizeof(FTStatusDesc)) == 0);

    model = dc_model_of(luigi_gobj);
    anims = 0;
    for (i = 0; i < 9; i++)
    {
        if ((kMotions[i] < (s32)model->motion_count) &&
            (model->motions[kMotions[i]].anim >= 0))
        {
            anims |= (1 << i);
        }
    }

    /* the third jab -- the same shape as Link's */
    ftMainSetStatus(luigi_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftCommonAttack13SetStatus(luigi_gobj);
    jab = (int)fp->status_id;

    /* a real B-tap into the fireball */
    ftMainSetStatus(luigi_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsGround;
    fp->attr->is_have_specialn = TRUE;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = 0;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    demux = (ftCommonSpecialNCheckInterruptCommon(luigi_gobj) != FALSE)
            ? (int)fp->status_id : -1;

    /* and the Tornado, into the AIR row even from the ground */
    ftMainSetStatus(luigi_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->ga = nMPKineticsGround;
    fp->attr->is_have_speciallw = TRUE;
    fp->input.pl.stick_range.y = FTCOMMON_SPECIALLW_STICK_RANGE_MIN;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    tornado = (ftCommonSpecialLwCheckInterruptCommon(luigi_gobj) != FALSE)
              ? (int)fp->status_id : -1;
    ftMarioSpecialAirLwSwitchStatusGround(luigi_gobj);
    land = (int)fp->status_id;
    fp->input.pl.button_tap = 0;
    fp->input.pl.stick_range.y = 0;

    dbglog(DBG_INFO,
           "db: luigi-table -- slots %d rows %d same %d anims 0x%X jab %d "
           "demux %d tornado %d land %d want 1 1 1 0x1FF %d %d %d %d\n",
           slots, rows, same, anims, jab, demux, tornado, land,
           (int)nFTLuigiStatusAttack13, (int)nFTLuigiStatusSpecialN,
           (int)nFTLuigiStatusSpecialAirLw, (int)nFTLuigiStatusSpecialLw);

    ftMainSetStatus(luigi_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    mpCommonSetFighterGround(fp);
    fp->motion_vars.flags.flag0 = fp->motion_vars.flags.flag1 =
        fp->motion_vars.flags.flag2 = 0;
    fp->proc_damage = NULL;
    fp->proc_accessory = NULL;
}
#endif /* DB_LUIGI_TABLE_PROBE */

/* ---- -DDB_NESS_TABLE_PROBE: Ness's table on the SH-4 ------------------
 *
 * Builds dFTNessSpecialStatusDescs -- six rows, and the last
 * live abort of the kind steps 64 and 65 found. Ness is not in
 * kBootKinds, so this needs -DDB_BOOT_NESS_P2 (P1 stays Fox).
 *
 * `jab` is the abort: ftCommonAttack13SetStatus's Ness arm has named
 * status 220 since the jab was ported, into a fighter with no table.
 * `wait` and `end` walk his entrance, which is the roster's longest --
 * five statuses, because he arrives inside the PSI ring -- through the
 * two update procs this step hand-ported out of ftcommonentry.c.
 * `anims` is the target-only one: all six motion ids are really in
 * NESS's pack, which the host mock (every motion anim 0) can never fail.
 * `inert` says the second fact out loud: the five
 * entrance rows are unreachable unless
 * ftCommonAppearSetStatus reads dFTCommonEntryAppearStatusIDs and that
 * table had a row only for Mario. The row is filled, so the field is
 * 0 and Ness really warps in -- see -DDB_ENTRANCE_PROBE below. */
#ifdef DB_NESS_TABLE_PROBE
static void db_ness_table_probe(int frame)
{
    extern FTStatusDesc dFTNessSpecialStatusDescs[];
    extern sb32 ftCommonEntryHasAppear(s32 fkind);
    extern void (*dFTCommonSpecialNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirLwStatusList[])(GObj*);
    static const s32 kMotions[6] = {
        nFTNessMotionAttack13,
        nFTNessMotionAppearRStart, nFTNessMotionAppearLStart,
        nFTNessMotionAppearWait,
        nFTNessMotionAppearREnd, nFTNessMotionAppearLEnd
    };
    GObj *ness_gobj = NULL;
    FTStruct *fp = NULL;
    const Fighter *model;
    int rows, anims, jab, slots, wait, end, inert, player, i;

    if (frame != 90)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindNess)
        {
            ness_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: ness-table -- no Ness player in the battle "
                         "(build with -DDB_BOOT_NESS_P2)\n");
        return;
    }

    /* his specials are holes, and every slot says so */
    slots = (dFTCommonSpecialNStatusList[nFTKindNess] == NULL) &&
            (dFTCommonSpecialHiStatusList[nFTKindNess] == NULL) &&
            (dFTCommonSpecialAirNStatusList[nFTKindNess] == NULL) &&
            (dFTCommonSpecialAirHiStatusList[nFTKindNess] == NULL) &&
            (dFTCommonSpecialAirLwStatusList[nFTKindNess] == NULL);

    rows = 1;

    for (i = 0; i < 6; i++)
    {
        FTStatusDesc *row = &dFTNessSpecialStatusDescs[i];

        rows = rows && (row->proc_update != NULL) && (row->proc_physics != NULL) &&
               (row->proc_map != NULL) && (row->mflags.motion_id == kMotions[i]);
    }
    rows = rows &&
           (dFTNessSpecialStatusDescs[nFTNessStatusAppearWait - nFTCommonStatusSpecialStart].proc_update
            == ftNessAppearWaitProcUpdate);

    model = dc_model_of(ness_gobj);
    anims = 0;
    for (i = 0; i < 6; i++)
    {
        if ((kMotions[i] < (s32)model->motion_count) &&
            (model->motions[kMotions[i]].anim >= 0))
        {
            anims |= (1 << i);
        }
    }

    /* the third jab -- the abort */
    ftMainSetStatus(ness_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftCommonAttack13SetStatus(ness_gobj);
    jab = (int)fp->status_id;

    /* the entrance walked on the live fighter: Start -> Wait -> End */
    ftMainSetStatus(ness_gobj, nFTNessStatusAppearRStart, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->status_vars.common.entry.lr = +1;
    ness_gobj->anim_frame = 0.0F;
    ftNessAppearStartProcUpdate(ness_gobj);
    wait = (int)fp->status_id;
    fp->status_vars.common.entry.lr = -1;
    ness_gobj->anim_frame = 0.0F;
    ftNessAppearWaitProcUpdate(ness_gobj);
    end = (int)fp->status_id;

    inert = (ftCommonEntryHasAppear(nFTKindNess) == FALSE);

    dbglog(DBG_INFO,
           "db: ness-table -- slots %d rows %d anims 0x%X jab %d wait %d "
           "end %d inert %d want 1 1 0x3F %d %d %d 0\n",
           slots, rows, anims, jab, wait, end, inert,
           (int)nFTNessStatusAttack13, (int)nFTNessStatusAppearWait,
           (int)nFTNessStatusAppearLEnd);

    ftMainSetStatus(ness_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->is_shadow_hide = FALSE;
    fp->motion_vars.flags.flag0 = fp->motion_vars.flags.flag1 =
        fp->motion_vars.flags.flag2 = 0;
    fp->proc_damage = NULL;
    fp->proc_accessory = NULL;
}
#endif /* DB_NESS_TABLE_PROBE */

/* ---- -DDB_JOLT_PROBE: Pikachu's Thunder Jolt on the SH-4 -------------
 *
 * Builds dFTPikachuSpecialStatusDescs -- four rows, and the
 * last fighter in the game to get a table of his own. Pikachu is not in
 * kBootKinds, so this needs -DDB_BOOT_PIKACHU_P2 (P1 stays Fox).
 *
 * `rows` is the four rows' motions and procs; `anims` is the target-only
 * one, that all four motion ids are really in PIKACHU's pack, which the
 * host mock (every motion anim 0) can never fail. `slots` is the two
 * Neutral-B demux slots live and the four other specials still NULL --
 * that pair of facts is what keeps rows 224-237 unreachable rather than
 * silently wrong.
 *
 * `warp` and `wait` walk the entrance on the live fighter: his row in dFTCommonEntryAppearStatusIDs needs these four
 * rows, so this is the probe that closes the roster's warp-in. `sn` and `airn` are the Neutral-B, ground and air, through
 * the setters the demux names; `flag` is the accessory proc's own gate,
 * the animation event that asks for the weapon -- it comes back 0
 * because the proc clears it whether or not a jolt was made, and no jolt
 * is made on this disc (neither of the two WPDescs has an
 * sWPManagerModels row, so wpManagerMakeWeapon returns NULL, the
 * decomp's own no-projectile path). */
#ifdef DB_JOLT_PROBE
static void db_jolt_probe(int frame)
{
    extern FTStatusDesc dFTPikachuSpecialStatusDescs[];
    extern void (*dFTCommonSpecialNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirNStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirLwStatusList[])(GObj*);
    static const s32 kMotions[4] = {
        nFTPikachuMotionAppearR, nFTPikachuMotionAppearL,
        nFTPikachuMotionSpecialN, nFTPikachuMotionSpecialAirN
    };
    GObj *pika_gobj = NULL;
    FTStruct *fp = NULL;
    const Fighter *model;
    int rows, anims, slots, warp, wait, sn, airn, flag, player, i;

    if (frame != 90)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindPikachu)
        {
            pika_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: jolt -- no Pikachu player in the battle "
                         "(build with -DDB_BOOT_PIKACHU_P2)\n");
        return;
    }

    /* the Neutral-B is live both ways; his other four specials are holes,
     * and every slot says so */
    slots = (dFTCommonSpecialNStatusList[nFTKindPikachu] == ftPikachuSpecialNSetStatus) &&
            (dFTCommonSpecialAirNStatusList[nFTKindPikachu] == ftPikachuSpecialAirNSetStatus) &&
            (dFTCommonSpecialHiStatusList[nFTKindPikachu] == NULL) &&
            (dFTCommonSpecialAirHiStatusList[nFTKindPikachu] == NULL) &&
            (dFTCommonSpecialAirLwStatusList[nFTKindPikachu] == NULL);

    rows = 1;
    for (i = 0; i < 4; i++)
    {
        FTStatusDesc *row = &dFTPikachuSpecialStatusDescs[i];

        rows = rows && (row->proc_update != NULL) && (row->proc_physics != NULL) &&
               (row->proc_map != NULL) && (row->mflags.motion_id == kMotions[i]);
    }
    rows = rows &&
           (dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialN - nFTCommonStatusSpecialStart].proc_map
            == ftPikachuSpecialNProcMap);

    model = dc_model_of(pika_gobj);
    anims = 0;
    for (i = 0; i < 4; i++)
    {
        if ((kMotions[i] < (s32)model->motion_count) &&
            (model->motions[kMotions[i]].anim >= 0))
        {
            anims |= (1 << i);
        }
    }

    /* the entrance, on the live fighter */
    fp->lr = +1;
    ftMainSetStatus(pika_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftCommonAppearSetStatus(pika_gobj);
    warp = (int)fp->status_id;
    pika_gobj->anim_frame = 0.0F;
    ftCommonAppearProcUpdate(pika_gobj);
    wait = (int)fp->status_id;

    /* the Neutral-B, ground then air, and the accessory proc's gate */
    mpCommonSetFighterGround(fp);
    ftMainSetStatus(pika_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftPikachuSpecialNSetStatus(pika_gobj);
    sn = (int)fp->status_id;

    fp->motion_vars.flags.flag0 = 1;
    ftPikachuSpecialNProcAccessory(pika_gobj);
    flag = (int)fp->motion_vars.flags.flag0;

    pika_gobj->anim_frame = 4.0F;
    ftPikachuSpecialNSwitchStatusAir(pika_gobj);
    airn = (int)fp->status_id;

    dbglog(DBG_INFO,
           "db: jolt -- slots %d rows %d anims 0x%X warp %d wait %d sn %d "
           "airn %d flag %d want 1 1 0xF %d %d %d %d 0\n",
           slots, rows, anims, warp, wait, sn, airn, flag,
           (int)nFTPikachuStatusAppearR, (int)nFTCommonStatusWait,
           (int)nFTPikachuStatusSpecialN, (int)nFTPikachuStatusSpecialAirN);

    mpCommonSetFighterGround(fp);
    ftMainSetStatus(pika_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->is_ghost = FALSE;
    fp->is_shadow_hide = FALSE;
    fp->is_playertag_hide = FALSE;
    fp->motion_vars.flags.flag0 = fp->motion_vars.flags.flag1 =
        fp->motion_vars.flags.flag2 = 0;
    fp->proc_damage = NULL;
    fp->proc_accessory = NULL;
}
#endif /* DB_JOLT_PROBE */

/* ---- -DDB_DOKAN_PROBE: the entry vehicles on the SH-4 ---------------
 *
 * Bakes the first of the ten entry vehicles and put the arm
 * of ftCommonAppearSetStatus that makes it back. This watches a real
 * countdown: it does not drive anything, it samples the effect link and
 * reports what the entrance made.
 *
 * `made` is the pipe's GObj being there, `joints` and `tris` are read
 * off the pack it is drawing, and `lit` says its batches came out shaded
 * 1 -- the pipe is a solid object and wants the scene's lights, where
 * every weapon model in this port is an unlit billboard. */
#ifdef DB_DOKAN_PROBE
static void db_dokan_probe(int frame)
{
    extern GObj* efManagerMarioEntryDokanMakeEffect(Vec3f *pos, s32 fkind);
    extern GObj* efManagerDonkeyEntryTaruMakeEffect(Vec3f *pos);
    extern GObj* efManagerSamusEntryPointMakeEffect(Vec3f *pos);
    extern GObj* efManagerKirbyEntryStarMakeEffect(Vec3f *pos, s32 lr);
    extern GObj* efManagerLinkEntryWaveMakeEffect(Vec3f *pos);
    extern GObj* efManagerLinkEntryBeamMakeEffect(Vec3f *pos);
    extern GObj* efManagerYoshiEntryEggMakeEffect(Vec3f *pos);
    extern GObj* efManagerFoxEntryArwingMakeEffect(Vec3f *pos, s32 lr);
    extern GObj* efManagerCaptainEntryCarMakeEffect(Vec3f *pos, s32 lr);
    GObj *mario = NULL;
    GObj *pipe, *taru, *point, *starR, *starL, *wave, *beam, *egg;
    GObj *arwR, *arwL, *carR, *carL;
    Vec3f pos;
    const Fighter *m;
    int made, joints, tris, lit, tjt, pjt, plit, sanim, sdiff;
    int wmo, bmo, ejt, etx, ajt, abb, cjt, cbb, crot, player, i;

    /* One sample DURING the countdown, before anything is
     * made by hand, so the arm of ftCommonAppearSetStatus is what put
     * these there. With -DDB_BOOT_P2_KIND=nFTKindYoshi one of them is the
     * egg, and a one-joint effect on this link is nothing else in the
     * port: the six vehicles before it have two joints or three, because
     * their o_dobjsetup is a DObjDesc TREE and the egg's is a bare
     * display list. It comes up around frame 140 and is still there at
     * 200. */
    if (frame == 200)
    {
        GObj *g = gGCCommonLinks[nGCCommonLinkIDEffect];
        int n = 0, one_joint = 0, twelve_joint = 0;

        while (g != NULL)
        {
            const Fighter *em = dc_model_of(g);

            n++;

            if (em != NULL && em->hd != NULL)
            {
                one_joint += (em->hd->joint_count == 1);
                twelve_joint += (em->hd->joint_count == 12);
            }
            g = g->link_next;
        }
        dbglog(DBG_INFO,
               "db: entry -- countdown effects %d, one-joint %d, "
               "twelve-joint %d (the baseline seven are efdisplay's CLD "
               "GObjs; one-joint is Yoshi's egg with "
               "-DDB_BOOT_P2_KIND=nFTKindYoshi and twelve-joint is Fox's "
               "Arwing, and Fox is P1; with "
               "-DDB_BOOT_P2_KIND=nFTKindCaptain the Blue Falcon is a "
               "twelve-joint one too)\n",
               n, one_joint, twelve_joint);
        return;
    }
    if (frame != 240)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        /* any fighter will do: a vehicle is placed where it is told and
         * asks nothing about who arrived on it */
        if (g != NULL)
        {
            mario = g;
            break;
        }
    }
    if (mario == NULL)
    {
        dbglog(DBG_INFO, "db: entry -- no fighter in the battle\n");
        return;
    }
    pos = DObjGetStruct(mario)->translate.vec.f;
    pipe = efManagerMarioEntryDokanMakeEffect(&pos, nFTKindMario);

    /* Two, both of whose makers are the pipe's without even
     * its file switch */
    taru = efManagerDonkeyEntryTaruMakeEffect(&pos);
    point = efManagerSamusEntryPointMakeEffect(&pos);
    /* Made twice -- once each way, because the two sweeps
     * are two animations of the one pack and the maker picks by index */
    starR = efManagerKirbyEntryStarMakeEffect(&pos, +1);
    starL = efManagerKirbyEntryStarMakeEffect(&pos, -1);
    /* Two, the one entrance that makes a pair */
    wave = efManagerLinkEntryWaveMakeEffect(&pos);
    beam = efManagerLinkEntryBeamMakeEffect(&pos);
    /* The first vehicle whose o_dobjsetup is a bare
     * display list and the first whose MObj steps a sprite array */
    egg = efManagerYoshiEntryEggMakeEffect(&pos);
    /* Made both ways -- its two AnimJoint arrays are the
     * pack's two animations and the maker picks by index, as the star's
     * does */
    arwR = efManagerFoxEntryArwingMakeEffect(&pos, +1);
    arwL = efManagerFoxEntryArwingMakeEffect(&pos, -1);
    /* Both ways -- the car has one animation and turns
     * around with a rotate on its root, so -1 is a 180 and not a second
     * sweep */
    carR = efManagerCaptainEntryCarMakeEffect(&pos, +1);
    carL = efManagerCaptainEntryCarMakeEffect(&pos, -1);

    made = (pipe != NULL) + (taru != NULL) + (point != NULL) +
           (starR != NULL) + (starL != NULL) +
           (wave != NULL) + (beam != NULL) + (egg != NULL) +
           (arwR != NULL) + (arwL != NULL) +
           (carR != NULL) + (carL != NULL);
    joints = tris = lit = tjt = pjt = plit = sanim = sdiff = -1;
    wmo = bmo = ejt = etx = ajt = abb = -1;
    cjt = cbb = crot = -1;

    if (pipe != NULL)
    {
        m = dc_model_of(pipe);

        if (m != NULL && m->hd != NULL)
        {
            joints = (int)m->hd->joint_count;
            tris = (int)m->hd->tri_count;
            lit = 1;

            for (i = 0; i < (int)m->hd->batch_count; i++)
            {
                if (m->batches[i].shaded != 1)
                {
                    lit = 0;
                }
            }
        }
        gcEjectGObj(pipe);
    }
    if (taru != NULL)
    {
        m = dc_model_of(taru);
        tjt = (m != NULL && m->hd != NULL) ? (int)m->hd->joint_count : -1;
        gcEjectGObj(taru);
    }
    if (point != NULL)
    {
        m = dc_model_of(point);
        pjt = (m != NULL && m->hd != NULL) ? (int)m->hd->joint_count : -1;
        plit = 0;

        if (m != NULL && m->hd != NULL)
        {
            for (i = 0; i < (int)m->hd->batch_count; i++)
            {
                plit += (m->batches[i].shaded == 1);
            }
        }
        gcEjectGObj(point);
    }
    if (starR != NULL && starL != NULL)
    {
        const Fighter *mR = dc_model_of(starR);

        if (mR != NULL && mR->hd != NULL)
        {
            sanim = (int)mR->hd->anim_count;
        }
        /* both sweeps made a star from the one pack, which is the thing
         * the index picker has to get right; that the two BLOCKS differ
         * is the host test's claim (their off_words are not equal) */
        sdiff = 1;
    }
    if (starR != NULL)
    {
        gcEjectGObj(starR);
    }
    if (starL != NULL)
    {
        gcEjectGObj(starL);
    }
    if (wave != NULL)
    {
        /* one MObj apiece is what makes these two different from the
         * four vehicles before them */
        m = dc_model_of(wave);
        wmo = (m != NULL && m->mobjs != NULL) ? (int)m->mobjs->mobj_count : -1;
        gcEjectGObj(wave);
    }
    if (beam != NULL)
    {
        m = dc_model_of(beam);
        bmo = (m != NULL && m->mobjs != NULL) ? (int)m->mobjs->mobj_count : -1;
        gcEjectGObj(beam);
    }
    if (egg != NULL)
    {
        /* one joint says the bare display list was read as a display
         * list, and the MObj's two-frame run says the flipbook came
         * across -- the two things to get right */
        m = dc_model_of(egg);

        if (m != NULL && m->hd != NULL)
        {
            ejt = (int)m->hd->joint_count;
            etx = (m->mobj_subs != NULL) ? (int)m->mobj_subs[0].tex_count : -1;
        }
        gcEjectGObj(egg);
    }
    if (arwR != NULL)
    {
        /* Twelve joints is the tree, and `abb` is the thing no earlier
         * vehicle could be asked: whether the maker's seven-siblings-deep
         * walk found node 10 and hung matrix kind 44 -- the RSP billboard
         * -- on it. The walk gives up quietly on a tree that is not the
         * Arwing's, so a 0 here means it went looking and came back. */
        DObj *d = DObjGetStruct(arwR);
        s32 k;

        m = dc_model_of(arwR);
        ajt = (m != NULL && m->hd != NULL) ? (int)m->hd->joint_count : -1;
        abb = 0;

        if (d != NULL && d->child != NULL && d->child->child != NULL)
        {
            d = d->child->child;

            for (k = 0; k < 6 && d != NULL; k++)
            {
                d = d->sib_next;
            }
            if (d != NULL && d->child != NULL)
            {
                for (k = 0; k < (s32)d->child->xobjs_num; k++)
                {
                    if (d->child->xobjs[k]->kind ==
                        nGCMatrixKindRecalcRotRpyRSca)
                    {
                        abb = 1;
                    }
                }
            }
        }
        gcEjectGObj(arwR);
    }
    if (arwL != NULL)
    {
        gcEjectGObj(arwL);
    }
    if (carR != NULL)
    {
        /* Twelve joints again, and `cbb` counts how many of the four
         * odd siblings under node 2 got matrix kind 44. The Blue
         * Falcon's EFDesc lacks 0x1, so the game builds its tree
         * straight on the GObj and the port's walk is the decomp's
         * unchanged -- which is what this number is really testing. */
        DObj *d = DObjGetStruct(carR);
        s32 k, j;

        m = dc_model_of(carR);
        cjt = (m != NULL && m->hd != NULL) ? (int)m->hd->joint_count : -1;
        cbb = 0;

        if (d != NULL && d->child != NULL && d->child->child != NULL)
        {
            DObj *w = d->child->child->child;

            for (k = 0; (k < 4) && (w != NULL); k++)
            {
                for (j = 0; j < (s32)w->xobjs_num; j++)
                {
                    if (w->xobjs[j]->kind == nGCMatrixKindRecalcRotRpyRSca)
                    {
                        cbb++;
                    }
                }
                w = (w->sib_next != NULL) ? w->sib_next->sib_next : NULL;
            }
        }
        gcEjectGObj(carR);
    }
    if (carL != NULL)
    {
        /* and the turn: lr -1 puts 180 degrees on the root's yaw */
        DObj *d = DObjGetStruct(carL);

        crot = (d != NULL && d->rotate.vec.f.y > 3.0F) ? 1 : 0;
        gcEjectGObj(carL);
    }
    dbglog(DBG_INFO,
           "db: entry -- made %d pipe %d/%d lit %d taru %d point %d lit %d "
           "star %d diff %d wave %d beam %d egg %d/%d arwing %d bb %d "
           "car %d bb %d turn %d "
           "want 12 3 44 1 2 2 2 2 1 1 1 1 2 12 1 12 4 1\n",
           made, joints, tris, lit, tjt, pjt, plit, sanim, sdiff, wmo, bmo,
           ejt, etx, ajt, abb, cjt, cbb, crot);
}
#endif /* DB_DOKAN_PROBE */

/* ---- -DDB_BANK_PROBE: the per-fighter particle banks on the SH-4 ----
 *
 * Loads the two per-fighter particle banks the port has a
 * consumer for -- Kirby's particles_unk0 and Yoshi's particles_unk2 --
 * and un-stubbed the two makers that read them. The load itself prints a
 * `particles:` line of its own from src/dc/ftmanager.c; this is the half
 * that line cannot show.
 *
 * What matters is not that a particle came back but WHICH BANK it came
 * out of. gFTDataKirbyParticleBankID and gFTDataYoshiParticleBankID were
 * bss-zero before this step, and bank 0 is efcommon's -- so a maker
 * called through an unloaded id would have returned a perfectly good
 * LBParticle running the COMMON bank's script 3 or 0xC, which is a
 * different effect drawn silently. `bank` below is LBParticle.bank_id,
 * and it has to be neither 0 nor the other fighter's. */
#ifdef DB_BANK_PROBE
static void db_bank_probe(int frame)
{
    extern LBParticle* efManagerYoshiEggExplodeMakeEffect(Vec3f *pos);
    extern LBParticle* efManagerKirbyInhaleWindMakeEffect(GObj *fighter_gobj);
    extern s32 gFTDataKirbyParticleBankID;
    extern s32 gFTDataYoshiParticleBankID;
    GObj *yoshi = NULL;
    GObj *kirby = NULL;
    LBParticle *pc;
    Vec3f pos;
    int eggbank, windbank, player;

    if (frame != 240)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g == NULL)
        {
            continue;
        }
        if (ftGetStruct(g)->fkind == nFTKindYoshi)
        {
            yoshi = g;
        }
        if (ftGetStruct(g)->fkind == nFTKindKirby)
        {
            kirby = g;
        }
    }
    eggbank = windbank = -1;

    /* Each maker is called only with its own fighter in the battle,
     * because a bank is loaded per kind per scene and asking otherwise
     * is asking for the wrong answer -- which is the point: with Kirby
     * absent his id is still 0, and 0 is efcommon, so the wind would
     * come back drawing the common bank's script 0xC. */
    if (yoshi != NULL)
    {
        pos = DObjGetStruct(yoshi)->translate.vec.f;
        pc = efManagerYoshiEggExplodeMakeEffect(&pos);

        if (pc != NULL)
        {
            eggbank = (int)(pc->bank_id & 7);
            lbParticleEjectStruct(pc);
        }
    }
    if (kirby != NULL)
    {
        pc = efManagerKirbyInhaleWindMakeEffect(kirby);

        if (pc != NULL)
        {
            /* the wind ORs LBPARTICLE_MASK_GENLINK(0), which is
             * ((0 + 1) * 8), so the bank is the low three bits
             * (lb/lbdef.h:82, lbparticle.c:1885) */
            windbank = (int)(pc->bank_id & 7);
            lbParticleEjectStruct(pc);
        }
    }
    /* ---- the common-bank makers ----
     *
     * Thirty of them, and not one has a caller yet: almost every one is
     * reached only through ft/ftparam.c's ftParamMakeEffect, which is
     * still a stub. So the probe calls a spread of them by hand and
     * counts how many came back with a particle -- which is the only
     * thing the target can say about them until the gate opens.
     *
     * The eight are one per shape: a plain script id, one that picks by
     * random from a table, one that takes a scale, one that ORs a
     * genlink, one that is a wrapper on another, and three of the dusts.
     * `common` is how many of the eight gave a particle back. */
    {
        Vec3f cp = { 0.0F, 0.0F, 0.0F };
        LBParticle *made[6];
        LBGenerator *gen[2];
        int common = 0, k;

        /* six that hand back a particle... */
        made[0] = efManagerFlashSmallMakeEffect(&cp);
        made[1] = efManagerFlashLargeMakeEffect(&cp);
        made[2] = efManagerMusicNoteMakeEffect(&cp);
        made[3] = efManagerSparkleWhiteScaleMakeEffect(&cp, 0.7F);
        made[4] = efManagerDustLightMakeEffect(&cp, +1, 0.0F);
        made[5] = efManagerDustHeavyMakeEffect(&cp, +1);
        /* ...and two that hand back a GENERATOR instead, which is the
         * other shape in this group: lbParticleMakeCommon rather than
         * lbParticleMakeScriptID */
        gen[0] = efManagerRippleMakeEffect(&cp);
        gen[1] = efManagerShieldBreakMakeEffect(&cp);

        for (k = 0; k < 6; k++)
        {
            if (made[k] != NULL)
            {
                /* every one of them reads the COMMON bank, which is 0 */
                common += ((made[k]->bank_id & 7) == 0);
                lbParticleEjectStruct(made[k]);
            }
        }
        for (k = 0; k < 2; k++)
        {
            if (gen[k] != NULL)
            {
                common += ((gen[k]->bank_id & 7) == 0);
                lbParticleEjectGenerator(gen[k]);
            }
        }
        dbglog(DBG_INFO, "db: banks -- common-bank makers %d of 8\n",
               common);
    }
    dbglog(DBG_INFO,
           "db: banks -- yoshi id %d in %d egg bank %d, kirby id %d in %d "
           "wind bank %d (a fighter in the battle wants a non-zero id and "
           "a particle out of that same bank; 0 is efcommon and is what "
           "an unloaded id would silently have drawn)\n",
           (int)gFTDataYoshiParticleBankID, yoshi != NULL, eggbank,
           (int)gFTDataKirbyParticleBankID, kirby != NULL, windbank);
}
#endif /* DB_BANK_PROBE */

/* ---- -DDB_GATE_PROBE: ftParamMakeEffect on the SH-4 ------------------
 *
 * Opens the gate: ft/ftparam.c's 313-line switch, which is
 * the only caller most of ef/efmanager.c's makers have. What the host
 * cannot see is any of it -- the host declines a particle script bank,
 * whose pointer arrays are 32-bit -- so everything past the dispatch is
 * this probe's.
 *
 * Three things are checked, and they are the three the gate is made of.
 *
 * `cycle` is the twenty arms that open with ftParamGetEffectJointPosition:
 * five calls with joint_id -1, so the head skips its own joint read and
 * the arm's cycler supplies the position. Each one has to land on the
 * world position of the joint the table names on that step. This is what
 * the same step's pack change is for -- effect_joint_ids was not carried
 * across until now, so all five read joint 0 and every flame came out of
 * TopN. `spread` counts how many of the five landed somewhere other than
 * the first, which is the visible half of that: a fighter on fire is on
 * fire all over, not from one bone.
 *
 * `joint` is the other twenty-odd: a named joint, an offset, a scatter
 * box, and the fighter's own size scaling. Two calls -- one plain on
 * joint 4, one with an offset -- against the same world position read
 * back independently. The offset goes in before the joint transform, so
 * it is a position in the bone's own space, not a displacement of the
 * result (gmcollision.c:196-205). The id has to be one whose arm leaves
 * pos alone: the large dust adds a random +-80 of its own on both axes,
 * which is what this probe asked for first and why it came back 0 of 2.
 *
 * `gobj` is the arms that make a model rather than a particle. The
 * impact wave is the one the port already had a live caller for
 * (ftcommon.c:6846 calls ftParamMakeEffect), so it is the one that proves the gate
 * changes what the game does and not just what it returns. */
#ifdef DB_GATE_PROBE
static void db_gate_probe(int frame)
{
    GObj *g;
    FTStruct *fp;
    Vec3f seen[5], want, off;
    int i, made = 0, placed = 0, spread = 0;
    int jmade = 0, jplaced = 0;
    GObj *wave;
    int waves = 0;

    if (frame != 240)
    {
        return;
    }
    g = db_fighter_gobj(0);

    if (g == NULL)
    {
        dbglog(DBG_INFO, "db: gate -- no player 0\n");
        return;
    }
    fp = ftGetStruct(g);

    /* -- the cycler's twenty arms -- */
    fp->effect_joint_array_id = 0;

    for (i = 0; i < 5; i++)
    {
        s32 want_id = (i + 1) % 5;
        LBParticle *pc = (LBParticle *)
            ftParamMakeEffect(g, nEFKindFlameStatic, -1, NULL, NULL,
                              fp->lr, FALSE, 0);

        if (pc == NULL || pc->xf == NULL)
        {
            continue;
        }
        made++;
        seen[i] = pc->xf->translate;

        want.x = want.y = want.z = 0.0F;
        gmCollisionGetFighterPartsWorldPosition(
            fp->joints[fp->attr->effect_joint_ids[want_id]], &want);

        if (want.x == seen[i].x && want.y == seen[i].y &&
            want.z == seen[i].z)
        {
            placed++;
        }
        if (i > 0 && (seen[i].x != seen[0].x || seen[i].y != seen[0].y ||
                      seen[i].z != seen[0].z))
        {
            spread++;
        }
        lbParticleEjectStruct(pc);
    }

    /* -- the named-joint arms: plain, then with an offset -- */
    for (i = 0; i < 2; i++)
    {
        LBParticle *pc;

        off.x = (i == 0) ? 0.0F : 40.0F;
        off.y = (i == 0) ? 0.0F : 20.0F;
        off.z = 0.0F;

        pc = (LBParticle *)
            ftParamMakeEffect(g, nEFKindDustExpandSmall, 4,
                              (i == 0) ? NULL : &off, NULL, fp->lr,
                              FALSE, 0);

        if (pc == NULL || pc->xf == NULL)
        {
            continue;
        }
        jmade++;

        want = off;
        gmCollisionGetFighterPartsWorldPosition(fp->joints[4], &want);

        if (want.x == pc->xf->translate.x && want.y == pc->xf->translate.y &&
            want.z == pc->xf->translate.z)
        {
            jplaced++;
        }
        lbParticleEjectStruct(pc);
    }

    /* -- and one that makes a model: the impact wave -- */
    wave = (GObj *)ftParamMakeEffect(g, nEFKindImpactWave,
                                     nFTPartsJointTopN, NULL, NULL,
                                     fp->lr, FALSE, 0);
    if (wave != NULL)
    {
        GObj *e = gGCCommonLinks[nGCCommonLinkIDEffect];

        while (e != NULL)
        {
            waves += (e == wave);
            e = e->link_next;
        }
    }
    dbglog(DBG_INFO,
           "db: gate -- cycle %d made %d placed %d spread, joint %d made "
           "%d placed, wave %d on the effect link (five calls through the "
           "cycler have to land on five different joints, a named joint "
           "and an offset on that joint's world position, and the impact "
           "wave on a GObj the link holds)\n",
           made, placed, spread, jmade, jplaced, waves);
}
#endif /* DB_GATE_PROBE */

/* ---- -DDB_SHADOW_PROBE: the blob shadow on the SH-4 ------------------
 *
 * Ports ft/ftshadow.c. The arithmetic is under the host
 * test; what only the target can say is that the strip reached the PVR
 * -- that the pack loaded, that the display walk ran the proc on link 7
 * in the translucent pass, and that the fighter's own floor line was
 * under him when it did.
 *
 * So this reads the model's log after a frame has drawn: how many
 * shadows the frame built, how many vertices the first one has, and the
 * two numbers that say the strip is on the floor and not at the origin
 * -- the span in world x, which is twice attr->shadow_size unless a
 * ledge clamped it, and the altitude, which has to be the floor line's
 * and not zero. */
#ifdef DB_SHADOW_PROBE
static void db_shadow_probe(int frame)
{
    const FTShadowDraw *d;
    GObj *g;
    FTStruct *fp;
    int i, shadows = 0, fighters = 0;
    int span, alt, ok = 0;

    if (frame != 240)
    {
        return;
    }
    /* link 13 carries the wallpaper and the transition too */
    for (g = gGCCommonLinks[nGCCommonLinkIDShadow]; g != NULL;
         g = g->link_next)
    {
        shadows += (g->id == nGCCommonKindShadow);
    }
    for (g = gGCCommonLinks[nGCCommonLinkIDFighter]; g != NULL;
         g = g->link_next)
    {
        fighters++;
    }
    if (gFTShadowLogCount == 0)
    {
        dbglog(DBG_INFO, "db: shadow -- %d on the link, none drawn\n",
               shadows);
        return;
    }
    d = &gFTShadowLog[0];
    span = d->vtx[2].n.ob[0] - d->vtx[0].n.ob[0];
    alt = d->vtx[0].n.ob[1];

    g = db_fighter_gobj(0);
    fp = (g != NULL) ? ftGetStruct(g) : NULL;

    /* every vertex of every draw at the floor's altitude, on the strip's
     * two z rails, and inside the texture */
    for (i = 0; i < gFTShadowLogCount; i++)
    {
        const FTShadowDraw *e = &gFTShadowLog[i];
        int k, good = (e->vtx_num >= 4 && e->tri_num >= 2 &&
                       e->prim[3] != 0);

        for (k = 0; k < e->vtx_num; k++)
        {
            good &= (e->vtx[k].n.ob[2] == 200 || e->vtx[k].n.ob[2] == -200);
            good &= (e->vtx[k].n.tc[1] >= 0 && e->vtx[k].n.tc[1] <= 2048);
        }
        ok += good;
    }
    dbglog(DBG_INFO,
           "db: shadow -- %d on the link for %d fighters, %d drawn, %d "
           "well formed; the "
           "first is %d verts %d tris, %d wide (2 * shadow_size %d) at "
           "y %d (fighter y %d), prim %02X%02X%02X%02X\n",
           shadows, fighters, (int)gFTShadowLogCount, ok, (int)d->vtx_num,
           (int)d->tri_num, span,
           (fp != NULL) ? (int)(2.0F * fp->attr->shadow_size) : -1, alt,
           (g != NULL) ? (int)DObjGetStruct(g)->translate.vec.f.y : -1,
           d->prim[0], d->prim[1], d->prim[2], d->prim[3]);
}
#endif /* DB_SHADOW_PROBE */

/* ---- -DDB_STANDIN_PROBE: the six stand-in rows, on the SH-4 ----------
 *
 * dFTCommonActionStatusDescs has six rows that must run the game's proc:
 * ft/ftcommon/ftcommonjumpaerial.c, ftcommonsquat.c and ftcommonfall.c
 * compile unmodified.
 *
 * `rows` is the six read back off the table. `jump` is the multi-jump
 * driven on a real fighter: a Mario in the air with a jump left takes
 * JumpAerialF and spends it, and one with none is refused -- which is
 * ftCommonJumpAerialCheckInterruptCommon, the function JumpAerialF's own
 * ProcInterrupt reaches through ftCommonFallProcInterrupt. `squat` is
 * the crouch: the stick down from Wait reaches Squat, and its ProcUpdate
 * hands it to SquatWait at the animation's end. */
#ifdef DB_STANDIN_PROBE
static void db_standin_probe(int frame)
{
    extern FTStatusDesc dFTCommonActionStatusDescs[];
    GObj *g = NULL;
    FTStruct *fp;
    FTStatusDesc *r;
    int rows, jump, jumpno, squat, squatwait, player;

    if (frame != 240)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *x = db_fighter_gobj(player);

        if (x != NULL)
        {
            g = x;
            break;
        }
    }
    if (g == NULL)
    {
        dbglog(DBG_INFO, "db: standin -- no fighter in the battle\n");
        return;
    }
    fp = ftGetStruct(g);

    r = &dFTCommonActionStatusDescs[nFTCommonStatusJumpAerialF - nFTCommonStatusActionStart];
    rows = (r->proc_update == ftCommonJumpAerialProcUpdate) &&
           (r->proc_interrupt == ftCommonJumpAerialProcInterrupt);
    r = &dFTCommonActionStatusDescs[nFTCommonStatusJumpAerialB - nFTCommonStatusActionStart];
    rows = rows && (r->proc_interrupt == ftCommonJumpAerialProcInterrupt);
    r = &dFTCommonActionStatusDescs[nFTCommonStatusFall - nFTCommonStatusActionStart];
    rows = rows && (r->proc_interrupt == ftCommonFallProcInterrupt);
    r = &dFTCommonActionStatusDescs[nFTCommonStatusFallAerial - nFTCommonStatusActionStart];
    rows = rows && (r->proc_interrupt == ftCommonFallProcInterrupt);
    r = &dFTCommonActionStatusDescs[nFTCommonStatusSquatWait - nFTCommonStatusActionStart];
    rows = rows && (r->proc_update == ftCommonSquatWaitProcUpdate);
    r = &dFTCommonActionStatusDescs[nFTCommonStatusSquatRv - nFTCommonStatusActionStart];
    rows = rows && (r->proc_interrupt == ftCommonSquatRvProcInterrupt);

    /* the second jump, on the fighter's own attributes */
    ftMainSetStatus(g, nFTCommonStatusFall, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    mpCommonSetFighterAir(fp);
    fp->jumps_used = 1;
    /* the jump input the game reads is a stick flick up inside the tap
     * window, or a jump button; the stick is the one a probe can set
     * without knowing the pad map (ftcommonkneebend.c:87-98) */
    fp->input.pl.stick_range.y = 80;
    fp->tap_stick_y = 0;
    jump = (ftCommonJumpAerialCheckInterruptCommon(g) != FALSE)
           ? (int)fp->status_id : -1;

    ftMainSetStatus(g, nFTCommonStatusFall, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    mpCommonSetFighterAir(fp);
    fp->jumps_used = (s32)fp->attr->jumps_max;
    fp->input.pl.stick_range.y = 80;
    fp->tap_stick_y = 0;
    jumpno = (ftCommonJumpAerialCheckInterruptCommon(g) != FALSE)
             ? (int)fp->status_id : -1;
    fp->input.pl.stick_range.y = 0;
    fp->jumps_used = 0;

    /* the crouch, and the wait its ProcUpdate hands it to */
    ftMainSetStatus(g, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    mpCommonSetFighterGround(fp);
    fp->input.pl.stick_range.y = -80;
    squat = (ftCommonSquatCheckInterruptCommon(g) != FALSE)
            ? (int)fp->status_id : -1;

    g->anim_frame = 0.0F;
    ftCommonSquatProcUpdate(g);
    squatwait = (int)fp->status_id;

    dbglog(DBG_INFO,
           "db: standin -- rows %d jump %d none %d squat %d wait %d "
           "want 1 %d -1 %d %d\n",
           rows, jump, jumpno, squat, squatwait,
           (int)nFTCommonStatusJumpAerialF, (int)nFTCommonStatusSquat,
           (int)nFTCommonStatusSquatWait);

    fp->input.pl.stick_range.y = 0;
    ftMainSetStatus(g, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
}
#endif /* DB_STANDIN_PROBE */

/* ---- -DDB_WPLIFE_PROBE: the weapon lifecycle on the SH-4 -------------
 *
 * Finds that wpManagerAllocWeapons was never called, so the
 * weapon struct pool was empty and every projectile this port compiled
 * failed on the first line of wpManagerMakeWeapon. That line is in now,
 * which means a whole path -- spawn, live, expire, return to the pool --
 * runs on target for the first time. This is the step that watches it,
 * over enough spawns that a leak of one struct per weapon would empty
 * the pool of 32 several times over.
 *
 * It drives Mario the way his own motion script does: put him in
 * SpecialN, raise the animation event, and call the accessory proc the
 * setter installed. One fireball every WPLIFE_EVERY tics for WPLIFE_N of
 * them, then a wait longer than the fireball's 140-tic lifetime, then
 * count the pool back.
 *
 * `free0` is the pool before the first spawn and `free1` after the last
 * has expired: they have to be equal, and equal to 32, or a weapon
 * leaked its struct. `made` is how many spawns came back non-NULL, `low`
 * the fewest free structs seen at any point (the peak in flight), and
 * `us` the microseconds a frame spent updating with the fireballs live,
 * which is the first cost this port has measured for a weapon at all. */
#ifdef DB_WPLIFE_PROBE
#define WPLIFE_EVERY 1
#define WPLIFE_N 200
#define WPLIFE_START 240

static s32 sDBWpLifeFree0 = -1;
static s32 sDBWpLifeLow = 99;
static s32 sDBWpLifeMade;
static s32 sDBWpLifeNull;
static u64 sDBWpLifeUs;
static s32 sDBWpLifeUsN;

static s32 db_wplife_free(void)
{
    extern WPStruct *sWPManagerStructsAllocFree;
    WPStruct *w;
    s32 n = 0;

    for (w = sWPManagerStructsAllocFree; w != NULL; w = w->next)
    {
        n++;
    }
    return n;
}

static void db_wplife_probe(int frame)
{
    GObj *mario = NULL;
    FTStruct *fp;
    s32 free_now, player;

    if (frame < WPLIFE_START)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindMario)
        {
            mario = g;
            break;
        }
    }
    if (mario == NULL)
    {
        if (frame == WPLIFE_START)
        {
            dbglog(DBG_INFO, "db: wplife -- no Mario in the battle\n");
        }
        return;
    }
    fp = ftGetStruct(mario);

    if (sDBWpLifeFree0 < 0)
    {
        sDBWpLifeFree0 = db_wplife_free();
    }
    free_now = db_wplife_free();

    if (free_now < sDBWpLifeLow)
    {
        sDBWpLifeLow = free_now;
    }
    if (frame >= WPLIFE_START && frame < WPLIFE_START + WPLIFE_N * WPLIFE_EVERY &&
        ((frame - WPLIFE_START) % WPLIFE_EVERY) == 0)
    {
        s32 before = free_now;

        mpCommonSetFighterGround(fp);
        ftMainSetStatus(mario, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
        ftMarioSpecialNSetStatus(mario);
        fp->motion_vars.flags.flag0 = 1;

        if (fp->proc_accessory != NULL)
        {
            fp->proc_accessory(mario);
        }
        if (db_wplife_free() < before)
        {
            sDBWpLifeMade++;
        }
        else
        {
            sDBWpLifeNull++;
        }
    }
    /* the frame cost while they are in the air */
    if (frame > WPLIFE_START && frame < WPLIFE_START + WPLIFE_N * WPLIFE_EVERY)
    {
        sDBWpLifeUs += dSYTaskmanUpdateTimeDelta;
        sDBWpLifeUsN++;
    }
    /* one fireball's lifetime past the last spawn, and then some */
    if (frame == WPLIFE_START + WPLIFE_N * WPLIFE_EVERY + 300)
    {
        mpCommonSetFighterGround(fp);
        ftMainSetStatus(mario, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
        fp->motion_vars.flags.flag0 = 0;

        dbglog(DBG_INFO,
               "db: wplife -- free0 %d free1 %d low %d made %d null %d "
               "us %d want %d %d <32 %d 0\n",
               (int)sDBWpLifeFree0, (int)db_wplife_free(), (int)sDBWpLifeLow,
               (int)sDBWpLifeMade, (int)sDBWpLifeNull,
               (int)(sDBWpLifeUsN ? sDBWpLifeUs / sDBWpLifeUsN : 0),
               WEAPON_ALLOC_MAX, WEAPON_ALLOC_MAX, WPLIFE_N);
    }
}
#endif /* DB_WPLIFE_PROBE */

/* ---- -DDB_WPQUAD_PROBE: the two new weapon models on the SH-4 --------
 *
 * Bakes Samus's Charge Shot and Pikachu's airborne Thunder
 * Jolt, the first projectiles this port has modelled since Fox's Blaster
 * Without a
 * sWPManagerModels row wpManagerAddModel finds nothing and
 * wpManagerMakeWeapon returns NULL.
 *
 * This spawns one of each directly, off whichever fighter is in the
 * battle, and reports what came back. `shot` and `jolt` are the two
 * GObjs being non-NULL, which is the whole point: a weapon with no pack
 * is a NULL. `tris` and `tex` are read off the packs the two GObjs are
 * drawing, and `life` is the jolt's own lifetime, which says the
 * WPStruct behind the model is the real one. Both are ejected before the
 * probe returns, so the pool is where it was. */
#ifdef DB_WPQUAD_PROBE
static void db_wpquad_probe(int frame)
{
    extern GObj* wpSamusChargeShotMakeWeapon(GObj *fighter_gobj, Vec3f *pos, s32 charge_level, sb32 is_release);
    extern GObj* wpPikachuThunderJoltAirMakeWeapon(GObj *fighter_gobj, Vec3f *pos, Vec3f *vel);
    extern GObj* wpYoshiStarMakeWeapon(GObj *fighter_gobj, Vec3f *pos, s32 lr);
    extern GObj* wpYoshiEggThrowMakeWeapon(GObj *fighter_gobj, Vec3f *pos);
    extern GObj* wpSamusBombMakeWeapon(GObj *fighter_gobj, Vec3f *pos);
    extern GObj* wpLinkBoomerangMakeWeapon(GObj *fighter_gobj, Vec3f *pos);
    extern GObj* wpKirbyCutterMakeWeapon(GObj *fighter_gobj, Vec3f *pos);
    extern GObj* wpPikachuThunderHeadMakeWeapon(GObj *fighter_gobj, Vec3f *pos, Vec3f *vel);
    GObj *owner = NULL;
    GObj *shot, *jolt, *star, *egg, *bomb, *boom, *cut, *gj, *th;
    Vec3f pos, vel;
    const Fighter *m;
    static int done = 0;
    int shot_ok, jolt_ok, star_ok, egg_ok, bomb_ok, boom_ok, cut_ok;
    int gj_ok, th_ok, bpal, bjnt, cjnt, gjmo, thtx;
    int tris, tex, life, player;

    /* latched rather than an equality on one frame: the weapon pool and
     * the battle both have to be up, and this says "the first frame at
     * or after 210" so a skipped count cannot silently skip the probe */
    if (done || frame < 210)
    {
        return;
    }
    done = 1;
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        if (db_fighter_gobj(player) != NULL)
        {
            owner = db_fighter_gobj(player);
            break;
        }
    }
    if (owner == NULL)
    {
        dbglog(DBG_INFO, "db: wpquad -- no fighter in the battle\n");
        return;
    }
    pos = DObjGetStruct(owner)->translate.vec.f;
    pos.y += 200.0F;
    vel.x = 20.0F;
    vel.y = 0.0F;
    vel.z = 0.0F;

    shot = wpSamusChargeShotMakeWeapon(owner, &pos, 1, TRUE);
    jolt = wpPikachuThunderJoltAirMakeWeapon(owner, &pos, &vel);
    /* Two, the ones the extern reloc walk found */
    star = wpYoshiStarMakeWeapon(owner, &pos, +1);
    egg = wpYoshiEggThrowMakeWeapon(owner, &pos);
    /* The only one with an MObj */
    bomb = wpSamusBombMakeWeapon(owner, &pos);
    /* The first weapon here with a DObjDesc TREE */
    boom = wpLinkBoomerangMakeWeapon(owner, &pos);
    /* A tree whose geometry is on DL links */
    cut = wpKirbyCutterMakeWeapon(owner, &pos);
    /* The ground crawler the air jolt becomes. It is
     * spawned from another WEAPON, so the air jolt above is its parent. */
    /* The bolt's head, whose model the trail shares */
    th = wpPikachuThunderHeadMakeWeapon(owner, &pos, &vel);
    gj = (jolt != NULL)
             ? wpPikachuThunderJoltGroundMakeWeapon(jolt, &pos,
                                                   nMPLineKindFloor)
             : NULL;

    shot_ok = (shot != NULL);
    jolt_ok = (jolt != NULL);
    star_ok = (star != NULL);
    egg_ok = (egg != NULL);
    bomb_ok = (bomb != NULL);
    boom_ok = (boom != NULL);
    cut_ok = (cut != NULL);
    gj_ok = (gj != NULL);
    th_ok = (th != NULL);
    bpal = bjnt = cjnt = gjmo = thtx = -1;
    tris = tex = life = -1;

    if (jolt != NULL)
    {
        m = dc_model_of(jolt);

        if (m != NULL && m->hd != NULL)
        {
            tris = (int)m->hd->tri_count;
            tex = (int)m->hd->tex_count;
        }
        life = (int)wpGetStruct(jolt)->lifetime;
        /* wpMainDestroyWeapon, not gcEjectGObj: the latter takes the GObj
         * and leaves the WPStruct out of the pool for good */
        wpMainDestroyWeapon(jolt);
    }
    if (shot != NULL)
    {
        wpMainDestroyWeapon(shot);
    }
    if (star != NULL)
    {
        wpMainDestroyWeapon(star);
    }
    if (egg != NULL)
    {
        wpMainDestroyWeapon(egg);
    }
    if (bomb != NULL)
    {
        /* the Bomb's two palettes are two frames of its one MObj, and its
         * own code winds palette_id between them as the fuse burns -- so
         * the MObj has to be there for the blink to have anywhere to go */
        m = dc_model_of(bomb);
        bpal = (m != NULL && m->mobjs != NULL) ? (int)m->mobjs->mobj_count : -1;
        wpMainDestroyWeapon(bomb);
    }
    if (boom != NULL)
    {
        /* three joints is the tree: every other weapon pack has one */
        m = dc_model_of(boom);
        bjnt = (m != NULL && m->hd != NULL) ? (int)m->hd->joint_count : -1;
        wpMainDestroyWeapon(boom);
    }
    if (cut != NULL)
    {
        m = dc_model_of(cut);
        cjnt = (m != NULL && m->hd != NULL) ? (int)m->hd->joint_count : -1;
        wpMainDestroyWeapon(cut);
    }
    if (gj != NULL)
    {
        /* six MObjs is the thing no weapon pack had before it */
        m = dc_model_of(gj);
        gjmo = (m != NULL && m->mobjs != NULL)
                   ? (int)m->mobjs->mobj_count : -1;
        wpMainDestroyWeapon(gj);
    }
    if (th != NULL)
    {
        /* four sprite frames on one MObj, and no animation to step them */
        m = dc_model_of(th);
        thtx = (m != NULL && m->hd != NULL) ? (int)m->hd->tex_count : -1;
        wpMainDestroyWeapon(th);
    }
    dbglog(DBG_INFO,
           "db: wpquad -- shot %d jolt %d star %d egg %d bomb %d boom %d "
           "cut %d gj %d th %d bpal %d bjnt %d cjnt %d gjmo %d thtx %d "
           "tris %d tex %d life %d "
           "want 1 1 1 1 1 1 1 1 1 1 3 2 6 4 2 1 %d\n",
           shot_ok, jolt_ok, star_ok, egg_ok, bomb_ok, boom_ok, cut_ok,
           gj_ok, th_ok, bpal, bjnt, cjnt, gjmo, thtx, tris, tex, life,
           (int)WPPIKACHUJOLT_LIFETIME);
}
#endif /* DB_WPQUAD_PROBE */

/* ---- -DDB_UB_PROBE: the AVOID_UB arms on the SH-4 --------------------
 *
 * Sets -DAVOID_UB in DECOMP_DEFS, which is the one variable
 * both KOS_CFLAGS and HOST_CFLAGS pick up. The host suite pins the four
 * reachable sites; this says the same four on the SH-4, where "fall off
 * the end of a non-void function" means a different register convention
 * and a different piece of garbage.
 *
 * `aobj` and `rate` are sys/objanim.c's two AObj readers asked about a
 * kind that matches no case; `trk` and `dtrk` are its two track readers
 * asked about nGCAnimTrackNone. Each is written to leave a DIFFERENT
 * value in the float unit first, so a fall-through would come back with
 * that value rather than with zero -- which is exactly what the host
 * bite showed. `shd` is src/dc/mnplayersvs.c's own guarded tail, which
 * the port keeps and now takes. */
#ifdef DB_UB_PROBE
static void db_ub_probe(int frame)
{
    extern f32 gcGetAObjValue(AObj *aobj);
    extern f32 gcGetAObjRate(AObj *aobj);
    extern f32 gcGetDObjAxisTrack(DObj *dobj, s32 track);
    extern f32 gcGetDObjDescAxisTrack(DObjDesc *dobjdesc, s32 track);
    extern s32 mnPlayersVSGetShade(s32 player);
    AObj aobj;
    DObjDesc desc;
    DObj dobj;
    int ok_aobj, ok_rate, ok_trk, ok_dtrk, live, shd;

    if (frame != 180)
    {
        return;
    }
    bzero(&aobj, sizeof(aobj));
    bzero(&desc, sizeof(desc));
    bzero(&dobj, sizeof(dobj));

    aobj.kind = nGCAnimKindNone;
    aobj.value_base = 111.0F;
    aobj.value_target = 222.0F;
    aobj.rate_base = 333.0F;
    ok_aobj = (gcGetAObjValue(&aobj) == 0.0F);
    ok_rate = (gcGetAObjRate(&aobj) == 0.0F);

    dobj.rotate.vec.f.x = 444.0F;
    dobj.scale.vec.f.z = 555.0F;
    ok_trk = (gcGetDObjAxisTrack(&dobj, nGCAnimTrackNone) == 0.0F);

    desc.rotate.x = 666.0F;
    desc.scale.z = 777.0F;
    ok_dtrk = (gcGetDObjDescAxisTrack(&desc, nGCAnimTrackNone) == 0.0F);

    /* and the three real cases still answer as the game answers, which
     * says the option added an arm rather than replacing one */
    aobj.kind = nGCAnimKindStep;
    aobj.length_invert = 1.0F;
    aobj.length = 2.0F;
    live = (gcGetAObjValue(&aobj) == 222.0F) &&
           (gcGetDObjAxisTrack(&dobj, nGCAnimTrackRotX) == 444.0F) &&
           (gcGetDObjDescAxisTrack(&desc, nGCAnimTrackScaZ) == 777.0F);

    shd = mnPlayersVSGetShade(0);

    dbglog(DBG_INFO,
           "db: ub -- aobj %d rate %d trk %d dtrk %d live %d shd %d "
           "want 1 1 1 1 1 0\n",
           ok_aobj, ok_rate, ok_trk, ok_dtrk, live, shd);
}
#endif /* DB_UB_PROBE */

/* ---- -DDB_AGILITY_PROBE: Pikachu's Quick Attack on the SH-4 ----------
 *
 * Finishes dFTPikachuSpecialStatusDescs at eighteen rows with
 * the Up-B, which finishes every playable fighter's moveset but the two
 * that wait on it/. Needs -DDB_BOOT_PIKACHU_P2 (P1 stays Fox).
 *
 * `rows` is the six rows and `anims` the four motions they name that are
 * real -- only four, because the two Start rows carry motion id -1 and
 * play no animation at all. `slots` is the two Up-B demux slots live.
 *
 * The rest walks the move on the live fighter. `start` is the B-tap with
 * the stick held up, `frz` says the Start really froze the animation and
 * made him intangible, `zip` is the twentieth tic teleporting him, `vy`
 * the speed a neutral stick gives that zip (straight up at full range:
 * 3 * 80 + 90 = 330), `jmp` says the zip spent his jumps, and `end` is
 * the recovery with a fifth of the velocity kept. */
#ifdef DB_AGILITY_PROBE
static void db_agility_probe(int frame)
{
    extern FTStatusDesc dFTPikachuSpecialStatusDescs[];
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirHiStatusList[])(GObj*);
    static const s32 kMotions[4] = {
        nFTPikachuMotionSpecialHi,    nFTPikachuMotionSpecialHiEnd,
        nFTPikachuMotionSpecialAirHi, nFTPikachuMotionSpecialAirHiEnd
    };
    GObj *pika_gobj = NULL;
    FTStruct *fp = NULL;
    const Fighter *model;
    int rows, anims, slots, start, frz, zip, jmp, end, player, i;
    f32 vy;

    if (frame != 150)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindPikachu)
        {
            pika_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: agility -- no Pikachu player in the battle "
                         "(build with -DDB_BOOT_PIKACHU_P2)\n");
        return;
    }

    slots = (dFTCommonSpecialHiStatusList[nFTKindPikachu] == ftPikachuSpecialHiStartSetStatus) &&
            (dFTCommonSpecialAirHiStatusList[nFTKindPikachu] == ftPikachuSpecialAirHiStartSetStatus);

    rows = 1;
    for (i = nFTPikachuStatusSpecialHiStart; i <= nFTPikachuStatusSpecialAirHiEnd; i++)
    {
        FTStatusDesc *row = &dFTPikachuSpecialStatusDescs[i - nFTCommonStatusSpecialStart];

        rows = rows && (row->proc_update != NULL) && (row->proc_physics != NULL) &&
               (row->proc_map != NULL) && (row->sflags.is_projectile == FALSE) &&
               (row->sflags.attack_id == nFTStatusAttackIDSpecialLw);
    }
    /* the two Start rows play nothing, and the airborne one is marked
     * Ground -- both the game's own, both kept */
    rows = rows &&
           (dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialHiStart - nFTCommonStatusSpecialStart].mflags.motion_id == -1) &&
           (dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialAirHiStart - nFTCommonStatusSpecialStart].mflags.motion_id == -1) &&
           (dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialAirHiStart - nFTCommonStatusSpecialStart].sflags.ga == nMPKineticsGround);

    model = dc_model_of(pika_gobj);
    anims = 0;
    for (i = 0; i < 4; i++)
    {
        if ((kMotions[i] < (s32)model->motion_count) &&
            (model->motions[kMotions[i]].anim >= 0))
        {
            anims |= (1 << i);
        }
    }

    /* the B-tap with the stick held up, on the ground */
    mpCommonSetFighterGround(fp);
    ftMainSetStatus(pika_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->attr->is_have_specialhi = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = I_CONTROLLER_RANGE_MAX;
    ftCommonSpecialHiCheckInterruptCommon(pika_gobj);
    start = (int)fp->status_id;
    frz = (fp->hitstatus == nGMHitStatusIntangible) &&
          (fp->status_vars.pikachu.specialhi.anim_frames == FTPIKACHU_QUICKATTACK_START_TIME);
    fp->input.pl.button_tap = 0;

    /* the stick goes neutral, so the twentieth tic zips straight up */
    fp->input.pl.stick_range.y = 0;
    fp->jumps_used = 0;
    for (i = 0; i < FTPIKACHU_QUICKATTACK_START_TIME; i++)
    {
        ftPikachuSpecialHiStartProcUpdate(pika_gobj);
    }
    zip = (int)fp->status_id;
    vy = fp->physics.vel_air.y;
    jmp = (fp->jumps_used == fp->attr->jumps_max);

    /* and the recovery keeps a fifth of it */
    fp->physics.vel_air.x = 0.0F;
    fp->physics.vel_air.y = 100.0F;
    ftPikachuSpecialAirHiEndSetStatus(pika_gobj);
    end = (int)fp->status_id;

    dbglog(DBG_INFO,
           "db: agility -- slots %d rows %d anims 0x%X start %d frz %d "
           "zip %d vy %d jmp %d end %d vbak %d want 1 1 0xF %d 1 %d %d 1 %d 20\n",
           slots, rows, anims, start, frz, zip, (int)vy, jmp, end,
           (int)fp->physics.vel_air.y,
           (int)nFTPikachuStatusSpecialHiStart,
           (int)nFTPikachuStatusSpecialAirHi,
           (int)(FTPIKACHU_QUICKATTACK_VEL_BASE * F_CONTROLLER_RANGE_MAX +
                 FTPIKACHU_QUICKATTACK_VEL_ADD),
           (int)nFTPikachuStatusSpecialAirHiEnd);

    mpCommonSetFighterGround(fp);
    ftMainSetStatus(pika_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    ftParamSetHitStatusAll(pika_gobj, nGMHitStatusNormal);
    fp->physics.vel_air.x = fp->physics.vel_air.y = fp->physics.vel_ground.x = 0.0F;
    fp->jumps_used = 0;
    fp->motion_vars.flags.flag0 = fp->motion_vars.flags.flag1 =
        fp->motion_vars.flags.flag2 = 0;
    fp->proc_damage = NULL;
    fp->proc_accessory = NULL;
}
#endif /* DB_AGILITY_PROBE */

/* ---- -DDB_THUNDER_PROBE: Pikachu's Thunder on the SH-4 ---------------
 *
 * dFTPikachuSpecialStatusDescs has twelve rows including the Down-B. Pikachu is not in kBootKinds, so this needs
 * -DDB_BOOT_PIKACHU_P2 (P1 stays Fox).
 *
 * `rows` is the eight rows' motions and procs and `anims` the
 * target-only one -- all eight motion ids really in PIKACHU's pack,
 * which the host mock (every motion anim 0) can never fail. `slots` is
 * the two Down-B demux slots live and the Up-B pair still NULL.
 *
 * The rest walks the move on the live fighter. `start` is the B-tap with
 * the stick held down, `loop` the animation's end calling the bolt, `end`
 * the recovery a spent bolt forces, `hit` the airborne strike and `vy`
 * the upward kick it gives him. `dmg` is the DECOMP QUIRK this step
 * found and did not fix: ftPikachuSpecialLwLoopUpdateThunder assigns
 * fp->proc_damage and the setter calls it BEFORE ftMainSetStatus, which
 * clears every proc hook -- so it comes back 0, and
 * ftPikachuSpecialLwProcDamage is dead in the shipped game. */
#ifdef DB_THUNDER_PROBE
static void db_thunder_probe(int frame)
{
    extern FTStatusDesc dFTPikachuSpecialStatusDescs[];
    extern void (*dFTCommonSpecialAirLwStatusList[])(GObj*);
    extern void (*dFTCommonSpecialAirHiStatusList[])(GObj*);
    extern void (*dFTCommonSpecialHiStatusList[])(GObj*);
    static const s32 kMotions[8] = {
        nFTPikachuMotionSpecialLwStart,    nFTPikachuMotionSpecialLwLoop,
        nFTPikachuMotionSpecialLwHit,      nFTPikachuMotionSpecialLwEnd,
        nFTPikachuMotionSpecialAirLwStart, nFTPikachuMotionSpecialAirLwLoop,
        nFTPikachuMotionSpecialAirLwHit,   nFTPikachuMotionSpecialAirLwEnd
    };
    GObj *pika_gobj = NULL;
    FTStruct *fp = NULL;
    const Fighter *model;
    int rows, anims, slots, start, loop, end, hit, dmg, player, i;
    f32 vy;

    if (frame != 120)
    {
        return;
    }
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);

        if (g != NULL && ftGetStruct(g)->fkind == nFTKindPikachu)
        {
            pika_gobj = g;
            fp = ftGetStruct(g);
            break;
        }
    }
    if (fp == NULL)
    {
        dbglog(DBG_INFO, "db: thunder -- no Pikachu player in the battle "
                         "(build with -DDB_BOOT_PIKACHU_P2)\n");
        return;
    }

    slots = (dFTCommonSpecialAirLwStatusList[nFTKindPikachu] == ftPikachuSpecialAirLwStartSetStatus) &&
            (dFTCommonSpecialHiStatusList[nFTKindPikachu] == NULL) &&
            (dFTCommonSpecialAirHiStatusList[nFTKindPikachu] == NULL);

    rows = 1;
    for (i = 0; i < 8; i++)
    {
        FTStatusDesc *row = &dFTPikachuSpecialStatusDescs[
            nFTPikachuStatusSpecialLwStart - nFTCommonStatusSpecialStart + i];

        rows = rows && (row->proc_update != NULL) && (row->proc_physics != NULL) &&
               (row->proc_map != NULL) && (row->mflags.motion_id == kMotions[i]) &&
               (row->sflags.is_projectile == TRUE);
    }
    rows = rows &&
           (dFTPikachuSpecialStatusDescs[nFTPikachuStatusSpecialAirLwHit - nFTCommonStatusSpecialStart].proc_physics
            == ftPikachuSpecialAirLwHitProcPhysics);

    model = dc_model_of(pika_gobj);
    anims = 0;
    for (i = 0; i < 8; i++)
    {
        if ((kMotions[i] < (s32)model->motion_count) &&
            (model->motions[kMotions[i]].anim >= 0))
        {
            anims |= (1 << i);
        }
    }

    /* the B-tap with the stick held down, on the ground */
    mpCommonSetFighterGround(fp);
    ftMainSetStatus(pika_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->attr->is_have_speciallw = TRUE;
    fp->input.pl.button_tap = fp->input.button_mask_b;
    fp->input.pl.stick_range.x = 0;
    fp->input.pl.stick_range.y = FTCOMMON_SPECIALLW_STICK_RANGE_MIN;
    ftCommonSpecialLwCheckInterruptCommon(pika_gobj);
    start = (int)fp->status_id;
    fp->input.pl.button_tap = 0;
    fp->input.pl.stick_range.y = 0;

    /* the animation's end calls the bolt down and enters the Loop */
    fp->status_vars.pikachu.speciallw.thunder_gobj = NULL;
    pika_gobj->anim_frame = 0.0F;
    ftPikachuSpecialLwStartProcUpdate(pika_gobj);
    loop = (int)fp->status_id;
    dmg = (fp->proc_damage != NULL);

    /* the bolt is unmodelled, so the Loop reads it as spent and recovers */
    fp->passive_vars.pikachu.is_thunder_destroy = FALSE;
    fp->status_vars.pikachu.speciallw.thunder_gobj = NULL;
    ftPikachuSpecialLwLoopProcUpdate(pika_gobj);
    end = (int)fp->status_id;

    /* the airborne strike, and the kick it gives him */
    mpCommonSetFighterAir(fp);
    ftMainSetStatus(pika_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->physics.vel_air.y = -99.0F;
    ftPikachuSpecialAirLwHitSetStatus(pika_gobj);
    hit = (int)fp->status_id;
    vy = fp->physics.vel_air.y;

    dbglog(DBG_INFO,
           "db: thunder -- slots %d rows %d anims 0x%X start %d loop %d "
           "end %d hit %d vy %d dmg %d want 1 1 0xFF %d %d %d %d %d 0\n",
           slots, rows, anims, start, loop, end, hit, (int)vy, dmg,
           (int)nFTPikachuStatusSpecialLwStart,
           (int)nFTPikachuStatusSpecialLwLoop,
           (int)nFTPikachuStatusSpecialLwEnd,
           (int)nFTPikachuStatusSpecialAirLwHit,
           (int)FTPIKACHU_THUNDER_HITVEL_Y);

    mpCommonSetFighterGround(fp);
    ftMainSetStatus(pika_gobj, nFTCommonStatusWait, 0.0F, 1.0F, FTSTATUS_PRESERVE_NONE);
    fp->physics.vel_air.y = 0.0F;
    fp->status_vars.pikachu.speciallw.thunder_gobj = NULL;
    fp->passive_vars.pikachu.is_thunder_destroy = FALSE;
    fp->motion_vars.flags.flag0 = fp->motion_vars.flags.flag1 =
        fp->motion_vars.flags.flag2 = 0;
    fp->proc_damage = NULL;
    fp->proc_accessory = NULL;
}
#endif /* DB_THUNDER_PROBE */

/* ---- -DDB_ENTRANCE_PROBE: the roster's warp-in on the SH-4 -----------
 *
 * Fills dFTCommonEntryAppearStatusIDs, which holds a row
 * for twenty-five of the twenty-seven
 * kinds warping in, and the four boot kinds -- Fox, Mario, Kirby, Yoshi --
 * do it on this disc without any knob at all. That is what this probe
 * watches: it does not DRIVE anything, it samples every frame from the
 * match's start and reports what the countdown really did.
 *
 * `live` is the census, both facings, over all twenty-seven kinds: the
 * entry table names a status AND that status has a row in the table
 * dFTMainSpecialStatusDescs points the kind at. Those are two facts and
 * this milestone found the gap between them four times, so
 * ftCommonEntryAppearIsLive asks both. It reads 26: the only kind left out is Master Hand, which has
 * no special table.
 *
 * `warp` is one bit per player who was seen in a status of his own
 * kind's Appear pair; `wait` is one bit per player who then reached Wait,
 * which is the animation's end handing him back his facing, his floor
 * line and his spawn point (ftCommonAppearProcUpdate). `anims` is the
 * target-only field the host mock can never fail -- every mock motion has
 * anim 0 -- and says the Appear motion each row named is really in that
 * fighter's pack. `dl` is Captain Falcon's display link during his
 * left-facing Start, which is 1 (STAGE_DLLINK, drawn behind the stage:
 * the Blue Falcon drives in from back there) and -1 when no Captain is
 * playing; build with -DDB_BOOT_P2_KIND=nFTKindCaptain to see it. */
#ifdef DB_ENTRANCE_PROBE
static s32 sDBEntranceStatus[GMCOMMON_PLAYERS_MAX];
static s32 sDBEntranceMotion[GMCOMMON_PLAYERS_MAX];
static s32 sDBEntranceKind[GMCOMMON_PLAYERS_MAX];
static s32 sDBEntranceWaited[GMCOMMON_PLAYERS_MAX];
static s32 sDBEntranceDLLink = -1;

/* the two Appear status ids of every kind the four boot rosters and the
 * DB_BOOT_* knobs can produce, in the kinds' own enums */
static sb32 db_entrance_is_appear(s32 fkind, s32 status_id)
{
    switch (fkind)
    {
    case nFTKindMario:
    case nFTKindLuigi:
    case nFTKindMMario:
        return (status_id == nFTMarioStatusAppearR) ||
               (status_id == nFTMarioStatusAppearL);
    case nFTKindFox:
        return (status_id == nFTFoxStatusAppearR) ||
               (status_id == nFTFoxStatusAppearL);
    case nFTKindDonkey:
    case nFTKindGDonkey:
        return (status_id == nFTDonkeyStatusAppearR) ||
               (status_id == nFTDonkeyStatusAppearL);
    case nFTKindSamus:
        return (status_id == nFTSamusStatusAppearR) ||
               (status_id == nFTSamusStatusAppearL);
    case nFTKindLink:
        return (status_id == nFTLinkStatusAppearR) ||
               (status_id == nFTLinkStatusAppearL);
    case nFTKindYoshi:
        return (status_id == nFTYoshiStatusAppearR) ||
               (status_id == nFTYoshiStatusAppearL);
    case nFTKindCaptain:
        return (status_id == nFTCaptainStatusAppearRStart) ||
               (status_id == nFTCaptainStatusAppearLStart) ||
               (status_id == nFTCaptainStatusAppearREnd) ||
               (status_id == nFTCaptainStatusAppearLEnd);
    case nFTKindKirby:
        return (status_id == nFTKirbyStatusAppearR) ||
               (status_id == nFTKirbyStatusAppearL);
    case nFTKindPurin:
        return (status_id == nFTPurinStatusAppearR) ||
               (status_id == nFTPurinStatusAppearL);
    case nFTKindNess:
        return (status_id == nFTNessStatusAppearRStart) ||
               (status_id == nFTNessStatusAppearLStart) ||
               (status_id == nFTNessStatusAppearWait) ||
               (status_id == nFTNessStatusAppearREnd) ||
               (status_id == nFTNessStatusAppearLEnd);
    }
    return FALSE;
}

static void db_entrance_probe(int frame)
{
    extern sb32 ftCommonEntryAppearIsLive(s32 fkind, s32 entry_id);
    int warp, waited, anims, live, player, i;

    if (frame == 0)
    {
        for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
        {
            sDBEntranceStatus[i] = -1;
            sDBEntranceMotion[i] = -1;
            sDBEntranceKind[i] = -1;
            sDBEntranceWaited[i] = 0;
        }
        sDBEntranceDLLink = -1;
    }
    if (frame <= 300)
    {
        for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
        {
            GObj *g = db_fighter_gobj(player);
            FTStruct *fp;

            if (g == NULL)
            {
                continue;
            }
            fp = ftGetStruct(g);
            sDBEntranceKind[player] = fp->fkind;

            if ((sDBEntranceStatus[player] < 0) &&
                db_entrance_is_appear(fp->fkind, fp->status_id))
            {
                sDBEntranceStatus[player] = (s32)fp->status_id;
                sDBEntranceMotion[player] = (s32)fp->motion_id;

                if ((fp->fkind == nFTKindCaptain) &&
                    (fp->status_vars.common.entry.lr == -1))
                {
                    sDBEntranceDLLink = (s32)fp->dl_link;
                }
            }
            if ((sDBEntranceStatus[player] >= 0) &&
                (fp->status_id == nFTCommonStatusWait))
            {
                sDBEntranceWaited[player] = 1;
            }
        }
    }
    if (frame != 300)
    {
        return;
    }
    live = 0;
    for (i = 0; i < nFTKindEnumCount; i++)
    {
        live += (ftCommonEntryAppearIsLive(i, 0) != FALSE) &&
                (ftCommonEntryAppearIsLive(i, 1) != FALSE);
    }
    warp = waited = anims = 0;
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        GObj *g = db_fighter_gobj(player);
        const Fighter *model;

        if (sDBEntranceStatus[player] < 0)
        {
            continue;
        }
        warp |= (1 << player);
        waited |= (sDBEntranceWaited[player] << player);

        if (g == NULL)
        {
            continue;
        }
        model = dc_model_of(g);

        if ((model != NULL) && (sDBEntranceMotion[player] >= 0) &&
            (sDBEntranceMotion[player] < (s32)model->motion_count) &&
            (model->motions[sDBEntranceMotion[player]].anim >= 0))
        {
            anims |= (1 << player);
        }
    }
    dbglog(DBG_INFO,
           "db: entrance -- live %d warp 0x%X wait 0x%X anims 0x%X dl %d "
           "kinds %d,%d,%d,%d statuses %d,%d,%d,%d want 26 0xF 0xF 0xF\n",
           live, warp, waited, anims, (int)sDBEntranceDLLink,
           (int)sDBEntranceKind[0], (int)sDBEntranceKind[1],
           (int)sDBEntranceKind[2], (int)sDBEntranceKind[3],
           (int)sDBEntranceStatus[0], (int)sDBEntranceStatus[1],
           (int)sDBEntranceStatus[2], (int)sDBEntranceStatus[3]);
}
#endif /* DB_ENTRANCE_PROBE */




/* ---- the build knobs (src/dc/db.h has the list) ----------------------
 *
 * Nothing here loads a stage: the stage select
 * acquires the one it is previewing and the battle the one it plays on,
 * and both give them back (src/dc/stage.h). What is left is the boot
 * stage's hold, taken in db_install below and only when the chain skips
 * the menus. */

/* -DDB_SOAK: an unattended soak build (`./run.sh soak`). Four CPU fighters
 * on a random stage, the results screen for DB_SOAK_RESULTS_TICS, then
 * a fresh random match, forever -- see db_soak_deal below. It is a
 * DB_BOOT_SCENE=nSCKindVSBattle build with the roster and stage dealt
 * anew each round. */
#ifdef DB_SOAK
#define DB_BOOT_SCENE nSCKindVSBattle
#define DB_BOOT_PLAYERS 4
#define DB_BOOT_COM_ALL
#ifndef DB_SOAK_MATCH_MINUTES
#define DB_SOAK_MATCH_MINUTES 2
#endif
#ifndef DB_SOAK_RESULTS_TICS
#define DB_SOAK_RESULTS_TICS 600
#endif
#endif

#ifndef DB_BOOT_STAGE
#define DB_BOOT_STAGE nGRKindHyrule
#endif
/* Which RUNG of the 1P ladder a direct jump to nSCKind1PIntro is for --
 * an nSC1PGameStage, not an nGRKind, which is why it is not
 * DB_BOOT_STAGE. 0 is the Link rung, the first one a 1P game plays:
 *
 *   EXTRA_CFLAGS="-DDB_BOOT_SCENE=nSCKind1PIntro -DDB_BOOT_1P_STAGE=4"
 *
 * puts the card on the Mario Bros. rung instead. The same knob picks
 * the rung a direct jump to nSCKind1PGame plays, which is
 * the same ladder position one scene later. */
#ifndef DB_BOOT_1P_STAGE
#define DB_BOOT_1P_STAGE 0
#endif
/* Which rule the chain's match plays under before the VS mode screen has
 * run. Stock, as this has always played it; a TIME match is the
 * one way to see the match clock (src/dc/ifcommon.c)
 * without driving the pad through the VS mode screen:
 *
 *   EXTRA_CFLAGS=-DDB_BOOT_RULE=SCBATTLE_GAMERULE_TIME ./run.sh disc
 */
#ifndef DB_BOOT_RULE
#define DB_BOOT_RULE SCBATTLE_GAMERULE_STOCK
#endif

#ifndef DB_BOOT_SCENE
#define DB_BOOT_SCENE nSCKindTitle
#endif

/* How many of the four slots the boot battle state fills, for a build
 * that skips the menus (DB_BOOT_SCENE=nSCKindVSBattle). Two is what the
 * chain has always played; four shows that four
 * physical ports reach four fighters, each reading its own. The
 * character select fills these in for itself when the chain runs. */
#ifndef DB_BOOT_PLAYERS
#define DB_BOOT_PLAYERS 2
#endif

/* P1's and P2's boot fighter -- any nFTKind* value, one flag per slot:
 * -DDB_BOOT_P2_KIND=nFTKindSamus reaches every
 * fighter this port has. */
/* Which special bonuses a direct jump to the score screen
 * should show. The low word of gSCManagerSceneData.bonus_get_mask, one
 * bit per bonus in nSC1PGameBonus order; src/dc/db.c's own boot block
 * says why it is dealt rather than earned. Four rows by default.
 *
 *   EXTRA_CFLAGS="-DDB_BOOT_SCENE=nSCKind1PStageClear -DDB_BOOT_1P_BONUS=0xFFFF"
 */
#ifndef DB_BOOT_1P_BONUS
#define DB_BOOT_1P_BONUS 0x0000000Fu
#endif

/* Whether a direct jump to a bonus practice select
 * should be dealt a save with the bonus games already won. The two
 * panels that make that select different from the 1P game's -- the
 * records panel beside the grid (mnPlayers1PBonusMakeHiScore, which
 * draws a best TIME only once the course is finished and a task count
 * before that) and the TOTAL TIME line (mnPlayers1PBonusMakeTotalTime,
 * which only exists once every one of the twelve is finished) -- read
 * gSCManagerBackupData.spgame_records, and a fresh save leaves both
 * empty. So a probe can ask for a finished one, the same way the score
 * screen above is dealt a finished rung.
 *
 *   EXTRA_CFLAGS="-DDB_BOOT_SCENE=nSCKind1PBonus2Players -DDB_BOOT_BONUS_RECORDS=1"
 */
#ifndef DB_BOOT_BONUS_RECORDS
#define DB_BOOT_BONUS_RECORDS 0
#endif

/* -DDB_BOOT_P1_COSTUME=<n>: the human's costume on a direct jump to the
 * 1P ladder's card or game (the common costume id, 0-3). What the team
 * cards' sprites vary with (tools/export/ssb_introcrowd.py). */
#ifndef DB_BOOT_P1_COSTUME
#define DB_BOOT_P1_COSTUME 0
#endif
#ifndef DB_BOOT_P1_KIND
#define DB_BOOT_P1_KIND nFTKindFox
#endif

#ifndef DB_BOOT_P2_KIND
#define DB_BOOT_P2_KIND nFTKindMario
#endif

/* P3 and P4, on a DB_BOOT_PLAYERS=4 build. Kirby and Yoshi unset, as
 * they always were. */
#ifndef DB_BOOT_P3_KIND
#define DB_BOOT_P3_KIND nFTKindKirby
#endif

#ifndef DB_BOOT_P4_KIND
#define DB_BOOT_P4_KIND nFTKindYoshi
#endif

#ifndef DB_BOOT_COM_LEVEL
#define DB_BOOT_COM_LEVEL 9
#endif

static s32 gBootStage = DB_BOOT_STAGE;

#ifdef DB_SOAK
static u32 sSoakRand;
static u32 sSoakMatch;

static u32 db_soak_rand(void)
{
    sSoakRand ^= sSoakRand << 13;
    sSoakRand ^= sSoakRand >> 17;
    sSoakRand ^= sSoakRand << 5;
    return sSoakRand;
}

/* Deal the next match into gSCManagerTransferBattleState: a random VS
 * stage that has a pack, and a random playable fighter in each slot, all
 * four CPUs. Same-fighter clones get distinct costumes, as the character
 * select would. The stage is stored in gBootStage and the scene data;
 * acquiring it is the caller's. */
static void db_soak_deal(void)
{
    static const s32 kStages[] = {
        nGRKindCastle, nGRKindSector, nGRKindJungle, nGRKindZebes,
        nGRKindHyrule, nGRKindYoster, nGRKindPupupu, nGRKindYamabuki,
        nGRKindInishie
    };
    s32 stage, player, other;

    /* a two-minute time match, every round; stocks high enough that
     * only the clock ends it, as the VS mode's TIME rule leaves them */
    gSCManagerTransferBattleState.game_rules = SCBATTLE_GAMERULE_TIME;
    gSCManagerTransferBattleState.time_limit = DB_SOAK_MATCH_MINUTES;
    gSCManagerTransferBattleState.stocks = 99;

    if (sSoakRand == 0)
    {
#ifdef DB_SOAK_SEED
        sSoakRand = DB_SOAK_SEED;
#else
        /* The RTC is the wall clock (set by the host or the cable), so two
         * launches differ; the microsecond timer adds the boot's own
         * jitter. Milliseconds since power-on alone repeat run to run. */
        sSoakRand = ((u32)rtc_unix_secs() * 2654435761u) ^
                    ((u32)timer_us_gettime64() | 1);
#endif
        dbglog(DBG_INFO, "soak: seed %lu\n", (unsigned long)sSoakRand);
    }
#ifndef DB_SOAK_SEED
    /* fresh entropy each round: the time the match happened to end */
    sSoakRand ^= (u32)timer_us_gettime64();
    if (sSoakRand == 0)
    {
        sSoakRand = 1;
    }
#endif
    do
    {
        stage = kStages[db_soak_rand() % ARRAY_COUNT(kStages)];
    } while (grStageFileName(stage) == NULL ||
             (sSoakMatch > 0 && stage == gBootStage));
    gBootStage = stage;
    gSCManagerTransferBattleState.gkind = stage;
    gSCManagerSceneData.gkind = stage;
    gSCManagerSceneData.maps_vsmode_gkind = stage;

    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        SCPlayerData *pd = &gSCManagerTransferBattleState.players[player];

        pd->pkind = nFTPlayerKindCom;
        pd->fkind = nFTKindPlayableStart +
            db_soak_rand() % (nFTKindPlayableEnd - nFTKindPlayableStart + 1);
        pd->costume = 0;
        for (other = 0; other < player; other++)
        {
            if (gSCManagerTransferBattleState.players[other].fkind == pd->fkind)
            {
                pd->costume++;
            }
        }
        pd->team = pd->player = pd->tag = pd->color = player;
        pd->shade = 0;
        pd->level = 9;
        pd->handicap = 5;
        pd->stock_count = gSCManagerTransferBattleState.stocks;
    }
    sSoakMatch++;
    dbglog(DBG_INFO, "soak: match %lu stage %d fighters %d %d %d %d\n",
           (unsigned long)sSoakMatch, (int)stage,
           (int)gSCManagerTransferBattleState.players[0].fkind,
           (int)gSCManagerTransferBattleState.players[1].fkind,
           (int)gSCManagerTransferBattleState.players[2].fkind,
           (int)gSCManagerTransferBattleState.players[3].fkind);
}

/* The results screen's exit, in place of its START press: after
 * DB_SOAK_RESULTS_TICS it does what mnVSResultsFuncRun's exit does
 * (silence, next scene) and names the battle as that scene, on a freshly
 * dealt roster with the boot stage's hold moved to the new stage. */
static void db_soak_results(void)
{
    s32 old_stage;

    if (gSCManagerSceneData.scene_curr != nSCKindVSResults ||
        dSYTaskmanUpdateCount < DB_SOAK_RESULTS_TICS)
    {
        return;
    }
    old_stage = gBootStage;
    db_soak_deal();
    if (grStageAcquire(gBootStage) == NULL)
    {
        dbglog(DBG_ERROR, "soak: cannot load stage %d\n", (int)gBootStage);
        return;
    }
    grStageRelease(old_stage);
    stage_bind(gGRStages[gBootStage]);
    func_800266A0_272A0();
    syAudioStopBGMAll();
    gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
    gSCManagerSceneData.scene_curr = nSCKindVSBattle;
    syTaskmanSetLoadScene();
}
#endif

/* P1's FTStruct, for this file's own log lines and the cliff warp */
#define DB_P1_FP() ftGetStruct(db_fighter_gobj(0))

/* ---- -DDB_COLLISION_OVERLAY: world lines as screen-space quads -------
 *
 * OFF unless the build asks for it. Every collision line the stage
 * carries, drawn over the game in the colour of its kind, plus each
 * fighter's collision diamond -- an instrument, and one that covers the
 * picture it is drawn over, which is why it is not in a build you play.
 *
 * The one thing that makes it draw is easy to lose, because the failure
 * is invisible from inside the file: four lines compile gLineHdr:
 *
 *     pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
 *     cxt.gen.culling = PVR_CULLING_NONE;
 *     pvr_poly_compile(&gLineHdr, &cxt);
 *
 * A zeroed pvr_poly_hdr_t is not a poly header, so the TA drops every
 * strip submitted behind it: no lines, no error, no dropped frames, and
 * a per-frame walk of every collision line still running to feed it.
 * Nothing catches that -- the port has no test that can see the screen.
 * db_overlay_init below is those four lines. */
#ifdef DB_COLLISION_OVERLAY

static pvr_poly_hdr_t gLineHdr;

/* draw_line winds its quads negative-area in screen space; the default
 * CCW mode culls those. */
static void db_overlay_init(void)
{
    pvr_poly_cxt_t cxt;

    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    cxt.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&gLineHdr, &cxt);
}

/* The camera pass established a view and a projection (row-major,
 * column vectors, objpvr.h); a world point on the z = 0 plane goes
 * through both. The game's battle camera tilts and yaws with its
 * target, so nothing here assumes the view is a translation. */
static int project(float x, float y, float *sx, float *sy)
{
    const float *view = gcGetViewF();
    const float *proj = gcGetProjF();
    float vx = view[0] * x + view[1] * y + view[3];
    float vy = view[4] * x + view[5] * y + view[7];
    float vz = view[8] * x + view[9] * y + view[11];
    float cx = proj[0] * vx + proj[1] * vy + proj[2] * vz + proj[3];
    float cy = proj[4] * vx + proj[5] * vy + proj[6] * vz + proj[7];
    float cw = proj[12] * vx + proj[13] * vy + proj[14] * vz + proj[15];

    if (cw <= 0.0f)
        return 0;
    *sx = gcGetViewport()->cx + cx / cw * gcGetViewport()->hw;
    *sy = gcGetViewport()->cy - cy / cw * gcGetViewport()->hh;
    return 1;
}

static void draw_line(float x1, float y1, float x2, float y2, uint32_t argb)
{
    float ax, ay, bx, by, nx, ny, len;
    pvr_vertex_t v;

    if (!project(x1, y1, &ax, &ay) || !project(x2, y2, &bx, &by))
        return;
    nx = -(by - ay);
    ny = bx - ax;
    len = fsqrt(nx * nx + ny * ny);
    if (len < 1e-3f)
        return;
    nx /= len;
    ny /= len;

    memset(&v, 0, sizeof(v));
    v.argb = argb;
    v.z = 4.0f;
    v.flags = PVR_CMD_VERTEX;
    v.x = ax - nx;
    v.y = ay - ny;
    pvr_prim(&v, sizeof(v));
    v.x = ax + nx;
    v.y = ay + ny;
    pvr_prim(&v, sizeof(v));
    v.x = bx - nx;
    v.y = by - ny;
    pvr_prim(&v, sizeof(v));
    v.flags = PVR_CMD_VERTEX_EOL;
    v.x = bx + nx;
    v.y = by + ny;
    pvr_prim(&v, sizeof(v));
}

/* Say so the first time the overlay meets a line that has gone away, and
 * not again: a hazard that drops a floor leaves it down for hundreds of
 * tics, and one line per frame would drown the log it is trying to keep
 * readable. Bounded the same 16 ways the rest of this file's reports
 * are. */
static void db_note_vanish(s32 line_id)
{
    static s32 seen[16];
    static int nseen;

    int i;

    for (i = 0; i < nseen; i++)
    {
        if (seen[i] == line_id)
        {
            return;
        }
    }
    if (nseen >= (int)(sizeof(seen) / sizeof(seen[0])))
    {
        return;
    }
    seen[nseen++] = line_id;

    dbglog(DBG_INFO, "db: overlay: line %d's yakumono is off -- not "
           "drawn (%d so far)\n", (int)line_id, nseen);
}

/* Walk the collision the way the game's own queries do: every line of
 * every kind, each a polyline of vertices. */
static void draw_collision(void)
{
    static s32 line_ids[256];
    s32 type, count, i, v, nvert;

    pvr_prim(&gLineHdr, sizeof(gLineHdr));

    for (type = 0; type < nMPLineKindEnumCount; type++)
    {
        count = mpCollisionGetLineCountType(type);
        if (count > (s32)(sizeof(line_ids) / sizeof(line_ids[0])))
            count = sizeof(line_ids) / sizeof(line_ids[0]);
        mpCollisionGetLineIDsTypeCount(type, count, line_ids);

        for (i = 0; i < count; i++)
        {
            uint32_t col;
            u16 flags;
            Vec3f a, b;

            /* A line whose yakumono the stage has turned off is not there
             * any more, and every mpCollisionGet* on it spins in
             * `while (TRUE)` until the abort shim fires
             * (mp/mpcollision.c:3077, "mpGetNbVertex() no collision").
             * The game's own callers -- ftshadow.c and mpprocess.c --
             * reach those functions only after mpCollisionCheckExistLineID
             * says the line exists, and the LOG is what names this one as
             * the exception: both of those routes pass through a function
             * carrying the SAME guard with a message of its own
             * (`mpGetLREdge() no collision`, `mpGetLRCommon() no
             * collision`), and neither message appeared. So the caller is
             * this overlay, the one caller that asks
             * mpCollisionGetVertexFlagsLineID first -- and that function
             * has no status guard at all.
             *
             * Yoshi's Island's evaporating cloud, Mushroom Kingdom's
             * falling scales and Sector Z's arwing all turn a line off for
             * real, so skipping the line would kill a match the first time one
             * of those hazards fired on the target.
             *
             * The overlay is an instrument, so a line that has gone away
             * is a line not to draw, and it says so once per line: a run
             * where a hazard fires then leaves a record of which line was
             * skipped rather than looking like a stage whose floor quietly
             * moved.
             *
             * The range check is not redundant with the one below it:
             * mpCollisionCheckExistLineID SPINS on a -1 rather than
             * answering it, so an id that far out has to be caught before
             * that question is asked. An id outside the vertex-info table
             * is the stale-table case this overlay is the only reader of
             * -- gMPCollisionLinesNum is what mpCollisionAllocVertexInfo
             * sized that table by. */
            if (line_ids[i] < 0 || line_ids[i] >= gMPCollisionLinesNum ||
                mpCollisionCheckExistLineID(line_ids[i]) == FALSE)
            {
                db_note_vanish(line_ids[i]);

                continue;
            }
            flags = mpCollisionGetVertexFlagsLineID(line_ids[i]);

            switch (type)
            {
            case nMPLineKindFloor:
                if (flags & MAP_VERTEX_COLL_CLIFF)
                    col = 0xFFFFFFFF;
                else if (flags & MAP_VERTEX_COLL_PASS)
                    col = 0xFF00E0E0;
                else
                    col = 0xFF00E000;
                break;
            case nMPLineKindCeil:
                col = 0xFFE0E000;
                break;
            default:
                col = 0xFFE00000;
                break;
            }
            nvert = mpCollisionGetVertexCountLineID(line_ids[i]);
            mpCollisionGetVertexPositionID(line_ids[i], 0, &a);
            for (v = 1; v < nvert; v++)
            {
                mpCollisionGetVertexPositionID(line_ids[i], v, &b);
                draw_line(a.x, a.y, b.x, b.y, col);
                a = b;
            }
        }
    }
}

/* Every fighter's collision diamond, at its current position: the four
 * corners MPObjectColl names (top, center +/- width, bottom). */
static void draw_map_coll(void)
{
    GObj *fighter_gobj;

    for (fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];
         fighter_gobj != NULL; fighter_gobj = fighter_gobj->link_next)
    {
        const FTStruct *fp = ftGetStruct(fighter_gobj);
        const MPObjectColl *mc = &fp->coll_data.map_coll;
        float x = DObjGetStruct(fighter_gobj)->translate.vec.f.x, y = DObjGetStruct(fighter_gobj)->translate.vec.f.y;
        uint32_t col = (fp->ga == nMPKineticsGround) ? 0xFFFF8000 : 0xFF8080FF;

        draw_line(x, y + mc->top, x + mc->width, y + mc->center, col);
        draw_line(x + mc->width, y + mc->center, x, y + mc->bottom, col);
        draw_line(x, y + mc->bottom, x - mc->width, y + mc->center, col);
        draw_line(x - mc->width, y + mc->center, x, y + mc->top, col);
    }
}
#endif /* DB_COLLISION_OVERLAY */

/* ---- -DDB_CLIFF_WARP: jump to a grabbable ledge ---------------------
 *
 * OFF unless the build asks for it, because the button it is on is a
 * button the game plays with. It is on C-up, and ftcommon.c's FT_JUMP_BUTTONS names all four C
 * buttons, so a build with it on jumps AND teleports on every press: on
 * the Dreamcast pad C-up is Y (input.c's kPadMap). Nothing else in the port
 * reads a real pad for a debug purpose -- gSYControllerMain appears
 * outside input.c only here -- so a build without this define has no
 * debug buttons at all, which is what a build you actually play wants.
 *
 * The game marks a ledge with MAP_VERTEX_COLL_CLIFF (mp/mpdef.h:19) on
 * the end vertex of a floor line, and it is sparing with them: Hyrule
 * has exactly two, at (-2676, 623) and (4601, 8), neither of them the
 * main deck's edge -- walk off that and you teeter, as on the N64. Both
 * are a long fall from the spawn, so a press drops the fighter just
 * outside the next one, airborne and facing the stage, and the ledge
 * states are one button away. Nothing in the game does this.
 *
 * Only the right one is reachable, and that is the N64's behaviour too
 * (checked on hardware). mpProcessCheckTestRCliffCollision sweeps a
 * probe point 400 units inboard and 360 up (Mario's cliffcatch_coll)
 * and wants it to cross the flagged floor; the left vertex sits
 * directly under the main deck's own left end, so the band the probe
 * would have to fall through is roofed by that deck, and the fighter
 * lands or falls past before the probe ever crosses line 5. */
#ifdef DB_CLIFF_WARP
static void warp_to_cliff(Stage *st, int which)
{
    const MPVertexData *vpos = st->geo.vertex_data;
    const uint16_t *vid = st->geo.vertex_id;
    const MPVertexLinks *links = st->geo.vertex_links;
    int yak, found = 0;

    if (db_fighter_gobj(0) == NULL)
        return;
    for (yak = 0; yak < (int)st->geo.yakumono_count; yak++)
    {
        const MPLineData *floors = &st->geo.line_info[yak].line_data[0];
        int l;

        for (l = 0; l < (int)floors->line_count; l++)
        {
            int id = floors->group_id + l;
            const MPVertexLinks *lk = &links[id];
            const MPVertexData *a = &vpos[vid[lk->vertex1]];
            const MPVertexData *b = &vpos[vid[lk->vertex1 +
                                              lk->vertex2 - 1]];
            const MPVertexData *right = (a->pos.x >= b->pos.x) ? a : b;
            const MPVertexData *left = (a->pos.x >= b->pos.x) ? b : a;
            const MPVertexData *v;
            int side;

            /* a ledge is approached from outside its own end of the
             * line: side +1 means stand to the right of it */
            if (right->vertex_flags & MAP_VERTEX_COLL_CLIFF)
                v = right, side = +1;
            else if (left->vertex_flags & MAP_VERTEX_COLL_CLIFF)
                v = left, side = -1;
            else
                continue;
            if (found++ != which)
                continue;

            /* facing first: ftMainSetStatus is what writes it onto
             * the TopN joint (ft/ftmain.c:4477), and the respawn's
             * settle sets a status. */
            DB_P1_FP()->lr = -side;
            ftMainRespawn(DB_P1_FP()->fighter_gobj,
                          (float)v->pos.x + side * 140.0f,
                          (float)v->pos.y + 260.0f);
            dbglog(DBG_INFO,
                   "db: warp to cliff %d on line %d at (%d, %d), "
                   "approaching from the %s\n",
                   which, id, v->pos.x, v->pos.y,
                   (side > 0) ? "right" : "left");
            return;
        }
    }
    if (found)
        warp_to_cliff(st, which % found);
}
#endif /* DB_CLIFF_WARP */

/* The DL link the overlay draws on.  The battle camera's grouping
 * (gm/gmcamera.c:1057-1101 gmCameraDefaultProcDisplay) captures 2|1,
 * then 4, then 12..6, then 15..13, 18..16, 20..19: background first,
 * fighters in the middle, HUD last.  20 is the last of them, so the
 * overlay draws over everything, and db_battle_start adds it to the
 * camera's mask because nothing else would. */
#ifdef DB_COLLISION_OVERLAY
#define DB_DLLINK_OVERLAY  20
#endif

/* ---- the scripted pad ------------------------------------------------
 *
 * The stand-in for pad 1. Most desks
 * have one pad, and P2 on port 1 would stand at his spawn forever; this
 * feeds port 1 a pattern instead when no pad is there: walk toward P1
 * until within a jab's reach, then jab, so a hit can be watched and heard
 * on target -- and so P1 can be hit. A real pad on port 1 (two connected)
 * takes over. Nothing of the game's: ft/ftcomputer.c is the game's CPU, and this
 * is 20 lines of debug facility.
 *
 * DB_JAB_PERIOD_FRAMES sets the cadence: the stand-in presses A every
 * that many frames once in range, with at least one frame of release
 * before each press -- sy_input's tap is an edge (input.h: "tap = press
 * edges"), so a button held across two consecutive feeds is one edge,
 * not two; a period below 1 clamps to 1 (a press every other frame, 30
 * taps/sec at 60 Hz, the fastest a press/release cycle can register).
 * Overridden from the environment (DB_JAB_PERIOD) for a target that
 * passes one through; many jabs/sec makes frame time climb, BGM stutter,
 * and eventually overflows the scene heap. */
#ifndef DB_JAB_PERIOD_FRAMES
#define DB_JAB_PERIOD_FRAMES 40
#endif

/* A Dreamcast port with nothing in it reads as an empty
 * port rather than shifting the pads after it down (src/dc/input.c), so
 * a run with four pads enabled has a real device on port 1 and the
 * stand-in below stands aside -- and the chain, which nothing else
 * walks, stops at the title. DB_SCRIPT_FORCE=1 makes it feed port 1
 * anyway, which is how the four-pad runs are driven. Debug-only, and
 * a lie about a pad that is really there: never on a build a person
 * plays. */
#ifndef DB_SCRIPT_FORCE
#define DB_SCRIPT_FORCE 0
#endif

static int gArrowScript;
static int gDownBScript;
static int gShieldScript;
static int gRollScript;
static int gDashScript;
static int gAppealScript;
static int gBreakScript;
static int gDashGrabScript;
static int gGrabScript;
static int gTumbleScript;
static int gTumbleLaunched;
static int gTechScript;
static int gTechLaunched;
static int gWallScript;
static int gWallLaunched;
static int gWallSlammed;

/* status_vars is a union of per-status structs (ftcommon.h's
 * FTCommonStatusVars): common.guard.effect_gobj is the guard's own GObj
 * ONLY while the fighter is in a guard status. The shield probe below
 * dereferences it, and a CPU P2 knocked out of guard (P1's knockback,
 * unattended) leaves the union holding that status's bytes -- a garbage
 * "pointer" (0xffffffc5, read live by sh-elf-gdb over dcload) that
 * DObjGetStruct faults on. So ask the status before reading a union arm
 * the status does not own. */
static int db_in_status(const FTStruct *fp, int a, int b, int c, int d)
{
    return fp->status_id == a || fp->status_id == b ||
           fp->status_id == c || fp->status_id == d;
}

static int db_is_guard_status(const FTStruct *fp)
{
    return db_in_status(fp, nFTCommonStatusGuardOn, nFTCommonStatusGuard,
                        nFTCommonStatusGuardOff, nFTCommonStatusGuardSetOff);
}

static void db_feed_pad1(void)
{
    static int jab_timer;
    static int period = -1;
    const FTStruct *p1, *p2;
    float dx;
    uint16_t btn = 0;
    int8_t sx = 0;

    if (period < 0)
    {
        const char *env = getenv("DB_JAB_PERIOD");
        period = (env != NULL) ? atoi(env) : DB_JAB_PERIOD_FRAMES;
        if (period < 1)
            period = 1;
    }

    /* A real pad on port 1 takes over. Asking sy_input_port_present and
     * not gSYControllerConnectedNum matters: this function's own feed
     * clears port 1's read error, so the count says "two connected" on
     * every tic after one it fed. Asking the count would make the stand-in
     * feed on alternate tics, so P2 would walk at half speed and jab at
     * half the rate DB_JAB_PERIOD names. */
    if (!DB_SCRIPT_FORCE && sy_input_port_present(1))
    {
        return;
    }
#ifdef DB_1P_SOAK
    /* -DDB_1P_SOAK: an unattended ladder. A on port 1 for one tic in
     * every sixty, whatever the scene, so the ladder's button waits go
     * on by themselves -- the VS card, Stage Clear's tally and its exit
     * (sc1PStageClearFuncRun), the continue screen. They all take a
     * press from any pad (scSubsysControllerGetPlayerTapButtons). Ahead
     * of the ladder arm below because Stage Clear does not read as a
     * ladder scene from out here: it sat at 59.8 fps, tally done, for
     * twelve minutes with the tap placed there. Port 1 is no player on
     * a rung, and A pauses nothing, where START would (below). */
    sy_input_feed(1, ((dSYTaskmanUpdateCount % 60) == 0) ? N64_A : 0, 0, 0);
    return;
#endif
    /* The 1P ladder gets NOTHING on port 1, and it has to
     * be said HERE, above every arm below, because of a sentinel that
     * makes a rung look like the title screen.
     *
     * sc1PIntroFuncRun (src/dc/sc1pintro.c:1992-1993 and 2009-2010,
     * decomp 0x801348F4) ends the VS card with
     *
     *     scene_prev = scene_curr;      // nSCKind1PIntro
     *     scene_curr = nSCKindTitle;
     *
     * on both its exits, the timeout as well as the button. It does not
     * mean "go to the title": sc1PManager is a blocking driver that
     * calls each scene's StartScene inline, so while the ladder runs the
     * scene kind is only what the last scene left behind, and the card
     * leaves nSCKindTitle. Every rung, every stage-clear screen and
     * every bonus stage after the first card therefore reads as
     * nSCKindTitle from out here -- which is why the title arm below
     * tapped START at port 1 through a whole Giant DK probe, twenty-six
     * pauses in twelve hundred frames, the pause menu eating the match
     * and "2P PAUSE" sitting over Congo Jungle in every screenshot. The
     * first tap landed on frame 601 and not frame 1 only because of
     * that arm's own ten-second wait.
     *
     * The pair is the tell: scene_prev is nSCKind1PIntro and nothing in
     * sc1pgame.c or sc1pbonusstage.c writes scene_prev, so it stands for
     * the whole rung. The real title screen is never reached with it.
     *
     * Quiet and not scripted, because the VS scripts below all assume
     * players[1] is the stand-in's own fighter facing the human; on the
     * Mario Bros. and Giant DK rungs that slot is an ALLY the router
     * shuffled in, on the human's side. */
    if ((gSCManagerSceneData.scene_prev == nSCKind1PIntro &&
         gSCManagerSceneData.scene_curr == nSCKindTitle) ||
        gSCManagerSceneData.scene_curr == nSCKind1PIntro ||
        gSCManagerSceneData.scene_curr == nSCKind1PGame ||
        gSCManagerSceneData.scene_curr == nSCKind1PBonusStage)
    {
        sy_input_feed(1, 0, 0, 0);
        return;
    }
    /* The results screen leaves on a START press from any pad
     * (mnVSResultsCheckExit), and its own 370-tic wait decides when one
     * counts; the title likewise (mnTitleFuncRun), once its clock has
     * shown PRESS START at tic 280. Tap it every other tic so an edge is
     * always there -- sy_input's tap is an edge, so a held button is one
     * press. A human with a pad would do this; an unattended run has to, or
     * the port sits on the results screen forever. */
    if (gSCManagerSceneData.scene_curr == nSCKindVSResults ||
        gSCManagerSceneData.scene_curr == nSCKindTitle)
    {
        /* on the title, not before ten seconds: the game accepts a press
         * from tic 280, and a stand-in that presses at once would leave a
         * screen nobody could look at */
        u16 btn = (dSYTaskmanUpdateCount & 1) ? N64_START : 0;
#ifdef DB_SOAK
        /* db_soak_results ends the results screen itself; a START here
         * would send the chain through the character select instead */
        if (gSCManagerSceneData.scene_curr == nSCKindVSResults)
        {
            btn = 0;
        }
#endif

        if (gSCManagerSceneData.scene_curr == nSCKindTitle &&
            dSYTaskmanUpdateCount < 600)
        {
            btn = 0;
        }
        sy_input_feed(1, btn, 0, 0);
        return;
    }
    /* The mode select waits for a choice, and the choice this port
     * has a scene for is VS MODE, one down from where the cursor starts
     * (mnModeSelectInitVars, coming from the title). Two seconds to
     * look, one tic of stick down, a second more, then A. A human with
     * a pad does the same; without this the port sits on the menu for
     * the five minutes the game allows and goes back to the title. */
    if (gSCManagerSceneData.scene_curr == nSCKindModeSelect)
    {
        u16 btn = (dSYTaskmanUpdateCount == 180) ? N64_A : 0;
        int8_t sy = (dSYTaskmanUpdateCount == 120) ? -80 : 0;

        sy_input_feed(1, btn, 0, sy);
        return;
    }
    /* The VS mode is walked twice. Coming from the mode select, two
     * seconds to look and then down three times to VS OPTIONS and A,
     * which is the only way to that screen. Coming back from it the
     * cursor is on VS OPTIONS already (mnVSModeFuncStartVars reads
     * scene_prev), so the second pass is the original one: up once to
     * TIME/STOCK, one stock up and one back down (the count remade each
     * time, the arrows blinking beside it), up twice to VS START, and
     * A. The rule is left alone: stepping it through a team rule
     * re-deals the costumes, and the port draws Mario in one. */
    if (gSCManagerSceneData.scene_curr == nSCKindVSMode)
    {
        u32 tic = dSYTaskmanUpdateCount;

        if (gSCManagerSceneData.scene_prev != nSCKindVSOptions)
        {
            u16 btn = (tic == 300) ? N64_A : 0;
            int8_t sy = (tic == 120 || tic == 180 || tic == 240) ? -80 : 0;

            sy_input_feed(1, btn, 0, sy);
            return;
        }
        {
            u16 btn = (tic == 420) ? N64_A : 0;
            int8_t sx = (tic == 240) ? 80 : (tic == 300) ? -80 : 0;
            int8_t sy = (tic == 120 || tic == 360 || tic == 390) ? 80 : 0;

            sy_input_feed(1, btn, sx, sy);
            return;
        }
    }
    /* The VS options: the cursor starts on HANDICAP, so A three times
     * takes it ON, AUTO and back to OFF -- the red word and the bar
     * under it moving each time, and every player's handicap written
     * with it -- then down to TEAM ATTACK and A twice for the same, two
     * more downs past STAGE SELECT (left alone: it decides whether the
     * chain shows the stage select at all) to DAMAGE, the ratio stepped
     * up and back, and B to save and return. */
    if (gSCManagerSceneData.scene_curr == nSCKindVSOptions)
    {
        u32 tic = dSYTaskmanUpdateCount;
        u16 btn = (tic == 120 || tic == 180 || tic == 240 ||
                   tic == 360 || tic == 420) ? N64_A :
                  (tic == 660) ? N64_B : 0;
        int8_t sx = (tic == 570) ? 80 : (tic == 600) ? -80 : 0;
        int8_t sy = (tic == 300 || tic == 480 || tic == 510) ? -80 : 0;

        sy_input_feed(1, btn, sx, sy);
        return;
    }
    /* The character select comes up with 2P's last pick on its gate
     * (the battle state below is a pick) and 1P's puck in hand. The
     * stand-in is P2's pad: five seconds to look, then B to recall its
     * puck (the gate empties), the stick left and up to carry the puck
     * from below the gates onto Mario's portrait (the second of the top
     * row; four pixels a tic), A to drop it (the announcer says the
     * name); then 1P's pick below, and START.
     *
     * Between the drop and START, P2 taps the D-pad right, down, left
     * and up, put through input.c's remap the way sy_input_poll puts a
     * real pad's: on a character select each direction is the C-button
     * on that side, so Mario steps through costumes 1, 2, 3 and back to
     * 0, and the log says which one the slot holds after each tap. */
    if (gSCManagerSceneData.scene_curr == nSCKindPlayersVS)
    {
        extern s32 mnPlayersVSGetCostume(s32 player);
        u32 tic = dSYTaskmanUpdateCount;
        u16 btn = (tic == 300) ? N64_B : (tic == 420) ? N64_A : (tic == 720) ? N64_START : 0;
        u16 dpad = (tic == 450) ? N64_J_RIGHT : (tic == 480) ? N64_J_DOWN :
                   (tic == 510) ? N64_J_LEFT : (tic == 540) ? N64_J_UP : 0;
        int8_t sx = (tic >= 340 && tic < 353) ? -80 : 0;
        int8_t sy = (tic >= 353 && tic < 381) ? 80 : 0;

        if (tic == 440 || tic == 460 || tic == 490 || tic == 520 || tic == 550)
        {
            dbglog(DBG_INFO, "db: dpad costume tic %lu: P2 costume %d\n",
                   (unsigned long)tic, (int)mnPlayersVSGetCostume(1));
        }
        btn |= sy_input_dpad_remap(dpad,
            sy_input_dpad_context(gSCManagerSceneData.scene_curr, -1));
        sy_input_feed(1, btn, sx, sy);
        /* ...and 1P's, which a direct boot leaves with its puck in hand
         * below its own panel -- READY TO FIGHT waits for it. Port 0
         * for the reason the bonus select's arm below gives: only a
         * DB_BOOT_SCENE boot installs this. Right and up onto DK's
         * portrait (the third of the top row), A to drop it, and the
         * START above once both have picked. */
        sy_input_feed(0, (tic == 640) ? N64_A : 0,
                      (tic >= 560 && tic < 580) ? 80 : 0,
                      (tic >= 580 && tic < 613) ? 80 : 0);
        return;
    }
    /* The stage select comes up on the stage the VS chain last used,
     * which is Hyrule below. Two seconds to look at it, one tic of
     * stick left -- the cursor steps to Congo Jungle, the name, the
     * emblem and the preview are remade for it -- three more seconds,
     * then A. That the battle after it plays on Congo Jungle and not on
     * Hyrule is the whole point of the scene, and the log line the
     * battle prints names the stage it bound. */
    if (gSCManagerSceneData.scene_curr == nSCKindMaps)
    {
        u32 tic = dSYTaskmanUpdateCount;
        u16 btn = (tic == 300) ? N64_A : 0;
        int8_t sx = (tic == 120) ? -80 : 0;

        sy_input_feed(1, btn, sx, 0);
        return;
    }
    /* The bonus practice select, and the one arm here
     * that feeds PORT 0 rather than port 1. The other selects have four
     * slots and the stand-in drives the second of them; this one has a
     * single slot and it belongs to gSCManagerSceneData.player, which a
     * direct boot leaves at 0 -- so there is no second pad to be. That
     * is safe only because this whole block is installed only on a
     * DB_BOOT_SCENE boot: a plain build never reaches it, and an
     * unattended probe's real port-0 pad is sitting still anyway.
     *
     * The cursor comes up holding the puck below the grid
     * (mnPlayers1PBonusResetPlayer leaves held_player set). Four seconds
     * to look at the empty screen, then the stick held up for 28 tics
     * -- mnPlayers1PBonusAdjustCursor moves stick_range.y / -20 a tic,
     * so a full stick is four pixels and 28 of them carry the cursor
     * from 170 to 58, which puts the puck on the top row of the grid
     * (mnPlayers1PBonusGetForcePuckFighterKind wants 35 < y < 79) in
     * the second column, Mario's. The records panel beside the grid is
     * redrawn for whoever the puck is over, every tic
     * (mnPlayers1PBonusMakeHiScore). A at tic 360 drops the puck: the
     * announcer says the name and the fighter stands on the gate. */
    if (gSCManagerSceneData.scene_curr == nSCKind1PBonus1Players ||
        gSCManagerSceneData.scene_curr == nSCKind1PBonus2Players)
    {
        u32 tic = dSYTaskmanUpdateCount;
        u16 btn = (tic == 360) ? N64_A : 0;
        int8_t sy = (tic >= 240 && tic < 268) ? 80 : 0;

        sy_input_feed(0, btn, 0, sy);
        return;
    }
    /* Feeding zeros and not just returning, because a port this
     * function walks away from is FROZEN, not quiet: sy_input_poll
     * marks an empty port as an error every tic, sy_input_update skips
     * an errored port, and gSYControllerDevices[1] therefore keeps the
     * last button_tap this function ever wrote -- forever. That is what
     * cycled "2P PAUSE" on and off through a whole rung probe: one
     * latched N64_START from an earlier scene's script, read as a fresh
     * tap by ifCommonBattlePauseCheckPause every tic it was allowed to
     * pause. */
    if (gSCManagerBattleState->game_status != nSCBattleGameStatusGo ||
        db_fighter_gobj(0) == NULL ||
        db_fighter_gobj(1) == NULL)
    {
        sy_input_feed(1, 0, 0, 0);
        return;
    }
    /* One scripted Down-B, once the arrow script has
     * finished with the first battle. Port 1 is P2, and P2 is Mario
     * (kBootKinds below), so this is the Tornado: one tic of B with the
     * stick down, from a fighter standing still. Everything before this
     * step made that input a crouch. */
    if (gArrowScript > 130 && gDownBScript <= 90)
    {
        gDownBScript++;

        if (gDownBScript == 30)
        {
            sy_input_feed(1, N64_B, 0, -80);
            return;
        }
        sy_input_feed(1, 0, 0, 0);
        return;
    }
    p1 = ftGetStruct(db_fighter_gobj(0));
    p2 = ftGetStruct(db_fighter_gobj(1));
    /* the shield: once the Down-B is done, hold Z for 120
     * tics and let go, logging what the guard makes of it -- GuardOn
     * (152) into Guard (153), YRotN scaled to the shield's size and
     * shrinking with its health, GuardOff (154) on release, and shield
     * stun (155) if P1's jab lands on it meanwhile */
    if (gDownBScript > 90 && gShieldScript <= 220)
    {
        gShieldScript++;

        if (gShieldScript == 2 || gShieldScript == 20 || gShieldScript == 60 ||
            gShieldScript == 119 || gShieldScript == 125 || gShieldScript == 150)
        {
            const DObj *yrotn = p2->joints[nFTPartsJointYRotN];
            /* GuardOn and Guard only: from GuardOff on the effect has been
             * ejected and the union member is a dangling pointer, which
             * zeroed RAM leaves harmless and a console does not
             * (a read through it froze the machine at tic 612 of seed
             * 1234, silently, in three builds) */
            int guard = db_in_status(p2, nFTCommonStatusGuardOn,
                                     nFTCommonStatusGuard, -1, -1);
            const GObj *bubble = guard
                ? p2->status_vars.common.guard.effect_gobj : NULL;
            const DObj *bd = (bubble != NULL) ? DObjGetStruct(bubble) : NULL;

            /* the bubble: the effect the guard attached,
             * its root's matrix kinds (0x4F on the root, 0x2C under it
             * for every fighter but Yoshi, whose egg is 0x50 and 0x2C on
             * the one DObj), and whether the root follows YRotN */
            dbglog(DBG_INFO, "db: guard tic %d: P2 status %d shield %d health "
                             "%d decay %d yrotn %s scale %.2f bubble %s "
                             "kinds %02X/%02X on %s attach %d\n",
                   gShieldScript, (int)p2->status_id, (int)p2->is_shield,
                   (int)p2->shield_health,
                   guard ? (int)p2->status_vars.common.guard.shield_decay_wait
                         : -1,
                   yrotn ? "linked" : "none",
                   yrotn ? yrotn->scale.vec.f.x : 0.0f,
                   bubble ? "up" : "none",
                   (bd != NULL && bd->xobjs_num > 0) ? bd->xobjs[0]->kind : 0,
                   (bd != NULL && bd->xobjs_num > 1) ? bd->xobjs[1]->kind : 0,
                   (bd != NULL && bd->user_data.p == yrotn && yrotn != NULL)
                       ? "yrotn" : "-",
                   (int)p2->is_effect_attach);
        }
        sy_input_feed(1, gShieldScript <= 120 ? N64_Z : 0, 0, 0);
        return;
    }
    /* the roll: once the shield script is done, put the
     * shield up again and tap the stick past 56 with Z still held --
     * ftCommonEscapeCheckInterruptGuard, the hole the shield's cascade
     * carries. What is logged is the status (156 EscapeF or
     * 157 EscapeB), whether the shield and its bubble went with it, the
     * TransN joint the roll's animation moves the fighter with, and how
     * far the fighter actually travelled. */
    if (gShieldScript > 220 && gRollScript <= 120)
    {
        gRollScript++;

        if (gRollScript == 20 || gRollScript == 29 || gRollScript == 31 ||
            gRollScript == 34 || gRollScript == 40 || gRollScript == 50 ||
            gRollScript == 70 || gRollScript == 100)
        {
            const DObj *transn = p2->joints[nFTPartsJointTransN];

            dbglog(DBG_INFO, "db: roll tic %d: P2 status %d %s lr %d x %.1f "
                             "vel %.2f transn %s shield %d bubble %s "
                             "attach %d buffer %d\n",
                   gRollScript, (int)p2->status_id,
                   ftMainStatusName(p2->status_id), (int)p2->lr,
                   DObjGetStruct(p2->fighter_gobj)->translate.vec.f.x,
                   p2->physics.vel_ground.x, transn ? "linked" : "none",
                   (int)p2->is_shield,
                   (db_is_guard_status(p2) &&
                    p2->status_vars.common.guard.effect_gobj != NULL) ? "up"
                                                                     : "none",
                   (int)p2->is_effect_attach,
                   (p2->status_id == nFTCommonStatusEscapeF ||
                    p2->status_id == nFTCommonStatusEscapeB)
                       ? (int)p2->status_vars.common.escape
                             .itemthrow_buffer_tics : -1);
        }
        /* Z from tic 2 to 60, and the stick tapped right at tic 30 --
         * a fresh crossing, which is what makes it a roll and not a
         * lean (ftCommonEscapeGetStatus reads tap_stick_x) */
        sy_input_feed(1, (gRollScript >= 2 && gRollScript <= 60) ? N64_Z : 0,
                      (gRollScript >= 30 && gRollScript <= 33) ? 80 : 0, 0);
        return;
    }
    /* the dash and the run: the stick tapped past 56 is a
     * dash, held it becomes a run at the fighter's dash_to_run frame,
     * let go it is RunBrake into Wait, and pushed back it is TurnRun.
     * What is logged is the status, the ground velocity against the
     * fighter's own dash_speed (54 for Mario) and run_speed (44), the
     * facing, and where the fighter got to. */
    if (gRollScript > 120 && gDashScript <= 130)
    {
        gDashScript++;

        if (gDashScript == 5 || gDashScript == 10 || gDashScript == 16 ||
            gDashScript == 22 || gDashScript == 24 || gDashScript == 27 ||
            gDashScript == 32 || gDashScript == 38 || gDashScript == 42 ||
            gDashScript == 50 || gDashScript == 70)
        {
            dbglog(DBG_INFO, "db: dash tic %d: P2 status %d %s lr %d x %.1f "
                             "vel %.2f frame %.1f\n",
                   gDashScript, (int)p2->status_id,
                   ftMainStatusName(p2->status_id), (int)p2->lr,
                   DObjGetStruct(p2->fighter_gobj)->translate.vec.f.x,
                   p2->physics.vel_ground.x,
                   p2->fighter_gobj->anim_frame);
        }
        if (gDashScript >= 4 && gDashScript <= 24)
        {
            sy_input_feed(1, 0, 80, 0);     /* dash, then run */
        }
        else if (gDashScript >= 25 && gDashScript <= 38)
        {
            sy_input_feed(1, 0, -80, 0);    /* pushed back: TurnRun */
        }
        else sy_input_feed(1, 0, 0, 0);     /* let go: RunBrake, Wait */
        return;
    }
    /* the taunt: one L tap out of Wait is the taunt, status
     * 189, and it ends in Wait when the animation runs out. The input
     * read edge-detects the tap from the button-hold transition
     * (src/dc/ftcommon.c:7397), so L is fed for a single tic -- as a
     * D-pad tap put through input.c's remap, which is how a Dreamcast
     * pad, with no L, taunts. */
    if (gDashScript > 130 && gAppealScript <= 60)
    {
        gAppealScript++;

        if (gAppealScript == 4 || gAppealScript == 6 || gAppealScript == 8 ||
            gAppealScript == 12 || gAppealScript == 20 || gAppealScript == 30 ||
            gAppealScript == 50)
        {
            dbglog(DBG_INFO, "db: appeal tic %d: P2 status %d %s frame %.1f\n",
                   gAppealScript, (int)p2->status_id,
                   ftMainStatusName(p2->status_id),
                   p2->fighter_gobj->anim_frame);
        }
        sy_input_feed(1, (gAppealScript == 5) ?
            sy_input_dpad_remap(N64_J_DOWN,
                sy_input_dpad_context(gSCManagerSceneData.scene_curr,
                                      gSCManagerBattleState->game_status)) :
            0, 0, 0);
        return;
    }
    /* the shield break: once the taunt is done, put the
     * shield up and cap its health at a few points so it decays to zero
     * within the probe; ftCommonGuardOnProcUpdate breaks it at zero and
     * the fighter runs the five-status chain -- ShieldBreakFly (158) up,
     * Fall (159), Down (160/161) on landing, Stand (162/163), FuraFura
     * (164) the dizzy -- and back to a ground status once A mashes the
     * breakout counter out. */
    if (gAppealScript > 60 && gBreakScript <= 400)
    {
        int in_break = (p2->status_id >= nFTCommonStatusShieldBreakFly &&
                        p2->status_id <= nFTCommonStatusFuraFura);
        u16 btn2 = 0;

        gBreakScript++;

        if ((p2->status_id == nFTCommonStatusGuardOn ||
             p2->status_id == nFTCommonStatusGuard) && p2->shield_health > 3)
        {
            /* probe-only: cap the shield low so it decays to zero in a
             * couple of seconds rather than the ~15 a full 55 would take.
             * The break itself is the game's, off the guard proc. */
            ((FTStruct *)p2)->shield_health = 3;
        }
        /* the transient chain (Fly/Fall/Down/Stand) lasts a few tics each,
         * so log every tic it is up, plus a coarse heartbeat around it */
        if (in_break || gBreakScript == 2 || (gBreakScript % 40) == 0)
        {
            dbglog(DBG_INFO, "db: break tic %d: P2 status %d %s health %d "
                             "breakout %d ga %d frame %.1f\n",
                   gBreakScript, (int)p2->status_id,
                   ftMainStatusName(p2->status_id), (int)p2->shield_health,
                   (int)p2->breakout_wait, (int)p2->ga,
                   p2->fighter_gobj->anim_frame);
        }
        if (p2->status_id == nFTCommonStatusFuraFura)
        {
            btn2 = (gBreakScript & 1) ? N64_A : 0;  /* mash out of the dizzy */
        }
        else if (!in_break && p2->ga == nMPKineticsGround)
        {
            btn2 = N64_Z;               /* raise and hold the shield */
        }
        sy_input_feed(1, btn2, 0, 0);
        return;
    }
    /* the grab, the connect and the throw: P2 walks up to
     * P1 and grabs (Z held, A tapped, one clean edge from a grounded
     * non-catch state). The catch search finds P1 -- is_catchstatus goes 1,
     * the catcher pulls P1 into CapturePulled then holds it in CaptureWait
     * (P1.capture_gobj = P2), while P2 goes CatchPull -> CatchWait. Then the
     * hold's throw_wait runs out and CatchWait's interrupt fires the throw:
     * P2 -> ThrowF, P1 -> ThrownCommon, and on the figatree's release frame
     * P1 is launched with the throw's knockback and percent (its real attr's
     * throw_desc, live off the disc). The log follows all three phases. */
    /* the dash and run grabs: before the forced-connect throw
     * demonstration below, drive P2 through a natural dash- and run-grab off
     * the pad -- a fresh full-stick flick toward P1 dashes (then, held, runs),
     * and Z held + A tapped now reaches ftCommonCatchCheckInterruptDashRun in
     * the Dash's second window and at the head of the Run interrupt, entering
     * Catch. Unlike the throw phase this is the natural path (no forced
     * connect): the log shows Dash/Run -> Catch. */
    if (gBreakScript > 400 && gDashGrabScript <= 140)
    {
        float ddx = DObjGetStruct(p1->fighter_gobj)->translate.vec.f.x -
                    DObjGetStruct(p2->fighter_gobj)->translate.vec.f.x;
        int8_t toward = (ddx >= 0.0f) ? 80 : -80;
        /* first stretch: chase P1 and hold the grab until the dash has become
         * a run, so the grab comes through the Run interrupt; second stretch:
         * dash a fixed direction across open stage (not chasing, so the dash
         * sustains) and grab in the dash's second window (anim past 5),
         * through the Dash interrupt. anim_frame is the GObj's, the frame the
         * interrupts read. */
        int want_run_grab = (gDashGrabScript < 70);
        int8_t sx2 = want_run_grab ? toward : 80;
        u16 btn2 = 0;

        gDashGrabScript++;

        if (want_run_grab && p2->status_id == nFTCommonStatusRun)
        {
            btn2 = N64_Z | N64_A;   /* the run grab */
        }
        else if (!want_run_grab && p2->status_id == nFTCommonStatusDash &&
                 p2->fighter_gobj->anim_frame > 5.0f)
        {
            btn2 = N64_Z | N64_A;   /* the dash grab (second window) */
        }
        else if (p2->status_id == nFTCommonStatusCatch)
        {
            sx2 = 0;                /* got it -- let the whiff run its course */
        }

        if (p2->status_id == nFTCommonStatusDash ||
            p2->status_id == nFTCommonStatusRun ||
            p2->status_id == nFTCommonStatusCatch || (gDashGrabScript % 20) == 1)
        {
            dbglog(DBG_INFO, "db: dashgrab tic %d: P2 %s anim %.1f%s | P2x %.0f\n",
                   gDashGrabScript, ftMainStatusName(p2->status_id),
                   p2->fighter_gobj->anim_frame,
                   btn2 ? " Z+A" : "",
                   DObjGetStruct(p2->fighter_gobj)->translate.vec.f.x);
        }
        sy_input_feed(1, btn2, sx2, 0);
        return;
    }

    if (gBreakScript > 400 && gGrabScript <= 300)
    {
        int p1caught = (p1->status_id == nFTCommonStatusCapturePulled ||
                        p1->status_id == nFTCommonStatusCaptureWait);
        int inthrow = (p2->status_id == nFTCommonStatusThrowF ||
                       p2->status_id == nFTCommonStatusThrowB);
        u16 btn2 = 0;

        gGrabScript++;

        /* Enter Catch (Z held, A tapped) and, on the frame P2 is reaching,
         * drive the connect the way ftMainProcSearchCatch does: the search
         * itself is a verbatim port (ftmain.c) and its geometric find is
         * hard to land with a scripted, AI-less pad at an exact distance and
         * frame, so fire proc_catch/proc_capture directly with P1 as the
         * found target. That exercises the ported two-body chain --
         * CatchPull -> CatchWait, CapturePulled -> CaptureWait,
         * and P1 pulled to P2's hand each frame by func_ovl0_800C9A38 /
         * gmCollisionGetWorldPosition on the SH-4. */
        if (p2->status_id == nFTCommonStatusCatch && !p1caught &&
            p2->catch_gobj == NULL)
        {
            /* the probe is otherwise read-only; this one drive mutates, on
             * purpose, to stand in for the search's find */
            FTStruct *p2w = (FTStruct *)p2;

            p2w->search_gobj = p1->fighter_gobj;
            p2w->proc_catch(p2w->fighter_gobj);
            p2w->proc_capture(p2w->search_gobj, p2w->fighter_gobj);
        }
        else if (p2->status_id == nFTCommonStatusWait &&
                 p2->ga == nMPKineticsGround && (gGrabScript % 16) == 0)
        {
            btn2 = N64_Z | N64_A;   /* Z held + A tapped, one edge: the grab */
        }

        if (p1caught || inthrow || (gGrabScript % 12) == 1)
        {
            dbglog(DBG_INFO, "db: grab tic %d: P2 %s catch_gobj %s | P1 %s "
                             "capture_gobj %s throw_wait %d | P1 %d%% | P1x %.0f P2x %.0f\n",
                   gGrabScript,
                   ftMainStatusName(p2->status_id),
                   (p2->catch_gobj != NULL) ? "set" : "null",
                   ftMainStatusName(p1->status_id),
                   (p1->capture_gobj != NULL) ? "set" : "null",
                   (int)p2->status_vars.common.catchwait.throw_wait,
                   (int)p1->percent_damage,
                   DObjGetStruct(p1->fighter_gobj)->translate.vec.f.x,
                   DObjGetStruct(p2->fighter_gobj)->translate.vec.f.x);
        }
        sy_input_feed(1, btn2, 0, 0);
        return;
    }
    /* the Fly/tumble launch and the knockdown floor: once the grab demo is done, deal P2 one launching hit straight
     * into its queued-damage fields and route it through the ported entry
     * (ftCommonDamageGotoDamageStatus), the same drive the host test uses --
     * a real level-3 hit is hard to land with an AI-less scripted pad. This
     * shows the physics-timed chain the host test cannot:
     * DamageFlyN -> (the flinch and hitstun end while airborne) DamageFall,
     * the tumble -> (the floor) DownBounce -> DownWait -> (the 180-frame
     * auto-stand timer) DownStand -> Wait. The window is long enough for the
     * whole knockdown loop (fly/fall + the 180-tic lie-down + get-up). */
    if (gGrabScript > 300 && gTumbleScript <= 400)
    {
        static s32 gLastTumbleStatus = -1;
        FTStruct *p2w = (FTStruct *)p2;

        gTumbleScript++;

        if (!gTumbleLaunched && p2->status_id == nFTCommonStatusWait &&
            p2->ga == nMPKineticsGround)
        {
            p2w->damage_queue = 30;
            p2w->damage_knockback = 90.0f;   /* hitstun 48 -> damage_level 3 */
            p2w->damage_angle = 55;          /* clear of the FlyTop window */
            p2w->damage_lr = -1;
            p2w->damage_index = 1;           /* the N tier */
            p2w->damage_element = nGMHitElementNormal;
            p2w->damage_player_num = 0;
            ftCommonDamageGotoDamageStatus(p2w->fighter_gobj);
            gTumbleLaunched = 1;
            gLastTumbleStatus = -1;
        }
        /* log every status change (the knockdown transitions) plus a slow
         * heartbeat, so the whole chain shows without flooding the lie-down. */
        if (gTumbleLaunched && (p2->status_id != gLastTumbleStatus ||
            (gTumbleScript % 30) == 1))
        {
            dbglog(DBG_INFO, "db: tumble tic %d: P2 %s ga %d frame %.1f | P2 %d%% | P2y %.0f\n",
                   gTumbleScript, ftMainStatusName(p2->status_id), (int)p2->ga,
                   p2->fighter_gobj->anim_frame, (int)p2->percent_damage,
                   DObjGetStruct(p2->fighter_gobj)->translate.vec.f.y);
            gLastTumbleStatus = p2->status_id;
        }
        sy_input_feed(1, 0, 0, 0);
        return;
    }
    /* the ground tech: once the knockdown loop is done, launch
     * P2 one more time and this time buffer a Z press on its pad every other
     * frame (a real tap edge, so ftMain resets tics_since_last_z to 0). When
     * the tumble reaches the floor, ftCommonDamageAirCommonProcMap sees the
     * Z buffered inside FTCOMMON_PASSIVE_BUFFER_TICS_MAX and techs -- with the
     * stick centred that is the neutral tech (Passive, status 81), so P2
     * springs up instead of knocking down: DamageFlyN -> DamageFall ->
     * Passive -> (the tech anim ends) Wait, all through the real dispatch and
     * the new descriptor row, live on SH-4. The F/B roll rows (73/74) share
     * the getups' verified proc set and are covered by the host test. */
    if (gTumbleScript > 400 && gTechScript <= 200)
    {
        static s32 gLastTechStatus = -1;
        FTStruct *p2w = (FTStruct *)p2;

        gTechScript++;

        if (!gTechLaunched && p2->status_id == nFTCommonStatusWait &&
            p2->ga == nMPKineticsGround)
        {
            p2w->damage_queue = 30;
            p2w->damage_knockback = 90.0f;
            p2w->damage_angle = 55;
            p2w->damage_lr = -1;
            p2w->damage_index = 1;
            p2w->damage_element = nGMHitElementNormal;
            p2w->damage_player_num = 0;
            ftCommonDamageGotoDamageStatus(p2w->fighter_gobj);
            gTechLaunched = 1;
            gLastTechStatus = -1;
        }
        if (gTechLaunched && (p2->status_id != gLastTechStatus ||
            (gTechScript % 30) == 1))
        {
            dbglog(DBG_INFO, "db: tech tic %d: P2 %s ga %d z %d frame %.1f | P2y %.0f\n",
                   gTechScript, ftMainStatusName(p2->status_id), (int)p2->ga,
                   (int)p2w->tics_since_last_z,
                   p2->fighter_gobj->anim_frame,
                   DObjGetStruct(p2->fighter_gobj)->translate.vec.f.y);
            gLastTechStatus = p2->status_id;
        }
        /* Z tap on the odd frames (edge each press), stick centred. */
        sy_input_feed(1, (gTechScript & 1) ? N64_Z : 0, 0, 0);
        return;
    }
    /* the wall bounce: once the tech loop is done, launch P2
     * one more time and, when it is airborne in the tumble, hand it a
     * synthetic left-wall collision and drive ftCommonWallDamageCheckGoto
     * directly -- the same "drive the setter" trick the tumble uses above,
     * because steering an AI-less scripted pad into a stage wall at launch
     * speed is not reliable. This shows on SH-4 what the host test cannot:
     * the WallDamage status entered (56), the blue impact wave (efimpactwave.mdl) made and drawn where the slam lands, the quake,
     * the knockback reflected off the wall, and the game's own Proc Update
     * counting the reflected hitstun down and handing back to DamageFall to
     * finish the fall -- DamageFlyN -> WallDamage -> DamageFall, live. */
    if (gTechScript > 200 && gWallScript <= 200)
    {
        static s32 gLastWallStatus = -1;
        FTStruct *p2w = (FTStruct *)p2;

        gWallScript++;

        if (!gWallLaunched && p2->status_id == nFTCommonStatusWait &&
            p2->ga == nMPKineticsGround)
        {
            p2w->damage_queue = 30;
            p2w->damage_knockback = 90.0f;
            p2w->damage_angle = 55;
            p2w->damage_lr = -1;
            p2w->damage_index = 1;
            p2w->damage_element = nGMHitElementNormal;
            p2w->damage_player_num = 0;
            ftCommonDamageGotoDamageStatus(p2w->fighter_gobj);
            gWallLaunched = 1;
            gWallSlammed = 0;
            gLastWallStatus = -1;
        }
        /* airborne in the tumble -> slam into a left wall (normal pointing
         * into the stage) exactly once, through the real CheckGoto. */
        if (gWallLaunched && !gWallSlammed && p2->ga == nMPKineticsAir &&
            (p2->status_id == nFTCommonStatusDamageFlyN ||
             p2->status_id == nFTCommonStatusDamageFall))
        {
            p2w->coll_data.lwall_angle.x = 1.0f;
            p2w->coll_data.lwall_angle.y = 0.0f;
            p2w->coll_data.lwall_angle.z = 0.0f;
            p2w->status_vars.common.damage.coll_mask_curr = MAP_FLAG_LWALL;
            if (ftCommonWallDamageCheckGoto(p2w->fighter_gobj))
            {
                gWallSlammed = 1;
                /* clear the queued launch hit, or the frame chain's
                 * ftMainProcParams re-launches off the stale queue before
                 * the wall bounce's own hitstun hands to DamageFall. */
                p2w->damage_queue = 0;
                p2w->damage_knockback = 0.0f;
            }
        }
        if (gWallLaunched && (p2->status_id != gLastWallStatus ||
            (gWallScript % 30) == 1))
        {
            dbglog(DBG_INFO, "db: wall tic %d: P2 %s ga %d frame %.1f | vdx %.1f vdy %.1f | P2y %.0f\n",
                   gWallScript, ftMainStatusName(p2->status_id), (int)p2->ga,
                   p2->fighter_gobj->anim_frame,
                   p2w->physics.vel_damage_air.x, p2w->physics.vel_damage_air.y,
                   DObjGetStruct(p2->fighter_gobj)->translate.vec.f.y);
            gLastWallStatus = p2->status_id;
        }
        sy_input_feed(1, 0, 0, 0);
        return;
    }
    dx = DObjGetStruct(p1->fighter_gobj)->translate.vec.f.x -
         DObjGetStruct(p2->fighter_gobj)->translate.vec.f.x;

    if (dx > 220.0f || dx < -220.0f)
    {
        sx = (dx > 0.0f) ? 40 : -40;        /* WalkMiddle toward P1 */
        jab_timer = 0;
    }
    else if (++jab_timer > period)
    {
        btn = N64_A;
        jab_timer = 0;
    }
    sy_input_feed(1, btn, sx, 0);
}

/* The game status is logged from the controller read and not from
 * db_battle_run. db_battle_run
 * is a GObj, and a paused tic never runs the object system
 * (src/dc/ifcommon.c) -- so the pause and the unpause were the two
 * transitions the log could not report. The controller read is called
 * every tic whatever the status, which is how the pause menu reads a pad
 * at all. Defined below, beside the status names it prints. */
static void db_log_game_status(void);

/* One frame's triangles, latched and cleared here rather than in
 * db_battle_run; same reason as the status log above:
 * db_battle_run is a GObj, and across a pause the counter would go
 * unreset for every frozen frame and then report the sum -- 92,371 triangles for a frame that drew 832. This
 * runs before the update, so what it latches is the frame the draw at
 * the end of the last tic filled. */
static unsigned gTrisLastFrame;

/* The off-screen arrows/magnify-glass diagnostic line; it no
 * longer moves the fighter to trigger them (that scripted teleport was
 * removed), so this now just logs whatever the real match already has
 * player 1 doing at the counted tics below.
 *
 * The three values it prints are written by the *draw* -- the last
 * frame's, since the controller read runs above the update -- which is
 * the only place they are written at all (src/dc/ftdisplaymain.c). */
static unsigned gTrisMagnifyLastFrame;
static unsigned gUsMagnifyLastFrame;

/* src/dc/ifcommon.c: where each player's magnifying glass is;
 * the arrow script logs the first player's */
extern IFPlayerMagnify sIFCommonPlayerMagnifyInterface[GMCOMMON_PLAYERS_MAX];

static void db_run_arrow_script(void)
{
    GObj *fighter_gobj;
    const FTStruct *fp;

    if (gSCManagerSceneData.scene_curr != nSCKindVSBattle ||
        gSCManagerBattleState->game_status != nSCBattleGameStatusGo)
    {
        return;
    }
    fighter_gobj = db_fighter_gobj(0);

    if (fighter_gobj == NULL || gArrowScript > 130)
    {
        return;
    }
    gArrowScript++;
    fp = ftGetStruct(fighter_gobj);

    if (gArrowScript == 29 || (gArrowScript >= 40 && gArrowScript <= 46) ||
        gArrowScript == 130)
    {
        /* the triangle count is the last frame's whole scene: the three
         * chevrons come and go inside it as the animation hides them,
         * which is the only way from here to see that they drew -- and
         * the fighter drawn again inside the glass, with its handle,
         * is in it too while the glass is up, counted on its own with
         * the microseconds its two draws took -- which is where the
         * clip's cost is. The frame time is the whole draw, vsync wait
         * included. */
        dbglog(DBG_INFO,
               "db: arrows flags %u left %u right %u, p1 show %d at "
               "(%.0f, %.0f), %u tris; glass mode %u scale %.2f at "
               "(%.0f, %.0f), %u tris in it in %u us; frame %lu us\n",
               (unsigned)gIFCommonPlayerInterface.arrows_flags,
               (unsigned)gIFCommonPlayerInterface.arrows_left_status,
               (unsigned)gIFCommonPlayerInterface.arrows_right_status,
               (int)fp->is_magnify_show,
               fp->magnify_pos.x, fp->magnify_pos.y, gTrisLastFrame,
               (unsigned)gIFCommonPlayerInterface.magnify_mode,
               gIFCommonPlayerInterface.magnify_scale,
               sIFCommonPlayerMagnifyInterface[0].pos.x,
               sIFCommonPlayerMagnifyInterface[0].pos.y,
               gTrisMagnifyLastFrame, gUsMagnifyLastFrame,
               (unsigned long)dSYTaskmanFrameTimeDelta);
    }
}

/* What the Down-B fed above did. P2 is Mario, so the status
 * the log should show at tic 31 is SpecialLw -- the grounded half, which
 * is where a Tornado started on a platform lands inside its own first
 * frame -- and Wait again once the animation ends. Before this step the
 * same input printed Squat. */
static void db_run_downb_script(void)
{
    GObj *fighter_gobj;
    const FTStruct *fp;

    if (gDownBScript != 29 && gDownBScript != 31 && gDownBScript != 90)
    {
        return;
    }
    fighter_gobj = db_fighter_gobj(1);

    if (fighter_gobj == NULL)
    {
        return;
    }
    fp = ftGetStruct(fighter_gobj);

    dbglog(DBG_INFO, "db: downb tic %d -- P2 %s, ga %d, vel_air.y %.1f, "
                     "motion %d\n",
           gDownBScript, ftMainStatusName(fp->status_id), (int)fp->ga,
           fp->physics.vel_air.y, (int)fp->motion_id);
}

/* SYTaskmanSceneSetup.func_controller, which db_install hands to every
 * scene the chain walks. The game's is syControllerFuncRead, the consume
 * half alone: its 60 Hz read runs off the scheduler's retrace callback.
 * The port has no scheduler thread, so the read and the consume both
 * happen here, once a frame. What this adds to syControllerFuncRead is
 * the stand-in feed between the two halves, and the log lines that want
 * to run on a tic the object system does not (a paused one). */
/* The results screen has no frame line of its own, and it has one thing worth a number: the confetti, two particle
 * generators mnVSResultsMakeConfetti makes at tic 120. So every sixty
 * tics of that scene, how many particles are live. */
static void db_log_results(void)
{
    if (gSCManagerSceneData.scene_curr != nSCKindVSResults ||
        (dSYTaskmanUpdateCount % 60) != 0)
    {
        return;
    }
    dbglog(DBG_INFO, "db: results tic %u, %u particles live\n",
           (unsigned)dSYTaskmanUpdateCount,
           (unsigned)gLBParticleStructsUsedNum);
}

/* ---- The hang watchdog --------------------------------------------
 * A frozen picture with the music still playing says the game thread has
 * stopped and nothing else about where. This asks KOS where: a thread
 * that wakes every second and, when the game thread has not moved for
 * DB_HANG_SECONDS, prints every thread's saved PC, PR (the return
 * address) and stack pointer -- once per stall. A preempted thread's
 * registers are in its kthread_t context, so the stuck thread's PC is
 * exactly where it was when the timer took it;
 *   sh-elf-addr2line -f -e src/game/ssb64/ssb64-disc.elf <pc> <pr>
 * names the loop. *
 * "Moved" is any of three:
 * - gSYTaskmanAlive (taskman.h), which every tic and every tic-wait
 *   vblank bumps, in every scene.  (A
 *   heartbeat in db_func_controller would only cover the scenes
 *   db_install hooks.)
 * - asset_io_state's call count (assetroot.h). A scene change reads
 *   for seconds with no tic; while the reads come, the stall is a
 *   load, and the log says "slow load" once, with no thread dump.
 * - a disc call under way for less than DB_HANG_SECONDS, which on a real
 *   drive is one big read, not a stuck one.
 * When all stop, a disc call still holding the I/O lock is named: "hang
 * in read /cd/x.bin after N ms" points at KOS's GD-ROM path, not the
 * game. The loader thread's reads (assetroot.h,
 * reading ahead) count as progress too, so while it works a stuck game
 * reads as a slow load; it stops when its queue or its RAM runs out.
 *
 * This thread's dbglog lines can interleave with the main thread's
 * (printf keeps no lock here; src/dc/fgm.c). They only come when the
 * main thread has stopped for seconds, so in practice they do not. */
#define DB_HANG_SECONDS 4

static volatile uint32_t gDbHeartbeat;

static int db_hang_print_thread(kthread_t *thd, void *user)
{
    (void)user;
    uint32_t sp = thd->context.r[15], *w;
    char line[160];
    int i, n = 0, len = 0;

    dbglog(DBG_WARNING, "db: hang: thread %d \"%s\" state %d pc %08lx "
           "pr %08lx sp %08lx\n", (int)thd->tid, thd->label, (int)thd->state,
           (unsigned long)thd->context.pc, (unsigned long)thd->context.pr,
           (unsigned long)sp);
    /* The stack words that look like return addresses into the program
     * text, innermost first: addr2line them for the call chain the PC
     * alone does not give (a PC in a KOS wait says only that it waits). */
    if (sp < 0x8c010000 || sp >= 0x8d000000)
        return 0;
    w = (uint32_t *)sp;
    for (i = 0; i < 256 && (uint32_t)(w + i) < 0x8d000000 && n < 16; i++)
    {
        extern char _etext;

        if (w[i] >= 0x8c010000 && w[i] < (uint32_t)&_etext && !(w[i] & 1))
        {
            len += snprintf(line + len, sizeof(line) - len, " %08lx",
                            (unsigned long)w[i]);
            n++;
            if (len > 100)
            {
                dbglog(DBG_WARNING, "db: hang:   stack%s\n", line);
                len = 0;
            }
        }
    }
    if (len)
        dbglog(DBG_WARNING, "db: hang:   stack%s\n", line);
    return 0;
}

static void *db_hang_watch(void *arg)
{
    uint32_t alive = gSYTaskmanAlive, still = 0, slow_from = 0;
    int reported = 0, slow = 0;
    AssetIOState io, last;

    (void)arg;
    asset_io_state(&last);
    for (;;)
    {
        thd_sleep(1000);
        asset_io_state(&io);
        if (gSYTaskmanAlive != alive)
        {
            if (slow)
                dbglog(DBG_INFO, "db: hang watch: the load ended after "
                       "%u s, %u disc calls\n", (unsigned)still,
                       (unsigned)(io.calls - slow_from));
            alive = gSYTaskmanAlive;
            last = io;
            still = 0;
            reported = 0;
            slow = 0;
            continue;
        }
        still++;
        /* a call under way for less than the limit is a load too: a real
         * drive takes over a second over one 1.5 MB pack read whole
         * (-DDB_IO_SLOW showed it on the Room's donkey.pack) */
        if (io.calls != last.calls ||
            (io.tid >= 0 && io.ms < DB_HANG_SECONDS * 1000u))
        {
            if (still >= DB_HANG_SECONDS && !slow)
            {
                dbglog(DBG_INFO, "db: hang watch: slow load -- no tic for "
                       "%u s (tic %u), disc calls still finishing\n",
                       (unsigned)still, (unsigned)dSYTaskmanUpdateCount);
                slow = 1;
                slow_from = last.calls;
            }
            last = io;
            reported = 0;
            continue;
        }
        if (still >= DB_HANG_SECONDS && !reported)
        {
            if (io.tid >= 0)
                dbglog(DBG_WARNING, "db: hang in %s %s after %u ms "
                       "(thread %d holds the I/O lock); every thread:\n",
                       io.op, (io.what != NULL) ? io.what : "-",
                       (unsigned)io.ms, io.tid);
            else
                dbglog(DBG_WARNING, "db: hang: no tic for %u s (tic %u), "
                       "no disc call under way; every thread:\n",
                       (unsigned)still, (unsigned)dSYTaskmanUpdateCount);
            thd_each(db_hang_print_thread, NULL);
            reported = 1;
        }
    }
    return NULL;
}

static void db_func_controller(void)
{
    gDbHeartbeat++;
#ifdef DB_FT_WATCH
    /* -DDB_FT_WATCH: once a second, in any scene with this controller
     * hook (training, 1P, the battles), every fighter's status and
     * position and the main camera's eye and at -- for a scene whose
     * picture (-DDB_FB_DUMP) is wrong and whose own logs are quiet. */
#ifdef DB_FORCE_FLASH
    /* -DDB_FORCE_FLASH: every fighter's colour animation held on the
     * invincibility star, all four at once, for a perf A/B of the fog
     * and offset-colour draw path against the same scene without it. */
    {
        GObj *g = gGCCommonLinks[nGCCommonLinkIDFighter];

        for (; g != NULL; g = g->link_next)
        {
            FTStruct *ffp = ftGetStruct(g);

            if (ffp->colanim.colanim_id != nGMColAnimFighterStar)
                ftParamCheckSetFighterColAnimID(g, nGMColAnimFighterStar, 0);
        }
    }
#endif
    if ((gDbHeartbeat % 60) == 0)
    {
        GObj *g = gGCCommonLinks[nGCCommonLinkIDFighter];

        for (; g != NULL; g = g->link_next)
        {
            FTStruct *wfp = ftGetStruct(g);

            dbglog(DBG_INFO, "db: watch %lu P%d %s (%.0f, %.0f) %s dead %d\n",
                   (unsigned long)gDbHeartbeat, (int)wfp->player + 1,
                   ftMainStatusName(wfp->status_id),
                   DObjGetStruct(g)->translate.vec.f.x,
                   DObjGetStruct(g)->translate.vec.f.y,
                   (wfp->ga == nMPKineticsGround) ? "ground" : "air",
                   (int)wfp->is_rebirth);
        }
        if (gGMCameraGObj != NULL)
        {
            CObj *wc = CObjGetStruct(gGMCameraGObj);

            dbglog(DBG_INFO, "db: watch camera eye (%.0f, %.0f, %.0f) at (%.0f, %.0f, %.0f) status %d\n",
                   wc->vec.eye.x, wc->vec.eye.y, wc->vec.eye.z,
                   wc->vec.at.x, wc->vec.at.y, wc->vec.at.z,
                   (int)gGMCameraStruct.status_curr);
        }
    }
#endif
    gTrisLastFrame = dc_model_tris();
    gTrisMagnifyLastFrame = dc_model_tris_magnify();
    gUsMagnifyLastFrame = dc_model_us_magnify();
    dc_model_tris_reset();

    sy_input_poll();
    /* This function stands in for syControllerFuncRead, so it owes the
     * same port report, and in the same place: above the stand-in, so
     * the line says what the machine has rather than what this file
     * is pretending it has (src/dc/input.h). */
    sy_input_report_ports();
#ifdef DB_SOAK
    db_soak_results();
#endif
    db_feed_pad1();
    sy_input_update();
    /* -DDB_LOG_TAPS: every button EDGE on every port, the frame the
     * game sees it. The port has no other way to ask "who pressed
     * that?", and the question is not rare -- the stand-in above drives
     * port 1 off gSCManagerSceneData.scene_curr, and a scene that lies
     * about which scene it is (see the 1P ladder arm) sends it to the
     * wrong script with no other symptom than the game misbehaving.
     * This is what found that: "tap port 1 1000 at frame 601", START,
     * on the odd frames the title arm feeds. */
#ifdef DB_LOG_TAPS
    {
        int p;

        for (p = 0; p < MAXCONTROLLERS; p++)
        {
            if (gSYControllerDevices[p].button_tap != 0)
            {
                dbglog(DBG_INFO, "db: tap port %d %04x at frame %d\n", p,
                       gSYControllerDevices[p].button_tap,
                       (int)dSYTaskmanUpdateCount);
            }
        }
    }
#endif
#ifdef DB_BOSS_KO
    /* -DDB_BOSS_KO (with -DDB_BOOT_1P_STAGE=nSC1PGameStageBoss): five
     * seconds after GO (a second after his stock_damage_all goes to
     * 299, which the background rows read), his damage goes to 300 and the game's
     * own check (ftBossCommonUpdateDamageStats) runs on it -- the defeat
     * the port could not otherwise reach in a probe: the MapZoom on the
     * hit joint, the wallpaper hooks and the Anim-camera defeat zoom. */
    {
        static int go_tics;
        extern void ftBossCommonUpdateDamageStats(GObj *fighter_gobj);

        if (gSCManagerBattleState != NULL &&
            gSCManagerBattleState->game_status == nSCBattleGameStatusGo &&
            ++go_tics == 240)
        {
            /* the background's rows step on the boss's stock_damage_all
             * (sc1PGameBossWallpaperProcUpdate: past 90, then past 180),
             * and the defeat's own change steps from the last of those
             * to the row whose fade ends the match -- so the damage the
             * rows read goes up a second before the KO, the way a real
             * fight's hits would have taken it */
            GObj *g = gGCCommonLinks[nGCCommonLinkIDFighter];

            for (; g != NULL; g = g->link_next)
            {
                if (ftGetStruct(g)->fkind == nFTKindBoss)
                    gSCManagerBattleState->players[ftGetStruct(g)->player].stock_damage_all = 299;
            }
        }
        if (gSCManagerBattleState != NULL &&
            gSCManagerBattleState->game_status == nSCBattleGameStatusGo &&
            go_tics == 300)
        {
            GObj *g = gGCCommonLinks[nGCCommonLinkIDFighter];

            for (; g != NULL; g = g->link_next)
            {
                FTStruct *bfp = ftGetStruct(g);

                if (bfp->fkind != nFTKindBoss)
                    continue;
                bfp->percent_damage = 300;
                dbglog(DBG_INFO, "db: boss ko: damage 300 at frame %d\n",
                       (int)dSYTaskmanUpdateCount);
                ftBossCommonUpdateDamageStats(g);
                dbglog(DBG_INFO, "db: boss ko: boss status %d, camera status %d\n",
                       (int)bfp->status_id, (int)gGMCameraStruct.status_curr);
                break;
            }
        }
        if (go_tics >= 300 && gSCManagerBattleState != NULL &&
            (dSYTaskmanUpdateCount % 120) == 0)
        {
            GObj *g = gGCCommonLinks[nGCCommonLinkIDFighter];

            for (; g != NULL; g = g->link_next)
            {
                if (ftGetStruct(g)->fkind == nFTKindBoss)
                {
                    dbglog(DBG_INFO, "db: boss ko: frame %d: game status %d, boss status %d, anim frame %d, camera status %d\n",
                           (int)dSYTaskmanUpdateCount, (int)gSCManagerBattleState->game_status,
                           (int)ftGetStruct(g)->status_id,
                           (int)DObjGetStruct(g)->anim_frame,
                           (int)gGMCameraStruct.status_curr);
                }
            }
        }
    }
#endif
#ifdef DB_BOSS_HIT
    /* -DDB_BOSS_HIT (with -DDB_BOOT_1P_STAGE=nSC1PGameStageBoss): every
     * two seconds of GO, Master Hand takes a 20% launching hit, written
     * into the fields a landed hit fills (ftMainUpdateDamageStatFighter)
     * so the hit-stats pass takes it the way it takes a real one. Logs
     * his status, position and damage each second: a boss that "falls
     * off" shows a common damage status and a falling y. */
    {
        static int go_tics;

        if (gSCManagerBattleState != NULL &&
            gSCManagerBattleState->game_status == nSCBattleGameStatusGo)
        {
            GObj *g = gGCCommonLinks[nGCCommonLinkIDFighter];

            go_tics++;

            for (; g != NULL; g = g->link_next)
            {
                FTStruct *bfp = ftGetStruct(g);

                if (bfp->fkind != nFTKindBoss)
                    continue;
                if ((go_tics % 120) == 0)
                {
                    bfp->damage_queue = 20;
                    bfp->damage_knockback = 120.0f;
                    bfp->damage_angle = 45;
                    bfp->damage_lr = -1;
                    bfp->damage_index = 1;
                    bfp->damage_element = nGMHitElementNormal;
                    bfp->damage_player_num = 0;
                    bfp->damage_player = 0;
                    dbglog(DBG_INFO, "db: boss hit at go tic %d\n", go_tics);
                }
                if ((go_tics % 60) == 0)
                {
                    dbglog(DBG_INFO, "db: boss tic %d: status %d ga %d pos %d,%d damage %d floor line %d\n",
                           go_tics, (int)bfp->status_id, (int)bfp->ga,
                           (int)DObjGetStruct(g)->translate.vec.f.x,
                           (int)DObjGetStruct(g)->translate.vec.f.y,
                           (int)bfp->percent_damage,
                           (int)bfp->coll_data.floor_line_id);
                }
            }
        }
    }
#endif
#ifdef DB_VS_KO
    /* -DDB_VS_KO (with -DDB_BOOT_SCENE=nSCKindVSBattle): every five
     * seconds of GO, P2 is set down below the bottom blast line, so a
     * stock match ends in P1's favour in well under a minute and the
     * results screen -- its wipe included -- comes up with a winner. */
    {
        static int go_tics;

        if (gSCManagerBattleState != NULL &&
            gSCManagerBattleState->game_type == nSCBattleGameTypeRoyal &&
            gSCManagerBattleState->game_status == nSCBattleGameStatusGo &&
            (++go_tics % 300) == 0)
        {
            GObj *g = db_fighter_gobj(1);

            if (g != NULL && !ftGetStruct(g)->is_ghost)
            {
                dbglog(DBG_INFO, "db: vs ko: P2 below the blast line\n");
                DObjGetStruct(g)->translate.vec.f.y = gMPCollisionGroundData->map_bound_bottom - 500;
            }
        }
    }
#endif
#ifdef DB_1P_AUTOCLEAR
    /* -DDB_1P_AUTOCLEAR=<seconds> (the load census's ladder):
     * the two kinds of rung an unattended player cannot finish are ended
     * that many seconds after GO. A bonus stage's clock is run down to
     * its last tics (the stage then ends the way a timeout does), and
     * Master Hand takes DB_BOSS_KO's defeat: damage 300 and the game's
     * own check. The ordinary rungs are left to the fight, or to
     * DB_1P_ENEMY_KO. Whatever ends a rung, the loads around it are the
     * same, which is all the census asks of it. */
    {
        static int go_tics;
        static u32 last_update;
        extern void ftBossCommonUpdateDamageStats(GObj *fighter_gobj);

        /* the update count starts again at every scene */
        if (dSYTaskmanUpdateCount < last_update)
            go_tics = 0;
        last_update = dSYTaskmanUpdateCount;

        if (gSCManagerBattleState != NULL &&
            (gSCManagerBattleState->game_type == nSCBattleGameType1PGame ||
             gSCManagerBattleState->game_type == nSCBattleGameTypeBonus) &&
            gSCManagerBattleState->game_status == nSCBattleGameStatusGo &&
            ++go_tics == (DB_1P_AUTOCLEAR) * 60 - 60)
        {
            /* DB_BOSS_KO's first step, a second ahead: the wallpaper's
             * rows step on his stock_damage_all, and the fade that ends
             * the match is the row after the last -- straight to 300
             * and the defeat plays but the scene never ends */
            GObj *g = gGCCommonLinks[nGCCommonLinkIDFighter];

            for (; g != NULL; g = g->link_next)
            {
                if (ftGetStruct(g)->fkind == nFTKindBoss)
                    gSCManagerBattleState->players[ftGetStruct(g)->player].stock_damage_all = 299;
            }
        }
        else if (gSCManagerBattleState != NULL &&
            (gSCManagerBattleState->game_type == nSCBattleGameType1PGame ||
             gSCManagerBattleState->game_type == nSCBattleGameTypeBonus) &&
            gSCManagerBattleState->game_status == nSCBattleGameStatusGo &&
            go_tics == (DB_1P_AUTOCLEAR) * 60)
        {
            GObj *g = gGCCommonLinks[nGCCommonLinkIDFighter];
            sb32 boss = FALSE;

            for (; g != NULL; g = g->link_next)
            {
                FTStruct *bfp = ftGetStruct(g);

                if (bfp->fkind != nFTKindBoss)
                    continue;
                boss = TRUE;
                bfp->percent_damage = 300;
                ftBossCommonUpdateDamageStats(g);
                dbglog(DBG_INFO, "db: 1p autoclear: boss defeated at frame %d\n",
                       (int)dSYTaskmanUpdateCount);
            }
            if (!boss && gSCManagerBattleState->game_type == nSCBattleGameTypeBonus)
            {
                gSCManagerBattleState->time_remain = 2;
                dbglog(DBG_INFO, "db: 1p autoclear: bonus clock run down at frame %d\n",
                       (int)dSYTaskmanUpdateCount);
            }
        }
    }
#endif
#ifdef DB_1P_ENEMY_KO
    /* -DDB_1P_ENEMY_KO (with -DDB_BOOT_SCENE=nSCKind1PGame): every five
     * seconds of a running rung, one live 1P enemy is set down just past
     * the stage's team box on the left, well inside the blast lines, so
     * the only thing that can KO it there is the team box
     * (ftCommonDeadCheckInterruptCommon's is_spgame_enemy arm). The log
     * then shows the enemy tally fall and the next member come on
     * (sc1PGameSpawnEnemyTeamNext). */
    {
        static int go_tics;
        extern u8 sSC1PGameEnemyStocksRemaining;

        if (gSCManagerSceneData.scene_curr != nSCKind1PBonusStage &&
            gSCManagerBattleState != NULL &&
            gSCManagerBattleState->game_type == nSCBattleGameType1PGame &&
            gSCManagerBattleState->game_status == nSCBattleGameStatusGo &&
            (++go_tics % 300) == 0)
        {
            s32 p;

            for (p = 0; p < GMCOMMON_PLAYERS_MAX; p++)
            {
                GObj *g = db_fighter_gobj(p);
                FTStruct *efp;

                if (!gSCManagerBattleState->players[p].is_spgame_enemy || g == NULL)
                    continue;
                efp = ftGetStruct(g);
                /* not Master Hand: the team box cannot KO a boss, and a
                 * boss moved there mid-defeat never finishes it -- the
                 * load census's ladder sat on his rung for 18 minutes.
                 * DB_1P_AUTOCLEAR or DB_BOSS_KO is his way out. */
                if (efp->is_ghost || efp->is_invisible ||
                    efp->fkind == nFTKindBoss)
                    continue;

                dbglog(DBG_INFO, "db: 1p ko: enemy %d (kind %d) to x %d, team box left %d, blast left %d; %d enemy stock(s) left\n",
                       (int)p, (int)efp->fkind,
                       (int)gMPCollisionGroundData->map_bound_team_left - 200,
                       (int)gMPCollisionGroundData->map_bound_team_left,
                       (int)gMPCollisionGroundData->map_bound_left,
                       (int)sSC1PGameEnemyStocksRemaining);
                DObjGetStruct(g)->translate.vec.f.x = gMPCollisionGroundData->map_bound_team_left - 200;
                DObjGetStruct(g)->translate.vec.f.y = 0.0F;
                break;
            }
        }
        if (gSCManagerBattleState != NULL && gSCManagerBattleState->game_type == nSCBattleGameType1PGame)
        {
            static int last_status[GMCOMMON_PLAYERS_MAX];
            s32 p;

            for (p = 0; p < GMCOMMON_PLAYERS_MAX; p++)
            {
                GObj *g = db_fighter_gobj(p);
                s32 st;

                if (g == NULL || !gSCManagerBattleState->players[p].is_spgame_enemy)
                    continue;
                st = ftGetStruct(g)->status_id;
                if ((st == nFTCommonStatusDeadDown || st == nFTCommonStatusDeadLeftRight ||
                     st == nFTCommonStatusDeadUpStar || st == nFTCommonStatusDeadUpFall) &&
                    last_status[p] != st)
                {
                    dbglog(DBG_INFO, "db: 1p ko: enemy %d dead (status %d) at frame %d; %d enemy stock(s) left\n",
                           (int)p, (int)st, (int)dSYTaskmanUpdateCount, (int)sSC1PGameEnemyStocksRemaining);
                }
                if (last_status[p] >= nFTCommonStatusDeadDown && last_status[p] <= nFTCommonStatusDeadUpFall &&
                    !(st >= nFTCommonStatusDeadDown && st <= nFTCommonStatusDeadUpFall))
                {
                    dbglog(DBG_INFO, "db: 1p ko: enemy %d back as kind %d (status %d)\n",
                           (int)p, (int)ftGetStruct(g)->fkind, (int)st);
                }
                last_status[p] = st;
            }
        }
    }
#endif
#ifdef DB_BONUS_PAUSE_RETRY
    /* -DDB_BONUS_PAUSE_RETRY (with -DDB_BOOT_SCENE=nSCKind1PBonusStage):
     * two seconds into a course, START on port 0, which pauses on the
     * whole map (gmCameraSetStatusMapZoom) with "L: RETRY" up; a second
     * later, L, which ends the task still paused, so the scene runs the
     * course again. Twice, then the probe leaves it alone. */
    {
        static int go_tics, pause_tics, retries;
        extern u8 sIFCommonBattlePauseKindInterface;

        if (gSCManagerSceneData.scene_curr == nSCKind1PBonusStage && retries < 2)
        {
            if (gSCManagerBattleState->game_status == nSCBattleGameStatusGo)
            {
                pause_tics = 0;
                if (++go_tics == 120)
                {
                    gSYControllerDevices[0].button_tap |= START_BUTTON;
                    dbglog(DBG_INFO, "db: bonus pause-retry: START at frame %d\n",
                           (int)dSYTaskmanUpdateCount);
                }
            }
            else if (gSCManagerBattleState->game_status == nSCBattleGameStatusPause)
            {
                go_tics = 0;
                if (++pause_tics == 2)
                {
                    dbglog(DBG_INFO, "db: bonus pause-retry: paused, kind %d, camera status %d\n",
                           (int)sIFCommonBattlePauseKindInterface,
                           (int)gGMCameraStruct.status_curr);
                }
                if (pause_tics == 60)
                {
                    gSYControllerDevices[0].button_tap |= L_TRIG;
                    retries++;
                    dbglog(DBG_INFO, "db: bonus pause-retry: L at frame %d (retry %d)\n",
                           (int)dSYTaskmanUpdateCount, retries);
                }
            }
        }
    }
#endif
    db_log_game_status();
#ifdef DB_MAGNIFY_PROBE
    /* -DDB_MAGNIFY_PROBE: from tic 120 of the match, hold P1 just
     * under the top blast line, still, so the
     * camera cannot follow and the magnifying glass comes up; the
     * glass's draw logs its depth range (src/dc/fighter.c). */
    if (gSCManagerSceneData.scene_curr == nSCKindVSBattle &&
        gSCManagerBattleState->game_status == nSCBattleGameStatusGo &&
        db_fighter_gobj(0) != NULL)
    {
        static int tics;
        GObj *ft = db_fighter_gobj(0);
        FTStruct *fp = ftGetStruct(ft);

        if (++tics >= 120)
        {
            Vec3f *pos = &fp->joints[nFTPartsJointTopN]->translate.vec.f;

            pos->x = 0.0F;
            pos->y = gMPCollisionGroundData->map_bound_top - 600.0F;
            fp->physics.vel_air.x = fp->physics.vel_air.y = 0.0F;
            if ((tics % 60) == 0)
                dbglog(DBG_INFO, "db: magnify probe tic %d, P1 at (%.0f, %.0f), "
                       "show %d\n", tics, pos->x, pos->y, (int)fp->is_magnify_show);
        }
    }
#endif
    db_run_arrow_script();
    db_run_downb_script();
    db_log_results();
#ifdef DB_BONUS2_PROBE
    /* HERE and not in db_battle_run below, which is a VS battle's alone
     * -- scVSBattleStartBattle is what makes its GObj. This function is
     * every scene's controller read, the bonus stage's among them. */
    db_bonus2_probe((int)dSYTaskmanUpdateCount);
#endif
}

#ifdef DB_CLIFF_WARP
static int gCliffWarp;
#endif

/* The debug overlay's proc_display: collision lines and the fighter's
 * diamond, on the DL link the game keeps its HUD on so they draw over
 * everything. Both are flat screen-space quads in the opaque list. */
#ifdef DB_COLLISION_OVERLAY
static void db_overlay_display(GObj *gobj)
{
    (void)gobj;

    if (gcGetDrawList() != PVR_LIST_OP_POLY)
        return;

    draw_collision();
    draw_map_coll();
}
#endif

/* Which animation the fighter is playing, for the sampled log line:
 * the status picked a motion, the motion names a pack animation. The
 * game has no such lookup -- it prints nothing per frame. */
static const char *db_anim_name(const FTStruct *fp)
{
    const FPackMotion *m;

    const Fighter *model = dc_model_of(fp->fighter_gobj);

    if (fp->motion_id < 0 || (u32)fp->motion_id >= model->motion_count)
        return "-";
    m = &model->motions[fp->motion_id];
    if (m->anim < 0 || (u32)m->anim >= model->hd->anim_count)
        return "-";
    return model->anims[m->anim].name;
}

/* The names of sc/scdef.h:225-234's game statuses, for the log. Same
 * deal as db_anim_name: the game prints nothing, but a match that ends
 * walks four of these in ninety-odd tics and the log is how the target
 * run is read. */
static const char *db_game_status_name(s32 status)
{
    static const char *const names[] = {
        "Wait", "Go", "Pause", "Unpause", "?4",
        "End", "BossDefeat", "Set",
    };

    if (status < 0 || (u32)status >= sizeof(names) / sizeof(names[0]))
        return "?";
    return names[status];
}

/* ---- what this adds to the battle -----------------------------------
 *
 * The scene is the game's now (src/dc/scvsbattle.c), and it does not
 * return until the match is over, so everything this file wants --
 * the collision overlay, the serial log, the FGM engine's frame and the
 * cliff warp -- has to be inside it. scVSBattleStartBattle calls
 * gSCVSBattleFuncDebug once every GObj a battle needs exists and before
 * the first tic, and this is what goes there: one more GObj,
 * with a func_run for the per-tic work and a proc_display for the
 * overlay.
 *
 * It sits on nGCCommonLinkIDInterface, where the game's HUD sits, for
 * two reasons. The overlay draws over everything, which is what that DL
 * link is for. And the interface link is one of the two
 * ifCommonBattleInterfaceProcUpdate resumes after freezing the world at
 * GAME SET (src/dc/ifcommon.c), so the log and the FGM engine keep
 * running through the end-state timeline -- which is when the announcer
 * is talking.
 *
 * DIVERGES from where the warp used to be: it ran on the ground GObj,
 * link 1, ahead of the fighters. On link 11 it runs after them, so a
 * START press lands one tic later than it did. Nothing in the game does
 * this at all.
 */

static int gFgmLive;
static int gLastStatus[GMCOMMON_PLAYERS_MAX] = { -1, -1, -1, -1 };
static int gLastPercent[GMCOMMON_PLAYERS_MAX];
static int gLastGameStatus = -1;

static void db_log_game_status(void)
{
    if (gSCManagerBattleState->game_status == gLastGameStatus)
    {
        return;
    }
    dbglog(DBG_INFO, "db: game status %s at frame %d%s\n",
           db_game_status_name(gSCManagerBattleState->game_status),
           (int)dSYTaskmanUpdateCount,
           (gSCManagerBattleState->game_status == nSCBattleGameStatusGo)
               ? " -- GO!" : "");
    gLastGameStatus = gSCManagerBattleState->game_status;
}
static int gReported;
static uint32_t gLastTick;
static int gLastFrame;
static uint64_t gUsUpdate, gUsDraw;
static int gMatch;

static void db_battle_run(GObj *gobj)
{
    int frame = (int)dSYTaskmanUpdateCount;
    int player;

    (void)gobj;

    /* C-up is a jump button (ftcommon.c's FT_JUMP_BUTTONS), and on the
     * Dreamcast pad that is Y, so this press is also a jump. That is why
     * the warp is opt-in -- see DB_CLIFF_WARP above. */
#ifdef DB_CLIFF_WARP
    if (gSYControllerMain.button_tap & N64_C_UP)
        warp_to_cliff(gGRStages[gSCManagerBattleState->gkind], gCliffWarp++);
#endif

    /* Say when voices start and stop, so the log shows a landing making
     * a sound. The engine's frame is not run here: it is the audio
     * thread's, at the game's own 174 Hz (fgm.h), because the N64 runs
     * it off the synthesizer and not off whichever scene happens to be
     * up. This is only a look at it. */
    {
        int live = fgm_live();

        if ((live != 0) != (gFgmLive != 0))
            dbglog(DBG_INFO, "db: FGM %s at frame %d (%d voice%s)\n",
                   live ? "sounding" : "quiet", frame, live,
                   live == 1 ? "" : "s");
        gFgmLive = live;
    }

    gUsUpdate += dSYTaskmanUpdateTimeDelta;
    gUsDraw += dSYTaskmanFrameTimeDelta;

#ifdef DB_PARTICLE_TRACE
    /* Once, on the first tic of the match: the bank is loaded by then
     * (scVSBattleStartBattle) and nothing else has made a particle. */
    if (frame == 1)
    {
        db_particle_replay();
    }
#endif
#ifdef DB_PARTICLE_LIVE
    db_particle_live(frame);
#endif
#ifdef DB_EFFECT_LIVE
    db_effect_live(frame);
#endif
#ifdef DB_QUAKE_TRACE
    db_quake_trace(frame);
#endif
#ifdef DB_MARIO_SPECIALN_PROBE
    db_mario_specialn_probe(frame);
#endif
#ifdef DB_MARIO_FIREBALL_PALETTE_PROBE
    db_mario_fireball_palette_probe(frame);
#endif
#ifdef DB_FOX_BLASTER_PROBE
    db_fox_blaster_probe(frame);
#endif
#ifdef DB_MARIO_SPECIALHI_PROBE
    db_mario_specialhi_probe(frame);
#endif
#ifdef DB_FALLSPECIAL_PROBE
    db_fallspecial_probe(frame);
#endif
#ifdef DB_PASSCLIFF_PROBE
    db_passcliff_probe(frame);
#endif
#ifdef DB_SPECIALN_TABLE_PROBE
    db_specialn_table_probe(frame);
#endif
#ifdef DB_SPECIALHI_TABLE_PROBE
    db_specialhi_table_probe(frame);
#endif
#ifdef DB_SPECIALN_DEMUX_PROBE
    db_specialn_demux_probe(frame);
#endif
#ifdef DB_PURIN_SPECIALN_PROBE
    db_purin_specialn_probe(frame);
#endif
#ifdef DB_FOX_SPECIALN_PROBE
    db_fox_specialn_probe(frame);
#endif
#ifdef DB_FOX_STATUS_PROBE
    db_fox_status_probe(frame);
#endif
#ifdef DB_FOX_BLASTER_LIVE_PROBE
    db_fox_blaster_live_probe(frame);
#endif
#ifdef DB_ITEM_LIVE_PROBE
    db_item_live_probe(frame);
#endif
#ifdef DB_ITEM_PICKUP_PROBE
    db_item_pickup_probe(frame);
#endif
#ifdef DB_ITEM_ALT_PROBE
    db_item_alt_probe(frame);
#endif
#ifdef DB_TARUBOMB_PROBE
    db_tarubomb_probe(frame);
#endif
#ifdef DB_COMPLETE_BANNER
    db_complete_banner(frame);
#endif
#ifdef DB_SPECIALHI_DEMUX_PROBE
    db_specialhi_demux_probe(frame);
#endif
#ifdef DB_PURIN_SPECIALHI_PROBE
    db_purin_specialhi_probe(frame);
#endif
#ifdef DB_DONKEY_SPECIALHI_PROBE
    db_donkey_specialhi_probe(frame);
#endif
#ifdef DB_SAMUS_SPECIALHI_PROBE
    db_samus_specialhi_probe(frame);
#endif
#ifdef DB_CAPTAIN_SPECIALN_PROBE
    db_captain_specialn_probe(frame);
#endif
#ifdef DB_SAMUS_SPECIALLW_PROBE
    db_samus_speciallw_probe(frame);
#endif
#ifdef DB_CAPTAIN_SPECIALLW_PROBE
    db_captain_speciallw_probe(frame);
#endif
#ifdef DB_YOSHI_SPECIALLW_PROBE
    db_yoshi_speciallw_probe(frame);
#endif
#ifdef DB_DONKEY_SPECIALN_PROBE
    db_donkey_specialn_probe(frame);
#endif
#ifdef DB_CAPTAIN_SPECIALHI_PROBE
    db_captain_specialhi_probe(frame);
#endif
#ifdef DB_SAMUS_SPECIALN_PROBE
    db_samus_specialn_probe(frame);
#endif
#ifdef DB_FOX_SPECIALLW_PROBE
    db_fox_speciallw_probe(frame);
#endif
#ifdef DB_FOX_SPECIALHI_PROBE
    db_fox_specialhi_probe(frame);
#endif
#ifdef DB_SPECIALAIR_PROBE
    db_specialair_probe(frame);
#endif
#ifdef DB_ATTACKAIR_PROBE
    db_attackair_probe(frame);
#endif
#ifdef DB_SMASH_PROBE
    db_smash_probe(frame);
#endif
#ifdef DB_ATTACKDASH_PROBE
    db_attackdash_probe(frame);
#endif
#ifdef DB_SINGLES_PROBE
    db_singles_probe(frame);
#endif
#ifdef DB_YOSHI_EGGLAY_PROBE
    db_yoshi_egglay_probe(frame);
#endif
#ifdef DB_YOSHI_EGGTHROW_PROBE
    db_yoshi_eggthrow_probe(frame);
#endif
#ifdef DB_KIRBY_CUTTER_PROBE
    db_kirby_cutter_probe(frame);
#endif
#ifdef DB_KIRBY_INHALE_PROBE
    db_kirby_inhale_probe(frame);
#endif
#ifdef DB_KIRBY_COPY_PROBE
    db_kirby_copy_probe(frame);
#endif
#ifdef DB_LINK_BOOMERANG_PROBE
    db_link_boomerang_probe(frame);
#endif
#ifdef DB_LUIGI_TABLE_PROBE
    db_luigi_table_probe(frame);
#endif
#ifdef DB_JOLT_PROBE
    db_jolt_probe(frame);
#endif
#ifdef DB_THUNDER_PROBE
    db_thunder_probe(frame);
#endif
#ifdef DB_AGILITY_PROBE
    db_agility_probe(frame);
#endif
#ifdef DB_UB_PROBE
    db_ub_probe(frame);
#endif
#ifdef DB_WPQUAD_PROBE
    db_wpquad_probe(frame);
#endif
#ifdef DB_WPLIFE_PROBE
    db_wplife_probe(frame);
#endif
#ifdef DB_BANK_PROBE
    db_bank_probe(frame);
#endif
#ifdef DB_GATE_PROBE
    db_gate_probe(frame);
#endif
#ifdef DB_SHADOW_PROBE
    db_shadow_probe(frame);
#endif
#ifdef DB_STANDIN_PROBE
    db_standin_probe(frame);
#endif
#ifdef DB_DOKAN_PROBE
    db_dokan_probe(frame);
#endif
#ifdef DB_ENTRANCE_PROBE
    db_entrance_probe(frame);
#endif
#ifdef DB_NESS_TABLE_PROBE
    db_ness_table_probe(frame);
#endif

    /* the action-state machine is what this file exists to watch,
     * so say every time it moves, per player; the sampled line below is
     * the frame-rate view. */
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        const FTStruct *fp;

        if (db_fighter_gobj(player) == NULL)
            continue;
        fp = ftGetStruct(db_fighter_gobj(player));
#ifdef DB_TRACE_SPAWN
        /* -DDB_TRACE_SPAWN: P1's first hundred tics, one line each --
         * where he is, which way physics has him, what the map proc
         * found under him. For a fighter that dies before the GO!. */
        if (player == 0 && dSYTaskmanUpdateCount < 100)
        {
            dbglog(DBG_INFO, "db: tic %lu P1 %s (%.1f, %.1f) %s floor %d vel (%.2f, %.2f) coll (%.1f, %.1f)\n",
                   (unsigned long)dSYTaskmanUpdateCount,
                   ftMainStatusName(fp->status_id),
                   DObjGetStruct(fp->fighter_gobj)->translate.vec.f.x,
                   DObjGetStruct(fp->fighter_gobj)->translate.vec.f.y,
                   (fp->ga == nMPKineticsGround) ? "ground" : "air",
                   (int)fp->coll_data.floor_line_id,
                   fp->physics.vel_air.x, fp->physics.vel_air.y,
                   fp->coll_data.pos_prev.x, fp->coll_data.pos_prev.y);
        }
#endif
        if (fp->status_id != gLastStatus[player])
        {
            dbglog(DBG_INFO, "db: P%d %s -> %s at (%.0f, %.0f) %s; tic %lu, TopN (%.0f, %.0f)\n",
                   player + 1,
                   ftMainStatusName(gLastStatus[player]),
                   ftMainStatusName(fp->status_id),
                   DObjGetStruct(fp->fighter_gobj)->translate.vec.f.x,
                   DObjGetStruct(fp->fighter_gobj)->translate.vec.f.y,
                   (fp->ga == nMPKineticsGround) ? "ground" : "air",
                   (unsigned long)dSYTaskmanUpdateCount,
                   fp->joints[nFTPartsJointTopN]->translate.vec.f.x,
                   fp->joints[nFTPartsJointTopN]->translate.vec.f.y);
            gLastStatus[player] = fp->status_id;
        }
        /* a landed hit: the victim's percent moves (ftMainProcParams ran
         * ftParamUpdateDamage) -- say by whom and how hard */
        if (fp->percent_damage != gLastPercent[player])
        {
            dbglog(DBG_INFO,
                   "db: P%d hit by P%d for %d%% -> %d%%, hitlag %d, "
                   "at frame %d\n",
                   player + 1, (int)fp->damage_player + 1,
                   (int)fp->percent_damage - gLastPercent[player],
                   (int)fp->percent_damage, (int)fp->hitlag_tics, frame);
            gLastPercent[player] = fp->percent_damage;
        }
    }
    /* Sampled over the frames actually run since the last line, not over
     * a fixed 300. This proc is a GObj, a
     * paused tic does not run the object system, and a pause that swallows
     * a whole sample would divide a longer wall clock by 300 and report
     * a slowdown that never happened -- 10 fps for a match that never
     * left 59.8. The sample lands on the first run frame at or after each
     * 300th, so the divisor is what the log's own frame numbers say. */
    if (frame == 0 || frame - gLastFrame >= 300)
    {
        uint32_t now = timer_ms_gettime64();
        /* the first fighter in the match, which a sudden death that left
         * P1 out makes someone else (db_fighter_gobj) */
        const FTStruct *fp = NULL;
        int ran = (frame - gLastFrame >= 1) ? frame - gLastFrame : 1;
        int shown;

        for (shown = 0; shown < GMCOMMON_PLAYERS_MAX; shown++)
        {
            if (db_fighter_gobj(shown) != NULL)
            {
                fp = ftGetStruct(db_fighter_gobj(shown));
                break;
            }
        }

        dbglog(DBG_INFO, "db: update %lu us/f, draw %lu us/f (%s)\n",
               (unsigned long)(gUsUpdate / ran), (unsigned long)(gUsDraw / ran),
               MTX_BACKEND == MTX_BACKEND_SH4ZAM ? "sh4zam" : "scalar");
#ifdef DB_QUIET
        /* db.h's DB_QUIET: the numbers above in one short line, at the
         * one level it leaves printing */
        dbglog(DBG_WARNING, "perf: frame %d, %.1f fps, update %lu us/f, "
               "draw %lu us/f, %u tris\n", frame,
               ran * 1000.0f / (float)(now - gLastTick),
               (unsigned long)(gUsUpdate / ran),
               (unsigned long)(gUsDraw / ran), gTrisLastFrame);
#endif
        gUsUpdate = gUsDraw = 0;
        gLastFrame = frame;
#ifdef DB_PERF
        {
            static int calibrated;

            if (!calibrated)
            {
                calibrated = 1;
                db_perf_calibrate();
            }
            db_perf_report();
        }
#endif
        if (fp != NULL)
            dbglog(DBG_INFO,
                   "db: %d frames, %.1f fps, %u tris, P%d (%.0f, %.0f) "
                   "%s floor=%d anim=%s\n",
                   frame, ran * 1000.0f / (float)(now - gLastTick),
                   gTrisLastFrame, shown + 1,
                   DObjGetStruct(fp->fighter_gobj)->translate.vec.f.x,
                   DObjGetStruct(fp->fighter_gobj)->translate.vec.f.y,
                   ftMainStatusName(fp->status_id),
                   (int)fp->coll_data.floor_line_id, db_anim_name(fp));
        /* Stress-diagnostic (slowdown chase): the object pools
         * and the scene heap they fall back onto are the only things a
         * rate of jabs could plausibly exhaust; watch both trend over
         * the run rather than just at boot.
         *
         * gcGetGObjsActiveNum is the free list plus the active count
         * (objman.c:384-395), which is the pool's size and not how many
         * are in use -- the boot line beside it has always said "in the
         * pool" and this one said "active", which is what made it look
         * like a battle that gained a GObj had not. */
        dbglog(DBG_INFO, "db: %d GObjs in the pool, %u/%u scene heap bytes\n",
               (int)gcGetGObjsActiveNum(),
               (unsigned)syTaskmanGeneralHeapUsed(),
               (unsigned)syTaskmanGeneralHeapSize());
        gLastTick = now;
    }

    /* sys/taskman.c:1023, where the game's run loop breaks out and the
     * scene manager loads the next scene. ifCommonBattleSetUpdateInterface
     * raised it earlier this same tic, before gcRunAll reached this GObj,
     * so this is the last chance to read the battle before gcEjectAll
     * takes it down: say how the match ended -- the placement each player
     * was given by ifCommonBattleUpdateScoreStocks, the winner being the
     * one team never given one. The results screen is what does something
     * with those numbers (src/dc/scvsresults.c). */
    if (!gReported && syTaskmanCheckBreakLoop() != FALSE)
    {
        dbglog(DBG_INFO, "db: match over at frame %d\n", frame);

        for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
        {
            const SCPlayerData *pd = &gSCManagerBattleState->players[player];

            if (pd->pkind == nFTPlayerKindNot)
                continue;
            dbglog(DBG_INFO,
                   "db: P%d place %d, %d KOs, %d falls, %d self-destructs\n",
                   player + 1, (int)pd->place, (int)pd->score,
                   (int)pd->falls, (int)pd->total_selfdestructs);
        }
        gReported = 1;
    }
}

#ifdef DB_WP_DISPLAY_PROBE
/* A counting stand-in for gcDrawDObjDLHead1/dc_model_proc_display, so the
 * probe can watch which display-mode branch wpDisplayMain takes without a
 * live weapon model. */
static s32 sDBWpDisplayProcCount;
static void db_wp_display_fake_proc(GObj *g) { (void)g; sDBWpDisplayProcCount++; }
#endif

/* gSCVSBattleFuncDebug (src/dc/scvsbattle.h): this file's own GObj,
 * and the one DL link the battle camera would not otherwise capture. */
static void db_battle_start(void)
{
    GObj *gobj;
    int player;

    /* a second match is a second scene, and everything this file
     * remembers about a match is per-match */
    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        gLastStatus[player] = -1;
        gLastPercent[player] = 0;
    }
    gLastGameStatus = -1;
    gFgmLive = 0;
    gReported = 0;
#ifdef DB_CLIFF_WARP
    gCliffWarp = 0;
#endif
    gArrowScript = 0;
    gUsUpdate = gUsDraw = 0;

    gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, db_battle_run,
                             nGCCommonLinkIDInterface,
                             GOBJ_PRIORITY_DEFAULT);

    /* The header is compiled per match rather than once at install: the
     * PVR is reset between scenes, and a compiled context is cheap. */
#ifdef DB_COLLISION_OVERLAY
    db_overlay_init();

    gcAddGObjDisplay(gobj, db_overlay_display, DB_DLLINK_OVERLAY,
                     GOBJ_PRIORITY_DEFAULT, ~0);

    gGMCameraGObj->camera_mask |= COBJ_MASK_DLLINK(DB_DLLINK_OVERLAY);
#else
    /* the GObj still runs db_battle_run every frame -- the overlay was
     * just the only thing that wanted the handle back */
    (void)gobj;
#endif

    gLastTick = timer_ms_gettime64();
    gLastFrame = 0;

    dbglog(DBG_INFO, "db: match %d on %s; %u PVR bytes free, "
           "%u/%u scene heap bytes, %d GObjs in the pool; "
           "%u malloc bytes in use, %u arena\n",
           ++gMatch,
           gGRStages[gSCManagerBattleState->gkind]->model.hd->name,
           (unsigned)pvr_mem_available(),
           (unsigned)syTaskmanGeneralHeapUsed(),
           (unsigned)syTaskmanGeneralHeapSize(),
           (int)gcGetGObjsActiveNum(),
           (unsigned)mallinfo().uordblks, (unsigned)mallinfo().arena);
    /* The battle's load, closed here rather than at the scene's end: the
     * match runs until someone wins and the scene manager's own window
     * would not print until then. What the manager prints for
     * "vsbattle" afterwards is therefore what the match read while it
     * was playing, which should be nothing. */
    asset_io_report("vsbattle load");
    asset_io_reset();

#ifdef DB_WP_POOL_PROBE
    /* Prove the WPStruct pool allocates on SH-4 the way it
     * does on the host -- syTaskmanMalloc from the same live scene heap,
     * an 8-aligned free list, the never-zero group counter. Pulls three
     * structs, logs them, and returns them so the match's heap is left
     * as it was. Nothing else touches the pool yet (wpManagerMakeWeapon
     * is still a stub), so this is the only place it runs. */
    {
        WPStruct *a, *b, *c;

        wpManagerAllocWeapons();
        a = wpManagerGetNextStructAlloc();
        b = wpManagerGetNextStructAlloc();
        c = wpManagerGetNextStructAlloc();
        dbglog(DBG_INFO,
               "db: wp pool -- WPStruct %u bytes x%d; got %p %p %p "
               "(align&7 %d %d %d); group ids %u %u\n",
               (unsigned)sizeof(WPStruct), WEAPON_ALLOC_MAX,
               (void *)a, (void *)b, (void *)c,
               (int)((uintptr_t)a & 7), (int)((uintptr_t)b & 7),
               (int)((uintptr_t)c & 7),
               (unsigned)wpManagerGetGroupID(),
               (unsigned)wpManagerGetGroupID());
        if (c != NULL) wpManagerSetPrevStructAlloc(c);
        if (b != NULL) wpManagerSetPrevStructAlloc(b);
        if (a != NULL) wpManagerSetPrevStructAlloc(a);
    }
#endif

#ifdef DB_WP_MAIN_PROBE
    /* Prove the weapon's per-frame helpers give the same
     * numbers on SH-4 as the host test proves against the decomp. The
     * two that carry real float work are the ones worth watching on
     * hardware: wpMainApplyGravityClampTVel (its sqrtf/normalise clamp)
     * and wpMainGetStaledDamage (its f32->s32 truncation). Nothing calls
     * them in-game yet (wpManagerMakeWeapon is a stub), so this stack
     * WPStruct is the only exercise; it touches no heap or GObj. */
    {
        WPStruct pw;

        pw.physics.vel_air.x = 6.0F;
        pw.physics.vel_air.y = 4.0F;
        pw.physics.vel_air.z = 0.0F;
        wpMainApplyGravityClampTVel(&pw, 12.0F, 5.0F);  /* (6,-8)/10*5 -> (3,-4) */

        pw.attack_coll.damage = 12; pw.attack_coll.stale = 0.5F;

        dbglog(DBG_INFO,
               "db: wp main -- gravity clamp (%.3f, %.3f) want (3.000, -4.000); "
               "staled 12*0.5 -> %d want 6\n",
               pw.physics.vel_air.x, pw.physics.vel_air.y,
               (int)wpMainGetStaledDamage(&pw));
    }
#endif

#ifdef DB_WP_PROCESS_PROBE
    /* Prove the weapon procs' real-float helper gives the
     * same numbers on SH-4 as the host test proves against the decomp.
     * wpProcessUpdateHitOffsets is the one that carries trig
     * (syVectorRotate3D's sinf/cosf, sys/vector.c compiled unmodified),
     * so it is the one worth watching on hardware. The procs themselves
     * need the live gGCCommonLinks weapon chain MakeWeapon builds, which
     * does not exist yet; this stack DObj touches no heap or GObj. */
    {
        DObj pd;
        Vec3f po;

        /* scale (2,3,1), no rotation, translate (10,20,30) on (5,7,9)
         * -> (5*2, 7*3, 9) + (10,20,30) = (20,41,39) */
        pd.scale.vec.f.x = 2.0F; pd.scale.vec.f.y = 3.0F; pd.scale.vec.f.z = 1.0F;
        pd.rotate.vec.f.z = 0.0F;
        pd.translate.vec.f.x = 10.0F; pd.translate.vec.f.y = 20.0F; pd.translate.vec.f.z = 30.0F;
        po.x = 5.0F; po.y = 7.0F; po.z = 9.0F;
        wpProcessUpdateHitOffsets(&pd, &po);

        dbglog(DBG_INFO,
               "db: wp process -- hit offset (%.3f, %.3f, %.3f) "
               "want (20.000, 41.000, 39.000)\n",
               po.x, po.y, po.z);

        /* 90-deg Z rotation sends (1,0,0) -> (0,1,0), scale 1, translate 0 */
        pd.scale.vec.f.x = pd.scale.vec.f.y = pd.scale.vec.f.z = 1.0F;
        pd.rotate.vec.f.z = 3.14159265F / 2.0F;
        pd.translate.vec.f.x = pd.translate.vec.f.y = pd.translate.vec.f.z = 0.0F;
        po.x = 1.0F; po.y = 0.0F; po.z = 0.0F;
        wpProcessUpdateHitOffsets(&pd, &po);

        dbglog(DBG_INFO,
               "db: wp process -- rot90 (%.3f, %.3f, %.3f) "
               "want (0.000, 1.000, 0.000)\n",
               po.x, po.y, po.z);
    }
#endif
#ifdef DB_WP_DISPLAY_PROBE
    /* Prove wpDisplayMain's display-mode dispatch on SH-4 --
     * the same branch logic the host test checks against the decomp. A
     * counting fake proc stands in for the DObj draw; the real renderers
     * need MakeWeapon's baked model, which does not exist yet. This stack
     * GObj/WPStruct touches no heap. */
    {
        WPStruct wpd;
        GObj wgd;

        memset(&wpd, 0, sizeof wpd);
        memset(&wgd, 0, sizeof wgd);        /* obj NULL -> dc_model_of returns NULL */
        wgd.user_data.p = &wpd;

        /* Master: always draws (proc once) whatever the hitbox. */
        wpd.display_mode = nDBDisplayModeMaster;
        wpd.attack_coll.attack_state = nGMAttackStateNew;
        sDBWpDisplayProcCount = 0;
        wpDisplayMain(&wgd, db_wp_display_fake_proc);
        dbglog(DBG_INFO, "db: wp display -- master draws %d want 1\n", sDBWpDisplayProcCount);

        /* HitCollisionFill + live hitbox: diverts to the visualiser, no draw. */
        wpd.display_mode = nDBDisplayModeHitCollisionFill;
        sDBWpDisplayProcCount = 0;
        wpDisplayMain(&wgd, db_wp_display_fake_proc);
        dbglog(DBG_INFO, "db: wp display -- hitmode draws %d want 0\n", sDBWpDisplayProcCount);

        /* PK Thunder colour packing (trail_id 2 -> C2D9FFFF / 8633D9FF). */
        wpd.display_mode = nDBDisplayModeMaster;
        wpd.weapon_vars.pkthunder_trail.trail_id = 2;
        wpDisplayPKThunderProcDisplay(&wgd);
        dbglog(DBG_INFO, "db: wp display -- pkthunder trail 2 (packing checked on host)\n");
    }
#endif
#ifdef DB_WP_MAP_PROBE
    /* Prove wpmap.c's wall-bounce math on SH-4 -- the same
     * lbCommonSim2D sign / lbCommonReflect2D / lbCommonScale2D path the
     * host test checks. A stack GObj with a stack DObj (for the translate
     * DObjGetStruct reads) and a zeroed WPStruct; no heap. The mpProcess*
     * collision procs need a live map, so they wait on MakeWeapon. */
    {
        WPStruct wpm;
        DObj dobjm;
        GObj wgm;
        Vec3f rpos;
        sb32 rb;

        memset(&wpm, 0, sizeof wpm);
        memset(&dobjm, 0, sizeof dobjm);
        memset(&wgm, 0, sizeof wgm);
        wgm.obj = &dobjm;
        wgm.user_data.p = &wpm;
        dobjm.translate.vec.f.x = 10.0F;
        dobjm.translate.vec.f.y = 20.0F;
        dobjm.translate.vec.f.z = 30.0F;
        wpm.coll_data.map_coll.width = 2.0F;
        wpm.coll_data.map_coll.center = 1.0F;
        wpm.coll_data.mask_prev = 0;
        wpm.coll_data.mask_curr = MAP_FLAG_LWALL;
        wpm.coll_data.lwall_angle.x = 1.0F;
        wpm.physics.vel_air.x = -4.0F;

        rb = wpMapCheckAllRebound(&wgm, MAP_FLAG_MAIN_MASK, 0.8F, &rpos);
        dbglog(DBG_INFO, "db: wp map -- rebound %d velx %d.%03d pos (%d,%d,%d) want 1 3.200 (12,21,30)\n",
               rb,
               (int)wpm.physics.vel_air.x,
               (int)((wpm.physics.vel_air.x - (int)wpm.physics.vel_air.x) * 1000.0F),
               (int)rpos.x, (int)rpos.y, (int)rpos.z);

        wpm.lr = -1;
        wpm.physics.vel_air.x = 3.2F;
        wpMapSetGround(&wpm);
        dbglog(DBG_INFO, "db: wp map -- setground ga %d velground %d.%03d want 0 -3.200\n",
               wpm.ga,
               (int)wpm.physics.vel_ground,
               (int)((wpm.physics.vel_ground < 0 ? -(wpm.physics.vel_ground - (int)wpm.physics.vel_ground)
                                                 :  (wpm.physics.vel_ground - (int)wpm.physics.vel_ground)) * 1000.0F));
    }
#endif

#ifdef DB_WP_MAKE_PROBE
    /* Prove wpManagerMakeWeapon's SH-4 path -- the pool take,
     * the weapon GObj make, the model divergence's empty-table refuse, and
     * the unwind (SetPrevStructAlloc + gcEjectGObj). No fighter's weapon is
     * exported yet, so a real spawn is refused; the field-fills are the
     * same C the host cross-test checks exhaustively. This confirms the
     * refuse and its unwind run clean on hardware and hand the pool struct
     * back. Serial should also carry "wpmanager: no pack for the WPDesc".
     * The pool is live from scVSBattle (scvsbattle.c calls
     * wpManagerAllocWeapons); this peeks the free-list head, spawns, and
     * checks the same struct comes back. */
    {
        WPAttributes attr;
        void *attr_base = &attr;
        WPDesc desc;
        Vec3f spawn = { 10.0F, 20.0F, 30.0F };
        WPStruct *before;
        WPStruct *after;
        GObj *wg;

        memset(&attr, 0, sizeof attr);
        memset(&desc, 0, sizeof desc);
        desc.kind = 0x123;
        desc.p_weapon = &attr_base;
        desc.o_attributes = 0;

        before = wpManagerGetNextStructAlloc();
        if (before != NULL)
        {
            wpManagerSetPrevStructAlloc(before);
        }
        wg = wpManagerMakeWeapon(NULL, &desc, &spawn, WEAPON_FLAG_PARENT_GROUND);
        after = wpManagerGetNextStructAlloc();

        dbglog(DBG_INFO, "db: wp make -- refused %d pool_restored %d want 1 1\n",
               (int)(wg == NULL), (int)(after == before));
    }
#endif
#ifdef DB_MARIO_FIREBALL_PROBE
    /* Prove Mario's fireball reads its attribute table with the
     * right bytes on SH-4, and that its per-frame update gives the numbers
     * the host test proves against the decomp. The spawner
     * wpMarioFireballMakeWeapon needs a live fighter and a baked model (the
     * empty model table refuses on target), so it is exercised
     * on the host; this touches only the table and the ProcUpdate helper,
     * over a stack GObj/WPStruct/DObj that reaches no heap or GObj. */
    {
        extern wpMarioFireballAttributes dWPMarioFireballWeaponAttributes[];
        extern sb32 wpMarioFireballProcUpdate(GObj *weapon_gobj);
        WPStruct wpf;
        GObj wgf;
        DObj dof;

        dbglog(DBG_INFO,
               "db: mario fireball -- mario grav %.3f vel %.3f spin %.3f life %d "
               "want 1.200 50.000 0.349 140\n",
               dWPMarioFireballWeaponAttributes[0].gravity,
               dWPMarioFireballWeaponAttributes[0].vel_base,
               dWPMarioFireballWeaponAttributes[0].rotate_speed,
               (int)dWPMarioFireballWeaponAttributes[0].lifetime);
        dbglog(DBG_INFO,
               "db: mario fireball -- luigi vel %.3f life %d want 36.000 80\n",
               dWPMarioFireballWeaponAttributes[1].vel_base,
               (int)dWPMarioFireballWeaponAttributes[1].lifetime);

        memset(&wpf, 0, sizeof wpf);
        memset(&wgf, 0, sizeof wgf);
        memset(&dof, 0, sizeof dof);
        wgf.user_data.p = &wpf;
        wgf.obj = &dof;                     /* DObjGetStruct(weapon_gobj) */
        wpf.weapon_vars.fireball.index = 0; /* Mario */
        wpf.lifetime = 100;
        wpf.physics.vel_air.y = 0.0F;
        dof.rotate.vec.f.x = 0.0F;

        (void)wpMarioFireballProcUpdate(&wgf);

        dbglog(DBG_INFO,
               "db: mario fireball -- update life %d vely %.3f spin %.3f "
               "want 99 -1.200 0.349\n",
               (int)wpf.lifetime, (double)wpf.physics.vel_air.y,
               (double)dof.rotate.vec.f.x);
    }
#endif
#ifdef DB_MARIO_FIREBALL_MODEL_PROBE
    /* Prove the fireball's model. This is a real spawn: with romdisk/wpmariofireball.mdl baked and
     * its sWPManagerModels row in place, wpManagerAddModel now finds the pack
     * (wpModelPackFor keys on the WPDesc pointer, the port's divergence),
     * loads it, and builds the DObj tree + MObj. So the whole wp/ chain runs
     * end to end on SH-4 for the first time: pool take -> weapon GObj ->
     * model -> field-fills -> the MObj wpMarioFireballMakeWeapon writes
     * palette_id into. The WPAttributes reloc read is redirected onto a
     * stack buffer (DB_WP_MAKE_PROBE's trick), since no fighter file is
     * streamed here; the model itself needs no attr (the port looks it up by
     * descriptor). PARENT_GROUND spares a live fighter. Ejected after, to
     * hand the pool struct back. The pool is live from scVSBattle. */
    {
        extern GObj *wpManagerMakeWeapon(GObj *parent_gobj, WPDesc *wp_desc,
                                         Vec3f *spawn_pos, u32 flags);
        extern void wpMainDestroyWeapon(GObj *weapon_gobj);
        extern WPDesc dWPMarioFireballWeaponDesc;
        WPAttributes attr;
        void *attr_base = &attr;
        Vec3f spawn = { 10.0F, 20.0F, 30.0F };
        WPStruct *before;
        WPStruct *after;
        GObj *wg;
        DObj *root;

        memset(&attr, 0, sizeof attr);
        dWPMarioFireballWeaponDesc.p_weapon = &attr_base;
        dWPMarioFireballWeaponDesc.o_attributes = 0;

        /* The game allocates the pool later in the real scVSBattle setup;
         * at this debug hook it is still empty, so cut it from the (live)
         * scene heap here, as DB_WP_POOL_PROBE does. */
        wpManagerAllocWeapons();
        before = wpManagerGetNextStructAlloc();
        if (before != NULL)
        {
            wpManagerSetPrevStructAlloc(before);
        }
        wg = wpManagerMakeWeapon(NULL, &dWPMarioFireballWeaponDesc, &spawn,
                                 WEAPON_FLAG_PARENT_GROUND);
        root = (wg != NULL) ? DObjGetStruct(wg) : NULL;

        dbglog(DBG_INFO,
               "db: fireball model -- spawned %d dobj %d mobj %d want 1 1 1\n",
               (int)(wg != NULL), (int)(root != NULL),
               (int)(root != NULL && root->mobj != NULL));

        if (wg != NULL)
        {
            wpMainDestroyWeapon(wg);        /* returns the pool struct */
        }
        after = wpManagerGetNextStructAlloc();
        dbglog(DBG_INFO, "db: fireball model -- pool_restored %d want 1\n",
               (int)(after == before));
    }
#endif
}

/* ---- the boot state, and the overrides that carry it -----------------
 *
 * Everything below is the debug facility's: the game's own boot is scManagerInitData() and
 * the title, with every field holding the number the game boots with.
 * This overwrites the handful an unattended run needs -- a match that ends,
 * a pick already made so the scripted pad has one screen fewer to walk,
 * and, for a build that names one, a scene to start in instead of the
 * title.
 *
 * Called from main() after scManagerInitData() and after the scene data
 * is set to the game's boot, so DB_BOOT_SCENE has something to override.
 */
#if defined(DB_DCTEXT_TEST) && !defined(FT_HOSTTEST)
/* -DDB_DCTEXT_TEST (src/dc/db.h): the port's font over every frame,
 * through the overlay hook the memory card's notice will use -- every
 * glyph at the credits' own size, and a line at twice it, on a
 * translucent panel. For a framebuffer dump (-DDB_FB_DUMP) of the title
 * at 320x240 and the staff roll at 640x480: the text is the same size on
 * the screen in both, because the overlay draws in framebuffer pixels. */
static void db_dctext_overlay(int list)
{
    /* the layer this one replaced (src/game/ssb64/main.c) */
    vmunotice_overlay(list);
    if (list != PVR_LIST_TR_POLY)
    {
        return;
    }
    dctext_fill(16.0F, 16.0F, 624.0F, 150.0F, 20000.0F, 0xC0101830);
    dctext_draw("ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789\n"
                "abcdefghijklmnopqrstuvwxyz :.-,&\"/'?()\n"
                "SAVING... Do not remove the VMU",
                24.0F, 22.0F, 1.0F, 20001.0F, 0xFFFFFFFF);
    dctext_draw("VMU A1", 24.0F, 90.0F, 2.0F, 20001.0F, 0xFFB7BCEC);
}
#endif

void db_install(void)
{
#if defined(DB_DCTEXT_TEST) && !defined(FT_HOSTTEST)
    syTaskmanSetOverlayHook(db_dctext_overlay);
#endif
#ifdef DB_GDB
    /* First thing, unconditionally ahead of everything else below: this
     * is the one call here that can block on a human. gdb_init() (KOS,
     * kernel/arch/dreamcast/gdb/gdb.c) fires an initial breakpoint the
     * instant it returns, so from here the boot is stopped until
     * sh-elf-gdb/kos-gdb attaches over whatever link is up -- the same
     * dcload link DB_PC_ROOT uses (serial or BBA, relayed through
     * dc-tool's -g on host port 2159) if one is, or raw SCIF at 57600
     * baud otherwise. src/dc/db.h. */
    gdb_init();
#endif
#ifdef DB_IO_SELFTEST
    /* before any scene reads, so its lines are the log's first I/O */
    asset_io_selftest();
#endif
    /* Everything below is what an unattended, menu-skipping run needs --
     * a match that ends, a pick already made so the scripted pad has one
     * screen fewer to walk, a scene to start in instead of the title --
     * gated on DB_BOOT_SCENE naming something other than the title, the
     * developer's own signal that a non-default boot was asked for. A
     * plain build (DB_BOOT_SCENE left at its default) runs none of this:
     * the game boots exactly like the retail cart -- real controller
     * only, blank character-select, no phantom second pad -- because
     * nothing here touches the menu chain, the controller hooks or the
     * battle state at all. The scripted feed is gated on a
     * DB_BOOT_* flag, not on physical port occupancy. */
    if (DB_BOOT_SCENE != nSCKindTitle)
    {
        int player;

        /* A stock match of three (the field is one less than the count) --
         * the VS mode screen reads this and writes back what the player
         * set, so it's also what its page first shows.
         *
         * No time limit baked in here: a boot's own duration belongs
         * outside the game, at the invocation that runs it -- `timeout N`
         * on the probe, not a rule this facility hands the battle. Left at
         * SCBATTLE_TIMELIMIT_INFINITE (ifcommon.c already treats that value
         * as "no timer at all") so a probe that explicitly asks for the
         * time-based rule via DB_BOOT_RULE still ends only when its own
         * invocation says to, instead of inheriting a default
         * cutoff. */
        gSCManagerTransferBattleState.game_rules = DB_BOOT_RULE;
        gSCManagerTransferBattleState.stocks = 2;
        gSCManagerTransferBattleState.time_limit = SCBATTLE_TIMELIMIT_INFINITE;
        gSCManagerTransferBattleState.pl_count = DB_BOOT_PLAYERS;
        /* what the battle plays on when DB_BOOT_SCENE skips the stage
         * select; the stage select overwrites it when it runs */
        gSCManagerTransferBattleState.gkind = gBootStage;
        /* The game boots with this TRUE, and the character select's first
         * entry then clears every slot and clears the flag
         * (mnPlayersVSInitVars -> mnPlayersVSResetPlayer): a player who has
         * just turned the machine on picks their fighter. The
         * scripted pad does not -- it drives one puck and presses START,
         * which is the state the *second* visit to that screen comes up in,
         * with the last picks still on their gates. So the chain starts as
         * if the player had been here before. A zeroed battle state would
         * leave every flag nobody thought of FALSE. */
        gSCManagerTransferBattleState.is_reset_players = FALSE;
#ifdef DB_ITEM_RAIN
        /* -DDB_ITEM_RAIN: every item switched on at the highest rate, so
         * one 120-second boot puts most of the item set on screen. For a
         * LOOK, not a measurement -- the screenshot pass that found the
         * cyan smoke and the boxed Snorlax is what this feeds. */
        gSCManagerTransferBattleState.item_toggles = ~(u32)0;
        gSCManagerTransferBattleState.item_appearance_rate =
            nSCBattleItemSwitchVeryHigh;
#endif
        for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
        {
            static const s32 kBootKinds[GMCOMMON_PLAYERS_MAX] = {
                DB_BOOT_P1_KIND, DB_BOOT_P2_KIND, DB_BOOT_P3_KIND,
                DB_BOOT_P4_KIND
            };
            SCPlayerData *pd = &gSCManagerTransferBattleState.players[player];

            pd->pkind = (player < DB_BOOT_PLAYERS) ? nFTPlayerKindMan
                                                   : nFTPlayerKindNot;
            /* an absent player has no kind: the character select reads a
             * kind as a pick made (mnPlayersVSInitPlayer) and would stand
             * a fighter on an empty gate */
            /* P1/P2 default to Fox/Mario (DB_BOOT_P1_KIND/DB_BOOT_P2_KIND's
             * own defaults), either overridable to any fighter this port
             * has by name. P3 Kirby and P4 Yoshi only exist on a
             * DB_BOOT_PLAYERS=4 build; a slot the count does not reach
             * keeps a null kind, because a kind on an empty slot reads
             * as a pick made. */
            pd->fkind = (player < DB_BOOT_PLAYERS) ? kBootKinds[player]
                                                   : nFTKindNull;
#ifdef DB_BOOT_COM_P1
            /* Either slot can be a CPU fighter: ftMainProcUpdateInterrupt's
             * nFTPlayerKindCom case (src/dc/ftcommon.c) drives it through
             * ftComputerProcessAll instead of reading no controller.
             * DB_BOOT_COM_P1_LEVEL overrides the level (the CPU 1-9
             * slider) this build boots it at; unset, it keeps the
             * unconditional pd->level = 1 below, same as every other
             * boot player. */
            if (player == 0)
            {
                pd->pkind = nFTPlayerKindCom;
            }
#endif
#ifdef DB_BOOT_COM_ALL
            /* Every filled slot a CPU at DB_BOOT_COM_LEVEL (9 unset): a
             * four-CPU match that plays itself, as a playtest sets one
             * up in the VS mode. */
            pd->pkind = (player < DB_BOOT_PLAYERS) ? nFTPlayerKindCom
                                                   : nFTPlayerKindNot;
#endif
#ifdef DB_BOOT_COM_P2
            /* The same, for P2. */
            if (player == 1)
            {
                pd->pkind = nFTPlayerKindCom;
            }
#endif
            pd->team = pd->player = player;
            /* the HUD's tag sprite (1P, 2P...) and the colour of the tag
             * and of the emblem beside the damage (if/ifcommon.c): the
             * character select's per-port values */
            pd->tag = player;
            pd->color = player;
            pd->costume = 0;
            /* What mnVSModeGetShade deals outside a team battle: 0, no
             * shade. Shade 1 would keep ftManagerMakeFighter's
             * attr->shade_color[shade - 1] read on the table, but the
             * game makes that same read one before the table for every
             * shade-0 fighter, and since the game draws the shade, 1
             * would put every boot fighter under a team's white tint. */
            pd->shade = 0;
            pd->level = 1;
#ifdef DB_BOOT_COM_P1_LEVEL
            if (player == 0)
            {
                pd->level = DB_BOOT_COM_P1_LEVEL;
            }
#endif
#ifdef DB_BOOT_COM_P2_LEVEL
            if (player == 1)
            {
                pd->level = DB_BOOT_COM_P2_LEVEL;
            }
#endif
#ifdef DB_BOOT_COM_ALL
            pd->level = DB_BOOT_COM_LEVEL;
#endif
            pd->handicap = 5;
            pd->stock_count = gSCManagerTransferBattleState.stocks;
        }
        /* Which stage the battle boots on before the stage select has run.
         * A build-time define rather than an environment variable: KOS ships
         * an empty environ (kernel/libc/newlib/newlib_environ.c), so getenv
         * is NULL on the target whatever the host sets. Looking at a newly
         * exported stage means skipping the menus too, since the stage
         * select overwrites the battle's gkind the moment it runs:
         *
         *   scripts/ctr.sh run make -C src/game/ssb64 EXTRA_CFLAGS=\
         *       "-DDB_BOOT_STAGE=nGRKindSector -DDB_BOOT_SCENE=nSCKindVSBattle"
         */
#ifdef DB_SOAK
        db_soak_deal();
#endif
        if (grStageFileName(gBootStage) == NULL)
        {
            /* Returning -1 out of main would leave the
             * machine on a black screen. The facility is not the
             * game, so it gives up on its own boot state instead and
             * leaves the chain to start at the title, where the stage
             * select will offer whatever packs the build does have. */
            dbglog(DBG_ERROR, "db: no pack for boot stage %d -- "
                   "starting at the title instead\n", (int)gBootStage);
            return;
        }
        gSCManagerSceneData.gkind = gBootStage;
        /* where the stage select puts its cursor when the character select
         * sends the chain to it (mnMapsInitVars reads this, as the VS
         * chain's last stage) */
        gSCManagerSceneData.maps_vsmode_gkind = gBootStage;

        /* A battle entered without a stage select behind it has had no
         * stage bound, and scVSBattleStartScene's own acquire happens
         * after the debug hook this file installs has already read
         * gGRStages. */
        if (grStageAcquire(gBootStage) == NULL)
        {
            dbglog(DBG_ERROR, "db: cannot load boot stage %d -- "
                   "starting at the title instead\n", (int)gBootStage);
            return;
        }
        /* Bind its collision, the way mpCollisionInitGroundData does
         * inside the scene -- from tables already in RAM rather than a
         * ROM file, so it happens before the scene rather than in it.
         * The music is the scene's own line and stays there. */
        stage_bind(gGRStages[gBootStage]);

        /* A controller read that feeds port 1 a pattern when only one
         * pad is connected, installed on the whole menu chain plus the
         * battle itself -- gated the same way as everything else in
         * this block, so a plain boot never sees a phantom second pad
         * fighting a real one. */
        dSCVSBattleTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNVSResultsTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNTitleTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNModeSelectTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMN1PModeTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNPlayers1PTrainingTaskmanSetup.scene_setup.func_controller = db_func_controller;
    dSC1PTrainingModeTaskmanSetup.scene_setup.func_controller = db_func_controller;
    dMNPlayers1PGameTaskmanSetup.scene_setup.func_controller = db_func_controller;
    dMNPlayers1PBonusTaskmanSetup.scene_setup.func_controller = db_func_controller;
    dSC1PChallengerTaskmanSetup.scene_setup.func_controller = db_func_controller;
    dGM1PStageClearTaskmanSetup.scene_setup.func_controller = db_func_controller;
    dSC1PIntroTaskmanSetup.scene_setup.func_controller = db_func_controller;
    dSC1PGameTaskmanSetup.scene_setup.func_controller = db_func_controller;
    dSC1PBonusStageTaskmanSetup.scene_setup.func_controller = db_func_controller;
    dMN1PContinueTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNVSModeTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNVSOptionsTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNPlayersVSTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNMapsTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNDataTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNVSRecordTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNCharactersTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNSoundTestTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNOptionTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNScreenAdjustTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNBackupClearTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNCongraTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNNoControllerTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dDCMemCardTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dSCAutoDemoTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dSCExplainTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVEndingTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningRoomTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningPortraitsTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningMarioTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningDonkeyTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningSamusTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningLinkTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningYoshiTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningKirbyTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningFoxTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningPikachuTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningRunTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningCliffTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningYamabukiTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningJungleTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningYosterTaskmanSetup.scene_setup.func_controller = db_func_controller;
        mvOpeningSectorTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningStandoffTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningClashTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMVOpeningNewcomersTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dMNStartupTaskmanSetup.scene_setup.func_controller = db_func_controller;
        dSCStaffrollTaskmanSetup.scene_setup.func_controller = db_func_controller;

        /* The one thing the game's own boot leaves alone that a build
         * naming a scene must not: DB_BOOT_SCENE is where the run starts.
         * The title is the game's, and the default here. */
        gSCManagerSceneData.scene_curr = DB_BOOT_SCENE;

        /* Character Data is the one screen in the chain that reads
         * scene_prev to decide what it is showing (mncharacters.c's own
         * mnCharactersInitVars): scene_prev == nSCKindData means "a
         * player opened DATA and picked this tab," anything else means
         * "the auto-demo landed here," which reads
         * gSCManagerSceneData.demo_fkind instead of
         * gSCManagerBackupData.characters_fkind -- and a direct
         * DB_BOOT_SCENE jump leaves demo_fkind at dSCManagerDefaultSceneData's
         * own boot value, not a real fighter kind. The game is never
         * reached any other way, so this is the one true scene_prev for
         * it, the same way the probe's controller feed stands in for a
         * player's own picks rather than the game's boot state. */
        if (DB_BOOT_SCENE == nSCKindCharacters)
        {
            gSCManagerSceneData.scene_prev = nSCKindData;
#ifdef DB_CHARACTERS_FKIND
            gSCManagerBackupData.characters_fkind = DB_CHARACTERS_FKIND;
#endif
        }

        /* Options reads scene_prev too (mnoption.c's own mnOptionInitVars):
         * nSCKindScreenAdjust or nSCKindBackupClear puts the cursor on
         * that tab, anything else lands on SOUND. Both child screens are
         * ported now, but a direct DB_BOOT_SCENE jump to
         * nSCKindOption itself still must not leave scene_prev at
         * whatever dSCManagerDefaultSceneData's own boot value is -- set
         * it to the mode select, the same "came from outside" value the
         * game's own OPTION entry sets before the jump. Neither child's
         * own InitVars reads scene_prev at all (mnBackupClearInitVars,
         * mnScreenAdjustInitVars), so a direct boot into either of
         * *them* needs no fix of its own. */
        if (DB_BOOT_SCENE == nSCKindOption)
        {
            gSCManagerSceneData.scene_prev = nSCKindModeSelect;
        }

        /* The same class of gap again: Training mode does
         * not read gSCManagerTransferBattleState at all --
         * sc1PTrainingModeInitVars builds its own SCBattleState out of
         * gSCManagerSceneData.training_man_fkind and _com_fkind, the
         * pair mnPlayers1PTrainingSetSceneData writes when the select
         * above it is left. A direct jump skips that select, so the two
         * kinds are dealt here from the same DB_BOOT_P1_KIND/P2_KIND
         * the battle's slots take, and the human is P1. Without this
         * both kinds are dSCManagerDefaultSceneData's boot value and
         * the scene spawns two of whatever that happens to be. */
        if (DB_BOOT_SCENE == nSCKind1PTrainingMode)
        {
            gSCManagerSceneData.scene_prev = nSCKindPlayers1PTraining;
            gSCManagerSceneData.player = 0;
            gSCManagerSceneData.training_man_fkind = DB_BOOT_P1_KIND;
            gSCManagerSceneData.training_com_fkind = DB_BOOT_P2_KIND;
            gSCManagerSceneData.training_man_costume = 0;
            gSCManagerSceneData.training_com_costume = 0;
        }

        /* The 1P game's select: mnPlayers1PGameInitPlayer puts the puck
         * on gSCManagerSceneData.fkind, the fighter the select last
         * chose, and shows that fighter's model. A direct jump has chosen
         * nothing, so the select opens empty; DB_BOOT_P1_KIND stands in. */
        if (DB_BOOT_SCENE == nSCKind1PGamePlayers)
        {
            gSCManagerSceneData.player = 0;
            gSCManagerSceneData.fkind = DB_BOOT_P1_KIND;
            gSCManagerSceneData.costume = 0;
        }

        /* The same gap once more, for the ladder's VS card.
         * sc1PIntroInitVars reads gSCManagerSceneData.spgame_stage --
         * which rung the card is for -- and the human's slot out of
         * gSCManager1PGameBattleState, and both are written by scenes a
         * direct jump skips: the rung by sc/sc1pmode/sc1pmanager.c and
         * the slot by the 1P game's select. So the rung is dealt from
         * DB_BOOT_1P_STAGE and the human from DB_BOOT_P1_KIND, and the
         * two ally slots from DB_BOOT_P2_KIND so the Mario Bros. and
         * Giant DK rungs have someone to name. Without this the card is
         * always rung 0 with whatever fighter the defaults carry.
         *
         * The router is ported, and nSCKind1PGame does not
         * start the rung -- it starts the whole ladder, from
         * DB_BOOT_1P_STAGE. The router then fills the human's slot
         * itself, out of gSCManagerSceneData.fkind (sc1pmanager.c:278-
         * 287), so that is the field DB_BOOT_P1_KIND has to reach for
         * the ladder; the battle-state writes below still stand because
         * a jump straight to the card, which the router never ran,
         * reads them and nothing fills them. The ally slots the router
         * writes only on the Mario Bros. and Giant DK rungs, and only
         * with fighters it shuffles -- a probe's DB_BOOT_P2_KIND is
         * overwritten there on purpose. */
        if (DB_BOOT_SCENE == nSCKind1PIntro || DB_BOOT_SCENE == nSCKind1PGame)
        {
            gSCManagerSceneData.scene_prev = nSCKind1PGamePlayers;
            gSCManagerSceneData.spgame_stage = DB_BOOT_1P_STAGE;
            gSCManagerSceneData.player = 0;
            gSCManagerSceneData.fkind = DB_BOOT_P1_KIND;
            gSCManagerSceneData.costume = DB_BOOT_P1_COSTUME;
            gSCManagerSceneData.ally_players[0] = 1;
            gSCManagerSceneData.ally_players[1] = 2;

            /* sc1pmanager.c:278-287 sc1PManagerStartNewGame, the whole
             * of the human's slot and not just its fighter. The card
             * reads only fkind and got away with less;
             * the rung does not -- sc1PGameSetupStageAll sets every
             * OTHER slot's pkind to nFTPlayerKindNot and leaves the
             * human's alone, so a slot the router never filled reads
             * as nFTPlayerKindNot, the spawn loop skips it, and the
             * rung runs with no fighters at all. Found by this step's
             * -DDB_PVR_BUDGET probe: zero triangles a frame, a match
             * that counted its clock down over an empty stage, and no
             * "Mario: ... pack loaded" line anywhere in the log. */
            gSCManager1PGameBattleState.players[0].fkind = DB_BOOT_P1_KIND;
            gSCManager1PGameBattleState.players[0].costume = DB_BOOT_P1_COSTUME;
            gSCManager1PGameBattleState.players[0].shade = 0;
            gSCManager1PGameBattleState.players[0].pkind = nFTPlayerKindMan;
            gSCManager1PGameBattleState.players[0].handicap = FTCOMMON_HANDICAP_DEFAULT;
            gSCManager1PGameBattleState.players[0].team = 0;
            gSCManager1PGameBattleState.players[0].color = 0;
            gSCManager1PGameBattleState.players[0].tag = 0;
            gSCManager1PGameBattleState.players[0].stock_count =
                gSCManagerBackupData.spgame_stock_count;
            gSCManager1PGameBattleState.players[0].is_spgame_enemy = FALSE;

            gSCManager1PGameBattleState.players[1].fkind = DB_BOOT_P2_KIND;
            gSCManager1PGameBattleState.players[1].costume = 0;
            gSCManager1PGameBattleState.players[1].shade = 0;
            gSCManager1PGameBattleState.players[2].fkind = DB_BOOT_P2_KIND;
            gSCManager1PGameBattleState.players[2].costume = 0;
            gSCManager1PGameBattleState.players[2].shade = 0;
        }

        /* The same gap once more, for the bonus stages.
         * sc1PBonusStageInitVars and sc1PBonusStageStartScene both read
         * which SCENE sent them and take the fighter from a different
         * field depending on the answer; a direct jump has been sent by
         * nothing. So a probe is dealt a PRACTICE run -- scene_prev is
         * the Break the Targets select -- which is the untimed path, the
         * one whose clock counts up and whose own six-digit timer this
         * scene builds. DB_BOOT_P1_KIND is both the human AND the
         * course: `gkind = fkind + nGRKindBonus1Start` is what makes the
         * twelve courses one per fighter, so naming the fighter names
         * the stage and there is no DB_BOOT_STAGE to set.
         *
         * WHICH GAME is -DDB_BOOT_BONUS2: the select the
         * probe is dealt is the only thing that says so, because the two
         * games are one scene and the range gkind lands in is what
         * decides. Without it, Break the Targets. */
        if (DB_BOOT_SCENE == nSCKind1PBonusStage)
        {
#ifdef DB_BOOT_BONUS2
            gSCManagerSceneData.scene_prev = nSCKind1PBonus2Players;
#else
            gSCManagerSceneData.scene_prev = nSCKind1PBonus1Players;
#endif
            gSCManagerSceneData.player = 0;
            gSCManagerSceneData.bonus_fkind = DB_BOOT_P1_KIND;
            gSCManagerSceneData.bonus_costume = 0;
        }

        /* The same gap once more, and the smallest of them, for
         * "CHALLENGER APPROACHING!". sc1PChallengerInitVars
         * reads gSCManagerSceneData.challenger_fkind and nothing else;
         * it is written by sc/sc1pmode/sc1pmanager.c, the file that
         * decides a challenger has been earned, which is not ported.
         * So the challenger is dealt from DB_BOOT_P1_KIND -- the card
         * shows one fighter and it is the opponent, not the human, so
         * the knob that names P1 names the silhouette here. The ladder
         * only ever challenges with Luigi, Ness, Jigglypuff or Captain
         * Falcon; a probe may name any of the twelve. */
        if (DB_BOOT_SCENE == nSCKind1PChallenger)
        {
            gSCManagerSceneData.scene_prev = nSCKind1PGame;
            gSCManagerSceneData.challenger_fkind = DB_BOOT_P1_KIND;
        }

        /* The ending poses the run's own fighter out of
         * gSCManager1PGameBattleState (mvEndingInitVars), which only a
         * 1P run writes. A direct boot is dealt DB_BOOT_P1_KIND there. */
        if (DB_BOOT_SCENE == nSCKindEnding)
        {
            gSCManager1PGameBattleState.players[gSCManagerSceneData.player].fkind = DB_BOOT_P1_KIND;
            gSCManager1PGameBattleState.players[gSCManagerSceneData.player].costume = 0;
            gSCManager1PGameBattleState.players[gSCManagerSceneData.player].shade = 0;
        }

        /* The score screen after a rung. This one reads
         * more of gSCManagerSceneData than any other scene in the
         * ladder -- the rung, the seconds left, the running score, the
         * three bonus-flag words and, for a bonus stage, how many tasks
         * were finished -- and the whole of it is written by the rung
         * and by sc/sc1pmode/sc1pmanager.c, which a direct jump skips.
         * The damage figure comes from somewhere else again, the
         * human's own battle-state slot, which the block above already
         * fills for nSCKind1PGame.
         *
         * So a probe is dealt a plausible finished rung: DB_BOOT_1P_STAGE
         * says which, and the numbers below are what one would leave.
         * DB_BOOT_1P_BONUS is the low word of the bonus mask, so a probe
         * can ask for the bonus table by name: the default lights the
         * first four bits, which is four rows and enough to see the
         * table build and the score count up. Without any of this the
         * screen is rung 0 with a score of zero and no bonuses, which
         * draws correctly and shows almost nothing. */
        if (DB_BOOT_SCENE == nSCKind1PScoreUnk ||
            DB_BOOT_SCENE == nSCKind1PStageClear)
        {
            gSCManagerSceneData.scene_prev = nSCKind1PGame;
            gSCManagerSceneData.spgame_stage = DB_BOOT_1P_STAGE;
            gSCManagerSceneData.player = 0;
            gSCManagerSceneData.spgame_time_limit = 3;
            gSCManagerSceneData.spgame_time_remain = 87;
            gSCManagerSceneData.spgame_score = 123400;
            gSCManagerSceneData.bonus_tasks_complete = 10;
            gSCManagerSceneData.bonus_get_mask[0] = DB_BOOT_1P_BONUS;
            gSCManagerSceneData.bonus_get_mask[1] = 0;
            gSCManagerSceneData.bonus_get_mask[2] = 0;
            gSCManagerBackupData.spgame_difficulty = 2;
            gSCManager1PGameBattleState.players[0].fkind = DB_BOOT_P1_KIND;
            gSCManager1PGameBattleState.players[0].pkind = nFTPlayerKindMan;
            gSCManager1PGameBattleState.players[0].total_damage_given = 432;
        }

        /* The Continue prompt, the other end of the same
         * rung and the same gap in miniature. mnPlayers1PGameContinue
         * reads exactly two things a direct jump would leave at zero:
         * the human's slot (fkind/costume/shade, mn1pcontinue.c:1071-1073,
         * which is what the fighter slumped in the spotlight is) and
         * gSCManagerSceneData.spgame_score, which the scene halves when
         * the player says YES and counts on screen either way. Both are
         * the router's to fill, and a direct boot skips the router. A
         * score of zero draws correctly and shows nothing move, so a
         * probe is dealt the same plausible finished rung the score
         * screen above is. */
        if (DB_BOOT_SCENE == nSCKind1PContinue)
        {
            gSCManagerSceneData.scene_prev = nSCKind1PGame;
            gSCManagerSceneData.player = 0;
            gSCManagerSceneData.spgame_score = 123400;
            gSCManager1PGameBattleState.players[0].fkind = DB_BOOT_P1_KIND;
            gSCManager1PGameBattleState.players[0].costume = 0;
            gSCManager1PGameBattleState.players[0].shade = 0;
            gSCManager1PGameBattleState.players[0].pkind = nFTPlayerKindMan;
        }

        /* The bonus practice selects. A direct jump needs
         * nothing of gSCManagerSceneData that the scene does not read
         * off scene_curr itself, so unlike every arm above this one
         * fills no gap -- it deals a save. A fresh one has neither bonus
         * game finished by anybody, and that is a real screen, so it is
         * the default; -DDB_BOOT_BONUS_RECORDS=1 asks for the other one,
         * where every fighter has broken all ten targets and boarded all
         * ten platforms, which is the only way to see the best-TIME arm
         * of the records panel and the TOTAL TIME line at all. The times
         * are plausible rather than real: a tic count that reads as a
         * different number of seconds per fighter, so a panel showing
         * the wrong fighter's record would be visible. */
        if (DB_BOOT_BONUS_RECORDS &&
            (DB_BOOT_SCENE == nSCKind1PBonus1Players ||
             DB_BOOT_SCENE == nSCKind1PBonus2Players))
        {
            s32 f;

            for (f = nFTKindPlayableStart; f <= nFTKindPlayableEnd; f++)
            {
                LBBackup1PRecord *rec = &gSCManagerBackupData.spgame_records[f];

                rec->bonus1_task_count = SCBATTLE_BONUSGAME_TASK_MAX;
                rec->bonus2_task_count = SCBATTLE_BONUSGAME_TASK_MAX;
                rec->bonus1_time = (u32)(900 + (f * 60));
                rec->bonus2_time = (u32)(1500 + (f * 60));
            }
        }

        /* The same class of gap as both cases just above, found by the
         * first -DDB_BOOT_SCENE=nSCKindAutoDemo probe:
         * scAutoDemoInitDemo reads gSCManagerSceneData.demo_fkind[0]/[1]
         * (scautodemo.c:631, verbatim), and on real hardware that pair
         * is always real by the time AutoDemo runs -- mnTitleProceedDemoNext
         * sets it during the title's own idle-demo cycle, several
         * scenes before AutoDemo is ever reached, and mncharacters.c's
         * own idle-timeout branch here only ever reads it, never sets
         * it (this file's earlier comment on nSCKindCharacters says the
         * same about scene_prev). mnTitleProceedDemoNext is not ported
         * (it is cut), so nothing in
         * this port ever writes demo_fkind before a direct boot into
         * AutoDemo -- it stays dSCManagerDefaultSceneData's own
         * verbatim ROM default, { nFTKindNull, nFTKindNull }, and
         * scAutoDemoInitDemo's ftManagerSetupFilesAllKind(nFTKindNull)
         * aborts the console ("no pack loaded for fighter kind 28").
         * Mario and Luigi stand in for a real player's picks the same
         * way this file's controller feed already does. */
        if (DB_BOOT_SCENE == nSCKindAutoDemo)
        {
            gSCManagerSceneData.demo_fkind[0] = nFTKindMario;
            gSCManagerSceneData.demo_fkind[1] = nFTKindLuigi;
        }

        /* Not the same gap as the two above -- scExplainSetBattleState
         * hardcodes Mario/Luigi itself, so a direct DB_BOOT_SCENE jump
         * into Explain reads nothing an upstream menu would have set --
         * but scExplainUpdatePhase's own 23rd-phase exit
         * (sc/sccommon/scexplain.c, verbatim) sets scene_curr to
         * nSCKindCharacters directly, without going back through
         * mnTitleProceedDemoNext (which is what sets scene_prev to
         * nSCKindData or leaves it for "the auto-demo landed here" on
         * real hardware -- see this file's own nSCKindCharacters comment
         * above). scene_prev will read nSCKindExplain there, which
         * mnCharactersInitVars treats the same as "the auto-demo landed
         * here": it reads demo_fkind, not gSCManagerBackupData.
         * characters_fkind. Set proactively here so a probe or host test
         * that runs Explain all the way to that exit does not hit the
         * same "no pack loaded for fighter kind 28" abort the AutoDemo
         * gap above did. */
        if (DB_BOOT_SCENE == nSCKindExplain)
        {
            gSCManagerSceneData.demo_fkind[0] = nFTKindMario;
            gSCManagerSceneData.demo_fkind[1] = nFTKindLuigi;
        }
    }

    /* The title is the other screen that reads scene_prev
     * -- nSCKindOpeningNewcomers means "the opening movie just ended,"
     * and the logo animation, the slash and the logo fire exist only on
     * that visit. The movie itself takes 80 s to reach the title from
     * nSCKindOpeningRoom, so a probe of that path names the pair:
     * -DDB_BOOT_SCENE=nSCKindTitle -DDB_BOOT_SCENE_PREV=nSCKindOpeningNewcomers.
     * Outside the block above on purpose: that block is gated on the
     * boot scene NOT being the title, and this knob's first use is the
     * title. With the scripted feed off, the title runs exactly as the
     * movie leaves it and goes on to How to Play by itself at tic 650.
     * Any scene's scene_prev can be set this way.
     *
     * It sets scene_curr too, and that is the whole
     * reason the pair works: the plain boot is the cart's own
     * (nSCKindStartup and the movie, src/game/ssb64/main.c), so a probe
     * naming DB_BOOT_SCENE=nSCKindTitle would otherwise sit through the
     * eighty seconds of movie the pair exists to skip -- the gated block
     * above, which is where scene_curr is normally written, is skipped
     * for exactly that value. Naming DB_BOOT_SCENE and not this knob is
     * unaffected: the block above writes the same field first.
     *
     * Booting the title with no movie before it is
     * therefore -DDB_BOOT_SCENE_PREV=nSCKindStartup and nothing else:
     * DB_BOOT_SCENE defaults to the title, and that is the scene_prev
     * the title screen has with no movie. */
#ifdef DB_BOOT_SCENE_PREV
    gSCManagerSceneData.scene_prev = DB_BOOT_SCENE_PREV;
    gSCManagerSceneData.scene_curr = DB_BOOT_SCENE;
#endif

    /* The debug GObj this file builds stays unconditional, so a
     * developer walking to any battle through a plain boot still gets
     * the serial log -- which is the whole of what it carries by
     * default, and the only part that costs a build you play nothing:
     * it writes to a wire nobody watching a TV can see. The two parts
     * that DO cost something each hang off their own define: the cliff
     * warp takes a button the game plays with (-DDB_CLIFF_WARP), and
     * the collision overlay draws over the picture you would be
     * judging (-DDB_COLLISION_OVERLAY). The GObj is unconditional;
     * those two affordances are not. */
    gSCVSBattleFuncDebug = db_battle_start;

    {
        kthread_t *watch = DC_THD_CREATE(1, db_hang_watch, NULL,
                                           "db hang watch");

        if (watch != NULL)
            thd_set_label(watch, "db hang watch");
    }
#ifdef DB_IO_STRESS
    asset_io_stress_start();
#endif
}
