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

# Supported Combinations

 Parameter      | Value
 ---------------|-----------
 ICSSM          | ICSSM0 - PRU0, PRU1, ICSSM1 (only in am261x) - PRU0, PRU1 
 Toolchain      | pru-cgt
 Board          | am261x-lp, am263px-lp, am263x-lp
 Example folder | examples/pru_io/gpio_toggle/

