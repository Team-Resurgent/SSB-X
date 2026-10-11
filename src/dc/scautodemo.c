/* scautodemo.c -- see scautodemo.h. Function-for-function from
 * ssb-decomp-re/src/sc/sccommon/scautodemo.c; every function names its
 * line range. Structurally this is src/dc/scvsbattle.c's own recipe --
 * the same ftManagerAllocFighter/grCommonSetupInitAll/ftManagerMakeFighter
 * spine, the same cuts (listed at scVSBattleStartBattle's header) -- with
 * one real difference: scVSBattleStartScene knows gkind before the scene
 * heap exists (the stage select set it), so it acquires the stage before
 * calling scManagerFuncUpdate; this scene only learns gkind inside its
 * own func_start (scAutoDemoInitDemo cycles it), so the stage acquire
 * moves inside scAutoDemoFuncStart instead -- stage.h says this is safe
 * ("nothing here comes out of the scene heap") -- and the release moves
 * to a pair of statics scAutoDemoStartScene reads after the task ends,
 * the same role sSCVSBattleStageHeld/Kind play in scvsbattle.c.
 */
#include "scautodemo.h"
#include "overlay.h"

#include "ftcommon.h"
#include "gmcamera.h"
#include "gmcommon.h"
#include "lbcommon.h" /* lbCommonDrawSObjAttr, lbCommonMakeSObjForGObj */
#include "lbpartex.h" /* lbpTexFreeAll */
#include "ifcommon.h"
#include "input.h"
#include "scmanager.h"
#include "sndres.h"
#include "sprite.h"
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
#include <if/interface.h>
#include <it/itmanager.h> /* itManagerInitItems */
#include <mp/map.h>
#include <sys/controller.h>
#include <wp/wpmanager.h> /* wpManagerAllocWeapons */
#include <lb/lbdef.h>
#include <lb/lbfade.h>

/* n_env.c:5128, the FGM voice-cue wrapper every scene entry calls --
 * declared locally in every file that calls it (mntitle.c, ifcommon.c,
 * mnsoundtest.c), defined in src/dc/fgm.c. */
void func_800266A0_272A0(void);

/* ---- the pools --------------------------------------------------------
 *
 * scautodemo.c:129-171 dSCAutoDemoTaskmanSetup carries zero for every
 * pool count, the same placeholder scvsbattle.c:24-64 documents (the
 * real numbers are in the ROM's data, not the decomp's C source). This
 * scene runs the same shape of match scvsbattle.c does -- four real
 * fighters, one stage, no menu of its own -- so it reuses that file's
 * own figures rather than guessing fresh ones: same fighters, same
 * DObj/XObj/AObj demand per fighter, and if anything a *smaller* SObj
 * bill (no countdown, no GO!, no GAME SET, no match clock -- auto-demo
 * skips the entry sequence and never calls ifCommonTimerMakeDigits). */
#define SCAUTODEMO_GOBJS       48
#define SCAUTODEMO_GOBJPROCS   48
#define SCAUTODEMO_XOBJS      288
#define SCAUTODEMO_AOBJS      576
#define SCAUTODEMO_MOBJS       32
#define SCAUTODEMO_DOBJS       96
#define SCAUTODEMO_SOBJS       96
#define SCAUTODEMO_COBJS        6

/* scautodemo.c:21-31 dSCAutoDemoGroundOrder, verbatim: the eight stages
 * this scene cycles through, one per demo. All eight are ported
 * (src/dc/grpupupu.c, grzebes.c, grcastle.c, grjungle.c, grsector.c,
 * gryoster.c, gryamabuki.c, grhyrule.c; src/dc/stage.c's grStageFileName
 * has the pack for each). */
u8 dSCAutoDemoGroundOrder[] =
{
    nGRKindPupupu,
    nGRKindZebes,
    nGRKindCastle,
    nGRKindJungle,
    nGRKindSector,
    nGRKindYoster,
    nGRKindYamabuki,
    nGRKindHyrule
};

/* scautodemo.c:34-44 dSCAutoDemoMapObjKindList, verbatim: the eight
 * per-player spawn-point map-object kinds scAutoDemoGetPlayerStartPosition
 * picks from (mp/mpcollision.c, compiled unmodified). */
s16 dSCAutoDemoMapObjKindList[] =
{
    nMPMapObjKindAutoDemoPlayer1,
    nMPMapObjKindAutoDemoPlayer2,
    nMPMapObjKindAutoDemoPlayer3,
    nMPMapObjKindAutoDemoPlayer4,
    nMPMapObjKindAutoDemoPlayer5,
    nMPMapObjKindAutoDemoPlayer6,
    nMPMapObjKindAutoDemoPlayer7,
    nMPMapObjKindAutoDemoPlayer8
};

/* scautodemo.c:47-97 dSCAutoDemoFuncList, verbatim: the focus-change
 * timeline scAutoDemoUpdateFocus walks, one entry at a time. */
SCAutoDemoProc dSCAutoDemoFuncList[] =
{
    /* Nothing? */
    { 0, NULL, NULL },

    /* Pre-focus */
    { 340, scAutoDemoMakeFade, NULL },

    /* Change to Player 1 focus */
    { 340, scAutoDemoSetFocusPlayer1, NULL },

    /* Player 1 focus */
    { 340, scAutoDemoSetFocusPlayer2, SCAutoDemoProcFocusPlayer1 },

    /* Player 2 focus */
    { 400, scAutoDemoResetFocusPlayerAll, SCAutoDemoProcFocusPlayer2 },

    /* End focus */
    { 60, scAutoDemoSetMagnifyDisplayOn, NULL },

    /* End demo */
    { 1, scAutoDemoExit, NULL }
};

/* scautodemo.c:100-103, verbatim: the camera zoom's random eye offsets,
 * in tenths of a degree (F_CLC_DTOR32 below converts). */
f32 dSCAutoDemoZoomEyeX[] = { -40.0F, -28.0F, -14.0F, 14.0F, 28.0F, 40.0F };
f32 dSCAutoDemoZoomEyeY[] = { 2.0F, 0.0F, -6.0F, -9.0F, -30.0F };

/* scautodemo.c:123 dSCAutoDemoFadeColor, verbatim: opaque black, the
 * same "from the colour to the scene" alpha-0 convention
 * dSCVSBattleCommonFadeColor uses (src/dc/lbfade.c). */
SYColorRGBA dSCAutoDemoFadeColor = { 0x00, 0x00, 0x00, 0x00 };

/* scautodemo.c:129-171 dSCAutoDemoTaskmanSetup.
 *
 * DIVERGES, the same shape scvsbattle.c:87-135 documents: the DL
 * buffers, graphics arena and RDP output buffer are zeroed (no RSP/RDP
 * here), func_lights is NULL (scAutoDemoFuncLights's whole body is
 * ftDisplayLightsDrawReflect, the same dropped reflection lighting
 * every other scene's func_lights drops), arena_start is NULL (keep the
 * region syTaskmanMakeGeneralHeap already made), and the matrix
 * function list is NULL (the matrix kinds are a switch in objdisplay.c),
 * the same as every other scene's setup in this port. */
SYTaskmanSetup dSCAutoDemoTaskmanSetup =
{
    {
        0,                              /* flags */
        scAutoDemoFuncUpdate,           /* update function */
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
    SCAUTODEMO_GOBJPROCS,
    SCAUTODEMO_GOBJS,   sizeof(GObj),
    SCAUTODEMO_XOBJS,
    NULL,                               /* matrix function list */
    NULL,                               /* DObjVec eject function */
    SCAUTODEMO_AOBJS,
    SCAUTODEMO_MOBJS,
    SCAUTODEMO_DOBJS,   sizeof(DObj),
    SCAUTODEMO_SOBJS,   sizeof(SObj),
    SCAUTODEMO_COBJS,   sizeof(CObj),

    scAutoDemoFuncStart                 /* task start function */
};

/* ---- state -------------------------------------------------------- */

static SCBattleState sSCAutoDemoBattleState;
static s32 sSCAutoDemoFocusChangeWait;
static u16 sSCAutoDemoFighterMask;
static GObj *sSCAutoDemoFighterNameGObj;

/* The port's own sprite bank for relocData 12, tools/export/ssb_spriteexport.py
 * --file 12 -- the twelve fighter name plates (see
 * scautodemo.h). Same one-bank shape src/dc/scexplain.c's own
 * sSCExplainGraphicsBank is. */
static SpriteBank sSCAutoDemoNamesBank;
static SpriteBank *sSCAutoDemoNamesFileHead;

/* scautodemo.c:106-120 dSCAutoDemoFighterNameSpriteOffsets. DIVERGES
 * only in how the offset is written, the same substitution
 * src/dc/scstaffroll.c's own dSCStaffrollTextBoxSpriteInfo makes: the
 * decomp's &llCharacterNames<Fighter>Sprite link labels are relocData
 * 12's own linker symbols, which this port does not link, so each entry
 * is the literal number tools/export/ssb_spriteexport.py --file 12 --list
 * reports for that same sprite -- the key sprite_bank_get looks it up
 * by. Order is the decomp's, which is nFTKind order. */
const u32 dSCAutoDemoFighterNameSpriteOffsets[12] =
{
    0x00138,    /* Mario */
    0x00258,    /* Fox */
    0x00378,    /* Donkey Kong */
    0x004f8,    /* Samus */
    0x00618,    /* Luigi */
    0x00738,    /* Link */
    0x00858,    /* Yoshi */
    0x00a38,    /* Captain Falcon */
    0x00bb8,    /* Kirby */
    0x00d38,    /* Pikachu */
    0x00f78,    /* Jigglypuff */
    0x01098     /* Ness */
};
static SCAutoDemoProc *sSCAutoDemoFunc;
static s16 sSCAutoDemoMapObjs[8];

/* The stage this scene holds a reference on, src/dc/scvsbattle.c's own
 * sSCVSBattleStageHeld/Kind pair -- except gkind is not known until
 * scAutoDemoInitDemo runs (inside the task, not before it), so the
 * acquire moves to scAutoDemoFuncStart and these are what
 * scAutoDemoStartScene reads back after scManagerFuncUpdate returns. */
static u8 sSCAutoDemoStageHeld;
static s32 sSCAutoDemoStageKind;

/* scautodemo.c:203-207 scAutoDemoFuncUpdate 0x8018D0C0, verbatim. */
void scAutoDemoFuncUpdate(void)
{
    gcRunAll();
}

/* sc/sccommon/scautodemofiles.c:23-38 scAutoDemoSetupFiles 0x8018E0C0.
 * DIVERGES the same way scVSBattleSetupFiles is: the reloc loader's
 * status-buffer setup and lbRelocLoadFilesListed(dGMCommonFileIDs,
 * gGMCommonFiles) are gone; what remains is the load, and it is the
 * port's own (src/dc/gmcommon.c), already shared with every other
 * battle-shaped scene. */
void scAutoDemoSetupFiles(void)
{
    gmCommonLoadFiles();
}

/* scautodemo.c:209-221 scAutoDemoStartBattle 0x8018D0E0, verbatim.
 * Unlike scVSBattleStartBattle's ifCommonBattleSetGameStatusWait (the
 * countdown's own status), the demo has no entry sequence to wait
 * through -- every fighter's desc.is_skip_entry is TRUE below -- so
 * this sets Go directly, the same as sudden death does after its own
 * skip-entry spawn. */
void scAutoDemoStartBattle(void)
{
    GObj *fighter_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

    while (fighter_gobj != NULL)
    {
        ftParamUnlockPlayerControl(fighter_gobj);

        fighter_gobj = fighter_gobj->link_next;
    }
    gSCManagerBattleState->game_status = nSCBattleGameStatusGo;
}

/* scautodemo.c:223-241 scAutoDemoDetectExit 0x8018D134, verbatim: any
 * pad's A/B/START ends the demo early, back to the title. */
void scAutoDemoDetectExit(void)
{
    s32 player;

    for (player = 0; player < (s32)ARRAY_COUNT(gSYControllerDevices); player++)
    {
        u16 button_tap = gSYControllerDevices[player].button_tap;

        if (button_tap & (A_BUTTON | B_BUTTON | START_BUTTON))
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            syTaskmanSetLoadScene();
            break;
        }
    }
}

/* scautodemo.c:243-249 scAutoDemoMakeFade 0x8018D19C, verbatim. */
void scAutoDemoMakeFade(void)
{
    SYColorRGBA color = dSCAutoDemoFadeColor;

    lbFadeMakeActor(nGCCommonKindTransition, nGCCommonLinkIDTransition, 10, &color, 30, TRUE, NULL);
}

/* scautodemo.c:251-264 scAutoDemoCheckStopFocusPlayer 0x8018D1EC,
 * verbatim: a focused fighter who has already died stops the focus
 * timer at once rather than lingering on an empty spawn point. */
sb32 scAutoDemoCheckStopFocusPlayer(FTStruct *fp)
{
    switch (fp->status_id)
    {
    case nFTCommonStatusDeadDown:
    case nFTCommonStatusDeadLeftRight:
    case nFTCommonStatusDeadUpStar:
        return TRUE;

    default:
        return FALSE;
    }
}

/* scautodemo.c:266-278 scAutoDemoSetCameraPlayerZoom 0x8018D220,
 * verbatim. */
void scAutoDemoSetCameraPlayerZoom(GObj *fighter_gobj)
{
    gmCameraSetStatusPlayerZoom
    (
        fighter_gobj,
        F_CLC_DTOR32(dSCAutoDemoZoomEyeX[syUtilsRandIntRange(ARRAY_COUNT(dSCAutoDemoZoomEyeX))]),
        F_CLC_DTOR32(dSCAutoDemoZoomEyeY[syUtilsRandIntRange(ARRAY_COUNT(dSCAutoDemoZoomEyeY))]),
        ftGetStruct(fighter_gobj)->attr->closeup_camera_zoom,
        0.3F,
        28.0F
    );
}

/* scautodemo.c:280-305 scAutoDemoSetFocusPlayer1 0x8018D2CC, verbatim
 * but for the name-plate sprite: scautodemo.h's header note is why
 * sSCAutoDemoFighterNameGObj can be NULL, and the one line that would
 * touch it is guarded rather than dropped -- everything else (the
 * level handicapping, the camera zoom, the detail bump, the magnify
 * flag) runs exactly as the decomp has it. */
void scAutoDemoSetFocusPlayer1(void)
{
    GObj *fighter_gobj = gSCManagerBattleState->players[0].fighter_gobj;
    FTStruct *fp = ftGetStruct(fighter_gobj);

    if (scAutoDemoCheckStopFocusPlayer(fp) != FALSE)
    {
        sSCAutoDemoFocusChangeWait = 0;
    }
    else
    {
        ftGetStruct(gSCManagerBattleState->players[1].fighter_gobj)->level =
        ftGetStruct(gSCManagerBattleState->players[2].fighter_gobj)->level =
        ftGetStruct(gSCManagerBattleState->players[3].fighter_gobj)->level = 1;

        scAutoDemoSetCameraPlayerZoom(fighter_gobj);
        ftParamSetModelPartDetailAll(fighter_gobj, nFTPartsDetailHigh);

        fp->detail_base = nFTPartsDetailHigh;

        if (sSCAutoDemoFighterNameGObj != NULL)
        {
            SObjGetStruct(sSCAutoDemoFighterNameGObj)->sprite.attr &= ~SP_HIDDEN;
        }

        gIFCommonPlayerInterface.is_magnify_display = FALSE;
    }
}

/* scautodemo.c:307-314 SCAutoDemoProcFocusPlayer1 0x8018D39C, verbatim. */
void SCAutoDemoProcFocusPlayer1(void)
{
    if (scAutoDemoCheckStopFocusPlayer(ftGetStruct(gSCManagerBattleState->players[0].fighter_gobj)) != FALSE)
    {
        sSCAutoDemoFocusChangeWait = 0;
    }
}

/* scautodemo.c:316-347 scAutoDemoSetFocusPlayer2 0x8018D3D4, verbatim
 * but for the name-plate sprite guard -- see scAutoDemoSetFocusPlayer1
 * above. */
void scAutoDemoSetFocusPlayer2(void)
{
    GObj *p2_gobj = gSCManagerBattleState->players[1].fighter_gobj;
    GObj *p1_gobj = gSCManagerBattleState->players[0].fighter_gobj;
    FTStruct *p2_fp = ftGetStruct(p2_gobj);

    if (sSCAutoDemoFighterNameGObj != NULL)
    {
        SObjGetStruct(sSCAutoDemoFighterNameGObj)->sprite.attr |= SP_HIDDEN;
    }

    ftParamSetModelPartDetailAll(p1_gobj, nFTPartsDetailLow);
    ftGetStruct(p1_gobj)->detail_base = nFTPartsDetailLow;

    if (scAutoDemoCheckStopFocusPlayer(p2_fp) != FALSE)
    {
        sSCAutoDemoFocusChangeWait = 0;
    }
    else
    {
        ftGetStruct(gSCManagerBattleState->players[1].fighter_gobj)->level = 9;

        ftGetStruct(gSCManagerBattleState->players[0].fighter_gobj)->level =
        ftGetStruct(gSCManagerBattleState->players[2].fighter_gobj)->level =
        ftGetStruct(gSCManagerBattleState->players[3].fighter_gobj)->level = 1;

        scAutoDemoSetCameraPlayerZoom(p2_gobj);
        ftParamSetModelPartDetailAll(p2_gobj, nFTPartsDetailHigh);

        p2_fp->detail_base = nFTPartsDetailHigh;

        if (sSCAutoDemoFighterNameGObj != NULL)
        {
            SObjGetStruct(sSCAutoDemoFighterNameGObj)->next->sprite.attr &= ~SP_HIDDEN;
        }
    }
}

/* scautodemo.c:349-356 SCAutoDemoProcFocusPlayer2 0x8018D4F0, verbatim. */
void SCAutoDemoProcFocusPlayer2(void)
{
    if (scAutoDemoCheckStopFocusPlayer(ftGetStruct(gSCManagerBattleState->players[1].fighter_gobj)) != FALSE)
    {
        sSCAutoDemoFocusChangeWait = 0;
    }
}

/* scautodemo.c:358-375 scAutoDemoResetFocusPlayerAll 0x8018D528,
 * verbatim but for the name-plate sprite guard. */
void scAutoDemoResetFocusPlayerAll(void)
{
    GObj *p2_gobj = gSCManagerBattleState->players[1].fighter_gobj;

    gmCameraSetStatusDefault();

    ftGetStruct(gSCManagerBattleState->players[0].fighter_gobj)->level =
    ftGetStruct(p2_gobj)->level =
    ftGetStruct(gSCManagerBattleState->players[2].fighter_gobj)->level =
    ftGetStruct(gSCManagerBattleState->players[3].fighter_gobj)->level = 9;

    ftParamSetModelPartDetailAll(p2_gobj, nFTPartsDetailLow);

    ftGetStruct(p2_gobj)->detail_base = nFTPartsDetailLow;

    if (sSCAutoDemoFighterNameGObj != NULL)
    {
        SObjGetStruct(sSCAutoDemoFighterNameGObj)->next->sprite.attr |= SP_HIDDEN;
    }
}

/* scautodemo.c:377-381 scAutoDemoSetMagnifyDisplayOn 0x8018D5E0,
 * verbatim. */
void scAutoDemoSetMagnifyDisplayOn(void)
{
    gIFCommonPlayerInterface.is_magnify_display = TRUE;
}

/* scautodemo.c:383-394 scAutoDemoExit 0x8018D5F0, REGION_US arm only
 * (this build has no JP arm): hand the scene back to Startup and ask
 * the manager to load it. This is the whole of the demo's own exit --
 * scAutoDemoStartScene below does not set scene_curr itself, unlike
 * scVSBattleStartScene, because by the time scManagerFuncUpdate returns
 * this has already run. */
void scAutoDemoExit(void)
{
    gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
    gSCManagerSceneData.scene_curr = nSCKindStartup;

    syTaskmanSetLoadScene();
}

/* scautodemo.c:396-406 scAutoDemoChangeFocus 0x8018D624, verbatim. */
void scAutoDemoChangeFocus(void)
{
    sSCAutoDemoFocusChangeWait = sSCAutoDemoFunc->focus_end_wait;

    if (sSCAutoDemoFunc->func_change != NULL)
    {
        sSCAutoDemoFunc->func_change();
    }
    sSCAutoDemoFunc++;
}

/* scautodemo.c:408-420 scAutoDemoUpdateFocus 0x8018D674, verbatim: run
 * the current entry's per-tic proc, then advance however many entries
 * this tic's wait count reaching zero calls for (an entry whose own
 * wait is 0 -- "Nothing?" -- falls through at once, which is why this
 * is a while and not an if). */
void scAutoDemoUpdateFocus(void)
{
    if (sSCAutoDemoFunc->func_focus != NULL)
    {
        sSCAutoDemoFunc->func_focus();
    }
    while (sSCAutoDemoFocusChangeWait == 0)
    {
        scAutoDemoChangeFocus();
    }
    sSCAutoDemoFocusChangeWait--;
}

/* scautodemo.c:422-427 scAutoDemoFuncRun 0x8018D6DC, verbatim. */
void scAutoDemoFuncRun(GObj *gobj)
{
    (void)gobj;

    scAutoDemoDetectExit();
    scAutoDemoUpdateFocus();
}

/* scautodemo.c:429-440 scAutoDemoMakeFocusInterface 0x8018D704,
 * verbatim. */
GObj *scAutoDemoMakeFocusInterface(void)
{
    GObj *interface_gobj = gcMakeGObjSPAfter(nGCCommonKindInterface, scAutoDemoFuncRun, nGCCommonLinkIDInterfaceActor, GOBJ_PRIORITY_DEFAULT);

    sSCAutoDemoFunc = dSCAutoDemoFuncList;
    sSCAutoDemoFocusChangeWait = 0;

    scAutoDemoUpdateFocus();

    return interface_gobj;
}

/* scautodemo.c:442-472 scAutoDemoGetPlayerStartPosition 0x8018D758,
 * verbatim: mpCollisionGetMapObjCountKind/IDsKind/PositionID are
 * mp/mpcollision.c, compiled unmodified. */
void scAutoDemoGetPlayerStartPosition(s32 mapobj_kind, Vec3f *mapobj_pos)
{
    s32 i, j;
    s32 mapobj_random;
    s32 mapobj_select;
    s32 mapobj;

    mapobj_random = syUtilsRandIntRange(((ARRAY_COUNT(dSCAutoDemoMapObjKindList) + ARRAY_COUNT(sSCAutoDemoMapObjs)) / 2) - mapobj_kind);

    for (i = j = 0; i < (s32)(ARRAY_COUNT(dSCAutoDemoMapObjKindList) + ARRAY_COUNT(sSCAutoDemoMapObjs)) / 2; i++)
    {
        mapobj_select = dSCAutoDemoMapObjKindList[i];

        if (sSCAutoDemoMapObjs[i] != -1)
        {
            if (mapobj_random == j)
            {
                sSCAutoDemoMapObjs[i] = -1;

                break;
            }
            else j++;
        }
    }
    if (mpCollisionGetMapObjCountKind(mapobj_select) != 0)
    {
        mpCollisionGetMapObjIDsKind(mapobj_select, &mapobj);
        mpCollisionGetMapObjPositionID(mapobj, mapobj_pos);
    }
}

/* scautodemo.c:474-487 scAutoDemoGetFighterKindsNum 0x8018D7FC,
 * verbatim: popcount of a 16-bit fighter mask. */
s32 scAutoDemoGetFighterKindsNum(u16 flag)
{
    s32 i, j;

    for (i = 0, j = 0; i < (s32)(sizeof(u16) * 8); i++, flag = flag >> 1)
    {
        if (flag & 1)
        {
            j++;
        }
    }
    return j;
}

/* scautodemo.c:489-508 scAutoDemoGetShuffledFighterKind 0x8018D874,
 * verbatim: the `random`-th fighter kind that is in `this_mask` and
 * not already in `prev_mask`. */
s32 scAutoDemoGetShuffledFighterKind(u16 this_mask, u16 prev_mask, s32 random)
{
    s32 ret = -1;

    random++;

    do
    {
        ret++;

        if ((this_mask & (1 << ret)) && !(prev_mask & (1 << ret)))
        {
            random--;
        }
    }
    while (random != 0);

    return ret;
}

/* scautodemo.c:510-533 scAutoDemoGetFighterKind 0x8018D8C0, verbatim:
 * players 0 and 1 are the two the title screen (or whatever set
 * gSCManagerSceneData.demo_fkind last) already
 * picked; 2 and 3 are shuffled from the unlocked roster, excluding
 * whichever kinds this demo has already used. */
s32 scAutoDemoGetFighterKind(s32 player)
{
    u16 fighter_mask;
    s32 fighter_count2;
    s32 fighter_count1;
    s32 fkind;

    if (player < 2)
    {
        return gSCManagerSceneData.demo_fkind[player];
    }
    fighter_mask = (gSCManagerBackupData.fighter_mask | LBBACKUP_CHARACTER_MASK_STARTER);

    fighter_count1 = scAutoDemoGetFighterKindsNum(fighter_mask);
    fighter_count2 = scAutoDemoGetFighterKindsNum(sSCAutoDemoFighterMask);

    fkind = scAutoDemoGetShuffledFighterKind(fighter_mask, sSCAutoDemoFighterMask, syUtilsRandIntRange(fighter_count1 - fighter_count2));

    sSCAutoDemoFighterMask |= (1 << fkind);

    return fkind;
}

/* scautodemo.c:535-543 scAutoDemoGetPlayerDamage 0x8018D954, verbatim:
 * players 0/1 (the picked pair) start light, 2/3 start heavy -- so the
 * demo's opening seconds are never a one-hit KO before the camera has
 * even reached the first focused fighter. */
s32 scAutoDemoGetPlayerDamage(s32 player)
{
    if (player < 2)
    {
        return syUtilsRandIntRange(30);
    }
    else return syUtilsRandIntRange(60) + 40;
}

/* scautodemo.c:545-579 scAutoDemoInitDemo 0x8018D990, verbatim: builds
 * this demo's own SCBattleState from the game's shared default
 * (dSCManagerDefaultBattleState, src/dc/scmanagerdata.c -- game_type
 * nSCBattleGameTypeDemo, 2 stocks, every item switch on), cycles the
 * stage order, and picks all four fighters and their starting damage. */
void scAutoDemoInitDemo(void)
{
    s32 i;

    sSCAutoDemoBattleState = dSCManagerDefaultBattleState;
    gSCManagerBattleState = &sSCAutoDemoBattleState;

    gSCManagerBattleState->game_type = nSCBattleGameTypeDemo;
    gSCManagerBattleState->gkind = dSCAutoDemoGroundOrder[gSCManagerSceneData.demo_gkind_order];

    gSCManagerSceneData.demo_gkind_order++;

    if (gSCManagerSceneData.demo_gkind_order >= ARRAY_COUNT(dSCAutoDemoGroundOrder))
    {
        gSCManagerSceneData.demo_gkind_order = 0;
    }
    sSCAutoDemoFighterMask = (1 << gSCManagerSceneData.demo_fkind[0]) | (1 << gSCManagerSceneData.demo_fkind[1]);

    for (i = 0; i < (s32)ARRAY_COUNT(gSCManagerBattleState->players); i++)
    {
        gSCManagerBattleState->players[i].pkind = nFTPlayerKindCom;
        gSCManagerBattleState->players[i].fkind = scAutoDemoGetFighterKind(i);
        gSCManagerBattleState->players[i].level = 9;

        gSCManagerBattleState->players[i].stock_damage_all = scAutoDemoGetPlayerDamage(i);
    }
    gSCManagerBattleState->pl_count = 0;
    gSCManagerBattleState->cp_count = GMCOMMON_PLAYERS_MAX;

    for (i = 0; i < (s32)ARRAY_COUNT(sSCAutoDemoMapObjs); i++)
    {
        sSCAutoDemoMapObjs[i] = 0;
    }
}

/* scautodemo.c:581-619 scAutoDemoInitSObjs 0x8018DB18.  *
 * DIVERGES only where every other already-ported scene's sprite load
 * does: lbRelocGetExternHeapFile over llCharacterNamesFileID plus
 * lbRelocGetFileData per sprite become one sprite_bank_load of
 * characternames.spr and a sprite_bank_get per plate, the substitution
 * src/dc/scexplain.c and src/dc/scstaffroll.c already make for their
 * own banks -- so the syTaskmanMalloc for the file image goes with it
 * (the bank owns its own storage). A bank that fails to load leaves
 * sSCAutoDemoFighterNameGObj NULL, which every read of it downstream still guards on. */
void scAutoDemoInitSObjs(void)
{
    GObj *interface_gobj;
    s32 player;

    if (sprite_bank_load(&sSCAutoDemoNamesBank, "characternames.spr") < 0)
    {
        sSCAutoDemoNamesFileHead = NULL;

        return;
    }
    sSCAutoDemoNamesFileHead = &sSCAutoDemoNamesBank;

    sSCAutoDemoFighterNameGObj = interface_gobj = gcMakeGObjSPAfter
    (
        nGCCommonKindInterface,
        NULL,
        nGCCommonLinkIDInterface,
        GOBJ_PRIORITY_DEFAULT
    );
    gcAddGObjDisplay(interface_gobj, lbCommonDrawSObjAttr, 23, GOBJ_PRIORITY_DEFAULT, ~0);

    for (player = 0; player < (s32)ARRAY_COUNT(gSCManagerSceneData.demo_fkind); player++)
    {
        SObj *sobj = lbCommonMakeSObjForGObj
        (
            interface_gobj,
            (Sprite *) sprite_bank_get
            (
                sSCAutoDemoNamesFileHead,
                dSCAutoDemoFighterNameSpriteOffsets
                    [gSCManagerBattleState->players[player].fkind]
            )
        );

        sobj->sprite.red   = 0xFF;
        sobj->sprite.green = 0xFF;
        sobj->sprite.blue  = 0xFF;

        sobj->sprite.attr = SP_TEXSHUF | SP_HIDDEN | SP_TRANSPARENT;

        sobj->pos.x = (s32) (160.0F - (sobj->sprite.width * 0.5F));
        sobj->pos.y = (s32) (50.0F - (sobj->sprite.height * 0.5F));
    }
}

/* scautodemo.c:621-700 scAutoDemoFuncStart 0x8018DCC4, the setup:
 * SYTaskmanSetup.func_start, so syTaskmanLoadScene calls it once the
 * pools exist and the frame loop starts the moment it returns. Ten
 * calls are cut from here for the same reasons scVSBattleStartBattle's
 * own header names (that function's comment has the full list; every
 * one applies here too, since this is the same battle spine):
 *
 *   - scAutoDemoSetupFiles above already stands in for the reloc loader
 *   - gcMakeDefaultCameraGObj (the black clear camera) is gone -- the
 *     PVR clears its own framebuffer
 *   - mpCollisionInitGroundData is stage_bind_collision below
 *   - gmCameraMakeWallpaperCamera is gone; grWallpaperMakeDecideKind
 *     (src/dc/stage.c) draws the wallpaper instead, moved after
 *     gmCameraMakeBattleCamera the way scVSBattleStartBattle's own copy
 *     is, since the substitute draws off the battle camera rather than
 *     a dedicated one
 *   - ftManagerSetupFilesPlayablesAll is gone -- the port's packs load
 *     whole up front (src/dc/ftmanager.c)
 *
 * Two differences from scVSBattleStartBattle that are the game's own,
 * not the port's: no ifCommonBattleSetGameStatusWait (no entry
 * sequence to wait through -- scAutoDemoStartBattle below sets Go
 * directly, every fighter spawns with is_skip_entry TRUE), and no
 * ifCommonEntryAllMakeInterface/ifCommonTimerMakeInterface/
 * ifCommonTimerMakeDigits -- the decomp's own scAutoDemoFuncStart calls
 * none of them, so neither does this.
 *
 * The stage acquire is this function's own addition, not scVSBattleStartScene's
 * -- see this file's header comment for why it has to be here instead
 * of in scAutoDemoStartScene. */
void scAutoDemoFuncStart(void)
{
    GObj *fighter_gobj;
    FTDesc desc;
    s32 player;

    scAutoDemoInitDemo();

    /* The stage grStageAcquire found for gkind -- exactly
     * scVSBattleStartScene's own block, fallback to Hyrule on a pack that
     * will not load included, just run here instead, since gkind was not
     * known until the line above. */
    {
        s32 gkind = gSCManagerBattleState->gkind;
        Stage *stage = grStageAcquire(gkind);

        if (stage == NULL)
        {
            syDebugPrintf("scAutoDemoFuncStart: no pack for stage %d, "
                          "playing Hyrule\n", (int)gkind);
            gSCManagerBattleState->gkind = nGRKindHyrule;
            stage = grStageAcquire(nGRKindHyrule);
        }
        sSCAutoDemoStageHeld = (stage != NULL);
        sSCAutoDemoStageKind = gSCManagerBattleState->gkind;
        if (stage != NULL)
        {
            stage_bind(stage);
        }
    }

    /* XBOX: the demo's samples -- its four fighters and its stage are
     * known only from here (src/dc/sndres.h sndres_enter_autodemo) */
    sndres_enter_autodemo();

    scAutoDemoSetupFiles();

    efParticleInitAll();
    ftParamInitGame();
    stage_bind_collision();

    gmCameraSetViewportDimensions(10, 10, 310, 230);

    itManagerInitItems();
    grCommonSetupInitAll();

    ftManagerAllocFighter(FTDATA_FLAG_MAINMOTION, GMCOMMON_PLAYERS_MAX);
    wpManagerAllocWeapons();
    efManagerLoadEffectBank();

    /* DIVERGES: same as scVSBattleStartBattle -- every model the port
     * has a pack for, loaded here rather than on first use. The item
     * switch mask on dSCManagerDefaultBattleState is ~0 (every item
     * on), so a demo can spawn anything a real match can. */
    efManagerPreloadModels();
    wpManagerPreloadModels();
    itemModelPreloadAll();

    ifScreenFlashMakeInterface(0xFF);
    gmRumbleMakeActor();
    ftPublicMakeActor();

    for (player = 0; player < (s32)ARRAY_COUNT(gSCManagerBattleState->players); player++)
    {
        desc = dFTManagerDefaultFighterDesc;

        ftManagerSetupFilesAllKind(gSCManagerBattleState->players[player].fkind);

        desc.fkind = gSCManagerBattleState->players[player].fkind;

        scAutoDemoGetPlayerStartPosition(player, &desc.pos);

        desc.lr = (desc.pos.x >= 0.0F) ? -1 : +1;
        desc.team = gSCManagerBattleState->players[player].team;
        desc.player = player;
        desc.detail = ((gSCManagerBattleState->pl_count + gSCManagerBattleState->cp_count) < 3) ? nFTPartsDetailHigh : nFTPartsDetailLow;
        desc.costume = gSCManagerBattleState->players[player].costume;
        desc.shade = gSCManagerBattleState->players[player].shade;
        desc.handicap = gSCManagerBattleState->players[player].handicap;
        desc.level = gSCManagerBattleState->players[player].level;
        desc.stock_count = gSCManagerBattleState->stocks;
        desc.damage = gSCManagerBattleState->players[player].stock_damage_all;
        desc.pkind = gSCManagerBattleState->players[player].pkind;
        desc.controller = &gSYControllerDevices[player];
        desc.figatree_heap = ftManagerAllocFigatreeHeapKind(gSCManagerBattleState->players[player].fkind);
        desc.is_skip_entry = TRUE;

        fighter_gobj = ftManagerMakeFighter(&desc);

        gSCManagerBattleState->players[player].color = player;
        gSCManagerBattleState->players[player].tag = player;

        ftParamInitPlayerBattleStats(player, fighter_gobj);
    }
    scAutoDemoStartBattle();

    gmCameraMakeBattleCamera();
    gGMCameraGObj->camera_mask = GMCAMERA_BATTLE_DLLINK_MASK;
    grWallpaperMakeDecideKind();

    gmCameraMakePlayerArrowsCamera();
    ifCommonPlayerArrowsInitInterface();
    gmCameraMakePlayerMagnifyCamera();
    ifCommonPlayerMagnifyMakeInterface();

    gIFCommonPlayerInterface.is_magnify_display = TRUE;

    gmCameraScreenFlashMakeCamera();
    gmCameraMakeInterfaceCamera();
    gmCameraMakeEffectCamera();
    ifCommonPlayerTagMakeInterface();
    ifCommonPlayerDamageSetDigitPositions();
    ifCommonPlayerDamageInitInterface();
    ifCommonPlayerDamageSetShowInterface();
    ifCommonPlayerStockInitInterface();
    scAutoDemoInitSObjs();
    mpCollisionSetPlayBGM();
    /* XBOX: the crowd, as the decomp has it (scautodemo.c:698). This had
     * become a stop-all -- the demo had no samples staged, so the cheer
     * could not have played; sndres_enter_autodemo above stages it. */
    func_800269C0_275C0(nSYAudioVoicePublicExcited);
    scAutoDemoMakeFocusInterface();

    /* From here to the scene's end nothing should be read: any file
     * that is gets a line naming it (src/dc/assetroot.h), the same
     * guard scVSBattleStartBattle ends on. */
    asset_io_expect_none("the auto-demo");
}

/* scautodemo.c:702-708 scAutoDemoFuncLights 0x8018DFC8. Not ported --
 * ftDisplayLightsDrawReflect is the same dropped reflection lighting
 * scVSBattleStartBattle's own header names, and dSCAutoDemoTaskmanSetup's
 * func_lights is NULL above, so this is never called; kept unported
 * rather than half-written, the same standard the rest of this file
 * holds to. */

/* scautodemo.c:710-731 scAutoDemoStartScene 0x8018E014, the scene
 * manager's entry.
 *
 * DIVERGES: syVideoInit and the zbuffer allocation are cut the same way
 * scVSBattleStartScene's own header explains -- one video mode, set
 * once at boot -- and dSCAutoDemoTaskmanSetup.scene_setup.arena_size is
 * the link-map address again, left at its static 0 (arena_start is
 * NULL, so syTaskmanStartTask keeps the region it already has). Unlike
 * scVSBattleStartScene, this function does not set scene_curr itself:
 * scAutoDemoExit already did, from inside the task, before
 * scManagerFuncUpdate below returns -- setting it again here would
 * overwrite that with whatever scene_curr happened to hold. The
 * BGM-stop-and-wait/volume-set/voice-cue block is NOT a sudden-death-only
 * special case the way it is in scvsbattle.c: the decomp's own
 * scAutoDemoStartScene runs it unconditionally, every time, because a
 * demo always ends into Startup rather than into a results screen that
 * wants the win jingle still playing. */
void scAutoDemoStartScene(void)
{
    dSCAutoDemoTaskmanSetup.func_start = scAutoDemoFuncStart;

    scManagerFuncUpdate(&dSCAutoDemoTaskmanSetup);

    syAudioStopBGMAll();

    while (syAudioCheckBGMPlaying(0) != FALSE)
    {
        continue;
    }
    syAudioSetBGMVolume(0, 0x7800);
    func_800266A0_272A0();
    gmRumbleInitPlayers();

    /* The stage, given back with the scene -- scVSBattleStartScene's
     * own release, read off the statics scAutoDemoFuncStart wrote
     * instead of a local the way that function's own acquire block is
     * local. */
    if (sSCAutoDemoStageHeld)
    {
        grStageRelease(sSCAutoDemoStageKind);
        sSCAutoDemoStageHeld = 0;
    }
    lbpTexFreeAll();
}

/* The port's own bzero arm for dSCManagerOverlays[64] (src/dc/overlay.c),
 * the same role scVSBattleOverlayLoad plays for overlay 4. */
void scAutoDemoOverlayLoad(void)
{
    OVERLAY_CLEAR(dSCAutoDemoFadeColor);
    OVERLAY_CLEAR(sSCAutoDemoBattleState);
    OVERLAY_CLEAR(sSCAutoDemoFocusChangeWait);
    OVERLAY_CLEAR(sSCAutoDemoFighterMask);
    OVERLAY_CLEAR(sSCAutoDemoFighterNameGObj);
    OVERLAY_CLEAR(sSCAutoDemoNamesBank);
    OVERLAY_CLEAR(sSCAutoDemoNamesFileHead);
    OVERLAY_CLEAR(sSCAutoDemoFunc);
    OVERLAY_CLEAR(sSCAutoDemoMapObjs);
    OVERLAY_CLEAR(sSCAutoDemoStageHeld);
    OVERLAY_CLEAR(sSCAutoDemoStageKind);
}
