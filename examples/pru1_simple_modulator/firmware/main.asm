; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2025-2026 Texas Instruments Incorporated - http://www.ti.com/

;******************************************************************************
;   File:     main.asm
;
;   Brief:    PRU1 simple modulator: ~20.83 MHz clock on PRU0_GPO0 and
;             MSB-first 32-bit serial data on PRU0_GPO1.
;
;   Target:   AM243x ICSS_G0 PRU1
;
;   Signals:
;             PRU0_GPO0 (pin 0) - ~20.83 MHz clock, 50% duty cycle
;             PRU0_GPO1 (pin 1) - Serial data, MSB-first, 32-bit frames
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
;             Data reload overhead added once per frame (lbco latency).
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

CLK_PIN     .set    0           ; PRU0_GPO0: clock output (bit 0 of R30)
DATA_PIN    .set    1           ; PRU0_GPO1: serial data output (bit 1 of R30)
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
; LOAD_DATA - load a 32-bit word from ICSS DRAM0 and reset the bit counter.
; Execution returns here after every 32-bit frame.
;------------------------------------------------------------------------------

LOAD_DATA:
    lbco    &DATA_REG, c24, 0, 4    ; load 32-bit word from DRAM0 offset 0
    ldi     BIT_CNT, NUM_BITS       ; reset counter to 32

;******************************************************************************
;   BIT_LOOP - exactly 16 PRU cycles per iteration (48 ns per bit)
;
;   Clock HIGH phase (cycles 1-8):
;     Build a shadow R30 with CLK=1 and the current MSB of DATA_REG, then
;     drive R30 in a single write.  Both the DATA=1 and DATA=0 paths through
;     the conditional branch complete in exactly 4 cycles before DRIVE_OUTPUT,
;     giving a total high phase of 8 cycles.
;
;     DATA=1 path (qbbc NOT taken):
;       ldi(1) + qbbc(1) + set(1) + qba(1) = 4 cycles -> DRIVE_OUTPUT
;     DATA=0 path (qbbc TAKEN):
;       ldi(1) + qbbc(1) + nop(1) + nop(1) = 4 cycles -> DRIVE_OUTPUT
;
;   Clock LOW phase (cycles 9-16):
;     Lower the clock, advance the shift register, decrement the counter,
;     pad with NOPs, then branch back or fall through to LOAD_DATA.
;******************************************************************************

BIT_LOOP:
    ; --- Clock HIGH phase (8 cycles) ---
    ldi     r4, (1 << CLK_PIN)          ; 1: shadow CLK=1, DATA=0
    qbbc    DATA_IS_LOW, DATA_REG, 31   ; 2: branch if MSB is 0
    set     r4, r4, DATA_PIN            ; 3: (DATA=1 path) set DATA=1
    qba     DRIVE_OUTPUT                ; 4: skip DATA=0 alignment nops
DATA_IS_LOW:                            ;    (DATA=0 path: qbbc taken at 2)
    nop                                 ; 3: align with DATA=1 path
    nop                                 ; 4: align with DATA=1 path
DRIVE_OUTPUT:
    mov     r30, r4                     ; 5: rising edge: CLK=1, DATA=bit N
    nop                                 ; 6:
    nop                                 ; 7:
    nop                                 ; 8: end of high phase

    ; --- Clock LOW phase (8 cycles) ---
    clr     r30, r30, CLK_PIN           ; 9: falling edge: CLK=0
    lsl     DATA_REG, DATA_REG, 1       ; 10: shift left, next MSB to bit 31
    sub     BIT_CNT, BIT_CNT, 1         ; 11: decrement bit counter
    nop                                 ; 12:
    nop                                 ; 13:
    nop                                 ; 14:
    nop                                 ; 15:
    qbne    BIT_LOOP, BIT_CNT, 0        ; 16: loop back if bits remain

    ; All 32 bits shifted out - reload data word and repeat
    qba     LOAD_DATA
