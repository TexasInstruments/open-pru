; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
;
; eth_tx.asm - ICSSG1 PRU0 of pdm_audio_eth: Gigabit Ethernet
; transmitter (Task 5).
; Stub: halts. The R5F loads and starts it, so the bring-up of Task 3 can
; be checked on its own.

    .include "eth_proto.inc"

    .retain
    .retainrefs
    .global ETH_TX
    .sect   ".text"

ETH_TX:
    halt
