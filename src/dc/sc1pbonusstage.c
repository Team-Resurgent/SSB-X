/* sc1pbonusstage.c -- see sc1pbonusstage.h. Every function is
 * sc/sc1pmode/sc1pbonusstage.c's by name and body unless marked
 * DIVERGES or CUT; the line numbers are the decomp's. */
#include "sc1pbonusstage.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"
#include "ftcommon.h"
#include "gmcommon.h"
#include "gmcamera.h"
#include "stage.h"
#include "lbpartex.h"
#include "fighter.h"
#include "objmodel.h"
#include "efmanager.h"
#include "ifcommon.h"
#include "itemmodel.h"
#include "itempack.h"
#include "wpattrs.h"            /* wpManagerPreloadModels */
#include "assetroot.h"

#include <sys/debug.h>
#include <sys/controller.h>
#include <sys/utils.h>
#include <sys/objdisplay.h>
#include <sys/objanim.h>
#include <sc/scdef.h>
#include <sc/sc1pmode/sc1pmanager.h>
#include <sc/scsubsys/scsubsys.h>
#include <gm/gmsound.h>
#include <if/ifcommon.h>
#include <if/interface.h>
#include <it/item.h>
#include <gr/ground.h>
#include <gr/grvars.h>
#include <mp/mpdef.h>           /* nMPMaterialDetect, nMPYakumonoStatusNone */
#include <mp/mpcollision.h>     /* the platform lines and their yakumono DObjs */
#include <ef/efparticle.h>
#include <wp/wpmanager.h>       /* wpManagerAllocWeapons */
#include <lb/lbdef.h>
#include <PR/os.h>

/* ---- the pools --------------------------------------------------------
 *
 * src/dc/scvsbattle.c's, trimmed where this scene is smaller and grown
 * where it is not. ONE fighter instead of four, so the XObj/AObj/DObj
 * counts a roster costs are a quarter of a VS battle's -- but the
 * SObjs are this scene's own worst case and larger: ten task sprites,
 * six timer digits, two timer symbols and the battle HUD on top. The
 * ten targets are ITEMS, which is where the GObj count goes. The scene
 * heap is 1280 KB and these come out of it; syTaskmanSetupPools prints
 * the high-water mark. */
#define SC1PBONUSSTAGE_GOBJS       48
#define SC1PBONUSSTAGE_GOBJPROCS   48
#define SC1PBONUSSTAGE_XOBJS      192
#define SC1PBONUSSTAGE_AOBJS      384
#define SC1PBONUSSTAGE_MOBJS       32
#define SC1PBONUSSTAGE_DOBJS       96
#define SC1PBONUSSTAGE_SOBJS      128
#define SC1PBONUSSTAGE_COBJS        6

/* n_env.c's stop-a-sound and start-a-voice-script (src/dc/sysshim.c) */
void func_800266A0_272A0(void);
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

void ftParamInitPlayerBattleStats(s32 player, GObj *fighter_gobj);
void ftParamInitGame(void);

/* The one sprite this scene reaches by offset: llSC1PStageClear3Target-
 * Sprite, 0x1d0 in relocData file 151, the little target head the task
 * row draws once per remaining target. The file is already a bank --
 * romdisk/sc1pstageclear3.spr, shared with the score screen,
 * whose OBJECTS row draws the same head.
 *
 * The timer's own sprites are not here: they come out of
 * gGMCommonFiles[3] through src/dc/ifcommon.c's own
 * dIFCommonTimerDigitSpriteOffsets, where the decomp names them
 * llIFCommonTimerDigit0Sprite (entry 0), ...SymbolSecSprite (11) and
 * ...SymbolCSecSprite (12). */
#define SC1PBONUSSTAGE_BANK_OBJECTS "sc1pstageclear3.spr"
#define llSC1PStageClear3TargetSprite 0x1d0
#define llSC1PStageClear3PlatformSprite 0xc0
#define IFCOMMON_TIMER_DIGIT0 0
#define IFCOMMON_TIMER_SYMBOL_SEC 11
#define IFCOMMON_TIMER_SYMBOL_CSEC 12

/* The game reaches a sprite as a file's base plus a link offset; the
 * port's file is a bank and the offset is the number (src/dc/sprite.h),
 * as every ported scene does it. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* the decomp's share of unused locals */
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* CUT: dSC1PBonusStageTargetDescs, dSC1PBonusStageBumperDescs,
 * dSC1PBonusStagePlatformDescs and dSC1PBonusStageBoardedPlatformDescs
 * (sc1pbonusstage.c:19-230). All four are tables of RELOC LABELS --
 * `&llGRBonus1MarioMapTargetsDObjDesc` and its like -- which the port
 * has no reloc runtime and no linker symbols for. Each is replaced where
 * its own reader is, and every one of the four by a pack:
 *   - TargetDescs   -> the .stg's BTG1 block (sc1PBonusStageMakeTargets);
 *   - BumperDescs   -> its BMP1 block (sc1PBonusStageMakeBumpers);
 *   - Platform- and BoardedPlatformDescs -> bonus2plat.pak, by the six
 *     names in dSC1PBonusStagePlatformNames below. */

/* sc1pbonusstage.c:233-241 dSC1PBonusStageTimerUnitLengths, verbatim.
 * Six units, largest first, so a division chain turns a tic count into
 * six digits: ten minutes, one minute, ten seconds, one second, a tenth
 * and -- the decomp's own comment -- a hundredth that is really
 * 277/500 of a tic, precision the original data lost. */
f32 dSC1PBonusStageTimerUnitLengths[/* */] =
{
    I_MIN_TO_TICS(10),
    I_MIN_TO_TICS(1),
    I_SEC_TO_TICS(10),
    I_SEC_TO_TICS(1),
    I_SEC_TO_TICS(0.1),
    277.0F / 500.0F
};

// 0x8018F014
s32 dSC1PBonusStageTimerDigitPositions[/* */] = { 207, 222, 240, 255, 273, 288 };

// 0x8018F02C
s32 dSC1PBonusStagePlayerInterfacePositions[/* */] = { 55, 55, 55, 55 };

// 0x8018F03C
SYColorRGBA dSC1PBonusStageFadeColor = { 0x00, 0x00, 0x00, 0x00 };

/* CUT: dSC1PBonusStageLights1 and dSC1PBonusStageDisplayList
 * (sc1pbonusstage.c:253-261), the ambient/diffuse pair the pre-render
 * hands the RSP. src/dc/scvsbattle.c cuts the identical pair for the
 * identical reason -- the port's lighting is the PVR back end's.
 *
 * CUT: dSC1PBonusStageVideoSetup, the N64's video mode --
 * sc1PBonusStageStartScene says why.
 *
 * 0x8018F09C dSC1PBonusStageTaskmanSetup. Every divergence here is
 * src/dc/scvsbattle.c's dSCVSBattleTaskmanSetup's, for that file's
 * reasons: the DL buffers, the graphics arena and the RDP output buffer
 * are zeroed (no RSP, no RDP), func_lights is NULL, arena_start is NULL
 * (keep the region syTaskmanMakeGeneralHeap already made) and the
 * matrix function list is NULL (the matrix kinds are a switch in
 * objdisplay.c). The pool counts are this file's own; the decomp
 * carries zero for every one of them. */
SYTaskmanSetup dSC1PBonusStageTaskmanSetup =
{
    {
        0,                          /* flags */
        sc1PBonusStageFuncUpdate,   /* update function */
        scManagerFuncDraw,          /* frame draw function */
        NULL,                       /* allocatable memory pool start */
        0,                          /* allocatable memory pool size */
        1,                          /* ??? */
        2,                          /* number of contexts? */
        0, 0, 0, 0,                 /* the four DL buffer sizes */
        0,                          /* graphics heap size */
        2,                          /* ??? */
        0,                          /* RDP output buffer size */
        NULL,                       /* pre-render function */
        syControllerFuncRead,       /* controller I/O function */
    },

    0,                              /* number of GObjThreads */
    sizeof(u64) * 192,              /* thread stack size */
    0,                              /* number of thread stacks */
    0,                              /* ??? */
    SC1PBONUSSTAGE_GOBJPROCS,
    SC1PBONUSSTAGE_GOBJS,   sizeof(GObj),
    SC1PBONUSSTAGE_XOBJS,
    NULL,                           /* matrix function list */
    NULL,                           /* DObjVec eject function */
    SC1PBONUSSTAGE_AOBJS,
    SC1PBONUSSTAGE_MOBJS,
    SC1PBONUSSTAGE_DOBJS,   sizeof(DObj),
    SC1PBONUSSTAGE_SOBJS,   sizeof(SObj),
    SC1PBONUSSTAGE_COBJS,   sizeof(CObj),

    sc1PBonusStageFuncStart         // Task start function
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* 0x8018F1A0 gSC1PBonusStageItemFile is NOT here: it is the ONE thing
 * sc1PBonusStageBonus1LoadFile made, the target's ITAttributes DMA'd
 * off the heap, and  13k put those in the item pack under the
 * decomp's own key instead (src/dc/ittarget.c's divergence). Nothing
 * else in the game reads the pointer. */

// 0x8018F1B0
SCBattleState sSC1PBonusStageBattleState;

// 0x8018F3A0
u8 sSC1PBonusStageTimerDigits[6];

// 0x8018F3A8
sb32 sSC1PBonusStageIsTimeUp;

/* CUT: sSC1PBonusStageStatusBuffer and sSC1PBonusStageForceStatusBuffer
 * (sc1pbonusstagefiles.c:5-8), the reloc loader's bookkeeping --
 * sc1PBonusStageSetupFiles replaces the loader they belong to. */

/* The port's own: the task-row sprite bank, and which stage pack
 * sc1PBonusStageStartScene holds (src/dc/stage.h's grStageAcquire/
 * grStageRelease pair). src/dc/sc1pgame.c:778 has the same two for the
 * same reason. */
static void *sSC1PBonusStageObjectFile;
static SpriteBank sSC1PBonusStageObjectBank;
static u8 sSC1PBonusStageStageHeld;
static s32 sSC1PBonusStageStageKind;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* sc1pbonusstage.c:339-348 func_ovl6_8018D0C0 and func_ovl6_8018D0C8,
 * two empty functions nothing calls, are not ported. */

// 0x8018D0D0
void sc1PBonusStageFuncUpdate(void)
{
    ifCommonBattleUpdateInterfaceAll();
}

/* sc1pbonusstagefiles.c:11-26 sc1PBonusStageSetupFiles 0x8018ED70.
 *
 * CUT: lbRelocInitSetup and its two status buffers, the reloc loader's
 * own bookkeeping. What is NOT a cut is the load beneath it:
 * lbRelocLoadFilesListed(dGMCommonFileIDs, gGMCommonFiles) is the
 * battle HUD's eight sprite banks, and gmCommonLoadFiles is exactly
 * that in the port -- src/dc/sc1pgame.c:873 is the precedent and says
 * what stubbing the whole thing costs (no damage readout, and only a
 * disc probe sees it).
 *
 * The port's own second line: this scene's task-row bank, which the
 * decomp loads inside sc1PBonusStageMakeTargetSprites off the reloc
 * heap. A bank that will not load leaves the pointer NULL, which is
 * what the game's own load-failure path leaves. */
void sc1PBonusStageSetupFiles(void)
{
    gmCommonLoadFiles();

    sSC1PBonusStageObjectFile = NULL;

    if (sprite_bank_load(&sSC1PBonusStageObjectBank,
                         SC1PBONUSSTAGE_BANK_OBJECTS) < 0)
    {
        syDebugPrintf("sc1PBonusStage: no bank for %s\n",
                      SC1PBONUSSTAGE_BANK_OBJECTS);
        return;
    }
    sSC1PBonusStageObjectFile = &sSC1PBonusStageObjectBank;
}

/* sc1pbonusstage.c:357-425 sc1PBonusStageInitVars 0x8018D0F0, verbatim.
 *
 * The scene's whole configuration, and every branch in it is the same
 * question: did the LADDER send us here, or the practice select? From
 * the ladder the fighter and costume are the run's
 * (gSCManagerSceneData.fkind/.costume) and the clock is two minutes
 * unless the rung is untimed; from the practice select they are the
 * select's own pair (.bonus_fkind/.bonus_costume) and the clock counts
 * up forever. `gkind` is the fighter's index added to the range start,
 * which is what makes the twelve courses one per fighter. */
void sc1PBonusStageInitVars(void)
{
    s32 player;
    s32 fkind;

    gSCManagerSceneData.is_reset = FALSE;

    sSC1PBonusStageBattleState = dSCManagerDefaultBattleState;
    gSCManagerBattleState = &sSC1PBonusStageBattleState;

    gSCManagerBattleState->game_type = nSCBattleGameTypeBonus;
    gSCManagerBattleState->game_rules = SCBATTLE_GAMERULE_BONUS | SCBATTLE_GAMERULE_TIME;
    gSCManagerBattleState->is_show_score = FALSE;
    gSCManagerBattleState->pl_count = 1;
    gSCManagerBattleState->cp_count = 0;

    if (gSCManagerSceneData.scene_prev == nSCKind1PGame)
    {
        fkind = gSCManagerSceneData.fkind;

        gSCManagerBattleState->time_limit = SCBATTLE_TIMELIMIT_INFINITE;

        if (gSCManagerSceneData.spgame_stage == nSC1PGameStageBonus1)
        {
            if (gSCManagerSceneData.spgame_time_limit != SCBATTLE_TIMELIMIT_INFINITE)
            {
                gSCManagerBattleState->time_limit = 2;
            }
            gSCManagerBattleState->gkind = fkind + nGRKindBonus1Start;
        }
        else
        {
            if (gSCManagerSceneData.spgame_time_limit != SCBATTLE_TIMELIMIT_INFINITE)
            {
                gSCManagerBattleState->time_limit = 2;
            }
            gSCManagerBattleState->gkind = fkind + nGRKindBonus2Start;
        }
    }
    else
    {
        fkind = gSCManagerSceneData.bonus_fkind;

        gSCManagerBattleState->time_limit = SCBATTLE_TIMELIMIT_INFINITE;

        if (gSCManagerSceneData.scene_prev == nSCKind1PBonus1Players)
        {
            gSCManagerBattleState->gkind = fkind + nGRKindBonus1Start;
        }
        else gSCManagerBattleState->gkind = fkind + nGRKindBonus2Start;
    }
    for (player = 0; player < ARRAY_COUNT(gSCManagerBattleState->players); player++)
    {
        if (player == gSCManagerSceneData.player)
        {
            gSCManagerBattleState->players[player].pkind = nFTPlayerKindMan;
            gSCManagerBattleState->players[player].fkind = fkind;

            if (gSCManagerSceneData.scene_prev == nSCKind1PGame)
            {
                gSCManagerBattleState->players[player].costume = gSCManagerSceneData.costume;
            }
            else gSCManagerBattleState->players[player].costume = gSCManagerSceneData.bonus_costume;

            gSCManagerBattleState->players[player].color = player;
        }
        else gSCManagerBattleState->players[player].pkind = nFTPlayerKindNot;
    }
}

/* sc1pbonusstage.c:428-431 sc1PBonusStageBonus1LoadFile 0x8018D330 is
 * NOT ported. Its one line DMAs relocData 253 (ITBonus1ObjectHeader)
 * onto the scene heap and hands the pointer to gSC1PBonusStageItemFile,
 * which exists only so the target's ITDesc can name it.  13k
 * put that table in the item pack under the decomp's own key instead
 * (src/dc/ittarget.c's divergence), so there is no file to load and no
 * pointer to keep. */

/* sc1pbonusstage.c:434-467 sc1PBonusStageMakeTargets 0x8018D374, the
 * pack's positions in place of the map file's DObjDesc walk.
 *
 * DIVERGES, and it is grBonus3MakeBumpers' divergence exactly
 * (src/dc/grbonus3.c 13j): the game finds two parallel arrays
 * -- a DObjDesc array and an AnimJoint array -- at a reloc label inside
 * the COURSE'S OWN layer file, reached by subtracting the course's
 * `start` from gMPCollisionGroundData->gr_desc[1].dobjdesc, and walks
 * them to the descs' DOBJ_ARRAY_MAX terminator. The port reads the ten
 * positions and their scripts out of the pack's BTG1 block
 * (tools/export/ssb_stageexport.py 13b). The exporter skips the
 * array's first entry exactly the way this function's `dobjdesc++,
 * anim_joints++` skips it, so `target_count` IS the game's own
 * iteration count and the order is the array's.
 *
 * THE COUNT CHECK IS KEPT AS THE DECOMP WROTE IT, spin and all: a
 * course carrying anything but SCBATTLE_BONUSGAME_TASK_MAX targets
 * hangs the console, because the task row, the backup record and the
 * COMPLETE! test all assume the ten. The exporter refuses to write a
 * course that has any other number, so this arm is a fact rather than a
 * risk -- the same trade grBonus3TaruBombMakeActor's "Too many
 * barrels!" makes. One line of it DIVERGES: the decomp's arm is a bare
 * `while (TRUE);`, which on this machine is a frozen console and not
 * one word of diagnosis, so the port spins on
 * scManagerRunPrintGObjStatus instead -- which aborts, naming the
 * collision state and every GObj (src/dc/scmanager.c:174). That is the
 * shape grBonus3TaruBombMakeActor's own kept arm already has.
 *
 * The `#ifdef FT_HOSTTEST` on the script attach is the third of
 * grbonus3.c's divergences, here for its reason: `union AObjEvent32` is
 * eight bytes on x86-64, so the interpreter walks a real script twice
 * as fast as it should and never terminates. */
void sc1PBonusStageMakeTargets(void)
{
    Stage *st = stage_bound();
    GObj *item_gobj;
    Vec3f vel;
    s32 i;

    vel.x = vel.y = vel.z = 0.0F;

    gGRCommonStruct.bonus1.target_count = 0;

    if (st == NULL)
    {
        return;
    }
    for (i = 0; i < (s32)st->target_count; i++)
    {
        AObjEvent32 *anim_joint = NULL;

        /* the decomp's `*anim_joints != NULL` test, one indirection
         * earlier: a still target is -1 here where it is a NULL entry
         * in the map file's parallel array. */
        if ((st->target_anim[i] >= 0) &&
            (st->target_anim[i] < (int8_t)st->target_anim_count))
        {
            anim_joint = st->target_anims[st->target_anim[i]];
        }
        item_gobj = itManagerMakeItemSetupCommon(NULL, nITKindTarget, &st->targets[i], &vel, ITEM_FLAG_PARENT_GROUND);

        if ((anim_joint != NULL) && (item_gobj != NULL))
        {
#ifdef FT_HOSTTEST
            (void)anim_joint;
#else
            gcAddDObjAnimJoint(DObjGetStruct(item_gobj), anim_joint, 0.0F);
            gcPlayAnimAll(item_gobj);
#endif
        }
        gGRCommonStruct.bonus1.target_count++;
    }
    if (gGRCommonStruct.bonus1.target_count != SCBATTLE_BONUSGAME_TASK_MAX)
    {
        syDebugPrintf("Error : not %d targets!\n", SCBATTLE_BONUSGAME_TASK_MAX);

        while (TRUE)
        {
            scManagerRunPrintGObjStatus();
        }
    }
#ifndef FT_HOSTTEST
    /* The port's own line, grbonus3.c's `bonus3:` twin: the ten
     * positions are the pack's and a target that failed to make is
     * silent otherwise. Disc probe: `--serial`, grep "bonus1:". */
    syDebugPrintf("bonus1: %d target(s), %d script(s)\n",
                  (int)st->target_count, (int)st->target_anim_count);
#endif
}

/* sc1pbonusstage.c:470-480 sc1PBonusStageUpdateTargetInterface
 * 0x8018D4C4, verbatim.
 *
 * The task row loses its LAST sprite, not the one that was hit: it
 * walks `target_count` links down the interface GObj's SObj chain and
 * ejects what it lands on. Called after the decrement, so the count is
 * already the new one and the walk stops one short of where it would
 * have. */
void sc1PBonusStageUpdateTargetInterface(void)
{
    SObj *sobj = SObjGetStruct(gGRCommonStruct.bonus1.interface_gobj);
    s32 i;

    for (i = 0; i < gGRCommonStruct.bonus1.target_count; i++)
    {
        sobj = sobj->next;
    }
    gcEjectSObj(sobj);
}

/* sc1pbonusstage.c:483-504 sc1PBonusStageUpdateTargetCount 0x8018D510,
 * verbatim. src/dc/ittarget.c's damage proc is the only caller.
 *
 * The NEW RECORD test is why a practice run and a ladder run announce
 * differently: only a practice run (scene_prev is not nSCKind1PGame)
 * can beat a record, and only if the fighter had already cleared all
 * ten once before. */
void sc1PBonusStageUpdateTargetCount(void)
{
    gGRCommonStruct.bonus1.target_count--;

    sc1PBonusStageUpdateTargetInterface();

    if (gGRCommonStruct.bonus1.target_count == 0)
    {
        if
        (
            (gSCManagerSceneData.scene_prev != nSCKind1PGame) &&
            (gSCManagerBackupData.spgame_records[gSCManagerSceneData.bonus_fkind].bonus1_task_count == SCBATTLE_BONUSGAME_TASK_MAX) &&
            (gSCManagerBattleState->time_passed < gSCManagerBackupData.spgame_records[gSCManagerSceneData.bonus_fkind].bonus1_time)
        )
        {
            ifCommonAnnounceCompleteInitInterface(nSYAudioVoiceAnnounceNewRecord);
        }
        else ifCommonAnnounceCompleteInitInterface(nSYAudioVoiceAnnounceComplete);

        ifCommonBattleEndAddSoundQueueID(nSYAudioFGMBonus1TargetBreak);
    }
}

/* sc1pbonusstage.c:507-510 sc1PBonusStageMakeBonus1Ground 0x8018D5C8,
 * verbatim. It makes no ground GObj of its own -- a Break the Targets
 * course has no hazard, only its ten items. Reached from
 * src/dc/stage.c's ground chain, where the game reaches it through
 * dGRMainSetupProcMakeList. */
void sc1PBonusStageMakeBonus1Ground(void)
{
    sc1PBonusStageMakeTargets();
}

/* ---- Board the Platforms -------------------------------
 *
 * The other game, and a different shape from the target course entirely.
 * A target is an ITEM the scene makes off a list of positions; a
 * platform is a piece of the MAP -- a floor line whose material is
 * nMPMaterialDetect -- and what the scene does is hang a MODEL on the
 * yakumono DObj the collision system already keeps for that line. Board
 * one and the model is swapped for its "Boarded" twin. Nothing counts
 * platforms but the walk over the floor lines, so a course's task count
 * is a property of its COLLISION, not of any table.
 *
 * The six models are shared by all twelve courses, and on this port they
 * are romdisk/bonus2plat.pak, loaded with the course's Stage (
 * 13e). dSC1PBonusStagePlatformDescs' three rows and
 * dSC1PBonusStageBoardedPlatformDescs' three become the six names below,
 * which is how every other pack in this port is reached. */
static const char *const dSC1PBonusStagePlatformNames[/* */] =
{
    "PlatformSmall", "PlatformMedium", "PlatformLarge"
};

static const char *const dSC1PBonusStageBoardedPlatformNames[/* */] =
{
    "BoardedPlatformSmall", "BoardedPlatformMedium", "BoardedPlatformLarge"
};

/* sc1pbonusstage.c:513-516 sc1PBonusStageBonus2LoadFile 0x8018D5E8.
 *
 * DIVERGES: the decomp opens relocData 136 (Bonus2Common) onto the scene
 * heap and hangs it on gGRCommonStruct.bonus2.file, which every platform
 * function then reads offsets out of. The port has those six trees baked
 * into romdisk/bonus2plat.pak, which the course's own Stage opened on
 * its way in -- so there is nothing to load, and what is left is the
 * check that it IS open. A course whose pack is missing would otherwise
 * make its platforms invisible and still count them, which is the worst
 * of the two failures.
 *
 * `gGRCommonStruct.bonus2.file` is left NULL: nothing in the port reads
 * it, and a stale pointer into a heap this scene does not own is worse
 * than a null one. */
void sc1PBonusStageBonus2LoadFile(void)
{
    Stage *st = stage_bound();

    gGRCommonStruct.bonus2.file = NULL;

    if ((st == NULL) ||
        (st->platform_model_count < (int)ARRAY_COUNT(dSC1PBonusStagePlatformNames) +
                                    (int)ARRAY_COUNT(dSC1PBonusStageBoardedPlatformNames)))
    {
        syDebugPrintf("bonus2: no platform models (%d)\n",
                      (st != NULL) ? st->platform_model_count : -1);
    }
}

/* sc1pbonusstage.c:519-537 sc1PBonusStageGetPlatformKind 0x8018D62C,
 * verbatim: which of the three sizes a floor line is, measured off the
 * line itself rather than read from anywhere. 750 and 1050 units are the
 * two cuts. */
s32 sc1PBonusStageGetPlatformKind(s32 line_id)
{
    Vec3f pos_left;
    Vec3f pos_right;

    mpCollisionGetFloorEdgeL(line_id, &pos_left);
    mpCollisionGetFloorEdgeR(line_id, &pos_right);

    if ((pos_right.x - pos_left.x) <= 750.0F)
    {
        return 0;
    }
    else if ((pos_right.x - pos_left.x) <= 1050.0F)
    {
        return 1;
    }
    else return 2;
}

/* The port's own, and the whole of the Bonus2 divergence in one place:
 * hang pack `name` under `dobj` the way lbCommonSetupTreeDObjs hangs a
 * DObjDesc tree, then give it the pack's scripts.
 *
 * The decomp's four reads out of gGRCommonStruct.bonus2.file --
 * DObjDesc, MObjSub***, AObjEvent32** and AObjEvent32*** -- are the four
 * halves of one baked tree here, so this is one call plus the two walks
 * that are still the decomp's own. It follows src/dc/sc1pgameboss.c's
 * dcBossSetupBackgroundDObjs exactly, including why the transform set is
 * dc_model_add_dobjs' Tra/RotRpyR/Sca triple where the decomp passes
 * 0x44 (nGCMatrixKindTraRotRpyRSca): the two compose the same transform,
 * and appending the fused one would leave the triple with nothing to
 * drive.
 *
 * Returns the tree's root, the DObj the decomp calls `dobj->child`, or
 * NULL when the pack is not there. */
static DObj *dcBonusStageSetupPlatformDObjs(DObj *dobj, const char *name,
                                            sb32 with_mobjs)
{
    Stage *st = stage_bound();
    DObj *joints[STAGE_BOSS_JOINTS_MAX];
    Fighter *pack;
    int index;
    int n, j;

    if (st == NULL)
    {
        return NULL;
    }
    index = stage_platform_pack(st, name);

    if (index < 0)
    {
        syDebugPrintf("bonus2: no platform pack %s\n", name);
        return NULL;
    }
    pack = &st->platform_models[index];

    n = dc_model_add_dobjs(NULL, dobj, pack, joints);

    if ((n < 0) || (dobj->child == NULL))
    {
        return NULL;
    }
    /* Every platform of a size shares this one pack, and they all hang in
     * the course's own tree, so a walk that only recorded matrices would
     * leave the last platform's in it and draw that one alone. Each DObj
     * draws itself as the walk reaches it instead, as Yoshi's Island's
     * clouds do (stage_bind_map_cloud). */
    for (j = 0; j < n; j++)
    {
        if (joints[j] != NULL)
        {
            dc_model_graft_display(joints[j]);
        }
    }
    /* lbCommonAddMObjForTreeDObjs' half. Only the three unboarded
     * platforms have materials of their own (tools/export/ssb_stageexport.py
     * gives a "Boarded" tree none), and a pack with none returns 0 here
     * the way the decomp's NULL MObjSub*** walks nothing. */
    if (with_mobjs && (pack->mobjs != NULL))
    {
        for (j = 0; j < n; j++)
        {
            if (joints[j] != NULL)
            {
                dc_model_add_mobjs_dobj(joints[j], pack, 0.0F, 0, j);
            }
        }
    }
    /* and lbCommonAddTreeDObjsAnimAll's, the AnimJoint half alone: the
     * MatAnimJoint half rode in with the tree above (its alt), the same
     * split sc1pgameboss.c and efground.c make. */
    lbCommonAddTreeDObjsAnimAll(dobj->child, st->platform_anim_tables[index],
                                NULL, 0.0F);
    return dobj->child;
}

/* sc1pbonusstage.c:539-570 sc1PBonusStageInitPlatforms 0x8018D6A8. One
 * floor line becomes one platform: the size decides the model, the model
 * goes under the line's yakumono DObj, and the size is remembered in the
 * new root's user_data so the boarding swap can find its twin.
 *
 * `id | 0x8000` is not a flag the port invented: nMPYakumonoStatusNone is
 * 0, and sc1PBonusStageBonus2ProcUpdate below tests `!= 0` to tell a
 * platform that is still standing from one already boarded -- so the
 * high bit is what makes size 0 (Small) a non-zero status.
 *
 * DIVERGES: the tree comes from the pack (dcBonusStageSetupPlatformDObjs
 * above). Everything else, including the two walks, is the decomp's. */
void sc1PBonusStageInitPlatforms(s32 line_id)
{
    DObj *dobj;
    DObj *root;
    s32 id;

    id = mpCollisionSetDObjNoID(line_id);
    dobj = gMPCollisionYakumonoDObjs->dobjs[id];
    id = sc1PBonusStageGetPlatformKind(line_id);

    root = dcBonusStageSetupPlatformDObjs(dobj, dSC1PBonusStagePlatformNames[id],
                                          TRUE);
    if (root == NULL)
    {
        return;
    }
#ifndef FT_HOSTTEST
    lbCommonPlayTreeDObjsAnim(root);
#endif
    root->user_data.s = id | 0x8000;
}

/* sc1pbonusstage.c:572-599 sc1PBonusStageMakePlatforms 0x8018D794,
 * verbatim: every floor line whose material is nMPMaterialDetect is a
 * platform, and the count of them IS the task count.
 *
 * `mpCollisionSetYakumonoOnID` on a DObj with no AnimJoint is the
 * decomp's own: a platform that carries no script of its own has to be
 * switched on by hand, because what switches the scripted ones on is
 * their script.
 *
 * The port's own `bonus2:` line at the tail is grbonus3.c's and
 * sc1PBonusStageMakeTargets' twin. Unlike the target course this does
 * NOT hang the console on a wrong count -- the decomp does not either,
 * because a course's platforms are its collision and there is no table
 * to disagree with. */
void sc1PBonusStageMakePlatforms(void)
{
    s32 line_count;
    s32 line_ids[100];
    s32 yakumono_id;
    s32 i;

    line_count = mpCollisionGetLineCountType(nMPLineKindFloor);

    gGRCommonStruct.bonus2.platform_count = 0;

    mpCollisionGetLineIDsTypeCount(nMPLineKindFloor, line_count, line_ids);

    for (i = 0; i < line_count; i++)
    {
        if ((mpCollisionGetVertexFlagsLineID(line_ids[i]) & MAP_VERTEX_MAT_MASK) == nMPMaterialDetect)
        {
            yakumono_id = mpCollisionSetDObjNoID(line_ids[i]);

            if (gMPCollisionYakumonoDObjs->dobjs[yakumono_id]->anim_joint.event32 == NULL)
            {
                mpCollisionSetYakumonoOnID(yakumono_id);
            }
            sc1PBonusStageInitPlatforms(line_ids[i]);

            gGRCommonStruct.bonus2.platform_count++;
        }
    }
#ifndef FT_HOSTTEST
    syDebugPrintf("bonus2: %d platform(s) of %d floor line(s)\n",
                  (int)gGRCommonStruct.bonus2.platform_count,
                  (int)line_count);
#endif
}

/* sc1pbonusstage.c:601-612 sc1PBonusStageUpdatePlatformInterface
 * 0x8018D890, verbatim: the target row's twin, one sprite off the end. */
void sc1PBonusStageUpdatePlatformInterface(void)
{
    SObj *sobj = SObjGetStruct(gGRCommonStruct.bonus2.interface_gobj);
    s32 i;

    for (i = 0; i < gGRCommonStruct.bonus2.platform_count; i++)
    {
        sobj = sobj->next;
    }
    gcEjectSObj(sobj);
}

/* sc1pbonusstage.c:614-657 sc1PBonusStageUpdatePlatformCount 0x8018D8DC.
 * A platform is boarded: throw its tree away and build the boarded twin
 * of the SAME size in its place, which is what `user_data.s & ~0x8000`
 * is kept for. Then the row, the landing cue, and COMPLETE! on the last
 * one -- the same three the target course does, with the same NEW RECORD
 * test one field over (bonus2_task_count, bonus2_time).
 *
 * DIVERGES: the tree, as above. Note what does NOT come back: the
 * boarded twin gets lbCommonAddDObjAnimJointAll where the standing one
 * got lbCommonAddTreeDObjsAnimAll and no MObjs at all -- the decomp's
 * own asymmetry, and the reason the exporter gives a "Boarded" tree no
 * materials. The port keeps it by asking for no MObjs here. */
void sc1PBonusStageUpdatePlatformCount(DObj *dobj)
{
    s32 id = dobj->child->user_data.s & ~0x8000;
    DObj *root;

    gcEjectDObj(dobj->child);

    root = dcBonusStageSetupPlatformDObjs(dobj,
                                          dSC1PBonusStageBoardedPlatformNames[id],
                                          FALSE);
#ifndef FT_HOSTTEST
    if (root != NULL)
    {
        lbCommonPlayTreeDObjsAnim(root);
    }
#endif
    gGRCommonStruct.bonus2.platform_count--;

    sc1PBonusStageUpdatePlatformInterface();
    func_800269C0_275C0(nSYAudioFGMBonus2PlatformLanding);

    if (gGRCommonStruct.bonus2.platform_count == 0)
    {
        if
        (
            (gSCManagerSceneData.scene_prev != nSCKind1PGame) &&
            (gSCManagerBackupData.spgame_records[gSCManagerSceneData.bonus_fkind].bonus2_task_count == SCBATTLE_BONUSGAME_TASK_MAX) &&
            (gSCManagerBattleState->time_passed < gSCManagerBackupData.spgame_records[gSCManagerSceneData.bonus_fkind].bonus2_time)
        )
        {
            ifCommonAnnounceCompleteInitInterface(nSYAudioVoiceAnnounceNewRecord);
        }
        else ifCommonAnnounceCompleteInitInterface(nSYAudioVoiceAnnounceComplete);

        ifCommonBattleEndAddSoundQueueID(nSYAudioFGMBonus2PlatformLanding);
    }
}

/* sc1pbonusstage.c:660-678 sc1PBonusStageBonus2ProcUpdate 0x8018DA2C,
 * verbatim: the whole game, once a frame. A fighter standing on a
 * nMPMaterialDetect floor has boarded whatever platform that line is --
 * unless the line's tree says it is already boarded, which is the
 * user_data test the 0x8000 above exists for. */
void sc1PBonusStageBonus2ProcUpdate(GObj *ground_gobj)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    while (fighter_gobj != NULL)
    {
        FTStruct *fp = ftGetStruct(fighter_gobj);

        if ((fp->ga == nMPKineticsGround) && ((fp->coll_data.floor_flags & MAP_VERTEX_MAT_MASK) == nMPMaterialDetect))
        {
            DObj *dobj = gMPCollisionYakumonoDObjs->dobjs[mpCollisionSetDObjNoID(fp->coll_data.floor_line_id)];

            if (dobj->child->user_data.s != nMPYakumonoStatusNone)
            {
                sc1PBonusStageUpdatePlatformCount(dobj);
            }
        }
        fighter_gobj = fighter_gobj->link_next;
    }
}

/* sc1pbonusstage.c:682-698 sc1PBonusStageMakeBonus2Ground 0x8018DAE0,
 * verbatim: the ground GObj a platform course does have, and it is the
 * proc above and nothing else. Priority 4 puts it after the fighters,
 * which is what makes "the fighter landed this frame" readable here. */
void sc1PBonusStageMakeBonus2Ground(void)
{
    gcAddGObjProcess
    (
        gcMakeGObjSPAfter
        (
            nGCCommonKindGround,
            NULL,
            nGCCommonLinkIDGround,
            GOBJ_PRIORITY_DEFAULT
        ),
        sc1PBonusStageBonus2ProcUpdate,
        nGCProcessKindFunc,
        4
    );
}

/* sc1pbonusstage.c:700-731 sc1PBonusStageMakeBumpers 0x8018DB24, the
 * pack's positions in place of the map file's DObjDesc walk.
 *
 * DIVERGES exactly the way src/dc/grbonus3.c's grBonus3MakeBumpers does,
 * for the same reason and off the same block: the game reaches the
 * bumpers at `map_head + dSC1PBonusStageBumperDescs[course][0]` and walks
 * a DObjDesc array and a parallel AnimJoint array to the
 * `DOBJ_ARRAY_MAX` terminator; the port reads the positions and scripts
 * the BMP1 block carries. The exporter skips the array's
 * first entry exactly where this function's `dobjdesc++, anim_joints++`
 * does, so `bumper_count` IS the game's own iteration count.
 *
 * Seven of the twelve courses have no bumpers at all -- their row in the
 * decomp's table is `{ 0x0, 0x0 }` and the `map_nodes != NULL` guard is
 * what keeps them out. The port's equivalent is `bumper_count == 0`,
 * which the exporter writes for those seven, so the loop simply does not
 * run and no guard is needed. */
void sc1PBonusStageMakeBumpers(void)
{
    Stage *st = stage_bound();
    Vec3f vel;
    s32 i;

    vel.x = vel.y = vel.z = 0.0F;

    if (st == NULL)
    {
        return;
    }
    for (i = 0; i < (s32)st->bumper_count; i++)
    {
        GObj *item_gobj;
        AObjEvent32 *anim_joint = NULL;

        /* the decomp's `*anim_joints != NULL` test one indirection
         * earlier, sc1PBonusStageMakeTargets' note again */
        if ((st->bumper_anim[i] >= 0) &&
            (st->bumper_anim[i] < (int8_t)st->bumper_anim_count))
        {
            anim_joint = st->bumper_anims[st->bumper_anim[i]];
        }
        item_gobj = itManagerMakeItemSetupCommon(NULL, nITKindGBumper, &st->bumpers[i], &vel, ITEM_FLAG_PARENT_GROUND);

        if ((anim_joint != NULL) && (item_gobj != NULL))
        {
#ifdef FT_HOSTTEST
            (void)anim_joint;
#else
            gcAddDObjAnimJoint(DObjGetStruct(item_gobj), anim_joint, 0.0F);
            gcPlayAnimAll(item_gobj);
#endif
        }
    }
#ifndef FT_HOSTTEST
    syDebugPrintf("bonus2: %d bumper(s), %d script(s)\n",
                  (int)st->bumper_count, (int)st->bumper_anim_count);
#endif
}

/* sc1pbonusstage.c:733-739 sc1PBonusStageInitBonus2 0x8018DC38, verbatim:
 * a platform course's whole ground setup, in the game's own order.
 * Reached from src/dc/stage.c's ground chain where the game reaches it
 * through gr/grmainsetup.c's own Bonus2 arm. */
void sc1PBonusStageInitBonus2(void)
{
    sc1PBonusStageMakeBumpers();
    sc1PBonusStageBonus2LoadFile();
    sc1PBonusStageMakePlatforms();
    sc1PBonusStageMakeBonus2Ground();
}

/* sc1pbonusstage.c:742-751 sc1PBonusStageInterfaceThreadUpdate
 * 0x8018DC70, verbatim: sleep a second, say GO, show the damage
 * readout, and eject.
 *
 * `gcEjectGObj(NULL)` ejects the RUNNING GObj, which here is this
 * thread's own -- deliberate and not a NULL bug
 * ([[geject-null-self-eject]] is the general case). The sleep after it
 * is what lets the eject take. */
void sc1PBonusStageInterfaceThreadUpdate(GObj *interface_gobj)
{
    gcSleepCurrentGObjThread(60);
    ifCommonAnnounceGoMakeInterface();
    ifCommonPlayerDamageSetShowInterface();
    func_800269C0_275C0(nSYAudioVoiceAnnounceGo);
    ifCommonAnnounceGoSetStatus();
    gcEjectGObj(NULL);
    gcSleepCurrentGObjThread(1);
}

// 0x8018DCC4
void sc1PBonusStageMakeInterface(void)
{
    gcAddGObjProcess
    (
        gcMakeGObjSPAfter
        (
            nGCCommonKindInterface,
            NULL,
            nGCCommonLinkIDInterfaceActor,
            GOBJ_PRIORITY_DEFAULT
        ),
        sc1PBonusStageInterfaceThreadUpdate,
        nGCProcessKindThread,
        5
    );
    gSCManagerBattleState->game_status = nSCBattleGameStatusWait;
}

/* sc1pbonusstage.c:773-791 sc1PBonusStageInitCamera 0x8018DD14,
 * verbatim. The camera follows the one human and nothing else, and the
 * platform courses get six more degrees of downward pitch than the
 * target courses -- they are tall and the targets are wide. */
void sc1PBonusStageInitCamera(void)
{
    s32 player;

    for (player = 0; player < ARRAY_COUNT(gSCManagerBattleState->players); player++)
    {
        if (gSCManagerBattleState->players[player].pkind == nFTPlayerKindNot)
        {
            continue;
        }
        if (gSCManagerBattleState->gkind >= nGRKindBonus2Start)
        {
            gmCameraSetStatusPlayerFollow(gSCManagerBattleState->players[player].fighter_gobj, 0.0F, F_CLC_DTOR32(-15.0F), 9000.0F, 0.3F, 31.5F);
        }
        else gmCameraSetStatusPlayerFollow(gSCManagerBattleState->players[player].fighter_gobj, 0.0F, F_CLC_DTOR32(-9.0F), 9000.0F, 0.3F, 31.5F);

        break;
    }
}

/* sc1pbonusstage.c:794-814 sc1PBonusStageMakeTargetSprites 0x8018DDE0,
 * verbatim but for the file.
 *
 * DIVERGES: the decomp opens relocData 151 onto the scene heap here,
 * once per scene; the port loaded the bank in sc1PBonusStageSetupFiles
 * and this reads it. One SObj per target still standing, laid left to
 * right from x=30 with three pixels between them.
 *
 * A NULL bank is survivable and left so: lbCommonMakeSObjForGObj on a
 * NULL sprite makes nothing, the row is empty, and the count itself
 * lives in gGRCommonStruct.bonus1 rather than in these sprites. */
void sc1PBonusStageMakeTargetSprites(void)
{
    GObj *interface_gobj;
    SObj *sobj;
    void *file;
    s32 i;

    file = sSC1PBonusStageObjectFile;

    gGRCommonStruct.bonus1.interface_gobj = interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

    if (file == NULL)
    {
        return;
    }
    for (i = 0; i < gGRCommonStruct.bonus1.target_count; i++)
    {
        sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, file, llSC1PStageClear3TargetSprite));

        if (sobj == NULL)
        {
            continue;
        }
        sobj->sprite.attr = SP_TEXSHUF | SP_TRANSPARENT;

        sobj->pos.x = -(sobj->sprite.width / 2) + (((sobj->sprite.width + 3) * i) + 30);
        sobj->pos.y = 30 - (sobj->sprite.height / 2);
    }
}

/* sc1pbonusstage.c:817-838 sc1PBonusStageMakePlatformSprites 0x8018DF3C,
 * the target row's twin one sprite offset over: the same
 * bank, the same layout, the same survivable NULL. The count it draws is
 * the one sc1PBonusStageMakePlatforms found in the collision, which is
 * why this has to run after the ground -- and in
 * sc1PBonusStageFuncStart's order it does. */
void sc1PBonusStageMakePlatformSprites(void)
{
    GObj *interface_gobj;
    SObj *sobj;
    void *file;
    s32 i;

    file = sSC1PBonusStageObjectFile;

    gGRCommonStruct.bonus2.interface_gobj = interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

    if (file == NULL)
    {
        return;
    }
    for (i = 0; i < gGRCommonStruct.bonus2.platform_count; i++)
    {
        sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, file, llSC1PStageClear3PlatformSprite));

        if (sobj == NULL)
        {
            continue;
        }
        sobj->sprite.attr = SP_TEXSHUF | SP_TRANSPARENT;

        sobj->pos.x = -(sobj->sprite.width / 2) + (((sobj->sprite.width + 3) * i) + 30);
        sobj->pos.y = 30 - (sobj->sprite.height / 2);
    }
}

/* sc1pbonusstage.c:840-847 sc1PBonusStageMakeTaskSprites 0x8018E098,
 * verbatim (both arms as of  13m). */
void sc1PBonusStageMakeTaskSprites(void)
{
    if (gSCManagerBattleState->gkind >= nGRKindBonus2Start)
    {
        sc1PBonusStageMakePlatformSprites();
    }
    else sc1PBonusStageMakeTargetSprites();
}

/* sc1pbonusstage.c:850-856 sc1PBonusStageGetPlayerStartPosition
 * 0x8018E0E0, verbatim: the map's one nMPMapObjKind1PGamePlayer
 * object, which is where the fighter starts. */
void sc1PBonusStageGetPlayerStartPosition(Vec3f *pos)
{
    s32 mapobj;

    mpCollisionGetMapObjIDsKind(nMPMapObjKind1PGamePlayer, &mapobj);
    mpCollisionGetMapObjPositionID(mapobj, pos);
}

/* sc1pbonusstage.c:859-896 sc1PBonusStageTimerProcUpdate 0x8018E114,
 * verbatim but for the digit sprites' file.
 *
 * THE PRACTICE CLOCK, the one that counts UP and shows hundredths --
 * six digits, each a division by dSC1PBonusStageTimerUnitLengths and a
 * subtraction of what it took. A digit whose value did not change is
 * not touched, which is why the loop writes the sprite inside the test
 * and steps `sobj` outside it. The first digit un-hides itself the
 * moment it is anything but the zero it was made with, so a run under
 * ten minutes never shows a leading zero. */
void sc1PBonusStageTimerProcUpdate(GObj *interface_gobj)
{
    s32 digit;
    u32 itime;
    f32 ftime;
    SObj *sobj;
    s32 i;

    itime = gSCManagerBattleState->time_passed;
    sobj = SObjGetStruct(interface_gobj);

    if (itime > I_TIME_TO_TICS(0, 59, 59, 59))
    {
        itime = I_TIME_TO_TICS(0, 59, 59, 59);
    }
    ftime = itime;

    for (i = 0; i < ARRAY_COUNT(sSC1PBonusStageTimerDigits); i++)
    {
        digit = ftime / dSC1PBonusStageTimerUnitLengths[i];
        ftime -= digit * dSC1PBonusStageTimerUnitLengths[i];

        if (sobj == NULL)
        {
            break;
        }
        if (digit != sSC1PBonusStageTimerDigits[i])
        {
            sobj->sprite = *lbRelocGetFileData(Sprite*, gGMCommonFiles[3], dIFCommonTimerDigitSpriteOffsets[digit]);
            sobj->pos.x = dSC1PBonusStageTimerDigitPositions[i] - (sobj->sprite.width * 0.5F);
            sobj->pos.y = 30.0F - (sobj->sprite.height * 0.5F);

            sSC1PBonusStageTimerDigits[i] = digit;

            if (i == 0)
            {
                sobj->sprite.attr &= ~SP_HIDDEN;
            }
        }
        sobj = sobj->next;
    }
}

// 0x8018E298
void sc1PBonusStageSetTimeUp(void)
{
    sSC1PBonusStageIsTimeUp = TRUE;
}

/* sc1pbonusstage.c:906-914 sc1PBonusStageTimeUpProcUpdate 0x8018E2A8,
 * verbatim. The clock's callback cannot raise the FAILURE banner
 * itself -- it fires from inside ifCommonTimerFuncRun -- so it sets a
 * flag and this, a process of its own, spends it on the next tic and
 * ejects. */
void sc1PBonusStageTimeUpProcUpdate(GObj *interface_gobj)
{
    if (sSC1PBonusStageIsTimeUp != FALSE)
    {
        ifCommonAnnounceFailureInitInterface();
        sSC1PBonusStageIsTimeUp = FALSE;
        gcEjectGObj(interface_gobj);
    }
}

/* sc1pbonusstage.c:917-937 sc1PBonusStageMakeTimeUp 0x8018E2E8,
 * verbatim. Only a LADDER run has a clock that can run out, so only a
 * ladder run makes the process that watches for it. */
void sc1PBonusStageMakeTimeUp(void)
{
    sSC1PBonusStageIsTimeUp = FALSE;

    if (gSCManagerSceneData.scene_prev == nSCKind1PGame)
    {
        gcAddGObjProcess
        (
            gcMakeGObjSPAfter
            (
                nGCCommonKindInterface,
                NULL,
                nGCCommonLinkIDInterface,
                GOBJ_PRIORITY_DEFAULT
            ),
            sc1PBonusStageTimeUpProcUpdate,
            nGCProcessKindFunc,
            0
        );
    }
}

/* sc1pbonusstage.c:941-989 sc1PBonusStageMakeTimer 0x8018E344, verbatim
 * but for the sprite files.
 *
 * TWO CLOCKS, and which one is built is the scene's whole ladder-versus-
 * practice split. A practice run builds this file's own: six digits
 * laid at dSC1PBonusStageTimerDigitPositions, a `"` and a `'` beside
 * them, and sc1PBonusStageTimerProcUpdate driving them from
 * time_passed. A ladder run builds the BATTLE HUD's clock instead
 * (ifCommonTimerMakeDigits), counting the two minutes down, and hands
 * it sc1PBonusStageSetTimeUp as the callback for zero. */
void sc1PBonusStageMakeTimer(void)
{
    GObj *interface_gobj;
    SObj *sobj;
    s32 i;

    if (gSCManagerSceneData.scene_prev != nSCKind1PGame)
    {
        ifCommonTimerMakeInterface(NULL);
        ifCommonTimerSetAttr();

        interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, NULL, nGCCommonLinkIDInterface, GOBJ_PRIORITY_DEFAULT);

        gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

        for (i = 0; i < ARRAY_COUNT(sSC1PBonusStageTimerDigits); i++)
        {
            sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[3], dIFCommonTimerDigitSpriteOffsets[IFCOMMON_TIMER_DIGIT0]));

            if (sobj != NULL)
            {
                sobj->pos.x = dSC1PBonusStageTimerDigitPositions[i] - (sobj->sprite.width * 0.5F);
                sobj->pos.y = 30.0F - (sobj->sprite.height * 0.5F);
            }
            sSC1PBonusStageTimerDigits[i] = 0;
        }
        sobj = SObjGetStruct(interface_gobj);

        if (sobj != NULL)
        {
            sobj->sprite.attr |= SP_HIDDEN;
        }

        sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[3], dIFCommonTimerDigitSpriteOffsets[IFCOMMON_TIMER_SYMBOL_SEC]));

        if (sobj != NULL)
        {
            sobj->pos.x = (s32) (231.0F - (sobj->sprite.width * 0.5F));
            sobj->pos.y = (s32) (20.0F - (sobj->sprite.height * 0.5F));
        }

        sobj = lbCommonMakeSObjForGObj(interface_gobj, lbRelocGetFileData(Sprite*, gGMCommonFiles[3], dIFCommonTimerDigitSpriteOffsets[IFCOMMON_TIMER_SYMBOL_CSEC]));

        if (sobj != NULL)
        {
            sobj->pos.x = (s32) (264.0F - (sobj->sprite.width * 0.5F));
            sobj->pos.y = (s32) (20.0F - (sobj->sprite.height * 0.5F));
        }

        gcAddGObjProcess(interface_gobj, sc1PBonusStageTimerProcUpdate, nGCProcessKindFunc, 5);
        return;
    }
    else
    {
        ifCommonTimerMakeInterface(sc1PBonusStageSetTimeUp);
        ifCommonTimerMakeDigits();
    }
}

// 0x8018E5D8
void sc1PBonusStageSetPlayerInterfacePositions(void)
{
    gIFCommonPlayerInterface.player_pos_x = dSC1PBonusStagePlayerInterfacePositions;
    gIFCommonPlayerInterface.player_pos_y = 210;
}

/* sc1pbonusstage.c:999-1079 sc1PBonusStageFuncStart 0x8018E5F8. The
 * scene's whole build, in the decomp's order but for the camera pair,
 * and every cut here is src/dc/sc1pgame.c's cut for its reason. */
void sc1PBonusStageFuncStart(void)
{
    s32 player;
    GObj *fighter_gobj;
    FTDesc desc;
    SYColorRGBA color;

    sc1PBonusStageInitVars();
    sc1PBonusStageSetupFiles();
    /* CUT: sc1PBonusStageBonus1LoadFile -- see the block at its own
     * site above.
     *
     * CUT: gcMakeDefaultCameraGObj, the black clear camera. The PVR
     * clears its own framebuffer, and this camera's COBJ_FLAG_FILLCOLOR
     * would paint over every 3D list behind it
     * ([[fillcolor-camera-paints-over-3d]]). */
    efParticleInitAll();
    ftParamInitGame();

    /* the port's name for mpCollisionInitGroundData: the game's reads
     * the stage's ground file off the ROM and the port's tables are
     * already in RAM (src/dc/sc1pgame.c:2215, src/dc/stage.c:1533). */
    stage_bind_collision();

    gmCameraSetViewportDimensions(10, 10, 310, 230);
    /* CUT: gmCameraMakeWallpaperCamera -- src/dc/scvsbattle.c's header
     * block, and src/dc/stage.c's grWallpaperMakeDecideKind for where
     * the wallpaper goes instead.
     *
     * DIVERGES: grWallpaperMakeDecideKind and gmCameraMakeBattleCamera
     * are sc1pbonusstage.c:1016-1017, here, and the port makes them
     * after the fighter instead -- see the block at their new site. */
    itManagerInitItems();
    grCommonSetupInitAll();
    ftManagerAllocFighter(FTDATA_FLAG_MAINMOTION, 4);
    wpManagerAllocWeapons();
    /* DIVERGES: efManagerInitEffects is the port's
     * efManagerLoadEffectBank, and the three preloads under it are the
     * port's own -- src/dc/scvsbattle.c carries the reasoning for all
     * four. The target's own death makes two of those effects
     * (src/dc/ittarget.c), so this is not optional here. */
    efManagerLoadEffectBank();
    efManagerPreloadModels();
    wpManagerPreloadModels();
    itemModelPreloadAll();
    ifScreenFlashMakeInterface(0xFF);
    gmRumbleMakeActor();
    ftPublicMakeActor();

    for (player = 0, desc = dFTManagerDefaultFighterDesc; player < ARRAY_COUNT(gSCManagerBattleState->players); player++)
    {
        if (gSCManagerBattleState->players[player].pkind == nFTPlayerKindNot)
        {
            continue;
        }
        ftManagerSetupFilesAllKind(gSCManagerBattleState->players[player].fkind);

        desc.fkind = gSCManagerBattleState->players[player].fkind;

        sc1PBonusStageGetPlayerStartPosition(&desc.pos);

        desc.lr = (desc.pos.x >= 0.0F) ? -1 : +1;

        desc.team = 0;
        desc.player = player;
        desc.detail = nFTPartsDetailHigh;
        desc.costume = gSCManagerBattleState->players[player].costume;
        desc.pkind = gSCManagerBattleState->players[player].pkind;
        desc.controller = &gSYControllerDevices[player];
        desc.figatree_heap = ftManagerAllocFigatreeHeapKind(gSCManagerBattleState->players[player].fkind);
        desc.is_skip_entry = TRUE;

        fighter_gobj = ftManagerMakeFighter(&desc);

        ftParamInitPlayerBattleStats(player, fighter_gobj);

        break;
    }
    /* CUT: ftManagerSetupFilesPlayablesAll, the ROM loader for every
     * playable kind -- src/dc/scvsbattle.c's header block. */
    ifCommonBattleSetGameStatusWait();

    /* sc1pbonusstage.c:1016-1017, moved down from before the spawn
     * loop. src/dc/scvsbattle.c:399 gives the reason in full: the
     * port's battle camera follows every fighter that EXISTS when it is
     * made, where the game's finds them by link, so making it first
     * captures nothing. */
    gmCameraMakeBattleCamera();

    /* The port's own line, src/dc/sc1pgame.c:2351's twin and not
     * optional: gmCameraMakeBattleCamera leaves camera_mask empty, and
     * an empty mask means gcCaptureCameraGObj walks no DL link at all
     * -- a live scene that submits not one triangle a frame. */
    gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK;

    grWallpaperMakeDecideKind();

    gmCameraScreenFlashMakeCamera();
    gmCameraMakeInterfaceCamera();
    gmCameraMakeEffectCamera();
    ifCommonPlayerTagMakeInterface();
    sc1PBonusStageSetPlayerInterfacePositions();
    ifCommonPlayerDamageInitInterface();
    ifCommonPlayerStockInitInterface();
    sc1PBonusStageMakeTaskSprites();
    sc1PBonusStageMakeInterface();
    mpCollisionSetPlayBGM();
    func_800269C0_275C0(nSYAudioVoicePublicExcited);
    sc1PBonusStageMakeTimer();
    sc1PBonusStageInitCamera();

    color = dSC1PBonusStageFadeColor;

    lbFadeMakeActor(nGCCommonKindTransition, nGCCommonLinkIDTransition, 10, &color, 12, TRUE, NULL);
    sc1PBonusStageMakeTimeUp();
    /* From here to the scene's end nothing should be read: any file that
     * is gets a line naming it (src/dc/assetroot.h), the same guard
     * scVSBattleStartBattle ends on. */
    asset_io_expect_none("the bonus stage");
}

/* sc1pbonusstage.c:1082-1100 sc1PBonusStageSetBonusStats 0x8018E8D0,
 * verbatim: what a LADDER bonus rung contributes to the score screen.
 * Clearing all ten is worth the PERFECT bonus and the seconds left;
 * leaving any standing is worth neither. */
void sc1PBonusStageSetBonusStats(s32 tasks_remain)
{
    gSC1PManagerTotalDamage += gSCManagerBattleState->players[gSCManagerSceneData.player].total_damage_all;

    if (tasks_remain != 0)
    {
        gSCManagerSceneData.spgame_time_remain = 0;
        gSCManagerSceneData.bonus_get_mask[0] = 0;
        gSCManagerSceneData.bonus_get_mask[1] = 0;
        gSCManagerSceneData.bonus_get_mask[2] = 0;
    }
    else
    {
        gSCManagerSceneData.spgame_time_remain = I_TICS_TO_SEC(gSCManagerBattleState->time_remain + 59);
        gSCManagerSceneData.bonus_get_mask[0] = SC1PGAME_BONUS_MASK0_PERFECT;
        gSCManagerSceneData.bonus_get_mask[1] = 0;
        gSCManagerSceneData.bonus_get_mask[2] = 0;
    }
}

/* sc1pbonusstage.c:1103-1151 sc1PBonusStageWriteBackup 0x8018E95C,
 * verbatim. A failed run records how many tasks were cleared if that
 * beats the record; a cleared run records the ten and, if the time
 * beats the record, the time -- and writes the save on either. */
void sc1PBonusStageWriteBackup(sb32 is_tasks_fail, s32 fkind)
{
    if (gSCManagerSceneData.is_reset == FALSE)
    {
        if (gSCManagerBattleState->gkind <= nGRKindBonus1End)
        {
            if (is_tasks_fail != FALSE)
            {
                if (gSCManagerBackupData.spgame_records[fkind].bonus1_task_count < gSCManagerSceneData.bonus_tasks_complete)
                {
                    gSCManagerBackupData.spgame_records[fkind].bonus1_task_count = gSCManagerSceneData.bonus_tasks_complete;

                    lbBackupWrite();
                }
            }
            else
            {
                gSCManagerBackupData.spgame_records[fkind].bonus1_task_count = SCBATTLE_BONUSGAME_TASK_MAX;

                if (gSCManagerBattleState->time_passed < gSCManagerBackupData.spgame_records[fkind].bonus1_time)
                {
                    gSCManagerBackupData.spgame_records[fkind].bonus1_time = gSCManagerBattleState->time_passed;

                    lbBackupWrite();
                }
            }
        }
        else if (is_tasks_fail != FALSE)
        {
            if (gSCManagerBackupData.spgame_records[fkind].bonus2_task_count < gSCManagerSceneData.bonus_tasks_complete)
            {
                gSCManagerBackupData.spgame_records[fkind].bonus2_task_count = gSCManagerSceneData.bonus_tasks_complete;

                lbBackupWrite();
            }
        }
        else
        {
            gSCManagerBackupData.spgame_records[fkind].bonus2_task_count = SCBATTLE_BONUSGAME_TASK_MAX;

            if (gSCManagerBattleState->time_passed < gSCManagerBackupData.spgame_records[fkind].bonus2_time)
            {
                gSCManagerBackupData.spgame_records[fkind].bonus2_time = gSCManagerBattleState->time_passed;

                lbBackupWrite();
            }
        }
    }
}

/* CUT: sc1PBonusStageFuncLights, the pre-render function --
 * dSC1PBonusStageTaskmanSetup carries NULL where the decomp names it,
 * as src/dc/scvsbattle.c does for the identical scVSBattleFuncLights. */

/* sc1pbonusstage.c:1162-1257 sc1PBonusStageStartScene 0x8018EACC.
 *
 * The run, and then what the run was worth. The tail is the LUIGI
 * unlock: clearing all ten targets on a practice run, with every
 * starter fighter's target record already at ten, sends the player
 * straight into Luigi's challenger rung instead of back to the select.
 * That branch is why this function reads the backup records itself. */
void sc1PBonusStageStartScene(void)
{
    u16 bonus_complete_fkinds;
    s32 tasks_remain;
    u32 tasks_complete;
    s32 i;

    /* CUT: syVideoInit and the z-buffer it is handed, and the arena
     * line under it, which reads a link map the ELF does not have.
     * Both are src/dc/scvsbattle.c's cuts, for its reasons. */

    /* The course's ground, on the port's own terms and in the game's
     * place -- src/dc/sc1pgame.c:3096 says it at length and this is
     * the same problem: the pack has to be acquired BEFORE
     * scManagerFuncUpdate empties the heap, and the kind is not known
     * until sc1PBonusStageInitVars runs INSIDE the task. So the kind is
     * computed here the way InitVars computes it -- fighter plus the
     * range start, chosen by which scene sent us -- and InitVars writes
     * the same number into the battle state a moment later.
     *
     * DIVERGES on a missing pack, and unlike a VS battle this one does
     * NOT fall back to Hyrule: a Break the Targets course with someone
     * else's ground has no targets, and MakeTargets' own count check
     * would hang the console on the empty BTG1 block. Refusing the
     * scene is the port's arm; src/dc/scmanager.c's caller sends the
     * player back where they came from. */
    {
        s32 fkind = (gSCManagerSceneData.scene_prev == nSCKind1PGame)
                        ? gSCManagerSceneData.fkind
                        : gSCManagerSceneData.bonus_fkind;
        s32 is_bonus1 =
            (gSCManagerSceneData.scene_prev == nSCKind1PGame)
                ? (gSCManagerSceneData.spgame_stage == nSC1PGameStageBonus1)
                : (gSCManagerSceneData.scene_prev == nSCKind1PBonus1Players);
        s32 gkind = fkind + (is_bonus1 ? nGRKindBonus1Start : nGRKindBonus2Start);
        Stage *stage = grStageAcquire(gkind);

        if (stage == NULL)
        {
            syDebugPrintf("sc1PBonusStageStartScene: no pack for stage %d, "
                          "not playing\n", (int)gkind);
            return;
        }
        sSC1PBonusStageStageHeld = TRUE;
        sSC1PBonusStageStageKind = gkind;
        stage_bind(stage);
    }

    dSC1PBonusStageTaskmanSetup.func_start = sc1PBonusStageFuncStart;
    scManagerFuncUpdate(&dSC1PBonusStageTaskmanSetup);

    syAudioStopBGMAll();

    while (syAudioCheckBGMPlaying(0) != FALSE)
    {
        continue;
    }

    syAudioSetBGMVolume(0, 0x7800);
    func_800266A0_272A0();
    gmRumbleInitPlayers();

    if (gSCManagerBattleState->game_status != nSCBattleGameStatusPause)
    {
        tasks_remain = (gSCManagerBattleState->gkind <= nGRKindBonus1End) ?
        gGRCommonStruct.bonus1.target_count :
        gGRCommonStruct.bonus2.platform_count;

        tasks_complete = SCBATTLE_BONUSGAME_TASK_MAX - tasks_remain;

        if (tasks_remain > 0); // Bruh

        gSCManagerSceneData.bonus_tasks_complete = tasks_complete;

        switch (gSCManagerSceneData.scene_prev)
        {
        case nSCKind1PGame:
            sc1PBonusStageSetBonusStats(tasks_remain);
            sc1PBonusStageWriteBackup(tasks_remain, gSCManagerSceneData.fkind);
            break;

        default:
            sc1PBonusStageWriteBackup(tasks_remain, gSCManagerSceneData.bonus_fkind);

            if (gSCManagerSceneData.scene_prev == nSCKind1PBonus1Players)
            {
                gSCManagerSceneData.scene_curr = nSCKind1PBonus1Players;

                if (tasks_complete == SCBATTLE_BONUSGAME_TASK_MAX)
                {
                    if (!(gSCManagerBackupData.unlock_mask & LBBACKUP_UNLOCK_MASK_LUIGI))
                    {
                        for (bonus_complete_fkinds = i = 0; i < ARRAY_COUNT(gSCManagerBackupData.spgame_records); i++)
                        {
                            if (gSCManagerBackupData.spgame_records[i].bonus1_task_count == SCBATTLE_BONUSGAME_TASK_MAX)
                            {
                                bonus_complete_fkinds |= (1 << i);
                            }
                        }
                        if ((bonus_complete_fkinds & LBBACKUP_CHARACTER_MASK_STARTER) == LBBACKUP_CHARACTER_MASK_STARTER)
                        {
                            gSCManagerSceneData.fkind = gSCManagerSceneData.bonus_fkind;
                            gSCManagerSceneData.costume = gSCManagerSceneData.bonus_costume;

                            gSCManagerSceneData.spgame_stage = nSC1PGameStageLuigi;
                            gSCManagerSceneData.scene_curr = nSCKind1PGame;

                            break;
                        }
                    }
                    if (sc1PManagerCheckUnlockSoundTest() != FALSE)
                    {
                        gSCManagerSceneData.unlock_messages[0] = nLBBackupUnlockSoundTest;
                        gSCManagerSceneData.scene_curr = nSCKindMessage;
                    }
                    break;
                }
            }
            else
            {
                gSCManagerSceneData.scene_curr = nSCKind1PBonus2Players;

                if ((tasks_complete == SCBATTLE_BONUSGAME_TASK_MAX) && (sc1PManagerCheckUnlockSoundTest() != FALSE))
                {
                    gSCManagerSceneData.unlock_messages[0] = nLBBackupUnlockSoundTest;
                    gSCManagerSceneData.scene_curr = nSCKindMessage;
                }
            }
            break;
        }
        gSCManagerSceneData.scene_prev = nSCKind1PBonusStage;
    }

    /* The stage, given back with the scene, and the particle banks'
     * textures with it -- src/dc/sc1pgame.c:3140 for both. */
    if (sSC1PBonusStageStageHeld)
    {
        grStageRelease(sSC1PBonusStageStageKind);
        sSC1PBonusStageStageHeld = 0;
    }
    lbpTexFreeAll();
}

/* The port's own bzero arm for dSCManagerOverlays[6] (src/dc/overlay.c):
 * sc1pbonusstage.c's whole .bss, which on the N64 the DMA clears on
 * every entry to the scene. dSC1PBonusStageFadeColor is the one
 * d-prefixed table here and is cleared for src/dc/sc1pgame.c:3197's
 * reason -- its initialiser is four zeroes, nothing writes it, so the
 * compiler puts it in .bss and the restore and the bzero are the same
 * four bytes. */
void sc1PBonusStageOverlayLoad(void)
{
    OVERLAY_CLEAR(sSC1PBonusStageBattleState);
    OVERLAY_CLEAR(sSC1PBonusStageTimerDigits);
    OVERLAY_CLEAR(sSC1PBonusStageIsTimeUp);
    OVERLAY_CLEAR(sSC1PBonusStageObjectFile);
    OVERLAY_CLEAR(sSC1PBonusStageObjectBank);
    OVERLAY_CLEAR(sSC1PBonusStageStageHeld);
    OVERLAY_CLEAR(sSC1PBonusStageStageKind);
    OVERLAY_CLEAR(dSC1PBonusStageFadeColor);
}
