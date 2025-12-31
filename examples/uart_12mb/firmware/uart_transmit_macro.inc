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

; CNT_BYTES_LOADED holds number of bytes to copy
    sub     CNT_BYTES_LOADED, WRITE_PTR, READ_PTR    ; raw difference
    and     CNT_BYTES_LOADED, CNT_BYTES_LOADED, 0x7FF         ; handle wraparound using 2k buffer
    min     CNT_BYTES_LOADED, CNT_BYTES_LOADED, 8      ; if CNT_BYTES_LOADED > 8, set CNT_BYTES_LOADED = 8

; TODO: Add logic to jump to a special function if CNT_BYTES_LOADED = 1 byte
; should this be a jump to EOF_BYTE1?
    qbeq    L_SEND_1_BYTE, CNT_BYTES_LOADED, 1

; is the data wrapping around?
    add     r1.w2, READ_PTR, CNT_BYTES_LOADED ; READ_PTR + read bytes
; TODO: is this logic correct? What if READ_PTR starts at BUFFER_END?
    qbgt    L_DATA_WRAP, BUFFER_END, r1.w2; branch if r1.w2 > BUFFER_END

;------------------ buffer copy does not wrap -----------------------
; if not, load data in a single command
; up to 8 bytes of data is stored in r2 & r3
    lbco    &r2, c24, READ_PTR, CNT_BYTES_LOADED     ; up to 2 32-bit words
; these up to 8 bytes could be not word aligned, so worst case is -> cc 5
; READ_PTR increments to keep track of where to grab data from next
    add     READ_PTR, READ_PTR, CNT_BYTES_LOADED
    sbco    READ_PTR, c24, READ_PTR_ADDRESS, 2
    qba     L_TX1_TRANSMIT_BITS

;---------------------- buffer copy wraps ------------------------------
L_DATA_WRAP:

; copy happens in 2 chunks
; up to 8 bytes of data is stored in r2 & r3
; up to 8 bytes of data is temporarily stored in r4 & r5

; If r6.w0 == zero, it would hang the lbco command for "copy until BUFFER END"
; If r1.w2 == BUFFER END, jump to "copy from BUFFER_START"
    sub     r6.w0, r1.w2, BUFFER_END ; r6.w0 holds bytes to copy through BUFFER_END
    qbeq    L_COPY_BUFFER_START, r1.w2, BUFFER_END

; copy until BUFFER_END
    lbco    &r2, c24, READ_PTR, r6.w0  ; copy r6.w0 bytes; cc 3-4

L_COPY_BUFFER_START:
; copy from BUFFER_START
    sub     r6.w2, CNT_BYTES_LOADED, r6.w0 ; r6.w2 = total bytes (CNT_BYTES_LOADED - r6.w0)
    lbco    &r4, c24, BUFFER_START, r6.w2 ; cc 3-4

; since both BUFFER_START and BUFFER_END are 32-bit word aligned,
; worst case is 7 clocks to perform the 2 lbco commands
; CASE 1: LBCO #1 is 5-7 bytes across the last 32-bit word boundary --> 4 clocks
;         LBCO #2 is 1-3 bytes in the first 32-bit word --> 3 clocks
; CASE 2: LBCO #1 is 1-3 bytes in the last 32-bit word --> 3 clocks
;         LBCO #2 is 5-7 bytes across the first 32-bit word boundary --> 4 clocks

; increment read buffer
    add     READ_PTR, BUFFER_START, r6.w2
    sbco    READ_PTR, c24, READ_PTR_ADDRESS, 2

; use MVIx to shift any data from r4 & r5 down to r2 & r3
; TODO: Is this logic correct?
; start with r1.b0 = 8 to point to r2.b0
; add r6.w0 to increase the pointer by the number of bytes already copied to r2-r3
;    example: r6.w0 = 3
;    shift the data in r4.b0-r4.b2 to a byte offset of 8+3=11 -> r2.b3-r3.b2
    ldi     r1.b0, 8
    add     r1.b0, r1.b0, r6.w0
    mvid    *r1.b0, r4
; shift data in r5 to 4 bytes above where the data in r4 was placed
;    example: r6.w0 = 3
;    shift the data in r5.b0-r5.b2 to a byte offset of 11+4=15 -> r3.b3-r4.b2
;    But we copy 8 bytes max. Thus r5 has at most 1 byte of data in this example
;    that possible 1 byte of data would go into r3.b3
    add     r1.b0, r1.b0, 4
    mvid    *r1.b0, r5

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

    qba     L_END_M_TX1

L_SEND_1_BYTE:

; is the data wrapping around?
    add     r1.w2, READ_PTR, CNT_BYTES_LOADED ; READ_PTR + read bytes
; TODO: is this logic correct? What if READ_PTR starts at BUFFER_END?
    qbgt    L_1_BYTE_DATA_WRAP, BUFFER_END, r1.w2; branch if r1.w2 > BUFFER_END

; load the 1 byte of data
    lbco    &r2, c24, READ_PTR, CNT_BYTES_LOADED     ; cc 3
; TODO: is this logic correct if we have a wraparound?
; READ_PTR increments to keep track of where to grab data from next
    add     READ_PTR, READ_PTR, CNT_BYTES_LOADED
    sbco    READ_PTR, c24, READ_PTR_ADDRESS, 2 ; update the variable for Linux
    qba     L_CALL_EOF_BYTE1

L_1_BYTE_DATA_WRAP:
    lbco    &r2, c24, BUFFER_START, CNT_BYTES_LOADED     ; cc 3
; READ_PTR increments to keep track of where to grab data from next
    add     READ_PTR, BUFFER_START, CNT_BYTES_LOADED
    sbco    READ_PTR, c24, READ_PTR_ADDRESS, 2 ; update the variable for Linux

L_CALL_EOF_BYTE1:
; call EOF_BYTE1
    m_eof_byte1 SCRATCH_PAD, STATE

L_END_M_TX1:
; save context for R0-R26
    xout    SCRATCH_PAD, &r0, 108
    .endm


;*******************************************************************************
; Macro: m_eof_byte1
;
; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So 1  1  1  1  1  1  (16 bit)
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
; combine D7 with stop bit and padding bits (0xFE = 11111110)
    or      r0.b3, r1.b2, 0xFE
; send to tx fifo
    mov     r30.b0, r0.b2
    mov     r30.b0, r0.b3

; all bytes have been sent
; set STATE to IDLE_STATE
    ldi     STATE, IDLE_STATE

;------------------------ Is there more active data? ----------------------------------
; Check to see if the buffer has active data
; r26 has both READ_PTR (w0) & WRITE_PTR (w2)
    lbco    &r26, c24, READ_PTR_ADDRESS, 4  ; cc 3

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

m_tx2      .macro    SCRATCH_PAD, STATE

; check for EOF conditions
    qbeq     L_CALL_EOF_BYTE2, CNT_BYTES_LOADED, 2
    qbeq     L_CALL_EOF_BYTE3, CNT_BYTES_LOADED, 3

; original macro START
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
    qba      L_END_M_TX2

L_CALL_EOF_BYTE2:
    m_eof_byte2 SCRATCH_PAD, STATE
    qba      L_END_M_TX2

L_CALL_EOF_BYTE3:
    m_eof_byte3 SCRATCH_PAD, STATE

L_END_M_TX2:
; save context for R0-R26
    xout     SCRATCH_PAD, &r0, 108
    .endm


;*******************************************************************************
; Macro: m_eof_byte2
;
; Data pattern is: D5 D6 D7 So 1  1  1  1   (8 bit)
; bit count        1  2  3  4  5  6  7  8
;
; Called from m_tx2 when CNT_BYTES_LOADED = 2
; Finishes transmitting byte 1 and pads with 1s
;
; PEAK cycles:
;     ~10 cycles
;*******************************************************************************

m_eof_byte2   .macro        SCRATCH_PAD, STATE

; data already loaded by m_tx2 into r2 via mvid
; r2.b1 contains byte 1

;--------------------- transmit bits ----------------------------
; extract D5 D6 D7 from byte 1 and place at bit positions 0-2
    lsr     r2.b0, r2.b1, 5
; add stop bit and padding 1s (0xF8 = 11111000)
    or      r2.b0, r2.b0, 0xF8
; send to tx fifo (8 bits)
    mov     r30.b0, r2.b0

; all bytes have been sent
; set STATE to IDLE_STATE
    ldi     STATE, IDLE_STATE

;------------------------ Is there more active data? ----------------------------------
; Check to see if the buffer has active data
; r26 has both READ_PTR (w0) & WRITE_PTR (w2)
    lbco    &r26, c24, READ_PTR_ADDRESS, 4  ; cc 3

; if READ_PTR == WRITE_PTR, buffer is empty
    qbeq    L_END_M_EOF_BYTE2, READ_PTR, WRITE_PTR

; else, the buffer has active data
; set STATE to TX1_STATE
    ldi     STATE, TX1_STATE

L_END_M_EOF_BYTE2:
; return to m_tx2
    .endm


;*******************************************************************************
; Macro: m_eof_byte3
;
; Data pattern is: D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So 1  1   (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; Called from m_tx2 when CNT_BYTES_LOADED = 3
; Finishes transmitting byte 1, transmits byte 2, and pads with 1s
;
; PEAK cycles:
;     ~13 cycles
;*******************************************************************************

m_eof_byte3   .macro        SCRATCH_PAD, STATE

; data already loaded by m_tx2 into r2 via mvid
; r2.b1 = byte 1, r2.b2 = byte 2

;--------------------- transmit bits ----------------------------
; First 8 bits: D5 D6 D7 So Sa D0 D1 D2
; extract D5 D6 D7 from byte 1 (shift right by 5)
    lsr     r0.b0, r2.b1, 5
; set stop bit at position 3
    set     r0.b0, r0.b0, 3
; extract D0 D1 D2 from byte 2 and shift to positions 5-7
    lsl     r0.b1, r2.b2, 5
; combine
    or      r0.b0, r0.b0, r0.b1

; Second 8 bits: D3 D4 D5 D6 D7 So 1 1
; extract D3 D4 D5 D6 D7 from byte 2 (shift right by 3)
    lsr     r0.b1, r2.b2, 3
; add stop bit and padding 1s (0xE0 = 11100000)
    or      r0.b1, r0.b1, 0xE0

; send to tx fifo (16 bits)
    mov     r30.b0, r0.b0
    mov     r30.b0, r0.b1

; all bytes have been sent
; set STATE to IDLE_STATE
    ldi     STATE, IDLE_STATE

;------------------------ Is there more active data? ----------------------------------
; Check to see if the buffer has active data
; r26 has both READ_PTR (w0) & WRITE_PTR (w2)
    lbco    &r26, c24, READ_PTR_ADDRESS, 4  ; cc 3

; if READ_PTR == WRITE_PTR, buffer is empty
    qbeq    L_END_M_EOF_BYTE3, READ_PTR, WRITE_PTR

; else, the buffer has active data
; set STATE to TX1_STATE
    ldi     STATE, TX1_STATE

L_END_M_EOF_BYTE3:
; return to m_tx2
    .endm


;*******************************************************************************
; Macro: m_tx3
;
; Data pattern is: D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     10 cycles
;*******************************************************************************

m_tx3   .macro    SCRATCH_PAD, STATE

; shift data
    ldi      r1.b0, 10 ; shift data from register offset at 10 bytes = r2.b2
    mvid     r2, *r1.b0 ; shift 32 bits from offset r2.b2 down to r2.b0

; byte  | contains data originally copied to
; ------------------------------------------
; r2.b0 | r2.b2
; r2.b1 | r2.b3
; r2.b2 | r3.b0
; r2.b3 | r3.b1

; check for EOF condition
    qbeq     L_CALL_EOF_BYTE4, CNT_BYTES_LOADED, 4

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
    qba      L_END_M_TX3

L_CALL_EOF_BYTE4:
    m_eof_byte4 SCRATCH_PAD, STATE

L_END_M_TX3:
; save context for R0-R26
    xout     SCRATCH_PAD, &r0, 108
    .endm


;*******************************************************************************
; Macro: m_eof_byte4
;
; Data pattern is: D1 D2 D3 D4 D5 D6 D7 So 1  1  1  1  1  1  1  1   (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; Called from m_tx3 when CNT_BYTES_LOADED = 4
; Finishes transmitting byte 3 and pads with 1s
;
; PEAK cycles:
;     ~10 cycles
;*******************************************************************************

m_eof_byte4   .macro        SCRATCH_PAD, STATE

; data already loaded by m_tx3 into r2 via mvid
; r2.b1 contains byte 3

;--------------------- transmit bits ----------------------------
; First 8 bits: D1 D2 D3 D4 D5 D6 D7 So
; extract D1-D7 from byte 3 (shift right by 1)
    lsr     r0.b0, r2.b1, 1
; set stop bit at position 7
    set     r0.b0, r0.b0, 7

; Second 8 bits: all padding 1s
    ldi     r0.b1, 0xFF

; send to tx fifo (16 bits)
    mov     r30.b0, r0.b0
    mov     r30.b0, r0.b1

; all bytes have been sent
; set STATE to IDLE_STATE
    ldi     STATE, IDLE_STATE

;------------------------ Is there more active data? ----------------------------------
; Check to see if the buffer has active data
; r26 has both READ_PTR (w0) & WRITE_PTR (w2)
    lbco    &r26, c24, READ_PTR_ADDRESS, 4  ; cc 3

; if READ_PTR == WRITE_PTR, buffer is empty
    qbeq    L_END_M_EOF_BYTE4, READ_PTR, WRITE_PTR

; else, the buffer has active data
; set STATE to TX1_STATE
    ldi     STATE, TX1_STATE

L_END_M_EOF_BYTE4:
; return to m_tx3
    .endm


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

m_tx4   .macro    SCRATCH_PAD, STATE

; shift data
    add      r1.b0, r1.b0, 2 ; shift data from register offset at 10+2=12 bytes = r3.b0
    mvid     r2, *r1.b0 ; shift 32 bits from offset r3.b0 down to r2.b0

; byte  | contains data originally copied to
; ------------------------------------------
; r2.b0 | r3.b0
; r2.b1 | r3.b1
; r2.b2 | r3.b2
; r2.b3 | r3.b3

; check for EOF conditions
    qbeq     L_CALL_EOF_BYTE5, CNT_BYTES_LOADED, 5
    qbeq     L_CALL_EOF_BYTE6, CNT_BYTES_LOADED, 6

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
    qba     L_END_M_TX4

L_CALL_EOF_BYTE5:
    m_eof_byte5 SCRATCH_PAD, STATE
    qba     L_END_M_TX4

L_CALL_EOF_BYTE6:
    m_eof_byte6 SCRATCH_PAD, STATE

L_END_M_TX4:
; save context for R0-R26
    xout    SCRATCH_PAD, &r0, 108
    .endm


;*******************************************************************************
; Macro: m_eof_byte5
;
; Data pattern is: D7 So 1  1  1  1  1  1   (8 bit)
; bit count        1  2  3  4  5  6  7  8
;
; Called from m_tx4 when CNT_BYTES_LOADED = 5
; Finishes transmitting byte 4 and pads with 1s
;
; PEAK cycles:
;     ~10 cycles
;*******************************************************************************

m_eof_byte5   .macro        SCRATCH_PAD, STATE

; data already loaded by m_tx4 into r2 via mvid
; r2.b0 contains byte 4

;--------------------- transmit bits ----------------------------
; extract D7 from byte 4 (shift right by 7)
    lsr     r0.b0, r2.b0, 7
; add stop bit and padding 1s (0xFE = 11111110)
    or      r0.b0, r0.b0, 0xFE
; send to tx fifo (8 bits)
    mov     r30.b0, r0.b0

; all bytes have been sent
; set STATE to IDLE_STATE
    ldi     STATE, IDLE_STATE

;------------------------ Is there more active data? ----------------------------------
; Check to see if the buffer has active data
; r26 has both READ_PTR (w0) & WRITE_PTR (w2)
    lbco    &r26, c24, READ_PTR_ADDRESS, 4  ; cc 3

; if READ_PTR == WRITE_PTR, buffer is empty
    qbeq    L_END_M_EOF_BYTE5, READ_PTR, WRITE_PTR

; else, the buffer has active data
; set STATE to TX1_STATE
    ldi     STATE, TX1_STATE

L_END_M_EOF_BYTE5:
; return to m_tx4
    .endm


;*******************************************************************************
; Macro: m_eof_byte6
;
; Data pattern is: D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So 1  1  1  1   (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; Called from m_tx4 when CNT_BYTES_LOADED = 6
; Finishes transmitting byte 4, transmits byte 5, and pads with 1s
;
; PEAK cycles:
;     ~13 cycles
;*******************************************************************************

m_eof_byte6   .macro        SCRATCH_PAD, STATE

; data already loaded by m_tx4 into r2 via mvid
; r2.b0 = byte 4, r2.b1 = byte 5

;--------------------- transmit bits ----------------------------
; First 8 bits: D7 So Sa D0 D1 D2 D3 D4
; extract D7 from byte 4 (shift right by 7)
    lsr     r0.b0, r2.b0, 7
; set stop bit at position 1
    set     r0.b0, r0.b0, 1
; extract D0-D4 from byte 5 and shift to positions 3-7
    lsl     r0.b1, r2.b1, 3
; combine
    or      r0.b0, r0.b0, r0.b1

; Second 8 bits: D5 D6 D7 So 1 1 1 1
; extract D5 D6 D7 from byte 5 (shift right by 5)
    lsr     r0.b1, r2.b1, 5
; add stop bit and padding 1s (0xF8 = 11111000)
    or      r0.b1, r0.b1, 0xF8

; send to tx fifo (16 bits)
    mov     r30.b0, r0.b0
    mov     r30.b0, r0.b1

; all bytes have been sent
; set STATE to IDLE_STATE
    ldi     STATE, IDLE_STATE

;------------------------ Is there more active data? ----------------------------------
; Check to see if the buffer has active data
; r26 has both READ_PTR (w0) & WRITE_PTR (w2)
    lbco    &r26, c24, READ_PTR_ADDRESS, 4  ; cc 3

; if READ_PTR == WRITE_PTR, buffer is empty
    qbeq    L_END_M_EOF_BYTE6, READ_PTR, WRITE_PTR

; else, the buffer has active data
; set STATE to TX1_STATE
    ldi     STATE, TX1_STATE

L_END_M_EOF_BYTE6:
; return to m_tx4
    .endm


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

m_tx5    .macro    SCRATCH_PAD, STATE

; check for EOF conditions
    qbeq     L_CALL_EOF_BYTE7, CNT_BYTES_LOADED, 7
    qbeq     L_CALL_EOF_BYTE8, CNT_BYTES_LOADED, 8

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
    qba     L_END_M_TX5

L_CALL_EOF_BYTE7:
    m_eof_byte7 SCRATCH_PAD, STATE
    qba     L_END_M_TX5

L_CALL_EOF_BYTE8:
    m_eof_byte8 SCRATCH_PAD, STATE

L_END_M_TX5:
; save context for R0-R26
    xout    SCRATCH_PAD, &r0, 108
    .endm


;*******************************************************************************
; Macro: m_eof_byte7
;
; Data pattern is: D3 D4 D5 D6 D7 So 1  1  (8 bit)
; bit count        1  2  3  4  5  6  7  8
;
; Called from m_tx5 when CNT_BYTES_LOADED = 7
; Finishes transmitting byte 6 and pads with 1s
;
; PEAK cycles:
;     ~10 cycles
;*******************************************************************************

m_eof_byte7   .macro        SCRATCH_PAD, STATE

; data already loaded by m_tx5 into r2 via mvid
; r2.b2 contains byte 6

;--------------------- transmit bits ----------------------------
; extract D3-D7 from byte 6 (shift right by 3)
    lsr     r0.b0, r2.b2, 3
; add stop bit and padding 1s (0xE0 = 11100000)
    or      r0.b0, r0.b0, 0xE0
; send to tx fifo (8 bits)
    mov     r30.b0, r0.b0

; all bytes have been sent
; set STATE to IDLE_STATE
    ldi     STATE, IDLE_STATE

;------------------------ Is there more active data? ----------------------------------
; Check to see if the buffer has active data
; r26 has both READ_PTR (w0) & WRITE_PTR (w2)
    lbco    &r26, c24, READ_PTR_ADDRESS, 4  ; cc 3

; if READ_PTR == WRITE_PTR, buffer is empty
    qbeq    L_END_M_EOF_BYTE7, READ_PTR, WRITE_PTR

; else, the buffer has active data
; set STATE to TX1_STATE
    ldi     STATE, TX1_STATE

L_END_M_EOF_BYTE7:
; return to m_tx5
    .endm


;*******************************************************************************
; Macro: m_eof_byte8
;
; Data pattern is: D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; Called from m_tx5 when CNT_BYTES_LOADED = 8
; Finishes transmitting byte 6 and transmits complete byte 7 (no padding needed)
;
; PEAK cycles:
;     ~12 cycles
;*******************************************************************************

m_eof_byte8   .macro        SCRATCH_PAD, STATE

; data already loaded by m_tx5 into r2 via mvid
; r2.b2 = byte 6, r2.b3 = byte 7

;--------------------- transmit bits ----------------------------
; First 8 bits: D3 D4 D5 D6 D7 So Sa D0
; extract D3-D7 from byte 6 (shift right by 3)
    lsr     r0.b0, r2.b2, 3
; set stop bit at position 5
    set     r0.b0, r0.b0, 5
; extract D0 from byte 7 and shift to position 7
    lsl     r0.b1, r2.b3, 7
; combine
    or      r0.b0, r0.b0, r0.b1

; Second 8 bits: D1 D2 D3 D4 D5 D6 D7 So
; extract D1-D7 from byte 7 (shift right by 1)
    lsr     r0.b1, r2.b3, 1
; set stop bit at position 7
    set     r0.b1, r0.b1, 7

; send to tx fifo (16 bits)
    mov     r30.b0, r0.b0
    mov     r30.b0, r0.b1

; all bytes have been sent
; set STATE to IDLE_STATE
    ldi     STATE, IDLE_STATE

;------------------------ Is there more active data? ----------------------------------
; Check to see if the buffer has active data
; r26 has both READ_PTR (w0) & WRITE_PTR (w2)
    lbco    &r26, c24, READ_PTR_ADDRESS, 4  ; cc 3

; if READ_PTR == WRITE_PTR, buffer is empty
    qbeq    L_END_M_EOF_BYTE8, READ_PTR, WRITE_PTR

; else, the buffer has active data
; set STATE to TX1_STATE
    ldi     STATE, TX1_STATE

L_END_M_EOF_BYTE8:
; return to m_tx5
    .endm


