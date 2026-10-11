# ssb64 -- the game.
#
# main.c is just a boot; the instrument lives in src/dc/db.c and the
# scene manager runs the game. Every object below except main.o comes from src/dc/ or, for
# the files the port compiles unmodified, from $(SSB_DECOMP_DIR).
#
# Invoked from the repo root as:
#   make -C src/game/ssb64
# (source scripts/dc_env.sh first), or `./run.sh build`.
#
# Collision is the decomp's own mp/mpcollision.c and mp/mpprocess.c,
# compiled straight out of $(SSB_DECOMP_DIR) behind the shim headers in
# src/dc/decomp/ (see the README there) -- no copy of them lives in this
# repo. Every object here sees the decomp headers, because ftcommon.h
# carries the game's MPCollData.

TARGET = ssb64.elf
# Where this directory is, from the repo root: the asset rules below run
# the exporters from there (scripts/ctr.sh mounts the repo at /work and
# the container's cwd is its root), so every --out is repo-relative. One
# variable rather than forty literals, so the next move is one line.
SELF = src/game/ssb64
FTCOMMON_STATUS_OBJS = ftcommonstopceil.o ftcommondownforwardback.o \
                       ftcommondownattack.o ftcommonrebound.o \
                       ftcommoncliffattack.o \
                       ftcommonlandingair.o ftcommonfallspecial.o \
                       ftcommonspeciallw.o ftcommondokan.o \
                       ftcommoncapturecaptain.o ftcommoncaptureyoshi.o \
                       ftcommoncapture.o ftcommoncapturekirby.o
FTCHAR_SPECIAL_OBJS = ftmariospeciallw.o ftdonkeyspeciallw.o \
                      ftpurinspeciallw.o ftkirbyspeciallw.o \
                      ftpurinspecialn.o ftkirbycopypurinspecialn.o \
                      ftmariospecialn.o ftmariospecialhi.o \
                      ftfoxspecialn.o ftpurinspecialhi.o \
                      ftdonkeyspecialhi.o ftsamusspecialhi.o \
                      ftcaptainspecialn.o ftsamusspeciallw.o \
                      ftcaptainspeciallw.o ftyoshispeciallw.o \
                      ftdonkeyspecialn.o ftcaptainspecialhi.o \
                      ftsamusspecialn.o ftfoxspeciallw.o \
                      ftfoxspecialhi.o ftyoshispecialn.o \
                      ftyoshispecialhi.o ftkirbyspecialhi.o \
                      ftkirbyspecialn.o \
                      ftkirbycopymariospecialn.o \
                      ftkirbycopyfoxspecialn.o \
                      ftkirbycopysamusspecialn.o \
                      ftkirbycopydonkeyspecialn.o \
                      ftkirbycopycaptainspecialn.o \
                      ftkirbycopyyoshispecialn.o \
                      ftlinkspecialn.o ftlinkspecialhi.o ftkirbycopylinkspecialn.o \
                      ftlinkspeciallw.o \
                      ftpikachuspecialn.o ftkirbycopypikachuspecialn.o \
                      ftpikachuspeciallw.o ftpikachuspecialhi.o \
                      ftnessspecialn.o ftnessspecialhi.o ftnessspeciallw.o \
                      ftkirbycopynessspecialn.o \
                   ftdonkeythrowfwait.o ftdonkeythrowfwalk.o \
                   ftdonkeythrowfturn.o ftdonkeythrowfkneebend.o \
                   ftdonkeythrowffall.o ftdonkeythrowflanding.o \
                   ftdonkeythrowfdamage.o ftdonkeythrowff.o \
                   ftkirbythrowf.o ftcommonattack100.o \
                   ftcommonjumpaerial.o ftcommonsquat.o \
                   ftcommonfall.o ftcommonpass.o \
                   ftcommonget.o ftcommonitemthrow.o \
                   ftcommonitemswing.o ftcommonitemshoot.o \
                   fthammer.o ftcommonhammerfall.o \
                   ftcommonhammerkneebend.o ftcommonhammerlanding.o \
                   ftcommonhammerturn.o ftcommonhammerwalk.o
# Special-move / ftcommon-status proc files the TARGET compiles unmodified and
# dead-strips but which were once absent from the host link (no --gc-sections
# there, so an unreferenced object still needs every callee defined). Both are
# now EMPTY: ftmariospecialhi.o and ftcommonfallspecial.o are host+target,
# because their whole callee closures (ftCommonFallSpecial / the fall-special
# common, ftCommonJumpAerialCheckInterruptCommon and the cliffcatch/landing/
# wait setters, all the ftPhysics*/mpCommon* leaves) are in the host build, so
# the host link resolves them. The two variables stay (still referenced in
# OBJS) as anchors for the next target-only file.
FTCHAR_SPECIAL_TARGET_OBJS =
FTCOMMON_TARGET_OBJS =
# Master Hand's status procs, ft/ftchar/ftboss/, compiled
# unmodified from the decomp. The same "code first, table later" every
# special above stands as: these are dead until
# dFTBossSpecialStatusDescs names them (ftcommon.c's
# dFTMainSpecialStatusDescs is FT_SPECIAL_NONE for him), and that table
# cannot be assembled until all 44 of its ftBoss* procs exist over the
# 35 files. Step 24 took the spine and 600 of the 1,941 lines:
# ftbosscommon.c is the 15 helpers every motion calls -- turn around,
# pick a floor edge, aim at the target -- and ftbosswait.c is the attack
# chooser that decides which of the 33 he does next.
#
# Step 25 takes 31 of the 33 motions -- every one that compiles against
# the port as it stands. The one held back is ftbossyubideppou3.c, the
# finger rocket, which calls wpBossBulletNormalMakeWeapon and
# wpBossBulletHardMakeWeapon: wp/wpboss/wpbossbullet.c wants a WPDesc
# and a model row in sWPManagerModels the way every projectile in
# WPCHAR_OBJS does, so it is its own step. (ftboss.c, the fourth file
# in that directory, is not a motion at all -- it is his four
# loaded-file base pointers, and rides FTCHAR_DATA_OBJS below.)
FTBOSS_OBJS = ftbosscommon.o ftbosswait.o \
              ftbossappear.o ftbossdeadcenter.o ftbossdeadleft.o \
              ftbossdeadright.o ftbossdefault.o ftbossdrill.o \
              ftbossgootsubusudown.o ftbossgootsubusuend.o \
              ftbossgootsubusuup.o ftbossgootsubusuwait.o \
              ftbossharau.o ftbosshippataku.o ftbossmove.o \
              ftbossokuhikouki1.o ftbossokuhikouki2.o \
              ftbossokuhikouki3.o ftbossokukouki.o \
              ftbossokupunch1.o ftbossokupunch2.o ftbossokupunch3.o \
              ftbossokutsubushi.o ftbossokutsubushistart.o \
              ftbosstsutsuku1.o ftbosstsutsuku2.o ftbosstsutsuku3.o \
              ftbosswalk.o ftbosswalkloop.o ftbosswalkshoot.o \
              ftbosswalkwait.o ftbossyubideppou1.o ftbossyubideppou2.o \
              ftbossyubideppou3.o
# The fighter data files that hold only the loaded-file base pointers a
# weapon's WPDesc names (ovl2 bss); their motion-desc tables are commented
# out in the decomp, so each compiles to just those NULL globals. Mario's
# fireball is the first to need them; Fox's Blaster is the second. ftpurin.o
# is here for a different reason: not a weapon this time, but gFTDataPurinSpecial2, the file base
# efManagerPurinSingMakeEffect's EFDesc points its reloc offsets into.
# ftboss.o is here for the first reason: his four base
# pointers (gFTDataBossMain, gFTDataBossMainMotion, gFTDataBossModel,
# gFTDataBossParticleBankID) are the whole of ft/ftchar/ftboss/ftboss.c
# -- 13 lines -- and the motions above reach them. It is in
# HOST_DECOMP_SRCS too, because HOST_OVL2_OBJS is derived from this
# variable: a data file the target builds and the host does not would
# ask objcopy to rename an object nobody made. His MOTIONS are not in
# the host build -- it has no --gc-sections, so an unreferenced object
# still needs every callee defined, and nothing there calls them.
FTCHAR_DATA_OBJS = ftmario.o ftluigi.o ftfox.o ftpurin.o ftsamus.o ftyoshi.o \
                   ftkirby.o ftlink.o ftpikachu.o ftdonkey.o ftness.o \
                   ftboss.o
# The per-fighter projectiles (ovl3): each is a WPDesc + attribute table +
# procs + a MakeWeapon that calls the step-24 wpManagerMakeWeapon, with a
# model row in sWPManagerModels (src/dc/wpmanager.c).
#
# wpbossbullet.o is the fifteenth. It is in
# HOST_DECOMP_SRCS too, for the same reason ftboss.c is: HOST_OVL3_OBJS
# is DERIVED from this variable, so a file the target builds and the
# host does not asks objcopy to rename an object nobody made.
WPCHAR_OBJS = wpmariofireball.o wpfoxblaster.o wpsamusbomb.o wpyoshistar.o \
              wpsamuschargeshot.o wpyoshieggthrow.o wpkirbycutter.o \
              wplinkboomerang.o wplinkspinattack.o wppikachuthunderjolt.o \
              wppikachuthunder.o wpnesspkfire.o wpnesspkthunder.o \
              itnesspkfire.o itlinkbomb.o wpbossbullet.o
OBJS = main.o fighter.o input.o stage.o ftcommon.o ftmanager.o \
       ftmain.o ftparam.o ftshadow.o ftcomputer.o gmcollision.o ftcommondata.o ftpublic.o \
       ftphysics.o ftcommontwister.o grhyrule.o grjungle.o ftcommontaru.o \
       grzebes.o gryoster.o grpupupu.o grinishie.o grcastle.o gryamabuki.o \
       grsector.o grbonus3.o \
       $(FTCOMMON_STATUS_OBJS) \
       $(FTCHAR_SPECIAL_OBJS) \
       $(FTCHAR_SPECIAL_TARGET_OBJS) \
       $(FTCOMMON_TARGET_OBJS) \
       $(FTBOSS_OBJS) \
       $(FTCHAR_DATA_OBJS) \
       $(WPCHAR_OBJS) \
       ifcommon.o ifscreenflash.o gmcolscripts.o ftdisplaymain.o scmanager.o scmanagerdata.o scvsbattle.o scvsresults.o scautodemo.o scexplain.o mvending.o scstaffroll.o mvopeningroom.o mvopeningportraits.o mvopeningmario.o mvopeningdonkey.o mvopeningsamus.o mvopeninglink.o mvopeningyoshi.o mvopeningkirby.o mvopeningfox.o mvopeningpikachu.o mvopeningrun.o mvopeningcliff.o mvopeningyamabuki.o mvopeningjungle.o mvopeningyoster.o mvopeningsector.o mvopeningstandoff.o mvopeningclash.o mvopeningnewcomers.o mntitle.o \
       mnmodeselect.o mn1pmode.o mnvsmode.o mnvsoptions.o mnplayersvs.o \
       mnplayers1ptraining.o sc1ptrainingmode.o mnplayers1pgame.o \
       mnplayers1pbonus.o \
       sc1pchallenger.o sc1pintro.o sc1pgame.o sc1pbonusstage.o \
       sc1pstageclear.o \
       sc1pgameboss.o sc1pmanager.o mn1pcontinue.o mnmaps.o \
       mnmessage.o mnvsitemswitch.o mndata.o mnvsrecord.o mncharacters.o mnsoundtest.o mnoption.o \
       mnscreenadjust.o mnbackupclear.o mncongra.o mnnocontroller.o dcmemcard.o \
       mnstartup.o \
       scsubsysfighter.o \
       scsubsysdata.o \
       lbfade.o \
       scsubsyscontroller.o \
       gmcommon.o \
       sprite.o camanim.o \
       gmcamera.o lbcommon.o mpcommon.o utils.o vector.o \
       wpmanager.o wpmain.o wpprocess.o wpdisplay.o wpmap.o wpattrs.o \
       itmanager.o itempack.o itemmodel.o itmain.o itprocess.o itvisuals.o itdisplay.o itmap.o \
       itbox.o ittaru.o itcapsule.o itegg.o itmball.o \
       itmew.o ittosakinto.o itsawamura.o itkabigon.o itmlucky.o itnyars.o itdogas.o itiwark.o itstarmie.o itlizardon.o itspear.o itkamex.o itpippi.o \
       itstar.o ittomato.o itheart.o itsword.o itbat.o ithammer.o itharisen.o itfflower.o itlgun.o itstarrod.o itgshell.o itrshell.o itnbumper.o itgbumper.o itpowerblock.o itpakkun.o itglucky.o itporygon.o itmarumine.o ithitokage.o itfushigibana.o ittarget.o ittarubomb.o itmsbomb.o itbombhei.o \
       gmrumble.o \
       mpshim.o mpcollision.o mpprocess.o \
       bgm.o bgmbank.o \
       fgm.o syaudio.o sndres.o taskman.o symalloc.o \
       objman.o objhelper.o objscript.o objanim.o interp.o ftanim.o \
       lbtransition.o lbbackup.o \
       sysshim.o stackguard.o textguard.o vmusave.o vmucard.o dctext.o vmunotice.o objdisplay.o objmodel.o clip.o overlay.o \
       assetroot.o disctrace.o loadcensus.o introcrowd.o primdump.o dma.o lbparticle.o lbpartex.o lbpdraw.o \
       efdisplay.o efmanager.o efparticle.o efground.o \
       matrix.o sintable.o \
       gusinf.o gucosf.o mtxutil.o mtxcatf.o mtxxfmf.o normalize.o db.o

# The romdisk is linked into the ELF by default: 13 MB of the 14 MB image,
# which is what the disc is for. With DISC=1 it is
# left out and the ELF
# carries code only -- scripts/make_cdi.sh stages this same directory into
# the disc's data track, where the loaders find it at /cd instead of /rd
# (src/dc/assetroot.h). DCLOAD=1 leaves it out the same way, for the same
# reason -- assets instead come from /pc, KOS's dcload host-filesystem
# passthrough (src/dc/db.h's DB_PC_ROOT, which this sets automatically
# below). The assets themselves are built either way; see ASSETS below.
NO_ROMDISK :=
ifeq ($(DISC),1)
NO_ROMDISK := 1
endif
ifeq ($(DCLOAD),1)
NO_ROMDISK := 1
endif
ifneq ($(NO_ROMDISK),1)
OBJS += romdisk.o
KOS_ROMDISK_DIR = romdisk
endif

# The port's own AICA firmware in place of KallistiOS's stream.drv.
OBJS += dcaica.o aica_fwblob.o bgmarm.o seqprep.o seqcore.o

# RELEASE=1 (`./run.sh release`): the build a player gets. No debug facility
# (src/dc/db.h: db.o is left out of the link and DB_ENABLE is 0), no log
# output anywhere (syDebugPrintf is a no-op, KOS's debug console is
# switched off in main()), asserts off. A stray EXTRA_CFLAGS is refused:
# a knob in a release is a debug build under another name.
RELEASE ?= 0
ifeq ($(RELEASE),1)
ifneq ($(strip $(EXTRA_CFLAGS)),)
$(error RELEASE=1 takes no EXTRA_CFLAGS ($(EXTRA_CFLAGS)))
endif
KOS_CFLAGS += -DSSB_RELEASE=1 -DDB_ENABLE=0 -DNDEBUG
# `override`: tools/check/standin_check.py reads only plain assignments, and
# a self-referential one it cannot expand
override OBJS := $(filter-out db.o,$(OBJS))
endif

# A DCLOAD build has no romdisk in the ELF and (usually) no disc either --
# without DB_PC_ROOT it would have no asset source at all, so this pairing
# is not left for the caller to remember. UNVERIFIED on real hardware
# (src/dc/db.h).
ifeq ($(DCLOAD),1)
KOS_CFLAGS += -DDB_PC_ROOT=1
endif

SSB_DECOMP_DIR ?= /opt/ssb-decomp-re
DECOMP_INC = -I../../dc/decomp -I$(SSB_DECOMP_DIR)/src \
             -idirafter $(SSB_DECOMP_DIR)/include
# The decomp's Makefile:93 passes -DF3DEX_GBI_2; _LANGUAGE_C is predefined
# by IDO and not by GCC, and PR/gbi.h guards its whole body on it
# (gbi.h:1061). Without both, the object system's headers stop at unknown
# type names for Gfx, Mtx, Vtx, LookAt and Hilite. REGION_US is the
# decomp Makefile:113's for VERSION=us, the ROM the port is built from:
# the headers branch on it (sc/scdef.h's scene kinds, gm/gmsound.h's FGM
# ids, ft/ftcommon.h's damage and shield constants, mn/mndef.h's title
# sprite kinds); without it every one of them silently takes the JP arm.
DECOMP_DEFS = -D_LANGUAGE_C -DF3DEX_GBI_2 -DREGION_US -DAVOID_UB
# AVOID_UB is the decomp's own option for "give an
# unspecified value a definite one", and a port has no reason to want the
# unspecified ones. It is set HERE, in DECOMP_DEFS, because that is the
# one variable both KOS_CFLAGS and HOST_CFLAGS pick up: the decomp files
# the port compiles unmodified, the port's own C, the host cross-test and
# the disc probe all see it, and there is no build in this tree where the
# arms differ.
#
# Twelve sites in the decomp, six of them in files this port compiles.
# Eleven are "the switch ran out of cases and the function fell off its
# end"; the twelfth is the one that matters here. sys/objscript.c's
# gcParseGObjScript is a FAKE MATCH -- without the option it assigns
# `gobj = gobj->gobjscripts` and then reads the loop bound at
# `(uintptr_t)gobj + (offsetof(GObj, gobjscripts_num) -
# offsetof(GObj, gobjscripts))`, which lands on the right field only
# because of where the N64 compiler put them. On the SH-4 that is a read
# into the script array itself. The decomp's own comment says to use the
# guarded arm in real applications, and this is one.
#
# Two more sites are the port's own hand-copies rather than the decomp's:
# src/dc/mnplayersvs.c keeps the #ifdef and now takes its arm, and
# src/dc/mntitle.c and src/dc/ftdisplaymain.c had already chosen the
# AVOID_UB arm by hand when they were written -- so the define and the
# hand-choice now say the same thing everywhere.
DECOMP_INC += $(DECOMP_DEFS)
# The decomp's C: initialisers that take addresses into intptr_t
# (GRFileInfo), and its share of unused locals.
DECOMP_CFLAGS = -Wno-int-conversion -Wno-unused-variable \
                -Wno-unused-but-set-variable -Wno-parentheses \
                -Wno-maybe-uninitialized
# syDmaLoadOverlay's bzero, for the decomp files the port compiles
# unmodified. Their statics are file-scope, so no reload can name them
# the way a port file's xxxOverlayLoad names its own; the N64 did not
# name them either, it bzeroed an address range. So does the port, and
# the range comes from the linker: compile the object with one .bss
# rather than one section per variable, rename that .bss to the overlay
# it belongs to, and ld -- which synthesizes __start_NAME and __stop_NAME
# for any orphan output section whose name is a C identifier -- hands
# src/dc/overlay.c the two ends. The section lands inside
# [__bss_start, end), so it is zero on the first entry as well as the
# rest. Membership is smashbrothers.us.yaml's and tools/check/overlay_check.py
# re-derives it from there; src/dc/overlay.h has the whole argument.
#
# (The obvious way to do this is a linker script fragment ending in
# INSERT AFTER .bss. It does not work: a symbol defined inside an INSERT
# fragment is created too late to satisfy a reference from an input
# object, and ld reports it undefined with its address sitting in the
# map file.)
OVL_NOLOAD_CFLAGS = -fno-data-sections
ovl-noload = sh-elf-objcopy --rename-section .bss=ovl$(1)_noload $(2).tmp $(2) \
             && rm -f $(2).tmp
KOS_CFLAGS += -I../../dc $(DECOMP_INC)
# `make EXTRA_CFLAGS=-DDB_BOOT_STAGE=nGRKindSector` boots the battle
# on another stage without walking the menus for it (src/dc/db.h).
KOS_CFLAGS += $(EXTRA_CFLAGS)
# Make tracks sources and headers, not flags: a build with a different
# EXTRA_CFLAGS or MTX_BACKEND used to need its objects deleted by hand
# (db.o at the least, and whichever else read the define). The flags are
# written to .cflags only when they differ from what is there, and every
# compiled object depends on it, so a changed knob rebuilds the tree and
# an unchanged one rebuilds nothing. romdisk.o is left out: it is 13 MB of
# bin2c that no flag reaches.
#
# DCLOAD and DISC are in the stamp too: they each change which defines and
# which sources go into the ELF (DCLOAD=1 sets DB_PC_ROOT and drops the
# romdisk), and a build that toggled one of them without touching
# EXTRA_CFLAGS used to link a stale assetroot.o with no /pc path in it.
CFLAGS_STAMP = .cflags
CFLAGS_NOW = $(strip $(EXTRA_CFLAGS) $(MTX_BACKEND) DCLOAD=$(DCLOAD) DISC=$(DISC) RELEASE=$(RELEASE))
ifneq ($(wildcard $(CFLAGS_STAMP))|$(strip $(shell cat $(CFLAGS_STAMP) 2>/dev/null)),$(CFLAGS_STAMP)|$(CFLAGS_NOW))
$(shell printf '%s\n' '$(CFLAGS_NOW)' > $(CFLAGS_STAMP))
endif

# SH4ZAM: `make MTX_BACKEND=SH4ZAM` builds the render core against the SH4's
# XMTRX/FTRV instead of scalar C. Both paths stay in the tree so a regression
# can be bisected on the target; see src/dc/mtx.h.
ifeq ($(MTX_BACKEND),SH4ZAM)
SH4ZAM_DIR ?= /opt/sh4zam
KOS_CFLAGS += -DMTX_BACKEND=MTX_BACKEND_SH4ZAM -I$(SH4ZAM_DIR)/include
endif

all: rm-elf $(TARGET)
# `make -j`: rm-elf deletes the ELF the link is about to write, and as a
# sibling prerequisite of `all` nothing stops it running after the link.
# Naming it here orders the two. It has to be a normal prerequisite and
# not an order-only one: make caches the ELF's mtime when it first
# considers it, so an order-only rm-elf leaves it believing a file that
# no longer exists is up to date and the link never runs. A phony
# prerequisite is always newer, which is also what keeps `all` relinking
# every time, as it did when rm-elf was only a sibling.
# scripts/build_target.sh passes -j.
$(TARGET): rm-elf

PACKTOOL_DEPS = ../../../tools/export/ssb_packexport.py \
                ../../../tools/lib/ssb_meshexport.py \
                ../../../tools/lib/ssb_assets.py \
                ../../../tools/lib/ssb_extract.py \
                ../../../tools/lib/ssb_texconv.py

# a fighter pack's .anm is laid out in the tiers this names
FIGHTERTOOL_DEPS = $(PACKTOOL_DEPS) ../../../tools/export/anm_tiers.tsv

STAGETOOL_DEPS = ../../../tools/export/ssb_stageexport.py \
                 ../../../tools/lib/ssb_logicexport.py $(PACKTOOL_DEPS)

SPRITETOOL_DEPS = ../../../tools/export/ssb_spriteexport.py \
                  ../../../tools/lib/ssb_logicexport.py \
                  ../../../tools/lib/ssb_paths.py $(PACKTOOL_DEPS)

# The item pack is not a stage, but it is built the same way and off the
# same ROM, so it gets its own deps rather than riding on the stage's --
# which is how it came to be built once and never rebuilt when the
# exporter changed (a 90,544-byte pack against a reader that
# expected 90,680, caught by a test that reads a record's own key).
# The camera-animation banks (src/dc/camanim.c). It reads
# relocData through ssb_assets' shared AObjEvent32 walk and nothing else,
# so it rides on the pack deps the way the sprite tool does.
CAMANIMTOOL_DEPS = ../../../tools/export/ssb_camanimexport.py \
                   ../../../tools/lib/ssb_paths.py $(PACKTOOL_DEPS)

ITEMTOOL_DEPS = ../../../tools/export/ssb_itemexport.py \
                ../../../tools/lib/ssb_logicexport.py \
                ../../../tools/lib/ssb_paths.py $(PACKTOOL_DEPS)

# The fighters' weapon hitbox tables and the linker fragment that gives
# their ll* symbols the ROM offsets (src/dc/wpattrs.c). Rides on the item
# exporter: the record format is its.
WPATTRTOOL_DEPS = ../../../tools/export/ssb_wpattrexport.py \
                  ../../../tools/export/ssb_itemexport.py \
                  ../../../tools/lib/ssb_logicexport.py \
                  ../../../tools/lib/ssb_paths.py $(PACKTOOL_DEPS) \
                  ../../dc/decomp/reloc_data.us.h

ITEMBAKE_DEPS = ../../../tools/export/ssb_itemmodelexport.py \
                ../../../tools/export/ssb_itemexport.py \
                ../../../tools/export/ssb_effectexport.py \
                ../../../tools/export/ssb_stageexport.py \
                ../../../tools/export/ssb_packexport.py \
                ../../../tools/lib/ssb_logicexport.py \
                ../../../tools/lib/ssb_paths.py $(PACKTOOL_DEPS)

# The eleven screen wipes of lb/lbtransition.c, one pack each. They are
# the only models whose texture is not in the file: the display lists
# read segment 1, which is a copy of the last frame drawn, so each pack
# has one FPACK_TEX_EXTERN batch that src/dc/lbtransition.c binds at
# load. 237 KB on the romdisk between them and one at a time in RAM,
# which is what the game does too -- mnVSResultsFuncStart rolls one and
# loads only that file.
TRANSTOOL_DEPS = ../../../tools/export/ssb_transexport.py $(PACKTOOL_DEPS)

TRANSITIONS = romdisk/aeroplane.tra romdisk/check.tra romdisk/gakubuthi.tra \
              romdisk/kannon.tra romdisk/star.tra romdisk/sudare1.tra \
              romdisk/sudare2.tra romdisk/camera.tra romdisk/block.tra \
              romdisk/rotscale.tra romdisk/curtain.tra

# $(1) pack basename, $(2) the dLBTransitionDescs name
define transition-rule
romdisk/$(1).tra: $$(TRANSTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_transexport.py \
	    --rom baserom.z64 --name $(2) \
	    --out $(SELF)/romdisk/$(1).tra
endef
$(eval $(call transition-rule,aeroplane,Aeroplane))
$(eval $(call transition-rule,check,Check))
$(eval $(call transition-rule,gakubuthi,Gakubuthi))
$(eval $(call transition-rule,kannon,Kannon))
$(eval $(call transition-rule,star,Star))
$(eval $(call transition-rule,sudare1,Sudare1))
$(eval $(call transition-rule,sudare2,Sudare2))
$(eval $(call transition-rule,camera,Camera))
$(eval $(call transition-rule,block,Block))
$(eval $(call transition-rule,rotscale,RotScale))
$(eval $(call transition-rule,curtain,Curtain))

# The winner's series emblem: ten little models out of relocData 35, one
# per series, and the first packs in the repo with a *material* -- their
# colour is an MObj chain the game's own MatAnimJoint parser recolours per
# player (tools/export/ssb_emblemexport.py, src/dc/scvsresults.c).
EMBLEMTOOL_DEPS = ../../../tools/export/ssb_emblemexport.py $(PACKTOOL_DEPS)

EMBLEMS = romdisk/mario.mdl romdisk/fox.mdl romdisk/donkey.mdl \
          romdisk/metroid.mdl romdisk/zelda.mdl romdisk/yoshi.mdl \
          romdisk/fzero.mdl romdisk/kirby.mdl romdisk/pmonsters.mdl \
          romdisk/mother.mdl

define emblem-rule
romdisk/$(1).mdl: $$(EMBLEMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_emblemexport.py \
	    --rom baserom.z64 --name $(2) \
	    --out $(SELF)/romdisk/$(1).mdl
endef

$(eval $(call emblem-rule,mario,Mario))
$(eval $(call emblem-rule,fox,Fox))
$(eval $(call emblem-rule,donkey,Donkey))
$(eval $(call emblem-rule,metroid,Metroid))
$(eval $(call emblem-rule,zelda,Zelda))
$(eval $(call emblem-rule,yoshi,Yoshi))
$(eval $(call emblem-rule,fzero,FZero))
$(eval $(call emblem-rule,kirby,Kirby))
$(eval $(call emblem-rule,pmonsters,PMonsters))
$(eval $(call emblem-rule,mother,Mother))

# The character select's spotlight: relocData 22, one quad whose whole
# shape is a 32x32 I8 tile the RDP reads in the alpha cycle alone
# (tools/export/ssb_spotexport.py, src/dc/mnplayersvs.c).
SPOTTOOL_DEPS = ../../../tools/export/ssb_spotexport.py $(PACKTOOL_DEPS)

romdisk/spotlite.mdl: $(SPOTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spotexport.py \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/spotlite.mdl

# The save file's pictures: the icon's three frames, the
# eyecatch and the VMU LCD image, made from file 0's emblem and file
# 167's wordmark (tools/export/ssb_vmuart.py). A cartridge's save has no
# pictures, so these are the port's, but every pixel is the game's art.
# The preview PNG is for looking at and sits outside romdisk/, which
# disc_layout.py would refuse it in; it is gitignored like the rest.
VMUARTTOOL_DEPS = ../../../tools/export/ssb_vmuart.py \
                  ../../../tools/check/vmuimg.py $(SPRITETOOL_DEPS)

romdisk/vmuart.bin: $(VMUARTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_vmuart.py \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/vmuart.bin \
	    --preview $(SELF)/vmuart-preview.png

# The port's own text (src/dc/dctext.h): the staff roll's
# popup-textbox font, file 195's 74 glyphs -- A-Z, a-z, 0-9 and eleven
# marks -- and not its brackets or crosshair. Loaded once at boot and kept
# (sprite_bank_load_resident), so the memory card's notice and pages can
# speak in any scene. The same glyphs are also in scstaffrollgraphics.spr,
# which the staff roll loads for itself as the game does.
DCFONT_ONLY = TextBoxAUpper,TextBoxALower,TextBoxBUpper,TextBoxBLower,TextBoxCUpper,TextBoxCLower,TextBoxDUpper,TextBoxDLower,TextBoxEUpper,TextBoxELower,TextBoxFUpper,TextBoxFLower,TextBoxGUpper,TextBoxGLower,TextBoxHUpper,TextBoxHLower,TextBoxIUpper,TextBoxILower,TextBoxJUpper,TextBoxJLower,TextBoxKUpper,TextBoxKLower,TextBoxLUpper,TextBoxLLower,TextBoxMUpper,TextBoxMLower,TextBoxNUpper,TextBoxNLower,TextBoxOUpper,TextBoxOLower,TextBoxPUpper,TextBoxPLower,TextBoxQUpper,TextBoxQLower,TextBoxRUpper,TextBoxRLower,TextBoxSUpper,TextBoxSLower,TextBoxTUpper,TextBoxTLower,TextBoxUUpper,TextBoxULower,TextBoxVUpper,TextBoxVLower,TextBoxWUpper,TextBoxWLower,TextBoxXUpper,TextBoxXLower,TextBoxYUpper,TextBoxYLower,TextBoxZUpper,TextBoxZLower,TextBoxColon,TextBoxComma,TextBoxPeriod,TextBoxDash,TextBox1,TextBox2,TextBox3,TextBox4,TextBox5,TextBox6,TextBox7,TextBox8,TextBox9,TextBox0,TextBoxQuote,TextBoxAmpers,TextBoxSlash,TextBoxApostrophe,TextBoxQuestion,TextBoxBracketOpen,TextBoxBracketClose,TextBoxEAccent
romdisk/dcfont.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 195 \
	    --only $(DCFONT_ONLY) \
	    --out $(SELF)/$@

# The off-screen arrows: relocData file 166's DObjDesc tree, its three
# one-triangle chevrons and the AnimJoint that blinks them, in one pack.
# The render state the game queues beside the model is baked in with it
# (tools/export/ssb_arrowexport.py).
ARROWTOOL_DEPS = ../../../tools/export/ssb_arrowexport.py $(PACKTOOL_DEPS)

romdisk/ifarrows.mdl: $(ARROWTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_arrowexport.py \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/ifarrows.mdl

# The magnifying glass: relocData file 166's one-triangle handle and
# its 16x16 IA8 frame, the latter as the two ARGB4444 tiles the PVR
# needs to lerp two live colours by a texel (tools/export/ssb_magnifyexport.py).
MAGNIFYTOOL_DEPS = ../../../tools/export/ssb_magnifyexport.py $(PACKTOOL_DEPS)

romdisk/ifmagnify.mdl: $(MAGNIFYTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_magnifyexport.py \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/ifmagnify.mdl

# How to Play's control-stick diagram: relocData file
# 198's three non-sprite entries -- the two-joint stick with its five
# MatAnimJoint tables baked side by side as FPackMObjs alternates, the
# tap-spark flash, and the special-move RGB overlay
# (tools/export/ssb_explainexport.py, src/dc/scexplain.c).
EXPLAINTOOL_DEPS = ../../../tools/export/ssb_explainexport.py \
                   ../../../tools/export/ssb_effectexport.py $(PACKTOOL_DEPS) \
                   ../../dc/decomp/reloc_data.us.h

define explain-rule
romdisk/$(1).mdl: $$(EXPLAINTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_explainexport.py \
	    --rom baserom.z64 --what $(2) \
	    --out $(SELF)/romdisk/$(1).mdl
endef

$(eval $(call explain-rule,scstick,stick))
$(eval $(call explain-rule,scspark,tapspark))
$(eval $(call explain-rule,scrgb,rgb))

EXPLAIN_MODELS = romdisk/scstick.mdl romdisk/scspark.mdl romdisk/scrgb.mdl

# The staff roll's own three: the 56 credit glyphs, one
# joint and one I4 tile each -- the quads scStaffrollInitNameAndJob
# DisplayLists builds in code on the N64, where this port has no runtime
# quad builder at all -- and the two-joint name plaque the backdrop
# behind a shot name is made of (tools/export/ssb_staffrollexport.py,
# src/dc/scstaffroll.c).
#
# The glyphs are TWO files of 28 because src/dc/fighter.h's own
# FIGHTER_MAX_JOINTS is 40 and fighter_init refuses a pack over it (the
# exporter's GLYPH_SPLIT has the whole note); the port picks pack
# i // 28, joint i % 28.
STAFFROLLTOOL_DEPS = ../../../tools/export/ssb_staffrollexport.py \
                     ../../../tools/export/ssb_effectexport.py $(PACKTOOL_DEPS) \
                     ../../dc/decomp/reloc_data.us.h

define staffroll-rule
romdisk/$(1).mdl: $$(STAFFROLLTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_staffrollexport.py \
	    --rom baserom.z64 --what $(2) \
	    --out $(SELF)/romdisk/$(1).mdl
endef

$(eval $(call staffroll-rule,scglyphs0,glyphs0))
$(eval $(call staffroll-rule,scglyphs1,glyphs1))
$(eval $(call staffroll-rule,scplaque,plaque))

STAFFROLL_MODELS = romdisk/scglyphs0.mdl romdisk/scglyphs1.mdl \
                   romdisk/scplaque.mdl

# The blob shadow: relocData file 84's 16x16 I4 quadrant, the one
# texture ft/ftshadow.c loads. No geometry -- ftShadowProcDisplay builds
# its vertices from the floor line every frame
# (tools/export/ssb_shadowexport.py).
SHADOWTOOL_DEPS = ../../../tools/export/ssb_shadowexport.py $(PACKTOOL_DEPS)

romdisk/ftshadow.mdl: $(SHADOWTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_shadowexport.py \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/ftshadow.mdl

# The rebirth halo: relocData file 85's three-node DObjDesc tree, the
# two display lists it queues into the opaque and translucent heads, and
# the AnimJoint that turns it (tools/export/ssb_effectexport.py). The first
# effect in the port that has a model at all.
EFFECTTOOL_DEPS = ../../../tools/export/ssb_effectexport.py $(PACKTOOL_DEPS) \
                  ../../dc/decomp/reloc_data.us.h

romdisk/efhalo.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efhalo.mdl

# The Poke Ball's opening rays: relocData file 85's four-joint
# tree with an MObjSub and a MatAnimJoint -- the halo's file and the halo's
# renderer, but the first effect model in this port whose materials are
# objects rather than baked colours, so the pack carries MObjs the way the
# slash's and the entry vehicles' do.
romdisk/efmballrays.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py \
	    --rom baserom.z64 \
	    --what mballrays \
	    --out $(SELF)/romdisk/efmballrays.mdl

# The item pickup swirl (the item-use step): relocData file 85 again, six
# joints with four MObjs and one animation -- the rays' shape above, made
# by efManagerItemGetSwirlProcUpdate when an item lands in a hand.
romdisk/efswirl.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py \
	    --rom baserom.z64 \
	    --what itemgetswirl \
	    --out $(SELF)/romdisk/efswirl.mdl

# The screen quake: relocData file 83's four AnimJoint scripts and one
# joint, with no geometry at all -- efManagerQuakeMakeEffect drives a
# lone DObj with them and reads its translate back out as the camera's
# velocity (tools/export/ssb_effectexport.py --what quake).
romdisk/efquake.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what quake \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efquake.mdl

# The damage orbs: one quad off relocData file 83's display list, with
# the stand efManagerMakeEffect's flags-0x1 arm builds by hand above it,
# and the one AnimJoint that shrinks it (tools/export/ssb_effectexport.py
# --what orbs).
romdisk/eforbs.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what orbs \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/eforbs.mdl

# The impact wave: relocData file 83's green shockwave, the wave a fighter
# throws when it slams into a wall, ceiling or floor
# (ft/ftcommon/ftcommonwalldamage.c). Its EFFECT_FLAG_USERDATA-only flags
# make it efManagerMakeEffect's simplest branch -- one DObj with the raw
# DL bound straight to it, like the magnify handle -- so the DObjDesc
# offset is a display list and the 18-vertex fan comes out of it, with the
# one MObj, the AnimJoint that spins it open and the MatAnimJoint that
# scrolls its UV (tools/export/ssb_effectexport.py --what impactwave).
romdisk/efimpactwave.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what impactwave \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efimpactwave.mdl

# Mario's fireball model: relocData file 297's 300x300 quad
# and CI4 sprite, the display list wp/wpmanager.c binds straight to the
# weapon DObj -- baked the same way as the impact wave, since the weapon
# model is not an effect but has the same one-DObj-one-DL shape. Shared by
# Mario's and Luigi's fireballs, both palettes baked in as its one MObj's
# two frames (tools/export/ssb_effectexport.py --what fireball).
romdisk/wpmariofireball.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what fireball \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wpmariofireball.mdl

# Fox's Blaster model: relocData file 316's one flat SHADE-
# coloured quad, no texture -- the display list wp/wpmanager.c binds
# straight to the weapon DObj, the same one-DObj-one-DL shape as the
# fireball above, but with FPackBatch.tex == -1 (no texel at all) rather
# than a baked picture (tools/export/ssb_effectexport.py --what blaster).
romdisk/wpfoxblaster.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what blaster \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wpfoxblaster.mdl

# Sector Z's Arwing laser bolt: relocData file 153's one
# display list at 0x1C50, a six-vertex spine-and-fins cross with a single
# 16x16 CI4 tile -- the model BOTH of grsector.c's WPDescs
# (ArwingLaser2D and ArwingLaser3D) name, resolved by following each
# WPAttributes.data's EXTERN reloc chain rather than a symbol the
# generated header carries (tools/export/ssb_effectexport.py --what arwinglaser
# asserts both land on the same bytes).
romdisk/wparwinglaser.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what arwinglaser \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wparwinglaser.mdl

# Samus's Charge Shot and Pikachu's airborne Thunder Jolt:
# the other two weapons whose WPDesc flags are 0x00 and whose
# WPAttributes.p_mobjsubs is NULL, so wp/wpmanager.c binds one display
# list to one DObj and nothing else. Each is one CI4-textured quad with
# its TLUT inline in the tile state -- simpler than the fireball, which
# needs an MObj only because it has two palettes to wind between. The
# display list is in the fighter's Special3 file while the attributes are
# in Special1, and WPAttributes.data is not a symbol in the generated
# header, so the packer decodes it with the (ptr & 0xFFFF) * 4 rule
# (tools/export/ssb_effectexport.py --what chargeshot|thunderjolt).
romdisk/wpsamuschargeshot.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what chargeshot \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wpsamuschargeshot.mdl
romdisk/wppikachujolt.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what thunderjolt \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wppikachujolt.mdl

# Yoshi's stars and his thrown egg. The same one-quad shape
# as the two above, but neither display list is in one of Yoshi's own four
# files -- the stars' is relocData 86 and the egg's is 338 -- so no rule
# over the attribute record's raw word could name them. The packer reads
# the EXTERN reloc chain instead (ssb_assets.py walk_reloc_extern), which
# is what the game itself does at load. The stars are also the first
# weapon whose batch lerps ENV and PRIM by the texel rather than reading
# SHADE: that is where their yellow comes from, and it is the shield
# bubble's material stage.
romdisk/wpyoshistar.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what yoshistar \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wpyoshistar.mdl
romdisk/wpyoshieggthrow.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what yoshieggthrow \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wpyoshieggthrow.mdl

# The Ray Gun's shot: one quad whose WPAttributes are in
# ITCommonData and whose display list the extern chain puts in
# ITCommonObject.
romdisk/wplgunammo.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what lgunammo \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wplgunammo.mdl

# Samus's Bomb, the last of the five weapons whose WPDesc
# flags are 0x00. It is the only one of them that carries an MObj, and
# that is why it could not bake before: its display list branches into
# segment 0xE -- the MObj's own list -- so without the MObjSub the baker
# had nothing to follow there. Its p_mobjsubs is an extern link of its
# own, into the same relocData 320 the display list is in.
#
# Two palettes, carried as two frames of the one MObj the way Mario's and
# Luigi's fireballs are: wpsamusbomb.c:107 winds palette_id between them
# as the fuse burns down, faster the closer it gets.
romdisk/wpsamusbomb.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what samusbomb \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wpsamusbomb.mdl

# Link's boomerang, the first weapon whose model is a
# DObjDesc TREE rather than one display list -- WEAPON_FLAG_DOBJDESC in
# its WPDesc, which is wp/wpmanager.c:262's gcSetupCustomDObjs path. Three
# joints, two batches, and the first weapon pack in this port with an
# ANIMATION: the boomerang turns end over end, and that is an AnimJoint,
# not anything the weapon's own code does. Its tree and its AnimJoint are
# both extern links into relocData 325.
romdisk/wplinkboomerang.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what boomerang \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wplinkboomerang.mdl

# Kirby's Final Cutter blade. Flags 0x03 rather than the
# boomerang's 0x01: a DObjDesc tree whose geometry hangs off DObjDLLink
# lists instead of each node's own dl pointer, so wpManagerMakeWeapon
# gives it wpDisplayDObjTreeDLLinks. Both of its two nodes carry exactly
# one link, which is why the pack needs nothing new to say which link a
# batch belongs to -- the packer checks that rather than assuming it.
romdisk/wpkirbycutter.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what cutter \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wpkirbycutter.mdl

# Master Hand's finger rocket. Flags 0x01 -- the
# boomerang's shape: WEAPON_FLAG_DOBJDESC alone (wp/wpdef.h makes that
# 0x1 and DOBJLINKS 0x2), so each node carries its own dl. Two joints,
# 22 triangles, one 32x32 tile, and NO animation: his bullet only flies
# where wpbossbullet.c pushes it, so BossMainMotion leaves anim_joints
# NULL -- the first weapon tree here that does, and why pack_wptree
# learned to take an absent AnimJoint. Its attributes are in
# BossMainMotion and its model in BossModel; the Normal and the Hard
# bullet name the same DObjDesc array, so one pack serves both rows.
romdisk/wpbossbullet.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what bossbullet \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wpbossbullet.mdl

# Pikachu's ground Thunder Jolt, the crawler his airborne
# jolt becomes on contact. A tree with DL links like the blade, but the
# first weapon whose joints carry MObjs -- six leaves, each drawing one
# triangle with a sprite array of its own -- and the first with a
# MatAnimJoint stepping through them. That is pack_slash's shape.
#
# Its six sprite arrays OVERLAP: node 3's begins at node 2's fourth
# entry, so they are one shared pool each MObj enters at a different
# offset, and a NULL-terminated read gives six different lengths for one
# run. What bounds an array is the script, and one of the six steps
# 0 -> 2 without ever setting 1 -- where the pool holds a genuine NULL.
# The packer fills that never-selected frame with frame 0 and refuses a
# NULL the script really does select.
romdisk/wppikachugjolt.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what groundjolt \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wppikachugjolt.mdl

# Pikachu's Thunder: the bolt's head and the trail segments
# it drops behind it, which share one model between them -- so one pack,
# named twice in src/dc/wpmanager.c.
#
# WPDesc flags 0x02: no WEAPON_FLAG_DOBJDESC, so wp/wpmanager.c takes
# gcAddDObjForGObj's branch, but WEAPON_FLAG_DOBJLINKS, so the renderer
# is gcDrawDObjDLLinksForGObj and what it walks is dobj->dl_link -- the
# same union field gcAddDObjForGObj filled from attr->data. So `data` is
# a DObjDLLink ARRAY here, not a display list, and reading it as a list
# is what made the mesh baker report a gsSPModifyVertex that is not
# there. One link, one list behind it.
#
# Four sprite frames and NO MatAnimJoint: wppikachuthunder.c picks among
# them itself, at random each tic for a trail segment and frame 3
# outright for the head. That makes this the one weapon where the sprite
# array's own NULL terminator is the right bound, because no script
# bounds it.
romdisk/wppikachuthunder.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what thunder \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wppikachuthunder.mdl

# G04: Ness's two specials that throw something. PK Fire is the one
# flags-0x00 weapon whose display list draws through MObjs with no picture:
# two lit triangles, one MObj each, and a MatAnimJoint that flickers their
# light colours (--what pkfire, pack_wpflat). PK Thunder's head is the
# ground jolt's shape -- a tree with DL links, a three-frame sprite array
# stepped by a MatAnimJoint, an AnimJoint pulsing the root's scale -- and
# its trail is Pikachu's Thunder's, one link and one MObj of three sprites
# that wpnesspkthunder.c picks among at random. The reflected head and
# trail are the same two models, so four WPDescs share these two packs.
romdisk/wpnesspkfire.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what pkfire \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wpnesspkfire.mdl
romdisk/wpnesspkthunder.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what pkthunder \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wpnesspkthunder.mdl
romdisk/wpnesspktrail.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what pkthundertrail \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wpnesspktrail.mdl

# Link's Spin Attack -- his Up-B's spinning hitbox (a
# DObjDesc tree with DL links, nine MObjs over two joints) and the glow
# around him while it runs (relocData 353, his Special2).
romdisk/wplinkspin.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what spinattack \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/wplinkspin.mdl
romdisk/efspin.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what spin \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efspin.mdl

# the spark Kirby's rapid jab throws -- relocData 348 again,
# a tree with DL links and no animation at all.
romdisk/efvulcan.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what vulcanjab \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efvulcan.mdl

# Ness's four effects, in three packs. PSI Magnet's bubble
# (relocData 352) and the PK Thunder wave (335) are the entry wave's shape,
# a tree with one DL link and an MObj of two sprites that a MatAnimJoint
# flips, under an AnimJoint. The last segment of PK Thunder's trail and its
# reflected twin's share one pack: a DObjDLLink array bound to one DObj,
# one quad with its own texture and no animation (EFENTRY_LINKDL).
romdisk/efpsimagnet.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what psimagnet \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efpsimagnet.mdl
romdisk/efpkwave.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what pkwave \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efpkwave.mdl
romdisk/efpktrail.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what pktrail \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efpktrail.mdl

# A reflector shattering: relocData 84's five-joint tree,
# three MObjs over three DL links, one AnimJoint and a MatAnimJoint -- the
# swirl's shape. EFENTRY_MOBJ's row says why its two symbols read crossed.
romdisk/efreflectbreak.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what reflectbreak \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efreflectbreak.mdl

# The grab swirl: relocData 84's six-joint tree, four MObjs
# over four DL links, one AnimJoint and a MatAnimJoint -- the reflector
# shards' shape, with the same two symbols crossed.
romdisk/efcatchswirl.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what catchswirl \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efcatchswirl.mdl

# Samus's grapple beam glow: SamusSpecial2's two-joint
# tree, one MObj whose MatAnimJoint flips between two sprites, and an
# AnimJoint.
romdisk/efgrapplebeam.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what grapplebeam \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efgrapplebeam.mdl

# Fox's Reflector: FoxSpecial2's two-joint hexagon and
# its four AnimJoints, Start, Loop, Hit and End.
romdisk/efreflector.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what reflector \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efreflector.mdl

# Falcon Kick's trail: CaptainSpecial2's two-joint tree,
# one MObj and its MatAnimJoint, and an AnimJoint.
romdisk/effalconkick.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what falconkick \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/effalconkick.mdl

# Falcon Punch's flame: CaptainSpecial3's display list
# bound to one DObj, one MObj stepping three sprites.
romdisk/effalconpunch.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what falconpunch \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/effalconpunch.mdl

# Sing's notes: PurinSpecial2's six-joint tree over four
# DL links, four MObjs, an AnimJoint and a MatAnimJoint.
romdisk/efsing.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what sing \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efsing.mdl

# The Poke Ball Pikachu and Jigglypuff are thrown in from:
# ITCommonObject's four-joint tree, one MObj, and an AnimJoint and
# MatAnimJoint per facing, R first.
romdisk/efmballthrown.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what mballthrown \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efmballthrown.mdl

# The ground Thunder Jolt's spark: PikachuSpecial3's
# two-joint tree, one MObj flipping two sprites, an AnimJoint and a
# MatAnimJoint.
romdisk/efjoltspark.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what joltspark \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efjoltspark.mdl

# The Thunder's trail spark: PikachuModel's DObjDLLink array
# on one DObj and one MObj over four sprites, which the code picks.
romdisk/efthundertrail.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what thundertrail \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efthundertrail.mdl

# Kirby's star: ITCommonObject's display list at 0x5458,
# the Star Rod's, on the child of a bare root, as the EFDescs' flags 0x1
# arm builds it.
romdisk/efkirbystar.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what kirbystar \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efkirbystar.mdl

# Pikachu's forward-smash sparks: relocData 347's four-joint
# tree, a plain textured link beside one with an MObj, and three
# AnimJoint/MatAnimJoint pairs baked side by side for the maker to pick.
romdisk/efthundershock.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what thundershock \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efthundershock.mdl

# Mario's (and Luigi's) warp pipe, the first of the ten
# entry vehicles the fighters arrive on. relocData 356's three-joint
# DObjDesc tree and its AnimJoint -- the rebirth halo's shape -- and the
# first model in this port that a FIGHTER's own Special2 file provides.
# It is the first vehicle because its maker is the only one that does
# nothing but pick a file by fighter kind and set a position; Captain
# Falcon's hangs three AnimJoints on the car's wheels by walking the tree
# by hand, and Fox's reaches seven siblings deep for the Arwing's.
#
# Unlike every weapon model here it is LIT -- a solid object standing on
# the stage rather than a billboard -- so its three batches bake to
# shaded 1 and want the scene's lights the way a fighter's model does.
romdisk/efdokan.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what dokan \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efdokan.mdl

# Donkey Kong's barrel and Samus's arrival point, the
# second and third entry vehicles. Both are the pipe's shape -- a
# DObjDesc tree and one AnimJoint -- and both makers do nothing but make
# the effect and set its position, which is the whole reason these three
# came first.
#
# Samus's is the first vehicle whose geometry hangs off DObjDLLink lists
# rather than each node's own dl pointer, and the first model in this
# port with MORE THAN ONE link on a node. The game draws both:
# gcDrawDObjDLLinks walks the array to its terminator and queues each
# list into gSYTaskmanDLHeads[list_id] (objdisplay.c:1561), so a link id
# picks a DL HEAD rather than one list out of several. It is also the
# first MIXED vehicle -- two lit batches and one flat quad for the glow --
# where the pipe and the barrel are lit throughout.
romdisk/eftaru.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what taru \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/eftaru.mdl
romdisk/efpoint.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what point \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efpoint.mdl

# Kirby's entry star, the fourth vehicle and the first
# effect pack in this port with TWO animations: the star sweeps in from
# the right or the left, and efManagerKirbyEntryStarMakeEffect picks by
# writing a block offset into the shared descriptor -- the same thing the
# dead explosion does to its MatAnimJoint. Both blocks are baked side by
# side, R first, and src/dc/efmanager.c picks by index.
#
# It is also a flat billboard with no lit batch at all, where the pipe and
# the barrel are lit throughout and Samus's point is mixed: there is no
# rule about a vehicle's lighting, only a number the packer prints.
romdisk/efstar.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what star \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efstar.mdl

# Link's entry wave and the beam that comes down with it,
# the fifth and sixth vehicles -- and the only entrance in the game that
# makes TWO effects. Both are the pipe's maker again; what is new is in
# the packs, an MObjSub and a MatAnimJoint apiece. Their MObjs carry NO
# sprite array, so what the material animation moves is a colour or a
# texture offset rather than a picture, which is what makes them simpler
# than the ground Thunder Jolt's six.
romdisk/efwave.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what wave \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efwave.mdl
romdisk/efbeam.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what beam \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efbeam.mdl

# Yoshi's entry egg: relocData file 354's one display list -- its EFDesc
# lacks flag 0x4, so what o_dobjsetup names is a DL and not a DObjDesc
# tree -- with the MObj over it stepping a two-sprite array, baked as a
# run of two textures the FPackMObjs indexes.
romdisk/efentryegg.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what egg \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efentryegg.mdl

# Fox's Arwing: relocData 161's twelve-joint DObjDesc tree and its DL
# links, with the two AnimJoint arrays the maker picks between read out
# of relocData 346 -- the first vehicle whose animation is in a different
# file from its geometry. Both arrays come across as animations of the
# one pack and src/dc/efmanager.c picks by index, R first.
romdisk/efarwing.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what arwing \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efarwing.mdl

# Captain Falcon's Blue Falcon: relocData 350's twelve-joint tree and its
# ten DL links, with the AnimJoint array at 0x6200 baked WITH the eight
# entries the maker overwrites by hand already written into it -- the
# wheel scripts at 0x6518 and 0x6598 (EFENTRY_ANIMPATCH). What is left of
# the maker's loop is the matrix kind, which no pack can carry.
romdisk/efcar.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what car \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efcar.mdl

# The egg a fighter caught by Yoshi's Egg Lay sits inside: relocData
# 339's two-joint tree and THREE AnimJoints over it -- the throw, the
# wait and the break -- baked throw first, because index 0 is the one
# efManagerAddModel applies from the descriptor and the other two are
# what efManagerYoshiEggLaySetAnim picks between.
romdisk/efegglay.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what egglay \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efegglay.mdl

# Kirby's Final Cutter: four trees out of relocData 348, his Special2 --
# the two arcs of the swing (untextured triangles, like the boomerang's
# motion trail), the blade he draws (no animation at all) and the trail
# behind it (the only one with DL links).
romdisk/efcutup.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what cutterup \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efcutup.mdl
romdisk/efcutdown.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what cutterdown \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efcutdown.mdl
romdisk/efcutdraw.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what cutterdraw \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efcutdraw.mdl
romdisk/efcuttrail.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what cuttertrail \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efcuttrail.mdl

# The damage slash: relocData file 83's three-joint DObjDesc tree, its two
# display lists, and the MObjs over them -- each with a sprite array its
# MatAnimJoint steps through, which the pack carries as a run of textures
# (tools/export/ssb_effectexport.py --what slash).
#
# It is built into disc/ and not into romdisk/, which is the one asset in
# the tree that is not in the ELF. Thirteen frames at 16 bits a texel is
# 43 KB, and a VS battle's malloc heap has less than that to spare: the
# ELF is 14.6 MB of a 16 MB machine, `ifstatus.spr` and `ifannounce.spr`
# have been failing to load out of that same shortage since before this
# effect existed, and measured both ways the pack in
# the romdisk turns that shortage into an abort a few seconds into the
# match while the same build without it runs at 59.9 fps. So the pack is
# built and checked (tools/check/slash_check.py) and left off the ELF, where
# efManagerAddModel refuses the effect and says so once per attempt.
#
# It is about three times the size it needs to be, for two reasons that
# are neither of them this rule's: the streak's tile is 16 texels wide
# and mirrored to 32, which the PVR can do itself with uv_flip rather
# than in the texels, and its 48 rows are padded to the PVR's 64. Fixing
# either shrinks every pack in the build and belongs in its own change; the disc,
# which holds the romdisk, removes the question.
disc/efslash.mdl: $(EFFECTTOOL_DEPS)
	@mkdir -p $(SELF)/disc
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what slash \
	    --rom baserom.z64 \
	    --out $(SELF)/disc/efslash.mdl

# The damage sparks' pack: the two DObjs the flyer's EFDesc
# builds by hand, the quad its display list draws, its AnimJoint and the
# MatAnimJoint that steps its MObj's seven-frame sprite array
# (tools/export/ssb_effectexport.py --what spark, checked by
# tools/check/spark_check.py). 15,204 bytes, of which 14,336 is the seven 32x32
# frames at 16 bits a texel.
#
# It is in the romdisk because src/dc/efmanager.c does not reserve a
# 17,400-byte Fighter in .bss per effect model, which keeps the heap base 86,016
# bytes higher -- five and a half times this file. Were it left out of the
# ELF's romdisk the heap would shrink and a one-player Castle battle could not
# find the 16 KB thread the announcer wants.
# See the note on sEFManagerModels there for the two measurements.
romdisk/efspark.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what spark \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efspark.mdl

# The metal dust's pack: the sparks' pack again out of four
# blocks of its own -- two DObjs, one quad, an AnimJoint and a
# MatAnimJoint stepping another seven-frame sprite array
# (tools/export/ssb_effectexport.py --what mdust, checked by
# tools/check/mdust_check.py). 15,184 bytes, of which 14,336 is the seven 32x32
# frames.
#
# Those 14,336 are not four times the ROM's, the way the slash's and the
# sparks' are: this effect's sprites are IA16 and the pack's are ARGB4444,
# sixteen bits a texel both ways, so there is nothing to take
# off this one.
#
# It is in the ELF's romdisk, and it is the first effect model with a
# picture that a battle actually draws: a one-player Castle battle making
# one of these every twenty frames runs at 59.9 fps with the fighter's
# 442 triangles going to 452 as the flyers come and go. What made room
# for it is the .bss the same step stopped reserving -- see the note on
# sEFManagerModels in src/dc/efmanager.c.
romdisk/efmdust.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what mdust \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efmdust.mdl

# The dead explosion's pack: the burst a fighter leaves
# when he is knocked off the screen, and the last effect model the port
# owed. Four joints -- a stand and three shards -- twelve triangles,
# three MObjs and *four* MatAnimJoints, one per player, baked side by
# side (tools/export/ssb_effectexport.py --what deadexplode, checked by
# tools/check/dexp_check.py). 17,116 bytes, almost all of it the three
# pictures: a 64x64, a 96x32 and a 32x64, ARGB4444.
#
# The first pack whose batches lerp between two flat colours -- ENV and
# PRIM -- rather than tinting one, which is how the player's colour gets
# into a shared model: see fighter.h's FPACK_ENVLERP.
romdisk/efdexp.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what deadexplode \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efdexp.mdl

# The shield bubble and Yoshi's egg: one quad each, out of
# the common fighter file (relocData 163) and Yoshi's model file (338)
# rather than an effects file, drawn as billboards at YRotN
# (tools/export/ssb_effectexport.py --what shield, --what yoshiegg).
romdisk/efshield.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what shield \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efshield.mdl

romdisk/efyegg.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what yoshiegg \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efyegg.mdl

# The small shock burst: efManagerShockSmallMakeEffect's
# own picture, EFCommonEffects2's own file rather than EFCommonEffects1
# (relocData 84 vs 82) -- one DObj, no stand, the Yoshi egg's shape
# above, and a five-frame MatAnimJoint on its one MObj the way the
# sparks' and the metal dust's own step through seven
# (tools/export/ssb_effectexport.py --what shock).
romdisk/efshock.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what shock \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/efshock.mdl

# The fire spark: EFCommonEffects2's own DObjDesc tree --
# a root and one child at (0, -90, 0), the position baked at load rather
# than written by the maker, so this is the dead explosion's shape
# (tools/export/ssb_effectexport.py --what firespark), not the sparks' hand-
# built one. A real AnimJoint drives the child's position and a
# MatAnimJoint steps its four-frame sprite array.
romdisk/effirespark.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py --what firespark \
	    --rom baserom.z64 \
	    --out $(SELF)/romdisk/effirespark.mdl

# The nine particle banks: eighteen ROM byte ranges, each cut out whole
# and byteswapped into the SH-4's order, so lb/lbparticle.c's own
# arithmetic makes the same pointers out of them that the N64 made
# (tools/export/ssb_particleexport.py, and tools/check/particle_check.py against
# the decomp's own particle bank sources). 394 KB, of which efcommon.txb is
# 307: nothing loads a bank it does not use, and a battle loads one.
PARTICLETOOL_DEPS = ../../../tools/export/ssb_particleexport.py
SINTOOL_DEPS = ../../../tools/export/ssb_sinexport.py
PARTICLE_NAMES = efcommon particles_unk0 particles_unk1 particles_unk2 itcommon grpupupu grhyrule gryoster mntitle
PARTICLE_BANKS = $(foreach b,$(PARTICLE_NAMES),\
                     romdisk/$(b).scb romdisk/$(b).txb romdisk/$(b).txp)

define particle-bank-rule
romdisk/$(1).$(2): $$(PARTICLETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_particleexport.py \
	    --rom baserom.z64 \
	    --bank $(1) --kind $(2) --out $(SELF)/romdisk/$(1).$(2)
endef
$(foreach b,$(PARTICLE_NAMES),\
    $(foreach k,scb txb,$(eval $(call particle-bank-rule,$(b),$(k)))))

# ...and the same banks' images again, converted for the PVR. The .txb
# above is the interpreter's and stays the ROM's bytes; this is the
# renderer's, because the PVR reads none of the N64's nine texel formats.
# 566 KB over the nine, of which efcommon is 481: a battle uploads that
# one and gives the VRAM back with the stage (src/dc/lbpartex.h).
PARTICLETEXTOOL_DEPS = ../../../tools/export/ssb_particletexexport.py \
                       ../../../tools/export/ssb_particleexport.py \
                       ../../../tools/lib/ssb_assets.py \
                       ../../../tools/lib/ssb_texconv.py

define particle-tex-rule
romdisk/$(1).txp: $$(PARTICLETEXTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_particletexexport.py \
	    --rom baserom.z64 \
	    --bank $(1) --out $(SELF)/romdisk/$(1).txp
endef
$(foreach b,$(PARTICLE_NAMES),$(eval $(call particle-tex-rule,$(b))))

# ...and the addresses those ranges had in the ROM, as absolute symbols.
# The game takes the address of six of them -- &lEFCommonParticleScriptBankLo
# and its fifteen siblings -- and hands it to efParticleGetLoadBankID, so
# with these defined ef/efparticle.c compiles unmodified and src/dc/dma.c
# turns the address back into the file above. Absolute assignments cost no
# space and no relocation. sh-elf prefixes C symbols with an underscore;
# the host test's link does not, so it generates its own.
particlebanks.ld: $(PARTICLETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_particleexport.py \
	    --emit-ld $(SELF)/particlebanks.ld --symbol-prefix _
hosttest_particlebanks.ld: $(PARTICLETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_particleexport.py \
	    --emit-ld $(SELF)/hosttest_particlebanks.ld

# The weapon-table offsets, the same way (tools/export/ssb_wpattrexport.py).
wpattrs.ld: $(WPATTRTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_wpattrexport.py \
	    --emit-ld $(SELF)/wpattrs.ld --symbol-prefix _
# --record-size 72: the host's WPAttributes (x86-64 gcc: four 8-byte
# pointers, then the N64's own 36 bytes, padded to 8). The host test asserts
# that sizeof, so a toolchain where it differs fails there, not silently.
hosttest_wpattrs.ld: $(WPATTRTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_wpattrexport.py \
	    --emit-ld $(SELF)/hosttest_wpattrs.ld --record-size 72

# The music: the whole bank, once, in the AICA's own sound RAM.
# tools/export/ssb_bgmpack.py decodes B1_sounds1's 117 waveforms and re-encodes
# them as the Yamaha ADPCM the AICA decodes in hardware -- 890 KB for
# every instrument in the game -- and carries all 47 compressed-MIDI
# sequences out of S1_music.sbk verbatim, 155 KB, which is the only part
# that stays in main RAM. bgmbank.c streams the samples straight to the
# SPU, so the blob is never resident, and src/dc/seq/seqcore.c (in the
# AICA firmware) is the sequencer over them.
BGMBANK ?= 1
BGMPACK_DEPS = ../../../tools/export/ssb_bgmpack.py ../../../tools/lib/ssb_adpcm.py \
               ../../../tools/lib/ssb_sfxexport.py \
               ../../../tools/export/ssb_bgmexport.py \
               $(SSB_DECOMP_DIR)/tools/audio_codec.py
ifeq ($(BGMBANK),1)
BGM_BANK = romdisk/bgm.pak
else
BGM_BANK =
endif

romdisk/bgm.pak: $(BGMPACK_DEPS)
	cd ../../.. && python3 tools/export/ssb_bgmpack.py \
	    --rom baserom.z64 \
	    --out $(SELF)/$@

# Every playable fighter, one pack each, loaded per scene by
# ftManagerSetupFilesAllKind (src/dc/ftmanager.c) from /rd/<name>.pack
# and relocated where it lies in the romdisk (fighter_load). A clone's
# pack carries the animations its motion table borrows -- Luigi Mario's,
# Purin Kirby's -- because the game names them by file id and never asks
# whose they are. 5.8 MB between them, resident in the ELF when the romdisk is linked in.
FIGHTERS = mario fox donkey samus luigi link yoshi captain kirby \
           pikachu purin ness
# The 1P game's own fighter kinds, which are NOT part of
# $(FIGHTERS): they have no costume on a character select, no
# congratulations picture, no Sound Test entry and no character-data
# page, so every $(foreach f,$(FIGHTERS),...) below would be wrong for
# them. They export through the same two rules and nothing else.
#
# Giant Donkey Kong has no model or animation file of his own at all --
# ft/ftdata.c's dFTGDonkeyData names llDonkeyModelFileID and
# llDonkeyMainMotionFileID -- so his pack is Donkey Kong's geometry and
# Donkey Kong's 153 animations under GDonkeyMain's own FTAttributes
# (size 2.0 rather than 1.25, and twenty more numbers with it). The
# exporter reads whose files a kind is made of off that table rather
# than off its name; tools/lib/ssb_meshexport.py's motion_source says so.
LADDER_FIGHTERS = gdonkey mmario
# The twelve Fighting Polygons, which take a pack and an
# emblem-only sprite bank: relocData 207_NMarioMain.c and its eleven
# siblings give FTSprites { NULL, NULL, dMasterHandIcon_FTEmblem } -- no
# stock icon and no stock palettes of their own. The game draws their
# stocks from llFTStocksZakoSprite, the grey head in file 25 that
# sc1pgame.c:1815 already reads, and their emblem is Master Hand's,
# which ifCommonPlayerDamageInitInterface draws behind the damage
# readout. So they are their own list rather than more LADDER_FIGHTERS.
POLY_FIGHTERS = nmario nfox ndonkey nsamus nluigi nlink \
                nyoshi ncaptain nkirby npikachu npurin nness
# Master Hand, rung 13's opponent, who is a fighter kind
# the same way the Polygons are -- ft/ftdata.c gives him dFTBossData, a
# motion table and a Main file -- and is on no list above. He takes a
# pack and a sprite bank: 250_BossMain.c's FTSprites names a stock icon,
# its one palette and an emblem, all in 345_MasterHandIcon.c. The emblem
# is the one behind his H.P readout. He is his own list rather than more
# $(POLY_FIGHTERS), which are twelve of one thing; he is one of another.
BOSS_FIGHTERS = boss
FIGHTER_PACKS = $(foreach f,$(FIGHTERS) $(LADDER_FIGHTERS) $(POLY_FIGHTERS) \
                           $(BOSS_FIGHTERS),romdisk/$(f).pack)
# Each pack's animations (src/dc/fighter.h FPackAnm), which
# ssb_packexport.py writes beside it, just after it: a pack's rule makes
# both, and this one only checks the .anm came with it.
FIGHTER_ANMS = $(FIGHTER_PACKS:.pack=.anm)
$(FIGHTER_ANMS): romdisk/%.anm: romdisk/%.pack
	@test -f $@ -a ! $@ -ot $< || { echo "$@ was not written with $<"; exit 1; }
FIGHTER_BANKS = $(foreach f,$(FIGHTERS) $(LADDER_FIGHTERS) $(POLY_FIGHTERS) \
                           $(BOSS_FIGHTERS),romdisk/ft$(f).spr)
# the decomp's name for a fighter: capitalised
fighter-name = $(shell echo $(1) | sed 's/./\U&/')

define FIGHTER_PACK_RULE
romdisk/$(1).pack: $$(FIGHTERTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_packexport.py \
	    --rom baserom.z64 \
	    --fighter $(call fighter-name,$(1)) \
	    --out $(SELF)/romdisk/$(1).pack
endef
$(foreach f,$(FIGHTERS),$(eval $(call FIGHTER_PACK_RULE,$(f))))

# The ladder kinds' pack and bank, same two tools with the same
# arguments; only the decomp's name for them is not $(fighter-name)'s
# one-letter capitalisation, so each carries its own.
ladder-name-gdonkey = GDonkey
ladder-name-mmario = MMario
ladder-name-nmario = NMario
ladder-name-nfox = NFox
ladder-name-ndonkey = NDonkey
ladder-name-nsamus = NSamus
ladder-name-nluigi = NLuigi
ladder-name-nlink = NLink
ladder-name-nyoshi = NYoshi
ladder-name-ncaptain = NCaptain
ladder-name-nkirby = NKirby
ladder-name-npikachu = NPikachu
ladder-name-npurin = NPurin
ladder-name-nness = NNess
ladder-name-nmario = NMario
ladder-name-nfox = NFox
ladder-name-ndonkey = NDonkey
ladder-name-nsamus = NSamus
ladder-name-nluigi = NLuigi
ladder-name-nlink = NLink
ladder-name-nyoshi = NYoshi
ladder-name-ncaptain = NCaptain
ladder-name-nkirby = NKirby
ladder-name-npikachu = NPikachu
ladder-name-npurin = NPurin
ladder-name-nness = NNess

define LADDER_FIGHTER_RULES
romdisk/$(1).pack: $$(FIGHTERTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_packexport.py \
	    --rom baserom.z64 \
	    --fighter $$(ladder-name-$(1)) \
	    --out $(SELF)/romdisk/$(1).pack

romdisk/ft$(1).spr: $$(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 \
	    --fighter $$(ladder-name-$(1)) \
	    --out $(SELF)/romdisk/ft$(1).spr
endef
$(foreach f,$(LADDER_FIGHTERS),$(eval $(call LADDER_FIGHTER_RULES,$(f))))

# A Polygon's pack, and its sprite bank: the emblem alone for a
# Polygon, the stock icon and the emblem for Master Hand
# (ssb_spriteexport.py fighter_sprite_parts reads which).
define POLY_FIGHTER_RULES
romdisk/$(1).pack: $$(PACKTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_packexport.py \
	    --rom baserom.z64 \
	    --fighter $$(ladder-name-$(1)) \
	    --out $(SELF)/romdisk/$(1).pack

romdisk/ft$(1).spr: $$(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 \
	    --fighter $$(ladder-name-$(1)) \
	    --out $(SELF)/romdisk/ft$(1).spr
endef
$(foreach f,$(POLY_FIGHTERS),$(eval $(call POLY_FIGHTER_RULES,$(f))))

# Master Hand's pack and bank, on the same rule.
ladder-name-boss = Boss
$(foreach f,$(BOSS_FIGHTERS),$(eval $(call POLY_FIGHTER_RULES,$(f))))

# The stages, one pack each, into gGRStages (src/dc/stage.h): the stage
# select previews from them and the battle binds one. All nine VS stages
# export. This comment said three of them could not -- Dream Land and
# Saffron City "still want the tile-size and texture-scale formulas of
# gcDrawMObjForDObj" and Mushroom Kingdom was "locked until the save
# defaults land" -- and all three were stale: MeshBaker._mobj_tile
# (tools/lib/ssb_assets.py) has implemented those formulas since the effect
# models needed them, and the save defaults exist. All three are verified by
# exporting them.
#
# Two of the three still have no gr/ logic file (Saffron's Gate monster
# and Mushroom Kingdom's `?` block both want the unported item roster),
# so their packs are built and selectable but their stage has no hazard.
STAGES = romdisk/castle.stg romdisk/jungle.stg romdisk/zebes.stg \
         romdisk/hyrule.stg romdisk/sector.stg romdisk/yoster.stg \
         romdisk/pupupu.stg romdisk/yamabuki.stg romdisk/inishie.stg \
         romdisk/last.stg \
         romdisk/zako.stg romdisk/metal.stg romdisk/yostersmall.stg \
         $(BONUS1_STAGES) \
         $(BONUS2_STAGES) \
         romdisk/bonus2plat.pak \
         romdisk/bonus3.stg \
         romdisk/itcommon.itp romdisk/wpattrs.bin

romdisk/castle.stg: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --stage Castle \
	    --out $(SELF)/$@

romdisk/jungle.stg: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --stage Jungle \
	    --out $(SELF)/$@

romdisk/zebes.stg: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --stage Zebes \
	    --out $(SELF)/$@

romdisk/hyrule.stg: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --stage Hyrule \
	    --out $(SELF)/romdisk/hyrule.stg

romdisk/sector.stg: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --stage Sector \
	    --out $(SELF)/$@

romdisk/yoster.stg: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --stage Yoster \
	    --out $(SELF)/$@

# The item pack: ITCommonData + ITCommonObject in one blob, see
# tools/export/ssb_itemexport.py and src/dc/itempack.h. Not a stage, but built
# the same way and for the same reason -- the ROM is the authority and
# nothing downstream of it can be trusted to agree.
romdisk/itcommon.itp: $(ITEMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_itemexport.py \
	    --rom baserom.z64 \
	    --out $(SELF)/$@

# The fighters' weapon hitbox tables, one blob per relocData file with each
# record at its ROM offset; see tools/export/ssb_wpattrexport.py and
# src/dc/wpattrs.h. The .ld beside particlebanks.ld gives the ll* symbols
# those offsets.
romdisk/wpattrs.bin: $(WPATTRTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_wpattrexport.py \
	    --rom baserom.z64 \
	    --out $(SELF)/$@

# The item models (src/dc/itemmodel.c's own table names them; the
# exporter's --emit-table prints that table and --list what each bakes).
# ONE run makes all 34, and the stamp is the target: every invocation
# re-reads and re-parses the whole ROM, so a rule per file would parse it
# 32 times over, and 32 outputs from one command is not a shape make can
# track. The stamp lives OUTSIDE romdisk/ on purpose -- tools/
# disc_layout.py refuses a staged file the manifest does not name, and a
# build artifact is not a disc file.
ITEM_MODEL_NAMES = capsule tomato heart star sword bat harisen lgun \
                   fflower hammer msbomb bombhei starrod gshell rshell \
                   box taru mball \
                   wark kabigon tosakinto mew nyars lizardon spear kamex \
                   mlucky egg starmie sawamura dogas pippi gbumper nbumper
# The seven monster WEAPON models, from ITCommonObject's
# `WeaponAttributes` blocks -- the same file and the same walk as the items
# above, but keyed by the weapon's own table rather than by an item's. Five
# of that file's twelve weapon tables still have no model; the exporter
# names each with its reason (KNOWN_UNBAKED in
# tools/export/ssb_itemmodelexport.py) and exits non-zero on any NEW one.
WPMONSTER_NAMES = warkrock nyarscoin spearswarm kamexhydro starmieswift \
                  dogassmog pippiswarm
WPMONSTER_MODELS = $(addprefix romdisk/wp,$(addsuffix .mdl,$(WPMONSTER_NAMES)))
# The STAGE items' models, baked out of the STAGE's own
# model files rather than ITCommonObject -- PowerBlock's tree is
# StageInishieFile3's DObjDesc at 0x11F8, which is what the map group's
# `DataStart PowerBlock` names. Same bake, same .mdl shape; the only
# difference is the file and the texture resolver.
STAGEITEM_NAMES = powerblock pakkun glucky porygon marumine hitokage fushigibana tarubomb target
STAGEITEM_MODELS = $(addprefix romdisk/it,$(addsuffix .mdl,$(STAGEITEM_NAMES)))
# The FIGHTER items: PK Fire's pillar, baked by the same
# --stage-items run out of NessSpecial3 with its AnimJoint table carried in
# the pack, because itnesspkfire.c is compiled unmodified and has nowhere of
# the port's own to attach a named script.
FIGHTERITEM_NAMES = nesspkfire linkbomb
FIGHTERITEM_MODELS = $(addprefix romdisk/it,$(addsuffix .mdl,$(FIGHTERITEM_NAMES)))

# The display lists five items swap in while they play (the Bob-omb's walk,
# the attached Bumper, three monsters' attack poses), one-joint packs that
# src/dc/itemmodel.c itemModelSetDisplayList switches the DObj to --
# ALT_DISPLAY_LISTS in tools/export/ssb_itemmodelexport.py.
ITALT_NAMES = bombheiwalkl bombheiwalkr nbumperwait kamex sawamura wark
ITALT_MODELS = $(addprefix romdisk/italt,$(addsuffix .mdl,$(ITALT_NAMES)))

ITEM_MODELS = $(addprefix romdisk/it,$(addsuffix .mdl,$(ITEM_MODEL_NAMES))) \
              romdisk/itboxsmash.mdl romdisk/ittarubombsmash.mdl \
              $(WPMONSTER_MODELS) $(STAGEITEM_MODELS) \
              $(FIGHTERITEM_MODELS) \
              $(ITALT_MODELS)

$(SELF)/.itmodels.stamp: $(ITEMBAKE_DEPS)
	cd ../../.. && python3 tools/export/ssb_itemmodelexport.py \
	    --rom baserom.z64 --all \
	    --outdir $(SELF)/romdisk && \
	    python3 tools/export/ssb_itemmodelexport.py \
	        --rom baserom.z64 --effect boxsmash \
	        --out $(SELF)/romdisk/itboxsmash.mdl && \
	    python3 tools/export/ssb_itemmodelexport.py \
	        --rom baserom.z64 \
	        --effect tarubombsmash \
	        --out $(SELF)/romdisk/ittarubombsmash.mdl && \
	    python3 tools/export/ssb_itemmodelexport.py \
	        --rom baserom.z64 --weapons \
	        --outdir $(SELF)/romdisk && \
	    python3 tools/export/ssb_itemmodelexport.py \
	        --rom baserom.z64 --stage-items \
	        --outdir $(SELF)/romdisk && \
	    python3 tools/export/ssb_itemmodelexport.py \
	        --rom baserom.z64 --alts \
	        --outdir $(SELF)/romdisk && touch $(SELF)/.itmodels.stamp

$(ITEM_MODELS): $(SELF)/.itmodels.stamp

romdisk/pupupu.stg: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --stage Pupupu \
	    --out $(SELF)/$@

romdisk/yamabuki.stg: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --stage Yamabuki \
	    --out $(SELF)/$@

romdisk/inishie.stg: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --stage Inishie \
	    --out $(SELF)/$@

# Final Destination, the Master Hand rung's stage. Not a VS
# stage -- the stage select cannot reach it -- but it is built with the
# nine because it is one of them in every other way: one map file, one
# wallpaper, one animated layer.
romdisk/last.stg: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --stage Last \
	    --out $(SELF)/$@

# The other three stages only the 1P ladder plays: the
# Duel Zone (rung 12), Meta Crystal (rung 10) and the small Yoshi's
# Island (rung 1). Like last.stg they are unreachable from the stage
# select, and like it they are built with the nine because they are one
# of them in every other way. The pack's name is the exporter's stage
# name lowercased, as everywhere else here.
romdisk/zako.stg: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --stage Zako \
	    --out $(SELF)/$@

romdisk/metal.stg: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --stage Metal \
	    --out $(SELF)/$@

romdisk/yostersmall.stg: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --stage YosterSmall \
	    --out $(SELF)/$@

# Break the Targets, one course per fighter. Twelve
# stages of one shape, so one template rather than twelve copies of
# last.stg's rule above: the exporter's stage name is Bonus1<Fighter>
# and the pack is bonus1<fighter>.stg, lowercase, which is the same
# spelling src/dc/stage.c's dGRStageFiles uses. tools/export/ssb_stageexport.py
# says what is in one and what is deliberately not (the targets).
# Each token is <pack name>:<exporter stage name>; make has no lowercase
# function, so the pair is written out rather than derived.
BONUS1_COURSES = mario:Mario fox:Fox donkey:Donkey samus:Samus luigi:Luigi link:Link yoshi:Yoshi captain:Captain kirby:Kirby pikachu:Pikachu purin:Purin ness:Ness
BONUS1_STAGES = $(foreach c,$(BONUS1_COURSES),romdisk/bonus1$(firstword $(subst :, ,$(c))).stg)

define BONUS1_STAGE_RULE
romdisk/bonus1$(firstword $(subst :, ,$(1))).stg: $$(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 \
	    --stage Bonus1$(lastword $(subst :, ,$(1))) --out $$(SELF)/$$@
endef
$(foreach c,$(BONUS1_COURSES),$(eval $(call BONUS1_STAGE_RULE,$(c))))

# Board the Platforms, the same twelve fighters and the
# same template. tools/export/ssb_stageexport.py says what a course carries and
# what it does not yet (the platforms and five courses' bumpers).
BONUS2_COURSES = mario:Mario fox:Fox donkey:Donkey samus:Samus luigi:Luigi link:Link yoshi:Yoshi captain:Captain kirby:Kirby pikachu:Pikachu purin:Purin ness:Ness
BONUS2_STAGES = $(foreach c,$(BONUS2_COURSES),romdisk/bonus2$(firstword $(subst :, ,$(c))).stg)

define BONUS2_STAGE_RULE
romdisk/bonus2$(firstword $(subst :, ,$(1))).stg: $$(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 \
	    --stage Bonus2$(lastword $(subst :, ,$(1))) --out $$(SELF)/$$@
endef
$(foreach c,$(BONUS2_COURSES),$(eval $(call BONUS2_STAGE_RULE,$(c))))

# The six platform trees all twelve of those courses build their
# platforms from. relocData 136 Bonus2Common, so no
# --stage: baked they are 229 KB, which is why they are a file of their
# own rather than a block in each of the twelve packs.
romdisk/bonus2plat.pak: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --platforms \
	    --out $(SELF)/$@

# Race to the Finish, the third bonus stage and the only
# one that is one course rather than twelve, so it gets a rule of its
# own the way last.stg does.
romdisk/bonus3.stg: $(STAGETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_stageexport.py \
	    --rom baserom.z64 --stage Bonus3 \
	    --out $(SELF)/$@

# The title screen's sprites: relocData file 167 (MNTitle), all fourteen,
# as one bank the scene loads on its way in (src/dc/mntitle.c).
romdisk/mntitle.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 167 \
	    --out $(SELF)/romdisk/mntitle.spr

# The rest of the title (src/dc/mntitle.c): the flames' thirty frames,
# relocData file 168 (MNTitleFireAnim), and file 167's two bare animation
# trees, the labels' and PRESS START's, as packs of joints and AnimJoint
# words with no geometry (tools/export/ssb_transexport.py TREES).
TITLE_ASSETS = romdisk/mntitlefire.spr romdisk/mntitlelabels.tra \
               romdisk/mntitlepress.tra \
               romdisk/mntitlelogo.tra romdisk/mntitlefiretree.tra \
               romdisk/mntitleslash.mdl

romdisk/mntitlefire.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 168 \
	    --out $(SELF)/romdisk/mntitlefire.spr

romdisk/mntitlelabels.tra: $(TRANSTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_transexport.py \
	    --rom baserom.z64 --name TitleLabels \
	    --out $(SELF)/romdisk/mntitlelabels.tra

romdisk/mntitlepress.tra: $(TRANSTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_transexport.py \
	    --rom baserom.z64 --name TitlePressStart \
	    --out $(SELF)/romdisk/mntitlepress.tra

# The title after the opening movie: the animated logo's
# tree and the logo fire's, two more bare trees of file 167, and the
# slash, a real model with MObjs and both animation kinds
# (tools/export/ssb_effectexport.py's titleslash row).
romdisk/mntitlelogo.tra: $(TRANSTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_transexport.py \
	    --rom baserom.z64 --name TitleLogo \
	    --out $(SELF)/romdisk/mntitlelogo.tra

romdisk/mntitlefiretree.tra: $(TRANSTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_transexport.py \
	    --rom baserom.z64 --name TitleFire \
	    --out $(SELF)/romdisk/mntitlefiretree.tra

romdisk/mntitleslash.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py \
	    --rom baserom.z64 \
	    --what titleslash --out $(SELF)/$@

# The HUD's sprites, one bank per relocData file the battle scene loads
# (src/dc/gmcommon.c): the damage digits (164), the stock digits (36),
# the player tags (38), the match clock (165), and GO!, GAME SET, TIME UP
# and the countdown (82).
# ifannounce.spr, the announcer's common alphabet (file 37), is the
# HUD's too: SUDDEN DEATH is the one banner in the
# ported game that spells itself out of the alphabet rather than out of
# file 82's pre-coloured letters. Its recipe is with the results
# screen's banks below, which wanted the same file first.
HUD_BANKS = romdisk/ifdamage.spr romdisk/ifdigits.spr romdisk/iftags.spr \
            romdisk/ifstatus.spr romdisk/iftimer.spr romdisk/ifannounce.spr \
            romdisk/ifpause.spr romdisk/ifcommonitem.spr \
            romdisk/sc1ptrain.spr $(TRAINING_WALLPAPER_BANKS)

# Training mode's three flat backgrounds (sc1PTrainingModeLoadWallpaper,
# src/dc/sc1ptrainingmode.c): relocData 26 GRWallpaperTrainingBlack, 27
# -Yellow and 28 -Blue, one 300x220 sprite each at 0x20718. The game
# swaps one into MPGroundData.wallpaper by the stage's
# dSC1PTrainingModeWallpaperIDs row, and the stage select's training
# preview shows the same one (src/dc/mnmaps.c).
TRAINING_WALLPAPER_BANKS = romdisk/grwptrainingblack.spr \
                           romdisk/grwptrainingyellow.spr \
                           romdisk/grwptrainingblue.spr

romdisk/grwptrainingblack.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 26 \
	    --out $(SELF)/$@

romdisk/grwptrainingyellow.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 27 \
	    --out $(SELF)/$@

romdisk/grwptrainingblue.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 28 \
	    --out $(SELF)/$@

# Training mode's HUD and pause menu (src/dc/sc1ptrainingmode.c). The game
# reads four layout tables out of relocData file 254
# whose every entry points across into file 29 IFCommonItemNames, the
# shared glyph atlas: the four stat labels, the ten menu labels and the
# two glyph streams the CP / ITEM / SPEED / VIEW rows are spelled from.
# The port keeps the tables in C (they are four small arrays of a
# position and an offset) and exports only the 84 sprites of file 29
# they name -- the other 34 are the JP variants, whose readers are the
# seven functions the port cuts.
SC1PTRAIN_ONLY = ArrowL,ArrowR,AttackText,AttackText2,BeamSwordText,BeamSwordText2,BobOmbText,BobOmbText2,BoxBottom,BoxLeft,BoxRight,BoxTop,BumperText,BumperText2,CPColonText,CloseUpText,ClosingBracket,ComboColonText,DamageColonText,Digit0,Digit1,Digit2,Digit3,Digit4,Digit5,Digit6,Digit7,Digit8,Digit9,EnemyColonText,EvadeText,EvadeText2,ExitText,FanText,FanText2,FireFlowerText,FireFlowerText2,GreenShellText,GreenShellText2,HammerText,HammerText2,HeartText,HeartText2,HomerunBatText,HomerunBatText2,ItemColonText,JumpText,JumpText2,MaximTomatoText,MaximTomatoText2,MotionSensorBomb,MotionSensorBomb2,NoneText,NoneText2,NormalText,OneSlashFourText,OneSlashFourText2,OneSlashOneText,OneSlashOneText2,OneSlashTwoText,OneSlashTwoText2,OpeningBracket,PercentageSign,PokeballText,PokeballText2,RayGunText,RayGunText2,RedShellText,RedShellText2,RedSphere,ResetText,SpeedColonText,SpeedText,StandText,StandText2,StarRodText,StarRodText2,StarText,StarText2,TwoSlashThreeText,TwoSlashThreeText2,ViewColonText,WalkText,WalkText2

romdisk/sc1ptrain.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 29 \
	    --only $(SC1PTRAIN_ONLY) \
	    --out $(SELF)/$@

# The item arrow (src/dc/ifcommon.c ifCommonItemArrowSetAttr): relocData
# file 87 IFCommonItem, which is one sprite and nothing else -- the red
# pointer a dropped item shows while it can be picked up. The game loads
# the file into the heap on first use from itManagerInitItems; the port
# loads it as a bank, the same shape as every other .spr here.
romdisk/ifcommonitem.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 87 \
	    --out $(SELF)/$@

romdisk/ifdamage.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 164 \
	    --out $(SELF)/$@

romdisk/ifdigits.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 36 \
	    --out $(SELF)/$@

romdisk/iftags.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 38 \
	    --out $(SELF)/$@

romdisk/ifstatus.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 82 \
	    --out $(SELF)/$@

# The match clock's digits and its colon (src/dc/ifcommon.c). Fifteen
# sprites, all of them the clock's, so the whole file goes in.
romdisk/iftimer.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 165 \
	    --out $(SELF)/$@

# The pause menu's frame decals and its four player numbers
# (src/dc/ifcommon.c). Sixteen sprites, all of them the
# pause menu's -- file 197 holds nothing else -- so the whole file goes
# in.
romdisk/ifpause.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 197 \
	    --out $(SELF)/$@

# The menus' sprites. Out of the menus' common file (0), the sprites
# the mode select (src/dc/mnmodeselect.c), the VS mode
# (src/dc/mnvsmode.c), the character select (src/dc/mnplayersvs.c) and
# the VS options (src/dc/mnvsoptions.c) read: the collage, the option
# tab's three pieces, the paper decal, the logo, GAME MODE, the ten
# digits, infinity, the two arrows, the colon, ON/OFF/AUTO, the slash
# and the percent sign -- 26 of the file's 31; the other five are the
# frame, the fight background and the three JP words. Then the mode
# select's own file (1) whole, the VS mode's (6) less its eight JP
# subtitles, and the VS options' (7) less its five.
MENU_BANKS = romdisk/mncommon.spr romdisk/mnmain.spr romdisk/mn1p.spr \
             romdisk/mnvsmode.spr \
             romdisk/mnvsoptions.spr $(PLAYERS_BANKS) $(MAPS_BANKS) \
             $(RESULTS_BANKS) romdisk/mnmessage.spr romdisk/mnitemswitch.spr \
             romdisk/mnoption.spr romdisk/mnscreenadjust.spr \
             romdisk/mnbackupclear.spr romdisk/mnbackupclearheader.spr \
             romdisk/mndata.spr \
             romdisk/mnvsrecord.spr \
             romdisk/mndatacommon.spr romdisk/mncharacters.spr romdisk/mnsoundtest.spr \
             $(MNCONGRA_BANKS) romdisk/mnnocontroller.spr \
             romdisk/scexplaingraphics.spr romdisk/scstaffrollgraphics.spr \
             romdisk/characternames.spr $(SC1PINTRO_BANKS) \
             $(SC1PCHALLENGER_BANKS) $(SC1PSTAGECLEAR_BANKS) \
             $(MN1PCONTINUE_BANKS) \
             romdisk/mvopeningroomwallpaper.spr romdisk/mvopeningrun.spr romdisk/mvopeningcliff.spr romdisk/mvopeningyamabuki.spr romdisk/mvopeningyoster.spr romdisk/mvopeningsector.spr \
             romdisk/mvopeningsectorwallpaper.spr \
             romdisk/n64logo.spr \
             romdisk/mvopeningportraitsset1.spr romdisk/mvopeningportraitsset2.spr \
             romdisk/mvroomwipe.bin

# The camera-animation banks (src/dc/camanim.h): the
# AObjEvent32 scripts gcPlayCamAnim runs on a scene's cameras, cut out of
# relocData by name because the port has no linker symbol to offset from.
#
# mvopeningcommon.cam is the odd one: nothing loads it yet. It is the ONE
# file all eight per-fighter opening scenes take their camera out of
# (dMVOpening<F>FileIDs[1] for every one of them), so it is exported once
# here rather than eight times as those scenes land.
CAMANIM_BANKS = romdisk/mvopeningroom.cam romdisk/mvopeningcommon.cam \
                romdisk/mvending.cam romdisk/mvopeningrun.cam \
                romdisk/mvopeningcliff.cam romdisk/mvopeningyamabuki.cam romdisk/mvopeningjungle.cam romdisk/mvopeningyoster.cam romdisk/mvopeningsector.cam romdisk/mvopeningstandoff.cam romdisk/mvopeningclash.cam \
                romdisk/sc1pintro.cam romdisk/sc1pgameboss.cam

romdisk/sc1pintro.cam: $(CAMANIMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_camanimexport.py \
	    --rom baserom.z64 --bank SC1PIntro \
	    --out $(SELF)/$@

romdisk/mvopeningroom.cam: $(CAMANIMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_camanimexport.py \
	    --rom baserom.z64 --bank OpeningRoom \
	    --out $(SELF)/$@

romdisk/mvopeningcommon.cam: $(CAMANIMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_camanimexport.py \
	    --rom baserom.z64 --bank OpeningCommon \
	    --out $(SELF)/$@

romdisk/mvopeningclash.cam: $(CAMANIMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_camanimexport.py \
	    --rom baserom.z64 --bank OpeningClash \
	    --out $(SELF)/$@

romdisk/mvopeningstandoff.cam: $(CAMANIMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_camanimexport.py \
	    --rom baserom.z64 --bank OpeningStandoff \
	    --out $(SELF)/$@

romdisk/mvopeningsector.cam: $(CAMANIMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_camanimexport.py \
	    --rom baserom.z64 --bank OpeningSector \
	    --out $(SELF)/$@

romdisk/mvopeningyoster.cam: $(CAMANIMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_camanimexport.py \
	    --rom baserom.z64 --bank OpeningYoster \
	    --out $(SELF)/$@

romdisk/mvopeningjungle.cam: $(CAMANIMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_camanimexport.py \
	    --rom baserom.z64 --bank OpeningJungle \
	    --out $(SELF)/$@

romdisk/mvopeningyamabuki.cam: $(CAMANIMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_camanimexport.py \
	    --rom baserom.z64 --bank OpeningYamabuki \
	    --out $(SELF)/$@

romdisk/mvopeningcliff.cam: $(CAMANIMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_camanimexport.py \
	    --rom baserom.z64 --bank OpeningCliff \
	    --out $(SELF)/$@

romdisk/mvopeningrun.cam: $(CAMANIMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_camanimexport.py \
	    --rom baserom.z64 --bank OpeningRun \
	    --out $(SELF)/$@

romdisk/mvending.cam: $(CAMANIMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_camanimexport.py \
	    --rom baserom.z64 --bank Ending \
	    --out $(SELF)/$@

# The boss rung's approach and defeat cameras (src/dc/sc1pgame.c).
romdisk/sc1pgameboss.cam: $(CAMANIMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_camanimexport.py \
	    --rom baserom.z64 --bank Boss \
	    --out $(SELF)/$@

# The character select's six sprite files (src/dc/mnplayersvs.c): its
# own (17) whole, with the gate card drawn once in each of the eight
# palettes the scene points its LUT at; the emblems (20), the stone
# wall (21), the two game-mode labels (18) and the portraits (19). The
# seventh file, the spotlight model, has no bank (the scene says why).
PLAYERS_BANKS = romdisk/mnplayers.spr romdisk/mn1pselect.spr \
                romdisk/mn1pdiff.spr romdisk/ftstockszako.spr \
                romdisk/ftemblems.spr romdisk/mnselect.spr \
                romdisk/mngamemodes.spr romdisk/mnportraits.spr

romdisk/mnplayers.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 17 \
	    --luts 'RedCard=GateMan1P,GateMan2P,GateMan3P,GateMan4P,GateCom1P,GateCom2P,GateCom3P,GateCom4P' \
	    --out $(SELF)/$@

# relocData file 23 MNPlayers1PMode, the file the two 1P selects share
# and the VS select does not use. Training mode wanted the
# red card behind the CPU slot and the CP gate's palette; the 1P game's
# select wants eight more -- its title, the LEVEL: and
# STOCK: labels, the parentheses the difficulty name sits in, the
# option outline and its text, and the Smash logo. Both selects load
# the whole file in the game, so both get the whole bank here.
#
# The last six -- BestTime, TotalBestTime, Targets, Platforms, Sec and
# CSec -- are the bonus practice select's records panel and its TOTAL
# BEST TIME line, and came in with it. So the bank is
# now the whole file, which is what the game loads for either select.
#
# The human's card in all three 1P selects is this file's red card (82
# wide) in one of file 17's GateMan palettes, so the card is baked in
# those four as well; the palette is the texture, and file 17's own
# RedCard in the same palette is the 66-wide VS card, which is what the
# 1P selects drew until then -- the fighter, centred on the real card,
# stood right of the narrow one's centre.
romdisk/mn1pselect.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 23 \
	    --only RedCard,1PlayerGameText,ClosingParenthesis,OpeningParenthesis,LevelColonText,StockColonText,OptionOutline,SmashLogo,OptionText,BestTimeText,TotalBestTimeText,TargetsText,PlatformsText,Sec,CSec \
	    --luts 'RedCard=GateCP,17:GateMan1P,17:GateMan2P,17:GateMan3P,17:GateMan4P' \
	    --out $(SELF)/$@

# The 1P game select's two own files (src/dc/mnplayers1pgame.c):
# the five difficulty names the LEVEL slider scrolls through (24),
# and the one grey head the STOCK row draws once per life (25, whose
# single sprite the decomp's list has no name for).
romdisk/mn1pdiff.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 24 \
	    --out $(SELF)/$@

romdisk/ftstockszako.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 25 \
	    --out $(SELF)/$@

romdisk/ftemblems.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 20 \
	    --out $(SELF)/$@

romdisk/mnselect.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 21 \
	    --out $(SELF)/$@

romdisk/mngamemodes.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 18 \
	    --out $(SELF)/$@

# The stage select's own two files (src/dc/mnmaps.c): the icons, names,
# cursor, plaque and tiles (30), and the letters the JP subtitles are
# drawn from (33), which the US build loads and never draws.
MAPS_BANKS = romdisk/mnmaps.spr romdisk/mnfonts.spr

romdisk/mnmaps.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 30 \
	    --out $(SELF)/$@

romdisk/mnfonts.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 33 \
	    --out $(SELF)/$@

romdisk/mnportraits.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 19 \
	    --out $(SELF)/$@

# The VS results screen's two own files (src/dc/scvsresults.c): its own
# (34) less the one sprite the decomp's list has no name for and the
# screen never asks for, and the announcer's alphabet (37) whole, which
# is where the winner's name and WINS! are drawn from. The rest of what
# the screen reads is already here: the player tags (38), the two digit
# files (36 and 164) and the two game-mode labels (18).
RESULTS_BANKS = romdisk/mnresults.spr romdisk/ifannounce.spr

romdisk/mnresults.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 34 \
	    --only TKOText,PlaceText,KOsText,PtsText,1PArrow,2PArrow,3PArrow,4PArrow,Wallpaper,Winner \
	    --out $(SELF)/$@

romdisk/ifannounce.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 37 \
	    --out $(SELF)/$@

# The unlock message's own file (9, src/dc/mnmessage.c) whole: the seven
# messages and the "!" decal. The eighth, SOUND TEST, is the JP build's
# alone -- sc1PManagerCheckUnlockSoundTest queues it and the US build
# has no such call -- and is exported with the rest because the sprite
# table mnMessageMakeMessage indexes is seven long either way. The
# wallpaper behind them is the collage, already in the common bank.
romdisk/mnmessage.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 9 \
	    --out $(SELF)/$@

# The DATA menu's own file (5, src/dc/mndata.c): the three option
# labels, the word DATA and the dark DATA icon -- 5 of the file's 8. The
# other three are the Japanese descriptions under each tab, which the US
# build makes no GObj for (mnDataMakeMenuGObj is REGION_JP throughout).
# Everything else the screen draws -- the tab's three pieces, the paper
# decal, the logo and the collage -- is in the common bank already.
romdisk/mndata.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 5 \
	    --only CharactersText,VSRecordText,SoundTestText,DataText,DataIconDark \
	    --out $(SELF)/$@

# The Options screen's own file (4, src/dc/mnoption.c): the three tab
# labels (SOUND, SCREEN ADJUST, BACKUP CLEAR), the STEREO/MONO toggle
# text, OPTION itself and the dark settings icon -- seven of the file's
# sprites; the rest are the JP build's own subtitle text (mnoption.h's
# own note on mnOptionMakeMenuGObj). Everything else the screen draws is
# in the common bank already (the tab pieces, the slash, the paper decal,
# the logo and the collage).
romdisk/mnoption.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 4 \
	    --only StereoText,MonoText,SoundText,ScreenAdjustText,BackupClearText,OptionText,SettingsIconDark \
	    --out $(SELF)/$@

# SCREEN ADJUST's own file (15, src/dc/mnscreenadjust.c): its guide
# picture and its instruction text, the whole of the file -- both used,
# so this is a whole-file export, as mnvsrecord.spr and mncharacters.spr
# are.
romdisk/mnscreenadjust.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 15 \
	    --out $(SELF)/$@

# BACKUP CLEAR's own file (0x4d/77, src/dc/mnbackupclear.c): the header's
# yellow title text, the six tab labels, the Yes/No/circle confirm
# pieces and the two confirm-question texts -- 17 of the file's 25; the
# rest are the JP build's own subtitle frame offsets (mnbackupclear.h's
# own note on mnBackupClearMakeUnused) and one sprite (OptionSubjectMode)
# the decomp's own body never references on either region. Yes and No
# each carry a second and third palette the sprite's own does not (the
# highlight/not pair, plus Yes's own confirm-flash palette): --luts
# names them the same way the character-select gate card's own colours
# are (src/dc/mnplayersvs.c).
romdisk/mnbackupclear.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 77 \
	    --only HeaderBackupClear,OptionNewcomers,Option1PHighScore,OptionVSRecord,OptionBonusStageTime,OptionPrize,OptionAllDataClear,OptionCircle,IsOkayText,AreYouSureText,OptionYes,OptionNo \
	    --luts "OptionYes=OptionYesHighlight,OptionYesNot,OptionConfirm;OptionNo=OptionNoHighlight,OptionNoNot" \
	    --out $(SELF)/$@

# BACKUP CLEAR's other file (0x4e/78): one sprite, the "OPTION" panel
# icon behind the header's title text -- the whole file, so this is a
# whole-file export, as mnvsrecord.spr and mncharacters.spr are.
romdisk/mnbackupclearheader.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 78 \
	    --out $(SELF)/$@

# The Records tab's own file (0x1f/31, src/dc/mnvsrecord.c): the three
# tables' digits, labels and fighter icons. Every sprite in the file is
# used somewhere across the three tables (Battle Score, Ranking,
# Individual), so this is a whole-file export, as mnmaps.spr and
# mnfonts.spr are.
romdisk/mnvsrecord.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 31 \
	    --out $(SELF)/$@

# The DATA menu's shared file (0x20/32): the DATA header banner and the
# left/right arrows the Individual and Ranking tables scroll with. Three
# sprites, all used; mnvsrecord.c, mncharacters.c and mnsoundtest.c all
# open it now, the name the decomp's file's own.
romdisk/mndatacommon.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 32 \
	    --out $(SELF)/$@

# The Character Data tab's own file (16, src/dc/mncharacters.c): the
# label, the two name-tag frames, and per fighter -- name, story text,
# works-mode text and the three special-move input/name pairs -- plus
# the works wallpaper and the story wallpaper. Every sprite in the file
# is used, so this is a whole-file export, as mnvsrecord.spr is. The
# screen's other three files are all already placed: mndatacommon.spr
# above, ftemblems.spr (20, PLAYERS_BANKS) and the winner's-emblem
# models, which are baked .mdl packs (EMBLEMTOOL_DEPS below), not a
# sprite bank.
romdisk/mncharacters.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 16 \
	    --out $(SELF)/$@

# The Sound Test tab's own file (0xc4/196, src/dc/mnsoundtest.c): the
# Music/Sound/Voice column labels, the two capsule caps, A/START/B's
# three ":PLAY"/":FADE OUT"/":EXIT" labels, the SOUND TEST title text
# and the START button decal. mnsoundtest.c is the only file that opens
# it so far.
romdisk/mnsoundtest.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 196 \
	    --out $(SELF)/$@

# The congratulations screen's own 24 files (170-193, src/dc/mncongra.c):
# one whole-file export per fighter's bottom/top picture -- each file
# holds exactly one sprite (tools/export/ssb_spriteexport.py --file <n> --list
# against every one of them), so this is 24 whole-file exports, not one
# shared bank the way mnvsrecord.spr/mncharacters.spr are. The file
# numbers are not a formula (relocFileDescriptions.us.txt's own -170..
# -193 names, in dMNCongraPictures' own order -- Mario, Fox, Donkey,
# Samus, Luigi, Link, Yoshi, Captain, Kirby, Pikachu, Purin, Ness, the
# same order $(FIGHTERS) already walks), so each fighter's pair is a
# small lookup table rather than an arithmetic offset.
MNCONGRA_BOTTOM_FILE_mario   = 186
MNCONGRA_TOP_FILE_mario      = 187
MNCONGRA_BOTTOM_FILE_fox     = 190
MNCONGRA_TOP_FILE_fox        = 191
MNCONGRA_BOTTOM_FILE_donkey  = 184
MNCONGRA_TOP_FILE_donkey     = 185
MNCONGRA_BOTTOM_FILE_samus   = 176
MNCONGRA_TOP_FILE_samus      = 177
MNCONGRA_BOTTOM_FILE_luigi   = 188
MNCONGRA_TOP_FILE_luigi      = 189
MNCONGRA_BOTTOM_FILE_link    = 178
MNCONGRA_TOP_FILE_link       = 179
MNCONGRA_BOTTOM_FILE_yoshi   = 172
MNCONGRA_TOP_FILE_yoshi      = 173
MNCONGRA_BOTTOM_FILE_captain = 182
MNCONGRA_TOP_FILE_captain    = 183
MNCONGRA_BOTTOM_FILE_kirby   = 170
MNCONGRA_TOP_FILE_kirby      = 171
MNCONGRA_BOTTOM_FILE_pikachu = 174
MNCONGRA_TOP_FILE_pikachu    = 175
MNCONGRA_BOTTOM_FILE_purin   = 180
MNCONGRA_TOP_FILE_purin      = 181
MNCONGRA_BOTTOM_FILE_ness    = 192
MNCONGRA_TOP_FILE_ness       = 193

MNCONGRA_BANKS = $(foreach f,$(FIGHTERS),romdisk/mncongra$(f)bottom.spr \
                                          romdisk/mncongra$(f)top.spr)

define MNCONGRA_BANK_RULE
romdisk/mncongra$(1)bottom.spr: $$(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 \
	    --file $$(MNCONGRA_BOTTOM_FILE_$(1)) \
	    --out $(SELF)/$$@

romdisk/mncongra$(1)top.spr: $$(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 \
	    --file $$(MNCONGRA_TOP_FILE_$(1)) \
	    --out $(SELF)/$$@
endef
$(foreach f,$(FIGHTERS),$(eval $(call MNCONGRA_BANK_RULE,$(f))))

# No Controller's own file (169, src/dc/mnnocontroller.c): one sprite,
# the whole file -- a whole-file export, as mnvsrecord.spr and
# mncharacters.spr are.
romdisk/mnnocontroller.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 169 \
	    --out $(SELF)/$@

# The N64 logo (194, llN64LogoFileID; src/dc/mnstartup.c): one sprite,
# the whole of the boot screen's art. Named for the ROM file rather than
# for the scene, since the file is not an MNStartup one -- the same
# choice characternames.spr makes.
romdisk/n64logo.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 194 \
	    --out $(SELF)/$@

# The openings' second scene, opening portraits (src/dc/mvopeningportraits.c):
# two sets of four fighter portraits, one file each --
# 53 (Samus/Mario/Fox/Pikachu, plus the cover sprite) and 54
# (Link/Kirby/Donkey/Yoshi).
romdisk/mvopeningportraitsset1.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 53 \
	    --out $(SELF)/$@

romdisk/mvopeningportraitsset2.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 54 \
	    --out $(SELF)/$@

# How to Play's own file (198, src/dc/scexplain.c): the textbox
# illustration and the six phase-indicator icons (A/B/Z buttons,
# "HERE", two "+" symbols) -- all 26 of the file's Sprite entries; its
# other eleven entries (the control-stick diagram's MObjSub/DObjDesc/
# MatAnimJoint/DisplayList data) are not sprites and this tool skips
# them (--list confirms). Those eleven are tools/export/ssb_explainexport.py's
# three packs instead, below.
romdisk/scexplaingraphics.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 198 \
	    --out $(SELF)/$@

# The staff roll's own file (195, src/dc/scstaffroll.c): the alphabet,
# digits and punctuation for the side textbox, the crosshair and the
# two textbox brackets -- all 77 of the file's Sprite entries. The
# file's other entries (the scrolling name/job glyphs' own raw texture
# blocks, and the DObjDesc/AnimJoint/Interpolation "name plaque" and
# rotating-backdrop data) are not libultra Sprite records and this tool
# skips them (--list confirms). Those are tools/export/ssb_staffrollexport.py's
# two packs instead, below.
romdisk/scstaffrollgraphics.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 195 \
	    --out $(SELF)/$@

# The attract demo's twelve fighter name plates (file 12,
# src/dc/scautodemo.c scAutoDemoInitSObjs): one Sprite per playable
# fighter, in nFTKind order, and nothing else in the file.
# cut them for want of an export, on the reading that ssb-decomp-re
# carried no llCharacterNames* symbol to find them by; the tool does not
# need one -- it walks the file's own Sprite records and names them
# itself, and --file 12 --list prints exactly the twelve
# dSCAutoDemoFighterNameSpriteOffsets wants.
romdisk/characternames.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 12 \
	    --out $(SELF)/$@

# The 1P game's "<player> VS <opponent>" card (src/dc/sc1pintro.c) loads four
# files, and file 12 above is its second -- the card
# spells the opponent's name out of the same twelve plates the attract
# demo does, for the two rungs (Link and Pikachu) whose opponent is a
# plain fighter rather than a team. The other three are here.
#
# File 11 is the card itself: the sky, the two black banners, the VS
# decal, the rung number, the fourteen route markers, the nine
# opponent-name banners and the ally line -- 46 sprites, of which 44 are
# drawn. (The two that are not, Cross and the unnamed 0x5028, belong to
# no reader in this file or any other; they come along because the
# export is by file.) The same file also holds the 23 camera scripts
# romdisk/sc1pintro.cam cuts out of it, which this tool steps over.
#
# Files 13 and 14 are one bonus-stage picture each -- the target and the
# finish line in 13, the platform in 14 -- drawn on the three bonus
# rungs in place of the VS decal.
SC1PCHALLENGER_BANKS = romdisk/sc1pchallenger.spr

# The score screen's own three files (src/dc/sc1pstageclear.c):
# 80 its text and the fifty-seven bonus names, 81 the word SCORE, 151 the
# target and platform icons. Its other three files are the battle HUD's
# digit sets, already in HUD_BANKS above.
SC1PSTAGECLEAR_BANKS = romdisk/sc1pstageclear1.spr romdisk/sc1pstageclear2.spr \
                       romdisk/sc1pstageclear3.spr

romdisk/sc1pstageclear1.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 80 \
	    --out $(SELF)/$@

romdisk/sc1pstageclear2.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 81 \
	    --out $(SELF)/$@

romdisk/sc1pstageclear3.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 151 \
	    --out $(SELF)/$@

# The Continue prompt's own file (src/dc/mn1pcontinue.c):
# 79 is the spotlit room, its floor shadow and the beam over it, the
# cursor, and the three words CONTINUE, YES and NO. Its other four
# reloc files all ship already -- 81 the word SCORE (SC1PSTAGECLEAR_BANKS
# above), 37 the announcer's letters (ifannounce.spr), 164 the damage
# digits it counts the score on (HUD_BANKS) and 80 the score screen's
# text -- so this is the one new bank the scene costs.
MN1PCONTINUE_BANKS = romdisk/mn1pcontinue.spr

romdisk/mn1pcontinue.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 79 \
	    --out $(SELF)/$@

romdisk/sc1pchallenger.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 10 \
	    --out $(SELF)/$@

SC1PINTRO_BANKS = romdisk/sc1pintro.spr romdisk/bonuspicture.spr \
                  romdisk/bonuspictureplatform.spr

romdisk/sc1pintro.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 11 \
	    --out $(SELF)/$@

romdisk/bonuspicture.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 13 \
	    --out $(SELF)/$@

romdisk/bonuspictureplatform.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 14 \
	    --out $(SELF)/$@

# The openings' room wallpaper (90, src/dc/mvopeningroom.c): the file's
# one and only Sprite entry, llMVOpeningRoomWallpaperSprite at 0x26c88,
# drawn behind the wipe transition from tic 1040. It is the only one of
# this scene's eight relocData files that is a sprite bank rather than a
# DObjDesc/MObjSub/MatAnimJoint scene-graph tree, and so the only one
# the port loads (src/dc/mvopeningroom.h).
romdisk/mvopeningroomwallpaper.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 90 \
	    --out $(SELF)/$@

# The Room's tic-1040 star wipe: relocData 63's two flat display lists,
# the inner star and the red outer one, as bare triangles (tools/
# ssb_roomwipeexport.py; drawn by src/dc/mvopeningroom.c).
romdisk/mvroomwipe.bin: ../../../tools/export/ssb_roomwipeexport.py \
                        ../../../tools/lib/ssb_assets.py Makefile
	cd ../../.. && python3 tools/export/ssb_roomwipeexport.py \
	    --rom baserom.z64 \
	    --out $(SELF)/$@

# mvopeningrun.c's wallpaper: the one sprite of relocData 55, beside the
# eight fighter scripts mvopeningrun.cam carries.
romdisk/mvopeningrun.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 55 \
	    --out $(SELF)/$@

# mvopeningcliff.c's wallpaper: file 70's one sprite.
romdisk/mvopeningcliff.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 70 \
	    --out $(SELF)/$@

# mvopeningcliff.c's two models out of relocData 68: the hills (drawn
# with the Z-buffer off) and Link's ocarina with its AnimJoint.
SCENEMODELTOOL_DEPS = ../../../tools/export/ssb_scenemodelexport.py $(PACKTOOL_DEPS)
CLIFF_MODELS = romdisk/mvopeningcliffhills.mdl romdisk/mvopeningcliffocarina.mdl \
               romdisk/mvopeningyamabukilegs.mdl romdisk/mvopeningyamabukilegsshadow.mdl \
               romdisk/mvopeningyamabukimball.mdl \
               romdisk/mvopeningyosternest.mdl romdisk/mvopeningyosternestrest.mdl \
               romdisk/mvopeningyosterground.mdl \
               romdisk/mvopeningsectorgreatfox.mdl romdisk/mvopeningsectorarwing.mdl \
               romdisk/mvopeningstandoffground.mdl romdisk/mvopeningstandofflightning.mdl \
               romdisk/mvopeningclashwallll.mdl romdisk/mvopeningclashwalllr.mdl \
               romdisk/mvopeningclashwallul.mdl romdisk/mvopeningclashwallur.mdl \
               romdisk/mvopeningncpurin.mdl romdisk/mvopeningncpurinhide.mdl romdisk/mvopeningruncrash.mdl \
               romdisk/mvopeningnccaptain.mdl romdisk/mvopeningnccaptainhide.mdl romdisk/mvopeningncluigi.mdl romdisk/mvopeningncluigihide.mdl romdisk/mvopeningncness.mdl romdisk/mvopeningncnesshide.mdl

# The opening room's four plain props (52 MVCommon, src/dc/mvopeningroom.c):
# the desk, the books, the pencils and the lamp. Each is a DObjDesc tree of
# plain display lists, three of them with the AnimJoint table sitting right
# behind the tree that opens the books, rolls the pencils and turns the
# lamp's neck. The offsets are the decomp's own manifest
# (relocFileDescriptions.us.txt `DObjDesc RoomDesk` and friends), the same
# catalogue the exporters read.
ROOM_MODELS = romdisk/mvopeningroomdesk.mdl romdisk/mvopeningroombooks.mdl \
              romdisk/mvopeningroompencils.mdl romdisk/mvopeningroomlamp.mdl \
              romdisk/mvopeningroomsunlight.mdl \
              romdisk/mvopeningroomoutside.mdl romdisk/mvopeningroomhaze.mdl \
              romdisk/mvopeningroomtissues.mdl \
              romdisk/mvopeningroombossshadow.mdl \
              romdisk/mvopeningroomdeskground.mdl \
              romdisk/mvopeningroomlogo.mdl \
              romdisk/mvopeningroombackground0.mdl \
              romdisk/mvopeningroombackground1.mdl \
              romdisk/mvopeningroomsnap.mdl \
              romdisk/mvopeningroomcloseupair.mdl \
              romdisk/mvopeningroomcloseupground.mdl \
              romdisk/mvopeningroomspotlight.mdl

romdisk/mvopeningroomdesk.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dobjdesc 0x8df8 --name RoomDesk --out $(SELF)/$@

romdisk/mvopeningroombooks.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dobjdesc 0xa6f8 --animjoint 0xa7b0 --name RoomBook \
	    --out $(SELF)/$@

romdisk/mvopeningroompencils.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dobjdesc 0xaeb8 --animjoint 0xaf70 --name RoomPenc \
	    --out $(SELF)/$@

romdisk/mvopeningroomlamp.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dobjdesc 0xbdc0 --animjoint 0xbea0 --name RoomLamp \
	    --out $(SELF)/$@

# The same scene's five LONE display lists (`DisplayList Room*` in the
# same manifest): no DObjDesc array at
# all, one block of commands bound straight to one DObj by
# gcAddDObjForGObj, which --dl reads. Three of them are DObjDLLink
# arrays rather than commands (--dllinks, the gcDrawDObjDLLinksForGObj
# makers), the tissue box and Master Hand's shadow carry one AnimJoint
# SCRIPT each rather than a per-joint table (--animdl), and the shadow
# alone is queued into DL head 1 by gcDrawDObjDLHead1 (--dlhead 1),
# which is what puts it in the translucent list -- its own display list
# sets no render mode, so nothing else would have said.
romdisk/mvopeningroomsunlight.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dl 0x24708 --dllinks --name RoomSun --out $(SELF)/$@

romdisk/mvopeningroomoutside.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dl 0x24200 --dllinks --name RoomOut --out $(SELF)/$@

romdisk/mvopeningroomhaze.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dl 0x098f8 --dllinks --name RoomHaze --out $(SELF)/$@

romdisk/mvopeningroomtissues.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dl 0x0c690 --animdl 0x0c884 --name RoomTiss --out $(SELF)/$@

romdisk/mvopeningroombossshadow.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dl 0x1f790 --dlhead 1 --animdl 0x1f924 --name RoomShad \
	    --out $(SELF)/$@

# And the two that carry MATERIALS (`MObjSub RoomDeskGround` /
# `MatAnimJoint RoomDeskGround` and the same pair for RoomLogo in that
# manifest): their makers call gcAddMObjAll and gcAddMatAnimJointAll
# beside the tree, so --mobjsub and --matanim bake the FPackMObjs block
# tools/export/ssb_emblemexport.py first wrote and src/dc/objmodel.c already
# hands back to those two functions. Each script is a 60-tic prim-colour
# fade -- the ground fades in, the logo's own runs at once.
romdisk/mvopeningroomdeskground.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dobjdesc 0x22440 --dllinks --mobjsub 0x20480 \
	    --matanim 0x225cc --name RoomDGnd --out $(SELF)/$@

romdisk/mvopeningroomlogo.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dobjdesc 0x1c4a8 --dllinks --mobjsub 0x1bc60 \
	    --matanim 0x1c52c --name RoomLogo --out $(SELF)/$@

# The room itself, in two packs off one DObjDesc array: 51 joints
# against FIGHTER_MAX_JOINTS's 40, so --entries splits it the way
# mvopeningyoster.c's nest is split. Entry 0 is the array's one root, at
# the identity with no display list, and goes in both halves so each
# resolves its own parent-by-id chain. All seven MObjSubs sit on the
# second half, which is why only it names --mobjsub/--matanim -- and the
# exporter is what carries their per-joint indices across the
# renumbering (read_matanim's `rows`).
romdisk/mvopeningroombackground0.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dobjdesc 0x07e98 --dllinks --entries 0-17 \
	    --name RoomBg0 --out $(SELF)/$@

romdisk/mvopeningroombackground1.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dobjdesc 0x07e98 --dllinks --entries 0,18-50 \
	    --mobjsub 0x042f8 --matanim 0x08788 \
	    --name RoomBg1 --out $(SELF)/$@

# The room's last four, which took no new exporter flag: they are the
# three shapes above composed. The snap is a tree with an AnimJoint;
# the close-up effect's two halves are the first models here to carry
# an AnimJoint AND a MatAnimJoint at once, which is just both sets of
# flags on one line; the spotlight is a lone display list WITH
# materials.
romdisk/mvopeningroomsnap.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dobjdesc 0x1ca68 --dllinks --animjoint 0x1caf0 \
	    --name RoomSnap --out $(SELF)/$@

romdisk/mvopeningroomcloseupair.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dobjdesc 0x1df28 --dllinks --animjoint 0x1e010 \
	    --mobjsub 0x1dca0 --matanim 0x1dfd8 \
	    --name RoomCUA --out $(SELF)/$@

romdisk/mvopeningroomcloseupground.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dobjdesc 0x1f270 --dllinks --animjoint 0x1f330 \
	    --mobjsub 0x1f0f8 --matanim 0x1f2f4 \
	    --name RoomCUG --out $(SELF)/$@

romdisk/mvopeningroomspotlight.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 52 \
	    --dl 0x22e18 --dlhead 1 --mobjsub 0x22c90 --matanim 0x22f10 \
	    --name RoomSpot --out $(SELF)/$@

romdisk/mvopeningcliffhills.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 68 \
	    --dobjdesc 0x37a0 --name CliffHil --noz --out $(SELF)/$@

romdisk/mvopeningcliffocarina.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 68 \
	    --dobjdesc 0x67a0 --animjoint 0x6850 --name CliffOca \
	    --out $(SELF)/$@

# mvopeningyamabuki.c's wallpaper: file 71's one sprite.
romdisk/mvopeningyamabuki.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 71 \
	    --out $(SELF)/$@

# ... and its three models: the legs, their shadow and the Poke Ball.
romdisk/mvopeningyamabukilegs.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 71 \
	    --dobjdesc 0x9548 --animjoint 0x98c0 --name YamLegs --out $(SELF)/$@

romdisk/mvopeningyamabukilegsshadow.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 71 \
	    --dobjdesc 0xb2b0 --animjoint 0xb390 --dllinks --name YamShad \
	    --out $(SELF)/$@

romdisk/mvopeningyamabukimball.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 71 \
	    --dobjdesc 0xc9e0 --animjoint 0xcac0 --dllinks --name YamBall \
	    --out $(SELF)/$@

# mvopeningyoster.c's wallpaper: the one sprite of file 93 (StageYoshi),
# and its nest and ground out of file 67.
romdisk/mvopeningyoster.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 93 \
	    --out $(SELF)/$@

romdisk/mvopeningyosternest.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 67 \
	    --dobjdesc 0x9808 --dllinks --entries 0-38 --name YosNest \
	    --out $(SELF)/$@

romdisk/mvopeningyosternestrest.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 67 \
	    --dobjdesc 0x9808 --dllinks --entries 0,4,39-72 --name YosNstB \
	    --out $(SELF)/$@

romdisk/mvopeningyosterground.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 67 \
	    --dobjdesc 0xb990 --animjoint 0xbf70 --dllinks --name YosGrnd \
	    --out $(SELF)/$@

# mvopeningsector.c's two sprites (file 73's cockpit, file 74's
# wallpaper) and its two models: the Great Fox out of file 73, and the
# Arwing out of file 161 flown on file 73's three tables.
romdisk/mvopeningsector.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 73 \
	    --out $(SELF)/$@

romdisk/mvopeningsectorwallpaper.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 74 \
	    --out $(SELF)/$@

romdisk/mvopeningsectorgreatfox.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 73 \
	    --dobjdesc 0xd820 --animjoint 0xda10 --dllinks --name SecFox \
	    --out $(SELF)/$@

romdisk/mvopeningsectorarwing.mdl: $(SCENEMODELTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_scenemodelexport.py \
	    --rom baserom.z64 --file 161 \
	    --dobjdesc 0x2c30 --dllinks --anim 0xe110@73 --anim 0xe910@73 \
	    --anim 0xf1c0@73 --name SecArw --out $(SELF)/$@

# mvopeningstandoff.c's two models out of relocData 69,
# through ssb_effectexport.py's rows for them: a single display list on
# one DObj, and a DObjDLLink tree with MObjs and both animation kinds.
romdisk/mvopeningstandoffground.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py \
	    --rom baserom.z64 \
	    --what standoffground --out $(SELF)/$@

romdisk/mvopeningstandofflightning.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py \
	    --rom baserom.z64 \
	    --what standofflight --out $(SELF)/$@

# mvopeningclash.c's four wallpaper quadrants out of relocData 66: one display
# list, one MObj, both animation kinds each.
romdisk/mvopeningclashwall%.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py \
	    --rom baserom.z64 \
	    --what clashwall$* --out $(SELF)/$@

# mvopeningnewcomers.c's eight silhouettes out of relocData 61 and 62: a Show
# and a Hidden display list per fighter, one DObj and one
# AnimJoint script each.
# mvopeningrun.c's crash: relocData 75's five-joint tree with
# four MObjs and a MatAnimJoint.
romdisk/mvopeningruncrash.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py \
	    --rom baserom.z64 \
	    --what runcrash --out $(SELF)/$@

romdisk/mvopeningnc%.mdl: $(EFFECTTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_effectexport.py \
	    --rom baserom.z64 \
	    --what newcomers$* --out $(SELF)/$@

# The item switch's own file (8, src/dc/mnvsitemswitch.c): the two
# labels, the six appearance rates, the ON/OFF toggle's three pieces,
# the button decal, the item list and the cursor -- 14 of the file's 37.
# The other 23 are the JP subtitle's, which the US build never draws
# (mnItemSwitchMakeSubtitle's whole body is under REGION_JP), and the
# frame it sits in.
romdisk/mnitemswitch.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 8 \
	    --only LabelVSOptions,LabelItemSwitch,AppearanceNone,AppearanceVeryLow,AppearanceLow,AppearanceMiddle,AppearanceHigh,AppearanceVeryHigh,ToggleOn,ToggleOff,ToggleSlash,DecalButton,ItemList,Cursor \
	    --out $(SELF)/$@

# (the two --only lists live here, so the Makefile is a dependency)
romdisk/mncommon.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 0 \
	    --only SmashBrosCollage,OptionTabLeft,OptionTabMiddle,OptionTabRight,DecalPaper,SmashLogo,GameModeText,Digit0,Digit1,Digit2,Digit3,Digit4,Digit5,Digit6,Digit7,Digit8,Digit9,Infinity,ArrowR,ArrowL,Colon,OnText,OffText,AutoText,Slash,Percentage \
	    --out $(SELF)/$@

romdisk/mnmain.spr: $(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 1 \
	    --out $(SELF)/$@

# The 1P submenu's own file (2), less its four JP subtitles: the US
# build has them and never draws them (mn1PModeMakeSubtitle's whole body
# is the REGION_JP arm).
romdisk/mn1p.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 2 \
	    --only OptionTab,1PGameText,ControllerIconDark,1PText,TrainingModeText,Bonus1PracticeText,Bonus2PracticeText \
	    --out $(SELF)/$@

romdisk/mnvsmode.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 6 \
	    --only VSStartText,RulePeriodText,TimeText,StockText,TeamText,TimePeriodText,MinText,StockPeriodText,VSOptionsText,ConsoleIconDark,VSText \
	    --out $(SELF)/$@

# The VS options screen's own file (7, src/dc/mnvsoptions.c) less its
# five JP subtitles: the label, the option bubble, the four option
# names, the item switch's, and the dark console icon.
romdisk/mnvsoptions.spr: $(SPRITETOOL_DEPS) Makefile
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 --file 7 \
	    --only VSOptionsText,Bubble,HandicapText,TeamAttackText,StageSelectText,ItemSwitchText,DamageText,ConsoleIconDark \
	    --out $(SELF)/$@

# Each fighter's stock icon (once per costume palette) and emblem, out
# of the file his FTSprites names -- the model file, or DK's icon file
# (src/dc/ftmanager.c ftManagerSetupFilesAllKind).
define FIGHTER_BANK_RULE
romdisk/ft$(1).spr: $$(SPRITETOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_spriteexport.py \
	    --rom baserom.z64 \
	    --fighter $(call fighter-name,$(1)) \
	    --out $(SELF)/$$@
endef
$(foreach f,$(FIGHTERS),$(eval $(call FIGHTER_BANK_RULE,$(f))))

# The FGM engine's data (tools/export/ssb_fgmexport.py): the three bytecode
# files verbatim and every B1_sounds2 sample the VS game can reach, as AICA
# ADPCM -- 264 of the 322, about 1.97 MB. That is more than sound RAM
# holds beside the music, so the romdisk carries them all and
# src/dc/sndres.c uploads the running scene's share, by the sets
# tools/export/ssb_sndsets.py measures out of the decomp: which files name which
# FGM ids, which samples those ids' voice scripts trigger, and which music
# waves each scene's tracks select. The same tool fails the build if any
# scene's worst case (four fighters, one stage) does not fit.
#
# Until the audio step this staged Mario's moveset and a 37-name hand
# list of cues -- 34 samples -- so every other fighter, most items and
# the announcer's names were silent.
SNDSET_DEPS = ../../../tools/export/ssb_sndsets.py ../../../tools/export/ssb_fgmexport.py \
              ../../../tools/export/ssb_bgmpack.py ../../../tools/export/ssb_bgmexport.py \
              ../../../tools/export/ssb_stageexport.py ../../../tools/lib/ssb_sfxexport.py \
              Makefile
# (not every src/dc/*.c, though the census reads the scene files' music
# and checks the port names no FGM id it has not placed: that would rerun
# it on every edit, and ./run.sh test sndsets checks the tree as it stands)
FGMTOOL_DEPS = ../../../tools/export/ssb_fgmexport.py ../../../tools/lib/ssb_sfxexport.py \
               ../../../tools/lib/ssb_adpcm.py ../../../tools/export/ssb_sndsets.py \
               Makefile $(SSB_DECOMP_DIR)/tools/audio_codec.py

romdisk/fgm.ucd: $(FGMTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_fgmexport.py \
	    --rom baserom.z64 \
	    --outdir $(SELF)/romdisk

romdisk/sndsets.bin: $(SNDSET_DEPS)
	cd ../../.. && python3 tools/export/ssb_sndsets.py \
	    --rom baserom.z64 \
	    --out $(SELF)/$@

# Everything the game reads at runtime, wherever it ends up: the romdisk
# image is made of these, and so is the disc's data track.
ASSETS = $(FIGHTER_PACKS) $(FIGHTER_ANMS) $(STAGES) $(BGM_BANK) \
         romdisk/fgm.ucd romdisk/sndsets.bin romdisk/mntitle.spr $(TITLE_ASSETS) $(HUD_BANKS) $(MENU_BANKS) \
         $(ITEM_MODELS) \
         $(FIGHTER_BANKS) $(TRANSITIONS) $(EMBLEMS) $(CAMANIM_BANKS) \
         romdisk/spotlite.mdl romdisk/ifarrows.mdl romdisk/ifmagnify.mdl \
         romdisk/ftshadow.mdl romdisk/vmuart.bin romdisk/dcfont.spr \
         $(EXPLAIN_MODELS) $(STAFFROLL_MODELS) $(CLIFF_MODELS) $(ROOM_MODELS) \
         romdisk/efhalo.mdl romdisk/efmballrays.mdl romdisk/efswirl.mdl \
         romdisk/efquake.mdl romdisk/eforbs.mdl \
         romdisk/efspark.mdl romdisk/efmdust.mdl romdisk/efdexp.mdl \
         romdisk/efshield.mdl romdisk/efyegg.mdl romdisk/efshock.mdl \
         romdisk/effirespark.mdl \
         romdisk/efimpactwave.mdl romdisk/wpmariofireball.mdl \
         romdisk/wpfoxblaster.mdl romdisk/wparwinglaser.mdl \
         romdisk/wpsamuschargeshot.mdl romdisk/wppikachujolt.mdl \
         romdisk/wpyoshistar.mdl romdisk/wpyoshieggthrow.mdl romdisk/wplgunammo.mdl \
         romdisk/wpsamusbomb.mdl \
         romdisk/wplinkboomerang.mdl \
         romdisk/wpkirbycutter.mdl romdisk/wpbossbullet.mdl \
         romdisk/wppikachugjolt.mdl \
         romdisk/wppikachuthunder.mdl \
         romdisk/wpnesspkfire.mdl romdisk/wpnesspkthunder.mdl \
         romdisk/wpnesspktrail.mdl \
         romdisk/wplinkspin.mdl romdisk/efspin.mdl \
         romdisk/efvulcan.mdl \
         romdisk/efpsimagnet.mdl romdisk/efpkwave.mdl romdisk/efpktrail.mdl \
         romdisk/efreflectbreak.mdl romdisk/efcatchswirl.mdl romdisk/efgrapplebeam.mdl romdisk/efreflector.mdl romdisk/effalconkick.mdl romdisk/effalconpunch.mdl romdisk/efsing.mdl romdisk/efmballthrown.mdl romdisk/efjoltspark.mdl romdisk/efthundertrail.mdl romdisk/efkirbystar.mdl romdisk/efthundershock.mdl \
         romdisk/efdokan.mdl \
         romdisk/eftaru.mdl romdisk/efpoint.mdl \
         romdisk/efstar.mdl \
         romdisk/efwave.mdl romdisk/efbeam.mdl \
         romdisk/efentryegg.mdl romdisk/efarwing.mdl romdisk/efcar.mdl romdisk/efegglay.mdl \
         romdisk/efcutup.mdl romdisk/efcutdown.mdl \
         romdisk/efcutdraw.mdl romdisk/efcuttrail.mdl \
         $(PARTICLE_BANKS)

# Assets that belong on the disc and never in the ELF's romdisk, because
# the ELF is 14.6 MB of a 16 MB machine and what is in it is heap the
# scenes do not get. The damage slash's 44 KB is the
# one left: the sparks' and the metal dust's 15 KB each are in the
# romdisk, because the effect manager does not reserve
# a Fighter per model in .bss.
#
# Note that nothing stages this directory onto the disc image either --
# scripts/make_cdi.sh copies romdisk/ and only romdisk/ -- so disc/ is where a
# pack is built and checked without being shipped.
DISC_ONLY = disc/efslash.mdl disc/models.bnd

# The three 1P team cards' baked crowds (tools/export/ssb_introcrowd.py,
# src/dc/introcrowd.h): the port's own draw path run on the host over
# a recording PVR (bake_host, `make hosttest BAKER=1`), drawn by
# tools/lib/pvrsoft.py and packed as VQ tiles, with no console needed. The card draws its
# models when a file is missing, so SSB_NO_INTROCROWD=1 skips them.
INTROCROWD = disc/introcrowd_yoshi.bin disc/introcrowd_kirby.bin disc/introcrowd_poly.bin
ifndef SSB_NO_INTROCROWD
DISC_ONLY += $(INTROCROWD)
endif

# hosttest is a phony target that rebuilds everything, so what says bake_host
# is stale is the sources it is made of
bake_host: $(wildcard ../../dc/*.c ../../dc/*.h hoststubs/*.[ch] hosttest/*.c) \
           hosttest_ft.c bakehost.c Makefile
	$(MAKE) hosttest BAKER=1 HOST_OUT=bake_host HOST_RUN=true

$(INTROCROWD) &: bake_host ../../../tools/export/ssb_introcrowd.py \
                 ../../../tools/lib/pvrsoft.py $(FIGHTER_PACKS) $(FIGHTER_ANMS) \
                 romdisk/sc1pintro.cam
	@mkdir -p $(SELF)/disc
	cd ../../.. && python3 tools/export/ssb_introcrowd.py \
	    --bake-host $(SELF)/bake_host --rom-dir $(SELF) $(SELF)/disc

# The disc's models.bnd (tools/export/ssb_bundle.py, src/dc/assetroot.h): the
# 124 effect, weapon and item models the battle's preloads read back to
# back, as one file served by name from inside asset_open. The names and
# their order are tools/export/disc_layout.py's BUNDLE_MODELS; the damage slash
# is the one of them built into disc/ rather than romdisk/ (above).
# scripts/make_cdi.sh stages the bundle and drops the loose copies; the
# romdisk keeps them. NOT YET TESTED ON REAL HARDWARE (2026-09-22).
BUNDLE_NAMES := $(shell cd ../../.. && python3 tools/export/ssb_bundle.py --names)
BUNDLE_SRCS := $(foreach n,$(BUNDLE_NAMES),$(if $(filter efslash.mdl,$(n)),disc/$(n),romdisk/$(n)))

disc/models.bnd: $(BUNDLE_SRCS) ../../../tools/export/ssb_bundle.py \
                 ../../../tools/export/disc_layout.py
	@mkdir -p $(SELF)/disc
	cd ../../.. && python3 tools/export/ssb_bundle.py --out $(SELF)/disc/models.bnd \
	    --src $(SELF)/romdisk --src $(SELF)/disc

romdisk.img: $(ASSETS)

# With no romdisk in the ELF nothing else asks for them, and a disc build
# with an empty data track would boot to a black screen. DCLOAD=1 has the
# same hole -- no romdisk.o either -- but wants $(ASSETS) alone: /pc
# serves romdisk/ directly (DB_PC_ROOT), never disc/'s bundle-specific
# tree, so $(DISC_ONLY) stays disc-only.
ifeq ($(DISC),1)
all: $(ASSETS) $(DISC_ONLY)
endif
ifeq ($(DCLOAD),1)
all: $(ASSETS)
endif


DC_HDRS = ../../dc/fighter.h ../../dc/input.h \
          ../../dc/mtx.h ../../dc/ifcommon.h ../../dc/gmcamera.h \
          ../../dc/lbcommon.h ../../dc/scmanager.h ../../dc/scvsbattle.h \
          ../../dc/scvsresults.h ../../dc/scautodemo.h ../../dc/scexplain.h ../../dc/mvending.h ../../dc/scstaffroll.h ../../dc/mvopeningroom.h ../../dc/mvopeningportraits.h ../../dc/mvopeningmario.h ../../dc/mvopeningdonkey.h ../../dc/mvopeningsamus.h ../../dc/mvopeninglink.h ../../dc/mvopeningyoshi.h ../../dc/mvopeningkirby.h ../../dc/mvopeningfox.h ../../dc/mvopeningpikachu.h ../../dc/mvopeningrun.h ../../dc/mvopeningcliff.h ../../dc/mvopeningyamabuki.h ../../dc/mvopeningjungle.h ../../dc/mvopeningyoster.h ../../dc/mvopeningsector.h ../../dc/mvopeningstandoff.h ../../dc/mvopeningclash.h ../../dc/mvopeningnewcomers.h ../../dc/mntitle.h ../../dc/sprite.h ../../dc/camanim.h \
          ../../dc/mnmodeselect.h ../../dc/mn1pmode.h ../../dc/mnvsmode.h ../../dc/mnplayersvs.h \
          ../../dc/mnplayers1ptraining.h \
          ../../dc/sc1ptrainingmode.h \
          ../../dc/mnplayers1pgame.h \
          ../../dc/mnplayers1pbonus.h \
          ../../dc/sc1pchallenger.h ../../dc/sc1pintro.h ../../dc/sc1pgame.h \
          ../../dc/sc1pbonusstage.h \
          ../../dc/sc1pstageclear.h ../../dc/mn1pcontinue.h \
          ../../dc/mnmaps.h ../../dc/mndata.h ../../dc/mnvsrecord.h \
          ../../dc/mncharacters.h \
          ../../dc/mnsoundtest.h \
          ../../dc/mnoption.h \
          ../../dc/mnscreenadjust.h \
          ../../dc/mnbackupclear.h \
          ../../dc/mncongra.h \
          ../../dc/mnnocontroller.h ../../dc/dcmemcard.h ../../dc/scport.h \
          ../../dc/mnstartup.h \
          ../../dc/gmcommon.h \
          ../../dc/stage.h ../../dc/ftcommon.h ../../dc/mpcommon.h \
          ../../dc/bgm.h ../../dc/fgm.h ../../dc/taskman.h \
          ../../dc/efmanager.h \
          ../../dc/objmodel.h ../../dc/objpvr.h ../../dc/dcpvr.h \
          ../../dc/ftdisplaymain.h ../../dc/clip.h \
          ../../dc/vmusave.h ../../dc/vmucard.h \
          ../../dc/dctext.h ../../dc/dcstrings.h ../../dc/vmunotice.h \
          ../../dc/overlay.h
# reloc_data.us.h is generated, not written: the decomp's own
# tools/genRelocSymbols.py emits an ll* declaration for every ROM
# file and every block inside one (5,048 at the current pin; the count is
# tools/check/reloc_check.py's to print, not this comment's to keep true).
# tools/export/gen_reloc_header.sh has the argument for why the
# port needs them and what they do not do. It is listed first so a clean
# tree builds it before anything includes <reloc_data.h>, and the wildcard
# picks it up afterwards for the dependency edge.
#
# FORCE, not a prerequisite list: the generator is a fifth of a second and
# writes only when the bytes differ, so running it every build costs
# nothing, rebuilds nothing, and repairs a hand-edited or stale header
# instead of believing its mtime. tools/check/reloc_check.py is the backstop.
RELOC_HDR = ../../dc/decomp/reloc_data.us.h
$(RELOC_HDR): FORCE
	@../../../tools/export/gen_reloc_header.sh $@
.PHONY: FORCE
FORCE:

SHIM_HDRS = $(RELOC_HDR) $(wildcard ../../dc/decomp/*.h ../../dc/decomp/*/*.h)

main.o: main.c ../../dc/db.h ../../dc/sndres.h ../../dc/bgm.h \
        ../../dc/stackguard.h ../../dc/textguard.h $(DC_HDRS) $(SHIM_HDRS)
# The port's debug facility (src/dc/db.h), the game's own src/db/ overlay's
# place in this build. A build without it drops this object and passes
# -DDB_ENABLE=0; nothing else in the link names it.
db.o: ../../dc/db.c ../../dc/db.h ../../dc/stackguard.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/db.c -o db.o
lbtransition.o: ../../dc/lbtransition.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/lbtransition.c -o lbtransition.o
ftcommon.o: ../../dc/ftcommon.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ftcommon.c -o ftcommon.o
ftmanager.o: ../../dc/ftmanager.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ftmanager.c -o ftmanager.o
ftcomputer.o: ../../dc/ftcomputer.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c ../../dc/ftcomputer.c -o ftcomputer.o

wpmanager.o: ../../dc/wpmanager.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/wpmanager.c -o wpmanager.o

wpattrs.o: ../../dc/wpattrs.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/wpattrs.c -o wpattrs.o

itmanager.o: ../../dc/itmanager.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itmanager.c -o itmanager.o

itempack.o: ../../dc/itempack.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itempack.c -o itempack.o

itemmodel.o: ../../dc/itemmodel.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itemmodel.c -o itemmodel.o

itmain.o: ../../dc/itmain.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itmain.c -o itmain.o

itprocess.o: ../../dc/itprocess.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itprocess.c -o itprocess.o

itvisuals.o: ../../dc/itvisuals.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itvisuals.c -o itvisuals.o

itdisplay.o: ../../dc/itdisplay.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itdisplay.c -o itdisplay.o

itmap.o: ../../dc/itmap.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itmap.c -o itmap.o

itbox.o: ../../dc/itbox.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itbox.c -o itbox.o

ittaru.o: ../../dc/ittaru.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ittaru.c -o ittaru.o

itcapsule.o: ../../dc/itcapsule.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itcapsule.c -o itcapsule.o

itegg.o: ../../dc/itegg.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itegg.c -o itegg.o

itmball.o: ../../dc/itmball.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itmball.c -o itmball.o

itmew.o: ../../dc/itmew.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itmew.c -o itmew.o

ittosakinto.o: ../../dc/ittosakinto.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ittosakinto.c -o ittosakinto.o

itsawamura.o: ../../dc/itsawamura.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itsawamura.c -o itsawamura.o

itkabigon.o: ../../dc/itkabigon.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itkabigon.c -o itkabigon.o

itmlucky.o: ../../dc/itmlucky.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itmlucky.c -o itmlucky.o

itnyars.o: ../../dc/itnyars.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itnyars.c -o itnyars.o

itdogas.o: ../../dc/itdogas.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itdogas.c -o itdogas.o

itiwark.o: ../../dc/itiwark.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itiwark.c -o itiwark.o

itstarmie.o: ../../dc/itstarmie.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itstarmie.c -o itstarmie.o

itlizardon.o: ../../dc/itlizardon.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itlizardon.c -o itlizardon.o

itspear.o: ../../dc/itspear.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itspear.c -o itspear.o

itkamex.o: ../../dc/itkamex.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itkamex.c -o itkamex.o

itpippi.o: ../../dc/itpippi.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itpippi.c -o itpippi.o

itstar.o: ../../dc/itstar.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itstar.c -o itstar.o

ittomato.o: ../../dc/ittomato.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ittomato.c -o ittomato.o

itheart.o: ../../dc/itheart.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itheart.c -o itheart.o

itsword.o: ../../dc/itsword.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itsword.c -o itsword.o

itbat.o: ../../dc/itbat.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itbat.c -o itbat.o

ithammer.o: ../../dc/ithammer.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ithammer.c -o ithammer.o

itharisen.o: ../../dc/itharisen.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itharisen.c -o itharisen.o

itfflower.o: ../../dc/itfflower.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itfflower.c -o itfflower.o

itlgun.o: ../../dc/itlgun.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itlgun.c -o itlgun.o

itstarrod.o: ../../dc/itstarrod.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itstarrod.c -o itstarrod.o

itgshell.o: ../../dc/itgshell.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itgshell.c -o itgshell.o

itrshell.o: ../../dc/itrshell.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itrshell.c -o itrshell.o

itnbumper.o: ../../dc/itnbumper.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itnbumper.c -o itnbumper.o

itgbumper.o: ../../dc/itgbumper.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itgbumper.c -o itgbumper.o

itpowerblock.o: ../../dc/itpowerblock.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itpowerblock.c -o itpowerblock.o

itpakkun.o: ../../dc/itpakkun.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itpakkun.c -o itpakkun.o

itglucky.o: ../../dc/itglucky.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itglucky.c -o itglucky.o

itporygon.o: ../../dc/itporygon.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itporygon.c -o itporygon.o

itmarumine.o: ../../dc/itmarumine.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itmarumine.c -o itmarumine.o

ittarget.o: ../../dc/ittarget.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ittarget.c -o ittarget.o
ittarubomb.o: ../../dc/ittarubomb.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ittarubomb.c -o ittarubomb.o

ithitokage.o: ../../dc/ithitokage.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ithitokage.c -o ithitokage.o

itfushigibana.o: ../../dc/itfushigibana.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itfushigibana.c -o itfushigibana.o

itmsbomb.o: ../../dc/itmsbomb.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itmsbomb.c -o itmsbomb.o

itbombhei.o: ../../dc/itbombhei.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/itbombhei.c -o itbombhei.o

gmrumble.o: ../../dc/gmrumble.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/gmrumble.c -o gmrumble.o

ftcommontwister.o: ../../dc/ftcommontwister.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ftcommontwister.c -o ftcommontwister.o

grhyrule.o: ../../dc/grhyrule.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/grhyrule.c -o grhyrule.o

grjungle.o: ../../dc/grjungle.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/grjungle.c -o grjungle.o

grzebes.o: ../../dc/grzebes.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/grzebes.c -o grzebes.o

gryoster.o: ../../dc/gryoster.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/gryoster.c -o gryoster.o

grpupupu.o: ../../dc/grpupupu.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/grpupupu.c -o grpupupu.o

grinishie.o: ../../dc/grinishie.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/grinishie.c -o grinishie.o

grcastle.o: ../../dc/grcastle.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/grcastle.c -o grcastle.o

gryamabuki.o: ../../dc/gryamabuki.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/gryamabuki.c -o gryamabuki.o

grsector.o: ../../dc/grsector.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/grsector.c -o grsector.o

grbonus3.o: ../../dc/grbonus3.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/grbonus3.c -o grbonus3.o

ftcommontaru.o: ../../dc/ftcommontaru.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ftcommontaru.c -o ftcommontaru.o

wpmain.o: ../../dc/wpmain.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/wpmain.c -o wpmain.o

wpprocess.o: ../../dc/wpprocess.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/wpprocess.c -o wpprocess.o

wpdisplay.o: ../../dc/wpdisplay.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/wpdisplay.c -o wpdisplay.o
wpmap.o: ../../dc/wpmap.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/wpmap.c -o wpmap.o
# the motion-command script engine (a verbatim copy of ft/ftmain.c's) and
# the ft/ftparam.c helpers under it
ftmain.o: ../../dc/ftmain.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c ../../dc/ftmain.c -o ftmain.o
ftparam.o: ../../dc/ftparam.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ftparam.c -o ftparam.o
# the blob shadow, which is a renderer of its own (src/dc/ftshadow.c)
ftshadow.o: ../../dc/ftshadow.c ../../dc/ftshadow.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c ../../dc/ftshadow.c -o ftshadow.o
ifcommon.o: ../../dc/ifcommon.c ../../dc/ifcommon.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ifcommon.c -o ifcommon.o
ifscreenflash.o: ../../dc/ifscreenflash.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ifscreenflash.c -o ifscreenflash.o
# The one block of ft/ftdisplaymain.c the port carries: which fighters
# have left the viewport, which is what the off-screen arrows are drawn
# from.
ftdisplaymain.o: ../../dc/ftdisplaymain.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/ftdisplaymain.c -o ftdisplaymain.o
# Where the game's files are -- /rd on a romdisk build, /cd on a disc one,
# probed once at the first load.
assetroot.o: ../../dc/assetroot.c ../../dc/assetroot.h ../../dc/stackguard.h
	kos-cc $(KOS_CFLAGS) -c ../../dc/assetroot.c -o assetroot.o
# sys/dma.c's one surviving call: the game asks for a ROM byte range and
# gets the file that range was exported to.
dma.o: ../../dc/dma.c ../../dc/dma.h ../../dc/assetroot.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/dma.c -o dma.o
# The bank half of lb/lbparticle.c -- the three free-list allocators and
# the pointerize walk -- and above it ef/efparticle.c, which is the
# decomp's whole file: the bank cache, and the two DMAs behind it.
lbparticle.o: ../../dc/lbparticle.c ../../dc/lbparticle.h \
              ../../dc/lbpdraw.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c ../../dc/lbparticle.c \
	    -o lbparticle.o
# The banks' textures, converted for the PVR at build time and uploaded
# at load time. Not the game's file: see src/dc/lbpartex.h.
lbpartex.o: ../../dc/lbpartex.c ../../dc/lbpartex.h ../../dc/assetroot.h
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c ../../dc/lbpartex.c \
	    -o lbpartex.o
# The RDP that lb/lbparticle.c's renderer draws through, and the PVR
# quads it comes out as: src/dc/lbpdraw.h. Not the game's file either.
lbpdraw.o: ../../dc/lbpdraw.c ../../dc/lbpdraw.h ../../dc/lbpartex.h \
           ../../dc/lbcommon.h ../../dc/objpvr.h
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c ../../dc/lbpdraw.c \
	    -o lbpdraw.o
# ef/efdisplay.c: the four display GObjs that are the only thing in the
# game which draws a particle.
efdisplay.o: ../../dc/efdisplay.c ../../dc/lbpdraw.h ../../dc/lbpartex.h \
             $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c ../../dc/efdisplay.c \
	    -o efdisplay.o
# ef/efmanager.c, the part of it that needs no model: the EFStruct pool
# and every effect maker that reaches straight for lb/lbparticle.c.
efmanager.o: ../../dc/efmanager.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c ../../dc/efmanager.c \
	    -o efmanager.o
efparticle.o: $(SSB_DECOMP_DIR)/src/ef/efparticle.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) \
	    -include sys/debug.h -include sys/dma.h -include sc/scene.h \
	    -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
# ef/efground.c, the background ground actors: the engine
# only -- dEFGroundDatas is unpopulated until a stage's own actor DObj
# tables export (see the file's header).
efground.o: ../../dc/efground.c ../../dc/scmanager.h ../../dc/stage.h \
            $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c ../../dc/efground.c \
	    -o efground.o
# Clip-space polygon clipping, out of fighter.c so the host test links
# it: the near clip and the magnifying glass's sixteen planes.
clip.o: ../../dc/clip.c ../../dc/clip.h
	kos-cc $(KOS_CFLAGS) -c ../../dc/clip.c -o clip.o
# The scene manager's loop and the VS battle scene: sc/scmanager.c and
# sc/sccommon/scvsbattle.c ported. The frame loop lives
# inside these.
scmanager.o: ../../dc/scmanager.c ../../dc/scmanager.h ../../dc/sndres.h \
             ../../dc/assetroot.h ../../dc/scprefetch.h ../../dc/stackguard.h \
             ../../dc/textguard.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/scmanager.c -o scmanager.o
# sc/scmanager.c's three .data tables, on their own so that
# tools/check/backup_oracle.c can compile them against nothing but headers.
scmanagerdata.o: ../../dc/scmanagerdata.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/scmanagerdata.c -o scmanagerdata.o
scvsbattle.o: ../../dc/scvsbattle.c ../../dc/scvsbattle.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/scvsbattle.c -o scvsbattle.o
scvsresults.o: ../../dc/scvsresults.c ../../dc/scvsresults.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/scvsresults.c -o scvsresults.o
# sc/sccommon/scautodemo.c, the auto-demo scene (src/dc/scautodemo.c):
# scvsbattle.c's own recipe under a cycling
# camera. sc/sccommon/scautodemofiles.c is not ported (see
# src/dc/scautodemo.h) -- gmCommonLoadFiles already replaces its one
# function port-wide.
scautodemo.o: ../../dc/scautodemo.c ../../dc/scautodemo.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/scautodemo.c -o scautodemo.o
# sc/sccommon/scexplain.c, the How to Play scene (src/dc/scexplain.c):
# two GameKey fighters posed by real canned
# FTKeyEvent scripts, a 23-phase textbox/icon table, a real Fire Flower
# spawn. sc/sccommon/scexplainfiles.c is not ported (see
# src/dc/scexplain.h) -- gmCommonLoadFiles already replaces its one
# function port-wide.
scexplain.o: ../../dc/scexplain.c ../../dc/scexplain.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/scexplain.c -o scexplain.o
CREDITS_ENCODED = credits/staff.credits.encoded credits/titles.credits.encoded \
                  credits/info.credits.encoded credits/companies.credits.encoded

# scstaffroll.c's own credit text (src/dc/scstaffroll.c), generated at
# build time off the decomp's own human-readable source
# (ssb-decomp-re/src/credits/*.us.txt) by its own standalone,
# ROM-independent converter -- the decomp's own Makefile generates the
# same .encoded/.metadata pair the same way (its own rules, -titleFont
# for staff/titles, -paragraphFont -multiline for info, -paragraphFont
# for companies), just checked into its own build dir instead of this
# one. Each rule's recipe writes both files; only .encoded is a make
# target, the same shape the decomp's own rules take for .metadata.
credits/staff.credits.encoded: $(SSB_DECOMP_DIR)/src/credits/staff.credits.us.txt \
                               $(SSB_DECOMP_DIR)/tools/creditsTextConverter.py
	mkdir -p credits
	python3 $(SSB_DECOMP_DIR)/tools/creditsTextConverter.py $< -titleFont --out-dir credits

credits/titles.credits.encoded: $(SSB_DECOMP_DIR)/src/credits/titles.credits.us.txt \
                                $(SSB_DECOMP_DIR)/tools/creditsTextConverter.py
	mkdir -p credits
	python3 $(SSB_DECOMP_DIR)/tools/creditsTextConverter.py $< -titleFont --out-dir credits

credits/info.credits.encoded: $(SSB_DECOMP_DIR)/src/credits/info.credits.us.txt \
                              $(SSB_DECOMP_DIR)/tools/creditsTextConverter.py
	mkdir -p credits
	python3 $(SSB_DECOMP_DIR)/tools/creditsTextConverter.py $< -paragraphFont -multiline --out-dir credits

credits/companies.credits.encoded: $(SSB_DECOMP_DIR)/src/credits/companies.credits.us.txt \
                                   $(SSB_DECOMP_DIR)/tools/creditsTextConverter.py
	mkdir -p credits
	python3 $(SSB_DECOMP_DIR)/tools/creditsTextConverter.py $< -paragraphFont --out-dir credits

# mv/mvending/mvending.c, the ending diorama (src/dc/mvending.c):
# one posed fighter and a tic-timed fade/light
# sequence that hands off to the staff roll. The room's own static
# props and the two main cameras' animated path are not ported (see
# src/dc/mvending.h).
mvending.o: ../../dc/mvending.c ../../dc/mvending.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvending.c -o mvending.o

# mv/mvopening/mvopeningroom.c, the openings' first scene and their
# spine (src/dc/mvopeningroom.c): Master Hand's desk, the
# game's own default boot scene, and the pattern the other eighteen
# opening scenes follow. Two posed trophy fighters and a tic-timed
# camera sequence are real; the whole relocData room set is the same cut
# mvending.c already takes (src/dc/mvopeningroom.h).
mvopeningroom.o: ../../dc/mvopeningroom.c ../../dc/mvopeningroom.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningroom.c -o mvopeningroom.o
# mv/mvopening/mvopeningportraits.c, the openings' second scene (src/dc/mvopeningportraits.c):
# the sliding fighter portraits, tier 1 of
# the eighteen still-unported opening scenes (mvopeningportraits.h).
mvopeningportraits.o: ../../dc/mvopeningportraits.c ../../dc/mvopeningportraits.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningportraits.c -o mvopeningportraits.o
# mv/mvopening/mvopeningmario.c, the openings' third scene (src/dc/mvopeningmario.c):
# the first of the eight per-fighter scenes,
# a real VS stage (Castle) and a real scripted fighter
# (mvopeningmario.h).
mvopeningmario.o: ../../dc/mvopeningmario.c ../../dc/mvopeningmario.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningmario.c -o mvopeningmario.o
# mv/mvopening/mvopeningdonkey.c, the openings' fourth scene (src/dc/mvopeningdonkey.c):
# the second of the eight per-fighter scenes,
# a real VS stage (Jungle) and a real scripted fighter, reusing Mario's
# own recipe (mvopeningdonkey.h).
mvopeningdonkey.o: ../../dc/mvopeningdonkey.c ../../dc/mvopeningdonkey.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningdonkey.c -o mvopeningdonkey.o

# mv/mvopening/mvopeningsamus.c, the openings' sixth scene (src/dc/mvopeningsamus.c):
# the fourth of the eight per-fighter scenes,
# a real Zebes stage and a real Samus alongside a posed name-card
# intro, reusing Mario's own recipe (mvopeningsamus.h).
mvopeningsamus.o: ../../dc/mvopeningsamus.c ../../dc/mvopeningsamus.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningsamus.c -o mvopeningsamus.o

# mv/mvopening/mvopeninglink.c, the openings' fifth scene (src/dc/mvopeninglink.c):
# the third of the eight per-fighter scenes,
# a real Hyrule stage and a real Link alongside a posed name-card
# intro, reusing Mario's/Donkey's own recipe (mvopeninglink.h).
mvopeninglink.o: ../../dc/mvopeninglink.c ../../dc/mvopeninglink.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeninglink.c -o mvopeninglink.o

# mv/mvopening/mvopeningyoshi.c, the openings' seventh scene (src/dc/mvopeningyoshi.c):
# the fifth of the eight per-fighter
# scenes, a real Yoster's Island stage and a real Yoshi alongside a
# posed name-card intro, reusing Mario's own recipe (mvopeningyoshi.h).
mvopeningyoshi.o: ../../dc/mvopeningyoshi.c ../../dc/mvopeningyoshi.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningyoshi.c -o mvopeningyoshi.o

# mv/mvopening/mvopeningkirby.c, the openings' eighth scene (src/dc/mvopeningkirby.c):
# the sixth of the eight per-fighter scenes, a
# real Dream Land stage and a real Kirby (mvopeningkirby.h).
mvopeningkirby.o: ../../dc/mvopeningkirby.c ../../dc/mvopeningkirby.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningkirby.c -o mvopeningkirby.o

# mv/mvopening/mvopeningfox.c, the openings' ninth scene (src/dc/mvopeningfox.c):
# the seventh of the eight per-fighter scenes, a
# real Sector Z stage and a real Fox (mvopeningfox.h).
mvopeningfox.o: ../../dc/mvopeningfox.c ../../dc/mvopeningfox.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningfox.c -o mvopeningfox.o

# mv/mvopening/mvopeningpikachu.c, the openings' tenth scene (src/dc/mvopeningpikachu.c):
# the eighth and last per-fighter scene,
# a real Saffron City stage and a real Pikachu (mvopeningpikachu.h).
mvopeningpikachu.o: ../../dc/mvopeningpikachu.c ../../dc/mvopeningpikachu.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningpikachu.c -o mvopeningpikachu.o

# mv/mvopening/mvopeningrun.c, the openings' eleventh scene (src/dc/mvopeningrun.c):
# all eight fighters at once, running across a
# scrolling wallpaper (mvopeningrun.h).
mvopeningrun.o: ../../dc/mvopeningrun.c ../../dc/mvopeningrun.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningrun.c -o mvopeningrun.o

# mv/mvopening/mvopeningcliff.c, the openings' twelfth scene (src/dc/mvopeningcliff.c):
# Link on a cliff before a scrolling wallpaper.
mvopeningcliff.o: ../../dc/mvopeningcliff.c ../../dc/mvopeningcliff.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningcliff.c -o mvopeningcliff.o

# mv/mvopening/mvopeningyamabuki.c, the openings' thirteenth scene
# (src/dc/mvopeningyamabuki.c): Pikachu, walking legs, a Poke Ball.
mvopeningyamabuki.o: ../../dc/mvopeningyamabuki.c ../../dc/mvopeningyamabuki.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningyamabuki.c -o mvopeningyamabuki.o

# mv/mvopening/mvopeningjungle.c, the openings' fourteenth scene (src/dc/mvopeningjungle.c):
# Donkey Kong and Samus on Jungle Japes.
mvopeningjungle.o: ../../dc/mvopeningjungle.c ../../dc/mvopeningjungle.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningjungle.c -o mvopeningjungle.o

# mv/mvopening/mvopeningyoster.c, the openings' fifteenth scene (src/dc/mvopeningyoster.c):
# four Yoshis on a nest.
mvopeningyoster.o: ../../dc/mvopeningyoster.c ../../dc/mvopeningyoster.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningyoster.c -o mvopeningyoster.o

# mv/mvopening/mvopeningsector.c, the openings' sixteenth scene (src/dc/mvopeningsector.c):
# the Great Fox and three Arwings.
mvopeningsector.o: ../../dc/mvopeningsector.c ../../dc/mvopeningsector.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningsector.c -o mvopeningsector.o

# mv/mvopening/mvopeningstandoff.c, the openings' seventeenth scene
# (src/dc/mvopeningstandoff.c): Mario and Kirby face off.
# mv/mvopening/mvopeningclash.c, the openings' eighteenth scene (src/dc/mvopeningclash.c):
# the eight fighters clash.
# mv/mvopening/mvopeningnewcomers.c, the openings' nineteenth and last
# scene (src/dc/mvopeningnewcomers.c): the four silhouettes.
mvopeningnewcomers.o: ../../dc/mvopeningnewcomers.c ../../dc/mvopeningnewcomers.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningnewcomers.c -o mvopeningnewcomers.o

mvopeningclash.o: ../../dc/mvopeningclash.c ../../dc/mvopeningclash.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningclash.c -o mvopeningclash.o

mvopeningstandoff.o: ../../dc/mvopeningstandoff.c ../../dc/mvopeningstandoff.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mvopeningstandoff.c -o mvopeningstandoff.o
# sc/sccommon/scstaffroll.c, the staff roll (src/dc/scstaffroll.c):
# the scrolling job/name credits list and its
# name-shooting minigame. Credit text is real (creditsTextConverter.py,
# below); the scrolling glyphs' own raw texture blocks are not ported
# (see src/dc/scstaffroll.h). -I. (this directory, where kos-cc already
# runs) resolves its own #include "credits/*.encoded"/"*.metadata"
# against $(CREDITS_ENCODED) below, generated at build time the same
# way the decomp's own Makefile generates them.
scstaffroll.o: ../../dc/scstaffroll.c ../../dc/scstaffroll.h $(DC_HDRS) \
               $(SHIM_HDRS) $(CREDITS_ENCODED)
	kos-cc $(KOS_CFLAGS) -I. -c ../../dc/scstaffroll.c -o scstaffroll.o
gmcommon.o: ../../dc/gmcommon.c ../../dc/gmcommon.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/gmcommon.c -o gmcommon.o

gmcamera.o: ../../dc/gmcamera.c ../../dc/gmcamera.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/gmcamera.c -o gmcamera.o
# lb/lbcommon.c's table trig, a verbatim copy (see the file)
lbcommon.o: ../../dc/lbcommon.c ../../dc/lbcommon.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/lbcommon.c -o lbcommon.o
# sys/vector.c, the game's vector algebra, compiled unmodified
vector.o: $(SSB_DECOMP_DIR)/src/sys/vector.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c $< -o $@
# sys/utils.c, the game's random numbers, trig helpers and qsort,
# compiled unmodified; the entry focus draws its id from its RNG.
utils.o: $(SSB_DECOMP_DIR)/src/sys/utils.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c $< -o $@
# sys/matrix.c, the game's 4x4s. lb/lbparticle.c's lbParticleGetPosVelDObj
# builds a joint's world matrix out of syMatrixScaF, syMatrixRotRpyRF and
# syMatrixTraF to read a generator's position and velocity off it, and the
# file compiles whole and unmodified, so all of it comes in rather than
# three functions being copied out. Most of it is unreferenced today and
# gm/gmcamera.c is what will want the rest. It reads gSYSinTable, which is
# sintable.o below.
matrix.o: $(SSB_DECOMP_DIR)/src/sys/matrix.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c $< -o $@
# libultra's own gu, out of ssb-decomp-re/src/libultra/gu. The three
# below compile unmodified and are the SDK's code: guMtxIdentF and the
# Mtx converters (mtxutil), guMtxCatF (mtxcatf) and guNormalize
# (normalize). sys/matrix.c above and src/dc/lbparticle.c call them.
# They are the decomp's own libultra source, not hand-written copies.
mtxutil.o: $(SSB_DECOMP_DIR)/src/libultra/gu/mtxutil.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c $< -o $@
mtxcatf.o: $(SSB_DECOMP_DIR)/src/libultra/gu/mtxcatf.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c $< -o $@
normalize.o: $(SSB_DECOMP_DIR)/src/libultra/gu/normalize.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c $< -o $@
mtxxfmf.o: $(SSB_DECOMP_DIR)/src/libultra/gu/mtxxfmf.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c $< -o $@
# The other two of that directory cannot compile as written: their
# constant tables go through a union that only reads correctly on a
# big-endian machine, and their #pragma weak aliases would take sinf and
# cosf from newlib. tools/export/ssb_trigexport.py writes the port's
# versions from the decomp at build time (it says why in full), and
# tools/check/trig_check.py holds them to the decomp line by line and to
# the ROM's .rodata word by word.
#
# -fno-strict-aliasing is not a warning switch, it is correctness. Both
# functions start `ix = *(int *)&x;` to look at the argument's exponent,
# which is exactly the pun -O2 is entitled to assume never happens: with
# strict aliasing on, GCC may keep `x` in a register and read the int
# from a stale slot. libultra was built by a compiler that did not do
# that. -Wno-strict-aliasing would only hide the warning.
GU_CFLAGS = -fno-strict-aliasing -Wno-missing-braces
gusinf.c gucosf.c &: ../../../tools/export/ssb_trigexport.py
	cd ../../.. && python3 tools/export/ssb_trigexport.py --out-dir $(SELF)
gusinf.o: gusinf.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(GU_CFLAGS) -c $< -o $@
gucosf.o: gucosf.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(GU_CFLAGS) -c $< -o $@
# gSYSinTable itself: 4 KB of .data in the boot segment, which no yaml
# segment names, so tools/export/ssb_sinexport.py converts the address and emits
# the table as C. Not a scene asset and not on the disc -- every scene
# reads it, and it is wanted before a file is open.
sintable.c: $(SINTOOL_DEPS)
	cd ../../.. && python3 tools/export/ssb_sinexport.py \
	    --rom baserom.z64 \
	    --emit-c $(SELF)/sintable.c
sintable.o: sintable.c
	kos-cc $(KOS_CFLAGS) -c $< -o $@
mpcommon.o: ../../dc/mpcommon.c $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mpcommon.c -o mpcommon.o
mpshim.o: ../../dc/mpshim.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mpshim.c -o mpshim.o
taskman.o: ../../dc/taskman.c ../../dc/taskman.h ../../dc/assetroot.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/taskman.c -o taskman.o
# sys/malloc.c is the game's bump allocator, compiled unmodified. The
# object is named symalloc.o so nothing confuses it with libc's malloc.
symalloc.o: $(SSB_DECOMP_DIR)/src/sys/malloc.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c $< -o $@
bgm.o: ../../dc/bgm.c ../../dc/bgm.h \
       ../../dc/bgmbank.h ../../dc/stackguard.h ../../dc/dcaica.h \
       ../../dc/bgmarm.h ../../dc/aica/shared.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/bgm.c -o bgm.o

bgmbank.o: ../../dc/bgmbank.c ../../dc/bgmbank.h
	kos-cc $(KOS_CFLAGS) -c ../../dc/bgmbank.c -o bgmbank.o

# The AICA firmware: src/dc/aica/main.c, ours, linked with KallistiOS's
# own crt0.s (the ARM's vectors and its 4410 Hz clock interrupt) and
# aica.c (the channel registers), compiled straight from the image's KOS
# tree -- the same queue and the same volume, pan and pitch arithmetic as
# stream.drv, so fgm.c cannot tell the two apart. ARMv3:
# DC_ARM_CFLAGS is -mcpu=arm7di.
AICA_KOS = $(KOS_BASE)/kernel/arch/dreamcast/sound/arm
#
# -fno-schedule-insns: GCC's pre-allocation scheduling pass (part of -O2)
# is left off for the ARM7 firmware, since a recompiling ARM7 core runs
# src/dc/seq/seqcore.c's fetch() wrong with it on (the sequencer drifts
# from the second chord of a song) although the code is correct ARMv3.
AICA_CFLAGS = $(DC_ARM_CFLAGS) -fno-schedule-insns -marm -ffreestanding \
              -I$(AICA_KOS) -I$(KOS_BASE)/kernel/arch/dreamcast/include/dc/sound
aica_crt0.o: $(AICA_KOS)/crt0.s
	$(DC_ARM_AS) $(DC_ARM_AFLAGS) $< -o $@
aica_kos.o: $(AICA_KOS)/aica.c
	$(DC_ARM_CC) $(AICA_CFLAGS) -c $< -o $@
AICA_HDRS = ../../dc/aica/shared.h ../../dc/aica/seqsyn.h \
            ../../dc/seq/seqcore.h ../../dc/bgmbank.h
aica_main.o: ../../dc/aica/main.c $(AICA_HDRS)
	$(DC_ARM_CC) $(AICA_CFLAGS) -c $< -o $@
aica_seqsyn.o: ../../dc/aica/seqsyn.c $(AICA_HDRS) ../../dc/aicamix.h \
               aica_sends.h
	$(DC_ARM_CC) $(AICA_CFLAGS) -I. -c $< -o $@
# The N64's reverb as a DSP program, and the tables the voices' sends are
# worked out from, written from the decomp's dSYAudioCustomFXParams and
# n_eqpower (tools/check/reverb_check.py holds the
# program to the N64's reverb).
aica_reverb.h aica_sends.h &: ../../../tools/export/ssb_reverbexport.py
	cd ../../.. && python3 tools/export/ssb_reverbexport.py --out-dir $(SELF)
# the port's own sequencer, the same source the host oracle
# and tools/check/bgm_check.py run
aica_seqcore.o: ../../dc/seq/seqcore.c ../../dc/seq/seqcore.h ../../dc/bgmbank.h
	$(DC_ARM_CC) $(AICA_CFLAGS) -c $< -o $@
aica_fw.elf: aica_crt0.o aica_main.o aica_seqsyn.o aica_seqcore.o aica_kos.o
	$(DC_ARM_CC) -mcpu=arm7di -Wl,-Ttext=0x00000000,-Map,aica_fw.map,-N,--no-warn-rwx-segments \
	    -nostartfiles -nostdlib -e reset -o $@ $^ -lgcc
# crt0.s starts the stack at 0xB000 and it grows down; everything the
# program keeps (_end covers .bss) must leave it 4 KB.
aica_fw.bin: aica_fw.elf
	$(DC_ARM_OBJCOPY) -O binary $< $@
	@end=$$($(DC_ARM_BASE)/bin/$(DC_ARM_PREFIX)-nm $< | \
	        awk '$$3 == "_end" { print $$1 }'); \
	 if [ $$((0x$$end)) -gt $$((0xA000)) ]; then \
	   echo "aica_fw.elf ends at 0x$$end; it must end below 0xA000," \
	        "4 KB under crt0.s's stack at 0xB000"; exit 1; fi
aica_fwblob.o: ../../dc/aica/fwblob.S aica_fw.bin
	kos-cc $(KOS_CFLAGS) -Wa,-I. -c $< -o $@
dcaica.o: ../../dc/dcaica.c ../../dc/dcaica.h ../../dc/aica/shared.h
	kos-cc $(KOS_CFLAGS) -c ../../dc/dcaica.c -o dcaica.o
# the SH-4's half of the ARM sequencer: the bank image and the commands
# (bgmarm.c), the oscillator tables (seqprep.c, f32 through __sinf), and
# seqcore.c for the one function seqprep.c borrows
SEQ_HDRS = ../../dc/seq/seqcore.h ../../dc/seq/seqprep.h ../../dc/bgmbank.h
bgmarm.o: ../../dc/bgmarm.c ../../dc/bgmarm.h ../../dc/dcaica.h \
          ../../dc/aica/shared.h $(SEQ_HDRS) aica_reverb.h
	kos-cc $(KOS_CFLAGS) -I. -c ../../dc/bgmarm.c -o bgmarm.o
seqprep.o: ../../dc/seq/seqprep.c $(SEQ_HDRS)
	kos-cc $(KOS_CFLAGS) -fno-strict-aliasing -c ../../dc/seq/seqprep.c -o seqprep.o
seqcore.o: ../../dc/seq/seqcore.c $(SEQ_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/seq/seqcore.c -o seqcore.o
# The FGM engine the motion scripts' sound events reach from,
# and the sound players over it: syaudio.c is sys/audio.c's FGM half,
# where syAudioPlayFGM and the handles it hands back live.
fgm.o: ../../dc/fgm.c ../../dc/fgm.h ../../dc/be.h ../../dc/aicamix.h \
       ../../dc/dcaica.h ../../dc/aica/shared.h
	kos-cc $(KOS_CFLAGS) -c ../../dc/fgm.c -o fgm.o
syaudio.o: ../../dc/syaudio.c ../../dc/syaudio.h ../../dc/fgm.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/syaudio.c -o syaudio.o
sndres.o: ../../dc/sndres.c ../../dc/sndres.h ../../dc/bgm.h ../../dc/bgmbank.h \
          ../../dc/fgm.h ../../dc/assetroot.h ../../dc/scmanager.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/sndres.c -o sndres.o
# The object system, compiled unmodified out of the decomp. sys/objanim.c
# is the whole AObj animation engine -- gcParseDObjAnimJoint, the
# AObjEvent32 parser, included -- and sys/interp.c the curve evaluator
# under it. ft/ftanim.c is the figatree half: ftAnimParseDObjFigatree is
# the AObjEvent16 parser only fighters run, and the only decomp file
# outside sys/ that the object system needs; its
# umbrella header, ft/fighter.h, is the real one.
# tools/check/figatree_check.py checks both parsers against the ROM's 1775
# animations, and they are also what animates
# Mario here: src/dc/ftcommon.c hangs a script on each of his DObjs and
# ftParamUpdateAnimKeys runs it through whichever parser the motion's
# FTANIM_FLAG_ANIMJOINT names (every Appear is an AObjEvent32 script).
#
# sys/objdisplay.c is deliberately absent: its body is the GBI emitter,
# and src/dc/objdisplay.c stands in its place.
objman.o: $(SSB_DECOMP_DIR)/src/sys/objman.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c $< -o $@
objhelper.o: $(SSB_DECOMP_DIR)/src/sys/objhelper.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c $< -o $@
objscript.o: $(SSB_DECOMP_DIR)/src/sys/objscript.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c $< -o $@
# objanim.o is src/dc/objanim.c, which #includes the decomp's file whole
# and stands in front of two of its functions: gcPlayMObjMatAnim's linear
# colour lerp is big-endian byte arithmetic.
objanim.o: ../../dc/objanim.c $(SSB_DECOMP_DIR)/src/sys/objanim.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -Wno-strict-aliasing -c $< -o $@
interp.o: $(SSB_DECOMP_DIR)/src/sys/interp.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c $< -o $@
# ft/ftanim is overlay 2; sys/objanim.c and sys/interp.c above are not in
# any overlay -- the object system is always resident -- which is why they
# take no rename.
ftanim.o: $(SSB_DECOMP_DIR)/src/ft/ftanim.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
# The Dreamcast side of what the object system reaches for outside itself.
sysshim.o: ../../dc/sysshim.c ../../dc/stackguard.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/sysshim.c -o sysshim.o
# -DDB_STACK_GUARD's thread stacks (src/dc/stackguard.h); empty without it.
stackguard.o: ../../dc/stackguard.c ../../dc/stackguard.h
	kos-cc $(KOS_CFLAGS) -c ../../dc/stackguard.c -o stackguard.o
# -DDB_TEXT_GUARD's code checksums (src/dc/textguard.h); empty without it.
textguard.o: ../../dc/textguard.c ../../dc/textguard.h
	kos-cc $(KOS_CFLAGS) -c ../../dc/textguard.c -o textguard.o
# -DDB_PRIM_DUMP's recorder (src/dc/primdump.h): empty without the flag.
primdump.o: ../../dc/primdump.c ../../dc/primdump.h
	kos-cc $(KOS_CFLAGS) -c ../../dc/primdump.c -o primdump.o

# The 1P team cards' baked crowds (src/dc/introcrowd.h).
introcrowd.o: ../../dc/introcrowd.c ../../dc/introcrowd.h ../../dc/assetroot.h \
              ../../dc/dcpvr.h ../../dc/lbcommon.h ../../dc/objpvr.h \
              ../../dc/taskman.h
	kos-cc $(KOS_CFLAGS) -c ../../dc/introcrowd.c -o introcrowd.o

# -DDB_DISC_TRACE's recorder (src/dc/disctrace.h): empty without the flag.
disctrace.o: ../../dc/disctrace.c ../../dc/disctrace.h
	kos-cc $(KOS_CFLAGS) -c ../../dc/disctrace.c -o disctrace.o

# -DDB_LOAD_CENSUS's recorder (src/dc/loadcensus.h): empty
# without the flag.
loadcensus.o: ../../dc/loadcensus.c ../../dc/loadcensus.h
	kos-cc $(KOS_CFLAGS) -c ../../dc/loadcensus.c -o loadcensus.o

# The save store, and the memory card behind it (src/dc/vmusave.h). Not
# an overlay module: sys/dma.c is always resident, and the save data the
# store holds is the scene manager's, so there is nothing here for a
# reload to clear.
vmusave.o: ../../dc/vmusave.c ../../dc/vmusave.h ../../dc/vmucard.h \
           ../../dc/stackguard.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/vmusave.c -o vmusave.o
# The memory cards under the store (src/dc/vmucard.h): vmufs and the
# save file's pictures, off the disc's I/O lock.
vmucard.o: ../../dc/vmucard.c ../../dc/vmucard.h ../../dc/assetroot.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/vmucard.c -o vmucard.o
# The port's own text in the staff roll's font (src/dc/dctext.h), and every
# string it says (src/dc/dcstrings.h).
dctext.o: ../../dc/dctext.c ../../dc/dctext.h ../../dc/dcstrings.h \
          ../../dc/sprite.h ../../dc/scstaffroll.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/dctext.c -o dctext.o
# The "SAVING..." notice over every frame (src/dc/vmunotice.h).
vmunotice.o: ../../dc/vmunotice.c ../../dc/vmunotice.h ../../dc/dctext.h \
             ../../dc/dcstrings.h ../../dc/vmusave.h ../../dc/scport.h \
             $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/vmunotice.c -o vmunotice.o
# syDmaLoadOverlay and the table it reads (src/dc/overlay.h)
overlay.o: ../../dc/overlay.c ../../dc/overlay.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/overlay.c -o overlay.o
objdisplay.o: ../../dc/objdisplay.c ../../dc/objpvr.h ../../dc/mtx.h \
              $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/objdisplay.c -o objdisplay.o
# The bridge between a .pack model and a DObj tree -- the port's own, with
# no decomp counterpart; see objmodel.h.
objmodel.o: ../../dc/objmodel.c ../../dc/objmodel.h ../../dc/objpvr.h \
            ../../dc/fighter.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/objmodel.c -o objmodel.o
# mp/mpcollision and mp/mpprocess are overlay 2 (and their statics are
# most of what a second battle would have read stale).
mpcollision.o: $(SSB_DECOMP_DIR)/src/mp/mpcollision.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
mpprocess.o: $(SSB_DECOMP_DIR)/src/mp/mpprocess.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
# Hit detection is the decomp's gm/gmcollision.c, compiled unmodified
# like the map collision above: matrix and sphere/box maths over the
# FTParts each fighter joint carries. It calls syDebugPrintf and
# scManagerRunPrintGObjStatus without including a header that declares
# them (IDO took the implicit declarations); -include supplies the two
# the shim tree has, so the build stays warning-free. ft/ftcommondata.c
# is the handicap table the knockback formula indexes, with the rest of
# that file's tables beside it.
# both overlay 2 as well; neither keeps a static today, and they are in
# the segment so that neither can start keeping one unnoticed.
gmcollision.o: $(SSB_DECOMP_DIR)/src/gm/gmcollision.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) \
	    -include sys/debug.h -include sc/scene.h -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
ftcommondata.o: $(SSB_DECOMP_DIR)/src/ft/ftcommondata.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
# gm/gmcolscripts.c, the colour-animation scripts and their priorities
# (for the screen flash): u32 arrays with the address of
# another script in a word wherever a command jumps, which is a 32-bit
# word here as on the N64. Data only, .data and no .bss; the rename is
# the same one the two above take, so overlay_check holds it to the
# same rule. The host cannot compile the file as it is (a 64-bit
# address does not fit the word) and takes hosttest_gmcolscripts.c.
gmcolscripts.o: $(SSB_DECOMP_DIR)/src/gm/gmcolscripts.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
# ft/ftphysics.c, the fighter's velocity maths, compiled unmodified.
# src/dc/ftcommon.c carries no hand copies of its 31 functions, and the nine
# functions the port never copied (ftPhysicsClampGroundVel, ftPhysicsApplyClampGroundVelStickRange,
# ftPhysicsSetGroundVelStickRange, ftPhysicsApplyGroundVelTransferAir,
# ftPhysicsAddClampAirVelY, ftPhysicsClampAirVelY, ftPhysicsApplyAirVelDrift,
# ftPhysicsSetAirVelTransN, ftPhysicsApplyAirVelTransNYZ) came with it --
# every one of them called by a status the moveset still owes.
# Overlay 2, and it keeps no static; it takes the rename anyway so that it
# cannot start keeping one unnoticed.
ftphysics.o: $(SSB_DECOMP_DIR)/src/ft/ftphysics.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
# The seven smallest common statuses, compiled unmodified
# 20 functions in 328 lines, all of overlay 3 (they sit between
# 0x0BEC00 and 0x0CB8C0 in smashbrothers.us.yaml). None of them is in
# that overlay's bss list -- they keep no statics -- but they take the
# rename with everything else so that they cannot start keeping one.
# Two replace code src/dc/ftcommon.c already had: the StopCeil setter
# (which had silently dropped its ftMainPlayAnimEventsAll) and the
# ReboundWait setter (once a do-nothing stub ft/ftmain.c called). Of the
# other five, ftcommoncliffattack.c is reached and four are not.
ftcommonstopceil.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonstopceil.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommondownforwardback.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommondownforwardback.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommondownattack.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommondownattack.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonrebound.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonrebound.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommoncliffattack.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommoncliffattack.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonlandingair.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonlandingair.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# The five complete special moves the sweep found closed, compiled
# unmodified: 69 functions in 924 lines, all of overlay
# 3. Every one was already reachable from the port's objects -- what was
# missing was never the code, it was the per-fighter status table to put
# it in and the Down-B check to enter it from.
ftmariospeciallw.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftmario/ftmariospeciallw.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftdonkeyspeciallw.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftdonkey/ftdonkeyspeciallw.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftpurinspeciallw.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftpurin/ftpurinspeciallw.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftkirbyspeciallw.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirbyspeciallw.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftpurinspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftpurin/ftpurinspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftkirbycopypurinspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirbycopypurinspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Fox's Blaster's caller (ftfoxspecialn.c, overlay 3, the
# roster's second Neutral-B status file after Purin's).
ftfoxspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftfox/ftfoxspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Purin's Up-B (Sing), her third and last special-move file --
# specialn and speciallw are separate files. Six procs, two statuses,
# every callee already in the port (ftAnimEndSetWait/SetFall, the
# mpCommonProcFighter* landing/edge/ground/air setters, ftPhysicsClampAir-
# VelXMax -- all of them Mario's or Purin's own from earlier steps); the
# only new leaf is efManagerPurinSingMakeEffect (src/dc/efmanager.c, this
# same step), and even that is unreached until fp->motion_vars.flags.flag1
# fires, which no host mock motion table sets.
ftpurinspecialhi.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftpurin/ftpurinspecialhi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Donkey's Up-B (Spinning Kong), the roster's third fighter with
# a whole moveset. 11 procs, two statuses, zero new leaves -- every callee
# (ftAnimEndSetWait, ftCommonFallSpecialSetStatus, the nine ft/ftphysics.c
# functions compiled unmodified, mpCommonProcFighterOnFloor/
# Cliff, mpCommonSetFighterGround/Air, ftMainSetStatus, ftMainPlayAnimEvents-
# All) is already in the port. No weapon, no effect --
# Spinning Kong is pure movement, so this is a straight table-and-demux entry
# like Purin's Sing but with nothing new to link in at all.
ftdonkeyspecialhi.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftdonkey/ftdonkeyspecialhi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Samus's Up-B (Screw Attack), her FIRST special file -- unlike
# Donkey/Purin this fighter starts from FT_SPECIAL_NONE (dFTMainSpecial-
# StatusDescs[nFTKindSamus] was a zero stub), so this file creates
# dFTSamusSpecialStatusDescs from scratch rather than adding to an existing
# table. 7 procs, two statuses, zero new leaves -- every callee (the
# ft/ftphysics.c functions compiled unmodified, including
# ftPhysicsApplyGravityDefault/AirVelTransNYZ, mpCommonCheckFighterProject/
# PassCliff, ftCommonFallSpecialSetStatus/CliffCatchSetStatus/LandingFall-
# SpecialSetStatus, ftMainSetStatus/PlayAnimEventsAll) was already in the
# port. Unlike Donkey's SetStatus pair, Samus's ground/air setters target
# their OWN status id each (no same-tic quirk).
ftsamusspecialhi.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftsamus/ftsamusspecialhi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Captain Falcon's Neutral-B (Falcon Punch), his FIRST special
# file -- dFTMainSpecialStatusDescs[nFTKindCaptain] was FT_SPECIAL_NONE, and
# the Neutral-B demux's slot 7 has named this exact setter in a comment.
# 11 procs, two statuses. One genuinely new leaf: the
# ProcPhysics pair calls efManagerCaptainFalconPunchMakeEffect (efmanager.c)
# to attach Falcon's flame, ported the same "descriptor + maker,
# no sEFManagerModels row yet" shape Purin's
# Sing established -- it returns NULL on host and target alike today, same as Sing
# does, since ftCaptainSpecialNUpdateEffect already handles that return
# value the decomp's own way. Every other callee (ftPhysicsApplyGroundVel-
# TransN/AirVelFriction/AirVelDriftFastFall, mpCommonSetFighterGround/Air,
# mpCommonProcFighterOnEdge/Landing, ftMainSetStatus/PlayAnimEventsAll) was
# already in the port.
ftcaptainspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftcaptain/ftcaptainspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Samus's Down-B (Bomb). ftsamus.o joins FTCHAR_DATA_OBJS (ovl2,
# the same bss-only file-base-pointer shape as ftmario/ftfox/ftpurin above)
# for gFTDataSamusMain, which wpsamusbomb.c's own WPDesc names; wpsamusbomb.o
# is ovl3 (the bomb itself), the same split wpmariofireball/wpfoxblaster
# stand as. dFTCommonSpecialLwStatusList's Samus/NSamus slots have named
# ftSamusSpecialLwSetStatus in a comment since the Down-B demux itself was
# built, so filling the table alone reaches it -- no demux wiring needed,
# the same shape Falcon Punch's Neutral-B slot has. One new
# leaf: efManagerSparkleWhiteMultiExplodeMakeEffect (efmanager.c),
# the bomb's hit/expire/absorb spark -- the same plain-particle
# shape efManagerSparkleWhiteMakeEffect already has, a
# different script id. Every other callee (ftPhysicsApplyGroundVelFriction/
# GravityDefault/ClampAirVelXDec/ClampAirVelX, mpCommonSetFighterGround/Air,
# mpCommonProcFighterOnEdge/Landing, ftMainSetStatus/PlayAnimEventsAll,
# gmCollisionGetFighterPartsWorldPosition) was already in the port.
ftsamusspeciallw.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftsamus/ftsamusspeciallw.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Captain's Down-B (Falcon Kick). Five statuses, not two --
# SpecialLw (grounded kick), SpecialLwAir (grounded kick that runs off an
# edge mid-move), SpecialLwLanding (landing after the aerial version),
# SpecialAirLw (the genuine aerial input), SpecialLwBound (bouncing off a
# wall) -- the biggest table this port has filled in one step, but still
# one self-contained named move: no weapon, no it/, every leaf either
# already in the port (ftPhysicsApplyGroundVelTransN/AirVelFriction/
# AirVelTransNAll, mpCommonSetFighterGround/Air, mpCommonProcFighterOnEdge/
# OnFloor/Landing/FallOnEdgeBreak/WaitOrLanding, ftMainSetStatus/
# PlayAnimEventsAll, ftAnimEndSetWait/SetFall/CheckSetStatus,
# ftCommonWaitSetStatus/FallSetStatus, ftParamProcPauseEffect/
# ResumeEffect/StopEffect) or the one new leaf efmanager.c gains for it,
# efManagerCaptainFalconKickMakeEffect (the boot's dust trail, the same
# "descriptor + maker, no sEFManagerModels row yet" shape Falcon Punch's
# flame has).
ftcaptainspeciallw.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftcaptain/ftcaptainspeciallw.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Yoshi's Down-B (Yoshi Bomb), his FIRST special file at all
# -- dFTMainSpecialStatusDescs[nFTKindYoshi] was FT_SPECIAL_NONE before
# that. Four statuses: SpecialLwStart (the ground hop), SpecialAirLwStart
# (the same hop from the air), SpecialAirLwLoop (the plunge, a motion-less
# row -- motion id -1, no ProcUpdate, entered with FTSTATUS_PRESERVE_HIT and
# the caller's own anim frame) and SpecialLwLanding, which is where the
# stars come from (wpYoshiStarMakeStars, wp/wpyoshi/wpyoshistar.c, below).
# Ten procs, compiled unmodified. No new effect leaf at all: the star's own
# ProcUpdate/ProcHit/ProcShield call efManagerDustExpandSmallMakeEffect and
# efManagerSparkleWhiteMakeEffect, both in the port.
# Every other callee (ftPhysicsCheckClampAirVelXDecMax/ApplyAirVelXFriction/
# ApplyAirVelTransNAll/ApplyGroundVelFriction, mpCommonCheckFighterCeilHeavy/
# CeilHeavyCliff, mpCommonSetFighterGround/Air/FallOnGroundBreak,
# ftCommonCliffCatchSetStatus/WaitSetStatus, ftAnimEndCheckSetStatus,
# ftMainSetStatus/PlayAnimEventsAll, gmCollisionGetFighterPartsWorldPosition,
# syVectorRotateAbout3D, syUtilsArcTan2, wpMainDecLifeCheckExpire/
# ReflectorSetLR, wpManagerMakeWeapon) was already in the port.
ftyoshispeciallw.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftyoshi/ftyoshispeciallw.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Donkey's Neutral-B (Giant Punch), his third and last special
# file -- Down-B's Hand Slap and Up-B's Spinning Kong are separate
# files, so this finishes his table. Eight statuses (Start/Loop/End/Full,
# ground and air each), twenty-four procs, compiled unmodified. No weapon,
# no it/, and no new leaf at all: the charge lives in passive_vars.donkey
# .charge_level and status_vars.donkey.specialn, the hitbox damage bonus is
# added in ftDonkeySpecialNEndProcUpdate over fp->attack_colls, and the one
# visual (ftParamCheckSetFighterColAnimID at full charge) is the port's
# existing colanim stub, returning FALSE. Every callee (ftAnimEndCheckSet-
# Status, mpCommonProcFighterOnEdge/Landing, mpCommonSetFighterGround/Air/
# WaitOrFall, ftPhysicsClampAirVelXMax/ApplyGroundVelFriction/ApplyAirVel-
# Friction, gcSetAnimSpeed, ftCommonEscapeCheckInterruptSpecialNDonkey,
# ftMainSetStatus/PlayAnimEventsAll, mpCommonSetFighterFallOnEdgeBreak) was
# already in the port; fp->proc_damage, which six of the setters assign,
# is called from ftParamProcDamage the decomp's own way.
ftdonkeyspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftdonkey/ftdonkeyspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Captain's Up-B (Falcon Dive), his third and last special file
# -- Falcon Punch and Falcon Kick are separate files, so
# this finishes his table. Four statuses (SpecialHi, SpecialHiCatch,
# SpecialHiThrow, SpecialAirHi), twelve procs, compiled unmodified. It is a
# grab: ftParamSetCatchParams names ftCaptainSpecialHiProcCatch for the
# catcher and ftCommonCaptureCaptainProcCapture for the caught, and the
# caught fighter's side is ft/ftcommon/ftcommoncapturecaptain.c below
# (overlay 3, four procs, one common status row -- 179, CaptureCaptain --
# in src/dc/ftcommon.c). The one thing neither file could find in the port
# was data, not code: ftCommonCaptureCaptainUpdatePositions reads a Vec2h
# per victim FTKind out of Captain's motion file (gFTDataCaptainMainMotion
# at llCaptainMainMotionSpecialHiVec2h); the port never loads that file, so
# src/dc/ftcommon.c carries the 27-row table verbatim from the decomp's own
# relocData/235_CaptainMainMotion.c and binds the file base to it at every
# overlay load. Every other callee (ftCommonFallSpecialSetStatus,
# efManagerQuakeMakeEffect, ftParamSetStickLR/SetCatchParams/SetCapture-
# ImmuneMask/StopVoiceRunProcDamage, ftPhysicsCheckClampAirVelXDec/ClampAir-
# VelXStickRange/ApplyAirVelXFriction/ApplyAirVelTransNAll/StopVelAll,
# mpCommonProcFighterCliffWaitOrLanding/Project, mpCommonCheckFighterCeil-
# HeavyCliff, mpCommonSetFighterAir, ftCommonCliffCatchSetStatus,
# ftCommonThrownReleaseFighterLoseGrip/SetStatusDamageRelease/ReleaseThrown-
# UpdateStats, syVector*, gmCollisionGetFighterPartsWorldPosition) was
# already in the port.
ftcaptainspecialhi.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftcaptain/ftcaptainspecialhi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommoncapturecaptain.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommoncapturecaptain.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Yoshi's Neutral-B (Egg Lay), his second special file --
# Yoshi Bomb is his first, so this leaves only his Up-B. Six
# statuses (SpecialN/Catch/Release and the air three), twenty-nine procs,
# compiled unmodified. It is a grab, and the same shape as Captain's Falcon
# Dive above: ftParamSetCatchParams names ftYoshiSpecialNCatchProcCatch for
# the catcher and ftCommonCaptureYoshiProcCapture for the caught, and the
# caught fighter's side is ft/ftcommon/ftcommoncaptureyoshi.c below (overlay
# 3, twelve procs and a 27-row per-victim descriptor table of its own, two
# common status rows -- 177 CaptureYoshi and 178 YoshiEgg -- in
# src/dc/ftcommon.c). Between them the two files named exactly three symbols
# the port did not have, and every other callee was already in: the egg's
# two effects (efManagerYoshiEggLay/EggExplodeMakeEffect, DIVERGES-stubbed
# in src/dc/ftcommon.c, the egg is unmodeled like Samus's Bomb and Yoshi's
# stars) and ftKirbySpecialNApplyCaptureDamage, ported verbatim beside them
# -- the egg charges its five points of damage through Kirby's helper, which
# is the game's own joke and not a mistake.
ftyoshispecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftyoshi/ftyoshispecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommoncaptureyoshi.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommoncaptureyoshi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Samus's Neutral-B (Charge Shot), her third and last special
# file -- Screw Attack and Bomb are separate files, so
# this finishes her table. Five statuses (SpecialNStart/Loop/End and the
# air Start/End), twenty-two procs, compiled unmodified; the shot itself is
# wp/wpsamus/wpsamuschargeshot.c below (ovl3, the Blaster's weapon-plus-
# caller shape). One new leaf, efManagerImpactShockMakeEffect (the hit
# flash, src/dc/efmanager.c, the same plain-particle shape SparkleWhite
# has). The charge lives in passive_vars.samus.charge_level across
# statuses, the held shot is a weapon parented to Samus (WEAPON_FLAG_PARENT_
# FIGHTER, no COLLPROJECT until fired) that the Loop resizes as it charges.
# ftSamusSpecialNLoopProcInterrupt calls ftCommonEscapeSetStatus with two
# arguments where the port's (and the game's) function takes three -- the
# decomp comments the prototype out and calls it undefined behaviour out
# loud (ftsamusspecialn.c:167-171); compiled as it stands, the third
# argument is whatever the register holds, and it only feeds the escape's
# item-throw buffer, which nothing reads until it/ arrives. Every other
# callee (ftParamPlayLoopSFX/StopLoopSFX, wpMainVelSetLR/ReflectorSetLR/
# DestroyWeapon, wpMapTestAllCheckCollEnd, wpProcessUpdateHitPositions,
# mpCommonRunWeaponCollisionDefault, mpCommonProcFighterOnEdge/Landing,
# mpCommonSetFighterGround/Air/WaitOrFall, ftCommonEscapeGetStatus/
# WaitSetStatus, ftPhysicsClampAirVelXMax, gmCollisionGetFighterParts-
# WorldPosition, efManagerDustExpandSmallMakeEffect, syVectorRotateAbout3D)
# was already in the port.
ftsamusspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftsamus/ftsamusspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Fox's Down-B (Reflector), his second special file -- the
# Blaster is a separate file. Ten statuses (Start/Hit/End/Loop/Turn,
# ground and air each), twenty-three procs, compiled unmodified. No weapon,
# no it/. Two things it needed: the reflector effect (dEFManagerFoxReflector-
# EffectDesc and its maker/two procs, src/dc/efmanager.c, verbatim -- the
# maker returns NULL on host and target alike until a sEFManagerModels row
# exists, the same shape Sing's/Falcon Punch's/Falcon Kick's took, and
# ftfoxspeciallw.c handles that NULL the decomp's own way), and data:
# ftFoxSpecialLwStartInitStatusVars points fp->special_coll into Fox's
# motion file (gFTDataFoxMainMotion at llFoxMainMotionLwReflectorFTSpecial-
# Coll), the reflector's own hurt sphere, which ft/ftmain.c reads by kind.
# The port never loads that file, so src/dc/ftcommon.c carries the decomp's
# own C copy of the sphere (relocData/208_FoxMainMotion.c) and binds the
# file base to it at every overlay load, the step-50 pattern. The port's
# stub ftFoxSpecialLwHitSetStatus (ftmain.c's reflect dispatch has named
# it) is gone; the real one is here. Every other callee
# (ftCommonTurnCheckInputSuccess, ftParamsUpdateFighterPartsTransformAll,
# ftPhysicsApplyGravityClampTVel/CheckClampAirVelXDecMax/ApplyAirVelX-
# Friction, mpCommonSetFighterWaitOrFall/FallOnEdgeBreak, mpCommonProc-
# FighterWaitOrLanding, ftParamProcStopEffect, ftMainSetStatus/PlayAnim-
# EventsAll) was already in the port.
ftfoxspeciallw.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftfox/ftfoxspeciallw.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Fox's Up-B, Fire Fox (ft/ftchar/ftfox/ftfoxspecialhi.c,
# overlay 3, compiled unmodified). Nine statuses (227-235), a ground and an
# air half of Start/Hold/travel/End plus the aerial-only Bound, thirty procs.
# No weapon, no it/, no effect: the fire is the motion's own script. Two
# leaves were missing and are now in the port: lbCommonCheckAdjustSim2D
# (src/dc/lbcommon.c, the graze test that rotates Fox's velocity onto a
# surface he clips at under 20 degrees instead of ending the move) and
# mpCommonCheckFighterPass (src/dc/mpcommon.c, the pass walk without the
# ledge half, so a travelling Fox drops through a platform for fifteen tics
# and grabs no cliff). Everything else (ftAnimEndCheckSetStatus/SetWait,
# ftMainSetStatus/PlayAnimEventsAll, ftPhysicsApplyGravityClampTVel/
# CheckClampAirVelXDecMax/ApplyAirVelXFriction/ApplyGroundVelFriction/
# SetGroundVelFriction/SetGroundVelTransferAir/ApplyAirVelDrift/
# ApplyAirVelTransNYZ/ClampAirVelXMax, ftParamSetStickLR,
# ftParamsUpdateFighterPartsTransformAll, ftCommonFallSpecialSetStatus,
# ftCommonWaitSetStatus, ftCommonCliffCatchSetStatus, mpCommonProcFighter-
# OnFloor/Landing/Cliff, mpCommonSetFighterAir/Ground/FallOnGroundBreak,
# mpCommonCheckFighterCliff, syUtilsArcTan2, syVectorAngleDiff3D) was
# already here.
ftfoxspecialhi.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftfox/ftfoxspecialhi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Mario's Neutral-B status procs (ftmariospecialn.c, overlay 3,
# compiled unmodified from the decomp). Its ProcAccessory is the fireball's
# caller -- it reads joints[FTMARIO_FIREBALL_SPAWN_JOINT]'s world position and
# calls the step-25 wpMarioFireballMakeWeapon. Every callee (mpCommon*,
# ftMainSetStatus, ftPhysics*, gmCollisionGetFighterPartsWorldPosition) was
# already in the port; what was missing was the file itself. Dead until a
# Mario status table names these procs -- the same "code first, table later"
# the ftpurin/ftkirby/ftmariospeciallw specials above stand as.
ftmariospecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftmario/ftmariospecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Mario's Up-B (SpecialHi, the Super Jump Punch) status procs
# (ftmariospecialhi.c, overlay 3, compiled unmodified from the decomp). This
# is the last of Mario's three special-move files -- specialn and
# speciallw are separate files -- so the dFTMarioSpecialStatusDescs table is
# assembled from all three. Its SetStatus entry points (ftMarioSpecialHiSetStatus /
# ftMarioSpecialAirHiSetStatus) reach only ftMainSetStatus/PlayAnimEventsAll,
# already ported; its Proc{Update,Map} reach ftCommonFallSpecial/Landing
# commons not yet in the build, but those are only linked once a status table
# names these procs. Function-level GC keeps them out of the ELF until then,
# exactly as specialn's procs stand above. It is in
# HOST_DECOMP_SRCS + FTCHAR_SPECIAL_OBJS (host+target): the whole callee
# closure -- ftCommonFallSpecial (promoted alongside), the cliffcatch/landing/
# wait setters, ftCommonJumpAerialCheckInterruptCommon, the ftPhysics*/mpCommon*
# leaves -- is now in the host build, so the host link resolves it even with
# no --gc-sections, and the Up-B rows go live in dFTMarioSpecialStatusDescs.
ftmariospecialhi.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftmario/ftmariospecialhi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# ftCommonFallSpecial, the first of the ftcommon closure the
# Mario special-move status table pulls in (ft/ftcommon/ftcommonfallspecial.c,
# overlay 3, compiled unmodified). The three specials' ProcMap route to
# ftCommonFallSpecialSetStatus (helpless-fall after Up-B etc.), so this common
# owns the recovery-fall state: ProcPhysics runs the gravity + air-drift maths,
# ProcMap does the cliff-catch / landing / wait handoff, ProcPass the
# pass-through-platform test. Every ftPhysics* callee is already in the port;
# the ftcommon handoffs (jumpaerial's interrupt tree, cliffcatch, landing,
# wait) are the remaining closure. Function-level GC keeps
# this whole file out of the ELF until the status table names one of its procs.
# It is in HOST_DECOMP_SRCS + FTCOMMON_STATUS_OBJS
# (host+target): with the closure complete the host link resolves every callee
# even without --gc-sections, so ftmariospecialhi (which reaches
# ftCommonFallSpecialSetStatus) can go host too. ProcPhysics is now host-tested.
ftcommonfallspecial.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonfallspecial.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# the Down-B demux, whole -- every fighter's setter it names
# is compiled since Link's Bomb, so the port's NULL-guarded copy is gone.
ftcommonspeciallw.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonspeciallw.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommondokan.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommondokan.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonjumpaerial.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonjumpaerial.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonsquat.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonsquat.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonfall.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonfall.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonpass.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonpass.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# The item-use step: the whole fighter-side item family, compiled
# unmodified. ftcommonget.c is the pickup (LightGet/HeavyGet, LiftWait and
# LiftTurn for a heavy item, and the get-check the attack cascades have
# once called into a FALSE stub); ftcommonitemthrow.c the
# drop and the twenty-two throws; ftcommonitemswing.c the Bat, Sword, Fan
# and Star Rod swings; ftcommonitemshoot.c the Ray Gun and Fire Flower.
# ft/fthammer.c is ovl2 like gm/gmcollision.c and ft/ftdisplaymain.c beside
# it in the map (smashbrothers.us.yaml:415); its five ftcommonhammer*.c
# status files are ovl3 with the rest.
ftcommonget.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonget.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonitemthrow.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonitemthrow.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonitemswing.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonitemswing.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonitemshoot.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonitemshoot.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonhammerfall.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonhammerfall.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonhammerkneebend.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonhammerkneebend.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonhammerlanding.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonhammerlanding.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonhammerturn.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonhammerturn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonhammerwalk.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonhammerwalk.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
fthammer.o: $(SSB_DECOMP_DIR)/src/ft/fthammer.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
# Mario's fireball. ftmario/ftluigi are ovl2 (their file-base
# globals), wpmariofireball is ovl3 (the projectile itself).
ftmario.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftmario/ftmario.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
ftluigi.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftluigi/ftluigi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
wpmariofireball.o: $(SSB_DECOMP_DIR)/src/wp/wpmario/wpmariofireball.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Fox's Blaster. ftfox is ovl2 (its file-base globals),
# wpfoxblaster is ovl3 (the shot itself) -- the same split as ftmario/
# ftluigi and wpmariofireball above.
ftfox.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftfox/ftfox.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
wpfoxblaster.o: $(SSB_DECOMP_DIR)/src/wp/wpfox/wpfoxblaster.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# ftpurin.c, ovl2, the same bss-only file-base-pointer shape as
# ftmario/ftluigi/ftfox above -- gFTDataPurinSpecial2 is the one Sing's
# EFDesc needs.
ftpurin.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftpurin/ftpurin.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
# ftsamus.c, ovl2, the same bss-only file-base-pointer shape as
# ftmario/ftluigi/ftfox/ftpurin above -- gFTDataSamusMain is the one
# wpsamusbomb.c's own WPDesc needs. wpsamusbomb is ovl3 (the bomb itself).
ftsamus.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftsamus/ftsamus.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
wpsamusbomb.o: $(SSB_DECOMP_DIR)/src/wp/wpsamus/wpsamusbomb.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# ftyoshi.c, ovl2, the same bss-only file-base-pointer shape as
# ftmario/ftluigi/ftfox/ftpurin/ftsamus above -- gFTDataYoshiMain is the one
# wpyoshistar.c's own WPDesc needs. It also defines gFTDataYoshiModel, which
# src/dc/efmanager.c once stood in for with a stub of its own
# (Yoshi's egg shield); the real file defines it. wpyoshistar is ovl3 (the
# stars themselves): two per
# landing, one each way, and unmodeled on target the same way Samus's Bomb
# is -- no sWPManagerModels row yet, so wpManagerMakeWeapon returns NULL
# and wpYoshiStarMakeStars's own NULL-tolerant shape carries on.
ftyoshi.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftyoshi/ftyoshi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
# ftkirby.c, ovl2, the same bss-only file-base-pointer shape as
# the six above -- gFTDataKirbyMain is the one wpkirbycutter.c's own WPDesc
# needs. It also defines gFTDataKirbySpecial2, which the four cutter effects
# would read if they were not stubbed, and gFTDataKirbyParticleBankID, which
# no per-fighter particle bank fills yet (see the Yoshi egg's explode note in
# src/dc/ftcommon.c -- Kirby's is the second such bank to go unloaded).
ftkirby.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirby.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
wpyoshistar.o: $(SSB_DECOMP_DIR)/src/wp/wpyoshi/wpyoshistar.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Yoshi's Up-B (Egg Throw), his LAST special file -- Yoshi Bomb
# and Egg Lay are separate files, so this finishes his
# table and makes him the seventh table-complete fighter. Two statuses (222
# SpecialHi, 223 SpecialAirHi), fifteen procs, compiled unmodified, and the
# thrown egg is wp/wpyoshi/wpyoshieggthrow.c beside it (ovl3, thirteen procs).
# A callee sweep over the pair found nothing missing at all beyond each
# other: syVectorRotateAbout3D (sys/vector.c, compiled unmodified),
# syUtilsArcTan2, wpMainDecLifeCheckExpire, wpManagerMakeWeapon/RemoveWeapon,
# efManagerDustExpandSmallMakeEffect and the rest were in already. Its WPDesc
# names gFTDataYoshiMain (ftyoshi.c, in FTCHAR_DATA_OBJS).
# wpYoshiEggThrowProcDisplay is the one gDP display proc in the pair: the egg
# is modeled now (romdisk/wpyoshieggthrow.mdl), so it runs, and its commands
# land in src/dc/wpmanager.c's gSYTaskmanDLHeads scratch, reset every pass.
ftyoshispecialhi.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftyoshi/ftyoshispecialhi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
wpyoshieggthrow.o: $(SSB_DECOMP_DIR)/src/wp/wpyoshi/wpyoshieggthrow.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Kirby's Up-B (Final Cutter), the first of his two holes --
# ftkirbyspeciallw.c (the Stone) is compiled and has its
# rows in the aerial demux, so this is his second special and leaves
# only the copy inhale. Four statuses (256 SpecialHi, 257 SpecialHiLanding,
# 258 SpecialAirHi, 259 SpecialAirHiFall), and the blade is
# wp/wpkirby/wpkirbycutter.c beside it (ovl3). The callee sweep over the
# pair came back with nothing missing but each other and FOUR EFFECTS --
# efManagerKirbyCutter{Up,Down,Draw,Trail}MakeEffect, all model-path makers
# reading DObjDescs out of gFTDataKirbySpecial2, a file the port has never
# baked. They are stubbed in src/dc/ftcommon.c beside Yoshi's egg pair; see
# the DIVERGES there for what that costs. The two physics leaves the sweep
# also named (ftPhysicsApplyAirVelTransNYZ, ftPhysicsCheckClampAirVelXDecMax)
# were false alarms -- both are in ft/ftphysics.c, compiled unmodified.
# The cutter blade is unmodeled like Samus's Bomb and both Yoshi eggs.
ftkirbyspecialhi.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirbyspecialhi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Kirby's Neutral-B, the copy inhale -- the last hole in any
# fighter's status table. Three files, all compiled unmodified, and the
# callee sweep is why it is three lines here rather than a hand-port:
# ftkirbyspecialn.c (976 lines, 92 procs, statuses 269-286) named seven
# symbols the port did not have, and three of those were the sweep
# over-reporting again (ftParamSetModelPartDefaultID is in ft/ftparam.c,
# ftPhysicsApplyFastFall and ftPhysicsCheckClampAirVelXDecMax in
# ft/ftphysics.c, all compiled). Two more were the swallowed
# fighter's side, ft/ftcommon/ftcommoncapturekirby.c (549 lines, the four
# common rows 173-176 in src/dc/ftcommon.c), and that file in turn wanted
# two knockback helpers out of ft/ftcommon/ftcommoncapture.c -- 159 lines
# whose own sweep came back EMPTY, the cleanest leaf the port has added.
# The last two were effects: three are stubbed in src/dc/ftcommon.c (the
# inhale's wind and the two head stars) and the fourth, the spit's star
# splash, was taken verbatim into src/dc/efmanager.c because it is
# common-bank-only. No wp/ and no it/ file at all -- Kirby's star is a
# STATUS on the victim, not a weapon, which is what kept this bounded.
#
# What is NOT here is the copy half: dFTKirbySpecialNStatusList in
# src/dc/ftcommon.c has Kirby's own rows live and the eight remaining
# ftkirbycopy*specialn.c files NULL. Purin's is live, compiled
# unmodified.
ftkirbyspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirbyspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Kirby's copy half -- the six ftkirbycopy*specialn.c files
# whose original fighter the port already has. Every one compiles
# unmodified, and the callee sweep over all nine remaining copy files came
# back with exactly four MISSING symbols, three of which are these six
# files' boundary: wpPikachuThunderJoltAirMakeWeapon, wpNessPKFireMake-
# Weapon and wpLinkBoomerangMakeWeapon are not in the build, so Pikachu's,
# Ness's and Link's copy files stay out; the fourth,
# wpSamusChargeShotMakeWeapon, was the sweep over-reporting (wp/wpsamus/
# wpsamuschargeshot.c is compiled). None of the six
# names a single global -- `grep -oE '\b[gd][A-Z][A-Za-z0-9_]*'` over all
# of them is EMPTY -- so they are pure leaves on the port's own ft/mp/wp
# functions plus each other.
#
# Mario's file drives FOUR statuses, not two: Luigi has no copy file of his
# own, and FTKIRBY_COPYMARIO_FIREBALL_CHECK_FTKIND picks CopyLuigiSpecialN
# over CopyMarioSpecialN from fp->passive_vars.kirby.copy_id. That is why
# dFTKirbySpecialNStatusList slots 0 (Mario), 4 (Luigi) and 13 (MMario) all
# point at the same setter.
ftkirbycopymariospecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirbycopymariospecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftkirbycopyfoxspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirbycopyfoxspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftkirbycopysamusspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirbycopysamusspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftkirbycopydonkeyspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirbycopydonkeyspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftkirbycopycaptainspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirbycopycaptainspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftkirbycopyyoshispecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirbycopyyoshispecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Link's boomerang, his FIRST special table, and Kirby's copy
# of it. Four decomp files, all compiled unmodified:
#
#   ftlink.o (ovl2, FTCHAR_DATA_OBJS)  the seven bss file-base pointers,
#     the same shape as ftmario/ftfox/ftsamus/ftkirby. Only
#     gFTDataLinkSpecial1 is read -- dWPLinkBoomerangWeaponDesc names it.
#   ftlinkspecialn.o (ovl3)                              Link's Neutral-B.
#   ftlinkspecialhi.o (ovl3)                             his Up-B, the
#       Spin Attack.
#   wplinkboomerang.o (ovl3)                             the boomerang itself.
#   wplinkspinattack.o (ovl3)                            the Spin Attack's
#       spinning hitbox.
#   ftkirbycopylinkspecialn.o (ovl3)   Kirby's copy, a mirror of the above.
#
# The callee sweep over the four came back with exactly one MISSING symbol
# that stayed missing: efManagerDustCollideMakeEffect, which is NOT stubbed
# -- it is ported verbatim into src/dc/efmanager.c, because its particle
# script (0x55, gEFManagerParticleBankID) is the one
# efManagerDustExpandSmallMakeEffect draws.
# The sweep's other three hits were the step's own additions:
# wpLinkBoomerangMakeWeapon, ftLinkSpecialNGetSetStatus and
# ftKirbyCopyLinkSpecialNGetSetStatus (the boomerang calls back into both
# owners' files, which is why the pair cannot be split across two steps).
# The globals sweep found gFTDataLinkSpecial1, dWPLinkBoomerangWeaponDesc
# (both defined by the new files) and gGMCameraGObj/Matrix/Struct, which
# src/dc/gmcamera.c has had.
#
# Unmodeled on target like every weapon since Samus's Bomb: no
# sWPManagerModels row, so wpManagerMakeWeapon returns NULL and
# ftLinkSpecialNMakeBoomerang stores that NULL -- which the decomp's own
# code reads as "no boomerang out", the empty-hand path. Its attributes
# symbol llLinkSpecial1BoomerangWeaponAttributes is a stand-in in
# src/dc/wpmanager.c.
ftlink.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftlink/ftlink.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
ftlinkspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftlink/ftlinkspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
wplinkboomerang.o: $(SSB_DECOMP_DIR)/src/wp/wplink/wplinkboomerang.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftlinkspecialhi.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftlink/ftlinkspecialhi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftdonkeythrowfwait.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftdonkey/ftdonkeythrowfwait.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftdonkeythrowfwalk.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftdonkey/ftdonkeythrowfwalk.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftdonkeythrowfturn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftdonkey/ftdonkeythrowfturn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftdonkeythrowfkneebend.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftdonkey/ftdonkeythrowfkneebend.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftdonkeythrowffall.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftdonkey/ftdonkeythrowffall.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftdonkeythrowflanding.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftdonkey/ftdonkeythrowflanding.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftdonkeythrowfdamage.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftdonkey/ftdonkeythrowfdamage.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftdonkeythrowff.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftdonkey/ftdonkeythrowff.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftkirbythrowf.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirbythrowf.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommonattack100.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommonattack100.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
wplinkspinattack.o: $(SSB_DECOMP_DIR)/src/wp/wplink/wplinkspinattack.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftkirbycopylinkspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirbycopylinkspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommoncapturekirby.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommoncapturekirby.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftcommoncapture.o: $(SSB_DECOMP_DIR)/src/ft/ftcommon/ftcommoncapture.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
wpkirbycutter.o: $(SSB_DECOMP_DIR)/src/wp/wpkirby/wpkirbycutter.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# the Charge Shot (ovl3). Its WPDesc names gFTDataSamusSpecial1,
# which ftsamus.c (in FTCHAR_DATA_OBJS) defines, and
# llSamusSpecial1ChargeShotWeaponAttributes, a stand-in in src/dc/wpmanager.c.
# Unmodeled on target the same way Samus's Bomb and Yoshi's stars are (no
# sWPManagerModels row yet), so wpManagerMakeWeapon returns NULL there and
# every caller in ftsamusspecialn.c already handles that the decomp's way.
wpsamuschargeshot.o: $(SSB_DECOMP_DIR)/src/wp/wpsamus/wpsamuschargeshot.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Pikachu's Thunder Jolt -- his Neutral-B, and with it his
# first status table. Four files, every one compiled UNMODIFIED:
#
#   ftpikachu.o (ovl2)                 the loaded-file base pointers, the
#                                      eighth FTCHAR_DATA_OBJS member. The
#                                      jolt's two WPDescs name
#                                      gFTDataPikachuSpecial1 and the
#                                      effect's EFDesc gFTDataPikachuSpecial3.
#   ftpikachuspecialn.o (ovl3)         seven procs, 101 lines.
#   wppikachuthunderjolt.o (ovl3)      the jolt itself, 751 lines and by far
#                                      the largest weapon file the port has
#                                      taken -- two weapons in one, the
#                                      airborne ball and the ground crawler
#                                      it becomes on contact.
#   ftkirbycopypikachuspecialn.o(ovl3) Kirby's ninth copy mouth, a mirror of
#                                      ftpikachuspecialn.c that calls the
#                                      same wpPikachuThunderJoltAirMakeWeapon.
#
# The callee sweep over the four came back with twenty MISSING and
# NINETEEN of them were the sweep over-reporting in its first known way
# (it greps src/dc/*.c only): seventeen mpCollision edge and line queries
# live in mp/mpcollision.c, compiled unmodified into this port,
# and syVectorRotateAbout3D and gcSetAllAnimSpeed are sys/vector.c's and
# sys/objanim.c's, unmodified too. The jolt crawls along the
# stage's own geometry -- floor to wall to floor, over ledges and around
# corners -- and every question it asks of the map was already answered.
# The ONE real hit was efManagerPikachuThunderJoltMakeEffect, ported
# verbatim into src/dc/efmanager.c with its EFDesc.
#
# The globals sweep found four: the file's own two WPDescs, and
# gFTDataPikachuSpecial1/3, which ftpikachu.o above defines.
#
# Unmodeled on target like every weapon since Samus's Bomb: no
# sWPManagerModels row for either WPDesc, so wpManagerMakeWeapon returns
# NULL and ftPikachuSpecialNProcAccessory stores nothing -- the decomp's
# own no-projectile path. Both attribute symbols
# (llPikachuSpecial1ThunderJolt{Air,Ground}WeaponAttributes) and the two
# ground-animation offsets (llPikachuSpecial3ThunderJoltB{,Mat}AnimJoint)
# are stand-ins: the attributes in wpattrs.ld, the animation offsets as
# arrays of NULLs in src/dc/wpattrs.c.
ftpikachu.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftpikachu/ftpikachu.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
# ftdonkey.c joins FTCHAR_DATA_OBJS for gFTDataDonkeySpecial2,
# the file dEFManagerDonkeyEntryTaruEffectDesc points its reloc offsets
# into -- the same reason ftpurin.o is linked for Sing.
ftdonkey.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftdonkey/ftdonkey.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
ftpikachuspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftpikachu/ftpikachuspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
wppikachuthunderjolt.o: $(SSB_DECOMP_DIR)/src/wp/wppikachu/wppikachuthunderjolt.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftkirbycopypikachuspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirbycopypikachuspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Pikachu's Thunder, his Down-B. Two more files, both
# compiled unmodified: ftpikachuspeciallw.c (444 lines, thirty procs) and
# wp/wppikachu/wppikachuthunder.c (238 lines, TWO weapons -- the bolt's
# head, which is not an attack at all, and the trail segments behind it,
# which are). The callee sweep over the pair came back with four MISSING
# and three were it over-reporting again: ftPhysicsCheckClampAirVelXDecMax
# is ft/ftphysics.c's, compiled unmodified. The fourth,
# wpPikachuThunderHeadMakeWeapon, is the pair's own -- the fighter file
# calls into the weapon file, which is why the two cannot be split.
# efManagerPikachuThunderTrailMakeEffect and its two procs are ported
# verbatim into src/dc/efmanager.c with their EFDesc.
#
# Unmodeled on target like every weapon since Samus's Bomb: no
# sWPManagerModels row for either WPDesc, so wpManagerMakeWeapon returns
# NULL and ftPikachuSpecialLwMakeThunder stores that NULL -- which the
# decomp's own ftPikachuSpecialLwCheckCollideThunder reads as
# is_thunder_destroy, the path where the bolt never arrived. Both
# attribute symbols (llPikachuMainThunder{Head,Trail}WeaponAttributes)
# are stand-ins in src/dc/wpmanager.c.
ftpikachuspeciallw.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftpikachu/ftpikachuspeciallw.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
wppikachuthunder.o: $(SSB_DECOMP_DIR)/src/wp/wppikachu/wppikachuthunder.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Pikachu's Agility, his Up-B, and the last special move any
# playable fighter was missing that does not wait on it/. One file,
# compiled unmodified: ftpikachuspecialhi.c, 557 lines and thirty-one
# procs. The cleanest ftchar file this port has taken -- the callee sweep
# came back with three MISSING and all three were ft/ftphysics.c's,
# compiled unmodified; the globals sweep and the reloc sweep both
# came back EMPTY. No weapon, no effect, no asset: the Quick Attack is two
# teleports and a model squash, and every leaf it needs was already here.
ftpikachuspecialhi.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftpikachu/ftpikachuspecialhi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Ness's specials -- PK Fire, PK Thunder, PSI Magnet --
# and Kirby's copied PK Fire, all compiled unmodified. ftness.c is ovl2
# (the file-base globals and gFTNessParticleBankID); the rest are ovl3.
# itnesspkfire.c, PK Fire's pillar, is the first it/ file the port builds
# unmodified; it rides WPCHAR_OBJS for the host's ovl3 rename, since the
# spark that spawns it is the only thing that can.
ftness.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftness/ftness.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
ftnessspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftness/ftnessspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftnessspecialhi.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftness/ftnessspecialhi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftnessspeciallw.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftness/ftnessspeciallw.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Master Hand's spine, ovl3 -- the same segment the
# fighter specials above come out of (smashbrothers.us.yaml puts
# ftbosscommon at 0x0D29A0, in the block that starts with
# ftmariospecialn at 0x0D0880). Compiled unmodified and dead-stripped
# until dFTBossSpecialStatusDescs names its procs.
#
# BOSS_DECL_CFLAGS is the one thing these need that the specials above
# did not. ft/ftchar/ftboss/*.c include only <ft/fighter.h>, and three
# of the functions they call are declared elsewhere in the decomp's own
# tree -- syDebugPrintf in sys/debug.h, scManagerRunPrintGObjStatus in
# sc/scmanager.h, sc1PGameBossDefeatInitInterface in
# sc/sc1pmode/sc1pgame.h. All three exist in the port; what is missing
# is only the declaration, so without this each gets an implicit `int
# f()` and compiles to a warning. It is benign on SH4 here (every one
# is called for effect, with pointer or no arguments) but an implicit
# declaration is exactly the trap that hides a REAL missing function,
# so the declarations are forced in rather than the warnings accepted.
# ft/fighter.h leads because sc1pgame.h names FTStruct.
BOSS_DECL_CFLAGS = -include ft/fighter.h -include sys/debug.h \
                   -include sc/scmanager.h -include sc/sc1pmode/sc1pgame.h \
                   -include wp/weapon.h
# One rule apiece, spelled out: tools/check/standin_check.py reads this
# Makefile statically -- every object in OBJS must have a rule whose
# text names its source -- so a $(foreach)/$(eval) template would
# link objects the census cannot account for.
ftbosscommon.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbosscommon.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbosswait.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbosswait.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossappear.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossappear.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossdeadcenter.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossdeadcenter.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossdeadleft.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossdeadleft.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossdeadright.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossdeadright.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossdefault.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossdefault.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossdrill.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossdrill.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossgootsubusudown.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossgootsubusudown.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossgootsubusuend.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossgootsubusuend.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossgootsubusuup.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossgootsubusuup.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossgootsubusuwait.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossgootsubusuwait.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossharau.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossharau.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbosshippataku.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbosshippataku.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossmove.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossmove.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossokuhikouki1.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossokuhikouki1.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossokuhikouki2.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossokuhikouki2.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossokuhikouki3.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossokuhikouki3.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossokukouki.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossokukouki.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossokupunch1.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossokupunch1.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossokupunch2.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossokupunch2.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossokupunch3.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossokupunch3.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossokutsubushi.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossokutsubushi.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossokutsubushistart.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossokutsubushistart.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbosstsutsuku1.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbosstsutsuku1.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbosstsutsuku2.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbosstsutsuku2.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbosstsutsuku3.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbosstsutsuku3.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbosswalk.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbosswalk.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbosswalkloop.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbosswalkloop.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbosswalkshoot.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbosswalkshoot.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbosswalkwait.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbosswalkwait.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossyubideppou1.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossyubideppou1.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossyubideppou2.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossyubideppou2.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
ftbossyubideppou3.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftbossyubideppou3.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(BOSS_DECL_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# The bullet itself, ovl3 beside the motion that throws
# it. -include ftcommon.h is the port's own header, not a decomp one:
# func_800269C0_275C0 -- n_env.c's "start a voice script" -- is
# declared there and in no decomp header, which is why eight other
# decomp files here still compile it implicitly. Forced in for this
# one so the new file lands clean; the other eight are their own
# cleanup.
wpbossbullet.o: $(SSB_DECOMP_DIR)/src/wp/wpboss/wpbossbullet.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -include ftcommon.h $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)

# His four loaded-file base pointers, ovl2 bss, beside every other
# fighter's in FTCHAR_DATA_OBJS.
ftboss.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftboss/ftboss.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,2,$@)
ftkirbycopynessspecialn.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftkirby/ftkirbycopynessspecialn.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
wpnesspkfire.o: $(SSB_DECOMP_DIR)/src/wp/wpness/wpnesspkfire.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
wpnesspkthunder.o: $(SSB_DECOMP_DIR)/src/wp/wpness/wpnesspkthunder.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
itnesspkfire.o: $(SSB_DECOMP_DIR)/src/it/itfighter/itnesspkfire.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# Link's Bomb, his Down-B, and the item it pulls out -- both
# compiled unmodified, the second fighter item after PK Fire's pillar.
ftlinkspeciallw.o: $(SSB_DECOMP_DIR)/src/ft/ftchar/ftlink/ftlinkspeciallw.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
itlinkbomb.o: $(SSB_DECOMP_DIR)/src/it/itfighter/itlinkbomb.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
# ft/ftpublic.c -- the crowd and the announcer, compiled unmodified. It is
# the first file the port compiles out of overlay 3, and it is one of only
# four in that overlay with a .bss at all (smashbrothers.us.yaml's ovl3
# bss list: ftcommonthrown2, ftpublic, wpmanager, itmanager), so it is
# what gives OVERLAY_FIGHTING a real segment instead of a NULL pair.
ftpublic.o: $(SSB_DECOMP_DIR)/src/ft/ftpublic.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) \
	    -include syaudio.h -c $< -o $@.tmp
	$(call ovl-noload,3,$@)
fighter.o: ../../dc/fighter.c ../../dc/fighter.h ../../dc/mtx.h \
            ../../dc/clip.h ../../dc/assetroot.h
	kos-cc $(KOS_CFLAGS) -c ../../dc/fighter.c -o fighter.o
input.o: ../../dc/input.c ../../dc/input.h
	kos-cc $(KOS_CFLAGS) -c ../../dc/input.c -o input.o
stage.o: ../../dc/stage.c ../../dc/stage.h ../../dc/grwallpaper.h ../../dc/fighter.h \
         ../../dc/assetroot.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/stage.c -o stage.o
# A bank of sprites as tools/export/ssb_spriteexport.py writes one: the port's
# side of lbRelocLoadFilesListed for a scene's sprite files.
sprite.o: ../../dc/sprite.c ../../dc/sprite.h ../../dc/taskman.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/sprite.c -o sprite.o
# The camera-anim banks: the same shape sprite.o has -- an
# asset loader, always resident, in no overlay -- for the AObjEvent32
# scripts sys/objanim.c's gcPlayCamAnim runs on a CObj.
camanim.o: ../../dc/camanim.c ../../dc/camanim.h ../../dc/taskman.h \
           ../../dc/assetroot.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/camanim.c -o camanim.o
# The title screen, mn/mncommon/mntitle.c reduced to its sprites.
mntitle.o: ../../dc/mntitle.c ../../dc/mntitle.h ../../dc/lbcommon.h \
           ../../dc/sprite.h ../../dc/scmanager.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mntitle.c -o mntitle.o
# The mode select, mn/mncommon/mnmodeselect.c verbatim.
mnmodeselect.o: ../../dc/mnmodeselect.c ../../dc/mnmodeselect.h ../../dc/lbcommon.h \
                ../../dc/sprite.h ../../dc/scmanager.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnmodeselect.c -o mnmodeselect.o
# The 1P submenu, mn/mn1pmode/mn1pmode.c verbatim.
mn1pmode.o: ../../dc/mn1pmode.c ../../dc/mn1pmode.h ../../dc/lbcommon.h \
            ../../dc/sprite.h ../../dc/scmanager.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mn1pmode.c -o mn1pmode.o
# The VS mode, mn/mnvsmode/mnvsmode.c verbatim.
mnvsmode.o: ../../dc/mnvsmode.c ../../dc/mnvsmode.h ../../dc/lbcommon.h \
            ../../dc/sprite.h ../../dc/scmanager.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnvsmode.c -o mnvsmode.o
# The VS options, mn/mnvsmode/mnvsoptions.c verbatim.
mnvsoptions.o: ../../dc/mnvsoptions.c ../../dc/mnvsoptions.h ../../dc/lbcommon.h \
               ../../dc/sprite.h ../../dc/scmanager.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnvsoptions.c -o mnvsoptions.o
# The character select, mn/mnplayers/mnplayersvs.c verbatim.
mnplayersvs.o: ../../dc/mnplayersvs.c ../../dc/mnplayersvs.h ../../dc/lbcommon.h \
               ../../dc/sprite.h ../../dc/scmanager.h ../../dc/ftcommon.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnplayersvs.c -o mnplayersvs.o
# Training mode's select, mn/mnplayers/mnplayers1ptraining.c.
mnplayers1ptraining.o: ../../dc/mnplayers1ptraining.c ../../dc/mnplayers1ptraining.h \
                       ../../dc/lbcommon.h ../../dc/sprite.h ../../dc/scmanager.h \
                       ../../dc/ftcommon.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnplayers1ptraining.c -o mnplayers1ptraining.o
# The 1P game's select, mn/mnplayers/mnplayers1pgame.c.
mnplayers1pgame.o: ../../dc/mnplayers1pgame.c ../../dc/mnplayers1pgame.h \
                   ../../dc/lbcommon.h ../../dc/sprite.h ../../dc/scmanager.h \
                   ../../dc/ftcommon.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnplayers1pgame.c -o mnplayers1pgame.o
# The bonus stages' practice select, mn/mnplayers/mnplayers1pbonus.c.
# One scene for both bonus games.
mnplayers1pbonus.o: ../../dc/mnplayers1pbonus.c ../../dc/mnplayers1pbonus.h \
                   ../../dc/lbcommon.h ../../dc/sprite.h ../../dc/scmanager.h \
                   ../../dc/ftcommon.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnplayers1pbonus.c -o mnplayers1pbonus.o
# "CHALLENGER APPROACHING!", sc/sc1pmode/sc1pchallenger.c.
sc1pchallenger.o: ../../dc/sc1pchallenger.c ../../dc/sc1pchallenger.h \
                  ../../dc/lbcommon.h ../../dc/sprite.h \
                  ../../dc/scmanager.h ../../dc/ftcommon.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/sc1pchallenger.c -o sc1pchallenger.o
# The 1P ladder's VS card, sc/sc1pmode/sc1pintro.c.
sc1pintro.o: ../../dc/sc1pintro.c ../../dc/sc1pintro.h ../../dc/introcrowd.h \
             ../../dc/lbcommon.h ../../dc/sprite.h ../../dc/camanim.h \
             ../../dc/scmanager.h ../../dc/ftcommon.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/sc1pintro.c -o sc1pintro.o
# The 1P ladder's score screen, sc/sc1pmode/sc1pstageclear.c.
sc1pstageclear.o: ../../dc/sc1pstageclear.c ../../dc/sc1pstageclear.h \
                  ../../dc/lbcommon.h ../../dc/sprite.h ../../dc/dcpvr.h \
                  ../../dc/scmanager.h ../../dc/ftcommon.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/sc1pstageclear.c -o sc1pstageclear.o
# The Continue prompt, mn/mn1pmode/mn1pcontinue.c. The
# router calls it when the human loses a rung; it is a scene of its own,
# not part of sc1pmanager.o.
mn1pcontinue.o: ../../dc/mn1pcontinue.c ../../dc/mn1pcontinue.h \
                ../../dc/lbcommon.h ../../dc/sprite.h ../../dc/bgm.h \
                ../../dc/fighter.h ../../dc/objmodel.h ../../dc/efmanager.h \
                ../../dc/scmanager.h ../../dc/ftcommon.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mn1pcontinue.c -o mn1pcontinue.o
# Final Destination's animated background, sc/sc1pmode/sc1pgameboss.c.
# Its five trees come out of the stage pack's BWP1 block.
sc1pgameboss.o: ../../dc/sc1pgameboss.c ../../dc/sc1pgameboss.h \
                ../../dc/stage.h ../../dc/objmodel.h ../../dc/fighter.h \
                ../../dc/lbcommon.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/sc1pgameboss.c -o sc1pgameboss.o
# The 1P ladder's router, sc/sc1pmode/sc1pmanager.c. Not a
# scene: scmanager.c's nSCKind1PGame arm calls it and it runs a whole run
# of the ladder inline. It is in overlay 2 and holds the run's totals.
sc1pmanager.o: ../../dc/sc1pmanager.c ../../dc/sc1pmanager.h \
               ../../dc/mn1pcontinue.h ../../dc/mnmessage.h \
               ../../dc/sc1pintro.h ../../dc/sc1pgame.h \
               ../../dc/sc1pstageclear.h ../../dc/sc1pchallenger.h \
               ../../dc/mvending.h ../../dc/scstaffroll.h \
               ../../dc/mncongra.h \
               ../../dc/overlay.h ../../dc/scmanager.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/sc1pmanager.c -o sc1pmanager.o
# The 1P ladder's battle scene, sc/sc1pmode/sc1pgame.c.
sc1pgame.o: ../../dc/sc1pgame.c ../../dc/sc1pgame.h \
            ../../dc/lbcommon.h ../../dc/sprite.h ../../dc/gmcommon.h \
            ../../dc/scmanager.h ../../dc/ftcommon.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/sc1pgame.c -o sc1pgame.o
# The bonus stages, sc/sc1pmode/sc1pbonusstage.c + sc1pbonusstagefiles.c
# (the Break the Targets half).
sc1pbonusstage.o: ../../dc/sc1pbonusstage.c ../../dc/sc1pbonusstage.h \
                  $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/sc1pbonusstage.c -o sc1pbonusstage.o
# Training mode itself, sc/sc1pmode/sc1ptrainingmode.c.
sc1ptrainingmode.o: ../../dc/sc1ptrainingmode.c ../../dc/sc1ptrainingmode.h \
                    $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/sc1ptrainingmode.c -o sc1ptrainingmode.o
# mn/mnmaps/mnmaps.c, the stage select (src/dc/mnmaps.c)
mnmaps.o: ../../dc/mnmaps.c ../../dc/mnmaps.h ../../dc/lbcommon.h \
          ../../dc/sprite.h ../../dc/scmanager.h ../../dc/stage.h \
          ../../dc/objmodel.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnmaps.c -o mnmaps.o
# mn/mncommon/mnmessage.c, the unlock message (src/dc/mnmessage.c)
mnmessage.o: ../../dc/mnmessage.c ../../dc/mnmessage.h ../../dc/lbcommon.h \
             ../../dc/sprite.h ../../dc/scmanager.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnmessage.c -o mnmessage.o
# mn/mndata/mndata.c, the DATA menu (src/dc/mndata.c)
mndata.o: ../../dc/mndata.c ../../dc/mndata.h ../../dc/lbcommon.h \
          ../../dc/sprite.h ../../dc/scmanager.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mndata.c -o mndata.o
# mn/mndata/mnvsrecord.c, the Records tab (src/dc/mnvsrecord.c)
mnvsrecord.o: ../../dc/mnvsrecord.c ../../dc/mnvsrecord.h ../../dc/lbcommon.h \
              ../../dc/sprite.h ../../dc/scmanager.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnvsrecord.c -o mnvsrecord.o
# mn/mndata/mncharacters.c, the Character Data tab (src/dc/mncharacters.c)
mncharacters.o: ../../dc/mncharacters.c ../../dc/mncharacters.h ../../dc/lbcommon.h \
                ../../dc/sprite.h ../../dc/scmanager.h ../../dc/ftcommon.h \
                ../../dc/bgm.h ../../dc/fighter.h ../../dc/objmodel.h \
                ../../dc/efmanager.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mncharacters.c -o mncharacters.o
# mn/mndata/mnsoundtest.c, the Sound Test tab (src/dc/mnsoundtest.c)
mnsoundtest.o: ../../dc/mnsoundtest.c ../../dc/mnsoundtest.h ../../dc/lbcommon.h \
               ../../dc/sprite.h ../../dc/scmanager.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnsoundtest.c -o mnsoundtest.o
# mn/mnoption/mnoption.c, the Options screen (src/dc/mnoption.c)
mnoption.o: ../../dc/mnoption.c ../../dc/mnoption.h ../../dc/lbcommon.h \
            ../../dc/sprite.h ../../dc/scmanager.h ../../dc/bgm.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnoption.c -o mnoption.o
# mn/mnoption/mnscreenadjust.c, the Screen Adjust screen (src/dc/mnscreenadjust.c)
mnscreenadjust.o: ../../dc/mnscreenadjust.c ../../dc/mnscreenadjust.h \
                  ../../dc/lbcommon.h ../../dc/sprite.h ../../dc/scmanager.h \
                  ../../dc/bgm.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnscreenadjust.c -o mnscreenadjust.o
# mn/mnoption/mnbackupclear.c, the Backup Clear screen (src/dc/mnbackupclear.c)
mnbackupclear.o: ../../dc/mnbackupclear.c ../../dc/mnbackupclear.h \
                 ../../dc/lbcommon.h ../../dc/sprite.h ../../dc/scmanager.h \
                 $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnbackupclear.c -o mnbackupclear.o
# mn/mncommon/mncongra.c, the congratulations screen (src/dc/mncongra.c)
mncongra.o: ../../dc/mncongra.c ../../dc/mncongra.h \
            ../../dc/lbcommon.h ../../dc/sprite.h ../../dc/scmanager.h \
            $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mncongra.c -o mncongra.o
# mn/mncommon/mnnocontroller.c (+ mnnocontrollerfiles.c), the No
# Controller screen (src/dc/mnnocontroller.c).
mnnocontroller.o: ../../dc/mnnocontroller.c ../../dc/mnnocontroller.h \
                  ../../dc/lbcommon.h ../../dc/sprite.h ../../dc/scmanager.h \
                  $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnnocontroller.c -o mnnocontroller.o
# The memory card's page, the port's own scene (src/dc/dcmemcard.h,
# src/dc/scport.h): no overlay, no decomp file behind it.
dcmemcard.o: ../../dc/dcmemcard.c ../../dc/dcmemcard.h ../../dc/scport.h \
             ../../dc/vmucard.h ../../dc/vmusave.h ../../dc/dctext.h \
             ../../dc/dcstrings.h $(DC_HDRS) $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/dcmemcard.c -o dcmemcard.o
# mn/mncommon/mnstartup.c, the N64 logo boot screen (src/dc/mnstartup.c)
# -- the only US route into the openings chain.
mnstartup.o: ../../dc/mnstartup.c ../../dc/mnstartup.h \
             ../../dc/lbcommon.h ../../dc/sprite.h \
             ../../dc/scmanager.h ../../dc/bgm.h \
             $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnstartup.c -o mnstartup.o
# mn/mnvsmode/mnvsitemswitch.c, the item switch (src/dc/mnvsitemswitch.c)
mnvsitemswitch.o: ../../dc/mnvsitemswitch.c ../../dc/mnvsitemswitch.h \
                  ../../dc/lbcommon.h ../../dc/sprite.h ../../dc/scmanager.h \
                  $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/mnvsitemswitch.c -o mnvsitemswitch.o
# sc/scsubsys/scsubsysfighter.c, the menus' fighter light and status
scsubsysfighter.o: ../../dc/scsubsysfighter.c ../../dc/ftcommon.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/scsubsysfighter.c -o scsubsysfighter.o
# sc/scsubsys/scsubsysdata.c, the two FTOpeningDesc tables a demo status
# is read out of and the per-kind menu scale. It does NOT
# take ../../dc/ftcommon.h: see the comment at the top of the file.
scsubsysdata.o: ../../dc/scsubsysdata.c $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/scsubsysdata.c -o scsubsysdata.o
# lb/lbfade.c, the fade between scenes, over a flat quad
lbfade.o: ../../dc/lbfade.c ../../dc/lbcommon.h ../../dc/objpvr.h $(SHIM_HDRS)
	kos-cc $(KOS_CFLAGS) -c ../../dc/lbfade.c -o lbfade.o
# sc/scsubsys/scsubsyscontroller.c, the menus' pad reads, compiled
# unmodified: eight loops over gSYControllerDevices through the
# device-status table src/dc/input.c keeps.
# sc/scsubsys/scsubsyscontroller is overlay 1, with src/dc/scsubsysfighter.c
scsubsyscontroller.o: $(SSB_DECOMP_DIR)/src/sc/scsubsys/scsubsyscontroller.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) -c $< -o $@.tmp
	$(call ovl-noload,1,$@)

# lb/lbbackup.c, the save data, compiled unmodified: the checksum, the
# two copies, the fallback from one to the other and the defaults when
# neither holds. It is overlay 0 -- the always-resident half of the game,
# which is why its one unused function is named func_ovl0_800D4C90 -- and
# it has no statics, so it takes no noload rename. The store under it is
# a byte array in src/dc/vmusave.c, read off a memory card at boot and
# written back when it changes; everything above that is
# the game's.
lbbackup.o: $(SSB_DECOMP_DIR)/src/lb/lbbackup.c $(SHIM_HDRS) Makefile
	kos-cc $(KOS_CFLAGS) $(DECOMP_CFLAGS) -c $< -o $@

include $(KOS_BASE)/Makefile.rules

# KOS's own rule (Makefile.rules, included just above, whose recipe this
# one replaces) but for one word: bin2c declares the
# image const, which puts it in .rodata. The fighter packs are relocated
# where they lie in it (src/dc/fighter.c fighter_load), so the image
# goes in .data. Nothing else changes -- the SH-4 under KOS has no
# memory protection either way, and the section is the honest label.
romdisk.o: romdisk.img
	$(KOS_BASE)/utils/bin2c/bin2c romdisk.img romdisk_tmp.c romdisk
	sed -i 's/^const unsigned char romdisk_data/unsigned char romdisk_data/' romdisk_tmp.c
	kos-cc -o romdisk_tmp.o -c romdisk_tmp.c
	$(KOS_CC) -o romdisk.o -r romdisk_tmp.o $(KOS_LIB_PATHS) -Wl,--whole-archive -lromdiskbase -nostdlib
	rm romdisk_tmp.c romdisk_tmp.o

$(filter-out romdisk.o,$(OBJS)): $(CFLAGS_STAMP)

# EXTRA_LDFLAGS is the link-time sibling of EXTRA_CFLAGS: it carries the
# -Wl,--wrap=... the heap probes need (src/dc/db.h's DB_HEAP_CALLERS and
# DB_HEAP_GUARD) to the one line that links the game. It is not in
# CFLAGS_NOW: the -D that turns a probe on lives in EXTRA_CFLAGS, so the
# tree already rebuilds when the probe changes.
$(TARGET): $(OBJS) particlebanks.ld wpattrs.ld
	kos-cc -o $(TARGET) $(OBJS) particlebanks.ld wpattrs.ld $(EXTRA_LDFLAGS)

clean: rm-elf
	-rm -f $(OBJS) $(CFLAGS_STAMP) romdisk.img $(FIGHTER_PACKS) $(FIGHTER_ANMS) romdisk/hyrule.stg \
	    romdisk/mntitle.spr $(TITLE_ASSETS) $(HUD_BANKS) $(MENU_BANKS) $(ITEM_MODELS) $(FIGHTER_BANKS) \
	    romdisk/bgm.pak \
	    romdisk/fgm.ucd romdisk/fgm.tbl romdisk/fgm.unk \
	    romdisk/fgm_sounds.pak romdisk/sndsets.bin $(TRANSITIONS) $(EMBLEMS) \
	    $(CAMANIM_BANKS) \
	    romdisk/spotlite.mdl romdisk/ifarrows.mdl romdisk/ifmagnify.mdl \
	    romdisk/ftshadow.mdl romdisk/vmuart.bin romdisk/dcfont.spr vmuart-preview.png \
	    $(EXPLAIN_MODELS) $(STAFFROLL_MODELS) $(CLIFF_MODELS) $(ROOM_MODELS) \
	    romdisk/efhalo.mdl romdisk/efmballrays.mdl romdisk/efswirl.mdl \
	    romdisk/efquake.mdl romdisk/eforbs.mdl \
	    disc/efslash.mdl disc/models.bnd romdisk/efspark.mdl romdisk/efmdust.mdl \
	    romdisk/efdexp.mdl romdisk/efshield.mdl romdisk/efyegg.mdl \
	    romdisk/efshock.mdl romdisk/effirespark.mdl \
	    romdisk/efimpactwave.mdl romdisk/wpmariofireball.mdl \
	    romdisk/wpfoxblaster.mdl romdisk/wparwinglaser.mdl \
	    romdisk/wpsamuschargeshot.mdl romdisk/wppikachujolt.mdl \
	    romdisk/wpyoshistar.mdl romdisk/wpyoshieggthrow.mdl romdisk/wplgunammo.mdl \
	    romdisk/wpsamusbomb.mdl \
	    romdisk/wplinkboomerang.mdl \
	    romdisk/wpkirbycutter.mdl romdisk/wpbossbullet.mdl \
	    romdisk/wppikachugjolt.mdl \
	    romdisk/wppikachuthunder.mdl \
	    romdisk/wpnesspkfire.mdl romdisk/wpnesspkthunder.mdl \
	    romdisk/wpnesspktrail.mdl \
	    romdisk/wplinkspin.mdl romdisk/efspin.mdl \
	    romdisk/efvulcan.mdl \
	    romdisk/efpsimagnet.mdl romdisk/efpkwave.mdl romdisk/efpktrail.mdl \
	    romdisk/efreflectbreak.mdl romdisk/efcatchswirl.mdl romdisk/efgrapplebeam.mdl romdisk/efreflector.mdl romdisk/effalconkick.mdl romdisk/effalconpunch.mdl romdisk/efsing.mdl romdisk/efmballthrown.mdl romdisk/efjoltspark.mdl romdisk/efthundertrail.mdl romdisk/efkirbystar.mdl romdisk/efthundershock.mdl \
	    romdisk/efdokan.mdl \
	    romdisk/eftaru.mdl romdisk/efpoint.mdl \
	    romdisk/efstar.mdl \
	    romdisk/efwave.mdl romdisk/efbeam.mdl \
	    romdisk/efentryegg.mdl romdisk/efarwing.mdl romdisk/efcar.mdl romdisk/efegglay.mdl \
	    romdisk/efcutup.mdl romdisk/efcutdown.mdl \
	    romdisk/efcutdraw.mdl romdisk/efcuttrail.mdl \
	    $(PARTICLE_BANKS) particlebanks.ld hosttest_particlebanks.ld \
	    wpattrs.ld hosttest_wpattrs.ld romdisk/wpattrs.bin \
	    sintable.c \
	    hosttest_ft hosttest_*.o hosttest_save.bin
	-rm -rf credits

rm-elf:
	-rm -f $(TARGET)

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist: $(TARGET)
	-rm -f $(OBJS)
	$(KOS_STRIP) $(TARGET)

# Headless cross-test of the ftcommon port against the decomp's logic
# (see hosttest_ft.c). Runs on the build host, no KOS needed. The
# collision under it is the decomp's mpcollision.c/mpprocess.c
# themselves, compiled for the host through the same shim headers.
# HOST_EXTRA_CFLAGS is where a diagnostic build goes in, e.g.
#   make hosttest HOST_EXTRA_CFLAGS="-fsanitize=address -DSCENE_HEAP_POISON"
# which is the pair that found the overlay bug (src/dc/overlay.h).
# -ffp-contract=off for the same reason the target build has it
# (scripts/dc_env.sh): this test's whole claim is that the port computes
# what the decomp computes, and it cannot make that claim from a machine
# whose compiler is allowed to fuse a multiply and an add when the N64's
# could not. x86-64's baseline has no FMA instruction, so today the flag
# changes nothing here -- which is exactly when to write it down, rather
# than after someone builds with -march=native and spends a day on the
# last two bits.
# The in-build baker is this same link with -DFT_BAKER and no run:
#   make hosttest BAKER=1 HOST_OUT=bake_host HOST_RUN=true HOST_EXTRA_CFLAGS=-m32
HOST_OUT ?= hosttest_ft
HOST_RUN ?= ./hosttest_ft
# SSB_NO_DRAW is what FT_HOSTTEST meant in the draw half (fighter.c, objmodel.c,
# objdisplay.c, ftdisplaymain.c, ftshadow.c, lbcommon.[ch], objpvr.h): no PVR,
# nothing drawn. BAKER=1 leaves it off and puts KOS's own dc/pvr.h and a
# recording pvr_* (hoststubs/bakepvr.c) in its place.
# The baker is a 32-bit program (see docker/Dockerfile). On an x86 host
# that is gcc -m32. Anywhere else (an arm64 Linux or macOS container) there is
# no 32-bit gcc and nothing that runs 32-bit x86 code natively, so it is
# cross-compiled to an i386 binary and run under qemu-i386 through a
# launcher script that has bake_host's name. HOST32=cross forces that.
HOST_GCC = gcc
HOST_OBJCOPY = objcopy
HOST32 ?= $(if $(filter x86_64 i686 i386,$(shell uname -m)),native,cross)
ifdef BAKER
ifeq ($(HOST32),cross)
HOST_GCC = i686-linux-gnu-gcc
HOST_OBJCOPY = i686-linux-gnu-objcopy
HOST_BIN = $(HOST_OUT).i386
else
HOST_BIN = $(HOST_OUT)
endif
HOST_DRAW_CFLAGS = -m32 -DFT_BAKER -include stddef.h '-D__pure2=__attribute__((__const__))' \
    -idirafter $(KOS_BASE)/include -idirafter $(KOS_BASE)/kernel/arch/dreamcast/include -idirafter $(KOS_BASE)/addons/include \
    -I hoststubs
HOST_DRAW_SRCS = hoststubs/bakepvr.c ../../dc/primdump.c
# KOS's polygon compiler, cut out of its own source for hoststubs/bakepvr.c
hosttest: hoststubs/kos_poly.inc
hoststubs/kos_poly.inc: $(KOS_BASE)/kernel/arch/dreamcast/hardware/pvr/pvr_prim.c
	awk '/^\/\* Compile a polygon context/ {on=1} /^\/\* Create an untextured sprite context/ {on=0} on' $< > $@
else
HOST_DRAW_CFLAGS = -DSSB_NO_DRAW
HOST_DRAW_SRCS =
endif
HOST_CFLAGS = -std=gnu99 -Wall -g -DFT_HOSTTEST $(HOST_DRAW_CFLAGS) -I hoststubs -I ../../dc \
              -ffp-contract=off \
              $(DECOMP_INC) $(HOST_EXTRA_CFLAGS)
.PHONY: hosttest
# The decomp sources the test links, each compiled for the host through
# the same shim headers the target build uses. The sys/ half is the
# object system, which the test now drives for real: the fighter it
# spawns is a GObj with a DObj tree and its animation runs through
# ft/ftanim.c and sys/objanim.c, exactly as on the target.
#
# ft/ftanim.c is the one decomp file deliberately left out: its
# ftAnimParseDObjFigatree reads animation data, and there is no ROM
# here to read. hosttest_ft.c stands in for it and scripts the
# animation instead; tools/check/figatree_check.py is what tests the real
# parser, against all 1775 of the ROM's animations.
HOST_DECOMP_SRCS = mp/mpcollision.c mp/mpprocess.c sys/malloc.c \
                   sys/objman.c sys/objhelper.c sys/objscript.c \
                   sys/interp.c sys/utils.c sys/vector.c \
                   gm/gmcollision.c ft/ftcommondata.c ft/ftpublic.c \
                   ft/ftphysics.c \
                   ft/ftcommon/ftcommonstopceil.c \
                   ft/ftcommon/ftcommondownforwardback.c \
                   ft/ftcommon/ftcommondownattack.c \
                   ft/ftcommon/ftcommonrebound.c \
                   ft/ftcommon/ftcommoncliffattack.c \
                   ft/ftcommon/ftcommonlandingair.c \
                   ft/ftcommon/ftcommonfallspecial.c \
                   ft/ftcommon/ftcommonspeciallw.c \
                   ft/ftcommon/ftcommondokan.c \
                   ft/ftchar/ftmario/ftmariospeciallw.c \
                   ft/ftchar/ftdonkey/ftdonkeyspeciallw.c \
                   ft/ftchar/ftpurin/ftpurinspeciallw.c \
                   ft/ftchar/ftkirby/ftkirbyspeciallw.c \
                   ft/ftchar/ftpurin/ftpurinspecialn.c \
                   ft/ftchar/ftkirby/ftkirbycopypurinspecialn.c \
                   ft/ftchar/ftkirby/ftkirbycopymariospecialn.c \
                   ft/ftchar/ftkirby/ftkirbycopyfoxspecialn.c \
                   ft/ftchar/ftkirby/ftkirbycopysamusspecialn.c \
                   ft/ftchar/ftkirby/ftkirbycopydonkeyspecialn.c \
                   ft/ftchar/ftkirby/ftkirbycopycaptainspecialn.c \
                   ft/ftchar/ftkirby/ftkirbycopyyoshispecialn.c \
                   ft/ftchar/ftfox/ftfoxspecialn.c \
                   ft/ftchar/ftmario/ftmariospecialn.c \
                   ft/ftchar/ftmario/ftmariospecialhi.c \
                   ft/ftchar/ftpurin/ftpurinspecialhi.c \
                   ft/ftchar/ftdonkey/ftdonkeyspecialhi.c \
                   ft/ftchar/ftsamus/ftsamusspecialhi.c \
                   ft/ftchar/ftcaptain/ftcaptainspecialn.c \
                   ft/ftchar/ftsamus/ftsamusspeciallw.c \
                   ft/ftchar/ftcaptain/ftcaptainspeciallw.c \
                   ft/ftchar/ftyoshi/ftyoshispeciallw.c \
                   ft/ftchar/ftdonkey/ftdonkeyspecialn.c \
                   ft/ftchar/ftcaptain/ftcaptainspecialhi.c \
                   ft/ftcommon/ftcommoncapturecaptain.c \
                   ft/ftchar/ftyoshi/ftyoshispecialn.c \
                   ft/ftcommon/ftcommoncaptureyoshi.c \
                   ft/ftcommon/ftcommoncapture.c \
                   ft/ftcommon/ftcommoncapturekirby.c \
                   ft/ftchar/ftkirby/ftkirbyspecialn.c \
                   ft/ftchar/ftsamus/ftsamusspecialn.c \
                   ft/ftchar/ftfox/ftfoxspeciallw.c \
                   ft/ftchar/ftfox/ftfoxspecialhi.c \
                   ft/ftchar/ftmario/ftmario.c \
                   ft/ftchar/ftluigi/ftluigi.c \
                   ft/ftchar/ftfox/ftfox.c \
                   ft/ftchar/ftpurin/ftpurin.c \
                   ft/ftchar/ftsamus/ftsamus.c \
                   ft/ftchar/ftyoshi/ftyoshi.c \
                   ft/ftchar/ftkirby/ftkirby.c \
                   wp/wpmario/wpmariofireball.c \
                   wp/wpfox/wpfoxblaster.c \
                   wp/wpsamus/wpsamusbomb.c \
                   wp/wpyoshi/wpyoshistar.c \
                   ft/ftchar/ftyoshi/ftyoshispecialhi.c \
                   wp/wpyoshi/wpyoshieggthrow.c \
                   ft/ftchar/ftkirby/ftkirbyspecialhi.c \
                   wp/wpkirby/wpkirbycutter.c \
                   wp/wpsamus/wpsamuschargeshot.c \
                   ft/ftchar/ftlink/ftlink.c \
                   ft/ftchar/ftlink/ftlinkspecialn.c \
                   ft/ftchar/ftlink/ftlinkspecialhi.c \
                   wp/wplink/wplinkboomerang.c \
                   wp/wplink/wplinkspinattack.c \
                   ft/ftchar/ftkirby/ftkirbycopylinkspecialn.c \
                   ft/ftchar/ftpikachu/ftpikachu.c \
                   ft/ftchar/ftdonkey/ftdonkey.c \
                   ft/ftchar/ftpikachu/ftpikachuspecialn.c \
                   wp/wppikachu/wppikachuthunderjolt.c \
                   ft/ftchar/ftkirby/ftkirbycopypikachuspecialn.c \
                   ft/ftchar/ftpikachu/ftpikachuspeciallw.c \
                   wp/wppikachu/wppikachuthunder.c \
                   ft/ftchar/ftpikachu/ftpikachuspecialhi.c \
                   ft/ftchar/ftness/ftness.c \
                   ft/ftchar/ftness/ftnessspecialn.c \
                   ft/ftchar/ftness/ftnessspecialhi.c \
                   ft/ftchar/ftness/ftnessspeciallw.c \
                   ft/ftchar/ftkirby/ftkirbycopynessspecialn.c \
                   ft/ftchar/ftboss/ftboss.c \
                   ft/ftchar/ftboss/ftbosscommon.c \
                   ft/ftchar/ftboss/ftbosswait.c \
                   ft/ftchar/ftboss/ftbossappear.c \
                   ft/ftchar/ftboss/ftbossdeadcenter.c \
                   ft/ftchar/ftboss/ftbossdeadleft.c \
                   ft/ftchar/ftboss/ftbossdeadright.c \
                   ft/ftchar/ftboss/ftbossdefault.c \
                   ft/ftchar/ftboss/ftbossdrill.c \
                   ft/ftchar/ftboss/ftbossgootsubusudown.c \
                   ft/ftchar/ftboss/ftbossgootsubusuend.c \
                   ft/ftchar/ftboss/ftbossgootsubusuup.c \
                   ft/ftchar/ftboss/ftbossgootsubusuwait.c \
                   ft/ftchar/ftboss/ftbossharau.c \
                   ft/ftchar/ftboss/ftbosshippataku.c \
                   ft/ftchar/ftboss/ftbossmove.c \
                   ft/ftchar/ftboss/ftbossokuhikouki1.c \
                   ft/ftchar/ftboss/ftbossokuhikouki2.c \
                   ft/ftchar/ftboss/ftbossokuhikouki3.c \
                   ft/ftchar/ftboss/ftbossokukouki.c \
                   ft/ftchar/ftboss/ftbossokupunch1.c \
                   ft/ftchar/ftboss/ftbossokupunch2.c \
                   ft/ftchar/ftboss/ftbossokupunch3.c \
                   ft/ftchar/ftboss/ftbossokutsubushi.c \
                   ft/ftchar/ftboss/ftbossokutsubushistart.c \
                   ft/ftchar/ftboss/ftbosstsutsuku1.c \
                   ft/ftchar/ftboss/ftbosstsutsuku2.c \
                   ft/ftchar/ftboss/ftbosstsutsuku3.c \
                   ft/ftchar/ftboss/ftbosswalk.c \
                   ft/ftchar/ftboss/ftbosswalkloop.c \
                   ft/ftchar/ftboss/ftbosswalkshoot.c \
                   ft/ftchar/ftboss/ftbosswalkwait.c \
                   ft/ftchar/ftboss/ftbossyubideppou1.c \
                   ft/ftchar/ftboss/ftbossyubideppou2.c \
                   ft/ftchar/ftboss/ftbossyubideppou3.c \
                   wp/wpboss/wpbossbullet.c \
                   wp/wpness/wpnesspkfire.c \
                   wp/wpness/wpnesspkthunder.c \
                   it/itfighter/itnesspkfire.c \
                   ft/ftchar/ftlink/ftlinkspeciallw.c \
                   it/itfighter/itlinkbomb.c \
                   ft/ftchar/ftdonkey/ftdonkeythrowfwait.c \
                   ft/ftchar/ftdonkey/ftdonkeythrowfwalk.c \
                   ft/ftchar/ftdonkey/ftdonkeythrowfturn.c \
                   ft/ftchar/ftdonkey/ftdonkeythrowfkneebend.c \
                   ft/ftchar/ftdonkey/ftdonkeythrowffall.c \
                   ft/ftchar/ftdonkey/ftdonkeythrowflanding.c \
                   ft/ftchar/ftdonkey/ftdonkeythrowfdamage.c \
                   ft/ftchar/ftdonkey/ftdonkeythrowff.c \
                   ft/ftchar/ftkirby/ftkirbythrowf.c \
                   ft/ftcommon/ftcommonattack100.c \
                   ft/ftcommon/ftcommonjumpaerial.c \
                   ft/ftcommon/ftcommonsquat.c \
                   ft/ftcommon/ftcommonfall.c \
                   ft/ftcommon/ftcommonpass.c \
                   ft/ftcommon/ftcommonget.c \
                   ft/ftcommon/ftcommonitemthrow.c \
                   ft/ftcommon/ftcommonitemswing.c \
                   ft/ftcommon/ftcommonitemshoot.c \
                   ft/fthammer.c \
                   ft/ftcommon/ftcommonhammerfall.c \
                   ft/ftcommon/ftcommonhammerkneebend.c \
                   ft/ftcommon/ftcommonhammerlanding.c \
                   ft/ftcommon/ftcommonhammerturn.c \
                   ft/ftcommon/ftcommonhammerwalk.c \
                   ef/efparticle.c sys/matrix.c \
                   libultra/gu/mtxutil.c libultra/gu/mtxcatf.c libultra/gu/mtxxfmf.c \
                   libultra/gu/normalize.c \
                   sc/scsubsys/scsubsyscontroller.c lb/lbbackup.c
ifdef BAKER
HOST_DECOMP_SRCS += ft/ftanim.c
endif
HOST_DECOMP_OBJS = $(patsubst %.c,hosttest_%.o,$(notdir $(HOST_DECOMP_SRCS))) \
                   hosttest_objanim.o
# of those, the ones smashbrothers.us.yaml puts in overlay 2
HOST_OVL2_OBJS = hosttest_mpcollision.o hosttest_mpprocess.o \
                 hosttest_gmcollision.o hosttest_ftcommondata.o \
                 hosttest_ftphysics.o hosttest_efparticle.o \
                 $(patsubst %.o,hosttest_%.o,$(FTCHAR_DATA_OBJS))
# and the ones it puts in overlay 3
HOST_OVL3_OBJS = hosttest_ftpublic.o \
                 $(patsubst %.o,hosttest_%.o,$(FTCOMMON_STATUS_OBJS)) \
                 $(patsubst %.o,hosttest_%.o,$(FTCHAR_SPECIAL_OBJS)) \
                 $(patsubst %.o,hosttest_%.o,$(WPCHAR_OBJS)) \
                 $(patsubst %.o,hosttest_%.o,$(FTBOSS_OBJS))

# hosttest/match.c opens every file the opening movie's prefetch list names
# (src/dc/scprefetch.h, sSCPrefetch_opening*), so each is a prerequisite:
# read out of the header, so a regenerated list cannot leave one unbuilt.
# Without this a clean tree failed the check unless `./run.sh disc` ran first.
HOST_PREFETCH_FILES := $(addprefix romdisk/,$(shell awk \
    '/sSCPrefetch_opening[a-z]*\[\] *=/ { f = 1; next } \
     f && /^};/ { f = 0 } \
     f { gsub(/[ ",]/, ""); if ($$0 != "") print }' ../../dc/scprefetch.h))
hosttest: $(HOST_PREFETCH_FILES)

hosttest: hosttest_ft.c $(wildcard hosttest/*.c) ../../dc/ftcommon.c ../../dc/ftmanager.c ../../dc/ftcomputer.c \
          ../../dc/wpmanager.c ../../dc/wpattrs.c ../../dc/wpmain.c ../../dc/wpprocess.c ../../dc/wpdisplay.c ../../dc/wpmap.c \
          ../../dc/itmanager.c ../../dc/itempack.c ../../dc/itemmodel.c ../../dc/itbox.c ../../dc/ittaru.c ../../dc/itcapsule.c ../../dc/itegg.c ../../dc/itmball.c ../../dc/itmew.c ../../dc/ittosakinto.c ../../dc/itsawamura.c ../../dc/itkabigon.c ../../dc/itmlucky.c ../../dc/itnyars.c ../../dc/itdogas.c ../../dc/itiwark.c ../../dc/itstarmie.c ../../dc/itlizardon.c ../../dc/itspear.c ../../dc/itkamex.c ../../dc/itpippi.c ../../dc/itmain.c ../../dc/itprocess.c ../../dc/itvisuals.c ../../dc/itdisplay.c ../../dc/itmap.c ../../dc/itstar.c ../../dc/ittomato.c ../../dc/itheart.c ../../dc/itsword.c ../../dc/itbat.c ../../dc/ithammer.c ../../dc/itharisen.c ../../dc/itfflower.c ../../dc/itlgun.c ../../dc/itstarrod.c ../../dc/itgshell.c ../../dc/itrshell.c ../../dc/itnbumper.c ../../dc/itgbumper.c ../../dc/itpowerblock.c ../../dc/itpakkun.c ../../dc/itglucky.c ../../dc/itporygon.c ../../dc/itmarumine.c ../../dc/ithitokage.c ../../dc/itfushigibana.c ../../dc/ittarget.c ../../dc/ittarubomb.c ../../dc/itmsbomb.c ../../dc/itbombhei.c \
          ../../dc/gmrumble.c ../../dc/ftcommontwister.c ../../dc/grhyrule.c ../../dc/grjungle.c ../../dc/ftcommontaru.c ../../dc/grzebes.c ../../dc/gryoster.c ../../dc/grpupupu.c ../../dc/grinishie.c ../../dc/grcastle.c ../../dc/gryamabuki.c ../../dc/grsector.c ../../dc/grbonus3.c \
          ../../dc/fighter.c ../../dc/assetroot.c $(FIGHTER_PACKS) $(FIGHTER_ANMS) \
          ../../dc/ftmain.c ../../dc/ftparam.c ../../dc/ftshadow.c \
          ../../dc/ifcommon.c ../../dc/ifscreenflash.c \
          hosttest_gmcolscripts.c ../../dc/ftdisplaymain.c \
          ../../dc/scmanager.c ../../dc/scmanagerdata.c ../../dc/sndres.c \
          ../../dc/scvsbattle.c \
          ../../dc/scvsresults.c ../../dc/scautodemo.c ../../dc/scexplain.c \
          ../../dc/mvending.c ../../dc/scstaffroll.c ../../dc/mvopeningroom.c ../../dc/mvopeningportraits.c ../../dc/mvopeningmario.c ../../dc/mvopeningdonkey.c ../../dc/mvopeningsamus.c ../../dc/mvopeninglink.c ../../dc/mvopeningyoshi.c ../../dc/mvopeningkirby.c ../../dc/mvopeningfox.c ../../dc/mvopeningpikachu.c ../../dc/mvopeningrun.c ../../dc/mvopeningcliff.c ../../dc/mvopeningyamabuki.c ../../dc/mvopeningjungle.c ../../dc/mvopeningyoster.c ../../dc/mvopeningsector.c ../../dc/mvopeningstandoff.c ../../dc/mvopeningclash.c ../../dc/mvopeningnewcomers.c \
          ../../dc/mntitle.c ../../dc/sprite.c ../../dc/camanim.c \
          ../../dc/mnmodeselect.c ../../dc/mn1pmode.c ../../dc/mnvsmode.c ../../dc/mnvsoptions.c \
          ../../dc/mnplayersvs.c ../../dc/mnplayers1ptraining.c \
          ../../dc/sc1ptrainingmode.c \
          ../../dc/mnplayers1pgame.c \
          ../../dc/mnplayers1pbonus.c \
          ../../dc/sc1pchallenger.c ../../dc/sc1pintro.c ../../dc/sc1pgame.c \
          ../../dc/sc1pbonusstage.c \
          ../../dc/sc1pgameboss.c \
          ../../dc/sc1pstageclear.c ../../dc/sc1pmanager.c \
          ../../dc/mn1pcontinue.c \
          ../../dc/mnmaps.c ../../dc/mnmessage.c \
          ../../dc/mnvsitemswitch.c ../../dc/mndata.c ../../dc/mnvsrecord.c \
          ../../dc/mncharacters.c \
          ../../dc/mnsoundtest.c \
          ../../dc/mnoption.c \
          ../../dc/mnscreenadjust.c \
          ../../dc/mnbackupclear.c \
          ../../dc/mncongra.c \
          ../../dc/mnnocontroller.c ../../dc/dcmemcard.c \
          ../../dc/mnstartup.c \
          ../../dc/scsubsysfighter.c ../../dc/scsubsysdata.c \
          ../../dc/lbfade.c ../../dc/lbtransition.c \
          ../../dc/gmcommon.c \
          romdisk/mntitle.spr $(TITLE_ASSETS) romdisk/sndsets.bin $(HUD_BANKS) $(MENU_BANKS) $(ITEM_MODELS) $(FIGHTER_BANKS) \
          $(CAMANIM_BANKS) \
          $(CREDITS_ENCODED) \
          ../../dc/gmcamera.c ../../dc/lbcommon.c \
          ../../dc/mpcommon.c \
          ../../dc/mpshim.c ../../dc/taskman.c ../../dc/sysshim.c \
          ../../dc/vmusave.c ../../dc/vmucard.c romdisk/vmuart.bin \
          ../../dc/dctext.c romdisk/dcfont.spr ../../dc/vmunotice.c \
          ../../dc/syaudio.c \
          ../../dc/introcrowd.c ../../dc/objdisplay.c ../../dc/objmodel.c ../../dc/overlay.c \
          ../../dc/clip.c ../../dc/dma.c ../../dc/lbparticle.c \
          ../../dc/lbpartex.c ../../dc/lbpdraw.c ../../dc/efdisplay.c \
          ../../dc/efmanager.c ../../dc/efground.c \
          sintable.c \
          gusinf.c gucosf.c \
          $(PARTICLE_BANKS) hosttest_particlebanks.ld hosttest_wpattrs.ld \
          romdisk/wpsamuschargeshot.mdl romdisk/wppikachujolt.mdl \
          romdisk/wpyoshistar.mdl romdisk/wpyoshieggthrow.mdl romdisk/wplgunammo.mdl \
          romdisk/wpsamusbomb.mdl \
          romdisk/wplinkboomerang.mdl \
          romdisk/wpkirbycutter.mdl romdisk/wpbossbullet.mdl \
          romdisk/wppikachugjolt.mdl \
          romdisk/wppikachuthunder.mdl \
          romdisk/wpnesspkfire.mdl romdisk/wpnesspkthunder.mdl \
          romdisk/wpnesspktrail.mdl \
          romdisk/wplinkspin.mdl romdisk/efspin.mdl \
          romdisk/efvulcan.mdl \
          romdisk/efpsimagnet.mdl romdisk/efpkwave.mdl romdisk/efpktrail.mdl \
          romdisk/efreflectbreak.mdl romdisk/efcatchswirl.mdl romdisk/efgrapplebeam.mdl romdisk/efreflector.mdl romdisk/effalconkick.mdl romdisk/effalconpunch.mdl romdisk/efsing.mdl romdisk/efmballthrown.mdl romdisk/efjoltspark.mdl romdisk/efthundertrail.mdl romdisk/efkirbystar.mdl romdisk/efthundershock.mdl \
          romdisk/efdokan.mdl romdisk/eftaru.mdl romdisk/efpoint.mdl \
          romdisk/efstar.mdl romdisk/efwave.mdl romdisk/efbeam.mdl \
          romdisk/efentryegg.mdl romdisk/efarwing.mdl romdisk/efcar.mdl romdisk/efegglay.mdl \
          romdisk/efcutup.mdl romdisk/efcutdown.mdl \
          romdisk/efmballrays.mdl romdisk/efswirl.mdl \
          romdisk/efcutdraw.mdl romdisk/efcuttrail.mdl \
          romdisk/jungle.stg romdisk/zebes.stg romdisk/yoster.stg romdisk/pupupu.stg \
          romdisk/inishie.stg romdisk/itcommon.itp romdisk/wpattrs.bin \
          romdisk/hyrule.stg romdisk/castle.stg romdisk/sector.stg \
          romdisk/yamabuki.stg romdisk/bonus1mario.stg romdisk/bonus2fox.stg \
          romdisk/bonus3.stg \
          $(DC_HDRS) $(SHIM_HDRS)
	for f in $(HOST_DECOMP_SRCS); do \
	    $(HOST_GCC) $(HOST_CFLAGS) $(DECOMP_CFLAGS) $(OVL_NOLOAD_CFLAGS) \
	        -include sys/debug.h -include sc/scene.h -include syaudio.h \
	        -include sys/dma.h -c \
	        $(SSB_DECOMP_DIR)/src/$$f -o hosttest_$$(basename $$f .c).o \
	        || exit 1; \
	done
# The same rename the target build does, so the host suite exercises the
# same mechanism: the two-pass scene tests then prove a decomp file's
# statics really are zero on the second entry, which is the half of the
# overlay reload no written-out list can reach. sys/* are always resident
# and take no rename; ft/ftanim is overlay 2 but the host link has no
# need of it.
	for o in $(HOST_OVL2_OBJS); do \
	    $(HOST_OBJCOPY) --rename-section .bss=ovl2_noload $$o || exit 1; \
	done
	$(HOST_OBJCOPY) --rename-section .bss=ovl1_noload hosttest_scsubsyscontroller.o
	for o in $(HOST_OVL3_OBJS); do \
	    $(HOST_OBJCOPY) --rename-section .bss=ovl3_noload $$o || exit 1; \
	done
	$(HOST_GCC) $(HOST_CFLAGS) $(DECOMP_CFLAGS) -c ../../dc/ftmain.c \
	    -o hosttest_ftmain.o
# sys/objanim.c, through the port file that #includes it (the colour
# lerp). The host is little-endian too, which is the point.
	$(HOST_GCC) $(HOST_CFLAGS) $(DECOMP_CFLAGS) \
	    -include sys/debug.h -include sc/scene.h -include syaudio.h \
	    -include sys/dma.h -c ../../dc/objanim.c -o hosttest_objanim.o
# -no-pie: particlebanks.ld defines the bank addresses as absolute
# symbols, and a PIE adds its load bias to those too -- so &lEFCommon...
# comes out as base+0xAC7340 and src/dc/dma.c finds no range for it. The
# target is a fixed-address ELF and never had the problem; the host link
# has to be told.
	$(HOST_GCC) $(HOST_CFLAGS) -no-pie -I. -o $(or $(HOST_BIN),$(HOST_OUT)) hosttest_ft.c $(HOST_DRAW_SRCS) ../../dc/ftcommon.c \
	    ../../dc/itemmodel.c ../../dc/itbox.c ../../dc/ittaru.c ../../dc/itcapsule.c ../../dc/itegg.c ../../dc/itmball.c ../../dc/itmew.c ../../dc/ittosakinto.c ../../dc/itsawamura.c ../../dc/itkabigon.c ../../dc/itmlucky.c ../../dc/itnyars.c ../../dc/itdogas.c ../../dc/itiwark.c ../../dc/itstarmie.c ../../dc/itlizardon.c ../../dc/itspear.c ../../dc/itkamex.c ../../dc/itpippi.c ../../dc/ftmanager.c ../../dc/ftcomputer.c ../../dc/wpmanager.c ../../dc/wpattrs.c ../../dc/wpmain.c ../../dc/wpprocess.c ../../dc/wpdisplay.c ../../dc/wpmap.c ../../dc/itmanager.c ../../dc/itempack.c ../../dc/itmain.c ../../dc/itprocess.c ../../dc/itvisuals.c ../../dc/itdisplay.c ../../dc/itmap.c ../../dc/itstar.c ../../dc/ittomato.c ../../dc/itheart.c ../../dc/itsword.c ../../dc/itbat.c ../../dc/ithammer.c ../../dc/itharisen.c ../../dc/itfflower.c ../../dc/itlgun.c ../../dc/itstarrod.c ../../dc/itgshell.c ../../dc/itrshell.c ../../dc/itnbumper.c ../../dc/itgbumper.c ../../dc/itpowerblock.c ../../dc/itpakkun.c ../../dc/itglucky.c ../../dc/itporygon.c ../../dc/itmarumine.c ../../dc/ithitokage.c ../../dc/itfushigibana.c ../../dc/ittarget.c ../../dc/ittarubomb.c ../../dc/itmsbomb.c ../../dc/itbombhei.c ../../dc/gmrumble.c ../../dc/ftcommontwister.c ../../dc/grhyrule.c ../../dc/grjungle.c ../../dc/ftcommontaru.c ../../dc/grzebes.c ../../dc/gryoster.c ../../dc/grpupupu.c ../../dc/grinishie.c ../../dc/grcastle.c ../../dc/gryamabuki.c ../../dc/grsector.c ../../dc/grbonus3.c hosttest_ftmain.o ../../dc/ftparam.c ../../dc/ftshadow.c \
	    ../../dc/fighter.c ../../dc/assetroot.c \
	    ../../dc/ifcommon.c ../../dc/ifscreenflash.c \
	    hosttest_gmcolscripts.c ../../dc/ftdisplaymain.c \
	    ../../dc/scmanager.c ../../dc/scmanagerdata.c ../../dc/sndres.c \
	    ../../dc/scvsbattle.c \
	    ../../dc/scvsresults.c ../../dc/scautodemo.c ../../dc/scexplain.c \
	    ../../dc/mvending.c ../../dc/scstaffroll.c ../../dc/mvopeningroom.c ../../dc/mvopeningportraits.c ../../dc/mvopeningmario.c ../../dc/mvopeningdonkey.c ../../dc/mvopeningsamus.c ../../dc/mvopeninglink.c ../../dc/mvopeningyoshi.c ../../dc/mvopeningkirby.c ../../dc/mvopeningfox.c ../../dc/mvopeningpikachu.c ../../dc/mvopeningrun.c ../../dc/mvopeningcliff.c ../../dc/mvopeningyamabuki.c ../../dc/mvopeningjungle.c ../../dc/mvopeningyoster.c ../../dc/mvopeningsector.c ../../dc/mvopeningstandoff.c ../../dc/mvopeningclash.c ../../dc/mvopeningnewcomers.c \
	    ../../dc/mntitle.c ../../dc/sprite.c ../../dc/camanim.c \
	    ../../dc/mnmodeselect.c ../../dc/mn1pmode.c ../../dc/mnvsmode.c ../../dc/mnvsoptions.c \
	    ../../dc/mnplayersvs.c ../../dc/mnplayers1ptraining.c \
	    ../../dc/sc1ptrainingmode.c \
	    ../../dc/mnplayers1pgame.c \
	    ../../dc/mnplayers1pbonus.c \
	    ../../dc/sc1pchallenger.c ../../dc/sc1pintro.c ../../dc/sc1pgame.c \
	    ../../dc/sc1pbonusstage.c \
	    ../../dc/sc1pgameboss.c \
	    ../../dc/sc1pstageclear.c ../../dc/sc1pmanager.c \
	    ../../dc/mn1pcontinue.c \
	    ../../dc/mnmaps.c ../../dc/mnmessage.c \
	    ../../dc/mnvsitemswitch.c ../../dc/mndata.c ../../dc/mnvsrecord.c \
	    ../../dc/mncharacters.c \
	    ../../dc/mnsoundtest.c \
	    ../../dc/mnoption.c \
	    ../../dc/mnscreenadjust.c \
	    ../../dc/mnbackupclear.c \
	    ../../dc/mncongra.c \
	    ../../dc/mnnocontroller.c ../../dc/dcmemcard.c \
	    ../../dc/mnstartup.c \
	    ../../dc/scsubsysfighter.c ../../dc/scsubsysdata.c \
	    ../../dc/lbfade.c \
	    ../../dc/lbtransition.c \
	    ../../dc/gmcommon.c ../../dc/gmcamera.c \
	    ../../dc/lbcommon.c ../../dc/taskman.c ../../dc/sysshim.c \
	    ../../dc/vmusave.c ../../dc/vmucard.c ../../dc/dctext.c \
	    ../../dc/vmunotice.c \
	    ../../dc/syaudio.c ../../dc/objdisplay.c \
	    ../../dc/introcrowd.c ../../dc/objmodel.c ../../dc/mpcommon.c ../../dc/mpshim.c \
	    ../../dc/overlay.c ../../dc/clip.c \
	    ../../dc/dma.c ../../dc/lbparticle.c ../../dc/lbpartex.c \
	    ../../dc/lbpdraw.c ../../dc/efdisplay.c ../../dc/efmanager.c \
	    ../../dc/efground.c \
	    sintable.c \
	    gusinf.c gucosf.c \
	    $(HOST_DECOMP_OBJS) hosttest_particlebanks.ld hosttest_wpattrs.ld -lm
ifeq ($(and $(BAKER),$(filter cross,$(HOST32))),cross)
	printf '#!/bin/sh\nexec qemu-i386 -L /usr/i686-linux-gnu "$$(dirname "$$0")/%s.i386" "$$@"\n' $(notdir $(HOST_OUT)) > $(HOST_OUT)
	chmod +x $(HOST_OUT)
endif
	$(HOST_RUN)
