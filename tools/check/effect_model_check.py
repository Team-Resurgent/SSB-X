#!/usr/bin/env python3
"""ssb64-dc: every effect/weapon model the exporter ships, against the ROM.

tools/export/ssb_effectexport.py writes ~115 .mdl packs (halo, quake, orbs,
the damage effects, the entry vehicles, the weapons, the opening scenes'
models). slash_check / spark_check / mdust_check / dexp_check each take one
or two of them apart; pack_anim_check replays the animation script words.
Nothing checked the rest of what a pack says: the DObjDesc tree it carries,
which MObj belongs to which joint and batch, the MObjSub fields, which
texture a batch or a sprite-array frame names, and which script each joint
and MObj got. A pack whose tree is one joint out of order, whose MObj table
is shifted by a hole, or whose frame 3 is frame 2 twice, draws plausibly
and wrongly. This is the table-driven oracle for all of it.

How it works. SPECS below is a table with one row per exported pack: the
ROM symbols (or WPAttributes extern chain) its tree, MObjSub table,
MatAnimJoint tables and AnimJoint tables sit at, and the handful of facts
about the game's maker that are code rather than data (is o_dobjsetup a
DObjDesc tree, one display list, a DObjDLLink array; does the maker build
a stand DObj by hand; which MObj/MatAnimJoint table is indexed from the
child). The rows for the entry vehicles and weapons are generated from the
exporter's own symbol tables (the symbol NAMES are the shared input; what
they decode to is not). Each pack is then produced by the real exporter
function and read back through fighter.h's FPack* layouts, and:

  tree      the DObjDesc array is decoded by hand (id, parent via the
            array_dobjs[id-1] rule, translate/rotate/scale as f32, the
            DObjDLLink arrays) and compared joint for joint with the pack's
            joint table, and with ssb_assets.read_dobj_tree.
  geometry  the tree is baked again, here, with ssb_assets.MeshBaker from
            the ROM data (one bake per sprite-array frame / palette), and
            the pack's vertices, triangles, batch ranges, batch joints,
            PVR list bits, material colours, texture indices, the joint
            and batch to MObj maps, bounds centre and radius must be the
            same numbers.
  textures  every batch's picture, and every frame of every MObj's sprite
            array / palette run, is compared byte for byte (pack_texture's
            own bytes, size, format, clamp) with a fresh bake from the ROM;
            palette banks likewise; no pack texture may be one the ROM
            does not produce. Dynamic palettes (Run, Clash) are compared
            with the ROM's raw TLUTs.
  MObjSub   flags, prim_l, the five colours against the ROM struct; the
            frame count against what the MatAnimJoint script can reach;
            the uv fields zero where nothing scrolls.
  scripts   which AnimJoint script each joint got and which MatAnimJoint
            script each MObj got (per alternate), by event-stream decode +
            replay (pack_anim_check's decoder), and -1 exactly where the
            ROM table has no script.
  sanity    section bounds, index ranges, header counts.

Policy that the exporter chooses and this takes from its tables rather than
deriving (a mismatch here would be a wrong table, not wrong decoding): the
render-mode seed an effect/weapon bakes under (EF_SEED/WP_SEED), whether a
pack is baked paletted (EFENTRY_PALETTED), the FPACK_NOZ bit of the Yoshi
egg, which entry vehicles are flat display lists. Not covered: the pixels
of a texture beyond "equal to a fresh MeshBaker bake" (flipbook_check and
texfmt_check cover the texel decode itself).

Usage: python3 tools/check/effect_model_check.py [--rom <rom.z64>]
           [--only name[,name...]] [--jobs N] [--list]
"""
import concurrent.futures
import contextlib
import io
import math
import os
import re
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_extract               # noqa: E402
import ssb_assets as A           # noqa: E402
import ssb_meshexport as M       # noqa: E402
import ssb_effectexport as EE    # noqa: E402
import pack_anim_check as PA     # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
RELOC_HEADER = os.path.join(ROOT, "src", "dc", "decomp", "reloc_data.us.h")

ZERO, ONE = (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)
XLU = [A.G_RM_AA_ZB_XLU_SURF, None, None, None]
NOZ_ENV_TL = A.FPACK_NOZ | A.FPACK_ENVLERP | A.FPACK_TEXLERP


# -- the table --------------------------------------------------------------

def sy(fsym, sym):
    """A (file, offset) named by two reloc-header symbols."""
    return ("sym", fsym, sym)


def raw(fsym, off):
    return ("raw", fsym, off)


def ex(fsym, sym, delta):
    """The word at WPAttributes `sym` + delta, resolved through the file's
    EXTERN relocation chain."""
    return ("ext", fsym, sym, delta)


class Spec(object):
    """One exported pack. See the module doc for what each field means.

    tree   ('desc', ref, links) | ('flat', ref) | ('linkarr', ref) |
           ('stand', ref, head, is_array) | ('single', ref, head) |
           ('wrap', ref) | ('none',)
    mobj   ref of the MObjSub table, mats a list of refs (alternates);
    shift  hand-built leading nodes the MObj/MatAnim/AnimJoint tables are
           not indexed from;
    anims  [(name, ref, table_len, patch{joint: ref})] AnimJoint blocks;
    anim_direct  the ref is one script, not a table;
    """

    def __init__(self, name, maker, tree, mobj=None, mats=(), anims=(),
                 shift=0, seed=None, paletted=False, palframes=0,
                 nframes=None, codeframes=False, holes=None, hole_mode=None,
                 multi=False, bucket_or=0, njoints=None, pack_name=None,
                 anim_direct=False, anim_ref_file=None, shared_runs=False):
        self.name, self.maker, self.tree = name, maker, tree
        self.mobj, self.mats, self.anims = mobj, list(mats), list(anims)
        self.shift, self.seed, self.paletted = shift, seed, paletted
        self.palframes, self.nframes = palframes, nframes
        self.codeframes, self.holes, self.hole_mode = codeframes, holes, \
            hole_mode
        self.multi, self.bucket_or, self.njoints = multi, bucket_or, njoints
        self.pack_name, self.anim_direct = pack_name, anim_direct
        self.shared_runs = shared_runs


def build_specs():
    S = []

    def add(*a, **k):
        S.append(Spec(*a, **k))

    E1 = "llEFCommonEffects1FileID"
    E2 = "llEFCommonEffects2FileID"
    E3 = "llEFCommonEffects3FileID"

    add("halo", EE.pack_halo,
        ("desc", sy(E3, "llEFCommonEffects3RebirthHaloDObjDesc"), True),
        anims=[("EFHalo", sy(E3, "llEFCommonEffects3RebirthHaloAnimJoint"),
                3, None)], njoints=3, pack_name="EFHalo")
    add("quake", EE.pack_quake, ("none",),
        anims=[("Mag%d" % k, sy(E1, EE.QUAKE_ANIMS[k]), 1, None)
               for k in range(4)], njoints=1, pack_name="EFQuake")
    add("orbs", EE.pack_orbs,
        ("stand", sy(E1, "llEFCommonEffects1FlyOrbsDObjDesc"), 1, False),
        anims=[("EFOrbs", sy(E1, "llEFCommonEffects1FlyOrbsAnimJoint"),
                1, None)], shift=1, njoints=2, pack_name="EFOrbs")
    add("slash", EE.pack_slash,
        ("desc", sy(E1, "llEFCommonEffects1DamageSlashDObjDesc"), True),
        mobj=sy(E1, "llEFCommonEffects1DamageSlashMObjSub"),
        mats=[sy(E1, "llEFCommonEffects1DamageSlashMatAnimJoint")],
        anims=[("EFSlash", sy(E1, "llEFCommonEffects1DamageSlashAnimJoint"),
                3, None)], njoints=3, pack_name="EFSlash")
    for what, nm, pre, links in (("spark", "EFSpark", "CommonSpark", False),
                                 ("mdust", "EFMDust", "DamageFlyMDust",
                                  True)):
        p = "llEFCommonEffects1" + pre
        add(what, getattr(EE, "pack_" + what),
            ("stand", sy(E1, p + "DObjDesc"), 1, links),
            mobj=sy(E1, p + "MObjSub"), mats=[sy(E1, p + "MatAnimJoint")],
            anims=[(nm, sy(E1, p + "AnimJoint"), 1, None)], shift=1,
            njoints=2, pack_name=nm)
    add("deadexplode", EE.pack_deadexplode,
        # the exporter's reloc list names these two the wrong way round
        # (dexp_check.py proves it); the DObjDesc is at the `MObjSub` name
        ("desc", sy(E2, "llEFCommonEffects2DeadExplodeDefaultMObjSub"), True),
        mobj=sy(E2, "llEFCommonEffects2DeadExplodeDefaultDObjDesc"),
        mats=[sy(E2, "llEFCommonEffects2DeadExplode%dMatAnimJoint" % k)
              for k in (1, 2, 3, 4)],
        anims=[("EFDExp", sy(E2, "llEFCommonEffects2DeadExplodeDefaultAnimJoint"),
                4, None)], njoints=4, pack_name="EFDExp")
    add("shield", EE.pack_shield,
        ("desc", sy("llFTManagerCommonFileID",
                    "llFTManagerCommonShieldDObjDesc"), True),
        njoints=2, pack_name="EFShield")
    add("yoshiegg", EE.pack_yoshiegg,
        ("single", sy("llYoshiModelFileID", "llYoshiModelShieldDObjDesc"),
         EE.YEGG_HEAD), bucket_or=A.FPACK_NOZ, njoints=1,
        pack_name="EFYEgg")
    add("impactwave", EE.pack_impactwave,
        ("flat", sy(E1, "llEFCommonEffects1ImpactWaveDObjDesc")),
        mobj=sy(E1, "llEFCommonEffects1ImpactWaveMObjSub"),
        mats=[sy(E1, "llEFCommonEffects1ImpactWaveMatAnimJoint")],
        anims=[("EFImpact", sy(E1, "llEFCommonEffects1ImpactWaveAnimJoint"),
                1, None)], seed=XLU, njoints=1, pack_name="EFImpact")
    # Mario's/Luigi's fireball: the exporter hard-codes Special3 0x1A8/0xD8;
    # here they come from the WPAttributes' own extern chain (which lands on
    # exactly those two places in file 297), so a wrong constant fails.
    fb = ("llMarioSpecial1FileID", "llMarioSpecial1FireballWeaponAttributes")
    add("fireball", EE.pack_fireball, ("flat", ex(fb[0], fb[1], 0)),
        mobj=ex(fb[0], fb[1], 4), palframes=2, njoints=1,
        pack_name="WPFireba")
    add("blaster", EE.pack_blaster,
        ("flat", sy("llFoxSpecial4FileID",
                    "llFoxSpecial4ReflectorDLDisplayList")),
        njoints=1, pack_name="WPBlast")
    add("arwinglaser", EE.pack_arwinglaser,
        ("flat", ex("llGRSectorMapFileID",
                    "llGRSectorMapArwingLaser2DWeaponAttributes", 0)),
        njoints=1, pack_name="WPArLsr")
    add("shock", EE.pack_shocksmall,
        ("single", sy(E2, "llEFCommonEffects2ShockSmallDObjDesc"),
         EE.SHOCKSMALL_HEAD),
        mobj=sy(E2, "llEFCommonEffects2ShockSmallMObjSub"),
        mats=[sy(E2, "llEFCommonEffects2ShockSmallMatAnimJoint")],
        njoints=1, pack_name="EFShock")
    add("firespark", EE.pack_firespark,
        ("desc", sy(E2, "llEFCommonEffects2FireSparkDObjDesc"), False),
        mobj=sy(E2, "llEFCommonEffects2FireSparkMObjSub"),
        mats=[sy(E2, "llEFCommonEffects2FireSparkMatAnimJoint")],
        anims=[("EFFireSpk", sy(E2, "llEFCommonEffects2FireSparkAnimJoint"),
                2, None)], seed=EE.EF_SEED, njoints=2,
        pack_name="EFFireSpk")

    # weapons: WPAttributes {data, p_mobjsubs, anim_joints, p_matanim_joints}
    for which, (_w, afile, asym, pname) in sorted(EE.WPQUAD.items()):
        npal = EE.WPQUAD_MOBJ.get(which, 0)
        add("wp:" + which, lambda rom, w=which: EE.pack_wpquad(rom, w),
            ("flat", ex(afile, asym, 0)),
            mobj=ex(afile, asym, 4) if npal else None, palframes=npal,
            seed=EE.WP_SEED, njoints=1, pack_name=pname)
    for which, row in sorted(EE.WPTREE.items()):
        _w, afile, asym, pname, nj, links = row
        add("wp:" + which, lambda rom, w=which: EE.pack_wptree(rom, w),
            ("desc", ex(afile, asym, 0), links), mobj=("optext", afile, asym, 4),
            mats=[("optext", afile, asym, 12)],
            anims=[(pname, ("optext", afile, asym, 8), nj, None)],
            seed=EE.WP_SEED, njoints=nj, pack_name=pname, holes=0,
            hole_mode="zero")
    for which, (_w, afile, asym, pname, nsp) in sorted(EE.WPLINK.items()):
        add("wp:" + which, lambda rom, w=which: EE.pack_wplink(rom, w),
            ("linkarr", ex(afile, asym, 0)), mobj=ex(afile, asym, 4),
            nframes=nsp, seed=EE.WP_SEED, njoints=1, pack_name=pname)
    for which, (_w, afile, asym, pname, nm) in sorted(EE.WPFLAT.items()):
        add("wp:" + which, lambda rom, w=which: EE.pack_wpflat(rom, w),
            ("flat", ex(afile, asym, 0)), mobj=ex(afile, asym, 4),
            mats=[ex(afile, asym, 12)], seed=EE.WP_SEED, njoints=1,
            pack_name=pname)

    # entry vehicles and the scene models
    for which in sorted(EE.EFENTRY):
        who, fsym, dsym, asyms, pname, nj, links = EE.EFENTRY[which]
        asyms = (asyms,) if isinstance(asyms, str) else tuple(asyms)
        if which in EE.EFENTRY_FLATDL:
            tree = ("wrap" if which in EE.EFENTRY_WRAPPED else "flat",
                    sy(fsym, dsym))
        elif which in EE.EFENTRY_LINKDL:
            tree = ("linkarr", sy(fsym, dsym))
        else:
            tree = ("desc", sy(fsym, dsym), links)
        mobj, mats = None, []
        if which in EE.EFENTRY_MOBJ:
            row = EE.EFENTRY_MOBJ[which]
            mobj = sy(fsym, row[7])
            ms = row[8] if isinstance(row[8], tuple) else (row[8],)
            mats = [sy(fsym, m) for m in ms]
        afs = EE.EFENTRY_ANIMFILE.get(which, fsym)
        anims = []
        patch = None
        for k, a in enumerate(asyms):
            if which in EE.EFENTRY_ANIMPATCH and k == 0:
                patch = {j: sy(fsym, s) for j, s in
                         EE.EFENTRY_ANIMPATCH[which].items()}
            anims.append(("%s%d" % (pname[:7], k) if len(asyms) > 1
                          else pname, sy(afs, a), nj, patch))
        add("ef:" + which, lambda rom, w=which: EE.pack_efentry(rom, w),
            tree, mobj=mobj, mats=mats, anims=anims,
            seed=EE.EF_PROC_RENDERMODE.get(which),
            paletted=which in EE.EFENTRY_PALETTED,
            holes=EE.EFENTRY_HOLES.get(which), hole_mode="prev",
            codeframes=which in EE.EFENTRY_CODEFRAMES,
            multi=which in EE.EFENTRY_MULTI, njoints=nj, pack_name=pname,
            anim_direct=which in EE.EFENTRY_DIRECTANIM,
            shared_runs=which in EE.EFENTRY_HOLES)
    return S


# -- the ROM side -----------------------------------------------------------

SYMS = {}


def load_syms():
    src = open(RELOC_HEADER).read()
    for m in re.finditer(r"extern int (ll\w+); // (0x[0-9a-fA-F]+)", src):
        SYMS[m.group(1)] = int(m.group(2), 16)


class Files(object):
    """The ROM's relocData files, parsed once each."""

    def __init__(self, rom):
        self.rom = rom
        self.c = {}

    def get(self, fid):
        if fid not in self.c:
            f = A.get_file(self.rom, fid, ssb_extract)
            ent = ssb_extract.read_entry(self.rom, ssb_extract.RELOC_SEG, fid)
            reloc = A.walk_reloc(f, ent["reloc_intern"])
            ext = A.walk_reloc_extern(self.rom, ssb_extract, fid, f)
            self.c[fid] = (f, reloc, ext)
        return self.c[fid]

    def resolve(self, ref):
        """(fid, off), or None for an optional ref whose word is NULL."""
        kind = ref[0]
        if kind == "sym":
            return SYMS[ref[1]], SYMS[ref[2]]
        if kind == "raw":
            return SYMS[ref[1]], ref[2]
        afid = SYMS[ref[1]]
        f, reloc, ext = self.get(afid)
        site = SYMS[ref[2]] + ref[3]
        if kind == "optext" and struct.unpack_from(">I", f, site)[0] == 0:
            return None
        if site not in ext:
            raise ValueError("WPAttributes word at file %d 0x%X is not an "
                             "extern relocation" % (afid, site))
        return ext[site]


def u32(f, o):
    return struct.unpack_from(">I", f, o)[0]


def f32s(f, o, n=3):
    return struct.unpack_from(">%df" % n, f, o)


def raw_tree(f, reloc, off, links):
    """DObjDesc array decoded by hand (lbCommonSetupTreeDObjs): a list of
    (joint_id, parent index or -1, T, R, S, [(list_id, dl)] or dl)."""
    out = []
    slots = [None] * A.DOBJ_ARRAY_MAX
    i = 0
    while True:
        b = off + i * A.DOBJDESC_SIZE
        jid = u32(f, b) & 0xFFF
        if jid == A.DOBJ_ARRAY_MAX:
            return out
        parent = -1 if jid == 0 else slots[jid - 1]
        if jid and parent is None:
            raise ValueError("DObjDesc[%d] id %d has no parent" % (i, jid))
        dl = reloc.get(b + 4)
        if links and dl is not None:
            arr, dl = dl, []
            k = 0
            while u32(f, arr + 8 * k) != 4:
                dl.append((struct.unpack_from(">i", f, arr + 8 * k)[0],
                           reloc.get(arr + 8 * k + 4)))
                k += 1
                if k > 16:
                    raise ValueError("unterminated DObjDLLink array")
        out.append((jid, parent, f32s(f, b + 8), f32s(f, b + 20),
                    f32s(f, b + 32), dl))
        slots[jid] = i
        i += 1


def raw_links(f, reloc, off):
    out, k = [], 0
    while u32(f, off + 8 * k) != 4:
        out.append((struct.unpack_from(">i", f, off + 8 * k)[0],
                    reloc.get(off + 8 * k + 4)))
        k += 1
        if k > 16:
            raise ValueError("unterminated DObjDLLink array")
    return out


class Tree(object):
    """The expected joints, plus a factory for the baker's own tree."""

    def __init__(self, joints, build, dls):
        self.joints = joints      # [(parent, T, R, S)] in draw order
        self.build = build        # () -> root DObjNode for MeshBaker
        self.dls = dls            # [dl list per joint] for the report


def make_tree(sp, files):
    kind = sp.tree[0]
    if kind == "none":
        return Tree([(-1, ZERO, ZERO, ONE)], None, [])
    fid, off = files.resolve(sp.tree[1])
    f, reloc, _ext = files.get(fid)

    def synth(spec):
        """spec: [(parent index, dl_off or None, [(head, dl)])]."""
        def build():
            root = A.DObjNode(-1, -1, None, ZERO, ZERO, ONE)
            nodes = []
            for i, (par, dl, links) in enumerate(spec):
                n = A.DObjNode(i, i, dl, ZERO, ZERO, ONE)
                n.dl_links = list(links)
                p = root if par < 0 else nodes[par]
                n.parent = p
                p.children.append(n)
                nodes.append(n)
            return root
        return Tree([(p, ZERO, ZERO, ONE) for p, _, _ in spec], build, spec)

    if kind == "desc":
        links = sp.tree[2]
        rt = raw_tree(f, reloc, off, links)

        def build():
            return A.read_dobj_tree(f, reloc, off, dl_links=links)[0]
        t = Tree([(p, T, R, Sc) for _j, p, T, R, Sc, _d in rt], build, rt)
        # the shared reader must agree with the hand decode
        _r, nodes = A.read_dobj_tree(f, reloc, off, dl_links=links)
        if len(nodes) != len(rt):
            raise ValueError("read_dobj_tree reads %d nodes, hand decode %d"
                             % (len(nodes), len(rt)))
        for n, r in zip(nodes, rt):
            if (n.joint_id, n.parent.index, n.translate, n.rotate,
                    n.scale) != (r[0], r[1], r[2], r[3], r[4]):
                raise ValueError("read_dobj_tree node %d differs from the "
                                 "hand decode" % n.index)
            want = r[5]
            have = n.dl_links if links else n.dl_off
            if links and want is not None and list(want) != list(have):
                raise ValueError("read_dobj_tree DL links differ at node %d"
                                 % n.index)
            if not links and want != have:
                raise ValueError("read_dobj_tree dl differs at node %d"
                                 % n.index)
        return t
    if kind == "flat":
        return synth([(-1, off, [])])
    if kind == "linkarr":
        return synth([(-1, None, raw_links(f, reloc, off))])
    if kind == "single":
        # root -> one node whose geometry is the one display list `off`
        # points at, queued into the given head
        return synth([(-1, None, [(sp.tree[2], off)])])
    if kind == "wrap":
        return synth([(-1, None, []), (0, off, [])])
    if kind == "stand":
        head, is_array = sp.tree[2], sp.tree[3]
        links = raw_links(f, reloc, off) if is_array else [(head, off)]
        if len(links) != 1:
            raise ValueError("stand's DObjDLLink array holds %d links"
                             % len(links))
        return synth([(-1, None, []), (0, None, links)])
    raise ValueError("tree kind " + kind)


# -- reading a pack back (fighter.h) ----------------------------------------

def read_pack(blob):
    hd = struct.unpack_from("<8s8I8I3ff8s8I", blob, 0)
    (njoint, nvert, ntri, nbatch, ntex, npal, nanim, texlen) = hd[1:9]
    offs = hd[9:17]
    p = {"hdr": hd, "blob": blob, "njoint": njoint, "nvert": nvert,
         "ntri": ntri, "nbatch": nbatch, "ntex": ntex, "npal": npal,
         "nanim": nanim, "texlen": texlen, "offs": offs,
         "center": hd[17:20], "radius": hd[20], "name": hd[21],
         "mobjs_off": hd[-1]}
    o_j, o_v, o_t, o_b, o_x, o_p, o_d, o_a = offs
    p["joints"] = [struct.unpack_from("<i9f", blob, o_j + i * 40)
                   for i in range(njoint)]
    p["verts"] = [struct.unpack_from("<8f2BH", blob, o_v + i * 36)
                  for i in range(nvert)]
    p["tris"] = [struct.unpack_from("<3H", blob, o_t + i * 6)
                 for i in range(ntri)]
    p["batches"] = [struct.unpack_from("<2Hh2B2H4I", blob, o_b + i * 28)
                    for i in range(nbatch)]
    p["texs"] = [struct.unpack_from("<2I2H4B", blob, o_x + i * 16)
                 for i in range(ntex)]
    p["pals"] = [struct.unpack_from("<16H", blob, o_p + i * 32)
                 for i in range(npal)]
    p["texdata"] = o_d
    anims = []
    for k in range(nanim):
        name, ow, cnt, oe, kind, orl, nrl = struct.unpack_from(
            "<%dsIIIIII" % EE.ANIM_NAME_LEN, blob,
            o_a + k * (EE.ANIM_NAME_LEN + 24))
        anims.append({
            "name": name.split(b"\0")[0].decode(),
            "words": list(struct.unpack_from("<%dI" % cnt, blob, ow)),
            "entries": struct.unpack_from("<%di" % njoint, blob, oe),
            "relocs": struct.unpack_from("<%dI" % nrl, blob, orl),
            "kind": kind, "orl": orl, "oe": oe})
    p["anims"] = anims
    p["mobj"] = None
    if p["mobjs_off"]:
        mo = struct.unpack_from("<12I", blob, p["mobjs_off"])
        n = mo[0]
        m = {"count": n, "alt": mo[9], "dpal_count": mo[11], "raw": mo}
        m["subs"] = [struct.unpack_from(A.MOBJSUB_FMT, blob,
                                        mo[1] + i * A.MOBJSUB_PACK_SIZE)
                     for i in range(n)]
        m["joint"] = struct.unpack_from("<%dh" % (2 * njoint), blob, mo[2])
        m["batch"] = struct.unpack_from("<%dh" % nbatch, blob, mo[3])
        m["entry"] = struct.unpack_from("<%di" % (n * mo[9]), blob, mo[4])
        m["words"] = list(struct.unpack_from("<%dI" % mo[6], blob, mo[5]))
        m["relocs"] = struct.unpack_from("<%dI" % mo[8], blob, mo[7])
        m["dpals"] = [struct.unpack_from("<16H", blob, mo[10] + 32 * i)
                      for i in range(mo[11])]
        p["mobj"] = m
    return p


def tex_bytes(p, i):
    off, size = p["texs"][i][0], p["texs"][i][1]
    return bytes(p["blob"][p["texdata"] + off:p["texdata"] + off + size])


def tex_key_pack(p, i):
    t = p["texs"][i]
    return (t[2], t[3], t[6], t[5], tex_bytes(p, i))


def tex_key_exp(t):
    return (t["w"], t["h"], t["fmt"],
            (2 if t["clamp_u"] else 0) | (1 if t["clamp_v"] else 0),
            bytes(A.pack_texture(t)))


# -- the check --------------------------------------------------------------

def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


def check_spec(sp, files, rom):
    import pygfxd

    F = []

    def fail(msg):
        F.append(msg)

    with contextlib.redirect_stdout(io.StringIO()):
        try:
            blob = sp.maker(rom)
        except (SystemExit, Exception) as e:       # noqa: B902
            return ["exporter did not run: %s" % str(e)[:200]], {}
    try:
        p = read_pack(blob)
    except (struct.error, ValueError) as e:
        return ["pack unreadable: %s" % e], {}
    st = {"joints": p["njoint"], "batches": p["nbatch"], "tex": p["ntex"],
          "mobjs": p["mobj"]["count"] if p["mobj"] else 0,
          "scripts": 0}

    # -- header and section sanity -----------------------------------------
    if p["hdr"][0] != b"SSBPACKA":
        fail("bad magic %r" % p["hdr"][0])
    if sp.pack_name and p["name"].split(b"\0")[0].decode() != \
            sp.pack_name[:8]:
        fail("pack name %r, wanted %r" % (p["name"], sp.pack_name))
    for i, t in enumerate(p["tris"]):
        if max(t) >= p["nvert"]:
            fail("triangle %d indexes vertex %d of %d" % (i, max(t),
                                                           p["nvert"]))
            break
    for i, v in enumerate(p["verts"]):
        if v[8 + 1] >= p["njoint"]:
            fail("vertex %d belongs to joint %d of %d" % (i, v[9],
                                                         p["njoint"]))
            break
    end = 0
    for i, t in enumerate(p["texs"]):
        end = max(end, t[0] + t[1])
        if t[0] + t[1] > p["texlen"]:
            fail("texture %d runs past texdata" % i)
    if end > p["texlen"]:
        fail("texdata is %d bytes, textures reach %d" % (p["texlen"], end))
    if sp.njoints is not None and p["njoint"] != sp.njoints:
        fail("pack has %d joints, the table says %d"
             % (p["njoint"], sp.njoints))
    # batches tile the triangle list in order
    pos = 0
    for i, b in enumerate(p["batches"]):
        if b[0] != pos:
            fail("batch %d starts at triangle %d, wanted %d" % (i, b[0],
                                                                 pos))
            break
        pos += b[1]
        if b[2] >= p["ntex"] or b[2] < -1:
            fail("batch %d names texture %d of %d" % (i, b[2], p["ntex"]))
        if b[4] >= p["njoint"]:
            fail("batch %d names joint %d" % (i, b[4]))
    if pos != p["ntri"]:
        fail("batches cover %d triangles of %d" % (pos, p["ntri"]))

    # -- the tree ------------------------------------------------------------
    try:
        tree = make_tree(sp, files)
    except (ValueError, KeyError) as e:
        return F + ["ROM tree: %s" % e], st
    if len(tree.joints) != p["njoint"]:
        fail("ROM tree has %d joints, pack %d" % (len(tree.joints),
                                                  p["njoint"]))
    for i, (j, e) in enumerate(zip(p["joints"], tree.joints)):
        par, T, R, Sc = e
        got = (j[0], tuple(j[1:4]), tuple(j[4:7]), tuple(j[7:10]))
        want = (par, tuple(f32(x) for x in T), tuple(f32(x) for x in R),
                tuple(f32(x) for x in Sc))
        for lbl, g, w in zip(("parent", "translate", "rotate", "scale"),
                             got, want):
            if g != w and not all(
                    (a == b) or (a != a and b != b) for a, b in
                    zip(g if isinstance(g, tuple) else (g,),
                        w if isinstance(w, tuple) else (w,))):
                fail("joint %d %s: pack %s, ROM %s" % (i, lbl, g, w))

    if sp.tree[0] == "none":
        if p["nvert"] or p["nbatch"]:
            fail("a no-geometry pack carries %d verts" % p["nvert"])
        return F + check_anims(sp, files, p, st), st

    # -- the MObj tables, ROM side ---------------------------------------------
    nodes_n = p["njoint"]
    subs_by_joint = [[] for _ in range(nodes_n)]
    mobj_ref = sp.mobj and files.resolve(sp.mobj)
    mf = None
    if mobj_ref:
        mf = files.get(mobj_ref[0])
        rd = A.read_mobjsubs(mf[0], mf[1], mobj_ref[1], nodes_n - sp.shift,
                             extern=mf[2])
        subs_by_joint = [[] for _ in range(sp.shift)] + rd
    flat = [s for lst in subs_by_joint for s in lst]
    joint_first, k = [], 0
    for lst in subs_by_joint:
        joint_first.append((k, len(lst)))
        k += len(lst)

    # scripts per alternate per MObj
    alts = []
    for mref in sp.mats:
        r = files.resolve(mref)
        if r is None:
            continue
        mfid, moff = r
        mfile, mreloc, _e = files.get(mfid)
        scripts = []
        for j in range(nodes_n):
            if j < sp.shift:
                continue
            arr = mreloc.get(moff + 4 * (j - sp.shift))
            for kk in range(len(subs_by_joint[j])):
                scripts.append(None if arr is None
                               else mreloc.get(arr + 4 * kk))
        alts.append((mfile, mreloc, scripts))

    # how many pictures each MObj shows
    nv = []
    for k, sub in enumerate(flat):
        n = 1
        for (mfile, mreloc, scripts) in alts:
            if k < len(scripts) and scripts[k] is not None:
                ids = A.matanim_frame_ids(mfile, mreloc, scripts[k])
                if ids:
                    n = max(n, int(max(ids)) + 1)
        if sp.codeframes and sub["sprites"]:
            n = max(n, len(sub["sprites"]))
        if sp.nframes:
            n = max(n, sp.nframes)
        if sp.palframes:
            if len(sub["palettes"]) != sp.palframes:
                fail("MObj %d has %d palettes, table says %d"
                     % (k, len(sub["palettes"]), sp.palframes))
            n = max(n, sp.palframes)
        nv.append(n)

    def subs_for(frame):
        rd = A.read_mobjsubs(mf[0], mf[1], mobj_ref[1], nodes_n - sp.shift,
                             extern=mf[2]) if mobj_ref else []
        rd = [[] for _ in range(sp.shift)] + rd
        kk = 0
        for lst in rd:
            for sub in lst:
                n = nv[kk]
                kk += 1
                if sp.palframes:
                    sub["palette_id"] = min(frame, sp.palframes - 1)
                    continue
                slots = list(sub["sprite_slots"][:max(n, 1)])
                if sp.holes is not None and sub["sprites"] is not None:
                    slots = list(sub["sprite_slots"][:sp.holes or n])
                if slots and any(s is None for s in slots) \
                        and sp.hole_mode == "prev":
                    for i in range(1, len(slots)):
                        if slots[i] is None:
                            slots[i] = slots[i - 1]
                if slots and any(s is None for s in slots):
                    sub["sprites"] = slots
                elif sp.hole_mode == "prev" and slots and sp.holes:
                    sub["sprites"] = slots
                if sub["sprites"] or slots:
                    i = min(frame, len(sub["sprites"] or slots) - 1)
                    if sub["sprites"] and sub["sprites"][i] is None:
                        i = 0
                    sub["texture_id_curr"] = max(i, 0)
        return rd

    # -- the independent bake ------------------------------------------------
    tfid = files.resolve(sp.tree[1])[0]
    f, reloc, _ext = files.get(tfid)
    try:
        baker = A.bake_retry(
            lambda force: A.MeshBaker(f, pygfxd, subs_for(0),
                                      force_extent=force, rendermode=sp.seed,
                                      mobj_batches=mobj_ref is not None,
                                      paletted=sp.paletted),
            lambda b: b.bake(tree.build()))
    except Exception as e:                           # noqa: B902
        return F + ["independent bake failed: %s" % str(e)[:200]], st
    verts, tris, joints = baker.verts, baker.tris, baker.joints
    forced = baker.force_extent

    if len(verts) != p["nvert"] or len(tris) != p["ntri"] or \
            len(baker.batches) != p["nbatch"]:
        fail("ROM bake gives %d verts/%d tris/%d batches, pack %d/%d/%d"
             % (len(verts), len(tris), len(baker.batches), p["nvert"],
                p["ntri"], p["nbatch"]))
        return F, st
    if len(baker.nodes) != p["njoint"]:
        fail("ROM bake has %d nodes, pack %d joints" % (len(baker.nodes),
                                                       p["njoint"]))
    for i, (par, T, R, Sc) in enumerate(
            [(n.parent.index, n.translate, n.rotate, n.scale)
             for n in baker.nodes]):
        if i < len(tree.joints) and (par, T, R, Sc) != tree.joints[i]:
            fail("baker node %d differs from the hand-decoded tree" % i)
    for i, (v, w) in enumerate(zip(p["verts"], verts)):
        # pack order x y z nx ny nz u v alpha joint ; baker x y z u v nx ny nz
        want = (f32(w[0]), f32(w[1]), f32(w[2]), f32(w[5]), f32(w[6]),
                f32(w[7]), f32(w[3]), f32(w[4]), w[8], w[9])
        if tuple(v[:10]) != want:
            fail("vertex %d: pack %s, ROM bake %s" % (i, v[:10], want))
            break
    if [tuple(t) for t in p["tris"]] != [tuple(t) for t in tris]:
        fail("triangle list differs from the ROM bake")

    joint_of = {}
    for j in joints:
        for b in range(j["batch_first"], j["batch_first"] + j["batch_count"]):
            joint_of[b] = j["node"].index
    exp_bm = []
    for i, b in enumerate(baker.batches):
        m = b["mobj"]
        exp_bm.append(-1 if (m is None or m[0] != joint_of[i])
                      else joint_first[m[0]][0] + m[1])
    batch_of = {}
    for i, m in enumerate(exp_bm):
        batch_of.setdefault(m, i)

    keep = ~NOZ_ENV_TL
    for i, (b, eb) in enumerate(zip(p["batches"], baker.batches)):
        mat = eb["mat"]
        lit_shaded = (1 if mat[5] else 2) if mat[4] else 0
        env = 0 if mat[8] is None else mat[8]
        ebucket = (eb.get("bucket", 0) | sp.bucket_or)
        if b[0] != eb["tri_first"] or b[1] != eb["tri_count"]:
            fail("batch %d: tris %d+%d, ROM bake %d+%d"
                 % (i, b[0], b[1], eb["tri_first"], eb["tri_count"]))
        if b[4] != joint_of[i]:
            fail("batch %d: joint %d, ROM bake %d" % (i, b[4], joint_of[i]))
        if (b[5] & keep) != (ebucket & keep) or \
                bool(b[5] & A.FPACK_NOZ) != bool(ebucket & A.FPACK_NOZ):
            fail("batch %d: bucket 0x%X, ROM bake 0x%X" % (i, b[5], ebucket))
        if bool(b[5] & A.FPACK_ENVLERP) != (mat[8] is not None):
            fail("batch %d: ENVLERP bit %d, ROM combiner %s"
                 % (i, bool(b[5] & A.FPACK_ENVLERP), mat[8]))
        if bool(b[5] & A.FPACK_TEXLERP) != bool(len(mat) > 10 and mat[10]):
            fail("batch %d: TEXLERP bit disagrees with the combiner" % i)
        if b[3] != lit_shaded:
            fail("batch %d: shaded %d, ROM bake %d" % (i, b[3], lit_shaded))
        if (b[7], b[8], b[9], b[10]) != (mat[1] & 0xFFFFFFFF,
                                         mat[2] & 0xFFFFFFFF,
                                         mat[3] & 0xFFFFFFFF,
                                         env & 0xFFFFFFFF):
            fail("batch %d: colours prim/l1/l2/env %08X/%08X/%08X/%08X, ROM "
                 "%08X/%08X/%08X/%08X" % (i, b[7], b[8], b[9], b[10],
                                          mat[1] & 0xFFFFFFFF,
                                          mat[2] & 0xFFFFFFFF,
                                          mat[3] & 0xFFFFFFFF,
                                          env & 0xFFFFFFFF))

    # bounds
    world = baker.world_verts()
    lo, hi = baker.bounds()
    c = tuple((lo[i] + hi[i]) / 2 for i in range(3))
    rad = max(math.sqrt(sum((q[i] - c[i]) ** 2 for i in range(3)))
              for q in world)
    if any(abs(a - b) > 1e-3 * max(1.0, abs(b)) for a, b in
           zip(p["center"], c)) or abs(p["radius"] - rad) > 1e-3 * max(
            1.0, rad):
        fail("bounds: pack centre %s radius %g, ROM %s %g"
             % (tuple(round(x, 3) for x in p["center"]), p["radius"],
                tuple(round(x, 3) for x in c), rad))

    # -- textures -----------------------------------------------------------
    expect_keys = set()

    def pack_pal(ti):
        pl = p["texs"][ti][4]
        return None if (pl >= p["npal"] or p["texs"][ti][6] != A.PVRTEX_PAL4) \
            else p["pals"][pl]

    def cmp_tex(label, ti, t):
        if not 0 <= ti < p["ntex"]:
            fail("%s names texture %d of %d" % (label, ti, p["ntex"]))
            return
        expect_keys.add(tex_key_exp(t))
        if tex_key_pack(p, ti) != tex_key_exp(t):
            g, w = tex_key_pack(p, ti), tex_key_exp(t)
            fail("%s (texture %d): pack %dx%d fmt %d clamp %d %dB, ROM bake "
                 "%dx%d fmt %d clamp %d %dB%s"
                 % (label, ti, g[0], g[1], g[2], g[3], len(g[4]), w[0], w[1],
                    w[2], w[3], len(w[4]),
                    "" if g[:4] != w[:4] else " (same size, texels differ)"))
        if t["pal"] >= 0:
            want = tuple(baker.palettes[t["pal"]])
            got = pack_pal(ti)
            if got is None or tuple(got) != want:
                # a dynamic-palette MObj's bank may be a copy; contents
                # still have to match
                fail("%s: palette bank differs from the ROM bake" % label)

    for i, (b, eb) in enumerate(zip(p["batches"], baker.batches)):
        t0 = eb["mat"][0]
        if t0 < 0:
            if b[2] != -1:
                fail("batch %d should be untextured, names texture %d"
                     % (i, b[2]))
            continue
        cmp_tex("batch %d" % i, b[2], baker.textures[t0])

    pm = p["mobj"]
    if mobj_ref is None:
        if pm is not None and pm["count"]:
            fail("pack carries %d MObjs, ROM effect has none" % pm["count"])
    else:
        if pm is None:
            fail("ROM effect has MObjs, pack has no MObj section")
            return F, st
        if pm["count"] != len(flat):
            fail("pack has %d MObjs, ROM tables hold %d" % (pm["count"],
                                                           len(flat)))
            return F, st
        if tuple(pm["joint"]) != tuple(v for pr in joint_first for v in pr):
            fail("joint->MObj run table %s, ROM %s"
                 % (tuple(pm["joint"]), tuple(v for pr in joint_first
                                              for v in pr)))
        if tuple(pm["batch"]) != tuple(exp_bm):
            fail("batch->MObj map %s, ROM bake %s" % (tuple(pm["batch"]),
                                                      tuple(exp_bm)))
        if pm["alt"] != max(1, len(alts)):
            fail("MObj alternates %d, ROM has %d MatAnimJoints"
                 % (pm["alt"], max(1, len(alts))))

        # later frames: one bake per frame, textures of each MObj's run
        runs = {}
        frames_b = {0: baker}
        for fr in range(1, max(nv)):
            try:
                b2 = A.MeshBaker(f, pygfxd, subs_for(fr),
                                 force_extent=forced, rendermode=sp.seed,
                                 mobj_batches=True, paletted=sp.paletted)
                b2.bake(tree.build())
            except Exception as e:                   # noqa: B902
                fail("frame %d bake failed: %s" % (fr, str(e)[:120]))
                continue
            if (len(b2.verts), len(b2.tris), len(b2.batches)) != \
                    (len(verts), len(tris), len(baker.batches)):
                fail("frame %d bakes to different geometry" % fr)
                continue
            frames_b[fr] = b2
        for k, sub in enumerate(flat):
            ps = pm["subs"][k]
            (flags, prim_l, _p, prim, env, blend, l1, l2, tfirst, tcount,
             ub0, ub1, us0, us1, u0b0, u0b1, u0s0, u0s1,
             dfirst, dcount, dbank, _dp) = ps
            for nm, g, w in (("flags", flags, sub["flags"]),
                             ("prim_l", prim_l, sub["prim_l"]),
                             ("primcolor", prim, sub["primcolor"]),
                             ("envcolor", env, sub["envcolor"]),
                             ("blendcolor", blend, sub["blendcolor"]),
                             ("light1", l1, sub["light1"]),
                             ("light2", l2, sub["light2"])):
                if g != w:
                    fail("MObj %d %s: pack 0x%X, ROM MObjSub@0x%04X 0x%X"
                         % (k, nm, g, sub["off"], w))
            if tcount != nv[k]:
                fail("MObj %d: pack carries %d frames, ROM reaches %d"
                     % (k, tcount, nv[k]))
            has_script = any(k < len(sc) and sc[k] is not None
                             for (_a, _b, sc) in alts)
            if not has_script and any((ub0, ub1, us0, us1, u0b0, u0b1, u0s0,
                                       u0s1)):
                fail("MObj %d has no script but nonzero uv scroll fields" % k)
            bi = batch_of.get(k)
            if bi is None:
                fail("MObj %d draws no batch" % k)
                continue
            for fr in range(nv[k]):
                b2 = frames_b.get(fr)
                if b2 is None:
                    continue
                t = b2.batches[bi]["mat"][0]
                if t < 0:
                    if p["batches"][bi][2] != -1 or nv[k] != 1:
                        fail("MObj %d frame %d is untextured in the ROM" %
                             (k, fr))
                    continue
                cmp_tex("MObj %d frame %d" % (k, fr), tfirst + fr,
                        b2.textures[t])
            # the batch itself shows frame 0 of its run
            if tfirst < p["ntex"] and p["batches"][bi][2] >= 0 and \
                    tex_key_pack(p, p["batches"][bi][2]) != \
                    tex_key_pack(p, tfirst):
                fail("MObj %d: its batch's texture is not frame 0 of the run"
                     % k)
            # dynamic palette
            dyn = (sub["flags"] & A.MOBJ_FLAG_PALETTE) and has_script
            exp_n = 0
            if dyn:
                ids = set()
                for (mfile, mreloc, sc) in alts:
                    if k < len(sc) and sc[k] is not None:
                        ids |= A.matanim_frame_ids(mfile, mreloc, sc[k],
                                                   A.MATANIM_BIT_PALETTEID)
                n = (int(max(ids)) + 1) if ids else 1
                exp_n = n if n > 1 else 0
            if dcount != exp_n:
                fail("MObj %d: dpal_count %d, script reaches %d palettes"
                     % (k, dcount, exp_n))
            if exp_n:
                for i in range(exp_n):
                    want = tuple(A.read_palette(mf[0], sub["palettes"][i]))
                    if dfirst + i >= len(pm["dpals"]) or \
                            tuple(pm["dpals"][dfirst + i]) != want:
                        fail("MObj %d dynamic palette %d differs from the "
                             "ROM TLUT" % (k, i))
                if 0 <= dbank < p["npal"] and \
                        tuple(p["pals"][dbank]) != tuple(pm["dpals"][dfirst]):
                    fail("MObj %d: bank %d is not dynamic frame 0" % (k, dbank))
            elif dbank != -1 and dcount == 0 and dbank != -1:
                fail("MObj %d: dpal_bank %d without frames" % (k, dbank))
        # script mapping
        for a, (mfile, mreloc, scripts) in enumerate(alts):
            rsrc = PA.rom_src(mfile, mreloc)
            psrc = PA.pack_src(pm["words"], pm["relocs"])
            for k, s in enumerate(scripts):
                e = pm["entry"][a * pm["count"] + k]
                if s is None:
                    if e != -1:
                        fail("MObj %d alt %d: pack has a script, ROM none"
                             % (k, a))
                    continue
                if e < 0:
                    fail("MObj %d alt %d: ROM has a script, pack none"
                         % (k, a))
                    continue
                st["scripts"] += 1
                PA.compare("MObj %d alt %d MatAnimJoint" % (k, a), rsrc, s,
                           psrc, e * 4, True)
        if not alts and any(e != -1 for e in pm["entry"]):
            fail("no ROM MatAnimJoint but the pack has script entries")

    # no texture the ROM does not produce
    for i in range(p["ntex"]):
        if tex_key_pack(p, i) not in expect_keys:
            fail("pack texture %d is not any ROM bake's picture" % i)

    F.extend(check_anims(sp, files, p, st))
    F.extend(PA.FAILS)
    del PA.FAILS[:]
    return F, st


def check_anims(sp, files, p, st):
    F = []
    exp = sp.anims
    if len(p["anims"]) != len(exp):
        return ["pack has %d AnimJoint blocks, table says %d"
                % (len(p["anims"]), len(exp))]
    for (name, ref, tlen, patch), blk in zip(exp, p["anims"]):
        r = files.resolve(ref)
        if r is None:
            if any(e >= 0 for e in blk["entries"]):
                F.append("anim %s: pack drives joints, ROM has none" % name)
            continue
        afid, aoff = r
        af, arl, _e = files.get(afid)
        if sp.anim_direct:
            table = [aoff]
        else:
            table = [arl.get(aoff + 4 * k) for k in
                     range(tlen if tlen is not None
                           else p["njoint"] - sp.shift)]
        if patch:
            for j, pref in patch.items():
                pf, po = files.resolve(pref)
                table[j] = po
        table = [None] * sp.shift + table
        if len(table) != p["njoint"]:
            F.append("anim %s: ROM table has %d slots for %d joints"
                     % (name, len(table), p["njoint"]))
            continue
        if blk["name"] != name[:EE.ANIM_NAME_LEN - 1]:
            F.append("anim block named %r, wanted %r" % (blk["name"], name))
        rsrc = PA.rom_src(af, arl)
        psrc = PA.pack_src(blk["words"], blk["relocs"])
        for j, t in enumerate(table):
            e = blk["entries"][j]
            if t is None:
                if e != -1:
                    F.append("anim %s joint %d: pack drives it, ROM does not"
                             % (name, j))
                continue
            if e < 0:
                F.append("anim %s joint %d: ROM drives it, pack does not"
                         % (name, j))
                continue
            st["scripts"] += 1
            PA.compare("anim %s joint %d" % (name, j), rsrc, t, psrc,
                       e * 4, False)
    return F


# -- driver -----------------------------------------------------------------

_G = {}


# The specs whose packs are weapons (romdisk/wp*.mdl), which the exporter
# bakes with EE.WP_ZSEED as the baker's default (its main()); both the
# pack and the independent bake below are made with it here too.
WEAPON_NAMES = ("fireball", "blaster", "arwinglaser")


def _work(name):
    sp = _G["specs"][name]
    files = _G["files"]
    A.MeshBaker.default_seed = (dict(EE.WP_ZSEED)
                                if name.startswith("wp:") or
                                name in WEAPON_NAMES else {})
    try:
        F, st = check_spec(sp, files, _G["rom"])
    except Exception as e:                           # noqa: B902
        import traceback
        F, st = ["checker error: %s" % traceback.format_exc()[-400:]], {}
    return name, F, st


def main():
    argv = sys.argv[1:]
    rom_path = M.ROM_DEFAULT
    if "--rom" in argv:
        rom_path = argv[argv.index("--rom") + 1]
    only = None
    if "--only" in argv:
        only = set(argv[argv.index("--only") + 1].split(","))
    jobs = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else \
        min(8, os.cpu_count() or 2)
    if not os.path.exists(rom_path):
        print("effect_model_check: no baserom at %s -- skipping" % rom_path)
        return
    load_syms()
    specs = build_specs()
    exported = set(["halo", "quake", "orbs", "slash", "spark", "mdust",
                    "deadexplode", "shield", "yoshiegg", "impactwave",
                    "fireball", "blaster", "shock", "firespark",
                    "arwinglaser"])
    exported |= set("wp:" + k for k in list(EE.WPQUAD) + list(EE.WPTREE) +
                    list(EE.WPLINK) + list(EE.WPFLAT))
    exported |= set("ef:" + k for k in EE.EFENTRY)
    have = set(s.name for s in specs)
    if exported != have:
        sys.exit("effect_model_check: the table and the exporter disagree: "
                 "missing %s, extra %s" % (sorted(exported - have),
                                           sorted(have - exported)))
    if "--list" in argv:
        for s in specs:
            print(s.name)
        return
    rom = open(rom_path, "rb").read()
    _G["specs"] = {s.name: s for s in specs}
    _G["files"] = Files(rom)
    _G["rom"] = rom
    names = [s.name for s in specs if not only or s.name in only]

    results = []
    if jobs > 1 and len(names) > 1:
        ctx = __import__("multiprocessing").get_context("fork")
        with concurrent.futures.ProcessPoolExecutor(
                max_workers=jobs, mp_context=ctx) as ex_:
            results = list(ex_.map(_work, names))
    else:
        results = [_work(n) for n in names]

    nfail = 0
    tot = {"joints": 0, "batches": 0, "tex": 0, "mobjs": 0, "scripts": 0}
    for name, F, st in results:
        for k in tot:
            tot[k] += st.get(k, 0)
        for m in F[:12]:
            print("FAIL %s: %s" % (name, m))
        if len(F) > 12:
            print("FAIL %s: ... and %d more" % (name, len(F) - 12))
        nfail += len(F)
    print("  %d packs: %d joints, %d batches, %d textures, %d MObjs, %d "
          "scripts mapped" % (len(results), tot["joints"], tot["batches"],
                              tot["tex"], tot["mobjs"], tot["scripts"]))
    if nfail:
        sys.exit("effect_model_check: %d problems" % nfail)
    print("effect_model_check: every exported effect/weapon pack matches "
          "the ROM")


if __name__ == "__main__":
    main()
