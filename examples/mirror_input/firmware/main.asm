; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2024-2025 Texas Instruments Incorporated - http://www.ti.com/

;***************************************************************************************
;   File:     main.asm
;
;   Brief:    Empty example assembly file (asm) with halt instruction
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

;********
;* MAIN *
;********

main:
    ; check GPI01 and set/clear GPO4 based on it 
    ; to change the input and output, we need to change the bit number used here 
    ; example : to give input to GPI2 , in line use qbbs bit_set,r31,2 
    ; example : to take output from GPI2, in line 45,48 use set/clr r30,r30,2
read_loop:
    qbbs bit_set,r31,0
bit_clear:
    clr  r30,r30,1
    qba  read_loop
bit_set:
    set  r30,r30,1
    qba  read_loop
