/* objpvr.h -- the port's own additions to the display back end.
 *
 * sys/objdisplay.h declares the game's display interface and the port
 * keeps every name in it (src/dc/objdisplay.c). What that interface has
 * no word for is the two things the Dreamcast side needs: what a DObj
 * points at where the N64 points at a Gfx*, and how the rest of the port
 * reaches the matrices a camera pass just established. Those are here.
 */
#ifndef SSB_DC_OBJPVR_H
#define SSB_DC_OBJPVR_H

#include <sys/obj.h>

#include "fighter.h"            /* DCViewport */

/* What DObj.dv holds here.
 *
 * On the N64 dv is a Gfx* and gcDrawDObjTree ends a DObj with
 * gSPDisplayList(dobj->dl): the matrix is already on the RSP's modelview
 * stack, and the display list draws against it. The PVR has no such
 * list, so dv points at one of these instead and the walk calls
 * proc_submit with the matrix it just composed. Keeping the callback in
 * the payload rather than in objdisplay.c is what lets the back end stay
 * ignorant of packs, sprites or anything else a DObj may come to mean --
 * exactly as the RSP is ignorant of what is in a display list.
 *
 * `model` and `joint` are what every payload so far needs (which pack,
 * which of its joints); a payload that needs more can put this struct
 * first and cast.
 */
typedef struct DCDisplay
{
    /* mtx is the DObj's composed model->world matrix, row-major
     * float[16] as everything in mtx.h. It is the walk's stack top and
     * is only valid for the duration of the call. */
    void (*proc_submit)(struct DCDisplay *disp, const float *mtx);
    /* gcDrawMObjForDObj's, where the N64 emits the MObj chain's colour
     * and texture GBI ahead of the display list. `chain` is the DObj's
     * live MObj list, never NULL when this is called. Left NULL by a
     * payload whose materials are all baked, which is every one but a
     * model with an FPackMObjs section (src/dc/objmodel.c). */
    void (*proc_material)(struct DCDisplay *disp, struct MObj *chain);
    void *model;
    s32 joint;
    /* The model part this payload draws (fighter.h FPackParts): a tag
     * the walk records as the joint's Fighter.part_cur, 0 for a joint
     * with none. ftParamSetModelPartID puts a part's payload on the DObj
     * where the game puts the part's display list. */
    s32 part;
} DCDisplay;

/* The camera matrices the running camera pass established, row-major
 * float[16]. Between passes they are the last camera's.
 *
 * The decomp has no equivalent: there the matrices go straight into the
 * display list and nothing reads them back. Here the port's own drawing
 * that is not yet a DObj tree -- a stage wallpaper, a debug overlay --
 * needs the same view and projection the tree walk is using, and this is
 * how it asks. */
const float *gcGetViewF(void);
const float *gcGetProjF(void);

/* One XObj kind's local matrix on its own, for the cross-test against
 * lb/lbcommon.c's matrix functions (src/dc/objdisplay.c has the note).
 * `m` is a mtx4_t; FALSE means the port has no such kind. */
sb32 gcDObjLocalMatrixF(float *m, DObj *dobj, s32 kind);

/* The other way: install a pair a display proc built itself, which is
 * what a gSPMatrix(&xobj->mtx, G_MTX_PROJECTION) from inside a display
 * proc does on the N64. The magnifying glass is the caller (if/ifcommon.c
 * loads its camera's two XObj matrices from the fighters' own display
 * procs, gm/gmcamera.c gmCameraLoadXObjMatrix). */
void gcSetCameraMatrixF(const float *view, const float *proj);

/* syMatrixLookAtF's transpose into the column-vector layout mtx.h uses:
 * the view a camera's LookAt XObj establishes, for a display proc that
 * builds one of its own (gm/gmcamera.c's magnify matrix functions). */
void gcMtxLookAt(float *m, f32 eye_x, f32 eye_y, f32 eye_z,
                 f32 at_x, f32 at_y, f32 at_z,
                 f32 up_x, f32 up_y, f32 up_z);

/* syMatrixModLookAtF transposed the same way: the look-at matrix kinds
 * 8-11 and 14-17 build, `roll` radians about the line of sight off the
 * fixed up axis (up_x, up_y, up_z). */
void gcMtxModLookAt(float *m, f32 eye_x, f32 eye_y, f32 eye_z,
                    f32 at_x, f32 at_y, f32 at_z, f32 roll,
                    f32 up_x, f32 up_y, f32 up_z);

/* sys/objdisplay.c func_80010918, matrix kinds 39 and 40, transposed the
 * same way: the basis that turns a DObj's geometry to face the camera,
 * with (kind 40) or without (kind 39) the DObj's own translate in it.
 * Taken apart from the kind dispatch so the host cross-test can drive
 * the arithmetic without a scene. */
void gcMtxBillboard(float *m, const Vec3f *at, const Vec3f *eye,
                    sb32 is_translate);

/* sys/objdisplay.c:822-856, matrix kind 44 (nGCMatrixKindRecalcRotRpyRSca),
 * the RSP billboard the shield bubble and Yoshi's egg are drawn with: at
 * world position `at`, the camera's axes for the DObj's own, scaled by
 * the running gGCScaleX (`scale_x`, updated as the N64 updates it) and
 * the DObj's scale. `view` is the pass's world-to-view matrix
 * (gcGetViewF). The walk installs it in place of the stack top rather
 * than multiplying it on. */
void gcMtxRecalcRotRpyRSca(float *m, const float *at, const float *view,
                           const Vec3f *scale, f32 *scale_x);

/* lb/lbcommon.c:1702 func_ovl0_800CA194, matrix kind 0x46: the same
 * billboard with an in-plane spin instead of a scale -- the camera's axes
 * turned about the view's own z by `rot_z`, at world position `at`, with
 * gGCScaleX untouched. The Motion-Sensor Bomb, the Master Ball, Saffron
 * City's Onix and Meowth, Marumine and two of efmanager.c's effects are
 * what ask for it. Installed in place of the stack top, as kind 44 is. */
void gcMtxRecalcRotZ(float *m, const float *at, const float *view,
                     f32 rot_z);

/* Matrix kinds 45/46 (rotate.x or rotate.z) and 0x47/0x48 (0x47 passes
 * NULL for scale and scale_x): see src/dc/objdisplay.c. */
void gcMtxRecalcRotSca(float *m, const float *at, const float *view,
                       f32 rot, const Vec3f *scale, f32 *scale_x);
void gcMtxRecalcRotRpy(float *m, const float *at, const float *view,
                       f32 rot_x, f32 rot_y, const Vec3f *scale,
                       f32 *scale_x);

/* Matrix kind 48: the camera's pass-time billboard with its yaw taken
 * out (sGCMatrixMod1F), scaled as 0x48 is. See src/dc/objdisplay.c. */
void gcMtxRecalcMod1(float *m, const float *at, const float *view,
                     const Vec3f *scale, f32 *scale_x);

/* One camera's projection times its view -- the matrix its pass would
 * establish -- into out[16], row-major, from its CObj alone. For code
 * that projects a world point to the screen itself, as
 * gm/gmcamera.c's gmCameraDefaultProcDisplay leaves gGMCameraMatrix
 * for ft/ftparam.c's projection to read. */
void gcCameraMatrixF(struct CObj *cobj, float *out);

/* The camera's viewport, in framebuffer pixels: centre and half-size,
 * as the camera pass establishes it from CObj.viewport (objdisplay.c
 * gcPrepCameraViewport, which is the decomp's func_8001663C reduced to
 * its arithmetic). Clip space maps onto it: x = cx + ndc_x * hw,
 * y = cy - ndc_y * hh -- what the RSP's viewport transform does with
 * vscale and vtrans, and what src/dc/fighter.c does per vertex. The
 * frame resets it to the whole framebuffer before the cameras run
 * (gcSetViewportFullScreen), so between scenes it is the screen, and
 * what a frame leaves outside the last camera's viewport it paints
 * black (src/dc/taskman.c) -- the N64's clear camera did that. */
const DCViewport *gcGetViewport(void);

/* The game's resolution, which is what syVideoInit sets on the N64:
 * 320x240 for every scene but the staff roll's 640x480. It sets
 * gDCScreenScale (lbcommon.h), the framebuffer pixels per game pixel,
 * which the sprite quads and gcPrepViewport scale by. */
void dcVideoSetResolution(s32 width, s32 height);
extern f32 gDCScreenScale;

/* The border-fill's real baseline: the union of every real camera's
 * viewport drawn so far this OP_POLY pass, not just the last one --
 * see objdisplay.c's own comment on gcExpandViewportUnion for why a
 * single "last camera" reading breaks once a scene opens more than one
 * real camera with different viewports in the same frame
 * (mv/mvopeningmario.c). syTaskmanCommonTaskDraw resets
 * this once a frame (gcResetViewportUnion); gcPrepCameraViewport
 * expands it for every camera drawn into PVR_LIST_OP_POLY
 * (gcExpandViewportUnion); src/dc/taskman.c's syTaskmanDrawBorder reads
 * it back (gcGetViewportUnion) in place of gcGetViewport. A frame with
 * no camera, or exactly one, ends with the same rectangle
 * gcGetViewport would have given, so this changes nothing for a
 * single-camera scene. */
void gcResetViewportUnion(void);
void gcExpandViewportUnion(void);
const DCViewport *gcGetViewportUnion(void);

/* The same claim for a camera that draws no 3D: a sprite camera's
 * viewport joins the union without becoming gcGetViewport's answer.
 * lbCommonDrawSprite calls it on the OP_POLY pass, the one the border
 * is painted on. */
void gcClaimViewport(const Vp_t *vp);

/* The screen rectangle one camera draws into, from its CObj viewport
 * clamped to the camera scissor -- gcGetViewport's answer for the rest of
 * the pass. func_80017D3C calls it for every camera it drives; a camera
 * whose scene gave it a display proc of its own has to call it itself,
 * and gm/gmcamera.c's arrows camera is the one that does.
 * The decomp's counterpart is func_8001663C, which is GBI. */
void gcPrepCameraViewport(struct CObj *cobj, s32 buffer_id);
void gcSetViewportFullScreen(void);

/* The same from a bare Vp -- gSPViewport from inside a display proc,
 * which is how the magnifying glass draws its fighter through an
 * 18-pixel square of its own (if/ifcommon.c
 * ifCommonPlayerMagnifyUpdateViewport). */
void gcPrepViewport(const Vp_t *vp);

/* Which PVR list the frame currently has open: PVR_LIST_OP_POLY,
 * PVR_LIST_PT_POLY or PVR_LIST_TR_POLY. The frame (src/dc/taskman.c
 * syTaskmanCommonTaskDraw) opens each list once and runs the scene's
 * whole draw inside it, so every camera and every display proc is called
 * once per list and submits only what belongs to the open one -- see the
 * note on the three passes in objdisplay.c. Code that belongs to exactly
 * one of them says so by returning early on the others, which is why
 * adding the punch-through pass did not have to touch any of it.
 * gcSetDrawList is the frame's to call. */
int gcGetDrawList(void);
void gcSetDrawList(int list);

/* gcEjectGObj(NULL) is NOT a no-op, and this is the port's fix for that.
 *
 * sys/objman.c:1780-1786: gcEjectGObj answers a NULL argument (or the
 * running GObj itself) by setting sGCRunStatus = nGCRunStatusEject and
 * returning -- and sys/objman.c:2132-2146 gcRunGObj reads that status
 * the moment the running GObj's func_run returns and ejects *that* GObj.
 * NULL is the object system's own "eject me" signal, not a nothing.
 *
 * On the N64 that never bites: a scene ejects props it really made. In
 * this port whole prop sets are cut (the relocData scene-graph gap the
 * mv/ scenes name), their GObj* statics stay NULL, and a verbatim
 * teardown line inside a func_run therefore kills the scene's own driver
 * GObj -- silently, with the rest of the scene still animating. Use this
 * instead of gcEjectGObj anywhere a GObj* may be NULL because the port
 * cut what would have filled it. Outside a run walk (scene setup and
 * teardown) plain gcEjectGObj(NULL) is still harmless; inside one it is
 * not, so prefer this everywhere rather than reasoning about context. */
void gcEjectGObjIfMade(struct GObj *gobj);

#ifdef SSB_NO_DRAW
/* The host cross-test drives the walk and never opens a list; the three
 * ids only have to be distinct. */
#define PVR_LIST_OP_POLY 0
#define PVR_LIST_PT_POLY 1
#define PVR_LIST_TR_POLY 2
#endif

#endif /* SSB_DC_OBJPVR_H */
