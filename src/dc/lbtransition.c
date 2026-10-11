/* lbtransition.c -- lb/lbtransition.c, the eleven screen wipes.
 *
 * The game's version is 239 lines and does five things: it photocopies
 * the last frame drawn into a heap (lbTransitionSetupTransition), makes a
 * camera to draw under (lbTransitionMakeCamera), loads one of eleven
 * relocData models and hangs its AnimJoint animation on it
 * (lbTransitionMakeTransition), binds the photocopy as segment 1 and
 * draws the model (lbTransitionProcDisplay), and ejects the GObj when the
 * animation runs out (lbTransitionProcUpdate). The picture on the model
 * is the frame you were just looking at, which is why the wipe looks like
 * the screen itself folding up.
 *
 * Only mn/mnvsmode/mnvsresults.c:3362-3364 uses it in the retail game
 * (db/dbcube.c is the debug menu): the VS results screen wipes in with a
 * transition rolled at random out of dLBTransitionDescs.
 *
 * DIVERGES, in three places, all of them the same seam the rest of the
 * port has:
 *
 *  - the model. The game loads a relocData file into a heap and calls
 *    gcSetupCustomDObjs on the DObjDesc inside it; the port has no
 *    runtime display-list interpreter for models, so tools/
 *    ssb_transexport.py bakes each of the eleven at build time and this
 *    loads the .tra pack and builds the same tree with
 *    dc_model_add_dobjs. The tree, the joint order and the AnimJoint
 *    animation are the file's own either way.
 *  - the picture. There is no segment 1 on this target and no
 *    gSPSegment: the photocopy is a PVR texture that the pack's one
 *    FPACK_TEX_EXTERN batch is bound to at load (fighter.c
 *    fighter_set_extern_texture), so lbTransitionProcDisplay has nothing
 *    to bind and is the port's model display proc instead.
 *  - the copy itself. There is none: the frame loop renders the
 *    battle's last frame into a texture (taskman.h, the photo), and the
 *    UVs the exporter baked 0..1 over the game's 300x220 are windowed
 *    onto that frame's middle (fighter_set_extern_window).
 *
 * The three file-scope statics have no xxxOverlayLoad and want none:
 * this file is in overlay 0, which scManagerRunLoop loads once at boot
 * (scmanager.c:826) and no scene reloads (tools/check/overlay_check.py says
 * the same). What gives the pack and the picture back is the eject at
 * the end of the wipe, which is where the game gets them back too --
 * its heap is the scene's.
 */
#include <lb/lbtypes.h>       /* LBTransitionDesc, for the header below */
#include <lb/lbtransition.h>

#include "fighter.h"
#include "objmodel.h"
#include "objpvr.h"

#include <sys/debug.h>
#include <sys/obj.h>
#include <sys/rdp.h>
#include <sys/utils.h>
#include <sys/video.h>
#include <config.h>              /* GS_SCREEN_WIDTH_DEFAULT */
#include <macros.h>              /* ARRAY_COUNT */

#include <dc/pvr.h>
#include "taskman.h"          /* syTaskmanGetPhoto */
#include <string.h>

/* The host cross-test links the scene, not the renderer: src/dc/fighter.c
 * is not in that build and there is no photo. What is left is the half
 * the test can see -- the GObj, its camera, its process and the eject --
 * and the model half is compiled out, the way src/dc/objmodel.c compiles
 * its draw out. */

/* lbtransition.c:15 dLBTransitionDescs, under another name because the
 * decomp header declares that one as LBTransitionDesc[11] and a row here
 * is not the same thing. Each row there is a relocData
 * file id, a DObjDesc offset and an AnimJoint offset; here the pack
 * carries the last two, so the row is the pack alone. The order is the
 * decomp's, because the id mnVSResultsFuncStart rolls indexes it
 * (syUtilsRandIntRange(ARRAY_COUNT(dLBTransitionDescs))). The comment is
 * the decomp's own name for the effect. */
static const char *const dLBTransitionPacks[] =
{
    "aeroplane.tra",        /* Paper Plane */
    "check.tra",            /* Checkered Board */
    "gakubuthi.tra",        /* Falling Board */
    "kannon.tra",           /* Doors */
    "star.tra",             /* Star */
    "sudare1.tra",          /* Vertical Lines */
    "sudare2.tra",          /* Diagonal Lines */
    "camera.tra",           /* Camera Shutter */
    "block.tra",            /* Collapsing Blocks */
    "rotscale.tra",         /* Rotating Frame Zooming Out */
    "curtain.tra",          /* Curtain */
};

// 0x800D6484
/* sLBTransitionFileHeap: the one transition's model. The game's is the
 * raw file in a heap the size of the largest of the eleven; the port's
 * is the loaded pack, released with the GObj. */
static Fighter sLBTransitionFileHeap;
static sb32 sLBTransitionIsLoaded;

// 0x800D6488 - Heap for "photocopy" of last frame drawn to framebuffer
/* Here the frame loop's photo (taskman.h), which it owns: TRUE while it
 * is bound to the pack's extern texture. */
static sb32 sLBTransitionPhotoHeap;

/* lbtransition.c:196-239 0x800D4404. Same job, different pixels: the
 * game's double loop reads 150 u32 per row straight out of
 * gSYSchedulerCurrentFramebuffer. Here the battle's last frame is already
 * a texture -- the frame loop rendered it there on the battle's last tic
 * (taskman.h syTaskmanGetPhoto, asked for by src/dc/scmanager.c) -- so
 * nothing is copied: the texture is bound as the pack's extern one, with
 * the window that puts the game's 300x220 middle under the UVs
 * tools/export/ssb_transexport.py normalised against it. The texture is
 * rendered to the pack's PVR headers before display.
 *
 * It is called before lbTransitionMakeTransition on purpose, in the game
 * because the heap has to exist first and here because a pack's PVR
 * headers are compiled at load and need the texture bound. No photo
 * binds nothing, and the pack then refuses to load (fighter.c), so the
 * results screen comes up without a wipe. */
#ifdef FT_HOSTTEST
void lbTransitionSetupTransition(void)
{
}
#else
void lbTransitionSetupTransition(void)
{
    s32 w, h;
    f32 win[4];
    void *photo = syTaskmanGetPhoto(&w, &h);

    sLBTransitionPhotoHeap = FALSE;

    if (photo == NULL)
    {
        syDebugPrintf("lbTransition: no photo of the last frame\n");
        return;
    }
    syTaskmanPhotoWindow(w, h, win);

    fighter_set_extern_texture(photo, PVR_TXRFMT_RGB565 |
                               PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_X32_STRIDE,
                               SY_TASKMAN_PHOTO_TEXW, SY_TASKMAN_PHOTO_TEXH);
    /* Bottom row first. The game's loop starts at framebuffer row 230
     * and steps UP a row each time (framebuffer_pixels -= 310 after 150
     * words), so the heap's row r is the screen's row 230 - r, and the
     * wipes' baked UVs put t = 0 at the bottom of the screen. So v runs
     * up the photo from game row 231 (the loop's one-row slip past the
     * border is kept), which the top-down copy this replaced never did:
     * the picture folded away upside down. */
    fighter_set_extern_window(win[0],
                              (f32)(h * 231 / 240) / SY_TASKMAN_PHOTO_TEXH,
                              win[2], -win[3]);
    sLBTransitionPhotoHeap = TRUE;
}
#endif /* !FT_HOSTTEST */

/* lbtransition.c:129-146 0x800D4130, verbatim apart from the camera's
 * far plane, which the game leaves at the CObj default. */
GObj* lbTransitionMakeCamera(u32 id, s32 link, u32 link_priority, u64 camera_mask)
{
    GObj *gobj;
    CObj *cobj;

    gobj = gcMakeGObjSPAfter(id, NULL, link, GOBJ_PRIORITY_DEFAULT);
    func_80009F74(gobj, func_80017DBC, link_priority, camera_mask, -1);

    cobj = gcAddCameraForGObj(gobj);
    gcAddXObjForCamera(cobj, nGCMatrixKindPerspFastF, 1);
    gcAddXObjForCamera(cobj, nGCMatrixKindLookAt, 1);
    syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);

    cobj->projection.persp.aspect = 15.0F / 11.0F;
    cobj->projection.persp.fovy = 45.0F;

    cobj->vec.eye.z = 1100.0F / syUtilsTan(F_CLC_DTOR32(cobj->projection.persp.fovy * 0.5F));

    cobj->flags |= COBJ_FLAG_DLBUFFERS | COBJ_FLAG_ZBUFFER;

    return gobj;
}

/* lbtransition.c:148-156 0x800D4248.
 *
 * DIVERGES: the game's three commands are a pipe sync, the gSPSegment
 * that puts the photocopy on segment 1 and another pipe sync, around
 * gcDrawDObjTreeForGObj. There is no segment here -- the picture is
 * already bound to the pack's one batch -- and the tree walk is the
 * port's model display proc, which does the same walk and then submits
 * the baked batches. Layered, because the wipe is the last thing over a
 * screen the results scene draws mostly in sprites. */
void lbTransitionProcDisplay(GObj *gobj)
{
    dc_model_proc_display_layered(gobj);
}

/* lbtransition.c:158-166 0x800D42C8, verbatim -- except that ejecting
 * the GObj is also where the port gives back the pack and the picture,
 * which the game gets back with the scene heap. */
void lbTransitionProcUpdate(GObj *gobj)
{
    gcPlayAnimAll(gobj);

    if (gobj->anim_frame <= 0.0F)
    {
        gcEjectGObj(gobj);

#ifndef FT_HOSTTEST
        if (sLBTransitionIsLoaded)
        {
            fighter_release(&sLBTransitionFileHeap);
            sLBTransitionIsLoaded = FALSE;
        }
        if (sLBTransitionPhotoHeap)
        {
            /* the photo is the frame loop's to free (taskman.h) */
            sLBTransitionPhotoHeap = FALSE;
            fighter_set_extern_texture(NULL, 0, 0, 0);
        }
#endif
    }
}

/* lbtransition.c:168-194 0x800D430C. The game's shape, one line at a
 * time:
 *
 *   lbRelocGetExternHeapFile     -> fighter_load of the baked pack
 *   gcMakeGObjSPAfter            -> the same
 *   gobj->user_data.s            -> the desc's fourth field is 0 in all
 *                                   eleven rows and nothing reads it
 *   gcAddGObjDisplay             -> the same
 *   gcSetupCustomDObjs           -> dc_model_add_dobjs, which builds one
 *                                   DObj per pack joint in the DObjDesc
 *                                   tree's own order and shape
 *   gcAddAnimJointAll            -> the loop below: the pack's animation
 *                                   carries a word index per joint in
 *                                   that same order, so stepping the
 *                                   table with the tree walk and calling
 *                                   gcAddDObjAnimJoint is what the game's
 *                                   one call does (sys/objanim.c:165).
 *                                   The animation's own pointers were
 *                                   relocated by fighter_init.
 *   gcPlayAnimAll                -> the same
 *   gcAddGObjProcess             -> the same
 */
GObj* lbTransitionMakeTransition(s32 transition_id, u32 id, s32 link, void (*proc_display)(GObj*), u8 dl_link_id, void (*func)(GObj*))
{
    GObj *gobj;

    if (transition_id < 0 ||
        transition_id >= (s32) ARRAY_COUNT(dLBTransitionPacks))
    {
        syDebugPrintf("lbTransitionMakeTransition: no transition %d\n",
                      (int)transition_id);
        return NULL;
    }
#ifndef FT_HOSTTEST
    {
        int pal_bank = 0;

        if (fighter_load(&sLBTransitionFileHeap,
                         dLBTransitionPacks[transition_id], &pal_bank) != 0)
        {
            return NULL;
        }
        sLBTransitionIsLoaded = TRUE;
    }
#endif
    gobj = gcMakeGObjSPAfter(id, NULL, link, GOBJ_PRIORITY_DEFAULT);

    gcAddGObjDisplay(gobj, proc_display, dl_link_id, GOBJ_PRIORITY_DEFAULT, ~0);

#ifndef FT_HOSTTEST
    {
        DObj *joints[FIGHTER_MAX_JOINTS];
        const FPackAnim *anim;
        const s32 *entries;
        s32 njoints;
        s32 i;

        njoints = dc_model_add_dobjs(gobj, NULL, &sLBTransitionFileHeap,
                                     joints);
        if (njoints <= 0)
        {
            return gobj;
        }
        anim = &sLBTransitionFileHeap.anims[0];
        entries = (const s32 *)((const u8 *)sLBTransitionFileHeap.blob +
                                anim->off_entries);

        for (i = 0; i < njoints; i++)
        {
            if (joints[i] != NULL && entries[i] >= 0 &&
                (u32) entries[i] < anim->nwords)
            {
                gcAddDObjAnimJoint(joints[i],
                                   (AObjEvent32 *)((u8 *)sLBTransitionFileHeap.blob +
                                                   anim->off_words) + entries[i],
                                   0.0F);
            }
        }
    }
#endif
    gcPlayAnimAll(gobj);

    gcAddGObjProcess(gobj, func, nGCProcessKindFunc, 1);

    return gobj;
}
