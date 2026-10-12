#!/usr/bin/env python3
"""One command from a ROM to an Xbox disc image.

    python setup.py                 # everything -> out/Release/XISO/SSB-X.iso
    python setup.py --check         # say what is there and what is missing
    python setup.py --skip-build    # stop before compiling (to build in Visual Studio)
    python setup.py --config Debug  # the debug configuration
    python setup.py --yes           # install missing prerequisites without asking

build.bat in this folder is the same with Python found (or installed) first.

Needs your US Super Smash Bros. ROM as baserom.z64 in this folder, and
the tools tools/xbox/prereqs.py lists: 64-bit Python 3.11-3.14 with numpy and
pygfxd, git, MSYS2 with GNU make, the .NET 8 runtime and RXDK (host tools,
SDK, LLVM). Before any work it checks every one and offers to install what
is missing -- winget for the programs, pacman for make, pip for the
packages, RXDK's own installer for RXDK.

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
import prereqs  # noqa: E402

ROM = os.path.join(ROOT, "baserom.z64")
ROM_SHA1 = "e2929e10fccc0aa84e5776227e798abc07cedabf"
ISO_NAME = "SSB-X.iso"          # rxdk.project.json "name" + .iso (tools/xbox/gen_manifest.py)
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

def git():
    """git by its full path: one winget has just installed is not on this
    process's PATH yet."""
    g = prereqs.find_git()
    if not g:
        die("git is not installed (python setup.py offers to install it)")
    return g


def rom_ok():
    if not os.path.isfile(ROM):
        return False, ("baserom.z64 not found. Copy your US Super Smash Bros. ROM into\n"
                       "    %s\n"
                       "  and name it exactly  baserom.z64  (big-endian .z64, SHA-1 %s)"
                       % (ROOT, ROM_SHA1))
    with open(ROM, "rb") as f:
        data = f.read()
    sha = hashlib.sha1(data).hexdigest()
    if sha != ROM_SHA1:
        if data[:4] in (b"\x37\x80\x40\x12", b"\x40\x12\x37\x80"):
            return False, "baserom.z64 is byte-swapped (.v64/.n64): convert it to .z64"
        return False, "baserom.z64 has SHA-1 %s, not the US v1.0 ROM's %s" % (sha, ROM_SHA1)
    return True, "US v1.0"


def decomp_state():
    if not os.path.isdir(os.path.join(DECOMP, ".git")):
        return "missing"
    g = prereqs.find_git()
    if not g:
        return "present (git missing, revision unknown)"
    r = subprocess.run([g, "rev-parse", "HEAD"], cwd=DECOMP, capture_output=True, text=True)
    if r.stdout.strip() != hostenv.decomp_rev():
        return "wrong revision"
    return "ok"


def check():
    good, why = rom_ok()
    say("ROM: %s" % why)
    say("prerequisites:")
    failed = prereqs.report()
    if failed:
        prereqs.explain(failed)
        print("\n  python setup.py offers to install the ones marked so.", flush=True)
    ok = good and not failed
    say("decomp: %s" % decomp_state())
    n = len(os.listdir(os.path.join(GAME, "romdisk"))) if os.path.isdir(os.path.join(GAME, "romdisk")) else 0
    say("exports: %d romdisk files, bundle %s" % (
        n, "present" if os.path.isfile(os.path.join(GAME, "disc", "models.bnd")) else "missing"))
    say("data/: %s" % ("staged" if os.path.isfile(os.path.join(ROOT, "data", "models.bnd")) else "not staged"))
    iso = os.path.join(ROOT, "out", "Release", "XISO", ISO_NAME)
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
        run([git(), "clone", "--quiet", "--config", "core.autocrlf=false",
             hostenv.DECOMP_URL, DECOMP])
    if state != "ok":
        run([git(), "checkout", "--quiet", "--force", rev], cwd=DECOMP)
        run([git(), "reset", "--quiet", "--hard", rev], cwd=DECOMP)
    # the patches: applied unless every one already is (reverse-applies)
    pdir = os.path.join(ROOT, "docker", "patches")
    patches = sorted(p for p in os.listdir(pdir) if p.endswith(".patch"))
    for p in patches:
        path = os.path.join(pdir, p)
        if subprocess.run([git(), "apply", "--reverse", "--check", path], cwd=DECOMP,
                          capture_output=True).returncode == 0:
            continue
        say("patching the decomp: %s" % p)
        run([git(), "apply", path], cwd=DECOMP)


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
    # SSB-X.vcxproj names the same sources as the manifest; when a source has
    # come or gone it is regenerated -- but not from inside a Visual Studio
    # build, where rewriting the open project makes VS stop to reload it
    import gen_vcxproj
    if not gen_vcxproj.in_sync():
        if os.environ.get("SSBX_NONINTERACTIVE"):
            die("SSB-X.vcxproj's file list is out of date: close Visual Studio, run "
                "python tools/xbox/gen_vcxproj.py (or build.bat), then reopen SSB-X.sln")
        say("the source list changed: regenerating SSB-X.vcxproj")
        gen_vcxproj.generate()


def step_build(config):
    cli = prereqs.find_rxdk_cli()
    if cli is None:
        die("Rxdk.Cli.exe not found (python setup.py offers to install RXDK)")
    say("building %s with %s" % (config, cli))
    run([cli, "build", "--project-root", ROOT, "--configuration", config])
    iso = os.path.join(ROOT, "out", config, "XISO", ISO_NAME)
    if os.path.isfile(iso):
        say("done: %s (%.1f MB)" % (iso, os.path.getsize(iso) / 1048576.0))
    else:
        say("built; the ISO is under out/%s/XISO" % config)


def main():
    ap = argparse.ArgumentParser(description="ROM -> Xbox disc image for SSB-X")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--skip-build", action="store_true")
    ap.add_argument("--config", default="Release", choices=["Release", "Debug"])
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--yes", action="store_true",
                    help="install missing prerequisites without asking")
    a = ap.parse_args()
    if a.check:
        sys.exit(0 if check() else 1)

    # the ROM first: it is the one thing no script can fetch, so say so
    # before anything is installed or downloaded
    good, why = rom_ok()
    if not good:
        die(why)
    say("ROM: %s" % why)
    if not prereqs.ensure(assume_yes=a.yes):
        die("the build cannot start until the items above are installed "
            "(python tools/xbox/prereqs.py shows the list again)")
    step_decomp()
    step_assets(a.jobs)
    step_xbox()
    if not a.skip_build:
        step_build(a.config)


if __name__ == "__main__":
    main()
