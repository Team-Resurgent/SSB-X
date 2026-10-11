/* mtx.h -- the port's 4x4 float matrix ops, behind one switch.
 *
 * Row-major storage, column-vector semantics: out = A * v, translation in
 * m[3], m[7], m[11]. This is the transpose of what the game's sys/matrix.c
 * writes, so matrices compose the usual way round (parent * local); the
 * game's matrix row/column layout is the reference.
 *
 * MTX_BACKEND picks the implementation. MTX_BACKEND_SCALAR is the plain C
 * these bodies were lifted from and is the default; MTX_BACKEND_SH4ZAM runs
 * the same algebra on the SH4's FPU back-bank. Both stay in the tree so a
 * regression can be bisected against the scalar path on the target.
 *
 * SH4ZAM stores column-major, so on our row-major arrays load_4x4(A) puts
 * A-transpose in XMTRX and load/apply/store reverses the operands. The two
 * idioms that follow differ, and mixing them up still renders something:
 *
 *   matmul to memory   raw loads, operands swapped   (no transpose ops)
 *   matrix to FTRV     transpose an odd number of times
 *
 * scripts/test_host.sh checks that algebra both ways round. It cannot check
 * accuracy: on the host SH4ZAM falls back to exact libm, so FSCA and FSRRA
 * divergence is invisible there and needs an on-target A/B run.
 */
#ifndef SSB_DC_MTX_H
#define SSB_DC_MTX_H

#include <math.h>
#include <string.h>

#define MTX_BACKEND_SCALAR 0
#define MTX_BACKEND_SH4ZAM 1

#ifndef MTX_BACKEND
#define MTX_BACKEND MTX_BACKEND_SCALAR
#endif

/* 8-byte aligned because that is what SH4ZAM's loads and KOS's mat_* both
 * require; declaring the storage aligned beats remembering which call takes
 * an unaligned variant. */
typedef float mtx4_t[16] __attribute__((aligned(8)));

/* ssb-decomp-re/include/macros.h:32 is `#define __attribute__(x)`, which
 * would silently drop that alignment in any file that reaches a decomp
 * header before this one -- and then Fighter.mtx sits at a different
 * offset here than in the translation unit next door. src/dc/decomp/
 * shadows macros.h to undo it; this is the check that the shadow is on
 * the include path. */
_Static_assert(__alignof__(mtx4_t) == 8,
               "mtx4_t lost its alignment -- see src/dc/decomp/macros.h");

#if MTX_BACKEND == MTX_BACKEND_SH4ZAM
#include <sh4zam/shz_sh4zam.h>
#endif

/* out = a * b. Aliasing is allowed: the render core passes t for both. */
static inline void mtx_mul(float *out, const float *a, const float *b)
{
#if MTX_BACKEND == MTX_BACKEND_SH4ZAM
    /* XMTRX = b_transposed * a_transposed, stored back as (a*b)_transposed,
     * which read row-major is a*b. Hence the swap. */
    shz_xmtrx_load_apply_store_unaligned_4x4(out, b, a);
#else
    float t[16];
    int r, c;
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++)
            t[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c] +
                           a[r * 4 + 1] * b[1 * 4 + c] +
                           a[r * 4 + 2] * b[2 * 4 + c] +
                           a[r * 4 + 3] * b[3 * 4 + c];
    memcpy(out, t, sizeof(t));
#endif
}

/* m * (x,y,z,1), w kept: the near clip runs on the undivided w, so nothing
 * here may fold in the perspective divide. */
static inline void mtx_apply(const float *m, float x, float y, float z,
                             float *ox, float *oy, float *oz, float *ow)
{
    *ox = m[0] * x + m[1] * y + m[2] * z + m[3];
    *oy = m[4] * x + m[5] * y + m[6] * z + m[7];
    *oz = m[8] * x + m[9] * y + m[10] * z + m[11];
    *ow = m[12] * x + m[13] * y + m[14] * z + m[15];
}

/* guPerspective (sys/matrix.c syMatrixPersp), fovy in degrees. */
static inline void mtx_proj(float *m, float fovy, float aspect, float znear,
                            float zfar)
{
    float f = 1.0f / tanf(fovy * 0.5f * 3.14159265f / 180.0f);
    memset(m, 0, 16 * sizeof(float));
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (znear + zfar) / (znear - zfar);
    m[11] = 2.0f * znear * zfar / (znear - zfar);
    m[14] = -1.0f;
}

/* guOrtho (sys/matrix.c syMatrixOrthoF), transposed into the row-major
 * layout mtx_proj uses: the decomp writes the translation down row 3 of a
 * Mtx44f and everything here is out = M * v. The `scale` argument the game
 * passes is the RSP's fixed-point rescale and has no meaning in float, as
 * for the perspective pair. */
static inline void mtx_ortho(float *m, float l, float r, float b, float t,
                             float n, float f)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = 2.0f / (r - l);
    m[3] = -(r + l) / (r - l);
    m[5] = 2.0f / (t - b);
    m[7] = -(t + b) / (t - b);
    m[10] = -2.0f / (f - n);
    m[11] = -(f + n) / (f - n);
    m[15] = 1.0f;
}

static inline void mtx_translate(float *m, float x, float y, float z)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
    m[3] = x;
    m[7] = y;
    m[11] = z;
}

/* ---- the active matrix ----
 *
 * On the SH4 this is XMTRX, the FPU's back-bank 4x4: FTRV transforms a
 * vector against it in one instruction, so a run of vertices sharing a
 * matrix costs one chain build plus one instruction each. The scalar
 * backend keeps the same shape over a plain array so both paths run the
 * same code.
 *
 * XMTRX survives interrupts and thread switches (KOS's irq_context saves
 * frbank[16], and SH4ZAM gives each thread its own copy), but KOS builds
 * with -m4-single rather than -m4-single-only, so GCC may use the back
 * bank for `double`. Hold it across the transform loop and nothing else:
 * no dbglog, no libm, no call you did not write.
 */

/* The scalar backend needs somewhere to keep it; XMTRX is registers. One
 * translation unit must carry MTX_ACTIVE_STORAGE (fighter.c does). */
#if MTX_BACKEND == MTX_BACKEND_SCALAR
extern mtx4_t gMtxActive;
#define MTX_ACTIVE_STORAGE mtx4_t gMtxActive
#else
#define MTX_ACTIVE_STORAGE extern int mtx_active_storage_unused
#endif

/* Active = a * b * c. The renderer's chain is proj * modelview * joint. */
static inline void mtx_load3(const float *a, const float *b, const float *c)
{
#if MTX_BACKEND == MTX_BACKEND_SH4ZAM
    /* load_transpose puts our row-major array into XMTRX unmodified, and
     * apply_transpose post-multiplies by another, so this reads in the
     * order it is written. */
    shz_xmtrx_load_transpose_unaligned_4x4(a);
    shz_xmtrx_apply_transpose_unaligned_4x4(b);
    shz_xmtrx_apply_transpose_unaligned_4x4(c);
#else
    float t[16];
    mtx_mul(t, a, b);
    mtx_mul(gMtxActive, t, c);
#endif
}

/* Active * (x,y,z,1) into out[4], w kept for the near clip. */
static inline void mtx_xform(float x, float y, float z, float *out)
{
#if MTX_BACKEND == MTX_BACKEND_SH4ZAM
    shz_vec4_t v, r;
    v.x = x; v.y = y; v.z = z; v.w = 1.0f;
    r = shz_xmtrx_transform_vec4(v);
    out[0] = r.x; out[1] = r.y; out[2] = r.z; out[3] = r.w;
#else
    mtx_apply(gMtxActive, x, y, z, &out[0], &out[1], &out[2], &out[3]);
#endif
}

/* ---- scalar math with an SH4 instruction behind it ----
 *
 * These are approximations on the SH4 and exact libm off it, so they belong
 * in the renderer and nowhere near the code whose floats must match the
 * N64's. scripts/test_purity.sh enforces that by grepping the emitted code;
 * see its header for why the host cross-tests cannot.
 */

/* FSCA takes a 16-bit angle index: radians * 10430.37835, truncated. The
 * product stops being an exact integer past 2^24, i.e. about 1608 rad, and
 * the angle then quantises more coarsely than the table does. Three of the
 * ROM's 1772 animations go past that (tools/check/fsca_range_check.py), so
 * large angles take libm and the FSCA path keeps its 9.59e-05 rad bound
 * unconditionally. The compare costs far less than the two calls it skips. */
#define MTX_FSCA_LIMIT 1024.0f

static inline void mtx_sincos(float a, float *s, float *c)
{
#if MTX_BACKEND == MTX_BACKEND_SH4ZAM
    if (a > -MTX_FSCA_LIMIT && a < MTX_FSCA_LIMIT)
    {
        shz_sincos_t sc = shz_sincosf(a);
        *s = sc.sin;
        *c = sc.cos;
        return;
    }
#endif
    *s = sinf(a);
    *c = cosf(a);
}

/* FIPR: a 4-wide dot in one instruction, rounded once at the end rather
 * than per term, so it is a different number from three multiply-adds and
 * not a worse one. */
static inline float mtx_dot3(float x1, float y1, float z1,
                             float x2, float y2, float z2)
{
#if MTX_BACKEND == MTX_BACKEND_SH4ZAM
    shz_vec3_t a, b;
    a.x = x1; a.y = y1; a.z = z1;
    b.x = x2; b.y = y2; b.z = z2;
    return shz_vec3_dot(a, b);
#else
    return x1 * x2 + y1 * y2 + z1 * z2;
#endif
}

/* FSRRA: 1/sqrt in one instruction, good to about 2^-21. Collapses the
 * sqrtf and the reciprocal that followed it. */
static inline float mtx_rsqrt(float x)
{
#if MTX_BACKEND == MTX_BACKEND_SH4ZAM
    return shz_inv_sqrtf(x);
#else
    return 1.0f / sqrtf(x);
#endif
}

static inline void mtx_identity(float *m)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

#endif /* SSB_DC_MTX_H */
