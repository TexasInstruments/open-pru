<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>

# Getting Started with OPEN PRU

</div>

[Supported HOST environments](#supported-host-environments)  
[Which core will initialize the PRU?](#which-core-will-initialize-the-pru)  
[Install dependencies](#install-dependencies)  
[Set up imports.mak](#set-up-importsmak)  

## Introduction

The "getting started" pages discuss how to install the associated SDK and tools
for your development platform (Windows or Linux).

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

## Install dependencies

> [!NOTE]
> Different releases of the open-pru repo are compatible with different SDK
> versions. Please refer to the [release notes](./release_notes.md) for more
> information.

> [!NOTE]
> In general, software dependencies should all be installed under the same
> folder. So Windows developers would put all software tools under C:\ti, and
> Linux developers would put all software tools under ${HOME}/ti.

If you will initialize the PRU cores from an RTOS or bare metal core, follow the
steps in [Getting Started with MCU+ SDK](./getting_started_mcuplus.md).

If you will initialize the PRU cores from a Linux core, follow the steps in
[Getting Started with Linux SDK](./getting_started_linux.md).

## Set up imports.mak

The imports.mak file contains the information that the OPEN PRU makefiles need
in order to build on your computer.

### Copy the default file

Copy `open-pru/imports.mak.default` into a new file, `open-pru/imports.mak`.

For ease of use, the new imports.mak file is already excluded from git tracking.

### Customize imports.mak for your computer

Open imports.mak and update the settings based on your specific computer. Follow
the `TODO` comments to see which settings need to be modified.

## Next steps

After the OPEN PRU repository has been set up, refer back to the
[README](./../README.md) for build steps.