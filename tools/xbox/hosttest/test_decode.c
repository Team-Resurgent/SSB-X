/* test_decode.c -- the Xbox port's decoders, run on the build machine.
 *
 *   test_decode tex   <in> <out> <w> <h> <mode3> <stride>
 *   test_decode adpcm <in> <out> <nsamples>
 *
 * `tex` decodes texture memory with Platform/xbox/pvr_texdecode.h (what the
 * renderer uses) into linear texels; `adpcm` decodes Yamaha ADPCM with
 * Platform/xbox/aica/aica_decode.h (what the mixer uses) into s16 samples.
 * tools/xbox/hosttest/run_decode_tests.py compares both against the port's
 * Python references. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../Platform/xbox/pvr_texdecode.h"
#include "../../../Platform/xbox/aica/aica_decode.h"
#include "../../../Platform/xbox/aica/aica_reverb_net.h"

static XbReverb sRv;

static unsigned char *slurp(const char *path, long *n)
{
    FILE *f = fopen(path, "rb");
    unsigned char *b;

    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc(*n + 16);
    fread(b, 1, *n, f);
    fclose(f);
    return b;
}

int main(int argc, char **argv)
{
    long n;
    unsigned char *in;
    FILE *out;

    if (argc < 4)
        return 2;
    in = slurp(argv[2], &n);
    if (!in)
        return 3;
    out = fopen(argv[3], "wb");
    if (!out)
        return 4;
    if (strcmp(argv[1], "tex") == 0 && argc == 8)
    {
        uint32_t w = (uint32_t)strtoul(argv[4], 0, 0), h = (uint32_t)strtoul(argv[5], 0, 0);
        uint32_t m3 = (uint32_t)strtoul(argv[6], 0, 0), stride = (uint32_t)strtoul(argv[7], 0, 0);
        uint8_t *lin = malloc(w * h * 2);
        int bpp = pvr_td_decode(in, w, h, m3, stride, (uint32_t)n, lin);

        if (bpp == 0)
            return 5;
        fwrite(lin, 1, w * h * bpp, out);
    }
    else if (strcmp(argv[1], "adpcm") == 0 && argc == 5)
    {
        uint32_t ns = (uint32_t)strtoul(argv[4], 0, 0), i;
        Voice v;

        memset(&v, 0, sizeof(v));
        v.fmt = 2;
        v.lsa = 0;
        v.lea = ns;
        v.data = in;
        v.scale = 127;
        for (i = 0; i < ns; i++)
        {
            int16_t s = (int16_t)decode_next(&v);

            fwrite(&s, 2, 1, out);
        }
    }
    else if (strcmp(argv[1], "reverb") == 0)
    {
        /* float32 samples in, float32 samples out */
        long i;

        for (i = 0; i < n / 4; i++)
        {
            float x, y;

            memcpy(&x, in + 4 * i, 4);
            y = xb_reverb_sample(&sRv, x);
            fwrite(&y, 4, 1, out);
        }
    }
    else
    {
        return 2;
    }
    fclose(out);
    return 0;
}
