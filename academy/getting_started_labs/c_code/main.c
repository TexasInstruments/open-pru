/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2018-2025 Texas Instruments Incorporated - http://www.ti.com/
 */
#include <stdint.h>

/* INSERT CODE: define c */
/* a, b, and c are stored in a defined location in PRU memory */
#define a  (*((volatile unsigned int *)0x110))
#define b  (*((volatile unsigned int *)0x114))

/*
 * main.c
 */
void main(void)
{
	/* INSERT CODE: define y & z */
	/* The compiler decides where to store x, y, and z */
	uint32_t x = 1;

	a = 1;
	b = 2;

	while(1) {
		/*
		 * INSERT CODE: store the sum of x and y in z
		 */

		/*
		 * INSERT CODE: store the sum of the numbers at memory locations a and
		 * b in memory location c
		 */
	}

	/* This program will not reach __halt because of the while loop */
	__halt();
}

