/* mnscreenadjust.c -- see mnscreenadjust.h. Every function is
 * mn/mnoption/mnscreenadjust.c's by name and body, REGION_US arms; the
 * line numbers are the decomp's. */
#include "mnscreenadjust.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"
#include "bgm.h"

#include <sys/debug.h>
#include <sys/controller.h>
#include <sys/rdp.h>
#include <sys/video.h>
#include <sc/scdef.h>
#include <sc/scsubsys/scsubsys.h>
#include <gm/gmsound.h>
#include <lb/lbdef.h>
#include <mn/mndef.h>
#include <macros.h>

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/sysshim.c defines it over the FGM engine (mnoption.c's own
 * copy of this declaration). */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset (src/dc/mnoption.c does the same). Both values are relocData
 * file 15 (MNScreenAdjust) as tools/export/ssb_spriteexport.py --list reads it
 * off the ROM. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

#define llMNScreenAdjustInstructionSprite 0x00918
#define llMNScreenAdjustGuideSprite       0x098A0

/* The scene's own one bank. */
#define MNSCREENADJUST_BANK "mnscreenadjust.spr"

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* mnscreenadjust.c:17 (0x80132830): { &llMNScreenAdjustFileID }, the one
 * link label's value written out. */
u32 dMNScreenAdjustFileIDs[/* */] = { 15 };

/* mnscreenadjust.c:20-28 dMNScreenAdjustLights1 and
 * dMNScreenAdjustDisplayList, the lighting the pre-render function would
 * set for the 3D this scene never draws. Dropped with
 * mnScreenAdjustFuncLights (dMNScreenAdjustTaskmanSetup says why). */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

/* mnscreenadjust.c:85 sMNScreenAdjustPad0x80132920[2], two words the
 * link map names but nothing reads. Dropped: the port has no link map
 * to keep a hole in (mnoption.c's own sMNOptionPad0x801337B0 note). */

/* mnscreenadjust.c:88, 91 */
f32 sMNScreenAdjustOffsetH;
f32 sMNScreenAdjustOffsetV;

/* mnscreenadjust.c:94, 97, 100 */
s32 sMNScreenAdjustButtonHoldWait;
s32 sMNScreenAdjustTotalTimeTics;
s32 sMNScreenAdjustReturnTic;

/* mnscreenadjust.c:103, 106 sMNScreenAdjustForceStatusBuffer,
 * sMNScreenAdjustStatusBuffer: the reloc loader's per-file status
 * records. Dropped with lbRelocInitSetup (mnScreenAdjustLoadFiles). */

/* mnscreenadjust.c:109 */
void *sMNScreenAdjustFiles[ARRAY_COUNT(dMNScreenAdjustFileIDs)];

/* the bank behind sMNScreenAdjustFiles */
static SpriteBank sMNScreenAdjustBanks[ARRAY_COUNT(dMNScreenAdjustFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mnscreenadjust.c:118-121 mnScreenAdjustFuncLights: the scene's
 * pre-render function, two GBI commands setting a single light for 3D
 * this scene has none of. Dropped, as every other menu scene's is
 * (src/dc/mnvsmode.c). */

/* mnscreenadjust.c:123-140 0x80131B24, the RDP commands in the port's
 * spelling (src/dc/lbcommon.h): the amber crosshair (two bars through
 * the guide picture's centre) and the grey border frame around it, both
 * G_CYC_1CYCLE -- the same cycle type mnOptionLabelsProcDisplay draws
 * under, so the corners are the decomp's unchanged (that file's own note
 * on why G_CYC_FILL's corner, not this one, needs the +1). The trailing
 * gDPPipeSync/gDPSetRenderMode reset is lbCommonClearExternSpriteParams,
 * the same reset mnOptionLabelsProcDisplay makes; this GObj carries no
 * SObj of its own (mnScreenAdjustMakeFrame below), so there is nothing
 * to call lbCommonDrawSObjAttr on afterward. */
void mnScreenAdjustFrameProcDisplay(GObj *gobj)
{
    (void)gobj;

    lbCommonSpriteFillRect(159, 0, 161, 254, 0xBF, 0xA4, 0x47, 0xFF);
    lbCommonSpriteFillRect(0, 119, 334, 121, 0xBF, 0xA4, 0x47, 0xFF);

    lbCommonSpriteFillRect(44, 44, 276, 45, 0x8B, 0x8B, 0x8B, 0xFF);
    lbCommonSpriteFillRect(44, 196, 276, 197, 0x8B, 0x8B, 0x8B, 0xFF);
    lbCommonSpriteFillRect(44, 44, 45, 196, 0x8B, 0x8B, 0x8B, 0xFF);
    lbCommonSpriteFillRect(276, 44, 277, 196, 0x8B, 0x8B, 0x8B, 0xFF);

    lbCommonClearExternSpriteParams();
}

/* mnscreenadjust.c:142-148 0x80131D00, verbatim. */
void mnScreenAdjustMakeFrame(void)
{
    GObj *gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, mnScreenAdjustFrameProcDisplay, 0, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mnscreenadjust.c:150-163 0x80131D4C, verbatim. */
void mnScreenAdjustMakeGuide(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNScreenAdjustFiles[0], llMNScreenAdjustGuideSprite));

    sobj->pos.x = 10.0F;
    sobj->pos.y = 10.0F;
}

/* mnscreenadjust.c:165-189 0x80131DCC, the REGION_US arm, verbatim. */
void mnScreenAdjustMakeInstruction(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 3, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 1, GOBJ_PRIORITY_DEFAULT, ~0);
    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMNScreenAdjustFiles[0], llMNScreenAdjustInstructionSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red   = 0xFF;
    sobj->sprite.green = 0xFF;
    sobj->sprite.blue  = 0xFF;

    sobj->pos.x = 16.0F;
    sobj->pos.y = 198.0F;
}

/* mnscreenadjust.c:191-214 0x80131E74, verbatim: the crosshair/border
 * camera, drawn last (DL priority 40) over the guide picture's own
 * (70, mnScreenAdjustMakeSpriteCamera below) so it reads on top as the
 * offset moves. */
void mnScreenAdjustMakeFrameCamera(void)
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

/* mnscreenadjust.c:216-239 0x80131F14, verbatim. */
void mnScreenAdjustMakeSpriteCamera(void)
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

/* mnscreenadjust.c:241-245 0x80131FB4, verbatim. DIVERGES: nothing reads
 * the offsets it writes (src/dc/sysshim.c's own note on
 * gSYVideoOffsetLeft/Top) -- the state is kept, correctly, and moves
 * nothing. */
void mnScreenAdjustApplyCenterOffsets(s16 h, s16 v)
{
    syVideoSetCenterOffsets(h, h, v, v);
}

/* mnscreenadjust.c:247-257 0x80131FF8, verbatim. */
void mnScreenAdjustInitVars(void)
{
    sMNScreenAdjustOffsetH = gSYVideoOffsetLeft;
    sMNScreenAdjustOffsetV = gSYVideoOffsetTop;

    sMNScreenAdjustButtonHoldWait = 0;
    sMNScreenAdjustTotalTimeTics = 0;

    sMNScreenAdjustReturnTic = sMNScreenAdjustTotalTimeTics + I_MIN_TO_TICS(5);
}

/* mnscreenadjust.c:259-266 0x8013204C, verbatim. */
void mnScreenAdjustBackupOffsets(void)
{
    gSCManagerBackupData.screen_adjust_h = sMNScreenAdjustOffsetH;
    gSCManagerBackupData.screen_adjust_v = sMNScreenAdjustOffsetV;

    lbBackupWrite();
}

/* mnscreenadjust.c:268-417 0x8013209C, verbatim.
 *
 * Ten tics deaf, then every tic: A, B or START writes the offsets back
 * and returns to Options; the D-pad/C-buttons U/D/L/R step one unit a
 * tap and, while sMNScreenAdjustButtonHoldWait is zero, the stick moves
 * it continuously (stick_range/50.0F a tic); Z resets both offsets to
 * zero. sMNScreenAdjustButtonHoldWait itself is read every tic but
 * written only by the in-range check near the top and the unconditional
 * zero at the bottom -- the decomp's own dead field, ported anyway
 * rather than silently dropped (mnoption.c's D_ovl60_801337D4 is the
 * same kind of thing). */
void mnScreenAdjustFuncRun(GObj *gobj)
{
    s32 stick_range;

    (void)gobj;

    sMNScreenAdjustTotalTimeTics++;

    if (sMNScreenAdjustTotalTimeTics >= 10)
    {
        if (sMNScreenAdjustButtonHoldWait != 0)
        {
            sMNScreenAdjustButtonHoldWait--;
        }
        if ((scSubsysControllerGetPlayerStickInRangeLR(-30, 30) != FALSE) && (scSubsysControllerGetPlayerStickInRangeUD(-30, 30) != FALSE))
        {
            sMNScreenAdjustButtonHoldWait = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
        {
            func_800269C0_275C0(nSYAudioFGMMenuSelect);

            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindOption;

            mnScreenAdjustBackupOffsets();
            syTaskmanSetLoadScene();
        }
        if (scSubsysControllerGetPlayerTapButtons(U_JPAD | U_CBUTTONS) != FALSE)
        {
            if (sMNScreenAdjustOffsetV > -14.0F)
            {
                sMNScreenAdjustOffsetV--;

                if (sMNScreenAdjustOffsetV < -14.0F)
                {
                    sMNScreenAdjustOffsetV = -14.0F;
                }
                mnScreenAdjustApplyCenterOffsets(sMNScreenAdjustOffsetH, sMNScreenAdjustOffsetV);
            }
        }
        if (sMNScreenAdjustButtonHoldWait == 0)
        {
            stick_range = scSubsysControllerGetPlayerStickUD(30, 1);

            if ((stick_range != 0) && (sMNScreenAdjustOffsetV > -14.0F))
            {
                sMNScreenAdjustOffsetV -= stick_range / 50.0F;

                if (sMNScreenAdjustOffsetV < -14.0F)
                {
                    sMNScreenAdjustOffsetV = -14.0F;
                }
                mnScreenAdjustApplyCenterOffsets(sMNScreenAdjustOffsetH, sMNScreenAdjustOffsetV);
            }
        }
        if (scSubsysControllerGetPlayerTapButtons(D_JPAD | D_CBUTTONS) != FALSE)
        {
            if (sMNScreenAdjustOffsetV < 14.0F)
            {
                sMNScreenAdjustOffsetV++;

                if (sMNScreenAdjustOffsetV > 14.0F)
                {
                    sMNScreenAdjustOffsetV = 14.0F;
                }
                mnScreenAdjustApplyCenterOffsets(sMNScreenAdjustOffsetH, sMNScreenAdjustOffsetV);
            }
        }
        if (sMNScreenAdjustButtonHoldWait == 0)
        {
            stick_range = scSubsysControllerGetPlayerStickUD(-30, 0);

            if ((stick_range != 0) && (sMNScreenAdjustOffsetV < 14.0F))
            {
                sMNScreenAdjustOffsetV -= stick_range / 50.0F;

                if (sMNScreenAdjustOffsetV > 14.0F)
                {
                    sMNScreenAdjustOffsetV = 14.0F;
                }
                mnScreenAdjustApplyCenterOffsets(sMNScreenAdjustOffsetH, sMNScreenAdjustOffsetV);
            }
        }
        if (scSubsysControllerGetPlayerTapButtons(L_JPAD | L_TRIG | L_CBUTTONS) != FALSE)
        {
            if (sMNScreenAdjustOffsetH > -14.0F)
            {
                sMNScreenAdjustOffsetH--;

                if (sMNScreenAdjustOffsetH < -14.0F)
                {
                    sMNScreenAdjustOffsetH = -14.0F;
                }
                mnScreenAdjustApplyCenterOffsets(sMNScreenAdjustOffsetH, sMNScreenAdjustOffsetV);
            }
        }
        if (sMNScreenAdjustButtonHoldWait == 0)
        {
            stick_range = scSubsysControllerGetPlayerStickLR(-30, 0);

            if ((stick_range != 0) && (sMNScreenAdjustOffsetH > -14.0F))
            {
                sMNScreenAdjustOffsetH += stick_range / 50.0F;

                if (sMNScreenAdjustOffsetH < -14.0F)
                {
                    sMNScreenAdjustOffsetH = -14.0F;
                }
                mnScreenAdjustApplyCenterOffsets(sMNScreenAdjustOffsetH, sMNScreenAdjustOffsetV);
            }
        }
        if (scSubsysControllerGetPlayerTapButtons(R_JPAD | R_TRIG | R_CBUTTONS) != FALSE)
        {
            if (sMNScreenAdjustOffsetH < 14.0F)
            {
                sMNScreenAdjustOffsetH++;

                if (sMNScreenAdjustOffsetH > 14.0F)
                {
                    sMNScreenAdjustOffsetH = 14.0F;
                }
                mnScreenAdjustApplyCenterOffsets(sMNScreenAdjustOffsetH, sMNScreenAdjustOffsetV);
            }
        }
        if (sMNScreenAdjustButtonHoldWait == 0)
        {
            stick_range = scSubsysControllerGetPlayerStickLR(30, 1);

            if ((stick_range != 0) && (sMNScreenAdjustOffsetH < 14.0F))
            {
                sMNScreenAdjustOffsetH += stick_range / 50.0F;

                if (sMNScreenAdjustOffsetH > 14.0F)
                {
                    sMNScreenAdjustOffsetH = 14.0F;
                }
                mnScreenAdjustApplyCenterOffsets(sMNScreenAdjustOffsetH, sMNScreenAdjustOffsetV);
            }
        }
        sMNScreenAdjustButtonHoldWait = 0;

        if (scSubsysControllerGetPlayerTapButtons(Z_TRIG) != FALSE)
        {
            sMNScreenAdjustOffsetH = 0.0F;
            sMNScreenAdjustOffsetV = 0.0F;

            mnScreenAdjustApplyCenterOffsets(sMNScreenAdjustOffsetH, sMNScreenAdjustOffsetV);
        }
    }
}

/* mnscreenadjust.c:419-435 lbRelocInitSetup and lbRelocLoadFilesListed,
 * as every other menu scene has them (src/dc/mndata.c): one sprite bank
 * stands in for the scene's one file, loaded out of the romdisk into
 * the scene heap and VRAM. */
static void mnScreenAdjustLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dMNScreenAdjustFileIDs)] =
    {
        MNSCREENADJUST_BANK
    };
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(dMNScreenAdjustFileIDs); i++)
    {
        if (sprite_bank_load(&sMNScreenAdjustBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("mnScreenAdjust: no bank for file %d (%s)\n",
                          (int)dMNScreenAdjustFileIDs[i], paths[i]);
            sMNScreenAdjustFiles[i] = NULL;
            continue;
        }
        sMNScreenAdjustFiles[i] = &sMNScreenAdjustBanks[i];
    }
}

/* mnscreenadjust.c:419-445 0x801326CC, in the game's order.
 *
 * DIVERGES: the LBRelocSetup block is mnScreenAdjustLoadFiles above, and
 * gcMakeDefaultCameraGObj -- the black clear camera -- is the frame
 * clear, which the PVR does itself (the same cut as src/dc/mnoption.c's
 * and src/dc/mndata.c's). */
void mnScreenAdjustFuncStart(void)
{
    mnScreenAdjustLoadFiles();

    gcMakeGObjSPAfter(0, mnScreenAdjustFuncRun, 0, GOBJ_PRIORITY_DEFAULT);

    mnScreenAdjustInitVars();
    mnScreenAdjustMakeFrameCamera();
    mnScreenAdjustMakeSpriteCamera();
    mnScreenAdjustMakeGuide();
    mnScreenAdjustMakeFrame();
    mnScreenAdjustMakeInstruction();
    syAudioStopBGMAll();
}

/* mnscreenadjust.c:30-76 (0x80132878, 0x80132894). The pool counts are
 * the game's, all zero: a menu scene takes its objects straight from the
 * scene heap. DIVERGES as every other menu scene's: the arena is the
 * port's region, the draw is the scene manager's, and
 * mnScreenAdjustFuncLights is dropped. dMNScreenAdjustVideoSetup itself
 * is not ported: see mnScreenAdjustStartScene. */
SYTaskmanSetup dMNScreenAdjustTaskmanSetup =
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

    mnScreenAdjustFuncStart             // Task start function
};

/* The bzero arm of syDmaLoadOverlay for overlay 25, this file:
 * src/dc/scmanager.c calls it on the way into the scene below.
 * src/dc/overlay.h says why the port needs it written out. */
void mnScreenAdjustOverlayLoad(void)
{
    OVERLAY_CLEAR(sMNScreenAdjustOffsetH);
    OVERLAY_CLEAR(sMNScreenAdjustOffsetV);
    OVERLAY_CLEAR(sMNScreenAdjustButtonHoldWait);
    OVERLAY_CLEAR(sMNScreenAdjustTotalTimeTics);
    OVERLAY_CLEAR(sMNScreenAdjustReturnTic);
    OVERLAY_CLEAR(sMNScreenAdjustFiles);
    OVERLAY_CLEAR(sMNScreenAdjustBanks);
}

/* mnscreenadjust.c:447-455 0x801327D8. DIVERGES: syVideoInit, the
 * zbuffer and the arena_size line are the N64's video mode and its link
 * map, set once at boot here as in every other scene. */
void mnScreenAdjustStartScene(void)
{
    syTaskmanStartTask(&dMNScreenAdjustTaskmanSetup);
}
