/* shade_oracle.c -- src/dc/ftshade.h run on the build host, so that
 * tools/check/shade_check.py can hold the port's lighting arithmetic against
 * an independent model of the RSP and RDP's. Built by the check itself.
 *
 * Two protocols, chosen by argv[1], records on stdin and answers on
 * stdout in host order:
 *
 *   shade  {u32 prim, u32 light1, u32 light2, float nl} -> u32 argb
 *          the colour ft_shade_lit gives a vertex whose N.L is nl, in
 *          a batch with those three colours -- the same three words an
 *          FPackBatch carries, unpacked the way material_of does
 *   light  {float angle_x, float angle_y} -> float dir[3]
 *          what dc_model_set_light_angles leaves in sLight
 *
 * Only the two ftshade.h functions are exercised; the trig they call is
 * src/dc/lbcommon.c's, linked in with its sprite half garbage-collected
 * away (the check passes --gc-sections). */
#include <stdio.h>
#include <string.h>

#include "ftshade.h"

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "shade") == 0)
    {
        struct { uint32_t prim, light1, light2; float nl; } in;

        while (fread(&in, sizeof in, 1, stdin) == 1)
        {
            float base[3], dif[3], amb[3];
            uint32_t out;

            ft_shade_unpack(in.prim, base);
            ft_shade_unpack(in.light1, dif);
            ft_shade_unpack(in.light2, amb);
            out = ft_shade_lit(base, amb, dif, in.nl, in.prim & 0xFF);
            fwrite(&out, sizeof out, 1, stdout);
        }
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "shadefx") == 0)
    {
        struct { uint32_t prim, light1, light2; float nl; } in;

        while (fread(&in, sizeof in, 1, stdin) == 1)
        {
            float base[3], dif[3], amb[3];
            FtShadeFx fx;
            uint32_t out;

            ft_shade_unpack(in.prim, base);
            ft_shade_unpack(in.light1, dif);
            ft_shade_unpack(in.light2, amb);
            ft_shade_fx_prep(&fx, base, amb, dif);
            out = ft_shade_lit_fx(&fx, in.nl, in.prim & 0xFF);
            fwrite(&out, sizeof out, 1, stdout);
        }
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "fogfx") == 0)
    {
        struct { uint32_t prim, light1, light2; float nl; uint32_t fog; } in;

        while (fread(&in, sizeof in, 1, stdin) == 1)
        {
            float base[3], dif[3], amb[3];
            FtShadeFx fx;
            uint32_t offset = 0, out;

            ft_shade_unpack(in.prim, base);
            ft_shade_unpack(in.light1, dif);
            ft_shade_unpack(in.light2, amb);
            ft_shade_fog(base, &offset, in.fog);
            ft_shade_fx_prep(&fx, base, amb, dif);
            out = ft_shade_add_offset(
                ft_shade_lit_fx(&fx, in.nl, in.prim & 0xFF), offset);
            fwrite(&out, sizeof out, 1, stdout);
        }
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "fog") == 0)
    {
        struct { uint32_t prim, light1, light2; float nl; uint32_t fog; } in;

        while (fread(&in, sizeof in, 1, stdin) == 1)
        {
            float base[3], dif[3], amb[3];
            uint32_t offset = 0, out;

            ft_shade_unpack(in.prim, base);
            ft_shade_unpack(in.light1, dif);
            ft_shade_unpack(in.light2, amb);
            ft_shade_fog(base, &offset, in.fog);
            out = ft_shade_add_offset(
                ft_shade_lit(base, amb, dif, in.nl, in.prim & 0xFF), offset);
            fwrite(&out, sizeof out, 1, stdout);
        }
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "light") == 0)
    {
        float in[2];

        while (fread(in, sizeof in, 1, stdin) == 1)
        {
            float out[3];

            ft_light_dir(in[0], in[1], out);
            fwrite(out, sizeof out, 1, stdout);
        }
        return 0;
    }
    fprintf(stderr, "usage: %s shade|shadefx|fog|fogfx|light < records\n", argv[0]);
    return 2;
}
