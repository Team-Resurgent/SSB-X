/* objdisplay.c -- the port's replacement for sys/objdisplay.c.
 *
 * objdisplay.c is the one file of the object system that does not come
 * across. Its 3394 lines are the GBI emitter: it walks the camera list
 * and the DObj trees, composes the matrix stack, and writes F3DEX2
 * commands into the task manager's display-list buffers. The walk is the
 * game's and the port keeps it -- function for function, in the same
 * order, off the same fields; the emission is the RDP's and the port
 * replaces it with PVR submission.
 *
 * So this is a rewrite behind a ported interface, like taskman.c: scene
 * code and the rest of the object system go on calling the names
 * sys/objdisplay.h declares -- which is why there is no header here, only
 * the decomp's and the port's own objpvr.h -- and what happens underneath
 * is the Dreamcast's. Each function below says which line of
 * objdisplay.c it stands in for.
 *
 * Three divergences run through the whole file and are not repeated at
 * every site:
 *
 * 1. THE MATRIX STACK IS OURS. The RSP holds a ten-deep modelview stack
 *    and gSPMatrix pushes, multiplies and loads into it; there is no RSP
 *    here, so the stack is the array below and gcPrepDObjMatrix drives it
 *    directly. It is deliberately ten deep, F3DEX2's G_MTX_STACKSIZE: a
 *    DObj tree that overflows it would have overflowed the N64's too, so
 *    this is the game's own limit and not a new one.
 *
 * 2. THREE PASSES, NOT FOUR BUFFERS. The N64 fills four display-list
 *    buffers in one walk -- opaque into 0 and 2, translucent into 1 and 3
 *    (func_8001663C picks the render mode from the buffer id) -- and the
 *    RSP runs them in order afterwards. The PVR cannot interleave its
 *    lists: a list is opened, filled and closed. So the walk itself runs
 *    once per list -- PVR_LIST_OP_POLY, then PVR_LIST_PT_POLY, then
 *    PVR_LIST_TR_POLY -- and a DObj's payload submits only the geometry
 *    belonging to the list that is open (gcGetDrawList). Everything the
 *    camera_mask/DL-link machinery decides -- which lists a camera draws,
 *    in which order -- is untouched; only the number of walks changes.
 *
 *    The buffer id is not what picks the list here. The four heads only
 *    ever say opaque or translucent, which cannot express the RDP's third
 *    case -- the alpha-tested cutout a stage's foliage is drawn with,
 *    G_RM_AA_ZB_TEX_EDGE -- and a stage whose layer_mask is 0 never
 *    reaches a second head at all. So the exporter reads the render mode
 *    itself and the batch carries the answer (fighter.h FPACK_LIST_*),
 *    with punch-through standing in for the cutout.
 *
 * 3. NO MATRIX CACHING. XObj.unk05 and gSYTaskmanGraphicsHeap between
 *    them decide, in about 150 lines of gcPrepDObjMatrix, whether a
 *    DObj's Mtx is recomputed this frame or reused from the XObj because
 *    the RSP still has last frame's copy. That is a DMA-and-double-buffer
 *    optimisation for a coprocessor reading memory the CPU is writing.
 *    Nothing here is read by anyone else, so every matrix is composed
 *    fresh and those lines are gone.
 */
#include <sys/obj.h>
#include <sys/debug.h>
#include <sys/video.h>
#include <sys/vector.h>      /* syVectorRotateAbout3D, gcMtxModLookAt */

#include <config.h>
#include <macros.h>

#include <string.h>

#include <gm/gmcollision.h>
#include <ft/fighter.h>         /* FTParts, for matrix kind 0x4F */

#include "mtx.h"
#include "objmodel.h"
#include "objpvr.h"
#include "perf.h"

#ifndef SSB_NO_DRAW
#include <dc/pvr.h>
#endif

/* src/dc/grsector.c's float-matrix twin of the decomp's
 * grSectorArwingLaser3DFuncMatrix, for matrix kind 0x53 below -- no header
 * of its own since it is port-only (the decomp's own signature takes the
 * N64's fixed-point Mtx, which is no use here).
 *
 * Guarded by the same macro as gm/gmcollision.c's functions above, and for
 * the same reason: tools/check/lbparticle_check.py and
 * tools/check/figatree_check.py link this file alone, without
 * src/dc/grsector.c, to build an isolated oracle -- GC_NO_GMCOLLISION is what
 * both already define for that. */
#ifndef GC_NO_GMCOLLISION
extern void grSectorArwingLaser3DFuncMatrixF(float *m, DObj *dobj);
/* lb/lbcommon.c:1477-1603 func_ovl0_800C9A38, the joint's rotation-only
 * world matrix, which matrix kind 0x52 below is built on. Ported in
 * src/dc/ftcommon.c (which links the collision layer this file does not
 * always) and declared in the decomp's own lb/lbcommon.h; the extern is
 * here under the same guard as the Arwing's for the same reason. */
extern void func_ovl0_800C9A38(Mtx44f mtx, DObj *dobj);
/* lb/lbcommon.c:312, defined in src/dc/lbcommon.c: matrix kind 0x4B's
 * running scale (gcDObjLocalMatrix below) */
extern Vec3f gLBCommonScale;
extern Vec2f dFTDisplayMainShufflePositions[/* */][4];
/* src/dc/lbcommon.c's flat quad, which a fill-colour camera draws (see
 * gcPrepCameraViewport); the two oracles above link no sprite code, and
 * make no camera that asks for a fill. */
extern void lbCommonSpriteFillRect(s32 ulx, s32 uly, s32 lrx, s32 lry,
                                   u32 r, u32 g, u32 b, u32 a);
#endif

/* objdisplay.c:88. A global in the decomp too (objdisplay.h:29). */
syMtxProcess *sGCMatrixFuncList;

/* objdisplay.c:86 (objdisplay.h:28). gcDrawDObjTree saves and restores it
 * around every DObj. The game's Sca kinds fold each joint's scale.x into
 * it and the MVP-recalc kinds (44 to 50, 0x46 to 0x48) read it; the
 * port's recalc kinds read it and 0x4F writes it, but its Sca kinds do
 * not fold it, so a billboard under a scaled parent stands at its own
 * scale. */
f32 gGCScaleX;

/* objdisplay.c:3072 */
static s32 sGCCameraMatrixMode;

/* objdisplay.c:39 sGCMatrixMod1F, the camera pass's first billboard
 * matrix, as gcPrepCameraMatrix computes it at its tail: the rotation
 * block of a look-at with the camera's yaw taken out, one row per local
 * axis in the decomp's row-vector order. The projection the game
 * multiplies in is the port's sGCProjF, applied after, so it is not
 * kept here. Matrix kind 48 reads it (gcMtxRecalcMod1). */
static f32 sGCMatrixMod1R[3][3];

/* objdisplay.c:93-96 writes these from gcSetCameraScissor and
 * func_8001663C clamps the viewport to them. */
static s32 dGCCameraScissorTop, dGCCameraScissorBottom;
static s32 dGCCameraScissorLeft, dGCCameraScissorRight;

/* The camera pass's own state: the two matrices it establishes and the
 * PVR list the walk under it has open. */
static mtx4_t sGCViewF;
static mtx4_t sGCProjF;
static int sGCDrawList;

/* ---- the modelview stack (divergence 1) ------------------------------ */

#define GC_MTX_STACK_DEPTH 10       /* PR/gbi.h G_MTX_STACKSIZE */

static mtx4_t sGCMtxStack[GC_MTX_STACK_DEPTH];
static s32 sGCMtxTop;

static void gcMtxReset(void)
{
    sGCMtxTop = 0;
    mtx_identity(sGCMtxStack[0]);
}

static void gcMtxPush(void)
{
    if (sGCMtxTop + 1 >= GC_MTX_STACK_DEPTH)
    {
        /* gSPMatrix has no error path: the RSP would silently scribble.
         * Say so once and keep drawing against the top we have. */
        static sb32 warned = FALSE;

        if (warned == FALSE)
        {
            warned = TRUE;
            syDebugPrintf("od : modelview stack past %d\n",
                          GC_MTX_STACK_DEPTH);
        }
        return;
    }
    memcpy(sGCMtxStack[sGCMtxTop + 1], sGCMtxStack[sGCMtxTop],
           sizeof(mtx4_t));
    sGCMtxTop++;
}

static void gcMtxPop(void)
{
    if (sGCMtxTop > 0)
    {
        sGCMtxTop--;
    }
}

/* gSPMatrix(G_MTX_MUL | G_MTX_MODELVIEW): top = top * local. */
static void gcMtxMul(const float *local)
{
    mtx4_t t;

    mtx_mul(t, sGCMtxStack[sGCMtxTop], local);
    memcpy(sGCMtxStack[sGCMtxTop], t, sizeof(t));
}

/* ---- the port's own accessors (objpvr.h) ----------------------------- */

const float *gcGetViewF(void) { return sGCViewF; }
const float *gcGetProjF(void) { return sGCProjF; }

void gcSetCameraMatrixF(const float *view, const float *proj)
{
    memcpy(sGCViewF, view, sizeof(sGCViewF));
    memcpy(sGCProjF, proj, sizeof(sGCProjF));
}
int gcGetDrawList(void) { return sGCDrawList; }
void gcSetDrawList(int list) { sGCDrawList = list; }

/* ---- matrix kinds ---------------------------------------------------- */

/* One line of syDebugPrintf per kind the port has not needed yet, rather
 * than a silent wrong matrix.
 *
 * Two words and not one: the kinds sGCMatrixFuncList
 * dispatches start at 66, so a 64-bit mask covered none of them and every
 * unported one warned on every DObj of every frame. The metal dust is
 * where that showed -- its child DObj asks for kind 0x44, four of them
 * live at once, and the serial log took 12,949 lines of it in a
 * twenty-second run. 128 is every kind the list can hold. */
static u64 sGCKindWarned[2];

/* See objpvr.h: gcEjectGObj(NULL) means "eject the running GObj", so a
 * verbatim teardown line on a GObj* the port never filled kills the
 * scene's own driver. In mvopeningroom.c's tic
 * 280 the scene froze with everything else still animating;
 * mvending.c's tic-540 teardown has the same hazard. */
void gcEjectGObjIfMade(GObj *gobj)
{
    if (gobj != NULL)
    {
        gcEjectGObj(gobj);
    }
}

static void gcWarnKind(const char *what, s32 kind)
{
    if ((kind >= 0) && (kind < 128))
    {
        u64 bit = 1ULL << (kind & 63);

        if (sGCKindWarned[kind >> 6] & bit)
        {
            return;
        }
        sGCKindWarned[kind >> 6] |= bit;
    }
    syDebugPrintf("od : %s matrix kind %d not ported\n", what, kind);
}

/* sys/matrix.c syMatrixTraF, transposed into the port's row-major. */
static void gcMtxTra(float *m, f32 x, f32 y, f32 z)
{
    mtx_translate(m, x, y, z);
}

/* syMatrixScaF. */
static void gcMtxSca(float *m, f32 x, f32 y, f32 z)
{
    memset(m, 0, sizeof(mtx4_t));
    m[0] = x;
    m[5] = y;
    m[10] = z;
    m[15] = 1.0F;
}

/* syMatrixRotRpyRF: roll about X, then pitch about Y, then yaw about Z,
 * in radians. This is the same product src/dc/fighter.c's joint_local
 * writes out fused -- deliberately, since a fighter's DObj tree and its
 * pack's bind pose have to agree to the bit. */
static void gcMtxRotRpyR(float *m, f32 roll, f32 pitch, f32 yaw)
{
    float sr, cr, sp, cp, sy, cy;

    mtx_sincos(roll, &sr, &cr);
    mtx_sincos(pitch, &sp, &cp);
    mtx_sincos(yaw, &sy, &cy);

    m[0] = cp * cy;
    m[1] = sr * sp * cy - cr * sy;
    m[2] = cr * sp * cy + sr * sy;
    m[3] = 0.0F;
    m[4] = cp * sy;
    m[5] = sr * sp * sy + cr * cy;
    m[6] = cr * sp * sy - sr * cy;
    m[7] = 0.0F;
    m[8] = -sp;
    m[9] = sr * cp;
    m[10] = cr * cp;
    m[11] = 0.0F;
    m[12] = m[13] = m[14] = 0.0F;
    m[15] = 1.0F;
}

/* sys/matrix.c:105-169 syMatrixLookAtF, transposed into the port's
 * row-major: the decomp writes the basis down the columns of mf[r][c]
 * because the RSP multiplies row-vector-first, and mtx.h is the other way
 * round. The algebra -- negate the look vector, right = up x look, then
 * re-derive up, each normalised, and the eye dotted into the last column
 * -- is line for line the decomp's. */
void gcMtxLookAt(float *m, f32 eye_x, f32 eye_y, f32 eye_z,
                        f32 at_x, f32 at_y, f32 at_z,
                        f32 up_x, f32 up_y, f32 up_z)
{
    f32 len, look_x, look_y, look_z, right_x, right_y, right_z;

    look_x = at_x - eye_x;
    look_y = at_y - eye_y;
    look_z = at_z - eye_z;

    /* Negate because positive Z is behind us: */
    len = -1.0F / sqrtf(SQUARE(look_x) + SQUARE(look_y) + SQUARE(look_z));
    look_x *= len;
    look_y *= len;
    look_z *= len;

    /* Right = Up x Look */
    right_x = up_y * look_z - up_z * look_y;
    right_y = up_z * look_x - up_x * look_z;
    right_z = up_x * look_y - up_y * look_x;

    len = 1.0F / sqrtf(SQUARE(right_x) + SQUARE(right_y) + SQUARE(right_z));
    right_x *= len;
    right_y *= len;
    right_z *= len;

    /* Up = Look x Right */
    up_x = look_y * right_z - look_z * right_y;
    up_y = look_z * right_x - look_x * right_z;
    up_z = look_x * right_y - look_y * right_x;

    len = 1.0F / sqrtf(SQUARE(up_x) + SQUARE(up_y) + SQUARE(up_z));
    up_x *= len;
    up_y *= len;
    up_z *= len;

    m[0] = right_x;
    m[1] = right_y;
    m[2] = right_z;
    m[3] = -(eye_x * right_x + eye_y * right_y + eye_z * right_z);

    m[4] = up_x;
    m[5] = up_y;
    m[6] = up_z;
    m[7] = -(eye_x * up_x + eye_y * up_y + eye_z * up_z);

    m[8] = look_x;
    m[9] = look_y;
    m[10] = look_z;
    m[11] = -(eye_x * look_x + eye_y * look_y + eye_z * look_z);

    m[12] = m[13] = m[14] = 0.0F;
    m[15] = 1.0F;
}

/* sys/matrix.c:194-260 syMatrixModLookAtF, transposed as gcMtxLookAt
 * above: the look-at whose `roll` (radians) turns the camera about its
 * own line of sight after the basis is built off a fixed up axis. Matrix
 * kinds 8-11 and 14-17 hand it cobj->vec.up.x as the roll -- the field
 * is an angle there, not a vector component. */
void gcMtxModLookAt(float *m, f32 eye_x, f32 eye_y, f32 eye_z,
                    f32 at_x, f32 at_y, f32 at_z, f32 roll,
                    f32 up_x, f32 up_y, f32 up_z)
{
    f32 len;
    Vec3f look, right;

    look.x = at_x - eye_x;
    look.y = at_y - eye_y;
    look.z = at_z - eye_z;

    /* Negate because positive Z is behind us: */
    len = -1.0F / sqrtf(SQUARE(look.x) + SQUARE(look.y) + SQUARE(look.z));
    look.x *= len;
    look.y *= len;
    look.z *= len;

    /* Right = Up x Look */
    right.x = up_y * look.z - up_z * look.y;
    right.y = up_z * look.x - up_x * look.z;
    right.z = up_x * look.y - up_y * look.x;
    len = 1.0F / sqrtf(SQUARE(right.x) + SQUARE(right.y) + SQUARE(right.z));
    right.x *= len;
    right.y *= len;
    right.z *= len;

    syVectorRotateAbout3D(&right, &look, roll);
    up_x = (look.y * right.z) - (look.z * right.y);
    up_y = (look.z * right.x) - (look.x * right.z);
    up_z = (look.x * right.y) - (look.y * right.x);
    len = 1.0F / sqrtf(SQUARE(up_x) + SQUARE(up_y) + SQUARE(up_z));
    up_x *= len;
    up_y *= len;
    up_z *= len;

    m[0] = right.x;
    m[1] = right.y;
    m[2] = right.z;
    m[3] = -(eye_x * right.x + eye_y * right.y + eye_z * right.z);

    m[4] = up_x;
    m[5] = up_y;
    m[6] = up_z;
    m[7] = -(eye_x * up_x + eye_y * up_y + eye_z * up_z);

    m[8] = look.x;
    m[9] = look.y;
    m[10] = look.z;
    m[11] = -(eye_x * look.x + eye_y * look.y + eye_z * look.z);

    m[12] = m[13] = m[14] = 0.0F;
    m[15] = 1.0F;
}

/* objdisplay.c:2841-2892, the camera look-at family, one place for both
 * of this file's matrix walks. Three shapes, as the decomp has them:
 * 6/7 and 12/13 (the Reflect pair, whose LookAt lights have no PVR
 * meaning) are the plain look-at off cobj->vec.up; 8/9 and 14/15 are
 * syMatrixModLookAt about +Y with up.x as the roll; 10/11 and 16/17 the
 * same about +Z. Returns the var_s3 the decomp sets beside each: which
 * axis the sGCMatrixMod1F half treats as up. */
static s32 gcCameraLookAtF(float *m, CObj *cobj, s32 kind)
{
    switch (kind)
    {
    case 8:
    case 9:
    case 14:
    case 15:
        gcMtxModLookAt(m, cobj->vec.eye.x, cobj->vec.eye.y, cobj->vec.eye.z,
                       cobj->vec.at.x, cobj->vec.at.y, cobj->vec.at.z,
                       cobj->vec.up.x, 0.0F, 1.0F, 0.0F);
        return 1;

    case 10:
    case 11:
    case 16:
    case 17:
        gcMtxModLookAt(m, cobj->vec.eye.x, cobj->vec.eye.y, cobj->vec.eye.z,
                       cobj->vec.at.x, cobj->vec.at.y, cobj->vec.at.z,
                       cobj->vec.up.x, 0.0F, 0.0F, 1.0F);
        return 2;

    default:
        gcMtxLookAt(m, cobj->vec.eye.x, cobj->vec.eye.y, cobj->vec.eye.z,
                    cobj->vec.at.x, cobj->vec.at.y, cobj->vec.at.z,
                    cobj->vec.up.x, cobj->vec.up.y, cobj->vec.up.z);
        return (cobj->vec.up.z < cobj->vec.up.y) ? 1 : 2;
    }
}

/* sys/objdisplay.c:169-223 func_80010918, which matrix kinds 39 and 40
 * dispatch to (objdisplay.c:665-671), transposed into the port's
 * row-major exactly as gcMtxLookAt above is.
 *
 * It is not a transform of the DObj's own vector at all: it builds the
 * basis that turns the DObj's geometry to face the camera. The unit
 * vector from the camera's eye to the DObj is the new -Z; the new Y is
 * the world's Y projected off it (`res` is the length of that vector's
 * XZ part, which is the cosine of its pitch); the new X is the two
 * crossed, which is why it has no Y term and why mf[0][1] stays zero.
 * With the eye directly above or below the DObj `res` is zero and there
 * is no answer, so the decomp leaves the identity there and so does
 * this.
 *
 * `is_translate` is the difference between kind 39 and kind 40: 40 puts
 * the DObj at its own translate as well as turning it, 39 turns it and
 * leaves it where the parent's matrix put it.
 *
 * ef/efmanager.c's damage orbs are the port's first caller: a flat quad
 * of two triangles that has to be a round sprite from any angle.
 */
void gcMtxBillboard(float *m, const Vec3f *at, const Vec3f *eye,
                    sb32 is_translate)
{
    f32 distx = at->x - eye->x;
    f32 disty = at->y - eye->y;
    f32 distz = at->z - eye->z;
    f32 res = 1.0F / sqrtf(SQUARE(distx) + SQUARE(disty) + SQUARE(distz));

    distx *= res;
    disty *= res;
    distz *= res;

    res = sqrtf(SQUARE(distx) + SQUARE(distz));

    m[12] = m[13] = m[14] = m[4] = 0.0F;
    m[15] = 1.0F;

    if (res != 0.0F)
    {
        f32 inv = 1.0F / res;

        m[0] = -distz * inv;
        m[1] = -disty * distx * inv;
        m[2] = -distx;

        m[5] = res;
        m[6] = -disty;

        m[8] = distx * inv;
        m[9] = -disty * distz * inv;
        m[10] = -distz;
    }
    else
    {
        m[1] = m[2] = m[6] = m[8] = m[9] = 0.0F;
        m[0] = m[5] = m[10] = 1.0F;
    }
    if (is_translate != FALSE)
    {
        m[3] = at->x;
        m[7] = at->y;
        m[11] = at->z;
    }
    else m[3] = m[7] = m[11] = 0.0F;
}

/* sys/objdisplay.c:822-856, matrix kind 44 (nGCMatrixKindRecalcRotRpyRSca),
 * the RSP billboard: the N64 recomputes the model-view-projection's
 * first three rows as the projection times diag(sx, sy, sx) -- no
 * rotation from the stack and none from the view -- and leaves the
 * fourth row, the translation, as the stack had it. So the DObj's local
 * axes become the camera's, at the stack's world position, at a scale
 * that folds gGCScaleX in: x and z by gGCScaleX * scale.x (and gGCScaleX
 * keeps the product), y by the *previous* gGCScaleX * scale.y.
 *
 * In world space that matrix is T(at) * R_camera * S, with R_camera the
 * transpose of the view's rotation block -- which is what this builds,
 * so that view * m comes out as diag(sx', sy', sx') beside view * at,
 * which is the property the decomp's twelve gMoveWd words state. The
 * shield bubble's quad and Yoshi's egg are the two DObjs that ask for it
 * (ef/efmanager.c). Exported for the host cross-test. */
void gcMtxRecalcRotRpyRSca(float *m, const float *at, const float *view,
                           const Vec3f *scale, f32 *scale_x)
{
    f32 sy = scale->y * *scale_x;
    f32 sx;
    s32 i;

    *scale_x *= scale->x;
    sx = *scale_x;

    for (i = 0; i < 3; i++)
    {
        m[i * 4 + 0] = view[0 * 4 + i] * sx;
        m[i * 4 + 1] = view[1 * 4 + i] * sy;
        m[i * 4 + 2] = view[2 * 4 + i] * sx;
    }
    m[3] = at[0];
    m[7] = at[1];
    m[11] = at[2];
    m[12] = m[13] = m[14] = 0.0F;
    m[15] = 1.0F;
}

/* lb/lbcommon.c:1702 func_ovl0_800CA194, matrix kind 0x46 -- entry 4 of
 * dLBCommonFuncMatrixList, pointers 8 and 9, by the same counting
 * gcDObjLocalMatrix's 0x44/0x45/0x52/0x53 cases already spell out.
 *
 * gcMtxRecalcRotRpyRSca's sibling, and the same kind of thing kind 44 is:
 * an RSP billboard that REPLACES the model-view-projection's first three
 * rows rather than contributing a factor, leaving the fourth -- the
 * translation -- as the stack had it. Where 44 writes the projection
 * times diag(sx, sy, sx), this writes the projection with an in-plane
 * spin folded in and no scale at all: MVP row 0 becomes P[0]*cos + P[1]*sin,
 * row 1 becomes -P[0]*sin + P[1]*cos, row 2 stays P[2]. So the DObj's
 * local axes become the camera's, turned about the view's own z by the
 * DObj's rotate.z, at the stack's world position.
 *
 * In world space that is T(at) * R_camera * Rz(theta), with R_camera the
 * transpose of the view's rotation block -- exactly 44's matrix with
 * diag(sx, sy, sx) swapped for Rz -- so that view * m comes out as
 * Rz(theta) beside view * at, which is what the decomp's twelve gMoveWd
 * words state. gGCScaleX is NOT touched: 44 folds it and this one does
 * not, which is the other difference between them.
 *
 * Seven objects in the game ask for it, all of them things that face the
 * camera and spin in its plane: the Motion-Sensor Bomb
 * (it/itcommon/itmsbomb.c:620), the Master Ball (itmball.c:489),
 * Saffron City's Onix (itmonster/itiwark.c:460) and Meowth
 * (itnyars.c:284), Marumine (itground/itmarumine.c:198), and two of
 * ef/efmanager.c's effects (:4601, :4755). */
void gcMtxRecalcRotZ(float *m, const float *at, const float *view, f32 rot_z)
{
    f32 s = sinf(rot_z);
    f32 c = cosf(rot_z);
    s32 i;

    for (i = 0; i < 3; i++)
    {
        f32 a0 = view[0 * 4 + i];
        f32 a1 = view[1 * 4 + i];

        m[i * 4 + 0] = (a0 * c) + (a1 * s);
        m[i * 4 + 1] = (a1 * c) - (a0 * s);
        m[i * 4 + 2] = view[2 * 4 + i];
    }
    m[3] = at[0];
    m[7] = at[1];
    m[11] = at[2];
    m[12] = m[13] = m[14] = 0.0F;
    m[15] = 1.0F;
}

/* The shape every RSP billboard below shares: the N64 rewrites the MVP's
 * first three rows as combinations of the projection's, row j = sum over
 * k of R[j][k] * P[k], and keeps the translation row. In view space that
 * is local axis j -> R[j], at the stack's world position; in world space
 * it is T(at) * R_camera * R^T, which is what this writes, so that
 * view * m has R^T for its rotation block and view * at for its
 * translation. gcMtxRecalcRotZ above is the case R = Rz; these are the
 * kinds whose R also scales or turns out of the view plane. */
static void gcMtxRecalcRows(float *m, const float *at, const float *view,
                            const f32 r[3][3])
{
    s32 i, j, k;

    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            f32 v = 0.0F;

            for (k = 0; k < 3; k++)
            {
                v += r[j][k] * view[k * 4 + i];
            }
            m[i * 4 + j] = v;
        }
    }
    m[3] = at[0];
    m[7] = at[1];
    m[11] = at[2];
    m[12] = m[13] = m[14] = 0.0F;
    m[15] = 1.0F;
}

/* sys/objdisplay.c:858-944, matrix kinds 45 and 46: the kind 44 billboard
 * with an in-plane spin by rotate.x (45) or rotate.z (46) folded in --
 * MVP row 0 = P00 sx cos, P11 sy sin; row 1 = -P00 sx sin, P11 sy cos;
 * row 2 = P2 sx -- with gGCScaleX folded as kind 44 folds it. Ness's PSI
 * Magnet bubble and PK Thunder wave, and Yoshi's egg, ask for 46
 * (ef/efmanager.c). */
void gcMtxRecalcRotSca(float *m, const float *at, const float *view,
                       f32 rot, const Vec3f *scale, f32 *scale_x)
{
    f32 s = sinf(rot);
    f32 c = cosf(rot);
    f32 sy = scale->y * *scale_x;
    f32 sx;
    f32 r[3][3];

    *scale_x *= scale->x;
    sx = *scale_x;

    r[0][0] = sx * c;  r[0][1] = sy * s; r[0][2] = 0.0F;
    r[1][0] = -sx * s; r[1][1] = sy * c; r[1][2] = 0.0F;
    r[2][0] = 0.0F;    r[2][1] = 0.0F;   r[2][2] = sx;

    gcMtxRecalcRows(m, at, view, r);
}

/* sys/objdisplay.c:980-1013, matrix kind 48: the MVP's first three rows
 * become sGCMatrixMod1F's -- the camera with its yaw taken out -- rows 0
 * and 2 scaled by gGCScaleX * scale.x and row 1 by the previous gGCScaleX
 * * scale.y, gGCScaleX keeping the product. So the DObj turns to face
 * the camera about the vertical axis and tilts with its pitch, at the
 * stack's world position; nothing of the parents' rotation or scale is
 * left. Yoshi's Island's clouds are the one thing in VS mode that ask
 * for it (gr/grcommon/gryoster.c:243). */
void gcMtxRecalcMod1(float *m, const float *at, const float *view,
                     const Vec3f *scale, f32 *scale_x)
{
    f32 sy = scale->y * *scale_x;
    f32 sx;
    f32 r[3][3];
    s32 k;

    *scale_x *= scale->x;
    sx = *scale_x;

    for (k = 0; k < 3; k++)
    {
        r[0][k] = sGCMatrixMod1R[0][k] * sx;
        r[1][k] = sGCMatrixMod1R[1][k] * sy;
        r[2][k] = sGCMatrixMod1R[2][k] * sx;
    }
    gcMtxRecalcRows(m, at, view, r);
}

/* lb/lbcommon.c:1760-1832 func_ovl0_800CA5C8 and :1834-1911
 * func_ovl0_800CAB48, matrix kinds 0x47 and 0x48: the billboard turned
 * by rotate.x and rotate.y out of the view plane -- row 0 = P0 cos y -
 * P2 sin y, row 1 = P0 sin x sin y + P1 cos x + P2 sin x cos y, row 2 =
 * P0 cos x sin y - P1 sin x + P2 cos x cos y -- with, for 0x48 (scale_x
 * not NULL), rows 0 and 2 scaled by gGCScaleX * scale.x and row 1 by the
 * previous gGCScaleX * scale.y, gGCScaleX keeping the product. Mario's
 * and Luigi's fireballs ask for 0x47 (wp/wpmario/wpmariofireball.c:62);
 * the Poke Ball monsters and both shells for 0x48. */
void gcMtxRecalcRotRpy(float *m, const float *at, const float *view,
                       f32 rot_x, f32 rot_y, const Vec3f *scale,
                       f32 *scale_x)
{
    f32 sinx = sinf(rot_x), cosx = cosf(rot_x);
    f32 siny = sinf(rot_y), cosy = cosf(rot_y);
    f32 sx = 1.0F, sy = 1.0F;
    f32 r[3][3];
    s32 k;

    if (scale_x != NULL)
    {
        sy = scale->y * *scale_x;
        *scale_x *= scale->x;
        sx = *scale_x;
    }
    r[0][0] = cosy;        r[0][1] = 0.0F;  r[0][2] = -siny;
    r[1][0] = sinx * siny; r[1][1] = cosx;  r[1][2] = sinx * cosy;
    r[2][0] = cosx * siny; r[2][1] = -sinx; r[2][2] = cosx * cosy;

    for (k = 0; k < 3; k++)
    {
        r[0][k] *= sx;
        r[1][k] *= sy;
        r[2][k] *= sx;
    }
    gcMtxRecalcRows(m, at, view, r);
}

/* Does this XObj still DRIVE the DObj's translate/rotate/scale, or has a
 * later one taken them over?
 *
 * sys/objman.c's gcAddXObjForDObjVar is where the question comes from: an
 * XObj of a vector kind does not merely sit in `dobj->xobjs[]`, it claims
 * the vectors it writes -- `*translate = dGCTranslateDefault;
 * translate->xobj = xobj;` -- so GCTranslate/GCRotate/GCScale each name
 * the one XObj that currently owns them. gcAddXObjForDObjFixed APPENDS
 * (it is gcAddXObjForDObjVar at index `xobjs_num`), so adding, say, a
 * TraRotRpyR to a DObj that already had the Tra/RotRpyR/Sca triple leaves
 * the first two in the array with nothing left to drive.
 *
 * The decomp's own walk does not ask, because on the N64 it never has to:
 * a DObj there is given its transform set ONCE. This port diverges at
 * exactly the place where it matters -- dc_model_add_dobjs gives every
 * joint the three-XObj set (gcAddDObjMatrixSetsRpyR) where a decomp item
 * root is given the single fused kind its ITDesc names (see
 * src/dc/itemmodel.c's own note) -- and then ten verbatim it/ makers
 * append their own fused kind on top of it: itmsbomb.c, itmball.c,
 * itiwark.c, itnyars.c, itbox.c, itgshell.c, itrshell.c, itdogas.c,
 * itlizardon.c, ittosakinto.c. Without this check both sets apply and
 * the item composes its translation and rotation TWICE.
 *
 * "Drives" is any of the vectors its kind writes, not all of them: the
 * kinds hand their vectors over as a unit, so an XObj that owns one of
 * its own owns the rest, and the one shape that could split them (a bare
 * Tra appended over a fused kind, taking the translate and leaving the
 * rotation) is not a thing the game does. A kind that writes no DObj
 * vector -- the billboards, the whole-matrix func-list kinds -- is never
 * claimed by anything and always drives. */
static sb32 gcXObjDrivesDObj(const DObj *dobj, const XObj *xobj, s32 kind)
{
    sb32 tra = FALSE, rot = FALSE, sca = FALSE;

    switch (kind)
    {
    case nGCMatrixKindTra:
        tra = TRUE;
        break;
    case nGCMatrixKindSca:
        sca = TRUE;
        break;
    case nGCMatrixKindRotD:
    case nGCMatrixKindRotRpyD:
    case nGCMatrixKindRotR:
    case nGCMatrixKindRotRpyR:
    case nGCMatrixKindRotPyrR:
        rot = TRUE;
        break;
    case nGCMatrixKindTraRotD:
    case nGCMatrixKindTraRotRpyD:
    case nGCMatrixKindTraRotR:
    case nGCMatrixKindTraRotRpyR:
    case nGCMatrixKindTraRotPyrR:
        tra = rot = TRUE;
        break;
    case nGCMatrixKindTraRotRSca:
    case nGCMatrixKindTraRotRpyRSca:
    case nGCMatrixKindTraRotPyrRSca:
        tra = rot = sca = TRUE;
        break;
    default:
        return TRUE;
    }
    if (tra && (dobj->translate.xobj == xobj))
    {
        return TRUE;
    }
    if (rot && (dobj->rotate.xobj == xobj))
    {
        return TRUE;
    }
    if (sca && (dobj->scale.xobj == xobj))
    {
        return TRUE;
    }
    return FALSE;
}

/* The DObj half of gcPrepDObjMatrix's `switch (xobj->kind)`
 * (objdisplay.c:472-1018), with the caching and the GBI stripped: build
 * this XObj's contribution to the DObj's local transform. Returns FALSE
 * for a kind the port has not needed yet, so the caller can skip it
 * rather than multiply in whatever was on the stack.
 *
 * The kinds here are the ones gcAddDObjMatrixSetsRpyD and ...RpyR install
 * (objhelper.c:267-280), which is what every DObj the port builds and
 * every fighter part in the game uses: Tra, a rotation, Sca. */
static sb32 gcDObjLocalMatrix(float *m, DObj *dobj, s32 kind)
{
    switch (kind)
    {
    case nGCMatrixKindTra:
        gcMtxTra(m, dobj->translate.vec.f.x, dobj->translate.vec.f.y,
                 dobj->translate.vec.f.z);
        return TRUE;

    case nGCMatrixKindSca:
        gcMtxSca(m, dobj->scale.vec.f.x, dobj->scale.vec.f.y,
                 dobj->scale.vec.f.z);
        return TRUE;

    case nGCMatrixKindRotRpyR:
        gcMtxRotRpyR(m, dobj->rotate.vec.f.x, dobj->rotate.vec.f.y,
                     dobj->rotate.vec.f.z);
        return TRUE;

    case nGCMatrixKindRotRpyD:
        gcMtxRotRpyR(m, dobj->rotate.vec.f.x * DTOR32,
                     dobj->rotate.vec.f.y * DTOR32,
                     dobj->rotate.vec.f.z * DTOR32);
        return TRUE;

    /* sys/matrix.c:1077 syMatrixTraRotRpyRScaF, taken whole like 0x44/0x45
     * above: a fighter part's tree gets Tra, RotRpyR and Sca as three
     * separate XObj slots (gcAddDObjMatrixSetsRpyR), but a ground actor's
     * bare wrapper DObj (ef/efground.c dcGroundMakeEffect, Part B) asks
     * for the fused kind directly, the same as the decomp's own EFDesc
     * does (efground.c:46, both Castle's and Kongo Jungle's). The
     * decomp's own syMatrixRowscaleF scales ROWS 0-2 of the rotation (the
     * row-vector convention's local axes) by sx/sy/sz and leaves row 3
     * (the translate row) alone; transposed into the port's column
     * layout that is COLUMNS 0-2, the same transpose 0x45 above already
     * spells out, and the translate slots (m[3]/m[7]/m[11]) are exactly
     * as untouched by it here as they are there. */
    case nGCMatrixKindTraRotRpyRSca:
    {
        s32 i;

        gcMtxRotRpyR(m, dobj->rotate.vec.f.x, dobj->rotate.vec.f.y,
                     dobj->rotate.vec.f.z);

        for (i = 0; i < 3; i++)
        {
            m[i * 4 + 0] *= dobj->scale.vec.f.x;
            m[i * 4 + 1] *= dobj->scale.vec.f.y;
            m[i * 4 + 2] *= dobj->scale.vec.f.z;
        }
        m[3] = dobj->translate.vec.f.x;
        m[7] = dobj->translate.vec.f.y;
        m[11] = dobj->translate.vec.f.z;
        return TRUE;
    }

    /* The same fused matrix WITHOUT the scale -- sys/matrix.c's
     * syMatrixTraRotRpyRF is literally syMatrixRotRpyRF plus the
     * translate row, which is the case above with the scale loop cut out.
     *
     * Fox's Arwing entry effect asks for it, on every battle it plays
     * in, and it was falling to the default's FALSE -- "skip this DObj's
     * contribution" -- so the DObj kept neither its translation nor its
     * rotation. Found the same way kind 28 above was: by a probe's own
     * log line, not by anything looking wrong on screen.
     *
     * Every EFGroundDesc in the game also names it as its second
     * transform kind (transform_types2, ef/efground.c), but that is the
     * decomp's data and not this port's path: dcGroundSetupEffectDObjs
     * builds an actor's tree through dc_model_add_dobjs like every other
     * model here, which gives each joint the port's own three XObj slots
     * instead of the fused kind. */
    case nGCMatrixKindTraRotRpyR:
        gcMtxRotRpyR(m, dobj->rotate.vec.f.x, dobj->rotate.vec.f.y,
                     dobj->rotate.vec.f.z);
        m[3] = dobj->translate.vec.f.x;
        m[7] = dobj->translate.vec.f.y;
        m[11] = dobj->translate.vec.f.z;
        return TRUE;

    /* Nothing, quietly. ft/ftdisplaymain.c switches a fighter's TopN
     * from its own kind to RotRpyR while the magnify camera draws it,
     * dropping the translation so the fighter stands at the origin of
     * the glass's own view (ftdisplaymain.c:1218). The port's TopN is
     * gcAddDObjMatrixSetsRpyR's three -- Tra, RotRpyR, Sca -- so the
     * same switch here is Tra to Null and back. */
    case nGCMatrixKindNull:
        return FALSE;

    /* Matrix kinds 39 and 40, the camera-facing billboard. The game
     * reaches them through gcDecideDObj3TransformsKind for a DObj whose
     * transform triple asks for one (sys/objanim.c:2224); an effect's
     * EFDesc asks for kind 40 by writing it into transform_types1
     * (ef/efmanager.c:143, the damage orbs). */
    case 39:
    case 40:
    {
        CObj *cobj;

        if (gGCCurrentCamera == NULL)
        {
            return FALSE;
        }
        cobj = CObjGetStruct(gGCCurrentCamera);

        gcMtxBillboard(m, &dobj->translate.vec.f, &cobj->vec.eye,
                       kind == 40);
        return TRUE;
    }

    /* lb/lbcommon.c:1639 func_ovl0_800CA024, matrix kind 0x44, and
     * lb/lbcommon.c:1686 lbCommonRotScaFuncMatrix, matrix kind 0x45.
     * sGCMatrixFuncList is a syMtxProcess array indexed by kind - 66
     * (sys/objdisplay.c:1090) and a syMtxProcess is a pair of function
     * pointers, so dLBCommonFuncMatrixList's flat list of pointers holds
     * two per kind: 0x44 is entry 2 of it, pointers 4 and 5, and 0x45 is
     * entry 3, pointers 6 and 7. It is the same counting that puts 0x50
     * at entry 14, below. sc/scvsbattle.c:54 is where a VS battle
     * installs that list.
     *
     * Both are a whole local matrix rather than one factor of one, which
     * is why neither composes with anything: the two DObjs that ask for
     * them (the metal dust's child at 0x44 and the damage
     * sparks' at 0x45) have nGCMatrixKindNull in the other
     * two slots of their transform triple.
     *
     * The decomp writes both as packed fixed point straight into an Mtx,
     * so what they are has to be read out of the element order: the
     * word pair m[0][k]/m[2][k] is elements (k*2) and (k*2 + 1) of the
     * row-vector matrix, counted along the rows. Doing that gives
     * 0x44 = diag(sx, sy, sz) with the translate in the last row, and
     * 0x45 = the RotRpyR product with column j scaled by s_j -- that is,
     * T * S and R * S. Transposed into the port's row-major they are the
     * two below.
     */
    case 0x44:
        gcMtxSca(m, dobj->scale.vec.f.x, dobj->scale.vec.f.y,
                 dobj->scale.vec.f.z);
        m[3] = dobj->translate.vec.f.x;
        m[7] = dobj->translate.vec.f.y;
        m[11] = dobj->translate.vec.f.z;
        return TRUE;

    case 0x45:
    {
        s32 i;

        gcMtxRotRpyR(m, dobj->rotate.vec.f.x, dobj->rotate.vec.f.y,
                     dobj->rotate.vec.f.z);

        /* M[i][j] = R[i][j] * s_j: the scale is on the right, so it is
         * the columns that are scaled, one per axis. */
        for (i = 0; i < 3; i++)
        {
            m[i * 4 + 0] *= dobj->scale.vec.f.x;
            m[i * 4 + 1] *= dobj->scale.vec.f.y;
            m[i * 4 + 2] *= dobj->scale.vec.f.z;
        }
        return TRUE;
    }

    /* lb/lbcommon.c:2153 func_ovl0_800C99CC, matrix kind 0x50, taken
     * whole: not a transform of this DObj's own vector at all but the
     * world position of the DObj its user_data points at. It is index 14
     * of dLBCommonFuncMatrixList, which sGCMatrixFuncList indexes by
     * kind - 66 (sys/objdisplay.c:1090); the port has no such list, so
     * the one kind an effect uses is dispatched here beside the ordinary
     * ones. ef/efmanager.c gives it to every effect that follows a
     * fighter -- the rebirth halo is the first (src/dc/efmanager.c), and
     * the joint it follows is the fighter's TopN. */
    case 0x50:
    {
#ifndef GC_NO_GMCOLLISION
        Vec3f translate_base = { 0.0F, 0.0F, 0.0F };

        if (dobj->user_data.p == NULL)
        {
            return FALSE;
        }
        gmCollisionGetFighterPartsWorldPosition(dobj->user_data.p,
                                                &translate_base);
        gcMtxTra(m, translate_base.x, translate_base.y, translate_base.z);
        return TRUE;
#else
        /* The oracle builds link this file without gm/gmcollision.c and
         * without a model to attach to; nothing in them draws an effect
         * (tools/check/figatree_check.py and tools/check/lbparticle_check.py
         * define the macro). The host cross-test links gm/gmcollision.c and
         * takes the real case. */
        return FALSE;
#endif
    }

    /* gr/grcommon/grsector.c:277 grSectorArwingLaser3DFuncMatrix, matrix
     * kind 0x53 (REGION_US) -- entry 34 of dLBCommonFuncMatrixList. That
     * list holds a syMtxProcess PAIR per kind (proc_diff, proc_same), both
     * of which this kind gives the same function, so it is flat index 34
     * -- pair 17 -- and sGCMatrixFuncList indexes pairs by kind - 66:
     * 66 + 17 = 0x53. Not laser-specific despite the name: it is joint 0's
     * OWN matrix, the Arwing's root, whose orientation is not a rotation
     * the DObj carries but a basis gr/grcommon/grsector.c's
     * func_ovl2_80106730 builds fresh every tic from the flight path.
     *
     * A whole local matrix, like 0x44/0x45 above -- so it is the ONLY
     * XObj joint 0 gets (grSectorArwingSetKinds clears the bake's default
     * triple for that one joint before installing this), never composed
     * with anything else. Reimplemented straight into the port's float
     * layout (grSectorArwingLaser3DFuncMatrixF) rather than called through
     * the decomp's own fixed-point-Mtx signature, the same reason 0x44 and
     * 0x45 above are hand-transposed instead of called. */
    case 0x53:
#ifndef GC_NO_GMCOLLISION
        grSectorArwingLaser3DFuncMatrixF(m, dobj);
        return TRUE;
#else
        /* No Sector Z in these isolated oracle builds either -- see the
         * extern's own comment above. */
        return FALSE;
#endif

    /* lb/lbcommon.c:1445 func_ovl0_800C994C, matrix kind 0x4F, entry 13
     * of the same list: the whole world matrix of the fighter joint the
     * DObj's user_data names -- func_ovl2_800EDBA4 brings the joint's
     * FTParts.mtx_translate up to date and the function takes it as it
     * stands, scale and all -- and gGCScaleX becomes the length of its
     * first row, which the billboard kind below reads. The shield bubble
     * is the effect that asks for it (ef/efmanager.c
     * dEFManagerShieldEffectDesc), on YRotN, whose scale is the shield's
     * size. The decomp's Mtx44f is row-vector; the port is
     * column-vector, so it is transposed on the way in, exactly as the
     * two kinds above are. */
    case 0x4F:
    {
#ifndef GC_NO_GMCOLLISION
        DObj *attach_dobj = dobj->user_data.p;
        FTParts *parts;
        Mtx44f f;
        s32 i, j;

        if (attach_dobj == NULL)
        {
            return FALSE;
        }
        parts = ftGetParts(attach_dobj);
        func_ovl2_800EDBA4(attach_dobj);
        gmCollisionCopyMatrix(f, parts->mtx_translate);
        gGCScaleX = sqrtf(SQUARE(f[0][0]) + SQUARE(f[0][1]) + SQUARE(f[0][2]));

        /* syMatrixF2LFixedW: the fourth column is not read off the float
         * matrix but written as the RSP wants it, 0 0 0 1. */
        for (i = 0; i < 3; i++)
        {
            for (j = 0; j < 4; j++)
            {
                m[i * 4 + j] = f[j][i];
            }
        }
        m[12] = m[13] = m[14] = 0.0F;
        m[15] = 1.0F;
        return TRUE;
#else
        return FALSE;
#endif
    }

    /* lb/lbcommon.c:1619-1635 func_ovl0_800C9F70, matrix kind 0x52 (0x51
     * on the JP ROM), entry 16 of dLBCommonFuncMatrixList: the same
     * counting as 0x4F and 0x50 above -- the list holds a syMtxProcess
     * PAIR per kind and sGCMatrixFuncList indexes pairs by kind - 66, so
     * flat indices 32 and 33 are pair 16, and 66 + 16 = 0x52.
     *
     * THE HELD ITEM'S MATRIX. it/itmain.c's itMainSetFighterHold splices a
     * fresh DObj above an item that has just been picked up, points its
     * user_data at the fighter's hand joint and gives it this one XObj;
     * from then on the item's whole tree hangs off whatever that hand is
     * doing. Where 0x4F takes the joint's world matrix scale and all, and
     * 0x50 takes only its position, this takes its orientation and
     * position with the scale divided out (func_ovl0_800C9A38 normalizes
     * each row) -- a fighter's own size must not stretch the sword he is
     * holding.
     *
     * The shuffle is the other half: a fighter in hitlag is jogged four
     * offsets in turn, and the game adds the same jog to the held item so
     * the two shake together (the fighter's own is src/dc/objmodel.c
     * dc_model_view_for).
     *
     * Transposed on the way in like every other whole-matrix kind here:
     * the decomp's Mtx44f is row-vector and the port's is column-vector,
     * and the fourth column is written as the RSP wants it rather than
     * read off the float matrix (syMatrixF2LFixedW). */
    case 0x52:
    {
#ifndef GC_NO_GMCOLLISION
        DObj *attach_dobj = dobj->user_data.p;
        FTStruct *fp;
        Mtx44f f;
        s32 i, j;

        if (attach_dobj == NULL || attach_dobj->parent_gobj == NULL)
        {
            return FALSE;
        }
        fp = ftGetStruct(attach_dobj->parent_gobj);

        func_ovl0_800C9A38(f, attach_dobj);

        if (fp->shuffle_tics != 0)
        {
            f[3][0] += dFTDisplayMainShufflePositions[fp->is_shuffle_electric][fp->shuffle_frame_index].x;
            f[3][1] += dFTDisplayMainShufflePositions[fp->is_shuffle_electric][fp->shuffle_frame_index].y;
        }
        for (i = 0; i < 3; i++)
        {
            for (j = 0; j < 4; j++)
            {
                m[i * 4 + j] = f[j][i];
            }
        }
        m[12] = m[13] = m[14] = 0.0F;
        m[15] = 1.0F;
        return TRUE;
#else
        /* No fighters in the isolated oracle builds -- the same note as
         * 0x4F and 0x50 above. */
        return FALSE;
#endif
    }

    /* lb/lbcommon.c:1369-1442 lbCommonFighterPartsFuncMatrix, matrix kind
     * 0x4B, which every fighter joint has (ft/ftmanager.c:762-775). Its
     * two halves, in float and transposed as the kinds above are:
     *  - a joint whose FTParts.transform_update_mode is not 0 draws the
     *    local matrix gm/gmcollision.c left in unk_dobjtrans_0x10: the
     *    one it cached this frame (1), or the one ftParamSetAnimLocks
     *    froze (3) -- which is how a locked joint stays put whatever its
     *    animation does;
     *  - otherwise the joint's own TraRotRpyRSca, except that while the
     *    fighter's motion sets is_use_animlocks the scale is not passed
     *    down as a factor of the parent: lbCommonMatrixTraRotScaInv
     *    divides each row by the scale the parent accumulated
     *    (gLBCommonScale) and scales each column by this joint's own times
     *    it, and this joint's product becomes the running scale for its
     *    children (the walk saves and restores it around each joint, as
     *    ftDisplayMainDrawDefault does). Yoshi's tongue and Samus's
     *    grapple are the scaled joints under that flag.
     * The port's fighter joints carry Tra, RotRpyR and Sca XObjs, not one
     * 0x4B (ftcommon.c ftMainUpdateHiddenPartID says why), so
     * gcPrepDObjMatrix asks for this kind in their place. */
    case 0x4B:
    {
#ifndef GC_NO_GMCOLLISION
        FTParts *parts = dobj->user_data.p;
        Vec3f inv = { 1.0F, 1.0F, 1.0F };
        Vec3f sca;
        sb32 flag;
        s32 i, j;

        if ((parts == NULL) || (dobj->parent_gobj == NULL))
        {
            return FALSE;
        }
        flag = ftGetStruct(dobj->parent_gobj)->is_use_animlocks;

        if (parts->transform_update_mode != 0)
        {
            for (i = 0; i < 4; i++)
            {
                for (j = 0; j < 4; j++)
                {
                    m[i * 4 + j] = parts->unk_dobjtrans_0x10[j][i];
                }
            }
            m[12] = m[13] = m[14] = 0.0F;
            m[15] = 1.0F;
        }
        else
        {
            sca = dobj->scale.vec.f;

            if (flag)
            {
                inv = gLBCommonScale;
                parts->vec_scale.x = sca.x = dobj->scale.vec.f.x * gLBCommonScale.x;
                parts->vec_scale.y = sca.y = dobj->scale.vec.f.y * gLBCommonScale.y;
                parts->vec_scale.z = sca.z = dobj->scale.vec.f.z * gLBCommonScale.z;
            }
            gcMtxRotRpyR(m, dobj->rotate.vec.f.x, dobj->rotate.vec.f.y,
                         dobj->rotate.vec.f.z);

            for (i = 0; i < 3; i++)
            {
                f32 r = (&inv.x)[i];

                r = (r != 0.0F) ? 1.0F / r : 0.0F;

                m[i * 4 + 0] *= sca.x * r;
                m[i * 4 + 1] *= sca.y * r;
                m[i * 4 + 2] *= sca.z * r;
            }
            m[3] = dobj->translate.vec.f.x;
            m[7] = dobj->translate.vec.f.y;
            m[11] = dobj->translate.vec.f.z;
        }
        if (flag)
        {
            gLBCommonScale = parts->vec_scale;
        }
        return TRUE;
#else
        return FALSE;
#endif
    }

    /* lb/lbcommon.c:1914-2008 func_ovl0_800CB140, matrix kind 0x49
     * ("adfDMatrixDirecXBillboardSca"): a rotation that keeps the x axis
     * of the fighter joint the DObj's user_data joint names and turns the
     * other two to face the camera as far as that allows -- Samus's
     * up-smash flames, along her arm (ef/efmanager.c:390). A factor like
     * any other (it returns 0), with no translation of its own. Transposed
     * as the whole-matrix kinds above are. DIVERGES: the game hangs in a
     * print loop on a zero-length axis; the port contributes nothing. */
    case 0x49:
    {
#ifndef GC_NO_GMCOLLISION
        DObj *attach_dobj = dobj->user_data.p;
        FTParts *parts;
        CObj *cobj;
        Mtx44f f;
        Vec3f sp50;
        Vec3f dist;
        Vec3f sp38;
        f32 magnitude;
        s32 i, j;

        if ((attach_dobj == NULL) || (gGCCurrentCamera == NULL))
        {
            return FALSE;
        }
        parts = attach_dobj->user_data.p;

        func_ovl2_800EDBA4(attach_dobj);

        magnitude = sqrtf
        (
            SQUARE(parts->mtx_translate[0][0]) +
            SQUARE(parts->mtx_translate[0][1]) +
            SQUARE(parts->mtx_translate[0][2])
        );
        if (magnitude == 0.0F)
        {
            return FALSE;
        }
        magnitude = 1.0F / magnitude;

        sp50.x = f[1][0] = parts->mtx_translate[0][0] * magnitude;
        sp50.y = f[1][1] = parts->mtx_translate[0][1] * magnitude;
        sp50.z = f[1][2] = parts->mtx_translate[0][2] * magnitude;

        cobj = CObjGetStruct(gGCCurrentCamera);

        syVectorDiff3D(&dist, &cobj->vec.eye, &cobj->vec.at);

        if (lbCommonSim3D(&sp50, &dist) < 0.999F)
        {
            syVectorNormCross3D(&sp50, &dist, &sp38);
            lbCommonCross3D(&sp50, &sp38, &dist);
        }
        else
        {
            dist.x = dist.y = dist.z = 0.0F;
            sp38 = dist;
        }
        f[0][0] = sp38.x;
        f[0][1] = sp38.y;
        f[0][2] = sp38.z;

        f[2][0] = dist.x;
        f[2][1] = dist.y;
        f[2][2] = dist.z;

        f[3][1] = f[3][2] = 0.0F;
        f[3][0] = 0.0F;

        /* syMatrixF2LFixedW */
        for (i = 0; i < 3; i++)
        {
            for (j = 0; j < 4; j++)
            {
                m[i * 4 + j] = f[j][i];
            }
        }
        m[12] = m[13] = m[14] = 0.0F;
        m[15] = 1.0F;
        return TRUE;
#else
        return FALSE;
#endif
    }

    default:
        gcWarnKind("dobj", kind);
        return FALSE;
    }
}

/* One XObj's contribution, on its own, for the cross-test: the same
 * matrix gcPrepDObjMatrix is about to multiply onto the stack. Nothing in
 * the game calls it. src/game/ssb64/hosttest_ft.c does, and compares the
 * answer against the lb/lbcommon.c function the game's sGCMatrixFuncList
 * would have dispatched the kind to, converted out of the RSP's fixed
 * point. Exported rather than reached through the walk
 * because the thing under test is the matrix and not the walk. */
sb32 gcDObjLocalMatrixF(float *m, DObj *dobj, s32 kind)
{
    return gcDObjLocalMatrix(m, dobj, kind);
}

/* ---- gcPrepDObjMatrix (objdisplay.c:322-1123) ------------------------
 *
 * What is left of it once the caching is gone is short and is the whole
 * point: multiply each of the DObj's XObj matrices onto the modelview
 * stack, pushing first if a sibling or the tree root will need the
 * current top back. Returns how many were multiplied, which is what
 * gcDrawDObjTree pops on.
 *
 * DIVERGES in one detail: the decomp counts an XObj of a kind its switch
 * does not handle (kind 1, and any kind >= 66 with no matrix function)
 * and emits a gSPMatrix for it anyway, against whatever the XObj's Mtx
 * last held. The port skips such a kind and does not count it, so an
 * unported kind draws unrotated rather than drawing garbage -- and says
 * so once, which is the point.
 */
s32 gcPrepDObjMatrix(Gfx **dl, DObj *dobj)
{
    s32 num = 0;
    s32 i;

    (void)dl;

#ifndef GC_NO_GMCOLLISION
    /* A fighter joint: the game's one XObj is kind 0x4B, which the port's
     * Tra/RotRpyR/Sca stand in for, so ask for that kind in their place
     * (gcDObjLocalMatrix says what it adds). The magnifying glass takes
     * TopN's matrix away by nulling its first XObj (ftdisplaymain.c), and
     * that joint keeps the three-XObj walk it had. */
    if ((dobj->parent_gobj != NULL) &&
        (dobj->parent_gobj->id == nGCCommonKindFighter) &&
        (dobj->user_data.p != NULL) && (dobj->xobjs_num > 0) &&
        (dobj->xobjs[0] != NULL) &&
        (dobj->xobjs[0]->kind != nGCMatrixKindNull))
    {
        mtx4_t local;

        if (gcDObjLocalMatrix(local, dobj, 0x4B) != FALSE)
        {
            if ((dobj->parent == DOBJ_PARENT_NULL) || (dobj->sib_next != NULL))
            {
                gcMtxPush();
            }
            gcMtxMul(local);

            return 1;
        }
    }
#endif
    for (i = 0; i < dobj->xobjs_num; i++)
    {
        XObj *xobj = dobj->xobjs[i];
        mtx4_t local;

        if (xobj == NULL)
        {
            continue;
        }
        /* objdisplay.c:1113 -- kind 2 contributes no matrix. */
        if (xobj->kind == 2)
        {
            continue;
        }
        /* nor does one a later XObj has taken the vectors from */
        if (gcXObjDrivesDObj(dobj, xobj, xobj->kind) == FALSE)
        {
            continue;
        }
        /* Kinds 44 and 0x46 are not a factor of the top but a replacement
         * for it: the billboard keeps the stack's translation and drops
         * the rest (gcMtxRecalcRotRpyRSca, gcMtxRecalcRotZ). Pushed the
         * same as any other, so the sibling's or the root's pop finds what
         * it left. */
        /* lb/lbcommon.c:2011-2020 func_ovl0_800CB2F0, kind 0x4A: no
         * matrix at all -- it returns 1 having written the RSP nothing --
         * but it turns the DObj's rotate.z to follow the joint its
         * user_data names (Yoshi's egg escape, whose next kind spins by
         * it). */
        if (xobj->kind == 0x4A)
        {
#ifndef GC_NO_GMCOLLISION
            DObj *attach_dobj = dobj->user_data.p;

            if (attach_dobj != NULL)
            {
                func_ovl2_800EDBA4(attach_dobj);

                dobj->rotate.vec.f.z = (ftGetParts(attach_dobj)->mtx_translate[0][2] > 0.0F) ?
                attach_dobj->rotate.vec.f.x : -attach_dobj->rotate.vec.f.x;
            }
#endif
            continue;
        }
        if ((xobj->kind == nGCMatrixKindRecalcRotRpyRSca) ||
            (xobj->kind == nGCMatrixKind45) ||
            (xobj->kind == nGCMatrixKind46) ||
            (xobj->kind == nGCMatrixKind48) ||
            (xobj->kind == 0x46) || (xobj->kind == 0x47) ||
            (xobj->kind == 0x48))
        {
            const float *top = sGCMtxStack[sGCMtxTop];
            float at[3];

            at[0] = top[3];
            at[1] = top[7];
            at[2] = top[11];
            switch (xobj->kind)
            {
            case 0x46:
                gcMtxRecalcRotZ(local, at, sGCViewF, dobj->rotate.vec.f.z);
                break;

            case nGCMatrixKind45:
                gcMtxRecalcRotSca(local, at, sGCViewF, dobj->rotate.vec.f.x,
                                  &dobj->scale.vec.f, &gGCScaleX);
                break;

            case nGCMatrixKind46:
                gcMtxRecalcRotSca(local, at, sGCViewF, dobj->rotate.vec.f.z,
                                  &dobj->scale.vec.f, &gGCScaleX);
                break;

            case nGCMatrixKind48:
                gcMtxRecalcMod1(local, at, sGCViewF, &dobj->scale.vec.f,
                                &gGCScaleX);
                break;

            case 0x47:
                gcMtxRecalcRotRpy(local, at, sGCViewF, dobj->rotate.vec.f.x,
                                  dobj->rotate.vec.f.y, NULL, NULL);
                break;

            case 0x48:
                gcMtxRecalcRotRpy(local, at, sGCViewF, dobj->rotate.vec.f.x,
                                  dobj->rotate.vec.f.y, &dobj->scale.vec.f,
                                  &gGCScaleX);
                break;

            default:
                gcMtxRecalcRotRpyRSca(local, at, sGCViewF,
                                      &dobj->scale.vec.f, &gGCScaleX);
                break;
            }
            if (num == 0)
            {
                if ((dobj->parent == DOBJ_PARENT_NULL) ||
                    (dobj->sib_next != NULL))
                {
                    gcMtxPush();
                }
            }
            memcpy(sGCMtxStack[sGCMtxTop], local, sizeof(mtx4_t));
            num++;
            continue;
        }
        if (gcDObjLocalMatrix(local, dobj, xobj->kind) == FALSE)
        {
            continue;
        }
        /* objdisplay.c:565, 599, 633 and 638: the kinds that scale fold
         * the joint's scale.x into gGCScaleX, which is how a billboard
         * below a scaled joint keeps its parent's size (its own kinds,
         * above, discard the rest of the stack) */
        if ((xobj->kind == nGCMatrixKindTraRotRpyRSca) ||
            (xobj->kind == nGCMatrixKindSca))
        {
            gGCScaleX *= dobj->scale.vec.f.x;
        }
        if (num == 0)
        {
            if ((dobj->parent == DOBJ_PARENT_NULL) ||
                (dobj->sib_next != NULL))
            {
                gcMtxPush();
            }
        }
        gcMtxMul(local);
        num++;
    }
    return num;
}

/* objdisplay.c:1125-1425 gcDrawMObjForDObj is 300 lines of texture,
 * palette, combiner and colour GBI driven by the DObj's MObj chain,
 * emitted into a scratch list ahead of the display list that draws with
 * it. Most of it is the N64's: which tile to load, from where, at what
 * size, through which TLUT. All of that is compiled into the pack at
 * build time here -- one pvr_poly_hdr_t per batch, written by
 * tools/export/ssb_packexport.py -- and only the part of it that a MatAnimJoint
 * can *change* while the scene runs has to happen per frame.
 *
 * That part is the colours: objdisplay.c:1206-1220's gDPSetPrimColor and
 * two gSPLightColor, each emitted only when the MObj's flags ask for it.
 * So this hands the chain to the payload, which knows what to do with it
 * (DCDisplay.proc_material, objpvr.h), and the model folds them over its
 * baked ones as it draws.
 *
 * That part is also the picture: an MObj with a
 * sprite array is one whose MatAnimJoint chooses a texture per frame,
 * and objanim.c's parser leaves the choice in texture_id_curr, which the
 * payload reads out with the colours. src/dc/fighter.c has one compiled
 * poly header per frame waiting for it.
 *
 * DIVERGES: an MObj that steps palette_id is handled where the exporter
 * bakes it (the dynamic palette upload); what is still
 * not here is an MObj that moves or scales the tile, and, in the emblem
 * and How to Play exporters, one that selects a palette or a texture.
 * Those exporters refuse a model whose MObjSub sets those flags, so that
 * much remains a gap checked at build time rather than one that shows on
 * screen. */
void gcDrawMObjForDObj(DObj *dobj, Gfx **dl_head)
{
    static sb32 warned = FALSE;
    DCDisplay *disp = dobj->dv;

    (void)dl_head;

    if (dobj->mobj == NULL)
    {
        return;
    }
    if (disp != NULL && disp->proc_material != NULL)
    {
        disp->proc_material(disp, dobj->mobj);
    }
    else if (warned == FALSE)
    {
        warned = TRUE;
        syDebugPrintf("od : MObj on a DObj whose payload has no material\n");
    }
}

/* The N64's gSPDisplayList(dobj->dl): the matrix is composed, now draw
 * what the DObj points at. See objpvr.h for what dv holds here. */
static void gcSubmitDObj(DObj *dobj)
{
    DCDisplay *disp = dobj->dv;

#ifdef DB_DRAW_GUARD
    /* A payload that is not a DCDisplay: name its GObj and skip it. */
    if ((disp->proc_submit != NULL) &&
        (((uintptr_t)disp->proc_submit < 0x8C010000u) ||
         ((uintptr_t)disp->proc_submit >= 0x8C200000u)))
    {
        static int told;
        GObj *g = dobj->parent_gobj;

        if (told++ < 4)
        {
            u32 *w = (u32 *)disp;

            syDebugPrintf("od : bad payload: dobj %p dv %p [%08X %08X %08X "
                          "%08X] gobj %p id %u link %u dl_link %u "
                          "run %p display %p user %p\n",
                          (void *)dobj, (void *)disp, (unsigned)w[0],
                          (unsigned)w[1], (unsigned)w[2], (unsigned)w[3],
                          (void *)g, g ? (unsigned)g->id : 0,
                          g ? (unsigned)g->link_id : 0,
                          g ? (unsigned)g->dl_link_id : 0,
                          g ? (void *)g->func_run : NULL,
                          g ? (void *)g->proc_display : NULL,
                          g ? (void *)g->user_data.p : NULL);
        }
        return;
    }
#endif
    if (disp->proc_submit != NULL)
    {
        disp->proc_submit(disp, sGCMtxStack[sGCMtxTop]);
    }
}

/* objdisplay.c:1427-1451, with the two GBI lines replaced. */
void gcDrawDObjForGObj(GObj *gobj, Gfx **dl_head)
{
    s32 num;
    DObj *dobj = DObjGetStruct(gobj);

    gGCScaleX = 1.0F;

    if (dobj->dv != NULL)
    {
        if (dobj->flags == DOBJ_FLAG_NONE)
        {
            num = gcPrepDObjMatrix(dl_head, dobj);
            gcDrawMObjForDObj(dobj, dl_head);
            gcSubmitDObj(dobj);

            if (num != 0)
            {
                if ((dobj->parent == DOBJ_PARENT_NULL) ||
                    (dobj->sib_next != NULL))
                {
                    gcMtxPop();
                }
            }
        }
    }
}

/* objdisplay.c:1478-1515, verbatim but for the same two lines. Note the
 * tail: a DObj with no previous sibling is a list head, so drawing it
 * draws every sibling after it -- which is how a model with several
 * roots (Hyrule's three layers) is one tree. */
void gcDrawDObjTree(DObj *this_dobj)
{
    s32 num;
    DObj *current_dobj;
    f32 bak;
#ifndef GC_NO_GMCOLLISION
    Vec3f bak_scale;
#endif

    if (!(this_dobj->flags & DOBJ_FLAG_HIDDEN))
    {
        bak = gGCScaleX;
#ifndef GC_NO_GMCOLLISION
        /* ftdisplaymain.c:767 and 831: kind 0x4B's running scale is put
         * back after each joint's subtree */
        bak_scale = gLBCommonScale;
#endif
        num = gcPrepDObjMatrix(NULL, this_dobj);

        if ((this_dobj->dv != NULL) &&
            !(this_dobj->flags & DOBJ_FLAG_NOTEXTURE))
        {
            gcDrawMObjForDObj(this_dobj, NULL);
            gcSubmitDObj(this_dobj);
        }
        if (this_dobj->child != NULL)
        {
            gcDrawDObjTree(this_dobj->child);
        }
        if (num != 0)
        {
            if ((this_dobj->parent == DOBJ_PARENT_NULL) ||
                (this_dobj->sib_next != NULL))
            {
                gcMtxPop();
            }
        }
        gGCScaleX = bak;
#ifndef GC_NO_GMCOLLISION
        gLBCommonScale = bak_scale;
#endif
    }
    if (this_dobj->sib_prev == NULL)
    {
        current_dobj = this_dobj->sib_next;

        while (current_dobj != NULL)
        {
            gcDrawDObjTree(current_dobj);
            current_dobj = current_dobj->sib_next;
        }
    }
}

/* objdisplay.c:1520-1524, verbatim. */
void gcDrawDObjTreeForGObj(GObj *gobj)
{
    gGCScaleX = 1.0F;
    gcDrawDObjTree(DObjGetStruct(gobj));
}

/* objdisplay.c:1454-1476. The four exist because the N64 has four
 * display-list buffers to draw into; here the open PVR list is the
 * camera pass's, not the caller's, so all four are the same walk. They
 * stay because scene code hands them to gcAddGObjDisplay by name. */
void gcDrawDObjDLHead0(GObj *gobj) { gcDrawDObjForGObj(gobj, NULL); }
void gcDrawDObjDLHead1(GObj *gobj) { gcDrawDObjForGObj(gobj, NULL); }
void gcDrawDObjDLHead2(GObj *gobj) { gcDrawDObjForGObj(gobj, NULL); }
void gcDrawDObjDLHead3(GObj *gobj) { gcDrawDObjForGObj(gobj, NULL); }

/* objdisplay.c:1606-1613. On the N64 this rewinds sGCCurrentDL and the
 * four forward-DL heads into the task manager's display-list buffers.
 * There are none here -- the back end submits straight to the tile
 * accelerator as it walks -- so what it resets is the modelview stack.
 * gcSetupObjman calls it (objman.c:2424). */
void gcInitDLs(void)
{
    gcMtxReset();
}

/* objdisplay.c:99-102, verbatim. Scenes hand the object system the matrix
 * routines their DObjs use (dLBCommonFuncMatrixList in the battle
 * scenes); syTaskmanStartTask passes it through from SYTaskmanSetup. */
void gcSetMatrixFuncList(syMtxProcess *proc_mtx)
{
    sGCMatrixFuncList = proc_mtx;
}

/* objdisplay.c:92-97, verbatim. */
void gcSetCameraScissor(s32 top, s32 bottom, s32 left, s32 right)
{
    dGCCameraScissorTop = top;
    dGCCameraScissorBottom = bottom;
    dGCCameraScissorLeft = left;
    dGCCameraScissorRight = right;
}

/* objdisplay.c:3074-3077, verbatim. gcSetupObjman calls it with 0
 * (objman.c:2439). */
void gcSetCameraMatrixMode(s32 val)
{
    sGCCameraMatrixMode = val;
}

/* ---- the camera pass ------------------------------------------------- */

/* objdisplay.c:2616-2689 func_8001663C, which is 30 GBI commands and one
 * piece of arithmetic. The arithmetic is the part that survives: turn the
 * CObj's viewport into a screen rectangle and clamp it to the camera
 * scissor. Everything else -- the ucode load, the render mode, the
 * z-buffer and fill-colour clears, the colour image -- is either the
 * PVR's own business or pvr_scene_begin's.
 *
 * gSYVideoResWidth/Height are the game's resolution (src/dc/sysshim.c),
 * so this rectangle is in the game's coordinates; the port's framebuffer
 * is 640x480, and the rectangle times gDCScreenScale is what the port's vertex
 * mapping (src/dc/fighter.c, through gcGetViewport) puts clip space
 * onto -- the RSP's viewport transform, done in float. */
const DCViewport *gcGetViewport(void)
{
    return &gDCViewport;
}

void gcSetViewportFullScreen(void)
{
    gDCViewport.cx = gDCViewport.hw = (float)gSYVideoResWidth * gDCScreenScale * 0.5F;
    gDCViewport.cy = gDCViewport.hh = (float)gSYVideoResHeight * gDCScreenScale * 0.5F;
}

void gcPrepViewport(const Vp_t *viewport)
{
    s32 ulx, uly, lrx, lry;

    ulx = (viewport->vtrans[0] / 4) - (viewport->vscale[0] / 4);
    uly = (viewport->vtrans[1] / 4) - (viewport->vscale[1] / 4);
    lrx = (viewport->vtrans[0] / 4) + (viewport->vscale[0] / 4);
    lry = (viewport->vtrans[1] / 4) + (viewport->vscale[1] / 4);

    if (ulx < (gSYVideoResWidth / GS_SCREEN_WIDTH_DEFAULT) * dGCCameraScissorLeft)
    {
        ulx = (gSYVideoResWidth / GS_SCREEN_WIDTH_DEFAULT) * dGCCameraScissorLeft;
    }
    if (uly < (gSYVideoResHeight / GS_SCREEN_HEIGHT_DEFAULT) * dGCCameraScissorTop)
    {
        uly = (gSYVideoResHeight / GS_SCREEN_HEIGHT_DEFAULT) * dGCCameraScissorTop;
    }
    if (lrx > gSYVideoResWidth - ((gSYVideoResWidth / GS_SCREEN_WIDTH_DEFAULT) * dGCCameraScissorRight))
    {
        lrx = gSYVideoResWidth - ((gSYVideoResWidth / GS_SCREEN_WIDTH_DEFAULT) * dGCCameraScissorRight);
    }
    if (lry > gSYVideoResHeight - ((gSYVideoResHeight / GS_SCREEN_HEIGHT_DEFAULT) * dGCCameraScissorBottom))
    {
        lry = gSYVideoResHeight - ((gSYVideoResHeight / GS_SCREEN_HEIGHT_DEFAULT) * dGCCameraScissorBottom);
    }
    /* the game's pixels onto the port's 640x480 */
    gDCViewport.cx = (float)(ulx + lrx) * (0.5F * gDCScreenScale);
    gDCViewport.cy = (float)(uly + lry) * (0.5F * gDCScreenScale);
    gDCViewport.hw = (float)(lrx - ulx) * (0.5F * gDCScreenScale);
    gDCViewport.hh = (float)(lry - uly) * (0.5F * gDCScreenScale);
}

/* syTaskmanDrawBorder's baseline (taskman.c): most scenes open one
 * real (non-FILLCOLOR) camera at most per frame, so "outside the last
 * camera's viewport" and "outside the union of every camera's viewport"
 * are the same rectangle. mv/mvopeningmario.c has several real cameras
 * open at once with genuinely different viewports (a narrow posed-fighter box and a wide real-stage view,
 * side by side) -- gcDrawAll's dl-link walk still runs them in
 * dl_link_priority order every OP_POLY pass, so gDCViewport itself ends
 * the pass holding whichever camera happens to sort last, and the
 * border quads painted outside *that one* over-paint every other
 * camera's own, legitimately-drawn region. Tracked here (reset once a
 * frame, expanded by every camera gcPrepCameraViewport runs during the
 * OP_POLY pass) so the border instead blackens only what no camera
 * claimed. A single-camera frame still ends with the union equal to
 * that one camera's viewport, so this changes nothing for
 * single-camera scenes. */
static DCViewport sGCViewportUnion;
static sb32 sGCViewportUnionEmpty = TRUE;

void gcResetViewportUnion(void)
{
    sGCViewportUnionEmpty = TRUE;
}

void gcExpandViewportUnion(void)
{
    float ulx = gDCViewport.cx - gDCViewport.hw;
    float uly = gDCViewport.cy - gDCViewport.hh;
    float lrx = gDCViewport.cx + gDCViewport.hw;
    float lry = gDCViewport.cy + gDCViewport.hh;

    if (sGCViewportUnionEmpty)
    {
        sGCViewportUnion.cx = ulx;
        sGCViewportUnion.cy = uly;
        sGCViewportUnion.hw = lrx;
        sGCViewportUnion.hh = lry;
        sGCViewportUnionEmpty = FALSE;
        return;
    }
    if (ulx < sGCViewportUnion.cx) { sGCViewportUnion.cx = ulx; }
    if (uly < sGCViewportUnion.cy) { sGCViewportUnion.cy = uly; }
    if (lrx > sGCViewportUnion.hw) { sGCViewportUnion.hw = lrx; }
    if (lry > sGCViewportUnion.hh) { sGCViewportUnion.hh = lry; }
}

/* A sprite camera's claim (lbCommonDrawSprite). It publishes no
 * viewport -- its quads are in screen space already -- so it never went
 * through gcPrepCameraViewport, and a frame whose only 3D cameras were
 * narrower than its sprite camera had the border painted over the
 * sprites: the per-fighter openings' name card, five letters across
 * 80-245, showed only the ones inside the posed fighter's 10-110 column
 * for the fourteen tics before the motion window exists. gDCViewport is
 * put back, because the pass's 3D state is not this camera's to move. */
void gcClaimViewport(const Vp_t *viewport)
{
    DCViewport saved = gDCViewport;

    gcPrepViewport(viewport);
    gcExpandViewportUnion();
    gDCViewport = saved;
}

const DCViewport *gcGetViewportUnion(void)
{
    static DCViewport result;

    if (sGCViewportUnionEmpty)
    {
        return &gDCViewport;
    }
    /* sGCViewportUnion holds (ulx, uly, lrx, lry) in its (cx, cy, hw,
     * hh) fields while accumulating -- see gcExpandViewportUnion --
     * converted to a real centre/half-size pair on the way out. */
    result.cx = (sGCViewportUnion.cx + sGCViewportUnion.hw) * 0.5F;
    result.cy = (sGCViewportUnion.cy + sGCViewportUnion.hh) * 0.5F;
    result.hw = (sGCViewportUnion.hw - sGCViewportUnion.cx) * 0.5F;
    result.hh = (sGCViewportUnion.hh - sGCViewportUnion.cy) * 0.5F;
    return &result;
}

/* objdisplay.c:2671-2677, the one clear the port keeps: a camera with
 * COBJ_FLAG_FILLCOLOR fills its clamped viewport with its colour before
 * anything it walks is drawn. On the RDP that overwrites whatever the
 * cameras before it drew and lies under whatever comes after, which is
 * what a quad at the frame's next sprite depth does in the translucent
 * list (lbCommonSpriteFillRect), so it is drawn there and in no other
 * pass. The title's fire camera is the colour behind the flames and its
 * proceed camera the three black frames before mode select
 * (mn/mncommon/mntitle.c:1393, :492). Every other scene's clear camera is
 * dropped for the PVR's own clear, so nothing else asks for this; a 3D
 * camera after a fill would draw under it, because the opaque list's
 * depths are all behind the sprite depths. The fill mode takes no
 * alpha, so neither does the quad.
 *
 * The staff roll
 * made a default camera with COBJ_FLAG_FILLCOLOR and then rendered
 * nothing at all, because this quad covered both of its 3D cameras.
 * The fix is to drop the flag and keep the GObj and the colour, which
 * is what six other ported scenes already do.
 *
 * Worth knowing because the failure is indistinguishable from "nothing
 * was submitted": every instrument reads healthy -- packs resident,
 * fighter_draw_tr returning real triangle counts, no assert, no DL
 * overflow -- since the geometry is drawn and then painted over. Build
 * with -DDB_PVR_BUDGET to tell the two apart: real triangle totals with
 * a black screen mean something is covering them; `tris op 0 pt 0 tr 0`
 * means nothing reached the TA and the fault is upstream.
 *
 * DIVERGES: a camera with an empty camera_mask (Room, Standoff and
 * Clash's own gcMakeDefaultCameraGObj, kept for its Z clear)
 * walks no DL links and draws nothing on the N64
 * either -- objhelper.c never gives it a viewport of its own, so
 * cobj->viewport is still whatever gcAddCameraForGObj default-built,
 * the full frame. That is fine on the N64: an empty camera paints no
 * pixels, so it has nothing to protect. Here it still ran through this
 * function every OP pass and unconditionally expanded
 * sGCViewportUnion to that full-frame default -- at dl_link_priority
 * 100, before any of the scene's own 10-310x10-230 cameras, so the
 * union stayed the whole screen for the rest of the frame regardless
 * of how narrow every real camera's own viewport was. The border
 * (syTaskmanDrawBorder) then had nothing left outside the union to
 * blacken, in every scene built this way. The union is a port
 * invention (the N64 has no such union), so it must track only
 * cameras that can put a pixel on screen. */
void gcPrepCameraViewport(CObj *cobj, s32 buffer_id)
{
    (void)buffer_id;

    gcPrepViewport(&cobj->viewport.vp);

    if (gcGetDrawList() == PVR_LIST_OP_POLY &&
        cobj->parent_gobj->camera_mask != 0)
    {
        gcExpandViewportUnion();
    }

#ifndef GC_NO_GMCOLLISION
    if ((cobj->flags & COBJ_FLAG_FILLCOLOR) &&
        (gcGetDrawList() == PVR_LIST_TR_POLY))
    {
        /* gcPrepViewport's clamped rectangle, back in game pixels */
        s32 ulx = (s32)(gDCViewport.cx - gDCViewport.hw) / 2;
        s32 uly = (s32)(gDCViewport.cy - gDCViewport.hh) / 2;
        s32 lrx = (s32)(gDCViewport.cx + gDCViewport.hw) / 2;
        s32 lry = (s32)(gDCViewport.cy + gDCViewport.hh) / 2;

        lbCommonSpriteFillRect(ulx, uly, lrx, lry, (cobj->color >> 24) & 0xFF,
                               (cobj->color >> 16) & 0xFF,
                               (cobj->color >> 8) & 0xFF, 0xFF);
    }
#endif
}

/* objdisplay.c:2757-3070 gcPrepCameraMatrix (sys/objdisplay.h:71 --
 * public here because gm/gmcamera.c's HUD cameras have display procs of
 * their own that call it, which is how the arrows camera establishes its
 * ortho projection), which is the same shape:
 * walk the camera's XObjs, and for each one build a matrix from the CObj
 * and hand it to the RSP. What the port keeps is the two matrices a
 * camera actually establishes, and the first of the two billboard
 * matrices at the tail (sGCMatrixMod1F, which kind 48 reads); what it
 * drops is the LookAt reflection kinds (12 to 17, RDP reflection
 * mapping), sGCMatrixMod2F (read only by kinds 49 and 50, which nothing
 * in VS mode asks for) and every gSPMatrix.
 *
 * DIVERGES: kinds 6 and 7 differ in the decomp only in where the look-at
 * lands -- multiplied into PROJECTION for 6, loaded into MODELVIEW for 7.
 * Here the view is kept beside the projection and src/dc/fighter.c
 * composes proj * view * joint per model, so the two are one case and
 * the product is the same either way. Likewise persp `scale`, which
 * multiplies the whole matrix and so cancels under the perspective
 * divide; it exists for syMatrixF2L's fixed point, which the port has no
 * step for. */
void gcPrepCameraMatrix(Gfx **dls, CObj *cobj)
{
    s32 i;
    s32 var_s3 = 0;

    (void)dls;

    mtx_identity(sGCViewF);

    for (i = 0; i < cobj->xobjs_num; i++)
    {
        XObj *xobj = cobj->xobjs[i];

        if (xobj == NULL)
        {
            continue;
        }
        switch (xobj->kind)
        {
        case nGCMatrixKindPerspFastF:
        case nGCMatrixKindPerspF:
            mtx_proj(sGCProjF, cobj->projection.persp.fovy,
                     cobj->projection.persp.aspect,
                     cobj->projection.persp.near,
                     cobj->projection.persp.far);
            break;

        /* objdisplay.c:2822-2835. The HUD's own cameras are ortho: the
         * off-screen arrows stand at x = +/-134 in a box the size of the
         * battle viewport. */
        case nGCMatrixKindOrtho:
            mtx_ortho(sGCProjF, cobj->projection.ortho.l,
                      cobj->projection.ortho.r, cobj->projection.ortho.b,
                      cobj->projection.ortho.t, cobj->projection.ortho.n,
                      cobj->projection.ortho.f);
            break;

        /* objman.c:1164's own switch (unmodified, compiled from decomp)
         * groups camera matrix kinds 6-17 onto the one cobj->vec union
         * this branch reads. They are not one shape, though:
         * gcCameraLookAtF has the decomp's three (plain, roll about +Y,
         * roll about +Z), and for the roll kinds up.x is an angle. A plain
         * look-at would misread them: gmCameraMakeMovieCamera's kind 8
         * (the eight per-fighter openings' motion camera), mvending.c's 8
         * and mvopeningroom.c's 14 read their roll as a tilt of the up vector toward world X. */
        case nGCMatrixKindLookAt:   /* 6 */
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
            var_s3 = gcCameraLookAtF(sGCViewF, cobj, xobj->kind);
            break;

        case 1:
        case 2:
            break;

        default:
            gcWarnKind("camera", xobj->kind);
            break;
        }
    }
    /* objdisplay.c:2975-3039, the sGCMatrixMod1F half, verbatim but for
     * what is kept: the mode picks whether the pass recomputes it (a
     * mode that does not leaves the last camera's), and it is a look-at
     * from straight ahead at the camera's own height and horizontal
     * distance -- the view with its yaw removed. The port keeps the
     * look-at's rotation block, transposed into the decomp's row order;
     * the projection it is multiplied by is sGCProjF's job. */
    switch (sGCCameraMatrixMode)
    {
    case 1:
    case 4:
    case 7:
        var_s3 = 0;
        break;

    case 2:
    case 3:
        var_s3 = 1;
        break;

    case 5:
    case 6:
        var_s3 = 2;
        break;
    }
    if (var_s3 != 0)
    {
        f32 eye_z, eye_y, at_y;
        s32 j, k;

        if (var_s3 == 1)
        {
            eye_z = sqrtf(SQUARE(cobj->vec.at.z - cobj->vec.eye.z) + SQUARE(cobj->vec.at.x - cobj->vec.eye.x));
            eye_y = cobj->vec.eye.y;
            at_y = cobj->vec.at.y;
        }
        else
        {
            eye_z = sqrtf(SQUARE(cobj->vec.at.y - cobj->vec.eye.y) + SQUARE(cobj->vec.at.x - cobj->vec.eye.x));
            eye_y = cobj->vec.eye.z;
            at_y = cobj->vec.at.z;
        }
        if (eye_z < 0.0001F)
        {
            memset(sGCMatrixMod1R, 0, sizeof(sGCMatrixMod1R));
        }
        else
        {
            float look[16];

            gcMtxLookAt(look, 0.0F, eye_y, eye_z, 0.0F, at_y, 0.0F, 0.0F, 1.0F, 0.0F);

            for (j = 0; j < 3; j++)
            {
                for (k = 0; k < 3; k++)
                {
                    sGCMatrixMod1R[j][k] = look[k * 4 + j];
                }
            }
        }
    }
}

/* See objpvr.h. The same cases as gcPrepCameraMatrix above, on a
 * CObj that may not be the running pass's. */
void gcCameraMatrixF(CObj *cobj, float *out)
{
    float proj[16], view[16];
    s32 i;

    mtx_identity(proj);
    mtx_identity(view);

    for (i = 0; i < cobj->xobjs_num; i++)
    {
        XObj *xobj = cobj->xobjs[i];

        if (xobj == NULL)
        {
            continue;
        }
        switch (xobj->kind)
        {
        case nGCMatrixKindPerspFastF:
        case nGCMatrixKindPerspF:
            mtx_proj(proj, cobj->projection.persp.fovy,
                     cobj->projection.persp.aspect,
                     cobj->projection.persp.near,
                     cobj->projection.persp.far);
            break;

        case nGCMatrixKindOrtho:
            mtx_ortho(proj, cobj->projection.ortho.l,
                      cobj->projection.ortho.r, cobj->projection.ortho.b,
                      cobj->projection.ortho.t, cobj->projection.ortho.n,
                      cobj->projection.ortho.f);
            break;

        /* The same 6-17 look-at family gcPrepCameraMatrix above carries
         * -- this switch had drifted one label behind it (6 and 7 alone
         * would leave a camera this function is asked about at identity
         * while the drawing pass placed it correctly). */
        case nGCMatrixKindLookAt:   /* 6 */
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
            (void)gcCameraLookAtF(view, cobj, xobj->kind);
            break;

        default:
            break;
        }
    }
    mtx_mul(out, proj, view);
}

/* objdisplay.c:3080-3086, verbatim. */
void gcRunFuncCamera(CObj *cobj, s32 dl_id)
{
    if (cobj->func_camera != NULL)
    {
        cobj->func_camera(cobj, dl_id);
    }
}

#ifdef DB_PERF_PROCS
#include <kos.h>
#include <string.h>
/* -DDB_PERF_PROCS (with DB_PERF): time every GObj display proc, per PVR
 * list, and print the five costliest of each list beside the perf line.
 * Resolve the addresses with addr2line. The timer read is inside the time,
 * so the numbers carry about 2 reads a call. */
#define PP_MAX 40
static struct { void *fn; uint64_t ns; uint32_t calls; } sPP[3][PP_MAX];

void db_perf_proc_add(void *fn, uint64_t ns)
{
    int l = (gcGetDrawList() == PVR_LIST_OP_POLY) ? 0
          : (gcGetDrawList() == PVR_LIST_PT_POLY) ? 1 : 2;
    int i;

    for (i = 0; i < PP_MAX; i++)
    {
        if (sPP[l][i].fn == fn || sPP[l][i].fn == NULL)
        {
            sPP[l][i].fn = fn;
            sPP[l][i].ns += ns;
            sPP[l][i].calls++;
            return;
        }
    }
}

void db_perf_proc_report(uint32_t frames)
{
    static const char *const kL[3] = { "OP", "PT", "TR" };
    uint32_t n = frames ? frames : 1;
    int l, k, j;

    for (l = 0; l < 3; l++)
    {
        uint8_t used[PP_MAX] = { 0 };

        for (k = 0; k < 5; k++)
        {
            int best = -1;

            for (j = 0; j < PP_MAX; j++)
            {
                if (sPP[l][j].fn != NULL && !used[j] &&
                    (best < 0 || sPP[l][j].ns > sPP[l][best].ns))
                    best = j;
            }
            if (best < 0)
                break;
            used[best] = 1;
            dbglog(DBG_INFO, "perf: proc %s %08x %lu us/f, %lu calls/f\n",
                   kL[l], (unsigned)(uintptr_t)sPP[l][best].fn,
                   (unsigned long)(sPP[l][best].ns / n / 1000u),
                   (unsigned long)(sPP[l][best].calls / n));
        }
        memset(sPP[l], 0, sizeof(sPP[l]));
    }
}
#endif

/* objdisplay.c:3089-3113, verbatim. This is the frame's real work: every
 * unhidden GObj on one DL link list, in link order, drawing itself. */
void gcCaptureTaggedGObjs(GObj *camera_gobj, s32 link_id,
                          sb32 is_tag_mask_or_id)
{
    GObj *current_gobj = gGCCommonDLLinks[link_id];

    while (current_gobj != NULL)
    {
        if (!(current_gobj->flags & GOBJ_FLAG_HIDDEN))
        {
            if
            (
                ((is_tag_mask_or_id == 0) && (camera_gobj->camera_tag &  current_gobj->camera_tag)) ||
                ((is_tag_mask_or_id == 1) && (camera_gobj->camera_tag == current_gobj->camera_tag))
            )
            {
                dGCCurrentStatus = nGCStatusDisplaying;
                gGCCurrentDisplay = current_gobj;

#ifdef DB_PERF_PROCS
                {
                    uint64_t t0 = db_perf_now();

                    current_gobj->proc_display(current_gobj);
                    db_perf_proc_add((void *)current_gobj->proc_display,
                                     db_perf_now() - t0);
                }
#else
                current_gobj->proc_display(current_gobj);
#endif
                dGCCurrentStatus = nGCStatusCapturing;

                current_gobj->frame_draw_last = dSYTaskmanFrameCount;
            }
        }
        current_gobj = current_gobj->dl_link_next;
    }
}

/* objdisplay.c:3167-3191 gcCaptureCameraGObj, minus its buffer_mask arm.
 *
 * DIVERGES: the decomp branches on buffer_mask into
 * gcCaptureDoubleBufferGObjs, which wraps a link list's output in a
 * gSPDisplayList/gSPBranchList pair so that a later camera looking at the
 * same list can replay the commands instead of walking it again
 * (gcAddLinkedDL). It is a display-list cache, and there is no display
 * list to cache; the port always walks. A scene that sets buffer_mask
 * therefore draws the same picture, more slowly, which is the right way
 * round for a lost optimisation. */
void gcCaptureCameraGObj(GObj *camera_gobj, sb32 is_tag_mask_or_id)
{
    s32 id = 0;
    u64 camera_mask = camera_gobj->camera_mask;

    while (camera_mask)
    {
        if (camera_mask & 1)
        {
            gcCaptureTaggedGObjs(camera_gobj, id, is_tag_mask_or_id);
        }
        camera_mask >>= 1;
        id++;
    }
}

/* objdisplay.c:3194-3210 func_80017CC8 is the three end-of-camera GBI
 * chores: swap the display-list buffers, close the graphics task, sync
 * the RDP. All three are the RSP's and none has a Dreamcast counterpart
 * -- the frame's pvr_list_finish is the whole of it. Kept as a name
 * because func_80017D3C calls it. */
void func_80017CC8(CObj *cobj)
{
    (void)cobj;
}

/* objdisplay.c:3212-3222. One camera's frame: set the viewport, build the
 * matrices, let the camera's own function draw (a stage wallpaper sits
 * here), then walk every DL link list the camera_mask names.
 *
 * The call order is the decomp's. What wraps it is divergence 2: the
 * frame calls the scene's draw -- and so every camera -- once per PVR
 * list (src/dc/taskman.c syTaskmanCommonTaskDraw), because a PVR list
 * can be opened only once per frame and a scene may have several
 * cameras (a menu's sprite camera beside a 3D one). On the N64 a camera
 * function or a display proc writes into whichever display-list buffer
 * it wants and the RSP sorts it out; here each can only submit into the
 * list that is open, so it is offered both and takes the one it belongs
 * to (gcGetDrawList). */
void func_80017D3C(GObj *gobj, Gfx **dls, s32 id)
{
    CObj *cobj = CObjGetStruct(gobj);
    sb32 is_tag = (cobj->flags & COBJ_FLAG_IDENTIFIER) ? TRUE : FALSE;

    (void)dls;

    DBPERF_LAP_BEGIN();
    gcPrepCameraViewport(cobj, id);
    gcPrepCameraMatrix(NULL, cobj);
    DBPERF_LAP(DBP_CAMPREP);

    /* The modelview stack starts each walk at identity: the default
     * camera's look-at is kind 6, which the decomp multiplies into
     * PROJECTION and not MODELVIEW, so a DObj tree composes
     * model->world and nothing else. */
    gcMtxReset();

    gcRunFuncCamera(cobj, id);
    DBPERF_LAP(DBP_CAMFUNC);
    gcCaptureCameraGObj(gobj, is_tag);
    DBPERF_LAP(DBP_CAPTURE);

    func_80017CC8(cobj);
}

/* objdisplay.c:3224-3227. The default camera's proc_display, handed to
 * every camera gcMakeCameraGObj builds with it (objhelper.c:568) and
 * called by gcDrawAll for each unhidden camera GObj on DL link 64
 * (objman.c:2087-2115). So it is the entry point of a frame's drawing. */
void func_80017DBC(GObj *gobj)
{
    func_80017D3C(gobj, NULL, 0);
}

/* objdisplay.c:3260-3300 func_80017EC0: the camera proc a scene names
 * for a camera with COBJ_FLAG_DLBUFFERS (mn/mnplayers/mnplayersvs.c
 * mnPlayersVSMakeFighterCamera), which opens a second display-list
 * buffer for the DObj trees it draws and branches the first into it.
 * DIVERGES: the buffers are the RSP's; here every camera submits to the
 * open PVR list, so this is the default proc by another name. */
void func_80017EC0(GObj *gobj)
{
    func_80017D3C(gobj, NULL, 0);
}

/* objdisplay.c:2380-2392 gcDrawDObjTreeDLLinksForGObj: the display proc a
 * GObj takes when each of its DObjs carries a DObjDLLink array rather
 * than a display list -- {list_id, Gfx*} pairs, queued into
 * gSYTaskmanDLHeads[list_id], whose render mode that head's own display
 * proc set. ef/efmanager.c names it in fourteen EFDescs and
 * mn/mnplayers/mnplayersvs.c in the spotlight's.
 *
 * DIVERGES: there are no display-list heads and no lists to queue into.
 * Which head a batch would have been queued into is baked into the pack
 * as FPackBatch.bucket (tools/export/ssb_effectexport.py reads the same
 * DObjDLLink array to get it), and src/dc/fighter.c routes each bucket
 * to the PVR list that head's render mode implies. So what is left of
 * this proc is the model draw -- the same substitution
 * src/dc/mnplayersvs.c makes for the spotlight, which takes the layered
 * proc instead because it is a sprite-depth model in a menu; an effect
 * in a battle is geometry at a real depth and takes this one. */
void gcDrawDObjTreeDLLinksForGObj(GObj *gobj)
{
#ifndef SSB_NO_DRAW
    dc_model_proc_display(gobj);
#else
    /* The symbol still has to exist: ef/efmanager.c's EFDescs take its
     * address, and the host cross-tests build those. Nothing in them
     * draws (src/dc/efmanager.c efManagerAddModel's own host arm). */
    (void)gobj;
#endif
}

/* objdisplay.c:1596-1602 gcDrawDObjDLLinksForGObj: the non-tree sibling
 * of the above -- gcDrawDObjDLLinks(dobj, dobj->dl_link) queues one DObj's
 * DObjDLLink array into the heads without recursing the child tree first.
 * wp/wpdisplay.c hands it to gcAddGObjDisplay for a weapon whose model is
 * a lone DObj with DL links (WEAPON_FLAG_DOBJLINKS, no children).
 *
 * DIVERGES: identically to gcDrawDObjTreeDLLinksForGObj -- the heads and
 * the per-link render-mode dance are gone (baked into FPackBatch.bucket),
 * so what is left is the model draw. dc_model_proc_display walks whatever
 * the baked pack holds; a one-DObj weapon pack is exactly that lone DObj,
 * so the tree-vs-single distinction the N64 drew here collapses. */
void gcDrawDObjDLLinksForGObj(GObj *gobj)
{
#ifndef SSB_NO_DRAW
    dc_model_proc_display(gobj);
#else
    (void)gobj;
#endif
}
