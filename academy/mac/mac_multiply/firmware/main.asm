; Copyright (C) 2024-2025 Texas Instruments Incorporated - http://www.ti.com/
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions
; are met:
;
; Redistributions of source code must retain the above copyright
; notice, this list of conditions and the following disclaimer.
;
; Redistributions in binary form must reproduce the above copyright
; notice, this list of conditions and the following disclaimer in the
; documentation and/or other materials provided with the
; distribution.
;
; Neither the name of Texas Instruments Incorporated nor the names of
; its contributors may be used to endorse or promote products derived
; from this software without specific prior written permission.
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

;***************************************************************************************
;   File:     main.asm
;
;   Brief:    MAC (multiply only) example assembly file (asm) with halt instruction
;
;   Steps to build :
;
;   - Using ccs:
;             - Import pru project to ccs workspace
;             - main.asm file gets copied to ccs workspace
;             - Modify main.asm file
;             - Build the pru project, after which .out (Executable output file) and .h (Firmware header) files gets generated
;             - Either .out (Executable output file) can be loaded to PRU using ccs or R5F can write to PRU IRAM using PRUICSS driver
;   - Using makefile:
;             - Use command gmake -all to build PRU project     
;
;***************************************************************************************

; CCS/makefile specific settings
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

    .global     main
    .sect       ".text"

;************************************* includes *************************************
; icss_constant_defines.inc: Defines symbols corresponding to Constant Table Entries
    .include "icss_constant_defines.inc"

    .asg    0x0 ,   DEVICE_ID       ; DEVICE_ID
    .asg    0x0,    MULTIPLY_ONLY   ; MULTIPLY-ONLY mode for MAC
;********
;* MAIN *
;********

main:

init:
    ; Clear the register space. PRU has 32 4-Byte registers.
    ; Set R0 - R29 to 0. R30 & R31 are special module interface registers.
    ; Give the starting address and number of bytes to clear.
    zero	&r0, 120

    ; Load operands into registers R10-R11
    LDI     R10, 50      ; Operand 1: a
    LDI     R11, 25      ; Operand 2: b


    ; Set up MAC accumulator registers R25-R29
    ; R25 = MAC control/status
    ; R26,R27 = Result registers (64-bit product)
    ; R28,R29 = Operand registers

    ; Step 1: Enable multiply only MAC_MODE
    LDI     R25, MULTIPLY_ONLY  ; Set R25[0] = 1 for MAC mode
    XOUT    DEVICE_ID, &R25, 1  ; Enable MAC mode

    ; Multiply the operands
    MOV     R28, R10    ; Load a into first operand register
    MOV     R29, R11    ; Load b into second operand register
    NOP     ; It takes 1 PRU clock to multiply a * b

    ; Read final result
    XIN     DEVICE_ID, &R25, 1     ; Read MAC status
    XIN     DEVICE_ID, &R26, 4     ; Read lower 32 bits of result into R26
    XIN     DEVICE_ID, &R27, 4     ; Read upper 32 bits of result into R27

    ; Result is now in R26 (lower 32 bits) and R27 (upper 32 bits)

; halt the program
    halt ; end of program
