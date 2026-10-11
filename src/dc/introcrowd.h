/* introcrowd.h -- the 1P team cards' crowds as baked sprites.
 *
 * The Yoshi Team, Kirby Team and Fighting Polygon Team cards
 * (src/dc/sc1pintro.c) pose 18, 8 and 30 models every frame for a picture
 * that has stopped changing by the end of its first second, and on the
 * console that costs the card most of its frame rate. So the
 * card can draw the finished crowd as a picture instead:
 * tools/export/ssb_introcrowd.py renders each variant on the host, cuts
 * the crowd's box out, and packs it as VQ-compressed ARGB1555 tiles into
 * disc/introcrowd_<card>.bin -- the layout is that tool's docstring.
 *
 * The file is optional. introcrowd_load returns FALSE for a missing file,
 * a variant the file does not hold, or a screen that is not the 640x480 the
 * pictures were made at, and the card then builds its models as it always
 * has. So a build without the files plays the card exactly as before.
 *
 * Which variant a card draws (the `key` in the file):
 *   Yoshi    0, or 1 + the costume when the player is Yoshi -- the three
 *            Yoshis in the player's colour are drawn in the alternate shade
 *   Kirby    (the rolled copy hat's model part id << 1) | 1 when the player
 *            is Kirby in his default costume, which recolours every Kirby
 *   Polygon  0
 *
 * DIVERGES from the card: the crowd is there from the first frame. The game
 * shows one Yoshi, Kirby or polygon pose every few updates until the
 * whole crowd is in (Yoshi 51 updates, Kirby 70, Polygon 62).
 */
#ifndef DC_INTROCROWD_H
#define DC_INTROCROWD_H

#include <ssb_types.h>
#include <sys/obj.h>            /* GObj */

/* "yoshi", "kirby" or "poly", and the variant's key. TRUE when the crowd
 * is loaded and introcrowd_proc_display will draw it. The textures are
 * released with the scene heap (syTaskmanAddHeapResetHook). */
sb32 introcrowd_load(const char *card, u32 key);

/* The card's display proc, for the crowd's camera link: the finished crowd
 * as quads in the translucent pass, at the next sprite depth, which is where
 * the layered models it replaces draw. */
void introcrowd_proc_display(GObj *gobj);

#endif /* DC_INTROCROWD_H */
