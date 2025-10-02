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
;   File:     main_pru0.asm
;
;   Brief:
;
;   Steps to build :
;
;   - Using ccs:
;             - Import pru project to ccs workspace
;             - main.asm file gets copied to ccs workspace
;             - Modify main.asm file
;             - Build the pru project, after which .out (Executable output file) and .h (Firmware header) files gets generated
;             - Either .out (Executable output file) can be loaded to PRU using ccs or R5F can write to PRU IRAM using PRUICSS driver
;   - Using makefile:
;             - Use command gmake -all to build PRU project     
;
;***************************************************************************************

     .include "lcd_pru0_macro.h"

; defines
; broadside ID for scratch pad bankd
SP0_ID     .set     10
SP1_ID     .set     11
SP2_ID     .set     12

; pins
CLK        .set     8
DE         .set     19
           .asg     Data_Red, b0   ; one to one match on red
           .asg     Data_Green, w1   ; green data is in b11-18
shit_green .set     3    ; move lsb from 8 to 11

r1_init_reg        .set   r24
nde_clk_mask_reg   .set   r25
de_clk_mask_reg    .set   r26

ONCHIP_SRAM .set     0x70000000  ; same for AM261 and AM243

; xfr2vbus read command for differnet size and mode
RD_AUTO_4B  .set 	1
RD_AUTO_32B .set    5
RD_AUTO_64B .set    7
RD_SIZE_4B  .set    0
RD_SIZE_32B .set    4
RD_SIZE_64B .set    6

; --------------------------------------------------------------
; VSYNC has 525 HSYNC
; HSYNC has 928 DCLK
; DCLK  has 7 cycles @ 5ns = 35 MHz
; DE is active after 32 HSYNC for 480 HSYNC and inactive for 13 HSYNC
; DE is active after 40 DCLK for 800 DCLK inactive for 40 DCLK and 48 DLCK (HSYNC low)
;
; State definition for data transfers.
; V_pre: 32_HYSNC
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
; display defines
HSYNC       .set   928
EO_LINES    .set   240
H_PRE       .set   40
HWIDTH      .set   48
V_PRE       .set   31

; CCS/makefile specific settings
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

    .global     main
    .sect       ".text"

;********
;* MAIN *
;********

main:

    ; init pointer start offsets, bank 1 is 7 because there is a split pixel and increment comes before move.
    ldi32  r1_init_reg, 0x0090708
    ; confiure xfr2vbus dma
    ldi    r18, RD_AUTO_64B         ; size 64 bytes, auto increments
    ldi32  r19, ONCHIP_SRAM
    ldi    r20, 0
                                     ; AM243x has 48 bit address, AM261 is 32 bit address
    xout   RD_ID0, &r18, 12          ; configure widget for PRU0, 12 bytes to  cover r18-r20

    ; mask to set DE and CLK in one step bit 19 and bit 8
    ldi32  de_clk_mask_reg,  0x00080100
    ; mask to clear DE and set CLK in one step set bit 8 and bit 0
    ldi32  nde_clk_mask_reg, 0x00000101
    ; line counter r28.b2 = 240 even+odd lines, r28.b3 current line count
    ldi    r28.w2, EO_LINES
    ; clear r2-r4 register
    zero   &r2, 12
    ; states for VSYNC
    ldi    r29.w2, 0x0a05
    ; set vsync high to begin with
    sbco   &r29.b3,c5,0x58, 1
    ;  output clock low
    clr    r30, r30, CLK
    ;  DE low
    clr    r30, r30, DE
    ; ***************** PRU1 synchronization  ************************
    ; this scheme requres to run PRU1 before PRU0
    ldi    r0.w0, 0x0001
    ; clear response flag
    xout   SP0_ID,&r0.b1, 1
    ; set trigger
    xout   SP0_ID, &r0.b0, 1
    nop
    xin    SP0_ID, &r0.b1, 1
    qbbs   l_1st_comp, r0.b1, 0
    xin    SP0_ID, &r0.b1, 1
    qbbs   l_2nd_comp, r0.b1, 0
    xin    SP0_ID, &r0.b1, 1
    qbbs   l_3rd_comp, r0.b1, 0
    ; none occured
    halt

l_1st_comp:
    ; not possible due to run-time of code
    halt
l_2nd_comp:
    ; delay by one cycle as it was detected earlier by PRU1
    nop
l_3rd_comp:
    ; send one cycle earlier as it took PRU1 longer to detect.

    ; ******************* start display endless loop *************
l_display:
    ; start with clock and VSYNC low for 928 clk
    clr    r30, r30, CLK
    sbco   &r29.b2,c5,0x58, 1           ; sbco takes two cyles to output
    set    r30, r30, CLK                ; reference time 0 which is VSYNC low and CLK high at the same time
    nop
    nop
    nop
    clr    r30, r30, CLK                ; clk low after 4 cyces (20ns)
    ldi    r29.w0, HSYNC-2              ; 928 -2 which are outside the loop
    ; *** 926 clk hardware loop start ***
    loop   l_vsync_low, r29.w0
    set    r30, r30, CLK                ; clcok high after 3 cycles (15ns) matches 35 ns DCLK
    nop
    nop
    nop
    clr    r30, r30, CLK
    nop
    nop
l_vsync_low:
    ; *** 926 clk hardware loop end ***
    ; clk 928
    ; last clock - prepare VSYNC high
    set    r30, r30, CLK
    nop
    nop
    nop
    clr    r30, r30, CLK
    sbco   &r29.b3, c5, 0x58, 1
    ; clk 929 - prepare hw loop
    set    r30, r30, CLK
    nop
    nop
    nop
    clr    r30, r30, CLK
    ; blanking time is 31 * H + 40 + 48 DCLK - 3 cycles
    ldi32  r28, (V_PRE*HSYNC)+ H_PRE + HWIDTH-3   ; oc2

l_vsync_high:
    set    r30, r30, CLK                          ; oc3
    nop
    nop
    nop
    clr    r30, r30, CLK
    sub    r28,r28, 1
    qbne   l_vsync_high, r28, 0

    ; last cycle before
    set    r30, r30, CLK
    nop
    nop
    ; line counter r28.b2 = 240 even+odd lines, r28.b3 current line count
    ldi    r28.w2, EO_LINES

l_new_line:
    ; next four lines are needed to keep clk timing in firmware loop for lines
    clr    r30, r30, CLK                 ; set clock low
    nop
    nop
    set    r30, r30, CLK
; ****************************** odd line ****************************
    m_lcd_pru0_odd_line
; ****************************** odd line end ************************
;   128 clks before next display period
    loop l_blank_time, 126
    nop
    nop
    clr    r30, r30, CLK                 ; set clock low
    nop
    nop                 ;
    set    r30, r30, CLK                 ; set clock high
    nop
l_blank_time:
    nop
    nop
    clr    r30, r30, CLK                 ; set clock low
    nop
    nop
    set    r30, r30, CLK
; ****************************** even line ****************************
    m_lcd_pru0_even_line
; ****************************** even line end ************************
    loop l_blank_time1, 126
    nop
    nop
    clr    r30, r30, CLK                 ; set clock low
    nop
    nop
    set    r30, r30, CLK                 ; set clock high
    nop
l_blank_time1:
    add    r28.b3, r28.b3, 1
    qbne   l_new_line, r28.b3, r28.b2
    ; first clock after display + 126 DCLKs
    ; need to wait for 40 DCLKs before going to remaining 13 H count
    ; remaining clocks before new VSYNC: (13*928)-40
    ; next clock is #127
    clr    r30, r30, CLK                 ; set clock low
    ldi32  r28, (13*HSYNC)-H_PRE-HWIDTH  ; post blank time -1 for wrap around
l_post_blank:
    set    r30, r30, CLK
    ;nop
    ;nop
    ;nop
    ; xfr2vbus restart for both channels takes 8 cycles
    ldi    r18, RD_SIZE_64B    ; clear auto mode
    xout   RD_ID0, &r18, 4
    xin    RD_ID0, &r2, 64

    clr    r30, r30, CLK
    sub    r28,r28, 1
    qbne   l_post_blank, r28, 0

    set    r30, r30, CLK
    ;nop
    ;nop
    ldi    r18, RD_AUTO_64B
    xout   RD_ID0, &r18, 8
    qba    l_display

; -----------------------------------------------------------




