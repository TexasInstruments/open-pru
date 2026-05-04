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

;--------------------------------------------------------------------------------------
;   File:     pru_i2c_main.asm
;
;   Brief:   Firmware for ICSS PRU
;--------------------------------------------------------------------------------------

; CCS/makefile specific settings
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file
 
    .global     main
    .sect       ".text"

;----------------------------------Includes----------------------------------------------------

 .include "pru_i2c_firmware_version.h"
 .include "pru_i2c_macro.h"
 .include "../../../source/firmware/common/icss_constant_defines.inc"
 .include "../../../source/firmware/common/icss_cfg_regs.inc"
 .include "../../../source/firmware/common/icss_iep_regs.inc"
 .include "../../../source/firmware/common/icss_intc_regs.inc"
;--------------------------------------------------------------------------------------

;
; symbols
;

    .asg    R1, TEMP_REG1                       ; temporary register 1
    .asg    R2, TEMP_REG2                       ; temporary register 2
    .asg    R3, TEMP_REG3                       ; temporary register 3
    .asg    R4, TEMP_REG4                       ; temporary register 4
    .asg    R5, TEMP_REG5                       ; temporary register 5
    .asg    R6, TEMP_REG6                       ; temporary register 6
    .asg    R7, TEMP_REG7                       ; temporary register 7
    .asg    R8, TEMP_REG8                       ; temporary register 8

    .asg    R20, IEP_COUNTER_NEXT_VAL1
    .asg    R21, IEP_COUNTER_NEXT_VAL2
    .asg    R22, I2C_GLOBAL_FREQ_REG




PRU0_IEP_CMP_REG                    .set    ICSS_IEP_CMP0_REG
PRU1_IEP_CMP_REG                    .set    ICSS_IEP_CMP1_REG
PRU0_IEP_CMP_ENABLE_BIT             .set    1
PRU1_IEP_CMP_ENABLE_BIT             .set    2
PRU0_IEP_CMP_STATUS_BIT             .set    0
PRU1_IEP_CMP_STATUS_BIT             .set    1


; Bank ids for Xfer instructions
BANK0                 .set    10
BANK1                 .set    11
BANK2                 .set    12
;--------------------------------------------------------------------------------------

;********
;* MAIN *
;********

main:

init:
;----------------------------------------------------------------------------
;   Clear the register space
;   Before begining with the application, make sure all the registers are set
;   to 0. PRU has 32 - 4 byte registers: R0 to R31, with R30 and R31 being special
;   registers for output and input respectively.
;----------------------------------------------------------------------------

; Give the starting address and number of bytes to clear.
    ZERO	&R0, 120
  

    ;need to add section to store firmware version in memory'
    ;/////

    ; Enable support of shifting during XIN/XOUT operation
    ENABLE_XIN_XOUT_SHITFTING

    ;Initialize I2C instance 0 registers
    LDI    R10.w0 , ICSS_I2C_INSTANCE0_ADDR
    LDI    R10.w2 , 0x0000
    LDI    R11.w0 , ICSS_I2C_INSTANCE0_TX_MEM
    LDI    R11.w2 , 0x0000
    LDI    R12.w0 , ICSS_I2C_INSTANCE0_RX_MEM
    LDI    R12.w2 , 0x0000
    LDI    R13.w0 , $CODE(RESET_MODE)
    LDI    R13.w2 , 0x0000
    LDI32  R14 , 0x00000000
    LDI32  R15 , 0x00000000
    LDI32  R16 , 0x00000000
    LDI32  R17 , 0x00000000
    LDI32  R18 , 0x00000000
    LDI32  R19 , 0x00000000
    
    LDI    R0.b0, 0x00
    XOUT   BANK0, &R10, 40
    
    ;add a variable delay for N cyles to avoid contention
    ;between PRU0 and PRU1 if both out of reset at the same time.
    ;;DELAY   DELAY_CYCLE

    ZERO    &R0, 124        ;Zero all registers

    ;decide the global working frequency for I2C FW
    ;Load frequency from DMEM
    LDI     TEMP_REG1.w0, ICSS_I2C_CONFIG_MEMORY
    ADD     TEMP_REG1.w0, TEMP_REG1.w0, I2C_BUS_FREQUENCY_OFFSET
    LBCO    &I2C_GLOBAL_FREQ_REG.w0, ICSS_DMEM0_CONST, TEMP_REG1.w0, 4
    
    ;Debug code for testing 
    .if $defined("DEBUG_CODE")
    LDI TEMP_REG1.w0, ICSS_I2C_400KHZ_FREQ
    LDI TEMP_REG1.w2, IEP_CMP_INCREMENT_VAL_400KHZ
    MOV I2C_GLOBAL_FREQ_REG,  TEMP_REG1
    .endif

    ; Setup the IEP Timer Counter
    I2C_SETUP_IEP_COUNTER

    ; jump to task_loop based on global frequency
    QBEQ   TASK_LOOP_100Khz, I2C_GLOBAL_FREQ_REG.w0, ICSS_I2C_100KHZ_FREQ
    QBEQ   TASK_LOOP_400Khz, I2C_GLOBAL_FREQ_REG.w0, ICSS_I2C_400KHZ_FREQ
    QBEQ   TASK_LOOP_1Mhz, I2C_GLOBAL_FREQ_REG.w0, ICSS_I2C_1MHZ_FREQ
    
ERROR_LOOP:
    LBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_GLOBAL_CFG_REG, 4
    CLR     TEMP_REG1 , TEMP_REG1 , 0
    SBCO    &TEMP_REG1, ICSS_IEP_CONST, ICSS_IEP_GLOBAL_CFG_REG, 4
    LDI     TEMP_REG5.w0, ICSS_INTC_SRSR1
    LDI32   TEMP_REG6, 0x00400000
    SBCO    &TEMP_REG6, ICSS_INTC_CONST, TEMP_REG5.w0, 4
    QBA     ERROR_LOOP


;----------------------------------------------------------
; Task for 100Khz mode
; 1) Waiting for cmp event 
; 2) Clear the event 
; 3) Perform the i2c wave transition
;----------------------------------------------------------
TASK_LOOP_100Khz:
    ;wait for the IEP CMP event 1 to happen
IEP_CHECK_EVENT0:
    I2C_WAIT_FOR_IEP_CMP
    
    ; clear IEP CMP event and interrupt associated with it.
    I2C_IEP_INTC_CLEAR_EVENT IEP_CHECK_EVENT0

    ;perform the i2c wave transition 
    I2C_WAVE_FUNCTION0
    
    QBA  TASK_LOOP_100Khz


;----------------------------------------------------------
; Task for 400Khz mode
; 1) Waiting for cmp event 
; 2) Clear the event 
; 3) Perform the i2c wave transition
;----------------------------------------------------------
TASK_LOOP_400Khz:
    ;wait for the IEP CMP event 1 to happen
IEP_CHECK_EVENT1:
    I2C_WAIT_FOR_IEP_CMP
    
    ; clear IEP CMP event and interrupt associated with it.
    I2C_IEP_INTC_CLEAR_EVENT IEP_CHECK_EVENT1

    ;perform the i2c wave transition 
    I2C_WAVE_FUNCTION0
    
    QBA   TASK_LOOP_400Khz


;----------------------------------------------------------
; Task for 1MHz mode
; 1) Waiting for cmp event 
; 2) Clear the event 
; 3) Perform the i2c wave transition
;----------------------------------------------------------

TASK_LOOP_1Mhz:
    ;wait for the IEP CMP event 1 to happen
IEP_CHECK_EVENT2:
    I2C_WAIT_FOR_IEP_CMP
    
    ; clear IEP CMP event and interrupt associated with it.
    I2C_IEP_INTC_CLEAR_EVENT IEP_CHECK_EVENT2

    ;perform the i2c wave transition 
    I2C_WAVE_FUNCTION0
    
    QBA   TASK_LOOP_1Mhz




;-----------------------------------------------------------I2C states----------------------------------------------------------------------------------
; 
;
; In all state, the processing is broken into multiple state we have a tight budget of 
; 20 cycles per state.
;
;-----------------------------------------------------------

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  This is the initial mode in which comes out of reset
;  It checks if the enabled bit is set or not.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RESET_MODE:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_CON_OFFSET, 4

    ;Debug Code
    .if $defined("DEBUG_CODE")
    SET TEMP_REG4, TEMP_REG4, ICSS_I2C_MODULE_ENABLE_BIT
    .endif

    QBBC    RESET_MODE_RETURN, TEMP_REG4, ICSS_I2C_MODULE_ENABLE_BIT
    UPDATE_NEXT_LOCAL_STATE CHECK_SETUP_COMMAND

RESET_MODE_RETURN:
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  In this state, firmware is only out of reset
;  The only command it can accept in the state is setup command.
;  if any other command is passed it will send invalid command response back
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
CHECK_SETUP_COMMAND:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4

    ;debug code
    .if $defined("DEBUG_CODE")
    LDI  TEMP_REG4, 0
    LDI TEMP_REG4.w2, ICSS_I2C_SETUP_CMD
    .endif

    QBEQ    CHECK_SETUP_COMMAND_RETURN, TEMP_REG4.w2, 0x00
    QBNE    CHECK_SETUP_COMMAND_ERROR, TEMP_REG4.w2, ICSS_I2C_SETUP_CMD
     
    UPDATE_NEXT_LOCAL_STATE SETUP_I2C_PRU_PIN_NUM
    STATE_TASK_OVER

CHECK_SETUP_COMMAND_ERROR:
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_ERROR
    LDI     TEMP_REG4.w0, INVALID_COMMAND
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    STATE_TASK_OVER

CHECK_SETUP_COMMAND_RETURN:
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  This state load the PRU pins no. for the instance.
;  SCL -> PRU GPO and SDA -> PRU GPI pins numbers 
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SETUP_I2C_PRU_PIN_NUM:
    LBBO    &R14, R10, ICSS_I2C_PRU_PIN_OFFSET, 4

    ;debug code
    .if $defined("DEBUG_CODE")
    LDI R14.b0, 1
    LDI R14.b1, 2
    .endif

    LDI     R14.b3, 0x00
    UPDATE_NEXT_LOCAL_STATE SETUP_I2C_INST_ID
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  This instance id of the FW is being loaded into the register.
;  This id helps the FW to decide which bit to raise high when raising an interrupt.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SETUP_I2C_INST_ID:
    LBBO    &R14.b3, R10, ICSS_I2C_PRU_INST_ID_OFFSET, 1

    ;debug code
    .if $defined("DEBUG_CODE")
    LDI R14.b3, 0
    .endif
    UPDATE_NEXT_LOCAL_STATE SETUP_I2C_SCL_SDA_HIGH
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  This set the SCL clk line and SDA data line high.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SETUP_I2C_SCL_SDA_HIGH:
    SET_SCL_PIN_HIGH
    SET_SDA_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE SETUP_I2C_TX_FIFO_SIZE
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  This configures the Tx fifo size of the firmware
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SETUP_I2C_TX_FIFO_SIZE:
    LBBO    &R16.b2, R10, ICSS_I2C_BUF_OFFSET, 1

   ;debug code 
   .if $defined("DEBUG_CODE")
   LDI R16.b2, 12
   .endif

    UPDATE_NEXT_LOCAL_STATE SETUP_I2C_RX_FIFO_SIZE
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  This configures the Rx fifo size of the firmware
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SETUP_I2C_RX_FIFO_SIZE:
    LBBO    &R16.b3, R10, ICSS_I2C_BUF_OFFSET+1, 1

    ;debug code 
    .if $defined("DEBUG_CODE")
    LDI R16.b3, 12
    .endif
    UPDATE_NEXT_LOCAL_STATE SETUP_I2C_MASTER_SLAVE_MODE
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  This configures the firmware into master or slave mode
;  currently only master mode is supported.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SETUP_I2C_MASTER_SLAVE_MODE:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_CON_OFFSET, 4

   ;debug code 
   .if $defined("DEBUG_CODE")
    SET TEMP_REG4, TEMP_REG4, ICSS_I2C_MASTER_SLAVE_MODE_BIT
   .endif

    QBBC    SETUP_I2C_MASTER_SLAVE_ERROR, TEMP_REG4, ICSS_I2C_MASTER_SLAVE_MODE_BIT
    UPDATE_NEXT_LOCAL_STATE SETUP_I2C_ADDRESSING_MODE
    STATE_TASK_OVER

SETUP_I2C_MASTER_SLAVE_ERROR:
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_ERROR
    LDI     TEMP_REG4.w0, MASTER_SLAVE_MODE_FAILED
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  This configures the firmware for 10 bits or 8 bits addressing mode
;  currently only 7 bits mode is supported.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SETUP_I2C_ADDRESSING_MODE:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_CON_OFFSET, 4
    QBBS    SETUP_I2C_ADDRESSING_10BIT, TEMP_REG4, ICSS_I2C_ADDRESSING_MODE_BIT
    CLR     R16, R16, ICSS_I2C_ADDRESSING_MODE_BIT
    JMP     SETUP_I2C_ADDRESSING_DONE

SETUP_I2C_ADDRESSING_10BIT:
    SET     R16, R16, ICSS_I2C_ADDRESSING_MODE_BIT

SETUP_I2C_ADDRESSING_DONE:
    UPDATE_NEXT_LOCAL_STATE SETUP_I2C_START_CTRL
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  This configures the firmware for sending start bit at beginning or not
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SETUP_I2C_START_CTRL:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_CON_OFFSET, 4
    
    ;debug code 
   .if $defined("DEBUG_CODE")
    SET TEMP_REG4, TEMP_REG4, ICSS_I2C_START_BIT
   .endif

    QBBC    SETUP_I2C_NO_START_CTRL, TEMP_REG4, ICSS_I2C_START_BIT
    SET     R16, R16, ICSS_I2C_START_BIT
    JMP     SETUP_I2C_START_DONE

SETUP_I2C_NO_START_CTRL:
    CLR     R16, R16, ICSS_I2C_START_BIT

SETUP_I2C_START_DONE:
    UPDATE_NEXT_LOCAL_STATE SETUP_I2C_STOP_CTRL
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  This configures the firmware for sending stop bit at end of data transfer
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SETUP_I2C_STOP_CTRL:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_CON_OFFSET, 4
    
    ;debug code 
    .if $defined("DEBUG_CODE")
    SET TEMP_REG4, TEMP_REG4, ICSS_I2C_STOP_BIT
    .endif

    QBBC    SETUP_I2C_NO_STOP_CTRL, TEMP_REG4, ICSS_I2C_STOP_BIT
    SET     R16, R16, ICSS_I2C_STOP_BIT
    JMP     SETUP_I2C_STOP_DONE

SETUP_I2C_NO_STOP_CTRL:
    CLR     R16, R16, ICSS_I2C_STOP_BIT

SETUP_I2C_STOP_DONE: 
    UPDATE_NEXT_LOCAL_STATE SETUP_I2C_SMBUS_BURST_CTRL
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  This configures the firmware for sending start bit at beginning or not
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SETUP_I2C_SMBUS_BURST_CTRL:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_CON_OFFSET, 4
    QBBC    SETUP_I2C_NO_SMBUS_BURST_CTRL, TEMP_REG4, ICSS_I2C_SMBUS_BURST_BIT
    SET     R16, R16, ICSS_I2C_SMBUS_BURST_BIT
    JMP     SETUP_I2C_SMBUS_BURST_DONE

SETUP_I2C_NO_SMBUS_BURST_CTRL:
    CLR     R16, R16, ICSS_I2C_SMBUS_BURST_BIT

SETUP_I2C_SMBUS_BURST_DONE:
    UPDATE_NEXT_LOCAL_STATE SETUP_I2C_NACK_CTRL
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  This configures the firmware for ending the read operation with NACK or not
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SETUP_I2C_NACK_CTRL:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_CON_OFFSET, 4
    
    ;debug code 
    .if $defined("DEBUG_CODE")
    SET TEMP_REG4, TEMP_REG4, ICSS_I2C_RECIEVE_NACK_BIT
    .endif

    QBBC    SETUP_I2C_NO_NACK_CTRL, TEMP_REG4, ICSS_I2C_RECIEVE_NACK_BIT
    SET     R16, R16, ICSS_I2C_RECIEVE_NACK_BIT
    JMP     SETUP_I2C_NACK_DONE



SETUP_I2C_NO_NACK_CTRL:
    CLR     R16, R16, ICSS_I2C_RECIEVE_NACK_BIT

SETUP_I2C_NACK_DONE:
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    
    ;debug code 
    .if $defined("DEBUG_CODE")
    UPDATE_NEXT_LOCAL_STATE  FIRMWARE_READY
    .endif

    LDI     TEMP_REG4.w0, COMMAND_SUCCESS
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  Set the MMap for host to find out which instance raised the interrupt.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RAISE_HOST_INTERRUPT_MEM_FOR_ERROR:
    RAISE_INTERRUPT_MEM_FOR_HOST RAISE_HOST_INTERRUPT_FOR_ERROR

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  Raise interrupt for Host while the firmware was trying to configure
;  setup procedures.
;  after raising the interrupt wait for host to respond.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RAISE_HOST_INTERRUPT_FOR_ERROR:
    RAISE_INTERRUPT_FOR_HOST CHECK_HOST_INTERRUPT_ERROR_RECEIVED

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  check for host to receive the command response
;  then jump to "reset" mode again for reconfiguration
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
CHECK_HOST_INTERRUPT_ERROR_RECEIVED:
    CHECK_INTERRUPT_RECEIVED RESET_MODE, RAISE_HOST_INTERRUPT_FOR_ERROR

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  Set the MMap for host to find out which instance raised the interrupt.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RAISE_HOST_INTERRUPT_MEM_FOR_READY:
    RAISE_INTERRUPT_MEM_FOR_HOST RAISE_HOST_INTERRUPT_FOR_READY

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  Raise interrupt for Host while the firmware succesfully does setup configuration
;  but fails to any transaction i.e. read or write.
;  after raising the interrupt wait for host to respond.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RAISE_HOST_INTERRUPT_FOR_READY:
    RAISE_INTERRUPT_FOR_HOST CHECK_HOST_INTERRUPT_READY_RECEIVED

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  check for host to receive the command response
;  then jump to "firmware ready" mode again to do a transaction again
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
CHECK_HOST_INTERRUPT_READY_RECEIVED:
    CHECK_INTERRUPT_RECEIVED FIRMWARE_READY, RAISE_HOST_INTERRUPT_FOR_READY

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  In this state, firmware is out of reset succesfully
;  firmware is ready to accept any command and check if the command word is not "0x0000"
;  then start to check which command is passed.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
FIRMWARE_READY:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
   
   ;debug code 
    .if $defined("DEBUG_CODE")
    LDI  TEMP_REG4.w2, ICSS_I2C_TX_CMD
    .endif

    QBEQ    FIRMWARE_READY_RETURN, TEMP_REG4.w2, 0x00
    UPDATE_NEXT_LOCAL_STATE ICSS_I2C_RESET_CMD_CHECK

FIRMWARE_READY_RETURN:
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the reset command has been passed.
;  if reset command is passed then resets the firmware else check for another command
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ICSS_I2C_RESET_CMD_CHECK:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4

    ;debug code 
    .if $defined("DEBUG_CODE")
    ;;LDI  TEMP_REG4.w2, ICSS_I2C_TX_CMD
    LDI  TEMP_REG4.w2, ICSS_I2C_RX_CMD
    .endif

    QBNE    ICSS_I2C_RESET_CMD_CHECK_RETURN, TEMP_REG4.w2, ICSS_I2C_RESET_CMD
    UPDATE_NEXT_LOCAL_STATE RESET_SCL_SDA_HIGH
    LDI     TEMP_REG4.w0, COMMAND_SUCCESS
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    STATE_TASK_OVER

ICSS_I2C_RESET_CMD_CHECK_RETURN:
    UPDATE_NEXT_LOCAL_STATE ICSS_I2C_SETUP_CMD_CHECK
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  set the value on iep gpo enable register to high value
;  this will pull the line to high impedence and open drain will keep the line high
;  set the value on iep gpo to low value for pulling the line
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RESET_SCL_SDA_HIGH:
    SET_OUTPUT_PIN_VALUE_HIGH RAISE_HOST_INTERRUPT_MEM_FOR_ERROR

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the setup command has been passed.
;  if setup command is passed then redo the firmware setup procedure 
;  else check for another command
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ICSS_I2C_SETUP_CMD_CHECK:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4

    ;debug code 
    .if $defined("DEBUG_CODE")
    ;;LDI  TEMP_REG4.w2, ICSS_I2C_TX_CMD
    LDI  TEMP_REG4.w2, ICSS_I2C_RX_CMD
    .endif

    QBNE    ICSS_I2C_SETUP_CMD_CHECK_RETURN, TEMP_REG4.w2, ICSS_I2C_SETUP_CMD
    UPDATE_NEXT_LOCAL_STATE RESET_MODE
    STATE_TASK_OVER

ICSS_I2C_SETUP_CMD_CHECK_RETURN:
    UPDATE_NEXT_LOCAL_STATE ICSS_I2C_RX_CMD_CHECK
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the Rx command has been passed.
;  if Rx command is passed then start receive data procedure
;  else check for another command
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ICSS_I2C_RX_CMD_CHECK:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4

     ;debug code 
    .if $defined("DEBUG_CODE")
    ;;LDI  TEMP_REG4.w2, ICSS_I2C_TX_CMD
    LDI  TEMP_REG4.w2, ICSS_I2C_RX_CMD
    .endif

    QBNE    ICSS_I2C_RX_CMD_CHECK_RETURN, TEMP_REG4.w2, ICSS_I2C_RX_CMD
    UPDATE_NEXT_LOCAL_STATE RX_MODE
    STATE_TASK_OVER

ICSS_I2C_RX_CMD_CHECK_RETURN:
    UPDATE_NEXT_LOCAL_STATE ICSS_I2C_TX_CMD_CHECK
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the Tx command has been passed.
;  if Tx command is passed then start transmit data procedure
;  else check for another command
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ICSS_I2C_TX_CMD_CHECK:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    
    ;debug code 
    .if $defined("DEBUG_CODE")
    LDI  TEMP_REG4.w2, ICSS_I2C_TX_CMD
    .endif

    QBNE    ICSS_I2C_TX_CMD_CHECK_RETURN, TEMP_REG4.w2, ICSS_I2C_TX_CMD
    UPDATE_NEXT_LOCAL_STATE TX_MODE
    STATE_TASK_OVER

ICSS_I2C_TX_CMD_CHECK_RETURN:
    UPDATE_NEXT_LOCAL_STATE ICSS_SMBUS_QUICK_CHECK
    STATE_TASK_OVER


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the smbus Quick command has been passed.
;  if Quick command is passed then start Quick data procedure
;  else check for another command
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ICSS_SMBUS_QUICK_CHECK:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    QBNE    ICSS_SMBUS_QUICK_CMD_CHECK_NEXT, TEMP_REG4.w2, ICSS_SMBUS_QUICK_CMD
   ;;;; UPDATE_NEXT_LOCAL_STATE QUICK_CMD_MODE
    STATE_TASK_OVER
    
ICSS_SMBUS_QUICK_CMD_CHECK_NEXT:
   ;;;; UPDATE_NEXT_LOCAL_STATE ICSS_SMBUS_SEND_BYTE
    STATE_TASK_OVER
    
    
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the smbus send byte command has been passed.
;  if send byte command is passed then start send byte data procedure
;  else check for another command
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
ICSS_SMBUS_SEND_BYTE:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    QBNE    ICSS_SMBUS_SEND_BYTE_CHECK_NEXT, TEMP_REG4.w2, ICSS_SMBUS_SEND_BYTE_CMD
   ;;;; UPDATE_NEXT_LOCAL_STATE SEND_BYTE_MODE
    STATE_TASK_OVER
    
ICSS_SMBUS_SEND_BYTE_CHECK_NEXT:
   ;;;; UPDATE_NEXT_LOCAL_STATE ICSS_SMBUS_RECEIVE_BYTE
    STATE_TASK_OVER
    
    
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the smbus receive byte command has been passed.
;  if receive byte command is passed then start receive byte data procedure
;  else check for another command
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ICSS_SMBUS_RECEIVE_BYTE:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    QBNE    ICSS_SMBUS_RECEIVE_BYTE_CHECK_NEXT, TEMP_REG4.w2, ICSS_SMBUS_RECEIVE_BYTE_CMD
   ;;;; UPDATE_NEXT_LOCAL_STATE RECEIVE_BYTE_MODE
    STATE_TASK_OVER
    
ICSS_SMBUS_RECEIVE_BYTE_CHECK_NEXT:
    UPDATE_NEXT_LOCAL_STATE ICSS_SMBUS_WRITE_BYTE
    STATE_TASK_OVER
    
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the smbus write byte command has been passed.
;  if write byte command is passed then start write byte data procedure
;  else check for another command
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ICSS_SMBUS_WRITE_BYTE:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    QBNE    ICSS_SMBUS_WRITE_BYTE_CHECK_NEXT, TEMP_REG4.w2, ICSS_SMBUS_WRITE_BYTE_CMD
   ;;;; UPDATE_NEXT_LOCAL_STATE WRITE_BYTE_MODE
    STATE_TASK_OVER
    
ICSS_SMBUS_WRITE_BYTE_CHECK_NEXT:
   ;;;; UPDATE_NEXT_LOCAL_STATE ICSS_SMBUS_READ_BYTE
    STATE_TASK_OVER
   
    
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the smbus read byte command has been passed.
;  if read byte command is passed then start read byte data procedure
;  else check for another command
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
   
ICSS_SMBUS_READ_BYTE:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    QBNE    ICSS_SMBUS_READ_BYTE_CHECK_NEXT, TEMP_REG4.w2, ICSS_SMBUS_READ_BYTE_CMD
  ;;;;  UPDATE_NEXT_LOCAL_STATE READ_BYTE_MODE
    STATE_TASK_OVER
    
ICSS_SMBUS_READ_BYTE_CHECK_NEXT:
  ;;;;  UPDATE_NEXT_LOCAL_STATE ICSS_SMBUS_WRITE_WORD
    STATE_TASK_OVER


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the smbus write word command has been passed.
;  if write word command is passed then start write word data procedure
;  else check for another command
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ICSS_SMBUS_WRITE_WORD:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    QBNE    ICSS_SMBUS_WRITE_WORD_CHECK_NEXT, TEMP_REG4.w2, ICSS_SMBUS_WRITE_WORD_CMD
  ;;;;  UPDATE_NEXT_LOCAL_STATE WRITE_WORD_MODE
    STATE_TASK_OVER
    
ICSS_SMBUS_WRITE_WORD_CHECK_NEXT:
  ;;;;  UPDATE_NEXT_LOCAL_STATE ICSS_SMBUS_READ_WORD
    STATE_TASK_OVER

    
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the smbus read word command has been passed.
;  if read word command is passed then start read word data procedure
;  else check for another command
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ICSS_SMBUS_READ_WORD:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    QBNE    ICSS_SMBUS_READ_WORD_CHECK_NEXT, TEMP_REG4.w2, ICSS_SMBUS_READ_WORD_CMD
  ;;;;  UPDATE_NEXT_LOCAL_STATE READ_WORD_MODE
    STATE_TASK_OVER
    
ICSS_SMBUS_READ_WORD_CHECK_NEXT:
  ;;;;  UPDATE_NEXT_LOCAL_STATE ICSS_SMBUS_BLOCK_WRITE
    STATE_TASK_OVER
   
    
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the smbus block write command has been passed.
;  if block write command is passed then start block write data procedure
;  else check for another command
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ICSS_SMBUS_BLOCK_WRITE:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    QBNE    ICSS_SMBUS_BLOCK_WRITE_CHECK_NEXT, TEMP_REG4.w2, ICSS_SMBUS_BLOCK_WRITE_CMD
   ;;;; UPDATE_NEXT_LOCAL_STATE BLOCK_WRITE_MODE
    STATE_TASK_OVER

ICSS_SMBUS_BLOCK_WRITE_CHECK_NEXT:
    UPDATE_NEXT_LOCAL_STATE ICSS_SMBUS_BLOCK_READ
    STATE_TASK_OVER
    
    
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the smbus block read command has been passed.
;  if block read command is passed then start block read data procedure
;  else no matching command has been found
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    
ICSS_SMBUS_BLOCK_READ:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    QBNE    ICSS_SMBUS_BLOCK_READ_CHECK_NEXT, TEMP_REG4.w2, ICSS_SMBUS_BLOCK_READ_CMD
  ;;;  UPDATE_NEXT_LOCAL_STATE BLOCK_READ_MODE
    STATE_TASK_OVER
    
ICSS_SMBUS_BLOCK_READ_CHECK_NEXT:
    UPDATE_NEXT_LOCAL_STATE ICSS_I2C_READ_SCL_CMD_CHECK
    STATE_TASK_OVER
    
    
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the I2C read SCL command has been passed.
;  if this command requires the host to change the pinmux to input for SCL pin
;  else no matching command has been found
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ICSS_I2C_READ_SCL_CMD_CHECK:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    QBNE    ICSS_I2C_READ_SCL_CMD_CHECK_NEXT, TEMP_REG4.w2, ICSS_I2C_READ_SCL_CMD
    UPDATE_NEXT_LOCAL_STATE READ_SCL_PIN_SETUP
    STATE_TASK_OVER

ICSS_I2C_READ_SCL_CMD_CHECK_NEXT:
    UPDATE_NEXT_LOCAL_STATE ICSS_I2C_RESET_SLAVE_CMD_CHECK
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the I2C reset slave command has been passed.
;  if this command will send 9 clock pulse to device for reseting it.
;  else no matching command has been found
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ICSS_I2C_RESET_SLAVE_CMD_CHECK:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    QBNE    ICSS_I2C_RESET_SLAVE_CMD_CHECK_NEXT, TEMP_REG4.w2, ICSS_I2C_RESET_SLAVE_CMD
    UPDATE_NEXT_LOCAL_STATE RESET_SLAVE_SCL_BEGIN
    STATE_TASK_OVER

ICSS_I2C_RESET_SLAVE_CMD_CHECK_NEXT:
    UPDATE_NEXT_LOCAL_STATE ICSS_I2C_LOOPBACK_CMD_CHECK
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  checks if the I2C reset slave command has been passed.
;  if this command will send 9 clock pulse to device for reseting it.
;  else no matching command has been found
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ICSS_I2C_LOOPBACK_CMD_CHECK:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    QBNE    ICSS_I2C_LOOPBACK_CMD_CHECK_NEXT, TEMP_REG4.w2, ICSS_I2C_LOOPBACK_CMD
    UPDATE_NEXT_LOCAL_STATE LOOPBACK_DATA_COUNT
    STATE_TASK_OVER

ICSS_I2C_LOOPBACK_CMD_CHECK_NEXT:
    UPDATE_NEXT_LOCAL_STATE FIRMWARE_READY_COMMAND_ERROR
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  no known command has been matched with passed cmd 
;  raise an interrupt and respond with error reponse word
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
FIRMWARE_READY_COMMAND_ERROR:
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    LDI     TEMP_REG4.w0, INVALID_COMMAND
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  set the local register to indicate tx mode
;  set the value on iep gpo to low value for pulling the line
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
TX_MODE:
    CLR     R16, R16, ICSS_I2C_READ_WRITE_BIT
    UPDATE_NEXT_GLOBAL_STATE TX_DATA_SDA_BEGIN
    UPDATE_NEXT_LOCAL_STATE SET_SCL_SDA_HIGH
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  set the local register to indicate rx mode
;  set the value on iep gpo to low value for pulling the line
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RX_MODE:
    SET     R16, R16, ICSS_I2C_READ_WRITE_BIT
    UPDATE_NEXT_GLOBAL_STATE RX_DATA_SDA_BEGIN
    UPDATE_NEXT_LOCAL_STATE SET_SCL_SDA_HIGH
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  set the value on iep gpo enable register to high value
;  this will pull the line to high impedence and open drain will keep the line high
;  set the value on iep gpo to low value for pulling the line
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SET_SCL_SDA_HIGH:
    SET_OUTPUT_PIN_VALUE_HIGH SLAVE_ADDRESS_SETUP

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  read the slave address from configuration registers.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SLAVE_ADDRESS_SETUP:
    READ_ADDRESS_REGISTER SLAVE_ADDRESS_RW_SETUP

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  configure the read/write bit in the slave address register for transmission
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SLAVE_ADDRESS_RW_SETUP:    
    READ_RW_REGISTER_BIT DATA_COUNT

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  Read the number of 8 bits data need to be read or writen 
;  also initialize the data count and bit count register to 0
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
DATA_COUNT:
    LBBO    &TEMP_REG4.w0, R10, ICSS_I2C_CNT_OFFSET, 2
    
    ;debug code 
    .if $defined("DEBUG_CODE")
    ; fif count
    LDI  TEMP_REG4.w0, 1
    .endif

    QBLT    DATA_COUNT_ERROR, TEMP_REG4.w0, 0xFF
    QBGT    DATA_COUNT_ERROR, TEMP_REG4.w0, 0x01

    ;debug code 
    .if $defined("DEBUG_CODE")
     LDI R24.b0, 0x53
     LDI r24.b1, 0xFF
     LDI r24.b2, 0x00
     ldi r24.b3, 0x00
     ldi r25.b0, 0x33
    SBBO &r24,  R11, 0, 1
    .endif

    AND     R15.b3, TEMP_REG4.b0, 0xFF
    AND     R15.b2, R15.b2, 0x00
    LBBO    &R15.b1, R11, R15.b2, 1

    UPDATE_NEXT_LOCAL_STATE START_CONDITION_SDA_LOW
    STATE_TASK_OVER

DATA_COUNT_ERROR:
    LDI     TEMP_REG4.w0, INVALID_DATA_COUNT
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SDA low for start condition
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
START_CONDITION_SDA_LOW:
    QBBC    START_CONDITION_SDA_LOW_RETURN, R16, ICSS_I2C_START_BIT
    SET_SDA_PIN_LOW
START_CONDITION_SDA_LOW_RETURN:
    UPDATE_NEXT_LOCAL_STATE START_CONDITION_SCL_LOW
    
    STATE_TASK_OVER
   
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL low for start condition
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
START_CONDITION_SCL_LOW:
    QBBC    START_CONDITION_SCL_LOW_RETURN, R16, ICSS_I2C_START_BIT
    SET_SCL_PIN_LOW
START_CONDITION_SCL_LOW_RETURN:
    UPDATE_NEXT_LOCAL_STATE ADDRESS_SDA_BEGIN
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  modify the SDA pin value based on the most significant bit of Address register
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ADDRESS_SDA_BEGIN:
    RSB     TEMP_REG5.b0, R15.b0, 15
    QBBC    ADDRESS_SDA_LOW, R13.w2, TEMP_REG5.b0
    SET_SDA_PIN_HIGH
    JMP     ADDRESS_SDA_CONTINUE
ADDRESS_SDA_LOW:
    SET_SDA_PIN_LOW
ADDRESS_SDA_CONTINUE:
    ADD     R15.b0, R15.b0, 0x01
    UPDATE_NEXT_LOCAL_STATE ADDRESS_SCL_BEGIN
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL high for sending SDA bit
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ADDRESS_SCL_BEGIN:
    SET_SCL_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE ADDRESS_SDA_READ
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  nothing to be read in address tx mode
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ADDRESS_SDA_READ:
    UPDATE_NEXT_LOCAL_STATE ADDRESS_SCL_END
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL low for ending the transmission of bit
;  also decide the next state based on all the bits have been sent or not.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ADDRESS_SCL_END:
    SET_SCL_PIN_LOW
    QBGT    SDA_NEXT_BIT, R15.b0, 0x08
    UPDATE_NEXT_LOCAL_STATE ADDRESS_ACK_BEGIN
    STATE_TASK_OVER

SDA_NEXT_BIT:
    UPDATE_NEXT_LOCAL_STATE ADDRESS_SDA_BEGIN
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  change the direction of PIN as INPUT.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ADDRESS_ACK_BEGIN:
    SET_SDA_PIN_INPUT_DIRECTION
    UPDATE_NEXT_LOCAL_STATE ADDRESS_ACK_SCL_BEGIN
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL high for reading ACK bit
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ADDRESS_ACK_SCL_BEGIN:
    SET_SCL_PIN_HIGH
    AND     R15.b0, R15.b0, 0x00
    UPDATE_NEXT_LOCAL_STATE ADDRESS_ACK_READ
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SDA line for reading ACK bit
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ADDRESS_ACK_READ:
    READ_SDA_PIN_ACK
    UPDATE_NEXT_LOCAL_STATE ADDRESS_ACK_SCL_END
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL low
;  read if ACK bit is set then start sending or receiving data else indicate no ack
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ADDRESS_ACK_SCL_END:
    SET_SCL_PIN_LOW
    ;SET the dirction for GPIO
    SET_SDA_PIN_OUTPUT_DIRECTION
    QBBS    ADDRESS_ACK_NOT_RECIEVED, R16, ICSS_I2C_ACK_RECIEVED_BIT
    QBBC    ADDRESS_ACK_SCL_END_DONE, R16, ICSS_I2C_ADDRESSING_MODE_BIT
    QBEQ    ADDRESS_ACK_SCL_END_DONE, R15.b2, 0x01
    ADD     R15.b2, R15.b2, 0x01
    UPDATE_NEXT_LOCAL_STATE ADDRESS_SDA_BEGIN
    STATE_TASK_OVER

ADDRESS_ACK_SCL_END_DONE:
    AND     R15.b2, R15.b2, 0x00
    COPY_LOCAL_TO_GLOBAL_STATE
    STATE_TASK_OVER

ADDRESS_ACK_NOT_RECIEVED:
    UPDATE_NEXT_LOCAL_STATE NO_ADDRESS_ACK_RECIEVED
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  if no address ack is recieved, response with no ack in response command
;  raise an interrupt
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
NO_ADDRESS_ACK_RECIEVED:
    SET_SCL_PIN_HIGH
    LDI     TEMP_REG4.w0, ADDRESS_ACKNOWLDEGE_FAILED
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    STATE_TASK_OVER
    
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  start sending TX data and wait for ACK receive
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
TX_DATA_SDA_BEGIN:
    SEND_TX_DATA_CHECK_FOR_ACK R15.b1, DATA_PROCESSING_COMPLETE, RAISE_HOST_INTERRUPT_MEM_FOR_READY

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  start sending RX data and send ACK to device
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RX_DATA_SDA_BEGIN:
    READ_RX_DATA_AND_SEND_ACK R15.b1, DATA_PROCESSING_COMPLETE

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  start sending TX data and wait for ACK receive for smbus Burst Mode
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
TX_DATA_SDA_BEGIN_BURST:
    SEND_TX_DATA_CHECK_FOR_ACK R17.b2, TX_DATA_SDA_BEGIN, RAISE_HOST_INTERRUPT_MEM_FOR_READY
    

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  start sending RX data and send ACK to device for smbus Burst Mode
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
RX_DATA_SDA_BEGIN_BURST:
    READ_RX_DATA_AND_SEND_ACK R15.b3, RX_DATA_SDA_BEGIN
    

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  modify the SDA pin value based on the most significant bit of DATA value
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
CMD_CODE_SDA_BEGIN:
    QBBC    CMD_CODE_SDA_LOW, R15.b1, 7
    SET_SDA_PIN_HIGH
    JMP     CMD_CODE_SDA_CONTINUE
CMD_CODE_SDA_LOW:
    SET_SDA_PIN_LOW
CMD_CODE_SDA_CONTINUE:
    LSL     R15.b1, R15.b1, 1
    ADD     R15.b0, R15.b0, 0x01
    UPDATE_NEXT_LOCAL_STATE CMD_CODE_SCL_BEGIN
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL high for sending SDA bit
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
CMD_CODE_SCL_BEGIN:
    SET_SCL_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE CMD_CODE_SDA_READ
    STATE_TASK_OVER
    

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  Jump to next state as this is tx mode, done for matching timing parameter.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
CMD_CODE_SDA_READ:
    UPDATE_NEXT_LOCAL_STATE CMD_CODE_SCL_END
    STATE_TASK_OVER
    

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL low for stop sending SDA bit
;  make decision for sending next data bit or check for ACK
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
CMD_CODE_SCL_END:
    SET_SCL_PIN_LOW
    QBGT    CMD_CODE_SDA_NEXT_BIT, R15.b0, 0x08
    UPDATE_NEXT_LOCAL_STATE CMD_CODE_ACK_BEGIN
    STATE_TASK_OVER

CMD_CODE_SDA_NEXT_BIT:
    UPDATE_NEXT_LOCAL_STATE CMD_CODE_SDA_BEGIN
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  release SDA line so slave can drive it.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
CMD_CODE_ACK_BEGIN:
    SET_SDA_PIN_HIGH
    SET     R13.w2, R13.w2, 0
    UPDATE_NEXT_LOCAL_STATE CMD_CODE_ACK_SCL_BEGIN
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL high for reading ACK bit
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
CMD_CODE_ACK_SCL_BEGIN:
    SET_SCL_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE CMD_CODE_ACK_READ
    STATE_TASK_OVER


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SDA line for reading ACK bit
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
CMD_CODE_ACK_READ:
    READ_SDA_PIN_ACK
    QBBS    CMD_CODE_ACK_READ_NEXT_STATE, R16, ICSS_I2C_SMBUS_BURST_BIT
    UPDATE_NEXT_LOCAL_STATE CMD_CODE_ACK_SCL_END
    STATE_TASK_OVER

CMD_CODE_ACK_READ_NEXT_STATE:
    UPDATE_NEXT_LOCAL_STATE CMD_CODE_ACK_SCL_END_V2
    STATE_TASK_OVER
    

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL low
;  read if ACK bit is set then check if data is still left to be sent
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
CMD_CODE_ACK_SCL_END:
    SET_SCL_PIN_LOW
    QBBS    CMD_CODE_ACK_NOT_RECIEVED, R16, ICSS_I2C_ACK_RECIEVED_BIT
    AND     R15.b2, R15.b2, 0x00
    AND     R15.b0, R15.b0, 0x00
    QBBS    CMD_CODE_RX, R16, ICSS_I2C_READ_WRITE_BIT
    LBBO    &R15.b1, R11, R15.b2, 1
    UPDATE_NEXT_LOCAL_STATE TX_DATA_SDA_BEGIN
    STATE_TASK_OVER

CMD_CODE_RX:
     UPDATE_NEXT_LOCAL_STATE START_CONDITION_SDA_LOW
     SET    R13.w2, R13.w2, 8
     UPDATE_NEXT_GLOBAL_STATE RX_DATA_SDA_BEGIN
     STATE_TASK_OVER
    
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL low
;  read if ACK bit is set then check if data is still left to be sent
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
CMD_CODE_ACK_SCL_END_V2:
    SET_SCL_PIN_LOW
    QBBS    CMD_CODE_ACK_NOT_RECIEVED, R16, ICSS_I2C_ACK_RECIEVED_BIT
    AND     R15.b2, R15.b2, 0x00
    AND     R15.b0, R15.b0, 0x00
    QBBS    CMD_CODE_RX_V2, R16, ICSS_I2C_READ_WRITE_BIT
    LBBO    &R15.b1, R11, R15.b2, 1
    UPDATE_NEXT_LOCAL_STATE TX_DATA_SDA_BEGIN_BURST
    STATE_TASK_OVER

CMD_CODE_RX_V2:
    UPDATE_NEXT_LOCAL_STATE START_CONDITION_SDA_LOW
    SET    R13.w2, R13.w2, 8
    UPDATE_NEXT_GLOBAL_STATE RX_DATA_SDA_BEGIN_BURST
    STATE_TASK_OVER
     
    
CMD_CODE_ACK_NOT_RECIEVED:
    UPDATE_NEXT_LOCAL_STATE NO_CMD_CODE_ACK_RECIEVED
    STATE_TASK_OVER
    

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  if no data ack is received, response with no ack in response command
;  raise an interrupt
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
NO_CMD_CODE_ACK_RECIEVED:
    LDI     TEMP_REG4.w0, DATA_ACKNOWLDEGE_FAILED
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    STATE_TASK_OVER
    

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  data transmit or receive is over
;  check whether to sent stop bit or not
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
DATA_PROCESSING_COMPLETE:
    SET_SDA_PIN_LOW
    QBBC    COMPLETE_WITH_NO_STOP, R16, ICSS_I2C_STOP_BIT
    UPDATE_NEXT_LOCAL_STATE STOP_CONDITION_SCL_HIGH
    STATE_TASK_OVER

COMPLETE_WITH_NO_STOP:
    UPDATE_NEXT_LOCAL_STATE NO_STOP_CONDITION_SDA_HIGH
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  send stop condition by making SCL high first
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
STOP_CONDITION_SCL_HIGH:
    SET_SCL_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE STOP_CONDITION_SDA_HIGH
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  send stop condition by making SDA high next
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
STOP_CONDITION_SDA_HIGH:
    SET_SDA_PIN_HIGH
    LDI     TEMP_REG4.w0, COMMAND_SUCCESS
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  do not send stop condition by making SDA high first
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
NO_STOP_CONDITION_SDA_HIGH:
    SET_SDA_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE NO_STOP_CONDITION_SCL_HIGH
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  do not send stop condition by making SCL high next
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
NO_STOP_CONDITION_SCL_HIGH:
    SET_SCL_PIN_HIGH
    LDI     TEMP_REG4.w0, COMMAND_SUCCESS
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  prepare to read clk value for 10 clk cycles
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
READ_SCL_PIN_SETUP:
    LDI    R15.w0, 0x0000
    LDI    R15.w2, 0x0A00
    UPDATE_NEXT_LOCAL_STATE READ_SCL_PIN_VALUE
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  read the scl clk value for 10 clk cycles
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
READ_SCL_PIN_VALUE:
    ADD     R15.b0, R15.b0, 0x01
    QBBS    SCL_PIN_VALUE_HIGH, R31, R14.b0
    ADD     R15.b2, R15.b2, 0x01

SCL_PIN_VALUE_HIGH:
    QBGT    READ_SCL_PIN_VALUE_REPEAT, R15.b0, R15.b3
    UPDATE_NEXT_LOCAL_STATE READ_SCL_PIN_DONE

READ_SCL_PIN_VALUE_REPEAT:
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  read scl pins for 10 cycles is done.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
READ_SCL_PIN_DONE:
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    QBEQ    SCL_PIN_READ_VALUE_LOW, R15.b0, R15.b2
    LDI     TEMP_REG4.w0, SCL_VALUE_HIGH
    JMP     READ_SCL_PIN_DONE_RETURN

SCL_PIN_READ_VALUE_LOW:
    LDI     TEMP_REG4.w0, SCL_VALUE_LOW

READ_SCL_PIN_DONE_RETURN:
    LDI    R15.w0, 0x0000
    LDI    R15.w2, 0x0000
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  setup for reseting the slave
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RESET_SLAVE_SCL_BEGIN:
    LDI    R15.w0, 0x0000
    LDI    R15.w2, 0x0900
    UPDATE_NEXT_LOCAL_STATE RESET_SLAVE_SCL_HIGH
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL high
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RESET_SLAVE_SCL_HIGH:
    SET_SCL_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE RESET_SLAVE_SCL_WAIT1
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  wait to match the timing parameters
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RESET_SLAVE_SCL_WAIT1:
    UPDATE_NEXT_LOCAL_STATE RESET_SLAVE_SCL_LOW
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL low 
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RESET_SLAVE_SCL_LOW:
    SET_SCL_PIN_LOW
    ADD     R15.b0, R15.b0, 0x01
    UPDATE_NEXT_LOCAL_STATE RESET_SLAVE_SCL_WAIT2
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  wait to match the timing parameters
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RESET_SLAVE_SCL_WAIT2:
    QBGT    RESET_SLAVE_SCL_RETURN, R15.b0, R15.b3
    UPDATE_NEXT_LOCAL_STATE RESET_SLAVE_RETURN
    STATE_TASK_OVER

RESET_SLAVE_SCL_RETURN:
    UPDATE_NEXT_LOCAL_STATE RESET_SLAVE_SCL_HIGH
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  Finish reseting slave
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RESET_SLAVE_RETURN:
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    LDI     TEMP_REG4.w0, RESET_SLAVE_DONE
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  Read the number of 8 bits data need to be copied over
;  also initialize the data count and bit count register to 0
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
LOOPBACK_DATA_COUNT:
    LBBO    &TEMP_REG4.w0, R10, ICSS_I2C_CNT_OFFSET, 2
    QBLT    LOOPBACK_DATA_COUNT_ERROR, TEMP_REG4.w0, 0xFF
    QBGT    LOOPBACK_DATA_COUNT_ERROR, TEMP_REG4.w0, 0x01
    AND     R15.b3, TEMP_REG4.b0, 0xFF
    AND     R15.b2, R15.b2, 0x00
    UPDATE_NEXT_LOCAL_STATE LOOPBACK_COPY_DATA
    STATE_TASK_OVER

LOOPBACK_DATA_COUNT_ERROR:
    LDI     TEMP_REG4.w0, INVALID_DATA_COUNT
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  copy data from Tx to Rx buffer.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
LOOPBACK_COPY_DATA:
    LBBO    &R15.b1, R11, R15.b2, 1
    SBBO    &R15.b1, R12, R15.b2, 1
    ADD     R15.b2, R15.b2, 1
    QBGE    LOOPBACK_COPY_DATA_RETURN, R15.b3, R15.b2
    STATE_TASK_OVER

LOOPBACK_COPY_DATA_RETURN:
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    LDI     TEMP_REG4.w0, COMMAND_SUCCESS
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    STATE_TASK_OVER


    halt ; end of program
