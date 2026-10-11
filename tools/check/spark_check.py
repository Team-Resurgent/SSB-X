#!/usr/bin/env python3
"""ssb64-dc: the damage sparks' material, against the ROM it came out of.

The sparks (ef/efmanager.c:3493) are the second flipbook in
the port and the first that depends on a rule nothing before it needed --
gcDrawMObjForDObj's flags-zero default. tools/check/flipbook_check.py holds
that check and everything else the sparks share with the metal dust
; this is the sparks' own half of it.

Their half is how a frame is matched to the ROM sprite it is supposed to
be. The sparks' sprites are CI4, drawn through the sixteen-entry TLUT the
display list loads at 0x8FD8, so what the pack's ARGB4444 frame and the
ROM's four-bit indices have in common is *how many distinct colours the
picture uses* -- a count no reordering and no swizzle preserves, and
therefore one that catches an array packed backwards where plain
distinctness would not.

Usage: python3 tools/check/spark_check.py [--rom <rom.z64>]
"""
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_effectexport as S     # noqa: E402
import flipbook_check as F       # noqa: E402

# How far past the animation to replay it. The spark's script runs seven
# tics; sixteen reaches the End command and the words after it.
FRAMES = 16

# The palette the display list loads, and the only one: a gsDPSetTextureImage
# at 0x8FB8 followed by the gsDPLoadTLUT at 0x8FD8. The MObjSub selects no
# palette of its own -- its `palettes` pointer is NULL -- so all seven
# frames are drawn through this one bank.
TLUT_TIMG = 0x8FBC


def texels(f, reloc, sub, k, w, h, picture):
    sp = sub["sprites"][k]
    tlut = struct.unpack_from(">16H", f, reloc[TLUT_TIMG])
    # The ROM sprite is CI4, so every byte is two indices, and the colours
    # it can draw are the TLUT entries those indices pick. Swizzling
    # permutes texels and cannot change that set.
    idx = set()
    for byte in f[sp:sp + (w * h) // 2]:
        idx.add(byte >> 4)
        idx.add(byte & 0xF)
    want = len({tlut[i] for i in idx})
    got = len(set(struct.unpack("<%dH" % (len(picture) // 2), picture)))
    if want != got:
        return ["frame %d draws %d colours and sprite 0x%04X, which the ROM "
                "has it at, has %d -- the array is packed out of order"
                % (k, got, sp, want)]
    return []


SPARK = F.Flipbook(
    "spark",
    ("llEFCommonEffects1FileID",
     "llEFCommonEffects1CommonSparkMObjSub",
     "llEFCommonEffects1CommonSparkDObjDesc",
     "llEFCommonEffects1CommonSparkAnimJoint",
     "llEFCommonEffects1CommonSparkMatAnimJoint"),
    dl_is_links=False, pack=S.pack_spark, frames=FRAMES, texels=texels)


if __name__ == "__main__":
    F.main(SPARK, "the sparks' MObj, its sprite array and its MatAnimJoint "
                  "are the ROM's")
