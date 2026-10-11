/* clip.h -- clip-space polygon clipping, out of src/dc/fighter.c so the
 * host cross-test can link it without the renderer.
 *
 * A ClipVtx is one vertex after fighter_frame: clip position (cx, cy,
 * cz, cw), texture coordinate and packed colour. The renderer clips
 * every triangle against the near plane before the perspective divide
 * (clip_near), and can also clip it against any set
 * of planes through the eye after that (clip_planes) -- which is how
 * the magnifying glass rounds off the fighter drawn inside it: the RDP
 * did that with a mask written into the Z-buffer, and the PVR has no
 * such thing, so the mask is sixteen planes instead (CLIP_MAGNIFY_PLANES).
 */
#ifndef SSB_DC_CLIP_H
#define SSB_DC_CLIP_H

#include <stdint.h>

typedef struct
{
    float cx, cy, cz, cw;
    float u, v;
    uint32_t argb;
} ClipVtx;

/* A triangle clipped against every plane can gain one vertex per plane. */
#define CLIP_MAX_PLANES 16
#define CLIP_MAX_VERTS (3 + CLIP_MAX_PLANES)

/* Clip one triangle against the near plane (cz + cw >= 0). Writes up to
 * four vertices into poly and returns how many: 3 or 4 for something to
 * draw, less for nothing. */
int clip_near(const ClipVtx in[3], ClipVtx poly[4]);

/* Clip a convex polygon of n vertices against nplanes planes, each
 * (a, b, c) keeping the side where a*cx + b*cy + c*cw >= 0. The
 * polygon is rewritten in place and the new count returned (0 when
 * nothing is left); poly and scratch must each hold CLIP_MAX_VERTS.
 * A polygon inside every plane comes back untouched.
 *
 * `inradius` is a circle about the NDC origin that lies inside every
 * plane, or 0 for none: a polygon whose vertices are all within it is
 * inside every plane and is returned before a plane is tested, which
 * is three multiplies a vertex against forty-eight. On a fighter
 * inside the glass that is nearly every triangle. */
int clip_planes(ClipVtx *poly, int n, const float (*planes)[3], int nplanes,
                float inradius, ClipVtx *scratch);

/* The magnifying glass's mask (if/ifcommon.c ifCommonPlayerMagnifyUpdateRender):
 * a regular 16-gon of inradius CLIP_MAGNIFY_INRADIUS in the NDC of the
 * glass's own viewport, whose half-width is the mask's radius. The RDP's
 * mask is the frame image's intensity, F to 8.6 texels of 9 and 0 by
 * 9.3 (tools/export/ssb_magnifyexport.py); this polygon's edges lie between
 * 8.82 and 9.0, inside that soft band and under the rim drawn over it. */
#define CLIP_MAGNIFY_NPLANES 16
#define CLIP_MAGNIFY_INRADIUS 0.98f
extern const float CLIP_MAGNIFY_PLANES[CLIP_MAGNIFY_NPLANES][3];

#endif /* SSB_DC_CLIP_H */
