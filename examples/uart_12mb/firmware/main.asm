; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2025 Texas Instruments Incorporated - http://www.ti.com/

;   filename:     main_PRU0.h
;   description:  PRU0 implements 4 half-duplex TX UART instances, which can run
;                 at up to 12Mbaud. 3 UART instances implemented with the PRU
;                 peripheral interface, 1 UART instance through the PRU
;                 subsystem's hardware UART instance.
;
;   name:         Thomas Leyrer       date: 14.10.2025

; ***********************************************************************


; ******************* includes ******************************************
; macro to convert data to UART to 16 bit TX FIFO data
    .include       "uart_transmit_macro.inc"

; ****************** defines / macros ***********************************

; Scratch pad banks
SP_BANK0                        .set   10
SP_BANK1                        .set   11
SP_BANK2                        .set   12

; max frame length is 88 Bytes
; actual frame length is passed through the TXm_CHn_CMD register
FRAME_LENGTH                    .set   88

; buffer pointer offset in ICSS DATA RAM 0 (c24 base address)
; 8 * 128 bytes = 1 KB
; group channel 0 data and control into one memory block for linux driver simplification
; command register in ICSS DATA RAM 0
; use different 32 bit values as host may do only 32 bit access 

TX_CH0_A                        .set   0
TX_CH0_B                        .set   128
TXA_CH0_CMD                     .set   256
TXB_CH0_CMD                     .set   256+4
TXA_CH0_STATUS                  .set   256+8
TXB_CH0_STATUS                  .set   256+12

TX_CH1_A                        .set   512
TX_CH1_B                        .set   512 + 128
TXA_CH1_CMD                     .set   512 + 256
TXB_CH1_CMD                     .set   512 + 256 + 4
TXA_CH1_STATUS                  .set   512 + 256 + 8
TXB_CH1_STATUS                  .set   512 + 256 + 12

TX_CH2_A                        .set   1024
TX_CH2_B                        .set   1024 + 128
TXA_CH2_CMD                     .set   1024 + 256
TXB_CH2_CMD                     .set   1024 + 256 + 4
TXA_CH2_STATUS                  .set   1024 + 256 + 8
TXB_CH2_STATUS                  .set   1024 + 256 + 12

TX_CH3_A                        .set   1536
TX_CH3_B                        .set   1536 + 128
TXA_CH3_CMD                     .set   1536 + 256
TXB_CH3_CMD                     .set   1536 + 256 + 4
TXA_CH3_STATUS                  .set   1536 + 256 + 8
TXB_CH3_STATUS                  .set   1536 + 256 + 12

CMD_EMPTY                       .set   0   ; no data in this buffer
STATUS_NOT_ACTIVE               .set   0
STATUS_ACTIVE                   .set   1   ; frame is in transmit
STATUS_DONE                     .set   2   ; frame transmit complete

PRU_FW_REV_REG                  .set   0x2000-4
PRU_FW_REV                      .set   0x00010001  ; major.minor (msw = 1, lsw = 1)

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

; MODIFIED: There was a bug in the code, where PAD_GPI_PU was used as the
; pinmux address, instead of being used as the pinmux bit values
; suspected buggy code is commented out under the MODIFIED label
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

; channel states
IDLE_STATE                        .set   1
SOF_STATE                         .set   2
NEXT1_STATE                       .set   3
NEXTF_STATE                       .set   4
EOF_STATE                         .set   8

; QUESTION: What do we mean by "permanent assignment"?
; That this register value is NOT saved and loaded to SPAD?
STATE_REG                       .set  r29     ; permanent assignment
; usage:
; STATE_REG.b0 = channel 0 state
; STATE_REG.b1 = channel 1 state
; STATE_REG.b2 = channel 2 state
; STATE_REG.b3 = channel 3 state

	.asg  r28.w2 ,   CH3_BUF_PTR
	.asg  r28.w0 ,   CH3_CNT8_REG
	.asg  r27.w2 ,   CH3_CMD_BUFF_REG
	.asg  r27.b1 ,   CH3_PREV_BUF

BUF_PTR_REG                     .set  r26     ; part of context in SP

CMD_BUFF_REG                    .set  r24     ; part of context in SP
; r24.b0 holds TXn_CHm_CMD (value must be >=8 bits)
; r24.b1 has previous buffer. 0 = previous buffer was A, 1 = previous buffer was B
; r24.w2 holds address offsets of registers to read

; TODO: Finish adding register assignments
    .asg r25.b0 ,   CNT16_ABSOLUTE_REG
    .asg r25.b1 ,   CNT16_RELATIVE_REG

; TODO: since we only have 8 bits for the length of bytes coming from Linux,
; this limits us to sending length >=255 bytes if we use TX_CMD to pass
; the length of the buffer. When we add larger buffers, look into changing this
;
; Additional notes:
; it looks like we can only divide registers into 4 bytes, 2 bytes, 1 byte, or
; 1 bit (i,e, NOT 12 bits & 4 bits). So we could not pass lengths up to 2kB by
; doing r24[11:0] = length, r24[15:12] = previous buffer


; ICSS HW UART Register
; UART I/O offsets , base 0x28000 is in C7
UART_RBR                        .set    0x00  ; receive  buffer in [7:0] byte 0 read
UART_THR                        .set    0x00  ; transmit buffer in [7:0] byte 0 write
UART_IER                        .set    0x04
UART_IIR                        .set    0x08
UART_FCR                        .set    0x08
UART_LCR                        .set    0x0C
UART_MSR                        .set    0x10
UART_LSR                        .set    0x14
UART_DLL                        .set    0x20
UART_DLH                        .set    0x24
UART_PID1                       .set    0x28
UART_PID2                       .set    0x2C
UART_PWREMU_MGMT                .set    0x30
UART_MDR                        .set    0x34
UART_IRQ_EN_RX	                .set    0x01
UART_IRQ_EN_TX	                .set    0x02
UART_IRQ_DISABLE                .set    0x00
UART_IIR_THR_EMPTY              .set    0x1
UART_IIR_RBR_FULL	            .set    0x2


; token to use PRU firmware in nohost mode. All config is done by PRU
NOHOST         .set     1

;CCS/makefile specific settings

    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

  	.global     main

main:

    zero        &r0, 128                     ; clear 128 bytes (R0-R31)

; configure C28 to point to ICSS shared RAM
    ldi         r2, 0x0100
    sbco        &r2, c11, 0x28, 2

; set firmware revision
    ldi        r2.w0, PRU_FW_REV_REG
    ldi32      r3, PRU_FW_REV
    sbco       &r3, c24, r2.w0, 4

; *************************** configure ICSS HW UART  *********************

; TODO: Modify this code so that the HW UART frequency can be independently
; set from the 3 channel peripheral interface frequency from the top of the
; file (assuming that is how we want to configure? Or do we want to pass those
; values in from the initializing core at runtime?)

; reset UART
    ldi32       r2, 0
    lbco        &r2, c7, UART_PWREMU_MGMT, 4

; TODO: Does the loop command unroll? It seems like not 
; wait for some time
    loop        L_INIT_UART_WAIT, 100
                nop
L_INIT_UART_WAIT:

;  0 = 16x oversampling, 1 x 13x oversampling
    ldi32       r2, 0
    sbco        &r2, c7, UART_MDR, 4

; 192 MHz / 16 divide by 1
    ldi32       r2, 1
    sbco        &r2, c7, UART_DLL, 4
    ldi32        r2, 0
    sbco        &r2, c7, UART_DLH, 4

;  non-FIFO mode
    sbco        &r2, c7, UART_FCR, 4
; clear FIFOs
    ldi32       r2, 0x0006
    sbco        &r2, c7, UART_FCR, 4

; LCR has 8 bit length, no parity and 1 stop
    ldi32       r2, 0x13
    sbco        &r2, c7, UART_LCR, 4

; enable power
    ldi32       r2, 0x6001
    sbco        &r2, c7, UART_PWREMU_MGMT, 4

; *************************** end configure ICSS HW UART  *****************


; PRU cycle counter handling
; enable PRU0 cycle counter
    ldi         r2.w0, 0x00b
	sbco        &r2.b0, c11, 0, 1

; clear PRU cycle counter
    zero        &r2, 4
	sbco        &r2.b0, c11, 0x0c, 4

; get latency from cycle counter
    lbco        &r2, c11, 0x0c, 4

; r2 has cycle count - 3 for lbco


   .if $defined(NOHOST)
;************************************* padconfig *************************************
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

;******************************** end padconfig *************************************
    .endif


;******************************** start with one symbol high 8 bits *************************************

    ldi32     r2, 0x00026000   ; PRU Subsystem CFG registers base address

; set shift mode for SP
    ldi32     r3, 0x00000002   ; [1] enable shift mode on PRU scratch pad
    sbbo      &r3,r2,0x34,4    ; CFG base address SPP register is offset 0x34

; set mode to 3 peripheral mode on both PRUs
    ldi32     r3, 0x04000001   ; mux mode to 3 peripheral mode
    sbbo      &r3,r2,8,4       ; GPCFG0_REG for PRU 0
    sbbo      &r3,r2,0x0c,4    ; GPCFG1_REG for PRU 1

; set ED0/1/2 TX fifo bit swap - UART is LSB first
    ldi32     r3, 0x80000000
    sbbo      &r3, r2, 0xe8, 4
    sbbo      &r3, r2, 0xf0, 4
    sbbo      &r3, r2, 0xf8, 4

; set TX clock source
; TODO: in generalized code, set TX clock divider at the top of the code
; PRU_ICSS_PRU0_ED_TX_CFG_REG[4] PRU0_ED_TX_CLK_SEL for the TX clock source
    ldi32     r3, 0x000f0000       ; TX divider - 16 assumes 1920 MHz UART PLL
                                   ; [4] = 0 -> UART CLOCK 192 MHz
    sbbo      &r3, r2, 0xe4, 4     ;

; QUESTION: was this section supposed to be commented out?
; re-init all tx channels
;    set       r31, r31, 19
; bit 5 indicates last bit on wire
;    wbc       r31, 5

; select channel 0
    ldi       r30.b2, 0         ; select channel 0
; pre load TX fifo - 32 bit
    ldi       r30.b0, 0xff
    ldi       r30.b0, 0xff
; select channel 1
    ldi       r30.b2, 1         ; select channel 1
; pre load TX fifo - 32 bit
    ldi       r30.b0, 0xff
    ldi       r30.b0, 0xff
; select channel 2
    ldi       r30.b2, 2         ; select channel 2
; pre load TX fifo - 32 bit
    ldi       r30.b0, 0xff
    ldi       r30.b0, 0xff

; Set R31[18] = 1 (tx_channel_go)
    set       r31, r31, 18
; select channel 1
    ldi       r30.b2, 1         ; select channel 1
; Set R31[18] = 1 (tx_channel_go)
    set       r31, r31, 18

; select channel 0
    ldi       r30.b2, 0         ; select channel 1
; Set R31[18] = 1 (tx_channel_go)
    set       r31, r31, 18

; it is 2 x 666 cycles from here before fifo runs empty

; enable pads for UART mode
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

; QUESTION: should these commented lines about MASK be removed?
;    ldi      MASK_REG1, 0x01fe                   ; 0b0000 0001 1111 1110
;    ldi      MASK_REG2, 0xf800                   ; 0b1111 1000 0000 0000
    ldi      BUF_PTR_REG, 0
    ldi      r24.b1, 1                           ; set "previous buffer" = B, so code starts by checking buffer A

; init CH0, CH1, CH2 context
    xout     SP_BANK0, &r24, 12                  ; R24-R26
    xout     SP_BANK1, &r24, 12                  ; R24-R26
    xout     SP_BANK2, &r24, 12                  ; R24-R26

; init CH3 context
    ldi      CH3_PREV_BUF, 1
    ldi      CH3_CNT8_REG,  0


;******************************** start with channel 0 *************************************

L_CH0_IDLE:
; select channel 0
    ldi      r30.b2, 0             ; select channel 0
; TODO: R0-R26 is pulled from SP_BANKx
; for all channels, the XOUT commands that store registers to SP_BANKx use
; R2-R28 instead. Is that a bug?
; How to store length of bytes left to send across cycles?
; get context for channel 0
    xin      SP_BANK0, &r0, 120-12   ; get R0-R26 context
; check on fifo tx level
    and      r2.b0, r31.b0, 0x1c    ; bit 2-4 has fifo level. 2 word = 8
    qblt     L_CH1_IDLE, r2.b0, 8   ; if fifo level > 2 word than go to next channel - nothing to do

; TODO:
; set channel-specific variables here?

; check on channel 0 state using permanent state register - r29
    qbeq     L_CH0_SOF,   STATE_REG.b0, SOF_STATE
    qbeq     L_CH0_NEXT1, STATE_REG.b0, NEXT1_STATE
    qbeq     L_CH0_NEXTF, STATE_REG.b0, NEXTF_STATE
    qbeq     L_CH0_EOF,   STATE_REG.b0, EOF_STATE

; no active frame state, add 8 bit of idle at the end of UART frame handler
    ldi      r30.b0, 0xff

; TODO: make this comment more useful (looks like it is describing the section)
; or are they serving some purpose?
;-------------
; check cmd on active frame data. This should operate in ping pong mode
; xor of buffer_id will alternate the buffers in case both are active.

; r24.b1 has previous buffer, if 0 it was A if 1 it was B
    qbbc     L_CHECK_CH0_B_FIRST,r24.b1,0

; check A buffer first as previous buffer was B
    ldi      r24.w2, TXA_CH0_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_NO_CH0_ACTIVE_A
    qbeq     L_NO_CH0_ACTIVE_A, r24.b0 , CMD_EMPTY
; found A active, set buffer ptr
    ldi      BUF_PTR_REG.w0, TX_CH0_A
; set STATE to SOF_STATE
    ldi      STATE_REG.b0, SOF_STATE
; set previous channel to A
    ldi      r24.b1, 0

; claim buffer A in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXA_CH0_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel A
    ldi      r2.b0, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
    qba      L_EXIT_CH0_IDLE

L_NO_CH0_ACTIVE_A:
    ldi      r24.w2, TXB_CH0_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_EXIT_CH0_IDLE
    qbeq     L_EXIT_CH0_IDLE, r24.b0, CMD_EMPTY
; found B active, set buffer ptr
    ldi      BUF_PTR_REG.w0, TX_CH0_B
; set STATE to CH0
    ldi      STATE_REG.b0, SOF_STATE
; set previous channel to B
    ldi      r24.b1, 0x01
; claim buffer B in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXB_CH0_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel B
    ldi      r2.b0, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1

; goto next channel
    qba      L_EXIT_CH0_IDLE

L_CHECK_CH0_B_FIRST:
    ldi      r24.w2, TXB_CH0_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_NO_CH0_ACTIVE_B
    qbeq     L_NO_CH0_ACTIVE_B, r24.b0, CMD_EMPTY
; found B active, set buffer ptr
    ldi      BUF_PTR_REG.w0, TX_CH0_B
; set STATE to SOF_STATE
    ldi      STATE_REG.b0, SOF_STATE
; set previous channel to B
    ldi      r24.b1, 0x01
; claim buffer B in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXB_CH0_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel B
    ldi      r2.b0, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
    qba      L_EXIT_CH0_IDLE
L_NO_CH0_ACTIVE_B:
    ldi      r24.w2, TXA_CH0_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_EXIT_CH0_IDLE
    qbeq     L_EXIT_CH0_IDLE, r24.b0, CMD_EMPTY
; found A active, set buffer ptr
    ldi      BUF_PTR_REG.w0, TX_CH0_A
; set STATE to CH0
    ldi      STATE_REG.b0, SOF_STATE
; set previous channel to A
    ldi      r24.b1, 0
; claim buffer A in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXA_CH0_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel A
    ldi      r2.b0, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
L_EXIT_CH0_IDLE:
; QUESTION
; This command copies 120-12 = 108 bytes = 27 registers, starting with R2
; So we copy R2 - R28 into the SPBANK for ALL channels (not just channel 0)
; but at the beginning of each channel section, we copy R0-R26 back from the SPBANK
; So which set of registers do we actually want to copy and replace?
    xout     SP_BANK0, &r2, 120-12
    qba      L_CH1_IDLE

; CH0_SOF state comes first as it takes most cycle
; *************************** State CH0_SOF ********************************************
L_CH0_SOF:

; budget is ~ 50 cycles per sub-state. Cycle counter cc is documented on each instruction
; context was initialized on EOF

; TODO: the cc comments are inconsistent. Calculate new cycle counts for all sections
; are these comments from Thomas running this on silicon and checking the cc
; from CCS?
; convert 8 bit data to 10 bit UART symbol with 1 start bit '0' and one stop bit '1'
; UART is LSB first which means the first bit on wire is start bit followed by bit 0 of data bits.
; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16

; use macro for send
    m_tx_sof SP_BANK0 STATE_REG.b0

; go to next channel
    qba      L_CH1_IDLE

; *************************** State CH0_NEXT1 ******************************************
L_CH0_NEXT1:

; now get back to 2nd 16 bit data which is spread into r2.b1-b3
; Data pattern is: D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16

;   transmit macro
    m_tx_next1 SP_BANK0 STATE_REG.b0

; go to next channel
    qba     L_CH1_IDLE                         ; cc 25

; *************************** State CH0_NEXTF ******************************************
L_CH0_NEXTF:

; in this state there are 5 different patterns depending on the 16 bit data count cnt16d
; cnt16d: 0 (first or every 5th)
; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; cnt16d: 1 (second or every 5th +1)
; Data pattern is: D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16

; cnt16d: 2 (third or every 5th + 2)
; Data pattern is: D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16

; cnt16d: 3 (fourth or every 5th + 3)
; Data pattern is: D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;

; cnt16d: 4 (fifth or every 5th + 2)
; Data pattern is: D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;

; need a absolute count and relative count as mod 5 is not easily done

    qbeq     L_CH0_NEXTF1, CNT16_RELATIVE_REG, 0
    qbeq     L_CH0_NEXTF2, CNT16_RELATIVE_REG, 1
    qbeq     L_CH0_NEXTF3, CNT16_RELATIVE_REG, 2
    qbeq     L_CH0_NEXTF4, CNT16_RELATIVE_REG, 3
    qbeq     L_CH0_NEXTF5, CNT16_RELATIVE_REG, 4

; TODO: Add error handling?
; should never reach this point
    halt

L_CH0_NEXTF1:
; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16

; transmit macro
    m_tx_nextf1 SP_BANK0

; go to next channel
    qba     L_CH1_IDLE

L_CH0_NEXTF2:
; 2nd 16 bit data which is spread into r2.b1-b3
; Data pattern is: D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16

; transmit macro
    m_tx_nextf2 SP_BANK0

; go to next channel
    qba     L_CH1_IDLE

L_CH0_NEXTF3:
; cnt16d: 2 (third or every 5th + 2)
; Data pattern is: D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16

; transmit macro
    m_tx_nextf3 SP_BANK0

; go to next channel
    qba     L_CH1_IDLE

L_CH0_NEXTF4:
; cnt16d: 3 (fourth or every 5th + 3)
; Data pattern is: D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;

; transmit macro
    m_tx_nextf4 SP_BANK0

; go to next channel
    qbne    L_SKIP_EOF_CHECK, CNT16_ABSOLUTE_REG, 54
    ldi     STATE_REG.b0, EOF_STATE
L_SKIP_EOF_CHECK:
    qba     L_CH1_IDLE

L_CH0_NEXTF5:
; cnt16d: 4 (fifth or every 5th + 2)
; Data pattern is: D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;

; transmit macro
    m_tx_nextf5 SP_BANK0

; go to next channel
    qba     L_CH1_IDLE

; *************************** State CH0_EOF ******************************************
L_CH0_EOF:
; cnt16d: 4 (fifth or every 5th + 2)
; Data pattern is: D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;

; transmit macro
    m_tx_eof SP_BANK0 STATE_REG.b0

; go to next channel
; set channel status transfer complete
    qbbs     L_CH0_TXB_STATUS_UPDATE, r24.b1, 0
    ldi      r2, STATUS_DONE
    ldi      r3.w0, TXA_CH0_STATUS
    sbco     &r2, c24, r3.w0, 1
; done with ch0, goto ch1
    JMP      L_CH1_IDLE
L_CH0_TXB_STATUS_UPDATE:
    ldi      r2, STATUS_DONE
    ldi      r3.w0, TXB_CH0_STATUS
    sbco     &r2, c24, r3.w0, 1
    qba     L_CH1_IDLE


; **************************** CH1_IDLE ****************************************

L_CH1_IDLE:

; select channel 1
    ldi       r30.b2, 1         ; select channel 1

; get context channel 1
    xin      SP_BANK1, &r0, 120-12   ; get R0-R26 context
; check on fifo tx level
    and      r2.b0, r31.b1, 0x1c    ; bit 2-4 has fifo level. 2 word = 8
    qblt     L_CH2_IDLE, r2.b0, 8   ; if fifo level > 2 word than go to next channel - nothing to do

; check on channel 1 state using permanent state register - r29
    qbeq     L_CH1_SOF,   STATE_REG.b1, SOF_STATE
    qbeq     L_CH1_NEXT1, STATE_REG.b1, NEXT1_STATE
    qbeq     L_CH1_NEXTF, STATE_REG.b1, NEXTF_STATE
    qbeq     L_CH1_EOF,   STATE_REG.b1, EOF_STATE

; no active frame state, add 8 bit of idle at the end of UART frame handler
    ldi      r30.b0, 0xff

; check cmd on active frame data. This should operate in ping pong mode
; xor of buffer_id will alternate the buffers in case both are active.

; r24.b1 has previous buffer, if 0 it was A if 1 it was B
    qbbc     L_CHECK_CH1_B_FIRST,r24.b1,0

; check A buffer first as previous buffer was B
    ldi      r24.w2, TXA_CH1_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_NO_CH1_ACTIVE_A
    qbeq     L_NO_CH1_ACTIVE_A, r24.b0 , CMD_EMPTY
; found A active, set buffer ptr
    ldi      BUF_PTR_REG.w0, TX_CH1_A
; set STATE to SOF_STATE
    ldi      STATE_REG.b1, SOF_STATE
; set previous channel to A
    ldi      r24.b1, 0
; claim buffer A in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXA_CH1_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel A
    ldi      r2, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
    qba      L_EXIT_CH1_IDLE
L_NO_CH1_ACTIVE_A:
    ldi      r24.w2, TXB_CH1_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_EXIT_CH1_IDLE
    qbeq     L_EXIT_CH1_IDLE, r24.b0, CMD_EMPTY
; found B active, set buffer ptr
    ldi      BUF_PTR_REG.w0, TX_CH1_B
; set STATE to CH0
    ldi      STATE_REG.b1, SOF_STATE
; set previous channel to B
    ldi      r24.b1, 0x01
; claim buffer B in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXB_CH1_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel B
    ldi      r2, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
    qba      L_EXIT_CH1_IDLE

L_CHECK_CH1_B_FIRST:
    ldi      r24.w2, TXB_CH1_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_NO_CH1_ACTIVE_B
    qbeq     L_NO_CH1_ACTIVE_B, r24.b0, CMD_EMPTY
; found B active, set buffer ptr
    ldi      BUF_PTR_REG.w0, TX_CH1_B
; set STATE to SOF_STATE
    ldi      STATE_REG.b1, SOF_STATE
; set previous channel to B
    ldi      r24.b1, 0x01
; claim buffer B in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXB_CH1_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel B
    ldi      r2, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
    qba      L_EXIT_CH1_IDLE
L_NO_CH1_ACTIVE_B:
    ldi      r24.w2, TXA_CH1_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_EXIT_CH1_IDLE
    qbeq     L_EXIT_CH1_IDLE, r24.b0 , CMD_EMPTY
; found A active, set buffer ptr
    ldi      BUF_PTR_REG.w0, TX_CH1_A
; set STATE to CH1
    ldi      STATE_REG.b1, SOF_STATE
; set previous channel to A
    ldi      r24.b1, 0
; claim buffer B in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXA_CH1_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel A
    ldi      r2, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
L_EXIT_CH1_IDLE:
    xout     SP_BANK1, &r2, 120-12
    qba      L_CH2_IDLE

; CH1_SOF state comes first as it takes most cycle
; *************************** State CH1_SOF ********************************************
L_CH1_SOF:

; See data pattern / bit count in CH0 version of state, or in the macro

; use macro for send
   m_tx_sof SP_BANK1 STATE_REG.b1

; go to next channel
    qba     L_CH2_IDLE                         ; cc 25

; *************************** State CH1_NEXT ******************************************
L_CH1_NEXT1:

; See data pattern / bit count in CH0 version of state, or in the macro

;   transmit macro
    m_tx_next1 SP_BANK1 STATE_REG.b1

; go to next channel
    qba     L_CH2_IDLE                         ; cc 25

; *************************** State CH1_NEXTF ******************************************
L_CH1_NEXTF:

; See data pattern / bit count in CH0 version of state, or in the macro

; need a absolute count and relative count as mod 5 is not easly done

    qbeq     L_CH1_NEXTF1, CNT16_RELATIVE_REG, 0
    qbeq     L_CH1_NEXTF2, CNT16_RELATIVE_REG, 1
    qbeq     L_CH1_NEXTF3, CNT16_RELATIVE_REG, 2
    qbeq     L_CH1_NEXTF4, CNT16_RELATIVE_REG, 3
    qbeq     L_CH1_NEXTF5, CNT16_RELATIVE_REG, 4

; TODO: Add error handling?
; should never reach this point
    halt

L_CH1_NEXTF1:
; transmit macro
    m_tx_nextf1 SP_BANK1

; go to next channel
    qba     L_CH2_IDLE

L_CH1_NEXTF2:
; transmit macro
    m_tx_nextf2 SP_BANK1

; go to next channel
    qba     L_CH2_IDLE

L_CH1_NEXTF3:
; transmit macro
    m_tx_nextf3 SP_BANK1

; go to next channel
    qba     L_CH2_IDLE

L_CH1_NEXTF4:
; transmit macro
    m_tx_nextf4 SP_BANK1

; go to next channel
    qbne    L_SKIP_CH1_EOF_CHECK, CNT16_ABSOLUTE_REG, 54
    ldi     STATE_REG.b1, EOF_STATE
L_SKIP_CH1_EOF_CHECK:
    qba     L_CH2_IDLE

L_CH1_NEXTF5:
; transmit macro
    m_tx_nextf5 SP_BANK1

; go to next channel
    qba     L_CH2_IDLE

; *************************** State CH1_EOF ******************************************
L_CH1_EOF:

; See data pattern / bit count in CH0 version of state, or in the macro

; transmit macro
    m_tx_eof SP_BANK1 STATE_REG.b1

; go to next channel
; set channel status transfer complete
    qbbs     L_CH1_TXB_STATUS_UPDATE, r24.b1, 0
    ldi      r2, STATUS_DONE
    ldi      r3.w0, TXA_CH1_STATUS
    sbco     &r2, c24, r3.w0, 1
; done with ch1, goto ch2
    JMP      L_CH2_IDLE
L_CH1_TXB_STATUS_UPDATE:
    ldi      r2, STATUS_DONE
    ldi      r3.w0, TXB_CH1_STATUS
    sbco     &r2, c24, r3.w0, 1
    qba     L_CH2_IDLE


; **************************** CH2_IDLE ****************************************

L_CH2_IDLE:

; select channel 2
    ldi       r30.b2, 2         ; select channel 1

; get context channel 2
    xin      SP_BANK2, &r0, 120-12   ; get R0-R26 context
; check on fifo tx level
    and      r2.b0, r31.b2, 0x1c    ; bit 2-4 has fifo level. 2 word = 8
    qblt     L_CH3_IDLE, r2.b0, 8   ; if fifo level > 2 word than go to next channel - nothing to do

; check on channel 2 state using permanent state register - r29
    qbeq     L_CH2_SOF,   STATE_REG.b2, SOF_STATE
    qbeq     L_CH2_NEXT1, STATE_REG.b2, NEXT1_STATE
    qbeq     L_CH2_NEXTF, STATE_REG.b2, NEXTF_STATE
    qbeq     L_CH2_EOF,   STATE_REG.b2, EOF_STATE

; no active frame state, add 8 bit of idle at the end of UART frame handler
    ldi      r30.b0, 0xff

; check cmd on active frame data. This should operate in ping pong mode
; xor of buffer_id will alternate the buffers in case both are active.

; r24.b1 has previous buffer, if 0 it was A if 1 it was B
    qbbc     L_CHECK_CH2_B_FIRST,r24.b1,0

; check A buffer first as previous buffer was B
    ldi      r24.w2, TXA_CH2_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_NO_CH2_ACTIVE_A
    qbeq     L_NO_CH2_ACTIVE_A, r24.b0 , CMD_EMPTY
; found A active, set buffer ptr
    ldi      BUF_PTR_REG.w0, TX_CH2_A
; set STATE to SOF_STATE
    ldi      STATE_REG.b2, SOF_STATE
; set previous channel to A
    ldi      r24.b1, 0
; claim buffer A in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXA_CH2_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel A
    ldi      r2.b0, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
    qba      L_EXIT_CH2_IDLE
L_NO_CH2_ACTIVE_A:
    ldi      r24.w2, TXB_CH2_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_EXIT_CH2_IDLE
    qbeq     L_EXIT_CH2_IDLE, r24.b0, CMD_EMPTY
; found B active, set buffer ptr
    ldi      BUF_PTR_REG.w0, TX_CH2_B
; set STATE to CH2
    ldi      STATE_REG.b2, SOF_STATE
; set previous channel to B
    ldi      r24.b1, 0x01
; claim buffer B in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXB_CH2_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel B
    ldi      r2.b0, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
    qba      L_EXIT_CH2_IDLE

L_CHECK_CH2_B_FIRST:
    ldi      r24.w2, TXB_CH2_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_NO_CH2_ACTIVE_B
    qbeq     L_NO_CH2_ACTIVE_B, r24.b0, CMD_EMPTY
; found B active, set buffer ptr
    ldi      BUF_PTR_REG.w0, TX_CH2_B
; set STATE to SOF_STATE
    ldi      STATE_REG.b2, SOF_STATE
; set previous channel to B
    ldi      r24.b1, 0x01
; claim buffer B in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXB_CH2_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel B
    ldi      r2.b0, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
    qba      L_EXIT_CH2_IDLE
L_NO_CH2_ACTIVE_B:
    ldi      r24.w2, TXA_CH2_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_EXIT_CH2_IDLE
    qbeq     L_EXIT_CH2_IDLE, r24.b0, CMD_EMPTY
; found A active, set buffer ptr
    ldi      BUF_PTR_REG.w0, TX_CH2_A
; set STATE to CH2
    ldi      STATE_REG.b2, SOF_STATE
; set previous channel to A
    ldi      r24.b1, 0
; claim buffer A in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXA_CH2_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel A
    ldi      r2.b0, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
L_EXIT_CH2_IDLE:
    xout     SP_BANK2, &r2, 120-12
    qba      L_CH3_IDLE

; CH2_SOF state comes first as it takes most cycle
; *************************** State CH2_SOF ********************************************
L_CH2_SOF:

; See data pattern / bit count in CH0 version of state, or in the macro

; use macro for send
   m_tx_sof SP_BANK2 STATE_REG.b2

; go to next channel
    qba     L_CH3_IDLE                         ; cc 25

; *************************** State CH2_NEXT1 ******************************************
L_CH2_NEXT1:

; See data pattern / bit count in CH0 version of state, or in the macro

;   transmit macro
    m_tx_next1 SP_BANK2 STATE_REG.b2

; go to next channel
    qba     L_CH3_IDLE                         ; cc 25

; *************************** State CH2_NEXTF ******************************************
L_CH2_NEXTF:

; See data pattern / bit count in CH0 version of state, or in the macro

; need a absolute count and relative count as mod 5 is not easly done

    qbeq     L_CH2_NEXTF1, CNT16_RELATIVE_REG, 0
    qbeq     L_CH2_NEXTF2, CNT16_RELATIVE_REG, 1
    qbeq     L_CH2_NEXTF3, CNT16_RELATIVE_REG, 2
    qbeq     L_CH2_NEXTF4, CNT16_RELATIVE_REG, 3
    qbeq     L_CH2_NEXTF5, CNT16_RELATIVE_REG, 4

; TODO: Add error handling?
; should never reach this point
    halt

L_CH2_NEXTF1:
; transmit macro
    m_tx_nextf1 SP_BANK2

; go to next channel
    qba     L_CH3_IDLE

L_CH2_NEXTF2:
; transmit macro
    m_tx_nextf2 SP_BANK2

; go to next channel
    qba     L_CH3_IDLE

L_CH2_NEXTF3:
; transmit macro
    m_tx_nextf3 SP_BANK2

; go to next channel
    qba     L_CH3_IDLE

L_CH2_NEXTF4:
; transmit macro
    m_tx_nextf4 SP_BANK2

; go to next channel
    qbne    L_SKIP_CH2_EOF_CHECK, CNT16_ABSOLUTE_REG, 54
    ldi     STATE_REG.b2, EOF_STATE
L_SKIP_CH2_EOF_CHECK:
    qba     L_CH3_IDLE

L_CH2_NEXTF5:
; transmit macro
    m_tx_nextf5 SP_BANK2

; go to next channel
    qba     L_CH3_IDLE

; *************************** State CH2_EOF ******************************************
L_CH2_EOF:

; See data pattern / bit count in CH0 version of state, or in the macro

; transmit macro
    m_tx_eof SP_BANK2 STATE_REG.b2

; go to next channel
; set channel status transfer complete
    qbbs     L_CH2_TXB_STATUS_UPDATE, r24.b1, 0
    ldi      r2, STATUS_DONE
    ldi      r3.w0, TXA_CH2_STATUS
    sbco     &r2, c24, r3.w0, 1
; done with ch3, goto ch0
    JMP      L_CH3_IDLE
L_CH2_TXB_STATUS_UPDATE:
    ldi      r2, STATUS_DONE
    ldi      r3.w0, TXB_CH2_STATUS
    sbco     &r2, c24, r3.w0, 1
    qba      L_CH3_IDLE


; **************************** CH3_IDLE ****************************************

L_CH3_IDLE:

; check on THR holding register empty. If not empty go to next channel.
    lbco     &r2.b0, c7, UART_LSR, 1
; holding register is empty if bit is set
    qbbc     L_EXIT_CH3, r2.b0,   5

; check on channel 3 state using permanent state register - r29
    qbeq     L_CH3_SOF, STATE_REG.b3, SOF_STATE

; in IDLE we wait for TX done on wire

; check if transmit shift register is empty if bit is set
; wait if it is not empty
    qbbc     L_EXIT_CH3, r2.b0,   6

; check cmd on active frame data. This should operate in ping pong mode
; xor of buffer_id will alternate the buffers in case both are active.

; r24.b1 has previous buffer, if 0 it was A if 1 it was B
    qbbc     L_CHECK_CH3_B_FIRST,CH3_PREV_BUF, 0

; check A buffer first as previous buffer was B
    ldi      r24.w2, TXA_CH3_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_NO_CH3_ACTIVE_A
    qbeq     L_NO_CH3_ACTIVE_A, r24.b0 , CMD_EMPTY
; found A active, set buffer ptr
    ldi      CH3_BUF_PTR, TX_CH3_A
; claim buffer A in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXA_CH3_STATUS
    sbco     &r2, c24, r3.w0, 1
; set STATE to SOF_STATE
    ldi      STATE_REG.b3, SOF_STATE
; set previous channel to A
    ldi      CH3_PREV_BUF, 0
; clear CMD of channel A
    ldi      r2.b0, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
    qba      L_EXIT_CH3_IDLE
L_NO_CH3_ACTIVE_A:
    ldi      r24.w2, TXB_CH3_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_EXIT_CH3_IDLE
    qbeq     L_EXIT_CH3_IDLE, r24.b0, CMD_EMPTY
; found B active, set buffer ptr
    ldi      CH3_BUF_PTR, TX_CH3_B
; set STATE to CH3
    ldi      STATE_REG.b3, SOF_STATE
; set previous channel to B
    ldi      CH3_PREV_BUF, 1
; claim buffer B in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXB_CH3_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel B
    ldi      r2.b0, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
    qba      L_EXIT_CH3_IDLE

L_CHECK_CH3_B_FIRST:
    ldi      r24.w2, TXB_CH3_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_NO_CH3_ACTIVE_B
    qbeq     L_NO_CH3_ACTIVE_B, r24.b0, CMD_EMPTY
; found B active, set buffer ptr
    ldi      CH3_BUF_PTR, TX_CH3_B
; set STATE to SOF_STATE
    ldi      STATE_REG.b3, SOF_STATE
; set previous channel to B
    ldi      CH3_PREV_BUF, 1
; claim buffer B in STATUS
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXB_CH3_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel B
    ldi      r2, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
    qba      L_EXIT_CH3_IDLE
L_NO_CH3_ACTIVE_B:
    ldi      r24.w2, TXA_CH3_CMD
    lbco     &r24.b0,c24, r24.w2, 1
; if TX_CMD == 0, buffer is not active. Jump to L_EXIT_CH3_IDLE
    qbeq     L_EXIT_CH3_IDLE, r24.b0, CMD_EMPTY
; found A active, set buffer ptr
    ldi      CH3_BUF_PTR, TX_CH3_A
; set STATE to SOF
    ldi      STATE_REG.b3, SOF_STATE
; set previous channel to A
    ldi      CH3_PREV_BUF, 0
; set status
    ldi      r2, STATUS_ACTIVE
    ldi      r3.w0, TXA_CH3_STATUS
    sbco     &r2, c24, r3.w0, 1
; clear CMD of channel A
    ldi      r2.b0, CMD_EMPTY
    sbco     &r2.b0, c24, r24.w2, 1
; goto next channel
L_EXIT_CH3_IDLE:
    JMP      L_CH0_IDLE

; with hardware UART we read and write every cycle if THR is empty
; until we reach the end of frame. At end of frame we need to clear context.
;
L_CH3_SOF:
; data can be taken from CH3_BUF_PTR and length is in CH3_CNT8_REG
    lbco     &r2, c24, CH3_BUF_PTR, 1
; send to THR register
    sbco     &r2, c7, 0 , 1
; update CH3_BUF_PTR which increments by one
    add      CH3_BUF_PTR, CH3_BUF_PTR, 1
    add      CH3_CNT8_REG, CH3_CNT8_REG, 1
; check end_of frame condition = 88 bytes sent
    qbeq     L_CH3_EOF, CH3_CNT8_REG, FRAME_LENGTH
; go to next channel
    jmp      L_CH0_IDLE
L_CH3_EOF:
; reset length counter
    ldi      CH3_CNT8_REG, 0
; set state to IDLE
    ldi      STATE_REG.b3, IDLE_STATE
; set channel status transfer complete
    qbbs     L_CH3_TXB_STATUS_UPDATE, CH3_PREV_BUF, 0
    ldi      r2, STATUS_DONE
    ldi      r3.w0, TXA_CH3_STATUS
    sbco     &r2, c24, r3.w0, 1
; done with ch3, goto ch0
    JMP      L_CH0_IDLE
L_CH3_TXB_STATUS_UPDATE:
    ldi      r2, STATUS_DONE
    ldi      r3.w0, TXB_CH3_STATUS
    sbco     &r2, c24, r3.w0, 1
; done with ch3, goto ch0
L_EXIT_CH3:
    JMP      L_CH0_IDLE

; **********************  end of channel processing  *******************
