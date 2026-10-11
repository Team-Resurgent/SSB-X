#ifndef SSB64DC_DCTEXT_H
#define SSB64DC_DCTEXT_H

/* dctext.h -- the port's own text, in the game's own letters.
 *
 * The N64 game only ever draws text it made at build time: every menu
 * label is a sprite of a whole word. The port has things to say that the
 * game never did -- the memory card's notice, its boot check and page --
 * so it needs an alphabet, and the game has exactly one it can spell
 * with: the staff roll's popup textbox (file 195), A-Z, a-z, 0-9 and
 * eleven marks, drawn at 640x480 in the credits. This is that font,
 * loaded once at boot and kept (romdisk/dcfont.spr,
 * sprite_bank_load_resident), laid out the way the credits lay it out:
 * scstaffroll.c:1042-1143 scStaffrollMakeStaffRoleTextSObjs's advance,
 * space, line step and per-glyph baseline drops, over the same glyph
 * table (dSCStaffrollTextBoxSpriteInfo) and the decomp's own index
 * macros (gm/gmdef.h GMSTAFFROLL_*).
 *
 * Two ways to draw it. A scene makes SObjs on a GObj of its own, like
 * any other sprite in the game (dctext_make_sobjs). The port's overlay,
 * which is not a scene and runs after every scene's draw
 * (syTaskmanSetOverlayHook), submits quads straight into the open
 * translucent list (dctext_draw, dctext_fill).
 *
 * Every string the port shows is in src/dc/dcstrings.h. */

#include <ssb_types.h>
#include <PR/sp.h>

struct GObj;

/* The credits' own spacing, in the font's pixels (scstaffroll.c:1036-
 * 1045): a space advances 3, a new line steps 20. */
#define DCTEXT_SPACE 3
#define DCTEXT_LINE  20

/* dctext_index's answers for what is not a glyph */
#define DCTEXT_IS_SPACE   (-1)
#define DCTEXT_IS_NEWLINE (-2)
#define DCTEXT_NO_GLYPH   (-3)

/* One glyph placed: its font index (dSCStaffrollTextBoxSpriteInfo), and
 * its rectangle in the font's pixels from the text's top-left. */
typedef struct DCTextGlyph
{
    s32 index;
    s32 x, y, w, h;
} DCTextGlyph;

/* Load the font. Once, at boot (src/game/ssb64/main.c), before the scene
 * heap and VRAM fill; later calls do nothing. 0, or -1 when the bank
 * could not be loaded -- after which nothing here draws. */
s32 dctext_init(void);

/* The font index of one character, or DCTEXT_IS_SPACE, DCTEXT_IS_NEWLINE
 * or DCTEXT_NO_GLYPH. */
s32 dctext_index(char c);

/* The credits' baseline drop for a glyph, in the font's pixels: small
 * letters sit 3 lower, the tall ones 1, and the period, dash and comma
 * further (scstaffroll.c:986-1033). */
s32 dctext_drop(s32 index);

/* Lay a string out: up to max glyphs into out, in order. Returns how many
 * there are in all, which may be more than max. A character with no
 * glyph advances like a space. */
s32 dctext_layout(const char *s, DCTextGlyph *out, s32 max);

/* The widest line and the whole height, in the font's pixels. */
s32 dctext_width(const char *s);
s32 dctext_height(const char *s);

/* The Sprite for a font index, out of the resident bank; NULL before
 * dctext_init has loaded it. */
Sprite *dctext_sprite(s32 index);

/* A scene's text: one SObj per glyph on gobj (which the caller has given
 * a sprite display, lbCommonDrawSObjAttr), its top-left at x, y in the
 * game's pixels, each glyph `scale` game pixels per font pixel and
 * tinted rgb (0xRRGGBB), the way the credits tint theirs. Returns the
 * number of SObjs made. */
s32 dctext_make_sobjs(struct GObj *gobj, const char *s, f32 x, f32 y,
                      f32 scale, u32 rgb);

/* The overlay's text: quads straight into the translucent list, which
 * must be the one open. x, y and scale are in framebuffer pixels (the
 * port's is 640x480 whatever the scene's resolution), z the PVR depth,
 * argb the tint and opacity. */
void dctext_draw(const char *s, f32 x, f32 y, f32 scale, f32 z, u32 argb);

/* A flat translucent rectangle, the same way. */
void dctext_fill(f32 x0, f32 y0, f32 x1, f32 y1, f32 z, u32 argb);

#endif /* SSB64DC_DCTEXT_H */
