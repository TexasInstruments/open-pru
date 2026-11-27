# GPIO Toggle C

## Introduction

This example acts as a basic demonstration of toggling PRU GPO signals high and low.

## Overview 

This example demonstrates a PRU core driving PRU GPO signals directly by writing to register R30. The PRU toggles bit 4 of R30, which drives PRU GPO4, to generate a digital oscillating signal, showcasing the basic PRU GPO control capabilities of the PRU subsystem.

The PRU firmware is written in C (`firmware/main.c`). The same example written in assembly is in `gpio_toggle`. The R5F core (running FreeRTOS) loads the PRU firmware onto the PRU cores. On AM64x and AM243x, the R5F also configures the board's I/O expander over I2C so that the PRU signals reach the board pins. Without this step the pin stays silent.

The implementation varies slightly depending on the target device:
- For AM64x and AM243x: The example toggles PRU0 GPO4 and PRU1 GPO4

For AM64x and AM243x, the toggled signals are routed to the J2 header of the EVM: PRU0 GPO4 on Pin A13 (J2 Row A) and PRU1 GPO4 on Pin B13 (J2 Row B). To verify the GPO pin toggle, the user will require a J2 High Speed Expansion Connector to get the required pin outs and measure the output using standard measurement equipment like oscilloscopes or logic analyzers. If using the TMDS64DC01 EVM as the J2 Expansion header for the AM64x/AM243x EVM, PRU0 GPO4 corresponds to Pin 9 in the J2 header and PRU1 GPO4 corresponds to Pin 13 in the J4 header.

Cores Tested for AM64x and AM243x: PRU0 and PRU1.
The default pinmux enables GPO4 on both PRU cores for AM64x and AM243x, and the R5F loads the same firmware on PRU0 and PRU1, so both toggle independently:
- PRU0 GPO4: Pin A13 of the EVM J2 header. On the TMDS64DC01 high speed expansion board, this is Pin 9 of J2.
- PRU1 GPO4: Pin B13 of the EVM J2 header. On the TMDS64DC01 high speed expansion board, this is Pin 13 of J4.

The oscillating signal generated can be used to validate proper PRU operation and PRU GPO pinmux configuration, making this example particularly useful for initial hardware bring-up and verification of PRU GPO functionality.

## Expected output

The pin stays high for 0.5 s and low for 0.5 s: a 1 Hz square wave with a duty
cycle of about 50%. The cycle count calculation, including the loop overhead,
is in the comment above `main()` in `firmware/main.c`.

It assumes a 200 MHz PRU clock. If your clock is different, the frequency 
changes in proportion.

The PRU core clock depends on the device and on how the host software
configures it. Different devices support different PRU core frequencies.
The [PRU Subsystem Features Comparison](https://www.ti.com/lit/sprac90)
app note lists the maximum frequency per device. Check the device datasheet 
and TRM, and the clock settings for the frequencies you can use.

For how many cycles each PRU instruction takes, and how to measure the
cycle count on hardware or in the simulator, see the
[PRU timing reference](https://github.com/TexasInstruments/pru-simulator/blob/main/skills/pru-assembly/references/timing.md)
in the pru-simulator repository.

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
   - To probe PRU0 GPO4, A13 (J2 Row A) on J2 Header, probe Pin 9 in J2 on TMDS64DC01 EVM (Expansion board) which is connected to the J2 header for AM64x and AM243x
   - To probe PRU1 GPO4, B13 (J2 Row B) on J2 Header, probe Pin 13 in J4 on TMDS64DC01 EVM (Expansion board) which is connected to the J2 header for AM64x and AM243x

## Debug steps

If the pin does not toggle, work through steps 1-4. Steps 5 and 6 are
firmware edits to try once the basic example works.

1. **Is the R5F application running?** Open the UART console. The host
   application asserts on most initialization failures (`Board_driversOpen()`, I/O expander, `PRUICSS_initMemory()`, `PRUICSS_loadFirmware()`). If it halts with an assert, the PRU is never
   started. `Drivers_open()` and `PRUICSS_open()` return values are not
   checked in this example.
2. **Is the PRU running?** In CCS, connect to the PRU core (for example
   PRU_ICSSG0_PRU0) and halt it. R30 should be `0x10` or `0x00`, and the
   program counter should be inside `main()`. Resume and halt a few
   times. If R30 never changes, the firmware is not running.
3. **Is the pin routed to the header?** On AM64x and AM243x the
   I/O expander must be set (done in `gpio_toggle_c.c`). Also check that GPO4
   is enabled for the right PRU core in `example.syscfg`.
4. **Are you probing the right header pin?** PRU0 GPO4 and PRU1 GPO4 are
   different pins. See the Overview for which header pin belongs to which core.
5. **Speed it up for a scope.** Set `TOGGLE_DELAY_CYCLES` in `main.c` to
   `100000`, which gives a 1 kHz square wave at 200 MHz.
6. **Toggle all pins at once.** In `main.c` change `gpio = 0x10;` to
   `gpio = 0xFFFFFFFF;`. Pins only show a signal if they are pinmuxed as
   PRU GPOs in `example.syscfg`.
