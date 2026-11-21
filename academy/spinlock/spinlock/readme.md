# Spinlock (assembly)

## Introduction

This example shows how a PRU takes and releases the PRU_ICSSG local hardware
spinlock from assembly. The same pattern written in C, with both PRUs
contending for one lock, is in `spinlock_c`.

## Overview

The PRU reaches the spinlock accelerator over the broadside interface with
`XIN`/`XOUT` (XID `0x90`). The lock id (`0`-`63`) goes in `R1.b0`, and an
acquire returns its status in `R1.b3`.

`firmware/spinlock_example.asm` defines two macros:

- `m_spinlock_acquire` busy-waits until bit 0 of the status is set, which
  means this core now owns the lock.
- `m_spinlock_release` frees the lock.

`main` takes the lock once, leaves room for a critical section, releases the
lock and halts. The R5F core (running FreeRTOS) loads the firmware onto PRU0 and
PRU1 of ICSSG0. The firmware drives no pins.

For how the spinlock accelerator works, see the PRU Academy page
AM243x/AM64x PRU Academy > Hardware information > PRU Broadside Accelerators > Spinlock:

- AM64x: https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AXw28VmX5TFHmSGgvY-0qw__AM64-ACADEMY__WI1KRXP__LATEST
- AM243x: https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__Ac5K7fXpNm5GbT1YWvI-8g__AM24X-ACADEMY__ZPSnq-h__LATEST

## Supported Combinations

Refer to open-pru/academy/readme.md > Supported processors per-project
for the list of processors that support building this project, and information
about porting this project to other processors.

## Validated HW & SW

This project was tested on hardware with these software versions:

| Processor | Hardware | Software                                          |
| --------- | -------- | ------------------------------------------------- |
| am243x    | FIXME    | MCU PLUS SDK FIXME, OpenPRU FIXME                 |
| am64x     | FIXME    | MCU PLUS SDK FIXME, OpenPRU FIXME                 |

## Steps to Run the Example

1. Build the project from the top-level make (`make` in `academy`, or in this
   folder), with `DEVICE` set to `am243x` or `am64x` in `imports.mak`.
2. Load and run the R5F image. It loads the firmware onto PRU0 and PRU1.
3. Connect to a PRU core in CCS and step through `main` to watch `R1.b3` while
   the core acquires and releases the lock.
