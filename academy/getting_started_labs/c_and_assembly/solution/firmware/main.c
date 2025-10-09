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

/* Declaration of the external assembly function */
uint32_t assm_add(uint32_t arg1, uint32_t arg2);

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
		 * TODO: use the assembly function to add x and y, then
		 * store the sum in z
		 */
		z = assm_add(x, y);

		/*
		 * TODO: use the assembly function to add a and b, then
		 * store the sum in memory location c
		 */
		c = assm_add(a, b);
	}

	/* This program will not reach __halt because of the while loop */
	__halt();
}
