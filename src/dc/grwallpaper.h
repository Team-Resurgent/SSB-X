/* grwallpaper.h -- where gr/grwallpaper.c puts a stage's background
 * sprite each frame, as the numbers alone: its top-left in N64 screen
 * pixels and its scale. Header-only so that the host tests run the same
 * lines src/dc/stage.c draws the wallpaper quad with, without its PVR
 * dependencies.
 *
 * grWallpaperMakeDecideKind (grwallpaper.c:267-302) gives a VS stage one
 * of three wallpapers, and each keeps its sprite placed its own way:
 *   - Yoshi's Island (and the small one, and the bonus stages):
 *     grWallpaperMakeStatic, fixed at (10, 10) and scale 1.0;
 *   - Sector Z: grWallpaperMakeSector, whose process
 *     grWallpaperSectorProcUpdate zooms it about the screen's centre;
 *   - every other stage: grWallpaperMakeCommon, whose process
 *     grWallpaperCalcPersp pans it with the view angles as it zooms.
 */
#ifndef SSB_DC_GRWALLPAPER_H
#define SSB_DC_GRWALLPAPER_H

#include <math.h>
#include <gr/grdef.h>
#include <sys/utils.h>

/* grwallpaper.c:45-123 grWallpaperCalcPersp 0x80104620: scale from the
 * eye's distance, pan from its angles, both clamped so the 300x220 sprite
 * covers the screen inside the 10-pixel border. `d` is eye - at. */
static inline void stage_wallpaper_persp(float dx, float dy, float dz,
                                        float *pos_x, float *pos_y,
                                        float *scale_out)
{
    float mag = sqrtf(dx * dx + dy * dy + dz * dz);
    float angle_x, angle_y, scale, width, height, px, py, neg;

    if (dz < 0.0f)
    {
        angle_x = angle_y = 0.0f;
    }
    else
    {
        angle_y = syUtilsArcTan2(dx, dz);
        angle_x = syUtilsArcTan2(dy, dz);
    }
    scale = 20000.0f / (mag + 8000.0f);

    if (scale < 1.004f)
    {
        scale = 1.004f;
    }
    else if (scale > 2.0f)
    {
        scale = 2.0f;
    }
    width = 300.0f * scale;
    height = 220.0f * scale;

    px = ((angle_y / F_CST_DTOR32(180.0F)) * width) - ((width - 320.0f) * 0.5f);
    py = ((-angle_x / F_CST_DTOR32(180.0F)) * height) - ((height - 240.0f) * 0.5f);

    if (px > 10.0f)
    {
        px = 10.0f;
    }
    else
    {
        neg = (-width - 10.0f) + 320.0f;

        if (px < neg)
        {
            px = neg;
        }
    }
    if (py > 10.0f)
    {
        py = 10.0f;
    }
    else
    {
        neg = (-height - 10.0f) + 240.0f;

        if (py < neg)
        {
            py = neg;
        }
    }
    *pos_x = px;
    *pos_y = py;
    *scale_out = scale;
}

/* grwallpaper.c:192-228 grWallpaperSectorProcUpdate 0x80104998: the same
 * zoom with a longer falloff, about the centre, and no pan. A zero
 * distance leaves the sprite where grWallpaperMakeSector put it. */
static inline void stage_wallpaper_sector(float dx, float dy, float dz,
                                               float *pos_x, float *pos_y,
                                               float *scale_out)
{
    float sqrt_d = sqrtf(dx * dx + dy * dy + dz * dz);
    float temp, scale;

    *pos_x = 10.0f;
    *pos_y = 10.0f;
    *scale_out = 1.0f;

    if (sqrt_d > 0.0f)
    {
        temp = 20000.0f / (sqrt_d + 10000.0f);

        if (temp < 1.004f)
        {
            temp = 1.004f;
        }
        else if (temp > 2.0f)
        {
            temp = 2.0f;
        }
        scale = (temp - 1.0f) * 0.5f;

        *scale_out = temp;
        *pos_x = 10.0f - (300.0f * scale);
        *pos_y = 10.0f - (220.0f * scale);
    }
}

/* grwallpaper.c:267-302 grWallpaperMakeDecideKind's VS arms, as the
 * placement the wallpaper each picks is kept at. */
static inline void stage_wallpaper_place(int gkind, float dx, float dy,
                                           float dz, float *pos_x,
                                           float *pos_y, float *scale)
{
    if ((gkind >= nGRKindBonusStageStart) || (gkind == nGRKindYoster) ||
        (gkind == nGRKindYosterSmall))
    {
        /* grwallpaper.c:162-189 grWallpaperMakeStatic 0x801048F8 */
        *pos_x = 10.0f;
        *pos_y = 10.0f;
        *scale = 1.0f;
    }
    else if (gkind == nGRKindSector)
    {
        stage_wallpaper_sector(dx, dy, dz, pos_x, pos_y, scale);
    }
    else stage_wallpaper_persp(dx, dy, dz, pos_x, pos_y, scale);
}

#endif /* SSB_DC_GRWALLPAPER_H */
