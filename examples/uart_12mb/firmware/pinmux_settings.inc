; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2025 Texas Instruments Incorporated - http://www.ti.com/

;   filename:     pinmux_settings.inc
;   description:  apply per-board pinmux settings
;
;   authors:         Thomas Leyrer       date: 14.10.2025
;                    Nick Saulnier       date: 15.12.2025

; ***********************************************************************

; ********************** AM62x SK settings *************************************

; kick values to lock and unlock registers
KICK0_UNLOCK_VAL                .set   0x68EF3490
KICK1_UNLOCK_VAL				.set   0xD172BC5A
KICK_LOCK_VAL					.set   0x00000000

; PADMMRs in main domain
PADMMR_LOCK0_KICK0_REG          .set   0x000F1008
PADMMR_LOCK0_KICK1_REG          .set   0x000F100C
PADMMR_LOCK1_KICK0_REG          .set   0x000F5008
PADMMR_LOCK1_KICK1_REG          .set   0x000F500C

; pinmux for 3 channel UART
PAD_GPI_MODE2                   .set   0x08254002
PAD_GPI_MODE5                   .set   0x08254005
PAD_GPO_MODE5                   .set   0x08054005
PAD_GPO_MODE4                   .set   0x08054004   ; receiver is active and reflected on R31
PAD_GPO_MODE3                   .set   0x08054003   ; receiver is active and reflected on R31
PAD_GPO_MODE1                   .set   0x08054001   ; receiver is active and reflected on R31

PAD_GPI_PU                      .set   0x08224007   ; set input with pull-up to start with!

; 3 channel RX: PRU header J10
; J10.14 - PRU1_GPI9   � N23 � PAD_CFG 0x000F4040: 0x08254002 (mode 2, driver disabled, receiver enabled)
PAD_PRU1_GPI9                   .set   0x000F4040
; J10.15 - PRU1_GPI10 � N24 � PAD_CFG 0x000F4044:  0x08254002 (mode 2, driver disabled, receiver enabled)
PAD_PRU1_GPI10                  .set   0x000F4044
;J10.16 - PRU1_GPI11 � N25 � PAD_CFG 0x000F4048:  0x08254002 (mode 2, driver disabled, receiver enabled)
PAD_PRU1_GPI11                  .set   0x000F4048

; 3 channel TX: PRU header J10
; AA3, SOC_MMC0_DAT2  rework via R376. PRU0_GPO1 � AA3 � PAD_CFG 0x000F420C:  0x08054003 (mode 3, driver enabled, receiver enabled)
PAD_PRU0_GPO1                   .set   0x000F420C
; J10.17 � PRU0_GPO4 � P24 � PAD_CFG 0x000F404C:  0x08054004 (mode 4, driver enabled, receiver enabled)
PAD_PRU0_GPO4                   .set   0x000F404C
; J10.20 � PRU0_GPO7 � R23 � PAD_CFG 0x000F4058:  0x08054004 (mode 4, driver enabled, receiver enabled)
PAD_PRU0_GPO7                   .set   0x000F4058

; PRU hardware UART
; RXD - J3.36 - B18 - 0x000F419C - mode 5
PAD_PRU_UART_RXD                .set   0x000F419C
; TXD - J3.33 - E18 - 0x000F41A0 - mode 5
PAD_PRU_UART_TXD                .set   0x000F41A0

; debug/test pad
; PRU0_GPO0 - J10.13 mode 4
PAD_PRU0_GPO0                   .set   0x000F403C
; EDIO_31   - J10.8 - PAD A16 - mode 1
PAD_EDIO_31                     .set   0x000F41E4
; EDIO_30   - J10.6 - PAD B16 - mode 1
PAD_EDIO_30                     .set   0x000F41E0


;************************************* padconfig *************************************

m_apply_padconfig    .macro

; unlock PADMMR config register
; partition 0
    ldi32    r2, PADMMR_LOCK0_KICK0_REG  ; LOCK0 KICK0 register
    ldi32    r3, PADMMR_LOCK0_KICK1_REG  ; LOCK0 KICK1 register
    ldi32    r4, KICK0_UNLOCK_VAL  ; Kick 0
    ldi32    r5, KICK1_UNLOCK_VAL  ; kick 1
; TODO: Is this register locked or unlocked by default in MCU+ SDK?
; TODO: Linux should have already unlocked the PADMMR config register during Uboot
; So for Linux, do we want to remove lock/unlock code?
; or better to leave it in so that this code is more portable between processors & SDKs?
;    ldi      r6, KICK_LOCK_VAL     ; LOCK value

    sbbo     &r4, r2, 0, 4
    sbbo     &r5, r3, 0, 4

; partition 1
    ldi32    r2, PADMMR_LOCK1_KICK0_REG  ; LOCK1 KICK0 register
    ldi32    r3, PADMMR_LOCK1_KICK1_REG  ; LOCK1 KICK1 register

    sbbo     &r4, r2, 0, 4
    sbbo     &r5, r3, 0, 4

; pin-mux configuration - PRU1_GPI9 - input J10.14
    ldi32    r2, PAD_PRU1_GPI9
    ldi32    r3, PAD_GPI_MODE2
    sbbo     &r3,r2, 0, 4

; pin-mux configuration - PRU1_GPI10 - input J10.15
    ldi32    r2, PAD_PRU1_GPI10
    ldi32    r3, PAD_GPI_MODE2
    sbbo     &r3,r2, 0, 4

; pin-mux configuration - PRU1_GPI11 - input J10.16
    ldi32    r2, PAD_PRU1_GPI11
    ldi32    r3, PAD_GPI_MODE2
    sbbo     &r3,r2, 0, 4

; pin-mux configuration - PRU0_GP0 - output J10.13 mode 3
    ldi32    r2, PAD_PRU0_GPO0
    ldi32    r3, PAD_GPO_MODE4
    sbbo     &r3,r2, 0, 4

; pin-mux configuration - PRU0_GP1 - output r376 mode 3
    ldi32    r2, PAD_PRU0_GPO1
    ldi32    r3, PAD_GPI_PU
    sbbo     &r3,r2, 0, 4

; pin-mux configuration - PRU0_GP4 - output J10.17
    ldi32    r2, PAD_PRU0_GPO4
    ldi32    r3, PAD_GPI_PU
    sbbo     &r3,r2, 0, 4

; pin-mux configuration - PRU0_GP7 - output J10.20
    ldi32    r2, PAD_PRU0_GPO7
    ldi32    r3, PAD_GPI_PU
    sbbo     &r3,r2, 0, 4

; pin-mux configuration - PRU_UART_RXD - output J2.36
    ldi32    r2, PAD_PRU_UART_RXD
    ldi32    r3, PAD_GPI_MODE5
    sbbo     &r3,r2, 0, 4

; pin-mux configuration - PRU_UART_TXD - output J2.33
    ldi32    r2, PAD_PRU_UART_TXD
    ldi32    r3, PAD_GPO_MODE5
    sbbo     &r3,r2, 0, 4

; pin-mux configuration - PAD_EDIO_31 - output J10.8
    ldi32    r2, PAD_EDIO_31
    ldi32    r3, PAD_GPO_MODE1
    sbbo     &r3,r2, 0, 4

; pin-mux configuration - PAD_EDIO_31 - output J10.6
    ldi32    r2, PAD_EDIO_30
    ldi32    r3, PAD_GPO_MODE1
    sbbo     &r3,r2, 0, 4

    .endm


;************************* enable output pins **********************************
m_enable_output_pins    .macro

; pin-mux configuration - PRU0_GP1 - output r376 mode 3
    ldi32    r2, PAD_PRU0_GPO1
    ldi32    r3, PAD_GPO_MODE3
    sbbo     &r3,r2, 0, 4

; pin-mux configuration - PRU0_GP4 - output J10.17
    ldi32    r2, PAD_PRU0_GPO4
    ldi32    r3, PAD_GPO_MODE4
    sbbo     &r3,r2, 0, 4

; pin-mux configuration - PRU0_GP7 - output J10.20
    ldi32    r2, PAD_PRU0_GPO7
    ldi32    r3, PAD_GPO_MODE4
    sbbo     &r3,r2, 0, 4

    .endm

; **********************end AM62x SK settings **********************************