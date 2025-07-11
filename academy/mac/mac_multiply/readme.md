# MAC (Multiply-Only Mode)

## Introduction

This example acts as a basic demonstration of the usage of MAC module in Multiply-Only mode in ICSSM PRU.

## Overview 

Here's the Overview section for the MAC (Multiply Only Mode) example README:

## Overview

This example demonstrates the usage of PRU's MAC (Multiply and Accumulate) broadside accelerator in its Multiply Only mode. While the MAC module is capable of both multiplication and accumulation operations, this mode specifically showcases its ability to perform high-speed unsigned multiplications without accumulation, making it ideal for applications requiring quick multiplication operations.

The example implements a straightforward multiplication operation between two unsigned numbers:
- Operand 1: 50
- Operand 2: 25

The MAC module performs the multiplication without any accumulation, demonstrating its capability to handle basic arithmetic operations efficiently. The 64-bit result of the multiplication is stored across two PRU registers:
- R26: Contains the lower 32 bits of the result
- R27: Contains the upper 32 bits of the result

This simple demonstration shows how the PRU's hardware acceleration can be utilized for basic arithmetic operations with improved performance compared to software-based multiplication.

# Supported Combinations

 Parameter      | Value
 ---------------|-----------
 ICSSM          | ICSSM0 - PRU0, PRU1, ICSSM1 (only in am261x) - PRU0, PRU1 
 Toolchain      | pru-cgt
 Board          | am261x-lp, am261x-som, am263px-cc, am263px-lp, am263x-cc, am263x-lp
 Example folder | examples/pru_io/crc/

