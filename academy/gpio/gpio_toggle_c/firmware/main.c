/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2024-2026 Texas Instruments Incorporated - http://www.ti.com/
 */

#include <stdint.h>
#include <pru_cfg.h>

/* Delay between toggles, in PRU clock cycles (not time). At the 200 MHz PRU
 * clock (5 ns per cycle), 100,000,000 cycles is 0.5 s, so the GPO pin stays
 * high for 0.5 s and low for 0.5 s (1 Hz square wave, ~50% duty cycle).  */
#define TOGGLE_DELAY_CYCLES 100000000

volatile register uint32_t __R30;
volatile register uint32_t __R31;

/*
 *  main
 *
 *  Toggles a PRU GPO pin at a fixed interval.
 *
 *  Parameters: none
 *
 *  return: none (never returns)
 *
 *  Pseudocode:
 *      - Select GPO pin 4 as the output to toggle (output mask = bit 4)
 *      - Repeat forever:
 *          * Invert the selected output bit, flipping the pin level
 *          * Wait for the toggle delay before the next flip
 */
int main(void)
{
	volatile uint32_t gpio;

	/* The reset default for the PRU GPIO config registers (GPCFG0 for PRU0,
	 * GPCFG1 for PRU1) is GPI Mode 0, GPO Mode 0, which is direct mode.
	 * Writing R30 drives the PRU GPO signals immediately, so no PRU CFG
	 * register write is needed here. */

	/* Note: 0xFFFF_FFFF toggles all GPO pins */
	/* Toggle GPO4 pin */
	gpio = 0x10;

	while (1) {
		__R30 ^= gpio; // toggle GPO pin 4
		__delay_cycles(TOGGLE_DELAY_CYCLES);
	}
}
