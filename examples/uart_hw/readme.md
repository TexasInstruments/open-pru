# High speed Hardware UART (Linux host)

## Introduction

This project provides the interrupt infrastructure that lets a Linux core hand
large data buffers to the PRU, and lets the PRU feed data into and receive data
from hardware UART instances. The goal is to replace the DMA that would
normally offload Linux.

**The UART data-movement logic is NOT implemented.** The project provides the
interrupt hookups (INTC mapping, event checks, empty handler stubs marked
`FIXME`) so that a developer can plug in their own logic. The division of work
between Linux and the PRU cores is undecided and is left to the developer.

The project is based on `rpmsg_echo_linux`. The RPMsg echo behavior of the
template is kept unchanged, so the PRU cores still boot, create an RPMsg
channel with Linux, and echo messages back. RPMsg functionality may be removed
if it is not needed.

## Supported Combinations

Refer to open-pru/examples/readme.md > Supported processors per-project
for the list of processors that support building this project, and information
about porting this project to other processors.

| Processor | Board  | PRU cores built       |
| --------- | ------ | --------------------- |
| am62x     | am62x-sk  | PRUSS0 PRU0, PRU1  |
| am64x     | am64x-evm | PRU_ICSSG0 PRU0, PRU1 |

## Validated HW & SW

This project has NOT been validated on hardware. It has only been built.

| Processor | Hardware | Software |
| --------- | -------- | -------- |
| am62x     | FIXME: board revision | FIXME: Linux SDK version, OpenPRU commit |
| am64x     | FIXME: board revision | FIXME: Linux SDK version, OpenPRU commit |

The Linux SDK notes in
[rpmsg_echo_linux](../rpmsg_echo_linux/readme.md) (older SDKs and SDK 11.0
requiring modifications to enable PRU RPMsg) also apply to this project.

## Overview

### How UART interrupts reach the PRU

External (device-level) UART interrupts and the PRU subsystem's local UART
interrupts are routed into the PRU INTC as system events. The PRU firmware
polls R31 for the host interrupt, then checks the INTC enabled-status
registers to see which system event fired, clears the event, and calls the
matching handler stub in `firmware/main.c`.

Which events a core receives is defined per core in
`firmware/<board>/<core>/ti-pru-cgt/intc_map.h`, which fills the
`.pru_irq_map` resource that the Linux remoteproc driver uses to configure the
INTC. A system event can map to only one channel, so each event is owned by
exactly one PRU core.

### All UART code runs on PRU0

For now, ALL UART events are defined in PRU0's `intc_map.h`, and all UART
handler code runs on PRU0. PRU1 keeps its RPMsg-only infrastructure. A single
`firmware/main.c` is shared by both cores and both devices; it compiles a
UART handler only if the core's `intc_map.h` defines that event's macro
(`UART1_EVENT`, `UART2_EVENT`, `PRU_UART_URXEVT_EVENT`,
`PRU_UART_UTXEVT_EVENT`, `PRU_UART_UINT_EVENT`).

To move an event to PRU1: move the event's `#define` and its `.pru_irq_map`
entry from PRU0's `intc_map.h` to PRU1's `intc_map.h` (use channel 1 / host
interrupt 1, which is R31 bit 31). A note in PRU1's `intc_map.h` repeats this.

### AM62x (PRUSS0, board am62x-sk)

External events 32-63 reach the PRU INTC only when
`MII_RT_REG.MII_RT_EVENT_EN = 0`. `MII_RT_REG` is at PRU_CFG offset 0x2C
(0x3006_602C), bit 0 is `MII_RT_EVENT_EN`, reset value 1
(`source/include/am62x/cslr_icss_m.h`). PRU0's `intc_map.h` defines
`CLEAR_MII_RT_EVENT_EN`, and `main.c` then clears the bit at start-up.

Event number = 32 + ICSSM0_PR1_SLV_INTR_IN_n (IN_n values are from the AM62x
TRM):

| UART signal             | INTR_IN | Event | Status        |
| ----------------------- | ------- | ----- | ------------- |
| UART0_USART_IRQ_OUT_0   | IN_19   | 51    | not used      |
| UART1_USART_IRQ_OUT_0   | IN_0    | 32    | hooked up     |
| UART2_USART_IRQ_OUT_0   | IN_20   | 52    | hooked up     |

### AM64x (PRU_ICSSG0, board am64x-evm)

PRU_ICSSG INTC external events start at INTC input 64, so
event number = 64 + PRU_ICSSG0_PR1_SLV_INTR_IN_n
(`source/include/am64x/cslr_intr_pru_icssg0.h`). `MII_RT_REG` exists on AM64x
but does not configure the INTC, so the firmware does not touch it.

| UART signal             | INTR_IN | Event | Status        |
| ----------------------- | ------- | ----- | ------------- |
| UART0_USART_IRQ_OUT_0   | IN_63   | 127   | not used      |
| UART1                   | IN_64   | 128   | hooked up     |
| UART2                   | IN_65   | 129   | hooked up     |
| UART3                   | IN_66   | 130   | not used      |
| UART4                   | IN_67   | 131   | not used      |
| UART5                   | IN_68   | 132   | not used      |
| UART6                   | IN_69   | 133   | not used      |
| MCU_UART0               | IN_70   | 134   | not used      |
| MCU_UART1               | IN_71   | 135   | not used      |

### UART0

UART0 interrupt connectivity to the PRU INTC exists on both devices, but UART0
is typically reserved for other use cases (for example the Linux console), so
this project does not hook it up.

### PRU subsystem local hardware UART

The PRU subsystem has its own hardware UART (UART0 of the PRU_ICSS /
PRU_ICSSG / PRUSS), which Linux can also use. Its events are the same on both
devices, and all three are hooked up:

| Event | Signal                      | Macro                   |
| ----- | --------------------------- | ----------------------- |
| 6     | pr0_uart0_uint_intr_req     | `PRU_UART_UINT_EVENT`   |
| 5     | pr0_uart0_utxevt_intr_req   | `PRU_UART_UTXEVT_EVENT` |
| 4     | pr0_uart0_urxevt_intr_req   | `PRU_UART_URXEVT_EVENT` |

Registers are accessed through `CT_UART` from
`source/include/<processor>/pru_uart.h`. For an example of driving the PRU
local UART, see the PRU Software Support Package
[PRU_Hardware_UART.c](https://git.ti.com/cgit/pru-software-support-package/pru-software-support-package/tree/examples/am335x/PRU_Hardware_UART/PRU_Hardware_UART.c).
That example is a pointer only; it is not ported or implemented here.

### Per-core IPC settings

The RPMsg part of the project is unchanged from `rpmsg_echo_linux`. Each PRU
core uses system events and a unique RPMsg endpoint (or "channel port"). These
values are defined in the core-specific makefiles:

| AM62x | HOST_INT_BIT | TO_ARM_HOST | FROM_ARM_HOST | CHAN_PORT |
| ----- | ------------ | ----------- | ------------- | --------- |
| PRU0  | 30           | 16          | 17            | 30        |
| PRU1  | 31           | 18          | 19            | 31        |

| AM64x       | HOST_INT_BIT | TO_ARM_HOST | FROM_ARM_HOST | CHAN_PORT |
| ----------- | ------------ | ----------- | ------------- | --------- |
| ICSSG0 PRU0 | 30           | 16          | 17            | 30        |
| ICSSG0 PRU1 | 31           | 18          | 19            | 31        |

## Pin routing is a manual step

This project does NOT configure pin routing, pinmux, or Linux device tree
nodes for any UART. Routing the UART pins, enabling the UART instances in
Linux (or reserving them for the PRU), and the device tree for the PRU
subsystem are manual steps for the user.

## Steps to Run the Example

1. Complete the pin routing / device tree steps above for the UART instances
   you want to use.
2. Boot the PRU cores from Linux remoteproc, following "Step 3: Run the PRU
   RPMsg Echo example" of
   [[FAQ] AM62x & AM64x: How to enable PRU RPMsg on Processor SDK Linux 11.0](https://e2e.ti.com/support/processors-group/processors/f/processors-forum/1494495/faq-am62x-am64x-how-to-enable-pru-rpmsg-on-processor-sdk-linux-11-0).
3. Confirm the RPMsg echo still works.
4. Implement the UART logic at each `FIXME` handler stub in
   `firmware/main.c`. Until then, the handlers are empty: UART events are
   cleared but no data is moved.

FIXME: expected output for the UART data path cannot be documented until the
UART logic is implemented and validated on hardware.
