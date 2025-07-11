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
;   Brief:    CRC example assembly file (asm) with halt instruction
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

    .asg    0x1,           CRC_ID          ; CRC_ID
    .asg    0x00000000,    CRC16_MODE      ; CRC 16 mode (write 4 bytes at a time)
    .asg    0x00000001,    CRC32_MODE      ; CRC 32 mode (write 4 bytes at a time)
    .asg    R10,           TEMP_CRC16      ; Temporary register for CRC16
    .asg    R11,           TEMP_CRC32      ; Temporary register for CRC32
    .asg    0x3412,        DATA16          ; 0x1234 written in little-endian format
;********
;* MAIN *
;********

main:
    ; Clear the register space. PRU has 32 4-Byte registers.
    ; Set R0 - R29 to 0. R30 & R31 are special module interface registers.
    ; Give the starting address and number of bytes to clear.
    zero	&r0, 120

crc16:

    ; Set up CRC registers for CRC16 mode
    LDI     R25, CRC16_MODE         ; Set R25[0] = 0 for CRC16 mode
    XOUT    CRC_ID, &R25, 4         ; Enable CRC mode

    LDI		R29.w0, DATA16   	    ; Move the data to R29 register
	XOUT 	CRC_ID, &R29.w0, 2		; Moving 2 bytes
    NOP
	XIN 	CRC_ID, &R28, 4         ; Reading  CRC-16 value (stored in R28[31:16])
	XIN 	CRC_ID, &R29, 4         ; Reading bit-flipped CRC-16 value (stored in R29[15:0])

    ; Store the result
    MOV     TEMP_CRC16.b0, R28.b2   ; Store lower 8-bits
    MOV     TEMP_CRC16.b1, R28.b3   ; Store upper 8-bits

crc32:

    ; Set up CRC registers for CRC32 mode
    LDI     R25, CRC32_MODE         ; Set R25[0] = 1 for CRC32 mode
    XOUT    CRC_ID, &R25, 4         ; Enable CRC mode

    LDI		R29.w0, DATA16   	    ; Move the data to R29 register
	XOUT 	CRC_ID, &R29.w0, 2		; Moving 2 bytes
    NOP
    NOP
	XIN 	CRC_ID, &R28, 4         ; Reading  CRC-32 value (stored in R28)
	XIN 	CRC_ID, &R29, 4         ; Reading bit-flipped CRC-32 value (stored in R29)

    ; Store the result
    MOV     TEMP_CRC32, R28         ; Store CRC-32 Result into TEMP_CRC32

    ; Result is now in R10 (for CRC-16) and R11 (for CRC-32)



; halt the program
    halt ; end of program
