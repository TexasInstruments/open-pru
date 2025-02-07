; Copyright (C) 2024 Texas Instruments Incorporated - http://www.ti.com/
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
;   Brief:    PRUEMIF16 Firmware Implementaion PRU0
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

    .include "pruemif16_cfg.inc"
    .include "pruemif16_emif_addr_cfg.inc"

;-----------------------------------------------------------------------------
; Register defines
;-----------------------------------------------------------------------------
     
;   Temporary Registers
    .asg    R2, TEMP_REG
    .asg    R3, TEMP2_REG
;   Share flag byte of Address register
    .asg    R4.b0, EM_A_STAT_REG   
;   Address word of Address register
    .asg    R4.w2, EM_A_REG
;   Register for sharing address and share flag to SPAD
    .asg    R4, SH_EM_A_REG
;   Register for storing address mask
    .asg    R5, EM_A_MASK_REG

;-----------------------------------------------------------------------------
;   R6-R29 registeres are unused
;   R0-R1 registers are unused 
;-----------------------------------------------------------------------------

    .global     main
    
	.sect     ".text"

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
    ; Initialize information shared with PRU1
    ;
    ; Initialize EMIF address valid flag
    ldi     EM_A_STAT_REG, EMIF_ADDR_VALID_RESET_VAL
    ; Initialize EMIF address
    ldi     EM_A_REG, EMIF_ADDR_RESET_VAL
    ; Write initialized EMIF address register to SPAD
    xout    SH_EM_A_BANK, &SH_EM_A_REG, 4
    
    ; Initialize EMIF address mask register
    ldi32   EM_A_MASK_REG, EM_A_MASK
    
;----------------------------------------------------------------------------
;   Synchronize with PRU1
;----------------------------------------------------------------------------
    ; Send notification to PRU1 that init complete
    ldi     TEMP_REG.b0, PRU_INIT_MASK
    sbco    &TEMP_REG.b0, ICSS_DMEM0_CONST, FW_REG_FW_STAT, FW_REG_FW_STAT_SZ

    ; Wait for PRU1 init to complete
WAIT_INIT:
    lbco    &TEMP_REG.b0, ICSS_DMEM1_CONST, FW_REG_FW_STAT, FW_REG_FW_STAT_SZ
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

    ; Latch Address = EM1A[EM_A_NBITS-3]:EM1A[0]:EM1BA[1]:0.
    ;   LSB of address is assumed to be grounded externally,
    ;   this provides a byte address of a 16-bit Data word.
    and     TEMP_REG, R31, EM_A_MASK_REG
    lsr     EM_A_REG, TEMP_REG, EM_A0_PIN
    
    ; Share latched Address with PRU1
    ;
    ; Check share flag status
    ; Share flag should always be set to 0 when a Read/Write is started
    xin     SH_EM_A_BANK, &EM_A_STAT_REG, 1
    qbbs    SHARE_EMIF_ADDR_ERR, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT

    ; Write Address to Shared location
    ;
    ; Set share flag to 1 indicating Address has been written
    set     EM_A_STAT_REG, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT
    ; Write EMIF address register to SPAD
    xout    SH_EM_A_BANK, &SH_EM_A_REG, 4

EMIF_CHK_RD1_END_LOOP:
    ; Wait for EM1OEn low->high.
    ; This indicates read ended.
    qbbc    EMIF_CHK_RD1_END_LOOP, R31, EM_OEn_PIN

EMIF_CHK_RD2_START_CS_END_LOOP:
    ; Wait for end of EMIF bus cycle -OR- second read in EMIF bus cycle (32-bit access)
    ;   One of two mutually exclusive events must occur:
    ;   1) EM1CSn low->high: EMIF bus cycle ended
    ;   2) EM1OEn high->low: 2nd read started
    qbbs    EMIF_CHK_CS_START_LOOP, R31, EM_CSn_PIN
    qbbs    EMIF_CHK_RD2_START_CS_END_LOOP, R31, EM_OEn_PIN

EMIF_RD2:
    ; EM1OEn==low
    ; Second EMIF read:
    ;   2nd 16-bit access of 32-bit read
    ;

    ; Latch Address = EM1A[EM_A_NBITS-3]:EM1A[0]:EM1BA[1]:0.
    ;   LSB of address is assumed to be grounded externally,
    ;   this provides a byte address of a 16-bit Data word.
    and     TEMP_REG, R31, EM_A_MASK_REG
    lsr     EM_A_REG, TEMP_REG, EM_A0_PIN

    ; Share latched Address with PRU1
    ;
    ; Check share flag status
    ; Share flag should always be set to 0 when a Read/Write is started
    xin     SH_EM_A_BANK, &EM_A_STAT_REG, 1
    qbbs    SHARE_EMIF_ADDR_ERR, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT

    ; Write Address to Shared location
    ;
    ; Set share flag to 1 indicating Address has been written
    set     EM_A_STAT_REG, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT
    ; Write EMIF address register to SPAD
    xout    SH_EM_A_BANK, &SH_EM_A_REG, 4

EMIF_CHK_RD2_END_LOOP:
    ; Wait for EM1OEn low->high.
    ; This indicates read ended.
    qbbc    EMIF_CHK_RD2_END_LOOP, R31, EM_OEn_PIN

EMIF_CHK_RD_CS_END_LOOP:
    ; Wait for EM1CSn low->high.
    ; This indicates the end of EMIF bus cycle.
    qbbc    EMIF_CHK_RD_CS_END_LOOP, R31, EM_CSn_PIN

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

    ; Latch Address = EM1A[EM_A_NBITS-3]:EM1A[0]:EM1BA[1]:0.
    ;   LSB of address is assumed to be grounded externally,
    ;   this provides a byte address of a 16-bit Data word.
    and     TEMP_REG, R31, EM_A_MASK_REG
    lsr     EM_A_REG, TEMP_REG, EM_A0_PIN

    ; Share latched Address with PRU1
    ;
    ; Check share flag status
    ; Share flag should always be set to 0 when a Read/Write is started
    xin     SH_EM_A_BANK, &EM_A_STAT_REG, 1
    qbbs    SHARE_EMIF_ADDR_ERR, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT

    ; Write Address to Shared location
    ;
    ; Set share flag to 1 indicating Address has been written
    set     EM_A_STAT_REG, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT
    ; Write EMIF address register to SPAD
    xout    SH_EM_A_BANK, &SH_EM_A_REG, 4

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

    ; Latch Address = EM1A[EM_A_NBITS-3]:EM1A[0]:EM1BA[1]:0.
    ;   LSB of address is assumed to be grounded externally,
    ;   this provides a byte address of a 16-bit Data word.
    and     TEMP_REG, R31, EM_A_MASK_REG
    lsr     EM_A_REG, TEMP_REG, EM_A0_PIN

    ; Share latched Address with PRU1
    ;
    ; Check share flag status
    ; Share flag should always be set to 0 when a Read/Write is started
    xin     SH_EM_A_BANK, &EM_A_STAT_REG, 1
    qbbs    SHARE_EMIF_ADDR_ERR, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT

    ; Write Address to Shared location
    ;
    ; Set share flag to 1 indicating Address has been written
    set     EM_A_STAT_REG, EM_A_STAT_REG, EMIF_ADDR_VALID_SHIFT
    ; Write EMIF address register to SPAD
    xout    SH_EM_A_BANK, &SH_EM_A_REG, 4

EMIF_CHK_WR2_END_LOOP
    ; Wait for EM1WEn low->high.
    ; This indicates write ended.
    qbbc    EMIF_CHK_WR2_END_LOOP, R31, EM_WEn_PIN

EMIF_CHK_WR_CS_END_LOOP:
    ; Wait for EM1CSn low->high.
    ; This indicates the end of EMIF bus cycle.
    qbbc    EMIF_CHK_WR_CS_END_LOOP, R31, EM_CSn_PIN

    ; EMIF bus cycle ended, start over
    jmp     EMIF_CHK_CS_START_LOOP

; EMIF address sharing error.
; For now, treat as unrecoverable error.
SHARE_EMIF_ADDR_ERR:
    lbco    &TEMP_REG.b0, ICSS_DMEM0_CONST, FW_REG_FW_STAT, FW_REG_FW_STAT_SZ
    or      TEMP_REG.b0, TEMP2_REG.b0, SHARE_ERR_MASK
    sbco    &TEMP_REG.b0, ICSS_DMEM0_CONST, FW_REG_FW_STAT, FW_REG_FW_STAT_SZ
    halt
