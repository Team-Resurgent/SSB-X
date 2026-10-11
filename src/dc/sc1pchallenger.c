/* sc1pchallenger.c -- see sc1pchallenger.h. Every function is
 * sc/sc1pmode/sc1pchallenger.c's by name and body unless marked
 * DIVERGES; the line numbers are the decomp's. */
#include "sc1pchallenger.h"
#include "overlay.h"
#include "lbcommon.h"
#include "sprite.h"
#include "scmanager.h"
#include "ftcommon.h"
#include "bgm.h"
#include "fighter.h"
#include "objmodel.h"
#include "efmanager.h"

#include <sys/debug.h>
#include <sys/controller.h>
#include <sys/rdp.h>
#include <sys/utils.h>
#include <sys/objdisplay.h>
#include <sc/scdef.h>
#include <sc/scsubsys/scsubsys.h>
#include <gm/gmsound.h>
#include <ef/efparticle.h>
#include <lb/lbdef.h>
#include <PR/os.h>

/* n_env.c's start-a-voice-script, which no decomp header declares;
 * src/dc/sysshim.c defines it over the FGM engine */
alSoundEffect *func_800269C0_275C0(u32 fgm_id);

/* ft/ftparam.h:2642 (src/dc/ftparam.c); the header itself wants the
 * effect types behind it, which the port has no header for */
s32 ftParamGetCostumeCommonID(s32 fkind, s32 color);

/* The game reaches a sprite as a file's base plus the offset its link
 * label took; the port's file is a bank and the label's value is the
 * offset, as src/dc/sc1pintro.c and every other ported scene do it.
 * Each `&llXxxSprite` is written `llXxxSprite`, the number. */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* the decomp's share of unused locals */
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

/* The one file, as src/dc/decomp/reloc_data.us.h reads it off the ROM:
 * 10 SC1PChallenger. */
#define SC1PCHALLENGER_BANK_MAIN "sc1pchallenger.spr"

/* The sprite offsets, from src/dc/decomp/reloc_data.us.h -- the values
 * the decomp's own ll<Name>Sprite link labels carry. */
#define llSC1PChallengerChallengerTextSprite    0x1f8
#define llSC1PChallengerApproachingTextSprite   0x488
#define llSC1PChallengerWarningTextSprite       0x968
#define llSC1PChallengerDecalExclaimSprite      0xdb0

/* ---- the pools --------------------------------------------------------
 *
 * The decomp's dSC1PChallengerTaskmanSetup carries zero for every count
 * (the real numbers are in the ROM's data, not in the decomp), so these
 * are the port's. This scene is one fighter, four sprites on one GObj
 * and two cameras -- half of src/dc/mvopeningmario.c's bill, which is
 * itself the smallest one-fighter scene in the port -- so these are
 * that file's numbers with the fighter counts halved and the SObjs and
 * CObjs raised to cover the four decals and the two cameras with room
 * to spare. The scene heap is 1280 KB and these come out of it;
 * syTaskmanSetupPools prints the high-water mark. */
#define SC1PCHALLENGER_GOBJS       32
#define SC1PCHALLENGER_GOBJPROCS   32
#define SC1PCHALLENGER_XOBJS      192
#define SC1PCHALLENGER_AOBJS      384
#define SC1PCHALLENGER_MOBJS       32
#define SC1PCHALLENGER_DOBJS       64
#define SC1PCHALLENGER_SOBJS       16
#define SC1PCHALLENGER_COBJS        8

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

/* 0x80132370. The decomp writes the link label; its value is what
 * src/dc/decomp/reloc_data.us.h prints, and the port's bank stands in
 * for the file itself (sc1PChallengerLoadFiles). */
u32 dSC1PChallengerFileIDs[/* */] = { 10 };

/* one SpriteBank per file */
static SpriteBank sSC1PChallengerBanks[ARRAY_COUNT(dSC1PChallengerFileIDs)];

/* CUT: dSC1PChallengerLights1 and dSC1PChallengerDisplayList, the
 * pre-render lighting list -- dSC1PChallengerTaskmanSetup says why. */

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x80132480
s32 sSC1PChallengerPad0x80132480[2];

// 0x80132488
s32 sSC1PChallengerFighterKind;

// 0x8013248C
void *sSC1PChallengerFigatreeHeap;

// 0x80132490
f32 sSC1PChallengerUnk0x80132490;

// 0x80132494
s32 sSC1PChallengerUnk0x80132494;

// 0x80132498
s32 sSC1PChallengerTotalTimeTics;

// 0x8013249C
s32 sSC1PChallengerPad0x8013249C;

/* CUT: sSC1PChallengerForceStatusBuffer and sSC1PChallengerStatusBuffer,
 * the reloc loader's bookkeeping -- sc1PChallengerLoadFiles replaces the
 * loader they belong to. */

// 0x801327F8
void *sSC1PChallengerFiles[ARRAY_COUNT(dSC1PChallengerFileIDs)];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* CUT: sc1PChallengerFuncLights (0x80131B00), the pre-render function --
 * dSC1PChallengerTaskmanSetup carries NULL where the decomp names it,
 * as every other ported scene's does. */

/* sc1pchallenger.c:124-138 0x80131B24.
 *
 * DIVERGES: the eight raw gDP commands are the one rectangle they add
 * up to. G_CYC_1CYCLE under G_CC_PRIMITIVE at 0x00,0x00,0x3B and 0xFF
 * alpha in G_RM_AA_XLU_SURF, then the render mode put back for whoever
 * draws next -- which on this port nobody needs, because a quad carries
 * its own blend state. 1CYCLE's lower-right corner is exclusive, which
 * is the corner lbCommonSpriteFillRect takes, so the numbers are the
 * decomp's unchanged. The same substitution src/dc/mn1pmode.c:406 and
 * src/dc/mndata.c:404 make, to the pixel and the byte.
 *
 * This is the dark blue bar the silhouette turns in front of: the
 * decals camera below is priority 70 and the fighter's is 40, so the
 * bar is laid down first and the fighter stands on it. */
void sc1PChallengerDecalsProcDisplay(GObj *gobj)
{
    lbCommonSpriteFillRect(207, 92, 287, 216, 0x00, 0x00, 0x3B, 0xFF);

    lbCommonClearExternSpriteParams();
    lbCommonDrawSObjAttr(gobj);
}

// 0x80131C40
void sc1PChallengerMakeDecals(void)
{
    GObj *gobj;
    SObj *sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 2, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, sc1PChallengerDecalsProcDisplay, 0, GOBJ_PRIORITY_DEFAULT, ~0);

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PChallengerFiles[0], llSC1PChallengerDecalExclaimSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->pos.x = 139.0F;
    sobj->pos.y = 22.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PChallengerFiles[0], llSC1PChallengerWarningTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xD5;
    sobj->sprite.green = 0x00;
    sobj->sprite.blue = 0x27;

    sobj->pos.x = 100.0F;
    sobj->pos.y = 63.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PChallengerFiles[0], llSC1PChallengerChallengerTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0xA8;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 55.0F;
    sobj->pos.y = 127.0F;

    sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSC1PChallengerFiles[0], llSC1PChallengerApproachingTextSprite));

    sobj->sprite.attr &= ~SP_FASTCOPY;
    sobj->sprite.attr |= SP_TRANSPARENT;

    sobj->sprite.red = 0xFF;
    sobj->sprite.green = 0xA8;
    sobj->sprite.blue = 0x00;

    sobj->pos.x = 55.0F;
    sobj->pos.y = 149.0F;
}

// 0x80131DF4
void sc1PChallengerFighterProcUpdate(GObj *fighter_gobj)
{
    DObjGetStruct(fighter_gobj)->rotate.vec.f.y += F_CST_DTOR32(2.0F);

    if (DObjGetStruct(fighter_gobj)->rotate.vec.f.y > F_CST_DTOR32(360.0F))
    {
        DObjGetStruct(fighter_gobj)->rotate.vec.f.y -= F_CST_DTOR32(360.0F);
    }
}

// 0x80131E3C
void sc1PChallengerMakeFighter(s32 fkind)
{
    GObj *fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = fkind;
    desc.costume = ftParamGetCostumeCommonID(fkind, 0);
    desc.player = 0;
    desc.figatree_heap = sSC1PChallengerFigatreeHeap;

    fighter_gobj = ftManagerMakeFighter(&desc);

    gcAddGObjProcess(fighter_gobj, sc1PChallengerFighterProcUpdate, nGCProcessKindFunc, 1);

    DObjGetStruct(fighter_gobj)->translate.vec.f.x = 610.0F;
    DObjGetStruct(fighter_gobj)->translate.vec.f.y = -550.0F;
    DObjGetStruct(fighter_gobj)->translate.vec.f.z = 0.0F;

    DObjGetStruct(fighter_gobj)->scale.vec.f.x = dSCSubsysFighterScales[fkind];
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = dSCSubsysFighterScales[fkind];
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = dSCSubsysFighterScales[fkind];

    /* gm/gmcolscripts.c:1021 dGMColScriptsFighterChallenger, the
     * 60-tic script that holds the silhouette black. That file compiles
     * unmodified and is in the link already, so this needs nothing. */
    ftParamCheckSetFighterColAnimID(fighter_gobj, nGMColAnimFighterChallenger, 0);
}

// 0x80131F58
void sc1PChallengerMakeFighterCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            1,
            NULL,
            1,
            GOBJ_PRIORITY_DEFAULT,
            func_80017EC0,
            40,
            COBJ_MASK_DLLINK(18) | COBJ_MASK_DLLINK(15) |
            COBJ_MASK_DLLINK(10) | COBJ_MASK_DLLINK(9),
            -1,
            TRUE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->vec.eye.x = 0.0F;
    cobj->vec.eye.y = 0.0F;
    cobj->vec.eye.z = 3000.0F;

    cobj->vec.at.x = 0.0F;
    cobj->vec.at.y = 0.0F;
    cobj->vec.at.z = 0.0F;

    cobj->vec.up.x = 0.0F;
    cobj->vec.up.y = 1.0F;
    cobj->vec.up.z = 0.0F;
}

// 0x80132040
void sc1PChallengerMakeDecalsCamera(void)
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

// 0x801320E0
void sc1PChallengerInitVars(void)
{
    sSC1PChallengerFighterKind = gSCManagerSceneData.challenger_fkind;

    sSC1PChallengerUnk0x80132490 = 0.0F;

    sSC1PChallengerUnk0x80132494 = 0;
    sSC1PChallengerTotalTimeTics = 0;
}

// 0x80132110 - unused?
void func_ovl23_80132110(void)
{
    return;
}

// 0x80132118
void sc1PChallengerFuncRun(GObj *gobj)
{
    sSC1PChallengerTotalTimeTics++;

    if (sSC1PChallengerTotalTimeTics >= 120)
    {
        if (sSC1PChallengerUnk0x80132494 != 0)
        {
            sSC1PChallengerUnk0x80132494--;
        }
        if ((scSubsysControllerGetPlayerStickInRangeLR(-30, 30) != FALSE) && (scSubsysControllerGetPlayerStickInRangeUD(-30, 30) != FALSE))
        {
            sSC1PChallengerUnk0x80132494 = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            func_ovl23_80132110();
            syTaskmanSetLoadScene();
        }
    }
}

/* DIVERGES. sc1pchallenger.c:340-353 is the reloc loader's setup and its
 * load of the one file; the port's file is a bank. Split out of
 * FuncStart under the name the other ported sc1pmode/ scenes use
 * (src/dc/sc1pintro.c sc1PIntroLoadFiles).
 *
 * A bank that will not load leaves its slot NULL, which is what the
 * game's own load-failure path leaves: the four decals are then absent
 * rather than fatal, and the loader has already named the missing
 * file. */
void sc1PChallengerLoadFiles(void)
{
    static const char *const paths[ARRAY_COUNT(dSC1PChallengerFileIDs)] =
    {
        SC1PCHALLENGER_BANK_MAIN
    };
    s32 i;

    for (i = 0; i < ARRAY_COUNT(dSC1PChallengerFileIDs); i++)
    {
        sSC1PChallengerFiles[i] = NULL;

        if (sprite_bank_load(&sSC1PChallengerBanks[i], paths[i]) < 0)
        {
            syDebugPrintf("sc1PChallenger: no bank for file %d (%s)\n",
                          (int)dSC1PChallengerFileIDs[i], paths[i]);
            continue;
        }
        sSC1PChallengerFiles[i] = &sSC1PChallengerBanks[i];
    }
}

// 0x801321C0
void sc1PChallengerFuncStart(void)
{
    sc1PChallengerLoadFiles();
    gcMakeGObjSPAfter(0, sc1PChallengerFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
    /* CUT: gcMakeDefaultCameraGObj, the black clear camera -- the PVR
     * clears its own framebuffer, and a COBJ_FLAG_FILLCOLOR camera whose
     * mask covers a 3D list paints over it (src/dc/mnstartup.c's own
     * note spells the second half out; here it would have covered both
     * the decals and the fighter). */
    sc1PChallengerInitVars();
    efParticleInitAll();
    /* DIVERGES: efManagerInitEffects is the port's
     * efManagerLoadEffectBank (src/dc/efmanager.h), as in the battle and
     * on the card before every rung. */
    efManagerLoadEffectBank();
    ftManagerAllocFighter(FTDATA_FLAG_SUBMOTION, 1);
    ftManagerSetupFilesAllKind(sSC1PChallengerFighterKind);

    /* DIVERGES: gFTManagerFigatreeHeapSize is the ROM loader's, and the
     * port has no figatree heap -- a fighter's pack plays its animations
     * in place (src/dc/mnplayersvs.c says it first). The NULL is what
     * sc1PChallengerMakeFighter then passes as desc.figatree_heap. */
    sSC1PChallengerFigatreeHeap = NULL;

    sc1PChallengerMakeDecalsCamera();
    sc1PChallengerMakeFighterCamera();
    sc1PChallengerMakeDecals();
    sc1PChallengerMakeFighter(sSC1PChallengerFighterKind);

    syAudioPlayBGM(0, nSYAudioBGM1PChallenger);
    func_800269C0_275C0(nSYAudioFGMDeadUpStar);
}

/* CUT: dSC1PChallengerVideoSetup (0x801323B8), the N64's video mode --
 * sc1PChallengerStartScene says why. */

// 0x801323D4
SYTaskmanSetup dSC1PChallengerTaskmanSetup =
{
    // Task Manager Buffer Setup
    {
        0,                          // ???
        gcRunAll,              		// Update function
        scManagerFuncDraw,          // Frame draw function
        /* the decomp names &ovl23_BSS_END here; a NULL arena_start keeps
         * the region syTaskmanMakeGeneralHeap already made, which is
         * what every ported scene's setup carries (src/dc/taskman.c) */
        NULL,                       // Allocatable memory pool start
        0,                          // Allocatable memory pool size
        1,                          // ???
        2,                          // Number of contexts?
        0, 0, 0, 0,                 // the four DL buffer sizes
        0,                          // Graphics Heap Size
        2,                          // ???
        0,                          // RDP Output Buffer Size
        /* CUT: sc1PChallengerFuncLights. The decomp's pre-render function
         * pushes one gsSPSetLights1 onto the frame's display list, which
         * is an RDP-state call with no PVR counterpart. What lights the
         * silhouette instead is ftManagerAllocFighter's own default --
         * FTDATA_FLAG_SUBMOTION makes it call scSubsysFighterSetLightParams
         * (src/dc/ftmanager.c:881) -- and the colour script over the top
         * of it is what the card is actually showing. */
        NULL,                       // Pre-render function
        syControllerFuncRead,       // Controller I/O function
    },

    0,                              // Number of GObjThreads
    sizeof(u64) * 192,              // Thread stack size
    0,                              // Number of thread stacks
    0,                              // ???
    SC1PCHALLENGER_GOBJPROCS,
    SC1PCHALLENGER_GOBJS,   sizeof(GObj),
    SC1PCHALLENGER_XOBJS,
    /* the decomp names dLBCommonFuncMatrixList here; the port has no
     * such table (the matrix kinds are a switch in objdisplay.c), the
     * same NULL every other ported scene's setup carries */
    NULL,                           // Matrix function list
    NULL,                           // DObjVec eject function
    SC1PCHALLENGER_AOBJS,
    SC1PCHALLENGER_MOBJS,
    SC1PCHALLENGER_DOBJS,   sizeof(DObj),
    SC1PCHALLENGER_SOBJS,   sizeof(SObj),
    SC1PCHALLENGER_COBJS,   sizeof(CObj),

    sc1PChallengerFuncStart         // Task start function
};

// 0x80132310
void sc1PChallengerStartScene(void)
{
    /* CUT: syVideoInit and the z-buffer it is handed. The port's video
     * mode is the PVR's, set once at boot; and the arena line below it
     * reads a link map the ELF does not have -- dSC1PChallengerTaskmanSetup
     * carries NULL for arena_start instead. Both cuts are
     * src/dc/scvsbattle.c's, for its reasons. */
    scManagerFuncUpdate(&dSC1PChallengerTaskmanSetup);
}

/* The port's own bzero arm for dSCManagerOverlays[23] (src/dc/overlay.c).
 * The bank's records are syTaskmanMalloc'd out of the scene heap and go
 * with it, so the SpriteBank static has to be cleared or the next visit
 * thinks it is still loaded. */
void sc1PChallengerOverlayLoad(void)
{
    OVERLAY_CLEAR(sSC1PChallengerBanks);
    OVERLAY_CLEAR(sSC1PChallengerFiles);
    OVERLAY_CLEAR(sSC1PChallengerFighterKind);
    OVERLAY_CLEAR(sSC1PChallengerFigatreeHeap);
    OVERLAY_CLEAR(sSC1PChallengerUnk0x80132490);
    OVERLAY_CLEAR(sSC1PChallengerUnk0x80132494);
    OVERLAY_CLEAR(sSC1PChallengerTotalTimeTics);
    OVERLAY_CLEAR(sSC1PChallengerPad0x80132480);
    OVERLAY_CLEAR(sSC1PChallengerPad0x8013249C);
}
