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
;   File:     pruemif16_emif_addr_cfg.inc
;
;   Brief:    Configuration constants for EMIF16 PRU address interface
;             Defines pin mappings for address bus and control signals
;
;************************************************************************************

    .if !$isdefed("__pruemif16_emif_addr_cfg_h_")
__pruemif16_emif_addr_cfg_h_    .set    1

;*****************************************************************************
; EMIF Control Signal Pin Definitions
; Brief: PRU GPIO pin assignments for EMIF control signals
; Note:  All pin numbers refer to ICSSG PRU GPIO pins
;*****************************************************************************
EM_CSn_PIN              .set    19      ; Chip Select, active low
EM_OEn_PIN              .set    18      ; Output Enable (Read), active low
EM_WEn_PIN              .set    17      ; Write Enable (Write), active low

;*****************************************************************************
; EMIF Address Bus Configuration
; Brief: Address bus pin assignments and mask configuration
; Note:  Address bits must be in contiguous range of PRU pins
;        Address configuration is byte-addressed
;*****************************************************************************
EM_A0_PIN               .set    0       ; Address LSB pin number
EM_A_MASK               .set    ((1<<EM_A_NBITS) - 1) << EM_A0_PIN

    .endif ; __pruemif16_emif_addr_cfg_h_
