; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2024-2026 Texas Instruments Incorporated - http://www.ti.com/

;******************************************************************************
;   File:   spinlock_example.asm
;
;   Brief:  Pure-assembly example of acquiring and releasing the PRU-local
;           hardware spinlock over the broadside interface (XIN/XOUT).
;
;   Hardware semantics (PRU_ICSSG local spinlock accelerator):
;       - Reached over the broadside interface with XIN/XOUT using broadside
;         device id (XID) 0x90 (= 144), which selects the *local* spinlock.
;       - The lock id (0..63) is presented in R1.b0.
;       - ACQUIRE: XIN returns acquisition status in R1.b3; bit 0 set means the
;         lock is now owned by this core. Busy-wait until bit 0 is set.
;       - RELEASE: XOUT with the same lock id in R1.b0 frees the lock.
;
;   Steps to build:
;       clpru --silicon_version=4 spinlock_example.asm
;******************************************************************************

; Required for building .out with an assembly file
    .retain
    .retainrefs

INT_SPIN_XID    .set    144     ; XID 0x90 - local PRU spinlock accelerator
SPINLOCK_ID     .set    0       ; lock id 0-63, fixed in PRU firmware

;******************************************************************************
;   Macro: M_SPINLOCK_ACQUIRE
;       Acquire the spinlock and busy-wait until successful.
;
;   Pseudocode:
;       R1.b0 = SPINLOCK_ID
;       do { XIN(XID 0x90) -> R1.b3 } while (R1.b3 bit 0 == 0)
;
;   Registers modified : R1.b0 (lock id), R1.b3 (status, bit 0 = 1 on exit)
;   Peak Cycles : 3 if the lock is free; unbounded while another core holds it
;                 (2 cycles per retry)
;******************************************************************************
M_SPINLOCK_ACQUIRE  .macro
    .newblock
    LDI     R1.b0, SPINLOCK_ID          ; lock id (0-63)
$1:
    XIN     INT_SPIN_XID, &R1.b3, 1     ; attempt acquire; status in R1.b3
    QBBC    $1, R1.b3, 0                ; bit 0 clear -> not acquired, retry
    .endm

;******************************************************************************
;   Macro: M_SPINLOCK_RELEASE
;       Release the spinlock. Must follow M_SPINLOCK_ACQUIRE (lock id is still
;       in R1.b0).
;
;   Pseudocode:
;       XOUT(XID 0x90) with lock id R1.b0   ; frees the lock
;
;   Registers modified : none
;   Peak Cycles : 1 (one XOUT)
;******************************************************************************
M_SPINLOCK_RELEASE  .macro
    XOUT    INT_SPIN_XID, &R1.b3, 1     ; release lock held in R1.b0
    .endm

;********
;* MAIN *
;********
    .global main
    .sect   ".text"
main:
    ; Clear the register space before starting.
    zero    &r0, 120

    M_SPINLOCK_ACQUIRE

    ;--------------------------------------------------------------------------
    ; Critical section: we now own SPINLOCK_ID. Access the shared resource here.
    ;--------------------------------------------------------------------------

    M_SPINLOCK_RELEASE

    halt
