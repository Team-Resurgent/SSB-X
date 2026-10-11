/* scvsbattle.c -- see scvsbattle.h. Function-for-function from
 * ssb-decomp-re/src/sc/sccommon/scvsbattle.c; every function names its
 * line range.
 */
#include "scvsbattle.h"
#include "overlay.h"

#include "ftcommon.h"
#include "ftshadow.h"
#include "gmcamera.h"
#include "gmcommon.h"
#include "ifcommon.h"
#include "input.h"
#include "scmanager.h"
#include "dma.h"
#include "lbparticle.h"
#include "lbpartex.h"
#include "lbpdraw.h"
#include "efmanager.h"
#include "itemmodel.h"
#include "wpattrs.h"
#include "assetroot.h"
#include "stage.h"
#include "taskman.h"

#include <ef/efdisplay.h>
#include <ef/efparticle.h>
#include <ef/effect.h>
#include <ft/fighter.h>
#include <ft/ftdef.h>
#include <ft/ftparam.h>
#include <ft/ftpublic.h>
#include <gm/gmrumble.h>
#include <gm/gmsound.h>
#include <gr/ground.h>
#include <if/ifcommon.h>
#include <mp/map.h>
#include <sys/controller.h>
#include <lb/lbfade.h>

/* ---- the pools ------------------------------------------------------
 *
 * sys/taskman.c:1249-1285 cuts one pool per count out of the scene heap.
 * The decomp's own dSCVSBattleTaskmanSetup carries zero for every one of
 * them (scvsbattle.c:47-63), so the real numbers are still in the ROM's
 * data and not in the decomp; these are the port's, chosen with headroom
 * and checked against the high-water mark src/dc/db.c prints.
 *
 * What is actually in use with two fighters on Hyrule: 6 GObjs; 70 DObjs
 * (the stage's 18 joints, and per fighter Mario's 25 and the TopN above
 * them); 212 XObjs, because gcAddDObjMatrixSets* gives every DObj three
 * (objhelper.c:267-280) and the camera takes two; and one AObj per
 * animated track per animated joint, allocated on demand by
 * gcAddAObjForDObj and reused for the life of the fighter -- eighteen of
 * Mario's joints carry six tracks each in his idle, and no animation on
 * the roster asks for all ten of all thirty-six. Running a pool dry is
 * not fatal -- gcGetXObjSetNextAlloc falls back to syTaskmanMalloc
 * (objman.c:576-580) -- it just moves the cost onto the scene heap,
 * silently. They grow again when the effects arrive.
 *
 * The HUD is where the SObjs went: per player five for
 * the damage (the emblem and four digits), six for the stock row, one
 * for the tag, and on top the countdown's eleven, GO!'s three and GAME
 * SET's seven -- 69 with four players. Each of those rows is a GObj
 * too, and the interface camera a CObj.
 */
#define SCVSBATTLE_GOBJS       48
#define SCVSBATTLE_GOBJPROCS   48
#define SCVSBATTLE_XOBJS      288
#define SCVSBATTLE_AOBJS      576
#define SCVSBATTLE_MOBJS       32
#define SCVSBATTLE_DOBJS       96
#define SCVSBATTLE_SOBJS       96
#define SCVSBATTLE_COBJS        6

/* scvsbattle.c:15 (0x8018E8A0): the colour the match fades in from,
 * black, with the alpha byte 0 saying "from the colour to the scene"
 * (src/dc/lbfade.c). */
SYColorRGBA dSCVSBattleCommonFadeColor = { 0x00, 0x00, 0x00, 0x00 };

/* scvsbattle.c:18 (0x8018E3D4): sudden death's fade colour, byte for
 * byte the same black as the one above. The game keeps two so that the
 * second battle fades in from its own; the port keeps two for the same
 * reason it keeps every other duplicate -- it is what is there. */
SYColorRGBA dSCVSBattleSuddenDeathFadeColor = { 0x00, 0x00, 0x00, 0x00 };

/* scvsbattle.c:24-64 dSCVSBattleTaskmanSetup.
 *
 * DIVERGES: the display-list buffers, the graphics arena and the RDP
 * output buffer are zeroed -- there is no RSP and no RDP here, and with
 * them goes the 0xD000 the battle scene's graphics arena would have
 * taken out of the scene heap. func_lights is NULL for the same reason:
 * scVSBattleFuncLights (scvsbattle.c:4507) emits a gSPSetGeometryMode
 * and the reflected fighter lights into a display list, and the port's
 * lighting is the PVR back end's (src/dc/objdisplay.c). arena_start is
 * NULL, which syTaskmanStartTask reads as "keep the region
 * syTaskmanMakeGeneralHeap already made"; the game's is &ovl4_BSS_END,
 * a link-map address with no meaning in an ELF KallistiOS laid out.
 * dLBCommonFuncMatrixList, the matrix function list, is NULL: the
 * port's matrix kinds are a switch in objdisplay.c, not a list. */
SYTaskmanSetup dSCVSBattleTaskmanSetup =
{
    {
        0,                              /* flags */
        scVSBattleFuncUpdate,           /* update function */
        scManagerFuncDraw,              /* frame draw function */
        NULL,                           /* allocatable memory pool start */
        0,                              /* allocatable memory pool size */
        1,                              /* ??? */
        2,                              /* number of contexts? */
        0, 0, 0, 0,                     /* the four DL buffer sizes */
        0,                              /* graphics heap size */
        2,                              /* ??? */
        0,                              /* RDP output buffer size */
        NULL,                           /* pre-render function */
        syControllerFuncRead,           /* controller I/O function */
    },

    0,                                  /* number of GObjThreads */
    sizeof(u64) * 192,                  /* thread stack size */
    0,                                  /* number of thread stacks */
    0,                                  /* ??? */
    SCVSBATTLE_GOBJPROCS,
    SCVSBATTLE_GOBJS,   sizeof(GObj),
    SCVSBATTLE_XOBJS,
    NULL,                               /* matrix function list */
    NULL,                               /* DObjVec eject function */
    SCVSBATTLE_AOBJS,
    SCVSBATTLE_MOBJS,
    SCVSBATTLE_DOBJS,   sizeof(DObj),
    SCVSBATTLE_SOBJS,   sizeof(SObj),
    SCVSBATTLE_COBJS,   sizeof(CObj),

    scVSBattleStartBattle               /* task start function */
};

/* The port's own -- see scvsbattle.h. */
void (*gSCVSBattleFuncDebug)(void);

/* sc/sccommon/scvsbattlefiles.c:23-38 scVSBattleSetupFiles 0x8018E330.
 * DIVERGES: the game's sets up the reloc loader's status buffers -- the
 * table of which ROM files are resident -- and then loads the eight
 * common files. There is no reloc loader here; what remains is the
 * load, and it is the port's (src/dc/gmcommon.c). */
void scVSBattleSetupFiles(void)
{
    gmCommonLoadFiles();
}

/* scvsbattle.c:74-77, verbatim. One line, and it is the whole of what a
 * battle does per tic: the dispatcher on game_status, whose every arm
 * runs the object system exactly once (src/dc/ifcommon.c). */
void scVSBattleFuncUpdate(void)
{
    ifCommonBattleUpdateInterfaceAll();
}

/* scvsbattle.c:80-121 scVSBattleGetStartPlayerLR 0x8018D0E0, verbatim
 * but for near_dist's initial value: face the nearest other team's
 * spawn point.
 *
 * DIVERGES: 65536.0F becomes 99999.0F. Hyrule's spawn points are inside
 * the N64 value and every stage's are, so nothing on the roster can tell
 * the difference; it is a guard against a stage whose
 * spawns are further apart than that and is kept because it costs
 * nothing. */
s32 scVSBattleGetStartPlayerLR(s32 this_player)
{
    Vec3f this_spawn_pos;
    Vec3f loop_spawn_pos;
    s32 loop_player;
    f32 near_dist;
    f32 near_spawn;
    f32 distx;
    s32 lr;

    near_dist = 99999.0F;
    near_spawn = 0.0F;

    mpCollisionGetPlayerMapObjPosition(this_player, &this_spawn_pos);

    for (loop_player = 0; loop_player < ARRAY_COUNT(gSCManagerBattleState->players); loop_player++)
    {
        if (loop_player == this_player)
        {
            continue;
        }
        else if (gSCManagerBattleState->players[loop_player].pkind == nFTPlayerKindNot)
        {
            continue;
        }
        else if (gSCManagerBattleState->players[loop_player].player != gSCManagerBattleState->players[this_player].player)
        {
            mpCollisionGetPlayerMapObjPosition(loop_player, &loop_spawn_pos);

            distx = (loop_spawn_pos.x < this_spawn_pos.x) ? -(loop_spawn_pos.x - this_spawn_pos.x) : (loop_spawn_pos.x - this_spawn_pos.x);

            if (near_dist > distx)
            {
                near_dist = distx;
                near_spawn = loop_spawn_pos.x - this_spawn_pos.x;
            }
        }
    }
    lr = (near_spawn >= 0.0F) ? +1 : -1;

    return lr;
}

/* scvsbattle.c:124-225 scVSBattleStartBattle 0x8018D228, the setup the
 * whole milestone has been aimed at: every GObj a match needs, in the
 * order the game makes them. SYTaskmanSetup.func_start, so
 * syTaskmanLoadScene calls it once the pools exist and the frame loop
 * starts the moment it returns.
 *
 * Ten of its calls are not here. Every one of them belongs to a
 * module the port has not reached, and dropping the call is what
 * "unported" looks like from inside a faithful sequence:
 *
 *   ftManagerSetupFilesPlayablesAll -- the ROM loader for every
 *       playable kind's model and motion files. The port's packs are
 *       loaded whole before the scene starts and stream per scene from
 *       the disc. scVSBattleSetupFiles and
 *       ftManagerSetupFilesAllKind, loading
 *       what they load that is a scene asset: the HUD's sprite banks
 *       and each fighter's stock icon and emblem.
 *   the llSYKseg1Validate block -- an anti-tamper check that runs
 *       relocatable code out of the ROM and sets a backup flag. There is
 *       no ROM and no backup data (gSCManagerBackupData is zeroed,
 *       src/dc/scmanager.c), so it is gone rather than half-written.
 *   gcMakeDefaultCameraGObj -- the black clear camera. The PVR clears
 *       its own framebuffer.
 *   efManagerInitEffects -- ef/efmanager.c, 4.1k lines.
 *       efParticleInitAll, and so is the one
 *       line of efManagerInitEffects that does not need it: see
 *       scVSBattleLoadEffectBank below. (efGroundMakeAppearActor is not
 *       this function's call at all -- the decomp reaches it from
 *       grCommonSetupInitAll, called below; see src/dc/stage.c.)
 *   (itManagerInitItems and gmRumbleMakeActor are not on this list: both
 *       are called below. Without the first, gITManagerCommonData stays
 *       NULL and every item this port can spawn fails on the first line
 *       that reads its attributes. Without gmRumbleMakeActor,
 *       ftParamMakeRumble's own gmRumbleSetPlayerRumbleParams walks a
 *       NULL sGMRumblePlayers[player].rlink the first time any already-
 *       ported hit/damage/death/capture/item path calls it.)
 *   gmCameraMakeWallpaperCamera -- see grWallpaperMakeDecideKind
 *       (src/dc/stage.c) for where the wallpaper goes instead.
 *
 * mpCollisionInitGroundData is here under the port's own name, because
 * the decomp's is in the link and reads a ROM file -- see
 * stage_bind_collision (src/dc/stage.c).
 */
/* ef/efmanager.c:1734 efManagerInitEffects, at both of scvsbattle.c's
 * sites (:164 and :436), through the port's wrapper that times it and
 * prints what the bank walk found -- efManagerLoadEffectBank, in
 * src/dc/efmanager.c, which the results screen's confetti uses too. */

void scVSBattleStartBattle(void)
{
    s32 player;
    FTDesc desc;
    SYColorRGBA color;

    gSCManagerSceneData.is_reset = FALSE;
    gSCManagerSceneData.is_suddendeath = FALSE;

    /* scvsbattle.c:137: the HUD's files (src/dc/gmcommon.c) */
    scVSBattleSetupFiles();

    /* scvsbattle.c:153: the particle pools -- 112 structs, 24
     * generators, 80 transforms out of the scene heap -- and the bank
     * cache they are indexed from. Before ftParamInitGame,
     * as there. */
    efParticleInitAll();

    /* scvsbattle.c:154: clear the ground obstacle and hazard tables, and
     * set the placement counter from the number of teams about to play
     * -- the counter the KO timeline walks down and, when it reaches
     * zero, ends the match on (src/dc/ifcommon.c). */
    ftParamInitGame();

    /* scvsbattle.c:155 mpCollisionInitGroundData: the game's reads the
     * stage's ground file off the ROM; the port's tables are already in
     * RAM (src/dc/stage.c). It belongs here and not before the scene
     * because syTaskmanStartTask has just emptied the heap the tables
     * come out of. */
    stage_bind_collision();

    gmCameraSetViewportDimensions(10, 10, 310, 230);

    /* scvsbattle.c:160 (:432) itManagerInitItems: the item struct pool,
     * the item pack and the item particle bank. The same shape as
     * wpManagerAllocWeapons, a few lines below. Without it
     * gITManagerCommonData is NULL, and that is the first thing
     * itManagerMakeItem reaches for: every item this port can spawn would
     * fail on the line that reads its attributes, exactly as every weapon
     * failed on wpManagerGetNextStructAlloc before that step. */
    itManagerInitItems();

    grCommonSetupInitAll();

    /* scvsbattle.c:161-202, the manager allocation and the per-player
     * spawn loop as the game writes them. Everything below the GObj is
     * the game's: ftManagerMakeFighter builds the TopN DObj and the
     * model's tree under it, hangs the two update processes on it, and
     * settles it; the animation runs on those DObjs through the decomp's
     * own parsers, and the drawing is dc_model_proc_display. */
    ftManagerAllocFighter(FTDATA_FLAG_MAINMOTION, GMCOMMON_PLAYERS_MAX);

    /* scvsbattle.c:163 wpManagerAllocWeapons: the weapon struct pool,
     * WEAPON_ALLOC_MAX of them out of the scene heap. Without this line the pool
     * head is NULL without it, wpManagerGetNextStructAlloc is the FIRST
     * line of wpManagerMakeWeapon, and so every projectile this port has
     * ever compiled failed on that line before touching a model. Mario's
     * fireball and Fox's Blaster could never spawn without it. */
    wpManagerAllocWeapons();

    /* scvsbattle.c:164 efManagerInitEffects, as far as it goes here. */
    efManagerLoadEffectBank();

    /* DIVERGES: every effect, weapon and item model the port has a pack
     * for, loaded here rather than the first time one is made -- where
     * the game already has them, in files loaded before the first tic
     * (src/dc/efmanager.c efManagerPreloadModels says the rest). Kept for
     * the run, so only the first battle pays. */
    efManagerPreloadModels();
    wpManagerPreloadModels();
    itemModelPreloadAll();
    /* scvsbattle.c:165 (:437): the screen flash. */
    ifScreenFlashMakeInterface(0xFF);

    /* scvsbattle.c:166 (:438) gmRumbleMakeActor: gm/gmrumble.c.
     * Sets up the per-player link-chain the rest of the rumble engine
     * (ftParamMakeRumble's own gmRumbleSetPlayerRumbleParams, called
     * from all over the already-ported hit/damage/death/capture/item
     * paths) assumes exists before the first hit of a real match. */
    gmRumbleMakeActor();

    /* scvsbattle.c:167 ftPublicMakeActor: the crowd. ft/ftpublic.c is in
     * the link, compiled unmodified, so this is the
     * game's own -- the GObj whose update ticks the audience's reaction
     * timer, chants a player's name once they pass 100%, and drains the
     * "<player> defeated" announcer queue ifcommon.c fills. It is made
     * after the fighter pool and before the spawn loop because its
     * update walks gGCCommonLinks[nGCCommonLinkIDFighter]. */
    ftPublicMakeActor();

    for (player = 0; player < ARRAY_COUNT(gSCManagerBattleState->players); player++)
    {
        desc = dFTManagerDefaultFighterDesc;

        if (gSCManagerBattleState->players[player].pkind == nFTPlayerKindNot)
        {
            continue;
        }
        desc.fkind = gSCManagerBattleState->players[player].fkind;

        /* scvsbattle.c:177: the kind's files, once per kind */
        ftManagerSetupFilesAllKind(desc.fkind);

        mpCollisionGetPlayerMapObjPosition(player, &desc.pos);

        desc.lr = scVSBattleGetStartPlayerLR(player);

        desc.team = gSCManagerBattleState->players[player].player;
        desc.player = player;

        desc.detail = ((gSCManagerBattleState->pl_count + gSCManagerBattleState->cp_count) < 3) ? nFTPartsDetailHigh : nFTPartsDetailLow;

        desc.costume = gSCManagerBattleState->players[player].costume;
        desc.shade = gSCManagerBattleState->players[player].shade;
        desc.handicap = gSCManagerBattleState->players[player].handicap;
        desc.level = gSCManagerBattleState->players[player].level;
        desc.stock_count = gSCManagerBattleState->stocks;
        desc.damage = 0;
        desc.pkind = gSCManagerBattleState->players[player].pkind;
        desc.controller = &gSYControllerDevices[player];

        desc.figatree_heap = ftManagerAllocFigatreeHeapKind(gSCManagerBattleState->players[player].fkind);

        ftParamInitPlayerBattleStats(player, ftManagerMakeFighter(&desc));
    }

    /* scvsbattle.c:204: the status the entry sequence starts from. */
    ifCommonBattleSetGameStatusWait();

    /* scvsbattle.c:166-168. The battle camera is made after the fighters
     * rather than before them, because the port's follows every fighter
     * that exists when it is made; the game's finds them by link. The
     * DL links it captures are set here rather than per pass as
     * gmCameraDefaultProcDisplay does (src/dc/gmcamera.c on why), and
     * the wallpaper hangs off its func_camera.
     *
     * The mask is the game's whole set (GMCAMERA_BATTLE_DLLINK_MASK,
     * src/dc/gmcamera.h), not a census of the links the port uses: a
     * hand-picked subset would miss links (stage hazards on 4/6/12/16,
     * items on 11, weapons on 14) whose GObjs then
     * animated correctly in memory (gcPlayAnimAll still ran on them every
     * tic) but were never captured for drawing. Both were found from real
     * bug reports, not code review: there is no log line that would show
     * a GObj was silently never drawn. */
    gmCameraMakeBattleCamera();
    gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK;
    grWallpaperMakeDecideKind();

    /* scvsbattle.c:209-215: the screen flash's camera, the HUD's camera
     * and the HUD -- the tags, the damage digits with the emblems, the
     * stock rows -- in the game's order (src/dc/ifcommon.c), with the
     * effects camera between the HUD's camera and the HUD (both
     * cameras: src/dc/gmcamera.c). */
    /* scvsbattle.c:205-208: the off-screen arrows and the camera that
     * draws them. They come before the HUD's camera, as
     * there, so the arrows are drawn under it. */
    gmCameraMakePlayerMagnifyCamera();
    ifCommonPlayerMagnifyMakeInterface();
    gmCameraMakePlayerArrowsCamera();
    ifCommonPlayerArrowsInitInterface();

    gmCameraScreenFlashMakeCamera();
    gmCameraMakeInterfaceCamera();
    gmCameraMakeEffectCamera();
    ifCommonPlayerTagMakeInterface();
    ifCommonPlayerDamageSetDigitPositions();
    ifCommonPlayerDamageInitInterface();
    ifCommonPlayerStockInitInterface();

    /* scvsbattle.c:216: the entry-all thread, which wakes at tic 90 to
     * start the countdown thread and the entry-focus thread, which in
     * turn starts each fighter's Appear; the countdown's GO! at five
     * seconds unlocks the controllers (src/dc/ifcommon.c). */
    ifCommonEntryAllMakeInterface();

    /* scvsbattle.c:217-218: the stage's music, and the crowd's cheer at
     * the top of a match. The voice is played through the port's FGM
     * engine like every other, and is refused quietly if that id is not
     * in the bank the scene loaded. */
    mpCollisionSetPlayBGM();
    func_800269C0_275C0(nSYAudioVoicePublicExcited);

    /* scvsbattle.c:219-220: the match clock. The runner exists whatever
     * the rule -- it is what fills time_passed -- and the digits only in
     * a TIME match with a finite limit (src/dc/ifcommon.c). */
    ifCommonTimerMakeInterface(ifCommonAnnounceTimeUpInitInterface);
    ifCommonTimerMakeDigits();

    /* scvsbattle.c:222-224: the fade in from black, twelve tics, on the
     * transition link (src/dc/lbfade.c). */
    color = dSCVSBattleCommonFadeColor;

    lbFadeMakeActor(nGCCommonKindTransition, nGCCommonLinkIDTransition, 10, &color, 12, TRUE, NULL);

    if (gSCVSBattleFuncDebug != NULL)
    {
        gSCVSBattleFuncDebug();
    }

    /* From here to the scene's end nothing should be read: any file that
     * is gets a line naming it (src/dc/assetroot.h). */
    asset_io_expect_none("the match");
}

/* ---- sudden death -----------------------------------
 *
 * A TIME match ends on the clock, not on stocks, so it can end level.
 * The game's answer is a second battle: the tied players only, one
 * stock each, spawned at 300% damage with the entry sequence skipped
 * and SUDDEN DEATH on the screen. The first player knocked off wins.
 *
 * The two functions below are the whole of it. Neither has any N64
 * service in it -- the first is a sort and the second is
 * scVSBattleStartBattle with five lines changed -- so both are
 * verbatim, with the same cuts scVSBattleStartBattle makes and no
 * others. * ------------------------------------------------------------------ */

/* scvsbattle.c:227-410 scVSBattleSetScoreCheckSuddenDeath 0x8018D5E0,
 * verbatim: sort a TIME match's players by KOs minus falls, and if two
 * or more share the top score, fill gSCManagerVSBattleState with just
 * those players and return TRUE.
 *
 * The tie is on tko -- score minus falls -- and not on the raw score,
 * so a player with three KOs and two falls ties one with one KO and no
 * falls. The sort is an insertion sort written as a double loop and it
 * is stable in neither direction, which does not matter: nothing reads
 * the order below the top score, only how many share it.
 *
 * The state it leaves behind is a battle state the second battle runs
 * on: the same stage and the same fighters, but the rule swapped to
 * STOCK, is_show_score off (a sudden death has no score row) and every
 * untied player's pkind set to nFTPlayerKindNot so the spawn loop skips
 * them. gSCManagerSceneData.is_suddendeath is the flag the results
 * screen reads (src/dc/scvsresults.c mnVSResultsSetPlaceTime), which is
 * why it is set here and not in the scene entry. */
sb32 scVSBattleSetScoreCheckSuddenDeath(void)
{
    s32 result_count;
    s32 tied_players;
    s32 i, j;
    SCBattleResults winner_results;
    SCBattleResults player_results[GMCOMMON_PLAYERS_MAX];

    if (!(gSCManagerBattleState->game_rules & SCBATTLE_GAMERULE_TIME))
    {
        return FALSE;
    }
    gSCManagerVSBattleState = gSCManagerTransferBattleState;
    gSCManagerVSBattleState.pl_count = gSCManagerVSBattleState.cp_count = 0;

    for (i = 0; i < ARRAY_COUNT(gSCManagerVSBattleState.players); i++)
    {
        gSCManagerVSBattleState.players[i].pkind = nFTPlayerKindNot;
    }
    switch (gSCManagerBattleState->is_team_battle)
    {
    case FALSE:
        for (result_count = i = 0; i < ARRAY_COUNT(gSCManagerBattleState->players); i++)
        {
            if (gSCManagerBattleState->players[i].pkind == nFTPlayerKindNot)
            {
                continue;
            }
            player_results[result_count].tko = gSCManagerBattleState->players[i].score - gSCManagerBattleState->players[i].falls;
            player_results[result_count].kos = gSCManagerBattleState->players[i].score;
            player_results[result_count].player_or_team = i;
            player_results[result_count].unk_battleres_0x9 = FALSE;

            if (gSCManagerBattleState->players[i].pkind == nFTPlayerKindMan)
            {
                player_results[result_count].is_human = TRUE;
            }
            else player_results[result_count].is_human = FALSE;

            result_count++;
        }
        for (i = 0; i < result_count; i++)
        {
            for (j = i + 1; j < result_count; j++)
            {
                if (player_results[i].tko < player_results[j].tko)
                {
                    winner_results = player_results[i];
                    player_results[i] = player_results[j];
                    player_results[j] = winner_results;
                }
            }
        }
        player_results[0].unk_battleres_0x9 = TRUE;

        for (tied_players = 1, i = 1; i < result_count; i++)
        {
            if (player_results[0].tko == player_results[i].tko)
            {
                player_results[i].unk_battleres_0x9 = TRUE;
                tied_players++;
            }
        }
        if (tied_players < 2)
        {
            return FALSE;
        }
        for (i = 0; i < tied_players; i++)
        {
            gSCManagerVSBattleState.players[player_results[i].player_or_team].pkind = gSCManagerBattleState->players[player_results[i].player_or_team].pkind;

            switch (gSCManagerVSBattleState.players[player_results[i].player_or_team].pkind)
            {
            case nFTPlayerKindMan:
                gSCManagerVSBattleState.pl_count++;
                break;

            case nFTPlayerKindCom:
                gSCManagerVSBattleState.cp_count++;
                break;
            }
        }
        break;

    case TRUE:
        for (result_count = i = 0; i < ARRAY_COUNT(gSCManagerBattleState->players); i++)
        {
            if (gSCManagerBattleState->players[i].pkind == nFTPlayerKindNot)
            {
                continue;
            }
            for (j = 0; j < result_count; j++)
            {
                if (gSCManagerBattleState->players[i].team == player_results[j].player_or_team)
                {
                    player_results[j].tko += gSCManagerBattleState->players[i].score - gSCManagerBattleState->players[i].falls;
                    player_results[j].kos += gSCManagerBattleState->players[i].score;

                    if (player_results[j].is_human || (gSCManagerBattleState->players[i].pkind == nFTPlayerKindMan))
                    {
                        player_results[j].is_human = TRUE;
                    }
                    else player_results[j].is_human = FALSE;

                    goto l_continue;
                }
            }
            player_results[result_count].tko = gSCManagerBattleState->players[i].score - gSCManagerBattleState->players[i].falls;
            player_results[result_count].kos = gSCManagerBattleState->players[i].score;
            player_results[result_count].player_or_team = gSCManagerBattleState->players[i].team;
            player_results[result_count].unk_battleres_0x9 = FALSE;

            if (player_results[result_count].is_human || (gSCManagerBattleState->players[i].pkind == nFTPlayerKindMan))
            {
                player_results[result_count].is_human = TRUE;
            }
            else player_results[result_count].is_human = FALSE;

            result_count++;

        l_continue:
            continue;
        }
        for (i = 0; i < result_count; i++)
        {
            for (j = i + 1; j < result_count; j++)
            {
                if (player_results[i].tko < player_results[j].tko)
                {
                    winner_results = player_results[i];
                    player_results[i] = player_results[j];
                    player_results[j] = winner_results;
                }
            }
        }
        player_results[0].unk_battleres_0x9 = TRUE;

        for (tied_players = 1, i = 1; i < result_count; i++)
        {
            if (player_results[0].tko == player_results[i].tko)
            {
                player_results[i].unk_battleres_0x9 = TRUE;
                tied_players++;
            }
        }
        if (tied_players < 2)
        {
            return FALSE;
        }
        for (i = 0; i < tied_players; i++)
        {
            for (j = 0; j < ARRAY_COUNT(gSCManagerBattleState->players); j++)
            {
                if (gSCManagerBattleState->players[j].pkind == nFTPlayerKindNot)
                {
                    continue;
                }
                if (gSCManagerBattleState->players[j].team == player_results[i].player_or_team)
                {
                    gSCManagerVSBattleState.players[j].pkind = gSCManagerBattleState->players[j].pkind;

                    switch (gSCManagerVSBattleState.players[j].pkind)
                    {
                    case nFTPlayerKindMan:
                        gSCManagerVSBattleState.pl_count++;
                        break;

                    case nFTPlayerKindCom:
                        gSCManagerVSBattleState.cp_count++;
                        break;
                    }
                }
            }
        }
        break;
    }
    gSCManagerVSBattleState.game_rules = SCBATTLE_GAMERULE_STOCK;
    gSCManagerVSBattleState.is_show_score = FALSE;

    gSCManagerSceneData.is_suddendeath = TRUE;

    return TRUE;
}

/* scvsbattle.c:412-501 scVSBattleStartSuddenDeath 0x8018DE20, the
 * second battle's SYTaskmanSetup.func_start. Line for line
 * scVSBattleStartBattle -- and it carries the same cuts, listed at that
 * function's head -- with five differences, all of them the game's:
 *
 *   - is_suddendeath is not cleared here, because this is the battle it
 *     is set for;
 *   - the llSYKseg1Validate block is absent from the game's too;
 *   - stock_count is 0 and damage is 300, so one hit that lands ends
 *     it;
 *   - is_skip_entry puts each fighter straight into Wait or Fall
 *     (ft/ftmanager.c:889, in the link) instead of the warp-in;
 *   - ifCommonSuddenDeathMakeInterface stands where
 *     ifCommonEntryAllMakeInterface stands, and it is a banner as well
 *     as a thread (src/dc/ifcommon.c).
 *
 * The clock is made here too, exactly as in a first battle. The rule is
 * STOCK by now so ifCommonTimerMakeDigits returns NULL and nothing is
 * drawn, but the runner still fills time_passed -- which is what the
 * results screen shows as the match length. */
void scVSBattleStartSuddenDeath(void)
{
    s32 player;
    FTDesc desc;
    SYColorRGBA color;

    gSCManagerSceneData.is_reset = FALSE;

    scVSBattleSetupFiles();

    /* scvsbattle.c:425, 436: the pools and the bank again. The scene
     * heap was emptied between the two battles, so everything the first
     * one allocated -- the banks included -- is gone, and
     * efParticleInitAll clearing the bank cache is what stops the second
     * battle finding the first's addresses still in it. */
    efParticleInitAll();

    ftParamInitGame();
    stage_bind_collision();

    gmCameraSetViewportDimensions(10, 10, 310, 230);

    /* scvsbattle.c:160 (:432) itManagerInitItems: the item struct pool,
     * the item pack and the item particle bank. The same shape as
     * wpManagerAllocWeapons, a few lines below. Without it
     * gITManagerCommonData is NULL, and that is the first thing
     * itManagerMakeItem reaches for: every item this port can spawn would
     * fail on the line that reads its attributes, exactly as every weapon
     * failed on wpManagerGetNextStructAlloc before that step. */
    itManagerInitItems();

    grCommonSetupInitAll();

    ftManagerAllocFighter(FTDATA_FLAG_MAINMOTION, GMCOMMON_PLAYERS_MAX);
    /* scvsbattle.c:435, the rematch's own copy of the line above */
    wpManagerAllocWeapons();
    efManagerLoadEffectBank();
    /* scvsbattle.c:165 (:437): the screen flash. */
    ifScreenFlashMakeInterface(0xFF);
    /* scvsbattle.c:166 (:438) gmRumbleMakeActor: see the rematch's own
     * copy above. */
    gmRumbleMakeActor();
    ftPublicMakeActor();

    for (player = 0; player < ARRAY_COUNT(gSCManagerBattleState->players); player++)
    {
        desc = dFTManagerDefaultFighterDesc;

        if (gSCManagerBattleState->players[player].pkind == nFTPlayerKindNot)
        {
            continue;
        }
        ftManagerSetupFilesAllKind(gSCManagerBattleState->players[player].fkind);

        desc.fkind = gSCManagerBattleState->players[player].fkind;

        mpCollisionGetPlayerMapObjPosition(player, &desc.pos);

        desc.lr = scVSBattleGetStartPlayerLR(player);

        desc.team = gSCManagerBattleState->players[player].player;
        desc.player = player;

        desc.detail = ((gSCManagerBattleState->pl_count + gSCManagerBattleState->cp_count) < 3) ? nFTPartsDetailHigh : nFTPartsDetailLow;

        desc.costume = gSCManagerBattleState->players[player].costume;
        desc.shade = gSCManagerBattleState->players[player].shade;
        desc.handicap = gSCManagerBattleState->players[player].handicap;
        desc.level = gSCManagerBattleState->players[player].level;
        desc.stock_count = 0;
        desc.damage = 300;
        desc.is_skip_entry = TRUE;
        desc.pkind = gSCManagerBattleState->players[player].pkind;
        desc.controller = &gSYControllerDevices[player];

        desc.figatree_heap = ftManagerAllocFigatreeHeapKind(gSCManagerBattleState->players[player].fkind);

        ftParamInitPlayerBattleStats(player, ftManagerMakeFighter(&desc));

        gSCManagerBattleState->players[player].is_single_stockicon = FALSE;
    }
    ifCommonBattleSetGameStatusWait();

    /* The camera after the fighters, and the DL links on it, for the
     * reason scVSBattleStartBattle gives. */
    gmCameraMakeBattleCamera();
    gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK;
    grWallpaperMakeDecideKind();

    /* scvsbattle.c:205-208: the off-screen arrows and the camera that
     * draws them. They come before the HUD's camera, as
     * there, so the arrows are drawn under it. */
    gmCameraMakePlayerMagnifyCamera();
    ifCommonPlayerMagnifyMakeInterface();
    gmCameraMakePlayerArrowsCamera();
    ifCommonPlayerArrowsInitInterface();

    gmCameraScreenFlashMakeCamera();
    gmCameraMakeInterfaceCamera();
    gmCameraMakeEffectCamera();
    ifCommonPlayerTagMakeInterface();
    ifCommonPlayerDamageSetDigitPositions();
    ifCommonPlayerDamageInitInterface();
    ifCommonPlayerStockInitInterface();
    ifCommonSuddenDeathMakeInterface();
    mpCollisionSetPlayBGM();
    func_800269C0_275C0(nSYAudioVoicePublicExcited);
    ifCommonTimerMakeInterface(ifCommonAnnounceTimeUpInitInterface);
    ifCommonTimerMakeDigits();

    color = dSCVSBattleSuddenDeathFadeColor;

    lbFadeMakeActor(nGCCommonKindTransition, nGCCommonLinkIDTransition, 10, &color, 12, TRUE, NULL);

    if (gSCVSBattleFuncDebug != NULL)
    {
        gSCVSBattleFuncDebug();
    }
}

/* The stage this scene holds a reference on (src/dc/stage.h). Two statics rather than a kind and a sentinel because
 * nGRKindCastle is zero and the overlay load's job is to write zeroes:
 * "no stage" has to be the cleared state. The game needs neither -- its
 * ground is a heap that goes with the scene. */
static u8 sSCVSBattleStageHeld;
static s32 sSCVSBattleStageKind;

/* scvsbattle.c:4553-4600 scVSBattleStartScene 0x8018E190, the scene
 * manager's entry: point the battle state at the transfer state the
 * menus filled in, run the battle, and name the scene that follows.
 *
 * DIVERGES. syVideoInit and the zbuffer allocation are the N64's video
 * mode, set per scene; the Dreamcast's is set once at boot
 * (src/game/ssb64/main.c's vid_set_mode) and every scene draws in it. The
 * arena_size line is the link map again.
 *
 * The BGM stop-and-wait, the volume set and func_800266A0_272A0 are
 * here in the sudden-death arm only, where they have
 * to be: the second battle plays the stage's music again from the top,
 * and the clock has just faded the first battle's volume down under the
 * last five seconds. After the second battle, and after a first battle
 * that needs no second, they stay cut for the reason they always were
 * -- the results screen waits for the win jingle rather than for
 * silence (src/dc/scvsresults.c mnVSResultsAudioThreadUpdate). */
void scVSBattleStartScene(void)
{
    gSCManagerBattleState = &gSCManagerTransferBattleState;
    gSCManagerBattleState->game_type = nSCBattleGameTypeRoyal;
    gSCManagerBattleState->gkind = gSCManagerSceneData.gkind;

    /* The game's next line is the ground: gr/grmain.c loads the map file
     * for this kind and mpCollisionInitGroundData points the collision
     * at it. This does the same, off the same medium -- grStageAcquire
     * (src/dc/stage.h) -- and gives it back at the end of the scene,
     * which is where the game's heap goes.  DIVERGES, defensively: every VS stage has a pack now,
     * so a NULL here is a pack that would not load; it plays on Hyrule,
     * said on the log, rather than starting a match with no ground. */
    {
        s32 gkind = gSCManagerBattleState->gkind;
        Stage *stage = grStageAcquire(gkind);

        if (stage == NULL)
        {
            syDebugPrintf("scVSBattleStartScene: no pack for stage %d, "
                          "playing Hyrule\n", (int)gkind);
            gSCManagerBattleState->gkind = nGRKindHyrule;
            stage = grStageAcquire(nGRKindHyrule);
        }
        sSCVSBattleStageHeld = (stage != NULL);
        sSCVSBattleStageKind = gSCManagerBattleState->gkind;
        if (stage != NULL)
        {
            stage_bind(stage);
        }
    }

    dSCVSBattleTaskmanSetup.func_start = scVSBattleStartBattle;

    scManagerFuncUpdate(&dSCVSBattleTaskmanSetup);

    /* scvsbattle.c:538: the pads stopped the tic the battle ends, before
     * sudden death decides whether there is a second one. The game
     * reaches it through the BGM stop-and-wait above it, which the port
     * does not run on this arm (see the DIVERGES block); the rumble is
     * not that, and stopping it here is what keeps a match that ended on
     * a hit from buzzing through the results screen. */
    gmRumbleInitPlayers();

    /* scvsbattle.c:540-553, verbatim: the whole of sudden death from the
     * scene's point of view. Ask whether the match was a tied TIME
     * match, and if it was, run the battle
     * scene a second time on the state the check just built. Two
     * battles in one scene, and the results screen only sees the
     * second. is_reset is the pause menu's "reset match", which cannot
     * be set yet -- nothing scans START. */
    if (!(gSCManagerSceneData.is_reset) && scVSBattleSetScoreCheckSuddenDeath() != FALSE)
    {
        gSCManagerBattleState = &gSCManagerVSBattleState;

        gSCManagerBattleState->game_type = nSCBattleGameTypeRoyal;

        dSCVSBattleTaskmanSetup.func_start = scVSBattleStartSuddenDeath;

        scManagerFuncUpdate(&dSCVSBattleTaskmanSetup);

        syAudioStopBGMAll();

        while (syAudioCheckBGMPlaying(0) != FALSE)
        {
            continue;
        }
        syAudioSetBGMVolume(0, 0x7800);
        func_800266A0_272A0();
        gmRumbleInitPlayers();
    }

    /* The stage, given back with the scene: both battles of a sudden
     * death play on it, so this is after them and not between them. */
    if (sSCVSBattleStageHeld)
    {
        grStageRelease(sSCVSBattleStageKind);
        sSCVSBattleStageHeld = 0;
    }
    /* And the particle banks' textures with it, for the same reason and
     * on the same beat: efParticleGetLoadBankID's cache spans a sudden
     * death and the scene heap under it does not span a scene. */
    lbpTexFreeAll();

    gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
    gSCManagerSceneData.scene_curr = nSCKindVSResults;
}

/* The bzero arm of syDmaLoadOverlay for overlay 4, of which this
 * file is a part: sc/scmanager.c calls it on the way into every
 * scene that loads that overlay. src/dc/overlay.h says why the port
 * needs it written out.
 *
 * gSCVSBattleFuncDebug is the one thing in this file the reload
 * leaves alone. It is the game's, and the game's overlay reload does
 * clear it -- but the port's debug layer installs it once before the
 * scene manager's loop (src/dc/db.c db_install) because it has
 * no ROM segment to be restored from, and clearing it would take the
 * collision overlay and the serial log out of the second battle.
 * DIVERGES, and it is the debug layer's divergence, not the game's. */
void scVSBattleOverlayLoad(void)
{
    OVERLAY_CLEAR(dSCVSBattleCommonFadeColor);
    OVERLAY_CLEAR(dSCVSBattleSuddenDeathFadeColor);
    OVERLAY_CLEAR(sSCVSBattleStageHeld);
    OVERLAY_CLEAR(sSCVSBattleStageKind);
}
