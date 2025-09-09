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

#ifndef _PRU_INTC_H_
#define _PRU_INTC_H_

/* PRU INTC register set */
typedef struct {

	/* PRU_INTC_REVISION_REG register bit field */
	union {
		volatile uint32_t REVISION_REG;

		volatile struct {
			uint32_t REV_MINOR : 6; // 5:0
			uint32_t REV_CUSTOM : 2; // 7:6
			uint32_t REV_MAJOR : 3; // 10:8
			uint32_t REV_RTL : 5; // 15:11
			uint32_t REV_MODULE : 12; // 27:16
			uint32_t rsvd28 : 2; // 29:28
			uint32_t REV_SCHEME : 2; // 31:30
		} REVISION_REG_bit;
	}; // 0x0

	/* PRU_INTC_CONTROL_REG register bit field */
	union {
		volatile uint32_t CONTROL_REG;

		volatile struct {
			uint32_t rsvd0 : 1; // 0
			uint32_t WAKEUP_MODE : 1; // 1
			uint32_t NEST_MODE : 2; // 3:2
			uint32_t PRIORITY_HOLD_MODE : 1; // 4
			uint32_t rsvd5 : 27; // 31:5
		} CONTROL_REG_bit;
	}; // 0x4

	uint8_t rsvd8[8]; // 0x8 - 0xf

	/* PRU_INTC_GLOBAL_ENABLE_HINT_REG register bit field */
	union {
		volatile uint32_t GLOBAL_ENABLE_HINT_REG;

		volatile struct {
			uint32_t ENABLE_HINT_ANY : 1; // 0
			uint32_t rsvd1 : 31; // 31:1
		} GLOBAL_ENABLE_HINT_REG_bit;
	}; // 0x10

	uint8_t rsvd14[8]; // 0x14 - 0x1b

	/* PRU_INTC_GLB_NEST_LEVEL_REG register bit field */
	union {
		volatile uint32_t GLB_NEST_LEVEL_REG;

		volatile struct {
			uint32_t GLB_NEST_LEVEL : 9; // 8:0
			uint32_t rsvd9 : 22; // 30:9
			uint32_t GLB_NEST_AUTO_OVR : 1; // 31
		} GLB_NEST_LEVEL_REG_bit;
	}; // 0x1c

	/* PRU_INTC_STATUS_SET_INDEX_REG register bit field */
	union {
		volatile uint32_t STATUS_SET_INDEX_REG;

		volatile struct {
			uint32_t STATUS_SET_INDEX : 10; // 9:0
			uint32_t rsvd10 : 22; // 31:10
		} STATUS_SET_INDEX_REG_bit;
	}; // 0x20

	/* PRU_INTC_STATUS_CLR_INDEX_REG register bit field */
	union {
		volatile uint32_t STATUS_CLR_INDEX_REG;

		volatile struct {
			uint32_t STATUS_CLR_INDEX : 10; // 9:0
			uint32_t rsvd10 : 22; // 31:10
		} STATUS_CLR_INDEX_REG_bit;
	}; // 0x24

	/* PRU_INTC_ENABLE_SET_INDEX_REG register bit field */
	union {
		volatile uint32_t ENABLE_SET_INDEX_REG;

		volatile struct {
			uint32_t ENABLE_SET_INDEX : 10; // 9:0
			uint32_t rsvd10 : 22; // 31:10
		} ENABLE_SET_INDEX_REG_bit;
	}; // 0x28

	/* PRU_INTC_ENABLE_CLR_INDEX_REG register bit field */
	union {
		volatile uint32_t ENABLE_CLR_INDEX_REG;

		volatile struct {
			uint32_t ENABLE_CLR_INDEX : 10; // 9:0
			uint32_t rsvd10 : 22; // 31:10
		} ENABLE_CLR_INDEX_REG_bit;
	}; // 0x2c

	uint8_t rsvd30[4]; // 0x30 - 0x33

	/* PRU_INTC_HINT_ENABLE_SET_INDEX_REG register bit field */
	union {
		volatile uint32_t HINT_ENABLE_SET_INDEX_REG;

		volatile struct {
			uint32_t HINT_ENABLE_SET_INDEX : 10; // 9:0
			uint32_t rsvd10 : 22; // 31:10
		} HINT_ENABLE_SET_INDEX_REG_bit;
	}; // 0x34

	/* PRU_INTC_HINT_ENABLE_CLR_INDEX_REG register bit field */
	union {
		volatile uint32_t HINT_ENABLE_CLR_INDEX_REG;

		volatile struct {
			uint32_t HINT_ENABLE_CLR_INDEX : 10; // 9:0
			uint32_t rsvd10 : 22; // 31:10
		} HINT_ENABLE_CLR_INDEX_REG_bit;
	}; // 0x38

	uint8_t rsvd3c[68]; // 0x3c - 0x7f

	/* PRU_INTC_GLB_PRI_INTR_REG register bit field */
	union {
		volatile uint32_t GLB_PRI_INTR_REG;

		volatile struct {
			uint32_t GLB_PRI_INTR : 10; // 9:0
			uint32_t rsvd10 : 21; // 30:10
			uint32_t GLB_NONE : 1; // 31
		} GLB_PRI_INTR_REG_bit;
	}; // 0x80

	uint8_t rsvd84[380]; // 0x84 - 0x1ff

	/* PRU_INTC_RAW_STATUS_REG0 register bit field */
	union {
		volatile uint32_t RAW_STATUS_REG0;

		volatile struct {
			uint32_t RAW_STATUS_0 : 1; // 0
			uint32_t RAW_STATUS_1 : 1; // 1
			uint32_t RAW_STATUS_2 : 1; // 2
			uint32_t RAW_STATUS_3 : 1; // 3
			uint32_t RAW_STATUS_4 : 1; // 4
			uint32_t RAW_STATUS_5 : 1; // 5
			uint32_t RAW_STATUS_6 : 1; // 6
			uint32_t RAW_STATUS_7 : 1; // 7
			uint32_t RAW_STATUS_8 : 1; // 8
			uint32_t RAW_STATUS_9 : 1; // 9
			uint32_t RAW_STATUS_10 : 1; // 10
			uint32_t RAW_STATUS_11 : 1; // 11
			uint32_t RAW_STATUS_12 : 1; // 12
			uint32_t RAW_STATUS_13 : 1; // 13
			uint32_t RAW_STATUS_14 : 1; // 14
			uint32_t RAW_STATUS_15 : 1; // 15
			uint32_t RAW_STATUS_16 : 1; // 16
			uint32_t RAW_STATUS_17 : 1; // 17
			uint32_t RAW_STATUS_18 : 1; // 18
			uint32_t RAW_STATUS_19 : 1; // 19
			uint32_t RAW_STATUS_20 : 1; // 20
			uint32_t RAW_STATUS_21 : 1; // 21
			uint32_t RAW_STATUS_22 : 1; // 22
			uint32_t RAW_STATUS_23 : 1; // 23
			uint32_t RAW_STATUS_24 : 1; // 24
			uint32_t RAW_STATUS_25 : 1; // 25
			uint32_t RAW_STATUS_26 : 1; // 26
			uint32_t RAW_STATUS_27 : 1; // 27
			uint32_t RAW_STATUS_28 : 1; // 28
			uint32_t RAW_STATUS_29 : 1; // 29
			uint32_t RAW_STATUS_30 : 1; // 30
			uint32_t RAW_STATUS_31 : 1; // 31
		} RAW_STATUS_REG0_bit;
	}; // 0x200

	/* PRU_INTC_RAW_STATUS_REG1 register bit field */
	union {
		volatile uint32_t RAW_STATUS_REG1;

		volatile struct {
			uint32_t RAW_STATUS_32 : 1; // 0
			uint32_t RAW_STATUS_33 : 1; // 1
			uint32_t RAW_STATUS_34 : 1; // 2
			uint32_t RAW_STATUS_35 : 1; // 3
			uint32_t RAW_STATUS_36 : 1; // 4
			uint32_t RAW_STATUS_37 : 1; // 5
			uint32_t RAW_STATUS_38 : 1; // 6
			uint32_t RAW_STATUS_39 : 1; // 7
			uint32_t RAW_STATUS_40 : 1; // 8
			uint32_t RAW_STATUS_41 : 1; // 9
			uint32_t RAW_STATUS_42 : 1; // 10
			uint32_t RAW_STATUS_43 : 1; // 11
			uint32_t RAW_STATUS_44 : 1; // 12
			uint32_t RAW_STATUS_45 : 1; // 13
			uint32_t RAW_STATUS_46 : 1; // 14
			uint32_t RAW_STATUS_47 : 1; // 15
			uint32_t RAW_STATUS_48 : 1; // 16
			uint32_t RAW_STATUS_49 : 1; // 17
			uint32_t RAW_STATUS_50 : 1; // 18
			uint32_t RAW_STATUS_51 : 1; // 19
			uint32_t RAW_STATUS_52 : 1; // 20
			uint32_t RAW_STATUS_53 : 1; // 21
			uint32_t RAW_STATUS_54 : 1; // 22
			uint32_t RAW_STATUS_55 : 1; // 23
			uint32_t RAW_STATUS_56 : 1; // 24
			uint32_t RAW_STATUS_57 : 1; // 25
			uint32_t RAW_STATUS_58 : 1; // 26
			uint32_t RAW_STATUS_59 : 1; // 27
			uint32_t RAW_STATUS_60 : 1; // 28
			uint32_t RAW_STATUS_61 : 1; // 29
			uint32_t RAW_STATUS_62 : 1; // 30
			uint32_t RAW_STATUS_63 : 1; // 31
		} RAW_STATUS_REG1_bit;
	}; // 0x204

	uint8_t rsvd208[120]; // 0x208 - 0x27f

	/* PRU_INTC_ENA_STATUS_REG0 register bit field */
	union {
		volatile uint32_t ENA_STATUS_REG0;

		volatile struct {
			uint32_t ENA_STATUS_0 : 1; // 0
			uint32_t ENA_STATUS_1 : 1; // 1
			uint32_t ENA_STATUS_2 : 1; // 2
			uint32_t ENA_STATUS_3 : 1; // 3
			uint32_t ENA_STATUS_4 : 1; // 4
			uint32_t ENA_STATUS_5 : 1; // 5
			uint32_t ENA_STATUS_6 : 1; // 6
			uint32_t ENA_STATUS_7 : 1; // 7
			uint32_t ENA_STATUS_8 : 1; // 8
			uint32_t ENA_STATUS_9 : 1; // 9
			uint32_t ENA_STATUS_10 : 1; // 10
			uint32_t ENA_STATUS_11 : 1; // 11
			uint32_t ENA_STATUS_12 : 1; // 12
			uint32_t ENA_STATUS_13 : 1; // 13
			uint32_t ENA_STATUS_14 : 1; // 14
			uint32_t ENA_STATUS_15 : 1; // 15
			uint32_t ENA_STATUS_16 : 1; // 16
			uint32_t ENA_STATUS_17 : 1; // 17
			uint32_t ENA_STATUS_18 : 1; // 18
			uint32_t ENA_STATUS_19 : 1; // 19
			uint32_t ENA_STATUS_20 : 1; // 20
			uint32_t ENA_STATUS_21 : 1; // 21
			uint32_t ENA_STATUS_22 : 1; // 22
			uint32_t ENA_STATUS_23 : 1; // 23
			uint32_t ENA_STATUS_24 : 1; // 24
			uint32_t ENA_STATUS_25 : 1; // 25
			uint32_t ENA_STATUS_26 : 1; // 26
			uint32_t ENA_STATUS_27 : 1; // 27
			uint32_t ENA_STATUS_28 : 1; // 28
			uint32_t ENA_STATUS_29 : 1; // 29
			uint32_t ENA_STATUS_30 : 1; // 30
			uint32_t ENA_STATUS_31 : 1; // 31
		} ENA_STATUS_REG0_bit;
	}; // 0x280

	/* PRU_INTC_ENA_STATUS_REG1 register bit field */
	union {
		volatile uint32_t ENA_STATUS_REG1;

		volatile struct {
			uint32_t ENA_STATUS_32 : 1; // 0
			uint32_t ENA_STATUS_33 : 1; // 1
			uint32_t ENA_STATUS_34 : 1; // 2
			uint32_t ENA_STATUS_35 : 1; // 3
			uint32_t ENA_STATUS_36 : 1; // 4
			uint32_t ENA_STATUS_37 : 1; // 5
			uint32_t ENA_STATUS_38 : 1; // 6
			uint32_t ENA_STATUS_39 : 1; // 7
			uint32_t ENA_STATUS_40 : 1; // 8
			uint32_t ENA_STATUS_41 : 1; // 9
			uint32_t ENA_STATUS_42 : 1; // 10
			uint32_t ENA_STATUS_43 : 1; // 11
			uint32_t ENA_STATUS_44 : 1; // 12
			uint32_t ENA_STATUS_45 : 1; // 13
			uint32_t ENA_STATUS_46 : 1; // 14
			uint32_t ENA_STATUS_47 : 1; // 15
			uint32_t ENA_STATUS_48 : 1; // 16
			uint32_t ENA_STATUS_49 : 1; // 17
			uint32_t ENA_STATUS_50 : 1; // 18
			uint32_t ENA_STATUS_51 : 1; // 19
			uint32_t ENA_STATUS_52 : 1; // 20
			uint32_t ENA_STATUS_53 : 1; // 21
			uint32_t ENA_STATUS_54 : 1; // 22
			uint32_t ENA_STATUS_55 : 1; // 23
			uint32_t ENA_STATUS_56 : 1; // 24
			uint32_t ENA_STATUS_57 : 1; // 25
			uint32_t ENA_STATUS_58 : 1; // 26
			uint32_t ENA_STATUS_59 : 1; // 27
			uint32_t ENA_STATUS_60 : 1; // 28
			uint32_t ENA_STATUS_61 : 1; // 29
			uint32_t ENA_STATUS_62 : 1; // 30
			uint32_t ENA_STATUS_63 : 1; // 31
		} ENA_STATUS_REG1_bit;
	}; // 0x284

	uint8_t rsvd288[120]; // 0x288 - 0x2ff

	/* PRU_INTC_ENABLE_REG0 register bit field */
	union {
		volatile uint32_t ENABLE_REG0;

		volatile struct {
			uint32_t ENABLE_0 : 1; // 0
			uint32_t ENABLE_1 : 1; // 1
			uint32_t ENABLE_2 : 1; // 2
			uint32_t ENABLE_3 : 1; // 3
			uint32_t ENABLE_4 : 1; // 4
			uint32_t ENABLE_5 : 1; // 5
			uint32_t ENABLE_6 : 1; // 6
			uint32_t ENABLE_7 : 1; // 7
			uint32_t ENABLE_8 : 1; // 8
			uint32_t ENABLE_9 : 1; // 9
			uint32_t ENABLE_10 : 1; // 10
			uint32_t ENABLE_11 : 1; // 11
			uint32_t ENABLE_12 : 1; // 12
			uint32_t ENABLE_13 : 1; // 13
			uint32_t ENABLE_14 : 1; // 14
			uint32_t ENABLE_15 : 1; // 15
			uint32_t ENABLE_16 : 1; // 16
			uint32_t ENABLE_17 : 1; // 17
			uint32_t ENABLE_18 : 1; // 18
			uint32_t ENABLE_19 : 1; // 19
			uint32_t ENABLE_20 : 1; // 20
			uint32_t ENABLE_21 : 1; // 21
			uint32_t ENABLE_22 : 1; // 22
			uint32_t ENABLE_23 : 1; // 23
			uint32_t ENABLE_24 : 1; // 24
			uint32_t ENABLE_25 : 1; // 25
			uint32_t ENABLE_26 : 1; // 26
			uint32_t ENABLE_27 : 1; // 27
			uint32_t ENABLE_28 : 1; // 28
			uint32_t ENABLE_29 : 1; // 29
			uint32_t ENABLE_30 : 1; // 30
			uint32_t ENABLE_31 : 1; // 31
		} ENABLE_REG0_bit;
	}; // 0x300

	/* PRU_INTC_ENABLE_REG1 register bit field */
	union {
		volatile uint32_t ENABLE_REG1;

		volatile struct {
			uint32_t ENABLE_32 : 1; // 0
			uint32_t ENABLE_33 : 1; // 1
			uint32_t ENABLE_34 : 1; // 2
			uint32_t ENABLE_35 : 1; // 3
			uint32_t ENABLE_36 : 1; // 4
			uint32_t ENABLE_37 : 1; // 5
			uint32_t ENABLE_38 : 1; // 6
			uint32_t ENABLE_39 : 1; // 7
			uint32_t ENABLE_40 : 1; // 8
			uint32_t ENABLE_41 : 1; // 9
			uint32_t ENABLE_42 : 1; // 10
			uint32_t ENABLE_43 : 1; // 11
			uint32_t ENABLE_44 : 1; // 12
			uint32_t ENABLE_45 : 1; // 13
			uint32_t ENABLE_46 : 1; // 14
			uint32_t ENABLE_47 : 1; // 15
			uint32_t ENABLE_48 : 1; // 16
			uint32_t ENABLE_49 : 1; // 17
			uint32_t ENABLE_50 : 1; // 18
			uint32_t ENABLE_51 : 1; // 19
			uint32_t ENABLE_52 : 1; // 20
			uint32_t ENABLE_53 : 1; // 21
			uint32_t ENABLE_54 : 1; // 22
			uint32_t ENABLE_55 : 1; // 23
			uint32_t ENABLE_56 : 1; // 24
			uint32_t ENABLE_57 : 1; // 25
			uint32_t ENABLE_58 : 1; // 26
			uint32_t ENABLE_59 : 1; // 27
			uint32_t ENABLE_60 : 1; // 28
			uint32_t ENABLE_61 : 1; // 29
			uint32_t ENABLE_62 : 1; // 30
			uint32_t ENABLE_63 : 1; // 31
		} ENABLE_REG1_bit;
	}; // 0x304

	uint8_t rsvd308[120]; // 0x308 - 0x37f

	/* PRU_INTC_ENABLE_CLR_REG0 register bit field */
	union {
		volatile uint32_t ENABLE_CLR_REG0;

		volatile struct {
			uint32_t ENABLE_0_CLR : 1; // 0
			uint32_t ENABLE_1_CLR : 1; // 1
			uint32_t ENABLE_2_CLR : 1; // 2
			uint32_t ENABLE_3_CLR : 1; // 3
			uint32_t ENABLE_4_CLR : 1; // 4
			uint32_t ENABLE_5_CLR : 1; // 5
			uint32_t ENABLE_6_CLR : 1; // 6
			uint32_t ENABLE_7_CLR : 1; // 7
			uint32_t ENABLE_8_CLR : 1; // 8
			uint32_t ENABLE_9_CLR : 1; // 9
			uint32_t ENABLE_10_CLR : 1; // 10
			uint32_t ENABLE_11_CLR : 1; // 11
			uint32_t ENABLE_12_CLR : 1; // 12
			uint32_t ENABLE_13_CLR : 1; // 13
			uint32_t ENABLE_14_CLR : 1; // 14
			uint32_t ENABLE_15_CLR : 1; // 15
			uint32_t ENABLE_16_CLR : 1; // 16
			uint32_t ENABLE_17_CLR : 1; // 17
			uint32_t ENABLE_18_CLR : 1; // 18
			uint32_t ENABLE_19_CLR : 1; // 19
			uint32_t ENABLE_20_CLR : 1; // 20
			uint32_t ENABLE_21_CLR : 1; // 21
			uint32_t ENABLE_22_CLR : 1; // 22
			uint32_t ENABLE_23_CLR : 1; // 23
			uint32_t ENABLE_24_CLR : 1; // 24
			uint32_t ENABLE_25_CLR : 1; // 25
			uint32_t ENABLE_26_CLR : 1; // 26
			uint32_t ENABLE_27_CLR : 1; // 27
			uint32_t ENABLE_28_CLR : 1; // 28
			uint32_t ENABLE_29_CLR : 1; // 29
			uint32_t ENABLE_30_CLR : 1; // 30
			uint32_t ENABLE_31_CLR : 1; // 31
		} ENABLE_CLR_REG0_bit;
	}; // 0x380

	/* PRU_INTC_ENABLE_CLR_REG1 register bit field */
	union {
		volatile uint32_t ENABLE_CLR_REG1;

		volatile struct {
			uint32_t ENABLE_32_CLR : 1; // 0
			uint32_t ENABLE_33_CLR : 1; // 1
			uint32_t ENABLE_34_CLR : 1; // 2
			uint32_t ENABLE_35_CLR : 1; // 3
			uint32_t ENABLE_36_CLR : 1; // 4
			uint32_t ENABLE_37_CLR : 1; // 5
			uint32_t ENABLE_38_CLR : 1; // 6
			uint32_t ENABLE_39_CLR : 1; // 7
			uint32_t ENABLE_40_CLR : 1; // 8
			uint32_t ENABLE_41_CLR : 1; // 9
			uint32_t ENABLE_42_CLR : 1; // 10
			uint32_t ENABLE_43_CLR : 1; // 11
			uint32_t ENABLE_44_CLR : 1; // 12
			uint32_t ENABLE_45_CLR : 1; // 13
			uint32_t ENABLE_46_CLR : 1; // 14
			uint32_t ENABLE_47_CLR : 1; // 15
			uint32_t ENABLE_48_CLR : 1; // 16
			uint32_t ENABLE_49_CLR : 1; // 17
			uint32_t ENABLE_50_CLR : 1; // 18
			uint32_t ENABLE_51_CLR : 1; // 19
			uint32_t ENABLE_52_CLR : 1; // 20
			uint32_t ENABLE_53_CLR : 1; // 21
			uint32_t ENABLE_54_CLR : 1; // 22
			uint32_t ENABLE_55_CLR : 1; // 23
			uint32_t ENABLE_56_CLR : 1; // 24
			uint32_t ENABLE_57_CLR : 1; // 25
			uint32_t ENABLE_58_CLR : 1; // 26
			uint32_t ENABLE_59_CLR : 1; // 27
			uint32_t ENABLE_60_CLR : 1; // 28
			uint32_t ENABLE_61_CLR : 1; // 29
			uint32_t ENABLE_62_CLR : 1; // 30
			uint32_t ENABLE_63_CLR : 1; // 31
		} ENABLE_CLR_REG1_bit;
	}; // 0x384

	uint8_t rsvd388[120]; // 0x388 - 0x3ff

	/* PRU_INTC_CH_MAP_REG0 register bit field */
	union {
		volatile uint32_t CH_MAP_REG0;

		volatile struct {
			uint32_t CH_MAP_0 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_1 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_2 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_3 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG0_bit;
	}; // 0x400

	/* PRU_INTC_CH_MAP_REG1 register bit field */
	union {
		volatile uint32_t CH_MAP_REG1;

		volatile struct {
			uint32_t CH_MAP_4 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_5 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_6 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_7 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG1_bit;
	}; // 0x404

	/* PRU_INTC_CH_MAP_REG2 register bit field */
	union {
		volatile uint32_t CH_MAP_REG2;

		volatile struct {
			uint32_t CH_MAP_8 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_9 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_10 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_11 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG2_bit;
	}; // 0x408

	/* PRU_INTC_CH_MAP_REG3 register bit field */
	union {
		volatile uint32_t CH_MAP_REG3;

		volatile struct {
			uint32_t CH_MAP_12 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_13 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_14 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_15 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG3_bit;
	}; // 0x40c

	/* PRU_INTC_CH_MAP_REG4 register bit field */
	union {
		volatile uint32_t CH_MAP_REG4;

		volatile struct {
			uint32_t CH_MAP_16 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_17 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_18 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_19 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG4_bit;
	}; // 0x410

	/* PRU_INTC_CH_MAP_REG5 register bit field */
	union {
		volatile uint32_t CH_MAP_REG5;

		volatile struct {
			uint32_t CH_MAP_20 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_21 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_22 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_23 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG5_bit;
	}; // 0x414

	/* PRU_INTC_CH_MAP_REG6 register bit field */
	union {
		volatile uint32_t CH_MAP_REG6;

		volatile struct {
			uint32_t CH_MAP_24 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_25 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_26 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_27 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG6_bit;
	}; // 0x418

	/* PRU_INTC_CH_MAP_REG7 register bit field */
	union {
		volatile uint32_t CH_MAP_REG7;

		volatile struct {
			uint32_t CH_MAP_28 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_29 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_30 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_31 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG7_bit;
	}; // 0x41c

	/* PRU_INTC_CH_MAP_REG8 register bit field */
	union {
		volatile uint32_t CH_MAP_REG8;

		volatile struct {
			uint32_t CH_MAP_32 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_33 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_34 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_35 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG8_bit;
	}; // 0x420

	/* PRU_INTC_CH_MAP_REG9 register bit field */
	union {
		volatile uint32_t CH_MAP_REG9;

		volatile struct {
			uint32_t CH_MAP_36 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_37 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_38 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_39 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG9_bit;
	}; // 0x424

	/* PRU_INTC_CH_MAP_REG10 register bit field */
	union {
		volatile uint32_t CH_MAP_REG10;

		volatile struct {
			uint32_t CH_MAP_40 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_41 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_42 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_43 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG10_bit;
	}; // 0x428

	/* PRU_INTC_CH_MAP_REG11 register bit field */
	union {
		volatile uint32_t CH_MAP_REG11;

		volatile struct {
			uint32_t CH_MAP_44 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_45 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_46 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_47 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG11_bit;
	}; // 0x42c

	/* PRU_INTC_CH_MAP_REG12 register bit field */
	union {
		volatile uint32_t CH_MAP_REG12;

		volatile struct {
			uint32_t CH_MAP_48 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_49 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_50 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_51 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG12_bit;
	}; // 0x430

	/* PRU_INTC_CH_MAP_REG13 register bit field */
	union {
		volatile uint32_t CH_MAP_REG13;

		volatile struct {
			uint32_t CH_MAP_52 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_53 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_54 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_55 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG13_bit;
	}; // 0x434

	/* PRU_INTC_CH_MAP_REG14 register bit field */
	union {
		volatile uint32_t CH_MAP_REG14;

		volatile struct {
			uint32_t CH_MAP_56 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_57 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_58 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_59 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG14_bit;
	}; // 0x438

	/* PRU_INTC_CH_MAP_REG15 register bit field */
	union {
		volatile uint32_t CH_MAP_REG15;

		volatile struct {
			uint32_t CH_MAP_60 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t CH_MAP_61 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t CH_MAP_62 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t CH_MAP_63 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} CH_MAP_REG15_bit;
	}; // 0x43c

	uint8_t rsvd440[960]; // 0x440 - 0x7ff

	/* PRU_INTC_HINT_MAP_REG0 register bit field */
	union {
		volatile uint32_t HINT_MAP_REG0;

		volatile struct {
			uint32_t HINT_MAP_0 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t HINT_MAP_1 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t HINT_MAP_2 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t HINT_MAP_3 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} HINT_MAP_REG0_bit;
	}; // 0x800

	/* PRU_INTC_HINT_MAP_REG1 register bit field */
	union {
		volatile uint32_t HINT_MAP_REG1;

		volatile struct {
			uint32_t HINT_MAP_4 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t HINT_MAP_5 : 4; // 11:8
			uint32_t rsvd12 : 4; // 15:12
			uint32_t HINT_MAP_6 : 4; // 19:16
			uint32_t rsvd20 : 4; // 23:20
			uint32_t HINT_MAP_7 : 4; // 27:24
			uint32_t rsvd28 : 4; // 31:28
		} HINT_MAP_REG1_bit;
	}; // 0x804

	/* PRU_INTC_HINT_MAP_REG2 register bit field */
	union {
		volatile uint32_t HINT_MAP_REG2;

		volatile struct {
			uint32_t HINT_MAP_8 : 4; // 3:0
			uint32_t rsvd4 : 4; // 7:4
			uint32_t HINT_MAP_9 : 4; // 11:8
			uint32_t rsvd12 : 20; // 31:12
		} HINT_MAP_REG2_bit;
	}; // 0x808

	uint8_t rsvd80c[244]; // 0x80c - 0x8ff

	/* PRU_INTC_PRI_HINT_REG0 register bit field */
	union {
		volatile uint32_t PRI_HINT_REG0;

		volatile struct {
			uint32_t PRI_HINT_0 : 10; // 9:0
			uint32_t rsvd10 : 21; // 30:10
			uint32_t NONE_HINT_0 : 1; // 31
		} PRI_HINT_REG0_bit;
	}; // 0x900

	/* PRU_INTC_PRI_HINT_REG1 register bit field */
	union {
		volatile uint32_t PRI_HINT_REG1;

		volatile struct {
			uint32_t PRI_HINT_1 : 10; // 9:0
			uint32_t rsvd10 : 21; // 30:10
			uint32_t NONE_HINT_1 : 1; // 31
		} PRI_HINT_REG1_bit;
	}; // 0x904

	/* PRU_INTC_PRI_HINT_REG2 register bit field */
	union {
		volatile uint32_t PRI_HINT_REG2;

		volatile struct {
			uint32_t PRI_HINT_2 : 10; // 9:0
			uint32_t rsvd10 : 21; // 30:10
			uint32_t NONE_HINT_2 : 1; // 31
		} PRI_HINT_REG2_bit;
	}; // 0x908

	/* PRU_INTC_PRI_HINT_REG3 register bit field */
	union {
		volatile uint32_t PRI_HINT_REG3;

		volatile struct {
			uint32_t PRI_HINT_3 : 10; // 9:0
			uint32_t rsvd10 : 21; // 30:10
			uint32_t NONE_HINT_3 : 1; // 31
		} PRI_HINT_REG3_bit;
	}; // 0x90c

	/* PRU_INTC_PRI_HINT_REG4 register bit field */
	union {
		volatile uint32_t PRI_HINT_REG4;

		volatile struct {
			uint32_t PRI_HINT_4 : 10; // 9:0
			uint32_t rsvd10 : 21; // 30:10
			uint32_t NONE_HINT_4 : 1; // 31
		} PRI_HINT_REG4_bit;
	}; // 0x910

	/* PRU_INTC_PRI_HINT_REG5 register bit field */
	union {
		volatile uint32_t PRI_HINT_REG5;

		volatile struct {
			uint32_t PRI_HINT_5 : 10; // 9:0
			uint32_t rsvd10 : 21; // 30:10
			uint32_t NONE_HINT_5 : 1; // 31
		} PRI_HINT_REG5_bit;
	}; // 0x914

	/* PRU_INTC_PRI_HINT_REG6 register bit field */
	union {
		volatile uint32_t PRI_HINT_REG6;

		volatile struct {
			uint32_t PRI_HINT_6 : 10; // 9:0
			uint32_t rsvd10 : 21; // 30:10
			uint32_t NONE_HINT_6 : 1; // 31
		} PRI_HINT_REG6_bit;
	}; // 0x918

	/* PRU_INTC_PRI_HINT_REG7 register bit field */
	union {
		volatile uint32_t PRI_HINT_REG7;

		volatile struct {
			uint32_t PRI_HINT_7 : 10; // 9:0
			uint32_t rsvd10 : 21; // 30:10
			uint32_t NONE_HINT_7 : 1; // 31
		} PRI_HINT_REG7_bit;
	}; // 0x91c

	/* PRU_INTC_PRI_HINT_REG8 register bit field */
	union {
		volatile uint32_t PRI_HINT_REG8;

		volatile struct {
			uint32_t PRI_HINT_8 : 10; // 9:0
			uint32_t rsvd10 : 21; // 30:10
			uint32_t NONE_HINT_8 : 1; // 31
		} PRI_HINT_REG8_bit;
	}; // 0x920

	/* PRU_INTC_PRI_HINT_REG9 register bit field */
	union {
		volatile uint32_t PRI_HINT_REG9;

		volatile struct {
			uint32_t PRI_HINT_9 : 10; // 9:0
			uint32_t rsvd10 : 21; // 30:10
			uint32_t NONE_HINT_9 : 1; // 31
		} PRI_HINT_REG9_bit;
	}; // 0x924

	uint8_t rsvd928[984]; // 0x928 - 0xcff

	/* PRU_INTC_POLARITY_REG0 register bit field */
	union {
		volatile uint32_t POLARITY_REG0;

		volatile struct {
			uint32_t POLARITY_0 : 1; // 0
			uint32_t POLARITY_1 : 1; // 1
			uint32_t POLARITY_2 : 1; // 2
			uint32_t POLARITY_3 : 1; // 3
			uint32_t POLARITY_4 : 1; // 4
			uint32_t POLARITY_5 : 1; // 5
			uint32_t POLARITY_6 : 1; // 6
			uint32_t POLARITY_7 : 1; // 7
			uint32_t POLARITY_8 : 1; // 8
			uint32_t POLARITY_9 : 1; // 9
			uint32_t POLARITY_10 : 1; // 10
			uint32_t POLARITY_11 : 1; // 11
			uint32_t POLARITY_12 : 1; // 12
			uint32_t POLARITY_13 : 1; // 13
			uint32_t POLARITY_14 : 1; // 14
			uint32_t POLARITY_15 : 1; // 15
			uint32_t POLARITY_16 : 1; // 16
			uint32_t POLARITY_17 : 1; // 17
			uint32_t POLARITY_18 : 1; // 18
			uint32_t POLARITY_19 : 1; // 19
			uint32_t POLARITY_20 : 1; // 20
			uint32_t POLARITY_21 : 1; // 21
			uint32_t POLARITY_22 : 1; // 22
			uint32_t POLARITY_23 : 1; // 23
			uint32_t POLARITY_24 : 1; // 24
			uint32_t POLARITY_25 : 1; // 25
			uint32_t POLARITY_26 : 1; // 26
			uint32_t POLARITY_27 : 1; // 27
			uint32_t POLARITY_28 : 1; // 28
			uint32_t POLARITY_29 : 1; // 29
			uint32_t POLARITY_30 : 1; // 30
			uint32_t POLARITY_31 : 1; // 31
		} POLARITY_REG0_bit;
	}; // 0xd00

	/* PRU_INTC_POLARITY_REG1 register bit field */
	union {
		volatile uint32_t POLARITY_REG1;

		volatile struct {
			uint32_t POLARITY_32 : 1; // 0
			uint32_t POLARITY_33 : 1; // 1
			uint32_t POLARITY_34 : 1; // 2
			uint32_t POLARITY_35 : 1; // 3
			uint32_t POLARITY_36 : 1; // 4
			uint32_t POLARITY_37 : 1; // 5
			uint32_t POLARITY_38 : 1; // 6
			uint32_t POLARITY_39 : 1; // 7
			uint32_t POLARITY_40 : 1; // 8
			uint32_t POLARITY_41 : 1; // 9
			uint32_t POLARITY_42 : 1; // 10
			uint32_t POLARITY_43 : 1; // 11
			uint32_t POLARITY_44 : 1; // 12
			uint32_t POLARITY_45 : 1; // 13
			uint32_t POLARITY_46 : 1; // 14
			uint32_t POLARITY_47 : 1; // 15
			uint32_t POLARITY_48 : 1; // 16
			uint32_t POLARITY_49 : 1; // 17
			uint32_t POLARITY_50 : 1; // 18
			uint32_t POLARITY_51 : 1; // 19
			uint32_t POLARITY_52 : 1; // 20
			uint32_t POLARITY_53 : 1; // 21
			uint32_t POLARITY_54 : 1; // 22
			uint32_t POLARITY_55 : 1; // 23
			uint32_t POLARITY_56 : 1; // 24
			uint32_t POLARITY_57 : 1; // 25
			uint32_t POLARITY_58 : 1; // 26
			uint32_t POLARITY_59 : 1; // 27
			uint32_t POLARITY_60 : 1; // 28
			uint32_t POLARITY_61 : 1; // 29
			uint32_t POLARITY_62 : 1; // 30
			uint32_t POLARITY_63 : 1; // 31
		} POLARITY_REG1_bit;
	}; // 0xd04

	uint8_t rsvdd08[120]; // 0xd08 - 0xd7f

	/* PRU_INTC_TYPE_REG0 register bit field */
	union {
		volatile uint32_t TYPE_REG0;

		volatile struct {
			uint32_t TYPE_0 : 1; // 0
			uint32_t TYPE_1 : 1; // 1
			uint32_t TYPE_2 : 1; // 2
			uint32_t TYPE_3 : 1; // 3
			uint32_t TYPE_4 : 1; // 4
			uint32_t TYPE_5 : 1; // 5
			uint32_t TYPE_6 : 1; // 6
			uint32_t TYPE_7 : 1; // 7
			uint32_t TYPE_8 : 1; // 8
			uint32_t TYPE_9 : 1; // 9
			uint32_t TYPE_10 : 1; // 10
			uint32_t TYPE_11 : 1; // 11
			uint32_t TYPE_12 : 1; // 12
			uint32_t TYPE_13 : 1; // 13
			uint32_t TYPE_14 : 1; // 14
			uint32_t TYPE_15 : 1; // 15
			uint32_t TYPE_16 : 1; // 16
			uint32_t TYPE_17 : 1; // 17
			uint32_t TYPE_18 : 1; // 18
			uint32_t TYPE_19 : 1; // 19
			uint32_t TYPE_20 : 1; // 20
			uint32_t TYPE_21 : 1; // 21
			uint32_t TYPE_22 : 1; // 22
			uint32_t TYPE_23 : 1; // 23
			uint32_t TYPE_24 : 1; // 24
			uint32_t TYPE_25 : 1; // 25
			uint32_t TYPE_26 : 1; // 26
			uint32_t TYPE_27 : 1; // 27
			uint32_t TYPE_28 : 1; // 28
			uint32_t TYPE_29 : 1; // 29
			uint32_t TYPE_30 : 1; // 30
			uint32_t TYPE_31 : 1; // 31
		} TYPE_REG0_bit;
	}; // 0xd80

	/* PRU_INTC_TYPE_REG1 register bit field */
	union {
		volatile uint32_t TYPE_REG1;

		volatile struct {
			uint32_t TYPE_32 : 1; // 0
			uint32_t TYPE_33 : 1; // 1
			uint32_t TYPE_34 : 1; // 2
			uint32_t TYPE_35 : 1; // 3
			uint32_t TYPE_36 : 1; // 4
			uint32_t TYPE_37 : 1; // 5
			uint32_t TYPE_38 : 1; // 6
			uint32_t TYPE_39 : 1; // 7
			uint32_t TYPE_40 : 1; // 8
			uint32_t TYPE_41 : 1; // 9
			uint32_t TYPE_42 : 1; // 10
			uint32_t TYPE_43 : 1; // 11
			uint32_t TYPE_44 : 1; // 12
			uint32_t TYPE_45 : 1; // 13
			uint32_t TYPE_46 : 1; // 14
			uint32_t TYPE_47 : 1; // 15
			uint32_t TYPE_48 : 1; // 16
			uint32_t TYPE_49 : 1; // 17
			uint32_t TYPE_50 : 1; // 18
			uint32_t TYPE_51 : 1; // 19
			uint32_t TYPE_52 : 1; // 20
			uint32_t TYPE_53 : 1; // 21
			uint32_t TYPE_54 : 1; // 22
			uint32_t TYPE_55 : 1; // 23
			uint32_t TYPE_56 : 1; // 24
			uint32_t TYPE_57 : 1; // 25
			uint32_t TYPE_58 : 1; // 26
			uint32_t TYPE_59 : 1; // 27
			uint32_t TYPE_60 : 1; // 28
			uint32_t TYPE_61 : 1; // 29
			uint32_t TYPE_62 : 1; // 30
			uint32_t TYPE_63 : 1; // 31
		} TYPE_REG1_bit;
	}; // 0xd84

	uint8_t rsvdd88[888]; // 0xd88 - 0x10ff

	/* PRU_INTC_NEST_LEVEL_REG0 register bit field */
	union {
		volatile uint32_t NEST_LEVEL_REG0;

		volatile struct {
			uint32_t NEST_HINT_0 : 9; // 8:0
			uint32_t rsvd9 : 22; // 30:9
			uint32_t NEST_AUTO_OVR : 1; // 31
		} NEST_LEVEL_REG0_bit;
	}; // 0x1100

	/* PRU_INTC_NEST_LEVEL_REG1 register bit field */
	union {
		volatile uint32_t NEST_LEVEL_REG1;

		volatile struct {
			uint32_t NEST_HINT_1 : 9; // 8:0
			uint32_t rsvd9 : 22; // 30:9
			uint32_t NEST_AUTO_OVR : 1; // 31
		} NEST_LEVEL_REG1_bit;
	}; // 0x1104

	/* PRU_INTC_NEST_LEVEL_REG2 register bit field */
	union {
		volatile uint32_t NEST_LEVEL_REG2;

		volatile struct {
			uint32_t NEST_HINT_2 : 9; // 8:0
			uint32_t rsvd9 : 22; // 30:9
			uint32_t NEST_AUTO_OVR : 1; // 31
		} NEST_LEVEL_REG2_bit;
	}; // 0x1108

	/* PRU_INTC_NEST_LEVEL_REG3 register bit field */
	union {
		volatile uint32_t NEST_LEVEL_REG3;

		volatile struct {
			uint32_t NEST_HINT_3 : 9; // 8:0
			uint32_t rsvd9 : 22; // 30:9
			uint32_t NEST_AUTO_OVR : 1; // 31
		} NEST_LEVEL_REG3_bit;
	}; // 0x110c

	/* PRU_INTC_NEST_LEVEL_REG4 register bit field */
	union {
		volatile uint32_t NEST_LEVEL_REG4;

		volatile struct {
			uint32_t NEST_HINT_4 : 9; // 8:0
			uint32_t rsvd9 : 22; // 30:9
			uint32_t NEST_AUTO_OVR : 1; // 31
		} NEST_LEVEL_REG4_bit;
	}; // 0x1110

	/* PRU_INTC_NEST_LEVEL_REG5 register bit field */
	union {
		volatile uint32_t NEST_LEVEL_REG5;

		volatile struct {
			uint32_t NEST_HINT_5 : 9; // 8:0
			uint32_t rsvd9 : 22; // 30:9
			uint32_t NEST_AUTO_OVR : 1; // 31
		} NEST_LEVEL_REG5_bit;
	}; // 0x1114

	/* PRU_INTC_NEST_LEVEL_REG6 register bit field */
	union {
		volatile uint32_t NEST_LEVEL_REG6;

		volatile struct {
			uint32_t NEST_HINT_6 : 9; // 8:0
			uint32_t rsvd9 : 22; // 30:9
			uint32_t NEST_AUTO_OVR : 1; // 31
		} NEST_LEVEL_REG6_bit;
	}; // 0x1118

	/* PRU_INTC_NEST_LEVEL_REG7 register bit field */
	union {
		volatile uint32_t NEST_LEVEL_REG7;

		volatile struct {
			uint32_t NEST_HINT_7 : 9; // 8:0
			uint32_t rsvd9 : 22; // 30:9
			uint32_t NEST_AUTO_OVR : 1; // 31
		} NEST_LEVEL_REG7_bit;
	}; // 0x111c

	/* PRU_INTC_NEST_LEVEL_REG8 register bit field */
	union {
		volatile uint32_t NEST_LEVEL_REG8;

		volatile struct {
			uint32_t NEST_HINT_8 : 9; // 8:0
			uint32_t rsvd9 : 22; // 30:9
			uint32_t NEST_AUTO_OVR : 1; // 31
		} NEST_LEVEL_REG8_bit;
	}; // 0x1120

	/* PRU_INTC_NEST_LEVEL_REG9 register bit field */
	union {
		volatile uint32_t NEST_LEVEL_REG9;

		volatile struct {
			uint32_t NEST_HINT_9 : 9; // 8:0
			uint32_t rsvd9 : 22; // 30:9
			uint32_t NEST_AUTO_OVR : 1; // 31
		} NEST_LEVEL_REG9_bit;
	}; // 0x1124

	uint8_t rsvd1128[984]; // 0x1128 - 0x14ff

	/* PRU_INTC_ENABLE_HINT_REG0 register bit field */
	union {
		volatile uint32_t ENABLE_HINT_REG0;

		volatile struct {
			uint32_t ENABLE_HINT_0 : 1; // 0
			uint32_t ENABLE_HINT_1 : 1; // 1
			uint32_t ENABLE_HINT_2 : 1; // 2
			uint32_t ENABLE_HINT_3 : 1; // 3
			uint32_t ENABLE_HINT_4 : 1; // 4
			uint32_t ENABLE_HINT_5 : 1; // 5
			uint32_t ENABLE_HINT_6 : 1; // 6
			uint32_t ENABLE_HINT_7 : 1; // 7
			uint32_t ENABLE_HINT_8 : 1; // 8
			uint32_t ENABLE_HINT_9 : 1; // 9
			uint32_t rsvd10 : 22; // 31:10
		} ENABLE_HINT_REG0_bit;
	}; // 0x1500

} intc;

volatile __far intc CT_INTC __attribute__((cregister("PRU_INTC", far), peripheral));

#endif /* _PRU_INTC_H_ */
