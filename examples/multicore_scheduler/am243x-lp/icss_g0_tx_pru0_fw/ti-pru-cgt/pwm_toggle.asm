; Copyright (C) 2024-2025 Texas Instruments Incorporated - http://www.ti.com/
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions
; are met:
;
;        * Redistributions of source code must retain the above copyright
;          notice, this list of conditions and the following disclaimer.
;
;        * Redistributions in binary form must reproduce the above copyright
;          notice, this list of conditions and the following disclaimer in the
;          documentation and/or other materials provided with the
;          distribution.
;
;        * Neither the name of Texas Instruments Incorporated nor the names of
;          its contributors may be used to endorse or promote products derived
;          from this software without specific prior written permission.
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
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;************************************************************************************
;   File:     pwm_toggle.asm
;
;   Brief:	  contains Multicore scheduling Task
;
;************************************************************************************
	.sect	".text:pwm_toggle"
	.clink
	.global	FN_PWM_INIT_TOGGLE_TXPRU0_TASK
; File includes
    .include  "pru_io\firmware\common\icss_xfer_defines.inc"
;*************************************************
;   Task : FN_PWM_INIT_TOGGLE_PRUTX0_TASK
;   Description : Sets PWM init to Logic High for configured PIN
;   			  => Configured PIN reads Logic High
;   Assumption : r24 preloaded with toggle value, PWM out is configured
;   Peak Cycles : 4
;
;*************************************************
FN_PWM_INIT_TOGGLE_TXPRU0_TASK:
    ; GPO toggle using PWM init state
    ; rx.b2 --> Set PIN high
	; rx.b3 --> Set PIN low
    sbco    &r24.b2,c5,0x58, 1

	; return from task
    xin    TM_YIELD_XID, &R0.b3,1
	nop
    nop

;*****************************************************************************
;                                     END
;*****************************************************************************
