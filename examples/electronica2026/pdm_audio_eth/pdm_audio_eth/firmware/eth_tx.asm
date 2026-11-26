; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
;
; eth_tx.asm - ICSSG1 PRU0 of pdm_audio_eth: Gigabit Ethernet transmitter on
; port 1 (RGMII2), docs/design.md 5.2, frame layout in eth_proto.inc.
;
; Nothing is sent until PRU1 has seen a valid frame from the PC (mac_valid);
; every frame goes to that PC MAC. The main loop polls the ICSSG1 shared RAM
; in this order:
;   HALF_SEQ (ICSSG0) changed -> two AUDIO_OUT frames of the half it
;                     announced (Task 10, below), TX_AUDIO += 2
;   PING_REQ       -> PONG (ping seq, echoed pc_ns, PRU1's RX IEP time; the
;                     header ts is the TX IEP time), TX_PONGS++
;   REPORT_REQ     -> TPUT_REPORT (copy of the TPUT_RX block)
;   TPUT_TX_COUNT  -> that many TPUT_DATA frames back to back, seq 0..n-1;
;                     1514 B, or 60 B (64 B with CRC) when TPUT_TX_LEN is
;                     60; the burst checks HALF_SEQ before every frame, so
;                     audio still goes out on time
;
; Audio: HALF_SEQ is polled at its global address (0x30010018). When it
; changes, PRU0 takes the IEP time (detect_iep), reads the ICSSG0 header
; words CLIP_FRAMES .. SD_OVF_COUNT (reading RTU1_FRAME_POS/LOOPS again until
; both reads agree, so a clip wrap between the two words is not torn), and
; builds the status block. Each 1 KB part of the half (global 0x30010100 +
; ((HALF_SEQ - 1) & 1) x 0x800 + part x 0x400) is copied in 64-byte bursts
; into TX_BUF + 94 and sent with the status block; first_index =
; (HALF_SEQ - 1) x 64 + part x 32. If HALF_SEQ moved on by more than one
; since the last send, TX_LATE++ and only the newest half is sent. Copy
; (16 cross-ICSSG bursts) plus two frames at wire rate (2 x 9 us) is far
; inside the 4 ms of one half.
;
; Frames are built in DRAM0 at TX_BUF (local 0x0000) and sent through the
; TX_L2 FIFO as in the HIL example: per 64-byte chunk LBBO, wait until the
; FIFO level (R19.b2 after XIN 40) is 0, XOUT 64; the tail with a fixed
; XOUT size (XOUT sizes are immediates); then 10 NOPs, CRC + EOF and
; TX_EOF_GAP NOPs. The HIL example waited 100 NOPs after EOF only because it
; halted next; the TRM (SPRUIM2H 6.4.11.2.4.1.1.1) lets a new frame start
; once TX_L2 is empty, before TX_L1 drains, 5 or more core clocks after the
; drain (CRC of the old frame). The gap on the wire is MII_RT TX_IPG (R5F).
; Frame sizes are therefore fixed: PONG 64 B, TPUT_REPORT 64 B, TPUT_DATA
; 1514 B or 60 B (CONTROL arg1 60), AUDIO_OUT 1118 B (Task 10).
;
; Registers:
;   R0, R1    IEP time (build_header)
;   R2..R17   frame header / chunk
;   R18       TX_BUF base
;   R19       TX_L2 status (R19.b2 = FIFO level)
;   R20.b0    frame type, R20.b1 tail size (0, 30, 42), R20.w2 length
;   R21       header seq
;   R22       polled request word
;   R23       TPUT_DATA seq;  R24 TPUT_DATA frames left
;   R25       64-byte chunks left; R26 chunk pointer
;   R27       TX_PONGS
;   R28.w0    return address (jal, build_header / send_frame)
;   R28.w2    return address (jal, audio)
;   R29       IEP0 base (local)
;   C28       ICSSG1 shared RAM (local 0x10000, set by the R5F)
;
; Cycle budget (333 MHz, 3 ns per cycle), small frames (TPUT_TX length 60):
;   Path                      Budget            Estimate        Verified
;   EOF -> next XOUT          < 480 ns queued   ~100 cyc 300 ns hand count
;   (small_loop: TX_EOF_GAP, audio poll with one cross-ICSSG read,
;   build_header, send_frame up to its XOUT)
; After EOF the wire still has up to 40 B of TX_L1 (320 ns), the IPG (96 ns)
; and the next preamble (64 ns) to send, so the wire (64 B frame + preamble
; + IPG = 672 ns, 1.488 Mframes/s) and not PRU0 should set the rate.

    .include "eth_proto.inc"
    .include "pdm8_shared.inc"

TX_BUF              .set    0x0000      ; DRAM0, 1536 B frame buffer
SRC_MAC_01          .set    0x0002      ; frame bytes 6, 7  = 02 00
SRC_MAC_25          .set    0x01004D5D  ; frame bytes 8..11 = 5D 4D 00 01
ETHERTYPE_LE        .set    0xB588      ; frame bytes 12, 13 = 88 B5
PONG_LEN            .set    34          ; 64 - 14 - 16 (24 B body + pad)
REPORT_LEN          .set    34          ; 64 - 14 - 16 (32 B body + pad)
TPUT_LEN            .set    1484        ; 1514 - 14 - 16
TPUT_CHUNKS         .set    23          ; 1514 = 23 x 64 + 42
TPUT_TAIL           .set    42
AUDIO_LEN           .set    1088        ; 1118 - 14 - 16 (status + PCM)
AUDIO_CHUNKS        .set    17          ; 1118 = 17 x 64 + 30
AUDIO_TAIL          .set    30
AUDIO_TX_GUARD      .set    50          ; 2-cycle loops (0.3 us) after audio
TX_EOF_GAP          .set    5           ; cycles after CRC + EOF (TRM: >= 5)
SMALL_ARG           .set    60          ; TPUT_TX length that asks for 60 B
SMALL_LEN           .set    30          ; 60 - 14 - 16 (64 B with CRC)
OFF_BODY            .set    30          ; frame offset of the body
OFF_PCM             .set    94          ; 30 + 64-byte status block
PART_BYTES          .set    1024        ; 32 frames x 8 ch x int32
ST_BASE             .set    0x0700      ; DRAM0 state (after TX_BUF)
ST_LAST_HALF        .set    0           ; last HALF_SEQ sent
ST_AUDIO_SEQ        .set    4           ; AUDIO_OUT header seq
ST_TX_LATE          .set    8
ST_TX_AUDIO         .set    12
ST_DETECT           .set    16          ; u64 detect_iep

    .retain
    .retainrefs
    .global ETH_TX
    .sect   ".text"

ETH_TX:
    ldi     r31.b2, ICSS_RX_RESET_BITS  ; as in the HIL example
    ldi     r31.b3, ICSS_TX_FIFO_RESET
    nop
    nop
    ldi32   r29, ICSS_IEP_LOCAL
    ldi     r18, TX_BUF
    zero    &r23, 20                    ; R23..R27
    sbco    &r27, c28, ETH1_TX_PONGS, 4
    ldi     r26, ST_BASE
    zero    &r2, 24
    sbbo    &r2, r26, 0, 24

idle:
    lbco    &r22.w0, c28, ETH1_MAC_VALID, 2
    qbeq    idle, r22.w0, 0
    jal     r28.w2, audio
    lbco    &r22, c28, ETH1_PING_REQ, 4
    qbne    do_pong, r22, 0
    lbco    &r22, c28, ETH1_REPORT_REQ, 4
    qbne    do_report, r22, 0
    lbco    &r22, c28, ETH1_TPUT_TX_COUNT, 4
    qbne    do_tput, r22, 0
    qba     idle

; ---- PONG ---------------------------------------------------------------
do_pong:
    lbco    &r12, c28, ETH1_PING_PC_NS, 16      ; R12/R13 pc_ns, R14/R15 rx_iep
    sub     r16, r22, 1                         ; ping seq
    ldi     r22, 0
    sbco    &r22, c28, ETH1_PING_REQ, 4         ; cleared before the send
    ldi     r20.b0, ETH_T_PONG
    ldi     r20.w2, PONG_LEN
    mov     r21, r27
    jal     r28.w0, build_header
    mov     r2, r16                             ; ping_seq
    ldi     r3, 0
    mov     r4, r12                             ; pc_ns
    mov     r5, r13
    mov     r6, r14                             ; rx_iep
    mov     r7, r15
    zero    &r8, 12                             ; pad to 64 B
    sbbo    &r2, r18, ETH_L2_SIZE + ETH_HDR_SIZE, PONG_LEN
    ldi     r25, 1
    ldi     r20.b1, 0
    jal     r28.w0, send_frame
    add     r27, r27, 1
    sbco    &r27, c28, ETH1_TX_PONGS, 4
    qba     idle

; ---- TPUT_REPORT --------------------------------------------------------
do_report:
    ldi     r22, 0
    sbco    &r22, c28, ETH1_REPORT_REQ, 4
    lbco    &r10, c28, ETH1_TPUT_RX_FRAMES, 32  ; = TPUT_REPORT body layout
    ldi     r20.b0, ETH_T_TPUT_REPORT
    ldi     r20.w2, REPORT_LEN
    ldi     r21, 0
    jal     r28.w0, build_header
    sbbo    &r10, r18, ETH_L2_SIZE + ETH_HDR_SIZE, 32
    zero    &r2, 4
    sbbo    &r2, r18, 62, 2                     ; pad to 64 B
    ldi     r25, 1
    ldi     r20.b1, 0
    jal     r28.w0, send_frame
    qba     idle

; ---- TPUT_DATA burst ----------------------------------------------------
do_tput:
    mov     r24, r22
    ldi     r23, 0
    lbco    &r22, c28, ETH1_TPUT_TX_LEN, 4
    qbeq    small_loop, r22, SMALL_ARG
tput_loop:
    jal     r28.w2, audio               ; a new half goes out first
    ldi     r20.b0, ETH_T_TPUT_DATA
    ldi     r20.w2, TPUT_LEN
    mov     r21, r23
    jal     r28.w0, build_header
    ldi     r25, TPUT_CHUNKS
    ldi     r20.b1, TPUT_TAIL
    jal     r28.w0, send_frame
    add     r23, r23, 1
    sub     r24, r24, 1
    qbne    tput_loop, r24, 0
    qba     tput_end

small_loop:                             ; 60-byte frames: one XOUT, no chunks
    jal     r28.w2, audio
    ldi     r20.b0, ETH_T_TPUT_DATA
    ldi     r20.w2, SMALL_LEN
    mov     r21, r23
    jal     r28.w0, build_header
    ldi     r25, 0
    ldi     r20.b1, SMALL_ARG
    jal     r28.w0, send_frame
    add     r23, r23, 1
    sub     r24, r24, 1
    qbne    small_loop, r24, 0
tput_end:
    ldi     r22, 0
    sbco    &r22, c28, ETH1_TPUT_TX_COUNT, 4
    qba     idle

; ---- audio: send the newest PCM half if HALF_SEQ changed ----------------
; Returns through R28.w2. Uses R0..R22, R25, R26 (not R23, R24, R27).
audio:
    ldi32   r22, PDM0_SHARED_GLOBAL
    lbbo    &r20, r22, PDM8_HDR_HALF_SEQ, 4
    ldi     r26, ST_BASE
    lbbo    &r21, r26, ST_LAST_HALF, 4
    qbne    audio_new, r20, r21
    jmp     r28.w2
audio_new:
    lbbo    &r0, r29, ICSS_IEP_COUNT_LO, 8      ; detect_iep
    sbbo    &r0, r26, ST_DETECT, 8
    sbbo    &r20, r26, ST_LAST_HALF, 4
    qbeq    audio_hdr, r21, 0                   ; first half after start
    sub     r21, r20, r21
    qbeq    audio_hdr, r21, 1
    qbeq    audio_hdr, r20, 0                   ; ICSSG0 restarted
    lbbo    &r21, r26, ST_TX_LATE, 4
    add     r21, r21, 1
    sbbo    &r21, r26, ST_TX_LATE, 4
    sbco    &r21, c28, ETH1_TX_LATE, 4
audio_hdr:
    qbne    audio_rd, r20, 0
    jmp     r28.w2
audio_rd:
    ; R10 CLIP_FRAMES, R11 HALF_SEQ, R12 UNDERRUN, R13 BATCH, R14 FRAME_POS,
    ; R15 LOOPS, R16 VERSION, R17 SD_OVF (header 0x14..0x33)
    lbbo    &r10, r22, PDM8_HDR_CLIP_FRAMES, 32
    lbbo    &r2, r22, PDM8_HDR_RTU1_FRAME_POS, 8
    qbne    audio_rd, r2, r14
    qbne    audio_rd, r3, r15
    sub     r2, r20, 1
    lsl     r2, r2, 6                           ; first_index of part 0
    mov     r3, r20                             ; half_seq
    mov     r4, r15                             ; rtu1_loops
    mov     r5, r14                             ; rtu1_frame_pos
    mov     r6, r10                             ; clip_frames
    mov     r7, r12                             ; underrun_count
    mov     r8, r17                             ; sd_ovf_count
    mov     r9, r13                             ; batch_count
    lbco    &r10, c28, ETH1_RX_FRAMES, 20       ; rx_frames .. last_in_end
    lbbo    &r15, r26, ST_TX_LATE, 4
    lbbo    &r16, r26, ST_DETECT, 8
    sbbo    &r2, r18, OFF_BODY, ETH_ST_SIZE
    ; source of part 0: half (HALF_SEQ - 1) & 1
    sub     r22, r20, 1
    and     r22, r22, 1
    lsl     r22, r22, 11                        ; x 0x800
    ldi32   r0, PDM0_HALF_A_GLOBAL
    add     r22, r22, r0
audio_part:
    ; copy 1024 B: R22 source (global), R25 destination, R21 bursts left
    ldi     r25, OFF_PCM
    add     r25, r25, r18
    ldi     r21, PART_BYTES / 64
audio_copy:
    lbbo    &r2, r22, 0, 64
    sbbo    &r2, r25, 0, 64
    add     r22, r22, 64
    add     r25, r25, 64
    sub     r21, r21, 1
    qbne    audio_copy, r21, 0
    ; header: seq from the state
    lbbo    &r21, r26, ST_AUDIO_SEQ, 4
    ldi     r20.b0, ETH_T_AUDIO_OUT
    ldi     r20.w2, AUDIO_LEN
    jal     r28.w0, build_header
    add     r21, r21, 1
    sbbo    &r21, r26, ST_AUDIO_SEQ, 4
    ldi     r25, AUDIO_CHUNKS
    ldi     r20.b1, AUDIO_TAIL
    jal     r28.w0, send_frame
    ldi     r26, ST_BASE
    lbbo    &r21, r26, ST_TX_AUDIO, 4
    add     r21, r21, 1
    sbbo    &r21, r26, ST_TX_AUDIO, 4
    sbco    &r21, c28, ETH1_TX_AUDIO, 4
    ; part 0 or 1? first_index (status word 0) has bit 5 set for part 1
    lbbo    &r2, r18, OFF_BODY, 4
    qbbs    audio_done, r2, 5
    ; part 1: first_index + 32, next 1 KB of the half (R22 already there)
    add     r2, r2, ETH_AO_FRAMES
    sbbo    &r2, r18, OFF_BODY, 4
    qba     audio_part
audio_done:
    ; pause (AUDIO_TX_GUARD x 2 cycles): the frame that followed the
    ; second AUDIO_OUT at once sometimes never left as a valid frame
    ldi     r21, AUDIO_TX_GUARD
audio_guard:
    sub     r21, r21, 1
    qbne    audio_guard, r21, 0
    jmp     r28.w2

; ---- build_header: frame bytes 0..29 at TX_BUF --------------------------
; In: R20.b0 type, R20.w2 length, R21 seq. Uses R0, R1, R2..R9.
build_header:
    lbco    &r2, c28, ETH1_PC_MAC, 8            ; R2, R3.w0 = destination
    ldi     r3.w2, SRC_MAC_01
    ldi32   r4, SRC_MAC_25
    ldi     r5.w0, ETHERTYPE_LE
    ldi     r5.b2, ETH_VERSION
    mov     r5.b3, r20.b0
    mov     r6.w0, r20.w2                       ; length
    mov     r6.w2, r21.w0                       ; seq, frame bytes 18..21
    mov     r7.w0, r21.w2
    lbbo    &r0, r29, ICSS_IEP_COUNT_LO, 8      ; ts, frame bytes 22..29
    mov     r7.w2, r0.w0
    mov     r8.w0, r0.w2
    mov     r8.w2, r1.w0
    mov     r9.w0, r1.w2
    sbbo    &r2, r18, 0, ETH_L2_SIZE + ETH_HDR_SIZE
    jmp     r28.w0

; ---- send_frame: R25 chunks of 64 B, then a tail of R20.b1 bytes --------
send_frame:
    mov     r26, r18
sf_chunk:
    qbeq    sf_tail, r25, 0
    lbbo    &r2, r26, 0, 64
sf_wait1:
    xin     ICSS_TXL2_XID, &r19, 4
    qbne    sf_wait1, r19.b2, 0
    xout    ICSS_TXL2_XID, &r2, 64
    add     r26, r26, 64
    sub     r25, r25, 1
    qba     sf_chunk
sf_tail:
    qbeq    sf_end, r20.b1, 0
    lbbo    &r2, r26, 0, 64
sf_wait2:
    xin     ICSS_TXL2_XID, &r19, 4
    qbne    sf_wait2, r19.b2, 0
    qbeq    sf_tail30, r20.b1, 30
    qbeq    sf_tail60, r20.b1, SMALL_ARG
    xout    ICSS_TXL2_XID, &r2, 42
    qba     sf_end
sf_tail60:
    xout    ICSS_TXL2_XID, &r2, 60
    qba     sf_end
sf_tail30:
    xout    ICSS_TXL2_XID, &r2, 30
sf_end:
    xin     ICSS_TXL2_XID, &r19, 4
    qbne    sf_end, r19.b2, 0
    loop    sf_nop1, 10
    nop
sf_nop1:
    ldi     r31.w2, ICSS_TX_CRC_EOF
    loop    sf_nop2, TX_EOF_GAP
    nop
sf_nop2:
    jmp     r28.w0
