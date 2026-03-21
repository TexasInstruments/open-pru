# include

This directory contains definitions that are used by PRU firmware.

The header files are written in C. In order to include a C header file in an
assembly project, the .cdecls directive must be used around the include.
For more information, refer to
[PRU Assembly Language Tools v2.3](https://www.ti.com/lit/spruhv6),
section "Sharing C/C++ Header Files With Assembly Source".

## Register definitions
Register definitions for PRU subsystem & commonly used peripherals:
am243x
am261x
am263Px
am263x
am62x
am64x

## Linux definitions
Include definitions used for interacting with Linux cores:
linux

## Other definitions
hw_types.h - contains general purpose API for HW access
