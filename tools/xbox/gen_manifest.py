#!/usr/bin/env python3
"""Generate rxdk.project.json from the upstream Makefile's object list.

The Dreamcast build's src/game/ssb64/Makefile is the authority on WHAT the
game is made of: ~350 hand-written rules, each naming a source, the flags it
needs, and -- for decomp files that sit in an N64 overlay -- the objcopy that
renames its .bss to ovlN_noload. This script asks make itself (a dry run of
every object in $(OBJS)) rather than parsing the Makefile's text, so the Xbox
source list cannot drift from the Dreamcast one.

Two things a manifest cannot say per file are turned into WRAPPER sources in
Platform/xbox/gen/tu/, one per affected object:

  * `-include hdr` flags (39 decomp files) -> `#include <hdr>` lines, in order;
  * the overlay .bss rename (152 decomp files) -> `#pragma clang section
    bss=".bss$zzssbovlN_m"`. lld groups `.bss$...` input sections by suffix,
    so the markers in Platform/xbox/overlay_bounds_xbox.c (`_a` / `_z`)
    bracket exactly the overlay's statics -- the job GNU ld's __start_/__stop_ symbols
    do for src/dc/overlay.c on the Dreamcast.

Objects that only exist for Dreamcast hardware are listed in DROP with the
file in Platform/xbox/ that takes their place.

Usage: python tools/xbox/gen_manifest.py [--from-log objs_dry.log]
"""
import argparse
import json
import os
import re
import shlex
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hostenv  # noqa: E402

ROOT = hostenv.ROOT
TU_DIR = os.path.join(ROOT, "Platform", "xbox", "gen", "tu")

# Objects the Dreamcast link has that the Xbox link replaces.
DROP = {
    # the romdisk image linked into the ELF: the Xbox reads D:\ instead
    "romdisk.o": "assetroot (D:\\ via Platform/xbox/kos_fs_xbox.c)",
    # the AICA ARM7 firmware blob: its C runs on the x86 here
    "aica_fwblob.o": "Platform/xbox/aica/aica_fw_xbox.c",
}

# src/game/ssb64/main.c defines the KOS `int main(int, char **)`. The Xbox
# entry is `void __cdecl main(void)` (Platform/xbox/xbox_main.c), which sets
# up the console and calls the game's under this name.
RENAME_MAIN = {"main.o": "ssb_main"}

PROJECT_NAME = "ssb64x"

COMMON_DEFINES = [
    "_arch_dreamcast",          # the KOS arch macro: Platform/xbox shims KOS
    "_arch_sub_pristine",
    "TARGET_XBOX=1",
    "_LANGUAGE_C",
    "F3DEX_GBI_2",
    "REGION_US",
    "AVOID_UB",
    "MTX_BACKEND=MTX_BACKEND_SCALAR",
]

RELEASE_DEFINES = ["SSB_RELEASE=1", "DB_ENABLE=0", "NDEBUG"]
DEBUG_DEFINES = ["DB_ENABLE=0"]

COMPILE_FLAGS = [
    # RXDK compiles C as -std=c23 (bool/true/false/nullptr keywords); the
    # Dreamcast build is GCC 13's default dialect.
    "-std=gnu17",
    # -fms-compatibility switches clang to MSVC bitfield layout, which
    # changes the size of any struct mixing bitfield types. The decomp's
    # event structs mirror N64 data and must have the GCC layout the
    # exporters wrote them with.
    "-mno-ms-bitfields",
    # SH-4 single precision, not x87 extended: every float op rounds to
    # float, as on the console the game logic was tuned on. (P3 has SSE,
    # not SSE2, so doubles stay on x87.)
    "-msse", "-mfpmath=sse",
    "-ffp-contract=off",
    "-fno-strict-aliasing",
    "-fwrapv",
    # The Dreamcast build's DECOMP_CFLAGS, applied everywhere here; clang
    # makes these errors by default where GCC 13 warns.
    "-Wno-int-conversion",
    "-Wno-incompatible-pointer-types",
    "-Wno-error=implicit-function-declaration",
    "-Wno-unused-variable",
    "-Wno-parentheses",
    "-Wno-missing-braces",
    "-Wno-unknown-pragmas",
    "-Wno-unknown-warning-option",
    "-Wno-pragma-pack",
    "-Wno-microsoft-anon-tag",
    # i686-pc-windows-gnu predefines these; nothing in this game is Win32.
    "-U_WIN32", "-U_WIN64", "-U__MINGW32__",
]

INCLUDE_PATHS = [
    "Platform/xbox",            # the KOS shim headers shadow <kos.h> etc.
    "src/dc",
    "src/dc/decomp",
    "extern/ssb-decomp-re/src",
    "src/game/ssb64",           # generated aica_reverb.h / aica_sends.h
    "extern/decomp_include",    # the Makefile's -idirafter (see below)
]

# The Makefile adds the decomp's include/ with -idirafter, so its libc
# look-alikes lose to the system's. A manifest include path is an ordinary
# -I, searched BEFORE clang's own headers, and clang runs in whatever
# directory the build was started from, so a relative -idirafter in
# compileFlags cannot be used either. The same search order is had by
# mirroring include/ without the headers the system must win for.
DECOMP_INCLUDE_SKIP = {"stdarg.h", "stddef.h", "stdlib.h", "string.h", "assert.h", "regdef.h"}


def mirror_decomp_include():
    import shutil
    src = os.path.join(hostenv.DECOMP_DIR, "include")
    dst = os.path.join(ROOT, "extern", "decomp_include")
    if not os.path.isdir(src):
        raise SystemExit("gen_manifest: no decomp at %s (python setup.py fetches it)" % src)
    # always rebuilt: it is derived from the decomp, which can change
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    n = 0
    for dp, dn, fn in os.walk(src):
        rel = os.path.relpath(dp, src)
        for f in fn:
            if rel == "." and f in DECOMP_INCLUDE_SKIP:
                continue
            os.makedirs(os.path.join(dst, rel), exist_ok=True)
            shutil.copy2(os.path.join(dp, f), os.path.join(dst, rel, f))
            n += 1
    return n

LIBS_RELEASE = ["libd3d8.lib", "libd3dx8.lib", "libxgraphics.lib", "libdsound.lib",
                "libxapi.lib", "libc.lib", "libcpp.lib", "libkernel.lib"]
LIBS_DEBUG = ["libxbdmd.lib", "libd3d8d.lib", "libd3dx8d.lib", "libxgraphicsd.lib",
              "libdsoundd.lib", "libxapid.lib", "libcd.lib", "libcppd.lib", "libkerneld.lib"]


def dry_run_log():
    r = hostenv.run_make(["xo"], evals=[".SECONDEXPANSION:", "xo: $$(OBJS)"],
                         extra_vars=["RELEASE=1", "DISC=1"],
                         dry_run=True, always=True, keep_going=True, capture=True)
    # -k: the ARM7 firmware's rules need KallistiOS sources and fail here;
    # keep going so every SH-4 object after them is still listed.
    return r.stdout


def expected_objects():
    r = hostenv.run_make(["po"], evals=["po: ; @echo $(OBJS)"],
                         extra_vars=["RELEASE=1", "DISC=1"], capture=True)
    return r.stdout.split()


def parse(log):
    root = hostenv.spaceless_root()
    decomp = root + "/extern/ssb-decomp-re"
    txt = log.replace("\\\n", " ")
    cmds = []
    for line in txt.splitlines():
        cmds += [p.strip() for p in line.split("&&")]
    objs, ovl, order = {}, {}, []
    for c in cmds:
        if c.startswith("kos-cc ") or c.startswith("cc "):
            t = shlex.split(c)
            if "-c" not in t or "-o" not in t:
                continue
            out = t[t.index("-o") + 1].replace(".tmp", "")
            # explicit rules: `-c src -o obj`; make's built-in %.o: %.c
            # (main.o): `-c -o obj src`
            src = t[t.index("-c") + 1]
            if src == "-o":
                src = [x for x in t if x.endswith(".c")][-1]
            if src.startswith(decomp):
                rel = "extern/ssb-decomp-re" + src[len(decomp):]
            else:
                rel = os.path.normpath(os.path.join("src/game/ssb64", src)).replace("\\", "/")
            incs = [t[i + 1] for i, x in enumerate(t) if x == "-include"]
            if out not in objs:
                order.append(out)
            objs[out] = {"src": rel, "includes": incs}
        m = re.match(r"sh-elf-objcopy --rename-section \.bss=(ovl\d)_noload (\S+)\.tmp", c)
        if m:
            ovl[m.group(2)] = m.group(1)
    return order, objs, ovl


def write_wrapper(obj, info, overlay):
    name = os.path.splitext(obj)[0]
    path = os.path.join(TU_DIR, name + ".c")
    src_abs = os.path.join(ROOT, info["src"])
    rel = os.path.relpath(src_abs, TU_DIR).replace("\\", "/")
    lines = ["/* GENERATED by tools/xbox/gen_manifest.py -- do not edit.",
             " *",
             " * %s, compiled the way src/game/ssb64/Makefile compiles %s." % (info["src"], obj)]
    if overlay:
        lines.append(" * Its .bss is %s's noload segment (src/dc/overlay.c)." % overlay)
    lines.append(" */")
    if overlay:
        lines.append('#pragma clang section bss=".bss$zzssb%s_m"' % overlay)
    for inc in info["includes"]:
        lines.append("#include <%s>" % inc)
    if obj in RENAME_MAIN:
        lines.append("#define main %s" % RENAME_MAIN[obj])
    lines.append('#include "%s"' % rel)
    text = "\n".join(lines) + "\n"
    os.makedirs(TU_DIR, exist_ok=True)
    old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
    if old != text:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    return os.path.relpath(path, ROOT).replace("\\", "/")


def platform_sources():
    out = []
    base = os.path.join(ROOT, "Platform", "xbox")
    for dp, dn, fn in os.walk(base):
        dn[:] = [d for d in dn if d != "gen"]
        for f in sorted(fn):
            if f.endswith((".c", ".cpp")):
                out.append(os.path.relpath(os.path.join(dp, f), ROOT).replace("\\", "/"))
    return sorted(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from-log", help="parse a saved dry-run log instead of running make")
    args = ap.parse_args()
    mirror_decomp_include()
    log = open(args.from_log, encoding="utf-8").read() if args.from_log else dry_run_log()
    order, objs, ovl = parse(log)
    want = expected_objects() if not args.from_log else order
    missing = [o for o in want if o not in objs and o not in DROP]
    if missing:
        sys.stderr.write(log[-4000:])
        raise SystemExit("gen_manifest: no compile rule seen for %s" % " ".join(missing))
    order = [o for o in want if o in objs or o in DROP]

    sources, wrapped = [], set()
    for obj in order:
        if obj in DROP:
            continue
        info = objs[obj]
        overlay = ovl.get(os.path.splitext(obj)[0] + ".o")
        if info["includes"] or overlay or obj in RENAME_MAIN:
            sources.append(write_wrapper(obj, info, overlay))
            wrapped.add(obj)
        else:
            sources.append(info["src"])
    # stale wrappers from an older object list
    keep = {os.path.basename(s) for s in sources}
    if os.path.isdir(TU_DIR):
        for f in os.listdir(TU_DIR):
            if f.endswith(".c") and f not in keep:
                os.remove(os.path.join(TU_DIR, f))
    # the absolute symbols of the Makefile's two ld scripts (gen_absyms.py)
    import gen_absyms
    gen_absyms.main()
    sources = platform_sources() + ["Platform/xbox/gen/abs_symbols.c"] + sources

    def cfg(debug):
        return {
            "configuration": "debug" if debug else "release",
            # the dashboard icons: BMPs from tools/xbox/make_icons.py, bundled
            # to DXT1 .xbx before the link
            "resources": ["Platform/xbox/titleimage.rdf", "Platform/xbox/saveimage.rdf"],
            "sources": sources,
            "libraries": LIBS_DEBUG if debug else LIBS_RELEASE,
            "includePaths": INCLUDE_PATHS,
            "defines": COMMON_DEFINES + (DEBUG_DEFINES if debug else RELEASE_DEFINES),
            "compileFlags": COMPILE_FLAGS,
            "cppStandard": "c++20",
            "exceptions": False,
            "deployPaths": ["data"],
            "forceCopy": True,
            "createIso": True,
            "outputDir": "out/Debug" if debug else "out/Release",
            "imageBuild": {
                "stackSize": 262144,
                "debug": debug,
                "noLogo": True,
                "noLibWarn": True,
                "testId": "0xffff0403",
                "testName": "Super Smash Bros.",
                "testVersion": "1",
                "titleImage": "Platform/xbox/gen/titleimage.xbx",
                "defaultSaveImage": "Platform/xbox/gen/saveimage.xbx",
            },
        }

    manifest = {
        "name": PROJECT_NAME,
        "type": "executable",
        "defaultConfiguration": "Release",
        "configurations": {"Debug": cfg(True), "Release": cfg(False)},
    }
    out = os.path.join(ROOT, "rxdk.project.json")
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    print("gen_manifest: %d sources (%d game objects, %d wrapped, %d overlay, %d dropped)"
          % (len(sources), len(order), len(wrapped), len(ovl),
             len([o for o in order if o in DROP])))


if __name__ == "__main__":
    main()
