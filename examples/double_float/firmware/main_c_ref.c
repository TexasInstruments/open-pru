/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2024-2025 Texas Instruments Incorporated - http://www.ti.com/
 */

/**
 * @file   main_c_ref.c
 *
 * @brief  IEEE 754 double-precision multiply: C library reference.
 *
 * This file shows the equivalent C code for the optimised PRU assembly in
 * main.asm.  When compiled for the PRU (which has no hardware FPU), the C
 * compiler generates a call to the software double-precision multiply routine
 * from the C runtime library (__mpy2d).  That software path does NOT use the
 * PRU MAC broadside accelerator, which is why the hand-written assembly in
 * main.asm is significantly faster.
 *
 * Typical cycle counts (200 MHz PRU clock):
 *   Assembly with MAC hardware accelerator : ~85 cycles  (~425 ns)
 *   C library  __mpy2d  (software FP)      : ~200+ cycles
 *
 * Memory layout used by both implementations (PRU Data RAM, byte offsets):
 *   0x00  operand A  low  word  (bits 31:0)
 *   0x04  operand A  high word  (bits 63:32)
 *   0x08  operand B  low  word
 *   0x0C  operand B  high word
 *   0x10  result     low  word  (written by the multiply)
 *   0x14  result     high word
 *
 * NOTE: This file is provided as a reference and is NOT compiled into the
 * default PRU firmware project (which uses main.asm).  To build it instead,
 * remove main.asm from the project and add this file.
 */

#include <stdint.h>

/* PRU Data RAM memory map (byte offsets from address 0x0000) */
#define OFFS_A_LO   0x00u
#define OFFS_A_HI   0x04u
#define OFFS_B_LO   0x08u
#define OFFS_B_HI   0x0Cu
#define OFFS_R_LO   0x10u
#define OFFS_R_HI   0x14u

/**
 * Union to overlay a 64-bit double over two 32-bit words.
 * w[0] = bits [31:0], w[1] = bits [63:32] (little-endian layout).
 */
typedef union
{
    double   d;
    uint32_t w[2];
} double_u;

/**
 * double_multiply_c
 *
 * Reads two doubles from PRU Data RAM, multiplies them using the C '*'
 * operator (which the PRU compiler converts to a __mpy2d library call),
 * and writes the result back.
 *
 * For comparison, the equivalent optimised assembly (main.asm) uses four
 * 32x32-bit hardware MAC operations to compute the same result in ~78 PRU
 * cycles, whereas this software path typically takes 200+ cycles.
 */
static void double_multiply_c(void)
{
    double_u a, b, result;

    /* Load operands directly from PRU Data RAM base addresses */
    a.w[0] = *(volatile uint32_t *)OFFS_A_LO;
    a.w[1] = *(volatile uint32_t *)OFFS_A_HI;
    b.w[0] = *(volatile uint32_t *)OFFS_B_LO;
    b.w[1] = *(volatile uint32_t *)OFFS_B_HI;

    /*
     * C library (software FP) multiplication.
     * The PRU has no hardware FPU, so the compiler calls __mpy2d from
     * the C runtime library.  math.h is not required for the '*' operator
     * on doubles; it is handled by the compiler's runtime support.
     */
    result.d = a.d * b.d;

    /* Store result back to PRU Data RAM */
    *(volatile uint32_t *)OFFS_R_LO = result.w[0];
    *(volatile uint32_t *)OFFS_R_HI = result.w[1];
}

int main(void)
{
    double_multiply_c();
    __halt();
}
