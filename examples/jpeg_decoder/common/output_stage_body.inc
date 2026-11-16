; File: output_stage_body.inc

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
; Pseudo code: four times, load 16 samples, clamp, pack, store 16 bytes.
; PEAK / local WCET bound: 4 * 128 + 16 = 528 instruction cycles.
; The output MSRAM stores may stall beyond this local instruction bound.
; Registers modified are listed below.
; Timing basis: conservative static instruction counting, not measured
; stage timing. Frame totals in the README are measured on silicon.
; Local memory bursts count transfer words plus instruction overhead.
; External memory contention has no finite bound here; the host watchdog
; bounds end-to-end waiting and disables both cores on failure.
; Output stage kernel body (level shift, clamp, pack), shared by the single-core and dual-core projects.
; In:  r11 = int16 IDCT samples (64), r12 = packed uint8 output (64 B).
; Clobbers r1..r9, r14..r19, r21..r23. Ends at label `done`; the caller halts or loops.
    ldi32 r21, 0x80008000      ; sign bias
    ldi32 r22, 0x7F807F80      ; low clamp (32640)
    ldi32 r23, 0x807F807F      ; high clamp (32895)
    ldi r1, 4                  ; loop count
    ldi r14, 0                 ; input byte offset
    ldi r15, 0                 ; output byte offset
sample_loop:
    lbbo &r2, r11, r14, 32     ; load 16 samples

    xor r2, r2, r21
    xor r3, r3, r21
    xor r4, r4, r21
    xor r5, r5, r21
    xor r6, r6, r21
    xor r7, r7, r21
    xor r8, r8, r21
    xor r9, r9, r21

    max r2.w0, r2.w0, r22.w0
    max r3.w0, r3.w0, r22.w0
    max r4.w0, r4.w0, r22.w0
    max r5.w0, r5.w0, r22.w0
    max r6.w0, r6.w0, r22.w0
    max r7.w0, r7.w0, r22.w0
    max r8.w0, r8.w0, r22.w0
    max r9.w0, r9.w0, r22.w0

    max r2.w2, r2.w2, r22.w2
    max r3.w2, r3.w2, r22.w2
    max r4.w2, r4.w2, r22.w2
    max r5.w2, r5.w2, r22.w2
    max r6.w2, r6.w2, r22.w2
    max r7.w2, r7.w2, r22.w2
    max r8.w2, r8.w2, r22.w2
    max r9.w2, r9.w2, r22.w2

    min r2.w0, r2.w0, r23.w0
    min r3.w0, r3.w0, r23.w0
    min r4.w0, r4.w0, r23.w0
    min r5.w0, r5.w0, r23.w0
    min r6.w0, r6.w0, r23.w0
    min r7.w0, r7.w0, r23.w0
    min r8.w0, r8.w0, r23.w0
    min r9.w0, r9.w0, r23.w0

    min r2.w2, r2.w2, r23.w2
    min r3.w2, r3.w2, r23.w2
    min r4.w2, r4.w2, r23.w2
    min r5.w2, r5.w2, r23.w2
    min r6.w2, r6.w2, r23.w2
    min r7.w2, r7.w2, r23.w2
    min r8.w2, r8.w2, r23.w2
    min r9.w2, r9.w2, r23.w2

    sub r2.w0, r2.w0, r22.w0
    sub r3.w0, r3.w0, r22.w0
    sub r4.w0, r4.w0, r22.w0
    sub r5.w0, r5.w0, r22.w0
    sub r6.w0, r6.w0, r22.w0
    sub r7.w0, r7.w0, r22.w0
    sub r8.w0, r8.w0, r22.w0
    sub r9.w0, r9.w0, r22.w0

    sub r2.w2, r2.w2, r22.w2
    sub r3.w2, r3.w2, r22.w2
    sub r4.w2, r4.w2, r22.w2
    sub r5.w2, r5.w2, r22.w2
    sub r6.w2, r6.w2, r22.w2
    sub r7.w2, r7.w2, r22.w2
    sub r8.w2, r8.w2, r22.w2
    sub r9.w2, r9.w2, r22.w2

    mov r16.b0, r2.b0
    mov r16.b1, r2.b2
    mov r16.b2, r3.b0
    mov r16.b3, r3.b2

    mov r17.b0, r4.b0
    mov r17.b1, r4.b2
    mov r17.b2, r5.b0
    mov r17.b3, r5.b2

    mov r18.b0, r6.b0
    mov r18.b1, r6.b2
    mov r18.b2, r7.b0
    mov r18.b3, r7.b2

    mov r19.b0, r8.b0
    mov r19.b1, r8.b2
    mov r19.b2, r9.b0
    mov r19.b3, r9.b2

    sbbo &r16, r12, r15, 16

    add r14, r14, 32
    add r15, r15, 16
    sub r1, r1, 1
    qbne sample_loop, r1, 0
done:
