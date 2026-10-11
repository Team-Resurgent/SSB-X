# Contributing

This is an experimental, Dreamcast-specific port. Bug fixes are welcome!
Only Dreamcast-hardware bugs or issues may be submitted. If the same bug
is reproducable on hardware _and_ an emulator you may mention that for
reproducability, but only hardware-reproducable issues will be accepted.

Specific remaining work may include:

1) Visual bugs
2) Game crashes
3) VMU art, "How-to-Play" controls

## Building and testing

`README.md` has the setup: docker or podman, bash, and your US ROM as
`baserom.z64` in the project root. Then:

```sh
./run.sh setup        # the image, the hook, a first build
./run.sh disc         # the disc files; the bundle and purity checks read them
./run.sh test         # every check -- run it before a PR
./run.sh test --ci    # the subset CI runs; needs no ROM
```

## How the code is written

- `src/dc/` ports the decomp function by function, keeping its names and
  structure. Where the port behaves differently from the decomp on
  purpose, mark it `DIVERGES` at that spot, with the reason.
- A fix belongs where the bug is. If a check in `tools/check/` would have
  caught it, extend the check; `./run.sh test --list` says what rule each
  one holds.
- Debug knobs are compile flags in `src/dc/db.h`, passed as
  `EXTRA_CFLAGS=-DDB_...`. A new one goes there, off by default, with a
  comment saying what it prints or does.
- Show the fix working on a Dreamcast, and put the build command in the
  pull request.

## License

By contributing you agree that your contribution is under the MIT License
in `LICENSE`. Code ported from the decomp stays outside that license, as
`LICENSE` describes.
