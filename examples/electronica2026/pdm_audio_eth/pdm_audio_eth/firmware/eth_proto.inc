;
;  SPDX-License-Identifier: BSD-3-Clause
;  Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
;
;  eth_proto.inc - frame protocol and ICSSG1 shared-RAM map of pdm_audio_eth
;  (docs/design.md section 6, docs/plan.md "Protocol"). Mirrored in
;  mcuplus/eth_proto.h and ../pc_app/src/proto.rs; pc_app/tests/proto_mirror.rs
;  checks that every ETH_* / ETH1_* name has the same value in all three.
;  All fields after the EtherType are little-endian. Plain literals only.
;

ETH_ETHERTYPE               .set    0x88B5
ETH_VERSION                 .set    1
ETH_L2_SIZE                 .set    14
ETH_HDR_SIZE                .set    16
ETH_PAYLOAD_MAX             .set    1500
ETH_FRAME_MAX               .set    1514
ETH_FRAME_MIN               .set    60
ETH_BOARD_MAC_0             .set    0x02
ETH_BOARD_MAC_1             .set    0x00
ETH_BOARD_MAC_2             .set    0x5D
ETH_BOARD_MAC_3             .set    0x4D
ETH_BOARD_MAC_4             .set    0x00
ETH_BOARD_MAC_5             .set    0x01

; --- common header, offsets from the payload start
ETH_HDR_VERSION             .set    0
ETH_HDR_TYPE                .set    1
ETH_HDR_LENGTH              .set    2
ETH_HDR_SEQ                 .set    4
ETH_HDR_TS                  .set    8

; --- frame types
ETH_T_PING                  .set    1
ETH_T_PONG                  .set    2
ETH_T_TPUT_DATA             .set    3
ETH_T_TPUT_REPORT           .set    4
ETH_T_AUDIO_IN              .set    5
ETH_T_AUDIO_OUT             .set    6
ETH_T_CONTROL               .set    7

; --- bodies, offsets from the body start (payload + ETH_HDR_SIZE)
ETH_PING_PC_NS              .set    0
ETH_PING_SIZE               .set    8
ETH_PONG_PING_SEQ           .set    0
ETH_PONG_PC_NS              .set    8
ETH_PONG_RX_IEP             .set    16
ETH_PONG_SIZE               .set    24
ETH_TR_FRAMES               .set    0
ETH_TR_SEQ_GAPS             .set    4
ETH_TR_BYTES                .set    8
ETH_TR_FIRST_IEP            .set    16
ETH_TR_LAST_IEP             .set    24
ETH_TR_SIZE                 .set    32
ETH_AI_FIRST_INDEX          .set    0
ETH_AI_PCM                  .set    8
ETH_AI_FRAMES               .set    64
ETH_AI_SIZE                 .set    1032
ETH_AO_STATUS               .set    0
ETH_AO_PCM                  .set    64
ETH_AO_FRAMES               .set    32
ETH_AO_SIZE                 .set    1088
ETH_CTL_CMD                 .set    0
ETH_CTL_ARG0                .set    4
ETH_CTL_ARG1                .set    8
ETH_CTL_SIZE                .set    12
ETH_CMD_RESET_COUNTERS      .set    1
ETH_CMD_TPUT_TX             .set    2
ETH_CMD_TPUT_REPORT_REQ     .set    3

; --- AUDIO_OUT status block, offsets from the block start
ETH_ST_FIRST_INDEX          .set    0
ETH_ST_HALF_SEQ             .set    4
ETH_ST_RTU1_LOOPS           .set    8
ETH_ST_RTU1_FRAME_POS       .set    12
ETH_ST_CLIP_FRAMES          .set    16
ETH_ST_UNDERRUN_COUNT       .set    20
ETH_ST_SD_OVF_COUNT         .set    24
ETH_ST_BATCH_COUNT          .set    28
ETH_ST_RX_FRAMES            .set    32
ETH_ST_RX_AUDIO             .set    36
ETH_ST_RX_FOREIGN           .set    40
ETH_ST_RX_BAD               .set    44
ETH_ST_LAST_IN_END          .set    48
ETH_ST_TX_LATE              .set    52
ETH_ST_DETECT_IEP           .set    56
ETH_ST_SIZE                 .set    64

; --- ICSSG1 shared RAM (local 0x10000, global 0x30090000)
ETH1_SHARED_LOCAL           .set    0x10000
ETH1_SHARED_GLOBAL          .set    0x30090000
ETH1_MAGIC_VALUE            .set    0x31485445
ETH1_VERSION_VALUE          .set    1
ETH1_MAGIC                  .set    0x00
ETH1_VERSION                .set    0x04
ETH1_PC_MAC                 .set    0x08
ETH1_MAC_VALID              .set    0x0E
ETH1_RX_FRAMES              .set    0x10
ETH1_RX_AUDIO               .set    0x14
ETH1_RX_FOREIGN             .set    0x18
ETH1_RX_BAD                 .set    0x1C
ETH1_LAST_IN_END            .set    0x20
ETH1_PING_REQ               .set    0x24
ETH1_PING_PC_NS             .set    0x28
ETH1_PING_RX_IEP            .set    0x30
ETH1_TPUT_TX_COUNT          .set    0x38
ETH1_TPUT_TX_LEN            .set    0x3C
ETH1_TPUT_RX_FRAMES         .set    0x40
ETH1_TPUT_RX_GAPS           .set    0x44
ETH1_TPUT_RX_BYTES          .set    0x48
ETH1_TPUT_RX_FIRST_IEP      .set    0x50
ETH1_TPUT_RX_LAST_IEP       .set    0x58
ETH1_REPORT_REQ             .set    0x60
ETH1_TX_AUDIO               .set    0x64
ETH1_TX_PONGS               .set    0x68
ETH1_TX_LATE                .set    0x6C
ETH1_CLIP_ADDR              .set    0x70
ETH1_CLIP_FRAMES            .set    0x74
ETH1_SIZE                   .set    0x78

; --- hardware constants (ICSSG MII_RT / IEP, from the HIL example; not mirrored)
ICSS_TXL2_XID               .set    40          ; TX_L2 FIFO XFR ID
ICSS_RXL2_SIDEA             .set    20          ; RX_L2 bank A, XIN 65 B to R2..R18
ICSS_RXL2_SIDEB             .set    21          ; RX_L2 bank B
ICSS_RX_EOF_BIT             .set    20          ; R31 bit: RX end of frame
ICSS_RX_RESET_BITS          .set    0xF4        ; or r31.b2: RX reset, EOF and error clear
ICSS_TX_FIFO_RESET          .set    0x40        ; ldi r31.b3: TX FIFO reset
ICSS_TX_CRC_EOF             .set    0x2C00      ; ldi r31.w2: push CRC MS+LS word, TX EOF
ICSS_IEP_LOCAL              .set    0x2E000     ; IEP0, local address
ICSS_IEP_COUNT_LO           .set    0x0C        ; IEP0 count, ns (CFG 0x331)
ICSS_IEP_COUNT_HI           .set    0x10
PDM0_SHARED_GLOBAL          .set    0x30010000  ; ICSSG0 shared RAM (pdm8_shared.inc header)
PDM0_HALF_A_GLOBAL          .set    0x30010100
PDM0_HALF_B_GLOBAL          .set    0x30010900
