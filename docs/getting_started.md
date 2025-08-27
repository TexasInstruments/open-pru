<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>
# Getting Started with OPEN PRU

[Supported HOST environments](#supported-host-environments) | [Which core will initialize the PRU?](#which-core-will-initialize-the-pru) | [Install dependencies (manual installation)](#install-dependencies-manual-installation) | [Install dependencies (script)](#install-dependencies-script) | [Generate buildfiles](#generate-buildfiles)

</div>

## Introduction

This page discusses how to:

1. Install the associated SDK and tools for your development platform (Windows or Linux). Install dependencies manually, or use the pru_dependencies script (AM243x, AM64x only)

2. Generate project buildfiles

After the OPEN PRU repository has been set up, refer back to the [README](./README.md) for build steps.

## Supported HOST environments

- Validated on Windows 10 64bit. Higher versions may work
- Validated on Ubuntu 18.04 64bit & Ubuntu 22.04 64bit. Higher versions may work

## Which core will initialize the PRU?

In your final design, the PRU cores must be initialized by another processor core. Depending on the processor, PRU cores can be initialized and controlled by cores running RTOS, bare metal, or Linux.

TI supports initializing the PRU from an RTOS/bare metal core on these processors:
- AM243x (R5F)
- AM261x (R5F)
- AM263x (R5F)
- AM263px (R5F)
- AM64x (R5F)

TI supports initializing the PRU from Linux on these processors:
- AM62x (A53)
- AM64x (A53)

TI supports RTOS & bare metal development through the MCU+ SDK, and Linux development through the Linux SDK.

## Install dependencies (manual installation)

AM243x & AM64x customers who will use the MCU+ SDK alongside PRU can install dependencies with the pru_dependencies script if they prefer. For those steps, refer to [Install dependencies (script)](#install-dependencies-script).

**Note:** In general, software dependencies should all be installed under the same folder. So Windows developers would put all software tools under C:\ti, and Linux developers would put all software tools under ${HOME}/ti.

If you will initialize the PRU cores from an RTOS or bare metal core, follow the steps in [Windows or Linux: Install the MCU+ SDK & tools](#windows-or-linux-install-the-mcu-sdk--tools).

If you will initialize the PRU cores from a Linux core, follow the steps in [Linux: Install the Linux SDK & tools](#linux-install-the-linux-sdk--tools).

### Windows or Linux: Install the MCU+ SDK & tools

The MCU+ SDK is only a dependency when building OPEN PRU projects that include code for an MCU+ core.

#### Install the MCU+ SDK

Users can either install the prebuilt MCU+ SDK, or clone the MCU+ SDK repository:

1. Download the prebuilt MCU+ SDK. The prebuilt SDK takes up less space on the computer, since it is packaged specifically for a single device.

Please use the links below to download the installers:
   - [AM243x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM243X)
   - [AM261x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM261X)
   - [AM263x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM263X)
   - [AM263Px MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM263PX)
   - [AM64x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM64X)

2. Clone the [MCU+ SDK repository](https://github.com/TexasInstruments/mcupsdk-core). Follow the mcupsdk-core README.md file for additional steps.

#### Install the tools

Users of the prebuilt MCU+ SDK, refer to the documentation associated with your specific SDK release. Users of the MCU+ SDK repository, refer to the documentation associated with the latest version of the MCU+ SDK.

1. Install the versions of the tools that are listed in these pages:
   - MCU+ SDK docs > Getting Started > Download, Install, and Setup SDK and Tools
   - MCU+ SDK docs > Getting Started > Download, Install, and Setup CCS
     - Developers using a Linux computer, make sure to follow the additional steps at "CCS Linux Host Support"
   
2. Download and install Node.js v12.18.4 LTS. Then install the node packages required for the open-pru:
   - Go to the [NodeJS Website](https://nodejs.org/en/) and use the installer to
     download and install v12.18.4 of node. Install in the default directory.
   - After successful installation, run an `npm ci` inside the `open-pru` folder like so:
      ```bash
      $ cd open-pru/
      $ npm ci
      ```
      - To specify a proxy server, use the --proxy option followed by the proxy server link, like this: `--proxy = <proxy server link>`

After installing all dependencies, continue to section [Generate buildfiles](#generate-buildfiles).

### Linux: Install the Linux SDK & tools

The Linux SDK can only be installed on a Linux computer, not a Windows computer.

#### Install the Linux SDK

The Linux SDK is only a dependency if building code that will run on a Linux A53 core.

Please use the links below to download the Linux SDK:
   - [AM62x Linux SDK](https://www.ti.com/tool/download/PROCESSOR-SDK-LINUX-AM62X)
   - [AM64x Linux SDK](https://www.ti.com/tool/download/PROCESSOR-SDK-LINUX-AM64X)

#### Install the tools

   1. Download and install Code Composer Studio v12.8 from [here](https://www.ti.com/tool/download/CCSTUDIO "Code Composer Studio")
         - Please install these dependencies before installing CCS on a Linux computer. Use the command
         ```bash
         $ sudo apt -y install libc6:i386 libusb-0.1-4 libgconf-2-4 libncurses5 libpython2.7 libtinfo5 build-essential
         ```

   2. Download and install SysConfig 1.21.0 from [here](https://www.ti.com/tool/download/SYSCONFIG "SYSCONFIG 1.21.0")

   3. Download and install the PRU compiler
      - [PRU-CGT-2-3](https://www.ti.com/tool/PRU-CGT) (ti-pru-cgt)

   4. Download and install GCC for Cortex A53 and R5F (if developing code to run on A53 or R5F)
      - A53: [GNU-A](https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-aarch64-none-elf.tar.xz)
      - R5F: [GNU-RM](https://developer.arm.com/-/media/Files/downloads/gnu-rm/7-2017q4/gcc-arm-none-eabi-7-2017-q4-major-linux.tar.bz2)

   5. Install Mono Runtime (required for creating bootloader images for application binaries)
      - To install, use the command `sudo apt install mono-runtime`

   6. Download and install Node.js v12.18.4 LTS
      - Go to the [NodeJS Website](https://nodejs.org/en/) and use the installer to
      download and install v12.18.4 of node. Install in the default directory.
      - After successful installation, run an `npm ci` inside the `open-pru` folder like so:
         ```bash
         $ cd open-pru/
         $ npm ci
         ```
      - To specify a proxy server, use the --proxy option followed by the proxy server link, like this: `--proxy = <proxy server link>`
      This should install the node packages required for the open-pru.

After installing all dependencies, continue to section [Generate buildfiles](#generate-buildfiles).

## Install dependencies (script)

The pru_dependencies scripts currently support installing MCU+ SDK and other dependencies on AM243x & AM64x. For all other devices, or for steps around installing the Linux SDK, please follow the steps at [Install dependencies (manual installation)](#install-dependencies-manual-installation).

**Note:** In general, software dependencies should all be installed under the same folder. So Windows developers would put all software tools under C:\ti, and Linux developers would put all software tools under ${HOME}/ti.

If the OPEN PRU repo is installed on a Windows machine, follow the steps in [Windows: Use script `pru_dependencies.bat` to install SDK & tools](#windows-use-script-pru_dependenciesbat-to-install-sdk--tools).

If the OPEN PRU repo is installed on a Linux machine, follow the steps in [Linux: Use script `pru_dependencies.sh` to install SDK & tools](#linux-use-script-pru_dependenciessh-to-install-sdk--tools).

### Windows: Use script `pru_dependencies.bat` to install SDK & tools

**NOTES**

   - If the script is executed from any folder but `C:\ti`, it will be copied to `C:\ti` after running it and a second terminal screen will be opened with the location of the script. Please re-run script in the new terminal screen
   - If OpenSSL is needed to be installed, when prompted select option to install binaries to /bin folder of installed path instead of Windows system path
   - If after installing the dependencies the script is executed to verify the installed, it will show the same missing dependencies. 
   The used terminal must be closed and re-opened to get the updated state (or PC must be restarted). This is a Windows limitation

#### Install the MCU+ SDK

By default, the script checks for and installs MCU+ SDK 11.0. In order to use a different SDK version, edit the **version numbers** in pru_dependencies.bat to match the desired SDK version from:
   - [AM243x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM243X)
   - [AM64x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM64X)

- For AM243x: run the command `pru_dependencies.bat -I --am243x_sdk`, `pru_dependencies.bat -i --am243x_sdk` or `pru_dependencies.bat install --am243x_sdk`

- For AM64x: run the command `pru_dependencies.bat -I --am64xx_sdk`, `pru_dependencies.bat -i --am64xx_sdk` or `pru_dependencies.bat install --am64xx_sdk`

Alternatively, the script can be used to clone the MCU+ SDK repository:

- run the command `pru_dependencies.bat -I --clone_sdk`, `pru_dependencies.bat -i --clone_sdk` or `pru_dependencies.bat install --clone_sdk`

#### Install the tools

By default, the script checks for and installs the tool versions used with MCU+ SDK 11.0. If using a different SDK version, edit the **version numbers** in pru_dependencies.bat to match the versions listed in **the MCU+ SDK docs**, sections **Getting Started > Download, Install, and Setup SDK and Tools** and **Download, Install, and Setup CCS**.

   1. It is recommended to verify the dependencies that are already installed. Run the command `pru_dependencies.bat -v`, `pru_dependencies.bat -V` or `pru_dependencies.bat verify`

   2. To install the dependencies, run the command `pru_dependencies.bat -I [dependencies]`, `pru_dependencies.bat -i [dependencies]` or `pru_dependencies.bat install [dependencies]`

   3. To get an assistance on how to use the script, run the command `pru_dependencies.bat -h` or `pru_dependencies.bat help`

After installing all dependencies, continue to section [Generate buildfiles](#generate-buildfiles).

### Linux: Use script `pru_dependencies.sh` to install SDK & tools

**NOTES**

- If the script is executed from any folder but `${HOME}/ti`, it will be copied to `${HOME}/ti` and executed from there automatically

#### Install the MCU+ SDK

By default, the script checks for and installs MCU+ SDK 11.0. In order to use a different SDK version, edit the **version numbers** in pru_dependencies.bat to match the desired SDK version from:
   - [AM243x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM243X)
   - [AM64x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM64X)

- For AM243x: run the command `./pru_dependencies.sh -I --am243x_sdk`, `./pru_dependencies.sh -i --am243x_sdk` or `./pru_dependencies.sh install --am243x_sdk`

- For AM64x: run the command `./pru_dependencies.sh -I --am64xx_sdk`, `./pru_dependencies.sh -i --am64xx_sdk` or `./pru_dependencies.sh install --am64xx_sdk`

Alternatively, the script can be used to clone the MCU+ SDK repository:

- run the command `./pru_dependencies.sh -I --clone_sdk`, `./pru_dependencies.sh -i --clone_sdk` or `./pru_dependencies.sh install --clone_sdk`

#### Install the tools

By default, the script checks for and installs the tool versions used with MCU+ SDK 11.0. If using a different SDK version, edit the **version numbers** in pru_dependencies.bat to match the versions listed in **the MCU+ SDK docs**, sections **Getting Started > Download, Install, and Setup SDK and Tools** and **Download, Install, and Setup CCS**.

   1. It is recommended to verify the dependencies that are already installed. Run the command `./pru_dependencies.sh -v`, `./pru_dependencies.sh -V` or `./pru_dependencies.sh verify`

   2. To install the dependencies, run the command `./pru_dependencies.sh -I [dependencies]`, `./pru_dependencies.sh -i [dependencies]` or `./pru_dependencies.sh install [dependencies]`

   3. To get an assistance on how to use the script, run the command `./pru_dependencies.sh -h` or `./pru_dependencies.sh help`

## Generate buildfiles

These steps are used to generate makefiles and other build infrastructure for each OPEN PRU project.

**NOTES**
- Use `gmake` in Windows, and `make` in Linux. gmake is present in CCS. Add the path to the CCS gmake at `C:\ti\ccsxxxx\ccs\utils\bin` to your windows PATH.
- Unless mentioned otherwise, all `make` commands are invoked from the root folder of the `open-pru` repository.
   ```bash
   cd open-pru/
   ```
- Current supported device names are am243x, am261x, am263x, am263px, am64x
- Pass one of these values for `"DEVICE="`
- You can also build components (examples, tests or libraries) in the `release` or `debug`
  profiles. To do this pass one of these values for `"PROFILE="`
---

#### Option 1: If you installed the device-specific prebuilt MCU+ SDK:

   ```bash
   make gen-buildfiles DEVICE=am64x MCU_PLUS_SDK_PATH=<MCU_PLUS_SDK_INSTALL_PATH>
   make gen-buildfiles DEVICE=am243x MCU_PLUS_SDK_PATH=<MCU_PLUS_SDK_INSTALL_PATH>
   make gen-buildfiles DEVICE=am261x MCU_PLUS_SDK_PATH=<MCU_PLUS_SDK_INSTALL_PATH>
   make gen-buildfiles DEVICE=am263px MCU_PLUS_SDK_PATH=<MCU_PLUS_SDK_INSTALL_PATH>
   make gen-buildfiles DEVICE=am263x MCU_PLUS_SDK_PATH=<MCU_PLUS_SDK_INSTALL_PATH>
   ```

#### Option 2: If you cloned the MCU+ SDK repository:

   ```bash
   make gen-buildfiles DEVICE=am64x MCU_PLUS_SDK_PATH=<MCU_PLUS_SDK_INSTALL_PATH> GEN_BUILDFILES_TARGET=development 
   make gen-buildfiles DEVICE=am243x MCU_PLUS_SDK_PATH=<MCU_PLUS_SDK_INSTALL_PATH> GEN_BUILDFILES_TARGET=development
   make gen-buildfiles DEVICE=am261x MCU_PLUS_SDK_PATH=<MCU_PLUS_SDK_INSTALL_PATH> GEN_BUILDFILES_TARGET=development
   make gen-buildfiles DEVICE=am263px MCU_PLUS_SDK_PATH=<MCU_PLUS_SDK_INSTALL_PATH> GEN_BUILDFILES_TARGET=development
   make gen-buildfiles DEVICE=am263x MCU_PLUS_SDK_PATH=<MCU_PLUS_SDK_INSTALL_PATH> GEN_BUILDFILES_TARGET=development
   ```