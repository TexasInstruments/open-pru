; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

;******************************************************************************
;   File:     main.asm
;
;   Brief:    Measures the throughput of the PRU MAC broadside accelerator in
;             multiply-only mode, the cost of turning its unsigned product
;             into a signed (and Q31) product, and the operand-to-XIN
;             latency. The MAC result is cross-checked on the same core
;             against a shift/add multiply that does not use the MAC.
;
;   Hardware: AM261x (PRU-ICSSM). Runs on PRU0 or PRU1, standalone, no host
;             code. The result record is written to the core's own data RAM.
;
;   Steps to build:
;             - CCS: import the PRU project and build it
;             - makefile: run "make" in the project directory
;
;   Runs (in order, all from the same operand recurrence):
;             1. MAC unsigned, timed       TIMED_ITERATIONS products
;             2. MAC unsigned, cross-check XCHECK_ITERATIONS products
;             3. shift/add, timed          XCHECK_ITERATIONS products
;             4. MAC + signed fixup, timed TIMED_ITERATIONS products
;             5. MAC + signed + Q31, timed TIMED_ITERATIONS products
;             6. MAC + signed + Q31, XOR checksum of every product,
;                XCHECK_ITERATIONS products
;             7. R29 written right before XIN (no spacing), XCHECK_ITERATIONS
;             8. R28 written right before XIN (no spacing), XCHECK_ITERATIONS
;
;   Unsigned operand recurrence, runs 1-3 and 7-8 (k = N down to 1):
;             (start code)
;             a = SEED_A; b = SEED_B;
;             for (k = N; k > 0; k--) {
;                 a ^= k;  b += B_STEP;  product = a * b;
;             }
;             (end code)
;   Signed operand recurrence, runs 4-6. Both operands change sign often,
;   so all four sign combinations go through the fixup:
;             (start code)
;             a = SEED_A; b = SEED_B;
;             for (k = N; k > 0; k--) {
;                 a += SIGNED_STEP_A;  b += SIGNED_STEP_B;
;                 product = (int32_t)a * (int32_t)b;
;             }
;             (end code)
;
;   Timing: each timed run is bracketed by two reads of the core's own
;   CYCLE register. The record also holds the cost of an empty bracket
;   (two back-to-back reads) so the host can subtract it. Results are in
;   PRU cycles, so they do not depend on the PRU clock; only a conversion
;   to ns does. Every instruction in the timed loops takes one cycle,
;   including XIN from the MAC and the QBNE back edge (taken or not), so
;   each run's budget is its instruction count per product times the
;   iteration count; the budget is given next to each timed loop. There is
;   no deadline: the lab measures what a loop costs, it does not have to
;   meet a period.
;
;   Result record (own data RAM, byte offsets from RECORD_BASE):
;       0x00  magic 0x4d414354 "MACT", cleared at start, written last
;       0x04  empty bracket cycles
;       0x08  run 1: iterations, cycles, product low, product high
;       0x18  run 2: iterations, product low, product high
;       0x24  run 3: iterations, cycles, product low, product high
;       0x34  run 4: iterations, cycles, product low, product high
;       0x44  run 5: iterations, cycles, Q31 result
;       0x50  run 6: iterations, XOR of product low words, XOR of product
;             high words, XOR of Q31 results
;       0x60  run 7: iterations, product low, product high
;       0x6C  run 8: iterations, product low, product high
;   utilities/mac_throughput_check.py decodes and checks the record.
;******************************************************************************

; CCS/makefile specific settings
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

    .global     main
    .global     FN_SHIFT_ADD_MUL64
    .sect       ".text"

    .include "icss_regs.inc"
    .include "icss_xfer_defines.inc"

; Defines
    .asg    0,              MULTIPLY_ONLY       ; R25.b0 value: no accumulate
    .asg    3,              COUNTER_ENABLE_BIT  ; PRU CONTROL.COUNTER_ENABLE
    ; Products per timed run (runs 1, 4, 5). 65536 resolves the cost per
    ; product to 1/65536 of a cycle, so a single stall cycle anywhere in a
    ; run shows as a non-integer cycles-per-product. The longest run (5, 16
    ; cycles per product) is 1,048,576 cycles, far below the 32-bit CYCLE
    ; limit, and all eight runs finish in about 2.6 million cycles.
    .asg    65536,          TIMED_ITERATIONS
    ; Products per cross-check, shift/add and latency run (runs 2, 3, 6-8).
    ; 1024 products step k through every value of its low 10 bits in the
    ; a ^= k recurrence and give the signed runs all four operand sign
    ; combinations (the host checker asserts both). The shift/add multiply
    ; costs 364 cycles per product, so run 3 stays at 372,736 cycles.
    .asg    1024,           XCHECK_ITERATIONS
    .asg    0x13579bdf,     SEED_A              ; first R28 value
    .asg    0x2468ace1,     SEED_B              ; first R29 value
    .asg    0x11,           B_STEP              ; R29 increment per product
    .asg    0x9e3779b9,     SIGNED_STEP_A       ; R28 increment, signed runs
    .asg    0x7f4a7c15,     SIGNED_STEP_B       ; R29 increment, signed runs
    .asg    0x4d414354,     RECORD_MAGIC        ; "MACT"

; Record layout (byte offsets). The record is 0x78 bytes and lives at the
; start of the core's own data RAM; nothing else is linked into data RAM.
    .asg    0x0000,         RECORD_BASE
    .asg    0x00,           REC_MAGIC
    .asg    0x04,           REC_BRACKET
    .asg    0x08,           REC_MAC_TIMED       ; iter, cycles, lo, hi
    .asg    0x18,           REC_MAC_XCHECK      ; iter, lo, hi
    .asg    0x24,           REC_SHIFT_ADD       ; iter, cycles, lo, hi
    .asg    0x34,           REC_SIGNED          ; iter, cycles, lo, hi
    .asg    0x44,           REC_Q31             ; iter, cycles, q31
    .asg    0x50,           REC_SIGNED_SUM      ; iter, lo, hi, q31
    .asg    0x60,           REC_LATE_B          ; iter, lo, hi
    .asg    0x6C,           REC_LATE_A          ; iter, lo, hi
    .asg    0x78,           RECORD_SIZE

; Control module of the core this firmware runs on, in the PRU local address
; map. On PRU-ICSSM 0x22000 is PRU0's and 0x24000 is PRU1's for both cores,
; so the core makefile / projectspec defines PRU0 or PRU1 to pick it.
    .if $isdefed("PRU1")
    .asg    ICSS_CNTL1_BASE, OWN_CNTL_BASE
    .else
    .asg    ICSS_CNTL0_BASE, OWN_CNTL_BASE
    .endif

    .if (RECORD_BASE + RECORD_SIZE) > 0x2000
    .emsg "Result record does not fit in the 8 KB PRU data RAM"
    .endif

; Register usage
;   r0          RECORD_BASE (store base)
;   r1, r2      scratch for record stores
;   r3.w2       return address for FN_SHIFT_ADD_MUL64
;   r4 - r11    FN_SHIFT_ADD_MUL64 working registers, r7:r6 = its result
;   r12, r13    signed fixup scratch, Q31 result
;   r14, r15    SIGNED_STEP_A, SIGNED_STEP_B (signed runs)
;   r16 - r18   run 6 checksums: product low, product high, Q31
;   r20         loop counter k
;   r21         copy of k (run 8 only)
;   r22, r23    CYCLE at start / end of a timed run
;   r24         OWN_CNTL_BASE, this core's control module
;   r25 - r29   MAC interface: r25 control, r27:r26 product, r28/r29 operands

;******************************************************************************
;   Macro: m_cycle_read
;
;   Reads the core's own CYCLE register.
;
;   PEAK cycles: one LBBO from the PRU control module (measured on silicon
;   as part of the empty bracket, record offset REC_BRACKET)
;
;   Parameters:
;       dst - register that receives CYCLE
;
;   Assumptions:
;       - r24 holds OWN_CNTL_BASE
;
;   Inputs:    r24
;   Outputs:   dst
;   Clobbers:  none
;   Preserves: every register other than dst
;******************************************************************************
m_cycle_read .macro dst
    lbbo    &dst, r24, PRUx_CNTL_CYCLE_COUNT_OFFSET, 4
    .endm

;******************************************************************************
;   Macro: m_mac_signed_fixup
;
;   Turns the unsigned 64-bit MAC product R27:R26 = R28 * R29 into the
;   signed product of the same operands read as int32. Branch-free, so it
;   costs the same for every operand value.
;
;   Pseudo code:
;       (start code)
;       if (R28 < 0) R27 -= R29;
;       if (R29 < 0) R27 -= R28;
;       (end code)
;
;   PEAK cycles: 8
;
;   Assumptions:
;       - R27:R26 holds the product of the current R28 and R29
;
;   Inputs:    r26, r27 (unsigned product), r28, r29 (operands)
;   Outputs:   r27:r26 (signed product; only r27 is written)
;   Clobbers:  r12
;   Preserves: every register other than r12 and r27
;******************************************************************************
m_mac_signed_fixup .macro
    lsr     r12, r28, 31            ; sign of a: 0 or 1
    rsb     r12, r12, 0             ; 0 or 0xffffffff
    and     r12, r12, r29
    sub     r27, r27, r12           ; a < 0: high word -= b
    lsr     r12, r29, 31            ; sign of b
    rsb     r12, r12, 0
    and     r12, r12, r28
    sub     r27, r27, r12           ; b < 0: high word -= a
    .endm

;******************************************************************************
;   Macro: m_q31_from_product
;
;   Extracts the Q31 result of a Q31 x Q31 multiply from the signed Q62
;   product in R27:R26 by truncation (product >> 31). There is no
;   saturation: 0x80000000 x 0x80000000 (-1.0 x -1.0) wraps to 0x80000000.
;
;   Pseudo code:
;       (start code)
;       dst = (R27 << 1) | (R26 >> 31);
;       (end code)
;
;   PEAK cycles: 3
;
;   Parameters:
;       dst - register that receives the Q31 result
;
;   Inputs:    r26, r27 (signed product)
;   Outputs:   dst
;   Clobbers:  r12
;   Preserves: every register other than dst and r12
;******************************************************************************
m_q31_from_product .macro dst
    lsl     dst, r27, 1
    lsr     r12, r26, 31
    or      dst, dst, r12
    .endm

;******************************************************************************
;   Macro: m_store_timed
;
;   Stores iterations, cycles and the 64-bit product of a timed run.
;
;   PEAK cycles: 2 (LDI32) + 1 (SUB) + 2 SBBO to own data RAM
;
;   Parameters:
;       offset     - record offset of the run
;       iterations - number of products in the run
;       lo_reg     - register holding the product low word (high word in
;                    the next register)
;
;   Inputs:    r0 (record base), r22, r23 (CYCLE at start / end), lo_reg
;              and the register after it
;   Outputs:   none in registers; the run is written to the record
;   Clobbers:  r1, r2
;   Preserves: every register other than r1 and r2
;******************************************************************************
m_store_timed .macro offset, iterations, lo_reg
    ldi32   r1, iterations
    sub     r2, r23, r22
    sbbo    &r1, r0, offset, 8
    sbbo    &lo_reg, r0, offset + 8, 8
    .endm

;********
;* MAIN *
;********

main:
    ; Clear R0 - R29. R30 and R31 are not used.
    zero    &r0, 120
    ldi     r0, RECORD_BASE

    ; Invalidate a record left in data RAM by an earlier execution, so a
    ; reader cannot take it for this one while the runs are in progress.
    ; The magic is written again, last, once every field is stored.
    sbbo    &r1, r0, REC_MAGIC, 4   ; r1 is zero

    ; Multiply-only mode
    ldi     r25.b0, MULTIPLY_ONLY
    xout    MAC_XID, &r25.b0, 1

    ; Reset and start the CYCLE counter. CYCLE is only writable while the
    ; counter is disabled.
    ldi32   r24, OWN_CNTL_BASE
    lbbo    &r1, r24, PRUx_CNTL_CONTROL_OFFSET, 4
    clr     r1, r1, COUNTER_ENABLE_BIT
    sbbo    &r1, r24, PRUx_CNTL_CONTROL_OFFSET, 4
    ldi     r2, 0
    sbbo    &r2, r24, PRUx_CNTL_CYCLE_COUNT_OFFSET, 4
    set     r1, r1, COUNTER_ENABLE_BIT
    sbbo    &r1, r24, PRUx_CNTL_CONTROL_OFFSET, 4

    ; Empty bracket: the cost of the two CYCLE reads around every run
    m_cycle_read r22
    m_cycle_read r23
    sub     r1, r23, r22
    sbbo    &r1, r0, REC_BRACKET, 4

;------------------------------------------------------------------------------
; Run 1: MAC unsigned, timed. Five instructions per product.
; Budget: 5 cycles x TIMED_ITERATIONS = 327,680 cycles inside the bracket.
;
; The MAC samples its operands one instruction late: the last write to R28
; or R29 must be at least one instruction before the XIN that reads the
; product, otherwise XIN returns the product of the previous operand value.
; The loop counter decrement fills that slot, so the spacing costs nothing.
; Runs 7 and 8 below show what XIN returns without the spacing.
;------------------------------------------------------------------------------
    ldi32   r28, SEED_A
    ldi32   r29, SEED_B
    ldi32   r20, TIMED_ITERATIONS
    m_cycle_read r22
MAC_TIMED_LOOP:
    xor     r28, r28, r20           ; new operand a
    add     r29, r29, B_STEP        ; new operand b, last operand write
    sub     r20, r20, 1             ; spacing slot, and the loop counter
    xin     MAC_XID, &r26, 8        ; R27:R26 = a * b (unsigned)
    qbne    MAC_TIMED_LOOP, r20, 0
    m_cycle_read r23
    m_store_timed REC_MAC_TIMED, TIMED_ITERATIONS, r26

;------------------------------------------------------------------------------
; Run 2: MAC unsigned, untimed cross-check. Same loop, XCHECK_ITERATIONS
; products. Run 3 must end on the same product without using the MAC.
;------------------------------------------------------------------------------
    ldi32   r28, SEED_A
    ldi32   r29, SEED_B
    ldi32   r20, XCHECK_ITERATIONS
MAC_XCHECK_LOOP:
    xor     r28, r28, r20
    add     r29, r29, B_STEP
    sub     r20, r20, 1
    xin     MAC_XID, &r26, 8
    qbne    MAC_XCHECK_LOOP, r20, 0
    ldi32   r1, XCHECK_ITERATIONS
    sbbo    &r1, r0, REC_MAC_XCHECK, 4
    sbbo    &r26, r0, REC_MAC_XCHECK + 4, 8

;------------------------------------------------------------------------------
; Run 3: shift/add multiply, timed. Same recurrence, no MAC.
; Budget: 4 loop instructions + 360 for FN_SHIFT_ADD_MUL64 including its JAL
; = 364 cycles x XCHECK_ITERATIONS = 372,736 cycles inside the bracket.
;------------------------------------------------------------------------------
    ldi32   r28, SEED_A
    ldi32   r29, SEED_B
    ldi32   r20, XCHECK_ITERATIONS
    m_cycle_read r22
SHIFT_ADD_LOOP:
    xor     r28, r28, r20
    add     r29, r29, B_STEP
    sub     r20, r20, 1
    jal     r3.w2, FN_SHIFT_ADD_MUL64 ; r7:r6 = a * b (unsigned)
    qbne    SHIFT_ADD_LOOP, r20, 0
    m_cycle_read r23
    m_store_timed REC_SHIFT_ADD, XCHECK_ITERATIONS, r6

;------------------------------------------------------------------------------
; Run 4: MAC + signed fixup, timed. Signed recurrence, operands read as int32.
; Budget: 5 loop instructions + 8 for the fixup = 13 cycles x
; TIMED_ITERATIONS = 851,968 cycles inside the bracket.
;------------------------------------------------------------------------------
    ldi32   r14, SIGNED_STEP_A
    ldi32   r15, SIGNED_STEP_B
    ldi32   r28, SEED_A
    ldi32   r29, SEED_B
    ldi32   r20, TIMED_ITERATIONS
    m_cycle_read r22
SIGNED_LOOP:
    add     r28, r28, r14
    add     r29, r29, r15
    sub     r20, r20, 1
    xin     MAC_XID, &r26, 8
    m_mac_signed_fixup              ; R27:R26 = (int32)a * (int32)b
    qbne    SIGNED_LOOP, r20, 0
    m_cycle_read r23
    m_store_timed REC_SIGNED, TIMED_ITERATIONS, r26

;------------------------------------------------------------------------------
; Run 5: MAC + signed fixup + Q31 extraction, timed. Signed recurrence.
; Budget: 13 + 3 for the Q31 extraction = 16 cycles x TIMED_ITERATIONS
; = 1,048,576 cycles inside the bracket.
;------------------------------------------------------------------------------
    ldi32   r28, SEED_A
    ldi32   r29, SEED_B
    ldi32   r20, TIMED_ITERATIONS
    m_cycle_read r22
Q31_LOOP:
    add     r28, r28, r14
    add     r29, r29, r15
    sub     r20, r20, 1
    xin     MAC_XID, &r26, 8
    m_mac_signed_fixup
    m_q31_from_product r13          ; r13 = Q31 a * b
    qbne    Q31_LOOP, r20, 0
    m_cycle_read r23
    ldi32   r1, TIMED_ITERATIONS
    sub     r2, r23, r22
    sbbo    &r1, r0, REC_Q31, 8
    sbbo    &r13, r0, REC_Q31 + 8, 4

;------------------------------------------------------------------------------
; Run 6: signed and Q31 checksum, untimed. Runs 4 and 5 only store the last
; product; this run folds every signed product and Q31 result into an XOR
; checksum so the host checks all XCHECK_ITERATIONS of them.
;------------------------------------------------------------------------------
    ldi32   r28, SEED_A
    ldi32   r29, SEED_B
    ldi32   r20, XCHECK_ITERATIONS
    ldi     r16, 0
    ldi     r17, 0
    ldi     r18, 0
SIGNED_SUM_LOOP:
    add     r28, r28, r14
    add     r29, r29, r15
    sub     r20, r20, 1
    xin     MAC_XID, &r26, 8
    m_mac_signed_fixup
    m_q31_from_product r13
    xor     r16, r16, r26
    xor     r17, r17, r27
    xor     r18, r18, r13
    qbne    SIGNED_SUM_LOOP, r20, 0
    ldi32   r1, XCHECK_ITERATIONS
    sbbo    &r1, r0, REC_SIGNED_SUM, 4
    sbbo    &r16, r0, REC_SIGNED_SUM + 4, 12

;------------------------------------------------------------------------------
; Run 7: R29 written by the instruction right before XIN (no spacing).
; Deliberately wrong usage, kept to show the operand latency on silicon.
;------------------------------------------------------------------------------
    ldi32   r28, SEED_A
    ldi32   r29, SEED_B
    ldi32   r20, XCHECK_ITERATIONS
LATE_B_LOOP:
    xor     r28, r28, r20
    sub     r20, r20, 1
    add     r29, r29, B_STEP        ; written right before XIN
    xin     MAC_XID, &r26, 8
    qbne    LATE_B_LOOP, r20, 0
    ldi32   r1, XCHECK_ITERATIONS
    sbbo    &r1, r0, REC_LATE_B, 4
    sbbo    &r26, r0, REC_LATE_B + 4, 8

;------------------------------------------------------------------------------
; Run 8: R28 written by the instruction right before XIN (no spacing).
; r21 keeps k before the decrement so the recurrence is unchanged.
;------------------------------------------------------------------------------
    ldi32   r28, SEED_A
    ldi32   r29, SEED_B
    ldi32   r20, XCHECK_ITERATIONS
LATE_A_LOOP:
    mov     r21, r20
    add     r29, r29, B_STEP
    sub     r20, r20, 1
    xor     r28, r28, r21           ; written right before XIN
    xin     MAC_XID, &r26, 8
    qbne    LATE_A_LOOP, r20, 0
    ldi32   r1, XCHECK_ITERATIONS
    sbbo    &r1, r0, REC_LATE_A, 4
    sbbo    &r26, r0, REC_LATE_A + 4, 8

    ; Publish the record: magic last, so a reader never sees a partial one
    ldi32   r1, RECORD_MAGIC
    sbbo    &r1, r0, REC_MAGIC, 4

    halt
