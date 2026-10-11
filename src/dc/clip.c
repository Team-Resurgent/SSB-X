/* clip.c -- see clip.h. */
#include "clip.h"

static float cd(const ClipVtx *v) { return v->cz + v->cw; }

static void lerp(ClipVtx *o, const ClipVtx *a, const ClipVtx *b, float t)
{
    o->cx = a->cx + t * (b->cx - a->cx);
    o->cy = a->cy + t * (b->cy - a->cy);
    o->cz = a->cz + t * (b->cz - a->cz);
    o->cw = a->cw + t * (b->cw - a->cw);
    o->u = a->u + t * (b->u - a->u);
    o->v = a->v + t * (b->v - a->v);
    /* The colour too, channel by channel, alpha included. It was copied
     * from `a`, so every vertex a clip made took one endpoint's colour
     * and the piece of the triangle past it a hard edge of the wrong
     * shade: Final Destination's vortex (pink rim, purple apex) and fog
     * (vertex alpha) showed a wedge or a band at every clipped edge, and
     * any vertex-coloured triangle crossing the near plane did the same.
     * Linear in clip space, as the u and v above are. */
    {
        uint32_t ca = a->argb, cb = b->argb, out = 0;
        int sh;

        for (sh = 0; sh < 32; sh += 8)
        {
            float x = (float)((ca >> sh) & 0xFF), y = (float)((cb >> sh) & 0xFF);
            int c = (int)(x + t * (y - x) + 0.5f);

            out |= (uint32_t)(c < 0 ? 0 : c > 255 ? 255 : c) << sh;
        }
        o->argb = out;
    }
}

/* H2's, moved here whole from src/dc/fighter.c. */
int clip_near(const ClipVtx in[3], ClipVtx poly[4])
{
    float d[3];
    ClipVtx tmp[4];
    ClipVtx *outp[4];
    int n = 0, i;

    d[0] = cd(&in[0]);
    d[1] = cd(&in[1]);
    d[2] = cd(&in[2]);
    if (d[0] >= 0 && d[1] >= 0 && d[2] >= 0)
    {
        poly[0] = in[0];
        poly[1] = in[1];
        poly[2] = in[2];
        return 3;
    }
    if (d[0] < 0 && d[1] < 0 && d[2] < 0)
        return 0;

    for (i = 0; i < 3; i++)
    {
        ClipVtx *cur = (ClipVtx *)&in[i];
        ClipVtx *nxt = (ClipVtx *)&in[(i + 1) % 3];
        float dc = d[i], dn = d[(i + 1) % 3];
        if (dc >= 0 && n < 4)
            outp[n++] = cur;
        if ((dc >= 0) != (dn >= 0) && n < 4)
        {
            float t = dc / (dc - dn);
            lerp(&tmp[n], cur, nxt, t);
            outp[n] = &tmp[n];
            n++;
        }
    }
    if (n < 3)
        return 0;
    for (i = 0; i < n; i++)
        poly[i] = *outp[i];
    return n;
}

/* Sutherland-Hodgman, one plane at a time, ping-ponging between the
 * caller's two buffers so the result always ends up back in poly. */
int clip_planes(ClipVtx *poly, int n, const float (*planes)[3], int nplanes,
                float inradius, ClipVtx *scratch)
{
    ClipVtx *src = poly, *dst = scratch;
    int p;

    if (inradius > 0.0f)
    {
        float r2 = inradius * inradius;
        int i;

        /* cx*cx + cy*cy <= r*r * cw*cw is |ndc| <= r without the divide */
        for (i = 0; i < n; i++)
        {
            float cx = poly[i].cx, cy = poly[i].cy, cw = poly[i].cw;

            if (cx * cx + cy * cy > r2 * cw * cw)
                break;
        }
        if (i == n)
            return n;
    }
    for (p = 0; p < nplanes && n > 0; p++)
    {
        float a = planes[p][0], b = planes[p][1], c = planes[p][2];
        float d[CLIP_MAX_VERTS];
        int all_in = 1, all_out = 1;
        int i, m = 0;

        for (i = 0; i < n; i++)
        {
            d[i] = a * src[i].cx + b * src[i].cy + c * src[i].cw;
            if (d[i] >= 0.0f)
                all_out = 0;
            else
                all_in = 0;
        }
        if (all_in)
            continue;
        if (all_out)
            return 0;
        for (i = 0; i < n; i++)
        {
            int j = (i + 1 == n) ? 0 : i + 1;
            float dc = d[i], dn = d[j];

            if (dc >= 0.0f && m < CLIP_MAX_VERTS)
                dst[m++] = src[i];
            if ((dc >= 0.0f) != (dn >= 0.0f) && m < CLIP_MAX_VERTS)
            {
                lerp(&dst[m], &src[i], &src[j], dc / (dc - dn));
                m++;
            }
        }
        n = m;
        if (src == poly)
        {
            src = scratch;
            dst = poly;
        }
        else
        {
            src = poly;
            dst = scratch;
        }
    }
    if (src != poly)
    {
        int i;

        for (i = 0; i < n; i++)
            poly[i] = src[i];
    }
    return (n < 3) ? 0 : n;
}

/* Plane k keeps -cos(t) * ndc_x - sin(t) * ndc_y + r >= 0 for
 * t = (k + 1/2) * 2pi/16: the half-plane inside the edge whose outward
 * normal is (cos t, sin t) at distance r from the centre. Multiplied
 * through by cw so it reads in clip space, where a plane through the
 * eye stays a plane. Written out rather than computed so the host test
 * and the target agree to the bit. */
const float CLIP_MAGNIFY_PLANES[CLIP_MAGNIFY_NPLANES][3] = {
    { -0.98078528f, -0.19509032f, CLIP_MAGNIFY_INRADIUS },
    { -0.83146961f, -0.55557023f, CLIP_MAGNIFY_INRADIUS },
    { -0.55557023f, -0.83146961f, CLIP_MAGNIFY_INRADIUS },
    { -0.19509032f, -0.98078528f, CLIP_MAGNIFY_INRADIUS },
    {  0.19509032f, -0.98078528f, CLIP_MAGNIFY_INRADIUS },
    {  0.55557023f, -0.83146961f, CLIP_MAGNIFY_INRADIUS },
    {  0.83146961f, -0.55557023f, CLIP_MAGNIFY_INRADIUS },
    {  0.98078528f, -0.19509032f, CLIP_MAGNIFY_INRADIUS },
    {  0.98078528f,  0.19509032f, CLIP_MAGNIFY_INRADIUS },
    {  0.83146961f,  0.55557023f, CLIP_MAGNIFY_INRADIUS },
    {  0.55557023f,  0.83146961f, CLIP_MAGNIFY_INRADIUS },
    {  0.19509032f,  0.98078528f, CLIP_MAGNIFY_INRADIUS },
    { -0.19509032f,  0.98078528f, CLIP_MAGNIFY_INRADIUS },
    { -0.55557023f,  0.83146961f, CLIP_MAGNIFY_INRADIUS },
    { -0.83146961f,  0.55557023f, CLIP_MAGNIFY_INRADIUS },
    { -0.98078528f,  0.19509032f, CLIP_MAGNIFY_INRADIUS },
};
