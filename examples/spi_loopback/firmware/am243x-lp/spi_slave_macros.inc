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
;   File:     spi_slave_macros.inc
;
;   Brief:    SPI Slave interface macros for all MODES. This file provides macros
;             for SPI data transfer, including reading, writing, and concurrent
;             read/write operations in MSB and LSB configurations.
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
;   Assumptions:
;       - CS must be pulled low before calling this macro.
;       - The SCLK_PIN and SDI_PIN must be configured correctly.
;       - dataReg should be cleared before calling this macro
;
;   Pseudo code:
;       ========== C ==========
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
;
;   Returns:
;       - The received data packet is stored in the `dataReg` register.
;************************************************************************************
m_read_packet_spi_slave_msb_gpi_sclk   .macro  dataReg, PACKETSIZE, bitId, SCLK_PIN, SDI_PIN, MODE
    ; Validating packetsize parameter
    .if (PACKETSIZE > 32)  |  (PACKETSIZE < 1)
    .emsg "Error: Invalid parameter used in macro: PACKETSIZE"
    .endif
    ldi     bitId, PACKETSIZE       ; bitId is bit position pointer

READ_BIT_LOOP?:
    ;decrementing pointer
    sub     bitId, bitId, 1

    ;wait for sampling edge
    m_handle_spi_slave_mode "SAMPLING_EDGE", MODE, SCLK_PIN

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
;   Assumptions:
;       - CS must be pulled low before calling this macro.
;       - The SCLK_PIN and SDI_PIN must be configured correctly.
;       - dataReg should be cleared before calling this macro
;
;   Pseudo code:
;       ========== C ==========
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
;
;   Returns:
;       - The received data packet is stored in the `dataReg` register.
;************************************************************************************
m_read_packet_spi_slave_lsb_gpo_sclk   .macro  dataReg, PACKETSIZE, bitId, SCLK_PIN, SDI_PIN, MODE
    ; Validating packetsize parameter
    .if (PACKETSIZE > 32)  |  (PACKETSIZE < 1)
    .emsg "Error: Invalid parameter used in macro: PACKETSIZE"
    .endif
    ldi     bitId, 255       ; bitId is bit position pointer

READ_BIT_LOOP?:
    ;incrementing pointer
    add     bitId, bitId, 1

    ;wait for sampling edge
    m_handle_spi_slave_mode "SAMPLING_EDGE", MODE, SCLK_PIN

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
;   Assumptions:
;       - CS must be pulled low before calling this macro.
;       - The SCLK_PIN and SDO_PIN must be configured correctly.
;       - PACKETSIZE should not be more than 32 (bits) or less than 1
;
;   Pseudo code:
;       ========== C ==========
;       bitId = PACKETSIZE - 1
;       wait for SCLK_PIN shifting edge only for MODE1 and MODE3
;       send dataReg bit
;       while (bitId !=0)
;           bitId --;
;           wait for SCLK_PIN sampling edge
;           send dataReg bit
;       =======================
;
;   Parameters:
;           dataReg         -  Contains data packet to send
;           PACKETSIZE      -  Size of data packet in bits
;           bitId           -  To track Bit index (8 bits wide)
;           SCLK_PIN        -  PRU Pin number for SCLK
;           SDO_PIN         -  PRU Pin number for SDO
;
;   Returns:
;       - No return value
;************************************************************************************
m_send_packet_spi_slave_msb_gpo_sclk   .macro  dataReg, PACKETSIZE, bitId, SCLK_PIN, SDO_PIN, MODE
    ; Validating packetsize parameter
    .if (PACKETSIZE > 32)  |  (PACKETSIZE < 1)
    .emsg "Error: Invalid parameter used in macro: PACKETSIZE"
    .endif
    ldi     bitId, PACKETSIZE - 1   ; bitId is bit position pointer

    ;wait for beginning shifting edge for only mode1 and mode3
    m_handle_spi_slave_mode "BEGINNING", MODE, SCLK_PIN

SEND_BIT_LOOP?:
    qbbc    data_low?, dataReg, bitId
    m_pru_set_pin   SDO_PIN
    qba     data_high?
data_low?:
    m_pru_clr_pin   SDO_PIN
    NOP
data_high?:
    qbeq    SEND_BIT_LOOP_END?, bitId, 0
    sub     bitId, bitId, 1

    ;wait for shifting edge
    m_handle_spi_slave_mode "SHIFTING_EDGE", MODE, SCLK_PIN

    qbbc    data_low?, dataReg, bitId
    m_pru_set_pin   SDO_PIN
    qba     data_high?
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
;   Assumptions:
;       - CS must be pulled low before calling this macro.
;       - The SCLK_PIN and SDO_PIN must be configured correctly.
;       - PACKETSIZE should not be more than 32 (bits) or less than 1
;
;   Pseudo code:
;       ========== C ==========
;       bitId = 0
;       wait for SCLK_PIN shifting edge only for MODE1 and MODE3
;       send dataReg bit
;       while (bitId < PACKETSIZE)
;           bitId ++;
;           wait for SCLK_PIN sampling edge
;           send dataReg bit
;       =======================
;
;   Parameters:
;           dataReg         -  Contains data packet to send
;           PACKETSIZE      -  Size of data packet in bits
;           bitId           -  To track Bit index (8 bits wide)
;           SCLK_PIN        -  PRU Pin number for SCLK
;           SDO_PIN         -  PRU Pin number for SDO
;
;   Returns:
;       - No return value
;************************************************************************************
m_send_packet_spi_slave_lsb_gpo_sclk   .macro  dataReg, PACKETSIZE, bitId, SCLK_PIN, SDO_PIN, MODE
    ; Validating packetsize parameter
    .if (PACKETSIZE > 32)  |  (PACKETSIZE < 1)
    .emsg "Error: Invalid parameter used in macro: PACKETSIZE"
    .endif
    ldi     bitId, 0   ; bitId is bit position pointer

    ;wait for beginning shifting edge for only mode1 and mode3
    m_handle_spi_slave_mode "BEGINNING", MODE, SCLK_PIN

SEND_BIT_LOOP?:

    qbbc    data_low?, dataReg, bitId
    m_pru_set_pin   SDO_PIN
    qba     data_high?
data_low?:
    m_pru_clr_pin   SDO_PIN
    NOP
data_high?:
    add     bitId, bitId, 1
    qbeq    SEND_BIT_LOOP_END?, bitId, PACKETSIZE

    ;wait for shifting edge
    m_handle_spi_slave_mode "SHIFTING_EDGE", MODE, SCLK_PIN

     qba    SEND_BIT_LOOP?
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
;   Assumptions:
;       - CS must be pulled low before calling this macro.
;       - The SCLK_PIN, SDI_PIN and SDO_PIN must be configured correctly.
;       - PACKETSIZE should not be more than 32 (bits) or less than 1
;       - r_dataReg should be cleared before calling this macro
;
;   Pseudo code:
;       ========== C ==========
;       if(S_BIT =="MSB")
;           bitId = PACKETSIZE-1;
;       else if(S_BIT =="LSB")
;           bitId = 0;
;       else 
;           "error message"
;       wait for SCLK_PIN shifting edge only for MODE1 and MODE3
;       loop PACKETSIZE:
;           send bit from s_dataReg
;           wait for SCLK_PIN sampling edge
;           read bit into r_dataReg 
;           if(S_BIT =="MSB")
;               bitId --
;           elseif(S_BIT =="LSB")
;               bitID ++
;           wait for SCLK_PIN shifting edge
;       =======================
;
;   Parameters:
;           r_dataReg       -  Received data is stored in this register
;           s_dataReg       -  Contains data packet to send
;           PACKETSIZE      -  Size of data packet in bits (max 32 bits)
;           bitId           -  To track Bit index (8 bits wide)
;           SCLK_PIN        -  PRU Pin number for SCLK
;           SDI_PIN         -  PRU Pin number for SDI
;           SDO_PIN         -  PRU Pin number for SDO
;           MODE            -  MODE0, MODE1, MODE2 or MODE3 mode of SPI operation
;           S_BIT           -  MSB or LSB
;
;   Returns:
;       - The received data packet is stored in the `r_dataReg` register.
;************************************************************************************
m_transfer_packet_spi_slave_gpi_sclk   .macro  r_dataReg, s_dataReg, PACKETSIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, MODE, S_BIT
    ; Validating packetsize parameter
    .if (PACKETSIZE > 32)  |  (PACKETSIZE < 1)
    .emsg "Error: Invalid parameter used in macro: PACKETSIZE"
    .endif
    .if $symcmp(S_BIT,"MSB") = 0
    ldi     bitId, PACKETSIZE - 1
    .elseif $symcmp(S_BIT,"LSB") = 0
    ldi     bitId, 0
    .else
    .emsg "Error: Invalid parameter used in macro"
    .endif

    ;wait for beginning shifting edge for only mode1 and mode3
    m_handle_spi_slave_mode "BEGINNING", MODE, SCLK_PIN

SEND_BIT_LOOP?:
    qbbc    data_low?, s_dataReg, bitId
    m_pru_set_pin   SDO_PIN
    qba     data_high?
data_low?:
    m_pru_clr_pin   SDO_PIN
    NOP
data_high?:

    ;wait for sampling edge
    m_handle_spi_slave_mode "SAMPLING_EDGE", MODE, SCLK_PIN

    ;sampling received bit
    qbbc    BIT_ENTRY_0?, r31, SDI_PIN
    set     r_dataReg, r_dataReg, bitId     ; store data in register
    qba     SKIP_BIT_ENTRY_0?
BIT_ENTRY_0?:
    clr     r_dataReg, r_dataReg, bitId
    NOP
SKIP_BIT_ENTRY_0?:
    .if $symcmp(S_BIT,"MSB") = 0
    sub     bitId, bitId, 1
    qbeq    SEND_BIT_LOOP_END?, bitId, 0xFF
    .elseif $symcmp(S_BIT,"LSB") = 0
    add     bitId, bitId, 1
    qbeq    SEND_BIT_LOOP_END?, bitId, PACKETSIZE
    .endif

    ;wait for shifting edge
    m_handle_spi_slave_mode "SHIFTING_EDGE", MODE, SCLK_PIN

    qba     SEND_BIT_LOOP?
SEND_BIT_LOOP_END?:

    .endm

;************************************************************************************
;   Macro: m_handle_spi_slave_mode
;
;   Abstraction macro that handles the waiting for sampling or shifting edge of SCLK 
;   pin based on SPI MODE using assembler directives.
;
;   PEAK cycles: same as m_wait_high_pulse or m_wait_low_pulse macros
;
;   Assumptions:
;       - Only meant to be called from one of the spi master macros.
;       - The SCLK_PIN must be configured correctly.
;
;   Pseudo code:
;       ========== C ==========
;       #if EDGE == SHIFTING_EDGE
;       #if (MODE == MODE0) | (MODE == MODE3)
;       wait for falling edge of SCLK_PIN
;       #elif (MODE == MODE1) | (MODE == MODE2)
;       wait for rising edge of SCLK_PIN
;       #else
;       Set Error msg for MODE
;       #endif
;
;       #elif EDGE == SAMPLING_EDGE
;       #if (MODE == MODE0) | (MODE == MODE3)
;       wait for rising edge of SCLK_PIN
;       #elif (MODE == MODE1) | (MODE == MODE2)
;       wait for falling edge of SCLK_PIN
;       #else
;       Set Error msg for MODE
;       #endif
;
;       #elif EDGE == BEGINNING
;       #if MODE == MODE1
;       wait for rising edge of SCLK_PIN
;       #elif MODE == MODE3
;       wait for falling edge of SCLK_PIN
;       #endif
;
;       #else
;       Set Error msg for EDGE
;       #endif
;       =======================
;
;   Parameters:
;           EDGE       -  Mentions which edge to wait for.
;                            -  SHIFTING_EDGE - waits for shifting edge
;                            -  SAMPLING_EDGE - waits for sampling edge
;                            -  BEGINNING     - Waits for shifting edge only for 
;                                                 mode1 or mode3 needed at the beginning
;           MODE       -  Determines SPI MODE of operation
;           SCLK_PIN   -  PRU Pin number for SCLK
;
;   Returns:
;       - No return value
;************************************************************************************
m_handle_spi_slave_mode .macro EDGE, MODE, SCLK_PIN
    ;shifting edge
    .if $symcmp(EDGE,"SHIFTING_EDGE") = 0
    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    ;wait for falling edge
    m_wait_high_pulse    1, SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    ;wait for rising edge
    m_wait_low_pulse    1, SCLK_PIN
    .else
    .emsg "Error: Invalid parameter used in macro: MODE"
    .endif
    
    ;Sampling edge
    .elseif $symcmp(EDGE,"SAMPLING_EDGE") = 0
    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    ;wait for rising edge
    m_wait_low_pulse    1, SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    ;wait for falling edge
    m_wait_high_pulse    1, SCLK_PIN
    .else
    .emsg "Error: Invalid parameter used in macro: MODE"
    .endif

    ;wait for first shifting edge at beginning only for mode1 and mode3
    .elseif $symcmp(EDGE,"BEGINNING") = 0
    .if $symcmp(MODE,"MODE1") = 0
    ;wait for rising edge
    m_wait_low_pulse    1, SCLK_PIN
    .elseif $symcmp(MODE,"MODE3") = 0
    ;wait for falling edge
    m_wait_high_pulse    1, SCLK_PIN
    .endif
    ;shift data right away for mode0 and mode2 as SCLK signal gives no indication for shifting of initial bit

    ;invalid edge
    .else
    .emsg "Error: Invalid parameter used in macro: EDGE"
    .endif

    .endm