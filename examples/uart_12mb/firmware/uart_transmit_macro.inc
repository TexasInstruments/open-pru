; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2025 Texas Instruments Incorporated - http://www.ti.com/

;   filename:     uart_transmit_macro.inc
;   description:  repeating macros for transmit
;
;   authors:         Thomas Leyrer       date: 14.10.2025
;                    Nick Saulnier       date: 15.12.2025

; ***********************************************************************

;*******************************************************************************
; Macro: m_tx1
;
; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     39 cycles
;        5 cycles to get here
;        32 cycles for a wrapped buffer copy
;        25 cycles for a non-wrapped buffer copy
;        25 for EOF_BYTE1
;            12 to m_eof_byte1
;            13 for m_eof_byte1
;        1 cycle to qba to next channel
;*******************************************************************************

m_tx1   .macro        SCRATCH_PAD, STATE

; r1.w0 holds number of bytes to copy
    sub     r1.w0, WRITE_PTR, READ_PTR    ; raw difference
    and     r1.w0, r1.w0, 0x7FF         ; handle wraparound using 2k buffer
    min     r1.w0, r1.w0, 8      ; if r1.w0 > 8, set r1.w0 = 8

; TODO: Add logic to jump to a special function if r1.w0 = 1 byte
; should this be a jump to EOF_BYTE1?
    qbeq    L_SEND_1_BYTE, r1.w0, 1

; is the data wrapping around?
    add     r1.w2, READ_PTR, r1.w0 ; READ_PTR + read bytes
; TODO: is this logic correct? What if READ_PTR starts at BUFFER_END?
    qbgt    L_DATA_WRAP, BUFFER_END, r1.w2; branch if r1.w2 > BUFFER_END

;------------------ buffer copy does not wrap -----------------------
; if not, load data in a single command
    lbco    &r2, c24, READ_PTR, r1.w0     ; up to 2 32-bit words
; these up to 8 bytes could be not word aligned, so worst case is -> cc 5
; READ_PTR increments to keep track of where to grab data from next
    add     READ_PTR, READ_PTR, r1.w0
    sbco    READ_PTR, c24, READ_PTR_ADDRESS, 2
    qba     L_TX1_TRANSMIT_BITS

;---------------------- buffer copy wraps ------------------------------
L_DATA_WRAP:
; copy happens in 2 chunks
; copy from BUFFER_START
    sub     r1.w0, r1.w2, BUFFER_END ; r5.w0 holds bytes to copy through BUFFER_END
    lbco    &r2, c24, READ_PTR, r5.w0  ; copy r5.w0 bytes; cc 4
    sub     r5.w2, r1.w0, r1.w0 ; r5.w2 = total bytes (r1.w0) - r5.w0 
    lbco    &r4, c24, BUFFER_START, r5.w2 ; cc 4
; worst case is 8 clocks across the 2 lbco commands
; CASE 1: LBCO #1 is 6 bytes across 3 word boundaries --> 5 clocks
;         LBCO #2 is 2 bytes on 32 bit word alignment --> 3 clocks
; CASE 2: LBCO #1 is 2 bytes across 2 word boundaries --> 4 clocks
;         LBCO #2 is 6 bytes on 32 bit word alignment --> 4 clocks
; increment read buffer
    add     READ_PTR, BUFFER_START, r5.w2
    sbco    READ_PTR, c24, READ_PTR_ADDRESS, 2
; TODO: use MVIx to shift, is this logic correct?
; start with r1.b0 = 16 to point to r4.b0, then subtract r5.w2?
    ldi     r1.b0, 16
    sub     r1.b0, r1.b0, r5.w2
    MVID    *r1.b0, r4

;--------------------- transmit bits ----------------------------
L_TX1_TRANSMIT_BITS:
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

;------------------------ state logic ----------------------------------
; change state
    ldi     STATE, TX2_STATE 

; update counter
; if CNT_BYTES_LOADED = 1, we would have jumped to a dedicated function
; so don't need to handle that here
; each state checks CNT_BYTES_LOADED to see where the EOF function is
    ldi     CNT_BYTES_SENT, 1
    ldi     CNT_BYTES_LOADED, r1.w0
; save context for R0-R26
    xout    SCRATCH_PAD, &r0, 108
    qba     L_END_M_TX1

L_SEND_1_BYTE:

; is the data wrapping around?
    add     r1.w2, READ_PTR, r1.w0 ; READ_PTR + read bytes
; TODO: is this logic correct? What if READ_PTR starts at BUFFER_END?
    qbgt    L_1_BYTE_DATA_WRAP, BUFFER_END, r1.w2; branch if r1.w2 > BUFFER_END

; load the 1 byte of data
    lbco    &r2, c24, READ_PTR, r1.w0     ; cc 3
; TODO: is this logic correct if we have a wraparound?
; READ_PTR increments to keep track of where to grab data from next
    add     READ_PTR, READ_PTR, r1.w0
    sbco    READ_PTR, c24, READ_PTR_ADDRESS, 2 ; update the variable for Linux
    qba     L_CALL_EOF_BYTE1

L_1_BYTE_DATA_WRAP:
    lbco    &r2, c24, BUFFER_START, r1.w0     ; cc 3
; READ_PTR increments to keep track of where to grab data from next
    add     READ_PTR, BUFFER_START, r1.w0
    sbco    READ_PTR, c24, READ_PTR_ADDRESS, 2 ; update the variable for Linux

L_CALL_EOF_BYTE1:
; call EOF_BYTE1
    m_eof_byte1 SCRATCH_PAD, STATE

L_END_M_TX1:
    .endm


;*******************************************************************************
; Macro: m_eof_byte1
;
; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     13 cycles
;        ? cycles to get here
;        ? cycles for a wrapped buffer copy
;        ? cycles for a non-wrapped buffer copy
;        ? cycle to qba to next channel
;*******************************************************************************

m_eof_byte1   .macro        SCRATCH_PAD, STATE

; this function only called from m_tx1
; so can use all the registers used in m_tx1
; data is already loaded in m_tx1

;--------------------- transmit bits ----------------------------
; make space for start bit - bit 0 already 0 after shift operand
    lsl     r0.b2, r2.b0, 1
; convert 2nd data byte to UART symbol
; d7 shift down by 7
    lsr     r1.b2, r2.b0, 7
; TODO: Finish the logic to set the rest of the FIFO to 1
; d0-d4 shift up by 3
    lsl     r1.b3, r2.b1, 3
; combine both into single byte
    or      r0.b3, r1.b2, r1.b3
; set stop bit
    set     r0.b3, r0.b3, 1
; send to tx fifo
    mov     r30.b0, r0.b2
    mov     r30.b0, r0.b3

; all bytes have been sent
; set STATE to IDLE_STATE
    ldi     STATE, IDLE_STATE

;------------------------ Is there more active data? ----------------------------------
; Check to see if the buffer has active data
; r26 has both READ_PTR (w0) & WRITE_PTR (w2)
    lbco    &r26, c24, TX_CH0_READ_PTR, 4  ; cc 3

; if READ_PTR == WRITE_PTR, buffer is empty
    qbeq    L_END_M_EOF_BYTE1, READ_PTR, WRITE_PTR

; else, the buffer has active data

; set STATE to TX1_STATE
    ldi     STATE, TX1_STATE

L_END_M_EOF_BYTE1:
; return to m_tx1
    .endm


;*******************************************************************************
; Macro: m_tx2
;
; 2nd 16 bit data which is spread into r2.b1-b3
; Data pattern is: D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     ?? cycles
;*******************************************************************************

m_tx2      .macro    SCRATCH_PAD

; TODO: Add logic to check for EOF states

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

; TODO: add m_eof_byte2 & m_eof_byte3

;*******************************************************************************
; Macro: m_tx3
;
; cnt16d: 2 (third or every 5th + 2)
; Data pattern is: D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     10 cycles
;*******************************************************************************

m_tx3   .macro    SCRATCH_PAD

; TODO: Add logic to check for EOF states

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

; TODO: add m_eof_byte4

;*******************************************************************************
; Macro: m_tx4
;
; cnt16d: 3 (fourth or every 5th + 3)
; Data pattern is: D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;
;
; TODO: Calculate cycles
; PEAK cycles:
;     ?? cycles
;*******************************************************************************

m_tx4   .macro    SCRATCH_PAD

; TODO: Add EOF 5 & 6

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

; TODO: add m_eof_byte5 & 6

;*******************************************************************************
; Macro: m_tx5
;
; cnt16d: 4 (fifth or every 5th + 2)
; Data pattern is: D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;
;
; PEAK cycles:
;     12 cycles
;*******************************************************************************

m_tx5    .macro    SCRATCH_PAD

; TODO: Add EOF 7 & 8

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

; TODO: add m_eof_byte7 & 8
; eof8 is based on the standard tx_eof macro

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
