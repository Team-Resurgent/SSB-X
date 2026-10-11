/* itvisuals.c -- it/itvisuals.c, verbatim, both functions. 27 decomp
 * lines: the item's per-frame col-anim tick and its spin-rotation step.
 */
#include <it/item.h>
#include <ft/fighter.h>

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#endif

/* it/itvisuals.c:9-18 itVisualsUpdateColAnim 0x801713B0, verbatim. */
void itVisualsUpdateColAnim(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);

    if (ftMainUpdateColAnim(&ip->colanim, item_gobj, FALSE, FALSE) != FALSE)
    {
        itMainClearColAnim(item_gobj);
    }
}

/* it/itvisuals.c:20-27 itVisualsUpdateSpin 0x801713F4, verbatim. */
void itVisualsUpdateSpin(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *dobj = DObjGetStruct(item_gobj);

    dobj->rotate.vec.f.z += ip->spin_step;
}
