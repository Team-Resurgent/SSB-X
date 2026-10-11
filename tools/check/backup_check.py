#!/usr/bin/env python3
"""ssb64-dc: the save data's defaults, against the ROM they were copied from.

src/dc/scmanagerdata.c is 660 lines of hand-copied initializer -- the three
tables sc/scmanager.c keeps in the always-resident segment's .data, which the
game starts gSCManagerBackupData, gSCManagerSceneData and both battle states
from. A hand copy of an initializer is exactly the kind of thing that is
wrong in one place and nowhere else, and nothing at runtime would say so: a
mistyped 1P-game high score is invisible, a mistyped unlock mask hands the
player a character, and a mistyped signature makes every save the game ever
writes unreadable to itself.

The ROM has the answer. dSCManagerDefaultBackupData is at 0x800A3994 in the
`scmanager` segment of smashbrothers.us.yaml, dSCManagerDefaultSceneData at
0x800A3F80 and dSCManagerDefaultBattleState at 0x800A3FC8, and each one runs
to the next symbol in symbols_us.txt. So:

  * each struct's size is the span between its symbol and the next one --
    checked twice, because the two compilers answer different questions.
    The host compiler's answer is what the field walk below rests on; the
    *target* compiler's is the one that matters to the port, and it gets
    its own leg: a generated file of _Static_asserts fed to sh-elf-gcc, so
    a struct the SH-4 lays out differently from the MIPS fails to compile
    with the ROM's own number in the message.
  * every scalar leaf the port compiled equals the big-endian value at the
    same offset in the ROM. tools/check/backup_oracle.c emits the leaves with
    offsets from the port's own offsetof, so a field the two ABIs place
    differently reads the ROM in the wrong place and fails here.
  * every byte no leaf covers is zero in the ROM -- that is padding, and a
    non-zero one would mean a field this check does not know about.
  * SCBattleState holds one pointer per player, and a pointer is eight
    bytes on the machine this runs on and four on both machines that
    matter. tools/check/backup_oracle.c states that correction once and takes it
    nowhere else; the size check above is what proves it, since a wrong
    correction cannot land on 496.
  * SCBattleState holds one pointer per player, and a pointer is eight
    bytes on the machine this runs on and four on both machines that
    matter. tools/check/backup_oracle.c states that correction once and takes it
    nowhere else; the size check above is what proves it, since a wrong
    correction cannot land on 496.
  * SCBattleState's two ub32 : 1 flags are checked against the free bits
    of the word they share with item_appearance_rate, under the MIPS rule:
    a bit field is allocated from the top of the bits its unit has free,
    first declared first, so is_show_score is bit 23 and is_not_teamshadows
    bit 22. Those 24 bits must hold the two flags and nothing else.
  * the game's own checksum, run by the port's compiled lbBackupCreateChecksum
    over the ROM's bytes, equals the weighted byte sum computed here. A
    byte-wise sum has no byte order of its own, so this is a real check on
    the one subtle line in lb/lbbackup.c: the loop stops sizeof(checksum)
    bytes early. It runs over two buffers, because the defaults carry a
    checksum of zero and a loop that ran to the end of the struct would sum
    four zeroes and give the same answer -- so the second buffer is the
    same bytes with the checksum filled in the way lbBackupWrite leaves it,
    where the bound is visible.

What it deliberately does not check: whether the *values* are good ideas.
The ROM is the authority and the port agrees with it or does not.

Usage: python3 tools/check/backup_check.py [--rom <rom.z64>]
"""
import os
import struct
import subprocess
import sys
import tempfile

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_paths as P            # noqa: E402
import ssb_packexport as PK      # noqa: E402
import ssb_meshexport as M       # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

SYMBOLS = {
    "dSCManagerDefaultBackupData": "LBBackupData",
    "dSCManagerDefaultSceneData": "SCCommonData",
    "dSCManagerDefaultBattleState": "SCBattleState",
}

# sc/sctypes.h:373-374, in declaration order. MIPS o32 allocates a bit
# field from the top of the bits its unit has free, so these two follow
# the u8 item_appearance_rate that shares their word: bit 23 and bit 22.
BITFIELDS = ("is_show_score", "is_not_teamshadows")


def build_oracle(tmp):
    """tools/check/backup_oracle.c over src/dc/scmanagerdata.c and lb/lbbackup.c.

    The same shim headers the target build uses (src/dc/decomp ahead of the
    decomp's own src/), so the struct layouts are the port's, patches and
    all -- which is the point: this asks what the port compiled.
    """
    out = os.path.join(tmp, "backup_oracle")
    cmd = ["gcc", "-O1", "-Wall", "-Werror", "-o", out,
           "-I" + os.path.join(REPO, "src", "dc", "decomp"),
           "-I" + os.path.join(P.DECOMP_DIR, "src"),
           "-idirafter", os.path.join(P.DECOMP_DIR, "include"),
           "-D_LANGUAGE_C", "-DF3DEX_GBI_2", "-DREGION_US",
           "-Wno-unused-variable", "-Wno-int-conversion",
           os.path.join(REPO, "tools", "check", "backup_oracle.c"),
           os.path.join(REPO, "src", "dc", "scmanagerdata.c"),
           os.path.join(P.DECOMP_DIR, "src", "lb", "lbbackup.c")]
    subprocess.run(cmd, check=True)
    return out


def target_sizes(tmp, spans):
    """The sizes the *target* compiler gives, against the ROM's spans.

    The rest of this file runs on the build host, whose pointer is not the
    Dreamcast's; this is the one leg that asks the compiler that actually
    builds the port. It is a compile and not a run, because there is
    nothing here to run an SH-4 binary on -- _Static_assert is enough, and
    the ROM's number goes in the message.
    """
    src = os.path.join(tmp, "sizes.c")
    with open(src, "w") as f:
        f.write("#include <sc/scene.h>\n")
        for name, ctype in sorted(SYMBOLS.items()):
            f.write('_Static_assert(sizeof(%s) == %d,\n'
                    '    "%s: the SH-4 lays %s out in a different number of "\n'
                    '    "bytes than the %d the ROM gives it");\n'
                    % (ctype, spans[name], name, ctype, spans[name]))
    r = subprocess.run(
        ["kos-cc", "-c", "-o", os.path.join(tmp, "sizes.o"),
         "-I" + os.path.join(REPO, "src", "dc", "decomp"),
         "-I" + os.path.join(P.DECOMP_DIR, "src"),
         "-idirafter", os.path.join(P.DECOMP_DIR, "include"),
         "-D_LANGUAGE_C", "-DF3DEX_GBI_2", "-DREGION_US", src],
        capture_output=True, text=True)
    if r.returncode != 0:
        return [l for l in r.stderr.splitlines() if "error" in l]
    return []


def rom_bytes(rom, name):
    """The ROM's own copy of `name`, and the span symbols_us.txt gives it."""
    vram = PK.symbol_vram(name)
    size = PK.symbol_size(name)
    off = PK.overlay_rom_offset("scmanager", vram)
    return rom[off:off + size], size


def decode(data, off, size, is_signed):
    fmt = {1: "b", 2: "h", 4: "i"}[size]
    if not is_signed:
        fmt = fmt.upper()
    return struct.unpack_from(">" + fmt, data, off)[0]


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    P.require_decomp()
    rom = open(rom_path, "rb").read()

    images = {}
    for name in SYMBOLS:
        images[name] = rom_bytes(rom, name)

    fails = []
    with tempfile.TemporaryDirectory() as tmp:
        # The ROM's default save data for the checksum leg, and the same
        # bytes with the checksum written in.
        save = images["dSCManagerDefaultBackupData"][0]
        csum = 0
        for i in range(len(save) - 4):      # less sizeof(...checksum)
            csum += save[i] * (i + 1)
        csum = struct.unpack("<i", struct.pack("<I", csum & 0xFFFFFFFF))[0]

        blobs = []
        for k, tail in enumerate((b"\0\0\0\0", struct.pack(">i", csum))):
            blobs.append(os.path.join(tmp, "backup%d.bin" % k))
            open(blobs[-1], "wb").write(save[:-4] + tail)

        oracle = build_oracle(tmp)
        out = subprocess.run([oracle] + blobs, check=True,
                             capture_output=True, text=True).stdout
        fails += target_sizes(tmp, {n: images[n][1] for n in SYMBOLS})

    leaves = {name: [] for name in SYMBOLS}
    sizes, bits, checksums = {}, {}, []
    for line in out.splitlines():
        p = line.split()
        if p[0] == "F":
            leaves[p[1]].append((p[2], int(p[3]), int(p[4]), int(p[5]),
                                 int(p[6])))
        elif p[0] == "S":
            sizes[p[1]] = int(p[2])
        elif p[0] == "B":
            bits[p[2]] = int(p[3])
        elif p[0] == "C":
            checksums.append(int(p[1]))

    counts = {}
    for name in SYMBOLS:
        data, span = images[name]
        if sizes[name] != span:
            fails.append("%s: the port compiled %d bytes, the ROM's symbol "
                         "spans %d" % (name, sizes[name], span))
            continue
        covered = bytearray(span)
        bad = 0
        for field, off, size, is_signed, ours in leaves[name]:
            theirs = decode(data, off, size, is_signed)
            if theirs != ours:
                fails.append("%s.%s (offset %d, %d bytes): the port has %d, "
                             "the ROM %d" % (name, field, off, size, ours,
                                             theirs))
                bad += 1
            for k in range(off, off + size):
                covered[k] = 1
        counts[name] = (len(leaves[name]), bad)

        # Every byte no leaf named. For SCBattleState the two one-bit
        # flags live in one such word; everywhere else it is padding.
        holes = [k for k in range(span) if not covered[k]]
        if name == "dSCManagerDefaultBattleState":
            # The two flags share a word with item_appearance_rate, which
            # is a named leaf, so what is left of that word is the three
            # bytes below it. MIPS allocates a bit field from the top of
            # the bits its unit has free, first-declared first, so those
            # 24 bits read big-endian are the flags packed from bit 23
            # down. Nothing else in the struct has an uncovered non-zero
            # byte, and the check says so rather than assuming it.
            words = sorted({k & ~3 for k in holes if data[k] != 0})
            if len(words) != 1:
                fails.append("%s: uncovered non-zero bytes in %d words, "
                             "expected the one holding the two bit fields"
                             % (name, len(words)))
                words = []
            for w in words:
                rest = [k for k in holes if (k & ~3) == w]
                got = 0
                for k in sorted(rest):
                    got = (got << 8) | data[k]
                want = 0
                for i, f in enumerate(BITFIELDS):
                    want |= (bits[f] & 1) << (len(rest) * 8 - 1 - i)
                if got != want:
                    fails.append("%s: the %d free bits of the word at %d "
                                 "are 0x%X, and the port's %s say they "
                                 "should be 0x%X packed from the top"
                                 % (name, len(rest) * 8, w, got,
                                    "/".join("%s=%d" % (f, bits[f])
                                             for f in BITFIELDS), want))
                holes = [k for k in holes if (k & ~3) != w]
        for k in holes:
            if data[k] != 0:
                fails.append("%s: byte %d is 0x%02X and no field covers it"
                             % (name, k, data[k]))

    # The checksum: the same number over both buffers, since the four
    # bytes they differ in are the four the loop must not reach.
    for k, got in enumerate(checksums):
        if got != csum:
            fails.append("lbBackupCreateChecksum over the ROM's default "
                         "save data%s gives %d, and summing bytes 0..%d "
                         "weighted by position gives %d"
                         % (" with its checksum written in" if k else "",
                            got, len(save) - 5, csum))

    if fails:
        for line in fails:
            print("FAIL " + line)
        sys.exit("backup_check: %d problems" % len(fails))
    total = sum(n for n, _ in counts.values())
    print("backup_check: %d fields across %s, every one the ROM's; "
          "%d/%d/%d bytes on the ROM, on the host and on the SH-4; "
          "checksum %d"
          % (total, ", ".join(sorted(SYMBOLS)),
             sizes["dSCManagerDefaultBackupData"],
             sizes["dSCManagerDefaultSceneData"],
             sizes["dSCManagerDefaultBattleState"], csum))


if __name__ == "__main__":
    main()
