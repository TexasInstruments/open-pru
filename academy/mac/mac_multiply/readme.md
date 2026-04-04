# MAC (Multiply-Only Mode)

## Introduction

This example acts as a basic demonstration of the usage of MAC module in Multiply-Only mode in ICSSM PRU.

## Overview

This example demonstrates the usage of PRU's MAC (Multiply and Accumulate) broadside accelerator in its Multiply Only mode. While the MAC module is capable of both multiplication and accumulation operations, this mode specifically showcases its ability to perform high-speed unsigned multiplications without accumulation, making it ideal for applications requiring quick multiplication operations.

The example implements a straightforward multiplication operation between two unsigned numbers:
- Operand 1: 50
- Operand 2: 25

The MAC module performs the multiplication without any accumulation, demonstrating its capability to handle basic arithmetic operations efficiently. The 64-bit result of the multiplication is stored across two PRU registers:
- R26: Contains the lower 32 bits of the result
- R27: Contains the upper 32 bits of the result

This simple demonstration shows how the PRU's hardware acceleration can be utilized for basic arithmetic operations with improved performance compared to software-based multiplication.

## Supported Combinations

Refer to open-pru/academy/readme.md > Supported processors per-project
for the list of processors that support building this project, and information
about porting this project to other processors.

## Validated HW & SW

This project was tested on hardware with these software versions:

| Processor | Hardware | Software                                |
| --------- | -------- | --------------------------------------- |
| am261x    | TODO     | MCU PLUS SDK TODO, OpenPRU TODO         |
| am263px   | TODO     | MCU PLUS SDK TODO, OpenPRU TODO         |
| am263x    | TODO     | MCU PLUS SDK TODO, OpenPRU TODO         |

## Steps to Run the Example

1. Build and run the PRU firmware.
2. After the firmware finishes running, inspect the PRU register values:
   - R26: lower 32 bits of the multiplication result; expected `0x4E2`
     (1250 decimal = 50 × 25)
   - R27: upper 32 bits; expected `0x00`

