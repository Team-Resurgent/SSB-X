# ssb64-dc

A native port of Super Smash Bros. (1999, N64) to the Sega Dreamcast, tuned
for the Dreamcast's hardware. The goal is to create a very good Dreamcast port
worth your time with improved framerates and resolution. This is not a recomp,
and all N64-specific code has been translated to work as efficiently on the
Dreamcast as possible.

This is considered to be in a PRERELEASE state. The complete game is here and
fully playable. The remaining work is minor visual bugs, and nice-to-haves.

**NOTE: No game content is included.** Building needs your own legally obtained US
ROM: See [ssb-decomp-re](https://github.com/VetriTheRetri/ssb-decomp-re) requirements
for this as they are the same.

## Acknowledgements

This project derives from or uses other people's work, and they deserve most of the
credit for this project being possible:

1. [ssb-decomp-re](https://github.com/VetriTheRetri/ssb-decomp-re), the
   decompilation source, of which we use as a submodule.
2. [KallistiOS](https://github.com/KallistiOS/KallistiOS), decades of
   work behind Dreamcast homebrew.
3. [@jnmartin](https://github.com/jnmartin84)'s N64 to Dreamcast ports, 
   which I used as a reference when stuck.
4. [AICAFlow](https://github.com/dfchil/AICAflow) by [dfchil](https://github.com/dfchil)
   as a reference to utilize the Dreamcast AICA's DSP.

### LLM use

This project used LLMs heavily. I know the problems with that, and I don't
judge anyone for judging this project for it. There is a lot of LLM slop in the retro gaming
community, and it matters to me that anything I release is worth your time and
runs well on original Dreamcast hardware. It took over
a month of manual testing, iteration, performance measurements, reading
documentation, asking questions and feedback from others for several hours a day.
I don't need fast internet points, and I want the output to be worth your while
and to provide the Dreamcast community something cool!

### Hardware only

This targets original Dreamcast hardware. Issues about Flycast, Deecy or other
emulators will be ignored. In my testing they run fine, but the best way to
play the game in an emulator is the original N64 version, which is the
definitive one. 

Doing debugging on real hardware will require some form of 
[dcload](https://github.com/KallistiOS/dcload-serial).

## Goals

1. Replace everything N64-specific (the RSP and RDP, display lists, libultra,
   the audio microcode) with Dreamcast implementations:
   - Fast3D display lists become pre-baked assets, or are ported to the
     Dreamcast's CPU and PowerVR.
   - Memory management is nearly written from scratch. The ROM does not fit 
     in a Dreamcast's RAM, and the N64 reads its cartridge at almost any time,
     which if ported verbatum would be an unplayable mess on the Dreamcast.
   - Every asset is converted from the ROM to PowerVR formats, uncompressed
     most of the time (textures) and re-compressed where necessary (audio)
2. A far more consistent 60fps frame rate than the N64 original during gameplay.
3. Reasonable load times from disc.
4. As faithful to the original as the above considerations allow.
5. Give the Dreamcast community a native port of an amazing game.

Not goals:

1. Being the best or "definitive" way to play the game.
2. Moddability. People have asked about adding Sonic--feel free to fork. Changes
   here exist only to make the Dreamcast version better, mainly in load times
   and visual parity.

## What changed from the N64 game

The main issues needed to be solved are memory memory and load times. After recording
dozens of hours of asset loading patterns, disc padding and file layout optimizations
keep CD-R load toimes reasonable in my testing. The main changes:

1. The N64's cartridge DMA calls during play are served from RAM instead of ROM: 
   each scene's data is preloaded when memory allows.
2. All FigaTree animations are pre-baked and loaded into RAM instead of being
   driven by Fast3D opcodes.
3. The Kirby, Yoshi and Polygon Team 1P screens are baked at build time into
   VQ-compressed textures, with every variation provided. They do not animate
   in one by one, which was a compromise I made to make the loading feel more
   seamless.
4. VMU saves are supported and notifications reuse the ROM's assets.
5. (2x) 3D rendering resolution. It supports both 480i and 480p progressive 
   scan output. Textures are uncompressed in all all cases except for point 3
   above.
6. The N64's lower-detail LOD models are dropped, so characters keep their
   detail at a distance. Gameplay is unaffected.
7. All audio is re-encoded to the AICA's 4-bit ADPCM, so there is some
   degradation. I don't hear a difference side by side, but consider yourself
   warned.
8. Sound effects use the AICA's DSP, which may differ slightly from the N64's.
   Again, I don't notice much side by side.


## Known Issues
I would say the remaining issues to solve are any kind of minor visual parity with
the original unrelated to increased resolution. This includes fine-tuning texure
alphas, gamma correction, etc. See CONTRIBUTING.md for a short list.

## Controller mapping

The Dreamcast port needs to account for L, Z or C-left/C-right, so the triggers and D-pad
cover them. Up to four pads work.

| Dreamcast | N64 | Notes |
| --- | --- | --- |
| Analog stick | Control stick | Move (scaled to the N64's ±80 range) |
| A | A | Attack |
| X | B | Special |
| Y | C-up | Jump |
| B | C-down | Jump |
| Left trigger | Z | Digital, at half travel |
| Right trigger | R | Digital, at half travel |
| D-pad | L (taunt) | Taunt, on any direction |
| Start | Start | Pause, confirm |

The D-pad changes meaning with the screen:

| Screen | D-pad sends |
| --- | --- |
| Character select (VS, 1P, Training, Bonus) | C-up/right/down/left, which pick the costume |
| In a match, once the fight has started | L, the taunt (also on a paused bonus stage, for "L to retry") |
| Everywhere else | The D-pad itself, for menus |

## Building

Builds run in a container that carries the cross-compiler, KallistiOS, the
decomp and the exporters at the revisions in `docker/pins.env`. You should
have git and docker/podman installed. Tested 
on Linux (x86) and macOS (arm64); Windows is untested, but I have no reason
to believe WSL2 wouldn't work here.

1. Put the ROM in the project root as `baserom.z64` (big-endian; `*.z64` is
   gitignored).
2. Build:

```sh
./run.sh setup     # build the image, install the commit hook, first build
./run.sh disc      # -> build-dc/ssb64.cdi
```

`./run.sh setup` pulls the published build image for your CPU (x86 or arm64)
when there is one, and otherwise compiles the toolchain, which takes about an
hour. `./run.sh image --local` always compiles, and `./run.sh image --prebuilt`
uses KallistiOS's published toolchain instead (x86 only).

Other commands (`./run.sh help` lists them all):

```sh
./run.sh release   # the player's disc: build-dc/ssb64-release.cdi
./run.sh run-hw    # push a dev-cable build to a console (dc-tool)
./run.sh test      # every check that needs no console (--list, --ci)
```

Quite a few debug knobs (`src/dc/db.h`) are provided that do things like boot 
into a scene, script input, or log what a system is doing over the DC's
serial port:

```sh
EXTRA_CFLAGS=-DDB_BOOT_SCENE=nSCKindVSBattle ./run.sh disc
EXTRA_CFLAGS= ./run.sh disc      # clean build, no knobs
```

The Makefile does not track header dependencies: after editing only a `.h`,
change `EXTRA_CFLAGS` or run `./run.sh clean`.

## Tests

`./run.sh test` runs on the build machine, and the pre-commit hook runs it
(`git commit --no-verify` skips it). `./run.sh test --ci` works without a ROM. 
It covers host cross-tests of the port's C against `ssb-decomp-re`.

## Layout

```
run.sh           every command (./run.sh help); each delegates to scripts/
scripts/         the container, build, disc and test steps
docker/          the build image and the pinned upstream revisions
src/dc/          the port itself
src/game/ssb64/  the game: main.c, the Makefile, the host cross-tests
tools/           lib/ shared readers, export/ the exporters, check/ the checks
```

About a third of the game is the decomp's C compiled unmodified; the rest is
ported into `src/dc/`, and each departure from the decomp is marked
`DIVERGES`.

The development history before this repository's first commit is private. The
commit hashes quoted in comments, and the `harnesses-final` tag, refer
to it and do not resolve here.

Bug-fix contributions are welcome; see `CONTRIBUTING.md`.

## License

`LICENSE` puts the project's original work under the MIT License: the tools,
scripts and build files, and the `src/` files marked ORIGINAL in
`PROVENANCE.tsv`. The code ported from the decomp, and the
libultra-derived files, carry no license from this project; see `LICENSE` and
`NOTICE`.
