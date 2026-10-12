"""Everything the build needs that is not in this repository.

Each requirement says what it is, why the build needs it, whether this
machine has it, and -- where it can -- installs it: with winget for the
programs (git, MSYS2, the .NET 8 runtime), MSYS2's pacman for GNU make, pip
for the Python packages, and RXDK's own installer for the toolchain. Nothing
is installed without asking first, unless --yes was given.

What the build does NOT need, because people have asked: MinGW or any other
C compiler (RXDK brings its own clang, and the Python packages come as
prebuilt wheels), Visual Studio (optional, for the IDE), Docker, KallistiOS
or a Dreamcast toolchain.

    python tools/xbox/prereqs.py          # the report
    python tools/xbox/prereqs.py --fix    # the report, then offer to install
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import hostenv  # noqa: E402

PY_MIN, PY_MAX = (3, 11), (3, 14)       # numpy 2.3.5 and pygfxd 1.0.5 ship wheels for these
RXDK_MIN = (1, 2, 8)                    # the oldest RXDK this port is built and tested with
RXDK_TOOLS_ZIP = ("https://github.com/Team-Resurgent/RXDK-Tools/releases/download/"
                  "latest/rxdk-managed-win-x64.zip")
# measured: the decomp 0.3 GB, exports + data 0.15 GB, build 0.15 GB; RXDK
# 0.9 GB more (and its 0.9 GB of downloads while it installs) when it is new
FREE_GB = 3


def say(msg):
    print("prereqs: " + msg, flush=True)


# ------------------------------------------------------------------ finders

def program_files():
    return [os.environ.get(v) for v in ("ProgramW6432", "ProgramFiles", "ProgramFiles(x86)")
            if os.environ.get(v)]


def find_git():
    g = shutil.which("git")
    if g:
        return g
    for pf in program_files():
        c = os.path.join(pf, "Git", "cmd", "git.exe")
        if os.path.isfile(c):
            return c
    return None


def find_dotnet8():
    """The .NET 8 runtime Rxdk.Cli runs on (a framework-dependent .NET 8
    program: 9 does not stand in for 8)."""
    roots = [os.environ.get("DOTNET_ROOT")] + [os.path.join(pf, "dotnet") for pf in program_files()]
    for r in roots:
        if not r:
            continue
        shared = os.path.join(r, "shared", "Microsoft.NETCore.App")
        if os.path.isdir(shared) and any(v.startswith("8.") for v in os.listdir(shared)):
            return r
    return None


def rxdk_root():
    return os.path.join(os.environ.get("ProgramData", r"C:\ProgramData"), "RXDK")


def find_rxdk_cli():
    cands = [os.environ.get("RXDK_CLI", ""),
             os.path.join(rxdk_root(), "tools", "Rxdk.Cli.exe"),
             os.path.join(rxdk_root(), "engine", "Rxdk.Cli.exe")]
    for c in cands:
        if c and os.path.isfile(c):
            return c
    return shutil.which("Rxdk.Cli.exe") or shutil.which("Rxdk.Cli")


def rxdk_versions(cli):
    """{component: installed version or None} from `Rxdk.Cli versions`."""
    try:
        r = subprocess.run([cli, "versions"], capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.TimeoutExpired):
        return None
    out = {}
    for line in r.stdout.splitlines():
        f = line.split("\t")
        if len(f) >= 2:
            out[f[0].strip()] = None if f[1].strip() in ("", "-") else f[1].strip()
    return out or None


def parse_ver(v):
    try:
        return tuple(int(x) for x in v.lstrip("v").split(".")[:3])
    except (AttributeError, ValueError):
        return None


def winget():
    return shutil.which("winget")


# ------------------------------------------------------------- requirements

class Req:
    """One requirement: `check` returns (ok, detail); `install`, when the
    script can do it, returns nothing and raises on failure."""

    def __init__(self, name, why, check, install=None, manual="", needs=(), optional=False):
        self.name, self.why, self.check, self.install = name, why, check, install
        self.manual, self.needs, self.optional = manual, needs, optional


def check_python():
    v = sys.version_info[:2]
    bits = struct.calcsize("P") * 8
    ok = PY_MIN <= v <= PY_MAX and bits == 64
    return ok, "%d.%d, %d-bit (%s)" % (v[0], v[1], bits, sys.executable)


def check_pydeps():
    missing = []
    for mod in ("numpy", "pygfxd"):
        try:
            __import__(mod)
        except ImportError:
            missing.append(mod)
    return not missing, ("numpy, pygfxd" if not missing else "missing " + ", ".join(missing))


def install_pydeps():
    req = os.path.join(ROOT, "docker", "requirements.txt")
    run([sys.executable, "-m", "pip", "install", "--only-binary=:all:", "-r", req])


def check_git():
    g = find_git()
    return bool(g), g or "not found"


def check_msys():
    b = hostenv.find_msys(need_make=False)
    return bool(b), os.path.dirname(os.path.dirname(os.path.dirname(b))) if b else "not found"


def check_make():
    b = hostenv.find_msys(need_make=True)
    return bool(b), "make, cmp in " + os.path.dirname(b) if b else "not installed in MSYS2"


def install_make():
    bash = hostenv.find_msys(need_make=False)
    if not bash:
        raise RuntimeError("MSYS2 is not installed")
    # -Sy: a fresh MSYS2's package database is empty until it is synced
    run([bash, "-lc", "pacman -Sy --needed --noconfirm make diffutils"])


def check_dotnet():
    r = find_dotnet8()
    return bool(r), ".NET 8 in " + r if r else "not found"


def check_rxdk_cli():
    c = find_rxdk_cli()
    return bool(c), c or "not found"


def install_rxdk_cli():
    """RXDK's host tools, Rxdk.Cli among them: the RXDK-Tools release
    archive, its tools/ folder into %ProgramData%\\RXDK\\tools -- what RXDK's
    own installers put there."""
    dest = os.path.join(rxdk_root(), "tools")
    os.makedirs(dest, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        z = os.path.join(tmp, "rxdk-tools.zip")
        say("downloading %s" % RXDK_TOOLS_ZIP)
        urllib.request.urlretrieve(RXDK_TOOLS_ZIP, z)
        with zipfile.ZipFile(z) as zf:
            n = 0
            for name in zf.namelist():
                parts = name.split("/")
                if len(parts) >= 2 and parts[-2] == "tools" and parts[-1]:
                    with open(os.path.join(dest, parts[-1]), "wb") as f:
                        f.write(zf.read(name))
                    n += 1
    if not n:
        raise RuntimeError("the RXDK-Tools archive had no tools/ folder")
    say("RXDK host tools: %d files into %s" % (n, dest))


def rxdk_component(comp):
    def check():
        cli = find_rxdk_cli()
        if not cli or not find_dotnet8():
            return False, "needs Rxdk.Cli and .NET 8 first"
        v = rxdk_versions(cli)
        if v is None:
            return False, "Rxdk.Cli did not answer"
        have = v.get(comp)
        if not have:
            return False, "not installed"
        if comp in ("SDK", "Tools") and parse_ver(have) and parse_ver(have) < RXDK_MIN:
            return False, "%s is older than v%d.%d.%d" % ((have,) + RXDK_MIN)
        return True, have
    return check


def rxdk_install(command, update_command=None):
    def install():
        cli = find_rxdk_cli()
        cmd = command
        if update_command:
            ok, detail = rxdk_component({"install-sdk": "SDK", "install-tools": "Tools"}[command])()
            if "older" in detail:
                cmd = update_command
        run([cli, cmd])
    return install


def check_vs():
    """Visual Studio, only for building in the IDE: RXDK's MSBuild targets
    import Microsoft.Cpp.ClangCl.Exclusion.props, which the component
    "MSBuild support for LLVM (clang-cl) toolset" installs. Without it VS
    cannot load SSB-X.vcxproj (or any RXDK sample): error MSB4019."""
    vswhere = os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
                           "Microsoft Visual Studio", "Installer", "vswhere.exe")
    if not os.path.isfile(vswhere):
        return True, "not installed (not needed for build.bat)"
    r = subprocess.run([vswhere, "-all", "-products", "*", "-property", "installationPath"],
                       capture_output=True, text=True)
    found, missing = [], []
    for inst in r.stdout.splitlines():
        for vc in ("v170", "v180"):
            d = os.path.join(inst.strip(), "MSBuild", "Microsoft", "VC", vc)
            if os.path.isdir(os.path.join(d, "Platforms", "Xbox")):
                (found if os.path.isfile(os.path.join(d, "Microsoft.Cpp.ClangCl.Exclusion.props"))
                 else missing).append(inst.strip())
    if missing:
        return False, ("%s: add \"MSBuild support for LLVM (clang-cl) toolset\" in the "
                       "Visual Studio Installer" % ", ".join(missing))
    if found:
        return True, "RXDK ready in " + ", ".join(found)
    return True, "no RXDK extension (not needed for build.bat)"


def check_disk():
    free = shutil.disk_usage(ROOT).free / 1024 ** 3
    return free >= FREE_GB, "%.1f GB free on %s" % (free, os.path.splitdrive(ROOT)[0] or ROOT)


def winget_install(pkg_id):
    def install():
        w = winget()
        if not w:
            raise RuntimeError("winget is not available")
        run([w, "install", "--id", pkg_id, "--exact", "--silent",
             "--accept-package-agreements", "--accept-source-agreements"])
    return install


REQS = [
    Req("Python 3.11-3.14, 64-bit", "runs setup.py and every asset exporter",
        check_python, None,
        "install 64-bit Python 3.13 from https://www.python.org/downloads/ "
        "(build.bat offers to do it with winget)"),
    Req("Python packages (numpy, pygfxd)", "the asset exporters' maths and display-list replay",
        check_pydeps, install_pydeps,
        "python -m pip install -r docker/requirements.txt"),
    Req("git", "fetches the pinned decompilation and applies docker/patches",
        check_git, winget_install("Git.Git"),
        "https://git-scm.com/download/win"),
    Req("MSYS2", "the POSIX shell the Dreamcast port's Makefile runs in",
        check_msys, winget_install("MSYS2.MSYS2"),
        "https://www.msys2.org/ (install to C:\\msys64, or set MSYS2_ROOT)"),
    Req("GNU make, diffutils (in MSYS2)", "drive the asset exporters (src/game/ssb64/Makefile)",
        check_make, install_make,
        "in an MSYS2 shell: pacman -Sy make diffutils", needs=("MSYS2",)),
    Req(".NET 8 runtime", "Rxdk.Cli and RXDK's host tools are .NET 8 programs",
        check_dotnet, winget_install("Microsoft.DotNet.Runtime.8"),
        "https://dotnet.microsoft.com/download/dotnet/8.0 (the .NET Runtime)"),
    Req("RXDK host tools (Rxdk.Cli)", "compiles, links and packs the XBE and the ISO",
        check_rxdk_cli, install_rxdk_cli,
        "RXDK's Visual Studio or VS Code extension: Install Prerequisites"),
    Req("RXDK SDK", "the Xbox headers and libraries (v%d.%d.%d or later)" % RXDK_MIN,
        rxdk_component("SDK"), rxdk_install("install-sdk", "update-sdk"),
        "Rxdk.Cli install-sdk", needs=(".NET 8 runtime", "RXDK host tools (Rxdk.Cli)")),
    Req("RXDK tools (imagebld, xdvdfs)", "XBE image and ISO packing",
        rxdk_component("Tools"), rxdk_install("install-tools", "update-tools"),
        "Rxdk.Cli install-tools", needs=(".NET 8 runtime", "RXDK host tools (Rxdk.Cli)")),
    Req("RXDK LLVM toolchain", "the clang and lld the Xbox code is compiled with",
        rxdk_component("LLVM"), rxdk_install("install-llvm"),
        "Rxdk.Cli install-llvm", needs=(".NET 8 runtime", "RXDK host tools (Rxdk.Cli)")),
    Req("disk space (%d GB)" % FREE_GB, "the decomp, the exported assets and the build",
        check_disk, None, "free some space on this drive"),
    Req("Visual Studio (SSB-X.sln only)", "building and F5 debugging in the IDE",
        check_vs, None,
        "Visual Studio Installer > Modify > Individual components > "
        "MSBuild support for LLVM (clang-cl) toolset", optional=True),
]


def run(cmd):
    say("running: " + " ".join('"%s"' % c if " " in c else c for c in cmd))
    r = subprocess.run(cmd)
    if r.returncode != 0:
        raise RuntimeError("exited %d: %s" % (r.returncode, cmd[0]))


def report():
    """Check everything; print a line each. Returns the failed Reqs."""
    failed = []
    for q in REQS:
        try:
            ok, detail = q.check()
        except Exception as e:      # a check must never stop the report
            ok, detail = False, "check failed: %s" % e
        mark = "ok" if ok else ("--" if q.optional else "!!")
        print("  [%s] %-34s %s" % (mark, q.name, detail), flush=True)
        if not ok and not q.optional:   # optional: reported, never blocks build.bat
            failed.append(q)
    return failed


def explain(failed):
    print("\nMissing or too old:", flush=True)
    for q in failed:
        how = ("the build script can install it (by hand: %s)" % q.manual if q.install
               else "install it yourself: %s" % q.manual)
        print("  * %s -- %s.\n      %s" % (q.name, q.why, how), flush=True)


def fix(failed, assume_yes):
    """Install what can be installed, in REQS order (dependencies first).
    Returns True when everything checks out afterwards."""
    def by_winget(q):
        return q.install.__qualname__.startswith("winget_install")

    auto = [q for q in failed if q.install]
    if not winget() and any(by_winget(q) for q in auto):
        say("winget is not on this machine, so %s cannot be installed for you; "
            "use the links above" % ", ".join(q.name for q in auto if by_winget(q)))
        auto = [q for q in auto if not by_winget(q)]
    if not auto:
        return False
    print("\nThe build script can install: %s" % ", ".join(q.name for q in auto), flush=True)
    if any(by_winget(q) for q in auto):
        print("(with winget: Windows may ask for permission)", flush=True)
    if any(q.name.startswith("RXDK") for q in auto):
        print("(RXDK goes into %s, where its own installers put it)" % rxdk_root(), flush=True)
    if not assume_yes and os.environ.get("SSBX_NONINTERACTIVE"):
        # inside a Visual Studio build: nobody to ask, and installers
        # should not start from the IDE's build
        say("error: missing prerequisites -- double-click build.bat (or run "
            "python setup.py) once to install them, then build again")
        return False
    if not assume_yes:
        try:
            a = input("Install them now? [Y/n] ").strip().lower()
        except EOFError:            # no one to ask (NUL reads as a console on Windows)
            say("no answer; run again with --yes to install without asking")
            return False
        if a not in ("", "y", "yes"):
            return False
    for q in REQS:
        if q not in auto:
            continue
        ok, _ = q.check()
        if ok:
            continue
        if any(not next(r for r in REQS if r.name == n).check()[0] for n in q.needs):
            say("skipping %s: what it needs is still missing" % q.name)
            continue
        say("installing %s" % q.name)
        try:
            q.install()
        except Exception as e:
            say("could not install %s: %s" % (q.name, e))
    print("\nAfter installing:", flush=True)
    return not report()


def ensure(assume_yes=False, install=True):
    """The whole check, as setup.py runs it first. Returns True when the
    build can go ahead."""
    say("checking what the build needs")
    failed = report()
    if not failed:
        return True
    explain(failed)
    if install and fix(failed, assume_yes):
        say("everything is in place")
        return True
    return False


def main():
    import argparse
    ap = argparse.ArgumentParser(description="what the SSB-X build needs, and installing it")
    ap.add_argument("--fix", action="store_true", help="offer to install what is missing")
    ap.add_argument("--yes", action="store_true", help="install without asking")
    a = ap.parse_args()
    sys.exit(0 if ensure(a.yes, install=a.fix or a.yes) else 1)


if __name__ == "__main__":
    main()
