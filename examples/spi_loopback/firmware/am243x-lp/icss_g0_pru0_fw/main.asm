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
;   Brief:    SPI Master PRU Implementation Example. This file demonstrates usage of 
;             the SPI master macros which provides functionalities including send, 
;             read, and transfer operations. The type of operation is selected by 
;             modifying the branch instruction under the PROGRAM_START label .
;
;   Psuedo-code:
;             init: 
;                Initializes registers and memory
;             while(1){ //PROGRAM_START:
;                Jumps to programmed SPI master macro
;                if(SPI_master_send){ //SPI_master_send:
;                   pull down CS pin
;                   send data
;                   pull up CS pin
;                }
;                else if(SPI_master_read){ //SPI_master_read:
;                   pull down CS pin
;                   read data
;                   pull up CS pin
;                   store data //store_data
;                }
;                else if(SPI_master_transfer){ //SPI_master_transfer:
;                   pull down CS pin
;                   send and read data concurrently
;                   pull up CS pin
;                   store data //store_data:
;                }
;             }
;             store_data:
;                Store data at memory address (c28 + temp_reg_1)
;                if(MAX_BUFFER_SIZE <= temp_reg_1){
;                   temp_reg_1 = DATA_BUFFER_OFFSET
;                }
;                else{
;                   temp_reg_1 = temp_reg_1 + 4;
;                }
;************************************************************************************

;************************************* includes *************************************
; Include files for importing symbols and macros
    .include "icss_constant_defines.inc"
    .include "spi_master_macros.inc"

; Used to receive command values and interrupt from R5F core
    .asg    0x16,       PRGM_FLOW_EVENT
    .asg    31,         PRGM_FLOW_EVENT_BIT
    .asg    3,          PRU_CLK_CYC_PRD_CONST ; 3ns cycle period

; Import the Chip Support Library Register Address defines
    .cdecls C,  NOLIST
%{
#include "drivers/pruicss/g_v0/cslr_icss_g.h"
%}

; CCS/makefile specific settings
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

    .global     main
    .sect       ".text"

; Registers used in this program
    .asg    R1,         TEMP_PTR_1
    .asg    R10,        TEMP_REG_1
    .asg    R11,        TEMP_REG_2
    .asg    R12,        TEMP_REG_3
    .asg    R13.b0,     bitIndex1
    .asg    R2,         dataReg
    .asg    R3.b0,      bitId
    .asg    R4,         s_dataReg
    .asg    R5,         r_dataReg

; PRU GPIO pin mapping for SPI Signals
SDI_PIN                 .set    14      ; Always input ; Master mode - Input to Master ; Device Mode - Input to Device
SDO_PIN                 .set    3       ; Always output ; Master mode - output from Master ; Device Mode - Output from Device
SCLK_PIN                .set    4       ; Output during Master mode ; Input during Device mode
CS_PIN                  .set    5       ; Output during Master mode ; Input during Device mode

;Constants definitions
DATA_BUFFER_OFFSET      .set    0x0028
MAX_BUFFER_SIZE         .set    40+(9*4)
TEST_DATA_1             .set    0xDEADBEEF
TEST_DATA_2             .set    0xAAAAAAAA

;********
;* MAIN *
;********;

main:

init:
    zero    &r0, 120  ; Clear the register space

    ; Configure the Constant Table entry C28 to point to start of shared memory
    ; PRU_ICSSG Shared RAM (local-C28) : 00nn_nn00h, nnnn = c28_pointer[15:0]
    ldi     TEMP_REG_1, 0x0100
    sbco    &TEMP_REG_1, ICSS_PRU_CTRL_CONST, 0x28, 2

    ; Initializing registers
    ldi32   TEMP_REG_1, DATA_BUFFER_OFFSET ;offset address at which received data is written (10*4 byte buffer)
    ldi32   TEMP_REG_2, TEST_DATA_2 ;example data
    ldi32   TEMP_REG_3, TEST_DATA_1 ;example data

PROGRAM_START:
    ; Change this label to select the type of SPI transmission.
    ; eg: SPI_master_send, SPI_master_read or SPI_master_transfer
    qba     SPI_master_transfer

SPI_master_send:
    mov     dataReg, TEMP_REG_2
    m_pru_clr_pin   CS_PIN  ; Clear the chip select pin
    m_wait_nano_sec 10      ; Wait for 10 nanoseconds
    ; For sending 32 bit data MSB first at SCLK of 41.66MHz
    m_send_packet_spi_msb_gpo_sclk dataReg, 32, bitId, SCLK_PIN, SDO_PIN, 0, 1, "MODE3"
    m_wait_nano_sec 10
    m_pru_set_pin   CS_PIN  ; Set the chip select pin
    m_wait_nano_sec 100
    qba     PROGRAM_START

SPI_master_read:
    m_pru_clr_pin   CS_PIN
    m_wait_nano_sec 35
    ; To read 32 bit data MSB first from SPI slave at max freq of 13.88MHz
    m_read_packet_spi_msb_gpo_sclk r_dataReg, 32, bitId, SCLK_PIN, SDI_PIN, 10, 7, "MODE3"
    m_wait_nano_sec 10
    m_pru_set_pin   CS_PIN
    qba     store_data

SPI_master_transfer:
    ldi32   r_dataReg, 0xFFFFFFFF
    mov     s_dataReg, TEMP_REG_3
    m_pru_clr_pin   CS_PIN
    m_wait_nano_sec 35
    ; For transfer (send and read) at 12.8MHz
    m_transfer_packet_spi_master_gpo_sclk r_dataReg, s_dataReg, 32, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, 9, 7, "MODE3", "MSB"
    m_wait_nano_sec 10
    m_pru_set_pin   CS_PIN

store_data:
    ; Write data read from SDI into R4
    sbco    &r_dataReg, c28, TEMP_REG_1, 4
    qble    reset_ptr, TEMP_REG_1, MAX_BUFFER_SIZE
    add     TEMP_REG_1, TEMP_REG_1, 4
    qba     continue
reset_ptr:
    ldi     TEMP_REG_1, DATA_BUFFER_OFFSET
continue:
    m_wait_nano_sec 100
    qba     PROGRAM_START

    halt ; should never reach this code