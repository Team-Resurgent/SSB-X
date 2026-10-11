/* itmonster.h -- what every Poké Ball monster shares.
 *
 * The thirteen files under it/itmonster/ are one shape repeated: an
 * `ITDesc`, an `ITStatusDesc` table, a proc set that spends
 * ITMONSTER_RISE_STOP_WAIT frames levitating out of the ball before
 * handing over to the monster's real state machine, and a `MakeItem` that
 * puts the model on the GObj. Nothing here is a monster's own; it is the
 * four things all thirteen reach for.
 *
 * This is the monster half of `itMainMakeMonster`'s roster --
 * the kinds from `nITKindMBallMonsterStart` (32) to `nITKindMew` (44),
 * which is what `itMBallOpenProcUpdate` dispatches into and what a stage's
 * own `dITManagerProcMakeList` entries reach by name.
 */
#ifndef SSB_DC_ITMONSTER_H
#define SSB_DC_ITMONSTER_H

#include <it/item.h>

#include "itemoffsets.h"        /* llITCommonDataMonsterAnimBankStart */

/* The decomp's own macro (it/item.h:44) is
 *
 *     ((void *)(((uintptr_t)(ip)->attr->data - (intptr_t)(off)) +
 *               (intptr_t)&llITCommonDataMonsterAnimBankStart))
 *
 * which is `itGetPData(ip, off, &Bank)` written out: every monster's anim
 * scripts live in ONE bank at the end of ITCommonObject (offset 0x13624),
 * and a monster names its own entry in it by the offset of its MODEL block
 * -- which is exactly what `itGetPData` cancels. On the N64 `&Bank` is the
 * number 0x13624 because the symbol is the reloc file's own boundary
 * label; the port has no such label, so src/dc/itemoffsets.h carries the
 * number and this is the macro that uses it.
 *
 * The `#undef` is the point of this file: `<it/item.h>` is the decomp's,
 * unmodified, and it has already defined this name the N64 way. Left
 * alone it would expand to `&0x13624`, which is not an address. Every port
 * file that calls this includes THIS header after `<it/item.h>` -- the
 * same substitution, and the same reason, as the offsets themselves. */
#undef itGetMonsterAnimNode
#define itGetMonsterAnimNode(ip, off)                                                  \
    ((void *)(((uintptr_t)(ip)->attr->data - (intptr_t)(off)) +                        \
              (intptr_t)llITCommonDataMonsterAnimBankStart))

#endif /* SSB_DC_ITMONSTER_H */
