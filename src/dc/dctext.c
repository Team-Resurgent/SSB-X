/* dctext.c -- see dctext.h. */
#include "dctext.h"
#include "dcstrings.h"

#include <string.h>

#include <sys/obj.h>
#include <sys/debug.h>
#include <gm/gmdef.h>

#include "lbcommon.h"   /* lbCommonMakeSObjForGObj */
#include "objpvr.h"     /* gcGetDrawList */
#include "perf.h"
#include "scstaffroll.h" /* dSCStaffrollTextBoxSpriteInfo */
#include "sprite.h"

#ifndef FT_HOSTTEST
#include <dc/pvr.h>
#endif

#define DCSTR_TEXT(id, text) text,
const char *const gDCStrings[nDCStrCount] = { DCSTRINGS(DCSTR_TEXT) };
#undef DCSTR_TEXT

#define DCTEXT_GLYPHS 74

static SpriteBank sDCTextBank;
static s32 sDCTextState; /* 0 not tried, 1 loaded, -1 failed */

s32 dctext_init(void)
{
    if (sDCTextState == 0)
    {
        sDCTextState =
            (sprite_bank_load_resident(&sDCTextBank, "dcfont.spr") == 0) ? 1 : -1;
    }
    return (sDCTextState == 1) ? 0 : -1;
}

s32 dctext_index(char c)
{
    if (((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')))
    {
        return GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX(c);
    }
    if ((c >= '0') && (c <= '9'))
    {
        return GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX(c);
    }
    switch (c)
    {
    case ' ':  return DCTEXT_IS_SPACE;
    case '\n': return DCTEXT_IS_NEWLINE;
    case ':':  return GMSTAFFROLL_COLON_PARA_FONT_INDEX;
    case '.':  return GMSTAFFROLL_PERIOD_PARA_FONT_INDEX;
    case '-':  return GMSTAFFROLL_DASH_PARA_FONT_INDEX;
    case ',':  return GMSTAFFROLL_COMMA_PARA_FONT_INDEX;
    case '&':  return GMSTAFFROLL_AMPERSAND_PARA_FONT_INDEX;
    case '"':  return GMSTAFFROLL_DOUBLE_QUOTES_PARA_FONT_INDEX;
    case '/':  return GMSTAFFROLL_SLASH_PARA_FONT_INDEX;
    case '\'': return GMSTAFFROLL_APOSTROPHE_PARA_FONT_INDEX;
    case '?':  return GMSTAFFROLL_QUESTION_MARK_PARA_FONT_INDEX;
    case '(':  return GMSTAFFROLL_OPEN_PARENTHESIS_PARA_FONT_INDEX;
    case ')':  return GMSTAFFROLL_CLOSE_PARENTHESIS_PARA_FONT_INDEX;
    default:   return DCTEXT_NO_GLYPH;
    }
}

/* scstaffroll.c:986-1033, the body of scStaffrollMakeStaffRoleTextSObjs's
 * loop that picks hvar, with the glyph as an argument rather than
 * dSCStaffrollStaffRoleCharacters[character_id]. */
s32 dctext_drop(s32 index)
{
    s32 hvar = 0;

    if (index >= GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('a'))
    {
        hvar = 3;

        if
        (
            index == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('b')      ||
            index == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('d')      ||
            index == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('f')      ||
            index == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('h')      ||
            index == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('i')      ||
            index == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('j')      ||
            index == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('k')      ||
            index == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('l')      ||
            index == GMSTAFFROLL_ASCII_LETTER_TO_FONT_INDEX('t')      ||
            index == GMSTAFFROLL_COLON_PARA_FONT_INDEX                ||
            index == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('9') ||
            index == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('8') ||
            index == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('7') ||
            index == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('6') ||
            index == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('5') ||
            index == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('4') ||
            index == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('3') ||
            index == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('2') ||
            index == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('1') ||
            index == GMSTAFFROLL_ASCII_NUMBER_TO_PARA_FONT_INDEX('0') ||
            index == GMSTAFFROLL_AMPERSAND_PARA_FONT_INDEX            ||
            index == GMSTAFFROLL_QUESTION_MARK_PARA_FONT_INDEX        ||
            index == GMSTAFFROLL_E_ACCENT_PARA_FONT_INDEX             ||
            index == GMSTAFFROLL_DOUBLE_QUOTES_PARA_FONT_INDEX
        )
        {
            hvar = 1;
        }
    }
    if (index == GMSTAFFROLL_PERIOD_PARA_FONT_INDEX)
    {
        hvar += 6;
    }
    if (index == GMSTAFFROLL_DASH_PARA_FONT_INDEX)
    {
        hvar += 2;
    }
    if (index == GMSTAFFROLL_COMMA_PARA_FONT_INDEX)
    {
        hvar += 7;
    }
    return hvar;
}

/* scStaffrollMakeStaffRoleTextSObjs's walk (scstaffroll.c:966-1045):
 * each glyph advances by its table width, a space by 3, a new line goes
 * back to the left edge 20 lower. */
s32 dctext_layout(const char *s, DCTextGlyph *out, s32 max)
{
    s32 wbase = 0, hbase = 0, n = 0;

    for (; *s != '\0'; s++)
    {
        s32 index = dctext_index(*s);

        if (index >= 0)
        {
            const SCStaffrollSprite *g = &dSCStaffrollTextBoxSpriteInfo[index];

            if (n < max)
            {
                out[n].index = index;
                out[n].x = wbase;
                out[n].y = hbase + dctext_drop(index);
                out[n].w = g->width;
                out[n].h = g->height;
            }
            n++;
            wbase += g->width;
        }
        else if (index == DCTEXT_IS_NEWLINE)
        {
            wbase = 0;
            hbase += DCTEXT_LINE;
        }
        else
        {
            wbase += DCTEXT_SPACE;
        }
    }
    return n;
}

s32 dctext_width(const char *s)
{
    s32 wbase = 0, widest = 0;

    for (; *s != '\0'; s++)
    {
        s32 index = dctext_index(*s);

        if (index >= 0)
        {
            wbase += dSCStaffrollTextBoxSpriteInfo[index].width;
        }
        else if (index == DCTEXT_IS_NEWLINE)
        {
            wbase = 0;
        }
        else
        {
            wbase += DCTEXT_SPACE;
        }
        if (wbase > widest)
        {
            widest = wbase;
        }
    }
    return widest;
}

s32 dctext_height(const char *s)
{
    s32 lines = 1;

    for (; *s != '\0'; s++)
    {
        if (*s == '\n')
        {
            lines++;
        }
    }
    /* the last line's tallest glyph is 14, dropped by at most 10 */
    return ((lines - 1) * DCTEXT_LINE) + 14;
}

Sprite *dctext_sprite(s32 index)
{
    if ((sDCTextState != 1) || (index < 0) || (index >= DCTEXT_GLYPHS))
    {
        return NULL;
    }
    return sprite_bank_get(&sDCTextBank,
                           (u32)dSCStaffrollTextBoxSpriteInfo[index].offset);
}

s32 dctext_make_sobjs(GObj *gobj, const char *s, f32 x, f32 y, f32 scale,
                      u32 rgb)
{
    DCTextGlyph glyphs[128];
    s32 n = dctext_layout(s, glyphs, ARRAY_COUNT(glyphs));
    s32 i, made = 0;

    if (n > ARRAY_COUNT(glyphs))
    {
        n = ARRAY_COUNT(glyphs);
    }
    for (i = 0; i < n; i++)
    {
        Sprite *sprite = dctext_sprite(glyphs[i].index);
        SObj *sobj;

        if ((sprite == NULL) ||
            ((sobj = lbCommonMakeSObjForGObj(gobj, sprite)) == NULL))
        {
            continue;
        }
        /* the credits' own attributes (scstaffroll.c:976-986) */
        sobj->sprite.attr = SP_TRANSPARENT;
        sobj->sprite.red = (rgb >> 16) & 0xFF;
        sobj->sprite.green = (rgb >> 8) & 0xFF;
        sobj->sprite.blue = rgb & 0xFF;
        sobj->sprite.scalex = sobj->sprite.scaley = scale;
        sobj->pos.x = x + (glyphs[i].x * scale);
        sobj->pos.y = y + (glyphs[i].y * scale);
        made++;
    }
    return made;
}

#ifndef FT_HOSTTEST

static void dctext_quad(f32 x0, f32 y0, f32 x1, f32 y1, f32 u1, f32 v1,
                        f32 z, u32 argb)
{
    pvr_vertex_t v;
    int i;

    v.oargb = 0;
    v.argb = argb;
    v.z = z;
    for (i = 0; i < 4; i++)
    {
        v.flags = (i == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = (i & 2) ? x1 : x0;
        v.y = (i & 1) ? y0 : y1;
        v.u = (i & 2) ? u1 : 0.0F;
        v.v = (i & 1) ? 0.0F : v1;
        pvr_prim(&v, sizeof(v));
    }
}

void dctext_draw(const char *s, f32 x, f32 y, f32 scale, f32 z, u32 argb)
{
    DCTextGlyph glyphs[128];
    s32 n, i;

    if ((sDCTextState != 1) || (gcGetDrawList() != PVR_LIST_TR_POLY))
    {
        return;
    }
    n = dctext_layout(s, glyphs, ARRAY_COUNT(glyphs));
    if (n > ARRAY_COUNT(glyphs))
    {
        n = ARRAY_COUNT(glyphs);
    }
    for (i = 0; i < n; i++)
    {
        DCSpriteEntry *e = sprite_bank_entry(
            &sDCTextBank, (u32)dSCStaffrollTextBoxSpriteInfo[glyphs[i].index].offset);
        const DCSpriteTex *t;
        pvr_poly_cxt_t cxt;
        pvr_poly_hdr_t hdr;
        f32 gx, gy;

        if (e == NULL)
        {
            continue;
        }
        t = &e->texs[0];
        /* one texture a glyph: a header each, as lbcommon.c's sprite
         * quads have (lbCommonSpriteSubmitHeader) -- point-sampled at
         * whole scales, where the credits' pixels land on the screen's */
        pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
                         (t->fmt == nDCSpriteTexFmtARGB1555) ? PVR_TXRFMT_ARGB1555
                                                             : PVR_TXRFMT_ARGB4444,
                         t->texw, t->texh, t->txr,
                         (scale == (f32)(s32)scale) ? PVR_FILTER_NONE
                                                    : PVR_FILTER_BILINEAR);
        cxt.gen.culling = PVR_CULLING_NONE;
        cxt.txr.env = PVR_TXRENV_MODULATEALPHA;
        cxt.txr.uv_clamp = PVR_UVCLAMP_UV;
        cxt.blend.src = PVR_BLEND_SRCALPHA;
        cxt.blend.dst = PVR_BLEND_INVSRCALPHA;
        cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
        cxt.depth.write = PVR_DEPTHWRITE_DISABLE;
        DBPERF_COMPILE();
        pvr_poly_compile(&hdr, &cxt);
        pvr_prim(&hdr, sizeof(hdr));

        gx = x + (glyphs[i].x * scale);
        gy = y + (glyphs[i].y * scale);
        dctext_quad(gx, gy, gx + (t->imgw * scale), gy + (t->imgh * scale),
                    (f32)t->imgw / t->texw, (f32)t->imgh / t->texh, z, argb);
    }
}

void dctext_fill(f32 x0, f32 y0, f32 x1, f32 y1, f32 z, u32 argb)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;

    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.blend.src = PVR_BLEND_SRCALPHA;
    cxt.blend.dst = PVR_BLEND_INVSRCALPHA;
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    cxt.depth.write = PVR_DEPTHWRITE_DISABLE;
    DBPERF_COMPILE();
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));
    dctext_quad(x0, y0, x1, y1, 0.0F, 0.0F, z, argb);
}

#else /* FT_HOSTTEST */

void dctext_draw(const char *s, f32 x, f32 y, f32 scale, f32 z, u32 argb)
{
    (void)s; (void)x; (void)y; (void)scale; (void)z; (void)argb;
}

void dctext_fill(f32 x0, f32 y0, f32 x1, f32 y1, f32 z, u32 argb)
{
    (void)x0; (void)y0; (void)x1; (void)y1; (void)z; (void)argb;
}

#endif /* FT_HOSTTEST */
