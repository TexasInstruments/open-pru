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
;   File:     lcd_pru1_macro.h
;
;   Brief:    macros to support display code for 800 x 480 24 bpp color display
;
;
;***************************************************************************************

    .if    !$defined("__lcd_pru1_macro_h")
__lcd_pru0_macro_h    .set 1


SP0_ID             .set   10
SP1_ID             .set   11
SP2_ID             .set   12

; xfr2vbus read widget
RX_ID1             .set   0x61

; defines

;************************************************************************************
;   Macro: m_lcd_pru1_first_even_line
;
;          Macro covers 7 clocks which is fixed pattern. First instruction assume previous instruction set clock high
;
;
;   PEAK cycles:
;           7 cycles exact wich is 35 ns per DCLK, 800 pixels per line
;
;   Registers:
;
;
;   Parameters:
;            no
;
;   Returns:
;            no
;
;************************************************************************************



; **************************** odd line display **********************
m_lcd_pru1_odd_line   .macro
    mov   r1, r1_init_reg                ; init values for pointer which is different per SP
l_next_64pxl:
    xin   RX_ID1, &r2, 64                ; read xfr2vbus
    mvib  data_reg, *r1.b0               ; read from register
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b0, r1.b0, 3                ; point to next blue data

    loop  l_sp0, 19                      ; 7 cycles per color
    mvib  data_reg, *r1.b0               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b0, r1.b0, 3                ; point to next blue data       cc 6
    nop
l_sp0:

    ; pixel 21
    mvib  data_reg, *r1.b0               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    nop

    ; pixel 1 of SP1 bank, again 21 data
    xin   RX_ID1, &r2, 64                ; read xfr2vbus
    mvib  data_reg, *r1.b1               ; read from register
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b1, r1.b1, 3                ; point to next blue data

    loop  l_sp1, 19                      ; 7 cycles per color
    mvib  data_reg, *r1.b1               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b1, r1.b1, 3                ; point to next blue data       cc 6
    nop
l_sp1:

    mvib  data_reg, *r1.b1               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   pxl_cnt_reg.b0, pxl_cnt_reg, 1 ; increment 64pxl counter

    ; pixel 1 of SP2 bank, 22 data
    xin   RX_ID1, &r2, 64                ; read xfr2vbus
    mvib  data_reg, *r1.b2               ; read from register
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b2, r1.b2, 3                ; point to next blue data

    loop  l_sp2, 19                      ; 7 cycles per color
    mvib  data_reg, *r1.b2               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b2, r1.b2, 3                ; point to next blue data       cc 6
    nop                                  ;                               cc 7
l_sp2:

    ; pixel 21
    mvib  data_reg, *r1.b2               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b2, r1.b2, 3                ; point to next blue data       cc 6

    ; pixel 22
    mvib  data_reg, *r1.b2               ; read from register            cc 1
    mov   r1, r1_init_reg                ; init values for pointer which is different per SP
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    qbne  l_next_64pxl, pxl_cnt_reg.b0, pxl_cnt_reg.b1 ; 12 iterations

    ; last 32 pxl of odd line
    xin   RX_ID1, &r2, 64                ; read xfr2vbus
    mvib  data_reg, *r1.b0               ; read from register
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b0, r1.b0, 3                ; point to next blue data

    loop  l_sp0l, 19                     ; 7 cycles per color
    mvib  data_reg, *r1.b0               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b0, r1.b0, 3                ; point to next blue data       cc 6
    nop
l_sp0l:

    ; pixel 21
    mvib  data_reg, *r1.b0               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    nop

    ; pixel 1 of SP1 bank, again 11 data
    xin   RX_ID1, &r2, 64                ; read xfr2vbus
    mvib  data_reg, *r1.b1               ; read from register
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b1, r1.b1, 3                ; point to next blue data

    loop  l_sp1l, 10                      ; 7 cycles per color
    mvib  data_reg, *r1.b1               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b1, r1.b1, 3                ; point to next blue data       cc 6
    nop
l_sp1l:

    ;for even line processing r1.b0 and SP_BANK1 are up to date.
   .endm

; **************************** even line display **********************

m_lcd_pru1_even_line    .macro

    ; send remainder of SP1 - 10 pxl + 22 pxl from SP2
    ; SP1 and r1.b1 are still correct from odd line
;    mov   r1, r1_init_reg                ; init values for pointer which is different per SP
    ldi   pxl_cnt_reg.b0, 0              ; reset pxl counter

    loop  l_sp0_e, 9                    ; 7 cycles per color
    mvib  data_reg, *r1.b1               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b1, r1.b1, 3                ; point to next blue data       cc 6
    nop
l_sp0_e:
    ; pixel 10
    mvib  data_reg, *r1.b1               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    nop

    ; pixel 1 of SP2 bank, 22 data
    xin   RX_ID1, &r2, 64                ; read xfr2vbus
    mvib  data_reg, *r1.b2               ; read from register
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b2, r1.b2, 3                ; point to next blue data

    loop  l_sp2_e, 20                      ; 7 cycles per color
    mvib  data_reg, *r1.b2               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b2, r1.b2, 3                ; point to next blue data       cc 6
    nop                                  ;                               cc 7
l_sp2_e:
    ; pixel 22
    mvib  data_reg, *r1.b2               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    mov   r1, r1_init_reg

l_next_64pxl_e:
    xin   RX_ID1, &r2, 64                ; read xr2vbus
    mvib  data_reg, *r1.b0               ; read from register
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b0, r1.b0, 3                ; point to next blue data

    loop  l_sp0_e1, 19                      ; 7 cycles per color
    mvib  data_reg, *r1.b0               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b0, r1.b0, 3                ; point to next blue data       cc 6
    nop
l_sp0_e1:

    ; pixel 21
    mvib  data_reg, *r1.b0               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    nop

    ; pixel 1 of SP1 bank, again 21 data
    xin   RX_ID1, &r2, 64                ; read xfr2vbus
    mvib  data_reg, *r1.b1               ; read from register
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b1, r1.b1, 3                ; point to next blue data

    loop  l_sp1_e, 19                      ; 7 cycles per color
    mvib  data_reg, *r1.b1               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b1, r1.b1, 3                ; point to next blue data       cc 6
    nop
l_sp1_e:

    mvib  data_reg, *r1.b1               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   pxl_cnt_reg.b0, pxl_cnt_reg, 1 ; increment 64pxl counter

    ; pixel 1 of SP2 bank, 22 data
    xin   RX_ID1, &r2, 64                ; read xfr2vbus
    mvib  data_reg, *r1.b2               ; read from register
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b2, r1.b2, 3                ; point to next blue data

    loop  l_sp2_e1, 19                      ; 7 cycles per color
    mvib  data_reg, *r1.b2               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b2, r1.b2, 3                ; point to next blue data       cc 6
    nop                                  ;                               cc 7
l_sp2_e1:

    ; pixel 21
    mvib  data_reg, *r1.b2               ; read from register            cc 1
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    add   r1.b2, r1.b2, 3                ; point to next blue data       cc 6

    ; pixel 22
    mvib  data_reg, *r1.b2               ; read from register            cc 1
    mov   r1, r1_init_reg                ; init values for pointer which is different per SP
    lsl   data_reg.w0, data_reg.b0, 1    ; point to 16 bit values
    lbco  &r30.w0, c24, data_reg.w0, 2   ; convert to 16 bit pin map and send
    qbne  l_next_64pxl_e, pxl_cnt_reg.b0, pxl_cnt_reg.b1 ; 12 iterations
    .endm

    .endif  ; __lcd_pru1_macro_h
