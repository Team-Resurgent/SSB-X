#!/usr/bin/env python3
"""Turn -DDB_FB_DUMP's serial lines back into PNGs (src/dc/db.c
db_fb_dump_frame): a visual check when no screen capture is possible. A
-DDB_FB_DUMP_BOX dump lands on a black canvas of the full size.

    python3 tools/check/fbdump.py tools/scratch/probe.log out_prefix
writes out_prefix_<frame>_s<scene>.png for every complete dump in the log.
"""
import re
import sys

from PIL import Image

TOKEN = re.compile(r"r([0-9a-f]{2})([0-9a-f]{4})|([0-9a-f]{4})")


def rgb565(v):
    r = (v >> 11) & 0x1F
    g = (v >> 5) & 0x3F
    b = v & 0x1F
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


def main():
    log, prefix = sys.argv[1], sys.argv[2]
    img = None
    header = None
    out = []
    for line in open(log, errors="replace"):
        m = re.search(r"fbdump: begin (\d+) (-?\d+) (\d+) (\d+)"
                      r"(?: box (\d+) (\d+))?", line)
        if m:
            header = (int(m.group(1)), int(m.group(2)))
            img = Image.new("RGB", (int(m.group(3)), int(m.group(4))))
            # -DDB_FB_DUMP_BOX: rows and chunks count from the box's corner
            box = (int(m.group(5)), int(m.group(6))) if m.group(5) else (0, 0)
            continue
        m = re.search(r"fbd (\d+) (\d+) ([0-9a-fr]+)", line)
        if m and img is not None:
            row, chunk, body = int(m.group(1)), int(m.group(2)), m.group(3)
            x = box[0] + chunk * 80
            row += box[1]
            for t in TOKEN.finditer(body):
                if t.group(1):
                    n, px = int(t.group(1), 16), rgb565(int(t.group(2), 16))
                else:
                    n, px = 1, rgb565(int(t.group(3), 16))
                for _ in range(n):
                    if x < img.width:
                        img.putpixel((x, row), px)
                    x += 1
            continue
        m = re.search(r"fbdump: end (\d+)", line)
        if m and img is not None:
            name = "%s_%d_s%d.png" % (prefix, header[0], header[1])
            img.save(name)
            out.append(name)
            img = None
    for name in out:
        print(name)
    if not out:
        print("fbdump: no complete dump in %s" % log, file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
