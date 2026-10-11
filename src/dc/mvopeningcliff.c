/* mvopeningcliff.c -- see mvopeningcliff.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningcliff.c; the substitutions are
 * explained once, in mvopeningcliff.h's own header comment -- not repeated
 * at every site.
 */
#include "mvopeningcliff.h"
#include "overlay.h"

#include "camanim.h"    /* the camera script */
#include "fighter.h"
#include "objmodel.h"    /* dc_model_add_dobjs, dc_model_proc_display */
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

/* The game reaches the wallpaper as file 70's base plus its link label's
 * offset; the port's file is a sprite bank and the label's value is the
 * offset (mvopeningrun.c does the same). */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* relocData file 70's one Sprite: llMVOpeningStandoffWallpaperSprite
 * (src/dc/decomp/reloc_data.us.h). */
#define llMVOpeningStandoffWallpaperSprite 0xB500

#define MVOPENINGCLIFF_MODEL_HILLS   "mvopeningcliffhills.mdl"
#define MVOPENINGCLIFF_MODEL_OCARINA "mvopeningcliffocarina.mdl"

/* ---- the pools -------------------------------------------------------- */
#define MVOPENINGCLIFF_GOBJS        96
#define MVOPENINGCLIFF_GOBJPROCS    96
#define MVOPENINGCLIFF_XOBJS       384
#define MVOPENINGCLIFF_AOBJS       800
#define MVOPENINGCLIFF_MOBJS       128
#define MVOPENINGCLIFF_DOBJS       192
#define MVOPENINGCLIFF_SOBJS        16
#define MVOPENINGCLIFF_COBJS         8

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static s32 sMVOpeningCliffTotalTimeTics;
static GObj *sMVOpeningCliffOcarinaGObj;
static GObj *sMVOpeningCliffFighterGObj;
static f32 sMVOpeningCliffWallpaperScrollSpeed;
static s32 sMVOpeningCliffUnused0x801327DC;

/* The port's stand-in for sMVOpeningCliffFiles[1] (file 70): its one
 * sprite, romdisk/mvopeningcliff.spr. Not static so hosttest_ft.c can
 * read it. */
static SpriteBank sMVOpeningCliffWallpaperBank;
void *sMVOpeningCliffWallpaperFileHead;

/* The port's stand-in for the camera script in file 68. */
static CamAnimBank sMVOpeningCliffCamAnimBank;

/* The two models of file 68, as packs, and the ocarina's table of
 * per-joint scripts rebuilt from its pack (ifcommon.c's arrows do the
 * same: gcAddAnimJointAll wants the pointers the file held). */
static Fighter sMVOpeningCliffHillsPack;
static Fighter sMVOpeningCliffOcarinaPack;
static Fighter sMVOpeningCliffHillsModel;
static Fighter sMVOpeningCliffOcarinaModel;
static sb32 sMVOpeningCliffModelsLoaded;
static AObjEvent32 *sMVOpeningCliffOcarinaAnimJoint[FIGHTER_MAX_JOINTS];

#ifndef FT_HOSTTEST
static void mvOpeningCliffSetOcarinaAnimJoint(void)
{
    const Fighter *f = &sMVOpeningCliffOcarinaPack;
    const FPackAnim *anim;
    const s32 *entries;
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sMVOpeningCliffOcarinaAnimJoint); i++)
    {
        sMVOpeningCliffOcarinaAnimJoint[i] = NULL;
    }
    if (f->hd->anim_count == 0)
    {
        return;
    }
    anim = &f->anims[0];
    entries = (const s32 *)((const u8 *)f->blob + anim->off_entries);

    for (i = 0; i < f->hd->joint_count &&
                i < ARRAY_COUNT(sMVOpeningCliffOcarinaAnimJoint); i++)
    {
        if (entries[i] >= 0 && (u32) entries[i] < anim->nwords)
        {
            sMVOpeningCliffOcarinaAnimJoint[i] =
                (AObjEvent32 *)((u8 *)f->blob + anim->off_words) + entries[i];
        }
    }
}
#endif

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningcliff.c:80131B00 mvOpeningCliffFuncLights. Not ported -- the
 * standing rule. func_lights is NULL below. */

/* mvopeningcliff.c:80131B58 mvOpeningCliffHillsProcDisplay. DIVERGES: the
 * render-state calls around the draw are what the pack's FPACK_NOZ bakes;
 * dc_model_proc_display is the draw. */

/* mvopeningcliff.c:80131C34 mvOpeningCliffMakeHills. DIVERGES:
 * gcSetupCommonDObjs over the file's DObjDesc is dc_model_add_dobjs over
 * the pack. */
void mvOpeningCliffMakeHills(void)
{
    GObj *hills_gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

#ifndef FT_HOSTTEST
    if (sMVOpeningCliffModelsLoaded != FALSE)
    {
        fighter_clone(&sMVOpeningCliffHillsModel, &sMVOpeningCliffHillsPack,
                      syTaskmanMalloc(sizeof(float[4]) *
                                      sMVOpeningCliffHillsPack.hd->vert_count, 0x8));
        dc_model_add_dobjs(hills_gobj, NULL, &sMVOpeningCliffHillsModel, NULL);
    }
#else
    gcAddDObjForGObj(hills_gobj, NULL);
#endif
    gcAddGObjDisplay(hills_gobj, dc_model_proc_display, 26, GOBJ_PRIORITY_DEFAULT, ~0);
}

/* mvopeningcliff.c:80131CAC mvOpeningCliffMakeFighter, verbatim but for
 * the figatree heap (Substitution 4). */
void mvOpeningCliffMakeFighter(void)
{
    GObj* fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = nFTKindLink;
    desc.costume = ftParamGetCostumeCommonID(nFTKindLink, 0);
    desc.pos.x = 0.0F;
    desc.pos.y = 0.0F;
    desc.pos.z = 0.0F;
    desc.figatree_heap = NULL;

    sMVOpeningCliffFighterGObj = fighter_gobj = ftManagerMakeFighter(&desc);

    scSubsysFighterSetStatus(fighter_gobj, 0x1000F);

    gcMoveGObjDL(fighter_gobj, 28, -1);

    DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;
}

/* mvopeningcliff.c:80131D8C mvOpeningCliffWallpaperProcDisplay, verbatim. */
void mvOpeningCliffWallpaperProcDisplay(GObj *gobj)
{
    SObj *sobj = SObjGetStruct(gobj);

    switch (sMVOpeningCliffTotalTimeTics)
    {
    case 1:
        sMVOpeningCliffWallpaperScrollSpeed = 15.0F;
        break;

    case 80:
        sMVOpeningCliffWallpaperScrollSpeed = 10.0F;
        break;

    case 90:
        sMVOpeningCliffWallpaperScrollSpeed = 6.0F;
        break;

    case 120:
        sMVOpeningCliffWallpaperScrollSpeed = 2.0F;
        break;

    case 180:
        sMVOpeningCliffWallpaperScrollSpeed = 0.0F;
        break;
    }
    if ((sMVOpeningCliffTotalTimeTics > 1) && (sMVOpeningCliffTotalTimeTics < 80))
    {
        sMVOpeningCliffWallpaperScrollSpeed += (-5.0F / 79.0F);
    }
    if ((sMVOpeningCliffTotalTimeTics > 80) && (sMVOpeningCliffTotalTimeTics < 90))
    {
        sMVOpeningCliffWallpaperScrollSpeed += (-2.0F / 5.0F);
    }
    if ((sMVOpeningCliffTotalTimeTics > 90) && (sMVOpeningCliffTotalTimeTics < 120))
    {
        sMVOpeningCliffWallpaperScrollSpeed += (-2.0F / 15.0F);
    }
    if ((sMVOpeningCliffTotalTimeTics > 120) && (sMVOpeningCliffTotalTimeTics < 180))
    {
        sMVOpeningCliffWallpaperScrollSpeed += (-1.0F / 30.0F);
    }
    sobj->pos.x -= sMVOpeningCliffWallpaperScrollSpeed;

    if (sobj->pos.x < -320.0F)
    {
        sobj->pos.x += 320.0F;
    }
    sobj->next->pos.x = sobj->pos.x + 320.0F;
}

/* mvopeningcliff.c:80131F2C mvOpeningCliffMakeWallpaper, verbatim but for
 * lbRelocGetFileData's own redefinition above. */
void mvOpeningCliffMakeWallpaper(void)
{
    GObj *wallpaper_gobj;
    SObj *wallpaper_sobj;

    wallpaper_gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(wallpaper_gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(wallpaper_gobj, mvOpeningCliffWallpaperProcDisplay, nGCProcessKindFunc, 1);

    wallpaper_sobj = lbCommonMakeSObjForGObj(wallpaper_gobj, lbRelocGetFileData(Sprite*, sMVOpeningCliffWallpaperFileHead, llMVOpeningStandoffWallpaperSprite));
    wallpaper_sobj->sprite.attr &= ~SP_FASTCOPY;

    wallpaper_sobj->sprite.scalex = 2.0F;
    wallpaper_sobj->sprite.scaley = 2.0F;

    wallpaper_sobj->pos.x = 0.0F;
    wallpaper_sobj->pos.y = 0.0F;

    wallpaper_sobj = lbCommonMakeSObjForGObj(wallpaper_gobj, lbRelocGetFileData(Sprite*, sMVOpeningCliffWallpaperFileHead, llMVOpeningStandoffWallpaperSprite));
    wallpaper_sobj->sprite.attr &= ~SP_FASTCOPY;

    wallpaper_sobj->sprite.scalex = 2.0F;
    wallpaper_sobj->sprite.scaley = 2.0F;

    wallpaper_sobj->pos.x = 320.0F;
    wallpaper_sobj->pos.y = 0.0F;
}

/* mvopeningcliff.c:80132024 mvOpeningCliffMakeOcarina. DIVERGES: the
 * DObj tree comes from the pack, the AnimJoint table from its rebuilt
 * pointers, and the attach is FT_HOSTTEST-guarded (the host cannot walk a
 * script). */
void mvOpeningCliffMakeOcarina(void)
{
    GObj *ocarina_gobj;

    sMVOpeningCliffOcarinaGObj = ocarina_gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
#ifndef FT_HOSTTEST
    if (sMVOpeningCliffModelsLoaded != FALSE)
    {
        fighter_clone(&sMVOpeningCliffOcarinaModel, &sMVOpeningCliffOcarinaPack,
                      syTaskmanMalloc(sizeof(float[4]) *
                                      sMVOpeningCliffOcarinaPack.hd->vert_count, 0x8));
        dc_model_add_dobjs(ocarina_gobj, NULL, &sMVOpeningCliffOcarinaModel, NULL);
    }
#else
    gcAddDObjRpyR(ocarina_gobj, NULL);
#endif
    gcAddGObjDisplay(ocarina_gobj, dc_model_proc_display, 26, GOBJ_PRIORITY_DEFAULT, ~0);

    DObjGetStruct(ocarina_gobj)->scale.vec.f.x = DObjGetStruct(sMVOpeningCliffFighterGObj)->scale.vec.f.x;
    DObjGetStruct(ocarina_gobj)->scale.vec.f.y = DObjGetStruct(sMVOpeningCliffFighterGObj)->scale.vec.f.y;
    DObjGetStruct(ocarina_gobj)->scale.vec.f.z = DObjGetStruct(sMVOpeningCliffFighterGObj)->scale.vec.f.z;

#ifndef FT_HOSTTEST
    if (sMVOpeningCliffModelsLoaded != FALSE)
    {
        gcAddAnimJointAll(ocarina_gobj, sMVOpeningCliffOcarinaAnimJoint, 0.0F);
        gcAddGObjProcess(ocarina_gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
    }
#endif
}

/* mvopeningcliff.c:8013212C mvOpeningCliffCameraProcUpdate, verbatim. */
void mvOpeningCliffCameraProcUpdate(GObj *gobj)
{
    gcPlayCamAnim(gobj);
}

/* mvopeningcliff.c:8013214C mvOpeningCliffMakeMainCamera. DIVERGES: the
 * script is camanim_get(..., "Cam") and the attach-and-play pair is
 * FT_HOSTTEST-guarded. Two cameras, one per display list (26: hills and
 * ocarina; 28: the fighter), fly on the same script. */
void mvOpeningCliffMakeMainCamera(void)
{
    GObj* camera_gobj;
    CObj* cobj;

    camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        80,
        COBJ_MASK_DLLINK(26),
        -1,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    gcAddXObjForCamera(CObjGetStruct(camera_gobj), nGCMatrixKindPerspF, 0);
    gcAddXObjForCamera(CObjGetStruct(camera_gobj), 6, 0);

    cobj = CObjGetStruct(camera_gobj);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->projection.persp.near = 128.0F;
    cobj->projection.persp.far = 16384.0F;

#ifndef FT_HOSTTEST
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningCliffCamAnimBank, "Cam"), 0.0F);
    gcAddGObjProcess(camera_gobj, mvOpeningCliffCameraProcUpdate, nGCProcessKindFunc, 1);
#endif

    camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        70,
        COBJ_MASK_DLLINK(28),
        -1,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    gcAddXObjForCamera(CObjGetStruct(camera_gobj), nGCMatrixKindPerspF, 0);
    gcAddXObjForCamera(CObjGetStruct(camera_gobj), 6, 0);

    cobj = CObjGetStruct(camera_gobj);

    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->projection.persp.near = 128.0F;
    cobj->projection.persp.far = 16384.0F;

#ifndef FT_HOSTTEST
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningCliffCamAnimBank, "Cam"), 0.0F);
    gcAddGObjProcess(camera_gobj, mvOpeningCliffCameraProcUpdate, nGCProcessKindFunc, 1);
#endif
}

/* mvopeningcliff.c:80132368 mvOpeningCliffMakeWallpaperCamera. DIVERGES:
 * lbCommonDrawSpriteBackdrop for lbCommonDrawSprite -- this camera's 90
 * puts the wallpaper under the hills' camera (80) and Link's (70) on
 * the N64, and the backdrop band is what puts it under their opaque
 * models here (src/dc/lbcommon.h). */
void mvOpeningCliffMakeWallpaperCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
        (
            nGCCommonKindSceneCamera,
            NULL,
            16,
            GOBJ_PRIORITY_DEFAULT,
            lbCommonDrawSpriteBackdrop,
            90,
            COBJ_MASK_DLLINK(27),
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

/* mvopeningcliff.c:80132408 mvOpeningCliffInitTotalTimeTics, verbatim. */
void mvOpeningCliffInitTotalTimeTics(void)
{
    sMVOpeningCliffTotalTimeTics = 0;
}

/* mvopeningcliff.c:80132414 mvOpeningCliffFuncRun, verbatim. */
void mvOpeningCliffFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningCliffTotalTimeTics++;

    if (sMVOpeningCliffTotalTimeTics >= 10)
    {
        if (sMVOpeningCliffUnused0x801327DC != 0)
        {
            sMVOpeningCliffUnused0x801327DC--;
        }
        if ((scSubsysControllerGetPlayerStickInRangeLR(-15, 15) != FALSE) && (scSubsysControllerGetPlayerStickInRangeUD(-15, 15) != FALSE))
        {
            sMVOpeningCliffUnused0x801327DC = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            syTaskmanSetLoadScene();
        }
        if (sMVOpeningCliffTotalTimeTics == 160)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindOpeningYamabuki;

            syTaskmanSetLoadScene();
        }
    }
}

/* mvopeningcliff.c:801324E0 mvOpeningCliffFuncStart. DIVERGES: the reloc
 * loader is the bank/pack loads (mvopeningcliff.h), efManagerInitEffects
 * is Substitution 3b, the figatree-heap malloc is cut (4) and so is the
 * trailing tic-sync busy-wait (5). */
void mvOpeningCliffFuncStart(void)
{
    if (sprite_bank_load(&sMVOpeningCliffWallpaperBank, "mvopeningcliff.spr") < 0)
    {
        sMVOpeningCliffWallpaperFileHead = NULL;
    }
    else
    {
        sMVOpeningCliffWallpaperFileHead = &sMVOpeningCliffWallpaperBank;
    }
    camanim_bank_load(&sMVOpeningCliffCamAnimBank, "mvopeningcliff.cam");
#ifndef FT_HOSTTEST
    {
        int pal_bank = 0;

        sMVOpeningCliffModelsLoaded =
            (fighter_load_scene(&sMVOpeningCliffHillsPack, MVOPENINGCLIFF_MODEL_HILLS, &pal_bank) == 0) &&
            (fighter_load_scene(&sMVOpeningCliffOcarinaPack, MVOPENINGCLIFF_MODEL_OCARINA, &pal_bank) == 0);
        if (sMVOpeningCliffModelsLoaded != FALSE)
        {
            mvOpeningCliffSetOcarinaAnimJoint();
        }
    }
#endif

    gcMakeGObjSPAfter(0, mvOpeningCliffFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
    gcMakeDefaultCameraGObj(0, GOBJ_PRIORITY_DEFAULT, 100, COBJ_FLAG_ZBUFFER, GPACK_RGBA8888(0x00, 0x00, 0x00, 0x00));

    efParticleInitAll();
    mvOpeningCliffInitTotalTimeTics();

    efManagerLoadEffectBank();
    efManagerPreloadModels();
    ftManagerAllocFighter(FTDATA_FLAG_SUBMOTION, 1);
    ftManagerSetupFilesAllKind(nFTKindLink);

    mvOpeningCliffMakeMainCamera();
    mvOpeningCliffMakeWallpaperCamera();
    mvOpeningCliffMakeWallpaper();
    mvOpeningCliffMakeHills();
    mvOpeningCliffMakeFighter();
    mvOpeningCliffMakeOcarina();

    scSubsysFighterSetLightParams(-45.0F, 25.0F, 0xFF, 0xFF, 0xFF, 0xFF);

    /* DIVERGES (port only): dc_model_set_demo_opaque(TRUE) -- the hills'
     * camera (80) and Link's own (70) fly the same script (same view),
     * so Link takes the opaque draw at his real depth against the
     * hills and the ocarina, as the N64's shared Z buffer does, instead
     * of the layered band the per-fighter panel scenes need. See
     * [[room-logo-layering]] / mvopeningroom.c. */
    dc_model_set_demo_opaque(TRUE);
    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 2500 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(2500);
}

/* mvopeningcliff.c:80132724-shaped dMVOpeningCliffTaskmanSetup. See
 * mvopeningportraits.h for func_draw (scManagerFuncDraw), the zeroed
 * RSP/RDP fields and the pre-render/controller/matrix-list
 * substitutions. */
SYTaskmanSetup dMVOpeningCliffTaskmanSetup =
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
    MVOPENINGCLIFF_GOBJPROCS,
    MVOPENINGCLIFF_GOBJS,
    sizeof(GObj),
    MVOPENINGCLIFF_XOBJS,
    NULL,
    NULL,
    MVOPENINGCLIFF_AOBJS,
    MVOPENINGCLIFF_MOBJS,
    MVOPENINGCLIFF_DOBJS,
    sizeof(DObj),
    MVOPENINGCLIFF_SOBJS,
    sizeof(SObj),
    MVOPENINGCLIFF_COBJS,
    sizeof(CObj),

    mvOpeningCliffFuncStart
};

/* mvopeningcliff.c:80132674 mvOpeningCliffStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeningcliff.h). */
void mvOpeningCliffStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningCliffTaskmanSetup);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[46].
 * The packs are given back first: the scene's end is where the
 * fighter_release of the arrows lives too. */
void mvOpeningCliffOverlayLoad(void)
{
#ifndef FT_HOSTTEST
    fighter_release(&sMVOpeningCliffHillsPack);
    fighter_release(&sMVOpeningCliffOcarinaPack);
#endif
    OVERLAY_CLEAR(sMVOpeningCliffTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningCliffOcarinaGObj);
    OVERLAY_CLEAR(sMVOpeningCliffFighterGObj);
    OVERLAY_CLEAR(sMVOpeningCliffWallpaperScrollSpeed);
    OVERLAY_CLEAR(sMVOpeningCliffUnused0x801327DC);
    OVERLAY_CLEAR(sMVOpeningCliffWallpaperBank);
    OVERLAY_CLEAR(sMVOpeningCliffWallpaperFileHead);
    OVERLAY_CLEAR(sMVOpeningCliffCamAnimBank);
    OVERLAY_CLEAR(sMVOpeningCliffHillsPack);
    OVERLAY_CLEAR(sMVOpeningCliffOcarinaPack);
    OVERLAY_CLEAR(sMVOpeningCliffHillsModel);
    OVERLAY_CLEAR(sMVOpeningCliffOcarinaModel);
    OVERLAY_CLEAR(sMVOpeningCliffModelsLoaded);
    OVERLAY_CLEAR(sMVOpeningCliffOcarinaAnimJoint);
}
