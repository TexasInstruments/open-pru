; Copyright (C) 2025 Texas Instruments Incorporated - http://www.ti.com/
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
;
;     file:   macros.inc
;
;     brief:  PRU cycle-counter macros

M_ENABLE_PRU_CYCLE_COUNTER .macro
    lbco    &scratch, PRU_CTRL_CONST, 0, 4
    clr     scratch, scratch, 3
    sbco    &scratch, PRU_CTRL_CONST, 0, 4
    ldi32   scratch2, 0
    sbco    &scratch2, PRU_CTRL_CONST, PRU_CYCLCNT, 4
    set     scratch, scratch, 3
    sbco    &scratch, PRU_CTRL_CONST, 0, 4
  .endm

; Macro to reset cycle counter
M_RESET_CYCLCNT .macro
    ldi32   scratch2, 0
    sbco    &scratch2, PRU_CTRL_CONST, PRU_CYCLCNT, 4
    .endm

; Macro to read cycle counter
M_READ_CYCLCNT .macro dst
    lbco    &dst, PRU_CTRL_CONST, PRU_CYCLCNT, 4
    .endm
