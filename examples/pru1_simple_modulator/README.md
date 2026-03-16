# PRU1 Simple Modulator

Generates two output signals from PRU1 on AM243x ICSS_G0:

- **PRU0_GPO0**: ~20 MHz clock signal
- **PRU0_GPO1**: Data signal that updates on the falling edge of the clock, shifting out 32-bit data MSB-first from ICSS DRAM0

## Overview

This example implements a simple serial modulator on the PRU1 core of AM243x ICSS_G0. The firmware produces a continuous clock on PRU0_GPO0 and a synchronous data output on PRU0_GPO1. On each falling edge of the clock, the next data bit is driven onto PRU0_GPO1.

The 32-bit data word is loaded from ICSS DRAM0 at offset 0 using the instruction:

```
lbco &r2, c24, 0, 4
```

Where `c24` is the PRU constant table entry pointing to DRAM0 base address. After all 32 bits are shifted out (MSB first), the data is re-loaded from DRAM0, allowing the host (R5F) to update the transmit data between frames.

### Timing

| Parameter          | Value                                    |
| ------------------ | ---------------------------------------- |
| PRU core clock     | 333 MHz (3 ns/cycle)                     |
| Clock frequency    | ~20.83 MHz (16 cycles = 48 ns period)    |
| Bits per frame     | 32                                       |
| Frame time         | ~1.536 us (32 x 48 ns)                   |
| Clock high time    | 24 ns (8 cycles)                         |
| Clock low time     | 24 ns (8 cycles)                         |

> **Note**: The target frequency is 20 MHz (50 ns period), but with the PRU running at 333 MHz the closest achievable frequency using integer cycle counts is ~20.83 MHz (48 ns). For exact 20 MHz generation, see the `custom_frequency_generator` example which uses a fractional adder approach.

### Signal Diagram

```
CLK  (GPO0): ‾‾‾‾\____/‾‾‾‾\____/‾‾‾‾\____/‾‾‾‾\____
DATA (GPO1): ==D31=====|=D30=====|=D29=====|=D28=====|...
                   ^         ^         ^         ^
               falling   falling   falling   falling
                edge      edge      edge      edge
```

Data updates on the falling edge of the clock. The receiver should sample DATA on the rising edge of CLK for reliable capture.

## How to Run

1. Import the device-specific project from `examples/pru1_simple_modulator` in the open-pru repository.
2. Write the desired 32-bit data word to ICSS DRAM0 offset 0 from the R5F host application before starting the PRU.
3. Build and load the `*.out` file into the core **ICSSG0 PRU1** of AM243x device.
4. Run the core. The clock signal appears on **PRU0_GPO0** and the data signal appears on **PRU0_GPO1**.
5. To view the signals, connect two channels of a logic analyzer or oscilloscope to the respective pins.

### Host-Side Data Update

From the R5F application, write a 32-bit value to DRAM0 offset 0:

```c
volatile uint32_t *pru_dram0 =
    (volatile uint32_t *)PRUICSS_getDataMemAddr(gPruIcssHandle, PRUICSS_PRU0);
pru_dram0[0] = 0xDEADBEEF;  /* 32-bit data to shift out */
```

The PRU will pick up the new value on the next frame reload (after finishing the current 32-bit shift).

## Configuration

| Define      | Default | Description                              |
| ----------- | ------- | ---------------------------------------- |
| `CLK_PIN`   | 0       | GPO pin number for clock (PRU0_GPO0)     |
| `DATA_PIN`  | 1       | GPO pin number for data (PRU0_GPO1)      |
| `NUM_BITS`  | 32      | Number of bits shifted per data frame    |

## Results

Test Environment: AM243x LaunchPad with MCU+SDK 11.0

The clock and data signals can be captured using the PRU Logic Scope (see `examples/logic_scope`) or an external logic analyzer connected to PRU0_GPO0 and PRU0_GPO1.
