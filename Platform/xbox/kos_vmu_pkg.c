/* kos_vmu_pkg.c -- VMS packages: KallistiOS's vmu_pkg_build and
 * vmu_pkg_parse (kernel/arch/dreamcast/util/vmu_pkg.c, (c)2002 Megan
 * Potter) with the CRC they use (kernel/net/net_crc.c net_crc16ccitt),
 * under the KOS licence (new BSD; see LICENSE.KOS in KallistiOS).
 *
 * The port wraps its save file in this format on the Xbox too, so the file
 * in the Xbox save game is byte-for-byte the file a VMU would hold, and
 * src/dc/vmucard.c judges it the same way (header, CRC, app id). */
#include <stdlib.h>
#include <string.h>

#include <dc/vmu_pkg.h>
#include <kos/dbglog.h>

static uint16_t net_crc16ccitt(const uint8_t *data, int size, uint16_t start)
{
    uint16_t rv = start, tmp;

    while (size--)
    {
        tmp = (rv >> 8) ^ *data++;
        tmp ^= tmp >> 4;
        rv = (rv << 8) ^ (tmp << 12) ^ (tmp << 5) ^ tmp;
    }
    return rv;
}

static int vmu_eyecatch_size(int eyecatch_type)
{
    switch (eyecatch_type)
    {
        case VMUPKG_EC_NONE:
            return 0;
        case VMUPKG_EC_16BIT:
            return 72 * 56 * 2;
        case VMUPKG_EC_256COL:
            return 512 + 72 * 56;
        case VMUPKG_EC_16COL:
            return 32 + 72 * 56 / 2;
        default:
            return -1;
    }
}

int vmu_pkg_build(vmu_pkg_t *src, uint8_t **dst, int *dst_size)
{
    uint8_t *out;
    int ec_size, out_size;
    vmu_hdr_t *hdr;

    if (src == NULL || dst == NULL)
        return -1;
    if (src->data_len < 0)
        return -2;

    out_size = sizeof(vmu_hdr_t) + 512 * src->icon_cnt + src->data_len;
    ec_size = vmu_eyecatch_size(src->eyecatch_type);
    if (ec_size < 0)
        return -1;
    out_size += ec_size;

    out = malloc(out_size);
    if (!out)
        return -1;
    *dst_size = out_size;
    *dst = out;

    memset(out, 0, out_size);
    hdr = (vmu_hdr_t *)out;
    memset(hdr->desc_short, 32, sizeof(hdr->desc_short));
    memset(hdr->desc_long, 32, sizeof(hdr->desc_long));
    memcpy(hdr->desc_short, src->desc_short, strlen(src->desc_short));
    memcpy(hdr->desc_long, src->desc_long, strlen(src->desc_long));
    strcpy(hdr->app_id, src->app_id);
    hdr->icon_cnt = src->icon_cnt;
    hdr->icon_anim_speed = src->icon_anim_speed;
    hdr->eyecatch_type = src->eyecatch_type;
    hdr->crc = 0;
    hdr->data_len = src->data_len;
    memcpy(hdr->icon_pal, src->icon_pal, sizeof(src->icon_pal));
    out += sizeof(vmu_hdr_t);

    memcpy(out, src->icon_data, 512 * src->icon_cnt);
    out += 512 * src->icon_cnt;
    memcpy(out, src->eyecatch_data, ec_size);
    out += ec_size;
    if (src->data)
        memcpy(out, src->data, src->data_len);
    out += src->data_len;

    out = *dst;
    hdr->crc = net_crc16ccitt(out, out_size, 0);
    return 0;
}

int vmu_pkg_parse(uint8_t *data, size_t data_size, vmu_pkg_t *pkg)
{
    uint16_t crc, crc_save;
    int ec_size, hdr_size, total_size, icon_size;
    vmu_hdr_t *hdr;

    if (data == NULL || pkg == NULL || data_size < sizeof(vmu_hdr_t))
        return -1;
    hdr = (vmu_hdr_t *)data;

    icon_size = 512 * hdr->icon_cnt;
    ec_size = vmu_eyecatch_size(hdr->eyecatch_type);
    hdr_size = sizeof(vmu_hdr_t) + icon_size + ec_size;
    total_size = hdr->data_len + hdr_size;

    if (ec_size < 0 || total_size < 0 || (size_t)total_size > data_size)
    {
        dbglog(DBG_ERROR, "vmu_pkg_parse: file header is corrupted, or no header?\n");
        return -1;
    }

    crc_save = hdr->crc;
    hdr->crc = 0;
    crc = net_crc16ccitt(data, total_size, 0);
    hdr->crc = crc_save;
    if (crc_save != crc)
    {
        dbglog(DBG_ERROR, "vmu_pkg_parse: expected CRC %04x, got %04x\n", crc_save, crc);
        return -1;
    }

    pkg->icon_cnt = hdr->icon_cnt;
    pkg->icon_anim_speed = hdr->icon_anim_speed;
    pkg->eyecatch_type = hdr->eyecatch_type;
    pkg->data_len = hdr->data_len;
    memcpy(pkg->icon_pal, hdr->icon_pal, sizeof(hdr->icon_pal));
    pkg->icon_data = data + sizeof(vmu_hdr_t);
    pkg->eyecatch_data = data + sizeof(vmu_hdr_t) + icon_size;
    pkg->data = data + hdr_size;
    memcpy(pkg->desc_short, hdr->desc_short, sizeof(hdr->desc_short));
    memcpy(pkg->desc_long, hdr->desc_long, sizeof(hdr->desc_long));
    memcpy(pkg->app_id, hdr->app_id, sizeof(hdr->app_id));
    pkg->desc_short[sizeof(hdr->desc_short)] = '\0';
    pkg->desc_long[sizeof(hdr->desc_long)] = '\0';
    pkg->app_id[sizeof(hdr->app_id)] = '\0';
    return 0;
}
