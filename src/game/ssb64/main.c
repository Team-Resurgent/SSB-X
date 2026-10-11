/* ssb64-dc -- the boot.
 *
 * Everything the machine has to be told before the game can start, and
 * then the game: the video mode and the PVR, the controller layer, the
 * scene heap the N64 cut out of the RAM behind an overlay's BSS, the two
 * audio banks the game keeps resident from boot, and
 * scManagerRunScene(), which does not return -- it is the game's own
 * navigation (sc/scmanager.c:867) and it plays scene after scene.
 *
 * Nothing else is here.  The collision overlay, the serial log, the
 * cliff warp, the scripted pad that walks the chain unattended, and the
 * boot battle state a build that skips the menus wants live in
 * src/dc/db.c, the port's debug facility, which enters through the one
 * db_install() call below.  A
 * build without it drops that call and the object with it.
 *
 * The stage and the fighter packs are not loaded here: a scene loads the kinds it shows
 * (ftManagerSetupFilesAllKind, grStageAcquire) and gives them back when
 * it ends.
 */
#include <kos.h>
#include <malloc.h>
#include <dc/pvr.h>

#include "assetroot.h"
#include "bgm.h"
#include "bgmbank.h"
#include "dcpvr.h"
#include "lbcommon.h"         /* LB_Z_ZCLIP */
#include "fgm.h"
#include "input.h"
#include "scmanager.h"
#include "sndres.h"
#include "taskman.h"
#include "vmusave.h"
#include "dctext.h"
#include "vmunotice.h"
#include "stackguard.h"
#include "textguard.h"

#include <sc/scdef.h>          /* nSCKindTitle, nSCKindStartup */

/* src/dc/db.h: the port's debug facility, the game's own src/db/
 * overlay's place in this build.  -DDB_ENABLE=0 and db.o out of the
 * link is the whole of what it takes to build without it. */
#ifndef DB_ENABLE
#define DB_ENABLE 1
#endif
#if DB_ENABLE
#include "db.h"
#endif

/* src/dc/db.h: DB_DCLOAD_NET. A file-scope macro, not a function call --
 * KOS's init machinery resolves it by weak-symbol override at link time,
 * before main() runs, so it has to sit in the translation unit that
 * actually links, which by KOS convention is this one. Do not move this
 * into db.c; it would silently stop taking effect. Off by default
 * (KOS's own INIT_DEFAULT, no INIT_NET): this port has never needed the
 * network stack up, and it costs boot time and memory to bring up. */
#ifdef DB_DCLOAD_NET
KOS_INIT_FLAGS(INIT_DEFAULT | INIT_NET);
#endif

/* The port's frame boundary (taskman.h syTaskmanSetFrameEndHook): the
 * save store's pump, and under src/dc/db.h's memory-hunt flags the heap
 * walk and the stack bands, so a clobber is reported in the frame that
 * made it rather than at the scene change that trips over it. */
static void main_frame_end(void)
{
    sy_sram_pump();
    ASSET_HEAP_WALK("frame end");
#if defined(DB_STACK_GUARD)
    stack_guard_check("frame end");
#endif
    TEXT_GUARD_FRAME("frame end");
}

#ifdef DB_BOOT_JUNK
/* src/dc/db.h's DB_BOOT_JUNK: make memory look like a console's power-on
 * memory before the game touches it. Zeroed RAM and VRAM make a read of
 * memory nobody wrote get a NULL, a 0 count or a black texel, and work;
 * a console's power-on memory holds whatever was there, and the same read gets garbage. Every free byte of both is
 * taken in blocks, filled with 0xDE (the byte DB_HEAP_POISON and the
 * scene heap's poison use: a pointer made of it faults, a float made of
 * it is -8e18) and given back, so the heap's fresh memory is never zero.
 * KOS's "Out of memory" line once at boot is this, not a leak. */
#define BOOT_JUNK_WORD 0xDEDEDEDEu

static void boot_junk_fill(void)
{
    static const size_t steps[] = { 64 * 1024, 4096, 256 };
    void *head = NULL;
    size_t ram = 0, vram = 0, i;
    pvr_ptr_t vhead = NULL;

    /* RAM: the blocks chain through their own first word */
    for (i = 0; i < sizeof(steps) / sizeof(steps[0]); i++)
    {
        void *b;

        while ((b = malloc(steps[i])) != NULL)
        {
            memset(b, 0xDE, steps[i]);
            *(void **)b = head;
            head = b;
            ram += steps[i];
        }
    }
    while (head != NULL)
    {
        void *next = *(void **)head;

        free(head);
        head = next;
    }
    /* VRAM: 32-bit writes only (a byte write to texture memory is lost),
     * through the store queues; the chain lives in the blocks too */
    for (i = 0; i < sizeof(steps) / sizeof(steps[0]); i++)
    {
        pvr_ptr_t b;

        while ((b = pvr_mem_malloc(steps[i])) != NULL)
        {
            sq_set32(b, BOOT_JUNK_WORD, steps[i]);
            *(volatile uint32_t *)b = (uint32_t)(uintptr_t)vhead;
            vhead = b;
            vram += steps[i];
        }
    }
    while (vhead != NULL)
    {
        pvr_ptr_t next = (pvr_ptr_t)(uintptr_t)*(volatile uint32_t *)vhead;

        *(volatile uint32_t *)vhead = BOOT_JUNK_WORD;
        pvr_mem_free(vhead);
        vhead = next;
    }
    dbglog(DBG_INFO, "boot junk: %u bytes of RAM and %u of VRAM filled "
           "with 0xDE\n", (unsigned)ram, (unsigned)vram);
}
#endif

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#ifdef SSB_RELEASE
    /* a release build writes nothing to the serial port or any other
     * debug console, whoever calls printf (the Makefile's RELEASE=1) */
    dbgio_disable();
#endif
#ifdef DB_QUIET
    /* src/dc/db.h's DB_QUIET: before anything logs */
    dbglog_set_level(DBG_WARNING);
#endif
#if defined(DB_STACK_GUARD)
    /* first, while almost none of the main stack is in use */
    stack_guard_paint_main();
#endif

    /* The generic mode rather than DM_640x480_NTSC_IL: KOS resolves it
     * against the cable, which is 640x480 progressive on a VGA box and
     * 640x480 interlaced NTSC on everything else -- both 60 Hz, and the
     * PAL 50 Hz mode on no console at all.  That is deliberate, and it is
     * what the disc declares (tools/check/ip_check.py, the VGA bit and the JUE
     * region): the game's tic is a frame, so 50 Hz would run every
     * animation and every timer seventeen percent slow. */
    vid_set_mode(DM_640x480, PM_RGB565);
    /* not pvr_init_defaults(): the stage geometry needs the punch-through
     * list, which the KOS defaults leave disabled (src/dc/dcpvr.h) */
    dc_pvr_init();
#ifdef DB_BOOT_JUNK
    boot_junk_fill();
#endif
    /* The PVR background plane defaults to PVR_MIN_Z (1e-4) with depth
     * write on, which occludes anything farther than 10000 units --
     * including the stage wallpaper quad. Push it out of the way. */
    pvr_set_zclip(LB_Z_ZCLIP);         /* src/dc/lbcommon.h */
    /* The controller latch's own defaults, before anything can read a
     * pad.  Four of its fields are not zero (input.c): the auto-repeat's
     * delay and rate, the per-port error flag, and the dense device map's
     * -1 "no pad".  The last two are rewritten by every sy_input_poll and
     * sy_input_update, so only the first two depend on this call -- but
     * they depend on it completely, because nothing else ever writes
     * them, and button_update repeats every frame instead of after 30 and
     * then every 5 when they are zero.
     *
     * The one test that covers this code cannot catch a missing call, because src/dc/test/input_hosttest.c calls
     * sy_input_init() itself at the top of all nine of its cases.  A
     * test that sets up the state it checks cannot see the boot that
     * does not. */
    sy_input_init();
    /* The scene's general heap, which syTaskmanStartTask sizes from the
     * scene's SYTaskmanSetup on the N64 -- from the RAM after the
     * overlay's BSS, which has no meaning in a KOS ELF, so the boot
     * makes the region and the scene keeps it. It holds the clip buffers
     * (FTMANAGER_VCLIP_MAX), an FTAttributes per kind on the select, the
     * effect bank (efcommon's textures alone are 307K), Hyrule's
     * collision tables, the object pools dSCVSBattleTaskmanSetup asks
     * for, and the four Fighter instances ftManagerAllocFighter cuts
     * out of it with their clip buffers. The high-water mark is printed rather
     * than trusted, because syMallocSet's overflow path is a message and
     * then a hang (sys/malloc.c:28-31). */
    /* 1280 K: mvopeningrun.c holds all eight fighters
     * at once (eight Fighter instances, ~15 K each, their FTStructs and
     * parts, on top of the effect bank), and 768 K overflows --
     * "ml : alloc overflow #65536" and a hang. Main RAM is 16 MB and the
     * heap is one malloc, so the margin is cheap. */
    if (syTaskmanMakeGeneralHeap(1280 * 1024) < 0)
        return -1;
    /* sys/audio.c:880 syAudioMakeBGMPlayers, before anything can ask for
     * music: mpCollisionSetPlayBGM does, inside the scene. It loads the
     * music bank's tables -- B1_sounds1's programs and keymaps, and every
     * sequence in S1_music.sbk, 155 KB of compressed MIDI in main RAM --
     * and starts the thread that clocks the sequencer. The waves are not
     * uploaded here any more (bgm_defer_samples): sound RAM cannot hold
     * the whole music bank and the VS game's sound effects at once, so
     * src/dc/sndres.c uploads a scene's share of both as the scene
     * starts. */
    bgm_defer_samples();
    syAudioMakeBGMPlayers();
    if (!gBGMBank.n_seqs)
        dbglog(DBG_WARNING, "boot: no music bank; the game is silent\n");
    else
        dbglog(DBG_INFO, "boot: music up -- %u programs, %u sounds, "
               "%u waves, %u sequences; %u bytes of sound RAM free\n",
               (unsigned)gBGMBank.n_insts, (unsigned)gBGMBank.n_sounds,
               (unsigned)gBGMBank.n_waves, (unsigned)gBGMBank.n_seqs,
               (unsigned)snd_mem_available());
    /* The FGM engine (sys/audio.c's setup hands n_alSeqp the fgm tables
     * and the RSP its sample bank): the three tables here, and the sound
     * set census and the sample pack's directory beside them. No sample
     * is resident until the first scene asks (scManagerRunScene). The
     * engine's 24 AICA channels start at 32, clear of the music's 0..23. */
    if (sndres_init(32) < 0)
        dbglog(DBG_WARNING, "boot: no sound sets; the game is silent\n");
    /* The two banks the game keeps resident from boot, in a window of
     * their own: they are the first thing this build reads, and the one
     * read on the whole chain that is not free (the music bank's
     * tables). */
    asset_io_report("audio");
    asset_io_reset();

    /* sc/scmanager.c:830-853, through src/dc/scmanager.c: the save data,
     * the scene data and the two battle states from the game's own
     * defaults, which is what scManagerRunLoop does before it runs a
     * scene. A memset would leave
     * gSCManagerSceneData.unlock_messages as seven zeroes, which is seven
     * queued "Luigi unlocked" messages where the game's default is seven
     * nLBBackupUnlockEnumCount, meaning none.
     *
     * After it, every field of the save data, the scene data and the two
     * battle states holds the number the game boots with -- and, on a
     * build with the debug facility in it, whatever db_install() names
     * below and nothing else. */
    scManagerInitData();
    /* sc/scmanager.c:867: the game's navigation. The title leads to the
     * mode select, VS MODE to the VS mode, VS OPTIONS to the options
     * screen and back, VS START to the character select, its START to
     * the battle, the results screen back to the title -- so
     * scManagerRunScene does not return, it plays match after match. A
     * scene that is not ported sends it back to the scene that asked,
     * and src/dc/scmanager.c says which on the log.
     *
     * The game boots into nSCKindStartup (the N64 logo), which hands off
     * to nSCKindOpeningRoom and walks the nineteen opening scenes to
     * nSCKindOpeningNewcomers, which ends at the title -- and scene_prev
     * is what the title then reads to know the movie is what brought it
     * there (the animated logo, the slash and the logo fire) rather than
     * a menu (the static logo).
     *
     * Nothing is set here: scManagerInitData above already
     * left scene_curr and scene_prev at dSCManagerDefaultSceneData's own
     * boot pair, which is nSCKindStartup twice on a US build and
     * nSCKindOpeningRoom twice on a JP one (src/dc/scmanagerdata.c:385) --
     * the region difference.
     *
     * A build that wants to boot straight to the title, with no
     * movie, asks for it: -DDB_BOOT_SCENE_PREV=nSCKindStartup (src/dc/db.c),
     * which is the same knob the movie-to-title probe uses. */
    /* The port's layer over every frame (src/dc/taskman.h): the memory
     * card's "SAVING..." notice (src/dc/vmunotice.h). Before
     * db_install, which may put a test layer of its own on top of it and
     * chains to this one. */
    syTaskmanSetOverlayHook(vmunotice_overlay);
#if DB_ENABLE
    /* src/dc/db.h. It reads the boot scene left above and may overwrite
     * it, and it fills in the battle state the VS menus would have left
     * behind, so it goes after scManagerInitData. */
    db_install();
#endif
    /* The port's own font (src/dc/dctext.h): the staff
     * roll's textbox glyphs, resident for the session, so the memory
     * card can speak in any scene. Here, before the first scene, so its
     * records and its 36 KB of VRAM sit below everything a scene takes
     * and frees. */
    dctext_init();
    /* Everything read before the scene manager's first window opens.
     * That is the save file's pictures, vmuart.bin (3820
     * bytes, src/dc/vmucard.c), and the font, dcfont.spr (44640) -- the
     * audio banks have a window of their own above and the stages belong
     * to the scenes that show them -- and one stage on a build whose
     * DB_BOOT_SCENE goes straight into a battle. src/dc/assetroot.h on
     * what the numbers mean. */
    asset_io_report("boot");
    /* The RSP display-list heads' own per-pass reset, for every scene
     * rather than only for one that allocates a weapon pool -- see
     * src/dc/wpmanager.c. Without it a scene that writes the game's own
     * gDP strokes (the ending's room fade, the staff roll's highlight)
     * walks its head out of that scratch buffer and through .bss. */
    wpManagerInitDLHeads();
    /* The save store's pump (src/dc/vmusave.h), at the end of every tic:
     * a save the game made inside the tic goes to the memory card after
     * it. Installed rather than called from the frame loop so the oracles
     * that link taskman.c need no store. */
    syTaskmanSetFrameEndHook(main_frame_end);
    scManagerRunScene();

    /* Not reached (the loop above never leaves); kept
     * so a scene manager that does return has somewhere to land. */
    while (1)
    {
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_scene_finish();
    }

    return 0;
}
