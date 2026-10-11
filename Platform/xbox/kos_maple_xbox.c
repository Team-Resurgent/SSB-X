/* kos_maple_xbox.c -- the Dreamcast's maple bus on the Xbox's game ports.
 * See Platform/xbox/dc/maple.h for the device layout.
 *
 * Controller: the Xbox pad reports what a Dreamcast pad reports, by
 * position, so src/dc/input.c's own mapping applies unchanged --
 *
 *   Xbox A / B / X / Y   ->  CONT_A / CONT_B / CONT_X / CONT_Y  (same places
 *                            on both pads: A bottom, B right, X left, Y top)
 *   START, D-pad         ->  CONT_START, CONT_DPAD_*
 *   left / right trigger ->  ltrig / rtrig, 0..255 (input.c takes Z and R at
 *                            half travel, as on the Dreamcast)
 *   left thumbstick      ->  joyx / joyy, -128..127, +y DOWN as maple has it
 *   right thumbstick     ->  joy2x / joy2y
 *
 * The face buttons are pressure sensitive on the Xbox; past a quarter of
 * their travel they read as pressed, as the Dreamcast's digital ones do.
 *
 * Jump pack: every Xbox pad carries two rumble motors, so each pad port
 * also reports a purupuru at unit 1; purupuru_rumble drives both motors.
 *
 * Memory card: port A, unit 2, always present -- it is the title's save
 * game on the hard disk (Platform/xbox/kos_vmu_xbox.c). */
#include "xbox_xtl.h"

#include <string.h>

#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <dc/maple/purupuru.h>
#include <kos/dbglog.h>

#define PADS 4
#define BUTTON_PRESSED 0x40     /* of 0xFF */

typedef struct
{
    maple_device_t cont;        /* unit 0 */
    maple_device_t puru;        /* unit 1 */
    HANDLE h;
    cont_state_t state;
    XINPUT_FEEDBACK fb;         /* in flight until fb.Header.dwStatus settles */
    int fb_busy;
    WORD want_left, want_right; /* the motor speeds asked for */
    WORD sent_left, sent_right;
} Pad;

static Pad sPads[PADS];
static maple_device_t sCard;
static int sReady;

void xbox_maple_init(void)
{
    XDEVICE_PREALLOC_TYPE prealloc[] = {
        { XDEVICE_TYPE_GAMEPAD, PADS },
        { XDEVICE_TYPE_MEMORY_UNIT, 8 },
    };
    int i;

    if (sReady)
        return;
    /* Enumeration is asynchronous: started first thing in main, it has the
     * whole boot to settle before the game's first look at the ports. */
    XInitDevices(sizeof(prealloc) / sizeof(prealloc[0]), prealloc);
    for (i = 0; i < PADS; i++)
    {
        Pad *p = &sPads[i];

        memset(p, 0, sizeof(*p));
        p->cont.valid = 1;
        p->cont.port = i;
        p->cont.unit = 0;
        p->cont.info.functions = MAPLE_FUNC_CONTROLLER;
        strncpy(p->cont.info.product_name, "Xbox Controller", sizeof(p->cont.info.product_name));
        p->cont.status = &p->state;
        p->puru.valid = 1;
        p->puru.port = i;
        p->puru.unit = 1;
        p->puru.info.functions = MAPLE_FUNC_PURUPURU;
        strncpy(p->puru.info.product_name, "Xbox Controller Rumble", sizeof(p->puru.info.product_name));
    }
    memset(&sCard, 0, sizeof(sCard));
    sCard.valid = 1;
    sCard.port = 0;
    sCard.unit = 2;
    sCard.info.functions = MAPLE_FUNC_MEMCARD;
    strncpy(sCard.info.product_name, "Xbox Hard Disk", sizeof(sCard.info.product_name));
    sReady = 1;
}

/* Open newly inserted pads, close removed ones. XGetDeviceChanges reports
 * each change once; a pad both removed and inserted since the last call is
 * re-opened. */
static volatile LONG sRefreshBusy;

static void pads_refresh(void)
{
    DWORD ins = 0, rem = 0;
    int i;

    if (!sReady)
        xbox_maple_init();
    /* the game reads pads on its main thread, the memory-card writer looks
     * up the card on its own: one refresh at a time, and a caller that
     * finds one running uses the table as it stands */
    if (InterlockedExchange((LONG *)&sRefreshBusy, 1) != 0)
        return;
    if (!XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, &ins, &rem))
    {
        sRefreshBusy = 0;
        return;
    }
    for (i = 0; i < PADS; i++)
    {
        Pad *p = &sPads[i];

        if ((rem & (1u << i)) && p->h != NULL)
        {
            XInputClose(p->h);
            p->h = NULL;
            p->fb_busy = 0;
            p->sent_left = p->sent_right = 0;
        }
        if ((ins & (1u << i)) && p->h == NULL)
        {
            p->h = XInputOpen(XDEVICE_TYPE_GAMEPAD, (DWORD)i, XDEVICE_NO_SLOT, NULL);
        }
    }
    sRefreshBusy = 0;
}

void xbox_maple_poll(void)
{
    pads_refresh();
}

maple_device_t *maple_enum_dev(int p, int u)
{
    if (p < 0 || p >= PADS)
        return NULL;
    pads_refresh();
    if (p == 0 && u == 2)
        return &sCard;
    if (sPads[p].h == NULL)
        return NULL;
    if (u == 0)
        return &sPads[p].cont;
    if (u == 1)
        return &sPads[p].puru;
    return NULL;
}

maple_device_t *maple_enum_type(int n, uint32_t func)
{
    int p, u, seen = 0;

    for (p = 0; p < PADS; p++)
    {
        for (u = 0; u < 3; u++)
        {
            maple_device_t *d = maple_enum_dev(p, u);

            if (d != NULL && (d->info.functions & func))
            {
                if (seen == n)
                    return d;
                seen++;
            }
        }
    }
    return NULL;
}

static int thumb(SHORT v)
{
    int j = v / 256;

    if (j > 127)
        j = 127;
    if (j < -128)
        j = -128;
    return j;
}

void *maple_dev_status(maple_device_t *dev)
{
    Pad *p;
    XINPUT_STATE xs;
    const XINPUT_GAMEPAD *g;
    cont_state_t *s;

    if (dev == NULL || dev->unit != 0 || dev->port < 0 || dev->port >= PADS)
        return NULL;
    p = &sPads[dev->port];
    if (p->h == NULL)
        return NULL;
    memset(&xs, 0, sizeof(xs));
    if (XInputGetState(p->h, &xs) != ERROR_SUCCESS)
        return NULL;
    g = &xs.Gamepad;
    s = &p->state;
    memset(s, 0, sizeof(*s));

    if (g->bAnalogButtons[XINPUT_GAMEPAD_A] > BUTTON_PRESSED)
        s->buttons |= CONT_A;
    if (g->bAnalogButtons[XINPUT_GAMEPAD_B] > BUTTON_PRESSED)
        s->buttons |= CONT_B;
    if (g->bAnalogButtons[XINPUT_GAMEPAD_X] > BUTTON_PRESSED)
        s->buttons |= CONT_X;
    if (g->bAnalogButtons[XINPUT_GAMEPAD_Y] > BUTTON_PRESSED)
        s->buttons |= CONT_Y;
    if (g->wButtons & XINPUT_GAMEPAD_START)
        s->buttons |= CONT_START;
    if (g->wButtons & XINPUT_GAMEPAD_DPAD_UP)
        s->buttons |= CONT_DPAD_UP;
    if (g->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
        s->buttons |= CONT_DPAD_DOWN;
    if (g->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
        s->buttons |= CONT_DPAD_LEFT;
    if (g->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
        s->buttons |= CONT_DPAD_RIGHT;

    s->ltrig = g->bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER];
    s->rtrig = g->bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER];
    /* maple: +x right, +y DOWN; the Xbox thumbstick is +y up */
    s->joyx = thumb(g->sThumbLX);
    s->joyy = -thumb(g->sThumbLY);
    if (s->joyy > 127)
        s->joyy = 127;
    s->joy2x = thumb(g->sThumbRX);
    s->joy2y = -thumb(g->sThumbRY);
    if (s->joy2y > 127)
        s->joy2y = 127;
    return s;
}

/* ================================================================ rumble
 * XInputSetState is asynchronous: its XINPUT_FEEDBACK must stay put until
 * the driver has taken it (Header.dwStatus leaves ERROR_IO_PENDING). One
 * request in flight per pad; a newer one waits for it and only the latest
 * wanted state is sent. */

static void rumble_send(Pad *p)
{
    if (p->h == NULL)
        return;
    if (p->fb_busy)
    {
        if (p->fb.Header.dwStatus == ERROR_IO_PENDING)
            return;
        p->fb_busy = 0;
    }
    if (p->want_left == p->sent_left && p->want_right == p->sent_right)
        return;
    memset(&p->fb, 0, sizeof(p->fb));
    p->fb.Rumble.wLeftMotorSpeed = p->want_left;
    p->fb.Rumble.wRightMotorSpeed = p->want_right;
    if (XInputSetState(p->h, &p->fb) == ERROR_IO_PENDING)
        p->fb_busy = 1;
    p->sent_left = p->want_left;
    p->sent_right = p->want_right;
}

int purupuru_rumble(maple_device_t *dev, const purupuru_effect_t *effect)
{
    Pad *p;
    WORD speed;

    if (dev == NULL || effect == NULL || dev->unit != 1 || dev->port < 0 || dev->port >= PADS)
        return MAPLE_EINVALID;
    p = &sPads[dev->port];
    if (p->h == NULL)
        return MAPLE_EFAIL;
    /* fpow 0..7 is the jump pack's forward strength, 0 = stop. The Xbox
     * pad's big (left) motor is the closest match to the pack's single
     * motor; the small one adds the high-frequency buzz the pack's
     * frequency setting gives above ~30 Hz. */
    speed = (WORD)((effect->fpow > 7 ? 7 : effect->fpow) * 65535u / 7u);
    p->want_left = speed;
    p->want_right = (effect->freq >= 30) ? speed : (WORD)(speed / 2);
    if (speed == 0)
        p->want_right = 0;
    rumble_send(p);
    return MAPLE_EOK;
}

int purupuru_rumble_raw(maple_device_t *dev, uint32_t effect)
{
    purupuru_effect_t e;

    e.raw = effect;
    return purupuru_rumble(dev, &e);
}

/* Called once a frame (Platform/xbox/xbox_main.c's frame hook): a request
 * that found the previous one still in flight goes out now. */
void xbox_maple_rumble_pump(void)
{
    int i;

    for (i = 0; i < PADS; i++)
        rumble_send(&sPads[i]);
}
