#ifndef SSB64DC_DB_H
#define SSB64DC_DB_H

/* db -- the port's debug facility (src/dc/db.c).
 *
 * The game's own is src/db/ in the decomp: an overlay carrying a debug
 * menu, a fighter picker and a collision viewer, installed at boot and
 * dropped from a shipping build.  This is that place in the port, and
 * it presents the same one-call face: main() calls db_install() once,
 * after scManagerInitData() and after the scene data is at the game's
 * boot, and everything else the facility does it does from inside the
 * game's own frame loop, on GObjs and on hooks the scenes offer.
 *
 * Dropping it from a build is two edits and no others: remove db.o from
 * the link and set -DDB_ENABLE=0.  That is the whole reason this file
 * exists.
 *
 * Build-time knobs, all read in db.c and none of them the game's.
 * The first two are what a build you PLAY does not want -- one takes a
 * button the game plays with, the other covers the picture -- and both
 * are off by default for that reason.  Everything after them only
 * writes to the serial line or changes what the run boots into:
 *
 *   DB_COLLISION_OVERLAY  draw every collision line the stage carries,
 *                      in the colour of its kind, plus each fighter's
 *                      collision diamond, over everything else.  An
 *                      instrument: it hides what it is drawn over, so
 *                      it is not in a build being looked at for
 *                      fidelity.  Its poly header must be compiled
 *                      (a zeroed header makes the TA drop the strips
 *                      in silence).
 *
 *   DB_CLIFF_WARP      C-up drops player one just outside the next
 *                      grabbable ledge on the stage, cycling through
 *                      them. C-up is a jump button (ftcommon.c's
 *                      FT_JUMP_BUTTONS) and is Y on the Dreamcast pad,
 *                      so a build with this on jumps and teleports on
 *                      the same press. Nothing else here reads a real
 *                      pad: without this define the port has no debug
 *                      buttons at all.
 *
 *   DB_BOOT_SCENE      which scene the run starts in (default the title).
 *                      Everything else in this group -- the boot battle
 *                      state, the scripted pad's install on the whole
 *                      menu chain -- is itself gated on this naming
 *                      anything but the title: a plain build (this
 *                      knob left at its default) boots exactly like the
 *                      retail game, real controller only, nothing here
 *                      touching the menus at all.
 *   DB_SOAK            (`./run.sh soak`) an endless unattended run: four CPU
 *                      fighters on a random stage, results for
 *                      DB_SOAK_RESULTS_TICS (600), then a fresh random
 *                      match. DB_SOAK_SEED pins the rolls.
 *   DB_PERF            where the frame goes (perf.h): a
 *                      `perf:` line beside each 300-frame `db:` line --
 *                      PVR wait, the three draw passes, scene finish,
 *                      header compiles per frame priced by a boot-time
 *                      unit cost, and per-subsystem display-proc time.
 *                      Build it WITHOUT DB_HEAP_WALK/STACK_GUARD/
 *                      HEAP_GUARD/TEXT_GUARD/GDB to see the retail cost.
 *   DB_HDR_ALIGN       log any fighter/stage polygon header
 *                      compiled at an address that is not 32-byte aligned:
 *                      KOS's pvr_poly_compile does movca.l, which loses the
 *                      rest of that cache line on a console
 *   DB_HEAP_WALK_READS with DB_HEAP_WALK: walk the heap after every file
 *                      read and log the read's buffer, so the first BAD
 *                      names the read (assetroot.c asset_read)
 *   DB_SHADE_FIXED     the per-vertex lighting in integers (ftshade.h
 *                      ft_shade_lit_fx); off by default
 *   DB_JOINT_TRACE     ftMainSetStatus: a joint slot outside RAM is logged (with the
 *                      whole table) and skipped instead of stored through
 *   DB_AICA_STATUS     log the port AICA firmware's status
 *                      block every 5 s (passes, packets, the sequencer's state,
 *                      clocks and headroom, clock-step outliers, missed waves),
 *                      every sequencer command sent, the wave-table refreshes,
 *                      and its first 64 voice starts as "seqtrace" lines --
 *                      the same columns tools/check/seqcore_oracle.c traces
 *   DB_REVERB_OFF      do not load the reverb's DSP program
 *                      for a dry A/B; the sends are still
 *                      set, and go nowhere. -DDB_AICA_STATUS also logs how
 *                      much of the reverb's line is not silence every 5 s
 *                      (all of it once music plays with the DSP running)
 *   DB_SEQ_SH4_SELFTEST run src/dc/seq on the SH-4 over song 9
 *                      at boot and log its first 80 voice starts ("shtrace"),
 *                      to tell a fault in the sequencer from one in the ARM
 *   DB_PREFETCH        fighter_frame with a PREF ahead of the vertex transform: OFF
 *                      by default, the results screen crashed on hardware with it
 *   DB_FORCE_FLASH     hold every fighter on the star colour animation (perf A/B)
 *   DB_NO_LISTMASK     no per-model list mask: every pass loops every batch,
 *                      and effects/items re-walk in passes they draw nothing
 *   DB_NO_CULL         draw_batch without its software backface cull
 *   DB_CULL_DRY        make the cull test and count it, skip nothing
 *   DB_CULL_SIGN       the cull test's winding sign (default -1.0f)
 *   DB_NO_VCACHE       draw_batch without the per-batch vertex cache, for an
 *                      A/B on the same source (fighter.c)
 *   DB_1P_SOAK         with DB_BOOT_SCENE=nSCKind1PGame: an unattended
 *                      1P ladder -- A on port 2 once a second, so the VS
 *                      card, Stage Clear and the continue screen go on by
 *                      themselves. DB_1P_ENEMY_KO keeps the rungs short
 *                      (read in src/dc/db.c)
 *   DB_1P_COM          the 1P ladder's own player is a level-9 CPU
 *                      (read in src/dc/sc1pmanager.c)
 *   DB_1P_AUTOCLEAR=<s> end the rungs nobody idle can finish, that many
 *                      seconds after GO: a bonus stage's clock is run
 *                      down, Master Hand takes damage 300
 *   DB_LOAD_CENSUS     the load census (src/dc/loadcensus.h):
 *                      each scene a node -- its files and where they
 *                      came from, its load time, its memory at begin,
 *                      first frame, peak and end -- on serial as `lc:`
 *                      lines, for tools/check/loadcensus.py
 *   DB_SNDRES_WHOLE    every sound RAM swap reads the whole set again,
 *                      as before the arena (src/dc/sndres.c): an A/B
 *   DB_BOOT_STAGE      the stage a menus-skipping build plays on
 *   DB_BOOT_RULE       stock or time, before the VS mode screen has run
 *   DB_BOOT_PLAYERS    how many of the four slots the boot state fills
 *   DB_BOOT_P1_KIND    P1's fighter (an nFTKind* value, default Fox)
 *   DB_BOOT_P2_KIND    P2's fighter (an nFTKind* value, default Mario)
 *   DB_BOOT_COM_P1     make P1 a CPU player (nFTPlayerKindCom) instead
 *                      of reading a controller
 *   DB_BOOT_COM_P1_LEVEL  P1's CPU level, 1-9 (only with DB_BOOT_COM_P1)
 *   DB_BOOT_COM_P2     make P2 a CPU player, the same way
 *   DB_BOOT_COM_P2_LEVEL  P2's CPU level, 1-9 (only with DB_BOOT_COM_P2)
 *   DB_JAB_PERIOD_FRAMES  the scripted pad's jab cadence
 *   DB_SCRIPT_FORCE    feed port 1 even when a real pad is in it
 *   DB_TRACE_SPAWN     one line per tic for P1's first hundred
 *   DB_PARTICLE_TRACE  replay the whole efcommon particle bank on the
 *                      SH-4 and print a digest per script and one
 *                      over all of them, for the same numbers out of
 *                      tools/check/lbparticle_check.py to be held to
 *   DB_PARTICLE_FIRST  eight frames of one script, every live particle's
 *                      whole trace record as its bytes, when that
 *                      script's digest is the one that disagrees
 *   DB_PARTICLE_FROM   which eight frames those are (default 0)
 *   DB_PARTICLE_DUMP   all of it: every record of every script, two
 *                      megabytes down the serial line, against
 *                      tools/check/lbparticle_check.py --dump. One diff
 *                      instead of a run of bisection per script
 *   DB_PARTICLE_FRAMES how many frames of each script (default 120)
 *   DB_PARTICLE_SEED   the random seed each starts from (default 1)
 *   DB_PARTICLE_LIVE   which script to make a particle from, at player
 *                      one's feet, in the running battle -- the only way
 *                      to see one until ef/efmanager.c lands, because
 *                      nothing in the port spawns one yet
 *   DB_PARTICLE_LIVE_PERIOD  how often, in frames (default 20)
 *   DB_EFFECT_LIVE     which of ef/efmanager.c's ported hit and KO
 *                      effects to make at player one's chest: 1 SetOff,
 *                      2 DamageFire, 3 DamageElectric, 4 DamageCoin,
 *                      5 DamageNormalLight, 6 DamageNormalHeavy,
 *                      7 SparkleWhiteDead, 8 DamageSpawnOrbs,
 *                      9 DamageSlash, 10 DamageSpawnSparks,
 *                      11 DamageSpawnMDust, 12 DeadExplode, and 13 the
 *                      screen flash (if/ifscreenflash.c, its five
 *                      scripts in turn). Logs the EFStruct pool's free
 *                      count, which is the half of that path a screen
 *                      cannot show
 *   DB_EFFECT_LIVE_PERIOD    how often, in frames (default 20)
 *   DB_IO_TRACE        one line per asset file at close -- reads,
 *                      bytes, the medium's milliseconds -- under the
 *                      scene's `io:` window line (read in
 *                      src/dc/assetroot.c, the one knob db.c does not)
 *   DB_DISC_TRACE      every trip to the disc, in order, for offline analysis of
 *                      which files are read together and how far the head
 *                      moves (src/dc/disctrace.h has the format): one 24-byte
 *                      record per open/seek/read/close and, with
 *                      EXTRA_LDFLAGS="-Wl,--wrap=cdrom_read_sectors_ex
 *                      -Wl,--wrap=cdrom_stream_start
 *                      -Wl,--wrap=cdrom_stream_request", per sector read the
 *                      drive is asked. Printed at DBG_WARNING (so DB_QUIET
 *                      keeps it) when each scene's load ends, never during it
 *   DB_FB_DUMP_FULL    with -DDB_FB_DUMP=f1,f2,..: the dump at the
 *                      framebuffer's own 640x480 instead of halved.
 *                      DB_FB_DUMP_BOX=x,y,w,h cuts it to a box (w a
 *                      multiple of 80), a quarter of the serial time for
 *                      a card's crowd (src/dc/db.c db_fb_dump_frame)
 *   DB_BOOT_P1_COSTUME the human's costume (common id 0-3) on a direct
 *                      jump to the 1P card or game
 *   DB_BOOT_KIRBY_HAT  the Kirby Team's rolled copy hat (a model part id;
 *                      0 none) on a direct jump to its card
 *   DB_INTRO_TICS      "intro tic N" every 30 updates of the 1P card, for
 *                      its frame rate (src/dc/sc1pintro.c)
 *   DB_INTRO_HIDE_AT   the team cards lose their crowd from that update on
 *   DB_PRIM_DUMP=<tic> the TA's input for one update of a team card's crowd,
 *                      on serial (src/dc/primdump.h); needs
 *                      EXTRA_LDFLAGS=-Wl,--wrap=pvr_prim
 *   DB_NO_INTROCROWD   keep the models on the three team cards even when
 *                      the disc has their baked pictures, to compare the
 *                      two (src/dc/introcrowd.h)
 *   DB_IO_SELFTEST     at boot, before the first scene: read every file
 *                      under the asset root and every models.bnd entry in
 *                      the shapes the loaders use, and check each against
 *                      the same bytes read through KOS's sector cache;
 *                      list the files of the tail-hang size and count the
 *                      reads that leave KOS a tail under 32.
 *                      One summary line, "io
 *                      selftest: ..."; about a minute (read
 *                      in src/dc/assetroot.c)
 *   DB_IO_SLOW         a disc as slow as a real one: every read off
 *                      the medium takes at least its bytes at this many
 *                      KB/s, the I/O lock held throughout. A fast medium hides
 *                      every load; a GD-ROM is nearer 1-2 MB/s
 *                      (unmeasured -- 1200 is a guess to compare
 *                      against, not a model). Read in
 *                      src/dc/assetroot.c
 *   DB_IO_SLOW_SEEK    and this many milliseconds more for each loose
 *                      open and each move of the bundle's handle
 *                      (default 0; also a guess)
 *   DB_IO_IRQOFF       the dev cable's reads: after every
 *                      open and read of the medium, spin with interrupts
 *                      masked for as long as dcload would have held them
 *                      -- the bytes at this many KB/s (250 is what the
 *                      dcload-serial hardware logs show) plus
 *                      DB_IO_IRQOFF_SEEK ms (default 15) per open or
 *                      move. "io irqoff: <scene>" totals it per scene
 *                      (read in src/dc/assetroot.c)
 *   DB_NO_PREFETCH     no read-ahead: no loader thread and no pool, every
 *                      open reads the medium on the calling thread. It
 *                      isolates the read-ahead loader when a hardware-only crash may
 *                      be its doing (read in src/dc/assetroot.c)
 *   DB_PREFETCH_SYNC   keep the pool but fill it on the caller's thread,
 *                      the way the host build already does: the same
 *                      entries and the same buffers, no loader thread. It
 *                      bisects a pool crash into "the loader thread" and
 *                      "the pool bookkeeping" (read in src/dc/assetroot.c)
 *   DB_DCTEXT_TEST     the port's font (src/dc/dctext.h) on a panel
 *                      over every frame, through the overlay hook: every
 *                      glyph at the credits' size and a line at twice it.
 *                      For a -DDB_FB_DUMP look at the title and the staff
 *                      roll
 *   DB_MEMCARD_SLOW_MS a slower memory card: every save write takes at
 *                      least this many ms on the writer thread. KOS sends
 *                      maple frames once a retrace and a 14-block save
 *                      is ~100 of them, so a write already takes
 *                      about 1.85 s; a real VMU's own flash time is
 *                      unmeasured. For a write that has to span
 *                      something -- 9000 outlasts a scene load. Read in
 *                      src/dc/vmusave.c
 *   DB_MEMCARD_PROBE   run the VMU's boot check (src/dc/dcmemcard.h) even
 *                      under DB_BOOT_SCENE, where it is skipped: a probe
 *                      that starts elsewhere is not a player at a
 *                      console. Without it the game's default write lands
 *                      in the store as it did before the check existed
 *   DB_MEMCARD_AUTO    answer the boot check without a pad: 1 makes the
 *                      save file where it can be made, 2 declines. For an
 *                      unattended boot from the default scene, where the
 *                      check would otherwise wait for A on a blank VMU
 *   DB_MEMCARD_FAIL_WRITE  the first N save writes fail without touching
 *                      the card, as a card pulled mid-write would (read
 *                      in src/dc/vmusave.c)
 *   DB_IO_STRESS       a second thread that reads every file and bundle
 *                      entry in a loop while the game runs, and logs a
 *                      mismatch against its own first read -- the port
 *                      has one reader, and this is the test that a second
 *                      one would be safe (read in src/dc/assetroot.c)
 *   DB_HEAP_MAP        after each scene's `io:` lines, the malloc
 *                      arena chunk by chunk: every chunk of 32 KB or
 *                      more on its own line, the runs of small ones
 *                      between summed. A load that fails with MBs
 *                      free is the free space split by small chunks
 *                      that stay (read in src/dc/assetroot.c)
 *   DB_HEAP_CALLERS    with DB_HEAP_MAP and the link flags
 *                      -Wl,--wrap=malloc,--wrap=free,--wrap=calloc,
 *                      --wrap=realloc,--wrap=memalign: also each chunk
 *                      in use past the first free hole of 256 KB, with
 *                      the address that allocated it (addr2line on
 *                      the ELF)
 *   DB_HEAP_GUARD      a canary on both sides of every heap block, to
 *                      find the out-of-bounds write that corrupts
 *                      dlmalloc's own bookkeeping before it faults (the
 *                      opening room's malloc_consolidate address error).
 *                      Build it with the same link flags DB_HEAP_CALLERS
 *                      uses; every allocation sweeps every live block
 *                      and a clobbered canary logs the block's size, its
 *                      allocating return address (addr2line it) and which
 *                      canary and offset went bad (read in
 *                      src/dc/assetroot.c)
 *   DB_HEAP_ZERO       with DB_HEAP_GUARD: hand every block out zeroed,
 *                      to test
 *                      whether a hardware-only crash is a read of
 *                      uninitialized memory (read in src/dc/assetroot.c)
 *   DB_HEAP_POISON     with DB_HEAP_GUARD: the opposite -- fill every
 *                      block with 0xDE, so such a read faults at its
 *                      first use (read in src/dc/assetroot.c)
 *   DB_HEAP_WALK       walk every dlmalloc chunk at every frame end,
 *                      before each fighter pack is freed and at each
 *                      scene's I/O report, checking sizes, footers and
 *                      free-list links; the first bad chunk prints with
 *                      its neighbours' first words (and their allocating
 *                      addresses, with DB_HEAP_CALLERS or DB_HEAP_GUARD).
 *                      Unlike the guard it sees KOS's own blocks -- thread
 *                      stacks, kthread_t -- and needs no link flags.
 *                      "heap walk: <scene> -- clean" per scene is the sign
 *                      it ran; gdb can `break asset_heap_walk_failed`
 *                      (src/dc/assetroot.c)
 *   DB_BOOT_JUNK       fill every free byte of RAM and VRAM with 0xDE
 *                      at boot, then give it back, so memory reads
 *                      like a console's power-on garbage instead of
 *                      zero: a read of memory nothing wrote stops
 *                      working by accident (src/game/ssb64/main.c)
 *   DB_STACK_GUARD     every thread stack the port makes (GObj threads,
 *                      the loader, BGM, VMU writer, hang watchdog) comes
 *                      from src/dc/stackguard.c, painted and with a 1 KB
 *                      guard band under it, and the main stack is painted
 *                      at boot: a band written is reported at the frame
 *                      end (or as a GObj thread yields) with the stack's
 *                      owner, and "stack: <scene> -- <kind> deepest N of
 *                      M" prints per scene. Also turns the DL scratch's
 *                      walk into .bss into an abort, as DB_HEAP_WALK and
 *                      DB_HEAP_GUARD do (src/dc/wpmanager.c)
 *   DB_TEXT_GUARD      the code and constant data, summed in 1 KB blocks
 *                      at the first frame end; 64 blocks are checked per
 *                      frame end, and all of them per scene. A changed
 *                      block prints "text guard: ..." with its address,
 *                      the frame and all 256 words, for a diff against
 *                      the ELF; gdb can `break text_guard_failed`.
 *                      KOS's interrupt stack, which lives in .text, is
 *                      painted instead and its depth printed per scene
 *                      (src/dc/textguard.c)
 *   DB_ANIM_CHECK      check every MObj material script against the
 *                      loaded packs: when a model hands its MObjs their
 *                      scripts (src/dc/objmodel.c), before each parse and
 *                      after it (src/dc/objanim.c). One that is in no pack
 *                      logs "animcheck: ...", every released pack that held
 *                      the address and the owning GObj, and is stopped --
 *                      where without this the parser spins on the first
 *                      command it does not know. Read in src/dc/fighter.c
 *   DB_ANIM_USE        one line per animation a fighter binds, once
 *                      per scene and pack: "animuse: <scene kind>
 *                      <pack> main|sub <motion id> <anim> <name>".
 *                      tools/export/anm_tiers.py reads a log of them
 *                      into the .anm tier lists (read in
 *                      src/dc/ftcommon.c)
 *   DB_CHARACTERS_TOUR the Characters screen plays every motion kind in
 *                      order, then turns the page, for N pages starting
 *                      at the ((pads plugged in) - 1) * N-th: with
 *                      -DDB_ANIM_USE, the trace of all the animations
 *                      the screen can play. (read in src/dc/mncharacters.c)
 *   DB_CHARACTERS_FKIND the fighter a DB_BOOT_SCENE=nSCKindCharacters
 *                      boot opens on (an nFTKind* value)
 *   DB_ROOM_FKIND      the fighter the opening Room pulls out of the
 *                      box (an nFTKind* value), not a random one (read
 *                      in src/dc/mvopeningroom.c)
 *   DB_QUAKE_TRACE     print the first N frames of camera shake the
 *                      screen quake asks for -- the one effect that
 *                      draws nothing, so the only way to see it is the
 *                      number it hands gmCameraSetVelAt
 *   DB_MAGNIFY_PROBE   from tic 120 of a DB_BOOT_SCENE battle, hold P1
 *                      under the top blast line so the magnifying glass
 *                      comes up; the glass's draw logs its triangles and
 *                      depth scale (src/dc/fighter.c). P1 is Fox unless
 *                      DB_BOOT_P1_KIND says otherwise
 *   DB_PC_ROOT         asset_root() also tries /pc -- KOS's dcload
 *                      host-filesystem passthrough over a serial coder's
 *                      cable or a BBA, dc-tool on the host end -- before
 *                      falling back to /cd then /rd (read in
 *                      src/dc/assetroot.c, the one knob db.c does not).
 *                      A runtime probe, not a build-time guarantee: a
 *                      DB_PC_ROOT ELF booted from a real disc, or
 *                      with no cable, has no dcload link and falls through to /cd or /rd same
 *                      as always. `./run.sh build` DCLOAD=1 sets this
 *                      automatically, since a DCLOAD build has no
 *                      romdisk and no disc and needs /pc to have any
 *                      asset source at all.
 *   DB_QUIET           dbglog at DBG_WARNING from the first line of
 *                      main(): warnings and errors still print, every
 *                      DBG_INFO line (the battle logger, io:, scene
 *                      reports) costs one comparison. For timing on a
 *                      console: KOS's SCIF write spins on each byte at
 *                      115200 baud (~87 us), cable or no cable, and a VS
 *                      match logs ~15 lines a second -- about a tenth of
 *                      the CPU. A battle still prints one "perf:" line
 *                      per 300 frames (fps, update and draw us/f, tris),
 *                      ~6 ms of serial per 5 s. Not the default: tools
 *                      that read the log want the INFO lines
 *   DB_DCLOAD_NET      adds KOS_INIT_FLAGS(INIT_DEFAULT | INIT_NET) in
 *                      main.c, which KOS's dcload-ip transport needs to
 *                      bring the network-backed syscall relay up
 *                      (arch_init_net_dcload_ip). Only the BBA transport
 *                      needs it -- dcload-serial works without it -- and
 *                      it costs boot time and memory this port has never
 *                      spent before, so it stays off unless the BBA path
 *                      (DB_PC_ROOT over IP, or DB_GDB over IP) is
 *                      actually wanted. UNVERIFIED on real hardware.
 *   DB_GDB             calls gdb_init() once, first thing in
 *                      db_install(). It rides the same dcload link as
 *                      DB_PC_ROOT when one is up (serial or BBA, via
 *                      dc-tool's -g relay on host port 2159) or falls
 *                      back to raw SCIF at 57600 baud otherwise. WARNING:
 *                      it fires an initial breakpoint immediately, so
 *                      the boot HANGS waiting for a debugger to attach --
 *                      only pass this to a build you are about to attach
 *                      sh-elf-gdb/kos-gdb to. Also: db_hang_watch below
 *                      is unconditional and will likely report a false
 *                      hang the moment a real debugger has every thread
 *                      stopped at a breakpoint -- two watchdogs sharing
 *                      one CPU, not a regression.
 *
 * e.g. scripts/ctr.sh run make -C src/game/ssb64 EXTRA_CFLAGS=\
 *          "-DDB_BOOT_SCENE=nSCKindVSBattle -DDB_BOOT_PLAYERS=4"
 * or   EXTRA_CFLAGS=-DDB_BOOT_SCENE=nSCKindVSResults ./run.sh disc
 *
 * The disc build is the one to steer at anything past a one-player
 * battle: a romdisk ELF carries the 13 MB romdisk and has no heap
 * left for the menus, the results screen or a second fighter. The Makefile rebuilds what a changed knob reaches.
 */
void db_install(void);

#endif /* SSB64DC_DB_H */
