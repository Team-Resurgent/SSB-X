#!/usr/bin/env python3
"""Hold the Xbox port's texture and ADPCM decoders to the port's own references.

The renderer turns PowerVR texture memory into NV2A textures with
Platform/xbox/pvr_texdecode.h, and the software AICA decodes Yamaha ADPCM with
Platform/xbox/aica/aica_decode.h. This builds both into a host program
(test_decode.c, with the host C compiler) and compares, byte for byte:

  * textures -- against tools/lib/pvrsoft.py's decode_texture, the port's
    software PVR: ARGB1555 / RGB565 / ARGB4444, 4 and 8 bpp paletted, VQ,
    twiddled and not, square and both rectangular shapes;
  * ADPCM -- against tools/lib/ssb_adpcm.py's decode, the model of the chip
    the port's encoder is written to, on signals encoded by that encoder.

    python tools/xbox/hosttest/run_decode_tests.py [--cc gcc]
"""
import argparse
import os
import random
import shutil
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path[:0] = [os.path.join(ROOT, d) for d in ("lib",)]
import pvrsoft    # noqa: E402
import ssb_adpcm  # noqa: E402


def find_cc(want):
    for c in ([want] if want else []) + ["gcc", r"C:\mingw32\bin\gcc.exe", r"C:\msys64\mingw64\bin\gcc.exe",
                                         "clang"]:
        p = shutil.which(c) or (c if os.path.isfile(c) else None)
        if p:
            return p
    sys.exit("no host C compiler (pass --cc)")


def build(cc, tmp):
    exe = os.path.join(tmp, "test_decode.exe")
    r = subprocess.run([cc, "-O2", "-std=gnu99", "-o", exe, os.path.join(HERE, "test_decode.c")],
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("build failed:\n" + r.stdout + r.stderr)
    return exe


def run(exe, tmp, args, blob):
    inp, out = os.path.join(tmp, "in.bin"), os.path.join(tmp, "out.bin")
    with open(inp, "wb") as f:
        f.write(blob)
    r = subprocess.run([exe, args[0], inp, out] + [str(a) for a in args[1:]])
    if r.returncode != 0:
        return None
    return open(out, "rb").read()


def size_code(v):
    return {8: 0, 16: 1, 32: 2, 64: 3, 128: 4, 256: 5, 512: 6, 1024: 7}[v]


def tex_case(exe, tmp, w, h, fmt, vq=False, twid=True, rng=None, bank=0):
    m3 = (fmt << 27) | ((0 if twid else 1) << 26) | ((1 if vq else 0) << 30)
    # the palette bank (4 bpp: bits 21-26, 8 bpp: 25-26) for the decoder
    # under test; the reference is given bank 0, because pvrsoft reads bit
    # 26 as "not twiddled" for paletted textures too, and the indices a
    # texture decodes to do not depend on its bank
    m3_mine = m3 | ((bank << 21) if fmt == 5 else (bank << 25) if fmt == 6 else 0)
    m2 = (size_code(w) << 3) | size_code(h)
    if vq:
        nbytes = 2048 + w * h // 4
    elif fmt == 5:
        nbytes = w * h // 2
    elif fmt == 6:
        nbytes = w * h
    else:
        nbytes = w * h * 2
    blob = bytes(rng.getrandbits(8) for _ in range(nbytes))
    got = run(exe, tmp, ["tex", w, h, m3_mine, 0], blob)
    if got is None:
        return "decoder refused"
    s = pvrsoft.Stream()
    s.vram = {0: blob}
    s.pal = np.arange(1024, dtype=np.uint32)        # entry i = i: indices survive
    s.palfmt = 0
    ref = pvrsoft.decode_texture(s, m2, m3)         # (h, w, 4)
    if fmt in (5, 6):
        idx = np.frombuffer(got, np.uint8).reshape(h, w).astype(np.uint32)
        mine = pvrsoft._rgba16(idx, 0)              # through the same palette
    else:
        mine = pvrsoft._rgba16(np.frombuffer(got, "<u2").reshape(h, w), fmt)
    if mine.shape != ref.shape:
        return "shape %s vs %s" % (mine.shape, ref.shape)
    bad = np.argwhere((mine != ref).any(-1))
    if len(bad):
        y, x = bad[0]
        return "%d texels differ, first at (%d, %d)" % (len(bad), x, y)
    return None


def adpcm_case(exe, tmp, pcm):
    if len(pcm) & 1:
        pcm = pcm + [0]
    enc = ssb_adpcm.encode(pcm)
    ref = ssb_adpcm.decode(enc, len(pcm))
    got = run(exe, tmp, ["adpcm", len(pcm)], enc)
    if got is None:
        return "decoder failed"
    mine = list(np.frombuffer(got, "<i2"))
    for i, (a, b) in enumerate(zip(mine, ref)):
        if a != b:
            return "sample %d: %d vs %d" % (i, a, b)
    return None


def reverb_case(exe, tmp, sig):
    """The mixer's network against reverb_check.run_net44, the port's float
    model of the reverb at 44.1 kHz. The network carries the DSP program's
    sqrt(2) input gain (ssb_reverbexport INPUT_GAIN); run_net44 takes the
    reverb's input after it."""
    sys.path[:0] = [os.path.join(ROOT, "check"), os.path.join(ROOT, "export")]
    import reverb_check as RC
    import ssb_reverbexport as RX
    decomp = os.environ.get("SSB_DECOMP_DIR", os.path.join(os.path.dirname(ROOT), "extern",
                                                           "ssb-decomp-re"))
    params, _ = RX.read_decomp(decomp)
    ref = RC.run_net44(params, [v * RX.INPUT_GAIN for v in sig])
    got = run(exe, tmp, ["reverb"], np.asarray(sig, dtype="<f4").tobytes())
    if got is None:
        return "reverb run failed"
    mine = np.frombuffer(got, "<f4").astype(np.float64)
    ref = np.asarray(ref)
    err = np.max(np.abs(mine - ref)) / max(1e-9, np.max(np.abs(ref)))
    if err > 1e-4:
        i = int(np.argmax(np.abs(mine - ref)))
        return "relative error %.2e (sample %d: %.6f vs %.6f)" % (err, i, mine[i], ref[i])
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc")
    a = ap.parse_args()
    cc = find_cc(a.cc)
    rng = random.Random(64)
    fails = 0
    total = 0
    with tempfile.TemporaryDirectory() as tmp:
        exe = build(cc, tmp)
        shapes = [(8, 8), (16, 8), (8, 16), (32, 64), (128, 32), (64, 64), (256, 64), (16, 512)]
        for fmt, name in ((0, "ARGB1555"), (1, "RGB565"), (2, "ARGB4444")):
            for twid in (True, False):
                for w, h in shapes:
                    total += 1
                    err = tex_case(exe, tmp, w, h, fmt, twid=twid, rng=rng)
                    if err:
                        fails += 1
                        print("FAIL tex %s %s %dx%d: %s" % (name, "twiddled" if twid else "linear", w, h, err))
        # paletted: always twiddled on the PVR; every bank, including the
        # ones whose number sets bit 26
        for fmt, name, banks in ((5, "PAL4", (0, 1, 31, 32, 33, 63)), (6, "PAL8", (0, 1, 2, 3))):
            for bank in banks:
                for w, h in shapes:
                    total += 1
                    err = tex_case(exe, tmp, w, h, fmt, rng=rng, bank=bank)
                    if err:
                        fails += 1
                        print("FAIL tex %s bank %d %dx%d: %s" % (name, bank, w, h, err))
        for w, h in [(16, 16), (32, 16), (16, 32), (64, 64), (128, 64), (256, 256)]:
            total += 1
            err = tex_case(exe, tmp, w, h, 0, vq=True, rng=rng)
            if err:
                fails += 1
                print("FAIL tex VQ ARGB1555 %dx%d: %s" % (w, h, err))
        signals = [
            [int(30000 * np.sin(i * 0.05)) for i in range(4000)],
            [int(32767 * np.sin(i * 0.9)) for i in range(4000)],            # near full scale, fast
            [rng.randint(-32768, 32767) for _ in range(4000)],               # noise: clamps
            [0] * 500 + [32767] * 50 + [-32768] * 50 + [0] * 500,           # steps
            [int(8000 * np.sin(i * 0.002) * np.sin(i * 0.3)) for i in range(20000)],
        ]
        for k, sig in enumerate(signals):
            total += 1
            err = adpcm_case(exe, tmp, sig)
            if err:
                fails += 1
                print("FAIL adpcm signal %d: %s" % (k, err))
        rsig = [1000.0] + [0.0] * 30000                                     # an impulse
        nrng = np.random.default_rng(7)
        rsig2 = list(nrng.uniform(-8000, 8000, 4410)) + [0.0] * 26000         # a noise burst
        for k, sig in enumerate((rsig, rsig2)):
            total += 1
            err = reverb_case(exe, tmp, sig)
            if err:
                fails += 1
                print("FAIL reverb signal %d: %s" % (k, err))
    print("%d of %d decode tests passed" % (total - fails, total))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
