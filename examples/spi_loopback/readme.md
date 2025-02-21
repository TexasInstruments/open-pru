# SPI Loop Back example

## Introduction

This example provides a base for runing loopback spi connection between a pair of master and slave PRU cores. 
The example uses generic SPI macros introduced in files spi_master_macros.inc and spi_slave_macros.inc to emulate generic SPI master and slave interfaces over PRU GPIO pins.

 ## Overview

 ## Instructions

 ## Limitations

# Supported Combinations

 Parameter      | Value
 ---------------|-----------
 ICSSG          | ICSSG0 - PRU0, PRU1
 Toolchain      | pru-cgt
 Board          | am243x-lp
 Example folder | examples/spi_loopback
