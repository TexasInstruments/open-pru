/*
 * AM243x_PRU0.cmd
 *
 * Linker command file for the pdm_audio PRU0 assembly firmware
 * (pru0_rx.asm), built with the TI-PRU-CGT on AM243x ICSSG0.
 *
 * pru0_rx.asm has no data sections. At run time it keeps its state in
 * PRU0 DRAM0 local 0x07F8..0x1AFF (publish block, FIR coefficients, 8
 * channel blocks, odd-channel FIR coefficients; register and memory map in pru0_rx.asm), which takes
 * the RTU0 / TX_PRU0 share of DRAM0 too: this demo runs neither core.
 * PRU0 sections may only go to PRU0_DMEM_0 (0x0000..0x07F7). PRU0 never
 * touches DRAM1 (local 0x2000..0x3FFF), where RTU1 keeps its scratch at
 * 0x3700..0x37FF (PDM8_RTU1_SCRATCH in pdm8_shared.inc).
 */

/* Specify the System Memory Map */
MEMORY
{
      PAGE 0:
	/* 12 KB PRU Instruction RAM */
	PRU_IMEM	: org = 0x00000000 len = 0x00003000

      PAGE 1:
	/* Data RAMs */
	/* 8 KB PRU Data RAM 0: the first 2 KB - 8 B for PRU0 sections */
	PRU0_DMEM_0	: org = 0x00000000 len = 0x000007F8
	/* pru0_rx.asm run-time state (PUB_BASE .. FIR_T_ODD_BASE + 0x100):
	 * no sections are placed here */
	PRU0_RX_STATE	: org = 0x000007F8 len = 0x00001308
	/* Rest of DRAM0, unused (RTU0 / TX_PRU0 are not run) */
	PRU0_DMEM_0_TOP	: org = 0x00001B00 len = 0x00000500
	/* 8 KB PRU Data RAM 1; reserved completely for Slice1 cores - PRU1,
	 * RTU1 and Tx_PRU1; do not use for any Slice0 cores */
	PRU1_DMEM_1	: org = 0x00002000 len = 0x00001700
	/* RTU1 scratch (rtu1_mod.asm): not for PRU0 */
	RTU1_SCRATCH	: org = 0x00003700 len = 0x00000100
	TX_PRU1_DMEM_1	: org = 0x00003800 len = 0x00000800

      PAGE 2:
	/* 64 KB PRU Shared RAM: header and PCM halves (pdm8_shared.inc) */
	PRU_SHAREDMEM	: org = 0x00010000 len = 0x00010000
}

/* Specify the sections allocation into memory */
SECTIONS {

	.text		>  PRU_IMEM, PAGE 0
	.stack		>  PRU0_DMEM_0, PAGE 1
	.bss		>  PRU0_DMEM_0, PAGE 1
	.cio		>  PRU0_DMEM_0, PAGE 1
	.data		>  PRU0_DMEM_0, PAGE 1
	.switch		>  PRU0_DMEM_0, PAGE 1
	.sysmem		>  PRU0_DMEM_0, PAGE 1
	.cinit		>  PRU0_DMEM_0, PAGE 1
	.rodata		>  PRU0_DMEM_0, PAGE 1
	.rofardata	>  PRU0_DMEM_0, PAGE 1
	.farbss		>  PRU0_DMEM_0, PAGE 1
	.fardata	>  PRU0_DMEM_0, PAGE 1
}
