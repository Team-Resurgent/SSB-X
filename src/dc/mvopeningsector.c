/* mvopeningsector.c -- see mvopeningsector.h. Function-for-function from
 * ssb-decomp-re/src/mv/mvopening/mvopeningsector.c; the substitutions are
 * explained once, in mvopeningsector.h's own header comment -- not repeated
 * at every site.
 */
#include "mvopeningsector.h"
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

#include <gm/gmsound.h>
#include <sc/scsubsys/scsubsys.h>  /* scSubsysFighterSetStatus/SetLightParams */
#include <sys/controller.h>
#include <sys/objanim.h>           /* gcAddDObjAnimJoint, gcAddCObjCamAnimJoint */
#include <sys/objdef.h>
#include <sys/rdp.h>
#include <lb/lbdef.h>

/* The game reaches its sprites as a file's base plus a link label's
 * offset; the port's files are sprite banks and the label's value is the
 * offset (mvopeningyoster.c does the same). */
#undef lbRelocGetFileData
#define lbRelocGetFileData(type, file, offset) \
    ((type)sprite_bank_get((SpriteBank *)(file), (u32)(offset)))

/* relocData 73's cockpit and 74's wallpaper: llMVOpeningSectorCockpitSprite
 * and llMVOpeningSectorWallpaperSprite (src/dc/decomp/reloc_data.us.h). */
#define llMVOpeningSectorCockpitSprite   0x3CC90
#define llMVOpeningSectorWallpaperSprite 0x26C88

/* ---- the pools -------------------------------------------------------- */
#define MVOPENINGSECTOR_GOBJS        64
#define MVOPENINGSECTOR_GOBJPROCS    64
#define MVOPENINGSECTOR_XOBJS       384
#define MVOPENINGSECTOR_AOBJS       800
#define MVOPENINGSECTOR_MOBJS       160
#define MVOPENINGSECTOR_DOBJS       192
#define MVOPENINGSECTOR_SOBJS        16
#define MVOPENINGSECTOR_COBJS         8

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static f32 sMVOpeningSectorWallpaperScrollSpeedX;
static f32 sMVOpeningSectorWallpaperScrollSpeedY;
static s32 sMVOpeningSectorTotalTimeTics;
static s32 sMVOpeningSectorCockpitAlpha;
static s32 sMVOpeningSectorUnused0x80132A40;
static GObj *sMVOpeningSectorWallpaperGObj;
static GObj *sMVOpeningSectorGreatFoxGObj;
static GObj *sMVOpeningSectorArwingGObjs[3];

/* The port's stand-ins for sMVOpeningSectorFiles[0] (file 73, the cockpit)
 * and [2] (file 74, the wallpaper): one sprite each. Not static so
 * hosttest_ft.c can read them. */
static SpriteBank sMVOpeningSectorCockpitBank;
static SpriteBank sMVOpeningSectorWallpaperBank;
void *sMVOpeningSectorCockpitFileHead;
void *sMVOpeningSectorWallpaperFileHead;

/* The port's stand-in for the camera script in file 73. */
static CamAnimBank sMVOpeningSectorCamAnimBank;

/* The two models as packs: the Great Fox (file 73) and the Arwing (file
 * 161, FoxSpecial3, flown three times on three tables that live in file
 * 73 -- the pack carries them as its three animations). One instance per
 * GObj, and each one's table of per-joint scripts rebuilt from its
 * pack's animation (ifcommon.c's arrows do the same). */
enum
{
    MVOPENINGSECTOR_MODEL_GREATFOX,
    MVOPENINGSECTOR_MODEL_ARWING,
    MVOPENINGSECTOR_MODEL_COUNT
};
#define MVOPENINGSECTOR_ARWING_NUM 3
#define MVOPENINGSECTOR_INSTANCE_NUM (1 + MVOPENINGSECTOR_ARWING_NUM)

#ifndef FT_HOSTTEST
static const char *const sMVOpeningSectorModelNames[MVOPENINGSECTOR_MODEL_COUNT] =
{
    "mvopeningsectorgreatfox.mdl",
    "mvopeningsectorarwing.mdl"
};
#endif

static Fighter sMVOpeningSectorPacks[MVOPENINGSECTOR_MODEL_COUNT];
static Fighter sMVOpeningSectorModels[MVOPENINGSECTOR_INSTANCE_NUM];
static sb32 sMVOpeningSectorModelsLoaded;
static AObjEvent32 *sMVOpeningSectorAnimJoints[MVOPENINGSECTOR_INSTANCE_NUM][FIGHTER_MAX_JOINTS];

#ifndef FT_HOSTTEST
/* Instance `slot`'s table out of animation `anim_id` of pack `pack_id`. */
static void mvOpeningSectorSetAnimJoint(s32 slot, s32 pack_id, s32 anim_id)
{
    const Fighter *f = &sMVOpeningSectorPacks[pack_id];
    AObjEvent32 **table = sMVOpeningSectorAnimJoints[slot];
    const FPackAnim *anim;
    const s32 *entries;
    u32 i;

    for (i = 0; i < FIGHTER_MAX_JOINTS; i++)
    {
        table[i] = NULL;
    }
    if ((u32)anim_id >= f->hd->anim_count)
    {
        return;
    }
    anim = &f->anims[anim_id];
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

/* The Make* functions below share this: the game's gcSetupCustomDObjs +
 * gcAddAnimJointAll + gcPlayAnimAll over a pack, instance `slot`.
 * DIVERGES: the tree comes from the pack, the table from its rebuilt
 * pointers, the display proc is dc_model_proc_display (the walk that
 * gcDrawDObjTreeDLLinksForGObj does), and the pack build and the attach
 * are FT_HOSTTEST-guarded (the host links no renderer and cannot walk a
 * script). */
static void mvOpeningSectorSetupModel(GObj *gobj, s32 slot, s32 pack_id)
{
#ifndef FT_HOSTTEST
    if (sMVOpeningSectorModelsLoaded != FALSE)
    {
        fighter_clone(&sMVOpeningSectorModels[slot], &sMVOpeningSectorPacks[pack_id],
                      syTaskmanMalloc(sizeof(float[4]) *
                                      sMVOpeningSectorPacks[pack_id].hd->vert_count, 0x8));
        dc_model_add_dobjs(gobj, NULL, &sMVOpeningSectorModels[slot], NULL);
    }
#else
    gcAddDObjRpyR(gobj, NULL);
#endif
    gcAddGObjDisplay(gobj, dc_model_proc_display, 27, GOBJ_PRIORITY_DEFAULT, ~0);

#ifndef FT_HOSTTEST
    if (sMVOpeningSectorModelsLoaded != FALSE)
    {
        gcAddAnimJointAll(gobj, sMVOpeningSectorAnimJoints[slot], 0.0F);
        gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
    }
#endif
}

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningsector.c:80131B00 mvOpeningSectorFuncLights. Not ported -- the
 * standing rule. func_lights is NULL below. */

/* mvopeningsector.c:80131B58 mvOpeningSectorWallpaperProcUpdate,
 * verbatim. */
void mvOpeningSectorWallpaperProcUpdate(GObj* wallpaper_gobj)
{
    SObj* wallpaper_sobj = SObjGetStruct(wallpaper_gobj);

    switch (sMVOpeningSectorTotalTimeTics)
    {
    case 1:
        sMVOpeningSectorWallpaperScrollSpeedX = 6.0F;
        break;

    case 120:
        sMVOpeningSectorWallpaperScrollSpeedX = 6.0F;
        break;

    case 140:
        sMVOpeningSectorWallpaperScrollSpeedX = 2.0F;
        break;

    case 160:
        sMVOpeningSectorWallpaperScrollSpeedX = 1.0F;
        break;
    }
    switch (sMVOpeningSectorTotalTimeTics)
    {
    case 1:
        sMVOpeningSectorWallpaperScrollSpeedY = 1.0F;
        break;

    case 120:
        sMVOpeningSectorWallpaperScrollSpeedY = 1; // yes, 1, not 1.0F
        break;

    case 160:
        sMVOpeningSectorWallpaperScrollSpeedY = 3.0F;
        break;
    }
    if ((sMVOpeningSectorTotalTimeTics > 1) && (sMVOpeningSectorTotalTimeTics < 120))
    {
        sMVOpeningSectorWallpaperScrollSpeedX += 0.0F;
    }
    if ((sMVOpeningSectorTotalTimeTics > 120) && (sMVOpeningSectorTotalTimeTics < 140))
    {
        sMVOpeningSectorWallpaperScrollSpeedX += -0.2F;
    }
    if ((sMVOpeningSectorTotalTimeTics > 140) && (sMVOpeningSectorTotalTimeTics < 160))
    {
        sMVOpeningSectorWallpaperScrollSpeedX += -0.05F;
    }
    if ((sMVOpeningSectorTotalTimeTics > 1) && (sMVOpeningSectorTotalTimeTics < 120))
    {
        sMVOpeningSectorWallpaperScrollSpeedY += 0.0F;
    }
    if ((sMVOpeningSectorTotalTimeTics > 120) && (sMVOpeningSectorTotalTimeTics < 160))
    {
        sMVOpeningSectorWallpaperScrollSpeedY += 0.05F;
    }
    wallpaper_sobj->pos.x += sMVOpeningSectorWallpaperScrollSpeedX;

    if (wallpaper_sobj->pos.x > 10.0F)
    {
        wallpaper_sobj->pos.x -= 300.0F;
    }
    if (wallpaper_sobj->pos.y < -220.0F)
    {
        wallpaper_sobj->pos.y += 220.0F;
    }
    wallpaper_sobj->next->pos.x = wallpaper_sobj->pos.x + 300.0F;
    wallpaper_sobj->next->pos.y = wallpaper_sobj->pos.y;

    wallpaper_sobj->next->next->pos.x = wallpaper_sobj->pos.x;
    wallpaper_sobj->next->next->pos.y = wallpaper_sobj->pos.y + 220.0F;

    wallpaper_sobj->next->next->next->pos.x = wallpaper_sobj->pos.x + 300.0F;
    wallpaper_sobj->next->next->next->pos.y = wallpaper_sobj->pos.y + 220.0F;
}

/* mvopeningsector.c:80131DEC mvOpeningSectorMakeWallpaper, verbatim but
 * for lbRelocGetFileData's own redefinition above. */
void mvOpeningSectorMakeWallpaper(void)
{
    GObj* wallpaper_gobj;
    SObj* wallpaper_sobj;
    static const f32 pos[4][2] = { { 10.0F, 10.0F }, { 310.0F, 10.0F }, { 10.0F, 230.0F }, { 310.0F, 230.0F } };
    s32 i;

    sMVOpeningSectorWallpaperGObj = wallpaper_gobj = gcMakeGObjSPAfter(0, NULL, 20, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(wallpaper_gobj, lbCommonDrawSObjAttr, 28, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(wallpaper_gobj, mvOpeningSectorWallpaperProcUpdate, nGCProcessKindFunc, 1);

    /* the decomp writes the four out; they differ only in position */
    for (i = 0; i < 4; i++)
    {
        wallpaper_sobj = lbCommonMakeSObjForGObj(wallpaper_gobj, lbRelocGetFileData(Sprite*, sMVOpeningSectorWallpaperFileHead, llMVOpeningSectorWallpaperSprite));
        wallpaper_sobj->sprite.attr &= ~SP_FASTCOPY;

        wallpaper_sobj->pos.x = pos[i][0];
        wallpaper_sobj->pos.y = pos[i][1];
    }
}

/* mvopeningsector.c:80131F4C mvOpeningSectorMakeGreatFox. */
void mvOpeningSectorMakeGreatFox(void)
{
    GObj* great_fox_gobj;

    sMVOpeningSectorGreatFoxGObj = great_fox_gobj = gcMakeGObjSPAfter(0, NULL, 19, GOBJ_PRIORITY_DEFAULT);
    mvOpeningSectorSetupModel(great_fox_gobj, 0, MVOPENINGSECTOR_MODEL_GREATFOX);

    DObjGetStruct(great_fox_gobj)->translate.vec.f.x = 0.0F;
    DObjGetStruct(great_fox_gobj)->translate.vec.f.y = 0.0F;
    DObjGetStruct(great_fox_gobj)->translate.vec.f.z = 0.0F;
}

/* mvopeningsector.c:8013202C mvOpeningSectorCockpitProcDisplay, verbatim
 * (the raw gDP writes are the port's standing shape for these procs,
 * mvopeningpikachu.c's posed wallpaper). */
void mvOpeningSectorCockpitProcDisplay(GObj *cockpit_gobj)
{
    SObj* cockpit_sobj = SObjGetStruct(cockpit_gobj);

    if (sMVOpeningSectorCockpitAlpha < 0xFF)
    {
        sMVOpeningSectorCockpitAlpha += 0x09;

        if (sMVOpeningSectorCockpitAlpha > 0xFF)
        {
            sMVOpeningSectorCockpitAlpha = 0xFF;
        }
    }
    gDPPipeSync(gSYTaskmanDLHeads[0]++);
    gDPSetCycleType(gSYTaskmanDLHeads[0]++, G_CYC_1CYCLE);
    gDPSetPrimColor(gSYTaskmanDLHeads[0]++, 0, 0, 0x00, 0x00, 0x00, sMVOpeningSectorCockpitAlpha);
    gDPSetEnvColor(gSYTaskmanDLHeads[0]++, cockpit_sobj->envcolor.r, cockpit_sobj->envcolor.g, cockpit_sobj->envcolor.b, cockpit_sobj->envcolor.a);
    gDPSetCombineLERP(gSYTaskmanDLHeads[0]++, 0, 0, 0, TEXEL0,  0, 0, 0, PRIMITIVE,  0, 0, 0, TEXEL0,  0, 0, 0, PRIMITIVE);
    gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);

    lbCommonDrawSObjNoAttr(cockpit_gobj);
}

/* mvopeningsector.c:8013215C mvOpeningSectorCockpitProcUpdate, verbatim. */
void mvOpeningSectorCockpitProcUpdate(GObj* cockpit_gobj)
{
    SObj* cockpit_sobj = SObjGetStruct(cockpit_gobj);
    f32 scale = cockpit_sobj->sprite.scalex;

    scale += 0.025F;

    if (scale > 1.0F)
    {
        scale = 1.0F;
    }
    cockpit_sobj->sprite.scalex = scale;
    cockpit_sobj->sprite.scaley = scale;

    cockpit_sobj->pos.x = 160.0F - ((320.0F * scale) / 2);
    cockpit_sobj->pos.y = 120.0F - ((240.0F * scale) / 2);
}

/* mvopeningsector.c:801321E0 mvOpeningSectorMakeCockpit, verbatim but for
 * lbRelocGetFileData's own redefinition above. */
void mvOpeningSectorMakeCockpit(void)
{
    GObj* cockpit_gobj;
    SObj* cockpit_sobj;

    sMVOpeningSectorCockpitAlpha = 0;

    cockpit_gobj = gcMakeGObjSPAfter(0, NULL, 21, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(cockpit_gobj, mvOpeningSectorCockpitProcDisplay, 29, GOBJ_PRIORITY_DEFAULT, ~0);
    gcAddGObjProcess(cockpit_gobj, mvOpeningSectorCockpitProcUpdate, nGCProcessKindFunc, 1);

    cockpit_sobj = lbCommonMakeSObjForGObj(cockpit_gobj, lbRelocGetFileData(Sprite*, sMVOpeningSectorCockpitFileHead, llMVOpeningSectorCockpitSprite));
    cockpit_sobj->sprite.attr &= ~SP_FASTCOPY;
    cockpit_sobj->sprite.attr |= SP_TRANSPARENT;

    cockpit_sobj->sprite.scalex = 0.25F;
    cockpit_sobj->sprite.scaley = 0.25F;
}

/* mvopeningsector.c:80132290 mvOpeningSectorMakeArwings. DIVERGES: the
 * three tables are the arwing pack's three animations (i), one per
 * GObj, instead of three offsets into file 73. */
void mvOpeningSectorMakeArwings(void)
{
    GObj* arwing_gobj;
    s32 i;

    for (i = 0; i < MVOPENINGSECTOR_ARWING_NUM; i++)
    {
        sMVOpeningSectorArwingGObjs[i] = arwing_gobj = gcMakeGObjSPAfter(0, NULL, 19, GOBJ_PRIORITY_DEFAULT);
        mvOpeningSectorSetupModel(arwing_gobj, 1 + i, MVOPENINGSECTOR_MODEL_ARWING);
    }
}

/* mvopeningsector.c:801323E0 mvOpeningSectorCameraProcUpdate, verbatim. */
void mvOpeningSectorCameraProcUpdate(GObj* camera_gobj)
{
    gcPlayCamAnim(camera_gobj);
}

/* mvopeningsector.c:80132400 mvOpeningSectorMakeMainCamera. DIVERGES: the
 * script is camanim_get(..., "Cam") and the attach-and-play pair is
 * FT_HOSTTEST-guarded. */
void mvOpeningSectorMakeMainCamera(void)
{
    GObj *camera_gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        40,
        COBJ_MASK_DLLINK(27),
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
    cobj->projection.persp.far = 30000.0F;

#ifndef FT_HOSTTEST
    gcAddCObjCamAnimJoint(cobj, camanim_get(&sMVOpeningSectorCamAnimBank, "Cam"), 0.0F);
    gcAddGObjProcess(camera_gobj, mvOpeningSectorCameraProcUpdate, nGCProcessKindFunc, 1);
#endif
}

/* mvopeningsector.c:80132500 mvOpeningSectorMakeWallpaperCamera.
 * DIVERGES: lbCommonDrawSpriteBackdrop for lbCommonDrawSprite -- this
 * camera's 90 puts the wallpaper under the Great Fox's camera (40) on
 * the N64, and the backdrop band is what puts it under the ships here
 * (src/dc/lbcommon.h). The cockpit camera (20) stays a front sprite
 * camera, over everything. */
void mvOpeningSectorMakeWallpaperCamera(void)
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

/* mvopeningsector.c:801325A0 mvOpeningSectorMakeCockpitCamera, verbatim. */
void mvOpeningSectorMakeCockpitCamera(void)
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
            COBJ_MASK_DLLINK(29),
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

/* mvopeningsector.c:80132640 mvOpeningSectorInitTotalTimeTics, verbatim. */
void mvOpeningSectorInitTotalTimeTics(void)
{
    sMVOpeningSectorTotalTimeTics = 0;
}

/* mvopeningsector.c:8013264C mvOpeningSectorFuncRun, verbatim. */
void mvOpeningSectorFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningSectorTotalTimeTics++;

    if (sMVOpeningSectorTotalTimeTics >= 10)
    {
        if (sMVOpeningSectorUnused0x80132A40 != 0)
        {
            sMVOpeningSectorUnused0x80132A40--;
        }
        if ((scSubsysControllerGetPlayerStickInRangeLR(-15, 15) != FALSE) && (scSubsysControllerGetPlayerStickInRangeUD(-15, 15) != FALSE))
        {
            sMVOpeningSectorUnused0x80132A40 = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            syTaskmanSetLoadScene();
        }
        if (sMVOpeningSectorTotalTimeTics == 120)
        {
            mvOpeningSectorMakeCockpit();
        }
        if (sMVOpeningSectorTotalTimeTics == 160)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindOpeningStandoff;

            syTaskmanSetLoadScene();
        }
    }
}

/* mvopeningsector.c:80132738 mvOpeningSectorFuncStart. DIVERGES: the
 * reloc loader is the bank/pack loads (mvopeningsector.h). The busy-wait is
 * sySchedulerWaitTicCount, and the ambient sound plays just before it. */
void mvOpeningSectorFuncStart(void)
{
    if (sprite_bank_load(&sMVOpeningSectorCockpitBank, "mvopeningsector.spr") < 0)
    {
        sMVOpeningSectorCockpitFileHead = NULL;
    }
    else
    {
        sMVOpeningSectorCockpitFileHead = &sMVOpeningSectorCockpitBank;
    }
    if (sprite_bank_load(&sMVOpeningSectorWallpaperBank, "mvopeningsectorwallpaper.spr") < 0)
    {
        sMVOpeningSectorWallpaperFileHead = NULL;
    }
    else
    {
        sMVOpeningSectorWallpaperFileHead = &sMVOpeningSectorWallpaperBank;
    }
    camanim_bank_load(&sMVOpeningSectorCamAnimBank, "mvopeningsector.cam");
#ifndef FT_HOSTTEST
    {
        int pal_bank = 0;
        s32 i;

        sMVOpeningSectorModelsLoaded = TRUE;
        for (i = 0; i < MVOPENINGSECTOR_MODEL_COUNT; i++)
        {
            if (fighter_load_scene(&sMVOpeningSectorPacks[i], sMVOpeningSectorModelNames[i], &pal_bank) != 0)
            {
                sMVOpeningSectorModelsLoaded = FALSE;
                break;
            }
        }
        if (sMVOpeningSectorModelsLoaded != FALSE)
        {
            mvOpeningSectorSetAnimJoint(0, MVOPENINGSECTOR_MODEL_GREATFOX, 0);
            for (i = 0; i < MVOPENINGSECTOR_ARWING_NUM; i++)
            {
                mvOpeningSectorSetAnimJoint(1 + i, MVOPENINGSECTOR_MODEL_ARWING, i);
            }
        }
    }
#endif

    gcMakeGObjSPAfter(0, mvOpeningSectorFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
    gcMakeDefaultCameraGObj(0, GOBJ_PRIORITY_DEFAULT, 100, COBJ_FLAG_ZBUFFER, GPACK_RGBA8888(0x00, 0x00, 0x00, 0x00));

    mvOpeningSectorInitTotalTimeTics();

    mvOpeningSectorMakeMainCamera();
    mvOpeningSectorMakeWallpaperCamera();
    mvOpeningSectorMakeCockpitCamera();
    mvOpeningSectorMakeWallpaper();
    mvOpeningSectorMakeGreatFox();
    mvOpeningSectorMakeArwings();

    scSubsysFighterSetLightParams(45.0F, 45.0F, 0xFF, 0xFF, 0xFF, 0xFF);

    func_800269C0_275C0(nSYAudioFGMOpeningSectorAmbient);

    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 3420 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(3420);
}

/* mvopeningsector.c:80132958-shaped mvOpeningSectorTaskmanSetup. See
 * mvopeningportraits.h for func_draw (scManagerFuncDraw), the zeroed
 * RSP/RDP fields and the pre-render/controller/matrix-list
 * substitutions. */
SYTaskmanSetup mvOpeningSectorTaskmanSetup =
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
    MVOPENINGSECTOR_GOBJPROCS,
    MVOPENINGSECTOR_GOBJS,
    sizeof(GObj),
    MVOPENINGSECTOR_XOBJS,
    NULL,
    NULL,
    MVOPENINGSECTOR_AOBJS,
    MVOPENINGSECTOR_MOBJS,
    MVOPENINGSECTOR_DOBJS,
    sizeof(DObj),
    MVOPENINGSECTOR_SOBJS,
    sizeof(SObj),
    MVOPENINGSECTOR_COBJS,
    sizeof(CObj),

    mvOpeningSectorFuncStart
};

/* mvopeningsector.c:80132604 mvOpeningSectorStartScene. DIVERGES: the
 * standard video/arena cuts (mvopeningsector.h). */
void mvOpeningSectorStartScene(void)
{
    syTaskmanStartTask(&mvOpeningSectorTaskmanSetup);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[50].
 * The packs are given back first (mvopeningcliff.c does the same). */
void mvOpeningSectorOverlayLoad(void)
{
#ifndef FT_HOSTTEST
    s32 i;

    for (i = 0; i < MVOPENINGSECTOR_MODEL_COUNT; i++)
    {
        fighter_release(&sMVOpeningSectorPacks[i]);
    }
#endif
    OVERLAY_CLEAR(sMVOpeningSectorWallpaperScrollSpeedX);
    OVERLAY_CLEAR(sMVOpeningSectorWallpaperScrollSpeedY);
    OVERLAY_CLEAR(sMVOpeningSectorTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningSectorCockpitAlpha);
    OVERLAY_CLEAR(sMVOpeningSectorUnused0x80132A40);
    OVERLAY_CLEAR(sMVOpeningSectorWallpaperGObj);
    OVERLAY_CLEAR(sMVOpeningSectorGreatFoxGObj);
    OVERLAY_CLEAR(sMVOpeningSectorArwingGObjs);
    OVERLAY_CLEAR(sMVOpeningSectorCockpitBank);
    OVERLAY_CLEAR(sMVOpeningSectorWallpaperBank);
    OVERLAY_CLEAR(sMVOpeningSectorCockpitFileHead);
    OVERLAY_CLEAR(sMVOpeningSectorWallpaperFileHead);
    OVERLAY_CLEAR(sMVOpeningSectorCamAnimBank);
    OVERLAY_CLEAR(sMVOpeningSectorPacks);
    OVERLAY_CLEAR(sMVOpeningSectorModels);
    OVERLAY_CLEAR(sMVOpeningSectorModelsLoaded);
    OVERLAY_CLEAR(sMVOpeningSectorAnimJoints);
}
