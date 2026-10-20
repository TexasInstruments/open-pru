; File: jpeg_pru0_map.inc

; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions
; are met:
;
;        * Redistributions of source code must retain the above copyright
;          notice, this list of conditions and the following disclaimer.
;
;        * Redistributions in binary form must reproduce the above copyright
;          notice, this list of conditions and the following disclaimer in the
;          documentation and/or other materials provided with the
;          distribution.
;
;        * Neither the name of Texas Instruments Incorporated nor the names of
;          its contributors may be used to endorse or promote products derived
;          from this software without specific prior written permission.
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
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; PRU0 data RAM layout shared by the PRU0 decoder and the R5F host
; (host/mcuplus/jpeg_host.c defines the same offsets). All addresses are PRU0
; local addresses; the host reaches them at ICSS base + the same offset.
;
; 0x0040..0x083F  Huffman lookup tables, 512 B each: DC0 0x040, DC1 0x240,
;                 AC0 0x440, AC1 0x640 (host-written)
; 0x0D40..0x0D7F  zigzag table, 64 B (host-written)
; 0x1000..0x102F  control block (host-written, CTRL_* offsets below)
; 0x1030          current restart segment index (PRU0 scratch)
; 0x1100..0x1107  error code and bit count (PRU0 writes on a decode error)
; 0x1300..0x132F  component descriptors, 16 B each (host-written)
; 0x1400..0x140B  DC predictors, one u32 per component (PRU0)
; Dual-core build:   0x1600 register spill (DUAL_SAVE), 0x1700 128 B staging
;                    block (DUAL_STAGE)
; Single-core build: 0x1100 dequantized block (also holds the error code,
;                    which is valid only when PRU0 halts without CTRL_DONE),
;                    0x1500 IDCT workspace, 0x1600 IDCT output, 0x1700
;                    coefficient block, 0x1800 register spill, 0x1870 output
;                    pointer, 0x1900 quantization tables (host-written)

CTRL_BASE       .set 0x1000
CTRL_SCAN       .set 0       ; u32 scan start (informational)
CTRL_SCAN_LEN   .set 4       ; u32 scan length in bytes (informational)
CTRL_DONE       .set 8       ; u32 completion word: PRU0 writes 1 when done
CTRL_BLOCKS     .set 12      ; u32 number of 8x8 blocks in the frame
CTRL_BLOCK_COMP .set 20      ; u32 address of the per-block component table
CTRL_BLOCK_RST  .set 24      ; u32 address of the per-block restart flag table
CTRL_COMP       .set 28      ; u32 address of the component descriptors
CTRL_DC_PRED    .set 32      ; u32 address of the DC predictors
CTRL_ZIGZAG     .set 36      ; u32 address of the zigzag table
CTRL_SEGMENTS   .set 40      ; u32 address of the restart segment table
SEG_INDEX       .set 0x1030
DIAG_CODE       .set 0x1100  ; u8 error code
DIAG_BITS       .set 0x1104  ; u32 real bits left in the bit window
ERR_TRUNCATED   .set 0x12    ; a symbol needed bits past the end of the scan
ERR_INVALID_CODE .set 0x13   ; no Huffman code matched real scan bits

; Single-core stage buffers (single_core_body.inc)
S_DEQ           .set 0x1100
S_WS            .set 0x1500
S_IDCT_OUT      .set 0x1600
S_COEF          .set 0x1700
S_SAVE          .set 0x1800
S_OUTPTR        .set 0x1870
S_QTAB          .set 0x1900
