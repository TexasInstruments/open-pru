# Empty Project

## Introduction

This example acts as a getting started point for PRU firmware development in assembly.

# Supported Combinations

 Parameter      | Value
 ---------------|-----------
 ICSSG          | ICSSG0 - PRU0, PRU1, RTU0, RTU1, TX_PRU0,TX_PRU1
 ICSSM          | ICSSM0 - PRU0, PRU1
                | ICSSM1 - PRU0, PRU1 (am261x only)
 Toolchain      | pru-cgt
 Board          | am64x-evm, am243x-evm, am243x-lp, am261x-lp, am261x-som, am263px-cc, am263px-lp, am263x-cc, am263x-lp
 Example folder | examples/pru_io/empty/

# Steps to Run the Example

> Prerequisite: [PRU-CGT-2-3](https://www.ti.com/tool/PRU-CGT) (ti-pru-cgt) should be installed at: `C:/ti/`

- **When using CCS projects to build**, import the CCS project from the above mentioned Example folder path for R5F and PRU, After this `main.asm`, `linker.cmd` files gets copied to ccs workspace of PRU project. The `main.asm` contains sample code to halt PRU program

     - Build the PRU project using the CCS project menu (see [for AM64x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/CCS_PROJECTS_PAGE.html), [for AM243x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/CCS_PROJECTS_PAGE.html), [for AM261x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM261X/latest/exports/docs/api_guide_am261x/CCS_PROJECTS_PAGE.html), [for AM263x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM263X/latest/exports/docs/api_guide_am263x/CCS_PROJECTS_PAGE.html), [for AM263Px](https://software-dl.ti.com/mcu-plus-sdk/esd/AM263PX/latest/exports/docs/api_guide_am263px/CCS_PROJECTS_PAGE.html)).
          - Build Flow: Once you click on build in PRU project, firmware header file which is generated in release or debug folder of ccs workspace, is moved to  `<open-pru/examples/pru_io/empty/firmware/device/>`

     - Build the R5F project using the CCS project menu (see [for AM64x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/CCS_PROJECTS_PAGE.html), [for AM243x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/CCS_PROJECTS_PAGE.html), [for AM261x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM261X/latest/exports/docs/api_guide_am261x/CCS_PROJECTS_PAGE.html), [for AM263x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM263X/latest/exports/docs/api_guide_am263x/CCS_PROJECTS_PAGE.html), [for AM263Px](https://software-dl.ti.com/mcu-plus-sdk/esd/AM263PX/latest/exports/docs/api_guide_am263px/CCS_PROJECTS_PAGE.html)).
          - Firmware header file path is included in R5F project include options by default, Instructions in Firmware header file can be written into PRU IRAM memory using PRUICSS_loadFirmware API (see [for AM64x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/group__DRV__PRUICSS__MODULE.html#ga3e7c763e5343fe98f7011f388a0b7ffe), [for AM243x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/group__DRV__PRUICSS__MODULE.html#ga3e7c763e5343fe98f7011f388a0b7ffe), [for AM261x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM261X/latest/exports/docs/api_guide_am261x/group__DRV__PRUICSS__MODULE.html#ga3e7c763e5343fe98f7011f388a0b7ffe), [for AM263x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM263X/latest/exports/docs/api_guide_am263x/group__DRV__PRUICSS__MODULE.html#ga3e7c763e5343fe98f7011f388a0b7ffe), [for AM263Px](https://software-dl.ti.com/mcu-plus-sdk/esd/AM263PX/latest/exports/docs/api_guide_am263px/group__DRV__PRUICSS__MODULE.html#ga3e7c763e5343fe98f7011f388a0b7ffe))
          - Build Flow: Once you click on build in R5F project, SysConfig files are generated, Finally the R5F project will be generated using both the generated SysConfig and PRU project binaries.

     - Launch a CCS debug session and run the executable, (see [for AM64x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM64X/latest/exports/docs/api_guide_am64x/CCS_LAUNCH_PAGE.html), [for AM243x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM243X/latest/exports/docs/api_guide_am243x/CCS_LAUNCH_PAGE.html), [for AM261x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM261X/latest/exports/docs/api_guide_am261x/CCS_LAUNCH_PAGE.html), [for AM263x](https://software-dl.ti.com/mcu-plus-sdk/esd/AM263X/latest/exports/docs/api_guide_am263x/CCS_LAUNCH_PAGE.html), [for AM263Px](https://software-dl.ti.com/mcu-plus-sdk/esd/AM263PX/latest/exports/docs/api_guide_am263px/CCS_LAUNCH_PAGE.html))

- **When using makefiles to build**, make sure to build the PRU firmware before attempting to build an example. For example,
   to build a PRU firmware for empty example am243x-lp, run the following:
     -    update MCU_PLUS_SDK_PATH in `<open-pru/makefile>` and `<open-pru/imports.mak>` 

     -    ```bash
          make -s -C examples/empty/firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt     all 
          make -s -C examples/empty/firmware/am243x-lp/icss_g0_pru1_fw/ti-pru-cgt     all 
          make -s -C examples/empty/firmware/am243x-lp/icss_g0_rtu_pru0_fw/ti-pru-cgt all
          make -s -C examples/empty/firmware/am243x-lp/icss_g0_rtu_pru1_fw/ti-pru-cgt all
          make -s -C examples/empty/firmware/am243x-lp/icss_g0_tx_pru0_fw/ti-pru-cgt  all
          make -s -C examples/empty/firmware/am243x-lp/icss_g0_tx_pru1_fw/ti-pru-cgt  all
          ```
     - Once the PRU Firmware build is complete, to build the example run:
          ```bash
          make -s -C examples/empty/am243x-lp/r5fss0-0_freertos/ti-arm-clang all PROFILE=debug
          ```

# Writing PRU code

* You can modify this example to write your own firmware. [Examples folder](../examples) contains some PRU projects which can act as reference. PRU sources are usually separated out in a `firmware/` subfolder in all these example project folders.
* You can change the core in project properties to develop firmware for other PRU cores.
