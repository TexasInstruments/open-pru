;************************************************************************************
;**+------------------------------------------------------------------------------+**
;**|                              ******                                          |**
;**|                              ******     o                                    |**
;**|                              *******__////__****                             |**
;**|                              ***** /_ //___/ ***                             |**
;**|                           ********* ////__ ******                            |**
;**|                             *******(_____/ ******                            |**
;**|                                 **********                                   |**
;**|                                   ******                                     |**
;**|                                      ***                                     |**
;**|                                                                              |**
;**|            Copyright (c) 2025 Texas Instruments Incorporated                 |**
;**|                           ALL RIGHTS RESERVED                                |**
;**|                                                                              |**
;**|    Permission is hereby granted to licensees of Texas Instruments            |**
;**|    Incorporated (TI) products to use this computer program for the sole      |**
;**|    purpose of implementing a licensee product based on TI products.          |**
;**|    No other rights to reproduce, use, or disseminate this computer           |**
;**|    program, whether in part or in whole, are granted.                        |**
;**|                                                                              |**
;**|    TI makes no representation or warranties with respect to the              |**
;**|    performance of this computer program, and specifically disclaims          |**
;**|    any responsibility for any damages, special or consequential,             |**
;**|    connected with the use of this program.                                   |**
;**|                                                                              |**
;**+------------------------------------------------------------------------------+**
;************************************************************************************
; file:     icss_core_clock_macros.inc
;
; brief:    Configuration constants and macros for the project "Dual-core Logic Scope"
;

DCLS_CORE       .set 0     ; Core instance number of the Dual-core Project(PRU0 : 0 or RTU0 : 1)
; Number of continuous samples (in units of 100). Eg: SMP_SIZE 5 => 5*100 samples
; max for MSRAM(2MB) : 20971
DCLS_SMP_MEM_GLB    .set 0x70000000 ; global address of mem that stores sampled data
DCLS_SMP_CNT        .set 20900
DCLS_CH_BYTE        .set 0      ; Options : 0,1,2 eg: Byte 0 => GPI 0-7
DCLS_TRIG_CH        .set 7      ; Low to High trigger channel (0-7)

; samples are divided among two cores. Core 1 handles one set extra for odd numbers
DCLS_SMP_CNT_C1     .set DCLS_SMP_CNT/2
DCLS_SMP_CNT_C0     .set (DCLS_SMP_CNT - DCLS_SMP_CNT_C1)
DCLS_SMP_WT_TIME    .set (100-(26)-5)   ; configured according to write delay(26 here)
MEM_OFFSET_CORE0    .set 0
MEM_OFFSET_CORE1    .set 100       ; (second starts from second cycle of 100 samples)

;************************************************************************************
;
;  Macro: m_sample_channel_byte
;
;  Collects 100 samples of the Byte(8 ips) that corresponds to the IO PIN
;
;   PEAK cycles:
;       100 cycles
;
;   Pseudo code:
;
;       *R1.b0 = (R31 >> 8*(DCLS_CH_BYTE))
;        R1.b0++
;************************************************************************************
m_sample_channel_byte   .macro
    .if (DCLS_CH_BYTE == 2)
    mvib *r1.b0++, r31.b2 ; sampling from an input pin in range 16-20
    .elseif (DCLS_CH_BYTE == 1)
    mvib *r1.b0++, r31.b1 ; sampling from an input pin in range 8-15
    .else
    mvib *r1.b0++, r31.b0 ; sampling from an input pin in range 0-7
    .endif
    .endm

;************************************************************************************
;
;  Macro: nopx
;
;  precisely wait for (P1 > 1) cycles
;
;   PEAK cycles:
;       100 cycles
;   Parameters :
;       P1 : Number of cycles to wait (>1)
;   Pseudo code:
;        LOOP:
;           if (P1-- >1) GOTO LOOP;
;************************************************************************************
nopx    .macro P1
    loop endloopx?, P1 - 1
    NOP
endloopx?:
    .endm