# spi_driver Project

## Introduction

This example implements a bit-banged SPI **master** driver on a PRU core,
controlled from the R5F over a small runtime configuration API. There is no
dedicated SPI peripheral involved — PRU0 toggles SCLK/SDO/CS and samples SDI
directly using the `spi_master_macros.inc` bit-bang macros. PRU1 runs a
matching SPI **slave** that emulates a simple fixed-response encoder, so the
R5F demo can drive real command/response transactions on hardware; PRU1 is
not part of the driver API and is not intended to be configurable.

Both PRU0 and PRU1 are loaded **once** and run forever — there is no
load-once-halt-once cycle per transfer. The R5F drives repeated SPI
transactions against the already-running PRU0 via a DMEM-based
trigger/poll mechanism (see "Driver Overview" below).

Each triggered transaction is a **burst of 5 independent 16-bit transfers**
run back-to-back under one CS-low assertion, with **no delay inserted
between transfers** — SCLK runs continuously for all 80 bits. This is a
deliberate design choice (minimum/no inter-transfer delay was the
requirement), not an artifact — see "One-transaction response delay" below
for the one real consequence of running transfers back-to-back like this.

This is a first-version prototype driver, not a production-hardened one.

## Supported Combinations

Refer to open-pru/examples/readme.md > Supported processors per-project
for the list of processors that support building this project, and information
about porting this project to other processors.

Currently only **AM261x LaunchPad** (ICSS_M1, PRU0 = master, PRU1 = slave) has
been implemented and validated.

## Validated HW & SW

This project was tested on hardware with these software versions:

| Processor | Hardware   | Software                   |
| --------- | ---------- | -------------------------- |
| AM261x    | AM261x LaunchPad | FIXME                 |

## Driver Overview

The R5F application (`mcuplus/empty_example.c`) loads PRU1 (slave responder)
then PRU0 (master), **once**. Neither PRU halts afterward — PRU0 sits in a
persistent loop waiting for a trigger, and PRU1 sits in a persistent loop
waiting for CS. `PRUICSS_loadFirmware()` internally does
`disableCore -> writeMemory -> resetCore -> enableCore`, i.e. it starts the PRU
running immediately, so mode/bitOrder configuration must be written before
this call — but unlike the transfer itself, that only happens once at startup.

Configuration and transfer data are exchanged through PRU0's own data RAM
(DMEM0):

| Offset      | Name                  | Direction     | Meaning                                                          |
| ----------- | --------------------- | ------------- | ----------------------------------------------------------------- |
| 0x00        | `cfg_mode`             | R5F -> PRU0   | SPI mode, 0-3 (`PRU_SPI_Mode`), read once at startup               |
| 0x04        | `cfg_bitorder`         | R5F -> PRU0   | 0 = MSB first, 1 = LSB first, read once at startup                 |
| 0x10        | `cfg_trigger`          | R5F <-> PRU0  | R5F sets to 1 to start a transaction; PRU0 clears to 0 when all 5 transfers complete |
| 0x14 - 0x24 | `cfg_cmd0` .. `cfg_cmd4`     | R5F -> PRU0   | 5 independent 16-bit command words, written before setting `cfg_trigger` |
| 0x28 - 0x38 | `master_rx5_0` .. `master_rx5_4` | PRU0 -> R5F   | 5 independent 16-bit response words, written after the 5-transfer burst completes |

### R5F driver API

```c
typedef enum { PRU_SPI_MODE_0=0, PRU_SPI_MODE_1, PRU_SPI_MODE_2, PRU_SPI_MODE_3 } PRU_SPI_Mode;
typedef enum { PRU_SPI_MSB_FIRST=0, PRU_SPI_LSB_FIRST } PRU_SPI_BitOrder;
typedef struct {
    PRU_SPI_Mode     mode;
    PRU_SPI_BitOrder bitOrder;
} PRU_SPI_Config;

/* Must be called BEFORE PRUICSS_loadFirmware(PRU0) */
static void PRU_SPI_setConfig(const PRU_SPI_Config *cfg);

/* Writes 5 independent 16-bit command words + sets cfg_trigger, polls until
 * PRU0 clears it (5ms timeout) after running all 5 transfers back-to-back
 * under one CS-low assertion, then reads 5 independent response words into
 * responsesOut. commandWords/responsesOut must each point to 5 uint32_t.
 * Returns 0 on timeout. */
static int PRU_SPI_runTransaction5(const uint32_t *commandWords, uint32_t *responsesOut);
```

Usage:

```c
PRU_SPI_Config cfg = { .mode = PRU_SPI_MODE_3, .bitOrder = PRU_SPI_MSB_FIRST };
PRU_SPI_setConfig(&cfg);
PRUICSS_loadFirmware(gPruHandle, PRUICSS_PRU1, ...);   /* slave responder first */
PRUICSS_loadFirmware(gPruHandle, PRUICSS_PRU0, ...);   /* master, auto-starts, never halts */

uint32_t commandWords[5] = { 0x1111, 0x2222, 0x3333, 0x4444, 0x5555 };
uint32_t responses[5];
if (PRU_SPI_runTransaction5(commandWords, responses)) {
    /* responses[0..4] hold the 5 words received during this burst */
}
/* Call PRU_SPI_runTransaction5() again for the next burst — no reload needed. */
```

All 8 (MODE x bit-order) combinations run the same 5x16-bit burst — PRU0's
runtime branch table (see "Configurable at runtime" below) now has a
dedicated 5-transfer subroutine per combination. PRU1 (the slave responder)
is still hardcoded to a single fixed mode/bit-order at build time — see
"PRU1 (slave)" below — so only the one combination PRU1 is built for will
produce a real round trip; the other 7 will still drive SCLK/SDO correctly
on the wire, but PRU1 won't be sampling in a matching mode.

**Load order matters.** PRU1 must load (and start polling CS) strictly before
PRU0 loads (and auto-starts) — otherwise PRU0 can complete its first transfer
before PRU1 is even watching for CS to go low.

**No inter-transfer delay, by design.** Within a single triggered burst, all
5 transfers run back-to-back with no gap — the last sampling edge of
transfer N is immediately followed by the shifting edge of transfer N+1's
first bit (a few cycles of `ldi`/branch overhead at most). SCLK toggles
continuously for the full 80-bit burst; there is no idle period between the
5 sub-transfers visible on a logic analyzer. This was an explicit
requirement (minimum/no delay from the SPI side), not something left
unoptimized.

**Response content is fixed, not decoded.** PRU1 doesn't inspect the 5
command words it receives — it always sends back the same 5 fixed response
words regardless of content (see "PRU1 (slave)" below). This sidesteps the
one-transaction pipeline delay that the earlier single-command version of
this driver had (where PRU1 decoded command N only after already sending
the response to command N-1) — since responses aren't decoded from input at
all in the 5x burst version, there's nothing to be stale.

### Configurable at runtime (via the driver API, no rebuild needed)

- **SPI mode** (0-3, i.e. CPOL/CPHA combination) — read once at PRU0 startup
- **Bit order** (MSB first / LSB first) — read once at PRU0 startup
- **5 command words** (via `PRU_SPI_runTransaction5`, once per burst — not a
  one-time config value like mode/bitOrder)

### Fixed at firmware build time (edit `firmware/am261x-lp/icss_m1_pru0_fw/main.asm` and rebuild)

- **Packet size** — fixed at 16 bits (`PACKET_SIZE`). The underlying macros
  (`spi_master_macros.inc`/`spi_slave_macros.inc`) enforce **1-32 bits** at
  assemble time (`.if (PACKETSIZE > 32) | (PACKETSIZE < 1) .emsg ...`) since
  data is shifted through a single 32-bit register — this is a hard ceiling
  for this bit-bang approach, not just this driver's current choice.
- **Delay compensation constants** `DELAY_COMPEN_1`/`DELAY_COMPEN_2` — set to
  8/27 (`d1+d2=35`), which targets **~5 MHz SCLK** at the AM261x PRU clock of
  225 MHz. `DELAY_COMPEN_1` should not be set below 8 (found unstable on this
  hardware); to hit a different frequency, keep `d1=8` and solve for `d2` from
  `(10 + d1 + d2) cycles/bit * 4.44ns = 1 / target_SCLK_freq`.
- **Pin assignments** (`SCLK_PIN`, `SDO_PIN`, `CS_PIN`, `SDI_PIN`)

Both PRU0 (master, via `m_transfer_packet_spi_master_gpo_sclk`) and PRU1
(slave, via `m_transfer_packet_spi_slave_gpi_sclk`) use the combined
full-duplex transfer macro, called 5 times per burst with no gap in between
— see "PRU1 (slave)" below for the fixed responses PRU1 sends back.

Mode and bit order can be selected at runtime because the underlying macro
(`m_transfer_packet_spi_master_gpo_sclk`) internally uses assembler directives
(`.if $symcmp(...)`) that only resolve against string literals at assemble
time — a runtime register value can't be passed in directly. The workaround
is an 8-way branch table: all 8 MODE x bit-order combinations are pre-assembled
as separate labeled blocks, and PRU0 reads `cfg_mode`/`cfg_bitorder` from its
own DMEM0 at startup and uses plain runtime branches (`qbeq`/`qba`) to jump to
the matching block. Packet size and delays are not part of this table (would
require a much larger branch table) and remain compile-time constants for this
prototype.

**PRU0 accesses its own DMEM0 via the constant table (`c24`, `lbco`/`sbco`)**,
whose default block index is already correctly pre-configured on this
hardware — no `PRUICSS_setConstantTblEntry` call is required. The R5F accesses
the same memory via its global/bus address (`0x48600000` for PRU0 DMEM0,
`0x48602000` for PRU1 DMEM1). These are two different address spaces for the
same physical memory — a PRU must never use the R5F's global address to access
its own DMEM via `sbbo`/`lbbo`.

### PRU1 (slave) — encoder-emulation responder

PRU1 is hardcoded (build-time constant, not configurable via the driver API)
to a single fixed mode/bit-order and emulates a simple fixed-response
encoder: within a 5-transfer burst it sends back 5 fixed 16-bit response
words, ignoring whatever command words it receives. It exists to give the
R5F demo something realistic to exercise the trigger mechanism against.

Currently built for **MODE3 / MSB first / 16-bit** (matches the R5F's
`PRU_SPI_setConfig` call in the usage example above). To test a different
mode/bit-order, rebuild PRU1's `main.asm` with a different mode/S_BIT string
in its `DO_5X_TRANSFER` subroutine and update the R5F's `PRU_SPI_setConfig`
call to match — PRU0 already supports all 8 combinations at runtime (see
"Driver Overview" above), only PRU1 needs a rebuild.

| Transfer # | Response word (fixed) |
| ---------- | ---------------------- |
| 1          | `0x1111`               |
| 2          | `0x2222`               |
| 3          | `0x3333`               |
| 4          | `0x4444`               |
| 5          | `0x5555`               |

PRU1 uses `m_transfer_packet_spi_slave_gpi_sclk` — the **combined**
full-duplex slave macro, which sends and samples every bit concurrently
within one CS-low burst.

**Confirmed MODE3 behavior on this driver** (from `spi_master_macros.inc`'s
own CPOL/CPHA table): SCLK idles **high**; data is **sampled** (received) on
the **rising** edge and **shifted out** (transmitted) on the **falling**
edge. Verified consistent on hardware with a logic analyzer.

## Hardware Setup (AM261x LaunchPad)

This demo needs PRU0 (master) wired to PRU1 (slave) on the LaunchPad header,
since both firmware images run on the same board.

**PRU0 (master) signals:**

| Signal | PRU pin       | SoC GPIO                  | LaunchPad header pin |
| ------ | ------------- | -------------------------- | --------------------- |
| SCLK   | r30.0 (GPO0)  | GPIO81 (PR1_PRU0_GPIO0)    | J2.11                 |
| SDO    | r30.1 (GPO1)  | GPIO82 (PR1_PRU0_GPIO1)    | J7.67                 |
| CS     | r30.2 (GPO2)  | GPIO83 (PR1_PRU0_GPIO2)    | J7.68                 |
| SDI    | r31.6 (GPI6)  | GPIO45 (PR1_PRU0_GPIO6)    | J7.69                 |

**PRU1 (slave, encoder-emulation responder) signals:**

| Signal | PRU pin        | SoC GPIO                   | LaunchPad header pin |
| ------ | -------------- | --------------------------- | --------------------- |
| SCLK   | r31.4  (GPI4)  | GPIO15 (PR1_PRU1_GPIO4)     | J1.5                  |
| CS     | r31.6  (GPI6)  | GPIO19 (PR1_PRU1_GPIO6)     | J5.48                 |
| SDO    | r30.5  (GPO5)  | GPIO54 (PR1_PRU1_GPIO5)     | J2.13                 |
| SDI    | r31.11 (GPI11) | GPIO3  (PR1_PRU1_GPIO11)    | J1.8                  |

**Wiring for the demo** — connect master to slave with jumper wires:

| Master pin | Slave pin | Signal        |
| ---------- | --------- | ------------- |
| J2.11 (SCLK) | J1.5 (SCLK) | Clock, master-driven |
| J7.68 (CS)   | J5.48 (CS)  | Chip select, master-driven, active low |
| J7.67 (SDO)  | J1.8 (SDI)  | Master -> slave data (command) |
| J7.69 (SDI)  | J2.13 (SDO) | Slave -> master data (response) |

All four wires are needed for this demo — unlike a simple loopback test, the
R5F command/response loop depends on actually receiving PRU1's response on
every transaction, not just verifying that a send occurred.

## Overview

For more information about using this example to write your own PRU firmware,
refer to
[Creating a New Project in the OpenPRU Repo](../../docs/open_pru_create_new_project.md).

## Steps to Run the Example

Refer to the **PRU Academy > PRU Getting Started Labs** for steps to build,
load, and debug PRU firmware.
