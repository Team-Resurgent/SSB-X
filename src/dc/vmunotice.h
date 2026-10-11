#ifndef SSB64DC_VMUNOTICE_H
#define SSB64DC_VMUNOTICE_H

/* vmunotice.h -- the "SAVING..." notice.
 *
 * A cartridge's SRAM takes a write inside a frame; a VMU takes one or two
 * seconds, and a player who pulls the card or the power in them loses the
 * save. So a Dreamcast game says so while it writes, and this is where the
 * port does: a small translucent box in the screen's bottom-right corner,
 * inside the title-safe area, reading
 *
 *     SAVING...
 *     Do not remove the VMU
 *     or turn off the power.
 *
 * in the staff roll's font (src/dc/dctext.h), for as long as
 * sy_sram_notice_visible says (src/dc/vmusave.h: while the writer thread is
 * busy, and at least a second). The game has no such box; nothing in it
 * is the game's but the letters.
 *
 * It is drawn by the port's overlay layer (src/dc/taskman.h
 * syTaskmanSetOverlayHook), after every scene's own draw and over the
 * frame border, in framebuffer pixels -- so it is the same size and in the
 * same place in a 320x240 scene and in the staff roll's 640x480 -- and
 * never into a photo (the exit photo, the Stage Clear grab), which is a
 * picture of the game. */

/* A rectangle in framebuffer pixels (the port's is 640x480). */
typedef struct VMUNoticeRect
{
    int x0, y0, x1, y1;
} VMUNoticeRect;

/* The screen's title-safe area, the inner 80% (SMPTE's, the one a
 * Dreamcast's TRC holds text to): what a CRT's overscan never hides. */
#define VMUNOTICE_SAFE_X0 64
#define VMUNOTICE_SAFE_Y0 48
#define VMUNOTICE_SAFE_X1 576
#define VMUNOTICE_SAFE_Y1 432

/* Above everything a scene draws, and above the frame border's quads
 * (src/dc/taskman.c syTaskmanDrawBorder, z 10000). */
#define VMUNOTICE_Z 20000.0F

/* The overlay itself: installed as the overlay hook by
 * src/game/ssb64/main.c. Draws on the translucent pass only. */
void vmunotice_overlay(int list);

/* Whether the box is up this frame. */
int vmunotice_visible(void);

/* Where it goes: the box, and the top-left of its three lines. */
void vmunotice_layout(VMUNoticeRect *box, int line_x[3], int line_y[3]);

#endif /* SSB64DC_VMUNOTICE_H */
