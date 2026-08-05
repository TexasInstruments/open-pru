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

EPWM_SIGNAL    .set    2   ;EPWM signal comes to GPI2
CMP_SIGNAL     .set    0   ;CMP signal comes to GPI0
ICL_SIGNAL     .set    1   ;ICL signal comes to GPI1     

main:
    ; check GPI00 and set/clear GPO1 based on it 
    ; EPWM signal is coming on GPI2
    zero &r0,124
high_pulse:
    ; when EPWM becomes high, copy input -> output 
    wbs r31, EPWM_SIGNAL
    ; move CMP input to ICL output 
    qbbs bit_set0, r31,CMP_SIGNAL
bit_clear0:
    clr  r30, r30, ICL_SIGNAL
    qba  low_pulse
bit_set0:
    set  r30, r30, ICL_SIGNAL
    
low_pulse:
    ; when EPWM becomes low, again copy input -> output
    wbc r31, EPWM_SIGNAL
    ; move CMP input to ICL output 
    qbbs bit_set1, r31,CMP_SIGNAL
bit_clear1:
    clr  r30, r30, ICL_SIGNAL
    qba  high_pulse
bit_set1:
    set  r30, r30, ICL_SIGNAL
    qba high_pulse