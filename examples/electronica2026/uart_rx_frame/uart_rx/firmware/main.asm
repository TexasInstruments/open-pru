; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

;***************************************************************************
;   File:     main.asm
;
;   Brief:    ICSSG0 PRU0 bit-bang UART receiver, 4 Mbaud 8N1, 11-byte
;             frames stored in PRU0 DRAM0.
;
;   Ported from pru_simulator source/uart_rx_11frame.asm to AM243x
;   LaunchPad (LP-AM243) silicon. See ../docs/porting_report.md for the
;   full change list.
;
;   Behaviour:
;             - Poll RX (R31 bit 2) for the falling edge of a start bit.
;             - Sample 8 data bits LSB first at mid-bit, check the stop
;               bit, and repeat for 11 bytes. Each byte resynchronises on
;               its own start bit.
;             - Store the 11 bytes with one SBCO to DRAM0 at offset r1,
;               then advance r1 by 11. r1 wraps to 0 when the next frame
;               would reach 0x0FFC, so DRAM0 0x0000-0x0FFB is a ring
;               buffer of 372 frames.
;             - After each stored frame, write the 16-bit frame count to
;               DRAM0 0x0FFC. The newest frame is at offset
;               ((count - 1) mod 372) * 11.
;             - Invalid stop bit: set the sticky framing error flag
;               (DRAM0 0x0FFE = 1), drop the partial frame and wait for
;               the next start bit.
;             The firmware has no frame sync. If it starts in the middle
;             of a stream, frames are misaligned until a framing error
;             lines them up again.
;
;   DRAM0 layout (PRU0 local 0x0000, R5F global 0x30000000):
;             0x0000-0x0FFB  frame ring buffer, 372 x 11 bytes
;             0x0FFC-0x0FFD  frame count, 16-bit little-endian, wraps
;             0x0FFE         framing error flag (sticky, 1 = error)
;
;   Hardware requirements:
;             - PRU core clock 200 MHz (example.syscfg, coreClk). The bit
;               timing is cycle-counted for 200 MHz: at 4 Mbaud one bit is
;               50 cycles.
;             - R30/R31 in direct GPIO mode (ICSSG GPCFG0 GP_MUX_SEL = 0,
;               set by the R5F host before the core starts).
;             - RX = PRG0_PRU0_GPI2 (R31 bit 2), pad PRG0_PRU0_GPO2 in mux
;               mode 1, LaunchPad BoosterPack pin BP.31 (header J4).
;               Pull-up enabled in
;               example.syscfg so an open input idles high.
;             - 3.3 V UART TX source, 4 Mbaud, 8N1, idle high.
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
; r1      DRAM0 offset of the next frame    everywhere        main
; r2      frame bytes 0-3 (packed LE)       receive..store    main
; r3      frame bytes 4-7 (packed LE)       receive..store    main
; r4      frame bytes 8-10 (b0, b1, b2)     receive..store    main
; r13     byte accumulator                  receive_byte      main
; r14     bit mask 1, 2, 4 .. 128           receive_byte      main
; r15     RX sample (0 or RX_MASK)          every poll/sample main
; r16     byte index 0..10                  receive_byte      main
; r17     bit counter 0..8                  receive_byte      main
; r18     delay counter; DRAM0 offset in    delay loops,      main
;         store_frame / framing_error       store/error paths
; r19     framing error flag (sticky)       framing_error     main
; r20     frame counter                     store_frame       main
; r30     GPO: not used                     -                 hardware
; r31     GPI: RX = bit 2                   everywhere        hardware
;
; ---- Cycle budget (200 MHz, 5 ns/cycle, 4 Mbaud = 50 cycles/bit) ---------
; Positions are cycles from the start-bit falling edge, or from the start of
; the bit cell (0..49, middle = 25). Measured in pru-simulator on the built
; .out (per-cycle trace, 0x55/0xAA/0x00/0xFF/.. payload).
; Path                          Budget       Measured     Note
; sample_bit .. sample_bit      50 cyc       50 / 51 cyc  bit = 0 / bit = 1
; start edge -> 1st data sample 75 cyc       75-77 cyc    middle of data bit 0
; data samples in bit cell      0..49        25-33        frames >= 1 idle bit
;                                            25-41        back-to-back frames:
;                                                         store_frame delays
;                                                         the next start
;                                                         detect by 12 cyc
; stop sample in stop cell      0..49        31-43        byte 10 latest: the
;                                                         qbeq store_bN chain
;                                                         adds up to 11 cyc
; Baud window (both directions) 4.00 Mbaud   3.85-4.00    pru-simulator sweep,
;                                                         50 kbaud steps
; --------------------------------------------------------------------------

BIT_TIME    .set 22          ; 2 cycles per sub+qbne iteration: 44 cycles
                              ; + 6/7 cycles overhead = 50/51 cycles per bit
START_DELAY .set 34          ; start edge -> middle of data bit 0
NEXT_DELAY  .set 34          ; same, for bytes 2..11
RX_MASK     .set 4           ; PRG0_PRU0_GPI2 = R31 bit 2 (1 << 2)
FRAME_LEN   .set 11          ; bytes per frame
RING_END    .set 0x0FFC      ; 372 * 11: wrap r1 here; frame count offset
ERR_ADDR    .set 0x0FFE      ; DRAM0 offset of the framing error flag

;********
;* MAIN *
;********

main:
    zero &r0, 120                   ; clear r0-r29

    ; Initialize (kept from the simulator source)
    ldi  r1, 0
    ldi  r16, 0
    ldi  r19, 0
    ldi  r20, 0
    ldi  r2, 0
    ldi  r3, 0
    ldi  r4, 0

wait_start:
    ; Poll RX for LOW (start bit)
    and  r15, r31, RX_MASK          ; 1
    qbne wait_start, r15, 0         ; 1

    ; Start bit detected. Delay to the middle of data bit 0
    ldi  r18, START_DELAY           ; 1
delay_start:
    sub  r18, r18, 1                ; 1
    qbne delay_start, r18, 0        ; 1

    ; Now at the center of data bit 0, begin byte reception
    ldi  r16, 0                     ; 1
    jmp  receive_byte               ; 1

receive_byte:
    ; Receive 8 data bits for the current byte (LSB first)
    ldi  r13, 0                     ; 1
    ldi  r14, 1                     ; 1
    ldi  r17, 0                     ; 1

sample_bit:
    ; Sample RX
    and  r15, r31, RX_MASK          ; 1
    qbeq bit_is_zero, r15, 0        ; 1
    ; Bit is 1: set bit in accumulator
    or   r13, r13, r14              ; 1 (bit = 1 only)
bit_is_zero:
    ; Advance bit mask
    lsl  r14, r14, 1                ; 1
    add  r17, r17, 1                ; 1

    ; Delay one bit period (compensates for overhead)
    ldi  r18, BIT_TIME              ; 1
delay_bit:
    sub  r18, r18, 1                ; 1
    qbne delay_bit, r18, 0          ; 1

    ; Check if all 8 bits received
    qbne sample_bit, r17, 8         ; 1

    ; All 8 bits received: store byte into the packed register buffer
    qbeq store_b0,  r16, 0
    qbeq store_b1,  r16, 1
    qbeq store_b2,  r16, 2
    qbeq store_b3,  r16, 3
    qbeq store_b4,  r16, 4
    qbeq store_b5,  r16, 5
    qbeq store_b6,  r16, 6
    qbeq store_b7,  r16, 7
    qbeq store_b8,  r16, 8
    qbeq store_b9,  r16, 9
    qbeq store_b10, r16, 10

store_b0:
    mov  r2.b0, r13.b0
    jmp  check_stop
store_b1:
    mov  r2.b1, r13.b0
    jmp  check_stop
store_b2:
    mov  r2.b2, r13.b0
    jmp  check_stop
store_b3:
    mov  r2.b3, r13.b0
    jmp  check_stop
store_b4:
    mov  r3.b0, r13.b0
    jmp  check_stop
store_b5:
    mov  r3.b1, r13.b0
    jmp  check_stop
store_b6:
    mov  r3.b2, r13.b0
    jmp  check_stop
store_b7:
    mov  r3.b3, r13.b0
    jmp  check_stop
store_b8:
    mov  r4.b0, r13.b0
    jmp  check_stop
store_b9:
    mov  r4.b1, r13.b0
    jmp  check_stop
store_b10:
    mov  r4.b2, r13.b0
    jmp  check_stop

check_stop:
    ; The last bit delay positions us near the stop bit.
    ; Verify STOP bit (should be HIGH)
    and  r15, r31, RX_MASK          ; 1
    qbeq framing_error, r15, 0      ; 1

    ; STOP bit valid: advance to the next byte
    add  r16, r16, 1                ; 1

    ; Check if all 11 bytes received
    qbeq store_frame, r16, FRAME_LEN ; 1

    ; Wait for the next start bit (falling edge) to resynchronize
wait_next_start:
    and  r15, r31, RX_MASK          ; 1
    qbne wait_next_start, r15, 0    ; 1

    ; Next start bit detected: delay to the center of data bit 0
    ldi  r18, NEXT_DELAY            ; 1
delay_next:
    sub  r18, r18, 1                ; 1
    qbne delay_next, r18, 0         ; 1
    jmp  receive_byte               ; 1

store_frame:
    ; All 11 bytes packed in r2-r4: store to DRAM0 with a single SBCO
    sbco &r2, c24, r1, FRAME_LEN

    ; Advance the storage offset, wrap at RING_END (ring buffer)
    add  r1, r1, FRAME_LEN          ; 1
    add  r20, r20, 1                ; 1
    ldi  r18, RING_END              ; 1
    qbgt ring_ok, r1, r18           ; 1  branch if RING_END > r1
    ldi  r1, 0                      ; 1  wrap to the first slot
ring_ok:
    ; Publish the frame count (16-bit) at DRAM0 RING_END
    sbco &r20, c24, r18, 2

    ; Clear the frame buffer for the next frame
    ldi  r2, 0                      ; 1
    ldi  r3, 0                      ; 1
    ldi  r4, 0                      ; 1

    ; Go back to waiting for the next frame's start bit
    jmp  wait_start                 ; 1

framing_error:
    ; Set the sticky error flag
    ldi  r19, 1

    ; Write the error flag to DRAM0 + 0x0FFE
    ldi  r18, ERR_ADDR
    sbco &r19, c24, r18, 1

    ; Reset the byte index and clear the buffer
    ldi  r16, 0
    ldi  r2, 0
    ldi  r3, 0
    ldi  r4, 0
    jmp  wait_start
