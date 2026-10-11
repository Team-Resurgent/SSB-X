/* dc/maple.h -- Xbox shim: the Dreamcast's maple bus, over the Xbox's USB
 * game ports (Platform/xbox/kos_maple_xbox.c).
 *
 * The four Xbox controller ports are maple ports A-D. On each:
 *   unit 0  the controller itself (MAPLE_FUNC_CONTROLLER), when a gamepad is
 *           plugged in -- read through XInputGetState;
 *   unit 1  a jump pack (MAPLE_FUNC_PURUPURU) on the same pad, which every
 *           Xbox gamepad carries as its rumble motors (XInputSetState);
 * and on port A only:
 *   unit 2  a memory card (MAPLE_FUNC_MEMCARD) backed by the title's save
 *           game on the hard disk (Platform/xbox/kos_vmu_xbox.c).
 *
 * The structs carry KallistiOS's public fields with their names, so the
 * port's device code reads them unchanged; the private half is not kept. */
#ifndef XBOX_DC_MAPLE_H
#define XBOX_DC_MAPLE_H

#include <kos/cdefs.h>

__BEGIN_DECLS

#define MAPLE_FUNC_PURUPURU     0x00010000
#define MAPLE_FUNC_MOUSE        0x00020000
#define MAPLE_FUNC_CAMERA       0x00080000
#define MAPLE_FUNC_CONTROLLER   0x01000000
#define MAPLE_FUNC_MEMCARD      0x02000000
#define MAPLE_FUNC_LCD          0x04000000
#define MAPLE_FUNC_CLOCK        0x08000000
#define MAPLE_FUNC_MICROPHONE   0x10000000
#define MAPLE_FUNC_ARGUN        0x20000000
#define MAPLE_FUNC_KEYBOARD     0x40000000
#define MAPLE_FUNC_LIGHTGUN     0x80000000
#define MAPLE_FUNC_ANY          0xffffffff

#define MAPLE_PORT_COUNT    4
#define MAPLE_UNIT_COUNT    6

#define MAPLE_EOK       0
#define MAPLE_EFAIL     -1
#define MAPLE_EAGAIN    -2
#define MAPLE_EINVALID  -3
#define MAPLE_ENOTSUPP  -4
#define MAPLE_ETIMEOUT  -5

typedef struct maple_devinfo {
    uint32_t  functions;
    uint32_t  function_data[3];
    uint8_t   area_code;
    uint8_t   connector_direction;
    char      product_name[30];
    char      product_license[60];
    uint16_t  standby_power;
    uint16_t  max_power;
} maple_devinfo_t;

typedef struct maple_device {
    uint8_t         valid;
    int             port;
    int             unit;
    maple_devinfo_t info;
    void           *status;     /* the device's status buffer */
} maple_device_t;

maple_device_t *maple_enum_dev(int p, int u);
maple_device_t *maple_enum_type(int n, uint32_t func);
void *maple_dev_status(maple_device_t *dev);

/* Platform/xbox: open the game ports and read the pads once a frame.
 * xbox_maple_init is called from main before the game; xbox_maple_poll
 * runs at the top of every maple_enum_dev/maple_dev_status pass that is
 * the first in its frame (Platform/xbox/kos_maple_xbox.c). */
void xbox_maple_init(void);
void xbox_maple_poll(void);

__END_DECLS

#include <dc/maple/controller.h>

#endif /* XBOX_DC_MAPLE_H */
