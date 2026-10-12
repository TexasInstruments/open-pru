/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
 */

#include <stdint.h>
#include <stdio.h>
#include <pru_intc.h>
#include <pru_cfg.h>
#include <pru_uart.h>
#include <rsc_types.h>
#include <pru_rpmsg.h>
#include "resource_table.h"
#include "intc_map.h"

volatile register uint32_t __R31;

/*
 * UART HARDWARE INTERRUPT HOOKS
 *
 * Purpose: let a Linux core hand large buffers to the PRU, and let the PRU feed
 * data into and receive data from hardware UART instances (instead of a DMA).
 * This file only provides the interrupt hookups and empty handlers. The UART
 * data-movement logic is NOT implemented. FIXME: plug in the logic at each
 * handler below. Which core does what (Linux vs PRU0 vs PRU1) is undecided.
 *
 * Which UART events a core receives is defined in that core's intc_map.h
 * (UART1_EVENT, UART2_EVENT, PRU_UART_*_EVENT). A system event can only be
 * mapped to one channel, so each event is owned by one PRU core.
 * Currently ALL UART events are defined in PRU0's intc_map.h, so all UART code
 * runs on PRU0. This single main.c is built for both cores: the PRU1
 * infrastructure is in place, and PRU1 compiles no UART code until events are
 * moved to PRU1's intc_map.h.
 *
 * AM62x (PRUSS0), event = 32 + ICSSM0_PR1_SLV_INTR_IN_n (from the AM62x TRM).
 *   External events 32-63 reach the PRU INTC only when
 *   MII_RT_REG.MII_RT_EVENT_EN = 0 (PRU_CFG offset 0x2C, bit 0; reset value 1).
 *   The PRU firmware clears it (see CLEAR_MII_RT_EVENT_EN in intc_map.h).
 *     UART1_USART_IRQ_OUT_0 -> IN_0  -> event 32  (hooked up)
 *     UART0_USART_IRQ_OUT_0 -> IN_19 -> event 51  (connectivity exists, NOT used:
 *                                                  UART0 is typically reserved)
 *     UART2_USART_IRQ_OUT_0 -> IN_20 -> event 52  (hooked up)
 *
 * AM64x (PRU_ICSSG0 only), event = 64 + PRU_ICSSG0_PR1_SLV_INTR_IN_n
 *   (source/include/am64x/cslr_intr_pru_icssg0.h).
 *     IN_63 UART0 (NOT used, reserved)   IN_64 UART1 (hooked up)
 *     IN_65 UART2 (hooked up)            IN_66 UART3, IN_67 UART4,
 *     IN_68 UART5, IN_69 UART6, IN_70 MCU_UART0, IN_71 MCU_UART1 (not used)
 *   AM64x also has MII_RT_REG, but it does not configure the INTC, so the
 *   firmware does not touch it on AM64x.
 *
 * PRU subsystem local hardware UART (PRU_ICSS(G) UART0, same on AM62x and AM64x):
 *   event 6 uart_uint_intr_req, event 5 uart_utxevt_intr_req,
 *   event 4 uart_urxevt_intr_req. Registers: CT_UART in pru_uart.h.
 *   This local UART is also usable by Linux.
 *
 * Pin routing / pinmux / device tree for the UARTs is a manual step and is not
 * configured by this project.
 *
 * Reference for driving the PRU local UART (do NOT port; pointer only):
 * https://git.ti.com/cgit/pru-software-support-package/pru-software-support-package/tree/examples/am335x/PRU_Hardware_UART/PRU_Hardware_UART.c
 */

/*
 * EVENT_PENDING(evt): nonzero if system event "evt" is pending in the INTC.
 * Each 32-bit ENA_STATUS_REGn holds 32 events: REG0 = events 0-31, REG1 = 32-63,
 * REG2 = 64-95, REG3 = 96-127, REG4 = 128-159 (REG2, 3, 4 only apply to ICSSG).
 *  - (evt) >> 5 is the register number n.
 *  - (&CT_INTC.ENA_STATUS_REG0)[n] reads REG0 for n = 0, REG1 for n = 1, etc.
 *    This relies on the registers being contiguous, 4 bytes apart
 *    (0x280, 0x284, ...), as in source/include/{am62x,am64x}/pru_intc.h.
 *  - (evt) & 31 is the bit within that register (event 52: REG1, bit 20).
 */
#define EVENT_PENDING(evt)	((&CT_INTC.ENA_STATUS_REG0)[(evt) >> 5] & ((uint32_t) 1 << ((evt) & 31)))
#define EVENT_CLEAR(evt)	(CT_INTC.STATUS_CLR_INDEX_REG_bit.STATUS_CLR_INDEX = (evt))

#ifdef UART1_EVENT
static void uart1_event_handler(void)
{
	/* FIXME: UART1 interrupt: service UART1 here (read IIR, move data) */
}
#endif

#ifdef UART2_EVENT
static void uart2_event_handler(void)
{
	/* FIXME: UART2 interrupt: service UART2 here (read IIR, move data) */
}
#endif

#ifdef PRU_UART_URXEVT_EVENT
static void pru_uart_rx_handler(void)
{
	/* FIXME: PRU local UART receive event: read CT_UART.RBR */
}
#endif

#ifdef PRU_UART_UTXEVT_EVENT
static void pru_uart_tx_handler(void)
{
	/* FIXME: PRU local UART transmit event: write CT_UART.THR */
}
#endif

#ifdef PRU_UART_UINT_EVENT
static void pru_uart_int_handler(void)
{
	/* FIXME: PRU local UART interrupt: check CT_UART.IIR for the cause */
}
#endif

/* Check each UART system event this core owns, clear it, and call its handler */
static void uart_events_poll(void)
{
#ifdef UART1_EVENT
	if (EVENT_PENDING(UART1_EVENT)) {
		EVENT_CLEAR(UART1_EVENT);
		uart1_event_handler();
	}
#endif
#ifdef UART2_EVENT
	if (EVENT_PENDING(UART2_EVENT)) {
		EVENT_CLEAR(UART2_EVENT);
		uart2_event_handler();
	}
#endif
#ifdef PRU_UART_URXEVT_EVENT
	if (EVENT_PENDING(PRU_UART_URXEVT_EVENT)) {
		EVENT_CLEAR(PRU_UART_URXEVT_EVENT);
		pru_uart_rx_handler();
	}
#endif
#ifdef PRU_UART_UTXEVT_EVENT
	if (EVENT_PENDING(PRU_UART_UTXEVT_EVENT)) {
		EVENT_CLEAR(PRU_UART_UTXEVT_EVENT);
		pru_uart_tx_handler();
	}
#endif
#ifdef PRU_UART_UINT_EVENT
	if (EVENT_PENDING(PRU_UART_UINT_EVENT)) {
		EVENT_CLEAR(PRU_UART_UINT_EVENT);
		pru_uart_int_handler();
	}
#endif
}

/*
 * linker.cmd has been updated to include sections for
 * .resource_table and .pru_irq_map
 *
 * Resource table
 *  - A resource table is required to initialize RPMsg with Linux
 *  - This example uses template resource table with 16 TX & 16 RX RPMsg buffers
 *    at source/include/linux/resource_table.h
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

#ifdef CLEAR_MII_RT_EVENT_EN
	/* AM62x: route external events 32-63 (device UART interrupts) to the PRU INTC */
	CT_CFG.mii_rt_reg_bit.mii_rt_event_en = 0;
#endif

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
			uart_events_poll();
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
