#!/usr/bin/env python3
"""overlay_check.py -- hold the port's overlay reloads to the ROM.

On the N64 the cartridge is large and the RAM is not, so most of the
game's code lives in ROM and is DMAed in a scene at a time. Each piece is
an overlay, and sc/scmanager.c reloads the ones a scene needs before
starting it: syDmaLoadOverlay copies the overlay's .data back off the ROM
and bzeroes its noload segment (sys/dma.c:109-112). A scene module's
statics are therefore zero on every entry, not only the first.

The port links the overlays in, so it has to do that bzero itself, and it
does it two ways (src/dc/overlay.h has the argument):

  - a port file clears its own statics in an xxxOverlayLoad(), because a
    port file's boundaries are not a decomp file's;
  - a decomp .c the port compiles unmodified has its whole .bss renamed
    into an ovlN_noload section by the game's Makefile, and
    src/dc/overlay.c bzeroes the range ld brackets it with.

Both rot the same way -- a module gains a static, or an object moves
overlay, and nobody remembers -- so this is what stops that. It checks
four things, and the first is the one that makes the rest mean anything:

  1. every claim below about which overlay a file belongs to is really
     in smashbrothers.us.yaml's segment list for that overlay;
  2. every port file mapped here has an xxxOverlayLoad, and it clears
     every symbol in its object's .bss;
  3. every decomp object mapped here is renamed into the right section
     by the Makefile, and its symbols really land inside that section's
     range in the linked ELF;
  4. no module has an xxxOverlayLoad the map below does not know about,
     and a port-only scene (PORT_ONLY) has none and its arm loads none;
  5. a decomp object in an overlay that takes no rename because it keeps
     nothing really keeps nothing. lb/lbbackup.c is the one -- the save
     data it works on is the scene manager's, not its own -- and a static
     added to it would otherwise survive a scene change with nobody's
     bzero over it.

Needs a build (the .o files and the ELF); scripts/test_oracle.sh runs it
after one.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src" / "dc"
OBJDIR = ROOT / "src" / "game" / "ssb64"
ELF = OBJDIR / "ssb64.elf"
MAKEFILE = OBJDIR / "Makefile"
DECOMP = Path(os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re"))
YAML = DECOMP / "smashbrothers.us.yaml"
NM = "sh-elf-nm"

# The port's files that stand in for a file in an overlay, with the
# overlay and the decomp files they stand in for. dSCManagerOverlays'
# numbering (sc/scmanager.c:63); src/dc/overlay.h has the same table as
# OVERLAY_* names, and src/dc/overlay.c is what calls each arm.
#
# Overlay 0 -- lb/lbcommon, lb/lbreloc, lb/lbfade, lb/lbtransition and
# lb/lbparticle -- is not here: scManagerRunLoop loads
# it once at boot (scmanager.c:826) and no scene reloads it, so those
# files keep their state on purpose. src/dc/lbparticle.c's free lists and
# bank tables are the clearest case of that: the scene that loads a bank
# writes them and the next one writes them again, and nothing between the
# two is entitled to zero them.
PORT_FILES = {
    "scsubsysfighter.c": (1, ["sc/scsubsys/scsubsysfighter"]),
    # the 1P ladder's router. smashbrothers.us.yaml:402 puts
    # it at the front of ovl2, and its .bss is the run's carried totals
    # -- so the clear below is what hands the NEXT run a fresh zero. The
    # router never loads overlay 2 itself; src/dc/sc1pmanager.h says why
    # that is the whole reason the state can live in an overlay at all.
    "sc1pmanager.c":     (2, ["sc/sc1pmode/sc1pmanager"]),
    "ftmanager.c":       (2, ["ft/ftmanager"]),
    "ftmain.c":          (2, ["ft/ftmain"]),
    "ftparam.c":         (2, ["ft/ftparam"]),
    "mpcommon.c":        (2, ["mp/mpcommon"]),
    "gmcommon.c":        (2, ["gm/gmcommon"]),
    "gmcamera.c":        (2, ["gm/gmcamera"]),
    "ifcommon.c":        (2, ["if/ifcommon"]),
    "ifscreenflash.c":   (2, ["if/ifscreenflash"]),
    # the port's one file for the whole gr/ side of the overlay
    "stage.c":           (2, ["gr/grcommonsetup", "gr/grwallpaper",
                              "gr/grcommon/grhyrule"]),
    # the last of the nine stage logic files, and the one
    # file so far that is a stage's own AND owns a pack -- it loads the
    # Arwing out of `efarwing.mdl`. Its clear is therefore ftShadow's
    # shape (release, then the pointers), and grOverlayLoad in stage.c is
    # what calls it: the decomp has no such hook, because its Arwing tree
    # is a pointer into a buffer the FIGHTER manager owns.
    "grsector.c":        (2, ["gr/grcommon/grsector"]),
    "ftcommon.c":        (3, ["ft/ftcommon/ftcommonwait"]),
    "ftshadow.c":        (3, ["ft/ftshadow"]),
    "scvsbattle.c":      (4, ["sc/sccommon/scvsbattle"]),
    "mntitle.c":        (10, ["mn/mncommon/mntitle"]),
    # the decomp splits this scene across two files (mnnocontroller.c
    # and its file-loading half, mnnocontrollerfiles.c); the port folds
    # both into one, so both decomp names claim overlay 11.
    "mnnocontroller.c": (11, ["mn/mncommon/mnnocontroller",
                              "mn/mncommon/mnnocontrollerfiles"]),
    "mnmodeselect.c":   (17, ["mn/mncommon/mnmodeselect"]),
    "mn1pmode.c":       (18, ["mn/mn1pmode/mn1pmode"]),
    "mnplayers1ptraining.c": (28, ["mn/mnplayers/mnplayers1ptraining"]),
    "sc1ptrainingmode.c": (7, ["sc/sc1pmode/sc1ptrainingmode"]),
    "mnplayers1pgame.c": (27, ["mn/mnplayers/mnplayers1pgame"]),
    "mnplayers1pbonus.c": (29, ["mn/mnplayers/mnplayers1pbonus"]),
    "sc1pchallenger.c": (23, ["sc/sc1pmode/sc1pchallenger"]),
    "sc1pintro.c": (24, ["sc/sc1pmode/sc1pintro"]),
    "sc1pgame.c": (65, ["sc/sc1pmode/sc1pgame"]),
    # the bonus stages. The scene and its file loader are
    # one port file, the shape mnnocontroller.c already set, so both
    # decomp names claim overlay 6.
    "sc1pbonusstage.c": (6, ["sc/sc1pmode/sc1pbonusstage",
                             "sc/sc1pmode/sc1pbonusstagefiles"]),
    "sc1pstageclear.c": (56, ["sc/sc1pmode/sc1pstageclear"]),
    "mn1pcontinue.c": (55, ["mn/mn1pmode/mn1pcontinue"]),
    "mnvsmode.c":       (19, ["mn/mnvsmode/mnvsmode"]),
    "mnvsoptions.c":    (20, ["mn/mnvsmode/mnvsoptions"]),
    "mnvsitemswitch.c": (21, ["mn/mnvsmode/mnvsitemswitch"]),
    "mnmessage.c":      (22, ["mn/mncommon/mnmessage"]),
    "mnscreenadjust.c": (25, ["mn/mnoption/mnscreenadjust"]),
    "mnplayersvs.c":    (26, ["mn/mnplayers/mnplayersvs"]),
    "mnmaps.c":         (30, ["mn/mnmaps/mnmaps"]),
    "scvsresults.c":    (31, ["mn/mnvsmode/mnvsresults"]),
    "mnvsrecord.c":     (32, ["mn/mndata/mnvsrecord"]),
    "mncharacters.c":   (33, ["mn/mndata/mncharacters"]),
    "mnbackupclear.c":  (53, ["mn/mnoption/mnbackupclear"]),
    "mvopeningroom.c":  (34, ["mv/mvopening/mvopeningroom"]),
    "mvopeningportraits.c": (35, ["mv/mvopening/mvopeningportraits"]),
    "mvopeningmario.c": (36, ["mv/mvopening/mvopeningmario"]),
    "mvopeningdonkey.c": (37, ["mv/mvopening/mvopeningdonkey"]),
    "mvopeningsamus.c": (38, ["mv/mvopening/mvopeningsamus"]),
    "mvopeninglink.c": (40, ["mv/mvopening/mvopeninglink"]),
    "mvopeningyoshi.c": (41, ["mv/mvopening/mvopeningyoshi"]),
    "mvopeningkirby.c": (43, ["mv/mvopening/mvopeningkirby"]),
    "mvopeningfox.c": (39, ["mv/mvopening/mvopeningfox"]),
    "mvopeningpikachu.c": (42, ["mv/mvopening/mvopeningpikachu"]),
    "mvopeningrun.c": (44, ["mv/mvopening/mvopeningrun"]),
    "mvopeningcliff.c": (46, ["mv/mvopening/mvopeningcliff"]),
    "mvopeningyamabuki.c": (48, ["mv/mvopening/mvopeningyamabuki"]),
    "mvopeningjungle.c": (51, ["mv/mvopening/mvopeningjungle"]),
    "mvopeningyoster.c": (45, ["mv/mvopening/mvopeningyoster"]),
    "mvopeningsector.c": (50, ["mv/mvopening/mvopeningsector"]),
    "mvopeningstandoff.c": (47, ["mv/mvopening/mvopeningstandoff"]),
    "mvopeningclash.c": (49, ["mv/mvopening/mvopeningclash"]),
    "mvopeningnewcomers.c": (52, ["mv/mvopening/mvopeningnewcomers"]),
    "mvending.c":       (54, ["mv/mvending/mvending"]),
    "mncongra.c":       (57, ["mn/mncommon/mncongra"]),
    "mnoption.c":       (60, ["mn/mnoption/mnoption"]),
    "mndata.c":         (61, ["mn/mndata/mndata"]),
    "mnsoundtest.c":    (62, ["mn/mndata/mnsoundtest"]),
    # the decomp splits this scene across two files (scautodemo.c and
    # its file-loading half, scautodemofiles.c); the port folds both
    # into one, the same call mnnocontroller.c's own entry above makes,
    # so both decomp names claim overlay 64.
    "scautodemo.c":     (64, ["sc/sccommon/scautodemo",
                              "sc/sccommon/scautodemofiles"]),
    # the decomp splits this scene across two files too (scexplain.c and
    # its file-loading half, scexplainfiles.c); both claim overlay 63.
    "scexplain.c":      (63, ["sc/sccommon/scexplain",
                              "sc/sccommon/scexplainfiles"]),
    "mnstartup.c":      (58, ["mn/mncommon/mnstartup"]),
    "scstaffroll.c":    (59, ["sc/sccommon/scstaffroll"]),
}

# The decomp .c files the game compiles unmodified that live in an
# overlay, and so get the linker-bracketed treatment instead. Everything
# else the game compiles out of the decomp is sys/ -- the object
# system, the allocator, the vector and RNG helpers -- which is always
# resident and has no reload at all.
DECOMP_OBJS = {
    "scsubsyscontroller.o": (1, "sc/scsubsys/scsubsyscontroller"),
    "mpcollision.o":        (2, "mp/mpcollision"),
    "mpprocess.o":          (2, "mp/mpprocess"),
    "gmcollision.o":        (2, "gm/gmcollision"),
    "ftcommondata.o":       (2, "ft/ftcommondata"),
    "gmcolscripts.o":       (2, "gm/gmcolscripts"),
    "ftphysics.o":          (2, "ft/ftphysics"),
    # ef/efparticle.c. It does keep statics -- the bank cache
    # -- and they must be zero on every scene entry, or the second battle
    # in a scene finds the first's heap addresses still in the table.
    "efparticle.o":         (2, "ef/efparticle"),
    "ftanim.o":             (2, "ft/ftanim"),
    "ftpublic.o":           (3, "ft/ftpublic"),
    # the smallest common statuses. None of them is in overlay
    # 3's bss list in smashbrothers.us.yaml -- they keep no statics -- but
    # they take the rename anyway, so check 5 below is what keeps that true
    # rather than a comment. (ftcommoncapturewait dropped off this list at
    # when its two functions were ported into src/dc/ftcommon.c
    # with the rest of the two-body catch connect.)
    "ftcommonstopceil.o":         (3, "ft/ftcommon/ftcommonstopceil"),
    "ftcommondownforwardback.o":  (3, "ft/ftcommon/ftcommondownforwardback"),
    "ftcommondownattack.o":       (3, "ft/ftcommon/ftcommondownattack"),
    "ftcommonrebound.o":          (3, "ft/ftcommon/ftcommonrebound"),
    "ftcommoncliffattack.o":      (3, "ft/ftcommon/ftcommoncliffattack"),
    "ftcommonlandingair.o":       (3, "ft/ftcommon/ftcommonlandingair"),
    "ftcommoncapturecaptain.o":   (3, "ft/ftcommon/ftcommoncapturecaptain"),
    # the five complete special moves. Overlay 3 as well,
    # and none of them in its bss list either.
    "ftmariospeciallw.o":         (3, "ft/ftchar/ftmario/ftmariospeciallw"),
    "ftdonkeyspeciallw.o":        (3, "ft/ftchar/ftdonkey/ftdonkeyspeciallw"),
    "ftpurinspeciallw.o":         (3, "ft/ftchar/ftpurin/ftpurinspeciallw"),
    "ftkirbyspeciallw.o":         (3, "ft/ftchar/ftkirby/ftkirbyspeciallw"),
    "ftpurinspecialn.o":          (3, "ft/ftchar/ftpurin/ftpurinspecialn"),
    "ftkirbycopypurinspecialn.o": (3, "ft/ftchar/ftkirby/ftkirbycopypurinspecialn"),
    # Mario's fireball. ftmario/ftluigi are overlay 2 -- they
    # keep the fighter's loaded-file base pointers (gFTMarioFileMain and the
    # rest), the only uncommented thing in either file, and their weapon
    # tables name those. wpmariofireball is overlay 3 like the specials; it
    # keeps no .bss (its WPDesc and attribute tables are .data), so it takes
    # the rename for form's sake the way the speciallw objects do.
    "ftmario.o":                  (2, "ft/ftchar/ftmario/ftmario"),
    "ftluigi.o":                  (2, "ft/ftchar/ftluigi/ftluigi"),
    "wpmariofireball.o":          (3, "wp/wpmario/wpmariofireball"),
}

# Decomp .c files the game compiles that live in an overlay and hold
# no state of their own, so there is nothing for a reload to clear and
# the Makefile gives them no rename. Check 5 is what keeps that true.
DECOMP_OBJS_NO_STATE = {
    "lbbackup.o": (0, "lb/lbbackup"),
}

# The port's own scenes (src/dc/scport.h), which stand in for nothing in
# the ROM and so have no overlay. Each must have no xxxOverlayLoad, and
# its scManagerRunScene arm must load no overlay: the memory card's scene
# runs between two of the game's scenes, and a reload there would
# zero the statics of the scene about to come back. Such a scene sets its
# own statics when it starts, instead. The value is the arm's case label.
PORT_ONLY = {
    "dcmemcard.c": "nSCKindDCMemCard",
}

# Symbols an overlay reload deliberately leaves alone, with the reason.
EXCLUDE = {
    # The debug facility installs this once, before the scene manager's
    # loop, because it has no ROM segment to be restored from
    # (src/dc/db.c db_install). Clearing it would take the collision
    # overlay and the serial log out of the second battle.
    "gSCVSBattleFuncDebug": "the debug facility's hook, installed at boot",
    # These are the overlay's *.data* on the N64 -- ft/ftdata.c's
    # dFTManagerDataFiles and the map files' table -- which the DMA
    # restores from the ROM unchanged and the game never writes. The port
    # has no ROM to restore from. The stage table is per scene
    # (stage acquisition) -- grStageAcquire fills a kind's slot and grStageRelease
    # empties it -- but it is not the overlay's to clear: a hold taken
    # before a scene starts (the debug facility's boot stage) spans the overlay
    # load, and clearing the table under it would free nothing and lose
    # everything. The count beside it goes the same way. The fighter
    # table is per scene since
    # ftManagerSetupFilesAllKind fills a kind's slot and
    # ftManagerReleaseFilesAll empties it where the scene's files go --
    # and the flag beside it says which slots the manager owns; both are
    # cleared by the release rather than by the overlay load, so that a
    # pack a caller installed by hand (the host test's mock) survives the
    # overlay load the way the game's file ids do. The keep set added in
    # is the third of that family: it is written before the
    # scene starts, by the scene manager, and has to outlive the overlay
    # load the scene's start runs or a heap reset inside the scene
    # (sudden death) would drop the packs the scene change kept.
    "gFTManagerModels": "the port's dFTManagerDataFiles, overlay .data",
    "sFTManagerLoadedByManager": "goes with gFTManagerModels, cleared by ftManagerReleaseFilesAll",
    "sFTManagerKeepKinds": "goes with gFTManagerModels, written only by ftManagerKeepFilesForScene",
    # and the same argument one more time: the clip buffers
    # a scene's fighters hold are main RAM, and this array is the only
    # thing that names them. It is emptied by ftManagerReleaseVClipAll,
    # the heap reset hook that frees them, and the overlay load a scene's
    # start runs comes after that -- so clearing it here could only ever
    # hide a buffer the reset had not reached, which is a leak rather
    # than the stale pointer the check is looking for.
    "sFTManagerVClipOwned": "the fighters' clip buffers, freed by ftManagerReleaseVClipAll",
    # And the instance buffer beside it, for the same reason. It was
    # cleared here until 2026-09-22; the overlay load runs before the heap
    # reset's hook at a scene change, so the hook freed NULL and every
    # fighter scene leaked its buffer (sizeof(Fighter) a slot), which
    # split the heap until a 600 KB pack no longer fitted.
    "sFTManagerModelsAllocBuf": "the fighters' instance buffer, freed by ftManagerReleaseVClipAll",
    "gGRStages": "the port's map-file table, held across scenes",
    "sGRStageRefs": "goes with gGRStages, cleared by grStageRelease",
}


def bss_symbols(obj):
    out = subprocess.run([NM, "-S", "--defined-only", str(obj)],
                         capture_output=True, text=True, check=True).stdout
    syms = []
    for line in out.splitlines():
        f = line.split()
        if len(f) == 4 and f[2] in ("b", "B"):
            name = f[3].lstrip("_")
            if "." in name:            # a function-local static
                continue
            syms.append(name)
    return syms


def elf_symbols():
    """name -> address, with the SH toolchain's leading underscore off.

    bss_symbols() strips leading underscores the same way, so a name
    looked up here is the name that came out of an object file. It costs
    one more underscore on ld's own __start_/__stop_ pair, which arrive
    as ___start_ovlN_noload; they are looked up below without it.
    """
    out = subprocess.run([NM, str(ELF)], capture_output=True, text=True,
                         check=True).stdout
    syms = {}
    for line in out.splitlines():
        f = line.split()
        if len(f) == 3:
            syms.setdefault(f[2].lstrip("_"), int(f[0], 16))
    return syms


def yaml_overlays():
    """Which files each overlay holds, from the decomp's own segment list.

    Every subsegment names a file without its extension, whichever of
    text, .data, .rodata or .bss it is; the union is what the overlay
    holds, which is what a claim here has to be in.
    """
    out = {}
    cur = None
    for line in YAML.read_text().splitlines():
        m = re.match(r"\s+- name: ovl(\d+)\s*$", line)
        if m:
            cur = int(m.group(1))
            continue
        if re.match(r"\s+- name: \w", line):       # a non-overlay segment
            cur = None
            continue
        if cur is None:
            continue
        m = (re.match(r"\s+- \[0x[0-9A-Fa-f]+, [\w.]+, (\S+)\]", line) or
             re.match(r"\s+- \{ start: \S+, type: \S+, name: (\S+) \}", line))
        if m:
            out.setdefault(cur, set()).add(m.group(1))
    return out


def makefile_renames():
    """object -> overlay, as the game's Makefile actually builds it."""
    text = MAKEFILE.read_text()
    out = {}
    for m in re.finditer(r"^(\w+\.o):.*?(?=^\w|\Z)", text, re.S | re.M):
        rename = re.search(r"\$\(call ovl-noload,(\d+),", m.group(0))
        if rename:
            out[m.group(1)] = int(rename.group(1))
    return out


def main():
    failures = []

    if not YAML.exists():
        return fail(["no decomp at %s -- set SSB_DECOMP_DIR" % DECOMP])
    if not ELF.exists():
        return fail(["no %s -- build ssb64 first" % ELF])

    # 1. every claim about which overlay a file is in, against the ROM
    ovl = yaml_overlays()
    claims = [(c, o, d) for c, (o, ds) in PORT_FILES.items() for d in ds]
    claims += [(o_, ov, d) for o_, (ov, d) in DECOMP_OBJS.items()]
    claims += [(o_, ov, d) for o_, (ov, d) in DECOMP_OBJS_NO_STATE.items()]
    for who, index, decomp in claims:
        if decomp not in ovl.get(index, ()):
            where = [k for k, v in ovl.items() if decomp in v]
            failures.append(
                "%s is mapped to overlay %d for %s, which the ROM puts in "
                "%s" % (who, index, decomp,
                        ("overlay %d" % where[0]) if where
                        else "no overlay at all"))

    # 2. every port file's reload, against the statics its object emits
    reloads = {}
    for src in sorted(SRC.glob("*.c")):
        m = re.search(r"^void (\w+OverlayLoad)\(void\)\n\{\n(.*?)^\}",
                      src.read_text(), re.S | re.M)
        if m is not None:
            reloads[src.name] = m

    for name in sorted(PORT_FILES):
        src = SRC / name
        m = reloads.get(name)
        if m is None:
            failures.append(
                "%s stands in for a file in overlay %d and has no "
                "xxxOverlayLoad" % (name, PORT_FILES[name][0]))
            continue
        text = src.read_text()
        cleared = set(re.findall(r"OVERLAY_CLEAR\((\w+)\)", m.group(2)))
        for sym in cleared:
            if not re.search(r"\b" + re.escape(sym) + r"\b", text[:m.start()]):
                failures.append(
                    "%s: %s clears %s, which this file does not declare"
                    % (name, m.group(1), sym))
        obj = OBJDIR / (src.stem + ".o")
        if not obj.exists():
            failures.append("%s: no %s -- build ssb64 first" % (name, obj))
            continue
        for sym in bss_symbols(obj):
            if sym in EXCLUDE or sym in cleared:
                continue
            failures.append(
                "%s: %s is in the module's noload segment and %s does not "
                "clear it" % (name, sym, m.group(1)))

    for name in sorted(set(reloads) - set(PORT_FILES)):
        failures.append(
            "%s has an xxxOverlayLoad and is not in this tool's map, so "
            "nothing checks which overlay it is in" % name)

    # 2b. every port-only scene: no reload of its own, and none in its arm
    scmanager = (SRC / "scmanager.c").read_text()
    # the arms, not sc_scene_name's case labels above them
    scmanager = scmanager[scmanager.find("\nvoid scManagerRunScene(void)"):]
    for name, label in sorted(PORT_ONLY.items()):
        if name in reloads:
            failures.append(
                "%s is a port-only scene and has %s: it has no overlay, so "
                "nothing is entitled to clear it" % (name, reloads[name].group(1)))
        if name in PORT_FILES:
            failures.append("%s is in both PORT_FILES and PORT_ONLY" % name)
        m = re.search(r"case %s:(.*?)break;" % re.escape(label), scmanager, re.S)
        if m is None:
            failures.append("scmanager.c has no arm for %s (%s)" % (label, name))
        elif "syDmaLoadOverlay" in re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S):
            failures.append(
                "scmanager.c's %s arm loads an overlay: a port-only scene "
                "runs between two of the game's and must not clear one"
                % label)

    # 3. every decomp object, against the Makefile and the linked ELF
    renamed = makefile_renames()
    syms = elf_symbols()
    for obj_name in sorted(DECOMP_OBJS):
        index = DECOMP_OBJS[obj_name][0]
        if renamed.get(obj_name) != index:
            failures.append(
                "%s belongs to overlay %d and the Makefile renames its .bss "
                "into %s" % (obj_name, index,
                             ("overlay %d's section" % renamed[obj_name])
                             if obj_name in renamed else "no section"))
            continue
        lo = syms.get("start_ovl%d_noload" % index)
        hi = syms.get("stop_ovl%d_noload" % index)
        if lo is None or hi is None:
            failures.append(
                "%s: the linked ELF has no ovl%d_noload section, so "
                "syDmaLoadOverlay(%d) bzeroes nothing"
                % (obj_name, index, index))
            continue
        obj = OBJDIR / obj_name
        if not obj.exists():
            failures.append("no %s -- build ssb64 first" % obj)
            continue
        for sym in bss_symbols(obj):
            addr = syms.get(sym)
            if addr is None:      # dropped by --gc-sections, so unreachable
                continue
            if not lo <= addr < hi:
                failures.append(
                    "%s: %s is at %#x, outside overlay %d's noload segment "
                    "%#x..%#x" % (obj_name, sym, addr, index, lo, hi))

    # 4. and the segments are inside the range KOS zeroes at boot, so an
    # overlay starts at zero the first time as well as the rest
    boot_lo, boot_hi = syms.get("bss_start"), syms.get("end")
    for index in sorted({i for i, _ in DECOMP_OBJS.values()}):
        lo = syms.get("start_ovl%d_noload" % index)
        if lo is None or boot_lo is None or boot_hi is None:
            continue
        if not boot_lo <= lo < boot_hi:
            failures.append(
                "overlay %d's noload segment starts at %#x, outside the "
                "%#x..%#x KOS zeroes at boot" % (index, lo, boot_lo, boot_hi))

    # 5. and a decomp object that takes no rename because it keeps
    # nothing really keeps nothing.
    for obj_name, (index, decomp) in sorted(DECOMP_OBJS_NO_STATE.items()):
        if obj_name in renamed:
            failures.append(
                "%s is mapped as holding no state and the Makefile renames "
                "its .bss into overlay %d's section anyway"
                % (obj_name, renamed[obj_name]))
        kept = bss_symbols(OBJDIR / obj_name)
        if kept:
            failures.append(
                "%s (%s, overlay %d) keeps %s, and nothing clears it: give "
                "it the $(call ovl-noload,%d) treatment and move it to "
                "DECOMP_OBJS"
                % (obj_name, decomp, index, ", ".join(sorted(kept)), index))

    if failures:
        return fail(failures)

    ports = len(PORT_FILES)
    objs = len(DECOMP_OBJS)
    overlays = len({o for o, _ in PORT_FILES.values()} |
                   {o for o, _ in DECOMP_OBJS.values()} |
                   {o for o, _ in DECOMP_OBJS_NO_STATE.values()})
    print("overlay_check: %d overlays; %d port modules clear every static "
          "they keep, %d decomp objects are in a segment the reload "
          "bzeroes, %d keeps nothing to clear, and %d port-only scene "
          "loads none"
          % (overlays, ports, objs, len(DECOMP_OBJS_NO_STATE),
             len(PORT_ONLY)))
    return 0


def fail(failures):
    for f in failures:
        print("overlay_check: " + f, file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
