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
;--------------------------------------------------------------------------;
; REGISTER ASSIGNMENT
    .asg   r0,     scratch
    .asg   r1,     scratch2
    .asg   r17.b0, curr_gpi_sample  ; Current GPI state after masking
    .asg   r17.b1, A_B_Z_edge       ; Edge detection for A,B,Z signals
    .asg   r18,    curr_ts          ; Timestamp counter
    .asg   r19.b0, edge             ; Edge detection
    .asg   r20.b0, prev_sample      ; Previous state of A,B,Z signals
    .asg   r21,    time_stamp       ; Time stamp counter
    .asg   r22.w0, buffer_addr      ; Buffer address pointer
    .asg   r22.w2, buffer_size      ; Buffer size
    .asg   r23,    wr_ptr           ; Write pointer
    .asg   r24.w0, READ_POS_BUFF    ; Read position buffer
    .asg   r24.w2, read_pos_speed_intr ; Read position speed interrupt
    .asg   r26.b0, a_b_transition   ; A/B transition state
    .asg   r26.b1, qpos_update      ; 2-> QPOS--, 1-> QPOS++, 0-> no change
    .asg   r27,    QPOS             ; Position counter
    .asg   r28,    DMEM0            ; Data memory pointer

; Define cur_sample based on core type
    .if $isdefed("RTU_PRU0") | $isdefed("RTU_PRU1")
        .asg   r31.b0, cur_sample   ; Use byte0 for GPIs 0-7
    .elseif $isdefed("PRU0") 
        .asg   r31.b0, cur_sample   ; Use byte0 for GPIs 0-7
    .elseif $isdefed("TX_PRU0") 
        .asg   r31.b0, cur_sample   ; Use byte0 for GPIs 0-7
    .elseif $isdefed("TX_PRU1") 
        .asg   r31.b1, cur_sample   ; Use byte1 for GPIs 8-15
    .elseif $isdefed("PRU1") 
        .asg   r31.b1, cur_sample   ; Use byte1 for GPIs 8-15
    .endif

; Constants
    .if $isdefed("TX_PRU0") | $isdefed("TX_PRU1")
    ; C11 is not set correctly for TXPRUs. So C28 is programmed and used.
PRU_CTRL_CONST	    .set	c28
    .else
PRU_CTRL_CONST	    .set	c11
    .endif
PRU_CYCLCNT	        .set	0x0c
MAX_BUFFER          .set    0x400     ; Buffer size = 8 kB
READ_POS_SPEED_BUFF .set    0x4

;--------------------------------------------------------------------------;
; Core-specific definitions

; RTU_PRU0 Configuration (GPIs 0-7)
    .if $isdefed("RTU_PRU0")
DMEM0_BASE          .set    0x0
DMEM1               .set    C25
intr_mask           .set    0x20    ; PRU INTC EVENT 16
A_SIGNAL_GPI        .set    (0x0)     ; 
B_SIGNAL_GPI        .set    (0x1)     ; 
Z_SIGNAL_GPI        .set    (0x6)     ; 
A_B_Z_GPI_mask      .set    0x43     ; Byte level mask for GPI0,1 (0b0100_0011)
CH_POS_OFFSET       .set    0x1C
WRITE_PTR_OFFSET    .set    0x0
PHASE_ERR_CNT_OFFSET .set   0x34
    .endif

; PRU0 Configuration (GPIs 8-15)
    .if $isdefed("PRU0")
DMEM0_BASE          .set    0x400
DMEM1               .set    C25
intr_mask           .set    0x21    ; PRU INTC EVENT 17
A_SIGNAL_GPI        .set    (0x2)      ; 
B_SIGNAL_GPI        .set    (0x3)      ; 
Z_SIGNAL_GPI        .set    (0x6)      ; 
A_B_Z_GPI_mask      .set    0x4C    ; Byte level mask for GPI2,3 (0b0100_1100)
CH_POS_OFFSET       .set    0x20
WRITE_PTR_OFFSET    .set    0x4
PHASE_ERR_CNT_OFFSET .set   0x38
    .endif

; TX_PRU0 Configuration (GPIs 16-19)
    .if $isdefed("TX_PRU0")
DMEM0_BASE          .set    0x800
DMEM1               .set    C25
intr_mask           .set    0x22    ; PRU INTC EVENT 18
A_SIGNAL_GPI        .set    (0x4)     ; 
B_SIGNAL_GPI        .set    (0x5)     ; 
Z_SIGNAL_GPI        .set    (0x6)     ; 
A_B_Z_GPI_mask      .set    0x70     ; Byte level mask for GPI4,5 (0b0111_0000)
CH_POS_OFFSET       .set    0x24
WRITE_PTR_OFFSET    .set    0x8
PHASE_ERR_CNT_OFFSET .set   0x3C
    .endif

; RTU_PRU1 Configuration (GPIs 0-7)
    .if $isdefed("RTU_PRU1")
DMEM0_BASE          .set    0x2C00
DMEM1               .set    C24
intr_mask           .set    0x23    ; PRU INTC EVENT 19
A_SIGNAL_GPI        .set    (0x0)     ; 
B_SIGNAL_GPI        .set    (0x1)     ; 
Z_SIGNAL_GPI        .set    (0x2)     ; 
A_B_Z_GPI_mask      .set    0x7     ; Byte level mask for GPI0,1 (0b0000_0111)
CH_POS_OFFSET       .set    0x28
WRITE_PTR_OFFSET    .set    0xC
PHASE_ERR_CNT_OFFSET .set   0x40
    .endif

; PRU1 Configuration (GPIs 8-15)
    .if $isdefed("PRU1")
DMEM0_BASE          .set    0x3000
DMEM1               .set    C24
intr_mask           .set    0x24    ; PRU INTC EVENT 20
A_SIGNAL_GPI        .set    (0xB) - 8  ;  
B_SIGNAL_GPI        .set    (0xC) - 8  ;  
Z_SIGNAL_GPI        .set    (0x2)      ;
A_B_Z_GPI_mask      .set    0x5C    ; Byte level mask for GPI11,12 (0b0101_1100)
CH_POS_OFFSET       .set    0x2C
WRITE_PTR_OFFSET    .set    0x10
PHASE_ERR_CNT_OFFSET .set   0x44
    .endif

; TX_PRU1 Configuration (GPIs 16-19)
    .if $isdefed("TX_PRU1")
DMEM0_BASE          .set    0x3400
DMEM1               .set    C24
intr_mask           .set    0x25    ; PRU INTC EVENT 21
A_SIGNAL_GPI        .set    (0xD) - 8   ;  
B_SIGNAL_GPI        .set    (0xF) - 8   ;  
Z_SIGNAL_GPI        .set    (0x2)     ; 
A_B_Z_GPI_mask      .set    0xA4     ; Byte level mask for GPI13,15 (0b1010_0100)

CH_POS_OFFSET       .set    0x30
WRITE_PTR_OFFSET    .set    0x14
PHASE_ERR_CNT_OFFSET .set   0x48
    .endif

;--------------------------------------------------------------------------;
; GPI Byte Selection Guide:
; GPIs 0-7:   Use r31.b0  (RTU_PRU0, RTU_PRU1)
; GPIs 8-15:  Use r31.b1  (PRU0, PRU1)
; GPIs 16-19: Use r31.b2  (TX_PRU0, TX_PRU1)
;
; Note: GPI numbers are adjusted to byte-level (0-7) for each byte:
; - For GPIs 8-15:  Subtract 8  from GPI number
; - For GPIs 16-19: Subtract 16 from GPI number
;
; Register Usage Notes:
; r17.b0 (curr_gpi_sample): Stores masked GPI state
; r17.b1 (A_B_Z_edge): Edge detection for signals
; r31.b0/b1/b2 (cur_sample): Raw GPI input - byte selection based on GPI numbers
;--------------------------------------------------------------------------;
