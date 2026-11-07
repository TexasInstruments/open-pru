; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

;***************************************************************************
;   File:     pru1_pdm.asm
;
;   Brief:    ICSSG0 PRU1: cycle-exact clock and pin driver of the 8-channel
;             PDM audio demo. Drives the 3.072 MHz PDM clock on GPO0 and the
;             8 data bits on GPO 1, 2, 6, 11, 12, 13, 15, 16, from the
;             28-period batches RTU1 hands over through the IPC scratchpad.
;             Design: ../docs/design.md sections 6 to 8; shared map and
;             hand-off protocol: pdm8_shared.inc.
;
;   Clock: 333.333 MHz / 3.072 MHz = 108 + 73/144 cycles per period, the
;   integer fractional adder of pru-simulator source/pdm/pdm_gen.asm:
;   high 54 (55 in a long period), low 54; 71 short + 73 long periods per
;   144 = 15625 cycles, zero drift, 1 cycle (3 ns) jitter. Edge times equal
;   pru-simulator reference.pdm_clock_edges(n, t0, 108, 73, 144): the test
;   runs in high half k on b = acc + FRAC_ACC (threshold FRAC_MAX +
;   FRAC_ACC = 217, b starts at 0) and the extra cycle goes into that
;   same high half.
;
;   Pins: one R30 write per half period, all 9 pins at once.
;     falling edge:  R30 = W(p)        clock 0, data of period p
;     rising edge:   R30 = W(p) | 1    clock 1, data unchanged
;   All SD channels sample at the rising edge (CLK_SEL = 0, CLK_INV = 0),
;   so the data changes once per period, with the falling edge that ends
;   the high half after the previous sampling edge:
;     setup = low time  = 54 cycles (162 ns) in every period (>= 26),
;     hold  = high time = 54 / 55 cycles (162 / 165 ns).
;   The long period's extra cycle is in the HIGH half so that the setup
;   never changes; only the hold grows by one cycle. Before the first
;   rising edge the data of period 0 is written with the clock low, one
;   full low time ahead.
;
;   Pin mapping (register-only, fixed 9 cycles): byte d (bit k = channel k)
;   to the pin word W (byte fields, W.b3 = 0, W bit 0 = clock = 0):
;     W.b0 = ((d << 1) & 0x06) | ((d << 4) & 0x40)   ch0, ch1, ch2 -> 1, 2, 6
;     W.b1 = (d & 0x38) | ((d << 1) & 0x80)          ch3..5, ch6 -> 11..13, 15
;     W.b2 = d >> 7                                  ch7 -> 16
;
;   Hand-off (IPC scratchpad, XFR ID 15, protocol in pdm8_shared.inc):
;     - Start: XOUT R9.b2 = R9.b3 = 0. Poll all 32 SPAD bytes every 9
;       cycles; while seq == 0, XOUT R9.b2 = 0, R9.b3 = RTU1's nonce
;       (R2.b0) when the SPAD does not hold that already: once per new
;       nonce, again if the write was lost (no SPAD arbitration). Take
;       seq 1 only after seq 0 was seen (a stale seq 1 is never played).
;       The clock stays low until then.
;     - Batch slot, high half of period 27 (fixed 11 cycles): XIN
;       R2..R9.w0 straight into the play registers. seq == expected: ack
;       R9.b3 = seq.b0, expected + 1. Otherwise (RTU1 late): R2..R8 =
;       silence (alternating 0/1 on every channel), R9.b2 + 1, no new ack;
;       the same batch is expected at the next slot. Both paths 9 cycles
;       after the compare, and both XOUT R9.b2 and R9.b3 together, so a
;       slot repeats a write of the slot before that a collision with
;       RTU1's XOUT lost (a lost ack: RTU1 waits, the next slot is
;       silence and re-sends it).
;     - XOUT -> XIN visibility: one XOUT/XIN moves the 30 bytes at once
;       (no torn batch). A batch that becomes visible after the slot's XIN
;       is simply taken one slot later (one underrun); no timing depends
;       on the delay.
;
;   Memory use: none. PRU1 executes no LBBO/SBBO/LBCO/SBCO at all, so its
;   timing cannot be stalled by RTU1 (DRAM1, which RTU1 uses at local
;   0x1700..0x17FF), PRU0 (shared RAM) or the R5F. Its counters go to the
;   header through RTU1: UNDERRUN_COUNT from R9.b2, BATCH_COUNT from the
;   acks (see pdm8_shared.inc). Registers only, XFR ID 15 only.
;
;   Hardware requirements:
;             - ICSSG0 core clock 333.333 MHz (R5F SysConfig coreClk).
;             - PRU1 pads GPO0, 1, 2, 6, 11, 12, 13, 15, 16 in mux mode 0,
;               GPCFG1 PRU1 mux = 0 (GP).
;             - RTU1 runs rtu1_mod (either core may start first).
;
;   Steps to build:
;             - Using CCS: import ti-pru-cgt/example.projectspec, build.
;             - Using makefile: make (project directory)
;
;***************************************************************************

; CCS/makefile specific settings
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

    .global     PRU1_PDM
    .sect       ".text"

    .include    "pdm8_shared.inc"

; ---- Register map --------------------------------------------------------
; Reg       Use                                   Live in           Owner
; r0, r1    not used                              -                 -
; r2..r8    play batch: byte j = period j,        everywhere        SPAD XIN
;           bit k = channel k
; r9.w0     seq of the last XIN                   start, slot       SPAD XIN
; r9.b2     underruns so far (mod 256); in the    everywhere        main
;           start loop the SPAD's R9.b2                             (SPAD XIN)
; r9.b3     ack / nonce echo; in the start loop   everywhere        main
;           the SPAD's R9.b3                                        (SPAD XIN)
; r10       pin word W of the current period      period loop       main
;           (clock bit 0 = 0, W.b3 = 0)
; r11       mapping scratch                       period loop       main
; r12       fractional accumulator b = acc + 73   period loop       main
; r13.w0    expected seq at the next slot         period loop       main
; r14       silence word 0xFF00FF00               period loop       main
; r15       1 once seq 0 was seen                 start             main
; r16..r29  not used                              -                 -
; r30       GPO: clock (bit 0) + 8 data pins      everywhere        hardware
; r31       not used                              -                 -
;
; ---- Cycle budget per half period (333.333 MHz, edge write included) ----
; Half                  length  busy  padding
; low (all periods)       54      1     53   (edge write; loop + 52 nop)
; high, short             54     13     41   (edge 1, map 9, adder 3)
; high, long              55     14     41   (adder 4)
; high, slot, short       54     25     29   (edge 1, slot 11, map 9,
; high, slot, long        55     26     29    adder 3/4, jmp 1)
; busy = edge write + work; padding = LOOP + NOP delay cycles (loop
; instruction included), collapsed by the pru-simulator idle skip.
; Per period: busy 14..27 of 108/109 cycles; per batch 404 + longs (~14).
; Batch slot detail: xin 1, qbne 1, then 9 on both paths:
;   ok:       mov 1, xout 1 (R9.b2 + R9.b3), add 1, loop+nop 5, qba 1
;   underrun: 7 x mov 7, add 1, xout 1 (R9.b2 + R9.b3)
; Verified: pru-simulator tests/test_pdm_open_pru_port.py (edge times vs.
; the 108 + 73/144 reference, setup/hold, one R30 write per half, bits).
; --------------------------------------------------------------------------

; ---- Build options (plain literals, see pdm8_shared.inc) -----------------
PDM1_CLK_PIN        .set    0           ; GPO0 = clock; data pins: see m_map
PDM1_T_HIGH_CYC     .set    54          ; high time of a short period
PDM1_T_LOW_CYC      .set    54          ; low time = setup of every bit
PDM1_SETUP_MIN_CYC  .set    26          ; required data setup (design 7)
PDM1_FRAC_TEST      .set    217         ; PDM8_CFG_FRAC_MAX + PDM8_CFG_FRAC_ACC
PDM1_FRAC_DROP      .set    71          ; PDM8_CFG_FRAC_MAX - PDM8_CFG_FRAC_ACC
PDM1_LOW_PAD        .set    52          ; T_LOW - 2 (edge, loop)
PDM1_HIGH_PAD       .set    40          ; T_HIGH - 14 (edge, map, adder, loop)
PDM1_SLOT_PAD       .set    28          ; T_HIGH - 26 (+ slot 11, jmp 1)
PDM1_SILENCE        .set    0xFF00FF00  ; bytes 00 FF 00 FF: 0/1 per period

    .if (PDM1_T_HIGH_CYC + PDM1_T_LOW_CYC) != PDM8_CFG_T_SH_CYC
    .emsg "PDM1_T_HIGH_CYC + PDM1_T_LOW_CYC must equal PDM8_CFG_T_SH_CYC"
    .endif
    .if PDM1_FRAC_TEST != (PDM8_CFG_FRAC_MAX + PDM8_CFG_FRAC_ACC)
    .emsg "PDM1_FRAC_TEST must be PDM8_CFG_FRAC_MAX + PDM8_CFG_FRAC_ACC"
    .endif
    .if PDM1_FRAC_DROP != (PDM8_CFG_FRAC_MAX - PDM8_CFG_FRAC_ACC)
    .emsg "PDM1_FRAC_DROP must be PDM8_CFG_FRAC_MAX - PDM8_CFG_FRAC_ACC"
    .endif
    .if (PDM1_FRAC_TEST > 255) | (PDM8_CFG_FRAC_ACC > 255)
    .emsg "fractional adder constants must fit an 8-bit immediate"
    .endif
    .if PDM1_FRAC_DROP > 255
    .emsg "fractional adder constants must fit an 8-bit immediate"
    .endif
    .if PDM1_LOW_PAD != (PDM1_T_LOW_CYC - 2)
    .emsg "PDM1_LOW_PAD must be PDM1_T_LOW_CYC - 2"
    .endif
    .if PDM1_HIGH_PAD != (PDM1_T_HIGH_CYC - 14)
    .emsg "PDM1_HIGH_PAD must be PDM1_T_HIGH_CYC - 14"
    .endif
    .if PDM1_SLOT_PAD != (PDM1_T_HIGH_CYC - 26)
    .emsg "PDM1_SLOT_PAD must be PDM1_T_HIGH_CYC - 26"
    .endif
    .if (PDM1_LOW_PAD < 1) | (PDM1_HIGH_PAD < 1) | (PDM1_SLOT_PAD < 1)
    .emsg "half periods too short for the fixed layout"
    .endif
    .if PDM1_LOW_PAD > 255
    .emsg "PDM1_LOW_PAD must fit the 8-bit LOOP count"
    .endif
    .if PDM1_T_LOW_CYC < PDM1_SETUP_MIN_CYC
    .emsg "the low time is the data setup: it must be >= PDM1_SETUP_MIN_CYC"
    .endif
    .if PDM1_CLK_PIN != 0
    .emsg "m_map keeps W bit 0 free for the clock: PDM1_CLK_PIN must be 0"
    .endif

;***************************************************************************
;   Macro: m_map
;
;   Description:
;       Pin word of one period from its batch byte (register-only).
;
;   Pseudo code:
;       W.b0 = ((d << 1) & 0x06) | ((d << 4) & 0x40);   // GPO1, 2, 6
;       W.b1 = (d & 0x38) | ((d << 1) & 0x80);          // GPO11..13, 15
;       W.b2 = d >> 7;                                  // GPO16
;
;   PEAK cycles: 9 (fixed)
;
;   Parameters:
;       d       byte field r2.b0 .. r8.b3: bit k = channel k
;
;   Returns: r10 (W.b3 and the clock bit stay 0)
;
;   Registers modified: r10.b0..b2, r11.b0..b1
;***************************************************************************
m_map       .macro  d
    lsl     r11.b0, d, 1                ; ch0, ch1 at bits 1, 2; ch6 at bit 7
    and     r10.b0, r11.b0, 0x06        ; GPO1, GPO2
    and     r11.b1, r11.b0, 0x80        ; ch6 at bit 7 of byte 1
    and     r10.b1, d, 0x38             ; ch3..5 -> GPO11..13
    or      r10.b1, r10.b1, r11.b1      ; ch6 -> GPO15
    lsl     r11.b0, d, 4
    and     r11.b0, r11.b0, 0x40
    or      r10.b0, r10.b0, r11.b0      ; ch2 -> GPO6
    lsr     r10.b2, d, 7                ; ch7 -> GPO16
    .endm

;***************************************************************************
;   Macro: m_frac
;
;   Description:
;       Fractional adder: decides whether this high half is long.
;
;   Pseudo code:
;       if (b >= FRAC_MAX + FRAC_ACC)  b -= FRAC_MAX - FRAC_ACC, +1 cycle;
;       else                           b += FRAC_ACC;
;
;   PEAK cycles: 3 (short), 4 (long)
;
;   Parameters:
;       llong   unique label: the long path
;       ljoin   unique label: after the adder
;
;   Returns: r12
;
;   Registers modified: r12
;***************************************************************************
m_frac      .macro  llong, ljoin
    qble    llong, r12, PDM1_FRAC_TEST  ; b >= 217: long period
    add     r12, r12, PDM8_CFG_FRAC_ACC
    qba     ljoin
llong:
    sub     r12, r12, PDM1_FRAC_DROP
    nop
    nop                                 ; the long period's extra cycle
ljoin:
    .endm

;***************************************************************************
;   Macro: m_period
;
;   Description:
;       One PDM period (periods 0..26 of a batch): the falling edge with
;       this period's data, the low time, the rising edge, then the pin
;       word of the next period and the fractional adder. m_map and m_frac
;       are written out inline: the pru-simulator assembler does not expand
;       a macro inside a macro.
;
;   Pseudo code:
;       R30 = W; wait; R30 = W | CLK; W = map(next); frac; wait
;
;   PEAK cycles: 108 (short), 109 (long); low 54, high 54/55 (fixed)
;
;   Parameters:
;       nxt     byte field of the next period (r2.b1 .. r8.b3)
;       lld, lfl, lfj, lhd   unique labels
;
;   Returns: r10 = W of the next period
;
;   Registers modified: r10, r11, r12, r30
;***************************************************************************
m_period    .macro  nxt, lld, lfl, lfj, lhd
    mov     r30, r10                    ; L t0      falling edge, data of p
    loop    lld, PDM1_LOW_PAD           ; L t1
    nop                                 ; L t2..t53
lld:
    set     r30, r10, PDM1_CLK_PIN      ; H t0      rising edge, data held
    lsl     r11.b0, nxt, 1              ; H t1      W of p+1 (= m_map):
    and     r10.b0, r11.b0, 0x06        ; H t2
    and     r11.b1, r11.b0, 0x80        ; H t3
    and     r10.b1, nxt, 0x38           ; H t4
    or      r10.b1, r10.b1, r11.b1      ; H t5
    lsl     r11.b0, nxt, 4              ; H t6
    and     r11.b0, r11.b0, 0x40        ; H t7
    or      r10.b0, r10.b0, r11.b0      ; H t8
    lsr     r10.b2, nxt, 7              ; H t9
    qble    lfl, r12, PDM1_FRAC_TEST    ; H t10     adder (= m_frac)
    add     r12, r12, PDM8_CFG_FRAC_ACC ; H t11
    qba     lfj                         ; H t12
lfl:
    sub     r12, r12, PDM1_FRAC_DROP    ; H t11     long period
    nop                                 ; H t12
    nop                                 ; H t13     extra high cycle
lfj:
    loop    lhd, PDM1_HIGH_PAD          ; H t13 (long t14)
    nop                                 ; H t14..t53 (t15..t54)
lhd:
    .endm

;***************************************************************************
;   PRU1_PDM: init, start hand-off, then the endless batch loop.
;   Never returns.
;***************************************************************************
PRU1_PDM:
    zero    &r0, 120                    ; r0..r29 = 0 (no stale registers)
    ldi     r30, 0                      ; clock and data low
    xout    PDM8_SPAD_XID, &r9.b2, 2    ; underruns = ack = 0
    ldi32   r14, PDM1_SILENCE

;---------------------------------------------------------------------------
; Start hand-off. PRU1 XINs all 32 bytes, its own R9.b2/b3 included, and
; XOUTs only when the scratchpad differs from what it must hold: while
; seq == 0, R9.b3 = RTU1's nonce R2.b0 and R9.b2 = 0. So it writes once
; per new nonce (not every pass, where it could hit RTU1's XOUTs: the SPAD
; has no arbitration), and a write of its own that was lost is written
; again on a later pass. The clock is idle here, so nothing is timed, but
; every pass is 9 cycles whatever the scratchpad holds, so the first edge
; comes at the same time after a clean or a stale start.
;---------------------------------------------------------------------------
WAIT_START:
    xin     PDM8_SPAD_XID, &r2, 32      ; 1  batch + seq + PRU1's two bytes
    qbeq    WS_ZERO, r9.w0, 0           ; 2  RTU1 cleared (or never sent)
    qbne    WS_SKIP, r9.w0, 1           ; 3  not batch 0
    qbne    WS_GO, r15, 0               ; 4  seq 1 after seq 0: batch 0
    qba     WS_PAD                      ; 5  stale seq 1: not played
WS_SKIP:
    nop                                 ; 4
    nop                                 ; 5
WS_PAD:
    nop                                 ; 6
    nop                                 ; 7
    nop                                 ; 8
    qba     WAIT_START                  ; 9
WS_ZERO:
    ldi     r15, 1                      ; 3  seq 0 seen
    qbne    WS_ECHO, r9.b3, r2.b0       ; 4  echo missing (new nonce, or lost)
    qbne    WS_WRITE, r9.b2, 0          ; 5  underrun byte not 0 (lost clear)
    nop                                 ; 6  both bytes hold their values
    nop                                 ; 7
    nop                                 ; 8
    qba     WAIT_START                  ; 9
WS_ECHO:
    nop                                 ; 5
WS_WRITE:
    mov     r9.b3, r2.b0                ; 6  echo RTU1's start nonce
    ldi     r9.b2, 0                    ; 7  underruns = 0
    xout    PDM8_SPAD_XID, &r9.b2, 2    ; 8
    qba     WAIT_START                  ; 9
WS_GO:
    ldi     r9.w2, 0x0100               ; underruns 0, ack batch 0 (seq 1)
    xout    PDM8_SPAD_XID, &r9.b2, 2
    ldi     r13, 2                      ; next expected seq
    ldi     r12, 0                      ; b: period 0 is short
    m_map   r2.b0                       ; W of period 0

;---------------------------------------------------------------------------
; Batch loop: 27 periods (each maps the next period's byte), then period
; 27 with the batch slot. Period 0's falling-edge write is the first data
; write after the start: the clock is already low.
;---------------------------------------------------------------------------
PERIOD0:
    m_period    r2.b1, P0_LD, P0_FL, P0_FJ, P0_HD               ; 0
    m_period    r2.b2, P1_LD, P1_FL, P1_FJ, P1_HD               ; 1
    m_period    r2.b3, P2_LD, P2_FL, P2_FJ, P2_HD               ; 2
    m_period    r3.b0, P3_LD, P3_FL, P3_FJ, P3_HD               ; 3
    m_period    r3.b1, P4_LD, P4_FL, P4_FJ, P4_HD               ; 4
    m_period    r3.b2, P5_LD, P5_FL, P5_FJ, P5_HD               ; 5
    m_period    r3.b3, P6_LD, P6_FL, P6_FJ, P6_HD               ; 6
    m_period    r4.b0, P7_LD, P7_FL, P7_FJ, P7_HD               ; 7
    m_period    r4.b1, P8_LD, P8_FL, P8_FJ, P8_HD               ; 8
    m_period    r4.b2, P9_LD, P9_FL, P9_FJ, P9_HD               ; 9
    m_period    r4.b3, P10_LD, P10_FL, P10_FJ, P10_HD           ; 10
    m_period    r5.b0, P11_LD, P11_FL, P11_FJ, P11_HD           ; 11
    m_period    r5.b1, P12_LD, P12_FL, P12_FJ, P12_HD           ; 12
    m_period    r5.b2, P13_LD, P13_FL, P13_FJ, P13_HD           ; 13
    m_period    r5.b3, P14_LD, P14_FL, P14_FJ, P14_HD           ; 14
    m_period    r6.b0, P15_LD, P15_FL, P15_FJ, P15_HD           ; 15
    m_period    r6.b1, P16_LD, P16_FL, P16_FJ, P16_HD           ; 16
    m_period    r6.b2, P17_LD, P17_FL, P17_FJ, P17_HD           ; 17
    m_period    r6.b3, P18_LD, P18_FL, P18_FJ, P18_HD           ; 18
    m_period    r7.b0, P19_LD, P19_FL, P19_FJ, P19_HD           ; 19
    m_period    r7.b1, P20_LD, P20_FL, P20_FJ, P20_HD           ; 20
    m_period    r7.b2, P21_LD, P21_FL, P21_FJ, P21_HD           ; 21
    m_period    r7.b3, P22_LD, P22_FL, P22_FJ, P22_HD           ; 22
    m_period    r8.b0, P23_LD, P23_FL, P23_FJ, P23_HD           ; 23
    m_period    r8.b1, P24_LD, P24_FL, P24_FJ, P24_HD           ; 24
    m_period    r8.b2, P25_LD, P25_FL, P25_FJ, P25_HD           ; 25
    m_period    r8.b3, P26_LD, P26_FL, P26_FJ, P26_HD           ; 26

; Period 27: low half as m_period; high half with the batch slot.
    mov     r30, r10                    ; L t0      falling edge, data of 27
    loop    P27_LD, PDM1_LOW_PAD        ; L t1
    nop                                 ; L t2..t53
P27_LD:
    set     r30, r10, PDM1_CLK_PIN      ; H t0      rising edge, data held
    xin     PDM8_SPAD_XID, &r2, PDM8_SPAD_RTU1_BYTES   ; H t1  next batch
    qbne    SLOT_UNDER, r9.w0, r13.w0   ; H t2      RTU1 late?
    mov     r9.b3, r9.b0                ; H t3      ack = seq.b0
    xout    PDM8_SPAD_XID, &r9.b2, 2    ; H t4      ack + underruns (again)
    add     r13.w0, r13.w0, 1           ; H t5      16-bit, wraps like seq
    loop    SLOT_OK_DLY, 4              ; H t6
    nop                                 ; H t7..t10
SLOT_OK_DLY:
    qba     SLOT_JOIN                   ; H t11
SLOT_UNDER:
    mov     r2, r14                     ; H t3      silence batch
    mov     r3, r14                     ; H t4
    mov     r4, r14                     ; H t5
    mov     r5, r14                     ; H t6
    mov     r6, r14                     ; H t7
    mov     r7, r14                     ; H t8
    mov     r8, r14                     ; H t9
    add     r9.b2, r9.b2, 1             ; H t10     underruns + 1
    xout    PDM8_SPAD_XID, &r9.b2, 2    ; H t11     + last ack (again)
SLOT_JOIN:
    m_map   r2.b0                       ; H t12..t20  W of period 0
    m_frac  P27_FL, P27_FJ              ; H t21..t23 (t24)
    loop    P27_HD, PDM1_SLOT_PAD       ; H t24 (t25)
    nop                                 ; H t25..t52 (t26..t53)
P27_HD:
    jmp     PERIOD0                     ; H t53 (t54)  (beyond the qba range)
