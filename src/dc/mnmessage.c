/* mnmessage.c -- see mnmessage.h. Every function is
 * mn/mncommon/mnmessage.c's by name and body, REGION_US arms; the line
 * numbers are the decomp's. */
#include "mnmessage.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"
#include "bgm.h"

#include <sys/debug.h>
#include <sys/controller.h>
#include <sys/rdp.h>
#include <sc/scdef.h>
#include <sc/scsubsys/scsubsys.h>
#include <gm/gmsound.h>
#include <lb/lbbackup.h>
#include <lb/lbdef.h>
#include <ft/ftdef.h>
#include <mn/mndef.h>
#include <PR/os.h>
#include <macros.h>

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/sysshim.c defines it over the FGM engine */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mnvsmode.c does the same). Each `&llXxxSprite` below
 * is written `llXxxSprite`, the number, with the label's name kept as
 * the macro's. The values are relocData files 0 (MNCommon) and 9
 * (MNMessage) as tools/export/ssb_spriteexport.py --list reads them off the
 * ROM. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

#define llMNCommonSmashBrosCollageSprite 0x18000

#define llMNMessageUnlockLuigiSprite      0x009E0
#define llMNMessageUnlockNessSprite       0x01148
#define llMNMessageUnlockCaptainSprite    0x01F50
#define llMNMessageUnlockPurinSprite      0x02E58
#define llMNMessageUnlockInishieSprite    0x03458
#define llMNMessageUnlockSoundTestSprite  0x04180
#define llMNMessageUnlockItemSwitchSprite 0x04EB0
#define llMNMessageDecalExclaimSprite     0x05300

/* The banks the two files became. The common bank is the mode select's,
 * exported with this scene's collage in it already -- the character
 * select and the VS mode draw the same sprite. */
#define MNMESSAGE_BANK_COMMON  "mncommon.spr"
#define MNMESSAGE_BANK_MESSAGE "mnmessage.spr"

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mnmessage.c:15 (0x80132500): { &llMNCommonFileID, &llMNMessageFileID },
 * the two link labels' values written out. */
u32 dMNMessageFileIDs[/* */] = { 0, 9 };

/* mnmessage.c:18-26 dMNMessageLights1 and dMNMessageDisplayList, the
 * lighting the pre-render function would set for the 3D this scene
 * never draws. Dropped with mnMessageFuncLights (dMNMessageTaskmanSetup
 * says why). */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* mnmessage.c:37 */
s32 sMNMessageUnlockID;

/* mnmessage.c:39 */
s32 sMNMessageQueueID;

/* mnmessage.c:41. Decremented by mnMessageFuncRun and zeroed by a
 * centred stick, and never set to anything: dead in the ROM as it is
 * here, and kept because it is a word of the overlay's noload segment
 * that the reload clears. */
static s32 sMNMessageUnk0x80132660;

/* mnmessage.c:43 */
static s32 sMNMessageTotalTimeTics;

/* mnmessage.c:46 sMNMessageStatusBuffer[100], the reloc loader's
 * per-file status records. Dropped with lbRelocInitSetup
 * (mnMessageLoadFiles). */

/* mnmessage.c:49 */
void *sMNMessageFiles[ARRAY_COUNT(dMNMessageFileIDs)];

/* the banks behind sMNMessageFiles */
static SpriteBank sMNMessageBanks[ARRAY_COUNT(dMNMessageFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mnmessage.c:58-61 mnMessageFuncLights: the scene's pre-render
 * function, two GBI commands setting a single light for 3D this scene
 * has none of. Dropped, as every other menu scene's is
 * (src/dc/mnvsmode.c). */

/* mnmessage.c:64-77 0x80131B24, verbatim. */
void mnMessageMakeWallpaper(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 0, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMessageFiles[0], llMNCommonSmashBrosCollageSprite));

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;
}

/* mnmessage.c:80-92 0x80131BA4, the blue sheet over the wallpaper: one
 * gDPFillRectangle in G_CYC_1CYCLE under G_CC_PRIMITIVE, blue at a
 * quarter alpha, across the whole safe area. 1CYCLE's lower-right
 * corner is exclusive, which is the corner lbCommonSpriteFillRect takes,
 * so the numbers are the decomp's unchanged (src/dc/scvsresults.c's
 * tint is the same shape). */
void mnMessageTintProcDisplay(GObj *gobj)
{
    (void)gobj;

    lbCommonSpriteFillRect(10, 10, 310, 230, 0x00, 0x00, 0xFF, 0x3F);

    lbCommonClearExternSpriteParams();
}

/* mnmessage.c:95-98 0x80131CB8, verbatim. */
void mnMessageMakeTint(void)
{
    gcAddGObjDisplay(gcMakeGObjSPAfter(0, NULL, 4, GOBJ_PRIORITY_DEFAULT), mnMessageTintProcDisplay, 2, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mnmessage.c:101-115 0x80131D04, verbatim. */
void mnMessageMakeExclaim(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 5, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 3, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMessageFiles[1], llMNMessageDecalExclaimSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 140.0F;
    sobj->pos.y = 62.0F;
}

/* mnmessage.c:118-166 0x80131D9C, verbatim, the REGION_US positions.
 * The two tables are the decomp's function-scope ones, at 0x80132548
 * and 0x80132564. */
void mnMessageMakeMessage(s32 message)
{
    GObj *gobj;
    SObj *sobj;

    // 0x80132548
    intptr_t message_offsets[/* */] =
    {
        llMNMessageUnlockLuigiSprite,
        llMNMessageUnlockNessSprite,
        llMNMessageUnlockCaptainSprite,
        llMNMessageUnlockPurinSprite,
        llMNMessageUnlockInishieSprite,
        llMNMessageUnlockSoundTestSprite,
        llMNMessageUnlockItemSwitchSprite
    };

    // 0x80132564
    Vec2i message_pos[/* */] =
    {
        { 85, 114 },
        { 35, 123 },
        { 58, 114 },
        { 44, 114 },
        { 48, 123 },
        { 56, 114 },
        { 54, 114 }
    };

    gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNMessageFiles[1], message_offsets[message]));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = message_pos[message].x;
    sobj->pos.y = message_pos[message].y;
}

/* mnmessage.c:169-190 0x80131EE8, verbatim. Four cameras, one per DL
 * link, drawn in the priority order the game gives them: the wallpaper
 * at 80, the tint over it at 70, the "!" at 60 and the message at 40. */
void mnMessageMakeTintCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            1,
            NULL,
            1,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            70,
            COBJ_MASK_DLLINK(2),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mnmessage.c:193-214 0x80131F88, verbatim. */
void mnMessageMakeMessageCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            1,
            NULL,
            1,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            40,
            COBJ_MASK_DLLINK(1),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mnmessage.c:217-238 0x80132028, verbatim. */
void mnMessageMakeWallpaperCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            1,
            NULL,
            1,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            80,
            COBJ_MASK_DLLINK(0),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mnmessage.c:241-262 0x801320C8, verbatim. */
void mnMessageMakeExclaimCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            1,
            NULL,
            1,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            60,
            COBJ_MASK_DLLINK(3),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
}

/* mnmessage.c:265-271 0x80132168, verbatim: take this visit's message
 * out of the queue and put "nothing here" back in its slot, so the
 * loop in mnMessageStartScene stops on it next time round. */
void mnMessageInitVars(void)
{
    sMNMessageUnlockID = gSCManagerSceneData.unlock_messages[sMNMessageQueueID];
    gSCManagerSceneData.unlock_messages[sMNMessageQueueID] = nLBBackupUnlockEnumCount;

    sMNMessageUnk0x80132660 = 0;
    sMNMessageTotalTimeTics = 0;
}

/* mnmessage.c:274-291 0x801321A4, verbatim: the unlock bit, and for the
 * four hidden fighters their bit in fighter_mask and the character
 * select's cursor moved onto them. Then the save data goes to the store
 * (src/dc/sysshim.c until the VMU). */
void mnMessageApplyUnlock(void)
{
    // 0x8013259C
    u8 fkinds[/* */] = { nFTKindLuigi, nFTKindNess, nFTKindCaptain, nFTKindPurin };

    gSCManagerBackupData.unlock_mask |= (1 << sMNMessageUnlockID);

    switch (sMNMessageUnlockID)
    {
    case nLBBackupUnlockLuigi:
    case nLBBackupUnlockNess:
    case nLBBackupUnlockCaptain:
    case nLBBackupUnlockPurin:
        gSCManagerBackupData.fighter_mask |= (1 << fkinds[sMNMessageUnlockID]);
        gSCManagerBackupData.characters_fkind = fkinds[sMNMessageUnlockID];
        break;
    }
    lbBackupWrite();
}

/* mnmessage.c:294-314 0x8013223C, verbatim: two seconds of nothing,
 * then A, B or START ends the scene. */
void mnMessageFuncRun(GObj *gobj)
{
    (void)gobj;

    sMNMessageTotalTimeTics++;

    if (sMNMessageTotalTimeTics >= 120)
    {
        if (sMNMessageUnk0x80132660 != 0)
        {
            sMNMessageUnk0x80132660--;
        }
        if ((scSubsysControllerGetPlayerStickInRangeLR(-30, 30) != FALSE) && (scSubsysControllerGetPlayerStickInRangeUD(-30, 30) != FALSE))
        {
            sMNMessageUnk0x80132660 = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
        {
            mnMessageApplyUnlock();
            syTaskmanSetLoadScene();
        }
    }
}

/* mnmessage.c:317-320 lbRelocInitSetup and :321 lbRelocLoadFilesListed,
 * as every other menu scene has them (src/dc/mnvsmode.c): two sprite
 * banks stand in for the two relocData files, loaded out of the romdisk
 * into the scene heap and VRAM. */
static void mnMessageLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMNMessageFileIDs)] =
    {
        MNMESSAGE_BANK_COMMON,
        MNMESSAGE_BANK_MESSAGE
    };
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(dMNMessageFileIDs); i++)
    {
        if (sprite_bank_load(&sMNMessageBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mnMessage: no bank for file %d (%s)\n",
                          (int)dMNMessageFileIDs[i], paths[i]);
            sMNMessageFiles[i] = NULL;
            continue;
        }
        sMNMessageFiles[i] = &sMNMessageBanks[i];
    }
}

/* mnmessage.c:317-359 0x801322D4, in the game's order.
 *
 * DIVERGES: the LBRelocSetup block is mnMessageLoadFiles above, and
 * gcMakeDefaultCameraGObj -- the black clear camera -- is dropped
 * because the PVR clears its own framebuffer (src/dc/taskman.c). */
void mnMessageFuncStart(void)
{
    mnMessageLoadFiles();

    gcMakeGObjSPAfter(0, mnMessageFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

    mnMessageInitVars();
    mnMessageMakeWallpaperCamera();
    mnMessageMakeMessageCamera();
    mnMessageMakeTintCamera();
    mnMessageMakeExclaimCamera();
    mnMessageMakeWallpaper();
    mnMessageMakeTint();
    mnMessageMakeExclaim();
    mnMessageMakeMessage(sMNMessageUnlockID);

    syAudioPlayBGM(0, nSYAudioBGMMessage);
    func_800269C0_275C0(nSYAudioFGMDeadUpStar);
}

/* mnmessage.c:362 dMNMessageVideoSetup is the N64's video mode: see
 * mnMessageStartScene. */

/* mnmessage.c:365-406 (0x801325BC). The pool counts are the game's, all
 * zero: a menu scene takes its objects straight from the scene heap.
 * DIVERGES as every other menu scene's: the arena is the port's region,
 * the draw is the scene manager's, and mnMessageFuncLights is dropped. */
SYTaskmanSetup dMNMessageTaskmanSetup =
{
    // Task Manager Buffer Setup
    {
        0,                              // ???
        gcRunAll,                       // Update function
        scManagerFuncDraw,              // Frame draw function
        NULL,                           // Allocatable memory pool start
        0,                              // Allocatable memory pool size
        1,                              // ???
        2,                              // Number of contexts?
        0, 0, 0, 0,                     // the four DL buffer sizes
        0,                              // Graphics Heap Size
        2,                              // ???
        0,                              // RDP Output Buffer Size
        NULL,                           // Pre-render function
        syControllerFuncRead,           // Controller I/O function
    },

    0,                                  // Number of GObjThreads
    sizeof(u64) * 192,                  // Thread stack size
    0,                                  // Number of thread stacks
    0,                                  // ???
    0,                                  // Number of GObjProcesses
    0,                                  // Number of GObjs
    sizeof(GObj),                       // GObj size
    0,                                  // Number of XObjs
    NULL,                               // Matrix function list
    NULL,                               // DObjVec eject function
    0,                                  // Number of AObjs
    0,                                  // Number of MObjs
    0,                                  // Number of DObjs
    sizeof(DObj),                       // DObj size
    0,                                  // Number of SObjs
    sizeof(SObj),                       // SObj size
    0,                                  // Number of CObjs
    sizeof(CObj),                       // Camera size

    mnMessageFuncStart                  // Task start function
};

/* The bzero arm of syDmaLoadOverlay for overlay 22, this file:
 * src/dc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnMessageOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNMessageUnlockID);
    OVERLAY_CLEAR(sMNMessageQueueID);
    OVERLAY_CLEAR(sMNMessageUnk0x80132660);
    OVERLAY_CLEAR(sMNMessageTotalTimeTics);
    OVERLAY_CLEAR(sMNMessageFiles);
    OVERLAY_CLEAR(sMNMessageBanks);
}

/* mnmessage.c:409-462 0x801323F8, the REGION_US arms.
 *
 * The loop is the scene: one task per queued message, walking
 * gSCManagerSceneData.unlock_messages from the front and stopping at
 * the first "nothing here" -- which is the slot mnMessageInitVars has
 * just put back, so each pass consumes one message and the next pass
 * finds the next. A scene that runs more than once is unusual in this
 * game and this is the only one.
 *
 * DIVERGES: syVideoInit, the zbuffer and the arena_size line are the
 * N64's video mode and its link map, set once at boot here as in every
 * other scene. Where the game goes when it did not come from the VS
 * results screen is nSCKindStartup (src/dc/mnstartup.c). That arm is
 * the 1P game's: sc1pmanager.c's router runs this scene for the unlocks
 * a run earns and for the bonus stages' Sound Test unlock
 * (sc1pbonusstage.c), and the router, not this arm, decides where those
 * go next. */
void mnMessageStartScene(void)
{
    for
    (
        sMNMessageQueueID = 0;
        sMNMessageQueueID < nLBBackupUnlockEnumCount && gSCManagerSceneData.unlock_messages[sMNMessageQueueID] != nLBBackupUnlockEnumCount;
        sMNMessageQueueID++
    )
    {
        syTaskmanStartTask(&dMNMessageTaskmanSetup);
    }

    if (gSCManagerSceneData.scene_prev == nSCKindVSResults)
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindPlayersVS;
    }
    else
    {
        gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
        gSCManagerSceneData.scene_curr = nSCKindStartup;
    }
}
