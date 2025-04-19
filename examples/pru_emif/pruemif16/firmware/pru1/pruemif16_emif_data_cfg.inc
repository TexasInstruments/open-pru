; Copyright (C) 2025 Texas Instruments Incorporated - http://www.ti.com/
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions
; are met:
;
; Redistributions of source code must retain the above copyright
; notice, this list of conditions and the following disclaimer.
;
; Redistributions in binary form must reproduce the above copyright
; notice, this list of conditions and the following disclaimer in the
; documentation and/or other materials provided with the
; distribution.
;
; Neither the name of Texas Instruments Incorporated nor the names of
; its contributors may be used to endorse or promote products derived
; from this software without specific prior written permission.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
; "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
; LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
; A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
; OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
; SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
; LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
; DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
; THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE

;************************************************************************************
;   File:     pruemif16_emif_data_cfg.inc
;
;   Brief:    Constant defines
;************************************************************************************

    .if !$isdefed("__pruemif16_emif_data_cfg_h_")
__pruemif16_emif_data_cfg_h_      .set    1

; EMIF control signals
EM_CSn_PIN              .set    16  ; ICSSG PRU GPI pin number. EMIF Chip Select pin number, active low.
EM_OEn_PIN              .set    14  ; ICSSG PRU GPI pin number. EMIF Output Enable (Read) pin number, active low.
EM_WEn_PIN              .set    13  ; ICSSG PRU GPI pin number. EMIF Write Enable (Write) pin number, active low.
EM_WAITn_PIN            .set    15  ; ICSSG PRU GPO pin number. EMIF Wait pin number, active low.

; EMIF data
;   EMIF address LSB can be any PRU pin number.
;   It is assumed the address bits are in a contiguous range of PRU pins.
EM_D0_PIN               .set    0   ; ICSSG PRU GPI/GPO pin number. EMIF data least-significant bit.
EM_D_NBITS              .set    13  ; EMIF data number of bits
EM_D_MASK               .set    ((1<<EM_D_NBITS) - 1) << EM_D0_PIN

; ICSSG Control Register address  ( Specific to am243x SoC )

CTRLMMR_ICSSG0_CTRL0    .set    0x43004100  ; PRU0
CTRLMMR_ICSSG0_CTRL1    .set    0x43004104  ; PRU1
CTRLMMR_ICSSG1_CTRL0    .set    0x43004110  ; PRU0
CTRLMMR_ICSSG1_CTRL1    .set    0x43004114  ; PRU1
CTRLMMR_ICSSG_CTRL      .set    CTRLMMR_ICSSG0_CTRL1

; ICSSG IEP base address
ICSSG0_IEP0_DIGIO_CTRL  .set    0x3002E300
ICSSG0_IEP1_DIGIO_CTRL  .set    0x3002F300
ICSSG1_IEP0_DIGIO_CTRL  .set    0x300AE300
ICSSG1_IEP1_DIGIO_CTRL  .set    0x300AF300
ICSSG_IEP_DIGIO_CTRL    .set    ICSSG0_IEP1_DIGIO_CTRL
SHMEM_BUF               .set    0x30010000

    .endif ; __pruemif16_emif_data_cfg_h_
