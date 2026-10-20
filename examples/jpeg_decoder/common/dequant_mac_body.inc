; File: dequant_mac_body.inc

; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
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
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Pseudo code: for 64 coefficients, multiply by quantizer; store int32.
; PEAK / local WCET bound: 64 * 40 + 16 = 2576 instruction cycles.
; Each coefficient follows at most the negative multiply path; no data loop
; can exceed 64 iterations. Registers modified are listed below.
; Timing basis: conservative static instruction counting, not measured
; stage timing. Frame totals in the README are measured on silicon.
; Local memory bursts count transfer words plus instruction overhead.
; External memory contention has no finite bound here; the host watchdog
; bounds end-to-end waiting and disables both cores on failure.
; Dequantizer (MAC) kernel body, shared by the single-core and dual-core projects.
; In:  r10 = uint16 qtable address, r11 = natural-order int16 coefficients, r12 = int32 output.
; Clobbers r1, r5, r6, r14, r15, r25..r29. Falls through at the end (the caller halts or loops).
    ldi r25, 0
    xout 0, &r25, 1
    nop
    ldi r1, 64
    ldi r14, 0
block_loop:
    ldi r5, 0                 ; LBBO of two bytes otherwise preserves r5.w2
    lbbo &r5, r11, r14, 2
    qbeq is_zero, r5, 0
    ldi r6, 0                 ; qtable is an unsigned 16-bit value
    lbbo &r6, r10, r14, 2
    ; Silicon (LP-AM261): the MAC samples R28/R29 one cycle late, so the last
    ; operand write must be >= 1 instruction before the XIN. Back-to-back
    ; mov r29 / xin returned r28 x (previous r29) on hardware.
    ; Both operands are written before the branch, so the
    ; qbbs is the gap on the positive path at no extra cost.
    mov r29, r6
    mov r28, r5
    qbbs negative, r5, 15     ; test the loaded int16 sign bit, not bit 31
    xin 0, &r26, 4
    qba store
negative:
    rsb r28.w0, r5.w0, 0      ; 16-bit negation: 0xff20 -> 224, high word stays 0
    nop                       ; MAC operand settle (see above)
    xin 0, &r26, 4
    rsb r26, r26, 0
    qba store
is_zero:
    ldi r26, 0
store:
    lsl r15, r14, 1
    sbbo &r26, r12, r15, 4
    add r14, r14, 2
    sub r1, r1, 1
    qbne block_loop, r1, 0
