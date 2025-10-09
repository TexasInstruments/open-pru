/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2018-2025 Texas Instruments Incorporated - http://www.ti.com/
 */

#include <stdint.h>

/* TODO: define c */
/* a, b, and c are stored in a defined location in PRU memory */
#define a  (*((volatile unsigned int *)0x110))
#define b  (*((volatile unsigned int *)0x114))
#define c  (*((volatile unsigned int *)0x118))

/* Declaration of the inline assembly function */
uint32_t inline_assm_add(uint32_t arg1, uint32_t arg2);

/*
 * main.c
 */
void main(void)
{
	/* TODO: define y & z */
	/* The compiler decides where to store x, y, and z */
	uint32_t x = 1;
	uint32_t y = 2;
	uint32_t z = 0;

	a = 1;
	b = 2;

	while(1) {
		/*
		 * TODO: use the inline assembly function to add x and y, then
		 * store the sum in z
		 */
		z = inline_assm_add(x, y);

		/*
		 * TODO: use the inline assembly function to add a and b, then
		 * store the sum in memory location c
		 */
		c = inline_assm_add(a, b);
	}

	/* This program will not reach __halt because of the while loop */
	__halt();
}

/*
 * inline_assm_add
 */
uint32_t inline_assm_add(uint32_t arg1, uint32_t arg2)
{
	/* 
	 * Function input arguments are stored in R14-R29.
	 * So the 32 bit value in arg1 is stored in R14, and the 32 bit value in
	 * arg2 is stored in R15.
 	 * 
	 * For more details about how function arguments
	 * are stored in registers, reference the document "PRU Optimizing C/C+
	 * Compiler User's Guide", section "Function Structure and Calling
	 * Conventions"
	 */

	/*
	 * For information about using __asm() to add inline assembly code,
	 * reference the document "PRU Optimizing C/C+ Compiler User's Guide",
	 * section "The __asm Statement".
	 *
	 * Note: You can use the newline and tab characters to place multiple
	 * assembly instructions in a single __asm() call. For example:
	 *
	 * __asm("   assem_instr_1 \n\t assem_instr_2 \n\t assem_instr_3");
	 */

	/*
	 * TODO: add arg1 and arg2
	 */
	__asm("   ADD r14, r14, r15");

	/*
	 * TODO: return the sum. The return value should be in R14.
	 */
	return arg1;
}
