/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
 */

/*
 * eth_proto.h - C mirror of firmware/eth_proto.inc (frame protocol and ICSSG1
 * shared-RAM map of pdm_audio_eth). Checked against the .inc and the PC
 * application by pc_app/tests/proto_mirror.rs.
 */

#ifndef ETH_PROTO_H_
#define ETH_PROTO_H_

#define ETH_ETHERTYPE               (0x88B5U)
#define ETH_VERSION                 (1U)
#define ETH_L2_SIZE                 (14U)
#define ETH_HDR_SIZE                (16U)
#define ETH_PAYLOAD_MAX             (1500U)
#define ETH_FRAME_MAX               (1514U)
#define ETH_FRAME_MIN               (60U)
#define ETH_BOARD_MAC_0             (0x02U)
#define ETH_BOARD_MAC_1             (0x00U)
#define ETH_BOARD_MAC_2             (0x5DU)
#define ETH_BOARD_MAC_3             (0x4DU)
#define ETH_BOARD_MAC_4             (0x00U)
#define ETH_BOARD_MAC_5             (0x01U)

/* common header, offsets from the payload start */
#define ETH_HDR_VERSION             (0U)
#define ETH_HDR_TYPE                (1U)
#define ETH_HDR_LENGTH              (2U)
#define ETH_HDR_SEQ                 (4U)
#define ETH_HDR_TS                  (8U)

/* frame types */
#define ETH_T_PING                  (1U)
#define ETH_T_PONG                  (2U)
#define ETH_T_TPUT_DATA             (3U)
#define ETH_T_TPUT_REPORT           (4U)
#define ETH_T_AUDIO_IN              (5U)
#define ETH_T_AUDIO_OUT             (6U)
#define ETH_T_CONTROL               (7U)

/* bodies, offsets from the body start (payload + ETH_HDR_SIZE) */
#define ETH_PING_PC_NS              (0U)
#define ETH_PING_SIZE               (8U)
#define ETH_PONG_PING_SEQ           (0U)
#define ETH_PONG_PC_NS              (8U)
#define ETH_PONG_RX_IEP             (16U)
#define ETH_PONG_SIZE               (24U)
#define ETH_TR_FRAMES               (0U)
#define ETH_TR_SEQ_GAPS             (4U)
#define ETH_TR_BYTES                (8U)
#define ETH_TR_FIRST_IEP            (16U)
#define ETH_TR_LAST_IEP             (24U)
#define ETH_TR_SIZE                 (32U)
#define ETH_AI_FIRST_INDEX          (0U)
#define ETH_AI_PCM                  (8U)
#define ETH_AI_FRAMES               (64U)
#define ETH_AI_SIZE                 (1032U)
#define ETH_AO_STATUS               (0U)
#define ETH_AO_PCM                  (64U)
#define ETH_AO_FRAMES               (32U)
#define ETH_AO_SIZE                 (1088U)
#define ETH_CTL_CMD                 (0U)
#define ETH_CTL_ARG0                (4U)
#define ETH_CTL_ARG1                (8U)
#define ETH_CTL_SIZE                (12U)
#define ETH_CMD_RESET_COUNTERS      (1U)
#define ETH_CMD_TPUT_TX             (2U)
#define ETH_CMD_TPUT_REPORT_REQ     (3U)

/* AUDIO_OUT status block, offsets from the block start */
#define ETH_ST_FIRST_INDEX          (0U)
#define ETH_ST_HALF_SEQ             (4U)
#define ETH_ST_RTU1_LOOPS           (8U)
#define ETH_ST_RTU1_FRAME_POS       (12U)
#define ETH_ST_CLIP_FRAMES          (16U)
#define ETH_ST_UNDERRUN_COUNT       (20U)
#define ETH_ST_SD_OVF_COUNT         (24U)
#define ETH_ST_BATCH_COUNT          (28U)
#define ETH_ST_RX_FRAMES            (32U)
#define ETH_ST_RX_AUDIO             (36U)
#define ETH_ST_RX_FOREIGN           (40U)
#define ETH_ST_RX_BAD               (44U)
#define ETH_ST_LAST_IN_END          (48U)
#define ETH_ST_TX_LATE              (52U)
#define ETH_ST_DETECT_IEP           (56U)
#define ETH_ST_SIZE                 (64U)

/* ICSSG1 shared RAM (local 0x10000, global 0x30090000) */
#define ETH1_SHARED_LOCAL           (0x10000U)
#define ETH1_SHARED_GLOBAL          (0x30090000U)
#define ETH1_MAGIC_VALUE            (0x31485445U)
#define ETH1_VERSION_VALUE          (1U)
#define ETH1_MAGIC                  (0x00U)
#define ETH1_VERSION                (0x04U)
#define ETH1_PC_MAC                 (0x08U)
#define ETH1_MAC_VALID              (0x0EU)
#define ETH1_RX_FRAMES              (0x10U)
#define ETH1_RX_AUDIO               (0x14U)
#define ETH1_RX_FOREIGN             (0x18U)
#define ETH1_RX_BAD                 (0x1CU)
#define ETH1_LAST_IN_END            (0x20U)
#define ETH1_PING_REQ               (0x24U)
#define ETH1_PING_PC_NS             (0x28U)
#define ETH1_PING_RX_IEP            (0x30U)
#define ETH1_TPUT_TX_COUNT          (0x38U)
#define ETH1_TPUT_TX_LEN            (0x3CU)
#define ETH1_TPUT_RX_FRAMES         (0x40U)
#define ETH1_TPUT_RX_GAPS           (0x44U)
#define ETH1_TPUT_RX_BYTES          (0x48U)
#define ETH1_TPUT_RX_FIRST_IEP      (0x50U)
#define ETH1_TPUT_RX_LAST_IEP       (0x58U)
#define ETH1_REPORT_REQ             (0x60U)
#define ETH1_TX_AUDIO               (0x64U)
#define ETH1_TX_PONGS               (0x68U)
#define ETH1_TX_LATE                (0x6CU)
#define ETH1_CLIP_ADDR              (0x70U)
#define ETH1_CLIP_FRAMES            (0x74U)
#define ETH1_SIZE                   (0x78U)

#endif /* ETH_PROTO_H_ */
