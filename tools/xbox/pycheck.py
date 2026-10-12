"""build.bat's test of a Python: 64-bit 3.11-3.14 prints its own path and
exits 0, anything else exits 1. (numpy 2.3.5 and pygfxd 1.0.5 have prebuilt
Windows wheels for exactly these, so nothing has to be compiled.)"""
import struct
import sys

if (3, 11) <= sys.version_info[:2] <= (3, 14) and struct.calcsize("P") == 8:
    print(sys.executable)
    sys.exit(0)
sys.exit(1)
