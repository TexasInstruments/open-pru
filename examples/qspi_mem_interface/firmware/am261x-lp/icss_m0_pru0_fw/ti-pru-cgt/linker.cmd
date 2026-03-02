/*
 * AM261x_PRU0.cmd
 *
 * Example Linker command file for linking assembly programs built with the TI-PRU-CGT
 * on AM261x PRU0 cores
 */

/* Specify the System Memory Map */
MEMORY
{
      PAGE 0:
	/* 16 KB PRU Instruction RAM */
	PRU_IMEM	: org = 0x00000000 len = 0x00004000

      PAGE 1:
	/* Data RAMs */
	/* 8 KB PRU Data RAM 0 */
	PRU0_DMEM_0	: org = 0x00000000 len = 0x00002000
	/* 8 KB PRU Data RAM 1 */
	PRU0_DMEM_1	: org = 0x00002000 len = 0x00002000

      PAGE 2:
	/* C28 needs to be programmed to point to SHAREDMEM, default is 0 */
	/* 32 KB PRU Shared RAM */
	PRU_SHAREDMEM	: org = 0x00010000 len = 0x00008000
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

