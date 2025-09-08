<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>
# Getting Started with OPEN PRU

</div>

[Supported HOST environments](#supported-host-environments)  
[Which core will initialize the PRU?](#which-core-will-initialize-the-pru)  
[Install dependencies (manual installation)](#install-dependencies-manual-installation)  
[Install dependencies (script)](#install-dependencies-script)  
[Set up imports.mak](#set-up-importsmak)  

## Introduction

This page discusses how to install the associated SDK and tools for your
development platform (Windows or Linux). Dependencies can be installed
manually, or with the pru_dependencies script (AM243x & AM64x only).

After the OPEN PRU repository has been set up, refer back to the
[README](./../README.md) for build steps.

## Supported HOST environments

- Validated on Windows 10 64bit & Windows 11 64bit. Higher versions may work
- Validated on Ubuntu 18.04 64bit & Ubuntu 22.04 64bit. Higher versions may work

## Which core will initialize the PRU?

In your final design, the PRU cores must be initialized by another processor
core. Depending on the processor, PRU cores can be initialized and controlled by
cores running RTOS, bare metal, or Linux.

TI supports initializing the PRU from an RTOS or bare metal core on:
- AM243x (R5F)
- AM261x (R5F)
- AM263px (R5F)
- AM263x (R5F)
- AM64x (R5F)

TI supports initializing the PRU from Linux on these processors:
- AM62x (A53)
- AM64x (A53)

TI supports RTOS & bare metal development through the MCU+ SDK, and Linux
development through the Linux SDK.

## Install dependencies (manual installation)

AM243x & AM64x customers who will use the MCU+ SDK alongside PRU can install
dependencies with the pru_dependencies script. For those steps, refer to
[Install dependencies (script)](#install-dependencies-script).

> [!NOTE]
> Different releases of the open-pru repo are compatible with different SDK
> versions. Please refer to the [release notes](./release_notes.md) for more
> information.

> [!NOTE]
> In general, software dependencies should all be installed under the same
> folder. So Windows developers would put all software tools under C:\ti, and
> Linux developers would put all software tools under ${HOME}/ti.

If you will initialize the PRU cores from an RTOS or bare metal core, follow the
steps in [Windows or Linux: Install the MCU+ SDK & tools](#windows-or-linux-install-the-mcu-sdk--tools).

If you will initialize the PRU cores from a Linux core, follow the steps in
[Linux: Install the Linux SDK & tools](#linux-install-the-linux-sdk--tools).

### Windows or Linux: Install the MCU+ SDK & tools

The MCU+ SDK is only a dependency when:
1) Building OPEN PRU projects that include code for an MCU+ core
2) Building OPEN PRU projects, where the PRU firmware includes code in the
   MCU+ SDK (such as macros)

#### Install the MCU+ SDK

Users can either install the prebuilt MCU+ SDK, or clone the MCU+ SDK
repository. If you are not sure, we suggest using the prebuilt MCU+ SDK:
   * The prebuilt SDK is packaged specifically for a single device, so it takes
     up less hard drive space than the MCU+ SDK repo
   * The CCS files in the OPEN PRU project need modification to work with the
     MCU+ SDK repository

**The prebuilt MCU+ SDK**
installer can be downloaded here:
   - [AM243x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM243X)
   - [AM261x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM261X)
   - [AM263Px MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM263PX)
   - [AM263x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM263X)
   - [AM64x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM64X)

**The MCU+ SDK repository**
can be cloned from
[here](https://github.com/TexasInstruments/mcupsdk-core). Follow the
mcupsdk-core README.md file for additional steps.

#### Install the tools

> [!NOTE]
> Prebuilt MCU+ SDK users:
> * Refer to the documentation associated with your specific SDK release
>
> MCU+ SDK repository users:
> * Refer to the documentation associated with the latest version of the MCU+ SDK

Install the versions of the tools that are listed in these pages:
   - MCU+ SDK docs > Getting Started > Download, Install, and Setup SDK and Tools
   - MCU+ SDK docs > Getting Started > Download, Install, and Setup CCS
     - Developers using a Linux computer, make sure to follow the additional steps at "CCS Linux Host Support"

After installing all dependencies, continue to section [Set up imports.mak](#set-up-importsmak).

### Linux: Install the Linux SDK & tools

The Linux SDK can only be installed on a Linux computer, not a Windows computer.

#### Install the Linux SDK

The Linux SDK is only a dependency if building code that will run on a Linux A53 core.

**The Linux SDK** installer can be downloaded here:
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

After installing all dependencies, continue to section [Set up imports.mak](#set-up-importsmak).

## Install dependencies (script)

The pru_dependencies scripts currently support installing MCU+ SDK and other
dependencies on **AM243x & AM64x**. For all other devices, or for steps when
installing the Linux SDK, please follow the steps at
[Install dependencies (manual installation)](#install-dependencies-manual-installation).

> [!NOTE]
> Different releases of the open-pru repo are compatible with different SDK
> versions. Please refer to the [release notes](./release_notes.md) for more
> information.

> [!NOTE]
> In general, software dependencies should all be installed under the same
> folder. So Windows developers would put all software tools under C:\ti, and
> Linux developers would put all software tools under ${HOME}/ti.

If the OPEN PRU repo is installed on a Windows machine, follow the steps in
[Windows: Use script `pru_dependencies.bat` to install SDK & tools](#windows-use-script-pru_dependenciesbat-to-install-sdk--tools).

If the OPEN PRU repo is installed on a Linux machine, follow the steps in
[Linux: Use script `pru_dependencies.sh` to install SDK & tools](#linux-use-script-pru_dependenciessh-to-install-sdk--tools).

### Windows: Use script `pru_dependencies.bat` to install SDK & tools

**NOTES**

   - If the script is executed from any folder but `C:\ti`, then the script will
     be copied to `C:\ti` after running. Then a second terminal screen will open
     with the location of the script. Please re-run script in the new terminal
     screen
   - If OpenSSL is needed to be installed, when prompted select the option to
     install binaries to /bin folder of installed path instead of the Windows
     system path
   - After installing dependencies, if the script is executed again in the same
     terminal to verify the installation, the script will show the same missing
     dependencies as the first run. This is a Windows limitation. Please close
     and re-open the terminal in order to get the updated state.

#### Install the MCU+ SDK

By default, the script checks for and installs MCU+ SDK 11.0. In order to use a
different SDK version, edit the **version numbers** in pru_dependencies.bat to
match the desired SDK version from:
   - [AM243x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM243X)
   - [AM64x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM64X)

Run the pru_dependencies script like this:
   - AM243x: `pru_dependencies.bat install --am243x_sdk`
   - AM64x: `pru_dependencies.bat install --am64xx_sdk`

Alternatively, the script can be used to clone the MCU+ SDK repository:
   - `pru_dependencies.bat install --clone_sdk`

#### Install the tools

By default, the script checks for and installs the tool versions used with
MCU+ SDK 11.0. If using a different SDK version, edit the **version numbers**
in pru_dependencies.bat to match the versions listed in **the MCU+ SDK docs**,
sections
   - Getting Started > Download, Install, and Setup SDK and Tools
   - Getting Started > Download, Install, and Setup CCS

   1. Verify the dependencies that are already installed:
       - `pru_dependencies.bat verify`

   2. Install dependencies:
       - `pru_dependencies.bat install [dependencies]`

For assistance on how to use the script, run the command `pru_dependencies.bat help`.

After installing all dependencies, continue to section [Set up imports.mak](#set-up-importsmak).

### Linux: Use script `pru_dependencies.sh` to install SDK & tools

**NOTES**

   - If the script is executed from any folder but `${HOME}/ti`, then the script
     will be copied to `${HOME}/ti`. The script will execute from there automatically

#### Install the MCU+ SDK

By default, the script checks for and installs MCU+ SDK 11.0. In order to use a
different SDK version, edit the **version numbers** in pru_dependencies.sh to
match the desired SDK version from:
   - [AM243x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM243X)
   - [AM64x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM64X)

Run the pru_dependencies script like this:
   - AM243x: `./pru_dependencies.sh install --am243x_sdk`
   - AM64x: `./pru_dependencies.sh install --am64xx_sdk`

Alternatively, the script can be used to clone the MCU+ SDK repository:
   - `./pru_dependencies.sh install --clone_sdk`

#### Install the tools

By default, the script checks for and installs the tool versions used with
MCU+ SDK 11.0. If using a different SDK version, edit the **version numbers**
in pru_dependencies.sh to match the versions listed in **the MCU+ SDK docs**,
sections
   - Getting Started > Download, Install, and Setup SDK and Tools
   - Getting Started > Download, Install, and Setup CCS

   1. Verify the dependencies that are already installed:
       - `./pru_dependencies.sh verify`

   2. Install dependencies:
       - `./pru_dependencies.sh install [dependencies]`

For assistance on how to use the script, run the command `./pru_dependencies.sh help`.

After installing all dependencies, continue to section [Set up imports.mak](#set-up-importsmak).

## Set up imports.mak

The imports.mak file contains the information that the OPEN PRU makefiles need
in order to build on your computer.

### Copy the default file

Copy `open-pru/imports.mak.default` into a new file, `open-pru/imports.mak`.

For ease of use, the new imports.mak file is already excluded from git tracking.

### Customize imports.mak for your computer

Open imports.mak and update the settings based on your specific computer. Follow
the `TODO` comments to see which settings need to be modified.
