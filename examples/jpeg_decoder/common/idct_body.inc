; File: idct_body.inc

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
; Pseudo code: transform 8 columns, then 8 rows, using zero shortcuts.
; PEAK / local WCET bound: 16 * 512 + 32 = 8224 instruction cycles.
; Each of the 16 transforms executes fewer than 512 local instruction cycles
; including local RAM bursts; zero shortcuts reduce that work.
; Registers modified are listed below; no direct GPO register is modified.
; Timing basis: conservative static instruction counting, not measured
; stage timing. Frame totals in the README are measured on silicon.
; Local memory bursts count transfer words plus instruction overhead.
; External memory contention has no finite bound here; the host watchdog
; bounds end-to-end waiting and disables both cores on failure.
; PRU integer IDCT (islow): zero-skip, MAC multiplies, branchless DESCALE, packed row store.
; Kernel body: the includer defines IDCT_IN (int32 coefficients,
; 256 B), IDCT_WS (256 B workspace), IDCT_OUT (128 B packed int16 rows).
; Clobbers r0..r16, r19, r20, r23..r29. Falls through at the end.
    ldi r23, 0
    xout 0, &r23, 1
    ldi32 r27, IDCT_IN
    ldi32 r25, IDCT_WS
    ldi32 r24, IDCT_OUT
    ldi r23, 0
    ldi32 r19, 0x80000400
    ldi32 r20, 0x00100000
col_loop:
    lsl r12, r23, 2
    add r12, r12, r27
    lbbo &r0, r12, 0, 4
    lbbo &r1, r12, 32, 4
    lbbo &r2, r12, 64, 4
    lbbo &r3, r12, 96, 4
    lbbo &r4, r12, 128, 4
    lbbo &r5, r12, 160, 4
    lbbo &r6, r12, 192, 4
    lbbo &r7, r12, 224, 4
    qbne col_full_0, r1, 0
    qbne col_full_0, r2, 0
    qbne col_full_0, r3, 0
    qbne col_full_0, r4, 0
    qbne col_full_0, r5, 0
    qbne col_full_0, r6, 0
    qbne col_full_0, r7, 0
    lsl r0, r0, 2
    lsl r12, r23, 2
    add r12, r12, r25
    sbbo &r0, r12, 0, 4
    sbbo &r0, r12, 32, 4
    sbbo &r0, r12, 64, 4
    sbbo &r0, r12, 96, 4
    sbbo &r0, r12, 128, 4
    sbbo &r0, r12, 160, 4
    sbbo &r0, r12, 192, 4
    sbbo &r0, r12, 224, 4
    add r23, r23, 1
    qbne col_loop, r23, 8
    qba col_done
col_full_0:
    ; ---- 1D shift 11 (MAC) ----
    add r28, r2, r6
    ldi r29, 4433
    lsl r13, r0, 13
    xin 0, &r26, 4
    mov r28, r6
    ldi r29, 15137
    mov r8, r26
    xin 0, &r26, 4
    mov r28, r2
    ldi r29, 6270
    sub r15, r8, r26
    xin 0, &r26, 4
    add r16, r8, r26
    add r13, r13, r19
    lsl r14, r4, 13
    add r9, r13, r14
    sub r14, r13, r14
    add r0, r9, r16
    sub r6, r9, r16
    add r2, r14, r15
    add r10, r7, r3
    add r11, r5, r1
    add r28, r10, r11
    ldi r29, 9633
    sub r4, r14, r15
    xin 0, &r26, 4
    mov r28, r10
    ldi r29, 16069
    mov r12, r26
    xin 0, &r26, 4
    mov r28, r11
    ldi r29, 3196
    sub r10, r12, r26
    xin 0, &r26, 4
    add r28, r7, r1
    ldi r29, 7373
    sub r11, r12, r26
    xin 0, &r26, 4
    add r28, r5, r3
    ldi r29, 20995
    mov r8, r26
    xin 0, &r26, 4
    mov r28, r7
    ldi r29, 2446
    mov r9, r26
    xin 0, &r26, 4
    mov r28, r5
    ldi r29, 16819
    sub r13, r26, r8
    add r13, r13, r10
    xin 0, &r26, 4
    mov r28, r3
    ldi r29, 25172
    sub r14, r26, r9
    add r14, r14, r11
    xin 0, &r26, 4
    mov r28, r1
    ldi r29, 12299
    sub r15, r26, r9
    add r15, r15, r10
    xin 0, &r26, 4
    sub r16, r26, r8
    add r16, r16, r11
    sub r7, r0, r16
    lsr r7, r7, 11
    sub r7, r7, r20
    add r8, r0, r16
    lsr r8, r8, 11
    sub r8, r8, r20
    add r1, r2, r15
    lsr r1, r1, 11
    sub r1, r1, r20
    sub r11, r2, r15
    lsr r11, r11, 11
    sub r11, r11, r20
    add r9, r4, r14
    lsr r9, r9, 11
    sub r9, r9, r20
    sub r5, r4, r14
    lsr r5, r5, 11
    sub r5, r5, r20
    add r3, r6, r13
    lsr r3, r3, 11
    sub r3, r3, r20
    sub r10, r6, r13
    lsr r10, r10, 11
    sub r10, r10, r20
    lsl r12, r23, 2
    add r12, r12, r25
    sbbo &r8, r12, 0, 4
    sbbo &r1, r12, 32, 4
    sbbo &r9, r12, 64, 4
    sbbo &r3, r12, 96, 4
    sbbo &r10, r12, 128, 4
    sbbo &r5, r12, 160, 4
    sbbo &r11, r12, 192, 4
    sbbo &r7, r12, 224, 4
    add r23, r23, 1
    qbne col_loop, r23, 8
col_done:
    ldi r23, 0
    ldi32 r19, 0x80020000
    ldi32 r20, 0x00002000
row_loop:
    lsl r12, r23, 5
    add r12, r12, r25
    lbbo &r0, r12, 0, 32
    qbne row_full_1, r1, 0
    qbne row_full_1, r2, 0
    qbne row_full_1, r3, 0
    qbne row_full_1, r4, 0
    qbne row_full_1, r5, 0
    qbne row_full_1, r6, 0
    qbne row_full_1, r7, 0
    lsl r0, r0, 13
    add r0, r0, r19
    lsr r0, r0, 18
    sub r0, r0, r20
    lsl r12, r23, 4
    add r12, r12, r24
    mov r0.w2, r0.w0
    mov r1, r0
    mov r2, r0
    mov r3, r0
    sbbo &r0, r12, 0, 16
    add r23, r23, 1
    qbne row_loop, r23, 8
    qba row_done
row_full_1:
    ; ---- 1D shift 18 (MAC) ----
    add r28, r2, r6
    ldi r29, 4433
    lsl r13, r0, 13
    xin 0, &r26, 4
    mov r28, r6
    ldi r29, 15137
    mov r8, r26
    xin 0, &r26, 4
    mov r28, r2
    ldi r29, 6270
    sub r15, r8, r26
    xin 0, &r26, 4
    add r16, r8, r26
    add r13, r13, r19
    lsl r14, r4, 13
    add r9, r13, r14
    sub r14, r13, r14
    add r0, r9, r16
    sub r6, r9, r16
    add r2, r14, r15
    add r10, r7, r3
    add r11, r5, r1
    add r28, r10, r11
    ldi r29, 9633
    sub r4, r14, r15
    xin 0, &r26, 4
    mov r28, r10
    ldi r29, 16069
    mov r12, r26
    xin 0, &r26, 4
    mov r28, r11
    ldi r29, 3196
    sub r10, r12, r26
    xin 0, &r26, 4
    add r28, r7, r1
    ldi r29, 7373
    sub r11, r12, r26
    xin 0, &r26, 4
    add r28, r5, r3
    ldi r29, 20995
    mov r8, r26
    xin 0, &r26, 4
    mov r28, r7
    ldi r29, 2446
    mov r9, r26
    xin 0, &r26, 4
    mov r28, r5
    ldi r29, 16819
    sub r13, r26, r8
    add r13, r13, r10
    xin 0, &r26, 4
    mov r28, r3
    ldi r29, 25172
    sub r14, r26, r9
    add r14, r14, r11
    xin 0, &r26, 4
    mov r28, r1
    ldi r29, 12299
    sub r15, r26, r9
    add r15, r15, r10
    xin 0, &r26, 4
    sub r16, r26, r8
    add r16, r16, r11
    sub r7, r0, r16
    lsr r7, r7, 18
    sub r7, r7, r20
    add r8, r0, r16
    lsr r8, r8, 18
    sub r8, r8, r20
    add r1, r2, r15
    lsr r1, r1, 18
    sub r1, r1, r20
    sub r11, r2, r15
    lsr r11, r11, 18
    sub r11, r11, r20
    add r9, r4, r14
    lsr r9, r9, 18
    sub r9, r9, r20
    sub r5, r4, r14
    lsr r5, r5, 18
    sub r5, r5, r20
    add r3, r6, r13
    lsr r3, r3, 18
    sub r3, r3, r20
    sub r10, r6, r13
    lsr r10, r10, 18
    sub r10, r10, r20
    lsl r12, r23, 4
    add r12, r12, r24
    mov r8.w2, r1.w0
    mov r9.w2, r3.w0
    mov r10.w2, r5.w0
    mov r11.w2, r7.w0
    sbbo &r8, r12, 0, 16
    add r23, r23, 1
    qbne row_loop, r23, 8
row_done:
