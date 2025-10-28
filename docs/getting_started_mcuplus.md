<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>

# Getting Started with MCU+ SDK

</div>

[Install the MCU+ SDK](#install-the-mcu-sdk)  
[Install the tools](#install-the-tools)  
[Set up imports.mak](#set-up-importsmak)  

The MCU+ SDK is only a dependency when:
1) Building OpenPRU projects that include code for an MCU+ core
2) Building OpenPRU projects, where the PRU firmware includes code in the
   MCU+ SDK (such as macros)

## Install the MCU+ SDK

Users can either install the prebuilt MCU+ SDK, or clone the MCU+ SDK
repository. In general, we suggest using the prebuilt MCU+ SDK:
   * The prebuilt SDK is packaged specifically for a single device, so it takes
     up less hard drive space than the MCU+ SDK repo
   * The CCS files in the OpenPRU project need modification to work with the
     MCU+ SDK repository

**The prebuilt MCU+ SDK**
installer can be downloaded here:
   - [AM243x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM243X)
   - [AM261x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM261X)
   - [AM263Px MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM263PX)
   - [AM263x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM263X)
   - [AM64x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM64X)

If **the MCU+ SDK repository** is used, please follow the steps on page
[Getting Started with the MCU+ SDK Repo](./getting_started_mcuplus_repo.md)

## Install the tools

> [!NOTE]
> Refer to the documentation associated with your specific SDK release

Install the versions of the tools that are listed in these pages:

   - MCU+ SDK docs > Getting Started > Download, Install, and Setup SDK and Tools

   - MCU+ SDK docs > Getting Started > Download, Install, and Setup CCS

     - Developers using a Linux computer, make sure to follow the additional
       steps at "CCS Linux Host Support"

     - If section **Create Target Configuration** says to “Bypass not used CPUs”,
       **do NOT bypass the ICSS / ICSS_G cores**. The CCS debugger cannot connect
       to a PRU core if the PRU core has been bypassed

## Set up imports.mak

The imports.mak file contains the information that the OpenPRU makefiles need
in order to build on your computer.

### Copy the default file

Copy `open-pru/imports.mak.default` into a new file, `open-pru/imports.mak`.

For ease of use, the new imports.mak file is already excluded from git tracking.

### Customize imports.mak for your computer

Open imports.mak and update the settings based on your specific computer. Follow
the `UPDATE` comments to see which settings need to be modified.

## Next steps

After the OpenPRU repository has been set up, refer back to the
[README](./../README.md) for build steps.
