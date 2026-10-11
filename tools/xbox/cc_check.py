#!/usr/bin/env python3
"""Compile every source in rxdk.project.json in parallel and report ALL errors.

Rxdk.Cli builds one file at a time and stops at the first failure, which is
the right behaviour for a build and the wrong one for bringing up a port of
350 files. This runs the same clang with the same command line the CLI uses
(the RXDK defaults below are copied from its log; the project's includes,
defines and flags come from the manifest) across every core, and prints a
per-file summary. It never writes objects unless --obj is given.

  python tools/xbox/cc_check.py                 # -fsyntax-only, all files
  python tools/xbox/cc_check.py src/dc/fgm.c    # just these
  python tools/xbox/cc_check.py --errors 3      # first 3 errors per file
"""
import argparse
import concurrent.futures as cf
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RXDK = os.environ.get("RXDK_ROOT", r"C:\ProgramData\RXDK")
CLANG = os.path.join(RXDK, "llvm", "xboxog-windows-x64", "bin", "clang.exe")
CLANG_INC = os.path.join(RXDK, "llvm", "xboxog-windows-x64", "lib", "clang", "23", "include")
SDK_INC = os.path.join(RXDK, "sdk", "include")

# Rxdk.Cli v1.2.8's fixed C/C++ flags (from its build log).
RXDK_C = ["-std=c23", "-target", "i686-pc-windows-gnu", "-isystem", CLANG_INC, "-O3",
          "-fno-sanitize=undefined", "-ffreestanding", "-fno-stack-protector",
          "-fms-extensions", "-fms-compatibility", "-nostdinc", "-include", "picolibc.h",
          "-march=pentium3", "-D_XBOX", "-DXBOX", "-fno-builtin", "-U_DEBUG",
          "-D__ASSERT_VERBOSE", "-femulated-tls", "-I", SDK_INC,
          "-Wno-c++11-narrowing", "-Wno-address-of-temporary", "-Wno-ignored-pragma-intrinsic",
          "-Wno-multichar", "-Wno-unused-command-line-argument",
          "-Wno-deprecated-enum-enum-conversion"]


def cmdline(src, cfg, obj=None):
    m = cfg
    c = [CLANG] + RXDK_C
    for inc in m.get("includePaths", []):
        c.append("-I" + os.path.join(ROOT, inc))
    for d in m.get("defines", []):
        c.append("-D" + d)
    c += m.get("compileFlags", [])
    if src.endswith(".cpp"):
        c[c.index("-std=c23")] = "-std=" + m.get("cppStandard", "c++20")
        c += ["-x", "c++", "-fno-exceptions"]
        c.insert(1, "-isystem")
        c.insert(2, os.path.join(SDK_INC, "c++", "v1"))
    else:
        c += ["-x", "c"]
    c += ["-c", os.path.join(ROOT, src)]
    c += (["-o", obj] if obj else ["-fsyntax-only"])
    return c


def run(src, cfg, nerr, obj):
    r = subprocess.run(cmdline(src, cfg, obj), capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    out = r.stdout + r.stderr
    errs = [l for l in out.splitlines() if re.search(r"\berror:", l)]
    warns = [l for l in out.splitlines() if re.search(r"\bwarning:", l)]
    return src, r.returncode, errs[:nerr] if nerr else errs, len(errs), warns, out


def obj_name(src):
    # the CLI's own naming: the path with separators turned to '_'
    return os.path.splitext(src.replace("/", "_").replace("\\", "_"))[0] + ".obj"


def link(cfg, objdir, objs):
    """Rxdk.Cli's link line (from its log), into objdir/check.exe."""
    rsp = os.path.join(objdir, "link_objs.rsp")
    with open(rsp, "w", encoding="utf-8") as f:
        for o in objs:
            f.write('"%s"\n' % o.replace("\\", "/"))
    libdir = os.path.join(RXDK, "sdk", "lib")
    libs = [os.path.join(libdir, l) for l in cfg.get("libraries", [])]
    builtins = os.path.join(RXDK, "llvm", "xboxog-windows-x64", "lib", "clang", "23", "lib",
                            "windows", "libclang_rt.builtins-i386.a")
    # Rxdk.Cli brackets .eh_frame with two marker objects whenever libcpp is
    # linked (libunwind reads __eh_frame_start/_end); do the same
    brackets = []
    if any("libcpp" in l for l in cfg.get("libraries", [])):
        for name, sym in (("rxdk_eh_begin", "___eh_frame_start"), ("rxdk_eh_end", "___eh_frame_end")):
            s = os.path.join(objdir, name + ".S")
            with open(s, "w", encoding="utf-8") as f:
                f.write('.section .eh_frame,"dr"\n.globl %s\n%s:\n' % (sym, sym))
            subprocess.run([CLANG, "-target", "i686-pc-windows-gnu", "-march=pentium3", "-c", s,
                            "-o", os.path.join(objdir, name + ".o")], check=True)
            brackets.append(os.path.join(objdir, name + ".o"))
    first = brackets[:1]
    last = brackets[1:]
    cmd = [CLANG] + first + ["@" + rsp] + last + libs + [builtins, "-target", "i686-pc-windows-gnu",
           "-march=pentium3", "-nostdlib", "-nostartfiles", "-Wl,--image-base=0x10000",
           "-O0", "-fuse-ld=lld", "-Wl,--error-limit=0", "-e", "XapiTitleStartup",
           "-o", os.path.join(objdir, "check.exe")]
    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    out = r.stdout + r.stderr
    undef = {}
    cur = None
    for line in out.splitlines():
        m = re.search(r"undefined symbol: (.+)$", line)
        if m:
            cur = m.group(1).strip()
            undef.setdefault(cur, set())
            continue
        m = re.search(r">>> referenced by (\S+)", line)
        if m and cur:
            undef[cur].add(os.path.basename(m.group(1)))
    other = [l for l in out.splitlines() if "error" in l and "undefined symbol" not in l]
    return r.returncode, undef, other


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="*")
    ap.add_argument("--link", action="store_true",
                    help="compile objects to out/check and link them, listing undefined symbols")
    ap.add_argument("--config", default="Release")
    ap.add_argument("--errors", type=int, default=4)
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    ap.add_argument("--warn", action="store_true", help="also list implicit-declaration warnings")
    ap.add_argument("--full", action="store_true", help="print the whole compiler output of failures")
    args = ap.parse_args()
    man = json.load(open(os.path.join(ROOT, "rxdk.project.json")))
    cfg = man["configurations"][args.config]
    files = args.files or cfg["sources"]
    bad = 0
    implicit = {}
    objdir = os.path.join(ROOT, "out", "check", args.config)
    if args.link:
        os.makedirs(objdir, exist_ok=True)
    with cf.ThreadPoolExecutor(args.jobs) as ex:
        futs = [ex.submit(run, f, cfg, args.errors,
                          os.path.join(objdir, obj_name(f)) if args.link else None)
                for f in files]
        for fut in cf.as_completed(futs):
            src, rc, errs, n, warns, out = fut.result()
            for w in warns:
                m = re.search(r"implicit declaration of function '([^']+)'", w)
                if m:
                    implicit.setdefault(m.group(1), set()).add(os.path.basename(src))
            if rc != 0:
                bad += 1
                print("FAIL %s (%d errors)" % (src, n))
                if args.full:
                    print(out)
                else:
                    for e in errs:
                        print("    " + e.replace(ROOT + os.sep, ""))
    if implicit and args.warn:
        print("\nimplicit declarations:")
        for k in sorted(implicit):
            print("  %-36s %s" % (k, " ".join(sorted(implicit[k]))[:100]))
    print("\n%d of %d files failed" % (bad, len(files)))
    if args.link and not bad:
        objs = [os.path.join(objdir, obj_name(f)) for f in cfg["sources"]]
        rc, undef, other = link(cfg, objdir, objs)
        for k in sorted(undef):
            print("UNDEF %-40s %s" % (k, " ".join(sorted(undef[k]))[:110]))
        for l in other[:40]:
            print(l)
        print("link: %s, %d undefined symbols" % ("ok" if rc == 0 else "FAILED", len(undef)))
        return rc
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
