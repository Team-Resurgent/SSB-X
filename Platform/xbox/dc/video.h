/* dc/video.h -- Xbox shim: the Dreamcast video modes the port names.
 *
 * The port asks for DM_640x480 (KOS resolves it against the cable: 480p on
 * VGA, 480i NTSC elsewhere -- always 60 Hz) in RGB565. The Xbox renderer
 * (Platform/xbox/pvr_xbox.cpp) opens a 640x480 60 Hz back buffer of its
 * own choosing, progressive when the dashboard allows 480p; the game's
 * tic is one 60 Hz frame on both consoles. */
#ifndef XBOX_DC_VIDEO_H
#define XBOX_DC_VIDEO_H

#include <kos/cdefs.h>

__BEGIN_DECLS

typedef enum vid_display_mode_generic {
    DM_GENERIC_FIRST = 0x1000,
    DM_320x240 = 0x1000,
    DM_640x480,
    DM_800x608,
    DM_256x256,
    DM_768x480,
    DM_768x576,
    DM_GENERIC_LAST = DM_768x576
} vid_display_mode_generic_t;

typedef enum vid_pixel_mode {
    PM_RGB555 = 0,
    PM_RGB565 = 1,
    PM_RGB888P = 2,
    PM_RGB0888 = 3,
    PM_RGB888 = PM_RGB0888
} vid_pixel_mode_t;

void vid_set_mode(int dm, vid_pixel_mode_t pm);
void vid_waitvbl(void);
void vid_border_color(int r, int g, int b);

__END_DECLS

#endif /* XBOX_DC_VIDEO_H */
