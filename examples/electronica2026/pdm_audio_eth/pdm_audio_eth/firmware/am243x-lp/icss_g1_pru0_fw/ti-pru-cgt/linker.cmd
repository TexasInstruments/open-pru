/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Linker command file for the pdm_audio_eth ICSSG1 PRU0 firmware
 * (eth_tx.asm, the Ethernet transmitter), TI-PRU-CGT, AM243x ICSSG1. The firmware has no data
 * sections; the map below only keeps any section away from the run-time
 * buffers. The shared RAM (local 0x10000) holds the ETH1_* block of
 * eth_proto.inc.
 */

MEMORY
{
      PAGE 0:
	/* 12 KB PRU Instruction RAM */
	PRU_IMEM	: org = 0x00000000 len = 0x00003000

      PAGE 1:
	/* 8 KB Data RAM 0: 0x0000..0x07FF is the TX staging buffer (one 2 KB
	 * PCM half copied from ICSSG0, eth_tx.asm); sections after it */
	TX_STAGE	: org = 0x00000000 len = 0x00000800
	PRU_DMEM_OWN	: org = 0x00000800 len = 0x00001800
	/* 8 KB Data RAM 1: PRU1 (eth_rx.asm), not for PRU0 */
	PRU_DMEM_OTHER	: org = 0x00002000 len = 0x00002000

      PAGE 2:
	/* 64 KB ICSSG1 shared RAM: ETH1_* block (eth_proto.inc) */
	PRU_SHAREDMEM	: org = 0x00010000 len = 0x00010000
}

SECTIONS {
	.text		>  PRU_IMEM, PAGE 0
	.stack		>  PRU_DMEM_OWN, PAGE 1
	.bss		>  PRU_DMEM_OWN, PAGE 1
	.cio		>  PRU_DMEM_OWN, PAGE 1
	.data		>  PRU_DMEM_OWN, PAGE 1
	.switch		>  PRU_DMEM_OWN, PAGE 1
	.sysmem		>  PRU_DMEM_OWN, PAGE 1
	.cinit		>  PRU_DMEM_OWN, PAGE 1
	.rodata		>  PRU_DMEM_OWN, PAGE 1
	.rofardata	>  PRU_DMEM_OWN, PAGE 1
	.farbss		>  PRU_DMEM_OWN, PAGE 1
	.fardata	>  PRU_DMEM_OWN, PAGE 1
}
