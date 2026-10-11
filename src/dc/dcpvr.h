#ifndef SSB_DC_DCPVR_H
#define SSB_DC_DCPVR_H

/* ssb64-dc: how this port starts the PVR.
 *
 * KOS's pvr_init_defaults() enables two lists -- opaque and translucent --
 * and leaves the punch-through OPB at length 0, which disables it. The
 * port needs a third: an N64 render mode with CVG_X_ALPHA and no FORCE_BL
 * is an alpha cutout, not a blend, and punch-through is the PVR's name for
 * exactly that -- alpha tested against PVR_PT_ALPHA_REF, depth written,
 * no per-pixel sort. Kongo Jungle's trees are drawn that way
 * (G_RM_AA_ZB_TEX_EDGE) and so is foliage and fencing on five of the other
 * six exported stages; through the translucent list they would sort wrong
 * against each other, and through the opaque list -- where KOS's
 * pvr_poly_cxt_txr turns blending off outright -- they were solid quads,
 * which is the bug this exists to fix.
 *
 * The bin size matches the other two lists. The cost is one more object
 * pointer buffer per tile per frame: 16 words * 4 bytes * 300 tiles at
 * 640x480, doubled for the two frames and multiplied by the overflow
 * count, out of 8 MB of video RAM.
 *
 * PVR_PT_ALPHA_REF is the punch-through alpha test's threshold, and KOS
 * has no wrapper for it and never writes it, so the register keeps
 * whatever the boot ROM left. 0x80 is the middle: an ARGB1555 texel is
 * 0 or 255 and lands either side of it whatever the value, and an
 * ARGB4444 one keeps its upper half. That is the N64's own cutout rule,
 * where CVG_X_ALPHA multiplies coverage by alpha and the pixel survives
 * on the half of the range this threshold keeps.
 */

#include <dc/pvr.h>

/* Off the SH-4 there is no PVR: src/game/ssb64/hoststubs/dc/pvr.h carries
 * the two types fighter.h and stage.h name and nothing else, and the host
 * suite compiles the loaders that call the fence below against it. So the
 * two functions here are the target's, and the fence is a no-op off it --
 * the same split assetroot.c makes, and for the same reason. */
#ifdef _arch_dreamcast

/* The TA's vertex buffer, named so the frame loop's -DDB_PVR_BUDGET
 * probe can say what fraction of it a frame spent. Every parameter every
 * pass submits goes here, and the hardware stops storing at the end of
 * it (src/dc/taskman.c). It costs TWICE its size in texture RAM: KOS
 * lays a copy at 0 and another at 0x400000, and texture memory is what
 * is left of the 64-bit space above both. */
#define DC_PVR_VERTEX_BUF_SIZE (512 * 1024)

static inline int dc_pvr_init(void)
{
    static const pvr_init_params_t params = {
        .opb_sizes = { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_16,
                       PVR_BINSIZE_0, PVR_BINSIZE_16 },
        .vertex_buf_size = DC_PVR_VERTEX_BUF_SIZE,
        .opb_overflow_count = 3,
    };
    int rv = pvr_init(&params);

    if (rv == 0)
    {
        PVR_SET(PVR_PT_ALPHA_REF, 0x80);
    }
    return rv;
}

/* Before any write to video RAM -- a texel upload, a palette entry, a
 * pvr_mem_free -- wait for the renderer to stop reading it.
 *
 * The frame is update-then-draw (src/dc/taskman.c syTaskmanRunFrame), and
 * pvr_wait_ready() is the FIRST call of the draw. So every load the update
 * does -- a fighter pack's textures and palettes, a stage's wallpaper, a
 * particle bank, a sprite bank, and every pvr_mem_free that precedes one --
 * runs while the PVR is still rendering the frame the previous draw
 * submitted, out of the very memory being overwritten. The TA collects the
 * next scene into its own vertex buffer, which is why the wait at the top
 * of the draw is not this wait: it is "ready to accept a scene", and a
 * scene can be accepted while the previous one is still rendering.
 *
 * On hardware the stage select is the worst of it -- every cursor move
 * frees a stage's textures and loads another's. Symptom: a polygon drawn
 * from half-freed or half-written texels, for one frame, wherever the load
 * happened to land.
 *
 * A render in flight is not the only reader. pvr_scene_finish does not
 * start the render: KOS starts it from the interrupt that sees the lists
 * in, and only once the frame before has been flipped onto the screen,
 * which happens at a vertical blank (pvr_irq.c pvr_render_lists). So for
 * up to a retrace after every pvr_scene_finish the scene sits queued --
 * the TA busy, the renderer idle -- and pvr_wait_render_done() alone, which
 * waits on the renderer, returns at once. The update runs in exactly that
 * retrace, and so does a scene change: the exit tic's releases and the next
 * scene's first uploads went into VRAM the old scene's last frame then
 * rendered from, and that frame stays on screen for the whole load -- the
 * dots and smears on every transition (the deferral is KOS's own). The
 * KOS allocator writes its
 * chunk headers into VRAM itself (pvr_mem_core.c is dlmalloc), so even a
 * bare pvr_mem_free is a write. -DDB_FENCE_TRACE prints each fence with
 * what it found.
 *
 * So outside a scene the fence first waits for a queued render to start
 * (pvr_wait_ready: the TA is busy until then), then for it to finish.
 * Inside one -- a display proc writing a palette (objmodel.c
 * dc_joint_material) -- the TA is busy with the scene being drawn, which
 * cannot start rendering until the draw finishes, and the frame before it
 * was already started by the draw's own pvr_wait_ready; there the render
 * wait alone is right. The frame loop says which (src/dc/taskman.c
 * syTaskmanDrawPasses).
 *
 * The cost: nothing when no scene is queued or rendering, and at most the
 * rest of a retrace plus one render when one is. Idempotent, so a loader
 * may call it without knowing whether its caller already did.
 *
 * tools/check/vramfence_check.py holds every call site to this rule.
 */
extern int gDCPVRSceneOpen;

static inline void dc_pvr_vram_fence(void)
{
    if (!gDCPVRSceneOpen)
    {
        pvr_wait_ready();
    }
    pvr_wait_render_done();
}

#ifdef DB_FENCE_TRACE
#include <kos/dbglog.h>
#include <arch/timer.h>

/* -DDB_FENCE_TRACE, off by default: one line per fence, naming the caller
 * and what it found -- "queued" a submitted scene whose render has not
 * started, "in scene" a fence inside the draw, "idle" neither. */
static inline void dc_pvr_vram_fence_traced(const char *who)
{
    const char *found = gDCPVRSceneOpen ? "in scene"
                        : pvr_check_ready() != 0 ? "queued" : "idle";
    uint64_t t0 = timer_us_gettime64();
    int ready = gDCPVRSceneOpen ? 0 : pvr_wait_ready();
    uint64_t t1 = timer_us_gettime64();
    int done = pvr_wait_render_done();
    uint64_t t2 = timer_us_gettime64();

    dbglog(DBG_INFO, "fence: %s %s; ready %d in %u us, done %d in %u us, "
           "after %s\n", who, found, ready, (unsigned)(t1 - t0), done,
           (unsigned)(t2 - t1),
           pvr_check_ready() != 0 ? "queued" : "clear");
}
#define dc_pvr_vram_fence() dc_pvr_vram_fence_traced(__func__)
#endif

#else  /* !_arch_dreamcast */

static inline void dc_pvr_vram_fence(void)
{
}

#endif /* _arch_dreamcast */

#endif /* SSB_DC_DCPVR_H */
