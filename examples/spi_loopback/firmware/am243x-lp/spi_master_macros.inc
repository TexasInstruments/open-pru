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
;   File:     spi_master_macros.inc
;
;   Brief:    SPI Master interface macros for all MODES
;
;   Includes:
;            icss_pin_macros.inc
;            time_macros.inc
;************************************************************************************

    .include "icss_pin_macros.inc"
    .include "time_macros.inc"

;  SPI Mode | CPOL | CPHA |  Idle State Clock Polarity | Clock Phase Used to Sample and/or Shift the Data
;     0     |  0   |  0   |       Logic low            | Data sampled on rising edge and shifted out on the falling edge
;     1     |  0   |  1   |       Logic low            | Data sampled on the falling edge and shifted out on the rising edge
;     2     |  1   |  0   |       Logic high           | Data sampled on the falling edge and shifted out on the rising edge
;     3     |  1   |  1   |       Logic high           | Data sampled on the rising edge and shifted out on the falling edge


;************************************************************************************
;   Macro: m_read_packet_spi_msb_gpo_sclk
;
;   Read 1 data packet through the SPI interface.
;
;   PEAK cycles:
;          First half   ~ (2 + d1) cycles
;          Second half  ~ (5 + d2) cycles
;          Total cycles ~ (7 + d1 + d2) cycles
;
;   Maximam SCLK freq (50% duty cycle) takes 10 cycles (d1 =3, d2 =0) and at 3ns cycle time: = 33.33 MHz
;
;   Pseudo code:
;       ========== C ==========
;       dataReg = 0
;       bitId = PACKETSIZE
;       loop:   PACKETSIZE
;           set/clr   SCLK_PIN
;           bitId --
;           wait  DELAY_COMPEN_1
;           clr/set   SCLK_PIN
;           read bit at bitId
;           wait  DELAY_COMPEN_2
;       =======================
;
;   Parameters:
;           dataReg         -  Contains data packet to send
;           PACKETSIZE      -  Size of data packet in bits
;           bitId           -  To track Bit index (8 bits wide)
;           SCLK_PIN        -  PRU Pin number for SCLK
;           SDI_PIN         -  PRU Pin number for SDI
;           DELAY_COMPEN_1  -  PRU cycles to wait for while SCLK is high
;           DELAY_COMPEN_2  -  PRU cycles to wait for while SCLK is low
;************************************************************************************
m_read_packet_spi_msb_gpo_sclk   .macro  dataReg, PACKETSIZE, bitId, SCLK_PIN, SDI_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, MODE
    ; clear dataReg which is used to store received data
    ldi     bitId, PACKETSIZE       ; bitId is bit position pointer
READ_BIT_LOOP?:
;    loop READ_COMPLETE, PACKETSIZE
    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    m_pru_clr_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    m_pru_set_pin   SCLK_PIN
    .endif

    sub     bitId, bitId, 1
    .loop   DELAY_COMPEN_1 ; 3 for 50% duty cycle at max SCLK freq
    NOP
    .endloop

    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    m_pru_set_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    m_pru_clr_pin   SCLK_PIN
    .endif

    qbbc    BIT_ENTRY_0?, r31, SDI_PIN
    set     dataReg, dataReg, bitId     ; store data in register
    qba     SKIP_BIT_ENTRY_0?
BIT_ENTRY_0?:
    clr     dataReg, dataReg, bitId
    nop
SKIP_BIT_ENTRY_0?:
    .loop   DELAY_COMPEN_2 ; 0
    NOP
    .endloop
    qbne    READ_BIT_LOOP?, bitId, 0
;READ_COMPLETE:
    .if $symcmp(MODE,"MODE0") = 0
    m_pru_clr_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE2") = 0
    m_pru_set_pin   SCLK_PIN
    .endif

    .endm


;************************************************************************************
;   Macro: m_read_packet_spi_lsb_gpo_sclk
;
;   Read 1 data packet through the SPI interface lsb first
;
;   PEAK cycles:
;          First half   ~ (2 + d1) cycles
;          Second half  ~ (5 + d2) cycles
;          Total cycles ~ (7 + d1 + d2) cycles
;
;   Pseudo code:
;       ========== C ==========
;       dataReg = 0
;       bitId = 0
;       loop:   PACKETSIZE
;           set/clr   SCLK_PIN
;           bitId ++
;           wait  DELAY_COMPEN_1
;           clr/set   SCLK_PIN
;           read bit at bitId
;           wait  DELAY_COMPEN_2
;       =======================
;
;   Parameters:
;           dataReg         -  Contains data packet to send
;           PACKETSIZE      -  Size of data packet in bits
;           bitId           -  To track Bit index (8 bits wide)
;           SCLK_PIN        -  PRU Pin number for SCLK
;           SDI_PIN         -  PRU Pin number for SDI
;           DELAY_COMPEN_1  -  PRU cycles to wait for while SCLK is high
;           DELAY_COMPEN_2  -  PRU cycles to wait for while SCLK is low
;************************************************************************************
m_read_packet_spi_lsb_gpo_sclk   .macro  dataReg, PACKETSIZE, bitId, SCLK_PIN, SDI_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, MODE
    ; clear dataReg which is used to store received data
    ldi     bitId, 255       ; bitId is bit position pointer
READ_BIT_LOOP?:
    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    m_pru_clr_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    m_pru_set_pin   SCLK_PIN
    .endif

    add     bitId, bitId, 1
    ; m_wait_cycles   7
    .loop   DELAY_COMPEN_1 ; 6
    NOP
    .endloop

    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    m_pru_set_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    m_pru_clr_pin   SCLK_PIN
    .endif

    qbbc    BIT_ENTRY_0?, r31, SDI_PIN
    set     dataReg, dataReg, bitId     ; store data in register
    qba     SKIP_BIT_ENTRY_0?
BIT_ENTRY_0?:
    clr     dataReg, dataReg, bitId
    nop
SKIP_BIT_ENTRY_0?:
    .loop   DELAY_COMPEN_2 ; 0
    NOP
    .endloop
    qbne    READ_BIT_LOOP?, bitId, PACKETSIZE-1

    .if $symcmp(MODE,"MODE0") = 0
    m_pru_clr_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE2") = 0
    m_pru_set_pin   SCLK_PIN
    .endif

    .endm


;************************************************************************************
;   Macro: m_send_packet_spi_msb_gpo_sclk
;
;   Send 1 data packet through the SPI interface.
;
;   PEAK cycles:
;          First half   ~ (4 + d1) cycles
;          Second half  ~ (3 + d2) cycles
;          Total cycles ~ (7 + d1 + d2) cycles
;
;   Pseudo code:
;       ========== C ==========
;       dataReg = 0
;       bitId = PACKETSIZE
;       loop:   PACKETSIZE
;           set/clr   SCLK_PIN
;           send SDO bit
;           wait  DELAY_COMPEN_1
;           clr/set   SCLK_PIN
;           wait  DELAY_COMPEN_2
;           bitId --
;       =======================
;
;   Parameters:
;           dataReg         -  Contains data packet to send
;           PACKETSIZE      -  Size of data packet in bits
;           bitId           -  To track Bit index (8 bits wide)
;           SCLK_PIN        -  PRU Pin number for SCLK
;           SDO_PIN         -  PRU Pin number for SDO
;           DELAY_COMPEN_1  -  PRU cycles to wait for while SCLK is high
;           DELAY_COMPEN_2  -  PRU cycles to wait for while SCLK is low
;************************************************************************************
m_send_packet_spi_msb_gpo_sclk   .macro  dataReg, PACKETSIZE, bitId, SCLK_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, MODE
    ldi     bitId, PACKETSIZE - 1   ; bitId is bit position pointer
    ; sub             bitId, bitId, 1 ; If using register for PACKETSIZE
    ; loop  SEND_BIT_LOOP_END, PACKETSIZE

SEND_BIT_LOOP?:

    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    m_pru_clr_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    m_pru_set_pin   SCLK_PIN
    .endif

    qbbc            data_low?, dataReg, bitId
    m_pru_set_pin   SDO_PIN
    qba             data_high?
data_low?:
    m_pru_clr_pin   SDO_PIN
    NOP
data_high?:
    .loop   DELAY_COMPEN_1 ; 0
    NOP
    .endloop

    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    m_pru_set_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    m_pru_clr_pin   SCLK_PIN
    .endif

    .loop   DELAY_COMPEN_2 ; 1
    NOP
    .endloop
    sub             bitId, bitId, 1
    qbne            SEND_BIT_LOOP?, bitId, 0xFF
    ; SEND_BIT_LOOP_END:

    .if $symcmp(MODE,"MODE0") = 0
    m_pru_clr_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE2") = 0
    m_pru_set_pin   SCLK_PIN
    .endif

    .endm

;************************************************************************************
;   Macro: m_send_packet_spi_lsb_gpo_sclk
;
;   Send 1 data packet through the SPI interface LSB first
;
;   PEAK cycles:
;          First half   ~ (4 + d1) cycles
;          Second half  ~ (3 + d2) cycles
;          Total cycles ~ (7 + d1 + d2) cycles
;
;   Pseudo code:
;       ========== C ==========
;       dataReg = 0
;       bitId = 0
;       loop:   PACKETSIZE
;           set/clr   SCLK_PIN
;           send SDO bit
;           wait  DELAY_COMPEN_1
;           clr/set   SCLK_PIN
;           wait  DELAY_COMPEN_2
;           bitId ++
;       =======================
;
;   Parameters:
;           dataReg         -  Contains data packet to send
;           PACKETSIZE      -  Size of data packet in bits
;           bitId           -  To track Bit index (8 bits wide)
;           SCLK_PIN        -  PRU Pin number for SCLK
;           SDO_PIN         -  PRU Pin number for SDO
;           DELAY_COMPEN_1  -  PRU cycles to wait for while SCLK is high
;           DELAY_COMPEN_2  -  PRU cycles to wait for while SCLK is low
;************************************************************************************
m_send_packet_spi_lsb_gpo_sclk   .macro  dataReg, PACKETSIZE, bitId, SCLK_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, MODE
    ldi     bitId, 0   ; bitId is bit position pointer
    ; sub             bitId, bitId, 1 ; If using register for PACKETSIZE
    ; loop  SEND_BIT_LOOP_END, PACKETSIZE

SEND_BIT_LOOP?:

    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    m_pru_clr_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    m_pru_set_pin   SCLK_PIN
    .endif

    qbbc            data_low?, dataReg, bitId
    m_pru_set_pin   SDO_PIN
    qba             data_high?
data_low?:
    m_pru_clr_pin   SDO_PIN
    NOP
data_high?:
    .loop   DELAY_COMPEN_1 ; 0
    NOP
    .endloop

    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    m_pru_set_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    m_pru_clr_pin   SCLK_PIN
    .endif

    .loop   DELAY_COMPEN_2 ; 1
    NOP
    .endloop
    add             bitId, bitId, 1
    qbne            SEND_BIT_LOOP?, bitId, PACKETSIZE
    ; SEND_BIT_LOOP_END:

    .if $symcmp(MODE,"MODE0") = 0
    m_pru_clr_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE2") = 0
    m_pru_set_pin   SCLK_PIN
    .endif

    .endm

;************************************************************************************
;   Macro: m_transfer_packet_spi_master_gpo_sclk
;
;   Description:
;       - Reads 1 data packet and Writes 1 data packet concurrently through
;         the SPI slave interface. SPI master initiates SCLK.
;       - CS is to be pulled down before calling this macro.
;       - Theoretical max frequency at 333MHz PRU clock is 27.77MHz (d1=2, d2=0)
;       - Practical limitation upto 12.8 MHz SCLK (d1=9, d2=7) in combination with
;         m_transfer_packet_spi_slave_gpi_sclk macro
;
;   PEAK cycles:
;           ~ (4+d1) + (6+d2) cycles
;
;
;   Pseudo code:
;       ========== C ==========
;       r_dataReg = 0
;       bitId = PACKETSIZE - 1 or 0 //depending on LSB or MSB config
;       loop:   READ_PACKET
;           set SCLK shifting edge
;           set SDO bit
;           wait d1 cycles
;           set SCLK sampling edge
;           read SDI bit
;           wait d2 cycles
;       =======================
;
;   Parameters:
;           s_dataReg       -  Contains data packet to send
;           r_dataReg       -  Register where received data packet is stored
;           PACKETSIZE      -  Size of data packet in bits
;           bitId           -  To track Bit index (8 bits wide)
;           SCLK_PIN        -  PRU Pin number for SCLK
;           SDO_PIN         -  PRU Pin number for SDO
;           SDI_PIN         -  PRU Pin number for SDI
;           DELAY_COMPEN_1  -  PRU cycles to wait for while SCLK is high
;           DELAY_COMPEN_2  -  PRU cycles to wait for while SCLK is low
;           MODE            -  MODE0, MODE1, MODE2 or MODE3 mode of SPI operation
;           S_BIT           -  MSB or LSB
;************************************************************************************
m_transfer_packet_spi_master_gpo_sclk   .macro  r_dataReg, s_dataReg, PACKETSIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, MODE, S_BIT
    ; clear dataReg which is used to store received data
    .if $symcmp(S_BIT,"MSB") = 0
    ldi     bitId, PACKETSIZE - 1
    .elseif $symcmp(S_BIT,"LSB") = 0
    ldi     bitId, 0
    .endif
READ_BIT_LOOP?:
    ;shifting edge
    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    m_pru_clr_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    m_pru_set_pin   SCLK_PIN
    .endif

    qbbc            data_low?, s_dataReg, bitId
    m_pru_set_pin   SDO_PIN
    qba             data_high?
data_low?:
    m_pru_clr_pin   SDO_PIN
    NOP
data_high?:
    .loop   DELAY_COMPEN_1 ;
    NOP
    .endloop

    ;sampling edge
    .if $symcmp(MODE,"MODE0") = 0 | $symcmp(MODE,"MODE3") = 0
    m_pru_set_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE1") = 0 | $symcmp(MODE,"MODE2") = 0
    m_pru_clr_pin   SCLK_PIN
    .endif

    qbbc    BIT_ENTRY_0?, r31, SDI_PIN
    set     r_dataReg, r_dataReg, bitId     ; store data in register
    qba     SKIP_BIT_ENTRY_0?
BIT_ENTRY_0?:
    clr     r_dataReg, r_dataReg, bitId
    nop
SKIP_BIT_ENTRY_0?:
    .loop   DELAY_COMPEN_2 ; 0
    NOP
    .endloop

    .if $symcmp(S_BIT,"MSB") = 0
    sub             bitId, bitId, 1
    qbne            READ_BIT_LOOP?, bitId, 0xFF
    .elseif $symcmp(S_BIT,"LSB") = 0
    add             bitId, bitId, 1
    qbne            READ_BIT_LOOP?, bitId, PACKETSIZE
    .endif

    .if $symcmp(MODE,"MODE0") = 0
    m_pru_clr_pin   SCLK_PIN
    .elseif $symcmp(MODE,"MODE2") = 0
    m_pru_set_pin   SCLK_PIN
    .endif

    .endm
