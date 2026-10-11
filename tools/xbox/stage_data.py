#!/usr/bin/env python3
"""Stage the game's data for the Xbox: data/ beside default.xbe.

The Dreamcast disc (scripts/make_cdi.sh) holds the exported romdisk/ and
disc/ trees flat in its root, minus every file the models.bnd bundle
already carries (ssb_bundle.py --names), plus a disc.id stamp that
src/dc/assetroot.c reads to know it booted from a disc. The Xbox build
stages exactly that into data/, which rxdk.project.json deploys next to
the XBE; Platform/xbox/kos_fs_xbox.c serves /cd from D:\\data.

The Dreamcast's sector-padding file (disc_layout.PAD) is left out: it only
moves files outward on a CD-R, and the Xbox reads from a DVD or a hard disk.

Always re-staged from the current exports (a stale copy would be silently
wrong, never absent).
"""
import os
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GAME = os.path.join(ROOT, "src", "game", "ssb64")
OUT = os.path.join(ROOT, "data")


def bundle_names():
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "export", "ssb_bundle.py"),
                        "--names"], cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit("stage_data: ssb_bundle.py --names failed:\n" + r.stderr)
    return set(r.stdout.split())


def declared_assets():
    """The Makefile's $(ASSETS): everything the game reads at runtime."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import hostenv
    r = hostenv.run_make(["xbox-asset-names"], evals=["xbox-asset-names: ; @echo $(ASSETS)"],
                         extra_vars=["SSB_NO_INTROCROWD=1"], capture=True)
    if r.returncode != 0:
        raise SystemExit("stage_data: could not list $(ASSETS):\n" + (r.stderr or ""))
    return (r.stdout or "").split()


def main():
    romdisk = os.path.join(GAME, "romdisk")
    disc = os.path.join(GAME, "disc")
    if not os.path.isdir(romdisk) or not os.path.isfile(os.path.join(disc, "models.bnd")):
        raise SystemExit("stage_data: the exports are missing -- run the asset step first")
    # Every one, not a sample: an export that stopped part way leaves a
    # tree that stages and builds and then fails in the scene that wants
    # the missing file (no music bank, no menus) -- on the console.
    missing = [n for n in declared_assets() if not os.path.isfile(os.path.join(GAME, n))]
    if missing:
        raise SystemExit("stage_data: %d of the game's assets were never exported, e.g. %s\n"
                         "  -- rerun the asset step (python setup.py)"
                         % (len(missing), " ".join(missing[:8])))
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)
    for src in (romdisk, disc):
        for name in os.listdir(src):
            p = os.path.join(src, name)
            if os.path.isfile(p):
                shutil.copy2(p, os.path.join(OUT, name))
    bundled = bundle_names()
    dropped = 0
    for name in bundled:
        p = os.path.join(OUT, name)
        if os.path.isfile(p):
            os.remove(p)
            dropped += 1
    stamp = "ssb64x %s" % time.strftime("%Y-%m-%dT%H:%MZ", time.gmtime())
    with open(os.path.join(OUT, "disc.id"), "w", encoding="ascii", newline="\n") as f:
        f.write(stamp + "\n")
    files = os.listdir(OUT)
    size = sum(os.path.getsize(os.path.join(OUT, f)) for f in files)
    print("stage_data: %d files, %.1f MB in data/ (%d carried by models.bnd)"
          % (len(files), size / 1048576.0, dropped))


if __name__ == "__main__":
    main()
