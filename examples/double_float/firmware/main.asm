; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2024-2025 Texas Instruments Incorporated - http://www.ti.com/

;*******************************************************************************
;   File:     main.asm
;
;   Brief:    IEEE 754 double-precision (64-bit) floating-point multiplication
;             using the PRU MAC broadside accelerator.  The 53-bit × 53-bit
;             significand product is computed via four 32×32→64-bit hardware
;             multiply operations, avoiding the slow software FP library path.
;
;   Steps to build:
;     - Using CCS:
;         Import PRU project, build → generates .out and .h files
;     - Using makefile:
;         gmake -all
;
;   IEEE 754 double (64-bit) format:
;     bit  63     : sign  (S)
;     bits 62:52  : biased exponent (E, bias = 1023)
;     bits 51:0   : mantissa fraction (M)
;     value = (-1)^S × 2^(E−1023) × 1.M   (for normal numbers)
;
;   Algorithm:
;     Given operands A and B stored in PRU Data RAM:
;       1.  Load hi/lo words of A and B from memory
;       2.  Restore the 53-bit significands: sigA = 1.M_A, sigB = 1.M_B
;       3.  result_sign = signA XOR signB
;       4.  result_exp  = expA + expB − 1023
;       5.  Multiply the 53-bit significands with four 32-bit MAC operations:
;
;             sigA = sigA_hi : sigA_lo   (21-bit upper, 32-bit lower)
;             sigB = sigB_hi : sigB_lo
;
;             p00 = sigA_lo × sigB_lo        position [63:0]
;             p01 = sigA_hi × sigB_lo        position [95:32]
;             p10 = sigA_lo × sigB_hi        position [95:32]
;             p11 = sigA_hi × sigB_hi        position [127:64]
;
;             106-bit product P accumulated in {pp3:pp2:pp1}:
;               pp1 = P[63:32]
;               pp2 = P[95:64]
;               pp3 = P[105:96]
;
;       6.  Normalise: P ∈ [2^104, 2^106), leading 1 at bit 104 or 105
;             bit 105 clear → mantissa = P[103:52], keep exp
;             bit 105 set   → mantissa = P[104:53], exp += 1
;       7.  Pack and store result double to PRU Data RAM
;
;   PRU Data RAM memory map (byte offsets from address 0x0000):
;     0x00  operand A  low  word  (bits 31:0)
;     0x04  operand A  high word  (bits 63:32)
;     0x08  operand B  low  word
;     0x0C  operand B  high word
;     0x10  result     low  word  (written by this code)
;     0x14  result     high word
;
;   MAC unit per-operation cost (multiply-only mode):
;     MOV R28, op1   (1 cy) — load operand 1
;     MOV R29, op2   (1 cy) — load operand 2, triggers hardware multiply
;     NOP            (1 cy) — 1-cycle result latency
;     XIN &R26, 4    (1 cy) — read result[31:0]
;     XIN &R27, 4    (1 cy) — read result[63:32]
;
;   Worst-case PRU cycle count : 85 cycles  (200 MHz clock → 425 ns)
;     Prologue (init + load + sign/exp + significand build) : 28 cy
;     4 × MAC with on-the-fly carry accumulation            : 32 cy
;     Normalise check (QBBS)                                :  1 cy
;     Normalise path (do_shift, worst case)                 :  9 cy
;     Round-to-nearest (round up + exponent overflow, rare) :  6 cy
;     Pack result                                           :  4 cy
;     Store + HALT                                          :  4 cy
;     ──────────────────────────────────────────────────────────────
;     NOTE: Round-to-nearest uses the "ties away from zero" mode.
;     The rounding guard bit is taken from the bit immediately below
;     the 52-bit mantissa field.  Sticky bits (lower product bits) are
;     not examined, matching standard IEEE 754 round-to-nearest behaviour
;     for all but exact-midpoint cases.
;
;   No-special-case handling: NaN, Infinity, denormals and zero are not
;   handled to keep focus on the MAC acceleration demonstration.
;   See main_c_ref.c for the equivalent C library code.
;
;*******************************************************************************

; CCS/makefile specific settings
    .retain
    .retainrefs

    .global  main
    .sect    ".text"

; ── MAC broadside peripheral constants ───────────────────────────────────────
    .asg  0x0,   DEVICE_ID       ; MAC broadside device ID (device 0)
    .asg  0x0,   MULTIPLY_ONLY   ; R25 control word: multiply-only mode

; ── PRU Data RAM byte offsets ─────────────────────────────────────────────────
    .asg  0x00,  OFFS_A_LO   ; operand A bits [31:0]
    .asg  0x04,  OFFS_A_HI   ; operand A bits [63:32]
    .asg  0x08,  OFFS_B_LO   ; operand B bits [31:0]
    .asg  0x0C,  OFFS_B_HI   ; operand B bits [63:32]
    .asg  0x10,  OFFS_R_LO   ; result    bits [31:0]
    .asg  0x14,  OFFS_R_HI   ; result    bits [63:32]

; ── Register aliases ──────────────────────────────────────────────────────────
    .asg  r2,    lo_A        ; A bits [31:0]
    .asg  r3,    hi_A        ; A bits [63:32]  → sign[31], exp[30:20], mant[19:0]
    .asg  r4,    lo_B        ; B bits [31:0]
    .asg  r5,    hi_B        ; B bits [63:32]
    .asg  r6,    sigA_lo     ; lower 32 bits of 53-bit significand of A
    .asg  r7,    sigA_hi     ; upper 21 bits of 53-bit significand of A (bit 20 = implicit 1)
    .asg  r8,    sigB_lo     ; lower 32 bits of 53-bit significand of B
    .asg  r9,    sigB_hi     ; upper 21 bits of 53-bit significand of B
    .asg  r10,   expA        ; 11-bit biased exponent of A
    .asg  r11,   expB        ; 11-bit biased exponent of B
    .asg  r12,   res_sign    ; result sign (bit 0)
    .asg  r13,   res_exp     ; result biased exponent
    .asg  r22,   pp1         ; accumulated product bits [63:32]
    .asg  r23,   pp2         ; accumulated product bits [95:64]
    .asg  r24,   pp3         ; accumulated product bits [105:96]

;****************************
;*          MAIN            *
;****************************
main:

; ── Initialise registers and MAC unit ────────────────────────────────────────
init:
    zero  &r0, 120                  ; clear R0–R29 (120 bytes = 30 × 4-byte registers)
                                    ; r1 = 0 after zero → used as base address for LBBO/SBBO
    LDI   r25, MULTIPLY_ONLY        ; MAC control word = 0 (multiply-only mode)
    XOUT  DEVICE_ID, &r25, 1        ; configure MAC unit

; ── Load the two operands from PRU Data RAM ───────────────────────────────────
    LBBO  &lo_A, r1, OFFS_A_LO, 4
    LBBO  &hi_A, r1, OFFS_A_HI, 4
    LBBO  &lo_B, r1, OFFS_B_LO, 4
    LBBO  &hi_B, r1, OFFS_B_HI, 4

; ── Extract sign bits ─────────────────────────────────────────────────────────
    LSR   res_sign, hi_A, 31        ; res_sign = signA  (bit 31 of hi word)
    LSR   r0,       hi_B, 31        ; r0       = signB
    XOR   res_sign, res_sign, r0    ; result sign = signA XOR signB

; ── Extract 11-bit biased exponents ──────────────────────────────────────────
    ; hi word layout: [31]=sign, [30:20]=exponent[10:0], [19:0]=mantissa[51:32]
    ; After LSR by 20 the exponent sits in bits [10:0] with sign_bit at [11].
    ; 0x7FF (2047) > 255 so we cannot use the 8-bit OP(255) immediate form of AND;
    ; load the mask into a register first.
    LSR   expA, hi_A, 20
    LDI   r0, 0x7FF                 ; 11-bit mask (fits in 16-bit LDI)
    AND   expA, expA, r0            ; expA = bits [10:0] = 11-bit exponent
    LSR   expB, hi_B, 20
    AND   expB, expB, r0            ; r0 still = 0x7FF

; ── Compute result biased exponent: expA + expB − 1023 ───────────────────────
    ADD   res_exp, expA, expB
    LDI   r0, 1023
    SUB   res_exp, res_exp, r0

; ── Restore 53-bit significands (re-insert the implicit leading 1) ────────────
    ; sigA_lo = lower 32 bits of significand = lo_A (direct copy)
    ; sigA_hi = upper 21 bits of significand:
    ;           bit 20 = implicit leading 1
    ;           bits [19:0] = mantissa[51:32] from hi_A[19:0]
    ; LSL/LSR by 12 isolates bits [19:0] of hi_A (discards sign and exponent).
    MOV   sigA_lo, lo_A
    LSL   sigA_hi, hi_A, 12         ; shift left  to discard sign[31] + exp[30:20]
    LSR   sigA_hi, sigA_hi, 12      ; shift right to right-align mantissa[51:32]
    LDI32 r0, 0x100000              ; 0x100000 = bit 20 = implicit leading 1
                                    ; (LDI32 is a 2-instruction pseudo-op: 2 cycles)
    OR    sigA_hi, sigA_hi, r0      ; sigA_hi = 21-bit upper significand

    MOV   sigB_lo, lo_B
    LSL   sigB_hi, hi_B, 12
    LSR   sigB_hi, sigB_hi, 12
    OR    sigB_hi, sigB_hi, r0      ; r0 still = 0x100000

; ── Multiply 53-bit significands via four 32-bit MAC operations ───────────────
;
;   sigA = sigA_hi × 2^32 + sigA_lo   (sigA_hi ≤ 21 bits, sigA_lo = 32 bits)
;   sigB = sigB_hi × 2^32 + sigB_lo
;
;   106-bit product P = sigA × sigB, accumulated in {pp3:pp2:pp1} = P[105:32]:
;
;     p00 contributes to P[63:0]    → pp1 seed = p00[63:32], p00[31:0] discarded
;     p01 contributes to P[95:32]   → pp1 += p01[31:0], pp2 += p01[63:32]
;     p10 contributes to P[95:32]   → pp1 += p10[31:0], pp2 += p10[63:32]
;     p11 contributes to P[127:64]  → pp2 += p11[31:0], pp3 += p11[63:32]
;
;   All carries are propagated immediately with ADC.

    ; --- p00 = sigA_lo × sigB_lo ------------------------------------------------
    MOV   r28, sigA_lo
    MOV   r29, sigB_lo              ; writing R29 triggers hardware multiply
    NOP                             ; 1-cycle result latency
    XIN   DEVICE_ID, &r26, 4       ; r26 = p00[31:0]  (below mantissa; read required by MAC protocol)
    XIN   DEVICE_ID, &r27, 4       ; r27 = p00[63:32]
    MOV   pp1, r27                  ; pp1 = p00_hi (seed, pp2/pp3 still 0 from zero)

    ; --- p01 = sigA_hi × sigB_lo ------------------------------------------------
    MOV   r28, sigA_hi
    MOV   r29, sigB_lo
    NOP
    XIN   DEVICE_ID, &r26, 4       ; r26 = p01[31:0]
    XIN   DEVICE_ID, &r27, 4       ; r27 = p01[63:32]
    ADD   pp1, pp1, r26             ; pp1 += p01[31:0]; carry to pp2
    ADC   pp2, pp2, 0               ; pp2 += carry (pp2 was 0)
    ADD   pp2, pp2, r27             ; pp2 += p01[63:32]; carry to pp3
    ADC   pp3, pp3, 0               ; pp3 += carry (pp3 was 0)

    ; --- p10 = sigA_lo × sigB_hi ------------------------------------------------
    MOV   r28, sigA_lo
    MOV   r29, sigB_hi
    NOP
    XIN   DEVICE_ID, &r26, 4       ; r26 = p10[31:0]
    XIN   DEVICE_ID, &r27, 4       ; r27 = p10[63:32]
    ADD   pp1, pp1, r26             ; pp1 += p10[31:0]; carry to pp2
    ADC   pp2, pp2, 0               ; pp2 += carry
    ADD   pp2, pp2, r27             ; pp2 += p10[63:32]; carry to pp3
    ADC   pp3, pp3, 0               ; pp3 += carry

    ; --- p11 = sigA_hi × sigB_hi ------------------------------------------------
    MOV   r28, sigA_hi
    MOV   r29, sigB_hi
    NOP
    XIN   DEVICE_ID, &r26, 4       ; r26 = p11[31:0]
    XIN   DEVICE_ID, &r27, 4       ; r27 = p11[63:32] (≤10 bits: 21-bit×21-bit=42-bit)
    ADD   pp2, pp2, r26             ; pp2 += p11[31:0]; carry to pp3
    ADC   pp3, pp3, 0               ; pp3 += carry
    ADD   pp3, pp3, r27             ; pp3 += p11[63:32]

; ── Normalise ─────────────────────────────────────────────────────────────────
;
;   P ∈ [2^104, 2^106) because sigA, sigB ∈ [2^52, 2^53).
;   The leading 1 of P is therefore at bit 104 or bit 105.
;
;   pp3 holds P[105:96].  P[105] = pp3[9].
;
;   bit 105 clear (no shift):  mantissa = P[103:52]
;     mantissa_lo[31:0]   = {pp2[19:0] << 12, pp1[31:20]}
;     mantissa_hi[19:0]   = {pp3[7:0]  << 12, pp2[31:20]}
;
;   bit 105 set (shift right 1, exponent += 1):  mantissa = P[104:53]
;     mantissa_lo[31:0]   = {pp2[20:0] << 11, pp1[31:21]}
;     mantissa_hi[19:0]   = {pp3[8:0]  << 11, pp2[31:21]}

    QBBS  do_shift, pp3, 9          ; branch if P[105]=pp3[9] is set

noshift:
    ; mantissa_lo = P[83:52] = {pp2[19:0] << 12, pp1[31:20]}
    LSL   r0, pp2, 12               ; r0[31:12] = pp2[19:0]
    LSR   r2, pp1, 20               ; r2[11:0]  = pp1[31:20]
    OR    r0, r0, r2                ; r0 = mantissa_lo (32 bits)

    ; mantissa_hi (20 bits) = P[103:84] = {pp3[7:0] << 12, pp2[31:20]}
    AND   r2, pp3, 0xFF             ; r2 = pp3[7:0]  (0xFF = 255, fits OP(255))
    LSL   r2, r2, 12                ; r2[19:12] = pp3[7:0]
    LSR   r3, pp2, 20               ; r3[11:0]  = pp2[31:20]
    OR    r2, r2, r3                ; r2 = mantissa_hi (20 bits)

    ; Round to nearest (ties away from zero): round bit = P[51] = pp1[19]
    QBBC  rnd_ns_done, pp1, 19      ; skip if round bit = 0
    ADD   r0, r0, 1                 ; mantissa_lo += 1
    ADC   r2, r2, 0                 ; propagate carry into mantissa_hi
    QBBC  rnd_ns_done, r2, 20      ; skip if no exponent overflow
    ; Exponent overflow: mantissa was 0xFFFFF and wrapped to 0x100000 (only bit 20 set).
    ; LSL by 12 shifts 0x100000 left 12 → 0x100000000 which truncates to 0x00000000.
    ; LSR by 12 of 0 = 0.  Result: mantissa_hi = 0, mantissa_lo = 0 (already wrapped).
    LSL   r2, r2, 12               ; (0x100000 << 12) & 0xFFFFFFFF = 0
    LSR   r2, r2, 12               ; r2 = 0 (mantissa_hi = 0, implicit 1 consumed)
    ADD   res_exp, res_exp, 1      ; exponent overflow: result = 1.0 × 2^(exp+1)
rnd_ns_done:
    QBA   pack_result

do_shift:
    ; Product has an extra factor of 2: normalise by incrementing exponent
    ADD   res_exp, res_exp, 1

    ; mantissa_lo = P[84:53] = {pp2[20:0] << 11, pp1[31:21]}
    LSL   r0, pp2, 11               ; r0[31:11] = pp2[20:0]
    LSR   r2, pp1, 21               ; r2[10:0]  = pp1[31:21]
    OR    r0, r0, r2                ; r0 = mantissa_lo (32 bits)

    ; mantissa_hi (20 bits) = P[104:85] = {pp3[8:0] << 11, pp2[31:21]}
    ; 0x1FF (511) > 255, cannot use as OP(255) immediate — load into register
    LDI   r3, 0x1FF                 ; 9-bit mask
    AND   r2, pp3, r3               ; r2 = pp3[8:0]
    LSL   r2, r2, 11                ; r2[19:11] = pp3[8:0]
    LSR   r3, pp2, 21               ; r3[10:0]  = pp2[31:21]
    OR    r2, r2, r3                ; r2 = mantissa_hi (20 bits)

    ; Round to nearest (ties away from zero): round bit = P[52] = pp1[20]
    QBBC  rnd_ds_done, pp1, 20      ; skip if round bit = 0
    ADD   r0, r0, 1                 ; mantissa_lo += 1
    ADC   r2, r2, 0                 ; propagate carry into mantissa_hi
    QBBC  rnd_ds_done, r2, 20      ; skip if no exponent overflow
    ; Exponent overflow: same as noshift path — mantissa wrapped to 0x100000.
    LSL   r2, r2, 12               ; (0x100000 << 12) & 0xFFFFFFFF = 0
    LSR   r2, r2, 12               ; r2 = 0 (mantissa_hi = 0, implicit 1 consumed)
    ADD   res_exp, res_exp, 1      ; exponent overflow: result = 1.0 × 2^(exp+1)
rnd_ds_done:

pack_result:
; ── Assemble the result double ────────────────────────────────────────────────
;   hi_result[31]    = result sign
;   hi_result[30:20] = result exponent (11 bits)
;   hi_result[19:0]  = mantissa[51:32]  (= r2, 20 bits)
;   lo_result[31:0]  = mantissa[31:0]   (= r0, 32 bits)
    LSL   r3, res_exp, 20           ; r3 = exp << 20 (bits [30:20])
    OR    r3, r3, r2                ; r3 |= mantissa_hi (bits [19:0])
    LSL   r1, res_sign, 31          ; r1 = sign << 31
    OR    r3, r3, r1                ; r3 = hi_result

; ── Store result to PRU Data RAM ─────────────────────────────────────────────
    LDI   r1, 0                     ; reset base address (r1 was modified above)
    SBBO  &r0, r1, OFFS_R_LO, 4    ; store mantissa_lo   → result bits [31:0]
    SBBO  &r3, r1, OFFS_R_HI, 4    ; store hi_result     → result bits [63:32]

    halt
