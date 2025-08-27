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
	.sect	".text:fn_fg_output"
	.clink
	.global	||FIR_64||
;==========================================================================;
;                             Include Files                                ;
;==========================================================================;
    .include  "pru_io\firmware\common\icss_xfer_defines.inc"
;==========================================================================;
;                             	Macros		                               ;
;==========================================================================;
FIR_IP_MEM				.set 0x30010000	; size : 0x2000
FIR_DATA_SIZE			.set 0x2000
FIR_OP_MEM				.set 0x30012000	; size : 0x2000
; Coefficient mem span in ICSS SH MEM : 0x30014000 - 0x300140FF
FIR_COEF_MEM			.set 0x70000000	; global address
;==========================================================================;
;                             	Reg assignment                             ;
;==========================================================================;
    .asg r0.b0	, xin_shift
    .asg r1.b0	, mvid_reg_ptr
    .asg r1.w2	, ip_offset
   	.asg r24	, mul_acc_low 		; accumulates lower 32 bits of q62 product
   	.asg r25	, mul_acc_upp 		; accumulates upper 32 bits of q62 product
   	.asg r26	, mul_prd_reg_low
   	.asg r27	, mul_prd_reg_upp
   	.asg r28	, mul_opr_reg_1
   	.asg r29	, mul_opr_reg_2
;*****************************************************************************
;            void FN_LOGIC_SCOPE_DUAL_CORE();
;*****************************************************************************
||FIR_64||:
	; save PC to unused SPAD location
	mov r29.w2, r3.w2
	xout PRU_SPAD_B2_XID, &r29.w2, 2
	;***********************************
	; Init IP buffer in SPADs to zeros *
	;***********************************
	; load zeros to the input buffer parts in SPAD BO, B1 and B2
	zero &r0, (24*4)
	xout PRU_SPAD_B0_XID, &r0, (24*4)
	xout PRU_SPAD_B1_XID, &r0, (24*4)
	xout PRU_SPAD_B2_XID, &r0, (16*4)
	; init pointer to input data
	ldi ip_offset, 0
FIR_CYCLE_START:
********* FIR cycle (for every new input) ******************
;*** reconfigure xfer2vbus every cycle ***
    ldi r18, 0x5                       		; config: auto read mode on, read size-32 bit
    ldi32 r19, FIR_COEF_MEM        	 		; start address of window coefficients
    xout XFR2VBUSP_RD0_XID, &r18, 8      	; set configuration and updated address
    xin XFR2VBUSP_RD0_XID, &r2, 32       	; drain fifo to clear already loaded values
	; perform Input shifting while we wait for buffer to be filled

;*********************************************
;* shift input delay line and load new input *
;********************************************************************************
;* 64 inputs stored over three SPAD banks backwards for achieving a 'delay line'
;*		Distribution : (24) - (24) - (16)
;* Delay line shifted in each new input cycle
;* For shifting, inputs shifted by one and new input inserted in the first position
;* Shifting performed over 3 SPAD banks
;*
;*				|<-- SPAD 10 --->|<-- SPAD 11 --->|<-- SPAD 12 --->|
;*				|	(R0-R23)	 |	(R0-R23)	  |   (R0-R15)	   |
;*	Before : 	||N|N-1| ..|N-23|||N-24|.. .|N-47|||N-48|.. .|N-63||
;*		 		| * <-- (new ip) |				  |				   |
;*	After :		||N+1|N|...|N-22|||N-23|.. .|N-46|||N-47|.. .|N-62||
;********************************************************************
	; **SPAD B0**
	ldi32 r6, FIR_IP_MEM 				; load input base mem
	lbbo &r5, r6, ip_offset, 4 			; load new input from MEM
	ldi xin_shift, 24					; set shift to x (SPAD R0 --> PRU R6)
	xin PRU_SPAD_B0_XID, &R6, (24*4)	; 24 inputs
	ldi xin_shift, 25					; set shift to x+1
	xout PRU_SPAD_B0_XID, &R5, (24*4)	; save inputs shifted
	; **SPAD B1**
	mov r5, r29 						; load new input from previous SPAD
	ldi xin_shift, 24					; set shift to x
	xin PRU_SPAD_B1_XID, &R6, (24*4)	; 24 inputs
	ldi xin_shift, 25					; set shift to x+1
	xout PRU_SPAD_B1_XID, &R5, (24*4)	; save inputs shifted
	; **SPAD B2**
	mov r5, r29 						; load new input from previous SPAD
	ldi xin_shift, 24					; set shift to x
	xin PRU_SPAD_B2_XID, &R6, (16*4)	; 16 inputs
	ldi xin_shift, 25					; set shift to x+1
	xout PRU_SPAD_B2_XID, &R5, (24*4)	; save inputs shifted

	; clear accumulator registers
	and mul_acc_low, mul_acc_low, 0
	and mul_acc_upp, mul_acc_upp, 0

    ; additional wait till coefficient data is loaded
WAIT_DATA_READY1:
    xin     XFR2VBUSP_RD0_XID, &r18, 1
    qbne    WAIT_DATA_READY1, r18.b0, 5

;*************
;* 	x64MAC	 *
;*************
	ldi xin_shift, 1 		; point to first input in SPAD R0 (for loading to r29)
; *** perform multiplications for all inputs in SPAD_B0 ***
	loop FIR_MAC24_B0, 3			; loop over input values in SPAD_B0, 3 x 8
	xin XFR2VBUSP_RD0_XID, &r2, 32 	; load 8 coefficient values
  	ldi mvid_reg_ptr, 8				; point to first coefficient in R2
FIR_MAC8_B0:
  	xin PRU_SPAD_B0_XID, &mul_opr_reg_2, 4 		; load input as operand 2
  	mvid mul_opr_reg_1, *mvid_reg_ptr++ 		; load coefficient as operand 1

  	; *** multiply and accumulate ***
	nop
	xin MAC_XID , &mul_prd_reg_low, 8			; get product lower and upper 32 bits
	; apply sign correction
	qbbc    FIR_SGNCHK_CMPL_B0_1, mul_opr_reg_1, 31   ; sign check for op 1
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2
FIR_SGNCHK_CMPL_B0_1:
    qbbc    FIR_SGNCHK_CMPL_B0_2, mul_opr_reg_2, 31   ; sign check for op2
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_1
FIR_SGNCHK_CMPL_B0_2:
	; accumulate as q62 over two registers
	add mul_acc_low, mul_acc_low, mul_prd_reg_low   ; (carry set automatically)
	adc mul_acc_upp, mul_acc_upp, mul_prd_reg_upp  	; (carry from lower ACC added to upper part)
  	; *** multiply and accumulate ends ***

	add xin_shift, xin_shift, 1			; point to next input
	qbge FIR_MAC8_B0, mvid_reg_ptr, 36	; loop till 8 coefficient values are covered
FIR_MAC24_B0:	; 3x8 loop ends

	ldi xin_shift, 1 				; point back to first input in SPAD R0 (for loading to r29)
; *** perform multiplications for all inputs in SPAD_B1 ***
	loop FIR_MAC24_B1, 3			; loop over input values in SPAD_B1, 3 x 8
	xin XFR2VBUSP_RD0_XID, &r2, 32	; load 8 coefficient values
;	xin PRU_BSRAM_XID, &r2, 32		; load 8 coefficient values
  	ldi mvid_reg_ptr, 8				; point to first coefficient in R2
FIR_MAC8_B1:
  	xin PRU_SPAD_B1_XID, &mul_opr_reg_2, 4	; load input as operand 2
  	mvid mul_opr_reg_1, *mvid_reg_ptr++ 	; load coefficient as operand 1

	; *** multiply and accumulate ***
	nop
	xin MAC_XID , &mul_prd_reg_low, 8	; get product lower and upper 32 bits
	; apply sign correction
	qbbc    FIR_SGNCHK_CMPL_B1_1, mul_opr_reg_1, 31   ; sign check for op 1
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2
FIR_SGNCHK_CMPL_B1_1:
    qbbc    FIR_SGNCHK_CMPL_B1_2, mul_opr_reg_2, 31   ; sign check for op2
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_1
FIR_SGNCHK_CMPL_B1_2:
	; accumulate as q62 over two registers
	add mul_acc_low, mul_acc_low, mul_prd_reg_low   ; (carry set automatically)
	adc mul_acc_upp, mul_acc_upp, mul_prd_reg_upp  	; (carry from lower ACC added to upper part)
  	; *** multiply and accumulate ends ***

	add xin_shift, xin_shift, 1			; point to next input
	qbge FIR_MAC8_B1, mvid_reg_ptr, 36	; loop till 8 coefficient values are covered
FIR_MAC24_B1:	; 3x8 loop ends

	ldi xin_shift, 1 		; point back to first input in SPAD R0 (for loading to r29)
; *** perform multiplications for all inputs in SPAD_B2 ***
	loop FIR_MAC24_B2, 2	; loop over input values in SPAD_B2, 2 x 8
	xin XFR2VBUSP_RD0_XID, &r2, 32	; load 8 coefficient values
;	xin PRU_BSRAM_XID, &r2, 32		; load 8 coefficient values
  	ldi mvid_reg_ptr, 8				; point to first coefficient in R2
FIR_MAC8_B2:
  	xin PRU_SPAD_B2_XID, &mul_opr_reg_2, 4	; load input as operand 2
  	mvid mul_opr_reg_1, *mvid_reg_ptr++ 	; load coefficient as operand 1

	; *** multiply and accumulate ***
	nop
	xin MAC_XID , &mul_prd_reg_low, 8	; get product lower and upper 32 bits
	; apply sign correction
	qbbc    FIR_SGNCHK_CMPL_B2_1, mul_opr_reg_1, 31   ; sign check for op 1
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2
FIR_SGNCHK_CMPL_B2_1:
    qbbc    FIR_SGNCHK_CMPL_B2_2, mul_opr_reg_2, 31   ; sign check for op2
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_1
FIR_SGNCHK_CMPL_B2_2:
	; accumulate as q62 over two registers
	add mul_acc_low, mul_acc_low, mul_prd_reg_low   ; (carry set automatically)
	adc mul_acc_upp, mul_acc_upp, mul_prd_reg_upp  	; (carry from lower ACC added to upper part)
  	; *** multiply and accumulate ends ***

	add xin_shift, xin_shift, 1			; point to next input
	qbge FIR_MAC8_B2, mvid_reg_ptr, 36	; loop till 8 coefficient values are covered
FIR_MAC24_B2:	; 3x8 loop ends

;**** x64 MAC ends ****

	; scale accumulated output to q31
	lsl mul_acc_upp, mul_acc_upp, 1  	; get 0-30 bits
	lsr mul_acc_low, mul_acc_low, 31 	; get 31st bit
	or mul_acc_upp, mul_acc_upp, mul_acc_low ; combine to get product in q31

	; store accumulated output
	ldi32	r6, FIR_OP_MEM 					; load op base mem
	sbbo &mul_acc_upp, r6, ip_offset, 4 	; store output at corresponding position

	; loop through all inputs
	add ip_offset, ip_offset, 4	; point to next input in memory
	ldi r6, FIR_DATA_SIZE		; load data size
	qbgt FIR_CYCLE_START, ip_offset, r6 ; check the loop limit and repeat

********* FIR cycle ends ******************
	xin PRU_SPAD_B2_XID, &r29.w2, 2	; fetch saved PC
	jmp r29.w2						; return to saved PC

;*****************************************************************************
;                                     END
;*****************************************************************************
