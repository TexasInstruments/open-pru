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

;*********************************************************************
; File: lut_load.asm
;
; Description:
;       This file implements the function that loads necessary LUTs
;
;*********************************************************************

    .sect       ".text:lut_load"
    .clink
    .global     ||FN_LOAD_LUT_VALUES||

; File includes
	.include  "pru_io\firmware\common\icss_xfer_defines.inc"

; Macros
TW_FACTOR_START_ADDRESS      .set 0x1A000   ; local address of initial location of Twiddle factors in Shared Memory
TW_FACTOR_SPACE              .set 0x3F50    ; required space for 32 bit twiddle factors for 4k FFT
TW_FACTOR_END_ADDRESS        .set TW_FACTOR_START_ADDRESS + TW_FACTOR_SPACE ;
TW_FACTOR_BRAM_ADDR          .set 0x00      ; fdb broadside RAM Base Address for Storing Twiddle Factors
; config for Broadside RAM : bit 15 - auto-increment enable, bits 8-0 - RAM Address
TW_FACTOR_FDB_BRAM_CONFIG    .set ((1<<15) + TW_FACTOR_BRAM_ADDR) ; auto-increment : enable, base address : 0x000
XID_FDB_BSRAM                .set 0x1E

.asmfunc
||FN_LOAD_LUT_VALUES||:
    ;*************************************************
    ;   Function : FN_LOAD_LUT_VALUES
    ;
    ;   Description:
    ;       Loads Twiddle factors to FDB BS-RAM.
    ;
    ;*************************************************
    xout PRU_SPAD_B0_XID, &r3.w2, 2       ; save return PC to SPAD
    ldi32 r12, TW_FACTOR_START_ADDRESS    ; shared memory tw factor address start
    ldi32 r13, TW_FACTOR_END_ADDRESS      ; shared memory LUT range end

    ldi  r10, TW_FACTOR_FDB_BRAM_CONFIG   ; load configuration
    xout  XID_FDB_BSRAM, &r10, 2          ; configure fdb-bs_ram to auto-increment and load base address as 0x000
TW_LUT_LOAD:
    qbge LUT_LOAD_END, r13, r12
    lbbo &r2, r12, 0, 32                  ; load 8 values, 4 bytes each, to r2-r9
    xout  XID_FDB_BSRAM, &r2, 32          ; save to fdb-bs-ram
    add r12, r12, 32                      ; increment memory address
    jmp TW_LUT_LOAD
LUT_LOAD_END:
    xin PRU_SPAD_B0_XID, &r3.w2, 2        ; restore PC

    jmp        r3.w2                      ; return from function
.endasmfunc

;*****************************************************************************
;*                                     END
;*****************************************************************************
