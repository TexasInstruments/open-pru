; Copyright (C) 2022 Texas Instruments Incorporated - http://www.ti.com/
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
;   File:     spi_slave_macros.inc
;
;   Brief:    SPI Slave interface macros for all MODES
;
;   Includes:
;            icss_pin_macros.inc
;            time_macros.inc
;************************************************************************************

; Chip select (CS)  is to be polled separately

    .include "icss_pin_macros.inc"
    .include "time_macros.inc"

;  SPI Mode | CPOL | CPHA |  Idle State Clock Polarity | Clock Phase Used to Sample and/or Shift the Data
;     0     |  0   |  0   |       Logic low            | Data sampled on the rising edge and shifted out on the falling edge
;     1     |  0   |  1   |       Logic low            | Data sampled on the falling edge and shifted out on the rising edge
;     2     |  1   |  0   |       Logic high           | Data sampled on the falling edge and shifted out on the rising edge
;     3     |  1   |  1   |       Logic high           | Data sampled on the rising edge and shifted out on the falling edge


;************************************************************************************
;   Macro: m_read_packet_spi_slave_msb_gpi_sclk
;
;   Reads 1 data packet through the SPI slave interface MSB first
;
;   PEAK cycles: 7 cycles
;   Acceptable SCLK freq at 3ns cycle time: <= 47.619 MHz
;   Acceptable CS to SCLK start time:       > 6 cycles + prop delay + slew rate
;
;   Pseudo code:
;       ========== C ==========
;       dataReg = 0;
;       bitId = PACKETSIZE;
;       for (bitId >= 0)
;           bitId --;
;           wait for SCLK_PIN sampling edge
;           read bit into dataReg
;       =======================
;
;   Parameters:
;           dataReg         -  Contains received data packet
;           PACKETSIZE      -  Size of data packet in bits (max 32 bits)
;           bitId           -  To track Bit index (8 bits wide)
;           SCLK_PIN        -  PRU Pin number for SCLK
;           SDI_PIN         -  PRU Pin number for SDI
;************************************************************************************
m_read_packet_spi_slave_msb_gpi_sclk   .macro  dataReg, PACKETSIZE, bitId, SCLK_PIN, SDI_PIN, MODE
    ; clear dataReg which is used to store received data
    ldi     bitId, PACKETSIZE       ; bitId is bit position pointer

READ_BIT_LOOP?:
    ;decrementing pointer
    sub     bitId, bitId, 1

    ;wait for sampling edge
    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    ;wait for rising edge
    m_wait_low_pulse    1, SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    ;wait for falling edge
    m_wait_high_pulse    1, SCLK_PIN
    .endif

    ;sampling received bit
    qbbc    BIT_ENTRY_0?, r31, SDI_PIN
    set     dataReg, dataReg, bitId     ; store data in register
    qba     SKIP_BIT_ENTRY_0?
BIT_ENTRY_0?:
    clr     dataReg, dataReg, bitId
    nop
SKIP_BIT_ENTRY_0?:
    qbne    READ_BIT_LOOP?, bitId, 0
    .endm


;************************************************************************************
;   Macro: m_read_packet_spi_slave_lsb_gpi_sclk
;
;   Reads 1 data packet through the SPI slave interface LSB first
;
;   PEAK cycles: 7 cycles
;   Acceptable SCLK freq at 3ns cycle time: <= 47.619 MHz
;   Acceptable CS to SCLK start time:       > 6 cycles + prop delay + slew rate
;
;   Pseudo code:
;       ========== C ==========
;       dataReg = 0;
;       bitId = 0;
;       for (bitId < PACKETSIZE)
;           bitId ++;
;           wait for SCLK_PIN sampling edge
;           read bit into dataReg
;       =======================
;
;   Parameters:
;           dataReg         -  Contains received data packet
;           PACKETSIZE      -  Size of data packet in bits (max 32 bits)
;           bitId           -  To track Bit index (8 bits wide)
;           SCLK_PIN        -  PRU Pin number for SCLK
;           SDI_PIN         -  PRU Pin number for SDI
;************************************************************************************
m_read_packet_spi_slave_lsb_gpo_sclk   .macro  dataReg, PACKETSIZE, bitId, SCLK_PIN, SDI_PIN, MODE

    ; clear dataReg which is used to store received data
    ldi     bitId, 255       ; bitId is bit position pointer

READ_BIT_LOOP?:
    ;incrementing pointer
    add     bitId, bitId, 1

    ;wait for sampling edge
    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    ;wait for rising edge
    m_wait_low_pulse    1, SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    ;wait for falling edge
    m_wait_high_pulse    1, SCLK_PIN
    .endif

    ;sampling received bit
    qbbc    BIT_ENTRY_0?, r31, SDI_PIN
    set     dataReg, dataReg, bitId     ; store data in register
    qba     SKIP_BIT_ENTRY_0?
BIT_ENTRY_0?:
    clr     dataReg, dataReg, bitId
    nop
SKIP_BIT_ENTRY_0?:
    qbne    READ_BIT_LOOP?, bitId, PACKETSIZE-1

    .endm


;************************************************************************************
;   Macro: m_send_packet_spi_slave_msb_gpo_sclk
;
;   Send 1 data packet through the SPI interface.
;
;   PEAK cycles: 7 cycles
;
;   Theoretical maximum acceptable SCLK freq at 3ns cycle time: <= 47.619 MHz
;   Practical limitation of 13.88MHz
;   Acceptable CS to SCLK start time: > 3 cycles + 2 cycles (for CS polling and jump)
;
;   Pseudo code:
;       ========== C ==========
;       dataReg = 0
;       bitId = PACKETSIZE - 1
;       loop:   READ_PACKET
;           bitId --;
;           wait for SCLK_PIN shifting edge
;           send dataReg bit
;           wait for SCLK_PIN sampling edge
;       =======================
;
;   Parameters:
;           dataReg         -  Contains data packet to send
;           PACKETSIZE      -  Size of data packet in bits
;           bitId           -  To track Bit index (8 bits wide)
;           SCLK_PIN        -  PRU Pin number for SCLK
;           SDO_PIN         -  PRU Pin number for SDO
;************************************************************************************
m_send_packet_spi_slave_msb_gpo_sclk   .macro  dataReg, PACKETSIZE, bitId, SCLK_PIN, SDO_PIN, MODE
    ldi     bitId, PACKETSIZE - 1   ; bitId is bit position pointer

    ;wait for shifting edge
    .if $symcmp(MODE,"MODE1") = 0
    ;wait for rising edge
    m_wait_low_pulse    1, SCLK_PIN
    .elseif $symcmp(MODE,"MODE3") = 0
    ;wait for falling edge
    m_wait_high_pulse    1, SCLK_PIN
    .endif
    ;shift data right away for mode0 and mode2 as SCLK signal gives no indication for shifting of intial bit

SEND_BIT_LOOP?:
    qbbc            data_low?, dataReg, bitId
    m_pru_set_pin   SDO_PIN
    qba             data_high?
data_low?:
    m_pru_clr_pin   SDO_PIN
    NOP
data_high?:
    qbeq            SEND_BIT_LOOP_END?, bitId, 0
    sub             bitId, bitId, 1

    ;wait for shifting edge
    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    ;wait for falling edge
    m_wait_high_pulse    1, SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    ;wait for rising edge
    m_wait_low_pulse    1, SCLK_PIN
    .endif

    qbbc            data_low?, dataReg, bitId
    m_pru_set_pin   SDO_PIN
    qba             data_high?
SEND_BIT_LOOP_END?:

    .endm

;************************************************************************************
;   Macro: m_send_packet_spi_slave_lsb_gpo_sclk
;
;   Send 1 data packet through the SPI interface.
;
;   PEAK cycles: 7 cycles
;
;   Theoretical maximum SCLK freq at 3ns cycle time: <= 47.619 MHz
;   Practical limitation of 13.88MHz
;   Acceptable CS to SCLK start time: > 3 cycles + 2 cycles (for CS polling and jump)
;
;   Pseudo code:
;       ========== C ==========
;       dataReg = 0
;       bitId = PACKETSIZE - 1
;       loop:   READ_PACKET
;           set   SCLK_PIN
;           wait  16 cycles
;           clear SCLK_PIN
;       =======================
;
;   Parameters:
;           dataReg         -  Contains data packet to send
;           PACKETSIZE      -  Size of data packet in bits
;           bitId           -  To track Bit index (8 bits wide)
;           SCLK_PIN        -  PRU Pin number for SCLK
;           SDO_PIN         -  PRU Pin number for SDO
;************************************************************************************
m_send_packet_spi_slave_lsb_gpo_sclk   .macro  dataReg, PACKETSIZE, bitId, SCLK_PIN, SDO_PIN, MODE
    ldi     bitId, 0   ; bitId is bit position pointer

    ;wait for shifting edge
    .if $symcmp(MODE,"MODE1") = 0
    ;wait for rising edge
    m_wait_low_pulse    1, SCLK_PIN
    .elseif $symcmp(MODE,"MODE3") = 0
    ;wait for falling edge
    m_wait_high_pulse    1, SCLK_PIN
    .endif
    ;shift data right away for mode0 and mode2 as SCLK signal gives no indication for shifting of intial bit

SEND_BIT_LOOP?:

    qbbc            data_low?, dataReg, bitId
    m_pru_set_pin   SDO_PIN
    qba             data_high?
data_low?:
    m_pru_clr_pin   SDO_PIN
    NOP
data_high?:
    add             bitId, bitId, 1
    qbeq            SEND_BIT_LOOP_END?, bitId, PACKETSIZE

    ;wait for shifting edge
    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    ;wait for falling edge
    m_wait_high_pulse    1, SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    ;wait for rising edge
    m_wait_low_pulse    1, SCLK_PIN
    .endif

     qba            SEND_BIT_LOOP?
SEND_BIT_LOOP_END?:

    .endm


;************************************************************************************
;   Macro: m_transfer_packet_spi_slave_gpi_sclk
;
;   Description:
;       Reads 1 data packet and Writes 1 data packet concurrently through
;       the SPI slave interface
;
;   PEAK cycles: 13 cycles = 6 cycles (shifting half) + 7 cycles (sampling half)
;   Theoretical maximum SCLK freq at 3ns cycle time (50% duty cycle): <= 23.809 MHz
;   Practical limitation of 12.8MHz at 3ns PRU system cycle time.
;   Acceptable CS to SCLK start time:       >= 3 cycles
;
;   Pseudo code:
;       ========== C ==========
;       dataReg = 0;
;       bitId = PACKETSIZE-1;
;       for (bitId >= 0)
;           wait for SCLK_PIN shifting edge
;           send bit from s_dataReg
;           wait for SCLK_PIN sampling edge
;           read bit into r_dataReg
;           bitId --;
;       =======================
;
;   Parameters:
;           r_dataReg       -  Contains received data packet
;           s_dataReg       -  Contains data packet to send
;           PACKETSIZE      -  Size of data packet in bits (max 32 bits)
;           bitId           -  To track Bit index (8 bits wide)
;           SCLK_PIN        -  PRU Pin number for SCLK
;           SDI_PIN         -  PRU Pin number for SDI
;           SDO_PIN         -  PRU Pin number for SDO
;           MODE            -  MODE0, MODE1, MODE2 or MODE3 mode of SPI operation
;           S_BIT           -  MSB or LSB
;************************************************************************************
m_transfer_packet_spi_slave_gpi_sclk   .macro  r_dataReg, s_dataReg, PACKETSIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, MODE, S_BIT
    ; bitId is bit position pointer
    .if $symcmp(S_BIT,"MSB") = 0
    ldi     bitId, PACKETSIZE - 1
    .elseif $symcmp(S_BIT,"LSB") = 0
    ldi     bitId, 0
    .endif

    ;wait for shifting edge
    .if $symcmp(MODE,"MODE1") = 0
    ;wait for rising edge
    m_wait_low_pulse    1, SCLK_PIN
    .elseif $symcmp(MODE,"MODE3") = 0
    ;wait for falling edge
    m_wait_high_pulse    1, SCLK_PIN
    .endif
    ;shift data right away for mode0 and mode2 as SCLK signal gives no indication for shifting of initial bit

SEND_BIT_LOOP?:
    qbbc            data_low?, s_dataReg, bitId
    m_pru_set_pin   SDO_PIN
    qba             data_high?
data_low?:
    m_pru_clr_pin   SDO_PIN
    NOP
data_high?:

    ;wait for sampling edge
    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    ;wait for rising edge
    m_wait_low_pulse    1, SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    ;wait for falling edge
    m_wait_high_pulse    1, SCLK_PIN
    .endif

    ;sampling received bit
    qbbc    BIT_ENTRY_0?, r31, SDI_PIN
    set     r_dataReg, r_dataReg, bitId     ; store data in register
    qba     SKIP_BIT_ENTRY_0?
BIT_ENTRY_0?:
    clr     r_dataReg, r_dataReg, bitId
    NOP
SKIP_BIT_ENTRY_0?:
    .if $symcmp(S_BIT,"MSB") = 0
    sub             bitId, bitId, 1
    qbeq            SEND_BIT_LOOP_END?, bitId, 0xFF
    .elseif $symcmp(S_BIT,"LSB") = 0
    add             bitId, bitId, 1
    qbeq            SEND_BIT_LOOP_END?, bitId, PACKETSIZE
    .endif

    ;wait for shifting edge
    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    ;wait for falling edge
    m_wait_high_pulse    1, SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    ;wait for rising edge
    m_wait_low_pulse    1, SCLK_PIN
    .endif

    qba            SEND_BIT_LOOP?
SEND_BIT_LOOP_END?:

    .endm
