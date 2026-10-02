# PRU-ICSS Documentation Map

Developers coming to the PRU from the AM335x generation often look for a single,
standalone *PRU-ICSS Reference Guide* (SPRUHF8A). For the Sitara MCUs supported
by OpenPRU there is **no direct SPRUHF8A equivalent** — and that is by design.

The AM335x device integrates a **first-generation PRU-ICSS**, which SPRUHF8A
documents as a standalone guide. The AM26x devices (AM261x, AM263x, AM263Px)
integrate a later-generation, MCU-oriented **PRU-ICSSM**, and the AM24x/AM64x
devices integrate the **PRU-ICSSG** variant. For all of these, the PRU-ICSS(M/G)
subsystem is documented as a **chapter of the device Technical Reference Manual
(TRM)**, complemented by the device **Register Addendum** for the full register
map, and by the common **PRU Assembly Instruction User Guide** for the
instruction set.

This page maps each supported device to its authoritative references.

## Per-device references

| Device family | Technical Reference Manual (contains the PRU-ICSS chapter) | Register Addendum | PRU assembly instruction set |
| --- | --- | --- | --- |
| AM263Px       | [SPRUJ55](https://www.ti.com/lit/ug/spruj55/spruj55.pdf) | AM263Px Register Addendum (search "AM263Px Register Addendum" on ti.com) | [SPRUIJ2](https://www.ti.com/lit/ug/spruij2/spruij2.pdf) |
| AM263x        | [SPRUJ17](https://www.ti.com/lit/ug/spruj17/spruj17.pdf) | [SPRUJ42](https://www.ti.com/lit/ug/spruj42/spruj42.pdf) | [SPRUIJ2](https://www.ti.com/lit/ug/spruij2/spruij2.pdf) |
| AM261x        | [SPRUJB6](https://www.ti.com/lit/ug/sprujb6/sprujb6.pdf) | AM261x Register Addendum (search "AM261x Register Addendum" on ti.com) | [SPRUIJ2](https://www.ti.com/lit/ug/spruij2/spruij2.pdf) |
| AM243x / AM64x | [SPRUIM2](https://www.ti.com/lit/ug/spruim2/spruim2.pdf) | device Register Addendum (search on ti.com) | [SPRUIJ2](https://www.ti.com/lit/ug/spruij2/spruij2.pdf) |

> Literature numbers are the base (un-revisioned) identifiers. TI serves the
> latest revision from the un-suffixed URL (for example `spruj55.pdf`); append a
> revision letter for a specific revision (for example `spruj55d.pdf`). Always
> confirm you are reading the latest revision for your silicon.

## How to find the PRU-ICSS content inside a TRM

Within the device TRM, the relevant material lives under the
**PRU-ICSS / PRU_ICSSM / PRU_ICSSG** chapter, which typically covers:

- Subsystem overview, memory map, and integration
- PRU cores, register files (`R30` output / `R31` input), and the constant table
- Interrupt controller (INTC) and event mapping
- Peripherals in the subsystem (IEP, MII_RT/MDIO where applicable, UART, etc.)
- Local INTC / interrupt connections

For the register-level detail (bit fields, offsets, reset values) use the
device **Register Addendum** alongside the TRM chapter.

## Instruction set

The PRU assembly instruction set is common across these devices and is
documented in the **PRU Assembly Instruction User Guide (SPRUIJ2)**. A quick
reference is also maintained in this repository:
[PRU Assembly Instruction Cheat Sheet](./PRU%20Assembly%20Instruction%20Cheat%20Sheet.md).

## Why there is no SPRUHF8A for these parts

SPRUHF8A documents the **first-generation PRU-ICSS** on AM335x. The OpenPRU
target devices use the later **PRU-ICSSM** (AM26x) and **PRU-ICSSG** (AM24x/AM64x)
generations, whose documentation was integrated into the device TRM + register
addendum model rather than published as a separate subsystem reference guide.
The mapping above is the equivalent starting point.
