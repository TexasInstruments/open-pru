/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
 */

/*
 * pdm8_shared.h - C mirror of firmware/pdm8_shared.inc, the shared map of
 * the 8-channel PDM audio demo (ICSSG0, 333.33 MHz). The .inc is the master
 * copy and documents every field (writer, protocol); this header holds the
 * same PDM8_* names with the same values, for the R5F application. The
 * pru-simulator test tests/test_pdm_open_pru_port.py::test_c_header_matches_inc
 * checks that both files define exactly the same names and values: change
 * them together.
 *
 * ICSS shared RAM (PRU local 0x10000, R5F global 0x30010000), 64 KB:
 *   +0x0000  header, 256 B (PDM8_HDR_* offsets, all u32)
 *   +0x0100  half A: 64 frames x 8 ch x int32 (signed 24-bit), PRU0
 *   +0x0900  half B: same layout, PRU0
 * Sample (frame f, half h, channel c) at 0x0100 + h*0x0800 + f*32 + c*4.
 * HALF_SEQ counts completed halves; the ready half is (HALF_SEQ - 1) & 1.
 * The R5F writes MAGIC, N_CH, FRAMES_PER_HALF, PCM_RATE, CLIP_ADDR and
 * CLIP_FRAMES before it starts the cores; the cores write the rest.
 */

#ifndef PDM8_SHARED_H_
#define PDM8_SHARED_H_

/* ---- Shared RAM map ---------------------------------------------------- */
#define PDM8_SHARED_BASE            (0x10000U)      /* ICSS shared RAM, PRU local addr */
#define PDM8_MAGIC                  (0x50444D38U)   /* 'PDM8' */
#define PDM8_FRAMES_PER_HALF        (64U)           /* PCM frames per ping-pong half */
#define PDM8_PCM_RATE               (16000U)        /* PCM sample rate, Hz */

#define PDM8_HDR_MAGIC              (0x00U)
#define PDM8_HDR_N_CH               (0x04U)
#define PDM8_HDR_FRAMES_PER_HALF    (0x08U)
#define PDM8_HDR_PCM_RATE           (0x0CU)
#define PDM8_HDR_CLIP_ADDR          (0x10U)         /* CLIP_FRAMES must follow */
#define PDM8_HDR_CLIP_FRAMES        (0x14U)
#define PDM8_HDR_HALF_SEQ           (0x18U)
#define PDM8_HDR_UNDERRUN_COUNT     (0x1CU)
#define PDM8_HDR_BATCH_COUNT        (0x20U)
#define PDM8_HDR_RTU1_FRAME_POS     (0x24U)
#define PDM8_HDR_RTU1_LOOPS         (0x28U)
#define PDM8_HDR_PCM_COUNT          (0x40U)         /* u32[8], up to 0x5F */

#define PDM8_HALF_A                 (0x0100U)       /* half 0, from SHARED_BASE */
#define PDM8_HALF_B                 (0x0900U)       /* half 1 = HALF_A + HALF_BYTES */
#define PDM8_HALF_BYTES             (0x0800U)       /* 64 frames x FRAME_BYTES */
#define PDM8_FRAME_BYTES            (32U)           /* one PCM frame: 8 ch x int32 */

#define PDM8_CLIP_FRAME_BYTES       (16U)           /* one clip frame: 8 ch x int16 */

/* ---- IPC scratchpad hand-off (RTU1 -> PRU1) ---------------------------- */
#define PDM8_SPAD_XID               (15U)           /* PRU_RTU_SPAD_XID */
#define PDM8_BATCH_PERIODS          (28U)           /* periods per batch (R2..R8) */
#define PDM8_SPAD_RTU1_BYTES        (30U)           /* RTU1's XOUT: R2..R9.w0 */
#define PDM8_NONCE_A                (2U)            /* RTU1 start nonce, unless the */
#define PDM8_NONCE_B                (3U)            /*   stale ack is NONCE_A */

/* ---- RTU1 scratch in DRAM1 (RTU1 local address) ------------------------ */
#define PDM8_RTU1_SCRATCH           (0x1700U)       /* 256 B, RTU1 only */

/* ---- PDM clock at 333.333 MHz ------------------------------------------ */
/* One PDM period = 108 + 73/144 cycles: 71 periods of 108 and 73 of 109. */
#define PDM8_CFG_T_SH_CYC           (108U)          /* short period, cycles */
#define PDM8_CFG_FRAC_ACC           (73U)           /* fraction added per period */
#define PDM8_CFG_FRAC_MAX           (144U)          /* accumulator wrap value */
#define PDM8_PCM_PERIODS            (192U)          /* PDM periods per 16 kHz frame */

/* ---- Guards (the .inc's .if/.emsg checks that matter to the R5F) ------- */
_Static_assert(PDM8_HALF_B == (PDM8_HALF_A + PDM8_HALF_BYTES),
               "PDM8_HALF_B must equal PDM8_HALF_A + PDM8_HALF_BYTES");
_Static_assert(PDM8_HALF_BYTES == (PDM8_FRAMES_PER_HALF * PDM8_FRAME_BYTES),
               "PDM8_HALF_BYTES must equal PDM8_FRAMES_PER_HALF * PDM8_FRAME_BYTES");
_Static_assert(PDM8_HDR_CLIP_FRAMES == (PDM8_HDR_CLIP_ADDR + 4U),
               "PDM8_HDR_CLIP_FRAMES must follow PDM8_HDR_CLIP_ADDR");
_Static_assert((PDM8_HDR_PCM_COUNT + 32U) <= PDM8_HALF_A,
               "header overlaps half A");

#endif /* PDM8_SHARED_H_ */
