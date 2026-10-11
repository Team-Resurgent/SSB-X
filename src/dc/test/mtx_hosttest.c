/* mtx_hosttest -- src/dc/mtx.h's algebra, both backends, on the build host.
 *
 * Two things are checked, and only two:
 *
 *   1. The refactor is inert. mtx_mul/mtx_apply/mtx_proj must agree
 *      BIT-EXACTLY with the scalar bodies they were lifted from
 *      (fighter.c and the five build_proj copies, reproduced below).
 *   2. The SH4ZAM backend computes the same algebra. SH4ZAM stores
 *      column-major and load/apply/store reverses the operands, so this is
 *      where a transpose or operand-order slip shows up -- and a wrong
 *      order still renders something, which is why it is worth a test.
 *
 * What this CANNOT check is accuracy. On the host SHZ_BACKEND is SHZ_SW,
 * where shz_sincosf is sinf/cosf and shz_inv_sqrtf_fsrra is 1/sqrtf: the
 * FSCA and FSRRA paths that run on the SH4 do not exist here. Divergence
 * from those is invisible to this test by construction and belongs to an
 * on-target A/B run (MTX_BACKEND=SH4ZAM against the scalar build). Do not grow this file into a parity claim.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtx.h"

MTX_ACTIVE_STORAGE;

#ifdef MTX_WITH_SH4ZAM
#include <sh4zam/shz_sh4zam.h>
#endif

static int gFail;

/* The pre-refactor bodies, verbatim from fighter.c:17-37 at 55ad0d2. */
static void ref_mul(float *out, const float *a, const float *b)
{
    float t[16];
    int r, c;
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++)
            t[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c] +
                           a[r * 4 + 1] * b[1 * 4 + c] +
                           a[r * 4 + 2] * b[2 * 4 + c] +
                           a[r * 4 + 3] * b[3 * 4 + c];
    memcpy(out, t, sizeof(t));
}

static void ref_apply(const float *m, float x, float y, float z,
                      float *ox, float *oy, float *oz, float *ow)
{
    *ox = m[0] * x + m[1] * y + m[2] * z + m[3];
    *oy = m[4] * x + m[5] * y + m[6] * z + m[7];
    *oz = m[8] * x + m[9] * y + m[10] * z + m[11];
    *ow = m[12] * x + m[13] * y + m[14] * z + m[15];
}

static void ref_proj(float *m, float fovy, float aspect, float znear,
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

static uint32_t rnd(void)
{
    static uint32_t s = 0x1234567u;
    s = s * 1664525u + 1013904223u;
    return s;
}

static float rndf(void)
{
    return (float)((double)(rnd() >> 8) / 8388608.0 - 1.0) * 100.0f;
}

static void bits(const char *what, const float *a, const float *b, int n)
{
    int i;
    for (i = 0; i < n; i++)
        if (memcmp(&a[i], &b[i], sizeof(float)) != 0)
        {
            printf("  FAIL %s: [%d] %.9g != %.9g\n", what, i,
                   (double)a[i], (double)b[i]);
            gFail++;
            return;
        }
}

/* Tolerant compare, for the SH4ZAM path: a different summation order is
 * expected there, a different answer is not. */
#if MTX_BACKEND == MTX_BACKEND_SH4ZAM
static void near(const char *what, const float *a, const float *b, int n,
                 float tol)
{
    int i;
    for (i = 0; i < n; i++)
    {
        float d = fabsf(a[i] - b[i]);
        float s = fabsf(a[i]) + fabsf(b[i]);
        if (d > tol * (s > 1.0f ? s : 1.0f))
        {
            printf("  FAIL %s: [%d] %.9g vs %.9g (delta %.3g)\n", what, i,
                   (double)a[i], (double)b[i], (double)d);
            gFail++;
            return;
        }
    }
}
#endif

int main(void)
{
    int it;

    printf("mtx_hosttest: backend=%s\n",
           MTX_BACKEND == MTX_BACKEND_SH4ZAM ? "SH4ZAM (SW fallback)"
                                             : "scalar");

    for (it = 0; it < 20000; it++)
    {
        mtx4_t a, b, got, want;
        float x = rndf(), y = rndf(), z = rndf();
        float g[4], w[4];
        int i;

        for (i = 0; i < 16; i++)
        {
            a[i] = rndf();
            b[i] = rndf();
        }

        mtx_mul(got, a, b);
        ref_mul(want, a, b);
#if MTX_BACKEND == MTX_BACKEND_SH4ZAM
        near("mtx_mul", got, want, 16, 1e-5f);
#else
        bits("mtx_mul", got, want, 16);
#endif

        /* Aliasing: the render core passes the same buffer as out and as an input. */
        memcpy(got, a, sizeof(got));
        mtx_mul(got, got, b);
        ref_mul(want, a, b);
#if MTX_BACKEND == MTX_BACKEND_SH4ZAM
        near("mtx_mul aliased", got, want, 16, 1e-5f);
#else
        bits("mtx_mul aliased", got, want, 16);
#endif

        mtx_apply(a, x, y, z, &g[0], &g[1], &g[2], &g[3]);
        ref_apply(a, x, y, z, &w[0], &w[1], &w[2], &w[3]);
        bits("mtx_apply", g, w, 4);

        /* The renderer's chain: proj * modelview * joint, then one
         * transform per vertex. This is where an operand-order or
         * transpose slip in the SH4ZAM path shows up, and a wrong order
         * still renders something, so it is worth asserting. */
        {
            mtx4_t mv, m, t;
            float p[4];
            for (i = 0; i < 16; i++)
            {
                mv[i] = rndf() * 0.01f;
                m[i] = rndf() * 0.01f;
            }
            mtx_load3(a, mv, m);
            mtx_xform(x, y, z, p);
            ref_mul(t, a, mv);
            ref_mul(t, t, m);
            ref_apply(t, x, y, z, &w[0], &w[1], &w[2], &w[3]);
#if MTX_BACKEND == MTX_BACKEND_SH4ZAM
            near("mtx_load3 + mtx_xform", p, w, 4, 1e-4f);
#else
            bits("mtx_load3 + mtx_xform", p, w, 4);
#endif
        }

    }

    /* The znear/zfar spread the port has used. */
    {
        static const float zf[] = { 1000.0f, 4000.0f, 8000.0f, 24000.0f };
        static const float fov[] = { 45.0f, 30.0f, 60.0f };
        size_t i, j;
        for (i = 0; i < sizeof(zf) / sizeof(zf[0]); i++)
            for (j = 0; j < sizeof(fov) / sizeof(fov[0]); j++)
            {
                mtx4_t got, want;
                mtx_proj(got, fov[j], 320.0f / 240.0f, 10.0f, zf[i]);
                ref_proj(want, fov[j], 320.0f / 240.0f, 10.0f, zf[i]);
                bits("mtx_proj", got, want, 16);
            }
    }

#ifdef MTX_WITH_SH4ZAM
    /* The convention itself, stated as an assertion rather than a comment:
     * on our row-major arrays, load/apply/store reverses the operands. */
    {
        mtx4_t a, b, got, want;
        int i;
        for (i = 0; i < 16; i++)
        {
            a[i] = rndf();
            b[i] = rndf();
        }
        shz_xmtrx_load_apply_store_unaligned_4x4(got, b, a);
        ref_mul(want, a, b);
        near("shz load/apply/store reverses operands", got, want, 16, 1e-5f);

        /* load_transpose of a row-major array puts it in XMTRX unmodified,
         * so an ftrv then gives a*v -- the Stage 5 vertex path. */
        {
            shz_vec4_t v, r;
            v.x = 1.5f; v.y = -2.25f; v.z = 3.75f; v.w = 1.0f;
            float w[4];
            shz_xmtrx_load_transpose_unaligned_4x4(a);
            r = shz_xmtrx_transform_vec4(v);
            ref_apply(a, v.x, v.y, v.z, &w[0], &w[1], &w[2], &w[3]);
            near("shz load_transpose + ftrv == mtx_apply",
                 (const float *)&r, w, 4, 1e-5f);
        }
    }
#endif

    if (gFail)
    {
        printf("mtx_hosttest: %d FAILURES\n", gFail);
        return 1;
    }
    printf("mtx_hosttest: ok\n");
    return 0;
}
