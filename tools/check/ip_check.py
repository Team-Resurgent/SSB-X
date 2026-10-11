#!/usr/bin/env python3
"""ip_check.py -- what the disc says it is, and the audit that it said it.

Every Dreamcast disc carries an IP.BIN in its system area: 32 KB of
bootstrap with a header of eleven text fields in front of it, which the
console reads before it reads a byte of the game.  Nine of those fields
are names for the software.  Two are declarations *about* it:

    Area Symbols (0x30)   the regions the disc is playable in
    Peripherals  (0x38)   the hardware it needs, and the hardware it can use

makeip defaults both, and the peripherals default is not neutral.
`E000F10` says the software supports a light gun, a keyboard and a mouse,
requires no controller feature at all, uses every expansion slot the
Dreamcast has including the microphone, and runs in VGA mode.  The port
reads none of those peripherals, cannot be played without the analog
stick the default does not ask for, and could not
run in VGA mode either, because main.c forced DM_640x480_NTSC_IL over
KOS's cable-aware default.  Left alone, the field is a claim about a
machine that does not exist, and one bit of it was a claim a VGA box
would have proved false.

This file is where the truth about that lives, as named bits, and it does
two jobs with it:

    --makeip-args    print the flags scripts/make_cdi.sh hands makeip
    <image>          read the field block back out of a finished image
                     and assert it says what the manifest says

The pair is tools/export/disc_layout.py and tools/check/disc_check.py's, for the same
reason: makeip is a tool we do not control, and a field that quietly
stopped being written would look exactly like one that was.

The image argument may be either the IP.BIN itself or the ISO mkisofs
wrote from it: `mkisofs -G` puts the bootstrap at the front of the
volume's system area, so the field block is at offset 0 of both.
"""
import argparse
import sys

# ---------------------------------------------------------------------------
# Area symbols: which regions the disc is playable in.
#
# Eight characters, of which three are ever used: J (Japan and the rest of
# East Asia), U (USA and Canada), E (Europe).  A space means the console
# in that region will not boot the disc.
#
# All three, because the port is a 60 Hz build everywhere and there is no
# region in which that is untrue.  src/game/ssb64/main.c asks KOS for the
# generic DM_640x480, which resolves to VGA 60 Hz on a VGA cable and NTSC
# 640x480 interlaced 60 Hz on everything else -- never the PAL 50 Hz mode,
# on any console, including a European one.  That is not an oversight: the
# game's tic is a frame (syTaskmanRunFrame), so a 50 Hz field rate would
# run Smash Bros. seventeen percent slow -- every animation, every hitstun
# counter, every timer.  A European console driving a 60 Hz-capable set
# gets the game at speed; one driving a set that cannot sync 60 Hz gets a
# rolling picture, which is a real limitation, kept
# rather than hidden by dropping the E.
AREA_SYMBOLS = "JUE"

# ---------------------------------------------------------------------------
# Peripherals: 28 bits, written as seven hex digits.
#
# The groups, from makeip's README (which is the only map of this field
# any of us have):
#
#   bit 0       Windows CE.  Never, for homebrew.
#   bit 4       VGA box.  "This disc can run in VGA mode" -- and a console
#               with a VGA box attached and this bit clear shows the
#               "cannot be used with the VGA box" screen instead of the game.
#   bits 8-11   expansion units the software can use (the slots in a pad).
#   bits 12-24  the controller the software *requires*.  A console holds a
#               connected pad to this: a peripheral missing a bit that is
#               set here is not accepted as a controller for this disc.
#   bits 25-27  optional peripherals the software can also read.
#
# One row per bit, with a yes or a no and the reason for it.  Nothing in
# this table is a guess about the port: every "True" below names the code
# that reads the thing.
PERIPHERALS = [
    # bit  name                             used   why
    (27, "mouse",                           False, "nothing reads one"),
    (26, "keyboard",                        False, "nothing reads one"),
    (25, "light gun",                       False, "nothing reads one"),
    (24, "expanded analog vertical",        False, "no second stick on a "
                                                   "standard pad"),
    (23, "expanded analog horizontal",      False, "no second stick on a "
                                                   "standard pad"),
    (22, "analog vertical",                 True,  "sy_input_poll st->joyy; "
                                                   "the game moves on the stick"),
    (21, "analog horizontal",               True,  "sy_input_poll st->joyx; "
                                                   "the game moves on the stick"),
    (20, "analog L trigger",                True,  "sy_input_poll st->ltrig "
                                                   "-> N64 Z (shield)"),
    (19, "analog R trigger",                True,  "sy_input_poll st->rtrig "
                                                   "-> N64 R (grab)"),
    (18, "expanded direction buttons",      False, "no second D-pad on a "
                                                   "standard pad"),
    (17, "Z button",                        False, "not on a standard pad"),
    (16, "Y button",                        True,  "kPadMap CONT_Y -> N64 C-up"),
    (15, "X button",                        True,  "kPadMap CONT_X -> N64 B"),
    (14, "D button",                        False, "not on a standard pad"),
    (13, "C button",                        False, "not on a standard pad"),
    (12, "start + A + B + directions",      True,  "kPadMap: start, A, B and "
                                                   "the four D-pad directions"),
    (11, "memory card",                     True,  "src/dc/vmucard.c -- the "
                                                   "game's save is a VMS file"),
    (10, "microphone",                      False, "nothing records"),
    (9,  "Puru Puru pack",                  False, "the port does not rumble; "
                                                   "the N64 game has no "
                                                   "Rumble Pak support either"),
    (8,  "other expansions",                False, "none"),
    (4,  "VGA box",                         True,  "src/game/ssb64/main.c asks "
                                                   "for the generic DM_640x480, "
                                                   "which is the VGA mode on a "
                                                   "VGA cable"),
    (0,  "Windows CE",                      False, "no"),
]

# The same number, written out by hand from the table above.  Two
# expressions of one truth, which is the only kind of check a manifest can
# be given: an edit to the table that was not meant to change what the
# disc declares fails here, and one that was meant to has to say so twice.
PERIPHERALS_EXPECTED = "0799810"

# ---------------------------------------------------------------------------
# The nine fields that are names rather than declarations.  Only the ones
# with an expected value are asserted; the rest are read back and printed,
# because a build stamp nobody looks at is a build stamp nobody notices is
# wrong.
#
# The product number is asserted, and this is why: a Dreamcast's save is a
# file on a VMU, and a memory card image kept on a host may be named after
# the disc's product number. Changing it silently retires every save made
# before the change.
PRODUCT_NO = "T-00000"
VERSION = "V1.000"
BOOT_FILENAME = "1ST_READ.BIN"
GAME_TITLE = "SSB64-DC"
SW_MAKER_NAME = "SSB64-DC"
DEVICE_INFO = "CD-ROM1/1"

FIELDS = [
    # offset  len   name             expected (None: report, do not assert)
    (0x00, 0x10, "Hardware ID",   "SEGA SEGAKATANA"),
    (0x10, 0x10, "Maker ID",      "SEGA ENTERPRISES"),
    (0x20, 0x10, "Device Info",   None),   # the CRC in front is checked below
    (0x30, 0x08, "Area Symbols",  AREA_SYMBOLS),
    (0x38, 0x08, "Peripherals",   PERIPHERALS_EXPECTED),
    (0x40, 0x0a, "Product No",    PRODUCT_NO),
    (0x4a, 0x06, "Version",       VERSION),
    (0x50, 0x10, "Release Date",  None),
    (0x60, 0x10, "Boot Filename", BOOT_FILENAME),
    (0x70, 0x10, "SW Maker Name", SW_MAKER_NAME),
    (0x80, 0x80, "Game Title",    GAME_TITLE),
]

HEADER_SIZE = 0x100


def peripherals_value():
    """The manifest as the seven hex digits makeip wants."""
    v = 0
    for bit, _name, used, _why in PERIPHERALS:
        if used:
            v |= 1 << bit
    return "%07X" % v


def decode_peripherals(text):
    """The names behind a seven-digit field, in the manifest's order."""
    v = int(text, 16)
    return [name for bit, name, _u, _w in PERIPHERALS if v & (1 << bit)]


def calc_crc(buf):
    """makeip's crc.c: the Device Info field's first four hex digits are
    this over the sixteen bytes of Product No + Version at 0x40."""
    n = 0xFFFF
    for b in buf:
        n ^= b << 8
        for _ in range(8):
            n = ((n << 1) ^ 4129) & 0xFFFF if n & 0x8000 else (n << 1) & 0xFFFF
    return n


def field(data, off, length):
    return data[off:off + length].decode("ascii", "replace").rstrip(" \0")


def check(path, verbose=True):
    with open(path, "rb") as f:
        data = f.read(HEADER_SIZE)
    if len(data) < HEADER_SIZE:
        print("ip_check: %s is shorter than an IP.BIN header" % path,
              file=sys.stderr)
        return False

    errors = []

    # The manifest has to agree with itself before it is used to judge
    # anything else.
    if peripherals_value() != PERIPHERALS_EXPECTED:
        errors.append("the peripheral table computes %s but this file says "
                      "the disc declares %s -- one of the two was edited "
                      "without the other"
                      % (peripherals_value(), PERIPHERALS_EXPECTED))

    for off, length, name, want in FIELDS:
        got = field(data, off, length)
        if want is not None and got != want:
            errors.append("%s (0x%02x) is %r, expected %r"
                          % (name, off, got, want))
        if verbose:
            print("  %-14s %s" % (name + ":", got))

    # Device Info: makeip writes "XXXX CD-ROMx/y", the four digits a CRC
    # over the product number and version.  Recomputing it here is an
    # independent read of the two fields the console identifies the disc
    # by -- and of the one field in the header that is not just text.
    info = field(data, 0x20, 0x10)
    parts = info.split(" ", 1)
    if len(parts) != 2 or parts[1] != DEVICE_INFO:
        errors.append("Device Info (0x20) is %r, expected a CRC and %r"
                      % (info, DEVICE_INFO))
    else:
        want_crc = "%04X" % calc_crc(data[0x40:0x50])
        if parts[0] != want_crc:
            errors.append("Device Info's CRC is %s, but the product number "
                          "and version at 0x40 give %s" % (parts[0], want_crc))

    date = field(data, 0x50, 0x10)
    if not (len(date) == 8 and date.isdigit()):
        errors.append("Release Date (0x50) is %r, expected YYYYMMDD" % date)

    if verbose:
        got = field(data, 0x38, 0x08)
        try:
            names = decode_peripherals(got)
        except ValueError:
            names = None
        if names is not None:
            print("  peripherals:   %s" % ", ".join(names))

    for e in errors:
        print("ip_check: %s" % e, file=sys.stderr)
    if not errors:
        print("ip_check: OK -- %s, region %s, %d peripheral bits"
              % (field(data, 0x40, 0x0a), field(data, 0x30, 0x08).strip(),
                 len(decode_peripherals(field(data, 0x38, 0x08)))))
    return not errors


def makeip_args():
    """The flags scripts/make_cdi.sh hands makeip, one per line so the
    caller can read them into an array without quoting games."""
    return ["-a", AREA_SYMBOLS,
            "-p", peripherals_value(),
            "-n", PRODUCT_NO,
            "-e", VERSION,
            "-b", BOOT_FILENAME,
            "-c", SW_MAKER_NAME,
            "-g", GAME_TITLE]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("image", nargs="?",
                    help="an IP.BIN, or the ISO whose system area holds one")
    ap.add_argument("--makeip-args", action="store_true",
                    help="print makeip's flags instead of checking an image")
    ap.add_argument("-q", "--quiet", action="store_true")
    args = ap.parse_args()

    if args.makeip_args:
        print("\n".join(makeip_args()))
        return 0
    if not args.image:
        ap.error("an image to check, or --makeip-args")
    return 0 if check(args.image, verbose=not args.quiet) else 1


if __name__ == "__main__":
    sys.exit(main())
