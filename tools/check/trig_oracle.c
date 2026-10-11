/* Host driver for the port's copies of libultra's __sinf and __cosf,
 * used by tools/check/trig_check.py. Sweeps float bit patterns through both and
 * against the C library's, and reports what it found; this never runs on
 * the Dreamcast.
 *
 * The comparison is not a correctness test -- libultra's polynomial is
 * not required to be correctly rounded, and the point of porting it is
 * that the N64's answer is the right one whatever the C library says.
 * What it does is put a number on the difference, and catch the mistake
 * that would otherwise look plausible: a constant table read with its
 * words the wrong way round makes the polynomial collapse and sin(x)
 * return x, which for small angles is nearly right.
 *
 * Usage: trig_oracle <stride>   (1 sweeps all 2^32 bit patterns)
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern float __sinf(float x);
extern float __cosf(float x);
extern float __libm_qnan_f;

/* The argument bands libultra's own code branches on (gusinf.c). */
#define XPT(ix) (((ix) >> 22) & 0x1ff)

static unsigned bits(float f)
{
    unsigned u;

    memcpy(&u, &f, sizeof(u));
    return u;
}

static unsigned ulps(float a, float b)
{
    unsigned ia = bits(a), ib = bits(b);

    if ((ia >> 31) != (ib >> 31))
    {
        return (ia & 0x7fffffffU) + (ib & 0x7fffffffU);
    }
    return ia > ib ? ia - ib : ib - ia;
}

int main(int argc, char **argv)
{
    unsigned long long stride = (argc > 1) ? strtoull(argv[1], NULL, 0) : 1021;
    unsigned long long i, tested = 0, differ = 0, big = 0, nans = 0;
    unsigned worst = 0, worst_at = 0;
    unsigned long long hist[4] = { 0, 0, 0, 0 };

    for (i = 0; i <= 0xFFFFFFFFULL; i += stride)
    {
        unsigned u = (unsigned)i;
        float x, us, uc, rs, rc;
        int ix;

        memcpy(&x, &u, sizeof(x));
        memcpy(&ix, &u, sizeof(ix));

        us = __sinf(x);
        uc = __cosf(x);

        if (x != x)
        {
            /* A NaN in: libultra returns its own quiet NaN, both ways. */
            nans++;
            if (bits(us) != bits(__libm_qnan_f) || bits(uc) != bits(__libm_qnan_f))
            {
                printf("FAIL nan %08x -> %08x %08x\n", u, bits(us), bits(uc));
                return 1;
            }
            continue;
        }
        if (XPT(ix) >= 0x136)
        {
            /* |x| >= 2^28, and an infinity: libultra gives up and
             * returns zero rather than reducing an argument it cannot
             * reduce. The C library returns the true sine, or a NaN for
             * an infinity, so there is nothing to compare -- only to
             * confirm the port gives up in the same place. */
            big++;
            if (bits(us) != 0 || bits(uc) != 0)
            {
                printf("FAIL big %08x -> %08x %08x\n", u, bits(us), bits(uc));
                return 1;
            }
            continue;
        }
        rs = sinf(x);
        rc = cosf(x);
        tested++;

        {
            unsigned k = ulps(us, rs);
            unsigned l = ulps(uc, rc);

            if (l > k)
            {
                k = l;
            }
            if (k)
            {
                differ++;
            }
            if (k > worst)
            {
                worst = k;
                worst_at = u;
            }
            hist[k < 3 ? k : 3]++;
        }
    }
    printf("stride %llu\n", stride);
    printf("compared %llu\n", tested);
    printf("differ %llu\n", differ);
    printf("worst %u at %08x\n", worst, worst_at);
    printf("hist %llu %llu %llu %llu\n", hist[0], hist[1], hist[2], hist[3]);
    printf("gaveup %llu\n", big);
    printf("nans %llu\n", nans);
    printf("qnan %08x\n", bits(__libm_qnan_f));
    return 0;
}
