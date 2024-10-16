<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>
# OPEN PRU

[Introduction](#introduction) | [Features](#features) | [Overview](#overview) | [Learn](#learn) | [Usage](#usage) | [Contribute](#contributing-to-the-project)

</div>

## Introduction

OPEN PRU is a software development package designed for usage with PRU processor from Texas Instruments Sitara devices and Jacinto class of devices. These devices currently include

- [AM2431](https://www.ti.com/product/AM2431), [AM2432](https://www.ti.com/product/AM2432), [AM2434](https://www.ti.com/product/AM2434)
- [AM6411](https://www.ti.com/product/AM6411), [AM6412](https://www.ti.com/product/AM6412), [AM6421](https://www.ti.com/product/AM6421), [AM6422](https://www.ti.com/product/AM6422), [AM6441](https://www.ti.com/product/AM6441), [AM6442](https://www.ti.com/product/AM6442)

OPEN PRU is designed with user experience and simplicity in mind. The repository includes out-of-box Getting started PRU labs, Macro definitions and application examples.

## Features

- Out of Box Getting started labs, Macro definitions and application Examples
  - [PRU Academy](./pru_academy): Task mangager, Interrupt controller, Broadside accelerator, Labs etc.,
  - [Application examples](./examples): ADC, SENT, I2C, I2S etc.,
  - [Macro definitions](./source) of PRUSS, PRUICSSM, PRUICSSG components

## Overview

The MCU+ SDK is a dependency when building OPEN PRU projects that include code for an MCU+ core, Users can use Prebuilt SDK installers for specific devices or clone MCU+ SDK repository

Prebuilt SDK installers for specific devices are available at below links. Please note that installers are packaged specific to each device to reduce size.

- [AM243x MCU+ SDK](https://www.ti.com/tool/MCU-PLUS-SDK-AM243X)
- [AM64x  MCU+ SDK](https://www.ti.com/tool/MCU-PLUS-SDK-AM64X)

MCU+ SDK repostiory is core for all the Sitara MCU and MPU devices, checkout README.md to clone
- [MCU PLUS SDK](https://github.com/TexasInstruments/mcupsdk-core)

## Learn

TI has an amazing collection of tutorials as PRU Academy to help you get started.

- [PRU getting_started](./pru_academy/getting_started)

## Usage

### Prerequisites

#### Supported HOST environments

- Validated on Windows 10 64bit, higher versions may work
- Validated on Ubuntu 18.04 64bit, higher versions may work

**In windows the dependencies has to be manually installed. Given below are the steps**:

1. Download and install Code Composer Studio v12.8 from [here](https://www.ti.com/tool/download/CCSTUDIO "Code Composer Studio")
   - Install at default folder, C:\ti

2. Download and install SysConfig 1.21.0 from [here](https://www.ti.com/tool/download/SYSCONFIG "SYSCONFIG 1.21.0")
   - Install at default folder, C:/ti

3. Download and install GCC for Cortex A53 (only needed for AM64x developers)
   - [GNU-A](https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-mingw-w64-i686-aarch64-none-elf.tar.xz)
   - [GNU-RM](https://developer.arm.com/-/media/Files/downloads/gnu-rm/7-2017q4/gcc-arm-none-eabi-7-2017-q4-major-win32.zip)
   - Install at default folder, C:/ti


### Building the SDK

#### Basic Building With Makefiles

---

**NOTE**

- Use `gmake` in windows, add path to gmake present in CCS at `C:\ti\ccsxxxx\ccs\utils\bin` to your windows PATH. We have
  used `make` in below instructions.
- Unless mentioned otherwise, all below commands are invoked from root folder of the "open-pru"  repository.
- Current supported device names are am64x, am243x
- Pass one of these values to `"DEVICE="`
- You can also build components (examples, tests or libraries) in `release` or `debug`
  profiles. To do this pass one of these values to `"PROFILE="`

---

1. Run the following command to create makefiles, this step is optional since this is invoked as part of other steps as well,

   ```bash
   make gen-buildfiles DEVICE=am243x
   ```

2. To see all granular build options, run

   ```bash
   make -s help DEVICE=am243x
   ```
   This should show you commands to build specific libraries, examples or tests.

3. Make sure to build the libraries before attempting to build an example. For example,
   to build a empty example for AM243x, run the following:
   ```bash
   make -s -j4 libs DEVICE=am243x PROFILE=debug
   ```
   Once the library build is complete, to build the example run:
   ```bash
   make -s -C examples/empty/am243x-lp/r5fss0-0_freertos/ti-arm-clang all PROFILE=debug
   ```

4. Following are the commands to build **all libraries** and **all examples**. Valid PROFILE's are "release" or "debug"

   ```bash
   make -s -j4 clean DEVICE=am243x PROFILE=debug
   make -s -j4 all   DEVICE=am243x PROFILE=debug
   ```

### More information on Board usage

For more details on Board usage, please refer to the Getting started section of MCU+ SDK README_FIRST_*.html page. User guides contain information on
  
- EVM setup
- CCS Setup, loading and running examples
- Flashing the EVM
- SBL, ROV and much more.

Getting started guides of MCU+ SDK are specific to a particular device. The links for all the supported devices are given below

- [AM243x Getting Started Guide](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/GETTING_STARTED.html)
- [AM64x  Getting Started Guide](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/GETTING_STARTED.html)

## Contributing to the project

This project is currently not accepting contributions. Coming Soon!
