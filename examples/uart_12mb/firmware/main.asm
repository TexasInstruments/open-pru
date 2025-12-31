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
;
; ***********************************************************************

; TODOs
;
; 1) Add support for variable length buffers
;
;    a) remove SPAD shift setting and directly load data into R13-23
;
;    b) test for length, and call special function if length = 1 (how many bytes
;       to have a special function to get through it?)
; in 5 states we send 8 bytes (2 words), so let's hardcode the logic
; 

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

; TODO: remove FRAME_LENGTH after adding support for variable length messages
; max frame length is 88 Bytes
; actual frame length is passed through the TXm_CHn_CMD register
FRAME_LENGTH                    .set   88

; buffer pointer offset in ICSS DATA RAM 0 (c24 base address)
; 8 * 128 bytes = 1 KB
; group channel 0 data and control into one memory block for linux driver simplification
; command register in ICSS DATA RAM 0
; use different 32 bit values as host may do only 32 bit access 

; leave space for up to 8 buffers per channel

; TODO: Need to configure the constant table register for SMEM

; 16 bit register addresses
TX_CH0_READ_PTR             .set   0
TX_CH0_WRITE_PTR            .set   2
TX_CH1_READ_PTR             .set   4
TX_CH1_WRITE_PTR            .set   6
TX_CH2_READ_PTR             .set   8
TX_CH2_WRITE_PTR            .set   10
TX_CH3_READ_PTR             .set   12
TX_CH4_WRITE_PTR            .set   14

; buffer addresses
TX_CH0_BUFFER_START             .set   0x10000
TX_CH0_BUFFER_END               .set   0x10800
TX_CH1_BUFFER_START             .set   0x10800
TX_CH1_BUFFER_END               .set   0x11000
TX_CH2_BUFFER_START             .set   0x11000
TX_CH2_BUFFER_END               .set   0x11800
TX_CH3_BUFFER_START             .set   0x11800
TX_CH3_BUFFER_END               .set   0x12000

PRU_FW_REV_REG                  .set   0x2000-4
PRU_FW_REV                      .set   0x00010003  ; major.minor (msw = 1, lsw = 2)

; channel states
IDLE_STATE                      .set   1
TX1_STATE                       .set   2
TX2_STATE                       .set   3
TX3_STATE                       .set   4
TX4_STATE                       .set   5
TX5_STATE                       .set   6
EOF_STATE                       .set   8

;*******************************************************************************
; Register definitions
;*******************************************************************************

; R29 register holds channel states
    .asg  r29.b0 ,   CH0_STATE_REG
    .asg  r29.b1 ,   CH1_STATE_REG
    .asg  r29.b2 ,   CH2_STATE_REG
    .asg  r29.b3 ,   CH3_STATE_REG

; TODO: Is it sufficient to replace CH3_CMD_PTR & CH3_PREV_BUF
; with CH3_READ_PTR & CH3_READ_PTR, or need more variables?
; R27 & R28 registers are for channel 3 (HW UART)
	.asg  r28.w2 ,   CH3_READ_PTR
	.asg  r28.w0 ,   CH3_CNT8_REG
	.asg  r27.w2 ,   CH3_CMD_PTR_REG
	.asg  r27.b1 ,   CH3_PREV_BUF

; TODO: should I 
; change channels 0-2 to use registers 0 - 24 to give
; HW UART more registers to play with
; channels 0, 1, 2 store their context for R0 - R24 in a scratchpad bank

    .asg  r26.w0 ,   READ_PTR             ; buffer's read pointer
    .asg  r26.w2 ,   WRITE_PTR            ; buffer's write pointer

    .asg r25.b0 ,   CNT_BYTES_SENT        ; count of whole bytes sent
    .asg r25.b1 ,   CNT_BYTES_LOADED      ; count of bytes loaded into registers
    .asg r25.w2 ,   READ_PTR_ADDRESS      ; address of the READ_PTR

    .asg r24.w0 ,   BUFFER_START          ; start address of the buffer
    .asg r24.w2 ,   BUFFER_END            ; end address of the buffer

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

; TODO: check all usages of c24 (DMEM0 for PRU0, DMEM1 for PRU1). Should I
; update the SMEM buffer addresses to use c28 instead? If so, do I need to
; change the  buffer addresses to account for the offset of 0x10000?

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

; TODO: Does the loop command unroll? No, add to documentation 
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

; TODO: Remove shift mode
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

    ldi       r30.b2, 0         ; select channel 0
; pre load TX fifo - 16 bit
    ldi       r30.b0, 0xff
    ldi       r30.b0, 0xff
; select channel 1
    ldi       r30.b2, 1         ; select channel 1
; pre load TX fifo - 16 bit
    ldi       r30.b0, 0xff
    ldi       r30.b0, 0xff
; select channel 2
    ldi       r30.b2, 2         ; select channel 2
; pre load TX fifo - 16 bit
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

; TODO: update comment to generalize
; it is 2 x 666 cycles from here before fifo runs empty

; enable pads for UART mode now that peripheral interface FIFO is initialized to
; 16 bits of "1"
    m_enable_output_pins

; init shared values
    ldi     CNT_BYTES_SENT, 0
    ldi     CNT_BYTES_LOADED, 0

; init CH0 context
    ldi     READ_PTR, TX_CH0_BUFFER_START
    ldi     WRITE_PTR, TX_CH0_BUFFER_START
    ldi     BUFFER_START, TX_CH0_BUFFER_START
    ldi     BUFFER_END, TX_CH0_BUFFER_END
    ldi     READ_PTR_ADDRESS, TX_CH0_READ_PTR
    xout     SP_BANK0, &r24, 12                  ; R24-R26

; init CH1 context
    ldi     READ_PTR, TX_CH1_BUFFER_START
    ldi     WRITE_PTR, TX_CH1_BUFFER_START
    ldi     BUFFER_START, TX_CH1_BUFFER_START
    ldi     BUFFER_END, TX_CH1_BUFFER_END
    ldi     READ_PTR_ADDRESS, TX_CH1_READ_PTR
    xout     SP_BANK1, &r24, 12                  ; R24-R26

; init CH2 context
    ldi     READ_PTR, TX_CH2_BUFFER_START
    ldi     WRITE_PTR, TX_CH2_BUFFER_START
    ldi     BUFFER_START, TX_CH2_BUFFER_START
    ldi     BUFFER_END, TX_CH2_BUFFER_END
    ldi     READ_PTR_ADDRESS, TX_CH2_READ_PTR
    xout     SP_BANK2, &r24, 12                  ; R24-R26

; init CH3 context
; TODO: Figure out what to do for HW UART
;    ldi     READ_PTR, 
;    ldi     WRITE_PTR, 
;    ldi     BUFFER_START, 
;    ldi     BUFFER_END, 
;    ldi     READ_PTR_ADDRESS, 

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

; get context for channel 0
    xin      SP_BANK0, &r0, 108   ; get R0-R26 context

; check on fifo tx level
    and      r2.b0, r31.b0, 0x1c    ; bit 2-4 has fifo level. 2 word = 8
    qblt     L_CH1_IDLE, r2.b0, 8   ; if fifo level > 2 word than go to next channel - nothing to do

; TODO: Replace this with a LUT?
; TODO: How to get back from a LUT jump to the IDLE states?
; check on channel 0 state using permanent state register - r29
    qbeq     L_CH0_TX1,   CH0_STATE_REG, TX1_STATE
    qbeq     L_CH0_TX2,   CH0_STATE_REG, TX2_STATE
    qbeq     L_CH0_TX3,   CH0_STATE_REG, TX3_STATE
    qbeq     L_CH0_TX4,   CH0_STATE_REG, TX4_STATE
    qbeq     L_CH0_TX5,   CH0_STATE_REG, TX5_STATE

;----------------------- check if next buffer has data -------------------------
;
; state is IDLE
;    - add 8 bit of idle at the end of UART frame handler
;    - check if there is more data to transmit
;    - if there is data to transmit, set STATE to TX1_STATE
;
; PEAK cycles:
;     ?? cycles to L_CH1_IDLE
;-------------------------------------------------------------------------------

; no active frame state, add 8 bit of idle at the end of UART frame handler
    ldi      r30.b0, 0xff

; Check to see if the buffer has active data
; r26 has both READ_PTR (w0) & WRITE_PTR (w2)
    lbco    &r26, c24, TX_CH0_READ_PTR, 4

; if READ_PTR == WRITE_PTR, buffer is empty
    qbeq    L_EXIT_CH0_IDLE, READ_PTR, WRITE_PTR

; else, the buffer has active data

; set STATE to TX1_STATE
    ldi      CH0_STATE_REG, TX1_STATE

; go to next channel
L_EXIT_CH0_IDLE:
    xout     SP_BANK0, &r2, 100   ; save R2-R26 context
    qba      L_CH1_IDLE


;*******************************************************************************
; State CH0_TX1
;
; TX1 copies 1 to 8 bytes of data from the TX buffer to the PRU's internal
; registers, and sends the first 16 bits of the buffer. If only 1 byte of
; data was copied, EOF_BYTE1 is called to fill the rest of the 16 bit FIFO with
; 1s. The EOF function then checks for additional buffer data, like in IDLE.
;
; convert 8 bit data to 10 bit UART symbol with 1 start bit '0' and one stop bit '1'
; UART is LSB first which means the first bit on wire is start bit followed by bit 0 of data bits.
; Data pattern is: Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
;
; PEAK cycles:
;     ?? cycles from L_CH0_IDLE to L_CH1_IDLE
;         5 cycles to get from IDLE to m_tx1
;         33 cycles max for m_tx1 macro
;             32 cycles for a wrapped buffer copy
;             25 cycles for a non-wrapped buffer copy
;             25 for EOF_BYTE1
;                 12 to m_eof_byte1
;                 13 for m_eof_byte1
;             1 cycle to qba to leave macro
;         1 cycle to qba to next channel
;*******************************************************************************
L_CH0_TX1:

; use macro for send
    m_tx1 SP_BANK0, CH0_STATE_REG

; go to next channel
    qba      L_CH1_IDLE

;*******************************************************************************
; State CH0_TX2
;
; TX2 sends the next 16 bits of the buffer. If only 2 or 3 bytes of
; data was copied, EOF_BYTE2 or EOF_BYTE3 is called to fill the rest of
; the FIFO with 1s (EOF2 only fills 8 bits of the FIFO, EOF3 fills 16 bits).
; The EOF function then checks for additional buffer data, like in IDLE.
;
; now get back to 2nd 16 bit data which is spread into r2.b1-b3
; Data pattern is: D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
; 
; TODO, fill cycles
; PEAK cycles:
;     ?? cycles from L_CH0_IDLE to L_CH1_IDLE
;         6 cycles to get from IDLE to m_tx_next1
;         33 cycles for m_tx_next1 macro
;         1 cycle to qba to next channel
;*******************************************************************************
L_CH0_TX2:

; transmit macro
    m_tx2 SP_BANK0, CH0_STATE_REG

; go to next channel
    qba     L_CH1_IDLE


;*******************************************************************************
; State CH0_TX3
;
; TX3 sends the next 16 bits of the buffer. If only 4 bytes of
; data was copied, EOF_BYTE4 is called to fill the rest of
; the 16 bit FIFO with 1s.
; The EOF function then checks for additional buffer data, like in IDLE.
;
; cnt16d: 2 (third or every 5th + 2)
; Data pattern is: D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
; 
; TODO, fill cycles
; PEAK cycles:
;     ?? cycles from L_CH0_IDLE to L_CH1_IDLE
;         6 cycles to get from IDLE to m_tx_next1
;         33 cycles for m_tx_next1 macro
;         1 cycle to qba to next channel
;*******************************************************************************
L_CH0_TX3:

; transmit macro
    m_tx3 SP_BANK0, CH0_STATE_REG

; go to next channel
    qba     L_CH1_IDLE


;*******************************************************************************
; State CH0_TX4
;
; TX4 sends the next 16 bits of the buffer. If only 5 or 6 bytes of
; data was copied, EOF_BYTE5 or EOF_BYTE6 is called to fill the rest of
; the FIFO with 1s (EOF5 only fills 8 bits of the FIFO, EOF6 fills 16 bits).
; The EOF function then checks for additional buffer data, like in IDLE.
;
; cnt16d: 3 (fourth or every 5th + 3)
; Data pattern is: D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So Sa D0 D1 D2  (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;
; 
; TODO, fill cycles
; PEAK cycles:
;     ?? cycles from L_CH0_IDLE to L_CH1_IDLE
;         6 cycles to get from IDLE to m_tx_next1
;         33 cycles for m_tx_next1 macro
;         1 cycle to qba to next channel
;*******************************************************************************
L_CH0_TX4:

; transmit macro
    m_tx4 SP_BANK0, CH0_STATE_REG

; go to next channel
    qba     L_CH1_IDLE


;*******************************************************************************
; State CH0_TX5
;
; TX5 sends the last 16 bits of the buffer. If only 7 bytes of
; data was copied, EOF_BYTE7 is called to fill the rest of
; the FIFO with 1s (EOF7 only fills 8 bits of the FIFO. Otherwise, EOF_BYTE8
; is called to fill the last 16 bits of the FIFO.
; The EOF function then checks for additional buffer data, like in IDLE.
;
; cnt16d: 4 (fifth or every 5th + 2)
; Data pattern is: D3 D4 D5 D6 D7 So Sa D0 D1 D2 D3 D4 D5 D6 D7 So (16 bit)
; bit count        1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16;
; 
; TODO, fill cycles
; PEAK cycles:
;     ?? cycles from L_CH0_IDLE to L_CH1_IDLE
;         6 cycles to get from IDLE to m_tx_next1
;         33 cycles for m_tx_next1 macro
;         1 cycle to qba to next channel
;*******************************************************************************
L_CH0_TX5:

; transmit macro
    m_tx5 SP_BANK0, CH0_STATE_REG

; go to next channel
    qba     L_CH1_IDLE


; TODO: Did not write the code for Channel 1 - 3

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
    ldi      READ_PTR, TX_CH1_BUF0 ; TX_CH1_BUF0 is too large to pass in OP(255), so store in register
    add      READ_PTR, r2.w2, READ_PTR  ; BUF_PTR = base address + offset
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
    ldi      READ_PTR, TX_CH2_BUF0 ; TX_CH2_BUF0 is too large to pass in OP(255), so store in register
    add      READ_PTR, r2.w2, READ_PTR  ; BUF_PTR = base address + offset
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
    ldi      CH3_READ_PTR, TX_CH3_BUF0 ; TX_CH3_BUF0 is too large to pass in OP(255), so store in register
    add      CH3_READ_PTR, r2.w2, CH3_READ_PTR  ; BUF_PTR = base address + offset
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
; data can be taken from CH3_READ_PTR and length is in CH3_CNT8_REG
    lbco     &r2, c24, CH3_READ_PTR, 1
; send to THR register
    sbco     &r2, c7, 0 , 1
; update CH3_READ_PTR which increments by one
    add      CH3_READ_PTR, CH3_READ_PTR, 1
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
