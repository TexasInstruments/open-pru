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
; File: butterfly_length_2.asm
;
; Description:
;       Implements length-2 butterflies for FFT
;
;*****************************************************************

    .sect    ".text:butterfly_length_2"
    .clink
    .global   ||FN_LENGTH_2_BUTTERFLY||

; Macros
OP_RAM_ADDR_BASE .set 0x14000
X_BASE_ADDRESS   .set (OP_RAM_ADDR_BASE - 4) ; base address shifted by 1 to deal with indexes/
                                             ;  that starts at 1 instead of 0
FFT_SIZE         .set (4096*4)               ; 4096 point FFT. 4 Bytes per input samples

;register assignments
    .asg r2     ,     I0
    .asg r3.w0  ,     N
    .asg r4     ,     x_base_addr
    .asg r5     ,     x_i0
    .asg r6     ,     x_i1  
    .asg r7     ,     temp_1
    .asg r8     ,     ID

.asmfunc
||FN_LENGTH_2_BUTTERFLY||:
    ;*******************************************
    ;   Function : FN_FFT_4K_WINDOWED
    ;
    ;   Description:
    ;       Performs 4096 point Split-radix DIT-FFT/
    ;       with windowing in the beginning
    ;
    ;*******************************************
    ldi32 x_base_addr, X_BASE_ADDRESS        ; base address shifted by 1 to deal with
                                             ;      addressing that starts at 1 instead of 0
    ldi N, FFT_SIZE
	ldi I0, (1*4)                            ; initial value
    ldi ID, (4*4)                            ; increment for length-2

DO_WHILE_LOOP:
*****************Do-While Loop**************************
FOR_LOOP:
*****************For Loop*********************
    qbgt END_FOR_LOOP, N, I0                  ; loop till I0>N
    lbbo &x_i0, x_base_addr, I0, 8            ; load ips to consecutive registers x_i0 and x_i1
    ;perform butterfly with result in consecutive registers x_i1 and temp1 and store
    sub temp_1, x_i0, x_i1
    add x_i1, x_i0, x_i1
    sbbo &x_i1, x_base_addr, I0, 8

    add I0, I0, ID
    jmp FOR_LOOP
******************For Loop ends****************
END_FOR_LOOP:
    ; IS = 2 * ID - 1 ;I0 = IS
    lsl I0, ID, 1
    sub I0, I0, 4
    lsl ID, ID, 2                             ; ID = 4 * ID
    qblt DO_WHILE_LOOP, N, I0                 ; repeat while (IS<N)

******************** Do-While Loop ends*****************
    ;exit task
    jmp r3.w2
.endasmfunc

;****************************************************************************
;*                                   END                                    *
;****************************************************************************
