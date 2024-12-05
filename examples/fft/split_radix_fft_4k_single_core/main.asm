
; Copyright (C) 2024-2025 Texas Instruments Incorporated - http://www.ti.com/
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


******************************************************************
; File: main.asm
;
;*****************************************************************

  .retain     ; Required for building .out with assembly file
  .retainrefs ; Required for building .out with assembly
  .global     main
  .sect    ".text"

  ; Declaration of the external assembly function
  .global   ||FN_FFT_4k_WINDOWED||
  .global   ||FN_LOAD_LUT_VALUES||

main:
    ; configure FDB-RAM as General Purpose
    ldi32 r11, 0x00033064   ; load MII_G_RT_FDB_GEN_CFG2 MMR address
    ldi32 r12, 0x180        ; set configuration
    sbbo &r12, r11, 0, 4    ; write configuration to MMR
    ;*
    ;* configure shared memory c28 pointer.
    ;* (nnnn = 0100) => c28 = 0x00010000
    ;*
    ldi     r2, 0x0100
    sbco    &r2, c11, 0x28, 2
    ;MUL config
    zero &r25.b0, 1     ; clear r25[0] for MUL only mode
    xout 0, &r25.b0, 1  ; load MUL only mode for MAC

    ;load LUT values from shared-ram and save to bs-ram
    jal r3.w2, FN_LOAD_LUT_VALUES

    ; call window function + FFT
    jal r3.w2, FN_FFT_4k_WINDOWED

    halt    ;end program


