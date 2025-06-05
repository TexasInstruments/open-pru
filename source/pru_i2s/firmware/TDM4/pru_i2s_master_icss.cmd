/****************************************************************************/
/*  ICSSG_PRU.cmd                                                           */
/*  Copyright (c) 2021  Texas Instruments Incorporated                      */
/*                                                                          */
/*    Description: This file is a linker command file that can be used for  */
/*                 linking PRU programs built with the C compiler and       */
/*                 the resulting .out file on an pru device.                */
/****************************************************************************/

/* -cr  */                          /* Link using C conventions */
/* -stack       0x100 */

/* Specify the System Memory Map */
MEMORY
{
      PAGE 0:
    PRU_IMEM            : org = 0x00000000 len = 0x00004000         /* 16kB ICSSG_PRU Instruction RAM */

      PAGE 1:

    /* RAM */

    PRU_DMEM_0_1_LOW    : org = 0x00000000 len = 0x00001000         /* 4kB ICSSG Data RAM 0_1 for PRU*/
    PRU_DMEM_0_1_HIGH   : org = 0x00001000 len = 0x00001000         /* 4kB ICSSG Data RAM 0_1 for RTU*/
    PRU_DMEM_1_0        : org = 0x00002000 len = 0x00002000         /* 8kB ICSSG Data RAM 1_0 */

      PAGE 2:
    PRU_SHAREDMEM       : org = 0x00010000 len = 0x00010000 CREGISTER=28 /* 64kB Shared RAM */

    /* Peripherals */

    PRU_CFG             : org = 0x00026000 len = 0x00000120 CREGISTER=4
    
    RSVD0               : org = 0x00020000 len = 0x00001504 CREGISTER=0
    RSVD1               : org = 0x48040000 len = 0x0000005C CREGISTER=1
    RSVD2               : org = 0x4802A000 len = 0x000000D8 CREGISTER=2
    RSVD3               : org = 0x00030000 len = 0x00000060 CREGISTER=3
    RSVD5               : org = 0x48060000 len = 0x00000300 CREGISTER=5
    RSVD6               : org = 0x48030000 len = 0x000001A4 CREGISTER=6
    RSVD7               : org = 0x00028000 len = 0x00000038 CREGISTER=7
    RSVD8               : org = 0x46000000 len = 0x00000100 CREGISTER=8
    RSVD9               : org = 0x4A100000 len = 0x0000128C CREGISTER=9
    RSVD10              : org = 0x48318000 len = 0x00000100 CREGISTER=10
    RSVD11              : org = 0x48022000 len = 0x00000088 CREGISTER=11
    RSVD12              : org = 0x48024000 len = 0x00000088 CREGISTER=12
    RSVD13              : org = 0x48310000 len = 0x00000100 CREGISTER=13
    RSVD14              : org = 0x481CC000 len = 0x000001E8 CREGISTER=14
    RSVD15              : org = 0x481D0000 len = 0x000001E8 CREGISTER=15
    RSVD16              : org = 0x481A0000 len = 0x000001A4 CREGISTER=16
    RSVD17              : org = 0x4819C000 len = 0x000000D8 CREGISTER=17
    RSVD18              : org = 0x48300000 len = 0x000002C4 CREGISTER=18
    RSVD19              : org = 0x48302000 len = 0x000002C4 CREGISTER=19
    RSVD20              : org = 0x48304000 len = 0x000002C4 CREGISTER=20
    RSVD21              : org = 0x00032400 len = 0x00000100 CREGISTER=21
    RSVD22              : org = 0x480C8000 len = 0x00000140 CREGISTER=22
    RSVD23              : org = 0x480CA000 len = 0x00000880 CREGISTER=23
    RSVD26              : org = 0x0002E000 len = 0x0000031C CREGISTER=26
    RSVD27              : org = 0x00032000 len = 0x00000100 CREGISTER=27
    RSVD29              : org = 0x49000000 len = 0x00001098 CREGISTER=29
}

/* Specify the sections allocation into memory */
SECTIONS {
    /* Forces _c_int00 to the start of PRU IRAM. Not necessary when loading
       an ELF file, but useful when loading a binary */
    /* .text:_c_int00*  >  0x0, PAGE 0 */

    /* .text       >  PRU_IMEM, PAGE 0 */
    /* .stack      >  PRU_DMEM_0_1_LOW, PAGE 1 */
    /* .bss        >  PRU_DMEM_0_1_LOW, PAGE 1 */
    /* .cio        >  PRU_DMEM_0_1_LOW, PAGE 1 */
    /* .data       >  PRU_DMEM_0_1_LOW, PAGE 1 */
    /* .switch     >  PRU_DMEM_0_1_LOW, PAGE 1 */
    /* .sysmem     >  PRU_DMEM_0_1_LOW, PAGE 1 */
    /* .cinit      >  PRU_DMEM_0_1_LOW, PAGE 1 */
    /* .rodata     >  PRU_DMEM_0_1_LOW, PAGE 1 */
    /* .rofardata  >  PRU_DMEM_0_1_LOW, PAGE 1 */
    /* .farbss     >  PRU_DMEM_0_1_LOW, PAGE 1 */
    /* .fardata    >  PRU_DMEM_0_1_LOW, PAGE 1 */
    
    .text           >  PRU_IMEM, PAGE 0
    .data           >  PRU_DMEM_0_1_LOW, PAGE 1    
    .fwRegs         >  0x0, PAGE 1

    .outSamps       >  PRU_DMEM_0_1_LOW, PAGE 1 
    
    .dbgBuf         > PRU_SHAREDMEM, PAGE 2
}
