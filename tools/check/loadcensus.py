#!/usr/bin/env python3
"""The load census: -DDB_LOAD_CENSUS serial logs
(src/dc/loadcensus.h) into an SQLite database of runs and nodes, and the
analyses the plan asks of them.

  loadcensus.py ingest LOG --db DB --label NAME [--scenario S] [--fighter F]
  loadcensus.py report  --db DB [--run NAME]     node by node
  loadcensus.py reloads --db DB [--run NAME]     files read again
  loadcensus.py scenes  --db DB                  per scene kind, all runs
  loadcensus.py summary --db DB                  per run: load totals
  loadcensus.py layout  --db DB [--runs A,B] [--out tools/export/disc_order.txt]
                                                 the cheapest disc order
  loadcensus.py pack    --db DB [--out tools/export/bundle_census.txt]
                                                 what to put in models.bnd

A NODE is one scene entered: its files (and where each came from), its
sound upload, its fighter-pack marks, and its memory at four moments --
begin, loaded (first frame drawn), peak (worst of each while it ran,
the load included) and end. `load_ms` is begin -> first frame;
`trans_ms` is the previous node's last drawn frame -> this node's first,
less the census's own serial print between them: the black screen a
player sees.

Host-served disc time is not a console's, so
every node also carries `gd_ms`, the medium time a GD-ROM would take
for the same trips, from a model fitted to a real console's
-DDB_DISC_TRACE log (see `calibrate`):

  loadcensus.py calibrate HW_DISCTRACE_LOG       fit the model

Sources: 0 disc (loose file), 1 models.bnd, 2 hold list (RAM),
3 read ahead (RAM, the loader's copy), 4 the loader thread's own read.
"""
import argparse
import collections
import os
import re
import sqlite3
import sys

LINE = re.compile(r'lc: (.*)$')
SRC = {0: 'disc', 1: 'bnd', 2: 'held', 3: 'ahead', 4: 'loader'}
MEDIUM = (0, 1, 4)          # sources that cost a trip to the medium

# GD-ROM model, fitted to a real console's -DDB_DISC_TRACE log
# (~/disc-trace2.log, 2026-09-29) by `calibrate`: a loose file costs its
# bytes at ~600 KB/s plus ~40 ms of open and stream starts; an entry of
# models.bnd, read on the bundle's one open handle, ~13 ms (5.6 ms for
# each of its ~2.4 reads and seeks).
GD_RATE_KBPS = 600.0
GD_OPEN_MS = 40.0
GD_BND_MS = 13.0

SCHEMA = """
CREATE TABLE IF NOT EXISTS runs (
    run TEXT PRIMARY KEY, scenario TEXT, fighter TEXT, log TEXT,
    nodes INTEGER, created TEXT DEFAULT CURRENT_TIMESTAMP);
CREATE TABLE IF NOT EXISTS nodes (
    run TEXT, seq INTEGER, scene TEXT, kind INTEGER, router INTEGER,
    heap_size INTEGER, t_begin INTEGER, t_loaded INTEGER, t_end INTEGER,
    t_last_frame INTEGER, tics INTEGER, frames INTEGER, print_us INTEGER,
    dropped INTEGER, load_ms REAL, trans_ms REAL,
    files INTEGER, medium_files INTEGER, medium_bytes INTEGER,
    medium_ms REAL, gd_ms REAL, snd_bytes INTEGER, snd_ms REAL,
    PRIMARY KEY (run, seq));
CREATE TABLE IF NOT EXISTS mem (
    run TEXT, seq INTEGER, at TEXT, malloc_used INTEGER, malloc_free INTEGER,
    malloc_top INTEGER, heap_used INTEGER, vram_free INTEGER,
    snd_used INTEGER);
CREATE TABLE IF NOT EXISTS files (
    run TEXT, seq INTEGER, t INTEGER, path TEXT, size INTEGER,
    bytes INTEGER, us INTEGER, src TEXT, phase TEXT, reads INTEGER);
CREATE TABLE IF NOT EXISTS sounds (
    run TEXT, seq INTEGER, t INTEGER, fgm_bytes INTEGER, bgm_bytes INTEGER,
    us INTEGER);
CREATE TABLE IF NOT EXISTS marks (
    run TEXT, seq INTEGER, t INTEGER, what TEXT, value INTEGER);
CREATE INDEX IF NOT EXISTS files_path ON files (path);
"""


def gd_ms(nbytes, trips, bnd_trips=0):
    return (nbytes / (GD_RATE_KBPS * 1024.0) * 1000.0 + trips * GD_OPEN_MS +
            bnd_trips * GD_BND_MS)


def parse(path):
    """One dict per node, in order."""
    names, nodes = {}, []
    cur, now, t_hi = None, 0, 0

    def unwrap(t32):
        return now - ((now - t32) & 0xFFFFFFFF)

    for raw in open(path, errors='replace'):
        m = LINE.search(raw)
        if not m:
            continue
        f = m.group(1).split()
        if not f:
            continue
        k = f[0]
        if k == 'H':
            now = int(f[2])
            cur = dict(files=[], mem={}, sounds=[], marks=[],
                       dropped=int(f[4]), print_us_prev=int(f[5]))
            nodes.append(cur)
            continue
        if k == 'n':
            names[int(f[1])] = ' '.join(f[2:])
            continue
        if cur is None or len(f) != 9:
            continue
        t = unwrap(int(f[1]))
        nid = int(f[2])
        v = [int(x) for x in f[3:]]
        name = names.get(nid, '')
        if k == 'N':
            cur.update(scene=name, kind=v[0], router=v[1], heap_size=v[2],
                       t_begin=t)
        elif k == 'F':
            cur['files'].append(dict(t=t, path=name, size=v[0], bytes=v[1],
                                     us=v[2], src=SRC.get(v[3], str(v[3])),
                                     phase='play' if v[4] else 'load',
                                     reads=v[5]))
        elif k == 'M':
            cur['mem'][name] = v
        elif k == 'S':
            cur['sounds'].append(dict(t=t, fgm=v[0], bgm=v[1], us=v[2]))
        elif k == 'X':
            cur['marks'].append(dict(t=t, what=name, value=v[0]))
        elif k == 'L':
            cur['t_loaded'] = t
        elif k == 'E':
            cur['t_end'] = t
            cur['tics'], cur['frames'] = v[0], v[1]
            cur['t_last_frame'] = unwrap(v[2]) if v[2] else None
    return [n for n in nodes if 'scene' in n]


def strip_path(p):
    return p.rsplit('/', 1)[-1]


def ingest(args):
    nodes = parse(args.log)
    db = sqlite3.connect(args.db)
    db.executescript(SCHEMA)
    for tbl in ('runs', 'nodes', 'mem', 'files', 'sounds', 'marks'):
        db.execute('DELETE FROM %s WHERE run = ?' % tbl, (args.label,))
    prev = None
    for seq, n in enumerate(nodes):
        load_ms = ((n['t_loaded'] - n['t_begin']) / 1000.0
                   if n.get('t_loaded') else None)
        trans_ms = None
        if n.get('t_loaded') and prev is not None and prev.get('t_last_frame'):
            # the census's own print sits between the two; take it out
            trans_ms = (n['t_loaded'] - prev['t_last_frame'] -
                        n['print_us_prev']) / 1000.0
        med = [f for f in n['files'] if f['src'] in ('disc', 'bnd')]
        mb = sum(f['bytes'] for f in med)
        mus = sum(f['us'] for f in med)
        snd_b = sum(s['fgm'] + s['bgm'] for s in n['sounds'])
        snd_us = sum(s['us'] for s in n['sounds'])
        # a sound upload is reads of the two packs, which close as files
        # too (fgm_sounds.pak, bgm.pak): they are already in `med`
        db.execute('INSERT INTO nodes VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,'
                   '?,?,?,?,?,?,?)',
                   (args.label, seq, n['scene'], n['kind'], n['router'],
                    n['heap_size'], n['t_begin'], n.get('t_loaded'),
                    n.get('t_end'), n.get('t_last_frame'), n.get('tics'),
                    n.get('frames'), n['print_us_prev'], n['dropped'],
                    load_ms, trans_ms, len(n['files']), len(med), mb,
                    mus / 1000.0,
                    gd_ms(mb, sum(1 for f in med if f['src'] == 'disc'),
                          sum(1 for f in med if f['src'] == 'bnd')), snd_b,
                    snd_us / 1000.0))
        for at, v in n['mem'].items():
            db.execute('INSERT INTO mem VALUES (?,?,?,?,?,?,?,?,?)',
                       (args.label, seq, at, *v))
        for f in n['files']:
            db.execute('INSERT INTO files VALUES (?,?,?,?,?,?,?,?,?,?)',
                       (args.label, seq, f['t'], strip_path(f['path']),
                        f['size'], f['bytes'], f['us'], f['src'], f['phase'],
                        f['reads']))
        for s in n['sounds']:
            db.execute('INSERT INTO sounds VALUES (?,?,?,?,?,?)',
                       (args.label, seq, s['t'], s['fgm'], s['bgm'], s['us']))
        for mk in n['marks']:
            db.execute('INSERT INTO marks VALUES (?,?,?,?,?)',
                       (args.label, seq, mk['t'], mk['what'], mk['value']))
        prev = n
    db.execute('INSERT INTO runs (run, scenario, fighter, log, nodes) '
               'VALUES (?,?,?,?,?)',
               (args.label, args.scenario, args.fighter,
                os.path.abspath(args.log), len(nodes)))
    db.commit()
    print('%s: %d nodes, %d files' % (args.label, len(nodes),
                                      sum(len(n['files']) for n in nodes)))


def kb(x):
    return '%7.0f' % (x / 1024.0) if x is not None else '      -'


def runs_of(db, name):
    if name:
        return [name]
    return [r[0] for r in db.execute('SELECT run FROM runs ORDER BY created')]


def report(args):
    db = sqlite3.connect(args.db)
    for run in runs_of(db, args.run):
        print('== %s' % run)
        print('%3s %-17s %8s %8s %8s %6s %7s %7s | %7s %7s %7s %7s %7s %6s'
              % ('#', 'scene', 'trans', 'load', 'gd est', 'files', 'discKB',
                 'sndKB', 'mUsedL', 'mUsedPk', 'mFreePk', 'heapPk',
                 'vramPk', 'sndKB'))
        for row in db.execute(
                'SELECT seq, scene, trans_ms, load_ms, gd_ms, medium_files, '
                'medium_bytes, snd_bytes FROM nodes WHERE run = ? '
                'ORDER BY seq', (run,)):
            seq = row[0]
            mem = {at: v for at, *v in db.execute(
                'SELECT at, malloc_used, malloc_free, malloc_top, heap_used, '
                'vram_free, snd_used FROM mem WHERE run = ? AND seq = ?',
                (run, seq))}
            ld = mem.get('loaded') or mem.get('end') or [None] * 6
            pk = mem.get('peak') or [None] * 6
            print('%3d %-17s %8s %8s %8.0f %6d %7s %7s | %7s %7s %7s %7s %7s %6s'
                  % (seq, row[1][:17],
                     '%.0f' % row[2] if row[2] is not None else '-',
                     '%.0f' % row[3] if row[3] is not None else '-',
                     row[4], row[5], kb(row[6]), kb(row[7]),
                     kb(ld[0]), kb(pk[0]), kb(pk[1]), kb(pk[3]), kb(pk[4]),
                     kb(pk[5])))


def reloads(args):
    """Files that cross the medium in more than one node of a run: every
    such read after the first is one the port could have kept. 'next'
    means it was read again by the very next node that read anything --
    given back and asked for again straight away."""
    db = sqlite3.connect(args.db)
    for run in runs_of(db, args.run):
        rows = list(db.execute(
            "SELECT f.seq, n.scene, f.path, SUM(f.bytes), SUM(f.us) "
            "FROM files f JOIN nodes n ON n.run = f.run AND n.seq = f.seq "
            "WHERE f.run = ? AND f.src IN ('disc', 'bnd', 'loader') "
            "GROUP BY f.seq, f.path ORDER BY f.seq", (run,)))
        by = collections.defaultdict(list)
        for seq, scene, p, b, us in rows:
            by[p].append((seq, scene, b, us))
        total = sum(r[3] for r in rows)
        waste = []
        for p, hits in by.items():
            if len(hits) < 2:
                continue
            again = hits[1:]
            wb = sum(h[2] for h in again)
            nxt = sum(1 for a, b in zip(hits, hits[1:]) if b[0] - a[0] <= 2)
            waste.append((wb, p, len(hits), nxt,
                          ' '.join('%d:%s' % (h[0], h[1]) for h in hits)))
        waste.sort(reverse=True)
        wt = sum(w[0] for w in waste)
        print('== %s: %.2f MB off the medium, %.2f MB of it (%.0f%%) a '
              'file read before in this run; gd est %.1f s of %.1f s'
              % (run, total / 1048576.0, wt / 1048576.0,
                 100.0 * wt / max(total, 1),
                 gd_ms(wt, sum(w[2] - 1 for w in waste)) / 1000.0,
                 gd_ms(total, len(rows)) / 1000.0))
        print('%9s %5s %5s  %-26s %s' % ('again KB', 'reads', '<=2',
                                          'file', 'nodes'))
        for wb, p, n, nxt, where in waste[:args.top]:
            print('%9.0f %5d %5d  %-26s %s' % (wb / 1024.0, n, nxt, p,
                                               where[:110]))


def scenes(args):
    """Per scene kind over every run: how often entered, and its mean load
    and disc bill -- and the files EVERY entry of that kind read, which is
    that scene's fixed cost."""
    db = sqlite3.connect(args.db)
    print('%-18s %5s %9s %9s %9s %9s %8s' % ('scene', 'n', 'trans ms',
                                             'load ms', 'gd est ms', 'disc KB',
                                             'snd KB'))
    for scene, n, tr, ld, gd, mb, sb in db.execute(
            'SELECT scene, COUNT(*), AVG(trans_ms), AVG(load_ms), AVG(gd_ms), '
            'AVG(medium_bytes), AVG(snd_bytes) FROM nodes GROUP BY scene '
            'ORDER BY SUM(gd_ms) DESC'):
        print('%-18s %5d %9.0f %9.0f %9.0f %9.0f %8.0f'
              % (scene, n, tr or 0, ld or 0, gd or 0, (mb or 0) / 1024.0,
                 (sb or 0) / 1024.0))
    if args.files:
        for scene, n in db.execute(
                'SELECT scene, COUNT(*) FROM nodes GROUP BY scene HAVING '
                'COUNT(*) > 1'):
            fixed = list(db.execute(
                "SELECT path, AVG(bytes) FROM files f JOIN nodes n ON "
                "n.run = f.run AND n.seq = f.seq WHERE n.scene = ? AND "
                "f.src IN ('disc','bnd') GROUP BY path HAVING "
                "COUNT(DISTINCT f.run || '/' || f.seq) = ?", (scene, n)))
            if fixed:
                print('\n%s: read by all %d entries: %s' % (
                    scene, n, ', '.join('%s %.0fK' % (p, b / 1024.0)
                                        for p, b in fixed)))


def summary(args):
    db = sqlite3.connect(args.db)
    print('%-24s %5s %9s %9s %9s %9s %9s %9s' % (
        'run', 'nodes', 'trans s', 'load s', 'gd est s', 'disc MB', 'snd MB',
        'peak MB'))
    for run, in db.execute('SELECT run FROM runs ORDER BY created'):
        n, tr, ld, gd, mb, sb = db.execute(
            'SELECT COUNT(*), SUM(trans_ms), SUM(load_ms), SUM(gd_ms), '
            'SUM(medium_bytes), SUM(snd_bytes) FROM nodes WHERE run = ?',
            (run,)).fetchone()
        pk, = db.execute("SELECT MAX(malloc_used) FROM mem WHERE run = ? "
                         "AND at = 'peak'", (run,)).fetchone()
        print('%-24s %5d %9.1f %9.1f %9.1f %9.2f %9.2f %9.2f' % (
            run, n, (tr or 0) / 1000.0, (ld or 0) / 1000.0,
            (gd or 0) / 1000.0, (mb or 0) / 1048576.0,
            (sb or 0) / 1048576.0, (pk or 0) / 1048576.0))


def calibrate(args):
    """Fit ms = bytes/rate + open_ms to a console's per-file trips, from a
    -DDB_DISC_TRACE log: each file's O..C records summed."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import disctrace
    recs = disctrace.parse(args.log)
    per = collections.defaultdict(lambda: [0, 0])     # (bytes, us)
    open_key = {}
    for i, r in enumerate(recs):
        if r['op'] == 'O':
            open_key[r['fid']] = i
        if r['op'] in 'ORSC' and r['fid'] in open_key:
            k = open_key[r['fid']]
            per[k][1] += r['dur']
            if r['op'] == 'R' and r['ret'] > 0:
                per[k][0] += r['ret']
    xs = [(b, us / 1000.0) for b, us in per.values() if b > 0]
    n = len(xs)
    sx = sum(b for b, _ in xs)
    sy = sum(ms for _, ms in xs)
    sxx = sum(b * b for b, _ in xs)
    sxy = sum(b * ms for b, ms in xs)
    slope = (n * sxy - sx * sy) / (n * sxx - sx * sx)
    icpt = (sy - slope * sx) / n
    print('%d file trips: ms = bytes / %.0f KB/s + %.1f ms per file'
          % (n, 1.0 / slope / 1024.0 * 1000.0, icpt))
    print('overall: %.2f MB in %.1f s = %.0f KB/s'
          % (sx / 1048576.0, sy / 1000.0, sx / 1024.0 / (sy / 1000.0)))


def trip_seqs(db, runs):
    """Each run's drive trips in order; a models.bnd entry is the bundle,
    and back-to-back reads of one file are one trip."""
    seqs = []
    for run in runs:
        seq = []
        for path, src in db.execute(
                "SELECT path, src FROM files WHERE run = ? AND src IN "
                "('disc', 'bnd', 'loader') ORDER BY seq, t", (run,)):
            f = 'models.bnd' if src == 'bnd' else path
            if not seq or seq[-1] != f:
                seq.append(f)
        seqs.append(seq)
    return seqs


def layout(args):
    """The disc order that makes these runs' trips cheapest
    (tools/check/disclayout_opt.py), against the manifest's."""
    here = os.path.dirname(os.path.abspath(__file__))
    sys.path[:0] = [here, os.path.join(here, '..', 'export')]
    import disc_layout
    import disclayout_opt as opt
    db = sqlite3.connect(args.db)
    runs = args.runs.split(',') if args.runs else [
        r for r, in db.execute("SELECT run FROM runs WHERE run LIKE 'after%' "
                               "ORDER BY created")]
    weights = [1.0] * len(runs)
    seqs = trip_seqs(db, runs)
    tree = args.tree
    sizes = {}
    for root, _d, fs in os.walk(tree):
        for n in fs:
            sizes[os.path.relpath(os.path.join(root, n), tree)] = \
                os.path.getsize(os.path.join(root, n))
    manifest = [f for f in disc_layout.manifest_order(bundled=True)
                if f in sizes and f != disc_layout.PAD]
    missing = sorted({f for s in seqs for f in s} - set(manifest))
    if missing:
        print('not on the disc, left out: %s' % ' '.join(missing))
        seqs = [[f for f in s if f in set(manifest)] for s in seqs]
    now = opt.Layout(manifest, sizes)
    c0 = now.cost(seqs, weights)
    best, trans = opt.optimise(seqs, weights, sizes, manifest,
                               iters=args.iters)
    c1 = best.cost(seqs, weights)
    trips = sum(len(s) for s in seqs)
    print('%d runs, %d trips, %d distinct files' % (
        len(runs), trips, len({f for s in seqs for f in s})))
    print('seek+start cost: manifest %.1f s, optimised %.1f s (%.1f s, '
          '%.0f%% less); the floor, every trip a 0-gap start: %.1f s' % (
              c0 / 1000, c1 / 1000, (c0 - c1) / 1000,
              100 * (c0 - c1) / max(c0, 1), trips * 0.042))
    for run, seq in zip(runs, seqs):
        print('  %-18s %5d trips  %6.1f s -> %6.1f s' % (
            run, len(seq), now.cost([seq], [1]) / 1000,
            best.cost([seq], [1]) / 1000))
    if args.out:
        hot = [f for f in best.order if any(f in s for s in seqs)]
        with open(args.out, 'w') as fh:
            fh.write('# tools/check/loadcensus.py layout: the disc order '
                     'for the files the census\n# runs read, cheapest first '
                     'to last; disc_layout.py puts these ahead of\n# its '
                     'manifest order. Runs: %s\n' % ','.join(runs))
            for f in hot:
                fh.write(f + '\n')
        print('%d files -> %s' % (len(hot), args.out))
    if args.bundles:
        print('\nback-to-back pairs worth a bundle (reads, a, b):')
        for w, a, b in opt.bundles(seqs, weights, trans, sizes)[:30]:
            print('  %4.0f  %-26s %s' % (w, a, b))


# Never packed: the audio files (sndres and bgm read them by offset, on
# their own schedule, and at boot), the boot files and the pad, and the
# bundle itself.
PACK_NEVER = {'fgm_sounds.pak', 'bgm.pak', 'sndsets.bin', 'fgm.ucd',
              'fgm.tbl', 'fgm.unk', '1ST_READ.BIN', 'disc.id', 'pad.bin',
              'models.bnd'}


def pack(args):
    """The files to put in models.bnd (tools/export/disc_layout.py
    PACK_FILE): every file the runs read back to back with another at
    least --min times, that the romdisk has (the bundle is built from
    it), and whose name fits an entry (31 bytes)."""
    here = os.path.dirname(os.path.abspath(__file__))
    sys.path[:0] = [here, os.path.join(here, '..', 'export')]
    import disclayout_opt as opt
    db = sqlite3.connect(args.db)
    runs = args.runs.split(',') if args.runs else [
        r for r, in db.execute("SELECT run FROM runs WHERE run LIKE 'after%' "
                               "ORDER BY created")]
    seqs = trip_seqs(db, runs)
    trans = opt.transitions(seqs, [1.0] * len(seqs))
    near = collections.Counter()
    for (a, b), w in trans.items():
        near[a] += w
        near[b] += w
    src = args.romdisk
    picked, skipped = [], collections.Counter()
    for f, w in near.most_common():
        if w < args.min:
            continue
        if f in PACK_NEVER or f.startswith('introcrowd_'):
            skipped['never'] += 1
        elif len(f.encode()) > 31:
            skipped['name too long: ' + f] += 1
        elif not os.path.isfile(os.path.join(src, f)):
            skipped['not in the romdisk: ' + f] += 1
        else:
            picked.append(f)
    picked.sort()
    size = sum(os.path.getsize(os.path.join(src, f)) for f in picked)
    trips = sum(1 for s in seqs for f in s if f in set(picked))
    alltrips = sum(len(s) for s in seqs)
    print('%d files, %.2f MB, into the bundle: %d of the runs\' %d trips'
          % (len(picked), size / 1048576.0, trips, alltrips))
    for why, n in sorted(skipped.items()):
        print('  skipped %s%s' % (why, '' if n == 1 else ' (%d)' % n))
    if args.out:
        with open(args.out, 'w') as fh:
            fh.write('# tools/check/loadcensus.py pack: files the census '
                     'runs read back to back,\n# packed into models.bnd '
                     '(tools/export/disc_layout.py bundle_names).\n'
                     '# Runs: %s\n' % ','.join(runs))
            for f in picked:
                fh.write(f + '\n')
        print('-> %s' % args.out)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('ingest')
    p.add_argument('log')
    p.add_argument('--db', required=True)
    p.add_argument('--label', required=True)
    p.add_argument('--scenario', default='')
    p.add_argument('--fighter', default='')
    p.set_defaults(fn=ingest)
    for name, fn in (('report', report), ('reloads', reloads),
                     ('scenes', scenes), ('summary', summary)):
        p = sub.add_parser(name)
        p.add_argument('--db', required=True)
        p.add_argument('--run')
        p.add_argument('--top', type=int, default=30)
        p.add_argument('--files', action='store_true')
        p.set_defaults(fn=fn)
    p = sub.add_parser('layout')
    p.add_argument('--db', required=True)
    p.add_argument('--runs', help='comma-separated (default every after* run)')
    p.add_argument('--tree', default='build-dc/disc/ssb64/iso')
    p.add_argument('--iters', type=int, default=20000)
    p.add_argument('--out')
    p.add_argument('--bundles', action='store_true')
    p.set_defaults(fn=layout)
    p = sub.add_parser('pack')
    p.add_argument('--db', required=True)
    p.add_argument('--runs')
    p.add_argument('--min', type=float, default=2)
    p.add_argument('--romdisk', default='src/game/ssb64/romdisk')
    p.add_argument('--out')
    p.set_defaults(fn=pack)
    p = sub.add_parser('calibrate')
    p.add_argument('log')
    p.set_defaults(fn=calibrate)
    args = ap.parse_args()
    args.fn(args)


if __name__ == '__main__':
    main()
