; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2025 Texas Instruments Incorporated - http://www.ti.com/

;   filename:     uart_transmit_macro.inc
;   description:  repeating macros for transmit
;
;   authors:         Thomas Leyrer       date: 14.10.2025
;                    Nick Saulnier       date: 15.12.2025

; ***********************************************************************


;*******************************************************************************
; Macro: m_tx_sof
;
; convert 8 bit data to 10 bit UART symbol with 1 start bit '0' and one stop bit '1'
; UART is LSB first which means the first bit on wire is start bit followed by bit 0 of data bits.
; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     24 cycles
;*******************************************************************************

m_tx_sof    .macro    SCRATCH_PAD, STATE

; load first 44 bytes from input buffer into R2-R12
; BUF_PTR_REG was assigned in IDLE when transition to SOF
    lbco    &r2, c24, BUF_PTR_REG, 44          ; 11 32-bit words --> cc 13
; TODO: Remove commented code later
; save data to scratch pad bank 0 for next iteration of 16 bit transfer into FIFO
    xout    SCRATCH_PAD, &r2, 44
;                   ; cc 14

; original macro START
; make space for start bit - bit 0 already 0 after shift operand
    lsl     r0.b2, r2.b0, 1
; convert 2nd data byte to UART symbol
; d7 shift down by 7
    lsr     r1.b2, r2.b0, 7
; d0-d4 shift up by 3
    lsl     r1.b3, r2.b1, 3
; combine both into single byte
    or      r0.b3, r1.b2, r1.b3
; set stop bit
    set     r0.b3, r0.b3, 1
; send to tx fifo
    mov     r30.b0, r0.b2
    mov     r30.b0, r0.b3
; original macro end

; increment buffer pointer
    add     BUF_PTR_REG, BUF_PTR_REG, 44
; change state
    ldi     STATE, NEXT1_STATE               ; cc 23
; TODO: Remove commented code later
; save context for R0-R26
    xout     SCRATCH_PAD, &r0, 120-12               ; cc 24

    .endm


;*******************************************************************************
; Macro: m_tx_next1
;
; now get back to 2nd 16 bit data which is spread into r2.b1-b3
; Data pattern is: D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     33 cycles
;*******************************************************************************

m_tx_next1    .macro    SCRATCH_PAD, STATE

; load 2nd half, 44 bytes from input buffer
; BUF_PTR_REG was assigned in IDLE when transition to SOF
    lbco    &r2, c24, BUF_PTR_REG, 44          ; 11 32-bit words --> cc 13
; set XOUT to shift data by 11 registers, into scratchpad locations for R13-R23
    ldi     r0.b0, 11
;    xout    SCRATCH_PAD, &r0.b0, 1
; save data to scratch pad bank 0 for next iteration of 16 bit transfer into FIFO
    xout    SCRATCH_PAD, &r2, 44                  ; cc 14
; set XOUT to shift data by 0 registers. Could also be moved to EOF e.g.
    ldi      r0.b0, 0
;   xout     SCRATCH_PAD, &r0.b0, 1
; get r2 data and use r2 to
; read full data set as we store full context at end of state!
    xin     SCRATCH_PAD, &r2, FRAME_LENGTH

; original macro START
; d5-d7 in b0, shift does 0 padding on higher bits!
    lsr     r2.b0, r2.b1, 5
; d0-d2 in b1, shift does 0 padding on lower bits!
    lsl     r2.b1, r2.b2, 5
; prepare first 8 bit which has data from two bytes
    or      r2.b0, r2.b0, r2.b1
; add stop bit
    set     r2.b0, r2.b0, 3

; d3-d7 in b1, shift does 0 padding on higher bits
    lsr     r2.b1, r2.b2, 3
; d0 in b1, shift up to bit 7
    lsl     r2.b2, r2.b3, 7
; prepare 2nd 8 bit which has data from two bytes
    or      r2.b1, r2.b1, r2.b2
; add stop bit
    set     r2.b1, r2.b1, 5
; send to tx fifo
    mov     r30.b0, r2.b0                      ; cc
    mov     r30.b0, r2.b1                      ; cc
; original macro end

; increment buffer pointer
    add     BUF_PTR_REG, BUF_PTR_REG, 44
; change state
    ldi     STATE, NEXTF_STATE               ; cc 23
; set CNT16 REG values to 2
    ldi     CNT16_ABSOLUTE_REG, 0x02
    ldi     CNT16_RELATIVE_REG, 0x02
; ptr at R2
    ldi     r1.b0, 8
; save context for R0-R26
    xout     SCRATCH_PAD, &r0, 120-12              ; cc 24
    .endm


;*******************************************************************************
; State NEXTF
;
; See main.asm > State CH0_NEXTF for detailed comments
;*******************************************************************************


;*******************************************************************************
; Macro: m_tx_nextf1
;
; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     11 cycles
;*******************************************************************************

m_tx_nextf1   .macro    SCRATCH_PAD

; original macro START
; get data shifted down to r2
    mvid     r2, *r1.b0
; make space for start bit - bit 0 already 0 after shift operand
    lsl     r0.b2, r2.b0, 1
; convert 2nd data byte to UART symbol
; d7 shift down by 7
    lsr     r1.b2, r2.b0, 7
; d0-d4 shift up by 3
    lsl     r1.b3, r2.b1, 3
; combine both into single byte
    or      r0.b3, r1.b2, r1.b3
; set stop bit
    set     r0.b3, r0.b3, 1
; send to tx fifo
    mov     r30.b0, r0.b2
    mov     r30.b0, r0.b3
; original macro END

; update counter
    add     CNT16_ABSOLUTE_REG, CNT16_ABSOLUTE_REG, 1
    add     CNT16_RELATIVE_REG, CNT16_RELATIVE_REG, 1
; save context for R0-R26
    xout     SCRATCH_PAD, &r0, 120-12

    .endm


;*******************************************************************************
; Macro: m_tx_nextf2
;
; 2nd 16 bit data which is spread into r2.b1-b3
; Data pattern is: D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     14 cycles
;*******************************************************************************

m_tx_nextf2      .macro    SCRATCH_PAD

; original macro START
; get data shifted down to r2
    mvid     r2, *r1.b0
; d5-d7 in b1, shift does 0 padding on higher bits!
    lsr     r2.b0, r2.b1, 5
; d0-d2 in b1, shift does 0 padding on lower bits!
    lsl     r2.b1, r2.b2, 5
; prepare first 8 bit which has data from two bytes
    or      r2.b0, r2.b0, r2.b1
; add stop bit
    set     r2.b0, r2.b0, 3

; d3-d7 in b2, shift does 0 padding on higher bits
    lsr     r2.b1, r2.b2, 3
; d0 in b3, shift up to bit 7
    lsl     r2.b2, r2.b3, 7
; prepare 2nd 8 bit which has data from two bytes
    or      r2.b1, r2.b1, r2.b2
; add stop bit
    set     r2.b1, r2.b1, 5
; send to tx fifo
    mov     r30.b0, r2.b0
    mov     r30.b0, r2.b1
; original macro END

; update counter
    add     CNT16_ABSOLUTE_REG, CNT16_ABSOLUTE_REG, 1
    add     CNT16_RELATIVE_REG, CNT16_RELATIVE_REG, 1
; save context for R0-R26
    xout     SCRATCH_PAD, &r0, 120-12

    .endm


;*******************************************************************************
; Macro: m_tx_nextf3
;
; cnt16d: 2 (third or every 5th + 2)
; Data pattern is: D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     10 cycles
;*******************************************************************************

m_tx_nextf3   .macro    SCRATCH_PAD

; original macro START
; index spans over r2 and r3
; shift index by 16 bit
    add      r1.b0, r1.b0, 2
; get data shifted down to r2
    mvid     r2, *r1.b0
; d1-d7 in b1, shift does 0 padding on higher bits!
    lsr     r2.b0, r2.b1, 1
; set stop bit
    set     r2.b0, r2.b0, 7
; d0-d6 in b2, shift does 0 padding on lower bits!
; padding 0 is a start bit
    lsl     r2.b1, r2.b2, 1
; prepare first 8 bit which has data from two bytes
; no 'or' function here as cross over is at byte boundary

; send to tx fifo
    mov     r30.b0, r2.b0
    mov     r30.b0, r2.b1
; original macro END

; update counter
    add     CNT16_ABSOLUTE_REG, CNT16_ABSOLUTE_REG, 1
    add     CNT16_RELATIVE_REG, CNT16_RELATIVE_REG, 1
; save context for R0-R26
    xout     SCRATCH_PAD, &r0, 120-12

    .endm


;*******************************************************************************
; Macro: m_tx_nextf4
;
; cnt16d: 3 (fourth or every 5th + 3)
; Data pattern is: D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;
;
; PEAK cycles:
;     15 cycles
;*******************************************************************************

m_tx_nextf4   .macro    SCRATCH_PAD

; original macro START
; shift index by 16 bit
    add      r1.b0, r1.b0, 2
; get data shifted down to r2
    mvid     r2, *r1.b0

; d7 in b0, shift does 0 padding on higher bits!
    lsr     r2.b0, r2.b0, 7
; set stop bit
    set     r2.b0, r2.b0, 1
; d0-d4 in b1, shift does 0 padding on lower bits!
    lsl     r0.b2, r2.b1, 3
; prepare first 8 bit which has data from two bytes
    or      r2.b0, r2.b0, r0.b2

; d5-d7 in b1, shift does 0 padding on higher bits
    lsr     r2.b1, r2.b1, 5
; set stop bit
    set     r2.b1, r2.b1, 3
; d0-d2 in b2, shift up to bit 7
    lsl     r2.b2, r2.b2, 5
; prepare 2nd 8 bit which has data from two bytes
    or      r2.b1, r2.b1, r2.b2
; send to tx fifo
    mov     r30.b0, r2.b0
    mov     r30.b0, r2.b1
; original macro END

 ; update counter
    add     CNT16_ABSOLUTE_REG, CNT16_ABSOLUTE_REG, 1
    add     CNT16_RELATIVE_REG, CNT16_RELATIVE_REG, 1
; save context for R0-R26
    xout    SCRATCH_PAD, &r0, 120-12

    .endm


;*******************************************************************************
; Macro: m_tx_nextf5
;
; cnt16d: 4 (fifth or every 5th + 2)
; Data pattern is: D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;
;
; PEAK cycles:
;     12 cycles
;*******************************************************************************

m_tx_nextf5    .macro    SCRATCH_PAD

; original macro START
; shift data into r2
; auto increment after the register offset is used. This sets us up for the next block of 5
    mvid     r2, *r1.b0++
; d3-d7 in b2, shift does 0 padding on higher bits!
    lsr     r2.b0, r2.b2, 3
; set stop bit
    set     r2.b0, r2.b0, 5
; d0 in b3, shift does 0 padding on lower bits!
    lsl     r2.b1, r2.b3, 7
; prepare first 8 bit which has data from two bytes
    or      r2.b0, r2.b0, r2.b1
; d1-d7 in b3, shift does 0 padding on higher bits
    lsr     r2.b1, r2.b3, 1
; set stop bit
    set     r2.b1, r2.b1, 7
; send to tx fifo
    mov     r30.b0, r2.b0
    mov     r30.b0, r2.b1
; original macro END

; update counter
    add     CNT16_ABSOLUTE_REG, CNT16_ABSOLUTE_REG, 1
    ldi     CNT16_RELATIVE_REG, 0
; save context for R0-R26
    xout    SCRATCH_PAD, &r0, 120-12

    .endm


;*******************************************************************************
; Macro: m_tx_eof
;
; cnt16d: 4 (fifth or every 5th + 2)
; Data pattern is: D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;
;
; PEAK cycles:
;     14 cycles
;*******************************************************************************

m_tx_eof    .macro    SCRATCH_PAD, STATE

; original macro START
; shift data into r2
; auto increment after the register offset is used for next block of 5
    mvid     r2, *r1.b0++
; d3-d7 in b2, shift does 0 padding on higher bits!
    lsr     r2.b0, r2.b2, 3
; set stop bit
    set     r2.b0, r2.b0, 5
; d0 in b3, shift does 0 padding on lower bits!
    lsl     r2.b1, r2.b3, 7
; prepare first 8 bit which has data from two bytes
    or      r2.b0, r2.b0, r2.b1
; d1-d7 in b3, shift does 0 padding on higher bits
    lsr     r2.b1, r2.b3, 1
; set stop bit
    set     r2.b1, r2.b1, 7
; send to tx fifo
    mov     r30.b0, r2.b0
    mov     r30.b0, r2.b1
; original macro END

; end of frame resets counter and states
; set CNT16 REG values to 2
    ldi     CNT16_ABSOLUTE_REG, 0x02
    ldi     CNT16_RELATIVE_REG, 0x02
; set state to idle
    ldi     STATE, IDLE_STATE
; set ptr
    ldi     r1.b0, 8
; save context for R0-R26
    xout    SCRATCH_PAD, &r0, 120-12

    .endm
