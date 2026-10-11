"""Where the third-party trees the asset tools read actually live.

The decomp and gfxdis are not part of this repository; they come from the
build image at the revisions in docker/pins.env, which sets SSB_DECOMP_DIR
and SSB_GFXDIS. Every tool that touches them goes through this module, so
there is one place to point somewhere else.
"""
import os

DECOMP_DIR = os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re")

# audio_codec (VADPCM) and cseq_to_mid are imported from here.
DECOMP_TOOLS = os.path.join(DECOMP_DIR, "tools")
# Source the exporters parse: file ids come from relocData filenames, and
# offsets, attribute initializers and motion tables from the text itself.
RELOC_DIR = os.path.join(DECOMP_DIR, "src", "relocData")
FTDATA_SOURCE = os.path.join(DECOMP_DIR, "src", "ft", "ftdata.c")
GMSOUND_H = os.path.join(DECOMP_DIR, "src", "gm", "gmsound.h")

# glankk's reference display-list disassembler, for tools/check/verify_dl.py.
GFXDIS = os.environ.get("SSB_GFXDIS", "/opt/ssb64-dc/bin/gfxdis.f3dex2")


def require_decomp():
    """Fail with a usable message rather than deep in a regex."""
    if not os.path.isdir(RELOC_DIR):
        raise SystemExit(
            "no decomp at %s\n"
            "  Asset tools run in the container: scripts/ctr.sh run python3 tools/export/<tool>.py ...\n"
            "  (or point SSB_DECOMP_DIR at a checkout)" % DECOMP_DIR)
