#!/usr/bin/env python3
"""ssb64-dc: the metal dust's material, against the ROM it came out of.

The metal dust (ef/efmanager.c:3574) is the damage sparks
again out of four blocks of its own, so tools/check/flipbook_check.py does
almost all of this: the flags-zero default, the tile the two formulas
compute, the MatAnimJoint replayed tic for tic, the tracks the script
leaves alone, and the seven frames being seven pictures.

Two things are this effect's own, and both are read here rather than
assumed:

  the display list is reached through a *DObjDLLink array* -- flags 0x1
  hands gcAddChildForDObj a pointer, and this EFDesc's render proc is
  gcDrawDObjTreeDLLinksForGObj (objdisplay.c:2380-2392), which reads that
  pointer as {s32 list_id; Gfx *dl} pairs rather than as commands. The
  shared checker walks the array and holds it to one link.

  a frame is matched to its ROM sprite *exactly*. The sparks' CI4 sprites
  can only be compared by how many colours they use, because the pack no
  longer holds the indices; the metal dust's are IA16, and IA16 -> ARGB4444
  is a function -- intensity to r, g and b, alpha to a, four bits each.
  So the set of 16-bit texels in the pack's frame must equal the set the
  ROM sprite converts to, value for value. The PVR twiddles the texels and
  the RDP mirrors the tile, and both are permutations, which is why this
  is a set and not a sequence; but it is the sixteen-bit values themselves
  and not a count of them, which makes it strictly the stronger of the two
  and catches an array packed backwards on the first frame.

Usage: python3 tools/check/mdust_check.py [--rom <rom.z64>]
"""
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_effectexport as S     # noqa: E402
import flipbook_check as F       # noqa: E402

# How far past the animation to replay it, as the sparks': the script runs
# seven tics and sixteen reaches the End command and the words after it.
FRAMES = 16


def ia16_to_argb4444(v):
    """The conversion tools/lib/ssb_assets.py bakes an IA16 texel with."""
    i, a = v >> 8, v & 0xFF
    return ((a >> 4) << 12) | ((i >> 4) << 8) | ((i >> 4) << 4) | (i >> 4)


def texels(f, reloc, sub, k, w, h, picture):
    sp = sub["sprites"][k]
    want = {ia16_to_argb4444(v)
            for v in struct.unpack_from(">%dH" % (w * h), f, sp)}
    got = set(struct.unpack("<%dH" % (len(picture) // 2), picture))
    if want != got:
        return ["frame %d holds %d texel values and sprite 0x%04X, which the "
                "ROM has it at, converts to %d, %d of them shared -- the "
                "array is packed out of order or converted wrong"
                % (k, len(got), sp, len(want), len(want & got))]
    return []


MDUST = F.Flipbook(
    "mdust",
    ("llEFCommonEffects1FileID",
     "llEFCommonEffects1DamageFlyMDustMObjSub",
     "llEFCommonEffects1DamageFlyMDustDObjDesc",
     "llEFCommonEffects1DamageFlyMDustAnimJoint",
     "llEFCommonEffects1DamageFlyMDustMatAnimJoint"),
    dl_is_links=True, pack=S.pack_mdust, frames=FRAMES, texels=texels)


if __name__ == "__main__":
    F.main(MDUST, "the metal dust's MObj, its sprite array and its "
                  "MatAnimJoint are the ROM's")
