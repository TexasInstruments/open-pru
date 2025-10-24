<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>
# OpenPRU

</div>

[Introduction](#introduction)  
[Features](#features)  
[Getting started](#getting-started)  
[OpenPRU organization](#open-pru-organization)  
[Building the examples](#building-the-examples)  
[Information about using EVM boards](#information-about-using-evm-boards)  
[Creating a new OpenPRU project](#creating-a-new-open-pru-project)  
[Contributing to the OpenPRU repo](#contributing-to-the-open-pru-repo)  
[Technical Support](#technical-support)  

## Introduction

OpenPRU is a software development package that enables development on the PRU processor core. PRU cores are included in Texas Instruments (TI) Sitara devices and Jacinto class of devices.

The OpenPRU project currently supports these processors:

- AM243x: [AM2431](https://www.ti.com/product/AM2431), [AM2432](https://www.ti.com/product/AM2432), [AM2434](https://www.ti.com/product/AM2434)
- AM261x: [AM2612](https://www.ti.com/product/AM2612)
- AM263x: [AM2631](https://www.ti.com/product/AM2631), [AM2631-Q1](https://www.ti.com/product/AM2631-Q1), [AM2632](https://www.ti.com/product/AM2632), [AM2632-Q1](https://www.ti.com/product/AM2632-Q1), [AM2634](https://www.ti.com/product/AM2634), [AM2634-Q1](https://www.ti.com/product/AM2634-Q1)
- AM263px: [AM263P2-Q1](https://www.ti.com/product/AM263P2-Q1), [AM263P2](https://www.ti.com/product/AM263P2), [AM263P4-Q1](https://www.ti.com/product/AM263P4-Q1), [AM263P4](https://www.ti.com/product/AM263P4)
- AM64x: [AM6411](https://www.ti.com/product/AM6411), [AM6412](https://www.ti.com/product/AM6412), [AM6421](https://www.ti.com/product/AM6421), [AM6422](https://www.ti.com/product/AM6422), [AM6441](https://www.ti.com/product/AM6441), [AM6442](https://www.ti.com/product/AM6442).

For basic PRU development support on AM335x, AM437x, AM57x, and AM62x, refer to the [PRU Software Support Package (PSSP)](https://git.ti.com/cgit/pru-software-support-package/pru-software-support-package).

[Release notes are here](./docs/release_notes.md). Please refer to the release
notes for information like compatible SDK versions on each release.

## Features

The OpenPRU project provides:
  - [PRU Academy](./academy)
    - PRU Getting Started Labs (project creation, coding in assembly & C, compiling, loading PRU code, debugging PRU code)
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

## Getting started

Please follow the [Getting started steps](./docs/getting_started.md) to install dependencies and properly set up the OpenPRU repository.

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

### Basic Building With CCS

- **When using CCS projects to build**, import the CCS project from the project
  folder. The project files can be copied to the ccs workspace of the PRU project.

- Build the PRU project using the CCS project menu. Refer to
  **the MCU+ SDK documentation > Using SDK with CCS Projects**:
  - [AM64x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/CCS_PROJECTS_PAGE.html)
  - [AM243x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/CCS_PROJECTS_PAGE.html)
  - [AM261x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM261X/latest/exports/docs/api_guide_am261x/CCS_PROJECTS_PAGE.html)
  - [AM263Px](https://software-dl.ti.com/mcu-plus-sdk/esd/AM263PX/latest/exports/docs/api_guide_am263px/CCS_PROJECTS_PAGE.html)
  - [AM263x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM263X/latest/exports/docs/api_guide_am263x/CCS_PROJECTS_PAGE.html)

#### Build the PRU firmware

- Once you click on **build** in the PRU project, the PRU firmware header file is generated in the CCS release or debug folder. The PRU header file is moved to  `<open-pru/examples/empty/firmware/device/>`

#### Build the R5F firmware

- Build the R5F project using the CCS project menu. For more details, refer to **the MCU+ SDK documentation > Using SDK with CCS Projects** linked above.

- The PRU Firmware header file path is included in the R5F project include options by default.

- Build Flow: Once you click on build in the R5F project, SysConfig files are generated. Then the R5F project will be generated using both the generated SysConfig files, and PRU project binaries.

## Information about using EVM boards

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

For example, copy `examples/empty` to `examples/my_project`.

Since `empty` and `my_project` are in the same parent directory, `my_project`
can be built from the project directory after updating any paths that use the
project name:

```
$ cd /path/to/open-pru/examples/my_project
$ make -s
```

For additional steps to create a new OpenPRU project, refer to page
[Creating a New Project in the OpenPRU Repo](./docs/open_pru_create_new_project.md).

## Contributing to the OpenPRU repo

Please refer to [Contributing to the OpenPRU project](./docs/contributing.md). 

## Technical Support

For questions to the TI apps team, reach out to us on
[TI's E2E forums](https://e2e.ti.com/). Please note that support may be limited
for OpenPRU projects in the examples/ folder.

The [open-pru Discussions tab](https://github.com/TexasInstruments/open-pru/discussions)
is another place for discussion between community members.
