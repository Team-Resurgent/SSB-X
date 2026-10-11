"""pvrsoft.py -- a software PVR: the stream the port hands the TA, drawn in numpy.

The in-build baker needs the picture the console would draw for a
team card's crowd, with no console. The port already computes
everything the PVR does not: poses, lighting, clipping, the viewport map. What
it submits is polygon headers and vertices in framebuffer pixels
(pvr_poly_hdr_t, pvr_vertex_t in KOS), plus the texture memory and palette
those headers name. This module reads that stream and draws it by the PVR's
rules; it does not model the tile accelerator, the ISP's fixed point or the
twiddled memory beyond what a texel fetch needs.

Stream, as an object:
    Stream.units   uint32 array (n, 8): 32-byte headers and vertices in order
    Stream.vram    {byte offset: bytes}  texture uploads, keyed by their base
    Stream.pal     uint32 array (1024,)  palette RAM
    Stream.palfmt  0 ARGB1555, 1 RGB565, 2 ARGB4444, 3 ARGB8888

Rules drawn (what the port's headers use; anything else raises):
  * pixel-centre coverage, no antialiasing, edges included on top-left only
    to the extent float64 decides ties;
  * depth: the header's comparison, z as submitted, written unless disabled;
  * texel: ARGB1555/RGB565/ARGB4444, 4bpp/8bpp paletted, VQ, twiddled or not;
    point or bilinear; wrap, mirror or clamp per axis;
  * UV interpolated as sum(l*uv*z) / sum(l*z), i.e. the PVR's perspective
    correction, which takes the vertex z to be 1/w (the port's banded depth is
    only approximately that, and so is the hardware's answer);
  * colour: Gouraud in screen space; texel x base (modulate / modulate-alpha /
    decal / replace) plus the offset colour; blend by the header's factors.
"""
import re

import numpy as np

CMP_NEVER, CMP_LESS, CMP_EQUAL, CMP_LEQUAL, CMP_GREATER, CMP_NOTEQUAL, \
    CMP_GEQUAL, CMP_ALWAYS = range(8)


class Stream:
    def __init__(self):
        self.units = np.zeros((0, 8), dtype=np.uint32)
        self.vram = {}
        self.pal = np.zeros(1024, dtype=np.uint32)
        self.palfmt = 0
        self.tic = None

    def texture_bytes(self, off, n):
        """n bytes of texture memory at `off`, from whichever upload holds it."""
        for base, blob in self.vram.items():
            if base <= off and off + n <= base + len(blob):
                return blob[off - base:off - base + n]
        raise KeyError("texture at %#x (%d bytes) is not in the dump" % (off, n))


def read_log(path):
    """The serial lines of src/dc/primdump.c, back into a Stream."""
    s = Stream()
    units = []
    tex = {}
    with open(path, errors="replace") as fh:
        for line in fh:
            m = re.search(r"\bpd (\d+) ([0-9a-f]+)\s*$", line)
            if m:
                units.append(bytes.fromhex(m.group(2)))
                continue
            m = re.search(r"\bpdx (\d+) ([0-9a-f]+)\s*$", line)
            if m:
                tex.setdefault(int(m.group(1)), bytes.fromhex(m.group(2)))
                continue
            m = re.search(r"\bpdp (\d+) ([0-9a-f]+)\s*$", line)
            if m:
                i = int(m.group(1))
                s.pal[i:i + 8] = np.frombuffer(bytes.fromhex(m.group(2)), "<u4")
                continue
            m = re.search(r"primdump: pal cfg (\d+)", line)
            if m:
                s.palfmt = int(m.group(1))
                continue
            m = re.search(r"primdump: begin (\d+)", line)
            if m:
                s.tic = int(m.group(1))
    raw = b"".join(units)
    s.units = np.frombuffer(raw, "<u4").reshape(-1, 8).copy()
    # 64-byte lines back into whole textures, keyed by the first line
    offs = sorted(tex)
    cur = None
    for o in offs:
        if cur is not None and o == cur + len(s.vram[cur]):
            s.vram[cur] += tex[o]
        else:
            cur = o
            s.vram[cur] = tex[o]
    return s


# ---- texture decode --------------------------------------------------------

def _spread(v, nb):
    out = np.zeros_like(v)
    for i in range(nb):
        out |= ((v >> i) & 1) << (2 * i)
    return out


def twiddle_index(w, h):
    """[h, w] -> texel number in twiddled memory (x in the odd bits)."""
    m = min(w, h)
    nb = m.bit_length() - 1
    y, x = np.mgrid[0:h, 0:w].astype(np.int64)
    idx = (_spread(x & (m - 1), nb) << 1) | _spread(y & (m - 1), nb)
    if w > h:
        idx |= (x >> nb) << (2 * nb)
    elif h > w:
        idx |= (y >> nb) << (2 * nb)
    return idx


def _rgba16(v, fmt):
    v = v.astype(np.uint32)
    out = np.zeros(v.shape + (4,), dtype=np.uint8)
    if fmt == 0:        # ARGB1555
        a = (v >> 15) & 1
        r, g, b = (v >> 10) & 31, (v >> 5) & 31, v & 31
        out[..., 0], out[..., 1], out[..., 2] = (r << 3 | r >> 2), (g << 3 | g >> 2), (b << 3 | b >> 2)
        out[..., 3] = a * 255
    elif fmt == 1:      # RGB565
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        out[..., 0], out[..., 1], out[..., 2] = (r << 3 | r >> 2), (g << 2 | g >> 4), (b << 3 | b >> 2)
        out[..., 3] = 255
    elif fmt == 2:      # ARGB4444
        out[..., 3] = ((v >> 12) & 15) * 17
        out[..., 0] = ((v >> 8) & 15) * 17
        out[..., 1] = ((v >> 4) & 15) * 17
        out[..., 2] = (v & 15) * 17
    else:
        raise ValueError("pixel format %d" % fmt)
    return out


def _rgba32(v):
    out = np.zeros(v.shape + (4,), dtype=np.uint8)
    out[..., 3] = (v >> 24) & 255
    out[..., 0] = (v >> 16) & 255
    out[..., 1] = (v >> 8) & 255
    out[..., 2] = v & 255
    return out


def decode_texture(stream, m2, m3):
    """(h, w, 4) uint8 RGBA of the texture a header's mode words name."""
    w, h = 8 << ((m2 >> 3) & 7), 8 << (m2 & 7)
    if m3 >> 31:
        raise ValueError("mipmapped texture")
    fmt = (m3 >> 27) & 7
    vq = (m3 >> 30) & 1
    twid = not ((m3 >> 26) & 1)
    off = (m3 & 0x1FFFFF) << 3
    if vq:
        n = 2048 + w * h // 4
        blob = stream.texture_bytes(off, n)
        book = np.frombuffer(blob[:2048], "<u2").reshape(256, 4)
        idx = np.frombuffer(blob[2048:], np.uint8)
        hw, hh = w // 2, h // 2
        t = twiddle_index(hw, hh)
        ci = idx[t]                                    # (hh, hw) codebook rows
        k = book[ci]                                   # (hh, hw, 4): x in the high bit
        px = np.zeros((h, w), dtype=np.uint16)
        px[0::2, 0::2] = k[..., 0]                     # (x0, y0)
        px[1::2, 0::2] = k[..., 1]                     # (x0, y1)
        px[0::2, 1::2] = k[..., 2]                     # (x1, y0)
        px[1::2, 1::2] = k[..., 3]
        return _rgba16(px, fmt)
    if fmt in (5, 6):
        n = w * h // 2 if fmt == 5 else w * h
        raw = np.frombuffer(stream.texture_bytes(off, n), np.uint8)
        t = twiddle_index(w, h) if twid else (np.arange(w * h).reshape(h, w))
        if fmt == 5:
            v = (raw[t >> 1] >> ((t & 1) * 4)) & 15
            base = ((m3 >> 21) & 63) * 16
        else:
            v = raw[t]
            base = ((m3 >> 25) & 3) * 256
        pal = stream.pal[(base + v.astype(np.int64)) & 1023]
        if stream.palfmt == 3:
            return _rgba32(pal)
        return _rgba16(pal & 0xFFFF, stream.palfmt)
    raw = np.frombuffer(stream.texture_bytes(off, w * h * 2), "<u2")
    t = twiddle_index(w, h) if twid else np.arange(w * h).reshape(h, w)
    return _rgba16(raw[t], fmt)


# ---- the rasteriser --------------------------------------------------------

class Header:
    def __init__(self, u):
        cmd, m1, m2, m3 = (int(x) for x in u[:4])
        self.cmd, self.m1, self.m2, self.m3 = cmd, m1, m2, m3
        self.textured = bool(cmd & 8)
        self.offset_on = bool(cmd & 4)
        self.gouraud = bool(cmd & 2)
        if cmd & 1:
            raise ValueError("16-bit UVs")
        if (cmd >> 4) & 3:
            raise ValueError("colour format %d" % ((cmd >> 4) & 3))
        self.cmp = (m1 >> 29) & 7
        self.cull = (m1 >> 27) & 3
        self.zwrite = not ((m1 >> 26) & 1)
        self.src = (m2 >> 29) & 7
        self.dst = (m2 >> 26) & 7
        self.use_alpha = bool((m2 >> 20) & 1)
        self.ignore_tex_alpha = bool((m2 >> 19) & 1)
        self.flip = (m2 >> 17) & 3
        self.clamp = (m2 >> 15) & 3
        self.filter = (m2 >> 13) & 3
        self.env = (m2 >> 6) & 3
        self.tex_key = (m2 & 0x3F, m3)


def _wrap(i, n, clamp, flip):
    if clamp:
        return np.clip(i, 0, n - 1)
    if flip:
        p = np.mod(i, 2 * n)
        return np.where(p >= n, 2 * n - 1 - p, p)
    return np.mod(i, n)


def _sample(tex, hdr, u, v):
    h, w = tex.shape[:2]
    # PVR_UVCLAMP / UVFLIP: 1 = V, 2 = U, 3 = both
    cu, cv = bool(hdr.clamp & 2), bool(hdr.clamp & 1)
    fu, fv = bool(hdr.flip & 2), bool(hdr.flip & 1)
    if hdr.filter == 0:
        xi = _wrap(np.floor(u * w).astype(np.int64), w, cu, fu)
        yi = _wrap(np.floor(v * h).astype(np.int64), h, cv, fv)
        return tex[yi, xi].astype(np.float64)
    x = u * w - 0.5
    y = v * h - 0.5
    x0 = np.floor(x).astype(np.int64)
    y0 = np.floor(y).astype(np.int64)
    fx = (x - x0)[..., None]
    fy = (y - y0)[..., None]
    xa, xb = _wrap(x0, w, cu, fu), _wrap(x0 + 1, w, cu, fu)
    ya, yb = _wrap(y0, h, cv, fv), _wrap(y0 + 1, h, cv, fv)
    t = tex.astype(np.float64)
    return ((t[ya, xa] * (1 - fx) + t[ya, xb] * fx) * (1 - fy) +
            (t[yb, xa] * (1 - fx) + t[yb, xb] * fx) * fy)


def _unpack(c):
    c = np.asarray(c, dtype=np.uint32)
    return np.stack([(c >> 16) & 255, (c >> 8) & 255, c & 255, (c >> 24) & 255], -1).astype(np.float64)


def _factor(code, src, dst, is_src):
    """The blend factor of `code` for an incoming colour `src` over `dst`, RGBA 0..255."""
    a_s = src[..., 3:4] / 255.0
    a_d = dst[..., 3:4] / 255.0
    if code == 0:
        return 0.0
    if code == 1:
        return 1.0
    if code == 2:
        return (dst if is_src else src)[..., :3] / 255.0
    if code == 3:
        return 1.0 - (dst if is_src else src)[..., :3] / 255.0
    if code == 4:
        return a_s
    if code == 5:
        return 1.0 - a_s
    if code == 6:
        return a_d
    return 1.0 - a_d


skipped = []       # what triangles() could not read: headers it does not model


def triangles(stream):
    """Yield (Header, [three vertex rows]) in submission order. A strip of n
    vertices is n-2 triangles, the odd ones flipped."""
    hdr = None
    strip = []
    for u in stream.units:
        top = int(u[0]) >> 29
        if top == 4:
            try:
                hdr = Header(u)
            except ValueError as e:
                hdr = None
                skipped.append(str(e))
            strip = []
        elif top == 7:
            if hdr is None:
                continue
            strip.append(u)
            if int(u[0]) & 0x10000000:
                for i in range(len(strip) - 2):
                    tri = strip[i:i + 3]
                    if i & 1:
                        tri = [tri[1], tri[0], tri[2]]
                    yield hdr, tri
                strip = []
        else:
            raise ValueError("parameter type %d" % top)


def render(stream, size=(640, 480), clear=(0, 0, 0, 0), depth_clear=0.0,
           cull_sign=1.0, sort=False):
    """RGBA uint8 (h, w, 4) and the depth buffer. `sort` orders triangles far
    to near by their nearest vertex first (the TR list's autosort)."""
    W, H = size
    fb = np.zeros((H, W, 4), dtype=np.float64)
    fb[:] = clear
    zb = np.full((H, W), depth_clear, dtype=np.float64)
    tex_cache = {}

    tris = list(triangles(stream))
    if sort:
        tris.sort(key=lambda t: max(struct_z(v) for v in t[1]))
    for hdr, tri in tris:
        v = np.array([[*x[1:4].view(np.float32), *x[4:6].view(np.float32)] for x in tri],
                     dtype=np.float64)          # x y z u v
        col = _unpack([x[6] for x in tri])
        off = _unpack([x[7] for x in tri])
        xs, ys, zs = v[:, 0], v[:, 1], v[:, 2]
        area = (xs[1] - xs[0]) * (ys[2] - ys[0]) - (xs[2] - xs[0]) * (ys[1] - ys[0])
        if abs(area) < 1e-9:
            continue
        if hdr.cull and hdr.cull != 1:
            # 2: cull counter-clockwise, 3: clockwise (screen y down)
            if (hdr.cull == 2) == (area * cull_sign < 0):
                continue
        x0 = max(int(np.floor(xs.min() - 0.5)), 0)
        x1 = min(int(np.ceil(xs.max() - 0.5)) + 1, W)
        y0 = max(int(np.floor(ys.min() - 0.5)), 0)
        y1 = min(int(np.ceil(ys.max() - 0.5)) + 1, H)
        if x0 >= x1 or y0 >= y1:
            continue
        gy, gx = np.mgrid[y0:y1, x0:x1]
        px, py = gx + 0.5, gy + 0.5
        l0 = ((xs[1] - px) * (ys[2] - py) - (xs[2] - px) * (ys[1] - py)) / area
        l1 = ((xs[2] - px) * (ys[0] - py) - (xs[0] - px) * (ys[2] - py)) / area
        l2 = 1.0 - l0 - l1
        eps = -1e-9
        inside = (l0 >= eps) & (l1 >= eps) & (l2 >= eps)
        if not inside.any():
            continue
        z = l0 * zs[0] + l1 * zs[1] + l2 * zs[2]
        zc = zb[y0:y1, x0:x1]
        ok = {CMP_NEVER: z < -np.inf, CMP_LESS: z < zc, CMP_EQUAL: z == zc,
              CMP_LEQUAL: z <= zc, CMP_GREATER: z > zc, CMP_NOTEQUAL: z != zc,
              CMP_GEQUAL: z >= zc, CMP_ALWAYS: z > -np.inf}[hdr.cmp]
        m = inside & ok
        if not m.any():
            continue
        lam = np.stack([l0, l1, l2], -1)
        if hdr.gouraud:
            base = lam @ col
            offc = lam @ off
        else:
            base = np.broadcast_to(col[2], lam.shape[:2] + (4,))
            offc = np.broadcast_to(off[2], lam.shape[:2] + (4,))
        if hdr.textured:
            key = hdr.tex_key
            if key not in tex_cache:
                tex_cache[key] = decode_texture(stream, hdr.m2, hdr.m3)
            tex = tex_cache[key]
            wz = lam * zs                       # l_i * z_i
            den = wz.sum(-1)
            den = np.where(den == 0, 1.0, den)
            u = (wz * v[:, 3]).sum(-1) / den
            vv = (wz * v[:, 4]).sum(-1) / den
            t = _sample(tex, hdr, u, vv)
            if hdr.ignore_tex_alpha:
                t[..., 3] = 255.0
            if hdr.env == 0:                            # replace
                c = t.copy()
            elif hdr.env == 1:                          # modulate
                c = t * base / 255.0
                c[..., 3] = t[..., 3]
            elif hdr.env == 2:                          # decal
                a = t[..., 3:4] / 255.0
                c = np.concatenate([t[..., :3] * a + base[..., :3] * (1 - a), base[..., 3:4]], -1)
            else:                                       # modulate alpha
                c = t * base / 255.0
            if hdr.offset_on:
                c[..., :3] = np.minimum(c[..., :3] + offc[..., :3], 255.0)
        else:
            c = base.copy()
        if not hdr.use_alpha:
            c[..., 3] = 255.0
        dst = fb[y0:y1, x0:x1]
        fs = _factor(hdr.src, c, dst, True)
        fd = _factor(hdr.dst, c, dst, False)
        out = np.empty_like(dst)
        out[..., :3] = c[..., :3] * fs + dst[..., :3] * fd
        # alpha: what the blend leaves behind, for a coverage read-back
        a_s = c[..., 3:4] / 255.0
        out[..., 3:4] = (a_s + dst[..., 3:4] / 255.0 * (1 - a_s)) * 255.0
        out = np.clip(out, 0, 255)
        dst[m] = out[m]
        if hdr.zwrite:
            zc[m] = z[m]
    return np.rint(fb).astype(np.uint8), zb


def struct_z(u):
    return float(u[3:4].view(np.float32)[0])
