; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
;
; eth_rx.asm - ICSSG1 PRU1 of pdm_audio_eth: Gigabit Ethernet receiver on
; port 1 (RGMII2), docs/design.md 5.3, frame layout in eth_proto.inc.
;
; Receive loop as in the HIL example (RX_L2 banks A/B of 32 bytes, R18.b0 =
; write position in the 64-byte double buffer, RX_EOF on R31 bit 20, RX reset
; and flag clear with "or r31.b2, r31.b2, 0xF4" after every frame).
;
; Per frame, from bank A (frame bytes 0..31 = Ethernet header 14 B + common
; header 16 B + body bytes 0..1):
;   - EtherType != 0x88B5, or destination neither the board MAC nor broadcast
;     -> RX_FOREIGN++
;   - version != 1 or type not 1..7 -> RX_BAD++
;   - else RX_FRAMES++, the source MAC goes to ETH1_PC_MAC (mac_valid = 1),
;     and by type:
;       PING      RX IEP time -> ETH1_PING_RX_IEP; from bank B: pc_ns ->
;                 ETH1_PING_PC_NS, then ETH1_PING_REQ = seq + 1 (PRU0 answers
;                 and clears it)
;       TPUT_DATA TPUT_RX_FRAMES++, TPUT_RX_BYTES += length, a gap when seq
;                 is not last seq + 1, first / last IEP time
;       CONTROL   from bank B: 1 = zero the counters, 2 = TPUT_TX_LEN = arg1
;                 then TPUT_TX_COUNT = arg0, 3 = REPORT_REQ = 1, else RX_BAD++
;       AUDIO_IN  RX_AUDIO++ (stored into the clip buffer in Task 12)
; The rest of the frame is drained until EOF. The counters RX_FRAMES ..
; LAST_IN_END live in R24..R28 and are written to the shared RAM after every
; frame.
;
; Timing: a 32-byte bank fills every 256 ns = 85 cycles at 333 MHz, and a
; bank is overwritten 512 ns after it was full. The longest bank-A path
; (TPUT_DATA: 32-byte LBCO + SBCO of the TPUT block, IEP read) is about 45
; cycles; the longest bank-B path (CONTROL reset) about 20.
;
; Registers:
;   R0, R1   scratch; R1.w0 keeps body bytes 0..1 from bank A for bank B
;   R2..R18  XIN target (R18.b0 = RX_L2 write position)
;   R19.b0   action for bank B (ACT_*)
;   R20      seq of the current frame
;   R21, R22 scratch (IEP time, MAC, CONTROL fields)
;   R23      seq of the last TPUT_DATA frame
;   R24..R28 RX_FRAMES, RX_AUDIO, RX_FOREIGN, RX_BAD, LAST_IN_END
;   R29      IEP0 base (local)
;   C28      ICSSG1 shared RAM (local 0x10000, set by the R5F)

    .include "eth_proto.inc"

ACT_NONE            .set    0
ACT_PING            .set    1
ACT_CTL             .set    2
BOARD_MAC_LO        .set    0x4D5D0002  ; dst bytes 0..3: 02 00 5D 4D
BOARD_MAC_HI        .set    0x0100      ; dst bytes 4..5: 00 01
ETHERTYPE_LE        .set    0xB588      ; bytes 12, 13 = 88 B5 read as LE w0

    .retain
    .retainrefs
    .global ETH_RX
    .sect   ".text"

ETH_RX:
    zero    &r19, 40                    ; R19..R28
    ldi32   r29, ICSS_IEP_LOCAL
    sbco    &r24, c28, ETH1_RX_FRAMES, 20
    or      r31.b2, r31.b2, ICSS_RX_RESET_BITS

; ---- bank A: Ethernet + common header -----------------------------------
wait_a:
    xin     ICSS_RXL2_SIDEA, &r2, 65
    qbbs    short_frame, r31, ICSS_RX_EOF_BIT
    qbgt    wait_a, r18.b0, 33          ; until bank A (bytes 0..31) is full

    ldi     r19.b0, ACT_NONE
    ldi     r0.w0, ETHERTYPE_LE
    qbne    foreign, r5.w0, r0.w0
    ldi32   r0, BOARD_MAC_LO
    qbne    dst_bcast, r2, r0
    ldi     r0.w0, BOARD_MAC_HI
    qbeq    dst_ok, r3.w0, r0.w0
dst_bcast:
    not     r0, r2
    qbne    foreign, r0, 0
    not     r0.w0, r3.w0
    qbne    foreign, r0.w0, 0
dst_ok:
    qbne    bad, r5.b2, ETH_VERSION
    qbeq    bad, r5.b3, 0
    qblt    bad, r5.b3, ETH_T_CONTROL   ; type > 7
    add     r24, r24, 1                 ; RX_FRAMES
    mov     r21.w0, r3.w2               ; source MAC bytes 0..1
    mov     r21.w2, r4.w0               ; bytes 2..3
    mov     r22.w0, r4.w2               ; bytes 4..5
    ldi     r22.w2, 1                   ; mac_valid
    sbco    &r21, c28, ETH1_PC_MAC, 8
    mov     r20.w0, r6.w2               ; seq = frame bytes 18..21
    mov     r20.w2, r7.w0
    mov     r1.w0, r9.w2                ; body bytes 0..1 (frame bytes 30, 31)
    qbeq    a_ping, r5.b3, ETH_T_PING
    qbeq    a_ctl, r5.b3, ETH_T_CONTROL
    qbeq    a_tput, r5.b3, ETH_T_TPUT_DATA
    qbeq    a_audio, r5.b3, ETH_T_AUDIO_IN
    qba     wait_b

a_ping:
    lbbo    &r21, r29, ICSS_IEP_COUNT_LO, 8
    sbco    &r21, c28, ETH1_PING_RX_IEP, 8
    ldi     r19.b0, ACT_PING
    qba     wait_b

a_ctl:
    ldi     r19.b0, ACT_CTL
    qba     wait_b

a_audio:
    add     r25, r25, 1                 ; RX_AUDIO
    qba     wait_b

a_tput:
    ; R10 frames, R11 gaps, R12/R13 bytes, R14/R15 first IEP, R16/R17 last
    lbco    &r10, c28, ETH1_TPUT_RX_FRAMES, 32
    lbbo    &r21, r29, ICSS_IEP_COUNT_LO, 8
    qbne    tput_next, r10, 0
    mov     r14, r21
    mov     r15, r22
    qba     tput_count
tput_next:
    add     r0, r23, 1
    qbeq    tput_count, r0, r20
    add     r11, r11, 1                 ; sequence gap
tput_count:
    add     r10, r10, 1
    add     r12, r12, r6.w0             ; += length
    adc     r13, r13, 0
    mov     r16, r21
    mov     r17, r22
    mov     r23, r20
    sbco    &r10, c28, ETH1_TPUT_RX_FRAMES, 32
    qba     wait_b

foreign:
    add     r26, r26, 1                 ; RX_FOREIGN
    qba     wait_b

bad:
    add     r27, r27, 1                 ; RX_BAD

; ---- bank B: body bytes 2.. (frame bytes 32..63) ------------------------
wait_b:
    xin     ICSS_RXL2_SIDEB, &r2, 65
    qbbs    b_ready, r31, ICSS_RX_EOF_BIT
    qblt    wait_b, r18.b0, 32          ; until bank B is full (position wraps)
b_ready:
    qbeq    drain_a, r19.b0, ACT_NONE
    qbeq    b_ping, r19.b0, ACT_PING

    ; CONTROL: cmd = frame bytes 30..33, arg0 = 34..37, arg1 = 38..41
    mov     r21.w0, r1.w0
    mov     r21.w2, r2.w0               ; cmd
    mov     r22.w0, r2.w2
    mov     r22.w2, r3.w0               ; arg0
    mov     r0.w0, r3.w2
    mov     r0.w2, r4.w0                ; arg1
    qbeq    c_reset, r21, ETH_CMD_RESET_COUNTERS
    qbeq    c_tput_tx, r21, ETH_CMD_TPUT_TX
    qbeq    c_report, r21, ETH_CMD_TPUT_REPORT_REQ
    add     r27, r27, 1                 ; unknown command: RX_BAD
    qba     drain_a

c_reset:
    zero    &r24, 16                    ; RX_FRAMES .. RX_BAD
    zero    &r10, 32
    sbco    &r10, c28, ETH1_TPUT_RX_FRAMES, 32
    qba     drain_a

c_tput_tx:
    sbco    &r0, c28, ETH1_TPUT_TX_LEN, 4
    sbco    &r22, c28, ETH1_TPUT_TX_COUNT, 4    ; count last: PRU0 starts
    qba     drain_a

c_report:
    ldi     r0, 1
    sbco    &r0, c28, ETH1_REPORT_REQ, 4
    qba     drain_a

b_ping:
    ; pc_ns = frame bytes 30..37
    mov     r21.w0, r1.w0
    mov     r21.w2, r2.w0
    mov     r22.w0, r2.w2
    mov     r22.w2, r3.w0
    sbco    &r21, c28, ETH1_PING_PC_NS, 8
    add     r0, r20, 1
    sbco    &r0, c28, ETH1_PING_REQ, 4          ; last: PRU0 answers

; ---- drain the rest of the frame until EOF ------------------------------
drain_a:
    xin     ICSS_RXL2_SIDEA, &r2, 65
    qbbs    end_frame, r31, ICSS_RX_EOF_BIT
    qbgt    drain_a, r18.b0, 33
drain_b:
    xin     ICSS_RXL2_SIDEB, &r2, 65
    qbbs    end_frame, r31, ICSS_RX_EOF_BIT
    qblt    drain_b, r18.b0, 32
    qba     drain_a

short_frame:
    add     r27, r27, 1                 ; EOF before bank A was full: RX_BAD

end_frame:
    or      r31.b2, r31.b2, ICSS_RX_RESET_BITS
    sbco    &r24, c28, ETH1_RX_FRAMES, 20
    or      r31.b2, r31.b2, ICSS_RX_RESET_BITS
    qba     wait_a
