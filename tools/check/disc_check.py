#!/usr/bin/env python3
"""disc_check.py -- read the finished ISO back and hold it to the layout.

tools/export/disc_layout.py writes mkisofs a sort file; this reads the image
mkisofs produced, walks its ISO9660 directory records, and asserts that
every file's extent ascends in the manifest's order. That is the whole
point of the pair: `-sort` is a hint to a tool we do not control, and a
layout that silently stopped applying would look exactly like one that
worked. This makes it a test failure instead of a feeling.

It reports three things beyond pass/fail, all of them per group:

  * the run -- first extent to last, and the bytes in it;
  * the padding, meaning sectors inside a group's span that no file in it
    occupies (mkisofs starts every file on a sector, so a run's span is
    always a little larger than its bytes);
  * where the group sits on the track, as a sector.

Called by scripts/make_cdi.sh after cdi4dc, so `./run.sh disc` checks its own
output. Nothing here needs the container: it is stdlib on a file.

The ISO the script is handed was written with mkisofs -C 0,11702, so
every extent in it is recorded as if the image lived at LBA 11702 -- the
MIL-CD convention make_cdi.sh explains. File offsets are therefore
(extent - 11702) * 2048, and --session-lba is that constant.
"""
import argparse
import os
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import disc_layout

SECTOR = 2048
PVD_LBA = 16            # always, relative to the start of the session
MSINFO_DEFAULT = 11702  # make_cdi.sh's -C 0,11702


class Iso:
    # Read a sector at a time rather than whole: a padded image is ~680 MB
    # (disc_layout.PAD), and all this ever wants is the directory.
    def __init__(self, path, session_lba):
        self.fp = open(path, "rb")
        self.size = os.fstat(self.fp.fileno()).st_size
        self.session = session_lba

    def sector(self, extent, count=1):
        off = (extent - self.session) * SECTOR
        if off < 0 or off + count * SECTOR > self.size:
            raise ValueError("extent %d is outside the image (session %d)"
                             % (extent, self.session))
        self.fp.seek(off)
        return self.fp.read(count * SECTOR)

    def primary_volume(self):
        pvd = self.sector(self.session + PVD_LBA)
        if pvd[0] != 1 or pvd[1:6] != b"CD001":
            raise ValueError("no primary volume descriptor at LBA %d"
                             % (self.session + PVD_LBA))
        return pvd

    def files(self):
        """{NAME: (extent, size)} over the whole volume, flattened.

        Keyed uppercase, because that is how the names are on the disc:
        the volume is plain ISO9660 -- no Joliet, no Rock Ridge, which
        the game's design wants -- so mkisofs upcased every name and the game
        still finds them because KOS's fncompare is case-insensitive
        (fs_iso9660.c). The manifest is keyed the same way below.

        The port's tree is flat and always will be -- every path in the
        game is a bare name joined to the asset root -- but a directory
        would be walked rather than silently skipped.
        """
        root = self.primary_volume()[156:190]
        out = {}
        self._walk(root, "", out)
        return out

    def _walk(self, record, prefix, out):
        extent, size = struct.unpack_from("<I", record, 2)[0], \
                       struct.unpack_from("<I", record, 10)[0]
        blob = self.sector(extent, (size + SECTOR - 1) // SECTOR)[:size]
        pos = 0
        while pos < size:
            length = blob[pos]
            if length == 0:
                # The rest of this sector is padding: a record never
                # straddles a sector boundary.
                pos = (pos // SECTOR + 1) * SECTOR
                continue
            rec = blob[pos:pos + length]
            pos += length
            name_len = rec[32]
            name = rec[33:33 + name_len]
            if name_len == 1 and name in (b"\x00", b"\x01"):
                continue        # "." and ".."
            name = name.decode("ascii").split(";")[0].upper()
            if rec[25] & 0x02:  # directory
                self._walk(rec, prefix + name + "/", out)
            else:
                out[prefix + name] = (struct.unpack_from("<I", rec, 2)[0],
                                      struct.unpack_from("<I", rec, 10)[0])


def check_bundle(iso, extent_size):
    """models.bnd as the ISO holds it, read back through the same parser
    tools/export/ssb_bundle.py --check uses: its index names disc_layout's
    BUNDLE_MODELS in order and every entry is on a 32-byte offset.
    None on a pass. The bytes are held to the loose files by
    `./run.sh test bundle`; this is the image's copy."""
    import ssb_bundle
    extent, size = extent_size
    blob = iso.sector(extent, (size + SECTOR - 1) // SECTOR)[:size]
    try:
        _data_off, entries = ssb_bundle.read_index(blob)
    except (ValueError, struct.error) as exc:
        return str(exc)
    if [e[0] for e in entries] != disc_layout.bundle_names():
        return "index is not disc_layout.bundle_names() in order"
    for name, off, n in entries:
        if off % ssb_bundle.ALIGN or off + n > size:
            return "%s at %d+%d is misplaced" % (name, off, n)
    return None


def check(path, session_lba, verbose=True):
    iso = Iso(path, session_lba)
    found = iso.files()
    bundled = disc_layout.BUNDLE.upper() in found
    order = disc_layout.census_order(disc_layout.manifest_order(bundled))
    upper = {n.upper(): n for n in order}

    unknown = sorted(set(found) - set(upper))
    if unknown:
        print("disc_check: FAIL -- %d file(s) on the disc that the layout "
              "table does not name:" % len(unknown))
        for name in unknown:
            print("  %s" % name)
        return 1

    placed = [(n, found[n.upper()][0], found[n.upper()][1])
              for n in order if n.upper() in found]
    if not placed:
        print("disc_check: FAIL -- no manifest file found in %s" % path)
        return 1

    # The assertion. Ascending, and strictly: two files cannot share an
    # extent, so an equality here is a parse that went wrong.
    bad = []
    for (n0, e0, _s0), (n1, e1, _s1) in zip(placed, placed[1:]):
        if e1 <= e0:
            bad.append((n0, e0, n1, e1))
    if bad:
        print("disc_check: FAIL -- %d file(s) out of manifest order in %s"
              % (len(bad), path))
        for n0, e0, n1, e1 in bad:
            print("  %-20s @ %-8d should precede %-20s @ %d"
                  % (n0, e0, n1, e1))
        return 1

    if bundled:
        err = check_bundle(iso, found[disc_layout.BUNDLE.upper()])
        if err:
            print("disc_check: FAIL -- %s: %s" % (disc_layout.BUNDLE, err))
            return 1

    # The pad's two claims: the image still fits an 80-minute disc, and
    # the game sits at the end of it. The order check above already puts
    # the pad before every other file; this holds it to its size.
    sectors = iso.size // SECTOR
    leadout = session_lba + sectors + disc_layout.POSTGAP
    if leadout > disc_layout.CD80_LEADOUT_LBA:
        print("disc_check: FAIL -- the lead-out would start at LBA %d, past "
              "an 80-minute disc's %d" % (leadout, disc_layout.CD80_LEADOUT_LBA))
        return 1
    padded = disc_layout.PAD.upper() in found
    if padded and sectors != disc_layout.PAD_TARGET_SECTORS:
        print("disc_check: FAIL -- padded image is %d sectors, not "
              "disc_layout.PAD_TARGET_SECTORS (%d)"
              % (sectors, disc_layout.PAD_TARGET_SECTORS))
        return 1

    if verbose:
        print("disc_check: %s" % path)
        print("  %-8s %5s  %10s  %10s  %8s  %s"
              % ("group", "files", "bytes", "span", "padding", "at sector"))
        total_bytes = 0
        for group, files in disc_layout.MANIFEST:
            here = [(n, found[n.upper()][0], found[n.upper()][1])
                    for n in disc_layout.group_files(files, bundled)
                    if n.upper() in found]
            if not here:
                continue
            first = min(e for _n, e, _s in here)
            last = max(e + (s + SECTOR - 1) // SECTOR for _n, e, s in here)
            nbytes = sum(s for _n, _e, s in here)
            span = (last - first) * SECTOR
            total_bytes += nbytes
            print("  %-8s %5d  %10d  %10d  %7d   %d"
                  % (group, len(here), nbytes, span, span - nbytes,
                     first - session_lba))
        print("  %-8s %5d  %10d" % ("total", len(placed), total_bytes))
        print("  order: ok, %d files ascending from sector %d to %d"
              % (len(placed), placed[0][1] - session_lba,
                 placed[-1][1] - session_lba))
        game = [e for n, e, _s in placed if n != disc_layout.PAD]
        print("  %s: game data at LBA %d..%d of %d (80 min), lead-out at %d"
              % ("padded" if padded else "unpadded", min(game),
                 session_lba + sectors - 1, disc_layout.CD80_LEADOUT_LBA,
                 leadout))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("iso", help="the data track mkisofs wrote")
    ap.add_argument("--session-lba", type=int, default=MSINFO_DEFAULT,
                    help="the -C next-session LBA (default %d)" % MSINFO_DEFAULT)
    ap.add_argument("-q", "--quiet", action="store_true")
    args = ap.parse_args()
    try:
        return check(args.iso, args.session_lba, verbose=not args.quiet)
    except ValueError as exc:
        print("disc_check: FAIL -- %s" % exc)
        return 1


if __name__ == "__main__":
    sys.exit(main())
