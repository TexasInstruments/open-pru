/*
 * AM263x_PRU1.cmd
 *
 * Example Linker command file for linking assembly programs built with the TI-PRU-CGT
 * on AM263x PRU1 cores
 */

/* Specify the System Memory Map */
MEMORY
{
      PAGE 0:
	/* 16 KB PRU Instruction RAM */
	PRU_IMEM	: org = 0x00000000 len = 0x00004000

      PAGE 1:
	/* Data RAMs */
	/* 8 KB PRU Data RAM 1 */
	PRU1_DMEM_1	: org = 0x00000000 len = 0x00002000
	/* 8 KB PRU Data RAM 0 */
	PRU0_DMEM_0	: org = 0x00002000 len = 0x00002000

      PAGE 2:
	/* C28 needs to be programmed to point to SHAREDMEM, default is 0 */
	/* 32 KB PRU Shared RAM */
	PRU_SHAREDMEM	: org = 0x00010000 len = 0x00008000
}

/* Specify the sections allocation into memory */
SECTIONS {

	.text		>  PRU_IMEM, PAGE 0
	.stack		>  PRU1_DMEM_1, PAGE 1
	.bss		>  PRU1_DMEM_1, PAGE 1
	.cio		>  PRU1_DMEM_1, PAGE 1
	.data		>  PRU1_DMEM_1, PAGE 1
	.switch		>  PRU1_DMEM_1, PAGE 1
	.sysmem		>  PRU1_DMEM_1, PAGE 1
	.cinit		>  PRU1_DMEM_1, PAGE 1
	.rodata		>  PRU1_DMEM_1, PAGE 1
	.rofardata	>  PRU1_DMEM_1, PAGE 1
	.farbss		>  PRU1_DMEM_1, PAGE 1
	.fardata	>  PRU1_DMEM_1, PAGE 1
}
