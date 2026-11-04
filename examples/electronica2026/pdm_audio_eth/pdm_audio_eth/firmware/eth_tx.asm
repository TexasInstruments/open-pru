; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
;
; eth_tx.asm - ICSSG1 PRU0 of pdm_audio_eth: Gigabit Ethernet transmitter on
; port 1 (RGMII2), docs/design.md 5.2, frame layout in eth_proto.inc.
;
; Nothing is sent until PRU1 has seen a valid frame from the PC (mac_valid);
; every frame goes to that PC MAC. The main loop polls the ICSSG1 shared RAM
; in this order:
;   PING_REQ       -> PONG (ping seq, echoed pc_ns, PRU1's RX IEP time; the
;                     header ts is the TX IEP time), TX_PONGS++
;   REPORT_REQ     -> TPUT_REPORT (copy of the TPUT_RX block)
;   TPUT_TX_COUNT  -> that many TPUT_DATA frames back to back, seq 0..n-1
;
; Frames are built in DRAM0 at TX_BUF (local 0x0000) and sent through the
; TX_L2 FIFO as in the HIL example: per 64-byte chunk LBBO, wait until the
; FIFO level (R19.b2 after XIN 40) is 0, XOUT 64; the tail with a fixed
; XOUT size (XOUT sizes are immediates); then 10 NOPs, CRC + EOF, 100 NOPs.
; Frame sizes are therefore fixed: PONG 64 B, TPUT_REPORT 64 B, TPUT_DATA
; 1514 B (CONTROL arg1 is ignored), AUDIO_OUT 1118 B (Task 10).
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
;   R28.w0    return address (jal)
;   R29       IEP0 base (local)
;   C28       ICSSG1 shared RAM (local 0x10000, set by the R5F)

    .include "eth_proto.inc"

TX_BUF              .set    0x0000      ; DRAM0, 1536 B frame buffer
SRC_MAC_01          .set    0x0002      ; frame bytes 6, 7  = 02 00
SRC_MAC_25          .set    0x01004D5D  ; frame bytes 8..11 = 5D 4D 00 01
ETHERTYPE_LE        .set    0xB588      ; frame bytes 12, 13 = 88 B5
PONG_LEN            .set    34          ; 64 - 14 - 16 (24 B body + pad)
REPORT_LEN          .set    34          ; 64 - 14 - 16 (32 B body + pad)
TPUT_LEN            .set    1484        ; 1514 - 14 - 16
TPUT_CHUNKS         .set    23          ; 1514 = 23 x 64 + 42
TPUT_TAIL           .set    42

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

idle:
    lbco    &r22.w0, c28, ETH1_MAC_VALID, 2
    qbeq    idle, r22.w0, 0
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
tput_loop:
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
    ldi     r22, 0
    sbco    &r22, c28, ETH1_TPUT_TX_COUNT, 4
    qba     idle

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
    xout    ICSS_TXL2_XID, &r2, 42
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
    loop    sf_nop2, 100
    nop
sf_nop2:
    jmp     r28.w0
