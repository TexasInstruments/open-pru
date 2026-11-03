; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

;***************************************************************************
;   File:     pdm8_shared.inc
;
;   Brief:    Shared map of the 8-channel PDM audio demo (ICSSG0, 333.33 MHz):
;             ICSS shared-RAM header and PCM ping-pong halves, the RTU1 ->
;             PRU1 IPC scratchpad hand-off, RTU1's DRAM1 scratch and the PDM
;             clock constants. Included by rtu1_mod.asm, pru1_pdm.asm and
;             pru0_rx.asm. Design: ../docs/design.md sections 7 to 9.
;
;   Every .set is a plain literal: the pru-simulator assembler does not
;   expand a .set whose value refers to another .set symbol. Derived values
;   are checked by the .if/.emsg guards at the end.
;
;   ICSS shared RAM (local 0x10000, R5F global 0x30010000), 64 KB:
;     +0x0000  header, 256 B (offsets below, all u32)
;     +0x0100  half A: 64 frames x 8 ch x int32 (signed 24-bit), PRU0
;     +0x0900  half B: same layout, PRU0
;   Sample (frame f, half h, channel c) at 0x0100 + h*0x0800 + f*32 + c*4.
;   HALF_SEQ counts completed halves; the ready half is (HALF_SEQ - 1) & 1.
;
;   Header fields (writer):
;     MAGIC           'PDM8' = 0x50444D38                       R5F
;     N_CH            channels (8)                              R5F
;     FRAMES_PER_HALF 64                                        R5F
;     PCM_RATE        16000                                     R5F
;     CLIP_ADDR       global address of the MSRAM clip buffer   R5F
;     CLIP_FRAMES     clip length in frames (16 B: 8 x int16)   R5F
;     HALF_SEQ        completed halves                          PRU0
;     UNDERRUN_COUNT  batches PRU1 replaced by silence          RTU1 (*)
;     BATCH_COUNT     batches PRU1 took from the scratchpad     RTU1 (*)
;     RTU1_FRAME_POS  clip frame RTU1 is modulating (0-based)   RTU1
;     RTU1_LOOPS      clip restarts (wraps) so far              RTU1
;     PCM_COUNT[8]    PCM samples written per channel           PRU0
;   (*) PRU1 touches no memory at all (its timed path must not stall), so
;   RTU1 stores PRU1's counters for it at each batch it publishes:
;   UNDERRUN_COUNT accumulates PRU1's 8-bit SPAD count R9.b2 (mod-256
;   deltas), BATCH_COUNT = the batches PRU1 has acked. Both lag PRU1 by up
;   to one batch (~9 us) and are not updated while RTU1 is stalled.
;   0x2C..0x3F are free.
;
;   IPC scratchpad (XFR ID 15 = PRU_RTU_SPAD_XID in
;   source/firmware/common/icss_xfer_defines.inc), 32 B mapped to R2..R9,
;   one copy per slice (PRU1 <-> RTU1), no hardware arbitration:
;     R2..R8  28 B  batch: byte j = PDM period j, bit k = channel k   RTU1
;     R9.w0   2 B   batch sequence number (batches published)         RTU1
;     R9.b2   1 B   underruns so far, mod 256                         PRU1
;     R9.b3   1 B   ack = low byte of the last seq PRU1 took          PRU1
;   Each core XOUTs only its own bytes: RTU1 "xout 15, &r2, 30",
;   PRU1 "xout 15, &r9.b2/b3, 1..2".
;
;   Start: the R5F halts BOTH RTU1 and PRU1, loads both, and starts RTU1
;   first, then PRU1. (Both orders start cleanly, see below, but only when
;   both cores restart together. A stale SPAD from an earlier run is fine.)
;   Restarting one core alone does NOT recover:
;     - only RTU1: the old PRU1 never sees a matching seq again and plays
;       silence forever (it may play the nonce batch {c, 0, ...} once, if
;       its expected seq happens to be 0).
;     - only PRU1: PRU1 waits for a seq 0 that never comes; RTU1 may
;       release one extra batch (if it waits for ack 0), and it counts up
;       to 255 false underruns (PRU1 cleared R9.b2).
;   Protocol of a joint restart:
;     RTU1: reads the stale R9.b3 = e, picks a start nonce
;           c = (e == NONCE_A) ? NONCE_B : NONCE_A, so c is never 0, 1 or e,
;           and XOUTs R2..R9.w0 = {c, 0, ..., 0}: seq 0, nonce in R2.b0.
;           It publishes batch 0 (seq 1) only once R9.b3 == c.
;     PRU1: XOUTs R9.b2 = R9.b3 = 0, then polls. While seq == 0 it echoes
;           R2.b0 into R9.b3. It takes seq 1 only after it has seen seq 0,
;           so a stale seq 1 is never played.
;   The echo tells RTU1 that this run's PRU1 has seen seq 0; a stale R9.b3
;   cannot match c. Batch n >= 1 (seq n + 1) goes out once ack == n
;   (low byte). PRU1 acks batch n with the low byte of seq n + 1.
;
;   PRU DRAM1 (PRU1 and RTU1 local 0x0000): RTU1 owns local
;   0x1700..0x17FF (PDM8_RTU1_SCRATCH). PRU1's timed path must not touch
;   DRAM1 at all, so RTU1's accesses never stall it.
;***************************************************************************

; ---- Shared RAM map -------------------------------------------------------
PDM8_SHARED_BASE            .set    0x10000     ; ICSS shared RAM, local addr
PDM8_MAGIC                  .set    0x50444D38  ; 'PDM8'
PDM8_FRAMES_PER_HALF        .set    64          ; PCM frames per ping-pong half
PDM8_PCM_RATE               .set    16000       ; PCM sample rate, Hz

PDM8_HDR_MAGIC              .set    0x00
PDM8_HDR_N_CH               .set    0x04
PDM8_HDR_FRAMES_PER_HALF    .set    0x08
PDM8_HDR_PCM_RATE           .set    0x0C
PDM8_HDR_CLIP_ADDR          .set    0x10        ; CLIP_FRAMES must follow
PDM8_HDR_CLIP_FRAMES        .set    0x14
PDM8_HDR_HALF_SEQ           .set    0x18
PDM8_HDR_UNDERRUN_COUNT     .set    0x1C
PDM8_HDR_BATCH_COUNT        .set    0x20
PDM8_HDR_RTU1_FRAME_POS     .set    0x24
PDM8_HDR_RTU1_LOOPS         .set    0x28
PDM8_HDR_PCM_COUNT          .set    0x40        ; u32[8], up to 0x5F

PDM8_HALF_A                 .set    0x0100      ; half 0, from SHARED_BASE
PDM8_HALF_B                 .set    0x0900      ; half 1 = HALF_A + HALF_BYTES
PDM8_HALF_BYTES             .set    0x0800      ; 64 frames x FRAME_BYTES
PDM8_FRAME_BYTES            .set    32          ; one PCM frame: 8 ch x int32

PDM8_CLIP_FRAME_BYTES       .set    16          ; one clip frame: 8 ch x int16

; ---- IPC scratchpad hand-off (RTU1 -> PRU1) -------------------------------
PDM8_SPAD_XID               .set    15          ; PRU_RTU_SPAD_XID
PDM8_BATCH_PERIODS          .set    28          ; periods per batch (R2..R8)
PDM8_SPAD_RTU1_BYTES        .set    30          ; RTU1's XOUT: R2..R9.w0
PDM8_NONCE_A                .set    2           ; RTU1 start nonce, unless the
PDM8_NONCE_B                .set    3           ;   stale ack is NONCE_A

; ---- RTU1 scratch in DRAM1 (RTU1 local address) ---------------------------
PDM8_RTU1_SCRATCH           .set    0x1700      ; 256 B, RTU1 only

; ---- PDM clock at 333.333 MHz ---------------------------------------------
; One PDM period = 333.333 MHz / 3.072 MHz = 108 + 73/144 cycles: 71
; periods of 108 and 73 of 109 per 144 (integer fractional adder).
PDM8_CFG_T_SH_CYC           .set    108         ; short period, cycles
PDM8_CFG_FRAC_ACC           .set    73          ; fraction added per period
PDM8_CFG_FRAC_MAX           .set    144         ; accumulator wrap value
PDM8_PCM_PERIODS            .set    192         ; PDM periods per 16 kHz frame

; ---- Guards ----------------------------------------------------------------
    .if PDM8_HALF_B != (PDM8_HALF_A + PDM8_HALF_BYTES)
    .emsg "PDM8_HALF_B must equal PDM8_HALF_A + PDM8_HALF_BYTES"
    .endif
    .if PDM8_HALF_BYTES != (PDM8_FRAMES_PER_HALF * PDM8_FRAME_BYTES)
    .emsg "PDM8_HALF_BYTES must equal PDM8_FRAMES_PER_HALF * PDM8_FRAME_BYTES"
    .endif
    .if PDM8_HDR_CLIP_FRAMES != (PDM8_HDR_CLIP_ADDR + 4)
    .emsg "PDM8_HDR_CLIP_FRAMES must follow PDM8_HDR_CLIP_ADDR"
    .endif
    .if (PDM8_HDR_PCM_COUNT + 32) > PDM8_HALF_A
    .emsg "header overlaps half A"
    .endif
    .if PDM8_SPAD_RTU1_BYTES != (PDM8_BATCH_PERIODS + 2)
    .emsg "PDM8_SPAD_RTU1_BYTES must be the batch plus the 16-bit seq"
    .endif
    .if PDM8_HDR_BATCH_COUNT != (PDM8_HDR_UNDERRUN_COUNT + 4)
    .emsg "PDM8_HDR_BATCH_COUNT must follow PDM8_HDR_UNDERRUN_COUNT"
    .endif
    .if (PDM8_NONCE_A < 2) | (PDM8_NONCE_B < 2)
    .emsg "start nonces: not 0 (cleared ack) or 1 (ack of batch 0)"
    .endif
    .if PDM8_NONCE_A == PDM8_NONCE_B
    .emsg "PDM8_NONCE_A and PDM8_NONCE_B must differ"
    .endif
    .if (PDM8_NONCE_A > 255) | (PDM8_NONCE_B > 255)
    .emsg "the start nonces must be bytes"
    .endif
    .if PDM8_BATCH_PERIODS != 28
    .emsg "the batch must fill R2..R8 exactly (28 periods)"
    .endif
    .if (PDM8_CFG_FRAC_ACC >= PDM8_CFG_FRAC_MAX) | (PDM8_CFG_FRAC_ACC == 0)
    .emsg "PDM8_CFG_FRAC_ACC must be 1 .. PDM8_CFG_FRAC_MAX - 1"
    .endif
