# INTC Example with MCU+ Core 

## Introduction

This example acts as a basic demonstration of generation of interrupts to and from PRU core using INTC module in ICSSM PRU.

## Overview 

This example demonstrates the bidirectional interrupt communication capabilities between the PRU and R5F cores using the PRU-ICSS Interrupt Controller (INTC) module. The example implements a handshaking mechanism through interrupts, showcasing both the generation and handling of system events.

The interrupt flow in this example occurs in the following sequence:
1. The PRU core initiates the communication by generating multiple interrupts to the R5F core
2. The R5F core counts and processes these interrupts until a predetermined maximum count is reached
3. Upon reaching the maximum count, the R5F core generates a response interrupt back to the PRU core
4. The PRU core, upon receiving this interrupt, writes a specific validation key (0x12345678) to its register R11

The successful completion of this interrupt sequence can be verified by checking the PRU register R11 for the presence of the key value (0x12345678). This serves as a confirmation that:
- The PRU successfully generated interrupts to R5F
- The R5F core properly received and processed these interrupts
- The R5F successfully generated a response interrupt
- The PRU correctly received and handled the response interrupt

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
2. After the firmware finishes running, Inspect PRU register R11. The expected
   value is `0x12345678`, confirming
   that the full PRU ↔ R5F interrupt handshake completed successfully.

