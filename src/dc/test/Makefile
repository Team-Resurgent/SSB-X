# The src/dc/ host tests, run from scripts/test_host.sh (./run.sh test host).
# No KOS, no baserom.
#
# mtx_hosttest -- src/dc/mtx.h's algebra on the build host, both backends.
# input_hosttest -- src/dc/input.c's controller state machine against the
# N64 semantics of sys/controller.c.
#
# The SH4ZAM build here takes that library's SW fallback (SHZ_BACKEND is
# SHZ_SW off-target), so it checks algebra only. Accuracy is an on-target
# question; see the header comment in mtx_hosttest.c.

SH4ZAM_DIR ?= /opt/sh4zam
HOST_CFLAGS = -std=gnu11 -Wall -O2 -I..

# input.h takes SYController from the game's own sys/controller.h, so the
# input test needs the shim tree ahead of the decomp on the include path.
# See src/dc/decomp/README.md.
SSB_DECOMP_DIR ?= /opt/ssb-decomp-re
DECOMP_INC = -I../decomp -I$(SSB_DECOMP_DIR)/src \
             -idirafter $(SSB_DECOMP_DIR)/include

.PHONY: hosttest mtx_hosttest input_hosttest clean
hosttest: mtx_hosttest input_hosttest

input_hosttest: input_hosttest.c ../input.c ../input.h
	gcc -Wall -Wextra -DSY_INPUT_HOST -I.. $(DECOMP_INC) \
	    -o input_hosttest input_hosttest.c ../input.c
	./input_hosttest

mtx_hosttest: mtx_hosttest.c ../mtx.h
	gcc $(HOST_CFLAGS) -o mtx_hosttest_scalar mtx_hosttest.c -lm
	./mtx_hosttest_scalar
	@if [ -d "$(SH4ZAM_DIR)/include" ]; then \
	    gcc $(HOST_CFLAGS) -I$(SH4ZAM_DIR)/include \
	        -DMTX_BACKEND=MTX_BACKEND_SH4ZAM -DMTX_WITH_SH4ZAM \
	        -o mtx_hosttest_shz mtx_hosttest.c \
	        $(SH4ZAM_DIR)/source/sw/shz_xmtrx_sw.c -lm && ./mtx_hosttest_shz; \
	else \
	    echo "mtx_hosttest: SH4ZAM_DIR unset, skipping the sh4zam backend"; \
	fi

clean:
	-rm -f mtx_hosttest_scalar mtx_hosttest_shz input_hosttest
