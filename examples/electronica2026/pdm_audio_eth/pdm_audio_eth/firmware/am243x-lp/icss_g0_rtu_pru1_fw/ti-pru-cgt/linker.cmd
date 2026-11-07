/*
 * AM243x_RTU1.cmd
 *
 * Linker command file for the pdm_audio_eth RTU1 (RTU_PRU1) assembly firmware,
 * built with the TI-PRU-CGT on AM243x ICSSG0.
 */

/* Specify the System Memory Map */
MEMORY
{
      PAGE 0:
	/* 8 KB RTU Instruction RAM */
	RTU_IMEM	: org = 0x00000000 len = 0x00002000

      PAGE 1:
	/* Data RAMs */
	/* 8 KB PRU Data RAM 1; use only the first 4 KB for PRU1 and reserve
	 * the second 4 KB for RTU1 and Tx_PRU1 */
	PRU1_DMEM_1	: org = 0x00000000 len = 0x00001000
	/* 8 KB PRU Data RAM 0; reserved completely for Slice0 cores - PRU0,
	 * RTU0 and Tx_PRU0; do not use for any Slice1 cores */
	PRU0_DMEM_0	: org = 0x00002000 len = 0x00001000
	/* NOTE: Custom split of the second 4 KB of ICSS Data RAMs 0 and 1
	 * split equally between the corresponding RTU and Tx_PRU cores in
	 * each slice */
	RTU1_DMEM_1	: org = 0x00001000 len = 0x00000700
	/* RTU1 scratch used directly by rtu1_mod.asm (PDM8_RTU1_SCRATCH in
	 * pdm8_shared.inc); no sections are placed here */
	RTU1_SCRATCH	: org = 0x00001700 len = 0x00000100
	TX_PRU1_DMEM_1	: org = 0x00001800 len = 0x00000800
	RTU0_DMEM_0	: org = 0x00003000 len = 0x00000800
	TX_PRU0_DMEM_0	: org = 0x00003800 len = 0x00000800

      PAGE 2:
	/* 64 KB PRU Shared RAM */
	PRU_SHAREDMEM	: org = 0x00010000 len = 0x00010000
}

/* Specify the sections allocation into memory */
SECTIONS {

	.text		>  RTU_IMEM, PAGE 0
	.stack		>  RTU1_DMEM_1, PAGE 1
	.bss		>  RTU1_DMEM_1, PAGE 1
	.cio		>  RTU1_DMEM_1, PAGE 1
	.data		>  RTU1_DMEM_1, PAGE 1
	.switch		>  RTU1_DMEM_1, PAGE 1
	.sysmem		>  RTU1_DMEM_1, PAGE 1
	.cinit		>  RTU1_DMEM_1, PAGE 1
	.rodata		>  RTU1_DMEM_1, PAGE 1
	.rofardata	>  RTU1_DMEM_1, PAGE 1
	.farbss		>  RTU1_DMEM_1, PAGE 1
	.fardata	>  RTU1_DMEM_1, PAGE 1
}
