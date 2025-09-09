/*
 * Copyright (C) 2022 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *	* Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *
 *	* Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the
 *	  distribution.
 *
 *	* Neither the name of Texas Instruments Incorporated nor the names of
 *	  its contributors may be used to endorse or promote products derived
 *	  from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_EPWM_H_
#define _SYS_EPWM_H_

/* SYS EPWM register set */
typedef struct {

	/* SYS_EPWM_TBCTL register bit field */
	volatile uint16_t TBCTL; // 0x0

	/* SYS_EPWM_TBSTS register bit field */
	volatile uint16_t TBSTS; // 0x2

	/* SYS_EPWM_TBPHSHR register bit field */
	volatile uint16_t TBPHSHR; // 0x4

	/* SYS_EPWM_TBPHS register bit field */
	volatile uint16_t TBPHS; // 0x6

	/* SYS_EPWM_TBCNT register bit field */
	volatile uint16_t TBCNT; // 0x8

	/* SYS_EPWM_TBPRD register bit field */
	volatile uint16_t TBPRD; // 0xa

	uint8_t rsvdc[2]; // 0xc - 0xd

	/* SYS_EPWM_CMPCTL register bit field */
	volatile uint16_t CMPCTL; // 0xe

	/* SYS_EPWM_CMPAHR register bit field */
	volatile uint16_t CMPAHR; // 0x10

	/* SYS_EPWM_CMPA register bit field */
	volatile uint16_t CMPA; // 0x12

	/* SYS_EPWM_CMPB register bit field */
	volatile uint16_t CMPB; // 0x14

	/* SYS_EPWM_AQCTLA register bit field */
	volatile uint16_t AQCTLA; // 0x16

	/* SYS_EPWM_AQCTLB register bit field */
	volatile uint16_t AQCTLB; // 0x18

	/* SYS_EPWM_AQSFRC register bit field */
	volatile uint16_t AQSFRC; // 0x1a

	/* SYS_EPWM_AQCSFRC register bit field */
	volatile uint16_t AQCSFRC; // 0x1c

	/* SYS_EPWM_DBCTL register bit field */
	volatile uint16_t DBCTL; // 0x1e

	/* SYS_EPWM_DBRED register bit field */
	volatile uint16_t DBRED; // 0x20

	/* SYS_EPWM_DBFED register bit field */
	volatile uint16_t DBFED; // 0x22

	/* SYS_EPWM_TZSEL register bit field */
	volatile uint16_t TZSEL; // 0x24

	uint8_t rsvd26[2]; // 0x26 - 0x27

	/* SYS_EPWM_TZCTL register bit field */
	volatile uint16_t TZCTL; // 0x28

	/* SYS_EPWM_TZEINT register bit field */
	volatile uint16_t TZEINT; // 0x2a

	/* SYS_EPWM_TZFLG register bit field */
	volatile uint16_t TZFLG; // 0x2c

	/* SYS_EPWM_TZCLR register bit field */
	volatile uint16_t TZCLR; // 0x2e

	/* SYS_EPWM_TZFRC register bit field */
	volatile uint16_t TZFRC; // 0x30

	/* SYS_EPWM_ETSEL register bit field */
	volatile uint16_t ETSEL; // 0x32

	/* SYS_EPWM_ETPS register bit field */
	volatile uint16_t ETPS; // 0x34

	/* SYS_EPWM_ETFLG register bit field */
	volatile uint16_t ETFLG; // 0x36

	/* SYS_EPWM_ETCLR register bit field */
	volatile uint16_t ETCLR; // 0x38

	/* SYS_EPWM_ETFRC register bit field */
	volatile uint16_t ETFRC; // 0x3a

	/* SYS_EPWM_PCCTL register bit field */
	volatile uint16_t PCCTL; // 0x3c

	uint8_t rsvd3e[30]; // 0x3e - 0x5b

	/* SYS_EPWM_PID register bit field */
	union {
		volatile uint32_t PID;

		volatile struct {
			uint32_t Y_MINOR : 6; // 5:0
			uint32_t CUSTOM : 2; // 7:6
			uint32_t X_MAJOR : 3; // 10:8
			uint32_t R_RTL : 5; // 15:11
			uint32_t FUNC : 12; // 27:16
			uint32_t rsvd28 : 2; // 29:28
			uint32_t SCHEME : 2; // 31:30
		} PID_bit;
	}; // 0x5c

} sys_epwm;

#define EPWM0 (*((volatile sys_epwm*)0x23000000))
#define EPWM1 (*((volatile sys_epwm*)0x23010000))
#define EPWM2 (*((volatile sys_epwm*)0x23020000))

#endif /* _SYS_EPWM_H_ */
