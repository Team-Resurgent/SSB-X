/* mvopeningclash.c -- see mvopeningclash.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningclash.c; the substitutions are
 * explained once, in mvopeningclash.h's own header comment -- not repeated
 * at every site.
 */
#include "mvopeningclash.h"
#include "overlay.h"

#include "camanim.h"    /* the cameras' scripts */
#include "fighter.h"
#include "objmodel.h"    /* dc_model_add_dobjs, dc_model_proc_display */
#include "ftcommon.h"
#include "lbcommon.h"   /* lbCommonDrawSprite, lbCommonSpriteFillRect */
#include "objpvr.h"     /* gcGetDrawList */
#include "scmanager.h"
#include "efmanager.h"
#include "taskman.h"

#include <ef/efparticle.h>
#include <ft/fighter.h>
#include <ft/ftdef.h>
#include <ft/ftparam.h>
#include <gm/gmsound.h>
#include <sc/scsubsys/scsubsys.h>  /* scSubsysFighterSetStatus/SetLightParams */
#include <sys/controller.h>
#include <sys/objanim.h>           /* gcAddAnimJointAll, gcAddCObjCamAnimJoint */
#include <sys/objdef.h>
#include <sys/rdp.h>
#include <lb/lbdef.h>

/* ---- the pools --------------------------------------------------------
 *
 * Eight fighters at once plus four wallpaper quadrants: Run's counts
 * (mvopeningrun.c), the port's one-XObj-set-per-joint model being short
 * of the decomp's 512s. */
#define MVOPENINGCLASH_GOBJS       192
#define MVOPENINGCLASH_GOBJPROCS   192
#define MVOPENINGCLASH_XOBJS       768
#define MVOPENINGCLASH_AOBJS      1200
#define MVOPENINGCLASH_MOBJS       240
#define MVOPENINGCLASH_DOBJS       384
#define MVOPENINGCLASH_SOBJS        16
#define MVOPENINGCLASH_COBJS        16

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static s32 sMVOpeningClashTotalTimeTics;
static s32 sMVOpeningClashVoidAlpha;
static s32 sMVOpeningClashUnused0x80132A10;

/* The port's stand-in for the cameras' scripts in files 72 and 66. */
static CamAnimBank sMVOpeningClashCamAnimBank;

/* The four wallpaper quadrants as packs (lower left, lower right, upper
 * left, upper right -- the order the game makes them in), their
 * instances, and each one's table of per-joint scripts rebuilt from its
 * pack (ifcommon.c's arrows do the same: gcAddAnimJointAll wants the
 * pointers the file held). */
#define MVOPENINGCLASH_QUADRANTS 4

#ifndef FT_HOSTTEST
static const char *const sMVOpeningClashModelNames[MVOPENINGCLASH_QUADRANTS] =
{
    "mvopeningclashwallll.mdl",
    "mvopeningclashwalllr.mdl",
    "mvopeningclashwallul.mdl",
    "mvopeningclashwallur.mdl"
};
#endif

static Fighter sMVOpeningClashPacks[MVOPENINGCLASH_QUADRANTS];
static Fighter sMVOpeningClashModels[MVOPENINGCLASH_QUADRANTS];
static sb32 sMVOpeningClashModelsLoaded;
static AObjEvent32 *sMVOpeningClashAnimJoints[MVOPENINGCLASH_QUADRANTS][FIGHTER_MAX_JOINTS];

#ifndef FT_HOSTTEST
static void mvOpeningClashSetAnimJoint(s32 id)
{
    const Fighter *f = &sMVOpeningClashPacks[id];
    AObjEvent32 **table = sMVOpeningClashAnimJoints[id];
    const FPackAnim *anim;
    const s32 *entries;
    u32 i;

    for (i = 0; i < FIGHTER_MAX_JOINTS; i++)
    {
        table[i] = NULL;
    }
    if (f->hd->anim_count == 0)
    {
        return;
    }
    anim = &f->anims[0];
    entries = (const s32 *)((const u8 *)f->blob + anim->off_entries);

    for (i = 0; i < f->hd->joint_count && i < FIGHTER_MAX_JOINTS; i++)
    {
        if (entries[i] >= 0 && (u32) entries[i] < anim->nwords)
        {
            table[i] = (AObjEvent32 *)((u8 *)f->blob + anim->off_words) + entries[i];
        }
    }
}
#endif

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningclash.c:80131B00 mvOpeningClashFuncLights. Not ported -- the
 * standing rule. func_lights is NULL below. */

/* mvopeningclash.c:80131B60 mvOpeningClashMakeFighters, verbatim but for
 * the figatree heaps (mvopeningmario.h's Substitution 4, NULL). */
void mvOpeningClashMakeFighters(void)
{
    GObj *fighter_gobj;
    s32 fkinds[/* */] =
    {
        nFTKindMario,
        nFTKindKirby,
        nFTKindLink,
        nFTKindYoshi,
        nFTKindFox,
        nFTKindDonkey,
        nFTKindSamus,
        nFTKindPikachu
    };
    s32 i;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    for (i = 0; i < (s32)ARRAY_COUNT(fkinds); i++)
    {
        desc.fkind = fkinds[i];
        desc.costume = ftParamGetCostumeCommonID(fkinds[i], 0);

        desc.pos.x = 0.0F;
        desc.pos.y = 0.0F;
        desc.pos.z = 0.0F;

        desc.figatree_heap = NULL;
        fighter_gobj = ftManagerMakeFighter(&desc);

        scSubsysFighterSetStatus(fighter_gobj, nFTDemoStatusClash);

        DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
        DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
        DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;
    }
}

/* mvopeningclash.c:80131CCC mvOpeningClashVoidProcDisplay. DIVERGES: the
 * white-out is a gDPFillRectangle under G_CC_PRIMITIVE, and the raw GBI
 * lands in the write-only scratch heads (src/dc/wpmanager.c), so it is
 * the fill quad src/dc/ifscreenflash.c draws instead, translucent pass
 * only. The alpha ramp runs once a frame, as the N64's one-pass draw
 * had it. Its camera (40) is walked after the fighters' (60), so the
 * quad takes the later sprite depth and covers the layered fighters. */
void mvOpeningClashVoidProcDisplay(GObj *gobj)
{
    (void)gobj;

    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    if (sMVOpeningClashVoidAlpha < 0xFF)
    {
        sMVOpeningClashVoidAlpha += 0x1E;

        if (sMVOpeningClashVoidAlpha > 0xFF)
        {
            sMVOpeningClashVoidAlpha = 0xFF;
        }
    }
    lbCommonSpriteFillRect(10, 10, 310, 230, 0xFF, 0xFF, 0xFF, sMVOpeningClashVoidAlpha);
}

/* mvopeningclash.c:80131E08 mvOpeningClashMakeVoid, verbatim. */
void mvOpeningClashMakeVoid(void)
{
    sMVOpeningClashVoidAlpha = 0;
    gcAddGObjDisplay
    (
        gcMakeGObjSPAfter
        (
            0,
            NULL,
            18,
            GOBJ_PRIORITY_DEFAULT
        ),
        mvOpeningClashVoidProcDisplay,
        26,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
}

/* mvopeningclash.c:80131E5C mvOpeningClashMakeWallpaper. The game builds
 * each quadrant by hand (gcAddDObjForGObj, gcAddMObjAll,
 * gcAddMatAnimJointAll, gcAddAnimJointAll, gcPlayAnimAll) four times
 * over; they differ in nothing but the file's symbols, so this is one
 * loop over the four packs.
 * DIVERGES: the tree and MObjs come from the pack (dc_model_add_dobjs,
 * dc_model_add_mobjs), the table from its rebuilt pointers, the display
 * proc is dc_model_proc_display, and the pack build and the attach are
 * FT_HOSTTEST-guarded (the host links no renderer and cannot walk a
 * script). */
void mvOpeningClashMakeWallpaper(void)
{
    s32 i;

    for (i = 0; i < MVOPENINGCLASH_QUADRANTS; i++)
    {
        GObj *gobj = gcMakeGObjSPAfter(0, NULL, 20, GOBJ_PRIORITY_DEFAULT);

#ifndef FT_HOSTTEST
        if (sMVOpeningClashModelsLoaded != FALSE)
        {
            fighter_clone(&sMVOpeningClashModels[i], &sMVOpeningClashPacks[i],
                          syTaskmanMalloc(sizeof(float[4]) *
                                          sMVOpeningClashPacks[i].hd->vert_count, 0x8));
            dc_model_add_dobjs(gobj, NULL, &sMVOpeningClashModels[i], NULL);
            dc_model_add_mobjs(gobj, &sMVOpeningClashModels[i], 0.0F);
        }
#else
        gcAddDObjRpyR(gobj, NULL);
#endif
        gcAddGObjDisplay(gobj, dc_model_proc_display, 29, GOBJ_PRIORITY_DEFAULT, ~0);

        DObjGetStruct(gobj)->translate.vec.f.x = 0.0F;
        DObjGetStruct(gobj)->translate.vec.f.y = 0.0F;
        DObjGetStruct(gobj)->translate.vec.f.z = 0.0F;

#ifndef FT_HOSTTEST
        if (sMVOpeningClashModelsLoaded != FALSE)
        {
            gcAddAnimJointAll(gobj, sMVOpeningClashAnimJoints[i], 0.0F);
            gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
        }
#endif
    }
}

/* mvopeningclash.c:80132204 mvOpeningClashMakeFightersCamera. DIVERGES:
 * the script is camanim_get(..., "Fighters") and the attach-and-play pair
 * is FT_HOSTTEST-guarded. */
void mvOpeningClashMakeFightersCamera(void)
{
    GObj *camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        60,
        COBJ_MASK_DLLINK(27) |
        COBJ_MASK_DLLINK(9),
        -1,
        TRUE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(camera_gobj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->flags |= COBJ_FLAG_ZBUFFER;

    cobj->projection.persp.near = 128.0F;
    cobj->projection.persp.far = 16384.0F;

#ifndef FT_HOSTTEST
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningClashCamAnimBank, "Fighters"), 0.0F);
    gcAddGObjProcess(camera_gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
    gcPlayCamAnim(camera_gobj);
#endif
}

/* mvopeningclash.c:80132314 mvOpeningClashMakeVoidCamera, verbatim. */
void mvOpeningClashMakeVoidCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSprite,
            40,
            COBJ_MASK_DLLINK(26),
            ~0,
            FALSE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->flags |= COBJ_FLAG_ZBUFFER;
}

/* mvopeningclash.c:801323C8 mvOpeningClashWallpaperProcDisplay, verbatim. */
void mvOpeningClashWallpaperProcDisplay(GObj *gobj)
{
    gDPPipeSync(gSYTaskmanDLHeads[0]++);
    gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);

    func_80017DBC(gobj);

    gDPPipeSync(gSYTaskmanDLHeads[0]++);
    gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
}

/* mvopeningclash.c:8013246C mvOpeningClashMakeWallpaperCamera. DIVERGES:
 * the script is camanim_get(..., "Wall"). */
void mvOpeningClashMakeWallpaperCamera(void)
{
    GObj *camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        mvOpeningClashWallpaperProcDisplay,
        90,
        COBJ_MASK_DLLINK(29),
        -1,
        TRUE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    CObj *cobj = CObjGetStruct(camera_gobj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

#ifndef FT_HOSTTEST
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningClashCamAnimBank, "Wall"), 0.0F);
    gcAddGObjProcess(camera_gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
#endif
}

/* mvopeningclash.c:80132550 mvOpeningClashInitTotalTimeTics, verbatim. */
void mvOpeningClashInitTotalTimeTics(void)
{
    sMVOpeningClashTotalTimeTics = 0;
}

/* mvopeningclash.c:8013255C mvOpeningClashFuncRun, verbatim. */
void mvOpeningClashFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningClashTotalTimeTics++;

    if (sMVOpeningClashTotalTimeTics >= 10)
    {
        if (sMVOpeningClashUnused0x80132A10 != 0)
        {
            sMVOpeningClashUnused0x80132A10--;
        }
        if ((scSubsysControllerGetPlayerStickInRangeLR(-15, 15) != FALSE) && (scSubsysControllerGetPlayerStickInRangeUD(-15, 15) != FALSE))
        {
            sMVOpeningClashUnused0x80132A10 = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            syTaskmanSetLoadScene();
        }

        if (sMVOpeningClashTotalTimeTics == 144)
        {
            mvOpeningClashMakeVoid();
        }
        if (sMVOpeningClashTotalTimeTics == 160)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindOpeningNewcomers;

            syTaskmanSetLoadScene();
        }
        if
        (
            (sMVOpeningClashTotalTimeTics == 15) ||
            (sMVOpeningClashTotalTimeTics == 75) ||
            (sMVOpeningClashTotalTimeTics == 90) ||
            (sMVOpeningClashTotalTimeTics == 105)
        )
        {
            func_800269C0_275C0(nSYAudioFGMOpeningClash);
        }
    }
}

/* mvopeningclash.c:8013267C mvOpeningClashFuncStart. DIVERGES: the reloc
 * loader is the pack loads and the cameras' bank (mvopeningclash.h),
 * efManagerInitEffects is Substitution 3b, the figatree-heap mallocs are
 * cut (4), the default camera's FILLCOLOR is dropped (fillcolor-camera
 * note: it paints over every 3D list here; the black clear is the same)
 * and so is the trailing tic-sync busy-wait (5). */
void mvOpeningClashFuncStart(void)
{
    camanim_bank_load(&sMVOpeningClashCamAnimBank, "mvopeningclash.cam");
#ifndef FT_HOSTTEST
    {
        int pal_bank = 0;
        s32 i;

        sMVOpeningClashModelsLoaded = TRUE;
        for (i = 0; i < MVOPENINGCLASH_QUADRANTS; i++)
        {
            if (fighter_load_scene(&sMVOpeningClashPacks[i], sMVOpeningClashModelNames[i], &pal_bank) != 0)
            {
                sMVOpeningClashModelsLoaded = FALSE;
                break;
            }
            mvOpeningClashSetAnimJoint(i);
        }
    }
#endif

    gcMakeGObjSPAfter(0, mvOpeningClashFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
    gcMakeDefaultCameraGObj(0, GOBJ_PRIORITY_DEFAULT, 100, COBJ_FLAG_ZBUFFER, GPACK_RGBA8888(0x00, 0x00, 0x00, 0x00));

    efParticleInitAll();
    mvOpeningClashInitTotalTimeTics();

    efManagerLoadEffectBank();
    efManagerPreloadModels();
    ftManagerAllocFighter(FTDATA_FLAG_SUBMOTION, 8);

    ftManagerSetupFilesAllKind(nFTKindMario);
    ftManagerSetupFilesAllKind(nFTKindFox);
    ftManagerSetupFilesAllKind(nFTKindDonkey);
    ftManagerSetupFilesAllKind(nFTKindSamus);
    ftManagerSetupFilesAllKind(nFTKindLink);
    ftManagerSetupFilesAllKind(nFTKindYoshi);
    ftManagerSetupFilesAllKind(nFTKindKirby);
    ftManagerSetupFilesAllKind(nFTKindPikachu);

    mvOpeningClashMakeFightersCamera();
    mvOpeningClashMakeVoidCamera();
    mvOpeningClashMakeWallpaperCamera();
    mvOpeningClashMakeFighters();
    mvOpeningClashMakeWallpaper();

    scSubsysFighterSetLightParams(45.0F, 45.0F, 0xFF, 0xFF, 0xFF, 0xFF);

    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 3975 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(3975);
}

/* mvopeningclash.c:80132944-shaped dMVOpeningClashTaskmanSetup. See
 * mvopeningportraits.h for func_draw (scManagerFuncDraw), the zeroed
 * RSP/RDP fields and the pre-render/controller/matrix-list
 * substitutions. */
SYTaskmanSetup dMVOpeningClashTaskmanSetup =
{
    {
        0,
        gcRunAll,
        scManagerFuncDraw,
        NULL,
        0,
        1,
        2,
        0,
        0,
        0,
        0,
        0,
        2,
        0,
        NULL,
        syControllerFuncRead,
    },

    0,
    sizeof(u64) * 192,
    0,
    0,
    MVOPENINGCLASH_GOBJPROCS,
    MVOPENINGCLASH_GOBJS,
    sizeof(GObj),
    MVOPENINGCLASH_XOBJS,
    NULL,
    NULL,
    MVOPENINGCLASH_AOBJS,
    MVOPENINGCLASH_MOBJS,
    MVOPENINGCLASH_DOBJS,
    sizeof(DObj),
    MVOPENINGCLASH_SOBJS,
    sizeof(SObj),
    MVOPENINGCLASH_COBJS,
    sizeof(CObj),

    mvOpeningClashFuncStart
};

/* mvopeningclash.c:80132874 mvOpeningClashStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeningclash.h). */
void mvOpeningClashStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningClashTaskmanSetup);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[49].
 * The packs are given back first (mvopeningcliff.c does the same). */
void mvOpeningClashOverlayLoad(void)
{
#ifndef FT_HOSTTEST
    s32 i;

    for (i = 0; i < MVOPENINGCLASH_QUADRANTS; i++)
    {
        fighter_release(&sMVOpeningClashPacks[i]);
    }
#endif
    OVERLAY_CLEAR(sMVOpeningClashTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningClashVoidAlpha);
    OVERLAY_CLEAR(sMVOpeningClashUnused0x80132A10);
    OVERLAY_CLEAR(sMVOpeningClashCamAnimBank);
    OVERLAY_CLEAR(sMVOpeningClashPacks);
    OVERLAY_CLEAR(sMVOpeningClashModels);
    OVERLAY_CLEAR(sMVOpeningClashModelsLoaded);
    OVERLAY_CLEAR(sMVOpeningClashAnimJoints);
}
