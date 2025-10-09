; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2018-2025 Texas Instruments Incorporated - http://www.ti.com/

;******************************************************************************
;   File:     main.asm
;
;   Brief:    pru_add: Add 2 numbers in an infinite loop
;******************************************************************************

;******************************************************************************
; Build Configuration
;******************************************************************************

; Required for building .out with assembly file
    .retain
    .retainrefs

;*****************************************************************************
; Definitions
; Brief: Define x & y values. Define storage registers
;*****************************************************************************

; TODO: define y
x	        .set	1

; TODO: define y_register and z_register
x_register	.set	r20

; .sect ".text:main" places all code below the .sect directive into the .text
; section, grouped into a subsection named "main"
    .sect       ".text:main"
    .clink
    .global     main

;*****************************************************************************
; Main
;*****************************************************************************
main:

;*****************************************************************************
; Initialization Section
; Brief: Clear registers and configure system
;*****************************************************************************
init:

;----------------------------------------------------------------------------
;   Clear the register space
;   Before begining with the application, make sure registers R0 - R29 are set
;   to 0. R30 & R31 are special, dedicated for output & input respectively
;----------------------------------------------------------------------------
    ; TODO: use zero to clear 30 four-byte registers, starting with R0
    ; Give the starting address and number of bytes to clear.

while_true:

    ; TODO: load y value into y_register
    ldi		x_register, x	; load x value into register r20

    ; TODO: add x_register and y_register. Store the result in z_register

    ; jump to continue refreshing z_register value
    jmp		while_true

    ; the jump prevents us from getting to halt
    halt			; end of program
