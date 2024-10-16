# Empty Project

## Introduction

This example acts as a getting started point for PRU firmware development in assembly. It is a demo that shows how to access local memory locations of PRU.

# Supported Combinations

 Parameter      | Value
 ---------------|-----------
 ICSSG          | ICSSG0 - PRU0, PRU1, RTU0, RTU1, TX_PRU0,TX_PRU1
 Toolchain      | pru-cgt
 Board          | am64x-evm, am243x-evm, am243x-lp
 Example folder | examples/pru_io/empty/

# Steps to Run the Example

- **When using CCS projects to build**, import the CCS project from the above mentioned Example folder path for R5F and PRU, After this `main.asm`, `linker.cmd` files gets copied to ccs workspace of PRU project. The `main.asm` contains sample code for accessing Local PRU memories available: DMEM0 (64kB), DMEM1 (64kB) and Shared Ram (64kB).

- Build the PRU project using the CCS project menu (see [for AM64x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/CCS_PROJECTS_PAGE.html), [for AM243x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/CCS_PROJECTS_PAGE.html)).
     - Build Flow: Once you click on build in PRU project, firmware header file which is generated in release or debug folder of ccs workspace, is moved to  `<sdk-install-dir/examples/pru_io/empty/firmware/device/>`

- Build the R5F project using the CCS project menu (see [for AM64x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/CCS_PROJECTS_PAGE.html), [for AM243x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/CCS_PROJECTS_PAGE.html)).
     - Firmware header file path is included in R5F project include options by default, Instructions in Firmware header file can be written into PRU IRAM memory using PRUICSS_loadFirmware API (see [for AM64x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/group__DRV__PRUICSS__MODULE.html#ga3e7c763e5343fe98f7011f388a0b7ffe), [for AM243x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/group__DRV__PRUICSS__MODULE.html#ga3e7c763e5343fe98f7011f388a0b7ffe))
     - Build Flow: Once you click on build in R5F project, SysConfig files are generated, Finally the R5F project will be generated using both the generated SysConfig and PRU project binaries.

> Prerequisite: [PRU-CGT-2-3](https://www.ti.com/tool/PRU-CGT) (ti-pru-cgt) should be installed at: `C:/ti/`

- Launch a CCS debug session and run the executable, (see [for AM64x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/CCS_LAUNCH_PAGE.html), [for AM243x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/CCS_LAUNCH_PAGE.html))


# Writing PRU code

* You can modify this example to write your own firmware. [Examples folder](../examples) contains some PRU projects which can act as reference. PRU sources are usually separated out in a `firmware/` subfolder in all these example project folders.
* You can change the core in project properties to develop firmware for other PRU cores.
