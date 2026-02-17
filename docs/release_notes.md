<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>

# Release Notes

</div>

[Checking out a specific release](#checking-out-a-specific-release)  
[Compatible versions of CCS & other tools](#compatible-versions-of-ccs--other-tools)  
[Release Notes](#release-notes)  
[v2026.00.00](#v20260000)  
[v2025.00.00](#v20250000)  

The open-pru repo does not have official software releases like the TI SDKs.

Instead, we will regularly update the open-pru repo. When the repo has
been significantly changed, we will add a new tag to the main branch.

## Checking out a specific release

To base your development on a specific tag (instead of the tip of the main
branch), checkout the tag like
```
$ git checkout -b my-branch-name tag-name
```

## Compatible versions of CCS & other tools

**MCU+ SDK users:**
Use the CCS version, SysConfig version, and other tool versions
listed in the MCU+ SDK "Getting Started" docs. See
[Getting Started with MCU+ SDK](./getting_started_mcuplus.md) for details.

**Linux SDK users:**
Use the CCS and tool versions listed in
[Getting Started with Linux SDK](./getting_started_linux.md) when the open-pru
repo is checked out to the specific release tag that you are using.

## Release Notes

### v2026.00.00

#### Major Updates

* Add support for building AM62x firmware to the OpenPRU repo
  * Add all infrastructure, add AM62x to Getting Started Labs, empty, empty_c
* Add support for AM62x & AM64x Linux RPMsg
* Add support for AM243x & AM64x MCU+ SDK 11.1 & 11.2 [1]

#### Supported Processors

AM243x, AM261x, AM263Px, AM263x, AM62x, AM64x

#### Compatible SDKs

These SDK release versions can be used to build OpenPRU projects with a specific
tag. The OpenPRU projects may require modifications before they can be built
with older SDK versions. For more information, refer to
[Using Older SDKs with OpenPRU](./using_older_sdks_with_open_pru.md).

| SDK       | am243x      | am261x    | am263px   | am263x    | am62x | am64x       |
| --------- | ----------- | --------- | --------- | --------- | ----- | ----------- |
| MCU+ SDK  | 11.1 - 11.2 | 10.2 only | 10.2 only | 10.2 only | N/A   | 11.1 - 11.2 |
| Linux SDK | N/A         | N/A       | N/A       | N/A       | 11.x  | 11.x        |

#### Academies

[PRU Academy for AM243x](https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AB.mSUi9ihL.a5hIt1grfw__AM24X-ACADEMY__ZPSnq-h__LATEST)  
[PRU Academy for AM64x](https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AB.mSUi9ihL.a5hIt1grfw__AM64-ACADEMY__WI1KRXP__LATEST)  

#### Additional Updates & bugfixes

* Bugfix: release notes > compatible SDKs > v2025.00.00
  * AM26x projects ONLY support MCU+ 10.2 in both v2025.00.00 & v2026.00.00

* Add libraries for PRU to communicate with Linux through the RPMsg
  Inter-Processor Communication (IPC) protocol
  * source/rpmsg

* Add examples/rpmsg_echo_linux
  * RPMsg example between Linux <--> PRU
  * supports AM62x & AM64x

* Add examples/fir
  * Implement 64-tap Finite Impulse Respose (FIR) filter on PRU
  * supports AM243x

* Add examples/multicore_scheduler
  * Use IEP timer + Task Manager to schedule tasks across all cores
  * supports AM243x

* Add github workflows to validate OpenPRU make infrastructure

* Bugfix: AM26x projects
  * remove unneeded makefile includes
  * remove empty example.syscfg files

[1]
* Adding support for AM243x & AM64x MCU+ SDK 11.1 & 11.2 breaks backwards
  compatibility with previous AM243x & AM64x MCU+ SDK releases. For more
  information on building an OpenPRU project with an earlier version of MCU+
  SDK, refer to
  https://github.com/TexasInstruments/open-pru/blob/main/docs/using_older_sdks_with_open_pru.md

* This version of OpenPRU does NOT update the AM26x MCU+ build infrastructure.
  Both v2025.00.00 and v2026.00.00 are only compatible with AM26x MCU+ SDK
  10.2. For more information about building an AM26x OpenPRU project with an
  earlier version of MCU+ SDK, refer to
  https://github.com/TexasInstruments/open-pru/blob/main/docs/using_older_sdks_with_open_pru.md

### v2025.00.00

#### Supported Processors

AM243x, AM261x, AM263Px, AM263x, AM64x

#### Compatible SDKs

These SDK release versions can be used to build OpenPRU projects with a specific
tag. The OpenPRU projects may require modifications before they can be built
with older SDK versions. For more information, refer to
[Using Older SDKs with OpenPRU](./using_older_sdks_with_open_pru.md).

| SDK      | am243x      | am261x    | am263px   | am263x    | am62x | am64x       |
| -------- | ----------- | --------- | --------- | --------- | ----- | ----------- |
| MCU+ SDK | 10.1 - 11.0 | 10.2 only | 10.2 only | 10.2 only | N/A   | 10.1 - 11.0 |

#### Academies

[PRU Academy for AM243x](https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AB.mSUi9ihL.a5hIt1grfw__AM24X-ACADEMY__ZPSnq-h__LATEST)  
[PRU Academy for AM64x](https://dev.ti.com/tirex/explore/node?isTheia=false&node=A__AB.mSUi9ihL.a5hIt1grfw__AM64-ACADEMY__WI1KRXP__LATEST)  

#### What is included in the initial release?

ACADEMY:
* Getting Started Labs: AM243x, AM261x, AM263Px, AM263x, AM64x
* crc/crc: AM261x, AM263Px, AM263x
* gpio/gpio_toggle: AM261x, AM263Px, AM263x
* intc/intc_mcu: AM261x, AM263Px, AM263x
* mac/mac: AM261x, AM263Px, AM263x
* mac/mac_multiply: AM261x, AM263Px, AM263x

EXAMPLES:
* custom_frequency_generator: AM243x
* empty: AM243x, AM261x, AM263Px, AM263x, AM64x
* empty_c: AM243x, AM261x, AM263Px, AM263x, AM64x
* fft/
    split_radix_fft_f4_single_core: AM243x, AM261x
    split_radix_fft_post_processing: AM243x
* LCD_interface: AM243x
* logic_scope: AM243x
* pru_emif: AM243x
* pru_i2s_diagnostic: AM261x, AM263x
* spi_loopback: AM243x

SOURCE:
* include/c_code: Header files for AM243x, AM62x, AM64x
* linker_cmd/asm_code: linker.cmd for AM243x, AM261x, AM263Px, AM263x, AM62x, AM64x
* linker_cmd/c_code: linker.cmd for AM243x, AM261x, AM263Px, AM263x, AM62x, AM64x
