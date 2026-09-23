<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>

# PRU-ICSS documentation map

</div>

[Introduction](#introduction)  
[Documentation by device](#documentation-by-device)  

## Introduction

Developers who started on AM335x look for SPRUHF8A. For OpenPRU-supported
AM26x, AM243x, and AM64x devices, the PRU-ICSS / PRU-ICSSM material is in the
device technical reference manual, the device register addendum, and the PRU
Assembly Instruction User Guide
([SPRUIJ2](https://www.ti.com/lit/pdf/SPRUIJ2)). There is no SPRUHF8A-style
standalone guide for these parts by design.

AM26x uses **PRU-ICSSM**, a later generation than the first-generation AM335x
PRU-ICSS described in SPRUHF8A.

The same question is on TI E2E (thread 1665810).

Each literature link uses `https://www.ti.com/lit/pdf/<ID>` and opens the
current revision on ti.com.

## Documentation by device

| Device family | Technical reference manual | Register addendum | Assembly instruction guide |
| --- | --- | --- | --- |
| AM263Px | [AM263Px Sitara Microcontrollers Technical Reference Manual (SPRUJ55)](https://www.ti.com/lit/pdf/SPRUJ55) | [AM263Px Sitara Microcontrollers Register Addendum (SPRUJ57)](https://www.ti.com/lit/pdf/SPRUJ57) | [PRU Assembly Instruction User Guide (SPRUIJ2)](https://www.ti.com/lit/pdf/SPRUIJ2) |
| AM263x | [AM263x Sitara Microcontroller Technical Reference Manual (SPRUJ17)](https://www.ti.com/lit/pdf/SPRUJ17) | [AM263x Sitara Microcontroller Register Addendum (SPRUJ42)](https://www.ti.com/lit/pdf/SPRUJ42) | [PRU Assembly Instruction User Guide (SPRUIJ2)](https://www.ti.com/lit/pdf/SPRUIJ2) |
| AM261x | [AM261x Sitara Microcontrollers Technical Reference Manual (SPRUJB6)](https://www.ti.com/lit/pdf/SPRUJB6) | [AM261x Sitara Microcontrollers Register Addendum (SPRUJ94)](https://www.ti.com/lit/pdf/SPRUJ94) | [PRU Assembly Instruction User Guide (SPRUIJ2)](https://www.ti.com/lit/pdf/SPRUIJ2) |
| AM243x / AM64x | [AM64x/AM243x Technical Reference Manual (SPRUIM2)](https://www.ti.com/lit/pdf/SPRUIM2) | See the technical documents on the device product page (for example [AM2434](https://www.ti.com/product/AM2434) or [AM6442](https://www.ti.com/product/AM6442)) and the related documentation listed with the TRM. | [PRU Assembly Instruction User Guide (SPRUIJ2)](https://www.ti.com/lit/pdf/SPRUIJ2) |

A short instruction summary in this repository is the
[PRU Assembly Instruction Cheat Sheet](./PRU%20Assembly%20Instruction%20Cheat%20Sheet.md),
which cites the PRU Assembly Instruction User Guide (SPRUIJ2).

OpenPRU also supports AM62x. Use that device's technical reference manual and
the technical documents on its product page.
