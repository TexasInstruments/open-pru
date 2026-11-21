# Spinlock (C)

## Introduction

This example shows PRU0 and PRU1 sharing a resource through the PRU_ICSSG local
hardware spinlock. Each core drives a debug pin while it holds the lock, so a
scope or logic analyzer shows the two cores taking turns. The same lock, used
from assembly only, is in `spinlock`.

## Overview

The PRU reaches the spinlock accelerator over the broadside interface with
`XIN`/`XOUT` (XID `0x90`, lock id `0`-`63` in `R1.b0`, status in `R1.b3`).

- `firmware/spinlock.asm` has the C-callable `spinlock_acquire()` (one
  non-blocking attempt; returns 1 if this core got the lock) and
  `spinlock_release()`.
- `firmware/main.c` spins on `spinlock_acquire()`, drives its debug pin high,
  holds the lock (1000 cycles on PRU0, 2000 on PRU1), drives the pin low,
  releases the lock, then waits `OUTSIDE_TIME` cycles before trying again.

The R5F core (running FreeRTOS) sets the pinmux for both debug pins and loads
the firmware onto PRU0 and PRU1 of ICSSG0. On the AM64x and AM243x EVMs it also
sets the board's I/O expander over I2C so that the PRU signals reach the
expansion header.

For how the spinlock accelerator works, see the PRU Academy page
AM243x/AM64x PRU Academy > Hardware information > PRU Broadside Accelerators > Spinlock:

- AM64x: https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AXw28VmX5TFHmSGgvY-0qw__AM64-ACADEMY__WI1KRXP__LATEST
- AM243x: https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__Ac5K7fXpNm5GbT1YWvI-8g__AM24X-ACADEMY__ZPSnq-h__LATEST

## Debug pins

| Board              | PRU0                                      | PRU1                                |
| ------------------ | ----------------------------------------- | ----------------------------------- |
| AM64x / AM243x EVM | GPO4: J2 pin A13 (TMDS64DC01: J2, pin 9)  | GPO4: TMDS64DC01 J4, pin 13         |
| AM243x LaunchPad   | GPO4: BoosterPack J2.17                   | GPO0: BoosterPack J2.11             |

On the EVMs, the pins are reached through a J2 High Speed Expansion Connector
such as the TMDS64DC01 expansion board, as in `gpio/gpio_toggle`.

The LaunchPad does not route PRU1 GPO4 to a header, so its PRU1 build passes
`-DDEBUG_PIN_SHIFT=0`.

## Usage notes

Measured on LP-AM243 (ICSSG0 PRU0 and PRU1):

- The lock is not fair. A core that tries to re-acquire straight after releasing wins
  every time, because the other core's retry loop is longer than that gap. In a test
  version of `main.c` with no gap, PRU1 never got the lock. Do some work outside the lock before trying
  again, as `main.c` does with `OUTSIDE_TIME`.
- An acquire by the core that already holds the lock also returns 1. The status means
  "this core holds the lock"; it is not a nesting count, so do not take the lock
  recursively.
- An acquire is one attempt. Waiting for the lock has no upper bound: it lasts as long
  as the other core holds it, so keep critical sections short.

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
2. Load and run the R5F image. It sets the pinmux and loads the firmware onto
   PRU0 and PRU1.
3. Probe the two debug pins in the table above. Each pin goes high only while
   its core holds the lock, and the two pins are never high at the same time.
