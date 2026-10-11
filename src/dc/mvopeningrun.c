/* mvopeningrun.c -- see mvopeningrun.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningrun.c; the substitutions are
 * explained once, in mvopeningrun.h's own header comment -- not repeated
 * at every site.
 */
#include "mvopeningrun.h"
#include "overlay.h"

#include "camanim.h"    /* the eight fighter scripts + the camera's */
#include "fighter.h"
#include "objmodel.h"    /* dc_model_add_dobjs, dc_model_add_mobjs, the crash */
#include "ftcommon.h"
#include "lbcommon.h"   /* lbCommonDrawSprite, lbCommonDrawSObjAttr */
#include "scmanager.h"
#include "sprite.h"
#include "efmanager.h"
#include "taskman.h"

#include <ef/efparticle.h>
#include <ft/fighter.h>
#include <ft/ftdef.h>
#include <ft/ftparam.h>
#include <gm/gmsound.h>
#include <sc/scsubsys/scsubsys.h>  /* scSubsysFighterSetStatus/SetLightParams */
#include <sys/controller.h>
#include <sys/objanim.h>           /* gcAddDObjAnimJoint, gcAddCObjCamAnimJoint */
#include <sys/objdef.h>
#include <sys/rdp.h>
#include <lb/lbdef.h>

/* The game reaches the wallpaper as file 55's base plus its link label's
 * offset; the port's file is a sprite bank and the label's value is the
 * offset (mvopeningroom.c does the same for its own wallpaper). */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* relocData file 55's one Sprite: the value llMVOpeningRunWallpaperSprite
 * carries (src/dc/decomp/reloc_data.us.h, // 0x58a0). */
#define llMVOpeningRunWallpaperSprite 0x58A0

/* ---- the pools --------------------------------------------------------
 *
 * The decomp's own counts (128/128/256/800/160/256/128/16) run out here:
 * the port gives every fighter joint its own XObj/DObj set (objmodel.c
 * dc_model_add_dobjs), and eight fighters at once are ~230 joints -- the
 * pool overflows ("ml : alloc overflow" from sys/malloc.c) and the
 * scene hangs before its first frame on the target. Sized for eight
 * fighters, their eight proxies, the wallpaper and the cameras. */
#define MVOPENINGRUN_GOBJS       192
#define MVOPENINGRUN_GOBJPROCS   192
#define MVOPENINGRUN_XOBJS       768
#define MVOPENINGRUN_AOBJS      1200
#define MVOPENINGRUN_MOBJS       240
#define MVOPENINGRUN_DOBJS       384
#define MVOPENINGRUN_SOBJS       128
#define MVOPENINGRUN_COBJS        16

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static GObj *sMVOpeningRunLinkFighterGObj;
static s32 sMVOpeningRunTotalTimeTics;
static s32 sMVOpeningRunUnused0x80132740;

/* The port's stand-in for sMVOpeningRunFiles[0] (file 55): its one sprite,
 * the wallpaper, as romdisk/mvopeningrun.spr. Not static so hosttest_ft.c
 * can read it. */
static SpriteBank sMVOpeningRunWallpaperBank;
void *sMVOpeningRunWallpaperFileHead;

/* The port's stand-in for the other two files' scripts: the eight
 * fighters' and the camera's, romdisk/mvopeningrun.cam. */
static CamAnimBank sMVOpeningRunCamAnimBank;

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningrun.c:80131B00 mvOpeningRunFuncLights. Not ported -- the
 * standing rule (mvopeningroom.h/mvopeningportraits.h). func_lights is
 * NULL below. */

/* mvopeningrun.c:80131B58 mvOpeningRunFighterProcUpdate, verbatim. */
void mvOpeningRunFighterProcUpdate(GObj *fighter_proxy_gobj)
{
    GObj *fighter_gobj = (GObj*) fighter_proxy_gobj->user_data.p;

    gcPlayAnimAll(fighter_proxy_gobj);

    DObjGetStruct(fighter_gobj)->translate.vec.f.x = DObjGetStruct(fighter_proxy_gobj)->translate.vec.f.x;
    DObjGetStruct(fighter_gobj)->translate.vec.f.y = DObjGetStruct(fighter_proxy_gobj)->translate.vec.f.y;
    DObjGetStruct(fighter_gobj)->translate.vec.f.z = DObjGetStruct(fighter_proxy_gobj)->translate.vec.f.z;

    DObjGetStruct(fighter_gobj)->rotate.vec.f.x = DObjGetStruct(fighter_proxy_gobj)->rotate.vec.f.x;
    DObjGetStruct(fighter_gobj)->rotate.vec.f.y = DObjGetStruct(fighter_proxy_gobj)->rotate.vec.f.y;
    DObjGetStruct(fighter_gobj)->rotate.vec.f.z = DObjGetStruct(fighter_proxy_gobj)->rotate.vec.f.z;
}

/* mvopeningrun.c:80131BE8 mvOpeningRunMakeFighters. DIVERGES: the
 * AnimJoint offsets become bank names (camanim_get), the attach is
 * `#ifdef FT_HOSTTEST`-guarded, and desc.figatree_heap stays NULL
 * (mvopeningmario.h's Substitution 4). */
void mvOpeningRunMakeFighters(void)
{
    GObj *fighter_gobj;
    GObj *fighter_proxy_gobj;
    DObj *fighter_proxy_dobj;
    s32 i;

    s32 fkinds[] =
    {
        nFTKindMario,
        nFTKindFox,
        nFTKindDonkey,
        nFTKindSamus,
        nFTKindLink,
        nFTKindYoshi,
        nFTKindKirby,
        nFTKindPikachu
    };

    static const char *names[] =
    {
        "Mario", "Fox", "Donkey", "Samus", "Link", "Yoshi", "Kirby", "Pikachu"
    };

    for (i = 0; i < (s32)ARRAY_COUNT(fkinds); i++)
    {
        FTDesc desc = dFTManagerDefaultFighterDesc;

        desc.fkind = fkinds[i];
        desc.costume = ftParamGetCostumeCommonID(fkinds[i], 0);

        desc.pos.x = 0.0F;
        desc.pos.y = 0.0F;
        desc.pos.z = 0.0F;

        desc.figatree_heap = NULL;
        fighter_gobj = fighter_proxy_gobj = ftManagerMakeFighter(&desc);

        if (fkinds[i] == nFTKindLink)
        {
            sMVOpeningRunLinkFighterGObj = fighter_proxy_gobj;
        }
        scSubsysFighterSetStatus(fighter_proxy_gobj, 0x10006);

        fighter_proxy_gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
        fighter_proxy_gobj->user_data.p = fighter_gobj;

        fighter_proxy_dobj = gcAddDObjForGObj(fighter_proxy_gobj, NULL);

        gcAddXObjForDObjFixed(fighter_proxy_dobj, nGCMatrixKindTraRotRpyRSca, 0);
#ifdef FT_HOSTTEST
        /* the host cannot walk a script (spins in the parser, as
         * mvopeningmario.c's camera note describes), so the proxy stays
         * at its origin there */
#else
        gcAddDObjAnimJoint(fighter_proxy_dobj, camanim_get(&sMVOpeningRunCamAnimBank, names[i]), 0.0F);
#endif

        gcAddGObjProcess(fighter_proxy_gobj, mvOpeningRunFighterProcUpdate, nGCProcessKindFunc, 1);
        gcPlayAnimAll(fighter_proxy_gobj);
    }
}

/* mvopeningrun.c:80131E28 mvOpeningRunWallpaperProcUpdate, verbatim. */
void mvOpeningRunWallpaperProcUpdate(GObj *gobj)
{
    SObj *sobj = SObjGetStruct(gobj);

    sobj->pos.x += 30.0F;

    if (sobj->pos.x > 0.0F)
    {
        sobj->pos.x += -320.0F;
    }
    sobj->next->pos.x = sobj->pos.x + 320.0F;
}

/* mvopeningrun.c:80131E88 mvOpeningRunMakeWallpaper, verbatim but for
 * lbRelocGetFileData's own redefinition above. */
void mvOpeningRunMakeWallpaper(void)
{
    GObj *gobj;
    SObj *left_sobj;
    SObj *right_sobj;

    gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 28, GOBJ_PRIORITY_DEFAULT, ~0);

    left_sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningRunWallpaperFileHead, llMVOpeningRunWallpaperSprite));

    left_sobj->sprite.attr &= ~SP_FASTCOPY;

    left_sobj->sprite.scalex = 2.0F;
    left_sobj->sprite.scaley = 2.0F;

    left_sobj->pos.x = -320.0F;
    left_sobj->pos.y = 0.0F;

    right_sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sMVOpeningRunWallpaperFileHead, llMVOpeningRunWallpaperSprite));

    right_sobj->sprite.attr &= ~SP_FASTCOPY;

    right_sobj->sprite.scalex = 2.0F;
    right_sobj->sprite.scaley = 2.0F;

    right_sobj->pos.x = 0.0F;
    right_sobj->pos.y = 0.0F;

    gcAddGObjProcess(gobj, mvOpeningRunWallpaperProcUpdate, nGCProcessKindFunc, 1);
}

/* The crash's pack (romdisk/mvopeningruncrash.mdl, file 75's five-joint
 * tree, tools/export/ssb_effectexport.py `--what runcrash`), loaded when the
 * crash is made -- tic 190 -- and given back in OverlayLoad. */
#ifndef FT_HOSTTEST
static Fighter sMVOpeningRunCrashPack;
static Fighter sMVOpeningRunCrashModel;
static sb32 sMVOpeningRunCrashPackLoaded;
#endif

/* mvopeningrun.c:80131F80 mvOpeningRunMakeCrash. DIVERGES: the tree and
 * its four MObjs come from the pack (dc_model_add_dobjs,
 * dc_model_add_mobjs -- gcSetupCommonDObjs, gcAddMObjAll and
 * gcAddMatAnimJointAll), the display proc is dc_model_proc_display, and
 * the pack build is FT_HOSTTEST-guarded (the host links no renderer and
 * cannot walk a script). */
void mvOpeningRunMakeCrash(void)
{
    GObj *gobj = gcMakeGObjSPAfter(0, NULL, 19, GOBJ_PRIORITY_DEFAULT);
    sb32 is_built = FALSE;

#ifndef FT_HOSTTEST
    {
        int pal_bank = 0;

        if (fighter_load_scene(&sMVOpeningRunCrashPack, "mvopeningruncrash.mdl", &pal_bank) == 0)
        {
            sMVOpeningRunCrashPackLoaded = TRUE;

            fighter_clone(&sMVOpeningRunCrashModel, &sMVOpeningRunCrashPack,
                          syTaskmanMalloc(sizeof(float[4]) *
                                          sMVOpeningRunCrashPack.hd->vert_count, 0x8));
            dc_model_add_dobjs(gobj, NULL, &sMVOpeningRunCrashModel, NULL);
            is_built = TRUE;
        }
    }
#endif
    if (is_built == FALSE)
    {
        gcAddDObjRpyR(gobj, NULL);
    }
    gcAddGObjDisplay(gobj, dc_model_proc_display, 6, GOBJ_PRIORITY_DEFAULT, ~0);

    DObjGetStruct(gobj)->translate.vec.f.x = 960.0F;
    DObjGetStruct(gobj)->translate.vec.f.y = 360.0F;
    DObjGetStruct(gobj)->translate.vec.f.z = -13.5F;

    DObjGetStruct(gobj)->rotate.vec.f.y = F_CST_DTOR32(90.0F);

    DObjGetStruct(gobj)->scale.vec.f.x = 0.9F;
    DObjGetStruct(gobj)->scale.vec.f.y = 0.9F;
    DObjGetStruct(gobj)->scale.vec.f.z = 0.9F;

#ifndef FT_HOSTTEST
    if (is_built != FALSE)
    {
        dc_model_add_mobjs(gobj, &sMVOpeningRunCrashModel, 0.0F);
        gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
        gcPlayAnimAll(gobj);
    }
#endif
}

/* mvopeningrun.c:801320B4 mvOpeningRunInitMainCamera. DIVERGES: the
 * script is camanim_get(..., "MainCam"), and the attach-and-play pair is
 * `#ifdef FT_HOSTTEST`-guarded like every scene in this chain. */
void mvOpeningRunInitMainCamera(GObj *camera_gobj)
{
    CObj *cobj = CObjGetStruct(camera_gobj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
#ifdef FT_HOSTTEST
    /* the host cannot walk a script; see mvopeningmario.c's own note */
#else
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningRunCamAnimBank, "MainCam"), 0.0F);
    gcAddGObjProcess(camera_gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
#endif
}

/* mvopeningrun.c:80132138 mvOpeningRunMakeMainCamera, verbatim. */
void mvOpeningRunMakeMainCamera(void)
{
    mvOpeningRunInitMainCamera
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            nGCCommonLinkIDSceneCamera,
            GOBJ_PRIORITY_DEFAULT,
            func_80017EC0,
            60,
            COBJ_MASK_DLLINK(18) | COBJ_MASK_DLLINK(15) |
            COBJ_MASK_DLLINK(10) | COBJ_MASK_DLLINK(9)  |
            COBJ_MASK_DLLINK(6),
            -1,
            TRUE,
            nGCProcessKindFunc,
            NULL,
            1,
            FALSE
        )
    );
}

/* mvopeningrun.c:801321BC mvOpeningRunMakeWallpaperCamera. DIVERGES:
 * lbCommonDrawSpriteBackdrop for lbCommonDrawSprite -- this camera's 80
 * puts the wallpaper under the fighters' camera (60) on the N64, and
 * the backdrop band is what puts it under their opaque props here
 * (src/dc/lbcommon.h). */
void mvOpeningRunMakeWallpaperCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            nGCCommonLinkIDSceneCamera,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSpriteBackdrop,
            80,
            COBJ_MASK_DLLINK(28),
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

/* mvopeningrun.c:8013225C mvOpeningRunInitVars, verbatim. */
void mvOpeningRunInitVars(void)
{
    sMVOpeningRunTotalTimeTics = 0;
}

/* mvopeningrun.c:80132268 mvOpeningRunFuncRun, verbatim. */
void mvOpeningRunFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningRunTotalTimeTics++;

    if (sMVOpeningRunTotalTimeTics >= 10)
    {
        if (sMVOpeningRunUnused0x80132740 != 0)
        {
            sMVOpeningRunUnused0x80132740--;
        }
        if ((scSubsysControllerGetPlayerStickInRangeLR(-15, 15) != FALSE) && (scSubsysControllerGetPlayerStickInRangeUD(-15, 15) != FALSE))
        {
            sMVOpeningRunUnused0x80132740 = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            syTaskmanSetLoadScene();
        }
        if (sMVOpeningRunTotalTimeTics == 45)
        {
            scSubsysFighterSetStatus(sMVOpeningRunLinkFighterGObj, 0x10007);
        }
        if (sMVOpeningRunTotalTimeTics == 190)
        {
            mvOpeningRunMakeCrash();
            func_800269C0_275C0(nSYAudioFGMExplodeL);
        }
        if (sMVOpeningRunTotalTimeTics == 220)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindOpeningCliff;

            syTaskmanSetLoadScene();
        }
    }
}

/* mvopeningrun.c:8013237C mvOpeningRunFuncStart. DIVERGES: the reloc
 * loader is the two bank loads (mvopeningrun.h), efManagerInitEffects is
 * mvopeningmario.h's Substitution 3b and the eight figatree-heap mallocs are
 * cut (Substitution 4). The trailing tic-sync busy-wait is
 * sySchedulerWaitTicCount. */
void mvOpeningRunFuncStart(void)
{
    if (sprite_bank_load(&sMVOpeningRunWallpaperBank, "mvopeningrun.spr") < 0)
    {
        sMVOpeningRunWallpaperFileHead = NULL;
    }
    else
    {
        sMVOpeningRunWallpaperFileHead = &sMVOpeningRunWallpaperBank;
    }
    camanim_bank_load(&sMVOpeningRunCamAnimBank, "mvopeningrun.cam");

    gcMakeGObjSPAfter(0, mvOpeningRunFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
    gcMakeDefaultCameraGObj(0, GOBJ_PRIORITY_DEFAULT, 100, COBJ_FLAG_ZBUFFER, GPACK_RGBA8888(0x00, 0x00, 0x00, 0x00));

    efParticleInitAll();
    mvOpeningRunInitVars();

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

    mvOpeningRunMakeMainCamera();
    mvOpeningRunMakeWallpaperCamera();
    mvOpeningRunMakeWallpaper();
    mvOpeningRunMakeFighters();

    scSubsysFighterSetLightParams(45.0F, 10.0F, 0xFF, 0xFF, 0xFF, 0xFF);

    /* DIVERGES (port only): dc_model_set_demo_opaque(TRUE) -- one camera
     * (60) covers the wreck and all eight fighters, so each takes the
     * opaque draw at its real depth against it, as the N64's shared
     * Z buffer does, instead of the layered band the per-fighter panel
     * scenes need. See [[room-logo-layering]] / mvopeningroom.c. */
    dc_model_set_demo_opaque(TRUE);
    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 2250 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(2250);
}

/* mvopeningrun.c:80132650-shaped dMVOpeningRunTaskmanSetup. See the pools
 * note above for the counts, and mvopeningportraits.h for func_draw
 * (scManagerFuncDraw), the zeroed RSP/RDP fields and the pre-render/
 * controller/matrix-list substitutions. */
SYTaskmanSetup dMVOpeningRunTaskmanSetup =
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
    MVOPENINGRUN_GOBJPROCS,
    MVOPENINGRUN_GOBJS,
    sizeof(GObj),
    MVOPENINGRUN_XOBJS,
    NULL,
    NULL,
    MVOPENINGRUN_AOBJS,
    MVOPENINGRUN_MOBJS,
    MVOPENINGRUN_DOBJS,
    sizeof(DObj),
    MVOPENINGRUN_SOBJS,
    sizeof(SObj),
    MVOPENINGRUN_COBJS,
    sizeof(CObj),

    mvOpeningRunFuncStart
};

/* mvopeningrun.c:8013256C mvOpeningRunStartScene. DIVERGES: the standard
 * video/arena cuts (mvopeningrun.h). */
void mvOpeningRunStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningRunTaskmanSetup);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[44]. */
void mvOpeningRunOverlayLoad(void)
{
#ifndef FT_HOSTTEST
    if (sMVOpeningRunCrashPackLoaded != FALSE)
    {
        fighter_release(&sMVOpeningRunCrashPack);
    }
    OVERLAY_CLEAR(sMVOpeningRunCrashPack);
    OVERLAY_CLEAR(sMVOpeningRunCrashModel);
    OVERLAY_CLEAR(sMVOpeningRunCrashPackLoaded);
#endif
    OVERLAY_CLEAR(sMVOpeningRunLinkFighterGObj);
    OVERLAY_CLEAR(sMVOpeningRunTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningRunUnused0x80132740);
    OVERLAY_CLEAR(sMVOpeningRunWallpaperBank);
    OVERLAY_CLEAR(sMVOpeningRunWallpaperFileHead);
    OVERLAY_CLEAR(sMVOpeningRunCamAnimBank);
}
