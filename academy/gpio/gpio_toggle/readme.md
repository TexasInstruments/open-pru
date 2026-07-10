# GPIO Toggle

## Introduction

This example acts as a basic demonstration of toggling of SoC GPIO pins using PRU GPIO module in ICSSM PRU.

## Overview 

This example demonstrates the functionality of the PRU enhanced GPIO module, which enables PRUs to directly control SoC GPIO pins through their dedicated PRU GPIO interface. The example implements a simple pin toggling mechanism to generate a digital oscillating signal, showcasing the basic GPIO control capabilities of the PRU subsystem.

The implementation varies slightly depending on the target device:
- For AM261x: The example toggles PR1_PRU1_GPIO4
- For AM263x and AM263Px: The example toggles PR0_PRU0_GPIO4

In both cases, the toggled signal is routed to a readily accessible location on the development boards - specifically to Pin 17 of the Boosterpack header J2 on the respective LaunchPads. This makes it convenient for users to observe and measure the output using standard measurement equipment like oscilloscopes or logic analyzers.

The oscillating signal generated can be used to validate proper PRU operation and GPIO configuration, making this example particularly useful for initial hardware bring-up and verification of PRU GPIO functionality.

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

1. Build and run the PRU firmware
2. Connect a logic analyzer or oscilloscope to header pin BP.17 (J2) and
   observe the toggling signal

