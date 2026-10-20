# MAC Throughput

## What this lab adds

**Board-tested on LP-AM261, ICSSM0 PRU0 and PRU1:** 20 executions per core,
with identical cycle counts in all 40 executions and 50/50 validation checks
passing. The measured PRU clock was 249.9 MHz.

- **Measured DSP building-block costs:** unsigned MAC products take 5 cycles,
  signed products take 13, and signed products with Q31 extraction take 16.
  These are complete loop costs, including operand updates and loop control,
  not the MAC accelerator's standalone latency.
- **A software baseline and independent cross-check:** a branch-free
  shift/add multiply takes 364 cycles per product, 72.8 times the unsigned
  MAC loop cost. Host integer arithmetic, MAC and shift/add results agree.
  This is a comparison of these loops, not an end-to-end application speedup.
- **A hardware operand-latency demonstration:** two deliberately unspaced
  operand writes reproduce stale-operand products on both cores. The working
  loop puts a useful instruction between the last operand write and XIN.
- **Complementary to the existing examples:** the MAC labs show accelerator
  usage and `examples/fft` provides an FFT application. This lab isolates the
  per-product timing and signed/Q31 costs needed to budget DSP kernels; it
  does not add another FFT implementation or claim FFT/EDMA offload speedup.

See [Results](#results) for the measurements and
[Steps to Run the Example](#steps-to-run-the-example) to reproduce them.
PRU0 and PRU1 were tested sequentially, not concurrently.

## Introduction

This lab measures the PRU MAC broadside accelerator in multiply-only mode on
real hardware:

- how many PRU cycles one 32 x 32 -> 64-bit product costs in a tight loop
- what a signed (int32 x int32) product and a Q31 x Q31 -> Q31 result cost
  on top of the unsigned product the MAC returns
- why the last write to an operand register (R28 or R29) has to be at least
  one instruction before the XIN that reads the product
- how much faster the MAC is than a multiply built from shift and add

Every product the firmware computes is checked bit-exactly, and the MAC result
is cross-checked on the same core against a shift/add multiply that does not
use the MAC at all.

`mac/mac_multiply` shows how to use multiply-only mode once and `mac/mac`
shows multiply and accumulate. This lab builds on them: it times the MAC in a
loop, adds the signed and Q31 handling that fixed-point DSP code (for example,
an FFT butterfly) needs, and demonstrates the operand latency on silicon.
For a complete FFT on the PRU, see `examples/fft`.

## Overview

The firmware (`firmware/main.asm`) does eight runs over the same operand
sequences, then writes a result record to the core's own data RAM and halts.
Timed runs are bracketed by two reads of the core's CYCLE register; the cost
of an empty bracket is recorded too, so it can be subtracted.

| Run | What it does | Products | Timed |
| ---- | ------------ | -------- | ----- |
| 1 | MAC, unsigned | 65536 | yes |
| 2 | MAC, unsigned (cross-check) | 1024 | no |
| 3 | shift/add multiply, no MAC (`firmware/shift_add_mul64.asm`) | 1024 | yes |
| 4 | MAC + signed fixup | 65536 | yes |
| 5 | MAC + signed fixup + Q31 extraction | 65536 | yes |
| 6 | as run 5, XOR checksum of every signed product and Q31 result | 1024 | no |
| 7 | R29 written by the instruction right before XIN | 1024 | no |
| 8 | R28 written by the instruction right before XIN | 1024 | no |

The timed MAC loop is five instructions per product:

```
MAC_TIMED_LOOP:
    xor     r28, r28, r20           ; new operand a
    add     r29, r29, B_STEP        ; new operand b, last operand write
    sub     r20, r20, 1             ; spacing slot, and the loop counter
    xin     MAC_XID, &r26, 8        ; R27:R26 = a * b (unsigned)
    qbne    MAC_TIMED_LOOP, r20, 0
```

The cycles per product therefore include operand update and loop control,
not only the MAC: they are what a loop that produces one product per
iteration actually costs.

### Signed and Q31 products

The MAC multiplies unsigned operands. For operands that are int32, the
signed product differs from the unsigned one only in the high word:

```
if (a < 0) high -= b;
if (b < 0) high -= a;
```

`m_mac_signed_fixup` does this without branches (8 instructions), so it costs
the same for every operand. `m_q31_from_product` then takes bits 62..31 of the
signed product (3 instructions), which is the truncated Q31 result of a
Q31 x Q31 multiply. There is no saturation: -1.0 x -1.0
(0x80000000 x 0x80000000) wraps to 0x80000000.

Runs 4 to 6 use a second operand sequence in which both operands change
sign often, so all four sign combinations go through the fixup.

### Operand latency: why one instruction of spacing

The MAC samples its operands one instruction late. If R28 or R29 is written
by the instruction right before the XIN, XIN returns the product computed
with the previous value of that register. The last operand write must
therefore be at least one instruction before the XIN. The timed loop uses the
loop counter decrement for that slot, so it costs nothing; where there is no
useful instruction to put there, use a NOP (as `mac/mac_multiply` and
`examples/fft` do).

Runs 7 and 8 leave the spacing out on purpose. The host checker predicts
what they return under a one-instruction latency (new R28 with the previous
R29 for run 7, previous R28 with the new R29 for run 8) and checks the
hardware against that prediction. Runs 1 and 2, which have the spacing,
must return the correct product. An instruction-set simulator that models the
MAC with zero latency returns the correct product for runs 7 and 8 too,
which is why this is measured on hardware.

## Results

Measured on LP-AM261, ICSSM0 PRU0 and PRU1, 20 executions of the firmware per
core. The cycle counts were identical in every execution on both cores. PRU
clock measured from the CYCLE counter against the host wall clock:
249.9 MHz.

| Run | Cycles per product | ns per product at 249.9 MHz |
| ---- | ------------------ | --------------------------- |
| 1, MAC unsigned | 5.0 | 20.0 |
| 4, MAC + signed fixup | 13.0 | 52.0 |
| 5, MAC + signed fixup + Q31 | 16.0 | 64.0 |
| 3, shift/add, no MAC | 364.0 | 1456.6 |

- empty bracket (two CYCLE reads): 4 cycles, subtracted above
- signed fixup: 8 cycles per product, Q31 extraction: 3 more
- shift/add vs MAC, unsigned: 72.8 times the cycles
- all cycle counts match the hand count of the loops (shift/add:
  4 loop instructions + 360 for `FN_SHIFT_ADD_MUL64` including JAL)
- every product, both checksums, and the run 7 and run 8 values matched the
  host model, and the MAC cross-check (run 2) matched the shift/add result
  (run 3), in all 40 executions

## Supported Combinations

Refer to open-pru/academy/readme.md > Supported processors per-project
for the list of processors that support building this project, and information
about porting this project to other processors.

## Validated HW & SW

This project was tested on hardware with these software versions:

| Processor | Hardware | Software |
| --------- | -------- | -------- |
| am261x | LP-AM261 (AM2612, HS-FS), board revision FIXME | PRU CGT 2.3.3, CCS 21.0, OpenPRU main (75f90c94) + this project |

The firmware is standalone: it needs no MCU+ code. On the LP-AM261 the AM261x
GEL file, which runs when CCS connects to the R5F core, enables the ICSSM
clocks.

## Steps to Run the Example

1. Build the firmware and load it on ICSSM0 PRU0 or PRU1. Each core has its
   own project because the address of the core's control module (and so of
   its CYCLE register) differs between PRU0 and PRU1.
2. Run the core. It halts by itself after about 2.6 million cycles (about
   11 ms at 250 MHz).
3. Read 0x78 bytes from the start of the core's own data RAM: SoC address
   0x48000000 for ICSSM0 PRU0, 0x48002000 for ICSSM0 PRU1. Save them as a
   raw binary file, or as text with one 32-bit word per line (hex), offset
   0x00 first.
4. Check the record on the host (Python 3, no extra packages):

   ```
   python utilities/mac_throughput_check.py --record <file> --clock-mhz 250
   ```

   It prints the cycles per product of each timed run and `PASS` when every
   value matches the model. The first word reads 0x4d414354 ("MACT") once
   the firmware has finished.
5. Optional: `python utilities/mac_throughput_check.py --selftest` checks the
   model (signed fixup and Q31 extraction against Python integer arithmetic
   on 100,000 operand pairs, shift/add model against `a * b`) and runs six
   negative tests that corrupt a good record and confirm the checker rejects
   each one.

The record layout is documented at the top of `firmware/main.asm`.
