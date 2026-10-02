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
 .include "icss_constant_defines.inc"
 .include "icss_cfg_regs.inc"
 .include "icss_iep_regs.inc"
 .include "icss_intc_regs.inc"
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

; MSS_CTRL ICSSMx_PRUy_GPIO_OUT_CTRL (AM261x, cslr_mss_ctrl.h). Setting a bit
; disables the output driver of that PRU GPO pin, which releases the line to
; the external pull-up. Used to hand SDA to the target for ACK and read data.
ICSSM0_PRU0_GPIO_OUT_CTRL           .set    0x50D00810
ICSSM0_PRU1_GPIO_OUT_CTRL           .set    0x50D00814
ICSSM1_PRU0_GPIO_OUT_CTRL           .set    0x50D00818
ICSSM1_PRU1_GPIO_OUT_CTRL           .set    0x50D0081C

; Per-core resources. The IEP and the scratchpad banks are shared by both
; PRUs, so each core must use its own compare register, status bit, enable
; bit and context bank. The build must define exactly one of PRU0 / PRU1
; (projectspec -DPRU0/-DPRU1, makefile DFLAGS); icss_constant_defines.inc
; also relies on it to select the DMEM constants.
    .if $defined("PRU0")
I2C_IEP_CMP_REG                     .set    PRU0_IEP_CMP_REG
I2C_IEP_CMP_ENABLE_BIT              .set    PRU0_IEP_CMP_ENABLE_BIT
I2C_IEP_CMP_STATUS_BIT              .set    PRU0_IEP_CMP_STATUS_BIT
I2C_CONTEXT_BANK                    .set    BANK0
    .if $defined("ICSSM1")
I2C_GPIO_OUT_CTRL                   .set    ICSSM1_PRU0_GPIO_OUT_CTRL
    .else
I2C_GPIO_OUT_CTRL                   .set    ICSSM0_PRU0_GPIO_OUT_CTRL
    .endif
    .elseif $defined("PRU1")
I2C_IEP_CMP_REG                     .set    PRU1_IEP_CMP_REG
I2C_IEP_CMP_ENABLE_BIT              .set    PRU1_IEP_CMP_ENABLE_BIT
I2C_IEP_CMP_STATUS_BIT              .set    PRU1_IEP_CMP_STATUS_BIT
I2C_CONTEXT_BANK                    .set    BANK1
    .if $defined("ICSSM1")
I2C_GPIO_OUT_CTRL                   .set    ICSSM1_PRU1_GPIO_OUT_CTRL
    .else
I2C_GPIO_OUT_CTRL                   .set    ICSSM0_PRU1_GPIO_OUT_CTRL
    .endif
    .else
    .emsg "pru_i2c: define PRU0 or PRU1 for the core this firmware runs on"
    .endif

; Register usage:
;   R1-R8    TEMP_REG1..8, scratch. TEMP_REG3.w0 holds the state return
;            address while a state runs (JAL in I2C_WAVE_FUNCTION0).
;   R10-R19  I2C instance context, saved/restored with XIN/XOUT on
;            I2C_CONTEXT_BANK:
;            R10 instance base, R11 Tx buffer, R12 Rx buffer,
;            R13.w0 next state, R13.w2 slave address shift register,
;            R14 b0 SCL pin, b1 SDA pin, b3 instance id,
;            R15 b0 bit count, b1 data byte, b2 byte index, b3 byte count,
;            R16 control flags (ICSS_I2C_*_BIT), R17.w0 post-address state,
;            R17.b2 SMBus read phase length.
;            R18 w0 SCL-low tick counter, w2 SCL-low timeout (ticks, 0 = off).
;            R19 b0 PEC CRC, b1 SMBus command code, b3 transfer flags SMB_F_*.
;   R20-R21  next IEP compare value (64-bit), R22 frequency word.
;   R23      shadow of I2C_GPIO_OUT_CTRL (open-drain SCL/SDA), R26 its address.

; Transfer flags in R19.b3 (cleared at the start of every transfer)
SMB_F_PEC                           .set    0   ; update the PEC CRC
SMB_F_PECTX                         .set    1   ; last write-phase byte is the PEC
SMB_F_RDPHASE                       .set    2   ; repeated START + read after writing
SMB_F_BLKRD                         .set    3   ; next byte read is the block count
SMB_F_SMB                           .set    4   ; SMBus: always START/STOP, NACK last byte
SMB_F_PECRX                         .set    5   ; the read ends with the target's PEC
SMB_F_ERR                           .set    6   ; error already reported, end with STOP


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
    XOUT   I2C_CONTEXT_BANK, &R10, 40
    
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
    
;   Unsupported frequency selector. Do not stop the IEP counter here: it is
;   shared with the other PRU, whose I2C instance may already be running.
ERROR_LOOP:
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
    ; Open-drain bus: R30 keeps the SCL and SDA bits at 0 and the lines are
    ; switched with their OUTDISABLE bits (see SET_SCL_PIN_HIGH). Start with
    ; both lines released, which is also the idle state of the bus.
    LDI32   R26, I2C_GPIO_OUT_CTRL
    LBBO    &R23, R26, 0, 4
    CLR     R30, R30, R14.b0
    CLR     R30, R30, R14.b1
    SET     R23, R23, R14.b0
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
    UPDATE_NEXT_LOCAL_STATE SETUP_I2C_SMBUS_CTRL
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  SMBus PEC enable and the SCL-low (clock stretching) timeout
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SETUP_I2C_SMBUS_CTRL:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_CON_OFFSET, 4
    CLR     R16, R16, ICSS_I2C_PEC_BIT
    QBBC    SETUP_I2C_NO_PEC, TEMP_REG4, ICSS_I2C_PEC_BIT
    SET     R16, R16, ICSS_I2C_PEC_BIT
SETUP_I2C_NO_PEC:
    LBBO    &R18.w2, R10, ICSS_I2C_SCL_TIMEOUT_OFFSET, 2
    LDI     R18.w0, 0
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
    SET_SDA_PIN_HIGH
    SET_SCL_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_ERROR
    STATE_TASK_OVER

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
    UPDATE_NEXT_LOCAL_STATE ICSS_SMBUS_CMD_CHECK
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  SMBus commands (ICSS_SMBUS_QUICK_CMD .. ICSS_SMBUS_BLOCK_READ_CMD) start an
;  SMBus transfer. Every other command is passed on to the next check.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ICSS_SMBUS_CMD_CHECK:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    QBLT    ICSS_SMBUS_CMD_CHECK_NEXT, TEMP_REG4.w2, ICSS_SMBUS_BLOCK_READ_CMD
    QBGT    ICSS_SMBUS_CMD_CHECK_NEXT, TEMP_REG4.w2, ICSS_SMBUS_QUICK_CMD
    UPDATE_NEXT_LOCAL_STATE SMBUS_MODE
    STATE_TASK_OVER

ICSS_SMBUS_CMD_CHECK_NEXT:
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
;  I2C write: send the Tx buffer to the target
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
TX_MODE:
    CLR     R16, R16, ICSS_I2C_READ_WRITE_BIT
    I2C_TRANSFER_INIT
    UPDATE_NEXT_GLOBAL_STATE TX_DATA_SDA_BEGIN
    UPDATE_NEXT_LOCAL_STATE SET_SCL_SDA_HIGH
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  I2C read: read the data count from the target into the Rx buffer
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RX_MODE:
    SET     R16, R16, ICSS_I2C_READ_WRITE_BIT
    I2C_TRANSFER_INIT
    UPDATE_NEXT_GLOBAL_STATE RX_DATA_SDA_BEGIN
    UPDATE_NEXT_LOCAL_STATE SET_SCL_SDA_HIGH
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  SMBus command: decode the protocol and describe the transfer.
;
;  A transfer is a write phase (the "stream": optional command code and
;  block count prefix bytes, the Tx buffer, optional PEC) followed, for the
;  read protocols, by a repeated START and a read phase into the Rx buffer.
;  The prefix bytes are staged at the two bytes just below the Tx buffer
;  (instance offsets 0xFE/0xFF), so the stream is contiguous from R11.
;
;      protocol      write phase                  read phase
;      quick         (address only, R/W = command code bit 0)
;      send byte     cmd
;      receive byte                               1 byte
;      write byte    cmd, Tx[0]
;      write word    cmd, Tx[0] (low), Tx[1] (high)
;      block write   cmd, N = count, Tx[0..N-1]
;      read byte     cmd                          Sr, 1 byte
;      read word     cmd                          Sr, 2 bytes (low first)
;      block read    cmd                          Sr, N, N bytes (N -> count)
;
;  With PEC enabled (CON ICSS_I2C_PEC_BIT) a write-only transfer ends with
;  the PEC byte and a read phase reads one more byte, the target's PEC,
;  which is checked. Quick command has no PEC. 10-bit addressing is not
;  supported for SMBus.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SMBUS_MODE:
    LBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 4
    QBBS    SMBUS_MODE_10BIT, R16, ICSS_I2C_ADDRESSING_MODE_BIT
    I2C_TRANSFER_INIT
    SET     R19.b3, R19.b3, SMB_F_SMB
    LBBO    &R19.b1, R10, ICSS_I2C_PRU_CMD_CODE_OFFSET, 1
    CLR     R16, R16, ICSS_I2C_READ_WRITE_BIT
    UPDATE_NEXT_GLOBAL_STATE TX_DATA_SDA_BEGIN
    QBEQ    SMBUS_MODE_QUICK, TEMP_REG4.w2, ICSS_SMBUS_QUICK_CMD
    QBBC    SMBUS_MODE_NO_PEC, R16, ICSS_I2C_PEC_BIT
    SET     R19.b3, R19.b3, SMB_F_PEC
SMBUS_MODE_NO_PEC:
    QBEQ    SMBUS_MODE_RECEIVE_BYTE, TEMP_REG4.w2, ICSS_SMBUS_RECEIVE_BYTE_CMD
    ; every other protocol starts with the command code
    LDI     TEMP_REG5, ICSS_I2C_INSTANCE0_TX_MEM - 1
    SBBO    &R19.b1, TEMP_REG5, 0, 1
    LDI     R11.w0, ICSS_I2C_INSTANCE0_TX_MEM - 1
    LDI     R15.b3, 1
    QBEQ    SMBUS_MODE_WRITE, TEMP_REG4.w2, ICSS_SMBUS_SEND_BYTE_CMD
    LDI     R15.b3, 2
    QBEQ    SMBUS_MODE_WRITE, TEMP_REG4.w2, ICSS_SMBUS_WRITE_BYTE_CMD
    LDI     R15.b3, 3
    QBEQ    SMBUS_MODE_WRITE, TEMP_REG4.w2, ICSS_SMBUS_WRITE_WORD_CMD
    QBEQ    SMBUS_MODE_BLOCK_WRITE, TEMP_REG4.w2, ICSS_SMBUS_BLOCK_WRITE_CMD
    ; read byte / read word / block read: write the command code, then read
    LDI     R15.b3, 1
    SET     R19.b3, R19.b3, SMB_F_RDPHASE
    LDI     R17.b2, 1
    QBEQ    SMBUS_MODE_READ, TEMP_REG4.w2, ICSS_SMBUS_READ_BYTE_CMD
    LDI     R17.b2, 2
    QBEQ    SMBUS_MODE_READ, TEMP_REG4.w2, ICSS_SMBUS_READ_WORD_CMD
    SET     R19.b3, R19.b3, SMB_F_BLKRD
SMBUS_MODE_READ:
    QBBC    SMBUS_MODE_START, R19.b3, SMB_F_PEC
    SET     R19.b3, R19.b3, SMB_F_PECRX
    JMP     SMBUS_MODE_START

SMBUS_MODE_BLOCK_WRITE:
    ; N = data count, 1..253 (252 with PEC) so that cmd, N, data and PEC fit
    ; the 8-bit stream index
    LBBO    &TEMP_REG4.w0, R10, ICSS_I2C_CNT_OFFSET, 2
    QBEQ    SMBUS_MODE_COUNT_ERROR, TEMP_REG4.w0, 0
    LDI     TEMP_REG6.w0, 253
    QBBC    SMBUS_MODE_BLOCK_MAX, R19.b3, SMB_F_PEC
    LDI     TEMP_REG6.w0, 252
SMBUS_MODE_BLOCK_MAX:
    QBLT    SMBUS_MODE_COUNT_ERROR, TEMP_REG4.w0, TEMP_REG6.w0
    LDI     TEMP_REG5, ICSS_I2C_INSTANCE0_TX_MEM - 2
    SBBO    &R19.b1, TEMP_REG5, 0, 1
    SBBO    &TEMP_REG4.b0, TEMP_REG5, 1, 1
    LDI     R11.w0, ICSS_I2C_INSTANCE0_TX_MEM - 2
    ADD     R15.b3, TEMP_REG4.b0, 2
SMBUS_MODE_WRITE:
    QBBC    SMBUS_MODE_START, R19.b3, SMB_F_PEC
    SET     R19.b3, R19.b3, SMB_F_PECTX
    ADD     R15.b3, R15.b3, 1
    JMP     SMBUS_MODE_START

SMBUS_MODE_RECEIVE_BYTE:
    SET     R16, R16, ICSS_I2C_READ_WRITE_BIT
    UPDATE_NEXT_GLOBAL_STATE RX_DATA_SDA_BEGIN
    LDI     R15.b3, 1
    QBBC    SMBUS_MODE_START, R19.b3, SMB_F_PEC
    SET     R19.b3, R19.b3, SMB_F_PECRX
    LDI     R15.b3, 2
    JMP     SMBUS_MODE_START

SMBUS_MODE_QUICK:
    ; address only; the R/W bit is bit 0 of the command code
    QBBC    SMBUS_MODE_QUICK_W, R19.b1, 0
    SET     R16, R16, ICSS_I2C_READ_WRITE_BIT
SMBUS_MODE_QUICK_W:
    LDI     R15.b3, 0
    UPDATE_NEXT_GLOBAL_STATE DATA_PROCESSING_COMPLETE

SMBUS_MODE_START:
    UPDATE_NEXT_LOCAL_STATE SET_SCL_SDA_HIGH
    STATE_TASK_OVER

SMBUS_MODE_COUNT_ERROR:
    LDI     TEMP_REG4.w0, INVALID_DATA_COUNT
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    STATE_TASK_OVER

SMBUS_MODE_10BIT:
    LDI     TEMP_REG4.w0, ADDRESSING_MODE_FAILED
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  release both lines before the START condition
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SET_SCL_SDA_HIGH:
    SET_SDA_PIN_HIGH
    SET_SCL_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE SLAVE_ADDRESS_SETUP
    STATE_TASK_OVER

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
;  I2C: read the number of bytes to read or write and load the first byte.
;  SMBus: the length was set by SMBUS_MODE, only load the first byte.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
DATA_COUNT:
    QBBS    DATA_COUNT_SMBUS, R19.b3, SMB_F_SMB
    LBBO    &TEMP_REG4.w0, R10, ICSS_I2C_CNT_OFFSET, 2

    ;debug code
    .if $defined("DEBUG_CODE")
    ; fif count
    LDI  TEMP_REG4.w0, 1
    .endif

    QBLT    DATA_COUNT_ERROR, TEMP_REG4.w0, 0xFF
    QBGT    DATA_COUNT_ERROR, TEMP_REG4.w0, 0x01

    AND     R15.b3, TEMP_REG4.b0, 0xFF
    AND     R15.b2, R15.b2, 0x00
    LBBO    &R15.b1, R11, R15.b2, 1

    UPDATE_NEXT_LOCAL_STATE START_CONDITION_SDA_LOW
    STATE_TASK_OVER

DATA_COUNT_SMBUS:
    QBBS    DATA_COUNT_SMBUS_DONE, R16, ICSS_I2C_READ_WRITE_BIT
    QBEQ    DATA_COUNT_SMBUS_DONE, R15.b3, 0
    TX_FETCH_BYTE R15.b1
DATA_COUNT_SMBUS_DONE:
    UPDATE_NEXT_LOCAL_STATE START_CONDITION_SDA_LOW
    STATE_TASK_OVER

DATA_COUNT_ERROR:
    LDI     TEMP_REG4.w0, INVALID_DATA_COUNT
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  START condition: SDA low while SCL is high. SMBus always sends START.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
START_CONDITION_SDA_LOW:
    WAIT_SCL_HIGH
    QBBS    START_CONDITION_SDA_LOW_DO, R19.b3, SMB_F_SMB
    QBBC    START_CONDITION_SDA_LOW_RETURN, R16, ICSS_I2C_START_BIT
START_CONDITION_SDA_LOW_DO:
    SET_SDA_PIN_LOW
START_CONDITION_SDA_LOW_RETURN:
    UPDATE_NEXT_LOCAL_STATE START_CONDITION_HOLD
    STATE_TASK_OVER

;  START hold time (tHD;STA): one more tick with SDA low and SCL high, so it
;  is two ticks long (5 us at 100 kHz, Standard-mode/SMBus needs 4.0 us)
START_CONDITION_HOLD:
    UPDATE_NEXT_LOCAL_STATE START_CONDITION_SCL_LOW
    STATE_TASK_OVER

START_CONDITION_SCL_LOW:
    QBBS    START_CONDITION_SCL_LOW_DO, R19.b3, SMB_F_SMB
    QBBC    START_CONDITION_SCL_LOW_RETURN, R16, ICSS_I2C_START_BIT
START_CONDITION_SCL_LOW_DO:
    SET_SCL_PIN_LOW
START_CONDITION_SCL_LOW_RETURN:
    UPDATE_NEXT_LOCAL_STATE ADDRESS_SDA_BEGIN
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  address byte(s): put the next address bit on SDA (SCL is low)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ADDRESS_SDA_BEGIN:
    RSB     TEMP_REG5.b0, R15.b0, 15
    QBBC    ADDRESS_SDA_LOW, R13.w2, TEMP_REG5.b0
    SET_SDA_PIN_HIGH
    JMP     ADDRESS_SDA_CONTINUE
ADDRESS_SDA_LOW:
    SET_SDA_PIN_LOW
ADDRESS_SDA_CONTINUE:
    PEC_UPDATE R13.w2, TEMP_REG5.b0
    ADD     R15.b0, R15.b0, 0x01
    UPDATE_NEXT_LOCAL_STATE ADDRESS_SCL_BEGIN
    STATE_TASK_OVER

ADDRESS_SCL_BEGIN:
    SET_SCL_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE ADDRESS_SDA_READ
    STATE_TASK_OVER

ADDRESS_SDA_READ:
    WAIT_SCL_HIGH
    UPDATE_NEXT_LOCAL_STATE ADDRESS_SCL_END
    STATE_TASK_OVER

;  pull SCL low; after the 8th bit release SDA on this same edge, so the
;  target can drive its ACK (SCL is already low, so this is not a STOP)
ADDRESS_SCL_END:
    SET_SCL_PIN_LOW
    QBGT    SDA_NEXT_BIT, R15.b0, 0x08
    SET_SDA_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE ADDRESS_ACK_BEGIN
    STATE_TASK_OVER

SDA_NEXT_BIT:
    UPDATE_NEXT_LOCAL_STATE ADDRESS_SDA_BEGIN
    STATE_TASK_OVER

;  SDA was released in ADDRESS_SCL_END; this tick keeps the 4-tick bit timing
ADDRESS_ACK_BEGIN:
    UPDATE_NEXT_LOCAL_STATE ADDRESS_ACK_SCL_BEGIN
    STATE_TASK_OVER

ADDRESS_ACK_SCL_BEGIN:
    SET_SCL_PIN_HIGH
    AND     R15.b0, R15.b0, 0x00
    UPDATE_NEXT_LOCAL_STATE ADDRESS_ACK_READ
    STATE_TASK_OVER

ADDRESS_ACK_READ:
    WAIT_SCL_HIGH
    READ_SDA_PIN_ACK
    UPDATE_NEXT_LOCAL_STATE ADDRESS_ACK_SCL_END
    STATE_TASK_OVER

;  pull SCL low; on ACK continue with the second address byte (10-bit) or
;  the data phase in R17.w0, on NACK abandon the transfer
ADDRESS_ACK_SCL_END:
    SET_SCL_PIN_LOW
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

;  no ACK for the address: report it and end the transfer with a STOP
NO_ADDRESS_ACK_RECIEVED:
    LDI     TEMP_REG4.w0, ADDRESS_ACKNOWLDEGE_FAILED
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    SET     R19.b3, R19.b3, SMB_F_ERR
    UPDATE_NEXT_LOCAL_STATE DATA_PROCESSING_COMPLETE
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  write phase: send the stream and check each ACK
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
TX_DATA_SDA_BEGIN:
    SEND_TX_DATA_CHECK_FOR_ACK R15.b1, TX_PHASE_DONE, TX_DATA_NACK

;  the stream is sent: SMBus reads continue with a repeated START
TX_PHASE_DONE:
    QBBS    TX_PHASE_DONE_READ, R19.b3, SMB_F_RDPHASE
    UPDATE_NEXT_LOCAL_STATE DATA_PROCESSING_COMPLETE
    STATE_TASK_OVER
TX_PHASE_DONE_READ:
    UPDATE_NEXT_LOCAL_STATE RSTART_SDA_HIGH
    STATE_TASK_OVER

;  no ACK for a data byte (DATA_ACKNOWLDEGE_FAILED is already reported):
;  end the transfer with a STOP
TX_DATA_NACK:
    SET     R19.b3, R19.b3, SMB_F_ERR
    UPDATE_NEXT_LOCAL_STATE DATA_PROCESSING_COMPLETE
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  repeated START (Sr) for the read phase of an SMBus read: release SDA and
;  SCL, then SDA low while SCL is high, then the address byte with R/W = 1
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RSTART_SDA_HIGH:
    SET_SDA_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE RSTART_SCL_HIGH
    STATE_TASK_OVER

RSTART_SCL_HIGH:
    SET_SCL_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE RSTART_SETUP
    STATE_TASK_OVER

;  repeated START setup time (tSU;STA): SCL high for two ticks before SDA falls
RSTART_SETUP:
    WAIT_SCL_HIGH
    UPDATE_NEXT_LOCAL_STATE RSTART_SDA_LOW
    STATE_TASK_OVER

RSTART_SDA_LOW:
    SET_SDA_PIN_LOW
    CLR     R19.b3, R19.b3, SMB_F_RDPHASE
    SET     R16, R16, ICSS_I2C_READ_WRITE_BIT
    SET     R13.w2, R13.w2, 8
    LDI     R15.w0, 0x0000
    MOV     R15.b3, R17.b2
    QBBC    RSTART_SDA_LOW_NO_PEC, R19.b3, SMB_F_PECRX
    ADD     R15.b3, R15.b3, 1
RSTART_SDA_LOW_NO_PEC:
    LDI     R15.b2, 0x00
    UPDATE_NEXT_GLOBAL_STATE RX_DATA_SDA_BEGIN
    UPDATE_NEXT_LOCAL_STATE RSTART_HOLD
    STATE_TASK_OVER

;  repeated START hold time (tHD;STA)
RSTART_HOLD:
    UPDATE_NEXT_LOCAL_STATE RSTART_SCL_LOW
    STATE_TASK_OVER

RSTART_SCL_LOW:
    SET_SCL_PIN_LOW
    UPDATE_NEXT_LOCAL_STATE ADDRESS_SDA_BEGIN
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  read phase: read bytes into the Rx buffer, ACK all but the last
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RX_DATA_SDA_BEGIN:
    READ_RX_DATA_AND_SEND_ACK R15.b1, DATA_PROCESSING_COMPLETE

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  end of the transfer (SCL is low). Send STOP if CON asks for it, and always
;  for SMBus and after an error.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
DATA_PROCESSING_COMPLETE:
    SET_SDA_PIN_LOW
    QBBS    COMPLETE_WITH_STOP, R19.b3, SMB_F_SMB
    QBBS    COMPLETE_WITH_STOP, R19.b3, SMB_F_ERR
    QBBC    COMPLETE_WITH_NO_STOP, R16, ICSS_I2C_STOP_BIT
COMPLETE_WITH_STOP:
    UPDATE_NEXT_LOCAL_STATE STOP_CONDITION_SCL_HIGH
    STATE_TASK_OVER

COMPLETE_WITH_NO_STOP:
    UPDATE_NEXT_LOCAL_STATE NO_STOP_CONDITION_SDA_HIGH
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  STOP condition: release SCL, then release SDA while SCL is high. The bus
;  is then idle with both lines released. Report the result unless an error
;  was already reported; an SMBus read with PEC must end with a zero CRC.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
STOP_CONDITION_SCL_HIGH:
    SET_SCL_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE STOP_CONDITION_SETUP
    STATE_TASK_OVER

;  STOP setup time (tSU;STO): SCL high for two ticks before SDA rises
STOP_CONDITION_SETUP:
    WAIT_SCL_HIGH
    UPDATE_NEXT_LOCAL_STATE STOP_CONDITION_SDA_HIGH
    STATE_TASK_OVER

STOP_CONDITION_SDA_HIGH:
    SET_SDA_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    QBBS    STOP_CONDITION_REPORTED, R19.b3, SMB_F_ERR
    LDI     TEMP_REG4.w0, COMMAND_SUCCESS
    QBBC    STOP_CONDITION_REPORT, R19.b3, SMB_F_PECRX
    QBEQ    STOP_CONDITION_REPORT, R19.b0, 0
    LDI     TEMP_REG4.w0, PEC_ERROR
STOP_CONDITION_REPORT:
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
STOP_CONDITION_REPORTED:
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  no STOP (I2C with CON STOP clear): release SDA, then SCL
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
NO_STOP_CONDITION_SDA_HIGH:
    SET_SDA_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE NO_STOP_CONDITION_SCL_HIGH
    STATE_TASK_OVER

NO_STOP_CONDITION_SCL_HIGH:
    SET_SCL_PIN_HIGH
    LDI     TEMP_REG4.w0, COMMAND_SUCCESS
    SBBO    &TEMP_REG4, R10, ICSS_I2C_COMMAND_OFFSET, 2
    UPDATE_NEXT_LOCAL_STATE RAISE_HOST_INTERRUPT_MEM_FOR_READY
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  a target held SCL low for longer than the configured timeout: release
;  both lines and report TIME_OUT_ERROR (no STOP is possible while SCL is
;  held low)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SCL_LOW_TIMEOUT:
    SET_SDA_PIN_HIGH
    SET_SCL_PIN_HIGH
    LDI     R18.w0, 0
    LDI     TEMP_REG4.w0, TIME_OUT_ERROR
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
    ; Bus recovery: make sure SDA is released so a target that holds it low
    ; can let go, then clock SCL until it does. SDA stays released afterwards,
    ; as on an idle bus.
    SET_SDA_PIN_HIGH
    UPDATE_NEXT_LOCAL_STATE RESET_SLAVE_SCL_LOW
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL high, counting one clock pulse per rising edge
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RESET_SLAVE_SCL_HIGH:
    SET_SCL_PIN_HIGH
    ADD     R15.b0, R15.b0, 0x01
    UPDATE_NEXT_LOCAL_STATE RESET_SLAVE_SCL_WAIT2
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  wait to match the timing parameters
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RESET_SLAVE_SCL_WAIT1:
    UPDATE_NEXT_LOCAL_STATE RESET_SLAVE_SCL_HIGH
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  make SCL low 
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RESET_SLAVE_SCL_LOW:
    SET_SCL_PIN_LOW
    UPDATE_NEXT_LOCAL_STATE RESET_SLAVE_SCL_WAIT1
    STATE_TASK_OVER

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  wait to match the timing parameters; after 9 pulses SCL is left high
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RESET_SLAVE_SCL_WAIT2:
    QBGT    RESET_SLAVE_SCL_RETURN, R15.b0, R15.b3
    UPDATE_NEXT_LOCAL_STATE RESET_SLAVE_RETURN
    STATE_TASK_OVER

RESET_SLAVE_SCL_RETURN:
    UPDATE_NEXT_LOCAL_STATE RESET_SLAVE_SCL_LOW
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
