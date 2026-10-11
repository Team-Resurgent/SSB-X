#!/usr/bin/env python3
"""ssb64-dc: cut the nine particle banks out of the SSB64 US baserom.

Each bank is two ROM byte ranges -- a script bank (.scb) and a texture
bank (.txb) -- that the game DMAs whole into two syTaskmanMalloc blocks
and then pointerizes in place (lbParticleSetupBankID, lbparticle.c:182).
Nothing about that changes here: the file this writes has the ROM's own
layout, byte for byte and offset for offset, so every pointer the game's
own arithmetic makes out of it lands where it lands on the N64.

The one thing that does change is byte order. The ROM is
big-endian and the SH-4 is not, so every field the decomp declares as a
u16/u32/s32/f32 is swapped here, once, at export -- and nothing else is:

  - script bytecode stays exactly as the ROM has it, because that is what
    the decomp's own src/particles/*_scb.c holds -- a u8 array -- and
    tools/check/particle_check.py holds this file to those. It used to say here
    that the bytecode was byte-order-neutral because the readers take it a
    byte at a time. Half of that is true: lbParticleReadUShort composes
    its bytes by hand. lbParticleReadFloatBigEnd copies four of them into
    a u8[4] and reads them back as a f32, which is a big-endian float only
    on a big-endian machine, and the decomp's own comment above it says
    so. The port undoes the order there instead, where the read is and
    where a DIVERGES comment can sit next to it (src/dc/lbparticle.c);
    swapping it here would mean decoding every opcode to find which four
    bytes are a float, and would put this file out of step with the
    decomp's;
  - image texels and CI palettes are `u8` arrays in the decomp's own
    src/particles/*_txb.c, and the port's texture converter reads them
    the way the RDP would.

So the file this writes is, byte for byte, what a little-endian compiler
makes of the decomp's src/particles/<bank>_{scb,txb}.c -- which is what
tools/check/particle_check.py checks it against.

It also writes the linker fragment that gives the port the six bank
addresses the game's own code takes the address of -- see --emit-ld and
SYMBOLS below.

Usage:
  python3 tools/export/ssb_particleexport.py [--rom <rom.z64>] --bank <name> --kind scb --out <file>
  python3 tools/export/ssb_particleexport.py [--rom <rom.z64>] --all --outdir <dir>
  python3 tools/export/ssb_particleexport.py --emit-ld <file> [--symbol-prefix _]
"""
import argparse
import os
import struct
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROM_DEFAULT = os.path.join(REPO_ROOT, "baserom.z64")

# ROM ranges, US. The source of truth is the decomp's smashbrothers.us.yaml
# (the eighteen `particles/*_{scb,txb}` segments) and its
# symbols/linker_constants.txt, which is where the game's own
# l<Bank>ParticleScriptBankLo/Hi come from. tools/check/particle_check.py
# re-reads both and fails if this table has drifted from them.
BANKS = {
    "efcommon":       {"scb": (0x00AC7340, 0x00AC9DE0), "txb": (0x00AC9DE0, 0x00B16C80)},
    "particles_unk0": {"scb": (0x00B16C80, 0x00B17060), "txb": (0x00B17060, 0x00B174A0)},
    "particles_unk1": {"scb": (0x00B174A0, 0x00B176A0), "txb": (0x00B176A0, 0x00B19700)},
    "particles_unk2": {"scb": (0x00B19700, 0x00B19850), "txb": (0x00B19850, 0x00B1BCA0)},
    "itcommon":       {"scb": (0x00B1BCA0, 0x00B1BDE0), "txb": (0x00B1BDE0, 0x00B1E640)},
    "grpupupu":       {"scb": (0x00B1E640, 0x00B1E7E0), "txb": (0x00B1E7E0, 0x00B1F960)},
    "grhyrule":       {"scb": (0x00B1F960, 0x00B1FC80), "txb": (0x00B1FC80, 0x00B22980)},
    "gryoster":       {"scb": (0x00B22980, 0x00B22A00), "txb": (0x00B22A00, 0x00B22C30)},
    "mntitle":        {"scb": (0x00B22C30, 0x00B22D40), "txb": (0x00B22D40, 0x00B277B0)},
}

# G_IM_FMT_CI, the only format whose textures carry palettes after the
# images -- and so the only one whose data[] length is not implied by the
# gap in front of the first image (PARTICLE_BANK_DISCOVERIES.md; the same
# three cases as ssb-decomp-re/tools/extractParticleTextures.py).
FMT_CI = 2

# The six banks the game names. ssb-decomp-re/symbols/linker_constants.txt
# defines l<Name>ParticleScriptBankLo/Hi and l<Name>ParticleTextureBankLo/Hi
# as the ROM start and end of the segment, and the callers of
# efParticleGetLoadBankID (efdisplay.c:100, gryoster.c:256, grpupupu.c:689,
# grhyrule.c:411, mntitle.c:1469, itmanager.c:150) pass their addresses.
# So --emit-ld writes exactly those assignments and the port's
# syDmaReadRom (src/dc/dma.c) turns an address back into a file.
#
# The three particles_unk* banks are named differently and are left out.
# Nothing in the game takes an l<Name>Particle... of
# theirs, because they are the THREE PER-FIGHTER BANKS and a fighter's
# FTData row carries the four addresses directly -- as
# `particles_unk0_scb_ROM_START` and its three siblings, which are the
# build's own segment symbols rather than named constants
# (ssb-decomp-re/src/ft/ftdata.c:3891, :4957 and Ness's). Kirby's bank is
# unk0, Ness's unk1 and Yoshi's unk2. --emit-ld writes those four
# segment symbols per bank too, so ft/ftmanager.c's
# efParticleGetLoadBankID call reaches them the same way.
SYMBOLS = {
    "efcommon": "EFCommon",
    "grpupupu": "GRPupupu",
    "gryoster": "GRYoster",
    "grhyrule": "GRHyrule",
    "mntitle":  "MNTitle",
    "itcommon": "ITManager",   # the bank is itcommon; the symbol is ITManager
}

LBSCRIPT_HEADER_SIZE = 0x30      # lbtypes.h LBScriptHeader
LBTEXTURE_HEADER_SIZE = 0x18     # lbtypes.h LBTextureHeader


def _swap32(buf, off, n=1):
    for i in range(n):
        p = off + i * 4
        buf[p:p + 4] = buf[p:p + 4][::-1]


def _swap16(buf, off, n=1):
    for i in range(n):
        p = off + i * 2
        buf[p:p + 2] = buf[p:p + 2][::-1]


def _be32(buf, off):
    return struct.unpack_from(">I", buf, off)[0]


def swap_script_bank(data: bytes) -> bytearray:
    """LBScriptDesc { s32 scripts_num; LBScript *scripts[n]; } plus one
    0x30-byte LBScriptHeader at each offset. Everything after a header is
    bytecode and is left alone."""
    buf = bytearray(data)
    n = _be32(buf, 0)
    offsets = [_be32(buf, 4 + i * 4) for i in range(n)]

    _swap32(buf, 0)                 # scripts_num
    _swap32(buf, 4, n)              # the offset array

    for off in offsets:
        if off + LBSCRIPT_HEADER_SIZE > len(buf):
            raise ValueError(f"script at {off:#x} runs past the bank")
        # kind, texture_id, generator_lifetime, particle_lifetime
        _swap16(buf, off, 4)
        # flags, gravity, friction, vel[3], unk20, unk24, update_rate, size
        _swap32(buf, off + 8, 10)
    return buf


def texture_data_count(buf, off) -> int:
    """How many u32 offsets follow an LBTextureHeader. Mirrors
    extractParticleTextures.py's parse_textures()."""
    count = _be32(buf, off)
    fmt = _be32(buf, off + 4)
    flags = _be32(buf, off + 20)

    if count == 0:
        return 0
    if fmt == FMT_CI:
        # One palette per image, or a single shared one after them.
        return count + 1 if (flags & 1) else count * 2
    first_data = _be32(buf, off + LBTEXTURE_HEADER_SIZE)
    return (first_data - (off + LBTEXTURE_HEADER_SIZE)) // 4


def swap_texture_bank(data: bytes) -> bytearray:
    """LBTextureDesc { s32 textures_num; LBTexture *textures[n]; } plus one
    0x18-byte LBTextureHeader and its u32 data[] at each offset. Image and
    palette bytes are not touched."""
    buf = bytearray(data)
    n = _be32(buf, 0)
    offsets = [_be32(buf, 4 + i * 4) for i in range(n)]

    for off in offsets:
        if off + LBTEXTURE_HEADER_SIZE > len(buf):
            raise ValueError(f"texture at {off:#x} runs past the bank")
        ndata = texture_data_count(buf, off)
        if ndata < 0 or off + LBTEXTURE_HEADER_SIZE + ndata * 4 > len(buf):
            raise ValueError(f"texture at {off:#x} claims {ndata} data words")
        _swap32(buf, off + LBTEXTURE_HEADER_SIZE, ndata)
        _swap32(buf, off, 6)        # count, fmt, siz, width, height, flags

    _swap32(buf, 0)                 # textures_num
    _swap32(buf, 4, n)              # the offset array
    return buf


def export(rom: bytes, bank: str, kind: str) -> bytes:
    lo, hi = BANKS[bank][kind]
    raw = rom[lo:hi]
    if len(raw) != hi - lo:
        raise ValueError(f"{bank}.{kind}: rom is short of {hi:#x}")
    return bytes(swap_script_bank(raw) if kind == "scb" else swap_texture_bank(raw))


def emit_ld(prefix: str) -> str:
    """The linker fragment. Absolute symbol assignments cost no space and
    no relocation: `&lEFCommonParticleScriptBankLo` becomes the literal
    0xAC7340 the N64 DMA'd from, which is what src/dc/dma.c looks up.
    `prefix` is the toolchain's C symbol prefix -- "_" for sh-elf, empty
    for the host's ELF."""
    out = ["/* Generated by tools/export/ssb_particleexport.py -- do not edit.",
           " *",
           " * The particle banks' ROM addresses, as the game's linker had them",
           " * (ssb-decomp-re/symbols/linker_constants.txt). The port never reads",
           " * one: src/dc/dma.c's syDmaReadRom turns the address back into the",
           " * file tools/export/ssb_particleexport.py cut from the same range, so",
           " * ef/efparticle.c and every caller of efParticleGetLoadBankID compile",
           " * unmodified. */"]
    for bank in sorted(SYMBOLS):
        name = SYMBOLS[bank]
        out.append("")
        for kind, what in (("scb", "Script"), ("txb", "Texture")):
            lo, hi = BANKS[bank][kind]
            for end, addr in (("Lo", lo), ("Hi", hi)):
                out.append(f"{prefix}l{name}Particle{what}Bank{end} = {addr:#010x};")
    # The per-fighter banks, by their segment symbols.
    out.append("")
    out.append("/* The three per-fighter banks. A fighter's FTData row names")
    out.append(" * these four segment symbols directly rather than an")
    out.append(" * l<Name>Particle... constant: unk0 is Kirby's, unk1 Ness's,")
    out.append(" * unk2 Yoshi's (ssb-decomp-re/src/ft/ftdata.c). */")
    for bank in ("particles_unk0", "particles_unk1", "particles_unk2"):
        for kind in ("scb", "txb"):
            lo, hi = BANKS[bank][kind]
            for end, addr in (("START", lo), ("END", hi)):
                out.append(f"{prefix}{bank}_{kind}_ROM_{end} = {addr:#010x};")
    return "\n".join(out) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", default=ROM_DEFAULT)
    ap.add_argument("--bank", choices=sorted(BANKS))
    ap.add_argument("--kind", choices=("scb", "txb"))
    ap.add_argument("--out", help="output file (with --bank and --kind)")
    ap.add_argument("--all", action="store_true", help="every bank, both kinds")
    ap.add_argument("--outdir", help="output directory (with --all)")
    ap.add_argument("--emit-ld", metavar="FILE",
                    help="write the linker fragment of bank addresses")
    ap.add_argument("--symbol-prefix", default="",
                    help="the toolchain's C symbol prefix (\"_\" for sh-elf)")
    args = ap.parse_args()

    if args.emit_ld:
        os.makedirs(os.path.dirname(os.path.abspath(args.emit_ld)), exist_ok=True)
        with open(args.emit_ld, "w") as f:
            f.write(emit_ld(args.symbol_prefix))
        print(f"{args.emit_ld}: {len(SYMBOLS) * 4} bank addresses")
        return 0

    with open(args.rom, "rb") as f:
        rom = f.read()

    if args.all:
        if not args.outdir:
            sys.exit("--all needs --outdir")
        os.makedirs(args.outdir, exist_ok=True)
        for bank in sorted(BANKS):
            for kind in ("scb", "txb"):
                path = os.path.join(args.outdir, f"{bank}.{kind}")
                data = export(rom, bank, kind)
                with open(path, "wb") as f:
                    f.write(data)
                print(f"{path}: {len(data)} bytes")
        return 0

    if not (args.bank and args.kind and args.out):
        sys.exit("need --bank, --kind and --out (or --all --outdir)")
    data = export(rom, args.bank, args.kind)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(data)
    print(f"{args.out}: {len(data)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
