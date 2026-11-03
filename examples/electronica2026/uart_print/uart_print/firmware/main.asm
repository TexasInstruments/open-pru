; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

;***************************************************************************
;   File:     main.asm
;
;   Brief:    ICSSG0 PRU0 bit-bang UART transmitter, 115200 baud 8N1.
;             Sends the null-terminated string "Hello 2026" from PRU0
;             DRAM0 and repeats it after an idle gap.
;
;   Ported from pru_simulator source/uart_print.asm to AM243x LaunchPad
;   (LP-AM243) silicon. See ../docs/porting_report.md for the full change
;   list.
;
;   Behaviour:
;             - Drive TX (R30 bit 0) high (UART idle).
;             - Write "Hello 2026\0" to DRAM0 offset 0 with one SBCO.
;             - Read the string byte by byte from DRAM0. For each byte
;               send a start bit (low), 8 data bits LSB first and a stop
;               bit (high). Stop at the null terminator.
;             - Hold TX high for IDLE_GAP (about 1 ms), then send the
;               string again, forever.
;             The string in DRAM0 can be changed at run time (for example
;             from the CCS memory browser). It must be null-terminated
;             and at most 255 bytes long.
;
;   DRAM0 layout (PRU0 local 0x0000, R5F global 0x30000000):
;             0x0000-0x000A  "Hello 2026\0" (written by the firmware)
;             0x000B         0 (padding of the 12-byte SBCO)
;
;   Hardware requirements:
;             - PRU core clock 200 MHz (example.syscfg, coreClk). The bit
;               timing is cycle-counted for 200 MHz: at 115200 baud one
;               bit is 1736 cycles.
;             - R30/R31 in direct GPIO mode (ICSSG GPCFG0 GP_MUX_SEL = 0,
;               set by the R5F host before the core starts).
;             - TX = PRG0_PRU0_GPO0 (R30 bit 0), pad PRG0_PRU0_GPO0,
;               LaunchPad BoosterPack pin BP.33 (header J4). 3.3 V
;               push-pull output.
;
;   Steps to build:
;             - Using CCS: import ti-pru-cgt/example.projectspec, build.
;             - Using makefile: make (project directory)
;
;***************************************************************************

; CCS/makefile specific settings
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

    .global     main
    .sect       ".text"

; ---- Register map --------------------------------------------------------
; Reg     Use                               Live in           Owner
; r0      current byte, shifted LSB first   next_char..stop   main
; r1      data bit counter 8..1             txloop            main
; r2      delay counter                     delay loops       main
; r4      DRAM0 byte offset of the string   everywhere        main
; r10-r12 string bytes for the SBCO         init only         main
; r30     GPO: TX = bit 0                   everywhere        hardware
; r31     GPI: not used                     -                 hardware
;
; ---- Cycle budget (200 MHz, 5 ns/cycle, 115200 baud = 1736.1 cycles) -----
; Cycles between TX edges, measured in pru-simulator on the built .out.
; Path                          Budget       Measured     Note
; start bit                     1736 cyc     1739 cyc
; data bit 0..6                 1736 cyc     1742 / 1743  bit = 1 / bit = 0
; data bit 7                    1736 cyc     1741 / 1742  bit = 1 / bit = 0
; stop bit                      1736 cyc     1745 cyc     incl. LBCO of the
;                                                         next byte
; byte, start to start          17361 cyc    17425 cyc    0.37 % slow; UART
;                                                         tolerance ~2 %
; idle gap after the string     200000 cyc   200017 cyc   1.000 ms
; repeat period                 -            374245 cyc   1.871 ms
; --------------------------------------------------------------------------

BAUD_COUNT  .set 868         ; 2 cycles per sub+qbne iteration: 1736 cycles
                              ; + 3..7 cycles overhead per bit
TX_BIT      .set 0           ; PRG0_PRU0_GPO0 = R30 bit 0
IDLE_GAP    .set 100000      ; 2 cycles per iteration: 200000 cycles = 1 ms

;********
;* MAIN *
;********

main:
    zero &r0, 120                   ; clear r0-r29 (r0.b1-b3 must be 0)

    set  r30, r30, TX_BIT           ; idle HIGH

    ; ---- write "Hello 2026\0" to DRAM0 (little-endian: first char in
    ;      low byte) ----
    ldi32 r10, 0x6C6C6548           ; 'H' 'e' 'l' 'l'
    ldi32 r11, 0x3032206F           ; 'o' ' ' '2' '0'
    ldi32 r12, 0x00003632           ; '2' '6' '\0' '\0'
    ldi  r4, 0
    sbco &r10, c24, r4.b0, 12       ; DRAM0[0..11] = r10..r12

send_string:
    ldi  r4, 0                      ; r4.b0 = DRAM0 byte offset

next_char:
    lbco &r0, c24, r4.b0, 1         ; r0.b0 = DRAM0[r4.b0]
    qbeq idle_gap, r0.b0, 0         ; 1, null terminator -> idle gap
    ldi  r1, 8                      ; 1, r1 = bit counter

    ; ---- START bit (LOW) ----
    clr  r30, r30, TX_BIT           ; 1
    ldi  r2, BAUD_COUNT             ; 1
delay_start:
    sub  r2, r2, 1                  ; 1
    qbne delay_start, r2, 0         ; 1

    ; ---- 8 data bits, LSB first ----
txloop:
    qbbs bit_high, r0, 0            ; 1
    clr  r30, r30, TX_BIT           ; 1
    qba  bit_done                   ; 1
bit_high:
    set  r30, r30, TX_BIT           ; 1
bit_done:
    lsr  r0, r0, 1                  ; 1
    ldi  r2, BAUD_COUNT             ; 1
delay_data:
    sub  r2, r2, 1                  ; 1
    qbne delay_data, r2, 0          ; 1
    sub  r1, r1, 1                  ; 1
    qbne txloop, r1, 0              ; 1

    ; ---- STOP bit (HIGH) ----
    set  r30, r30, TX_BIT           ; 1
    ldi  r2, BAUD_COUNT             ; 1
delay_stop:
    sub  r2, r2, 1                  ; 1
    qbne delay_stop, r2, 0          ; 1

    add  r4, r4, 1                  ; 1, advance to next byte
    qba  next_char                  ; 1

    ; ---- idle gap: TX stays HIGH, then send the string again ----
idle_gap:
    ldi32 r2, IDLE_GAP              ; 2
delay_gap:
    sub  r2, r2, 1                  ; 1
    qbne delay_gap, r2, 0           ; 1
    qba  send_string                ; 1

    halt                            ; not reached
