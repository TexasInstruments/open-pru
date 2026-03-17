; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2025-2026 Texas Instruments Incorporated - http://www.ti.com/

;******************************************************************************
;   File:     main.asm
;
;   Brief:    PRU1 simple modulator: ~16.67 MHz clock on PRU1_GPO0 and
;             MSB-first 32-bit serial data on PRU1_GPO1.
;
;   Target:   AM243x ICSS_G0 PRU1
;
;   Signals:
;             PRU1_GPO0 (pin 0) - ~16.67 MHz clock, 50% duty cycle
;             PRU1_GPO1 (pin 1) - Serial data, MSB-first, 32-bit frames
;
;   Timing:
;             PRU clock       - 333 MHz (3 ns per cycle)
;             Bit period      - 20 cycles (60 ns, ~16.67 MHz)
;             Clock high time - 10 cycles (30 ns)
;             Clock low time  - 10 cycles (30 ns)
;             Frame length    - 32 bits (~1.92 us)
;
;   Data Source:
;             ICSS DRAM0 offset 0, loaded with: lbco &r2, c24, 0, 4
;             c24 is the PRU constant table entry for DRAM0 base address.
;             The host (R5F) may update this word between frames.
;
;   PEAK cycles:
;             20 cycles per bit (deterministic), 32 bits per frame.
;             lbco reload is pipelined into the last bit's LOW phase.
;
;   Registers modified:
;             r0-r29 cleared at init; r30 (GPO output)
;             r2 (DATA_REG), r3 (BIT_CNT), r4 (rising shadow),
;             r5 (falling shadow), r6 (scratch)
;
;   Build:
;   - Using ccs:
;             - Import pru project to ccs workspace
;             - Build the pru project to generate .out file
;             - Load .out to core ICSSG0 PRU1 of AM243x device
;   - Using makefile:
;             - Use command gmake -all to build PRU project
;
;******************************************************************************

; CCS/makefile specific settings
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

    .global     main
    .sect       ".text"

;------------------------------------------------------------------------------
; Pin definitions
;------------------------------------------------------------------------------

CLK_PIN     .set    0           ; PRU1_GPO0: clock output (bit 0 of R30)
DATA_PIN    .set    1           ; PRU1_GPO1: serial data output (bit 1 of R30)
NUM_BITS    .set    32          ; Bits per data frame

;------------------------------------------------------------------------------
; Register aliases
;------------------------------------------------------------------------------

    .asg    r2, DATA_REG        ; 32-bit data shift register (MSB first)
    .asg    r3, BIT_CNT         ; Bit counter, counts down from NUM_BITS to 0

;******************************************************************************
;* MAIN
;******************************************************************************

main:

init:
    ; Clear all general-purpose registers before starting
    zero    &r0, 120            ; clear R0-R29 (30 registers x 4 bytes)

;------------------------------------------------------------------------------
; Load initial data word, set bit counter, and pre-build the first rising-edge
; shadow register (r4) before entering the main bit loop.
;------------------------------------------------------------------------------

    lbco    &DATA_REG, c24, 0, 4        ; load 32-bit word from DRAM0 offset 0
    ldi     BIT_CNT, NUM_BITS           ; set counter to 32
    ldi     r4, (1 << CLK_PIN)          ; r4 = CLK=1, DATA=0
    lsr     r6, DATA_REG, (31 - DATA_PIN) ; bit 31 → DATA_PIN position
    and     r6, r6, (1 << DATA_PIN)     ; mask to DATA_PIN only
    or      r4, r4, r6                  ; r4 = CLK=1, DATA=first bit

;******************************************************************************
;   BIT_LOOP - exactly 20 PRU cycles per iteration (60 ns per bit)
;
;   Clock HIGH phase (cycles 1-10):
;     Drive rising edge immediately from pre-built shadow r4 (CLK=1, DATA=bit
;     N).  Then compute falling-edge shadow r5 (CLK=0, DATA=bit N+1) using
;     lsl+lsr+and.  Remaining cycles are NOPs.
;
;   Clock LOW phase (cycles 11-20):
;     Drive falling edge from r5 (CLK=0, DATA=bit N+1).  Decrement counter,
;     then rebuild the rising-edge shadow r4 (CLK=1, DATA=bit N+1) for the
;     next iteration using ldi+lsr+and+or.
;
;     Normal path (BIT_CNT > 0 after decrement) - rebuild r4, pad with NOPs:
;       mov(11)+sub(12)+qbne_taken(13)
;       +ldi(14)+lsr(15)+and(16)+or(17)+nop(18)+nop(19)+qba(20)
;
;     Last-bit path (BIT_CNT = 0 after decrement) - reload data, rebuild r4:
;       mov(11)+sub(12)+qbne_ntaken(13)
;       +lbco(14)+ldi(15)+ldi(16)+lsr(17)+and(18)+or(19)+qba(20)
;******************************************************************************

BIT_LOOP:
    ; --- Clock HIGH phase (10 cycles) ---
    mov     r30, r4                     ; 1: rising edge: CLK=1, DATA=bit N
    lsl     DATA_REG, DATA_REG, 1       ; 2: shift reg, bit N+1 now at bit 31
    lsr     r5, DATA_REG, (31 - DATA_PIN) ; 3: bit N+1 → DATA_PIN position
    and     r5, r5, (1 << DATA_PIN)     ; 4: r5 = CLK=0, DATA=bit N+1
    nop                                 ; 5:
    nop                                 ; 6:
    nop                                 ; 7:
    nop                                 ; 8:
    nop                                 ; 9:
    nop                                 ; 10:

    ; --- Clock LOW phase (10 cycles) ---
    mov     r30, r5                     ; 11: falling edge: CLK=0, DATA=bit N+1
    sub     BIT_CNT, BIT_CNT, 1         ; 12: decrement bit counter
    qbne    BIT_LOOP_NORMAL, BIT_CNT, 0 ; 13: branch if bits remain

    ; Last-bit path: reload data word and rebuild rising-edge shadow
    lbco    &DATA_REG, c24, 0, 4        ; 14: reload data word from DRAM0
    ldi     BIT_CNT, NUM_BITS           ; 15: reset counter to 32
    ldi     r4, (1 << CLK_PIN)          ; 16: r4 = CLK=1, DATA=0
    lsr     r6, DATA_REG, (31 - DATA_PIN) ; 17: bit 31 → DATA_PIN position
    and     r6, r6, (1 << DATA_PIN)     ; 18: mask to DATA_PIN only
    or      r4, r4, r6                  ; 19: r4 = CLK=1, DATA=first bit
    qba     BIT_LOOP                    ; 20: start next bit

BIT_LOOP_NORMAL:
    ; Normal path: rebuild rising-edge shadow for next bit
    ldi     r4, (1 << CLK_PIN)          ; 14: r4 = CLK=1, DATA=0
    lsr     r6, DATA_REG, (31 - DATA_PIN) ; 15: bit 31 → DATA_PIN position
    and     r6, r6, (1 << DATA_PIN)     ; 16: mask to DATA_PIN only
    or      r4, r4, r6                  ; 17: r4 = CLK=1, DATA=next bit
    nop                                 ; 18:
    nop                                 ; 19:
    qba     BIT_LOOP                    ; 20: start next bit
