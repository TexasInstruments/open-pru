<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>
# OpenPRU

</div>

[Introduction](#introduction)  
[Features](#features)  
[Training](#training)  
[Getting started](#getting-started)  
[OpenPRU organization](#open-pru-organization)  
[Building the examples](#building-the-examples)  
[Using EVM boards](#using-evm-boards)  
[Creating a new OpenPRU project](#creating-a-new-open-pru-project)  
[Contributing to the OpenPRU repo](#contributing-to-the-open-pru-repo)  
[Technical Support](#technical-support)  

## Introduction

OpenPRU is a software development package that enables development on the PRU processor core. PRU cores are included in Texas Instruments (TI) Sitara devices and Jacinto class of devices.

The OpenPRU project currently supports these processors:

- AM243x: [AM2431](https://www.ti.com/product/AM2431), [AM2432](https://www.ti.com/product/AM2432), [AM2434](https://www.ti.com/product/AM2434)
- AM261x: [AM2612](https://www.ti.com/product/AM2612)
- AM263x: [AM2631](https://www.ti.com/product/AM2631), [AM2631-Q1](https://www.ti.com/product/AM2631-Q1), [AM2632](https://www.ti.com/product/AM2632), [AM2632-Q1](https://www.ti.com/product/AM2632-Q1), [AM2634](https://www.ti.com/product/AM2634), [AM2634-Q1](https://www.ti.com/product/AM2634-Q1)
- AM263Px: [AM263P2-Q1](https://www.ti.com/product/AM263P2-Q1), [AM263P2](https://www.ti.com/product/AM263P2), [AM263P4-Q1](https://www.ti.com/product/AM263P4-Q1), [AM263P4](https://www.ti.com/product/AM263P4)
- AM62x: [AM623](https://www.ti.com/product/AM623), [AM625](https://www.ti.com/product/AM625)
- AM64x: [AM6411](https://www.ti.com/product/AM6411), [AM6412](https://www.ti.com/product/AM6412), [AM6421](https://www.ti.com/product/AM6421), [AM6422](https://www.ti.com/product/AM6422), [AM6441](https://www.ti.com/product/AM6441), [AM6442](https://www.ti.com/product/AM6442).

For basic PRU development support on AM335x, AM437x, and AM57x, refer to the [PRU Software Support Package (PSSP)](https://git.ti.com/cgit/pru-software-support-package/pru-software-support-package).

[Release notes are here](./docs/release_notes.md). Please refer to the release
notes for information like compatible SDK versions on each release.

## Features

The OpenPRU project provides:
  - [PRU Academy](./academy)
    - PRU Getting Started Labs (project creation, coding in assembly & C,
      compiling, loading PRU code, debugging PRU code)
    - Training labs (GPIO, interrupt controller, broadside accelerators, etc.)
  - [Application examples](./examples)
    - These PRU examples can be used as a foundation for your design: I2S, SPI, I2C, ADC, etc.
  - [Helpful software tools](./source)
    - firmware macros
    - register definitions
    - example drivers

Each project is tested on at least one processor. Many projects can be ported to
other processors, even if the example does not currently have a build
configuration for the other processors. For more information, refer to
[academy/readme.md](./academy/readme.md) and
[examples/readme.md](./examples/readme.md).

## Training

TI provides additional training for programming the PRU subsystem in
processor-specific academies. The currently published PRU Academies are:

[PRU Academy for AM26x](https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AA63DGBS9m5Kf5FElQrpyQ__AM26X-ACADEMY__t0CaxbG__LATEST)  
[PRU Academy for AM243x](https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AB.mSUi9ihL.a5hIt1grfw__AM24X-ACADEMY__ZPSnq-h__LATEST)  
[PRU Academy for AM62x](https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AEIJm0rwIeU.2P1OBWwlaA__AM62-ACADEMY__uiYMDcq__LATEST)  
[PRU Academy for AM64x](https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AB.mSUi9ihL.a5hIt1grfw__AM64-ACADEMY__WI1KRXP__LATEST)  

## Getting started

Please follow the [Getting started steps](./docs/getting_started.md) to install
dependencies and properly set up the OpenPRU repository.

## OpenPRU organization

For more information about the organization of both the OpenPRU repo and the
OpenPRU projects, please refer to
[OpenPRU organization](./docs/open_pru_organization.md)

## Building the examples 

### Building With Makefiles

> [!NOTE]
> Use `gmake` in Windows, and `make` in Linux.
>
> gmake is present in CCS. Add the path to the CCS gmake at
> `C:\ti\ccsxxxx\ccs\utils\bin` to your windows PATH.
>
> Unless mentioned otherwise, all `make` commands are invoked from the root
> folder of the `open-pru` repository.

Makefiles can be used to:
   - build or clean all OpenPRU projects
   - build or clean a specific OpenPRU project
   - build or clean code for a specific core

For detailed steps on how to use makefiles, run ```make help``` from the root
folder of the `open-pru` repository.

### Building With CCS

- Import the project into CCS. For steps to create a new PRU project in CCS,
  refer to
  **PRU Academy > PRU Getting Started Labs > How to Create a PRU Project**

- Right-click a PRU project in the EXPLORER menu, and select "Build Projects" to
  build the PRU firmware, or "Clean Projects" to clean the output

- For additional information about building PRU firmware, refer to the
  **PRU Academy > PRU Getting Started Labs > How to Compile PRU Firmware**

#### Building MCU+ projects in CCS

- The PRU firmware must be built before the MCU+ firmware. By default, projects
  from the OpenPRU repo place the PRU firmware header file in the CCS workspace,
  in the top level of the PRU project's directory

- The MCU+ project include paths **must** include the path to the PRU project
  directory in the CCS workspace

- For additional information, refer to
  - **PRU Academy > PRU Getting Started Labs > How to Create a PRU Project > Creating a CCS PRU Project with MCU+ Code**
  - **PRU Academy > PRU Getting Started Labs > How to Initialize the PRU > Initializing the PRU from MCU+ core**

- For more information about using MCU+ SDK projects with CCS, refer to the
  MCU+ SDK documentation: **Developer Guides > Using SDK with CCS Projects** 
  - [AM64x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/CCS_PROJECTS_PAGE.html)
  - [AM243x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/CCS_PROJECTS_PAGE.html)
  - [AM261x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM261X/latest/exports/docs/api_guide_am261x/CCS_PROJECTS_PAGE.html)
  - [AM263Px](https://software-dl.ti.com/mcu-plus-sdk/esd/AM263PX/latest/exports/docs/api_guide_am263px/CCS_PROJECTS_PAGE.html)
  - [AM263x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM263X/latest/exports/docs/api_guide_am263x/CCS_PROJECTS_PAGE.html)

## Using EVM boards

Please note that different EVMs have different signals pinned out. Before
purchasing an EVM, we suggest checking the signals that are routed out on the
board, and selecting an EVM which exposes the PRU signals that you will need for
your development.

### Using an EVM with MCU+ SDK

For more details on EVM Board usage, please refer to the Getting started section of MCU+ SDK README_FIRST_*.html page. The MCU+ SDK User guides contain information on
  
- EVM setup
- CCS Setup, loading and running examples
- Flashing the EVM
- SBL, ROV and much more.

Getting started guides of MCU+ SDK are specific to a particular device. The links for all the supported devices are given below

- [AM243x Getting Started Guide](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/GETTING_STARTED.html)
- [AM64x  Getting Started Guide](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/GETTING_STARTED.html)
- [AM261x Getting Started Guide](https://software-dl.ti.com/mcu-plus-sdk/esd/AM261X/latest/exports/docs/api_guide_am261x/GETTING_STARTED.html)
- [AM263Px Getting Started Guide](https://software-dl.ti.com/mcu-plus-sdk/esd/AM263PX/latest/exports/docs/api_guide_am263px/GETTING_STARTED.html)
- [AM263x Getting Started Guide](https://software-dl.ti.com/mcu-plus-sdk/esd/AM263X/latest/exports/docs/api_guide_am263x/GETTING_STARTED.html)

## Creating a new OpenPRU project

An existing project can be copied into a new directory, or imported into CCS,
to serve as a starting point for PRU development.

For steps to create a new OpenPRU project, refer to page
[Creating a New Project in the OpenPRU Repo](./docs/open_pru_create_new_project.md).

For steps to create a new project in CCS, or for device-specific steps to create
a new OpenPRU project, refer to the
**PRU Academy > PRU Getting Started Labs > How to Create a PRU Project**.

## Contributing to the OpenPRU repo

Please refer to [Contributing to the OpenPRU project](./docs/contributing.md). 

## Technical Support

For questions to the TI apps team, reach out to us on
[TI's E2E forums](https://e2e.ti.com/). Please note that support may be limited
for OpenPRU projects in the examples/ folder.

The [open-pru Discussions tab](https://github.com/TexasInstruments/open-pru/discussions)
is another place for discussion between community members.
