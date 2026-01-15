# RPMsg IPC - Linux Echo example

## Introduction

This RPMsg example demonstrates the RPMsg inter-processor communication (IPC)
protocol for communication between Linux running on an ARM core, and the PRU.

## Supported Combinations

Refer to open-pru/examples/readme.md > Supported processors per-project
for the list of processors that support building this project, and information
about porting this project to other processors.

## Validated HW & SW

This project was tested on hardware with these software versions:

| Processor | Hardware  | Software                   |
| --------- | --------- | -------------------------- |
| am62x     | SK-AM62B  | Linux SDK 11.1, OpenPRU commit "examples: add rpmsg_echo_linux project" |
| am64x     | TMDS64EVM | Linux SDK 11.1, OpenPRU commit "examples: add rpmsg_echo_linux project" |

Note:
Linux SDK 9.0 - 10.1 (Linux kernels 6.1 & 6.6) requires
modifications to run. Refer to
[open-pru/docs/using_older_sdks_with_open_pru.md](../../docs/using_older_sdks_with_open_pru.md),
Linux SDK > "Optional modifications" [5].

Note:
Linux SDK 11.0 (Linux kernel 6.12) requires a Linux kernel patch in order to
enable PRU RPMsg. Refer to
[open-pru/docs/using_older_sdks_with_open_pru.md](../../docs/using_older_sdks_with_open_pru.md),
Linux SDK > "Optional modifications" [6].

## Overview

RPMsg may not be the best IPC method for every design, but RPMsg is useful for
debugging, demonstrations, and usecases that do not require
communication that is low latency or high throughput.

In this example, the PRU initializes an RPMsg channel with Linux. The PRU then
polls until it detects that an RPMsg is received. An identical RPMsg message is
echoed back to the Linux source that sent the message.

### Per-core IPC settings

The RPMsg example was written so that the example could run simultaneously on
multiple PRU cores. Each PRU core uses system events and a unique RPMsg endpoint
(or "channel port") for RPMsg signaling.
The RPMsg endpoint must be unique from all other RPMsg
endpoints on the processor. The system events must be unique from all other PRU
cores within the same PRU subsystem instance. These values are defined
in the core-specific makefiles:

| AM62x | HOST_INT_BIT | TO_ARM_HOST | FROM_ARM_HOST | CHAN_PORT |
| ----- | ------------ | ----------- | ------------- | --------- |
| PRU0  | 30           | 16          | 17            | 30        |
| PRU1  | 31           | 18          | 19            | 31        |

| AM64x       | HOST_INT_BIT | TO_ARM_HOST | FROM_ARM_HOST | CHAN_PORT |
| ----------- | ------------ | ----------- | ------------- | --------- |
| ICSSG0 PRU0 | 30           | 16          | 17            | 30        |
| ICSSG0 PRU1 | 31           | 18          | 19            | 31        |
| ICSSG0 RTU0 | 30           | 20          | 21            | 32        |
| ICSSG0 RTU1 | 31           | 22          | 23            | 33        |
| ICSSG1 PRU0 | 30           | 16          | 17            | 34        |
| ICSSG1 PRU1 | 31           | 18          | 19            | 35        |
| ICSSG1 RTU0 | 30           | 20          | 21            | 36        |
| ICSSG1 RTU1 | 31           | 22          | 23            | 37        |

## Steps to Run the Example

For now, the most up-to-date steps to run the PRU RPMsg example are in
"Step 3: Run the PRU RPMsg Echo example" of
[[FAQ] AM62x & AM64x: How to enable PRU RPMsg on Processor SDK Linux 11.0](https://e2e.ti.com/support/processors-group/processors/f/processors-forum/1494495/faq-am62x-am64x-how-to-enable-pru-rpmsg-on-processor-sdk-linux-11-0).

The out-of-date PRU "RPMsg Quick Start Guide" is located in the Linux SDK docs [1].
This document will be updated and ported to PRU Academy at some point in the future.

For additional information about the Linux <--> PRU RPMsg implementation, refer
to the Linux SDK docs > PRU Subsystem > "RPMsg" [2].

[1] https://software-dl.ti.com/processor-sdk-linux/esd/AM62X/11_01_05_03/exports/docs/linux/Foundational_Components/PRU-ICSS/RPMsg_Quick_Start_Guide.html
[2] https://software-dl.ti.com/processor-sdk-linux/esd/AM62X/11_01_05_03/exports/docs/linux/Foundational_Components/PRU-ICSS/Linux_Drivers/RPMsg.html
