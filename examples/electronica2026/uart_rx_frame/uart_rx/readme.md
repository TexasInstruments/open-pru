# UART RX Frame (electronica 2026)

## Introduction

ICSSG0 PRU0 bit-bangs a 4 Mbaud 8N1 UART receiver on one direct GPI pin and
stores 11-byte frames in a PRU0 DRAM0 ring buffer. The firmware was developed
and verified in TI's pru-simulator (`source/uart_rx_11frame.asm`, with the
IO panel's UART RX Inject as the stimulus) and then ported to the AM243x
LaunchPad. The R5F (MCU+ SDK, FreeRTOS) configures pin mux, the input pull-up
and the PRU clock, and then loads and starts PRU0 only.

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
| AM243x    | LP-AM243, Rev A          | MCU+ SDK 12.01.00.33, CCS 21.0.1, ti-cgt-pru 2.3.3, SysConfig 1.28.1. Hardware run 2026-11-03 with the PicoScope AWG sender (see Validate) |

The project builds with MCU+ SDK 12.01.00.33. The open-pru release notes list
MCU+ SDK 11.1 - 12.0 as validated for AM243x.

## Overview

### What the firmware does

1. Poll RX (R31 bit 2) for the falling edge of a start bit.
2. Wait 1.5 bit times, then sample 8 data bits, LSB first, one bit time
   (50 PRU cycles) apart.
3. Check the stop bit. If it is low, set the framing error flag, drop the
   partial frame and go back to step 1.
4. Resynchronise on the start bit of the next byte. After 11 bytes, store the
   frame with one `SBCO` at the ring buffer offset, advance the offset by 11
   (wrap after 372 frames) and write the frame count.

### DRAM0 layout

PRU0 local address 0x0000 = R5F / CCS global address 0x30000000.

| Offset        | Content                                                     |
| ------------- | ----------------------------------------------------------- |
| 0x0000-0x0FFB | Ring buffer, 372 frames x 11 bytes. Frame n (0-based) is at `(n mod 372) * 11` |
| 0x0FFC-0x0FFD | Frame count, 16-bit little-endian, wraps at 65536          |
| 0x0FFE        | Framing error flag, sticky, 1 = at least one bad stop bit  |

The newest frame is at `((count - 1) mod 372) * 11`. The R5F clears PRU0
DRAM before it starts the core.

### PRU resources

| Resource   | Setting                                                    |
| ---------- | ---------------------------------------------------------- |
| Core       | ICSSG0 PRU0 only (PRU1, RTU_PRU and TX_PRU not loaded)     |
| Core clock | 200 MHz (`example.syscfg`, `coreClk`). Bit timing is cycle-counted for 200 MHz |
| GPI mode   | Direct GPIO, GPCFG0 GP_MUX_SEL = 0 (set by R5F)            |
| RX         | R31 bit 2 = PRG0_PRU0_GPI2, pad PRG0_PRU0_GPO2 (ball G1), mux mode 1, pull-up |
| R30        | Not used                                                   |

### Timing (200 MHz, 4 Mbaud, 50 cycles per bit)

| Item                                | Measured                    |
| ----------------------------------- | --------------------------- |
| Start edge to first data sample     | 75-77 cycles (ideal 75)     |
| Data sample position in the bit cell (0-49) | 25-33; 25-41 for back-to-back frames |
| Stop sample position in the stop cell (0-49) | 31-43                 |
| Baud window, 50 kbaud steps         | 3.85-4.00 Mbaud             |

Measured in pru-simulator on the built `.out`. See the porting report for how.

### Limitations

- **Sender must not be fast.** The stop bit of byte 11 is sampled 43 cycles
  into its 50-cycle cell, so a sender only 1.25 % fast (4.05 Mbaud) already
  fails. The same limit is in the simulator source.
- **No frame sync.** Frames are cut every 11 bytes from the first start bit
  the firmware sees. Start the PRU before the sender. An idle gap does not
  realign frames; only a framing error resets the byte index.
- **No host read-out.** The R5F only loads PRU0. Read the frames with the
  CCS memory browser.

## Steps to Run the Example

### Hardware setup

| Signal | PRU signal                   | LP header      | Connect to          |
| ------ | ---------------------------- | -------------- | ------------------- |
| RX     | PRG0_PRU0_GPI2 (pad GPO2)    | BP.31 (J4)     | UART TX of the sender |
| GND    | -                            | BP.20 (J2)     | Sender GND          |

- The sender must use 3.3 V logic, 4 Mbaud (exact or slightly slow), 8N1,
  idle high. Check in the sender's datasheet that its baud generator hits
  4 Mbaud exactly or slightly below (see Limitations).
- The LaunchPad headers are labelled with BoosterPack pin numbers. BP.31 for
  PRG0_PRU0_GPO2 comes from the AM243x LaunchPad pin mapping in
  `examples/LCD_interface/documents/PRU IO - LCD Interface.pptx` (slide 6:
  PRU0 GPO0/1/2 = BP.33/32/31).

### Build and run

1. Build the PRU project `uart_rx_am243x-lp_icss_g0_pru0_fw_ti-pru-cgt` first.
   It generates `firmware/am243x-lp/pru0_load_bin.h`.
2. Build the R5F project `uart_rx_am243x-lp_r5fss0-0_freertos_ti-arm-clang`.
3. Load and run the R5F project. Only PRU0 of ICSSG0 is loaded and started.
   The UART console prints
   `UART RX frame: ICSSG0 PRU0 running, frames at PRU0 DRAM0 (0x30000000)`.

### Validate

1. Send one frame, for example `HelloWorld!` (11 bytes, no line ending).
2. In the CCS memory browser, open 0x30000000. The first 11 bytes are
   `48 65 6C 6C 6F 57 6F 72 6C 64 21`. The 16-bit word at 0x30000FFC is 1,
   and the byte at 0x30000FFE is 0.
3. Send more frames. Each one appears 11 bytes after the previous one, and
   the count at 0x30000FFC goes up by one per frame.

### Sender: PicoScope arbitrary waveform generator

`test/helloworld_4mbaud_awg.csv` is a PicoScope 7 AWG file that sends
`HelloWorld!` at 4 Mbaud once per AWG period. `test/make_awg_csv.py`
regenerates it, optionally with another 11-byte payload. Each period is
200 bit times: 45 idle bits, the 110-bit frame, then 45 idle bits.

1. In the PicoScope signal generator, select Arbitrary and import the CSV.
2. Set Frequency to 20 kHz (one 50 us period = 200 bits of 250 ns),
   amplitude 1 V and offset +1 V, which gives 0 V / 2 V levels.
   - Check the unit: at 20 MHz the AWG cannot play the 1600-sample buffer
     and the output looks like a sine.
   - Set the amplitude before the offset. The allowed offset range shrinks
     as the amplitude grows (at 2 V amplitude it is only about +/-0.5 mV).
   - Import the CSV from a copy outside the repository, so that saving in
     PicoScope cannot overwrite the committed file.
3. Start order, because the firmware has no frame sync: set the generator to
   DC +2 V (line idle high), run the R5F project, then switch to Arbitrary.
   If the PRU starts in the middle of a frame, every frame comes out shifted
   by one or more bytes and 0x30000FFE is set. Restart from the DC step.
4. Every 11-byte slot from 0x30000000 then holds `HelloWorld!`, the count at
   0x30000FFC increases by 20,000 per second, and 0x30000FFE stays 0.

2 V is the generator's maximum output and is at the bottom of the 3.3 V
input-high range. If bytes are wrong, add a level shifter (for example an
SN74LVC1T45 with VCCA = 1.8 V and VCCB = 3.3 V).

The CSV and the start-order effect were checked in pru-simulator by playing
the CSV into the built `.out` at 20 kHz.
