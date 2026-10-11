/* The AICA firmware (src/dc/aica/), as built by src/game/ssb64/Makefile
 * into aica_fw.bin, linked into the SH-4 program for dc_aica_init. */
    .section .rodata
    .balign 32
    .global _dc_aica_fw
_dc_aica_fw:
    .incbin "aica_fw.bin"
    .global _dc_aica_fw_end
_dc_aica_fw_end:
