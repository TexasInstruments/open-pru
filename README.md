<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>
# OPEN PRU

[Introduction](#introduction) | [Features](#features) | [Overview](#overview) | [Learn](#learn) | [Usage](#usage) | [Contribute](#contributing-to-the-project)

</div>

## Introduction

OPEN PRU is a software development package designed for usage with the PRU processor from Texas Instruments Sitara devices and Jacinto class of devices. These devices currently include:

- [AM2431](https://www.ti.com/product/AM2431), [AM2432](https://www.ti.com/product/AM2432), [AM2434](https://www.ti.com/product/AM2434)
- [AM6411](https://www.ti.com/product/AM6411), [AM6412](https://www.ti.com/product/AM6412), [AM6421](https://www.ti.com/product/AM6421), [AM6422](https://www.ti.com/product/AM6422), [AM6441](https://www.ti.com/product/AM6441), [AM6442](https://www.ti.com/product/AM6442).

OPEN PRU is designed with user experience and simplicity in mind. The repository includes out-of-box Getting started PRU labs, Macro definitions and application examples.

## Features

- Out of Box Getting started labs, Macro definitions and application Examples
  - [PRU Academy](./pru_academy): Task manager, Interrupt controller, Broadside accelerator, Labs etc.,
  - [Application examples](./examples): ADC, SENT, I2C, I2S etc.,
  - [Macro definitions](./source) of PRUSS, PRUICSSM, PRUICSSG components

## Usage

### Prerequisites

#### Supported HOST environments

- Validated on Windows 10 64bit, higher versions may work
- Validated on Ubuntu 18.04 64bit, higher versions may work

**On Windows the dependencies have to be manually installed. Follow these steps**:

1. Download and install Code Composer Studio v12.8 from [here](https://www.ti.com/tool/download/CCSTUDIO "Code Composer Studio")
   - Install at default folder: C:\ti

2. Download and install SysConfig 1.21.0 from [here](https://www.ti.com/tool/download/SYSCONFIG "SYSCONFIG 1.21.0")
   - Install at default folder: C:/ti

3. Download and install the PRU compiler
   - [PRU-CGT-2-3](https://www.ti.com/tool/PRU-CGT) (ti-pru-cgt)
   - Install at default folder: C:/ti

4. Download and install GCC for Cortex A53 (required only for AM64x developers)
   - [GNU-A](https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-mingw-w64-i686-aarch64-none-elf.tar.xz)
   - [GNU-RM](https://developer.arm.com/-/media/Files/downloads/gnu-rm/7-2017q4/gcc-arm-none-eabi-7-2017-q4-major-win32.zip)
   - Install at default folder: C:/ti

5. Download and install Node.js v12.18.4 LTS
   - Go to the [NodeJS Website](https://nodejs.org/en/) and use the installer to
    download and install v12.18.4 of node. Install in the default directory.
   - After successful installation, run an `npm ci` inside the `open-pru` folder like so:
      ```bash
      $ cd open-pru/
      $ npm ci
      ```
   - To specify a proxy server, use the --proxy option followed by the proxy server link, like this: --proxy = <proxy server link>
     This should install the node packages required for the open-pru.

## Overview

The MCU+ SDK is a dependency when building OPEN PRU projects that include code for an MCU+ core, Users can either use the Prebuilt SDK installers for specific devices or clone the MCU+ SDK repository in `C:\ti`

---
**NOTE**
- Use `gmake` in windows, add path to gmake present in CCS at `C:\ti\ccsxxxx\ccs\utils\bin` to your windows PATH. We have
  used `make` in below instructions.
- Unless mentioned otherwise, all `make` commands are invoked from the root folder of the `open-pru` repository.
   ```bash
   cd open-pru/
   ```
- Current supported device names are am64x, am243x
- Pass one of these values for `"DEVICE="`
- You can also build components (examples, tests or libraries) in the `release` or `debug`
  profiles. To do this pass one of these values for `"PROFILE="`
---

#### Option 1: Prebuilt SDK installers for specific devices are available at below links. Please note that installers are packaged specifically to each device to reduce size.
- [AM243x MCU+ SDK](https://www.ti.com/tool/MCU-PLUS-SDK-AM243X)
- [AM64x  MCU+ SDK](https://www.ti.com/tool/MCU-PLUS-SDK-AM64X)
   ```bash
   make gen-buildfiles DEVICE=am64x MCU_PLUS_SDK_PATH=<MCU_PLUS_SDK_INSTALL_PATH>
   make gen-buildfiles DEVICE=am243x MCU_PLUS_SDK_PATH=<MCU_PLUS_SDK_INSTALL_PATH>
   ```

#### Option 2: MCU+ SDK repository is core for all the Sitara MCU and MPU devices, checkout README.md to clone
- [MCU PLUS SDK](https://github.com/TexasInstruments/mcupsdk-core)
   ```bash
   make gen-buildfiles DEVICE=am64x MCU_PLUS_SDK_PATH=<MCU_PLUS_SDK_INSTALL_PATH> GEN_BUILDFILES_TARGET=development 
   make gen-buildfiles DEVICE=am243x MCU_PLUS_SDK_PATH=<MCU_PLUS_SDK_INSTALL_PATH> GEN_BUILDFILES_TARGET=development
   ```

## Learn

TI has a large collection of tutorials as part of PRU Academy to help you get started.

- [PRU getting_started](./pru_academy/getting_started)

### Building the examples 

#### Basic Building With Makefiles

1. Make sure to build the PRU firmware before attempting to build an example. For example,
   to build a PRU firmware for empty example of AM243x, run the following:
   ```bash
	make -s -C examples/empty/firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt     all 
	make -s -C examples/empty/firmware/am243x-lp/icss_g0_pru1_fw/ti-pru-cgt     all 
	make -s -C examples/empty/firmware/am243x-lp/icss_g0_rtu_pru0_fw/ti-pru-cgt all
	make -s -C examples/empty/firmware/am243x-lp/icss_g0_rtu_pru1_fw/ti-pru-cgt all
	make -s -C examples/empty/firmware/am243x-lp/icss_g0_tx_pru0_fw/ti-pru-cgt  all
	make -s -C examples/empty/firmware/am243x-lp/icss_g0_tx_pru1_fw/ti-pru-cgt  all
   ```
   Once the PRU Firmware build is complete, Firmware header files of PRU are moved to R5F default include path, to build R5F example run:
   ```bash
   make -s -C examples/empty/am243x-lp/r5fss0-0_freertos/ti-arm-clang all PROFILE=debug
   ```

2. Following are the commands to build **all examples**. Valid PROFILE's are "release" or "debug"

   ```bash
   make -s -j4 clean DEVICE=am243x PROFILE=debug
   make -s -j4 all   DEVICE=am243x PROFILE=debug
   ```

#### Basic Building With CCS

- **When using CCS projects to build**, import the CCS project from the above mentioned Example folder path for R5F and PRU, After this `main.asm`, `linker.cmd` files get copied to ccs workspace of PRU project.

- Build the PRU project using the CCS project menu (see [for AM64x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/CCS_PROJECTS_PAGE.html), [for AM243x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/CCS_PROJECTS_PAGE.html)).
     - Build Flow: Once you click on build in PRU project, firmware header file which is generated in release or debug folder of ccs workspace, is moved to  `<open-pru/examples/pru_io/empty/firmware/device/>`

- Build the R5F project using the CCS project menu (see [for AM64x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/CCS_PROJECTS_PAGE.html), [for AM243x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/CCS_PROJECTS_PAGE.html)).
     - Firmware header file path is included in R5F project include options by default, Instructions in Firmware header file can be written into PRU IRAM memory using PRUICSS_loadFirmware API (see [for AM64x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/group__DRV__PRUICSS__MODULE.html#ga3e7c763e5343fe98f7011f388a0b7ffe), [for AM243x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/group__DRV__PRUICSS__MODULE.html#ga3e7c763e5343fe98f7011f388a0b7ffe))
     - Build Flow: Once you click on build in R5F project, SysConfig files are generated, Finally the R5F project will be generated using both the generated SysConfig and PRU project binaries.

### More information on Board usage

For more details on Board usage, please refer to the Getting started section of MCU+ SDK README_FIRST_*.html page. User guides contain information on
  
- EVM setup
- CCS Setup, loading and running examples
- Flashing the EVM
- SBL, ROV and much more.

Getting started guides of MCU+ SDK are specific to a particular device. The links for all the supported devices are given below

- [AM243x Getting Started Guide](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/GETTING_STARTED.html)
- [AM64x  Getting Started Guide](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/GETTING_STARTED.html)

## Technical Support

Please consider creating a post on [TI's E2E forum](https://e2e.ti.com/).

## Contributing to the project

### Steps to add new example in open-pru repository

#### Step 1 : create .project folder inside new_example folder 

This folder contains the following files:

- project.js : Gets the component properties and component build properties from project_{device}.js file. There should only be one project.js file inside .project folder
- project_{device.js} : Different custom build properties, compiler flags, linker flags etc. depending on the device can be specified in this file. These files can be created for different devices, for example, project_am64x.js , project_am243x.js etc.

Note : project.js file can be used from empty example without any modifications and initial version of project_{device}.js can be copied from empty example and can be modified as per requirements, when new R5F project to be created use project_{device.js} from examples/empty/.project/project_{device.js} and when new PRU project to be created use project_{device.js} from examples/empty/firmware/.project/project_{device.js}

In general, the .project folder inside the new example should look similar to below image

<img src="./images/readme/fig1.png" width="600"><br/>

#### Step 2 : Modify the contents of the project_{device}.js

This file uses 2 functions: getComponentProperty() and getComponentBuildProperty() where user can provide different values as per their requirements to build a PRU project of our choice, some of which are discussed as follows.

- Change the name of the project by modifying the value property.name .
- Add a description for the project by modifying the value property.description . 
   - An example is given below
          ```
          property.name = "new_example";
          property.description = "new example PRU Project"
          ```
- Provide the different build option combos, by modifying the value of property.buildOptionCombos. It takes in an array of JavaScript objects wherein one can specify the device, cpu, cgt, board and os. An example array of build option combos is given below.
   ```
   const buildOptionCombos = [
      { device: device, cpu: "icssg0-pru0", cgt: "ti-pru-cgt", board: "am243x-evm", os: "fw"},
      { device: device, cpu: "icssg0-pru0", cgt: "ti-pru-cgt", board: "am243x-lp", os: "fw"},
   ];
   ```
- Default PRU post build steps and custom PRU post build steps can be added as below
   - In case of default Post Build steps, just assign the value true to property.defaultPruPostBuildSteps. 
   - In case of custom Post Build steps, create a JavaScript array with the steps and assign it to property.postBuildSteps. An example array of Post Build steps is given below. 
   ```
   let postBuildSteps = [
      "$(CGT_TI_PRU_PATH)/bin/hexpru --diag_wrap=off --array --array:name_prefix=PRUFirmware  -o new_example.h new_example_{board}_{cpu}_{os}_{cgt}.out"
   ];
   - Note : {board}, {cpu}, {os}, {cgt} from build options is added to property.name then project and .out is created with new_example_{board}_{cpu}_{os}_{cgt} name
   ```
- Compiler includes while building the project can be specified using build_property.includes. An example of a custom includes object is given below.
   ```
   const includes = {
      common: [
         "${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/common",
         "${OPEN_PRU_PATH}/source"
      ],
   };
   ```
- Custom defines while building the project can be specified using build_property.defines in getComponentBuildProperty function. An example of a custom defines object is given below
   ```
   const defines = {
      common: [
         "NEW_EXAMPLE"
      ],
   };
   ```
- Custom compiler flags and linker flags can be specified using build_property.cflags and build_property.cflags respectively in getComponentBuildProperty function. An example of custom compiler flags and linker flags objects are given below
   ```
   const cflags = {
      common: [
         "-v4"
      ],
   };
   
   const lflags = {
      common: [
         "--entry_point=main",
      ],
   };
   ```
#### Step 3 : Add the path of the project.js file to the respective device's examples file list

The path to the project.js of the new project needs to be added to the example file list of the required device. Go to open_pru/.project/device/project_{device}.js and add the path to the project.js of the new example to the example_file_list array.

- For example, if one is using AM243x, go to  open_pru/.project/device/project_am243x.js and add examples/new_example/.project/project.js to the example_file_list array. In general, it should look similar below image.

<img src="./images/readme/fig2.png" width="600"><br/>

#### Step 4 : Generate build files

- Run one of the below command from the root folder of open-pru

##### Option 1: Prebuilt MCU PLUS SDK installers for specific devices are used
   ```bash
   make gen-buildfiles DEVICE={device} 
   ```
   - For example, if the new example is created am243x run below command
      ```bash
      make gen-buildfiles DEVICE=am243x
      ```

##### Option 2: MCU+ SDK repository which is core for all the Sitara MCU and MPU devices is used
   ```bash
   make gen-buildfiles DEVICE={device} GEN_BUILDFILES_TARGET=development
   ```
   - For example, if the new example is created am243x run below command
      ```bash
      make gen-buildfiles DEVICE=am243x GEN_BUILDFILES_TARGET=development
      ```

#### Step 5 : Link or Copy source files to ccs workspace
- In order to link source files to workspace add below in getComponentBuildProperty
   ```bash
   build_property.projecspecFileAction = "link"
   ```
- In order to copy source files to workspace use below in getComponentBuildProperty
   ```bash
   build_property.projecspecFileAction = "copy"
   ```

#### Step 6:  Import to CCS
   - After step4, example.projectspec, makefile and the files mentioned in build_property.templates get generated
   - Now one can simply import the project by browsing to that folder in CCS, then build and test it.

This project is currently not accepting contributions. Coming Soon!


