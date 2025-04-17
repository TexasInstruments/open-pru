;
; Copyright (c) 2025, Texas Instruments Incorporated
; All rights reserved.
;
;  Redistribution and use in source and binary forms, with or without
;  modification, are permitted provided that the following conditions
;  are met:
;
;  *  Redistributions of source code must retain the above copyright
;     notice, this list of conditions and the following disclaimer.
;
;  *  Redistributions in binary form must reproduce the above copyright
;     notice, this list of conditions and the following disclaimer in the
;     documentation and/or other materials provided with the distribution.
;
;  *  Neither the name of Texas Instruments Incorporated nor the names of
;     its contributors may be used to endorse or promote products derived
;     from this software without specific prior written permission.
;
;  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
;  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
;  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
;  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
;  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
;  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
;  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
;  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
;  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
;  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
;  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;

        .cdecls C,NOLIST
%{
        #include "icss_pru_i2s_fw.h"
%}

                                ; PRU I2S Firmware Registers
                                .sect   ".fwRegs"
                                .retain ".fwRegs"
                                .retainrefs ".fwRegs"
                                .space  ICSS_PRUI2S_FW_REG_BASE
                                .if $isdefed("NUMBER_OF_TX_2")
                                .byte   2       ; NUM_TX_I2S
                                .endif
                                .if $isdefed("NUMBER_OF_TX_3")
                                .byte   3       ; NUM_TX_I2S
                                .endif
                                .if	!$isdefed("NUMBER_OF_TX_2")
                                .if	!$isdefed("NUMBER_OF_TX_3")
                                .byte   0       ; NUM_TX_I2S
                                .endif
                                .endif
                                .if $isdefed("I2S_RX")
                                .byte   2       ; NUM_RX_I2S
                                .else
                                .byte   0       ; NUM_RX_I2S
                                .endif
                                .byte   48      ; SAMP_FREQ
                                .if $isdefed("SOC_AM64X")
                                .byte   16      ; BITS_PER_SLOT
                                .else
                                .byte   32      ; BITS_PER_SLOT
                                .endif
                                .if $isdefed("PRU0")
                                .word   0x10000 ; TX_PING_PONG_BUF_ADDR
                                .else
                                .word   0x10200 ; TX_PING_PONG_BUF_ADDR
                                .endif
                                .if $isdefed("NUMBER_OF_TX_3")
                                .short  264     ; PING_PONG_BUF_SZ
                                .else
                                .short  256     ; PING_PONG_BUF_SZ
                                .endif
                                .if $isdefed("PRU0")
                                .byte   18      ; I2S_TX_ICSS_INTC_SYS_EVT (Event 18=16+2)
                                .byte   19      ; I2S_RX_ICSS_INTC_SYS_EVT (Event 19=16+3)
                                .byte   20      ; I2S_ERR_ICSS_INTC_SYS_EVT (Event 20=16+4)
                                .else
                                .byte   21      ; I2S_TX_ICSS_INTC_SYS_EVT (Event 21=16+5)
                                .byte   22      ; I2S_RX_ICSS_INTC_SYS_EVT (Event 22=16+6)
                                .byte   23      ; I2S_ERR_ICSS_INTC_SYS_EVT (Event 23=16+7)
                                .endif

                                .if $isdefed("PRU0")
                                .byte   6       ; PIN_NUM_BCLK
                                .byte   1       ; PIN_NUM_FSYNC
                                .byte   2       ; PIN_NUM_TX0/RESERVED in case of RX Only
                                .byte   3       ; PIN_NUM_TX1/RESERVED in case of RX Only
                                .byte   5       ; PIN_NUM_TX2/RESERVED in case of RX Only/2Tx Only
                                .byte   0       ; PIN_NUM_RX0
                                .byte   4       ; PIN_NUM_RX1
                                .else
                                .byte   0       ; PIN_NUM_BCLK
                                .byte   1       ; PIN_NUM_FSYNC
                                .byte   4       ; PIN_NUM_TX0/RESERVED in case of RX Only
                                .byte   5       ; PIN_NUM_TX1/RESERVED in case of RX Only
                                .byte   6       ; PIN_NUM_TX2/RESERVED in case of RX Only/2Tx Only
                                .byte   2       ; PIN_NUM_RX0
                                .byte   3       ; PIN_NUM_RX1
                                .endif

                                .if $isdefed("PRU0")
                                .word   0x10100 ; RX_PING_PONG_BUF_ADDR
                                .else
                                .word   0x10300 ; RX_PING_PONG_BUF_ADDR
                                .endif
                                .byte   0       ; TX_PING_PONG_SEL
                                .byte   0       ; RX_PING_PONG_SEL
                                .byte   2       ; TX_PING_PONG_STAT
                                .byte   1       ; RX_PING_PONG_STAT
                                .byte   0       ; ERR_STAT

