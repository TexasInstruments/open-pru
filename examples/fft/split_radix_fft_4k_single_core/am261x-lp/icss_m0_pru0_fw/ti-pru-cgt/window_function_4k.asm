; Copyright (C) 2024-2025 Texas Instruments Incorporated - http://www.ti.com/
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

******************************************************************
; File: window_function_4k.asm
;
; Description:
;       File containing window function for 4096 point FFT
;
;*****************************************************************
    .sect    ".text:window_function_4k"
    .clink
    .global  ||FN_WINDOW_PLUS_BOR_4k||
; File includes
    .include  "pru_io\firmware\common\icss_xfer_defines.inc"
    .include "fft_macros.inc"

;register assignments
    .asg r1 , bor_lut_addr_base
    ;window coefficient registers
    .asg r2    , w_coef_reg_base
    .asg r3    , w_coef_reg_1
    .asg r4    , w_coef_reg_2
    .asg r5    , w_coef_reg_3
    .asg r6    , w_coef_reg_4
    .asg r7    , w_coef_reg_5
    .asg r8    , w_coef_reg_6
    .asg r9    , w_coef_reg_7
    ;inpult value registers
    .asg r10 , bram_config_reg
    .asg r11 , ip_reg_base
    .asg r12 , ip_reg_val_1
    .asg r13 , ip_reg_val_2
    .asg r14 , ip_reg_val_3
    .asg r15 , ip_reg_val_4
    .asg r16 , ip_reg_val_5
    .asg r17 , ip_reg_val_6
    .asg r18 , ip_reg_val_7
    .asg r19.w0 , ip_ram_idx_1            ; input memory index 1
    .asg r19.w2 , ip_ram_idx_2            ; input memory index 2
    .asg r20    , op_dram_off             ; register to store calculated output memory offset
    .asg r21.w0 , ip_brv_idx_1            ; bit reversed index for input set 1
    .asg r21.b1 , ip_brv_idx_1_b1         ; b1 of bit reversed index for input set 1
    .asg r21.w2 , ip_brv_idx_2            ; bit reversed index for input set 2
    .asg r21.b3 , ip_brv_idx_2_b1         ; b1 of bit reversed index for input set 2
    .asg r22.w0 , ip_dram_brv_lut_offset  ; offset for bit reversed index LUT
    .asg r23.w0 , complement_mask         ; offset for bit reversed index LUT
    .asg r23.w2 , loop_count              ; outer loop index
    .asg r24 , op_mem_addr_base           ; base address for output memory
    .asg r25.b0 , mul_config_reg_b0
    .asg r28 , mul_opr_reg_1
    .asg r29 , mul_opr_reg_2
    .asg r26 , mul_prd_reg_low
    .asg r27 , mul_prd_reg_upp

.asmfunc
||FN_WINDOW_PLUS_BOR_4k||:
    ;*******************************************
    ;   Function : FN_WINDOW_4k
    ;
    ;   Description:
    ;       Applies window function and performs/
    ;       bit-order reversal on inputs.
    ;
    ;*******************************************
    xout PRU_SPAD_B1_XID, &r3.w2, 2              ; save return PC to SPAD (unused SPAD 11 used)
    ;set ip value indexs(offsets?)
    ldi ip_ram_idx_1, IP_RAM_ADDR_1
    ldi ip_ram_idx_2, IP_RAM_ADDR_2
    ldi32 bor_lut_addr_base, BRV_RAM_ADDR_BASE   ; initialize base address of bit order reveresed index LUT
    ldi ip_dram_brv_lut_offset, BRV_LUT_OFFSET   ; initialize base offet of bit-reversed index LUT
    ldi32 op_mem_addr_base, OP_RAM_ADDR_BASE     ; initialize base address of output memory
    ldi loop_count, WF_LOOP_COUNT  ; load loop count for interation over corresponding input values for each core
    ldi complement_mask, 0x07FC ; initialize value for complementing the index (for accessing 4096 values)

    ;**********************Outer Loop***************************
    ;*            Operates on inputs in a loop, 16 at a time   *
    ;***********************************************************
    loop POST_MUL, loop_count

    lbco &w_coef_reg_base, c28, ip_ram_idx_1, 32  ; load first set of 8 input values
    xout PRU_SPAD_B0_XID, &w_coef_reg_base, 32         ; save to scratchpad
    xin XFR2VBUSP_RD0_XID, &w_coef_reg_base, 32      ; load 8 window coefficients (RD_AUTO enabled)
    lbco &ip_reg_base, c28, ip_ram_idx_2, 32    ; load second set of 8 input values

    ;*******Bit order reverse Index Storage Procedure - 4k********************************
    ;*                                                                                   *
    ;* original 12 bit index --> (abcdefghijkl)                                          *
    ;* step 1 - get (ihgfedcba) from LUT for each set of (abcdefghi000 to abcdefghi111)  *
    ;* step 2 - get real address (output memory address base for each loop)by adding     *
    ;*                                                 base address value to LUT value   *
    ;* step 3 - add constant (lkj<<9) for every input 000 to 111 individually            *
    ;*                                                   and save output directly        *
    ;* step 4 - bit reverse index for a complementary input is                           *
    ;*                obtained by complementing the calculated bit reversed index itself *
    ;*************************************************************************************
    lbbo &ip_brv_idx_1, bor_lut_addr_base, ip_dram_brv_lut_offset, 2 ; load 9-bit reversed index LUT value
    lsl ip_brv_idx_1, ip_brv_idx_1, 2                 ; multiply by 2 to get address base for 4 Byte values
    xor ip_brv_idx_2, ip_brv_idx_1, complement_mask   ; calculate the 9 bit index for saving second set

    ;*********************Inner Loop *****************************************
    ;*    UNROLLED(x8) : Multiplies 8 window coefficients with 2 inputs each *
    ;*        [NB: Inner loop is not a loop now, after its fully unrolled]   *
    ;*************************************************************************
    ;**************************
    ;*  Window Coefficient 0  *
    ;**************************
    mov mul_opr_reg_1, w_coef_reg_base           ; load window coefficient
    mov mul_opr_reg_2, ip_reg_val_7              ; move input value from second input set
    xin PRU_SPAD_B0_XID, &w_coef_reg_base, 4            ; Load first input during 1 cycle MUL delay
    xin MAC_XID, &mul_prd_reg_low, 8             ; get product in lower and upper registers
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low   ; add msb from previous step
    ; set bit reversed prefix to calculate store address
    ;     prefix with 0b111 : add (0b111<<3 = 0b111000 = 56) to most significant Byte
    add ip_brv_idx_2_b1, ip_brv_idx_2_b1, (56)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_2, 4 ; store value using calculated offset

    mov mul_opr_reg_2, w_coef_reg_base  ; load input value from first input set
    nop
    xin MAC_XID, &mul_prd_reg_low, 8    ; get product lower and upper 32 bits after 1 cycle delay
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                 ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low    ; add msb from previous step
    ; set bit reversed prefix to calculate store address
    ; prefix with 0x00 - Instruction unnecessary
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_1, 4 ; store values using calculated index

    ;**************************
    ;*  Window Coefficient 1  *
    ;**************************
    mov mul_opr_reg_1, w_coef_reg_1              ; load window coefficient
    mov mul_opr_reg_2, ip_reg_val_6              ; move input value from second input set
    xin PRU_SPAD_B0_XID, &w_coef_reg_1, 4               ; Load first input during 1 cycle MUL delay
    xin MAC_XID, &mul_prd_reg_low, 8             ; get product in lower and upper registers
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                   ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low      ; add msb from previous step
    ; set bit reversed prefix to calculate store address
        ; prefix with 0b011 : add (0b011<<3 = 0b011000 = 24) to most significant Byte
        ; current value of most significant Byte --> (msB + 56)
        ; (msB + 24) = (msB + 56) - (32) ;calculation to avoid additional step
    sub ip_brv_idx_2_b1, ip_brv_idx_2_b1, (32)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_2, 4 ; store value using calculated offset

    mov mul_opr_reg_2, w_coef_reg_1                    ; load input value from first input set
    nop
    xin MAC_XID, &mul_prd_reg_low, 8                   ; get product lower and upper 32 bits
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                 ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low    ; add msb from previous step
    ;set bit reversed prefix to calculate store address
        ; prefix with 0b100
        ; add (0b100<<3 = 0b100000 = 32) to most significant Byte
    add ip_brv_idx_1_b1, ip_brv_idx_1_b1, (32)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_1, 4 ; store value using calculated offset

    ;**************************
    ;*  Window Coefficient 2  *
    ;**************************
    mov mul_opr_reg_1, w_coef_reg_2               ; load window coefficient
    mov mul_opr_reg_2, ip_reg_val_5               ; move input value from second input set
    xin PRU_SPAD_B0_XID, &w_coef_reg_2, 4                ; Load first input during 1 cycle MUL delay
    xin MAC_XID, &mul_prd_reg_low, 8              ; get product in lower and upper registers
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                   ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low      ; add msb from previous step
    ; set bit reversed prefix to calculate store address
    ; prefix with 0b101 : add (0b101<<3 = 0b101000 = 40) to most significant Byte
        ;current value of most significant Byte --> (msB + 24)
        ;(msB + 40) = (msB + 24) + 16 ;calculation to avoid additional step
    add ip_brv_idx_2_b1, ip_brv_idx_2_b1, (16)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_2, 4 ; store value using calculated offset

    mov mul_opr_reg_2, w_coef_reg_2                   ; load input value from first input set
    nop
    xin MAC_XID, &mul_prd_reg_low, 8                  ; get product lower and upper 32 bits
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                   ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low      ; add msb from previous step
    ;set bit reversed prefix to calculate store address
        ; prefix with 0b010 : add (0b010<<3 = 0b010000 = 16) to most significant Byte
        ;current value of most significant Byte --> (msB + 32)
        ;(msB + 16) = (msB + 32) - (16) ;calculation to avoid additional step
    sub ip_brv_idx_1_b1, ip_brv_idx_1_b1, (16)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_1, 4 ; store value using calculated offset
    
    ;**************************
    ;*  Window Coefficient 3  *
    ;**************************
    mov mul_opr_reg_1, w_coef_reg_3                ; load window coefficient
    mov mul_opr_reg_2, ip_reg_val_4                ; move input value from second input set
    xin PRU_SPAD_B0_XID, &w_coef_reg_3, 4                 ; Load first input during 1 cycle MUL delay
    xin MAC_XID, &mul_prd_reg_low, 8               ; get product in lower and upper registers
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                   ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low      ; add msb from previous step
    ; set bit reversed prefix to calculate store address
    ; prefix with 0b001 : add (0b001<<3 = 0b001000 = 8) to most significant Byte
        ;current value of most significant Byte --> (msB + 40)
        ;(msB + 8) = (msB + 40) - 32 ;calculation to avoid additional step
    sub ip_brv_idx_2_b1, ip_brv_idx_2_b1, (32)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_2, 4   ; store value using calculated offset

    mov mul_opr_reg_2, w_coef_reg_3                   ; load input value from first input set
    nop
    xin MAC_XID, &mul_prd_reg_low, 8                  ; get product lower and upper 32 bits
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                   ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low      ; add msb from previous step
    ;set bit reversed prefix to calculate store address
    ; prefix with 0b110 : add (0b110<<3 = 0b110000 = 48) to most significant Byte
        ;current value of most significant Byte --> (msB + 16)
        ;(msB + 48) = (msB + 16) + (32) ;calculation to avoid additional step
    add ip_brv_idx_1_b1, ip_brv_idx_1_b1, (32)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_1, 4 ; store value using calculated offset

    ;**************************
    ;*  Window Coefficient 4  *
    ;**************************
    mov mul_opr_reg_1, w_coef_reg_4                ; load window coefficient
    mov mul_opr_reg_2, ip_reg_val_3                ; move input value from second input set
    xin PRU_SPAD_B0_XID, &w_coef_reg_4, 4                 ; Load first input during 1 cycle MUL delay
    xin MAC_XID, &mul_prd_reg_low, 8               ; get product in lower and upper registers
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                   ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low      ; add msb from previous step
    ;set bit reversed prefix to calculate store address
    ; prefix with 0b110 : add (0b110<<3 = 0b110000 = 48) to most significant Byte
        ;current value of most significant Byte --> (msB + 8)
        ;(msB + 48) = (msB + 8) + (40) ;calculation to avoid additional step
    add ip_brv_idx_2_b1, ip_brv_idx_2_b1, (40)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_2, 4 ; store value using calculated offset

    mov mul_opr_reg_2, w_coef_reg_4                ; load input value from first input set
    nop
    xin MAC_XID, &mul_prd_reg_low, 8               ; get product lower and upper 32 bits
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                 ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low    ; add msb from previous step
    ;set bit reversed prefix to calculate store address
    ; prefix with 0b001 : add (0b001<<3 = 0b001000 = 8) to most significant Byte
        ;current value of most significant Byte --> (msB + 0xC)
        ;(msB + 8) = (msB + 48) - (0xC - 40) ;calculation to avoid additional step
    sub ip_brv_idx_1_b1, ip_brv_idx_1_b1, (40)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_1, 4 ; store value using calculated offset

    ;**************************
    ;*  Window Coefficient 5  *
    ;**************************
    mov mul_opr_reg_1, w_coef_reg_5                ; load window coefficient
    mov mul_opr_reg_2, ip_reg_val_2                ; move input value from second input set
    xin PRU_SPAD_B0_XID, &w_coef_reg_5, 4                 ; Load first input during 1 cycle MUL delay
    xin MAC_XID, &mul_prd_reg_low, 8               ; get product in lower and upper registers
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                   ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low      ; add msb from previous step
    ;set bit reversed prefix to calculate store address
    ; prefix with 0b010 : add (0b010<<3 = 0b010000 = 16) to most significant Byte
        ;current value of most significant Byte --> (msB + 48)
        ;(msB + 16) = (msB + 48) - (32) ;calculation to avoid additional step
    sub ip_brv_idx_2_b1, ip_brv_idx_2_b1, (32)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_2, 4 ; store value using calculated offset

    mov mul_opr_reg_2, w_coef_reg_5                   ; load input value from first input set
    nop
    xin MAC_XID, &mul_prd_reg_low, 8                  ; get product lower and upper 32 bits
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                 ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low    ; add msb from previous step
    ;set bit reversed prefix to calculate store address
    ; prefix with 0b101 : add (0b101<<3 = 0b101000 = 40) to most significant Byte
        ;current value of most significant Byte --> (msB + 8)
        ;(msB + 40) = (msB + 8) + (32) ;calculation to avoid additional step
    add ip_brv_idx_1_b1, ip_brv_idx_1_b1, (32)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_1, 4 ; store value using calculated offset

    ;**************************
    ;*  Window Coefficient 6  *
    ;**************************
    mov mul_opr_reg_1, w_coef_reg_6               ; load window coefficient
    mov mul_opr_reg_2, ip_reg_val_1               ; move input value from second input set
    xin PRU_SPAD_B0_XID, &w_coef_reg_6, 4                ; Load first input during 1 cycle MUL delay
    xin MAC_XID, &mul_prd_reg_low, 8              ; get product in lower and upper registers
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                   ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low      ; add msb from previous step
    ; set bit reversed prefix to calculate store address
    ; prefix with 0b100 : add (0b100<<3 = 0b100000 = 32) to most significant Byte
        ;current value of most significant Byte --> (msB + 16)
        ;(msB + 32) = (msB + 16) + (16) ;calculation to avoid additional step
    add ip_brv_idx_2_b1, ip_brv_idx_2_b1, (16)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_2, 4 ; store value using calculated offset

    mov mul_opr_reg_2, w_coef_reg_6                   ; load input value from first input set
    nop
    xin MAC_XID, &mul_prd_reg_low, 8                  ; get product lower and upper 32 bits
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                   ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low      ; add msb from previous step
    ; set bit reversed prefix to calculate store address
    ; prefix with 0b011 : add (0b011<<3 = 0b011000 = 24) to most significant Byte
        ;current value of most significant Byte --> (msB + 40)
        ;(msB + 24) = (msB + 40) - (16) ;calculation to avoid additional step
    sub ip_brv_idx_1_b1, ip_brv_idx_1_b1, (16)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_1, 4   ; store value using calculated offset

    ;**************************
    ;*  Window Coefficient 7  *
    ;**************************
    mov mul_opr_reg_1, w_coef_reg_7                ; load window coefficient
    mov mul_opr_reg_2, ip_reg_base                 ; move input value from second input set
    xin PRU_SPAD_B0_XID, &w_coef_reg_7, 4                 ; Load first input during 1 cycle MUL delay
    xin MAC_XID, &mul_prd_reg_low, 8               ; get product in lower and upper registers
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                   ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low      ; add msb from previous step
    ;set bit reversed prefix to calculate store address
    ; prefix with 0b000 : add (0b000<<3 = 0b000000 = 0x0) to most significant Byte
        ;current value of most significant Byte --> (msB + 32)
        ;(msB + 0x0) = (msB + 32) - (32) ;calculation to avoid additional step
    sub ip_brv_idx_2_b1, ip_brv_idx_2_b1, (32)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_2, 4 ; store value using calculated offset

    mov mul_opr_reg_2, w_coef_reg_7                      ; load input value from firt input set
    nop
    xin MAC_XID, &mul_prd_reg_low, 8                     ; get product lower and upper 32 bits
    ;round upper product value based on msb of lower product value
    lsr mul_prd_reg_low, mul_prd_reg_low, 31                   ; get msb of lower product value
    add mul_prd_reg_low, mul_prd_reg_upp, mul_prd_reg_low      ; add msb from previous step
    ;set bit reversed prefix to calculate store address
    ; prefix with 0b111 : add (0b111<<3 = 0b111000 = 56) to most significant Byte
        ;current value of most significant Byte --> (msB + 24)
        ;(msB + 56) = (msB + 24) + (32) ;calculation to avoid additional step
    add ip_brv_idx_1_b1, ip_brv_idx_1_b1, (32)
    sbbo &mul_prd_reg_low, op_mem_addr_base, ip_brv_idx_1, 4   ; store value using calculated offset

*********** Inner Loop (Unrolled x8) ends ******************
    add ip_ram_idx_1, ip_ram_idx_1, 32             ; increment offset for first set of inputs
    sub ip_ram_idx_2, ip_ram_idx_2, 32             ; decrement offset for second set of inputs
    add ip_dram_brv_lut_offset, ip_dram_brv_lut_offset, 2 ; increment offset for bit-reversed LUT

;*********** Outer Loop ends **************************************************
POST_MUL:
    xin PRU_SPAD_B1_XID, &r3.w2, 2                ; restore return PC from SPAD
    jmp r3.w2                                     ; exit function
.endasmfunc

;*****************************************************************************
;*                                     END
;*****************************************************************************
