/* input.h -- the game's controller layer (sys/controller.c) fed from maple.
 *
 * The N64 game splits input across two moments: a 60 Hz retrace read
 * (syControllerReadDeviceData) that computes edges and *accumulates* them,
 * and a per-game-frame consume (syControllerUpdateGlobalData) that latches
 * the accumulators into gSYControllerDevices[] and clears them.  A 30 fps
 * game frame therefore never misses a 60 Hz tap.  This port keeps both
 * moments: sy_input_poll() per retrace, sy_input_update() per game frame.
 *
 * The pure state machine (sy_input_feed) is separated from the maple read
 * so the exact ported logic can be host-tested; build with -DSY_INPUT_HOST
 * to drop the KOS parts.
 *
 * The N64 button bits below are PR/os.h's CONT_* values; they get an N64_
 * prefix because KOS's maple headers define CONT_A etc. with different
 * values, and both must coexist in input.c.
 */
#ifndef SSB_DC_INPUT_H
#define SSB_DC_INPUT_H

#include <stdint.h>

/* The controller type, its four globals and MAXCONTROLLERS are the game's
 * own, out of the decomp -- this file supplies only the Dreamcast half.
 * Until milestone 8 the port declared a second SYController here, laid out
 * the same but spelling the stick as two int8_t; sys/taskman.c includes
 * sys/controller.h, so the two would have collided the moment the frame
 * loop came across. The decomp's Vec2b spelling wins: ftcommon.h's
 * FTPlayerInput already reads `stick_range`. */
#include <sys/controller.h>

#define N64_A       0x8000
#define N64_B       0x4000
#define N64_Z       0x2000
#define N64_START   0x1000
#define N64_J_UP    0x0800
#define N64_J_DOWN  0x0400
#define N64_J_LEFT  0x0200
#define N64_J_RIGHT 0x0100
#define N64_L       0x0020
#define N64_R       0x0010
#define N64_C_UP    0x0008
#define N64_C_DOWN  0x0004
#define N64_C_LEFT  0x0002
#define N64_C_RIGHT 0x0001

/* What the game's SYController members mean, since its header only
 * carries the decomp's question marks: hold = buttons down right now,
 * tap = press edges, release = release edges, update = the auto-repeat
 * channel (fires on press, then after 30 reads, then every 5 -- menu
 * scrolling), stick_range = the analog stick.  The three edge words
 * accumulate between sy_input_update() calls.
 *
 * gSYControllerDevices[MAXCONTROLLERS], gSYControllerMain,
 * gSYControllerConnectedNum and gSYControllerDeviceStatuses (the k-th
 * connected pad's port, -1 past the last: what the menus' pad reads go
 * through) are declared by sys/controller.h and defined by
 * src/dc/input.c. */

void sy_input_init(void);

/* The syControllerReadDeviceData edge/repeat/accumulate logic for one
 * port, given raw state already in N64 terms.  poll() calls this from
 * maple; host tests call it directly. */
void sy_input_feed(int port, uint16_t buttons, int8_t stick_x,
                   int8_t stick_y);
/* A port with no controller: the game skips both edge math and the latch
 * for ports whose read errno is set, leaving the last values in place. */
void sy_input_feed_err(int port);

/* Latch accumulators into gSYControllerDevices / gSYControllerMain and
 * clear them (syControllerUpdateGlobalData). */
void sy_input_update(void);

/* Whether the port has a device on it -- strictly, whether the last read
 * of it set no error, which is what the game's own
 * syControllerReadDeviceData records and what gSYControllerConnectedNum
 * is counted from.
 *
 * The game has no such accessor because it has no reason to want one:
 * nothing in it writes a controller. A debug build standing in for a missing
 * pad does, and it cannot ask gSYControllerConnectedNum instead --
 * feeding a port clears its error, so the count answers "yes, connected"
 * about the very pad the stand-in is inventing. Call it after
 * sy_input_poll and before feeding. */
sb32 sy_input_port_present(int port);

/* The port's own D-pad remap (the pad has no L, C-left or C-right).
 * sy_input_dpad_context picks one of these from the scene and the
 * battle's game_status; sy_input_dpad_remap adds the stand-in bits:
 *   SY_DPAD_SELECT  character selects: up/right/down/left -> the C-button
 *                   of that side, i.e. the four costumes;
 *   SY_DPAD_TAUNT   a running match (or a paused bonus stage, for its
 *                   L-to-retry): any direction -> L;
 *   SY_DPAD_PLAIN   everywhere else: the D-pad only. */
enum { SY_DPAD_PLAIN, SY_DPAD_SELECT, SY_DPAD_TAUNT };
int sy_input_dpad_context(int scene, int game_status);
uint16_t sy_input_dpad_remap(uint16_t btn, int ctx);

#ifndef SY_INPUT_HOST
/* Read the maple controllers and feed every port (call once per frame). */
void sy_input_poll(void);

/* sys/controller.c syControllerFuncRead, which every scene's
 * SYTaskmanSetup names as its func_controller and the frame loop calls
 * once a tic before the scene's update. The game's is the consume half
 * alone -- its 60 Hz read runs off the scheduler's retrace callback --
 * so the port's is the read and the consume together, which is where
 * both have always happened here. */
void syControllerFuncRead(void);

/* The port's own: log which physical ports carry a pad,
 * once and then only when it changes. It reads the poll, not the latch,
 * so call it between sy_input_poll and sy_input_update:
 * syControllerFuncRead does, and so must anything that stands in for
 * syControllerFuncRead -- which is what src/dc/db.c's
 * db_func_controller is. */
void sy_input_report_ports(void);
#endif

#endif /* SSB_DC_INPUT_H */
