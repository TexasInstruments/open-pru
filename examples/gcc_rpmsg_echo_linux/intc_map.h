#ifndef _INTC_MAP_H_
#define _INTC_MAP_H_

/*
 * ======== PRU INTC Map ========
 *
 * Define the INTC mapping for interrupts going to the ICSS / ICSSG:
 * 	ICSS Host interrupts 0, 1
 * 	ICSSG Host interrupts 0, 1, 10-19
 *
 * Note that INTC interrupts going to the ARM Linux host should not be defined
 * in this file (ICSS/ICSSG Host interrupts 2-9).
 *
 * The INTC configuration for interrupts going to the ARM host should be defined
 * in the device tree node of the client driver, "interrupts" property.
 * See Documentation/devicetree/bindings/interrupt-controller/ti,pruss-intc.yaml
 * entry #interrupt-cells for more.
 *
 * For example, on ICSSG:
 *
 * &client_driver0 {
 * 	interrupt-parent = <&icssg0_intc>;
 * 	interrupts = <21 2 2>, <22 3 3>;
 * 	interrupt-names = "interrupt_name1", "interrupt_name2";
 * };
 */

#include <stddef.h>
#include <rsc_types.h>
#include "config.h"

/*
 * .pru_irq_map is used by the RemoteProc driver during initialization. However,
 * the map is NOT used by the PRU firmware. The GNU linker's default script for PRU
 * knows about the ".pru_irq_map", and will retain it.
 */

struct pru_irq_rsc my_irq_rsc
__attribute__((section(".pru_irq_map"),unavailable("pru_irq_map is for usage by the host only"))) = {
	0,			/* type = 0 */
	1,			/* number of system events being mapped */
	{
		{FROM_ARM_HOST, INTMAP_CHANNEL, INTMAP_HOST},	/* {sysevt, channel, host interrupt} */
	},
};

#endif /* _INTC_MAP_H_ */
