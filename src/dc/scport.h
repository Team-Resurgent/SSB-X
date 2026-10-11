#ifndef SSB64DC_SCPORT_H
#define SSB64DC_SCPORT_H

/* scport.h -- the scenes the port has and the game does not.
 *
 * sc/scdef.h's SCKind is the game's: every scene its scene manager can
 * run, nSCKindNoController to nSCKindAutoDemo. The port has one more, the
 * memory card's (src/dc/dcmemcard.h), which a cartridge never needed. It
 * takes a number well past the game's last, so no decomp table indexed by
 * scene kind can mistake it for one of the game's, and inside a u8,
 * because gSCManagerSceneData.scene_curr and scene_prev are u8
 * (sc/sctypes.h:380).
 *
 * Every place that switches on a scene kind and should know it --
 * src/dc/scmanager.c's arm and sc_scene_name, src/dc/sndres.c's
 * sndres_compose and sndres_scene_name, src/dc/db.c's heartbeat list --
 * names it from here. */

#include <sc/scdef.h>

/* the memory card's scene: its information page, the boot check and the
 * failure prompt (src/dc/dcmemcard.h) */
#define nSCKindDCMemCard 0x80

_Static_assert(nSCKindDCMemCard > nSCKindAutoDemo,
               "a port scene must not take one of the game's numbers");
_Static_assert(nSCKindDCMemCard <= 0xFF,
               "scene_curr and scene_prev are u8");

#endif /* SSB64DC_SCPORT_H */
