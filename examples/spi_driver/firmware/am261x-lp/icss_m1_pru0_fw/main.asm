; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2025 Texas Instruments Incorporated - http://www.ti.com/

;************************************************************************************
;   File:     main.asm
;
;   Brief:    PRU0 SPI Master firmware for AM261x.
;             Reads SPI mode/bit order from PRU0 DMEM0 once at startup and
;             selects one of an 8-way branch table (MODE0-3 x MSB/LSB) since
;             the underlying macro resolves MODE/S_BIT at assemble time
;             (.if $symcmp) and cannot take a runtime register value. Then
;             enters a persistent loop: wait for cfg_trigger, run transfer(s),
;             store rx, clear cfg_trigger, repeat. Never halts.
;
;             Phase 3, all 8 (MODE x bit-order) paths: on trigger, loads 5
;             independent 16-bit command words from DMEM and runs 5
;             back-to-back full-duplex transfers within one CS-low burst (no
;             inter-transfer delay), storing all 5 received words to DMEM
;             after the burst.
;
;   Pin mapping (PRU0 r30/r31 bit numbers):
;       SCLK = GPO0 (r30.0, output)
;       SDO  = GPO1 (r30.1, output)
;       CS   = GPO2 (r30.2, output, active low)
;       SDI  = GPI6 (r31.6, input)
;
;   PRU0 DMEM0 config/data (R5F uses global address 0x48600000; PRU0 itself
;   uses c24, the constant-table entry for its own DMEM):
;       0x00        mode         0=MODE0, 1=MODE1, 2=MODE2, 3=MODE3 (read once at startup)
;       0x04        bitOrder     0=MSB first, 1=LSB first (read once at startup)
;       0x10        cfg_trigger  R5F sets to 1 to start a transaction; PRU0
;                                 clears to 0 when the 5 transfers complete
;       0x14-0x24   cfg_cmd0..4     R5F writes 5 command words here before
;                                    setting cfg_trigger
;       0x28-0x38   master_rx5_0..4 PRU0 writes 5 received words here after
;                                    the 5-transfer burst completes
;************************************************************************************

    .include "spi_master_macros.inc"

    .retain
    .retainrefs

    .global     main
    .sect       ".text:main"

; PRU clock = 225 MHz on AM261x -> 4.44ns per cycle, use 5 (ceil) for safe timing
    .asg    5,      PRU_CLK_CYC_PRD_CONST

; Pin assignments
SCLK_PIN            .set    0
SDO_PIN             .set    1
CS_PIN              .set    2
SDI_PIN             .set    6

; Fixed for this driver's use case
PACKET_SIZE         .set    16
DELAY_COMPEN_1      .set    8
DELAY_COMPEN_2      .set    27   ; d1+d2=35 -> (10+35)*4.44ns = ~200ns period -> ~5 MHz SCLK

; PRU0 DMEM0 offsets (base 0x48600000)
DMEM_CFG_MODE       .set    0x00
DMEM_CFG_BITORDER   .set    0x04
DMEM_CFG_TRIGGER    .set    0x10

; 5x16-bit command/response DMEM slots, all MODE x bit-order combinations
DMEM_CFG_CMD0       .set    0x14
DMEM_CFG_CMD1       .set    0x18
DMEM_CFG_CMD2       .set    0x1C
DMEM_CFG_CMD3       .set    0x20
DMEM_CFG_CMD4       .set    0x24
DMEM_MASTER_RX5_0   .set    0x28
DMEM_MASTER_RX5_1   .set    0x2C
DMEM_MASTER_RX5_2   .set    0x30
DMEM_MASTER_RX5_3   .set    0x34
DMEM_MASTER_RX5_4   .set    0x38

    .asg    R4.b0,  bitId
    .asg    R6,     cfgMode
    .asg    R7,     cfgBitOrder
    .asg    R8,     cfgTrigger
    .asg    R9,     retAddr

; 5x16-bit tx/rx registers for the MODE3/LSB 5-transfer burst
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

    ; Read runtime mode/bitOrder from PRU0's own DMEM0 once at startup, via the
    ; constant table (c24 = own DMEM, default block index -> local 0x0000).
    ; cfg_command/cfg_trigger are re-read every loop iteration below instead,
    ; since they change per transaction.
    lbco    &cfgMode,     c24, DMEM_CFG_MODE,     4
    lbco    &cfgBitOrder, c24, DMEM_CFG_BITORDER, 4

    ; Branch to the (MODE x BITORDER) variant matching the runtime config
    qbeq    M0_MSB, cfgMode, 0
    qbeq    M1_MSB, cfgMode, 1
    qbeq    M2_MSB, cfgMode, 2
    qbeq    M3_MSB, cfgMode, 3
    qba     M3_MSB              ; default: MODE3

M0_MSB:
    qbeq    DO_M0_LSB, cfgBitOrder, 1
    qba     DO_M0_MSB
M1_MSB:
    qbeq    DO_M1_LSB, cfgBitOrder, 1
    qba     DO_M1_MSB
M2_MSB:
    qbeq    DO_M2_LSB, cfgBitOrder, 1
    qba     DO_M2_MSB
M3_MSB:
    qbeq    DO_M3_LSB, cfgBitOrder, 1
    qba     DO_M3_MSB

DO_M0_MSB:
    lbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qbbc    DO_M0_MSB, cfgTrigger, 0    ; wait for R5F to set cfg_trigger
    lbco    &tx0, c24, DMEM_CFG_CMD0, 4
    lbco    &tx1, c24, DMEM_CFG_CMD1, 4
    lbco    &tx2, c24, DMEM_CFG_CMD2, 4
    lbco    &tx3, c24, DMEM_CFG_CMD3, 4
    lbco    &tx4, c24, DMEM_CFG_CMD4, 4
    ldi32   rx0, 0x00000000
    ldi32   rx1, 0x00000000
    ldi32   rx2, 0x00000000
    ldi32   rx3, 0x00000000
    ldi32   rx4, 0x00000000
    m_pru_clr_pin   SCLK_PIN           ; MODE0 idle low
    m_pru_set_pin   CS_PIN
    m_pru_clr_pin   CS_PIN
    m_wait_nano_sec 35
    jal     retAddr, DO_5X_TRANSFER_M0_MSB
    qba     TRANSFER_DONE_M0_MSB
DO_M0_LSB:
    lbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qbbc    DO_M0_LSB, cfgTrigger, 0
    lbco    &tx0, c24, DMEM_CFG_CMD0, 4
    lbco    &tx1, c24, DMEM_CFG_CMD1, 4
    lbco    &tx2, c24, DMEM_CFG_CMD2, 4
    lbco    &tx3, c24, DMEM_CFG_CMD3, 4
    lbco    &tx4, c24, DMEM_CFG_CMD4, 4
    ldi32   rx0, 0x00000000
    ldi32   rx1, 0x00000000
    ldi32   rx2, 0x00000000
    ldi32   rx3, 0x00000000
    ldi32   rx4, 0x00000000
    m_pru_clr_pin   SCLK_PIN
    m_pru_set_pin   CS_PIN
    m_pru_clr_pin   CS_PIN
    m_wait_nano_sec 35
    jal     retAddr, DO_5X_TRANSFER_M0_LSB
    qba     TRANSFER_DONE_M0_LSB

DO_M1_MSB:
    lbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qbbc    DO_M1_MSB, cfgTrigger, 0
    lbco    &tx0, c24, DMEM_CFG_CMD0, 4
    lbco    &tx1, c24, DMEM_CFG_CMD1, 4
    lbco    &tx2, c24, DMEM_CFG_CMD2, 4
    lbco    &tx3, c24, DMEM_CFG_CMD3, 4
    lbco    &tx4, c24, DMEM_CFG_CMD4, 4
    ldi32   rx0, 0x00000000
    ldi32   rx1, 0x00000000
    ldi32   rx2, 0x00000000
    ldi32   rx3, 0x00000000
    ldi32   rx4, 0x00000000
    m_pru_clr_pin   SCLK_PIN           ; MODE1 idle low
    m_pru_set_pin   CS_PIN
    m_pru_clr_pin   CS_PIN
    m_wait_nano_sec 35
    jal     retAddr, DO_5X_TRANSFER_M1_MSB
    qba     TRANSFER_DONE_M1_MSB
DO_M1_LSB:
    lbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qbbc    DO_M1_LSB, cfgTrigger, 0
    lbco    &tx0, c24, DMEM_CFG_CMD0, 4
    lbco    &tx1, c24, DMEM_CFG_CMD1, 4
    lbco    &tx2, c24, DMEM_CFG_CMD2, 4
    lbco    &tx3, c24, DMEM_CFG_CMD3, 4
    lbco    &tx4, c24, DMEM_CFG_CMD4, 4
    ldi32   rx0, 0x00000000
    ldi32   rx1, 0x00000000
    ldi32   rx2, 0x00000000
    ldi32   rx3, 0x00000000
    ldi32   rx4, 0x00000000
    m_pru_clr_pin   SCLK_PIN
    m_pru_set_pin   CS_PIN
    m_pru_clr_pin   CS_PIN
    m_wait_nano_sec 35
    jal     retAddr, DO_5X_TRANSFER_M1_LSB
    qba     TRANSFER_DONE_M1_LSB

DO_M2_MSB:
    lbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qbbc    DO_M2_MSB, cfgTrigger, 0
    lbco    &tx0, c24, DMEM_CFG_CMD0, 4
    lbco    &tx1, c24, DMEM_CFG_CMD1, 4
    lbco    &tx2, c24, DMEM_CFG_CMD2, 4
    lbco    &tx3, c24, DMEM_CFG_CMD3, 4
    lbco    &tx4, c24, DMEM_CFG_CMD4, 4
    ldi32   rx0, 0x00000000
    ldi32   rx1, 0x00000000
    ldi32   rx2, 0x00000000
    ldi32   rx3, 0x00000000
    ldi32   rx4, 0x00000000
    m_pru_set_pin   SCLK_PIN           ; MODE2 idle high
    m_pru_set_pin   CS_PIN
    m_pru_clr_pin   CS_PIN
    m_wait_nano_sec 35
    jal     retAddr, DO_5X_TRANSFER_M2_MSB
    qba     TRANSFER_DONE_M2_MSB
DO_M2_LSB:
    lbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qbbc    DO_M2_LSB, cfgTrigger, 0
    lbco    &tx0, c24, DMEM_CFG_CMD0, 4
    lbco    &tx1, c24, DMEM_CFG_CMD1, 4
    lbco    &tx2, c24, DMEM_CFG_CMD2, 4
    lbco    &tx3, c24, DMEM_CFG_CMD3, 4
    lbco    &tx4, c24, DMEM_CFG_CMD4, 4
    ldi32   rx0, 0x00000000
    ldi32   rx1, 0x00000000
    ldi32   rx2, 0x00000000
    ldi32   rx3, 0x00000000
    ldi32   rx4, 0x00000000
    m_pru_set_pin   SCLK_PIN
    m_pru_set_pin   CS_PIN
    m_pru_clr_pin   CS_PIN
    m_wait_nano_sec 35
    jal     retAddr, DO_5X_TRANSFER_M2_LSB
    qba     TRANSFER_DONE_M2_LSB

DO_M3_MSB:
    lbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qbbc    DO_M3_MSB, cfgTrigger, 0
    lbco    &tx0, c24, DMEM_CFG_CMD0, 4
    lbco    &tx1, c24, DMEM_CFG_CMD1, 4
    lbco    &tx2, c24, DMEM_CFG_CMD2, 4
    lbco    &tx3, c24, DMEM_CFG_CMD3, 4
    lbco    &tx4, c24, DMEM_CFG_CMD4, 4
    ldi32   rx0, 0x00000000
    ldi32   rx1, 0x00000000
    ldi32   rx2, 0x00000000
    ldi32   rx3, 0x00000000
    ldi32   rx4, 0x00000000
    m_pru_set_pin   SCLK_PIN           ; MODE3 idle high
    m_pru_set_pin   CS_PIN
    m_pru_clr_pin   CS_PIN
    m_wait_nano_sec 35
    jal     retAddr, DO_5X_TRANSFER_M3_MSB
    qba     TRANSFER_DONE_M3_MSB
DO_M3_LSB:
    lbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qbbc    DO_M3_LSB, cfgTrigger, 0
    ; Load all 5 command words up front - the burst below runs all 5
    ; transfers back-to-back under one CS-low assertion.
    lbco    &tx0, c24, DMEM_CFG_CMD0, 4
    lbco    &tx1, c24, DMEM_CFG_CMD1, 4
    lbco    &tx2, c24, DMEM_CFG_CMD2, 4
    lbco    &tx3, c24, DMEM_CFG_CMD3, 4
    lbco    &tx4, c24, DMEM_CFG_CMD4, 4
    ldi32   rx0, 0x00000000
    ldi32   rx1, 0x00000000
    ldi32   rx2, 0x00000000
    ldi32   rx3, 0x00000000
    ldi32   rx4, 0x00000000
    m_pru_set_pin   SCLK_PIN
    m_pru_set_pin   CS_PIN
    m_pru_clr_pin   CS_PIN
    m_wait_nano_sec 35
    jal     retAddr, DO_5X_TRANSFER_M3_LSB
    qba     TRANSFER_DONE_M3_LSB

TRANSFER_DONE_M0_MSB:
    m_wait_nano_sec 10
    m_pru_set_pin   CS_PIN
    sbco    &rx0, c24, DMEM_MASTER_RX5_0, 4
    sbco    &rx1, c24, DMEM_MASTER_RX5_1, 4
    sbco    &rx2, c24, DMEM_MASTER_RX5_2, 4
    sbco    &rx3, c24, DMEM_MASTER_RX5_3, 4
    sbco    &rx4, c24, DMEM_MASTER_RX5_4, 4
    ldi32   cfgTrigger, 0x00000000
    sbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qba     DO_M0_MSB
TRANSFER_DONE_M0_LSB:
    m_wait_nano_sec 10
    m_pru_set_pin   CS_PIN
    sbco    &rx0, c24, DMEM_MASTER_RX5_0, 4
    sbco    &rx1, c24, DMEM_MASTER_RX5_1, 4
    sbco    &rx2, c24, DMEM_MASTER_RX5_2, 4
    sbco    &rx3, c24, DMEM_MASTER_RX5_3, 4
    sbco    &rx4, c24, DMEM_MASTER_RX5_4, 4
    ldi32   cfgTrigger, 0x00000000
    sbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qba     DO_M0_LSB
TRANSFER_DONE_M1_MSB:
    m_wait_nano_sec 10
    m_pru_set_pin   CS_PIN
    sbco    &rx0, c24, DMEM_MASTER_RX5_0, 4
    sbco    &rx1, c24, DMEM_MASTER_RX5_1, 4
    sbco    &rx2, c24, DMEM_MASTER_RX5_2, 4
    sbco    &rx3, c24, DMEM_MASTER_RX5_3, 4
    sbco    &rx4, c24, DMEM_MASTER_RX5_4, 4
    ldi32   cfgTrigger, 0x00000000
    sbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qba     DO_M1_MSB
TRANSFER_DONE_M1_LSB:
    m_wait_nano_sec 10
    m_pru_set_pin   CS_PIN
    sbco    &rx0, c24, DMEM_MASTER_RX5_0, 4
    sbco    &rx1, c24, DMEM_MASTER_RX5_1, 4
    sbco    &rx2, c24, DMEM_MASTER_RX5_2, 4
    sbco    &rx3, c24, DMEM_MASTER_RX5_3, 4
    sbco    &rx4, c24, DMEM_MASTER_RX5_4, 4
    ldi32   cfgTrigger, 0x00000000
    sbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qba     DO_M1_LSB
TRANSFER_DONE_M2_MSB:
    m_wait_nano_sec 10
    m_pru_set_pin   CS_PIN
    sbco    &rx0, c24, DMEM_MASTER_RX5_0, 4
    sbco    &rx1, c24, DMEM_MASTER_RX5_1, 4
    sbco    &rx2, c24, DMEM_MASTER_RX5_2, 4
    sbco    &rx3, c24, DMEM_MASTER_RX5_3, 4
    sbco    &rx4, c24, DMEM_MASTER_RX5_4, 4
    ldi32   cfgTrigger, 0x00000000
    sbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qba     DO_M2_MSB
TRANSFER_DONE_M2_LSB:
    m_wait_nano_sec 10
    m_pru_set_pin   CS_PIN
    sbco    &rx0, c24, DMEM_MASTER_RX5_0, 4
    sbco    &rx1, c24, DMEM_MASTER_RX5_1, 4
    sbco    &rx2, c24, DMEM_MASTER_RX5_2, 4
    sbco    &rx3, c24, DMEM_MASTER_RX5_3, 4
    sbco    &rx4, c24, DMEM_MASTER_RX5_4, 4
    ldi32   cfgTrigger, 0x00000000
    sbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qba     DO_M2_LSB
TRANSFER_DONE_M3_MSB:
    m_wait_nano_sec 10
    m_pru_set_pin   CS_PIN
    sbco    &rx0, c24, DMEM_MASTER_RX5_0, 4
    sbco    &rx1, c24, DMEM_MASTER_RX5_1, 4
    sbco    &rx2, c24, DMEM_MASTER_RX5_2, 4
    sbco    &rx3, c24, DMEM_MASTER_RX5_3, 4
    sbco    &rx4, c24, DMEM_MASTER_RX5_4, 4
    ldi32   cfgTrigger, 0x00000000
    sbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qba     DO_M3_MSB
TRANSFER_DONE_M3_LSB:
    m_wait_nano_sec 10
    m_pru_set_pin   CS_PIN
    sbco    &rx0, c24, DMEM_MASTER_RX5_0, 4
    sbco    &rx1, c24, DMEM_MASTER_RX5_1, 4
    sbco    &rx2, c24, DMEM_MASTER_RX5_2, 4
    sbco    &rx3, c24, DMEM_MASTER_RX5_3, 4
    sbco    &rx4, c24, DMEM_MASTER_RX5_4, 4
    ldi32   cfgTrigger, 0x00000000
    sbco    &cfgTrigger, c24, DMEM_CFG_TRIGGER, 4
    qba     DO_M3_LSB

; Out-of-line subroutines: 5 independent full-duplex transfers within one
; CS-low burst, 16-bit each, one per (MODE x bit-order) combination. Reached
; via jal (full-range jump) instead of being inlined in DO_M*_*, since 5
; macro expansions inline pushed every qba in the file past the 10-bit
; PC-relative offset limit. tx0..4 must already hold the 5 command words;
; rx0..4 receive the 5 response words independently (each transfer uses its
; own register pair). No inter-transfer delay - back-to-back at full rate.
DO_5X_TRANSFER_M0_MSB:
    m_transfer_packet_spi_master_gpo_sclk rx0, tx0, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE0", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx1, tx1, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE0", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx2, tx2, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE0", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx3, tx3, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE0", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx4, tx4, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE0", "MSB"
    jmp     retAddr
DO_5X_TRANSFER_M0_LSB:
    m_transfer_packet_spi_master_gpo_sclk rx0, tx0, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE0", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx1, tx1, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE0", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx2, tx2, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE0", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx3, tx3, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE0", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx4, tx4, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE0", "LSB"
    jmp     retAddr

DO_5X_TRANSFER_M1_MSB:
    m_transfer_packet_spi_master_gpo_sclk rx0, tx0, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE1", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx1, tx1, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE1", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx2, tx2, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE1", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx3, tx3, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE1", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx4, tx4, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE1", "MSB"
    jmp     retAddr
DO_5X_TRANSFER_M1_LSB:
    m_transfer_packet_spi_master_gpo_sclk rx0, tx0, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE1", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx1, tx1, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE1", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx2, tx2, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE1", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx3, tx3, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE1", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx4, tx4, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE1", "LSB"
    jmp     retAddr

DO_5X_TRANSFER_M2_MSB:
    m_transfer_packet_spi_master_gpo_sclk rx0, tx0, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE2", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx1, tx1, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE2", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx2, tx2, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE2", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx3, tx3, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE2", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx4, tx4, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE2", "MSB"
    jmp     retAddr
DO_5X_TRANSFER_M2_LSB:
    m_transfer_packet_spi_master_gpo_sclk rx0, tx0, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE2", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx1, tx1, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE2", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx2, tx2, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE2", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx3, tx3, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE2", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx4, tx4, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE2", "LSB"
    jmp     retAddr

DO_5X_TRANSFER_M3_MSB:
    m_transfer_packet_spi_master_gpo_sclk rx0, tx0, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE3", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx1, tx1, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE3", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx2, tx2, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE3", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx3, tx3, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE3", "MSB"
    m_transfer_packet_spi_master_gpo_sclk rx4, tx4, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE3", "MSB"
    jmp     retAddr
DO_5X_TRANSFER_M3_LSB:
    m_transfer_packet_spi_master_gpo_sclk rx0, tx0, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE3", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx1, tx1, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE3", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx2, tx2, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE3", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx3, tx3, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE3", "LSB"
    m_transfer_packet_spi_master_gpo_sclk rx4, tx4, PACKET_SIZE, bitId, SCLK_PIN, SDI_PIN, SDO_PIN, DELAY_COMPEN_1, DELAY_COMPEN_2, "MODE3", "LSB"
    jmp     retAddr
