; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2025-2026 Texas Instruments Incorporated - http://www.ti.com/

;******************************************************************************
;   File:     main.asm
;
;   Brief:    PRU1 simple modulator: ~20.83 MHz clock on PRU1_GPO0 and
;             MSB-first 32-bit serial data on PRU1_GPO1.
;
;   Target:   AM243x ICSS_G0 PRU1
;
;   Signals:
;             PRU1_GPO0 (pin 0) - ~20.83 MHz clock, 50% duty cycle
;             PRU1_GPO1 (pin 1) - Serial data, MSB-first, 32-bit frames
;
;   Timing:
;             PRU clock       - 333 MHz (3 ns per cycle)
;             Bit period      - 16 cycles (48 ns, ~20.83 MHz)
;             Clock high time - 8 cycles (24 ns)
;             Clock low time  - 8 cycles (24 ns)
;             Frame length    - 32 bits (~1.536 us)
;
;   Data Source:
;             ICSS DRAM0 offset 0, loaded with: lbco &r2, c24, 0, 4
;             c24 is the PRU constant table entry for DRAM0 base address.
;             The host (R5F) may update this word between frames.
;
;   PEAK cycles:
;             16 cycles per bit (deterministic), 32 bits per frame.
;             lbco reload is pipelined into the last bit's LOW phase.
;
;   Registers modified:
;             r0-r29 cleared at init; r30 (GPO output)
;             r2 (DATA_REG), r3 (BIT_CNT), r4 (shadow R30)
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
; Load initial data word and set bit counter before entering the main loop.
;------------------------------------------------------------------------------

    lbco    &DATA_REG, c24, 0, 4    ; load 32-bit word from DRAM0 offset 0
    ldi     BIT_CNT, NUM_BITS       ; set counter to 32

;******************************************************************************
;   BIT_LOOP - exactly 16 PRU cycles per iteration (48 ns per bit)
;
;   Clock HIGH phase (cycles 1-8):
;     Build shadow R30 with CLK=1 and the current MSB, drive R30 (rising
;     edge).  Then left-shift DATA_REG and extract the next bit into a
;     second shadow: CLK=0 + bit N+1.
;
;     DATA=1 path (qbbc NOT taken):
;       ldi(1)+qbbc(1)+set(1)+qba(1) = 4 cycles -> DRIVE_CLK_HIGH
;     DATA=0 path (qbbc TAKEN):
;       ldi(1)+qbbc(1)+nop(1)+nop(1) = 4 cycles -> DRIVE_CLK_HIGH
;
;   Clock LOW phase (cycles 9-16):
;     Single mov drives CLK=0 and DATA=bit N+1 simultaneously (falling
;     edge).  Decrement counter, then either pad NOPs and loop (normal
;     path) or inline lbco+ldi reload (last-bit path).
;     Both paths are exactly 8 cycles (16 total per bit).
;
;     Normal path (BIT_CNT>0 after decrement):
;       mov(9)+sub(10)+qbne_taken(11)+nop*4(12-15)+qba(16) = 8 cycles
;     Last-bit path (BIT_CNT==0 after decrement):
;       mov(9)+sub(10)+qbne_ntaken(11)+lbco(12)+ldi(13)+nop*2(14-15)
;       +qba(16) = 8 cycles
;******************************************************************************

BIT_LOOP:
    ; --- Clock HIGH phase (8 cycles) ---
    ldi     r4, (1 << CLK_PIN)          ; 1: shadow: CLK=1, DATA=0
    qbbc    DATA_IS_LOW, DATA_REG, 31   ; 2: branch if current MSB is 0
    set     r4, r4, DATA_PIN            ; 3: (DATA=1 path) set DATA bit
    qba     DRIVE_CLK_HIGH              ; 4: skip alignment nops
DATA_IS_LOW:                            ;    (DATA=0 path: qbbc taken at 2)
    nop                                 ; 3: align with DATA=1 path
    nop                                 ; 4: align with DATA=1 path
DRIVE_CLK_HIGH:
    mov     r30, r4                     ; 5: rising edge: CLK=1, DATA=bit N
    lsl     DATA_REG, DATA_REG, 1       ; 6: shift reg, bit N+1 now at bit 31
    lsr     r4, DATA_REG, (31 - DATA_PIN) ; 7: bit N+1 → DATA_PIN position
    and     r4, r4, (1 << DATA_PIN)     ; 8: keep DATA_PIN only (CLK=0)

    ; --- Clock LOW phase (8 cycles) ---
    mov     r30, r4                     ; 9: falling edge: CLK=0, DATA=next
    sub     BIT_CNT, BIT_CNT, 1         ; 10: decrement bit counter
    qbne    BIT_LOOP_NORMAL, BIT_CNT, 0 ; 11: branch if bits remain
    lbco    &DATA_REG, c24, 0, 4        ; 12: last bit: reload from DRAM0
    ldi     BIT_CNT, NUM_BITS           ; 13: reset counter to 32
    nop                                 ; 14:
    nop                                 ; 15:
    qba     BIT_LOOP                    ; 16: back to start
BIT_LOOP_NORMAL:
    nop                                 ; 12:
    nop                                 ; 13:
    nop                                 ; 14:
    nop                                 ; 15:
    qba     BIT_LOOP                    ; 16: back to start
