#!/usr/bin/env python3
"""ssb64-dc: the fighter core's lighting against a model of the N64's.

Two things the port gets a fighter's colour from are checked here, each
against a second implementation that shares nothing with the first but
the numbers going in:

  * the shade -- ours is src/dc/ftshade.h ft_shade_lit, the lines
    fighter.c lights every vertex by, run on the host through
    tools/check/shade_oracle.c; theirs is the RSP's lighting followed by the
    RDP's combiner, written out here as the two separate stages they are:
    SHADE = clamp255(ambient + diffuse * N.L) per channel first, then
    PRIM * SHADE / 255. The order is the point. The port clamps the sum first; multiplying first and
    clamping the product would let a channel keep
    growing past where the N64 stops it; each channel saturates at a
    different N.L, so the hue slides, and Mario's skin (prim FFE199,
    light2 8C6666) came out yellow. Every batch of every exported fighter
    is swept over N.L from below zero to one.

  * the light -- ours is ft_light_dir, what dc_model_set_light_angles
    leaves for fighter_frame; theirs is ft/ftdisplaylights.c
    ftDisplayLightsDrawReflect re-read here in Python over the decomp's
    own sine table (lb/lbcommon.c dLBCommonSinLookup), down to the three
    s8 bytes the RSP would have been handed. Ours must be those bytes on
    the 0x7F == 1.0 scale, to within the byte's own truncation. Swept
    over every exported stage's MPGroundData.light_angle, the character
    select's pair, and a grid of angles across all four quadrants.

Usage: python3 tools/check/shade_check.py [--rom <rom.z64>] [-v]
"""
import math
import os
import re
import struct
import subprocess
import sys
import tempfile

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_meshexport as M       # noqa: E402  (ROM_DEFAULT, build_fighter)
import ssb_paths as P            # noqa: E402
import ssb_stageexport as S      # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, "src", "dc")
HOSTSTUBS = os.path.join(ROOT, "src", "game", "ssb64", "hoststubs")

# The fighters that export today: src/game/ssb64/Makefile builds
# one pack, Mario's. Add a name here when a second pack lands.
FIGHTERS = ["Mario"]

# The stages that export today, the list tools/check/rendermode_check.py walks.
STAGES = ["Castle", "Sector", "Jungle", "Zebes", "Hyrule", "Yoster",
          "Inishie"]

# mn/mnplayersvs.c: the pair the character select hands
# scSubsysFighterSetLightParams (src/dc/mnplayersvs.c).
MENU_LIGHT = (45.0, 45.0)

# ft_shade_lit rounds a float product to the nearest byte; the reference
# keeps the float. Half a byte plus float noise, so one byte is the bound,
# and the old order of operations misses it by up to 61.
SHADE_TOL = 1.0
# The fog adds two more truncations (the offset's byte and the float
# base's scale) to the shade's one.
FOG_TOL = 2.0
# ftDisplayLightsDrawReflect truncates vec * 100 to an s8; ours keeps the
# float, so the two agree to within one byte -- and, with the truncation
# taken out, to the last place of a float that has been through six
# single-precision operations at a magnitude of 100. A slip in the
# formula is a tenth of the vector or more.
LIGHT_BYTE_TOL = 1.0
LIGHT_FLOAT_TOL = 1e-3

# lbcommon.c is host-buildable under the same flags the ssb64 host
# suite uses (Makefile HOST_CFLAGS); its sprite half is compiled with it
# and dropped again at link time.
DECOMP_INC = ["-I", os.path.join(SRC, "decomp"),
              "-I", os.path.join(P.DECOMP_DIR, "src"),
              "-idirafter", os.path.join(P.DECOMP_DIR, "include"),
              "-D_LANGUAGE_C", "-DF3DEX_GBI_2", "-DREGION_US"]


def build(tmpdir):
    exe = os.path.join(tmpdir, "shade_oracle")
    oracle_o = os.path.join(tmpdir, "shade_oracle.o")
    lbcommon_o = os.path.join(tmpdir, "lbcommon.o")
    subprocess.run(["cc", "-O2", "-std=gnu99", "-Wall", "-Wextra", "-Werror",
                    "-c", os.path.join(HERE, "shade_oracle.c"),
                    "-I", SRC, "-o", oracle_o], check=True)
    subprocess.run(["cc", "-O2", "-std=gnu99", "-Wall", "-DFT_HOSTTEST", "-DSSB_NO_DRAW",
                    "-ffunction-sections", "-fdata-sections",
                    "-c", os.path.join(SRC, "lbcommon.c"),
                    "-I", HOSTSTUBS, "-I", SRC] + DECOMP_INC +
                   ["-o", lbcommon_o], check=True)
    subprocess.run(["cc", "-Wl,--gc-sections", "-o", exe, oracle_o,
                    lbcommon_o, "-lm"], check=True)
    return exe


def run(exe, mode, blob, out_fmt):
    out = subprocess.run([exe, mode], input=blob, stdout=subprocess.PIPE,
                         check=True).stdout
    rec = struct.Struct(out_fmt)
    return [rec.unpack_from(out, i) for i in range(0, len(out), rec.size)]


# ---- the shade ----

def n64_shade(prim, light1, light2, nl):
    """The RSP's SHADE, then the RDP's PRIM * SHADE, per colour channel."""
    nl = max(0.0, nl)
    out = []
    for shift in (24, 16, 8):
        p = (prim >> shift) & 0xFF
        dif = (light1 >> shift) & 0xFF
        amb = (light2 >> shift) & 0xFF
        shade = min(255.0, amb + dif * nl)
        out.append(p * shade / 255.0)
    return out


def fighter_materials(rom, fighter):
    """(prim, light1, light2) for every lit batch of one fighter's high
    tree, from the ROM the way the pack exporter reads it."""
    built = M.build_fighter(rom, fighter, "high")
    mats = []
    for b in built["baker"].batches:
        tex, prim, l1, l2, uses_shade, lit = b["mat"][:6]
        if uses_shade and lit:
            mats.append((prim, l1, l2))
    return mats


def check_shade(exe, rom, verbose, report, mode="shade"):
    nls = [-0.5, -0.01] + [i / 100.0 for i in range(101)]
    if mode == "shadefx":
        # the fixed-point path saturates N.L at 1.0 (the port's light has length
        # 0.787, so a unit normal never reaches it): sweep that range finer
        nls = [-0.5, -0.01] + [i / 250.0 for i in range(251)]
    bad = 0
    total = 0
    worst = 0.0
    for fighter in FIGHTERS:
        mats = fighter_materials(rom, fighter)
        distinct = sorted(set(mats))
        blob = b"".join(struct.pack("=3If", *m, nl)
                        for m in distinct for nl in nls)
        got = run(exe, mode, blob, "=I")
        k = 0
        for m in distinct:
            for nl in nls:
                argb = got[k][0]
                k += 1
                total += 1
                want = n64_shade(*m, nl)
                ours = [(argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF]
                if (argb >> 24) != (m[0] & 0xFF):
                    report("  %s prim %08X: alpha %02X, batch says %02X"
                           % (fighter, m[0], argb >> 24, m[0] & 0xFF))
                    bad += 1
                    continue
                d = max(abs(o - w) for o, w in zip(ours, want))
                worst = max(worst, d)
                if d > SHADE_TOL:
                    report("  %s prim %08X l1 %08X l2 %08X N.L %.2f: "
                           "ours (%d, %d, %d), N64 (%.1f, %.1f, %.1f)"
                           % (fighter, m[0], m[1], m[2], nl, *ours, *want))
                    bad += 1
        if verbose:
            for m in distinct:
                print("  %s prim %08X l1 %08X l2 %08X: N.L 1.0 -> "
                      "(%d, %d, %d)"
                      % (fighter, *m, *[round(c) for c in n64_shade(*m, 1.0)]))
        print("%s: %d lit batches, %d distinct materials, %d N.L samples "
              "each" % (fighter, len(mats), len(distinct), len(nls)))
    return bad, total, worst


# ---- the fog ----

# The respawn flash's white at a spread of strengths, and two tints.
FOGS = [0xFFFFFF00 | a for a in (0x20, 0x80, 0xC0, 0xFF)] + \
    [0xFF000080, 0x3050FFA0]


def check_fog(exe, rom, verbose, report, mode="fog"):
    """ft_shade_fog then the offset added into the vertex colour (what an
    untextured batch draws, and a textured one with a white texel) against
    G_RM_FOG_PRIM_A: the N64's shaded pixel lerped toward the fog colour
    by the fog's alpha. Until the respawn-flash fix an untextured batch
    lost the `+ fog * a` and only darkened."""
    nls = [-0.5, 0.0, 0.25, 0.5, 0.75, 1.0]
    bad = 0
    total = 0
    worst = 0.0
    for fighter in FIGHTERS:
        distinct = sorted(set(fighter_materials(rom, fighter)))
        cases = [(m, nl, fog) for m in distinct for nl in nls for fog in FOGS]
        blob = b"".join(struct.pack("=3IfI", *m, nl, fog)
                        for m, nl, fog in cases)
        got = run(exe, mode, blob, "=I")
        for (m, nl, fog), (argb,) in zip(cases, got):
            a = (fog & 0xFF) / 255.0
            fc = [(fog >> shift) & 0xFF for shift in (24, 16, 8)]
            want = [p * (1.0 - a) + f * a
                    for p, f in zip(n64_shade(*m, nl), fc)]
            ours = [(argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF]
            d = max(abs(o - w) for o, w in zip(ours, want))
            worst = max(worst, d)
            total += 1
            if d > FOG_TOL:
                report("  %s prim %08X N.L %.2f fog %08X: ours (%d, %d, %d), "
                       "N64 (%.1f, %.1f, %.1f)"
                       % (fighter, m[0], nl, fog, *ours, *want))
                bad += 1
    return bad, total, worst


# ---- the light ----

def f32(x):
    """Round to single precision: every step of the game's arithmetic is
    an f32 op, and the table index is the integer part of one, so a
    reference in doubles picks a different entry at an exact quarter
    turn -- 90 degrees is index 1024.0 in float and 1023.99 in double."""
    return struct.unpack("f", struct.pack("f", x))[0]


def read_dtor32():
    """macros.h DTOR32 -- (float)(PI64 / 180) -- the constant lbCommonCos
    turns its quarter turn with (F_CST_DTOR32(90.0F))."""
    src = open(os.path.join(P.DECOMP_DIR, "include", "macros.h")).read()
    m = re.search(r"#define\s+PI64\s+([0-9.]+)", src)
    if not m:
        raise SystemExit("no PI64 in the decomp's macros.h")
    return f32(float(m.group(1)) / 180.0)


PI32 = f32(3.1415927)
DTOR32 = None


def read_sin_table():
    """lb/lbcommon.c dLBCommonSinLookup: 1024 floats, a quarter wave."""
    src = open(os.path.join(P.DECOMP_DIR, "src", "lb", "lbcommon.c")).read()
    m = re.search(r"dLBCommonSinLookup\[[^\]]*\]\s*=\s*\{(.*?)\};", src,
                  re.S)
    if not m:
        raise SystemExit("no dLBCommonSinLookup in the decomp's lbcommon.c")
    table = [float(x) for x in re.findall(r"[-+]?\d+\.\d+", m.group(1))]
    if len(table) != 1024:
        raise SystemExit("dLBCommonSinLookup has %d entries, not 1024"
                         % len(table))
    return table


def table_sin(table, angle):
    """lbCommonSin (lbcommon.c:321): the 4096-step index, folded."""
    index = int(f32(f32(angle) * f32(651.8986206))) & 0xFFF
    if index & 0x400:
        s = table[0x3FF - (index & 0x3FF)]
    else:
        s = table[index & 0x3FF]
    return -s if index & 0x800 else s


def table_cos(table, angle):
    """lbCommonCos (lbcommon.c:340): the sine, a quarter turn on."""
    return table_sin(table, f32(angle + f32(90.0 * DTOR32)))


def dtor(deg):
    """F_CLC_DTOR32: (x * PI32) / 180.0F, a step at a time."""
    return f32(f32(deg * PI32) / 180.0)


def n64_light_bytes(table, angle_x, angle_y):
    """ftDisplayLightsDrawReflect: the Light's three s8 direction bytes."""
    rx = dtor(angle_x)
    ry = dtor(angle_y)
    y = -table_sin(table, -ry)
    z = table_cos(table, -ry)
    x = f32(table_sin(table, rx) * z)
    z = f32(z * table_cos(table, rx))
    return [int(f32(v * 100.0)) for v in (x, y, z)], (x, y, z)


def light_cases(rom):
    cases = []
    for stage in STAGES:
        ax, ay, _az = S.read_ground(rom, stage)["light_angle"]
        cases.append(("%s MPGroundData.light_angle" % stage, ax, ay))
    cases.append(("character select scSubsysFighterSetLightParams",
                  MENU_LIGHT[0], MENU_LIGHT[1]))
    for ax in range(-180, 181, 15):
        for ay in range(-180, 181, 15):
            cases.append(("grid", float(ax), float(ay)))
    return cases


def check_light(exe, rom, verbose, report):
    global DTOR32
    DTOR32 = read_dtor32()
    table = read_sin_table()
    # The table itself, against the real sine: a parse that dropped or
    # doubled an entry would show here before it could hide a formula slip.
    for i in range(0, 1024, 7):
        if abs(table[i] - math.sin(i * math.pi / 2048.0)) > 2e-3:
            raise SystemExit("dLBCommonSinLookup[%d] = %.6f is not sin"
                             % (i, table[i]))
    cases = light_cases(rom)
    blob = b"".join(struct.pack("=2f", ax, ay) for _, ax, ay in cases)
    got = run(exe, "light", blob, "=3f")
    bad = 0
    worst_byte = 0.0
    worst_float = 0.0
    for (name, ax, ay), ours in zip(cases, got):
        want_bytes, unit = n64_light_bytes(table, ax, ay)
        scaled = [c * 127.0 for c in ours]
        d_byte = max(abs(s - b) for s, b in zip(scaled, want_bytes))
        d_float = max(abs(s - u * 100.0) for s, u in zip(scaled, unit))
        worst_byte = max(worst_byte, d_byte)
        worst_float = max(worst_float, d_float)
        if d_byte > LIGHT_BYTE_TOL or d_float > LIGHT_FLOAT_TOL:
            report("  %s (%.1f, %.1f): ours * 127 = (%.2f, %.2f, %.2f), "
                   "N64 bytes (%d, %d, %d)"
                   % (name, ax, ay, *scaled, *want_bytes))
            bad += 1
        elif verbose or name != "grid":
            print("  %-46s (%6.1f, %6.1f) -> light (%+.3f, %+.3f, %+.3f), "
                  "|L| %.3f" % (name, ax, ay, *ours,
                                math.sqrt(sum(c * c for c in ours))))
    return bad, len(cases), worst_byte, worst_float


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    verbose = "-v" in argv
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    P.require_decomp()
    rom = open(rom_path, "rb").read()

    problems = []

    def report(line):
        problems.append(line)
        print(line)

    tmp = tempfile.TemporaryDirectory()
    exe = build(tmp.name)

    print("shade: ft_shade_lit vs the RSP's clamp and the RDP's multiply")
    bad_s, total_s, worst_s = check_shade(exe, rom, verbose, report)
    print("shade: ft_shade_lit_fx (the per-vertex path) vs the same")
    bad_sx, total_sx, worst_sx = check_shade(exe, rom, verbose, report, "shadefx")
    print("shadefx: %d/%d samples within %g of the N64 (worst %.3f)"
          % (total_sx - bad_sx, total_sx, SHADE_TOL, worst_sx))
    print("fog: ft_shade_fog and the offset vs G_RM_FOG_PRIM_A")
    bad_f, total_f, worst_fog = check_fog(exe, rom, verbose, report)
    print("fogfx: ft_shade_fx_prep after the fog, the same offset")
    bad_fx, total_fx, worst_fogfx = check_fog(exe, rom, verbose, report, "fogfx")
    print("fogfx: %d/%d samples within %g of the N64 (worst %.3f)"
          % (total_fx - bad_fx, total_fx, FOG_TOL, worst_fogfx))
    print("fog: %d/%d samples within %g of the N64 (worst %.3f)"
          % (total_f - bad_f, total_f, FOG_TOL, worst_fog))
    print("light: ft_light_dir vs ftDisplayLightsDrawReflect")
    bad_l, total_l, worst_b, worst_f = check_light(exe, rom, verbose, report)

    print("shade: %d/%d samples within %g of the N64 (worst %.3f); "
          "light: %d/%d directions within a byte (worst %.3f) and %.1e "
          "of the formula"
          % (total_s - bad_s, total_s, SHADE_TOL, worst_s,
             total_l - bad_l, total_l, worst_b, worst_f))
    if problems:
        sys.exit("shade_check: %d problems" % len(problems))
    print("shade_check: OK")


if __name__ == "__main__":
    main()
