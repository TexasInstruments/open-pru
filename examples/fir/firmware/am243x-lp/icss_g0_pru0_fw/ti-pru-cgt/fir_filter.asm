; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2018-2025 Texas Instruments Incorporated - http://www.ti.com/

******************************************************************
; File: fir_filter.asm
;
; Description:
;       File that implements a 64 point FIR filter
;
;*****************************************************************
	.sect	".text:fir_filter"
	.clink
	.global	||FN_FIR_64||
;--------------------------------------------------------------------------;
;   File includes
;--------------------------------------------------------------------------;
    .include  "pru_io/firmware/common/icss_xfer_defines.inc"
;==========================================================================;
;                             	Macros		                               ;
;==========================================================================;
FIR_IP_MEM				.set 0x10000	; size : 0x2000
FIR_DATA_SIZE			.set 0x2000
FIR_OP_MEM				.set 0x12000	; size : 0x2000
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

;*******************************************
;   Function : FN_FIR_64
;
;   Description:
;   			This function applies a 64-point FIR filter on a configured number of inputs.
;	Pre-requisites:
;               1. FIR Coefficients loaded in FIR_COEF_MEM
;			    2. Inputs for FIR operation present in FIR_IP_MEM
;   Pseudocode:
;			INIT:
;				SET SPADs to zeros // ip Delay Line buffer in SPADs
;				SET ip_offset to 0
;				CONFIG XFER2VBUS to FIR_COEF_MEM
;				START XFER2VBUSSTART DMA
;				WAIT (DATA_READY(XFER2VBUS)==TRUE)	// wait for XFER2VBUS to finish
;			FIR_CYCLE_START:
;				LOAD (FIR_IP_MEM + ip_offset) //new input
;				SHIFT SPAD BUffer // ip Delay Line shifted
;				INSERT new input to SPAD buffer // update ip Delay Line buffer
;				CLEAR accumulator registers
;				// FIR operation starts
;				// first 24 inputs (SPAD B0)
;				xin_shift = 1 // shift value points to first input in SPAD (for loading from SPAD R0 to R29)
;				LOOP(3){
;						LOAD 8 Coefficients from XFER2VBUS
;						POINT mvid_reg_ptr to R2
;						WHILE(*mvid_reg_ptr <=R9)	// loops 8 times
;						{
;							MOV (*mvid_reg_ptr) to mul_opr_reg_1
;							SHIFT XIN (SPAD_B0)(xin_shift)  to mul_opr_reg_2 // shift load from SPAD
;							MUL ACCUMULATE (fir_result) // multiply and manually accumulate
;							xin_shift ++ // shift value points to next input in SPAD
;						}
;						}
;				// next 24 inputs (SPAD B1)
;				xin_shift = 1 // shift value points to first input in SPAD (for loading from SPAD R0 to R29)
;				LOOP(3){
;						LOAD 8 Coefficients from XFER2VBUS
;						POINT mvid_reg_ptr to R2
;						WHILE(*mvid_reg_ptr <=R9)	// loops 8 times
;						{
;							MOV (*mvid_reg_ptr) to mul_opr_reg_1
;							SHIFT XIN (SPAD_B1)(xin_shift)  to mul_opr_reg_2 // shift load from SPAD
;							MUL ACCUMULATE (fir_result) // multiply and manually accumulate
;							xin_shift ++ // shift value points to next input in SPAD
;						}
;						}
;				// last 16 inputs (SPAD B2)
;				xin_shift = 1 // shift value points to first input in SPAD (for loading from SPAD R0 to R29)
;				LOOP(2){
;						/* Note: loop un-rolled in implementation and conditional statement is absent*/
;						if (LOOP == 2) /* re-trigger xfer2vbus beginning of last loop for next cycle*/
;						{
;							CONFIG XFER2VBUS to FIR_COEF_MEM
;							START XFER2VBUSSTART DMA
;						}
;						LOAD 8 Coefficients from XFER2VBUS
;						POINT mvid_reg_ptr to R2
;						WHILE(*mvid_reg_ptr <=R9)	// loops 8 times
;						{
;							MOV (*mvid_reg_ptr) to mul_opr_reg_1
;							SHIFT XIN (SPAD_B2)(xin_shift)  to mul_opr_reg_2 // shift load from SPAD
;							MUL ACCUMULATE (fir_result) // multiply and manually accumulate
;							xin_shift ++ // shift value points to next input in SPAD
;						}
;						CONFIG XFER2VBUS to FIR_COEF_MEM
;						START XFER2VBUSSTART DMA
;						}
;				SCALE fir_result to 32 bit
;				WRITE (fir_result) to (FIR_oP_MEM + ip_offset)
;				ip_offset ++ // point to next input in memory
;				IF (ip_offset>FIR_DATA_SIZE)
;						{ GOTO FIR_CYCLE_START}
;
;	Cycle budget: INIT(one time) : 12 cycles + time for Xfer2vbus data ready
;				  FIR_CYCLE(worst case) : 872 cycles (for each new input)
;   Registers modified: R0 - R29
;*******************************************
||FN_FIR_64||:
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

	;*** configure xfer2vbus first time (reconfigured at the end of every FIR loop)***
    ldi r18, 0x5                            ; config: auto read mode on, read size : 32 Bytes
    ldi32 r19, FIR_COEF_MEM                 ; start address of window coefficients
    xout XFR2VBUSP_RD0_XID, &r18, 8         ; set configuration and updated address
    xin XFR2VBUSP_RD0_XID, &r2, 32          ; drain fifo to clear already loaded values

    ; wait till coefficient data is loaded
	; (non-deterministic wait only in the beginning)
WAIT_DATA_READY:
    xin     XFR2VBUSP_RD0_XID, &r18, 1
    qbbs    WAIT_DATA_READY, r18.b0, 3

FIR_CYCLE_START:
********* FIR cycle (for every new input) ******************

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
	xout PRU_SPAD_B2_XID, &R5, (16*4)	; save inputs shifted

	; clear accumulator registers
	and mul_acc_low, mul_acc_low, 0
	and mul_acc_upp, mul_acc_upp, 0

;*************
;* 	x64MAC	 *
;*************
	ldi xin_shift, 1 		; point to first input in SPAD R0 (for loading to r29)
; *** perform multiplications for all inputs in SPAD_B0 ***
	loop FIR_MAC24_B0, 3			; loop over input values in SPAD_B0, 3 x 8
	; no data-ready check needed except in the first load
	; timing requirements are met for configured clock
	xin XFR2VBUSP_RD0_XID, &r2, 32 	; load 8 coefficient values
  	ldi mvid_reg_ptr, 8				; point to first coefficient in R2
FIR_MAC8_B0: 
; (max cycles per FIR_MAC8_B0 loop : 12)
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
; (max cycles per FIR_MAC8_B1 loop : 12)
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
; *** perform multiplications for all inputs in SPAD_B2 (loop unrolled * 2)***
    ;*** unrolled loop 1 ***
	xin XFR2VBUSP_RD0_XID, &r2, 32	; load 8 coefficient values
  	ldi mvid_reg_ptr, 8				; point to first coefficient in R2
FIR_MAC8_B2_L1:
; (max cycles per FIR_MAC8_B2_L1 loop : 12)
  	xin PRU_SPAD_B2_XID, &mul_opr_reg_2, 4	; load input as operand 2
  	mvid mul_opr_reg_1, *mvid_reg_ptr++ 	; load coefficient as operand 1

	; *** multiply and accumulate ***
	nop
	xin MAC_XID , &mul_prd_reg_low, 8	; get product lower and upper 32 bits
	; apply sign correction
	qbbc    FIR_SGNCHK_CMPL_B2_L1_1, mul_opr_reg_1, 31   ; sign check for op 1
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2
FIR_SGNCHK_CMPL_B2_L1_1:
    qbbc    FIR_SGNCHK_CMPL_B2_L1_2, mul_opr_reg_2, 31   ; sign check for op2
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_1
FIR_SGNCHK_CMPL_B2_L1_2:
	; accumulate as q62 over two registers
	add mul_acc_low, mul_acc_low, mul_prd_reg_low   ; (carry set automatically)
	adc mul_acc_upp, mul_acc_upp, mul_prd_reg_upp  	; (carry from lower ACC added to upper part)
  	; *** multiply and accumulate ends ***

	add xin_shift, xin_shift, 1			    ; point to next input
	qbge FIR_MAC8_B2_L1, mvid_reg_ptr, 36	; loop till 8 coefficient values are covered

	;*** unrolled loop 2 ***

	; reconfigure and trigger XVER2VBUS for next FIR cycle
    ldi r18, 0x5                            ; config: auto read mode on, read size: 32 Bytes
    ldi32 r19, FIR_COEF_MEM                 ; start address of window coefficients
    xout XFR2VBUSP_RD0_XID, &r18, 8         ; set configuration and updated address

    xin XFR2VBUSP_RD0_XID, &r2, 32  ; load last set of 8 coefficient values in buffer
	; Note: last step drains FIFO for next set of transfer we triggered
  	ldi mvid_reg_ptr, 8				; point to first coefficient in R2
FIR_MAC8_B2_L2:
; (max cycles per FIR_MAC8_B2_L2 loop : 12)
  	xin PRU_SPAD_B2_XID, &mul_opr_reg_2, 4	; load input as operand 2
  	mvid mul_opr_reg_1, *mvid_reg_ptr++ 	; load coefficient as operand 1

	; *** multiply and accumulate ***
	nop
	xin MAC_XID , &mul_prd_reg_low, 8	; get product lower and upper 32 bits
	; apply sign correction
	qbbc    FIR_SGNCHK_CMPL_B2_L2_1, mul_opr_reg_1, 31   ; sign check for op 1
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2
FIR_SGNCHK_CMPL_B2_L2_1:
    qbbc    FIR_SGNCHK_CMPL_B2_L2_2, mul_opr_reg_2, 31   ; sign check for op2
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_1
FIR_SGNCHK_CMPL_B2_L2_2:
	; accumulate as q62 over two registers
	add mul_acc_low, mul_acc_low, mul_prd_reg_low   ; (carry set automatically)
	adc mul_acc_upp, mul_acc_upp, mul_prd_reg_upp  	; (carry from lower ACC added to upper part)
  	; *** multiply and accumulate ends ***

	add xin_shift, xin_shift, 1			    ; point to next input
	qbge FIR_MAC8_B2_L2, mvid_reg_ptr, 36	; loop till 8 coefficient values are covered
;FIR_MAC24_B2:	; 3x8 loop ends

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
	; fetch saved PC
	ldi xin_shift, 0				; shift value reset to 0
	xin PRU_SPAD_B2_XID, &r29.w2, 2
	jmp r29.w2						; return to saved PC

;*****************************************************************************
;                                     END
;*****************************************************************************
