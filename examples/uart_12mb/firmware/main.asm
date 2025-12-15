; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2025 Texas Instruments Incorporated - http://www.ti.com/

;   filename:     main_PRU0.h
;   description:  PRU0 implements 4 half-duplex TX UART instances, which can run
;                 at up to 12Mbaud. 3 UART instances implemented with the PRU
;                 peripheral interface, 1 UART instance through the PRU
;                 subsystem's hardware UART instance.
;
;   authors:         Thomas Leyrer       date: 14.10.2025
;                    Nick Saulnier       date: 15.12.2025

; ***********************************************************************


; ******************* includes ******************************************
; macro to convert data to UART to 16 bit TX FIFO data
    .include       "uart_transmit_macro.inc"

; code to do pad configuration
    .include       "pinmux_settings.inc"

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

; leave space for up to 8 buffers per channel

; CH0 CMD & STATUS registers
TX_CH0_CMD0                     .set   0
TX_CH0_CMD1                     .set   0+4
TX_CH0_CMD2                     .set   0+8
TX_CH0_CMD3                     .set   0+12
TX_CH0_CMD4                     .set   0+16
TX_CH0_CMD5                     .set   0+20
TX_CH0_CMD6                     .set   0+24
TX_CH0_CMD7                     .set   0+28

TX_CH0_STATUS0                  .set   32
TX_CH0_STATUS1                  .set   32+4
TX_CH0_STATUS2                  .set   32+8
TX_CH0_STATUS3                  .set   32+12
TX_CH0_STATUS4                  .set   32+16
TX_CH0_STATUS5                  .set   32+20
TX_CH0_STATUS6                  .set   32+24
TX_CH0_STATUS7                  .set   32+28

; CH1 CMD & STATUS registers
TX_CH1_CMD0                     .set   64
TX_CH1_CMD1                     .set   64+4
TX_CH1_CMD2                     .set   64+8
TX_CH1_CMD3                     .set   64+12
TX_CH1_CMD4                     .set   64+16
TX_CH1_CMD5                     .set   64+20
TX_CH1_CMD6                     .set   64+24
TX_CH1_CMD7                     .set   64+28

TX_CH1_STATUS0                  .set   96
TX_CH1_STATUS1                  .set   96+4
TX_CH1_STATUS2                  .set   96+8
TX_CH1_STATUS3                  .set   96+12
TX_CH1_STATUS4                  .set   96+16
TX_CH1_STATUS5                  .set   96+20
TX_CH1_STATUS6                  .set   96+24
TX_CH1_STATUS7                  .set   96+28

; CH2 CMD & STATUS registers
TX_CH2_CMD0                     .set   128
TX_CH2_CMD1                     .set   128+4
TX_CH2_CMD2                     .set   128+8
TX_CH2_CMD3                     .set   128+12
TX_CH2_CMD4                     .set   128+16
TX_CH2_CMD5                     .set   128+20
TX_CH2_CMD6                     .set   128+24
TX_CH2_CMD7                     .set   128+28

TX_CH2_STATUS0                  .set   160
TX_CH2_STATUS1                  .set   160+4
TX_CH2_STATUS2                  .set   160+8
TX_CH2_STATUS3                  .set   160+12
TX_CH2_STATUS4                  .set   160+16
TX_CH2_STATUS5                  .set   160+20
TX_CH2_STATUS6                  .set   160+24
TX_CH2_STATUS7                  .set   160+28

; CH3 CMD & STATUS registers - for HW UART
TX_CH3_CMD0                     .set   192
TX_CH3_CMD1                     .set   192+4
TX_CH3_CMD2                     .set   192+8
TX_CH3_CMD3                     .set   192+12
TX_CH3_CMD4                     .set   192+16
TX_CH3_CMD5                     .set   192+20
TX_CH3_CMD6                     .set   192+24
TX_CH3_CMD7                     .set   192+28

TX_CH3_STATUS0                  .set   224
TX_CH3_STATUS1                  .set   224+4
TX_CH3_STATUS2                  .set   224+8
TX_CH3_STATUS3                  .set   224+12
TX_CH3_STATUS4                  .set   224+16
TX_CH3_STATUS5                  .set   224+20
TX_CH3_STATUS6                  .set   224+24
TX_CH3_STATUS7                  .set   224+28

; leave space for 8 buffers of size 128 Bytes per channel
TX_CH0_BUF0                     .set   4096
TX_CH0_BUF1                     .set   4096 + 128
TX_CH0_BUF2                     .set   4096 + 256
TX_CH0_BUF3                     .set   4096 + 384
TX_CH0_BUF4                     .set   4096 + 512
TX_CH0_BUF5                     .set   4096 + 640
TX_CH0_BUF6                     .set   4096 + 768
TX_CH0_BUF7                     .set   4096 + 896

TX_CH1_BUF0                     .set   5120
TX_CH1_BUF1                     .set   5120 + 128
TX_CH1_BUF2                     .set   5120 + 256
TX_CH1_BUF3                     .set   5120 + 384
TX_CH1_BUF4                     .set   5120 + 512
TX_CH1_BUF5                     .set   5120 + 640
TX_CH1_BUF6                     .set   5120 + 768
TX_CH1_BUF7                     .set   5120 + 896

TX_CH2_BUF0                     .set   6144
TX_CH2_BUF1                     .set   6144 + 128
TX_CH2_BUF2                     .set   6144 + 256
TX_CH2_BUF3                     .set   6144 + 384
TX_CH2_BUF4                     .set   6144 + 512
TX_CH2_BUF5                     .set   6144 + 640
TX_CH2_BUF6                     .set   6144 + 768
TX_CH2_BUF7                     .set   6144 + 896

TX_CH3_BUF0                     .set   7168
TX_CH3_BUF1                     .set   7168 + 128
TX_CH3_BUF2                     .set   7168 + 256
TX_CH3_BUF3                     .set   7168 + 384
TX_CH3_BUF4                     .set   7168 + 512
TX_CH3_BUF5                     .set   7168 + 640
TX_CH3_BUF6                     .set   7168 + 768
TX_CH3_BUF7                     .set   7168 + 896

;TX_CH0_A                        .set   0
;TX_CH0_B                        .set   128
;TXA_CH0_CMD                     .set   256
;TXB_CH0_CMD                     .set   256+4
;TXA_CH0_STATUS                  .set   256+8
;TXB_CH0_STATUS                  .set   256+12

;TX_CH1_A                        .set   512
;TX_CH1_B                        .set   512 + 128
;TXA_CH1_CMD                     .set   512 + 256
;TXB_CH1_CMD                     .set   512 + 256 + 4
;TXA_CH1_STATUS                  .set   512 + 256 + 8
;TXB_CH1_STATUS                  .set   512 + 256 + 12

;TX_CH2_A                        .set   1024
;TX_CH2_B                        .set   1024 + 128
;TXA_CH2_CMD                     .set   1024 + 256
;TXB_CH2_CMD                     .set   1024 + 256 + 4
;TXA_CH2_STATUS                  .set   1024 + 256 + 8
;TXB_CH2_STATUS                  .set   1024 + 256 + 12;

;TX_CH3_A                        .set   1536
;TX_CH3_B                        .set   1536 + 128
;TXA_CH3_CMD                     .set   1536 + 256
;TXB_CH3_CMD                     .set   1536 + 256 + 4
;TXA_CH3_STATUS                  .set   1536 + 256 + 8
;TXB_CH3_STATUS                  .set   1536 + 256 + 12;

CMD_EMPTY                       .set   0   ; no data in this buffer
STATUS_NOT_ACTIVE               .set   0
STATUS_ACTIVE                   .set   1   ; frame is in transmit
STATUS_DONE                     .set   2   ; frame transmit complete

PRU_FW_REV_REG                  .set   0x2000-4
PRU_FW_REV                      .set   0x00010002  ; major.minor (msw = 1, lsw = 2)

; channel states
IDLE_STATE                        .set   1
SOF_STATE                         .set   2
NEXT1_STATE                       .set   3
NEXTF_STATE                       .set   4
EOF_STATE                         .set   8

;*******************************************************************************
; Register definitions
;*******************************************************************************

; R29 register holds channel states
    .asg  r29.b0 ,   CH0_STATE_REG
    .asg  r29.b1 ,   CH1_STATE_REG
    .asg  r29.b2 ,   CH2_STATE_REG
    .asg  r29.b3 ,   CH3_STATE_REG

; R27 & R28 registers are for channel 3 (HW UART)
	.asg  r28.w2 ,   CH3_BUF_PTR_REG
	.asg  r28.w0 ,   CH3_CNT8_REG
	.asg  r27.w2 ,   CH3_CMD_PTR_REG
	.asg  r27.b1 ,   CH3_PREV_BUF

; channels 0, 1, 2 store their context for R0 - R26 in a scratchpad bank

;BUF_PTR_REG                     .set  r26     ; part of context in SP
    .asg  r26.w0 ,   BUF_PTR_REG              ; pointer to data buffer
    .asg  r26.w2 ,   STATUS_PTR_REG           ; pointer to status register

    .asg r25.b0 ,   CNT16_ABSOLUTE_REG        ; absolute count of the number of 16 bit loads
    .asg r25.b1 ,   CNT16_RELATIVE_REG        ; relative count of the number of 16 bit loads
    ; r25.w2 unassigned

; TODO: Replace FRAME_LENGTH with BUF_LENGTH when adding variable length buffers
    .asg r24.b0 ,   BUF_LENGTH                ; number of bytes to read from the TX buffer
        ; CMD register = 0 = CMD_EMPTY (no buffer)
        ; CMD register != 0 = BUF_LENGTH, BUF_LENGTH number of bytes are in buffer
    .asg r24.b1 ,   PREV_BUF                  ; previous buffer used
        ; 0 = previous buffer was A, 1 = B, 2 = C, 3 = D, 4 = E
    .asg r24.w2 ,   CMD_PTR_REG               ; pointer to command register

;CMD_BUFF_REG                    .set  r24     ; part of context in SP
; OLD ASSOCIATIONS
; r24.b0 holds TXn_CHm_CMD (value must be >=8 bits)
; r24.b1 has previous buffer. 0 = previous buffer was A, 1 = previous buffer was B
; r24.w2 holds address offsets of registers to read

; TODO: Finish discussing register usage
; R2-R12 - 44 bytes of data to send (grabbed in state SOF)
; R13-R23: 44 bytes of data to send (grabbed in state NEXT1)
; r0.b0 is saved in the SPAD bank for keeping track of how much scratchpad shift to apply 
; r1.b0 holds the register pointer for mvid shifting

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

;*******************************************************************************
; Initialization
;*******************************************************************************

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


; ********************** configure PRU cycle counter  **************************

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


;************************************* padconfig *******************************
    .if $defined(NOHOST)
    m_apply_padconfig
    .endif


;************************** start with one symbol high 8 bits *************************************

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

; QUESTION: This preloads 16 bits, not 32 bits. Which did you want to load?
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
    ldi       r30.b2, 0         ; select channel 0
; Set R31[18] = 1 (tx_channel_go)
    set       r31, r31, 18

; QUESTION: Where did this math come from?
; it is 2 x 666 cycles from here before fifo runs empty

; enable pads for UART mode now that peripheral interface FIFO is initialized to
; 16 bits of "1"
    m_enable_output_pins


; QUESTION: should these commented lines about MASK be removed?
;    ldi      MASK_REG1, 0x01fe                   ; 0b0000 0001 1111 1110
;    ldi      MASK_REG2, 0xf800                   ; 0b1111 1000 0000 0000
    ldi      BUF_PTR_REG, 0
    ldi      PREV_BUF, 7                           ; set "previous buffer" = 7, so code starts by checking buffer 0

; init CH0, CH1, CH2 context
    xout     SP_BANK0, &r24, 12                  ; R24-R26
    xout     SP_BANK1, &r24, 12                  ; R24-R26
    xout     SP_BANK2, &r24, 12                  ; R24-R26

; init CH3 context
    ldi      CH3_PREV_BUF, 7                       ; set "previous buffer" = 7, so code starts by checking buffer 0
    ldi      CH3_CNT8_REG,  0


;*******************************************************************************
; Main loop - check all channels in round robin
;
; clock budget:
; 12.5 Mbps = 80ns/bit
; writing 16 bits to the FIFO gives 1.28 us for the program to add more data
;     ==> 426 PRU clocks @ 3ns/clock
; writing 8 bits in the FIFO gives 640 ns for the program to add more data
;     ==> 213 PRU clocks @ 3ns/clock
;
; If each UART channel gets equal time, the PRU clocks from L_CHn_IDLE to the
; next L_CHn_IDLE must be < 50 clocks. "cc" comments refer to cycle counts
;*******************************************************************************

;*******************************************************************************
; CH0_IDLE
;
; PEAK cycles:
;     28 cycles to L_CH1_IDLE (STATE = IDLE, PREV_BUF = 7, NEXT buffer active)
;*******************************************************************************
L_CH0_IDLE:

;------------------- check channel state & fifo level --------------------------
; PEAK cycles:
;     7 cycles
;-------------------------------------------------------------------------------

; select channel 0
    ldi      r30.b2, 0             ; select channel 0
; TODO: R0-R26 is pulled from SP_BANKx
; for all channels, the XOUT commands that store registers to SP_BANKx use
; R2-R28 instead. Is that a bug?
; How to store length of bytes left to send across cycles?
; get context for channel 0
    xin      SP_BANK0, &r0, 108   ; get R0-R26 context
; check on fifo tx level
    and      r2.b0, r31.b0, 0x1c    ; bit 2-4 has fifo level. 2 word = 8
    qblt     L_CH1_IDLE, r2.b0, 8   ; if fifo level > 2 word than go to next channel - nothing to do

; check on channel 0 state using permanent state register - r29
    qbeq     L_CH0_SOF,   CH0_STATE_REG, SOF_STATE
    qbeq     L_CH0_NEXT1, CH0_STATE_REG, NEXT1_STATE
    qbeq     L_CH0_NEXTF, CH0_STATE_REG, NEXTF_STATE
    qbeq     L_CH0_EOF,   CH0_STATE_REG, EOF_STATE


;----------------------- check if next buffer has data -------------------------
;
; state is IDLE
;    - add 8 bit of idle at the end of UART frame handler
;    - check if next buffer has data
;    - if next buffer has data, claim buffer & set STATE to SOF_STATE
;
; PEAK cycles:
;     21 cycles to L_CH1_IDLE (if PREV_BUF = 7)
;-------------------------------------------------------------------------------

; no active frame state, add 8 bit of idle at the end of UART frame handler
    ldi      r30.b0, 0xff

; Check the CMD register to see if the next buffer has active frame data
; r2.b0 holds the next buffer that will be filled
; r2.b1 holds the register address offset = r2.b0 x 4

; if PREV_BUF was 7, NEXT_BUF is buffer 0
    qbne     L_CH0_SET_NEXT_BUF, PREV_BUF, 7
    ldi      r2, 0           ; r2.b0 = 0
    qba      L_CH0_CHECK_BUF

; else, NEXT_BUF is PREV_BUF + 1
L_CH0_SET_NEXT_BUF:
    add      r2, PREV_BUF, 1 ; r2.b0 = PREV_BUF + 1

L_CH0_CHECK_BUF:

; load the CMD register for NEXT_BUF
    lsl     r2.b1, r2.b0, 2         ; 4x buffer number = register address offset
    add     CMD_PTR_REG, r2.b1, TX_CH0_CMD0  ; CMD_PTR = base address + offset
    lbco    &r3.b0, c24, CMD_PTR_REG, 1

; jump to L_EXIT_CH0_IDLE if buffer is empty
    qbeq     L_EXIT_CH0_IDLE, r3.b0, CMD_EMPTY

; next buffer is active

; set STATE to SOF_STATE
    ldi      CH0_STATE_REG, SOF_STATE
    mov      PREV_BUF, r2.b0 ; set previous channel to current active channel 

; set register pointers
    lsl      r2.w2, r2.b0, 7         ; 128x buffer number = buffer address offset
    ldi      BUF_PTR_REG, TX_CH0_BUF0 ; TX_CH0_BUF0 is too large to pass in OP(255), so store in register
    add      BUF_PTR_REG, r2.w2, BUF_PTR_REG  ; BUF_PTR = base address + offset
    add      STATUS_PTR_REG, r2.b1, TX_CH0_STATUS0 ; STATUS_PTR = base address + offset

; claim buffer in STATUS register, clear CMD register
; ldi sets r2.b0 & r2.b1 simultaneously:
; r2.b0 = STATUS
; r2.b1 = 0 = CMD_EMPTY
    ldi      r2.w0, STATUS_ACTIVE
    sbco     &r2.b0, c24, STATUS_PTR_REG, 1 ; set channel's STATUS register
    sbco     &r2.b1, c24, CMD_PTR_REG, 1    ; clear channel's CMD register

; go to next channel
L_EXIT_CH0_IDLE:
    xout     SP_BANK0, &r2, 100   ; save R2-R26 context
    qba      L_CH1_IDLE


;*******************************************************************************
; State CH0_SOF
;
; SOF state starts copying data from the TX buffer to the PRU's internal
; registers, and sends the first 16 bits of the buffer (i.e., Start Of Frame)
;
; convert 8 bit data to 10 bit UART symbol with 1 start bit '0' and one stop bit '1'
; UART is LSB first which means the first bit on wire is start bit followed by bit 0 of data bits.
; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     30 cycles from L_CH0_IDLE to L_CH1_IDLE
;         5 cycles to get from IDLE to m_tx_sof
;         24 cycles for m_tx_sof macro
;         1 cycle to qba to next channel
;*******************************************************************************
L_CH0_SOF:

; use macro for send
    m_tx_sof SP_BANK0, CH0_STATE_REG

; go to next channel
    qba      L_CH1_IDLE

;*******************************************************************************
; State CH0_NEXT1
;
; now get back to 2nd 16 bit data which is spread into r2.b1-b3
; Data pattern is: D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
; 
; PEAK cycles:
;     40 cycles from L_CH0_IDLE to L_CH1_IDLE
;         6 cycles to get from IDLE to m_tx_next1
;         33 cycles for m_tx_next1 macro
;         1 cycle to qba to next channel
;*******************************************************************************
L_CH0_NEXT1:

; transmit macro
    m_tx_next1 SP_BANK0, CH0_STATE_REG

; go to next channel
    qba     L_CH1_IDLE

;*******************************************************************************
; State CH0_NEXTF
;
; in this state there are 5 different patterns depending on the 16 bit data count cnt16d
; cnt16d: 0 (first or every 5th)
; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; cnt16d: 1 (second or every 5th +1)
; Data pattern is: D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; cnt16d: 2 (third or every 5th + 2)
; Data pattern is: D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; cnt16d: 3 (fourth or every 5th + 3)
; Data pattern is: D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;
;
; cnt16d: 4 (fifth or every 5th + 2)
; Data pattern is: D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;
;
;*******************************************************************************
L_CH0_NEXTF:

; need a absolute count and relative count as mod 5 is not easily done

    qbeq     L_CH0_NEXTF1, CNT16_RELATIVE_REG, 0
    qbeq     L_CH0_NEXTF2, CNT16_RELATIVE_REG, 1
    qbeq     L_CH0_NEXTF3, CNT16_RELATIVE_REG, 2
    qbeq     L_CH0_NEXTF4, CNT16_RELATIVE_REG, 3
    qbeq     L_CH0_NEXTF5, CNT16_RELATIVE_REG, 4

; TODO: Add error handling?
; should never reach this point
    halt

;-------------------------------------------------------------------------------
; CH0_NEXTF1
;
; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     20 cycles from L_CH0_IDLE to L_CH1_IDLE
;         8 cycles to get from IDLE to m_tx_nextf1
;         11 cycles for m_tx_nextf1 macro
;         1 cycle to qba to next channel
;-------------------------------------------------------------------------------
L_CH0_NEXTF1:

; transmit macro
    m_tx_nextf1 SP_BANK0

; go to next channel
    qba     L_CH1_IDLE

;-------------------------------------------------------------------------------
; CH0_NEXTF2
;
; 2nd 16 bit data which is spread into r2.b1-b3
; Data pattern is: D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     24 cycles from L_CH0_IDLE to L_CH1_IDLE
;         9 cycles to get from IDLE to m_tx_nextf2
;         14 cycles for m_tx_nextf2 macro
;         1 cycle to qba to next channel
;-------------------------------------------------------------------------------
L_CH0_NEXTF2:

; transmit macro
    m_tx_nextf2 SP_BANK0

; go to next channel
    qba     L_CH1_IDLE

;-------------------------------------------------------------------------------
; CH0_NEXTF3
;
; cnt16d: 2 (third or every 5th + 2)
; Data pattern is: D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     21 cycles from L_CH0_IDLE to L_CH1_IDLE
;         10 cycles to get from IDLE to m_tx_nextf3
;         10 cycles for m_tx_nextf3 macro
;         1 cycle to qba to next channel
;-------------------------------------------------------------------------------
L_CH0_NEXTF3:

; transmit macro
    m_tx_nextf3 SP_BANK0

; go to next channel
    qba     L_CH1_IDLE

;-------------------------------------------------------------------------------
; CH0_NEXTF4
;
; cnt16d: 3 (fourth or every 5th + 3)
; Data pattern is: D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;
;
; PEAK cycles:
;     29 cycles from L_CH0_IDLE to L_CH1_IDLE
;         11 cycles to get from IDLE to m_tx_nextf4
;         15 cycles for m_tx_nextf4 macro
;         3 cycle to qba to next channel
;-------------------------------------------------------------------------------
L_CH0_NEXTF4:

; transmit macro
    m_tx_nextf4 SP_BANK0

; go to next channel
    qbne    L_SKIP_EOF_CHECK, CNT16_ABSOLUTE_REG, 54
    ldi     CH0_STATE_REG, EOF_STATE
L_SKIP_EOF_CHECK:
    qba     L_CH1_IDLE

;-------------------------------------------------------------------------------
; CH0_NEXTF5
;
; cnt16d: 4 (fifth or every 5th + 2)
; Data pattern is: D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;
;
; PEAK cycles:
;     25 cycles from L_CH0_IDLE to L_CH1_IDLE
;         12 cycles to get from IDLE to m_tx_nextf5
;         12 cycles for m_tx_nextf5 macro
;         1 cycle to qba to next channel
;-------------------------------------------------------------------------------
L_CH0_NEXTF5:

; transmit macro
    m_tx_nextf5 SP_BANK0

; go to next channel
    qba     L_CH1_IDLE

;*******************************************************************************
; State CH0_EOF
;
; cnt16d: 4 (fifth or every 5th + 2)
; Data pattern is: D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;
;
; PEAK cycles:
;     25 cycles from L_CH0_IDLE to L_CH1_IDLE
;         8 cycles to get from IDLE to m_tx_eof
;         14 cycles for m_tx_eof macro
;         3 cycles to qba to next channel
;*******************************************************************************
L_CH0_EOF:

; transmit macro
    m_tx_eof SP_BANK0, CH0_STATE_REG

; set channel status transfer complete
    ldi      r2, STATUS_DONE
    sbco     &r2, c24, STATUS_PTR_REG, 1

; go to next channel
    qba     L_CH1_IDLE


;*******************************************************************************
; CH1_IDLE
;
; For detailed comments on each section and cycle counts, see CH0_IDLE
;*******************************************************************************
L_CH1_IDLE:

;------------------- check channel state & fifo level --------------------------

; select channel 1
    ldi       r30.b2, 1         ; select channel 1

; get context channel 1
    xin      SP_BANK1, &r0, 108   ; get R0-R26 context
; check on fifo tx level
    and      r2.b0, r31.b1, 0x1c    ; bit 2-4 has fifo level. 2 word = 8
    qblt     L_CH2_IDLE, r2.b0, 8   ; if fifo level > 2 word than go to next channel - nothing to do

; check on channel 1 state using permanent state register - r29
    qbeq     L_CH1_SOF,   CH1_STATE_REG, SOF_STATE
    qbeq     L_CH1_NEXT1, CH1_STATE_REG, NEXT1_STATE
    qbeq     L_CH1_NEXTF, CH1_STATE_REG, NEXTF_STATE
    qbeq     L_CH1_EOF,   CH1_STATE_REG, EOF_STATE

;----------------------- check if next buffer has data -------------------------

; no active frame state, add 8 bit of idle at the end of UART frame handler
    ldi      r30.b0, 0xff

; Check the CMD register to see if the next buffer has active frame data
; r2.b0 holds the next buffer that will be filled
; r2.b1 holds the register address offset = r2.b0 x 4

; if PREV_BUF was 7, NEXT_BUF is buffer 0
    qbne     L_CH1_SET_NEXT_BUF, PREV_BUF, 7
    ldi      r2, 0           ; r2.b0 = 0
    qba      L_CH1_CHECK_BUF

; else, NEXT_BUF is PREV_BUF + 1
L_CH1_SET_NEXT_BUF:
    add      r2, PREV_BUF, 1 ; r2.b0 = PREV_BUF + 1

L_CH1_CHECK_BUF:

; load the CMD register for NEXT_BUF
    lsl     r2.b1, r2.b0, 2         ; 4x buffer number = register address offset
    add     CMD_PTR_REG, r2.b1, TX_CH1_CMD0  ; CMD_PTR = base address + offset
    lbco    &r3.b0, c24, CMD_PTR_REG, 1

; jump to L_EXIT_CH1_IDLE if buffer is empty
    qbeq     L_EXIT_CH1_IDLE, r3.b0, CMD_EMPTY

; next buffer is active

; set STATE to SOF_STATE
    ldi      CH1_STATE_REG, SOF_STATE
    mov      PREV_BUF, r2.b0 ; set previous channel to current active channel 

; set register pointers
    lsl      r2.w2, r2.b0, 7         ; 128x buffer number = buffer address offset
    ldi      BUF_PTR_REG, TX_CH1_BUF0 ; TX_CH1_BUF0 is too large to pass in OP(255), so store in register
    add      BUF_PTR_REG, r2.w2, BUF_PTR_REG  ; BUF_PTR = base address + offset
    add      STATUS_PTR_REG, r2.b1, TX_CH1_STATUS0 ; STATUS_PTR = base address + offset

; claim buffer in STATUS register, clear CMD register
; ldi sets r2.b0 & r2.b1 simultaneously:
; r2.b0 = STATUS
; r2.b1 = 0 = CMD_EMPTY
    ldi      r2.w0, STATUS_ACTIVE
    sbco     &r2.b0, c24, STATUS_PTR_REG, 1 ; set channel's STATUS register
    sbco     &r2.b1, c24, CMD_PTR_REG, 1    ; clear channel's CMD register

; go to next channel
L_EXIT_CH1_IDLE:
    xout     SP_BANK1, &r2, 100   ; save R2-R26 context
    qba      L_CH2_IDLE

; CH1_SOF state comes first as it takes most cycle
; *************************** State CH1_SOF ********************************************
L_CH1_SOF:

; See data pattern / bit count in CH0 version of state, or in the macro

; use macro for send
   m_tx_sof SP_BANK1, CH1_STATE_REG

; go to next channel
    qba     L_CH2_IDLE                         ; cc 25

; *************************** State CH1_NEXT ******************************************
L_CH1_NEXT1:

; See data pattern / bit count in CH0 version of state, or in the macro

;   transmit macro
    m_tx_next1 SP_BANK1, CH1_STATE_REG

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
    ldi     CH1_STATE_REG, EOF_STATE
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
    m_tx_eof SP_BANK1, CH1_STATE_REG

; set channel status transfer complete
    ldi      r2, STATUS_DONE
    sbco     &r2, c24, STATUS_PTR_REG, 1

; go to next channel
    qba     L_CH2_IDLE


;*******************************************************************************
; CH2_IDLE
;
; For detailed comments on each section and cycle counts, see CH0_IDLE
;*******************************************************************************
L_CH2_IDLE:

;------------------- check channel state & fifo level --------------------------

; select channel 2
    ldi       r30.b2, 2         ; select channel 2

; get context channel 2
    xin      SP_BANK2, &r0, 108   ; get R0-R26 context
; check on fifo tx level
    and      r2.b0, r31.b2, 0x1c    ; bit 2-4 has fifo level. 2 word = 8
    qblt     L_CH3_IDLE, r2.b0, 8   ; if fifo level > 2 word than go to next channel - nothing to do

; check on channel 2 state using permanent state register - r29
    qbeq     L_CH2_SOF,   CH2_STATE_REG, SOF_STATE
    qbeq     L_CH2_NEXT1, CH2_STATE_REG, NEXT1_STATE
    qbeq     L_CH2_NEXTF, CH2_STATE_REG, NEXTF_STATE
    qbeq     L_CH2_EOF,   CH2_STATE_REG, EOF_STATE

;----------------------- check if next buffer has data -------------------------

; no active frame state, add 8 bit of idle at the end of UART frame handler
    ldi      r30.b0, 0xff

; Check the CMD register to see if the next buffer has active frame data
; r2.b0 holds the next buffer that will be filled
; r2.b1 holds the register address offset = r2.b0 x 4

; if PREV_BUF was 7, NEXT_BUF is buffer 0
    qbne     L_CH2_SET_NEXT_BUF, PREV_BUF, 7
    ldi      r2, 0           ; r2.b0 = 0
    qba      L_CH2_CHECK_BUF

; else, NEXT_BUF is PREV_BUF + 1
L_CH2_SET_NEXT_BUF:
    add      r2, PREV_BUF, 1 ; r2.b0 = PREV_BUF + 1

L_CH2_CHECK_BUF:

; load the CMD register for NEXT_BUF
    lsl     r2.b1, r2.b0, 2         ; 4x buffer number = register address offset
    add     CMD_PTR_REG, r2.b1, TX_CH2_CMD0  ; CMD_PTR = base address + offset
    lbco    &r3.b0, c24, CMD_PTR_REG, 1

; jump to L_EXIT_CH2_IDLE if buffer is empty
    qbeq     L_EXIT_CH2_IDLE, r3.b0, CMD_EMPTY

; next buffer is active

; set STATE to SOF_STATE
    ldi      CH2_STATE_REG, SOF_STATE
    mov      PREV_BUF, r2.b0 ; set previous channel to current active channel 

; set register pointers
    lsl      r2.w2, r2.b0, 7         ; 128x buffer number = buffer address offset
    ldi      BUF_PTR_REG, TX_CH2_BUF0 ; TX_CH2_BUF0 is too large to pass in OP(255), so store in register
    add      BUF_PTR_REG, r2.w2, BUF_PTR_REG  ; BUF_PTR = base address + offset
    add      STATUS_PTR_REG, r2.b1, TX_CH2_STATUS0 ; STATUS_PTR = base address + offset

; claim buffer in STATUS register, clear CMD register
; ldi sets r2.b0 & r2.b1 simultaneously:
; r2.b0 = STATUS
; r2.b1 = 0 = CMD_EMPTY
    ldi      r2.w0, STATUS_ACTIVE
    sbco     &r2.b0, c24, STATUS_PTR_REG, 1 ; set channel's STATUS register
    sbco     &r2.b1, c24, CMD_PTR_REG, 1    ; clear channel's CMD register

; go to next channel
L_EXIT_CH2_IDLE:
    xout     SP_BANK2, &r2, 100   ; save R2-R26 context
    qba      L_CH3_IDLE

; CH2_SOF state comes first as it takes most cycle
; *************************** State CH2_SOF ********************************************
L_CH2_SOF:

; See data pattern / bit count in CH0 version of state, or in the macro

; use macro for send
   m_tx_sof SP_BANK2, CH2_STATE_REG

; go to next channel
    qba     L_CH3_IDLE                         ; cc 25

; *************************** State CH2_NEXT1 ******************************************
L_CH2_NEXT1:

; See data pattern / bit count in CH0 version of state, or in the macro

;   transmit macro
    m_tx_next1 SP_BANK2, CH2_STATE_REG

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
    ldi     CH2_STATE_REG, EOF_STATE
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
    m_tx_eof SP_BANK2, CH2_STATE_REG

; set channel status transfer complete
    ldi      r2, STATUS_DONE
    sbco     &r2, c24, STATUS_PTR_REG, 1

; go to next channel
    qba      L_CH3_IDLE


; **************************** CH3_IDLE ****************************************

L_CH3_IDLE:

; check on THR holding register empty. If not empty go to next channel.
    lbco     &r2.b0, c7, UART_LSR, 1
; holding register is empty if bit is set
    qbbc     L_EXIT_CH3, r2.b0,   5

; check on channel 3 state using permanent state register - r29
    qbeq     L_CH3_SOF, CH3_STATE_REG, SOF_STATE

; in IDLE we wait for TX done on wire

; check if transmit shift register is empty if bit is set
; wait if it is not empty
    qbbc     L_EXIT_CH3, r2.b0,   6

; Check the CMD register to see if the next buffer has active frame data
;------------------------------------------------------------------------
; r2.b0 holds the next buffer that will be filled
; r2.b1 holds the register address offset = r2.b0 x 4

; if CH3_PREV_BUF was 7, NEXT_BUF is buffer 0
    qbne     L_CH3_SET_NEXT_BUF, CH3_PREV_BUF, 7
    ldi      r2, 0           ; r2.b0 = 0
    qba      L_CH3_CHECK_BUF

; else, NEXT_BUF is CH3_PREV_BUF + 1
L_CH3_SET_NEXT_BUF:
    add      r2, CH3_PREV_BUF, 1 ; r2.b0 = CH3_PREV_BUF + 1

L_CH3_CHECK_BUF:

; load the CMD register for NEXT_BUF
    lsl     r2.b1, r2.b0, 2         ; 4x buffer number = register address offset
    add     CH3_CMD_PTR_REG, r2.b1, TX_CH3_CMD0  ; CMD_PTR = base address + offset
    lbco    &r3.b0, c24, CH3_CMD_PTR_REG, 1

; jump to L_EXIT_CH3_IDLE if buffer is empty
    qbeq     L_EXIT_CH3_IDLE, r3.b0, CMD_EMPTY

; next buffer is active

; set STATE to SOF_STATE
    ldi      CH3_STATE_REG, SOF_STATE
    mov      CH3_PREV_BUF, r2.b0 ; set previous channel to current active channel 

; set register pointers
; STATUS_PTR_REG name is reused, but CH3 does not have a permanent STATUS_PTR_REG
    lsl      r2.w2, r2.b0, 7         ; 128x buffer number = buffer address offset
    ldi      CH3_BUF_PTR_REG, TX_CH3_BUF0 ; TX_CH3_BUF0 is too large to pass in OP(255), so store in register
    add      CH3_BUF_PTR_REG, r2.w2, CH3_BUF_PTR_REG  ; BUF_PTR = base address + offset
    add      STATUS_PTR_REG, r2.b1, TX_CH3_STATUS0 ; STATUS_PTR = base address + offset

; claim buffer in STATUS register, clear CMD register
; ldi sets r2.b0 & r2.b1 simultaneously:
; r2.b0 = STATUS
; r2.b1 = 0 = CMD_EMPTY
    ldi      r2.w0, STATUS_ACTIVE
    sbco     &r2.b0, c24, STATUS_PTR_REG, 1 ; set channel's STATUS register
    sbco     &r2.b1, c24, CH3_CMD_PTR_REG, 1    ; clear channel's CMD register

; go to next channel
L_EXIT_CH3_IDLE:
    JMP      L_CH0_IDLE

; with hardware UART we read and write every cycle if THR is empty
; until we reach the end of frame. At end of frame we need to clear context.
;
L_CH3_SOF:
; data can be taken from CH3_BUF_PTR_REG and length is in CH3_CNT8_REG
    lbco     &r2, c24, CH3_BUF_PTR_REG, 1
; send to THR register
    sbco     &r2, c7, 0 , 1
; update CH3_BUF_PTR_REG which increments by one
    add      CH3_BUF_PTR_REG, CH3_BUF_PTR_REG, 1
    add      CH3_CNT8_REG, CH3_CNT8_REG, 1
; check end_of frame condition = 88 bytes sent
    qbeq     L_CH3_EOF, CH3_CNT8_REG, FRAME_LENGTH
; go to next channel
    jmp      L_CH0_IDLE
L_CH3_EOF:
; reset length counter
    ldi      CH3_CNT8_REG, 0
; set state to IDLE
    ldi      CH3_STATE_REG, IDLE_STATE

; set channel status transfer complete
; HW UART does not have a permanent STATUS_PTR_REG, so recalculate register offset
; CH3_PREV_BUF holds the currently active buffer
    lsl      r2.b1, CH3_PREV_BUF, 2         ; 4x buffer number = register address offset
    add      STATUS_PTR_REG, r2.b1, TX_CH3_STATUS0 ; STATUS_PTR = base address + offset
    ldi      r2, STATUS_DONE
    sbco     &r2, c24, STATUS_PTR_REG, 1

; go to next channel
L_EXIT_CH3:
    JMP      L_CH0_IDLE

; **********************  end of channel processing  *******************
