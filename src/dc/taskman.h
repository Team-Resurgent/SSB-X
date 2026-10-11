/* taskman.h -- the port's side of the game's task manager.
 *
 * sys/taskman.c is the N64 frame driver: it builds OSTasks, display-list
 * and RDP output buffers, loads RSP ucodes and drives the scheduler.
 * Almost none of that survives the move -- there is no RSP, no RDP and no
 * OSTask here -- so unlike mp/mpcollision.c this file is a *rewrite behind
 * a ported interface*, not a port: scene code keeps calling the names the
 * decomp declares in sys/taskman.h, and what happens underneath is the
 * Dreamcast's.
 *
 * The exception is the memory model, which is portable and is what this
 * file carries first. The game gives each scene one bump-allocated heap:
 * syTaskmanMalloc hands out aligned blocks and nothing is ever freed
 * individually -- the scene manager resets the whole region when the scene
 * changes. sys/malloc.c, the allocator itself, compiles unmodified out of
 * the decomp.
 *
 * The two entry points below are the port's own, and stand in for what the
 * N64 got from its link map: on the N64 the general heap is simply the RAM
 * after the scene overlay's BSS (SYTaskmanSetup.heap_start is
 * &ovl4_BSS_END in dSCVSBattleTaskmanSetup), which has no meaning in an
 * ELF that KallistiOS has already laid out.
 */
#ifndef SSB_DC_TASKMAN_H
#define SSB_DC_TASKMAN_H

#include <stddef.h>

#include <sys/obj.h>
#include <sys/taskman.h>

/* Take `size` bytes from the C heap and make them the scene's general
 * heap, as syTaskmanStartTask does with the pool SYTaskmanSetup names
 * (sys/taskman.c:1227-1285). Returns 0, or -1 with the reason on dbglog.
 * Calling it again releases the previous region. */
int syTaskmanMakeGeneralHeap(size_t size);

/* Put the general heap back to empty, the way the scene manager does
 * between scenes. Everything syTaskmanMalloc handed out is dead after
 * this -- drop the pointers first. */
void syTaskmanResetGeneralHeap(void);

/* Called by syTaskmanResetGeneralHeap just before the heap goes, so a
 * module that keeps state in it can let go while its pointers are still
 * good. Four modules install themselves, five hooks between them:
 * src/dc/sprite.c, whose bank records live in the scene heap and whose
 * "already loaded" flag is what ftManagerSetupFilesAllKind tests;
 * src/dc/ftmanager.c, whose packs are loaded per scene,
 * whose per-kind attributes are cut from the heap, and whose fighter
 * slots hold clip buffers that are not; and src/dc/fighter.c,
 * whose scene fighters are registered in it (fighter_scene_release_all);
 * and src/dc/introcrowd.c, whose baked-crowd textures are in PVR memory.
 * A hook past the table's end aborts rather than being dropped. The table
 * is a tripwire, not a tuned size: it was four, exactly the hooks there
 * were, and the fifth was found on the real 1P ladder (a boot straight
 * into the card had not run the scenes that install the others). The
 * decomp has nothing like it --
 * the game's files come back off the cartridge and the flag is the
 * reloc loader's, cleared with the overlay.
 *
 * They are pointers and not direct calls because taskman.c is linked
 * into the animation oracle without the renderer beside it
 * (tools/check/figatree_check.py), and that build widens f32 to double, which
 * sprite.c's record-size assertions will not have. None installed
 * there, and nothing happens. Installing the same hook twice keeps
 * one; the order they run in is the order they were installed. */
#define SY_TASKMAN_HEAP_RESET_HOOKS 8
void syTaskmanAddHeapResetHook(void (*hook)(void));

/* Called before each of syTaskmanCommonTaskDraw's passes, the way the
 * N64 reset its display-list heads before each frame. src/dc/wpmanager.c
 * installs one to put gSYTaskmanDLHeads back at the start of its scratch
 * buffer. A pointer and not a direct call for the same reason as the
 * heap-reset hooks: the oracles that link this file have no heads, or
 * their own. Installing another replaces it. */
void syTaskmanSetDrawPassHook(void (*hook)(void));

/* Called once at the end of every tic syTaskmanRunFrame runs, drawn or
 * not, including the tic that ends the scene -- the port's one frame
 * boundary. src/game/ssb64/main.c installs the save store's pump
 * (src/dc/vmusave.h): lb/lbbackup.c's write lands in RAM inside a tic
 * and the memory card hears of it here. A pointer for the heap-reset
 * hooks' reason: the oracles link taskman.c without the store. Installing
 * another replaces it. */
void syTaskmanSetFrameEndHook(void (*hook)(void));

/* Called on each of a frame's three PVR passes, after the scene's draw
 * and the border, with that pass's list still open -- the port's own
 * layer over the game, for what the port has to tell the player that the
 * game never did (the memory card's "SAVING..." notice). `list` is
 * the PVR_LIST_* open. Only for a frame going to the screen: a frame
 * drawn into the photo (syTaskmanWantPhoto, syTaskmanWantExitPhoto) is a
 * picture of the game and the scene after it shows it as one, so the
 * layer never lands in it. Installing another replaces it. */
void syTaskmanSetOverlayHook(void (*hook)(int list));

/* The one thing that hook is ever set to, declared here rather than in a
 * port wpmanager header the port does not have: gSYTaskmanDLHeads and
 * their scratch live in src/dc/wpmanager.c (that file's own comment says
 * why -- tools/check/lbparticle_check.py's oracle links taskman.c beside a
 * driver that defines the heads itself, so taskman.c must not name
 * anything of wpmanager's), but the reset has to be installed for every
 * scene, not only for one that allocates weapons. An unreset head walks out of that scratch and through .bss. Called once, at boot. */
void wpManagerInitDLHeads(void);

/* Bytes handed out so far, and the region's size. The game had no reason
 * to ask; the port does, because the size is a guess until a scene has
 * run and syMallocSet's overflow path (sys/malloc.c:28-31) is a printf
 * and then a hang. src/dc/db.c logs these. */
size_t syTaskmanGeneralHeapUsed(void);
size_t syTaskmanGeneralHeapSize(void);

/* Which emptying of the region this is: it changes every time the region
 * is initialised or reset, so a pointer the port cached out of it can be
 * told apart from one into a region that has since been handed out
 * again. src/dc/objmodel.c keys a pack's joint payloads on it. */
u32 syTaskmanGeneralHeapEpoch(void);

/* One turn of the game's frame loop: the controller read, the scene's
 * update, and -- on every syTaskmanSetIntervals framedraw-th tic, unless
 * the update ended the scene -- the scene's draw. Returns TRUE on a tic
 * that drew.
 *
 * The decomp has no such function: its body is inline in syTaskmanRunTask
 * (sys/taskman.c:973-1049), twice. It is broken out here so a caller that
 * drives its own tics can have one -- the host test does. Scene code
 * never calls it; syTaskmanRunTask does.
 *
 * The frame's wall-clock cost lands in dSYTaskmanUpdateTimeDelta and
 * dSYTaskmanFrameTimeDelta, in microseconds. */
sb32 syTaskmanRunFrame(void);

extern u32 dSYTaskmanUpdateTimeDelta;
extern u32 dSYTaskmanFrameTimeDelta;

/* The port's own: counts up on every tic syTaskmanRunFrame runs and on
 * every vblank sySchedulerWaitTicCount sleeps through, in every scene,
 * so it stops only when the game thread does. The hang watchdog's
 * heartbeat (src/dc/db.c); nothing else reads it. */
extern volatile u32 gSYTaskmanAlive;

/* sys/scheduler.c:109-118, the retrace counter's two accessors. The
 * decomp declares them nowhere -- each caller writes its own extern
 * (mn/mncommon/mntitle.c:11) -- so this is the port's declaration, put
 * here because taskman.c is where the port's tic is counted. The match
 * clock is the only reader in the ported game (src/dc/ifcommon.c). */
void sySchedulerSetTicCount(u32 tics);
u32 sySchedulerGetTicCount(void);
void sySchedulerWaitTicCount(u32 target);

/* sys/taskman.c:946 syTaskmanRunTask, which the decomp's own taskman.h
 * does not declare either: the scene's frame loop. Runs tics until
 * syTaskmanCheckBreakLoop, then returns to the scene manager.
 * syTaskmanStartTask ends here, so starting a scene does not return
 * until that scene is over. */
void syTaskmanRunTask(void);

/* The pool half of syTaskmanStartTask, without the frame loop after it:
 * cut the object system's pools out of the scene heap and thread them
 * into gcSetupObjman's free lists, and stop. The decomp has no such
 * split -- see the DIVERGES on it in taskman.c. Only the host test wants
 * it; a scene calls syTaskmanStartTask. */
void syTaskmanSetupPools(SYTaskmanSetup *tsetup);

/* sys/taskman.c:904 syTaskmanCheckBreakLoop, which the decomp's own
 * taskman.h does not declare: TRUE once the scene has called
 * syTaskmanSetLoadScene (sys/taskman.h:101) and wants to be left. In the
 * decomp that is the `break` out of syTaskmanRunTask's while (TRUE); here
 * the loop is the caller's, so the caller asks. */
sb32 syTaskmanCheckBreakLoop(void);

/* Puts sSYTaskmanStatus back to nSYTaskmanStatusDefault without running
 * syTaskmanRunTask's own loop -- the one line that loop runs before it,
 * broken out the same way syTaskmanRunFrame/syTaskmanSetupPools are.
 * Only the host test wants it: a test that drives a scene's own exit
 * (syTaskmanSetLoadScene) by hand, tic-by-tic, and needs
 * syTaskmanCheckBreakLoop clean again for whatever test runs next in
 * the same process. A real scene never calls this. */
void syTaskmanResetBreakLoop(void);

/* The photo (port only): a frame rendered into a texture, for a scene to
 * draw a picture of the screen. The N64 game reads its framebuffer for
 * that three times: Stage Clear's wallpaper (the rung's last frame), the
 * VS results wipe (the battle's) and the opening Room's star wipe (its
 * own frame at tic 1039). Reading vram_s is not reliable
 * for a frame rendered to the screen, so all three would show black.
 *
 * So the frame is rendered into a 640x480 RGB565 buffer instead, with
 * pvr_scene_begin_rtt at a 640-pixel stride, and the PVR samples it as a
 * strided non-twiddled texture. That works on every target, because the
 * renderer that wrote it is the one reading it. Its UVs count over
 * 1024x512.
 *
 * syTaskmanWantPhoto: the next frame goes into the photo instead of onto
 * the screen, and the screen keeps the frame before it for one more
 * retrace (the Room's star wipe, mid-scene).
 * syTaskmanWantExitPhoto: the running or next-started scene's last
 * frame. The tic a scene ends has no draw, and its GObjs are ejected
 * right after, so the frame loop draws the scene once more, into the
 * photo (Stage Clear and the VS results wipe, asked for before the
 * battle starts). Two requests and not one: a "next frame" asked for
 * before a battle is its first frame, mid fade-in from black.
 *
 * syTaskmanGetPhoto: the photo, or NULL, with its size in pixels. It is
 * readable in the scene that took it and in the next one; the frame loop
 * frees it when the scene after that starts, or at
 * syTaskmanReleasePhoto. Reading it also sets the PVR's texture stride
 * to the photo's. */
void syTaskmanWantPhoto(void);
void syTaskmanWantExitPhoto(void);
void *syTaskmanGetPhoto(s32 *w, s32 *h);
void syTaskmanReleasePhoto(void);

/* The powers of two the photo's UVs count over. */
#define SY_TASKMAN_PHOTO_TEXW 1024
#define SY_TASKMAN_PHOTO_TEXH 512

/* The part of a w x h photo the game's two wipes show, as a UV window
 * over SY_TASKMAN_PHOTO_TEXW x TEXH: the middle 300x220 of the game's
 * 320x240, the ten-pixel border its copy loops leave off
 * (lbtransition.c:216), scaled. win = u0, v0, su, sv. */
static inline void syTaskmanPhotoWindow(s32 w, s32 h, f32 win[4])
{
    win[0] = (f32)(w * 10 / 320) / SY_TASKMAN_PHOTO_TEXW;
    win[1] = (f32)(h * 10 / 240) / SY_TASKMAN_PHOTO_TEXH;
    win[2] = (f32)(w - 2 * (w * 10 / 320)) / SY_TASKMAN_PHOTO_TEXW;
    win[3] = (f32)(h - 2 * (h * 10 / 240)) / SY_TASKMAN_PHOTO_TEXH;
}

#endif /* SSB_DC_TASKMAN_H */
