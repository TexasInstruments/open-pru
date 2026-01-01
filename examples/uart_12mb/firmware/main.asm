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

; ******************* includes ******************************************
; macros to convert data to UART to 16 bit TX FIFO data
    .include       "uart_transmit_macro.inc"

; code to do pad configuration
    .include       "pinmux_settings.inc"

; ****************** defines / macros ***********************************

; Scratch pad banks
SP_BANK0                        .set   10
SP_BANK1                        .set   11
SP_BANK2                        .set   12

; TODO: Need to configure the constant table register for SMEM

; 16 bit register addresses
TX_CH0_READ_PTR             .set   0
TX_CH0_WRITE_PTR            .set   2
TX_CH1_READ_PTR             .set   4
TX_CH1_WRITE_PTR            .set   6
TX_CH2_READ_PTR             .set   8
TX_CH2_WRITE_PTR            .set   10
TX_CH3_READ_PTR             .set   12
TX_CH3_WRITE_PTR            .set   14

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

;*******************************************************************************
; Register definitions
;*******************************************************************************

; TODO: Update register usage summary (channels 0-2 only use r0-r24, channel 3 now uses r25-r28)
; Register Usage Summary (R0-R29)
;
;  Based on the current modified code:
;
;  ---
;  Permanent Registers (Never Context-Switched)
;
;  R29 - Channel State Register (permanent across all channels)
;  - r29.b0 = CH0_STATE_REG
;  - r29.b1 = CH1_STATE_REG
;  - r29.b2 = CH2_STATE_REG
;  - r29.b3 = CH3_STATE_REG
;
;  R27-R28 - Channel 3 (HW UART) Context (permanent)
;  - r28.w0 = CH3_WRITE_PTR (write pointer, loaded from memory each cycle)
;  - r28.w2 = CH3_READ_PTR (read pointer in circular buffer)
;
;  R30 - PRU GPIO/Peripheral Output (hardware register)
;  - r30.b0 = TX FIFO data output
;  - r30.b2 = Channel select
;
;  R31 - PRU GPIO/Peripheral Input (hardware register)
;  - r31.b0-b2 = FIFO status for channels 0-2
;  - r31[18] = tx_channel_go bit
;
;  ---
;
;  Context-Switched Registers (R0-R24 for Channels 0-2)
;
;  Saved/restored via xin/xout (100 bytes = R0-R24):
;
;  R24 - Buffer Configuration
;  - r24.w0 = BUFFER_START
;  - r24.w2 = BUFFER_END
;
;  R25 - Counters and Pointers
;  - r25.b0  (UNUSED)
;  - r25.b1 = CNT_BYTES_LOADED (1-8 bytes loaded from buffer)
;  - r25.w2 = READ_PTR_ADDRESS
;
;  R26 - Buffer Pointers
;  - r26.w0 = READ_PTR
;  - r26.w2 = WRITE_PTR
;
;  R0-R1 - Temporary/Working Registers
;  - r0.b0-b3 = Temporary bit manipulation, UART symbol formation
;  - r1.b0 = Register pointer for mvid instruction
;    - m_tx3 initializes to 10 (line 384)
;    - m_tx4 increments by 2 to 12 (line 490)
;    - m_tx2, m_tx5 don't use r1.b0 (data already in r2 from previous state)
;  - r1.b2, r1.b3 = Temporary bit manipulation
;  - r1.w2 = Temporary address calculations (wraparound checks)
;
;  R2-R3 - Primary Data Storage
;  - Loaded in m_tx1: up to 8 bytes (r2.b0-r3.b3)
;    - m_tx1: transmits bytes 0-1
;    - m_tx2: uses r2.b1-r2.b3 (bytes 1-3) from m_tx1
;    - m_tx3: reloads r2 from offset 10 (bytes 2-5)
;    - m_tx4: reloads r2 from offset 12 (bytes 4-7)
;    - m_tx5: uses r2.b2-r2.b3 (bytes 6-7) from m_tx4
;
;  R4-R5 - Wrapped Buffer Data Storage (m_tx1 only)
;  - Used to hold data from second chunk when buffer wraps
;
;  R6 - Buffer Wrap Calculations (m_tx1 only)
;  - r6.w0 = Bytes to copy through BUFFER_END
;  - r6.w2 = Remaining bytes to copy from BUFFER_START
;
;  R7-R23 - Available/Unused


; R29 register holds channel states
    .asg  r29.b0 ,   CH0_STATE_REG
    .asg  r29.b1 ,   CH1_STATE_REG
    .asg  r29.b2 ,   CH2_STATE_REG
    .asg  r29.b3 ,   CH3_STATE_REG

; R25 - R28 registers are for channel 3 (HW UART)
; R25 & R26.w0 currently unused
    .asg  r28.w0 ,   CH3_READ_PTR
    .asg  r28.w2 ,   CH3_WRITE_PTR

    .asg  r27.w0 ,   CH3_BUFFER_START
    .asg  r27.w2 ,   CH3_BUFFER_END

    .asg  r26.w2 ,   CH3_READ_PTR_ADDRESS

; channels 0, 1, 2 store their context for R0 - R24 in a scratchpad bank

    .asg  r24.w0 ,   READ_PTR             ; buffer's read pointer
    .asg  r24.w2 ,   WRITE_PTR            ; buffer's write pointer

    .asg r23.b1 ,   CNT_BYTES_LOADED      ; count of bytes loaded into registers
    .asg r23.w2 ,   READ_PTR_ADDRESS      ; address of the READ_PTR

    .asg r22.w0 ,   BUFFER_START          ; start address of the buffer
    .asg r22.w2 ,   BUFFER_END            ; end address of the buffer


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

; QUESTION: Why set to non-FIFO mode? Wouldn't it be better to set to FIFO mode
; in order to allow us to use
; the 16 byte version of THR to give ourselves some wiggle room on both
; transmit and receive?

; Design idea for v1.4: main domain UART0, UART1, UART2 have direct interrupt
; connectivity to PRU subsystem. Could we use interrupts so that PRU doesn't have
; to execute a read in order to see if a UART is ready to send more data?
; How would that work with the other PRU core using an interrupt to see if the
; UART has data to receive? Need to get around the blocking read issue,
; unless there is some way to use DMA or XFR2VBUS to pull in data when triggered?

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
    sbco    &READ_PTR, c24, TX_CH0_READ_PTR, 4      ; store READ_PTR and WRITE_PTR to memory (4 bytes)
    xout     SP_BANK0, &r22, 12                     ; R22-R24

; init CH1 context
    ldi     READ_PTR, TX_CH1_BUFFER_START
    ldi     WRITE_PTR, TX_CH1_BUFFER_START
    ldi     BUFFER_START, TX_CH1_BUFFER_START
    ldi     BUFFER_END, TX_CH1_BUFFER_END
    ldi     READ_PTR_ADDRESS, TX_CH1_READ_PTR
    sbco    &READ_PTR, c24, TX_CH1_READ_PTR, 4      ; store READ_PTR and WRITE_PTR to memory (4 bytes)
    xout     SP_BANK1, &r22, 12                     ; R22-R24

; init CH2 context
    ldi     READ_PTR, TX_CH2_BUFFER_START
    ldi     WRITE_PTR, TX_CH2_BUFFER_START
    ldi     BUFFER_START, TX_CH2_BUFFER_START
    ldi     BUFFER_END, TX_CH2_BUFFER_END
    ldi     READ_PTR_ADDRESS, TX_CH2_READ_PTR
    sbco    &READ_PTR, c24, TX_CH2_READ_PTR, 4      ; store READ_PTR and WRITE_PTR to memory (4 bytes)
    xout     SP_BANK2, &r22, 12                     ; R22-R24

; init CH3 context
; Initialize Channel 3 permanent registers and write pointers to memory for host communication
    ldi     CH3_BUFFER_START, TX_CH3_BUFFER_START
    ldi     CH3_BUFFER_END, TX_CH3_BUFFER_END
    ldi     CH3_READ_PTR_ADDRESS, TX_CH3_READ_PTR
    ldi     CH3_READ_PTR, TX_CH3_BUFFER_START
    ldi     CH3_WRITE_PTR, TX_CH3_BUFFER_START
    sbco    &CH3_READ_PTR, c24, TX_CH3_READ_PTR, 4  ; store READ_PTR and WRITE_PTR to memory (4 bytes)

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

; TODO: If using a 200MHz clock, 640ns / 5ns/clk = 128 clocks
;    ==> 32 PRU clocks per channel
; Is there a way for us to get every channel at 30 clocks or less? This would
; allow us to do 12.5Mbps with the PRU core clock instead of modifying PLLs

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
    xin      SP_BANK0, &r0, 100   ; get R0-R24 context

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
    lbco    &r26, c24, READ_PTR_ADDRESS, 4

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

; TODO: How many cycles does it take to get through TX1 now?
; is there any way to reduce the number of cycles?


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


;*******************************************************************************
; CH1_IDLE
;
; For detailed comments on each section and cycle counts, see CH0_IDLE
;*******************************************************************************
L_CH1_IDLE:

;------------------- check channel state & fifo level --------------------------

; select channel 1
    ldi      r30.b2, 1             ; select channel 1

; get context for channel 1
    xin      SP_BANK1, &r0, 100   ; get R0-R24 context

; check on fifo tx level
    and      r2.b0, r31.b1, 0x1c    ; bit 2-4 has fifo level. 2 word = 8
    qblt     L_CH2_IDLE, r2.b0, 8   ; if fifo level > 2 word than go to next channel - nothing to do

; check on channel 1 state using permanent state register - r29
    qbeq     L_CH1_TX1,   CH1_STATE_REG, TX1_STATE
    qbeq     L_CH1_TX2,   CH1_STATE_REG, TX2_STATE
    qbeq     L_CH1_TX3,   CH1_STATE_REG, TX3_STATE
    qbeq     L_CH1_TX4,   CH1_STATE_REG, TX4_STATE
    qbeq     L_CH1_TX5,   CH1_STATE_REG, TX5_STATE

;----------------------- check if next buffer has data -------------------------
;
; state is IDLE
;    - add 8 bit of idle at the end of UART frame handler
;    - check if there is more data to transmit
;    - if there is data to transmit, set STATE to TX1_STATE
;
;-------------------------------------------------------------------------------

; no active frame state, add 8 bit of idle at the end of UART frame handler
    ldi      r30.b0, 0xff

; Check to see if the buffer has active data
; r26 has both READ_PTR (w0) & WRITE_PTR (w2)
    lbco    &r26, c24, READ_PTR_ADDRESS, 4

; if READ_PTR == WRITE_PTR, buffer is empty
    qbeq    L_EXIT_CH1_IDLE, READ_PTR, WRITE_PTR

; else, the buffer has active data

; set STATE to TX1_STATE
    ldi      CH1_STATE_REG, TX1_STATE

; go to next channel
L_EXIT_CH1_IDLE:
    xout     SP_BANK1, &r2, 100   ; save R2-R26 context
    qba      L_CH2_IDLE


;*******************************************************************************
; State CH1_TX1
;
; For detailed comments, see CH0_TX1
;*******************************************************************************
L_CH1_TX1:

; use macro for send
    m_tx1 SP_BANK1, CH1_STATE_REG

; go to next channel
    qba      L_CH2_IDLE

;*******************************************************************************
; State CH1_TX2
;
; For detailed comments, see CH0_TX2
;*******************************************************************************
L_CH1_TX2:

; transmit macro
    m_tx2 SP_BANK1, CH1_STATE_REG

; go to next channel
    qba     L_CH2_IDLE


;*******************************************************************************
; State CH1_TX3
;
; For detailed comments, see CH0_TX3
;*******************************************************************************
L_CH1_TX3:

; transmit macro
    m_tx3 SP_BANK1, CH1_STATE_REG

; go to next channel
    qba     L_CH2_IDLE


;*******************************************************************************
; State CH1_TX4
;
; For detailed comments, see CH0_TX4
;*******************************************************************************
L_CH1_TX4:

; transmit macro
    m_tx4 SP_BANK1, CH1_STATE_REG

; go to next channel
    qba     L_CH2_IDLE


;*******************************************************************************
; State CH1_TX5
;
; For detailed comments, see CH0_TX5
;*******************************************************************************
L_CH1_TX5:

; transmit macro
    m_tx5 SP_BANK1, CH1_STATE_REG

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
    ldi      r30.b2, 2             ; select channel 2

; get context for channel 2
    xin      SP_BANK2, &r0, 100   ; get R0-R24 context

; check on fifo tx level
    and      r2.b0, r31.b2, 0x1c    ; bit 2-4 has fifo level. 2 word = 8
    qblt     L_CH3_IDLE, r2.b0, 8   ; if fifo level > 2 word than go to next channel - nothing to do

; check on channel 2 state using permanent state register - r29
    qbeq     L_CH2_TX1,   CH2_STATE_REG, TX1_STATE
    qbeq     L_CH2_TX2,   CH2_STATE_REG, TX2_STATE
    qbeq     L_CH2_TX3,   CH2_STATE_REG, TX3_STATE
    qbeq     L_CH2_TX4,   CH2_STATE_REG, TX4_STATE
    qbeq     L_CH2_TX5,   CH2_STATE_REG, TX5_STATE

;----------------------- check if next buffer has data -------------------------
;
; state is IDLE
;    - add 8 bit of idle at the end of UART frame handler
;    - check if there is more data to transmit
;    - if there is data to transmit, set STATE to TX1_STATE
;
;-------------------------------------------------------------------------------

; no active frame state, add 8 bit of idle at the end of UART frame handler
    ldi      r30.b0, 0xff

; Check to see if the buffer has active data
; r26 has both READ_PTR (w0) & WRITE_PTR (w2)
    lbco    &r26, c24, READ_PTR_ADDRESS, 4

; if READ_PTR == WRITE_PTR, buffer is empty
    qbeq    L_EXIT_CH2_IDLE, READ_PTR, WRITE_PTR

; else, the buffer has active data

; set STATE to TX1_STATE
    ldi      CH2_STATE_REG, TX1_STATE

; go to next channel
L_EXIT_CH2_IDLE:
    xout     SP_BANK2, &r2, 100   ; save R2-R26 context
    qba      L_CH3_IDLE


;*******************************************************************************
; State CH2_TX1
;
; For detailed comments, see CH0_TX1
;*******************************************************************************
L_CH2_TX1:

; use macro for send
    m_tx1 SP_BANK2, CH2_STATE_REG

; go to next channel
    qba      L_CH3_IDLE

;*******************************************************************************
; State CH2_TX2
;
; For detailed comments, see CH0_TX2
;*******************************************************************************
L_CH2_TX2:

; transmit macro
    m_tx2 SP_BANK2, CH2_STATE_REG

; go to next channel
    qba     L_CH3_IDLE


;*******************************************************************************
; State CH2_TX3
;
; For detailed comments, see CH0_TX3
;*******************************************************************************
L_CH2_TX3:

; transmit macro
    m_tx3 SP_BANK2, CH2_STATE_REG

; go to next channel
    qba     L_CH3_IDLE


;*******************************************************************************
; State CH2_TX4
;
; For detailed comments, see CH0_TX4
;*******************************************************************************
L_CH2_TX4:

; transmit macro
    m_tx4 SP_BANK2, CH2_STATE_REG

; go to next channel
    qba     L_CH3_IDLE


;*******************************************************************************
; State CH2_TX5
;
; For detailed comments, see CH0_TX5
;*******************************************************************************
L_CH2_TX5:

; transmit macro
    m_tx5 SP_BANK2, CH2_STATE_REG

; go to next channel
    qba     L_CH3_IDLE


; **************************** CH3_IDLE ****************************************

L_CH3_IDLE:

; Check if THR (Transmit Holding Register) is empty
; Read UART Line Status Register (LSR)
    lbco     &r2.b0, c7, UART_LSR, 1
; THR is empty if bit 5 is set
    qbbc     L_EXIT_CH3, r2.b0, 5

; TODO: Check if UART_LSR[6] TEMT (Transmitter Empty) needs to be set before
; writing additional data to the THR register. This may not be needed.

; Load WRITE_PTR from memory to check if buffer has data
    lbco     &CH3_WRITE_PTR, c24, TX_CH3_WRITE_PTR, 2

; Check if buffer is empty (READ_PTR == WRITE_PTR)
    qbeq     L_EXIT_CH3, CH3_READ_PTR, CH3_WRITE_PTR

; Buffer has data - load 1 byte from circular buffer at CH3_READ_PTR
    lbco     &r2.b0, c24, CH3_READ_PTR, 1
; Send byte to THR register
    sbco     &r2.b0, c7, 0, 1

; Increment CH3_READ_PTR with wraparound check
    add      CH3_READ_PTR, CH3_READ_PTR, 1
; Check if we've reached BUFFER_END
    qbne     L_CH3_SAVE_PTR, CH3_READ_PTR, CH3_BUFFER_END
; Wraparound: reset to BUFFER_START
    mov      CH3_READ_PTR, CH3_BUFFER_START

L_CH3_SAVE_PTR:
; Save updated READ_PTR back to memory
    sbco     &CH3_READ_PTR, c24, CH3_READ_PTR_ADDRESS, 2

; go to next channel
L_EXIT_CH3:
    JMP      L_CH0_IDLE

; **********************  end of channel processing  *******************
