/* input_hosttest.c -- feeds synthetic read sequences through the ported
 * controller state machine (src/dc/input.c, built with -DSY_INPUT_HOST)
 * and asserts the N64 semantics:
 *
 *   - tap fires exactly on the press edge, release on the release edge;
 *   - button_update (auto-repeat) fires on the press, again 30 reads
 *     later, then every 5 reads -- the trace hand-derived from
 *     syControllerReadDeviceData: fires at reads 0, 30, 35, 40, ...;
 *   - any button change resets the repeat countdown;
 *   - edges accumulate across reads until sy_input_update() latches and
 *     clears them (the 60 Hz read / 30 fps consume split);
 *   - a port in error keeps its last latched values and does not count
 *     as connected;
 *   - the device index is the physical port, so the statuses table is
 *     the game's dense map with real holes in it, and gSYControllerMain
 *     follows the first *connected* pad rather than port 0
 *     (syControllerUpdateGlobalData's own last six lines);
 *   - the port's D-pad remap: the four C-buttons (costumes) on every
 *     character select, L (taunt) in a running match and a paused bonus
 *     stage, and the plain D-pad everywhere else.
 */
#include <stdio.h>
#include <stdlib.h>

#include "input.h"

#include <sc/scdef.h>

static int nfail;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            nfail++; \
        } \
    } while (0)

/* One read then one latch: the 60 Hz case. */
static void step(uint16_t btn, int8_t sx, int8_t sy)
{
    sy_input_feed(0, btn, sx, sy);
    sy_input_update();
}

static void test_edges(void)
{
    sy_input_init();
    step(0, 0, 0);
    CHECK(gSYControllerMain.button_tap == 0);

    step(N64_A, 0, 0);
    CHECK(gSYControllerMain.button_tap == N64_A);
    CHECK(gSYControllerMain.button_hold == N64_A);
    CHECK(gSYControllerMain.button_update == N64_A);
    CHECK(gSYControllerMain.button_release == 0);

    step(N64_A, 0, 0);
    CHECK(gSYControllerMain.button_tap == 0);
    CHECK(gSYControllerMain.button_hold == N64_A);

    step(0, 0, 0);
    CHECK(gSYControllerMain.button_release == N64_A);
    CHECK(gSYControllerMain.button_hold == 0);
    CHECK(gSYControllerMain.button_tap == 0);
}

static void test_repeat_cadence(void)
{
    int t, fires = 0;

    sy_input_init();
    step(0, 0, 0);
    for (t = 0; t <= 45; t++)
    {
        step(N64_J_DOWN, 0, 0);
        if (gSYControllerMain.button_update & N64_J_DOWN)
        {
            int expect = (t == 0 || t == 30 || t == 35 || t == 40 ||
                          t == 45);
            CHECK(expect);
            fires++;
        }
    }
    CHECK(fires == 5);
}

static void test_repeat_reset_on_change(void)
{
    int t;

    sy_input_init();
    step(0, 0, 0);
    for (t = 0; t < 20; t++)
        step(N64_J_DOWN, 0, 0);
    /* A second button pressed at read 20: change resets the countdown, and
     * update carries only the *new* press (the game's rpt = tap). */
    step(N64_J_DOWN | N64_A, 0, 0);
    CHECK(gSYControllerMain.button_update == N64_A);
    CHECK(gSYControllerMain.button_tap == N64_A);
    CHECK(gSYControllerMain.button_hold == (N64_J_DOWN | N64_A));
    /* Held unchanged from here: next fire is a full 30 reads later and
     * carries everything held. */
    for (t = 1; t < 30; t++)
    {
        step(N64_J_DOWN | N64_A, 0, 0);
        CHECK(gSYControllerMain.button_update == 0);
    }
    step(N64_J_DOWN | N64_A, 0, 0);
    CHECK(gSYControllerMain.button_update == (N64_J_DOWN | N64_A));
}

static void test_accumulate(void)
{
    sy_input_init();
    step(0, 0, 0);
    /* Three reads between latches: tap on read 1, release on read 3 --
     * one game frame must see both edges. */
    sy_input_feed(0, N64_B, 0, 0);
    sy_input_feed(0, N64_B, 0, 0);
    sy_input_feed(0, 0, 0, 0);
    sy_input_update();
    CHECK(gSYControllerMain.button_tap == N64_B);
    CHECK(gSYControllerMain.button_release == N64_B);
    CHECK(gSYControllerMain.button_hold == 0);
    /* And they were cleared by the latch. */
    sy_input_feed(0, 0, 0, 0);
    sy_input_update();
    CHECK(gSYControllerMain.button_tap == 0);
    CHECK(gSYControllerMain.button_release == 0);
}

static void test_err_port(void)
{
    sy_input_init();
    step(N64_A, 33, -44);
    CHECK(gSYControllerConnectedNum == 1);
    CHECK(gSYControllerMain.stick_range.x == 33 && gSYControllerMain.stick_range.y == -44);
    /* Unplugged: latch skipped, last values stay, count drops. */
    sy_input_feed_err(0);
    sy_input_update();
    CHECK(gSYControllerConnectedNum == 0);
    CHECK(gSYControllerMain.button_hold == N64_A);
    CHECK(gSYControllerMain.stick_range.x == 33);
    /* Port 1 alive while port 0 is dead. The system pad is the first
     * *connected* one (sys/controller.c:188), so it is port 1's now. */
    sy_input_feed(1, N64_START, 0, 0);
    sy_input_update();
    CHECK(gSYControllerConnectedNum == 1);
    CHECK(gSYControllerDevices[1].button_tap == N64_START);
    CHECK(gSYControllerDeviceStatuses[0] == 1);
    CHECK(gSYControllerMain.button_tap == N64_START);
}

/* The device index is the physical port: a pad in port C with nothing in B
 * is player 3, not player 2. sy_input_poll is what makes the index physical
 * (maple_enum_dev, KOS-only), and the table, count and system pad all follow
 * from the game's own arithmetic over ports with holes in them. */
static void test_port_identity(void)
{
    int i;

    sy_input_init();

    /* Pads in A, B and D; nothing in C. */
    sy_input_feed(0, 0, 0, 0);
    sy_input_feed(1, 0, 0, 0);
    sy_input_feed(3, N64_A, 0, 0);
    sy_input_update();
    CHECK(gSYControllerConnectedNum == 3);
    CHECK(gSYControllerDeviceStatuses[0] == 0);
    CHECK(gSYControllerDeviceStatuses[1] == 1);
    CHECK(gSYControllerDeviceStatuses[2] == 3);
    CHECK(gSYControllerDeviceStatuses[3] == -1);
    /* The fourth player's buttons are the fourth port's, not the third
     * entry's: this is the whole point of the step. */
    CHECK(gSYControllerDevices[3].button_tap == N64_A);
    CHECK(gSYControllerDevices[2].button_tap == 0);
    /* scSubsysControllerCheckConnected's arithmetic, which is a search
     * of the table above for the port: 3 is in it, 2 is not. */
    {
        int found3 = 0, found2 = 0;

        for (i = 0; i < MAXCONTROLLERS; i++)
        {
            if (gSYControllerDeviceStatuses[i] == 3) found3 = 1;
            if (gSYControllerDeviceStatuses[i] == 2) found2 = 1;
        }
        CHECK(found3 == 1);
        CHECK(found2 == 0);
    }

    /* One pad, in C. It is the system pad and it is player 3. */
    sy_input_init();
    sy_input_feed(2, N64_START, 0, 0);
    sy_input_update();
    CHECK(gSYControllerConnectedNum == 1);
    CHECK(gSYControllerDeviceStatuses[0] == 2);
    CHECK(gSYControllerDeviceStatuses[1] == -1);
    CHECK(gSYControllerMain.button_tap == N64_START);
    CHECK(gSYControllerDevices[0].button_tap == 0);

    /* All four. */
    sy_input_init();
    for (i = 0; i < MAXCONTROLLERS; i++)
    {
        sy_input_feed(i, (uint16_t)(N64_A << 0), (int8_t)(i * 10), 0);
    }
    sy_input_update();
    CHECK(gSYControllerConnectedNum == MAXCONTROLLERS);
    for (i = 0; i < MAXCONTROLLERS; i++)
    {
        CHECK(gSYControllerDeviceStatuses[i] == i);
        CHECK(gSYControllerDevices[i].stick_range.x == (int8_t)(i * 10));
    }

    /* Nothing connected: the table is empty, the count is zero, and the
     * system pad keeps what it had rather than reading off the front of
     * the array the way the N64's does (input.c's DIVERGES). */
    sy_input_init();
    sy_input_feed(0, N64_B, 7, 0);
    sy_input_update();
    CHECK(gSYControllerMain.button_hold == N64_B);
    for (i = 0; i < MAXCONTROLLERS; i++)
    {
        sy_input_feed_err(i);
    }
    sy_input_update();
    CHECK(gSYControllerConnectedNum == 0);
    CHECK(gSYControllerDeviceStatuses[0] == -1);
    CHECK(gSYControllerMain.button_hold == N64_B);
    CHECK(gSYControllerMain.stick_range.x == 7);
}

static void test_dpad_remap(void)
{
    static const int kSelects[] = {
        nSCKindPlayersVS, nSCKind1PGamePlayers, nSCKindPlayers1PTraining,
        nSCKind1PBonus1Players, nSCKind1PBonus2Players,
    };
    static const int kBattles[] = {
        nSCKindVSBattle, nSCKind1PGame, nSCKind1PBonusStage,
        nSCKind1PTrainingMode,
    };
    const uint16_t dpad = N64_J_UP | N64_J_DOWN | N64_J_LEFT | N64_J_RIGHT;
    int i, ctx;

    /* Every character select: each direction is the C-button on that
     * side -- mnplayersvs.c's costumes 0..3 are up, right, down, left. */
    for (i = 0; i < (int)(sizeof(kSelects) / sizeof(kSelects[0])); i++)
    {
        ctx = sy_input_dpad_context(kSelects[i], -1);
        CHECK(ctx == SY_DPAD_SELECT);
        CHECK(sy_input_dpad_remap(N64_J_UP, ctx) == (N64_J_UP | N64_C_UP));
        CHECK(sy_input_dpad_remap(N64_J_RIGHT, ctx) ==
              (N64_J_RIGHT | N64_C_RIGHT));
        CHECK(sy_input_dpad_remap(N64_J_DOWN, ctx) ==
              (N64_J_DOWN | N64_C_DOWN));
        CHECK(sy_input_dpad_remap(N64_J_LEFT, ctx) ==
              (N64_J_LEFT | N64_C_LEFT));
        CHECK(sy_input_dpad_remap(N64_A, ctx) == N64_A);
    }

    /* A running match: any direction taunts. Paused or not yet started:
     * the plain D-pad (the Training menu reads it while paused), except
     * a paused bonus stage, where L is ifcommon.c's retry. */
    for (i = 0; i < (int)(sizeof(kBattles) / sizeof(kBattles[0])); i++)
    {
        ctx = sy_input_dpad_context(kBattles[i], nSCBattleGameStatusGo);
        CHECK(ctx == SY_DPAD_TAUNT);
        CHECK(sy_input_dpad_remap(N64_J_LEFT, ctx) == (N64_J_LEFT | N64_L));
        CHECK(sy_input_dpad_remap(N64_A | N64_Z, ctx) == (N64_A | N64_Z));
        CHECK(sy_input_dpad_context(kBattles[i], nSCBattleGameStatusWait)
              == SY_DPAD_PLAIN);
        CHECK(sy_input_dpad_context(kBattles[i], nSCBattleGameStatusPause)
              == ((kBattles[i] == nSCKind1PBonusStage) ? SY_DPAD_TAUNT
                                                       : SY_DPAD_PLAIN));
    }

    /* Everywhere else the D-pad is itself and nothing more. */
    ctx = sy_input_dpad_context(nSCKindMaps, nSCBattleGameStatusGo);
    CHECK(ctx == SY_DPAD_PLAIN);
    CHECK(sy_input_dpad_remap(dpad, ctx) == dpad);
    CHECK(sy_input_dpad_context(nSCKindCharacters, -1) == SY_DPAD_PLAIN);
    CHECK(sy_input_dpad_context(nSCKindVSOptions, -1) == SY_DPAD_PLAIN);
}

int main(void)
{
    test_edges();
    test_repeat_cadence();
    test_repeat_reset_on_change();
    test_accumulate();
    test_err_port();
    test_port_identity();
    test_dpad_remap();
    if (nfail)
    {
        printf("hosttest: %d FAILURES\n", nfail);
        return 1;
    }
    printf("hosttest: all input semantics checks passed\n");
    return 0;
}
