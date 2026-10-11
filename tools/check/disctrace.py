#!/usr/bin/env python3
"""Parse a -DDB_DISC_TRACE serial log (src/dc/disctrace.h) into records.

  disctrace.py LOG                 per-scene summary
  disctrace.py LOG --csv OUT.csv   every record, one row each

CSV columns: scene, t_us (unwrapped, since boot), dur_us, thread, op, file,
off, len, ret. `file` is the path for O/R/S/C, the marker for E, and empty
for the drive's own ops (K read sectors, Z stream start, Q stream request),
whose `off` is the sector as the drive is asked (ISO sector + 150) and `len`
the sector count (K, Z) or bytes (Q).

The 32-bit start times wrap every 71 minutes; each dump's H line carries the
64-bit clock at the dump, so a record is placed at the latest time at or
before that dump that matches its low 32 bits.
"""
import csv
import re
import sys
from collections import defaultdict

LINE = re.compile(r'dt: (.*)$')


def parse(path):
    files, threads, recs = {}, {}, []
    scene, now = None, 0
    for raw in open(path, errors='replace'):
        m = LINE.search(raw)
        if not m:
            continue
        f = m.group(1).split()
        if not f:
            continue
        k = f[0]
        if k == 'H':
            scene, now = f[1], int(f[2])
        elif k == 'F':
            files[int(f[1])] = ' '.join(f[2:])
        elif k == 'T':
            threads[int(f[1])] = ' '.join(f[2:])
        elif len(k) == 1 and len(f) == 8 and k in 'OCSRKZQE':
            t32, dur, tid, fid, off, ln, ret = (int(x) for x in f[1:])
            # latest absolute time <= now with these low 32 bits
            t = now - ((now - t32) & 0xFFFFFFFF)
            recs.append(dict(scene=scene, t=t, dur=dur, tid=tid, op=k,
                             fid=fid, off=off, len=ln, ret=ret))
    for r in recs:
        r['thread'] = threads.get(r['tid'], str(r['tid']))
        r['file'] = files.get(r['fid'], '') if r['op'] in 'OCSRE' else ''
    return recs


def summary(recs):
    by = defaultdict(list)
    order = []
    for r in recs:
        if r['scene'] not in by:
            order.append(r['scene'])
        by[r['scene']].append(r)
    print('%-18s %6s %8s %9s %8s %8s %10s' %
          ('scene', 'recs', 'reads', 'MB read', 'io ms', 'moves', 'sector span'))
    for s in order:
        rs = by[s]
        reads = [r for r in rs if r['op'] == 'R' and r['ret'] > 0]
        nbytes = sum(r['ret'] for r in reads)
        io_ms = sum(r['dur'] for r in rs if r['op'] in 'ORSCKZQ') / 1000.0
        # head moves: drive reads (K, Z) whose sector is not where the last
        # one ended
        drive = [r for r in rs if r['op'] in 'KZ']
        moves = span = 0
        end = None
        for r in drive:
            if end is not None and r['off'] != end:
                moves += 1
                span += abs(r['off'] - end)
            end = r['off'] + r['len']
        print('%-18s %6d %8d %9.2f %8.1f %8d %10d' %
              (s, len(rs), len(reads), nbytes / 1048576.0, io_ms, moves, span))


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    recs = parse(sys.argv[1])
    if '--csv' in sys.argv:
        out = sys.argv[sys.argv.index('--csv') + 1]
        with open(out, 'w', newline='') as fh:
            w = csv.writer(fh)
            w.writerow(['scene', 't_us', 'dur_us', 'thread', 'op', 'file',
                        'off', 'len', 'ret'])
            for r in recs:
                w.writerow([r['scene'], r['t'], r['dur'], r['thread'], r['op'],
                            r['file'], r['off'], r['len'], r['ret']])
        print('%d records -> %s' % (len(recs), out))
    else:
        summary(recs)


if __name__ == '__main__':
    main()
