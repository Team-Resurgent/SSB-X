#!/usr/bin/env python3
"""ssb64-dc: shared, centralized SSB64 asset-decoding helpers.

Every exporter builds its assets out of relocData files. This module
holds the deterministic decode primitives so the pointer rule, vertex
decoding, joint-tree walk, MObj material chain, N64 tile addressing and the
pygfxd display-list replay are defined once.

Key deterministic facts (all verified against the game's own bytes):

  * Pointer rule:  byte_off = (ptr & 0xFFFF) * 4
      Applies to every relocated pointer in a relocData file, for both the
      vertex pool (G_VTX address) and texture image (SETTIMG address).
  * Vtx is 16 bytes big-endian: x,y,z s16; flag u16; s,t s10.5; then four
      bytes that are a *normal* (nx,ny,nz signed, |n| == 127) plus alpha --
      this model is lit (gsSPLightColor), so those bytes are not a colour.
  * A display list ends at the first G_ENDDL (0xDF) command; every F3DEX2
      command is 8 bytes, so the end is found by scanning opcode bytes.
  * The DObjDesc array is a *tree*: array_dobjs[id] is a child of
      array_dobjs[id - 1] (lbCommonSetupTreeDObjs), terminated by
      id == DOBJ_ARRAY_MAX. Each node's matrix is composed
      M_node = L_node * M_parent (row-vector; gSPMatrix G_MTX_MUL), and the
      RSP transforms vertices *at gsSPVertex time*, so the persistent 32-slot
      vertex buffer holds world-space vertices from a mix of joints. That is
      how joints that load only slots 3..5 but reference 0..5 work.
  * Materials come from two places at once. The joint's own display list sets
      the combiner, lights, tile and tile size, and calls out to segment 0xE;
      segment 0xE is the scratch list gcDrawMObjForDObj builds from the DObj's
      MObj chain, one gSPBranchList per MObj at 8-byte stride. So
      `gsSPDisplayList(0x0E0000NN)` means "apply MObjSub[NN / 8]", which
      supplies the TLUT, the selected sprite, and prim/light colours.
  * A joint's triangles are therefore split into *material batches*: a new
      batch starts whenever the resolved material changes mid-list.
  * Texture coordinates are baked flat. The N64 addresses a tile through
      gsSPTexture scale, the gsDPSetTileSize origin, and per-axis
      mask/mirror/clamp; bake_tile() evaluates all of that offline and emits a
      plain image whose UVs are linear, leaving the PVR only clamp or repeat.
"""
import collections
import copy
import math
import struct

from ssb_texconv import (twiddle_4bpp, twiddle_16bpp,  # noqa: F401
                         rotate5551)                   # (re-exported)

G_ENDDL = 0xDF
DOBJ_ARRAY_MAX = 18       # sys/objtypes.h
DOBJDESC_SIZE = 44        # s32 id; void *dl; Vec3f translate, rotate, scale
MOBJSUB_SIZE = 120        # sizeof(MObjSub), sys/objtypes.h

# MObjSub.flags (sys/objtypes.h)
MOBJ_FLAG_ALPHA = 1 << 0
MOBJ_FLAG_SPLIT = 1 << 1
MOBJ_FLAG_PALETTE = 1 << 2
MOBJ_FLAG_FRAC = 1 << 4
MOBJ_FLAG_TILESIZE = 1 << 5      # gcDrawMObjForDObj's `flags & 0x20`
MOBJ_FLAG_TILESIZE1 = 1 << 6     # ... and `flags & 0x40`
MOBJ_FLAG_TEXTURE = 1 << 7
MOBJ_FLAG_PRIMCOLOR = 1 << 9
MOBJ_FLAG_ENVCOLOR = 1 << 10
MOBJ_FLAG_BLENDCOLOR = 1 << 11
MOBJ_FLAG_LIGHT1 = 1 << 12
MOBJ_FLAG_LIGHT2 = 1 << 13

# gsDPSetTile cm bits, and the combiner source ids we care about.
G_TX_MIRROR = 1
G_TX_CLAMP = 2
G_IM_FMT_CI = 2         # gsDPSetTile fmt: colour-indexed
CC_TEXEL0 = 1
CC_TEXEL1 = 2
CC_PRIMITIVE = 3
CC_SHADE = 4
CC_ENVIRONMENT = 5
# The C slot's own table runs past the four sources above: 14 is
# PRIM_LOD_FRAC, the primitive colour's LOD byte (PR/gbi.h G_CCMUX_*), which
# objdisplay.c:1243 writes as `mobj->lfrac * 255`. It is the *only* slot
# wide enough to name it, and the only combiner in the game that does is
# the two-tile cross-fade on Dream Land's clouds.
CC_PRIM_LOD_FRAC = 14

# G_ACMUX, the alpha combiner's own operand table. The colour and alpha
# cycles are programmed independently and the stage display lists really do
# disagree: 37 of them compute alpha as TEXEL0 * SHADE, 23 as TEXEL0 alone,
# 5 as SHADE alone. A batch that takes alpha from the vertex when the RDP
# does not is invisible wherever alpha is read.
AC_TEXEL0 = 1
AC_PRIMITIVE = 3
AC_SHADE = 4
AC_ZERO = 7

MOBJ_SEGMENT = 0x0E


class RelocFile(bytes):
    """A relocData file's bytes, remembering which file they are.

    Plain bytes everywhere they are read; the three attributes are what
    lets a reader answer questions the bytes alone cannot -- above all
    "and which file are THIS file's images in?", whose answer is in the
    extern relocation chain and not in the pointer -- see
    extern_texture_files, which is the one reader that needs them.
    Threading that answer by hand through every exporter is how it got
    missed twice, so the file carries it instead.

    (No __slots__: bytes is variable-length, so a subclass of it cannot
    have any, and the three attributes live in an instance dict.)
    """


def get_file(rom, file_id, extract):
    """Decompress relocData `file_id` exactly as the game would (vpk0).

    `extract` is the ssb_extract module (passed in to avoid a hard import).
    """
    entry = extract.read_entry(rom, extract.RELOC_SEG, file_id)
    data_off = extract.rom_table_hi(extract.RELOC_SEG,
                                    extract.FILE_COUNT) + entry["data_offset"]
    blob = rom[data_off:data_off + entry["compressed_size"] * 4]
    if entry["is_compressed"]:
        out = RelocFile(extract.decode_vpk0(blob))
    else:
        out = RelocFile(rom[data_off:
                            data_off + entry["decompressed_size"] * 4])
    out.rom, out.fid, out.extract = rom, file_id, extract
    return out


def walk_reloc(filedata, reloc_intern):
    """Walk the intern reloc chain; return {location_byte: target_byte}.

    Each chain descriptor is {reloc_next u16, words_num u16} living at the
    location to patch; words_num is the target offset / 4.
    """
    reloc = {}
    idx = reloc_intern
    while idx != 0xFFFF:
        nxt, wn = struct.unpack_from(">HH", filedata, idx * 4)
        reloc[idx * 4] = wn * 4
        idx = nxt
    return reloc


def walk_reloc_extern(rom, extract, file_id, filedata):
    """Walk the EXTERN reloc chain; return {location_byte: (file_id, byte)}.

    lb/lbreloc.c:165-204. The chain is the same shape as the intern one --
    an LBRelocDesc {u16 reloc_next, u16 words_num} living at the location
    to patch, `words_num` the target offset in words -- but the target
    lives in ANOTHER relocData file, and which one is not in the chain at
    all: the game reads one u16 file id per link straight out of the ROM,
    from a list that sits immediately after this file's stored data
    (`data_rom_offset += compressed_size * sizeof(u32)`, then two bytes a
    link). So the chain gives the offsets and the ROM list gives the
    files, in lockstep.

    That is why a weapon's WPAttributes.data can point into the fighter's
    Special3 file while the record itself is in Special1, and why the
    (ptr & 0xFFFF) * 4 rule -- which is the right one for an UNRELOCATED
    slot -- decodes those to nonsense.
    """
    entry = extract.read_entry(rom, extract.RELOC_SEG, file_id)
    csr = (extract.rom_table_hi(extract.RELOC_SEG, extract.FILE_COUNT) +
           entry["data_offset"] + entry["compressed_size"] * 4)
    out = {}
    idx = entry["reloc_extern"]
    while idx != 0xFFFF:
        nxt, wn = struct.unpack_from(">HH", filedata, idx * 4)
        (fid,) = struct.unpack_from(">H", rom, csr)
        out[idx * 4] = (fid, wn * 4)
        csr += 2
        idx = nxt
    return out


def extern_texture_files(filedata):
    """Where a file's images are: {target byte offset: the bytes they are in}.

    The answer to the question ptr_off cannot answer. A pointer slot in an
    unrelocated file is an LBRelocDesc (lb/lbtypes.h:15) -- its low half
    the target's word offset, its HIGH half the reloc chain's next-link
    index -- and which file the target is in is not in the slot at all: it
    is one u16 per link in a ROM list the extern chain walks in lockstep
    (walk_reloc_extern). The game never has to ask, because
    lbRelocLoadAndRelocFile patches both chains into real addresses before
    anything draws; an exporter reading the ROM bytes does.

    So a tile whose pointer is an extern site and that is read out of the
    file holding the display list decodes that file's own bytes as texels.
    Empty for a file that names only its own images, which is nearly all of
    them -- Yoshi's Island's four visual layers (file 111 into file 110),
    Fox's Arwing (161 into 109) and its laser (153 into 161) are the whole
    of the game's cross-file texture references.

    `filedata` is a RelocFile, so this needs nothing else from the caller.
    Raises if two files claim one offset, which would make the map itself
    the ambiguity it exists to remove.
    """
    rom, fid, extract = filedata.rom, filedata.fid, filedata.extract
    files, owner, out = {}, {}, {}
    for _site, (tfid, toff) in sorted(
            walk_reloc_extern(rom, extract, fid, filedata).items()):
        if owner.setdefault(toff, tfid) != tfid:
            raise ValueError("file %d's extern chain points at offset 0x%04X "
                             "in both file %d and file %d"
                             % (fid, toff, owner[toff], tfid))
        if tfid not in files:
            files[tfid] = get_file(rom, tfid, extract)
        out[toff] = files[tfid]
    return out


def ptr_off(ptr):
    """SSB pointer -> file byte offset (the deterministic rule)."""
    return (ptr & 0xFFFF) * 4


def s16(b):
    v = (b[0] << 8) | b[1]
    return v - 65536 if v >= 32768 else v


def s8(v):
    return v - 256 if v >= 128 else v


def dl_length(filedata, dl_off, limit=4096):
    """Command count of the display list at `dl_off`, up to and including
    its G_ENDDL. Every F3DEX2 command is 8 bytes, so the opcode is the first
    byte of each octet."""
    for i in range(limit):
        if filedata[dl_off + i * 8] == G_ENDDL:
            return i + 1
    raise ValueError("no G_ENDDL within %d commands of 0x%04X"
                     % (limit, dl_off))


G_DL = 0xDE
# F3DEX2 swaps the two SETOTHERMODE opcodes relative to F3DEX; _L is the
# word the render mode, the blender and the alpha compare live in.
G_SETOTHERMODE_H = 0xE3
G_SETOTHERMODE_L = 0xE2
G_RDPSETOTHERMODE = 0xEF
G_LIGHTING = 0x00020000
G_TEXTURE_GEN = 0x00040000
# F3DEX_GBI_2 numbering (ssb-decomp-re/include/PR/gbi.h:373-375, this
# build's -DF3DEX_GBI_2): the geometry-mode bits sys/rdp.c's
# sSYRdpResetDisplayList sets G_CULL_BACK in by default every frame, so
# a display list that never mentions culling is still asking for it.
G_CULL_FRONT = 0x00000200
G_CULL_BACK = 0x00000400
# The geometry-mode bit the RSP computes per-vertex depth under; the
# RDP's Z compare (RM_Z_CMP below) needs both. sys/rdp.c's reset list
# sets it every frame.
G_ZBUFFER = 0x00000001


class ExtentOverrun(ValueError):
    """A vertex sampled past the clamp extent of a tile baked as a
    repeating period. The caller re-bakes with that tile forced to a
    clamped extent bake on that axis (see bake_retry). `force` is what
    goes in the baker's force_extent set: the tile (tile_id) and the
    axis, "u" or "v"."""

    def __init__(self, img, force, msg):
        super().__init__(msg)
        self.img = img
        self.force = force


def bake_retry(make_baker, run):
    """Run `run(baker)` with fresh bakers from `make_baker(force)` until
    no tile overruns its clamp extent; returns the converged baker."""
    force = set()
    while True:
        baker = make_baker(frozenset(force))
        try:
            run(baker)
            return baker
        except ExtentOverrun as e:
            if e.force in force:
                raise
            force.add(e.force)


def flatten_dl(filedata, dl_off, depth=0, budget=None):
    """Inline nested gsSPDisplayList calls into one command stream.

    Stage geometry nests plain DL calls (a preamble DL calls the mesh DL);
    fighters only ever call into the MObj segment, which the baker applies
    as a material, so those commands survive verbatim. A call (flag 0)
    inlines the sub-list minus its G_ENDDL; a branch (flag 1) is a tail
    jump. The output for a DL with no plain calls is byte-identical to the
    original stream.
    """
    if depth > 8:
        raise ValueError("DL nesting deeper than 8 at 0x%04X" % dl_off)
    if budget is None:
        budget = [8192]
    out = bytearray()
    off = dl_off
    while True:
        budget[0] -= 1
        if budget[0] < 0:
            raise ValueError("flattened DL exceeds budget at 0x%04X"
                             % dl_off)
        op = filedata[off]
        if op == G_ENDDL:
            out += filedata[off:off + 8]
            return bytes(out)
        if op == G_DL:
            flag = filedata[off + 1]
            ptr = struct.unpack_from(">I", filedata, off + 4)[0]
            if (ptr >> 16) == (MOBJ_SEGMENT << 8):
                # A true MObj call is 0x0E000000|(index*8); a stage file's
                # unpatched reloc word can also start with 0x0E, so test
                # the whole high half.
                out += filedata[off:off + 8]
                off += 8
                continue
            sub = ptr_off(ptr)
            if not 0 < sub < len(filedata) - 8:
                raise ValueError("gsSPDisplayList to 0x%08X (off 0x%X) out "
                                 "of file at 0x%04X" % (ptr, sub, off))
            if flag:
                off = sub
                continue
            out += flatten_dl(filedata, sub, depth + 1, budget)[:-8]
            off += 8
            continue
        out += filedata[off:off + 8]
        off += 8


def decode_vtx(filedata, ptr, num, lit=True):
    """Read `num` big-endian Vtx from the pool at `ptr`.

    Returns (x, y, z, s, t, nx, ny, nz, alpha): position as ints, s/t in
    texels (s10.5 -> float), normal as signed bytes, alpha 0..255. With
    lit=False (G_LIGHTING off -- stage geometry) bytes 12..14 are an
    unsigned vertex colour instead of a normal.
    """
    off = ptr_off(ptr)
    out = []
    for i in range(num):
        p = filedata[off + i * 16:off + i * 16 + 16]
        n = (s8(p[12]), s8(p[13]), s8(p[14])) if lit else \
            (p[12], p[13], p[14])
        out.append((s16(p[0:2]), s16(p[2:4]), s16(p[4:6]),
                    s16(p[8:10]) / 32.0, s16(p[10:12]) / 32.0,
                    n[0], n[1], n[2], p[15]))
    return out


# --- row-vector 4x4 matrices, matching sys/matrix.c ------------------------

def mat_ident():
    return [[1.0 if i == j else 0.0 for j in range(4)] for i in range(4)]


def mat_local(t, r, sca=None):
    """syMatrixTraRotRpyRScaF: roll/pitch/yaw radians, translation in row 3,
    then syMatrixRowscaleF -- rows 0..2 scaled by x, y, z, the translation row
    left alone. Scaling row i scales the local axis i, so this is "scale in
    the joint's own frame, then rotate"."""
    sr, cr = math.sin(r[0]), math.cos(r[0])
    sp, cp = math.sin(r[1]), math.cos(r[1])
    sy, cy = math.sin(r[2]), math.cos(r[2])
    m = [
        [cp * cy,                cp * sy,                -sp,     0.0],
        [sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, sr * cp, 0.0],
        [cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp, 0.0],
        [t[0],                   t[1],                   t[2],    1.0],
    ]
    if sca is not None:
        for i in range(3):
            for j in range(4):
                m[i][j] *= sca[i]
    return m


def mat_mul(a, b):
    """Row-vector compose: (v @ a) @ b == v @ mat_mul(a, b)."""
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)]
            for i in range(4)]


def xform_point(m, x, y, z):
    return (x * m[0][0] + y * m[1][0] + z * m[2][0] + m[3][0],
            x * m[0][1] + y * m[1][1] + z * m[2][1] + m[3][1],
            x * m[0][2] + y * m[1][2] + z * m[2][2] + m[3][2])


def xform_dir(m, x, y, z):
    """Rotate a direction (no translation). Only valid where the matrix is
    rigid; a joint carrying a scale needs the rows normalised first."""
    return (x * m[0][0] + y * m[1][0] + z * m[2][0],
            x * m[0][1] + y * m[1][1] + z * m[2][1],
            x * m[0][2] + y * m[1][2] + z * m[2][2])


# --- DObjDesc joint tree ---------------------------------------------------

class DObjNode:
    """One DObjDesc entry, linked into the tree lbCommonSetupTreeDObjs builds."""

    __slots__ = ("index", "joint_id", "dl_off", "dl_pre_off", "translate",
                 "rotate", "scale", "parent", "children", "matrix",
                 "dl_links", "flags")

    def __init__(self, index, joint_id, dl_off, translate, rotate, scale):
        self.index = index
        self.joint_id = joint_id
        self.dl_off = dl_off
        self.dl_pre_off = None
        self.translate = translate
        self.rotate = rotate
        self.scale = scale
        self.parent = None
        self.children = []
        self.matrix = None
        self.dl_links = []   # [(bucket, dl_off)] when the payload is a DObjDLLink array
        self.flags = 0       # the id's high nibble (raw id & 0xF000)


def read_dobj_tree(filedata, reloc, dobj_off, dls_pair=False,
                   dl_links=False):
    """Build the DObj tree from the DObjDesc array at `dobj_off`.

    Mirrors lbCommonSetupTreeDObjs: entries are consumed in order until
    id == DOBJ_ARRAY_MAX; `array_dobjs[id]` is (re)bound to each new node and
    a node's parent is whatever currently occupies `array_dobjs[id - 1]`, so
    a repeated id starts a new limb off the same ancestor. Children are
    appended, which is the order gcDrawDObjTree visits siblings in.

    With `dls_pair` (an FTCommonPart whose flags nibble is 1 -- Yoshi), each
    desc's dl points at Gfx *dls[2] instead of at commands:
    ftDisplayMainDrawDefault draws dls[0] *before* gcPrepDObjMatrix, i.e.
    under the parent's matrix, and dls[1] under the joint's own.

    Returns (root, nodes) where root is a synthetic identity node.
    """
    root = DObjNode(-1, -1, None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                    (1.0, 1.0, 1.0))
    slots = [None] * DOBJ_ARRAY_MAX
    nodes = []
    i = 0
    while True:
        b = dobj_off + i * DOBJDESC_SIZE
        raw_id = struct.unpack_from(">I", filedata, b)[0]
        joint_id = raw_id & 0xFFF
        if joint_id == DOBJ_ARRAY_MAX:
            break
        node = DObjNode(i, joint_id, reloc.get(b + 4),
                        struct.unpack_from(">fff", filedata, b + 8),
                        struct.unpack_from(">fff", filedata, b + 20),
                        struct.unpack_from(">fff", filedata, b + 32))
        node.flags = raw_id & 0xF000
        if dls_pair and node.dl_off is not None:
            pair = node.dl_off
            node.dl_pre_off = reloc.get(pair)
            node.dl_off = reloc.get(pair + 4)
        if dl_links and node.dl_off is not None:
            # DObjDLLink array {s32 list_id; Gfx *dl}, terminated by
            # list_id == the DL-head count (4); gcDrawDObjDLLinks queues
            # each dl into gSYTaskmanDLHeads[list_id], whose render mode
            # the layer's display proc set (0 opaque, 1 xlu).
            arr = node.dl_off
            node.dl_off = None
            k = 0
            while True:
                list_id = struct.unpack_from(">i", filedata, arr + k * 8)[0]
                if list_id == 4:
                    break
                dl = reloc.get(arr + k * 8 + 4)
                if dl is not None:
                    node.dl_links.append((list_id, dl))
                k += 1
                if k > 16:
                    raise ValueError("unterminated DObjDLLink array at "
                                     "0x%04X" % arr)
        parent = root if joint_id == 0 else slots[joint_id - 1]
        if parent is None:
            raise ValueError("DObjDesc[%d] id=%d has no parent in slot %d"
                             % (i, joint_id, joint_id - 1))
        node.parent = parent
        parent.children.append(node)
        slots[joint_id] = node
        nodes.append(node)
        i += 1
    return root, nodes


# --- MObj material chain ---------------------------------------------------

def _ptr_list(reloc, off, extern=None):
    """A NULL-terminated array of relocated pointers, as file offsets.

    `extern` is the file's EXTERNAL relocation chain as {site: target byte
    offset} -- ssb_logicexport.file_info's `sites`, which is already that
    list of pairs. A relocData file may point at another file's data, and
    then the slot is in that chain and not in the intern dict, so a walk
    that consults only the intern dict reads the array as empty. Yoshi's
    Island's two animated MObjSubs are the case this exists for: their
    sprite arrays hold four pointers into file 110 and not one of them is
    an intern relocation. Which file each target lives in is the caller's
    problem (MeshBaker.texture_files already answers it by offset).
    """
    out = []
    while off is not None:
        t = reloc.get(off)
        if t is None and extern is not None:
            t = extern.get(off)
        if t is None:
            break
        out.append(t)
        off += 4
    return out


def _ptr_slots(filedata, reloc, off, extern=None, count=16):
    """Up to `count` pointer slots from `off`, None for a NULL one; the
    run ends early at a word that is neither a pointer nor NULL, or at
    the end of the file."""
    out = []
    while off is not None and len(out) < count and off + 4 <= len(filedata):
        t = reloc.get(off)
        if t is None and extern is not None:
            t = extern.get(off)
        if t is None and struct.unpack_from(">I", filedata, off)[0] != 0:
            break
        out.append(t)
        off += 4
    return out


def read_mobjsub(filedata, reloc, off, extern=None):
    """One MObjSub, with its sprite and palette pointer arrays resolved.

    `extern` is passed through to _ptr_list for the two arrays, which are
    the only fields that can point outside the file.
    """
    return {
        "off": off,
        "fmt": filedata[off + 0x02],
        "siz": filedata[off + 0x03],
        "sprites": _ptr_list(reloc, reloc.get(off + 0x04), extern),
        "unk08": struct.unpack_from(">H", filedata, off + 0x08)[0],
        "unk0A": struct.unpack_from(">H", filedata, off + 0x0A)[0],
        "tile_w": struct.unpack_from(">H", filedata, off + 0x0C)[0],
        "tile_h": struct.unpack_from(">H", filedata, off + 0x0E)[0],
        # gcDrawMObjForDObj branches on this three ways (objdisplay.c:1173,
        # 1355, 1401): 1 halves the U scale for a split tile, 2 measures the
        # tile in texels rather than in TMEM words, anything else is the
        # ordinary case.
        "unk10": struct.unpack_from(">i", filedata, off + 0x10)[0],
        "trau": struct.unpack_from(">f", filedata, off + 0x14)[0],
        "trav": struct.unpack_from(">f", filedata, off + 0x18)[0],
        "scau": struct.unpack_from(">f", filedata, off + 0x1C)[0],
        "scav": struct.unpack_from(">f", filedata, off + 0x20)[0],
        "unk24": struct.unpack_from(">f", filedata, off + 0x24)[0],
        "unk28": struct.unpack_from(">f", filedata, off + 0x28)[0],
        "palettes": _ptr_list(reloc, reloc.get(off + 0x2C), extern),
        # The sprite array slot by slot, holes kept: a face's array is
        # sparse (Kirby's frames 1-4 are NULL and 5-10 are not), so
        # `sprites`, which stops at the first hole, is not the whole of
        # what texture_id_curr can pick. Only a texture part's frames read
        # this (MeshBaker frame_mobjs), and only as far as its motion
        # scripts ask.
        "sprite_slots": _ptr_slots(filedata, reloc, reloc.get(off + 0x04),
                                   extern),
        "flags": struct.unpack_from(">H", filedata, off + 0x30)[0],
        "block_fmt": filedata[off + 0x32],
        "block_siz": filedata[off + 0x33],
        "block_dxt": struct.unpack_from(">H", filedata, off + 0x34)[0],
        "block_rows": struct.unpack_from(">H", filedata, off + 0x36)[0],
        # Tile 1: its size, and the translation gcDrawMObjForDObj moves its
        # origin by (objdisplay.c:1383-1397, `flags & 0x40`). Tile 0 takes
        # trau/trav and unk0C/unk0E; tile 1 takes these. Nothing but a
        # two-tile combiner reads tile 1, which in this game is Dream
        # Land's clouds and nothing else.
        "tile1_w": struct.unpack_from(">H", filedata, off + 0x38)[0],
        "tile1_h": struct.unpack_from(">H", filedata, off + 0x3A)[0],
        "scrollu": struct.unpack_from(">f", filedata, off + 0x3C)[0],
        "scrollv": struct.unpack_from(">f", filedata, off + 0x40)[0],
        "primcolor": struct.unpack_from(">I", filedata, off + 0x50)[0],
        "prim_l": filedata[off + 0x54],
        "envcolor": struct.unpack_from(">I", filedata, off + 0x58)[0],
        "blendcolor": struct.unpack_from(">I", filedata, off + 0x5C)[0],
        "light1": struct.unpack_from(">I", filedata, off + 0x60)[0],
        "light2": struct.unpack_from(">I", filedata, off + 0x64)[0],
        # gcAddMObjForDObj's defaults; the costume script overrides them.
        "texture_id_curr": 0,
        "palette_id": 0,
    }


def read_mobjsubs(filedata, reloc, p_off, count, extern=None):
    """FTCommonPart.p_mobjsubs: `MObjSub ***`, indexed by DObjDesc index.

    Entry i is either NULL or a NULL-terminated `MObjSub *` array; the DObj
    built from DObjDesc[i] gets one MObj per element, in order
    (lbCommonSetupFighterPartsDObjs -> lbCommonAddMObjForFighterPartsDObj).

    `extern` reaches the MObjSubs' own sprite and palette arrays; the
    MObjSub structs themselves are always in this file.
    """
    out = []
    for i in range(count):
        arr = reloc.get(p_off + i * 4)
        out.append([read_mobjsub(filedata, reloc, s, extern)
                    for s in _ptr_list(reloc, arr)])
    return out


# --- costume material animation (AObjEvent32) ------------------------------
#
# The values sitting in an MObjSub are *not* what gets drawn. Every fighter
# MObj is handed a costume script by lbCommonAddMObjForFighterPartsDObj, which
# parses it, plays it once at anim_frame == the costume id, and then drops the
# AObj again. So a costume is "evaluate this script at frame N", and without
# doing that Mario has green sleeves and orange legs -- the raw struct holds
# whatever the last costume in the script left behind.

AOBJ_END = 0
AOBJ_JUMP = 1
AOBJ_WAIT = 2
AOBJ_SETVAL_BLOCK = 3
AOBJ_SETVAL = 4
AOBJ_SETVAL0RATE_BLOCK = 8
AOBJ_SETVAL0RATE = 9
AOBJ_SETVALAFTER_BLOCK = 10
AOBJ_SETVALAFTER = 11
AOBJ_SETANIM = 14
AOBJ_SETEXTVALAFTER_BLOCK = 18
AOBJ_SETEXTVALAFTER = 19
AOBJ_SETEXTVAL_BLOCK = 20
AOBJ_SETEXTVAL = 21

# mat_aobjs[i] drives track nGCAnimTrackMaterialStart + i (sys/objdef.h).
MAT_TRACKS = ["texture_id_curr", "trau", "trav", "scau", "scav",
              "texture_id_next", "scrollu", "scrollv", "lfrac", "palette_id"]
# matspecial_aobjs[i] drives nGCAnimTrackMaterialSubStart + i.
EXT_TRACKS = ["primcolor", "envcolor", "blendcolor", "light1", "light2"]


class _AObj:
    __slots__ = ("kind", "value_base", "value_target", "rate_base",
                 "rate_target", "length", "length_invert")

    def __init__(self):
        self.kind = None
        self.value_base = 0.0
        self.value_target = 0.0
        self.rate_base = 0.0
        self.rate_target = 0.0
        self.length = 0.0
        self.length_invert = 0.0

    def value(self):
        if self.kind == "step":
            return (self.value_target if self.length_invert <= self.length
                    else self.value_base)
        if self.kind == "linear":
            return self.value_base + self.length * self.rate_base
        if self.kind == "cubic":
            # gcPlayDObjAnimJoint's Hermite, written out the same way round.
            li2 = self.length_invert * self.length_invert
            l2 = self.length * self.length
            f18 = self.length_invert * l2
            f14 = self.length * l2 * li2
            f20 = 2.0 * f14 * self.length_invert
            f22 = 3.0 * l2 * li2
            f24 = f14 - f18
            return (self.value_base * ((f20 - f22) + 1.0) +
                    self.value_target * (f22 - f20) +
                    self.rate_base * ((f24 - f18) + self.length) +
                    self.rate_target * f24)
        if self.kind == "linear_color":
            # objanim.c:1350-1389, gcPlayMObjMatAnim's colour lerp, which
            # is not the scalar one: the two endpoints are packed RGBA
            # words and the RSP-era trick interpolates *two channels at a
            # time* in one 32-bit multiply. R and G go into bytes 0 and 2
            # of a scratch word, both products are at most 255 * 256 and
            # so cannot carry into each other, and the result's bytes 0
            # and 2 are the answer; then the same for B and A.
            #
            # `interp` is a 0..256 fraction of the way along, clamped at
            # both ends -- so a track whose command gave it no duration
            # (length_invert 0) simply holds its base colour, which is
            # what a fade's opening SetExtVal does.
            #
            # This raised once: no pack the port had baked
            # animated a colour over a duration, and the dead explosion's
            # four MatAnimJoints are twelve scripts that do nothing else.
            interp = int(self.length * self.length_invert * 256.0)
            interp = 0 if interp < 0 else (256 if interp > 256 else interp)
            base = int(self.value_base) & 0xFFFFFFFF
            target = int(self.value_target) & 0xFFFFFFFF
            out = 0
            for half in (0, 2):
                b = (((base >> (24 - 8 * half)) & 0xFF) << 16) | \
                    ((base >> (16 - 8 * half)) & 0xFF)
                t = (((target >> (24 - 8 * half)) & 0xFF) << 16) | \
                    ((target >> (16 - 8 * half)) & 0xFF)
                v = ((256 - interp) * b + t * interp) & 0xFFFFFFFF
                out |= (((v >> 24) & 0xFF) << (24 - 8 * half)) | \
                       (((v >> 8) & 0xFF) << (16 - 8 * half))
            return out
        raise ValueError("unhandled AObj kind %r" % self.kind)


def read_matanim_table(filedata, reloc, table_off, counts):
    """FTCommonPart.p_costume_matanim_joints: `AObjEvent32 ***` by DObjDesc
    index. The inner array is not NULL-terminated -- the game walks it in
    lockstep with the MObj list -- so `counts[i]` says how far to read."""
    out = []
    for i, n in enumerate(counts):
        cell = reloc.get(table_off + i * 4)
        if cell is not None and n == 0:
            # A row of scripts for a joint with no MObj to run them is a
            # table read from the wrong place: Yoshi's and Luigi's were a
            # row or two early (ssb_meshexport.py
            # symbol_offset and the decomp's PAD()), and every joint took
            # its neighbour's costume.
            raise ValueError("costume table @ 0x%X: row %d has scripts and "
                             "DObjDesc %d has no MObj" % (table_off, i, i))
        out.append([reloc.get(cell + j * 4) for j in range(n)]
                   if cell is not None else [None] * n)
    return out


def play_matanim(filedata, reloc, script_off, anim_frame, anim_speed=1.0):
    """Run a material anim script once, as gcParseMObjMatAnimJoint +
    gcPlayMObjMatAnim do, and return {"mat": {...}, "ext": {...}}.

    anim_wait starts at -anim_frame and each blocking command adds its own
    duration to it, so the parse stops on the command that first pushes the
    clock past the requested frame; the value that survives is the previous
    one. That is the whole costume mechanism.
    """
    def u32(o):
        return struct.unpack_from(">I", filedata, o)[0]

    def f32(o):
        return struct.unpack_from(">f", filedata, o)[0]

    mat, ext = {}, {}
    wait = -float(anim_frame)
    pc = script_off
    ended = False

    for _ in range(4096):
        w = u32(pc)
        op = (w >> 25) & 0x7F
        flags = (w >> 15) & 0x3FF
        payload = float(w & 0x7FFF)

        if op == AOBJ_END:
            for a in list(mat.values()) + list(ext.values()):
                a.length += anim_speed + wait
            ended = True
            break

        if op == AOBJ_WAIT:
            wait += payload
            pc += 4
        elif op in (AOBJ_JUMP, AOBJ_SETANIM):
            target = reloc.get(pc + 4)
            if target is None:
                raise ValueError("unrelocated anim jump at 0x%04X" % pc)
            if op == AOBJ_SETANIM:
                wait = -wait
            pc = target
        elif op in (AOBJ_SETVAL0RATE_BLOCK, AOBJ_SETVAL0RATE):
            # objanim.c:900-932. A cubic that keeps the rate it had and
            # aims at zero -- one value word per set bit, as the SetVal
            # ops have, and material tracks only: there is no Ext
            # spelling of it. The damage sparks' MatAnimJoint opens with
            # one and no costume script has ever used it,
            # which is why it was not here before.
            pc += 4
            bits = flags
            for i in range(len(MAT_TRACKS)):
                if bits == 0:
                    break
                if bits & 1:
                    a = mat.setdefault(i, _AObj())
                    a.value_base = a.value_target
                    a.value_target = f32(pc)
                    pc += 4
                    a.rate_base = a.rate_target
                    a.rate_target = 0.0
                    a.kind = "cubic"
                    if payload != 0.0:
                        a.length_invert = 1.0 / payload
                    a.length = -wait - anim_speed
                bits >>= 1
            if op == AOBJ_SETVAL0RATE_BLOCK:
                wait += payload
        elif op in (AOBJ_SETVALAFTER_BLOCK, AOBJ_SETVALAFTER,
                    AOBJ_SETVAL_BLOCK, AOBJ_SETVAL,
                    AOBJ_SETEXTVALAFTER_BLOCK, AOBJ_SETEXTVALAFTER,
                    AOBJ_SETEXTVAL_BLOCK, AOBJ_SETEXTVAL):
            is_ext = op >= AOBJ_SETEXTVALAFTER_BLOCK
            is_step = op in (AOBJ_SETVALAFTER_BLOCK, AOBJ_SETVALAFTER,
                             AOBJ_SETEXTVALAFTER_BLOCK, AOBJ_SETEXTVALAFTER)
            blocks = op in (AOBJ_SETVALAFTER_BLOCK, AOBJ_SETVAL_BLOCK,
                            AOBJ_SETEXTVALAFTER_BLOCK, AOBJ_SETEXTVAL_BLOCK)
            table = ext if is_ext else mat
            ntracks = len(EXT_TRACKS) if is_ext else len(MAT_TRACKS)
            # `payload` comes from this word and `flags` from the same word
            # (AObjAnimAdvance is a post-increment), then one value word
            # follows per set flag bit.
            pc += 4
            bits = flags
            for i in range(ntracks):
                if bits == 0:
                    break
                if bits & 1:
                    a = table.setdefault(i, _AObj())
                    a.value_base = a.value_target
                    a.value_target = u32(pc) if is_ext else f32(pc)
                    pc += 4
                    a.length = -wait - anim_speed
                    if is_step:
                        a.kind = "step"
                        a.length_invert = payload
                        a.rate_target = 0.0
                    elif is_ext:
                        a.kind = "linear_color"
                        if payload != 0.0:
                            a.length_invert = 1.0 / payload
                    else:
                        a.kind = "linear"
                        if payload != 0.0:
                            a.rate_base = ((a.value_target - a.value_base)
                                           / payload)
                        a.rate_target = 0.0
                bits >>= 1
            if blocks:
                wait += payload
        else:
            raise ValueError("unhandled AObjEvent32 opcode %d at 0x%04X"
                             % (op, pc))

        if wait > 0.0:
            break
    else:
        raise ValueError("material anim script at 0x%04X did not terminate"
                         % script_off)

    out = {"mat": {}, "ext": {}}
    for table, dest in ((mat, out["mat"]), (ext, out["ext"])):
        for i, a in table.items():
            if not ended:
                a.length += anim_speed
            dest[i] = a.value()
    return out


# --- Figatree joint animation (AObjEvent16) --------------------------------
#
# A fighter animation is its own relocData file: a table of AObjEvent16 script
# pointers, one per DObj in draw order, then the scripts. Each command word is
# {opcode:5, flags:10, toggle:1}; `flags` is a bitmask of the ten joint tracks
# and one s16 follows per set bit (two for the Rate forms). Values are fixed
# point, scaled per track by ftAnimGetTargetValue.

FIG_END = 0
FIG_BLOCK = 1
FIG_SETVAL_BLOCK = 2
FIG_SETVAL = 3
FIG_SETVALRATE_BLOCK = 4
FIG_SETVALRATE = 5
FIG_SETTARGETRATE = 6
FIG_SETVAL0RATE_BLOCK = 7
FIG_SETVAL0RATE = 8
FIG_SETVALAFTER_BLOCK = 9
FIG_SETVALAFTER = 10
FIG_ADDLENGTH = 11
FIG_SETTRANSLATEINTERP = 12
FIG_LOOP = 13
FIG_SETFLAGS = 14

# mat_aobjs-style track order: nGCAnimTrackJointStart + i.
JOINT_TRACKS = ["rx", "ry", "rz", "tri", "tx", "ty", "tz", "sx", "sy", "sz"]

# ftAnimGetTargetValue's fracs[]: the s16 payloads are fixed point, and the
# scale differs per track *and* between a value and a rate.
#
# The TraI scale is the odd one. ftanim.c:24 writes it as
# `1.0F / 16384.0F - (3.0F / 1000000000000.0F)` -- with a `// why tho` next
# to it -- and every literal there is a float, so the subtraction happens in
# float and the constant the hardware actually holds is 6.103515261202119e-05.
# Read as a double the same text means 6.103515325e-05, which is a number the
# game never has: the fudge is a little over half an ulp at 2^-14, so in float
# it moves the constant down exactly one step and no further. Rounding it here
# is what lets tools/check/figatree_check.py demand *exact* agreement from a
# double build -- otherwise every TraI track is out by 3e-12 times its payload,
# which is what caught this.
_FIG_TRI_SCALE = struct.unpack("<f", struct.pack("<f",
                                                 1.0 / 16384.0 - 3.0e-12))[0]

_FIG_VALUE_SCALE = {"rx": 1.0 / 512.0, "ry": 1.0 / 512.0, "rz": 1.0 / 512.0,
                    "tx": 1.0 / 4.0, "ty": 1.0 / 4.0, "tz": 1.0 / 4.0,
                    "sx": 1.0 / 4096.0, "sy": 1.0 / 4096.0, "sz": 1.0 / 4096.0,
                    "tri": _FIG_TRI_SCALE}
_FIG_RATE_SCALE = {"rx": 1.0 / 512.0, "ry": 1.0 / 512.0, "rz": 1.0 / 512.0,
                   "tx": 1.0 / 32.0, "ty": 1.0 / 32.0, "tz": 1.0 / 32.0,
                   "sx": 1.0 / 8192.0, "sy": 1.0 / 8192.0, "sz": 1.0 / 8192.0,
                   "tri": _FIG_TRI_SCALE}


class Figatree:
    """One joint's animation script, stepped a frame at a time.

    Mirrors ftAnimParseDObjFigatree followed by gcPlayDObjAnimJoint: parse
    advances the clock and runs commands until the next one would overshoot
    the frame, play evaluates each live track's interpolator. Tracks the
    script never touches are simply absent from the result, and keep whatever
    the DObjDesc bind pose gave them.
    """

    def __init__(self, filedata, word_off, start_frame=0.0, speed=1.0):
        self.f = filedata
        self.pc = word_off
        self.start = word_off
        self.speed = speed
        self.frame = start_frame
        self.wait = None          # None stands in for AOBJ_ANIM_CHANGED
        self.ended = False
        self.dead = False
        self.aobjs = {}
        self.loops = 0

    def _u16(self, i):
        return struct.unpack_from(">H", self.f, i * 2)[0]

    def _s16(self, i):
        return struct.unpack_from(">h", self.f, i * 2)[0]

    def _track(self, i):
        a = self.aobjs.get(i)
        if a is None:
            a = self.aobjs[i] = _AObj()
        return a

    def _parse(self):
        if self.dead:
            return
        if self.wait is None:
            self.wait = -self.frame
        else:
            self.wait -= self.speed
            self.frame += self.speed
            if self.wait > 0.0:
                return

        for _ in range(4096):
            w = self._u16(self.pc)
            op = (w >> 11) & 0x1F
            flags = (w >> 1) & 0x3FF
            toggle = w & 1

            if op == FIG_END:
                for a in self.aobjs.values():
                    if a.kind is not None:
                        a.length += self.speed + self.wait
                self.frame = self.wait
                self.ended = True
                return

            if op > FIG_SETFLAGS:
                # AObjEvent16Kind stops at 14 and the opcode field is five
                # bits wide, so anything above it is not a command at all.
                # The game's own parser has no case for it either, and its
                # `default: break` does not advance the script pointer, so
                # ftAnimParseDObjFigatree spins on exactly this. Stop, and
                # let the caller report the joint rather than invent a
                # reading of bytes that are not code. (Every slot in the
                # ROM that used to land here was an AnimJoint script being
                # read as a figatree -- see the AnimJoint class below.)
                raise ValueError("opcode %d at word %d is not an "
                                 "AObjEvent16Kind" % (op, self.pc))
            if op == FIG_LOOP:
                self.pc += 1
                self.pc += self._s16(self.pc) // 2
                self.frame = -self.wait
                self.loops += 1
            elif op == FIG_SETTRANSLATEINTERP:
                raise ValueError("SetTranslateInterp (TraI spline) at word "
                                 "%d is not implemented" % self.pc)
            else:
                # `payload` and `flags` both come out of the command word --
                # AObjAnimAdvance is a post-increment -- and the duration only
                # follows when the toggle bit is set.
                self.pc += 1
                payload = 0.0
                if toggle:
                    payload = float(self._u16(self.pc))
                    self.pc += 1

                if op in (FIG_BLOCK, FIG_SETFLAGS):
                    # SetFlags sets DObj->flags -- which nothing here reads --
                    # and then blocks for its payload exactly as Block does.
                    self.wait += payload
                else:
                    self._tracks(op, flags, payload)
                    if op in (FIG_SETVAL_BLOCK, FIG_SETVALRATE_BLOCK,
                              FIG_SETVAL0RATE_BLOCK, FIG_SETVALAFTER_BLOCK):
                        self.wait += payload

            if self.wait > 0.0:
                return
        raise ValueError("figatree at word %d did not terminate" % self.start)

    def _tracks(self, op, flags, payload):
        for i in range(len(JOINT_TRACKS)):
            if flags == 0:
                break
            if flags & 1:
                name = JOINT_TRACKS[i]
                a = self._track(i)
                if op == FIG_ADDLENGTH:
                    a.length += payload
                    flags >>= 1
                    continue
                if op == FIG_SETTARGETRATE:
                    a.rate_target = (self._s16(self.pc) *
                                     _FIG_RATE_SCALE[name])
                    self.pc += 1
                    flags >>= 1
                    continue

                a.value_base = a.value_target
                a.value_target = self._s16(self.pc) * _FIG_VALUE_SCALE[name]
                self.pc += 1
                a.length = -self.wait - self.speed

                if op in (FIG_SETVALAFTER_BLOCK, FIG_SETVALAFTER):
                    a.kind = "step"
                    a.length_invert = payload
                    a.rate_target = 0.0
                elif op in (FIG_SETVAL_BLOCK, FIG_SETVAL):
                    a.kind = "linear"
                    if payload != 0.0:
                        a.rate_base = ((a.value_target - a.value_base) /
                                       payload)
                    a.rate_target = 0.0
                else:                       # SetValRate / SetVal0Rate, cubic
                    a.kind = "cubic"
                    a.rate_base = a.rate_target
                    if op in (FIG_SETVALRATE_BLOCK, FIG_SETVALRATE):
                        a.rate_target = (self._s16(self.pc) *
                                         _FIG_RATE_SCALE[name])
                        self.pc += 1
                    else:
                        a.rate_target = 0.0
                    if payload != 0.0:
                        a.length_invert = 1.0 / payload
            flags >>= 1

    def _play(self):
        out = {}
        if self.dead:
            return out
        for i, a in self.aobjs.items():
            if a.kind is None:
                continue
            if not self.ended:
                a.length += self.speed
            out[JOINT_TRACKS[i]] = a.value()
        if self.ended:
            self.dead = True
        return out

    def step(self):
        """Advance one frame; returns {track name: value} for live tracks."""
        self._parse()
        return self._play()


# --- AnimJoint joint animation (AObjEvent32) --------------------------------
#
# Not every fighter animation is a figatree. FTMotionDesc's anim_desc has a
# bit for it -- FTANIM_FLAG_ANIMJOINT, "whether current animation is type
# Figatree (0) or AnimJoint (1)" (ft/fttypes.h:59) -- and ftParamUpdateAnimKeys
# (ft/ftparam.c:412) parses a joint with gcParseDObjAnimJoint instead of
# ftAnimParseDObjFigatree when it is set. Thirty-five animations carry it:
# every Appear (the warp-in at match start), Kirby's DK stare, Captain's Blue
# Falcon. Their files have the same shape -- a pointer table, one entry per
# DObj in draw order, then the scripts -- but the scripts are the 32-bit
# AObjEvent32 language every non-fighter DObj in the game animates with:
# {opcode:7, flags:10, payload:15} per word, f32 values in the stream, and
# Jump/SetAnim/SetInterp naming their target by a relocated pointer. Read as
# 16-bit figatrees they parse plausibly for a while and then hit a word that
# is not a command, which is how they were found.
#
# This mirrors gcParseDObjAnimJoint (sys/objanim.c:268-633) followed by
# gcPlayDObjAnimJoint, the same way Figatree mirrors its two functions, and
# shares its AObj evaluator. The func_anim hooks (End, Jump, SetAnim, and the
# two numbered commands) are consumed but not modelled: they reach the GObj's
# callback, and a fighter GObj has none.

AJ_END = 0
AJ_JUMP = 1
AJ_WAIT = 2
AJ_SETVAL_BLOCK = 3
AJ_SETVAL = 4
AJ_SETVALRATE_BLOCK = 5
AJ_SETVALRATE = 6
AJ_SETTARGETRATE = 7
AJ_SETVAL0RATE_BLOCK = 8
AJ_SETVAL0RATE = 9
AJ_SETVALAFTER_BLOCK = 10
AJ_SETVALAFTER = 11
AJ_ADDLENGTH = 12           # ANIM_CMD_12 in the decomp
AJ_SETINTERP = 13
AJ_SETANIM = 14
AJ_SETFLAGS = 15
AJ_FUNCANIM = 16            # ANIM_CMD_16: func_anim(dobj, flags >> 8, flags & 0xFF)
AJ_FUNCANIM_TRACKS = 17     # ANIM_CMD_17: func_anim per flag bit, one f32 each

AJ_SETEXTVALAFTER_BLOCK = 18
AJ_SETEXTVALAFTER = 19
AJ_SETEXTVAL_BLOCK = 20
AJ_SETEXTVAL = 21

_AJ_TRACK_OPS = (AJ_SETVAL_BLOCK, AJ_SETVAL, AJ_SETVALRATE_BLOCK,
                 AJ_SETVALRATE, AJ_SETTARGETRATE, AJ_SETVAL0RATE_BLOCK,
                 AJ_SETVAL0RATE, AJ_SETVALAFTER_BLOCK, AJ_SETVALAFTER,
                 AJ_ADDLENGTH)
# The four the *material* parser has and the joint parser does not
# (objanim.c:1131-1200 gcParseMObjMatAnimJoint). One value word per flag
# bit, as the SetVal ops have, except that the word is a packed RGBA
# colour rather than an f32 -- which matters to a reader of the value and
# not to a walk of the script, so they count the same here.
_AJ_MAT_EXT_OPS = (AJ_SETEXTVALAFTER_BLOCK, AJ_SETEXTVALAFTER,
                   AJ_SETEXTVAL_BLOCK, AJ_SETEXTVAL)
_AJ_BLOCK_OPS = (AJ_SETVAL_BLOCK, AJ_SETVALRATE_BLOCK, AJ_SETVAL0RATE_BLOCK,
                 AJ_SETVALAFTER_BLOCK)


def _aj_decode(w):
    """(opcode, flags, payload) of one AObjEvent32 command word."""
    return (w >> 25) & 0x7F, (w >> 15) & 0x3FF, float(w & 0x7FFF)


class AnimJoint:
    """One joint's AObjEvent32 script, stepped a frame at a time; the same
    protocol as Figatree (step() -> {track: value}), over byte offsets
    because the scripts hold pointers that only the file's reloc map can
    resolve."""

    def __init__(self, filedata, reloc, byte_off, start_frame=0.0,
                 speed=1.0):
        self.f = filedata
        self.reloc = reloc
        self.pc = byte_off
        self.start = byte_off
        self.speed = speed
        self.frame = start_frame
        self.wait = None          # None stands in for AOBJ_ANIM_CHANGED
        self.ended = False
        self.dead = False
        self.aobjs = {}
        self.loops = 0

    def _u32(self, o):
        return struct.unpack_from(">I", self.f, o)[0]

    def _f32(self, o):
        return struct.unpack_from(">f", self.f, o)[0]

    def _target(self, o):
        t = self.reloc.get(o)
        if t is None:
            raise ValueError("unrelocated pointer at byte 0x%04X" % o)
        return t

    def _track(self, i):
        a = self.aobjs.get(i)
        if a is None:
            a = self.aobjs[i] = _AObj()
        return a

    def _parse(self):
        if self.dead:
            return
        if self.wait is None:
            self.wait = -self.frame
        else:
            self.wait -= self.speed
            self.frame += self.speed
            if self.wait > 0.0:
                return

        for _ in range(4096):
            op, flags, payload = _aj_decode(self._u32(self.pc))

            if op == AJ_END:
                for a in self.aobjs.values():
                    if a.kind is not None:
                        a.length += self.speed + self.wait
                self.frame = self.wait
                self.ended = True
                return

            if op in _AJ_TRACK_OPS:
                # payload and flags both come out of the command word --
                # AObjAnimAdvance is a post-increment -- and the values
                # follow, one f32 per set flag bit (two for SetValRate).
                self.pc += 4
                self._tracks(op, flags, payload)
                if op in _AJ_BLOCK_OPS:
                    self.wait += payload
            elif op in (AJ_WAIT, AJ_SETFLAGS, AJ_FUNCANIM):
                # SetFlags writes DObj->flags and Cmd16 calls func_anim;
                # neither moves a track, and both then wait their payload.
                self.wait += payload
                self.pc += 4
            elif op == AJ_FUNCANIM_TRACKS:
                self.wait += payload
                self.pc += 4
                bits = flags
                for _i in range(4, 14):
                    if bits == 0:
                        break
                    if bits & 1:
                        self.pc += 4    # the f32 handed to func_anim
                    bits >>= 1
            elif op in (AJ_JUMP, AJ_SETANIM):
                target = self._target(self.pc + 4)
                if op == AJ_SETANIM:
                    self.frame = -self.wait
                self.loops += 1
                self.pc = target
            elif op == AJ_SETINTERP:
                raise ValueError("SetInterp (TraI spline) at byte 0x%04X is "
                                 "not implemented" % self.pc)
            else:
                # 18..21 are the material SetExtVal forms, which
                # gcParseDObjAnimJoint has no case for either, and 22/23
                # are reserved; its `default: break` spins on all of them.
                raise ValueError("opcode %d at byte 0x%04X is not a DObj "
                                 "AObjEvent32Kind" % (op, self.pc))

            if self.wait > 0.0:
                return
        raise ValueError("anim joint at byte 0x%04X did not terminate"
                         % self.start)

    def _tracks(self, op, flags, payload):
        for i in range(len(JOINT_TRACKS)):
            if flags == 0:
                break
            if flags & 1:
                a = self._track(i)
                if op == AJ_ADDLENGTH:
                    a.length += payload
                    flags >>= 1
                    continue
                if op == AJ_SETTARGETRATE:
                    a.rate_target = self._f32(self.pc)
                    self.pc += 4
                    flags >>= 1
                    continue
                a.value_base = a.value_target
                a.value_target = self._f32(self.pc)
                self.pc += 4
                a.length = -self.wait - self.speed

                if op in (AJ_SETVALAFTER_BLOCK, AJ_SETVALAFTER):
                    a.kind = "step"
                    a.length_invert = payload
                    a.rate_target = 0.0
                elif op in (AJ_SETVAL_BLOCK, AJ_SETVAL):
                    a.kind = "linear"
                    if payload != 0.0:
                        a.rate_base = ((a.value_target - a.value_base) /
                                       payload)
                    a.rate_target = 0.0
                else:                       # SetValRate / SetVal0Rate, cubic
                    a.kind = "cubic"
                    a.rate_base = a.rate_target
                    if op in (AJ_SETVALRATE_BLOCK, AJ_SETVALRATE):
                        a.rate_target = self._f32(self.pc)
                        self.pc += 4
                    else:
                        a.rate_target = 0.0
                    if payload != 0.0:
                        a.length_invert = 1.0 / payload
            flags >>= 1

    def _play(self):
        out = {}
        if self.dead:
            return out
        for i, a in self.aobjs.items():
            if a.kind is None:
                continue
            if not self.ended:
                a.length += self.speed
            out[JOINT_TRACKS[i]] = a.value()
        if self.ended:
            self.dead = True
        return out

    def step(self):
        """Advance one frame; returns {track name: value} for live tracks."""
        self._parse()
        return self._play()


# --- static walks: what a script can reach, without running it -------------
#
# The two parsers end in `default: break`, which does not advance the script
# pointer, so an opcode neither has a case for puts the game into a loop that
# never ends. The ROM's own data never does that -- it is the game's data --
# but a pack is the port's data, so tools/export/ssb_packexport.py walks every
# script it ships, once, following every jump, and refuses an animation that
# could reach such a word. That is the hang guard: at export, where the data is
# made, rather than in a parser this project runs unmodified.
#
# The AnimJoint walk also reports which words are f32 values, which the host
# cross-check needs: its double-precision build widens f32 to double, so the
# oracle has to know which stream words to widen (tools/check/objanim_oracle.c).

def figatree_walk(filedata, word_off, splines=None):
    """Follow a figatree script from `word_off` until it ends or revisits a
    word. Raises ValueError on an undefined opcode or a read past the file;
    returns the set of word offsets visited. With `splines`, the byte offset
    of each SYInterpDesc a SetTranslateInterp names is added to it
    (ft/ftanim.c:350: the delta word's own address plus its s16 over two,
    in words, the division C's)."""
    nwords = len(filedata) // 2
    seen = set()
    pc = word_off
    while True:
        if pc < 0 or pc >= nwords:
            raise ValueError("figatree script reads word %d of %d"
                             % (pc, nwords))
        if pc in seen:
            return seen
        seen.add(pc)
        w = struct.unpack_from(">H", filedata, pc * 2)[0]
        op = (w >> 11) & 0x1F
        flags = (w >> 1) & 0x3FF
        toggle = w & 1
        if op == FIG_END:
            return seen
        if op > FIG_SETFLAGS:
            raise ValueError("opcode %d at word %d is not an AObjEvent16Kind"
                             % (op, pc))
        if op == FIG_LOOP:
            if pc + 1 >= nwords:
                raise ValueError("figatree Loop at word %d has no delta" % pc)
            seen.add(pc + 1)
            pc = pc + 1 + struct.unpack_from(">h", filedata,
                                             (pc + 1) * 2)[0] // 2
            continue
        pc += 1
        if op == FIG_SETTRANSLATEINTERP:
            if splines is not None and pc < nwords:
                d = struct.unpack_from(">h", filedata, pc * 2)[0]
                splines.add((pc + (d // 2 if d >= 0 else -(-d // 2))) * 2)
            pc += 1                     # the spline's s16 delta
            continue
        if toggle:
            pc += 1                     # the duration payload
        if op in (FIG_BLOCK, FIG_SETFLAGS, FIG_ADDLENGTH):
            continue
        per_bit = 2 if op in (FIG_SETVALRATE_BLOCK, FIG_SETVALRATE) else 1
        pc += per_bit * bin(flags).count("1")


def animjoint_walk(filedata, reloc, byte_off, floats=None, mat=False):
    """Follow an AnimJoint script from `byte_off` until it ends or revisits
    a word. Raises ValueError on an undefined opcode, an unrelocated
    pointer or a read past the file; returns (words visited, f32 value
    words), both as sets of byte offsets. `floats` may be a set to add
    to, so that what was found before a refusal is kept.

    `mat` walks it as a MatAnimJoint instead: the same stream read by
    objanim.c's gcParseMObjMatAnimJoint rather than its joint parser, so
    the SetExtVal* opcodes are defined and SetInterp/SetFlags/the two
    FuncAnims are not. Which parser a script belongs to is decided by the
    list it is registered on and by nothing in the script itself
    (sys/objdef.h:369-374), so the caller has to say."""
    n = len(filedata)
    seen = set()
    if floats is None:
        floats = set()
    pc = byte_off
    while True:
        if pc < 0 or pc + 4 > n:
            raise ValueError("anim joint script reads byte 0x%04X of 0x%04X"
                             % (pc, n))
        if pc in seen:
            return seen, floats
        seen.add(pc)
        op, flags, _ = _aj_decode(struct.unpack_from(">I", filedata, pc)[0])
        if op == AJ_END:
            return seen, floats
        if op in (AJ_JUMP, AJ_SETANIM):
            target = reloc.get(pc + 4)
            if target is None:
                raise ValueError("unrelocated pointer at byte 0x%04X"
                                 % (pc + 4))
            seen.add(pc + 4)
            pc = target
            continue
        if op == AJ_SETINTERP and not mat:
            if reloc.get(pc + 4) is None:
                raise ValueError("unrelocated pointer at byte 0x%04X"
                                 % (pc + 4))
            seen.add(pc + 4)
            pc += 8
            continue
        pc += 4
        if op == AJ_WAIT or op == AJ_ADDLENGTH or \
                (not mat and op in (AJ_SETFLAGS, AJ_FUNCANIM)):
            continue
        if op == AJ_FUNCANIM_TRACKS and not mat:
            nvals = bin(flags & 0x3FF).count("1")
        elif op in _AJ_TRACK_OPS or (mat and op in _AJ_MAT_EXT_OPS):
            per_bit = 2 if op in (AJ_SETVALRATE_BLOCK, AJ_SETVALRATE) else 1
            nvals = per_bit * bin(flags).count("1")
        else:
            raise ValueError("opcode %d at byte 0x%04X is not a %s "
                             "AObjEvent32Kind"
                             % (op, pc - 4, "MObj" if mat else "DObj"))
        for k in range(nvals):
            floats.add(pc + 4 * k)
        pc += 4 * nvals


# MAT_TRACKS indices the two frame-selecting tracks sit at. A MatAnimJoint
# that drives either is asking the pack for more than one picture.
MATANIM_BIT_TEXID = 0           # nGCAnimTrackTextureIDCurrent
MATANIM_BIT_TEXID_NEXT = 5      # nGCAnimTrackTextureIDNext -- the second
                                # tile of a cross-fade, out of the SAME
                                # sprite array as the first
MATANIM_BIT_PALETTEID = 9       # nGCAnimTrackPaletteID


def matanim_frame_ids(filedata, reloc, script, bit=MATANIM_BIT_TEXID):
    """Every value one MatAnimJoint script gives one frame track.

    `bit` is an index into MAT_TRACKS: 0 for texture_id_curr (a sprite
    array) or 9 for palette_id (a palette array). The two are the same
    question asked of different arrays -- gcDrawMObjForDObj indexes
    `sprites[texture_id_curr]` in one case and `palettes[palette_id]` in
    the other -- so one walk answers both.

    NEITHER ARRAY SAYS HOW LONG IT IS. The game reads it and nothing
    counts, and the arrays of a tree's MObjs sit end to end, so a
    NULL-terminated read of one runs straight into the next: Brinstar's
    eighteen MObjs read 55, 52, 48, 44, ... palettes where they have 3, 4,
    4, 2, ... What bounds the array is the SCRIPT -- the pack carries as
    many frames as the animation ever asks for, and the array is only
    checked to be at least that long.

    Only the SetVal family is inspected: the SetExtVal opcodes' flags
    address EXT_TRACKS (the colours) and not MAT_TRACKS, so a bit index
    means something else there.
    """
    ids, pc, seen = set(), script, set()
    while True:
        if pc in seen:
            break
        seen.add(pc)
        op, flags, _ = _aj_decode(struct.unpack_from(">I", filedata, pc)[0])
        if op == AJ_END:
            break
        if op in (AJ_JUMP, AJ_SETANIM):
            pc = reloc[pc + 4]
            continue
        pc += 4
        if op in (AJ_WAIT, AJ_ADDLENGTH):
            continue
        per_bit = 2 if op in (AJ_SETVALRATE_BLOCK, AJ_SETVALRATE) else 1
        n = 0
        for b in range(len(MAT_TRACKS)):
            if flags & (1 << b):
                if op in _AJ_TRACK_OPS and b == bit:
                    ids.add(struct.unpack_from(">f", filedata, pc + 4 * n)[0])
                n += per_bit
        pc += 4 * n
    return ids


def matanim_tracks(filedata, reloc, script):
    """The MAT_TRACKS names one MatAnimJoint script ever writes.

    matanim_frame_ids' walk, asking which tracks rather than which values.
    Only the SetVal family is counted, for the reason given there.
    """
    names, pc, seen = set(), script, set()
    while pc not in seen:
        seen.add(pc)
        op, flags, _ = _aj_decode(struct.unpack_from(">I", filedata, pc)[0])
        if op == AJ_END:
            break
        if op in (AJ_JUMP, AJ_SETANIM):
            pc = reloc[pc + 4]
            continue
        pc += 4
        if op in (AJ_WAIT, AJ_ADDLENGTH):
            continue
        per_bit = 2 if op in (AJ_SETVALRATE_BLOCK, AJ_SETVALRATE) else 1
        n = 0
        for b in range(len(MAT_TRACKS)):
            if flags & (1 << b):
                if op in _AJ_TRACK_OPS:
                    names.add(MAT_TRACKS[b])
                n += per_bit
        pc += 4 * n
    return names


def matanim_frame_count(filedata, reloc, script, bit=MATANIM_BIT_TEXID):
    """How many frames a MatAnimJoint script's frame track needs, or 0.

    The ids are floats the parser interpolates, so the count is the
    highest one it ever reaches, plus one. A script that never touches the
    track wants the one picture the bake already interned, and says 0 so
    the caller can leave tex_count at 1.
    """
    ids = matanim_frame_ids(filedata, reloc, script, bit)
    if not ids:
        return 0
    top = max(ids)
    if top < 0.0:
        return 0
    return int(top) + 1


def figatree_slots(reloc):
    """How many entries an animation file's header table has.

    The table is the only thing before the scripts, and every table entry is
    a relocated pointer to a script, so the lowest pointer target *is* the
    end of the table. Varies per animation, not per fighter: a file's table
    is only as long as the highest DObj index the animation drives.
    """
    return min(reloc.values()) // 4


def read_figatree_table(filedata, reloc, count):
    """An animation file's header: `count` AObjEvent16 * entries, indexed by
    DObj draw order. Returns word offsets (or None), since the scripts are
    u16 arrays."""
    out = []
    for i in range(count):
        t = reloc.get(i * 4)
        out.append(None if t is None else t // 2)
    return out


def apply_costume(filedata, reloc, subs, scripts, costume):
    """Fold each MObj's costume script into its MObjSub, in place.

    `subs` and `scripts` are the per-DObj lists from read_mobjsubs() and
    read_matanim_table(); a NULL script leaves the struct's own values.
    """
    for sub, script in zip(subs, scripts):
        if script is None:
            continue
        res = play_matanim(filedata, reloc, script, costume)
        for i, v in res["mat"].items():
            name = MAT_TRACKS[i]
            sub[name] = int(v) if name.endswith("_id") or \
                name.endswith("_curr") or name.endswith("_next") else v
        for i, v in res["ext"].items():
            sub[EXT_TRACKS[i]] = v & 0xFFFFFFFF


# --- N64 texel formats, decoded offline ------------------------------------

# Bits per texel, and texels per 64-bit word of a tile's `line`. 32-bit images
# are described with G_IM_SIZ_32b_LINE_BYTES == 2 (gbi.h), i.e. counted as
# 16-bit for `line`, so they share 16-bit's four texels per word.
BPP = {0: 4, 1: 8, 2: 16, 3: 32}
LINE_TEXELS = {0: 16, 1: 8, 2: 4, 3: 4}

# (fmt, siz) the RDP defines. G_IM_FMT_YUV (1) is not one the game uses.
N64_FORMATS = {
    (0, 2): "RGBA16", (0, 3): "RGBA32",
    (2, 0): "CI4", (2, 1): "CI8",
    (3, 0): "IA4", (3, 1): "IA8", (3, 2): "IA16",
    (4, 0): "I4", (4, 1): "I8",
}


def _rgba5551(val):
    """One N64 RGBA5551 word -> (r, g, b, a), 8 bits per channel."""
    return (((val >> 11) & 0x1F) << 3, ((val >> 6) & 0x1F) << 3,
            ((val >> 1) & 0x1F) << 3, 0xFF if (val & 1) else 0x00)


def n64_texel(data, base, k, fmt, siz, tlut=(), stride=0):
    """Texel `k` of the image at `base` as (r, g, b, a), 8 bits a channel.

    The per-texel half of the decomp's tools/relocSpriteTool.py n64_to_rgba
    (lines 297-424): the same nine format/size cases with the same bit
    expansions, read one texel at a time because the baker fetches through
    the RDP's clamp/mirror/wrap remap rather than row by row. `tlut` is the
    palette as raw N64 RGBA5551 words, for the CI formats.

    The unswizzle half of n64_to_rgba is deliberately not copied, and this
    is the rule for the whole model path: a texture a display list brings
    in with gDPLoadTextureBlock is stored in ROM as a plain linear raster,
    because the RDP's load-time odd-line word swap is undone by the same
    swap in its texel fetch. A libultra Sprite's bitmaps are the other
    case -- stored pre-shuffled, and saying so with SP_TEXSHUF in
    Sprite.attr -- and they go through ssb_spriteexport.unshuffle_row.
    tools/check/texshuf_check.py holds both halves of that; the CI4 path here
    has rendered correctly on hardware since H1 on exactly this reading.

    RGBA32 IS NOT AN EXCEPTION, and used to be one here. This function
    carried an odd-row `col ^= 1` for siz 3 alone, on the reading that
    "RGBA32 swaps adjacent texels on odd source rows" -- which combed the
    only RGBA32 tiles in the game, Yoshi's Island's five fruit frames
    (image 0x1030 of StageYosterImages, 65x14 at stride 66). Decoded
    three ways and looked at, the swap-free read is the picture and both
    swaps comb it, which is also what the decomp says: relocSpriteTool's
    unswizzle_n64_texture returns RGBA32 untouched. The 16-byte-unit
    swap in ssb_spriteexport.unshuffle_row32 is the SPRITE case and is
    unrelated -- it undoes SP_TEXSHUF storage, not a TMEM load.
    """
    if fmt == 2:                                    # CI
        idx = (data[base + (k >> 1)] >> 4) if (k & 1) == 0 else \
            (data[base + (k >> 1)] & 0xF)
        if siz == 1:
            idx = data[base + k]
        if idx < len(tlut):
            return _rgba5551(tlut[idx])
        i8 = ((idx << 4) | idx) if siz == 0 else idx
        return (i8, i8, i8, 0xFF)
    if fmt == 0:                                    # RGBA
        if siz == 2:
            return _rgba5551(struct.unpack_from(">H", data, base + k * 2)[0])
        # siz 3 (RGBA32) reads linearly like every other model format --
        # see the docstring for the odd-row swap that used to be here.
        r, g, b, a = data[base + k * 4:base + k * 4 + 4]
        return (r, g, b, a)
    if fmt == 3:                                    # IA
        if siz == 0:                                # 3-bit I + 1-bit A
            n = (data[base + (k >> 1)] >> 4) if (k & 1) == 0 else \
                (data[base + (k >> 1)] & 0xF)
            i3, a1 = (n >> 1) & 0x7, n & 0x1
            i8 = (i3 << 5) | (i3 << 2) | (i3 >> 1)
            return (i8, i8, i8, 0xFF if a1 else 0x00)
        if siz == 1:                                # 4-bit I + 4-bit A
            byte = data[base + k]
            i4, a4 = (byte >> 4) & 0xF, byte & 0xF
            return ((i4 << 4) | i4,) * 3 + ((a4 << 4) | a4,)
        i8, a8 = data[base + k * 2], data[base + k * 2 + 1]
        return (i8, i8, i8, a8)
    if fmt == 4:                                    # I
        if siz == 0:
            i4 = (data[base + (k >> 1)] >> 4) if (k & 1) == 0 else \
                (data[base + (k >> 1)] & 0xF)
            i8 = (i4 << 4) | i4
        else:
            i8 = data[base + k]
        # An I texel is the intensity in all four channels, alpha included:
        # the RDP's texture unit replicates it. The oracle disagrees here
        # and is not the authority -- relocSpriteTool.n64_to_rgba writes
        # PNGs, where an opaque alpha is what a viewer wants (the narrowing
        # is written down in tools/check/texfmt_check.py). The game's own data
        # settles it: the character select's spotlight is a 32x32 I8 tile
        # whose display list computes alpha as TEXEL0 * PRIMITIVE and
        # nothing else (relocData 22), and it is a round soft glow, not the
        # solid square an alpha of 0xFF would draw.
        return (i8, i8, i8, i8)
    raise ValueError("unhandled texture format %d/%d" % (fmt, siz))


# FPackTex.fmt: how the baked texels are encoded for the PVR (fighter.h).
PVRTEX_PAL4 = 0
PVRTEX_ARGB1555 = 1
PVRTEX_ARGB4444 = 2
# The pack carries no texels at all: the runtime binds one it made itself.
PVRTEX_EXTERN = 3

# gsDPSetTextureImage into segment 1. Only lb/lbtransition.c does this: its
# display lists texture from the photocopy of the last frame drawn, which
# lbTransitionProcDisplay installs as segment 1 (lbtransition.c:150
# gSPSegment) out of sLBTransitionPhotoHeap. There is no image in the file
# to bake, so the tile is interned as one extern texture and the UVs are
# normalised against the *whole* picture rather than a loaded strip.
EXTERN_SEGMENT = 1
EXTERN_TIMG = -1


# FPackBatch.bucket (src/dc/fighter.h): bits[1:0] pick the PVR list, bit 2
# turns the z-test off. The three lists are the PVR's own answer to what the
# RDP does with one render mode word -- opaque, alpha-tested, blended.
FPACK_LIST_OP = 0
FPACK_LIST_PT = 1
FPACK_LIST_TR = 2
FPACK_LIST_MASK = 3
FPACK_NOZ = 4
FPACK_FILT_POINT = 8
# The batch's colour cycle is lerp(ENV, PRIM, TEXEL0) and FPackBatch.env
# holds the ENV it lerps from (MeshBaker._combine_env_lerp). Set by
# ssb_packexport.model_sections out of the material key, not by a caller.
FPACK_ENVLERP = 16
# The batch's colour cycle is lerp(TEXEL0, TEXEL1, PRIM_LOD_FRAC) -- two
# tiles cross-faded by the MObj's own lfrac, which objdisplay.c:1243 writes
# into the primitive colour's LOD byte. Dream Land's clouds are the only
# thing in the game that asks for it (MeshBaker._combine_tex_lerp).
FPACK_TEXLERP = 32
FPACK_OWNRUN = 512
# The DL state's G_CULL_BACK/G_CULL_FRONT geometry-mode bits at the time
# this batch's triangles were emitted (_fresh_state's "cull_back"/
# "cull_front", default cull_back=True to match sys/rdp.c's per-frame
# reset list). A baker default read out of real DL data, not an
# exporter opt-in like FPACK_NOZ -- every MeshBaker exporter gets it
# for free. fighter.c compile_batch reads it to pick the PVR's cull
# mode instead of always drawing both sides.
FPACK_CULL_BACK = 64
FPACK_CULL_FRONT = 128
# The DL state's G_TEXTURE_GEN bit: the RSP generates this batch's texture
# coordinates from each vertex's normal against the two gSPLookAt vectors
# instead of reading the ones in the Vtx. Metal Mario is the whole of it
# in this game -- his fourteen drawn joints each wrap their triangles in
# gsSPSetGeometryMode(G_TEXTURE_GEN)/gsSPClearGeometryMode, and his one
# texture is a chrome sphere map. Baking the Vtx UVs for
# such a batch draws the map's black corner over the whole model, which
# is exactly what the first Metal Mario probe showed.
FPACK_TEXGEN = 256
# The batch's triangles were drawn with the Z compare OFF -- the render
# mode had no RM_Z_CMP, or G_ZBUFFER was clear -- so on the N64 they
# covered whatever the frame had drawn before them, nearer or not. Only
# baked where an exporter opts in with the `ztrack` seed (_fresh_state),
# because the stage layers' z-off draws already have their own answer
# (FPACK_NOZ's painting-order nudge). The weapons are the case:
# wpDisplayDrawNormal (wp/wpdisplay.c) clears G_ZBUFFER and sets
# G_RM_AA_XLU_SURF before every weapon's draw. fighter.c compiles a
# translucent batch with it under PVR_DEPTHCMP_ALWAYS.
FPACK_ZALWAYS = 1024


# One FPackMObjSub (src/dc/fighter.h). Sixteen call sites across five
# exporters wrote this format string out by hand before the two UV fields
# went in -- sixteen chances for the pack and the struct to disagree, and
# sixteen edits to widen it. It lives here now and they call this;
# tools/check/slash_check.py reads a pack back through the same two names.
#
# `uv` is (base_u, base_v, scale_u, scale_v) for the MObj's SECOND tile:
# the scrollu/scrollv at which that tile lands where the bake froze the
# first one, and the normalised UV distance one unit of each track moves
# the tile origin. `uv0` is the same four for the FIRST tile, against its
# own trau/trav tracks. All zero for every MObj that does not animate its
# UV, and then the runtime's offset is identically zero.
# `dpal` is (dpal_first, dpal_count, dpal_bank): a MOBJ_FLAG_PALETTE MObj
# whose script steps palette_id through more than one value (Run's crash
# and Clash's wallpaper quadrants) rather than bake one
# texture+bank per step -- FPACK_PAL_BANKS is 64 and shared by every
# resident pack, and Run's crash alone needs 130 after every dedupe --
# names ONE bank (dpal_bank, an index the same as any batch's FPackTex.pal)
# whose 16 entries src/dc/objmodel.c rewrites in place whenever palette_id
# changes, the real N64's own TLUT swap, from dpal_count raw frames
# starting at dpal_first in the pack's own FPackMObjs.off_dpal array
# (tools/export/ssb_effectexport.py build_pack). (0, 0, -1) for every MObj that
# does not animate this way, which today is everything but those two.
MOBJSUB_FMT = "<HBBIIIIIhh8fhhhh"
MOBJSUB_PACK_SIZE = 68


def pack_mobjsub(sub, tex_first, tex_count, uv=None, dpal=(0, 0, -1),
                 uv0=None):
    b = struct.pack(MOBJSUB_FMT, sub["flags"], sub["prim_l"], 0,
                    sub["primcolor"], sub["envcolor"], sub["blendcolor"],
                    sub["light1"], sub["light2"], tex_first, tex_count,
                    *(uv if uv is not None else (0.0, 0.0, 0.0, 0.0)),
                    *(uv0 if uv0 is not None else (0.0, 0.0, 0.0, 0.0)),
                    *dpal, 0)
    assert len(b) == MOBJSUB_PACK_SIZE
    return b


def dpal_bind(textures, batch_tex, batch_mobj, k, bank):
    """Put every batch MObj `k` draws on a texture in `bank`, the one bank
    the runtime rewrites when that MObj's palette_id moves. On the N64 the
    MObj's TLUT is the one every draw of its joint looks up, not only the
    first: Run's crash draws five slices per joint, and four of them are
    the same pictures in all four joints, so they intern to one texture
    each -- which can only sit in ONE joint's bank. A texture another
    MObj (or no MObj) also draws is copied into `bank`; one only `k` draws
    is moved. `batch_tex` is updated in place."""
    copies = {}
    for i, m in enumerate(batch_mobj):
        ti = batch_tex[i]
        if m != k or ti < 0 or textures[ti]["pal"] == bank:
            continue
        if not any(batch_tex[j] == ti and batch_mobj[j] != k
                   for j in range(len(batch_tex))):
            textures[ti] = dict(textures[ti], pal=bank)
            continue
        if ti not in copies:
            copies[ti] = len(textures)
            textures.append(dict(textures[ti], pal=bank))
        batch_tex[i] = copies[ti]

# G_SETOTHERMODE_H's texture filter field (PR/gbi.h). sys/rdp.c's
# sSYRdpResetDisplayList -- the display list every frame starts with --
# programs other-mode H whole and puts G_TF_BILERP in it, so *bilinear is
# the game's standing state* and a point-sampled batch is the exception a
# display list has to ask for. G_TF_AVERAGE is the RDP's box filter, used
# by the sprite path (lb/lbcommon.c) rather than by any model; it is
# filtering too, so only G_TF_POINT sets FPACK_FILT_POINT.
G_MDSFT_TEXTFILT = 12
# other-mode H's cycle type field (PR/gbi.h G_MDSFT_CYCLETYPE, G_CYC_*)
G_MDSFT_CYCLETYPE = 20
G_CYC_2CYCLE = 1
G_TF_POINT = 0
G_TF_AVERAGE = 3
G_TF_BILERP = 2
OTHERMODE_H_RESET = G_TF_BILERP << G_MDSFT_TEXTFILT


def textfilt_flag(mode):
    """The FPACK_ bit one other-mode H word's texture filter asks for."""
    return FPACK_FILT_POINT \
        if ((mode >> G_MDSFT_TEXTFILT) & 3) == G_TF_POINT else 0

# G_SETOTHERMODE_L bits the bucket is read out of (PR/gbi.h:683-698).
# CVG_X_ALPHA without FORCE_BL is the RDP's alpha cutout -- the coverage,
# not the framebuffer, is what the texel's alpha kills -- which is the
# punch-through list here; FORCE_BL is a real blend, which is the
# translucent one.
RM_CVG_X_ALPHA = 0x1000
RM_ALPHA_CVG_SEL = 0x2000
RM_FORCE_BL = 0x4000
# The render mode's Z compare: G_RM_AA_ZB_* set it, G_RM_AA_* do not.
RM_Z_CMP = 0x10

# The five render modes the stage display lists actually use, as the
# cycle-1|cycle-2 words they are written with (each G_RM_x | G_RM_x2).
# Named here because the layer seeds below are quoted from gr/grdisplay.c
# by name, and because a build warning reads better with one.
G_RM_AA_OPA_SURF = 0x00552048
G_RM_AA_ZB_OPA_SURF = 0x00552078
G_RM_AA_TEX_EDGE = 0x00553048
G_RM_AA_ZB_TEX_EDGE = 0x00553078
G_RM_AA_XLU_SURF = 0x005041C8
G_RM_AA_ZB_XLU_SURF = 0x005049D8

RENDERMODE_NAMES = {
    G_RM_AA_OPA_SURF: "G_RM_AA_OPA_SURF",
    G_RM_AA_ZB_OPA_SURF: "G_RM_AA_ZB_OPA_SURF",
    G_RM_AA_TEX_EDGE: "G_RM_AA_TEX_EDGE",
    G_RM_AA_ZB_TEX_EDGE: "G_RM_AA_ZB_TEX_EDGE",
    G_RM_AA_XLU_SURF: "G_RM_AA_XLU_SURF",
    G_RM_AA_ZB_XLU_SURF: "G_RM_AA_ZB_XLU_SURF",
}


def rendermode_name(mode):
    """The G_RM_* name of an other-mode L word. Bits 0-2 are the alpha
    compare and the z source, which the render mode words do not carry."""
    return RENDERMODE_NAMES.get(mode & ~0x7, "0x%08X" % mode)


def rendermode_list(mode):
    """Which PVR list one G_SETOTHERMODE_L word belongs in."""
    if mode & RM_FORCE_BL:
        return FPACK_LIST_TR
    if mode & RM_CVG_X_ALPHA:
        return FPACK_LIST_PT
    return FPACK_LIST_OP


def _pvr_argb1555(t):
    r, g, b, a = t
    return ((0x8000 if a >= 128 else 0) | ((r >> 3) << 10) |
            ((g >> 3) << 5) | (b >> 3))


def _pvr_argb4444(t):
    r, g, b, a = t
    return (((a >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4))


# --- N64 tile addressing, evaluated offline --------------------------------

def _axis_plan(mask, cm, extent, force=False):
    """Output size for one baked axis, and whether the PVR must clamp it.

    Clamp (or no mask at all) pins the coordinate inside the tile, so the tile
    extent is all we need to store. Otherwise the coordinate wraps, and the
    period we have to reproduce is 2 << mask when mirroring, 1 << mask when
    not -- both powers of two, so they need no padding.
    """
    if mask == 0:
        return extent, True
    period = (2 << mask) if (cm & G_TX_MIRROR) else (1 << mask)
    if (cm & G_TX_CLAMP) and (extent <= period or force):
        # One period or less is a plain clamped bake. `force` marks tiles
        # whose geometry deliberately samples past the clamp edge (Mario
        # holds U=69 against a 64-texel extent): the RDP pins the edge
        # texel there, and only a full extent bake reproduces that.
        return extent, True
    # Wrapping -- or clamping only after one or more full periods, the
    # RDP idiom for "repeat N times": keep one period and let the PVR
    # repeat. Stage walls tile a 64-texel image across a 576-texel
    # extent; baking the repeats would multiply the pack size for
    # nothing. Geometry must stay inside the clamped extent for this to
    # be exact -- MeshBaker._finish checks it does.
    return period, False


def _axis_fetch(i, mask, cm, extent, phys):
    """Source texel for output index `i` along one axis: clamp, then the
    mask/mirror wrap, exactly in the order the RDP applies them."""
    v = i
    if (cm & G_TX_CLAMP) or mask == 0:
        v = min(max(v, 0), extent - 1)
    if mask:
        n = 1 << mask
        if cm & G_TX_MIRROR:
            k = v % (2 * n)
            v = (2 * n - 1 - k) if k >= n else k
        else:
            v %= n
    return min(max(v, 0), phys - 1)


def _pot(n):
    p = 1
    while p < n:
        p *= 2
    return p


# One baked tile's identity. A tuple would do -- it only has to be hashable
# for interning -- but it has grown two load kinds and two colour formats,
# and _finish reads four of the fields back by name.
TexKey = collections.namedtuple("TexKey", (
    "img",                  # the image's base offset in the file
    "origin",               # first texel of the tile, a texel index from img
    "stride",               # texels per row of the source image
    "phys_w", "phys_h",     # the loaded tile's own size, in texels
    "masks", "cms", "maskt", "cmt",
    "ext_w", "ext_h",       # gsDPSetTileSize's extent
    "pal",                  # PVR palette bank for CI4, -1 for direct colour
    "force",                # (U, V): bake that axis's whole clamped extent
    "fmt", "siz", "tlut",
    # The colour combiner does not read this tile; only the alpha one
    # does, so the texel's own RGB must not reach the picture. See
    # MeshBaker._texture.
    "alpha_only"))


def tile_id(key):
    """What a forced extent bake is keyed by (ExtentOverrun.force): where
    the tile's texels come from and how it addresses them -- TexKey's
    first eleven fields. The palette and the rest are left out, so one
    overrun forces the tile under every costume's TLUT."""
    return tuple(key[:11])


def bake_tile(filedata, tex):
    """Flatten one tile into a plain PoT image of texels.

    `tex` is a texture key (see MeshBaker._texture). Returns
    (texels, out_w, out_h, clamp_u, clamp_v). A CI4 tile stays paletted and
    its texels are 4-bit indices; every other format is decoded to
    RGBA8888 tuples here and encoded for the PVR by `pack_texture`. A
    clamped axis whose extent is not a power of two is padded by
    replicating its last row/column, so the PVR's clamp at 1.0 still lands
    on the same texel the RDP would have clamped to.
    """
    fmt, siz, tlut = tex.fmt, tex.siz, tex.tlut
    phys_w, phys_h = tex.phys_w, tex.phys_h

    raw_w, clamp_u = _axis_plan(tex.masks, tex.cms, tex.ext_w, tex.force[0])
    raw_h, clamp_v = _axis_plan(tex.maskt, tex.cmt, tex.ext_h, tex.force[1])
    out_w, out_h = _pot(raw_w), _pot(raw_h)
    if out_w != raw_w and not clamp_u:
        raise ValueError("wrapped U axis is not power-of-two (%d)" % raw_w)
    if out_h != raw_h and not clamp_v:
        raise ValueError("wrapped V axis is not power-of-two (%d)" % raw_h)

    cols = [_axis_fetch(min(x, raw_w - 1), tex.masks, tex.cms, tex.ext_w,
                        phys_w) for x in range(out_w)]
    rows = [_axis_fetch(min(y, raw_h - 1), tex.maskt, tex.cmt, tex.ext_h,
                        phys_h) for y in range(out_h)]

    paletted = tex.pal >= 0
    idx = []
    for t in rows:
        base = tex.origin + t * tex.stride
        for s_ in cols:
            k = base + s_
            if paletted:
                b = filedata[tex.img + (k >> 1)]
                idx.append((b >> 4) if (k & 1) == 0 else (b & 0xF))
            else:
                idx.append(n64_texel(filedata, tex.img, k, fmt, siz, tlut,
                                 tex.stride))

    # The PVR's smallest texture is 8x8 (and the twiddler needs it too);
    # Pikachu has 16x1 strips. A wrapping axis is padded by tiling the
    # content, which keeps it periodic in the original; a CLAMPED axis by
    # repeating its last row/column, which is what the RDP fetches past a
    # clamped edge. Tiling a clamped axis was wrong twice over: geometry
    # past the extent (Captain Falcon's 16x4 clamped strip is sampled to
    # row 6.25) read rows 0..2 again where the N64 holds row 3, and the
    # bilinear filter at the edge blended the last row with the first.
    # UVs are normalised by the padded size either way.
    if out_w < 8 or out_h < 8:
        w8, h8 = max(out_w, 8), max(out_h, 8)

        def pad(i, n, clamp):
            return min(i, n - 1) if clamp else i % n
        idx = [idx[pad(y, out_h, clamp_v) * out_w + pad(x, out_w, clamp_u)]
               for y in range(h8) for x in range(w8)]
        out_w, out_h = w8, h8
    return idx, out_w, out_h, clamp_u, clamp_v


# Whether a CI4 tile ships paletted. It does not, and the reason is the
# PVR's palette RAM: 1024 entries, 64 banks of 16, shared by every pack
# resident at once. A VS stage costs 6 to 17 banks, so six of the nine do
# not fit, and going over does not fail -- it wraps and repaints an
# earlier pack's textures in another pack's colours.
#
# Resolving the tile through its TLUT instead costs nothing in fidelity: a
# mesh TLUT is RGBA5551 and PVR ARGB1555 is the same 5/5/5/1 with the
# alpha bit rotated, which is the rotation `rotate5551` already did to
# fill the bank. It costs VRAM -- four bytes a texel where CI4 spent one,
# 157 KB to 628 KB over six stages and a fighter, against ~4 MB free --
# and that is the trade: 471 KB for every stage the port can hold at once.
#
# The paletted path stays here and in fighter.c because the game animates
# MObjSub.palette_id, and a bank swap is how that is done cheaply if the
# port ever needs it per frame rather than baked.
MESH_PALETTED = False


def pvr_encoding(texels, paletted):
    """The PVR format a baked tile should ship in, and its texels in it.

    A paletted tile stays PAL4BPP against its own 16-entry bank -- see
    MESH_PALETTED, which turns that off. Everything else arrives as
    RGBA8888: ARGB1555 keeps five bits of colour and is exact whenever
    every alpha is already 0 or 255 (the RDP's own 5551, and every I/CI
    source); a tile with graded alpha -- IA8, IA16, RGBA32 -- needs
    ARGB4444 instead, trading a colour bit per channel for the four alpha
    bits that carry the gradient.
    """
    if paletted:
        return PVRTEX_PAL4, texels
    if all(a in (0, 0xFF) for (_r, _g, _b, a) in texels):
        return PVRTEX_ARGB1555, [_pvr_argb1555(t) for t in texels]
    return PVRTEX_ARGB4444, [_pvr_argb4444(t) for t in texels]


def pack_texture(t):
    """One baked tile -> the twiddled bytes the target uploads verbatim."""
    if t["fmt"] == PVRTEX_EXTERN:
        return b""
    if t["fmt"] == PVRTEX_PAL4:
        return twiddle_4bpp(pack_ci4(t["texels"]), t["w"], t["h"])
    return twiddle_16bpp(t["texels"], t["w"], t["h"])


def pack_ci4(indices):
    """Palette indices -> CI4 bytes in PVR order (first texel in the LOW
    nibble), ready for twiddle_4bpp."""
    out = bytearray(len(indices) // 2)
    for i in range(0, len(indices), 2):
        out[i >> 1] = (indices[i] & 0xF) | ((indices[i + 1] & 0xF) << 4)
    return bytes(out)


def read_palette(filedata, off):
    """16 big-endian RGBA5551 entries -> PVR ARGB1555."""
    return [rotate5551(struct.unpack_from(">H", filedata, off + i * 2)[0])
            for i in range(16)]


def read_tlut(filedata, off, n):
    """`n` big-endian RGBA5551 entries, unconverted, for n64_texel."""
    return tuple(struct.unpack_from(">H", filedata, off + i * 2)[0]
                 for i in range(n))


# --- build-time bake -------------------------------------------------------

class MeshBaker:
    """Replay a model's joint tree into one flat world-space mesh.

    The RSP transforms vertices by the modelview *when gsSPVertex executes*,
    and the 32-slot vertex buffer persists across the joint sub-lists, so this
    walks the tree in gcDrawDObjTree order and bakes each loaded vertex with
    the joint matrix that was current at load time. Triangles then index a
    single world-space pool and the target needs no per-joint transform.

    Materials are tracked alongside: the joint list's own combiner/light/tile
    state, merged with whatever the MObj chain contributes through segment
    0xE. Triangles are grouped into batches of constant material, and each
    batch's UVs are normalised against its own baked tile.
    """

    # State every baker this process makes starts from, under its own
    # `seed`: an exporter whose whole output shares one draw context sets
    # it once (ssb_effectexport.py's weapons, which wpDisplayMain draws
    # Z-less -- see FPACK_ZALWAYS) instead of at each of its bakes.
    default_seed = {}

    def __init__(self, filedata, pygfxd, mobjsubs=None,
                 force_extent=frozenset(), rendermode=None,
                 extern_size=None, mobj_batches=False, seed=None,
                 texture_files=None, frame_mobjs=None,
                 paletted=MESH_PALETTED):
        # Per-bake override of the module default: a CI4 tile ships
        # paletted (a real PVR bank, swappable in place) rather than
        # resolved through its TLUT to direct colour. MESH_PALETTED's own
        # comment says why every other caller leaves this alone -- the
        # PVR has 64 banks shared by every pack resident at once, and a
        # VS stage alone can cost 17 -- so this is opt in, per bake, for
        # the rare pack whose whole point IS the bank swap (Run's crash and
        # Clash's wallpaper quadrants, tools/
        # ssb_effectexport.py pack_efentry EFENTRY_PALETTED).
        self.paletted = paletted
        self.f = filedata
        self.g = pygfxd
        # Where this file's textures actually live, for a file whose
        # pointers name another file's images: a texture offset is the key,
        # the file's bytes the value. extern_texture_files explains the
        # rule and why the pointer cannot answer it; the default is that
        # rule applied to the file this baker was handed, so a new caller
        # gets it right by doing nothing. Pass one explicitly only to say
        # something the chain does not (and `{}` to say "none").
        if texture_files is None and isinstance(filedata, RelocFile):
            texture_files = extern_texture_files(filedata)
        self.texture_files = texture_files or {}
        self.mobjsubs = mobjsubs or []
        self.force_extent = force_extent
        # {(DObjDesc index, MObj index): (texture part, frame count)}: the
        # MObjs a fighter's texture parts pick the picture of
        # (ftParamSetTexturePartID sets texture_id_curr). A batch drawn
        # under one bakes every frame's tile beside its own (_material's
        # last element), so the runtime can switch between them.
        self.frame_mobjs = frame_mobjs or {}
        # Whether which MObj drew a batch is part of the batch's material
        # key. Off for every pack whose materials are fixed: the MObj's
        # values are folded in (_apply_mobj) and two MObjs that fold to the
        # same thing may as well be one batch. On only where the MObj is
        # *animated* and the runtime has to reach it -- the winner's emblem,
        # whose MatAnimJoint recolours it per player (ssb_emblemexport.py).
        # Keeping it off by default is what makes every existing pack bake
        # byte-identically after this went in.
        self.mobj_batches = mobj_batches
        # (w, h) of the picture a segment-1 tile draws, or None -- and None
        # is right for everything but a transition wipe. The top byte of a
        # relocated SSB pointer means nothing (the rule is (ptr & 0xFFFF) *
        # 4), so an ordinary file's texture pointer can start 0x01 exactly
        # as it can start 0x0E, and reading a segment out of it would be
        # guesswork. Only tools/export/ssb_transexport.py, which knows what it
        # is opening, turns the segment on -- and then the size is the game's
        # own number, lbtransition.c:216's 300 * 220 * sizeof(u16).
        self.extern_size = extern_size
        # G_SETOTHERMODE_L as each of the four display-list heads starts
        # the walk, when the caller knows it: a stage layer's display proc
        # sets one per head before gcDrawDObjTree (gr/grdisplay.c), and a
        # DL that never sets its own inherits it. None where the caller has
        # no answer -- a fighter, whose display lists set no render mode at
        # all -- and then the head id alone picks the bucket, as before.
        self.rendermode = list(rendermode) if rendermode else [None] * 4
        # RDP state the walk starts from, over _fresh_state's defaults --
        # for the same reason `rendermode` exists: a display list the game
        # runs *before* this one, on the same DL link, may set state this
        # one inherits and never sets itself. The off-screen arrows are the
        # case, and the only one: ifCommonPlayerArrowsMainProcDisplay
        # queues dIFCommonPlayerArrowsDisplayList at priority 8 ahead of
        # the two arrow GObjs, and it is the only thing that ever sets
        # their combiner or their colour (ifcommon.c:231). None everywhere
        # else, and then the defaults stand, as before.
        self.seed = dict(MeshBaker.default_seed)
        self.seed.update(seed or {})
        self.vbuf = {}        # F3DEX2 slot -> baked vertex (raw texel u,v)
        self.verts = []       # deduped joint-local pool (normalised u,v)
        self.nodes = []       # DObj tree in draw order (animation index order)
        self._vert_ix = {}
        self.tris = []        # [i0, i1, i2] into verts
        self.joints = []      # one entry per DL that produced triangles
        self.batches = []     # one entry per run of constant material
        self.textures = []    # interned baked tiles
        self._tex_ix = {}
        self.palettes = []    # interned 16-entry ARGB1555 banks
        self._pal_ix = {}
        self.modified_vtx = 0
        # The model part (FTModelPart) whose display list is being baked:
        # 0 outside one, else the tag bake's `parts` gave it. Batches carry
        # it ("part") and so does the vertex pool's key (_pool).
        self.part = 0
        # tag -> (vert_first, vert_count) of each part's vertex run
        self.part_verts = {}

    # -- vertex pool --------------------------------------------------------

    def _pool(self, v):
        # Keyed by the model part being baked as well (bake's `parts`): a
        # part's vertices must be a run of their own, which the runtime
        # skips whole while another part is on the joint, so they never
        # share a pooled vertex with anything outside the part. Everything
        # outside a part is part 0 and pools exactly as it always has.
        key = (self.part, v)
        i = self._vert_ix.get(key)
        if i is None:
            i = len(self.verts)
            self.verts.append(v)
            self._vert_ix[key] = i
        return i

    # -- textures -----------------------------------------------------------

    def _tex_filedata(self, off):
        """The bytes the image at `off` is in (see texture_files)."""
        return self.texture_files.get(off, self.f)

    def _palette(self, off):
        pal = tuple(read_palette(self._tex_filedata(off), off))
        i = self._pal_ix.get(pal)
        if i is None:
            i = len(self.palettes)
            self.palettes.append(pal)
            self._pal_ix[pal] = i
        return i

    def _extern(self, st):
        """Intern the one tile that is not in this file: segment 1.

        lbTransitionProcDisplay binds sLBTransitionPhotoHeap -- a copy of
        the last frame drawn, 300x220 by lbTransitionSetupTransition's own
        allocation -- as segment 1, and the display list then walks it in
        strips, one gsDPLoadTile per band. Nothing here can bake that; the
        picture does not exist until the game runs. So the whole picture is
        one texture the runtime supplies, the strips collapse into it, and
        _finish normalises UVs against the picture rather than against a
        band (which is why the band origins are not subtracted there).
        """
        w, h = self.extern_size
        key = TexKey(EXTERN_TIMG, 0, w, w, h, 0, G_TX_CLAMP, 0, G_TX_CLAMP,
                     w, h, -1, (False, False), 0, 2, (), False)
        i = self._tex_ix.get(key)
        if i is None:
            i = len(self.textures)
            self.textures.append({
                "key": key, "texels": [], "w": w, "h": h,
                "clamp_u": True, "clamp_v": True, "pal": -1,
                "fmt": PVRTEX_EXTERN, "img": EXTERN_TIMG,
                "phys_w": w, "phys_h": h, "extern": True,
            })
            self._tex_ix[key] = i
        return i

    def _texture(self, st, alpha_only=False):
        """Intern the tile described by the current texture state.

        The tile is the one in TMEM, which is the image that was current
        at the last LOAD -- not whatever gsDPSetTextureImage left in the
        image register afterwards. The two differ whenever something sets
        the register for a reason other than loading a tile, and an MObj
        with MOBJ_FLAG_PALETTE is exactly that: objdisplay.c:1184 emits a
        gsDPSetTextureImage naming the PALETTE, and (without SPLIT or
        ALPHA beside it) nothing else, leaving the joint's own display
        list to issue the gsDPLoadTLUTCmd that reads it. The tile was
        loaded before the MObj's branch and stays in TMEM across all of
        it. Reading the register instead decodes a TLUT's bytes as texels
        -- Brinstar's acid and rock, Yoshi's, Captain Falcon's and
        Pikachu's palette-animated parts.

        Where the tile sits in the source image depends on how it was
        loaded. gsDPLoadBlock moves a whole image contiguously, so its size
        comes from the render tile's `line` (64-bit words per row) and the
        byte count the load actually moved. gsDPLoadTile instead cuts a
        rectangle out of an image whose width gsDPSetTextureImage declared,
        which is how Yoshi's Island draws its clouds: one 66-wide RGBA32
        image, five horizontal bands, one gsDPLoadTile each.
        """
        tile, size = st["tile"], st["tilesize"]
        if st["timg"] == EXTERN_TIMG:
            return self._extern(st)
        img, img_w = st["load_img"], st["load_img_w"]
        fmt, siz = tile["fmt"], tile["siz"]
        if (fmt, siz) not in N64_FORMATS:
            raise ValueError("unhandled texture format %d/%d at 0x%04X"
                             % (fmt, siz, img))
        if st["loadtile"] is not None:
            uls, ult, lrs, lrt = st["loadtile"]
            # The image's width and the load's s range count texels of the
            # size gsDPSetTextureImage declared, which need not be the
            # render tile's. Clash's wallpaper loads its CI4 picture as 8b
            # (80 bytes a row, 160 texels), and reading 80 as the CI4
            # stride took every row's right half from the row below: an
            # interlaced comb over the whole wall.
            lsiz = st["load_img_siz"]
            if lsiz is not None and lsiz != siz:
                num, den = BPP[lsiz], BPP[siz]
                if (img_w * num) % den or (uls * num) % den or \
                        ((lrs + 1) * num) % den:
                    raise ValueError("%db load of a %db tile at 0x%04X does "
                                     "not land on whole texels"
                                     % (BPP[lsiz], BPP[siz], img))
                img_w = img_w * num // den
                uls, lrs = uls * num // den, (lrs + 1) * num // den - 1
            stride = img_w
            origin = ult * stride + uls
            phys_w, phys_h = lrs - uls + 1, lrt - ult + 1
            if stride < phys_w:
                raise ValueError("gsDPLoadTile cuts %d texels from a %d-wide "
                                 "image at 0x%04X" % (phys_w, stride, img))
        elif st["loadblock"]:
            stride = phys_w = tile["line"] * LINE_TEXELS[siz]
            origin = 0
            if phys_w == 0:
                raise ValueError("tile has no line state")
            row_bytes = phys_w * BPP[siz] // 8
            phys_h = st["loadblock"] // row_bytes
            if phys_h == 0:
                raise ValueError("gsDPLoadBlock moved %d bytes, less than one "
                                 "%s row of %d"
                                 % (st["loadblock"], N64_FORMATS[(fmt, siz)],
                                    row_bytes))
        else:
            raise ValueError("tile at %s was never loaded"
                             % ("0x%04X" % st["timg"]
                                if st["timg"] is not None else "?"))
        if BPP[siz] == 4 and (origin & 1):
            raise ValueError("4-bit tile starts on an odd texel (0x%04X)"
                             % img)
        ext_w = int(round(size[2] - size[0])) + 1
        ext_h = int(round(size[3] - size[1])) + 1
        # A CI4 tile keeps its indices and rides a 16-entry PVR palette bank.
        # CI8's bank is 256 entries, which the PVR shares four ways rather
        # than 64, so a CI8 tile resolves through its TLUT here and ships as
        # direct colour like every other non-CI4 format.
        pal, tlut = -1, ()
        if fmt == 2:
            if st["tlut"] is None:
                raise ValueError("CI tile 0x%04X with no TLUT loaded" % img)
            if siz == 0 and self.paletted:
                pal = self._palette(st["tlut"])
            else:
                tlut = read_tlut(self._tex_filedata(st["tlut"]), st["tlut"],
                                 16 if siz == 0 else 256)
        # tile_id's fields, in its order
        tid = (img, origin, stride, phys_w, phys_h, tile["masks"],
               tile["cms"], tile["maskt"], tile["cmt"], ext_w, ext_h)
        key = TexKey(img, origin, stride, phys_w, phys_h,
                     tile["masks"], tile["cms"], tile["maskt"], tile["cmt"],
                     ext_w, ext_h, pal,
                     ((tid, "u") in self.force_extent,
                      (tid, "v") in self.force_extent),
                     fmt, siz, tlut, alpha_only)
        i = self._tex_ix.get(key)
        if i is None:
            texels, w, h, cu, cv = bake_tile(self._tex_filedata(img), key)
            if alpha_only:
                # The RDP reads this tile in the alpha cycle only, so its
                # colour is whatever the colour cycle computes without it.
                # The PVR has no alpha-only texture environment -- the
                # nearest is MODULATEALPHA, which multiplies both -- so the
                # tile ships white and the multiply is the identity on
                # colour and the texel on alpha. Keyed, so a tile read both
                # ways in one file is interned twice rather than once
                # wrongly.
                texels = [(0xFF, 0xFF, 0xFF, a) for (_r, _g, _b, a) in texels]
            pvrfmt, texels = pvr_encoding(texels, pal >= 0)
            i = len(self.textures)
            self.textures.append({
                "key": key, "texels": texels, "w": w, "h": h,
                "clamp_u": cu, "clamp_v": cv, "pal": pal, "fmt": pvrfmt,
                "img": img, "phys_w": phys_w, "phys_h": phys_h,
                # which file the tile was read out of, where the bytes
                # know: `key` is an offset, and two files' can coincide
                "fid": getattr(self.f, "fid", None),
            })
            self._tex_ix[key] = i
        return i

    # -- material state -----------------------------------------------------

    @staticmethod
    def _combine_alpha_uses(a, b, c, d):
        """Which sources the alpha combiner's first cycle reads.

        The cycle is (a - b) * c + d over 3-bit operands sharing one table,
        and 7 is the literal 0 in every slot. A c of 7 kills the product,
        so only d survives.
        """
        used = set()
        if c != AC_ZERO:
            used.update((a, b, c))
        used.add(d)
        used.discard(AC_ZERO)
        return (AC_TEXEL0 in used, AC_SHADE in used, AC_PRIMITIVE in used)

    @staticmethod
    def _combine_uses(a, b, c, d):
        """Which sources the colour combiner's first cycle actually reads.

        The four operand slots have different widths, and each has a spare
        encoding that means a literal 0: A/B are 4-bit with 8..15 == 0, C is
        5-bit with 16..31 == 0, D is 3-bit with 7 == 0.
        """
        used = set()
        if a < 8:
            used.add(a)
        if b < 8:
            used.add(b)
        if c < 16:
            used.add(c)
        if d < 7:
            used.add(d)
        return (CC_TEXEL0 in used, CC_PRIMITIVE in used, CC_SHADE in used)

    @staticmethod
    def _combine_env_lerp(a, b, c, d):
        """Is the colour cycle lerp(ENV, PRIM, TEXEL0)?

        (PRIM - ENV) * TEXEL0 + ENV: the texel's intensity chooses between
        two flat colours rather than tinting one. The PVR draws it exactly
        -- base colour PRIM - ENV modulated by the texel, plus ENV as the
        offset colour -- and src/dc/lbcommon.c already does that for the
        sprite renderer's nLBCommonCombineIAPrimEnv, which is the same
        combiner reached through the same reasoning. Every other use of
        ENV is still dropped, as it has been since the first pack: this
        one is named because a dead explosion is nothing else.
        """
        return (a == CC_PRIMITIVE and b == CC_ENVIRONMENT and
                c == CC_TEXEL0 and d == CC_ENVIRONMENT)

    @staticmethod
    def _combine_tex_lerp(a, b, c, d):
        """Is the colour cycle lerp(TEXEL0, TEXEL1, PRIM_LOD_FRAC)?

        (TEXEL1 - TEXEL0) * PRIM_LOD_FRAC + TEXEL0: two tiles of the same
        MObj's sprite array cross-faded by a factor the RDP reads out of the
        primitive colour's LOD byte, which objdisplay.c:1243 writes as
        `mobj->lfrac * 255`. Dream Land's two cloud layers are the only
        display lists in the game that program it.

        The PVR samples one texture per polygon, so the port answers this
        with two passes rather than one (fighter.h FPACK_TEXLERP). What is
        baked here is tile 0 alone -- tile 1 is another frame of the same
        run, and the second pass picks it by index.
        """
        return (a == CC_TEXEL1 and b == CC_TEXEL0 and
                c == CC_PRIM_LOD_FRAC and d == CC_TEXEL0)

    @staticmethod
    def _fold_cycle2(c0, c1, is_alpha):
        """What the second cycle does to the first's result, for a head in
        G_CYC_2CYCLE: None where it passes it through (or recomputes the
        first cycle's own formula), else the one source it multiplies
        COMBINED by.

        The combiner's second cycle reads the first's output as COMBINED
        (operand 0 in the a, b and d slots, both tables). The material
        key describes ONE cycle, so only the shapes that fold into one
        are taken: (COMBINED - 0) * X + 0 for X in SHADE and PRIMITIVE,
        which is a tint the one-cycle key already has a flag for. Final
        Destination's purple fog is the first (sc1pgameboss.c
        SC1PGameBossWallpaper1ProcDisplay sets G_CYC_2CYCLE; its list
        cross-fades two cloud tiles in cycle 0 and multiplies by the
        purple vertex colour and the fade's primitive alpha in cycle 1).
        Anything else raises, so a new shape is a build break rather than
        a colour quietly dropped.
        """
        a, b, c, d = c1
        if is_alpha:
            zero, srcs = AC_ZERO, (AC_SHADE, AC_PRIMITIVE)
            is_zero_ab = (lambda v: v == AC_ZERO)
            is_zero_c = (lambda v: v == AC_ZERO)
            is_zero_d = (lambda v: v == AC_ZERO)
        else:
            srcs = (CC_SHADE, CC_PRIMITIVE)
            is_zero_ab = (lambda v: v >= 8)
            is_zero_c = (lambda v: v >= 16)
            is_zero_d = (lambda v: v == 7)
        reads_combined = (a == 0 or b == 0 or d == 0 or
                          (not is_alpha and c == 0))
        if is_zero_c(c) and d == 0:
            return None                     # (x - y) * 0 + COMBINED
        if tuple(c1) == tuple(c0) and not reads_combined:
            return None                     # the same formula again
        if a == 0 and is_zero_ab(b) and c in srcs and is_zero_d(d):
            return c                        # (COMBINED - 0) * X + 0
        raise ValueError("2-cycle combiner: cycle 1 %s %r (cycle 0 %r) "
                         "does not fold into one cycle"
                         % ("alpha" if is_alpha else "colour", c1, c0))

    def _material(self, st, two_cycle=False):
        """Freeze the current state into a hashable material key."""
        uses_tex, uses_prim, uses_shade = self._combine_uses(*st["combine"])
        alpha_tex, alpha_shade, alpha_prim = \
            self._combine_alpha_uses(*st["combine_a"])
        if two_cycle:
            x = self._fold_cycle2(st["combine"], st["combine1"], False)
            uses_shade = uses_shade or x == CC_SHADE
            uses_prim = uses_prim or x == CC_PRIMITIVE
            x = self._fold_cycle2(st["combine_a"], st["combine1_a"], True)
            alpha_shade = alpha_shade or x == AC_SHADE
            alpha_prim = alpha_prim or x == AC_PRIMITIVE
        tex = -1
        if (uses_tex or alpha_tex) and st["texture_on"] and \
                st["timg"] is not None:
            # A tile the colour cycle never reads is still a tile: the
            # character select's spotlight is a white quad whose whole
            # shape is one I8 mask in the alpha cycle (relocData 22), and
            # before this it baked as no texture at all -- a solid square.
            tex = self._texture(st, alpha_only=not uses_tex)
        # None where the cycle does not lerp between ENV and PRIM, which
        # is every batch this port baked before the dead explosion.
        env = st["env"] >> 8 if self._combine_env_lerp(*st["combine"]) \
            else None
        frames = None
        fm = self.frame_mobjs.get(st["mobj"]) if st["mobj"] else None
        if tex >= 0 and fm is not None:
            frames = self._texture_frames(st, fm, tex, not uses_tex)
        # PRIM is two registers to the combiner, not one: the colour cycle
        # reads its RGB and the alpha cycle its A, each or neither. Until
        # this kept all four bytes or none by the COLOUR
        # cycle's answer alone, so a list that shades with TEXEL0 * SHADE
        # and fades with TEXEL0 * PRIMITIVE baked opaque: the opening
        # Room's haze (alpha 4C) and Master Hand's shadow (80) among them.
        # The two answers ride the key's tail for model_sections, which
        # tells the runtime which half a LIVE prim may replace.
        prim = ((st["prim"] & 0xFFFFFF00) if uses_prim else 0xFFFFFF00) | \
               ((st["prim"] & 0xFF) if alpha_prim else 0xFF)
        return (tex, prim,
                st["light1"], st["light2"], uses_shade, st["lighting"],
                alpha_tex, alpha_shade, env,
                self._batch_mobj(st) if self.mobj_batches else None,
                self._combine_tex_lerp(*st["combine"]), frames,
                uses_prim, alpha_prim)

    def _batch_mobj(self, st):
        """The MObj the batch about to be emitted is drawn under, or None.

        That is the last segment-0xE branch the walk saw, unless nothing
        the branch did is still in effect: it set no colour and no tile,
        and the display list has since loaded texels and a palette of its
        own. Final Destination's body is the case (relocData 114, joints
        1-4). Each joint branches into five palette MObjs for its
        gradient panels, then loads its own cracked and gold textures for
        the panels over them. Those batches are not the MObj's. Tagged
        with it, the runtime drew the MObj's palette frames on them
        (fighter.c hdr_mobj), and the body came out magenta.
        """
        if st["mobj"] is None or st["mobj_other"]:
            return st["mobj"]
        imgs = st["mobj_imgs"]
        if st["load_img"] in imgs:
            return st["mobj"]
        if st["tile"]["fmt"] == G_IM_FMT_CI and st["tlut"] in imgs:
            return st["mobj"]
        return None

    def _texture_frames(self, st, fm, tex, alpha_only):
        """(texture part, (tile per frame, -1 for a hole)) for a batch drawn
        under a texture part's MObj, or None where the tile it drew is not
        the MObj's sprite (the display list loaded one of its own).

        gcDrawMObjForDObj sets the image register to sprites[texture_id_curr]
        (objdisplay.c:1340) and the joint's list loads it, so the frames are
        the same load with the other sprites in the register. They must bake
        to the same size, or the vertices' UVs -- normalised against the tile
        -- would be wrong for them."""
        part, count = fm
        ni, mi = st["mobj"]
        sub = self.mobjsubs[ni][mi]
        slots = sub.get("sprite_slots") or []
        cur = sub["texture_id_curr"]
        if cur >= len(slots) or slots[cur] is None or \
                st["load_img"] != slots[cur]:
            return None
        base = self.textures[tex]
        out = []
        saved = st["load_img"]
        try:
            for k in range(count):
                if k >= len(slots) or slots[k] is None:
                    out.append(-1)
                    continue
                st["load_img"] = slots[k]
                i = self._texture(st, alpha_only=alpha_only)
                t = self.textures[i]
                if (t["w"], t["h"]) != (base["w"], base["h"]):
                    raise ValueError("texture part %s frame %d bakes to "
                                     "%dx%d, frame %d to %dx%d"
                                     % (part, k, t["w"], t["h"], cur,
                                        base["w"], base["h"]))
                out.append(i)
        finally:
            st["load_img"] = saved
        return (part, tuple(out))

    def _mobj_tile(self, sub, flags, st):
        """objdisplay.c:1163-1177, 1355-1397 and 1399-1420: an MObj's tile
        size and texture scale.

        These three flags are what a *zero* flags word turns into
        (objdisplay.c:1159), so every MObjSub that sets nothing sets these,
        and the CommonSpark's is the first the port packs. The formulas are
        the decomp's, in the same order, writing into the same state a
        display list's own gsDPSetTileSize and gsSPTexture write into --
        which is right, because the MObj's list is a real display list the
        joint's branches into (the gsSPDisplayList into segment 0xE) and the
        RDP sees the two one after the other.

        Tile 1 (`flags & 0x40`) is dropped, exactly as _run_dl drops a
        display list's own gsDPSetTileSize on a tile that is not the render
        tile: nothing here reads tile 1.
        """
        eps = 1.0 / 65535.0
        scau, scav = sub["scau"], sub["scav"]
        trau, trav = sub["trau"], sub["trav"]

        # objdisplay.c:1171-1177, the split-tile halving. Only the U side
        # moves, and only for kind 1.
        if sub["unk10"] == 1:
            scau *= 0.5
            trau = ((trau - sub["unk24"]) + 1.0 -
                    (sub["unk28"] * 0.5)) * 0.5

        if flags & MOBJ_FLAG_TILESIZE:
            w, h = sub["tile_w"], sub["tile_h"]
            if sub["unk10"] == 2:
                uls = int(((w * trau) / scau) * 4.0) if abs(scau) > eps else 0
                ult = int(((h * trav) / scav) * 4.0) if abs(scav) > eps else 0
                uls, ult = max(uls, 0), max(ult, 0)
            else:
                uls = int((((w * trau) + sub["unk0A"]) / scau) * 4.0) \
                    if abs(scau) > eps else 0
                ult = int(((((1.0 - scav) - trav) * h + sub["unk0A"])
                           / scav) * 4.0) if abs(scav) > eps else 0
            st["tilesize"] = (uls / 4.0, ult / 4.0,
                              (((w - 1) << 2) + uls) / 4.0,
                              (((h - 1) << 2) + ult) / 4.0)

        if flags & MOBJ_FLAG_TEXTURE:
            if sub["unk10"] == 2:
                s = int((sub["tile_w"] * 64) / scau) if abs(scau) > eps else 0
                t = int((sub["tile_h"] * 64) / scav) if abs(scav) > eps else 0
            elif sub["unk08"] == 0:
                raise ValueError("MObjSub@0x%04X asks for a texture scale "
                                 "and its unk08 is zero" % sub["off"])
            else:
                s = int((2097152.0 / sub["unk08"]) / scau) \
                    if abs(scau) > eps else 0
                t = int((2097152.0 / sub["unk08"]) / scav) \
                    if abs(scav) > eps else 0
            st["scale_s"] = min(s, 0xFFFF) / 65536.0
            st["scale_t"] = min(t, 0xFFFF) / 65536.0
            st["texture_on"] = True

    def _apply_mobj(self, node, addr, st):
        """gsSPDisplayList into segment 0xE: apply one MObj's contribution.

        gcDrawMObjForDObj lays the chain out as one gSPBranchList per MObj at
        the head of the graphics heap, so the segment offset selects the MObj
        by index.
        """
        subs = self.mobjsubs[node.index] if node.index < len(self.mobjsubs) \
            else []
        i = (addr & 0xFFFFFF) // 8
        if i >= len(subs):
            raise ValueError("DObjDesc[%d] has no MObj %d for segment 0x%08X"
                             % (node.index, i, addr))
        sub = subs[i]
        # Which MObj of this DObj drew what follows. The values below are
        # the struct's own, folded in at bake time; a model whose MObjs are
        # *animated* (the winner's emblem, tools/export/ssb_emblemexport.py)
        # needs the batch to say which MObj to take the animated ones from, and
        # this is that link. It is part of the material key, so a batch
        # never spans two of them.
        st["mobj"] = (node.index, i)
        flags = sub["flags"]
        if flags == 0:
            flags = MOBJ_FLAG_TEXTURE | MOBJ_FLAG_TILESIZE | MOBJ_FLAG_ALPHA
        imgs = set()
        st["mobj_other"] = bool(flags & (
            MOBJ_FLAG_TILESIZE | MOBJ_FLAG_TILESIZE1 | MOBJ_FLAG_TEXTURE |
            MOBJ_FLAG_PRIMCOLOR | MOBJ_FLAG_ENVCOLOR | MOBJ_FLAG_BLENDCOLOR |
            MOBJ_FLAG_LIGHT1 | MOBJ_FLAG_LIGHT2))
        if flags & (MOBJ_FLAG_TILESIZE | MOBJ_FLAG_TILESIZE1 |
                    MOBJ_FLAG_TEXTURE):
            self._mobj_tile(sub, flags, st)
        if flags & MOBJ_FLAG_PALETTE and sub["palettes"]:
            st["timg"] = sub["palettes"][sub["palette_id"]]
            imgs.add(st["timg"])
            if flags & (MOBJ_FLAG_SPLIT | MOBJ_FLAG_ALPHA):
                # With SPLIT or ALPHA alongside, gcDrawMObjForDObj emits the
                # gsDPLoadTLUT into the scratch list itself (objdisplay.c),
                # so the joint's DL never issues the TLUT load -- Kirby's
                # face relies on this; Mario's DLs load their own.
                st["tlut"] = sub["palettes"][sub["palette_id"]]
        if flags & (MOBJ_FLAG_FRAC | MOBJ_FLAG_ALPHA) and sub["sprites"]:
            st["timg"] = sub["sprites"][sub["texture_id_curr"]]
            imgs.add(st["timg"])
        st["mobj_imgs"] = frozenset(imgs)
        if flags & MOBJ_FLAG_PRIMCOLOR:
            st["prim"] = sub["primcolor"]
        if flags & MOBJ_FLAG_ENVCOLOR:
            st["env"] = sub["envcolor"]
        if flags & MOBJ_FLAG_LIGHT1:
            st["light1"] = sub["light1"]
        if flags & MOBJ_FLAG_LIGHT2:
            st["light2"] = sub["light2"]

    # -- display-list replay ------------------------------------------------

    def _fresh_state(self):
        """RSP/RDP state at the top of a display walk. The hardware
        carries this state across every DL the walk submits -- stage
        nodes really do inherit gsSPTexture/combine from a sibling's DL
        -- so bake() creates it once and _run_dl continues it."""
        st = {
            "timg": None, "timg_w": 0, "timg_siz": None, "tlut": None,
            # The image register as it stood at the last tile load --
            # what is actually in TMEM, which is what _texture bakes.
            "load_img": None, "load_img_w": 0, "load_img_siz": None,
            "loadblock": 0, "loadtile": None,
            "tile": {"fmt": 0, "siz": 0, "line": 0, "cms": 0, "masks": 0,
                     "cmt": 0, "maskt": 0},
            "tilesize": (0.0, 0.0, 0.0, 0.0),
            "scale_s": 1.0, "scale_t": 1.0, "texture_on": False,
            "combine": (0xF, 0xF, 0x1F, 7),
            # the alpha cycle's (Aa, Ab, Ac, Ad); (0-0)*0+1, i.e. opaque,
            # until a display list says otherwise
            "combine_a": (AC_ZERO, AC_ZERO, AC_ZERO, 6),
            # the second cycle's colour and alpha operands, in the same
            # order; read only where the head is in G_CYC_2CYCLE
            # (_fold_cycle2), and what the reset leaves until a list says
            "combine1": (0xF, 0xF, 0x1F, 7),
            "combine1_a": (AC_ZERO, AC_ZERO, AC_ZERO, 6),
            "prim": 0xFFFFFFFF, "light1": 0xFFFFFF00, "light2": 0x00000000,
            # The RDP's environment colour. Black is the identity for the
            # one combiner shape below that reads it -- lerp(ENV, PRIM, T)
            # with ENV = 0 is PRIM * T -- so a display list that never
            # sets it bakes exactly as it did before ENV was read at all.
            "env": 0x000000FF,
            "lighting": True,
            # sys/rdp.c's sSYRdpResetDisplayList clears G_CULL_BOTH then
            # sets G_CULL_BACK -- the engine's real per-frame default,
            # not "draw both sides" (fighter.c compile_batch used to
            # assume the latter; see FPACK_CULL_BACK/_FRONT below).
            "cull_back": True, "cull_front": False,
            # G_TEXTURE_GEN, off in sys/rdp.c's reset list
            "texgen": False,
            # G_ZBUFFER, on in it. FPACK_ZALWAYS is baked only with
            # `ztrack`; `zcmp` is each head's Z compare while that head
            # has no render mode of its own (bucket_of's fallback).
            "zbuffer": True, "ztrack": False,
            "zcmp": [True, True, True, True],
            # (DObjDesc index, MObj index) of the last segment-0xE branch,
            # or None where the joint's DL never made one -- see _apply_mobj
            "mobj": None,
            # What that branch did besides naming itself: the image and
            # palette addresses it put in the texture registers, and
            # whether it set a colour or the tile (_batch_mobj)
            "mobj_imgs": frozenset(), "mobj_other": False,
            # per display-list head, so a render mode a head-0 DL sets does
            # not leak into a head-1 one: the RDP sees the heads one after
            # the other, but each is a separate list the game builds.
            "othermode_l": list(self.rendermode),
            # Whether the render-mode *field* of that word has been written
            # yet, per head. A display list may write another field of the
            # word first -- the shield bubble's sets only the alpha compare
            # (gsDPSetAlphaCompare, mask 0x3) and never a render mode --
            # and a word that holds that write alone is not a render mode
            # to bucket by: until the field is set, the head still decides.
            "othermode_l_rm": [m is not None for m in self.rendermode],
            # and per head for the same reason. Seeded from the reset
            # display list, because that is what the RDP is holding when
            # the game's own lists start.
            "othermode_h": [OTHERMODE_H_RESET] * 4,
        }
        for k, v in self.seed.items():
            if k not in st:
                raise ValueError("no display-list state named %r" % k)
            # a per-head list is written in place as the lists run, so
            # each walk takes its own copy of the seed
            st[k] = list(v) if isinstance(v, list) else v
        return st

    def _run_dl(self, node, dl_off, head=0):
        """Replay one display list into `head`, the DL head the game queues
        it into (gcDrawDObjTree always head 0; gcDrawDObjDLLinks the link's
        list_id)."""
        g = self.g
        dl_length(self.f, dl_off)     # sanity: unflattened list terminates
        first = len(self.tris)
        batch_first = len(self.batches)
        ended = [False]

        st = self._st
        cur = [None]          # current material key
        batch = [None]        # current batch dict
        dl = flatten_dl(self.f, dl_off)

        def bucket_of():
            """The PVR list this DL's triangles belong in right now. With no
            render mode to read, fall back to what the head id says --
            objdisplay.c queues opaque into heads 0 and 2, translucent into
            1 and 3."""
            mode = st["othermode_l"][head]
            if mode is None or not st["othermode_l_rm"][head]:
                return FPACK_LIST_TR if (head & 1) else FPACK_LIST_OP
            return rendermode_list(mode)

        def emit(slots):
            mat = cur[0]
            if mat is None:
                mat = cur[0] = self._material(
                    st, ((st["othermode_h"][head] >> G_MDSFT_CYCLETYPE) & 3)
                    == G_CYC_2CYCLE)
            bucket = bucket_of() | textfilt_flag(st["othermode_h"][head]) \
                | (FPACK_CULL_BACK if st["cull_back"] else 0) \
                | (FPACK_CULL_FRONT if st["cull_front"] else 0) \
                | (FPACK_TEXGEN if st["texgen"] else 0)
            if st["ztrack"]:
                mode = st["othermode_l"][head]
                zcmp = (bool(mode & RM_Z_CMP)
                        if mode is not None and st["othermode_l_rm"][head]
                        else st["zcmp"][head])
                if not (zcmp and st["zbuffer"]):
                    bucket |= FPACK_ZALWAYS
            if batch[0] is None or batch[0]["mat"] != mat or \
                    batch[0]["bucket"] != bucket:
                batch[0] = {"mat": mat, "tri_first": len(self.tris),
                            "tri_count": 0, "node": node, "bucket": bucket,
                            # kept for the oracle: the other-mode L word the
                            # bucket was read out of, or None where the head
                            # had none and the head id decided
                            "rendermode": st["othermode_l"][head],
                            "mobj": self._batch_mobj(st),
                            "dl": dl_off, "head": head}
                self.batches.append(batch[0])
            tex = self.textures[mat[0]] if mat[0] >= 0 else None
            for s in slots:
                if self.vbuf[s][10] != st["lighting"]:
                    raise ValueError("triangle mixes vertices loaded with "
                                     "G_LIGHTING %s into a %s batch"
                                     % (self.vbuf[s][10], st["lighting"]))
            self.tris.append([self._pool(self._finish(self.vbuf[s][:10],
                                                      tex, st))
                              for s in slots])
            batch[0]["tri_count"] += 1

        trouble = []

        def macro_fn():
            if trouble:
                return 0
            try:
                return macro_body()
            except Exception as e:               # noqa: BLE001
                trouble.append(e)
                return 0

        def macro_body():
            if ended[0]:
                return 0
            name = g.gfxd_macro_name()
            a = [g.gfxd_arg_value(i)[1] for i in range(g.gfxd_arg_count())]

            if name == "gsSPEndDisplayList":
                ended[0] = True
            elif name == "gsSPVertex":
                # Kept in this joint's local space, tagged with the joint that
                # owns it. The RSP would have transformed here, by the matrix
                # current at load; deferring that to the target is what lets
                # the joint move. A joint's triangles can still reference
                # slots its parent loaded, so ownership is per vertex, not
                # per triangle.
                lit = st["lighting"]
                for i, v in enumerate(decode_vtx(self.f, a[0], a[1], lit)):
                    if lit:
                        n = math.sqrt(v[5] ** 2 + v[6] ** 2 +
                                      v[7] ** 2) or 1.0
                        nrm = (v[5] / n, v[6] / n, v[7] / n)
                    else:
                        # vertex colour, 0..1 floats in the normal slots
                        nrm = (v[5] / 255.0, v[6] / 255.0, v[7] / 255.0)
                    self.vbuf[a[2] + i] = (float(v[0]), float(v[1]),
                                           float(v[2]), v[3], v[4],
                                           nrm[0], nrm[1], nrm[2],
                                           v[8], node.index, lit)
            elif name == "gsSPModifyVertex":
                self._modify_vertex(a)
            elif name == "gsSP2Triangles":
                emit(a[0:3])
                emit(a[4:7])
            elif name == "gsSP1Triangle":
                emit(a[0:3])
            elif name == "gsDPSetTextureImage":
                # A pointer into segment 1 is the framebuffer photocopy, not
                # anything this file holds (EXTERN_TIMG). The declared width
                # is still the picture's: 300, which is what
                # lbTransitionSetupTransition copies out of the framebuffer.
                st["timg"] = (EXTERN_TIMG
                              if (self.extern_size is not None and
                                  a[3] == (EXTERN_SEGMENT << 24))
                              else ptr_off(a[3]))
                st["timg_w"] = a[2]
                st["timg_siz"] = a[1]
            elif name == "gsDPLoadTLUTCmd":
                st["tlut"] = st["timg"]
                cur[0] = None
            elif name in ("gsDPLoadTLUT", "gsDPLoadTLUT_pal16",
                          "gsDPLoadTLUT_pal256"):
                # gfxd folds a whole SETTIMG/SETTILE/LOADSYNC/LOADTLUT run
                # into one macro when the words match gbi.h's, and then the
                # SETTIMG never reaches the branch above. The palette is the
                # macro's last argument in all three spellings. Nothing but
                # ef/efmanager.c's effect lists is written this way -- every
                # other pack's CI tiles come through gsDPLoadTLUTCmd -- so
                # this changes no existing pack's bytes.
                st["timg"] = st["tlut"] = ptr_off(a[-1])
                cur[0] = None
            elif name == "gsDPLoadBlock":
                # (lrs + 1) counts texels of the size SETTIMG declared. gbi
                # loads 4b and 8b images as 16b, but a 32b image loads as
                # 32b (G_IM_SIZ_32b_LOAD_BLOCK): the Break the Targets
                # target, whose bottom half was lost counting it as 16b.
                # An MObj's SETTIMG leaves timg_siz unset and loads as 16b.
                st["loadblock"] = (a[3] + 1) * (4 if st["timg_siz"] == 3
                                                else 2)
                st["loadtile"] = None
                st["load_img"] = st["timg"]
                st["load_img_w"] = st["timg_w"]
                cur[0] = None
            elif name == "gsDPLoadTile":
                # s,t in 10.2 fixed point, inclusive of both corners.
                st["loadtile"] = (a[1] // 4, a[2] // 4, a[3] // 4, a[4] // 4)
                st["loadblock"] = 0
                st["load_img"] = st["timg"]
                st["load_img_w"] = st["timg_w"]
                st["load_img_siz"] = st["timg_siz"]
                cur[0] = None
            elif name == "gsDPSetTile":
                if a[4] == 0:                    # G_TX_RENDERTILE
                    st["tile"] = {"fmt": a[0], "siz": a[1], "line": a[2],
                                  "cmt": a[6], "maskt": a[7], "cms": a[9],
                                  "masks": a[10]}
                    cur[0] = None
            elif name == "gsDPSetTileSize":
                if a[0] == 0:
                    st["tilesize"] = (a[1] / 4.0, a[2] / 4.0,
                                      a[3] / 4.0, a[4] / 4.0)
                    cur[0] = None
            elif name == "gsSPTexture":
                st["scale_s"] = a[0] / 65536.0
                st["scale_t"] = a[1] / 65536.0
                st["texture_on"] = bool(a[4])
                cur[0] = None
            elif dl[g.gfxd_macro_offset()] == G_SETOTHERMODE_H:
                # The same dozen spellings as other-mode L below
                # (gsDPSetTextureFilter, gsDPSetCycleType,
                # gsSPSetOtherModeH), so dispatch on the opcode here too.
                # Only the texture filter is read back out (textfilt_flag).
                o = g.gfxd_macro_offset()
                w0, w1 = struct.unpack_from(">II", dl, o)
                length = (w0 & 0xFF) + 1
                shift = 32 - ((w0 >> 8) & 0xFF) - length
                mask = ((1 << length) - 1) << shift
                st["othermode_h"][head] = \
                    (st["othermode_h"][head] & ~mask) | (w1 & mask)
                cur[0] = None
            elif (dl[g.gfxd_macro_offset()] == G_SETOTHERMODE_L or
                  dl[g.gfxd_macro_offset()] == G_RDPSETOTHERMODE):
                # gfxd names this command a dozen ways -- gsDPSetRenderMode,
                # gsDPSetAlphaCompare, gsSPSetOtherModeL -- depending on the
                # field it happens to write, so dispatch on the opcode and
                # apply the write to the head's other-mode L word directly.
                # Only the render-mode field is read back out (bucket_of),
                # but keeping the whole word is what makes the spelling not
                # matter.
                o = g.gfxd_macro_offset()
                w0, w1 = struct.unpack_from(">II", dl, o)
                if dl[o] == G_RDPSETOTHERMODE:
                    # 0xEF writes both words at once: H in w0's low 24 bits.
                    st["othermode_h"][head] = w0 & 0x00FFFFFF
                    st["othermode_l"][head] = w1
                    st["othermode_l_rm"][head] = True
                else:
                    length = (w0 & 0xFF) + 1
                    shift = 32 - ((w0 >> 8) & 0xFF) - length
                    mask = ((1 << length) - 1) << shift
                    prev = st["othermode_l"][head] or 0
                    st["othermode_l"][head] = (prev & ~mask) | (w1 & mask)
                    # The render mode is bits 3 and up (G_MDSFT_RENDERMODE);
                    # below it sit the alpha compare and the Z source.
                    if mask & ~0x7:
                        st["othermode_l_rm"][head] = True
                cur[0] = None
            elif name in ("gsDPSetCombineLERP", "gsDPSetCombineMode"):
                # gfxd prints a stock mode symbolically (gsDPSetCombineMode)
                # when the words match one; decode cycle 0 from the raw
                # command instead so both spellings land here identically.
                o = g.gfxd_macro_offset()
                w0 = struct.unpack_from(">I", dl, o)[0]
                w1 = struct.unpack_from(">I", dl, o + 4)[0]
                st["combine"] = ((w0 >> 20) & 0xF, (w1 >> 28) & 0xF,
                                 (w0 >> 15) & 0x1F, (w1 >> 15) & 0x7)
                # GACc0w0/GACc0w1 (PR/gbi.h): Aa and Ac in w0 at 12 and 9,
                # Ab and Ad in w1 at the same two shifts
                st["combine_a"] = ((w0 >> 12) & 0x7, (w1 >> 12) & 0x7,
                                   (w0 >> 9) & 0x7, (w1 >> 9) & 0x7)
                # GCCc1w0/GCCc1w1 and GACc1w1: the second cycle's a, b, c,
                # d for colour at w0 5, w1 24, w0 0, w1 6 and for alpha
                # at w1 21, 3, 18, 0
                st["combine1"] = ((w0 >> 5) & 0xF, (w1 >> 24) & 0xF,
                                  w0 & 0x1F, (w1 >> 6) & 0x7)
                st["combine1_a"] = ((w1 >> 21) & 0x7, (w1 >> 3) & 0x7,
                                    (w1 >> 18) & 0x7, w1 & 0x7)
                cur[0] = None
            elif name == "gsDPSetPrimColor":
                st["prim"] = ((a[2] << 24) | (a[3] << 16) |
                              (a[4] << 8) | a[5])
                cur[0] = None
            elif name == "gsDPSetEnvColor":
                # No m/l pair on this one: r, g, b, a and nothing else.
                st["env"] = ((a[0] << 24) | (a[1] << 16) |
                             (a[2] << 8) | a[3])
                cur[0] = None
            elif name == "gsSPGeometryMode":
                if a[0] & G_LIGHTING:
                    st["lighting"] = False
                    cur[0] = None
                if a[1] & G_LIGHTING:
                    st["lighting"] = True
                    cur[0] = None
                # Cull bits don't change the material key -- only
                # `bucket`, which emit() recomputes every call -- so no
                # cur[0] = None here; a batch boundary still falls out
                # automatically when bucket_of()'s caller ORs this in.
                if a[0] & G_CULL_BACK:
                    st["cull_back"] = False
                if a[1] & G_CULL_BACK:
                    st["cull_back"] = True
                if a[0] & G_CULL_FRONT:
                    st["cull_front"] = False
                if a[1] & G_CULL_FRONT:
                    st["cull_front"] = True
                if a[0] & G_TEXTURE_GEN:
                    st["texgen"] = False
                if a[1] & G_TEXTURE_GEN:
                    st["texgen"] = True
                if a[0] & G_ZBUFFER:
                    st["zbuffer"] = False
                if a[1] & G_ZBUFFER:
                    st["zbuffer"] = True
            elif name == "gsSPSetGeometryMode":
                if a[0] & G_LIGHTING:
                    st["lighting"] = True
                    cur[0] = None
                if a[0] & G_CULL_BACK:
                    st["cull_back"] = True
                if a[0] & G_CULL_FRONT:
                    st["cull_front"] = True
                if a[0] & G_TEXTURE_GEN:
                    st["texgen"] = True
                if a[0] & G_ZBUFFER:
                    st["zbuffer"] = True
            elif name == "gsSPClearGeometryMode":
                if a[0] & G_LIGHTING:
                    st["lighting"] = False
                    cur[0] = None
                if a[0] & G_CULL_BACK:
                    st["cull_back"] = False
                if a[0] & G_CULL_FRONT:
                    st["cull_front"] = False
                if a[0] & G_TEXTURE_GEN:
                    st["texgen"] = False
                if a[0] & G_ZBUFFER:
                    st["zbuffer"] = False
            elif name == "gsSPLoadGeometryMode":
                st["zbuffer"] = bool(a[0] & G_ZBUFFER)
                st["lighting"] = bool(a[0] & G_LIGHTING)
                st["cull_back"] = bool(a[0] & G_CULL_BACK)
                st["cull_front"] = bool(a[0] & G_CULL_FRONT)
                st["texgen"] = bool(a[0] & G_TEXTURE_GEN)
                cur[0] = None
            elif name == "gsSPLightColor":
                st["light1" if a[0] == 1 else "light2"] = a[1]
                cur[0] = None
            elif name == "gsSPDisplayList":
                if (a[0] >> 16) != (MOBJ_SEGMENT << 8):
                    raise ValueError("gsSPDisplayList to segment 0x%X at "
                                     "0x%04X" % (a[0] >> 24, dl_off))
                self._apply_mobj(node, a[0], st)
                cur[0] = None
            return 0

        g.gfxd_macro_fn(macro_fn)
        g.gfxd_output_buffer(b"\0" * (len(dl) * 80 + 4096))
        g.gfxd_input_buffer(dl)
        g.gfxd_target(g.gfxd_f3dex2)
        g.gfxd_endian(g.GfxdEndian.big, 4)
        g.gfxd_puts("")
        g.gfxd_macro_dflt()
        g.gfxd_execute()
        if trouble:
            # ctypes swallows exceptions inside the callback; resurface the
            # first one so a broken display list fails the build loudly
            # instead of shipping a silently truncated mesh. An
            # ExtentOverrun keeps its type: bake_retry dispatches on it.
            if isinstance(trouble[0], ExtentOverrun):
                raise trouble[0]
            raise ValueError("DL 0x%04X (DObjDesc[%d]): %s"
                             % (dl_off, node.index, trouble[0]))

        if len(self.tris) > first:
            self.joints.append({
                "node": node,
                "tri_first": first,
                "tri_count": len(self.tris) - first,
                "batch_first": batch_first,
                "batch_count": len(self.batches) - batch_first,
            })

    def _finish(self, v, tex, st):
        """Turn a slot's raw texel s,t into the baked tile's linear UVs.

        The RSP scales the vertex's s10.5 coordinate by gsSPTexture, then the
        RDP subtracts the tile origin set by gsDPSetTileSize; everything after
        that (mask, mirror, clamp) is already inside the baked image.
        """
        if tex is None:
            return (v[0], v[1], v[2], 0.0, 0.0) + v[5:]
        if tex.get("extern"):
            # The texture is the whole picture, so the band origin
            # gsDPSetTileSize set is exactly what must *not* be taken off:
            # a texel s is column s of the photocopy, whichever band was
            # loaded to reach it.
            su, tv = v[3] * st["scale_s"], v[4] * st["scale_t"]
            return (v[0], v[1], v[2], su / tex["w"], tv / tex["h"]) + v[5:]
        uls, ult = st["tilesize"][0], st["tilesize"][1]
        su = v[3] * st["scale_s"] - uls
        tv = v[4] * st["scale_t"] - ult
        ext_w, ext_h = tex["key"].ext_w, tex["key"].ext_h
        # Only a clamp-mode tile baked as a repeating period has a clamp
        # edge to overrun; a genuine wrap tile is periodic everywhere. The
        # overrun forces that axis alone: the other one stays a period,
        # which is exact wherever the geometry stays inside the extent.
        # Forcing both, and every tile cut from the same image, made
        # Donkey Kong's 16x16 fur tile a 512x128 texture in each of his
        # five costumes, 800 KB of his pack's 1 MB of texels.
        #
        # Below zero is past the edge too: the RDP clamps s to 0 and the
        # period would wrap it to the far side of the tile.
        key = tex["key"]
        tid = tile_id(key)
        if (key.cms & G_TX_CLAMP) and not tex["clamp_u"] and \
                (su > ext_w + 0.5 or su < -0.5):
            raise ExtentOverrun(tex["img"], (tid, "u"),
                                "U %.1f outside clamp extent %d of tile "
                                "0x%X" % (su, ext_w, tex["img"]))
        if (key.cmt & G_TX_CLAMP) and not tex["clamp_v"] and \
                (tv > ext_h + 0.5 or tv < -0.5):
            raise ExtentOverrun(tex["img"], (tid, "v"),
                                "V %.1f outside clamp extent %d of tile "
                                "0x%X" % (tv, ext_h, tex["img"]))
        return (v[0], v[1], v[2], su / tex["w"], tv / tex["h"]) + v[5:]

    def _modify_vertex(self, a):
        """gsSPModifyVertex(slot, where, val). Only G_MWO_POINT_ST (0x14) is
        used by the fighter models: it rewrites a slot's texture coords in
        place, on a vertex a *parent* joint loaded."""
        slot, where, val = a[0], a[1], a[2]
        if where != 0x14:
            raise ValueError("unhandled gsSPModifyVertex where 0x%X" % where)
        s = (val >> 16) & 0xFFFF
        t = val & 0xFFFF
        s = (s - 65536 if s >= 32768 else s) / 32.0
        t = (t - 65536 if t >= 32768 else t) / 32.0
        # The RSP keeps a vertex's texture coordinates ALREADY scaled by
        # gsSPTexture (F3DEX2 scales them at gsSPVertex), and this command
        # writes into that cache: the value is the final coordinate, never
        # scaled again. The baker scales at emit instead (_uv), so it is
        # divided out here. Final Destination's fog is why: its lists
        # scale t by 0x8000 and rewrite the shared edge of each of its 8
        # bands to t = 64 -- scaled again that was 32, the tile origin, so
        # bands 2-8 sampled one texel row (vertical curtains, a seam at
        # every band edge) where the N64 shows the whole cloud texture.
        # Every other user (the fighters, the comets) sets 0xFFFF, where
        # this moves nothing visible.
        st = self._st
        s = s / st["scale_s"] if st["scale_s"] else s
        t = t / st["scale_t"] if st["scale_t"] else t
        v = self.vbuf[slot]
        self.vbuf[slot] = (v[0], v[1], v[2], s, t) + v[5:]
        self.modified_vtx += 1

    # -- driver -------------------------------------------------------------

    def _bake_part(self, node, tag, dl_off, subs, pf):
        """Bake one model part's display list for `node`, tagged `tag`.

        ftParamSetModelPartID swaps the DObj's display list (and its MObjs)
        for the part's, so the runtime has to be able to draw any one of a
        joint's parts in the default's place. The first of `node`'s parts
        is its own display list, baked in the walk's live state like any
        other joint's; each other one is baked from a copy of that state
        and the state put back after, so what the walk bakes next is
        exactly what it would have been with no parts at all. The copy is
        the state as it stood before the default ran, which is what the
        RSP holds when the swapped DObj is drawn instead -- except for a
        child's triangles on vertices its parent loaded, which are baked
        against the default parent (the one part that is on at bind).

        `subs` is the part's MObjSub list, costume folded, or None to keep
        the joint's own; `pf` the file the display list is in when it is
        not the model's (Link's boomerang hand, Kirby's copies of it and
        of Fox's reflector), or None.
        """
        if tag in self.part_verts:
            raise ValueError("part tag %d baked twice" % tag)
        saved = (self.mobjsubs, self.f, self.texture_files, self._tex_ix)
        if subs is not None:
            self.mobjsubs = list(self.mobjsubs) + \
                [[]] * (node.index + 1 - len(self.mobjsubs))
            self.mobjsubs[node.index] = subs
        if pf is not None:
            self.f = pf
            self.texture_files = extern_texture_files(pf) \
                if isinstance(pf, RelocFile) else {}
            # a texture's key is its offset in its file, so each file
            # interns its own
            self._tex_ix = self._tex_ix_by_file.setdefault(pf.fid, {})
        v0, b0 = len(self.verts), len(self.batches)
        self.part = tag
        try:
            self._run_dl(node, dl_off)
        finally:
            self.part = 0
        for b in self.batches[b0:]:
            b["part"] = tag
        self.part_verts[tag] = (v0, len(self.verts) - v0)
        self.mobjsubs, self.f, self.texture_files, self._tex_ix = saved

    def bake(self, root, skip=frozenset(), parts=None, skeleton=None):
        """Walk the tree in draw order, accumulating matrices and geometry.

        The bind matrices are still computed here -- they are the reference
        the joint-local geometry is checked against -- but they are no longer
        folded into the vertices.

        `skip` is the set of DObjDesc entries the game never instantiates
        (setup_parts bits that are clear): their transforms still exist for
        the tree walk, but their display lists are not drawn.

        `parts` is {DObjDesc index: [(tag, dl, subs, file), ...]}, a
        joint's model parts, dl None for the joint's own display list (see
        _bake_part); None for a model without them.

        `skeleton` is {skeleton id: {DObjDesc index: (tag, pre dl, dl)}},
        the electric shock's skeleton, or None. Each id is
        a second walk of its own after the model's, in draw order from a
        fresh RSP, because ftDisplayMainDrawSkeleton draws those lists in
        place of every joint's own: a child's triangles land on the
        vertices its parent's skeleton list loaded. The pre list (flags
        nibble 1) runs under the parent's matrix, as dls[0] does. Each
        joint's lists are one tag, in the model's file and the joint's own
        MObjs (gcDrawMObjForDObj on the same DObj).
        """
        self.nodes = []
        self._st = self._fresh_state()
        parts = parts or {}
        self._tex_ix_by_file = {}

        def visit(node, parent_matrix):
            if node.index < 0:
                node.matrix = parent_matrix
            else:
                node.matrix = mat_mul(mat_local(node.translate, node.rotate,
                                                node.scale),
                                      parent_matrix)
                self.nodes.append(node)
            if node.index not in skip:
                if node.dl_pre_off is not None and node.parent is not None \
                        and node.parent.index >= 0:
                    # dls[0] runs before gcPrepDObjMatrix: its vertices land
                    # under the parent's matrix, so the parent owns them.
                    self._run_dl(node.parent, node.dl_pre_off)
                alts = parts.get(node.index)
                if alts:
                    before = (copy.deepcopy(self.vbuf),
                              copy.deepcopy(self._st))
                    if node.dl_off is not None and \
                            all(dl is not None for _t, dl, _s, _f in alts):
                        # a joint whose only alternate is drawn beside its
                        # own list (an accessory): the list is untagged
                        self._run_dl(node, node.dl_off)
                    after = (self.vbuf, self._st)
                    for tag, dl, subs, pf in alts:
                        if dl is None:
                            # the tree's own, in the walk's live state
                            self.vbuf, self._st = after
                            self._bake_part(node, tag, node.dl_off, None,
                                            None)
                            after = (self.vbuf, self._st)
                        else:
                            self.vbuf, self._st = copy.deepcopy(before)
                            self._bake_part(node, tag, dl, subs, pf)
                    self.vbuf, self._st = after
                elif node.dl_off is not None:
                    self._run_dl(node, node.dl_off)
                for list_id, dl in node.dl_links:
                    if list_id == 0:
                        self._run_dl(node, dl, list_id)
            for child in node.children:
                visit(child, node.matrix)

        visit(root, mat_ident())

        # The other heads, each its own walk. gcDrawDObjTreeDLLinksForGObj
        # queues a link into gSYTaskmanDLHeads[list_id], and the RDP runs
        # the heads one after the other -- 0, 2, 1, 3 (taskman.c's branch
        # chain) -- so a head-1 list inherits what the head-1 lists before
        # it set, not what its own joint's head-0 list left. The opening
        # room's castle railing (file 52, joint 2) sets no texture, TLUT or
        # combiner of its own; joint 0's head-1 list does, and walked
        # joint by joint the railing got joint 2's head-0 G_TT_NONE, SHADE
        # and texture-off instead, and baked untextured. The other way
        # round too: a head-1 list's closing texture-off and G_LIGHTING
        # reached the NEXT joint's head-0 list, which is how the Board the
        # Platforms stages and Zebes lost textures that list relies on.
        # Each head carries on from the one before; the RDP reset the
        # taskman puts between heads only when head 2 is in use is not
        # modelled (on every tree baked here it changes only light colours
        # under lighting off, and the Yoster nest's head-1 cull).
        for head in (2, 1, 3):
            for node in self.nodes:
                if node.index in skip:
                    continue
                for list_id, dl in node.dl_links:
                    if list_id == head:
                        self._run_dl(node, dl, list_id)

        for sid in sorted(skeleton or {}):
            rows = skeleton[sid]
            self.vbuf = {}
            self._st = self._fresh_state()

            def skeleton_visit(node):
                if node.index >= 0 and node.index not in skip and \
                        node.index in rows:
                    tag, pre, dl = rows[node.index]
                    if tag in self.part_verts:
                        raise ValueError("part tag %d baked twice" % tag)
                    v0 = len(self.verts)
                    b0 = len(self.batches)
                    self.part = tag
                    try:
                        if pre is not None:
                            if node.parent is None or node.parent.index < 0:
                                raise ValueError("skeleton %d entry %d: a "
                                                 "pre list with no parent"
                                                 % (sid, node.index))
                            self._run_dl(node.parent, pre)
                        if dl is not None:
                            self._run_dl(node, dl)
                    finally:
                        self.part = 0
                    for bt in self.batches[b0:]:
                        bt["part"] = tag
                    self.part_verts[tag] = (v0, len(self.verts) - v0)
                for child in node.children:
                    skeleton_visit(child)

            skeleton_visit(root)
        # gcAddAnimJointAll walks the tree and steps its anim_joints table once
        # per DObj, so the animation data is indexed by this order. It happens
        # to equal DObjDesc array order, because lbCommonSetupTreeDObjs creates
        # nodes in the order it reads them; assert that rather than assume it.
        for i, n in enumerate(self.nodes):
            if n.index != i:
                raise ValueError("draw order %d is DObjDesc entry %d: the "
                                 "animation table's indexing does not hold"
                                 % (i, n.index))
        return self.verts, self.tris, self.joints

    def world_verts(self, matrices=None):
        """The vertex pool posed by joint matrices (bind pose by default)."""
        mats = matrices or [n.matrix for n in self.nodes]
        return [xform_point(mats[v[9]], v[0], v[1], v[2]) for v in self.verts]

    def bounds(self, matrices=None):
        w = self.world_verts(matrices)
        return (tuple(min(p[i] for p in w) for i in range(3)),
                tuple(max(p[i] for p in w) for i in range(3)))
