# PRU Academy

[Projects](#projects)  
[Supported processors per-project](#supported-processors-per-project)

This directory contains labs for the PRU Academy. It includes the PRU Getting
Started Labs, as well as training labs about different topics.

The PRU Academy is a module of the processor academy:
* [AM243x Academy](https://dev.ti.com/tirex/explore/node?node=A__AEIJm0rwIeU.2P1OBWwlaA__AM24X-ACADEMY__ZPSnq-h__LATEST)
* [AM26x Academy (AM261x, AM263Px, AM263x. PRU Academy coming soon)](https://dev.ti.com/tirex/explore/node?node=A__AEIJm0rwIeU.2P1OBWwlaA__AM26X-ACADEMY__t0CaxbG__LATEST)
* [AM62x Academy (PRU Academy coming soon)](https://dev.ti.com/tirex/explore/node?node=A__AEIJm0rwIeU.2P1OBWwlaA__AM62-ACADEMY__uiYMDcq__LATEST)
* [AM64x Academy](https://dev.ti.com/tirex/explore/node?node=A__AEIJm0rwIeU.2P1OBWwlaA__AM64-ACADEMY__WI1KRXP__LATEST)

## Projects

### PRU Getting Started Labs

The PRU Getting Started Labs demonstrate how to create a PRU project. The labs
also demonstrate how to write, compile, load, and debug PRU firmware.

assembly_code
* Example project written in assembly

c_code
* Example project written in C

c_and_assembly
* Example project written in C, with a function written in assembly

c_and_inline_assembly
* Example project written in C, with inline assembly code

### Training labs

crc/crc
* How to use the CRC16/32 module

gpio/gpio_toggle
* How to toggle the SoC GPIO pins using the PRU GPIO module

intc/intc_mcu
* How to use the INTC module to send and receive interrupts
  between the PRU core and an MCU+ core

mac/mac
* How to use the MAC module in Multiply and Accumulate mode

mac/mac_multiply
* How to use the MAC module in Multiply-Only mode

## Supported processors per-project

Each project is tested on at least one processor. Many projects can be ported to
other processors, even if the project does not currently have a build
configuration for the other processors.

For more information about porting PRU
projects to different processors, refer to app note
[PRU Subsystem Migration Guide](https://www.ti.com/lit/spracj8).
For more information about the PRU features on each processor, refer to app note
[PRU Subsystem Features Comparison](https://www.ti.com/lit/sprac90).

* Y = project has build infrastructure for this processor
* Yport = project can be ported to this processor
* Npru = project is not compatible with the PRU subsystem on this processor
  * The project cannot be ported to this processor
* N-hw = project relies on SoC hardware that does not exist on this processor
  * The project cannot be ported to this processor, unless the project can be
    modified to work with the new processor's hardware
* N-sw = project's non-PRU software is not compatible with this processor
  * The PRU firmware may be able to be ported to this processor,
    but you will need to write new code for the non-PRU cores
  * There may be limitations related to the OS (for example, let's say the
    project requires a real-time OS that always responds within 10 usec. This
    project would not work with OSes like Linux, or even RT Linux)

| Project              | am243x | am261x | am263px | am263x | am62x | am64x |
| -------------------- | ------ | ------ | ------- | ------ | ----- | ----- |
| crc/crc              | Yport  | Y      | Y       | Y      | Yport | Yport |
| getting_started_labs | Y      | Y      | Y       | Y      | Y     | Y     |
| gpio/gpio_toggle     | Yport  | Y      | Y       | Y      | N-sw  | Yport |
| intc/intc_mcu        | Yport  | Y      | Y       | Y      | N-sw  | Yport |
| mac/mac              | Yport  | Y      | Y       | Y      | Yport | Yport |
| mac/mac_multiply     | Yport  | Y      | Y       | Y      | Yport | Yport |
