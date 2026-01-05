; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2018-2025 Texas Instruments Incorporated - http://www.ti.com/

;************************************************************************************
;   File:     pwm_toggle.asm
;
;   Brief:	  contains Multicore scheduling Task
;
;************************************************************************************
	.sect	".text:pwm_toggle"
	.clink
	.global	FN_PWM_INIT_TOGGLE_PRU1_TASK
; File includes
    .include  "pru_io/firmware/common/icss_xfer_defines.inc"
;*************************************************
;   Task : FN_PWM_INIT_TOGGLE_PRU1_TASK
;   Description : Sets PWM init to Logic Low for configured PIN
;   			  => Configured PIN reads Logic Low
;   Assumption : r24 preloaded with toggle value, PWM out is configured
;   Peak Cycles : 4
;
;*************************************************
FN_PWM_INIT_TOGGLE_PRU1_TASK:
    ; GPO toggle using PWM init state
    ; rx.b2 --> Set PIN high
	; rx.b3 --> Set PIN low
    sbco    &r24.b3,c5,0x58, 1

	; return from task
    xin    TM_YIELD_XID, &R0.b3,1
 	nop
    nop

;*****************************************************************************
;                                     END
;*****************************************************************************
