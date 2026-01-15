# Using Older SDKs with OpenPRU

As TI releases new SDKs, the OpenPRU projects will be updated so that the
projects can be built with the latest SDK releases. Refer to the
[Release Notes](./release_notes.md) for a list of **Compatible SDKs** that can
build OpenPRU projects without changes.

Sometimes, backwards compatibility breaks between SDK releases. You
can still use older SDKs with the OpenPRU code, but some modifications may be
required.

> [!NOTE]
> Older SDK releases may not have been tested with OpenPRU.
> Additional modifications may be required.

## Modifications to use older SDK versions

There are several different kinds of modifications that may be required to use
open-pru with an older SDK version. We will use a
number to represent each kind of modification.

<details>
  <summary>AM243x</summary>

  | AM243X SDK      | Mandatory modifications | Optional modifications |
  | --------------- | ----------------------- | ---------------------- |
  | MCU+ 8.6 - 9.2  | [1] [2]                 | [4]                    |
  | MCU+ 10.0       | [1]                     | [4]                    |

</details>

<details>
  <summary>AM261x</summary>

  | AM261x SDK      | Mandatory modifications | Optional modifications |
  | --------------- | ----------------------- | ---------------------- |
  | MCU+ SDK 10.0.1 | Either [2] or [3]       |                        |

</details>

<details>
  <summary>AM263Px</summary>

  | AM263Px SDK   | Mandatory modifications | Optional modifications |
  | ------------- | ----------------------- | ---------------------- |
  | MCU+ SDK 10.0 | [1]. Either [2] or [3]  | [4]                    |
  | MCU+ SDK 10.1 | Either [2] or [3]       |                        |

</details>

<details>
  <summary>AM263x</summary>

  | AM263x SDK      | Mandatory modifications  | Optional modifications |
  | --------------- | ------------------------ | ---------------------- |
  | MCU+ 9.2        | [1]. Either [2] or [3]   | [4]                    |
  | MCU+ 10.0, 10.1 | Either [2] or [3]        |                        |

</details>

<details>
  <summary>AM62x</summary>

  | SDK             | Mandatory modifications | Optional modifications |
  | --------------- | ----------------------- | ---------------------- |
  | Linux 9.0-10.1  |                         | [5]                    |
  | Linux 11.0      |                         | [6]                    |

</details>

<details>
  <summary>AM64x</summary>

  | SDK             | Mandatory modifications | Optional modifications |
  | --------------- | ----------------------- | ---------------------- |
  | MCU+ 8.6 - 9.2  | [1] [2]                 | [4]                    |
  | MCU+ 10.0       | [1]                     | [4]                    |
  | Linux 9.0-10.1  |                         | [5]                    |
  | Linux 11.0      |                         | [6]                    |

</details>

[1. Update SysConfig version](#1-update-sysconfig-version). Affects: PRU code

[2. Use MCU+ code from the SDK](#2-use-mcu-code-from-the-sdk). Affects: MCU+ cores

[3. Modify max-segment-size](#3-modify-max-segment-size). Affects: AM26x MCU+ cores

[4. Update MCU+ source/pru_io/firmware/common code](#4-update-mcu-sourcepru_iofirmwarecommon-code). Affects: PRU code

[5. Revert RPMsg channel name from rpmsg-raw to rpmsg-pru](#5-revert-rpmsg-channel-name-from-rpmsg-raw-to-rpmsg-pru). Affects: PRU firmware w/ RPMsg code

[6. Apply patch to enable PRU RPMsg](#6-apply-patch-to-enable-pru-rpmsg). Affects: Linux RPMsg kernel driver

## 1. Update SysConfig version

Affects: PRU code

By default, the OpenPRU project enforces a minimum SysConfig version of 1.21.0.
This allows for building projects that modify the SysConfig infrastructure,
like examples/pru_i2c_diagnostic.

This minimum SysConfig version does not matter if the OpenPRU project does not
do anything custom to the SysConfig source code.

Update the minimum SysConfig version from 1.21.0 to the SysConfig version used
by your MCU+ SDK version in these files:  
.metadata/.tirex/package.tirex.json  
.metadata/product.json

## 2. Use MCU+ code from the SDK

Affects: MCU+ cores

Sometimes, the MCU+ infrastructure changes significantly between SDK releases.
In this case, it is probably easier to copy over an MCU+ project from the MCU+
SDK and modify that project, instead of using the MCU+ projects that are already
in the OpenPRU repo.

For more information about adding MCU+ code to an OpenPRU project, refer to
**the PRU Academy > Getting Started Labs > Lab 1: How to Create a PRU Project >
Creating an OpenPRU Project with MCU+ Code**

### Example: MCU+ SDK 9.1 Memory Configurator Tool

Starting in AM243x & AM64x MCU+ SDK 9.1, the MCU+ projects switched to using the
SysConfig Memory configurator tool. Attempting to build unmodified OpenPRU
projects with an AM243x/AM64x SDK from before 9.1 will lead to build errors like this:

```
Generating SysConfig files ...
Running script...
Error: Exception occurred calling scripting.addModule(): No such resource: /memory_configurator/default_linker.syscfg.js
```

## 3. Modify max-segment-size

Affects: AM26x MCU+ cores

For AM26x devices (AM261x, AM263Px, AM263x), MCU+ SDK 10.1 and earlier used flag
`--max_segment_size` in the R5F makefiles. However, MCU+ SDK 10.2 and later
change the flag to use hyphens: `--max-segment-size`.

In order to modify existing MCU+ projects in the open-pru repo to build with
older versions of MCU+ SDK,
modify the flag to use underscores. This is the commit where we originally
updated the flag and broke backwards compatibility:  
https://github.com/TexasInstruments/open-pru/commit/77c5c6eca5f4357a6874a3c6a9b03414a9b6ebc5

## 4. Update MCU+ source/pru_io/firmware/common code

Affects: PRU code

There have been bugfixes to the macros in MCU+ sdk source/pru_io/firmware/common
over the last few years. If your project has a dependency on any macros in this
directory, you may want to use the latest version of those files:  
https://github.com/TexasInstruments/mcupsdk-core/commits/next/source/pru_io/firmware/common

## 5. Revert RPMsg channel name from rpmsg-raw to rpmsg-pru

Starting in Linux kernel 6.12, we removed the separate PRU RPMsg (RPMessage)
driver[1], and use the generic RPMsg character driver for communication with PRU
instead [2].

[1] linux-x.y.z/drivers/rpmsg/rpmsg_pru.c
[2] linux-x.y.z/drivers/rpmsg/rpmsg_char.c

The generic Linux RPMsg character driver uses a different channel name
(rpmsg-raw) than the previous PRU RPMsg driver (rpmsg-pru). In order to get
PRU RPMsg firmware to work with Linux kernel 6.1 or 6.6, revert the PRU RPMsg
firmware from using rpmsg-raw to use rpmsg-pru instead.

In other words, revert this patch from the earlier PRU Software Support Package:
[Linux kernel 6.12: update rpmsg-pru to rpmsg-raw](https://git.ti.com/cgit/pru-software-support-package/pru-software-support-package/commit/?id=e4127728b027f42deb0b88dfdf8b3483cba45e85)

## 6. Apply patch to enable PRU RPMsg

Linux SDK 11.0 (Linux kernel 6.12) requires a Linux kernel patch in order to
enable PRU RPMsg. The patch is not required if using SDK 10.1 or earlier, or
SDK 11.1 or later.

For more details, refer to
[[FAQ] AM62x & AM64x: How to enable PRU RPMsg on Processor SDK Linux 11.0](https://e2e.ti.com/support/processors-group/processors/f/processors-forum/1494495/faq-am62x-am64x-how-to-enable-pru-rpmsg-on-processor-sdk-linux-11-0).