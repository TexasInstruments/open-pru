# PRU Subsystem Features Comparison

*Catalog Processors*

#### ABSTRACT

This application report documents the feature differences between the PRU Subsystems available on different TI processors.

# Table of Contents

| 1 Introduction |  |
|---|--|
| 1.1 PRU-ICSS: The Programmable Real-time Unit and Industrial Communication Subsystem |  |
| 1.2 PRU_ICSSG: The Programmable Real-time Unit and Industrial Communication Subsystem - Gigabit |  |
| 1.3 PRUSS: The Programmable Real-time Unit Subsystem |  |
| 1.4 PRU Subsystem Feature Comparison |  |
| 2 PRU-ICSS Feature Comparison |  |
| 3 PRU_ICSSG Feature Comparison |  |
| 4 PRUSS Features |  |
| 5 References |  |
| 6 Revision History |  |
|  |  |
| List of Tables |  |
| Table 1-1. PRU Subsystem Feature Comparison |  |
| Table 2-1. PRU-ICSS Feature Comparison |  |
| Table 3-1. PRU_ICSSG Feature Comparison Across Devices |  |
| Table 4-1. PRUSS Features |  |

## Trademarks

All trademarks are the property of their respective owners.

## 1 Introduction

The Programmable Real-time Unit (PRU) is a small processor core that is tightly integrated with an IO subsystem, offering low-latency control of IO pins. The TI Sitara family of devices offer three flavors of PRU Subsystem.

## 1.1 PRU-ICSS: The Programmable Real-time Unit and Industrial Communication Subsystem

The Programmable Real-time Unit and Industrial Communication Subsystem (PRU-ICSS) consists of dual 32 bit RISC cores (the PRUs), shared data, instruction memories, internal peripheral modules, and an interrupt controller (INTC). The programmable nature of the PRU, along with its access to pins (IOs), events and all System-on-Chip (SoC) resources, provides flexibility in implementing fast real-time responses, specialized data handling operations, custom peripheral interfaces, and in off-loading tasks from the other processor cores of the SoC.

Devices offering the PRU-ICSS capability include AM263x, AM335x, AM437x, AM57x and K2G.

## 1.2 PRU\_ICSSG: The Programmable Real-time Unit and Industrial Communication Subsystem - Gigabit

The Programmable Real-time Unit and Industrial Communication Subsystem - Gigabit (PRU\_ICSSG) can be considered a superset of the PRU-ICSS. In addition to all PRU-ICSS features, the PRU\_ICSSG adds two Auxiliary Programmable Real-Time Unit (RTU) cores, two Transmit PRU (TX\_PRU) cores, broadside memories, improved event management with the task manager, data processing and data movement accelerators, and new peripherals such as PWM.

Devices offering the PRU\_ICSSG capability include AM65x, AM64x and AM243x.

## 1.3 PRUSS: The Programmable Real-time Unit Subsystem

The Programmable Real-time Unit Subsystem (PRUSS) consists of dual 32-bit RISC cores (the PRUs), shared data, instruction memories, internal peripheral modules, and an interrupt controller (INTC). The programmable nature of the PRU cores, along with their access to pins, events and all device resources, provides flexibility in implementing fast real-time responses, specialized data handling operations, custom peripheral interfaces, and in offloading tasks from the other processor cores of the device.

Industrial Communication Subsystem features including Ethernet (MII signals and MDIO signals are not pinned out) are not supported.

Devices offering the PRUSS capability include AM62x.

## 1.4 PRU Subsystem Feature Comparison

Table 1-1 shows a high-level feature of the PRU subsystems.. The subsequent sections show the feature differences between each device that supports the given subsystem flavor. For a more detailed comparison, see the *[PRU-ICSS/PRU\\_ICSSG Migration Guide](https://www.ti.com/lit/pdf/SPRACJ8)*.

**Table 1-1. PRU Subsystem Feature Comparison**

| Category | Feature | PRUSS | PRU-ICSS | PRU_ICSSG |
|---|---|---|---|---|
| General PRU Specifications | PRU cores | Yes | Yes | Yes |
| General PRU Specifications | RTU_PRU (Auxiliary PRU) cores | No | No | Yes |
| General PRU Specifications | TX PRU (Transmit PRU) cores | No | No | Yes |
| General PRU Specifications | IRAM (per PRU / RTU_PRU / TX_PRU core) | Yes | Yes | Yes |
| General PRU Specifications | DRAM (2 DRAMs per PRU-ICSS/PRU_ICSSG) | Yes | Yes | Yes |
| General PRU Specifications | Shared DRAM | Yes | Yes | Yes |
| General PRU Specifications | INTC | Yes | Yes | Yes |
| General Purpose Inputs | Direct Input | Yes | Yes | Yes |
| General Purpose Inputs | 16-bit Parallel Capture | Yes | Yes | Yes |
| General Purpose Inputs | 28-bit Shift | Yes | Yes | Yes |
| General Purpose Inputs | 3 Ch. Peripheral Interface (EnDAT) | No | Device dependent | Yes |
| General Purpose Inputs | 9 Ch. Sigma Delta | No | Device dependent | Yes |
| General Purpose Outputs | Direct Output | Yes | Yes | Yes |
| General Purpose Outputs | Shift out | Yes | Yes | Yes |
| Accelerators: Data Processing | MPY/MAC | Yes | Yes | Yes |
| Accelerators: Data Processing | CRC 16/32 | Yes | Device dependent | Yes |
| Accelerators: Data Processing | Scratch Pad | Yes | Yes | Yes |
| Accelerators: Data Processing | IPC Scratch Pad | No | No | Yes |
| Accelerators: Data Processing | Broadside RAM | No | No | Yes |
| Accelerators: Data Processing | BSWAP | No | No | Yes |
| Accelerators: Data Processing | SUM32 | No | No | Yes |
| Accelerators: Data Processing | Task Manager | No | No | Yes |
| Accelerators: Data Processing | Spinlock | No | No | Yes |
| Accelerators: Data Processing | Filter Data Base (FDB) | No | No | Yes |
| Accelerators: Data Movement | XFR2VBUS | Yes | No | Yes |
| Accelerators: Data Movement | PSI TX & RX | No | No | Yes |
| Accelerators: Data Movement | XFR2TR | No | No | Yes |
| Peripherals | UART | Yes | Yes | Yes |
| Peripherals | eCAP | Yes | Yes | Yes |
| Peripherals | IEP | Yes | Yes | Yes |
| Peripherals | MII_RT or MII_G_RT | No | Yes (MII) | Yes (MII/RGMII) |
| Peripherals | MDIO | No | Yes | Yes |
| Peripherals | SGMII | No | No | Device Dependent |
| Peripherals | PWM | No | No | Yes |

# 2 PRU-ICSS Feature Comparison

**Table 2-1. PRU-ICSS Feature Comparison**

| Category | Feature | AM335x (PRU-ICSS1) | AM437x (PRU-ICSS1) | AM437x (PRU-ICSS0) | AM570x (2x PRU-ICSS) | AM571x (2x PRU-ICSS) | AM572x (2x PRU-ICSS) | AM574x (2x PRU-ICSS) | K2G (2x PRU-ICSS) | AM263x (1x PRU-ICSS) |
|---|---|---|---|---|---|---|---|---|---|---|
| General Specifications | Number of PRU cores | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 2 |
| General Specifications | Max Frequency | 200 MHz | 225 MHz <sup>(2)</sup> | 225 MHz <sup>(2)</sup> | 200 MHz | 200 MHz | 200 MHz | 200 MHz | 200 MHz | 200 MHz |
| General Specifications | IRAM size (per PRU core) | 8 KB | 12 KB | 4 KB | 12 KB | 12 KB | 12 KB | 12 KB | 16 KB | 12 KB |
| General Specifications | DRAM size (2 DRAMs per PRU-ICSS) | 8 KB | 8 KB | 4 KB | 8 KB | 8 KB | 8 KB | 8 KB | 8 KB | 8 KB |
| General Specifications | Shared DRAM size | 12 KB | 32 KB | 0 KB | 32 KB | 32 KB | 32 KB | 32 KB | 64 KB w/ ECC | 32 KB |
| General Specifications | General Purpose Input (per PRU core) | Direct; or 16-bit parallel capture; or 28-bit shift | Direct; or 16-bit parallel capture; or 28-bit shift; or 3 ch EnDat 2.2; or 9 ch Sigma Delta | Direct; or 16-bit parallel capture; or 28-bit shift; or 3 ch EnDat 2.2; or 9 ch Sigma Delta | Direct; or 16-bit parallel capture; or 28-bit shift; or 3 ch EnDat 2.2; or 9 ch Sigma Delta | Direct; or 16-bit parallel capture; or 28-bit shift; or 3 ch EnDat 2.2; or 9 ch Sigma Delta | Direct; or 16-bit parallel capture; or 28-bit shift | Direct; or 16-bit parallel capture; or 28-bit shift; or 3 ch EnDat 2.2; or 9 ch Sigma Delta | Direct; or 16-bit parallel capture; or 28-bit shift; or 3 ch EnDat 2.2; or 9 ch Sigma Delta | Direct; or 16-bit parallel capture; or 28-bit shift; or 3 ch EnDat 2.2; or 9 ch Sigma Delta |
| General Specifications | General Purpose Output (per PRU core) | Direct; or Shift out | Direct; or Shift out | Direct; or Shift out | Direct; or Shift out | Direct; or Shift out | Direct; or Shift out | Direct; or Shift out | Direct; or Shift out | Direct; or Shift out |
| General Specifications | GPI Pins (PRU0, PRU1) | 17, 17 | 13, 0 | 20, 20 | 0/21 <sup>(3)</sup>, 21/17 | 0/21 <sup>(3)</sup>, 21/21 | 21, 21 | 21, 21 | 20, 20 | 17, 20 |
| General Specifications | GPO Pins (PRU0, PRU1) | 16, 16 | 12, 0 | 20, 20 | 0/21 <sup>(3)</sup>, 21/17 | 0/21 <sup>(3)</sup>, 21/21 | 21, 21 | 21, 21 | 20, 20 | 17, 20 |
| General Specifications | MPY/MAC | Y | Y | Y | Y | Y | Y | Y | Y | Y |
| General Specifications | Scratchpad | Y (3 banks) | Y (3 banks) | N | Y (3 banks) | Y (3 banks) | Y (3 banks) | Y (3 banks) | Y (3 banks) | Y (3 banks) |
| General Specifications | CRC16/32 | 0 | 2 | 2 | 2 | 2 | 2 <sup>(4)</sup> | 2 | 2 | 2 |
| General Specifications | INTC | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| Peripherals | UART | 1 | 1 | 1 | 1 / not pinned out <sup>(5)</sup> | 1 | 1 | 1 | 1 | 1 |
| Peripherals | eCAP | 1 | 1 | not pinned out | 1 / not pinned out <sup>(5)</sup> | 1 | 1 | 1 | 1 | 1 |
| Peripherals | IEP | 1 | 1 | not pinned out | 1 / not pinned out <sup>(5)</sup> | 1 | 1 | 1 | 1 | 1 |
| Peripherals | MII_RT | 2 | 2 | not pinned out | 2 | 2 | 2 | 2 | 2 | 2 |
| Peripherals | MDIO | 1 | 1 | not pinned out | 1 | 1 | 1 | 1 | 1 | 1 |

<sup>(1)</sup> The name PRU-ICSS and PRUSS are used interchangeably throughout the AM57xx and K2G documentation to describe the Programmable Real-Time Unit (PRU) and Industrial Communication Subsystem.

<sup>(2)</sup> The default frequency for AM437x is 200 MHz. However, the max frequency 225 MHz is achievable through display PLL CLKOUT. For DSS limitations when configuring this PLL for frequencies >200 MHz, see the *[AM437x Sitara Processors Technical Reference Manual](https://www.ti.com/lit/pdf/SPRUHL7)*.

<sup>(3)</sup> AM571x and AM570x PRU-ICSS1 does not pin out the PRU0 core GPIs/GPOs. The other AM571x and AM570x PRU cores (PRU-ICSS1 PRU1, PRU-ICSS2 PRU0, PRU-ICSS2 PRU1) each pin out the number of GPIs/GPOs is listed in Table 2-1.

<sup>(4)</sup> AM572x SR1.1 does not have CRC16/32. Within the AM57x family, this feature is only available in AM572x SR2.0, AM571x, and AM570x.

<sup>(5)</sup> AM570x PRU-ICSS2 does not pin out these sub-modules. However, they are pinned out on the other AM570x subsystem (PRU-ICSS1).

# 3 PRU\_ICSSG Feature Comparison

Table 3-1 summarizes the PRU\_ICSSG features.

**Table 3-1. PRU\_ICSSG Feature Comparison Across Devices**

| Category | Feature | AM65x SR1.0 | AM65x SR2.0 | AM64x/AM243x |
|---|---|---|---|---|
| General PRU Specifications | Subsystem type | 3x PRU_ICSSG | 3x PRU_ICSSG | 2x PRU_ICSSG |
| General PRU Specifications | Number of PRU cores | 2 | 2 | 2 |
| General PRU Specifications | Number of RTU_PRU (Auxiliary PRU) cores | 2 | 2 | 2 |
| General PRU Specifications | Number of TX_PRU (Transmit PRU) cores | 0 | 2 | 2 |
| General PRU Specifications | Max Frequency | 250 MHz | 250 MHz | 333 MHz |
| General PRU Specifications | IRAM Size (per PRU / RTU_PRU / TX_PRU core) | 12 KB (w/ ECC) / 8 KB (w/ ECC) / 0 KB | 12 KB (w/ ECC) / 8 KB (w/ ECC) / 6 KB (w/ ECC) | 12 KB (w/ ECC) / 8 KB (w/ ECC) / 6 KB (w/ ECC) |
| General PRU Specifications | DRAM Size (2 DRAMs per PRU_ICSSG) | 8 KB (w/ ECC) | 8 KB (w/ ECC) | 8 KB (w/ ECC) |
| General PRU Specifications | Shared DRAM Size | 64 KB (w/ ECC) | 64 KB (w/ ECC) | 64 KB (w/ ECC) |
| General PRU Specifications | INTC | Yes | Yes | Yes |
| General PRU Specifications | General Purpose Inputs (per PRU core) | Direct; or 16-bit parallel capture; or 28-bit shift; or 3 ch EnDat 2.2; or 9 ch Sigma Delta | Direct; or 16-bit parallel capture; or 28-bit shift; or 3 ch EnDat 2.2; or 9 ch Sigma Delta | Direct; or 16-bit parallel capture; or 28-bit shift; or 3 ch EnDat 2.2; or 9 ch Sigma Delta |
| General PRU Specifications | General Purpose Outputs (per PRU core) | Direct or Shift out | Direct or Shift out | Direct or Shift out |
| General PRU Specifications | GPI Pins (PRU0, PRU1) | PRU_ICSSG0: 20/20<br>PRU_ICSSG1: 20/20<br>PRU_ICSSG2: 18/18 <sup>(1)</sup> | PRU_ICSSG0: 20/20<br>PRU_ICSSG1: 20/20<br>PRU_ICSSG2: 18/18 <sup>(1)</sup> | PRU_ICSSG0: 20/20<br>PRU_ICSSG1: 20/20 |
| General PRU Specifications | GPO Pins (PRU0, PRU1) | PRU_ICSSG0: 20/20<br>PRU_ICSSG1: 20/20<br>PRU_ICSSG2: 18/18 <sup>(1)</sup> | PRU_ICSSG0: 20/20<br>PRU_ICSSG1: 20/20<br>PRU_ICSSG2: 18/18 <sup>(1)</sup> | PRU_ICSSG0: 20/20<br>PRU_ICSSG1: 20/20 |
| Accelerators: Data Processing | MPY/MAC | Yes | Yes | Yes |
| Accelerators: Data Processing | CRC 16/32 | Yes | Yes | Yes |
| Accelerators: Data Processing | Scratch Pad | Yes (PRU cores: 4 banks, RTU_PRU cores: 2 banks) | Yes (PRU cores: 3 banks, RTU_PRU cores: 3 banks, TX_PRU cores: 2 banks) | Yes (PRU cores: 3 banks, RTU_PRU cores: 3 banks, TX_PRU cores: 2 banks) |
| Accelerators: Data Processing | IPC Scratch Pad | Yes | Yes | Yes |
| Accelerators: Data Processing | Broadside RAM | 4 KB | 2 KB | 2 KB |
| Accelerators: Data Processing | BSWAP | Yes | Yes | Yes |
| Accelerators: Data Processing | SUM32 | Yes | Yes | Yes |
| Accelerators: Data Processing | Task Manager | Yes | Yes | Yes |
| Accelerators: Data Processing | Spinlock | Yes | Yes | Yes |
| Accelerators: Data Processing | Filter Data Base (FDB) | Yes | Yes | Yes |
| Accelerators: Data Movement | XFR2VBUS | Yes | Yes | Yes |
| Accelerators: Data Movement | PSI TX & RX | Yes | Yes | Yes |
| Accelerators: Data Movement | XFR2TR | Yes | Yes | Yes |
| Peripherals | UART | 1 | 1 | 1 |
| Peripherals | eCAP | 1 | 1 | 1 |
| Peripherals | IEP | 2 | 2 | 2 |
| Peripherals | MII_G_RT (MII/RGMII) | 2 | 2 | 2 |
| Peripherals | MDIO | 1 | 1 | 1 |
| Peripherals | SGMII | 2 (PRU_ICSSG2 instance only) | 2 (PRU_ICSSG2 instance only) | No |
| Peripherals | PWM | 12 primary and 12 complimentary outputs | 12 primary and 12 complimentary outputs | 12 primary and 12 complimentary outputs |

<sup>(1)</sup> PRG2\_PRU0/1\_GPI/O17 does not have a ball named after it, but it is still muxed out

## 4 PRUSS Features

Table 4-1 summarizes the PRUSS features.

**Table 4-1. PRUSS Features** 

| Category | Feature | AM62x |
|---|---|---|
| General PRU Specifications | Subsystem type | 1x PRUSS |
| General PRU Specifications | Number of PRU cores | 2 |
| General PRU Specifications | Number of RTU_PRU (Auxiliary PRU) cores | 0 |
| General PRU Specifications | Number of TX_PRU (Transmit PRU) cores | 0 |
| General PRU Specifications | Max Frequency | 333 MHz |
| General PRU Specifications | IRAM Size (per PRU / RTU_PRU / TX_PRU core) | 16 KB (w/ ECC) |
| General PRU Specifications | DRAM Size (2 DRAMs per PRU_ICSSG) | 8 KB (w/ ECC) |
| General PRU Specifications | Shared DRAM Size | 32 KB (w/ ECC) |
| General PRU Specifications | INTC | Yes |
| General PRU Specifications | General Purpose Inputs (per PRU core) | Direct; or 16-bit parallel capture; or 28-bit shift; |
| General PRU Specifications | General Purpose Outputs (per PRU core) | Direct; or Shift out |
| General PRU Specifications | GPI Pins (PRU0, PRU1) | 20, 20 |
| General PRU Specifications | GPO Pins (PRU0, PRU1) | 20, 20 |
| Accelerators: Data Processing | MPY/MAC | Yes |
| Accelerators: Data Processing | CRC 16/32 | Yes |
| Accelerators: Data Processing | Scratch Pad | Yes (3 banks) |
| Accelerators: Data Processing | IPC Scratch Pad | No |
| Accelerators: Data Processing | Broadside RAM | No |
| Accelerators: Data Processing | BSWAP | No |
| Accelerators: Data Processing | SUM32 | No |
| Accelerators: Data Processing | Task Manager | No |
| Accelerators: Data Processing | Spinlock | No |
| Accelerators: Data Processing | Filter Data Base (FDB) | No |
| Accelerators: Data Movement | XFR2VBUS | Yes |
| Accelerators: Data Movement | PSI TX & RX | No |
| Accelerators: Data Movement | XFR2TR | No |
| Peripherals | UART | 1 |
| Peripherals | eCAP | 1 |
| Peripherals | IEP | 1 |
| Peripherals | MII_G_RT (MII/RGMII) | No |
| Peripherals | MDIO | No |
| Peripherals | SGMII | No |
| Peripherals | PWM | No |

## 5 References

- Texas Instruments: *[AM335x and AMIC110 Sitara™ Processors Technical Reference Manual](https://www.ti.com/lit/pdf/spruh73)*
- Texas Instruments: *[AM437x Sitara Processors Technical Reference Manual](https://www.ti.com/lit/pdf/SPRUHL7)*
- Texas Instruments: *[PRU-ICSS/PRU\_ICSSG Migration Guide](https://www.ti.com/lit/pdf/SPRACJ8)*
- Texas Instruments: *[AM64x/AM243x Technical Reference Manual](https://www.ti.com/lit/pdf/SPRUIM2)*
- Texas Instruments: *[AM65x/DRA80xM Processors Technical Reference Manual](https://www.ti.com/lit/pdf/SPRUID7)*
- Texas Instruments: *[AM62x Sitara Processors Technical Reference Manual](https://www.ti.com/lit/pdf/SPRUIV7)*

# 6 Revision History

NOTE: Page numbers for previous revisions may differ from page numbers in the current version.

|   | Changes from Revision F (August 2022) to Revision G (October 2022)                             | Page |
|---|------------------------------------------------------------------------------------------------|------|
| • | Updated the numbering format for tables, figures and cross-references throughout the document |      |
| • | Added AM263x to PRU-ICSS capable devices |      |
| • | Added AM243x to PRU_ICSSG capable devices |      |
| • | Changed table view to landscape and added AM263x to table |      |
| • | Added AM243x device to table and updated table formatting |      |

#### IMPORTANT NOTICE AND DISCLAIMER

TI PROVIDES TECHNICAL AND RELIABILITY DATA (INCLUDING DATA SHEETS), DESIGN RESOURCES (INCLUDING REFERENCE DESIGNS), APPLICATION OR OTHER DESIGN ADVICE, WEB TOOLS, SAFETY INFORMATION, AND OTHER RESOURCES "AS IS" AND WITH ALL FAULTS, AND DISCLAIMS ALL WARRANTIES, EXPRESS AND IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY RIGHTS.

These resources are intended for skilled developers designing with TI products. You are solely responsible for (1) selecting the appropriate TI products for your application, (2) designing, validating and testing your application, and (3) ensuring your application meets applicable standards, and any other safety, security, regulatory or other requirements.

These resources are subject to change without notice. TI grants you permission to use these resources only for development of an application that uses the TI products described in the resource. Other reproduction and display of these resources is prohibited. No license is granted to any other TI intellectual property right or to any third party intellectual property right. TI disclaims responsibility for, and you will fully indemnify TI and its representatives against, any claims, damages, costs, losses, and liabilities arising out of your use of these resources.

TI's products are provided subject to [TI's Terms of Sale](https://www.ti.com/legal/termsofsale.html) or other applicable terms available either on [ti.com](https://www.ti.com) or provided in conjunction with such TI products. TI's provision of these resources does not expand or otherwise alter TI's applicable warranties or warranty disclaimers for TI products.

TI objects to and rejects any additional or different terms you may have proposed. IMPORTANT NOTICE

Mailing Address: Texas Instruments, Post Office Box 655303, Dallas, Texas 75265 Copyright © 2022, Texas Instruments Incorporated
