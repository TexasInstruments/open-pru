; File: pru0_decoder.inc
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
; File: pru0_decoder.inc
; JPEG baseline Huffman (entropy) decoder for PRU0, shared by the single-core
; and dual-core projects. Each project's main.asm sets SINGLE_CORE=1 or
; DUAL_PRU=1 (clpru --asm_define) and includes this file.
;
; Bit window: the valid bits are the top r1 bits of r0; the bits below them
; are either zero or the correct following stream bits (a refill ORs a whole
; word at bit r1 but advances the pointer by whole bytes only, so the next
; refill ORs the same bits again). r11 counts the fill bits (0xFF padding
; past the end of the segment) in the window; real bits = r1 - r11.
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly
    .text
    .global main

    .include "jpeg_pru0_map.inc"

    .if $isdefed("DUAL_PRU")
    .else
DUAL_PRU .set 0
    .endif
    .if $isdefed("SINGLE_CORE")
    .else
SINGLE_CORE .set 0
    .endif
    .if DUAL_PRU == SINGLE_CORE
    .emsg "Define exactly one of SINGLE_CORE=1 or DUAL_PRU=1"
    .endif

    .if DUAL_PRU
; Dual-PRU build: this core only runs the scan. Each finished block
; is decoded into a staging buffer in PRU0 DRAM and handed to PRU1 through the
; scratchpad banks (ipc_xfr.inc), together with its component index.
DUAL_SAVE  .set 0x1600      ; spill area for R2..R22 live state (84 B)
DUAL_STAGE .set 0x1700      ; 128 B staging block
    .include "ipc_xfr.inc"
    .endif
; Single-core build (SINGLE_CORE=1): this core runs the complete decoder.
; Each finished block is decoded into a fixed 128 B coefficient buffer in
; DRAM (0x1700) and handed to single_core_body.inc, which runs the
; dequantize, inverse DCT, and output stages before the next block is
; scanned. The host supplies the quantization tables at 0x1900 and the
; output pointer at 0x1870; it does no inter-stage hand-off.

; ---------------------------------------------------------------- macros
; Consume n bits (n is r12/r18/r19), failing 0x12 if that eats padding bits.
; stub restores r1 so the diagnostic matches the pre-consume state.
; Pseudo code: shift window, decrement valid bits, reject padding.
; PEAK / WCET: 3 instruction cycles; modifies R0, R1.
CONSUME .macro n, stub
    lsl r0, r0, n
    sub r1, r1, n
    qbgt stub, r1, r11
    .endm

; Left-aligned word refill (needs r1 < n <= 16 on entry to the refill).
; Fast path: >= 4 real bytes left in the segment -> one 4-byte lbbo, PRU
; byte-swap to big-endian, OR at bit r1, advance by whole bytes.
; Slow path (segment tail): byte loop with 0xFF lookahead fill.
; Pseudo code: if short, refill a word or at most two tail bytes.
; PEAK / local WCET: 64 cycles including tail refill; modifies R0..R7,
; R11, R19. MSRAM load stalls are additional.
ENSURE_L .macro n, setop
    qble done?, r1, n
    sub r5, r3, r2
    qbgt slow?, r5, 4
    lbbo &r5, r2, 0, 4
    mov r6.b3, r5.b0
    mov r6.b2, r5.b1
    mov r6.b1, r5.b2
    mov r6.b0, r5.b3
    lsr r6, r6, r1
    or r0, r0, r6
    rsb r7, r1, 32
    lsr r7, r7, 3
    add r2, r2, r7
    lsl r7, r7, 3
    add r1, r1, r7
    qba done?
slow?:
    setop r6, n
    jal r7, refill_slow
done?:
    .endm

; Canonical search.  The partition header holds the table offset at +2 (u16);
; the table is 8 x {limit u32, off4 u32} for lengths 9..16 where limit is the
; left-justified exclusive upper bound of the length's codes (0 if none) and
; off4 = 4*(valptr - mincode).  Entry address = sec + 4 + 4*(p>>(16-L)) + off4.
; Pseudo code: test at most 8 code lengths, load symbol, consume bits.
; PEAK / local WCET: 96 cycles; modifies R0, R1, R5..R8, R18, R19.
; Secondary tables reside in MSRAM; their stalls are additional.
LONGCANON .macro sec, done
    lbbo &r5, sec, 2, 2
    add r5, sec, r5.w0
    lbbo &r6, r5, 0, 4
    qbgt f0?, r12, r6
    lbbo &r6, r5, 8, 4
    qbgt f1?, r12, r6
    lbbo &r6, r5, 16, 4
    qbgt f2?, r12, r6
    lbbo &r6, r5, 24, 4
    qbgt f3?, r12, r6
    lbbo &r6, r5, 32, 4
    qbgt f4?, r12, r6
    lbbo &r6, r5, 40, 4
    qbgt f5?, r12, r6
    lbbo &r6, r5, 48, 4
    qbgt f6?, r12, r6
    lbbo &r6, r5, 56, 4
    qbgt f7?, r12, r6
    qba fail_long
f0?:
    ldi r7, 7
    lbbo &r6, r5, 4, 4
    qba fc?
f1?:
    ldi r7, 6
    lbbo &r6, r5, 12, 4
    qba fc?
f2?:
    ldi r7, 5
    lbbo &r6, r5, 20, 4
    qba fc?
f3?:
    ldi r7, 4
    lbbo &r6, r5, 28, 4
    qba fc?
f4?:
    ldi r7, 3
    lbbo &r6, r5, 36, 4
    qba fc?
f5?:
    ldi r7, 2
    lbbo &r6, r5, 44, 4
    qba fc?
f6?:
    ldi r7, 1
    lbbo &r6, r5, 52, 4
    qba fc?
f7?:
    ldi r7, 0
    lbbo &r6, r5, 60, 4
fc?:
    lsr r8, r12, r7
    lsl r8, r8, 2
    add r8, r8, r6
    add r8, r8, sec
    lbbo &r18, r8, 4, 2
    mov r19, r18.b0
    mov r18, r18.b1
    CONSUME r19, cstub19
    qba done
    .endm

; Decode one Huffman symbol into r18; codes longer than 8 bits branch to lng.
; lut = 256 x {len<<4, sym} primary table base.
; Pseudo code: ensure 16 bits, lookup prefix, consume or use long path.
; PEAK / local WCET: 192 cycles including LONGCANON; modifies R0..R8,
; R11, R12, R18, R19. External lookup stalls are additional.
SYMDECODE .macro lut, lng
    ENSURE_L 16, ldi
    lsr r12, r0, 24
    lsl r12, r12, 1
    lbbo &r18, lut, r12, 2
    lsr r12, r18.b0, 4
    qbeq lng, r12, 0
    CONSUME r12, cstub12
    mov r18, r18.b1
    .endm

; Extension bits: r12 = size (1..15) -> r8 = sign-extended value, bits consumed.
; Pseudo code: ensure category bits, sign extend, consume.
; PEAK / local WCET: 80 cycles; modifies R0..R8, R11, R19.
EXTVAL .macro
    ENSURE_L r12, mov
    rsb r7, r12, 32
    qbbs pos?, r0, 31
    not r8, r0
    lsr r8, r8, r7
    rsb r8, r8, 0
    qba cons?
pos?:
    lsr r8, r0, r7
cons?:
    CONSUME r12, cstub12
    .endm

; ---------------------------------------------------------------- main
; Frame loop pseudo code: for each block, select component/restart segment,
; decode DC and at most 63 AC symbols, then reconstruct or publish the block.
; PEAK / local entropy WCET bound per block: 64 * (192 + 80 + 64)
; + 512 = 22016 instruction cycles. AC index advances on each iteration
; and is checked against 64 before each coefficient store. Registers R0..R29
; are modified. Add the stage bound or IPC service time for the chosen mode.
; Basis: conservative instruction counting with local memory transfers;
; MSRAM stalls and peer waiting are additional. Only frame totals are measured
; on silicon. JPEG_TIMEOUT_US in the host bounds failed frame recovery.
main:
    ldi r0, 0
    ldi r1, 0
    ; r11 counts the lookahead-fill bits (real bits = r1 - r11).
    ldi r11, 0
    ldi32 r10, CTRL_BASE
    ; Segment bounds come from the host segment table (CTRL+40), entry 0.
    ; The restart path pre-increments the index cell, so entry 0 is read
    ; here and only here; restarts start at entry 1.
    lbbo &r6, r10, CTRL_SEGMENTS, 4
    ldi32 r12, SEG_INDEX
    ldi r13, 0
    sbbo &r13, r12, 0, 4
    lbbo &r2, r6, 0, 4
    lbbo &r3, r6, 4, 4
    lbbo &r5, r10, CTRL_BLOCKS, 4
    lbbo &r21, r10, CTRL_BLOCK_COMP, 4
    lbbo &r22, r10, CTRL_BLOCK_RST, 4
    lbbo &r9, r10, CTRL_COMP, 4
    lbbo &r14, r10, CTRL_DC_PRED, 4
    lbbo &r10, r10, CTRL_ZIGZAG, 4
    mov r15, r5
    ldi r5, 0
    sbbo &r5, r14, 0, 4
    sbbo &r5, r14, 4, 4
    sbbo &r5, r14, 8, 4
    ldi r4, 0
scan_block_loop:
    qbge block_done, r15, r4
    .if DUAL_PRU
    ldi r16, DUAL_STAGE
    .else
    ldi r16, S_COEF
    .endif
    add r12, r21, r4
    lbbo &r17, r12, 0, 1
    and r17, r17, 255
    add r12, r22, r4
    lbbo &r18, r12, 0, 1
    and r18, r18, 255
    qbeq no_restart, r18, 0
    ldi r5, 0
    sbbo &r5, r14, 0, 4
    sbbo &r5, r14, 4, 4
    sbbo &r5, r14, 8, 4
    ldi r0, 0
    ldi r1, 0
    ldi r11, 0
    ; Segment table pointer is CTRL+40; its mutable index lives in DRAM
    ; because R30/R31 are PRU I/O registers, not general persistent state.
    ldi32 r6, SEG_INDEX
    lbbo &r12, r6, 0, 4
    add r12, r12, 1
    sbbo &r12, r6, 0, 4
    lsl r12, r12, 3
    ldi32 r6, CTRL_BASE + CTRL_SEGMENTS
    lbbo &r6, r6, 0, 4
    add r12, r6, r12
    lbbo &r2, r12, 0, 4
    lbbo &r3, r12, 4, 4
no_restart:
    lsl r12, r17, 4
    add r12, r9, r12
    ; Zero the 128 B coefficient block from a zeroed register bank, then
    ; fetch the four table bases (dc lut, ac lut, dc sec, ac sec) in one lbbo.
    zero &r24, 24
    sbbo &r24, r16, 0, 24
    sbbo &r24, r16, 24, 24
    sbbo &r24, r16, 48, 24
    sbbo &r24, r16, 72, 24
    sbbo &r24, r16, 96, 24
    sbbo &r24, r16, 120, 8
    lbbo &r24, r12, 0, 16
    ; DC symbol
    SYMDECODE r24, dc_long
dc_sym_done:
    qbeq dc_store_noext, r18, 0
    mov r12, r18
    EXTVAL
    lsl r13, r17, 2
    add r13, r14, r13
    lbbo &r6, r13, 0, 4
    add r6, r6, r8
    sbbo &r6, r13, 0, 4
    mov r8, r6
    qba dc_store_val
dc_store_noext:
    lsl r13, r17, 2
    add r13, r14, r13
    lbbo &r8, r13, 0, 4
dc_store_val:
    sbbo &r8, r16, 0, 2
    ldi r23, 1
    ; Rotated loop: the k<64 test sits at the bottom, so a continuing
    ; coefficient costs one branch instead of two.
ac_loop:
    SYMDECODE r25, ac_long
ac_sym_done:
    lsr r12, r18, 4
    and r13, r18, 15
    qbeq ac_eob, r13, 0
    add r23, r23, r12
    qble ac_done, r23, 64
    mov r12, r13
    EXTVAL
    lbbo &r7.b0, r10, r23, 1
    lsl r7, r7.b0, 1
    sbbo &r8, r16, r7, 2
    add r23, r23, 1
    qbgt ac_loop, r23, 64
    qba ac_done
ac_eob:
    qbeq ac_done, r12, 0
    qbne ac_done, r12, 15
    add r23, r23, 16
    qbgt ac_loop, r23, 64
    qba ac_done
ac_done:
    .if DUAL_PRU
    ; Send the block. IPC_TX stages through R2..R23, which hold the decoder's live
    ; state (r2 r3 segment, r4 block index, r9 r10 r11, r14 r15, r20..r22), so
    ; spill those to DRAM around it. r16..r18 are dead here (reloaded at the
    ; top of the loop); r26..r29 are reloaded per block too.
    add r26, r4, 1              ; sequence number = block index + 1
    ldi r27, DUAL_STAGE
    mov r28, r17                ; component index -> PRU1 qtable selector
    ldi r29, DUAL_SAVE
    sbbo &r2, r29, 0, 12
    sbbo &r9, r29, 12, 12
    sbbo &r14, r29, 24, 8
    sbbo &r21, r29, 32, 8
    IPC_TX r26, r27, r28
    lbbo &r2, r29, 0, 12
    lbbo &r9, r29, 12, 12
    lbbo &r14, r29, 24, 8
    lbbo &r21, r29, 32, 8
    .else
    jmp single_stage
    .endif
single_return:
    add r4, r4, 1
    qba scan_block_loop
    ; Long-code paths live out of line so the short path never jumps over them.
dc_long:
    lsr r12, r0, 16
    LONGCANON r26, dc_sym_done
ac_long:
    lsr r12, r0, 16
    LONGCANON r27, ac_sym_done
block_done:
    ; Completion flag lets the host distinguish normal end from truncated input.
    ldi32 r5, CTRL_BASE + CTRL_DONE
    ldi r6, 1
    sbbo &r6, r5, 0, 4
    halt
decode_fail:
    ; 0x12 means a symbol/extension required lookahead-fill bits.  Keep
    ; completed clear and expose the count for host truncation diagnostics.
    ; Diagnostics live at 0x1100/0x1104, outside the 11-word control
    ; struct at 0x1000 (whose word at +0x27 they used to clobber).
    sub r11, r1, r11
    ldi32 r5, DIAG_CODE
    ldi r6, ERR_TRUNCATED
    sbbo &r6, r5, 0, 1
    add r5, r5, 4
    sbbo &r11, r5, 0, 4
    halt
fail_long:
    qba invalid_code
invalid_code:
    ; No Huffman code matched the lookahead.  If the bit buffer holds
    ; lookahead-fill bits the mismatch was caused by scan truncation, not a
    ; corrupt table: report 0x12 like the guards.  Only an invalid code in an
    ; all-real buffer is 0x13.
    ldi32 r5, DIAG_CODE
    qbne invalid_code_trunc, r11, 0
    sub r11, r1, r11
    ldi r6, ERR_INVALID_CODE
    qba invalid_code_write
invalid_code_trunc:
    sub r11, r1, r11
    ldi r6, ERR_TRUNCATED
invalid_code_write:
    sbbo &r6, r5, 0, 1
    add r5, r5, 4
    sbbo &r11, r5, 0, 4
    halt
    ; Guard-failure stubs: undo the consume so the diagnostic is pre-consume.
cstub12:
    add r1, r1, r12
    qba decode_fail
cstub18:
    add r1, r1, r18
    qba decode_fail
cstub19:
    add r1, r1, r19
    qba decode_fail
; Left-aligned byte refill for the segment tail.  In: r6 = bits needed,
; link r7.  Clobbers r5, r19.
; PEAK / local WCET: <= 2 byte iterations, <= 48 cycles; see ENSURE_L.
refill_slow:
sl_loop:
    qble sl_done, r1, r6
    qbge sl_fill, r3, r2
    lbbo &r5, r2, 0, 1
    and r5, r5, 255
    add r2, r2, 1
    rsb r19, r1, 24
    lsl r5, r5, r19
    or r0, r0, r5
    add r1, r1, 8
    qba sl_loop
sl_fill:
    ldi r5, 255
    rsb r19, r1, 24
    lsl r5, r5, r19
    or r0, r0, r5
    add r1, r1, 8
    add r11, r11, 8
    qba sl_loop
sl_done:
    jmp r7
    .if SINGLE_CORE
    .include "single_core_body.inc"
    .endif
