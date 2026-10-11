#!/usr/bin/env python3
"""ssb64-dc: the N64's reverb as an AICA DSP program, made at build time.

SSB boots with libultra's AL_FX_CUSTOM effect, the parameter list
dSYAudioCustomFXParams (ssb-decomp-re/src/sys/audio.c): fourteen sections
working in place on one 19,200-sample delay line at 32,006 Hz.
The N64's algorithm runs sample by sample; this
script builds the same network for the AICA's DSP at 44,100 Hz and writes
it as a C header for src/dc/bgmarm.c to upload. It also
writes libultra's equal-power table (n_eqpower, libultra/n_audio/n_env.c),
which the per-voice sends in src/dc/aica/seqsyn.c are computed from, and
the thresholds that turn a Q15 level into the AICA's 3 dB send steps.
Neither table is copied into this repository: both are read from the
decomp tree in the image, as tools/export/ssb_trigexport.py does.

What changes on the way to the AICA:

  rate     every tap is scaled by 44100/32006 and rounded; taps that are
           the same position on the N64 stay the same position here. The
           all-pass and comb coefficients are kept, and the one low-pass
           pole is moved so its time constant (and so its 2.4 kHz corner)
           stays: c' = c ** (32006/44100).

  in place The N64 runs section after section over the shared line, every
           read seeing every earlier write. In SSB's table two sections
           only ever touch the same position when it is the same tap (one
           section's output is the next one's input), so here that value
           is passed in the DSP's TEMP registers instead of through the
           line, and each position is read once and written once a sample
           -- 19 reads and 20 writes, within the DSP's 64 memory steps.

  maths    each value written is a short sum of products, one product a
           step into the accumulator. The DSP's coefficients are 13 bits,
           -1..+4095/4096, so every sum is computed at half scale and
           written with SHIFT=1 (x2, saturated to 24 bits): unity terms
           are then exact. The line holds the AICA's 16-bit packed float
           (the packed float is always used), so its silence is
           0x6000, not 0.

  sends    the N64 mixes each voice's wet signal into a stereo aux bus
           after its pan, and the reverb takes 0.707 (L + R) as its mono
           input. An AICA channel's DSP send is mono and before pan, so the
           input coefficient is sqrt(2): a centred voice whose send equals
           its dry level then reaches the reverb at the N64's ratio to its
           dry signal (seqsyn.c scales the send by the voice's pan).

  memory   MADRS: aicaflow
           (github.com/dfchil/aicaflow, documented in driver/sh4/include/
           aicaflow/dsp.h) reports hardware reading MADRS[MASA << 1]. Only
           MASA values none of which is twice another are used, and each
           tap is written at both MADRS[m] and MADRS[2m], so the program is
           right either way.

tools/check/reverb_check.py runs the program through a model of the DSP
and holds it to the network, and the network to the N64's at 32,006 Hz.

Usage:
  python3 tools/export/ssb_reverbexport.py --decomp <ssb-decomp-re> \\
      --out-dir <dir>         (writes aica_reverb.h and aica_sends.h)
"""
import argparse
import math
import os
import re
import sys

N64_RATE = 32006
AICA_RATE = 44100
STEPS = 128
RING_RBL = 2                    # 0x2804 RBL: 8192 << 2 = 32768 words
RING_WORDS = 8192 << RING_RBL
INPUT_GAIN = math.sqrt(2.0)
TEMP_STATE = 120                # the low-pass state: written here, read
                                # at TEMP_STATE + 1 the next sample

# MASA values none of which is twice another (see "memory" above)
MASA_OK = [0] + list(range(1, 32, 2)) + [4, 12, 20, 28, 16]


# --- the decomp's tables ---------------------------------------------------

def c_array(text, name):
    m = re.search(r"\b%s\s*\[[^\]]*\]\s*=\s*\{(.*?)\};" % re.escape(name),
                  text, re.S)
    if not m:
        sys.exit("ssb_reverbexport: %s not found" % name)
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    body = re.sub(r"//[^\n]*", "", body)
    return [int(v, 0) for v in body.replace("\n", " ").split(",")
            if v.strip()]


def read_decomp(decomp):
    with open(os.path.join(decomp, "src/sys/audio.c")) as f:
        params = c_array(f.read(), "dSYAudioCustomFXParams")
    with open(os.path.join(decomp, "src/libultra/n_audio/n_env.c")) as f:
        eqpower = c_array(f.read(), "n_eqpower")
    return params, eqpower


def sections(params):
    """[(input, output, fb, ff, gain, lp)] in samples at 32 kHz, Q15."""
    count, length = params[0], params[1]
    out = []
    for s in range(count):
        p = params[2 + 8 * s: 10 + 8 * s]
        inp, outp, fb, ff, gain, crate, cdepth, lp = p
        if crate or cdepth:
            sys.exit("ssb_reverbexport: section %d has chorus; the DSP "
                     "program does not model it" % s)
        out.append((inp, outp, fb, ff, gain, lp))
    return length, out


def scale_tap(t):
    return int(round(t * AICA_RATE / N64_RATE))


# --- the network as sums of products -----------------------------------------
#
# A value is ('mix',) for the voices' send bus, ('mem', tap) for the line
# as it was before this sample's writes, ('temp', name) for a value made
# earlier this sample, or ('state',) for the low-pass's last output.
# A chain is (name, [(coef, value)], {'mem': tap, 'temp': bool, 'ef': bool}).

def network(params):
    """The chains, in an order that respects their dependencies, with the
    N64's own coefficients (as floats) and taps (32 kHz samples)."""
    length, secs = sections(params)
    q15 = lambda v: v / 32768.0
    chains = []
    # what each position holds after this sample, and who wrote it last
    final = {}
    value_at = {}           # position -> the value a later section reads

    def src(tap):
        return value_at.get(tap, ('mem', tap))

    out_terms = []
    a0 = None
    for s, (inp, outp, fb, ff, gain, lp) in enumerate(secs):
        fb, ff, gain = q15(fb), q15(ff), q15(gain)
        if s == 0 and inp == 0:
            A = [(1.0, ('mix',))]        # the line's newest sample is the input
        else:
            A = [(1.0, src(inp))]
        B = [(1.0, src(outp))]
        if lp:
            # the comb with a filtered write-back: A' = A + fb*B into the
            # input tap, lowpass(B) into the output tap
            if ff:
                sys.exit("ssb_reverbexport: low-pass section with ff")
            chains.append(("A%d" % s, A + [(fb * c, v) for c, v in B],
                           {}))
            final[inp] = "A%d" % s
            chains.append(("LP%d" % s, [(1.0, v) for c, v in B],
                           {'lp': lp}))
            final[outp] = "LP%d" % s
            continue
        # B' = B + ff*A, A' = A + fb*B' = (1 + fb*ff) A + fb B
        bname, aname = "B%d" % s, "A%d" % s
        chains.append((bname, B + [(ff * c, v) for c, v in A], {}))
        if fb:
            chains.append((aname, [((1.0 + fb * ff) * c, v) for c, v in A] +
                           [(fb * c, v) for c, v in B], {}))
            value_at[inp] = ('temp', aname)
            final[inp] = aname
        value_at[outp] = ('temp', bname)
        final[outp] = bname
        if gain:
            out_terms.append((gain, ('temp', bname)))
    chains.append(("OUT", out_terms, {'ef': True}))
    return length, chains, final


def to_dsp(length, chains, final):
    """Rescale to 44.1 kHz, fold the input gain in, resolve the low-pass
    and decide each chain's outputs. Returns chains with 44.1 kHz taps and
    the 13-bit coefficients the DSP will use (as halved integers)."""
    users = {}
    for name, terms, _ in chains:
        for _, v in terms:
            if v[0] == 'temp':
                users.setdefault(v[1], []).append(name)
    writers = {name: tap for tap, name in final.items()}
    if len(writers) != len(final):
        sys.exit("ssb_reverbexport: one value is the last write of two taps")
    out = []
    for name, terms, extra in chains:
        nt = []
        lp = extra.get('lp')
        if lp:
            c = (lp * 16384 >> 15) / 16384.0          # the N64's Q14 pole
            c = c ** (N64_RATE / AICA_RATE)
            (_, x), = terms
            nt = [(1.0 - c, x), (c, ('state',))]
        else:
            nt = list(terms)
        q = []
        for coef, v in nt:
            if v[0] == 'mix':
                coef *= INPUT_GAIN
            elif v[0] == 'mem':
                v = ('mem', scale_tap(v[1]))
            half = int(round(coef * 4096 / 2))
            half = max(-4096, min(4095, half))
            q.append((half, v))
        dest = {}
        if name in writers:
            dest['mem'] = scale_tap(writers[name])
        if name in users:
            dest['temp'] = True
        if lp:
            dest['state'] = True
        if extra.get('ef'):
            dest['ef'] = True
        out.append((name, q, dest))
    return scale_tap(length), out


# --- scheduling onto the 128 steps -----------------------------------------

class Step:
    def __init__(self):
        self.tra = 0; self.twt = 0; self.twa = 0
        self.xsel = 0; self.ysel = 0; self.ira = 0; self.iwt = 0; self.iwa = 0
        self.mwt = 0; self.mrd = 0; self.ewt = 0; self.ewa = 0
        self.shift = 1; self.negb = 0; self.zero = 0; self.bsel = 0
        self.masa = 0
        self.coef = 0
        self.used = False           # a product is accumulated here
        self.mem = False            # MRD or MWT here

    def words(self):
        w0 = (self.tra << 9) | (self.twt << 8) | (self.twa << 1)
        w1 = ((self.xsel << 15) | (self.ysel << 13) | (self.ira << 7) |
              (self.iwt << 6) | (self.iwa << 1))
        w2 = ((self.mwt << 14) | (self.mrd << 13) | (self.ewt << 12) |
              (self.ewa << 8) | (self.shift << 4) | (self.negb << 2) |
              (self.zero << 1) | self.bsel)
        w3 = self.masa << 9
        return [w0, w1, w2, w3]


def schedule(chains):
    steps = [Step() for _ in range(STEPS)]
    taps = []                       # MASA index -> tap
    masa_of = {}
    mems_of = {}                    # tap -> MEMS register
    mems_ready = {}                 # tap -> first step it can be an X
    temp_of = {}                    # chain name -> TEMP address
    temp_ready = {}

    def masa(tap):
        if tap not in masa_of:
            if len(taps) == len(MASA_OK):
                sys.exit("ssb_reverbexport: more than %d taps" % len(MASA_OK))
            masa_of[tap] = MASA_OK[len(taps)]
            taps.append(tap)
        return masa_of[tap]

    def free_odd(lo, hi):
        """the earliest odd step in [lo, hi] with no memory access"""
        for r in range(lo | 1, hi + 1, 2):
            if r < STEPS and not steps[r].mem:
                return r
        return None

    def read(tap, by):
        """issue the read of `tap` so it is an X operand at step `by`"""
        if tap in mems_ready:
            return mems_ready[tap] <= by
        r = free_odd(1, by - 3)
        while r is not None and steps[r + 2].iwt:
            r = free_odd(r + 2, by - 3)
        if r is None:
            return False
        steps[r].mrd = 1
        steps[r].mem = True
        steps[r].masa = masa(tap)
        reg = len(mems_of)
        if reg >= 32:
            sys.exit("ssb_reverbexport: more than 32 reads")
        mems_of[tap] = reg
        steps[r + 2].iwt = 1
        steps[r + 2].iwa = reg
        mems_ready[tap] = r + 3
        return True

    def ready_at(v):
        if v[0] == 'temp':
            return temp_ready[v[1]]
        return 0

    cur = 0
    next_temp = 0
    for name, terms, dest in chains:
        # terms whose operands come ready latest go last
        terms = sorted(terms, key=lambda t: ready_at(t[1]))
        n = len(terms)
        s = cur
        while True:
            if s + n >= STEPS:
                sys.exit("ssb_reverbexport: the program does not fit "
                         "(chain %s at step %d)" % (name, s))
            ok = all(ready_at(v) <= s + k for k, (_, v) in enumerate(terms))
            w = s + n                   # the step the result is written
            if ok and 'mem' in dest:
                ok = (w & 1) == 1 and not steps[w].mem
            if ok:
                # reads must not take the write's slot: reserve it first
                if 'mem' in dest:
                    steps[w].mem = True
                for k, (_, v) in enumerate(terms):
                    if v[0] == 'mem' and not read(v[1], s + k):
                        ok = False
                        break
                if 'mem' in dest:
                    steps[w].mem = False
            if ok:
                break
            s += 1
        for k, (coef, v) in enumerate(terms):
            st = steps[s + k]
            st.used = True
            st.ysel = 1
            st.coef = coef
            if k == 0:
                st.zero = 1
            else:
                st.bsel = 1
            if v[0] == 'mix':
                st.xsel, st.ira = 1, 32
            elif v[0] == 'mem':
                st.xsel, st.ira = 1, mems_of[v[1]]
            elif v[0] == 'temp':
                st.tra = temp_of[v[1]]
            elif v[0] == 'state':
                st.tra = TEMP_STATE + 1
        w = s + n
        st = steps[w]
        if 'mem' in dest:
            st.mwt = 1
            st.mem = True
            st.masa = masa(dest['mem'])
        if 'temp' in dest:
            temp_of[name] = next_temp
            next_temp += 1
            if next_temp >= TEMP_STATE:
                sys.exit("ssb_reverbexport: out of TEMP")
            st.twt, st.twa = 1, temp_of[name]
            temp_ready[name] = w + 1
        if 'state' in dest:
            if st.twt:
                sys.exit("ssb_reverbexport: two TEMP writes at step %d" % w)
            st.twt, st.twa = 1, TEMP_STATE
        if 'ef' in dest:
            st.ewt, st.ewa = 1, 0
        cur = w
    return steps, taps, cur + 1


def build(params):
    length, chains, final = network(params)
    length, chains = to_dsp(length, chains, final)
    steps, taps, used = schedule(chains)
    if max(taps) >= RING_WORDS:
        sys.exit("ssb_reverbexport: tap %d past the ring" % max(taps))
    mpro = []
    coef = []
    for st in steps:
        mpro += st.words()
        coef.append((st.coef << 3) & 0xFFFF)
    madrs = [0] * 64
    for i, tap in enumerate(taps):
        m = MASA_OK[i]
        madrs[m] = tap
        madrs[2 * m] = tap
    return {
        'mpro': mpro, 'coef': coef, 'madrs': madrs, 'taps': taps,
        'chains': chains, 'steps_used': used, 'length': length,
    }


# --- output ------------------------------------------------------------------

def words(vals, per, fmt):
    lines = []
    for i in range(0, len(vals), per):
        lines.append("    " + ", ".join(fmt % v for v in vals[i:i + per]) +
                     ",")
    return "\n".join(lines)


def reverb_header(prog):
    reads = sum(1 for i in range(STEPS) if prog['mpro'][4 * i + 2] & 0x2000)
    writes = sum(1 for i in range(STEPS) if prog['mpro'][4 * i + 2] & 0x4000)
    return """/* aica_reverb.h -- written by tools/export/ssb_reverbexport.py from
 * ssb-decomp-re's dSYAudioCustomFXParams; do not edit. The N64's reverb as
 * an AICA DSP program at 44.1 kHz: %d of 128 steps, %d line reads and %d
 * writes, taps up to %d samples in a ring of %d words. */
#ifndef AICA_REVERB_H
#define AICA_REVERB_H

#include <stdint.h>

#define AICA_REVERB_RBL %d             /* register 0x2804 bits 13-14 */
#define AICA_REVERB_RING_WORDS %d
#define AICA_REVERB_SILENCE 0x6000u    /* packed-float zero */

/* register 0x3400 on, one u16 in each 32-bit slot */
static const uint16_t kAicaReverbMpro[512] = {
%s
};

/* register 0x3000 on */
static const uint16_t kAicaReverbCoef[128] = {
%s
};

/* register 0x3200 on */
static const uint16_t kAicaReverbMadrs[64] = {
%s
};

#endif /* AICA_REVERB_H */
""" % (prog['steps_used'], reads, writes, max(prog['taps']), RING_WORDS,
       RING_RBL, RING_WORDS,
       words(prog['mpro'], 8, "0x%04x"), words(prog['coef'], 8, "0x%04x"),
       words(prog['madrs'], 8, "%5d"))


def send_steps():
    """kAicaSendStep[i]: the least Q15 level that takes send step i. The
    AICA's DISDL and IMXL are 3 dB a step, 15 = 0 dB, 0 = off; a level
    takes the nearest step on a log scale."""
    return [0x7FFFFFFF] + [int(round(32768 * 2 ** (-(15 - i) / 2.0 - 0.25)))
                           for i in range(1, 16)]


def sends_header(eqpower):
    return """/* aica_sends.h -- written by tools/export/ssb_reverbexport.py; do not
 * edit. kAicaEqPower is ssb-decomp-re's libultra n_eqpower (n_audio/
 * n_env.c): the equal-power law the N64 splits each voice's dry and wet
 * levels and its pan by (Q15).
 * kAicaSendStep[i] is the least Q15 level the AICA's 3 dB send step i
 * (DISDL, IMXL: 15 = 0 dB, 0 = off) stands for; [0] is never taken. */
#ifndef AICA_SENDS_H
#define AICA_SENDS_H

#define AICA_EQPOWER_LENGTH %d

static const short kAicaEqPower[AICA_EQPOWER_LENGTH] = {
%s
};

static const int kAicaSendStep[16] = {
%s
};

#endif /* AICA_SENDS_H */
""" % (len(eqpower), words(eqpower, 8, "%6d"), words(send_steps(), 4, "%10d"))


def write_all(decomp, out_dir):
    params, eqpower = read_decomp(decomp)
    prog = build(params)
    for name, text in (("aica_reverb.h", reverb_header(prog)),
                       ("aica_sends.h", sends_header(eqpower))):
        path = os.path.join(out_dir, name)
        tmp = path + ".tmp"
        with open(tmp, "w") as f:
            f.write(text)
        os.replace(tmp, path)
    return prog


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--decomp", default=os.environ.get(
        "SSB_DECOMP_DIR", "/opt/ssb-decomp-re"))
    ap.add_argument("--out-dir", required=True)
    a = ap.parse_args()
    prog = write_all(a.decomp, a.out_dir)
    print("ssb_reverbexport: %d steps, taps %s" %
          (prog['steps_used'], prog['taps']))


if __name__ == "__main__":
    main()
