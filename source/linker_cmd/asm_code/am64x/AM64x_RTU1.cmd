/*
 * AM64x_RTU1.cmd
 *
 * Example Linker command file for linking assembly programs built with the TI-PRU-CGT
 * on AM64x RTU1 cores
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
	PRU1_DMEM_0	: org = 0x00002000 len = 0x00001000
	/* NOTE: Custom split of the second 4 KB of ICSS Data RAMs 0 and 1
	 * split equally between the corresponding RTU and Tx_PRU cores in
	 * each slice */
	RTU1_DMEM_1	: org = 0x00001000 len = 0x00000800
	TX_PRU1_DMEM_1	: org = 0x00001800 len = 0x00000800
	RTU1_DMEM_0	: org = 0x00003000 len = 0x00000800
	TX_PRU1_DMEM_0	: org = 0x00003800 len = 0x00000800

      PAGE 2:
	/* C28 needs to be programmed to point to SHAREDMEM, default is 0 */
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
