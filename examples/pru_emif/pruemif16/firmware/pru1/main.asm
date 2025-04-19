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

;************************************************************************************
;   File:     main.asm
;
;   Brief:    PRUEMIF16 Firmware Implementaion PRU1
;************************************************************************************

; CCS/makefile specific settings
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

;************************************* includes *************************************
        .cdecls C,NOLIST
%{
        #include "icssg_pruemif16_fw.h"
%}
; icss_constant_defines.inc: Defines symbols corresponding to Constant Table Entries
    .include "icss_constant_defines.inc"
; icss_constant_defines.inc: Defines symbols for XFER ID defines
    .include "icss_xfer_defines.inc"
; icss_reg_regs.inc: Defines symbols for IEP registers
    .include "icss_iep_regs.inc"

    .include "pruemif16_cfg.inc"
    .include "pruemif16_emif_data_cfg.inc"
;-----------------------------------------------------------------------------
;Register Defines
;-----------------------------------------------------------------------------

;   Tempoary register definition 
    .asg    R2, TEMP_REG
    .asg    R3, TEMP2_REG
;   Share flag byte of Address register
    .asg    R4.b0, EM_A_STAT_REG
;   Address word of Address register
    .asg    R4.w2, EM_A_REG
;   Register for sharing address and sahre flag to SPAD
    .asg    R4, SH_EM_A_REG
;   Register for storing data mask 
    .asg    R5, EM_D_MASK_REG
;   Register for storing complement(negation) of data mask
    .asg    R6, EM_D_MASKn_REG
;   Register for storing starting address of SHMEM
    .asg    R7, SHMEM_BUF_ADDR_REG
;   Register for modifying ICCSG_IEP Digital IO configuration
    .asg    R8, ICSSG_IEP_DIGIO_CTRL_ADDR_REG
;   Register for storing data to R30 Register 
    .asg    R9, R30_VALUE
;   Register for storing data from R31 Register
    .asg    R10, DATA_REG

;-----------------------------------------------------------------------------
;   Register R0-R1 are unused 
;   Register R11-R29 are unused 
;-----------------------------------------------------------------------------

    .global     main

     .sect       ".text"

;********
;* MAIN *
;********
main:

INIT:
;----------------------------------------------------------------------------
;   Clear the register space
;   Before begining with the application, make sure all the registers are set
;   to 0. PRU has 32 - 4 byte registers: R0 to R31, with R30 and R31 being special
;   registers for output and input respectively.
;----------------------------------------------------------------------------
; Give the starting address and number of bytes to clear.
    zero    &R0, 120

;----------------------------------------------------------------------------
;   Constant Table Entries Configuration
;   Sample code to configure Constant Table Entries.
;----------------------------------------------------------------------------
; Configure the Constant Table entry C28 to point to start of shared memory
; PRU_ICSSG Shared RAM (local-C28) : 00nn_nn00h, nnnn = c28_pointer[15:0]
; By default it is set to 0000_0000h so it will point to DMEM0 address
    ldi     TEMP_REG, 0x0100
    sbco    &TEMP_REG, ICSS_PRU_CTRL_CONST, 0x28, 2

;----------------------------------------------------------------------------
;   Initialization
;----------------------------------------------------------------------------
;----------------------------------------------------------------------------   
; Init ICSSG GPIO for Fast GPIO
; 
;This Section is SOC Specific( Modificatin required in case of different SOC)
;-----------------------------------------------------------------------------
    ; Unlock CTRLMMR partition 1
    ldi32   r4, 0x68EF3490  ; Kick 0
    ldi32   r5, 0xD172BC5A  ; kick 1
    ldi32   r6, 0x43005008  ; LOCK0 KICK0 register
    ldi32   r7, 0x4300500C  ; LOCK0 KICK1 register
    sbbo    &r4, r6, 0, 4   ; copy r4 (kick0) into r2 (lock0) with 0 offset and 4 byte length
    sbbo    &r5, r7, 0, 4   ; copy r5 (kick1) into r3 (lock1) with 0 offset and 4 byte length
    ; Write Data mask to ICSSGm_CTRLn
    ldi32   TEMP_REG, EM_D_MASK
    ldi32   TEMP2_REG, CTRLMMR_ICSSG_CTRL
    sbbo    &TEMP_REG, TEMP2_REG, 0, 3
    ; Lock CTRLMMR partition 1
    ; Unlock CTRLMMR partition 1
    ldi     r4, 0           ; Kick 0
    sbbo    &r4, r6, 0, 4   ; copy r4 (0) into r2 (lock0) with 0 offset and 4 byte length
    sbbo    &r4, r7, 0, 4   ; copy r5 (0) into r3 (lock1) with 0 offset and 4 byte length

    ; Initialize IEP DIGIO base address register
    ldi32   ICSSG_IEP_DIGIO_CTRL_ADDR_REG, ICSSG_IEP_DIGIO_CTRL

    ; Initialize Data pins as inputs
    ;sbco     &TEMP_REG, ICSS_IEP_CONST, ICSS_IEP_DIGIO_DATA_OUT_EN_REG, 3
    sbbo     &TEMP_REG, ICSSG_IEP_DIGIO_CTRL_ADDR_REG, ICSS_IEP_DIGIO_DATA_OUT_EN_REG, 3

    ; Initialize EMIF data mask registers
    mov     EM_D_MASK_REG, TEMP_REG
    not     EM_D_MASKn_REG, EM_D_MASK_REG

    ; Initialize shared memory buffer address register
    ldi32	SHMEM_BUF_ADDR_REG, SHMEM_BUF
    
    ; Initialize R30 value
    ldi   R30_VALUE, 0

    ; Drive EM1WAITn=1 (deassert)
    set     R30, R30, EM_WAITn_PIN

;----------------------------------------------------------------------------
;   Synchronize with PRU0
;----------------------------------------------------------------------------
    ; Send notification to PRU1 that init complete
    ldi     TEMP_REG.b0, PRU_INIT_MASK
    sbco    &TEMP_REG.b0, ICSS_DMEM1_CONST, FW_REG_FW_STAT, FW_REG_FW_STAT_SZ

    ; Wait for PRU1 init to complete
WAIT_INIT:
    lbco    &TEMP_REG.b0, ICSS_DMEM0_CONST, FW_REG_FW_STAT, FW_REG_FW_STAT_SZ
    qbbc    WAIT_INIT, TEMP_REG, PRU_INIT_SHIFT

WAIT_CS_HIGH:
    ; Wait for EM1CSn high
    qbbc    WAIT_CS_HIGH, R31, EM_CSn_PIN

;----------------------------------------------------------------------------
;   EMIF bus loop, wait for CSn low
;----------------------------------------------------------------------------
EMIF_CHK_CS_START_LOOP:
    ; Wait for EM1CSn high->low.
    ; This indicates the start of an EMIF bus cycle.
    qbbs    EMIF_CHK_CS_START_LOOP, R31, EM_CSn_PIN

EMIF_CHK_RD_WR_START_LOOP:
    ; Wait for EM1OEn or EM1WEn high->low
    ; The two events are mutually exclusive and indicate the start of a Read or Write:
    ;   EM1WEn=high->low => write started
    ;   EM1OEn=high->low => read started
    qbbc    EMIF_WR1, R31, EM_WEn_PIN
    qbbs    EMIF_CHK_RD_WR_START_LOOP, R31, EM_OEn_PIN

;
; EM1OEn==low
; This is an EMIF read access
;
EMIF_RD1:
    ; First EMIF read:
    ;   16-bit read -or-
    ;   1st 16-bit access of 32-bit read
    ;

    ; Drive EM1WAITn low.
    ; This indicates PRU is busy with read.
    ldi     R30_VALUE, 0
    ;clr     R30_VALUE, R30_VALUE, EM_WAITn_PIN
    mov     R30, R30_VALUE

    ; Configure Data pins as outputs
    ;
    sbbo     &EM_D_MASKn_REG, ICSSG_IEP_DIGIO_CTRL_ADDR_REG, ICSS_IEP_DIGIO_DATA_OUT_EN_REG, 3

WAIT_RD1_EMIF_ADDR:
    ; Wait for latched Address from PRU0
    ; Check share flag status
    xin     SH_EM_A_BANK, &SH_EM_A_REG, 4
    qbbc    WAIT_RD1_EMIF_ADDR, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT

    ; Reset share flag indicating Address has been read
    clr     EM_A_STAT_REG, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT
    xout    SH_EM_A_BANK, &EM_A_STAT_REG, 1

    ; Read Data from memory buffer.
    ;   Read 16-bit Data @ byte Address EM1A[EM_A_NBITS-3]:EM1A[0]:EM1BA[1]:0.
    ;   Address is offset from memory buffer base address.
    lbbo    &R30_VALUE, SHMEM_BUF_ADDR_REG, EM_A_REG, 2

    ; Drive Data onto EMIF bus
    ;   EM1D[EM_D_NBITS-1..0] = Data
    lsl     R30_VALUE, R30_VALUE, EM_D0_PIN
    and     R30_VALUE, R30_VALUE, EM_D_MASK_REG
    ; Drive EM1WAITn high.
    ; This indicates PRU is no longer busy.
    set     R30_VALUE, R30_VALUE, EM_WAITn_PIN
    ; Write to R30 
    mov     R30, R30_VALUE

EMIF_CHK_RD1_END_LOOP
    ; Wait for EM1OEn low->high.
    ; This indicates read ended.
    qbbc    EMIF_CHK_RD1_END_LOOP, R31, EM_OEn_PIN

EMIF_CHK_RD2_START_CS_END_LOOP:
    ; Wait for end of EMIF bus cycle -OR- second read in EMIF bus cycle (32-bit access)
    ;   One of two mutually exclusive events must occur:
    ;   1) EM1CSn low->high: EMIF bus cycle ended
    ;   2) EM1OEn high->low: 2nd read started
    qbbs    EMIF_RD_EXIT, R31, EM_CSn_PIN
    qbbs    EMIF_CHK_RD2_START_CS_END_LOOP, R31, EM_OEn_PIN

EMIF_RD2:
    ; EM1OEn==low
    ; Second EMIF read:
    ;   2nd 16-bit access of 32-bit read
    ;

    ; Drive EM1WAITn low.
    ; This indicates PRU is busy with read.
    ldi     R30_VALUE, 0
    ;clr     R30_VALUE, R30_VALUE, EM_WAITn_PIN
    mov     R30, R30_VALUE

WAIT_RD2_EMIF_ADDR:
    ; Wait for latched Address from PRU0
    ; Check share flag status
    xin     SH_EM_A_BANK, &SH_EM_A_REG, 4
    qbbc    WAIT_RD1_EMIF_ADDR, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT

    ; Reset share flag indicating Address has been read
    clr     EM_A_STAT_REG, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT
    xout    SH_EM_A_BANK, &EM_A_STAT_REG, 1

    ; Read Data from memory buffer.
    ;   Read 16-bit Data @ byte Address EM1A[EM_A_NBITS-3]:EM1A[0]:EM1BA[1]:0.
    ;   Address is offset from memory buffer base address.
    lbbo    &R30_VALUE, SHMEM_BUF_ADDR_REG, EM_A_REG, 2

    ; Drive Data onto EMIF bus
    ;   EM1D[EM_D_NBITS-1..0] = Data
    lsl     R30_VALUE, R30_VALUE, EM_D0_PIN
    and     R30_VALUE, R30_VALUE, EM_D_MASK_REG
    ; Drive EM1WAITn high.
    ; This indicates PRU is no longer busy.
    set     R30_VALUE, R30_VALUE, EM_WAITn_PIN
    ; Write to R30 
    mov     R30, R30_VALUE

EMIF_CHK_RD2_END_LOOP:
    ; Wait for EM1OEn low->high.
    ; This indicates read ended.
    qbbc    EMIF_CHK_RD2_END_LOOP, R31, EM_OEn_PIN

EMIF_CHK_RD_CS_END_LOOP:
    ; Wait for EM1CSn low->high.
    ; This indicates the end of EMIF bus cycle.
    qbbc    EMIF_CHK_RD_CS_END_LOOP, R31, EM_CSn_PIN

EMIF_RD_EXIT:
    ; Configure Data pins as inputs
    ;
    sbbo     &EM_D_MASK_REG, ICSSG_IEP_DIGIO_CTRL_ADDR_REG, ICSS_IEP_DIGIO_DATA_OUT_EN_REG, 3

    ; EMIF bus cycle ended, start over
    jmp     EMIF_CHK_CS_START_LOOP

;
; EM1WEn==low
; This is an EMIF write access
;
EMIF_WR1:
    ; First EMIF write:
    ;   16-bit write -or-
    ;   1st 16-bit access of 32-bit write
    ;
    
    ; Drive EM1WAITn low.
    ; This indicates PRU is busy with write.
    ;clr     R30_VALUE, R30_VALUE, EM_WAITn_PIN
    ;mov     R30, R30_VALUE
    clr     R30, R30, EM_WAITn_PIN
    
    ; Latch EM1D[EM_D_NBITS-1..0]
    and     DATA_REG, R31, EM_D_MASK_REG
    lsr     DATA_REG, DATA_REG, EM_D0_PIN

WAIT_WR1_EMIF_ADDR:
    ; Wait for latched Address from PRU0
    ; Check share flag status
    xin     SH_EM_A_BANK, &SH_EM_A_REG, 4
    qbbc    WAIT_WR1_EMIF_ADDR, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT

    ; Reset share flag indicating Address has been read
    clr     EM_A_STAT_REG, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT
    xout    SH_EM_A_BANK, &EM_A_STAT_REG, 1

    ; Write Data to memory buffer.
    ;   Write 16-bit Data @ byte Address EM1A[EM_A_NBITS-3]:EM1A[0]:EM1BA[1]:0.
    ;   Address is offset from memory buffer base address.
    sbbo    &DATA_REG, SHMEM_BUF_ADDR_REG, EM_A_REG, 2
    
    ; Drive EM1WAITn high.
    ; This indicates PRU is no longer busy.
    ;set     R30_VALUE, R30_VALUE, EM_WAITn_PIN
    ;mov     R30, R30_VALUE
    set     R30, R30, EM_WAITn_PIN
    
    ; Wait for EM1WEn low->high.
    ; This indicates a write ended.
EMIF_CHK_WR1_END_LOOP:
    ; Wait for EM1WEn low->high.
    ; This indicates write ended.
    qbbc    EMIF_CHK_WR1_END_LOOP, R31, EM_WEn_PIN
    
EMIF_CHK_WR2_START_CS_END_LOOP:
    ; Wait for end of EMIF bus cycle -OR- second write in EMIF bus cycle (32-bit access)
    ;   One of two mutually exclusive events must occur:
    ;   1) EM1CSn low->high: EMIF bus cycle ended
    ;   2) EM1WEn high->low: 2nd write started
    qbbs    EMIF_CHK_CS_START_LOOP, R31, EM_CSn_PIN
    qbbs    EMIF_CHK_WR2_START_CS_END_LOOP, R31, EM_WEn_PIN
    
EMIF_WR2:
    ; EM1WEn==low
    ; Second EMIF write:
    ;   2nd 16-bit access of 32-bit write
    ;

    ; Drive EM1WAITn low.
    ; This indicates PRU is busy with write.
    ;clr     R30_VALUE, R30_VALUE, EM_WAITn_PIN
    ;mov     R30, R30_VALUE
    clr     R30, R30, EM_WAITn_PIN

    ; Latch EM1D[EM_D_NBITS-1..0]
    and     DATA_REG, R31, EM_D_MASK_REG
    lsr     DATA_REG, DATA_REG, EM_D0_PIN

WAIT_WR2_EMIF_ADDR:
    ; Wait for latched Address from PRU0
    ; Check share flag status
    xin     SH_EM_A_BANK, &SH_EM_A_REG, 4
    qbbc    WAIT_WR2_EMIF_ADDR, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT

    ; Reset share flag indicating Address has been read
    clr     EM_A_STAT_REG, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT
    xout    SH_EM_A_BANK, &EM_A_STAT_REG, 1

    ; Write Data to memory buffer.
    ;   Write 16-bit Data @ byte Address EM1A[EM_A_NBITS-3]:EM1A[0]:EM1BA[1]:0.
    ;   Address is offset from memory buffer base address.
    sbbo    &DATA_REG, SHMEM_BUF_ADDR_REG, EM_A_REG, 2

    ; Drive EM1WAITn high.
    ; This indicates PRU is no longer busy.
    ;set     R30_VALUE, R30_VALUE, EM_WAITn_PIN
    ;mov     R30, R30_VALUE
    set     R30, R30, EM_WAITn_PIN

EMIF_CHK_WR2_END_LOOP:
    ; Wait for EM1WEn low->high.
    ; This indicates write ended.
    qbbc    EMIF_CHK_WR2_END_LOOP, R31, EM_WEn_PIN

EMIF_CHK_WR_CS_END_LOOP:
    ; Wait for EM1CSn low->high.
    ; This indicates the end of EMIF bus cycle.
    qbbc    EMIF_CHK_WR_CS_END_LOOP, R31, EM_CSn_PIN

    ; EMIF bus cycle ended, start over
    jmp     EMIF_CHK_CS_START_LOOP

    halt ; end of program
