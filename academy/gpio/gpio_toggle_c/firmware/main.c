/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2024-2026 Texas Instruments Incorporated - http://www.ti.com/
 */

#include <stdint.h>
#include <pru_cfg.h>

volatile register uint32_t __R30;
volatile register uint32_t __R31;

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
		__delay_cycles(100000000);
	}
}