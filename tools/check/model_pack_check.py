#!/usr/bin/env python3
"""ssb64-dc: the exported model packs' geometry and materials, against the ROM.

effect_model_check re-bakes each effect pack with ssb_assets.MeshBaker and
compares. Nothing did that for the fighters, the stages, the items and the
weapons, and a re-bake with the exporter's own baker only shows that the
pack is what the baker said. This is the oracle that does not use the
baker's display-list code at all.

What is independent. A second display-list interpreter lives in this file
(RefRun): it reads the F3DEX2 commands as raw 8-byte words (bit-sliced
here, not through pygfxd), resolves every pointer through the file's
relocation chain, and keeps its own RSP/RDP state -- vertex buffer,
geometry mode, gsSPTexture scale, tile 0 and its tile size, the combiner
words, the per-head other-mode words, PRIM/ENV/light colours. An MObj
call (gsSPDisplayList into segment 0xE) is applied by a fresh port of
sys/objdisplay.c gcDrawMObjForDObj's tile-size/texture-scale/colour math.
The DObjDesc tree is decoded by hand and walked in gcDrawDObjTree's order
(own pre-list, part alternates, DObjDLLinks, the other heads, skeleton
walks). For every triangle it produces the joint, the owning joint of each
vertex, the position, normal or vertex colour, alpha, the texel-space UV
(s * scale - tile origin), the texture identity (image address, tile
format/mask/clamp, TLUT) and the material (list bits, shaded mode, alpha
and PRIM sources, light colours, ENV/TEXLERP combiners).

A third reader checks the first: the same command bytes are fed to glank's
gfxdis (the C reference, as verify_dl.py does) and its vertex loads,
triangle index triples and gsSPTexture values must equal the interpreter's,
in order.

Against that, the pack is read back through fighter.h's FPack* layouts and,
unit by unit (a unit is the triangles of one joint under one model-part
tag), every triangle must exist on both sides with the same positions, the
same vertex owners, the same normals/colours/alpha and the same UV in texel
units, and every batch's list, filter, cull and texgen bits, shaded mode,
alpha/prim sources, ENV and PRIM and light colours, texturedness and the
texture-key -> texture-index map must agree with the reference triangles it
holds. The joint table is compared with the hand-decoded tree.

What is shared with the exporter, by necessity: WHICH trees, part
alternates, skip masks, skeleton rows, MObjSub tables (costume folded) and
render-mode seeds a pack is baked from -- those are resolved from the
decomp's symbol tables and are policy (they are captured from the real
exporter's own bake() calls so a change there is followed), and the MObjSub
struct reader. Texel values ARE covered (tex_values): every tile's load is
replayed into a TMEM of the check's own -- LoadBlock's dxt row counter,
LoadTile's rectangle, the odd-row word swap, the render tile's stride,
clamp, mirror and mask, the TLUT, images and palettes in another file by
the extern chain -- and read against the shipped texels after the PVR
format's rounding; only RGBA32 tiles are skipped. Not covered: costume
materials of non-zero costumes, animation scripts (pack_anim_check, anm_body_check), and
the order of triangles within a unit.

Usage: python3 tools/check/model_pack_check.py [--rom <rom.z64>]
           [--only name[,name...]] [--jobs N] [--list] [--selftest]
           [--no-selftest]
"""
import collections
import concurrent.futures
import contextlib
import copy
import io
import math
import os
import re
import struct
import subprocess
import sys
import tempfile

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract               # noqa: E402
import ssb_paths as SP           # noqa: E402
import ssb_assets as A           # noqa: E402
import ssb_meshexport as M       # noqa: E402
import effect_model_check as EM  # noqa: E402
import texsheet as TS            # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

# -- F3DEX2 constants, written out here (not taken from ssb_assets) --------
OP_VTX, OP_MODIFYVTX, OP_TRI1, OP_TRI2, OP_QUAD = 0x01, 0x02, 0x05, 0x06, 0x07
OP_DL, OP_ENDDL, OP_TEXTURE, OP_GEOMODE = 0xDE, 0xDF, 0xD7, 0xD9
OP_MOVEWORD = 0xDB
OP_SETOTHERL, OP_SETOTHERH, OP_RDPOTHER = 0xE2, 0xE3, 0xEF
OP_SETCOMBINE, OP_PRIM, OP_ENV = 0xFC, 0xFA, 0xFB
OP_SETTIMG, OP_SETTILE, OP_TILESIZE = 0xFD, 0xF5, 0xF2
OP_LOADBLOCK, OP_LOADTILE, OP_LOADTLUT = 0xF3, 0xF4, 0xF0
GM_LIGHTING, GM_TEXGEN, GM_CULL_FRONT, GM_CULL_BACK = \
    0x20000, 0x40000, 0x200, 0x400
RM_CVG_X_ALPHA, RM_FORCE_BL = 0x1000, 0x4000
CYC_2CYCLE = 1
H_RESET = 2 << 12                # sSYRdpResetDisplayList: bilerp filter
B_LIST_MASK = 3
B_FILT, B_CULLB, B_CULLF, B_TEXGEN = 8, 64, 128, 256
B_ENVLERP, B_TEXLERP = 16, 32
B_ZALWAYS = 1024                 # ssb_assets.FPACK_ZALWAYS
GM_ZBUFFER = 0x1
RM_Z_CMP = 0x10
MOBJ_ALPHA, MOBJ_SPLIT, MOBJ_PALETTE, MOBJ_FRAC = 1, 2, 4, 16
MOBJ_TILESIZE, MOBJ_TILESIZE1, MOBJ_TEXTURE = 0x20, 0x40, 0x80
MOBJ_PRIM, MOBJ_ENV, MOBJ_L1, MOBJ_L2 = 1 << 9, 1 << 10, 1 << 12, 1 << 13
EPS = 1.0 / 65535.0

TOL_N = 2e-5            # normal / colour (f32 of a float64)
TOL_UV = 2e-3           # texels
ROM_PATH = None
# Every file a SETTIMG has pointed into, by fid, for the texel replay
# (tex_values): the key names an image by (fid, offset), not by bytes.
FILES = {}
MAX_REPORT = 6          # failures printed per category per pack


def be32(f, o):
    return struct.unpack_from(">I", f, o)[0]


def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


# -- file access --------------------------------------------------------------

_RELOC = {}


def reloc_of(f):
    """The intern reloc chain of a RelocFile, via ssb_extract directly."""
    fid = f.fid
    if fid not in _RELOC:
        e = ssb_extract.read_entry(f.rom, ssb_extract.RELOC_SEG, fid)
        idx, out = e["reloc_intern"], {}
        while idx != 0xFFFF:
            nxt, wn = struct.unpack_from(">HH", f, idx * 4)
            out[idx * 4] = wn * 4
            idx = nxt
        _RELOC[fid] = out
    return _RELOC[fid]


_RELOC_X = {}


def reloc_extern_of(f):
    """The EXTERN reloc chain of a RelocFile: {site: (file id, target
    byte)}. The chain is the intern one's shape; the target file is one
    u16 per link from a ROM list right after the file's stored data
    (lb/lbreloc.c), read here off the ROM directly."""
    fid = getattr(f, "fid", None)
    if fid is None or not hasattr(f, "rom"):
        return {}
    if fid not in _RELOC_X:
        e = ssb_extract.read_entry(f.rom, ssb_extract.RELOC_SEG, fid)
        csr = (ssb_extract.rom_table_hi(ssb_extract.RELOC_SEG,
                                        ssb_extract.FILE_COUNT) +
               e["data_offset"] + e["compressed_size"] * 4)
        idx, out = e["reloc_extern"], {}
        while idx != 0xFFFF:
            nxt, wn = struct.unpack_from(">HH", f, idx * 4)
            out[idx * 4] = (struct.unpack_from(">H", f.rom, csr)[0], wn * 4)
            csr += 2
            idx = nxt
        _RELOC_X[fid] = out
    return _RELOC_X[fid]


def file_of(f, fid):
    """relocData file `fid`, for an image an extern pointer lands in."""
    if fid not in FILES:
        FILES[fid] = A.get_file(f.rom, fid, f.extract)
    return FILES[fid]


class Src(object):
    def __init__(self, f, reloc=None):
        self.f = f
        self.reloc = reloc if reloc is not None else reloc_of(f)
        self.fid = getattr(f, "fid", id(f))

    def ptr(self, site):
        """Target file offset of the pointer word at `site`, None for NULL;
        an unrelocated non-zero word is an error (the baker's token rule
        is exactly what this is here to not use)."""
        t = self.reloc.get(site)
        if t is None and be32(self.f, site) != 0:
            return ("tok", be32(self.f, site))
        return t


# -- the hand-decoded DObjDesc tree --------------------------------------------

class Node(object):
    __slots__ = ("idx", "jid", "parent", "children", "dl", "pre", "links",
                 "T", "R", "S")


def raw_tree(src, off, dls_pair=False, dl_links=False):
    f, nodes, slots = src.f, [], [None] * 18
    i = 0
    while True:
        b = off + 44 * i
        jid = be32(f, b) & 0xFFF
        if jid == 18:
            break
        n = Node()
        n.idx, n.jid, n.children, n.links, n.pre = i, jid, [], [], None
        n.dl = src.ptr(b + 4)
        n.T = struct.unpack_from(">3f", f, b + 8)
        n.R = struct.unpack_from(">3f", f, b + 20)
        n.S = struct.unpack_from(">3f", f, b + 32)
        if dls_pair and n.dl is not None:
            pair = n.dl
            n.pre, n.dl = src.ptr(pair), src.ptr(pair + 4)
        if dl_links and n.dl is not None:
            arr, n.dl, k = n.dl, None, 0
            while True:
                lid = struct.unpack_from(">i", f, arr + 8 * k)[0]
                if lid == 4:
                    break
                dl = src.ptr(arr + 8 * k + 4)
                if dl is not None:
                    n.links.append((lid, dl))
                k += 1
                if k > 16:
                    raise ValueError("unterminated DObjDLLink array")
        n.parent = None if jid == 0 else slots[jid - 1]
        if jid != 0 and n.parent is None:
            raise ValueError("DObjDesc[%d] id %d has no parent" % (i, jid))
        if n.parent is not None:
            n.parent.children.append(n)
        slots[jid] = n
        nodes.append(n)
        i += 1
    return nodes


# -- combiner, other-mode ------------------------------------------------------

def comb_fields(w0, w1):
    """((a,b,c,d) colour, (Aa,Ab,Ac,Ad) alpha) for cycle 0, and for cycle 1."""
    c0 = ((w0 >> 20) & 0xF, (w1 >> 28) & 0xF, (w0 >> 15) & 0x1F,
          (w1 >> 15) & 0x7)
    a0 = ((w0 >> 12) & 7, (w1 >> 12) & 7, (w0 >> 9) & 7, (w1 >> 9) & 7)
    c1 = ((w0 >> 5) & 0xF, (w1 >> 24) & 0xF, w0 & 0x1F, (w1 >> 6) & 7)
    a1 = ((w1 >> 21) & 7, (w1 >> 3) & 7, (w1 >> 18) & 7, w1 & 7)
    return c0, a0, c1, a1


DEFAULT_COMB = ((0xF, 0xF, 0x1F, 7), (7, 7, 7, 6), (0xF, 0xF, 0x1F, 7),
                (7, 7, 7, 6))


def colour_uses(a, b, c, d):
    """(TEXEL0, PRIM, SHADE) read by a colour cycle: A and B are 4-bit with
    8..15 the literal 0, C 5-bit with 16..31, D 3-bit with 7."""
    used = set()
    if a < 8:
        used.add(a)
    if b < 8:
        used.add(b)
    if c < 16:
        used.add(c)
    if d < 7:
        used.add(d)
    return (1 in used, 3 in used, 4 in used)


def alpha_uses(a, b, c, d):
    """(TEXEL0, SHADE, PRIM) read by an alpha cycle: 3-bit operands, 7 is 0,
    a zero C kills the product."""
    used = set()
    if c != 7:
        used.update((a, b, c))
    used.add(d)
    used.discard(7)
    return (1 in used, 4 in used, 3 in used)


# -- the interpreter -----------------------------------------------------------

class RefError(Exception):
    pass


def new_state(rendermode, mode0=None, omh=None, z=None):
    """`z` is the baker's `ztrack` seed, as {"zbuffer", "zcmp"}, or None
    where the bake does not track the Z compare (FPACK_ZALWAYS)."""
    if z is not None:
        mode0 = (GM_LIGHTING | GM_CULL_BACK if mode0 is None else mode0)
        mode0 = (mode0 | GM_ZBUFFER) if z["zbuffer"] else \
            (mode0 & ~GM_ZBUFFER)
    return {
        "z": z,
        "mode": GM_LIGHTING | GM_CULL_BACK if mode0 is None else mode0,
        "tex_on": False, "scale": (1.0, 1.0),
        "timg": None, "timg_w": 0, "tlut": None, "load": None,
        "tile": {"fmt": 0, "siz": 0, "line": 0, "cms": 0, "masks": 0,
                 "cmt": 0, "maskt": 0, "tmem": 0, "pal": 0},
        "load_tile": {"tmem": 0, "line": 0},
        "tsize": (0.0, 0.0, 0.0, 0.0),
        "comb": DEFAULT_COMB,
        "prim": 0xFFFFFFFF, "env": 0x000000FF,
        "l1": 0xFFFFFF00, "l2": 0,
        "oml": list(rendermode), "oml_rm": [m is not None for m in rendermode],
        "omh": list(omh) if omh else [H_RESET] * 4,
        "mobj_imgs": None,
    }


class RefRun(object):
    """One display walk of one model. See the module doc."""

    def __init__(self, rendermode=None, mode0=None, omh=None, z=None):
        self.rendermode0, self.mode0, self.omh0 = \
            list(rendermode or [None] * 4), mode0, omh
        self.z0 = z
        self.vbuf = {}
        self.st = new_state(self.rendermode0, mode0, omh, z)
        self.units = collections.OrderedDict()   # (joint, tag) -> [tri]
        self.flat = bytearray()
        self.ev_vtx, self.ev_tri, self.ev_tex = [], [], []
        self.tok_checks = 0
        self.mismatch_tok = []
        self.ntri = 0

    # state snapshot for the part-alternate walks
    def snapshot(self):
        return copy.deepcopy((self.vbuf, self.st))

    def restore(self, snap):
        self.vbuf, self.st = copy.deepcopy(snap)

    # -- one display list ------------------------------------------------------
    def run(self, src, subs, node_idx, off, head=0, tag=0):
        self._dl(src, subs, node_idx, off, head, tag, 0, True)

    def _dl(self, src, subs, ni, off, head, tag, depth, top):
        if depth > 8:
            raise RefError("DL nesting deeper than 8")
        f, st = src.f, self.st
        n = 0
        while True:
            n += 1
            if n > 8192:
                raise RefError("display list runs on")
            w0, w1 = struct.unpack_from(">II", f, off)
            op = w0 >> 24
            if op == OP_ENDDL:
                return
            if op == OP_DL:
                if (w1 >> 16) == 0x0E00:
                    self.flat += f[off:off + 8]
                    self._mobj(src, subs, ni, w1 & 0xFFFFFF)
                    off += 8
                    continue
                tgt = src.ptr(off + 4)
                if not isinstance(tgt, int) or not 0 < tgt < len(f) - 8:
                    raise RefError("gsSPDisplayList to bad target %r" % (tgt,))
                if (w0 >> 16) & 0xFF:        # branch
                    off = tgt
                    continue
                self._dl(src, subs, ni, tgt, head, tag, depth + 1, False)
                off += 8
                continue
            self.flat += f[off:off + 8]
            self._cmd(src, ni, off, op, w0, w1, head, tag)
            off += 8

    # -- commands --------------------------------------------------------------
    def _cmd(self, src, ni, off, op, w0, w1, head, tag):
        st = self.st
        if op == OP_VTX:
            nv = (w0 >> 12) & 0xFF
            v0 = ((w0 & 0xFF) >> 1) - nv
            tgt = src.ptr(off + 4)
            if not isinstance(tgt, int):
                raise RefError("gsSPVertex at 0x%X: unrelocated %r"
                               % (off, tgt))
            self.ev_vtx.append((nv, v0, w1))
            self.tok_checks += 1
            if tgt != (w1 & 0xFFFF) * 4:
                self.mismatch_tok.append((off, tgt, (w1 & 0xFFFF) * 4))
            lit = bool(st["mode"] & GM_LIGHTING)
            for i in range(nv):
                p = src.f[tgt + 16 * i:tgt + 16 * i + 16]
                x, y, z = struct.unpack(">3h", p[0:6])
                s, t = struct.unpack(">2h", p[8:12])
                if lit:
                    nx, ny, nz = struct.unpack(">3b", p[12:15])
                    ln = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
                    nrm = (nx / ln, ny / ln, nz / ln)
                else:
                    nrm = (p[12] / 255.0, p[13] / 255.0, p[14] / 255.0)
                self.vbuf[v0 + i] = (float(x), float(y), float(z), s / 32.0,
                                     t / 32.0, nrm, p[15], ni, lit)
        elif op == OP_MODIFYVTX:
            where, slot = (w0 >> 16) & 0xFF, (w0 & 0xFFFF) // 2
            if where != 0x14:
                raise RefError("gsSPModifyVertex where 0x%X" % where)
            s, t = (w1 >> 16) & 0xFFFF, w1 & 0xFFFF
            s = (s - 65536 if s >= 32768 else s) / 32.0
            t = (t - 65536 if t >= 32768 else t) / 32.0
            # the RSP's cached coordinates are post-gsSPTexture-scale and
            # this writes them as they are; _tri scales, so divide it out
            sc = st["scale"]
            s = s / sc[0] if sc[0] else s
            t = t / sc[1] if sc[1] else t
            v = self.vbuf[slot]
            self.vbuf[slot] = v[:3] + (s, t) + v[5:]
        elif op == OP_TRI1:
            self.ev_tri.append(((w0 >> 16 & 0xFF) // 2, (w0 >> 8 & 0xFF) // 2,
                                (w0 & 0xFF) // 2))
            self._tri(ni, head, tag, self.ev_tri[-1])
        elif op == OP_TRI2:
            for w in (w0, w1):
                t3 = ((w >> 16 & 0xFF) // 2, (w >> 8 & 0xFF) // 2,
                      (w & 0xFF) // 2)
                self.ev_tri.append(t3)
                self._tri(ni, head, tag, t3)
        elif op == OP_QUAD:
            raise RefError("gsSP1Quadrangle at 0x%X" % off)
        elif op == OP_TEXTURE:
            s, t = w1 >> 16, w1 & 0xFFFF
            on = (w0 >> 1) & 0x7F
            self.ev_tex.append((s, t, (w0 >> 11) & 7, (w0 >> 8) & 7, on))
            st["scale"] = (s / 65536.0, t / 65536.0)
            st["tex_on"] = bool(on)
        elif op == OP_GEOMODE:
            st["mode"] = (st["mode"] & (w0 & 0xFFFFFF)) | w1
        elif op == OP_MOVEWORD:
            if ((w0 >> 16) & 0xFF) == 0x0A:               # G_MW_LIGHTCOL
                n = (w0 & 0xFFFF) // 0x18 + 1             # a/b words of LIGHT_n
                if n == 1:
                    st["l1"] = w1
                elif n == 2:
                    st["l2"] = w1
        elif op == OP_SETOTHERH or op == OP_SETOTHERL:
            ln = (w0 & 0xFF) + 1
            sh = 32 - ((w0 >> 8) & 0xFF) - ln
            mask = ((1 << ln) - 1) << sh
            if op == OP_SETOTHERH:
                st["omh"][head] = (st["omh"][head] & ~mask) | (w1 & mask)
            else:
                prev = st["oml"][head] or 0
                st["oml"][head] = (prev & ~mask) | (w1 & mask)
                if mask & ~0x7:
                    st["oml_rm"][head] = True
        elif op == OP_RDPOTHER:
            st["omh"][head] = w0 & 0xFFFFFF
            st["oml"][head] = w1
            st["oml_rm"][head] = True
        elif op == OP_SETCOMBINE:
            st["comb"] = comb_fields(w0, w1)
        elif op == OP_PRIM:
            st["prim"] = w1
        elif op == OP_ENV:
            st["env"] = w1
        elif op == OP_SETTIMG:
            st["timg"] = (src.fid, src.ptr(off + 4), (w0 >> 21) & 7,
                          (w0 >> 19) & 3)
            ext = reloc_extern_of(src.f).get(off + 4)
            if ext is not None:
                # an image in another file (the polygon fighters' and the
                # bonus courses' shared textures)
                file_of(src.f, ext[0])
                st["timg"] = (ext[0], ext[1]) + st["timg"][2:]
            st["timg_w"] = (w0 & 0xFFF) + 1
            FILES[src.fid] = src.f
        elif op == OP_LOADBLOCK:
            st["load"] = ("blk", st["timg"], (w1 >> 12) & 0xFFF, w1 & 0xFFF,
                          st["load_tile"]["tmem"])
        elif op == OP_LOADTILE:
            st["load"] = ("til", st["timg"], (w0 >> 12) & 0xFFF,
                          w0 & 0xFFF, (w1 >> 12) & 0xFFF, w1 & 0xFFF,
                          st["timg_w"], st["load_tile"]["tmem"],
                          st["load_tile"]["line"])
        elif op == OP_LOADTLUT:
            st["tlut"] = st["timg"]
        elif op == OP_SETTILE:
            if ((w1 >> 24) & 7) == 0:
                st["tile"] = {"fmt": (w0 >> 21) & 7, "siz": (w0 >> 19) & 3,
                              "line": (w0 >> 9) & 0x1FF,
                              "cmt": (w1 >> 18) & 3, "maskt": (w1 >> 14) & 0xF,
                              "cms": (w1 >> 8) & 3, "masks": (w1 >> 4) & 0xF,
                              "tmem": (w0 & 0x1FF) * 8,
                              "pal": (w1 >> 20) & 0xF}
            elif ((w1 >> 24) & 7) == 7:
                st["load_tile"] = {"tmem": (w0 & 0x1FF) * 8,
                                   "line": (w0 >> 9) & 0x1FF}
        elif op == OP_TILESIZE:
            if ((w1 >> 24) & 7) == 0:
                st["tsize"] = (((w0 >> 12) & 0xFFF) / 4.0, (w0 & 0xFFF) / 4.0,
                               ((w1 >> 12) & 0xFFF) / 4.0, (w1 & 0xFFF) / 4.0)

    # -- gcDrawMObjForDObj, ported from sys/objdisplay.c ----------------------
    def _mobj(self, src, subs, ni, addr):
        st = self.st
        i = addr // 8
        lst = subs[ni] if ni < len(subs) else []
        FILES[src.fid] = src.f
        if i >= len(lst):
            raise RefError("DObj %d has no MObj %d" % (ni, i))
        sub = lst[i]
        flags = sub["flags"] or (MOBJ_TEXTURE | MOBJ_TILESIZE | MOBJ_ALPHA)
        scau, scav, trau, trav = sub["scau"], sub["scav"], sub["trau"], \
            sub["trav"]
        if flags & (MOBJ_TEXTURE | MOBJ_TILESIZE1 | MOBJ_TILESIZE):
            if sub["unk10"] == 1:
                scau *= 0.5
                trau = ((trau - sub["unk24"]) + 1.0 - sub["unk28"] * 0.5) * 0.5
        imgs = []

        def slot_file(field, k, off_):
            # the array slot's own site, against this file's extern chain:
            # Yoshi's Island's, Dream Land's and the Bonus courses' sprite
            # and palette arrays point into another file
            arr = src.reloc.get(sub["off"] + field) if "off" in sub else None
            ext = None if arr is None else \
                reloc_extern_of(src.f).get(arr + 4 * k)
            if ext is None:
                return src.fid, off_
            file_of(src.f, ext[0])
            return ext
        if flags & MOBJ_PALETTE and sub["palettes"]:
            pal = sub["palettes"][sub["palette_id"]]
            st["timg"] = slot_file(0x2C, sub["palette_id"], pal) + (0, 2)
            imgs.append(st["timg"])
            if flags & (MOBJ_SPLIT | MOBJ_ALPHA):
                st["tlut"] = st["timg"]
        if flags & MOBJ_L1:
            st["l1"] = sub["light1"]
        if flags & MOBJ_L2:
            st["l2"] = sub["light2"]
        if flags & (MOBJ_PRIM | MOBJ_FRAC | 0x8):
            st["prim"] = sub["primcolor"]
        if flags & MOBJ_ENV:
            st["env"] = sub["envcolor"]
        if flags & (MOBJ_FRAC | MOBJ_ALPHA) and sub["sprites"]:
            st["timg"] = slot_file(0x04, sub["texture_id_curr"],
                                   sub["sprites"][sub["texture_id_curr"]]) \
                + (sub["fmt"], sub["siz"])
            imgs.append(st["timg"])
        if flags & MOBJ_TILESIZE:
            w, h = sub["tile_w"], sub["tile_h"]
            if sub["unk10"] == 2:
                uls = int(((w * trau) / scau) * 4.0) if abs(scau) > EPS else 0
                ult = int(((h * trav) / scav) * 4.0) if abs(scav) > EPS else 0
                uls, ult = max(uls, 0), max(ult, 0)
            else:
                uls = int((((w * trau) + sub["unk0A"]) / scau) * 4.0) \
                    if abs(scau) > EPS else 0
                ult = int(((((1.0 - scav) - trav) * h + sub["unk0A"]) / scav)
                          * 4.0) if abs(scav) > EPS else 0
            st["tsize"] = (uls / 4.0, ult / 4.0,
                           (((w - 1) << 2) + uls) / 4.0,
                           (((h - 1) << 2) + ult) / 4.0)
        if flags & MOBJ_TEXTURE:
            if sub["unk10"] == 2:
                s = (sub["tile_w"] * 64) / scau if abs(scau) > EPS else 0.0
                t = (sub["tile_h"] * 64) / scav if abs(scav) > EPS else 0.0
            else:
                s = (2097152.0 / sub["unk08"]) / scau if abs(scau) > EPS \
                    else 0.0
                t = (2097152.0 / sub["unk08"]) / scav if abs(scav) > EPS \
                    else 0.0
            # gSPTexture's operands are u16: a float over 0xFFFF clamps, and
            # the conversion truncates
            st["scale"] = (int(min(s, 0xFFFF)) / 65536.0,
                           int(min(t, 0xFFFF)) / 65536.0)
            st["tex_on"] = True

    # -- a triangle ------------------------------------------------------------
    def _tri(self, ni, head, tag, slots):
        st = self.st
        vs = []
        for s in slots:
            v = self.vbuf.get(s)
            if v is None:
                raise RefError("triangle reads empty vertex slot %d" % s)
            vs.append(v)
        lits = {v[8] for v in vs}
        want = bool(st["mode"] & GM_LIGHTING)
        if lits != {want}:
            raise RefError("triangle mixes G_LIGHTING states")
        c0, a0, c1, a1 = st["comb"]
        two = ((st["omh"][head] >> 20) & 3) == CYC_2CYCLE
        u_tex, u_prim, u_shade = colour_uses(*c0)
        a_tex, a_shade, a_prim = alpha_uses(*a0)
        textured = bool((u_tex or a_tex) and st["tex_on"]
                        and st["timg"] is not None)
        # list bits
        mode = st["oml"][head]
        if mode is None or not st["oml_rm"][head]:
            lst = 2 if head & 1 else 0
        elif mode & RM_FORCE_BL:
            lst = 2
        elif mode & RM_CVG_X_ALPHA:
            lst = 1
        else:
            lst = 0
        bucket = lst | (B_FILT if ((st["omh"][head] >> 12) & 3) == 0 else 0) \
            | (B_CULLB if st["mode"] & GM_CULL_BACK else 0) \
            | (B_CULLF if st["mode"] & GM_CULL_FRONT else 0) \
            | (B_TEXGEN if st["mode"] & GM_TEXGEN else 0)
        if st["z"] is not None:
            zcmp = (bool(mode & RM_Z_CMP)
                    if mode is not None and st["oml_rm"][head]
                    else st["z"]["zcmp"][head])
            if not (zcmp and st["mode"] & GM_ZBUFFER):
                bucket |= B_ZALWAYS
        shaded = (1 if want else 2) if u_shade else 0
        prim = ((st["prim"] & 0xFFFFFF00) if u_prim else 0xFFFFFF00) | \
               ((st["prim"] & 0xFF) if a_prim else 0xFF)
        asrc = (1 if a_tex else 0) | (2 if a_shade else 0) | \
               (4 if a_prim else 0) | (8 if u_prim else 0)
        envlerp = (c0[0] == 3 and c0[1] == 5 and c0[2] == 1 and c0[3] == 5)
        texlerp = (c0[0] == 2 and c0[1] == 1 and c0[2] == 14 and c0[3] == 1)
        bucket |= (B_ENVLERP if envlerp else 0) | \
                  (B_TEXLERP if texlerp else 0)
        t = st["tile"]
        tkey = None
        if textured:
            tkey = (st["load"], t["fmt"], t["siz"], t["line"], t["masks"],
                    t["maskt"], t["cms"], t["cmt"], st["tsize"],
                    st["tlut"] if t["fmt"] == 2 else None, t["tmem"],
                    t["pal"], bool(a_tex and not u_tex))
        sc, uls, ult = st["scale"], st["tsize"][0], st["tsize"][1]
        out = []
        for v in vs:
            out.append((v[7], v[0], v[1], v[2], v[5][0], v[5][1], v[5][2],
                        v[6],
                        v[3] * sc[0] - uls, v[4] * sc[1] - ult))
        rec = {"joint": ni, "v": out, "textured": textured, "tkey": tkey,
               "bucket": bucket, "shaded": shaded, "asrc": asrc, "prim": prim,
               "l1": st["l1"], "l2": st["l2"], "two": two,
               "env": (st["env"] >> 8) if envlerp else 0,
               "texgen": bool(st["mode"] & GM_TEXGEN)}
        self.units.setdefault((ni, tag), []).append(rec)
        self.ntri += 1


# -- the walk, in gcDrawDObjTree / ftDisplayMain order ------------------------

def walk(run, src, nodes, subs, skip=frozenset(), parts=None, skeleton=None,
         part_files=None):
    """Mirror of the draw order the pack's batches were emitted in."""
    parts = parts or {}
    roots = [n for n in nodes if n.parent is None]

    def sub_ctx(ni, psubs):
        if psubs is None:
            return subs
        s = list(subs) + [[]] * (ni + 1 - len(subs))
        s[ni] = psubs
        return s

    def visit(n):
        if n.idx not in skip:
            if n.pre is not None and n.parent is not None:
                run.run(src, subs, n.parent.idx, n.pre)
            alts = parts.get(n.idx)
            if alts:
                before = run.snapshot()
                if n.dl is not None and all(dl is not None
                                            for _t, dl, _s, _f in alts):
                    run.run(src, subs, n.idx, n.dl)
                after = run.snapshot()
                for tag, dl, psubs, pf in alts:
                    if dl is None:
                        run.restore(after)
                        run.run(src, subs, n.idx, n.dl, tag=tag)
                        after = run.snapshot()
                    else:
                        run.restore(before)
                        psrc = src if pf is None else Src(pf)
                        run.run(psrc, sub_ctx(n.idx, psubs), n.idx, dl,
                                tag=tag)
                run.restore(after)
            elif n.dl is not None:
                run.run(src, subs, n.idx, n.dl)
            for lid, dl in n.links:
                if lid == 0:
                    run.run(src, subs, n.idx, dl, head=0)
        for c in n.children:
            visit(c)

    for r in roots:
        visit(r)
    for head in (2, 1, 3):
        for n in nodes:
            if n.idx in skip:
                continue
            for lid, dl in n.links:
                if lid == head:
                    run.run(src, subs, n.idx, dl, head=lid)
    for sid in sorted(skeleton or {}):
        rows = skeleton[sid]
        run.vbuf = {}
        run.st = new_state(run.rendermode0, run.mode0, run.omh0, run.z0)

        def sk(n):
            if n.idx not in skip and n.idx in rows:
                tag, pre, dl = rows[n.idx]
                if pre is not None:
                    run.run(src, subs, n.parent.idx, pre, tag=tag)
                if dl is not None:
                    run.run(src, subs, n.idx, dl, tag=tag)
            for c in n.children:
                sk(c)
        for r in roots:
            sk(r)


# -- capturing the exporter's own inputs ---------------------------------------

class Capture(object):
    """Patch ssb_assets so every read_dobj_tree and MeshBaker.bake call the
    exporter makes is recorded with its arguments."""

    def __init__(self):
        self.trees = {}       # id(root) -> (src info, off, flags)
        self.bakes = []       # records
        self.ctx = []         # label stack
        self.keep = []

    def __enter__(self):
        cap = self
        self.o_tree, self.o_bake = A.read_dobj_tree, A.MeshBaker.bake

        def tree(filedata, reloc, dobj_off, dls_pair=False, dl_links=False):
            r = cap.o_tree(filedata, reloc, dobj_off, dls_pair=dls_pair,
                           dl_links=dl_links)
            cap.trees[id(r[0])] = (filedata, reloc, dobj_off, dls_pair,
                                   dl_links, r[1])
            cap.keep.append(r)
            return r

        def bake(self_, root, skip=frozenset(), parts=None, skeleton=None):
            info = cap.trees.get(id(root))
            rec = {"baker": self_, "root": root, "skip": set(skip),
                   "parts": {i: [(t, d, copy.deepcopy(sb), pf) for t, d, sb, pf in alts]
                     for i, alts in (parts or {}).items()},
                   "skeleton": copy.deepcopy(skeleton) if skeleton else {},
                   "tree": info, "ctx": tuple(cap.ctx),
                   "subs": copy.deepcopy(self_.mobjsubs),
                   "rendermode": list(self_.rendermode),
                   "seed": dict(self_.seed), "f": self_.f,
                   "seq": len(cap.bakes)}
            out = cap.o_bake(self_, root, skip, parts, skeleton)
            rec["nodes"] = list(self_.nodes)
            cap.bakes.append(rec)
            return out
        A.read_dobj_tree = tree
        A.MeshBaker.bake = bake
        return self

    def __exit__(self, *a):
        A.read_dobj_tree, A.MeshBaker.bake = self.o_tree, self.o_bake

    def wrap(self, mod, name, label):
        orig = getattr(mod, name)
        cap = self

        def w(*a, **k):
            cap.ctx.append(label)
            try:
                return orig(*a, **k)
            finally:
                cap.ctx.pop()
        setattr(mod, name, w)
        return lambda: setattr(mod, name, orig)

    def for_baker(self, baker):
        for r in self.bakes:
            if r["baker"] is baker:
                return r
        return None


# -- gfxdis, the third reader --------------------------------------------------

def gfxdis_events(flat):
    """Vertex loads (n, v0, word), triangle triples and gsSPTexture tuples
    from glank's gfxdis, in order."""
    with tempfile.NamedTemporaryFile(suffix=".dl", delete=False) as t:
        t.write(bytes(flat))
        path = t.name
    try:
        out = subprocess.run([SP.GFXDIS, "-f", path], capture_output=True,
                             text=True).stdout
    finally:
        os.unlink(path)
    vtx, tri, tex = [], [], []
    for line in out.splitlines():
        m = re.match(r"\s*gs(\w+)\((.*)\),?\s*$", line)
        if not m:
            continue
        name, args = m.group(1), [x.strip() for x in m.group(2).split(",")]
        if name == "SPVertex":
            vtx.append((int(args[1], 0), int(args[2], 0), int(args[0], 0)))
        elif name == "SP1Triangle":
            tri.append(tuple(int(x, 0) for x in args[:3]))
        elif name == "SP2Triangles":
            v = [int(x, 0) for x in args]
            tri.append(tuple(v[0:3]))
            tri.append(tuple(v[4:7]))
        elif name == "SP1Quadrangle":
            v = [int(x, 0) for x in args]
            tri.append((v[0], v[1], v[2]))
            tri.append((v[0], v[2], v[3]))
        elif name == "SPTexture":
            tex.append(tuple(int(x, 0) if re.match(r"^-?(0x)?[0-9a-fA-F]+$", x)
                             else x for x in args))
    return vtx, tri, tex


# -- the pack side -------------------------------------------------------------

def read_model(blob):
    """The model sections of an SSBPACKA blob (fighter.h), without the
    animation directory (a fighter's words live in its .anm)."""
    hd = struct.unpack_from("<8s8I8I3ff8s8I", blob, 0)
    if hd[0] != b"SSBPACKA":
        raise ValueError("bad magic %r" % hd[0])
    nj, nv, nt, nb, nx, npl = hd[1:7]
    o_j, o_v, o_t, o_b, o_x, o_p, o_d = hd[9:16]
    p = {"hdr": hd, "blob": blob, "njoint": nj, "nvert": nv, "ntri": nt,
         "nbatch": nb, "ntex": nx, "npal": npl, "texdata": o_d,
         "texlen": hd[8]}
    p["joints"] = [struct.unpack_from("<i9f", blob, o_j + i * 40)
                   for i in range(nj)]
    p["verts"] = [struct.unpack_from("<8f2BH", blob, o_v + i * 36)
                  for i in range(nv)]
    p["tris"] = [struct.unpack_from("<3H", blob, o_t + i * 6)
                 for i in range(nt)]
    p["batches"] = [struct.unpack_from("<2Hh2B2H4I", blob, o_b + i * 28)
                    for i in range(nb)]
    p["texs"] = [struct.unpack_from("<2I2H4B", blob, o_x + i * 16)
                 for i in range(nx)]
    p["pals"] = [struct.unpack_from("<16H", blob, o_p + i * 32)
                 for i in range(npl)]
    return p


def tex_key_pack(p, i):
    t = p["texs"][i]
    off = p["texdata"] + t[0]
    return (t[2], t[3], t[6], t[5], bytes(p["blob"][off:off + t[1]]))


def pack_units(p):
    """{(joint, part): [(batch index, tri)]} and the per-vertex data."""
    units = collections.OrderedDict()
    for bi, b in enumerate(p["batches"]):
        part = b[6] >> 8
        lst = units.setdefault((b[4], part), [])
        for ti in range(b[0], b[0] + b[1]):
            lst.append((bi, ti))
    return units


def coarse(vs):
    return tuple((int(v[0]), int(v[1]), int(v[2]), int(v[3]), int(v[7]))
                 for v in vs)


def close(a, b, tol):
    return abs(a - b) <= tol + 1e-5 * abs(b)


def fine_match(rv, pv):
    """rv: reference vertices (owner x y z nx ny nz alpha u v, uv in
    texels); pv: pack vertices as (nx, ny, nz, u, v), uv in texels. Returns
    the first differing field or None."""
    names = ("nx", "ny", "nz", "u", "v")
    ri = (4, 5, 6, 8, 9)
    for k in range(3):
        for i in range(5):
            tol = TOL_UV if i >= 3 else TOL_N
            if not close(rv[k][ri[i]], pv[k][i], tol):
                return "v%d %s ref %.5g pack %.5g" % (
                    k, names[i], rv[k][ri[i]], pv[k][i])
    return None


# (job label, failure category) pairs that are understood and reported rather
# than failed on: a NEW mismatch still fails the check.
KNOWN = {}


class Report(object):
    def __init__(self, label):
        self.label = label
        self.known_key = label
        self.known = collections.Counter()
        self.fails = []
        self.cat = collections.Counter()
        self.stats = collections.Counter()
        self.notes = collections.Counter()

    def fail(self, cat, msg):
        if (self.known_key, cat) in KNOWN:
            self.known[(self.known_key, cat)] += 1
            return
        self.cat[cat] += 1
        if self.cat[cat] <= MAX_REPORT:
            self.fails.append("%s: %s" % (self.label, msg))
        elif self.cat[cat] == MAX_REPORT + 1:
            self.fails.append("%s: ... more '%s' failures" % (self.label, cat))


def compare_pack(rep, blob, run, nodes, tree_nodes_hint=None, joint_base=0,
                 joint_count=None, seed_note=""):
    """Compare one pack (or a joint range of a merged pack) with a RefRun.
    `nodes` are the hand-decoded Nodes in pack joint order from joint_base."""
    p = read_model(blob)
    # -- the joints
    for n in nodes:
        pj = p["joints"][joint_base + n.idx]
        par = -1 if n.parent is None else n.parent.idx + joint_base
        want = (par,) + tuple(f32(x) for x in n.T + n.R + n.S)
        got = tuple(pj)
        bad = [i for i in range(10)
               if got[i] != want[i] and not (got[i] != got[i]
                                             and want[i] != want[i])]
        if bad:
            rep.fail("joint", "joint %d: pack %s, ROM %s"
                     % (joint_base + n.idx, got, want))
    rep.stats["joints"] += len(nodes)

    # -- units
    pu_all = pack_units(p)
    lo, hi = joint_base, joint_base + (joint_count if joint_count is not None
                                       else len(nodes))
    pu = collections.OrderedDict((k, v) for k, v in pu_all.items()
                                 if lo <= k[0] < hi)
    def shift(r):
        if not joint_base:
            return r
        r = dict(r)
        r["v"] = [(v[0] + joint_base,) + tuple(v[1:]) for v in r["v"]]
        return r
    ru = collections.OrderedDict(((j + joint_base, t), [shift(r) for r in v])
                                 for (j, t), v in run.units.items())
    # pack tri -> vertex tuples
    def pvs(ti, b):
        tex = p["texs"][b[2]] if b[2] >= 0 else None
        w, h = (tex[2], tex[3]) if tex else (1, 1)
        out = []
        for vi in p["tris"][ti]:
            v = p["verts"][vi]
            out.append((int(v[9]), int(v[0]), int(v[1]), int(v[2]),
                        v[3], v[4], v[5], int(v[8]),
                        v[6] * w if tex else 0.0, v[7] * h if tex else 0.0))
        return out

    by_joint_ref = collections.defaultdict(list)
    for k, v in ru.items():
        by_joint_ref[k[0]].append((k, v))
    by_joint_pack = collections.defaultdict(list)
    for k, v in pu.items():
        by_joint_pack[k[0]].append((k, v))

    tex_map = collections.defaultdict(set)         # tkey -> pack tex idx
    for j in sorted(set(by_joint_ref) | set(by_joint_pack)):
        rl = list(by_joint_ref.get(j, []))
        pl = list(by_joint_pack.get(j, []))
        # signature matching, ignoring the tag numbers
        psig = []
        for k, lst in pl:
            vs = [(bi, ti, pvs(ti, p["batches"][bi])) for bi, ti in lst]
            psig.append((k, vs, collections.Counter(
                (coarse(v[2]) for v in vs))))
        rsig = [(k, lst, collections.Counter(coarse(r["v"]) for r in lst))
                for k, lst in rl]
        used = set()
        pairs = []
        for pk, pvl, pc in psig:
            best, bscore = None, -1
            for ri, (rk, rlst, rc) in enumerate(rsig):
                if ri in used:
                    continue
                score = sum((pc & rc).values())
                if score > bscore:
                    best, bscore = ri, score
            if best is None:
                rep.fail("unit", "pack joint %d part %d (%d tris) has no "
                         "reference unit" % (pk[0], pk[1], len(pvl)))
                continue
            used.add(best)
            pairs.append((pk, pvl, pc, rsig[best]))
        for ri, (rk, rlst, rc) in enumerate(rsig):
            if ri not in used:
                rep.fail("unit", "reference joint %d tag %d (%d tris) has no "
                         "pack unit" % (rk[0], rk[1], len(rlst)))
        for pk, pvl, pc, (rk, rlst, rc) in pairs:
            rep.stats["units"] += 1
            if pk[1] != rk[1]:
                rep.fail("tag", "joint %d: pack part %d, exporter tag %d"
                         % (pk[0], pk[1], rk[1]))
            if pc != rc:
                miss = rc - pc
                extra = pc - rc
                rep.fail("tris", "joint %d part %d: ref %d tris, pack %d; "
                         "%d only in ROM (e.g. %s), %d only in pack (e.g. %s)"
                         % (pk[0], pk[1], len(rlst), len(pvl),
                            sum(miss.values()),
                            next(iter(miss)) if miss else None,
                            sum(extra.values()),
                            next(iter(extra)) if extra else None))
            # pair up equal coarse keys, then compare the rest
            pb = collections.defaultdict(list)
            for bi, ti, v in pvl:
                pb[coarse(v)].append((bi, ti, v))
            for r in rlst:
                cands = pb.get(coarse(r["v"]))
                if not cands:
                    continue
                pick = None
                for ci, (bi, ti, v) in enumerate(cands):
                    b = p["batches"][bi]
                    rv_uv = r["v"]
                    if r["textured"] and b[2] >= 0 and not r["texgen"]:
                        pf = fine_match(rv_uv, [(x[4], x[5], x[6], x[8], x[9])
                                                for x in v])
                    elif r["textured"] and b[2] >= 0:
                        pf = fine_match(
                            [x[:8] + (0.0, 0.0) for x in rv_uv],
                            [(x[4], x[5], x[6], 0.0, 0.0) for x in v])
                    else:
                        pf = fine_match(
                            [x[:8] + (0.0, 0.0) for x in rv_uv],
                            [(x[4], x[5], x[6], 0.0, 0.0) for x in v])
                    if pf is None:
                        pick = ci
                        break
                    last = pf
                if pick is None:
                    rep.fail("vertex", "joint %d part %d tri %s: %s"
                             % (pk[0], pk[1], coarse(r["v"])[0][:4], last))
                    cands.pop(0)
                    continue
                bi, ti, v = cands.pop(pick)
                rep.stats["tris"] += 1
                b = p["batches"][bi]
                check_batch(rep, p, bi, b, r, pk, tex_map)
    # texture identity: one key, one texture (or equal contents), and a
    # pack texture at least as big as the tile it stands for
    for key, idxs in tex_map.items():
        _ld, _fm, _sz, _ln, ms, mt, cs, ct, ts, _tl, _tm, _pl, _ao = key
        for axis, m, cm, lo, hi_ in (("w", ms, cs, ts[0], ts[2]),
                                     ("h", mt, ct, ts[1], ts[3])):
            ext = int(math.floor(hi_)) - int(math.floor(lo)) + 1
            need = min(ext, 1 << m) if m else ext
            for i in idxs:
                got = p["texs"][i][2 if axis == "w" else 3]
                if got < min(need, 8) or (got < need and not (cm & 2)
                                          and not m):
                    rep.fail("texsize", "pack texture %d is %s=%d, the ROM "
                             "tile needs at least %d (mask %d, cm %d, tile "
                             "size %s)" % (i, axis, got, need, m, cm, ts))
        if len(idxs) > 1:
            kk = {tex_key_pack(p, i) for i in idxs}
            if len(kk) > 1:
                rep.fail("texmap", "one ROM tile (image %r) maps to %d "
                         "different pack textures %s"
                         % (key[0], len(kk),
                            sorted(idxs)))
        rep.stats["tex_keys"] += 1
    mobj_runs(rep, p)
    tex_values(rep, p, tex_map)
    return p


# -- texel values: the ROM tile through TMEM, against the shipped texels -------
#
# The baker resolves each tile's texels itself: gsDPLoadBlock/LoadTile into
# TMEM, the render tile's line stride and odd-row word swap, its clamp,
# mirror and mask, and the TLUT. Nothing above checks the result -- the
# texture identity compares packs with packs, and texfmt_check proves only
# the per-texel format decode. This replays the load and the fetch the way
# the RDP does them, on a 4 KB TMEM of its own, and reads every texel of
# the shipped (twiddled) texture back against it. The per-texel decode is
# ssb_assets.n64_texel, the one texfmt_check holds to the decomp's.

TMEM_BYTES = 4096
TEXEL_BYTES = {1: 1, 2: 2}      # load sizes this replay models (8b, 16b)


def tmem_load(load):
    """TMEM after one gsDPLoadBlock/LoadTile, or None where the replay does
    not model it (a 4b or 32b load, an unrelocated image)."""
    timg = load[1]
    if timg is None or not isinstance(timg[1], int) or timg[0] not in FILES:
        return None
    data, ptr, siz = FILES[timg[0]], timg[1], timg[3]
    bpt = TEXEL_BYTES.get(siz)
    if bpt is None:
        return None
    tm = bytearray(TMEM_BYTES)
    if load[0] == "blk":
        _k, _t, lrs, dxt, tmem = load
        nbytes = (lrs + 1) * bpt
        counter = 0
        for w in range((nbytes + 7) // 8):
            odd = (counter >> 11) & 1
            for b in range(8):
                si = ptr + w * 8 + b
                tm[(tmem + w * 8 + (b ^ (4 if odd else 0))) % TMEM_BYTES] = \
                    data[si] if si < len(data) else 0
            counter += dxt
        return tm
    _k, _t, uls, ult, lrs, lrt, width, tmem, line = load
    for row, t in enumerate(range(ult >> 2, (lrt >> 2) + 1)):
        for col, x in enumerate(range(uls >> 2, (lrs >> 2) + 1)):
            for b in range(bpt):
                si = ptr + (t * width + x) * bpt + b
                d = tmem + row * line * 8 + col * bpt + b
                tm[(d ^ (4 if row & 1 else 0)) % TMEM_BYTES] = \
                    data[si] if si < len(data) else 0
    return tm


def tile_coord(x, mask, cm, ext):
    """The RDP's clamp, then mirror and mask, of a tile-relative texel."""
    if cm & 2 or not mask:
        x = min(max(x, 0), ext)
    if mask:
        if cm & 1 and (x >> mask) & 1:
            x = ~x
        x &= (1 << mask) - 1
    return x


def rom_tile(key, w, h):
    """The w x h texels the RDP fetches for a tile key, (r, g, b, a) each,
    or None where the replay does not model the load."""
    load, fmt, siz, line, ms, mt, cs, ct, ts, tlut, tmem, pal, aonly = key
    if load is None or siz == 3:
        return None
    tm = tmem_load(load)
    if tm is None:
        return None
    lut = ()
    if fmt == 2:
        if tlut is None or not isinstance(tlut[1], int) or \
                tlut[0] not in FILES:
            return None
        f = FILES[tlut[0]]
        n = min(16 if siz == 0 else 256, (len(f) - tlut[1]) // 2)
        lut = A.read_tlut(f, tlut[1], n)
    exts = int(math.floor(ts[2])) - int(math.floor(ts[0]))
    extt = int(math.floor(ts[3])) - int(math.floor(ts[1]))
    out = []
    for y in range(h):
        ty = tile_coord(y, mt, ct, extt)
        row = tmem + ty * line * 8
        for x in range(w):
            tx = tile_coord(x, ms, cs, exts)
            if siz == 0:
                a, k = row + (tx >> 1), tx & 1
            else:
                a, k = row + tx * (1 << (siz - 1)), 0
            if ty & 1:
                a ^= 4
            out.append(A.n64_texel(tm, a % TMEM_BYTES, k, fmt, siz, lut))
    if aonly:
        # read in the alpha cycle only: the colour cycle never sees the
        # texel, and the pack ships it white (MeshBaker's alpha_only)
        out = [(0xFF, 0xFF, 0xFF, a) for (_r, _g, _b, a) in out]
    return out


def texel_close(r, p, fmt):
    """Whether pack texel p is ROM texel r after the PVR format's rounding.
    A clear texel's colour is not compared: nothing draws it."""
    if fmt == TS.FPACK_TEX_ARGB4444:
        tol_c, tol_a = 18, 18
    else:
        tol_c, tol_a = 9, 127
    if abs(r[3] - p[3]) > tol_a:
        return False
    if r[3] == 0 and p[3] == 0:
        return True
    return all(abs(r[i] - p[i]) <= tol_c for i in range(3))


def tex_values(rep, p, tex_map):
    texs = TS.read_pack(p["blob"])[1]
    for key, idxs in tex_map.items():
        for i in sorted(idxs):
            t = texs[i]
            if t["px"] is None:
                continue
            want = rom_tile(key, t["w"], t["h"])
            if want is None:
                ld = key[0]
                why = ("noload" if ld is None else
                       "siz3" if key[2] == 3 else
                       "tok" if ld[1] is None or not isinstance(ld[1][1], int)
                       else "loadsiz%d_%s" % (ld[1][3], ld[0]) if
                       ld[1][3] not in TEXEL_BYTES else "tlut")
                rep.stats["texels_unmodelled_" + why] += 1
                continue
            bad = [k for k in range(len(want))
                   if not texel_close(want[k], t["px"][k], t["fmt"])]
            rep.stats["textures_valued"] += 1
            if bad:
                k = bad[0]
                rep.fail("texel", "pack texture %d (%dx%d, fmt %d): %d of %d "
                         "texels differ from the ROM tile; first at (%d, %d) "
                         "pack %s ROM %s (key %r)"
                         % (i, t["w"], t["h"], t["fmt"], len(bad), len(want),
                            k % t["w"], k // t["w"], t["px"][k], want[k],
                            key))


def mobj_runs(rep, p):
    """A picture-stepping MObj's run (fighter.h FPackMObjSub.tex_first,
    tex_count) is what every batch under it steps through, unless the
    batch carries its own (FPACK_OWNRUN: its tex is the run's start). So a
    batch under such a MObj must have one or the other: its run starts at
    its own picture. The bug this is for drew Final Destination's boss
    wall as one slice five times: four MObjs, five different pictures
    each, one run."""
    blob = p["blob"]
    om = p["hdr"][-1]
    if not om:
        return
    _nm, osub, _oj, obatch = struct.unpack_from("<4I", blob, om)
    for bi, b in enumerate(p["batches"]):
        mo = struct.unpack_from("<h", blob, obatch + 2 * bi)[0]
        if mo < 0:
            continue
        # FPackMObjSub: flags, prim_l, pad, five colours, then tex_first
        # and tex_count (fighter.h), 68 bytes apiece
        tf, tc = struct.unpack_from("<2h", blob, osub + mo * 68 + 24)
        rep.stats["mobj_batches"] += 1
        if tc < 2:
            continue
        rep.stats["stepping_batches"] += 1
        first = b[2] if b[5] & 512 else tf
        if first < 0 or first + tc > len(p["texs"]):
            rep.fail("mobj_run", "batch %d: its run %d..%d is outside the "
                     "pack's %d textures" % (bi, first, first + tc - 1,
                                             len(p["texs"])))
        elif not b[5] & 512 and b[2] != tf:
            rep.fail("mobj_run", "batch %d draws picture %d but its MObj "
                     "%d steps a run from %d: every frame would be the "
                     "wrong picture (no FPACK_OWNRUN)" % (bi, b[2], mo, tf))


def check_batch(rep, p, bi, b, r, pk, tex_map):
    where = "joint %d part %d batch %d" % (pk[0], pk[1], bi)
    if r["textured"] != (b[2] >= 0):
        rep.fail("textured", "%s: ROM %s, pack texture %d"
                 % (where, "textured" if r["textured"] else "untextured",
                    b[2]))
    elif r["textured"]:
        tex_map[r["tkey"]].add(b[2])
        rep.stats["textured_tris"] += 1
    keep = B_LIST_MASK | B_FILT | B_CULLB | B_CULLF | B_TEXGEN | B_ENVLERP \
        | B_ZALWAYS
    if (b[5] & keep) != (r["bucket"] & keep):
        rep.fail("bucket", "%s: pack bucket 0x%X, ROM 0x%X (list/filt/"
                 "cull/texgen/env/zalways)"
                 % (where, b[5] & keep, r["bucket"] & keep))
    if (b[5] & B_TEXLERP) != (r["bucket"] & B_TEXLERP):
        rep.fail("texlerp_bit", "%s: pack TEXLERP %d, the ROM combiner is %s"
                 % (where, bool(b[5] & B_TEXLERP),
                    "lerp(TEXEL0, TEXEL1, PRIM_LOD_FRAC)"
                    if r["bucket"] & B_TEXLERP else "not that"))
    if r["two"]:
        rep.stats["two_cycle_tris"] += 1
        return
    asrc = b[6] & 0xFF
    if b[3] != r["shaded"]:
        rep.fail("shaded", "%s: pack %d, ROM %d" % (where, b[3], r["shaded"]))
    if asrc != r["asrc"]:
        rep.fail("alpha_src", "%s: pack %d, ROM %d" % (where, asrc,
                                                       r["asrc"]))
    if (b[7], b[8], b[9]) != (r["prim"], r["l1"], r["l2"]):
        rep.fail("colours", "%s: pack prim/l1/l2 %08X/%08X/%08X, ROM "
                 "%08X/%08X/%08X" % (where, b[7], b[8], b[9], r["prim"],
                                     r["l1"], r["l2"]))
    if b[10] != r["env"]:
        rep.fail("env", "%s: pack env %06X, ROM %06X" % (where, b[10],
                                                         r["env"]))


# -- drivers -------------------------------------------------------------------

def run_gfxdis(rep, run):
    vtx, tri, tex = gfxdis_events(run.flat)
    for name, mine, theirs in (
            ("vertex loads", [(a, b, c) for a, b, c in run.ev_vtx], vtx),
            ("triangles", run.ev_tri, tri)):
        if name == "vertex loads":
            mine = [(a, b) for a, b, _c in mine]
            theirs = [(a, b) for a, b, _c in theirs]
        if mine != theirs:
            rep.fail("gfxdis", "%s: interpreter %d, gfxdis %d, first "
                     "difference at %s" % (
                         name, len(mine), len(theirs),
                         next((i for i, (x, y) in enumerate(zip(mine, theirs))
                               if x != y), min(len(mine), len(theirs)))))
    # gsSPTexture: scale pair and on flag
    def q16(x):
        m = re.match(r"qu016\(([-0-9.e]+)\)", str(x))
        return int(round(float(m.group(1)) * 65536)) if m else x
    mt = [(s, t, on) for s, t, _l, _tl, on in run.ev_tex]
    gt = [(q16(x[0]), q16(x[1]), 1 if x[4] in ("G_ON", 1) else 0)
          for x in tex if len(x) >= 5]
    if mt != gt:
        rep.fail("gfxdis", "gsSPTexture: interpreter %d, gfxdis %d"
                 % (len(mt), len(gt)))
    for a in run.mismatch_tok:
        rep.fail("tokrule", "vertex pointer at 0x%X: relocation -> 0x%X, "
                 "token rule -> 0x%X" % a)
    rep.stats["gfxdis_cmds"] += len(run.ev_vtx) + len(run.ev_tri)


def tree_vs_capture(rep, nodes, rec):
    """The hand-decoded tree against what the exporter's read_dobj_tree gave."""
    for n, c in zip(nodes, rec["tree"][5]):
        if (n.jid, n.T, n.R, n.S) != (c.joint_id, c.translate, c.rotate,
                                      c.scale):
            rep.fail("tree", "node %d differs from read_dobj_tree" % n.idx)
        if (n.dl if not n.links else None) != c.dl_off and not n.links:
            rep.fail("tree", "node %d display list 0x%X vs exporter 0x%X"
                     % (n.idx, n.dl or 0, c.dl_off or 0))
    if len(nodes) != len(rec["tree"][5]):
        rep.fail("tree", "%d nodes vs exporter %d" % (len(nodes),
                                                       len(rec["tree"][5])))


def nodes_from_root(root):
    """Node objects from an exporter-built DObjNode tree (a pack whose tree
    is made, not read: an effect list, a graft, a single-DL map object)."""
    out = []

    def conv(c, parent):
        n = Node()
        n.idx, n.jid, n.parent, n.children = c.index, c.joint_id, parent, []
        n.dl, n.pre = c.dl_off, c.dl_pre_off
        n.links = list(c.dl_links)
        n.T, n.R, n.S = c.translate, c.rotate, c.scale
        if parent is not None:
            parent.children.append(n)
        out.append(n)
        for k in c.children:
            conv(k, n)
    for c in root.children:
        conv(c, None)
    out.sort(key=lambda n: n.idx)
    return out


def ref_for(rep, rec):
    """Build the reference run for one captured bake. Returns None when
    the baker's seed cannot be modelled (noted in the report)."""
    seed = dict(rec["seed"])
    omh = seed.pop("othermode_h", None)
    z = None
    if seed.pop("ztrack", False):
        z = {"zbuffer": seed.pop("zbuffer", True),
             "zcmp": seed.pop("zcmp", [True] * 4)}
    if seed:
        rep.stats["skipped_seeded"] += 1
        rep.fail("seed", "baker seeded with %s: not modelled" % sorted(seed))
        return None
    ti = rec["tree"]
    if ti is not None:
        f, reloc, off, dls_pair, dl_links, _n = ti
        src = Src(f, reloc)
        nodes = raw_tree(src, off, dls_pair, dl_links)
        tree_vs_capture(rep, nodes, {"tree": ti})
    else:
        src = Src(rec["f"])
        nodes = nodes_from_root(rec["root"])
        rep.stats["made_trees"] += 1
    run = RefRun(rec["rendermode"], None, omh, z)
    walk(run, src, nodes, rec["subs"], rec["skip"], rec["parts"],
         rec["skeleton"])
    return src, nodes, run


def fighter_case(rom, name):
    import ssb_packexport as P
    cap = Capture()
    built = []
    orig_bf = M.build_fighter

    def bf(*a, **k):
        r = orig_bf(*a, **k)
        built.append(r)
        return r
    M.build_fighter = bf
    try:
        with cap, contextlib.redirect_stdout(io.StringIO()):
            blob, _anm = P.pack_fighter(rom, name, "high")
    finally:
        M.build_fighter = orig_bf
    return blob, cap.for_baker(built[0]["baker"])


def check_fighter(rom, name):
    rep = Report(name)
    blob, rec = fighter_case(rom, name)
    if rec is None or rec["tree"] is None:
        rep.fail("capture", "no bake record")
        return rep
    n, t = check_one(rep, blob, rec)
    p = read_model(blob)
    if not rep.fails and (n, t) != (p["njoint"], p["ntri"]):
        rep.fail("totals", "pack has %d joints/%d tris, reference %d/%d"
                 % (p["njoint"], p["ntri"], n, t))
    return rep


def selftest(rom):
    """Mutate a Mario pack one field at a time and demand that every
    mutation is caught: the check must fail where the pack is wrong."""
    blob, rec = fighter_case(rom, "Mario")
    hd = struct.unpack_from("<8s8I8I3ff8s8I", blob, 0)
    o_j, o_v, o_t, o_b = hd[9:13]
    p = read_model(blob)
    tb = next(i for i, b in enumerate(p["batches"]) if b[2] >= 0)   # textured
    bi_t = p["batches"][tb]
    v_t = p["tris"][bi_t[0]][0]
    muts = []

    def m(name, off, fmt, fn):
        muts.append((name, off, fmt, fn))
    m("vertex x", o_v + 36 * 5, "<f", lambda x: x + 1.0)
    m("vertex y", o_v + 36 * 5 + 4, "<f", lambda x: x - 2.0)
    m("normal x", o_v + 36 * 5 + 12, "<f", lambda x: -x - 0.1)
    m("uv u", o_v + 36 * v_t + 24, "<f", lambda x: x + 0.05)
    m("uv v", o_v + 36 * v_t + 28, "<f", lambda x: x + 0.05)
    m("alpha", o_v + 36 * 5 + 32, "<B", lambda x: x ^ 0x40)
    m("owner joint", o_v + 36 * 5 + 33, "<B", lambda x: (x + 1) % 20)
    m("tri index", o_t + 6 * 3, "<H", lambda x: (x + 1) % p["nvert"])
    m("joint translate", o_j + 40 * 3 + 4, "<f", lambda x: x + 0.5)
    m("joint parent", o_j + 40 * 3, "<i", lambda x: x + 1)
    bo = o_b + 28 * tb
    m("batch tex", bo + 4, "<h", lambda x: -1)
    m("batch shaded", bo + 6, "<B", lambda x: (x + 1) % 3)
    m("batch list bits", bo + 8, "<H", lambda x: x ^ 2)
    m("batch cull bit", bo + 8, "<H", lambda x: x ^ 64)
    m("batch alpha_src", bo + 10, "<B", lambda x: x ^ 1)
    m("batch prim", bo + 12, "<I", lambda x: x ^ 0x00FF0000)
    m("batch light1", bo + 16, "<I", lambda x: x ^ 0x0000FF00)
    m("batch light2", bo + 20, "<I", lambda x: x ^ 0x0000FF00)
    # the texel check: one word of the batch's texels, and its palette
    tx = p["texs"][bi_t[2]]
    m("texels", hd[15] + tx[0] + (tx[1] // 2 & ~1), "<H",
      lambda x: x ^ 0x7777)
    if tx[6] == TS.FPACK_TEX_PAL4:
        m("palette", hd[14] + 32 * tx[4] + 2, "<H", lambda x: x ^ 0x7C1F)
    bad = 0
    for name, off, fmt, fn in muts:
        b = bytearray(blob)
        (v,) = struct.unpack_from(fmt, b, off)
        struct.pack_into(fmt, b, off, fn(v))
        rep = Report("mut")
        try:
            check_one(rep, bytes(b), rec)
        except Exception as e:                   # noqa: BLE001
            rep.fail("crash", repr(e))
        print("  mutation %-16s %s" % (name, "caught" if rep.fails else
                                       "MISSED"))
        bad += not rep.fails
    print("selftest: %d of %d mutations caught" % (len(muts) - bad, len(muts)))
    return 1 if bad else 0


def check_one(rep, blob, rec, joint_base=0):
    """One pack (or one layer of a merged pack) against its captured bake."""
    try:
        got = ref_for(rep, rec)
        if got is None:
            return 0, 0
        src, nodes, run = got
    except (RefError, ValueError, KeyError, IndexError) as e:
        rep.fail("interp", "reference interpreter: %r" % (e,))
        return 0, 0
    run_gfxdis(rep, run)
    compare_pack(rep, blob, run, nodes, joint_base=joint_base)
    rep.stats["ref_tris"] += run.ntri
    return len(nodes), run.ntri


def first_per_root(recs):
    seen, out = set(), []
    for r in sorted(recs, key=lambda r: r["seq"]):
        if id(r["root"]) not in seen:
            seen.add(id(r["root"]))
            out.append(r)
    return out


STAGES = ("Castle Jungle Zebes Hyrule Sector Yoster Pupupu Yamabuki Inishie "
          "Last Zako Metal YosterSmall Bonus3").split() + \
    ["Bonus1" + n for n in "Mario Fox Donkey Samus Luigi Link Yoshi Captain "
                           "Kirby Pikachu Purin Ness".split()] + \
    ["Bonus2" + n for n in "Mario Fox Donkey Samus Luigi Link Yoshi Captain "
                           "Kirby Pikachu Purin Ness".split()]


def check_stage(rom, arg):
    """One .stg (or the bonus platforms file): the merged layer pack layer
    by layer, and every object pack the exporter writes (map objects,
    grafts, ground actors, boss wallpaper, bonus platforms)."""
    rep = Report("stage:" + arg)
    import ssb_stageexport as S
    cap = Capture()
    entries, b2p = [], {}
    o_b2p, o_op = S.baker_to_pack, S.object_pack

    def b2p_w(baker):
        r = o_b2p(baker)
        b2p[id(r[1])] = baker
        return r

    def op_w(*a, **k):
        blob = o_op(*a, **k)
        name = a[6] if len(a) > 6 else k.get("name")
        entries.append((name, blob, b2p.get(id(a[1]))))
        return blob
    S.baker_to_pack, S.object_pack = b2p_w, op_w
    undo = cap.wrap(S, "merge_layers", "layers")
    out = os.path.join(tempfile.mkdtemp(), "x.out")
    argv, sys.argv = sys.argv, ["ssb_stageexport.py", "--rom", "-"] + \
        (["--platforms"] if arg == "--platforms" else ["--stage", arg]) + \
        ["--out", out]
    sys.argv[2] = ROM_PATH
    try:
        with cap, contextlib.redirect_stdout(io.StringIO()):
            S.main()
    finally:
        sys.argv = argv
        S.baker_to_pack, S.object_pack = o_b2p, o_op
        undo()
    data = open(out, "rb").read()
    os.unlink(out)
    if arg != "--platforms":
        plen = struct.unpack_from("<I", data, 8)[0]
        pack = data[16:16 + plen]
        recs = first_per_root([r for r in cap.bakes if r["ctx"] == ("layers",)])
        base, ntri = 0, 0
        for r in recs:
            n, t = check_one(rep, pack, r, joint_base=base)
            base += len(r["nodes"])
            ntri += t
            rep.stats["layers"] += 1
        p = read_model(pack)
        if base != p["njoint"] or ntri != p["ntri"]:
            rep.fail("layers", "pack has %d joints/%d tris, the layers' "
                     "references %d/%d" % (p["njoint"], p["ntri"], base, ntri))
    for name, blob, baker in entries:
        rec = cap.for_baker(baker) if baker is not None else None
        if rec is None:
            rep.fail("capture", "object pack %r has no bake record" % name)
            continue
        sub = Report("%s/%s" % (arg, name))
        sub.known_key = rep.label
        n, t = check_one(sub, blob, rec)
        p = read_model(blob)
        if (n, t) != (p["njoint"], p["ntri"]) and not sub.fails:
            sub.fail("objects", "pack has %d joints/%d tris, reference "
                     "%d/%d" % (p["njoint"], p["ntri"], n, t))
        rep.fails += sub.fails
        rep.notes.update(sub.notes)
        rep.known.update(sub.known)
        rep.cat.update(sub.cat)
        rep.stats.update(sub.stats)
        rep.stats["object_packs"] += 1
    return rep


ITEM_MODES = ["all", "weapons", "stage-items", "alts", "effect:boxsmash",
              "effect:tarubombsmash"]


def check_items(rom, arg):
    """ssb_itemmodelexport's .mdl packs: the 34 items, the monster weapons,
    the stage items and weapons, the fighter items, the swapped-in display
    lists and the two item effects."""
    rep = Report("items:" + arg)
    import ssb_itemmodelexport as I
    cap = Capture()
    entries = []
    undo = []

    def hook(name):
        orig = getattr(I, name)

        def w(*a, **k):
            start = len(cap.bakes)
            r = orig(*a, **k)
            if isinstance(r, (bytes, bytearray)):
                entries.append(("%s:%s" % (name[5:], a[1]), bytes(r),
                                cap.bakes[start:]))
            return r
        setattr(I, name, w)
        undo.append(lambda: setattr(I, name, orig))
    for n in ("pack_item", "pack_alt", "pack_effect", "pack_fighter_item"):
        hook(n)
    outdir = tempfile.mkdtemp()
    if arg.startswith("effect:"):
        args = ["--effect", arg[7:], "--out", os.path.join(outdir, "e.mdl")]
    elif arg == "all":
        args = ["--all", "--outdir", outdir]
    else:
        args = ["--" + arg, "--outdir", outdir]
    argv, sys.argv = sys.argv, ["ssb_itemmodelexport.py", "--rom",
                                ROM_PATH] + args
    try:
        with cap, contextlib.redirect_stdout(io.StringIO()):
            try:
                I.main()
            except SystemExit as e:
                if e.code not in (None, 0):
                    rep.fail("export", "exporter exited: %s" % (e.code,))
    finally:
        sys.argv = argv
        for u in undo:
            u()
    for label, blob, recs in entries:
        recs = first_per_root(recs)
        if not recs:
            rep.fail("capture", "%s: no bake record" % label)
            continue
        sub = Report(label)
        sub.known_key = rep.label
        n, t = check_one(sub, blob, recs[0])
        p = read_model(blob)
        if not sub.fails and (n, t) != (p["njoint"], p["ntri"]):
            sub.fail("objects", "pack has %d joints/%d tris, reference "
                     "%d/%d" % (p["njoint"], p["ntri"], n, t))
        rep.fails += sub.fails
        rep.notes.update(sub.notes)
        rep.known.update(sub.known)
        rep.cat.update(sub.cat)
        rep.stats.update(sub.stats)
        rep.stats["item_packs"] += 1
    if not entries:
        rep.fail("export", "no packs produced")
    return rep


def fighter_names():
    stems = ("mario fox donkey samus luigi link yoshi captain kirby pikachu "
             "purin ness gdonkey mmario nmario nfox ndonkey nsamus nluigi "
             "nlink nyoshi ncaptain nkirby npikachu npurin nness boss").split()
    special = {"gdonkey": "GDonkey", "mmario": "MMario"}
    out = []
    for s in stems:
        if s in special:
            out.append(special[s])
        elif s.startswith("n") and s != "ness":
            out.append("N" + s[1].upper() + s[2:])
        else:
            out.append(s[0].upper() + s[1:])
    return out


def jobs_table():
    jobs = []
    for n in fighter_names():
        jobs.append(("fighter:" + n, check_fighter, n))
    for n in STAGES + ["--platforms"]:
        jobs.append(("stage:" + n, check_stage, n))
    for n in ITEM_MODES:
        jobs.append(("items:" + n, check_items, n))
    return jobs


def run_job(args):
    global ROM_PATH
    rom_path, label, fn_name, arg = args
    ROM_PATH = rom_path
    rom = open(rom_path, "rb").read()
    fn = globals()[fn_name]
    try:
        rep = fn(rom, arg)
    except Exception as e:                   # noqa: BLE001
        import traceback
        rep = Report(label)
        rep.fail("crash", "%r\n%s" % (e, traceback.format_exc()))
    notes = dict(rep.notes)
    for (lab, cat), n in rep.known.items():
        notes["KNOWN %s %s (%d tris): %s" % (lab, cat, n,
                                                     KNOWN[(lab, cat)])] = 0
    return label, rep.fails, dict(rep.stats), notes


def main():
    global ROM_PATH
    argv = sys.argv[1:]
    rom_path = SP.ROM_DEFAULT if hasattr(SP, "ROM_DEFAULT") else \
        ssb_extract.ROM_DEFAULT
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    only = None
    if "--only" in argv:
        only = argv[argv.index("--only") + 1].split(",")
    jobs_n = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else \
        min(8, os.cpu_count() or 2)
    table = jobs_table()
    if "--selftest" in argv:
        ROM_PATH = rom_path
        return selftest(open(rom_path, "rb").read())
    if "--list" in argv:
        for label, _fn, _a in table:
            print(label)
        return 0
    if not os.path.exists(rom_path):
        print("model_pack_check: no baserom at %s -- skipping" % rom_path)
        return 0
    if not os.path.exists(SP.GFXDIS):
        print("missing %s -- run in the container: scripts/ctr.sh run ..."
              % SP.GFXDIS)
        return 1
    if only:
        table = [t for t in table
                 if any(t[0] == o or t[0].startswith(o) or o in t[0]
                        for o in only)]
    work = [(rom_path, l, f.__name__, a) for l, f, a in table]
    failed = False
    tot = collections.Counter()
    allnotes = collections.Counter()
    with concurrent.futures.ProcessPoolExecutor(jobs_n) as ex:
        for label, fails, stats, notes in ex.map(run_job, work):
            tot.update(stats)
            allnotes.update(notes)
            line = " ".join("%s=%d" % kv for kv in sorted(stats.items())
                            if kv[0] in ("tris", "units", "joints"))
            print("%-22s %s  %s" % (label, "FAIL" if fails else "ok", line))
            for m in fails:
                print("    " + m.replace("\n", "\n    "))
            failed = failed or bool(fails)
    print("\nchecked %s" % ", ".join("%d %s" % (v, k)
                                     for k, v in sorted(tot.items())))
    for k, v in sorted(allnotes.items()):
        print(("%s" % k) if k.startswith("KNOWN") else
              "note: %d x %s" % (v, k))
    if not only and "--no-selftest" not in argv:
        ROM_PATH = rom_path
        failed = bool(selftest(open(rom_path, "rb").read())) or failed
    print("model_pack_check: %s" % ("FAIL" if failed else "ok"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
