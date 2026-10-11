# Super Smash Bros. for the original Xbox

A native original-Xbox port of *Super Smash Bros.* (N64, 1999), built from
[ssb64-dc](https://github.com/jnmartin84/ssb64-dc), the Dreamcast port, the
same way as the Ocarina of Time and Mario Kart 64 Xbox ports: the Dreamcast
port's game code is compiled unchanged for the Xbox, and the slice of
KallistiOS it runs on -- the PowerVR, the AICA sound chip, the maple bus, the
memory card, threads, files, timers -- is reimplemented on the Xbox's own
hardware in `Platform/xbox/`.

**No game data is included.** You need your own US ROM (v1.0,
SHA-1 `e2929e10fccc0aa84e5776227e798abc07cedabf`). Everything taken from it is
generated on your machine and never committed.

The Dreamcast port's own README is kept as [README.dreamcast.md](README.dreamcast.md);
its credits and the description of what it changed from the N64 game apply here too.

## Requirements

* the [RXDK](https://github.com/Team-Resurgent) toolchain (RXDK > RXDK Window >
  Install Prerequisites puts it in `C:\ProgramData\RXDK`)
* Python 3 (numpy and pygfxd are installed by `setup.py` on first run)
* git
* [MSYS2](https://www.msys2.org/) with GNU make (`pacman -S make`). The asset
  exporters are driven by the Dreamcast port's own Makefile, unchanged, which
  needs a POSIX shell and make 4.
* optional: Visual Studio 2022 or later, to build and debug in the IDE

## Building

**1. Put your ROM in the root of this folder, named `baserom.z64`.**

Copy your US *Super Smash Bros.* ROM into the top folder of this repository
-- the one with `setup.py` and this README in it -- and rename it to exactly
`baserom.z64`:

```
SSB-X\
  baserom.z64      <-- your ROM, this exact name
  setup.py
  README.md
  ...
```

It must be the US v1.0 ROM in big-endian `.z64` format (SHA-1
`e2929e10fccc0aa84e5776227e798abc07cedabf`). A `.n64` or `.v64` dump is
byte-swapped: convert it to `.z64` first. `setup.py` checks the file and says
what is wrong if it is missing, misnamed or a different version.

**2. Run the build script from the same folder:**

```bash
python setup.py
```

The result is **`out/Release/XISO/SSB-X.iso`**. `setup.py` runs, in order:

1. fetches [ssb-decomp-re](https://github.com/VetriTheRetri/ssb-decomp-re) at
   the revision `docker/pins.env` names into `extern/`, and applies
   `docker/patches/`;
2. runs the Dreamcast port's exporters (through `src/game/ssb64/Makefile`) to
   turn the ROM into the fighter, stage, effect, sprite and sound files, and
   the `models.bnd` bundle the game reads them from;
3. generates the Xbox pieces: the reverb table, the dashboard icons (from the
   game's own emblem), `data/` and `rxdk.project.json`;
4. builds with `Rxdk.Cli`.

```bash
python setup.py --check         # what is present and what is missing
python setup.py --skip-build    # everything but the compile, e.g. to build in Visual Studio
python setup.py --config Debug  # the debug configuration (debug SDK libraries, -O0)
```

### In Visual Studio

Run `python setup.py --skip-build` once, open `ssb64x.sln`, choose the **Xbox**
platform and Build. The project is a thin wrapper around the same `Rxdk.Cli`
build, so both give the same ISO. **F5** is the Xbox Debugger (RXDK).

## Running

Boot `out/Release/XISO/SSB-X.iso` on a modded console, or copy the folder
`out/Release/Build/SSB-X/` (the `default.xbe` and its `data` folder) to the
hard disk. `D:` is the XBE's own folder, so both work. To a devkit:

```bash
C:\ProgramData\RXDK\tools\Rxdk.Cli.exe deploy --project-root . --console <ip>
C:\ProgramData\RXDK\tools\Rxdk.Cli.exe run --project-root . --console <ip>
```

The game saves to the Xbox hard disk as a normal save game (dashboard >
Memory > Super Smash Bros.). Its title ID is `54520011` (Team Resurgent's
TitleID-DB).

### Video modes

The mode follows the dashboard's settings: **720p** on component (HDTV)
cables with 720p ticked, otherwise **480p** when that is ticked, otherwise
480i. At 720p the game's 4:3 picture is drawn natively at 960x720 (1.5x the
original 640x480, not an upscale of it) with black bars either side; the
game's camera and HUD are 4:3, so it is not stretched to 16:9.

## Controls

The Xbox pad's face buttons sit where the Dreamcast pad's do, so the Dreamcast
port's mapping applies as it is:

| Xbox | N64 | |
| --- | --- | --- |
| Left stick | Control stick | |
| A | A | attack |
| X | B | special |
| Y | C-up | jump |
| B | C-down | jump |
| Left trigger | Z | shield (past half travel) |
| Right trigger | R | grab (past half travel) |
| D-pad | taunt / costume / menus | as on the Dreamcast port, by screen |
| START | START | |

All four controller ports work, and the pads rumble where the N64's Rumble
Pak would.

## How it maps onto the Xbox

| Dreamcast | Xbox (`Platform/xbox/`) |
| --- | --- |
| PowerVR (KOS `pvr_*`) | `pvr_xbox.c`: Direct3D 8 on the NV2A. Texture memory and palette RAM are kept as on the PVR; textures are untwiddled (16-bit, paletted, VQ) into NV2A textures when a polygon names them, palettes captured per batch; opaque / punch-through / translucent lists, the translucent list sorted per triangle; render-to-texture; the secondary accumulation buffer; the PVR's 1/w depth in a floating-point depth buffer. |
| AICA (KOS `snd_*`, `spu_*`, `g2_*`) | `aica/`: sound RAM, the channel registers and the command queue, the port's own ARM7 firmware compiled for the x86, 64 channels of Yamaha ADPCM / PCM mixed in software, the N64's reverb, into a DirectSound stream. |
| maple bus | `kos_maple_xbox.c`: XInput pads and their rumble motors. |
| VMU | `kos_vmu_xbox.c` + `kos_vmu_pkg.c`: a VMU whose files live in the title's save game on `U:`. |
| `/cd`, `/rd` | `kos_fs_xbox.c`: `D:\data`. |
| threads, mutexes, semaphores, condition variables, timers | `kos_thread_xbox.c`, `kos_sys_xbox.c` |
| debug log | `kos_log_xbox.c`: to xbWatson, off in release builds as on the Dreamcast. |

The changes to the Dreamcast port's own files are few and each is marked
`XBOX:`:

* `src/dc/dcstrings.h` -- the memory card's wording ("Hard Disk");
* `src/dc/assetroot.c` -- asset buffers from `malloc`, not `memalign`: RXDK's
  `realloc` moves an over-aligned block even to shrink it, and the fighter
  packs are trimmed in place;
* `src/dc/fgm.c` -- the one decimated sound effect (the title announcer)
  played at the right pitch;
* `src/dc/mntitle.c`, `src/dc/sndres.c`, `src/dc/fgm.c|h`, `src/dc/bgm.c|h` --
  after the opening movie the title lets its music and the announcer play on,
  as the N64 does, and the sound set is swapped in without stopping them;
* `src/dc/scautodemo.c`, `src/dc/sndres.c|h` -- the attract-mode demo's
  fighters, stage and crowd are staged and play;
* `tools/export/ssb_sndsets.py` -- decomp paths with `/` on Windows too, and
  the title-after-movie set held to the sound RAM budget.

Build-system glue lives in `tools/xbox/`.

## Layout

```
setup.py              the one-command build
rxdk.project.json     the build manifest (generated by tools/xbox/gen_manifest.py)
ssb64x.sln/.vcxproj   Visual Studio
Platform/xbox/        the Xbox platform layer (KallistiOS on the Xbox)
tools/xbox/           host-side build glue (manifest, data, icons, reverb, checks)
src/dc/               the Dreamcast port (game code and its KOS-facing layer)
src/game/ssb64/       its main.c and Makefile (asset rules and object list)
tools/export, lib     its asset exporters
extern/               the decomp, fetched by setup.py (not committed)
data/                 the staged game data (generated, not committed)
```

## Licensing

See [LICENSE](LICENSE), [NOTICE](NOTICE) and [PROVENANCE.tsv](PROVENANCE.tsv).
The Xbox port's own code (`Platform/xbox/`, `tools/xbox/`, `setup.py`, the
Visual Studio and RXDK project files, and the `XBOX:` changes) is MIT,
Copyright (c) 2026 Team Resurgent; the Dreamcast port's original work keeps its
own MIT notice. Code carried over from KallistiOS stays under the KOS License,
[Platform/xbox/LICENSE.KOS](Platform/xbox/LICENSE.KOS). No ROM, asset or other
game data is in this repository; `.gitignore` keeps everything `setup.py`
generates out of it.

## Credits

* [ssb-decomp-re](https://github.com/VetriTheRetri/ssb-decomp-re), the decompilation.
* [ssb64-dc](https://github.com/jnmartin84/ssb64-dc), the Dreamcast port this is built from.
* [KallistiOS](https://github.com/KallistiOS/KallistiOS), whose API this reimplements
  (and whose PVR header compiler, VMS package code and AICA channel routines are
  carried over, under the KOS licence).
* Team Resurgent's RXDK.
