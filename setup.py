#!/usr/bin/env python3
"""One command from a ROM to an Xbox disc image.

    python setup.py                 # everything -> out/Release/XISO/ssb64x.iso
    python setup.py --check         # say what is there and what is missing
    python setup.py --skip-build    # stop before compiling (to build in Visual Studio)
    python setup.py --config Debug  # the debug configuration

Needs: Python 3 with numpy and pygfxd (installed on first run), git, MSYS2
with GNU make (the Dreamcast port's own Makefile drives the asset exporters),
the RXDK toolchain, and your US Super Smash Bros. ROM as baserom.z64 in this
folder.

The steps, in order. None is skipped because its output already exists:
every one is either cheap and always run, or driven by make, whose rules
know their inputs. (A derived file gated on its own existence goes stale
silently -- the MK64 port's lesson.)

  1. the decomp, at the revision docker/pins.env names, with the patches in
     docker/patches applied (extern/ssb-decomp-re)
  2. the asset exporters, through src/game/ssb64/Makefile: every romdisk/
     file, then the disc bundle (tools/xbox/hostenv.py on how make is run)
  3. the code the Makefile generates for the game: the particle-bank and
     weapon-table symbols, the sine table, libultra's sinf/cosf, the reverb
     and send tables, the credits text
  4. the Xbox's own: the reverb network (tools/xbox/gen_reverb.py), the
     dashboard icons (make_icons.py), data/ (stage_data.py), and
     rxdk.project.json (gen_manifest.py)
  5. the build: Rxdk.Cli compiles, links, bundles the icons and packs the ISO
"""
import argparse
import hashlib
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT, "tools", "xbox"))
import hostenv  # noqa: E402

ROM = os.path.join(ROOT, "baserom.z64")
ROM_SHA1 = "e2929e10fccc0aa84e5776227e798abc07cedabf"
DECOMP = hostenv.DECOMP_DIR
GAME = hostenv.GAME_DIR

CODEGEN = ["particlebanks.ld", "wpattrs.ld", "gusinf.c", "gucosf.c", "sintable.c",
           "aica_reverb.h", "aica_sends.h",
           "credits/staff.credits.encoded", "credits/titles.credits.encoded",
           "credits/info.credits.encoded", "credits/companies.credits.encoded"]


def say(msg):
    print("setup: " + msg, flush=True)


def die(msg):
    print("setup: ERROR: " + msg, file=sys.stderr, flush=True)
    sys.exit(1)


def run(cmd, cwd=ROOT, env=None):
    r = subprocess.run(cmd, cwd=cwd, env=env)
    if r.returncode != 0:
        die("failed (%d): %s" % (r.returncode, " ".join(cmd)))


# ------------------------------------------------------------------ checks

def find_rxdk_cli():
    cands = [os.environ.get("RXDK_CLI", "")]
    pd = os.environ.get("ProgramData", r"C:\ProgramData")
    cands += [os.path.join(pd, "RXDK", "tools", "Rxdk.Cli.exe"),
              os.path.join(pd, "RXDK", "engine", "Rxdk.Cli.exe")]
    for c in cands:
        if c and os.path.isfile(c):
            return c
    return shutil.which("Rxdk.Cli.exe") or shutil.which("Rxdk.Cli")


def rom_ok():
    if not os.path.isfile(ROM):
        return False, "baserom.z64 is missing (your US ROM, big-endian .z64)"
    with open(ROM, "rb") as f:
        data = f.read()
    sha = hashlib.sha1(data).hexdigest()
    if sha != ROM_SHA1:
        if data[:4] in (b"\x37\x80\x40\x12", b"\x40\x12\x37\x80"):
            return False, "baserom.z64 is byte-swapped (.v64/.n64): convert it to .z64"
        return False, "baserom.z64 has SHA-1 %s, not the US v1.0 ROM's %s" % (sha, ROM_SHA1)
    return True, "US v1.0"


def python_deps(install):
    missing = []
    for mod in ("numpy", "pygfxd"):
        try:
            __import__(mod)
        except ImportError:
            missing.append(mod)
    if missing and install:
        say("installing %s" % " ".join(missing))
        req = os.path.join(ROOT, "docker", "requirements.txt")
        run([sys.executable, "-m", "pip", "install", "-r", req])
        missing = []
    return missing


def decomp_state():
    if not os.path.isdir(os.path.join(DECOMP, ".git")):
        return "missing"
    r = subprocess.run(["git", "rev-parse", "HEAD"], cwd=DECOMP, capture_output=True, text=True)
    if r.stdout.strip() != hostenv.decomp_rev():
        return "wrong revision"
    return "ok"


def check():
    ok = True
    good, why = rom_ok()
    say("ROM: %s" % why)
    ok &= good
    miss = python_deps(False)
    say("python: %s" % ("numpy, pygfxd present" if not miss else "missing " + " ".join(miss)))
    try:
        say("MSYS2 make: %s" % os.path.dirname(hostenv.find_msys_bash()))
    except SystemExit as e:
        say("MSYS2 make: %s" % e)
        ok = False
    cli = find_rxdk_cli()
    say("RXDK CLI: %s" % (cli or "not found"))
    ok &= cli is not None
    say("decomp: %s" % decomp_state())
    n = len(os.listdir(os.path.join(GAME, "romdisk"))) if os.path.isdir(os.path.join(GAME, "romdisk")) else 0
    say("exports: %d romdisk files, bundle %s" % (
        n, "present" if os.path.isfile(os.path.join(GAME, "disc", "models.bnd")) else "missing"))
    say("data/: %s" % ("staged" if os.path.isfile(os.path.join(ROOT, "data", "models.bnd")) else "not staged"))
    iso = os.path.join(ROOT, "out", "Release", "XISO", "ssb64x.iso")
    say("ISO: %s" % (iso if os.path.isfile(iso) else "not built"))
    return ok


# ------------------------------------------------------------------- steps

def step_decomp():
    rev = hostenv.decomp_rev()
    state = decomp_state()
    if state == "missing":
        say("fetching the decomp at %s" % rev[:9])
        if os.path.isdir(DECOMP):
            shutil.rmtree(DECOMP)
        os.makedirs(os.path.dirname(DECOMP), exist_ok=True)
        # LF, whatever this machine's git says: docker/patches are LF
        # (.gitattributes) and must apply to the decomp as it is on Linux
        run(["git", "clone", "--quiet", "--config", "core.autocrlf=false",
             hostenv.DECOMP_URL, DECOMP])
    if state != "ok":
        run(["git", "checkout", "--quiet", "--force", rev], cwd=DECOMP)
        run(["git", "reset", "--quiet", "--hard", rev], cwd=DECOMP)
    # the patches: applied unless every one already is (reverse-applies)
    pdir = os.path.join(ROOT, "docker", "patches")
    patches = sorted(p for p in os.listdir(pdir) if p.endswith(".patch"))
    for p in patches:
        path = os.path.join(pdir, p)
        if subprocess.run(["git", "apply", "--reverse", "--check", path], cwd=DECOMP,
                          capture_output=True).returncode == 0:
            continue
        say("patching the decomp: %s" % p)
        run(["git", "apply", path], cwd=DECOMP)


def step_assets(jobs):
    say("exporting the assets (src/game/ssb64/Makefile)")
    env_vars = ["SSB_NO_INTROCROWD=1"]
    # the item models write wpfushigibanarazor.mdl as a side output the
    # bundle rule needs but no rule names: the romdisk first, then the disc
    r = hostenv.run_make(["xbox-assets"], evals=[".SECONDEXPANSION:", "xbox-assets: $$(ASSETS)"],
                         extra_vars=env_vars, jobs=jobs)
    if r.returncode != 0:
        die("the asset exporters failed (see above)")
    r = hostenv.run_make(["disc/efslash.mdl", "disc/models.bnd"], extra_vars=env_vars, jobs=jobs)
    if r.returncode != 0:
        die("the disc bundle failed (see above)")
    say("generating the game's generated sources")
    r = hostenv.run_make(CODEGEN, jobs=jobs)
    if r.returncode != 0:
        die("code generation failed (see above)")


def step_xbox():
    env = dict(os.environ, SSB_DECOMP_DIR=DECOMP)
    for tool in ("gen_reverb.py", "make_icons.py", "stage_data.py", "gen_manifest.py"):
        run([sys.executable, os.path.join(ROOT, "tools", "xbox", tool)], env=env)


def step_build(config):
    cli = find_rxdk_cli()
    if cli is None:
        die("Rxdk.Cli.exe not found: install RXDK (RXDK > RXDK Window > Install "
            "Prerequisites) or set RXDK_CLI")
    say("building %s with %s" % (config, cli))
    run([cli, "build", "--project-root", ROOT, "--configuration", config])
    iso = os.path.join(ROOT, "out", config, "XISO", "ssb64x.iso")
    if os.path.isfile(iso):
        say("done: %s (%.1f MB)" % (iso, os.path.getsize(iso) / 1048576.0))
    else:
        say("built; the ISO is under out/%s/XISO" % config)


def main():
    ap = argparse.ArgumentParser(description="ROM -> Xbox disc image for SSB64X")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--skip-build", action="store_true")
    ap.add_argument("--config", default="Release", choices=["Release", "Debug"])
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    a = ap.parse_args()
    if a.check:
        sys.exit(0 if check() else 1)

    good, why = rom_ok()
    if not good:
        die(why)
    python_deps(True)
    hostenv.find_msys_bash()
    step_decomp()
    step_assets(a.jobs)
    step_xbox()
    if not a.skip_build:
        step_build(a.config)


if __name__ == "__main__":
    main()
