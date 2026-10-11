/* vmunotice.c -- see vmunotice.h. */
#include "vmunotice.h"

#include <ssb_types.h>

#include "dcstrings.h"
#include "dctext.h"
#include "vmusave.h"
#include "scmanager.h"
#include "scport.h"

#ifndef FT_HOSTTEST
#include <dc/pvr.h>
#endif

/* The box's margin round the text, and its frame's width, in framebuffer
 * pixels: the frame is one of the game's pixels at 320x240. */
#define VMUNOTICE_PAD   10
#define VMUNOTICE_FRAME 2

/* The panel, its frame, the first line and the other two: the colours of
 * the game's own menu text on a dark translucent slate. */
#define VMUNOTICE_RGBA_PANEL 0xC0101830
#define VMUNOTICE_RGBA_FRAME 0xE0B7BCEC
#define VMUNOTICE_RGBA_TITLE 0xFFFFFFFF
#define VMUNOTICE_RGBA_WARN  0xFFB7BCEC

static const int sLines[3] = { nDCStrSaving, nDCStrSavingWarn,
                               nDCStrSavingPower };

/* the first line, by what the card is doing */
static int notice_line(int i)
{
    return ((i == 0) && (sy_sram_notice_is_load() != 0)) ? nDCStrLoading
                                                         : sLines[i];
}

void vmunotice_layout(VMUNoticeRect *box, int line_x[3], int line_y[3])
{
    int w = 0, h, i;

    for (i = 0; i < 3; i++)
    {
        int lw = dctext_width(gDCStrings[sLines[i]]);
        int lw2 = dctext_width(gDCStrings[nDCStrLoading]);

        if ((i == 0) && (lw2 > lw))
        {
            lw = lw2; /* a box the same size for either word */
        }

        if (lw > w)
        {
            w = lw;
        }
    }
    /* three lines at the credits' own step (dctext_height's measure of
     * "a\nb\nc") */
    h = (2 * DCTEXT_LINE) + dctext_height("");

    box->x1 = VMUNOTICE_SAFE_X1;
    box->y1 = VMUNOTICE_SAFE_Y1;
    box->x0 = box->x1 - w - (2 * VMUNOTICE_PAD);
    box->y0 = box->y1 - h - (2 * VMUNOTICE_PAD);
    for (i = 0; i < 3; i++)
    {
        line_x[i] = box->x0 + VMUNOTICE_PAD;
        line_y[i] = box->y0 + VMUNOTICE_PAD + (i * DCTEXT_LINE);
    }
}

int vmunotice_visible(void)
{
    /* The memory card's own page says what the card is doing, where the
     * box would sit over its list (src/dc/dcmemcard.h). */
    if (gSCManagerSceneData.scene_curr == nSCKindDCMemCard)
    {
        return 0;
    }
    return sy_sram_notice_visible();
}

void vmunotice_overlay(int list)
{
#ifndef FT_HOSTTEST
    VMUNoticeRect b;
    int lx[3], ly[3], i;
    f32 z = VMUNOTICE_Z;

    if ((list != PVR_LIST_TR_POLY) || (vmunotice_visible() == 0))
    {
        return;
    }
    vmunotice_layout(&b, lx, ly);

    dctext_fill(b.x0, b.y0, b.x1, b.y1, z, VMUNOTICE_RGBA_PANEL);
    /* the frame: top, bottom, left, right, over the panel */
    dctext_fill(b.x0, b.y0, b.x1, b.y0 + VMUNOTICE_FRAME, z + 1.0F,
                VMUNOTICE_RGBA_FRAME);
    dctext_fill(b.x0, b.y1 - VMUNOTICE_FRAME, b.x1, b.y1, z + 1.0F,
                VMUNOTICE_RGBA_FRAME);
    dctext_fill(b.x0, b.y0 + VMUNOTICE_FRAME, b.x0 + VMUNOTICE_FRAME,
                b.y1 - VMUNOTICE_FRAME, z + 1.0F, VMUNOTICE_RGBA_FRAME);
    dctext_fill(b.x1 - VMUNOTICE_FRAME, b.y0 + VMUNOTICE_FRAME, b.x1,
                b.y1 - VMUNOTICE_FRAME, z + 1.0F, VMUNOTICE_RGBA_FRAME);
    for (i = 0; i < 3; i++)
    {
        dctext_draw(gDCStrings[notice_line(i)], lx[i], ly[i], 1.0F, z + 2.0F,
                    (i == 0) ? VMUNOTICE_RGBA_TITLE : VMUNOTICE_RGBA_WARN);
    }
#else
    (void)list;
#endif
}
