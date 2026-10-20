; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

;******************************************************************************
;   File:     shift_add_mul64.asm
;
;   Brief:    Unsigned 32 x 32 -> 64-bit multiply using only shift, AND and
;             ADD/ADC. It does not touch the MAC, so main.asm uses it as an
;             independent check of the MAC result on the same core, and as
;             the software baseline for the MAC throughput.
;
;   Hardware: any PRU core (PRU-ICSS, PRU-ICSSM, PRU_ICSSG, PRUSS)
;******************************************************************************

    .sect       ".text"
    .global     FN_SHIFT_ADD_MUL64

;******************************************************************************
;   Function: FN_SHIFT_ADD_MUL64
;
;   r7:r6 = r28 * r29, unsigned. Every bit step takes the same path (the
;   partial product is masked instead of branched over), so the cycle count
;   does not depend on the operands.
;
;   Pseudo code:
;       (start code)
;       mcand = (uint64_t)r28;  mplier = r29;  acc = 0;
;       for (i = 0; i < 32; i++) {
;           mask = -(mplier & 1);
;           acc += mcand & ((uint64_t)mask << 32 | mask);
;           mcand <<= 1;  mplier >>= 1;
;       }
;       r7:r6 = acc;
;       (end code)
;
;   PEAK cycles: 5 + 1 (LOOP) + 32 x 11 + 1 (JMP) = 359, plus 1 for the JAL
;
;   Parameters:
;       r28 - multiplicand, read only
;       r29 - multiplier, read only
;
;   Returns:
;       r7:r6 - 64-bit product (r6 low word)
;
;   Registers modified: r4, r5, r6, r7, r8, r9, r10, r11
;   Return address: r3.w2 (JAL / JMP)
;******************************************************************************
FN_SHIFT_ADD_MUL64:
    .asmfunc
    mov     r4, r28                 ; multiplicand low
    ldi     r5, 0                   ; multiplicand high
    mov     r8, r29                 ; multiplier
    ldi     r6, 0                   ; accumulator low
    ldi     r7, 0                   ; accumulator high
    loop    SHIFT_ADD_BIT_END, 32
    and     r9, r8, 1
    rsb     r9, r9, 0               ; mask: 0 or 0xffffffff
    and     r10, r4, r9
    and     r11, r5, r9
    add     r6, r6, r10             ; low half, sets carry
    adc     r7, r7, r11             ; high half, consumes carry
    lsl     r5, r5, 1               ; 64-bit multiplicand shift
    lsr     r11, r4, 31
    or      r5, r5, r11
    lsl     r4, r4, 1
    lsr     r8, r8, 1               ; next multiplier bit
SHIFT_ADD_BIT_END:
    jmp     r3.w2
    .endasmfunc
