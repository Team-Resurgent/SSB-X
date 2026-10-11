/* itemoffsets.h -- the offsets into ITCommonData and ITCommonObject that
 * the game names by SYMBOL.
 *
 * On the N64 `llITCommonData*` are the reloc files' own boundary labels,
 * so the symbol's ADDRESS is the offset. That is why every use in the
 * decomp writes `&llITCommonDataXxx`, and why `itGetPData(ip, &A, &B)`
 * (it/item.h:41) is `attr->data - A + B`. The port has no such labels and
 * no single base to compute them from -- the addresses are unrelated
 * globals, so no one pointer makes all of them an offset -- so the numbers
 * are written out, the same substitution src/dc/ifcommon.c's sprite-offset
 * tables already make.
 *
 * THEY ARE reloc_data.us.h'S OWN VALUES, and tools/check/itoffset_check.py
 * holds them to it (wired into `./run.sh test`). A wrong number here is a silent
 * read of the wrong bytes, which is the one failure a host test cannot
 * see, so this table has an oracle instead.
 *
 * Note that they address TWO files. The `ItemAttributes` tables are in
 * ITCommonData (relocData 0xFB, 3,392 bytes) and everything else --
 * `DataStart`, `DisplayList`, `AnimJoint`, `MObjSub`, `AttackEvents` --
 * is in ITCommonObject (0x56, 79,584 bytes), which is why the second group
 * is numbered far past the first file's end. `itGetPData`'s arithmetic
 * cancels the difference: `attr->data` points into the models, so
 * subtracting the block's own offset and adding the wanted one lands on
 * the wanted one.
 *
 * The Poké Ball monsters are the same two groups again, plus
 * one shape worth naming: `itGetMonsterAnimNode(ip, off)` is
 * `itGetPData(ip, off, &llITCommonDataMonsterAnimBankStart)` -- every
 * monster's anim scripts live in ONE bank at the end of ITCommonObject, and
 * each monster names its own entry by the offset of its model block. So
 * `llITCommonDataMonsterAnimBankStart` is an offset like any other here,
 * and the port's redefinition of the macro (src/dc/itmonster.h) is what
 * turns the decomp's `&sym` into the number.
 */
#ifndef SSB_DC_ITEMOFFSETS_H
#define SSB_DC_ITEMOFFSETS_H

#define llITCommonDataBatItemAttributes                0x1d8
#define llITCommonDataBombHeiAttackEvents              0x46c
#define llITCommonDataBombHeiDataStart                 0x33f8
#define llITCommonDataBombHeiItemAttributes            0x424
#define llITCommonDataBombHeiWalkLeftDisplayList       0x34c0
#define llITCommonDataBombHeiWalkMatAnimJoint          0x35b8
#define llITCommonDataBombHeiWalkRightDisplayList      0x3310
#define llITCommonDataBoxAttackEvents                  0x614
#define llITCommonDataDogasAnimJoint                   0x128dc
#define llITCommonDataDogasDataStart                   0x12820
#define llITCommonDataDogasItemAttributes              0xbf8
#define llITCommonDataDogasSmogWeaponAttributes        0xc40
#define llITCommonDataBoxItemAttributes                0x5cc
#define llITCommonDataCapsuleAttackEvents              0x98
#define llITCommonDataCapsuleItemAttributes            0x50
#define llITCommonDataContainerVelocitiesY             0x0
#define llITCommonDataEggAttackEvents                  0xb14
#define llITCommonDataEggItemAttributes                0xacc
#define llITCommonDataFFlowerFlameAngles               0x360
#define llITCommonDataFFlowerFlameWeaponAttributes     0x32c
#define llITCommonDataFFlowerItemAttributes            0x2e4
#define llITCommonDataGBumperItemAttributes            0xcf0
#define llITCommonDataGShellItemAttributes             0x53c
#define llITCommonDataHammerItemAttributes             0x374
#define llITCommonDataHarisenItemAttributes            0x220
#define llITCommonDataHeartItemAttributes              0x100
#define llITCommonDataKabigonAnimJoint                 0xb158
#define llITCommonDataKabigonItemAttributes            0x7a8
#define llITCommonDataKamexDataStart                   0xea60
#define llITCommonDataKamexDisplayList                 0xed60
#define llITCommonDataKamexHydroWeaponAttributes       0xa50
#define llITCommonDataKamexItemAttributes              0xa08
#define llITCommonDataLGunAmmoWeaponAttributes         0x2b0
#define llITCommonDataLGunItemAttributes               0x268
#define llITCommonDataLizardonAnimJoint                0xd658
#define llITCommonDataLizardonDataStart                0xd5c0
#define llITCommonDataLizardonFlameWeaponAttributes    0x944
#define llITCommonDataLizardonItemAttributes           0x8fc
#define llITCommonDataLizardonMatAnimJoint             0xd688
#define llITCommonDataLuckyAnimJoint                   0x100bc
#define llITCommonDataLuckyDataStart                   0x10000
#define llITCommonDataMBallDataStart                   0x9430
#define llITCommonDataMBallItemAttributes              0x6e4
#define llITCommonDataMBallMatAnimJoint                0x9520
#define llITCommonDataMBallThrownFileHead              0x6e4
#define llITCommonDataMewDataStart                     0xbcc0
#define llITCommonDataMewItemAttributes                0x838
#define llITCommonDataMLuckyItemAttributes             0xa84
#define llITCommonDataMonsterAnimBankStart             0x13624
#define llITCommonDataMSBombAttackEvents               0x404
#define llITCommonDataMSBombItemAttributes             0x3bc
#define llITCommonDataNBumperDataStart                 0x7648
#define llITCommonDataNBumperItemAttributes            0x69c
#define llITCommonDataNBumperWaitDisplayList           0x7af8
#define llITCommonDataNBumperWaitMObjSub               0x7a38
#define llITCommonDataNyarsAnimJoint                   0xc130
#define llITCommonDataNyarsCoinWeaponAttributes        0x8c8
#define llITCommonDataNyarsItemAttributes              0x880
#define llITCommonDataPippiDataStart                   0x13598
#define llITCommonDataPippiItemAttributes              0xc74
/* Clefairy's own swarm, which its OWN file defines a second WPDesc for --
 * the same nWPKindSpearSwarm as Beedrill's and a different table, so the
 * port's weapon model table needs a row for each. */
#define llITCommonDataPippiSwarmWeaponAttributes       0xcbc
#define llITCommonDataRShellItemAttributes             0x584
#define llITCommonDataSawamuraDataStart                0x11f40
#define llITCommonDataSawamuraDisplayList              0x12340
#define llITCommonDataSawamuraItemAttributes           0xbb0
#define llITCommonDataShellAnimJoint                   0x6018
#define llITCommonDataShellDataStart                   0x5f88
#define llITCommonDataShellMatAnimJoint                0x6048
#define llITCommonDataSpearAnimJoint                   0xdffc
#define llITCommonDataSpearDataStart                   0xdf38
#define llITCommonDataSpearItemAttributes              0x98c
#define llITCommonDataSpearMatAnimJoint                0xe12c
#define llITCommonDataSpearSwarmWeaponAttributes       0x9d4
#define llITCommonDataStarItemAttributes               0x148
#define llITCommonDataStarRodItemAttributes            0x48c
#define llITCommonDataStarRodSmashWeaponAttributes     0x508
#define llITCommonDataStarRodWeaponAttributes          0x4d4
#define llITCommonDataStarmieDataStart                 0x112a0
#define llITCommonDataStarmieItemAttributes            0xb34
#define llITCommonDataStarmieMatAnimJoint              0x11338
#define llITCommonDataStarmieSwiftWeaponAttributes     0xb7c
#define llITCommonDataSwordItemAttributes              0x190
#define llITCommonDataTaruAttackEvents                 0x67c
#define llITCommonDataTaruItemAttributes               0x634
#define llITCommonDataTomatoItemAttributes             0xb8
#define llITCommonDataTosakintoAnimJoint               0xb7cc
#define llITCommonDataTosakintoDataStart               0xb708
#define llITCommonDataTosakintoItemAttributes          0x7f0
#define llITCommonDataTosakintoMatAnimJoint            0xb90c
#define llITCommonDataWarkDataStart                    0xa140
#define llITCommonDataWarkDisplayList                  0xa640
#define llITCommonDataWarkItemAttributes               0x72c
#define llITCommonDataWarkRockWeaponAttributes         0x774

#endif /* SSB_DC_ITEMOFFSETS_H */
