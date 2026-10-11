/* gmrumble.c -- gm/gmrumble.c, verbatim: the Rumble Pak scripted-event
 * engine (fifteen functions, all ported, including the decomp's own
 * "0x801155C4 - Unused?" dead function, kept per this port's byte-for-
 * byte doctrine) plus this file's own DIVERGES replacement for the two
 * halves of sys/controller.c the engine calls by name --
 * syControllerStartRumble/StopRumble/InitRumble.
 *
 * DIVERGES: the N64 side of those three writes a rumble-pak command
 * into a controller's mempak slot over the joybus, through an async
 * message queue (sys/controller.c's own ControllerEvent/osContPak
 * plumbing) this port never carries -- there is no mempak here to write
 * into. The Dreamcast's own vibration peripheral is the Puru Puru
 * (Jump) Pack, a maple sub-device in one of a controller's two
 * expansion slots (KOS's dc/maple/purupuru.h), addressed the same way
 * src/dc/input.c already addresses the controller itself
 * (maple_enum_dev(port, unit)) -- just walking every sub-unit (1-5,
 * unit 0 being the pad) looking for MAPLE_FUNC_PURUPURU instead of
 * assuming unit 0. No pack present is a normal, silent no-op (the same
 * shape a real N64 with no rumble pak inserted has -- the game's own
 * event engine above neither knows nor cares). syControllerInitRumble
 * needs no work of its own: KOS's purupuru_init() runs unconditionally
 * at boot (kernel/arch/dreamcast/hardware/maple/maple_init_shutdown.c,
 * KOS_INIT_FLAG_WEAK(purupuru_init, true), and this port sets no
 * KOS_INIT_FLAGS to turn it off) -- there is no per-port arming step
 * left to do once the driver already exists. Start sends a constant,
 * continuous vibration (motor 1, forward power 7, no decay -- the
 * event engine's own script data is what supplies timing/looping, this
 * function only has an on/off to give it); Stop sends the effect the
 * KOS rumble example itself documents as the correct all-hardware-safe
 * stop (`{.motor = 1}`, every other field zero -- motor 0 is a hardware
 * error on real Puru Puru units, so it can't just be memset to zero).
 *
 * The file has two halves. The first is the engine itself: the
 * event byte-code interpreter (UpdateEventExecute/Queue/CheckEnd,
 * reading GMRumbleEventDefault's 3-bit opcode + 13-bit param pairs),
 * the doubly-linked free-list of GMRumbleLink slots per player
 * (AddLinkAfter, MakeActor's own "brain damage" 2D-array-as-linked-list
 * setup, kept verbatim including the decomp's own admitted-confusing
 * shape and comment), the priority-preemption search
 * (GetEventPriorityRelink, walking dGMRumblePriorities), and the
 * per-tic actor (ActorProcUpdate, on link 13/nGCCommonLinkIDRumble --
 * already a real, shared decomp enum value this port's other files
 * reuse, e.g. nGCCommonLinkIDMagnify). ftparam.c's ftParamMakeRumble and every
 * ftmain/ftcommon/itmsbomb call site under it reach
 * gmRumbleSetPlayerRumbleParams, and gmRumbleMakeActor's link-chain setup
 * is a hard prerequisite
 * (scvsbattle.c's scene setups, and hosttest's main()).
 *
 * The second wires the rest: the two
 * `syControllerInitRumble/StopRumble` loops (mnTitleFuncStart and
 * mnVSResultsStartScene/func_ovl31_8013797C), the winner's own rumble
 * thread (func_ovl31_80137898/80137938), ifcommon.c's three
 * gmRumbleInitPlayers and one gmRumbleResumeProcessAll, and
 * scvsbattle.c's two gmRumbleInitPlayers (the end of each battle,
 * either side of the sudden-death check). Every caller named here is
 * called now.
 *
 * gm/gmdef.h's dGMRumbleEventList[]/dGMRumblePriorities[] are 11
 * entries (rumble_id 0-10, GMRumbleEvent0..10) -- both tables ported
 * with that exact count, matching the decomp's own array literal.
 */
#include <gm/generic.h>
#include <sc/scene.h>

#ifdef FT_HOSTTEST
#include "hoststubs.h"
#else
#include <kos.h>
#include <dc/maple.h>
#include <dc/maple/purupuru.h>
#endif

// // // // // // // // // // // //
//                               //
//       INITIALIZED DATA        //
//                               //
// // // // // // // // // // // //

// 0x8012F1A0
GMRumbleEventDefault dGMRumbleEvent0[] =
{
    { nGMRumbleEventStartRumble, 8000 },
    { nGMRumbleEventEnd,          0 }
};

// 0x8012F1A4
GMRumbleEventDefault dGMRumbleEvent1[] =
{
    { nGMRumbleEventLoopBegin, 8000 },
    { nGMRumbleEventStartRumble,  2 },
    { nGMRumbleEventStopRumble,   2 },
    { nGMRumbleEventLoopEnd,      0 },
    { nGMRumbleEventEnd,          0 } // Needed for 0-pad?
};

// 0x8012F1B0
GMRumbleEventDefault dGMRumbleEvent2[] =
{
    { nGMRumbleEventLoopBegin, 8000 },
    { nGMRumbleEventStartRumble,  2 },
    { nGMRumbleEventStopRumble,   4 },
    { nGMRumbleEventLoopEnd,      0 },
    { nGMRumbleEventEnd,    0 }
};

// 0x8012F1BC
GMRumbleEventDefault dGMRumbleEvent3[] =
{
    { nGMRumbleEventLoopBegin, 8000 },
    { nGMRumbleEventStartRumble,  2 },
    { nGMRumbleEventStopRumble,   8 },
    { nGMRumbleEventLoopEnd,      0 },
    { nGMRumbleEventEnd,          0 }
};

// 0x8012F1C8
GMRumbleEventDefault dGMRumbleEvent4[] =
{
    { nGMRumbleEventStartRumble, 12 },
    { nGMRumbleEventEnd,          0 }
};

// 0x8012F1CC
GMRumbleEventDefault dGMRumbleEvent5[] =
{
    { nGMRumbleEventLoopBegin, 8000 },
    { nGMRumbleEventStartRumble,  2 },
    { nGMRumbleEventStopRumble,   1 },
    { nGMRumbleEventLoopEnd,      0 },
    { nGMRumbleEventEnd,          0 }
};

// 0x8012F1D8
GMRumbleEventDefault dGMRumbleEvent6[] =
{
    { nGMRumbleEventStartRumble,  3 },
    { nGMRumbleEventEnd,          0 }
};

// 0x8012F1DC
GMRumbleEventDefault dGMRumbleEvent7[] =
{
    { nGMRumbleEventLoopBegin, 8000 },
    { nGMRumbleEventStartRumble,  2 },
    { nGMRumbleEventStopRumble,   3 },
    { nGMRumbleEventLoopEnd,      0 },
    { nGMRumbleEventEnd,          0 }
};

// 0x8012F1E8
GMRumbleEventDefault dGMRumbleEvent8[] =
{
    { nGMRumbleEventLoopBegin, 8000 },
    { nGMRumbleEventStartRumble,  1 },
    { nGMRumbleEventStopRumble,   3 },
    { nGMRumbleEventLoopEnd,      0 },
    { nGMRumbleEventEnd,          0 }
};

// 0x8012F1F4
GMRumbleEventDefault dGMRumbleEvent9[] =
{
    { nGMRumbleEventStartRumble,  8 },
    { nGMRumbleEventEnd,          0 }
};

// 0x8012F1F8
GMRumbleEventDefault dGMRumbleEvent10[] =
{
    { nGMRumbleEventStartRumble, 16 },
    { nGMRumbleEventEnd,          0 }
};

// 0x8012F1FC - padding?
GMRumbleEventDefault dGMRumbleEvent11[] =
{
    { nGMRumbleEventEnd,          0 }
};

// 0x8012F200
GMRumbleEventDefault *dGMRumbleEventList[] =
{
    dGMRumbleEvent0,
    dGMRumbleEvent1,
    dGMRumbleEvent2,
    dGMRumbleEvent3,
    dGMRumbleEvent4,
    dGMRumbleEvent5,
    dGMRumbleEvent6,
    dGMRumbleEvent7,
    dGMRumbleEvent8,
    dGMRumbleEvent9,
    dGMRumbleEvent10
};

// 0x8012F22C
u8 dGMRumblePriorities[] =
{
    10, 9,
    5,  5,
    6,  5,
    3,  3,
    3,  3,
    3
};

// // // // // // // // // // // //
//                               //
//   GLOBAL / STATIC VARIABLES   //
//                               //
// // // // // // // // // // // //

// 0x80131870
GMRumbleScript sGMRumbleScripts[GMCOMMON_PLAYERS_MAX * GMRUMBLE_ARRAY_COLS];

// 0x80131960
GMRumbleLink sGMRumbleLinks[GMCOMMON_PLAYERS_MAX * GMRUMBLE_ARRAY_COLS];

// 0x801319F0
GMRumblePlayer sGMRumblePlayers[GMCOMMON_PLAYERS_MAX];

// // // // // // // // // // // //
//                               //
//   DC-SIDE RUMBLE PRIMITIVES   //
//                               //
// // // // // // // // // // // //

#ifndef FT_HOSTTEST
/* Find the Puru Puru sub-device on a controller port, if any. Mirrors
 * input.c's own maple_enum_dev(port, 0) for the pad itself -- a jump
 * pack is a sub-unit (1-5), not the main device (0). */
static maple_device_t *gmRumbleFindPuruPuru(s32 port)
{
    int unit;
    maple_device_t *dev;

    for (unit = 1; unit < MAPLE_UNIT_COUNT; unit++)
    {
        dev = maple_enum_dev(port, unit);

        if ((dev != NULL) && (dev->info.functions & MAPLE_FUNC_PURUPURU))
        {
            return dev;
        }
    }
    return NULL;
}
#endif

#ifdef FT_HOSTTEST
/* Host-only recording, the same "mirror the target's own effect with a
 * counter the test can read" shape the ColAnim env-colour capture uses: there is no purupuru to send to on the
 * host, so this is what proves gmRumbleActorProcUpdate really called
 * Start/Stop for the right port, not just that nothing crashed. */
s32 gSYControllerRumbleStartCount[GMCOMMON_PLAYERS_MAX];
s32 gSYControllerRumbleStopCount[GMCOMMON_PLAYERS_MAX];
#endif

/* sys/controller.c 0x80004474 syControllerStartRumble. DIVERGES: see
 * this file's own header comment. */
void syControllerStartRumble(s32 port)
{
#ifndef FT_HOSTTEST
    maple_device_t *dev = gmRumbleFindPuruPuru(port);

    if (dev != NULL)
    {
        purupuru_effect_t effect = { .cont = true, .motor = 1, .fpow = 7, .freq = 20 };

        purupuru_rumble(dev, &effect);
    }
#else
    gSYControllerRumbleStartCount[port]++;
#endif
}

/* sys/controller.c 0x80004494 syControllerStopRumble. DIVERGES: see
 * this file's own header comment. */
void syControllerStopRumble(s32 port)
{
#ifndef FT_HOSTTEST
    maple_device_t *dev = gmRumbleFindPuruPuru(port);

    if (dev != NULL)
    {
        purupuru_effect_t effect = { .motor = 1 };

        purupuru_rumble(dev, &effect);
    }
#else
    gSYControllerRumbleStopCount[port]++;
#endif
}

/* sys/controller.c 0x800044B4 syControllerInitRumble. DIVERGES: see
 * this file's own header comment -- nothing to arm, the driver is
 * already registered by the time this is ever called. */
void syControllerInitRumble(s32 port)
{
    (void)port;
}

// // // // // // // // // // // //
//                               //
//           FUNCTIONS           //
//                               //
// // // // // // // // // // // //

// 0x80114E30
sb32 gmRumbleUpdateEventExecute(ub8 *is_active, GMRumbleScript *p_script, s32 player)
{
    while (p_script->rumble_status == 0)
    {
        switch (gmRumbleEventCast(p_script->p_event, GMRumbleEventDefault)->opcode)
        {
        case nGMRumbleEventEnd:
            p_script->p_event = dGMRumbleEventList[p_script->rumble_id];
            break;

        case nGMRumbleEventStartRumble:
            p_script->rumble_status = gmRumbleEventCast(p_script->p_event, GMRumbleEventDefault)->param;

            gmRumbleEventAdvance(p_script->p_event, GMRumbleEventDefault);

            if (*is_active == FALSE)
            {
                syControllerStartRumble(player);

                *is_active = p_script->is_rumble_active = TRUE;
            }
            break;

        case nGMRumbleEventStopRumble:
            p_script->rumble_status = gmRumbleEventCast(p_script->p_event, GMRumbleEventDefault)->param;

            gmRumbleEventAdvance(p_script->p_event, GMRumbleEventDefault);

            if (*is_active != FALSE)
            {
                syControllerStopRumble(player);

                *is_active = p_script->is_rumble_active = FALSE;
            }
            break;

        case nGMRumbleEventLoopBegin:
            p_script->loop_count = gmRumbleEventCast(p_script->p_event, GMRumbleEventDefault)->param;

            gmRumbleEventAdvance(p_script->p_event, GMRumbleEventDefault);

            p_script->p_goto = p_script->p_event;
            break;

        case nGMRumbleEventLoopEnd:
            if (--p_script->loop_count != 0)
            {
                p_script->p_event = p_script->p_goto;
            }
            else gmRumbleEventAdvance(p_script->p_event, GMRumbleEventDefault);
        }
    }
    return FALSE;
}

// 0x80114F98
sb32 gmRumbleUpdateEventQueue(ub8 *is_active, GMRumbleScript *p_script)
{
    while (p_script->rumble_status == 0)
    {
        switch (gmRumbleEventCast(p_script->p_event, GMRumbleEventDefault)->opcode)
        {
        case nGMRumbleEventEnd:
            p_script->p_event = dGMRumbleEventList[p_script->rumble_id];
            break;

        case nGMRumbleEventStartRumble:
            p_script->rumble_status = gmRumbleEventCast(p_script->p_event, GMRumbleEventDefault)->param;

            gmRumbleEventAdvance(p_script->p_event, GMRumbleEventDefault);

            p_script->is_rumble_active = TRUE;
            break;

        case nGMRumbleEventStopRumble:
            p_script->rumble_status = gmRumbleEventCast(p_script->p_event, GMRumbleEventDefault)->param;

            gmRumbleEventAdvance(p_script->p_event, GMRumbleEventDefault);

            p_script->is_rumble_active = FALSE;
            break;

        case nGMRumbleEventLoopBegin:
            p_script->loop_count = gmRumbleEventCast(p_script->p_event, GMRumbleEventDefault)->param;

            gmRumbleEventAdvance(p_script->p_event, GMRumbleEventDefault);

            p_script->p_goto = p_script->p_event;
            break;

        case nGMRumbleEventLoopEnd:
            if (--p_script->loop_count != 0)
            {
                p_script->p_event = p_script->p_goto;
            }
            else gmRumbleEventAdvance(p_script->p_event, GMRumbleEventDefault);
        }
    }
    return FALSE;
}

// 0x80115090 - I suck at linked lists, this name might be wrong
void gmRumbleAddLinkAfter(GMRumblePlayer *rplayer, GMRumbleLink *rlink)
{
    GMRumbleLink *rprev = rplayer->rlink->rprev;
    GMRumbleLink *rnext = rlink->rnext;

    rlink->p_script->p_event = NULL;

    if (rnext != NULL)
    {
        if (rlink->rprev->rnext == NULL)
        {
            rplayer->rlink = rlink->rnext;
        }
        else
        {
            rlink->rprev->rnext = rnext;
            rnext->rprev = rlink->rprev;
        }
        rlink->rnext = rprev->rnext;
        rprev->rnext = rlink;
        rlink->rprev = rprev;
        rplayer->rlink->rprev = rlink;
    }
}

// 0x801150F0
sb32 gmRumbleUpdateEventCheckEnd(GMRumbleScript *p_script)
{
    void *p_event = p_script->p_event;
    ub32 is_terminate = p_script->rumble_status;

    while (is_terminate == FALSE)
    {
        switch (gmRumbleEventCast(p_event, GMRumbleEventDefault)->opcode)
        {
        case nGMRumbleEventEnd:
            return TRUE;
            break;

        case nGMRumbleEventStartRumble:
        case nGMRumbleEventStopRumble:
            is_terminate = TRUE;
            break;

        case nGMRumbleEventLoopBegin:
            gmRumbleEventAdvance(p_event, GMRumbleEventDefault);
            break;

        case nGMRumbleEventLoopEnd:
            if (p_script->loop_count == 1)
            {
                gmRumbleEventAdvance(p_event, GMRumbleEventDefault);
            }
            else p_event = p_script->p_goto;
            break;
        }
    }
    return FALSE;
}

// 0x80115170
void gmRumbleUpdateEventLinks(GMRumblePlayer *rplayer)
{
    GMRumbleLink *rlink = rplayer->rlink;

    while (rlink != NULL)
    {
        GMRumbleScript *p_script = rlink->p_script;

        if (p_script->p_event == NULL) break;

        if ((p_script->rumble_timer == 0) || ((p_script->rumble_timer < 0) && (gmRumbleUpdateEventCheckEnd(p_script) != FALSE)))
        {
            gmRumbleAddLinkAfter(rplayer, rlink);

            rlink = rplayer->rlink;
        }
        else rlink = rlink->rnext;
    }
}

// 0x801151F4
sb32 gmRumbleGetMotorUpdateStatus(ub8 *is_active, GMRumbleScript *p_script, s32 player)
{
    if (p_script->p_event == NULL)
    {
        syControllerInitRumble(player);
        syControllerStopRumble(player);

        if (*is_active != FALSE)
        {
            *is_active = FALSE;
        }
        return TRUE;
    }
    else if (p_script->is_rumble_active != *is_active)
    {
        if (*is_active != FALSE)
        {
            syControllerStopRumble(player);

            *is_active = FALSE;
        }
        else
        {
            syControllerStartRumble(player);

            *is_active = TRUE;
        }
    }
    return FALSE;
}

// 0x80115298
void gmRumbleActorProcUpdate(GObj *rumble_gobj)
{
    s32 player;
    GMRumbleScript *p_script;
    GMRumbleLink *rlink;

    for (player = 0; player < ARRAY_COUNT(sGMRumblePlayers); player++)
    {
        rlink = sGMRumblePlayers[player].rlink;

        if (rlink->p_script->p_event != NULL)
        {
            gmRumbleUpdateEventLinks(&sGMRumblePlayers[player]);

            rlink = sGMRumblePlayers[player].rlink;
            p_script = rlink->p_script;

            if (gmRumbleGetMotorUpdateStatus(&sGMRumblePlayers[player].is_active, p_script, player) == FALSE)
            {
                if (gmRumbleUpdateEventExecute(&sGMRumblePlayers[player].is_active, p_script, player) == FALSE)
                {
                    p_script->rumble_status--;

                    if (p_script->rumble_timer > 0)
                    {
                        p_script->rumble_timer--;
                    }
                }
                rlink = rlink->rnext;

                while (rlink != NULL)
                {
                    p_script = rlink->p_script;

                    if (p_script->p_event == NULL) break;

                    if (gmRumbleUpdateEventQueue(&sGMRumblePlayers[player].is_active, p_script) == FALSE)
                    {
                        p_script->rumble_status--;

                        if (p_script->rumble_timer > 0)
                        {
                            p_script->rumble_timer--;
                        }
                    }
                    rlink = rlink->rnext;
                }
            }
        }
    }
}

// 0x801153B8
sb32 gmRumbleCheckSetEventID(GMRumblePlayer *rplayer, s32 rumble_id, s32 rumble_timer)
{
    GMRumbleLink *rlink = rplayer->rlink;

    while (rlink != NULL)
    {
        if (rlink->p_script != NULL)
        {
            GMRumbleScript *p_script = rlink->p_script;

            if ((p_script->p_event != NULL) && (p_script->rumble_id == rumble_id))
            {
                if (rumble_timer != 0)
                {
                    if ((p_script->rumble_timer > 0) && (p_script->rumble_timer < rumble_timer))
                    {
                        p_script->rumble_timer = rumble_timer;

                        return TRUE;
                    }
                }
                else
                {
                    p_script->rumble_status = p_script->loop_count = 0;
                    p_script->p_event = dGMRumbleEventList[rumble_id];

                    return TRUE;
                }
            }
        }
        else return FALSE;

        rlink = rlink->rnext;
    }
    return FALSE;
}

// 0x80115458
GMRumbleScript* gmRumbleGetEventPriorityRelink(GMRumblePlayer *rplayer, s32 rumble_id)
{
    GMRumbleScript *p_script;
    GMRumbleLink *rlink_main;
    GMRumbleLink *rprev;
    GMRumbleLink *rlink_curr;

    rlink_main = rplayer->rlink;
    rprev = rlink_main->rprev;
    rlink_curr = rlink_main;

    while (rlink_curr != NULL)
    {
        p_script = rlink_curr->p_script;

        if (p_script->p_event != NULL)
        {
            if (dGMRumblePriorities[rumble_id] >= dGMRumblePriorities[p_script->rumble_id])
            {
                if (rlink_curr == rlink_main)
                {
                    rprev->rprev->rnext = rprev->rnext;

                    rplayer->rlink = rprev;

                    rprev->rnext = rlink_curr;
                    rlink_curr->rprev = rprev;

                    return rprev->p_script;
                }
                else if (rlink_curr != rprev)
                {
                    rprev->rprev->rnext = rprev->rnext;

                    rplayer->rlink->rprev = rprev->rprev;

                    rprev->rprev = rlink_curr->rprev;
                    rprev->rprev->rnext = rprev;

                    rprev->rnext = rlink_curr;
                    rlink_curr->rprev = rprev;

                    return rprev->p_script;
                }
                else return rprev->p_script;
            }
        }
        else return p_script;

        rlink_curr = rlink_curr->rnext;
    }
    return NULL;
}

// 0x80115530
void gmRumbleSetPlayerRumbleParams(s32 player, s32 rumble_id, s32 rumble_timer)
{
    s32 unused[4];

    if (gmRumbleCheckSetEventID(&sGMRumblePlayers[player], rumble_id, rumble_timer) == FALSE)
    {
        GMRumbleScript *p_script = gmRumbleGetEventPriorityRelink(&sGMRumblePlayers[player], rumble_id);

        if (p_script != NULL)
        {
            if (rumble_timer == 0)
            {
                p_script->rumble_timer = -1;
            }
            else p_script->rumble_timer = rumble_timer;

            p_script->rumble_id = rumble_id;

            p_script->is_rumble_active = FALSE;

            p_script->rumble_status = p_script->loop_count = 0;

            p_script->p_goto = NULL;

            p_script->p_event = dGMRumbleEventList[rumble_id];
        }
    }
}

// 0x801155C4 - Unused?
void func_ovl2_801155C4(s32 player)
{
    GMRumbleLink *rlink;

    syControllerInitRumble(player);
    syControllerStopRumble(player);

    if (sGMRumblePlayers[player].is_active != FALSE)
    {
        sGMRumblePlayers[player].is_active = FALSE;
    }
    rlink = sGMRumblePlayers[player].rlink;

    while (rlink != NULL)
    {
        rlink->p_script->p_event = NULL;

        rlink = rlink->rnext;
    }
}

// 0x80115630
void gmRumbleStopRumbleID(s32 player, s32 rumble_id)
{
    GMRumbleLink *rlink = sGMRumblePlayers[player].rlink;

    if (rlink->p_script->p_event != NULL)
    {
        while (rlink != NULL)
        {
            GMRumbleScript *p_script = rlink->p_script;

            if (p_script->p_event == NULL)
            {
                break;
            }
            if (rumble_id == p_script->rumble_id)
            {
                gmRumbleAddLinkAfter(&sGMRumblePlayers[player], rlink);

                rlink = sGMRumblePlayers[player].rlink;
            }
            else rlink = rlink->rnext;
        }
        gmRumbleGetMotorUpdateStatus(&sGMRumblePlayers[player].is_active, sGMRumblePlayers[player].rlink->p_script, player);
    }
}

// 0x801156E4
void gmRumbleMakeActor(void)
{
    // This is an actual brain damage function. Not only does it not match with 80131870 and 80131960 arranged as 2D arrays
    // and therefore mandating iterator gymnastics to track each player's rumble struct,
    // it also requires this exact setup seen below to match, line for line, except the second if(TRUE) hack statement.
    // I am not sure what HAL did here... nor do I want to know at this point.

    GMRumbleScript *p_script;
    GMRumbleLink *rlink;
    GMRumbleLink *rlink_end;
    GMRumblePlayer *rplayer;
    s32 i;
    s32 j;
    s32 k;
    s32 l;
    s32 player;

    gcAddGObjProcess(gcMakeGObjSPAfter(nGCCommonKindRumble, NULL, nGCCommonLinkIDRumble, GOBJ_PRIORITY_DEFAULT), gmRumbleActorProcUpdate, nGCProcessKindFunc, 0);

    for (j = i = player = 0; i < (ARRAY_COUNT(sGMRumbleScripts) + ARRAY_COUNT(sGMRumbleLinks)) / 2; j += GMRUMBLE_ARRAY_COLS, i += GMRUMBLE_ARRAY_COLS, player++)
    {
        if (TRUE)
        {
            rplayer = &sGMRumblePlayers[player];
        }
        rlink = &sGMRumbleLinks[j];
        p_script = &sGMRumbleScripts[i];

        rplayer->is_active = FALSE;
        rplayer->rlink = rlink;

        rlink->p_script = p_script;
        p_script->p_goto = NULL;
        p_script->p_event = NULL;

        if (TRUE)
        {
            rlink = &sGMRumbleLinks[j];
        }
        for (k = 1, l = j + 1; k < GMRUMBLE_ARRAY_COLS; k++, l++)
        {
            rlink->rnext = &sGMRumbleLinks[l];
            sGMRumbleLinks[l].rprev = rlink;

            rlink_end = &sGMRumbleLinks[l];

            sGMRumbleLinks[l].p_script = &sGMRumbleScripts[l];
            sGMRumbleScripts[l].p_goto = NULL;
            sGMRumbleScripts[l].p_event = NULL;

            rlink = &sGMRumbleLinks[l];
        }
        rlink_end->rnext = NULL;
        sGMRumblePlayers[player].rlink->rprev = rlink_end;
    }
}

// 0x801157EC
void gmRumbleInitPlayers(void)
{
    s32 player;

    for (player = 0; player < GMCOMMON_PLAYERS_MAX; player++)
    {
        syControllerInitRumble(player);
        syControllerStopRumble(player);
    }
}

// 0x80115834
void gmRumbleResumeProcessAll(void)
{
    GObj *rumble_gobj = gGCCommonLinks[nGCCommonLinkIDRumble];

    while (rumble_gobj != NULL)
    {
        if (rumble_gobj->id == nGCCommonKindRumble)
        {
            gcResumeGObjProcessAll(rumble_gobj);
        }
        rumble_gobj = rumble_gobj->link_next;
    }
}
