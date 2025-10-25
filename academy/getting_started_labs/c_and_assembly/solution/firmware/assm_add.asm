; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2018-2025 Texas Instruments Incorporated - http://www.ti.com/

;******************************************************************************
; Build Configuration
;******************************************************************************

; Required for building .out with assembly file
    .retain
    .retainrefs

; .sect ".text:assm_add" places all code below the .sect directive into the
; .text section, grouped into a subsection named "assm_add"
    .sect	".text:assm_add"
    .clink
    .global	assm_add

;*****************************************************************************
; uint32_t assm_add(uint32_t arg1, uint32_t arg2);
;*****************************************************************************
assm_add:

;-----------------------------------------------------------------------------
;   Function input arguments are stored in R14-R29.
;   So the 32 bit value in arg1 is stored in R14, and the 32 bit value in
;   arg2 is stored in R15. The return value is stored in R14.
;
;   For more details about how function arguments are stored in registers,
;   reference the document "PRU Optimizing C/C+ Compiler User's Guide",
;   section "Function Structure and Calling Conventions"
;-----------------------------------------------------------------------------

    ; INSERT CODE: add arg1 and arg2. Store the sum in the return register
    ADD		R14, R14, R15

    ; return from function assm_add
    JMP		r3.w2
