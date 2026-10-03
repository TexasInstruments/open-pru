; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2024-2026 Texas Instruments Incorporated - http://www.ti.com/

;******************************************************************************
;   File:   spinlock.asm
;
;   Brief:  C-callable acquire/release for the PRU-local hardware spinlock,
;           accessed over the broadside interface with XIN/XOUT.
;
;   Hardware semantics (PRU_ICSSG local spinlock accelerator):
;       - The spinlock accelerator owns 64 ownership flags (lock id 0..63).
;       - It is reached from the PRU over the broadside interface using the
;         XIN/XOUT instructions with broadside device id (XID) 0x90 (= 144),
;         which selects the PRU's *local* spinlock. A different XID is needed
;         to reach a spinlock in another PRU subsystem.
;       - The lock id is presented to the accelerator in R1.b0.
;       - ACQUIRE: an XIN returns the acquisition status in R1.b3 (bit 0):
;             * bit0 == 1 -> the lock is now OWNED BY YOU
;             * bit0 == 0 -> the lock is held by someone else; retry
;         The acquire is non-blocking - one XIN is one attempt. The caller
;         (here, C) spins until it gets a 1.
;       - RELEASE: an XOUT with the same lock id in R1.b0 frees the lock.
;
;   Note:
;       These helpers use XID 0x90 (local PRU subsystem). A separate function
;       would be needed for accessing spinlocks in a different PRU subsystem.
;******************************************************************************

; Required for building .out with an assembly file
    .retain
    .retainrefs

INT_SPIN_XID    .set    0x90    ; broadside XID for the local PRU spinlock

;******************************************************************************
; uint8_t spinlock_acquire(uint8_t flag_id);
;
;   One non-blocking attempt to acquire spinlock 'flag_id'.
;   Returns 1 if the lock was acquired, 0 if it is held by someone else.
;
;   Calling convention (PRU C/C++ compiler): the uint8_t argument arrives in
;   R14.b0 and the uint8_t return value is passed back in R14.b0. The return
;   address is in r3.w2.
;
;   Pseudocode:
;       R1.b0 = flag_id                 ; present lock id to the accelerator
;       XIN(XID 0x90) -> R1.b3          ; one acquire attempt, bit0 = acquired
;       return R1.b3                    ; 1 = acquired, 0 = held by another
;
;   Registers modified : R1 (R1.b0, R1.b3), R14.b0 (return value)
;   Peak Cycles : 4 (one acquire attempt; excludes any C-side retry spin)
;******************************************************************************
    .sect       ".text:spinlock_acquire"
    .clink
    .global     spinlock_acquire
spinlock_acquire:
    MOV         R1.b0, R14.b0           ; lock id -> R1.b0 (presented to accel)
    XIN         INT_SPIN_XID, &R1.b3, 1 ; status returned in R1.b3 (bit0=acquired)
    MOV         R14.b0, R1.b3           ; return status
    JMP         r3.w2

;******************************************************************************
; void spinlock_release(uint8_t flag_id);
;
;   Release spinlock 'flag_id'. Only the owner should release, and it must do
;   so promptly.
;
;   Calling convention (PRU C/C++ compiler): the uint8_t argument arrives in
;   R14.b0. The return address is in r3.w2.
;
;   Pseudocode:
;       R1.b0 = flag_id                 ; present lock id to the accelerator
;       XOUT(XID 0x90)                  ; free the lock
;
;   Registers modified : R1.b0
;   Peak Cycles : 3
;******************************************************************************
    .sect       ".text:spinlock_release"
    .clink
    .global     spinlock_release
spinlock_release:
    MOV         R1.b0, R14.b0           ; lock id -> R1.b0
    XOUT        INT_SPIN_XID, &R1.b3, 1 ; release the lock
    JMP         r3.w2
