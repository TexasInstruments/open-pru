# GPIO Toggle

## Introduction

This example acts as a basic demonstration of toggling of SoC GPIO pins using PRU GPIO module in ICSSM PRU.

## Overview 

This example demonstrates the functionality of the PRU enhanced GPIO module, which enables PRUs to directly control SoC GPIO pins through their dedicated PRU GPIO interface. The example implements a simple pin toggling mechanism to generate a digital oscillating signal, showcasing the basic GPIO control capabilities of the PRU subsystem.

The implementation varies slightly depending on the target device:
- For AM64x: The example toggles PR0_PRU0_GPIO4

For AM64x, the toggled signal is routed to the J2 header, Pin A13 (in J2 Row A). To verify the GPIO toggle, the user will require a J2 High Speed Expansion Connector to get the required pin outs and measure the output using standard measurement equipment like oscilloscopes or logic analyzers. If using the TMDS64DC01 EVM as the J2 Expansion header for the AM64x EVM, it corresponds to the Pin 9 in the J2 header.

Cores Tested for AM64x: PRU0 and PRU1

The oscillating signal generated can be used to validate proper PRU operation and GPIO configuration, making this example particularly useful for initial hardware bring-up and verification of PRU GPIO functionality.

## Supported Combinations

Refer to open-pru/academy/readme.md > Supported processors per-project
for the list of processors that support building this project, and information
about porting this project to other processors.

## Validated HW & SW

This project was tested on hardware with these software versions:

| Processor | Hardware | Software                                |
| --------- | -------- | --------------------------------------- |
| am64x     | PROC101D, E2  | MCU PLUS SDK 12.01.00.33, OpenPRU v2026.02.00   |

## Steps to Run the Example

1. Build and run the PRU firmware
2. Connect a logic analyzer or oscilloscope to header pin below to observe the toggling signal
   - To probe, A13 (J2 Row A) on J2 Header, probe Pin 9 in J2 on TMDS64SDC01 EVM (Expansion board) which is connected to the J2 header for AM64x

