/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2022-2025 Texas Instruments Incorporated - http://www.ti.com/
 */

#include <stdint.h>
#include <stdio.h>
#include <pru_intc.h>
#include <rsc_types.h>
#include <pru_rpmsg.h>
#include "resource_table.h"
#include "intc_map.h"

#if !defined(__GNUC__)
  volatile register uint32_t __R31;
#else
  #include <pru/io.h>
#endif

/*
 * linker.cmd has been updated to include sections for
 * .resource_table and .pru_irq_map
 *
 * Resource table
 *  - A resource table is required to initialize RPMsg with Linux
 *  - This example uses template resource table with 16 TX & 16 RX RPMsg buffers
 *    at source/include/c_code/linux/resource_table.h
 *
 * INTC map
 *  - intc_map.h defines INTC mapping for interrupts going to the PRU core
 */

/*
 * INTC CONFIGURATION
 *
 * Interrupts going to the PRU Subsystem are configured in intc_map.h
 * Interrupts going to Linux are configured in the Linux devicetree file "interrupts" entry:
 *   - AM62x: k3-am62-main.dtsi
 *   - AM64x: k3-am64-main.dtsi
 *
 * For more information, refer to the processor's Technical Reference Manual (TRM),
 * section "Processors > PRU > PRU Local INTC"
 */
#ifndef HOST_INT_BIT
#error "HOST_INT_BIT not defined, must be passed to the compiler using --define"
#endif
#define HOST_INT			((uint32_t) 1 << HOST_INT_BIT)

/* System events TO_ARM_HOST and FROM_ARM_HOST should align with intc_map.h & devicetree file */
#ifndef TO_ARM_HOST
#error "TO_ARM_HOST not defined, must be passed to the compiler using --define"
#endif
#ifndef FROM_ARM_HOST
#error "FROM_ARM_HOST not defined, must be passed to the compiler using --define"
#endif
/*
 * FROM_ARM_HOST < 32 for all cores, so (ENA_STATUS_REG0 & FROM_ARM_HOST_BIT)
 * can be used to check the status of the system event.
 */
#define FROM_ARM_HOST_BIT		((uint32_t) 1 << FROM_ARM_HOST)

/* 
 * RPMSG CONFIGURATION
 *
 * Using the name 'rpmsg-raw' will probe the Linux rpmsg_char driver
 * at linux-x.y.z/drivers/rpmsg/rpmsg_char.c
 *
 * Each PRU subsystem core should have a unique channel port (endpoint)
 */
#define CHAN_NAME                       "rpmsg-raw"

#ifndef CHAN_PORT
#error "CHAN_PORT not defined, must be passed to the compiler using --define"
#endif

/*
 * Used to make sure the Linux drivers are ready for RPMsg communication
 * Found at linux-x.y.z/include/uapi/linux/virtio_config.h
 */
#define VIRTIO_CONFIG_S_DRIVER_OK	4

uint8_t payload[RPMSG_MESSAGE_SIZE];

/*
 * main.c
 */
void main(void)
{
	struct pru_rpmsg_transport transport;
	uint16_t src, dst, len;
	volatile uint8_t *status;

	/* Clear the status of the PRU system event that the ARM will use to 'kick' us */
	CT_INTC.STATUS_CLR_INDEX_REG_bit.STATUS_CLR_INDEX = FROM_ARM_HOST;

	/* Make sure the Linux drivers are ready for RPMsg communication */
	status = &resourceTable.rpmsg_vdev.status;
	while (!(*status & VIRTIO_CONFIG_S_DRIVER_OK));

	/* Initialize the RPMsg transport structure */
	pru_rpmsg_init(&transport, &resourceTable.rpmsg_vring0, &resourceTable.rpmsg_vring1, TO_ARM_HOST, FROM_ARM_HOST);

	/* Create the RPMsg channel between the PRU and ARM user space using the transport structure. */
	while (pru_rpmsg_channel(RPMSG_NS_CREATE, &transport, CHAN_NAME, CHAN_PORT) != PRU_RPMSG_SUCCESS);

	while (1) {
		/* Check register R31 to see if an interrupt has been received */
		if (__R31 & HOST_INT) {
			/* check the status of system event FROM_ARM_HOST to see if the ARM has kicked us */
			if (CT_INTC.ENA_STATUS_REG0 & FROM_ARM_HOST_BIT) {
				/* Clear the event status */
				CT_INTC.STATUS_CLR_INDEX_REG_bit.STATUS_CLR_INDEX = FROM_ARM_HOST;
				/* Receive all available messages. Multiple messages can be sent per kick */
				while (pru_rpmsg_receive(&transport, &src, &dst, payload, &len) == PRU_RPMSG_SUCCESS) {
					/* Echo the message back to the same address from which we just received */
					pru_rpmsg_send(&transport, dst, src, payload, len);
				}
			}
		}
	}
}
