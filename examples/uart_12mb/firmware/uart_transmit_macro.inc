; Copyright (C) 2025 Texas Instruments Incorporated - http://www.ti.com/
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions
; are met:
;
;        * Redistributions of source code must retain the above copyright
;          notice, this list of conditions and the following disclaimer.
;
;        * Redistributions in binary form must reproduce the above copyright
;          notice, this list of conditions and the following disclaimer in the
;          documentation and/or other materials provided with the
;          distribution.
;
;        * Neither the name of Texas Instruments Incorporated nor the names of
;          its contributors may be used to endorse or promote products derived
;          from this software without specific prior written permission.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
; "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
; LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
; A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
; OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
; SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
; LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
; DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
; THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


;   filename:     uart_transmit_macro.inc
;   description:  repeating macros for transmit
;
;   name:         Thomas Leyrer       date: 14.10.2025

; ***********************************************************************


; CH0_SOF state comes first as it takes most cycle
; *************************** State CH0_SOF ********************************************
; budget is ~ 50 cycles per substate - cycle counter cc is documented on each instruction
; context was initialized on EOF

; convert 8 bit data to 10 bit UART symbol with 1 start bit '0' and one stop bit '1'
; UART is LSB first which means the first bit on wire is start bit follwed by bit 0 of data bits.
; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; make space for start bit - bit 0 already 0 after shift operand

m_tx_sof    .macro
    lsl     r0.b2, r2.b0, 1
; convert 2nd data byte to UART sysmbol
; d7 shift down by 7
    lsr     r1.b2, r2.b0, 7
; d0-d4 sift uo by 3
    lsl     r1.b3, r2.b1, 3
; combine both into single byte
    or      r0.b3, r1.b2, r1.b3
; set stop bit
    set     r0.b3, r0.b3, 1
; send to tx fifo
    mov     r30.b0, r0.b2
    mov     r30.b0, r0.b3
    .endm

; *************************** State CHn_NEXT1 ******************************************
; now get back to 2nd 16 bit data which is spread into r2.b1-b3
; Data pattern is: D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
m_tx_next1    .macro

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
    .endm


; *************************** State CHn_NEXTF ******************************************

; in this state there are 5 different patterns depending on the 16 bit data count cnt16d
; cnt16d: 0 (first or every 5th)
; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; cnt16d: 1 (second or every 5th +1)
; Data pattern is: D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16

; cnt16d: 2 (third or every 5th + 2)
; Data pattern is: D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16

; cnt16d: 3 (fourth or every 5th + 3)
; Data pattern is: D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;

; cnt16d: 4 (fifth or every 5th + 2)
; Data pattern is: D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;

; need a absolute count and relative count as mod 5 is not easly done

; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16

; ***********************************************************************
m_tx_nextf1   .macro

; get data shifted down to r2
    mvid     r2, *r1.b0
; make space for start bit - bit 0 already 0 after shift operand
    lsl     r0.b2, r2.b0, 1
; convert 2nd data byte to UART sysmbol
; d7 shift down by 7
    lsr     r1.b2, r2.b0, 7
; d0-d4 sift uo by 3
    lsl     r1.b3, r2.b1, 3
; combine both into single byte
    or      r0.b3, r1.b2, r1.b3
; set stop bit
    set     r0.b3, r0.b3, 1
; send to tx fifo
    mov     r30.b0, r0.b2
    mov     r30.b0, r0.b3
    .endm

; ***********************************************************************
; 2nd 16 bit data which is spread into r2.b1-b3
; Data pattern is: D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
m_tx_nextf2      .macro
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
    .endm

; ***********************************************************************
; cnt16d: 2 (third or every 5th + 2)
; Data pattern is: D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16

m_tx_nextf3   .macro

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
    .endm

; ***********************************************************************
; cnt16d: 3 (fourth or every 5th + 3)
; Data pattern is: D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;

m_tx_nextf4   .macro
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
    .endm

; ***********************************************************************
; cnt16d: 4 (fifth or every 5th + 2)
; Data pattern is: D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;

m_tx_nextf5    .macro
; get data shifted down to r2, auto increment for next block of 5
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
    .endm

; ***********************************************************************
; cnt16d: 4 (fifth or every 5th + 2)
; Data pattern is: D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;

m_tx_eof    .macro
; get data shifted down to r2, auto increment for next block of 5
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
    .endm
