/* efmanager.c -- ef/efmanager.c, as much of it as has a model to draw.
 *
 * The effect manager is 6,234 lines and 159 functions, and it splits
 * cleanly in two. Seventy-four of those functions build a GObj out of an
 * EFDesc -- a DObj tree, an MObj chain and two animation joints, read
 * out of the three llEFCommonEffects files -- through
 * efManagerMakeEffect. That path is the N64 display list's, and the port
 * replaced it with packs and src/dc/fighter.c: an effect on that path
 * arrives here one pack at a time, as tools/export/ssb_effectexport.py bakes
 * it. The rest reaches straight for lb/lbparticle.c, which this port has.
 *
 * So this file is the manager's spine -- the EFStruct pool, the four
 * update procs, the two Z sorters, the run hook, and the three
 * functions that give a struct back -- with every particle-only maker
 * above it and the model-path makers that have a pack:
 *
 *   efManagerSetOffMakeEffect              the clash flash, script 0x65
 *   efManagerDamageFireMakeEffect          script 0x4D
 *   efManagerDamageElectricMakeEffect      script 0x53
 *   efManagerDamageCoinMakeEffect          script 0x60
 *   efManagerDamageNormalLightMakeEffect   scripts 0x49..0x4C, by player
 *   efManagerDamageNormalHeavyMakeEffect   script 0x64, then a Light
 *   efManagerSparkleWhiteDeadMakeEffect    the star-KO twinkle, 0x5C
 *   efManagerDustExpandSmallMakeEffect     what a dying coin leaves
 *   efManagerDustCollideMakeEffect         the boomerang's puff, the
 *                                          same script 0x55
 *   efManagerConfettiMakeEffect            the results screen's, script
 *                                          0x70 twice
 *
 *   efManagerRebirthHaloMakeEffect         efhalo.mdl
 *   efManagerQuakeMakeEffect               efquake.mdl
 *   efManagerDamageSpawnOrbsMakeEffect     eforbs.mdl
 *   efManagerDamageSlashMakeEffect         efslash.mdl
 *   efManagerDamageSpawnSparksMakeEffect   efspark.mdl
 *   efManagerDamageSpawnMDustMakeEffect    efmdust.mdl
 *   efManagerDeadExplodeMakeEffect         efdexp.mdl
 *   efManagerImpactWaveMakeEffect          efimpactwave.mdl
 *
 * The makers once stubbed in src/dc/ftcommon.c for want of a bake or a
 * per-fighter particle bank -- Kirby's Final Cutter four and his two
 * stars, Yoshi's egg effects, Kirby's inhale wind -- are all here now,
 * verbatim, with their packs and banks loaded, as are the Reflector's hexagon,
 * Falcon Punch's flame, Falcon Kick's trail and Sing's notes. The impact wave
 * joined the list when the wall bounce (ftcommonwalldamage.c) became the first
 * code to make it -- its two older callers reached it through ftParamMakeEffect,
 * then still a stub. The dead explosion was left until last because it is
 * the only one whose descriptor names four MatAnimJoints -- one per
 * player, swapped into the shared EFDesc before the effect is made --
 * and the only one whose combiner reads the environment colour, which
 * is where the player's colour arrives. Both were new when it landed;
 * see the maker, and fighter.h's FPACK_ENVLERP.
 *
 * DIVERGES, three times, each at the function and each held to what it
 * may drop by tools/check/efmanager_check.py:
 *
 *   efManagerInitEffects drops the three lbRelocGetExternHeapFile calls
 *   that read llEFCommonEffects1/2/3 into the scene heap. Nothing in
 *   this file reads gEFManagerFiles: the readers are the EFDescs and
 *   efManagerMakeEffect, and here a model comes out of a pack. Loading
 *   them would spend scene heap on data no code touches.
 *
 *   efManagerMakeEffect's four branches on flags 0x1 and 0x4 become one
 *   call to efManagerAddModel, which builds the same tree out of the
 *   pack. See the long note there.
 *
 *   efManagerQuakeMakeEffect's four lbRelocGetFileData calls become
 *   efQuakeAnimJoint(magnitude) over the pack the same four AnimJoint
 *   blocks were baked into.
 *
 * Everything else is ef/efmanager.c character for character;
 * tools/check/efmanager_check.py names the runs and holds them to it.
 */
#include <ssb_types.h>
#include <sys/obj.h>
#include <sys/utils.h>
#include <sys/debug.h>
#include <sys/taskman.h>
#include <lb/library.h>
#include <ef/effect.h>
#include <ft/fighter.h>
#include <wp/weapon.h>         /* WPStruct: Ness's PK Thunder trails */
#include <gm/generic.h>
#include <reloc_data.h>

#include <stdlib.h>
#include <string.h>

#include "fighter.h"
#include "objmodel.h"
#include "efmanager.h"
#include "dma.h"
#include "lbparticle.h"
#include "lbpartex.h"
#include "taskman.h"


/* ---- efmanager.c:14-18: the three angles a damage spark leaves at
 * and the three a speck of metal dust leaves at, which are the same three.
 * Each spawner fires on three of its
 * eight tics and reads its own table backwards -- see
 * efManagerDamageSpawnSparksProcUpdate below -- so the first goes
 * eighteen degrees up, the second straight out and the third eighteen
 * down. Two tables and not one because the game has two, at two
 * addresses, and neither proc reads the other's. ---- */

// 0x8012DE80
f32 dEFManagerDamageSpawnSparksAngles[/* */] = { 18.0F, 0.0F, -18.0F };

// 0x8012DE8C
f32 dEFManagerDamageSpawnMDustAngles[/* */] = { 18.0F, 0.0F, -18.0F };


/* ---- efmanager.c:20-36, 77-78, 1712-1723: the manager's data and its
 * four file-scope variables. The colour tables are the ones a heavy
 * normal hit tints its particle by, per player; the ids are the four
 * light-hit scripts, one per player. ---- */

// 0x8012DE98
u8 dEFManagerDamageNormalHeavyPrimColorR[/* */] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// 0x8012DEA0
u8 dEFManagerDamageNormalHeavyPrimColorG[/* */] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// 0x8012DEA8
u8 dEFManagerDamageNormalHeavyPrimColorB[/* */] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// 0x8012DEB0
u8 dEFManagerDamageNormalHeavyEnvColorR[/* */] = { 0xFF, 0x00, 0x00, 0x78, 0xFF };

// 0x8012DEB8
u8 dEFManagerDamageNormalHeavyEnvColorG[/* */] = { 0x00, 0xFF, 0x00, 0x78, 0xFF };

// 0x8012DEC0
u8 dEFManagerDamageNormalHeavyEnvColorB[/* */] = { 0x00, 0x00, 0xFF, 0x78, 0xFF };

/* ---- efmanager.c:39-45: the impact wave's prim colour rows.
 * The caller passes a SYVECTOR_AXIS_* (0=X, 1=Y, 2=Z) as the index;
 * ftcommonwalldamage.c asks for SYVECTOR_AXIS_Z (2), the blue row. The
 * env rows (efmanager.c:48-54) are all zero and the DAIRANTOU_OPT0 build
 * hardcodes (0,0,0,0xFF) instead, so the port drops them the way that
 * build does and its ProcDisplay writes a flat black env. ---- */

// 0x8012DEC8
u8 dEFManagerImpactWavePrimColorR[/* */] = { 0xFF, 0x00, 0x00, 0xFF, 0xFF };

// 0x8012DED0
u8 dEFManagerImpactWavePrimColorG[/* */] = { 0x00, 0xFF, 0x00, 0xFF, 0xFF };

// 0x8012DED8
u8 dEFManagerImpactWavePrimColorB[/* */] = { 0x00, 0x00, 0xFF, 0x00, 0xFF };

/* ---- efmanager.c:57-75: the dead explosion's per-player colours.
 *
 * Two of the burst's three shards take a colour from the player who
 * died: the first child gets the Child triple and the third sibling the
 * Sibling triple, and both go into the *environment* colour of that
 * shard's MObj (efmanager.c:4825-4835). The Sibling triple is the four
 * player colours -- FF624B, 007EFF, FFFF00, 00FF00, red blue yellow
 * green -- and the Child triple is the darker inner one. ENV is half of
 * what each shard's combiner computes, lerp(ENV, PRIM, TEXEL0), and
 * PRIM is the other half and comes from whichever of the four
 * MatAnimJoints the player picks. ---- */

// 0x8012DEF8
u8 dEFManagerDeadExplodeEnvColorSiblingR[/* */] = { 0xFF, 0x00, 0xFF, 0x00 };

// 0x8012DEFC
u8 dEFManagerDeadExplodeEnvColorSiblingG[/* */] = { 0x62, 0x7E, 0xFF, 0xFF };

// 0x8012DF00
u8 dEFManagerDeadExplodeEnvColorSiblingB[/* */] = { 0x4B, 0xFF, 0x00, 0x00 };

// 0x8012DF04
u8 dEFManagerDeadExplodeEnvColorChildR[/* */] = { 0xA6, 0x1F, 0x3E, 0xFB };

// 0x8012DF08
u8 dEFManagerDeadExplodeEnvColorChildG[/* */] = { 0x62, 0xFF, 0x6D, 0x66 };

// 0x8012DF0C
u8 dEFManagerDeadExplodeEnvColorChildB[/* */] = { 0x21, 0xA1, 0xFF, 0xC7 };

/* Which way up the burst is: type 0 is a top blast-line KO, 1 and 3 the
 * two sides, and ft/ftcommon/ftcommondead.c passes 0, 1 and 3 from its
 * three dead statuses. Type 2 is in the table and nothing asks for it. */
// 0x8012DF10
f32 dEFManagerDeadExplodeRotateD[/* */] = { 0.0F, 90.0F, 180.0F, 270.0F };

// 0x8012DF20
u8 dEFManagerDamageNormalLightIDs[/* */] = { 0x49, 0x4A, 0x4B, 0x4C };


/* The damage slash's four blocks. The names are the
 * generated header's; the values are the port's, which is to say none --
 * as with every other effect, what the offsets pick out of relocData
 * file 83 has been baked into a pack and efModelPackFor keys on the
 * descriptor instead. This is the first descriptor the port reads that
 * names all four: the MObjSub table and the MatAnimJoint are here, and
 * they are what makes the slash a moving picture rather than a still
 * one. */
int llEFCommonEffects1DamageSlashDObjDesc;
int llEFCommonEffects1DamageSlashMObjSub;
int llEFCommonEffects1DamageSlashAnimJoint;
int llEFCommonEffects1DamageSlashMatAnimJoint;


/* ---- efmanager.c:80-108: the damage slash.
 *
 * Flags 0x4 | 0x1: a DObjDesc *tree* -- three joints, a stand and the
 * two quads over it -- read by lbCommonSetupTreeDObjs, which is the
 * branch the halo takes and the orbs do not. transform_types1.tk1 is
 * 0x28, the camera-facing billboard, so the streak faces the camera
 * wherever the hit was; the two under it are TraRotRpyRSca.
 *
 * Each of the two drawn joints has one MObj, and each of those has a
 * sprite array its MatAnimJoint steps through: eight frames for the
 * streak and five for the flash, over thirteen and five tics, while the
 * primitive colour fades. ---- */

// 0x8012DF24
EFDesc dEFManagerDamageSlashEffectDesc = 
{
    0x4 | 0x1,                              // Flags
    18,                                     // DL Link
    &gEFManagerFiles[0],                      // Texture file

    // DObj transformation struct 1
    {
        0x28,                               // Main matrix transformations   
        0x45,                               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,     // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llEFCommonEffects1DamageSlashDObjDesc,    // DObj Setup attributes offset (?)
    &llEFCommonEffects1DamageSlashMObjSub,     // MObjSub offset
    &llEFCommonEffects1DamageSlashAnimJoint,   // AnimJoint offset
    &llEFCommonEffects1DamageSlashMatAnimJoint // MatAnimJoint offset
};


/* The same for the damage orbs' two blocks. The names are
 * the generated header's; the values are the port's, which is to say
 * none. */
int llEFCommonEffects1FlyOrbsDObjDesc;
int llEFCommonEffects1FlyOrbsAnimJoint;


/* ---- efmanager.c:140-198: the damage orbs' two EFDescs.
 *
 * They are a pair and only the second is ever asked for by name: a hit
 * makes a *spawner*, which has no model and no display proc at all --
 * efManagerMakeEffect returns as soon as it sees proc_display NULL --
 * and whose update makes a flying orb every fourth tic until its
 * lifetime runs out. The flyer is the one with the model.
 *
 * The flyer's flags are EFFECT_FLAG_USERDATA | 0x1 and not 0x4, which
 * is the one branch of efManagerMakeEffect the halo did not take: the
 * DObj tree is not read out of a DObjDesc array but built by hand, a
 * stand carrying transform_types1 with one child carrying
 * transform_types2 and the display list. So o_dobjsetup points at
 * F3DEX2 and not at descriptors, whatever the reloc symbol is called,
 * and tools/export/ssb_effectexport.py bakes those two DObjs as the pack's two
 * joints.
 *
 * transform_types1.tk1 is 0x28 -- matrix kind 40, the camera-facing
 * billboard (src/dc/objdisplay.c gcMtxBillboard) -- which is what makes
 * a flat quad a round orb from any angle. ---- */

// 0x8012DF74
EFDesc dEFManagerDamageFlyOrbsEffectDesc =
{
    EFFECT_FLAG_USERDATA | 0x1,             // Flags
    15,                                     // DL Link
    &gEFManagerFiles[0],                      // Texture file

    // DObj transformation struct 1
    {
        0x28,                               // Main matrix transformations   
        nGCMatrixKindSca,                // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindSca,                // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerDamageFlyOrbsProcUpdate,    // Proc Update
    lbCommonDObjScaleXProcDisplay,       // Proc Render

    &llEFCommonEffects1FlyOrbsDObjDesc,     // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    &llEFCommonEffects1FlyOrbsAnimJoint,    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

// 0x8012DF9C
EFDesc dEFManagerDamageSpawnOrbsEffectDesc =
{
    EFFECT_FLAG_USERDATA,                   // Flags
    0,                                      // DL Link
    &gEFManagerFiles[0],                      // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindNull,               // Main matrix transformations   
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindNull,               // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerDamageSpawnOrbsProcUpdate,  // Proc Update
    NULL,                                   // Proc Render

    0x0,                                    // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* reloc_data.us.h's declarations for the impact wave's four blocks,
 * defined here so the EFDesc below links -- zero, the slash's and the
 * halo's reason exactly: the game gives a reloc symbol its value in the
 * link (0x7C28 and its neighbours in relocData file 83) and the port does
 * not. tools/export/ssb_effectexport.py --what impactwave reads those offsets
 * out of the generated header and bakes efimpactwave.mdl at them, so
 * nothing at runtime needs the values. */
int llEFCommonEffects1ImpactWaveDObjDesc;
int llEFCommonEffects1ImpactWaveMObjSub;
int llEFCommonEffects1ImpactWaveAnimJoint;
int llEFCommonEffects1ImpactWaveMatAnimJoint;

/* ---- efmanager.c:201-227: the impact wave's descriptor.
 * EFFECT_FLAG_USERDATA alone (0x2), so efManagerMakeEffect's simplest
 * branch builds it -- one DObj with the DL at o_dobjsetup bound straight
 * to it -- and efManagerAddModel loads efimpactwave.mdl for it (the model
 * table below). Verbatim; the reloc offsets are the four symbols just
 * defined, used only by the N64 path the port replaces with the pack. */

// 0x8012DFC4
EFDesc dEFManagerImpactWaveEffectDesc =
{
    EFFECT_FLAG_USERDATA,                   // Flags
    10,                                     // DL Link
    &gEFManagerFiles[0],                      // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations   
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindNull,               // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerImpactWaveProcUpdate,       // Proc Update
    efManagerImpactWaveProcDisplay,      // Proc Render

    &llEFCommonEffects1ImpactWaveDObjDesc,    // DObj Setup attributes offset (?)
    &llEFCommonEffects1ImpactWaveMObjSub,     // MObjSub offset
    &llEFCommonEffects1ImpactWaveAnimJoint,   // AnimJoint offset
    &llEFCommonEffects1ImpactWaveMatAnimJoint // MatAnimJoint offset
};

/* The CommonSpark's four blocks, and the same again: the
 * names are the generated header's and the values are the port's, which
 * is to say none. `Common` is not decoration -- dEFManagerStarRodSparkEffectDesc
 * (efmanager.c:231) names the same four, so the pack under them is one
 * model two effects draw, and efModelPackFor keys on the descriptor. */
int llEFCommonEffects1CommonSparkDObjDesc;
int llEFCommonEffects1CommonSparkMObjSub;
int llEFCommonEffects1CommonSparkAnimJoint;
int llEFCommonEffects1CommonSparkMatAnimJoint;

/* the star rod's sparks are ported now, and the comment
 * above was wrong about why not -- there is no efstarrod.mdl to unpack.
 * dEFManagerStarRodSparkEffectDesc reuses the same four blocks the
 * damage sparks do, already loaded as efspark.mdl since  18;
 * efModelPackFor keys on the EFDesc pointer, not the offsets it holds,
 * so this desc gets its own row in sEFManagerModels pointing at the
 * same file rather than a new one. The item that swings this is
 * it/itcommon/itstarrod.c; this is ft/ftparam.c's arm of
 * ftParamMakeEffect. */

// 0x8012DFEC
EFDesc dEFManagerStarRodSparkEffectDesc =
{
    EFFECT_FLAG_USERDATA | 0x1,             // Flags
    15,                                     // DL Link
    &gEFManagerFiles[0],                      // Texture file

    // DObj transformation struct 1
    {
        0x28,                               // Main matrix transformations   
        0x45,                               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        0x45,                               // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerStarRodSparkProcUpdate,     // Proc Update
    lbCommonDObjScaleXProcDisplay,       // Proc Render

    &llEFCommonEffects1CommonSparkDObjDesc,    // DObj Setup attributes offset (?)
    &llEFCommonEffects1CommonSparkMObjSub,     // MObjSub offset
    &llEFCommonEffects1CommonSparkAnimJoint,   // AnimJoint offset
    &llEFCommonEffects1CommonSparkMatAnimJoint // MatAnimJoint offset
};


/* ---- efmanager.c:260-318: the damage sparks' two EFDescs.
 *
 * A pair like the orbs', and read the same way round: a hit makes the
 * *spawner*, which has no model, no display proc and all four offsets
 * zero, and every fourth tic of its eight it makes a *flyer* that does.
 *
 * The flyer's flags are EFFECT_FLAG_USERDATA | 0x1 and not 0x4, so
 * o_dobjsetup is a display list rather than a DObjDesc array and the
 * tree is two DObjs built by hand (efmanager.c:1995-2000): a stand
 * carrying transform_types1 -- tk1 0x28, the camera-facing billboard --
 * and one child carrying transform_types2 and the list.
 *
 * What the orbs did not have is the picture. The child's one MObj owns
 * a sprite array of seven 32x32 frames and a MatAnimJoint that steps
 * nGCAnimTrackTextureIDCurrent through it, which is the machinery
 * built for the slash. And its MObjSub's flags word is *zero*,
 * which gcDrawMObjForDObj (objdisplay.c:1159) reads as
 * MOBJ_FLAG_TEXTURE | 0x20 | MOBJ_FLAG_ALPHA rather than as nothing --
 * ALPHA being the bit that makes the sprite array live at all.
 * tools/check/spark_check.py is the check on that. ---- */

// 0x8012E014
EFDesc dEFManagerDamageFlySparksEffectDesc =
{
    EFFECT_FLAG_USERDATA | 0x1,             // Flags
    15,                                     // DL Link
    &gEFManagerFiles[0],                      // Texture file

    // DObj transformation struct 1
    {
        0x28,                               // Main matrix transformations   
        nGCMatrixKindRotRpyR,            // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        0x45,                               // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerDamageFlySparksProcUpdate,  // Proc Update
    lbCommonDObjScaleXProcDisplay,       // Proc Render

    &llEFCommonEffects1CommonSparkDObjDesc,    // DObj Setup attributes offset (?)
    &llEFCommonEffects1CommonSparkMObjSub,     // MObjSub offset
    &llEFCommonEffects1CommonSparkAnimJoint,   // AnimJoint offset
    &llEFCommonEffects1CommonSparkMatAnimJoint // MatAnimJoint offset
};

// 0x8012E03C
EFDesc dEFManagerDamageSpawnSparksEffectDesc =
{
    EFFECT_FLAG_USERDATA,                   // Flags
    0,                                      // DL Link
    &gEFManagerFiles[0],                      // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindNull,               // Main matrix transformations   
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindNull,               // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerDamageSpawnSparksProcUpdate,// Proc Update
    NULL,                                   // Proc Render

    0x0,                                    // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* The DamageFlyMDust's four blocks, and for the third time
 * the names are the generated header's and the values are the port's,
 * which is to say none. Unlike the CommonSpark above, these are one
 * effect's alone. */
int llEFCommonEffects1DamageFlyMDustDObjDesc;
int llEFCommonEffects1DamageFlyMDustMObjSub;
int llEFCommonEffects1DamageFlyMDustAnimJoint;
int llEFCommonEffects1DamageFlyMDustMatAnimJoint;


/* ---- efmanager.c:320-378: the metal dust's two EFDescs.
 *
 * The sparks again, and almost to the letter: the same pair, the same
 * eight tics, the same modulo four, the same three angles, and a flyer
 * that shares efManagerDamageFlySparksProcUpdate outright. What it does
 * not share is the material -- llEFCommonEffects1DamageFlyMDust* are
 * four blocks of its own, another seven 32x32 frames and another
 * MatAnimJoint stepping through them.
 *
 * Two fields differ from dEFManagerDamageFlySparksEffectDesc, and both
 * are read at runtime here rather than only at bake time:
 *
 *   proc_display is gcDrawDObjTreeDLLinksForGObj and not
 *   lbCommonDObjScaleXProcDisplay. The two read the pointer flags 0x1
 *   hands gcAddChildForDObj differently -- the first as a DObjDLLink
 *   array {s32 list_id; Gfx *dl} terminated by list_id == 4
 *   (objdisplay.c:2380-2392), the second as the commands themselves --
 *   which is why tools/export/ssb_effectexport.py walks the array for this one
 *   and takes the display list straight for the sparks. The array holds
 *   one link, into DL head 1, which is the head the sparks reach
 *   directly; tools/check/mdust_check.py says so.
 *
 *   transform_types2.tk1 is 0x44 where the sparks' is 0x45. Both go on
 *   the child's XObj through efManagerSetTransformTypes as they always
 *   have.
 *
 * And the MObjSub's flags word is zero here too, so everything 
 * 18 wrote about gcDrawMObjForDObj (objdisplay.c:1159) reading that as
 * MOBJ_FLAG_TEXTURE | 0x20 | MOBJ_FLAG_ALPHA holds for this effect as
 * well -- tools/check/flipbook_check.py is now the check on both. ---- */

// 0x8012E064
EFDesc dEFManagerDamageFlyMDustEffectDesc = 
{
    EFFECT_FLAG_USERDATA | 0x1,             // Flags
    15,                                     // DL Link
    &gEFManagerFiles[0],                      // Texture file

    // DObj transformation struct 1
    {
        0x28,                               // Main matrix transformations   
        nGCMatrixKindRotRpyR,            // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        0x44,                               // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerDamageFlySparksProcUpdate,  // Proc Update
    gcDrawDObjTreeDLLinksForGObj,        // Proc Render

    &llEFCommonEffects1DamageFlyMDustDObjDesc,     // DObj Setup attributes offset (?)
    &llEFCommonEffects1DamageFlyMDustMObjSub,      // MObjSub offset
    &llEFCommonEffects1DamageFlyMDustAnimJoint,    // AnimJoint offset
    &llEFCommonEffects1DamageFlyMDustMatAnimJoint  // MatAnimJoint offset
};

// 0x8012E08C
EFDesc dEFManagerDamageSpawnMDustEffectDesc =
{
    EFFECT_FLAG_USERDATA,                   // Flags
    0,                                      // DL Link
    &gEFManagerFiles[0],                      // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindNull,               // Main matrix transformations   
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindNull,               // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerDamageSpawnMDustProcUpdate, // Proc Update
    NULL,                                   // Proc Render

    0x0,                                    // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* The Reflector's five reloc offsets, the same stand-in
 * shape as llFTManagerCommonShieldDObjDesc below: real symbols out of the
 * generated header, defined here at zero because the port never runs the
 * real relocation. gFTDataFoxSpecial2 itself is ft/ftchar/ftfox/ftfox.c's,
 * compiled for real since  25. */
int llFoxSpecial2ReflectorDObjDesc;
int llFoxSpecial2ReflectorStartAnimJoint;
int llFoxSpecial2ReflectorLoopAnimJoint;
int llFoxSpecial2ReflectorHitAnimJoint;
int llFoxSpecial2ReflectorEndAnimJoint;

/* ---- efmanager.c:410-447 dEFManagerFoxReflectorAnimJointOffsets and
 * dEFManagerFoxReflectorEffectDesc: the Reflector's hexagon,
 * with its four animations (start, loop, hit, end) picked by
 * efManagerFoxReflectorSetAnimID below. Verbatim. No sEFManagerModels row
 * yet, so efManagerAddModel refuses it on both host and target -- see
 * efManagerFoxReflectorMakeEffect below. ---- */
// 0x8012E0DC
intptr_t dEFManagerFoxReflectorAnimJointOffsets[/* */] = 
{ 
    &llFoxSpecial2ReflectorStartAnimJoint, 
    &llFoxSpecial2ReflectorLoopAnimJoint, 
    &llFoxSpecial2ReflectorHitAnimJoint, 
    &llFoxSpecial2ReflectorEndAnimJoint 
};

// 0x8012E0EC
EFDesc dEFManagerFoxReflectorEffectDesc = 
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    15,                                     // DL Link
    &gFTDataFoxSpecial2,                    // Texture file

    // DObj transformation struct 1
    {
        0x4F,                               // Main matrix transformations   
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTra,                // Main matrix transformations  
        0x2C,                               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerFoxReflectorProcUpdate,      // Proc Update
    gcDrawDObjTreeForGObj,                // Proc Render

    &llFoxSpecial2ReflectorDObjDesc,        // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    &llFoxSpecial2ReflectorStartAnimJoint,  // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* ---- efmanager.c:449-517: the shield's colours and its two EFDescs.
 * Verbatim; what differs is the four symbols they name.
 *
 * gFTManagerCommonFile is ft/ftmanager.c:43, the common fighter file
 * ftManagerInitFighters reads off the ROM for every battle, and
 * gFTDataYoshiModel is ft/ftchar/ftyoshi/ftyoshi.c:10, Yoshi's model
 * file; neither file is loaded by the port, whose fighters come out of
 * packs (src/dc/ftmanager.c). The two DObjDesc offsets are the
 * generated header's names for where in those files the models sit.
 * Three of the four are defined here at zero, as every other reloc
 * symbol above is; gFTDataYoshiModel stood in here too from  5
 * until , when ft/ftchar/ftyoshi/ftyoshi.c itself joined the
 * build (Yoshi's Down-B needs its gFTDataYoshiMain), so the real file's
 * bss defines it now and the declaration comes in through ft/fighter.h.
 * The models come out of efshield.mdl and efyegg.mdl instead
 * (tools/export/ssb_effectexport.py --what shield, --what yoshiegg; the table
 * below names them). Yoshi's "DObjDesc" is a display list: the
 * descriptor's flags carry no 0x4, so the game hands it to
 * gcAddDObjForGObj whole (efmanager.c:2047). */
void *gFTManagerCommonFile;
int llFTManagerCommonShieldDObjDesc;
int llYoshiModelShieldDObjDesc;

// 0x8012E114
SYColorRGBPair dEFManagerShieldColors[/* */] =
{
    { { 0xFF, 0xFF, 0xFF }, { 0xFF, 0x00, 0x00 } }, // Player 1
    { { 0xFF, 0xFF, 0xFF }, { 0x00, 0xFF, 0x00 } }, // Player 2
    { { 0xFF, 0xFF, 0xFF }, { 0x00, 0x00, 0xFF } }, // Player 3
    { { 0xFF, 0xFF, 0xFF }, { 0x00, 0x00, 0x00 } }, // Player 4 / CPU
    { { 0xFF, 0xFF, 0xFF }, { 0xC0, 0xC0, 0xC0 } }  // Shield Damage
};

// 0x8012E134
EFDesc dEFManagerShieldEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    15,                                     // DL Link
    &gFTManagerCommonFile,                         // Texture file

    // DObj transformation struct 1
    {
        0x4F,                               // Main matrix transformations   
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        0x2C,                               // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerShieldProcUpdate,                     // Proc Update
    efManagerShieldProcDisplay,                    // Proc Render

    &llFTManagerCommonShieldDObjDesc,       // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

// 0x8012E15C
EFDesc dEFManagerYoshiShieldEffectDesc =
{
    EFFECT_FLAG_USERDATA,                   // Flags
    15,                                     // DL Link
    &gFTDataYoshiModel,                     // Texture file

    // DObj transformation struct 1
    {
        0x50,                               // Main matrix transformations   
        0x2C,                               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindNull,               // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerShieldProcUpdate,                     // Proc Update
    efManagerYoshiShieldProcDisplay,               // Proc Render

    &llYoshiModelShieldDObjDesc,            // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* gFTDataCaptainSpecial2 stands in for a file this port never compiles
 * (Captain's Special2 bank has no other reason to exist yet), the same
 * shape as gFTDataYoshiModel above and gFTDataCaptainSpecial3 below --
 * Falcon Kick's own texture file, distinct from Falcon Punch's. */
void *gFTDataCaptainSpecial2;

/* Falcon Kick's four reloc offsets, the same stand-in shape
 * as llFTManagerCommonShieldDObjDesc above: real symbols out of the
 * generated header, defined here at zero because the port never runs the
 * real relocation and reads the model through sEFManagerModels instead.
 * Unlike Falcon Punch's own trio below, the decomp names a real AnimJoint
 * offset for this one too, so all four are declared. */
int llCaptainSpecial2FalconKickDObjDesc;
int llCaptainSpecial2FalconKickMObjSub;
int llCaptainSpecial2FalconKickAnimJoint;
int llCaptainSpecial2FalconKickMatAnimJoint;

/* ---- efmanager.c:760-787 dEFManagerCaptainFalconKickEffectDesc:
 * the dust cloud/trail Falcon Kick's boot leaves as Captain
 * kicks. Verbatim -- a real DObjDesc/MObjSub/AnimJoint/MatAnimJoint
 * quartet, all four reloc symbols named for real (unlike Falcon Punch's
 * flame below, which leaves its AnimJoint a literal 0x0). Its model is
 * effalconkick.mdl. ---- */
// 0x8012E2C4
EFDesc dEFManagerCaptainFalconKickEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    15,                                     // DL Link
    &gFTDataCaptainSpecial2,                // Texture file

    // DObj transformation struct 1
    {
        0x50,                               // Main matrix transformations
        nGCMatrixKindRotRpyR,            // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerNoEjectProcUpdate,                     // Proc Update
    gcDrawDObjTreeForGObj,                // Proc Render

    &llCaptainSpecial2FalconKickDObjDesc,                // DObj Setup attributes offset (?)
    &llCaptainSpecial2FalconKickMObjSub,                  // MObjSub offset
    &llCaptainSpecial2FalconKickAnimJoint,                // AnimJoint offset
    &llCaptainSpecial2FalconKickMatAnimJoint              // MatAnimJoint offset
};

/* gFTDataCaptainSpecial3 stands in for a file this port never compiles
 * (ft/ftchar/ftcaptain/ftcaptain.c is not in FTCHAR_DATA_OBJS), the same
 * shape as gFTDataYoshiModel above -- Captain's own data file is not
 * needed for anything else this step, unlike Purin's below. */
void *gFTDataCaptainSpecial3;

/* Falcon Punch's three reloc offsets, the same stand-in
 * shape as llFTManagerCommonShieldDObjDesc above: real symbols out of the
 * generated header, defined here at zero because the port never runs the
 * real relocation and reads the model through sEFManagerModels instead.
 * The decomp leaves the AnimJoint offset a literal 0x0 (Falcon Punch's
 * flame has no joint animation of its own), so only three are declared. */
int llCaptainSpecial3FalconPunchDObjDesc;
int llCaptainSpecial3FalconPunchMObjSub;
int llCaptainSpecial3FalconPunchMatAnimJoint;

/* ---- efmanager.c:790-817 dEFManagerCaptainFalconPunchEffectDesc:
 * the flame Falcon Punch attaches to Captain's fist while
 * charged. Verbatim -- same shape as the Sing descriptor below (a real
 * DObjDesc/MObjSub/MatAnimJoint trio, AnimJoint left a literal 0x0). Its
 * model is effalconpunch.mdl. ---- */
// 0x8012E27C
EFDesc dEFManagerCaptainFalconPunchEffectDesc =
{
    EFFECT_FLAG_USERDATA,                   // Flags
    15,                                     // DL Link
    &gFTDataCaptainSpecial3,                // Texture file

    // DObj transformation struct 1
    {
        0x50,                               // Main matrix transformations
        nGCMatrixKindRotRpyR,            // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindNull,               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerNoEjectProcUpdate,                     // Proc Update
    lbCommonDObjScaleXProcDisplay,                     // Proc Render

    &llCaptainSpecial3FalconPunchDObjDesc,               // DObj Setup attributes offset (?)
    &llCaptainSpecial3FalconPunchMObjSub,                 // MObjSub offset
    0x0,                                    // AnimJoint offset
    &llCaptainSpecial3FalconPunchMatAnimJoint             // MatAnimJoint offset
};

/* gFTDataPurinSpecial2 is ft/ftchar/ftpurin/ftpurin.c's own -- real this
 * time (unlike gFTDataYoshiModel above, which stands in for a file this
 * port never compiles), since  42 adds ftpurin.o to FTCHAR_DATA_OBJS
 * for exactly this global. */
extern void *gFTDataPurinSpecial2;

/* Sing's four reloc offsets, the same stand-in-for-relocData
 * shape as llFTManagerCommonShieldDObjDesc above: real symbols out of the
 * generated header, defined here at zero because the port never runs the
 * real relocation and reads the model through sEFManagerModels instead. */
int llPurinSpecial2SingDObjDesc;
int llPurinSpecial2SingMObjSub;
int llPurinSpecial2SingAnimJoint;
int llPurinSpecial2SingMatAnimJoint;

/* ---- efmanager.c:820-847 dEFManagerPurinSingEffectDesc:
 * the note cloud Sing spawns at Purin's TopN joint. Verbatim -- unlike the
 * shield pair above, whose MObjSub/AnimJoint/MatAnimJoint offsets are all
 * genuinely zero in the decomp, this one names all four reloc symbols for
 * real, so all four are declared rather than three of them left as literal
 * 0x0. Its model is efsing.mdl. ---- */
// 0x8012E314
EFDesc dEFManagerPurinSingEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    15,                                     // DL Link
    &gFTDataPurinSpecial2,                  // Texture file

    // DObj transformation struct 1
    {
        0x4F,                               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTra,                // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,          // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llPurinSpecial2SingDObjDesc,                 // DObj Setup attributes offset (?)
    &llPurinSpecial2SingMObjSub,                   // MObjSub offset
    &llPurinSpecial2SingAnimJoint,                 // AnimJoint offset
    &llPurinSpecial2SingMatAnimJoint               // MatAnimJoint offset
};

/* efmanager.c:580-607 dEFManagerPikachuUnkEffectDesc, the
 * last of ftParamMakeEffect's forty-five arms with a maker to write.
 * Flags 0x1|0x4 together -- a DObjDesc tree walked onto a SEPARATE stand
 * DObj rather than the
 * effect's own (the rebirth halo's own shape). Its four reloc symbols,
 * the same stand-in shape as every other block above; gFTDataPikachu-
 * Special2 itself is ft/ftchar/ftpikachu/ftpikachu.c's own global,
 * compiled since  68, but nothing in the port ever assigns it --
 * no fighter's own Special-file streaming exists yet, for any of them --
 * so no sEFManagerModels row is added here either. efManagerAddModel
 * refuses on both host and target before it is ever read; see
 * func_ovl2_8010183C below. */
int llPikachuSpecial2UnkDObjDesc;
int llPikachuSpecial2UnkMObjSub;
int llPikachuSpecial2UnkAnimJoint;
int llPikachuSpecial2UnkMatAnimJoint;

// 0x8012E1D4
EFDesc dEFManagerPikachuUnkEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA | 0x1,       // Flags
    15,                                     // DL Link
    &gFTDataPikachuSpecial2,                // Texture file

    // DObj transformation struct 1
    {
        0x28,                               // Main matrix transformations   
        nGCMatrixKindRotRpyR,            // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    func_ovl2_801017E8,                     // Proc Update
    gcDrawDObjTreeForGObj,                // Proc Render

    &llPikachuSpecial2UnkDObjDesc,                // DObj Setup attributes offset (?)
    &llPikachuSpecial2UnkMObjSub,                  // MObjSub offset
    &llPikachuSpecial2UnkAnimJoint,                // AnimJoint offset
    &llPikachuSpecial2UnkMatAnimJoint              // MatAnimJoint offset
};

/* Pikachu's two effect descriptors and their reloc offsets, the same
 * stand-in shape as Sing's above.
 *
 * The Thunder Jolt's four are the spark the ground jolt
 * throws off; its two OTHER offsets -- the B animation pair
 * wpPikachuThunderJoltGroundAddAnim reads every time the crawler turns a
 * corner -- are in src/dc/wpmanager.c beside the weapon's attributes,
 * because it is the weapon file and not this one that reads them.
 *
 * The Thunder trail's two come out of gFTDataPikachuModel,
 * the fighter's OWN model file rather than a Special file: the sparks the
 * thunderbolt leaves behind it are drawn from Pikachu's own textures.
 * Its descriptor names only two -- its AnimJoint and MatAnimJoint offsets
 * are literal 0x0 in the decomp, the way the shield pair's are -- so only
 * two are declared here. */
int llPikachuSpecial3ThunderJoltDObjDesc;
int llPikachuSpecial3ThunderJoltMObjSub;
int llPikachuSpecial3ThunderJoltAnimJoint;
int llPikachuSpecial3ThunderJoltMatAnimJoint;
int llPikachuModelThunderTrailDObjDesc;
int llPikachuModelThunderTrailMObjSub;

/* ---- efmanager.c:639-667 dEFManagerPikachuThunderTrailEffectDesc:
 * the spark the thunderbolt leaves in its wake, one per
 * tic of the bolt's fall. Verbatim. Its pack is efthundertrail.mdl. ---- */
// 0x8012E224
EFDesc dEFManagerPikachuThunderTrailEffectDesc =
{
    EFFECT_FLAG_USERDATA,                   // Flags
    15,                                     // DL Link
    &gFTDataPikachuModel,                   // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindNull,               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerPikachuThunderTrailProcUpdate,     // Proc Update
    efManagerPikachuThunderTrailProcDisplay,     // Proc Render

    &llPikachuModelThunderTrailDObjDesc,              // DObj Setup attributes offset (?)
    &llPikachuModelThunderTrailMObjSub,                // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* ---- efmanager.c:669-697 dEFManagerThunderJoltEffectDesc:
 * the spark the ground jolt throws off at frame 6 of its push animation,
 * a model out of Pikachu's Special3 file. Verbatim, all four reloc
 * symbols named for real as Sing's are. Its pack is efjoltspark.mdl. ---- */
// 0x8012E24C
EFDesc dEFManagerThunderJoltEffectDesc =
{
    0x4,                                    // Flags
    15,                                     // DL Link
    &gFTDataPikachuSpecial3,                // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,     // Proc Update
    gcDrawDObjTreeForGObj,                // Proc Render

    &llPikachuSpecial3ThunderJoltDObjDesc,               // DObj Setup attributes offset (?)
    &llPikachuSpecial3ThunderJoltMObjSub,                 // MObjSub offset
    &llPikachuSpecial3ThunderJoltAnimJoint,               // AnimJoint offset
    &llPikachuSpecial3ThunderJoltMatAnimJoint             // MatAnimJoint offset
};

/* Mario's warp pipe's two reloc offsets, the same stand-in
 * shape as the two above. Its MObjSub and MatAnimJoint offsets are
 * literal 0x0 in the decomp, so only two are declared. */
int llMarioSpecial2EntryDokanDObjDesc;
int llMarioSpecial2EntryDokanAnimJoint;

/* ---- efmanager.c:1524-1552 dEFManagerMarioEntryDokanEffectDesc
 * ): the pipe Mario and Luigi rise out of at the countdown, the
 * first of the ten entry vehicles. A DObjDesc tree and one AnimJoint,
 * which is the rebirth halo's shape. Its file_head is written by the
 * maker below, by fighter kind. ---- */
// 0x8012E6CC
EFDesc dEFManagerMarioEntryDokanEffectDesc =
{
    0x4,                                    // Flags
    10,                                     // DL Link
    &gFTMarioFileSpecial2,                  // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,     // Proc Update
    gcDrawDObjTreeForGObj,                // Proc Render

    &llMarioSpecial2EntryDokanDObjDesc,           // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    &llMarioSpecial2EntryDokanAnimJoint,           // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* Donkey Kong's barrel's and Samus's point's reloc offsets,
 * the same stand-in shape as the pipe's. Both descriptors name only a
 * DObjDesc and an AnimJoint; their MObjSub and MatAnimJoint offsets are
 * literal 0x0 in the decomp. */
int llDonkeySpecial2EntryTaruDObjDesc;
int llDonkeySpecial2EntryTaruAnimJoint;
int llSamusSpecial2EntryPointDObjDesc;
int llSamusSpecial2EntryPointAnimJoint;

/* ---- efmanager.c:1434-1462 dEFManagerDonkeyEntryTaruEffectDesc and
 * :1464-1492 dEFManagerSamusEntryPointEffectDesc: the
 * barrel Donkey Kong rides in and the point Samus beams down onto, the
 * second and third entry vehicles. Both are the pipe's shape. ---- */
// 0x8012E654
EFDesc dEFManagerDonkeyEntryTaruEffectDesc =
{
    0x4,                                    // Flags
    10,                                     // DL Link
    &gFTDataDonkeySpecial2,                 // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,     // Proc Update
    gcDrawDObjTreeForGObj,                // Proc Render

    &llDonkeySpecial2EntryTaruDObjDesc,           // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    &llDonkeySpecial2EntryTaruAnimJoint,           // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

// 0x8012E67C
EFDesc dEFManagerSamusEntryPointEffectDesc =
{
    0x4,                                    // Flags
    10,                                     // DL Link
    &gFTDataSamusSpecial2,                  // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,     // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llSamusSpecial2EntryPointDObjDesc,           // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    &llSamusSpecial2EntryPointAnimJoint,           // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* Kirby's entry star's three reloc offsets. Two of them
 * are AnimJoints -- the star sweeps in from the right or from the left --
 * and its maker picks between them; see efManagerKirbyEntryStarMakeEffect
 * below. */
int llKirbySpecial2EntryStarDObjDesc;
int llKirbySpecial2EntryStarRAnimJoint;
int llKirbySpecial2EntryStarLAnimJoint;

/* ---- efmanager.c:1222-1250 dEFManagerKirbyEntryStarEffectDesc
 * ): the star Kirby rides in on, the fourth entry vehicle. ---- */
EFDesc dEFManagerKirbyEntryStarEffectDesc =
{
    0x4 | 0x1,                              // Flags
    10,                                     // DL Link
    &gFTDataKirbySpecial2,                  // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTra,                // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,     // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llKirbySpecial2EntryStarDObjDesc,            // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    &llKirbySpecial2EntryStarLAnimJoint,           // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};


/* Link's entry wave's and beam's eight reloc offsets. Both
 * are the first vehicles here with an MObjSub and a MatAnimJoint -- the
 * MObj carries no sprite array, so what the material animation moves is a
 * colour or a texture offset rather than a picture. */
int llLinkSpecial2EntryWaveDObjDesc;
int llLinkSpecial2EntryWaveMObjSub;
int llLinkSpecial2EntryWaveAnimJoint;
int llLinkSpecial2EntryWaveMatAnimJoint;
int llLinkSpecial2EntryBeamDObjDesc;
int llLinkSpecial2EntryBeamMObjSub;
int llLinkSpecial2EntryBeamAnimJoint;
int llLinkSpecial2EntryBeamMatAnimJoint;

/* ---- efmanager.c:1161-1189 dEFManagerLinkEntryWaveEffectDesc and
 * :1191-1219 dEFManagerLinkEntryBeamEffectDesc: the wave
 * Link rides in on and the beam that comes down with it -- the one
 * fighter whose entrance makes TWO effects. ---- */
// 0x8012E4E4
EFDesc dEFManagerLinkEntryWaveEffectDesc =
{
    0x4,                                    // Flags
    10,                                     // DL Link
    &gFTDataLinkSpecial2,                   // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindNull,               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    gcPlayAnimAll,                          // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llLinkSpecial2EntryWaveDObjDesc,             // DObj Setup attributes offset (?)
    &llLinkSpecial2EntryWaveMObjSub,               // MObjSub offset
    &llLinkSpecial2EntryWaveAnimJoint,             // AnimJoint offset
    &llLinkSpecial2EntryWaveMatAnimJoint           // MatAnimJoint offset
};

// 0x8012E50C
EFDesc dEFManagerLinkEntryBeamEffectDesc =
{
    0x4,                                    // Flags
    10,                                     // DL Link
    &gFTDataLinkSpecial2,                   // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindNull,               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    gcPlayAnimAll,                          // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llLinkSpecial2EntryBeamDObjDesc,             // DObj Setup attributes offset (?)
    &llLinkSpecial2EntryBeamMObjSub,               // MObjSub offset
    &llLinkSpecial2EntryBeamAnimJoint,             // AnimJoint offset
    &llLinkSpecial2EntryBeamMatAnimJoint           // MatAnimJoint offset
};

/* Yoshi's Special2 file, relocData 354. The four offsets are the port's
 * stand-ins again -- the pack carries the geometry. ---- */
int llYoshiSpecial2EntryEggDObjDesc;
int llYoshiSpecial2EntryEggMObjSub;
int llYoshiSpecial2EntryEggAnimJoint;
int llYoshiSpecial2EntryEggMatAnimJoint;

/* ---- efmanager.c:1312-1339 dEFManagerYoshiEntryEggEffectDesc
 * 84): the egg Yoshi hatches out of, the seventh entry vehicle.
 *
 * Its flags are 0x1 and nothing else, and it is the only vehicle without
 * 0x4. efmanager.c:1992-2004 reads that bit to decide what o_dobjsetup
 * points AT: with it, a DObjDesc tree walked by lbCommonSetupTreeDObjs;
 * without it, a plain display list handed to gcAddChildForDObj. So the
 * generated header's name llYoshiSpecial2EntryEggDObjDesc says the
 * opposite of what the field is -- the same trap as
 * llLinkSpecial3BoomerangDLDisplayList at , in reverse. The
 * packer builds the one-node tree the game builds by hand.
 *
 * Its ProcDisplay is lbCommonDObjScaleXProcDisplay, not
 * gcDrawDObjTreeForGObj: the egg is drawn mirrored when its scale.x is
 * negative. That proc is in src/dc/lbcommon.c:1684. ---- */
// 0x8012E5AC
EFDesc dEFManagerYoshiEntryEggEffectDesc =
{
    0x1,                                    // Flags
    10,                                     // DL Link
    &gFTDataYoshiSpecial2,                  // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,     // Proc Update
    lbCommonDObjScaleXProcDisplay,                     // Proc Render

    &llYoshiSpecial2EntryEggDObjDesc,             // DObj Setup attributes offset (?)
    &llYoshiSpecial2EntryEggMObjSub,               // MObjSub offset
    &llYoshiSpecial2EntryEggAnimJoint,             // AnimJoint offset
    &llYoshiSpecial2EntryEggMatAnimJoint           // MatAnimJoint offset
};

/* Fox's Special3, relocData 161 -- the Arwing's tree. Its two AnimJoint
 * arrays are NOT in this file: they live in his Special2 (relocData 346)
 * and the maker reads them from there, which makes the Arwing the first
 * vehicle whose animation and geometry come out of two files. The pack
 * carries both as its two animations, so the port needs neither symbol.
 * The port's stand-in is the tree's, and zero as always. ---- */
int llFoxSpecial3EntryArwingDObjDesc;

/* ---- efmanager.c:1555-1582 dEFManagerFoxEntryArwingEffectDesc
 * ): the eighth entry vehicle, and the first whose maker does
 * more than place it.
 *
 * Note what is zero here: o_anim_joint AND o_matanim_joint. The Arwing
 * is not animated by efManagerMakeEffect at all -- the maker animates
 * it, picking one of two AnimJoint arrays by which way Fox flies in.
 * That is Kirby's star's problem again, so it has the star's answer:
 * both arrays are baked into the one pack and the port picks by index.
 * ---- */
// 0x8012E6F4
EFDesc dEFManagerFoxEntryArwingEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA | 0x1,       // Flags
    10,                                     // DL Link
    &gFTDataFoxSpecial3,                    // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyR,         // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerFoxEntryArwingProcUpdate,             // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llFoxSpecial3EntryArwingDObjDesc,            // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* Captain Falcon's Special2, relocData 350 -- the Blue Falcon's tree.
 * The three AnimJoints the maker reads out of the same file (the array
 * at 0x6200 and the two wheel scripts at 0x6518 and 0x6598) are the
 * pack's one animation, so the port needs none of their symbols. ---- */
int llCaptainSpecial2EntryCarDObjDesc;

/* ---- efmanager.c:1495-1522 dEFManagerCaptainEntryCarEffectDesc
 * ): the ninth and last entry vehicle with an arm of its own.
 *
 * Its flags are 0x4 | EFFECT_FLAG_USERDATA and, ALONE AMONG THE
 * VEHICLES, LACK 0x1. That bit is what puts an empty DObj above the
 * tree: with it, efmanager.c:1986-2004 makes one and hangs the tree
 * under it; without it, efmanager.c:2017-2043 calls gcSetupCustomDObjs
 * and the tree goes straight on the GObj. The second is exactly the
 * shape this port builds for every effect, so the Blue Falcon's maker
 * ports with its tree walk unchanged -- where the Arwing's, one step
 * ago, needed a level taken off. It also means the transform kinds land
 * where the game puts them without the port doing anything: types1 on
 * the tree's own root, types2 on everything below it. ---- */
// 0x8012E6A4
EFDesc dEFManagerCaptainEntryCarEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    10,                                     // DL Link
    &gFTDataCaptainSpecial2,                // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerCaptainEntryCarProcUpdate,            // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llCaptainSpecial2EntryCarDObjDesc,           // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* Yoshi's Special3, relocData 339 -- the Egg Lay egg's tree and its
 * three AnimJoints. The port's stand-ins are two: the descriptor's own
 * offset, and the table efManagerYoshiEggLaySetAnim indexes.
 *
 * DIVERGES: the game's dEFManagerYoshiEggLayAnimJoints holds two BLOCK
 * POINTERS into that file and hands each to lbRelocGetFileData; the pack
 * carries all three animations side by side, so the port's holds the two
 * PACK INDEXES instead. Baked throw first, because index 0 is the one
 * efManagerAddModel applies from the descriptor: 0 throw, 1 wait, 2
 * break. Kirby's entry star's answer at  82, one table wider. */
int llYoshiSpecial3EggLayDObjDesc;
int llYoshiSpecial3EggLayThrowAnimJoint;

// 0x8012E5D4
s32 dEFManagerYoshiEggLayAnimJoints[/* */] = { 1, 2 };

/* ---- efmanager.c:5343-5370 dEFManagerYoshiEggLayEffectDesc
 * 88): the egg a fighter caught by Yoshi's Egg Lay sits inside, and the
 * last of the two makers  59 stubbed.
 *
 * Its flags carry 0x1, so on the N64 the DObjDesc tree hangs under an
 * empty DObj and here it does not ( 86 named the rule). Every
 * dobj->child in the two functions below is therefore the decomp's
 * dobj->child->child, marked where it happens.
 *
 * transform_types1 is {0x50, Sca} -- kind 0x50 is the one that follows
 * another DObj's matrix out of user_data.p, which the maker sets to the
 * caught fighter's TopN joint, and the Sca beside it is what the maker
 * writes the egg's size into. ---- */
// 0x8012E5DC
EFDesc dEFManagerYoshiEggLayEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA | 0x1,       // Flags
    10,                                     // DL Link
    &gFTDataYoshiSpecial3,                  // Texture file

    // DObj transformation struct 1
    {
        0x50,                               // Main matrix transformations
        nGCMatrixKindSca,                // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerYoshiEggLayProcUpdate,                     // Proc Update
    gcDrawDObjTreeForGObj,                // Proc Render

    &llYoshiSpecial3EggLayDObjDesc,               // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    &llYoshiSpecial3EggLayThrowAnimJoint,          // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* efmanager.c:1375-1402 dEFManagerYoshiEggEscapeEffectDesc,
 * one desc after the egg lay's above (the fighter hatching out of its
 * own egg, once the throw or the escape-from-being-eaten finishes). Its
 * o_dobjsetup and proc_display are &llYoshiModelShieldDObjDesc and
 * efManagerYoshiShieldProcDisplay -- the Yoshi shield's own, already
 * packed as efyegg.mdl since  5 -- so this is a second EFDesc
 * over a model this port already has, the same shape as the star rod's
 * spark over efspark.mdl. No new export. */
// 0x8012E604
EFDesc dEFManagerYoshiEggEscapeEffectDesc =
{
    EFFECT_FLAG_USERDATA,                   // Flags
    15,                                     // DL Link
    &gFTDataYoshiModel,                     // Texture file

    // DObj transformation struct 1
    {
        0x50,                               // Main matrix transformations
        0x4A,                               // Secondary matrix transformations
        0x2E                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindNull,               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    NULL,                                   // Proc Update
    efManagerYoshiShieldProcDisplay,        // Proc Render

    &llYoshiModelShieldDObjDesc,            // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* Kirby's Special2, relocData 348 -- the file his entry star came out of
 * at  82, and now the Final Cutter's four. The port's stand-ins
 * are seven: four trees and three AnimJoints, because the blade the
 * descriptor calls "Draw" has none. ---- */
int llKirbySpecial2CutterUpDObjDesc;
int llKirbySpecial2CutterUpAnimJoint;
int llKirbySpecial2CutterDownDObjDesc;
int llKirbySpecial2CutterDownAnimJoint;
int llKirbySpecial2CutterDrawDObjDesc;
int llKirbySpecial2CutterTrailDObjDesc;
int llKirbySpecial2CutterTrailAnimJoint;
int llKirbySpecial2VulcanJabDObjDesc;

/* ---- efmanager.c:699-727 dEFManagerVulcanJabEffectDesc:
 * the spark Kirby's rapid jab throws, out of the same Special2 his
 * Final Cutter and entry star came from. A tree with DL links, no
 * MObjSub and NO AnimJoint -- the second descriptor here with none,
 * after the Cutter's blade. Its flags carry 0x1, so the game hangs the
 * tree under an empty DObj and this port hangs it on the GObj
 * 86's rule); the maker below only writes the root's translate and
 * rotate, which is the case that difference does not reach. ---- */
// 0x8012E274
EFDesc dEFManagerVulcanJabEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA | 0x1,       // Flags
    15,                                     // DL Link
    &gFTDataKirbySpecial2,                  // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindRotRpyR,            // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerKirbyVulcanJabProcUpdate,        // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llKirbySpecial2VulcanJabDObjDesc,                 // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* Link's Special2, relocData 353 -- the file his entry wave and beam
 * came out of at  83, and now the Spin Attack's glow. Four
 * stand-ins: a tree, an MObjSub, an AnimJoint and a MatAnimJoint. ---- */
int llLinkSpecial2SpinAttackDObjDesc;
int llLinkSpecial2SpinAttackMObjSub;
int llLinkSpecial2SpinAttackAnimJoint;
int llLinkSpecial2SpinAttackMatAnimJoint;


/* ---- efmanager.c:892-1009, the Final Cutter's four EFDescs
 * 89): the blade Kirby draws, the two arcs of the swing, and the trail
 * behind it.
 *
 * All four carry 0x4 | EFFECT_FLAG_USERDATA and NO 0x1, so the game
 * builds their trees straight on the GObj (efmanager.c:2017-2043) and
 * the port's makers below need no level taken off --  86's rule,
 * used rather than found for the second step running.
 *
 * The four differ in exactly the two ways that matter to a bake: the
 * trail is the only one drawn through gcDrawDObjTreeDLLinksForGObj, and
 * the blade is the only one whose o_anim_joint is zero. Their transform
 * kinds say what they hang off: 0x50 on three of them, the kind that
 * follows another DObj's matrix out of user_data.p, which the makers set
 * to a joint of Kirby's; and 0x4F on the blade. ---- */
EFDesc dEFManagerKirbyCutterUpEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    15,                                     // DL Link
    &gFTDataKirbySpecial2,                  // Texture file

    // DObj transformation struct 1
    {
        0x50,                               // Main matrix transformations
        nGCMatrixKindRotRpyR,            // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerNoEjectProcUpdate,                     // Proc Update
    gcDrawDObjTreeForGObj,                // Proc Render

    &llKirbySpecial2CutterUpDObjDesc,             // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    &llKirbySpecial2CutterUpAnimJoint,             // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

// 0x8012E3A4
EFDesc dEFManagerKirbyCutterDownEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    15,                                     // DL Link
    &gFTDataKirbySpecial2,                  // Texture file

    // DObj transformation struct 1
    {
        0x50,                               // Main matrix transformations
        nGCMatrixKindRotRpyR,            // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerNoEjectProcUpdate,                     // Proc Update
    gcDrawDObjTreeForGObj,                // Proc Render

    &llKirbySpecial2CutterDownDObjDesc,           // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    &llKirbySpecial2CutterDownAnimJoint,           // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

// 0x8012E3CC
EFDesc dEFManagerKirbyCutterDrawEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    15,                                     // DL Link
    &gFTDataKirbySpecial2,                  // Texture file

    // DObj transformation struct 1
    {
        0x4F,                               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerNoEjectProcUpdate,                     // Proc Update
    gcDrawDObjTreeForGObj,                // Proc Render

    &llKirbySpecial2CutterDrawDObjDesc,           // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

// 0x8012E3F4
EFDesc dEFManagerKirbyCutterTrailEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    15,                                     // DL Link
    &gFTDataKirbySpecial2,                  // Texture file

    // DObj transformation struct 1
    {
        0x50,                               // Main matrix transformations
        nGCMatrixKindRotRpyR,            // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerNoEjectProcUpdate,                     // Proc Update
    gcDrawDObjTreeDLLinksForGObj,                // Proc Render

    &llKirbySpecial2CutterTrailDObjDesc,          // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    &llKirbySpecial2CutterTrailAnimJoint,          // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* ---- efmanager.c:1405-1432 dEFManagerLinkSpinAttackEffectDesc
 * ): the glow around Link while his Up-B spins. Flags without
 * 0x1, so no stand and the maker's DObj is the tree's root. ---- */
EFDesc dEFManagerLinkSpinAttackEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    15,                                     // DL Link
    &gFTDataLinkSpecial2,                   // Texture file

    // DObj transformation struct 1
    {
        0x50,                               // Main matrix transformations
        nGCMatrixKindRotRpyR,            // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,     // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llLinkSpecial2SpinAttackDObjDesc,                // DObj Setup attributes offset (?)
    &llLinkSpecial2SpinAttackMObjSub,                  // MObjSub offset
    &llLinkSpecial2SpinAttackAnimJoint,                // AnimJoint offset
    &llLinkSpecial2SpinAttackMatAnimJoint              // MatAnimJoint offset
};

/* efmanager.c:111-138 dEFManagerShockSmallEffectDesc, the
 * small shock burst -- the first of the two DIVERGES arms that turned
 * out to be real (s 100 and 101 closed the other two with no
 * new asset). Its three reloc symbols, the same stand-in shape as
 * every other block above. */
int llEFCommonEffects2ShockSmallDObjDesc;
int llEFCommonEffects2ShockSmallMObjSub;
int llEFCommonEffects2ShockSmallMatAnimJoint;

// 0x8012DF4C
EFDesc dEFManagerShockSmallEffectDesc =
{
    EFFECT_FLAG_USERDATA,                   // Flags
    18,                                     // DL Link
    &gEFManagerFiles[1],                      // Texture file

    // DObj transformation struct 1
    {
        0x28,                               // Main matrix transformations   
        0x45,                               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindNull,               // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerVelAddDestroyAnimEnd,           // Proc Update
    lbCommonDObjScaleXProcDisplay,           // Proc Render

    &llEFCommonEffects2ShockSmallDObjDesc,     // DObj Setup attributes offset (?)
    &llEFCommonEffects2ShockSmallMObjSub,      // MObjSub offset
    0x0,                                       // AnimJoint offset
    &llEFCommonEffects2ShockSmallMatAnimJoint  // MatAnimJoint offset
};

/* efmanager.c:381-408 dEFManagerFireSparkEffectDesc, the
 * second and last of the two real DIVERGES arms. Its four reloc
 * symbols, the same stand-in shape as every other block above. */
int llEFCommonEffects2FireSparkDObjDesc;
int llEFCommonEffects2FireSparkMObjSub;
int llEFCommonEffects2FireSparkAnimJoint;
int llEFCommonEffects2FireSparkMatAnimJoint;

// 0x8012E0DC
EFDesc dEFManagerFireSparkEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    15,                                     // DL Link
    &gEFManagerFiles[1],                      // Texture file

    // DObj transformation struct 1
    {
        0x50,                               // Main matrix transformations   
        0x49,                               // Secondary matrix transformations
        0x12                                // ???
    },

    // DObj transformation struct 2
    {
        0x45,                               // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,     // Proc Update
    lbCommonDObjScaleXProcDisplay,     // Proc Render

    &llEFCommonEffects2FireSparkDObjDesc,    // DObj Setup attributes offset (?)
    &llEFCommonEffects2FireSparkMObjSub,     // MObjSub offset
    &llEFCommonEffects2FireSparkAnimJoint,   // AnimJoint offset
    &llEFCommonEffects2FireSparkMatAnimJoint // MatAnimJoint offset
};

/* The dead explosion's four blocks, and the four MatAnimJoints over them.
 * The names are the generated header's; the values are
 * the port's, which is to say none.
 *
 * Two of these six names do not mean what they say, and the decomp says
 * so: dEFManagerDeadExplodeEffectDesc lists o_dobjsetup first and
 * o_mobjsub second (ef/eftypes.h:20-21), and what it lists first is
 * ...DeadExplodeDefaultMObjSub. So the DObjDesc array is at the symbol
 * called MObjSub and the MObjSub table at the one called DObjDesc --
 * ssb-decomp-re/src/relocData/84_EFCommonEffects2.c:1371 and :1549 both
 * mark them "MIS-TYPED". The names come out of the decomp's own
 * generator (tools/export/gen_reloc_header.sh) and are not the port's to
 * correct, so the descriptor below keeps the decomp's spelling and
 * tools/export/ssb_effectexport.py reads the blocks the way round they really
 * are. */
int llEFCommonEffects2DeadExplodeDefaultDObjDesc;
int llEFCommonEffects2DeadExplodeDefaultMObjSub;
int llEFCommonEffects2DeadExplodeDefaultAnimJoint;
int llEFCommonEffects2DeadExplodeDefaultMatAnimJoint;
int llEFCommonEffects2DeadExplode1MatAnimJoint;
int llEFCommonEffects2DeadExplode2MatAnimJoint;
int llEFCommonEffects2DeadExplode3MatAnimJoint;
int llEFCommonEffects2DeadExplode4MatAnimJoint;

/* ---- efmanager.c:850-889: the dead explosion's descriptor and its two
 * tables.
 *
 * Flags EFFECT_FLAG_SPECIALLINK | 0x4 and *not* 0x1: the DObjDesc array
 * is read by gcSetupCustomDObjs, which is the halo's branch, and
 * SPECIALLINK puts the GObj on nGCCommonLinkIDSpecialEffect rather than
 * the ordinary effect link. No USERDATA, so no EFStruct is taken and the
 * proc_update named here is never installed -- the decomp names it all
 * the same and so does this.
 *
 * The tree is a stand and three shards; each shard has one MObj and one
 * display list into DL head 1.
 *
 * dEFManagerDeadExplodeMatAnimJoints is the part with no precedent: the
 * maker writes its player'th entry into o_matanim_joint *before* making
 * the effect, so the descriptor is mutated and four different material
 * animations come out of one EFDesc. See the DIVERGES at the maker. ---- */

// 0x8012E33C
EFDesc dEFManagerDeadExplodeEffectDesc =
{
    EFFECT_FLAG_SPECIALLINK | 0x4,          // Flags
    18,                                     // DL Link
    &gEFManagerFiles[1],                      // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,          // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llEFCommonEffects2DeadExplodeDefaultMObjSub,        // DObj Setup attributes offset (?)
    &llEFCommonEffects2DeadExplodeDefaultDObjDesc,          // MObjSub offset
    &llEFCommonEffects2DeadExplodeDefaultAnimJoint,        // AnimJoint offset
    &llEFCommonEffects2DeadExplodeDefaultMatAnimJoint      // MatAnimJoint offset
};

/* Two rows of four read as one array of eight, which is the decomp's own
 * warning: `(type % 2) * GMCOMMON_PLAYERS_MAX + player` indexes it, so
 * the first four are the up-blast generator and the second four the
 * sideways one. */
// 0x8012E364
u8 dEFManagerDeadExplodeGenID[/* */] = { 0x2D, 0x2C, 0x2B, 0x2A, 0x3F, 0x3E, 0x3D, 0x3C };

// 0x8012E36C
intptr_t dEFManagerDeadExplodeMatAnimJoints[/* */] =
{
    &llEFCommonEffects2DeadExplode1MatAnimJoint,
    &llEFCommonEffects2DeadExplode2MatAnimJoint,
    &llEFCommonEffects2DeadExplode3MatAnimJoint,
    &llEFCommonEffects2DeadExplode4MatAnimJoint
};


/* reloc_data.us.h's declarations for the halo's two blocks, defined here
 * so the EFDesc below links. Zero, like src/dc/mpshim.c's map file ids
 * and for the same reason: the game gives a reloc symbol its value in the
 * link -- the *address* of llEFCommonEffects3RebirthHaloDObjDesc is
 * 0x2AC0, the block's offset in relocData file 85 -- and the port does
 * not. tools/export/ssb_effectexport.py reads that offset out
 * of the generated header and bakes the model at it, so nothing at
 * runtime needs the value; what does need saying is that these two are
 * not it, which is why efModelPackFor below keys on the descriptor and
 * not on the numbers inside it. */
int llEFCommonEffects3RebirthHaloDObjDesc;
int llEFCommonEffects3RebirthHaloAnimJoint;

/* the Poké Ball's opening rays, out of the same file (relocData
 * 85) and with a MObjSub and a MatAnimJoint the halo does not have -- the
 * first EFDesc here whose MObjSub offset is not 0x0. Same stand-in, same
 * reason, and the exporter reads the real offsets out of the header. */
int llEFCommonEffects3MBallRaysDObjDesc;
int llEFCommonEffects3MBallRaysMObjSub;
int llEFCommonEffects3MBallRaysAnimJoint;
int llEFCommonEffects3MBallRaysMatAnimJoint;

/* The item-use step: the pickup swirl, the rays' shape again out of the
 * same relocData file 85 -- six joints, four MObjs, one animation. Same
 * stand-ins for the same reason. */
int llEFCommonEffects3ItemGetSwirlDObjDesc;
int llEFCommonEffects3ItemGetSwirlMObjSub;
int llEFCommonEffects3ItemGetSwirlAnimJoint;
int llEFCommonEffects3ItemGetSwirlMatAnimJoint;

/* ---- efmanager.c:1646-1675: the one EFDesc the port reads.
 *
 * Fifty-three of these describe an effect's model, and this is the
 * first the port has anywhere to put. It is kept whole and to the
 * character -- flags, DL link, both transform-type triples, both
 * procs and all four block offsets -- because everything in it but
 * the block offsets is read at runtime: the flags choose the link and
 * whether a struct is taken, the two procs are installed as they are
 * (gcDrawDObjTreeDLLinksForGObj is a real function in this port), and
 * the transform-type triples are written onto the tree's XObjs. The
 * offsets are what tools/export/ssb_effectexport.py read the model out of,
 * and what row 13 would one day make true here too. ---- */

// 0x8012E770
EFDesc dEFManagerRebirthHaloEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    10,                                     // DL Link
    &gEFManagerFiles[2],                      // Texture file

    // DObj transformation struct 1
    {
        0x50,                               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        0x00,                               // Secondary matrix transformations
        0x00                                // ???
    },

    gcPlayAnimAll,                          // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llEFCommonEffects3RebirthHaloDObjDesc,               // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    &llEFCommonEffects3RebirthHaloAnimJoint,               // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* efmanager.c:1252-1273 dEFManagerMBallRaysEffectDesc, verbatim
 * 56). The halo's shape with two differences worth naming: its Flags are
 * 0x4 ALONE -- no EFFECT_FLAG_USERDATA -- while its proc_update is still
 * efManagerHaveStructProcUpdate, which is the decomp's own combination and
 * is kept; and its MObjSub offset is real, so the pack carries MObjs and
 * dc_model_add_mobjs rebuilds them. gcDrawDObjTreeDLLinksForGObj is the
 * renderer, so its geometry hangs off DL links like the halo's. */
// 0x8012E55C
EFDesc dEFManagerMBallRaysEffectDesc =
{
    0x4,                                    // Flags
    10,                                     // DL Link
    &gEFManagerFiles[2],                      // Texture file

    // DObj transformation struct 1
    {
        0x44,                               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,     // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llEFCommonEffects3MBallRaysDObjDesc,                 // DObj Setup attributes offset (?)
    &llEFCommonEffects3MBallRaysMObjSub,                   // MObjSub offset
    &llEFCommonEffects3MBallRaysAnimJoint,                 // AnimJoint offset
    &llEFCommonEffects3MBallRaysMatAnimJoint               // MatAnimJoint offset
};

/* efmanager.c:1677-1703 dEFManagerItemGetSwirlEffectDesc, verbatim (the
 * item-use step): the swirl that spins up around a fighter the moment an
 * item lands in his hands, made by efManagerItemGetSwirlProcUpdate from
 * it/itmain.c's itMainSetFighterHold at the world position of the hand
 * joint.
 *
 * The rays' descriptor again -- same bank, same renderer, MObjs and a
 * MatAnimJoint -- differing in its flags (0x4 | 0x1, so a struct IS
 * taken and the tree hangs under an empty DObj) and in its first
 * transform kind, 0x28: the billboard, which src/dc/objdisplay.c's
 * gcDObjLocalMatrix already handles, so the swirl always faces the
 * camera however the fighter is turned. */
// 0x8012E798
EFDesc dEFManagerItemGetSwirlEffectDesc =
{
    0x4 | 0x1,                              // Flags
    18,                                     // DL Link
    &gEFManagerFiles[2],                      // Texture file

    // DObj transformation struct 1
    {
        0x28,                               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        0x00,                               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,     // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llEFCommonEffects3ItemGetSwirlDObjDesc,              // DObj Setup attributes offset (?)
    &llEFCommonEffects3ItemGetSwirlMObjSub,                // MObjSub offset
    &llEFCommonEffects3ItemGetSwirlAnimJoint,              // AnimJoint offset
    &llEFCommonEffects3ItemGetSwirlMatAnimJoint            // MatAnimJoint offset
};


// 0x801313B0
void *gEFManagerFiles[3];

// 0x801313BC
EFStruct *sEFManagerStructsAllocFree;

// 0x801313C0
s32 sEFManagerStructsFreeNum;

// 0x801313C4
s32 gEFManagerParticleBankID;

static void efManagerFixupModelProcDisplays(void);

/* ---- efmanager.c:1731-1759 efManagerInitEffects ----
 *
 * DIVERGES. The three gEFManagerFiles loads are gone; see the file
 * header. The comment above the function is the decomp's own.
 */
/* 0x800FD300 - OLD NOTE: To match this, lbRelocGetExternHeapFile and lbRelocGetFileSize must take intptr_t or other int type as first argument
 *              NEW NOTE: Not entirely correct, their types do need to be identical however
 */
void efManagerInitEffects(void)
{
    EFStruct *ep;
    s32 i;
    s32 unused;

    sEFManagerStructsAllocFree = ep = syTaskmanMalloc(sizeof(EFStruct) * EFFECT_ALLOC_NUM, 0x8);
    sEFManagerStructsFreeNum = EFFECT_ALLOC_NUM;

    for (i = 0; i < (EFFECT_ALLOC_NUM - 1); i++)
    {
        ep[i].next = &ep[i + 1];
    }
    if (ep != NULL)
    {
        ep[i].next = NULL;
    }
    efDisplayMakeCLD();
    efDisplayMakeXLU();

    /* DIVERGES: efmanager.c:1754-1756, the three effect model files. */

    efDisplayInitAll();

    efManagerFixupModelProcDisplays();
}

/* DIVERGES, port-only, and not in the decomp: every one of these EFDescs'
 * Proc Render field is verbatim decomp text (tools/check/efmanager_check.py's
 * RUNS check every one of these tables byte for byte, so the fix cannot
 * land on the table itself without breaking that). On real hardware the
 * field genuinely names the generic tree walk -- gcDrawDObjTreeForGObj or
 * its DL-links twin -- because the RSP draws inline as it walks. On this
 * port that walk (src/dc/objdisplay.c) only records each joint's matrix
 * (dc_joint_submit); the actual PVR submission is a second pass,
 * dc_model_proc_display, and every one of these effects has a real baked
 * pack (tools/export/ssb_effectexport.py) that needs it or nothing of it ever
 * reaches the screen -- the same gap src/dc/itdisplay.c's header
 * describes for items, and src/dc/wpdisplay.c's for most weapons. Found
 * from a real bug report: Yoshi's Egg Lay never appeared, traced back to
 * every one of these sharing the identical table entry. One overwrite
 * per desc, once, before anything plays. */
static void efManagerFixupModelProcDisplays(void)
{
    dEFManagerDamageSlashEffectDesc.proc_display     = dc_model_proc_display;
    dEFManagerDamageFlyMDustEffectDesc.proc_display  = dc_model_proc_display;
    dEFManagerMarioEntryDokanEffectDesc.proc_display  = dc_model_proc_display;
    dEFManagerDonkeyEntryTaruEffectDesc.proc_display  = dc_model_proc_display;
    dEFManagerSamusEntryPointEffectDesc.proc_display  = dc_model_proc_display;
    dEFManagerKirbyEntryStarEffectDesc.proc_display   = dc_model_proc_display;
    dEFManagerLinkEntryWaveEffectDesc.proc_display    = dc_model_proc_display;
    dEFManagerLinkEntryBeamEffectDesc.proc_display    = dc_model_proc_display;
    dEFManagerFoxEntryArwingEffectDesc.proc_display   = dc_model_proc_display;
    dEFManagerCaptainEntryCarEffectDesc.proc_display  = dc_model_proc_display;
    dEFManagerYoshiEggLayEffectDesc.proc_display      = dc_model_proc_display;
    dEFManagerVulcanJabEffectDesc.proc_display        = dc_model_proc_display;
    dEFManagerKirbyCutterUpEffectDesc.proc_display    = dc_model_proc_display;
    dEFManagerKirbyCutterDownEffectDesc.proc_display  = dc_model_proc_display;
    dEFManagerKirbyCutterDrawEffectDesc.proc_display  = dc_model_proc_display;
    dEFManagerKirbyCutterTrailEffectDesc.proc_display = dc_model_proc_display;
    dEFManagerLinkSpinAttackEffectDesc.proc_display   = dc_model_proc_display;
    dEFManagerDeadExplodeEffectDesc.proc_display      = dc_model_proc_display;
    dEFManagerRebirthHaloEffectDesc.proc_display      = dc_model_proc_display;
    dEFManagerMBallRaysEffectDesc.proc_display        = dc_model_proc_display;
    dEFManagerItemGetSwirlEffectDesc.proc_display     = dc_model_proc_display;
}


/* ---- efmanager.c:1761-1926: the pool, the procs, the sorters, the
 * run hook. ---- */

// 0x800FD43C
EFStruct* efManagerGetNextStructAlloc(sb32 is_force_return)
{
    EFStruct *ep;

    if ((is_force_return == FALSE) && (sEFManagerStructsFreeNum < 5))
    {
        return NULL;
    }
    ep = sEFManagerStructsAllocFree;

    if (ep == NULL)
    {
        return NULL;
    }
    sEFManagerStructsAllocFree = ep->next;

    ep->fighter_gobj = NULL;
    ep->xf = NULL;
    ep->is_pause_effect = FALSE;

    sEFManagerStructsFreeNum--;

    return ep;
}

// 0x800FD4B8
EFStruct* efManagerGetEffectNoForce(void)
{
    return efManagerGetNextStructAlloc(FALSE);
}

// 0x800FD4D8
EFStruct* efManagerGetEffectForce(void)
{
    return efManagerGetNextStructAlloc(TRUE);
}

// 0x800FD4F8
void efManagerSetPrevStructAlloc(EFStruct *ep)
{
    ep->next = sEFManagerStructsAllocFree;

    sEFManagerStructsAllocFree = ep;

    sEFManagerStructsFreeNum++;
}

// 0x800FD524
void efManagerNoStructProcUpdate(GObj *effect_gobj)
{
    gcPlayAnimAll(effect_gobj);

    if (effect_gobj->anim_frame <= 0.0F)
    {
        gcEjectGObj(effect_gobj);
    }
}

// 0x800FD568
void efManagerHaveStructProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    if (!(ep->is_pause_effect))
    {
        gcPlayAnimAll(effect_gobj);

        if (effect_gobj->anim_frame <= 0.0F)
        {
            efManagerSetPrevStructAlloc(efGetStruct(effect_gobj));

            gcEjectGObj(effect_gobj);
        }
    }
}

// New file? Unused
void func_ovl2_800FD5D0(void)
{
    return;
}

// 0x800FD5D8
void efManagerNoEjectProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    if (!(ep->is_pause_effect))
    {
        gcPlayAnimAll(effect_gobj);
    }
}

// 0x800FD60C
void efManagerSortZNeg(DObj *dobj)
{
    GObj *parent_gobj;

    if (dobj->translate.vec.f.z < -1000.0F)
    {
        parent_gobj = dobj->parent_gobj;

        if (parent_gobj->dl_link_id != 2)
        {
            gcMoveGObjDL(parent_gobj, 2, 2);
        }
    }
    else
    {
        parent_gobj = dobj->parent_gobj;

        if (parent_gobj->dl_link_id != 20)
        {
            gcMoveGObjDL(parent_gobj, 20, 2);
        }
    }
}

// 0x800FD68C
void efManagerSortZPos(DObj *dobj)
{
    GObj *parent_gobj;

    if (dobj->translate.vec.f.z > 1000.0F)
    {
        parent_gobj = dobj->parent_gobj;

        if (parent_gobj->dl_link_id != 2)
        {
            gcMoveGObjDL(parent_gobj, 2, 2);
        }
    }
    else
    {
        parent_gobj = dobj->parent_gobj;

        if (parent_gobj->dl_link_id != 20)
        {
            gcMoveGObjDL(parent_gobj, 20, 2);
        }
    }
}

// Another unused func
void func_ovl2_800FD70C(void)
{
    return;
}

// 0x800FD714
void efManagerFuncRun(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    if (ep != NULL)
    {
        if (ep->proc_update != NULL)
        {
            gcAddGObjProcess(effect_gobj, ep->proc_update, nGCProcessKindFunc, 3);
        }
    }
    else gcAddGObjProcess(effect_gobj, efManagerNoStructProcUpdate, nGCProcessKindFunc, 3);

    effect_gobj->func_run = NULL;
}

/* ---- efmanager.c:1928-2058 efManagerMakeEffect ----
 *
 * DIVERGES, and this is the file's second and larger divergence.
 *
 * The game's builds a DObj tree by reading a DObjDesc array out of one
 * of the three llEFCommonEffects files at the offset the EFDesc names,
 * hangs an MObj chain and two animation joints off it, and leaves the
 * drawing to a display list. The port has none of that: a model is a
 * pack, built ahead of time by tools/export/ssb_effectexport.py out of the very
 * same DObjDesc, and drawn by src/dc/fighter.c. That is the same
 * substitution src/dc/mnplayersvs.c makes for the character select's
 * spotlight and src/dc/ifcommon.c for the off-screen arrows, and it is
 * made here once for every effect that will ever come through this
 * function.
 *
 * What is kept: the EFStruct take and its unwind, the GObj on the link
 * the flags choose, efManagerFuncRun, ep->proc_update, the early return
 * for a descriptor with no display proc, and the display proc and DL
 * link the EFDesc names -- gcDrawDObjTreeDLLinksForGObj is a real
 * function in this port (src/dc/objdisplay.c), so nothing is
 * substituted for it here.
 *
 * What is replaced: the four branches on flags 0x1 and 0x4. All four
 * end in a tree of DObjs with a transform-type triple on the root and
 * another on everything below it, and dc_model_add_dobjs builds exactly
 * that tree out of the pack -- one DObj per pack joint, parented as the
 * DObjDesc said, because the pack's joints *are* the DObjDesc's. So the
 * branches collapse into one call, and the two triples are applied
 * after it.
 *
 * What is dropped: MObjSub and MatAnimJoint. The halo has neither
 * (o_mobjsub and o_matanim_joint are 0 in its EFDesc); the effects that
 * do are the next step's, and dc_model_add_mobjs is what will carry
 * them.
 *
 * The one thing that is not yet general is the table: efModelPackFor
 * knows one model. An EFDesc it does not know is refused with a line on
 * the serial log rather than silently drawing nothing, which is what
 * says how far this has got.
 */

/* The port's own: which pack a descriptor's model was exported into.
 *
 * Keyed on the EFDesc itself. The tempting key is the pair the game uses
 * to find the model -- which of the three files, and the DObjDesc's
 * offset in it -- but the port has neither number: gEFManagerFiles is
 * never loaded (see efManagerInitEffects) and the reloc symbols are
 * defined at zero above. The descriptor is the same fact by another
 * name, and it is one the port really holds. */
typedef struct EFModel
{
    const EFDesc *desc;
    const char *pack;
    Fighter *loaded;
    sb32 is_loaded;
} EFModel;

/* `loaded` starts NULL and efModelLoad mallocs it, rather than each
 * model owning a `static Fighter`.
 *
 * A Fighter is 17,400 bytes on the SH-4 -- FIGHTER_MAX_JOINTS of pose,
 * matrix, light and animation state, sized for the biggest model the
 * port loads -- and a static one is .bss, reserved for the whole run
 * whether the pack ever loads or not. One of the five below never does:
 * efslash.mdl is built into disc/ and deliberately left out of the ELF's
 * romdisk rows 21 and 22), so efModelLoad refuses it every
 * time. efhalo.mdl and eforbs.mdl were always in the romdisk;
 * efspark.mdl and efmdust.mdl are there only because of this change.
 *
 * That was measured, and it is why this is not a tidy-up. Adding the
 * metal dust's static took the ELF's .bss up by one Fighter -- the heap
 * base moved from 0x8CFDD000 to 0x8CFE2000, one page more than the
 * 17,400 bytes -- and that was enough that the 16 KB thread the
 * announcer's "GO!" wants could not be had: a one-player Castle battle
 * that runs 3,302 frames at 59.8 fps on the commit before this one
 * aborted at frame 20 with `osCreateThread: thd_create_ex failed`.
 * Allocating on load instead moved the heap base the other way by
 * 86,016 bytes -- the five statics, twenty-one pages -- which is enough
 * that ifannounce.spr loads in a battle for the first time and that both
 * flipbook packs fit the ELF.
 *
 * It is ft/ftmanager.c's own pattern (src/dc/ftmanager.c:325), and the
 * malloc heap is not the scene heap: a pack loaded here stays loaded for
 * the run, as the statics did.
 *
 * The screen quake's pack below is the same change for the same reason;
 * it is not in this table only because it has no EFDesc. */
/* Ness's four EFDescs are copied at the foot of this file, after the
 * table that names them. */
extern EFDesc dEFManagerNessPsychicMagnetEffectDesc;
extern EFDesc dEFManagerNessPKThunderWaveEffectDesc;
extern EFDesc dEFManagerNessPKThunderTrailEffectDesc;
extern EFDesc dEFManagerNessPKReflectTrailEffectDesc;
/* and the reflector's shards, the same way */
extern EFDesc dEFManagerReflectBreakEffectDesc;
/* and the grab swirl */
extern EFDesc dEFManagerCatchSwirlEffectDesc;
/* and Samus's grapple beam glow */
extern EFDesc dEFManagerSamusGrappleBeamEffectDesc;
/* and the entry Poke Ball */
extern EFDesc dEFManagerMBallThrownEffectDesc;
/* and Kirby's two stars */
extern EFDesc dEFManagerCaptureKirbyStarEffectDesc;
extern EFDesc dEFManagerLoseKirbyStarEffectDesc;
extern EFDesc dEFManagerPikachuThunderShockEffectDesc;

static EFModel sEFManagerModels[] =
{
    { &dEFManagerRebirthHaloEffectDesc, "efhalo.mdl", NULL, FALSE },
    /* the Poké Ball's opening rays, the one effect in
     * this table the ITEM system makes rather than a fighter. 4
     * joints, 4 batches over 2 DL links, and 4 MObjs the MatAnimJoint
     * steps -- the first effect pack here with an MObjSub at all. */
    { &dEFManagerMBallRaysEffectDesc, "efmballrays.mdl", NULL, FALSE },
    /* The item-use step: the pickup swirl, out of the same bank as the
     * halo and the rays. Six joints, four MObjs, one animation. */
    { &dEFManagerItemGetSwirlEffectDesc, "efswirl.mdl", NULL, FALSE },
    { &dEFManagerDamageFlyOrbsEffectDesc, "eforbs.mdl", NULL, FALSE },
    { &dEFManagerDamageSlashEffectDesc, "efslash.mdl", NULL, FALSE },
    { &dEFManagerDamageFlySparksEffectDesc, "efspark.mdl", NULL, FALSE },
    /* the star rod's own EFDesc, the same efspark.mdl --
     * efModelPackFor keys on the descriptor, not the file, so a second
     * EFDesc over the same picture needs its own row here too. */
    { &dEFManagerStarRodSparkEffectDesc, "efspark.mdl", NULL, FALSE },
    { &dEFManagerDamageFlyMDustEffectDesc, "efmdust.mdl", NULL, FALSE },
    { &dEFManagerDeadExplodeEffectDesc, "efdexp.mdl", NULL, FALSE },
    { &dEFManagerShieldEffectDesc, "efshield.mdl", NULL, FALSE },
    { &dEFManagerYoshiShieldEffectDesc, "efyegg.mdl", NULL, FALSE },
    /* dEFManagerYoshiEggEscapeEffectDesc, the same
     * efyegg.mdl the shield above already loads. */
    { &dEFManagerYoshiEggEscapeEffectDesc, "efyegg.mdl", NULL, FALSE },
    /* the small shock burst's own row, its own file --
     * EFCommonEffects2, not the picture the sparks and the dust share. */
    { &dEFManagerShockSmallEffectDesc, "efshock.mdl", NULL, FALSE },
    /* the fire spark's own row, its own file too --
     * EFCommonEffects2 again, different offsets from the shock burst. */
    { &dEFManagerFireSparkEffectDesc, "effirespark.mdl", NULL, FALSE },
    { &dEFManagerImpactWaveEffectDesc, "efimpactwave.mdl", NULL, FALSE },
    /* Mario's (and Luigi's) warp pipe, the first entry
     * vehicle. relocData 356's three-joint tree and its AnimJoint, baked
     * to romdisk/efdokan.mdl by tools/export/ssb_effectexport.py --what dokan.
     * Unlike every weapon model this port packs it is LIT -- a solid
     * object on the stage rather than a billboard. */
    { &dEFManagerMarioEntryDokanEffectDesc, "efdokan.mdl", NULL, FALSE },
    /* Donkey Kong's barrel (relocData 355) and Samus's
     * arrival point (349). The point is the first model in this port with
     * more than one DObjDLLink on a node -- the game draws both, one into
     * each DL head -- and the first mixed one, two lit batches and a flat
     * quad for the glow. */
    { &dEFManagerDonkeyEntryTaruEffectDesc, "eftaru.mdl", NULL, FALSE },
    { &dEFManagerSamusEntryPointEffectDesc, "efpoint.mdl", NULL, FALSE },
    /* Kirby's entry star (relocData 348), the first effect
     * pack in this port with TWO animations -- the star sweeps in from
     * the right or the left and its maker picks by index. It is also a
     * flat billboard, no lit batch at all, where the pipe and the barrel
     * are lit throughout: there is no rule about a vehicle's lighting. */
    { &dEFManagerKirbyEntryStarEffectDesc, "efstar.mdl", NULL, FALSE },
    /* Link's entry wave and the beam that comes down with
     * it (relocData 353), the fifth and sixth vehicles -- and the only
     * entrance in the game that makes two effects. Both are the first
     * vehicles here with an MObjSub and a MatAnimJoint. */
    { &dEFManagerLinkEntryWaveEffectDesc, "efwave.mdl", NULL, FALSE },
    { &dEFManagerLinkEntryBeamEffectDesc, "efbeam.mdl", NULL, FALSE },
    /* Yoshi's entry egg (relocData 354), the seventh
     * vehicle -- the first whose o_dobjsetup is a bare display list
     * rather than a DObjDesc tree, and the first effect pack in this
     * port whose MObj steps a SPRITE ARRAY: two pictures of the egg,
     * baked side by side as a run its FPackMObjs indexes. */
    { &dEFManagerYoshiEntryEggEffectDesc, "efentryegg.mdl", NULL, FALSE },
    /* Fox's Arwing (relocData 161 for the tree, 346 for the
     * two AnimJoint arrays), the eighth vehicle and by a distance the
     * largest -- twelve joints, twenty-seven batches and twenty-one
     * textures, where no vehicle before it passed three joints. */
    { &dEFManagerFoxEntryArwingEffectDesc, "efarwing.mdl", NULL, FALSE },
    /* Captain Falcon's Blue Falcon (relocData 350), the
     * ninth vehicle and the last with an arm of its own -- the tenth,
     * the Poke Ball, reads its file out of gITManagerCommonData and has
     * its own row above. Twelve joints again, and the only vehicle
     * whose EFDesc lacks 0x1. */
    { &dEFManagerCaptainEntryCarEffectDesc, "efcar.mdl", NULL, FALSE },
    /* the Egg Lay egg (relocData 339). Two joints and one
     * quad, and three animations over them -- the first pack in this
     * port with three. It is small because it is drawn at the size the
     * maker writes into it, one per fighter kind. */
    { &dEFManagerYoshiEggLayEffectDesc, "efegglay.mdl", NULL, FALSE },
    /* Kirby's Final Cutter, four packs out of relocData
     * 348. The two arcs carry NO texture at all -- untextured triangles,
     * as the boomerang's motion trail is -- and the blade carries no
     * animation. */
    { &dEFManagerKirbyCutterUpEffectDesc, "efcutup.mdl", NULL, FALSE },
    { &dEFManagerKirbyCutterDownEffectDesc, "efcutdown.mdl", NULL, FALSE },
    { &dEFManagerKirbyCutterDrawEffectDesc, "efcutdraw.mdl", NULL, FALSE },
    { &dEFManagerKirbyCutterTrailEffectDesc, "efcuttrail.mdl", NULL, FALSE },
    /* the glow around Link's Spin Attack, out of the same
     * Special2 his entry wave and beam came from. */
    { &dEFManagerLinkSpinAttackEffectDesc, "efspin.mdl", NULL, FALSE },
    /* the spark Kirby's rapid jab throws (relocData 348). */
    { &dEFManagerVulcanJabEffectDesc, "efvulcan.mdl", NULL, FALSE },
    /* G04: Ness's four. PSI Magnet's bubble is NessSpecial2's
     * (relocData 352) and the PK Thunder wave NessModel's (335) -- both
     * the entry wave's shape, a tree with one DL link, one MObj of two
     * sprites stepped by a MatAnimJoint, and an AnimJoint. The bolt's
     * last trail segment and its reflected twin's are one EFDesc shape
     * over one llNessModelPKThunderTrailDObjDesc -- a DObjDLLink array
     * bound to a single DObj, one quad with its own texture and nothing
     * to animate -- so two rows name efpktrail.mdl, as the star rod's
     * does efspark.mdl. */
    { &dEFManagerNessPsychicMagnetEffectDesc, "efpsimagnet.mdl", NULL, FALSE },
    { &dEFManagerNessPKThunderWaveEffectDesc, "efpkwave.mdl", NULL, FALSE },
    { &dEFManagerNessPKThunderTrailEffectDesc, "efpktrail.mdl", NULL, FALSE },
    { &dEFManagerNessPKReflectTrailEffectDesc, "efpktrail.mdl", NULL, FALSE },
    /* G10: a reflector's shards. Five joints, three MObjs over
     * three DL links, one AnimJoint and a MatAnimJoint. */
    { &dEFManagerReflectBreakEffectDesc, "efreflectbreak.mdl", NULL, FALSE },
    /* V10: the swirl a grab pulls its catch into. Six joints,
     * four MObjs over four DL links, one AnimJoint and a MatAnimJoint. */
    { &dEFManagerCatchSwirlEffectDesc, "efcatchswirl.mdl", NULL, FALSE },
    /* V10: Samus's grapple beam glow. Two joints, one MObj
     * whose MatAnimJoint flips between its two sprites. */
    { &dEFManagerSamusGrappleBeamEffectDesc, "efgrapplebeam.mdl", NULL, FALSE },
    /* V18: the four makers ported with no pack. The Reflector's
     * two joints carry its four AnimJoints side by side, in
     * dEFManagerFoxReflectorAnimJointOffsets' order; Falcon Punch's flame
     * is one DObj and no AnimJoint; Sing's six joints hang four DL links. */
    { &dEFManagerFoxReflectorEffectDesc, "efreflector.mdl", NULL, FALSE },
    { &dEFManagerCaptainFalconKickEffectDesc, "effalconkick.mdl", NULL, FALSE },
    { &dEFManagerCaptainFalconPunchEffectDesc, "effalconpunch.mdl", NULL, FALSE },
    { &dEFManagerPurinSingEffectDesc, "efsing.mdl", NULL, FALSE },
    /* V10: the Poke Ball Pikachu and Jigglypuff are thrown in
     * from. Four joints, one MObj, and two AnimJoint/MatAnimJoint pairs,
     * one per facing. */
    { &dEFManagerMBallThrownEffectDesc, "efmballthrown.mdl", NULL, FALSE },
    /* V10: the ground Thunder Jolt's spark. Two joints, one MObj
     * flipping two sprites, an AnimJoint and a MatAnimJoint. */
    { &dEFManagerThunderJoltEffectDesc, "efjoltspark.mdl", NULL, FALSE },
    /* V10: the Thunder's trail spark. One DObj with a DL link,
     * one MObj whose four sprites the maker and the update pick. */
    { &dEFManagerPikachuThunderTrailEffectDesc, "efthundertrail.mdl", NULL, FALSE },
    /* V10: Kirby's star, over a swallowed fighter and as the lost
     * copy. A bare root and the Star Rod's display list on its child. */
    { &dEFManagerCaptureKirbyStarEffectDesc, "efkirbystar.mdl", NULL, FALSE },
    { &dEFManagerLoseKirbyStarEffectDesc, "efkirbystar.mdl", NULL, FALSE },
    /* G17: Pikachu's forward-smash sparks, three animations and
     * three MatAnimJoints side by side (the maker picks) */
    { &dEFManagerPikachuThunderShockEffectDesc, "efthundershock.mdl", NULL, FALSE },
};

/* Which of the pack's MatAnimJoints the next effect plays, and the port's
 * answer to the one thing efManagerDeadExplodeMakeEffect does that no
 * other maker does: it writes a *different* block offset into the shared
 * descriptor's o_matanim_joint before making the effect
 * (efmanager.c:4808), so one EFDesc yields four material animations. The
 * port's descriptor offsets are all zero and the scripts live in the
 * pack, so the four are baked side by side (FPackMObjs.alt_count) and
 * this says which. Set by the maker immediately before
 * efManagerMakeEffectForce and cleared by efManagerAddModel, which is
 * the same window the decomp's assignment is live for. */
static s32 sEFManagerMatAnimAlt;

/* The screen quake's four AnimJoint scripts. It has no
 * EFDesc and does not come through efManagerMakeEffect at all -- the
 * maker below builds its one DObj by hand, as the game's does -- so its
 * pack is loaded on its own. */
static Fighter *sEFManagerQuakePack;
static sb32 sEFManagerQuakeIsLoaded;

static EFModel *efModelPackFor(EFDesc *effect_desc)
{
    s32 i;

    for (i = 0; i < (s32)ARRAY_COUNT(sEFManagerModels); i++)
    {
        if (sEFManagerModels[i].desc == effect_desc)
        {
            return &sEFManagerModels[i];
        }
    }
    return NULL;
}

#ifndef FT_HOSTTEST
static Fighter *efModelLoad(EFModel *m)
{
    int pal_bank = 0;
    s32 i;

    if (!m->is_loaded)
    {
        Fighter *pack;

        /* efModelPackFor keys on the EFDesc pointer, so
         * the star rod's row and the damage sparks' row both name
         * "efspark.mdl" and would otherwise malloc and load it twice.
         * A second row for a file another row already has stays a
         * pointer, not a second Fighter -- the same file the makers
         * that reach it draw is the same file in memory. */
        for (i = 0; i < (s32)ARRAY_COUNT(sEFManagerModels); i++)
        {
            if (sEFManagerModels[i].is_loaded &&
                strcmp(sEFManagerModels[i].pack, m->pack) == 0)
            {
                m->loaded = sEFManagerModels[i].loaded;
                m->is_loaded = TRUE;
                return m->loaded;
            }
        }
        pack = malloc(sizeof(Fighter));

        if (pack == NULL)
        {
            syDebugPrintf("efmanager: no room for %s's Fighter\n", m->pack);
            return NULL;
        }
        if (fighter_load(pack, m->pack, &pal_bank) != 0)
        {
            free(pack);
            return NULL;
        }
        m->loaded = pack;
        m->is_loaded = TRUE;
    }
    return m->loaded;
}
#endif

/* efmanager.c:1990, 1994, 2001, 2029, 2035, the transform-type triple.
 *
 * Every branch of efManagerMakeEffect ends by giving a DObj one of the
 * EFDesc's two triples, through lbCommonInitDObj3Transforms or
 * gcAddDObj3TransformsKind, both of which install one XObj per non-Null
 * kind in order. dc_model_add_dobjs has already installed three --
 * gcAddDObjRpyR's Tra, RotRpyR, Sca -- so what is left is to rewrite
 * their kinds, which is the same switch ft/ftdisplaymain.c makes on a
 * fighter's TopN (src/dc/ftdisplaymain.c:139).
 *
 * The kinds go into the slots as they stand, because the port's three
 * slots are multiplied in order and so are the game's XObjs. The one
 * triple that cannot is a *fused* kind: nGCMatrixKindTraRotRpyRSca is
 * one XObj on the N64 and three here, and it is what the halo's
 * transform_types2 asks for. Spelling it out is exactly what the slots
 * already hold, so that case is the one the port names. Any other fused
 * kind would need the same treatment and says so on the serial log
 * rather than drawing wrong. */
static void efManagerSetTransformTypes(DObj *dobj,
                                       const DObjTransformTypes *types)
{
    u8 kinds[3];
    s32 i;

    kinds[0] = types->tk1;
    kinds[1] = types->tk2;
    kinds[2] = types->tk3;

    if (kinds[0] == nGCMatrixKindTraRotRpyRSca &&
        kinds[1] == nGCMatrixKindNull && kinds[2] == nGCMatrixKindNull)
    {
        kinds[0] = nGCMatrixKindTra;
        kinds[1] = nGCMatrixKindRotRpyR;
        kinds[2] = nGCMatrixKindSca;
    }
    if (dobj->xobjs_num < 3)
    {
        return;
    }
    for (i = 0; i < 3; i++)
    {
        dobj->xobjs[i]->kind = kinds[i];
    }
}

/* efmanager.c:2004-2011 and 2043-2048, the AnimJoint.
 *
 * The game reads an AObjEvent32* per DObj out of the effect's file at
 * the offset the EFDesc names and hands the array to gcAddAnimAll. The
 * pack carries the same array as a word index per joint, -1 where the
 * joint has none (FPackAnim.off_entries), because a baked pointer would
 * not survive the trip; this turns it back into pointers, once per
 * effect made. It is the same rebuild if/ifcommon.c does for the
 * off-screen arrows and src/dc/efmanager.c's own efQuakeAnimJoint does
 * for the quake, and it is what was missing from  13: the halo
 * has an AnimJoint and until now nothing attached it. */
static AObjEvent32 *sEFManagerAnimJoint[FIGHTER_MAX_JOINTS];

/* Which of the pack's AnimJoints the next effect plays. The same shape
 * as sEFManagerMatAnimAlt above and for the same reason: Kirby's entry
 * star picks between an L block and an R one by writing the offset into
 * the shared descriptor's o_anim_joint (efmanager.c:5195) before making
 * the effect, so one EFDesc yields two animations. The port's offsets
 * are all zero and both blocks live in the pack, so this says which.
 * Set by the maker immediately before efManagerMakeEffect*, taken and
 * cleared by efManagerAddModel. */
static s32 sEFManagerAnimAlt;

static AObjEvent32 **efModelAnimJoint(const Fighter *f, s32 alt)
{
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sEFManagerAnimJoint); i++)
    {
        sEFManagerAnimJoint[i] = NULL;
    }
#ifndef FT_HOSTTEST
    if ((f != NULL) && (alt >= 0) && (alt < (s32)f->hd->anim_count))
    {
        const FPackAnim *anim = &f->anims[alt];
        const s32 *entries = (const s32 *)((const u8 *)f->blob
                                           + anim->off_entries);

        for (i = 0; (i < f->hd->joint_count) &&
                    (i < ARRAY_COUNT(sEFManagerAnimJoint)); i++)
        {
            if ((entries[i] >= 0) && ((u32)entries[i] < anim->nwords))
            {
                sEFManagerAnimJoint[i] =
                    (AObjEvent32 *)((u8 *)f->blob + anim->off_words)
                    + entries[i];
            }
        }
    }
#else
    (void)f;
    (void)alt;
#endif
    return sEFManagerAnimJoint;
}

/* efmanager.c:1985-2049, the four branches, as one.
 *
 * All four end in a tree of DObjs with transform_types1 on the root and
 * transform_types2 on everything under it, and dc_model_add_dobjs builds
 * that tree out of the pack. The walk below is the decomp's own
 * (efmanager.c:2036-2041) with gcGetTreeDObjNext for
 * lbCommonGetTreeDObjNextFromRoot: the difference between the two shows
 * only for a descriptor with several roots, where the decomp leaves the
 * later roots' kinds alone, and no descriptor the port has read is one.
 */
static sb32 efManagerAddModel(GObj *effect_gobj, EFDesc *effect_desc)
{
    EFModel *m = efModelPackFor(effect_desc);
    Fighter *pack = NULL;
    DObj *root;
    /* Taken and cleared here, before anything can return: the maker sets
     * it for exactly one effect, as the decomp's assignment to
     * o_matanim_joint stands for exactly one call. */
    s32 alt = sEFManagerMatAnimAlt;
    s32 anim_alt = sEFManagerAnimAlt;

    sEFManagerMatAnimAlt = 0;
    sEFManagerAnimAlt = 0;

    if (m == NULL)
    {
        syDebugPrintf("efmanager: no pack for the EFDesc at 0x%08X\n",
                      (unsigned)(uintptr_t)effect_desc);
        return FALSE;
    }
#ifndef FT_HOSTTEST
    pack = efModelLoad(m);

    if (pack == NULL)
    {
        return FALSE;
    }
    if (dc_model_add_dobjs(effect_gobj, NULL, pack, NULL) < 0)
    {
        return FALSE;
    }
#else
    /* As the spotlight's and the arrows': the host cross-test links the
     * scene and not the renderer, so there is no pack to build a tree
     * from and the makers' own placement still needs one.
     *
     * A stand and three children with one MObj each, which is the
     * deepest any maker in this file reaches: efManagerDeadExplode-
     * MakeEffect takes dobj->child and dobj->child->sib_next->sib_next
     * and writes a player colour into both of their MObjs
     * (efmanager.c:4818-4835). Every other maker takes the first child
     * or none, so one shape does for all of them, and the MObjs come
     * through gcAddMObjAll -- the decomp's own walk, the one
     * dc_model_add_mobjs feeds on the target -- rather than by hand. */
    {
        static MObjSub subs[4];
        static MObjSub *rows[4][2];
        static MObjSub **p_subs[4];
        s32 k;

        if (gcAddDObjRpyR(effect_gobj, NULL) == NULL)
        {
            syDebugPrintf("efmanager: no DObj for the stand\n");
            return FALSE;
        }
        for (k = 0; k < 3; k++)
        {
            if (gcAddDObjChildRpyR(DObjGetStruct(effect_gobj), NULL) == NULL)
            {
                syDebugPrintf("efmanager: no DObj for child %d\n", (int)k);
                return FALSE;
            }
            memset(&subs[k], 0, sizeof(subs[k]));
            rows[k][0] = &subs[k];
            rows[k][1] = NULL;
            p_subs[k + 1] = rows[k];
        }
        /* and the stand one too: the Thunder's trail spark is a single
         * DObj whose maker writes dobj->mobj (efmanager.c:4530) */
        memset(&subs[3], 0, sizeof(subs[3]));
        rows[3][0] = &subs[3];
        rows[3][1] = NULL;
        p_subs[0] = rows[3];
        gcAddMObjAll(effect_gobj, p_subs);

        /* and a grandchild under the first child, with no MObj, added
         * after the MObjs so their walk is unchanged: the Thunder Shock's
         * maker hangs an XObj on dobj->child->child (efmanager.c:4430). */
        if (gcAddDObjChildRpyR(DObjGetStruct(effect_gobj)->child, NULL) == NULL)
        {
            syDebugPrintf("efmanager: no DObj for the grandchild\n");
            return FALSE;
        }
        /* and, for Sing alone, three under the second child: its maker
         * walks dobj->child->sib_next->child and two siblings of it
         * (efmanager.c:4760-4770). Alone, because every other effect's
         * DObjs would grow the pool the item tests hold level. */
        for (k = 0; (effect_desc == &dEFManagerPurinSingEffectDesc) && (k < 3); k++)
        {
            if (gcAddDObjChildRpyR(DObjGetStruct(effect_gobj)->child->sib_next, NULL) == NULL)
            {
                syDebugPrintf("efmanager: no DObj for grandchild %d\n", (int)k);
                return FALSE;
            }
        }
    }
#endif
    root = DObjGetStruct(effect_gobj);

    if (root != NULL)
    {
        DObj *dobj;

        efManagerSetTransformTypes(root, &effect_desc->transform_types1);

        for (dobj = gcGetTreeDObjNext(root); dobj != NULL;
             dobj = gcGetTreeDObjNext(dobj))
        {
            efManagerSetTransformTypes(dobj,
                                       &effect_desc->transform_types2);
        }
    }
    /* efmanager.c:2002-2003 and 2041-2042's gcAddMObjAll, and the
     * MatAnimJoint half of the gcAddAnimAll under it. The decomp makes
     * one call with both tables because both sit in the same relocData
     * file; here the MObjs and their scripts are the pack's FPackMObjs
     * section and the joint animation is its animation section, so it is
     * two calls -- dc_model_add_mobjs is gcAddMObjAll followed by
     * gcAddMatAnimJointAll, in that order, both the decomp's own
     * (src/dc/objmodel.c). A pack with no MObjs does nothing here, which
     * is every effect before this one, and every effect in the host
     * cross-test, where there is no pack at all. */
    if (pack != NULL)
    {
        dc_model_add_mobjs_alt(effect_gobj, pack, 0.0F, alt);
    }

    if (effect_desc->o_anim_joint != 0)
    {
        gcAddAnimAll(effect_gobj, efModelAnimJoint(pack, anim_alt), NULL,
                     0.0F);
    }
    return TRUE;
}

GObj* efManagerMakeEffect(EFDesc *effect_desc, sb32 is_force_return)
{
    GObj *effect_gobj;
    EFStruct *ep;
    u8 effect_flags;

    effect_flags = effect_desc->flags;

    if (effect_flags & EFFECT_FLAG_USERDATA)
    {
        ep = efManagerGetNextStructAlloc(is_force_return);

        if (ep == NULL)
        {
            return NULL;
        }
        ep->proc_update = effect_desc->proc_update;
    }
    else ep = NULL;

    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, efManagerFuncRun, (effect_flags & EFFECT_FLAG_SPECIALLINK) ? nGCCommonLinkIDSpecialEffect : nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        if (ep != NULL)
        {
            efManagerSetPrevStructAlloc(ep);
        }
        return NULL;
    }
    effect_gobj->user_data.p = ep;

    if (effect_desc->proc_display == NULL)
    {
        return effect_gobj;
    }
    gcAddGObjDisplay(effect_gobj, effect_desc->proc_display, effect_desc->dl_link, 2, -1);

    /* DIVERGES: efmanager.c:1985-2049, the model. */
    if (efManagerAddModel(effect_gobj, effect_desc) == FALSE)
    {
        if (ep != NULL)
        {
            efManagerSetPrevStructAlloc(ep);
        }
        gcEjectGObj(effect_gobj);

        return NULL;
    }
    gcPlayAnimAll(effect_gobj);

    return effect_gobj;
}

/* ---- efmanager.c:2060-2070: the two wrappers, which are the whole of
 * what the rest of the file calls. ---- */
// 0x800FDAFC
GObj* efManagerMakeEffectNoForce(EFDesc *effect_desc)
{
    return efManagerMakeEffect(effect_desc, FALSE);
}

// 0x800FDB1C
GObj* efManagerMakeEffectForce(EFDesc *effect_desc)
{
    return efManagerMakeEffect(effect_desc, TRUE);
}

/* ---- efmanager.c:2072-2115: giving a struct back. efManagerMakeEffect
 * and its two wrappers (efmanager.c:1928-2070) are the model path and
 * are not here. ---- */

// 0x800FDB3C - Destroy effect GObj and particle too if applicable
LBParticle* efManagerDestroyParticleGObj(LBParticle *pc, GObj *effect_gobj)
{
    if (pc != NULL)
    {
        lbParticleEjectStruct(pc);
    }
    if (efGetStruct(effect_gobj) != NULL)
    {
        EFStruct *ep = efGetStruct(effect_gobj);

        efManagerSetPrevStructAlloc(ep);
    }
    gcEjectGObj(effect_gobj);

    return NULL;
}

// 0x800FDB88
void efManagerDefaultProcDead(LBTransform *xf)
{
    if (efGetStruct(xf->effect_gobj) != NULL)
    {
        EFStruct *ep = efGetStruct(xf->effect_gobj);

        efManagerSetPrevStructAlloc(ep);
    }
    gcEjectGObj(xf->effect_gobj);
}

// 0x800FDBCC
void efManagerDefaultProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    ep->effect_vars.common.xf->translate.x += ep->effect_vars.common.vel.x;
    ep->effect_vars.common.xf->translate.y += ep->effect_vars.common.vel.y;
}

// 0x800FDBFC - Unused
void func_ovl2_800FDBFC(void)
{
    return;
}

/* ---- efmanager.c:2117-2259, 2261-2325, 2349-2483, 3078-3136, 3724-3755,
 * 3862-3995: the makers. ---- */

// 0x800FDC04
LBParticle* efManagerDamageNormalLightMakeEffect(Vec3f *pos, s32 player, s32 size, sb32 is_static)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;
    f32 angle;
    f32 vel;
    f32 scale;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = lbParticleMakeScriptID(gEFManagerParticleBankID, dEFManagerDamageNormalLightIDs[player]);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            xf->effect_gobj = effect_gobj;
            xf->proc_dead = efManagerDefaultProcDead;

            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            gcAddGObjProcess(effect_gobj, efManagerDefaultProcUpdate, nGCProcessKindFunc, 3);

            ep->effect_vars.common.xf = xf;

            xf->translate = *pos;

            vel = (is_static != FALSE) ? 0.0F : ((syUtilsRandFloat() * 38.0F) + 12.0F);

            angle = syUtilsRandFloat() * F_CLC_DTOR32(360.0F);

            ep->effect_vars.common.vel.x = __cosf(angle) * vel;
            ep->effect_vars.common.vel.y = __sinf(angle) * vel;

            scale = (size < 10) ? (((10 - size) * -0.05F) + 1.0F) : (((size - 10) * 0.13F) + 1.0F);

            xf->scale.x = xf->scale.y = xf->scale.z = scale;
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}

// 0x800FDE3C
void efManagerDamageNormalHeavyProcDead(LBTransform *xf)
{
    EFStruct *ep = efGetStruct(xf->effect_gobj);
    Vec3f pos = xf->translate;

    efManagerDamageNormalLightMakeEffect(&pos, ep->effect_vars.damage_normal_heavy.player, ep->effect_vars.damage_normal_heavy.size, FALSE);
    efManagerSetPrevStructAlloc(efGetStruct(xf->effect_gobj));
    gcEjectGObj(xf->effect_gobj);
}

// 0x800FDEAC
LBParticle* efManagerDamageNormalHeavyMakeEffect(Vec3f *pos, s32 player, s32 size)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = lbParticleMakeScriptID(gEFManagerParticleBankID, 0x64);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            xf->effect_gobj = effect_gobj;
            xf->proc_dead = efManagerDamageNormalHeavyProcDead;

            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return FALSE;
            }
            ep->effect_vars.common.xf = xf; // WHAT (This overlaps with damage_normal_heavy.size and is eventually overwritten with the correct value)

            xf->translate = *pos;

            ep->effect_vars.damage_normal_heavy.pos = *pos;
            ep->effect_vars.damage_normal_heavy.player = player;
            ep->effect_vars.damage_normal_heavy.size = size;

            pc->primcolor.r = dEFManagerDamageNormalHeavyPrimColorR[player];
            pc->primcolor.g = dEFManagerDamageNormalHeavyPrimColorG[player];
            pc->primcolor.b = dEFManagerDamageNormalHeavyPrimColorB[player];
            pc->primcolor.a = 0xFF;

            pc->envcolor.r = dEFManagerDamageNormalHeavyEnvColorR[player];
            pc->envcolor.g = dEFManagerDamageNormalHeavyEnvColorG[player];
            pc->envcolor.b = dEFManagerDamageNormalHeavyEnvColorB[player];
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}

/* ---- efmanager.c:2261-2327 efManagerImpactShockMakeEffect:
 * the Charge Shot's hit flash (wp/wpsamus/wpsamuschargeshot.c's ProcHit,
 * sized by the shot's damage). Verbatim, the same plain-particle shape as
 * the damage sparks either side of it: one script (0x25) out of the
 * common effect bank, a random kick, a scale from the size. ---- */
// 0x800FE068
LBParticle* efManagerImpactShockMakeEffect(Vec3f *pos, s32 size)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;
    f32 angle;
    f32 vel;
    f32 scale;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = lbParticleMakeScriptID(gEFManagerParticleBankID, 0x25);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            xf->effect_gobj = effect_gobj;
            xf->proc_dead = efManagerDefaultProcDead;

            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            gcAddGObjProcess(effect_gobj, efManagerDefaultProcUpdate, nGCProcessKindFunc, 3);

            ep->effect_vars.common.xf = xf;

            xf->translate = *pos;

            vel = ((syUtilsRandFloat() * 8.0F) + 2.0F);

            angle = syUtilsRandFloat() * F_CLC_DTOR32(360.0F);

            ep->effect_vars.common.vel.x = __cosf(angle) * vel;
            ep->effect_vars.common.vel.y = __sinf(angle) * vel;

            scale = (size < 10) ? (((10 - size) * -0.05F) + 1.0F) : (((size - 10) * 0.15F) + 1.0F);

            xf->scale.x = xf->scale.y = xf->scale.z = scale;
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}

// 0x800FE2F4
LBParticle* efManagerDamageFireMakeEffect(Vec3f *pos, s32 size)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;
    f32 angle;
    f32 vel;
    f32 scale;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = lbParticleMakeScriptID(gEFManagerParticleBankID, 0x4D);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            xf->effect_gobj = effect_gobj;
            xf->proc_dead = efManagerDefaultProcDead;

            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            gcAddGObjProcess(effect_gobj, efManagerDefaultProcUpdate, nGCProcessKindFunc, 3);

            ep->effect_vars.common.xf = xf;

            xf->translate = *pos;

            vel = ((syUtilsRandFloat() * 18.0F) + 12.0F);

            angle = syUtilsRandFloat() * F_CLC_DTOR32(360.0F);

            ep->effect_vars.common.vel.x = __cosf(angle) * vel;
            ep->effect_vars.common.vel.y = __sinf(angle) * vel;

            scale = (size < 10) ? (((10 - size) * -0.05F) + 1.0F) : (((size - 10) * 0.15F) + 1.0F);

            xf->scale.x = xf->scale.y = xf->scale.z = scale;
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}

// 0x800FE4EC
LBParticle* efManagerDamageElectricMakeEffect(Vec3f *pos, s32 size)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;
    f32 angle;
    f32 vel;
    f32 scale;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = lbParticleMakeScriptID(gEFManagerParticleBankID, 0x53);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            xf->effect_gobj = effect_gobj;
            xf->proc_dead = efManagerDefaultProcDead;

            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            gcAddGObjProcess(effect_gobj, efManagerDefaultProcUpdate, nGCProcessKindFunc, 3);

            ep->effect_vars.common.xf = xf;

            xf->translate = *pos;

            vel = (syUtilsRandFloat() * 7.0F) + 3.0F;

            angle = syUtilsRandFloat() * F_CLC_DTOR32(360.0F);

            ep->effect_vars.common.vel.x = __cosf(angle) * vel;
            ep->effect_vars.common.vel.y = __sinf(angle) * vel;

            scale = (size < 5) ? (((5 - size) * -0.08F) + 1.0F) : (((size - 5) * 0.15F) + 1.0F);

            xf->scale.x = xf->scale.y = xf->scale.z = scale;
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}

/* ---- efmanager.c:2485-2509: the damage slash.
 *
 * A white streak across the point of a hit, made by
 * ftCommonDamageSetSlash and the item and weapon code. Everything about
 * it that moves is in the two animations the pack carries: the maker
 * only stands it where the hit was, turns it, and scales it by the hit's
 * size. ---- */

// 0x800FE6E4
GObj* efManagerDamageSlashMakeEffect(Vec3f *pos, s32 size, f32 rotate)
{
    GObj *effect_gobj;
    DObj *dobj;
    f32 scale;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerDamageSlashEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    dobj->rotate.vec.f.z = rotate;

    scale = (size < 5) ? (((5 - size) * -0.08F) + 1.0F) : (((size - 5) * 0.18F) + 1.0F);

    dobj->scale.vec.f.x = dobj->scale.vec.f.y = scale;

    return effect_gobj;
}

/* ---- efmanager.c:2704-2768 efManagerDustCollideMakeEffect 0x800FECBC,
 * verbatim. The puff Link's boomerang leaves wherever it
 * touches the stage -- wp/wplink/wplinkboomerang.c's ProcMap is its only
 * caller in the roster (Venusaur's is the other, and Poke Balls are not
 * in the build).
 *
 * Nothing diverges and nothing is stubbed: it is the ordinary particle
 * form, and its script id is 0x55 in gEFManagerParticleBankID -- the
 * SAME script in the SAME bank efManagerDustExpandSmallMakeEffect below
 * has been drawing since  25. So there is no new asset here at
 * all; only the scatter is its own (a random offset in a 300-unit box,
 * a launch angle in [45, 135) degrees, and a random 1x-2x scale). It
 * omits the LBPARTICLE_MASK_GENLINK(0) its twin ORs in, which is zero --
 * copied as it stands, not corrected. ---- */

// 0x800FECBC - Called only by Venusaur and Link's Boomerang?
LBParticle* efManagerDustCollideMakeEffect(Vec3f *pos)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;
    f32 angle;
    f32 vel;
    f32 scale;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = lbParticleMakeScriptID(gEFManagerParticleBankID, 0x55);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            xf->effect_gobj = effect_gobj;
            xf->proc_dead = efManagerDefaultProcDead;

            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            gcAddGObjProcess(effect_gobj, efManagerDefaultProcUpdate, nGCProcessKindFunc, 3);

            ep->effect_vars.common.xf = xf;

            xf->translate = *pos;

            xf->translate.x += ((syUtilsRandFloat() * EFCOMMON_DUSTCOLL_OFF_BASE) + EFCOMMON_DUSTCOLL_OFF_ADD);
            xf->translate.y += ((syUtilsRandFloat() * EFCOMMON_DUSTCOLL_OFF_BASE) + EFCOMMON_DUSTCOLL_OFF_ADD);

            angle = (syUtilsRandFloat() * EFCOMMON_DUSTCOLL_ANGLE_BASE) + EFCOMMON_DUSTCOLL_ANGLE_ADD; // F_CLC_DTOR32(90.0F), QUART_PI32

            ep->effect_vars.common.vel.x = __cosf(angle) * EFCOMMON_DUSTCOLL_VEL_BASE;
            ep->effect_vars.common.vel.y = __sinf(angle) * EFCOMMON_DUSTCOLL_VEL_BASE;

            xf->scale.x = xf->scale.y = xf->scale.z = (syUtilsRandFloat() * 1) + 1.0F;
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}
// 0x800FF648
LBParticle* efManagerDustExpandSmallMakeEffect(Vec3f *pos, f32 f_index)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;
    f32 angle;
    f32 vel;
    f32 scale;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = (f_index == 2.0F) ? lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0x56) : lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0x55);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            xf->effect_gobj = effect_gobj;
            xf->proc_dead = efManagerDefaultProcDead;

            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            gcAddGObjProcess(effect_gobj, efManagerDefaultProcUpdate, nGCProcessKindFunc, 3);

            ep->effect_vars.common.xf = xf;

            xf->translate = *pos;

            ep->effect_vars.common.vel.y = EFCOMMON_DUSTEXPANDSMALL_VEL_Y;
            ep->effect_vars.common.vel.x = EFCOMMON_DUSTEXPANDSMALL_VEL_X;
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}

/* efmanager.c:3605 efManagerSparkleWhiteMakeEffect 0x80100480, verbatim.
 * efmanager.c:3663 efManagerSparkleWhiteMultiExplodeMakeEffect, verbatim
 * -- wp/wpsamus/wpsamusbomb.c's own hit/expire/absorb spark,
 * script id 0x22 rather than SparkleWhite's 0x73, otherwise identical.
 * efmanager.c:5287 efManagerFireGrindMakeEffect   0x80102DEC, verbatim.
 * Three of the roster's spark effects (wp/wpmario/wpmariofireball.c calls
 * SparkleWhite on a hit, wp/wpsamus/wpsamusbomb.c calls MultiExplode on
 * hit/expire/absorb, FireGrind is the fireball's wall grind). All three
 * are the plain particle form -- one lbParticleMakeScriptID, a transform,
 * a translate -- exactly as the decomp writes them, over the same
 * lbParticle the DC build already runs. Nothing diverges. */
LBParticle* efManagerSparkleWhiteMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0x73);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

LBParticle* efManagerSparkleWhiteMultiExplodeMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0x22);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

LBParticle* efManagerFireGrindMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0xB);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

/* efmanager.c:5517 efManagerFoxBlasterGlowMakeEffect 0x80103320, verbatim.
 *  39: Fox's Blaster shot glow (wp/wpfox/wpfoxblaster.c calls it
 * on spawn, on every hit/hop/setoff, and once per map-collision tic while
 * the shot is alive). Simpler than SparkleWhite/FireGrind above --
 * lbParticleMakeCommon instead of lbParticleMakeScriptID (no GENLINK bank
 * mask; script_id 0x62 is looked up in the *fighter's own* particle bank
 * pointed at by gEFManagerParticleBankID at call time, not a fixed one),
 * and the particle's own `pos` is written directly rather than through an
 * LBTransform. Nothing diverges. */
LBParticle* efManagerFoxBlasterGlowMakeEffect(Vec3f *pos)
{
    LBParticle *pc;

    pc = lbParticleMakeCommon(gEFManagerParticleBankID, 0x62);

    if (pc != NULL)
    {
        pc->pos.x = pos->x;
        pc->pos.y = pos->y;
        pc->pos.z = pos->z;
    }
    return pc;
}

/* ---- efmanager.c:3182-3278: the damage orbs.
 *
 * The flyer's update is the whole of its motion -- a velocity added to
 * the DObj's translate each tic, gravity taken off the Y, and the
 * struct given back when the lifetime runs out. The spawner's makes a
 * flyer every fourth tic at a random angle in the upper 60 degrees,
 * random speed and random scale, and gives its own struct back the same
 * way. Between them they are two of the thirty-eight EFStructs a match
 * has, taken and returned dozens of times a hit, which is what
 *  12's pool test was about.
 *
 * The Random maker is a one-in-four coin toss and ft/ftmain.c:2740 is
 * what calls it: one orb burst in four damage frames. ---- */

// 0x800FF8C0
void efManagerDamageFlyOrbsProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    DObj *dobj = DObjGetStruct(effect_gobj);

    gcPlayAnimAll(effect_gobj);

    ep->effect_vars.damage_fly_orbs.lifetime--;

    if (ep->effect_vars.damage_fly_orbs.lifetime < 0)
    {
        efManagerSetPrevStructAlloc(ep);
        gcEjectGObj(effect_gobj);
    }
    else
    {
        dobj->translate.vec.f.x += ep->effect_vars.damage_fly_orbs.vel.x;
        dobj->translate.vec.f.y += ep->effect_vars.damage_fly_orbs.vel.y;

        ep->effect_vars.damage_fly_orbs.vel.y -= EFCOMMON_DAMAGEFLYORBS_VEL_SUB;
    }
}

// 0x800FF95C
void efManagerDamageSpawnOrbsProcUpdate(GObj *this_gobj)
{
    GObj *new_gobj;
    DObj *dobj;
    EFStruct *this_ep;
    EFStruct *new_ep;
    f32 vel;
    f32 angle;

    this_ep = efGetStruct(this_gobj);

    if (!(this_ep->effect_vars.damage_spawn_orbs.lifetime % EFCOMMON_DAMAGESPAWNORBS_LIFETIME_RANDOM_MOD))
    {
        new_gobj = efManagerMakeEffectNoForce(&dEFManagerDamageFlyOrbsEffectDesc);

        if (new_gobj != NULL)
        {
            dobj = DObjGetStruct(new_gobj);
            new_ep = efGetStruct(new_gobj);

            dobj->translate.vec.f = this_ep->effect_vars.damage_spawn_orbs.pos;

            dobj->scale.vec.f.x = dobj->scale.vec.f.y = (syUtilsRandFloat() * EFCOMMON_DAMAGESPAWNORBS_SCALE_BASE) + EFCOMMON_DAMAGESPAWNORBS_SCALE_ADD;

            vel = (syUtilsRandFloat() * EFCOMMON_DAMAGESPAWNORBS_VEL_BASE) + EFCOMMON_DAMAGESPAWNORBS_VEL_ADD;

            angle = (syUtilsRandFloat() * EFCOMMON_DAMAGESPAWNORBS_ANGLE_BASE) + EFCOMMON_DAMAGESPAWNORBS_ANGLE_ADD1 + EFCOMMON_DAMAGESPAWNORBS_ANGLE_ADD2;

            new_ep->effect_vars.damage_fly_orbs.vel.x = __cosf(angle) * vel;
            new_ep->effect_vars.damage_fly_orbs.vel.y = __sinf(angle) * vel;
            new_ep->effect_vars.damage_fly_orbs.lifetime = syUtilsRandIntRange(EFCOMMON_DAMAGESPAWNORBS_LIFETIME_RANDOM_MOD) + EFCOMMON_DAMAGESPAWNORBS_LIFETIME_ADD;
        }
    }
    this_ep->effect_vars.damage_spawn_orbs.lifetime--;

    if (this_ep->effect_vars.damage_spawn_orbs.lifetime < 0)
    {
        efManagerSetPrevStructAlloc(this_ep);
        gcEjectGObj(this_gobj);
    }
}

// 0x800FFAB8
GObj* efManagerDamageSpawnOrbsMakeEffect(Vec3f *pos)
{
    GObj *effect_gobj;
    EFStruct *ep;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerDamageSpawnOrbsEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->effect_vars.damage_spawn_orbs.pos = *pos;

    ep->effect_vars.damage_spawn_orbs.lifetime = (syUtilsRandIntRange(3) * 4) + 4;

    return effect_gobj;
}

// 0x800FFB38
GObj* efManagerDamageSpawnOrbsRandomMakeEffect(Vec3f *pos)
{
    if (syUtilsRandIntRange(4) != 0)
    {
        return NULL;
    }
    else return efManagerDamageSpawnOrbsMakeEffect(pos);
}

/* ---- The port's own: the impact wave's colours and its draw
 * 17), the shield's pattern (efManagerShieldModelColors 5).
 *
 * efmanager.c:3287-3295's gDPSetPrimColor/gDPSetEnvColor write the row
 * colour at the decayed alpha and a flat-black env into the translucent
 * head ahead of the model; here the model is a pack shared by every
 * instance of the effect, so the two colours go onto that pack as its
 * live overrides (src/dc/fighter.h fighter_set_prim_color,
 * fighter_set_env_color) just before its draw, which is the same moment.
 * The gDPPipeSync and gDPSetRenderMode(G_RM_AA_ZB_XLU_SURF) are baked
 * into the pack's bucket; the env rows are all zero and the OPT0 build
 * hardcodes (0,0,0,0xFF), so env is a flat black either way. The draw is
 * dc_model_proc_display, the port of gcDrawDObjDLHead0.
 *
 * The host cross-test has no pack and no renderer, so there the colours
 * land in two words it can read back and the draw is nothing. ---- */
#ifdef FT_HOSTTEST
u32 gEFManagerImpactWaveLastPrim;
u32 gEFManagerImpactWaveLastEnv;
#endif

static void efManagerImpactWaveModelColors(GObj *effect_gobj, s32 index, u8 alpha)
{
    u32 prim = ((u32)dEFManagerImpactWavePrimColorR[index] << 24)
             | ((u32)dEFManagerImpactWavePrimColorG[index] << 16)
             | ((u32)dEFManagerImpactWavePrimColorB[index] << 8) | alpha;
    u32 env = 0x000000FF;
#ifndef FT_HOSTTEST
    DObj *root = DObjGetStruct(effect_gobj);

    if ((root != NULL) && (root->dv != NULL))
    {
        Fighter *f = dc_model_of(effect_gobj);

        fighter_set_prim_color(f, prim);
        fighter_set_env_color(f, env);
    }
#else
    (void)effect_gobj;
    gEFManagerImpactWaveLastPrim = prim;
    gEFManagerImpactWaveLastEnv = env;
#endif
}

static void efManagerImpactWaveModelDraw(GObj *effect_gobj)
{
#ifndef FT_HOSTTEST
    dc_model_proc_display(effect_gobj);
#else
    (void)effect_gobj;
#endif
}

// 0x800FFB74
void efManagerImpactWaveProcDisplay(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    s32 index = ep->effect_vars.impact_wave.index;

    /* DIVERGES: the two colours onto the shared pack, then the model
     * draw -- see the note above. */
    efManagerImpactWaveModelColors(effect_gobj, index, (s32)ep->effect_vars.impact_wave.alpha);
    efManagerImpactWaveModelDraw(effect_gobj);
}

// 0x800FFCA4
void efManagerImpactWaveProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    gcPlayAnimAll(effect_gobj);

    if (effect_gobj->anim_frame <= 0.0F)
    {
        efManagerSetPrevStructAlloc(efGetStruct(effect_gobj));
        gcEjectGObj(effect_gobj);
    }
    else
    {
        ep->effect_vars.impact_wave.alpha -= ep->effect_vars.impact_wave.decay;

        if (ep->effect_vars.impact_wave.alpha > 0xFF)
        {
            ep->effect_vars.impact_wave.alpha = 0xFF;
        }
        else if (ep->effect_vars.impact_wave.alpha < 0x00)
        {
            ep->effect_vars.impact_wave.alpha = 0x00;
        }
    }
}

// 0x800FFD58
GObj* efManagerImpactWaveMakeEffect(Vec3f *pos, s32 index, f32 rotate)
{
    GObj *effect_gobj = efManagerMakeEffectNoForce(&dEFManagerImpactWaveEffectDesc);
    DObj *dobj;
    EFStruct *ep;

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);
    ep = efGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    dobj->rotate.vec.f.z = rotate;

    ep->effect_vars.impact_wave.index = index;
    ep->effect_vars.impact_wave.alpha = 255.0F;
    ep->effect_vars.impact_wave.decay = 127.0F / 11.0F;

    return effect_gobj;
}

// 0x800FFDE8
GObj* efManagerImpactAirWaveMakeEffect(Vec3f *pos, s32 index)
{
    return efManagerImpactWaveMakeEffect(pos, index, 0.0F);
}

/* ---- efmanager.c:3359-3411: the star rod's spark, verbatim. It reuses
 * the damage sparks' picture (dEFManagerStarRodSparkEffectDesc's four
 * offsets are the same ones), so this is the maker and the update, not
 * a new model. The update slides the DObj sideways by a velocity that
 * decays toward zero over sixty-two tics -- add_timer counting down --
 * and ejects once the anim runs out, the same shape the flyers below
 * use for the same reason. ---- */

// 0x800FFE08
void efManagerStarRodSparkProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    gcPlayAnimAll(effect_gobj);

    if (effect_gobj->anim_frame <= 0.0F)
    {
        efManagerSetPrevStructAlloc(ep);
        gcEjectGObj(effect_gobj);

        return;
    }
    else
    {
        if (ep->effect_vars.star_rod_spark.add_timer != 0)
        {
            ep->effect_vars.star_rod_spark.add_timer--;

            ep->effect_vars.star_rod_spark.vel.x += ep->effect_vars.star_rod_spark.add.x;
        }
        DObjGetStruct(effect_gobj)->translate.vec.f.x += ep->effect_vars.star_rod_spark.vel.x;
    }
}

// 0x800FFEA4
GObj* efManagerStarRodSparkMakeEffect(Vec3f *pos, s32 lr)
{
    GObj *effect_gobj;
    DObj *dobj;
    EFStruct *ep;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerStarRodSparkEffectDesc);

    if (effect_gobj != NULL)
    {
        dobj = DObjGetStruct(effect_gobj);
        ep = efGetStruct(effect_gobj);

        dobj->translate.vec.f = *pos;

        dobj->rotate.vec.f.z = syUtilsRandFloat() * F_CLC_DTOR32(360.0F); // F_CLC_DTOR32(360.0F)

        dobj->scale.vec.f.x = EFCOMMON_STARRODSPARK_SCALE;
        dobj->scale.vec.f.y = EFCOMMON_STARRODSPARK_SCALE;

        ep->effect_vars.star_rod_spark.vel.x = lr * EFCOMMON_STARRODSPARK_VEL_BASE;
        ep->effect_vars.star_rod_spark.add.x = lr * EFCOMMON_STARRODSPARK_VEL_ADD;
        ep->effect_vars.star_rod_spark.add_timer = EFCOMMON_STARRODSPARK_ADD_TIMER;
    }
    return effect_gobj;
}


/* ---- efmanager.c:3413-3521: the damage sparks.
 *
 * Verbatim, all four. The flyer's update is the plainest in the file --
 * play the animation, eject when it runs out, otherwise move by the
 * velocity and bend it toward the middle while the add timer lasts --
 * and the spawner's is the orbs' shape with three fixed angles instead
 * of a random one. `-(lifetime / 4) + 2` walks
 * dEFManagerDamageSpawnSparksAngles backwards as the lifetime counts
 * down from eight, so the three sparks leave at +18, 0 and -18 degrees,
 * mirrored by the attacker's facing.
 *
 * efManagerDamageFlySparksProcUpdate is dEFManagerDamageFlyMDustEffectDesc's
 * update as well as this one's; the metal dust is the next step and has
 * a model of its own. ---- */

// 0x800FFF74
void efManagerDamageFlySparksProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    gcPlayAnimAll(effect_gobj);

    if (effect_gobj->anim_frame <= 0.0F)
    {
        efManagerSetPrevStructAlloc(ep);
        gcEjectGObj(effect_gobj);
    }
    else
    {
        DObj *dobj = DObjGetStruct(effect_gobj);

        dobj->translate.vec.f.x += ep->effect_vars.damage_fly_sparks.vel.x;
        dobj->translate.vec.f.y += ep->effect_vars.damage_fly_sparks.vel.y;

        if (ep->effect_vars.damage_fly_sparks.add_timer != 0)
        {
            ep->effect_vars.damage_fly_sparks.add_timer--;

            ep->effect_vars.damage_fly_sparks.vel.x += ep->effect_vars.damage_fly_sparks.add.x;
            ep->effect_vars.damage_fly_sparks.vel.y += ep->effect_vars.damage_fly_sparks.add.y;
        }
    }
}

// 0x80100030
void efManagerDamageSpawnSparksProcUpdate(GObj *effect_gobj)
{
    EFStruct *this_ep;
    DObj *dobj;
    EFStruct *new_ep;
    GObj *new_gobj;
    s32 lifetime;
    f32 angle;
    f32 var;
    f32 unused;

    this_ep = efGetStruct(effect_gobj);
    lifetime = this_ep->effect_vars.damage_spawn_sparks.lifetime;

    if (!(lifetime % EFCOMMON_DAMAGESPAWNSPARK_LIFETIME_MOD))
    {
        new_gobj = efManagerMakeEffectNoForce(&dEFManagerDamageFlySparksEffectDesc);

        if (new_gobj != NULL)
        {
            dobj = DObjGetStruct(new_gobj);
            new_ep = efGetStruct(new_gobj);

            dobj->translate.vec.f = this_ep->effect_vars.damage_spawn_sparks.pos;

            dobj->rotate.vec.f.z = syUtilsRandFloat() * F_CLC_DTOR32(360.0F);

            var = dEFManagerDamageSpawnSparksAngles[ -(lifetime / EFCOMMON_DAMAGESPAWNSPARK_LIFETIME_MOD) + (EFCOMMON_DAMAGESPAWNSPARK_LIFETIME_MOD / 2) ];

            angle = F_CLC_DTOR32(var);

            new_ep->effect_vars.damage_fly_sparks.vel.x = __cosf(angle) * EFCOMMON_DAMAGESPAWNSPARK_VEL_BASE * this_ep->effect_vars.damage_spawn_sparks.lr;
            new_ep->effect_vars.damage_fly_sparks.vel.y = __sinf(angle) * EFCOMMON_DAMAGESPAWNSPARK_VEL_BASE;

            new_ep->effect_vars.damage_fly_sparks.add.x = -new_ep->effect_vars.damage_fly_sparks.vel.x * EFCOMMON_DAMAGESPAWNSPARK_VEL_ADD;
            new_ep->effect_vars.damage_fly_sparks.add.y = -new_ep->effect_vars.damage_fly_sparks.vel.y * EFCOMMON_DAMAGESPAWNSPARK_VEL_ADD;

            new_ep->effect_vars.damage_fly_sparks.add_timer = EFCOMMON_DAMAGESPAWNSPARK_ADD_TIMER;
        }
    }
    this_ep->effect_vars.damage_spawn_sparks.lifetime--;

    if (this_ep->effect_vars.damage_spawn_sparks.lifetime < 0)
    {
        efManagerSetPrevStructAlloc(this_ep);
        gcEjectGObj(effect_gobj);
    }
}

// 0x801001A8
GObj* efManagerDamageSpawnSparksMakeEffect(Vec3f *pos, s32 lr)
{
    GObj *effect_gobj;
    EFStruct *ep;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerDamageSpawnSparksEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->effect_vars.damage_spawn_sparks.pos = *pos;
    ep->effect_vars.damage_spawn_sparks.lifetime = EFCOMMON_DAMAGESPAWNSPARK_LIFETIME;
    ep->effect_vars.damage_spawn_sparks.lr = lr;

    return effect_gobj;
}

// 0x80100218
GObj* efManagerDamageSpawnSparksRandomMakeEffect(Vec3f *pos, s32 lr)
{
    if (syUtilsRandIntRange(4) != 0)
    {
        return NULL;
    }
    else return efManagerDamageSpawnSparksMakeEffect(pos, lr);
}

/* ---- efmanager.c:3523-3602: the metal dust's proc and its two makers.
 *
 * Line for line the sparks' three with `sparks` spelled `mdust`, down to
 * the unused f32 the compiler kept and the backwards read of the angle
 * table: lifetime counts down from eight, and -(lifetime / 4) + 2 turns
 * the three tics where lifetime % 4 == 0 into indices 0, 1 and 2. The
 * flyer's own update is efManagerDamageFlySparksProcUpdate above, which
 * dEFManagerDamageFlyMDustEffectDesc names as its proc_update; there is
 * no efManagerDamageFlyMDustProcUpdate anywhere in the game.
 *
 * What ft/ftparam.c calls is the Random one, which makes a spawner on
 * one hit in four. ---- */

// 0x80100258
void efManagerDamageSpawnMDustProcUpdate(GObj *effect_gobj)
{
    EFStruct *this_ep;
    DObj *dobj;
    EFStruct *new_ep;
    GObj *new_gobj;
    s32 lifetime;
    f32 angle;
    f32 var;
    f32 unused;

    this_ep = efGetStruct(effect_gobj);
    lifetime = this_ep->effect_vars.damage_spawn_mdust.lifetime;

    if (!(lifetime % EFCOMMON_DAMAGESPAWNMDUST_LIFETIME_MOD))
    {
        new_gobj = efManagerMakeEffectNoForce(&dEFManagerDamageFlyMDustEffectDesc);

        if (new_gobj != NULL)
        {
            dobj = DObjGetStruct(new_gobj);
            new_ep = efGetStruct(new_gobj);

            dobj->translate.vec.f = this_ep->effect_vars.damage_spawn_mdust.pos;

            dobj->rotate.vec.f.z = syUtilsRandFloat() * F_CLC_DTOR32(360.0F);

            var = dEFManagerDamageSpawnMDustAngles[ -(lifetime / EFCOMMON_DAMAGESPAWNMDUST_LIFETIME_MOD) + (EFCOMMON_DAMAGESPAWNMDUST_LIFETIME_MOD / 2) ];

            angle = F_CLC_DTOR32(var);

            new_ep->effect_vars.damage_fly_mdust.vel.x = __cosf(angle) * EFCOMMON_DAMAGESPAWNMDUSTVEL_BASE * this_ep->effect_vars.damage_spawn_mdust.lr;
            new_ep->effect_vars.damage_fly_mdust.vel.y = __sinf(angle) * EFCOMMON_DAMAGESPAWNMDUSTVEL_BASE;

            new_ep->effect_vars.damage_fly_mdust.add.x = -new_ep->effect_vars.damage_fly_mdust.vel.x * EFCOMMON_DAMAGESPAWNMDUSTVEL_ADD;
            new_ep->effect_vars.damage_fly_mdust.add.y = -new_ep->effect_vars.damage_fly_mdust.vel.y * EFCOMMON_DAMAGESPAWNMDUSTVEL_ADD;

            new_ep->effect_vars.damage_fly_mdust.add_timer = EFCOMMON_DAMAGESPAWNMDUST_ADD_TIMER;
        }
    }
    this_ep->effect_vars.damage_spawn_mdust.lifetime--;

    if (this_ep->effect_vars.damage_spawn_mdust.lifetime < 0)
    {
        efManagerSetPrevStructAlloc(this_ep);
        gcEjectGObj(effect_gobj);
    }
}

// 0x801003D0
GObj* efManagerDamageSpawnMDustMakeEffect(Vec3f *pos, s32 lr)
{
    GObj *effect_gobj;
    EFStruct *ep;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerDamageSpawnMDustEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->effect_vars.damage_spawn_mdust.pos = *pos;
    ep->effect_vars.damage_spawn_mdust.lifetime = EFCOMMON_DAMAGESPAWNMDUST_LIFETIME;
    ep->effect_vars.damage_spawn_mdust.lr = lr;

    return effect_gobj;
}

// 0x80100440
GObj* efManagerDamageSpawnMDustRandomMakeEffect(Vec3f *pos, s32 lr)
{
    if (syUtilsRandIntRange(4) != 0)
    {
        return NULL;
    }
    else return efManagerDamageSpawnMDustMakeEffect(pos, lr);
}

// 0x80100720 - Plays when a fighter is Star KO'd
LBParticle* efManagerSparkleWhiteDeadMakeEffect(Vec3f *pos, f32 scale)
{
    LBParticle *pc = lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(1), 0x5C);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;

            xf->scale.x = scale;
            xf->scale.y = scale;
            xf->scale.z = scale;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

/* ---- efmanager.c:3756-3811: the screen quake's two procs ----
 *
 * Verbatim, and there is nothing to divert: the quake has no geometry.
 * One DObj with a Translate XObj, an AnimJoint script driving it, and an
 * update that reads the translate back out every frame and hands it to
 * gmCameraSetVelAt -- the camera shakes, nothing is drawn. The one thing
 * it needs of the world is the battle camera's eye-to-target distance,
 * which scales the shake so a zoomed-out camera moves as far on screen
 * as a close one. ---- */

// 0x801007D8
void efManagerQuakeProcUpdate(GObj *effect_gobj)
{
    DObj *dobj;
    Vec3f sub;
    Vec3f pos;
    CObj *cobj;
    f32 mag;

    gcPlayAnimAll(effect_gobj);

    if (effect_gobj->anim_frame <= 0.0F)
    {
        efManagerSetPrevStructAlloc(efGetStruct(effect_gobj));
        gcEjectGObj(effect_gobj);
    }
    else
    {
        cobj = CObjGetStruct(gGMCameraGObj);

        dobj = DObjGetStruct(effect_gobj);

        syVectorDiff3D(&sub, &cobj->vec.at, &cobj->vec.eye);

        mag = syVectorMag3D(&sub);

        if (mag > EFCOMMON_QUAKE_MAGNITUDE)
        {
            mag = mag / EFCOMMON_QUAKE_MAGNITUDE;

            pos.x = dobj->translate.vec.f.z * mag;
            pos.y = dobj->translate.vec.f.y * mag;
        }
        else
        {
            pos.x = dobj->translate.vec.f.z;
            pos.y = dobj->translate.vec.f.y;
        }
        pos.z = 0.0F;

        gmCameraSetVelAt(&pos);
    }
}

// 0x801008B8
void efManagerQuakeFuncRun(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    gcAddGObjProcess(effect_gobj, efManagerQuakeProcUpdate, nGCProcessKindFunc, ep->effect_vars.quake.priority);

    effect_gobj->func_run = NULL;
}

// 0x801008F4

/* ---- efmanager.c:3812-3861 efManagerQuakeMakeEffect ----
 *
 * DIVERGES in four lines and nowhere else: the switch's four
 * lbRelocGetFileData calls, which read an AnimJoint table out of
 * llEFCommonEffects1 at a fixed offset. The port has neither the file
 * nor the offsets (see the reloc symbols above), so each becomes
 * efQuakeAnimJoint(magnitude), which is the same table built out of the
 * pack tools/export/ssb_effectexport.py baked those very four blocks into --
 * one AObjEvent32* per joint, and the quake has one joint.
 */

/* The AObjEvent32** the game reads out of the file, built from the pack
 * the way src/dc/ifcommon.c builds the off-screen arrows' -- the pack
 * carries a script's position as a word index because a baked pointer
 * would not survive the trip, and src/dc/fighter.c turns the indices
 * inside the words into addresses at load. */
static AObjEvent32 *sEFManagerQuakeAnimJoint[1];

static AObjEvent32 **efQuakeAnimJoint(s32 magnitude)
{
    sEFManagerQuakeAnimJoint[0] = NULL;

#ifndef FT_HOSTTEST
    if (!sEFManagerQuakeIsLoaded)
    {
        int pal_bank = 0;
        Fighter *pack = malloc(sizeof(Fighter));

        if (pack == NULL)
        {
            return sEFManagerQuakeAnimJoint;
        }
        if (fighter_load(pack, "efquake.mdl", &pal_bank) != 0)
        {
            free(pack);
            return sEFManagerQuakeAnimJoint;
        }
        sEFManagerQuakePack = pack;
        sEFManagerQuakeIsLoaded = TRUE;
    }
    {
        const Fighter *f = sEFManagerQuakePack;
        const FPackAnim *anim;
        const s32 *entries;

        if ((magnitude < 0) || (magnitude >= (s32)f->hd->anim_count))
        {
            return sEFManagerQuakeAnimJoint;
        }
        anim = &f->anims[magnitude];
        entries = (const s32 *)((const u8 *)f->blob + anim->off_entries);

        if ((entries[0] >= 0) && ((u32)entries[0] < anim->nwords))
        {
            sEFManagerQuakeAnimJoint[0] =
                (AObjEvent32 *)((u8 *)f->blob + anim->off_words) + entries[0];
        }
    }
#else
    (void)magnitude;
#endif
    return sEFManagerQuakeAnimJoint;
}

/* Every model effect this port has a pack for, and the quake's.
 *
 * The preload, and why (DIVERGES). On the N64 none of this is a load
 * at all: the models live in relocData files the battle has loaded
 * before the first tic -- the common effect and item files, each
 * fighter's Special files -- and making one is pointer arithmetic. The
 * port bakes a pack per model and used to read each off the disc the
 * first time it was made, in the middle of a match: a pause on the game
 * thread while the drive seeks, and the read path behind the Poke Ball
 * freeze (src/dc/assetroot.c asset_read). scVSBattleStartBattle calls
 * this beside the rest of the battle's loading, so a match reads
 * nothing; what is loaded is kept for the run, as it always was. Every
 * row, not the ones this roster and this stage can reach: the set is
 * small, and a census would be one more thing to fall out of date. */
void efManagerPreloadModels(void)
{
#ifndef FT_HOSTTEST
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sEFManagerModels); i++)
    {
        (void)efModelLoad(&sEFManagerModels[i]);
    }
    (void)efQuakeAnimJoint(0);
#endif
}

// 0x801008F4
GObj* efManagerQuakeMakeEffect(s32 magnitude)
{
    s32 unused[2];
    EFStruct *ep;
    GObj *effect_gobj;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, efManagerQuakeFuncRun, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    effect_gobj->user_data.p = ep;

    gcAddXObjForDObjFixed(gcAddDObjForGObj(effect_gobj, NULL), nGCMatrixKindTra, 0);

    switch (magnitude)
    {
    case 0:
        gcAddAnimJointAll(effect_gobj, efQuakeAnimJoint(0), 0.0F);
        break;

    case 1:
        gcAddAnimJointAll(effect_gobj, efQuakeAnimJoint(1), 0.0F);
        break;

    case 2:
        gcAddAnimJointAll(effect_gobj, efQuakeAnimJoint(2), 0.0F);
        break;

    case 3:
        gcAddAnimJointAll(effect_gobj, efQuakeAnimJoint(3), 0.0F);
        break;

    default:
        break;
    }
    gcPlayAnimAll(effect_gobj);

    ep->effect_vars.quake.priority = 3 - magnitude;

    return effect_gobj;
}

// 0x80100A58
void efManagerDamageCoinProcDead(LBTransform *xf)
{
    Vec3f pos = xf->translate;

    pos.y += 200.0F;

    efManagerDustExpandSmallMakeEffect(&pos, 2.0F);
    efManagerSetPrevStructAlloc(efGetStruct(xf->effect_gobj));
    gcEjectGObj(xf->effect_gobj);
}

// 0x80100ACC
LBParticle* efManagerDamageCoinMakeEffect(Vec3f *pos)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;
    f32 angle;
    f32 vel;
    f32 scale;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }

    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = lbParticleMakeScriptID(gEFManagerParticleBankID, 0x60);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            xf->effect_gobj = effect_gobj;
            xf->proc_dead = efManagerDamageCoinProcDead;

            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            ep->effect_vars.common.xf = xf;

            xf->translate = *pos;
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}

// 0x80100BF0
LBParticle* efManagerSetOffMakeEffect(Vec3f *pos, s32 size)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;
    f32 vel;
    f32 angle;
    f32 scale;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0x65);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            xf->effect_gobj = effect_gobj;
            xf->proc_dead = efManagerDefaultProcDead;

            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            gcAddGObjProcess(effect_gobj, efManagerDefaultProcUpdate, nGCProcessKindFunc, 3);

            ep->effect_vars.common.xf = xf;

            xf->translate = *pos;

            vel = (syUtilsRandFloat() * 18.0F) + 12.0F;
            angle = syUtilsRandFloat() * F_CLC_DTOR32(360.0F); // F_CLC_DTOR32(360.0F)

            ep->effect_vars.common.vel.x = __cosf(angle) * vel;
            ep->effect_vars.common.vel.y = __sinf(angle) * vel;

            scale = (size < 10) ? (((10 - size) * -0.05F) + 1.0F) : (((size - 10) * 0.15F) + 1.0F);

            xf->scale.x = xf->scale.y = xf->scale.z = scale;
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}

/* ---- efmanager.c:4028-4092 efManagerFoxReflectorSetAnimID,
 * efManagerFoxReflectorProcUpdate, efManagerFoxReflectorMakeEffect
 * ) ----
 *
 * The hexagon follows Fox's TopN (user_data) and is told which animation
 * to play through effect_vars.reflector.status, which ftFoxSpecialLwUpdate-
 * Effect writes from the fighter's motion flag2 each tic and this proc
 * consumes (4 means "no change"); the end animation (3) ejects it. The
 * model is efreflector.mdl, its four animations side by side;
 * SetAnimID's one DIVERGES says so. ---- */
// 0x80100E84
void efManagerFoxReflectorSetAnimID(GObj *effect_gobj, s32 anim_id)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    ep->effect_vars.reflector.index = anim_id;

    /* DIVERGES: the game reads the anim_id'th of
     * dEFManagerFoxReflectorAnimJointOffsets out of gFTDataFoxSpecial2;
     * efreflector.mdl carries the four side by side in that order, so it
     * is an index into the pack. */
    gcAddAnimJointAll(effect_gobj, efModelAnimJoint(dc_model_of(effect_gobj), anim_id), 0.0F);
    gcPlayAnimAll(effect_gobj);
}

// 0x80100ED4
void efManagerFoxReflectorProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    gcPlayAnimAll(effect_gobj);

    if (effect_gobj->anim_frame <= 0.0F)
    {
        switch (ep->effect_vars.reflector.index)
        {
        case 1:
            break;

        case 0:
        case 2:
            efManagerFoxReflectorSetAnimID(effect_gobj, 1);
            break;

        case 3:
            efManagerSetPrevStructAlloc(ep);
            gcEjectGObj(effect_gobj);
            return;
        }
    }
    if (ep->effect_vars.reflector.status != 4)
    {
        efManagerFoxReflectorSetAnimID(effect_gobj, ep->effect_vars.reflector.status);

        ep->effect_vars.reflector.status = 4;
    }
}

// 0x80100FA4
GObj* efManagerFoxReflectorMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj = efManagerMakeEffectForce(&dEFManagerFoxReflectorEffectDesc);
    EFStruct *ep;

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->fighter_gobj = fighter_gobj;

    DObjGetStruct(effect_gobj)->user_data.p = ftGetStruct(fighter_gobj)->joints[nFTPartsJointTopN];

    ep->effect_vars.reflector.index = 0;
    ep->effect_vars.reflector.status = 4;

    return effect_gobj;
}

/* ---- The port's own, before efmanager.c:4094: what the shield's two
 * display procs do with their colours and their draw.
 *
 * The game's procs write colours into the translucent head with
 * gDPSetPrimColor and gDPSetEnvColor and let the model's display list
 * read them; here the model is a pack shared by every instance of the
 * effect, so the colours go onto that pack as its two live overrides
 * (src/dc/fighter.h fighter_set_prim_color, fighter_set_env_color) just
 * before its draw, which is the same moment. The draw itself is
 * dc_model_proc_display, the port of every effect's
 * gcDrawDObjTreeDLLinksForGObj (src/dc/objdisplay.c) -- for the egg,
 * whose game proc draws its one DObj through gcDrawDObjDLHead1, it is
 * named outright.
 *
 * The host cross-test has no pack and no renderer, so there the colours
 * land in two words it can read back, and the draw is nothing. ---- */
#ifdef FT_HOSTTEST
u32 gEFManagerShieldLastPrim;
u32 gEFManagerShieldLastEnv;
#endif

static void efManagerShieldModelColors(GObj *effect_gobj, const SYColorRGBPair *colors, u8 alpha)
{
    u32 prim = ((u32)colors->prim.r << 24) | ((u32)colors->prim.g << 16) | ((u32)colors->prim.b << 8) | alpha;
    u32 env = ((u32)colors->env.r << 24) | ((u32)colors->env.g << 16) | ((u32)colors->env.b << 8) | alpha;
#ifndef FT_HOSTTEST
    DObj *root = DObjGetStruct(effect_gobj);

    if ((root != NULL) && (root->dv != NULL))
    {
        Fighter *f = dc_model_of(effect_gobj);

        fighter_set_prim_color(f, prim);
        fighter_set_env_color(f, env);
    }
#else
    (void)effect_gobj;
    gEFManagerShieldLastPrim = prim;
    gEFManagerShieldLastEnv = env;
#endif
}

static void efManagerShieldModelEnv(GObj *effect_gobj, u8 r, u8 g, u8 b, u8 alpha)
{
    u32 env = ((u32)r << 24) | ((u32)g << 16) | ((u32)b << 8) | alpha;
#ifndef FT_HOSTTEST
    DObj *root = DObjGetStruct(effect_gobj);

    if ((root != NULL) && (root->dv != NULL))
    {
        fighter_set_env_color(dc_model_of(effect_gobj), env);
    }
#else
    (void)effect_gobj;
    gEFManagerShieldLastEnv = env;
#endif
}

static void efManagerShieldModelDraw(GObj *effect_gobj)
{
#ifndef FT_HOSTTEST
    dc_model_proc_display(effect_gobj);
#else
    (void)effect_gobj;
#endif
}

/* ---- efmanager.c:4094-4199: the shield. ----
 *
 * efManagerShieldMakeEffect is what ftCommonGuardOnSetStatus calls for
 * every fighter but Yoshi (src/dc/ftcommon.c): an effect on
 * dEFManagerShieldEffectDesc whose DObj's user_data is the fighter's
 * YRotN, so that matrix kind 0x4F puts the bubble at that joint's world
 * matrix -- scale and all, and the scale is the shield's size,
 * ftCommonGuardUpdateShieldCollision's -- and kind 0x2C turns the disc
 * to face the camera (src/dc/objdisplay.c). It sets is_effect_attach,
 * which is how the fighter tears it down: ftParamProcStopEffect ejects
 * every effect whose fighter_gobj is his when a status that does not
 * preserve effects starts (ftMainSetStatus), which for the guard is Wait
 * after GuardOff, or the jump. efManagerShieldProcUpdate clears the
 * damage flag the frame after ftCommonGuardSetOffSetStatus set it, so a
 * blocked hit shows one frame of colour row 4. efManagerYoshiShieldMake-
 * Effect is the egg, the same effect on Yoshi's descriptor at scale
 * 1.5. The two display procs are just above, marked. ---- */
// 0x80101008
void efManagerShieldProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    if (ep->effect_vars.shield.is_damage_shield != FALSE)
    {
        ep->effect_vars.shield.is_damage_shield = FALSE;
    }
}

// 0x80101024
void efManagerShieldProcDisplay(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    s32 id = (ep->effect_vars.shield.is_damage_shield != FALSE) ? 4 : ep->effect_vars.shield.player;

    /* DIVERGES: efmanager.c:4111-4113's gDPPipeSync, gDPSetPrimColor and
     * gDPSetEnvColor -- both colours at alpha 0xC0, into the translucent
     * head ahead of the model -- become the two live colours on the
     * pack (efManagerShieldModelColors); the draw is the game's own. */
    efManagerShieldModelColors(effect_gobj, &dEFManagerShieldColors[id], 0xC0);

    /* DIVERGES: efManagerShieldModelDraw (dc_model_proc_display), not the
     * decomp's gcDrawDObjTreeDLLinksForGObj -- the same gap
     * src/dc/itdisplay.c's header describes for items, and the sibling
     * Yoshi shield below already avoids it. */
    efManagerShieldModelDraw(effect_gobj);
}

// 0x80101108
GObj* efManagerShieldMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj;
    EFStruct *ep;
    FTStruct *fp;

    fp = ftGetStruct(fighter_gobj);

    effect_gobj = efManagerMakeEffectForce(&dEFManagerShieldEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->fighter_gobj = fighter_gobj;

    fp->is_effect_attach = TRUE;

    DObjGetStruct(effect_gobj)->user_data.p = fp->joints[nFTPartsJointYRotN];

    ep->effect_vars.shield.player = fp->player;
    ep->effect_vars.shield.is_damage_shield = FALSE;

    return effect_gobj;
}

// 0x80101180
void efManagerYoshiShieldProcDisplay(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    FTStruct *fp = ftGetStruct(ep->fighter_gobj);
    f32 blend = 1.0F - (fp->shield_health / 55.0F);
    u8 color[3];

    if (blend < 0.0F)
    {
        blend = 0.0F;
    }
    color[0] = 0xAE * blend;
    color[1] = 0xD6 * blend;
    color[2] = 0xD6 * blend;

    /* DIVERGES: efmanager.c:4164-4165's gDPPipeSync and gDPSetEnvColor
     * become the live environment colour on the pack, and 4167-4169's
     * gcDrawDObjDLHead1 and efDisplayCLDProcDisplay -- the one DObj's
     * list into head 1, and the cloud render mode that head's display
     * GObj sets ahead of it -- become the port's model draw, whose
     * render state the pack baked out of the list itself
     * (tools/export/ssb_effectexport.py --what yoshiegg). The colour is set
     * and not seen: the egg's combiner is TEXEL0 * (SHADE - ENV), a
     * subtraction the PVR's one multiply and one add cannot do, so the
     * port draws TEXEL0 * SHADE and the egg does not darken as the
     * shield weakens. */
    efManagerShieldModelEnv(effect_gobj, color[0], color[1], color[2], 0x00);
    efManagerShieldModelDraw(effect_gobj);
}

// 0x80101374
GObj* efManagerYoshiShieldMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj;
    EFStruct *ep;
    FTStruct *fp;

    fp = ftGetStruct(fighter_gobj);

    effect_gobj = efManagerMakeEffectForce(&dEFManagerYoshiShieldEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->fighter_gobj = fighter_gobj;

    fp->is_effect_attach = TRUE;

    DObjGetStruct(effect_gobj)->user_data.p = fp->joints[nFTPartsJointYRotN];
    DObjGetStruct(effect_gobj)->scale.vec.f.x = DObjGetStruct(effect_gobj)->scale.vec.f.y = 1.5F;

    ep->effect_vars.shield.player = fp->player;
    ep->effect_vars.shield.is_damage_shield = FALSE;

    return effect_gobj;
}

/* ---- efmanager.c:5426-5455 efManagerYoshiEggEscapeMakeEffect
 * 101): the fighter hatching back out of its own egg. Its ProcRender is
 * the Yoshi shield's own above, unverified whether that is meant to
 * shade with shield_health or is just what the decomp reused; the port
 * reuses it the same way. joints[5] is the decomp's own literal index,
 * not nFTPartsJointYRotN (3) the shield above names -- ported as
 * written, not normalised to a label the original didn't use. ---- */
// 0x80103150
GObj* efManagerYoshiEggEscapeMakeEffect(GObj *fighter_gobj)
{
    FTStruct *fp;
    EFStruct *ep;
    GObj *effect_gobj;
    DObj *dobj;

    fp = ftGetStruct(fighter_gobj);

    effect_gobj = efManagerMakeEffectForce(&dEFManagerYoshiEggEscapeEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ftParamHideModelPartAll(fighter_gobj);

    fp->is_effect_attach = TRUE;

    ep = efGetStruct(effect_gobj);

    ep->fighter_gobj = fighter_gobj;

    dobj = DObjGetStruct(effect_gobj);

    dobj->scale.vec.f.x = dobj->scale.vec.f.y = 1.5F;

    dobj->user_data.p = ftGetStruct(fighter_gobj)->joints[5];

    return effect_gobj;
}

/* ---- efmanager.c:4644-4673 efManagerCaptainFalconKickMakeEffect
 * ----
 *
 * The dust trail attaches to joint 23 (Captain's kicking foot) and tilts
 * further for the aerial version (nFTCaptainStatusSpecialAirLw), matching
 * the extra rotate.z the ground version leaves at zero. No DIVERGES. ---- */
// 0x80101ED8
GObj* efManagerCaptainFalconKickMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj;
    EFStruct *ep;
    FTStruct *fp;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectForce(&dEFManagerCaptainFalconKickEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->fighter_gobj = fighter_gobj;

    fp = ftGetStruct(fighter_gobj);
    dobj = DObjGetStruct(effect_gobj);

    dobj->user_data.p = ftGetStruct(fighter_gobj)->joints[23];

    dobj->rotate.vec.f.y = fp->lr * F_CLC_DTOR32(90.0F);

    if (fp->status_id == nFTCaptainStatusSpecialAirLw)
    {
        dobj->rotate.vec.f.z = -fp->lr * F_CLC_DTOR32(60.0F);
    }
    return effect_gobj;
}

/* ---- efmanager.c:4676-4704 efManagerCaptainFalconPunchMakeEffect
 * ----
 *
 * The flame attaches to joint 16 for Captain/NCaptain (his own fist) or
 * joint 30 for anyone else reachable through this maker (Kirby's copy
 * ability, per the decomp, ftkirbycopycaptainspecialn.c). No DIVERGES. ---- */
// 0x80101F70
GObj* efManagerCaptainFalconPunchMakeEffect(GObj *fighter_gobj)
{
    FTStruct *fp;
    EFStruct *ep;
    DObj *dobj, *joint;
    GObj *effect_gobj;

    effect_gobj = efManagerMakeEffectForce(&dEFManagerCaptainFalconPunchEffectDesc);

    if (effect_gobj == NULL)
    {
        return FALSE;
    }
    ep = efGetStruct(effect_gobj);

    ep->fighter_gobj = fighter_gobj;

    fp = ftGetStruct(fighter_gobj);

    dobj = DObjGetStruct(effect_gobj);

    joint = ((fp->fkind == nFTKindCaptain) || (fp->fkind == nFTKindNCaptain)) ? fp->joints[16] : fp->joints[30];

    dobj->user_data.p = joint;

    dobj->rotate.vec.f.y = fp->lr * F_CLC_DTOR32(-90.0F);

    return effect_gobj;
}

/* ---- efmanager.c:4721-4732 efManagerStarSplashMakeEffect --
 *
 * The puff of stars where a spat-out fighter lands. Two generator ids,
 * 0x10 and 0x11, one per facing, both out of gEFManagerParticleBankID --
 * the efcommon bank the port loads at src/dc/efdisplay.c:150 -- so unlike
 * Kirby's inhale wind and his two star effects (see the header above)
 * this one needs no
 * per-fighter bank and no model, and is taken verbatim. tools/
 * lbparticle_check.py already runs both scripts against their original.
 * Called from ft/ftcommon/ftcommoncapturekirby.c. ---- */
LBGenerator* efManagerStarSplashMakeEffect(Vec3f *pos, s32 lr)
{
    LBGenerator *gn = (lr == -1) ? lbParticleMakeGenerator(gEFManagerParticleBankID, 0x10) : lbParticleMakeGenerator(gEFManagerParticleBankID, 0x11);

    if (gn != NULL)
    {
        gn->pos.x = pos->x;
        gn->pos.y = pos->y;
        gn->pos.z = pos->z;
    }
    return gn;
}

/* ---- efmanager.c:4735-4774 efManagerPurinSingMakeEffect ----
 *
 * Sing's note cloud: a stand at Purin's TopN joint (matrix kind 0x46) with
 * a rotating child (RotRpyR) carrying three notes (matrix kind 0x2A each),
 * every gcAddXObjForDObjFixed call the decomp's own (sys/objman.c, compiled
 * unmodified into this port since HOST_DECOMP_SRCS's earliest days). No
 * DIVERGES at all. ---- */
// 0x801020F4
GObj* efManagerPurinSingMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj;
    DObj *dobj, *sibling_dobj;
    EFStruct *ep;

    effect_gobj = efManagerMakeEffectForce(&dEFManagerPurinSingEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);
    ep->fighter_gobj = fighter_gobj;

    dobj = DObjGetStruct(effect_gobj);
    dobj->user_data.p = ftGetStruct(fighter_gobj)->joints[nFTPartsJointTopN];

    sibling_dobj = dobj->child;

    gcAddXObjForDObjFixed(sibling_dobj, 0x46, 0);

    sibling_dobj = dobj->child->sib_next;

    gcAddXObjForDObjFixed(sibling_dobj, nGCMatrixKindRotRpyR, 0);

    sibling_dobj = sibling_dobj->child;

    gcAddXObjForDObjFixed(sibling_dobj, 0x2A, 0);

    sibling_dobj = sibling_dobj->sib_next;

    gcAddXObjForDObjFixed(sibling_dobj, 0x2A, 0);

    sibling_dobj = sibling_dobj->sib_next;

    gcAddXObjForDObjFixed(sibling_dobj, 0x2A, 0);

    return effect_gobj;
}

/* ---- efmanager.c:4371-4406 func_ovl2_801017E8 and func_ovl2_8010183C,
 * dEFManagerPikachuUnkEffectDesc's proc_update and maker. Both unnamed in the decomp; kept verbatim, address and all, the
 * same as every other unnamed function this port carries across.
 * func_ovl2_8010183C is nEFKindCrashTheGame's own maker
 * (ftparam.c:2055-2056) -- always refused here, since no
 * sEFManagerModels row exists for its EFDesc. ---- */
// 0x801017E8
void func_ovl2_801017E8(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    gcPlayAnimAll(effect_gobj);

    if (ep->effect_vars.unknown1.efvars_unk1_0x0 == 0)
    {
        efManagerSetPrevStructAlloc(ep);
        gcEjectGObj(effect_gobj);
    }
    else ep->effect_vars.unknown1.efvars_unk1_0x0--;
}

// 0x8010183C
GObj* func_ovl2_8010183C(Vec3f *pos, s32 arg1)
{
    GObj *effect_gobj;
    EFStruct *ep;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerPikachuUnkEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->effect_vars.unknown1.efvars_unk1_0x0 = arg1;

    dobj = DObjGetStruct(effect_gobj);
    dobj->translate.vec.f = *pos;

    return effect_gobj;
}

/* ---- efmanager.c:4461-4485 efManagerPikachuThunderTrailProcUpdate
 * ----
 *
 * The trail spark's own update: count the lifetime down, eject when it
 * runs out, and until then flicker between the first three textures --
 * except on the last tic, which takes the fourth and turns the sprite
 * upside down. Verbatim. ---- */
// 0x80101A08
void efManagerPikachuThunderTrailProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    if (ep->effect_vars.thunder_trail.lifetime == 0)
    {
        efManagerSetPrevStructAlloc(ep);
        gcEjectGObj(effect_gobj);

        return;
    }
    else ep->effect_vars.thunder_trail.lifetime--;

    if (DObjGetStruct(effect_gobj)->mobj->texture_id_curr != 3)
    {
        if (ep->effect_vars.thunder_trail.lifetime == 0)
        {
            DObjGetStruct(effect_gobj)->mobj->texture_id_curr = 3;

            DObjGetStruct(effect_gobj)->rotate.vec.f.z = F_CLC_DTOR32(180.0F);
        }
        else DObjGetStruct(effect_gobj)->mobj->texture_id_curr = syUtilsRandIntRange(3);
    }
}

/* ---- efmanager.c:4487-4503 efManagerPikachuThunderTrailProcDisplay
 * ----
 *
 * DIVERGES, six lines dropped and none added -- the same shape
 * efManagerShieldProcDisplay and efManagerYoshiShieldProcDisplay have
 * had since  5. The game brackets its draw with a pipe sync, a
 * translucent render mode with alpha compare off, and then puts the
 * common cloud mode and threshold back; the PVR takes its blend and its
 * alpha test off the pack's own bucket, so the six gDP lines have
 * nothing to write to and the draw between them stands alone.
 * ---- */
// 0x80101AA8
void efManagerPikachuThunderTrailProcDisplay(GObj *effect_gobj)
{
    gcDrawDObjDLLinksForGObj(effect_gobj);
}

/* ---- efmanager.c:4505-4533 efManagerPikachuThunderTrailMakeEffect
 * ----
 *
 * Verbatim. Two callers, both in wp/wppikachu/wppikachuthunder.c's
 * wpPikachuThunderHeadMakeTrailEffect: the bolt's head asks for a
 * ten-tic spark on the fourth texture when it lands or expires, and a
 * spent trail weapon asks for a six-tic one on the first. Neither
 * caller reads the result. ---- */
// 0x80101B88
GObj* efManagerPikachuThunderTrailMakeEffect(Vec3f *pos, s32 lifetime, s32 texture_index)
{
    GObj *effect_gobj;
    EFStruct *ep;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerPikachuThunderTrailEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    dobj->scale.vec.f.x = 0.5F;
    dobj->scale.vec.f.y = 0.5F;
    dobj->scale.vec.f.z = 0.5F;

    ep = efGetStruct(effect_gobj);

    ep->effect_vars.thunder_trail.lifetime = lifetime;

    dobj->mobj->texture_id_curr = (texture_index == 3) ? 3 : 0;

    return effect_gobj;
}

/* ---- efmanager.c:4536-4549 efManagerPikachuThunderJoltMakeEffect
 * ) ----
 *
 * Three lines and no DIVERGES: place the spark where the jolt is and turn
 * it the way the jolt is turned. wpPikachuThunderJoltGroundProcUpdate is
 * its only caller, at exactly one frame of the crawler's animation, and
 * it passes DObjGetStruct(weapon_gobj)->rotate.vec.f.z -- which on a wall
 * is the angle syUtilsArcTan2 gave the wall's own normal, so the spark
 * lies along the surface the jolt is riding.
 * ---- */
// 0x80101C34
GObj* efManagerPikachuThunderJoltMakeEffect(Vec3f *pos, f32 rotate)
{
    GObj *effect_gobj = efManagerMakeEffectNoForce(&dEFManagerThunderJoltEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    DObjGetStruct(effect_gobj)->translate.vec.f = *pos;

    DObjGetStruct(effect_gobj)->rotate.vec.f.z = rotate;

    return effect_gobj;
}

/* ---- efmanager.c:5669-5696 efManagerMarioEntryDokanMakeEffect
 * ) ----
 *
 * Verbatim, including the file_head switch. There is nothing to choose
 * between: Luigi's FTData names llMarioSpecial2FileID for his Special2
 * (ft/ftdata.c, dFTLuigiData), so both pipes are the one file and the one
 * pack, "efdokan.mdl", serves both. ---- */
// 0x801036EC
GObj* efManagerMarioEntryDokanMakeEffect(Vec3f *pos, s32 fkind)
{
    GObj *effect_gobj;
    DObj *dobj;

    switch (fkind)
    {
    case nFTKindMario:
        dEFManagerMarioEntryDokanEffectDesc.file_head = &gFTMarioFileSpecial2;
        break;

    case nFTKindLuigi:
        dEFManagerMarioEntryDokanEffectDesc.file_head = &gFTDataLuigiSpecial2;
        break;
    }
    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerMarioEntryDokanEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    return effect_gobj;
}

/* ---- efmanager.c:5564-5581 efManagerDonkeyEntryTaruMakeEffect and
 * :5583-5600 efManagerSamusEntryPointMakeEffect ----
 *
 * Both verbatim, and both are the pipe's maker without even its file
 * switch: make the effect, put it where the fighter is, hand it back.
 * That is why these three vehicles came before the other seven. ---- */
// 0x80103418
GObj* efManagerDonkeyEntryTaruMakeEffect(Vec3f *pos)
{
    GObj *effect_gobj;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerDonkeyEntryTaruEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    return effect_gobj;
}

// 0x80103474
GObj* efManagerSamusEntryPointMakeEffect(Vec3f *pos)
{
    GObj *effect_gobj;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerSamusEntryPointEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    return effect_gobj;
}

/* ---- efmanager.c:5189-5208 efManagerKirbyEntryStarMakeEffect
 * 82) ----
 *
 * DIVERGES, one line and one only, and it is the line
 * efManagerDeadExplodeMakeEffect diverges on for the same reason: the
 * game picks which of two animations to play by writing a BLOCK OFFSET
 * into the shared descriptor (`o_anim_joint = R or L`), and this port's
 * descriptor offsets are all zero because both blocks are baked side by
 * side into efstar.mdl. So the assignment becomes an INDEX --
 * sEFManagerAnimAlt, which efManagerAddModel takes and clears -- and R
 * is animation 0 because the packer bakes it first. ---- */
// 0x80102B90
GObj* efManagerKirbyEntryStarMakeEffect(Vec3f *pos, s32 lr)
{
    GObj *effect_gobj;
    DObj *dobj;

    /* DIVERGES: dEFManagerKirbyEntryStarEffectDesc.o_anim_joint =
     * (lr == +1) ? &llKirbySpecial2EntryStarRAnimJoint
     *            : &llKirbySpecial2EntryStarLAnimJoint (efmanager.c:5195) */
    sEFManagerAnimAlt = (lr == +1) ? 0 : 1;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerKirbyEntryStarEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    return effect_gobj;
}

/* ---- efmanager.c:5151-5168 efManagerLinkEntryWaveMakeEffect and
 * :5170-5187 efManagerLinkEntryBeamMakeEffect ----
 *
 * Both verbatim, and both the pipe's maker again: make the effect, put it
 * where the fighter is, hand it back. What is new is not in the makers
 * but in the packs -- an MObjSub and a MatAnimJoint apiece. ---- */
// 0x80102AE4
GObj* efManagerLinkEntryWaveMakeEffect(Vec3f *pos)
{
    GObj *effect_gobj;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerLinkEntryWaveEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    return effect_gobj;
}

// 0x80102B40
GObj* efManagerLinkEntryBeamMakeEffect(Vec3f *pos)
{
    GObj *effect_gobj;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerLinkEntryBeamEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    return effect_gobj;
}

/* ---- efmanager.c:5345-5361 efManagerYoshiEntryEggMakeEffect
 * 84) ----
 *
 * The pipe's maker for the seventh time, verbatim. ---- */
// 0x80102F34
GObj* efManagerYoshiEntryEggMakeEffect(Vec3f *pos)
{
    GObj *effect_gobj;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerYoshiEntryEggEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    return effect_gobj;
}

/* ---- efmanager.c:5699-5711 efManagerFoxEntryArwingProcUpdate and
 * :5714-5745 efManagerFoxEntryArwingMakeEffect ----
 *
 * The proc is the first vehicle proc in this file at all: the seven
 * before it use gcPlayAnimAll as their ProcUpdate and are ejected by the
 * effect manager when their animation ends. The Arwing ejects itself the
 * moment its animation runs its frame counter down, and otherwise sorts
 * itself behind the stage.
 *
 * The maker is what made these two vehicles "held up by their makers"
 * since  80. Three things happen in it that no earlier maker
 * does, and only one survives into the port unchanged.
 *
 * 1. It reaches SEVEN SIBLINGS DEEP -- dobj->child->child->child, then
 *    six sib_nexts, then one more child -- to find one node, which is
 *    index 10 of the twelve.
 *
 * 2. It gives that node a gcAddXObjForDObjFixed(what, 0x2C, 0), matrix
 *    kind 44, nGCMatrixKindRecalcRotRpyRSca -- the RSP billboard, which
 *    src/dc/objdisplay.c:640-665 draws this for the shield bubble and
 *    Yoshi's egg. That is the one thing here taken verbatim.
 *
 * 3. It then puts an AnimJoint on that same node out of Special3, at
 *    llFoxSpecial3_2E74_AnimJoint -- AND THAT LINE IS DEAD IN THE
 *    SHIPPED GAME. gcAddDObjAnimJoint REPLACES dobj->anim_joint
 *    (sys/objanim.c:137-149); lbCommonAddDObjAnimJointAll walks the tree
 *    in the same depth-first order this file's reader numbers it
 *    (lb/lbcommon.c:751-805), so array slot 10 is that same node; and
 *    both arrays' slot 10 is non-NULL -- 0x0B98 going right, 0x0748
 *    going left. The array's script overwrites the one the maker set,
 *    every time. The XObj it needed stays; the script does not. This is
 *     69's ftPikachuSpecialLwProcDamage again: a line the game
 *    runs and immediately undoes.
 *
 * DIVERGES, and the biggest of the three reasons is structural and is
 * worth stating once for every vehicle.
 *
 * THE PORT'S EFFECT TREE IS ONE DObj SHALLOWER THAN THE GAME'S.
 * efmanager.c:1986 makes an empty DObj for the GObj and hangs the
 * DObjDesc tree UNDER it, so DObjGetStruct(effect_gobj) is that empty
 * stand and dobj->child is the tree's own root. Here the tree is the
 * pack's joints and dc_model_add_dobjs puts joint 0 straight on the
 * GObj, so DObjGetStruct(effect_gobj) IS the tree's root and there is no
 * stand. Nothing noticed for seven vehicles because every one of their
 * makers only writes dobj->translate, and every one of these packs has a
 * joint 0 whose bind translate is zero, so writing the root is writing
 * the stand. The Arwing is the first maker that WALKS the tree, and for
 * it the difference is one level: every dobj->child below is the
 * decomp's dobj->child->child. (pack_dexp does keep the stand as its
 * joint 0, which is why efManagerDeadExplodeMakeEffect's dobj->child is
 * verbatim. The two packers disagree; making pack_efentry agree would
 * re-shape seven packs and is its own step.)
 *
 * The other two: the dead gcAddDObjAnimJoint is dropped with the reason
 * above rather than copied, and the two lbRelocGetFileData reads become
 * one index into the pack, as Kirby's star's do, because both arrays are
 * baked into it. ---- */

/* The port's own, and the two lines the maker below has instead of the
 * decomp's six.
 *
 * The walk is efmanager.c:5727's one expression, a level shallower per
 * the note above, with every link checked -- because the host
 * cross-test's mock tree is a stand and three children, efManagerAddModel
 * says so, and the seventh sibling is not there to be found. On the
 * target it finds node 10 of the twelve. */
static void efManagerArwingBillboard(DObj *dobj)
{
    s32 i;

    if ((dobj == NULL) || (dobj->child == NULL) ||
        (dobj->child->child == NULL))
    {
        return;
    }
    dobj = dobj->child->child;

    for (i = 0; i < 6; i++)
    {
        if (dobj->sib_next == NULL)
        {
            return;
        }
        dobj = dobj->sib_next;
    }
    if (dobj->child != NULL)
    {
        gcAddXObjForDObjFixed(dobj->child, 0x2C, 0);
    }
}

/* And the animation. The game reads one of two AnimJoint arrays out of
 * Fox's Special2 by which way he flies in; both are baked into the one
 * pack, so this is an index -- Kirby's star's answer, and
 * efModelAnimJoint is the same lookup efManagerAddModel uses for the
 * seven vehicles whose descriptor names an AnimJoint. It starts at
 * `dobj` and not at dobj->child for the same reason the walk above is a
 * level shallower: slot 0 of the array is the tree's root, and here the
 * tree's root is the effect's root. */
static void efManagerArwingAnim(GObj *effect_gobj, DObj *dobj, s32 lr)
{
    lbCommonAddDObjAnimJointAll(dobj,
                                efModelAnimJoint(dc_model_of(effect_gobj),
                                                 (lr == +1) ? 0 : 1),
                                0.0F);
}

// 0x80103780
void efManagerFoxEntryArwingProcUpdate(GObj *effect_gobj)
{
    DObj *dobj = DObjGetStruct(effect_gobj);

    gcPlayAnimAll(effect_gobj);

    if (dobj->anim_frame <= 0.0F)
    {
        efManagerSetPrevStructAlloc(efGetStruct(effect_gobj));
        gcEjectGObj(effect_gobj);
    }
    else efManagerSortZNeg(dobj);
}

// 0x801037EC
GObj* efManagerFoxEntryArwingMakeEffect(Vec3f *pos, s32 lr)
{
    GObj *effect_gobj;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerFoxEntryArwingEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);
    efManagerArwingBillboard(dobj);
    efManagerArwingAnim(effect_gobj, dobj, lr);

    gcPlayAnimAll(effect_gobj);

    dobj->translate.vec.f = *pos;

    efManagerSortZNeg(dobj);

    return effect_gobj;
}

/* ---- efmanager.c:5603-5620 efManagerCaptainEntryCarProcUpdate and
 * :5622-5667 efManagerCaptainEntryCarMakeEffect ----
 *
 * The last of the nine entry arms. The proc is the Arwing's with one
 * more question: the car is sorted behind the stage when it drives in
 * facing +Z and in front of it when it has been turned around, and the
 * decomp's own comment on that test ("This could mean trouble if the
 * macro is changed... Need different zero literals") is kept.
 *
 * Both come across with THEIR TREE WALKS UNCHANGED. The port's effect
 * tree is one DObj shallower than the game's. The reason is
 * EFDesc flag 0x1: with it, efmanager.c:1986-2004 makes an empty DObj
 * for the GObj and hangs the DObjDesc tree under it; without it,
 * efmanager.c:2017-2043 calls gcSetupCustomDObjs and the tree goes
 * straight on the GObj. This is the one vehicle without 0x1, so the
 * game's tree and the port's are the same tree, and dobj->child->child->
 * child is the same node in both. The Arwing needed the level taken off
 * because it HAS 0x1. That is the whole rule, and it is worth having
 * before anything else in this file walks a tree.
 *
 * DIVERGES in the animation, and only there. The game applies one
 * AnimJoint array over the tree and then overwrites eight of its entries
 * by hand -- four passes of two siblings, the odd one getting the wheel
 * script at 0x6518 and the even one the script at 0x6598. Because
 * gcAddDObjAnimJoint replaces rather than appends (sys/objanim.c:137-149)
 * and the maker writes last, those eight writes are what the game plays,
 * so the pack bakes the array WITH THEM ALREADY IN IT
 * (EFENTRY_ANIMPATCH in tools/export/ssb_effectexport.py) and the port applies
 * one animation. This is the mirror image of the Arwing's dead extra
 * AnimJoint, which the array overwrote: there the maker wrote first and
 * lost, here it writes last and wins.
 *
 * What the loop still does is the matrix: kind 44,
 * nGCMatrixKindRecalcRotRpyRSca, on the four odd siblings. That is not
 * animation and the pack cannot carry it. ---- */
// 0x801034D0
void efManagerCaptainEntryCarProcUpdate(GObj *effect_gobj)
{
    DObj *dobj = DObjGetStruct(effect_gobj)->child;

    gcPlayAnimAll(effect_gobj);

    if (dobj->anim_frame <= 0.0F)
    {
        efManagerSetPrevStructAlloc(efGetStruct(effect_gobj));
        gcEjectGObj(effect_gobj);
    }
    else if (DObjGetStruct(effect_gobj)->rotate.vec.f.y == F_CLC_DTOR32(0.0F)) // This could mean trouble if the macro is changed... Need different zero literals
    {
        efManagerSortZNeg(dobj);
    }
    else efManagerSortZPos(dobj);
}

/* The port's own: efmanager.c:5640-5651's loop with the two
 * gcAddDObjAnimJoint calls taken out (the pack plays them) and every
 * link checked, because the host cross-test's mock tree is a stand and
 * three children and dobj->child->child->child is not there to be
 * found. On the target it walks the eight siblings under node 2. */
static void efManagerCarWheelXObjs(DObj *dobj)
{
    DObj *node_dobj;
    s32 i;

    if ((dobj == NULL) || (dobj->child == NULL) ||
        (dobj->child->child == NULL))
    {
        return;
    }
    node_dobj = dobj->child->child->child;

    for (i = nFTPartsJointCommonStart; (i > 0) && (node_dobj != NULL); i--)
    {
        gcAddXObjForDObjFixed(node_dobj, nGCMatrixKindRecalcRotRpyRSca, 0);

        node_dobj = node_dobj->sib_next;

        if (node_dobj == NULL)
        {
            return;
        }
        node_dobj = node_dobj->sib_next;
    }
}

// 0x8010356C
GObj* efManagerCaptainEntryCarMakeEffect(Vec3f *pos, s32 lr)
{
    GObj *effect_gobj;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerCaptainEntryCarEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    gcAddAnimJointAll(effect_gobj, efModelAnimJoint(dc_model_of(effect_gobj), 0), 0.0F);

    efManagerCarWheelXObjs(dobj);

    gcPlayAnimAll(effect_gobj);

    dobj->translate.vec.f = *pos;

    if (lr == -1)
    {
        dobj->rotate.vec.f.y = F_CLC_DTOR32(180.0F);
    }
    if (DObjGetStruct(effect_gobj)->rotate.vec.f.y == F_CLC_DTOR32(0.0F))
    {
        efManagerSortZNeg(dobj->child);
    }
    else efManagerSortZPos(dobj->child);

    return effect_gobj;
}

/* ---- efmanager.c:5364-5371 efManagerYoshiEggLaySetAnim, :5374-5388
 * efManagerYoshiEggLayProcUpdate and :5391-5423
 * efManagerYoshiEggLayMakeEffect ----
 *
 * The egg a fighter caught by Yoshi's Egg Lay sits inside, and the last
 * of the two makers  59 stubbed. ft/ftcommon/ftcommoncaptureyoshi.c
 * has compiled unmodified since that step and has been driving
 * force_index at an effect that was not there; it is there now.
 *
 * DIVERGES, three ways and all three already named by earlier steps.
 *
 * 1. THE LEVEL. The descriptor's flags carry 0x1, so the game hangs the
 *    DObjDesc tree under an empty DObj and this port hangs it on the
 *    GObj. Every dobj->child below is the decomp's
 *    dobj->child->child.
 *
 * 2. THE ANIMATION INDEX. dEFManagerYoshiEggLayAnimJoints holds two
 *    block pointers in the game and two pack indexes here, because all
 *    three animations are baked into the one pack -- the star's answer
 *    at  82.
 *
 * 3. lbCommonSetDObjTransformsForTreeDObjs(dobj->child, the DObjDesc) is
 *    dropped. It re-reads the tree's own bind transforms out of the
 *    relocData file and writes them back over the DObjs, undoing
 *    whatever an earlier use of this shared effect left on them. The
 *    port has no DObjDesc at runtime; what it has is the pack, whose
 *    joint table IS those bind transforms, and dc_model_add_dobjs writes
 *    them on every make. So the line has already happened by the time
 *    the maker runs.
 *
 * What is NOT dropped is the pair above it. xobjs[0]->kind is forced to
 * Tra on the tree's second node -- the descriptor gave it
 * TraRotRpyRSca and the egg wants only the translation -- and 0x2E is
 * added beside it. Both are matrix kinds src/dc/objdisplay.c already
 * draws. ---- */
// 0x80102F90
void efManagerYoshiEggLaySetAnim(GObj *effect_gobj, s32 index)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    /* DIVERGES: the index -- the pack's, not a block pointer */
    s32 alt = dEFManagerYoshiEggLayAnimJoints[index];
    Fighter *pack = dc_model_of(effect_gobj);

    ep->effect_vars.yoshi_egg_lay.index = index;

    /* DIVERGES: the level -- DObjGetStruct is the decomp's ->child */
    lbCommonAddDObjAnimJointAll(DObjGetStruct(effect_gobj), efModelAnimJoint(pack, alt), 1.0F);
}

// 0x80102FE4
void efManagerYoshiEggLayProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    if (ep->effect_vars.yoshi_egg_lay.force_index != ep->effect_vars.yoshi_egg_lay.index)
    {
        efManagerYoshiEggLaySetAnim(effect_gobj, ep->effect_vars.yoshi_egg_lay.force_index);
    }
    gcPlayAnimAll(effect_gobj);

    if ((ep->effect_vars.yoshi_egg_lay.index == 2) && (effect_gobj->anim_frame <= 0.0F))
    {
        ep->effect_vars.yoshi_egg_lay.force_index = 0;
    }
}

// 0x80103060
GObj* efManagerYoshiEggLayMakeEffect(GObj *fighter_gobj)
{
    FTStruct *fp;
    EFStruct *ep;
    GObj *effect_gobj;
    DObj *dobj;

    fp = ftGetStruct(fighter_gobj);

    effect_gobj = efManagerMakeEffectForce(&dEFManagerYoshiEggLayEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);
    ep->fighter_gobj = fighter_gobj;
    ep->effect_vars.yoshi_egg_lay.index = ep->effect_vars.yoshi_egg_lay.force_index = 2;

    dobj = DObjGetStruct(effect_gobj);
    dobj->user_data.p = ftGetStruct(fighter_gobj)->joints[nFTPartsJointTopN];

    dobj->scale.vec.f.x = dobj->scale.vec.f.y = dFTCommonYoshiEggDamageCollDescs[fp->fkind].effect_size;
    dobj->scale.vec.f.z = 1.0F;

    /* DIVERGES: the level. And the fourth line of the decomp's four,
     * lbCommonSetDObjTransformsForTreeDObjs, is dropped -- see above. */
    if (dobj->child != NULL)
    {
        dobj->child->xobjs[0]->kind = nGCMatrixKindTra;

        gcAddXObjForDObjFixed(dobj->child, 0x2E, 0);
    }
    return effect_gobj;
}

/* ---- efmanager.c:4850-4949, the Final Cutter's four makers
 * 89), all four verbatim ----
 *
 * The last four stubs in this port that fail for want of a bake and are
 * not it/'s. Each makes its effect, hangs it off a joint of Kirby's
 * through user_data.p, and turns it to face the way he faces; the blade
 * is the one that does not turn, because it is drawn at joint 17 and its
 * transform kind is 0x4F rather than 0x50.
 *
 * ft/ftchar/ftkirby/ftkirbyspecialhi.c is compiled unmodified
 * and guards every one of these with `if (make(...) != NULL)`.
 * That guard also clears motion_vars.flags.flag2 INSIDE the if, so with
 * a NULL maker the flag stayed raised and the switch asked for the same
 * effect again every frame for as long as the status lasted -- a busier
 * no-op than the game's, which wrote down and which goes away
 * here. ---- */
// 0x80102418
GObj* efManagerKirbyCutterUpMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj;
    EFStruct *ep;
    FTStruct *fp;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectForce(&dEFManagerKirbyCutterUpEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);
    ep->fighter_gobj = fighter_gobj;

    fp = ftGetStruct(fighter_gobj);
    dobj = DObjGetStruct(effect_gobj);

    dobj->user_data.p = ftGetStruct(fighter_gobj)->joints[nFTPartsJointTopN];
    dobj->rotate.vec.f.y = fp->lr * F_CLC_DTOR32(90.0F);

    return effect_gobj;
}

// 0x80102490
GObj* efManagerKirbyCutterDownMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj;
    EFStruct *ep;
    FTStruct *fp;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectForce(&dEFManagerKirbyCutterDownEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);
    ep->fighter_gobj = fighter_gobj;

    fp = ftGetStruct(fighter_gobj);
    dobj = DObjGetStruct(effect_gobj);

    dobj->user_data.p = ftGetStruct(fighter_gobj)->joints[nFTPartsJointTopN];
    dobj->rotate.vec.f.y = fp->lr * F_CLC_DTOR32(90.0F);

    return effect_gobj;
}

// 0x80102508
GObj* efManagerKirbyCutterDrawMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj;
    EFStruct *ep;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectForce(&dEFManagerKirbyCutterDrawEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->fighter_gobj = fighter_gobj;

    dobj = DObjGetStruct(effect_gobj);

    dobj->user_data.p = ftGetStruct(fighter_gobj)->joints[17];

    return effect_gobj;
}

// 0x80102560
GObj* efManagerKirbyCutterTrailMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj;
    EFStruct *ep;
    FTStruct *fp;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectForce(&dEFManagerKirbyCutterTrailEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);
    ep->fighter_gobj = fighter_gobj;

    fp = ftGetStruct(fighter_gobj);
    dobj = DObjGetStruct(effect_gobj);

    dobj->user_data.p = ftGetStruct(fighter_gobj)->joints[17];
    dobj->rotate.vec.f.y = fp->lr * F_CLC_DTOR32(90.0F);

    return effect_gobj;
}

/* ---- efmanager.c:4552-4619 efManagerKirbyVulcanJabProcUpdate and
 * efManagerKirbyVulcanJabMakeEffect, both verbatim ----
 *
 * ft/ftcommon/ftcommonattack100.c makes one of these per jab frame with
 * a rotation, a velocity and an acceleration, and the proc walks it out
 * and ejects it when its life runs down. Kirby is the only fighter whose
 * rapid jab draws anything. ---- */
// 0x80101CA0
void efManagerKirbyVulcanJabProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    DObj *dobj = DObjGetStruct(effect_gobj);

    if (ep->effect_vars.vulcan_jab.lifetime != 0)
    {
        ep->effect_vars.vulcan_jab.vel.x += ep->effect_vars.vulcan_jab.add.x;
        dobj->translate.vec.f.x += ep->effect_vars.vulcan_jab.vel.x;

        ep->effect_vars.vulcan_jab.vel.y += ep->effect_vars.vulcan_jab.add.y;
        dobj->translate.vec.f.y += ep->effect_vars.vulcan_jab.vel.y;

        ep->effect_vars.vulcan_jab.lifetime--;
    }
    else
    {
        efManagerSetPrevStructAlloc(ep);
        gcEjectGObj(effect_gobj);
    }
}

// 0x80101D34
GObj* efManagerKirbyVulcanJabMakeEffect(Vec3f *pos, s32 lr, f32 rotate, f32 vel, f32 add)
{
    GObj *effect_gobj;
    DObj *dobj;
    EFStruct *ep;
    f32 sin, cos;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerVulcanJabEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);
    ep = efGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    if (lr == -1)
    {
        dobj->rotate.vec.f.y = F_CLC_DTOR32(180.0F);

        rotate = -rotate;
        vel = -vel;
        add = -add;
    }
    gcAddXObjForDObjFixed(dobj->child->child, 0x46, 0);

    dobj->rotate.vec.f.z = F_CLC_DTOR32(rotate);

    sin = lbCommonSin(dobj->rotate.vec.f.z);
    cos = lbCommonCos(dobj->rotate.vec.f.z);

    ep->effect_vars.vulcan_jab.lifetime = 6;

    ep->effect_vars.vulcan_jab.vel.x = vel * cos;
    ep->effect_vars.vulcan_jab.vel.y = vel * sin;
    ep->effect_vars.vulcan_jab.vel.z = 0.0F;

    ep->effect_vars.vulcan_jab.add.x = add * cos;
    ep->effect_vars.vulcan_jab.add.y = add * sin;
    ep->effect_vars.vulcan_jab.add.z = 0.0F;

    return effect_gobj;
}

/* ---- efmanager.c:5533-5562 efManagerLinkSpinAttackMakeEffect
 * 90), verbatim ----
 *
 * The Cutter's makers' shape with one thing more: it installs
 * ftParamProcPauseEffect and ftParamProcResumeEffect on the FIGHTER, so
 * the glow stops with him when a hit's lag freezes the frame. Both are
 * src/dc/ftparam.c's and both are real. ---- */
// 0x80103378
GObj* efManagerLinkSpinAttackMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj;
    EFStruct *ep;
    FTStruct *fp;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerLinkSpinAttackEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->fighter_gobj = fighter_gobj;

    fp = ftGetStruct(fighter_gobj);

    fp->proc_lagstart = ftParamProcPauseEffect;
    fp->proc_lagend = ftParamProcResumeEffect;

    dobj = DObjGetStruct(effect_gobj);

    dobj->user_data.p = fp->joints[nFTPartsJointTopN];

    dobj->rotate.vec.f.y = (ftGetStruct(fighter_gobj)->lr == +1) ? F_CLC_DTOR32(30.0F) : F_CLC_DTOR32(210.0F);

    return effect_gobj;
}

/* ---- efmanager.c:2772-2822 efManagerShockSmallMakeEffect
 * 102) ----
 *
 * DIVERGES: efmanager.c:2794-2811 computes `angle` and calls __cosf and
 * __sinf on it, then writes 0.0F over both results -- the decomp's own
 * comment above them says the DAIRANTOU_OPT0 build (the one the ROM
 * shipped) strips all of it as dead code, so the port drops the same
 * lines the shipped build never ran and keeps the two writes that
 * survive: vel.x and vel.y both 0.0F. Nothing downstream reads `angle`
 * either way. ---- */
// 0x800FEEB0
GObj* efManagerShockSmallMakeEffect(Vec3f *pos)
{
    GObj *effect_gobj;
    DObj *dobj;
    EFStruct *ep;
    f32 scale;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerShockSmallEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);
    ep = efGetStruct(effect_gobj);

    pos->x += (syUtilsRandFloat() * EFCOMMON_SHOCKSMALL_OFF_BASE) + EFCOMMON_SHOCKSMALL_OFF_ADD;
    pos->y += (syUtilsRandFloat() * EFCOMMON_SHOCKSMALL_OFF_BASE) + EFCOMMON_SHOCKSMALL_OFF_ADD;

    dobj->translate.vec.f = *pos;

    ep->effect_vars.common.vel.x = 0.0F;
    ep->effect_vars.common.vel.y = 0.0F;

    scale = (syUtilsRandFloat() * EFCOMMON_SHOCKSMALL_SCALE_BASE) + EFCOMMON_SHOCKSMALL_SCALE_ADD;

    dobj->scale.vec.f.x = dobj->scale.vec.f.y = scale;

    dobj->rotate.vec.f.z = syUtilsRandFloat() * F_CLC_DTOR32(360.0F); // F_CLC_DTOR32(360.0F)

    return effect_gobj;
}

/* ---- efmanager.c:3998-4026 efManagerFireSparkMakeEffect
 * 103), the decomp's own "I really have no idea where this effect is
 * used; it can only be created by script" ----
 *
 * DIVERGES: efmanager.c:4023's lbCommonSetDObjTransformsForTreeDObjs
 * over dobj->child is dropped. Flags 0x4 and not 0x1 means
 * efManagerMakeEffectNoForce already walked this same DObjDesc array
 * onto effect_gobj's own DObj (efmanager.c:2021-2040, the dead
 * explosion's branch, not the sparks'), so dobj->child already carries
 * the tree efshock.mdl -- effirespark.mdl, rather -- baked at export;
 * the decomp's second walk re-reads relocData the port has no copy of
 * and would only repeat work already done, the same shape as
 * efManagerYoshiEggLaySetAnim's dropped lbRelocGetFileData call above. */
// 0x80100DEC
GObj* efManagerFireSparkMakeEffect(GObj *fighter_gobj) // I really have no idea where this effect is used; it can only be created by script
{
    FTStruct *fp;
    EFStruct *ep;
    GObj *effect_gobj;
    DObj *dobj;

    fp = ftGetStruct(fighter_gobj);

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerFireSparkEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    fp->is_effect_attach = TRUE;

    ep = efGetStruct(effect_gobj);
    ep->fighter_gobj = fighter_gobj;

    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f.y = 160.0F;
    dobj->user_data.p = fp->joints[16];

    return effect_gobj;
}

/* ---- efmanager.c:4777-4842 efManagerDeadExplodeMakeEffect ----
 *
 * The burst a fighter leaves when he is knocked off the screen, and the
 * last efManager* the port had stubbed. ft/ftcommon/ftcommondead.c calls
 * it from three statuses with type 0, 1 and 3 -- up, and the two sides
 * -- and follows it with the screen flash.
 *
 * Two things happen at once and neither depends on the other. A particle
 * generator is made from the bank, its id chosen by player and by which
 * way up the burst is out of dEFManagerDeadExplodeGenID; and an effect
 * GObj is made from the descriptor, its three shards placed and two of
 * them given the player's colour. If the generator cannot be had the
 * effect is still made, which is why the particle half falls through
 * rather than returning.
 *
 * Verbatim but for the DIVERGES below.
 *
 * DIVERGES, one line added and none taken away: `dEFManagerDeadExplodeEffectDesc.o_matanim_joint =
 * dEFManagerDeadExplodeMatAnimJoints[player]` (efmanager.c:4808) becomes
 * sEFManagerMatAnimAlt = player. The game picks a MatAnimJoint by
 * writing its block offset into the shared descriptor; here the four
 * blocks are baked side by side into efdexp.mdl and the alternate is an
 * index (FPackMObjs.alt_count, src/dc/objmodel.c dc_model_add_mobjs_alt).
 * The assignment above it is kept -- the table and the descriptor field
 * are both still here, and both still say what the game says -- so what
 * the port drops is only the indirection through relocData that every
 * other block offset in this file has already lost. ---- */
// 0x801021C0
GObj* efManagerDeadExplodeMakeEffect(Vec3f *pos, s32 player, u32 type)
{
    s32 unused[4];
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    DObj *dobj;
    DObj *child_dobj;
    DObj *sibling_dobj;
    u8 index = ((type % 2) * GMCOMMON_PLAYERS_MAX) + player; // WARNING: dEFManagerDeadExplodeGenID should be u8[2][GMCOMMON_PLAYERS_MAX], but it will not match this way; UB-risk

    pc = lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(1), dEFManagerDeadExplodeGenID[index]);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;

            xf->rotate.z = F_CLC_DTOR32(dEFManagerDeadExplodeRotateD[type]);
        }
        else lbParticleEjectStruct(pc);
    }
    /* DIVERGES: efmanager.c:4808, see above. */
    dEFManagerDeadExplodeEffectDesc.o_matanim_joint = dEFManagerDeadExplodeMatAnimJoints[player];
    sEFManagerMatAnimAlt = player;

    effect_gobj = efManagerMakeEffectForce(&dEFManagerDeadExplodeEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);
    dobj->translate.vec.f = *pos;

    child_dobj = dobj->child;

    dobj->rotate.vec.f.z = F_CLC_DTOR32(dEFManagerDeadExplodeRotateD[type]);

    sibling_dobj = dobj->child->sib_next->sib_next;

    sibling_dobj->mobj->sub.envcolor.s.r = dEFManagerDeadExplodeEnvColorSiblingR[player];
    sibling_dobj->mobj->sub.envcolor.s.g = dEFManagerDeadExplodeEnvColorSiblingG[player];
    sibling_dobj->mobj->sub.envcolor.s.b = dEFManagerDeadExplodeEnvColorSiblingB[player];

    sibling_dobj->mobj->sub.flags |= MOBJ_FLAG_ENVCOLOR;

    child_dobj->mobj->sub.envcolor.s.r = dEFManagerDeadExplodeEnvColorChildR[player];
    child_dobj->mobj->sub.envcolor.s.g = dEFManagerDeadExplodeEnvColorChildG[player];
    child_dobj->mobj->sub.envcolor.s.b = dEFManagerDeadExplodeEnvColorChildB[player];

    child_dobj->mobj->sub.flags |= MOBJ_FLAG_ENVCOLOR;

    return effect_gobj;
}

/* ---- efmanager.c:5991-6018 efManagerRebirthHaloMakeEffect ----
 *
 * Verbatim, and it can be: DObj.user_data is not the field the
 * port's model tree uses (that is DObj.dv, sys/objtypes.h:439), so
 * the attach joint goes exactly where the game puts it, and the
 * matrix kind the EFDesc named for the root -- 0x50, lb/lbcommon.c's
 * func_ovl0_800C99CC -- reads it from there and puts the halo at
 * that joint's world position (src/dc/objdisplay.c). `->child` is
 * the halo itself, pack joint 1, sixty units under the fighter's
 * TopN, and `scale` is the fighter's own halo_size. ---- */
// 0x80104068
GObj* efManagerRebirthHaloMakeEffect(GObj *fighter_gobj, f32 scale)
{
    GObj *effect_gobj;
    EFStruct *ep;
    DObj *dobj, *child;

    effect_gobj = efManagerMakeEffectForce(&dEFManagerRebirthHaloEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->fighter_gobj = fighter_gobj;

    dobj = DObjGetStruct(effect_gobj);
    dobj->user_data.p = ftGetStruct(fighter_gobj)->joints[nFTPartsJointTopN];

    child = DObjGetStruct(effect_gobj)->child;
    child->scale.vec.f.x = child->scale.vec.f.y = child->scale.vec.f.z = scale;

    return effect_gobj;
}

/* efmanager.c:5211-5225 efManagerMBallRaysMakeEffect 0x80102C28, verbatim.
 * The halo's shape exactly -- efManagerMakeEffectNoForce, a
 * NULL check, the position -- and the only maker in the port whose caller
 * KEEPS the GObj: itMBallOpenInitVars stores it in
 * item_vars.mball.effect_gobj and itMBallOpenProcUpdate drags it along with
 * the ball every frame until the ball's open animation ends. Nothing here
 * frees it; the effect's own life is the ball's. */
GObj* efManagerMBallRaysMakeEffect(Vec3f *pos)
{
    GObj *effect_gobj;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerMBallRaysEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    return effect_gobj;
}

/* efmanager.c:6158-6174 efManagerItemGetSwirlProcUpdate 0x80104474,
 * verbatim (the item-use step). The rays' maker again, and named "Proc
 * Update" in the decomp though it is a maker like the rest -- the game's
 * own name, kept. it/itmain.c's itMainSetFighterHold calls it with the
 * world position of the hand the item has just gone into, and drops the
 * GObj on the floor: the swirl's own animation ends it. */
GObj* efManagerItemGetSwirlProcUpdate(Vec3f *pos)
{
    GObj *effect_gobj;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerItemGetSwirlEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    return effect_gobj;
}

/* ---- efmanager.c:6047-6074 efManagerEggBreakMakeEffect, verbatim: the
 * shell fragments when Yoshi's egg breaks -- script 0x54 of the common
 * bank, at the world position of YRotN, from ftCommonGuardOffProcUpdate
 * (src/dc/ftcommon.c). ---- */
// 0x801041A0
LBParticle* efManagerEggBreakMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeScriptID(gEFManagerParticleBankID, 0x54);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

// 0x8012E71C
u8 dEFManagerMusicNoteScriptIDs[/* */] = { 0x40, 0x41, 0x42 };

/* ---- the common-bank makers ----
 *
 * Everything left in ef/efmanager.c that needs neither a baked model nor
 * an it/ file: thirty functions across thirteen runs, every one of them
 * a lbParticle maker reading a script out of gEFManagerParticleBankID --
 * the dust a landing kicks up, the flames, the three flashes, the
 * sparkles, the ripple, the shield break, the stock icons the results
 * screen snaps and steals, the music note, the battle score.
 *
 * They were absent for one reason and it was not the effects: almost
 * every one is reached only through ft/ftparam.c's ftParamMakeEffect,
 * the switch a motion script's effect event goes through, and that was
 * still a stub. So this step makes them all real and the next one opens
 * the gate -- which is the order that keeps each step's claim checkable,
 * because efmanager_check reads every script id these name and holds it
 * to the efcommon bank's 119.
 *
 * Taken in the decomp's own order so the runs below are contiguous. ---- */
// 0x800FE260
void efManagerVelAddDestroyAnimEnd(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    DObj *dobj = DObjGetStruct(effect_gobj);

    gcPlayAnimAll(effect_gobj);

    if (dobj->mobj->anim_frame <= 0.0F)
    {
        efManagerSetPrevStructAlloc(ep);
        gcEjectGObj(effect_gobj);
    }
    else
    {
        dobj->translate.vec.f.x += ep->effect_vars.common.vel.x;
        dobj->translate.vec.f.y += ep->effect_vars.common.vel.y;
    }
}

// 0x800FE9B4
LBParticle* efManagerFlameLRMakeEffect(Vec3f *pos, s32 lr)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;
    f32 angle;
    f32 vel;
    f32 scale;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = lbParticleMakeScriptID(gEFManagerParticleBankID, 0x12);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            xf->effect_gobj = effect_gobj;
            xf->proc_dead = efManagerDefaultProcDead;

            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            gcAddGObjProcess(effect_gobj, efManagerDefaultProcUpdate, nGCProcessKindFunc, 3);

            ep->effect_vars.common.xf = xf;

            xf->translate = *pos;

            xf->translate.x += ((syUtilsRandFloat() * EFCOMMON_FLAMELR_OFF_X_BASE) + EFCOMMON_FLAMELR_OFF_X_ADD);
            xf->translate.y += ((syUtilsRandFloat() * EFCOMMON_FLAMELR_OFF_Y_BASE) + EFCOMMON_FLAMELR_OFF_Y_ADD);

            angle = syUtilsRandFloat() * F_CLC_DTOR32(90.0F);

            ep->effect_vars.common.vel.x = __cosf(angle) * EFCOMMON_FLAMELR_VEL_BASE * -lr;
            ep->effect_vars.common.vel.y = __sinf(angle) * EFCOMMON_FLAMELR_VEL_BASE;

            xf->scale.x = xf->scale.y = xf->scale.z = (syUtilsRandFloat() * 1) + 1.0F;
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}

// 0x800FE9B4
LBParticle* efManagerFlameRandomMakeEffect(Vec3f *pos)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;
    f32 angle;
    f32 vel;
    f32 scale;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = lbParticleMakeScriptID(gEFManagerParticleBankID, 0x55);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            xf->effect_gobj = effect_gobj;
            xf->proc_dead = efManagerDefaultProcDead;

            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            gcAddGObjProcess(effect_gobj, efManagerDefaultProcUpdate, nGCProcessKindFunc, 3);

            ep->effect_vars.common.xf = xf;

            xf->translate = *pos;

            angle = (syUtilsRandFloat() * EFCOMMON_FLAMERANDOM_ANGLE_BASE) + EFCOMMON_FLAMERANDOM_ANGLE_ADD;

            ep->effect_vars.common.vel.x = __cosf(angle) * EFCOMMON_FLAMERANDOM_VEL_BASE;
            ep->effect_vars.common.vel.y = __sinf(angle) * EFCOMMON_FLAMERANDOM_VEL_BASE;

            xf->scale.x = xf->scale.y = xf->scale.z = (syUtilsRandFloat() * 1) + 1.0F;
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}

// 0x800FEB58
LBParticle* efManagerFlameStaticMakeEffect(Vec3f *pos)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;
    f32 angle;
    f32 vel;
    f32 scale;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = lbParticleMakeScriptID(gEFManagerParticleBankID, 0x55);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            xf->effect_gobj = effect_gobj;
            xf->proc_dead = efManagerDefaultProcDead;

            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            gcAddGObjProcess(effect_gobj, efManagerDefaultProcUpdate, nGCProcessKindFunc, 3);

            ep->effect_vars.common.xf = xf;

            xf->translate = *pos;

            ep->effect_vars.common.vel.x = 0.0F;
            ep->effect_vars.common.vel.y = 0.0F;

            xf->scale.x = xf->scale.y = xf->scale.z = (syUtilsRandFloat() * 1) + 1.0F;
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}

// 0x800FEFE0
void efManagerDustLightProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    ep->effect_vars.dust_light.xf->translate.x += ep->effect_vars.dust_light.vel1.x;
    ep->effect_vars.dust_light.xf->translate.y += ep->effect_vars.dust_light.vel1.y;

    if (ep->effect_vars.dust_light.lifetime != 0)
    {
        ep->effect_vars.dust_light.lifetime--;

        ep->effect_vars.dust_light.vel1.x += ep->effect_vars.dust_light.vel2.x;
        ep->effect_vars.dust_light.vel1.y += ep->effect_vars.dust_light.vel2.y;
    }
}

// 0x800FF048
LBParticle* efManagerDustLightMakeEffect(Vec3f *pos, s32 lr, f32 f_index)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;
    f32 angle;
    f32 vel;
    f32 scale;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = (f_index == 2.0F) ? lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0x56) : lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0x55);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            xf->effect_gobj = effect_gobj;
            xf->proc_dead = efManagerDefaultProcDead;

            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            gcAddGObjProcess(effect_gobj, efManagerDustLightProcUpdate, nGCProcessKindFunc, 3);

            ep->effect_vars.dust_light.xf = xf;

            xf->translate = *pos;

            xf->translate.y += EFCOMMON_DUSTNORMAL_OFF_Y;

            xf->rotate.z = syUtilsRandFloat() * F_CLC_DTOR32(360.0F); // F_CLC_DTOR32(360.0F)

            angle = (syUtilsRandFloat() * EFCOMMON_DUSTNORMAL_ANGLE_BASE) + EFCOMMON_DUSTNORMAL_ANGLE_ADD;

            ep->effect_vars.dust_light.vel1.x = __cosf(angle) * EFCOMMON_DUSTNORMAL_VEL_BASE;

            if (lr == +1)
            {
                ep->effect_vars.dust_light.vel1.x = -ep->effect_vars.dust_light.vel1.x;
            }
            ep->effect_vars.dust_light.vel1.y = __sinf(angle) * EFCOMMON_DUSTNORMAL_VEL_BASE;

            ep->effect_vars.dust_light.lifetime = EFCOMMON_DUSTNORMAL_LIFETIME;

            ep->effect_vars.dust_light.vel2.x = -ep->effect_vars.dust_light.vel1.x * EFCOMMON_DUSTNORMAL_SCATTER;
            ep->effect_vars.dust_light.vel2.y = -ep->effect_vars.dust_light.vel1.y * EFCOMMON_DUSTNORMAL_SCATTER;
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}

// 0x800FF278
LBParticle* efManagerDustHeavyMakeEffect(Vec3f *pos, s32 lr)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;
    f32 angle;
    f32 vel;
    f32 scale;

    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    effect_gobj->user_data.p = NULL;

    pc = lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0x58);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            xf->effect_gobj = effect_gobj;

            xf->translate = *pos;

            xf->translate.y += EFCOMMON_DUSTHEAVY_OFF_Y;

            if (lr == -1)
            {
                xf->rotate.y = F_CLC_DTOR32(180.0F);
            }
            xf->proc_dead = efManagerDefaultProcDead;
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}

// 0x800FF384
void efManagerDustHeavyDoubleProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    s32 unused;

    ep->effect_vars.dust_heavy.anim_frame++;

    if (ep->effect_vars.dust_heavy.anim_frame == 2)
    {
        Vec3f pos = ep->effect_vars.dust_heavy.xf->translate;

        pos.y -= 126.0F;

        efManagerDustHeavyMakeEffect(&pos, -ep->effect_vars.dust_heavy.lr);
    }
}

// 0x800FF3F4
LBParticle* efManagerDustHeavyDoubleMakeEffect(Vec3f *pos, s32 lr, f32 f_index)
{
    GObj *effect_gobj;
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;
    f32 angle;
    f32 vel;
    f32 scale;

    ep = efManagerGetEffectNoForce();

    if (ep == NULL)
    {
        return NULL;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = (f_index == 1.7F) ? lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0x59) : lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0x58); // Why such a specific check when a bool could've worked?

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            gcAddGObjProcess(effect_gobj, efManagerDustHeavyDoubleProcUpdate, nGCProcessKindFunc, 3);

            xf->effect_gobj = effect_gobj;

            ep->effect_vars.dust_heavy.xf = xf;

            xf->translate = *pos;

            xf->translate.y += EFCOMMON_DUSTHEAVY_OFF_Y;

            ep->effect_vars.dust_heavy.pos = *pos;

            ep->effect_vars.dust_heavy.anim_frame = 0;

            ep->effect_vars.dust_heavy.lr = lr;

            if (lr == -1)
            {
                xf->rotate.y = F_CLC_DTOR32(180.0F);
            }
            xf->proc_dead = efManagerDefaultProcDead;
        }
        else pc = efManagerDestroyParticleGObj(pc, effect_gobj);
    }
    else efManagerDestroyParticleGObj(NULL, effect_gobj);

    return pc;
}

// 0x800FF590
LBParticle* efManagerDustExpandLargeMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0x57);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;

            xf->scale.x = EFCOMMON_DUSTEXPANDLARGE_SCALE;
            xf->scale.y = EFCOMMON_DUSTEXPANDLARGE_SCALE;
            xf->scale.z = EFCOMMON_DUSTEXPANDLARGE_SCALE;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

// 0x800FF7D8
LBParticle* efManagerDustDashMakeEffect(Vec3f *pos, s32 lr, f32 scale)
{
    LBParticle *pc;
    LBTransform *xf;
    EFStruct *ep;

    pc = lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0x5A);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;

            xf->scale.x = scale;
            xf->scale.y = scale;
            xf->scale.z = scale;

            xf->translate.y += EFCOMMON_DUSTDASH_OFF_Y;

            if (lr == -1)
            {
                xf->rotate.y = F_CLC_DTOR32(180.0F);
            }
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

// 0x80100524
LBParticle* efManagerSparkleWhiteMultiMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0x1A);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

// 0x8010066C
LBParticle* efManagerSparkleWhiteScaleMakeEffect(Vec3f *pos, f32 scale)
{
    LBParticle *pc = lbParticleMakeScriptID(gEFManagerParticleBankID, 0x5B);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;

            xf->scale.x = scale;
            xf->scale.y = scale;
            xf->scale.z = scale;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

// 0x80101408
LBParticle* efManagerThunderAmpMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeScriptID(gEFManagerParticleBankID, 0x74);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

// 0x801014A8
LBGenerator* efManagerRippleMakeEffect(Vec3f *pos)
{
    LBGenerator *gn = lbParticleMakeGenerator(gEFManagerParticleBankID, 0x61);

    if (gn != NULL)
    {
        gn->pos.x = pos->x;
        gn->pos.y = pos->y;
        gn->pos.z = pos->z;
    }
    return gn;
}

// 0x801015D4
LBParticle* efManagerFuraSparkleMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeCommon(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0);

    if (pc != NULL)
    {
        pc->pos.x = pos->x;
        pc->pos.y = pos->y;
        pc->pos.z = pos->z;
    }
    return pc;
}

// 0x80101630
LBParticle* efManagerPsionicMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeCommon(gEFManagerParticleBankID, 7);

    if (pc != NULL)
    {
        pc->pos.x = pos->x;
        pc->pos.y = pos->y;
        pc->pos.z = pos->z;
    }
    return pc;
}

// 0x80101688
LBParticle* efManagerFlashSmallMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeCommon(gEFManagerParticleBankID, 4);

    if (pc != NULL)
    {
        pc->pos.x = pos->x;
        pc->pos.y = pos->y;
        pc->pos.z = pos->z;
    }
    return pc;
}

// 0x801016E0
LBParticle* efManagerFlashMiddleMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeCommon(gEFManagerParticleBankID, 5);

    if (pc != NULL)
    {
        pc->pos.x = pos->x;
        pc->pos.y = pos->y;
        pc->pos.z = pos->z;
    }
    return pc;
}

// 0x80101738
LBParticle* efManagerFlashLargeMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeCommon(gEFManagerParticleBankID, 6);

    if (pc != NULL)
    {
        pc->pos.x = pos->x;
        pc->pos.y = pos->y;
        pc->pos.z = pos->z;
    }
    return pc;
}

// 0x80101790
LBGenerator* efManagerShieldBreakMakeEffect(Vec3f *pos)
{
    LBGenerator *gn = lbParticleMakeGenerator(gEFManagerParticleBankID, 3);

    if (gn != NULL)
    {
        gn->pos.x = pos->x;
        gn->pos.y = pos->y;
        gn->pos.z = pos->z;
    }
    return gn;
}

// 0x80102018
LBGenerator* efManagerKirbyStarMakeEffect(Vec3f *pos)
{
    LBGenerator *gn = lbParticleMakeGenerator(gEFManagerParticleBankID, 0xF);

    if (gn != NULL)
    {
        gn->pos.x = pos->x;
        gn->pos.y = pos->y;
        gn->pos.z = pos->z;
    }
    return gn;
}

// 0x80102E90
LBParticle* efManagerHealSparklesMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0xE);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

// 0x80103918
void efManagerStockCommonMakeEffectID(f32 pos_x, f32 pos_y, s32 script_id)
{
    pos_x *= 4.0F;
    pos_y *= 4.0F;

    lbParticleMakePosVel(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(2), script_id, pos_x, pos_y, 0.0F, 0.0F, 0.0F, 0.0F);
}

// 0x80103974
void efManagerStockSnapMakeEffect(f32 pos_x, f32 pos_y)
{
    efManagerStockCommonMakeEffectID(pos_x, pos_y, 0x26);
}

// 0x80103994
void efManagerStockStealStartMakeEffect(f32 pos_x, f32 pos_y)
{
    efManagerStockCommonMakeEffectID(pos_x, pos_y, 0x75);
}

// 0x801039B4
void efManagerStockStealEndMakeEffect(f32 pos_x, f32 pos_y)
{
    efManagerStockCommonMakeEffectID(pos_x, pos_y, 0x76);
}

// 0x801039D4
LBParticle* efManagerMusicNoteMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeScriptID
    (
        gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(0),
        dEFManagerMusicNoteScriptIDs[syUtilsRandIntRange(ARRAY_COUNT(dEFManagerMusicNoteScriptIDs))]
    );

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

// 0x801040E0
LBParticle* efManagerBattleScoreMakeEffect(Vec3f *pos, s32 score)
{
    LBParticle *pc = lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(2), (score > 0) ? 0x43 : 0x44);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf == NULL)
        {
            lbParticleEjectStruct(pc);

            return NULL;
        }
        LBParticleProcessStruct(pc);

        if (xf->users_num == 0)
        {
            return NULL;
        }
        xf->translate = *pos;

        xf->scale.y = 0.25F;
    }
    return pc;
}

// 0x801044B4
LBParticle* efManagerItemSpawnSwirlMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeScriptID(gEFManagerParticleBankID, 0x69);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

/* ---- efmanager.c:5808-5834 efManagerYoshiEggExplodeMakeEffect and
 * :6077-6086 / :6089-6155 efManagerKirbyInhaleWindProcUpdate and
 * MakeEffect, all three verbatim ----
 *
 * These three are the only makers in this file that read a PER-FIGHTER
 * particle bank, and until this step they were the port's only two
 * efManager* stubs that needed no model at all -- they sat in
 * src/dc/ftcommon.c returning NULL, since s 59 and 62, because
 * gFTDataYoshiParticleBankID and gFTDataKirbyParticleBankID were
 * bss-zero and bank 0 is efcommon's. Calling through an unloaded id
 * would not have drawn nothing; it would have drawn the COMMON bank's
 * script 3 and script 0xC, silently and wrongly, which is why an honest
 * stub was better than a wrong effect.
 *
 * src/dc/ftmanager.c loads both banks now, per kind and per scene, where
 * ft/ftmanager.c:281-296 loads them, so the ids are real and the stubs
 * are gone. Yoshi's egg shatters when it breaks, and Kirby's inhale has
 * its wind again. The third per-fighter bank, Ness's, joined with his
 * specials. ---- */
// 0x80103A88
LBParticle* efManagerYoshiEggExplodeMakeEffect(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeScriptID(gFTDataYoshiParticleBankID, 3);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

// 0x80104240
void efManagerKirbyInhaleWindProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    LBTransform *xf = ep->xf;

    xf->translate = DObjGetStruct(ep->fighter_gobj)->translate.vec.f;

    xf->translate.x += ftGetStruct(ep->fighter_gobj)->lr * 800.0F;
    xf->translate.y += 230.0F;
}

// 0x801042B4
LBParticle* efManagerKirbyInhaleWindMakeEffect(GObj *fighter_gobj)
{
    LBParticle *pc;
    LBTransform *xf;
    GObj *effect_gobj;
    EFStruct *ep;

    ep = efManagerGetEffectForce();

    if (ep == NULL)
    {
        return 0;
    }
    effect_gobj = gcMakeGObjSPAfter(nGCCommonKindEffect, NULL, nGCCommonLinkIDEffect, GOBJ_PRIORITY_DEFAULT);

    if (effect_gobj == NULL)
    {
        efManagerSetPrevStructAlloc(ep);

        return NULL;
    }
    effect_gobj->user_data.p = ep;

    pc = lbParticleMakeScriptID(gFTDataKirbyParticleBankID | LBPARTICLE_MASK_GENLINK(0), 0xC);

    if (pc != NULL)
    {
        xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusDefault);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = DObjGetStruct(fighter_gobj)->translate.vec.f;

            xf->translate.x += ftGetStruct(fighter_gobj)->lr * 800.0F;
            xf->translate.y += 230.0F;

            xf->scale.x = 1.0F;
            xf->scale.y = 1.0F;
            xf->scale.z = 1.0F;

            xf->rotate.z = (ftGetStruct(fighter_gobj)->lr == -1) ? F_CLC_DTOR32(90.0F) : F_CLC_DTOR32(-90.0F);

            effect_gobj->user_data.p = ep; // y u do dis again

            gcAddGObjProcess(effect_gobj, efManagerKirbyInhaleWindProcUpdate, nGCProcessKindFunc, 3);

            ep->xf = pc->xf;

            ep->bank_id = pc->bank_id;

            ep->fighter_gobj = fighter_gobj;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

/* ---- The port's own, between two of the decomp's runs: the wrapper
 * every scene that makes effects calls where the game calls
 * efManagerInitEffects -- scvsbattle.c:164 and :436, once per battle,
 * and mnvsresults.c:3340, once per results screen(for the
 * confetti; it was scvsbattle.c's static until then). efManagerInitEffects
 * above makes the 38-entry EFStruct pool every effect takes from, the two
 * pairs of render-mode GObjs, and then efDisplayInitAll: the four display
 * GObjs whose procs are the only thing in the game that draws a
 * particle, and the common effect bank, loaded once per scene and cached
 * by ROM address for the rest of it (ef/efparticle.c
 * efParticleGetLoadBankID, in the link since  5). What is here
 * is the timing and the one line that prints what the walk found, and
 * the one thing the host build cannot do. ---- */
void efManagerLoadEffectBank(void)
{
    uint32_t reads, bytes, vram;
    int frames;

#ifdef FT_HOSTTEST
    /* DIVERGES, and only in this build: the host cross-tests run on
     * x86-64, where `LBScript *` is eight bytes and the bank's own arrays
     * are four. lbParticleSetupBankID walks `script_desc->scripts[i]` --
     * an `LBScript*[]` overlaid on the ROM's `u32[]` -- so on a 64-bit
     * host it reads and writes at twice the stride and off the end of the
     * block. Nothing can be done about that from this side: the game's
     * data model is a 32-bit machine's, and this is the first file the
     * port compiles whose *data* says so rather than its code.
     *
     * So the bank does not load in that build, and the host suite tests
     * what it can honestly reach: src/dc/dma.c's side of it, and the
     * exported bank's own shape (hosttest_ft.c test_particle_bank).
     * tools/check/particle_check.py has the rest -- all eighteen banks against
     * the decomp's own src/particles sources -- and the walk itself is
     * checked on the target, where a pointer is the width the game wrote
     * it: the `particles:` line below prints the counts it arrived at. */
    (void)reads;
    (void)bytes;
    gEFManagerParticleBankID = -1;
    return;
#endif

    reads = sy_dma_reads(&bytes);

    efManagerInitEffects();

    frames = 0;
    vram = lbpTexBytes(&frames);

    /* One line a run, because it is the only place the whole chain shows
     * at once: the two ranges came off the medium, the walk found the
     * counts the decomp's own efcommon_scb.c/efcommon_txb.c declare
     * (119 and 47), and the second battle in a scene reads nothing --
     * efParticleGetLoadBankID's cache -- while the second *scene* reads
     * both again, because the heap under them was emptied. */
    reads = sy_dma_reads(&bytes) - reads;
    syDebugPrintf("particles: efcommon bank %d -- %d scripts, %d textures,"
           " %u reads, %u bytes; %u/%u scene heap bytes;"
           " %d frames, %u VRAM bytes\n",
           (int)gEFManagerParticleBankID,
           (int)lb_particle_scripts(gEFManagerParticleBankID),
           (int)lb_particle_textures(gEFManagerParticleBankID),
           (unsigned)reads, (unsigned)bytes,
           (unsigned)syTaskmanGeneralHeapUsed(),
           (unsigned)syTaskmanGeneralHeapSize(),
           frames, (unsigned)vram);
}

/* ---- efmanager.c:6205-6234 efManagerConfettiMakeEffect, verbatim: the
 * results screen's two showers (mn/mnvsmode/mnvsresults.c
 * mnVSResultsMakeConfetti, src/dc/scvsresults.c), script 0x70 of the
 * common bank, the second on generator link 3. ---- */
// 0x80104554
LBParticle* efManagerConfettiMakeEffect(Vec3f *pos, sb32 is_genlink_mask)
{
    LBParticle *pc = (is_genlink_mask != FALSE) ?
    lbParticleMakeScriptID(gEFManagerParticleBankID, 0x70) :
    lbParticleMakeScriptID(gEFManagerParticleBankID | LBPARTICLE_MASK_GENLINK(3), 0x70);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

/* efmanager.c:5459-5514 func_ovl2_801031E0 0x801031E0 and func_ovl2_80103280
 * 0x80103280, verbatim.  97: the two arms of ftParamMakeEffect's
 * switch that have no nEFKind name -- the decomp writes them as bare
 * `case 0x4C:` and `case 0x4D:`. Both are Kirby's own particle bank
 * (particles_unk0, loaded since ), scripts 2 and 5, in the shape the
 * port already knows: make, add a ready LBTransform, process once, and if
 * the script consumed no users give the particle back. Nothing diverges. */
LBParticle* func_ovl2_801031E0(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeScriptID(gFTDataKirbyParticleBankID, 2);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

// 0x80103280
LBParticle* func_ovl2_80103280(Vec3f *pos)
{
    LBParticle *pc = lbParticleMakeScriptID(gFTDataKirbyParticleBankID, 5);

    if (pc != NULL)
    {
        LBTransform *xf = lbParticleAddTransformForStruct(pc, nLBTransformStatusReady);

        if (xf != NULL)
        {
            LBParticleProcessStruct(pc);

            if (xf->users_num == 0)
            {
                return NULL;
            }
            xf->translate = *pos;
        }
        else
        {
            lbParticleEjectStruct(pc);

            pc = NULL;
        }
    }
    return pc;
}

/* ---- G04: Ness's four effects ----
 *
 * PSI Magnet's bubble (NessSpecial2), and out of NessModel the PK Thunder
 * wave he calls it with and the trail both the bolt and its reflected
 * twin drag. The EFDescs (efmanager.c:1011-1129) and the makers
 * (4951-5009, 5025-5130) are verbatim; the ProcDisplay between them
 * diverges below, the way Pikachu's trail's does. Their models are packs
 * (tools/export/ssb_effectexport.py --what psimagnet, pkwave, pktrail) named by
 * sEFManagerModels, and their nine reloc symbols are defined at zero, as
 * every other effect's are.
 *
 * The trail makers run their ProcUpdate before returning, and it reads
 * the OWNER's state -- the fighter's passive_vars trail ring, or the
 * reflected head's DObj -- and never the model: efManagerAddModel has
 * only built the one DObj whose translate and rotate it writes. The
 * bubble's and the wave's second transform kind is 0x2E, which
 * src/dc/objdisplay.c's gcPrepDObjMatrix draws as the game's billboard
 * with the joint's rotate.z and scale. ---- */
int llNessSpecial2PsychicMagnetDObjDesc;
int llNessSpecial2PsychicMagnetMObjSub;
int llNessSpecial2PsychicMagnetAnimJoint;
int llNessSpecial2PsychicMagnetMatAnimJoint;
int llNessModelPKThunderTrailDObjDesc;
int llNessModelPKThunderWaveDObjDesc;
int llNessModelPKThunderWaveMObjSub;
int llNessModelPKThunderWaveAnimJoint;
int llNessModelPKThunderWaveMatAnimJoint;

// 0x8012E41C
EFDesc dEFManagerNessPsychicMagnetEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    15,                                     // DL Link
    &gFTNessFileSpecial2,                   // Texture file

    // DObj transformation struct 1
    {
        0x50,                               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTra,                // Main matrix transformations
        0x2E,                               // Secondary matrix transformations
        0x00                                // ???
    },

    gcPlayAnimAll,                          // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llNessSpecial2PsychicMagnetDObjDesc,             // DObj Setup attributes offset (?)
    &llNessSpecial2PsychicMagnetMObjSub,               // MObjSub offset
    &llNessSpecial2PsychicMagnetAnimJoint,             // AnimJoint offset
    &llNessSpecial2PsychicMagnetMatAnimJoint           // MatAnimJoint offset
};

// 0x8012E444
EFDesc dEFManagerNessPKThunderTrailEffectDesc =
{
    EFFECT_FLAG_USERDATA,                   // Flags
    15,                                     // DL Link
    &gFTNessFileModel,                      // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindNull,               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerNessPKThunderTrailProcUpdate,   // Proc Update
    efManagerNessPKThunderTrailProcDisplay,   // Proc Render

    &llNessModelPKThunderTrailDObjDesc,            // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

// 0x8012E46C
EFDesc dEFManagerNessPKReflectTrailEffectDesc =
{
    EFFECT_FLAG_USERDATA,                   // Flags
    18,                                     // DL Link
    &gFTNessFileModel,                      // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindNull,               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerNessPKReflectTrailProcUpdate,   // Proc Update
    efManagerNessPKThunderTrailProcDisplay,   // Proc Render

    &llNessModelPKThunderTrailDObjDesc,            // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

// 0x8012E494
EFDesc dEFManagerNessPKThunderWaveEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    15,                                     // DL Link
    &gFTNessFileModel,                      // Texture file

    // DObj transformation struct 1
    {
        0x50,                               // Main matrix transformations
        nGCMatrixKindRotRpyR,            // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTra,                // Main matrix transformations
        0x2E,                               // Secondary matrix transformations
        0x00                                // ???
    },

    gcPlayAnimAll,                          // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llNessModelPKThunderWaveDObjDesc,             // DObj Setup attributes offset (?)
    &llNessModelPKThunderWaveMObjSub,               // MObjSub offset
    &llNessModelPKThunderWaveAnimJoint,             // AnimJoint offset
    &llNessModelPKThunderWaveMatAnimJoint           // MatAnimJoint offset
};

// 0x801025D8
GObj* efManagerNessPsychicMagnetMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj;
    EFStruct *ep;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectForce(&dEFManagerNessPsychicMagnetEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->fighter_gobj = fighter_gobj;

    dobj = DObjGetStruct(effect_gobj);

    dobj->user_data.p = ftGetStruct(fighter_gobj)->joints[nFTPartsJointTopN];

    return effect_gobj;
}

// 0x80102630
void efManagerNessPKThunderTrailProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    FTStruct *fp;
    s32 index;

    if (ep->effect_vars.pkthunder.status & nWPNessPKThunderStatusDestroy)
    {
        efManagerSetPrevStructAlloc(ep);
        gcEjectGObj(effect_gobj);

        return;
    }
    fp = ftGetStruct(ep->effect_vars.pkthunder.owner_gobj);

    index = fp->passive_vars.ness.pkthunder_trail_id - (FTNESS_PKTHUNDER_TRAIL_POS_COUNT - 2);

    if (index < 0)
    {
        index += FTNESS_PKTHUNDER_TRAIL_POS_COUNT;
    }
    DObjGetStruct(effect_gobj)->translate.vec.f.x = fp->passive_vars.ness.pkthunder_trail_x[index];
    DObjGetStruct(effect_gobj)->translate.vec.f.y = fp->passive_vars.ness.pkthunder_trail_y[index];

    if (index > 0)
    {
        DObjGetStruct(effect_gobj)->rotate.vec.f.z = syUtilsArcTan2((fp->passive_vars.ness.pkthunder_trail_y[index] - fp->passive_vars.ness.pkthunder_trail_y[index - 1]), (fp->passive_vars.ness.pkthunder_trail_x[index] - fp->passive_vars.ness.pkthunder_trail_x[index - 1]));
    }
    else
    {
        DObjGetStruct(effect_gobj)->rotate.vec.f.z = syUtilsArcTan2((fp->passive_vars.ness.pkthunder_trail_y[index] - fp->passive_vars.ness.pkthunder_trail_y[FTNESS_PKTHUNDER_TRAIL_POS_COUNT - 1]), (fp->passive_vars.ness.pkthunder_trail_x[index] - fp->passive_vars.ness.pkthunder_trail_x[FTNESS_PKTHUNDER_TRAIL_POS_COUNT - 1]));
    }
    DObjGetStruct(effect_gobj)->rotate.vec.f.z -= F_CLC_DTOR32(90.0F);
}

/* ---- efmanager.c:5011-5023 efManagerNessPKThunderTrailProcDisplay ----
 *
 * DIVERGES, six lines dropped and none added, for the reason
 * efManagerPikachuThunderTrailProcDisplay gives: the PVR takes its blend
 * and alpha test off the pack's own bucket. ---- */
// 0x80102768
void efManagerNessPKThunderTrailProcDisplay(GObj *effect_gobj)
{
    gcDrawDObjDLLinksForGObj(effect_gobj);
}

// 0x80102848
GObj* efManagerNessPKThunderTrailMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj;
    FTStruct *fp;
    EFStruct *ep;
    WPStruct *wp;

    fp = ftGetStruct(fighter_gobj);

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerNessPKThunderTrailEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->effect_vars.pkthunder.owner_gobj = fighter_gobj;
    ep->effect_vars.pkthunder.status = nWPNessPKThunderStatusActive;

    DObjGetStruct(effect_gobj)->translate.vec.f.z = 0.0F;

    wp = wpGetStruct(fp->status_vars.ness.specialhi.pkthunder_gobj);

    wp->weapon_vars.pkthunder.trail_gobj[ARRAY_COUNT(wp->weapon_vars.pkthunder.trail_gobj) - 1] = effect_gobj;

    efManagerNessPKThunderTrailProcUpdate(effect_gobj);

    return effect_gobj;
}

// 0x801028C0
void efManagerNessPKReflectTrailProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);

    if (ep->effect_vars.pkthunder.status & nWPNessPKThunderStatusDestroy)
    {
        efManagerSetPrevStructAlloc(ep);
        gcEjectGObj(effect_gobj);
    }
    else
    {
        WPStruct *wp = wpGetStruct(ep->effect_vars.pkthunder.owner_gobj);

        DObjGetStruct(effect_gobj)->translate.vec.f.x = (DObjGetStruct(ep->effect_vars.pkthunder.owner_gobj)->translate.vec.f.x - wp->physics.vel_air.x * 5.0F * 2);
        DObjGetStruct(effect_gobj)->translate.vec.f.y = (DObjGetStruct(ep->effect_vars.pkthunder.owner_gobj)->translate.vec.f.y - wp->physics.vel_air.y * 5.0F * 2);
    }
}

// 0x80102968
GObj* efManagerNessPKReflectTrailMakeEffect(GObj *weapon_gobj)
{
    GObj *effect_gobj;
    WPStruct *wp;
    EFStruct *ep;

    wp = wpGetStruct(weapon_gobj);

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerNessPKReflectTrailEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->effect_vars.pkthunder.owner_gobj = weapon_gobj;
    ep->effect_vars.pkthunder.status = nWPNessPKThunderStatusActive;

    DObjGetStruct(effect_gobj)->translate.vec.f.z = 0.0F;

    DObjGetStruct(effect_gobj)->rotate.vec.f.z = DObjGetStruct(weapon_gobj)->rotate.vec.f.z - F_CLC_DTOR32(90.0F);

    wp->weapon_vars.pkthunder.trail_gobj[ARRAY_COUNT(wp->weapon_vars.pkthunder.trail_gobj) - 1] = effect_gobj;

    efManagerNessPKReflectTrailProcUpdate(effect_gobj);

    return effect_gobj;
}

// 0x801029F8
GObj* efManagerNessPKThunderWaveMakeEffect(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    GObj *effect_gobj = efManagerMakeEffectNoForce(&dEFManagerNessPKThunderWaveEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    else
    {
        EFStruct *ep = efGetStruct(effect_gobj);

        ep->fighter_gobj = fighter_gobj;

        DObjGetStruct(effect_gobj)->user_data.p = fp->joints[5];

        DObjGetStruct(effect_gobj)->rotate.vec.f.y = fp->lr * F_CLC_DTOR32(90.0F);
        DObjGetStruct(effect_gobj)->translate.vec.f.z = 0.0F;

        return effect_gobj;
    }
}

/* ---- G10: a reflector shattering ----
 *
 * ft/ftcommon/ftcommonshieldbreakfly.c's ftCommonShieldBreakFlyReflector-
 * SetStatus makes this where the reflector stood when a projectile beats
 * its resist. The EFDesc (efmanager.c:550-577) and the maker (4264-4284)
 * are verbatim; the model is a pack (tools/export/ssb_effectexport.py --what
 * reflectbreak) named by sEFManagerModels, and the four reloc symbols are
 * defined at zero, as every other effect's are. The first two are named
 * the wrong way round in the generated header -- ...MObjSub is the
 * DObjDesc and ...DObjDesc the MObjSub -- and the EFDesc puts each in the
 * slot that matches what it really is. ---- */
int llEFCommonEffects2ReflectBreakMObjSub;
int llEFCommonEffects2ReflectBreakDObjDesc;
int llEFCommonEffects2ReflectBreakAnimJoint;
int llEFCommonEffects2ReflectBreakMatAnimJoint;

EFDesc dEFManagerReflectBreakEffectDesc =
{
    0x4 | 0x1,                              // Flags
    18,                                     // DL Link
    &gEFManagerFiles[1],                      // Texture file

    // DObj transformation struct 1
    {
        0x28,                               // Main matrix transformations   
        nGCMatrixKindRotRpyR,            // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,     // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llEFCommonEffects2ReflectBreakMObjSub,              // DObj Setup attributes offset (?)
    &llEFCommonEffects2ReflectBreakDObjDesc,                // MObjSub offset
    &llEFCommonEffects2ReflectBreakAnimJoint,              // AnimJoint offset
    &llEFCommonEffects2ReflectBreakMatAnimJoint            // MatAnimJoint offset
};

/* ---- efmanager.c:4264-4284 efManagerReflectBreakMakeEffect, verbatim:
 * the shards at the reflector, turned round for a fighter facing left. */
GObj* efManagerReflectBreakMakeEffect(Vec3f *pos, s32 lr)
{
    GObj *effect_gobj;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerReflectBreakEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    if (lr == -1)
    {
        dobj->rotate.vec.f.y = F_CLC_DTOR32(180.0F);
    }
    return effect_gobj;
}

/* ---- G17: the sparks Pikachu's forward smash throws off ----
 *
 * ft/ftcommon/ftcommonattacks4.c's ftCommonAttackS4ProcUpdate makes one
 * at his tail on each of the two motion flags. The EFDesc
 * (efmanager.c:610-637) is verbatim and the model a pack
 * (tools/export/ssb_effectexport.py --what thundershock) named by
 * sEFManagerModels; the maker (4409-4459) is checked line by line in
 * tools/check/efmanager_check.py. The seven reloc symbols are defined at zero,
 * as every other effect's are. ---- */
int llPikachuSpecial2ThunderShockDObjDesc;
int llPikachuSpecial2ThunderShockMObjSub;
int llPikachuSpecial2ThunderShock0AnimJoint;
int llPikachuSpecial2ThunderShock0MatAnimJoint;

// 0x801018A8
GObj* efManagerPikachuThunderShockMakeEffect(GObj *fighter_gobj, Vec3f *pos, s32 frame)
{
    GObj *effect_gobj;
    EFStruct *ep;
    DObj *dobj;

    /* DIVERGES: the decomp makes the effect with the desc's animation 0
     * and, for gfx_id 1 or 2, swaps in that one's AnimJoint and
     * MatAnimJoint with gcAddAnimAll out of gFTDataPikachuSpecial2 before
     * playing it (efmanager.c:4431-4456). The pack carries all three side
     * by side, so the maker names the alternate before the effect is made
     * and efManagerAddModel attaches it -- efManagerKirbyEntryStarMake-
     * Effect's shape; the play below is the decomp's. */
    sEFManagerAnimAlt = sEFManagerMatAnimAlt = ((frame == 1) || (frame == 2)) ? frame : 0;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerPikachuThunderShockEffectDesc);

    if (effect_gobj == NULL)
    {
        sEFManagerAnimAlt = sEFManagerMatAnimAlt = 0;

        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->fighter_gobj = fighter_gobj;

    dobj = DObjGetStruct(effect_gobj);
    dobj->user_data.p = ftGetStruct(fighter_gobj)->joints[nFTPartsJointTopN];

    dobj->child->translate.vec.f = *pos;

    dobj->child->translate.vec.f.x = (ftGetStruct(fighter_gobj)->lr == -1) ? -pos->x : pos->x;

    gcAddXObjForDObjFixed(dobj->child->child, 0x2E, 0);

    switch (frame)
    {
    case 1:
        gcPlayAnimAll(effect_gobj);
        break;

    case 2:
        gcPlayAnimAll(effect_gobj);
        break;
    }
    return effect_gobj;
}

// 0x8012E1FC
EFDesc dEFManagerPikachuThunderShockEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    15,                                     // DL Link
    &gFTDataPikachuSpecial2,                // Texture file

    // DObj transformation struct 1
    {
        0x4F,                               // Main matrix transformations   
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,     // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llPikachuSpecial2ThunderShockDObjDesc,              // DObj Setup attributes offset (?)
    &llPikachuSpecial2ThunderShockMObjSub,                // MObjSub offset
    &llPikachuSpecial2ThunderShock0AnimJoint,              // AnimJoint offset
    &llPikachuSpecial2ThunderShock0MatAnimJoint            // MatAnimJoint offset
};

/* ---- V10: the grab swirl ----
 *
 * ft/ftcommon/ftcommoncatch2.c's ftCommonCatchPullProcCatch makes this at
 * the catcher's heavy-item joint as a grab connects. The EFDesc
 * (efmanager.c:520-547) and the maker (4245-4261) are verbatim; the model
 * is a pack (tools/export/ssb_effectexport.py --what catchswirl) named by
 * sEFManagerModels, and the four reloc symbols are defined at zero, as
 * every other effect's are. The first two are crossed in the generated
 * header the way the reflector shards' are, and the EFDesc already puts
 * each in the slot that matches what it really is. ---- */
int llEFCommonEffects2CatchSwirlMObjSub;
int llEFCommonEffects2CatchSwirlDObjDesc;
int llEFCommonEffects2CatchSwirlAnimJoint;
int llEFCommonEffects2CatchSwirlMatAnimJoint;

EFDesc dEFManagerCatchSwirlEffectDesc = 
{
    0x4 | 0x1,                              // Flags
    18,                                     // DL Link
    &gEFManagerFiles[1],                      // Texture file

    // DObj transformation struct 1
    {
        0x28,                               // Main matrix transformations   
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations  
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerHaveStructProcUpdate,     // Proc Update
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llEFCommonEffects2CatchSwirlMObjSub,                // DObj Setup attributes offset (?)
    &llEFCommonEffects2CatchSwirlDObjDesc,                  // MObjSub offset
    &llEFCommonEffects2CatchSwirlAnimJoint,                // AnimJoint offset
    &llEFCommonEffects2CatchSwirlMatAnimJoint              // MatAnimJoint offset
};

/* ---- efmanager.c:4245-4261 efManagerCatchSwirlMakeEffect, verbatim:
 * the swirl at the grabbing hand. */
GObj* efManagerCatchSwirlMakeEffect(Vec3f *pos)
{
    GObj *effect_gobj;
    DObj *dobj;

    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerCatchSwirlEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    return effect_gobj;
}

/* ---- V10: Samus's grapple beam glow ----
 *
 * ft/ftcommon/ftcommoncatch1.c's ftCommonCatchSetStatus and
 * ftcommonthrow.c's throw setter make this for Samus, hung off her joint
 * 23 by kind 0x4F. The EFDesc (efmanager.c:730-757) and the maker
 * (4622-4641) are verbatim; the model is a pack
 * (tools/export/ssb_effectexport.py
 * --what grapplebeam) named by sEFManagerModels, and the four reloc
 * symbols are defined at zero, as every other effect's are. ---- */
int llSamusSpecial2GrappleBeamDObjDesc;
int llSamusSpecial2GrappleBeamMObjSub;
int llSamusSpecial2GrappleBeamAnimJoint;
int llSamusSpecial2GrappleBeamMatAnimJoint;

EFDesc dEFManagerSamusGrappleBeamEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    15,                                     // DL Link
    &gFTDataSamusSpecial2,                  // Texture file

    // DObj transformation struct 1
    {
        0x4F,                               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        0x2E,                               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    gcPlayAnimAll,                          // Proc Update (WHAT IS THIS FUNCTION???)
    gcDrawDObjTreeDLLinksForGObj,         // Proc Render

    &llSamusSpecial2GrappleBeamDObjDesc,               // DObj Setup attributes offset (?)
    &llSamusSpecial2GrappleBeamMObjSub,                 // MObjSub offset
    &llSamusSpecial2GrappleBeamAnimJoint,               // AnimJoint offset
    &llSamusSpecial2GrappleBeamMatAnimJoint             // MatAnimJoint offset
};

/* ---- efmanager.c:4622-4641 efManagerSamusGrappleBeamGlowMakeEffect,
 * verbatim: the glow, following the beam's joint. */
GObj* efManagerSamusGrappleBeamGlowMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj = efManagerMakeEffectNoForce(&dEFManagerSamusGrappleBeamEffectDesc);
    EFStruct *ep;
    DObj *dobj;

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->fighter_gobj = fighter_gobj;

    dobj = DObjGetStruct(effect_gobj);

    dobj->user_data.p = ftGetStruct(fighter_gobj)->joints[23];

    return effect_gobj;
}

/* ---- V10: the entry Poke Ball ----
 *
 * ft/ftcommon/ftcommonentry.c's appear setter throws this in for Pikachu
 * and Jigglypuff. The EFDesc (efmanager.c:1282-1309) and the ProcUpdate
 * (5230-5246) are verbatim; the maker (5249-5284) diverges on the lines
 * that find the model, as Kirby's entry star's does. The model is a pack
 * (tools/export/ssb_effectexport.py --what mballthrown) named by
 * sEFManagerModels, and the four reloc symbols the EFDesc names are
 * defined at zero, as every other effect's are. ---- */
int llITCommonDataMBallThrownDObjDesc;
int llITCommonDataMBallThrownMObjSub;
int llITCommonDataMBallThrownLAnimJoint;
int llITCommonDataMBallThrownLMatAnimJoint;
/* it/itmanager.c's, the file the game finds the ball's through */
extern void *gITManagerCommonData;

EFDesc dEFManagerMBallThrownEffectDesc =
{
    0x4 | EFFECT_FLAG_USERDATA,             // Flags
    20,                                     // DL Link
    &gITManagerCommonData,                           // Texture file

    // DObj transformation struct 1
    {
        0x44,                               // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTraRotRpyRSca,      // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerMBallThrownProcUpdate,                // Proc Update
    gcDrawDObjTreeForGObj,                // Proc Render

    &llITCommonDataMBallThrownDObjDesc,               // DObj Setup attributes offset (?)
    &llITCommonDataMBallThrownMObjSub,                 // MObjSub offset
    &llITCommonDataMBallThrownLAnimJoint,              // AnimJoint offset
    &llITCommonDataMBallThrownLMatAnimJoint            // MatAnimJoint offset
};

/* ---- efmanager.c:5249-5284 efManagerMBallThrownMakeEffect: the ball at
 * the entry point, sweeping in from the side the fighter faces. */
// 0x80102D14
GObj* efManagerMBallThrownMakeEffect(Vec3f *pos, s32 lr)
{
    GObj *effect_gobj;
    DObj *dobj;

    /* DIVERGES: the file_head arithmetic and the R/L block offsets written
     * into the shared descriptor (efmanager.c:5256-5271) become the index
     * of the pack's animation and MatAnimJoint pair, R first. */
    if (lr == +1)
    {
        sEFManagerAnimAlt = sEFManagerMatAnimAlt = 0;
    }
    else
    {
        sEFManagerAnimAlt = sEFManagerMatAnimAlt = 1;
    }
    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerMBallThrownEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f = *pos;

    efManagerSortZNeg(dobj->child);

    return effect_gobj;
}

/* ---- efmanager.c:5230-5246 efManagerMBallThrownProcUpdate, verbatim:
 * the ball changes DL link as it passes z 1000. */
// 0x80102C84
void efManagerMBallThrownProcUpdate(GObj *effect_gobj)
{
    DObj *dobj = DObjGetStruct(effect_gobj)->child;

    if (dobj->translate.vec.f.z > 1000.0F)
    {
        if (dobj->parent_gobj->dl_link_id != 20)
        {
            gcMoveGObjDL(dobj->parent_gobj, 20, 2);
        }
    }
    else if (dobj->parent_gobj->dl_link_id != 10)
    {
        gcMoveGObjDL(dobj->parent_gobj, 10, 2);
    }
    efManagerHaveStructProcUpdate(effect_gobj);
}

/* ---- V10: Kirby's stars ----
 *
 * ft/ftcommon/ftcommoncapturekirby.c spits a swallowed fighter out inside
 * the first, and ft/ftchar/ftkirby/ftkirbyspecialn.c throws the second
 * off when Kirby loses a copy power. The EFDescs (efmanager.c:1588-1615,
 * 1618-1645) and both ProcUpdates (5837-5876, 5919-5954) are verbatim;
 * the makers (5879-5916, 5957-5991) drop only the lines that find the
 * model through the Star Rod's attributes. The pack is efkirbystar.mdl
 * (tools/export/ssb_effectexport.py --what kirbystar): the bare root the flags
 * 0x1 arm makes and the display list on its child. The reloc symbols the
 * copies name are defined at zero, as every other effect's are, and the
 * FGM call has the port's own declaration (src/dc/ftcommon.h's). ---- */
extern alSoundEffect *func_800269C0_275C0(u32 fgm_id);
int llITCommonDataKirbyStarDObjDesc;

EFDesc dEFManagerCaptureKirbyStarEffectDesc =
{
    EFFECT_FLAG_USERDATA | 0x1,             // Flags
    15,                                     // DL Link
    &gITManagerCommonData,                           // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTra,                // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        0x50,                               // Main matrix transformations
        0x2E,                               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerCaptureKirbyStarProcUpdate,    // Proc Update
    lbCommonDObjScaleXProcDisplay,                     // Proc Render

    &llITCommonDataKirbyStarDObjDesc,                 // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

EFDesc dEFManagerLoseKirbyStarEffectDesc =
{
    EFFECT_FLAG_USERDATA | 0x1,             // Flags
    15,                                     // DL Link
    &gITManagerCommonData,                           // Texture file

    // DObj transformation struct 1
    {
        nGCMatrixKindTra,                // Main matrix transformations
        nGCMatrixKindNull,               // Secondary matrix transformations
        0x00                                // ???
    },

    // DObj transformation struct 2
    {
        nGCMatrixKindTra,                // Main matrix transformations
        0x2E,                               // Secondary matrix transformations
        0x00                                // ???
    },

    efManagerLoseKirbyStarProcUpdate,       // Proc Update
    lbCommonDObjScaleXProcDisplay,                     // Proc Render

    &llITCommonDataKirbyStarDObjDesc,                 // DObj Setup attributes offset (?)
    0x0,                                    // MObjSub offset
    0x0,                                    // AnimJoint offset
    0x0                                     // MatAnimJoint offset
};

/* ---- efmanager.c:5879-5916 efManagerCaptureKirbyStarMakeEffect: the
 * star round the fighter, scaled to its kind, following its TopN. */
// 0x80103CF8
GObj* efManagerCaptureKirbyStarMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj;
    EFStruct *ep;
    DObj *dobj;
    FTKirbyCopy *copy;

    copy = lbRelocGetFileData(FTKirbyCopy*, gFTDataKirbyMainMotion, &llKirbyMainMotionSpecialNFTKirbyCopy);

    /* DIVERGES: the file_head arithmetic through the Star Rod's attributes
     * (efmanager.c:5888-5891) is gone -- the model is the pack's. */
    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerCaptureKirbyStarEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);
    ep->fighter_gobj = fighter_gobj;

    dobj = DObjGetStruct(effect_gobj);

    dobj->translate.vec.f.y += EFCOMMON_CAPTUREKIRBYSTAR_SPARK_OFF_Y;

    dobj->child->user_data.p = ftGetStruct(fighter_gobj)->joints[nFTPartsJointTopN];

    dobj->child->scale.vec.f.x = dobj->child->scale.vec.f.y = copy[ftGetStruct(fighter_gobj)->fkind].effect_scale;
    dobj->child->scale.vec.f.z = 1.0F;

    ep->effect_vars.capture_kirby_star.effect_timer = 0;

    return effect_gobj;
}

/* ---- efmanager.c:5837-5876 efManagerCaptureKirbyStarProcUpdate,
 * verbatim: the star spins and throws Star Rod sparks behind it. */
// 0x80103B28
void efManagerCaptureKirbyStarProcUpdate(GObj *effect_gobj)
{
    DObj *topn_dobj;
    EFStruct *ep;
    FTStruct *fp;
    FTKirbyCopy *copy;
    Vec3f pos;
    DObj *child_dobj;

    ep = efGetStruct(effect_gobj);
    fp = ftGetStruct(ep->fighter_gobj);
    topn_dobj = DObjGetStruct(effect_gobj);

    copy = lbRelocGetFileData(FTKirbyCopy*, gFTDataKirbyMainMotion, &llKirbyMainMotionSpecialNFTKirbyCopy);

    child_dobj = topn_dobj->child;

    topn_dobj->translate.vec.f.z = 0.0F;

    child_dobj->rotate.vec.f.z += EFCOMMON_CAPTUREKIRBYSTAR_ROTATE_STEP;

    if (ep->effect_vars.capture_kirby_star.effect_timer % EFCOMMON_CAPTUREKIRBYSTAR_SPARK_TIMER_MOD)
    {
        pos = DObjGetStruct(ep->fighter_gobj)->translate.vec.f;

        pos.y += syUtilsRandIntRange(copy[fp->fkind].effect_scale * EFCOMMON_CAPTUREKIRBYSTAR_SPARK_SCATTER_Y);

        if (fp->physics.vel_air.x > 0.0F)
        {
            pos.x -= syUtilsRandIntRange(copy[fp->fkind].effect_scale * EFCOMMON_CAPTUREKIRBYSTAR_SPARK_SCATTER_X);
            efManagerStarRodSparkMakeEffect(&pos, -1);
        }
        else
        {
            pos.x += syUtilsRandIntRange(copy[fp->fkind].effect_scale * EFCOMMON_CAPTUREKIRBYSTAR_SPARK_SCATTER_X);
            efManagerStarRodSparkMakeEffect(&pos, +1);
        }
    }
    ep->effect_vars.capture_kirby_star.effect_timer++;
}

/* ---- efmanager.c:5957-5991 efManagerLoseKirbyStarMakeEffect: the lost
 * copy flies up and away from Kirby's back. */
// 0x80103F78
GObj* efManagerLoseKirbyStarMakeEffect(GObj *fighter_gobj)
{
    GObj *effect_gobj;
    EFStruct *ep;
    DObj *dobj;

    /* DIVERGES: the file_head arithmetic through the Star Rod's attributes
     * (efmanager.c:5965-5968) is gone -- the model is the pack's. */
    effect_gobj = efManagerMakeEffectNoForce(&dEFManagerLoseKirbyStarEffectDesc);

    if (effect_gobj == NULL)
    {
        return NULL;
    }
    ep = efGetStruct(effect_gobj);

    ep->effect_vars.lose_kirby_star.vel.x = ftGetStruct(fighter_gobj)->lr * EFCOMMON_LOSEKIRBYSTAR_VEL_X;
    ep->effect_vars.lose_kirby_star.vel.y = EFCOMMON_LOSEKIRBYSTAR_VEL_Y;

    ep->effect_vars.lose_kirby_star.lifetime = EFCOMMON_LOSEKIRBYSTAR_LIFETIME;

    ep->effect_vars.lose_kirby_star.lr = ftGetStruct(fighter_gobj)->lr;

    dobj = DObjGetStruct(effect_gobj);
    dobj->translate.vec.f.y += EFCOMMON_LOSEKIRBYSTAR_OFF_Y;

    dobj->child->translate.vec.f = DObjGetStruct(fighter_gobj)->translate.vec.f;

    return effect_gobj;
}

/* ---- efmanager.c:5919-5954 efManagerLoseKirbyStarProcUpdate, verbatim:
 * it falls, spinning, and bursts into the splash when its time is up. */
// 0x80103DF8
void efManagerLoseKirbyStarProcUpdate(GObj *effect_gobj)
{
    EFStruct *ep = efGetStruct(effect_gobj);
    DObj *dobj = DObjGetStruct(effect_gobj)->child;
    Vec3f *translate = &dobj->translate.vec.f;

    dobj->rotate.vec.f.z += F_CLC_DTOR32(10.0F);

    dobj->translate.vec.f.x += ep->effect_vars.lose_kirby_star.vel.x;
    dobj->translate.vec.f.y += ep->effect_vars.lose_kirby_star.vel.y;

    ep->effect_vars.lose_kirby_star.vel.y -= EFCOMMON_LOSEKIRBYSTAR_GRAVITY;

    if (ep->effect_vars.lose_kirby_star.vel.y < EFCOMMON_LOSEKIRBYSTAR_TVEL)
    {
        ep->effect_vars.lose_kirby_star.vel.y = EFCOMMON_LOSEKIRBYSTAR_TVEL;
    }
    if (ep->effect_vars.lose_kirby_star.lifetime-- <= 0)
    {
        func_800269C0_275C0(nSYAudioFGMKirbyStarPing1);
        efManagerStarSplashMakeEffect(translate, ep->effect_vars.lose_kirby_star.lr);
        efManagerSetPrevStructAlloc(ep);
        gcEjectGObj(effect_gobj);
    }
    else if
    (
        (gMPCollisionGroundData->map_bound_bottom > translate->y) ||
        (gMPCollisionGroundData->map_bound_right  < translate->x) ||
        (gMPCollisionGroundData->map_bound_left   > translate->x) ||
        (gMPCollisionGroundData->map_bound_top    < translate->y)
    )
    {
        efManagerSetPrevStructAlloc(ep);
        gcEjectGObj(effect_gobj);
    }
}
