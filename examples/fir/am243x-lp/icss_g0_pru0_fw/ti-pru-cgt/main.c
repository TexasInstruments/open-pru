/*
 * Copyright (C) 2024-2025 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 *  \file   main.c
 *
 *  \brief  File that calls the FIR filter
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include "am243_am64_pru_pinmux.h"
#include <drivers/pruicss.h>
#include <drivers/hw_include/hw_types.h>
/* ========================================================================== */
/*                             Constant Macros                                */
/* ========================================================================== */
#define FIR_COEF_MEM            0x70000000 //MSRAM
#define FIR_TWO_RAISED_TO_31    2147483648.0

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */
volatile int32_t *fir_coef;    /* variable to access FIR coefficient location in memory */
/* configure filter coefficients here */
static double fir_filter_taps[64] = {
    -0.003743790557955713,
    -0.003058147698064947,
    -0.0030497021701321246,
    -0.001942142907318171,
    0.0002464973410403542,
    0.0030692534302086518,
    0.005702286387239753,
    0.0071604789447699,
    0.00664154615248411,
    0.003880897110238272,
    -0.000623437896701586,
    -0.005623129621924286,
    -0.00940181141429925,
    -0.010312709610212833,
    -0.007409357574092328,
    -0.0009169381995706036,
    0.007565989356619614,
    0.015380872938955504,
    0.019509686396069776,
    0.01757387396832163,
    0.008801107357699772,
    -0.005353988064870881,
    -0.0211958354226486,
    -0.03348283953673643,
    -0.03670883944653822,
    -0.026663899395729943,
    -0.001825420589755506,
    0.03583858823723252,
    0.08094566162214854,
    0.1257145420648719,
    0.1616805613887539,
    0.18170256085498485,
    0.18170256085498485,
    0.1616805613887539,
    0.1257145420648719,
    0.08094566162214854,
    0.03583858823723252,
    -0.001825420589755506,
    -0.026663899395729943,
    -0.03670883944653822,
    -0.03348283953673643,
    -0.0211958354226486,
    -0.005353988064870881,
    0.008801107357699772,
    0.01757387396832163,
    0.019509686396069776,
    0.015380872938955504,
    0.007565989356619614,
    -0.0009169381995706036,
    -0.007409357574092328,
    -0.010312709610212833,
    -0.00940181141429925,
    -0.005623129621924286,
    -0.000623437896701586,
    0.003880897110238272,
    0.00664154615248411,
    0.0071604789447699,
    0.005702286387239753,
    0.0030692534302086518,
    0.0002464973410403542,
    -0.001942142907318171,
    -0.0030497021701321246,
    -0.003058147698064947,
    -0.003743790557955713
};
/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */
void FIR_64();
/*
 * void main(void)
 */
int main(void)
{
volatile uint8_t coef_count = 0; /**/
    /*  ----------------- clock config --------------------------  */
    /*
    ; core clock options:
    ; ICSSG0_CLKMUX_SEL: bit 0 = 1 PLL0, bit 0 = 0 PLL2
    ; MAIN_PLL0_HSDIV9: 2 (333 MHz), 3 (250 MHz)
    ; MAIN_PLL2_HSDIV2: 5 (300 MHz), 7 (225 MHz)
    */
    /* configure ICSS clocks */
    /* unlock CTRLMMR config register partition 2 */
    HW_WR_REG32(0x43009008, 0x68EF3490);
    HW_WR_REG32(0x4300900c, 0xD172BC5A);
    /* ICSS_G0_CLKMUX_SEL:
      1h - MAIN_PLL0_HSDIV9_CLKOUT
    */
    HW_WR_REG32(0x43008044, 0x00010001);  // ICSS_G0 address is 0x43008040
    /* unlock PLL0 and PLL2 ctrl register*/
    HW_WR_REG32(0x00680010, 0x68EF3490);
    HW_WR_REG32(0x00680014, 0xD172BC5A);
    HW_WR_REG32(0x00682010, 0x68EF3490);
    HW_WR_REG32(0x00682014, 0xD172BC5A);
    /* set MAIN_PLL0_HSDIV9 to 2 for core_clock =333MHz (3 for 250 MHz, 1 = 500 MHz, 9 = 100 MHz)*/
    HW_WR_REG8(0x006800A4, 2);

    /* configure Scratch Pad Shift enable using ICSSG_SPP_REG MMR */
    asm("    ldi32 r11, 0x30026034");   /* load MII_G_RT_FDB_GEN_CFG2 MMR address */
    asm("    ldi32 r12, 0x2");          /* XFR_SHIFT_EN = 1 */
    asm("    sbbo &r12, r11, 0, 4");    /* write configuration to MMR */

    /* populate FIR coefficients in MEM*/
    fir_coef    = (int32_t*)FIR_COEF_MEM; /*point to the location of the FFT output data */
    while (coef_count < 64)
    {
        fir_coef[coef_count] = (int32_t)(fir_filter_taps[coef_count] * FIR_TWO_RAISED_TO_31);
        coef_count++;
    }
    /* call FIR function */
    FIR_64();
    return 0;
}
