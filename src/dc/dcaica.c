/* See dcaica.h. */
#include "dcaica.h"

#include <kos.h>
#include <dc/spu.h>
#include <dc/sound/sound.h>
#include <dc/g2bus.h>
#include <dc/sound/aica_comm.h>
#include <string.h>

/* The AICA's registers, from the SH-4: channels at 0, the DSP's output
 * mixer at 0x2000, common registers at 0x2800, the DSP at 0x3000. */
#define AICA_REG(off) (0xa0700000u + (off))

/* src/dc/aica/fwblob.S: the ARM binary the build links in. */
extern const uint8_t dc_aica_fw[];
extern const uint8_t dc_aica_fw_end[];

void dc_aica_init(void)
{
    static int done;
    size_t n = (size_t)(dc_aica_fw_end - dc_aica_fw);

    if (done)
        return;
    spu_disable();
    /* aica_cmd_iface.h (KOS's ARM side) puts the firmware, its queues and
     * its channel mirror below 0x030000; shared.h puts the reverb's ring
     * above them, so sample memory starts at DC_AICA_RAM_START */
    spu_memset_sq(0, 0, DC_AICA_RAM_START);
    spu_memload(0, dc_aica_fw, (n + 3) & ~(size_t)3);
    spu_enable();
    /* the same settle snd_init gives KOS's firmware before the first
     * command */
    thd_sleep(10);
    snd_mem_init(DC_AICA_RAM_START);
    {
        DCAicaStatus st;
        int ms;

        for (ms = 0; ms < 200; ms++)
        {
            dc_aica_status(&st);
            if (st.magic == DC_AICA_MAGIC && st.passes > 1)
                break;
            thd_sleep(1);
        }
        if (st.magic == DC_AICA_MAGIC)
            dbglog(DBG_INFO, "aica: port firmware running (%u bytes, %lu "
                   "passes after %d ms)\n", (unsigned)n,
                   (unsigned long)st.passes, ms);
        else
            dbglog(DBG_ERROR, "aica: port firmware did not start (%u "
                   "bytes, magic %08lx)\n", (unsigned)n,
                   (unsigned long)st.magic);
    }
    done = 1;
}

void dc_aica_status(DCAicaStatus *out)
{
    /* the AICA's RAM is not cacheable from here; g2 reads it a word at a
     * time, under the FIFO wait KOS's own reads use */
    uint32_t *w = (uint32_t *)out;
    size_t i;

    for (i = 0; i < sizeof(*out) / 4; i++)
        w[i] = g2_read_32(SPU_RAM_UNCACHED_BASE + DC_AICA_STATUS_AT + 4 * i);
}

void dc_aica_seq_cmd(uint32_t what, uint32_t a0, uint32_t a1)
{
    aica_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));
    cmd.size = sizeof(cmd) / 4;
    cmd.cmd = DC_AICA_CMD_SEQ;
    cmd.cmd_id = what;
    cmd.misc[0] = a0;
    cmd.misc[1] = a1;
    snd_sh4_to_aica(&cmd, cmd.size);
}

void dc_aica_dsp_load(const uint16_t *mpro, const uint16_t *coef,
                      const uint16_t *madrs, int rbl, uint16_t silence)
{
    int i;

    /* Mute the DSP's return and stop every write the old program makes
     * (the firmware cleared it at boot) before the operands change under
     * it, then fill the line with silence -- 0 is not silence in the
     * DSP's packed float -- and only then load the new program. */
    g2_write_32(AICA_REG(0x2000), 0);
    for (i = 0; i < 512; i++)
        g2_write_32(AICA_REG(0x3400 + 4 * i), 0);
    for (i = 0; i < DC_AICA_RING_BYTES; i += 4)
        g2_write_32(SPU_RAM_UNCACHED_BASE + DC_AICA_RING_AT + i,
                    ((uint32_t)silence << 16) | silence);
    for (i = 0; i < 128; i++)
        g2_write_32(AICA_REG(0x3000 + 4 * i), coef[i]);
    for (i = 0; i < 64; i++)
        g2_write_32(AICA_REG(0x3200 + 4 * i), madrs[i]);
    g2_write_32(AICA_REG(0x2804),
                ((uint32_t)rbl << 13) | (DC_AICA_RING_AT >> 11));
    for (i = 0; i < 512; i++)
        g2_write_32(AICA_REG(0x3400 + 4 * i), mpro[i]);
    /* EFREG0, the program's mono output: full level (EFSDL 15), centred,
     * the N64's main += aux on both sides */
    g2_write_32(AICA_REG(0x2000), 15 << 8);
}
