# GPIO Toggle

## Introduction

This example acts as a basic demonstration of toggling of SoC GPIO pins using PRU GPIO module in ICSSM PRU.

## Overview 

This example demonstrates the functionality of the PRU enhanced GPIO module, which enables PRUs to directly control SoC GPIO pins through their dedicated PRU GPIO interface. The example implements a simple pin toggling mechanism to generate a digital oscillating signal, showcasing the basic GPIO control capabilities of the PRU subsystem.

The implementation varies slightly depending on the target device:
- For AM261x: The example toggles PR1_PRU1_GPIO4
- For AM263x, AM263Px, AM64x and AM243x: The example toggles PR0_PRU0_GPIO4

For AM263x and AM263Px, the toggled signal is routed to a readily accessible location on the development boards - specifically to Pin 17 of the Boosterpack header J2 on the respective LaunchPads. This makes it convenient for users to observe and measure the output using standard measurement equipment like oscilloscopes or logic analyzers.

For AM64x and AM243x, the toggled signal is routed to the J2 header, Pin A13 (in J2A). To verify the GPIO toggle, the user will require a J2 High Speed Expansion Connector to get the required pin outs and measure the output using standard measurement equipment like oscilloscopes or logic analyzers. If using the TMDS64DC01 EVM, it corresponds to the Pin 9 in the J2 header.

The oscillating signal generated can be used to validate proper PRU operation and GPIO configuration, making this example particularly useful for initial hardware bring-up and verification of PRU GPIO functionality.

## Supported Combinations

Refer to open-pru/academy/readme.md > Supported processors per-project
for the list of processors that support building this project, and information
about porting this project to other processors.

## Validated HW & SW

This project was tested on hardware with these software versions:

| Processor | Hardware | Software                                |
| --------- | -------- | --------------------------------------- |
| am261x    | FIXME     | MCU PLUS SDK FIXME, OpenPRU FIXME         |
| am263px   | FIXME     | MCU PLUS SDK FIXME, OpenPRU FIXME         |
| am263x    | FIXME     | MCU PLUS SDK FIXME, OpenPRU FIXME         |
| am64x     | FIXME     | MCU PLUS SDK FIXME, OpenPRU FIXME         |
| am243x    | FIXME     | MCU PLUS SDK FIXME, OpenPRU FIXME         |

## Steps to Run the Example

1. Build and run the PRU firmware
2. Connect a logic analyzer or oscilloscope to header pin below to observe the toggling signal
   - BP.17 (J2) for AM263x/AM263Px
   - A13 (J2A) on J2 Header or Pin 9 in J2 on TMDS64SDC01 EVM for AM64x and AM243x
   
