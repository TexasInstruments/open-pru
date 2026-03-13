# Double Float Multiply (64-bit IEEE 754) using PRU MAC Hardware

## Introduction

This example implements IEEE 754 double-precision (64-bit) floating-point
multiplication entirely in PRU assembly, using the PRU's 32-bit MAC broadside
accelerator to compute the critical 53-bit × 53-bit significand product.  It
demonstrates how hardware acceleration can replace the slow C-library software
floating-point path and provides a precise worst-case PRU cycle count.

A companion C reference (`firmware/main_c_ref.c`) shows the equivalent code
that the C compiler produces, where the `*` operator on `double` values is
lowered to a call into the software FP runtime library (`__mpy2d`).

## IEEE 754 Double-Precision Format

```
Bit  63     : sign  (S)
Bits 62:52  : biased exponent (E, bias = 1023)
Bits 51:0   : mantissa fraction (M)
Value = (−1)^S × 2^(E−1023) × 1.M   (normal numbers)
```

## Algorithm Overview

Given two doubles A and B stored in PRU Data RAM:

1. **Load** the 64-bit operands (two 32-bit words each) from PRU Data RAM.
2. **Sign**: `result_sign = sign_A XOR sign_B`
3. **Exponent**: `result_exp  = exp_A + exp_B − 1023`
4. **Restore significands**: Re-insert the implicit leading 1 bit at position 52.
   ```
   sigA_hi = (hi_A & 0xFFFFF) | 0x100000   (21-bit upper part)
   sigA_lo = lo_A                           (32-bit lower part)
   ```
5. **Significand product** using four 32×32→64 MAC operations:
   ```
   sigA = sigA_hi:sigA_lo  (21-bit : 32-bit)
   sigB = sigB_hi:sigB_lo

   p00 = sigA_lo × sigB_lo   contributes to product bits [63:0]
   p01 = sigA_hi × sigB_lo   contributes to product bits [95:32]
   p10 = sigA_lo × sigB_hi   contributes to product bits [95:32]
   p11 = sigA_hi × sigB_hi   contributes to product bits [127:64]
   ```
   Partial products are summed on-the-fly into the 106-bit accumulator
   `{pp3:pp2:pp1}` (product bits [105:32]) using `ADD`/`ADC` with
   immediate carry propagation.  `p00[31:0]` lies below the mantissa
   field and is discarded.

6. **Normalise**: Since sigA, sigB ∈ [2^52, 2^53), their product lies in
   [2^104, 2^106).  The leading 1 is therefore at product bit 104 or 105:
   - bit 105 clear → mantissa = P[103:52], keep `result_exp`
   - bit 105 set   → mantissa = P[104:53], `result_exp += 1`

7. **Pack and store**: Assemble `hi_result` and `lo_result` and write
   the 64-bit result back to PRU Data RAM.

## PRU Data RAM Memory Map

| Offset | Content                        |
|--------|-------------------------------|
| 0x00   | Operand A – low  word (bits 31:0)  |
| 0x04   | Operand A – high word (bits 63:32) |
| 0x08   | Operand B – low  word              |
| 0x0C   | Operand B – high word              |
| 0x10   | Result    – low  word (written)    |
| 0x14   | Result    – high word (written)    |

## PRU MAC Broadside Accelerator

The MAC unit (device ID 0) performs unsigned 32×32→64-bit multiplication in
hardware.  In multiply-only mode:

```asm
MOV   r28, operand_1          ; load first operand  (1 cycle)
MOV   r29, operand_2          ; load second operand (1 cycle), triggers multiply
NOP                            ; 1-cycle result latency
XIN   0, &r26, 4              ; read result[31:0]   (1 cycle)
XIN   0, &r27, 4              ; read result[63:32]  (1 cycle)
```

**Cost per hardware multiply: 5 cycles.**  Four multiplies are therefore
completed in 20 cycles, compared to a software 32×32 multiply which requires
many more instructions without hardware support.

## Worst-Case PRU Cycle Count

| Section                                             | Cycles |
|-----------------------------------------------------|--------|
| Prologue: init + load + sign/exp + significand build | 28     |
| 4 × MAC with on-the-fly carry accumulation           | 32     |
| Normalise branch (`QBBS`)                            |  1     |
| Normalise path (`do_shift`, worst case)              |  9     |
| Round-to-nearest check + adjust (worst case)         |  6     |
| Pack result                                          |  4     |
| Store + `HALT`                                       |  4     |
| **Total (worst case)**                               | **85** |

**Maximum PRU cycle count: 85 cycles**

At 200 MHz PRU clock → **425 ns** per double-precision multiply.

> **Rounding mode:** The implementation uses *round to nearest, ties away from zero*.
> The guard bit is taken from the bit immediately below the 52-bit mantissa field
> (P[51] in the no-shift case, P[52] in the shift case).  This matches IEEE 754
> round-to-nearest behaviour for all inputs except exact half-way cases, which
> are extremely rare in practice.

## Comparison with C Library

| Implementation                         | Cycle count (worst case) |
|----------------------------------------|--------------------------|
| Assembly + MAC hardware (this example) | ~85 cycles               |
| C library `__mpy2d` (software FP)      | ~200+ cycles             |

The C compiler for PRU has no hardware FPU to target, so the `*` operator on
`double` values is compiled to a call to the `__mpy2d` runtime routine, which
performs a purely software multiplication.  See `firmware/main_c_ref.c` for
the equivalent C source.

## Register Map

| Register | Role |
|----------|------|
| R0       | Scratch / temporary |
| R1       | Memory base address (0) |
| R2       | `lo_A` — operand A bits [31:0] |
| R3       | `hi_A` — operand A bits [63:32] |
| R4       | `lo_B` |
| R5       | `hi_B` |
| R6       | `sigA_lo` — lower 32 bits of significand A |
| R7       | `sigA_hi` — upper 21 bits of significand A |
| R8       | `sigB_lo` |
| R9       | `sigB_hi` |
| R10      | `expA` — 11-bit biased exponent of A |
| R11      | `expB` |
| R12      | `res_sign` — result sign |
| R13      | `res_exp`  — result biased exponent |
| R22      | `pp1` — product bits [63:32] |
| R23      | `pp2` — product bits [95:64] |
| R24      | `pp3` — product bits [105:96] |
| R25      | MAC control register |
| R26      | MAC result low  32 bits |
| R27      | MAC result high 32 bits |
| R28      | MAC operand 1 |
| R29      | MAC operand 2 |

## Special Cases

This implementation handles **normal numbers only**.  The following cases are
not handled in order to keep the focus on the MAC acceleration technique:

- NaN (Not-a-Number)
- Infinity (±Inf)
- Denormalised (subnormal) numbers
- Zero (±0)

For production use these cases should be detected and handled prior to the
main multiplication path.

## Supported Combinations

| Parameter      | Value |
|----------------|-------|
| ICSSM          | ICSSM0 – PRU0, PRU1; ICSSM1 (only in am261x) – PRU0, PRU1 |
| Toolchain      | pru-cgt |
| Board          | am261x-lp, am261x-som, am263px-cc, am263px-lp, am263x-cc, am263x-lp |
| Example folder | examples/double_float |
