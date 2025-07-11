# MAC (Multiply and Accumulate Mode)

## Introduction

This example acts as a basic demonstration of the usage of MAC module in Multiply and Accuulate mode in ICSSM PRU.

## Overview 

This example demonstrates the capabilities of the PRU's MAC (Multiply and Accumulate) broadside accelerator, which is designed for efficient multiplication and accumulation operations. The MAC module is particularly valuable for mathematical computations that require repeated multiply-and-add operations, such as vector calculations, digital signal processing, and matrix operations.

The example implements a basic vector dot product calculation to showcase the MAC module's functionality. Using two 3-dimensional vectors:
- Vector A = (1, 2, 3)
- Vector B = (4, 5, 6)

The dot product computation performs the following operations:
1. (1 × 4)
2. (2 × 5)
3. (3 × 6)
4. Accumulates all results: (1×4 + 2×5 + 3×6 = 32 or 0x20)

The final result is stored across two PRU registers:
- R26: Contains the lower 32 bits of the result
- R27: Contains the upper 32 bits of the result


# Supported Combinations

 Parameter      | Value
 ---------------|-----------
 ICSSM          | ICSSM0 - PRU0, PRU1, ICSSM1 (only in am261x) - PRU0, PRU1 
 Toolchain      | pru-cgt
 Board          | am261x-lp, am261x-som, am263px-cc, am263px-lp, am263x-cc, am263x-lp
 Example folder | examples/pru_io/mac/

