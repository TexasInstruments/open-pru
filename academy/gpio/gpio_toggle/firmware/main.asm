; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2024-2026 Texas Instruments Incorporated - http://www.ti.com/

;*****************************************************************************
;   File:     main.asm
;
;   Brief:    Toggle PRU GPO output signal
;
;   What this example teaches:
;       - How a PRU core drives a pin: write a value to register R30.
;         Each bit of R30 is wired to one PRU GPO (General Purpose Output)
;         pin of that core: R30 bit 4 -> GPO4, R30 bit 0 -> GPO0, and so on.
;       - How to create a delay by counting down in a loop. Most PRU
;         instructions take 1 cycle, so the delay is easy to calculate.
;         See the PEAK cycles comment above main_loop.
;
;   Who runs this code:
;       A host core (for example an MCU+ core, or Linux or Zephyr on an
;       application core) copies this firmware into the PRU instruction
;       memory and starts the PRU. The PRU does not start by itself. The
;       same main.asm is built for PRU0 and for PRU1. Each core toggles
;       its own GPO4.
;
;   The pin that GPO4 reaches on the board depends on the device. See the
;   readme for the pin location on each board, and for debug steps.
;*****************************************************************************

; CCS/makefile specific settings
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

    .global     main
    .sect       ".text"

; definitions
; Number of loop iterations per delay. Each iteration costs 2 cycles
; (sub + qbne), so one delay lasts 2 x DELAY_COUNT cycles. Increase this
; value to slow the toggle down (easier to see on a slow logic analyzer),
; decrease it to speed the toggle up. See the PEAK cycles comment above
; main_loop for the resulting timing.
    .asg    100000 ,DELAY_COUNT        ; Delay counter value


;********
;* MAIN *
;********

main:

init:

;----------------------------------------------------------------------------
;   Clear the register space
;   Before beginning with the application, make sure all the registers are
;   set to 0. PRU has 32 - 4 byte registers: R0 to R31, with R30 and R31
;   being special registers for output and input respectively.
;
;   R0-R29 are general purpose and are cleared by the zero instruction
;   below. R31 reads the input pins, so it cannot be cleared. R30 drives
;   the output pins and is cleared separately, right after.
;----------------------------------------------------------------------------

; Give the starting address and number of bytes to clear.
; &r0 = start at R0, 120 bytes = 30 registers x 4 bytes = R0 to R29.
    zero    &r0, 120

; Starting with every output low. Without this the
; level of the first half period is unknown.
    ldi     r30, 0

; GPIO configuration note:
; After reset the PRU GPIO config register (GPCFG0 for PRU0, GPCFG1 for
; PRU1) defaults to "direct" GPO mode. In direct mode, the value written to R30
; appears on the pins right away, so no configuration is needed here.

;----------------------------------------------------------------------------
;   PEAK cycles per pass of main_loop: 4 x DELAY_COUNT + 7
;
;   ldi32 r5 + ldi32 r6 ............ 2 + 2        =  4
;   delay_loop1: sub + qbne ........ 2 x COUNT   =  2 x DELAY_COUNT
;   ldi r30.b0, 0x10 ............... 1
;   delay_loop2: sub + qbne ........ 2 x COUNT   =  2 x DELAY_COUNT
;   ldi r30.b0, 0x00 ............... 1
;   qba main_loop .................. 1
;
;   DELAY_COUNT = 100000 gives 400007 cycles. At a 200 MHz PRU clock
;   (5 ns per cycle) that is about 2.0 ms, so the pin is a ~500 Hz
;   square wave with ~50% duty cycle (about 1 ms high, 1 ms low).
;
;   Hand-counted from the instruction listing (dispru). The one-time
;   "zero" and "ldi r30, 0" in init are not counted. The loop counts are
;   constants, so this is also the worst case. Update this total if
;   DELAY_COUNT or the loop body changes.
;----------------------------------------------------------------------------
main_loop:
    ; Load the two delay counters. ldi32 loads a 32-bit constant and is
    ; assembled as two ldi instructions, so each costs 2 cycles.
    ldi32     r5, DELAY_COUNT
    ldi32     r6, DELAY_COUNT

    ; Delay #1: count r5 down to 0. The pin is LOW during this delay
    ; (R30 was cleared in init, and at the end of the previous pass).
delay_loop1:
    sub     r5, r5, 1
    qbne    delay_loop1, r5, 0

    ; Three ways to change an output pin. This example uses the first.
    ;   1. Overwrite:  ldi r30.b0, 0x10
    ;      0x10 = binary 0001 0000 = bit 4 set. ".b0" writes the lowest
    ;      byte (bits 7:0) of R30. ALL 8 bits are overwritten: bit 4 is
    ;      set and bits 7:5 and 3:0 are cleared. Bits 31:8 are not
    ;      touched. That is fine here, because only GPO4 is used. Use
    ;      "r30" instead of "r30.b0" to overwrite all 32 bits.
    ;   2. Set or clear one bit:  set r30, r30, 4  (pin high)
    ;                             clr r30, r30, 4  (pin low)
    ;      Only bit 4 changes. Every other bit of R30 keeps its value.
    ;   3. Toggle (XOR):  xor r30, r30, 0x10
    ;      Flips bit 4, whatever its current level is, and leaves every
    ;      other bit alone. The immediate can only be 0 to 255 (bits
    ;      7:0). To flip a higher bit, put the mask in a register:
    ;      xor r30, r30, r1. The C version (gpio_toggle_c) uses this
    ;      method.
    ;
    ; Toggle GPIO pin 4 by writing 1 into bit 4 of R30.
    ; The pin goes HIGH as soon as this instruction executes.
    ldi     r30.b0, 0x10

    ; Delay #2: the pin stays HIGH for this long.
delay_loop2:
    sub     r6, r6, 1
    qbne    delay_loop2, r6, 0

    ; Toggle GPIO pin 4 by writing 0 into bit 4 of R30.
    ; This overwrites all of bits 7:0 with 0. The pin goes LOW again.
    ldi     r30.b0, 0x00

    ; Continue loop
    ; qba = "branch always". Jump back and repeat forever.
    qba     main_loop

    ; halt the program
    halt ; end of program (never reached)
