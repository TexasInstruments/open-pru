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


	.sect	".text:logic_scope"
	.clink
	.global	||FN_LOGIC_SCOPE_DUAL_CORE||

;==========================================================================;
;                             Include Files                                ;
;==========================================================================;
	.include "pru_io/firmware/common/icss_xfer_defines.inc"
	.include "DCLS_config_macros.inc"
;*******************************************
;   Function : FN_LOGIC_SCOPE_DUAL_CORE
;
;   Description:
;   			Collects continuous samples over two cores using R30 registers
;	Pre-requisites:
;				1. Function should run on PRU0 and RTU0
;				2. Function in RTU0 should start before the one in PRU0
;				3. configuration constants are set to valid values in DCLS_config_macros.inc
;   Pseudocode:
;			//configure constants as set in DCLS_config_macros.inc
;				Rx --> R2.b0	//point to first byte of R2
;			//synchronization of both cores
;			//wait for low to high trigger
;			#ifdef PRU0
;			{
;			START_SAMPLING:
;				WHILE (100)	//collect 100 samples
;					{
;						MOV *Rx.b0++, R30.bx	// bx configured as CHANNEL BYTE
;						// Register pointer updated in the last instruction
;			 		}
;				WRITE (&R2, MEM_POINTER, 100)	// Save 100 samples collected that starts from R2
;				MEM_POINTER += 200	// update pointer for next cycle
;				Rx --> R2.b0	//point back to first byte of R2
;				WAIT(X)	// wait precisely the remaining time till other core finishes sampling
;				CORE_SAMPLE_COUNT_1--;	//update sample count
;				if (CORE_SAMPLE_COUNT_1 != 0) GOTO START_SAMPLING
;			}
;			#ifdef RTU0
;			{
;			CYCLE_CORE_1:
;				WAIT(Y) //wait for other core to finish sampling
;				if (CORE_SAMPLE_COUNT_2 == 0) GOTO END_SAMPLING
;				WHILE (100)	//collect 100 samples
;					{
;						MOV *Rx.b0++, R30.bx	// bx configured as CHANNEL BYTE
;						// Register pointer updated in the last instruction
;			 		}
;				WRITE (&R2, MEM_POINTER, 100)	// Save 100 samples collected that starts from R2
;				MEM_POINTER += 200	// update pointer for next cycle
;				Rx --> R2.b0	//point back to first byte of R2
;				CORE_SAMPLE_COUNT_2--;	//update sample count
;				GOTO CYCLE_CORE_1
;			END_SAMPLING:
;			}
;	Cycle budget: Config + Synchronization Core0(PRU0) : 15 or 16 cycles
;				 				  		   Core1(RTU0) : (15 or 16) cycles plus PRU0 start delay

;				  Logic Scope : 1+(100*(N+1)) cycles, N: number of samples in multiples of 100 (n=5 => 500 samples)
;
;*******************************************
||FN_LOGIC_SCOPE_DUAL_CORE||:
	;***********************
	;**	Pre-configuration **
	;***********************
	ldi  r0.b0, 100				; fixed 100 samples sampled by one core at a time
	ldi  r1.b0, 8				; sampling starts from r2
	ldi32 r29, DCLS_SMP_MEM_GLB	; set sample storage mem address
	.if (DCLS_CORE == 0) 		; PRU0
	ldi  r28, MEM_OFFSET_CORE0 	; set memory offset start
	ldi  r27, DCLS_SMP_CNT_C0 	; set initial sample size
	.elseif (DCLS_CORE == 1) 	; RTU0
	ldi  r28, MEM_OFFSET_CORE1 	; set memory offset start
	ldi  r27, DCLS_SMP_CNT_C1 	; set initial sample size
	.endif

	;*********************
	;**	Synchronization **
	;*********************
	; PRU - RTU sync synchronization before waiting for trigger
	; Uses IPC SPAD r2.b0 and r2.b1
	; (!!only works when RTU is started before PRU!!)
	.if (DCLS_CORE == 0) ; PRU0
	;load sync bit
	ldi r2.b0, 1
	ldi r2.b1, 0
	; set trigger and poll for response
	xout	PRU_RTU_SPAD_XID, &r2.b0, 1
    nop
    nop
    nop
    xin		PRU_RTU_SPAD_XID, &r2.b1, 1
    qbbs	DCLS_SYNC_CASE_1, r2.b1, 0
    xin		PRU_RTU_SPAD_XID, &r2.b1, 1
    qbbs	DCLS_SYNC_CASE_2, r2.b1, 0
    ; none occurred
    halt
DCLS_SYNC_CASE_1: ; PRU1 sees trigger early
    nop
    qba		DCLS_SYNC_CMP	   ; sync complete
DCLS_SYNC_CASE_2: ; PRU1 sees trigger late
    ; send one cycle earlier as it took
    ; PRU1 longer to detect.
    qba	DCLS_SYNC_CMP	   ; sync complete

	.elseif (DCLS_CORE == 1) ; RTU0
; requires to RTU0 to start before PRU0
	; clear SPAD r2.b0 and r2.b1
	ldi r2.w0, 0
	xout   PRU_RTU_SPAD_XID, &r2.b0, 2
	;load sync bit
	ldi r2.b1, 1
DCLS_SYNC_WAIT_FOR_TRIGGER:
    xin    PRU_RTU_SPAD_XID, &r2.b0, 1
    qbne   DCLS_SYNC_WAIT_FOR_TRIGGER, r2.b0, 1
    xout   PRU_RTU_SPAD_XID, &r2.b1, 1
    nop
    nop
    nop
    qba	DCLS_SYNC_CMP	   ; sync complete
	.endif

DCLS_SYNC_CMP:
;*** End Sync ***

	;******************
	;**	Logic Scope  **
	;******************
	; both cores wait for a low to high trigger
	wbc  r31, DCLS_TRIG_CH
	wbs  r31, DCLS_TRIG_CH

	;**************
	;**	 Core 0  **
	;**************
	.if (DCLS_CORE == 0)
DCLS_START_SAMPLING:
	; core 0 starts sampling Byte M() after trigger
	loop DCLS_SMP_LOOP_0, r0.b0
	m_sample_channel_byte
DCLS_SMP_LOOP_0:
	; transfer 100 samples to memory
	sbbo &r2, r29, r28, 100
	; update offset for storage
	add r28, r28, 200
	; point back to r2 for next cycle
	ldi  r1.b0, 8
	; wait till the other core finishes sampling
	nopx DCLS_SMP_WT_TIME
	; update the remaining sample count
	sub r27, r27, 1
	; check sample counter and start a new cycle of samples if needed
	qble DCLS_START_SAMPLING, r27, 1

	;**************
	;**	 Core 1  **
	;**************
	; (first loop unrolled to manage wait times)
	.elseif (DCLS_CORE == 1)
	; wait till first core finishes sampling
	nopx (100 - DCLS_SMP_WT_TIME) ; additional wait time for the first cycle
DCLS_CYCLE_CORE_1:
	nopx (DCLS_SMP_WT_TIME-1)	; wait time for normal cycles
	; end sampling if no count remains
	qbge DCLS_END_SAMPLING, r27, 0
	; core 1 starts sampling
	loop DCLS_SMP_LOOP_1, r0.b0
	m_sample_channel_byte
DCLS_SMP_LOOP_1:
	; transfer 100 samples to memory
	sbbo &r2, r29, r28, 100
	; update offset for storage
	add r28, r28, 200
	; point back to r2 for next cycle
	ldi  r1.b0, 8
	; update the sample count
	sub r27, r27, 1
	; repeat cycle
	qba DCLS_CYCLE_CORE_1
DCLS_END_SAMPLING:
	.endif
	halt

;*****************************************************************************
;                                     END
;*****************************************************************************
