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
; File: fft_macros.inc
;
; Description:
;       Include file that defines fft related constant macros
;       
;*****************************************************************

AM243                     .set 0x1
AM261                     .set 0x2
FFT_DEVICE                .set AM261
FFT_SIZE_NUM .set 4096        ;  4096 point FFT
FFT_SIZE         .set (FFT_SIZE_NUM*4)               ; memory size for 4 Bytes per input samples
OP_RAM_ADDR_BASE .set 0x14000
X_BASE_ADDRESS   .set (OP_RAM_ADDR_BASE - 4) ; base address shifted by 1 to deal with indexes/
                                             ;  that starts at 1 instead of 0

    .if (FFT_DEVICE == AM261)
WC_RAM_ADDR_BASE       .set 0x78000000       ; base address of window coefficients in memory
                                             ;   (IMP! Global address required)
    .else
WC_RAM_ADDR_BASE       .set 0x30018000       ; base address of window coefficients in memory
                                             ;   (IMP! Global address required)
    .endif
M                      .set 12               ; M= log2(FFT_SIZE_NUM), M = 12 for FFT_SIZE_NUM  = 4096
ONE_OVER_ROOT_TWO_Q32  .set 0xB504F333       ; (1/root(2))*2^(32)  (Q32 format)
FDB_BSRAM_CONFIG       .set 0x180    ; Config for FDB General Purpose mode

    .if (FFT_DEVICE == AM243)
TW_FACTOR_START_ADDRESS      .set 0x1A000   ; local address of initial location of Twiddle factors in Shared Memory
    .else
TW_FACTOR_START_ADDRESS      .set 0x0000
    .endif

TW_FACTOR_SPACE              .set 0x3F50    ; required space for 32 bit twiddle factors for 4k FFT
TW_FACTOR_END_ADDRESS        .set TW_FACTOR_START_ADDRESS + TW_FACTOR_SPACE ;
TW_FACTOR_BRAM_ADDR          .set 0x00      ; fdb broadside RAM Base Address for Storing Twiddle Factors
; config for Broadside RAM : bit 15 - auto-increment enable, bits 8-0 - RAM Address
TW_FACTOR_FDB_BRAM_CONFIG    .set ((1<<15) + TW_FACTOR_BRAM_ADDR) ; auto-increment : enable, base address : 0x000
; config for Broadside RAM - bit 15 : auto-increment enable(1),  bits 8-0 : RAM Address
IP_RAM_ADDR_BASE    .set 0x00000
IP_RAM_ADDR_1       .set IP_RAM_ADDR_BASE
IP_RAM_ADDR_2       .set IP_RAM_ADDR_BASE + (FFT_SIZE-32)
WF_LOOP_COUNT          .set (4096/(8*2))    ; Single loop handles 16 inputs
BRV_LUT_OFFSET      .set 0
    .if (FFT_DEVICE == AM243)
BRV_RAM_ADDR_BASE    .set 0x1E000        ; BRV LUT local address
    .else
BRV_RAM_ADDR_BASE     .set 0x78002000      ; BRV address
    .endif
