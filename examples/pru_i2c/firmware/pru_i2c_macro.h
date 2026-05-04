; Copyright (C) 2024 Texas Instruments Incorporated - http://www.ti.com/
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
;   File:     icss_i2c_macro.h
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
;   wait until the IEP CMP Event triggers and interrupt
;   
;   PEAK cycles:
;       NA
;   Pseudo code:
;       while(r31.t31==0)
;
;   Parameters:
;      None 
;
;   Returns:
;      None
;
;************************************************************************************
I2C_WAIT_FOR_IEP_CMP    .macro

WAIT_FOR_INT?:
    LBCO   &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_CMP_STATUS_REG, 4
    QBBC    WAIT_FOR_INT?, TEMP_REG1, PRU0_IEP_CMP_STATUS_BIT
    ;;;;QBBC WAIT_FOR_INT?, R31, 31

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
    .if $defined("PRU0")
    QBBS    IEP_INTC_CLEAR?, TEMP_REG1, PRU0_IEP_CMP_STATUS_BIT
    JMP     arg1
    .else
    QBBS    IEP_INTC_CLEAR?, TEMP_REG1, PRU1_IEP_CMP_STATUS_BIT
    JMP     arg1
    .endif
IEP_INTC_CLEAR?:
    
    .if $defined("PRU0")
    SET    TEMP_REG1, TEMP_REG1, PRU0_IEP_CMP_STATUS_BIT
    .else
    SET    TEMP_REG1, TEMP_REG1, PRU1_IEP_CMP_STATUS_BIT
    .endif
    SBCO   &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_CMP_STATUS_REG, 4

    ;Set compare values
    ADD     IEP_COUNTER_NEXT_VAL1, IEP_COUNTER_NEXT_VAL1, I2C_GLOBAL_FREQ_REG.w2
    ADC     IEP_COUNTER_NEXT_VAL2, IEP_COUNTER_NEXT_VAL2, 0x00
    .if $defined("PRU0")
    SBCO    &IEP_COUNTER_NEXT_VAL1, ICSS_IEP_CONST, PRU0_IEP_CMP_REG, 8
    .endif  ;PRU0

    .if $defined("PRU1")
    SBCO    &IEP_COUNTER_NEXT_VAL1, ICSS_IEP_CONST, PRU1_IEP_CMP_REG, 8
    .endif  ;PRU1
    
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
    XIN    BANK0, &R10, 40
   
    ;Jump and link to the next task function
    JAL    TEMP_REG3.w0, R13.w0 ;R13 store RESET MODE 
    
    ;Save context to SPAD
    LDI    R0.b0, 0x00
    XOUT   BANK0, &R10, 40
    
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
;   Macro: SET_SCL_PIN_HIGH
;
;   Set high value on SCL pin
;   
;   PEAK cycles:
;       
;   Pseudo code:
;       
;
;   Parameters:
;       None
;
;   Returns:
;      None
;
;************************************************************************************
SET_SCL_PIN_HIGH    .macro
    SET     R30, R30, R14.b0
    .endm

;************************************************************************************
;
;   Macro: SET_SDA_PIN_HIGH
;
;   Set high value on SDA pin
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
SET_SDA_PIN_HIGH    .macro
    SET     R30, R30, R14.b1 
    .endm



;----------------------------------------------------------------------
; Macro Name: SET_SCL_PIN_LOW
; Description: Set low value on SCL pin
; Input Parameters: none
; Output Parameters: none
;----------------------------------------------------------------------
SET_SCL_PIN_LOW    .macro
    CLR     R30, R30, R14.b0
    .endm

;----------------------------------------------------------------------
; Macro Name: SET_SDA_PIN_LOW
; Description: Set low value on SDA pin
; Input Parameters: Label on new memory location
; Output Parameters: none
;----------------------------------------------------------------------
SET_SDA_PIN_LOW  .macro
   CLR   R30, R30, R14.b1
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
;   Macro: SET_OUTPUT_PIN_VALUE_HIGH
;
;   Set high value on SCL and SDA pin
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
SET_OUTPUT_PIN_VALUE_HIGH    .macro    arg1
    SET_SDA_PIN_HIGH
    SET_SCL_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE arg1
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
;  modify the SDA pin value based on the most significant bit of DATA value
;
tx_data_sda_begin?:
    QBBC    tx_data_sda_low?, arg1, 7
    SET_SDA_PIN_HIGH
    JMP     tx_data_sda_continue?
tx_data_sda_low?:
    SET_SDA_PIN_LOW
tx_data_sda_continue?:
    LSL     arg1, arg1, 1
    ADD     R15.b0, R15.b0, 0x01
    LDI     R13.w0, $CODE(tx_data_scl_begin?)
    STATE_TASK_OVER

;
;  make SCL high for sending SDA bit
;
tx_data_scl_begin?:
    SET_SCL_PIN_HIGH
    LDI     R13.w0, $CODE(tx_data_sda_read?)
    STATE_TASK_OVER

;
;  Jump to next state as this is tx mode, done for matching timing parameter.
;
tx_data_sda_read?:
    LDI     R13.w0, $CODE(tx_data_scl_end?)
    STATE_TASK_OVER

;
;  make SCL low for stop sending SDA bit
;  make decision for sending next data bit or check for ACK
;
tx_data_scl_end?:
    SET_SCL_PIN_LOW
    QBGT    tx_sda_next_bit?, R15.b0, 0x08
    LDI     R13.w0, $CODE(tx_data_ack_begin?)
    STATE_TASK_OVER

tx_sda_next_bit?:
    LDI     R13.w0, $CODE(tx_data_sda_begin?)
    STATE_TASK_OVER

;
;  Change direction of GPIO
;
tx_data_ack_begin?:
    SET_SDA_PIN_INPUT_DIRECTION
    LDI     R13.w0, $CODE(tx_data_ack_scl_begin?)
    STATE_TASK_OVER

;
;  make SCL high for reading ACK bit
;
tx_data_ack_scl_begin?:
    SET_SCL_PIN_HIGH
    LDI     R13.w0, $CODE(tx_data_ack_read?)
    STATE_TASK_OVER

;
;  make SDA line for reading ACK bit
;
tx_data_ack_read?:
    READ_SDA_PIN_ACK
    QBBS    tx_data_ack_read_next_state?, R16, ICSS_I2C_SMBUS_BURST_BIT
    LDI     R13.w0, $CODE(tx_data_ack_scl_end?)
    STATE_TASK_OVER

tx_data_ack_read_next_state?:
    LDI     R13.w0, $CODE(tx_data_ack_scl_end_v2?)
    STATE_TASK_OVER

;
;  make SCL low
;  read if ACK bit is set then check if data is still left to be sent
;
tx_data_ack_scl_end?:
    SET_SCL_PIN_LOW
    SET_SDA_PIN_OUTPUT_DIRECTION
    QBBS    tx_data_ack_not_recieved?, R16, ICSS_I2C_ACK_RECIEVED_BIT
    ADD     R15.b2, R15.b2, 0x01
    AND     R15.b0, R15.b0, 0x00
    QBGT    tx_mode_continue?, R15.b2, R15.b3
    AND     R15.b2, R15.b2, 0x00
    LDI     R13.w0, $CODE(arg2)
    STATE_TASK_OVER
tx_mode_continue?:
    LBBO    &arg1, R11, R15.b2, 1
    LDI     R13.w0, $CODE(tx_data_sda_begin?)

    STATE_TASK_OVER

;
;  make SCL low
;  read if ACK bit is set then check if data is still left to be sent
;

tx_data_ack_scl_end_v2?:
    SET_SCL_PIN_LOW
    SET_SDA_PIN_OUTPUT_DIRECTION
    QBBS    tx_data_ack_not_recieved?, R16, ICSS_I2C_ACK_RECIEVED_BIT
    AND     R15.b0, R15.b0, 0x00
    AND     R15.b2, R15.b2, 0x00
    CLR     R16, R16, ICSS_I2C_SMBUS_BURST_BIT
    LBBO    &arg1, R11, R15.b2, 1
    LDI     R13.w0, $CODE(arg2)
    STATE_TASK_OVER
       

tx_data_ack_not_recieved?:
    LDI     R13.w0, $CODE(no_tx_data_ack_recieved?)
    STATE_TASK_OVER

;
;  if no data ack is recieved, response with no ack in response command
;  raise an interrupt
;
no_tx_data_ack_recieved?:
    SET_SCL_PIN_HIGH
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
    ;Set compare values
    
    LDI     TEMP_REG3.w0, ICSS_I2C_CONFIG_MEMORY
    LBCO    &IEP_COUNTER_NEXT_VAL1, ICSS_DMEM0_CONST, TEMP_REG3.w0, 8
    LBCO    &TEMP_REG2, ICSS_IEP_CONST, ICSS_IEP_COUNT_REG, 8
    
    ;;;debug code 
    ;;.if $defined("DEBUG_CODE")
    LDI IEP_COUNTER_NEXT_VAL1, 0x9c3
    LDI IEP_COUNTER_NEXT_VAL2, 0x0
    ;;.endif

loop3?:
    ADD     IEP_COUNTER_NEXT_VAL1, IEP_COUNTER_NEXT_VAL1, I2C_GLOBAL_FREQ_REG.w2
    ADC     IEP_COUNTER_NEXT_VAL2, IEP_COUNTER_NEXT_VAL2, 0x00
    ;;QBGT    loop3?, IEP_COUNTER_NEXT_VAL2, TEMP_REG3
    ;;QBLT    loop4?, IEP_COUNTER_NEXT_VAL2, TEMP_REG3
    ;;QBGT    loop3?, IEP_COUNTER_NEXT_VAL1, TEMP_REG2
loop4?:
    ;;ADD     IEP_COUNTER_NEXT_VAL1, IEP_COUNTER_NEXT_VAL1, I2C_GLOBAL_FREQ_REG.w2
    ;;ADC     IEP_COUNTER_NEXT_VAL2, IEP_COUNTER_NEXT_VAL2, 0x00
    SBCO    &IEP_COUNTER_NEXT_VAL1, ICSS_IEP_CONST, PRU0_IEP_CMP_REG, 8

    ;Enable compare events
    .if $defined("PRU0")
    ZERO    &TEMP_REG1, 4
    SET     TEMP_REG1, TEMP_REG1, PRU0_IEP_CMP_ENABLE_BIT
    SBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_CMP_CFG_REG, 4
    .endif  ;PRU0

    .if $defined("PRU1")
    ZERO    &TEMP_REG1, 4
    SET     TEMP_REG1, TEMP_REG1, PRU1_IEP_CMP_ENABLE_BIT
    SBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_CMP_CFG_REG, 4
    .endif  ;PRU1

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
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  release SDA line so slave can drive it.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    SET_SDA_PIN_INPUT_DIRECTION
rx_data_sda_begin?:
    LDI     R13.w0, $CODE(rx_data_scl_begin?)
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL high for reading data value
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
rx_data_scl_begin?:
    SET_SCL_PIN_HIGH
    LSL     arg1, arg1, 1
    LDI     R13.w0, $CODE(rx_data_sda_read?)
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  Read the SDA pin value and store it in the MSB of receive register
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
rx_data_sda_read?:
    QBBS    rx_data_sda_high?, R31, R14.b1
    AND     arg1, arg1, 0xFE
    JMP     rx_data_sda_read_continue?
rx_data_sda_high?:
    OR      arg1, arg1, 0x01
rx_data_sda_read_continue?:
    ADD     R15.b0, R15.b0, 0x01
    LDI     R13.w0, $CODE(rx_data_scl_end?)
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL low for finishing data read
;  also check if all the data needed to read is over then send ACK
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
rx_data_scl_end?:
    SET_SCL_PIN_LOW
    QBGT    rx_sda_next_bit?, R15.b0, 0x08
    ADD     R15.b2, R15.b2, 0x01
    QBBS    rx_data_next_state?, R16, ICSS_I2C_SMBUS_BURST_BIT
    QBGT    rx_data_next_state?, R15.b2, R15.b3
    QBBC    rx_data_next_state?, R16, ICSS_I2C_RECIEVE_NACK_BIT
    LDI     R13.w0, $CODE(rx_data_nack_begin?)
    SET_SDA_PIN_OUTPUT_DIRECTION
    STATE_TASK_OVER

rx_data_next_state?:
    LDI     R13.w0, $CODE(rx_data_ack_begin?)
    SET_SDA_PIN_OUTPUT_DIRECTION
    STATE_TASK_OVER    

rx_sda_next_bit?:
    LDI     R13.w0, $CODE(rx_data_sda_begin?)
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SDA line low to send an ACK.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
rx_data_ack_begin?:
   ;SET_SDA_PIN_OUTPUT_DIRECTION
   SET_SDA_PIN_HIGH
    LDI     R13.w0, $CODE(rx_data_ack_scl_begin?)
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SDA line low to send an NACK.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
rx_data_nack_begin?:
    SET_SDA_PIN_HIGH
    LDI     R13.w0, $CODE(rx_data_ack_scl_begin?)
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL high for reading ACK bit
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
rx_data_ack_scl_begin?:
    SET_SCL_PIN_HIGH
    SBBO    &R15.b3, R10, ICSS_I2C_CNT_OFFSET, 1
    LDI     R13.w0, $CODE(rx_data_ack_read?)
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  jump to next state, done to match the timing
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
rx_data_ack_read?:
    QBBS    rx_data_ack_read_next_state?, R16, ICSS_I2C_SMBUS_BURST_BIT
    LDI     R13.w0, $CODE(rx_data_ack_scl_end?)
    STATE_TASK_OVER

rx_data_ack_read_next_state?:
    LDI     R13.w0, $CODE(rx_data_ack_scl_end_v2?)
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL low for finishing data read
;  also check if all the data needed to read is over then finish read
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
rx_data_ack_scl_end?:
    SET_SCL_PIN_LOW
    AND     R15.b0, R15.b0, 0x00
    SUB     TEMP_REG4.b0, R15.b2, 1
    SBBO    &arg1, R12, TEMP_REG4.b0, 1
    QBGT    rx_mode_continue?, R15.b2, R15.b3
    AND     R15.b2, R15.b2, 0x00
    LDI     R13.w0, $CODE(arg2)
    ; SET_SDA_PIN_INPUT_DIRECTION
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL low for finishing data read
;  also check if all the data needed to read is over then finish read
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
rx_data_ack_scl_end_v2?:
    SET_SCL_PIN_LOW
    AND     R15.b0, R15.b0, 0x00
    AND     R15.b2, R15.b2, 0x00
    CLR     R16, R16, ICSS_I2C_SMBUS_BURST_BIT
    LDI     R13.w0, $CODE(arg2)
    STATE_TASK_OVER

rx_mode_continue?:
    SET_SDA_PIN_INPUT_DIRECTION
    LDI     R13.w0, $CODE(rx_data_sda_begin?)
    STATE_TASK_OVER

    .endm


;************************************************************************************
;
;   Macro: SET_SDA_PIN_INPUT_DIRECTION
;
;   Description: Setup the Control register for SDA pin input 
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
SET_SDA_PIN_INPUT_DIRECTION .macro  
    
   ;; For Am263x LDI32 r29, 0x50D00824
   ;for Am261x
    LDI32 r29, 0x50D00818
    LBBO &r28, r29, 0, 4
    SET r28, r28, 9
    SBBO  &r28, r29, 0, 4

    .endm

;************************************************************************************
;
;   Macro: SET_SDA_PIN_OUTPUT_DIRECTION
;
;   Description: Clear the Control register for SDA pin output.
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

SET_SDA_PIN_OUTPUT_DIRECTION .macro 

   ;; For Am263x LDI32 r29, 0x50D00824
   ;for Am261x
    LDI32 r29, 0x50D00818
    LBBO &r28, r29, 0, 4
    LDI32 R27, 0xFFFFFDFF
    AND r28, r28, r27
    SBBO  &r28, r29, 0, 4

    .endm
.endif	; __icss_i2c_macros_h