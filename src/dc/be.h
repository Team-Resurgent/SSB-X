/* Big-endian accessors for game data kept in its N64 byte order (the FGM
 * bytecode tables, src/dc/fgm.c). A raw struct overlay on such data is
 * always a bug on this side of the port.
 */
#ifndef DC_BE_H
#define DC_BE_H

#include <stdint.h>
#include <string.h>

static inline uint16_t be16u(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return (uint16_t)((b[0] << 8) | b[1]);
}

static inline int16_t be16s(const void *p)
{
    return (int16_t)be16u(p);
}

static inline uint32_t be32u(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

static inline int32_t be32s(const void *p)
{
    return (int32_t)be32u(p);
}

static inline float bef32(const void *p)
{
    uint32_t u = be32u(p);
    float f;
    memcpy(&f, &u, sizeof(f));
    return f;
}

#endif /* DC_BE_H */
