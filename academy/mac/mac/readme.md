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
   - R26: lower 32 bits of the dot product result; expected `0x20` (32 decimal)
   - R27: upper 32 bits; expected `0x00`
