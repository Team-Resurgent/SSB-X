/* host-test stand-in for KOS <dc/pvr.h>.
 *
 * Without BAKER (the plain host suite): just the types fighter.h and stage.h
 * name; nothing is drawn. Never compiled for the target.
 *
 * With BAKER=1 (FT_BAKER): KOS's own header, found through the
 * Makefile's -idirafter, so the poly headers, vertices and constants are the
 * console's byte for byte; hoststubs/bakepvr.c is the pvr_* it links against.
 */
#ifndef HOSTSTUB_DC_PVR_H
#define HOSTSTUB_DC_PVR_H
#ifdef FT_BAKER
#include_next <dc/pvr.h>
#else
#include <stdint.h>
typedef void *pvr_ptr_t;
typedef struct { uint32_t cmd[8]; } pvr_poly_hdr_t;
#endif
#endif
