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
import hashlib

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
    # The stamp names the data, not the moment: a digest of what was staged,
    # with the newest staged file's time. Re-staging the same exports then
    # leaves data/ exactly as it was -- copy2 keeps every file's time -- so a
    # Visual Studio build does not repack or redeploy an unchanged disc.
    h = hashlib.sha1()
    newest = 0.0
    for name in sorted(os.listdir(OUT)):
        p = os.path.join(OUT, name)
        st = os.stat(p)
        newest = max(newest, st.st_mtime)
        h.update(name.encode() + b"\0" + str(st.st_size).encode() + b"\0")
        with open(p, "rb") as f:
            h.update(hashlib.sha1(f.read()).digest())
    stamp = "SSB-X %s" % h.hexdigest()[:16]
    idp = os.path.join(OUT, "disc.id")
    with open(idp, "w", encoding="ascii", newline="\n") as f:
        f.write(stamp + "\n")
    os.utime(idp, (newest, newest))
    files = os.listdir(OUT)
    size = sum(os.path.getsize(os.path.join(OUT, f)) for f in files)
    print("stage_data: %d files, %.1f MB in data/ (%d carried by models.bnd)"
          % (len(files), size / 1048576.0, dropped))


if __name__ == "__main__":
    main()
