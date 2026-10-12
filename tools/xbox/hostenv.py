"""Where the Windows host tools live, and how to run the upstream Makefile.

The Dreamcast build runs src/game/ssb64/Makefile inside a Linux container.
The Xbox build runs the SAME Makefile -- its asset rules and its object list
are the single source of truth, never a copy -- under MSYS2's GNU make, with
three adjustments that change nothing about what a rule does:

  * KOS_BASE points at tools/xbox/kos_stub, whose empty Makefile.rules is
    all the Makefile needs of KallistiOS when no SH-4 object is built.
  * `python3` on the recipe PATH is tools/xbox/hostbin/python3, a two-line
    shim to this interpreter (on Windows `python3` is the Store stub).
  * The repository path is handed to make without spaces. GNU make splits
    `include` and prerequisite lists on spaces, and a Windows checkout's
    path often has one. The 8.3 short name is used when the volume has one;
    otherwise a directory junction in %TEMP% gives the same tree a
    space-free name.
"""
import ctypes
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GAME_DIR = os.path.join(ROOT, "src", "game", "ssb64")
DECOMP_DIR = os.path.join(ROOT, "extern", "ssb-decomp-re")

# docker/pins.env: the decomp revision the port is written against.
DECOMP_URL = "https://github.com/VetriTheRetri/ssb-decomp-re.git"


def decomp_rev():
    with open(os.path.join(ROOT, "docker", "pins.env"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("DECOMP_REV="):
                return line.split("=", 1)[1].strip()
    raise SystemExit("docker/pins.env has no DECOMP_REV")


def _short_path(path):
    if os.name != "nt":
        return path
    buf = ctypes.create_unicode_buffer(32768)
    n = ctypes.windll.kernel32.GetShortPathNameW(path, buf, len(buf))
    if n == 0 or n >= len(buf):
        return path
    return buf.value


def spaceless_root():
    """The repository root under a name with no spaces (forward slashes)."""
    if " " not in ROOT:
        return ROOT.replace("\\", "/")
    short = _short_path(ROOT)
    if " " not in short:
        return short.replace("\\", "/")
    # No 8.3 names on this volume: a junction does not need elevation.
    link = os.path.join(os.environ.get("TEMP", "C:\\Temp"), "ssb64x_root")
    if not os.path.isdir(link) or os.path.realpath(link) != os.path.realpath(ROOT):
        if os.path.lexists(link):
            subprocess.run(["cmd", "/c", "rmdir", link], check=False)
        subprocess.run(["cmd", "/c", "mklink", "/J", link, ROOT], check=True,
                       stdout=subprocess.DEVNULL)
    if " " in link:
        raise SystemExit("cannot find a space-free path for %s" % ROOT)
    return link.replace("\\", "/")


def msys_roots():
    """Where MSYS2 may be: $MSYS2_ROOT, then the installer's and winget's
    default, then Chocolatey's."""
    cands = []
    for env in ("MSYS2_ROOT", "MSYS2_PATH"):
        if os.environ.get(env):
            cands.append(os.environ[env])
    cands += [r"C:\msys64", r"C:\tools\msys64"]
    return cands


MSYS_TOOLS = ("make.exe", "cmp.exe")   # GNU make; diffutils' cmp (gen_reloc_header.sh)


def find_msys(need_make=True):
    """MSYS2's bash.exe, or None. With need_make, only an MSYS2 that has the
    packages the build uses -- make, and diffutils, whose cmp keeps the decomp's
    reloc header from being rewritten (and everything recompiled) every run."""
    for root in msys_roots():
        bindir = os.path.join(root, "usr", "bin")
        bash = os.path.join(bindir, "bash.exe")
        if os.path.isfile(bash) and (not need_make or all(
                os.path.isfile(os.path.join(bindir, t)) for t in MSYS_TOOLS)):
            return bash
    return None


def find_msys_bash():
    """MSYS2's bash: the Makefile's recipes need a POSIX shell, GNU sed
    and awk, and make 4.x (grouped targets, `&:`)."""
    bash = find_msys(need_make=True)
    if bash:
        return bash
    raise SystemExit(
        "MSYS2 with GNU make is required -- run build.bat (or python setup.py),\n"
        "  which offers to install it. Set MSYS2_ROOT if it is not in C:\\msys64.")


def _to_msys(path):
    p = path.replace("\\", "/")
    if len(p) > 1 and p[1] == ":":
        p = "/" + p[0].lower() + p[2:]
    return p


def run_make(targets, evals=(), extra_vars=(), jobs=None, dry_run=False,
             always=False, keep_going=False, capture=False):
    """Run the upstream Makefile's rules for `targets`."""
    root = spaceless_root()
    bash = find_msys_bash()
    pydir = os.path.dirname(sys.executable)
    hostbin = _to_msys(root + "/tools/xbox/hostbin")
    args = ["make"]
    if jobs:
        args.append("-j%d" % jobs)
    if dry_run:
        args.append("-n")
    if always:
        args.append("-B")
    if keep_going:
        args.append("-k")
    args += ["KOS_BASE=%s/tools/xbox/kos_stub" % root,
             "SSB_DECOMP_DIR=%s/extern/ssb-decomp-re" % root]
    args += list(extra_vars)
    for e in evals:
        args += ["--eval", e]
    args += list(targets)
    quoted = " ".join("'" + a.replace("'", "'\\''") + "'" for a in args)
    script = ('export PATH="%s:%s:$PATH"; export SSB_DECOMP_DIR=%s/extern/ssb-decomp-re '
              'PYTHONUTF8=1 PYTHONIOENCODING=utf-8; cd %s/src/game/ssb64 && %s'
              % (hostbin, _to_msys(pydir), root, _to_msys(root), quoted))
    env = dict(os.environ, MSYSTEM="MSYS", CHERE_INVOKING="1")
    # MSYS2's login shell builds its own PATH from the Windows system folders
    # (MSYS2_PATH_TYPE=minimal drops the rest anyway), but with no PATH at all
    # it leaves recipes with none -- "which: no bash in ((null))". A Visual
    # Studio build of an RXDK project runs its Pre-Build Event with PATH
    # removed (the toolset's ExecutablePath is empty). So the shell always
    # gets the system folders, and nothing a Git Bash or Cygwin session
    # might have put first.
    sysroot = os.environ.get("SystemRoot") or os.environ.get("windir") or r"C:\Windows"
    env["PATH"] = os.pathsep.join([os.path.join(sysroot, "System32"), sysroot,
                                   os.path.join(sysroot, "System32", "Wbem")])
    for v in ("ORIGINAL_PATH", "MSYS2_PATH_TYPE", "SHELL"):
        env.pop(v, None)
    r = subprocess.run([bash, "-lc", script], env=env,
                       stdout=subprocess.PIPE if capture else None,
                       stderr=subprocess.STDOUT if capture else None,
                       text=capture, encoding="utf-8" if capture else None,
                       errors="replace" if capture else None)
    return r
