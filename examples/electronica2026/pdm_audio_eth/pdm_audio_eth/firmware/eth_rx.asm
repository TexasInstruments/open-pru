; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
;
; eth_rx.asm - ICSSG1 PRU1 of pdm_audio_eth: Gigabit Ethernet
; receiver (Task 4).
; Stub: halts. The R5F loads and starts it, so the bring-up of Task 3 can
; be checked on its own.

    .include "eth_proto.inc"

    .retain
    .retainrefs
    .global ETH_RX
    .sect   ".text"

ETH_RX:
    halt
