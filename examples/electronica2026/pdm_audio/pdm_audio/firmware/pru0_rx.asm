; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

;***************************************************************************
;   File:     pru0_rx.asm
;
;   Brief:    ICSSG0 PRU0: 8-channel PDM receiver of the PDM audio demo.
;             Each channel: SINC3 decimation by SD_OSR in the ICSS
;             sigma-delta (SD) hardware plus a firmware comb (32 kHz), the
;             63-tap stage-2 FIR decimating by 2 (16 kHz), a 20 Hz DC
;             blocker and sat24. Signed 24-bit PCM goes to the
;             sample-interleaved ping-pong halves in ICSS shared RAM;
;             HALF_SEQ counts the completed halves. Design: ../docs/design.md
;             sections 7, 9 and 10; shared map: pdm8_shared.inc.
;
;   Port of pru-simulator source/pdm/pdm_rx.asm (itself a port of the
;   bit-exact audio_pipe receiver). The DSP blocks are unchanged, byte for
;   byte: comb mod 2^28, the MAC in multiply-only mode with signed
;   correction and the one-NOP operand latency, the Q27 FIR from
;   fir_coefs.inc (tools/gen_fir_coefs.py from decimation_fir.h), the Q32
;   DC blocker, sat24 and the even/odd FIR stagger. Changed: the SD set-up
;   (every channel CLK_SEL = 0, CLK_INV = 0, SD channel map SD_CH_k), the
;   output (interleaved halves, PCM_COUNT[c]) and HALF_SEQ publishing.
;   Python reference: pru-simulator source/pdm/reference.py
;   (pcm_chain_for_channel); tests: pru-simulator
;   tests/test_pdm_open_pru_port.py (3 channels, all three cores).
;
;   Input: PRU1 (pru1_pdm.asm) drives the 3.072 MHz PDM clock on PRU1_GPO0,
;   wired to PRU0_GPI16 = SD8_CLK, and channel c's data to the SD data pin
;   SD(SD_CH_c)_D. Every SD channel selects sd8_clk (CLK_SEL = 0) on the
;   rising edge (CLK_INV = 0), so all channels sample the same edge.
;
; Numbers (333.333 MHz ICSSG0 clock): 3.072 MHz / 96 = 32 kHz SINC3, one SD
;   window = 96 x (108 + 73/144) = 10416.7 PRU cycles; FIR every 2nd
;   sample -> 16 kHz PCM.
;   y = (sum T[j] * D[w+1+j] + 2^26) >> 27, T in Q27 scaled by
;   2^23 / (OSR^3 / 2) so full-scale SINC3 input = +-2^23.
;   DC blocker (DC_BLOCK = 1), H(z) = a (1 - z^-1) / (1 - a z^-1), a = 1 - 2^-7,
;   -3 dB at 20.1 Hz: e += (y * 2^32 - e) >> 7 (int64 Q32, floor shift; no
;   dead-band), y -= round(e / 2^32). PCM = sat24(y).
;
; Scheduling: the SD R31 interface shows one channel at a time (ch_sel in
;   R30[29:26]), so the main loop round-robins c = 0..N_CH-1 once per SD
;   window: select SD channel SD_CH_c, spin on R31[28] (valid), read the
;   acc3 shadow, count a shadow overflow (R31[29], see SD_OVF_COUNT below),
;   clear valid + ovf, comb, store state, next channel. All channels
;   run on one PDM clock with the same OSR and are reinitialised back to
;   back (3 cycles each), so their shadows latch within one PDM period of
;   each other. A shadow is overwritten one window after it latched: one
;   round must stay below 10416 cycles (budget below).
;   FIR stagger: odd channels c drop WARMUP_SAMPLES + 1 comb outputs, even
;   channels WARMUP_SAMPLES. Every channel runs its FIR on its own 2nd,
;   4th, ... sample, so even channels FIR on global decimation phase 0 and
;   odd channels on phase 1, and a window runs at most ceil(N_CH / 2) FIRs.
;   Odd channel c writes PCM n one window after channel c - 1 (the parity
;   of the logical index c counts, not the SD channel number).
;
; Output (pdm8_shared.inc, PDM8_SHARED_BASE in r22): sample n of channel c
;   goes to half h = (n >> 6) & 1, frame f = n & 63:
;     PDM8_HALF_A + h * 0x800 + f * 32 + c * 4 = 0x100 + (n & 127) * 32 + c * 4
;   then PCM_COUNT[c] = n + 1 (header, after the sample).
; SD_OVF_COUNT (header): at each spin exit, before it clears valid and ovf,
;   PRU0 tests R31[29] = shadow_update_flag_ovf (TRM SPRUIM2H Table 6-419:
;   the channel's shadow latched again while valid was still set, so one
;   SINC3 sample was lost) and adds 1 to SD_OVF_COUNT (out of line; the
;   normal path costs the one QBBS). The count is sticky (cleared only at
;   the init), so the board check reads it once, at any time: 0 means
;   PRU0 kept up with all 8 channels.
; HALF_SEQ rule: HALF_SEQ is incremented and stored once EVERY channel has
;   written frame 63 of the half, after that last sample and PCM_COUNT
;   store. A channel that writes frame 63 of half h counts itself in a
;   per-half counter (PUB_BASE +4 b0 / b1); the N_CH-th one resets that
;   counter and publishes. This does not depend on which channel finishes
;   last: with the stagger it is the last odd channel (ch 1 at N_CH = 3,
;   ch 7 at N_CH = 8), one window after the even channels. The reader's
;   ready half is (HALF_SEQ - 1) & 1 and stays unchanged for the next 64
;   samples (4 ms); channel 0 writes frame 0 of the other half one window
;   (31.25 us) after the publish, and that half holds the half before
;   until then. The init clears HALF_SEQ, then PCM_COUNT[0..7] (stale
;   values of an earlier run) and SD_OVF_COUNT.
;
;   Cycle budget per SD window (PRU cycles at 333.333 MHz, window 10416.7),
;   worst cases measured in pru-simulator at N_CH = 3 over 388 windows by
;   tests/test_pdm_open_pru_port.py::test_worst_window_budget_8ch (a path
;   runs from a channel's spin exit to the next spin entry; cycle counts do
;   not depend on the clock: pdm_rx.asm gave w_comb 48, w_fir 905):
;     path                                   cycles
;     w_comb  comb + state, next channel         49   (incl. the R31[29]
;             test of SD_OVF_COUNT)
;     w_fir   FIR + DC + sat + store on top     920   (MAC sign correction
;             +1 per negative operand, DC/sat branches, HALF_SEQ path)
;     spin    wait of a latched channel           1
;     overhead: last channel (round wrap)        +3
;     N_CH = 3 worst window                    1983
;     N_CH = 8: 8 w_comb + 4 w_fir + 7 spin + overhead = 4082 (39 %);
;             pessimistic (every FIR input negative: +64 per FIR; shadows
;             one PDM period apart: +109) = 4447 (43 %)
;   The 8-channel figure is a count of the measured paths: the simulator
;   models SD channels 0..2 only, so the 8-channel build is assembled but
;   not run there. On the board, SD_OVF_COUNT shows an overrun.
;
; PRU0 DRAM0 (c24, local 0x0000; everything stays below 0x2000, so PRU0
;   never reaches DRAM1 through its 0x2000 window, where RTU1 keeps its
;   scratch at 0x3700..0x37FF):
;   0x7F8  publish block (PUB_BASE): +0 HALF_SEQ (PRU0's copy), +4 b0/b1
;          channels done with frame 63 of half 0 / half 1
;   0x800  64 x Q27 coefficients T[j], oldest input first (FIR_T_BASE),
;          shared by all channels
;   0x900  channel block c at CH_BASE + c * CH_STRIDE (0x220), 8 blocks
;          up to 0x1A00:
;     +0x00  comb state x[-1], d3, d5 (3 words)
;     +0x0C  PCM count (copy of PCM_COUNT[c])
;     +0x10  state: b0 warm-up count, b1 decimation phase, b2 delay index w,
;            b3 SD R30.b3 value (SD_CH_c << 2 | sd_en)
;     +0x14  SAMPLE_COUNT: 32 kHz samples after the warm-up
;     +0x18  DC estimate e, int64 Q32 (low word first)
;     +0x20  128 x int32 mirrored delay line: sample at index w in D[w],
;            D[w + 64]
;   The 0x1000..0x1A00 part is the RTU0 / TX_PRU0 share of DRAM0 in the
;   open-pru linker split; this demo runs neither core (linker.cmd), and
;   the R5F halts both before it starts PRU0.
;
; ---- Register map --------------------------------------------
; Reg       Use                                   Live in
; r0, r1    scratch (r1.b0/b1 mvid pointers,      everywhere
;           r1.w2 FIR input offset)
; r2-r9     FIR coefficient block / sample        per channel
;           scratch (r5-r9 DC blocker; r2, r3
;           publish block)
; r10-r15   channel state block +0x00..+0x17      per channel
; r10-r17   FIR input block (state saved first)   FIR
; r18, r19  64-bit accumulator                    FIR
; r20       DRAM0 address of channel block c      main loop
; r21       logical channel index c               main loop
; r22       PDM8_SHARED_BASE                      main loop
; r23       28-bit mask                           main loop
; r24       R31 command: clear valid + ovf        main loop
; r25-r29   MAC                                   FIR
; R30       only R30.b3 (SD channel select + sd_en) is written; GPO
;           R30[19:0] is untouched
;
; Host preconditions (the R5F application):
;   - GPCFG0 PRU0_GP_MUX_SEL = 3 (SD mode); without it R31[28] never rises
;     and PDM_WAIT spins forever (PCM_COUNT stays 0).
;   - SD_CFG_REG (ICSSG CFG + 0x44) at reset: SHARE_EN = 0, MAN_SD_EN = 0.
;     PRU0 SD_CLKOUT / shift-out off (it would drive PRU1_GPO1).
;   - Pinmux: PRU0_GPI16 (SD8_CLK) and the SDk_D pins of SD_CH_0..7 in GPI
;     mode with the receiver on.
;   - CTBIR0 C24_BLK_INDEX = 0 (c24 = PRU0 DRAM0 at local 0, reset value).
;   - The R5F writes the header (MAGIC, N_CH, ...) before the start; PRU0
;     writes only HALF_SEQ, SD_OVF_COUNT, PCM_COUNT[0..7] and the halves.
;   - RTU0, TX_PRU0 and TX_PRU1 halted (the R5F does it): DRAM0
;     0x1000..0x19FF below is their share.
;******************************************************************************

    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

    .global     PRU0_RX
    .sect       ".text"

    .include    "pdm8_shared.inc"

;------------------------------------------------------------------------------
; Build options. Keep every value a plain literal: pru-simulator does not
; expand a .set that refers to another .set symbol. Derived literals are
; guarded below. open-pru ships the 8-channel build only; the pru-simulator
; harness overrides N_CH and SD_CH_k at load time (3 channels, SD 0..2).
;------------------------------------------------------------------------------
N_CH                .set    8           ; channels 1..8
SD_CH_0             .set    0           ; SD ch of logical ch 0: SD0_D, GPI1
SD_CH_1             .set    1           ; SD1_D, GPI3
SD_CH_2             .set    2           ; SD2_D, GPI5
SD_CH_3             .set    3           ; SD3_D, GPI7
SD_CH_4             .set    5           ; SD5_D, GPI11 (SD4_D: GPI9 = PHY)
SD_CH_5             .set    6           ; SD6_D, GPI13
SD_CH_6             .set    7           ; SD7_D, GPI15
SD_CH_7             .set    8           ; SD8_D, GPI17
SD_OSR              .set    96          ; oversampling ratio 1..256
SD_SAMPLE_SIZE      .set    95          ; SD_OSR - 1
SD_HALF_SCALE       .set    442368      ; SD_OSR^3 / 2
DC_BLOCK            .set    1           ; 1 = 20 Hz DC blocker on, 0 = off
WARMUP_SAMPLES      .set    3           ; comb outputs dropped after reinit (+1 on odd ch)
PUB_BASE            .set    0x7F8       ; publish block in DRAM0 (8 bytes)
FIR_T_BASE          .set    0x800       ; coefficient table in DRAM0
CH_BASE             .set    0x900       ; channel block 0 in DRAM0 (FIR_T_BASE + 0x100)
CH_STRIDE           .set    0x220       ; bytes per channel block
CH_STATE_OFS        .set    0x10        ; state word in a channel block
CH_DC_OFS           .set    0x18        ; DC estimate in a channel block
CH_D_OFS            .set    0x20        ; delay line in a channel block

    .if (N_CH < 1) | (N_CH > 8)
    .emsg "N_CH must be 1..8"
    .endif
    .if (SD_CH_0 < 0) | (SD_CH_0 > 8)
    .emsg "SD_CH_k must be an SD channel 0..8"
    .endif
    .if (SD_CH_1 < 0) | (SD_CH_1 > 8)
    .emsg "SD_CH_k must be an SD channel 0..8"
    .endif
    .if (SD_CH_2 < 0) | (SD_CH_2 > 8)
    .emsg "SD_CH_k must be an SD channel 0..8"
    .endif
    .if (SD_CH_3 < 0) | (SD_CH_3 > 8)
    .emsg "SD_CH_k must be an SD channel 0..8"
    .endif
    .if (SD_CH_4 < 0) | (SD_CH_4 > 8)
    .emsg "SD_CH_k must be an SD channel 0..8"
    .endif
    .if (SD_CH_5 < 0) | (SD_CH_5 > 8)
    .emsg "SD_CH_k must be an SD channel 0..8"
    .endif
    .if (SD_CH_6 < 0) | (SD_CH_6 > 8)
    .emsg "SD_CH_k must be an SD channel 0..8"
    .endif
    .if (SD_CH_7 < 0) | (SD_CH_7 > 8)
    .emsg "SD_CH_k must be an SD channel 0..8"
    .endif
    .if (SD_CH_0 == SD_CH_1) | (SD_CH_0 == SD_CH_2) | (SD_CH_0 == SD_CH_3)
    .emsg "SD_CH_k must be distinct"
    .endif
    .if (SD_CH_0 == SD_CH_4) | (SD_CH_0 == SD_CH_5) | (SD_CH_0 == SD_CH_6)
    .emsg "SD_CH_k must be distinct"
    .endif
    .if (SD_CH_0 == SD_CH_7)
    .emsg "SD_CH_k must be distinct"
    .endif
    .if (SD_CH_1 == SD_CH_2) | (SD_CH_1 == SD_CH_3) | (SD_CH_1 == SD_CH_4)
    .emsg "SD_CH_k must be distinct"
    .endif
    .if (SD_CH_1 == SD_CH_5) | (SD_CH_1 == SD_CH_6) | (SD_CH_1 == SD_CH_7)
    .emsg "SD_CH_k must be distinct"
    .endif
    .if (SD_CH_2 == SD_CH_3) | (SD_CH_2 == SD_CH_4) | (SD_CH_2 == SD_CH_5)
    .emsg "SD_CH_k must be distinct"
    .endif
    .if (SD_CH_2 == SD_CH_6) | (SD_CH_2 == SD_CH_7)
    .emsg "SD_CH_k must be distinct"
    .endif
    .if (SD_CH_3 == SD_CH_4) | (SD_CH_3 == SD_CH_5) | (SD_CH_3 == SD_CH_6)
    .emsg "SD_CH_k must be distinct"
    .endif
    .if (SD_CH_3 == SD_CH_7)
    .emsg "SD_CH_k must be distinct"
    .endif
    .if (SD_CH_4 == SD_CH_5) | (SD_CH_4 == SD_CH_6) | (SD_CH_4 == SD_CH_7)
    .emsg "SD_CH_k must be distinct"
    .endif
    .if (SD_CH_5 == SD_CH_6) | (SD_CH_5 == SD_CH_7)
    .emsg "SD_CH_k must be distinct"
    .endif
    .if (SD_CH_6 == SD_CH_7)
    .emsg "SD_CH_k must be distinct"
    .endif
    .if (SD_OSR < 1) | (SD_OSR > 256)
    .emsg "SD_OSR must be 1..256"
    .endif
    .if (SD_SAMPLE_SIZE != SD_OSR - 1) | (SD_HALF_SCALE != SD_OSR * SD_OSR * SD_OSR / 2)
    .emsg "SD_SAMPLE_SIZE / SD_HALF_SCALE do not match SD_OSR"
    .endif
    .if (PDM8_FRAMES_PER_HALF != 64) | (PDM8_FRAME_BYTES != 32)
    .emsg "half addressing ((n and 127) lsl 5) assumes 64 frames x 32 B"
    .endif
    .if (PDM8_HALF_BYTES != 0x800) | (PDM8_HALF_B != PDM8_HALF_A + 0x800)
    .emsg "half addressing ((n and 127) lsl 5) assumes contiguous halves"
    .endif
    .if (PDM8_HDR_HALF_SEQ > 252) | (PDM8_HDR_PCM_COUNT > 224) | (PDM8_HDR_SD_OVF_COUNT > 252)
    .emsg "HALF_SEQ / PCM_COUNT / SD_OVF_COUNT must be reachable with an 8-bit SBBO offset"
    .endif
    .if (CH_BASE != FIR_T_BASE + 0x100) | (CH_STRIDE != CH_D_OFS + 0x200)
    .emsg "FIR_T_BASE / CH_BASE / CH_STRIDE overlap"
    .endif
    .if CH_BASE + 8 * CH_STRIDE > 0x2000
    .emsg "channel blocks leave PRU0 DRAM0 (0x2000 and up is DRAM1)"
    .endif
    .if PUB_BASE + 8 > FIR_T_BASE
    .emsg "PUB_BASE overlaps the coefficient table"
    .endif
    .if (CH_STATE_OFS != 0x10) | (CH_DC_OFS != 0x18) | (CH_D_OFS != 0x20)
    .emsg "channel block layout inconsistent"
    .endif
    .if (DC_BLOCK < 0) | (DC_BLOCK > 1)
    .emsg "DC_BLOCK must be 0 or 1"
    .endif

;***************************************************************************
;   Macro: m_pdm_ch_init
;
;   Description:
;       Configure SD channel sd_ch for logical channel c and write the state
;       word of c's channel block (init only).
;
;   Pseudo code:
;       SD_CLK_SEL_REG(sd_ch) = 0;      // CLK_SEL 0 = sd8_clk (GPI16),
;                                       // CLK_INV 0 = rising, ACC_SEL 0 = acc3
;       SD_SAMPLE_SIZE_REG(sd_ch) = SD_SAMPLE_SIZE;
;       state(c) = {WARMUP_SAMPLES + odd, 0, 0, sd_ch << 2 | sd_en};
;       r20 += CH_STRIDE;
;
;   PEAK cycles: 13 instructions (3 SBCO)
;
;   Parameters:
;       sd_ch   SD channel SD_CH_c, 0..8 (ICSSG CFG + 0x48 + 8 sd_ch and
;               + 0x4C + 8 sd_ch)
;       odd     c & 1 (FIR stagger)
;       r20 = CH_BASE + c * CH_STRIDE on entry
;
;   Returns: r20 = block of channel c + 1
;
;   Registers modified: r0, r14, r20
;***************************************************************************
m_pdm_ch_init .macro sd_ch, odd
    ldi     r0, 0                           ; CLK_INV, CLK_SEL, ACC_SEL 0
    sbco    &r0, c4, 0x48 + 8 * sd_ch, 4    ; SD_CLK_SEL_REG(sd_ch)
    ldi     r0, SD_SAMPLE_SIZE
    sbco    &r0, c4, 0x4C + 8 * sd_ch, 4    ; SD_SAMPLE_SIZE_REG(sd_ch)
    ldi     r14.b0, WARMUP_SAMPLES
    add     r14.b0, r14.b0, odd             ; FIR stagger: odd ch one more
    ldi     r14.b1, 0                       ; decimation phase
    ldi     r14.b2, 0                       ; delay index w
    ldi     r14.b3, sd_ch * 4 + 2           ; R30[29:26] sd_ch, [25] sd_en
    add     r0, r20, CH_STATE_OFS
    sbco    &r14, c24, r0, 4
    ldi     r0, CH_STRIDE
    add     r20, r20, r0
    .endm

;***************************************************************************
;   Macro: m_pdm_ch_reinit
;
;   Description:
;       Select SD channel sd_ch, reinit its accumulators and drop a stale
;       valid shadow (the reinit keeps it).
;
;   Pseudo code:
;       R30.b3 = sd_ch << 2 | sd_en;  R31 = reinit;  R31 = clear valid + ovf;
;
;   PEAK cycles: 3
;
;   Parameters:
;       sd_ch   SD channel SD_CH_c, 0..8
;       r0 = 0x00800000 (reinit), r24 = 0x01000000 (clear valid + ovf)
;
;   Returns: none (SD channel sd_ch restarted)
;
;   Registers modified: r30.b3
;***************************************************************************
m_pdm_ch_reinit .macro sd_ch
    ldi     r30.b3, sd_ch * 4 + 2           ; R30[29:26] sd_ch, [25] sd_en
    mov     r31, r0                         ; one 32-bit write: reinit only
    mov     r31, r24                        ; clear valid + ovf
    .endm

PRU0_RX:
    ; --- zero the 8 channel blocks, the publish block, HALF_SEQ,
    ;     PCM_COUNT[0..7] and SD_OVF_COUNT (stale values of an earlier run) -
    ldi     r10, 0
    ldi     r11, 0
    ldi     r12, 0
    ldi     r13, 0
    ldi     r14, 0
    ldi     r15, 0
    ldi     r16, 0
    ldi     r17, 0
    ldi     r0, CH_BASE
    loop    PDM_ZERO, 136                   ; 8 * CH_STRIDE / 32
    sbco    &r10, c24, r0, 32
    add     r0, r0, 32
PDM_ZERO:
    ldi     r0, PUB_BASE
    sbco    &r10, c24, r0, 8                ; HALF_SEQ copy, done counts
    ldi32   r22, PDM8_SHARED_BASE
    sbbo    &r10, r22, PDM8_HDR_HALF_SEQ, 4  ; first: no stale ready half
    sbbo    &r10, r22, PDM8_HDR_PCM_COUNT, 32
    sbbo    &r10, r22, PDM8_HDR_SD_OVF_COUNT, 4

    ; --- coefficient table T to DRAM0 FIR_T_BASE (generated) ---------------
    ldi     r1, FIR_T_BASE
    .include "fir_coefs.inc"
    .if FIR_COEF_OSR != SD_OSR
    .emsg "fir_coefs.inc was generated for a different SD_OSR"
    .endif

    ; --- MAC: multiply-only mode -------------------------------------------
    ldi     r25, 0
    xout    0, &r25, 1

    ; --- SD channels: clock, OSR, state blocks; then reinit back to back ----
    ldi32   r23, 0x0FFFFFFF                 ; 28-bit accumulator mask
    ldi32   r24, 0x01000000                 ; R31 bit 24: clear valid + ovf
    ldi     r20, CH_BASE
    m_pdm_ch_init SD_CH_0, 0
    .if N_CH > 1
    m_pdm_ch_init SD_CH_1, 1
    .endif
    .if N_CH > 2
    m_pdm_ch_init SD_CH_2, 0
    .endif
    .if N_CH > 3
    m_pdm_ch_init SD_CH_3, 1
    .endif
    .if N_CH > 4
    m_pdm_ch_init SD_CH_4, 0
    .endif
    .if N_CH > 5
    m_pdm_ch_init SD_CH_5, 1
    .endif
    .if N_CH > 6
    m_pdm_ch_init SD_CH_6, 0
    .endif
    .if N_CH > 7
    m_pdm_ch_init SD_CH_7, 1
    .endif
    ldi32   r0, 0x00800000                  ; R31 bit 23: reinit accumulators
    m_pdm_ch_reinit SD_CH_0
    .if N_CH > 1
    m_pdm_ch_reinit SD_CH_1
    .endif
    .if N_CH > 2
    m_pdm_ch_reinit SD_CH_2
    .endif
    .if N_CH > 3
    m_pdm_ch_reinit SD_CH_3
    .endif
    .if N_CH > 4
    m_pdm_ch_reinit SD_CH_4
    .endif
    .if N_CH > 5
    m_pdm_ch_reinit SD_CH_5
    .endif
    .if N_CH > 6
    m_pdm_ch_reinit SD_CH_6
    .endif
    .if N_CH > 7
    m_pdm_ch_reinit SD_CH_7
    .endif
    ldi     r20, CH_BASE
    ldi     r21, 0

    ; --- main loop: one channel per pass, N_CH passes per SD window ---------
PDM_CH:
    lbco    &r10, c24, r20, 24              ; comb, PCM count, state, SAMPLE_COUNT
    mov     r30.b3, r14.b3                  ; select SD channel k
PDM_WAIT:
    qbbc    PDM_WAIT, r31, 28               ; wait for shadow valid
    and     r2, r31, r23                    ; acc3 shadow
    qbbs    PDM_OVF, r31, 29                ; a shadow was lost: count it
PDM_CLEAR:
    mov     r31, r24                        ; one 32-bit write: clear valid + ovf

    ; --- comb3: y = x - 3x[-1] + 3x[-2] - x[-3], modulo 2^28 ---------------
    rsb     r6, r10, r2
    mov     r10, r2
    rsb     r7, r11, r6
    mov     r11, r6
    rsb     r8, r12, r7
    mov     r12, r7
    and     r8, r8, r23

    qbeq    PDM_SAMPLE, r14.b0, 0           ; drop comb warm-up outputs
    sub     r14.b0, r14.b0, 1
    sbco    &r10, c24, r20, 24
    qba     PDM_NEXT

PDM_SAMPLE:
    ldi32   r0, SD_HALF_SCALE
    sub     r8, r8, r0                      ; signed 32 kHz sample
    add     r15, r15, 1                     ; SAMPLE_COUNT
    lsl     r0, r14.b2, 2                   ; D[w] and D[w + 64]
    add     r0, r0, r20
    add     r0, r0, CH_D_OFS
    sbco    &r8, c24, r0, 4
    ldi     r1, 0x100                       ; mirror distance: 64 words
    add     r0, r0, r1
    sbco    &r8, c24, r0, 4
    xor     r14.b1, r14.b1, 1               ; decimation phase
    qbeq    PDM_FIR, r14.b1, 0              ; every 2nd sample: FIR
    add     r14.b2, r14.b2, 1               ; w = (w + 1) mod 64
    and     r14.b2, r14.b2, 63
    sbco    &r10, c24, r20, 24
    qba     PDM_NEXT

    ; --- FIR: acc = sum_j T[j] * D[w + 1 + j], 8 blocks of 8 taps ----------
PDM_FIR:
    sbco    &r10, c24, r20, 24              ; save state (r10-r17 become inputs)
    add     r0, r14.b2, 1
    lsl     r0, r0, 2
    add     r0, r0, CH_D_OFS
    add     r0, r0, r20
    mov     r1.w2, r0.w0                    ; input offset of D[w + 1]
    ldi     r0, FIR_T_BASE                  ; coefficient offset
    ldi     r18, 0
    ldi     r19, 0
    loop    PDM_FIR_END, 8
    lbco    &r2, c24, r0, 32                ; T[8b .. 8b+7]
    lbco    &r10, c24, r1.w2, 32            ; D[w+1+8b .. w+8+8b]
    ldi     r1.b0, 8                        ; mvid pointer -> r2
    ldi     r1.b1, 40                       ; mvid pointer -> r10
PDM_TAP:
    mvid    r28, *r1.b0++                   ; coefficient
    mvid    r29, *r1.b1++                   ; input
    nop
    xin     0, &r26, 8                      ; unsigned 64-bit product r27:r26
    qbbc    PDM_SGN1, r28, 31               ; signed correction
    sub     r27, r27, r29
PDM_SGN1:
    qbbc    PDM_SGN2, r29, 31
    sub     r27, r27, r28
PDM_SGN2:
    add     r18, r18, r26                   ; 64-bit accumulate
    adc     r19, r19, r27
    qbgt    PDM_TAP, r1.b0, 40              ; 8 taps per block
    add     r0, r0, 32
    add     r1.w2, r1.w2, 32
PDM_FIR_END:

    ; --- round, >> 27, saturate to +-(2^23 - 1) -----------------------------
    ldi32   r0, 0x04000000
    add     r18, r18, r0
    adc     r19, r19, 0
    lsr     r2, r18, 27
    lsl     r3, r19, 5
    or      r2, r2, r3                      ; y = acc >> 27
    .if DC_BLOCK != 0
    ; --- DC blocker: e += (y * 2^32 - e) >> 7, y -= round(e / 2^32) -------
    ; Borrow and sign are handled with branches, not suc/rsc (their carry
    ; convention is not needed, so the V3/V4 carry semantics do not matter).
    add     r0, r20, CH_DC_OFS
    lbco    &r6, c24, r0, 8                 ; e = r7:r6
    rsb     r8, r6, 0                       ; d = y * 2^32 - e: low 0 - e_lo
    sub     r9, r2, r7                      ;   high y - e_hi ...
    qbeq    PDM_DC_NB, r6, 0
    sub     r9, r9, 1                       ;   ... - borrow (e_lo != 0)
PDM_DC_NB:
    lsr     r8, r8, 7                       ; d >> 7, 64-bit arithmetic
    lsl     r5, r9, 25
    or      r8, r8, r5
    qbbs    PDM_DC_NEG, r9, 31
    lsr     r9, r9, 7
    qba     PDM_DC_ADD
PDM_DC_NEG:
    not     r9, r9                          ; floor shift of a negative word:
    lsr     r9, r9, 7                       ;   ~(~x >> 7)
    not     r9, r9
PDM_DC_ADD:
    add     r6, r6, r8                      ; e += d >> 7
    adc     r7, r7, r9
    sbco    &r6, c24, r0, 8
    lsr     r5, r6, 31                      ; round(e / 2^32) = e_hi + e_lo[31]
    add     r5, r7, r5
    sub     r2, r2, r5                      ; y - e[n]
    .endif
    ldi32   r0, 8388607
    add     r3, r2, r0                      ; y + max, in 0..2*max when in range
    ldi32   r4, 16777214
    qbge    PDM_SAT_OK, r3, r4
    qbbc    PDM_SAT_POS, r3, 31
    rsb     r2, r0, 0                       ; -max
    qba     PDM_SAT_OK
PDM_SAT_POS:
    mov     r2, r0                          ; +max
PDM_SAT_OK:

    ; --- publish: sample n of channel c to half (n >> 6) & 1, frame n & 63,
    ;     then PCM_COUNT[c] -------------------------------------------------
    lbco    &r10, c24, r20, 24              ; reload state
    and     r0, r13, 127                    ; h * 64 + f
    lsl     r0, r0, 5                       ; h * 0x800 + f * FRAME_BYTES
    lsl     r1, r21, 2                      ; + c * 4
    add     r0, r0, r1
    ldi     r1, PDM8_HALF_A
    add     r0, r0, r1
    sbbo    &r2, r22, r0, 4                 ; PCM sample first ...
    add     r13, r13, 1
    lsl     r0, r21, 2
    add     r0, r0, PDM8_HDR_PCM_COUNT
    sbbo    &r13, r22, r0, 4                ; ... then PCM_COUNT[c]
    add     r14.b2, r14.b2, 1               ; w = (w + 1) mod 64
    and     r14.b2, r14.b2, 63
    sbco    &r10, c24, r20, 24

    ; --- HALF_SEQ: count channel c done with half h when it wrote frame 63;
    ;     the N_CH-th channel done publishes ---------------------------------
    and     r0, r13, 63                     ; n + 1 = 64 m: frame 63 written
    qbne    PDM_NEXT, r0, 0
    ldi     r0, PUB_BASE
    lbco    &r2, c24, r0, 8                 ; HALF_SEQ, r3.b0/b1 done counts
    qbbc    PDM_PUB_H1, r13, 6              ; (n + 1) bit 6 = 0: m even: half 1
    add     r3.b0, r3.b0, 1
    qbne    PDM_PUB_ST, r3.b0, N_CH
    ldi     r3.b0, 0
    qba     PDM_PUB_SEQ
PDM_PUB_H1:
    add     r3.b1, r3.b1, 1
    qbne    PDM_PUB_ST, r3.b1, N_CH
    ldi     r3.b1, 0
PDM_PUB_SEQ:
    add     r2, r2, 1
    sbbo    &r2, r22, PDM8_HDR_HALF_SEQ, 4  ; after every channel's frame 63
PDM_PUB_ST:
    sbco    &r2, c24, r0, 8

PDM_NEXT:
    add     r21, r21, 1
    ldi     r0, CH_STRIDE
    add     r20, r20, r0
    qbgt    PDM_CH, r21, N_CH               ; next channel while c < N_CH
    ldi     r21, 0
    ldi     r20, CH_BASE
    qba     PDM_CH

    ; --- SD shadow overflow (R31[29] = shadow_update_flag_ovf: the shadow
    ;     latched again before this channel's last one was read). Out of
    ;     line, so the normal path costs only the QBBS. PRU0 is the only
    ;     writer of SD_OVF_COUNT (sticky: never cleared after the init). ----
PDM_OVF:
    lbbo    &r0, r22, PDM8_HDR_SD_OVF_COUNT, 4
    add     r0, r0, 1
    sbbo    &r0, r22, PDM8_HDR_SD_OVF_COUNT, 4
    qba     PDM_CLEAR
