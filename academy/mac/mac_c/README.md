# MAC Multiply-Accumulate (C)

## Introduction

This example demonstrates the PRU MAC (Multiply and Accumulate) hardware
accelerator using PRU C firmware.

## Overview

This example demonstrates the PRU MAC (Multiply and Accumulate) hardware
accelerator using PRU C firmware. It allocates 256 pairs of 32-bit operands
in PRU Data RAM, fills them with sequential values, performs 256
multiply-accumulate operations accumulating a 64-bit result, stores the
result, and halts.

The example is adapted from the PRU Software Support Package (PSSP)
`PRU_MAC_Multiply_Accum` example.

The 256-entry operand buffer (`buf`) uses 2 KB (0x800 bytes) of PRU Data RAM.
After building, inspect the `.map` file to verify the total Data RAM usage
fits within the target device's Data RAM.

## Supported Combinations

Refer to open-pru/academy/readme.md > Supported processors per-project
for the list of processors that support building this project, and information
about porting this project to other processors.

## Validated HW & SW

- Board: SK-AM62B, TMDS64EVM
- TI PRU Code Generation Tools (CGT) v2.3.3
- Code Composer Studio 20.x
- OpenPRU v2026.01.00

## Running and Validating

1. Load the built ELF onto the target PRU core using CCS. Refer to the **PRU
   Getting Started Labs > Lab 4: How to Initialize the PRU** for details.
2. Run the PRU core.
3. Halt execution at `__halt()`.
4. Inspect the value in `storeValue`. Refer to the **PRU Getting Started Labs >
   Lab 5: How to debug PRU firmware** for details.
   * The expected accumulated result in storeValue is 5,592,320 = 0x555500 [1]

[1] Calculation for storeValue:
Each iteration contributes i * (i+1) = i² + i, so:
result = Σ(i=0 to 255) (i² + i) = Σi² + Σi
Using the standard closed-form formulas for sums up to n=255:
 - Σi (0→255) = 255×256/2 = 32,640
 - Σi² (0→255) = 255×256×511/6 = 5,559,680
 - result = 32,640 + 5,559,680 = 5,592,320 = 0x555500
