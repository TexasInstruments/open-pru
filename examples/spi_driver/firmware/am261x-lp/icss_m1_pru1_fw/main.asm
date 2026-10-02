; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2025 Texas Instruments Incorporated - http://www.ti.com/

;************************************************************************************
;   File:     main.asm
;
;   Brief:    PRU1 SPI Slave firmware for AM261x — encoder-emulation responder
;             (phase 3, MODE3/MSB only for now). Loops forever: waits for CS
;             low, then runs 5 independent full-duplex transfers back-to-back
;             within one CS-low burst via m_transfer_packet_spi_slave_gpi_sclk
;             (MODE3, MSB first, 16-bit each). Each transfer sends one of 5
;             fixed response words (0x1111/0x2222/0x3333/0x4444/0x5555) while
;             receiving one of the master's 5 command words; the 5 received
;             words are latched to DMEM1 after the burst for R5F visibility.
;             No decode logic — responses are fixed regardless of command
;             content. Never halts. (Older 4-command/1-response decode path
;             below main loop is now dead code, kept for reference.)
;
;   Pin mapping (PRU1 GPO/GPI numbers):
;       SCLK = GPI4  (r31.4,  input  — driven by master)
;       CS   = GPI6  (r31.6,  input  — driven by master, active low)
;       SDO  = GPO5  (r30.5,  output — slave transmits)
;       SDI  = GPI11 (r31.11, input  — slave receives)
;************************************************************************************

    .include "spi_slave_macros.inc"

; CCS/makefile specific settings
    .retain
    .retainrefs

    .global     main
    .sect       ".text:main"

; PRU clock = 225 MHz on AM261x -> 4.44ns per cycle, use 5 (ceil) for safe timing
    .asg    5,  PRU_CLK_CYC_PRD_CONST

; Pin assignments
SCLK_PIN            .set    4
CS_PIN              .set    6
SDO_PIN             .set    5
SDI_PIN             .set    11

PACKET_SIZE         .set    16

; The 4 known commands PRU0 may send, and PRU1's fixed response to each.
; Fixed at build time (not DMEM-configurable) per phase 2 design decision.
CMD0                .set    0x1
CMD1                .set    0x2
CMD2                .set    0x3
CMD3                .set    0x4
RESP0               .set    0x1111
RESP1               .set    0x2222
RESP2               .set    0x3333
RESP3               .set    0x4444
RESP_DEFAULT        .set    0xABCD  ; sent on the very first transaction (nothing decoded yet)

; Fixed responses for the 5x16-bit MODE3/MSB burst (independent of the 4
; single-transfer CMD/RESP pairs above, which are now unused/dead code).
RESP5_0             .set    0x1111
RESP5_1             .set    0x2222
RESP5_2             .set    0x3333
RESP5_3             .set    0x4444
RESP5_4             .set    0x5555

; PRU1 DMEM1 offsets (base 0x48602000)
DMEM_SLAVE_RX5_0    .set    0x00
DMEM_SLAVE_RX5_1    .set    0x04
DMEM_SLAVE_RX5_2    .set    0x08
DMEM_SLAVE_RX5_3    .set    0x0C
DMEM_SLAVE_RX5_4    .set    0x10

; Register aliases
    .asg    R2,     s_dataReg
    .asg    R3,     r_dataReg
    .asg    R4.b0,  bitId
    .asg    R5,     temp
    .asg    R6,     retAddr

; 5x16-bit tx/rx registers for the MODE3/MSB 5-transfer burst
    .asg    R10,    tx0
    .asg    R11,    tx1
    .asg    R12,    tx2
    .asg    R13,    tx3
    .asg    R14,    tx4
    .asg    R15,    rx0
    .asg    R16,    rx1
    .asg    R17,    rx2
    .asg    R18,    rx3
    .asg    R19,    rx4
;********
;* MAIN *
;********

main:
    zero    &r0, 120

    ; First transaction has nothing decoded yet — caller should disregard
    ; the response read back from the very first transaction.
    ldi32   s_dataReg, RESP_DEFAULT

LOOP_BACK:
    ; Load the 5 fixed responses to send, and clear the 5 rx registers,
    ; before each 5-transfer burst.
    ldi32   tx0, RESP5_0
    ldi32   tx1, RESP5_1
    ldi32   tx2, RESP5_2
    ldi32   tx3, RESP5_3
    ldi32   tx4, RESP5_4
    ldi32   rx0, 0x00000000
    ldi32   rx1, 0x00000000
    ldi32   rx2, 0x00000000
    ldi32   rx3, 0x00000000
    ldi32   rx4, 0x00000000

WAIT_CS_LOW:
    ; Poll until CS goes low (master asserts)
    qbbs    WAIT_CS_LOW, r31, CS_PIN

    ; Matches PRU0's DO_5X_TRANSFER_M3_LSB: master keeps CS low across 5
    ; back-to-back transfers. Each transfer sends its own fixed response
    ; while receiving the command word into its own register - not decoded,
    ; just latched to DMEM1 afterward for R5F/debug visibility.
    jal     retAddr, DO_5X_TRANSFER

    ldi32   temp, 0x00000000
    sbbo    &rx0, temp, DMEM_SLAVE_RX5_0, 4
    sbbo    &rx1, temp, DMEM_SLAVE_RX5_1, 4
    sbbo    &rx2, temp, DMEM_SLAVE_RX5_2, 4
    sbbo    &rx3, temp, DMEM_SLAVE_RX5_3, 4
    sbbo    &rx4, temp, DMEM_SLAVE_RX5_4, 4
    qba     LOOP_BACK

SET_R0:
    ldi32   s_dataReg, RESP0
    qba     LOOP_BACK
SET_R1:
    ldi32   s_dataReg, RESP1
    qba     LOOP_BACK
SET_R2:
    ldi32   s_dataReg, RESP2
    qba     LOOP_BACK
SET_R3:
    ldi32   s_dataReg, RESP3
    qba     LOOP_BACK

; Out-of-line subroutine: 5 independent full-duplex transfers within one
; CS-low burst, MODE3/MSB, 16-bit each, matching PRU0's DO_5X_TRANSFER_M3_MSB.
; Reached via jal instead of inlined in LOOP_BACK for the same qba
; 10-bit-offset reason as PRU0. tx0..4 must already hold the 5 fixed
; responses; rx0..4 receive the 5 command words independently.
DO_5X_TRANSFER:
    m_transfer_packet_spi_slave_gpi_sclk rx0, tx0, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, "MODE3", "MSB"
    m_transfer_packet_spi_slave_gpi_sclk rx1, tx1, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, "MODE3", "MSB"
    m_transfer_packet_spi_slave_gpi_sclk rx2, tx2, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, "MODE3", "MSB"
    m_transfer_packet_spi_slave_gpi_sclk rx3, tx3, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, "MODE3", "MSB"
    m_transfer_packet_spi_slave_gpi_sclk rx4, tx4, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, "MODE3", "MSB"
    jmp     retAddr
