;
; Copyright (C) 2025 Texas Instruments Incorporated
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions
; are met:
;
;   Redistributions of source code must retain the above copyright
;   notice, this list of conditions and the following disclaimer.
;
;   Redistributions in binary form must reproduce the above copyright
;   notice, this list of conditions and the following disclaimer in the
;   documentation and/or other materials provided with the
;   distribution.
;
;   Neither the name of Texas Instruments Incorporated nor the names of
;   its contributors may be used to endorse or promote products derived
;   from this software without specific prior written permission.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
; "AS IS" AND ANY EXPgResS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
; LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
; A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
; OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
; SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
; LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
; DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
; THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;

    .if $isdefed("SOC_AM263X")
    .if $isdefed("PRU0")
I2S_INSTANCE_BCLK_PIN           .set    6   ; PRG0_PRU0_GPOx
I2S_INSTANCE_BCLK_PIN_POS       .set    64  ; 2 << I2S_INSTANCE_BCLK_PIN

I2S_INSTANCE_FS_PIN             .set    1
I2S_INSTANCE_FS_PIN_POS         .set    2   ; 2 << I2S_INSTANCE_FS_PIN
    .else
I2S_INSTANCE_BCLK_PIN           .set    0
I2S_INSTANCE_BCLK_PIN_POS       .set    1

I2S_INSTANCE_FS_PIN             .set    1
I2S_INSTANCE_FS_PIN_POS         .set    2
    .endif

    .if $isdefed("I2S_TX")
    .if $isdefed("PRU0")
I2S_INSTANCE1_SD_TX_PIN         .set    2
I2S_INSTANCE1_SD_TX_PIN_SHIFT   .set    I2S_INSTANCE1_SD_TX_PIN
I2S_INSTANCE1_SD_TX_PIN_POS     .set    4   ; 2 << I2S_INSTANCE1_SD_Tx_PIN

I2S_INSTANCE2_SD_TX_PIN         .set    3
I2S_INSTANCE2_SD_TX_PIN_SHIFT   .set    I2S_INSTANCE2_SD_TX_PIN
I2S_INSTANCE2_SD_TX_PIN_POS     .set    8   ; 2 << I2S_INSTANCE2_SD_Tx_PIN

I2S_INSTANCE3_SD_TX_PIN         .set    5
I2S_INSTANCE3_SD_TX_PIN_SHIFT   .set    I2S_INSTANCE3_SD_TX_PIN
I2S_INSTANCE3_SD_TX_PIN_POS     .set    32  ; 2 << I2S_INSTANCE3_SD_Tx_PIN
    .else
I2S_INSTANCE1_SD_TX_PIN         .set    4
I2S_INSTANCE1_SD_TX_PIN_SHIFT   .set    I2S_INSTANCE1_SD_TX_PIN
I2S_INSTANCE1_SD_TX_PIN_POS     .set    16  ; 2 << I2S_INSTANCE1_SD_Tx_PIN

I2S_INSTANCE2_SD_TX_PIN         .set    5
I2S_INSTANCE2_SD_TX_PIN_SHIFT   .set    I2S_INSTANCE2_SD_TX_PIN
I2S_INSTANCE2_SD_TX_PIN_POS     .set    32  ; 2 << I2S_INSTANCE2_SD_Tx_PIN

I2S_INSTANCE3_SD_TX_PIN         .set    6
I2S_INSTANCE3_SD_TX_PIN_SHIFT   .set    I2S_INSTANCE3_SD_TX_PIN
I2S_INSTANCE3_SD_TX_PIN_POS     .set    64  ; 2 << I2S_INSTANCE3_SD_Tx_PIN
    .endif
    .endif

    .if $isdefed("I2S_RX")
    .if $isdefed("PRU0")
I2S_INSTANCE1_SD_RX_PIN         .set    0
I2S_INSTANCE1_SD_RX_PIN_SHIFT   .set    I2S_INSTANCE1_SD_RX_PIN
I2S_INSTANCE1_SD_RX_PIN_POS     .set    1   ; 2 << I2S_INSTANCE1_SD_Rx_PIN

I2S_INSTANCE2_SD_RX_PIN         .set    4
I2S_INSTANCE2_SD_RX_PIN_SHIFT   .set    I2S_INSTANCE2_SD_RX_PIN
I2S_INSTANCE2_SD_RX_PIN_POS     .set    16  ; 2 << I2S_INSTANCE2_SD_Rx_PIN
    .else
I2S_INSTANCE1_SD_RX_PIN         .set    2
I2S_INSTANCE1_SD_RX_PIN_SHIFT   .set    I2S_INSTANCE1_SD_RX_PIN
I2S_INSTANCE1_SD_RX_PIN_POS     .set    4   ; 2 << I2S_INSTANCE1_SD_Rx_PIN

I2S_INSTANCE2_SD_RX_PIN         .set    3
I2S_INSTANCE2_SD_RX_PIN_SHIFT   .set    I2S_INSTANCE2_SD_RX_PIN
I2S_INSTANCE2_SD_RX_PIN_POS     .set    8   ; 2 << I2S_INSTANCE2_SD_Rx_PIN
    .endif
    .endif
    .endif  ;SOC_AM263X
    .if $isdefed("SOC_AM261X")
    .if $isdefed("PRU0")
; PRU0 Configuration
I2S_INSTANCE_BCLK_PIN           .set    5   ; PRU0 BCLK pin
I2S_INSTANCE_BCLK_PIN_POS       .set    32  ; 2 << 5

I2S_INSTANCE_FS_PIN             .set    6   ; PRU0 FSYNC pin
I2S_INSTANCE_FS_PIN_POS         .set    64  ; 2 << 6

    .if $isdefed("I2S_TX")
; PRU0 TX pins
I2S_INSTANCE1_SD_TX_PIN         .set    7   ; TX1
I2S_INSTANCE1_SD_TX_PIN_SHIFT   .set    I2S_INSTANCE1_SD_TX_PIN
I2S_INSTANCE1_SD_TX_PIN_POS     .set    128 ; 2 << 7

I2S_INSTANCE2_SD_TX_PIN         .set    8   ; TX2
I2S_INSTANCE2_SD_TX_PIN_SHIFT   .set    I2S_INSTANCE2_SD_TX_PIN
I2S_INSTANCE2_SD_TX_PIN_POS     .set    256 ; 2 << 8

I2S_INSTANCE3_SD_TX_PIN         .set    9   ; TX3
I2S_INSTANCE3_SD_TX_PIN_SHIFT   .set    I2S_INSTANCE3_SD_TX_PIN
I2S_INSTANCE3_SD_TX_PIN_POS     .set    512 ; 2 << 9
    .endif

    .else
; PRU1 Configuration
I2S_INSTANCE_BCLK_PIN           .set    5   ; PRU1 BCLK pin
I2S_INSTANCE_BCLK_PIN_POS       .set    32  ; 2 << 5

I2S_INSTANCE_FS_PIN             .set    9   ; PRU1 FSYNC pin
I2S_INSTANCE_FS_PIN_POS         .set    512 ; 2 << 9

    .if $isdefed("I2S_RX")
; PRU1 RX pins
I2S_INSTANCE1_SD_RX_PIN         .set    12  ; RX1
I2S_INSTANCE1_SD_RX_PIN_SHIFT   .set    I2S_INSTANCE1_SD_RX_PIN
I2S_INSTANCE1_SD_RX_PIN_POS     .set    4096 ; 2 << 12

I2S_INSTANCE2_SD_RX_PIN         .set    13  ; RX2
I2S_INSTANCE2_SD_RX_PIN_SHIFT   .set    I2S_INSTANCE2_SD_RX_PIN
I2S_INSTANCE2_SD_RX_PIN_POS     .set    8192 ; 2 << 13
    .endif
    .endif

    .endif ;SOC_AM261X
I2S_RXOVERFLOW_ERROR_POSITION   .set    0
I2S_TXUNDERFLOW_ERROR_POSITION  .set    1
I2S_FRAMESYNC_ERROR_POSITION    .set    2

;Address in FW Register space where TX PING address is stored
I2S_TX_BUF_PING_ADD       .set    4

;Address in FW Register space where buffer size is stored. Buffer size is same for Tx and Rx. Buffer size is PING+PONG.
I2S_PING_PONG_BUFSIZE_ADD       .set    8

;Address in FW Register space where RX PING address is stored
I2S_RX_BUF_PING_ADD       .set    0x14

I2S_TX_BUF_PING_PONG_STAT_ADD   .set    0x18

I2S_RX_BUF_PING_PONG_STAT_ADD   .set    0x19

I2S_TX_INSTANCE_PING_PONG_STAT_ADD  .set    0x1A

I2S_RX_INSTANCE_PING_PONG_STAT_ADD  .set    0x1B

I2S_ERR_STAT_ADD  .set    0x1C

;Load 4 bytes from memory to register.
BYTES_TO_LOAD   .set 2   ; 4 bytes to load from memory for 4 bytes per channel configuration

    .if $isdefed("PRU0")
PRUx_CYCLE_CNT_REG_ADD          .set    0x2200C; PRU0 cycle counter register address offset
    .else
PRUx_CYCLE_CNT_REG_ADD          .set    0x2400C; PRU1 cycle counter register address offset
    .endif
;Number of samples per slot/channel
I2S_SAMPLES_PER_CHANNEL         .set    16
I2S_SAMPLES_PER_CHANNEL_LESS_1  .set    15 ;I2S_SAMPLES_PER_CHANNEL-1
TDM_CHANNELS                    .set    6 ; TDM channel slots used in TDM mode
MAX_TDM_CHANNELS                .set    6 ; Maximum TDM Slots (TDM4/TDM8)
    .if $isdefed("PRU0")
; Compile-time Host event for I2S Tx, pr0_pru_mst_intr[2]_intr_req
TRIGGER_HOST_I2S_TX_EVT         .set 18 ; 2+16
; R31 event interface mapping, add pru<n>_r31_vec_valid to system event number
TRIGGER_HOST_I2S_TX_IRQ         .set TRIGGER_HOST_I2S_TX_EVT + 16 ; 2+16+16 = 2+32 = 2 + 1<<5

; Compile-time Host event for I2S Rx, pr0_pru_mst_intr[3]_intr_req
TRIGGER_HOST_I2S_RX_EVT         .set 19 ; 3+16
; R31 event interface mapping, add pru<n>_r31_vec_valid to system event number
TRIGGER_HOST_I2S_RX_IRQ         .set TRIGGER_HOST_I2S_RX_EVT + 16 ; 3+16+16 = 3+32 = 3 + 1<<5

; Compile-time Host event for I2S Error, pr0_pru_mst_intr[4]_intr_req
TRIGGER_HOST_I2S_ERR_EVT        .set 20 ; 4+16
; R31 event interface mapping, add pru<n>_r31_vec_valid to system event number
TRIGGER_HOST_I2S_ERR_IRQ        .set TRIGGER_HOST_I2S_ERR_EVT + 16 ; 4+16+16 = 4+32 = 4 + 1<<5

    .else
; Compile-time Host event for I2S Tx, pr0_pru_mst_intr[2]_intr_req
TRIGGER_HOST_I2S_TX_EVT         .set 21 ; 5+16
; R31 event interface mapping, add pru<n>_r31_vec_valid to system event number
TRIGGER_HOST_I2S_TX_IRQ         .set TRIGGER_HOST_I2S_TX_EVT + 16 ; 2+16+16 = 2+32 = 2 + 1<<5

; Compile-time Host event for I2S Rx, pr0_pru_mst_intr[3]_intr_req
TRIGGER_HOST_I2S_RX_EVT         .set 22 ; 6+16
; R31 event interface mapping, add pru<n>_r31_vec_valid to system event number
TRIGGER_HOST_I2S_RX_IRQ         .set TRIGGER_HOST_I2S_RX_EVT + 16 ; 3+16+16 = 3+32 = 3 + 1<<5

; Compile-time Host event for I2S Error, pr0_pru_mst_intr[4]_intr_req
TRIGGER_HOST_I2S_ERR_EVT        .set 23 ; 7+16
; R31 event interface mapping, add pru<n>_r31_vec_valid to system event number
TRIGGER_HOST_I2S_ERR_IRQ        .set TRIGGER_HOST_I2S_ERR_EVT + 16 ; 4+16+16 = 4+32 = 4 + 1<<5
    
    .endif
