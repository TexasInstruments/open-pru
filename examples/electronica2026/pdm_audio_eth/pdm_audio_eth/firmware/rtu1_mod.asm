; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

;***************************************************************************
;   File:     rtu1_mod.asm
;
;   Brief:    ICSSG0 RTU1 (RTU_PRU1): 8 second-order sigma-delta modulators
;             of the 8-channel PDM audio demo. Reads the clip frames from
;             MSRAM, runs one modulator per channel and hands the bits to
;             PRU1 through the IPC scratchpad in batches of 28 PDM periods.
;             RTU1 is not cycle-exact: it runs ahead of PRU1 by at most one
;             batch and waits for PRU1's ack. Design: ../docs/design.md
;             sections 7 to 9; shared map: pdm8_shared.inc.
;
;   Modulator (per channel and PDM period, bit-exact with pru-simulator
;   source/pdm/reference.py sigma_delta2; FS = 32768, bit 1 = +FS):
;             i1 += x - fb;  i2 += i1 - fb;  bit = (i2 >= 0);
;             fb = bit ? +FS : -FS;  start i1 = i2 = 0, fb = -FS.
;             x = int16 clip sample, held for 192 periods (16 kHz ZOH).
;   Stored as j1 = i1 - FS and xm = x - FS. With b = previous bit:
;             b = 1:  j1 += xm;            i2 += j1
;             b = 0:  j1 += xm + 2 FS;     i2 += j1 + 2 FS
;   (2 FS = 0x10000: one add to the upper halfword). b is the sign of i2
;   from the previous step, so one branch per channel step both selects
;   the feedback and packs the previous step's bit: both paths are 5
;   cycles. The bit of step m is packed in step m+1 (the bits of the last
;   step of a batch are taken from the i2 signs at the batch end). Step 0
;   has b = 0 by definition and runs once at start (INIT_STEP0).
;
;   Clip: frame-interleaved 8 x int16 per frame (16 B), CLIP_FRAMES frames
;   from the global address CLIP_ADDR (header, written by the R5F),
;   looping forever. CLIP_FRAMES = 0 plays silence (one zero frame in
;   scratch). The header's RTU1_FRAME_POS is the frame being modulated
;   and RTU1_LOOPS counts clip restarts; both are stored at each fetch.
;
;   Hand-off (IPC scratchpad, XFR ID 15, see pdm8_shared.inc):
;             - At start: read the stale ack e, XOUT R2..R9.w0 = seq 0
;               with the start nonce c (NONCE_A, or NONCE_B if e ==
;               NONCE_A) in R2.b0, the rest 0. Read the nonce and seq
;               back, XOUT again until they are there.
;             - Batch n: R2..R8 = 28 period bytes (bit k = channel k),
;               R9.w0 = seq = n + 1. RTU1 computes batch n, then spins on
;               "XIN R9.b3 until ack == target" and XOUTs the 30 bytes
;               R2..R9.w0 in one transfer, then XINs seq back and XOUTs
;               again until it is there. The target is c for batch 0
;               (PRU1 echoes c once it has seen seq 0, so neither core
;               can run ahead on a stale ack), n (low byte) after that.
;             - Read-back retry: the SPAD has no arbitration, so an XOUT
;               can be lost to a PRU1 XOUT in the same cycle (a late
;               batch meets PRU1's underrun XOUT at slot t11). A lost batch
;               would hang the pair (PRU1 plays silence, RTU1 waits for an
;               ack that never comes). The retry is unbounded: RTU1 alone
;               writes bytes 0..29, so the first XOUT not hit ends it.
;             - After each XOUT: PRU1's counters to the header (PRU1
;               touches no memory): UNDERRUN_COUNT += new underruns from
;               PRU1's 8-bit R9.b2, BATCH_COUNT = seq - 1 = batches PRU1
;               acked.
;             - XOUT -> XIN visibility: PRU1 reads seq/bytes in its own
;               XIN; the 30 bytes are written by one XOUT, so a reader
;               sees either the old or the new batch. RTU1 does not
;               depend on a visibility delay: it only polls the ack.
;
;   Memory use (RTU1 local addresses):
;             - Shared RAM 0x10000: header reads (CLIP_ADDR, CLIP_FRAMES)
;               and stores (RTU1_FRAME_POS, RTU1_LOOPS, and PRU1's
;               UNDERRUN_COUNT, BATCH_COUNT).
;             - DRAM1 PDM8_RTU1_SCRATCH (0x1700..0x17FF): batch buffer,
;               x - FS copy, clip pointers (layout below). PRU1 shares
;               DRAM1 but its timed path never touches it.
;             - MSRAM (CLIP_ADDR): one 16-byte LBBO per frame.
;
;   Hardware requirements:
;             - ICSSG0 core clock 333.333 MHz (R5F SysConfig coreClk).
;             - Started after the R5F wrote CLIP_ADDR / CLIP_FRAMES and
;               the clip buffer (uncached MSRAM).
;
;   Steps to build:
;             - Using CCS: import ti-pru-cgt/example.projectspec, build.
;             - Using makefile: make (project directory)
;
;***************************************************************************

; CCS/makefile specific settings
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

    .global     RTU1_MOD
    .sect       ".text"

    .include    "pdm8_shared.inc"

; ---- Register map --------------------------------------------------------
; Reg       Use                                   Live in           Owner
; r0        packed bits of the group: byte s =    group loop        main
;           step s, bit k = channel k
;           r0.b0: start nonce c                  init              main
; r1        PDM8_RTU1_SCRATCH (DRAM1)             everywhere        main
; r2..r9    xm0..xm7 = x_k - FS (32 bit)          group loop        main
;           batch R2..R8 + seq R9.w0, ack R9.b3,  BATCH_END         SPAD
;           R9.b2 ack target / PRU1's underruns
;           frame + clip pointers                 FETCH             main
; r10..r17  j1_0..j1_7 = i1_k - FS                everywhere        main
; r18..r25  i2_0..i2_7                            everywhere        main
; r26       scratch offset of the next group      group loop        main
;           word: SCR_BATCH_M1 + 4 g (INIT_MARK
;           while the first frame is fetched)
; r27       groups left in the current frame      group loop        main
; r28       seq = batches published               everywhere        main
; r29       PDM8_SHARED_BASE                      everywhere        main
; r30/r31   not used (RTU1 R30 drives no pins)    -                 hardware
;
; ---- Scratch (DRAM1, offsets from r1) ------------------------------------
; +0x03        B[-1]: dummy byte of group 0 (the previous batch's last
;              period again); B[j] = SCR_BATCH + j
; +0x04..0x1F  batch B[0..27]; group g stores B[4g-1 .. 4g+2], the batch
;              end puts B[27] straight into r8.b3
; +0x20..0x3F  xm0..xm7 of the current frame (restored after the XOUT)
; +0x40        next frame address    +0x44  next frame index
; +0x48        clip base address     +0x4C  clip frames
; +0x50        clip restarts (RTU1_LOOPS)
; +0x54        ack target of the next batch (byte): nonce, then seq.b0
; +0x58        underruns in total (u32)   +0x5C  PRU1's R9.b2 last seen
; +0x60..0x6F  zero frame (CLIP_FRAMES = 0)
;
; ---- Cycle budget (333.333 MHz) ------------------------------------------
; PRU1 plays one batch in 28 x 108.51 = 3038 cycles; RTU1's limit is 75 %
; of that, 2278 cycles, so that flat-latency MSRAM reads in the simulator
; keep 25 % margin for silicon.
; Path                    Cycles                          Verified
; channel step            5 (both paths)                  hand count
; period (8 channels)     40                              hand count
; group (4 periods)       168 (160 + SBBO 4 B + 4)        simulated
; frame fetch, 1 per 48   73, 83 with a clip wrap         simulated
;   groups                (MSRAM LBBO 16 B: flat latency in the simulator)
; batch end               38..43 before the counters (tail simulated
;                         bits 8..16, ack already there),
;                         + 2 seq read-back, + ~20 ack target and counters
; batch, XOUT to XOUT     min 1243, mean 1256, worst 1329 simulated (*)
; (*) pru-simulator tests/test_pdm_open_pru_port.py::
;     test_rtu1_budget_checkpoint: 8 channels, PRU1 stand-in acking at
;     once, 2-frame clip (fetches and wraps included). Worst case = 7
;     groups + batch end + fetch with wrap = 58 % of the 2278-cycle limit,
;     44 % of PRU1's batch time. The 949 spare cycles would absorb a
;     silicon MSRAM LBBO of up to ~970 cycles (vs. ~24 simulated).
; --------------------------------------------------------------------------

; ---- Build options (plain literals, see pdm8_shared.inc) -----------------
RTU1_ZOH_GROUPS     .set    48          ; groups of 4 periods per clip frame
                                        ; = PDM8_PCM_PERIODS / 4

SCR_BATCH_M1        .set    0x03        ; B[-1]
SCR_BATCH           .set    0x04        ; B[0]
SCR_BATCH_END       .set    0x1F        ; r26 after 7 groups = B[27]
SCR_XM              .set    0x20        ; xm0..xm7
SCR_NEXT_ADDR       .set    0x40        ; next addr, next index, base, frames
SCR_LOOPS           .set    0x50
SCR_ACK_TARGET      .set    0x54        ; byte
SCR_UNDER           .set    0x58        ; u32 total, then the last R9.b2
SCR_ZERO            .set    0x60        ; 16-byte zero frame
INIT_MARK           .set    0xFF        ; r26 while the first frame is fetched

    .if (RTU1_ZOH_GROUPS * 4) != PDM8_PCM_PERIODS
    .emsg "RTU1_ZOH_GROUPS must be PDM8_PCM_PERIODS / 4"
    .endif
    .if RTU1_ZOH_GROUPS > 0xFFFF
    .emsg "RTU1_ZOH_GROUPS must fit the 16-bit LDI"
    .endif
    .if SCR_BATCH_END != (SCR_BATCH_M1 + PDM8_BATCH_PERIODS)
    .emsg "SCR_BATCH_END must be SCR_BATCH_M1 + PDM8_BATCH_PERIODS"
    .endif
    .if SCR_BATCH != (SCR_BATCH_M1 + 1)
    .emsg "SCR_BATCH must follow SCR_BATCH_M1"
    .endif
    .if (SCR_UNDER + 5) > SCR_ZERO
    .emsg "SCR_UNDER overlaps SCR_ZERO"
    .endif
    .if (SCR_ZERO + PDM8_CLIP_FRAME_BYTES) > 0x100
    .emsg "RTU1 scratch exceeds 256 bytes"
    .endif
    .if PDM8_HDR_RTU1_LOOPS != (PDM8_HDR_RTU1_FRAME_POS + 4)
    .emsg "PDM8_HDR_RTU1_LOOPS must follow PDM8_HDR_RTU1_FRAME_POS"
    .endif
    .if PDM8_CLIP_FRAME_BYTES != 16
    .emsg "the frame unpack assumes 8 x int16 per clip frame"
    .endif

;***************************************************************************
;   Macro: m_mod_step
;
;   Description:
;       One modulator step of one channel; packs the previous step's bit.
;
;   Pseudo code:
;       if (i2 >= 0) { acc |= 1 << bit; j1 += xm; i2 += j1; }
;       else         { j1 += xm + 0x10000; i2 += j1 + 0x10000; }
;
;   PEAK cycles: 5 (both paths)
;
;   Parameters:
;       j1      register: i1 - FS of the channel
;       i2      register: i2 of the channel
;       xm      register: x - FS of the channel
;       bit     0..31: bit of r0 for the previous step's bit
;       lneg    unique label: the b = 0 path
;       ljoin   unique label: after the step
;
;   Returns: none (updates the channel state)
;
;   Registers modified: j1, i2, r0
;***************************************************************************
m_mod_step  .macro  j1, i2, xm, bit, lneg, ljoin
    qbbs    lneg, i2, 31                ; 1  previous bit 0?
    set     r0, r0, bit                 ; 1  previous bit 1 -> packed
    add     j1, j1, xm                  ; 1  fb = +FS: j1 += x - FS
    add     i2, i2, j1                  ; 1  i2 += i1 - FS
    qba     ljoin                       ; 1
lneg:
    add     j1, j1, xm                  ; 1  fb = -FS: j1 += x + FS
    add     j1.w2, j1.w2, 1             ; 1    (+ 0x10000)
    add     i2, i2, j1                  ; 1  i2 += i1 + FS
    add     i2.w2, i2.w2, 1             ; 1    (+ 0x10000)
ljoin:
    .endm

;***************************************************************************
;   Macro: m_mod_first
;
;   Description:
;       Step 0 of one channel: the previous bit is 0 by definition.
;
;   Pseudo code:
;       j1 += xm + 0x10000; i2 += j1 + 0x10000;
;
;   PEAK cycles: 4
;
;   Parameters: j1, i2, xm as m_mod_step
;
;   Returns: none (updates the channel state)
;
;   Registers modified: j1, i2
;***************************************************************************
m_mod_first .macro  j1, i2, xm
    add     j1, j1, xm
    add     j1.w2, j1.w2, 1
    add     i2, i2, j1
    add     i2.w2, i2.w2, 1
    .endm

;***************************************************************************
;   Macro: m_xm_lane
;
;   Description:
;       One channel's int16 sample to xm = x - FS (32 bit).
;
;   Pseudo code:
;       dst = (src ^ 0x8000) - 0x10000      ; = sign_extend(src) - 32768
;
;   PEAK cycles: 3
;
;   Parameters:
;       dst     register: xm of the channel
;       src     halfword field holding the int16 sample (may be dst.w0)
;
;   Returns: dst
;
;   Registers modified: dst
;***************************************************************************
m_xm_lane   .macro  dst, src
    mov     dst.w0, src
    xor     dst.b1, dst.b1, 0x80
    ldi     dst.w2, 0xFFFF
    .endm

;***************************************************************************
;   Macro: m_tail_bit
;
;   Description:
;       Packs one channel's bit of the batch's last period into r8 (B[27]).
;
;   Pseudo code:
;       if (i2 >= 0) r8 |= 1 << bit;
;
;   PEAK cycles: 2
;
;   Parameters:
;       i2      register: i2 of the channel
;       bit     24..31: channel k = 24 + k
;       lskip   unique label
;
;   Returns: r8 bit set if i2 >= 0
;
;   Registers modified: r8
;***************************************************************************
m_tail_bit  .macro  i2, bit, lskip
    qbbs    lskip, i2, 31
    set     r8, r8, bit
lskip:
    .endm

;***************************************************************************
;   RTU1_MOD: init, then the endless batch loop. Never returns.
;***************************************************************************
RTU1_MOD:
    zero    &r0, 120                        ; r0..r29 = 0
    xin     PDM8_SPAD_XID, &r9.b3, 1        ; e = stale ack (or PRU1's 0)
    qbeq    INIT_NONCE_B, r9.b3, PDM8_NONCE_A
    ldi     r0.b0, PDM8_NONCE_A             ; nonce c != 0, 1, e
    qba     INIT_NONCE_SET
INIT_NONCE_B:
    ldi     r0.b0, PDM8_NONCE_B
    nop                                     ; both paths 3 cycles
INIT_NONCE_SET:
    ; R2..R9.w0 = {c, 0, ..., 0}: seq 0, nonce in R2.b0. Read the nonce and
    ; seq back and write again until they are there: the SPAD has no
    ; arbitration, so this write can be lost to a PRU1 XOUT in the same
    ; cycle. Unbounded retry: RTU1 alone writes bytes 0..29, so the first
    ; write that is not hit by a collision ends it (PRU1 XOUTs at most once
    ; per new nonce here). R3..R8 stay 0 in the registers; their SPAD bytes
    ; are not played (seq 0).
    mov     r2.b0, r0.b0
    ldi     r9.w0, 0
    xout    PDM8_SPAD_XID, &r2, PDM8_SPAD_RTU1_BYTES   ; seq 0, nonce, batch 0
    xin     PDM8_SPAD_XID, &r2.b0, 1        ; read back the nonce ...
    xin     PDM8_SPAD_XID, &r9.w0, 2        ; ... and seq
    qbne    INIT_NONCE_SET, r2.b0, r0.b0    ; lost: write again
    qbne    INIT_NONCE_SET, r9.w0, 0
    ldi32   r29, PDM8_SHARED_BASE
    ldi     r1, PDM8_RTU1_SCRATCH
    sbbo    &r2.b0, r1, SCR_ACK_TARGET, 1   ; batch 0 waits for the echo
    ldi     r2.b0, 0
    ldi     r9.b3, 0
    sbbo    &r2, r1, SCR_ZERO, PDM8_CLIP_FRAME_BYTES    ; zero frame
    sbbo    &r2, r1, SCR_LOOPS, 4                       ; restarts = 0
    sbbo    &r2, r1, SCR_UNDER, 5                       ; underruns, last R9.b2
    sbbo    &r2, r29, PDM8_HDR_UNDERRUN_COUNT, 8        ; UNDERRUN = BATCH = 0
    sbbo    &r2, r29, PDM8_HDR_RTU1_FRAME_POS, 8        ; FRAME_POS = LOOPS = 0
    lbbo    &r6, r29, PDM8_HDR_CLIP_ADDR, 8 ; r6 = CLIP_ADDR, r7 = CLIP_FRAMES
    mov     r8, r6                          ; clip base
    mov     r9, r7                          ; clip frames
    ldi     r7, 0                           ; next frame index
    qbne    INIT_CLIP_OK, r9, 0
    add     r6, r1, SCR_ZERO                ; no clip: one frame of silence
    mov     r8, r6
    ldi     r9, 1
INIT_CLIP_OK:
    sbbo    &r6, r1, SCR_NEXT_ADDR, 16      ; next addr, index, base, frames
    ldi32   r10, 0xFFFF8000                 ; j1 = i1 - FS = -FS; i2 = 0
    mov     r11, r10
    mov     r12, r10
    mov     r13, r10
    mov     r14, r10
    mov     r15, r10
    mov     r16, r10
    mov     r17, r10
    ldi     r26, INIT_MARK                  ; FETCH returns to INIT_STEP0
    qba     FETCH

;---------------------------------------------------------------------------
; Group: 4 PDM periods of 8 channels (steps 4g .. 4g+3 of the batch).
; Step s packs the bits of step s-1 into r0 byte s.
;---------------------------------------------------------------------------
GROUP:
    m_mod_step  r10, r18, r2,  0, S0N0, S0J0
    m_mod_step  r11, r19, r3,  1, S0N1, S0J1
    m_mod_step  r12, r20, r4,  2, S0N2, S0J2
    m_mod_step  r13, r21, r5,  3, S0N3, S0J3
    m_mod_step  r14, r22, r6,  4, S0N4, S0J4
    m_mod_step  r15, r23, r7,  5, S0N5, S0J5
    m_mod_step  r16, r24, r8,  6, S0N6, S0J6
    m_mod_step  r17, r25, r9,  7, S0N7, S0J7
STEP1:
    m_mod_step  r10, r18, r2,  8, S1N0, S1J0
    m_mod_step  r11, r19, r3,  9, S1N1, S1J1
    m_mod_step  r12, r20, r4, 10, S1N2, S1J2
    m_mod_step  r13, r21, r5, 11, S1N3, S1J3
    m_mod_step  r14, r22, r6, 12, S1N4, S1J4
    m_mod_step  r15, r23, r7, 13, S1N5, S1J5
    m_mod_step  r16, r24, r8, 14, S1N6, S1J6
    m_mod_step  r17, r25, r9, 15, S1N7, S1J7
    m_mod_step  r10, r18, r2, 16, S2N0, S2J0
    m_mod_step  r11, r19, r3, 17, S2N1, S2J1
    m_mod_step  r12, r20, r4, 18, S2N2, S2J2
    m_mod_step  r13, r21, r5, 19, S2N3, S2J3
    m_mod_step  r14, r22, r6, 20, S2N4, S2J4
    m_mod_step  r15, r23, r7, 21, S2N5, S2J5
    m_mod_step  r16, r24, r8, 22, S2N6, S2J6
    m_mod_step  r17, r25, r9, 23, S2N7, S2J7
    m_mod_step  r10, r18, r2, 24, S3N0, S3J0
    m_mod_step  r11, r19, r3, 25, S3N1, S3J1
    m_mod_step  r12, r20, r4, 26, S3N2, S3J2
    m_mod_step  r13, r21, r5, 27, S3N3, S3J3
    m_mod_step  r14, r22, r6, 28, S3N4, S3J4
    m_mod_step  r15, r23, r7, 29, S3N5, S3J5
    m_mod_step  r16, r24, r8, 30, S3N6, S3J6
    m_mod_step  r17, r25, r9, 31, S3N7, S3J7
    sbbo    &r0, r1, r26, 4                 ; B[4g-1 .. 4g+2]
    ldi     r0, 0
    add     r26, r26, 4
    sub     r27, r27, 1
    qbeq    FETCH, r27, 0                   ; next step needs the next frame
GROUP_NEXT:
    qbne    GROUP, r26, SCR_BATCH_END

;---------------------------------------------------------------------------
; Batch end: R2..R8 = B[0..27], wait for ack == target (nonce for batch 0,
; seq.b0 after that), XOUT with seq + 1, then PRU1's counters.
;---------------------------------------------------------------------------
    lbbo    &r2, r1, SCR_BATCH, PDM8_BATCH_PERIODS  ; r8.b3 = stale B[27]
    ldi     r8.b3, 0                        ; B[27] = signs of the last step
    m_tail_bit  r18, 24, T0
    m_tail_bit  r19, 25, T1
    m_tail_bit  r20, 26, T2
    m_tail_bit  r21, 27, T3
    m_tail_bit  r22, 28, T4
    m_tail_bit  r23, 29, T5
    m_tail_bit  r24, 30, T6
    m_tail_bit  r25, 31, T7
    lbbo    &r9.b2, r1, SCR_ACK_TARGET, 1   ; nonce, or seq.b0 (batches sent)
WAIT_ACK:
    xin     PDM8_SPAD_XID, &r9.b3, 1        ; PRU1's ack
    qbne    WAIT_ACK, r9.b3, r9.b2          ; until PRU1 took batch seq - 1
    add     r28, r28, 1
PUBLISH:
    mov     r9.w0, r28.w0
    xout    PDM8_SPAD_XID, &r2, PDM8_SPAD_RTU1_BYTES   ; batch + seq
    ; Read seq back and write again until it is there (unbounded retry, as
    ; at the init): a late publish can land in the same cycle as PRU1's
    ; underrun XOUT of R9.b2/b3 (slot t11), and a lost batch would leave
    ; PRU1 playing silence while RTU1 waits for an ack that never comes.
    ; seq is new in every batch, so it shows whether this XOUT landed.
    xin     PDM8_SPAD_XID, &r9.w0, 2
    qbne    PUBLISH, r9.w0, r28.w0
    sbbo    &r28.b0, r1, SCR_ACK_TARGET, 1  ; next batch waits for ack seq.b0
    ; PRU1's counters to the header (r2..r9 are free until the xm reload)
    xin     PDM8_SPAD_XID, &r9.b2, 1        ; PRU1's underruns, mod 256
    lbbo    &r2, r1, SCR_UNDER, 5           ; r2 = total, r3.b0 = last R9.b2
    sub     r3.b1, r9.b2, r3.b0             ; new underruns (mod 256)
    add     r2, r2, r3.b1
    mov     r3.b0, r9.b2
    sbbo    &r2, r1, SCR_UNDER, 5
    sub     r3, r28, 1                      ; BATCH_COUNT = batches PRU1 acked
    sbbo    &r2, r29, PDM8_HDR_UNDERRUN_COUNT, 8
    lbbo    &r2, r1, SCR_XM, 32             ; xm0..xm7 back
    ldi     r26, SCR_BATCH_M1
    qba     GROUP

;---------------------------------------------------------------------------
; FETCH: next clip frame (wrapping) into xm0..xm7, header frame pos/loops.
; Entered from the group loop (returns to GROUP_NEXT) or from the init
; (r26 = INIT_MARK, continues at INIT_STEP0).
;---------------------------------------------------------------------------
FETCH:
    ldi     r27, RTU1_ZOH_GROUPS
    lbbo    &r6, r1, SCR_NEXT_ADDR, 16      ; r6 addr, r7 index, r8 base,
                                            ; r9 frames
    qblt    FETCH_LOAD, r9, r7              ; index < frames
    mov     r6, r8                          ; wrap to frame 0
    ldi     r7, 0
    lbbo    &r8, r1, SCR_LOOPS, 4
    add     r8, r8, 1
    sbbo    &r8, r1, SCR_LOOPS, 4
    sbbo    &r8, r29, PDM8_HDR_RTU1_LOOPS, 4
FETCH_LOAD:
    lbbo    &r2, r6, 0, PDM8_CLIP_FRAME_BYTES   ; 8 x int16 (MSRAM)
    sbbo    &r7, r29, PDM8_HDR_RTU1_FRAME_POS, 4
    add     r6, r6, PDM8_CLIP_FRAME_BYTES
    add     r7, r7, 1
    sbbo    &r6, r1, SCR_NEXT_ADDR, 8
    m_xm_lane   r9, r5.w2                   ; high lanes first: r2..r5 are
    m_xm_lane   r8, r5.w0                   ; read before they are written
    m_xm_lane   r7, r4.w2
    m_xm_lane   r6, r4.w0
    m_xm_lane   r5, r3.w2
    m_xm_lane   r4, r3.w0
    m_xm_lane   r3, r2.w2
    m_xm_lane   r2, r2.w0
    sbbo    &r2, r1, SCR_XM, 32
    qbne    GROUP_NEXT, r26, INIT_MARK

;---------------------------------------------------------------------------
; INIT_STEP0: step 0 of every channel (previous bit 0), then the group
; loop from step 1. r0 byte 0 (B[-1]) stays 0.
;---------------------------------------------------------------------------
INIT_STEP0:
    m_mod_first r10, r18, r2
    m_mod_first r11, r19, r3
    m_mod_first r12, r20, r4
    m_mod_first r13, r21, r5
    m_mod_first r14, r22, r6
    m_mod_first r15, r23, r7
    m_mod_first r16, r24, r8
    m_mod_first r17, r25, r9
    ldi     r0, 0
    ldi     r26, SCR_BATCH_M1
    qba     STEP1
