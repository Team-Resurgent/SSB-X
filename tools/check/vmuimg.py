#!/usr/bin/env python3
"""vmuimg.py -- build, damage and read VMU memory card images.

A memory card image is 128 KB of flash, the
same 256 blocks of 512 bytes a VMU has: the root at block 255, one FAT
block at 254, thirteen directory blocks from 253 down to 241, and 200 user
blocks below them. KOS (external/kos/kernel/arch/dreamcast/fs/vmufs.c)
reads and writes that layout and nothing else, so this is that layout from
the other side: a card to boot the game against in a known state, and a
way to read back what the game wrote to it.

What a card holds for the game is one VMS file, SSB64DC.SAV
(src/dc/vmucard.c): a 128-byte header in vmu_hdr_t's layout
(external/kos/.../include/dc/vmu_pkg.h), the icon frames, the eyecatch,
then the payload -- the 3036 bytes lb/lbbackup.c writes, two copies of
LBBackupData at 0 and 1520. The header's CRC is KOS's net_crc16ccitt over
the whole file with the CRC field zeroed, which is what vmu_pkg_parse
checks. Each copy is the game's own: valid when lbBackupCreateChecksum --
sum of byte[i] * (i + 1) over the first 0x5E8 bytes -- equals the s32 at
0x5E8 and the u16 signature at 0x5E4 is 666 (lb/lbbackup.c:13-33). The
offsets are ssb-decomp-re/src/lb/lbtypes.h's LBBackupData on the SH-4;
tools/check/backup_check.py is what holds the port's layout to the ROM's.

Commands (IMG is a card image; one that does not exist yet is an error
except for `format`):

  format IMG                      a blank card, formatted
  add IMG --synthetic [--boot N]  SSB64DC.SAV with a minimal valid payload:
                                  zeroes, signature 666, the checksum, and
                                  `boot` = N so a probe can tell cards apart
  add IMG --payload FILE          ... with FILE's bytes as the payload
  extract IMG --out FILE          the payload of SSB64DC.SAV
  fill IMG --free N               a FILLER.BIN that leaves N blocks free
  corrupt IMG --mode M            crc    the header CRC no longer matches
                                  appid  another game's app id, CRC redone
                                  copy1  the first copy's checksum wrong
                                  copies both copies' checksums wrong
  dump IMG                        the card, the file, both copies
  show IMG --out PNG              the file's icon frames and eyecatch as
                                  a picture, from the bytes on the card
  selftest                        every command against every other

Names in `add`, `extract` and `corrupt` default to SSB64DC.SAV (--name).
"""
import argparse
import os
import struct
import sys
import tempfile
import zlib

BLOCK = 512
BLOCKS = 256
ROOT_BLK = 255
FAT_BLK = 254
DIR_BLK = 253
DIR_SIZE = 13
USER_BLOCKS = 200
FAT_FREE = 0xFFFC
FAT_END = 0xFFFA

SAVE_NAME = "SSB64DC.SAV"
FILETYPE_DATA = 0x33

# vmu_hdr_t
HDR_SIZE = 128
# vmu_hdr_t.icon_pal
HDR_PAL = 0x60
# an eyecatch's bytes by type (KOS util/vmu_pkg.c vmu_eyecatch_size)
EC_SIZE = {0: 0, 1: 72 * 56 * 2, 2: 512 + 72 * 56, 3: 32 + 72 * 56 // 2}
HDR_CRC = 0x46

# src/dc/vmucard.c card_pkg
DESC_SHORT = "SUPER SMASH BROS"
DESC_LONG = "Save data -- records and options"
APP_ID = "SSB64-DC"

# LBBackupData on the SH-4 (lb/lbtypes.h; sizes held by backup_check.py)
BACKUP_SIZE = 1516
COPY2 = 1520                  # ALIGN(1516, 0x10)
SAVED = COPY2 + BACKUP_SIZE   # 3036, vmusave.c SRAM_SAVED
OFF_BOOT = 0x5E3
OFF_SIGNATURE = 0x5E4
OFF_CHECKSUM = 0x5E8
SIGNATURE = 666

# The formatted root's 24 media-info bytes: 0xFF blocks, partition 0, root 0xFF, FAT 0xFE x1,
# directory 0xFD x13, icon 0, save area 0xC8 (KOS's blk_cnt) x31.
ROOT_MEDIA = bytes.fromhex("ff000000ff00fe000100fd000d000000c8001f0000000000")
ROOT_TIMESTAMP = bytes.fromhex("1998112700005904")


def crc16ccitt(data, start=0):
    """KOS kernel/net/net_crc.c net_crc16ccitt."""
    rv = start
    for b in data:
        tmp = ((rv >> 8) ^ b) & 0xFF
        tmp ^= tmp >> 4
        rv = ((rv << 8) ^ (tmp << 12) ^ (tmp << 5) ^ tmp) & 0xFFFF
    return rv


def backup_checksum(copy):
    """lb/lbbackup.c lbBackupCreateChecksum, as the s32 it wraps to."""
    total = sum(b * (i + 1) for i, b in enumerate(copy[:OFF_CHECKSUM]))
    total &= 0xFFFFFFFF
    return total - (1 << 32) if total & 0x80000000 else total


def copy_state(copy):
    """(valid, checksum_ok, signature, boot) for one LBBackupData."""
    stored = struct.unpack_from("<i", copy, OFF_CHECKSUM)[0]
    sig = struct.unpack_from("<H", copy, OFF_SIGNATURE)[0]
    ok = backup_checksum(copy) == stored
    return ok and sig == SIGNATURE, ok, sig, copy[OFF_BOOT]


def synthetic_payload(boot):
    copy = bytearray(BACKUP_SIZE)
    copy[OFF_BOOT] = boot & 0xFF
    struct.pack_into("<H", copy, OFF_SIGNATURE, SIGNATURE)
    struct.pack_into("<i", copy, OFF_CHECKSUM, backup_checksum(copy))
    out = bytearray(SAVED)
    out[0:BACKUP_SIZE] = copy
    out[COPY2:COPY2 + BACKUP_SIZE] = copy
    return bytes(out)


def pad_field(s, n):
    """vmu_pkg_build's desc_short/desc_long: the field is spaces, the
    string over them."""
    raw = s.encode("ascii")[:n]
    return raw + b" " * (n - len(raw))


def build_vms(payload, icons=1, desc_short=DESC_SHORT, desc_long=DESC_LONG,
              app_id=APP_ID, icon_pal=None, icon_data=None, ec_type=0,
              ec_data=b""):
    """vmu_pkg_build, padded to a block as vmufs_write pads it. With no
    icon data the frames are zeroes (transparent)."""
    hdr = bytearray(HDR_SIZE)
    hdr[0:16] = pad_field(desc_short, 16)
    hdr[16:48] = pad_field(desc_long, 32)
    # app_id is strcpy'd into a zeroed header: NUL padding, not spaces
    hdr[48:64] = app_id.encode("ascii")[:16].ljust(16, b"\0")
    struct.pack_into("<HHHHI", hdr, 0x40, icons, 0, ec_type, 0, len(payload))
    if icon_pal is not None:
        struct.pack_into("<16H", hdr, HDR_PAL, *icon_pal)
    if icon_data is None:
        icon_data = bytes(BLOCK * icons)
    if len(icon_data) != BLOCK * icons or len(ec_data) != EC_SIZE[ec_type]:
        raise ValueError("vmuimg: icon or eyecatch the wrong size")
    body = bytes(hdr) + icon_data + ec_data + payload
    crc = crc16ccitt(body)
    body = body[:HDR_CRC] + struct.pack("<H", crc) + body[HDR_CRC + 2:]
    return body + bytes(-len(body) % BLOCK)


class Card:
    def __init__(self, data):
        if len(data) != BLOCK * BLOCKS:
            raise SystemExit("vmuimg: a card image is %d bytes, not %d"
                             % (BLOCK * BLOCKS, len(data)))
        self.data = bytearray(data)

    @classmethod
    def blank(cls):
        data = bytearray(BLOCK * BLOCKS)
        root = ROOT_BLK * BLOCK
        data[root:root + 16] = b"\x55" * 16
        data[root + 0x30:root + 0x38] = ROOT_TIMESTAMP
        data[root + 0x40:root + 0x58] = ROOT_MEDIA
        fat = [FAT_FREE] * BLOCKS
        fat[ROOT_BLK] = FAT_END
        fat[FAT_BLK] = FAT_END
        for b in range(DIR_BLK, DIR_BLK - DIR_SIZE + 1, -1):
            fat[b] = b - 1
        fat[DIR_BLK - DIR_SIZE + 1] = FAT_END
        card = cls(data)
        card.fat = fat
        return card

    @classmethod
    def load(cls, path):
        with open(path, "rb") as f:
            return cls(f.read())

    def save(self, path):
        with open(path, "wb") as f:
            f.write(self.data)

    def blk(self, n):
        return self.data[n * BLOCK:(n + 1) * BLOCK]

    @property
    def fat(self):
        return list(struct.unpack_from("<256H", self.data, FAT_BLK * BLOCK))

    @fat.setter
    def fat(self, fat):
        struct.pack_into("<256H", self.data, FAT_BLK * BLOCK, *fat)

    def root_ok(self):
        root = ROOT_BLK * BLOCK
        return self.data[root:root + 16] == b"\x55" * 16

    def free_blocks(self):
        """vmufs_fat_free: only the user blocks count."""
        return sum(1 for b in self.fat[:USER_BLOCKS] if b == FAT_FREE)

    def dir_slots(self):
        """(offset, 32-byte entry) for every slot, in KOS's order: block
        253 first, down to 241."""
        for i in range(DIR_SIZE):
            base = (DIR_BLK - i) * BLOCK
            for j in range(BLOCK // 32):
                off = base + j * 32
                yield off, self.data[off:off + 32]

    def entries(self):
        for off, e in self.dir_slots():
            if e[0] != 0:
                name = e[4:16].rstrip(b"\0").decode("ascii", "replace")
                yield off, name, e

    def find(self, name):
        for off, n, e in self.entries():
            if n == name:
                return off, e
        return None, None

    def chain(self, first):
        fat, out = self.fat, []
        blk = first
        while blk != FAT_END:
            if blk >= USER_BLOCKS or blk in out:
                raise SystemExit("vmuimg: broken FAT chain at block %d" % blk)
            out.append(blk)
            blk = fat[blk]
        return out

    def read(self, name):
        off, e = self.find(name)
        if off is None:
            return None
        first, = struct.unpack_from("<H", e, 2)
        return b"".join(bytes(self.blk(b)) for b in self.chain(first))

    def delete(self, name):
        off, e = self.find(name)
        if off is None:
            return
        fat = self.fat
        for b in self.chain(struct.unpack_from("<H", e, 2)[0]):
            fat[b] = FAT_FREE
        self.fat = fat
        self.data[off:off + 32] = bytes(32)

    def write(self, name, body, timestamp=bytes.fromhex("2026092412000003")):
        """vmufs_write(..., VMUFS_OVERWRITE) for a data file: the old file
        freed, blocks taken from the top of the user area down."""
        if len(body) % BLOCK:
            body = body + bytes(-len(body) % BLOCK)
        self.delete(name)
        need = len(body) // BLOCK
        fat = self.fat
        free = [b for b in range(USER_BLOCKS - 1, -1, -1) if fat[b] == FAT_FREE]
        if len(free) < need:
            raise SystemExit("vmuimg: %s needs %d blocks, %d free"
                             % (name, need, len(free)))
        blocks = free[:need]
        for i, b in enumerate(blocks):
            fat[b] = blocks[i + 1] if i + 1 < need else FAT_END
            self.data[b * BLOCK:(b + 1) * BLOCK] = body[i * BLOCK:(i + 1) * BLOCK]
        self.fat = fat
        slot = next((off for off, e in self.dir_slots() if e[0] == 0), None)
        if slot is None:
            raise SystemExit("vmuimg: the directory is full")
        ent = bytearray(32)
        ent[0] = FILETYPE_DATA
        struct.pack_into("<H", ent, 2, blocks[0])
        ent[4:16] = name.encode("ascii")[:12].ljust(12, b"\0")
        ent[16:24] = timestamp
        struct.pack_into("<HH", ent, 24, need, 0)
        self.data[slot:slot + 32] = ent


def parse_vms(body):
    """vmu_pkg_parse's checks: (header dict, payload or None, crc_ok)."""
    if body is None or len(body) < HDR_SIZE:
        return None, None, False
    h = {
        "desc_short": body[0:16].decode("ascii", "replace").rstrip(),
        "desc_long": body[16:48].decode("ascii", "replace").rstrip(),
        "app_id": body[48:64].decode("ascii", "replace").rstrip(" \0"),
    }
    (h["icon_cnt"], h["anim_speed"], h["eyecatch"], h["crc"],
     h["data_len"]) = struct.unpack_from("<HHHHI", body, 0x40)
    size = (HDR_SIZE + BLOCK * h["icon_cnt"] + EC_SIZE.get(h["eyecatch"], 0)
            + h["data_len"])
    h["size"] = size
    if size > len(body):
        return h, None, False
    zeroed = body[:HDR_CRC] + b"\0\0" + body[HDR_CRC + 2:size]
    crc_ok = crc16ccitt(zeroed) == h["crc"]
    return h, body[size - h["data_len"]:size], crc_ok


def vms_pictures(body):
    """The icon palette, frames and eyecatch of a VMS file's bytes."""
    h, _, _ = parse_vms(body)
    if h is None:
        raise SystemExit("vmuimg: not a VMS file")
    pal = list(struct.unpack_from("<16H", body, HDR_PAL))
    at = HDR_SIZE + BLOCK * h["icon_cnt"]
    ec = EC_SIZE.get(h["eyecatch"], 0)
    return (pal, body[HDR_SIZE:at], h["icon_cnt"], h["eyecatch"],
            body[at:at + ec])


def argb4444_rgb(c, under=(0, 0, 0)):
    a = (c >> 12 & 15) / 15.0
    return tuple(int(((c >> sh & 15) * 17) * a + u * (1 - a) + 0.5)
                 for sh, u in zip((8, 4, 0), under))


def write_png(path, w, h, rgb):
    """rgb: w * h (r, g, b) tuples, row-major."""
    raw = b"".join(b"\0" + bytes(v for p in rgb[y * w:(y + 1) * w] for v in p)
                   for y in range(h))

    def chunk(tag, data):
        c = tag + data
        return (struct.pack(">I", len(data)) + c +
                struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF))

    with open(path, "wb") as fp:
        fp.write(b"\x89PNG\r\n\x1a\n")
        fp.write(chunk(b"IHDR", struct.pack(">2I5B", w, h, 8, 2, 0, 0, 0)))
        fp.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        fp.write(chunk(b"IEND", b""))


def render_png(path, icon_pal, icons, icon_cnt, ec_type, eyecatch, lcd=None,
               scale=4):
    """The icon frames side by side over a checkerboard (so transparency
    shows), then the eyecatch, then the LCD image as a VMU shows it (dark
    on a pale green), all `scale` times. Only the 16-colour eyecatch is
    drawn; another type leaves a gap."""
    pad = 4
    parts = []

    def nibble(data, i):
        b = data[i // 2]
        return b >> 4 if i % 2 == 0 else b & 15

    for k in range(icon_cnt):
        frame = icons[BLOCK * k:BLOCK * (k + 1)]
        px = []
        for y in range(32):
            for x in range(32):
                under = (200, 200, 200) if (x // 4 + y // 4) % 2 else (140, 140, 140)
                px.append(argb4444_rgb(icon_pal[nibble(frame, y * 32 + x)], under))
        parts.append((32, 32, px))
    if ec_type == 3 and len(eyecatch) == EC_SIZE[3]:
        pal = struct.unpack_from("<16H", eyecatch, 0)
        px = [argb4444_rgb(pal[nibble(eyecatch[32:], i)]) for i in range(72 * 56)]
        parts.append((72, 56, px))
    if lcd is not None:
        px = []
        for y in range(32):
            for x in range(48):
                on = lcd[y * 6 + x // 8] & (0x80 >> (x % 8))
                px.append((30, 40, 30) if on else (170, 200, 160))
        parts.append((48, 32, px))

    w = sum(p[0] for p in parts) * scale + pad * (len(parts) + 1)
    h = max(p[1] for p in parts) * scale + 2 * pad
    out = [(40, 40, 40)] * (w * h)
    ox = pad
    for pw, ph, px in parts:
        for y in range(ph * scale):
            for x in range(pw * scale):
                out[(pad + y) * w + ox + x] = px[(y // scale) * pw + x // scale]
        ox += pw * scale + pad
    write_png(path, w, h, out)


def classify(card, name=SAVE_NAME):
    """The plan's D4 card state for one card: 'nosave', 'valid' or
    'corrupt:<why>'. A first copy that fails with a second that holds is
    valid -- lb/lbbackup.c lbBackupIsSramValid repairs it."""
    body = card.read(name)
    if body is None:
        return "nosave"
    h, payload, crc_ok = parse_vms(body)
    if h is None or not crc_ok:
        return "corrupt:crc"
    if h["app_id"] != APP_ID:
        return "corrupt:appid"
    if h["data_len"] < SAVED:
        return "corrupt:short"
    if not (copy_state(payload[:BACKUP_SIZE])[0]
            or copy_state(payload[COPY2:COPY2 + BACKUP_SIZE])[0]):
        return "corrupt:copies"
    return "valid"


def cmd_dump(card, name):
    print("root: %s, %d of %d blocks free"
          % ("formatted" if card.root_ok() else "NOT FORMATTED",
             card.free_blocks(), USER_BLOCKS))
    for off, n, e in card.entries():
        first, = struct.unpack_from("<H", e, 2)
        size, hdroff = struct.unpack_from("<HH", e, 24)
        print("file: %-12s type %02x first %3d blocks %3d stamp %s"
              % (n, e[0], first, size, bytes(e[16:24]).hex()))
    body = card.read(name)
    if body is None:
        print("%s: absent" % name)
        return
    h, payload, crc_ok = parse_vms(body)
    print("%s: %r / %r / app %r" % (name, h["desc_short"], h["desc_long"],
                                   h["app_id"]))
    print("%s: icons %d speed %d eyecatch %d data_len %d file %d bytes, "
          "crc %04x %s" % (name, h["icon_cnt"], h["anim_speed"],
                           h["eyecatch"], h["data_len"], h["size"], h["crc"],
                           "ok" if crc_ok else "BAD"))
    if payload is not None and len(payload) >= SAVED:
        for k, at in ((1, 0), (2, COPY2)):
            valid, ok, sig, boot = copy_state(payload[at:at + BACKUP_SIZE])
            print("%s: copy %d %s (checksum %s, signature %d, boot %d)"
                  % (name, k, "valid" if valid else "INVALID",
                     "ok" if ok else "bad", sig, boot))
    print("%s: %s" % (name, classify(card, name)))


def cmd_corrupt(card, name, mode):
    body = card.read(name)
    if body is None:
        raise SystemExit("vmuimg: no %s to corrupt" % name)
    h, payload, _ = parse_vms(body)
    if mode == "crc":
        body = bytearray(body)
        body[0] ^= 0x20          # desc_short's first letter: the CRC breaks
        card.write(name, bytes(body))
        return
    if mode == "appid":
        card.write(name, build_vms(payload, h["icon_cnt"], app_id="SOMEGAME"))
        return
    payload = bytearray(payload)
    for at in ((0,) if mode == "copy1" else (0, COPY2)):
        struct.pack_into("<i", payload, at + OFF_CHECKSUM,
                         backup_checksum(payload[at:at + BACKUP_SIZE]) ^ 1)
    card.write(name, build_vms(bytes(payload), h["icon_cnt"]))


def cmd_fill(card, free):
    card.delete("FILLER.BIN")
    have = card.free_blocks()
    if have < free:
        raise SystemExit("vmuimg: %d blocks free already, fewer than %d"
                         % (have, free))
    if have > free:
        card.write("FILLER.BIN", bytes((have - free) * BLOCK))


def selftest():
    fails = []

    def check(what, cond):
        if not cond:
            fails.append(what)

    # the CRC is KOS's: the CCITT-FALSE check value is 0x29B1 from 0xFFFF,
    # and 0x31C3 from 0 (XMODEM)
    check("crc xmodem", crc16ccitt(b"123456789") == 0x31C3)
    check("crc false", crc16ccitt(b"123456789", 0xFFFF) == 0x29B1)

    card = Card.blank()
    check("blank formatted", card.root_ok())
    check("blank free 200", card.free_blocks() == USER_BLOCKS)
    check("blank nosave", classify(card) == "nosave")

    payload = synthetic_payload(7)
    card.write(SAVE_NAME, build_vms(payload))
    # 128 + 512 + 3036 = 3676 -> 8 blocks: vmucard.c's plain icon
    check("save 8 blocks", card.free_blocks() == USER_BLOCKS - 8)
    check("save valid", classify(card) == "valid")
    _, got, crc_ok = parse_vms(card.read(SAVE_NAME))
    check("payload round trip", got == payload and crc_ok)
    check("boot marker", copy_state(got[:BACKUP_SIZE])[3] == 7)
    off, e = card.find(SAVE_NAME)
    check("top-down first block", struct.unpack_from("<H", e, 2)[0] == 199)

    card.write(SAVE_NAME, build_vms(synthetic_payload(9)))
    check("overwrite keeps count", card.free_blocks() == USER_BLOCKS - 8)
    check("one entry", sum(1 for _ in card.entries()) == 1)

    for mode, want in (("crc", "corrupt:crc"), ("appid", "corrupt:appid"),
                       ("copy1", "valid"), ("copies", "corrupt:copies")):
        c = Card(bytes(card.data))
        cmd_corrupt(c, SAVE_NAME, mode)
        check("corrupt %s -> %s" % (mode, want), classify(c) == want)

    c = Card(bytes(card.data))
    cmd_fill(c, 3)
    check("fill leaves 3", c.free_blocks() == 3)
    cmd_fill(c, 50)
    check("refill leaves 50", c.free_blocks() == 50)
    check("fill keeps save", classify(c) == "valid")

    c.delete(SAVE_NAME)
    check("delete frees", c.free_blocks() == 58)

    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "card.bin")
        card.save(p)
        check("file round trip", Card.load(p).data == card.data)

        # the save file: three icon frames and a 16-colour eyecatch, 14
        # blocks (the plan's D9), and every picture where it was put
        pal = [0x0000] + [0xF000 | i for i in range(1, 16)]
        icons = bytes(range(256)) * 6
        ec = bytes(range(32)) + bytes([0x5A]) * (72 * 56 // 2)
        c = Card.blank()
        c.write(SAVE_NAME, build_vms(synthetic_payload(3), icons=3,
                                     icon_pal=pal, icon_data=icons,
                                     ec_type=3, ec_data=ec))
        check("art 14 blocks", c.free_blocks() == USER_BLOCKS - 14)
        check("art valid", classify(c) == "valid")
        got = vms_pictures(c.read(SAVE_NAME))
        check("art round trip", got == (pal, icons, 3, 3, ec))
        png = os.path.join(d, "show.png")
        render_png(png, *got, lcd=bytes(192))
        with open(png, "rb") as fp:
            check("png written", fp.read(8) == b"\x89PNG\r\n\x1a\n")

    for f in fails:
        print("vmuimg selftest FAILED: %s" % f)
    if not fails:
        print("vmuimg selftest: ok")
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("cmd", choices=("format", "add", "extract", "fill",
                                    "corrupt", "dump", "show", "selftest"))
    ap.add_argument("img", nargs="?")
    ap.add_argument("--name", default=SAVE_NAME)
    ap.add_argument("--synthetic", action="store_true")
    ap.add_argument("--boot", type=int, default=1)
    ap.add_argument("--payload")
    ap.add_argument("--out")
    ap.add_argument("--free", type=int)
    ap.add_argument("--mode", choices=("crc", "appid", "copy1", "copies"))
    a = ap.parse_args()

    if a.cmd == "selftest":
        return selftest()
    if a.img is None:
        ap.error("%s needs an image" % a.cmd)
    if a.cmd == "format":
        Card.blank().save(a.img)
        return 0

    card = Card.load(a.img)
    if a.cmd == "dump":
        cmd_dump(card, a.name)
        return 0
    if a.cmd == "show":
        body = card.read(a.name)
        if body is None or a.out is None:
            raise SystemExit("vmuimg: nothing to show (or no --out)")
        render_png(a.out, *vms_pictures(body))
        return 0
    if a.cmd == "extract":
        _, payload, _ = parse_vms(card.read(a.name))
        if payload is None or a.out is None:
            raise SystemExit("vmuimg: nothing to extract (or no --out)")
        with open(a.out, "wb") as f:
            f.write(payload)
        return 0
    if a.cmd == "add":
        if a.synthetic == (a.payload is not None):
            ap.error("add wants exactly one of --synthetic and --payload")
        if a.synthetic:
            payload = synthetic_payload(a.boot)
        else:
            with open(a.payload, "rb") as f:
                payload = f.read()
        card.write(a.name, build_vms(payload))
    elif a.cmd == "fill":
        if a.free is None:
            ap.error("fill wants --free")
        cmd_fill(card, a.free)
    elif a.cmd == "corrupt":
        if a.mode is None:
            ap.error("corrupt wants --mode")
        cmd_corrupt(card, a.name, a.mode)
    card.save(a.img)
    return 0


if __name__ == "__main__":
    sys.exit(main())
