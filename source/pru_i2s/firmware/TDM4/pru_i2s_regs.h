;
; Copyright (C) 2021 Texas Instruments Incorporated
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

; define registers
    .if $isdefed("I2S_PROFILE_FW")
prux_cycle_cnt_0   .set r0
prux_cycle_cnt_1   .set r1
scratchreg_cycle_cnt  .set    r2
prux_cycle_cnt   .set r3
cnt_buffer_address .set r4
cnt_buffer_address_1 .set r5
    .endif

; Pin mask registers
    .if !$isdefed("I2S_RX")
i2s_instance_fs_pin_pos     .set    r11
    .else
i2s_instance_fs_pin_pos     .set    r7
i2s_instance1_sd_rx_pin_pos .set    r8
i2s_instance2_sd_rx_pin_pos .set    r9
    .endif

    .if !$isdefed("I2S_RX")
;In case of Tx only, use some of the Rx registers.
; tx_buffer_address keeps track of where we are in the audio buffer
tx_buffer_address       .set    r0
tx_buffer_address_end   .set    r1
tx_ping_buffer_address       .set    r2
tx_pong_buffer_address       .set    r3
; ch0_data_tx is 16/32 bit audio sample
ch0_data_tx         .set    r4
ch1_data_tx         .set    r5
ch2_data_tx         .set    r6
r30_value           .set    r7
r30_value_1         .set    r8
r30_value_2         .set    r9
    .endif

tx_buf_size         .set    r10

    .if $isdefed("I2S_RX")
rx_ping_pong_sel_add   .set r11
rx_ping_pong_stat_add   .set r12
rx_buffer_address   .set    r13
rx_buffer_address_end .set  r14
rx_ping_buffer_address       .set    r15
rx_pong_buffer_address       .set    r16
rx_buf_size         .set    r17
;Read receive data
ch0_rx_val_reg      .set    r18
ch1_rx_val_reg      .set    r19
;Accumulate receive data
ch0_data_rx         .set    r20
ch1_data_rx         .set    r21
    .endif

    ; fs_counter counts 16/32 clocks per frame sync
    .asg r22.b0,    fs_counter
    ; fs_counter_1 used to compare FS after transition
    .asg r22.b1,    fs_counter_1
    ; fs_num is frame sync number
    ; 00_b (0) is L channel, 10_b (2) is R channel
    .asg r22.b2,    fs_num
    .asg r28,       fs_level

scratchreg0         .set    r23
scratchreg1         .set    r24
scratchreg2         .set    r25

    .asg    r26.b0,    tx_sd_counter
    .asg    r26.b1,    rx_sd_counter
    ; this FW uses 2 buffers, tx_buffer_num says which buffer is current
    .asg    r26.b2,    tx_buffer_num
    .asg    r26.b3,    rx_buffer_num

    .asg    r27.b0,    tx_ping_pong_stat
    .asg    r27.b1,    rx_ping_pong_stat
    .asg    r27.b2,    err_stat
    .if $isdefed("I2S_RX")
    .asg    r27.b3,    do_rx_overflow_error_check
    .endif
    .if $isdefed("I2S_TX")
    .asg    r27.b3,    do_tx_underflow_error_check
    .endif
    .asg r29.b0 ,  current_channel_no