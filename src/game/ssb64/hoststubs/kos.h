/* host-test stand-in for <kos.h> in the BAKER build: what the
 * draw half of the port takes from it, and nothing that touches hardware. */
#ifndef HOSTSTUB_KOS_H
#define HOSTSTUB_KOS_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <dc/pvr.h>

#define DBG_CRITICAL 0
#define DBG_ERROR 1
#define DBG_WARNING 2
#define DBG_NOTICE 3
#define DBG_INFO 4
#define DBG_DEBUG 5
#define dbglog(lvl, ...) fprintf(stderr, __VA_ARGS__)
#define arch_abort() abort()
#endif
