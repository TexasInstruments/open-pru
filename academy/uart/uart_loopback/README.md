# UART Loopback

## Overview

This example demonstrates the PRU UART peripheral by configuring it at 115200
baud in loopback mode, transmitting 6 characters ("Hello!"), receiving them
back through the internal loopback path, and halting. No external wiring is
required because loopback mode connects the transmit output to the receive
input inside the UART block.

The example is adapted from the PRU Software Support Package (PSSP)
`PRU_Hardware_UART` example.

## Supported Combinations

 Parameter      | Value
 ---------------|-----------
 PRU subsystem  | PRUSS0 - PRU0
 Toolchain      | pru-cgt
 Board          | am62x-sk
 Example folder | academy/uart/uart_loopback/

For portability to other devices, see [academy/readme.md §Supported processors per-project](../../readme.md#supported-processors-per-project).

## Hardware

No external connections are required. The UART operates in internal loopback
mode. To route UART signals to physical pins, disable loopback (clear
`CT_UART.MCTR`) and configure the appropriate pinmux before running the
firmware.

## Validated HW & SW

- Board: SK-AM62B
- TI PRU Code Generation Tools (CGT) v2.3.3
- Code Composer Studio 20.x
- OpenPRU v2026.01.00

## Running and Validating

1. Load the built ELF onto the AM62x-SK PRU0 core using CCS. Refer to the **PRU
   Getting Started Labs > Lab 4: How to Initialize the PRU** for details.
2. Run the PRU core.
3. Halt execution at `__halt()`.
4. Inspect the `buffer` array. Refer to the **PRU Getting Started Labs >
   Lab 5: How to debug PRU firmware** for details.
   * The first 6 bytes should contain `0x48 0x65 0x6C 0x6C 0x6F 0x21` ("Hello!").
