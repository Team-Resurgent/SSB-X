"""The load census's disc layout optimiser
(see loadcensus.py). `loadcensus.py layout` is its face; this is the work.

Input: the order every census run asked the drive for files in (the
trips: sources disc, models.bnd and the loader thread -- a file served
from the hold list or the read-ahead pool never reaches the drive), and
the size of every file in the staged ISO tree.

Cost of a layout: every run's trips replayed against it. A trip starting
where the one before ended still costs the drive a stream start; one
further away costs more, by how far -- the table below, measured from a
real console's -DDB_DISC_TRACE log (~/disc-trace2.log, 2026-09-29: the
first data after each stream start, median over 527 starts):

    gap 0 sectors              42 ms   (449 starts)
    gap < 2048 (4 MB)          64 ms   (37)
    gap < 20000 (40 MB)       127 ms   (37)
    further                   300 ms   (4; up to 520)

The transfer itself (bytes / 600 KB/s) does not depend on the layout and
is reported, not optimised. Neither does the fixed 42 ms: only fewer
trips (bundling files that are always read together) would remove that,
and `bundles` below lists the candidates.

Search: Pettis-Hansen chain merging on the trip-to-trip transition
counts (a file placed right after the one most often read before it),
then simulated annealing over the chain order and single-file moves,
scored by the full replay. Files no run read keep their manifest order,
after the ones that were read.
"""
import collections
import math
import random

SECTOR = 2048
RATE = 600.0 * 1024          # bytes per second, calibrate's fit

GAP_COST = ((0, 42.0), (2048, 64.0), (20000, 127.0))
FAR_COST = 300.0


def trip_ms(gap):
    if gap == 0:
        return GAP_COST[0][1]
    for lim, ms in GAP_COST[1:]:
        if gap < lim:
            return ms
    return FAR_COST


def sectors(nbytes):
    return max(1, (nbytes + SECTOR - 1) // SECTOR)


class Layout:
    """Files in order, each at a sector; the replay's cost."""

    def __init__(self, order, sizes):
        self.order = list(order)
        self.sizes = sizes
        self.place()

    def place(self):
        self.start, self.end = {}, {}
        at = 0
        for f in self.order:
            self.start[f] = at
            at += sectors(self.sizes.get(f, 0))
            self.end[f] = at

    def cost(self, seqs, weights):
        total = 0.0
        for seq, w in zip(seqs, weights):
            end = None
            c = 0.0
            for f in seq:
                s = self.start[f]
                c += trip_ms(abs(s - end) if end is not None else 0)
                end = self.end[f]
            total += w * c
        return total


def transitions(seqs, weights):
    t = collections.Counter()
    for seq, w in zip(seqs, weights):
        for a, b in zip(seq, seq[1:]):
            if a != b:
                t[(a, b)] += w
    return t


def chains(files, trans):
    """Pettis-Hansen: join a chain's tail to another's head along the
    heaviest transitions first."""
    head = {f: [f] for f in files}      # chain by its first file
    where = {f: f for f in files}       # file -> its chain's first file
    for (a, b), _w in sorted(trans.items(), key=lambda kv: -kv[1]):
        ca, cb = where.get(a), where.get(b)
        if ca is None or cb is None or ca == cb:
            continue
        if head[ca][-1] != a or cb != b:
            continue
        head[ca].extend(head[cb])
        for f in head[cb]:
            where[f] = ca
        del head[cb]
    return list(head.values())


def optimise(seqs, weights, sizes, manifest, iters=20000, seed=1):
    read = []
    seen = set()
    for seq in seqs:
        for f in seq:
            if f not in seen:
                seen.add(f)
                read.append(f)
    cold = [f for f in manifest if f not in seen and f in sizes]
    trans = transitions(seqs, weights)
    cs = chains(read, trans)
    # chains in the order their first file is first read
    first = {f: i for i, f in enumerate(read)}
    cs.sort(key=lambda c: min(first[f] for f in c))
    rng = random.Random(seed)

    def flat(cs):
        return [f for c in cs for f in c] + cold

    best = Layout(flat(cs), sizes)
    best_cost = best.cost(seqs, weights)
    cur, cur_cost = [list(c) for c in cs], best_cost
    temp0 = best_cost * 0.002
    for it in range(iters):
        temp = temp0 * (1.0 - it / iters) + 1e-9
        cand = [list(c) for c in cur]
        r = rng.random()
        if r < 0.4 and len(cand) > 1:
            # move one chain elsewhere
            c = cand.pop(rng.randrange(len(cand)))
            cand.insert(rng.randrange(len(cand) + 1), c)
        elif r < 0.7 and len(cand) > 1:
            i, j = rng.randrange(len(cand)), rng.randrange(len(cand))
            cand[i], cand[j] = cand[j], cand[i]
        else:
            # move one file into another chain, anywhere in it
            i = rng.randrange(len(cand))
            if not cand[i]:
                continue
            f = cand[i].pop(rng.randrange(len(cand[i])))
            j = rng.randrange(len(cand))
            cand[j].insert(rng.randrange(len(cand[j]) + 1), f)
            cand = [c for c in cand if c]
        lay = Layout(flat(cand), sizes)
        c = lay.cost(seqs, weights)
        if c < cur_cost or rng.random() < math.exp((cur_cost - c) / temp):
            cur, cur_cost = cand, c
            if c < best_cost:
                best, best_cost = lay, c
    return best, trans


def bundles(seqs, weights, trans, sizes, min_weight=3, max_bytes=512 * 1024):
    """Runs of small files the replay always reads back to back: each join
    in one saves a stream start (42 ms) per read of the run."""
    out = []
    for (a, b), w in trans.most_common():
        if w < min_weight:
            break
        if sizes.get(a, 0) + sizes.get(b, 0) > max_bytes:
            continue
        out.append((w, a, b))
    return out
