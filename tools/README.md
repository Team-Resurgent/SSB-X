# tools/

Everything here runs on the build machine, inside the container
(`scripts/ctr.sh run python3 tools/<dir>/<tool>.py ...`), never on the Dreamcast.

| directory | what it holds | who runs it |
|---|---|---|
| `lib/` | shared readers: the ROM and relocData extractors (`ssb_extract`, `ssb_assets`), the fighter mesh baker (`ssb_meshexport`), the PVR texel and AICA codecs, and where the decomp lives (`ssb_paths`) | imported by the other two |
| `export/` | the build-time exporters: one per asset kind, each turning ROM data into a pack, bank or table the port loads, plus `disc_layout.py` and the reloc header generator | `src/game/ssb64/Makefile`, `scripts/make_cdi.sh` |
| `check/` | every check: the oracles `./run.sh test oracle` runs (with their `*_oracle.c` host drivers), the tree checks `./run.sh test --list` names, their `*_known.tsv` classifications, and two by-eye helpers (`texsheet.py`, `fbdump.py`) | `./run.sh test`; `scripts/test_oracle.sh` |
| `build_gfxdis.sh` | builds glankk's `gfxdis`, the reference display-list disassembler | `docker/Dockerfile`, which copies it by this path |

A tool imports its siblings by module name whichever directory they are in.
Every script starts with the same two lines to make that true:

```python
sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
```

Checks import exporters as libraries (a pack is checked by rebuilding it),
so an exporter keeps its logic importable and its command line in `main()`.
