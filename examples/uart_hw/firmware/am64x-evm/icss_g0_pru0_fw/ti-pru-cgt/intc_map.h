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

/*
 * UART system events (AM64x PRU_ICSSG0)
 *
 * NOTE: currently ALL INTC configuration, and all UART code, runs on PRU0. The
 * PRU1 infrastructure is in place (see PRU1's intc_map.h) for when events are
 * moved to PRU1. A system event maps to exactly one channel, so an event is
 * owned by one PRU core: to move it, move its #defines and its .pru_irq_map
 * entry to the other core's intc_map.h.
 *
 * On PRU_ICSSG the external events start at INTC input 64, so
 * event number = 64 + PRU_ICSSG0_PR1_SLV_INTR_IN_n (not 32 as on AM62x PRUSS).
 * IN_n values: source/include/am64x/cslr_intr_pru_icssg0.h.
 *   UART0_USART_IRQ_OUT_0 -> IN_63 -> event 127 (NOT hooked up; UART0 is
 *                                               typically reserved for other
 *                                               use cases)
 *   UART1_USART_IRQ_OUT_0 -> IN_64 -> event 128 (hooked up)
 *   UART2_USART_IRQ_OUT_0 -> IN_65 -> event 129 (hooked up)
 *   UART3 -> IN_66 (event 130), UART4 -> IN_67 (131), UART5 -> IN_68 (132),
 *   UART6 -> IN_69 (133), MCU_UART0 -> IN_70 (134), MCU_UART1 -> IN_71 (135)
 *                                              (NOT hooked up)
 * PRU subsystem local UART (events 4-6, Table 13 of
 * docs_ai/reference/pru_subsystem_migration_guide; also usable by Linux):
 *   event 6 pr0_uart0_uint_intr_req, 5 utxevt, 4 urxevt (hooked up)
 *
 * MII_RT_REG.MII_RT_EVENT_EN exists on AM64x but does not configure the INTC,
 * so CLEAR_MII_RT_EVENT_EN is intentionally NOT defined here.
 */
#define UART1_EVENT		128
#define UART2_EVENT		129
#define PRU_UART_UINT_EVENT	6
#define PRU_UART_UTXEVT_EVENT	5
#define PRU_UART_URXEVT_EVENT	4

#include <stddef.h>
#include <rsc_types.h>

/*
 * .pru_irq_map is used by the RemoteProc driver during initialization. However,
 * the map is NOT used by the PRU firmware. That means DATA_SECTION and RETAIN
 * are required to prevent the PRU compiler from optimizing out .pru_irq_map.
 *
 * UART events share channel 0 / host interrupt 0 (R31 bit 30) with the RPMsg
 * "from ARM" event; main.c tells them apart with the INTC enabled-status regs.
 */
#pragma DATA_SECTION(my_irq_rsc, ".pru_irq_map")
#pragma RETAIN(my_irq_rsc)

struct pru_irq_rsc my_irq_rsc  = {
	0,			/* type = 0 */
	6,			/* number of system events being mapped */
	{
		{17, 0, 0},	/* {sysevt, channel, host interrupt} */
		{UART1_EVENT, 0, 0},
		{UART2_EVENT, 0, 0},
		{PRU_UART_UINT_EVENT, 0, 0},
		{PRU_UART_UTXEVT_EVENT, 0, 0},
		{PRU_UART_URXEVT_EVENT, 0, 0},
	},
};

#endif /* _INTC_MAP_H_ */
