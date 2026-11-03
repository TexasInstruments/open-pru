# UART Print (electronica 2026)

## Introduction

ICSSG0 PRU0 bit-bangs a 115200 baud 8N1 UART transmitter on one direct GPO
pin. It writes the string `Hello 2026` to PRU0 DRAM0, sends it, waits about
1 ms with the line idle high, and sends it again, forever. The firmware was
developed in TI's pru-simulator (`source/uart_print.asm`) and then ported to
the AM243x LaunchPad. The R5F (MCU+ SDK, FreeRTOS) configures pin mux and the
PRU clock, and then loads and starts PRU0 only.

The porting effort, including what changed between the simulator and silicon,
is documented in [docs/porting_report.md](docs/porting_report.md).

## Supported Combinations

| Processor | Board            | PRU core    | Host              |
| --------- | ---------------- | ----------- | ----------------- |
| AM243x    | AM243x LaunchPad | ICSSG0 PRU0 | r5fss0-0 FreeRTOS |

Refer to open-pru/examples/readme.md > Supported processors per-project for
porting information.

## Validated HW & SW

| Processor | Hardware                 | Software                                  |
| --------- | ------------------------ | ----------------------------------------- |
| AM243x    | LP-AM243, Rev A          | MCU+ SDK 12.01.00.33, CCS 21.0.1, ti-cgt-pru 2.3.3, SysConfig 1.28.1. Hardware run 2026-11-03 with a PicoScope UART decode (see Validate) |

The project builds with MCU+ SDK 12.01.00.33. The open-pru release notes list
MCU+ SDK 11.1 - 12.0 as validated for AM243x.

## Overview

### What the firmware does

1. Drive TX (R30 bit 0) high, the UART idle level.
2. Write `Hello 2026` and a null terminator to DRAM0 offset 0 with one
   12-byte `SBCO`.
3. Read the string from DRAM0 one byte at a time. Send each byte as a start
   bit (low), 8 data bits LSB first and a stop bit (high), 1736 PRU cycles
   per bit.
4. At the null terminator, hold TX high for 200,000 cycles (1 ms), then go
   back to step 3.

Because step 3 reads DRAM0 on every pass, a new string written into DRAM0
(for example from the CCS memory browser) is sent from the next pass on. It
must end with a 0 byte and be at most 255 bytes long. A write during a pass
can mix the old and the new string in that one pass.

### DRAM0 layout

PRU0 local address 0x0000 = R5F / CCS global address 0x30000000.

| Offset        | Content                                     |
| ------------- | ------------------------------------------- |
| 0x0000-0x0009 | `Hello 2026` (`48 65 6C 6C 6F 20 32 30 32 36`) |
| 0x000A        | 0, string terminator                        |
| 0x000B        | 0, padding of the 12-byte `SBCO`            |

The R5F clears PRU0 DRAM before it starts the core.

### PRU resources

| Resource   | Setting                                                    |
| ---------- | ---------------------------------------------------------- |
| Core       | ICSSG0 PRU0 only (PRU1, RTU_PRU and TX_PRU not loaded)     |
| Core clock | 200 MHz (`example.syscfg`, `coreClk`). Bit timing is cycle-counted for 200 MHz |
| GPO mode   | Direct GPIO, GPCFG0 GP_MUX_SEL = 0 (set by R5F)            |
| TX         | R30 bit 0 = PRG0_PRU0_GPO0, pad PRG0_PRU0_GPO0, mux mode 0, no pull |
| R31        | Not used                                                   |

### Timing (200 MHz, 115200 baud = 1736.1 cycles per bit)

| Item                        | Measured (pru-simulator, built `.out`) |
| --------------------------- | --------------------------------------- |
| Start bit                   | 1739 cycles                             |
| Data bit                    | 1742 (bit = 1) / 1743 (bit = 0) cycles; bit 7: 1741 / 1742 |
| Stop bit                    | 1745 cycles (includes reading the next byte) |
| One byte, start to start    | 17425 cycles = 87.1 us (ideal 17361): 114,780 baud, 0.37 % slow |
| Whole string (10 bytes)     | 871 us                                  |
| Idle gap after the string   | 1.000 ms                                |
| Repeat period               | 374,245 cycles = 1.871 ms               |

A UART receiver typically tolerates about 2 % baud error, so 0.37 % leaves a
wide margin. The bit timing is the same, cycle for cycle, as in the simulator
source.

### Limitations

- **Clock dependent.** At any PRU clock other than 200 MHz the baud rate is
  wrong (for example 300 MHz gives 172 kbaud). Change `BAUD_COUNT` with the
  clock.
- **No host interface.** The R5F only loads PRU0. To change the string, write
  DRAM0 from CCS.

## Steps to Run the Example

### Hardware setup

| Signal | PRU signal                   | LP header      | Connect to                         |
| ------ | ---------------------------- | -------------- | ---------------------------------- |
| TX     | PRG0_PRU0_GPO0               | BP.33 (J4)     | PicoScope channel A (or UART RX of a 3.3 V USB-UART adapter) |
| GND    | -                            | BP.20 (J2)     | PicoScope / adapter GND            |

- The output is 3.3 V push-pull, idle high.
- The LaunchPad headers are labelled with BoosterPack pin numbers. BP.33 for
  PRG0_PRU0_GPO0 comes from the AM243x LaunchPad pin mapping in
  `examples/LCD_interface/documents/PRU IO - LCD Interface.pptx` (slide 6:
  PRU0 GPO0/1/2 = BP.33/32/31).

### Build and run

1. Build the PRU project `uart_print_am243x-lp_icss_g0_pru0_fw_ti-pru-cgt`
   first. It generates `firmware/am243x-lp/pru0_load_bin.h`.
2. Build the R5F project `uart_print_am243x-lp_r5fss0-0_freertos_ti-arm-clang`.
3. Load and run the R5F project. Only PRU0 of ICSSG0 is loaded and started.
   The UART console prints
   `UART print: ICSSG0 PRU0 running, 115200 8N1 TX on PRG0_PRU0_GPO0 (BP.33)`.

### Validate with a PicoScope

Suggested settings. The hardware run on 2026-11-03 passed with a PicoScope
UART decode; the exact settings used were not recorded in this readme:

1. Channel A: DC, +/-5 V range.
2. Serial decoding: UART/RS-232 on channel A, 115200 baud, 8 data bits, no
   parity, 1 stop bit, LSB first, idle high, threshold about 1.65 V.
3. Trigger: pulse width on channel A, positive pulse longer than 500 us,
   threshold 1.65 V, repeat mode, pre-trigger about 10 %. The only high
   pulse that long is the 1 ms idle gap, so each capture starts at the first
   start bit of the string. Inside the string the longest high level is 9 bit
   times (78 us).
4. Timebase about 100 us/div (1 ms window) shows the whole string.

The decoder shows `48 65 6C 6C 6F 20 32 30 32 36` (`Hello 2026`) with no
framing errors. In the CCS memory browser, 0x30000000 holds the same bytes
followed by 0.
