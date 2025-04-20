;*****************************************************************************
; Copyright (C) 2024 Texas Instruments Incorporated - http://www.ti.com/
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
; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;
;*****************************************************************************
;   File:     pruemif16_emif_data_cfg.inc
;
;   Brief:    Configuration constants for EMIF16 PRU interface
;
;*****************************************************************************
    .if !$isdefed("__pruemif16_emif_data_cfg_h_")
__pruemif16_emif_data_cfg_h_      .set    1

; EMIF control signals
;*****************************************************************************
; Brief: Pin definitions for EMIF control signals
; Note:  All pin numbers refer to ICSSG PRU GPIO pins
;*****************************************************************************

EM_CSn_PIN              .set    16      ; Chip Select, active low
EM_OEn_PIN              .set    14      ; Output Enable (Read), active low
EM_WEn_PIN              .set    13      ; Write Enable, active low
EM_WAITn_PIN            .set    15      ; Wait signal, active low

; EMIF data configuration
;*****************************************************************************
; Brief: Data bus pin and width configuration
; Note:  Address bits must be in contiguous range of PRU pins
;*****************************************************************************

EM_D0_PIN               .set    0       ; Data LSB pin number
EM_D_NBITS              .set    13      ; Data bus width
EM_D_MASK               .set    ((1<<EM_D_NBITS) - 1) << EM_D0_PIN

; ICSSG Control Register addresses
;*****************************************************************************
; Brief: Control register addresses for AM243x SoC
;*****************************************************************************

CTRLMMR_ICSSG0_CTRL0    .set    0x43004100  ; ICSSG0 PRU0 control
CTRLMMR_ICSSG0_CTRL1    .set    0x43004104  ; ICSSG0 PRU1 control
CTRLMMR_ICSSG1_CTRL0    .set    0x43004110  ; ICSSG1 PRU0 control
CTRLMMR_ICSSG1_CTRL1    .set    0x43004114  ; ICSSG1 PRU1 control
CTRLMMR_ICSSG_CTRL      .set    CTRLMMR_ICSSG0_CTRL1

; ICSSG IEP base addresses
;*****************************************************************************
; Brief: IEP module base addresses
;*****************************************************************************

ICSSG0_IEP0_DIGIO_CTRL  .set    0x3002E300
ICSSG0_IEP1_DIGIO_CTRL  .set    0x3002F300
ICSSG1_IEP0_DIGIO_CTRL  .set    0x300AE300
ICSSG1_IEP1_DIGIO_CTRL  .set    0x300AF300
ICSSG_IEP_DIGIO_CTRL    .set    ICSSG0_IEP1_DIGIO_CTRL

; Shared memory configuration
SHMEM_BUF               .set    0x30010000

    .endif ; __pruemif16_emif_data_cfg_h_
