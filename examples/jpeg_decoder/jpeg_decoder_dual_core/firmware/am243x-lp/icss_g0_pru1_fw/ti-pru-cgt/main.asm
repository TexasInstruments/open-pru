; File: main.asm

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
; PRU1 block loop for the dual-core JPEG decoder: receive -> dequantize -> IDCT -> output.
;
; PRU0 runs the Huffman scan (the PRU0 project's main.asm, built with
; DUAL_PRU=1) and hands each finished 128 B coefficient block over the
; scratchpad banks (ipc_xfr.inc). For each block PRU1 then runs, in order:
;   dequantize (dequant_mac_body.inc)
;   IDCT       (idct_body.inc)
;   output     (output_stage_body.inc)
; The three bodies are shared with the single-core project through the
; common include directory; only their buffer addresses differ. The slot is
; acked right after the copy-out, before the dequantize starts, so PRU0
; refills it while PRU1 computes.
;
; PRU1 local DRAM map (host fills it before reset; PRU1 DRAM is global 0x2000):
;   0x0000  blocks remaining (u32)   -- host sets to the block count
;   0x0004  last seq consumed  (u32)   -- host sets to 0
;   0x0008  output pointer     (u32)   -- host sets to the output buffer; the
;                                         64 sample bytes land there, then +64
;   0x0100  qtables, 128 B each (uint16 x 64, natural order), index = the
;           component index PRU0 sends with each block
;   0x0300  received coefficient block (128 B)
;   0x0400  dequantizer output / IDCT input, int32 x 64 (256 B)
;   0x0500  IDCT workspace (256 B)
;   0x0600  IDCT output / output-stage input, packed int16 x 64 (128 B)
; Loop state lives in DRAM because the IDCT clobbers nearly every register.

STATE    .set 0x0000
QTAB     .set 0x0100
COEF     .set 0x0300
DEQ      .set 0x0400
WS       .set 0x0500
IDCT_OUT .set 0x0600
IDCT_IN  .set DEQ
IDCT_WS  .set WS

    .include "ipc_xfr.inc"
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly
    .text
    .global main
main:
    ; Ready follows scratchpad initialization in the start stub. The host
    ; waits for this word before starting the producer (no timing assumption).
    ldi     r0, 0
    ldi     r1, 1
    sbbo    &r1, r0, 12, 4
blk_top:
    ldi     r0, 0
    lbbo    &r26, r0, STATE, 12     ; r26 remaining, r27 last seq, r28 out ptr
    qbeq    all_done, r26, 0
    add     r27, r27, 1             ; seq we are waiting for
    ldi     r29, COEF
    IPC_RX  r27, r29, r1            ; r1 = component index of this block
    sub     r26, r26, 1
    sbbo    &r26, r0, STATE, 12     ; r0 is still 0 (the macro clobbers R2..R23)
    ; ---- dequantize: r10 qtable, r11 coefficients in, r12 dequantised out ----
    lsl     r10, r1, 7
    ldi     r12, QTAB
    add     r10, r10, r12
    ldi     r11, COEF
    ldi     r12, DEQ
    .include "dequant_mac_body.inc"
    ; ---- IDCT: IDCT_IN -> IDCT_OUT ----
    .include "idct_body.inc"
    ; ---- output stage: r11 samples in, r12 output pointer ----
    ldi     r0, 0
    lbbo    &r12, r0, STATE + 8, 4
    add     r13, r12, 64
    sbbo    &r13, r0, STATE + 8, 4
    ldi     r11, IDCT_OUT
    .include "output_stage_body.inc"
    qba     blk_top
all_done:
    halt
