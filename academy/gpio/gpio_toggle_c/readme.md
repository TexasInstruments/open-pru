# GPIO Toggle C

## Introduction

This example acts as a basic demonstration of toggling PRU GPO signals high and low.

## Overview 

This example demonstrates a PRU core driving PRU GPO signals directly by writing to register R30. The PRU toggles bit 4 of R30, which drives PRU GPO4, to generate a digital oscillating signal, showcasing the basic PRU GPO control capabilities of the PRU subsystem.

The PRU firmware is written in C (`firmware/main.c`). The same example written in assembly is in `gpio_toggle`. The R5F core (running FreeRTOS) loads the PRU firmware onto the PRU cores. On AM64x and AM243x, the R5F also configures the board's I/O expander over I2C so that the PRU signals reach the board pins. Without this step the pin stays silent.

The implementation varies slightly depending on the target device:
- For AM64x and AM243x: The example toggles PRU0 GPO4

For AM64x and AM243x, the toggled signal is routed to the J2 header, Pin A13 (in J2 Row A). To verify the GPO pin toggle, the user will require a J2 High Speed Expansion Connector to get the required pin outs and measure the output using standard measurement equipment like oscilloscopes or logic analyzers. If using the TMDS64DC01 EVM as the J2 Expansion header for the AM64x EVM, it corresponds to the Pin 9 in the J2 header.

Cores Tested for AM64x and AM243x: PRU0 and PRU1. Default: Enabled for PRU0.

The default pinmux uses PRU0 GPO4. If the pinmux is modified to use PRU1 GPO4 on AM64x and AM243x, probe Pin 13 of the J4 header on the high speed expansion board instead of Pin 9 of the J2 header.

The oscillating signal generated can be used to validate proper PRU operation and PRU GPO pinmux configuration, making this example particularly useful for initial hardware bring-up and verification of PRU GPO functionality.

## Supported Combinations

Refer to open-pru/academy/readme.md > Supported processors per-project
for the list of processors that support building this project, and information
about porting this project to other processors.

## Validated HW & SW

This project was tested on hardware with these software versions:

| Processor | Hardware | Software                                |
| --------- | -------- | --------------------------------------- |
| am64x     | TMDS64EVM PROC101D, E2  | MCU PLUS SDK 12.01.00.33, OpenPRU v2026.02.00   |
| am243x    | TMDS243EVM PROC101D, E2  | MCU PLUS SDK 12.00.00.26, OpenPRU v2026.02.00   |

## Steps to Run the Example

1. Build and run the PRU firmware
2. Connect a logic analyzer or oscilloscope to header pin below to observe the toggling signal
   - To probe, A13 (J2 Row A) on J2 Header, probe Pin 9 in J2 on TMDS64DC01 EVM (Expansion board) which is connected to the J2 header for AM64x and AM243x

