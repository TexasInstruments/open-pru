/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2024-2026 Texas Instruments Incorporated - http://www.ti.com/
 *
 * File:  main.c
 *
 * Brief: Toggle PRU GPO output signal
 *
 * What this example teaches:
 *   - How a PRU core drives a pin: write a value to register R30. Each bit
 *     of R30 is wired to one PRU GPO (General Purpose Output) pin of that
 *     core: R30 bit 4 -> GPO4, R30 bit 0 -> GPO0, and so on.
 *   - How to reach the special registers R30 (outputs) and R31 (inputs)
 *     from C.
 *   - How to create a delay with the __delay_cycles() compiler intrinsic,
 *     which waits for a fixed number of PRU clock cycles.
 *
 * Who runs this code:
 *   A host core (for example an MCU+ core, or Linux or Zephyr on an
 *   application core) copies this firmware into the PRU instruction
 *   memory and starts the PRU. The PRU does not start by itself. The same
 *   main.c is built for PRU0 and for PRU1. Each core toggles its own
 *   GPO4.
 *
 * The pin that GPO4 reaches on the board depends on the device. See the
 * readme for the pin location on each board, and for debug steps.
 */

#include <stdint.h>
#include <pru_cfg.h>

/* Delay between toggles, in PRU clock cycles (not time). See the PEAK
 * cycles comment above main() for the resulting timing. */
#define TOGGLE_DELAY_CYCLES 100000000

/*
 * R30 and R31 are PRU hardware registers, not memory. "register" tells the
 * compiler to use the real register, and "volatile" tells it that the value
 * can change outside the program (R31) or has an effect outside the program
 * (R30), so it must not optimize accesses away.
 *   __R30: write to drive the GPO pins
 *   __R31: read to see the GPI pins
 */
volatile register uint32_t __R30;
volatile register uint32_t __R31;

/*
 * main
 *
 * Toggles a PRU GPO pin at a fixed interval.
 *
 * Parameters: none
 *
 * return: none (never returns)
 *
 * Pseudocode:
 *     - Select GPO pin 4 as the output to toggle (output mask = bit 4)
 *     - Repeat forever:
 *         * Invert the selected output bit, flipping the pin level
 *         * Wait for the toggle delay before the next flip
 *
 * PEAK cycles per loop iteration: TOGGLE_DELAY_CYCLES + a few cycles
 *     __delay_cycles(N) is built as a counted loop that takes exactly N
 *     cycles. For N = 100,000,000 the listing (dispru) shows 2 ldi +
 *     49,999,999 passes x 2 cycles (sub, qbne) = 100,000,000.
 *     On top of that each iteration runs an lbbo (reads "gpio" back
 *     from the stack, because it is volatile), an xor and a jmp. The
 *     lbbo access time was not counted, so the overhead is "a few
 *     cycles", not an exact number, and negligible next to the delay.
 *
 *     At a 200 MHz PRU clock (5 ns per cycle), 100,000,000 cycles is
 *     0.5 s, so the GPO pin stays high for 0.5 s and low for 0.5 s
 *     (1 Hz square wave, ~50% duty cycle).
 *
 *     The delay is a build-time constant, so this is also the worst
 *     case. Update this note if TOGGLE_DELAY_CYCLES or the loop body
 *     changes.
 */
int main(void)
{
    /* Bit mask of the pins to toggle. Bit n set means GPOn is toggled. */
    volatile uint32_t gpio;

    /* The reset default for the PRU GPIO config registers (GPCFG0 for PRU0,
     * GPCFG1 for PRU1) is GPI Mode 0, GPO Mode 0, which is direct mode.
     * Writing R30 drives the PRU GPO signals immediately, so no PRU CFG
     * register write is needed here. */

    /* Toggle GPO4 pin. 0x10 is binary 0001 0000, so only bit 4 is set. */
    gpio = 0x10;

    /* Starting with every output low. Without this the
     * level of the first half period is unknown. */
    __R30 = 0;

    /* Three ways to change an output pin. This example uses the third.
     *   1. Overwrite:  __R30 = 0x10;
     *      Writes all 32 bits of R30: bit 4 is set and every other bit
     *      is cleared. Fine if only GPO4 is used. The assembly version
     *      (gpio_toggle) uses this method, on the lowest byte only.
     *   2. Set or clear one bit:  __R30 |= (1u << 4);   (pin high)
     *                             __R30 &= ~(1u << 4);  (pin low)
     *      Only bit 4 changes. Every other bit keeps its value.
     *   3. Toggle (XOR):  __R30 ^= (1u << 4);
     *      Flips bit 4, whatever its current level is, and leaves every
     *      other bit alone. Because one XOR flips the pin on every pass,
     *      the loop needs no separate set and clear steps. */

    /* INTENTIONALLY_UNBOUNDED_LOOP: runs forever, the PRU firmware has
     * no exit path. */
    while (1) {
        __R30 ^= gpio;
        __delay_cycles(TOGGLE_DELAY_CYCLES);
    }
}
