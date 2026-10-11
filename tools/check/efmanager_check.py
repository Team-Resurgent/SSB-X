#!/usr/bin/env python3
"""The port's effect manager against the decomp's, two ways.

src/dc/efmanager.c is ef/efmanager.c with three functions changed and
109 of its 158 left out. That makes it a copy, and a copy is only worth
what checks it.

  text     every line the port took is the decomp's line, character for
           character, in the runs listed below. The runs are what the port
           claims to have taken -- the manager's data and EFDescs, its
           four file-scope variables, the EFStruct pool and the procs
           above it, the three functions that give a struct back, the
           eleven makers that reach straight for lb/lbparticle.c, and the
           model-path effects that have a pack. Between them the copy may
           have comments and nothing else: a line of code that appeared
           between two runs would be a divergence nobody wrote down, and
           this is what says so. The three functions the port edited are
           deliberately outside every run and checked line by line.

  scripts  every particle script id the copy names, read back out of the
           copy itself rather than listed here, against the exported
           efcommon bank. A maker asking for script 0x65 is asking the
           interpreter to index sLBParticleScriptBanks[0][0x65], and
           lbParticleMakeChildScriptID's only guard is against running
           off the end -- a wrong id inside the bank is a different
           effect, silently. This says each one exists, says which
           texture and lifetime it has, and says the texture is in the
           bank too.

What this does not do is run the manager against the decomp's. It could
be built, but it would not be worth reading: the text leg says the two
files are the same text, the makers call nothing but lb/lbparticle.c,
and tools/check/lbparticle_check.py already runs that file against its own
original over all 119 scripts of this bank. The step's own test is
src/game/ssb64/hosttest/effects.c test_effect_manager, which is about the
thing neither of those can see -- what the pool does when a maker fails.

Usage: python3 tools/check/efmanager_check.py [-v]
"""
import argparse
import os
import re
import struct
import sys

sys.path[:0] = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), d)
                for d in ("lib", "export", "check")]
import ssb_particleexport as PE     # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, "src", "dc")
DECOMP = os.environ.get("SSB_DECOMP_DIR", "/opt/ssb-decomp-re")
if not os.path.isdir(DECOMP):
    DECOMP = os.path.join(ROOT, "ssb-decomp-re")

PORT_FILE = os.path.join(SRC, "efmanager.c")
DECOMP_FILE = os.path.join(DECOMP, "src", "ef", "efmanager.c")

# The runs of ef/efmanager.c the port took verbatim, 1-based and
# inclusive, in the order they appear in both files. tools/ is the only
# place these numbers are written down twice; src/dc/efmanager.c says the
# same in prose.
RUNS = [
    (14, 18),       # 0             the sparks' and the metal dust's angle tables
    (20, 36),       # 1             the DamageNormalHeavy colour tables
    (39, 45),       # 2             the impact wave's three prim colour rows
    (57, 73),       # 3             the dead explosion's six per-player colour tables
    (74, 75),       # 4             dEFManagerDeadExplodeRotateD
    (77, 78),       # 5             dEFManagerDamageNormalLightIDs
    (80, 108),      # 6             dEFManagerDamageSlashEffectDesc
    (140, 198),     # 7             the damage orbs' two EFDescs
    (201, 228),     # 8             dEFManagerImpactWaveEffectDesc
    (230, 258),     # 8b            dEFManagerStarRodSparkEffectDesc
    (260, 318),     # 9             the damage sparks' two EFDescs
    (320, 378),     # 10 the metal dust's two EFDescs
    (410, 447),     # 11 the Fox Reflector's offsets and EFDesc
    (449, 517),     # 12 the shield's colours and its two EFDescs
    (760, 787),     # 13 dEFManagerCaptainFalconKickEffectDesc
    (790, 817),     # 14 dEFManagerCaptainFalconPunchEffectDesc
    (819, 847),     # 15 dEFManagerPurinSingEffectDesc
    (580, 607),   # dEFManagerPikachuUnkEffectDesc
    (639, 667),     # 16 dEFManagerPikachuThunderTrailEffectDesc
    (669, 697),     # 17 dEFManagerThunderJoltEffectDesc
    (1524, 1552),   # 18 dEFManagerMarioEntryDokanEffectDesc
    (1434, 1462),   # 19 dEFManagerDonkeyEntryTaruEffectDesc
    (1464, 1492),   # 20 dEFManagerSamusEntryPointEffectDesc
    (1222, 1250),   # 21 dEFManagerKirbyEntryStarEffectDesc
    (1161, 1189),   # 22 dEFManagerLinkEntryWaveEffectDesc
    (1191, 1219),   # 23 dEFManagerLinkEntryBeamEffectDesc
    (1312, 1339),   # 24 dEFManagerYoshiEntryEggEffectDesc
    (1555, 1582),   # 25 dEFManagerFoxEntryArwingEffectDesc
    (1495, 1522),   # 26 dEFManagerCaptainEntryCarEffectDesc
    (1344, 1372),   # 27 dEFManagerYoshiEggLayEffectDesc
    (1375, 1402),   # dEFManagerYoshiEggEscapeEffectDesc
    (699, 727),     # 28 dEFManagerVulcanJabEffectDesc
    (892, 1009),    # 29 the Final Cutter's four EFDescs
    (1405, 1432),   # 30 dEFManagerLinkSpinAttackEffectDesc
    (111, 138),   # dEFManagerShockSmallEffectDesc
    (381, 408),   # dEFManagerFireSparkEffectDesc
    (850, 877),     # 31 dEFManagerDeadExplodeEffectDesc
    (879, 889),     # 32 its generator ids and its four MatAnimJoints
    (1646, 1675),   # 33 dEFManagerRebirthHaloEffectDesc
    (1252, 1279),   # 33b dEFManagerMBallRaysEffectDesc
    (1677, 1705),   # 33c dEFManagerItemGetSwirlEffectDesc (item-use step)
    (1712, 1723),   # 34 gEFManagerFiles, the pool head and count, the bank id
    (1761, 1926),   # 35 the allocator, the four procs, the two sorters, FuncRun
    (2060, 2070),   # 36 efManagerMakeEffectNoForce, efManagerMakeEffectForce
    (2072, 2115),   # 37 DestroyParticleGObj, DefaultProcDead, DefaultProcUpdate
    (2117, 2259),   # 38 DamageNormalLight, DamageNormalHeavy
    (2261, 2327),   # 39 efManagerImpactShockMakeEffect
    (2349, 2483),   # 40 DamageFire, DamageElectric
    (2485, 2509),   # 41 efManagerDamageSlashMakeEffect
    (2704, 2769),   # 42 efManagerDustCollideMakeEffect
    (3078, 3136),   # 43 DustExpandSmall
    (3605, 3631),   # 44 efManagerSparkleWhiteMakeEffect
    (3663, 3689),   # 45 efManagerSparkleWhiteMultiExplodeMakeEffect
    (5287, 5313),   # 46 efManagerFireGrindMakeEffect
    (5517, 5530),   # 47 efManagerFoxBlasterGlowMakeEffect
    (3182, 3278),   # 48 the damage orbs' two procs and two makers
    (3302, 3358),   # 49 the impact wave's ProcUpdate and two makers
    (3359, 3411),   # 49b the star rod's ProcUpdate and MakeEffect
    (3413, 3521),   # 50 the damage sparks' two procs and two makers
    (3523, 3602),   # 51 the metal dust's proc and its two makers
    (3724, 3755),   # 52 SparkleWhiteDead
    (3756, 3811),   # 53 efManagerQuakeProcUpdate, efManagerQuakeFuncRun
    (3862, 3995),   # 54 DamageCoin, SetOff
    (4039, 4092),   # 55 the Fox Reflector's ProcUpdate, MakeEffect
    (4094, 4103),   # 56 efManagerShieldProcUpdate
    (4118, 4145),   # 57 efManagerShieldMakeEffect
    (4171, 4199),   # 58 efManagerYoshiShieldMakeEffect
    (5426, 5456),   # efManagerYoshiEggEscapeMakeEffect
    (4644, 4673),   # 59 efManagerCaptainFalconKickMakeEffect
    (4676, 4704),   # 60 efManagerCaptainFalconPunchMakeEffect
    (4721, 4732),   # 61 efManagerStarSplashMakeEffect
    (4734, 4774),   # 62 efManagerPurinSingMakeEffect
    (4371, 4383), # func_ovl2_801017E8
    (4386, 4406), # func_ovl2_8010183C
    (4461, 4485),   # 63 efManagerPikachuThunderTrailProcUpdate
    (4505, 4533),   # 64 efManagerPikachuThunderTrailMakeEffect
    (4535, 4549),   # 65 efManagerPikachuThunderJoltMakeEffect
    (5669, 5696),   # 66 efManagerMarioEntryDokanMakeEffect
    (5564, 5581),   # 67 efManagerDonkeyEntryTaruMakeEffect
    (5583, 5600),   # 68 efManagerSamusEntryPointMakeEffect
    (5151, 5168),   # 69 efManagerLinkEntryWaveMakeEffect
    (5170, 5187),   # 70 efManagerLinkEntryBeamMakeEffect
    (5345, 5361),   # 71 efManagerYoshiEntryEggMakeEffect
    (5603, 5620),   # 72 efManagerCaptainEntryCarProcUpdate
    (5373, 5388),   # 73 efManagerYoshiEggLayProcUpdate
    (4850, 4949),   # 74 the Final Cutter's four makers
    (4551, 4619),   # 75 Kirby's Vulcan Jab, proc and maker
    (5533, 5562),   # 76 efManagerLinkSpinAttackMakeEffect
    (5993, 6018),   # 77 efManagerRebirthHaloMakeEffect
    (5211, 5227),   # 77b efManagerMBallRaysMakeEffect
    (6158, 6174),   # 77c efManagerItemGetSwirlProcUpdate (item-use step)
    (6047, 6074),   # 78 efManagerEggBreakMakeEffect
    (1584, 1585),   # 79 dEFManagerMusicNoteScriptIDs
    (2329, 2347),   # 80 efManagerVelAddDestroyAnimEnd
    (2511, 2702),   # 81 the three flames
    (2824, 3076),   # 82 the dusts: light, heavy, heavy-double, expand-large (s96)
    (3138, 3180),   # 83 efManagerDustDashMakeEffect
    (3633, 3660),   # 84 efManagerSparkleWhiteMultiMakeEffect
    (3691, 3722),   # 85 efManagerSparkleWhiteScaleMakeEffect
    (4201, 4242),   # 86 the thunder amp and the ripple
    (4286, 4368),   # 87 FuraSparkle, Psionic, the three flashes, ShieldBreak (s96)
    (4706, 4718),   # 88 efManagerKirbyStarMakeEffect
    (5315, 5342),   # 89 efManagerHealSparklesMakeEffect
    (5747, 5805),   # 90 the stock icons and the music note
    (6019, 6045),   # 91 efManagerBattleScoreMakeEffect
    (6176, 6203),   # 92 efManagerItemSpawnSwirlMakeEffect
    (5808, 5834),   # 93 efManagerYoshiEggExplodeMakeEffect
    (6077, 6086),   # 94 efManagerKirbyInhaleWindProcUpdate
    (6089, 6155),   # 95 efManagerKirbyInhaleWindMakeEffect
    (6205, 6234),   # 96 efManagerConfettiMakeEffect
    (5459, 5514),   # 97 the two unnamed Kirby-bank arms
    (1011, 1129),   # 98 Ness's four EFDescs
    (4951, 5009),   # 99 PSI Magnet's maker, the PK Thunder trail's ProcUpdate
    (5025, 5130),   # 100 the two trails' makers, the reflect ProcUpdate, the wave
    (550, 577),     # 114 dEFManagerReflectBreakEffectDesc
    (4264, 4284),   # 115 efManagerReflectBreakMakeEffect
    (610, 637),     # 116 dEFManagerPikachuThunderShockEffectDesc
    (520, 547),     # 117 dEFManagerCatchSwirlEffectDesc
    (4245, 4261),   # 118 efManagerCatchSwirlMakeEffect
    (730, 757),     # 119 dEFManagerSamusGrappleBeamEffectDesc
    (4622, 4641),   # 120 efManagerSamusGrappleBeamGlowMakeEffect
    (1282, 1309),   # 121 dEFManagerMBallThrownEffectDesc
    (5230, 5246),   # 122 efManagerMBallThrownProcUpdate
    (1588, 1615),   # 123 dEFManagerCaptureKirbyStarEffectDesc
    (1618, 1645),   # 124 dEFManagerLoseKirbyStarEffectDesc
    (5837, 5876),   # 125 efManagerCaptureKirbyStarProcUpdate
    (5919, 5954),   # 126 efManagerLoseKirbyStarProcUpdate
]
# What the copy is allowed to have between two runs beside comments, by
# the index of the run it comes before. Everywhere else, a line of code
# between runs is a divergence nobody wrote down.
#
# THE INDICES MOVE WHEN A RUN IS INSERTED, and this dict and DIVERGED's
# `chunk` fields are the two places that say so in numbers. Adding a run at
# position N means every index >= N here and in DIVERGED must go up by one
# -- which is what two additions (dEFManagerMBallRays-
# EffectDesc at 39 and efManagerMBallRaysMakeEffect at 88) needed, and
# getting it half right shows up as a page of "is between runs and is not a
# comment" over code that was always allowed. The run a chunk's index names
# is the one that comes AFTER it, so a key is right when the port text from
# the previous run's end to that run's start is the code named here.
DIVERGE_CHUNKS = {
    0: "the copy's own includes",
    6: "the slash's four reloc symbols, defined at zero",
    7: "the orbs' two reloc symbols, the same",
    8: "the impact wave's four reloc symbols, the same",
    9: "the CommonSpark's four reloc symbols, the same",
    11: "the DamageFlyMDust's four reloc symbols, the same",
    12: "the Reflector's five reloc symbols, the same; its "
        "file pointer is ftfox.c's own",
    13: "the shield's one file pointer and two reloc symbols, the same "
        "(gFTDataYoshiModel left for ft/ftchar/ftyoshi/ftyoshi.c)",
    14: "gFTDataCaptainSpecial2's stub global and Falcon Kick's four "
        "reloc symbols, the same",
    15: "gFTDataCaptainSpecial3's stub global and Falcon Punch's three "
        "reloc symbols, the same",
    16: "gFTDataPurinSpecial2's extern and Sing's four reloc symbols, the "
        "same",
    17: "the Pikachu Unk effect's four reloc symbols, defined at zero",
    18: "the Thunder Jolt's four reloc symbols and the Thunder trail's "
        "two, defined at zero, and the comment that says whose file each "
        "block is in",
    20: "Mario's warp pipe's two reloc symbols, defined at zero",
    21: "Donkey Kong's barrel's and Samus's point's four reloc "
        "symbols, defined at zero",
    23: "Kirby's entry star's three reloc symbols, defined at zero "
        "-- two of them AnimJoints",
    24: "Link's entry wave's and beam's eight reloc symbols, "
        "defined at zero",
    26: "Yoshi's entry egg's four reloc symbols, defined at zero",
    27: "Fox's Arwing's one reloc symbol, defined at zero, and the "
        "note that its two AnimJoint arrays are not in the file its "
        "tree is in",
    28: "Captain Falcon's Blue Falcon's one reloc symbol, defined at "
        "zero, and the note that its flags lack 0x1",
    29: "Yoshi's Egg Lay egg's two reloc symbols, defined at zero, "
        "and dEFManagerYoshiEggLayAnimJoints -- two pack indexes "
        "where the game has two block pointers",
    31: "Kirby's Vulcan Jab's one reloc symbol, defined at zero",
    32: "the Final Cutter's seven reloc symbols, defined at zero -- "
        "four trees and three AnimJoints, because the blade has none",
    33: "Link's Spin Attack glow's four reloc symbols, defined at "
        "zero",
    34: "the small shock burst's three reloc symbols, defined at zero",
    35: "the fire spark's four reloc symbols, defined at zero",
    36: "the dead explosion's eight reloc symbols, the same -- two of them "
        "named the wrong way round, which the copy says out loud",
    38: "the halo's two reloc symbols, the same",
    42: "efManagerInitEffects, checked line by line below, plus the port's "
        "own efManagerFixupModelProcDisplays beside it",
    43: "efManagerMakeEffect and the model bridge under it",
    56: "the impact wave's two model helpers, the port's own -- the row "
        "colour and a flat-black env onto the shared pack, and the model "
        "draw -- and its ProcDisplay, checked line by line below",
    62: "efManagerQuakeMakeEffect, checked line by line below",
    63: "efManagerFoxReflectorSetAnimID, checked line by line below",
    64: "the shield's three model helpers, the port's own: the two live "
        "colours onto the shared pack and the draw",
    65: "efManagerShieldProcDisplay, checked line by line below",
    66: "efManagerYoshiShieldProcDisplay, checked line by line below",
    75: "efManagerPikachuThunderTrailProcDisplay, checked line by line "
        "below",
    80: "efManagerKirbyEntryStarMakeEffect, checked line by line "
        "below. It sat inside the dead explosion's "
        "chunk until step 83 put two runs between them, which is "
        "the same accident step 68 had: a copy landing in a chunk "
        "that was already exempt for something else.",
    83: "the Arwing's ProcUpdate and MakeEffect, with the port's own "
        "efManagerArwingBillboard and efManagerArwingAnim beside them "
        "-- both copied ones checked line by line below",
    84: "the Blue Falcon's MakeEffect and "
        "efManagerYoshiEggLaySetAnim, with the port's own "
        "efManagerCarWheelXObjs beside them -- both copied ones checked "
        "line by line below",
    85: "efManagerYoshiEggLayMakeEffect, checked line by line below",
    89: "the tail of efManagerRebirthHaloMakeEffect -- the port's own "
        "child-scale lines, which the run above stops short of because "
        "the decomp writes them without the local the port needs -- and "
        "the comment over the Poke Ball's rays maker",
    88: "efManagerDeadExplodeMakeEffect, checked line by line below",
    111: "Ness's nine reloc symbols, defined at zero",
    114: "the reflector shards' four reloc symbols, defined at zero",
    116: "the Thunder Shock's four reloc symbols, defined at zero, and "
         "efManagerPikachuThunderShockMakeEffect, checked line by line "
         "below",
    117: "the grab swirl's four reloc symbols, defined at zero",
    119: "the grapple beam glow's four reloc symbols, defined at zero",
    121: "the entry Poke Ball's four reloc symbols, defined at zero",
    122: "efManagerMBallThrownMakeEffect, checked line by line below",
    123: "Kirby's stars' reloc symbol, defined at zero, and the FGM "
         "call's declaration",
    125: "efManagerCaptureKirbyStarMakeEffect, checked line by line below",
    126: "efManagerLoseKirbyStarMakeEffect, checked line by line below",
    113: "efManagerNessPKThunderTrailProcDisplay, checked line by line "
         "below",
    109: "efManagerLoadEffectBank, the port's own: the wrapper that times "
        "efManagerInitEffects and prints what the bank walk found, called "
        "by the battle and the results screen",
}
# The functions the port edited rather than copied. Each is checked line
# by line rather than described: the lines it dropped all have to match
# `drop`, there have to be `drops` of them, and any line it added that is
# not a comment has to match `add`. That is what keeps a divergence from
# growing quietly.
#
#   efManagerInitEffects drops the three lbRelocGetExternHeapFile calls
#   that read the effect model files, because nothing in the copy reads
#   gEFManagerFiles. It is why the run at index 9 starts at 1761 rather
#   than 1731.
#
#   efManagerQuakeMakeEffect's switch reads an AnimJoint table out of
#   llEFCommonEffects1 four times; the port has neither the file nor the
#   offsets, so each call becomes efQuakeAnimJoint(magnitude) over the
#   pack the same four blocks were baked into.
#
#   efManagerDeadExplodeMakeEffect drops nothing at all: it keeps the
#   assignment to the descriptor's o_matanim_joint, table and all, and
#   adds one line beside it. The game picks a MatAnimJoint by writing a
#   block offset into the shared EFDesc; the port's offsets are zero and
#   the four blocks are baked side by side into efdexp.mdl, so the port
#   also says which of them by index (sEFManagerMatAnimAlt).
#
#   efManagerShieldProcDisplay drops its three gDP lines -- the pipe sync
#   and the two colours -- for one call that puts the same two colours on
#   the shared pack; the draw line is the decomp's. efManagerYoshiShield-
#   ProcDisplay drops four: the same two gDP lines, and gcDrawDObjDLHead1
#   with the efDisplayCLDProcDisplay that sets its render mode, for the
#   port's model draw. `drop` may name several substrings.
DIVERGED = [
    {"name": "efManagerInitEffects", "span": (1731, 1759), "chunk": 42,
     "drop": "lbRelocGetExternHeapFile", "drops": 3,
     "add": "efManagerFixupModelProcDisplays"},
    {"name": "efManagerImpactWaveProcDisplay", "span": (3281, 3300),
     "chunk": 56, "drop": ("gDP", "gcDrawDObjDLHead0", "EnvColor",
                           "#if", "#else", "#endif"),
     "drops": 10, "add": "efManagerImpactWaveModel"},
    {"name": "efManagerQuakeMakeEffect", "span": (3812, 3861), "chunk": 62,
     "drop": "lbRelocGetFileData", "drops": 4, "add": "efQuakeAnimJoint("},
    {"name": "efManagerShieldProcDisplay", "span": (4105, 4116),
     "chunk": 65, "drop": ("gDP", "gcDrawDObjTreeDLLinksForGObj"),
     "drops": 4, "add": "efManagerShieldModel"},
    {"name": "efManagerYoshiShieldProcDisplay", "span": (4147, 4169),
     "chunk": 66, "drop": ("gDP", "gcDrawDObjDLHead1",
                           "efDisplayCLDProcDisplay"),
     "drops": 4, "add": "efManagerShieldModel"},
    {"name": "efManagerPikachuThunderTrailProcDisplay", "span": (4487, 4503),
     "chunk": 75, "drop": "gDP", "drops": 6, "add": None},
    {"name": "efManagerNessPKThunderTrailProcDisplay", "span": (5011, 5023),
     "chunk": 113, "drop": "gDP", "drops": 6, "add": None},
    {"name": "efManagerKirbyEntryStarMakeEffect", "span": (5189, 5208),
     "chunk": 80, "drop": "o_anim_joint", "drops": 1,
     "add": "sEFManagerAnimAlt"},
    {"name": "efManagerDeadExplodeMakeEffect", "span": (4776, 4842),
     "chunk": 88, "drop": None, "drops": 0,
     "add": "sEFManagerMatAnimAlt"},
    {"name": "efManagerFoxEntryArwingProcUpdate", "span": (5699, 5711),
     "chunk": 83, "drop": "DObjGetStruct(effect_gobj)->child", "drops": 1,
     "add": "DObjGetStruct(effect_gobj);"},
    {"name": "efManagerFoxEntryArwingMakeEffect", "span": (5714, 5745),
     "chunk": 83, "drop": ("DObj *what;", "dobj->child->child->child",
                           "gcAddXObjForDObjFixed", "gcAddDObjAnimJoint(",
                           "if (lr == +1)", "lbCommonAddDObjAnimJointAll",
                           "efManagerSortZNeg(dobj->child);"),
     "drops": 8, "add": ("efManagerArwing", "efManagerSortZNeg(dobj);")},
    {"name": "efManagerCaptainEntryCarMakeEffect", "span": (5622, 5667),
     "chunk": 84, "drop": ("DObj *node_dobj;", "s32 i;",
                           "lbRelocGetFileData", "node_dobj = dobj->child",
                           "for (i = nFTPartsJointCommonStart",
                           "gcAddXObjForDObjFixed",
                           "gcAddDObjAnimJoint(",
                           "node_dobj = node_dobj->sib_next;"),
     "drops": 10, "add": ("efModelAnimJoint", "efManagerCarWheelXObjs")},
    {"name": "efManagerYoshiEggLaySetAnim", "span": (5364, 5371),
     "chunk": 84, "drop": "lbRelocGetFileData", "drops": 1,
     "add": ("efModelAnimJoint", "lbCommonAddDObjAnimJointAll",
             "dEFManagerYoshiEggLayAnimJoints", "dc_model_of")},
    {"name": "efManagerYoshiEggLayMakeEffect", "span": (5391, 5423),
     "chunk": 85, "drop": ("void *unused;", "dobj->child->child",
                           "lbCommonSetDObjTransformsForTreeDObjs"),
     "drops": 4, "add": ("dobj->child", "if (", "}")},
    {"name": "efManagerShockSmallMakeEffect", "span": (2772, 2822),
     "chunk": 88, "drop": ("f32 angle;", "/*", "*/",
                           "The following float random",
                           "Guarded by the preprocessor",
                           "#if !defined (DAIRANTOU_OPT0)", "#endif",
                           "angle = syUtilsRandFloat", "__cosf(angle)",
                           "__sinf(angle)"),
     "drops": 14, "add": None},
    {"name": "efManagerPikachuThunderShockMakeEffect", "span": (4409, 4459),
     "chunk": 116, "drop": ("gcAddAnimAll", "        (", "effect_gobj, ",
                            "lbRelocGetFileData", "0.0F", "        );"),
     "drops": 14, "add": "sEFManagerAnimAlt"},
    {"name": "efManagerFoxReflectorSetAnimID", "span": (4028, 4037),
     "chunk": 63, "drop": "lbRelocGetFileData", "drops": 1,
     "add": "efModelAnimJoint"},
    {"name": "efManagerFireSparkMakeEffect", "span": (3998, 4026),
     "chunk": 88, "drop": "lbCommonSetDObjTransformsForTreeDObjs",
     "drops": 1, "add": None},
    {"name": "efManagerMBallThrownMakeEffect", "span": (5249, 5284),
     "chunk": 122, "drop": ("p_file", "void *file;", "file_head",
                            "o_anim_joint", "o_matanim_joint"),
     "drops": 9, "add": "sEFManagerAnimAlt"},
    {"name": "efManagerCaptureKirbyStarMakeEffect", "span": (5879, 5916),
     "chunk": 125, "drop": "addr", "drops": 5, "add": None},
    {"name": "efManagerLoseKirbyStarMakeEffect", "span": (5957, 5991),
     "chunk": 126, "drop": "addr", "drops": 5, "add": None},
]

LBSCRIPT_HEADER_SIZE = 0x30


def func_body(lines, sig):
    """`lines` from the one equal to `sig` to the next bare closing brace."""
    try:
        i = lines.index(sig)
    except ValueError:
        return None
    for j in range(i, len(lines)):
        if lines[j] == "}":
            return lines[i:j + 1]
    return None


def check_text() -> int:
    """Every line of the copy is the decomp's line."""
    with open(DECOMP_FILE) as f:
        src = f.read().splitlines()
    with open(PORT_FILE) as f:
        port = f.read().splitlines()

    at = 0
    spans = []
    for lo, hi in RUNS:
        want = src[lo - 1:hi]
        found = -1
        for i in range(at, len(port) - len(want) + 1):
            if port[i:i + len(want)] == want:
                found = i
                break
        if found < 0:
            # Say where it first stops matching, at the best start we can
            # find, rather than only that it does not.
            best, best_hits = at, -1
            for i in range(at, len(port) - len(want) + 1):
                hits = sum(1 for a, b in zip(port[i:i + len(want)], want)
                           if a == b)
                if hits > best_hits:
                    best, best_hits = i, hits
            for k, (a, b) in enumerate(zip(port[best:best + len(want)], want)):
                if a != b:
                    print(f"FAIL text: ef/efmanager.c:{lo + k} is")
                    print(f"       {b!r}")
                    print(f"     src/dc/efmanager.c:{best + k + 1} is")
                    print(f"       {a!r}")
                    break
            else:
                print(f"FAIL text: ef/efmanager.c:{lo}-{hi} is not in the copy")
            return 1
        spans.append((lo, hi, found, at))
        at = found + len(want)

    lines = sum(hi - lo + 1 for lo, hi in RUNS)
    print(f"text: {lines} lines of ef/efmanager.c in {len(RUNS)} runs, "
          f"character for character")

    # And nothing but comments between them. A run matched above is
    # contiguous in the copy; what the copy put *between* two runs is
    # where an edit would hide, and the only thing allowed there is the
    # prose that says which run comes next -- plus, once, the diverged
    # efManagerInitEffects, which is checked separately below.
    bad = 0
    for idx, (lo, hi, found, prev_end) in enumerate(spans):
        chunk = port[prev_end:found]
        if idx in DIVERGE_CHUNKS:
            continue
        in_comment = False
        for k, ln in enumerate(chunk):
            t = ln.strip()
            if in_comment:
                in_comment = "*/" not in t
                continue
            if not t or t.startswith("//"):
                continue
            if t.startswith("/*"):
                in_comment = "*/" not in t[2:]
                continue
            print(f"FAIL text: src/dc/efmanager.c:{prev_end + k + 1} is "
                  f"between runs and is not a comment:")
            print(f"       {ln!r}")
            bad = 1
    # And after the last run there is nothing at all.
    tail = [ln for ln in port[spans[-1][2] + (spans[-1][1] - spans[-1][0] + 1):]
            if ln.strip()]
    if tail:
        print(f"FAIL text: src/dc/efmanager.c has {len(tail)} lines after the "
              f"last run, starting {tail[0]!r}")
        bad = 1
    if bad:
        return 1
    print(f"text: nothing between the runs but comments and the "
          f"{len(DIVERGE_CHUNKS)} places the copy says it diverges")

    # The divergences, named rather than described.
    for d in DIVERGED:
        lo, hi = d["span"]
        want = [ln for ln in src[lo - 1:hi] if ln.strip()]
        chunk = spans[d["chunk"]]
        got = [ln for ln in port[chunk[3]:chunk[2]] if ln.strip()]
        # A chunk may hold the port's own helpers beside the diverged
        # function; compare the function alone, from its signature to the
        # closing brace in column 1.
        sig = next(ln for ln in want if ln.startswith(d["name"])
                   or (d["name"] + "(") in ln and not ln.startswith((" ", "\t")))
        want = func_body(want, sig)
        got = func_body(got, sig)
        if got is None:
            print(f"FAIL text: {d['name']} is not in the copy where the runs "
                  f"say it is")
            return 1
        dropped = [ln for ln in want if ln not in got]
        added = [ln for ln in got if ln not in want and
                 not ln.strip().startswith(("/*", "*", "//"))]
        if d["add"] is not None:
            adds = d["add"] if isinstance(d["add"], tuple) else (d["add"],)
            added = [ln for ln in added
                     if not any(x in ln for x in adds)]
        drops = d["drop"] if isinstance(d["drop"], tuple) else (d["drop"],)
        for ln in dropped:
            if not any(x is not None and x in ln for x in drops):
                print(f"FAIL text: {d['name']} dropped a line that is not "
                      f"a {'/'.join(map(str, drops))} call:")
                print(f"       {ln!r}")
                return 1
        if len(dropped) != d["drops"]:
            print(f"FAIL text: {d['name']} dropped {len(dropped)} lines, "
                  f"expected {d['drops']}")
            return 1
        if added:
            print(f"FAIL text: {d['name']} has a line the decomp's has not, "
                  f"outside a comment and outside its own divergence:")
            print(f"       {added[0]!r}")
            return 1
        if d["drops"]:
            print(f"text: {d['name']} is the decomp's but for "
                  f"{d['drops']} {'/'.join(map(str, drops))} lines")
        else:
            adds = d["add"] if isinstance(d["add"], tuple) else (d["add"],)
            print(f"text: {d['name']} is the decomp's line for line, plus "
                  f"the {'/'.join(adds)} line(s) it says it adds")
    return 0


def bank_scripts(scb: bytes):
    """The exported bank's scripts: (texture_id, particle_lifetime) each."""
    n = struct.unpack_from("<I", scb, 0)[0]
    offs = struct.unpack_from(f"<{n}I", scb, 4)
    out = []
    for off in offs:
        _, texture_id, _, lifetime = struct.unpack_from("<4H", scb, off)
        out.append((texture_id, lifetime))
    return out


# Which particle bank each bank-id global names, so a script id is
# checked against the bank it is actually read out of. Three of the four
# fighters' banks exist; Ness's is real and unloaded, and
# nothing in the copy names it, so it is here for the day it does.
BANK_OF = {
    "gEFManagerParticleBankID": "efcommon",
    "gFTDataKirbyParticleBankID": "particles_unk0",
    "gFTNessParticleBankID": "particles_unk1",
    "gFTDataYoshiParticleBankID": "particles_unk2",
}


def port_script_ids():
    """Every particle script id the copy names, with the BANK it names it
    in, out of the copy itself.

    Three shapes: the literal in a lbParticleMakeScriptID call, the
    four-entry table efManagerDamageNormalLightMakeEffect indexes with
    the attacker's player number, and the eight-entry one
    efManagerDeadExplodeMakeEffect indexes with the player *and* which
    way up the burst is.

    The bank matters: every id here
    was efcommon's, and checking a fighter-bank id against efcommon's
    script count is exactly the mistake loading no bank at all would have
    made at runtime -- bank 0 is efcommon and a wrong id resolves rather
    than failing."""
    with open(PORT_FILE) as f:
        text = f.read()
    ids = []
    for m in re.finditer(r"lbParticleMake(?:ScriptID|Common)"
                         r"\(\s*(g[A-Za-z0-9_]*ParticleBankID)"
                         r"[^;]*?,\s*(0x[0-9A-Fa-f]+|\d+)\)", text):
        bank = BANK_OF.get(m.group(1))
        if bank is None:
            sys.exit("efmanager_check: %s names no bank this tool knows"
                     % m.group(1))
        ids.append((bank, int(m.group(2), 0), "lbParticleMakeScriptID"))
    for name in ("dEFManagerDamageNormalLightIDs",
                 "dEFManagerDeadExplodeGenID"):
        m = re.search(name + r"\[[^\]]*\]\s*=\s*\{([^}]*)\}", text)
        if m:
            for v in m.group(1).split(","):
                ids.append(("efcommon", int(v.strip(), 16), name))
    return sorted(set(ids))


def check_scripts(verbose: bool) -> int:
    rom_path = PE.ROM_DEFAULT
    if not os.path.exists(rom_path):
        print("no baserom -- the text check ran, the scripts check needs "
              "the bank")
        return 0
    with open(rom_path, "rb") as f:
        rom = f.read()
    ids = port_script_ids()
    if not ids:
        print("FAIL scripts: the copy names no script ids at all")
        return 1
    banks = {}
    for bank in sorted({b for b, _, _ in ids}):
        scb = PE.export(rom, bank, "scb")
        txb = PE.export(rom, bank, "txb")
        banks[bank] = (bank_scripts(scb),
                       struct.unpack_from("<I", txb, 0)[0])
    for bank, sid, where in ids:
        scripts, textures_num = banks[bank]

        if sid >= len(scripts):
            print(f"FAIL scripts: {where} asks for script {sid:#x} and the "
                  f"{bank} bank has {len(scripts)}")
            return 1
        texture_id, lifetime = scripts[sid]
        if texture_id >= textures_num:
            print(f"FAIL scripts: {bank} script {sid:#x} wants texture "
                  f"{texture_id} and the bank has {textures_num}")
            return 1
        if verbose:
            print(f"scripts:   {bank} {sid:#04x}  texture {texture_id:2d}  "
                  f"lifetime {lifetime:4d}  ({where})")
    print("scripts: %d script ids the copy names across %d banks, %s, every "
          "one inside its own bank's scripts and naming one of its own "
          "bank's textures"
          % (len(ids), len(banks),
             ", ".join("%s %d/%d" % (b, sum(1 for x in ids if x[0] == b),
                                     len(banks[b][0]))
                       for b in sorted(banks))))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.isdir(DECOMP):
        print(f"no decomp checkout at {DECOMP} -- skipping")
        return 0
    if check_text() != 0:
        return 1
    return check_scripts(args.verbose)


if __name__ == "__main__":
    sys.exit(main())
