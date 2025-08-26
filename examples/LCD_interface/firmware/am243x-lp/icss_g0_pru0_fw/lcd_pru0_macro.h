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
;   File:     lcd_pru0_macro.h
;
;   Brief:    macros to support display code for 800 x 480 24 bpp color display
;
;
;***************************************************************************************

    .if    !$defined("__lcd_pru0_macro_h")
__lcd_pru0_macro_h    .set 1

nde_clk_mask_reg   .set   r25
de_clk_mask_reg    .set   r26
data_reg           .set   r27
SP_BANK0           .set   10
SP_BANK1           .set   11
SP_BANK2           .set   12
RD_ID0             .set   0x60
CLK                .set   8
DE                 .set   19
; defines

;************************************************************************************
;   Macro: m_lcd_pru0_first_even_line
;
;          Macro covers 7 clocks which is fixed pattern. First instruction assume previous instruction set clock high
;
;
;   PEAK cycles:
;           7 cycles exact wich is 35 ns per macro
;
;   Registers:
;           r1.b0    - pointer for mviw insruction
;           r2 - r17 - 64 byte of RGB data
;           r28.b0   - pixel block counter
;           r28.b1   - max pixel block count = 12
;           r27      - data_reg
;          (r29      - used for VSYNC level in main)
;           r30      - PRU output register
;
;   Parameters:
;            no
;
;   Returns:
;            no
;
;************************************************************************************



; **************************** odd line display **********************
m_lcd_pru0_odd_line   .macro
    ; previous cycle needs to be clock high setting
    ; there are 4 high cycles and three low cycle
    ldi    r28.w0, 0x0C00                ; number 64 pxl blocks
l_next_64px?:
    xin    RD_ID0, &r2, 64               ; read 64 bytes data from SRAM
    mviw   data_reg.w0, *r1.b0           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; send data with rising edge of clock
    loop   end_loop1?, 19

; repeat 19 times
    add    r1.b0, r1.b0, 3               ; starts with 8 + 21 * 3
    mviw   data_reg.w0, *r1.b0           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; send data with rising edge of clock
    nop                                  ; moving this nop to the end supports the hw loop
end_loop1?:
; need to seperate out repeat 20 to transition without nop at the end
    add    r1.b0, r1.b0, 3               ; starts with 8 + 21 * 3
    mviw   data_reg.w0, *r1.b0           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; send data with rising edge of clock

    ; last pixel 22 which is split into two banks 0/1
    mov    data_reg.b0, r17.b3           ; pixel 22 red
    xin    RD_ID0, &r2, 64               ; read 64 bytes data from SRAM
    add    r28.b0, r28.b0, 1             ; increment 64 pxl counter
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, r2.b0, 3         ; pixel 22 green
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; send data with rising edge of clock

; repeat 20 times as last pixel is not spanning over two banks for PRU0
    loop   end_loop2?, 20
    add    r1.b1, r1.b1, 3
    mviw   data_reg.w0, *r1.b1           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; send data with rising edge of clock
    nop                                  ; moving this nop to the end supports the hw loop
end_loop2?:
; need to seperate out repeat 21 to transition without nop at the end
    add    r1.b1, r1.b1, 3
    mviw   data_reg.w0, *r1.b1           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; send data with rising edge of clock

    ; bank 2 all is aligend except the start position
    ; nop                                  ; moved r1 to SP
    mov    r1, r1_init_reg
    xin    RD_ID0, &r2, 64               ; read 64 bytes data from SRAM
    mviw   data_reg.w0, *r1.b2           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; send data with rising edge of clock

; repeat 19 times
    loop   end_loop3?, 19
    add    r1.b2, r1.b2, 3
    mviw   data_reg.w0, *r1.b2           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; send data with rising edge of clock
    nop
end_loop3?:
    add    r1.b2, r1.b2, 3
    mviw   data_reg.w0, *r1.b2           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; send data with rising edge of clock
    qbne   l_next_64px?, r28.b1, r28.b0   ; exit loop after 12 iterations

; last 32 pixels

    ; send last 32 pixel
    xin    RD_ID0, &r2, 64               ; read 64 bytes data from SRAM
    mviw   data_reg.w0, *r1.b0           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; send data with rising edge of clock

; repeat 19 times
    loop   end_loop4?, 19
    add    r1.b0, r1.b0, 3
    mviw   data_reg.w0, *r1.b0           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; send data with rising edge of clock
    nop
end_loop4?:
; need to take out last bit to get proper clock timing
    add    r1.b0, r1.b0, 3
    mviw   data_reg.w0, *r1.b0           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; send data with rising edge of clock
    nop

   ; last pixel 22 which is split into two banks 0/1
    mov    data_reg.b0, r17.b3           ; pixel 22 red
    xin    RD_ID0, &r2, 64               ; read 64 bytes data from SRAM
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, r2.b0, 3         ; pixel 22 green
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;   set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; send data with rising edge of clock

; repeat 10 times to match 32 pxl at the end of 800 pixel
    loop   end_loop5?, 10
    add    r1.b1, r1.b1, 3
    mviw   data_reg.w0, *r1.b1           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; send data with rising edge of clock
    nop
end_loop5?:
    nop
    nop
    clr    r30, r30, CLK                 ; set clock low
    nop
    nop
; set data high after last with clock high and DE low
;    set    r30, r30, CLK                ; set clock high
    mov     r30, nde_clk_mask_reg        ; set clock and data high

    ;for even line processing r1.b1 and SP_BANK1 are up to date.
   .endm

; **************************** even line display **********************

m_lcd_pru0_even_line    .macro
    ; continue with bank1 data - 11 pxls
    ; repeat 11 times to match 32 pxl at the beginnning of even line
    loop   end_loop6?, 10
    add    r1.b1, r1.b1, 3
    mviw   data_reg.w0, *r1.b1           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; pxl 1-10
    nop
end_loop6?:
    ; 10th is out of loop to get clean clock timing.
    add    r1.b1, r1.b1, 3
    mviw   data_reg.w0, *r1.b1           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; pxl 11
    ; remaining 21 of first 32 pxl
    ; bank 2 all is aligend except the start position
    ; nop                                ; moved r1 to SP
    mov    r1, r1_init_reg
    xin    RD_ID0, &r2, 64               ; read 64 bytes data from SRAM
    mviw   data_reg.w0, *r1.b2           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; pxl 12
; repeat 19 times
    loop   end_loop7?, 19
    add    r1.b2, r1.b2, 3
    mviw   data_reg.w0, *r1.b2           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; pxl 12-31
    nop
end_loop7?:
    add    r1.b2, r1.b2, 3
    mviw   data_reg.w0, *r1.b2           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; pxl 32

    ; send 12 times 64 pxl - even line also ends even on e/o SP2
    ldi    r28.w0, 0x0C00                ; number 64 pxl blocks
l_next_64px_e?:
    xin    RD_ID0, &r2, 64               ; read 64 bytes data from SRAM
    mviw   data_reg.w0, *r1.b0           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
 ;   set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; pxl 1
    loop   end_loop8?, 19

; repeat 19 times
    add    r1.b0, r1.b0, 3               ; starts with 8 + 21 * 3
    mviw   data_reg.w0, *r1.b0           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; pxl 2-20
    nop                                  ; moving this nop to the end supports the hw loop
end_loop8?:
; need to seperate out repeat 20 to transition without nop at the end
    add    r1.b0, r1.b0, 3               ; starts with 8 + 21 * 3
    mviw   data_reg.w0, *r1.b0           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; pxl 21

    ; last pixel 22 which is split into two banks 0/1
    mov    data_reg.b0, r17.b3           ; pixel 22 red
    xin    RD_ID0, &r2, 64               ; read 64 bytes data from SRAM
    add    r28.b0, r28.b0, 1             ; increment 64 pxl counter
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, r2.b0, 3         ; pixel 22 green
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; pxl 22

; repeat 20 times as last pixel is not spanning over two banks for PRU0
    loop   end_loop9?, 20
    add    r1.b1, r1.b1, 3
    mviw   data_reg.w0, *r1.b1           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; pxl 21-41
    nop                                  ; moving this nop to the end supports the hw loop
end_loop9?:
; need to seperate out repeat 21 to transition without nop at the end
    add    r1.b1, r1.b1, 3
    mviw   data_reg.w0, *r1.b1           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; pxl 42

    ; bank 2 all is aligend except the start position
    ; nop                                  ; moved r1 to SP
    mov  r1, r1_init_reg
    xin    RD_ID0, &r2, 64               ; read 64 bytes data from SRAM
    mviw   data_reg.w0, *r1.b2           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; pxl 43

; repeat 19 times
    loop   end_loop10?, 19
    add    r1.b2, r1.b2, 3
    mviw   data_reg.w0, *r1.b2           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; pxl 44-63
    nop
end_loop10?:
    add    r1.b2, r1.b2, 3
    mviw   data_reg.w0, *r1.b2           ; pointer operation
    clr    r30, r30, CLK                 ; set clock low
    lsl    data_reg.w1, data_reg.b1, 3   ; align green data to pin mapping
    or     data_reg, data_reg, de_clk_mask_reg  ;set clock and DE
;    set    data_reg, data_reg, CLK       ; set clock in output data
    mov    r30, data_reg                 ; pxl 64
    qbne   l_next_64px_e?, r28.b1, r28.b0   ; exit loop after 12 iterations
    nop
    nop
    clr    r30, r30, CLK                 ; set clock low
    nop
    mov    r1, r1_init_reg
; set data high after last
;    set    r30, r30, CLK                ; set clock high
;    ldi     r30.w0, 0x100                ; set clock and data high
    mov     r30, nde_clk_mask_reg        ; set clock and data high
    .endm

    .endif  ; __lcd_pru0_macro_h
