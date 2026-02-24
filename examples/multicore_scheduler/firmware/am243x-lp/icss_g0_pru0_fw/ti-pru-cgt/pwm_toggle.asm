; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2018-2025 Texas Instruments Incorporated - http://www.ti.com/

;************************************************************************************
;   File:     pwm_toggle.asm
;
;   Brief:	  contains Multicore scheduling Task, it's configuration macros and function
;
;************************************************************************************
	.sect	".text:pwm_toggle"
	.clink
	.global	FN_PWM_INIT_TOGGLE_PRU0_TASK
	.global	||FN_MCS_CFG||

; File includes
    .include  "pru_io/firmware/common/icss_regs.inc"
    .include  "pru_io/firmware/common/icss_cfg_regs.inc"
    .include  "pru_io/firmware/common/icss_iep_regs.inc"
    .include  "pru_io/firmware/common/icss_xfer_defines.inc"

	.cdecls C,NOLIST
%{
    #include <drivers/pruicss/g_v0/cslr_icss_g.h>

%}

; Macros

; increment value for configured clock
MS_TIMER_INC	.set	4 ; 250 Mhz, 4ns

; set CMP0 time to desired time period in ns
	; (COUNT - increment value) is set as count starts from 0
MS_CMP0_TIME	.set	(100-MS_TIMER_INC) ; cycle time
MS_CMP1_TIME	.set	2000   ; CMP1 time
MS_CMP2_TIME	.set	(2*MS_TIMER_INC)  ; CMP2 time
MS_CMP3_TIME	.set	(3*MS_TIMER_INC) ; CMP3 time
MS_CMP4_TIME	.set	(4*MS_TIMER_INC) ; CMP4 time
MS_CMP5_TIME	.set	(5*MS_TIMER_INC) ; CMP5 time
MS_CMP6_TIME	.set	(6*MS_TIMER_INC) ; CMP6 time
MS_CMP7_TIME	.set	(1*MS_TIMER_INC)  ; CMP7 time

; set TM base address
TM_REG_BASE_PRU0		.set	CSL_ICSS_G_PR1_TASKS_MGR_PRU0_PR1_TASKS_MGR_PRU0_MMR_REGS_BASE
TM_REG_BASE_PRU1		.set	CSL_ICSS_G_PR1_TASKS_MGR_PRU1_PR1_TASKS_MGR_PRU1_MMR_REGS_BASE
TM_REG_BASE_RTU0		.set	CSL_ICSS_G_PR1_TASKS_MGR_RTU0_PR1_TASKS_MGR_RTU0_MMR_REGS_BASE
TM_REG_BASE_RTU1		.set	CSL_ICSS_G_PR1_TASKS_MGR_RTU1_PR1_TASKS_MGR_RTU1_MMR_REGS_BASE
TM_REG_BASE_TXPRU0		.set	CSL_ICSS_G_PR1_TASKS_MGR_PRU_TX0_PR1_TASKS_MGR_PRU_TX0_MMR_REGS_BASE
TM_REG_BASE_TXPRU1		.set	CSL_ICSS_G_PR1_TASKS_MGR_PRU_TX1_PR1_TASKS_MGR_PRU_TX1_MMR_REGS_BASE

TASKS_MGR_GLOBAL_CFG		.set 0x00
TASKS_MGR_TS2_PC_S0			.set 0x1C
TASKS_MGR_TS2_GEN_CFG1		.set 0x40

;relevant event mappings
IEP0_CMP_0	.set	(16+0)
IEP0_CMP_1	.set	(16+1)
IEP0_CMP_2	.set	(16+2)
IEP0_CMP_3	.set	(16+3)
IEP0_CMP_4	.set	(16+4)
IEP0_CMP_5	.set	(16+5)
IEP0_CMP_6	.set	(16+6)
IEP0_CMP_7	.set	(16+7)

TRG_EVENT_PRU0				.set IEP0_CMP_7
TRG_EVENT_PRU1				.set IEP0_CMP_2
TRG_EVENT_RTU1				.set IEP0_CMP_3
TRG_EVENT_RTU0				.set IEP0_CMP_4
TRG_EVENT_TXPRU1			.set IEP0_CMP_5
TRG_EVENT_TXPRU0			.set IEP0_CMP_6

; Offset for used PWM register
ICSSG_PWM2	.set	0x138

; toggling value for pwm pin (toggling sets POS_INIT and NEG_INT to 0 or 1)
; rx.b2 --> Set PIN high
; rx.b3 --> Set PIN low
TOGGLE_VAL	.set	0x050a8000


;*************************************************
;   Function : FN_MCS_CFG
;   Description:
;       One time configurations and pre-loads for Multi-core Scheduling:
;		1. Configures IEP timer and compare register times
;		2. Configures task manager and maps tasks in all cores to events
;		3. Pre-loads r24 with value required for toggling task
;		3. Sets address of PRU0 task
;		4. Force PWM trip mode
;
;*************************************************
.asmfunc
||FN_MCS_CFG||:
	xout PRU_SPAD_B0_XID, &r3.w2, 2	; save return PC to SPAD
;* ----------- iep configuration  ----------------------- *
	; Configure ICSSG_IEPCLK_REG Register to run in sync mode
	; IEP_OCP_CLK_EN = 1 (bit 0 = 1) to set the source of the IEP Clock to ICSSGn_CORE_CLK
	ldi32 	r4, PRUx_CFG_BASE
	ldi 	r5, 0x01
	sbbo 	&r5.b0, r4, ICSS_CFG_IEPCLK, 1
	; ICSSG_SA_MX_REG: set PWM_EFC_EN mode, auto clear cmp pending flags
	sbbo 	&r5.b0, r4, (ICSS_CFG_MUXSEL+2), 1
	; reset iep0 timer
	ldi32 	r4, PRUx_IEP0_BASE
	ldi 	r5, 0x20
	sbbo 	&r5.b0, r4, 0, 1
	ldi 	r5, 0x0
	sbbo 	&r5.b0, r4, ICSS_IEP_COUNT_REG, 1
	; set time period of toggling pattern
	ldi 	r5, MS_CMP0_TIME
	sbbo 	&r5, r4, ICSS_IEP_CMP0_REG, 4
	; set PRU0 trigger time
	ldi 	r5, MS_CMP7_TIME
	sbbo 	&r5, r4, ICSS_IEP_CMP7_REG, 4
	; set PRU1 trigger time
	ldi 	r5, MS_CMP2_TIME
	sbbo 	&r5, r4, ICSS_IEP_CMP2_REG, 4
	; set RTU1 trigger time
	ldi 	r5, MS_CMP3_TIME
	sbbo 	&r5, r4, ICSS_IEP_CMP3_REG, 4
	; set RTU0 trigger time
	ldi 	r5, MS_CMP4_TIME
	sbbo 	&r5, r4, ICSS_IEP_CMP4_REG, 4
	; set TXPRU0 trigger time
	ldi 	r5, MS_CMP5_TIME
	sbbo 	&r5, r4, ICSS_IEP_CMP5_REG, 4
	; set TXPRU1 trigger time
	ldi 	r5, MS_CMP6_TIME
	sbbo 	&r5, r4, ICSS_IEP_CMP6_REG, 4
	; set CMP1 time to a high value(since PWM init toggle is used)
	ldi 	r5, MS_CMP1_TIME
	sbbo 	&r5, r4, ICSS_IEP_CMP1_REG, 4
	; Enable cmp 0,2,3,4,5,6,7 and wrap around
	ldi 	r5, 0x1fb
	sbbo 	&r5, r4, ICSS_IEP_CMP_CFG_REG, 4
	; set default increment and enable count
	; set default increment to 3 for 333 MHz , 4 for 250 MHz
	ldi 	r5, 0x441
	sbbo 	&r5, r4, ICSS_IEP_GLOBAL_CFG_REG, 2
;* ----------- Task Manager configuration  ----------------------- *
	; disable task manager
	tsen 	0
	; clear event from previous debug
	ldi32 	r4, TM_REG_BASE_PRU0
	ldi		r5, 0x0fff
	sbbo 	&r5, r4, TASKS_MGR_GLOBAL_CFG, 4
    xin		TM_YIELD_XID, &r0.b3,1
    nop
    nop
	ldi		r5, 0x0000
	sbbo 	&r5, r4, 0, 4
	; configure task manager general purpose mode = 2 , TS2_S0 task enable (bit 7)
	ldi		r5, 0x0082
	sbbo 	&r5, r4, TASKS_MGR_GLOBAL_CFG, 4
	; set address of task in TS2_0
	ldi     r5.w0, $CODE(FN_PWM_INIT_TOGGLE_PRU0_TASK)
    sbbo    &r5.w0, r4, TASKS_MGR_TS2_PC_S0, 4
    ; set TS2_S0 trigger for all cores, TS2_S0 (bit 7-0)
    ;PRU0
    ldi		r5, TRG_EVENT_PRU0
    sbbo    &r5, r4, TASKS_MGR_TS2_GEN_CFG1, 4
    ;PRU1
    ldi32 	r4, TM_REG_BASE_PRU1
    ldi		r5, TRG_EVENT_PRU1
    sbbo    &r5, r4, TASKS_MGR_TS2_GEN_CFG1, 4
    ;RTU0
    ldi32 	r4, TM_REG_BASE_RTU0
    ldi		r5, TRG_EVENT_RTU0
    sbbo    &r5, r4, TASKS_MGR_TS2_GEN_CFG1, 4
    ;RTU1
    ldi32 	r4, TM_REG_BASE_RTU1
    ldi		r5, TRG_EVENT_RTU1
    sbbo    &r5, r4, TASKS_MGR_TS2_GEN_CFG1, 4
    ;TXPRU0
    ldi32 	r4, TM_REG_BASE_TXPRU0
    ldi		r5, TRG_EVENT_TXPRU0
    sbbo    &r5, r4, TASKS_MGR_TS2_GEN_CFG1, 4
    ;TXPRU1
    ldi32 	r4, TM_REG_BASE_TXPRU1
    ldi		r5, TRG_EVENT_TXPRU1
    sbbo    &r5, r4, TASKS_MGR_TS2_GEN_CFG1, 4

   	; pre-load for toggling
   	ldi32 r24, TOGGLE_VAL
   	; force PWM trip mode
   	ldi32 	r4, PRUx_CFG_BASE
   	ldi32 	r5, 0x0003ff00
   	ldi32 	r6, ICSSG_PWM2
   	sbbo 	&r5, r4, r6, 4
   	ldi32 	r5, 0x0004ff00
   	sbbo 	&r5, r4, r6, 4

    xin PRU_SPAD_B0_XID, &r3.w2, 2 ; restore PC
	jmp		r3.w2
.endasmfunc

;*************************************************
;   Task : FN_PWM_INIT_TOGGLE_PRU0_TASK
;   Description : Sets PWM init to Logic High for configured PIN
;   			  => Configured PIN reads Logic High
;   Assumption : r24 preloaded with toggle value, PWM out is configured
;   Peak Cycles : 4
;
;*************************************************
FN_PWM_INIT_TOGGLE_PRU0_TASK:

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
