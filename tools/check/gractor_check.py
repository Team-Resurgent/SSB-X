#!/usr/bin/env python3
"""ssb64-dc: every stage's ground actors, against ef/efground.c's own data.

A ground actor's table is the ONE part of a stage pack that is not read out
of the ROM. `EFGroundDesc`/`EFGroundParam` are the game executable's `.data`,
compiled from `ef/efground.c` -- there is no relocData block to walk -- so
`STAGES[stage]["ground_actors"]` in tools/export/ssb_stageexport.py is a hand
transcription of roughly ninety numbers, and a hand transcription is exactly
the kind of thing that is wrong in a way nothing notices.

Nothing notices, specifically, because every field here degrades quietly. A
wrong `weight` picks the wrong actor slightly too often. A wrong `pos_z`
puts a bird at the wrong depth in a scene full of other birds. A wrong
`update_kind` drops the one line that makes Sector Z's rocket grow. A desc
pointed at the wrong `pack` spawns the wrong creature, which on a background
layer, once every few thousand tics, nobody sees. None of it crashes and
none of it fails a build.

So: parse the decomp's own arrays and compare them to the GRA1 block the
build actually wrote, field for field, for every stage that ships one --
and name any stage the decomp gives a table that the port does not carry,
so a gap stays counted rather than forgotten.

What is checked, per desc: alt_high, alt_low, pos_z, scale, effect_status,
the update proc, the spawn hook, the DL link, and the AnimJoint's name.
Structurally: two descs share a model index exactly when they share a
DObjDesc symbol, a desc's pack carries an MObjs section exactly when the
decomp gives it a MObjSub and a MatAnimJoint, the desc's material
alternate is one the pack actually carries, and two descs on one pack
agree on that alternate exactly when they name the same MatAnimJoint --
Sector Z's two ships share one script and must share an alternate, Dream
Land's two Brontos have one each and must not. Per param: all four
fields, in order.

One thing is NOT checked: the EFDesc flags and transform kinds, which
stage_load_ground hard-codes because every desc in the game agrees on
them.

Usage: python3 tools/check/gractor_check.py
"""
import glob
import os
import re
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_paths                            # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROMDISK = os.path.join(ROOT, "src", "game", "ssb64", "romdisk")
EFGROUND_C = os.path.join(ssb_paths.DECOMP_DIR, "src", "ef", "efground.c")

# EFGroundDesc.effect_desc.proc_update, and EFGroundDesc.proc_groundeffect.
# The numbers are stage_load_ground's own switch; the names are efground.c's.
UPDATE_KINDS = {"efGroundCommonProcUpdate": 0,
                "efGroundUpdateEffectYaw": 1,
                "efGroundUpdateStepPositions": 2}
SETUP_KINDS = {"NULL": 0, "efGroundSetStepPositions": 1}

# GRA1's own record sizes (src/dc/stage.c stage_load_ground spells out every
# byte offset; tools/export/ssb_stageexport.py ground_actors_section writes
# them).
DESC_SIZE = 60
PARAM_SIZE = 12


def decomp_tables():
    """{stage: (descs, params)} out of ef/efground.c's own arrays.

    A desc comes back as a dict of the fields GRA1 carries plus the three
    asset symbols; a param as the four-tuple the C initialiser is.
    """
    src = open(EFGROUND_C).read()
    src = re.sub(r"//.*", "", src)
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    out = {}

    for m in re.finditer(r"dEFGround(\w+)EffectDescs\[[^\]]*\]\s*=\s*"
                         r"\{(.*?)\n\};", src, re.S):
        stage = m.group(1)
        blocks, depth, cur = [], 0, ""
        for ch in m.group(2):
            if ch == "{":
                depth += 1
                if depth == 1:
                    cur = ""
                    continue
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    blocks.append(cur)
                    continue
            if depth >= 1:
                cur += ch
        descs = []
        for b in blocks:
            t = [x.strip() for x in re.split(r"[,\n{}]", b) if x.strip()]
            # The scalars are the first six and the four asset symbols the
            # last four, so both ends are indexed from their own end and
            # the middle (flags, the two transform triples, the two procs)
            # is stepped over rather than counted -- the flags field is an
            # OR expression and the transform triples are nested braces,
            # and neither tokenises to a fixed width worth relying on.
            descs.append({
                "alt_high": float(t[0].rstrip("Ff")),
                "alt_low": float(t[1].rstrip("Ff")),
                "pos_z": float(t[2].rstrip("Ff")),
                "scale": float(t[3].rstrip("Ff")),
                "effect_status": int(t[4]),
                "setup": t[5],
                "dl_link": int(t[7]),
                "update": t[-6],
                "display": t[-5],
                "dobjdesc": t[-4],
                "mobjsub": t[-3],
                "anim": t[-2],
                "matanim": t[-1],
            })
        params = None
        p = re.search(r"dEFGround%sParams\[[^\]]*\]\s*=\s*\n?\{(.*?)\n\};"
                      % stage, src, re.S)
        if p:
            params = [tuple(eval(x) for x in r.split(","))
                      for r in re.findall(r"\{([^}]*)\}", p.group(1))]
        out[stage] = (descs, params)
    return out


def symbol_tail(sym, stage, suffix):
    """`&llGRYosterMapHeihoFruitSlowAnimJoint` -> `HeihoFruitSlow`.

    The exporter names a GRA1 anim by its map-group BLOCK name, and the
    block name is exactly this middle piece -- which is the whole reason
    the two can be compared at all.
    """
    if sym in ("0x0", "NULL", "0"):
        return None
    m = re.match(r"&?llGR%sMap(\w+)%s$" % (stage, suffix), sym)
    if not m:
        raise AssertionError("%s is not a GR%sMap*%s symbol"
                             % (sym, stage, suffix))
    return m.group(1)


def read_gra1(path):
    """A .stg's GRA1 block, or None. Mirrors stage_load_ground's reader."""
    blob = open(path, "rb").read()
    i = blob.find(b"GRA1")
    if i < 0:
        return None
    (mc, dc, pc, ac, _fc, off_pack, _pack_size, off_models, off_descs,
     off_params, off_anims, _off_fixups,
     _bsz) = struct.unpack_from("<13I", blob, i + 4)

    anims = []
    off = i + off_anims
    for _ in range(ac):
        n, joint, name = struct.unpack_from("<II32s", blob, off)
        anims.append((name.split(b"\0")[0].decode(), joint))
        off += 40 + n * 4

    models = []
    for k in range(mc):
        o, _sz = struct.unpack_from("<2I", blob, i + off_models + k * 8)
        # FPackHeader's last trailing word is off_mobjs, 0 for a pack with
        # no MObjs section (src/dc/fighter.h).
        hd = struct.unpack_from("<8s8I8I3ff8s8I", blob, i + off_pack + o)
        # FPackMObjs' tenth and last word is alt_count -- how many whole
        # MatAnimJoints the pack carries, one per state the descs pick
        # between (src/dc/fighter.h).
        alts = 0
        if hd[-1] != 0:
            alts, = struct.unpack_from("<I", blob,
                                       i + off_pack + o + hd[-1] + 9 * 4)
        models.append({"joints": hd[1], "mobjs": hd[-1] != 0, "alts": alts})

    descs = []
    for k in range(dc):
        r = i + off_descs + k * DESC_SIZE
        f = struct.unpack_from("<4f", blob, r)
        status, update = struct.unpack_from("<2H", blob, r + 16)
        model, = struct.unpack_from("<I", blob, r + 20)
        descs.append({
            "alt_high": f[0], "alt_low": f[1], "pos_z": f[2], "scale": f[3],
            # written as a u16; EFGroundDesc.effect_status is signed and
            # every desc in the game carries -1.
            "effect_status": status - 0x10000 if status > 0x7FFF else status,
            "update": update, "model": model,
            "dl_link": blob[r + 24], "setup": blob[r + 25],
            "matanim_alt": blob[r + 26], "billboard": blob[r + 27],
            "anim": blob[r + 28:r + DESC_SIZE].split(b"\0")[0].decode(),
        })

    params = [struct.unpack_from("<HHiB3x", blob, i + off_params + k * PARAM_SIZE)
              for k in range(pc)]
    return {"models": models, "descs": descs, "params": params,
            "anims": anims}


def check():
    fails = []
    tables = decomp_tables()
    carried = 0
    missing = []

    for stage, (want_descs, want_params) in sorted(tables.items()):
        mats = {}
        path = os.path.join(ROMDISK, "%s.stg" % stage.lower())
        if not os.path.exists(path):
            fails.append("%s: no %s.stg in the romdisk" % (stage, stage.lower()))
            continue
        got = read_gra1(path)
        if got is None:
            missing.append("%s (%d desc(s))" % (stage, len(want_descs)))
            continue
        carried += 1

        if len(got["descs"]) != len(want_descs):
            fails.append("%s: GRA1 carries %d desc(s), efground.c has %d"
                         % (stage, len(got["descs"]), len(want_descs)))
            continue

        # Two descs share a model exactly when they share a DObjDesc.
        same = {}
        for k, (g, w) in enumerate(zip(got["descs"], want_descs)):
            for f in ("alt_high", "alt_low", "pos_z", "scale"):
                if abs(g[f] - w[f]) > 1e-4:
                    fails.append("%s desc %d: %s is %g, efground.c says %g"
                                 % (stage, k, f, g[f], w[f]))
            if g["effect_status"] != w["effect_status"]:
                fails.append("%s desc %d: effect_status %d, efground.c says %d"
                             % (stage, k, g["effect_status"],
                                w["effect_status"]))
            if g["dl_link"] != w["dl_link"]:
                fails.append("%s desc %d: dl_link %d, efground.c says %d"
                             % (stage, k, g["dl_link"], w["dl_link"]))
            if w["update"] not in UPDATE_KINDS:
                fails.append("%s desc %d: efground.c's proc_update is %s, "
                             "which stage_load_ground has no case for"
                             % (stage, k, w["update"]))
            elif g["update"] != UPDATE_KINDS[w["update"]]:
                fails.append("%s desc %d: update_kind %d, efground.c says "
                             "%s (%d)" % (stage, k, g["update"], w["update"],
                                          UPDATE_KINDS[w["update"]]))
            if w["setup"] not in SETUP_KINDS:
                fails.append("%s desc %d: efground.c's proc_groundeffect is "
                             "%s, which stage_load_ground has no case for"
                             % (stage, k, w["setup"]))
            elif g["setup"] != SETUP_KINDS[w["setup"]]:
                fails.append("%s desc %d: setup_kind %d, efground.c says "
                             "%s (%d)" % (stage, k, g["setup"], w["setup"],
                                          SETUP_KINDS[w["setup"]]))

            anim = symbol_tail(w["anim"], stage, "AnimJoint")
            if (anim or "") != g["anim"]:
                fails.append("%s desc %d: anim %r, efground.c names %s"
                             % (stage, k, g["anim"], w["anim"]))
            if anim and not any(a == anim for (a, _j) in got["anims"]):
                fails.append("%s desc %d: anim %s is in no GRA1 script"
                             % (stage, k, anim))

            dd = w["dobjdesc"]
            if dd in same and same[dd] != g["model"]:
                fails.append("%s desc %d: model %d, but desc %d shares its "
                             "DObjDesc %s and took model %d"
                             % (stage, k, g["model"],
                                [d["dobjdesc"] for d in want_descs].index(dd),
                                dd, same[dd]))
            same.setdefault(dd, g["model"])

            if g["model"] >= len(got["models"]):
                fails.append("%s desc %d: model %d out of %d"
                             % (stage, k, g["model"], len(got["models"])))
            else:
                m = got["models"][g["model"]]
                has = m["mobjs"]
                wants = symbol_tail(w["mobjsub"], stage, "MObjSub") is not None
                if has and not wants:
                    fails.append("%s desc %d: its pack carries an MObjs "
                                 "section and efground.c gives it no MObjSub"
                                 % (stage, k))
                # The MATERIAL animation. A desc whose EFDesc names a
                # MatAnimJoint has to end up with one to play, which means
                # its pack carries an MObjs section (the scripts ride in
                # it -- src/dc/efground.h EFGroundActorAsset) and the
                # desc's own alternate is one of that pack's.
                mat = symbol_tail(w["matanim"], stage, "MatAnimJoint")
                if (mat is not None) != has:
                    fails.append(
                        "%s desc %d: efground.c %s a MatAnimJoint and its "
                        "pack %s an MObjs section"
                        % (stage, k, "names" if mat else "names no",
                           "carries" if has else "carries no"))
                elif mat is not None and g["matanim_alt"] >= m["alts"]:
                    fails.append(
                        "%s desc %d: material alternate %d, but its pack "
                        "carries %d" % (stage, k, g["matanim_alt"],
                                        m["alts"]))
                mats.setdefault(g["model"], {}).setdefault(
                    mat, []).append((k, g["matanim_alt"]))
                # efground.c:1328-1343's billboard joints. Every ground
                # actor in the game has one, and a bit past the pack's
                # joints would name a DObj the tree does not have.
                if g["billboard"] == 0 or g["billboard"] >> m["joints"]:
                    fails.append("%s desc %d: billboard mask 0x%X over a "
                                 "%d-joint pack" % (stage, k, g["billboard"],
                                                    m["joints"]))

        # Two descs on one pack agree on the alternate exactly when they
        # name the same MatAnimJoint. Sector Z's two ships and Dream
        # Land's two Brontos are the pairs that make this say something:
        # the ships share one script and must share an alternate, the
        # Brontos have one each and must not.
        for model, by_sym in sorted(mats.items()):
            seen = {}
            for sym, uses in sorted(by_sym.items(), key=lambda kv: str(kv[0])):
                alts = {a for (_k, a) in uses}
                if len(alts) != 1:
                    fails.append(
                        "%s: descs %s share model %d and MatAnimJoint %s but "
                        "play alternates %s"
                        % (stage, [k for (k, _a) in uses], model, sym,
                           sorted(alts)))
                    continue
                a = alts.pop()
                if a in seen and seen[a] != sym:
                    fails.append(
                        "%s: model %d plays alternate %d for both %s and %s"
                        % (stage, model, a, seen[a], sym))
                seen[a] = sym

        # Two descs with DIFFERENT DObjDescs must not share a model.
        by_model = {}
        for k, w in enumerate(want_descs):
            m = got["descs"][k]["model"]
            if m in by_model and by_model[m][0] != w["dobjdesc"]:
                fails.append("%s: descs %d and %d share model %d and name "
                             "different DObjDescs (%s, %s)"
                             % (stage, by_model[m][1], k, m,
                                by_model[m][0], w["dobjdesc"]))
            by_model.setdefault(m, (w["dobjdesc"], k))

        if want_params is None:
            fails.append("%s: efground.c has no dEFGround%sParams"
                         % (stage, stage))
        elif list(got["params"]) != [tuple(p) for p in want_params]:
            fails.append("%s: params %s, efground.c says %s"
                         % (stage, list(got["params"]), want_params))

    print("gractor_check: %d of %d stage(s) with a ground-actor table in "
          "ef/efground.c carry one in their .stg" % (carried, len(tables)))
    if missing:
        print("  not carried yet: %s" % ", ".join(missing))
    return fails


def main():
    fails = check()
    if fails:
        for m in fails:
            print("gractor_check: %s" % m)
        sys.exit("gractor_check: %d problem(s)" % len(fails))
    print("gractor_check: every carried desc and param is ef/efground.c's")


if __name__ == "__main__":
    main()
