# SPI Loop Back example

## Introduction

This example provides a base for running loopback spi connection between a pair of master and slave PRU cores. 
The example uses generic SPI macros introduced in files spi_master_macros.inc and spi_slave_macros.inc to emulate generic SPI master and slave interfaces over PRU GPIO pins.

 ## Overview
 ### Timing Parameters
 All cycle level frequency calculations are based on a 333MHz PRU System clock, which gives 3ns per instruction cycle, i.e $t_{f} = 3ns$
 
 $t_{slew}$ : Slew rate of Data toggling (slew rate of PRU GPIO mode pin) = $2ns$
 $t_{p}$ : Propagation delay of wire (In the example, a 15 cm wire was used) = $0.5ns$
 #### SPI Master Macros: 

 | Macros Name | Description         | Peak cycles per bit | Theoretical Max. SCLK | Recommended delay components (50% duty cycle) | Practical Max. SCLK
 |--------------|:------------------:|:--------:|:--------:|:-------:|:-------:|
 | m_read_packet_spi_msb_gpo_sclk |  SPI Master side macro to read 1 data packet through SPI interface MSB first.  | (2+d1)+(5+d2) cycles  | 33.33MHz | d1 =10, d2 =7 | 13.88MHz
 | m_read_packet_spi_lsb_gpo_sclk |  SPI Master side macro to read 1 data packet through SPI interface LSB first.  | (2+d1)+(5+d2) cycles  | 33.33MHz | d1 =10, d2 =7 | 13.88MHz
 | m_send_packet_spi_msb_gpo_sclk |  SPI Master side macro to send 1 data packet through SPI interface MSB first.  | (4+d1)+(3+d2) cycles  | 41.66MHz | d1 =0, d2 =1 | 41.66MHz
 | m_send_packet_spi_lsb_gpo_sclk |  SPI Master side macro to send 1 data packet through SPI interface LSB first.  | (4+d1)+(3+d2) cycles  | 41.66MHz | d1 =0, d2 =1 | 41.66MHz
 | m_transfer_packet_spi_master_gpo_sclk |  SPI Master side macro to read 1 data packet and write 1 data packet concurrently through the SPI interface. | (4+d1)+(6+d2) cycles  | 27.77MHz | d1 =9, d2 =7 | 12.8MHz

Timing parameters for macros: **m_read_packet_spi_msb_gpo_sclk** and **m_read_packet_spi_lsb_gpo_sclk** are shown below:

<figure>
<img src="images\SPI_Master_read.png" alt="Master_mode_read_macros" width="900">
<figcaption>Fig.1 : Master mode Read functions</figcaption>
</figure>

 |Parameter         | Description | MIN | MAX | unit |
 |--------------|:------------------:|:--------:|:--------:|:-------:|
 |$t_{csp\_m}$         | CS poll detection to macro entry. *(This can be application dependent)*. Any delay can be inserted | 0 |  | ns |
 |$t_{msh}$         | Delay time: minimum time between macro start to first shift edge | $2t_{f} + t_{slew}$ |  | ns |
 |$t_{msm}$         | Delay time: minimum time between macro start to first sampling edge | $(4+d_{1})t_{f} + t_{slew}$ |  | ns |
 |$t_{ssh}$          | Setup time: minimum time required between CS pull down to SCLK shifting edge | $t_{csp\_m} + t_{msh}= t_{csp\_m} + 2t_{f} + t_{slew}$ |  | ns |
 |$t_{ssm}$         | Setup time: minimum time required between CS pull down to SCLK sampling edge | $t_{csp\_m} + t_{msm} = t_{csp\_m} + (4+d_{1})t_{f} + t_{slew}$|  | ns |
 |$t_{s}$         | Setup time: time SDI needs to be stable before sampling edge | $0$ |  | ns |
 |$t_{h}$         | Hold time: time SDI has to be stable after sampling edge | $t_{f}$ |  | ns |

Timing parameters for macros  **m_send_packet_spi_msb_gpo_sclk**, and **m_send_packet_spi_lsb_gpo_sclk** are shown below:

<figure>
<img src="images\SPI_Master_send.png" alt="Master_mode_send_macros" width="900">
<figcaption>Fig.2 : Master mode Send functions</figcaption>
</figure>

 |Parameter         | Description | MIN | MAX | unit |
 |--------------|:------------------:|:--------:|:--------:|:-------:|
 |$t_{csp\_m}$         | CS poll detection to macro entry. *(This can be application dependent)*. Any delay can be inserted | 0 |  | ns |
 |$t_{msh}$         | Delay time: minimum time between macro start to first shift edge | $2t_{f} + t_{slew}$ |  | ns |
 |$t_{msm}$         | Delay time: minimum time between macro start to first sampling edge | $(6+d_{1})t_{f} + t_{slew}$ |  | ns |
 |$t_{ssh}$          | Setup time: minimum time required between CS pull down to SCLK shifting edge | $t_{csp\_m} + t_{msh}= t_{csp\_m} + 2t_{f} + t_{slew}$ |  | ns |
 |$t_{ssm}$         | Setup time: minimum time required between CS pull down to SCLK sampling edge | $t_{csp\_m} + t_{msm} = t_{csp\_m} + (6+d_{1})t_{f} + t_{slew}$|  | ns |
 |$t_{sb}$         | Delay time: shifting edge to SDO bit on wire|  $2t_{f}$  |  | ns |
 |$t_{cd}$          | Delay time: CS pull down to first SDO bit on wire |  $t_{csp\_m} + t_{msh} + t_{sb} = t_{csp\_m} + 4t_{f} + t_{slew}$ |  | ns |
 |$t_{s}$         | Setup time: time SDO is stable before sampling edge | $(2+d_{1})t_{f} - t_{slew}$ |  | ns |
 |$t_{h}$         | Hold time: time SDO has to be stable after sampling edge | $(5+d_{2})t_{f}$ |  | ns |

Timing parameters for macro **m_transfer_packet_spi_master_gpo_sclk** is shown below:

<figure>
<img src="images\SPI_Master_transfer.png" alt="Master_mode_send_macros" width="900">
<figcaption>Fig.3 : Master mode Transfer functions</figcaption>
</figure>

 |Parameter         | Description | MIN | MAX | unit |
 |--------------|:------------------:|:--------:|:--------:|:-------:|
 |$t_{csp\_m}$         | CS poll detection to macro entry. *(This can be application dependent)*. Any delay can be inserted |  |  | ns |
 |$t_{msh}$         | Delay time: minimum time between macro start to first shift edge | $2t_{f} + t_{slew}$ |  | ns |
 |$t_{msm}$         | Delay time: minimum time between macro start to first sampling edge | $(6+d_{1})t_{f} + t_{slew}$ |  | ns |
 |$t_{ssh}$          | Setup time: minimum time required between CS pull down to SCLK shifting edge | $t_{csp\_m} + t_{msh}= t_{csp\_m} + 2t_{f} + t_{slew}$ |  | ns |
 |$t_{ssm}$         | Setup time: minimum time required between CS pull down to SCLK sampling edge | $t_{csp\_m} + t_{msm} = t_{csp\_m} + (6+d_{1})t_{f} + t_{slew}$|  | ns |
 |$t_{sb}$         | Delay time: shifting edge to SDO bit on wire (except 1st bit) |  $2t_{f} + t_{slew}$  |  | ns |
 |$t_{cd}$          | Delay time: CS pull down to first SDO bit on wire |  $t_{csp\_m} + t_{msh} + t_{sb} = t_{csp\_m} + 4t_{f} + t_{slew}$ |  | ns |
 |$t_{so}$         | Setup time: Time SDO is kept stable before sampling edge | $(2+d_{1})t_{f} - t_{slew}$ |  | ns |
 |$t_{ho}$         | Hold time: Time SDO is kept stable after sampling edge | $(8+d_{2})t_{f}$ |  | ns |
 |$t_{si}$         | SetUp time: Time SDI needs to be stable before sampling edge | $0$ |  | ns |
 |$t_{hi}$         | Hold time: Time SDI needs to be stable after sampling edge | $t_{f}$ |  | ns |

 #### SPI Slave Macros:  
  | Macros Name | Description         | Peak cycle per bit| Max. SCLK | Practical Max. SCLK| 
 |--------------|:------------------:|:--------:|:--------:|:--------:|
 | m_read_packet_spi_slave_msb_gpi_sclk |  SPI Slave side macro to read 1 data packet through SPI interface MSB first.  | 7 cycles  | 47.619MHz |47.619MHz |
 | m_read_packet_spi_slave_lsb_gpi_sclk |  SPI Slave side macro to read 1 data packet through SPI interface LSB first.  | 7 cycles  | 47.619MHz | 47.619MHz |
 | m_send_packet_spi_slave_msb_gpo_sclk |  SPI Slave side macro to send 1 data packet through SPI interface MSB first.  | 7 cycles  | 47.619MHz |13.88MHz |
 | m_send_packet_spi_slave_lsb_gpo_sclk |  SPI Slave side macro to send 1 data packet through SPI interface LSB first.  | 7 cycles  | 47.619MHz |13.88MHz |
 | m_transfer_packet_spi_slave_gpi_sclk |  SPI Slave side macro to read and write 1 data packet concurrently through SPI interface.  | 6 cycles (shifting half) + 7 cycles (sampling half) = 13 cycles | 23.8MHz | 12.8MHz |

Timing parameters for macros: **m_read_packet_spi_slave_msb_gpi_sclk**, **m_read_packet_spi_slave_lsb_gpi_sclk**, **m_send_packet_spi_slave_msb_gpo_sclk**, and **m_send_packet_spi_slave_lsb_gpo_sclk** is shown below:

<figure>
<img src="images\SPI_Slave_read.png" alt="Slave_mode_read_macros" width="900">
<figcaption>Fig.4 : Peripheral mode/ Slave mode Read functions</figcaption>
</figure>

<figure>
<img src="images\SPI_Slave_send.png" alt="Slave_mode_send_macros" width="900">
<figcaption>Fig.5 : Peripheral mode/ Slave mode Send functions</figcaption>
</figure>

 |Parameter         | Description | MIN | MAX | unit |
 |--------------|:------------------:|:--------:|:--------:|:-------:|
 |$t_{csp\_m}$         | CS poll detection to macro entry. This can be application dependent | $3t_{f}$ + $t_{p}$ |  | ns |
 |$t_{msh}$         | Delay time: macro start to first shift edge | $2t_{f} + t_{slew}$ |  | ns |
 |$t_{msm}$         | Delay time: minimum time between macro start to first sampling edge | $3t_{f} + t_{slew}$ |  | ns |
 |$t_{ssh}$          | Setup time: minimum time required between CS pull down to SCLK shifting edge | $t_{csp\_m} + t_{msh}= t_{csp\_m} + 2t_{f} + t_{slew}$ |  | ns |
 |$t_{ssm}$         | Setup time: minimum time required between CS pull down to SCLK sampling edge | $t_{csp\_m} + t_{msm} = t_{csp\_m} + 3t_{f} + t_{slew}$|  | ns |
 |$t_{sb}$         | Delay time: shifting edge to SDO bit on wire |  $3t_{f} + t_{slew}$  |  | ns |
 |$t_{cd}$          | Delay time: CS pull down to first SDO bit on wire |  $t_{csp\_m} + t_{msh} + t_{sb} = t_{csp\_m} + 5t_{f} + 2t_{slew}$ |  | ns |
 |$t_{h}$         | Hold time: minimum time data has to be stable after sampling edge | $2t_{f} + t_{slew}$ |  | ns |

Timing parameters for macro **m_transfer_packet_spi_slave_gpi_sclk** is shown below:

<figure>
<img src="images\SPI_Slave_transfer.png" alt="Slave_mode_transfer_macro" width="900">
<figcaption>Fig.6 : Peripheral mode/ Slave mode Transfer function</figcaption>
</figure>


 |Parameter         | Description | MIN | MAX | unit |
 |--------------|:------------------:|:--------:|:--------:|:-------:|
 |$t_{csp\_m}$         | CS poll detection to macro entry. This can be application dependent | $3t_{f}$ + $t_{p}$ |  | ns |
 |$t_{msh}$         | Delay time: minimum time between macro start to first shift edge | $2t_{f} + t_{slew}$ |  | ns |
 |$t_{msm}$         | Delay time: minimum time between macro start to first sampling edge | $7t_{f} + t_{slew}$ |  | ns |
 |$t_{ssh}$          | Setup time: minimum time required between CS pull down to SCLK shifting edge | $t_{csp\_m} + t_{msh}= t_{csp\_m} + 2t_{f} + t_{slew}$ |  | ns |
 |$t_{ssm}$         | Setup time: minimum time required between CS pull down to SCLK sampling edge | $t_{csp\_m} + t_{msm} = t_{csp\_m} + 7t_{f} + t_{slew}$|  | ns |
 |$t_{sb1}$         | Delay time: shifting edge to first SDO bit on wire |  $3t_{f} + t_{slew}$  |  | ns |
 |$t_{sb}$         | Delay time: shifting edge to SDO bit on wire (except 1st bit) |  $4t_{f} + t_{slew}$  |  | ns |
 |$t_{cd}$          | Delay time: CS pull down to first SDO bit on wire |  $t_{csp\_m} + t_{msh} + t_{sb1} = t_{csp\_m} + 5t_{f} + 2t_{slew}$ |  | ns |
 |$t_{h}$         | Hold time: SDI has to be kept stable after sampling edge | $2t_{f} + t_{slew}$ |  | ns |
 |$t_{s}$         | Setup time: SDO bit is kept stable before sampling edge | $2t_{f} - t_{slew}$ |  | ns |

 ## Instructions
 1. Open CCS (version 12.8.1) and import the SPI Loopback example (open-pru\examples\spi_loopback) from the Open-pru repo. Make sure to import projects corresponding to r5fss0-0, icss_g0_pru0 and icss_g0_pru1 as all three cores are utilised in this example.

 2. In project spi_loopback_am243x-lp_icss_g0_pru0_fw_ti-pru-cgt (here after shall be called the pru0 project or spi master project), adapt the code in main.asm, line 102 to select the type of SPI transmission needed. By default this is set to SPI master transfer. 

 3. In project spi_loopback_am243x-lp_icss_g0_pru1_fw_ti-pru-cgt (here after shall be called the pru1 project or spi slave project), adapt the code in main.asm, line 106 to match the SPI transmission type selected in the pru0 project. By default this is set to SPI slave transfer.

 4. The required SPI mode can also be set in the corresponding macros. Make sure the selected mode is same on both spi master and slave projects.

 5. Build the pru0 and pru1 projects first. This ensures that the header files with the binary for pru0 and pru1 are generated.

 6. Now build the project spi_loopback_am243x-lp_r5fss0-0_freertos_ti-arm-clang (Here after will be called r5f project).
 
 7. Connect the SPI master and slave pins as per configuration. In the example these pins are mapped as shown below:

 <table>
 <tr>
    <th colspan="4">Slave
    <th colspan="4">Master
 </tr> 
 <tr>
    <th>Pin
    <th>SOC Pad name
    <th>PRU Signal name
    <th>Signal
    <th>Signal
    <th>PRU Signal name
    <th>SOC Pad name
    <th>Pin
 </tr> 
 <tr>
    <td>J7.7 (pin 67)
    <td>GPIO1_21
    <td>PRG0_PRU1_GPO1
    <td>SDI
    <td>SDO
    <td>PRG0_PRU0_GPO3
    <td>GPIO1_3
    <td>J2.2 (pin 19)
 </tr>
 <tr>
    <td>J7.8 (pin 68)
    <td>GPIO1_22
    <td>PRG0_PRU1_GPO2
    <td>SDO
    <td>SDI
    <td>PRG0_PRU0_GPO14
    <td>SPI3_D1
    <td>J2.7 (pin 14)
 </tr>
 <tr>
    <td>J7.9 (pin 69)
    <td>GPIO1_26
    <td>PRG0_PRU1_GPO6
    <td>CS
    <td>CS
    <td>PRG0_PRU0_GPO5
    <td>GPIO1_5
    <td>J2.8 (pin 13)
 </tr>
 <tr>
    <td>J7.10 (pin 70)
    <td>GPIO1_31
    <td>PRG0_PRU1_GPO11
    <td>SCLK
    <td>SCLK
    <td>PRG0_PRU0_GPO4
    <td>GPIO1_4
    <td>J2.4 (pin 17)
 </tr>
 </table>

 8. Power on the AM243x Launch Pad. It is recommended to go through the "Getting started" section of MCU + SDK if this is your first time working on the launchpad.
 
 9. Set up Target configuration window from View>>Target Configuration. Then launch any project target configuration by right clicking on the AM2434_ALX.ccxml file and selecting "launch selected configuration"

 10. From the debug window, select the r5f project and load the corresponding .out file and then press the "Resume" button to start the example in debug mode. 

 11. Once the debug console shows that both PRU0 and PRU1 has been initialized, then proceed to check memory arrays at 0x30010000 and 0x3001004C to see the data being received by the SPI Slave and SPI master respectively.  

Note: None of the ICSS Broadside Accelerators are used for this implementation.

 ## Limitations
 Some of the macros were not stable at the theoretical maximum. These include: m_send_packet_spi_slave_msb_gpo_sclk, m_send_packet_spi_slave_lsb_gpo_sclk, m_transfer_packet_spi_slave_gpi_sclk, m_read_packet_spi_msb_gpo_sclk, m_read_packet_spi_lsb_gpo_sclk and m_transfer_packet_spi_master_gpo_sclk. Experimentally a frequency limitation was found and is documented in this document a "Practical Max SCLK." at which tests where successfully carried out.
# Supported Combinations

 Parameter      | Value
 ---------------|-----------
 ICSSG          | ICSSG0 - PRU0, PRU1
 Toolchain      | pru-cgt
 Board          | am243x-lp
 Example folder | examples/spi_loopback
