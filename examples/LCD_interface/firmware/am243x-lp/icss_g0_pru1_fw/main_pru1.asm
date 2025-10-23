; Copyright (C) 2024-2025 Texas Instruments Incorporated - http://www.ti.com/
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

;***************************************************************************************
;   File:     main_pru1.asm
;
;   Brief:    PRU1 firmware to load from frame buffer and write blue color along with HSYNC signal
;
;   Steps to build :
;
;   - Using ccs:
;             - Import pru project to ccs workspace
;             - main_pru1.asm file gets copied to ccs workspace
;             - Modify main_pru1.asm file
;             - Build the pru project, after which .out (Executable output file) and .h (Firmware header) files gets generated
;             - Either .out (Executable output file) can be loaded to PRU using ccs or R5F can write to PRU IRAM using PRUICSS driver
;   - Using makefile:
;             - Use command gmake -all to build PRU project     
;
;***************************************************************************************

; includes

    .include "lcd_pru1_macro.h"
; SP_BANK0 id is specified in macro

; xfr2vbus widget
RD_ID0      .set     0x60
RD_ID1      .set     0x61
; xfr2vbus read command for different size and mode
RD_AUTO_4B  .set 	1
RD_AUTO_32B .set    5
RD_AUTO_64B .set    7
RD_SIZE_4B  .set    0
RD_SIZE_32B .set    4
RD_SIZE_64B .set    6

SYNC_TEST   .set     0
DISP_MODE   .set     1
RED         .set     1
ONCHIP_SRAM .set     0x70000000  ; same for AM261 and AM243
HSYNC       .set     16

; display defines
LINES           .set 480
H_LINES         .set 525
VACTIVE         .set 880
EVEN_ODD_LINES  .set 240 ; even + odd line count
HWIDTH          .set 48
HPORCH          .set 40
DCLK            .set 7


    ; register name assignment
    .asg     r20.w2 , h_count
    .asg     r22.w0 , h_count_max
    .asg     r26    , line_counter_reg
    .asg     r27    , pxl_cnt_reg
    .asg     r28    , r1_init_reg
    .asg     r29    , data_reg

; CCS/makefile specific settings
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

    .global     main
    .sect       ".text"

;********
;* MAIN *
;********

main:
    jmp    l_lut_gen                 ; LUT for blue data pin mapping
l_lut_ret:

; init counters
    ldi    h_count_max, H_LINES+1   ; it is 525 + 1 because of add instruction before comparison
    ldi    h_count, 1
    ldi    pxl_cnt_reg, 0x0c00      ; b1 = 12 iterations with 64 pixels -> 768 pxl
    ldi32  r1_init_reg, 0x0008090a  ; init values for mvib instruction
                                    ; starting index for red pixel shifts every 64 byte DMA and repeats after 3
; configure xfr2vbus dma
    ldi    r18, RD_AUTO_64B         ; size 64 bytes, auto increments
    ldi32  r19, ONCHIP_SRAM         ; same address for AM243 and AM261
    ldi    r20, 0                   ; no 48 bit address
    xout   RD_ID1, &r18, 12         ; write address and command
                                    ; AM243x has 48 bit address, AM261 is 32 bit address

    set    r30, r30, HSYNC          ; start with HSYNC high
    ldi    r0.w0, HWIDTH*DCLK-4     ; 48 DCLKs * 7 cycles - 4 cycles

    ; init SP0 r0 = 0
    ldi    r0.w0, 0xff00            ; start sync sequence with PRU0 using SP0 and r0.b0
    xout   SP_BANK0, &r0.b0, 1
l_start:
;   set   output signal
    set    r30,r30, 0               ; set data output to 0 before start

; wait for first flag from PRU0 in SPAD0
; requires to start PRU1 before PRU0
l_wait_for_trigger:
    xin    SP_BANK0, &r0.b0, 1
    qbne   l_wait_for_trigger, r0.b0, 1
    xout   SP_BANK0, &r0.b1, 1

    nop                             ; wait period to align with PRU0
    nop                             ; 3 nops to cycle align both cores
    nop
    nop
    ldi    r0.w0, HWIDTH*DCLK-4     ; 48 DCLKs * 7 cycles - 4 cycles
                                    ; 4 cycles between clr and set instructions
    ; line counter r26.b2 = 240 even+odd lines, r26.b3 current line count
    ldi     line_counter_reg.w2, EVEN_ODD_LINES
; --------------------------------------------------------------
; VSYNC has 525 HSYNC
; HSYNC has 925 DCLK
; DCLK  has 7 cycles @ 5ns = 35 MHz
; DE is active after 32 HSYNC for 480 HSYNC and inactive for 13 HSYNC
; DE is active after 40 DCLK for 800 DCLK inactive for 40 DCLK and 48 DLCK (HSYNC low)
;
; State definition for data transfers.
; V_pre: 32_HSYNC
; V_active: 512_HSYNC
; V_post: 525_HSYNC
; H_pre: 40_DCLK
; H_active:880_DCLK
; H_post: 928_DCLK
;
; Counter definition
; h_count: 1..525
;
; ---------------------------------------------------------------


iteration:
    ; hsync low
    ; stay low for 48 DCLK (35 ns) = 1.680 us
    clr    r30, r30, HSYNC     ;

    loop l_endloop, r0.w0  ; 48 DCLKs * 7 cycles - 4 cycles
    nop
l_endloop:
; hsync_high
    add    h_count, h_count, 1      ; hsync counter
; todo branch after 32
    qbeq   l_v_active,h_count, 33   ; branch after 32
;    nop
    set    r30,r30, HSYNC
    qbeq   l_last_hs, h_count, h_count_max
    ; subtract 5 extra cycle (ec) from additional instructions outside the loop!
    ldi    r0.w0, VACTIVE*DCLK-6        ; oc1
    loop l1_endloop, r0.w0
    nop                             ; oc2
l1_endloop:
    ; subtract 2 extra cycles from additional instructions outside the loop.
    ldi   r0.w0, HWIDTH*DCLK-4      ; oc3
    qba   iteration                 ; oc4

; *****************************  end of one vertical period ***************************

l_last_hs:
    ; subtract 5 extra cycle (ec) from additional instructions outside the loop!
    ldi    r0.w0, VACTIVE*DCLK-13        ; ec1
    loop l1_endloop_hs, r0.w0
    nop                         ; ec2
l1_endloop_hs:
    ; xfr2vbus restart for both channels takes 8 cycles
    ; ldi    r18, RD_SIZE_64B    ; clear auto mode
    ; xout   RD_ID1, &r18, 4
    xin    RD_ID1, &r2, 64

    ldi    r18, RD_AUTO_64B
    ldi32  r19, ONCHIP_SRAM
    xout   RD_ID1, &r18, 8

    ;reset h_count
    ldi   h_count, 1
    ldi   line_counter_reg.w2, EVEN_ODD_LINES
    ; subtract 2 extra cycles from additional instructions outside the loop.
    ldi   r0.w0, HWIDTH*DCLK-4       ; oc3
    qba   iteration           ; oc4

    halt


; HSYNC count = 32 start with active data transmission in h_count=33 and 40 DCLKs
l_v_active:
     set    r30,r30, HSYNC
     ; line counter r26.b2 = 240 even+odd lines, r26.b3 current line count
    nop                                   ; oc1
    ldi    pxl_cnt_reg.b0, 0              ; oc2, reset pxl counter
    ; wait for 39 DCLKs - 4 opcodes - 1 DCLK until data gets out
    ; remaining DCLK to make it exact 40 is in macro
    ldi    r0.w0, (HPORCH-1)*DCLK-4                  ; oc3
    loop   la1_endloop, r0.w0             ; oc4 as loop has one cycle on first iteration
    nop                                   ; nop doe not count as opcode as it is part of the loop
la1_endloop:
    ; one DCLK to get data from IPC
    ; r1.b0 used for SP0, r1.b1 used for SP1 and r1.b2 used for SP2
    ; send SP0 Bank wich is 21 x blue

; ****************************** odd line ****************************
    m_lcd_pru1_odd_line
; ****************************** odd line end ************************
    ldi    r0.w0, (HPORCH+1)*DCLK-5        		  ; oc3, wait 41 DCLK before new HSYNC
    loop l1_endloop_o1, r0.w0             ; oc4
    nop                                   ; oc5
l1_endloop_o1:
    clr   r30, r30, HSYNC
    ldi   r0.w0, HWIDTH*DCLK-5                   ; oc1
    loop l_endloop_o2, r0.w0              ; oc2, 48 DCLKs * 7 cycles - 4 cycles
    nop
l_endloop_o2:
; hsync_high
    add    h_count, h_count, 1            ; oc3, hsync counter
    nop                                   ; oc4
    set    r30,r30, HSYNC                 ; oc5

    ; wait for 39 DCLKs - 2 opcodes
    ldi    r0.w0, (HPORCH-1)*DCLK-2       ; oc1
    loop la1_endloop_e, r0.w0             ; oc2
    nop                         ; ec2
la1_endloop_e:

; ****************************** even line ****************************
    m_lcd_pru1_even_line
; ****************************** even line end ************************

    add    line_counter_reg.b3, line_counter_reg.b3, 1    ; oc2
    ldi    r0.w0, (HPORCH+1)*DCLK-5       ; oc3, wait 41 DCLK - 5 opcodes before new HSYNC
    loop l1_endloop_o3, r0.w0             ; oc4
    nop
l1_endloop_o3:
    clr   r30, r30, HSYNC                 ; oc5
    ldi   r0.w0, HWIDTH*DCLK-5                   ; oc1
    loop l_endloop_o4, r0.w0              ; oc2, 48 DCLKs * 7 cycles - 4 cycles
    nop
l_endloop_o4:
; hsync_high
    add    h_count, h_count, 1            ; oc3, hsync counter
    qbne   l_v_active, line_counter_reg.b2, line_counter_reg.b3  ; oc4

; **************************** end of display area ********************

    ;first hsync after data enable low
    set    r30,r30, HSYNC                 ; oc5
    ; stay high for 880 DCLKs
    ; subtract 8 extra cycle (oc) from additional instructions outside the loop!
    ldi    r0.w0, VACTIVE*DCLK-8          ; oc1
    loop   l2_endloop, r0.w0              ; oc2
    nop
l2_endloop:

    ; xfr2vbus restart for both channels takes 8 cycles
    ldi    r18, RD_SIZE_64B               ; oc3, clear auto mode
    xout   RD_ID1, &r18, 4                ; oc4
    xin    RD_ID1, &r2, 64                ; oc5
    ldi    r0.w0, HWIDTH*DCLK-4               ; oc6
    qba    iteration                      ; oc7
                                          ; oc8 is HSYNC out

; *********************** LUT for blue data pin map *************************
; LUT has 256 entries to convert index (16-bit pointer) to actual bit map
; bit 0 = GPO0
; bit 1 = GPO1
; bit 2 = GPO2
; bit 3 = GPO6
; bit 4 = GPO11
; bit 5 = GPO12
; bit 6 = GPO13
; bit 7 = GPO15

l_lut_gen:
    ; look_up table in PRU firmware
    ldi32  r3, 0x02000000
    ldi    r2.w0, 0xff00
l_lut:
    ldi    r2.w2, 0   ; init 16 bit output
    qbbc   l_bit0, r2.b0, 0
    set    r2.w2, r2.w2, 0
l_bit0:
    qbbc   l_bit1, r2.b0, 1
    set    r2.w2, r2.w2, 1
l_bit1:
    qbbc   l_bit2, r2.b0, 2
    set    r2.w2, r2.w2, 2
l_bit2:
    qbbc   l_bit6, r2.b0, 3
    set    r2.w2, r2.w2, 6
l_bit6:
    qbbc   l_bit11, r2.b0, 4
    set    r2.w2, r2.w2, 11
l_bit11:
    qbbc   l_bit12, r2.b0, 5
    set    r2.w2, r2.w2, 12
l_bit12:
    qbbc   l_bit13, r2.b0, 6
    set    r2.w2, r2.w2, 13
l_bit13:
    qbbc   l_bit15, r2.b0, 7
    set    r2.w2, r2.w2, 15
l_bit15:
    sbco   &r2.w2, c24, r3.w0, 2 ; store in DRAM1 from PRU1
    add    r2.b0, r2.b0, 1       ; input value
    add    r3.w0, r3.w0, 2       ; lut index
    qbne   l_lut, r3.w0, r3.w2   ; 256 iterations

    jmp    l_lut_ret

; *********************** LUT for blue data pin map end **********************

