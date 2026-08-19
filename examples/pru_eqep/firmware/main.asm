; Copyright (C) 2025 Texas Instruments Incorporated - http://www.ti.com/
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
;
;     file:   main.asm
;
;     brief:  incremental encoder interface with max 4 MHz

    .include "memory.inc"
    .include "macros.inc"

    ; Required directives for CCS/makefile
    .retain
    .retainrefs
    .global     main
    .sect    ".text"

main:
    ; Initialize registers
    zero    &r0, 116                    ; Clear registers
    ldi32   DMEM0, DMEM0_BASE
    ldi     buffer_size, MAX_BUFFER     ; Set buffer size to 8 kB
    ldi     A_B_Z_edge,A_B_Z_GPI_mask
    ldi     read_pos_speed_intr, READ_POS_SPEED_BUFF
    ldi     a_b_transition, 0
    mov     curr_gpi_sample, cur_sample
    and     curr_gpi_sample, curr_gpi_sample, A_B_Z_edge
    mov     prev_sample, curr_gpi_sample     ; Initialize previous sample

    ; Initialize A/B transition states
    qbbc    Curr_A_not_set0, curr_gpi_sample, A_SIGNAL_GPI
    SET     a_b_transition, a_b_transition, 1
Curr_A_not_set0:
    qbbc    Curr_B_not_set0, curr_gpi_sample, B_SIGNAL_GPI
    SET     a_b_transition, a_b_transition, 0
Curr_B_not_set0:

    ; Publish QPOSMAX to R5F once at boot, so the host can read the actual
    ; counter modulus instead of relying on a hand-duplicated constant.
    ldi32   scratch2, QPOSMAX
    sbco    &scratch2, DMEM1, QPOSMAX_OFFSET, 4

    ; Cache QPOSMAX in a dedicated register once at boot, so the hot-path
    ; overflow compares below don't pay for an ldi32 reload on every edge.
    ldi32   qposmax_reg, QPOSMAX

    ; Start high-speed capture routine
    M_RESET_CYCLCNT

capture:
capture_wrap:
    ; If PRU counter has saturated, reenable the counter
    lbco	&scratch, PRU_CTRL_CONST, 0, 4
    qbbs    skip_counter_reenable, scratch, 3
    M_RESET_CYCLCNT
	set 	scratch, scratch, 3
	sbco	&scratch, PRU_CTRL_CONST, 0, 4
skip_counter_reenable:

    ; Wait for signal change
    ; Load current state of A, B, Z signals into a register
    mov     curr_gpi_sample, cur_sample               ; Get current GPI states
    and     curr_gpi_sample, curr_gpi_sample, A_B_Z_edge ; Mask for GPI3(A), GPI4(B), GPI6(Z) for PRU0

    ; Compare with previous state
    qbeq    capture, curr_gpi_sample, prev_sample ; If no change in A,B,Z, keep polling

    ; Capture timestamp
    M_READ_CYCLCNT curr_ts
    M_RESET_CYCLCNT
    add     time_stamp, time_stamp, curr_ts
    mov     edge, curr_gpi_sample

    ; Update A/B transition register
    LSL     a_b_transition, a_b_transition, 2
    qbbc    Curr_A_not_set, curr_gpi_sample, A_SIGNAL_GPI
    SET     a_b_transition, a_b_transition, 1
Curr_A_not_set:
    qbbc    Curr_B_not_set, curr_gpi_sample, B_SIGNAL_GPI
    SET     a_b_transition, a_b_transition, 0
Curr_B_not_set:

    ; Save current state
    mov     prev_sample, curr_gpi_sample

    ; Handle buffer wrap-around before writing, so buffer_addr stays in bounds
    qbgt    write_in_bounds, buffer_addr, buffer_size
    ldi     buffer_addr, 0
    ldi     READ_POS_BUFF, 0

write_in_bounds:
    sbbo    &time_stamp, DMEM0, buffer_addr, 4
    sbco    &buffer_addr, DMEM1, WRITE_PTR_OFFSET, 2
    qbgt    no_buffer_wrap, buffer_addr, buffer_size

    ; Handle Z signal reset
    qbbs    Z_interrupt0, r31, Z_SIGNAL_GPI
    ;zero    &QPOS, 4
Z_interrupt0:

    ; Position counter update logic
    and     a_b_transition, a_b_transition, 0xf
    add     a_b_transition, a_b_transition, 0xf0
    lbco    &qpos_update, DMEM1, a_b_transition, 1

    ; Handle phase error (simultaneous A/B transition)
    qbne    no_phase_err0, qpos_update, 3
    lbco    &scratch2, DMEM1, PHASE_ERR_CNT_OFFSET, 4
    add     scratch2, scratch2, 1
    sbco    &scratch2, DMEM1, PHASE_ERR_CNT_OFFSET, 4
    sbco    &QPOS, DMEM1, CH_POS_OFFSET, 4
    qba     capture_wrap
no_phase_err0:
    ; Handle increment/decrement based on direction
    qbbc    no_increment0, qpos_update, 0
    add     QPOS, QPOS, 1
    qbge    no_qpos_overflow0, QPOS, qposmax_reg   ; jump if qposmax_reg >= QPOS (i.e. QPOS <= QPOSMAX)
    ldi     QPOS, 0
no_qpos_overflow0:
    sbco    &QPOS, DMEM1, CH_POS_OFFSET, 4
    qba     capture_wrap
no_increment0:
    qbbc    no_decrement0, qpos_update, 1
    sub     QPOS, QPOS, 1
    ldi32   scratch2, 0xFFFFFFFF
    qbne    no_qpos_underflow0, QPOS, scratch2  ; only reload if result is exactly 0xFFFFFFFF (true underflow)
    ldi32   QPOS, QPOSMAX
no_qpos_underflow0:
no_decrement0:
    sbco    &QPOS, DMEM1, CH_POS_OFFSET, 4
    qba     capture_wrap

no_buffer_wrap:
    add     buffer_addr, buffer_addr, 4
    add     READ_POS_BUFF, READ_POS_BUFF, 4

    ; Handle Z signal reset
    qbbs    Z_interrupt1, r31, Z_SIGNAL_GPI
    ;zero    &QPOS, 4
Z_interrupt1:

    ; Position counter update logic
    and     a_b_transition, a_b_transition, 0xf
    add     a_b_transition, a_b_transition, 0xf0
    lbco    &qpos_update, DMEM1, a_b_transition, 1

    ; Handle phase error (simultaneous A/B transition)
    qbne    no_phase_err1, qpos_update, 3
    lbco    &scratch2, DMEM1, PHASE_ERR_CNT_OFFSET, 4
    add     scratch2, scratch2, 1
    sbco    &scratch2, DMEM1, PHASE_ERR_CNT_OFFSET, 4
    sbco    &QPOS, DMEM1, CH_POS_OFFSET, 4
    qba     capture_wrap
no_phase_err1:
    ; Handle increment/decrement based on direction
    qbbc    no_increment, qpos_update, 0
    add     QPOS, QPOS, 1
    qbge    no_qpos_overflow, QPOS, qposmax_reg    ; jump if qposmax_reg >= QPOS (i.e. QPOS <= QPOSMAX)
    ldi     QPOS, 0
no_qpos_overflow:
    sbco    &QPOS, DMEM1, CH_POS_OFFSET, 4
    qba     capture_wrap
no_increment:
    qbbc    no_decrement, qpos_update, 1
    sub     QPOS, QPOS, 1
    ldi32   scratch2, 0xFFFFFFFF
    qbne    no_qpos_underflow, QPOS, scratch2   ; only reload if result is exactly 0xFFFFFFFF (true underflow)
    ldi32   QPOS, QPOSMAX
no_qpos_underflow:
no_decrement:
    sbco    &QPOS, DMEM1, CH_POS_OFFSET, 4

    ; Check for slow mode transition
    qbbc    capture_wrap, r31, 30

capture_slow:
    halt    ; Halt PRU when in slow mode
