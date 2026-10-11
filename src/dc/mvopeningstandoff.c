/* mvopeningstandoff.c -- see mvopeningstandoff.h. Function-for-function
 * from ssb-decomp-re/src/mv/mvopening/mvopeningstandoff.c; the
 * substitutions are explained once, in mvopeningstandoff.h's own header
 * comment -- not repeated at every site.
 */
#include "mvopeningstandoff.h"
#include "overlay.h"

#include "camanim.h"    /* the camera script */
#include "fighter.h"
#include "objmodel.h"    /* dc_model_add_dobjs, dc_model_proc_display */
#include "ftcommon.h"
#include "lbcommon.h"   /* lbCommonDrawSprite, lbCommonDrawSObjAttr, lbCommonSpriteFillRect */
#include "objpvr.h"     /* gcGetDrawList */
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
#include <sys/objanim.h>           /* gcAddAnimJointAll, gcAddCObjCamAnimJoint */
#include <sys/objdef.h>
#include <sys/rdp.h>
#include <lb/lbdef.h>

/* The game reaches its wallpaper as file 70's base plus a link label's
 * offset; the port's file is a sprite bank and the label's value is the
 * offset (mvopeningcliff.c does the same). */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* llMVOpeningStandoffWallpaperSprite (src/dc/decomp/reloc_data.us.h). */
#define llMVOpeningStandoffWallpaperSprite 0xB500

/* ---- the pools --------------------------------------------------------
 *
 * Two fighters plus the ground and the thirteen-joint lightning: the
 * decomp's own counts (512 XObjs/DObjs) are short of the port's
 * one-set-per-joint model (mvopeningrun.c), so these are sized as Run's
 * are. */
#define MVOPENINGSTANDOFF_GOBJS       128
#define MVOPENINGSTANDOFF_GOBJPROCS   128
#define MVOPENINGSTANDOFF_XOBJS       768
#define MVOPENINGSTANDOFF_AOBJS      1200
#define MVOPENINGSTANDOFF_MOBJS       240
#define MVOPENINGSTANDOFF_DOBJS       512
#define MVOPENINGSTANDOFF_SOBJS        16
#define MVOPENINGSTANDOFF_COBJS         8

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static s32 sMVOpeningStandoffTotalTimeTics;
static f32 sMVOpeningStandoffWallpaperScrollSpeed;
static s32 sMVOpeningStandoffUnused0x801329DC;

/* The port's stand-in for sMVOpeningStandoffFiles[1] (file 70): the
 * wallpaper sprite, romdisk/mvopeningcliff.spr. Not static so
 * hosttest_ft.c can read it. */
static SpriteBank sMVOpeningStandoffWallpaperBank;
void *sMVOpeningStandoffWallpaperFileHead;

/* The port's stand-in for the camera script in file 69. */
static CamAnimBank sMVOpeningStandoffCamAnimBank;

/* The file's two models as packs, their instances, and the lightning's
 * table of per-joint scripts rebuilt from its pack (ifcommon.c's arrows do
 * the same: gcAddAnimJointAll wants the pointers the file held). */
enum
{
    MVOPENINGSTANDOFF_MODEL_GROUND,
    MVOPENINGSTANDOFF_MODEL_LIGHTNING,
    MVOPENINGSTANDOFF_MODEL_COUNT
};

#ifndef FT_HOSTTEST
static const char *const sMVOpeningStandoffModelNames[MVOPENINGSTANDOFF_MODEL_COUNT] =
{
    "mvopeningstandoffground.mdl",
    "mvopeningstandofflightning.mdl"
};
#endif

static Fighter sMVOpeningStandoffPacks[MVOPENINGSTANDOFF_MODEL_COUNT];
static Fighter sMVOpeningStandoffModels[MVOPENINGSTANDOFF_MODEL_COUNT];
static sb32 sMVOpeningStandoffModelsLoaded;
static AObjEvent32 *sMVOpeningStandoffAnimJoints[FIGHTER_MAX_JOINTS];

#ifndef FT_HOSTTEST
static void mvOpeningStandoffSetAnimJoint(s32 id)
{
    const Fighter *f = &sMVOpeningStandoffPacks[id];
    const FPackAnim *anim;
    const s32 *entries;
    u32 i;

    for (i = 0; i < FIGHTER_MAX_JOINTS; i++)
    {
        sMVOpeningStandoffAnimJoints[i] = NULL;
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
            sMVOpeningStandoffAnimJoints[i] = (AObjEvent32 *)((u8 *)f->blob + anim->off_words) + entries[i];
        }
    }
}
#endif

/* The pack's tree on `gobj`: the game's gcAddDObjForGObj (the ground) or
 * gcSetupCustomDObjs (the lightning) in the pack's stead.
 * DIVERGES: the tree comes from the pack, the display proc is
 * dc_model_proc_display (both of the game's are the walk it does), and
 * the pack build is FT_HOSTTEST-guarded (the host links no renderer and
 * cannot walk a script). */
static void mvOpeningStandoffSetupModel(GObj *gobj, s32 id)
{
#ifndef FT_HOSTTEST
    if (sMVOpeningStandoffModelsLoaded != FALSE)
    {
        fighter_clone(&sMVOpeningStandoffModels[id], &sMVOpeningStandoffPacks[id],
                      syTaskmanMalloc(sizeof(float[4]) *
                                      sMVOpeningStandoffPacks[id].hd->vert_count, 0x8));
        dc_model_add_dobjs(gobj, NULL, &sMVOpeningStandoffModels[id], NULL);
        dc_model_add_mobjs(gobj, &sMVOpeningStandoffModels[id], 0.0F);
    }
#else
    gcAddDObjRpyR(gobj, NULL);
#endif
    gcAddGObjDisplay(gobj, dc_model_proc_display, 26, GOBJ_PRIORITY_DEFAULT, ~0);

    DObjGetStruct(gobj)->translate.vec.f.x = 0.0F;
    DObjGetStruct(gobj)->translate.vec.f.y = 0.0F;
    DObjGetStruct(gobj)->translate.vec.f.z = 0.0F;
}

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningstandoff.c:80131B00 mvOpeningStandoffFuncLights. Not ported --
 * the standing rule. func_lights is NULL below. */

/* mvopeningstandoff.c:80131B58 mvOpeningStandoffMakeGround. */
void mvOpeningStandoffMakeGround(void)
{
    mvOpeningStandoffSetupModel(gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT),
                                MVOPENINGSTANDOFF_MODEL_GROUND);
}

/* mvopeningstandoff.c:80131C00 mvOpeningStandoffMakeFighters, verbatim but
 * for the figatree heaps (mvopeningmario.h's Substitution 4, NULL). */
void mvOpeningStandoffMakeFighters(void)
{
    GObj *fighter_gobj;
    FTDesc desc = dFTManagerDefaultFighterDesc;

    desc.fkind = nFTKindMario;
    desc.costume = ftParamGetCostumeCommonID(nFTKindMario, 0);
    desc.pos.x = 0.0F;
    desc.pos.y = 0.0F;
    desc.pos.z = 0.0F;
    desc.figatree_heap = NULL;

    fighter_gobj = ftManagerMakeFighter(&desc);

    scSubsysFighterSetStatus(fighter_gobj, 0x1000F);

    DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;

    desc.fkind = nFTKindKirby;
    desc.costume = ftParamGetCostumeCommonID(nFTKindKirby, 0);
    desc.pos.x = 0.0F;
    desc.pos.y = 0.0F;
    desc.pos.z = 0.0F;
    desc.figatree_heap = NULL;

    fighter_gobj = ftManagerMakeFighter(&desc);

    scSubsysFighterSetStatus(fighter_gobj, 0x1000F);

    DObjGetStruct(fighter_gobj)->scale.vec.f.x = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.y = 1.0F;
    DObjGetStruct(fighter_gobj)->scale.vec.f.z = 1.0F;
}

/* mvopeningstandoff.c:80131D38 mvOpeningStandoffWallpaperProcUpdate,
 * verbatim. */
void mvOpeningStandoffWallpaperProcUpdate(GObj *wallpaper_gobj)
{
    SObj *wallpaper_sobj = SObjGetStruct(wallpaper_gobj);

    switch (sMVOpeningStandoffTotalTimeTics)
    {
    case 1:
        sMVOpeningStandoffWallpaperScrollSpeed = 2.0F;
        break;

    case 90:
        sMVOpeningStandoffWallpaperScrollSpeed = 8.0F;
        break;

    case 105:
        sMVOpeningStandoffWallpaperScrollSpeed = 2.0F;
        break;

    case 180:
        sMVOpeningStandoffWallpaperScrollSpeed = 8.0F;
        break;

    case 195:
        sMVOpeningStandoffWallpaperScrollSpeed = 2.0F;
        break;

    case 280:
        sMVOpeningStandoffWallpaperScrollSpeed = 2.0F;
        break;

    case 300:
        sMVOpeningStandoffWallpaperScrollSpeed = 7.0F;
        break;
    }
    if ((sMVOpeningStandoffTotalTimeTics >= 281) && (sMVOpeningStandoffTotalTimeTics < 300))
    {
        sMVOpeningStandoffWallpaperScrollSpeed += -0.05F;
    }

    if ((sMVOpeningStandoffTotalTimeTics >= 301) && (sMVOpeningStandoffTotalTimeTics < 320))
    {
        sMVOpeningStandoffWallpaperScrollSpeed += 0.4F;
    }

    if (sMVOpeningStandoffTotalTimeTics >= 301)
    {
        wallpaper_sobj->pos.y += sMVOpeningStandoffWallpaperScrollSpeed;
    }
    else
    {
        wallpaper_sobj->pos.x += sMVOpeningStandoffWallpaperScrollSpeed;

        if (wallpaper_sobj->pos.x > 320.0F)
        {
            wallpaper_sobj->pos.x -= 320.0F;
        }
    }
    if
    (
        ((sMVOpeningStandoffTotalTimeTics >  90) && (sMVOpeningStandoffTotalTimeTics < 105)) ||
        ((sMVOpeningStandoffTotalTimeTics > 180) && (sMVOpeningStandoffTotalTimeTics < 195))
    )
    {
        wallpaper_sobj->next->pos.x = wallpaper_sobj->pos.x - 640.0F;
        wallpaper_sobj->next->pos.y = wallpaper_sobj->pos.y;

        wallpaper_sobj->sprite.scalex = 4.0F;
        wallpaper_sobj->sprite.scaley = 4.0F;

        wallpaper_sobj->next->sprite.scalex = 4.0F;
        wallpaper_sobj->next->sprite.scaley = 4.0F;

        wallpaper_sobj->pos.y = -240.0F;
    }
    else
    {
        wallpaper_sobj->next->pos.x = wallpaper_sobj->pos.x - 320.0F;
        wallpaper_sobj->next->pos.y = wallpaper_sobj->pos.y;

        wallpaper_sobj->sprite.scalex = 2.0F;
        wallpaper_sobj->sprite.scaley = 2.0F;

        wallpaper_sobj->next->sprite.scalex = 2.0F;
        wallpaper_sobj->next->sprite.scaley = 2.0F;

        if (sMVOpeningStandoffTotalTimeTics < 300)
        {
            wallpaper_sobj->pos.y = 0.0F;
        }
    }
}

/* mvopeningstandoff.c:80131FC8 mvOpeningStandoffMakeWallpaper, verbatim
 * but for lbRelocGetFileData's own redefinition above. */
void mvOpeningStandoffMakeWallpaper(void)
{
    GObj *wallpaper_gobj;
    SObj *wallpaper_sobj;

    wallpaper_gobj = gcMakeGObjSPAfter(0, NULL, 19, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(wallpaper_gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(wallpaper_gobj, mvOpeningStandoffWallpaperProcUpdate, nGCProcessKindFunc, 1);

    wallpaper_sobj = lbCommonMakeSObjForGObj(wallpaper_gobj, lbRelocGetFileData(Sprite*, sMVOpeningStandoffWallpaperFileHead, llMVOpeningStandoffWallpaperSprite));
    wallpaper_sobj->sprite.attr &= ~SP_FASTCOPY;

    wallpaper_sobj->sprite.scalex = 2.0F;
    wallpaper_sobj->sprite.scaley = 2.0F;

    wallpaper_sobj->pos.x = 0.0F;
    wallpaper_sobj->pos.y = 0.0F;

    wallpaper_sobj = lbCommonMakeSObjForGObj(wallpaper_gobj, lbRelocGetFileData(Sprite*, sMVOpeningStandoffWallpaperFileHead, llMVOpeningStandoffWallpaperSprite));
    wallpaper_sobj->sprite.attr &= ~SP_FASTCOPY;

    wallpaper_sobj->sprite.scalex = 2.0F;
    wallpaper_sobj->sprite.scaley = 2.0F;

    wallpaper_sobj->pos.x = 320.0F;
    wallpaper_sobj->pos.y = 0.0F;
}

/* mvopeningstandoff.c:801320C0 mvOpeningStandoffMakeLightning. DIVERGES:
 * gcSetupCustomDObjs + gcAddMObjAll + gcAddMatAnimJointAll are the pack's
 * tree and dc_model_add_mobjs (mvOpeningStandoffSetupModel); the
 * AnimJoint table is the pack's, rebuilt. */
void mvOpeningStandoffMakeLightning(void)
{
    GObj *lightning_gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);

    mvOpeningStandoffSetupModel(lightning_gobj, MVOPENINGSTANDOFF_MODEL_LIGHTNING);

#ifndef FT_HOSTTEST
    if (sMVOpeningStandoffModelsLoaded != FALSE)
    {
        gcAddAnimJointAll(lightning_gobj, sMVOpeningStandoffAnimJoints, 0.0F);
        gcAddGObjProcess(lightning_gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
    }
#endif
}

/* mvopeningstandoff.c:801321D8 mvOpeningStandoffLightningFlashProcDisplay.
 * DIVERGES: the flash is a gDPFillRectangle under G_CC_PRIMITIVE, and
 * the raw GBI lands in the write-only scratch heads (src/dc/wpmanager.c;
 * the head index only ever chose a scratch row), so it is the fill quad
 * src/dc/ifscreenflash.c draws instead, translucent pass only. Outside
 * the three windows the N64 drew the same rectangle at alpha 0; here
 * nothing is drawn, which is the same picture for less fill. */
void mvOpeningStandoffLightningFlashProcDisplay(GObj *gobj)
{
    (void)gobj;

    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    if
    (
        ((sMVOpeningStandoffTotalTimeTics >  19) && (sMVOpeningStandoffTotalTimeTics <  23)) ||
        ((sMVOpeningStandoffTotalTimeTics > 149) && (sMVOpeningStandoffTotalTimeTics < 153)) ||
        ((sMVOpeningStandoffTotalTimeTics > 260) && (sMVOpeningStandoffTotalTimeTics < 264))
    )
    {
        lbCommonSpriteFillRect(10, 10, 310, 230, 0xFF, 0xFF, 0xFF, 0x40);
    }
}

/* mvopeningstandoff.c:80132338 mvOpeningStandoffMakeLightningFlash,
 * verbatim. */
void mvOpeningStandoffMakeLightningFlash(void)
{
    gcAddGObjDisplay
    (
        gcMakeGObjSPAfter
        (
            0,
            NULL,
            18,
            GOBJ_PRIORITY_DEFAULT
        ),
        mvOpeningStandoffLightningFlashProcDisplay,
        28,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
}

/* mvopeningstandoff.c:80132384 mvOpeningStandoffMakeLightningFlashCamera,
 * verbatim. */
void mvOpeningStandoffMakeLightningFlashCamera(void)
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
            20,
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

/* mvopeningstandoff.c:8013242C mvOpeningStandoffMakeMainCamera. DIVERGES:
 * the script is camanim_get(..., "Cam") and the attach-and-play pair is
 * FT_HOSTTEST-guarded. */
void mvOpeningStandoffMakeMainCamera(void)
{
    GObj *camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        80,
        COBJ_MASK_DLLINK(26) |
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

    cobj->projection.persp.near = 128.0F;
    cobj->projection.persp.far = 16384.0F;

#ifndef FT_HOSTTEST
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningStandoffCamAnimBank, "Cam"), 0.0F);
    gcAddGObjProcess(camera_gobj, gcPlayCamAnim, nGCProcessKindFunc, 1);
    gcPlayCamAnim(camera_gobj);
#endif
}

/* mvopeningstandoff.c:80132530 mvOpeningStandoffMakeWallpaperCamera.
 * DIVERGES: lbCommonDrawSpriteBackdrop for lbCommonDrawSprite -- this
 * camera's 90 puts the wallpaper under the fighters' camera (80) on the
 * N64, and the backdrop band is what puts it under their opaque props
 * here (src/dc/lbcommon.h). */
void mvOpeningStandoffMakeWallpaperCamera(void)
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

/* mvopeningstandoff.c:801325D0 mvOpeningStandoffInitTotalTimeTics,
 * verbatim. */
void mvOpeningStandoffInitTotalTimeTics(void)
{
    sMVOpeningStandoffTotalTimeTics = 0;
}

/* mvopeningstandoff.c:801325DC mvOpeningStandoffFuncRun, verbatim. */
void mvOpeningStandoffFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningStandoffTotalTimeTics++;

    if (sMVOpeningStandoffTotalTimeTics >= 10)
    {
        if (sMVOpeningStandoffUnused0x801329DC != 0)
        {
            sMVOpeningStandoffUnused0x801329DC--;
        }
        if ((scSubsysControllerGetPlayerStickInRangeLR(-15, 15) != FALSE) && (scSubsysControllerGetPlayerStickInRangeUD(-15, 15) != FALSE))
        {
            sMVOpeningStandoffUnused0x801329DC = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            syTaskmanSetLoadScene();
        }
        if (sMVOpeningStandoffTotalTimeTics == 320)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindOpeningClash;

            syTaskmanSetLoadScene();
        }
    }
}

/* mvopeningstandoff.c:801326A8 mvOpeningStandoffFuncStart. DIVERGES: the
 * reloc loader is the bank/pack loads (mvopeningstandoff.h),
 * efManagerInitEffects is Substitution 3b, the figatree-heap mallocs are
 * cut (4), the default camera's FILLCOLOR is dropped (fillcolor-camera
 * note: it paints over every 3D list here; the black clear is the same)
 * and so is the trailing tic-sync busy-wait (5). */
void mvOpeningStandoffFuncStart(void)
{
    if (sprite_bank_load(&sMVOpeningStandoffWallpaperBank, "mvopeningcliff.spr") < 0)
    {
        sMVOpeningStandoffWallpaperFileHead = NULL;
    }
    else
    {
        sMVOpeningStandoffWallpaperFileHead = &sMVOpeningStandoffWallpaperBank;
    }
    camanim_bank_load(&sMVOpeningStandoffCamAnimBank, "mvopeningstandoff.cam");
#ifndef FT_HOSTTEST
    {
        int pal_bank = 0;
        s32 i;

        sMVOpeningStandoffModelsLoaded = TRUE;
        for (i = 0; i < MVOPENINGSTANDOFF_MODEL_COUNT; i++)
        {
            if (fighter_load_scene(&sMVOpeningStandoffPacks[i], sMVOpeningStandoffModelNames[i], &pal_bank) != 0)
            {
                sMVOpeningStandoffModelsLoaded = FALSE;
                break;
            }
        }
        if (sMVOpeningStandoffModelsLoaded != FALSE)
        {
            mvOpeningStandoffSetAnimJoint(MVOPENINGSTANDOFF_MODEL_LIGHTNING);
        }
    }
#endif

    gcMakeGObjSPAfter(0, mvOpeningStandoffFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
    gcMakeDefaultCameraGObj(0, GOBJ_PRIORITY_DEFAULT, 100, COBJ_FLAG_ZBUFFER, GPACK_RGBA8888(0x00, 0x00, 0x00, 0x00));

    efParticleInitAll();
    mvOpeningStandoffInitTotalTimeTics();

    efManagerLoadEffectBank();
    efManagerPreloadModels();
    ftManagerAllocFighter(FTDATA_FLAG_SUBMOTION, 2);
    ftManagerSetupFilesAllKind(nFTKindMario);
    ftManagerSetupFilesAllKind(nFTKindKirby);

    mvOpeningStandoffMakeMainCamera();
    mvOpeningStandoffMakeWallpaperCamera();
    mvOpeningStandoffMakeLightningFlashCamera();
    mvOpeningStandoffMakeWallpaper();
    mvOpeningStandoffMakeFighters();
    mvOpeningStandoffMakeGround();
    mvOpeningStandoffMakeLightning();
    mvOpeningStandoffMakeLightningFlash();

    scSubsysFighterSetLightParams(45.0F, 45.0F, 0xFF, 0xFF, 0xFF, 0xFF);

    /* DIVERGES (port only): dc_model_set_demo_opaque(TRUE) -- one camera
     * (80) covers the ground and both fighters, so Mario and Kirby take
     * the opaque draw at their real depth against it, as the N64's
     * shared Z buffer does, instead of the layered band the per-fighter
     * panel scenes need. See [[room-logo-layering]] / mvopeningroom.c. */
    dc_model_set_demo_opaque(TRUE);
    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 3610 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(3610);
}

/* mvopeningstandoff.c:80132924-shaped dMVOpeningStandoffTaskmanSetup. See
 * mvopeningportraits.h for func_draw (scManagerFuncDraw), the zeroed
 * RSP/RDP fields and the pre-render/controller/matrix-list
 * substitutions. */
SYTaskmanSetup dMVOpeningStandoffTaskmanSetup =
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
    MVOPENINGSTANDOFF_GOBJPROCS,
    MVOPENINGSTANDOFF_GOBJS,
    sizeof(GObj),
    MVOPENINGSTANDOFF_XOBJS,
    NULL,
    NULL,
    MVOPENINGSTANDOFF_AOBJS,
    MVOPENINGSTANDOFF_MOBJS,
    MVOPENINGSTANDOFF_DOBJS,
    sizeof(DObj),
    MVOPENINGSTANDOFF_SOBJS,
    sizeof(SObj),
    MVOPENINGSTANDOFF_COBJS,
    sizeof(CObj),

    mvOpeningStandoffFuncStart
};

/* mvopeningstandoff.c:8013286C mvOpeningStandoffStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeningstandoff.h). */
void mvOpeningStandoffStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningStandoffTaskmanSetup);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[47].
 * The packs are given back first (mvopeningcliff.c does the same). */
void mvOpeningStandoffOverlayLoad(void)
{
#ifndef FT_HOSTTEST
    s32 i;

    for (i = 0; i < MVOPENINGSTANDOFF_MODEL_COUNT; i++)
    {
        fighter_release(&sMVOpeningStandoffPacks[i]);
    }
#endif
    OVERLAY_CLEAR(sMVOpeningStandoffTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningStandoffWallpaperScrollSpeed);
    OVERLAY_CLEAR(sMVOpeningStandoffUnused0x801329DC);
    OVERLAY_CLEAR(sMVOpeningStandoffWallpaperBank);
    OVERLAY_CLEAR(sMVOpeningStandoffWallpaperFileHead);
    OVERLAY_CLEAR(sMVOpeningStandoffCamAnimBank);
    OVERLAY_CLEAR(sMVOpeningStandoffPacks);
    OVERLAY_CLEAR(sMVOpeningStandoffModels);
    OVERLAY_CLEAR(sMVOpeningStandoffModelsLoaded);
    OVERLAY_CLEAR(sMVOpeningStandoffAnimJoints);
}
