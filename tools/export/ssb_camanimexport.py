#!/usr/bin/env python3
"""ssb64-dc: the game's camera-animation scripts, out of relocData and into
a bank the port loads (src/dc/camanim.c).

A CamAnimJoint is one AObjEvent32 script -- the same 32-bit animation
language sys/objanim.c's gcParseDObjAnimJoint reads for a DObj -- but
registered on a CObj instead, where gcParseCObjCamAnimJoint drives ten
camera tracks (nGCAnimTrackEyeX..nGCAnimTrackFovY, objdef.h:248-259)
rather than a joint's transform. The scene says which parser a script
belongs to; nothing in the script itself does.

The game reaches one the way it reaches everything else in a relocData
file: `lbRelocGetFileData(AObjEvent32*, sFiles[n], &ll<Name>CamAnimJoint)`
-- the loaded file's base plus a linker offset. The port has no such
address space, so this tool cuts each named script out of its file and
writes it to a bank the scene loads by name.

    python3 tools/export/ssb_camanimexport.py --bank OpeningRoom \\
        --out romdisk/mvopeningroom.cam [--rom <rom.z64>]
    python3 tools/export/ssb_camanimexport.py --bank OpeningRoom --list

WHY A COPY IS NOT ENOUGH, AND WHAT THIS TOOL DOES ABOUT IT.

An AnimJoint script's Jump/SetAnim/SetInterp opcodes are followed by a
POINTER word, and in the ROM that word is an absolute N64 address the
file's intern reloc chain patched into place. Lift the words out by
memcpy alone and those pointers still name the file they came from: the
parser follows one into whatever now lives there, never finds an End,
and spins. That does not crash -- it hangs a frame loop, which presents
as a black screen (src/dc/stage.c:230-257 carries the same note for the
stage packs' map AnimJoints, and it is why they are written the way they
are).

So the bank is emitted the way a stage's map anims and a fighter pack's
AnimJoint animations already are: the script's words verbatim, plus a
relocation list naming which of them hold a pointer. Each listed word
carries its target's WORD INDEX in the bank, and camanim_bank_load turns
it into an address once the bank is in RAM. A pointer whose target is
outside the script is a hard refusal here rather than a silent hang
there.

None of the thirteen scripts this tool ships today has a pointer word at
all (all thirteen are linear: no Jump, no SetAnim, no SetInterp), so the
list comes out empty for every one of them -- but it is the walk that
establishes that, not an assumption, and ssb_assets.animjoint_walk
refuses an unrelocated pointer rather than stepping over it.
"""
import os
import re
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract           # noqa: E402
import ssb_assets as A       # noqa: E402
import ssb_paths as P        # noqa: E402

MAGIC = b"SSBCAM1\0"
NAME_LEN = 32                   # CAMANIM_NAME_LEN, src/dc/camanim.h
HEADER_FMT = "<8s6I"            # magic, count, nwords, nreloc, 3 offsets
DIR_FMT = "<%dsII" % NAME_LEN

# objdef.h:248-259: nGCAnimTrackCameraStart(25)..nGCAnimTrackFovY(34).
# gcParseCObjCamAnimJoint's track_aobjs array is exactly this long and its
# flag loop breaks at the end of it, so a script that set a bit above bit 9
# would leave the parser's own pc mis-positioned -- the game would already
# be broken. Checked here because the shared walker counts every set bit.
CAMERA_TRACKS = 10

# Which named scripts each bank carries: (script name, relocData file id,
# byte offset in that file). The offsets are the values the decomp's own
# ll<Name>CamAnimJoint link labels carry, as src/dc/decomp/reloc_data.us.h
# prints them; the file ids are that header's ll<Name>FileID values.
#
# The names are the port's, and they are what a scene passes camanim_get:
# the decomp's label minus its ll-prefix, its file prefix and its
# CamAnimJoint suffix.
BANKS = {
    # llMVOpeningRoomScene1..4FileID = 0x38..0x3b. One camera script per
    # file and nothing else in it -- file 57 is eighty bytes whole.
    # src/dc/mvopeningroom.c: scenes 1 and 2 have no framing of their own
    # in the C source at all, so without these two the shot sat at
    # gcAddXObjForCamera's default look-at.
    "OpeningRoom": ("romdisk/mvopeningroom.cam", [
        ("Scene1", 56, 0x0),
        ("Scene2", 57, 0x0),
        ("Scene3", 58, 0x0),
        ("Scene4", 59, 0x0),
    ]),
    # llMVOpeningCommonFileID = 0x41. The eight per-fighter opening
    # scenes (mv/mvopening/mvopening{mario,donkey,samus,fox,link,yoshi,
    # pikachu,kirby}.c) each take their camera out of this one shared
    # file -- dMVOpening<F>FileIDs is {IFCommonAnnounceCommon,
    # MVOpeningCommon} for all eight, and each reads
    # sMVOpening<F>Files[1] + &llMVOpeningCommon<F>CamAnimJoint. So one
    # export serves all eight, and it is done now rather than eight
    # times later. In file order, which is also nFTKind order for these
    # eight (objdef/ftdef): Mario, Donkey, Samus, Fox, Link, Yoshi,
    # Pikachu, Kirby.
    "OpeningCommon": ("romdisk/mvopeningcommon.cam", [
        ("Mario", 65, 0x0),
        ("Donkey", 65, 0x30),
        ("Samus", 65, 0x60),
        ("Fox", 65, 0x90),
        ("Link", 65, 0xC0),
        ("Yoshi", 65, 0xF0),
        ("Pikachu", 65, 0x120),
        ("Kirby", 65, 0x150),
    ]),
    # mvopeningrun.c: llMVOpeningRunFileID = 0x37 holds one DObj script
    # per fighter (the proxy DObj mvOpeningRunMakeFighters flies each
    # fighter along, gcAddDObjAnimJoint), llMVOpeningRunMainFileID = 0x3c
    # the camera's. The DObj scripts set the ordinary joint tracks, 24 of
    # them at most (nGCAnimTrackRotStart..nGCAnimTrackScaleEnd, below the
    # camera's 25..34).
    "OpeningRun": ("romdisk/mvopeningrun.cam", [
        (n, 55, o, "llMVOpeningRun%sAnimJoint" % n, 25)
        for n, o in (("Mario", 0x4), ("Fox", 0xB4), ("Donkey", 0x124),
                     ("Samus", 0x184), ("Link", 0x224), ("Yoshi", 0x334),
                     ("Kirby", 0x3A4), ("Pikachu", 0x484))
    ] + [("MainCam", 60, 0x0, "llMVOpeningRunMainCamAnimJoint")]),
    # mvopeningcliff.c: llMVOpeningCliffFileID = 0x44 (68) holds the
    # camera's script beside the hills' and ocarina's models.
    "OpeningCliff": ("romdisk/mvopeningcliff.cam", [
        ("Cam", 68, 0x8910, "llMVOpeningCliffCamAnimJoint"),
    ]),
    # mvopeningyamabuki.c: llMVOpeningYamabukiFileID = 0x47 (71) holds the
    # camera's script beside the wallpaper and the three models.
    "OpeningYamabuki": ("romdisk/mvopeningyamabuki.cam", [
        ("Cam", 71, 0xD330, "llMVOpeningYamabukiCamAnimJoint"),
    ]),
    # mvopeningjungle.c: llMVOpeningJungleFileID = 0x40 (64) holds only the
    # stage camera's script, at offset 0.
    "OpeningJungle": ("romdisk/mvopeningjungle.cam", [
        ("Cam", 64, 0x0),
    ]),
    # mvopeningyoster.c: llMVOpeningYosterFileID = 0x43 (67) holds the
    # camera's script beside the nest and ground models.
    "OpeningYoster": ("romdisk/mvopeningyoster.cam", [
        ("Cam", 67, 0xC940),
    ]),
    # mvopeningsector.c: llMVOpeningSectorFileID = 0x49 (73) holds the
    # camera's script beside the Great Fox, the cockpit and the arwings'
    # tables.
    "OpeningSector": ("romdisk/mvopeningsector.cam", [
        ("Cam", 73, 0xF9A0),
    ]),
    # mvopeningstandoff.c: llMVOpeningStandoffFileID = 0x45 (69), whose
    # llMVOpeningStandoffCamAnimJoint is the camera's script.
    "OpeningStandoff": ("romdisk/mvopeningstandoff.cam", [
        ("Cam", 69, 0x7250),
    ]),
    # mvopeningclash.c: llMVOpeningClashFightersFileID = 0x48 (72) holds
    # the fighters' camera, llMVOpeningClashWallpaperFileID = 0x42 (66) the
    # wallpaper's.
    "OpeningClash": ("romdisk/mvopeningclash.cam", [
        ("Fighters", 72, 0x1440),
        ("Wall", 66, 0x4AB0),
    ]),
    # llMVEndingFileID = 0x4c. mvEndingSetupOperatorCamera's own path,
    # shared by both of the ending diorama's main cameras.
    "Ending": ("romdisk/mvending.cam", [
        ("Operator", 76, 0x0),
    ]),
    # The boss rung's two camera scripts (sc/sc1pmode/sc1pgame.c): the
    # approach (sc1PGameWaitStageBossUpdate, D_NF_00006010) and the
    # defeat zoom (sc1PGameBossDefeatInterfaceProcSet, D_NF_00006450).
    # The game reaches both as `gr_desc[1].dobjdesc - llGRLastMapFileHead
    # + D_NF_...`, which is reloc file 114's base plus the offset -- the
    # same file_head sc1pgameboss.c's own tables resolve against (see
    # tools/export/ssb_stageexport.py's boss block).
    "Boss": ("romdisk/sc1pgameboss.cam", [
        ("Approach", 114, 0x6010),
        ("Defeat", 114, 0x6450),
    ]),
    # llSC1PIntroFileID = 0xb. The 1P game's "<player> VS <opponent>"
    # card (sc/sc1pmode/sc1pintro.c) is the one scene that
    # keeps TWO sets of camera scripts in one file and picks between
    # them by different indices: sc1PIntroMakeStageCamera indexes the
    # first fourteen by nSC1PGameStage (the ladder's rung), and
    # sc1PIntroMakeFighterCamera indexes the next twelve by nFTKind (the
    # human's fighter). Both sets live in file 11 beside the card's own
    # sprites, which is why the sprite bank and this one name the same
    # file.
    #
    # The four zero rows of the stage table (Bonus 1, 2, 3 and the
    # fourteenth) have no script and no camera: sc1PIntroFuncStart calls
    # neither camera maker on a bonus rung, so nothing ever looks them
    # up, and there is nothing in the file to export for them.
    "SC1PIntro": ("romdisk/sc1pintro.cam", [
        (n, 11, o, "llSC1PIntro%sCamAnimJoint" % n)
        for n, o in (
            # sc1PIntroMakeStageCamera, in nSC1PGameStage order
            ("StageLink", 0x6FE0), ("StageYoshi", 0x6EF0),
            ("StageFox", 0x6F80), ("StageMario", 0x7040),
            ("StagePikachu", 0x6FB0), ("StageDonkey", 0x7010),
            ("StageKirby", 0x6EC0), ("StageSamus", 0x6F50),
            ("StageMMario", 0x7070), ("StageZako", 0x70A0),
            ("StageBoss", 0x6F20),
            # sc1PIntroMakeFighterCamera, in nFTKind order
            ("FighterMario", 0x6C80), ("FighterFox", 0x6CB0),
            ("FighterDonkey", 0x6CE0), ("FighterSamus", 0x6D10),
            ("FighterLuigi", 0x6D40), ("FighterLink", 0x6D70),
            ("FighterYoshi", 0x6DA0), ("FighterCaptain", 0x6DD0),
            ("FighterKirby", 0x6E00), ("FighterPikachu", 0x6E30),
            ("FighterPurin", 0x6E60), ("FighterNess", 0x6E90),
        )
    ]),
}


def default_rom():
    for p in ("baserom.z64",
              "../../../baserom.z64"):
        if os.path.exists(p):
            return p
    sys.exit("no base ROM found; pass --rom")


def label_offset(label):
    """The offset src/dc/decomp/reloc_data.us.h prints for a link label.

    The BANKS table above repeats these so the tool reads on its own, and
    this is what checks the repetition: a header regenerated against a
    different ROM revision would move a label and the mismatch is caught
    here rather than by a camera that quietly films the wrong thing.
    """
    hdr = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "..", "..", "src", "dc", "decomp", "reloc_data.us.h")
    m = re.search(r"^extern int %s; // (0x[0-9a-fA-F]+)$" % re.escape(label),
                  open(hdr).read(), re.M)
    return None if m is None else int(m.group(1), 16)


def cut_script(rom, fid, off, label, tracks=CAMERA_TRACKS):
    """One camera script's words and its pointer words, out of file `fid`.

    Returns (words, ptr_word_indices, reloc_targets) with the words as
    big-endian u32 already byte-swapped to host order, indices relative to
    the script's own first word, and reloc_targets the same indices' target
    word indices.
    """
    f = A.get_file(rom, fid, ssb_extract)
    entry = ssb_extract.read_entry(rom, ssb_extract.RELOC_SEG, fid)
    reloc = A.walk_reloc(f, entry["reloc_intern"])

    # The shared AObjEvent32 walk (tools/lib/ssb_assets.py). It refuses an
    # undefined opcode, a read past the file and -- the case that matters
    # here -- a Jump/SetAnim/SetInterp whose pointer word the file's reloc
    # chain never patched, which is precisely the word a memcpy would
    # carry over as an absolute N64 address.
    seen, floats = A.animjoint_walk(f, reloc, off)
    words = seen | floats
    lo, hi = min(words), max(words) + 4
    if lo != off:
        raise SystemExit("%s: walk started at 0x%X but reaches back to 0x%X"
                         % (label, off, lo))
    gaps = [w for w in range(lo, hi, 4) if w not in words]
    if gaps:
        raise SystemExit("%s: script is not contiguous; %d unread words "
                         "(first 0x%X)" % (label, len(gaps), gaps[0]))

    # Every set flag bit must name one of the ten camera tracks.
    for w in sorted(seen):
        (_op, flags, _p) = A._aj_decode(struct.unpack_from(">I", f, w)[0])
        if flags >> tracks:
            raise SystemExit("%s: command at 0x%X sets track flag bit %d, "
                             "past the %d tracks"
                             % (label, w, flags.bit_length() - 1, tracks))

    n = (hi - lo) // 4
    out = list(struct.unpack_from(">%dI" % n, f, lo))
    ptrs, targets = [], []
    for p in sorted(o for o in seen if o in reloc):
        t = reloc[p]
        if not (lo <= t < hi):
            raise SystemExit("%s: pointer at 0x%X targets 0x%X, outside the "
                             "script (0x%X..0x%X)" % (label, p, t, lo, hi))
        ptrs.append((p - lo) // 4)
        targets.append((t - lo) // 4)
    return out, ptrs, targets


def build(rom, entries, verbose=True):
    words, dirent, relocs = [], [], []
    for ent in entries:
        name, fid, off = ent[:3]
        # a 5-tuple names its own link label and track count: the DObj
        # scripts of mvopeningrun.c are not camera scripts (gcAddDObjAnimJoint
        # feeds them the rotate/translate/scale tracks)
        label = ent[3] if len(ent) > 3 else \
            "ll%sCamAnimJoint" % _label_stem(name, fid)
        tracks = ent[4] if len(ent) > 4 else CAMERA_TRACKS
        want = label_offset(label)
        if want is not None and want != off:
            raise SystemExit("%s: reloc_data.us.h says 0x%X, BANKS says 0x%X"
                             % (label, want, off))
        w, ptrs, targets = cut_script(rom, fid, off, label, tracks)
        base = len(words)
        for i, p in enumerate(ptrs):
            # the stored value is the target's word index in the BANK;
            # camanim_bank_load makes it an address (src/dc/camanim.c)
            w[p] = base + targets[i]
            relocs.append(base + p)
        if len(name.encode()) >= NAME_LEN:
            raise SystemExit("%s: name too long for CAMANIM_NAME_LEN" % name)
        dirent.append((name.encode(), base, len(w)))
        words.extend(w)
        if verbose:
            print("  %-10s file %3d @ 0x%04X  %4d words, %d pointer%s"
                  % (name, fid, off, len(w), len(ptrs),
                     "" if len(ptrs) == 1 else "s"))

    off_dir = struct.calcsize(HEADER_FMT)
    off_words = off_dir + struct.calcsize(DIR_FMT) * len(dirent)
    off_reloc = off_words + 4 * len(words)
    blob = struct.pack(HEADER_FMT, MAGIC, len(dirent), len(words),
                       len(relocs), off_dir, off_words, off_reloc)
    for nm, first, n in dirent:
        blob += struct.pack(DIR_FMT, nm, first, n)
    blob += struct.pack("<%dI" % len(words), *words)
    blob += struct.pack("<%dI" % len(relocs), *relocs)
    return blob


# The decomp's label stem for a (port name, file id) pair: the port's
# script names drop the file prefix, so it has to be put back to check the
# offset against reloc_data.us.h.
_STEMS = {56: "MVOpeningRoomScene1", 57: "MVOpeningRoomScene2",
          58: "MVOpeningRoomScene3", 59: "MVOpeningRoomScene4",
          65: "MVOpeningCommon%s", 76: "MVEndingOperator"}


def _label_stem(name, fid):
    stem = _STEMS.get(fid)
    if stem is None:
        return name
    return stem % name if "%s" in stem else stem


def main(argv):
    bank = out = None
    rom_path = None
    listing = False
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--bank":
            i += 1
            bank = argv[i]
        elif a == "--out":
            i += 1
            out = argv[i]
        elif a == "--rom":
            i += 1
            rom_path = argv[i]
        elif a == "--list":
            listing = True
        else:
            sys.exit("unknown argument %s" % a)
        i += 1
    if bank is None or bank not in BANKS:
        sys.exit("--bank must be one of: %s" % ", ".join(sorted(BANKS)))
    P.require_decomp()
    default_out, entries = BANKS[bank]
    rom = open(rom_path or default_rom(), "rb").read()

    print("camanim: %s" % bank)
    blob = build(rom, entries)
    if listing:
        return 0
    path = out or default_out
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(path, "wb") as fp:
        fp.write(blob)
    print("  -> %s, %d bytes" % (path, len(blob)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
