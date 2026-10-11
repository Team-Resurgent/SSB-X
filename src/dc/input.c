/* input.c -- port of sys/controller.c's read/update pair.
 *
 * The edge and auto-repeat logic is transcribed from
 * syControllerReadDeviceData; the latch from syControllerUpdateGlobalData.
 * Field names follow the decomp's ControllerInfo comments rather than its
 * unkNN offsets.
 *
 * Device index is the physical port, as it is on the N64: entry i of
 * gSYControllerDevices is the pad in Dreamcast port i (A, B, C, D), and
 * gSYControllerDeviceStatuses is the game's own dense map from k-th
 * connected to port, built by syControllerUpdateDeviceIndexes.
 * maple_enum_dev(i, 0) is the port itself (maple_enum_type would
 * enumerate connected pads densely, so pads in A and C would arrive as
 * ports 0 and 1), so holes in the map mean what they mean on a console.
 */
#include "input.h"

#include <string.h>

#include <sc/scdef.h>

/* ControllerInfo, minus the rumble/status plumbing (unk10/14/18 become
 * named repeat fields; 30-then-5 are the game's init values). */
typedef struct
{
    uint16_t held;      /* unk00: buttons down at last read */
    uint16_t tap;       /* unk02: press edges this read */
    uint16_t tap_acc;   /* unk04: press edges since last latch */
    uint16_t rpt;       /* unk06: auto-repeat fires this read */
    uint16_t rpt_acc;   /* unk08: repeats since last latch */
    uint16_t rel;       /* unk0A: release edges this read */
    uint16_t rel_acc;   /* unk0C: releases since last latch */
    int8_t stick_x;     /* unk0E */
    int8_t stick_y;     /* unk0F */
    int32_t rpt_delay;  /* unk10: reads before the first repeat (30) */
    int32_t rpt_rate;   /* unk14: reads between repeats after that (5) */
    int32_t rpt_count;  /* unk18: countdown to the next repeat */
    uint8_t err;        /* unk1C: nonzero = no controller this read */
} SYInputDesc;

static SYInputDesc sDesc[MAXCONTROLLERS];

SYController gSYControllerDevices[MAXCONTROLLERS];
SYController gSYControllerMain;
uint32_t gSYControllerConnectedNum;

/* sys/controller.c:39. The game's device-to-player table: entry k is
 * the port of the k-th connected pad, -1 past the last. The menus read
 * their pads through it (sc/scsubsys/scsubsyscontroller.c
 * scSubsysControllerCheckConnected); the battle reads the ports
 * directly. The ports are physical, so this is a real
 * map with real holes in it: pads in A and C give { 0, 2, -1, -1 }. */
s8 gSYControllerDeviceStatuses[MAXCONTROLLERS];

void sy_input_init(void)
{
    int i;

    memset(sDesc, 0, sizeof(sDesc));
    memset(gSYControllerDevices, 0, sizeof(gSYControllerDevices));
    memset(&gSYControllerMain, 0, sizeof(gSYControllerMain));
    gSYControllerConnectedNum = 0;

    for (i = 0; i < MAXCONTROLLERS; i++)
    {
        sDesc[i].rpt_delay = 30;
        sDesc[i].rpt_rate = 5;
        sDesc[i].rpt_count = 30;
        sDesc[i].err = 1;
        gSYControllerDeviceStatuses[i] = -1;
    }
}

void sy_input_feed(int port, uint16_t buttons, int8_t stick_x,
                   int8_t stick_y)
{
    SYInputDesc *d = &sDesc[port];

    d->err = 0;
    d->tap = (uint16_t)((buttons ^ d->held) & buttons);
    d->rel = (uint16_t)((buttons ^ d->held) & d->held);

    if (buttons ^ d->held)
    {
        d->rpt = d->tap;
        d->rpt_count = d->rpt_delay;
    }
    else
    {
        d->rpt_count--;
        if (d->rpt_count > 0)
        {
            d->rpt = 0;
        }
        else
        {
            d->rpt = buttons;
            d->rpt_count = d->rpt_rate;
        }
    }
    d->held = buttons;
    d->stick_x = stick_x;
    d->stick_y = stick_y;
    d->tap_acc |= d->tap;
    d->rel_acc |= d->rel;
    d->rpt_acc |= d->rpt;
}

void sy_input_feed_err(int port)
{
    sDesc[port].err = 1;
}

/* Whether the last sy_input_poll found a real device on this port -- see
 * input.h. */
sb32 sy_input_port_present(int port)
{
    return (sDesc[port].err == 0) ? TRUE : FALSE;
}

void sy_input_update(void)
{
    int i;
    uint32_t n = 0;

    for (i = 0; i < MAXCONTROLLERS; i++)
    {
        if (sDesc[i].err == 0)
        {
            gSYControllerDevices[i].button_hold = sDesc[i].held;
            gSYControllerDevices[i].button_tap = sDesc[i].tap_acc;
            gSYControllerDevices[i].button_release = sDesc[i].rel_acc;
            gSYControllerDevices[i].button_update = sDesc[i].rpt_acc;
            gSYControllerDevices[i].stick_range.x = sDesc[i].stick_x;
            gSYControllerDevices[i].stick_range.y = sDesc[i].stick_y;
            sDesc[i].tap_acc = sDesc[i].rpt_acc = sDesc[i].rel_acc = 0;
            /* sys/controller.c:74-79 syControllerUpdateDeviceIndexes */
            gSYControllerDeviceStatuses[n] = (s8)i;
            n++;
        }
    }
    for (i = (int)n; i < MAXCONTROLLERS; i++)
    {
        gSYControllerDeviceStatuses[i] = -1;
    }
    gSYControllerConnectedNum = n;
    /* sys/controller.c:188: the system pad is the *first connected* one,
     * not port 0 -- any pad drives the menus when the ports before it
     * are empty. ---- DIVERGES: no pad at all ----------------------
     * With nothing connected gSYControllerDeviceStatuses[0] is -1 and
     * the game indexes gSYControllerDevices[-1], reading whatever the
     * N64's linker put before the array. The port leaves gSYControllerMain
     * holding its last values instead, which is what the rest of this
     * function does for an unplugged port. */
    if (gSYControllerDeviceStatuses[0] >= 0)
    {
        gSYControllerMain = gSYControllerDevices[gSYControllerDeviceStatuses[0]];
    }
}

/* The port's own: what the D-pad sends -- see input.h.
 *
 * The Dreamcast pad has no L and no C-left/C-right, and the game reads
 * the D-pad only in menus (no *_JPAD under ft/, and the Training menu
 * runs while the battle is paused). So the D-pad stands in for the
 * missing buttons where they matter: the four C-buttons on the
 * character selects (mnplayersvs.c's costume pick is one C-button per
 * colour), and L -- the taunt, ftcommonappeal.c's button_mask_l -- while
 * a match is running. A paused bonus stage gets L too, for ifcommon.c's
 * "L to retry". The D-pad's own bits are kept in every context. */
int sy_input_dpad_context(int scene, int game_status)
{
    switch (scene)
    {
    case nSCKindPlayersVS:
    case nSCKind1PGamePlayers:
    case nSCKindPlayers1PTraining:
    case nSCKind1PBonus1Players:
    case nSCKind1PBonus2Players:
        return SY_DPAD_SELECT;

    case nSCKindVSBattle:
    case nSCKind1PGame:
    case nSCKind1PBonusStage:
    case nSCKind1PTrainingMode:
        if (game_status == nSCBattleGameStatusGo)
            return SY_DPAD_TAUNT;
        if ((game_status == nSCBattleGameStatusPause) &&
            (scene == nSCKind1PBonusStage))
            return SY_DPAD_TAUNT;
        return SY_DPAD_PLAIN;

    default:
        return SY_DPAD_PLAIN;
    }
}

uint16_t sy_input_dpad_remap(uint16_t btn, int ctx)
{
    if (ctx == SY_DPAD_SELECT)
    {
        if (btn & N64_J_UP)
            btn |= N64_C_UP;
        if (btn & N64_J_RIGHT)
            btn |= N64_C_RIGHT;
        if (btn & N64_J_DOWN)
            btn |= N64_C_DOWN;
        if (btn & N64_J_LEFT)
            btn |= N64_C_LEFT;
    }
    else if (ctx == SY_DPAD_TAUNT)
    {
        if (btn & (N64_J_UP | N64_J_DOWN | N64_J_LEFT | N64_J_RIGHT))
            btn |= N64_L;
    }
    return btn;
}

#ifndef SY_INPUT_HOST

#include <kos.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>

#include <sc/scmanager.h>
#include <sc/sctypes.h>

/* DC face buttons to the SSB64 scheme: A attack, B special, C jump,
 * Z shield, R grab.  X sits left of A like the N64's B, so it takes
 * special; B and Y become C-down and C-up (both jump); the triggers
 * take Z and R (the analog halves squashed to digital at half travel);
 * the D-pad maps straight across, and sy_input_dpad_remap lends it to
 * the buttons the pad lacks (taunt, C-left/C-right). */
static const struct
{
    uint32_t dc;
    uint16_t n64;
} kPadMap[] = {
    { CONT_A, N64_A },
    { CONT_X, N64_B },
    { CONT_Y, N64_C_UP },
    { CONT_B, N64_C_DOWN },
    { CONT_START, N64_START },
    { CONT_DPAD_UP, N64_J_UP },
    { CONT_DPAD_DOWN, N64_J_DOWN },
    { CONT_DPAD_LEFT, N64_J_LEFT },
    { CONT_DPAD_RIGHT, N64_J_RIGHT },
};

void sy_input_poll(void)
{
    int i;

    for (i = 0; i < MAXCONTROLLERS; i++)
    {
        /* The port, not the i-th connected pad: unit 0 is a Dreamcast
         * port's base device, and a port with nothing in it -- or with
         * something that is not a controller -- reads as an error, the
         * way an empty N64 port does (see the file comment). */
        maple_device_t *dev = maple_enum_dev(i, 0);
        cont_state_t *st;
        uint16_t btn = 0;
        int j, sx, sy;

        if (dev == NULL ||
            !(dev->info.functions & MAPLE_FUNC_CONTROLLER) ||
            (st = maple_dev_status(dev)) == NULL)
        {
            sy_input_feed_err(i);
            continue;
        }
        for (j = 0; j < (int)(sizeof(kPadMap) / sizeof(kPadMap[0])); j++)
        {
            if (st->buttons & kPadMap[j].dc)
                btn |= kPadMap[j].n64;
        }
        if (st->ltrig >= 128)
            btn |= N64_Z;
        if (st->rtrig >= 128)
            btn |= N64_R;
        btn = sy_input_dpad_remap(btn,
            sy_input_dpad_context(gSCManagerSceneData.scene_curr,
                                  (gSCManagerBattleState != NULL) ?
                                  gSCManagerBattleState->game_status : -1));

        /* maple sticks run -128..127; the N64 stick reaches about +/-80
         * and the game's thresholds assume that range. */
        sx = st->joyx * 80 / 128;
        sy = -st->joyy * 80 / 128; /* maple +y is down; N64 +y is up */
        sy_input_feed(i, btn, (int8_t)sx, (int8_t)sy);
    }
}

/* The port's own: which physical ports carry a pad, said once and then
 * only when it changes. Nothing in the game wants this -- an N64 has
 * four ports and a player can see them -- but a Dreamcast port that
 * reads its pads through maple has to be able to show that A, B and D
 * arrive as ports 0, 1 and 3 with a hole at 2, which no amount of gameplay makes visible.
 *
 * It reads the read, not the latch: called between sy_input_poll and
 * sy_input_update, it is the machine's own port map, and a stand-in
 * feeding a port it has no pad for (src/dc/db.c's) is not in it. */
void sy_input_report_ports(void)
{
    static int last = -1;
    char map[MAXCONTROLLERS + 1];
    int i, n = 0, count = 0, first = -1;

    for (i = 0; i < MAXCONTROLLERS; i++)
    {
        if (sy_input_port_present(i))
        {
            map[i] = (char)('A' + i);
            n |= 1 << i;
            count++;
            if (first < 0)
            {
                first = i;
            }
        }
        else map[i] = '.';
    }
    map[MAXCONTROLLERS] = '\0';

    if (n != last)
    {
        last = n;
        dbglog(DBG_INFO, "pads: %d on %s -- first is port %d\n",
               count, map, first);
    }
}

/* sys/controller.c syControllerFuncRead -- see input.h. */
void syControllerFuncRead(void)
{
    sy_input_poll();
    sy_input_report_ports();
    sy_input_update();
}

#endif /* !SY_INPUT_HOST */
