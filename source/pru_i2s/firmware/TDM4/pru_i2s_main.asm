; 
; Copyright (C) 2025 Texas Instruments Incorporated - http://www.ti.com/
; 
; 
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions
; are met:
; 
;        * Redistributions of source code must retain the above copyright
;          notice, this list of conditions and the following disclaimer.
; 
;        * Redistributions in binary form must reproduce the above copyright
;          notice, this list of conditions and the following disclaimer in the
;          documentation and/or other materials provided with the
;          distribution.
; 
;        * Neither the name of Texas Instruments Incorporated nor the names of
;          its contributors may be used to endorse or promote products derived
;          from this software without specific prior written permission.
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
	.sect	".text:main"
	.clink
	.global main

	.include "pru_i2s_interface.h"
	.include "pru_i2s_regs.h"
main:
;enable cyclecount
	lbco		&R0, c11, 0x00, 4
	or		R0, R0, (1<<3)
	sbco		&R0, c11, 0x00, 4
	.if	$isdefed("I2S_TX")
	;err_stat = err_stat >> 1. Remove the Rx Overflow error bit.
	LSR err_stat, err_stat, 1
	;If there was an underflow/FrameSync Error, clearup thr Tx PingPong Buffer space
	;err_stat is already initialized during normal operation.
	; FS_ERROR	UNDERFLOW_ERROR
	;    0          0		No Error
	;    0          1		Under Flow error
	;    1          0       FS Error
	;    1          1       Both errors
	QBEQ CONTIUNE_INIT, err_stat, 0
	;If set, this means underflow has happened.
	;Below Initializations can be avoided but after power on, registers
	;may contain random addresses and may result in accessing illegal addresses.
	LDI32 	ch0_data_tx, 0x0
	LDI32	scratchreg0, 0x0
	LDI32	tx_buf_size, 0x0
	LDI		scratchreg0, I2S_TX_BUF_PING_ADD
	LBBO	&tx_ping_buffer_address, scratchreg0, 0, 4
	LDI		scratchreg0, I2S_PING_PONG_BUFSIZE_ADD
	LBBO	&tx_buf_size, scratchreg0, 0, 2
	ADD		tx_buffer_address_end, tx_ping_buffer_address, tx_buf_size
ZERO_TX_PING_PONG:
	SBBO	&ch0_data_tx, tx_ping_buffer_address, 0, 4
	ADD		tx_ping_buffer_address, tx_ping_buffer_address, 0x4
	QBGT	ZERO_TX_PING_PONG, tx_ping_buffer_address, tx_buffer_address_end
	.endif

CONTIUNE_INIT:
	;Clear registers R0-R29. 4*30=120 bytes
	ZERO 	&r0, 120
	LDI32	r30, 0x0
	LDI32	r31, 0x0

	; Load pin mask registers
	.if	$isdefed("I2S_TX")
    LDI32   i2s_instance_fs_pin_pos, I2S_INSTANCE_FS_PIN_POS
	.endif
	.if	$isdefed("I2S_RX")
    LDI32   i2s_instance_fs_pin_pos, I2S_INSTANCE_FS_PIN_POS
    LDI32   i2s_instance1_sd_rx_pin_pos, I2S_INSTANCE1_SD_RX_PIN_POS
    LDI32   i2s_instance2_sd_rx_pin_pos, I2S_INSTANCE2_SD_RX_PIN_POS
	.endif
    
    LDI32	scratchreg2, I2S_TX_INSTANCE_PING_PONG_STAT_ADD
	LDI32 	scratchreg0, 0x2
	;Store 0x2 into TX_PING_PONG_STAT
	SBBO	&scratchreg0, scratchreg2, 0, 1

	;I2S_RX_INSTANCE_PING_PONG_STAT_ADD = I2S_TX_INSTANCE_PING_PONG_STAT_ADD+1
	LDI32 	scratchreg0, 0x1
	;Store 0x1 into RX_PING_PONG_STAT
	SBBO	&scratchreg0, scratchreg2, 1, 1

	;Start with Ping Buffer
	LDI	tx_buffer_num, 0x1
	;Start with Ping Buffer
	LDI	rx_buffer_num, 0x1
	LDI	tx_sd_counter, I2S_SAMPLES_PER_CHANNEL_LESS_1
	LDI current_channel_no,1



	.if	$isdefed("I2S_TX")
	; these 3 pseudoinstructions are used for each additional output bit
	; move the bit of audio data we are going to output next to the LSb of r30_value
	LSR	r30_value, ch0_data_tx, tx_sd_counter
	;Remove other bits
	AND	r30_value, r30_value, 1
	;Move the LSb to the required position
	LSL r30_value, r30_value, I2S_INSTANCE1_SD_TX_PIN_SHIFT
	.endif
	.if	!$isdefed("I2S_TX")
	.if	$isdefed("I2S_RX")
	;Read the PING/PONG SEL address
	LDI32	rx_ping_pong_sel_add, I2S_RX_BUF_PING_PONG_STAT_ADD
	;Read the PING/PONG STAT address
	LDI32	rx_ping_pong_stat_add, I2S_RX_INSTANCE_PING_PONG_STAT_ADD
	.endif
	.endif

	.if	$isdefed("I2S_TX")
	;read the Tx buffer address provided by host. This should be in Shared memory
	; point to the PING audio buffer
	LDI	scratchreg0, I2S_TX_BUF_PING_ADD
	LBBO	&tx_buffer_address, scratchreg0, 0, 4
	MOV	tx_ping_buffer_address, tx_buffer_address
	.endif

	;read the buffer size provided by host. buffer size is ping+pong
	LDI	scratchreg0, I2S_PING_PONG_BUFSIZE_ADD
	LBBO	&tx_buf_size, scratchreg0, 0, 2
	;Shift right by 1 to get PING/PONG size buffer size
	LSR	tx_buf_size, tx_buf_size, 1
	.if	$isdefed("I2S_RX")
	MOV rx_buf_size, tx_buf_size
	;Rx buffer size is one less than PING/PONG buffer size
	SUB rx_buf_size, rx_buf_size, 1

	;read the Rx buffer address provided by host
	LDI	scratchreg0, I2S_RX_BUF_PING_ADD
	LBBO	&rx_buffer_address, scratchreg0, 0, 4
	MOV	rx_ping_buffer_address, rx_buffer_address
	LDI rx_sd_counter, I2S_SAMPLES_PER_CHANNEL
	.endif

	;Initialize the end address of the Tx/Rx buffers
	.if	$isdefed("I2S_TX")
	ADD	tx_buffer_address_end, tx_buffer_address, tx_buf_size
	.endif
	.if	$isdefed("I2S_RX")
	ADD	rx_buffer_address_end, rx_buffer_address, rx_buf_size
	.endif
	;Initialize the PONG buffer addresses
	.if	$isdefed("I2S_TX")
	MOV	tx_pong_buffer_address, tx_buffer_address_end
	.endif
	.if	$isdefed("I2S_RX")
	MOV rx_pong_buffer_address, rx_buffer_address_end
	ADD rx_pong_buffer_address, rx_pong_buffer_address, 1
	.endif
	.if	$isdefed("I2S_TX")
	;Read the first bytes from Ping buffer
	LBBO	&ch0_data_tx, tx_buffer_address, 0, BYTES_TO_LOAD
	ADD		tx_buffer_address, tx_buffer_address, BYTES_TO_LOAD
	ADD current_channel_no,current_channel_no,1
	.endif

INITIAL_STATE:
	; start iterating
	; wait until the input BCLK is low
	; then branch to wait for the rising edge
	QBBS	INITIAL_STATE, r31, I2S_INSTANCE_BCLK_PIN
	;Wait until there are 4 transitions of FS H-L-H-L-H
INITIAL_STATE_FS:
	; start iterating
	; wait until the input FS is low
	; then branch to wait for the rising edge
	QBBS	INITIAL_STATE_FS, r31, I2S_INSTANCE_FS_PIN
INITIAL_STATE_FS1:
	; wait until the input FS is high
	QBBC	INITIAL_STATE_FS1, r31, I2S_INSTANCE_FS_PIN
INITIAL_STATE_FS2:
	; wait until the input FS is low
	QBBS	INITIAL_STATE_FS2, r31, I2S_INSTANCE_FS_PIN
INITIAL_STATE_FS3:
	; wait until the input FS is low
	QBBC	INITIAL_STATE_FS3, r31, I2S_INSTANCE_FS_PIN
BCLK_RISING_EDGE_LOW1:
	QBBS	BCLK_RISING_EDGE_LOW1, r31, I2S_INSTANCE_BCLK_PIN
BCLK_RISING_EDGE_LOW:
	; wait until we see a high value
	QBBC	BCLK_RISING_EDGE_LOW, r31, I2S_INSTANCE_BCLK_PIN

	; Read FS.
	AND fs_level, r31, i2s_instance_fs_pin_pos
	.if	$isdefed("I2S_RX")
	;Read I2S1 Rx data
	AND ch0_rx_val_reg, r31, i2s_instance1_sd_rx_pin_pos
	;Read I2S2 Rx data
	AND ch1_rx_val_reg, r31, i2s_instance2_sd_rx_pin_pos

	;Once the buffer is full, switch to new buffer
	QBGT	MANAGE_RX_BUFFERS, rx_buffer_address_end, rx_buffer_address
	.endif
	.if	$isdefed("I2S_TX")
	.if	!$isdefed("I2S_RX")
CONTINUE_TX_PROCESSING:
	.endif
	.endif
	; if fs_counter = 0x0, then do tx buffer management
	QBEQ	MANAGE_TX_BUFFERS, fs_counter, 0x0
	.if $isdefed("SOC_AM64X")
	;if fs_counter = max(fs_counter) -1, then check FS before expected FS transition
	QBEQ	CHECK_FS_1, fs_counter, 0xE
	;if fs_counter = max(fs_counter), then check FS after expected FS transition
	QBEQ	CHECK_FS_2, fs_counter, 0xF
	;if fs_counter = max(fs_counter) -2, then load audio data
	QBEQ	LOAD_AUDIO_DATA, fs_counter, 0xD
	.else
	;if fs_counter = max(fs_counter) -1, then check FS before expected FS transition
	QBEQ	CHECK_FS_1, fs_counter, (I2S_SAMPLES_PER_CHANNEL_LESS_1 - 1)
	;if fs_counter = max(fs_counter), then check FS after expected FS transition
	QBEQ	CHECK_FS_2, fs_counter, I2S_SAMPLES_PER_CHANNEL_LESS_1
	;if fs_counter = max(fs_counter) -2, then load audio data
	QBEQ	LOAD_AUDIO_DATA, fs_counter, (I2S_SAMPLES_PER_CHANNEL_LESS_1 - 2)
	.endif
	;In case of Rx Only, Jump here after Sending Rx Notify to save PRU Cycles as we know fs_counter is 1.
	.if	!$isdefed("I2S_TX")
	.if	$isdefed("I2S_RX")
CONTINUE_TX_PROCESSING:
	.endif
	.endif
	;else, increment fs_counter and wait for falling edge
	ADD	fs_counter, fs_counter, 0x1
	.if	$isdefed("I2S_TX_DETECT_UNDERFLOW")
	;Reset the variable here to save 1 cycle in BCLK LOW.
	LDI do_tx_underflow_error_check, 0
	.endif
	;if fs_counter = max(fs_counter)+1, then check FS after expected FS transition
	QBGE CHECK_FS_3, fs_counter_1, 1
	;If not Jump
	JMP	BCLK_FALLING_EDGE_HIGH
CHECK_FS_3:
	LDI	fs_counter_1, 0x0
	LSR	fs_level, fs_level, I2S_INSTANCE_FS_PIN
	JMP	BCLK_FALLING_EDGE_HIGH

CHECK_FS_1:
	; if fs_counter = max(fs_counter) -1: Check for Early FS.
	LSR	fs_level, fs_level, I2S_INSTANCE_FS_PIN
	; increment fs_counter and wait for falling edge
	ADD	fs_counter, fs_counter, 0x1
	JMP	BCLK_FALLING_EDGE_HIGH

CHECK_FS_2:
	; if fs_counter = max(fs_counter):
	;Increment counter.
	ADD	fs_counter_1, fs_counter_1, 0x1
	; reset fs_counter and wait for falling edge
	LDI	fs_counter, 0x0
	JMP	BCLK_FALLING_EDGE_HIGH

LOAD_AUDIO_DATA:
	.if	$isdefed("I2S_TX")
	.if	$isdefed("SOC_AM64X")
	; load audio data from ping-pong buffers
	; note that scratch registers must be sequential
	LBBO	&scratchreg0, tx_buffer_address, 0, 4
	ADD	tx_buffer_address, tx_buffer_address, 0x4
	.else
	; load audio data from ping-pong buffers
	; note that scratch registers must be sequential
	LBBO	&scratchreg0, tx_buffer_address, 0, BYTES_TO_LOAD
	QBLT    DATA_LOAD_DONE, current_channel_no, TDM_CHANNELS
	ADD	tx_buffer_address, tx_buffer_address, BYTES_TO_LOAD
DATA_LOAD_DONE:
	.endif
	.endif
	; increment fs_counter and wait for falling edge
	ADD	fs_counter, fs_counter, 0x1
	JMP	BCLK_FALLING_EDGE_HIGH

MANAGE_RX_BUFFERS:
	.if	$isdefed("I2S_RX")
	; if tx_buffer_address has reached the end of the buffer, point
	; to the beginning of the next buffer and notify the host core to
	; push fresh data to the buffer we just left.
	; if we are on buffer1, point to buffer2
	QBEQ	START_RX_BUFFER2, rx_buffer_num, 0x1

	; else, start buffer1
	MOV	rx_buffer_address, rx_ping_buffer_address
	JMP	RX_CONTINUE

START_RX_BUFFER2:
	MOV	rx_buffer_address, rx_pong_buffer_address

RX_CONTINUE:
	ADD	rx_buffer_address_end, rx_buffer_address, rx_buf_size
	; flip rx_buffer_num between 01_b and 01_b
	XOR	rx_buffer_num, rx_buffer_num, 0x1
	SBBO 	&rx_buffer_num, rx_ping_pong_sel_add, 0, 1
	;set a condition to do Rx Overflow check
	LDI 	do_rx_overflow_error_check, 1
	; notify the host
	;LDI    R31.w0, TRIGGER_HOST_I2S_RX_IRQ
	.endif ;I2S_RX
	JMP	CONTINUE_TX_PROCESSING

MANAGE_TX_BUFFERS:
	.if	$isdefed("I2S_TX")
	; if tx_buffer_address has reached the end of the buffer, point
	; to the beginning of the next buffer and notify the host core to
	; push fresh data to the buffer we just left.
	QBEQ	START_NEW_BUFFER, tx_buffer_address, tx_buffer_address_end
	.endif
	; increment fs_counter and wait for falling edge
	ADD	fs_counter, fs_counter, 0x1
	JMP	BCLK_FALLING_EDGE_HIGH

START_NEW_BUFFER:
	.if	$isdefed("I2S_TX")
	; if we are on buffer1, point to buffer2
	QBEQ	START_BUFFER2, tx_buffer_num, 0x1

	; else, start buffer1
	MOV	tx_buffer_address, tx_ping_buffer_address
	JMP	CONTINUE

START_BUFFER2:
	MOV	tx_buffer_address, tx_pong_buffer_address

CONTINUE:
	ADD	tx_buffer_address_end, tx_buffer_address, tx_buf_size

NOTIFY:
	; flip tx_buffer_num between 00_b and 01_b
	XOR	tx_buffer_num, tx_buffer_num, 0x1
	LDI32	scratchreg2, I2S_TX_BUF_PING_PONG_STAT_ADD
	SBBO 	&tx_buffer_num, scratchreg2, 0, 1
	.if	$isdefed("I2S_TX_DETECT_UNDERFLOW")
	;Read the Tx Ping Pong buffer stat from register address space
	;I2S_TX_INSTANCE_PING_PONG_STAT_ADD = I2S_TX_BUF_PING_PONG_STAT_ADD+2
	LBBO 	&tx_ping_pong_stat, scratchreg2, 2, 1

	;If tx_buffer_num is 0, this means FW has already consumed PING buffer.
	QBNE TX_PONG_BUF, tx_buffer_num, 0x0
	;Check if the PONG buffer has valid data
	QBBC ERROR_HANDLING_UNDERFLOW, tx_ping_pong_stat, 1
	;If 1, this means PONG buffer has valid data. Clear the PING buffer status
	CLR 	tx_ping_pong_stat, tx_ping_pong_stat, 0
	JMP STORE_TX_PING_PONG_STAT

TX_PONG_BUF:
	;Check if the PING buffer is full
	QBBC ERROR_HANDLING_UNDERFLOW, tx_ping_pong_stat, 0
	;If 1, this means PING buffer has valid data. Clear the PONG buffer status
	CLR 	tx_ping_pong_stat, tx_ping_pong_stat, 1

STORE_TX_PING_PONG_STAT:
	;set a condition to perform some part of Tx Underflow in other half of BLCK
	LDI 	do_tx_underflow_error_check, 1
	;SBBO 	&tx_ping_pong_stat, scratchreg2, 2, 1
	.endif ;I2S_TX_DETECT_UNDERFLOW
	; notify the host
	;LDI    R31.w0, TRIGGER_HOST_I2S_TX_IRQ
	.endif ;I2S_TX
	; increment fs_counter and wait for falling edge
	ADD	fs_counter, fs_counter, 0x1

BCLK_FALLING_EDGE_HIGH:

	.if	$isdefed("I2S_TX")
	; write to R30 as soon as we see the falling clock edge
	MOV	r30, r30_value
	.endif
WAIT_BCLK_REMAIN_HIGH:
	QBBS	WAIT_BCLK_REMAIN_HIGH, r31, I2S_INSTANCE_BCLK_PIN
	.if	$isdefed("I2S_TX")
	.if	$isdefed("I2S_TX_DETECT_UNDERFLOW")
	QBNE CONTINUE_TX_PROCESS, do_tx_underflow_error_check, 0x1
	;Moved this to BCLK HIGH to save 1 cycle to meet I2S spec
	;LDI do_tx_underflow_error_check, 0
	SBBO 	&tx_ping_pong_stat, scratchreg2, 2, 1
	.endif ;I2S_TX_DETECT_UNDERFLOW
	; notify the host
	LDI    R31.w0, TRIGGER_HOST_I2S_TX_IRQ
CONTINUE_TX_PROCESS:
	.endif
	.if	$isdefed("I2S_RX")
	.if	$isdefed("I2S_RX_DETECT_OVERFLOW")
	QBNE CONTINUE_RX_PROCESS, do_rx_overflow_error_check, 0x1
	LDI do_rx_overflow_error_check, 0
	;Read the Rx Ping Pong buffer stat from register address space
	LBBO 	&rx_ping_pong_stat, rx_ping_pong_stat_add, 0, 1
	;If rx_buffer_num is 0, this means FW has already filled PING buffer.
	QBNE RX_PONG_BUF, rx_buffer_num, 0x0
	;Check if the PONG buffer is consumed by R5F. If consumed, bit 1 will be set to 0 by R5F, else it will be 1.
	QBBS ERROR_HANDLING_OVERFLOW, rx_ping_pong_stat, 1
	;If 0, this means PONG buffer is consumed by R5F. SET the PING buffer status
	SET rx_ping_pong_stat, rx_ping_pong_stat, 0
	JMP STORE_RX_PING_PONG_STAT

RX_PONG_BUF:
	;Check if the PING buffer is consumed by R5F. If consumed, bit 0 will be set to 0 by R5F, else it will be 1.
	QBBS ERROR_HANDLING_OVERFLOW, rx_ping_pong_stat, 0
	;If 0, this means PING buffer is consumed by R5F. SET the PONG buffer status
	SET rx_ping_pong_stat, rx_ping_pong_stat, 1
STORE_RX_PING_PONG_STAT:
	SBBO 	&rx_ping_pong_stat, rx_ping_pong_stat_add, 0, 1
	.endif ;I2S_RX_DETECT_OVERFLOW
	; notify the host
	LDI    R31.w0, TRIGGER_HOST_I2S_RX_IRQ
	.endif
CONTINUE_RX_PROCESS:
	.if	$isdefed("I2S_TX")
	; if tx_sd_counter = 0, set to samples per channel 15/31
	QBEQ	RESET_SD_COUNTER, tx_sd_counter, 0
	; else, increment tx_sd_counter
	SUB	tx_sd_counter, tx_sd_counter, 0x1
	.endif
	JMP	PREPARE_INPUT

RESET_SD_COUNTER:
	.if	$isdefed("I2S_TX")
	; reset fs_counter and wait for falling edge
	LDI	tx_sd_counter, I2S_SAMPLES_PER_CHANNEL_LESS_1
	QBGE 	TX_DATA_LOADING, current_channel_no, TDM_CHANNELS
	LDI32 	ch0_data_tx,0
	QBA 	TX_DATA_LOADING_DONE
TX_DATA_LOADING:
	MOV ch0_data_tx, scratchreg0
TX_DATA_LOADING_DONE:
	QBNE NO_CH_RESET,current_channel_no, MAX_TDM_CHANNELS
	LDI current_channel_no,1
	QBA CH_UPDATE_DONE
NO_CH_RESET:
	ADD current_channel_no, current_channel_no, 1
CH_UPDATE_DONE:
	.endif

PREPARE_INPUT:
		; these 3 pseudoinstructions are used for each additional output bit
	; move the bit of audio data we are going to output next to the LSb of r30_value
	.if	$isdefed("I2S_TX")
	LSR	r30_value, ch0_data_tx, tx_sd_counter
	;Remove other bits
	AND	r30_value, r30_value, 1
	;Move the LSb to the required position
	LSL r30_value, r30_value, I2S_INSTANCE1_SD_TX_PIN_SHIFT
	.endif
	.if	$isdefed("I2S_RX")
	; Receive Data
	; these 3 pseudoinstructions are used for each additional input bit
	; Skip for the first time since the first receive bit is valid after one bitclock after FS level change. See I2S spec.
	QBEQ SKIP, rx_sd_counter, I2S_SAMPLES_PER_CHANNEL
	; Move the received bit to LSb position
	LSR ch0_rx_val_reg, ch0_rx_val_reg, I2S_INSTANCE1_SD_RX_PIN_SHIFT
	LSR ch1_rx_val_reg, ch1_rx_val_reg, I2S_INSTANCE2_SD_RX_PIN_SHIFT
	; Shift it to the required position
	LSL	ch0_rx_val_reg, ch0_rx_val_reg, rx_sd_counter
	LSL	ch1_rx_val_reg, ch1_rx_val_reg, rx_sd_counter
	; Append the bits of information to exisiting data
	OR	ch0_data_rx, ch0_data_rx, ch0_rx_val_reg
	OR	ch1_data_rx, ch1_data_rx, ch1_rx_val_reg
	; Once all the data is received, store the audio data.
	QBEQ STORE_AUDIO_DATA, rx_sd_counter, 0

SKIP:
	SUB rx_sd_counter, rx_sd_counter, 1
	.endif
	JMP	BCLK_RISING_EDGE_LOW

STORE_AUDIO_DATA:
	.if	$isdefed("I2S_RX")
	; store 32 bits of audio data per channel.
	QBLT    STORAGE_DONE,current_channel_no, TDM_CHANNELS
	SBBO	&ch0_data_rx, rx_buffer_address, 0, BYTES_TO_LOAD
	ADD	rx_buffer_address, rx_buffer_address, BYTES_TO_LOAD
STORAGE_DONE:
	ADD current_channel_no,current_channel_no,1
	QBGE RX_CH_UPDATE_DONE,current_channel_no, MAX_TDM_CHANNELS
	LDI current_channel_no,1
RX_CH_UPDATE_DONE:

	; Reset ch0_rx_val_reg, ch1_rx_val_reg, ch0_data_rx, ch1_data_rx to 0. All these register must be continuous
	ZERO 	&ch0_rx_val_reg, 16
	LDI	rx_sd_counter, I2S_SAMPLES_PER_CHANNEL_LESS_1
	.endif
	JMP	BCLK_RISING_EDGE_LOW
ERROR_HANDLING_OVERFLOW:
	SET err_stat, err_stat, I2S_RXOVERFLOW_ERROR_POSITION
	JMP ERROR_HANDLING_NOTIFY

ERROR_HANDLING_UNDERFLOW:
	SET err_stat, err_stat, I2S_TXUNDERFLOW_ERROR_POSITION
	JMP ERROR_HANDLING_NOTIFY

ERROR_HANDLING_FS:
	SET err_stat, err_stat, I2S_FRAMESYNC_ERROR_POSITION

ERROR_HANDLING_NOTIFY:
	;Load the ERR_STAT address into a scratch register
	LDI32	scratchreg2, I2S_ERR_STAT_ADD
	;Store the error in to ERR_STAT
	SBBO	&err_stat, scratchreg2, 0, 1
	; notify the host
	LDI    R31.w0, TRIGGER_HOST_I2S_ERR_IRQ
	JMP	||main||
