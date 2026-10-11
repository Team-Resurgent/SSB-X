#!/usr/bin/env python3
"""ssb64-dc: hold the AICA reverb program to the N64's reverb.

tools/export/ssb_reverbexport.py turns SSB's AL_FX_CUSTOM parameters into
an AICA DSP program. Three models, and two comparisons:

  dsp    the program itself, run step by step the way the AICA's DSP runs
         it: a multiply-accumulate a step with a one-step delay before the
         shifter, 13-bit coefficients, TEMP and the ring addressed through
         a counter that falls by one each sample, reads landing in MEMS two
         steps after MRD, and the ring holding 16-bit packed floats.

  net44  the N64 reverb's per-sample algorithm, in
         floating point, with the taps and the low-pass pole moved to
         44,100 Hz as the exporter moves them: what the program means.

  n64    the same pseudo-code in the N64's own arithmetic at 32,006 Hz:
         s16 saturation, Q15 products, the Q14 low-pass. The RSP's
         rounding of its products is not in the decomp (spec 12); this
         rounds to nearest. Rounding down instead leaves a DC offset of
         about -16 circulating in the feedback for good, which is the
         model's, not the N64's.

  dsp vs net44   sample by sample, on an impulse and a noise burst, after
                 the line's longest tap and two trips round the comb.
                 Gate: signal-to-error ratio (the error is the packed
                 float's 12 bits and the 13-bit coefficients).
  net44 vs n64   the energy decay curve of each response (the energy
                 still to come after each 10 ms point, unnormalized, so
                 the level counts too) over 1.2 s: the rescaled network
                 decays as the N64's does. Gate: every point down to
                 EDC_FLOOR_DB below the total within MAX_EDC_DB. The input
                 is one continuous pulse, band-limited to 12 kHz (below
                 the N64's Nyquist), sampled at each rate, and energies are
                 per second: a one-sample impulse would carry 6 kHz more
                 bandwidth at 44.1 kHz, and the low-pass would take a
                 larger share of it on every trip round the comb.

Usage:  python3 tools/check/reverb_check.py [--decomp <ssb-decomp-re>]
"""
import argparse
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "export"))
import ssb_reverbexport as RX  # noqa: E402

MIN_SER_DB = 40.0
MAX_EDC_DB = 1.0
EDC_FLOOR_DB = -40.0


# --- the DSP -------------------------------------------------------------

def pack(v):
    """24-bit signed -> the AICA's 16-bit float: sign, a 4-bit count of
    redundant sign bits (12 = none left), 11 mantissa bits."""
    s = (v >> 23) & 1
    t = (v ^ (v << 1)) & 0xFFFFFF
    e = 0
    while e < 12 and not (t & 0x800000):
        t <<= 1
        e += 1
    v = (v << (e if e < 12 else 11)) & 0xFFFFFF
    return (s << 15) | (e << 11) | ((v >> 11) & 0x7FF)


def unpack(w):
    s = (w >> 15) & 1
    e = (w >> 11) & 0xF
    u = ((w & 0x7FF) << 11) | (s << 22)
    if e > 11:
        e = 11
    else:
        u ^= 1 << 22            # the implicit bit is the sign's opposite
    u |= s << 23
    if u & 0x800000:
        u -= 1 << 24
    return u >> e


def sat24(v):
    return max(-0x800000, min(0x7FFFFF, v))


def decode(prog):
    steps = []
    for i in range(RX.STEPS):
        w0, w1, w2, w3 = prog['mpro'][4 * i: 4 * i + 4]
        st = dict(
            tra=(w0 >> 9) & 0x7F, twt=(w0 >> 8) & 1, twa=(w0 >> 1) & 0x7F,
            xsel=(w1 >> 15) & 1, ysel=(w1 >> 13) & 3, ira=(w1 >> 7) & 0x3F,
            iwt=(w1 >> 6) & 1, iwa=(w1 >> 1) & 0x1F,
            table=(w2 >> 15) & 1, mwt=(w2 >> 14) & 1, mrd=(w2 >> 13) & 1,
            ewt=(w2 >> 12) & 1, ewa=(w2 >> 8) & 0xF, adrl=(w2 >> 7) & 1,
            frcl=(w2 >> 6) & 1, shift=(w2 >> 4) & 3, yrl=(w2 >> 3) & 1,
            negb=(w2 >> 2) & 1, zero=(w2 >> 1) & 1, bsel=w2 & 1,
            nofl=(w3 >> 15) & 1, masa=(w3 >> 9) & 0x3F,
            adreb=(w3 >> 8) & 1, nxadr=(w3 >> 7) & 1,
            y=((prog['coef'][i] ^ 0x8000) - 0x8000) >> 3)
        for k in ('table', 'adrl', 'frcl', 'yrl', 'adreb', 'nxadr'):
            if st[k]:
                sys.exit("reverb_check: step %d uses %s, which this model "
                         "does not" % (i, k.upper()))
        if (st['mrd'] or st['mwt']) and not (i & 1):
            sys.exit("reverb_check: memory access on even step %d" % i)
        steps.append(st)
    return steps


def run_dsp(prog, mix, madrs_index):
    """mix: the MIXS0 input per sample (s16 scale). Returns the 24-bit
    value each sample's EWT wrote (EFREG is its top 16 bits).
    madrs_index: lambda masa -> MADRS entry, to run both readings."""
    steps = decode(prog)
    live = [(i, st) for i, st in enumerate(steps)
            if st['ysel'] == 1 or st['iwt'] or st['mrd'] or st['mwt'] or
            st['twt'] or st['ewt']]
    madrs = prog['madrs']
    rbl = RX.RING_WORDS - 1
    ring = [0x6000] * RX.RING_WORDS         # packed-float silence
    temp = [0] * 128
    mems = [0] * 32
    memval = [0] * 4
    dec = RX.RING_WORDS
    out = []
    for x in mix:
        acc = 0
        ef = 0
        mixs = x << 4
        prev = -1
        for i, st in live:
            if i != prev + 1:
                acc = 0             # a gap: nothing reads its ACC
            prev = i
            ira = st['ira']
            if ira < 32:
                inputs = mems[ira]
            elif ira == 32:
                inputs = mixs
            else:
                inputs = 0
            if st['iwt']:
                mems[st['iwa']] = memval[i & 3]
            t = temp[(st['tra'] + dec) & 0x7F]
            if st['zero']:
                b = 0
            else:
                b = acc if st['bsel'] else t
                if st['negb']:
                    b = -b
            xv = inputs if st['xsel'] else t
            y = st['y'] if st['ysel'] == 1 else 0
            sh = st['shift']
            shifted = acc if sh in (0, 3) else acc << 1
            if sh < 2:
                shifted = sat24(shifted)
            acc = ((xv * y) >> 12) + b
            if st['twt']:
                temp[(st['twa'] + dec) & 0x7F] = shifted
            if st['mrd'] or st['mwt']:
                addr = (madrs[madrs_index(st['masa'])] + dec) & rbl
                if st['mrd']:
                    memval[(i + 2) & 3] = unpack(ring[addr])
                if st['mwt']:
                    ring[addr] = pack(shifted)
            if st['ewt']:
                ef = shifted
        out.append(ef)
        dec -= 1
        if dec == 0:
            dec = RX.RING_WORDS
    return out


# --- the network, per the spec -----------------------------------------------

def run_net44(params, m_in):
    """The N64 reverb in floating point at 44.1 kHz; m_in is the reverb's
    mono input per sample. Returns its output per sample."""
    length, secs = RX.sections(params)
    secs = [(RX.scale_tap(i), RX.scale_tap(o), fb / 32768.0, ff / 32768.0,
             g / 32768.0, lp) for i, o, fb, ff, g, lp in secs]
    size = RX.RING_WORDS
    ring = [0.0] * size
    lpstate = {}
    out = []
    n = 0
    for m in m_in:
        ring[n] = m
        o = 0.0
        for k, (ti, to, fb, ff, g, lp) in enumerate(secs):
            ia, ib = (n - ti) % size, (n - to) % size
            a, b = ring[ia], ring[ib]
            if ff:
                b = b + ff * a
            if fb:
                a = a + fb * b
                ring[ia] = a
            if lp:
                c = (lp * 16384 >> 15) / 16384.0
                c = c ** (RX.N64_RATE / RX.AICA_RATE)
                b = (1.0 - c) * b + c * lpstate.get(k, 0.0)
                lpstate[k] = b
            ring[ib] = b
            if g:
                o += g * b
        out.append(o)
        n = (n + 1) % size
    return out


def run_n64(params, aux):
    """the N64's arithmetic at 32,006 Hz; aux is auxL = auxR per sample."""
    length, secs = RX.sections(params)
    sat = lambda v: max(-32768, min(32767, v))
    mul = lambda x, c: (x * c + 0x4000) >> 15
    ring = [0] * length
    lpstate = {}
    out = []
    n = 0
    for x in aux:
        m = sat(x - mul(x, 9597))
        m = sat(m + mul(x, 23170))
        ring[n] = m
        o = 0
        for k, (ti, to, fb, ff, g, lp) in enumerate(secs):
            ia, ib = (n - ti) % length, (n - to) % length
            a, b = ring[ia], ring[ib]
            if ff:
                b = sat(b + mul(a, ff))
            if fb:
                a = sat(a + mul(b, fb))
                ring[ia] = a
            if lp:
                c = (lp * 16384) >> 15
                b = (b * (16384 - c) + lpstate.get(k, 0) * c + 8192) >> 14
                lpstate[k] = b
            ring[ib] = b
            if g:
                o = sat(o + mul(b, g))
        out.append(o)
        n = (n + 1) % length
    return out


# --- comparisons -------------------------------------------------------------

def ser_db(ref, got):
    num = sum(r * r for r in ref)
    den = sum((r - g) ** 2 for r, g in zip(ref, got))
    return 10 * math.log10(num / den) if den else float('inf')


def pulse(rate, n, peak, fc=12000.0, at_s=0.002, half_s=0.002):
    """a pulse band-limited to fc: a Hann-windowed sinc centred at at_s,
    the same continuous signal whatever the rate"""
    out = [0.0] * n
    for k in range(n):
        t = k / rate - at_s
        if abs(t) >= half_s:
            continue
        x = 2 * fc * t
        sinc = 1.0 if x == 0 else math.sin(math.pi * x) / (math.pi * x)
        out[k] = peak * sinc * 0.5 * (1 + math.cos(math.pi * t / half_s))
    return out


def edc(sig, rate, step_s=0.010, total_s=1.2):
    """the energy per second from each step_s point to the end"""
    tail = 0.0
    acc = [0.0] * (len(sig) + 1)
    for k in range(len(sig) - 1, -1, -1):
        tail += sig[k] * sig[k] / rate
        acc[k] = tail
    return [acc[min(len(sig), int(k * step_s * rate))]
            for k in range(int(total_s / step_s))]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--decomp", default=os.environ.get(
        "SSB_DECOMP_DIR", "/opt/ssb-decomp-re"))
    a = ap.parse_args()
    params, _ = RX.read_decomp(a.decomp)
    prog = RX.build(params)
    fails = 0

    # dsp vs net44: an impulse, then a noise burst, 0.8 s
    n = int(0.8 * RX.AICA_RATE)
    mix = [0] * n
    mix[10] = 20000
    seed = 12345
    for k in range(4000, 4400):
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        mix[k] = ((seed >> 8) % 16001) - 8000
    want = run_net44(params, [RX.INPUT_GAIN * (x << 4) for x in mix])
    for label, idx in (("MADRS[MASA]", lambda m: m),
                       ("MADRS[MASA << 1]", lambda m: 2 * m)):
        got = run_dsp(prog, mix, idx)
        ser = ser_db(want, got)
        ok = ser >= MIN_SER_DB
        fails += not ok
        print("reverb_check: program vs network (%s): %.1f dB "
              "signal-to-error over %d samples%s" %
              (label, ser, n, "" if ok else "  FAIL (< %.0f)" % MIN_SER_DB))

    # net44 vs n64: energy decay curves of a band-limited pulse, 1.2 s
    # (both run 1.6 s so the last points have their tails)
    imp44 = [RX.INPUT_GAIN * v for v in
             pulse(RX.AICA_RATE, int(1.6 * RX.AICA_RATE), 8000.0)]
    imp64 = [int(round(v)) for v in
             pulse(RX.N64_RATE, int(1.6 * RX.N64_RATE), 8000.0)]
    e44 = edc(run_net44(params, imp44), RX.AICA_RATE)
    e64 = edc(run_n64(params, imp64), RX.N64_RATE)
    total = e64[0]
    worst, worst_at, counted = 0.0, 0, 0
    for k, (x, y) in enumerate(zip(e44, e64)):
        if y <= 0 or 10 * math.log10(y / total) < EDC_FLOOR_DB:
            break
        counted += 1
        d = abs(10 * math.log10(max(x, 1e-12) / y))
        if d > worst:
            worst, worst_at = d, k
    ok = worst <= MAX_EDC_DB and counted > 20
    fails += not ok
    print("reverb_check: network at 44.1 kHz vs the N64's at 32 kHz: energy "
          "decay within %.2f dB (worst at %d ms) for %d ms, down to %.0f dB%s"
          % (worst, worst_at * 10, counted * 10, EDC_FLOOR_DB,
             "" if ok else "  FAIL (> %.1f dB)" % MAX_EDC_DB))
    print("reverb_check: %d steps of 128, taps %s" %
          (prog['steps_used'], prog['taps']))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
