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
;   Brief:    Toggle PRU GPO output signal
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

    .asg	R2,	   TEMP_REG
    .asg    R3.w0, OFFSET_ADDR

    .asg    100000 ,DELAY_COUNT        ; Delay counter value


;********
;* MAIN *
;********

main:

init:

;----------------------------------------------------------------------------
;   Clear the register space
;   Before begining with the application, make sure all the registers are set
;   to 0. PRU has 32 - 4 byte registers: R0 to R31, with R30 and R31 being special
;   registers for output and input respectively.
;----------------------------------------------------------------------------

; Give the starting address and number of bytes to clear.
    zero	&r0, 120

main_loop:
    ; Generate a delay
    ldi32     r5, DELAY_COUNT
    ldi32     r6, DELAY_COUNT
delay_loop1:
    sub     r5, r5, 1
    qbne    delay_loop1, r5, 0

    ; Toggle GPIO pin 4 by writing 1 into bit 4 of R30
    ldi     r30.b0, 0x10
delay_loop2:
    sub     r6, r6, 1
    qbne    delay_loop2, r6, 0

    ; Toggle GPIO pin 4 by writing 0 into bit 4 of R30
    ldi     r30.b0, 0x00

    ; Continue loop
    qba     main_loop

    ; The GPIO toggle signal is generated at the pin 17 of J2 BoosterPack header on AM26x devices' launchpads. 

    ; halt the program
    halt ; end of program (never reached)
