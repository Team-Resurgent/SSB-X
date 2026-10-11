/* scmanager.c -- see scmanager.h. */
#include <stdio.h>             /* snprintf */
#include <stdlib.h>
#include "objmodel.h"
#include "overlay.h"

#include "scmanager.h"
#include "taskman.h"           /* syTaskmanWantExitPhoto */
#include "scvsbattle.h"
#include "scvsresults.h"
#include "mntitle.h"
#include "mnnocontroller.h"
#include "dcmemcard.h"
#include "loadcensus.h"
#include "scport.h"
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
#include "sc1pmanager.h"
#include "mn1pcontinue.h"
#include "mnvsmode.h"
#include "mnvsoptions.h"
#include "mnplayersvs.h"
#include "mnmaps.h"
#include "mnmessage.h"
#include "mnvsitemswitch.h"
#include "mndata.h"
#include "mnvsrecord.h"
#include "mncharacters.h"
#include "mnsoundtest.h"
#include "mnoption.h"
#include "mnscreenadjust.h"
#include "mnbackupclear.h"
#include "mncongra.h"
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
#include "sprite.h"
#include "ftcommon.h"
#include "assetroot.h"
#include "stackguard.h"
#include "textguard.h"
#include "scprefetch.h"          /* dSCPrefetchOpening */
#include "efmanager.h"            /* efManagerPreloadModels */
#include "itemmodel.h"            /* itemModelPreloadAll, itemModelRelease */
#include "wpattrs.h"             /* wpAttrsLoad, the weapon models */
#include "sndres.h"
#include "vmusave.h"

#include <lb/lbbackup.h>

#include <sys/obj.h>
#include <sys/debug.h>
#include <sc/sc1pmode/sc1pmanager.h>   /* gSC1PManagerKirbyTeamModelPartID */
#include <mp/map.h>               /* the collision state the abort dumps */

/* sc/scmanager.c:27, 31, 36, 39, 42. The first four are the manager's
 * state; the fifth is the pointer game logic reads the current battle
 * through.
 *
 * They start from the game's own defaults -- the three tables
 * in src/dc/scmanagerdata.c -- which scManagerInitData installs the way
 * scManagerRunLoop does. */
LBBackupData gSCManagerBackupData;
SCCommonData gSCManagerSceneData;
SCBattleState gSCManagerTransferBattleState;
SCBattleState gSCManagerVSBattleState;
SCBattleState gSCManager1PGameBattleState;
SCBattleState *gSCManagerBattleState;

/* The 1P run's carried state -- gSC1PManagerTotalTimeTics and the five
 * beside it -- lives in src/dc/sc1pmanager.c, not here, because
 * src/dc/sc1pintro.c and src/dc/sc1pgame.c read it and neither may
 * define it: both are in an overlay a rung reloads, so their statics
 * are zero on every entry, and the whole point of this state is to
 * carry a decision from one scene into the next. It is in
 * src/dc/sc1pmanager.c now, where sc1pmanager.c:74-89 has it, and that
 * file's header says why overlay 2 is a safe home for it when overlays
 * 24 and 65 are not. */

/* sc/scmanager.c:816-853 scManagerRunLoop, the part of it that is the
 * game's boot: the four globals from the three tables above, then the
 * save data off the store and the options out of the save data.
 *
 * DIVERGES: the rest of scManagerRunLoop is N64. The three overlay DMAs
 * have no overlays to load (src/dc/overlay.h); syDebugStartRmonThread5Hang
 * is the rmon debug thread; ftManagerSetupFileSize measures the largest
 * fighter file so the N64 can size one buffer for the biggest DMA, and
 * the port loads whole packs (src/dc/ftmanager.c); the audio settings
 * handshake (dSYAudioPublicSettings, syAudioSetFXType and the two spin
 * waits) is the RSP audio task restarting, and the port configures the
 * AICA once at boot (src/dc/bgm.c); the framebuffer fill to black is
 * gSYFramebufferSets, which the PVR clears itself; and
 * gSYControllerConnectedNum == 0 is a check this function never makes --
 * nSCKindNoController exists (src/dc/mnnocontroller.c),
 * but only as ported content, not as a reachable boot path: the port
 * still assumes a pad is always connected, so the scene is reachable
 * only by a direct DB_BOOT_SCENE boot (mnnocontroller.h's own header
 * note says why re-adding the check is its own design question, not
 * this file's). What is left is the six lines that decide what the game
 * starts from.
 *
 * gSCManager1PGameBattleState is defined above, beside the other two, and
 * scManagerInitData gives it dSCManagerDefaultBattleState in the game's
 * order with them. Nothing WRITES the two fields the select reads
 * (is_team_battle and game_rules) until sc/sc1pmode/sc1pmanager.c is
 * ported -- that is the one function that sets them -- so until then
 * the select reads the defaults, which is what a first 1P game would
 * see anyway.
 *
 * Note what lbBackupIsSramValid does on a store with nothing in it: the
 * checksum of all-zero bytes is zero and matches the zero it reads back,
 * but the signature is not 666, so both copies are refused, the defaults
 * are installed, and lbBackupWrite puts them in the store with a
 * checksum that does hold. That is the path a fresh cartridge takes, and
 * it is also the path a machine with a blank memory
 * card takes -- the write it ends in is what puts the first file on the
 * card (src/dc/vmusave.c). */
void scManagerInitData(void)
{
    /* The port's own, and the first line here because the rest of this
     * function reads the store: it comes off a memory
     * card (src/dc/vmusave.c), and it has to be there before
     * lbBackupIsSramValid looks. On the N64 the SRAM is simply present
     * and there is nothing to do. */
    sy_sram_init();

    gSCManagerBackupData = dSCManagerDefaultBackupData;
    gSCManagerSceneData = dSCManagerDefaultSceneData;

    gSCManagerTransferBattleState =
    gSCManagerVSBattleState       =
    gSCManager1PGameBattleState   = dSCManagerDefaultBattleState;

    /* DIVERGES: a write the game makes while it starts up (defaults on a
     * blank or damaged card) reads to the player as the card being loaded */
    sy_sram_next_write_is_load();
    lbBackupIsSramValid();
    lbBackupApplyOptions();
}

/* sc/scmanager.c scManagerRunPrintGObjStatus: dumps every GObj and never
 * returns; the collision code calls it inside `while (TRUE)` after an
 * invalid line id ("mpGetUUCommon() id = -1" and friends), and so do
 * ftmanager.c and gmcamera.c when a pool runs dry. DIVERGES: a hung
 * console is no use, so the port aborts, which stops the host test with
 * a message and drops the target into KOS's abort handler.
 *
 * What it prints first is the COLLISION state rather than the GObj list,
 * because every one of those guards is a question about a line whose
 * yakumono has been turned off and the guard's own message does not say
 * which line, which DObj, or what its status was. That is the whole
 * content of a bug report that reads `mpGetNbVertex() no collision` and
 * nothing else. The dump is bounded: the first
 * 32 yakumono slots and the first 16 offending lines, so a wild index
 * cannot turn the diagnostic into a second crash. */
void scManagerRunPrintGObjStatus(void)
{
    s32 i;
    s32 offenders = 0;

    if (gMPCollisionGeometry != NULL && gMPCollisionYakumonoDObjs != NULL)
    {
        syDebugPrintf("gobjstatus: %d line(s), %d yakumono slot(s), "
                      "%d group(s)\n", (int)gMPCollisionLinesNum,
                      (int)gMPCollisionYakumonosNum,
                      (int)gMPCollisionGeometry->yakumono_count);

        for (i = 0; i < gMPCollisionYakumonosNum && i < 32; i++)
        {
            syDebugPrintf("gobjstatus: yakumono %d status %d at (%.1f, "
                          "%.1f)\n", (int)i,
                          (int)gMPCollisionYakumonoDObjs->dobjs[i]->user_data.s,
                          gMPCollisionYakumonoDObjs->dobjs[i]->translate.vec.f.x,
                          gMPCollisionYakumonoDObjs->dobjs[i]->translate.vec.f.y);
        }
        if (gMPCollisionVertexInfo != NULL)
        {
            for (i = 0; i < gMPCollisionLinesNum; i++)
            {
                s32 yak = gMPCollisionVertexInfo->vertex_info[i].yakumono_id;

                if (yak < 0 || yak >= gMPCollisionYakumonosNum)
                {
                    syDebugPrintf("gobjstatus: line %d NAMES OUT-OF-RANGE "
                                  "yakumono %d\n", (int)i, (int)yak);
                }
                else if (gMPCollisionYakumonoDObjs->dobjs[yak]->user_data.s
                         >= nMPYakumonoStatusOff)
                {
                    if (offenders < 16)
                    {
                        syDebugPrintf("gobjstatus: line %d is on OFF yakumono "
                                      "%d (status %d)\n", (int)i, (int)yak,
                                      (int)gMPCollisionYakumonoDObjs
                                          ->dobjs[yak]->user_data.s);
                    }
                    offenders++;
                }
            }
            syDebugPrintf("gobjstatus: %d line(s) on an off yakumono\n",
                          (int)offenders);
        }
    }
    abort();
}

/* sc/scmanager.c:1286-1290, verbatim. On the N64 this is where a scene
 * blocks: syTaskmanStartTask ends in syTaskmanRunTask's while (TRUE) and
 * comes back only when the scene has called syTaskmanSetLoadScene. The
 * port's does the same (src/dc/taskman.c). */
void scManagerFuncUpdate(SYTaskmanSetup *arg)
{
    syTaskmanStartTask(arg);
}

/* sc/scmanager.c:1292-1295, verbatim. */
void scManagerFuncDraw(void)
{
    gcDrawAll();
}

/* The port's, for the load instrument's log line (src/dc/assetroot.h):
 * the number the game keeps is no use to a reader of a serial log, and
 * the decomp has no name table for these -- the arms of
 * scmanager.c:867's switch are the only place the kinds are spelled. */
static const char *sc_scene_name(s32 kind);

const char *scManagerSceneName(s32 kind)
{
    return sc_scene_name(kind);
}

static const char *sc_scene_name(s32 kind)
{
    switch (kind)
    {
    case nSCKindNoController: return "nocontroller";
    case nSCKindDCMemCard:    return "dcmemcard";
    case nSCKindTitle:        return "title";
    case nSCKindModeSelect:   return "modeselect";
    case nSCKind1PMode:       return "1pmode";
    case nSCKind1PGamePlayers: return "1pgameselect";
    case nSCKindPlayers1PTraining: return "1ptraining";
    case nSCKind1PBonus1Players: return "1pbonus1select";
    case nSCKind1PBonus2Players: return "1pbonus2select";
    case nSCKind1PTrainingMode: return "trainingmode";
    case nSCKind1PBonusStage: return "1pbonus";
    case nSCKind1PChallenger: return "1pchallenger";
    case nSCKind1PContinue:   return "1pcontinue";
    case nSCKind1PIntro:      return "1pintro";
    case nSCKind1PGame:       return "1pgame";
    case nSCKind1PScoreUnk:
    case nSCKind1PStageClear: return "1pstageclear";
    case nSCKindVSMode:       return "vsmode";
    case nSCKindVSOptions:    return "vsoptions";
    case nSCKindPlayersVS:    return "playersvs";
    case nSCKindMaps:         return "maps";
    case nSCKindVSBattle:     return "vsbattle";
    case nSCKindVSItemSwitch: return "itemswitch";
    case nSCKindMessage:      return "message";
    case nSCKindVSResults:    return "vsresults";
    case nSCKindData:         return "data";
    case nSCKindVSRecord:     return "vsrecord";
    case nSCKindCharacters:   return "characters";
    case nSCKindSoundTest:    return "soundtest";
    case nSCKindOption:       return "option";
    case nSCKindScreenAdjust: return "screenadjust";
    case nSCKindBackupClear:  return "backupclear";
    case nSCKindCongra:       return "congra";
    case nSCKindExplain:      return "explain";
    case nSCKindAutoDemo:     return "autodemo";
    case nSCKindEnding:       return "ending";
    case nSCKindStaffroll:    return "staffroll";
    case nSCKindStartup:      return "startup";
    case nSCKindOpeningRoom:  return "openingroom";
    case nSCKindOpeningPortraits: return "openingportraits";
    case nSCKindOpeningMario: return "openingmario";
    case nSCKindOpeningDonkey: return "openingdonkey";
    case nSCKindOpeningSamus: return "openingsamus";
    case nSCKindOpeningLink: return "openinglink";
    case nSCKindOpeningYoshi: return "openingyoshi";
    case nSCKindOpeningKirby: return "openingkirby";
    case nSCKindOpeningFox: return "openingfox";
    case nSCKindOpeningPikachu: return "openingpikachu";
    case nSCKindOpeningRun: return "openingrun";
    case nSCKindOpeningCliff: return "openingcliff";
    case nSCKindOpeningYamabuki: return "openingyamabuki";
    case nSCKindOpeningJungle: return "openingjungle";
    case nSCKindOpeningYoster: return "openingyoster";
    case nSCKindOpeningSector: return "openingsector";
    case nSCKindOpeningStandoff: return "openingstandoff";
    case nSCKindOpeningClash: return "openingclash";
    case nSCKindOpeningNewcomers: return "openingnewcomers";
    default:                  return "unported";
    }
}

/* The port's own. The files the opening movie opens again in nearly
 * every scene -- the effect bank, the item bank, the announcer's and the
 * eight fighters' sprites, the shadow, the two particle banks the
 * fighters bring and the shared camera script -- measured by a
 * -DDB_IO_TRACE boot to the title: about 8 MB of disc reads for 1.1 MB of RAM. Held
 * from the Room, which reads most of them first, through Clash; every
 * other scene gives them back. The fighter packs are
 * ftManagerKeepFilesForScene's, not this list's. */
static const char *const sSCManagerOpeningHeld[] = {
    "efcommon.scb", "efcommon.txb", "efcommon.txp",
    "itcommon.scb", "itcommon.txb", "itcommon.txp",
    "ifannounce.spr", "ifcommonitem.spr", "ftshadow.mdl",
    "mvopeningcommon.cam",
    "particles_unk0.scb", "particles_unk0.txb", "particles_unk0.txp",
    "particles_unk2.scb", "particles_unk2.txb", "particles_unk2.txp",
    "ftmario.spr", "ftdonkey.spr", "ftsamus.spr", "ftfox.spr",
    "ftlink.spr", "ftyoshi.spr", "ftpikachu.spr", "ftkirby.spr",
};

/* The VS loop's: the battle and the
 * results screen both read the effect bank and the announcer's sprites,
 * and a round of VS is battle, results, character select, stage select,
 * battle. The census measured the pair read again at every battle and
 * every results screen: 0.9 MB of disc for 0.9 MB of RAM, on a loop that
 * peaks at 7.2 MB of malloc in use. */
static const char *const sSCManagerVSHeld[] = {
    "efcommon.scb", "efcommon.txb", "efcommon.txp", "ifannounce.spr",
};

static void scManagerHoldFilesForScene(s32 scene)
{
    if ((scene == nSCKindOpeningRoom) ||
        (ftManagerSceneIsOpeningChain(scene) != FALSE))
    {
        asset_hold_set(sSCManagerOpeningHeld,
                       (int)ARRAY_COUNT(sSCManagerOpeningHeld));
    }
    else if ((scene == nSCKindVSBattle) || (scene == nSCKindVSResults) ||
             (scene == nSCKindPlayersVS) || (scene == nSCKindMaps))
    {
        asset_hold_set(sSCManagerVSHeld, (int)ARRAY_COUNT(sSCManagerVSHeld));
    }
    else
    {
        asset_hold_set(NULL, 0);
    }
}

/* The port's own read-ahead. The opening
 * movie is one fixed run of scenes, N64 logo to title, and what each
 * opens is known (src/dc/scprefetch.h, generated from a -DDB_IO_TRACE
 * boot by tools/export/prefetch_lists.py). Entering any scene of that
 * run queues every scene after it, and the loader reads them into RAM
 * while the scenes play, as far ahead as RAM allows: the Room's twenty
 * seconds reach most of the fighters' scenes. Each scene then opens its
 * files from RAM and frees them, and the loader reads on into the room
 * that leaves. Any other scene -- START pressed, or the menus -- gives
 * all of it back. */
static void scManagerPrefetchForScene(s32 scene)
{
    s32 row, i;
    char pack[32];

    for (row = 0; row < (s32)ARRAY_COUNT(dSCPrefetchOpening); row++)
    {
        if (dSCPrefetchOpening[row].scene == scene)
            break;
    }
    if (row == (s32)ARRAY_COUNT(dSCPrefetchOpening))
    {
        asset_prefetch_clear();
        return;
    }
    if (!asset_prefetch_enter(scene))
    {
        asset_prefetch_clear();
        for (i = row; i < (s32)ARRAY_COUNT(dSCPrefetchOpening); i++)
        {
            asset_prefetch_add(dSCPrefetchOpening[i].scene,
                               dSCPrefetchOpening[i].files,
                               dSCPrefetchOpening[i].count);
        }
        asset_prefetch_enter(scene);
    }
    /* The Room puts two fighters on the desk at random (mvopeningroom.c,
     * syUtilsRandTimeUCharRange), and the movie keeps their packs
     * (ftManagerKeepFilesForScene), so their own scenes will not open
     * them: the lists name every fighter scene's pack, and this takes
     * back the ones already resident. */
    for (i = nFTKindPlayableStart; i <= nFTKindPlayableEnd; i++)
    {
        if ((gFTManagerModels[i] != NULL) && (ftManagerKindName(i) != NULL))
        {
            snprintf(pack, sizeof(pack), "%s.pack", ftManagerKindName(i));
            asset_prefetch_drop(pack);
            snprintf(pack, sizeof(pack), "%s.anm", ftManagerKindName(i));
            asset_prefetch_drop(pack);
        }
    }
}

/* DIVERGES: the models every battle scene loads and keeps for the run
 * (efmanager.c efManagerPreloadModels on why), loaded here, at a scene
 * change, while the heap holds nothing of the scene just ended -- so
 * they sit low in one run and not between that scene's buffers. Loaded
 * by the scenes themselves, they landed wherever the scene left room:
 * How to Play put the item and weapon models above its demo fighters'
 * packs, and once those were freed the Characters scene's twelve packs
 * (11.4 MB) no longer fit the free space around them; with the loader
 * reading ahead the effect models ended up anywhere up to the top of
 * the heap (-DDB_HEAP_MAP, 2026-09-22). The scenes' own preload calls
 * then find everything loaded.
 *
 * The effect models (and the weapon tables) load at boot, before any
 * scene, and stay. The weapon and item models do not go with the
 * opening movie: it uses four of them, loaded as it meets them, and all
 * of them with their Fighter structs are 1.1 MB its Pikachu scene --
 * eight kept packs and a 614 KB stage, the movie's peak -- cannot spare.
 * So they load for any scene outside the movie and go back on the way
 * into it (the attract loop's, after How to Play). Given back at the
 * Room instead, they were still there while the N64 logo's read-ahead
 * laid out the movie's first buffers, and the third pass's Pikachu
 * scene found no piece of its 4.3 MB free that held its stage. */
static void scManagerModelsForScene(s32 scene)
{
    s32 row;

    for (row = 0; row < (s32)ARRAY_COUNT(dSCPrefetchOpening); row++)
    {
        if (dSCPrefetchOpening[row].scene == scene)
            break;
    }
    if (row < (s32)ARRAY_COUNT(dSCPrefetchOpening))
    {
        /* from the N64 logo on, before the loader reads a byte for the
         * movie, so each pass lays the heap out as the first did */
        itemModelRelease();
        wpManagerReleaseModels();
        return;
    }
    wpManagerPreloadModels();
    itemModelPreloadAll();
}

/* sc/scmanager.c:867-1283 scManagerRunScene's while (TRUE), which is the
 * game's whole navigation: read scene_curr, DMA that scene's overlays in,
 * call its StartScene -- which does not return until the scene is over --
 * and go round again on whatever scene_curr the scene left behind.
 *
 * DIVERGES, in two ways. Every scene the game can run has its arm
 * (nSCKind1PScoreUnk, which nothing enters, is the exception), and there
 * is one arm that is the port's own:
 *   - the default arm, any scene the port does not have, goes back to
 *     the scene that asked for it as if the player had backed out, and
 *     says which scene it was asked for. That is the game's own return
 *     route: a scene puts its cursor on the option it came back from
 *     (mnModeSelectInitVars and mnVSModeFuncStartVars read scene_prev
 *     for exactly this). Nothing reaches it today; it is a guard.
 * Every syDmaLoadOverlay is gone, because there are no overlays -- an overlay
 * is a chunk of the ROM the N64 pages into a fixed address, and the Dreamcast
 * build links every scene it has into the ELF; per-scene streaming goes here,
 * which is exactly where the game put it. What the port does release here,
 * after each scene, is the VRAM its sprite banks took (sprite.h): the scene's
 * files live in the heap the next scene re-inits, and their textures live
 * where the heap cannot reach.
 *
 * The loop never leaves: a scene the port does not have
 * would come back to the one that asked, and the serial log names it.
 *
 * The heap between scenes is handled where the game handles it, inside
 * syTaskmanStartTask: every scene's setup re-inits the general heap, so
 * a scene starts with the previous scene's collision tables, fighter
 * instances and object pools all gone at once (src/dc/taskman.c on the
 * one difference -- the port keeps its region and empties it, because
 * the game's arena_start is a link-map address).
 *
 * What is still missing before this is the game's loop rather than a
 * sketch of it: syVideoInit per scene, which has nothing to do while
 * there is one video mode. The fades are the scenes' own
 * (src/dc/lbfade.c): the battle fades in from black as the game's does. */
void scManagerRunScene(void)
{
    efManagerPreloadModels();
    (void)wpAttrsLoad();
    scManagerModelsForScene(gSCManagerSceneData.scene_curr);

    /* the first scene has no change before it to queue from */
    scManagerPrefetchForScene(gSCManagerSceneData.scene_curr);

    /* DIVERGES: the VMU's boot check, ahead of the first scene (the N64
     * logo): it may put the port's own scene in front of it, which hands
     * the manager back to it when the player has answered */
    dcMemCardBootBegin();

    while (TRUE)
    {
        /* what this scene reads off the medium, counted from
         * here to the line after it ends. The scene's own loads all
         * happen inside StartScene, which does not return until the
         * scene is over, so a scene's window is exactly its arm. */
        const char *scene = sc_scene_name(gSCManagerSceneData.scene_curr);

        /* -DDB_LOAD_CENSUS (loadcensus.h): a node per scene, begun
         * before anything of this scene's load, the sounds included */
        lc_node(gSCManagerSceneData.scene_curr, scene, 0);
        asset_io_reset();
        dc_model_set_demo_opaque(FALSE);
        dc_model_set_layered_rewalk(FALSE);

        /* The samples this scene can reach, into sound RAM if they are
         * not already there (src/dc/sndres.h's DIVERGES: the N64 keeps
         * both banks resident). Before the switch, because the battle
         * arm's pack substitution below is one sndres_compose makes for
         * itself; inside the scene's window, because it is this scene's
         * load. */
        sndres_enter_scene(gSCManagerSceneData.scene_curr);

        switch (gSCManagerSceneData.scene_curr)
        {
        case nSCKindNoController:
            /* scmanager.c:870-873, the decomp's own first arm -- and the
             * only one this function's own boot check ever sets
             * scene_curr to, a check the port's scManagerInitData does
             * not make (this file's own note above). Loads only overlay
             * 11, the decomp's own list. This scene has no exit of any
             * kind (src/dc/mnnocontroller.h): reaching it here blocks
             * mnNoControllerStartScene forever, exactly as the decomp's
             * NoController screen leaves the N64 until a pad is plugged
             * in and the console power-cycled. Reachable only by a
             * direct DB_BOOT_SCENE boot. */
            syDmaLoadOverlay(OVERLAY_NOCONTROLLER);
            mnNoControllerStartScene();
            break;

        case nSCKindTitle:
            /* scmanager.c:876-879 loads 2, 10, 8 and 9; 8 and 9 are the
             * movie player and the opening's models, which the port has
             * no file in. Every list below is the game's the same way. */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_TITLE);
            mnTitleStartScene();
            break;

        case nSCKindModeSelect:
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_MODESELECT);
            mnModeSelectStartScene();
            break;

        /* The 1P submenu. The four scenes it can choose are still
         * unported, so a select from here falls
         * through to the default arm below and the manager says so. */
        case nSCKind1PMode:
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_1PMODE);
            mn1PModeStartScene();
            break;

        /* The 1P game's character select. scmanager.c:884-888 loads
         * 1, 2, 3 and 27 -- the select stands a fighter on its gate, so
         * it takes the battle's two overlays like the other two. */
        case nSCKind1PGamePlayers:
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_1PGAME);
            mnPlayers1PGameStartScene();
            break;

        /* The card before every rung of the ladder.
         * scmanager.c:981-986 loads 2, 1 and 24; it makes fighters, so
         * OVERLAY_FIGHTING comes with them as it does above. The scene
         * that follows a rung's card, nSCKind1PGame, has its own case. */
        case nSCKind1PIntro:
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_1PINTRO);
            sc1PIntroStartScene();
            break;

        /* "CHALLENGER APPROACHING!". scmanager.c:974-979
         * loads 2, 1 and 23; it stands one fighter, so OVERLAY_FIGHTING
         * comes with them as it does for the card above. */
        case nSCKind1PChallenger:
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_1PCHALLENGER);
            sc1PChallengerStartScene();
            break;

        /* The 1P ladder. scmanager.c:1023-1025 starts no
         * scene for this kind and loads no overlay: it calls
         * sc1PManagerUpdateScene, the router, and that function runs a
         * whole run inline -- every rung's card, the rung, the score
         * screen, the challenger fight, the ending -- returning only
         * once the run is over, with scene_curr set to wherever it left
         * the player. src/dc/sc1pmanager.h has the rest of the argument,
         * including which arm of it is still CUT. */
        case nSCKind1PGame:
            sc1PManagerUpdateScene();
            break;

        /* "CONTINUE?" after a lost rung. scmanager.c:1213
         * -1218 loads 2, 1 and 55 -- the screen stands the human's
         * fighter, so it takes the battle's overlay as the two cards
         * do; it needs no OVERLAY_FIGHTING, because the fighter only
         * stands there. */
        case nSCKind1PContinue:
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_1PCONTINUE);
            mnPlayers1PGameContinueStartScene();
            break;

        /* The score screen after a rung. Two kinds, one
         * scene: scmanager.c:1220-1233 loads 2, 1 and 56 for both and
         * calls the same function, and what it shows is decided from
         * gSCManagerSceneData.spgame_stage rather than from the kind.
         * No OVERLAY_FIGHTING here -- unlike the two cards, this scene
         * stands no fighter. */
        case nSCKind1PScoreUnk:
        case nSCKind1PStageClear:
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_1PSTAGECLEAR);
            sc1PStageClearStartScene();
            break;

        /* Training mode's character select, and below it the mode
         * itself. */
        case nSCKindPlayers1PTraining:
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_1PTRAINING);
            mnPlayers1PTrainingStartScene();
            break;

        /* The bonus stages' practice select.
         * scmanager.c:1181-1198 loads 2, 1 and 29 and calls the one
         * function for both kinds -- the decomp writes the arm twice
         * only because DAIRANTOU_OPT0 splits the fallthrough; the two
         * bodies are identical, so one arm serves. Which of the two
         * bonus games the select runs it reads off scene_curr itself
         * (mnplayers1pbonus.c:2951). It stands a fighter on the gate,
         * so OVERLAY_FIGHTING comes with the battle's as it does for
         * the 1P game's select above. */
        case nSCKind1PBonus1Players:
        case nSCKind1PBonus2Players:
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_1PBONUS);
            mnPlayers1PBonusStartScene();
            break;

        case nSCKindVSMode:
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_VSMODE);
            mnVSModeStartScene();
            break;

        case nSCKindVSOptions:
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_VSOPTIONS);
            mnVSOptionsStartScene();
            break;

        case nSCKindPlayersVS:
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_PLAYERSVS);
            mnPlayersVSStartScene();
            break;

        case nSCKindMaps:
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_MAPS);
            mnMapsStartScene();
            break;

        case nSCKindVSBattle:
            /* DIVERGES, defensively: a pick whose pack is not resident
             * plays as Mario, said on the log, rather than stopping the
             * loop in the battle's spawn. All 27 kinds have a pack, so
             * this only catches one that would not load; it asks the
             * manager rather than its table,
             * which the battle fills for itself. The stage's own
             * substitution is the same answer, made where the stage is
             * bound (src/dc/scvsbattle.c). */
            {
                s32 i;

                for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
                {
                    SCPlayerData *pd = &gSCManagerTransferBattleState.players[i];

                    if (pd->pkind != nFTPlayerKindNot && pd->fkind != nFTKindNull &&
                        !ftManagerKindHasPack(pd->fkind))
                    {
                        syDebugPrintf("scManagerRunScene: no pack for P%d's kind %d, playing Mario\n",
                                      (int)i + 1, (int)pd->fkind);
                        pd->fkind = nFTKindMario;
                        pd->costume = 0;
                    }
                }
            }
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_VSBATTLE);
            /* DIVERGES (port only): the battle's last frame is what the
             * VS results wipe folds away (lbtransition.c), and the N64
             * keeps it in the framebuffer for free. Here the frame loop
             * has to be asked to render it into a texture (taskman.h). */
            syTaskmanWantExitPhoto();
            scVSBattleStartScene();
            break;

        /* scmanager.c:1027-1032 loads 2, 3 and 6. A bonus stage is a
         * battle scene like the two below it, with its own overlay on
         * top -- both games, Break the Targets and Board
         * the Platforms (13m), through this one arm, because one scene
         * plays either and decides which off gkind. */
        case nSCKind1PBonusStage:
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_1PBONUSSTAGE);
            sc1PBonusStageStartScene();
            break;

        /* scmanager.c:889-894 loads 2, 3 and 7. Training is a battle
         * scene: the same two overlays a VS battle takes, and its own
         * on top. It needs no pack substitution -- the select above it
         * offers only kinds the port has (src/dc/mnplayers1ptraining.c
         * walks the same roster the VS select does). */
        case nSCKind1PTrainingMode:
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_TRAININGMODE);
            sc1PTrainingModeStartScene();
            break;

        case nSCKindVSItemSwitch:
            /* scmanager.c:961-965 loads 1 and 21, and not 2 -- this is
             * the one VS scene that draws no fighter. Reached only from
             * the VS options screen's last row, which exists only once
             * the item switch is unlocked (src/dc/mnvsitemswitch.c). */
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_ITEMSWITCH);
            mnVSItemSwitchStartScene();
            break;

        case nSCKindMessage:
            /* scmanager.c:967-971 loads 2, 1 and 22. The message scene
             * runs one task per queued unlock and leaves for the
             * character select (src/dc/mnmessage.c). */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_MESSAGE);
            mnMessageStartScene();
            break;

        case nSCKindVSResults:
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_VSRESULTS);
            mnVSResultsStartScene();
            break;

        case nSCKindOption:
            /* scmanager.c:929-932 loads 1 and, on the US arm, 60. The
             * mode select's OPTION entry; its own two tabs,
             * SCREEN ADJUST and BACKUP CLEAR,
             * are both ported now and each falls to the default arm
             * below and comes straight back here with the cursor on the
             * tab that was chosen (src/dc/mnoption.h). */
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_OPTION);
            mnOptionStartScene();
            break;

        case nSCKindScreenAdjust:
            /* scmanager.c:986-992 loads 2, 1 and 25. Options' first
             * child; A, B or START all return to
             * nSCKindOption with the cursor already on this tab
             * (src/dc/mnscreenadjust.h). The decomp's own overlay 2
             * load is kept -- ScreenAdjust draws no 3D of its own, but
             * this port follows the decomp's load list rather than
             * re-deriving which overlays a scene "really" needs. */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_SCREENADJUST);
            mnScreenAdjustStartScene();
            break;

        case nSCKindBackupClear:
            /* scmanager.c:1198-1203 loads 2, 1 and 53. Options' second
             * child; B, or a confirmed clear, returns to
             * nSCKindOption with the cursor already on this tab
             * (src/dc/mnbackupclear.h). The decomp's own overlay 2 load
             * is kept, the same choice ScreenAdjust's arm above made. */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_BACKUPCLEAR);
            mnBackupClearStartScene();
            break;

        case nSCKindData:
            /* scmanager.c:939-946 loads 1 and, on the US arm, 61. All
             * three of DATA's tabs are ported now (below); each still
             * falls to the default arm below and comes straight back
             * here with the cursor on the tab that was chosen
             * (src/dc/mndata.h). */
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_DATA);
            mnDataStartScene();
            break;

        case nSCKindVSRecord:
            /* scmanager.c:1047-1051 loads 1 and 32. First of DATA's
             * three tabs to land. B
             * on its top table (Battle Score) sets scene_curr to
             * nSCKindData itself, handled above; B on either other table
             * just steps back one within this scene
             * (src/dc/mnvsrecord.h). */
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_VSRECORD);
            mnVSRecordStartScene();
            break;

        case nSCKindCharacters:
            /* scmanager.c:1054-1059 loads 2, 1 and 33: the only one of
             * DATA's three tabs that needs the battle overlay too, since
             * it plays a real fighter's demo motions (src/dc/mncharacters.c's
             * own header note on FTSTAT_CHARDATA_START). Last of DATA's
             * three tabs to land. B
             * sets scene_curr back to nSCKindData. */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_CHARACTERS);
            mnCharactersStartScene();
            break;

        case nSCKindSoundTest:
            /* scmanager.c:1250-1258 loads 1 and, on the US arm, 62.
             * Third of DATA's three tabs. B always sets scene_curr to
             * nSCKindData; the three columns' own play/stop/fade calls are
             * src/dc/mnsoundtest.h's own DIVERGES note. */
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_SOUNDTEST);
            mnSoundTestStartScene();
            break;

        case nSCKindCongra:
            /* scmanager.c:1243-1247, REGION_US only (the JP build's
             * congratulations screen is a different overlay this port
             * has no file for). Loads only overlay 57 -- the decomp's
             * own list, kept rather than re-derived. Reached from 1P
             * mode or Debug Battle, neither ported yet;
             * A/B/START all fade to black and
             * hand off to nSCKindTitle, which this scene sets itself
             * (src/dc/mncongra.h). */
            syDmaLoadOverlay(OVERLAY_CONGRA);
            mnCongraStartScene();
            break;

        case nSCKindExplain:
            /* scmanager.c:1259-1266, REGION_US arm: loads 2, 3 and 63.
             * A real battle (src/dc/scexplain.c), not a menu, so it
             * loads the same overlays 2/3 the VS battle and auto-demo
             * arms above do, plus its own. Reached the game's own way:
             * mnTitleProceedDemoNext, the title's
             * idle demonstration, 650 tics untouched (1190 after the
             * opening movie). scExplainUpdatePhase
             * and scExplainDetectExit both set scene_curr themselves
             * (to nSCKindCharacters or nSCKindTitle), so nothing here
             * does. */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_EXPLAIN);
            scExplainStartScene();
            break;

        case nSCKindAutoDemo:
            /* scmanager.c:1271-1279, REGION_US arm: loads 2, 3 and 64.
             * A real battle (src/dc/scautodemo.c), not a menu, so it
             * loads the same overlays 2/3 the VS battle arm above does,
             * plus its own -- reached from mncharacters.c's own
             * 600-tic idle-timeout branch. scAutoDemoExit sets scene_curr
             * itself, back to nSCKindStartup, so nothing here does. */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_AUTODEMO);
            scAutoDemoStartScene();
            break;

        case nSCKindEnding:
            /* scmanager.c:1206-1211: loads 2, 1 and 54 (the decomp's
             * own order, kept). One posed fighter and a fade/light
             * sequence, no fighting -- so overlay 3 (ft/ftcommon,
             * wp/it) is not loaded here, unlike nSCKindExplain/
             * nSCKindAutoDemo above. Reached today only by a direct
             * DB_BOOT_SCENE boot -- the real entry point (the 1P mode
             * clear sequence) is not ported. mvEndingFuncRun sets
             * scene_curr itself (to nSCKindStaffroll, at tic 660), so
             * nothing here does. */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_ENDING);
            mvEndingStartScene();
            break;

        case nSCKindStartup:
            /* scmanager.c:1061-1067, REGION_US only -- the whole arm
             * sits inside a #if defined(REGION_US), because a JP build
             * has no N64 logo screen and boots straight into
             * nSCKindOpeningRoom instead. Loads 1 and 58, the decomp's
             * own two, and nothing else: this scene draws one sprite.
             *
             * This is the US build's default boot scene
             * (src/dc/scmanagerdata.c:385-393) and the only route into
             * the openings chain -- mnStartupActorFuncRun sets
             * scene_curr itself, to nSCKindTitle on A/B/START or to
             * nSCKindOpeningRoom once its logo thread finishes, so
             * nothing here does (src/dc/mnstartup.h). */
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_STARTUP);
            mnStartupStartScene();
            break;

        case nSCKindOpeningRoom:
            /* scmanager.c:1069-1073. Master Hand's desk, the first of
             * the nineteen opening scenes.
             *
             * It is the default boot scene on a JP build but NOT on this
             * one: dSCManagerDefaultSceneData's REGION_US arm carries
             * nSCKindStartup as both scene_curr and scene_prev
             * (src/dc/scmanagerdata.c:385-393), and OpeningRoom is what
             * the #else arm holds. nSCKindStartup is the N64 logo screen
             * that hands off here on its own (the arm just above,
             * src/dc/mnstartup.c), and it is the only US
             * route into this chain.
             *
             * The decomp's own arm loads 1 and 34. This port also loads
             * 2, which the decomp does not -- an extra re-zero of the
             * battle overlay's statics, not a missing one, so it costs
             * correctness nothing; it is recorded here rather than
             * removed because this scene is disc-verified as it stands
             * and nothing has established whether the extra clear is
             * load-bearing for the two posed trophy fighters. Settle it
             * when the next opening scene lands.
             *
             * Two posed trophy fighters and a tic-timed camera cut
             * sequence, no fighting -- so overlay 3 (ft/ftcommon, wp/it)
             * is not loaded here, the same choice the nSCKindEnding arm
             * above makes for the same reason.
             * mvOpeningRoomFuncRun sets scene_curr itself, either to
             * nSCKindTitle (A/B/START) or to nSCKindOpeningPortraits
             * (tic 22s), so nothing here does. That second destination
             * is ported now (the arm just below); it was not
             * when this comment was first written, and the default arm's
             * "not ported, back to scene N" loop was the observed
             * behaviour then -- src/dc/mvopeningroom.h still says why
             * that reading was correct and not a hang, for whichever
             * scene in the chain is the next one still missing. */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_SUBSYS);
            syDmaLoadOverlay(OVERLAY_OPENINGROOM);
            mvOpeningRoomStartScene();
            break;

        case nSCKindOpeningPortraits:
            /* scmanager.c:1075-1078. Two sets of four fighter portraits
             * sliding onto a 4-row grid behind a wipe-cover sprite --
             * pure sprite content, no fighters, no relocData scene-graph
             * gap, so this is the one scene of the remaining eighteen
             * that needed no new tooling (src/dc/mvopeningportraits.h's
             * header note calls this out as "tier 1"). Loads only
             * overlay 35, the decomp's own single load.
             * mvOpeningPortraitsFuncRun sets scene_curr itself, to
             * nSCKindTitle (A/B/START) or to nSCKindOpeningMario (tic
             * 150), so nothing here does. Mario is not ported yet, so
             * the default arm below catches it and this scene loops --
             * the same reading room's own arm above already documents,
             * one scene further down the chain. */
            syDmaLoadOverlay(OVERLAY_OPENINGPORTRAITS);
            mvOpeningPortraitsStartScene();
            break;

        case nSCKindOpeningMario:
            /* scmanager.c:1080-1084, REGION_US arm: loads 3 and 36 --
             * the decomp's own list, and notably NOT overlay 2 (ft, mp,
             * gm, gr, if, ef), even though mvOpeningMarioFuncStart calls
             * straight into all of those modules (a real stage via
             * grStageAcquire/stage_bind, a real fighter via
             * ftManagerAllocFighter/ftManagerMakeFighter, mpCollision*
             * for the spawn point, efParticleInitAll/efManagerInitEffects).
             * That is because overlay 2 is not reloaded anywhere in the
             * openings chain the real game's own arms show (room's own
             * arm above loads 1, not 2 either) -- the chain runs as one
             * continuous sequence and never re-zeroes it once past
             * Startup. This port still loads it here, the same
             * deliberate "extra re-zero, costs nothing" liberty room's
             * own arm above already takes and flags as unsettled: this
             * is the next opening scene landing, and it turns out the
             * decomp's own answer is "no, overlay 2 is not part of any
             * scene's per-scene reload in this chain" -- not evidence
             * that room's own trophies needed it, just confirmation that
             * keeping the extra clear costs nothing here either, for a
             * scene that leans on overlay 2's pools far more directly
             * than room's cut room-set ever did. Overlay 1 (sc/scsubsys)
             * is NOT loaded, matching the decomp exactly: this scene's
             * own scSubsysFighterSetStatus call evidently does not need
             * it, unlike room's own reasons for loading it.
             * mvOpeningMarioFuncRun sets scene_curr itself, to
             * nSCKindTitle (A/B/START) or to nSCKindOpeningDonkey (tic
             * 60), so nothing here does. Donkey is not ported yet, so
             * the default arm below catches it and this scene loops --
             * the same reading every scene above it in the chain already
             * documents. */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGMARIO);
            mvOpeningMarioStartScene();
            break;

        case nSCKindOpeningDonkey:
            /* scmanager.c:1086-1089, REGION_US arm: loads only overlay
             * 37 (itself) -- unlike Mario's own arm just above, the decomp's
             * own list here has no overlay 2 or 3. That is because the real
             * console reaches this scene only by falling straight through from
             * Mario within the same continuous run (nothing in the chain
             * unloads 2/3 in between); the enum's own declaration order
             * (nSCKindOpeningSamus, nSCKindOpeningFox sit between Donkey and
             * Link there) is not the chain's runtime order --
             * mvOpeningDonkeyFuncRun's own tic-60 arm hands off to
             * nSCKindOpeningLink directly, checked against the decomp source,
             * not assumed. This port's arm takes the same "extra re-zero,
             * costs nothing" liberty Mario's own arm already takes over the
             * decomp's list (mvopeningdonkey.h's own header, Substitution 6):
             * OVERLAY_BATTLE and OVERLAY_FIGHTING are reloaded here too, so a
             * probe that boots directly into this scene via DB_BOOT_SCENE
             * (never having run Mario's own scene first) still gets a freshly
             * zeroed fighter/stage/camera pool. mvOpeningDonkeyFuncRun sets
             * scene_curr itself, to nSCKindTitle (A/B/START) or to
             * nSCKindOpeningLink (tic 60), so nothing here does -- Link is
             * ported (case nSCKindOpeningLink below) and picks the hand-off up
             * from there. */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGDONKEY);
            mvOpeningDonkeyStartScene();
            break;

        case nSCKindOpeningLink:
            /* scmanager.c:1101-1104, REGION_US arm: loads only overlay
             * 40 (itself), the same "falls through with 2/3 already resident"
             * shape Donkey's own arm has -- confirmed directly against the
             * decomp source, not assumed: Donkey hands off to this scene at
             * its own tic 60 (37 -> 40, skipping the enum's Samus/Fox
             * entries), and this scene's own mvOpeningLinkFuncRun hands off at
             * its own tic 60 to nSCKindOpeningSamus (40 -> 38) -- the chain
             * doubles back through the overlay numbering rather than climbing
             * it, so neither the enum's declaration order nor the overlay
             * numbers say anything about play order on their own. This port's
             * arm takes mvopeningdonkey.h's own Substitution 6 liberty again:
             * OVERLAY_BATTLE and OVERLAY_FIGHTING are reloaded here too, so a
             * probe that boots directly into this scene via DB_BOOT_SCENE
             * (never having run Mario's or Donkey's own scenes first) still
             * gets a freshly zeroed fighter/stage/camera pool.
             * mvOpeningLinkFuncRun sets scene_curr itself, to nSCKindTitle
             * (A/B/START) or to nSCKindOpeningSamus (tic 60), so nothing here
             * does -- Samus is ported (case nSCKindOpeningSamus below) and
             * picks the hand-off up from there. */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGLINK);
            mvOpeningLinkStartScene();
            break;

        case nSCKindOpeningSamus:
            /* scmanager.c:1091-1094, REGION_US arm: loads only overlay
             * 38 (itself), the same "falls through with 2/3 already resident"
             * shape Donkey's/Link's own arms have -- confirmed directly
             * against the decomp source, not assumed: Link hands off to this
             * scene at its own tic 60 (40 -> 38, mvopeninglink.h's own note),
             * and this scene's own mvOpeningSamusFuncRun hands off at its own
             * tic 60 to nSCKindOpeningYoshi (38 -> 41), skipping Fox (39)
             * entirely again -- the chain doubles back through the overlay
             * numbering rather than climbing it, the same reading
             * mvopeninglink.h already establishes. This port's arm takes
             * mvopeningdonkey.h's own Substitution 6 liberty again:
             * OVERLAY_BATTLE and OVERLAY_FIGHTING are reloaded here too, so a
             * probe that boots directly into this scene via DB_BOOT_SCENE
             * (never having run Mario's, Donkey's, or Link's own scenes first)
             * still gets a freshly zeroed fighter/stage/camera pool.
             * mvOpeningSamusFuncRun sets scene_curr itself, to nSCKindTitle
             * (A/B/START) or to nSCKindOpeningYoshi (tic 60), so nothing here
             * does -- Yoshi is ported (case nSCKindOpeningYoshi below) and
             * picks up the hand-off from there. */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGSAMUS);
            mvOpeningSamusStartScene();
            break;

        case nSCKindOpeningYoshi:
            /* scmanager.c:1106-1109, REGION_US arm: loads only overlay
             * 41 (itself), the same "falls through with 2/3 already
             * resident" shape Donkey's/Link's/Samus's own arms have --
             * confirmed directly against the decomp source, not
             * assumed: Samus hands off to this scene at its own tic 60
             * (38 -> 41, mvopeningsamus.h's own note), and this scene's
             * own mvOpeningYoshiFuncRun hands off at its own tic 60 to
             * nSCKindOpeningKirby (41 -> 43), skipping Pikachu (42)
             * entirely -- the chain doubles back through the overlay
             * numbering rather than climbing it, the same reading
             * mvopeninglink.h/mvopeningsamus.h already establish. This
             * port's arm takes mvopeningdonkey.h's own Substitution 6
             * liberty again: OVERLAY_BATTLE and OVERLAY_FIGHTING are
             * reloaded here too, so a probe that boots directly into
             * this scene via DB_BOOT_SCENE (never having run Mario's,
             * Donkey's, Link's, or Samus's own scenes first) still gets
             * a freshly zeroed fighter/stage/camera pool.
             * mvOpeningYoshiFuncRun sets scene_curr itself, to
             * nSCKindTitle (A/B/START) or to nSCKindOpeningKirby (tic
             * 60), so nothing here does. Kirby is ported (the arm below). */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGYOSHI);
            mvOpeningYoshiStartScene();
            break;

        case nSCKindOpeningKirby:
            /* scmanager.c REGION_US arm: loads only overlay 43 (itself),
             * confirmed against the decomp. Reached from Yoshi's tic 60
             * (41 -> 43); mvOpeningKirbyFuncRun hands off at its own tic
             * 60 to nSCKindOpeningFox (39), which is not ported, so the
             * default arm loops this scene back into itself -- the
             * reading every scene above it already documents. Battle and
             * fighting are reloaded too (mvopeningdonkey.h Substitution
             * 6). mvOpeningKirbyFuncRun sets scene_curr itself. */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGKIRBY);
            mvOpeningKirbyStartScene();
            break;

        case nSCKindOpeningFox:
            /* scmanager.c REGION_US arm: loads only overlay 39 (itself).
             * Reached from Kirby's tic 60 (43 -> 39);
             * mvOpeningFoxFuncRun hands off at its own tic 60 to
             * nSCKindOpeningPikachu (42), not ported, so the default arm
             * loops this scene back into itself. Battle and fighting are
             * reloaded too (mvopeningdonkey.h Substitution 6). */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGFOX);
            mvOpeningFoxStartScene();
            break;

        case nSCKindOpeningPikachu:
            /* scmanager.c REGION_US arm: loads only overlay 42 (itself).
             * Reached from Fox's tic 60 (39 -> 42);
             * mvOpeningPikachuFuncRun hands off at its own tic 60 to
             * nSCKindOpeningRun, not ported, so the default arm loops
             * this scene back into itself. Battle and fighting are
             * reloaded too (mvopeningdonkey.h Substitution 6). */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGPIKACHU);
            mvOpeningPikachuStartScene();
            break;

        case nSCKindOpeningRun:
            /* scmanager.c REGION_US arm: loads only overlay 44 (itself).
             * Reached from Pikachu's tic 60 (42 -> 44);
             * mvOpeningRunFuncRun hands off at its own tic 220 to
             * nSCKindOpeningCliff (ported). Battle and fighting are
             * reloaded too (mvopeningdonkey.h Substitution 6): this
             * scene holds all eight fighters. */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGRUN);
            mvOpeningRunStartScene();
            break;

        case nSCKindOpeningCliff:
            /* scmanager.c REGION_US arm: loads only overlay 46 (itself).
             * Reached from Run's tic 220 (44 -> 46);
             * mvOpeningCliffFuncRun hands off at its own tic 160 to
             * nSCKindOpeningYamabuki (ported). Battle and fighting
             * are reloaded too (mvopeningdonkey.h Substitution 6). */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGCLIFF);
            mvOpeningCliffStartScene();
            break;

        case nSCKindOpeningYamabuki:
            /* scmanager.c REGION_US arm: loads only overlay 48 (itself).
             * Reached from Cliff's tic 160 (46 -> 48);
             * mvOpeningYamabukiFuncRun hands off at its own tic 160 to
             * nSCKindOpeningJungle (ported); it loops
             * this scene back into itself. Battle and fighting are
             * reloaded too (mvopeningdonkey.h Substitution 6). */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGYAMABUKI);
            mvOpeningYamabukiStartScene();
            break;

        case nSCKindOpeningJungle:
            /* scmanager.c REGION_US arm: loads only overlay 51 (itself).
             * Reached from Yamabuki's tic 160 (48 -> 51);
             * mvOpeningJungleFuncRun hands off at its own tic 320 to
             * nSCKindOpeningYoster (ported); it loops
             * this scene back into itself. Battle and fighting are
             * reloaded too (mvopeningdonkey.h Substitution 6). */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGJUNGLE);
            mvOpeningJungleStartScene();
            break;

        case nSCKindOpeningYoster:
            /* scmanager.c REGION_US arm: loads only overlay 45 (itself).
             * Reached from Jungle's tic 320 (51 -> 45);
             * mvOpeningYosterMainProc hands off at its own tic 160 to
             * nSCKindOpeningSector (ported); it loops
             * this scene back into itself. Battle and fighting are
             * reloaded too (mvopeningdonkey.h Substitution 6). */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGYOSTER);
            mvOpeningYosterStartScene();
            break;

        case nSCKindOpeningSector:
            /* scmanager.c REGION_US arm: loads only overlay 50 (itself).
             * Reached from Yoster's tic 160 (45 -> 50);
             * mvOpeningSectorFuncRun hands off at its own tic 160 to
             * nSCKindOpeningStandoff (ported). Battle and fighting are
             * reloaded too (mvopeningdonkey.h Substitution 6). */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGSECTOR);
            mvOpeningSectorStartScene();
            break;

        case nSCKindOpeningStandoff:
            /* scmanager.c REGION_US arm: loads only overlay 47 (itself).
             * Reached from Sector's tic 160 (50 -> 47);
             * mvOpeningStandoffFuncRun hands off at its own tic 320 to
             * nSCKindOpeningClash (ported). Battle and fighting are
             * reloaded too (mvopeningdonkey.h Substitution 6). */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGSTANDOFF);
            mvOpeningStandoffStartScene();
            break;

        case nSCKindOpeningClash:
            /* scmanager.c REGION_US arm: loads only overlay 49 (itself).
             * Reached from Standoff's tic 320 (47 -> 49);
             * mvOpeningClashFuncRun hands off at its own tic 160 to
             * nSCKindOpeningNewcomers (ported). Battle and
             * fighting are reloaded too (mvopeningdonkey.h Substitution
             * 6). */
            syDmaLoadOverlay(OVERLAY_BATTLE);
            syDmaLoadOverlay(OVERLAY_FIGHTING);
            syDmaLoadOverlay(OVERLAY_OPENINGCLASH);
            mvOpeningClashStartScene();
            break;

        case nSCKindOpeningNewcomers:
            /* scmanager.c REGION_US arm: loads only overlay 52 (itself).
             * Reached from Clash's tic 160 (49 -> 52); it is the last of
             * the movie: mvOpeningNewcomersFuncRun goes to nSCKindTitle at
             * its own tic 40. Battle and fighting are not needed (no
             * fighter is made), so only the scene's own overlay loads. */
            syDmaLoadOverlay(OVERLAY_OPENINGNEWCOMERS);
            mvOpeningNewcomersStartScene();
            break;

        case nSCKindDCMemCard:
            /* DIVERGES: the port's own scene (src/dc/scport.h), the
             * memory card's page (src/dc/dcmemcard.h). No
             * syDmaLoadOverlay, because it has no overlay and it must
             * not clear one: it sets every static of its own when it
             * starts, and the scene it returns to keeps its own. */
            dcMemCardStartScene();
            break;

        case nSCKindStaffroll:
            /* scmanager.c:1234-1241, REGION_US arm: loads only 59 --
             * no fighters, no battle interface, so none of the shared
             * overlays above are needed. Reached from mvEndingFuncRun
             * (this scene) or by a direct DB_BOOT_SCENE boot.
             * scStaffrollFuncDraw sets scene_curr itself (to
             * nSCKindStartup), so nothing here does. */
            syDmaLoadOverlay(OVERLAY_STAFFROLL);
            scStaffrollStartScene();
            break;

        default:
            /* DIVERGES: back to the scene that asked, as if the player
             * had backed out of the one it wanted (the header) */
            syDebugPrintf("scManagerRunScene: scene %d is not ported, back to scene %d\n",
                          (int)gSCManagerSceneData.scene_curr,
                          (int)gSCManagerSceneData.scene_prev);
            {
                s32 asked = gSCManagerSceneData.scene_curr;

                gSCManagerSceneData.scene_curr = gSCManagerSceneData.scene_prev;
                gSCManagerSceneData.scene_prev = asked;
            }
            break;
        }
        /* The banks a scene loaded, given back with the scene. 
         * syTaskmanResetGeneralHeap does this too, for the
         * case this line cannot see -- a second syTaskmanStartTask
         * inside one scene, which is what sudden death is -- and by the
         * time that runs there is nothing left to release. Both calls
         * stay: this one frees the VRAM at the scene change, where the
         * chain's peak is measured, rather than holding it until the
         * next scene starts. */
        sprite_bank_release_all();
        /* and the fighter packs the scene loaded: the
         * same two places, for the same reason -- except the ones the
         * scene about to start will ask for again, which
         * stay where they are. scene_curr is the scene the one just
         * ending named, so this is the only place in the port that
         * knows both what is going and what is coming; src/dc/ftmanager.c
         * says what the game does instead, and why the port does not. */
        ftManagerKeepFilesForScene(gSCManagerSceneData.scene_curr);
        ftManagerReleaseFilesAll();
        /* The scene's bill, before the next scene's window opens. On a
         * romdisk build the bytes are near zero and the files are not,
         * because a mapped file crosses no medium; on the disc build
         * this line is the load screen. */
        asset_io_report(scene);
#if defined(DB_STACK_GUARD) && defined(_arch_dreamcast)
        /* src/dc/db.h: how deep every kind of stack has gone so far */
        stack_guard_report(scene);
#endif
        /* src/dc/textguard.h: every block of code and constants, once */
        TEXT_GUARD_ALL(scene);
        /* and the files the opening movie reads in scene after scene,
         * held in RAM from the Room through Clash (assetroot.h, the
         * hold list) and given back on the way out of it -- after the
         * bill, whose "held" figure is the scene's own */
        scManagerHoldFilesForScene(gSCManagerSceneData.scene_curr);
        /* the run-long models for the scene about to start, while the
         * heap holds nothing of the scene just ended */
        scManagerModelsForScene(gSCManagerSceneData.scene_curr);
#ifdef SC_LOOP_SCENE
        /* diagnostic build only: replay one scene forever, so a fault
         * that needs a second visit needs seconds and not a match */
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = SC_LOOP_SCENE;
#endif
        /* and what the loader read ahead for the scenes passed goes
         * too, and it queues on from the scene about to start */
        scManagerPrefetchForScene(gSCManagerSceneData.scene_curr);
    }
}
