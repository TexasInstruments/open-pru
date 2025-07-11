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
;   Brief:    System Event generation using INTC example assembly file (asm) with halt instruction
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

    .asg	R10,	     TEMP_REG_1

;************************************* includes *************************************
; icss_constant_defines.inc: Defines symbols corresponding to Constant Table Entries
    .include "icss_constant_defines.inc"
    .include "icss_program_flow_macros.inc"

    .asg    0x20 ,       SYSTEM_EVENT_16    ; System event number to generate
    .asg    100000000 ,  DELAY_COUNT        ; Delay counter value
    .asg    R11 ,        KEY_REG            ; Key register
    .asg    0x12345678 , KEY_VAL            ; Key value

    ; Used to receive command values and interrupt from R5F core
    .asg    60,          PRGM_FLOW_EVENT
    .asg    31,          PRGM_FLOW_EVENT_BIT

;********
;* MAIN *
;********

main:
    ; Clear all registers
    zero    &r0, 120

    ; Configure the Constant Table entry C28 to point to start of shared memory
    ; PRU_ICSSM Shared RAM (local-C28) : 00nn_nn00h, nnnn = c28_pointer[15:0]
    ldi     TEMP_REG_1, 0x0100
    sbco    &TEMP_REG_1, ICSS_PRU_CTRL_CONST, 0x28, 2

main_loop:
    ; Generate a delay
    ldi32     r0, DELAY_COUNT
delay_loop:
    sub     r0, r0, 1
    qbne    delay_loop, r0, 0

    m_prgm_flow_jump_on_intr    PRGM_FLOW_EVENT, PRGM_FLOW_EVENT_BIT, 1, TEMP_REG_1, intr_receive_from_r5f
intr_send_to_r5f:
    ; Generate system event by writing 100000 to R31[5:0]
    ldi     r31.b0, SYSTEM_EVENT_16

    ; Continue loop
    qba     main_loop

intr_receive_from_r5f:
    ; Handle interrupt from R5F
    ldi32   KEY_REG, KEY_VAL 

    ; Interrupt received from R5F, key register R11 will now contain the key value

    ; Never reached
    halt
