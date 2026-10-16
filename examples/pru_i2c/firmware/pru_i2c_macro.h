; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
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
;   File:     pru_i2c_macro.h
;
;   Brief:   This file contains ICSS I2C macros definations  
;************************************************************************************

    .if    !$defined("__icss_i2c_macros_h")
__icss_i2c_macros_h    .set 1

;************************************* includes *************************************

    ;;.include "icss_constant_defines.inc"
    ;;.include "icss_cfg_regs.h"
    ;;.include "icss_iep_regs.h"
    ;;.include "icss_intc_regs.inc"
    ;;;.include "endat_icss_reg_defs.h"
    .cdecls C,NOLIST
%{
#include "pru_i2c_interface.h"
%}

;---------------------------------------------------------------------------------------

;************************************************************************************
;
;   Macro: ENABLE_XIN_XOUT_SHITFTING
;
;   Enable support of shifting during XIN/XOUT operation
;   
;   PEAK cycles:
;        3 cycles
;   Pseudo code:
;       ICSS_CFG_SPPC[0-7] |= 0x02
;
;   Parameters:
;      None 
;
;   Returns:
;      None
;
ENABLE_XIN_XOUT_SHITFTING    .macro
    LBCO    &TEMP_REG1, ICSS_CFG_CONST, ICSS_CFG_SPPC, 4
    OR      TEMP_REG1.b0, TEMP_REG1.b0, 0x02
    SBCO    &TEMP_REG1, ICSS_CFG_CONST, ICSS_CFG_SPPC, 4
    .endm


;************************************************************************************
;
;   Macro: I2C_WAIT_FOR_IEP_CMP
;
;   wait until this core's IEP compare event (I2C_IEP_CMP_STATUS_BIT) is set
;
;   PEAK cycles:
;       Unbounded by design: this is the scheduler tick. It returns at most
;       one IEP compare period after the previous tick. The compare is armed
;       by I2C_SETUP_IEP_COUNTER and re-armed by I2C_IEP_INTC_CLEAR_EVENT, and
;       the shared IEP counter is never stopped by this firmware, so the wait
;       always terminates. There is no other time base to time it out against.
;   Pseudo code:
;       while(IEP_CMP_STATUS_REG.I2C_IEP_CMP_STATUS_BIT == 0)
;
;   Parameters:
;      None
;
;   Returns:
;      None
;
;   Registers modified:
;      TEMP_REG1
;
;************************************************************************************
I2C_WAIT_FOR_IEP_CMP    .macro

WAIT_FOR_INT?:
    LBCO   &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_CMP_STATUS_REG, 4
    QBBC    WAIT_FOR_INT?, TEMP_REG1, I2C_IEP_CMP_STATUS_BIT

    .endm


;************************************************************************************
;
;   Macro: I2C_IEP_INTC_CLEAR_EVENT
;
;   Clear the iep cmp event and intc event 
;   
;   PEAK cycles:
;       
;   Pseudo code:
;       
;
;   Parameters:
;      None 
;
;   Returns:
;      None
;
;************************************************************************************
I2C_IEP_INTC_CLEAR_EVENT    .macro    arg1

    ; clear the IEP compare event happened
    LBCO   &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_CMP_STATUS_REG, 4
    QBBS    IEP_INTC_CLEAR?, TEMP_REG1, I2C_IEP_CMP_STATUS_BIT
    JMP     arg1
IEP_INTC_CLEAR?:

    ; write-1-to-clear only this core's bit: the other PRU shares the register
    LDI     TEMP_REG1, 0
    SET     TEMP_REG1, TEMP_REG1, I2C_IEP_CMP_STATUS_BIT
    SBCO   &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_CMP_STATUS_REG, 4

    ;Set compare values
    ADD     IEP_COUNTER_NEXT_VAL1, IEP_COUNTER_NEXT_VAL1, I2C_GLOBAL_FREQ_REG.w2
    ADC     IEP_COUNTER_NEXT_VAL2, IEP_COUNTER_NEXT_VAL2, 0x00
    SBCO    &IEP_COUNTER_NEXT_VAL1, ICSS_IEP_CONST, I2C_IEP_CMP_REG, 8

    ; Clear the intc interrupt event flag
    LDI    TEMP_REG1.w0, 0x0080
    LDI    TEMP_REG2.w0, ICSS_INTC_SECR1
    SBCO   &TEMP_REG1, ICSS_INTC_CONST, TEMP_REG2.w0, 4
    
    .endm

;************************************************************************************
;
;   Macro: I2C_WAVE_FUNCTION0
;
;   jump to the next state of i2c function 
;   
;   PEAK cycles:
;       
;   Pseudo code:
;       
;
;   Parameters:
;      None 
;
;   Returns:
;      None
;
;************************************************************************************
I2C_WAVE_FUNCTION0    .macro

    ;Restore I2C instance context
    LDI    R0.b0, 0x00
    XIN    I2C_CONTEXT_BANK, &R10, 40
   
    ;Jump and link to the next task function
    JAL    TEMP_REG3.w0, R13.w0 ;R13 store RESET MODE 
    
    ;Save context to SPAD
    LDI    R0.b0, 0x00
    XOUT   I2C_CONTEXT_BANK, &R10, 40
    
    .endm


;************************************************************************************
;
;   Macro: UPDATE_NEXT_LOCAL_STATE
;
;   update next state in state keep register 
;   
;   PEAK cycles:
;       
;   Pseudo code:
;       
;
;   Parameters:
;      arg1: Label of next state 
;
;   Returns:
;      None
;
;************************************************************************************
UPDATE_NEXT_LOCAL_STATE    .macro    arg1
    LDI     R13.w0, $CODE(arg1)
    .endm


;************************************************************************************
;
;   Macro: STATE_TASK_OVER
;
;   Return to the scheduler as state task is over.
;   
;   PEAK cycles:
;       
;   Pseudo code:
;       
;
;   Parameters:
;      arg1: Label of next state 
;
;   Returns:
;      None
;
;************************************************************************************
STATE_TASK_OVER    .macro
    JMP     TEMP_REG3.w0
    .endm

;************************************************************************************
;
;   Open-drain bus pins
;
;   SCL and SDA are never driven high. R30 keeps both pin bits at 0 and the
;   line is controlled through the pin's OUTDISABLE bit in this core's
;   ICSSMx_PRUy_GPIO_OUT_CTRL register (I2C_GPIO_OUT_CTRL):
;       OUTDISABLE = 1 -> released, the external pull-up takes the line high
;       OUTDISABLE = 0 -> driven low
;   R23 is a shadow copy of that register and R26 holds its address, both
;   set up in SETUP_I2C_SCL_SDA_HIGH, so a pin change is a single store.
;   A target can therefore hold SCL low (clock stretching, see
;   WAIT_SCL_HIGH) and the master never fights a target on SDA.
;
;   PEAK cycles:
;       2 + store
;
;   Registers modified:
;      R23
;
;************************************************************************************
SET_SCL_PIN_HIGH    .macro
    SET     R23, R23, R14.b0
    SBBO    &R23, R26, 0, 4
    .endm

SET_SCL_PIN_LOW    .macro
    CLR     R23, R23, R14.b0
    SBBO    &R23, R26, 0, 4
    .endm

SET_SDA_PIN_HIGH    .macro
    SET     R23, R23, R14.b1
    SBBO    &R23, R26, 0, 4
    .endm

SET_SDA_PIN_LOW  .macro
    CLR     R23, R23, R14.b1
    SBBO    &R23, R26, 0, 4
    .endm

;************************************************************************************
;
;   Macro: WAIT_SCL_HIGH
;
;   Clock stretching. Put at the top of every state that runs one tick after
;   SCL was released. If a target still holds SCL low, return without
;   advancing the state, so the same state runs again on the next tick.
;   R18.w0 counts the ticks SCL has been held low; when it reaches the limit
;   in R18.w2 (ICSS_I2C_SCL_TIMEOUT_OFFSET, 0 = no limit) the transfer is
;   abandoned with TIME_OUT_ERROR (SCL_LOW_TIMEOUT).
;   When SCL comes up after being held, wait one more tick before going on,
;   so the high period is counted from the moment SCL actually rose and is
;   never shorter than without stretching (tHIGH).
;
;   Registers modified:
;      R18.w0, R13.w0 (on timeout)
;
;************************************************************************************
WAIT_SCL_HIGH    .macro
    QBBS    scl_high?, R31, R14.b0
    ADD     R18.w0, R18.w0, 1
    QBEQ    scl_wait?, R18.w2, 0
    QBLT    scl_wait?, R18.w2, R18.w0
    LDI     R13.w0, $CODE(SCL_LOW_TIMEOUT)
scl_wait?:
    STATE_TASK_OVER
scl_high?:
    QBEQ    scl_ok?, R18.w0, 0
    LDI     R18.w0, 0
    STATE_TASK_OVER
scl_ok?:
    .endm

;************************************************************************************
;
;   Macro: PEC_UPDATE
;
;   SMBus Packet Error Code: CRC-8, x^8 + x^2 + x + 1, initial value 0,
;   updated one bit at a time as bits go out or come in, MSB first. Runs
;   only when SMB_F_PEC is set for the current transfer.
;
;   Parameters:
;      src, bitn: the bit (register, bit number) just sent or received
;
;   Registers modified:
;      R19.b0 (CRC), TEMP_REG6.b0
;
;************************************************************************************
PEC_UPDATE    .macro    src, bitn
    QBBC    pec_done?, R19.b3, SMB_F_PEC
    LSR     TEMP_REG6.b0, R19.b0, 7
    QBBC    pec_bit0?, src, bitn
    XOR     TEMP_REG6.b0, TEMP_REG6.b0, 1
pec_bit0?:
    LSL     R19.b0, R19.b0, 1
    QBEQ    pec_done?, TEMP_REG6.b0, 0
    XOR     R19.b0, R19.b0, 0x07
pec_done?:
    .endm

;************************************************************************************
;
;   Macro: TX_FETCH_BYTE
;
;   Load byte R15.b2 of the transmit stream into dest. The stream starts at
;   R11: the Tx buffer for I2C, or up to two SMBus prefix bytes (command
;   code, block count) staged just in front of it. When SMB_F_PECTX is set
;   the last byte of the stream is the PEC, taken from R19.b0, which by then
;   covers every bit sent before it.
;
;   Registers modified:
;      dest, TEMP_REG4.b0
;
;************************************************************************************
TX_FETCH_BYTE    .macro    dest
    QBBC    fetch_buf?, R19.b3, SMB_F_PECTX
    SUB     TEMP_REG4.b0, R15.b3, 1
    QBNE    fetch_buf?, R15.b2, TEMP_REG4.b0
    MOV     dest, R19.b0
    JMP     fetch_done?
fetch_buf?:
    LBBO    &dest, R11, R15.b2, 1
fetch_done?:
    .endm

;----------------------------------------------------------------------
; Macro Name: READ_SDA_PIN_ACK
; Description: Set high value on SCL pin
; Input Parameters: none
; Output Parameters: none
;----------------------------------------------------------------------
READ_SDA_PIN_ACK    .macro
    QBBS    ACK_RECIEVED?, R31, R14.b1
    CLR     R16, R16, ICSS_I2C_ACK_RECIEVED_BIT
    JMP     NO_ACK_RECIEVED?
ACK_RECIEVED?:
    SET     R16, R16, ICSS_I2C_ACK_RECIEVED_BIT
NO_ACK_RECIEVED?:
    .endm

;----------------------------------------------------------------------
; Macro Name: COPY_LOCAL_TO_GLOBAL_STATE
; Description: copy next state from global state register.
; Input Parameters: none
; Output Parameters: none
;----------------------------------------------------------------------
COPY_LOCAL_TO_GLOBAL_STATE    .macro
    AND     R13.w0, R17.w0, R17.w0
    .endm


;************************************************************************************
;
;   Macro: RAISE_INTERRUPT_MEM_FOR_HOST
;
;   raise the interrupt memory for telling host which instance raise the interrupt.
;   
;   PEAK cycles:
;       
;   Pseudo code:
;       
;
;   Parameters:
;      None: 
;
;   Returns:
;      None
;
;************************************************************************************
RAISE_INTERRUPT_MEM_FOR_HOST    .macro    arg1
    LDI     TEMP_REG4.w0, 0x0000
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET+2, 2

    LDI     TEMP_REG5.w0, ICSS_I2C_CONFIG_MEMORY
    ADD     TEMP_REG5.w0, TEMP_REG5.w0, IRQ_COMMON_REGISTER_OFFSET
    LBCO    &TEMP_REG4.w0, ICSS_DMEM0_CONST, TEMP_REG5.w0, 2
    SET     TEMP_REG4.w0, TEMP_REG4.w0, R14.b3
    SBCO    &TEMP_REG4.w0, ICSS_DMEM0_CONST, TEMP_REG5.w0, 2
    UPDATE_NEXT_LOCAL_STATE arg1
    STATE_TASK_OVER
    .endm


;************************************************************************************
;
;   Macro: RAISE_INTERRUPT_FOR_HOST
;
;   raise the interrupt memory for telling host which instance raise the interrupt.
;   
;   PEAK cycles:
;       
;   Pseudo code:
;       
;
;   Parameters:
;      None: 
;
;   Returns:
;      None
;
;************************************************************************************
RAISE_INTERRUPT_FOR_HOST    .macro    arg1
    LDI     TEMP_REG5.w0, ICSS_INTC_SRSR1
    .if $defined("PRU0")
    LDI32   TEMP_REG6, ICSS_I2C_INTC_PRU0_BIT_VAL
    .else
    LDI32   TEMP_REG6, ICSS_I2C_INTC_PRU1_BIT_VAL
    .endif
    SBCO    &TEMP_REG6, ICSS_INTC_CONST, TEMP_REG5.w0, 4
    UPDATE_NEXT_LOCAL_STATE arg1
    STATE_TASK_OVER
    .endm

;************************************************************************************
;
;   Macro: CHECK_INTERRUPT_RECEIVED
;
;   raise the interrupt memory for telling host which instance raise the interrupt.
;   
;   PEAK cycles:
;       
;   Pseudo code:
;       
;
;   Parameters:
;      arg1: Label on new memory location if interrupt is ACKed
;      arg2: Label on new memory location if interrupt is not ACKed
;
;   Returns:
;      None
;
;************************************************************************************
CHECK_INTERRUPT_RECEIVED    .macro    arg1, arg2
    LDI     TEMP_REG5.w0, ICSS_I2C_CONFIG_MEMORY
    ADD     TEMP_REG5.w0, TEMP_REG5.w0, IRQ_COMMON_REGISTER_OFFSET
    LBCO    &TEMP_REG4.w0, ICSS_DMEM0_CONST, TEMP_REG5.w0, 2
    QBBC    INTERRUPT_RECEIVED_JMP_STATE?, TEMP_REG4.w0, R14.b3
    JMP     INTERRUPT_RECEIVED_REPEAT_STATE?

INTERRUPT_RECEIVED_JMP_STATE?:
    UPDATE_NEXT_LOCAL_STATE arg1
    STATE_TASK_OVER

INTERRUPT_RECEIVED_REPEAT_STATE?:
    UPDATE_NEXT_LOCAL_STATE arg2
    STATE_TASK_OVER
    .endm


;************************************************************************************
;
;   Macro: UPDATE_NEXT_GLOBAL_STATE
;
;    update next state in state keep register
;   
;   PEAK cycles:
;       
;   Pseudo code:
;       
;
;   Parameters:
;      arg1: Label on next memory location
;      
;
;   Returns:
;      None
;
;************************************************************************************
UPDATE_NEXT_GLOBAL_STATE    .macro    arg1
    LDI     R17.w0, $CODE(arg1)
    .endm

;************************************************************************************
;
;   Macro: READ_ADDRESS_REGISTER
;
;   Read the address register for slave address
;   
;   PEAK cycles:
;       
;   Pseudo code:
;       
;
;   Parameters:
;      arg1: Label on next memory location
;      
;
;   Returns:
;      None
;
;************************************************************************************
READ_ADDRESS_REGISTER    .macro    arg1
    QBBS    read_address_register_10bits?, R16, ICSS_I2C_ADDRESSING_MODE_BIT 
    LDI     TEMP_REG4.w0, 0x007F
    JMP     read_address_register_done?

read_address_register_10bits?:
    LDI     TEMP_REG4.w0, 0x03FF

read_address_register_done?:
    LBBO    &R13.w2, R10, ICSS_I2C_SA_OFFSET, 2

    ;debug code 
    .if $defined("DEBUG_CODE")
    LDI R13.w2, 0x53 
    .endif

    AND     R13.w2, R13.w2, TEMP_REG4.w0
    UPDATE_NEXT_LOCAL_STATE arg1
    STATE_TASK_OVER
    .endm

;************************************************************************************
;
;   Macro: READ_RW_REGISTER_BIT
;
;   Read the RW bit to find read or write operation
;   
;   PEAK cycles:
;       
;   Pseudo code:
;       
;
;   Parameters:
;      arg1: Label on new memory location
;      
;
;   Returns:
;      None
;
;************************************************************************************
READ_RW_REGISTER_BIT    .macro    arg1
    QBBS    read_rw_register_bit_10bits?, R16, ICSS_I2C_ADDRESSING_MODE_BIT
    LSL     R13.w2, R13.w2, 9
    QBBC    slave_address_w_setup?, R16, ICSS_I2C_READ_WRITE_BIT
    SET     R13.w2, R13.w2, 8
    JMP     slave_address_rw_setup_return?
slave_address_w_setup?:
    CLR     R13.w2, R13.w2, 8
slave_address_rw_setup_return?:
    JMP     read_rw_register_bit_done?

read_rw_register_bit_10bits?:
    LSL     R13.b3, R13.b3, 1
    QBBC    slave_address_w_setup_10bits?, R16, ICSS_I2C_READ_WRITE_BIT
    SET     R13.b3, R13.b3, 0
    JMP     slave_address_rw_setup_return_10bits?
slave_address_w_setup_10bits?:
    CLR     R13.b3, R13.b3, 0
slave_address_rw_setup_return_10bits?:
    AND     R13.b3, R13.b3, 0x07
    OR      R13.b3, R13.b3, 0xF0

read_rw_register_bit_done?:
    AND     R15.b0, R15.b0, 0x00
    AND     R15.b2, R15.b2, 0x00
    UPDATE_NEXT_LOCAL_STATE arg1
    STATE_TASK_OVER
   .endm
   

;************************************************************************************
;
;   Macro: SEND_TX_DATA_CHECK_FOR_ACK
;
;   Description: send 8 bits of Tx data
;              check if ACK is recieved from slave or not.
;              keep on sending until all data is sent.
;   
;   PEAK cycles:
;       
;   Pseudo code:
;       
;
;   Parameters:
;      arg1 is register to be used for reading that data.
;      arg2 is Label on new memory location for TX DATA complete
;      arg3 is Label on new memory location for No ACK
;   Returns:
;      None
;
;************************************************************************************
SEND_TX_DATA_CHECK_FOR_ACK    .macro    arg1, arg2, arg3
;
;  put the most significant bit of the data byte on SDA (SCL is low)
;
tx_data_sda_begin?:
    QBBC    tx_data_sda_low?, arg1, 7
    SET_SDA_PIN_HIGH
    JMP     tx_data_sda_continue?
tx_data_sda_low?:
    SET_SDA_PIN_LOW
tx_data_sda_continue?:
    PEC_UPDATE arg1, 7
    LSL     arg1, arg1, 1
    ADD     R15.b0, R15.b0, 0x01
    LDI     R13.w0, $CODE(tx_data_scl_begin?)
    STATE_TASK_OVER

;
;  release SCL
;
tx_data_scl_begin?:
    SET_SCL_PIN_HIGH
    LDI     R13.w0, $CODE(tx_data_sda_read?)
    STATE_TASK_OVER

;
;  SCL high period (waits here while a target stretches the clock)
;
tx_data_sda_read?:
    WAIT_SCL_HIGH
    LDI     R13.w0, $CODE(tx_data_scl_end?)
    STATE_TASK_OVER

;
;  pull SCL low; after the 8th bit release SDA on this same edge, so the
;  target can drive its ACK
;
tx_data_scl_end?:
    SET_SCL_PIN_LOW
    QBGT    tx_sda_next_bit?, R15.b0, 0x08
    SET_SDA_PIN_HIGH
    LDI     R13.w0, $CODE(tx_data_ack_begin?)
    STATE_TASK_OVER

tx_sda_next_bit?:
    LDI     R13.w0, $CODE(tx_data_sda_begin?)
    STATE_TASK_OVER

;
;  SDA was released in tx_data_scl_end; this tick keeps the 4-tick bit timing
;
tx_data_ack_begin?:
    LDI     R13.w0, $CODE(tx_data_ack_scl_begin?)
    STATE_TASK_OVER

;
;  release SCL for the ACK bit
;
tx_data_ack_scl_begin?:
    SET_SCL_PIN_HIGH
    LDI     R13.w0, $CODE(tx_data_ack_read?)
    STATE_TASK_OVER

;
;  sample the ACK bit once SCL is high
;
tx_data_ack_read?:
    WAIT_SCL_HIGH
    READ_SDA_PIN_ACK
    LDI     R13.w0, $CODE(tx_data_ack_scl_end?)
    STATE_TASK_OVER

;
;  pull SCL low; on ACK send the next byte or finish
;
tx_data_ack_scl_end?:
    SET_SCL_PIN_LOW
    QBBS    tx_data_ack_not_recieved?, R16, ICSS_I2C_ACK_RECIEVED_BIT
    ADD     R15.b2, R15.b2, 0x01
    LDI     R15.b0, 0x00
    QBGT    tx_mode_continue?, R15.b2, R15.b3
    LDI     R15.b2, 0x00
    LDI     R13.w0, $CODE(arg2)
    STATE_TASK_OVER
tx_mode_continue?:
    TX_FETCH_BYTE arg1
    LDI     R13.w0, $CODE(tx_data_sda_begin?)
    STATE_TASK_OVER

;
;  the target did not acknowledge a data byte
;
tx_data_ack_not_recieved?:
    LDI     TEMP_REG4.w0, DATA_ACKNOWLDEGE_FAILED
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    LDI     R13.w0, $CODE(arg3)
    STATE_TASK_OVER

    .endm





;************************************************************************************
;
;   Macro: I2C_SETUP_IEP_COUNTER
;
;   Description: Setup the IEP timer counter for periodic interrupt.
;   
;   PEAK cycles:
;       
;   Pseudo code:
;       
;
;   Parameters:
;      None
;   Returns:
;      None
;
;************************************************************************************

I2C_SETUP_IEP_COUNTER     .macro
    ;read IEP Timer enabled or not
    LBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_GLOBAL_CFG_REG, 4
    QBBS    iep_counter_setup_done?, TEMP_REG1 , 0

    LBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_COUNT_REG, 8
    LDI32   TEMP_REG1 , 0xFFFFFFFF
    LDI32   TEMP_REG2 , 0xFFFFFFFF
    SBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_COUNT_REG, 8

    ;Clear overflow status register
    LBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_GLOBAL_STATUS_REG, 4
    SET     TEMP_REG1 , TEMP_REG1 , 0
    SBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_GLOBAL_STATUS_REG, 4

    ;Clear compare status
    LBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_CMP_STATUS_REG, 4
    LDI     TEMP_REG1.w0 , 0xFFFF
    SBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_CMP_STATUS_REG, 4

    ;Enable IEP counter
    LBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_GLOBAL_CFG_REG, 4
    SET     TEMP_REG1 , TEMP_REG1 , 0
    SBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_GLOBAL_CFG_REG, 4

iep_counter_setup_done?:
    ; First compare = current count + one tick. Starting from the live count
    ; (rather than a constant) is what lets the second PRU join a counter the
    ; first PRU already started.
    LBCO    &IEP_COUNTER_NEXT_VAL1, ICSS_IEP_CONST, ICSS_IEP_COUNT_REG, 8
    ADD     IEP_COUNTER_NEXT_VAL1, IEP_COUNTER_NEXT_VAL1, I2C_GLOBAL_FREQ_REG.w2
    ADC     IEP_COUNTER_NEXT_VAL2, IEP_COUNTER_NEXT_VAL2, 0x00
    SBCO    &IEP_COUNTER_NEXT_VAL1, ICSS_IEP_CONST, I2C_IEP_CMP_REG, 8

    ; Enable this core's compare event without disabling the other core's
    LBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_CMP_CFG_REG, 4
    SET     TEMP_REG1, TEMP_REG1, I2C_IEP_CMP_ENABLE_BIT
    SBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_CMP_CFG_REG, 4

    .endm



;-------------------------------------------------------------------------------------------------------------------------------
; Macro Name: READ_RX_DATA_AND_SEND_ACK
; Description: read 8 bits of Rx data then send ack
;              keep on sending until all data is read.
; Input Parameters: arg1 is register to be used for reading data.
; Input Parameters: arg2 is Label on new memory location for DATA complete
; Output Parameters: none
;----------------------------------------------------------------------------------------------------------------------------
READ_RX_DATA_AND_SEND_ACK    .macro    arg1, arg2
;
;  release SDA so the target can drive it
;
    SET_SDA_PIN_HIGH
rx_data_sda_begin?:
    LDI     R13.w0, $CODE(rx_data_scl_begin?)
    STATE_TASK_OVER

;
;  release SCL for the data bit
;
rx_data_scl_begin?:
    SET_SCL_PIN_HIGH
    LSL     arg1, arg1, 1
    LDI     R13.w0, $CODE(rx_data_sda_read?)
    STATE_TASK_OVER

;
;  sample SDA into the LSB once SCL is high
;
rx_data_sda_read?:
    WAIT_SCL_HIGH
    QBBS    rx_data_sda_high?, R31, R14.b1
    AND     arg1, arg1, 0xFE
    JMP     rx_data_sda_read_continue?
rx_data_sda_high?:
    OR      arg1, arg1, 0x01
rx_data_sda_read_continue?:
    PEC_UPDATE arg1, 0
    ADD     R15.b0, R15.b0, 0x01
    LDI     R13.w0, $CODE(rx_data_scl_end?)
    STATE_TASK_OVER

;
;  pull SCL low; after the 8th bit decide between ACK and NACK
;
rx_data_scl_end?:
    SET_SCL_PIN_LOW
    QBGT    rx_sda_next_bit?, R15.b0, 0x08
    QBBC    rx_data_not_count?, R19.b3, SMB_F_BLKRD
    ; SMBus block read: this byte is the block count. It is reported in the
    ; count register and not stored. The count plus an optional PEC byte must
    ; fit the 8-bit byte index.
    CLR     R19.b3, R19.b3, SMB_F_BLKRD
    QBEQ    rx_count_bad?, arg1, 0x00
    MOV     R15.b3, arg1
    QBBC    rx_count_ok?, R19.b3, SMB_F_PECRX
    QBEQ    rx_count_bad?, arg1, 0xFF
    ADD     R15.b3, R15.b3, 0x01
rx_count_ok?:
    LDI     TEMP_REG4.w0, 0x0000
    MOV     TEMP_REG4.b0, arg1
    SBBO    &TEMP_REG4, R10, ICSS_I2C_CNT_OFFSET, 2
    LDI     R13.w0, $CODE(rx_data_ack_begin?)
    STATE_TASK_OVER
rx_count_bad?:
    LDI     TEMP_REG4.w0, INVALID_DATA_COUNT
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    SET     R19.b3, R19.b3, SMB_F_ERR
    LDI     R15.w2, 0x0101
    LDI     R13.w0, $CODE(rx_data_nack_begin?)
    STATE_TASK_OVER
rx_data_not_count?:
    ADD     R15.b2, R15.b2, 0x01
    QBGT    rx_data_next_state?, R15.b2, R15.b3
    QBBS    rx_data_nack?, R19.b3, SMB_F_SMB
    QBBC    rx_data_next_state?, R16, ICSS_I2C_RECIEVE_NACK_BIT
rx_data_nack?:
    LDI     R13.w0, $CODE(rx_data_nack_begin?)
    STATE_TASK_OVER

rx_data_next_state?:
    LDI     R13.w0, $CODE(rx_data_ack_begin?)
    STATE_TASK_OVER

rx_sda_next_bit?:
    LDI     R13.w0, $CODE(rx_data_sda_begin?)
    STATE_TASK_OVER

;
;  pull SDA low to ACK the byte
;
rx_data_ack_begin?:
    SET_SDA_PIN_LOW
    LDI     R13.w0, $CODE(rx_data_ack_scl_begin?)
    STATE_TASK_OVER

;
;  leave SDA released to NACK the byte
;
rx_data_nack_begin?:
    SET_SDA_PIN_HIGH
    LDI     R13.w0, $CODE(rx_data_ack_scl_begin?)
    STATE_TASK_OVER

;
;  release SCL for the ACK/NACK bit
;
rx_data_ack_scl_begin?:
    SET_SCL_PIN_HIGH
    LDI     R13.w0, $CODE(rx_data_ack_read?)
    STATE_TASK_OVER

;
;  ACK/NACK high period (waits here while a target stretches the clock)
;
rx_data_ack_read?:
    WAIT_SCL_HIGH
    LDI     R13.w0, $CODE(rx_data_ack_scl_end?)
    STATE_TASK_OVER

;
;  pull SCL low, store the byte, then read the next one or finish. The
;  block count byte (byte index still 0) is not stored.
;
rx_data_ack_scl_end?:
    SET_SCL_PIN_LOW
    LDI     R15.b0, 0x00
    QBEQ    rx_mode_continue?, R15.b2, 0x00
    SUB     TEMP_REG4.b0, R15.b2, 1
    SBBO    &arg1, R12, TEMP_REG4.b0, 1
    QBGT    rx_mode_continue?, R15.b2, R15.b3
    LDI     R15.b2, 0x00
    LDI     R13.w0, $CODE(arg2)
    STATE_TASK_OVER

rx_mode_continue?:
    SET_SDA_PIN_HIGH
    LDI     R13.w0, $CODE(rx_data_sda_begin?)
    STATE_TASK_OVER

    .endm


;************************************************************************************
;
;   Macro: I2C_TRANSFER_INIT
;
;   Reset the per-transfer state: PEC, command code, flags (R19), the
;   SCL-low counter and the transmit stream base (R11 = Tx buffer).
;
;************************************************************************************
I2C_TRANSFER_INIT    .macro
    LDI     R19, 0
    LDI     R18.w0, 0
    LDI     R11.w0, ICSS_I2C_INSTANCE0_TX_MEM
    .endm

    .endif	; __icss_i2c_macros_h
