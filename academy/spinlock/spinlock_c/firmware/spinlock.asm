; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

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
;   Parameters:
;       flag_id : lock id, valid range 0-63. Not range-checked here; the
;                 caller must pass a valid id.
;
;   Calling convention (PRU C/C++ compiler): the uint8_t argument arrives in
;   R14.b0 and the uint8_t return value is passed back in R14.b0. The return
;   address is in r3.w2.
;
;   Pseudocode:
;       R1.b0 = flag_id                 ; present lock id to the accelerator
;       XIN(XID 0x90) -> R1.b3          ; one acquire attempt, bit0 = acquired
;       return R1.b3 & 1                ; 1 = acquired, 0 = held by another
;
;   Registers modified : R1 (R1.b0, R1.b3), R14.b0 (return value)
;   Peak Cycles : 4, hand-counted at one cycle per instruction (mov, xin, and,
;                 jmp), for one acquire attempt; excludes any C-side retry
;                 spin. Assumes the broadside xin completes in one cycle; not
;                 measured separately on hardware.
;******************************************************************************
    .sect       ".text:spinlock_acquire"
    .clink
    .global     spinlock_acquire
spinlock_acquire:   .asmfunc
    mov     r1.b0, r14.b0               ; lock id -> R1.b0 (presented to accel)
    xin     INT_SPIN_XID, &r1.b3, 1     ; status returned in R1.b3 (bit0=acquired)
    and     r14.b0, r1.b3, 1            ; return bit 0 only: 1 = acquired
    jmp     r3.w2
    .endasmfunc

;******************************************************************************
; void spinlock_release(uint8_t flag_id);
;
;   Release spinlock 'flag_id'. Only the owner should release, and it must do
;   so promptly.
;
;   Parameters:
;       flag_id : lock id, valid range 0-63, the same id passed to
;                 spinlock_acquire(). Not range-checked here.
;
;   Calling convention (PRU C/C++ compiler): the uint8_t argument arrives in
;   R14.b0. The return address is in r3.w2.
;
;   Pseudocode:
;       R1.b0 = flag_id                 ; present lock id to the accelerator
;       XOUT(XID 0x90)                  ; free the lock
;
;   Registers modified : R1.b0
;   Peak Cycles : 3, hand-counted at one cycle per instruction (mov, xout,
;                 jmp). Assumes the broadside xout completes in one cycle; not
;                 measured separately on hardware.
;******************************************************************************
    .sect       ".text:spinlock_release"
    .clink
    .global     spinlock_release
spinlock_release:   .asmfunc
    mov     r1.b0, r14.b0               ; lock id -> R1.b0
    xout    INT_SPIN_XID, &r1.b3, 1     ; release the lock
    jmp     r3.w2
    .endasmfunc
