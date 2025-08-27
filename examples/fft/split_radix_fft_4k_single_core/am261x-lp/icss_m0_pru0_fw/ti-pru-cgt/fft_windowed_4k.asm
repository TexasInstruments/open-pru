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
; File: fft_windowed_4k.asm
;
; Description:
;       File that implements 4096 point FFT after/
;       applying a window function in the beginning
;
;*****************************************************************

    .sect    ".text:fft_windowed_4k"
    .clink
    .global   ||FN_FFT_4k_WINDOWED||
    .global   ||FN_LENGTH_2_BUTTERFLY||
    .global   ||FN_WINDOW_PLUS_BOR_4k||

; File includes
	.include  "pru_io\firmware\common\icss_xfer_defines.inc"
    .include  "fft_macros.inc"

;register assignments
    .asg r0    , x_base_addr
    .asg r1.w0 , N
    .asg r1.w2 , N2
    .asg r2    , CC1
    .asg r3    , SS1
    .asg r4    , CC3
    .asg r5    , SS3
    ; following registers are used temporarily for 32 bit loading of twiddle factors
    .asg r6    , CC1_next
    .asg r7    , SS1_next
    .asg r8    , CC3_next
    .asg r9    , SS3_next
    .asg r6    , x_i1
    .asg r7    , x_i2
    .asg r8    , x_i3
    .asg r9    , x_i4
    .asg r10   , temp_1
    .asg r11   , temp_2
    .asg r12.w0 , N4
    .asg r12.w2 , N8
    .asg r13.w0 , index_i
    .asg r14.w0 , I1
    .asg r14.w2 , I2
    .asg r15.w0 , I3
    .asg r15.w2 , I4
    .asg r16.w0 , index_j
    .asg r16.w2 , bsram_fdb_fetch_cycle
    .asg r17.w0 , I5
    .asg r17.w2 , I6
    .asg r18.w0 , I7
    .asg r18.w2 , I8
    .asg r19    , x_i5
    .asg r20    , x_i6
    .asg r21    , x_i7
    .asg r22    , ID
    .asg r23    , temp_3
    .asg r24    , temp_4
    .asg r25.w0 , tw_fac_idx
    .asg r28    , mul_opr_reg_1
    .asg r29    , mul_opr_reg_2
    .asg r26    , mul_prd_reg_low
    .asg r27    , mul_prd_reg_upp
    ; following registers are reused when multiplication is no longer required
    .asg r26    , temp_5
    .asg r27    , temp_6

.asmfunc
||FN_FFT_4k_WINDOWED||:
    ;*******************************************
    ;   Function : FN_FFT_4K_WINDOWED
    ;
    ;   Description:
    ;       Performs 4096 point Split-radix DIT-FFT/
    ;       with windowing in the beginning
    ;
    ;*******************************************
    xout PRU_SPAD_B2_XID, &r3.w2, 2      ; save return PC to SPAD (unused SPAD 12 used)
    ;*********************** Load Window Coefficients *******************************
    ;*    - window coefficients loaded from specified memory using xfver2vbus       *
    ;*    - the transfer of first set of coefficients to FIFO buffer is initiated   *
    ;*    - FIFO is drained to clear the values from previous transfer              *
    ;*    - there is a wait involved for loading the first set of coefficients      *
    ;*    - xin to register fetches the window coefficients from FIFO in 1 cycle    *
    ;*    - next set of window coefficients are loaded to FIFO in background        *
    ;*                    as the window coefficients are applied by the algorithm   *
    ;********************************************************************************
    ;configuration of window coefficients for xfer2vbus
    ldi r18, 0x5                         ; Config: auto read mode on, read size-32 bit
    ldi32 r19, WC_RAM_ADDR_BASE          ; start address of window coefficients
    xout XFR2VBUSP_RD0_XID, &r18, 8           ; set configuration and address
    xin XFR2VBUSP_RD0_XID, &r2, 32            ; drain fifo to clear already loaded values
WAIT_DATA_READY1:                        ; wait till data is ready for the first set/
                                         ;      of window coefficients
    xin     XFR2VBUSP_RD0_XID, &r18, 1
    qbne    WAIT_DATA_READY1, r18.b0, 5

    jal r3.w2, FN_WINDOW_PLUS_BOR_4k     ; apply window function and bit-order reversal

    jal r3.w2, FN_LENGTH_2_BUTTERFLY     ; calculate the l-2 butterflies

    ;******** l-shaped butterflies *************
    ldi N, FFT_SIZE
    ldi N2, (2*4)
    ldi32 x_base_addr, X_BASE_ADDRESS   ; set base address of data samples
    .if (FFT_DEVICE == AM243)
    ; configure fdb bs_ram to auto-increment and load base address as 0x00
    ldi  r10, (1<<15)
    xout  FDB_DATA_XID, &r10, 2
    .else
    zero &tw_fac_idx, 2
    .endif
    ;** [ note :twiddle factors are only loaded from bs-ram on alternate loops] **
    ldi bsram_fdb_fetch_cycle, 0        ; set fetch cycle for twiddle factors to 0

;********************For Loop K*******************************************
    loop END_FOR_LOOP_K, (M-1)          ; for(K=2;K<=M;K++)
    lsl N2, N2, 1                       ; N2  = N2 * 2
    lsr N4, N2, 2                       ; N4  = N2/4
    lsr N8, N2, 3                       ; N8  = N2/8
    ;E     = (float) 6.2831853071719586f/N2
    ;[note:] E need not be calculated for LUT based implementation
    ldi index_i, 0                      ; IS    = 0
    lsl ID, N2, 1                       ; ID    = N2 * 2
;********************Do-While Loop***********************************
DO_WHILE_LOOP_1:
;********************For Loop I***************************
FOR_LOOP_I_1:
    qbge END_FOR_LOOP_I_1, N, index_i     ; loop till I>=N
    add I1, index_i, 4                    ; I1 = I + 1
    add I2, I1, N4                        ; I2 = I1 + N4
    add I3, I2, N4                        ; I3 = I2 + N4
    add I4, I3, N4                        ; I4 = I3 + N4
    ;load X[I1], X[I3], X[I4]
    lbbo &x_i1, x_base_addr, I1, 4
    lbbo &x_i3, x_base_addr, I3, 4
    lbbo &x_i4, x_base_addr, I4, 4
    add temp_1, x_i4, x_i3               ; T1 = X[I4] + X[I3];
    sub x_i4, x_i4, x_i3                 ; X[I4] = X[I4] - X[I3]
    sub x_i3, x_i1, temp_1               ; X[I3] = X[I1] - T1
    add x_i1, x_i1, temp_1               ; X[I1] = X[I1] + T1
    ;store X[I1], X[I3], X[I4]
    sbbo &x_i1, x_base_addr, I1, 4
    sbbo &x_i3, x_base_addr, I3, 4
    sbbo &x_i4, x_base_addr, I4, 4
    qbeq END_IF_N4, N4, 4                ; execute only if(N4!=1)
    add I1, I1, N8                       ; I1 += N8
    add I2, I2, N8                       ; I2 += N8
    add I3, I3, N8                       ; I3 += N8
    add I4, I4, N8                       ; I4 += N8
    ;load new X[I1], X[I2], X[I3], X[I4]
    lbbo &x_i1, x_base_addr, I1, 4
    lbbo &x_i2, x_base_addr, I2, 4
    lbbo &x_i3, x_base_addr, I3, 4
    lbbo &x_i4, x_base_addr, I4, 4
    ;********************************************************************************
    ;* T1 = (X[I3] + X[I4])*.7071067811865475244f                                   *
    ;* [note:] signed multiplication is necessary as operand_1 might be negative    *
    ;* after multiplication, product in Q24 format in upper product register.       *
    ;* [note:] might have to implement rounding if accuracy is low                  *
    ;********************************************************************************
    ldi32 mul_opr_reg_2, ONE_OVER_ROOT_TWO_Q32              ; load multiplicant
    add mul_opr_reg_1, x_i3, x_i4                           ; X[I3] + X[I4]
    nop                                                     ; wait 1 cycle
    xin MAC_XID, &mul_prd_reg_low, 8                        ; get unsigned product.
    qbbc    SIGNCHECK_COMPLETE_1, mul_opr_reg_1, 31         ; sign check for x value
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2 ; correction to get signed product
SIGNCHECK_COMPLETE_1:
    ;Perform operations that use T1 later directly here and store them temporarily/
    ;     (since we cannot overwrite X[3] and X[4] yet)
    sub temp_1, x_i2, mul_prd_reg_upp                       ; X[I4] = X[I2] - T1;
    rsb temp_2, x_i2, 0                                     ; -(X[I2])
    sub temp_2, temp_2, mul_prd_reg_upp                     ; X[I3] = -(X[I2]) - T1
    ;********************************************************************************
    ;* T2 = (X[I3] - X[I4])*.7071067811865475244f                                   *
    ;* [note:] signed multiplication is necessary as operand_1 might be negative    *
    ;* after multiplication, product in Q24 format in upper product register.       *
    ;* [note:] might have to implement rounding if accuracy is low                  *
    ;********************************************************************************
    sub mul_opr_reg_1, x_i3, x_i4     ; do subtraction and move to register for multiplication
    nop                                                     ; wait 1 cycle
    xin MAC_XID , &mul_prd_reg_low, 8                        ; get unsigned product.
    qbbc    SIGNCHECK_COMPLETE_2, mul_opr_reg_1, 31         ; sign check for x value
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2 ; correction to get signed product
SIGNCHECK_COMPLETE_2:
    sub x_i2, x_i1, mul_prd_reg_upp                         ; X[I2] = X[I1] - T2;;
    add x_i1, x_i1, mul_prd_reg_upp                         ; X[I1] = X[I1] + T2;
    ;store X[I1], X[I2], X[I3], X[I4]
    sbbo &x_i1, x_base_addr, I1, 4
    sbbo &x_i2, x_base_addr, I2, 4
    sbbo &temp_2, x_base_addr, I3, 4                        ; temp2 holds X[I3] now
    sbbo &temp_1, x_base_addr, I4, 4                        ; temp1 holds X[I4] now
END_IF_N4:
    add index_i, index_i, ID
    jmp FOR_LOOP_I_1
END_FOR_LOOP_I_1:
;********************For Loop I ends**********************
    lsl index_i, ID, 1                                      ; 2 * ID
    sub index_i, index_i, N2                                ; index_i = IS = 2 * ID - N2
    lsl ID, ID, 2                                           ; ID = 4 * ID
    qblt DO_WHILE_LOOP_1, N, index_i                        ; repeat while(IS<N)
;********************Do-While Loop ends*******************************
    ldi index_j, 2*4                                        ; J = 2
;********************For Loop J**********************
FOR_LOOP_J:
    ;***************** LUT twiddle factor fetch *****************************************
    ;*     - a new set of cos(A), sin(A), cos(3A), sin(3A) required in each fetch cycle *
    ;*     - two sets of these values are stored in one page of fdb-bsram               *
    ;*     - since address auto-increment(page-wise) happens on every read,             *
    ;*                  second set is saved in scratchpad for next fetch                *
    ;*                    cycle and read from bsram happens only every alternate cycle  *
    ;*    - after fetch, CC1, SS1, CC3 and SS3 has cos(A), sin(A), cos(3A) and sin(3A)  *                                                                            *
    ;************************************************************************************
    qbgt END_FOR_LOOP_J, N8, index_j                    ; loop till J>N8
    qbbs SKIP_TWIDDLE_FETCH, bsram_fdb_fetch_cycle, 0   ; load new values in even fetch cycles
    .if (FFT_DEVICE == AM261)
    lbco &CC1, C24, tw_fac_idx, 32                      ; fetch two sets of cos(A), sin(A),
                                                        ;   cos(3A), sin(3A) from LUT
    add tw_fac_idx, tw_fac_idx, 32                      ; increment index for loading next set
    .else
    xin FDB_DATA_XID, &CC1, 32                          ; fetch two sets of cos(A), sin(A),
                                                        ;   cos(3A), sin(3A) from LUT
    add tw_fac_idx, tw_fac_idx, 32                      ; (!) TEST
    .endif

    xout PRU_SPAD_B0_XID, &CC1_next, 16       ; save second set of values for the next cycle
    jmp POST_TWIDDLE_FETCH
SKIP_TWIDDLE_FETCH:
    xin PRU_SPAD_B0_XID, &CC1_next, 16        ; get saved values from previous cycle
    xin BSWAP_4_8_XID, &CC1, 16               ; byte-swap using broadside 4_8 function
POST_TWIDDLE_FETCH:
    add bsram_fdb_fetch_cycle, bsram_fdb_fetch_cycle, 1 ; increment fetch cycle

    ldi index_i, 0                                      ; IS = 0
    lsl ID, N2, 1                                       ; ID = N2 * 2
********************Do-While Loop***********************************
DO_WHILE_LOOP_2:
********************For Loop I***************************
FOR_LOOP_I_2:
    qbge END_FOR_LOOP_I_2, N, index_i                   ; loop till I>=N
    add I1, index_i, index_j                            ; I1 = I + J
    add I2, I1, N4                                      ; I2 = I1 + N4
    add I3, I2, N4                                      ; I3 = I2 + N4
    add I4, I3, N4                                      ; I4 = I3 + N4
    add I5, index_i, N4                                 ; I5 = I + N4 - J + 2
    sub I5, I5, index_j
    add I5, I5, 8                                       ; +2 (*4)
    add I6, I5, N4                                      ; I6 = I5 + N4
    add I7, I6, N4                                      ; I7 = I6 + N4
    add I8, I7, N4                                      ; I8 = I7 + N4
    ;********************************************
    ;***** T1 = X[I3] * CC1 + X[I7] * SS1 *******
    ;***** T2 = X[I7] * CC1 - X[I3] * SS1 *******
    ;********************************************
    ;*  [Note:]CC1 and SS1 are always unsigned. *
    ;*  Therefore sign consideration and        *
    ;*  corresponding correction only performed *
    ;*  for X values                            *
    ;********************************************
    lbbo &mul_opr_reg_1, x_base_addr, I3, 4                   ; load X[I3] to multiplicant register
    mov mul_opr_reg_2, CC1                                    ; move CC1(Q32) for multiplication
    nop                                                       ; wait 1 cycle
    xin MAC_XID , &mul_prd_reg_low, 8                         ; get product [note:]no rounding/scaling here
                                                              ;  as CC1 can be stored as Q32. unsigned product
                                                              ;  available in Q24 format in upper mul reg
    qbbc    SIGNCHECK_COMPLETE_XI3_CC1, mul_opr_reg_1, 31     ; sign check for x value
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2   ; correction to get signed product
SIGNCHECK_COMPLETE_XI3_CC1:
    mov temp_1, mul_prd_reg_upp                               ; (X[I3] * CC1) in temp1
    mov mul_opr_reg_2, SS1                                    ; move SS1(Q32) for multiplication
    nop                                                       ; wait 1 cycle
    xin MAC_XID , &mul_prd_reg_low, 8                         ; get unsigned product
    qbbc    SIGNCHECK_COMPLETE_XI3_SS1, mul_opr_reg_1, 31     ; sign check for x value
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2   ; correction to get signed product
SIGNCHECK_COMPLETE_XI3_SS1:
    mov temp_2, mul_prd_reg_upp                               ; (X[I3] * SS1) in temp 2
    lbbo &mul_opr_reg_1, x_base_addr, I7, 4                   ; load X[I7] to multiplicant register
    mov mul_opr_reg_2, CC1                                    ; move CC1(Q32) for multiplication
    nop                                                       ; wait 1 cycle
    xin MAC_XID , &mul_prd_reg_low, 8                         ; get unsigned product
    qbbc    SIGNCHECK_COMPLETE_XI7_CC1, mul_opr_reg_1, 31     ; sign check for x value
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2   ; correction to get signed product
SIGNCHECK_COMPLETE_XI7_CC1:
    sub temp_2, mul_prd_reg_upp, temp_2                       ; temp_2 --> (X[I7] * CC1 - X[I3] * SS1)
    mov mul_opr_reg_2, SS1                                    ; move SS1(Q32) for multiplication
    nop                                                       ; wait 1 cycle
    xin MAC_XID , &mul_prd_reg_low, 8                         ; get unsigned product
    qbbc    SIGNCHECK_COMPLETE_XI7_SS1, mul_opr_reg_1, 31     ; sign check for x value
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2   ; correction to get signed product
SIGNCHECK_COMPLETE_XI7_SS1:
    add temp_1, temp_1, mul_prd_reg_upp                       ; temp_1 --> (X[I3] * CC1 + X[I7] * SS1)
    ;********************************************
    ;***** T3 = X[I4] * CC3 + X[I8] * SS3 *******
    ;***** T4 = X[I8] * CC3 - X[I4] * SS3 *******
    ;********************************************
    ;* [Note:]SS3 is always unsigned. CC3 can   *
    ;* be signed. Sign consideration and        *
    ;* corresponding correction performed for   *
    ;* both values in CC3 multiplication and    *
    ;* only for X values for SS3 multiplication *
    ;********************************************
    lbbo &mul_opr_reg_1, x_base_addr, I4, 4                   ; load X[I4] to multiplicant register
    mov mul_opr_reg_2, CC3                                    ; move CC3(Q24) for multiplication
                                                              ;[note:]CC3 signed so cannot use Q32
    nop                                                       ; wait 1 cycle
    xin MAC_XID , &mul_prd_reg_low, 8                         ; get unsigned product
    qbbc    SIGNCHECK_COMPLETE_XI4_CC3_1, mul_opr_reg_1, 31   ; sign check for x value
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2   ; correction to get signed product
SIGNCHECK_COMPLETE_XI4_CC3_1:
    qbbc    SIGNCHECK_COMPLETE_XI4_CC3_2, mul_opr_reg_2, 31   ; sign check for CC3
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_1   ; correction to get signed product
SIGNCHECK_COMPLETE_XI4_CC3_2:
    lsl temp_3, mul_prd_reg_upp, 8                            ; get b[2:0] of upper produt
    add temp_3, temp_3, mul_prd_reg_low.b3                    ; get b3 of lower product to save/
                                                              ;     (X[I4] * CC3) in Q24 format in temp_3
    mov mul_opr_reg_2, SS3                                    ; move SS3(Q32) for multiplication
    nop                                                       ; wait 1 cycle
    xin MAC_XID , &mul_prd_reg_low, 8                         ; get unsigned product
    qbbc    SIGNCHECK_COMPLETE_XI4_SS3, mul_opr_reg_1, 31     ; sign check for x value
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2   ; correction to get signed product
SIGNCHECK_COMPLETE_XI4_SS3:
    mov temp_4, mul_prd_reg_upp                               ; (X[I4] * SS3) in temp_4
    lbbo &mul_opr_reg_1, x_base_addr, I8, 4                   ; load X[I8] to multiplicant register
    mov mul_opr_reg_2, CC3                                    ; move CC3(Q24) for multiplication
    nop                                                       ; wait 1 cycle
    xin MAC_XID , &mul_prd_reg_low, 8                          ; get unsigned product
    qbbc    SIGNCHECK_COMPLETE_XI8_CC3_1, mul_opr_reg_1, 31   ; sign check for x value
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2   ; correction to get signed product
SIGNCHECK_COMPLETE_XI8_CC3_1:
    qbbc    SIGNCHECK_COMPLETE_XI8_CC3_2, mul_opr_reg_2, 31   ; sign check for x value
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_1   ; correction to get signed product
SIGNCHECK_COMPLETE_XI8_CC3_2:
    lsl mul_opr_reg_2, mul_prd_reg_upp, 8                     ; get b[2:0] of upper produt
    add mul_opr_reg_2, mul_opr_reg_2, mul_prd_reg_low.b3      ; get b3 of lower product to save
                                                              ;     (X[I8] * CC3) in Q24 format
                                                              ; [note:]rounding might needed if accuracy is low
    sub temp_4, mul_opr_reg_2, temp_4                         ; temp_4 --> (X[I8] * CC3 - X[I4] * SS3)
    mov mul_opr_reg_2, SS3                                    ; move SS3(Q32) for multiplication
    nop                                                       ; wait 1 cycle
    xin MAC_XID , &mul_prd_reg_low, 8                         ; get unsigned product
    qbbc    SIGNCHECK_COMPLETE_XI8_SS3, mul_opr_reg_1, 31     ; sign check for x value
    sub     mul_prd_reg_upp, mul_prd_reg_upp, mul_opr_reg_2   ; correction to get signed product
SIGNCHECK_COMPLETE_XI8_SS3:
    add temp_3, temp_3, mul_prd_reg_upp           ; temp_3 --> (X[I4] * CC3 + X[I8] * SS3)
    add temp_5, temp_1, temp_3                    ; T5 = T1 + T3
    add temp_6, temp_2, temp_4                    ; T6 = T2 + T4
    sub temp_3, temp_1, temp_3                    ; T3 = T1 - T3
    sub temp_4, temp_2, temp_4                    ; T4 = T2 - T4

    lbbo &x_i6, x_base_addr, I6, 4                ; load X[I6]
    add temp_2, x_i6, temp_6                      ; T2 = X[I6] + T6
    sub x_i3, temp_6, x_i6                        ; X[I3] = T6 - X[I6]
    sbbo &x_i3, x_base_addr, I3, 4                ; store X[I3]
    sbbo &temp_2, x_base_addr, I8, 4           	  ; (X[I8] = T2) and store X[I8]
    lbbo &x_i2, x_base_addr, I2, 4                ; load X[I2]
    sub temp_2, x_i2, temp_3                      ; T2    = X[I2] - T3
    rsb x_i2, x_i2, 0                             ; (-X[I2])
    sub x_i7, x_i2, temp_3                        ; X[I7] = -X[I2] - T3
    sbbo &x_i7, x_base_addr, I7, 4                ; store X[I7]
    sbbo &temp_2, x_base_addr, I4, 4              ; (X[I4] = T2) and store X[I4]

    lbbo &x_i1, x_base_addr, I1, 4                ; load X[I1]
    add temp_1, x_i1, temp_5                      ; T1    = X[I1] + T5
    sub x_i6, x_i1, temp_5                        ; X[I6] = X[I1] - T5
    sbbo &x_i6, x_base_addr, I6, 4                ; store X[I6]
    sbbo &temp_1, x_base_addr, I1, 4              ; (X[I1] = T1) and store X[I1]
    lbbo &x_i5, x_base_addr, I5, 4                ; load X[I5]
    add temp_1, x_i5, temp_4                      ; T1    = X[I5] + T4
    sub x_i5, x_i5, temp_4                        ; X[I5] = X[I5] - T4
    sbbo &x_i5, x_base_addr, I5, 4                ; store X[I5]
    sbbo &temp_1, x_base_addr, I2, 4              ; (X[I2] = T1) and store X[I2]
    add index_i, index_i, ID					  ; increment ID  
    jmp FOR_LOOP_I_2
END_FOR_LOOP_I_2:
******************** For Loop I ends **********************
    ;IS = 2 * ID - N2
    lsl index_i, ID, 1                            ; 2 * ID
    sub index_i, index_i, N2                      ; index_i = IS = 2 * ID - N2
    lsl ID, ID, 2                                 ; ID = 4 * ID
    qblt DO_WHILE_LOOP_2, N, index_i              ; repeat while(IS<N)
******************** Do-While Loop ends *******************************
    add index_j, index_j, 4                       ; j++
    jmp FOR_LOOP_J
END_FOR_LOOP_J:
********************For Loop J ends*****************
END_FOR_LOOP_K:
********************For Loop K*******************************************
    xin PRU_SPAD_B2_XID, &r3.w2, 2                ; restore return PC from SPAD
    jmp        r3.w2                              ; return from function
.endasmfunc

;*****************************************************************************
;*                                     END
;*****************************************************************************
