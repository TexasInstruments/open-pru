; File: ipc_xfr.inc

; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions
; are met:
;
;        * Redistributions of source code must retain the above copyright
;          notice, this list of conditions and the following disclaimer.
;
;        * Redistributions in binary form must reproduce the above copyright
;          notice, this list of conditions and the following disclaimer in the
;          documentation and/or other materials provided with the
;          distribution.
;
;        * Neither the name of Texas Instruments Incorporated nor the names of
;          its contributors may be used to endorse or promote products derived
;          from this software without specific prior written permission.
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
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Dual-PRU block hand-off protocol: shared macros.
; ipc_xfr.inc is included by the PRU0 (DUAL_PRU) and PRU1 sources, so there
; is exactly one copy of the slot protocol.
;
; Scratchpad layout. Without XFR shift, register Rn always lands in bank word n,
; so payload and protocol words are staged in R2..R25; a caller must keep its
; own state in R0, R1, R26..R29 (or save R2..R25 around the macro).
;   slot A: bank 10 words 2..25 (block bytes 0..95) + bank 12 words 2..9  (96..127)
;   slot B: bank 11 words 2..25 (block bytes 0..95) + bank 12 words 10..17 (96..127)
;   bank 12 word 18 A_seq, 19 B_seq  (producer writes, after payload + meta)
;   bank 12 word 20 A_ack, 21 B_ack  (consumer writes, after it has copied out)
;   bank 12 word 22 A_meta, 23 B_meta (producer: component index of the block)
; Sequence numbers start at 1 and alternate slots (odd -> A, even -> B); all
; banks start zeroed, so both slots start free. The slot for sequence s is free
; once its ack >= s-2 (the sequence the producer last wrote there; acks only
; ever hold a value the producer wrote, so >= is the same test as ==, and it
; needs no special case for the first two blocks).
;
; IPC_TX  s_seq, s_src, s_meta   (producer; all three registers outside R2..R25)
;   s_seq  sequence number to publish (1, 2, 3, ...)
;   s_src  register holding the address of the 128 B block to send
;   s_meta register holding the 32-bit metadata word (component index)
;   Clobbers R2..R23.
; IPC_RX  s_seq, s_dst, s_meta   (consumer; registers outside R2..R25)
;   s_seq  sequence number to wait for
;   s_dst  register holding the address the 128 B block is copied to
;   s_meta register receiving the metadata word
;   Clobbers R2..R23. The slot is acked BEFORE the macro returns, that is, before
;   the consumer computes, so the producer can refill it meanwhile.

; Pseudo code: wait for slot, copy payload and metadata, publish sequence.
; PEAK / local service bound: 128 cycles excluding peer wait; poll takes
; <= 4 instruction cycles per iteration. WCET including peer wait is
; intentionally unbounded in firmware; host watchdog recovery is documented
; at each polling label. Basis: static instruction and local transfer counts.
; Modified registers and parameters are documented above.
IPC_TX .macro s_seq, s_src, s_meta
    qbbs    txa?, s_seq, 0          ; odd seq -> slot A
; INTENTIONALLY_UNBOUNDED_LOOP: peer wait; the host JPEG_TIMEOUT_US watchdog
; disables both cores on timeout or producer decode failure.
txb?:
    xin     12, &r21, 4             ; B_ack
    add     r21, r21, 2
    qbgt    txb?, r21, s_seq        ; slot still owned by the consumer: poll
    lbbo    &r2, s_src, 0, 96
    xout    11, &r2, 96
    lbbo    &r10, s_src, 96, 32
    xout    12, &r10, 32
    mov     r23, s_meta
    xout    12, &r23, 4             ; B_meta
    mov     r19, s_seq
    xout    12, &r19, 4             ; publish: payload and meta first, seq last
    qba     txd?
; INTENTIONALLY_UNBOUNDED_LOOP: peer wait; the host JPEG_TIMEOUT_US watchdog
; disables both cores on timeout or producer decode failure.
txa?:
    xin     12, &r20, 4             ; A_ack
    add     r20, r20, 2
    qbgt    txa?, r20, s_seq
    lbbo    &r2, s_src, 0, 96
    xout    10, &r2, 96
    lbbo    &r2, s_src, 96, 32
    xout    12, &r2, 32
    mov     r22, s_meta
    xout    12, &r22, 4             ; A_meta
    mov     r18, s_seq
    xout    12, &r18, 4
txd?:
    .endm

; Pseudo code: wait for sequence, copy payload and metadata, acknowledge.
; PEAK / local service bound: 128 cycles excluding peer wait; poll takes
; <= 4 instruction cycles per iteration. WCET including peer wait is
; intentionally unbounded in firmware; host watchdog recovery is documented
; at each polling label. Basis: static instruction and local transfer counts.
; Modified registers and parameters are documented above.
IPC_RX .macro s_seq, s_dst, s_meta
    qbbs    rxa?, s_seq, 0
; INTENTIONALLY_UNBOUNDED_LOOP: peer wait; the host JPEG_TIMEOUT_US watchdog
; disables both cores on timeout or producer decode failure.
rxb?:
    xin     12, &r19, 4             ; B_seq
    qbne    rxb?, r19, s_seq        ; not published yet: poll
    xin     11, &r2, 96
    sbbo    &r2, s_dst, 0, 96
    xin     12, &r10, 32
    sbbo    &r10, s_dst, 96, 32
    xin     12, &r23, 4             ; B_meta (published before B_seq)
    mov     r21, s_seq
    xout    12, &r21, 4             ; B_ack: slot B free again
    mov     s_meta, r23
    qba     rxd?
; INTENTIONALLY_UNBOUNDED_LOOP: peer wait; the host JPEG_TIMEOUT_US watchdog
; disables both cores on timeout or producer decode failure.
rxa?:
    xin     12, &r18, 4             ; A_seq
    qbne    rxa?, r18, s_seq
    xin     10, &r2, 96
    sbbo    &r2, s_dst, 0, 96
    xin     12, &r2, 32
    sbbo    &r2, s_dst, 96, 32
    xin     12, &r22, 4             ; A_meta
    mov     r20, s_seq
    xout    12, &r20, 4             ; A_ack
    mov     s_meta, r22
rxd?:
    .endm
