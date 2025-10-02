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
******************************************************************
; File: fn_fg_output.asm
;
; Description:
;       File that implements a Configurable Frequency Generator
;
;*****************************************************************
	.sect	".text:fn_fg_output"
	.clink
	.global	||CONFIG_FREQ_GEN||
;--------------------------------------------------------------------------;
;   File includes
;--------------------------------------------------------------------------;
    .include  "icss_xfer_defines.inc"

;--------------------------------------------------------------------------;
;   Macros
;--------------------------------------------------------------------------;

; ** Frequency Configuration ** 
; eg. desired frequecy = 16.384Mhz, T = 61.0351562500
; Set the fraction to be accumulated in fractional adder in Q30 format
; eg. T-T_shorter_period = 1.03515625
CFG_FRAC_ACC		.set	0x42400000	; q30 representation of 1.03515625 ns
; Set max accumulation limit = T_higher_period - T_lower_period
CFG_FRAC_ACC_MAX	.set 	0xc0000000	; q30 representation of 3 ns
; set the closest shorter time period in cycles
CFG_T_SH_CYC	.set	20 ; 20 cycles = 60 ns @333Mhz
CFG_HIGH_CYC	.set	(CFG_T_SH_CYC/2) ; number of cycles GPO needs to be high
CFG_LOW_CYC		.set	CFG_T_SH_CYC-CFG_HIGH_CYC

; enforce constraints for configuration
    .if (CFG_T_SH_CYC < 8)
    .error "parameter CFG_T_SH_CYC must be set to >= 8 cycles"
    .endif
;************************************************************************************
;
;  Macro: m_nopx
;
;  precisely wait for P1 cycles(P1 > 1) 
;
;   PEAK cycles:
;       P1
;   Pseudo code:
;       WAIT(P1) // wait for P1 cycles  ; 
;
;   Parameters:
;      P1 : Number of cycles to wait
;
;   Returns:
;      None
;************************************************************************************

m_nopx    .macro P1
    loop endloopx?, P1 - 1
    NOP
endloopx?:
    .endm

;*******************************************
;   Function : CONFIG_FREQ_GEN
;
;   Description:
;   			Custom(configurable) Frequency Clock(square wave) generator using
;   			fractional adder. Required Time period of desired
;				frequency is generated on an average by generating
;   			the closest longer and shorter time periods, a
;   			calculated number of times, using a fractional adder.
;   Pseudocode:
;			//configure constants for WAIT times
;   		INIT:
;   			SET GPO to High
;			CYCLE_START:
;				WAIT(X)  // wait necessary time for GPO to stay high
;				if (Accumulated Fraction > max limit(cycle time))
;					GOTO LONGER_PERIOD
;				SET GPO to low
;				Add fraction //accumulate every short period
;				WAIT(Y)  // wait necessary time for GPO to stay low
;				SET GPO to High
;				GOTO CYCLE_START
;			LONGER_PERIOD:
;				SET GPO to low
;				SUB cycle time
;				ADD fraction //accumulate every short period
;				WAIT(Z)  // wait necessary time for GPO to stay low
;				SET GPO to High
;				WAIT(1)	// wait one more cycle to make period longer
;				GOTO CYCLE_START
;	Cycle budget: N/A
;   Registers modified: r7, r8, r9, r30.b0
;   Assumptions : CFG_HIGH_CYC >= 3, CFG_LOW_CYC >= 3 
;               : Frequency configuration macros calculated and adjusted for clock and desired frequency 
;*******************************************
||CONFIG_FREQ_GEN||:
    ; intialization
    ldi32    r7, CFG_FRAC_ACC
    ldi32    r8, CFG_FRAC_ACC_MAX
    ldi32    r9, 0
    ; set GPO0 to high
	ldi		r30.b0, 1
CFG_START_PERIOD:
    ; wait for configured number of cycles
	m_nopx	(CFG_HIGH_CYC-3)
	qblt    CFG_SEND_HIGHER_T, r9.b3, r8.b3 ; check remainder in register r9.b3
	ldi		r30.b0, 0	; set GPO0 to low
    add     r9, r9, r7	; add fraction
    ; wait for configured number of cycles
	m_nopx	(CFG_LOW_CYC-2)
    ldi		r30.b0, 1	; set GPO0 to back to high
    qba     CFG_START_PERIOD
CFG_SEND_HIGHER_T:
    ldi		r30.b0, 0	; set GPO0 to low
    sub     r9, r9, r8	; subtract max acc value from reminder
    add     r9, r9, r7	; add fraction
    ; wait for configured number of cycles
	m_nopx	(CFG_LOW_CYC-3)
    ldi		r30.b0, 1	; set GPO0 to back to high
    nop					; wait an extra cycle (for higher T)
    qba     CFG_START_PERIOD
	; return from function
	jmp		r3.w2
;*****************************************************************************
;                                     END
;*****************************************************************************
