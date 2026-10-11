/* kos.h -- Xbox shim for <kos.h>, KallistiOS's umbrella header.
 *
 * Platform/xbox is FIRST on the include path, so every `#include <kos.h>`
 * and `<kos/...>`, `<arch/...>`, `<dc/...>` in the Dreamcast port resolves
 * to these headers. They declare the slice of KallistiOS the port uses,
 * with KallistiOS's own names, values and struct layouts; the Xbox
 * implementations are the Platform/xbox/*.c and *.cpp files:
 *
 *   kos_sys_xbox.c       timers, the program break, irq, rtc, abort, video
 *   kos_thread_xbox.c    threads, mutexes, semaphores, condition variables
 *   kos_log_xbox.c       dbglog / dbgio (a ring drained to xbdm)
 *   kos_fs_xbox.c        fs_* (/cd and /rd -> D:\data)
 *   kos_maple_xbox.c     maple bus: controllers and jump packs (XInput)
 *   kos_vmu_xbox.c       the memory card (vmufs over the Xbox save game)
 *   kos_vmu_pkg.c        VMS packages (KallistiOS's builder and parser)
 *   pvr_xbox.cpp         the PowerVR API on Direct3D 8
 *   aica/                the AICA: sound RAM, registers, the port's ARM
 *                        firmware on the x86, and the mixer (DirectSound)
 *
 * Nothing here emulates more of KallistiOS than the port calls: a new call
 * fails to compile or link, which is the intent. */
#ifndef XBOX_KOS_H
#define XBOX_KOS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <time.h>

#include <kos/cdefs.h>
#include <arch/types.h>
#include <arch/arch.h>
#include <arch/irq.h>
#include <arch/timer.h>
#include <arch/stack.h>
#include <arch/rtc.h>
#include <kos/thread.h>
#include <kos/mutex.h>
#include <kos/sem.h>
#include <kos/cond.h>
#include <kos/fs.h>
#include <kos/dbglog.h>
#include <kos/dbgio.h>
#include <kos/mm.h>
#include <dc/video.h>
#include <dc/pvr.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <dc/maple/purupuru.h>
#include <dc/maple/vmu.h>
#include <dc/vmufs.h>
#include <dc/vmu_pkg.h>
#include <dc/sound/sound.h>
#include <dc/spu.h>
#include <dc/g2bus.h>
#include <dc/sq.h>

#endif /* XBOX_KOS_H */
