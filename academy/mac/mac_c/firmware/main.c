/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2021 Texas Instruments Incorporated - http://www.ti.com/
 */

// ***************************************
// *    Global Structure Definitions     *
// ***************************************

#include <stdint.h>


/* R31 is used to generate the "I'm done" back to the ARM */
volatile register uint8_t __R31;

/* Accessing the operands structure within the multiplyParams structure
 * forces the compiler to pair the registers together */
typedef struct {
	uint32_t op1;
	uint32_t op2;
} operands;

#define NUMMACS 256

/*
 * Structure buf is pretty big: uint32 * 2 * 256 = 2kB, or 0x800 memory.
 * Make sure that there is enough Data RAM for your program. This example
 * allocates space for buf in .bss (uninitialized near data section).
 * After you build the example, you can verify the amount of Data RAM the
 * example uses by inspecting the .map file in the outputs folder.
 * The .map file for the MAC example tells us:
 * MEMORY CONFIGURATION --> PRUx_DMEMx: 0x900 memory is used
 * SECTION ALLOCATION MAP: .bss uses 0x800 (this is buf), .stack uses 0x100
 *
 * To modify the size of the stack, edit Makefile > STACK_SIZE.
 *
 * Reference "PRU Optimizing C/C++ Compiler User's Guide", section "Dynamic
 * Memory Allocation" if you want to allocate large arrays from the heap instead
 * of the .bss section.
 */
operands buf[NUMMACS];

/* Need to create a while loop inside main to wait for interrupt from host.
 * The interrupt will signify that a buffer of data has been passed and is
 * ready for MAC processing. This will be passed by rpmsg driver.
 */
int main(void)
{
	uint32_t i;
	uint16_t numMacs = NUMMACS; // Arbitrary number
	uint64_t result = 0;
	volatile uint64_t storeValue = 0;

	/* Dummy data set */
	for (i = 0; i < numMacs; i++) {
		buf[i].op1 = i;
		buf[i].op2 = i+1;
	}

	/* Perform numMacs MAC operations */
	for (i = 0; i < numMacs; i++) {
		result += (uint64_t)buf[i].op1 * (uint64_t)buf[i].op2;
	}

	/*
	 * NOTE!
	 * 
	 * This C code does NOT use the PRU subsystem's MAC hardware
	 * accelerator. Instead, the multiplication is converted by the C
	 * compiler into a series of additions in the generated assembly file.
	 * Test by adding --keep_asm to the compiler flags as per the PRU
	 * Getting Started Labs, and searching the generated main.asm file for
	 * the line number with the multiply.
	 * 
	 * For an example of using assembly code to use the MAC hardware
	 * accelerator, refer to academy/mac/mac.
	 * 
	 * For an example of writing mixed C and assembly code, refer to the
	 * PRU Getting Started Labs under academy/getting_started_labs.
	 */

	/*
	 * FIXME: research, is there a way to call MAC hardware accelerator
	 * directly from C code?
	 */

	storeValue = result;

	/* Nothing to do so halt */
	__halt();
}
