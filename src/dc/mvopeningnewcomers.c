/* mvopeningnewcomers.c -- see mvopeningnewcomers.h. Function-for-function
 * from ssb-decomp-re/src/mv/mvopening/mvopeningnewcomers.c; the
 * substitutions are explained once, in mvopeningnewcomers.h's own header
 * comment -- not repeated at every site.
 */
#include "mvopeningnewcomers.h"
#include "overlay.h"

#include "fighter.h"
#include "objmodel.h"    /* dc_model_add_dobjs, dc_model_proc_display */
#include "ftcommon.h"
#include "lbcommon.h"   /* lbCommonDrawSprite, lbCommonSpriteFillRect */
#include "objpvr.h"     /* gcGetDrawList */
#include "scmanager.h"
#include "efmanager.h"
#include "taskman.h"

#ifndef FT_HOSTTEST
#include <dc/pvr.h>
#endif

#include <ft/fighter.h>
#include <ft/ftdef.h>
#include <gm/gmsound.h>
#include <lb/lbdef.h>
#include <sc/scsubsys/scsubsys.h>  /* scSubsysFighterSetLightParams */
#include <sys/controller.h>
#include <sys/objanim.h>           /* gcAddDObjAnimJoint */
#include <sys/objdef.h>
#include <sys/rdp.h>

/* ---- the pools -------------------------------------------------------- */
#define MVOPENINGNEWCOMERS_GOBJS       32
#define MVOPENINGNEWCOMERS_GOBJPROCS   32
#define MVOPENINGNEWCOMERS_XOBJS       64
#define MVOPENINGNEWCOMERS_AOBJS      128
#define MVOPENINGNEWCOMERS_MOBJS        8
#define MVOPENINGNEWCOMERS_DOBJS       32
#define MVOPENINGNEWCOMERS_SOBJS        4
#define MVOPENINGNEWCOMERS_COBJS        8

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

static s32 sMVOpeningNewcomersTotalTimeTics;
static s32 sMVOpeningNewcomersOverlayAlpha;
static u16 sMVOpeningNewcomersCharacterMask;
static s32 sMVOpeningNewcomersUnused0x80132754;

/* The four silhouettes as packs, in the order the game makes them, their
 * instances, and each one's script rebuilt from its pack
 * (gcAddDObjAnimJoint wants the pointer the file held). */
enum
{
    MVOPENINGNEWCOMERS_MODEL_PURIN,
    MVOPENINGNEWCOMERS_MODEL_CAPTAIN,
    MVOPENINGNEWCOMERS_MODEL_LUIGI,
    MVOPENINGNEWCOMERS_MODEL_NESS,
    MVOPENINGNEWCOMERS_MODEL_COUNT
};

#ifndef FT_HOSTTEST
/* Show, then Hidden, per silhouette. */
static const char *const sMVOpeningNewcomersModelNames[MVOPENINGNEWCOMERS_MODEL_COUNT][2] =
{
    { "mvopeningncpurin.mdl",   "mvopeningncpurinhide.mdl" },
    { "mvopeningnccaptain.mdl", "mvopeningnccaptainhide.mdl" },
    { "mvopeningncluigi.mdl",   "mvopeningncluigihide.mdl" },
    { "mvopeningncness.mdl",    "mvopeningncnesshide.mdl" }
};
#endif

static Fighter sMVOpeningNewcomersPacks[MVOPENINGNEWCOMERS_MODEL_COUNT];
static Fighter sMVOpeningNewcomersModels[MVOPENINGNEWCOMERS_MODEL_COUNT];
static sb32 sMVOpeningNewcomersModelsLoaded;
static AObjEvent32 *sMVOpeningNewcomersScripts[MVOPENINGNEWCOMERS_MODEL_COUNT];

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

/* mvopeningnewcomers.c:80131B00 mvOpeningNewcomersFuncLights. Not ported
 * -- the standing rule. func_lights is NULL below. */

/* The white screen (mvopeningnewcomers.h): the PVR's background plane. */
static void mvOpeningNewcomersSetBackground(f32 level)
{
#ifndef FT_HOSTTEST
    pvr_set_bg_color(level, level, level);
#else
    (void)level;
#endif
}

/* mvopeningnewcomers.c:80131B58 mvOpeningNewcomersCheckLocked, verbatim. */
sb32 mvOpeningNewcomersCheckLocked(s32 fkind)
{
    switch (fkind)
    {
    case nFTKindCaptain:
        return (sMVOpeningNewcomersCharacterMask & LBBACKUP_MASK_FIGHTER(nFTKindCaptain)) ? FALSE : TRUE;

    case nFTKindNess:
        return (sMVOpeningNewcomersCharacterMask & LBBACKUP_MASK_FIGHTER(nFTKindNess)) ? FALSE : TRUE;

    case nFTKindPurin:
        return (sMVOpeningNewcomersCharacterMask & LBBACKUP_MASK_FIGHTER(nFTKindPurin)) ? FALSE : TRUE;

    case nFTKindLuigi:
        return (sMVOpeningNewcomersCharacterMask & LBBACKUP_MASK_FIGHTER(nFTKindLuigi)) ? FALSE : TRUE;

    default:
        return FALSE;
    }
}

/* The four Make* functions of the game (Purin, Captain, Luigi, Ness) are
 * one body over a different display list and script; this is it. The
 * Show or Hidden pick is made where the pack is loaded (FuncStart).
 * DIVERGES: the DObj tree is the pack's, the display proc is
 * dc_model_proc_display (gcDrawDObjDLHead1's walk), and the pack build is
 * FT_HOSTTEST-guarded (the host links no renderer and cannot walk a
 * script). */
static void mvOpeningNewcomersMakeModel(s32 id)
{
    GObj *gobj = gcMakeGObjSPAfter(0, NULL, 19, GOBJ_PRIORITY_DEFAULT);

#ifndef FT_HOSTTEST
    if (sMVOpeningNewcomersModelsLoaded != FALSE)
    {
        fighter_clone(&sMVOpeningNewcomersModels[id], &sMVOpeningNewcomersPacks[id],
                      syTaskmanMalloc(sizeof(float[4]) *
                                      sMVOpeningNewcomersPacks[id].hd->vert_count, 0x8));
        dc_model_add_dobjs(gobj, NULL, &sMVOpeningNewcomersModels[id], NULL);
    }
#else
    gcAddDObjRpyR(gobj, NULL);
#endif
    gcAddGObjDisplay(gobj, dc_model_proc_display, 27, GOBJ_PRIORITY_DEFAULT, ~0);

#ifndef FT_HOSTTEST
    if ((sMVOpeningNewcomersModelsLoaded != FALSE) && (sMVOpeningNewcomersScripts[id] != NULL))
    {
        gcAddDObjAnimJoint(DObjGetStruct(gobj), sMVOpeningNewcomersScripts[id], 0.0F);
    }
    gcAddGObjProcess(gobj, gcPlayAnimAll, nGCProcessKindFunc, 1);
#endif
}

/* mvopeningnewcomers.c:80131C38-80131F08 mvOpeningNewcomersMakePurin,
 * MakeCaptain, MakeLuigi and MakeNess, in the order MakeAll calls them. */
void mvOpeningNewcomersMakeAll(void)
{
    mvOpeningNewcomersMakeModel(MVOPENINGNEWCOMERS_MODEL_PURIN);
    mvOpeningNewcomersMakeModel(MVOPENINGNEWCOMERS_MODEL_CAPTAIN);
    mvOpeningNewcomersMakeModel(MVOPENINGNEWCOMERS_MODEL_LUIGI);
    mvOpeningNewcomersMakeModel(MVOPENINGNEWCOMERS_MODEL_NESS);
}

/* mvopeningnewcomers.c:80132030 mvOpeningNewcomersHideProcDisplay.
 * DIVERGES: the black-out is a gDPFillRectangle under G_CC_PRIMITIVE,
 * and the raw GBI lands in the write-only scratch heads
 * (src/dc/wpmanager.c; the head index only ever chose a scratch row),
 * so it is the fill quad src/dc/ifscreenflash.c draws instead,
 * translucent pass only -- and the alpha ramp runs once a frame with
 * it, as the N64's one-pass draw had it. */
void mvOpeningNewcomersHideProcDisplay(GObj *gobj)
{
    (void)gobj;

    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    if (sMVOpeningNewcomersOverlayAlpha < 0xFF)
    {
        sMVOpeningNewcomersOverlayAlpha += 0x28;

        if (sMVOpeningNewcomersOverlayAlpha > 0xFF)
        {
            sMVOpeningNewcomersOverlayAlpha = 0xFF;
        }
    }
    lbCommonSpriteFillRect(10, 10, 310, 230, 0x00, 0x00, 0x00, sMVOpeningNewcomersOverlayAlpha);
}

/* mvopeningnewcomers.c:80132164 mvOpeningNewcomersMakeHide, verbatim. */
void mvOpeningNewcomersMakeHide(void)
{
    sMVOpeningNewcomersOverlayAlpha = 0x00;
    gcAddGObjDisplay
    (
        gcMakeGObjSPAfter
        (
            0,
            NULL,
            18,
            GOBJ_PRIORITY_DEFAULT
        ),
        mvOpeningNewcomersHideProcDisplay,
        26,
        GOBJ_PRIORITY_DEFAULT,
        ~0
    );
}

/* mvopeningnewcomers.c:801321B8 mvOpeningNewcomersMakeNewcomersCamera,
 * verbatim. */
void mvOpeningNewcomersMakeNewcomersCamera(void)
{
    CObj *cobj = CObjGetStruct
    (
        gcMakeCameraGObj
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
        )
    );
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->vec.eye.x = 45.36104F;
    cobj->vec.eye.y = 19.91594F;
    cobj->vec.eye.z = 15494.226F;

    cobj->vec.at.x = -109.73612F;
    cobj->vec.at.y = 257.7266F;
    cobj->vec.at.z = -14.981689F;

    cobj->vec.up.x = 0.0F;
    cobj->vec.up.y = 1.0F;
    cobj->vec.up.z = 0.0F;

    cobj->projection.persp.fovy = 2.864789F;
    cobj->projection.persp.near = 128.0F;
    cobj->projection.persp.far = 16384.0F;
}

/* mvopeningnewcomers.c:801322E8 mvOpeningNewcomersMakeHideCamera,
 * verbatim. */
void mvOpeningNewcomersMakeHideCamera(void)
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
}

/* mvopeningnewcomers.c:80132388 mvOpeningNewcomersInitVars, verbatim. */
void mvOpeningNewcomersInitVars(void)
{
    sMVOpeningNewcomersTotalTimeTics = 0;
    sMVOpeningNewcomersCharacterMask = gSCManagerBackupData.fighter_mask;
}

/* mvopeningnewcomers.c:801323A4 mvOpeningNewcomersFuncRun. DIVERGES: both
 * exits put the PVR's background back to black first (the header). */
void mvOpeningNewcomersFuncRun(GObj *gobj)
{
    (void)gobj;

    sMVOpeningNewcomersTotalTimeTics++;

    if (sMVOpeningNewcomersTotalTimeTics >= 10)
    {
        if (sMVOpeningNewcomersUnused0x80132754 != 0)
        {
            sMVOpeningNewcomersUnused0x80132754--;
        }
        if ((scSubsysControllerGetPlayerStickInRangeLR(-15, 15) != FALSE) && (scSubsysControllerGetPlayerStickInRangeUD(-15, 15) != FALSE))
        {
            sMVOpeningNewcomersUnused0x80132754 = 0;
        }
        if (scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            mvOpeningNewcomersSetBackground(0.0F);
            syTaskmanSetLoadScene();
        }
        if (sMVOpeningNewcomersTotalTimeTics == 30)
        {
            mvOpeningNewcomersMakeHide();
        }
        if (sMVOpeningNewcomersTotalTimeTics == 40)
        {
            gSCManagerSceneData.scene_prev = gSCManagerSceneData.scene_curr;
            gSCManagerSceneData.scene_curr = nSCKindTitle;

            mvOpeningNewcomersSetBackground(0.0F);
            syTaskmanSetLoadScene();
        }
    }
}

/* mvopeningnewcomers.c:80132490 mvOpeningNewcomersFuncStart. DIVERGES:
 * the reloc loader is the pack loads (mvopeningnewcomers.h), the default
 * camera's FILLCOLOR is dropped and the PVR's background plane is
 * white in its place, and the busy-wait is cut (5) -- the two sounds it
 * preceded play at its place. */
void mvOpeningNewcomersFuncStart(void)
{
    gcMakeGObjSPAfter(0, mvOpeningNewcomersFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
    gcMakeDefaultCameraGObj(0, GOBJ_PRIORITY_DEFAULT, 100, COBJ_FLAG_ZBUFFER, GPACK_RGBA8888(0xFF, 0xFF, 0xFF, 0xFF));

    mvOpeningNewcomersInitVars();
    mvOpeningNewcomersSetBackground(1.0F);

#ifndef FT_HOSTTEST
    {
        static const s32 fkinds[MVOPENINGNEWCOMERS_MODEL_COUNT] =
        {
            nFTKindPurin, nFTKindCaptain, nFTKindLuigi, nFTKindNess
        };
        int pal_bank = 0;
        s32 i;

        sMVOpeningNewcomersModelsLoaded = TRUE;
        for (i = 0; i < MVOPENINGNEWCOMERS_MODEL_COUNT; i++)
        {
            const char *name = sMVOpeningNewcomersModelNames[i][mvOpeningNewcomersCheckLocked(fkinds[i]) != FALSE];
            const Fighter *f = &sMVOpeningNewcomersPacks[i];

            if (fighter_load_scene(&sMVOpeningNewcomersPacks[i], name, &pal_bank) != 0)
            {
                sMVOpeningNewcomersModelsLoaded = FALSE;
                break;
            }
            sMVOpeningNewcomersScripts[i] = NULL;
            if (f->hd->anim_count != 0)
            {
                const FPackAnim *anim = &f->anims[0];
                const s32 *entries = (const s32 *)((const u8 *)f->blob + anim->off_entries);

                if (entries[0] >= 0 && (u32) entries[0] < anim->nwords)
                {
                    sMVOpeningNewcomersScripts[i] = (AObjEvent32 *)((u8 *)f->blob + anim->off_words) + entries[0];
                }
            }
        }
    }
#endif

    mvOpeningNewcomersMakeNewcomersCamera();
    mvOpeningNewcomersMakeHideCamera();
    mvOpeningNewcomersMakeAll();

    scSubsysFighterSetLightParams(0.0F, 0.0F, 0xFF, 0xFF, 0xFF, 0xFF);

    func_800269C0_275C0(nSYAudioFGMOpeningNewcomersClash);
    func_800269C0_275C0(nSYAudioVoiceAnnounceTitleWait);

    /* mvopening*.c: the decomp spins here until the retrace count reaches
     * 4155 (since the Room's BGM start); see sySchedulerWaitTicCount. */
    sySchedulerWaitTicCount(4155);
}

/* mvopeningnewcomers.c:80132694-shaped dMVOpeningNewcomersTaskmanSetup.
 * See mvopeningportraits.h for func_draw (scManagerFuncDraw), the zeroed
 * RSP/RDP fields and the pre-render/controller/matrix-list
 * substitutions, and mvopeningnewcomers.h for the pools. */
SYTaskmanSetup dMVOpeningNewcomersTaskmanSetup =
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
    MVOPENINGNEWCOMERS_GOBJPROCS,
    MVOPENINGNEWCOMERS_GOBJS,
    sizeof(GObj),
    MVOPENINGNEWCOMERS_XOBJS,
    NULL,
    NULL,
    MVOPENINGNEWCOMERS_AOBJS,
    MVOPENINGNEWCOMERS_MOBJS,
    MVOPENINGNEWCOMERS_DOBJS,
    sizeof(DObj),
    MVOPENINGNEWCOMERS_SOBJS,
    sizeof(SObj),
    MVOPENINGNEWCOMERS_COBJS,
    sizeof(CObj),

    mvOpeningNewcomersFuncStart
};

/* mvopeningnewcomers.c:801325E0 mvOpeningNewcomersStartScene. DIVERGES:
 * the standard video/arena cuts (mvopeningnewcomers.h). */
void mvOpeningNewcomersStartScene(void)
{
    syTaskmanStartTask(&dMVOpeningNewcomersTaskmanSetup);
}

/* The port's own bzero arm, src/dc/overlay.c's dSCManagerOverlays[52].
 * The packs are given back first (mvopeningcliff.c does the same). */
void mvOpeningNewcomersOverlayLoad(void)
{
#ifndef FT_HOSTTEST
    s32 i;

    for (i = 0; i < MVOPENINGNEWCOMERS_MODEL_COUNT; i++)
    {
        fighter_release(&sMVOpeningNewcomersPacks[i]);
    }
#endif
    OVERLAY_CLEAR(sMVOpeningNewcomersTotalTimeTics);
    OVERLAY_CLEAR(sMVOpeningNewcomersOverlayAlpha);
    OVERLAY_CLEAR(sMVOpeningNewcomersCharacterMask);
    OVERLAY_CLEAR(sMVOpeningNewcomersUnused0x80132754);
    OVERLAY_CLEAR(sMVOpeningNewcomersPacks);
    OVERLAY_CLEAR(sMVOpeningNewcomersModels);
    OVERLAY_CLEAR(sMVOpeningNewcomersModelsLoaded);
    OVERLAY_CLEAR(sMVOpeningNewcomersScripts);
}
