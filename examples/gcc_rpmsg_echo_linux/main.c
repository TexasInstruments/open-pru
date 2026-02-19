/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2022-2025 Texas Instruments Incorporated - http://www.ti.com/
 */

#include <stdint.h>
#include <stdio.h>
#include <pru_intc.h>
#include <rsc_types.h>
#include <pru_rpmsg.h>
#include "config.h"
#include "resource_table.h"
#include "intc_map.h"

#include <pru/io.h>

/*
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
 *
 * For more information, refer to the processor's Technical Reference Manual (TRM),
 * section "Processors > PRU > PRU Local INTC"
 */
#define HOST_INT			((uint32_t) 1 << HOST_INT_BIT)

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

/*
 * Used to make sure the Linux drivers are ready for RPMsg communication
 * Found at linux-x.y.z/include/uapi/linux/virtio_config.h
 */
#define VIRTIO_CONFIG_S_DRIVER_OK	4

uint8_t payload[RPMSG_MESSAGE_SIZE];

/*
 * main.c
 */
int main(void)
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
					/* On PRU_RPMSG_SUCCESS, the pointers will be valid.
					 * On len=0, the assembly will never loop even once, so it is
					 * safe to temporarily have invalid "p_end" pointer.
					 */
					uint8_t *p_begin = &payload[0];
					uint8_t *p_end = &payload[len-1];

					/* Show off GCC: Reverse the payload buffer using inline assembly. */
					asm volatile (
					    "jmp	2f\n\t"			/* Jump forward to local label 2. */
					    "1:\n\t"
					    "lbbo	r0.b0, %[R_begin], 0, 1\n\t" /* Load characters at beginning
											and end of buffer. */
					    "lbbo	r0.b1, %[R_end], 0, 1\n\t"
					    "sbbo	r0.b1, %[R_begin], 0, 1\n\t" /* Store, but swapped. */
					    "sbbo	r0.b0, %[R_end], 0, 1\n\t"
					    "add	%[R_begin], %[R_begin], 1\n\t" /* Adjust pointers. */
					    "sub	%[R_end], %[R_end], 1\n\t"
					    "2:\n\t"
					    "qblt	1b, %[R_end], %[R_begin]\n\t" /* Jump to local label 1 backward,
										     if R_begin < R_end. */
					    : [R_begin] "+r" (p_begin),		/* Register, both read and written to.
										   See https://gcc.gnu.org/onlinedocs/gcc/Modifiers.html */
					      [R_end] "+r" (p_end)
					    : 					/* No input-only operands. */
					    : "memory",				/* Memory will be clobbered. */
					      "r0.b0", "r0.b1");		/* clobbered registers (always 8-bit!) */
					/* Echo the message back to the same address from which we just received */
					pru_rpmsg_send(&transport, dst, src, payload, len);
				}
			}
		}
	}
}
