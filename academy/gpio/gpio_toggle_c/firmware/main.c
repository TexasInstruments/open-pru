/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
 */

#include <stdint.h>
#include <pru_cfg.h>

volatile register uint32_t __R30;
volatile register uint32_t __R31;

int main(void)
{
	volatile uint32_t gpio;

	/* GPI Mode 0, GPO Mode 0 */
	CT_CFG.gpcfg0_reg = 0;

	/* Note: 0xFFFF_FFFF toggles all GPO pins */
	/* Toggle GPO4 pin */
	gpio = 0x10;

	while (1) {
		__R30 ^= gpio; // toggle GPO pin 4
		__delay_cycles(100000000);
	}
}