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
;   File:     pruemif16_cfg.inc
;
;   Brief:    Constant defines
;************************************************************************************

    .if !$isdefed("__pruemif16_cfg_h_")
__pruemif16_cfg_h_      .set    1

; SPAD address sharing flag
EMIF_ADDR_VALID_SHIFT       .set    0
EMIF_ADDR_VALID_RESET_VAL   .set    ( 0x0 << EMIF_ADDR_VALID_SHIFT )

; SPAD shared address
EMIF_ADDR_SHIFT             .set    0
EMIF_ADDR_RESET_VAL         .set    ( 0x7FFF << EMIF_ADDR_SHIFT )


SH_EM_A_BANK                .set    PRU_SPAD_B0_XID ; SPAD bank used for sharing address
EM_A_NBITS                  .set    9   ; EMIF address number of bits

    .endif ; __pruemif16_cfg_h_
